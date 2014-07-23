#include "event.hh"
#include "config.hh"
#include "sim_os.hh"
#include "cpu_sim.hh"
#include "msim.hh"
#include "mman.hh"
#include "page_table.hh"

#define ZONE0 0
#define ZONE1 1

class TraceReader;
class Event;

using namespace std;
extern TraceReader *trace_reader;
extern SimOS *simOS;
extern Tick globalTick;
extern OsStats *stats;
extern AddrTranslator atrans;

#if PAGESTATS
extern MemoryArrayStats arrayStats; 
#endif

SimOS::SimOS(MainMemory *mem) {
  this->mem = mem;
  nCoresLeft = nCores = trace_reader->getNumTraces();

  for(byte c=0; c<nCores; c++) {
    cpus[c] = new CpuSim();
    tlb[c] = new TLB();
  }
  page_table = new PageTable(mem);

  #if WRITEQ
  mem->initBanks((byte)nCores);
  #endif

  mem->setMMU(page_table);
  globalTick = 0;
  lastTS = 0;
  prog = 0;
}

void SimOS::init() { // few things that cannot be done at the constructor
  for(byte i=0; i<nCores; i++) {
    cpus[i]->setCoreID(i);
    if(!cpus[i]->start()) {
      cpus[i]->setDone(); 
      decCoresLeft();
    }
  }
}

void SimOS::decCoresLeft() {
  nCoresLeft--;
  printf("One core finished (%d left)\n",nCoresLeft);
}

/******************************************************************************
 * SWAPPING adds latency to and/or from disk (DISKLATENCY) to the timing of the
 * CPU that caused the page fault. We push back the memory operations that
 * would happen if pages on the disk would get actually transferred to memory
 * (memory writes) or vice-versa (memory reads). The event that originally
 * caused the page-fault is attached to the end of the issuing queue of the
 * respective thread. That is why we get a new value for event. The new event
 * corresponds to the first access of the DMA transfer resulting from the SWAP.
 *
 * Note: due to long simulation time, we skip the latency and traffic for cold
 * page faults (which also changes timing and energy). This warm-up phase ends
 * when the OS starts swapping (because the memor is full).
 ******************************************************************************/
Event *SimOS::processEvent(Event *event) {
  byte pid   = event->pid;
  Addr vaddr = event->vaddr;
  byte c     = event->cid;

  FrameNum ppn, vpn = (FrameNum)(vaddr >> pageSizeBits);
  bool dirty = false;

  //printf("BEFORE %016llx (%c %llu pid %u)",(Addr)event,event->itype,event->paddr,event->pid);
 
  try  { // TLB HIT

    ppn = tlb[c]->translate(vpn, pid); // may raise an exception catched below

    stats->core[c].tlbHits++;
    stats->tlbHits++;
    page_table->touch(ppn); // to update LRU bookkeeping
    event->addDelay(TLB_HIT_DELAY);
    cpus[c]->addDelay(TLB_HIT_DELAY);

    //printf("TLB HIT ppn %llu delayedTo %llu)\n",ppn,event->getIssueTick());

  } catch(bool x) { // TLB MISS 

    //printf("TLB MISS\n");
    stats->core[c].tlbMisses++;
    stats->tlbMisses++;
    event->addDelay(TLB_MISS_DELAY); 
    cpus[c]->addDelay(TLB_MISS_DELAY);

    dirty = !event->isRead; // WRITEs make the page dirty

    if(!page_table->lookUp(pid, vpn, &ppn, &dirty)) { // PAGE FAULT
      #if SWAPPING // PAGE FAULT ocurred
        // get unallocated frame or LRU
        page_table->getFreeFrame(pid, vpn, &ppn, &dirty); //printf("PAGE_FAULT requesting virtual addr %llu\n",event->paddr);

        #if WARMSTART
        if(page_table->isMemFull()) { // EXPERIMENTAL: warmup does not cause page fetches (only after the memory is full)
        #endif

          // add the overhead of the PAGE FAULT (passing base of virtual address)
          event->hold = true; //HOLD_PAGEFAULT; // offending access will be held while a DMA transfer occurs

            mem->scheduleDMA(c, ((Addr)ppn)<<pageSizeBits, dirty); // no translation necessary, use the PPN
            mem->lockSB(c, page_table->getLockPerm(ppn)); // OS cannot be selected as a LRU victim until DMA is completed

        #if WARMSTART
        }
        #endif

      #else // DEBUG  allocate new frame or replace LRU
        page_table->getFreeFrame(pid, vpn, &ppn, &dirty);
      #endif

      //printf("TLB MISS ppn %llu delayedTo %llu)\n",ppn,event->getIssueTick());

      stats->pageFaults++; 
      stats->core[c].pageFaults++;
    }

    // in case the entry was already cached, the physical address is updated inside tlb.associate
    tlb[c]->associate(vpn, ppn, pid);
  } 

  //printf(" AFTER %016llx (%d %llu pid %u)\n",(Addr)event,event->isRead,event->paddr,event->pid);

  // this is the base address of the page as far as the TLB and page tables can tell
  event->paddr = (((Addr)ppn)<<pageSizeBits) + (vaddr & (pageSize-1));

  if(event->isRead) { stats->reads++;  } else { stats->writes++; } 
  event->authorized = true; 
  return event;
}

Tick SimOS :: tryIssuing(){
  //printf("GTS %llu\n",globalTick);
  Event *e;
  //printf("ISSUING: ");
  while(!issueQ.empty()) {
    e = issueQ.front();
    if(e->getIssueTick() <= globalTick) {
      //printf("%llu -> ",e->getIssueTick());
      issueQ.pop_front();
      if(e->authorized) { // non-blocking acceses do not stop the CPU
		  //mbchoi
		  //printf("[mbchoi] funname:tryIssuing file:sim_os.cc e->paddr:%llu isRead:%d\n", e->paddr, e->isRead);
		  //embchoi
		//mbchoi
        //if(!mem->scheduleCpuAccess(e->cid, globalTick, e->paddr, e->isRead, e->hold)) { 
        if(!mem->scheduleCpuAccess(e->cid, globalTick, e->vaddr, e->isRead, e->hold)) { 
		//embchoi
          if(cpus[e->cid]->tick(bufDelay)) // 1 CPU cyc to enqueue write
            stats->core[e->cid].finTick = globalTick+bufDelay; // core finishes when it buffers last request
        } 
        delete e;
      } else {
        toIssueQ(processEvent(e)); // back to issue queue with the timestamp updated
      }
    } else break;
  }
  //printf("NULL\n");

  if(issueQ.empty())
    return INVALID_TICK;
  else
    return (issueQ.front())->getIssueTick();
}

void SimOS::toIssueQ(Event *e){ // insertion sort
  //if(e->authorized) printf("HOT\n"); else printf("COLD\n");
  #if 0 // DEBUG
  processEvent(e);
  #else
  //printf(">> NEW EVENT TS: %llu\n",e->getIssueTick());
  if(issueQ.empty()) {
    issueQ.push_back(e);
    return;
  }

  for(IQit it=issueQ.begin(); it!=issueQ.end(); it++) {
    if((*it)->getIssueTick() > e->getIssueTick()) {
      issueQ.insert(it,e);
      return;
    }
  }

  issueQ.push_back(e);
  //printf("ENQUEUEING ");
  //for(IQit it=issueQ.begin(); it!=issueQ.end(); it++) {
  //  printf("%llu -> ",(*it)->getIssueTick());
  //}
  //printf("NULL\n");
  #endif
}

void SimOS::printStats() {
  #if BIRDS && 0
  pplace->ms.printStats(globalTick);
  #endif

  MemPowerCounter *p = new MemPowerCounter();  
  //printf("globalTick %llu before -> ",globalTick);
  globalTick = mem->getFinishTime(globalTick);
  //printf("after %llu\n",globalTick);
  mem->getPowerStats(p, globalTick);
  double seconds = (double)globalTick/(cpuGhz * 1000000000.0); // delay
  //double joules = p->getJoules(seconds); // energy
  p->print(seconds);

  printf("\n");
  //printf("TOTAL_ENERGY: %lf J\n", joules);
  //printf("ED: %lf\n", joules * seconds);
  //printf("ED2: %lf\n", joules * seconds * seconds);

  if(mem->ready.size() > 0) printf("WARNING: there are pending accesses in the READY queue!\n");
  for(byte s=0; s<NBANKS; s++) {
    if(mem->sched[s]->size() > 0) printf("WARNING: there are pending accesses in the SCHED queue %d!\n",s);
  }

  double misses = (double)trace_reader->getEventCount();

  printf(HLINE);
  printf("Runtime: %.7lf seconds: %llu CPU cycles\n", seconds, globalTick);
  printf("\n");
  printf("MC.Total accesses %llu CPU %llu DMA %llu SB %llu NB %llu retired %llu\n",
         stats->completed,stats->nonDma, stats->completed-stats->nonDma,
         stats->toDisk+stats->fromDisk, stats->migReads+stats->migWrites,
         stats->rbs.retired);

  printf("\nAccess Breakdown\n");
  printf("CPU.CPU_reads    %8llu %6.2lf %%\n", stats->reads,     100.0 * (double)stats->reads / (double)stats->completed);
  printf("CPU.CPU_writes   %8llu %6.2lf %%\n", stats->writes,    100.0 * (double)stats->writes / (double)stats->completed);
  printf("DMA.page_fetches %8llu %6.2lf %%\n", stats->fromDisk,  100.0 * (double)stats->fromDisk / (double)stats->completed);
  printf("DMA.page_wbacks  %8llu %6.2lf %%\n", stats->toDisk,    100.0 * (double)stats->toDisk / (double)stats->completed);
  printf("DMA.xfer_reads   %8llu %6.2lf %%\n", stats->migReads,  100.0 * (double)stats->migReads / (double)stats->completed);
  printf("DMA.xfer_writes  %8llu %6.2lf %%\n", stats->migWrites, 100.0 * (double)stats->migWrites / (double)stats->completed);

  double pageHitRatio = (double)stats->rowHit / (double)(stats->rowMissBkOpen + stats->rowMissBkClosed + stats->rowHit);

  printf("\nMC.rowMissBkOpen %llu rowMissBkClosed %llu rowHit %llu pageHitRatio (%lf)\n", 
         stats->rowMissBkOpen, stats->rowMissBkClosed, stats->rowHit, pageHitRatio);

  printf("MC.writeQueueHits %llu\n",stats->writeQueueHits);

  #if PAGESTATS
  printf("\n");
  arrayStats.printStats(seconds);
  #endif

  printf("\n");
  latQ.print  ("MC.avgLatQ(ns)  ");
  latNoQ.print("MC.avgLatNoQ(ns)");

  printf("\n");
  stats->rbs.printStats();

  printf("\n");
  printf("CPU.TLB_hit_ratio %lf hits %llu misses %llu\n",
         (double)stats->tlbHits / (double)(stats->tlbHits+stats->tlbMisses),
         stats->tlbHits,stats->tlbMisses);
  printf("CPU.Miss_count: %.0lf %lf read_ratio %lf write_ratio\n",misses,
         (double)stats->reads / misses,(double)stats->writes / misses);
  printf("\n");
  printf("PT.page_faults %llu %.6lf %% of CPU misses\n",stats->pageFaults,100.0*(double)stats->pageFaults / misses);
  page_table->printStats(); 

  printf("\n");
  stats->printCoreStats(nCores);
  page_table->debug();
  delete p;
}

#if PAGESTATS
// otag is a directory name ans the partial name for an output file
void SimOS :: printPageStats(char *otag) {
  char *psfile = new char[strlen(otag)+16];
  sprintf(psfile,"%spagestat.dat",(otag==NULL)?"./":otag); // page stats file
  arrayStats.savePageStats(psfile);
  delete psfile;
}
#endif

/*****************************************************************************
 * The entire simulation will end when all these conditions are satisfied:
 * all cores have their event queues empty and their traces are done
 * there are no more in-flight accesses
 *****************************************************************************/
inline bool SimOS::isDone(){
  if(nCoresLeft == 0) {
    if(mem->inFlightAccesses == 0) {
      return true;
    }
  }
  return false;
}

bool SimOS::tick() {
  Tick now, next = INVALID_TICK;
  MemRequest *r;

  if(!issueQ.empty()) {
      next = tryIssuing();
    //printf("NEXT BREAKPOINT: %llu\n",next);
  } else if(isDone()) { // stop simulation
    return false;
  }

  //printf("SIMOS:: TICK========================================================\n");
  now = mem->update(globalTick, next);

  #if VALIDATE
  if(now == lastTS) { prog++; assert(prog<10); } else {prog=0;} // DEBUG
  #endif

  // any accesses to retire?
  while((r=mem->retireAccess()) != NULL) {
    if(r->id < MAXCORES && !r->isDMA) {
      if(r->isBlocking) { // CPU access (blocking = any access, except for buffered CPU writes)
        #if CPU_RW_DEBUG
        r->debug_req_min("END_B",globalTick);
        #endif

        assert ( cpus[r->id]->getCoreID() == r->id);
        //assert (r->finTime-r->arrTime > 0);

        if(cpus[r->id]->tick(r->finTime-r->arrTime)) // adds latency to core and advances its progress
          stats->core[r->id].finTick = r->finTime;
      } 
      #if CPU_RW_DEBUG
      else 
        r->debug_req_min("END_N",globalTick);
      #endif

      if(r->addToAvgMemLatency) {
        latQ.inc  (r->isRead, r->mda.zn, (r->finTime-r->arrTime));
        latNoQ.inc(r->isRead, r->mda.zn, (r->finTime-r->schTime));
      }

      if(nCoresLeft == 0) { 
        assert(issueQ.empty());
        return false;
      }
    } 
    
    delete r;
  }

  //if(now >= 2051068977) { printf("==========================\nTerminating simulation\n=============================="); exit(0); }

  if(now < globalTick) { // DEBUG
    printf("FATAL: now %llu is earlier than globalTick %llu\n",now,globalTick);
    mem->debug();
    exit(1);
  }

  globalTick = now;

  return true; // continue simulation
}
