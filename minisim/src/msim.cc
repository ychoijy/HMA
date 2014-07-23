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
#include "msim.hh"
#include "mman.hh"

extern OsStats *stats;
extern MemManager *pplace;
extern AddrTranslator atrans;

inline void MainMemory :: lightWeightSchedule (MemRequest *r) {
  r->addToAvgMemLatency = true; // avoid retired accesses (will be added to r->mda.zn)
  schedule(r); 
}

bool MainMemory :: scheduleCpuAccess (unsigned id, Tick curTick, Addr addr, bool isRead, bool hold) {
  //printf("scheduleCpuAccess id %d ppn %llu (addr %016llx) isRead %d\n",id,addr>>pageSizeBits,addr,isRead);

  #if VALIDATE
  if(addr >= TOT_MEM_SIZE) { printf("FATAL: ignoring address out-of-bounds %016llx (> memSize: %016llx)\n",addr,TOT_MEM_SIZE); exit(1); }
  #endif

  //mbchoi
  //printf("[mbchoi] funname:scheduleCpuAccess file:msim.cc addr:%llu curTick:%llu isRead:%d\n", addr, curTick, isRead);
  //embchoi
  //
  bool blocking = true;
  MultiDimAddr md;
  atrans.translate(addr, &md); // translating address into multi-dimensional address
  MemRequest *r;

  //mbchoi
  //printf("[mbchoi] funname:scheduleCpuAccess file:msim.cc addr:%llu curTick:%llu isRead:%d\n", addr, curTick, isRead);
  //embchoi

  if(hold) { // PAGE_FAULT to be handled!
    r = new MemRequest(id, curTick, addr, &md, isRead, false);
    r = sbridge.hold(r); // r will be replaced by the first operation of the SBDMA
    schedule(curTick, r); 

  } else { // do not hold (no page faults to be handled)
    r = new MemRequest(id, curTick, addr, &md, isRead, false);

    if(!isRead) { // assume that non-blocking accesses fo not stop the CPU
      r->isBlocking = false;
      blocking = false;
    }

    lightWeightSchedule(r); 
  } 

  inFlightAccesses++;
  return blocking;
}

void MainMemory :: schedule (MemRequest *r) { // used ONLY by lightWeightSchedule
  #if LATDEBUG
  testBankMove(r);
  #endif

  bool schedule = true;
  
  if(sched[r->bkUID()]->inWriteQueue(r->addr)) { // retire accesses that hit the write buffer
    schedule = false;
  }
  
  if(schedule) {
    sched[r->bkUID()]->push(r);
  } else {
    r->finTime = r->schTime; // NO_CUR_TIME
    stats->writeQueueHits++;
    retire(r);
  }

  #if TIME_DEBUG
  r->debug_req_detailed("LWSCHED");
  #endif
}

// schedule acess readjusting its timestamp and multi-dimensional address
void MainMemory :: schedule (Tick curTick, MemRequest *r) {
  atrans.translate(r->addr, &r->mda); // need to translate because r->addr may not have been translated yet
  r->started = false;
  r->schTime = MAX(r->arrTime,curTick);

  assert(r->bkUID() < NBANKS);

  #if LATDEBUG
  testBankMove(r);
  #endif

  bool schedule = true;

  if(sched[r->bkUID()]->inWriteQueue(r->addr)) { // retire accesses that hit the write buffer
    schedule = false;
  }

  if(schedule) {
    sched[r->bkUID()]->push(r);
  } else { // retire
    r->finTime = curTick; // CUR_TIME_PRESENT
    stats->writeQueueHits++;
    retire(r);
  }

  #if TIME_DEBUG
  r->debug_req_detailed("RESCHED");
  #endif
}

#if LATDEBUG
inline void MainMemory :: testBankMove (MemRequest *r) {
  if(r->id < XFER_ENGINE_ID && !r->isDMA) {
    if(!r->isBlocking && r->wbufQueueTime==0) {
      r->wbufQueueTime = MAX((r->schTime-r->arrTime), 1);
    }
    //if(r->bkSizeOnArrival == (unsigned)INVALID) {
    //  printf("BK_MOVE bkUID %u QSoArr %u QDELAYSoFar(ns) %.2lf [arr %llu id %u R %d blocking %d ]\n",
    //         r->bkUID(), r->bkSizeOnArrival, ((double)(r->schTime-r->arrTime))/cpuGhz,
    //         r->arrTime, r->id, r->isRead, r->isBlocking);
    //}
    r->bkSizeOnArrival = sched[r->bkUID()]->size();
  }
}

inline void MainMemory :: testBankExec (MemRequest *r) {
  if(r->id < XFER_ENGINE_ID && !r->isDMA) {
    double qLat = ((double)(r->schTime-r->arrTime))/cpuGhz; // nanosec
    if(qLat > 500.0) { // nanosec
      printf(">>>> ");
    } else {
      printf("<<<< ");
    }
      printf("EXEC bkUID %2u QSzArr %2u Q(ns) %8.2lf (wb %8.2lf bz %7.2lf rrd %5.2lf ) R/W(ns) %5.2lf [ arr %010llu id %2u R %d blocking %d ]\n",
             r->bkUID(), r->bkSizeOnArrival, qLat, 
             (double)r->wbufQueueTime/cpuGhz, 
             (double)r->bkBusyTime/cpuGhz, 
             (double)r->rrdTime/cpuGhz,
             ((double)(r->finTime-r->schTime))/cpuGhz,
             r->arrTime, r->id, r->isRead, r->isBlocking);
    //}
  }
}
#endif

void MainMemory :: scheduleDMA (byte id, Addr paddr, bool dirty) {
  inFlightAccesses += sbridge.scheduleDMA (id, paddr, dirty);
}

inline void MainMemory :: adjustByState (Tick curTick, MemRequest *r, MemRank *crk) {
  #if 1
  // exiting BUSY_WAIT mode
  if(crk->waiting) {
    crk->waiting = false;
    crk->decCurrActivityCounter();

    #if PSTATE_DEBUG
    crk->debug("WAITING", curTick, r->mda.ch, r->mda.rk, r->mda.bk,false);
    //printf("ending_waiting crk->currActivityCounter-- (%u)\n",crk->currActivityCounter);
    //printf("###### STOP_WAITING at %llu CH %d RK %d\n",curTick, r->mda.ch, r->mda.rk);
    #endif

  // exiting POWER_DOWN mode (first access that finds the rank in power down)
  } else if(crk->power_down) { // the rank is in PD mode, so all its banks will be inaccessible until tXP CPU ticks are satisfied
    #if VALIDATE
    if(crk->total_access > 0) { assert( ((crk->busy_until-crk->powerDownAt) >= crk->getTPD()) ); }
    //if( crk->total_access>0 && (crk->busy_until-crk->powerDownAt) < (crk->getTPD()) ) {
    //   printf("FATAL: (crk->busy_until %llu - crk->powerDownAt %llu = diff %llu ( > tPD %llu)",
    //         crk->busy_until, crk->powerDownAt, (crk->busy_until-crk->powerDownAt), crk->getTPD()); exit(1);
    //}
    #endif

    crk->power_down = false; // activating this rank
    setRankBusyUntil(r->mda.ch, r->mda.rk, MAX(crk->busy_until, curTick) + crk->getTXP());
    crk->exitingPowerDown = true;
    crk->incCurrActivityCounter();

    #if PSTATE_DEBUG
    printf("\nSTART_EXIT_POWERDOWN at %llu CH %d RK %d (MUST END AT %llu ) BKUID %u\n",curTick, r->mda.ch, r->mda.rk, crk->busy_until, r->bkUID());
    crk->debug("EXIT_PD", curTick, r->mda.ch, r->mda.rk, r->mda.bk,true);
    #endif
  }

  // rank is still activating, postpone access
  if(crk->exitingPowerDown) { 
    // DIRTY_FIX: sometimes the access that activated the rank is not there anymore 
    // because it was migrated or retired so hopefully there will be another access
    if(crk->busy_until <= curTick) {
      crk->exitingPowerDown = false; 
      crk->decCurrActivityCounter();
      #if PSTATE_DEBUG
      crk->debug("EXIT_PD", curTick, r->mda.ch, r->mda.rk, r->mda.bk, false);
      #endif

      if(crk->total_access > 0) fixCKE(crk->getTPD(), crk); // tPD had been accounted as powerdown, but is actually active

      #if PSTATE_DEBUG
      printf("FINISH_EXIT_POWERDOWN at %llu CH %d RK %d bkUID %u\n\n",curTick, r->mda.ch, r->mda.rk, r->bkUID());
      #endif
      crk->powerUpTransitions++;
    } 
  }
  #endif
}

#if VALIDATE
void MainMemory :: debugRank (byte channel, byte rank) {
  #if 0 
  MemChannel *cch = mc[channel];
  byte begBk = (rank * BANKS_PRK); // not absolutely unique (only within the channel)
  byte endBk = (begBk + BANKS_PRK); 

  printf("\nDEBUG_RANK CH %d RK %d BK [ %d %d ] BUSY_UNTIL %llu\n",channel, rank, begBk, endBk-1, cch->rk[rank]->busy_until);
  for(byte b=begBk; b<endBk; b++) {
    printf(" + CH[%d].BANK[%2d]->busy_until %llu\n",channel, b, cch->bk[b]->busy_until);
  }
  #else
  (void)channel, (void)rank;
  #endif
}

void MainMemory :: debugState (Tick curTick, MemRequest *r, MemRank *crk, const char *label) {
  printf("\n%s curTick %llu rank [ PowerDownAt %llu BusyUntil %llu ActivityCounter %u Waiting %d PowerDown %d ExitingPowerDown %d]\n", 
         label, curTick, crk->powerDownAt, crk->busy_until, crk->getCurrActivityCounter(), crk->waiting, crk->power_down, crk->exitingPowerDown);

  debugRank(r->mda.ch, r->mda.rk);
}
#endif

void MainMemory :: startAccess(Tick curTick, MemRequest *r) {
  Tick timeSinceLastAct, tmp;
  MemBank *cbk = mc[r->mda.ch]->bk[r->mda.bk];
  MemRank *crk = mc[r->mda.ch]->rk[r->mda.rk];
  bool executeNow = false;

  // at this point, accesses are already in the request queue ('<=' is because an access can get delayed without having its schTime updated)
  if(r->schTime <= curTick) { 
    //debugState(curTick, r, crk, "BEFORE_ADJUST");

    adjustByState(curTick, r, crk); // possible extra delays associated with power states and their changes
    // rank is already powered up, access must wait until bank is free
    if(cbk->busy_until > r->schTime) { 
      #if LATDEBUG
      r->bkBusyTime = (cbk->busy_until-r->schTime);
      #endif
      r->schTime = cbk->busy_until; 
    }

    //debugState(curTick, r, crk, "AFTER_ADJUST");
  }

  // test again, since the schedule time may have changed
  if(r->schTime <= curTick) { 
    // ISSUE_AS_BURST: bursts (row_open + row_hit) can proceed immediately (and will be aligned later)
    if((cbk->isOpen()) && (cbk->rb.isHit(r->addr))) { // the acess can be issued immediately as a burst
      executeNow = (r->schTime <= curTick); // no delay, try to execute now
      #if EVTDEBUG
      //if(curTick>1424873000)
        if(executeNow) printf("BURST_");
      #endif

    // CONFLICTING row activations, must respect a minimum latency (tRRD)
    // access to different banks of a same rank must respect min activation-to-activation delay (per rank)
    } else {  // an activation is required, possibly preceeded by a precharge latency
      timeSinceLastAct = (r->schTime - crk->time_last_act); // unsigned value always > 0
      tmp = crk->getRRD(); // min time between activations within a rank
      if(timeSinceLastAct < tmp && crk->total_act > 0) {
        r->schTime += (tmp-timeSinceLastAct); // delay access: conflicting activations
        #if LATDEBUG
        r->rrdTime += (tmp-timeSinceLastAct); // accumulate rrd for this access' stats
        #endif
      } else { // RRD was respected, start now!
        executeNow = (r->schTime <= curTick); // no delay, try to execute now
      }
    }

    // ISSUE IMMEDIATELY: access has exclusive use of the bank at this tick until it finishes
    if (executeNow) { 
      #if VALIDATE
      //debugState(curTick, r, crk, "ON_EXECUTE");
      if(crk->exitingPowerDown || crk->power_down || crk->waiting) {
        debugState(curTick, r, crk, "ERROR:");
        r->debug_req_detailed("START");
        printf("FATAL: corrupted state on executing access\n"); debug(); exit(1);
      } else if(cbk->isPrecharging && cbk->prechargeEndsAt != curTick) {
        debugState(curTick, r, crk, "ERROR:");
        r->debug_req_detailed("START");
        printf("FATAL: bank is precharging and had an access (prechargeEndsAt %llu )\n",cbk->prechargeEndsAt); debug(); exit(1);
      }
      #endif

      // updates r's timings and the bank and rank activation status
      mc[r->mda.ch]->calculateFinishTick(r->schTime, r); 
      r->started = true;
      crk->incCurrActivityCounter(); // we started one more access in this rank at curTick
      #if PSTATE_DEBUG
      crk->debug("ACCESS", curTick, r->mda.ch, r->mda.rk, r->mda.bk, true);
      //printf("starting_access crk->currActivityCounter++ (%u)\n",crk->currActivityCounter);
      #endif

      #if EVTDEBUG 
      //if(curTick>1424873000) 
        r->debug_req_detailed("START"); //printf("bkUID %u QSIZE %u\n",r->bkUID(), sched[r->bkUID()]->size());
      #endif

      #if CPU_RW_DEBUG
      if(r->isBlocking) r->debug_req_min("BEG_B",curTick); 
      else r->debug_req_min("BEG_N",curTick);
      #endif

      #if LATDEBUG
      testBankExec(r);
      #endif

      #if WR_DEBUG
      if(!r->isRead) { 
        r->debug_req_detailed("ROWBUF_WRITE");
        if(r->getFrameNum() == 4521) printf(">>>>> MW_PPN++ PPN %llu ", r->getFrameNum());
      }
      #endif

      if(crk->busy_until < cbk->busy_until) { crk->busy_until = cbk->busy_until; }
    } 
  }
}

void MainMemory :: tryAdvanceDMA (Tick curTick, MemRequest *r) { // RETIRE ACCESS
  // South Bridge
  if(r->id < XFER_ENGINE_ID) { // advance SB DMA transfer
    if(r->isDMA) { 
      MemRequest *x = sbridge.retrieveNext(r->id);
      if(x != NULL) schedule(curTick, x);
    }
  }
}

Tick MainMemory :: updateRankStates (Tick curTick) {
  byte b, c, bkUID=0; // b=unique within channel; UID=unique across the entire memory
  MemBank *cbk;
  MemRank *crk;
  Tick earliest = INVALID_TICK;
  //const bool debug = false; //EVTDEBUG;

  for(c=0; c<NCHANNELS; c++) {
    for(b=0; b<BANKS_PCH; b++) { // relative bank address [0-BANKS_PCH]
      cbk = mc[c]->bk[b];
      crk = mc[c]->rk[getRankByBank(b)];

      #if CLOSE_PAGE
      if(cbk->isPrecharging) {
        //if(cbk->prechargeEndsAt<curTick) {printf("missed it CH %d RK %d BK %d prechargeEndsAt %llu now %llu\n",
        //   c, getRankByBank(b), getChannelBankWithinRank(b), cbk->prechargeEndsAt, curTick); exit(1);} 

        if(cbk->prechargeEndsAt == curTick) {
          cbk->isPrecharging = false;
          crk->decCurrActivityCounter();
          #if PSTATE_DEBUG
          crk->debug("PRECHARGE", curTick, c, getRankByBank(b), getChannelBankWithinRank(b), false);
          //if(debug) { printf("PRECHARGE_END at %llu CH %d RK %d BK %d CLOSED\n",curTick,c, getRankByBank(b), getChannelBankWithinRank(b)); }
          #endif
        }
      }

      if(cbk->isOpen() && curTick >= cbk->busy_until) {
        MemRequest *r=sched[bkUID]->findNew();

        if(r != NULL) {
          /* There is an access to this bank, so even if all banks of this rank are precharged we should still keep it active */
          if(!cbk->isHit(r->addr)) { // ROW_MISS: precharge but still keep active
            mc[c]->prechargeBank(curTick, cbk, crk); // calls incCurrActivityCounter if a precharge occurs

            #if PSTATE_DEBUG
            if(cbk->isPrecharging)
              crk->debug("PRECHARGE(ROW_MISS)", curTick, c, getRankByBank(b), getChannelBankWithinRank(b), true);
            #endif

            #if WR_DEBUG
            printf("PRECHARGE BKUID %u (msim.cc)\n",(c*BANKS_PCH)+b);
            #endif

            //if(debug)  
            //  printf("PRECHARGE_START (ROW_MISS) at %llu CH %d RK %d BK %d PRECHARGING\n", curTick, c, getRankByBank(b), getChannelBankWithinRank(b)); 

          } //else if(debug)  
            //printf("DO_NOT_PRECHARGE_BANK(ROW_HIT) at %llu CH %d RK %d BK %d FUTURE_REUSE\n", curTick, c, getRankByBank(b), getChannelBankWithinRank(b));
        } 

        else { // no access to the same bank: close it (bank idle)
          mc[c]->prechargeBank(curTick, cbk, crk); // calls incCurrActivityCounter if a precharge occurs

          #if PSTATE_DEBUG
          if(cbk->isPrecharging)
            crk->debug("PRECHARGE(NO_ACCESS)", curTick, c, getRankByBank(b), getChannelBankWithinRank(b), true);
          #endif

          #if WR_DEBUG
          printf("PRECHARGE BKUID %u (msim.cc)\n",(c*BANKS_PCH)+b);
          #endif

          //printf("starting_precharge crk->currActivityCounter++ (%u)\n",crk->currActivityCounter);
          //if(debug) 
          //  printf("PRECHARGE_START (NO_ACCESS) at %llu CH %d RK %d BK %d PRECHARGING\n", curTick, c, getRankByBank(b), getChannelBankWithinRank(b)); 
        }
      }

      if(cbk->prechargeEndsAt > curTick && cbk->prechargeEndsAt < earliest) {
        earliest = cbk->prechargeEndsAt;
        assert(cbk->isPrecharging);
      }
      #endif

      // finds the next busy_until event
      if(cbk->busy_until > curTick && cbk->busy_until < earliest)
        earliest = cbk->busy_until;

      bkUID++; // absolute bank address [0-NBANKS]
    }
  }
  return earliest;
}

// penalizes the active state and assigns the penalty to the powerdown state
inline void MainMemory :: fixCKE (Tick penalty, MemRank *crk) {
  crk->cyc_CKEhiact += penalty; //printf("   crk->cyc_CKEhiact += %llu\n", penalty);
  #if CLOSE_PAGE
  crk->cyc_CKElopre -= penalty; //printf("   crk->cyc_CKElopre -= %llu\n", penalty);
  #else
  crk->cyc_CKEloact -= penalty; //printf("   crk->cyc_CKEloact -= %llu\n", penalty);
  #endif
}

inline bool MainMemory :: anyBankHasAccesses (byte channel, byte rank) {
  byte begUID = (channel * BANKS_PCH) + (rank * BANKS_PRK);
  byte endUID = begUID + BANKS_PRK; //printf(" CHECKING BKUID [%d %d] from CH %d RK %d\n",begUID,endUID-1,channel,rank);
  for (byte b=begUID; b<endUID; b++) {
    if(sched[b]->findNew() != NULL) 
      return true;
  }
  return false;
}

inline void MainMemory :: setRankBusyUntil (byte channel, byte rank, Tick busyUntil) {
  MemChannel *cch = mc[channel];
  byte begBk = (rank * BANKS_PRK); // not absolutely unique (only within the channel)
  byte endBk = (begBk + BANKS_PRK); //printf(" (BUSY_UNTIL %llu CH %d RK %d BK [ %d %d ])\n",busyUntil, channel, rank, begBk, endBk-1);

  // setting rank's busy_until
  cch->rk[rank]->busy_until = busyUntil;

  // setting banks likewise
  for(byte b=begBk; b<endBk; b++) {
    cch->bk[b]->busy_until = busyUntil;
  }
}

void MainMemory :: updateCKE (Tick curTick) {
  //printf("\n%llu ************************************************************\n", curTick);
  MemRank *crk;
  for(byte c=0; c<NCHANNELS; c++) {
    for(byte r=0; r<RANKS_PCH; r++) {
      crk = mc[c]->rk[r];
      #if VALIDATE
      if(crk->getPrevActivityCounter() > BANKS_PRK) { 
        printf("FATAL: prevActivityCounter (%u) > BANKS_PRK (%d) CH %d RK %d\n", crk->getPrevActivityCounter(), BANKS_PRK, c, r); 
        exit(1);
      }
      #endif

      // computing state power of previous interval
      if(crk->getPrevActivityCounter() > 0) { // STANDBY
        //printf("   CH %d RK %d finTick %llu curTick %llu ACT (COUNTER %u waiting %d)",
        //       c,r,finalTick,curTick,crk->prevActivityCounter,crk->waiting);
        crk->cyc_CKEhiact += (curTick-finalTick);   //printf("   crk->cyc_CKEhiact += %llu\n", (curTick-finalTick));
      } else { // POWERDOWN or WAITING
        //printf("   CH %d RK %d finTick %llu curTick %llu PPD/APD",c,r,finalTick,curTick);
        #if CLOSE_PAGE
          crk->cyc_CKElopre += (curTick-finalTick); //printf("   crk->cyc_CKElopre += %llu (POWERDOWN)\n", (curTick-finalTick));
        #else
          crk->cyc_CKEloact += (curTick-finalTick); //printf("   crk->cyc_CKEloact += %llu (POWERDOWN)\n", (curTick-finalTick));
        #endif
          //mbchoi
          //printf("[mbchoi]   crk->cyc_CKElopre += %llu (POWERDOWN)\n", (curTick-finalTick));
          //embchoi
      }

      #if 1
      // deciding on actions for the future interval based on the current status
      if(crk->getCurrActivityCounter() == 0) {
        if(!crk->power_down) {
          if(anyBankHasAccesses(c, r)) { // wait for next access in active state
            crk->waiting = true;
            crk->incCurrActivityCounter(); // change activity counter here to allow continuity of active state
            #if PSTATE_DEBUG
            crk->debug("WAITING", curTick, c, r, -1, true);
            //printf("start_waiting crk->currActivityCounter++ (%u)\n",crk->currActivityCounter);
            //printf("###### ENTER_WAITING at %llu CH %d RK %d\n",curTick, c, r); //debug(); exit(0);
            #endif

          } else { // enter powerdown now
            crk->power_down = true;
            crk->powerDownAt = curTick;
            setRankBusyUntil(c, r, curTick + crk->getTPD()); // adding powerdown entry latency to all banks of this rank
            //printf("###### ENTER_POWERDOWN at %llu CH %d RK %d\n",curTick, c, r);
            crk->powerDownTransitions++;
          }
        }
      }
      #endif

      crk->copyCurrToPrevActivityCounter(); //crk->prevActivityCounter = crk->currActivityCounter;
    } 
  }
  finalTick = curTick; 
}

/******************************************************************************
 Updating the system and finding the next event: access issued or completed.
*******************************************************************************/
Tick MainMemory :: update (Tick curTick, Tick nextCpuAccessIssue) {
  MemRequest *r;
  Tick nextSch = INVALID_TICK, nextFin = INVALID_TICK, breakPoint = nextCpuAccessIssue; 

  //printf("****************************************\n");
  for(byte i=0; i<NBANKS; i++) {
    if(!sched[i]->empty()) { 
      r = sched[i]->top(); // sched[i] is a heap ordered by arrival time to bank i
      if(r->started == false) {

          startAccess(curTick, r);

      } else if(r->finTime == curTick) {
        //printf("READY: sched[%d] = id %d arr %llu sch %llu fin %llu\n",i,r->id,r->arrTime,r->schTime,r->finTime);
        sched[i]->pop(); 
        ready.push(r); //ready.append(r); //ready.push(r); // all accesses should go here because physical writes must be counted outside of msim

        MemRank *crk = mc[r->mda.ch]->rk[r->mda.rk];
        crk->decCurrActivityCounter(); // we finished one more access in this rank at curTick
        #if PSTATE_DEBUG
        crk->debug("ACCESS", curTick, r->mda.ch, r->mda.rk, r->mda.bk, false);
        #endif

        tryAdvanceDMA(curTick, r);
      }

      if(r->schTime < nextSch && r->schTime > curTick) nextSch = r->schTime; // ORIGINAL
      if(r->finTime < nextFin && r->finTime > 0) nextFin = r->finTime;
    }
  }

  // Further updates are possible if pages migrated, so check next event again.
  // In addition, new events (stated=false) may have entered the queues and we
  // might need another round at the curTick to get started.
  for(byte i=0; i<NBANKS; i++) {
    if(!sched[i]->empty()) {
      r = sched[i]->top(); // sched[i] is a heap ordered by arrival time to bank i
      
      if(!r->started && r->schTime < curTick) r->schTime = curTick; // NEW: someone that didn't have the chance to start before will start now

      if(r->schTime < nextSch && ((r->started && r->schTime > curTick) || (!r->started && r->schTime >= curTick))) nextSch = r->schTime;
      if(r->finTime < nextFin && r->finTime > 0) nextFin = r->finTime;
    }
  }

  // IMPORTANT: note that only events that get scheduled and executed in 'sched[i]' queues must update nextSch and nextFin
  if(!preReady.empty()) {
    r = preReady.top();
    if(r->finTime == curTick) {
      preReady.pop();
      ready.push(r); 
      stats->rbs.retired++;
    }
    if(r->finTime < breakPoint && r->finTime > 0) breakPoint = r->finTime; 
  }

  #if CLOSE_PAGE
  // CLOSE_PAGE: updates ranks states and may close pages; OPEN_PAGE: updates rank states; returns next bank transition time;
  Tick bkTransition = updateRankStates(curTick);
  breakPoint = MIN(MIN(breakPoint,bkTransition), MIN(nextSch, nextFin)); 
  #else
  breakPoint = MIN(breakPoint, MIN(nextSch, nextFin)); 
  #endif

  #if EVTDEBUG
  if(curTick>368752518) {
    printf("breakPoint %llu curTick %llu nextCpuAccessIssue %llu bkTransition %llu nextSch %llu nextFin %llu\n",
       breakPoint,curTick,nextCpuAccessIssue,bkTransition,nextSch,nextFin);
  }
  #endif

  #if VALIDATE
  if(breakPoint < curTick) {
    printf("FATAL: breakPoint %llu < curTick %llu\n",breakPoint,curTick);
    printf("nextCpuAccessIssue %llu\n",nextCpuAccessIssue);
    printf("bkTransition %llu\n",bkTransition);
    printf("nextSch %llu\n",nextSch);
    printf("nextFin %llu\n",nextFin);
  }
  #endif

  //printf("BP %llu ",breakPoint);

  if(breakPoint == curTick) 
    return curTick;

  if(breakPoint == INVALID_TICK) {
    printf("FATAL: no next event.\n");
    debug(); 
    exit(1); //return curTick;
  } 

  updateCKE (curTick);

  return breakPoint; // when is the next event?
}

MemRequest *MainMemory :: retireAccess() {
  if(!ready.empty()) {
    MemRequest *r = ready.top();
    ready.pop();
    inFlightAccesses--;

    #if TIME_DEBUG
    r->debug_req_simple("DONE");
    #endif

    if(r->isDMA == false) 
      stats->nonDma++;

    if(++stats->completed % 1000000 == 0) {
      printf("COMPLETED %llu\n",stats->completed);
    }

    return r;
  } 
  return NULL;
}

Addr MainMemory :: getMemSize () { 
  return TOT_MEM_SIZE; 
}

void MainMemory :: debug () {
  printf("+++++++++++++++++++++++++++++++++++++++++\n");
  printf("DEBUG SCREEN\n");
  printf("inFlightAccesses %u ready.size %u sbridge.size %llu\n",
         inFlightAccesses, (unsigned)ready.size(), sbridge.size());
  printf("+++++++++++++++++++++++++++++++++++++++++\n");
  for(byte i=0; i<NBANKS; i++) sched[i]->show(i);
  printf("+++++++++++++++++++++++++++++++++++++++++\n");
  printf("preReady.size %u\n", (unsigned)preReady.size());
  MemRequest *r;
  while(preReady.size() > 0) {
    r = preReady.top();
    printf("id %d arr %llu sch %llu fin %llu started %d dma %d ->",
           r->id, r->arrTime, r->schTime, r->finTime, r->started, r->isDMA);
    preReady.pop();
    delete r;
  }
  printf("NULL\n");
  printf("+++++++++++++++++++++++++++++++++++++++++\n");
}

MainMemory :: MainMemory (int type0, int type1, float rankProp, Addr defaultRankSize) {
  MemFeatures *f[MAXZONES];
  byte c, r, z, t[MAXZONES];
  t[0] = type0;
  t[1] = type1;

  atrans.totRanks = NCHANNELS * RANKS_PCH;
  atrans.zoneLimit = (byte)ceil(atrans.totRanks * rankProp); // zoneLimit = number of ranks in the first zone

  Addr znSize[MAXZONES]; // number of ranks belonging to each zone
  //mbchoi
  znSize[0] = atrans.zoneLimit;
  znSize[1] = atrans.totRanks - atrans.zoneLimit;
  //znSize[0] = 4;
  //znSize[1] = 1;
  //embchoi
  //mbchoi
  //printf("[mbchoi]: NCHANNELS:%d RANKS_PCH:%d totRanks:%d rankProp:%lf znSize[0]:%llu znSize[1]:%llu\n", NCHANNELS, RANKS_PCH, atrans.totRanks, rankProp, znSize[0], znSize[1]);
  //embchoi

  if(type0 != type1)
    assert(type0 != PCM); // luramos: I'm simplifying the design by assuming that if PCM is used (so, phantomZone >= 0)

  done = false;
  finalTick = 0;
  inFlightAccesses = 0;
  for(byte b=0; b<NBANKS; b++) sched[b] = new SchedQueue ();

  TOT_MEM_SIZE = 0;
  for(z=0; z<MAXZONES; z++) {
    f[z] = new MemFeatures(t[z], defaultRankSize, (type0!=type1)); // SMALL_DRAM: if the system is homogeneous, use a smaller DRAM
    TOT_MEM_SIZE += atrans.setZone(z, f[z], t[z], TOT_MEM_SIZE, znSize[z]);
  }

  printf("&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&\n");
  printf("Memory System\n");
  printf("&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&\n");
  printf("Total Channels: %d | Total Ranks: %d | Total Banks: %d | Row Size: %d | Cols/Row %d\n",
         NCHANNELS, atrans.totRanks, NCHANNELS*BANKS_PCH, ROW_SIZE, ROW_SIZE/COL_SIZE);

  printf("MEM_SIZE: %.2f MB [0x%016llx - 0x%016llx] (Rank Prop: %.3f)\n",(float)TOT_MEM_SIZE/(1024.0*1024.0),(Addr)0,(Addr)TOT_MEM_SIZE-1,rankProp);

  for(c=0; c<MAXZONES; c++) {
    printf("+ %llu %s (rank_size %.2fMB, bank_size %.2fMB) (Rows %llu)\n", 
           znSize[c], f[c]->MNAME(), 
           (float)atrans.mz[c]->rank_size/(1024.0*1024.0),
           (float)atrans.mz[c]->bank_size/(1024.0*1024.0), 
           atrans.mz[c]->bank_size/ROW_SIZE);
    f[c]->print();
  }
  
  byte crk_id = 0;
  for(c=0; c<NCHANNELS; c++) {
    mc[c] = new MemChannel();
    for(r=0; r<RANKS_PCH; r++) {
      z = atrans.getZoneByRank(crk_id); //(crk_id>=zoneLimit);
      //printf("Ch %d Rk %d (id %d) is of type %s belongs to ",c,r,crk_id,MNAME(mz[z]->type));
      //printf("ZONE %d (0x%016llx - 0x%016llx)\n",z,mz[z]->base_addr,mz[z]->max_addr);
      mc[c]->addRank(r,f[z]); // deprecated: all ranks of a channel should have the same features
      atrans.addChPages(c,f[z]->RK_SIZE()/pageSize);
      crk_id++;
    }
    mc[c]->validate();
  }

  atrans.adjustPhantomZone((t[0] != t[1] && atrans.mz[1]->type == PCM)); // luramos: phantomZone >= 0

  #if 1 // MAP BY BANK
  for(byte cbk_id=0; cbk_id<NCHANNELS*BANKS_PCH; cbk_id++) { 
    c = cbk_id / BANKS_PCH;
    r = cbk_id / BANKS_PRK;
    z = atrans.getZoneByRank(r); 
    Addr beg_addr = (cbk_id * atrans.mz[z]->bank_size);
    Addr end_addr = ((cbk_id+1) * atrans.mz[z]->bank_size)-1;
    if(z > 0 && atrans.phantomZone>0) { beg_addr -= atrans.phantomZone; end_addr -= atrans.phantomZone; }
    printf("CH %2d RK %2d (id %2d) BK %2d (id %2d) (ZN %2d) [0x%016llx - 0x%016llx] [%s]\n",
           c,r%RANKS_PCH,r,cbk_id%BANKS_PRK,cbk_id,z,beg_addr,end_addr,f[z]->MNAME());

    //MultiDimAddr a, b;
    //atrans.translate(beg_addr,&a);
    //atrans.translate(end_addr,&b);
    //printf("CH %2d RK %2d (id %2d) BK %02d-%02d (id %02d-%02d) (ZN %2d) [0x%016llx - 0x%016llx] [%s]\n",
    //       a.ch,a.rk,r,a.bk,b.bk,a.bkUID,b.bkUID,a.zn,beg_addr,end_addr,f[z]->MNAME());

    #if 0 // ROWS
    MultiDimAddr mda;
    for(; beg_addr<=end_addr; beg_addr+=ROW_SIZE) {
      //bzero(&mda,sizeof(MultiDimAddr));
      translate(beg_addr,&mda);
      printf("ch %2d rk %2d         bk %d row %d (0x%016llx)\n", mda.ch,mda.rk,mda.bk,mda.row,beg_addr);
      translate(beg_addr+ROW_SIZE-1,&mda);
      printf("ch %2d rk %2d         bk %d row %d (0x%016llx)\n", mda.ch,mda.rk,mda.bk,mda.row,beg_addr+ROW_SIZE-1);
    }
    translate(end_addr,&mda);
    printf("ch %2d rk %2d         bk %d row %d (0x%016llx)\n", mda.ch,mda.rk,mda.bk,mda.row,end_addr);
    #endif
  }
  #endif

  printf("&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&\n");
  //printf("DEBUG END\n"); exit(0);

  assert((FrameNum)(TOT_MEM_SIZE/4096) <= (FrameNum)0x7FFFFF); // MEM <= 32GB (our limit to save memory)
}

#if WRITEQ
void MainMemory :: initBanks (byte nCores) {
  for(byte b=0; b<NBANKS; b++) 
    sched[b]->init(nCores/2, nCores/4);
}
#endif

// To be called after the last access to figure out when the last access completed (and the simulation finished)
Tick MainMemory :: getFinishTime (Tick curTick) {
  Tick temp=0;
  byte c;

  //printf("\n$$$ GET_FINISH_TIME $$$$$$$$$$$$$$$$ \n");
  
  #if CLOSE_PAGE
  updateRankStates (curTick);
  #endif
  updateCKE(curTick); // updates finalTick of all channels simmultaneously

  if(!done) {
    for(c=0; c<NCHANNELS; c++) {
      for(byte b=0; b<BANKS_PCH; b++) // finding latest busy_time of all ranks in this channel
        if(mc[c]->bk[b]->busy_until > temp)
          temp = mc[c]->bk[b]->busy_until;
    }

    updateCKE(temp); // updates finalTick of all channels simmultaneously
    done = true;
  }

  return finalTick;
}

void MainMemory :: getPowerStats (MemPowerCounter *p, Tick curTick) {
  if(!done)
    getFinishTime(curTick); // finalTick will be automatically updated

  printf("\n&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&");
  p->reset(); // resetting the power stats
  for(byte c=0; c<NCHANNELS; c++) {
    printf("\n");
    printf("Ch%d->",c);
    mc[c]->getPowerStats(finalTick,p); // accumulating power stats across all ranks of all channels
  }
  printf("&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&\n\n");
}
