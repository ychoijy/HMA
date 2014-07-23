#ifndef CPU_SIM_HH 
#define CPU_SIM_HH

#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <stdint.h>
#include "msim.hh"
#include "config.hh"
#include "event.hh"

using namespace std;
class Event;
class CpuSim;
class CpuThread;

typedef vector<Event*> EventList;

class CpuSim {
  private:
    byte coreID;
    Tick delay;//total delay that needed to add to subsequent requests because of memory accesses
    bool done;
    EventList events;

  public:
    CpuSim();
    ~CpuSim();
    
    void setCoreID(byte);
    byte getCoreID();

    void enqueueEvent(Event*);
    void addDelay(Tick);
    Tick getDelay();
    bool start();
    bool tick(Tick);

    bool fillInstructions();
    bool processEvent(Event*);
    void authorize(); // authorize new accesses (authorizes core to proceed)
    bool isAuthorized();
    bool isDone();
    void setDone();
    Tick nextEventTS();
    Event *pushPage(Addr,bool,Event*); // inserts accesses to a whole page + last event
};

class Trace {
  public:
    FILE *f;
    byte ite; // number of iterations on each log file
    Tick lastIteTick; // take last tick when the iteration changes value
    bool done;
    char fname[1024];
};

class TraceReader {
  private:
    Trace trc[MAXCORES];

    //FILE *f[MAXCORES];
    unsigned nEvents;
    
    //bool done[MAXCORES];
    unsigned nFiles, nFilesLeft;
    

  public:
    TraceReader(char *);

    Event* readNextEvent(byte);

    bool isDone();

    unsigned getEventCount() { return nEvents; };

    unsigned getNumTraces() { return nFiles; };
};
#endif
