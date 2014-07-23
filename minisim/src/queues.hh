#ifndef __QUEUES_HH__
#define __QUEUES_HH__
#include <list>
#include <queue>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include "base.hh"

using namespace std;
extern OsStats *stats;
extern AddrTranslator atrans;

typedef list<MemRequest*> Buffer;
typedef Buffer::iterator Bufit;
typedef Buffer::const_iterator Bufcit;

class SchedQueue {
  private:
    Buffer q;

    #if WRITEQ
    Buffer wq;
    MemRequest *head;
    bool wb_priority; // flushing wq (giving priority to writes) or not?
    byte hi_mark, lo_mark; // hi_mark activates the wq flushing; lo_mark deactivates it
    #endif

    void pushToQueue (Buffer &buf, MemRequest *r) { // using Insertion Sort
      if(buf.empty()) { buf.push_back(r); return; }
      if(r->arrTime > (buf.back())->arrTime) { buf.push_back(r); /*r->debug_req_detailed("PUSH");*/ return; }
      for(Bufit it=buf.begin(); it!=buf.end(); it++) {
        if((*it)->arrTime > r->arrTime && !(*it)->started) 
        {
          buf.insert(it,r);
          /*r->debug_req_detailed("PUSH");*/
          return;
        }
      }
      buf.push_back(r);
      /*r->debug_req_detailed("PUSH");*/
    };

  public:
    #if WRITEQ
      void init (byte hi, byte lo) { wb_priority = false; hi_mark = hi; lo_mark = lo;};
      bool empty() { return (q.empty() && wq.empty()); }; //can just check the head
      unsigned size () { return q.size() + wq.size(); };
      MemRequest *top() { return head; }
    #else
      bool empty () { return q.empty(); };
      unsigned size () { return q.size(); };
      MemRequest *top () { return q.front(); };
      void push(MemRequest *r) { pushToQueue(q,r); };
      void pop () { q.pop_front(); };
    #endif

    #if CLOSE_PAGE
    MemRequest *findNew () {
      for(Bufcit it=q.begin(); it!=q.end(); it++) {
        if(!(*it)->started) {
          return (*it);
        }
      }
      #if WRITEQ // need to check in the write queue too
      for(Bufcit it=wq.begin(); it!=wq.end(); it++) {
        if(!(*it)->started) {
          return (*it);
        }
      }
      #endif
      return NULL;
    };
    #endif

    bool inWriteQueue(Addr addr){
      #if WRITEQ
      for(Bufcit it=wq.begin(); it!=wq.end(); it++){
        if((*it)->addr == addr)
            return true;
      }
      #else
      for(Bufcit it=q.begin(); it!=q.end(); it++){
        if((*it)->addr == addr && !(*it)->isRead)
            return true;
      }
      #endif
      return false;
    };

    #if WRITEQ
     void selectHead () {
       if(wb_priority){ // flushing enabled: test disable
         if((wq.size() <= lo_mark) || (q.size() >= wq.size())){
           wb_priority = false; // enough writebacks have been flushed, return to normal read priority
         }
       } else { // flushing disabled: test enable
         if(wq.size() >= hi_mark && wq.size() > q.size()) {
           wb_priority = true; // activating machanism that flushes writesbacks with priority
         }
       }

       if(!q.empty() && !wq.empty()){
         #if VALIDATE
         assert(!(q.front()->started && wq.front()->started)); // only one may have started, not both
         #endif
         if(q.front()->started)
           head = q.front();
         else if(wq.front()->started)
           head = wq.front();
         else { //neither of fronts is started
           head = (wb_priority) ? wq.front() : q.front();
         }
       } else if(!q.empty() && wq.empty())
          head = q.front();
         else if(q.empty() && !wq.empty())
           head = wq.front();
         else
           head = q.front();
     };

     void push (MemRequest *r){
       if(r->isRead) {
         pushToQueue(q,r);
       } else {
         pushToQueue(wq,r);
       }
       selectHead();
     };

     void pop() {
       if(head == q.front()) { // printf("pop q.front\n");
         assert(!q.empty());
         q.pop_front();
       } else if(head == wq.front()) { // printf("pop wq.front\n");                  
         assert(!wq.empty());
         wq.pop_front();
       } else { printf("schedQueue->head == UNDEFINED\n"); exit(1); }
       selectHead();
     };
    #endif

    void showQueue (Buffer buf) {
      for(Bufcit it=buf.begin(); it!=buf.end(); it++) {
        MemRequest *r = (*it);
        //printf("id %d arr %llu sch %llu fin %llu started %d dma %d ->", 
        //       r->id, r->arrTime, r->schTime, r->finTime, r->started, r->isDMA);
        printf("id %d ppn %llu ar %llu sc %llu fi %llu %s %s ->", 
               r->id, r->getFrameNum(), r->arrTime, r->schTime, r->finTime, r->started?"serving":"waiting", r->isDMA?"dma":"cpu");
      }
      printf("NULL\n");
    };

    void show(byte tag){
      if(q.empty()) {
        printf("Q[%03d] empty\n",tag);
      } else {
        printf("Q[%03d] (size %d): ",tag, (unsigned)q.size());
        showQueue(q);
      }

      #if WRITEQ
      if(wq.empty()) {
        printf("W[%03d] empty\n",tag);
      } else {
        printf("W[%03d] (size %d, wpri %s):  ",tag, (unsigned)wq.size(), wb_priority?"true":"false");
        showQueue(wq);
      }
      #endif
    }

    //void append (MemRequest *r) { q.push_back(r); }; // for ReadyQueue
    //void push_front (MemRequest *r) { q.push_front(r); }; // for ReadyQueue
};


#if 1
  class minFinTimeComparator {
    public:
      bool operator() (MemRequest *a, MemRequest *b) const {
        return (a->finTime > b->finTime);
      }
  };
  typedef priority_queue <MemRequest*, vector<MemRequest*>, minFinTimeComparator> ReadyQueue;
#else
  typedef SchedQueue ReadyQueue;
#endif


class Xfer {
  public:
    Addr addr;
    bool isRead;
    Xfer(Addr addr, bool isRead) {
      this->addr = addr;
      this->isRead = isRead;
    }
};

typedef list<Xfer*> XferQueue;
typedef XferQueue::iterator XQit;

class IOXfer {
  public:
    Addr addr;
    bool isRead;
    IOXfer(Addr addr, bool isRead) {
      this->addr = addr;
      this->isRead = isRead;
    }
};

class MemManager;
class TConfig;

typedef list<IOXfer*> IOQueue;
typedef IOQueue::iterator IOQit;

class SouthBridgeController { // does page replacements
  private:
    MemRequest *buf[MAXCORES]; // holds offending CPU or southbridge DMA until matching northbridge access finish
    bool signal[MAXCORES]; // set to true when a DMA finishes and reset the first time it is verified with dmaJustCompleted
    IOQueue io[MAXCORES]; // southbridge accesses
    Tick arrTime[MAXCORES];
    bool *perm[MAXCORES]; // MMU entry of the page (avoids OS from replacing a page under DMA)

    MemRequest *retrieve (unsigned id, IOQueue q[]) {
      //printf("RETRIEVE id %u\n",id);
      //assuming 1 outstanding miss per core, if(ELEMENTS IN IOQUEUE == cacheBlocksPerPage, ADD DISK LATENCY to timestamp)
      Tick diskDelay = (q[id].size() == cacheBlocksPerPage || q[id].size() == (2*cacheBlocksPerPage)) ? diskCycles : 0;
      //Tick diskDelay = (q[id].size() == cacheBlocksPerPage) ? diskCycles : 0;
      IOXfer *x = q[id].front();
      q[id].pop_front();
      if(x->isRead) stats->toDisk++;
      else stats->fromDisk++;
      // returns the first memory access of the transfer corresponding to this core
      MemRequest *r = new MemRequest(id, arrTime[id] + diskDelay, x->addr, NULL, x->isRead, true);
      delete x;
      return r;
    };

    void unlock (unsigned id) {
      if(perm[id] == NULL) { printf("FATAL: SB unlocking inexisting lock\n"); exit(1); }
      *perm[id] = false;
       perm[id] = NULL;
      //printf("SB UNLOCKING id %d\n",id);
    };

    void dmaJustCompleted (unsigned id) {
      if(signal[id]) {
        unlock(id);
        signal[id] = false; // completion has been signaled
      }
    };

  public:
    SouthBridgeController() {
      bzero(buf,MAXCORES*sizeof(MemRequest*));
      bzero(signal,MAXCORES*sizeof(bool));
      bzero(perm,MAXCORES*sizeof(bool*));
    };

    void lock (unsigned id, bool *p) {
      #if VALIDATE
      if(perm[id] != NULL) { printf("SB->lock when already locked.\n"); exit(0); }
      #endif

      perm[id] = p;
      *p = true;
      //printf("SB LOCKING id %d\n",id);
    };

    MemRequest *hold (MemRequest *r) {
      #if VALIDATE
      if(buf[r->id] != NULL) { printf("FATAL: 2 accesses on hold for the same CPU\n"); exit(1); } // DEBUG
      if(io[r->id].empty()) { printf("FATAL: holding CPU access without matching SB DMAs\n"); exit(1); } // DEBUG
      #endif

      buf[r->id] = r;
      arrTime[r->id] = r->arrTime;

      #if TIME_DEBUG
      r->debug_req_simple("** SBRIDGE_DMA_BEGIN");
      #endif

      //printf("SB_HOLD id %d ppn %llu isRead %d\n",r->id, r->getFrameNum(), r->isRead);
      return retrieve(r->id,io);
    };

    Counter scheduleDMA (byte id, Addr paddr, bool dirty) {
      #if VALIDATE
      if(!io[id].empty()) {
        printf("FATAL: overlapping SouthBridge DMAs with same CPU id %d addr %llu dirty %d (xq.size %u nullBuf %d)\n",
               id,paddr,dirty,(unsigned)io[id].size(),(buf[id]==NULL)); exit(1);
      }
      #endif

      //printf("SCHEDULING DMA id %u paddr %016llx dirty %u\n",id,paddr,dirty);
      byte i;
      if(dirty) { // WRITEBACK //printf("dirty\n"); exit(0);
        for(i=0; i<cacheBlocksPerPage; i++) {
          io[id].push_back(new IOXfer(paddr+(i<<cacheBlockSizeBits),true)); // isRead=true; reading from MEM writing to disk or buffer
        }
      }

      for(i=0; i<cacheBlocksPerPage; i++) //PAGE FETCHES: now we need to read from the disk and write into memory
        io[id].push_back(new IOXfer(paddr+(i<<cacheBlockSizeBits),false)); // isRead=false; reading from DISK and writing to MEM

      return (Counter) io[id].size(); // returns the number of accesses added (assumes1 outstanding miss per core)
    };
    
    MemRequest *retrieveNext(unsigned id) {
      if(io[id].empty()) {// if transfer is done, returns the violating request
        MemRequest *r = buf[id];
        if(buf[id]==NULL) { printf("FATAL: SB no next io %u\n",(unsigned)io[id].size()); exit(1);}
        //printf("TRANSFER COMPLETE\n");
        buf[id] = NULL;
        signal[id] = true; // signal completion once

        #if TIME_DEBUG
        r->debug_req_simple("** SBRIDGE_DMA_COMPLETED");
        #endif
          
        //printf("SOUTHBRIDGE RELEASING ACCESS id %d\n",id);
        dmaJustCompleted(id); // unlocks previously locked frame

        return r;
      } else { 
        return retrieve(id,io);
      }
    };

    Counter size() { // for DEBUG
      Counter n=0;
      for(unsigned i=0; i<MAXCORES; i++) if(!io[i].empty()) n += (Counter)(1+io[i].size());
      return n;
    };
};

#endif // QUEUES_HH
