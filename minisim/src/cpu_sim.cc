#include "event.hh"
#include "config.hh"
#include "cpu_sim.hh" 
#include "sim_os.hh"

class Event;
class TraceReader;
class Event;
class SysCallEvent;
class Event;

extern SimOS *simOS;
extern Tick globalTick;
extern TraceReader *trace_reader;
extern TConfig *cfg;

/******************************************************************************
 * CPU Sim
*******************************************************************************/
CpuSim::CpuSim(): coreID(0), delay(0){
  done = false;
}

CpuSim::~CpuSim() {
  events.clear();
}

bool CpuSim::fillInstructions(){
  Event *e = trace_reader->readNextEvent(coreID);
  if(e == NULL) 
    return false;

  //printf("CORE %d LAST TS: %llu\n",e->cid, e->getIssueTick());
  enqueueEvent(e);
  return true;
}

void CpuSim::setCoreID(byte cid){
  coreID = cid;
}

byte CpuSim::getCoreID(){
  return coreID;
}

void CpuSim::addDelay(Tick cycles){
  delay += cycles;
}

Tick CpuSim::getDelay(){
  return delay;
}

void CpuSim::enqueueEvent(Event* event){
	//mbchoi
	//printf("[mbchoi] funname:enqueueEvent file:cpu_sim.cc vaddr:%llu paddr:%llu\n", event->vaddr, event->paddr);
	//embchoi
  events.push_back(event); 
}

bool CpuSim::isDone(){
  return done;
}

void CpuSim::setDone(){
  done = true;
  simOS->decCoresLeft();
}

bool CpuSim::start() {
  bool hasWorkload = fillInstructions();
  stats->core[coreID].iniTick = events[0]->getIssueTick();
  tick(0); // add no delay to the CPU, it starts at iniTick
  return hasWorkload;
}

bool CpuSim::tick(Tick accessDelay) {
  if(events.empty()){ // nothing else to do on this core
    if(!fillInstructions()) {
      addDelay(accessDelay);
      stats->core[coreID].totalLatency += accessDelay;
      //printf("TOTALLATENCY += %llu\n",accessDelay);
      //printf("CORE %d FINAL DELAY %llu\n",coreID, delay);
      setDone();
      return true; // done
    }
  }

  addDelay(accessDelay);
  stats->core[coreID].totalLatency += accessDelay;
  //printf("TOTALLATENCY += %llu\n",accessDelay);

  events[0]->setDelay(delay);
  simOS->toIssueQ(events[0]); //simOS->processEvent(events[0]);
  events.erase(events.begin());
  return false; // not done
}

Tick CpuSim::nextEventTS(){
  //if(events.empty()){ //no more instructions
  //  printf("FATAL: asked for nexte event without ticking or after done.\n");
  //  exit(1);
  //}

  return (events[0]->getIssueTick() + delay);
}

/******************************************************************************
 * CpuTraffic
 * We assume that 1 process runs on 1 core from the beginning to the
 * end.  Under this assumption, making pid = coreID is valid, since pid is
 * unique, and each process will have a separate virtual address space.
*******************************************************************************/
TraceReader::TraceReader(char *dirName) {
  Trace *t;
  nEvents=0;
  nFiles=0;
  
  // dirName is the directory where the multiple input traces will be found (1 per CPU core)
  for(unsigned i=0; i<MAXCORES; i++) {
    t = &trc[i];
    t->done = true;
    sprintf(t->fname,"%s/core%1x.trc",dirName,i);
    if((t->f = fopen(t->fname,"r"))!=NULL) { // if there is a trace, we're not done
      t->done = false;
      t->ite = cfg->trace_iterations;
      t->lastIteTick = 0; // update this value whenever ite changes value
      nFiles++;
      printf(" * trace file: %s\n",t->fname);
    }
  }

  nFilesLeft = nFiles;
  printf("Total number of CPU cores: %u\n", nFiles);
}

bool TraceReader::isDone(){
  return (nFilesLeft == 0);
}

Event* TraceReader::readNextEvent(byte coreID) {
  Event *e = NULL;
  byte itype;
  Tick ts;
  Addr vaddr;
  Trace *t = &trc[coreID];

  if(t->done) return NULL;
  //printf("Reading event on core %d\n",coreID);

  if(!feof(t->f)) {
    if(fscanf(t->f,"%llu %llu %c\n", &ts, &vaddr, &itype)<0) {
      printf("Error: cannot read from trace file.\n");
      exit(1);
    } else {
      //fprintf(stdout,"JUST READ [%llu %u %016llx %c]\n", ts, coreID, vaddr, itype);
      e = new Event(t->lastIteTick + ts, coreID, coreID, vaddr, itype);
      nEvents++;
      stats->core[coreID].nAccesses++;

      if(stats->core[coreID].lastTraceTick > ts) { 
        printf("FATAL: core%d lastTraceTick %llu > currTS %llu (unsorted trace!)\n", coreID, stats->core[coreID].lastTraceTick, ts); 
        exit(1);
      };
      stats->core[coreID].lastTraceTick = ts;
    }
	//mbchoi
	//printf("[mbchoi] funname:readNextEvent file:cpu_sim.cc vaddr:%llu isRead:%c\n", vaddr, itype);
	//embchoi
  }

  if(feof(t->f)) {
    fclose(t->f);
    if(--t->ite == 0) { // no more iterations on this trace file
      t->done = true;
      nFilesLeft--; 
    } else { // iterate once more on this trace
      if((t->f = fopen(t->fname,"r")) == NULL) {
        printf("FATAL: cannot reopen trace file\n"); 
        exit(1);
      }
      // timestamp of the last event in the trace will serve as base for the next iteration
      t->lastIteTick = (t->lastIteTick + ts); 
      printf("Trace %d new iteration\n",coreID);
    }
  }

  return e;
}
