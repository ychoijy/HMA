#include <stdlib.h>
#include "event.hh" 
#include "config.hh"
#include "sim_os.hh"
#include "cpu_sim.hh"
#include "mman.hh"
#include "page_table.hh"

class TraceReader;
class Event;

using namespace std;

TConfig *cfg;
TraceReader *trace_reader;
SimOS *simOS;
Tick globalTick;
MainMemory *mem;
OsStats *stats;
AddrTranslator atrans;

#if PAGESTATS
MemoryArrayStats arrayStats;
#endif

void usage(char *name)
{
  printf("%s cfg wload otag\n"
         "   cfg: configuration file\n"
         " wload: workload directory\n"
         "  otag: output tag (optional)\n"
         "\n",name);
}

int main(int argc, char** argv)
{
  if(argc != 4 && argc != 3) {
    usage(argv[0]); 
    exit(0);
  }

  char *cfile = argv[1]; // configuration file
  char *wload = argv[2]; // workload dir
  #if PAGESTATS
  char *otag = NULL;
  if(argc == 4)
    otag = argv[3]; // output dir
  else
    otag = NULL;
  #endif

  stats = new OsStats();

  printf(HLINE);
  printf("MINISIM 7.0 NOEC Module\n");
  printf(HLINE);
  printf("Configuration file: %s\n",cfile);
  cfg = new TConfig(cfile,1);

  printf("Searching for workload traces at: %s\n",wload);
  trace_reader = new TraceReader(wload);
  mem = new MainMemory(cfg->memtype[0], cfg->memtype[1], cfg->ratio, cfg->defaultRankSize);

  simOS = new SimOS(mem);
  simOS->init();
  bool alive;
  do { alive = simOS->tick(); } while(alive); // main simulation loop

  simOS->printStats();

  // shutting down
  if(trace_reader != NULL){
    delete trace_reader;
    trace_reader = NULL;
  }

  if(simOS != NULL){
    #if PAGESTATS
    if(otag != NULL)
      simOS->printPageStats(otag);
    #endif

    delete simOS;
    simOS = NULL;
  }

  printf(HLINE);
  return 0;
}
