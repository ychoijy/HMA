#ifndef _TCONFIG_HH
#define _TCONFIG_HH

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "msim.hh"

// Util
#define HLINE "=================================================================================================\n"
#define READ	'R' // regular read
#define WRITE	'W' // regular write
#define PFETCH	'F' // page fetch from disk
#define PWBACK	'B' // page writeback into disk

/*****************************************************************************
 DiskLatency = SeekTime + RotationalLatency + (4KB / TransferRate)
 Maxtor 5A300J0 (300GB 5400RPM)
 http://www.utronicsusa.com/products/HD/MAXTOR_hd.htm
 Page Transfer Time: 4KB / (46000KB/sec) = 0.09 ms; 
 Avg Seek Time: 10 ms
 5400 RPM: 60min / 5400 RPM * 0.5 (avg) = 5.56ms (RotationalLatency)
 Note: buffer cache block has size equal to 512BYTES (= size of disk block)
 PageSwapLatency = 0.09 ms + 10 ms + 5.56ms = 16ms = 64000000 CPU cycles
*******************************************************************************/
/*****************************************************************************
 Intel X25-E SATA Solid State Drive (SSD)
 Disk Parameter Value
 Page read latency     0.080 ms
 Page read power         2.6 W
 Idle power             0.06 W

Models:	X25-M (http://techreport.com/articles.x/17269)
Flash fabrication process: 50nm (Capacity: 80, 160GB, Cache: 16MB)
Max sequential access 250MB/s (reads) and 70MB/s (writes)
Latency: 85 µs (read) and 115 µs (write)
Max 4KB IOPS: 3,300 (write) and 35,000 (read)
Power consumption: 150 mW (active) and 60 mW (idle)

4K fetch ns: 85usec + 16usec (=4KB/250MB/s)
*******************************************************************************/
//#define DISK_LATENCY 100000 // ns (for Intel X25-M SSD)

typedef struct {
  bool used;
  char field[64];
  char val[128];
} ParserEntry;

class Parser {
  private:
    int n;
    ParserEntry tab[64]; // max of 64 entries in the config file

  public:
    Parser(char *fname) { 
      char str[256];
      FILE *f = fopen(fname,"r");
      if(f == NULL) {
        printf("FATAL ERROR: cannot open configuration file.\n");
        exit(1);
      }

      n = 0;
      ParserEntry *t = tab;
      while(!feof(f)) {
        if(fgets(str, 255,f) != NULL)
         if(sscanf(str,"%s %s %*s",(char*)&t->field, (char*)&t->val) > 0) { // searching line 
           if(t->field[0] != '#') { // ignoring coments
             t->used = false;
             //printf("READ: %s = %s\n", t->field, t->val);   
             n++; t++;
           }
         }
      }
      fclose(f);
    };

    char *getField(char const *field, bool optional) {
      for(int i=0; i<n; i++) {
        if(tab[i].used == false) {
          if(strcmp(tab[i].field,field)==0) {
            //printf(" * %s = %s\n",field,tab[i].val);
            tab[i].used = true;
            return tab[i].val;
          }
        }
      }

      if(optional) return NULL;

      printf("WARNING: field '%s' not found\n",field);
      exit(0);
    };
};

class TConfig {
  public:
    Addr defaultRankSize; 
    int memtype[MAXZONES];
    float ratio;
    byte trace_iterations;

    TConfig (char *fname, byte trace_iterations) {
      char *t;
      Parser *p = new Parser(fname);
      defaultRankSize = (Addr)atoll(p->getField("defRankSize",false));

      t = p->getField("zone",false);
      memtype[0] = (t[0]=='d' ? DRAM : (t[0]=='s') ? STT : PCM);
      t = p->getField("zone",false);
      memtype[1] = (t[0]=='d' ? DRAM : (t[0]=='s') ? STT : PCM);
      ratio = atof(p->getField("ratio",false));
	  //printf("mbchoi: ratio: %.4f\n", ratio);

      delete p;

      printf("[NORMAL ALLOCATION]\n");
      printf("CPU %0.2f GHz, MEM %.2f GHz, cpu_ratio %.2f\n", CPU_GHZ, MEM_GHZ, CPU_RATIO);
      printf("BUS_WIDTH_BYTES %u (bytes) CHIP_WIDTH %u (bits)\n", BUS_WIDTH_BYTES, CHIP_WIDTH);
      printf("pageSize %u (bytes) cacheBlockSize %u (bytes) cacheBlocksPerPage %u\n",
             pageSize, cacheBlockSize, cacheBlocksPerPage); 
      printf("tlbSize %u (entries per core) TLB_HIT_DELAY %u (cycles) TLB_MISS_DELAY %u (cycles)\n",
             tlbSize, (unsigned)TLB_HIT_DELAY, (unsigned)TLB_MISS_DELAY);
      printf("DISK_LATENCY %u (ns or %llu CPU cycles)\n",DISK_LATENCY,diskCycles);
      printf("memtypes %d and %d (ratio %.4f)\n",memtype[0],memtype[1],ratio);

      printf("-DPAGESTATS=%d\n"
             "-DXFER_DEBUG=%d -DSWAP_OVERHEAD=%d\n"
             "-DMONITOR=%d -DMONITORED=%d -DTIME_DEBUG=%d -DVALIDATE=%d\n"
             "-DSWAPPING=%d -DCH=%d -DCLOSE_PAGE=%d\n",
             PAGESTATS, 
             XFER_DEBUG, SWAP_OVERHEAD, 
             MONITOR, MONITORED, TIME_DEBUG, VALIDATE,
             SWAPPING, CH, CLOSE_PAGE);

      printf("DRAM_MULTIPLIER %.2f\n",DRAM_MULTIPLIER);

      this->trace_iterations = trace_iterations;
    }
};

#endif
