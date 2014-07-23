#ifndef SIM_OS_HH
#define SIM_OS_HH

#include <list>
#include "cpu_sim.hh"
#include "page_table.hh"
#include "config.hh"
#include "event.hh"

typedef list<Event*> IssueQueue;
typedef IssueQueue::iterator IQit;
typedef IssueQueue::const_iterator IQcit;

class SimOS {
  private:
    CpuSim *cpus[MAXCORES];
    TLB *tlb[MAXCORES];
    PageTable *page_table;
    MainMemory *mem;
    byte nCores, nCoresLeft;
    IssueQueue issueQ;
    LatencyCounter latQ, latNoQ;

    Tick lastTS; // DEBUG
    Counter prog; // DEBUG

  public:
    SimOS(MainMemory*);
    void init();
    void decCoresLeft();
    Event *processEvent(Event*);
    void accessMemory(Event*);
    bool tick();
    bool isDone();
    void printStats();
    #if PAGESTATS
    void printPageStats(char*);
    #endif

    Tick tryIssuing();
    void toIssueQ(Event*);
};

#endif
