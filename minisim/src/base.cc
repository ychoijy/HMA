/*****************************************************************************
The authors: "split transaction on m5 makes it challenging to  model the DRAM.
A single cycle latency is assumed for dequeueing an address bus request.  In
response to that, the current DRAM implementation assumes that a seperate DRAM
command generator / controller exists per bank and the dequeued addresses are
queued to these controllers. We can view this as an ideal scenario for a shared
DRAM command generator / controller with support for overlapping DRAM commands.
Compare DRAM PRE,ACT,CAS etc. latencies, DRAM clock  frequency and the number
of banks to determine whether the ideal scenario with a shared DRAM command
generator is equivalent to having multiple DRAM command generators per bank."

Note: if interleaved accesses is enabled, the memory controller maps the
addresses differently depending on the row_size, every row is mapped to another
bank. Thus, the text segment uses half of every bank, the heap the next quarter
of each bank, and the stack the rest.

******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include "base.hh"
#include "msim.hh"
#include "config.hh"

using namespace std;
class MemFeatures;

extern OsStats *stats;
extern MemoryArrayStats arrayStats;
extern AddrTranslator atrans;

/******************************************************************************
 * MemFeatures
 ******************************************************************************/
MemFeatures :: MemFeatures (int mtype, Addr defaultRankSize, bool reduce) {
  if(mtype == DRAM) { // DDR3-1333 667MHz bus (1Gb chips)
    strcpy(mname,"DRAM"); 
    (void)reduce;
    rk_size = (Addr) ((double)defaultRankSize * (double)DRAM_MULTIPLIER);
    //rk_size = defaultRankSize;

    //#if SMALL_DRAM
    //rk_size = reduce ? (defaultRankSize/4) : defaultRankSize; // SMALL_DRAM: using a smaller DRAM size for heterogeneous systems
    //#else
    //rk_size = defaultRankSize;
    //(void)reduce;
    //#endif
    DD0=110, DD0R=DD0, DD5A=240; // mA
    VDDR = VDDW = VDD; // volts
    tRCD=10, tRP=10, tRRDact=4, tRRDpre=4, tRFCmin=74; tREFI = (64000000.0 / ((double)DRAM_ROWS_PER_BANK(rk_size)));

  } else if(mtype == PCM) {
    DD0=(242.0*PCMPOW_MULT), DD0R=(102.0*PCMPOW_MULT);
    DD5A=0;
    VDDR = 1.0; VDDW = 1.6; // volts
    tRCD=(int)(37*PCM_RD_MULT), tRP=(int)(100*PCM_WR_MULT), tRRDact=3, tRRDpre=18, tRFCmin=0; tREFI = 0; // lee09
    strcpy(mname,"_PCM"); 
    rk_size = (Addr) ((double)defaultRankSize * (double)_PCM_MULTIPLIER);
  } else { printf("FATAL: invalid memory type\n"); exit(1); }

  tRAS = (int)ceil((double)(tRCD+tCL+tBurst+tRCD+tCWD+tBurst)/2.0);
  tRC = tRAS + tRP;
  this->mtype = mtype;
  tWR = 10;
  tRTP=5;
}

/////////////////////////////////////////////////////////////////////////////
inline Tick MemChannel :: toCpuCycles (Tick mem_cyc) {
  return (Tick)ceil(CPU_RATIO * mem_cyc);
}

// actTime is a timestamp in CPU cycles!
void MemChannel :: activateBank(Tick actTime, MemRank *crk) {
  crk->total_act++; 
  if(crk->total_act == 1) {
    crk->time_first_act = actTime;
  }
  crk->time_last_act = actTime;
}

#if CLOSE_PAGE
void MemChannel :: prechargeBank (Tick curTick, MemBank *cbk, MemRank *crk) { // WRAPPER for external calls
  cbk->busy_until = curTick + toCpuCycles(prechargeBankInRank(cbk, crk));
  cbk->prechargeEndsAt = cbk->busy_until; // when curTick == this, the precharge is done

  // NOTE: in PCM, the precharge may be unnecessary, when the row buffer is clean
  if(cbk->busy_until > curTick) {
    cbk->isPrecharging = true; // meanwhile, the bank is precharging
    crk->incCurrActivityCounter(); // precharge start
  }

  if(crk->busy_until < cbk->busy_until)
    crk->busy_until = cbk->busy_until;
}
#endif

class MemFeatures;

Tick MemChannel :: prechargeBankInRank (MemBank *cbk, MemRank *crk) {
  Tick precharge_overhead = 0;

  if(crk->f->mtype==DRAM) {
    precharge_overhead += crk->f->tRP + (cbk->lastCmdIsRead ? crk->f->tRTP : crk->f->tWR); // precharge overhead (MEM_CYC)
    crk->total_awr += cbk->rb.precharge(true); // always precharge
  } else { 
    if(cbk->isDirty()) {
      precharge_overhead = crk->f->tRP + (cbk->lastCmdIsRead ? crk->f->tRTP : crk->f->tWR); // (MEM_CYC)
      crk->total_awr += cbk->rb.precharge(false); // conditional precharge
    } else {
      if(cbk->rb.precharge(false) > 0) { // conditional precharge
        printf("FATAL: 'clean' row buffer had dirty blocks\n"); exit(1);
      } 
    }
  } 

  return precharge_overhead;
}

// same for CLOSE_PAGE or OPEN_PAGE (the difference is in when we precharge banks/ranks: msim.cc)
void MemChannel :: calculateFinishTick(Tick curTick, MemRequest *r) {
  MemBank *cbk = bk[r->mda.bk]; // current bank
  MemRank *crk = rk[r->mda.rk]; // current rank
  Tick lat=0, timeCurBurst, tRTR_WTR=0; // covers the writes
  bool doColSelection = true; // only false when it is a row hit (bank obviously open)

  #if PAGESTATS
  if(r->id < XFER_ENGINE_ID) {
    if(r->isRead)
      arrayStats.countCpuRead(r->addr);
    else
      arrayStats.countCpuWrite(r->addr);
  }
  #endif

  #if (RANKS_PCH > 1)
  // if the case, calculating rank-to-rank delay and write-to-read latencies
  if(r->isRead) {
    if(lastCmdIsRead) {
      tRTR_WTR = (lastRank!=r->mda.rk && cbk->busy_until==curTick) ? crk->f->tDQ : 0; // rank-to-rank latency (MEM_CYC)
    } else {
      tRTR_WTR = (lastRank==r->mda.rk && cbk->busy_until==curTick) ? crk->f->tWTR : 0; // write-to-read latency (MEM_CYC)
    }
  } // RTR and WTR are not applicable if the current operation is a write
  #endif
    
  lastRank = r->mda.rk;
  lastCmdIsRead = r->isRead;
  timeCurBurst = tRTR_WTR; // (MEM_CYC) will be zero if FAST_XFER is enabled

  //mbchoi
  //printf("[mbchoi]: mda.zn:%llu\n", r->mda.zn);
  //embchoi
  if(cbk->isOpen()) 
  { //printf("BK OPEN,");
    if(cbk->rb.isHit(r->addr)) 
    {
      stats->rowHit++; //else printf("curTick %llu ch %d bk %d ROW HIT :-)\n",curTick,r->ch,r->bk);
      stats->rbs.incRowHit(r->id, r->mda.zn, r->isDMA, r->isRead);
	  //mbchoi
	  //printf("[mbchoi]: mda.zn:%d\n", r->mda.zn);
	  //embchoi
      
      #if BURSTY_DMA
      if(r->isDMA) doColSelection = false; // DMA burst does not need a column selection
      #endif

    } else {  // row miss: precharge (if the case) and activate
      stats->rowMissBkOpen++; //printf("ROW MISS\n");
      stats->rbs.incRowMissBkOpen(r->id, r->mda.zn, r->isDMA, r->isRead);
      #if WR_DEBUG
      Tick tmp = prechargeBankInRank (cbk, crk); // (MEM_CYC)
      if(tmp > 0) {
        //printf("PRECHARGE BKUID %u (base.cc)\n",r->bkUID());
        r->debug_req_detailed("CAUSE_ARRAY_WR"); // this access (read or write) caused a writeback to the cell array
        timeCurBurst += tmp;
      }
      #else
      timeCurBurst += prechargeBankInRank (cbk, crk); // (MEM_CYC)
      #endif

      // checkFAW: input (CPU_CYC); output (MEM_CYC); (respect the four-activation window)
      timeCurBurst += crk->checkFAW(curTick + toCpuCycles(timeCurBurst), crk->lastCmdIsRead);
      activateBank(curTick + toCpuCycles(timeCurBurst), crk); // (CPU_CYC) changes cbk->activation bookkeeping and energy
      timeCurBurst += crk->f->tRCD; // (MEM_CYC)
      arrayStats.countArrayRead(r->addr);
      #if 0
      if(r->id == XFER_ENGINE_ID) { printf("XFER_RB_CONFLICT(bkUID%d) ppn %llu\n",r->bkUID(),r->getFrameNum()); }
      else { printf("REGULAR(bkUID%d) ppn %llu\n",r->bkUID(),r->getFrameNum()); }
      #endif
    }

  } else {
    stats->rowMissBkClosed++; // printf("curTick %llu ch %d BK %d CLOSED\n",curTick,r->ch,r->bk);
    stats->rbs.incRowMissBkClosed(r->id, r->mda.zn, r->isDMA, r->isRead);

    // checkFAW: input (CPU_CYC); output (MEM_CYC); (respect the four-activation window)
    timeCurBurst += crk->checkFAW(curTick + toCpuCycles(timeCurBurst), crk->lastCmdIsRead);
    activateBank(curTick + toCpuCycles(timeCurBurst), crk); // (CPU_CYC)
    timeCurBurst += crk->f->tRCD; // (MEM_CYC) bank closed, activating new row
    arrayStats.countArrayRead(r->addr);

    #if 0
    if(r->id == XFER_ENGINE_ID) { printf("XFER_RB_CONFLICT(bkUID%d) ppn %llu\n",r->bkUID(),r->getFrameNum()); }
    else { printf("REGULAR(bkUID%d) ppn %llu\n",r->bkUID(),r->getFrameNum()); }
    #endif
  }

  #if VALIDATE
  assert(atrans.getZoneByAddress(r->addr)==r->mda.zn);
  #endif

  if(r->isRead){ 
    if(doColSelection) {
      timeCurBurst += crk->f->tCL; // (MEM_CYC)
    }

      lat = toCpuCycles(timeCurBurst + crk->f->tBurst); // (CPU_CYC)
      crk->cyc_RD += crk->f->tBurst; // (MEM_CYC)

      //printf("$$$ OPEN CH %d RK %d BK %d\n",r->mda.ch,r->mda.rk,r->mda.bk);
      cbk->rb.touch(r->addr, r->mda.zn, false); // reads don't make the row dirty

  } else { // isWrite
    if(doColSelection) {
      timeCurBurst += crk->f->tCWD; // (MEM_CYC)
    } 

      lat = toCpuCycles(timeCurBurst + crk->f->tBurst); // (CPU_CYC)
      crk->cyc_WR += crk->f->tBurst; // (MEM_CYC)
   
      //printf("$$$ OPEN CH %d RK %d BK %d\n",r->mda.ch,r->mda.rk,r->mda.bk);
      cbk->rb.touch(r->addr, r->mda.zn , true); //cbk->dirty = true;
  }

  #if 1 // aligning bursts
  timeCurBurst = curTick + toCpuCycles(timeCurBurst); // converting timeCurBurst to CPU_CYC
  if(timeCurBurst < endLastBurst){
    lat += (endLastBurst-timeCurBurst);
    endLastBurst = endLastBurst + toCpuCycles(crk->f->tBurst);
  } else {
    endLastBurst = timeCurBurst + toCpuCycles(crk->f->tBurst);
  }
  #endif

  #if VALIDATE
  if(cbk->busy_until > curTick) { printf("FATAL: incorrect access overlap\n"); exit(1); } // DEBUG
  #endif

  cbk->busy_until = curTick + lat;
  cbk->lastCmdIsRead = r->isRead;
  crk->lastCmdIsRead = r->isRead;
  crk->total_access++;
  r->finTime = curTick + lat; 
}

MemRank :: MemRank (MemFeatures *f) {
  time_last_access = 0; // luramos: has to start with zero
  busy_until = 0; // luramos: rank-level milestone
  last_bank = INVALID; // last bank accessed in this rank
  lastCmdIsRead = true;
  total_access=0;
  this->f = f; // timing and memory features

  cyc_CKElopre=0; // PPD cycles
  cyc_CKEhipre=0; // PSB cycles
  cyc_CKEloact=0; // APD cycles
  cyc_CKEhiact=0; // APD+ASB cycles
  cyc_RD=0; // sum_per_RD
  cyc_WR=0; // sum_per_WR

  total_act = 0;
  total_awr = 0;
  time_first_act = 0;
  time_last_act = 0; // used for keeping track of times between activations (page misses)

  faw = new FawVerifier (f->tFAW, f->tRRDact, f->tRRDpre);

  // state variables
  prevActivityCounter = currActivityCounter = 0;
  power_down = true; // we start idle in POWER_DOWN mode
  exitingPowerDown = waiting = false;
  powerDownAt = 0; // instant at which the bank entered POWER_DOWN (before the entry exit)

  powerUpTransitions = powerDownTransitions = 0;
}

Tick MemRank :: getRRD() {
  return (Tick)ceil((double)(lastCmdIsRead ? f->tRRDact : f->tRRDpre) * CPU_RATIO);
}

MemChannel :: MemChannel()
{
  for(byte r=0; r<RANKS_PCH; r++)
    rk[r] = NULL; //new MemRank(f);

  for(byte b=0; b<BANKS_PCH; b++)
    bk[b] = new MemBank (); // banks created here

  done = false;
  endLastBurst = 0; // last data transfer burst (Reads or Writes)
  lastRank = (byte)INVALID;
  lastCmdIsRead = true;
  time_last_access = 0; // luramos: has to start with zero

  assert(RANKS_PCH >= 1);
  assert(BANKS_PCH >= 1);
}

void MemChannel :: addRank(byte r, MemFeatures *f) {
  if(rk[r] == NULL && r < RANKS_PCH) 
    rk[r] = new MemRank(f);
  else {
    printf("Error: rank overwritten or out of bounds\n");
    exit(1);
  }
}

void MemChannel :: validate () {
  for(byte r=0; r<RANKS_PCH; r++)
    if(rk[r] == NULL) {
      printf("Error: rank %d is NULL",r);
      exit(1);
    } //else printf("tRFC = %d\n",rk[r]->f->tRFCmin);
}

MemPowerCounter :: MemPowerCounter() {
  reset();
}

void MemPowerCounter :: reset() {
  PPD = PSB = APD = ASB = 0; // static power
  REF = MICRON_ACT = WR  = RD  = 0; // dynamic power (mem device dependent)
  DQ = termW = termRDoth = termWRoth = 0; // dynamic power (mem device independent)
  mW = 0;
  DETAIL_ACT = 0;

  powerDownTransitions = powerUpTransitions = 0;

  //#if OP_ENERGY
  //arrayReadJoules = arrayWriteJoules = rowBufferReadJoules = rowBufferWriteJoules = 0;
  //#endif
}

void MemPowerCounter :: addToPower(MemFeatures *f, double tWRsch, double tRDsch, double WRp, double RDp, double tACTavg, double BnkPre, double nCKEpre, double CKEpre, double nCKEact, double CKEact, double detailDD0)
{
  double vs = (f->VDD * f->VDD) / (f->VDDmax * f->VDDmax); // voltage scaling
  double fs = (f->tCK * DEV_GHZ); // frequency scaling
  int chips = CHIPS_PRK; // chips per rank

  //printf("%8d %8lf %8lf %8lf %8lf %8lf %8lf %8lf %8lf %8lf %8lf\n", f->mtype, tWRsch, tRDsch, WRp, RDp, tACTavg, BnkPre, nCKEpre, CKEpre, nCKEact, CKEact);

  // MICRON  and DRAMSIM vs: all except for DQ, termW, Roth and Woth
  // MICRON  fs: PPD, PSB, APD, ASB, WR and RD (I'm following this one, since I don't simulate refreshes)
  // DRAMSIM fs: PSB, ASB, ACT, REF, DQ, WR and RD
  // Note: in this function we are accumulationg power values of several ranks over several calls

  PPD += (f->DD2P * f->VDDmax) * BnkPre * nCKEpre * fs * vs * chips; 
  PSB += (f->DD2N * f->VDDmax) * BnkPre *  CKEpre * fs * vs * chips;
  APD += (f->DD3P * f->VDDmax) * (1.0-BnkPre) * nCKEact * fs * vs * chips;
  ASB += (f->DD3N * f->VDDmax) * (1.0-BnkPre) *  CKEact * fs * vs * chips;
  printf("StatePower(mW) PPD %.2lf PSB %.2lf APD %.2lf ASB %.2lf\n",
    (f->DD2P * f->VDDmax) * BnkPre * nCKEpre * fs * vs * chips,
    (f->DD2N * f->VDDmax) * BnkPre *  CKEpre * fs * vs * chips,
    (f->DD3P * f->VDDmax) * (1.0-BnkPre) * nCKEact * fs * vs * chips,
    (f->DD3N * f->VDDmax) * (1.0-BnkPre) *  CKEact * fs * vs * chips);

  //mbchoi
  printf("[mbchoi] f->DD3N:%.2lf f->VDDmax:%.2lf BnkPre:%.2lf CKEact:%.2lf fs:%.2lf vs:%.2lf chips:%d\n",
          f->DD3N,
          f->VDDmax,
          BnkPre,
          CKEact,
          fs,
          vs,
          chips);
  //embchoi

  if(f->mtype == DRAM){ // otherwise zero
    assert(f->DD3N < f->DD5A);
    REF += (f->DD5A-f->DD3N) * ((double)f->tRFCmin/(double)f->tREFI) * f->VDDmax * vs * chips;
  }

  if(tACTavg > 0 && f->tRC > 0) {
    double inc = (f->DD0-(((f->DD3N*(double)f->tRAS) + (f->DD2N*(double)(f->tRC-f->tRAS)))/(double)f->tRC)) * 
                 f->VDDmax * ((double)f->tRC/tACTavg) * vs * chips; 
    assert(inc >= 0); // check the increment
    MICRON_ACT += inc;

    inc = (detailDD0-(((f->DD3N*(double)f->tRAS) + (f->DD2N*(double)(f->tRC-f->tRAS)))/(double)f->tRC)) *
                 f->VDDmax * ((double)f->tRC/tACTavg) * vs * chips;


    if(inc <0) {
      printf("FATAL: base.cc: inc(%lf) < 0 \n", inc);
      printf("inc=(detailDD0-(((f->DD3N*(double)f->tRAS) + (f->DD2N*(double)(f->tRC-f->tRAS)))/(double)f->tRC)) * f->VDDmax*((double)f->tRC/tACTavg) * vs * chips;\n");
      printf("inc=(%lf-(((%lf*(double)f->tRAS) + (%lf*(double)(f->tRC-f->tRAS)))/(double)f->tRC)) * %lf*((double)%d/%lf) * vs * chips;\n",
             detailDD0, f->DD3N, f->DD2N, f->VDDmax, f->tRC, tACTavg);
      exit(0);
    }
    //assert(inc >= 0); // check the increment
    DETAIL_ACT += inc;  
  } 
  
  WR += (f->DD4W-f->DD3N) * f->VDDmax * WRp * fs * vs * chips; // row buffer writes
  RD += (f->DD4R-f->DD3N) * f->VDDmax * RDp * fs * vs * chips; // row buffer reads

  DQ += PdqRD * (f->DQ + f->DQS) * (RDp) * chips;
  if(RANKS_PCH == 1) { 
    termW += PdqWR * (f->DQ + f->DQS + f->DQM) * (WRp) * chips;
  } else if(RANKS_PCH > 1) {
    termRDoth += PdqRDoth * (f->DQ + f->DQS) * (tRDsch) * chips;
    termWRoth += PdqWRoth * (f->DQ + f->DQS + f->DQM) * (tWRsch) *chips;
  }

  // mW is equal because all the other powers are already being accumulated
  mW = PPD + PSB + APD + ASB + REF + WR  + RD  + DQ + termW + termRDoth + termWRoth; // activation is added later
}

void MemPowerCounter :: addRankStats (MemRank *crk) { 
  powerDownTransitions += crk->powerDownTransitions; 
  powerUpTransitions += crk->powerUpTransitions;

  //#if OP_ENERGY
  //  MemFeatures *f = crk->f;
  //  //crk->total_act: every activation reads (ROW_SIZE/cacheBlockSize) blocks
  //  arrayReadJoules += f->arr_r * (crk->total_act * (ROW_SIZE/cacheBlockSize));

  //  //crk->total_awr: awr accounts for each cache blocks written (differentl from total_act)
  //  arrayWriteJoules += f->arr_w * (crk->total_awr);

  //  rowBufferReadJoules += f->getBufferReadJoules(crk->cyc_RD);
  //  rowBufferWriteJoules += f->getBufferWriteJoules(crk->cyc_WR);
  //#endif
}


void MemPowerCounter :: print (double sec) {
  printf("Power/Energy Breakdown\n\n");

  printf("Circuitry (W)\n");
  printf("PPD        %12.9lf\n", PPD/1000.0);
  printf("PSB        %12.9lf\n", PSB/1000.0);
  printf("APD        %12.9lf\n", APD/1000.0);
  printf("ASB        %12.9lf\n", ASB/1000.0);
  printf("REF        %12.9lf\n", REF/1000.0); 
  printf("WR         %12.9lf\n", WR/1000.0);
  printf("RD         %12.9lf\n", RD/1000.0); 
  printf("DQ         %12.9lf\n", DQ/1000.0); 
  printf("termW      %12.9lf\n", termW/1000.0); 
  printf("termRDoth  %12.9lf\n", (termRDoth<0 ? 0 : termRDoth)/1000.0); 
  printf("termWRoth  %12.9lf\n", termWRoth/1000.0); 
  printf("-----------------------\n");
  printf("STATE(W)   %12.9lf\n", (PPD+PSB+APD+ASB+REF)/1000.0);
  printf("R/W/T(W)   %12.9lf\n", (WR+RD+DQ+termW+(termRDoth<0 ? 0 : termRDoth)+termWRoth)/1000.0); 
  printf("avgCirc(W) %12.9lf\n", mW/1000.0);
  printf("avgCirc(J) %12.9lf\n", mW*sec/1000.0); 
  printf("-----------------------\n");

  double watts = mW/1000.0;
  double array_watts = MICRON_ACT/1000.0;

  // from MICRON average power to energy
  printf("MICRON_AVG_ACT(W) sec:%.10lf watts:%.10lf ARRAY_WATTS%.10lf ARRAY_ENERGY(J) %.10lf TOT_ENERGY(J) %.10lf TOT_POWER(W) %.10lf ED: %.10lf ED2: %.10lf\n",
          sec,
          watts,
         array_watts, 
         array_watts * sec, 
         (watts + array_watts) * sec, // energy calculated as a function of power 
         (watts + array_watts),
         (watts + array_watts) * sec * sec, // funny looking, but it is ED:
         (watts + array_watts) * sec * sec * sec); // funny looking but it is ED2:

  array_watts = DETAIL_ACT/1000.0;
  printf("DETAIL_AVG_ACT(W) %.4lf ARRAY_ENERGY(J) %.7lf TOT_ENERGY(J) %.7lf TOT_POWER(W) %.7lf ED: %.7lf ED2: %.7lf\n",
       array_watts,
       array_watts * sec,
       (watts + array_watts) * sec, // energy calculated as a function of power 
       (watts + array_watts),
       (watts + array_watts) * sec * sec, // funny looking, but it is ED:
       (watts + array_watts) * sec * sec * sec); // funny looking but it is ED2:

  //#if OP_ENERGY
  //printf("\n");
  //double joules = (((PPD+PSB+APD+ASB+REF)/1000.0)*sec) + arrayReadJoules + arrayWriteJoules + rowBufferReadJoules + rowBufferWriteJoules;
  //printf("OP.State(J)          %12.7lf\n", ((PPD+PSB+APD+ASB+REF)/1000.0)*sec);
  //printf("OP.arrayRead(J)      %12.7lf\n", arrayReadJoules); 
  //printf("OP.arrayWrite(J)     %12.7lf\n", arrayWriteJoules);
  //printf("OP.rowBufferRead(J)  %12.7lf\n", rowBufferReadJoules);
  //printf("OP.rowBufferWrite(J) %12.7lf\n", rowBufferWriteJoules);
  //printf("OP.TOT_ENERGY(J)     %12.7lf\n", joules);
  //printf("OP.AVG_POWER(W)      %12.7lf\n", joules/sec);
  //#endif

  printf("\ntotalPowerDownTransitions %llu totalPowerUpTransitions %llu\n", powerDownTransitions, powerUpTransitions);
}

void MemChannel :: getPowerStats (Tick finalTick, MemPowerCounter *p) { // must be called only in the end of timing simulation
  byte r;
  double sumWRchan=0, sumRDchan=0, termWRsch, termRDsch, WRpercent, RDpercent, tACTavg, BnkPre, CKElopre, CKEhipre, CKEloact, CKEhiact;

  if(!done) { // updating cycles and getting stats for power calculation (must be done once in the end)
    sumWRchan = sumRDchan = 0;
    for(r=0; r<RANKS_PCH; r++) {
      if(rk[r]->total_act > 2) {
        rk[r]->sumAct = rk[r]->time_last_act - rk[r]->time_first_act; // in CPU cycles
        //printf("rk %u first %llu last %llu sumAct %llu final %llu\n",
        // r,rk[r]->time_first_act,rk[r]->time_last_act,rk[r]->sumAct,finalTick);

        // accumulating WR and RD percent across all ranks of this channel
        sumWRchan += (double)toCpuCycles(rk[r]->cyc_WR) / (double)rk[r]->sumAct;
        sumRDchan += (double)toCpuCycles(rk[r]->cyc_RD) / (double)rk[r]->sumAct;
      } else {
        rk[r]->sumAct = 0;
      }
    }
    done = true;
  }

  for(r=0; r<RANKS_PCH; r++) {
    termWRsch = termRDsch = WRpercent = RDpercent = tACTavg = BnkPre = CKElopre = CKEhipre = CKEloact = CKEhiact = 0.0;

    if(rk[r]->sumAct > 0) {
      WRpercent = (double)toCpuCycles(rk[r]->cyc_WR) / (double)rk[r]->sumAct; // % active time reading
      RDpercent = (double)toCpuCycles(rk[r]->cyc_RD) / (double)rk[r]->sumAct; // % active time writing
      //printf("RK: %d WRpct: %lf RDpct: %lf", r, WRpercent, RDpercent);

      #if RANKS_PCH > 1
      if(RANKS_PCH > 1) {// finish calculating termination power for other DRAMs

       /* Unlike in DRAMSim, we should accumulate, not take the average.
        * However termination cycles are half of those spent reading  from or writting
        * to memory.  since we consider DDR. */

        termWRsch = (sumWRchan-WRpercent); // / (double)(RANKS_PCH-1); 
        termRDsch = (sumRDchan-RDpercent); // / (double)(RANKS_PCH-1);
      } // else both are zero
      #endif
    }

    if(termWRsch < 0) termWRsch = 0;
    if(termRDsch < 0) termRDsch = 0;

    if(rk[r]->total_act > 2) // luramos: it only makes sense when we have 2 activations
      tACTavg = ceil((double)rk[r]->sumAct/(double)rk[r]->total_act-1) / CPU_RATIO; // avg time between activations (in memory cycles)
    else if (rk[r]->total_act == 2)
      tACTavg = (double)rk[r]->sumAct / CPU_RATIO; // avg time between activations (in memory cycles)

    Tick CKEtotal = (rk[r]->cyc_CKEhiact + rk[r]->cyc_CKEloact + rk[r]->cyc_CKEhipre + rk[r]->cyc_CKElopre);
    //mbchoi
    //printf("[mbchoi] cyc_CKEhiact:%llu cyc_CKEloact:%llu cyc_CKEhipre:%llu cyc_CKElopre:%llu\n", rk[r]->cyc_CKEhiact, rk[r]->cyc_CKEloact, rk[r]->cyc_CKEhipre, rk[r]->cyc_CKElopre);
    //embchoi

    if(CKEtotal != finalTick) 
      printf("WARNING: sumCKE (%llu) != finalTick (%llu)\n", CKEtotal,finalTick);

    if(finalTick > 0) {
      BnkPre = (double)(rk[r]->cyc_CKElopre + rk[r]->cyc_CKEhipre) / (double)CKEtotal; // % time precharge state
      //mbchoi
      //printf("[mbchoi] BNKPRE = (%lf+%lf) / %lf = %lf",(double)rk[r]->cyc_CKElopre, (double)rk[r]->cyc_CKEhipre,(double)finalTick,BnkPre);
      //embchoi
    }

    if(rk[r]->cyc_CKElopre + rk[r]->cyc_CKEhipre > 0) {
      CKElopre = (double)rk[r]->cyc_CKElopre / (double)(rk[r]->cyc_CKElopre + rk[r]->cyc_CKEhipre);
      CKEhipre = (double)rk[r]->cyc_CKEhipre / (double)(rk[r]->cyc_CKElopre + rk[r]->cyc_CKEhipre);
    }

    if(rk[r]->cyc_CKEloact + rk[r]->cyc_CKEhiact > 0) {
      CKEloact = (double)rk[r]->cyc_CKEloact / (double)(rk[r]->cyc_CKEloact + rk[r]->cyc_CKEhiact);
      CKEhiact = (double)rk[r]->cyc_CKEhiact / (double)(rk[r]->cyc_CKEloact + rk[r]->cyc_CKEhiact);
    }

    #if 1
    printf("Rk%d: accesses %llu acts %llu arrayWrites(blocks) %llu\n",
           r, rk[r]->total_access, rk[r]->total_act,  rk[r]->total_awr);
    // cyc breakdown
    printf("powerDownTransitions %llu powerUpTransitions %llu\n",rk[r]->powerDownTransitions,rk[r]->powerUpTransitions);
    //printf("{cycPPD %llu | cycPSB %llu | cycAPD %llu | cycASB %llu} ",
    //       rk[r]->cyc_CKElopre, rk[r]->cyc_CKEhipre, rk[r]->cyc_CKEloact, rk[r]->cyc_CKEhiact); 
    //printf("{cycPPD %llu | cycPSB %lf | cycAPD %llu | cycASB %llu} ",
    //       rk[r]->cyc_CKElopre, rk[r]->cyc_CKEhipre, rk[r]->cyc_CKEloact, rk[r]->cyc_CKEhiact); 
    printf("{cycPPD %lf | cycPSB %llu | cycAPD %llu | cycASB %llu} ",
           rk[r]->cyc_CKElopre, rk[r]->cyc_CKEhipre, rk[r]->cyc_CKEloact, rk[r]->cyc_CKEhiact); 
    //printf("CKEtotal %llu\n",rk[r]->cyc_CKEhiact + rk[r]->cyc_CKEloact + rk[r]->cyc_CKEhipre + rk[r]->cyc_CKElopre); // total
    printf("CKEtotal %lf\n",rk[r]->cyc_CKEhiact + rk[r]->cyc_CKEloact + rk[r]->cyc_CKEhipre + rk[r]->cyc_CKElopre); // total
    printf("{cyc_RD %llu | cyc_WR %llu | cyc_RASsum %llu}\n",rk[r]->cyc_RD, rk[r]->cyc_WR, rk[r]->sumAct);
    printf("Avg MEM CYCLES between activations %.2lf mem cycles (CPU_RATIO %.2lf) (tRC = %u mem cyc)\n", 
           tACTavg, CPU_RATIO, rk[r]->f->tRC);
    printf("WRp: %.4lf RDp: %.4lf termWR: %.4lf termRD: %.4lf tACTavg: %.2lf BnkPre %.2lf CKElopre %.2lf CKEloact %.2lf\n", 
           WRpercent, RDpercent, termWRsch, termRDsch, tACTavg, BnkPre, CKElopre, CKEloact);
    #endif

    //printf(">>>>> ACTIVE TIME PERCENT (1.0-BnkPre): %lf\n",(1.0-BnkPre));
    p->addToPower(rk[r]->f, termWRsch, termRDsch, WRpercent, RDpercent, 
                  tACTavg, BnkPre, CKElopre, CKEhipre, CKEloact, CKEhiact, rk[r]->getDetailDD0());

    p->addRankStats(rk[r]);
  }
}

byte RowBuffer :: precharge (bool alwaysWriteBack) {
  byte writebacks = 0; // number of dirty cache blocks in a row

  for(byte i=0; i<cacheBlocksPerRow; i++) {
    if(rowStatus[i] || alwaysWriteBack) { // this cache block is dirty and needs to be written back (or bypass has been activated)
      Addr rowCbBaseAddr = (lastRowUID * ROW_SIZE) | (((Addr)i) << cacheBlockSizeBits); // base addr of the cache block within the open row

      #if WR_DEBUG
      if((rowCbBaseAddr >> cacheBlockSizeBits) == 289359) printf(">>>>> MW_CBUID++ CBUID %llu ",(rowCbBaseAddr >> cacheBlockSizeBits));
      #endif

      //stats->countPhysicalWrite(rowCbBaseAddr, lastRowZone); // when we precharge, we write to a cell

      #if PAGESTATS
      arrayStats.countArrayWrite(rowCbBaseAddr); //stats->countArrayWrite(rowCbBaseAddr); 
      #endif

      writebacks++;
    }
  }

  reset();
  return writebacks; // number of cache blocks written back
};



#if PAGESTATS
inline PageStatsEntry *MemoryArrayStats :: find (FrameNum f) {
  PSit it=pages.find(f);
  PageStatsEntry *p;
  if(it == pages.end()) {
    p = new PageStatsEntry();
    pages.insert(PSpair(f,p));
  } else { p=it->second; }
  return p;
}

void MemoryArrayStats :: countCpuRead (Addr addr) {
  FrameNum ppn = (FrameNum)(addr>>pageSizeBits);
  if(ppn == INVALID_PPN) return;
  PageStatsEntry *p = find(ppn);
  p->cpuReads++;
}

void MemoryArrayStats :: countCpuWrite (Addr addr) {
  FrameNum ppn = (FrameNum)(addr>>pageSizeBits);
  if(ppn == INVALID_PPN) return;
  PageStatsEntry *p = find(ppn);
  p->cpuWrites++;
}

void MemoryArrayStats :: countArrayRead (Addr addr) { 
  FrameNum ppn = (FrameNum)(addr>>pageSizeBits);
  if(ppn == INVALID_PPN) return;
  PageStatsEntry *p = find(ppn);
  p->arrayReads++;
}

void MemoryArrayStats :: countArrayWrite (Addr addr) { 
  FrameNum ppn = (FrameNum)(addr>>pageSizeBits);
  if(ppn == INVALID_PPN) return;
  PageStatsEntry *p = find(ppn);
  // index = index of cache block whithin a page = offset whithin page / cache block
  unsigned index = (unsigned)((addr&(Addr)(pageSize-1)) >> cacheBlockSizeBits); 
  //printf("addr %llu ppn %llu cb_index %d\n",addr,ppn,index);
  p->cbArrayWrites[index]++;

  // cache block stats
  byte z = atrans.getZoneByAddress(addr);
  totalZoneWrites[z]++; // one more write in the zone
  if(p->pristine) {
    zoneWrittenPages[z]++; // new written page in this zone
    p->pristine = false;
  }

  if(hiWritesToZoneCB[z] < p->cbArrayWrites[index]) {
    hiWritesToZoneCB[z] = p->cbArrayWrites[index];
    hiWritesCBUID[z] = (addr >> cacheBlockSizeBits); // cache block unique ID
  }  
}

void MemoryArrayStats :: countWasSrc (FrameNum f) { if(f==INVALID_PPN) return; PageStatsEntry *p = find(f); p->wasSrc++;}
void MemoryArrayStats :: countWasDst (FrameNum f) { if(f==INVALID_PPN) return; PageStatsEntry *p = find(f); p->wasDst++;}
void MemoryArrayStats :: countWasRnd (FrameNum f) { if(f==INVALID_PPN) return; PageStatsEntry *p = find(f); p->wasRnd++;}
void MemoryArrayStats :: countWasRep (FrameNum f) { if(f==INVALID_PPN) return; PageStatsEntry *p = find(f); p->wasRep++;}

void MemoryArrayStats :: savePageStats (char *psfile) {
  FILE* f = fopen(psfile,"w");
  if(f != NULL) {
    fprintf(f,"%8s %8s %8s %8s %8s %8s %8s %8s %8s %8s\n\n", "ppn","arr_mwb","tot_pgw","arr_r","cpu_r","cpu_w","src","dst","rnd","rep");
    for(PSit it=pages.begin(); it!=pages.end(); it++) 
      it->second->print(f,it->first);
    fclose(f);
  } else { printf("WARNING: cannot open %s to save pagestats.\n",psfile); }
}

void MemoryArrayStats :: printStats (double seconds) {
  Counter taw=0, nwp=0, pcmb=0; // total array writes, number of written pages and total capacity of pcm in Bytes
  double wpp=0.0, bps=0, alpha=0, e=0.0; // average writes per page, write bandwidth in Bytes/s wear-levelling efficiency and required endurance
  Counter mwb = 0; // most writes to a cache block in memory

  printf("ENDURANCE ****************************************************************************\n");
  printf("MC.zone0 most written cache line %llu times (PPN %llu CBUID %llu)\n",
         hiWritesToZoneCB[0], hiWritesCBUID[0]/cacheBlocksPerPage, hiWritesCBUID[0]);
  printf("MC.zone1 most written cache line %llu times (PPN %llu CBUID %llu)\n",
         hiWritesToZoneCB[1], hiWritesCBUID[1]/cacheBlocksPerPage, hiWritesCBUID[1]);

  if(atrans.getZoneType(1)==PCM) {
    if(atrans.getZoneType(0)==PCM) { // PCM + PCM
      bool b = (hiWritesToZoneCB[0] > hiWritesToZoneCB[1]) ? 0 : 1;
      printf("MC.most_writes_to_PCM: %llu times (PPN %llu)\n",hiWritesToZoneCB[b],hiWritesCBUID[b]/cacheBlocksPerPage);
      taw = totalZoneWrites[0] + totalZoneWrites[1];
      nwp = zoneWrittenPages[0] + zoneWrittenPages[1];
      mwb = hiWritesToZoneCB[b];
      pcmb = atrans.getZoneSize(0) + atrans.getZoneSize(1);
      
    } else { // DRAM + PCM
      printf("MC.most_writes_to_PCM: %llu times (PPN %llu)\n",hiWritesToZoneCB[1],hiWritesCBUID[1]/cacheBlocksPerPage);
      taw = totalZoneWrites[1];
      nwp = zoneWrittenPages[1];
      mwb = hiWritesToZoneCB[1];
      pcmb = atrans.getZoneSize(1);
    }
  } else { 
    if(atrans.getZoneType(0)==PCM) { // PCM + DRAM
      printf("MC.most_writes_to_PCM: %llu times (PPN %llu)\n",hiWritesToZoneCB[0],hiWritesCBUID[0]/cacheBlocksPerPage);
      taw = totalZoneWrites[0];
      nwp = zoneWrittenPages[0];
      mwb = hiWritesToZoneCB[0];
      pcmb = atrans.getZoneSize(0);

    } else { // DRAM + DRAM
      printf("MC.most_writes_to_PCM: x times (PPN x)\n");
    }
  }

  wpp = (nwp==0) ? 0.0 : (double) taw / (double)nwp;
  bps = ((double)(taw * cacheBlockSize)) / seconds;
  alpha = ((double)wpp / (double)cacheBlocksPerPage) / (double)mwb; // avgWritesPerBlock / mostWrittenBlock
  e = ((double)(5*365*24*3600) * bps) / (alpha * pcmb);

  printf("\n");
  printf("ZONE0: totZoneW %llu zoneWpages %llu zoneMWBlock %llu zoneSize(bytes) %llu\n",
         totalZoneWrites[0],zoneWrittenPages[0],hiWritesToZoneCB[0], atrans.getZoneSize(0));
  printf("ZONE1: totZoneW %llu zoneWpages %llu zoneMWBlock %llu zoneSize(bytes) %llu\n",
         totalZoneWrites[1],zoneWrittenPages[1],hiWritesToZoneCB[1], atrans.getZoneSize(1));

  printf("TAW %llu WPP %.2lf WPblock %.2lf MWblock %llu pcmCapacity(bytes) %llu alpha %.4lf\n", 
         taw, wpp, ((double)wpp / (double)cacheBlocksPerPage), mwb, pcmb, alpha);
  printf("reqE(5yrs) %.0lf logE(5yrs) %.2lf\n", e, (e>0) ? log10(e) : 0.0);

  e = ((double)(3*365*24*3600) * bps) / (alpha * pcmb);
  printf("reqE(3yrs) %.0lf logE(3yrs) %.2lf\n", e, (e>0) ? log10(e) : 0.0);
  printf("**************************************************************************************\n");
}
#endif
