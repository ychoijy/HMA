#ifndef __BASE_HH__
#define __BASE_HH__
#include <map>
#include <vector>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>
#include <list>

#define DRAM 0
#define STT 1
#define PCM 2

#define LINEAR 0
#define INTERLEAVED 1
#define INVALID -1

#define MAXCORES 16 // maximum number of cores in all CPUs
#define MAXZONES  2

using namespace std;

#define MIN(a,b) (((a)<(b)) ? (a) : (b))
#define MAX(a,b) (((a)>(b)) ? (a) : (b))

typedef unsigned long long Tick;
typedef unsigned long long Addr;
//typedef unsigned FrameNum; // = log2 (Addr/PageSize) bytes -> unsigned seems to consume more memory than ULL in the end!
typedef unsigned long long FrameNum; // = log2 (Addr/PageSize) bytes
typedef unsigned long long Counter;
typedef uint8_t byte; // 8 bits unsigned
typedef int8_t sbyte; // 8 bits with sign

static const double PCM_ENDURANCE = 100000000.0; // writes PCM
static const double OTH_ENDURANCE = 10000000000000000.0; // writes DRAM/STT
//mbchoi
//static const byte NCHANNELS = CH; //4; // 4
//#define RANKS_PCH 1 // must be a macro because there are pre-processor statements depending on it (static const byte RANKS_PCH = 1; // 2)
static const byte NCHANNELS = 1;
#define RANKS_PCH 5
//embchoi
static const byte BANKS_PCH = (8*RANKS_PCH); // make sure BANKS_PCH >= RANKS_PCH and divisible by RANKS_PCH

#define CHIP_WIDTH 8 // 8 bits per chip (DQ pins = x8 mode)
#define CHIPS_PRK 8 // the total size of a row

//mbchoi
//#if 1 // 8KB pages and row buffer
#if (0) // 8KB pages and row buffer
//embchoi
static const unsigned pageSize = 8192; // bytes (8KB pages)
static const unsigned pageSizeBits = 13; // log2(pageSize)  
static const unsigned CHIP_ROW_SIZE  = 0x400; // in bytes = 1KByte per chip = 8Kbit per chip (with 8-bit cols, because of x8 CHIP_WIDTH)
#else // 4KB pages and row buffer
static const unsigned pageSize = 4096; // bytes (4KB pages)
static const unsigned pageSizeBits = 12; // log2(pageSize)  
static const unsigned CHIP_ROW_SIZE  = 0x200; // in bytes = 512bytes per chip = 4Kbit per chip (with 8-bit cols, because of x8 CHIP_WIDTH)
#endif

static const unsigned CHIP_COL_SIZE  = CHIP_WIDTH; // for every chip the column size is the chip width
// Note: ROW_SIZE and COL_SIZE are here for an entire bank (for addressing purposes)
static const unsigned ROW_SIZE  = CHIP_ROW_SIZE * CHIPS_PRK; // in bytes
static const byte COL_SIZE  = (CHIP_WIDTH * CHIPS_PRK) / 8; // in bytes

static const byte BANKS_PRK = (BANKS_PCH/RANKS_PCH); // banks per rank
static const byte NBANKS = (BANKS_PCH*NCHANNELS); // absolute total number of banks

#define DRAM_ROWS_PER_BANK(x) ((x*RANKS_PCH)/(ROW_SIZE*BANKS_PCH)) 
#define DEV_GHZ 0.667 //0.667 // DDR3-1333 devices run at 667MHz
#define MEM_GHZ DEV_GHZ // DDR3-1333 with 2GHz memory controller
#define CPU_GHZ 2.668
#define CPU_RATIO (CPU_GHZ/MEM_GHZ)
#define BUS_WIDTH_BYTES 8 // 64-bit channel

#define XFER_ENGINE_ID 666
#define SRAM_CACHE_ID 888
#define WBUF_ID (MAXCORES) // write buffer ID

static const unsigned cacheBlockSize = 64; // bytes
static const unsigned cacheBlockSizeBits = 6; // log2(cacheBlockSize)
static const unsigned cacheBlocksPerPage = pageSize / cacheBlockSize; // pageSize / 64-byte cache blocks
static const unsigned cacheBlocksPerRow = ROW_SIZE / cacheBlockSize; // 4KB or 8KB / 64-byte cache blocks

static const Tick INVALID_TICK = (Tick) INVALID;
static const Addr INVALID_ADDR = (Addr) INVALID;
static const byte INVALID_BYTE = (byte) INVALID;
static const sbyte INVALID_QUEUE = (sbyte) INVALID;
static const FrameNum INVALID_PPN = (((FrameNum) INVALID)/pageSize);

static const double cpuGhz = CPU_GHZ;
#define SRAM_LAT_NSEC 1 // SRAM latency in nanoseconds
static const Tick SRAM_LATENCY = (Tick)(SRAM_LAT_NSEC*CPU_GHZ); // CPU cycles
static const Tick bufDelay = SRAM_LATENCY;
#define DISK_LATENCY 100000 // ns (for Intel X25-M SSD)

#if ENABLE_TLB
#define TLB_HIT_DELAY SRAM_LATENCY // CPU cycle
#define TLB_MISS_DELAY (15*SRAM_LATENCY) // CPU cycles (time for 2 mem lookups)
#else
#define TLB_HIT_DELAY 0
#define TLB_MISS_DELAY 0
#endif
static const Tick diskCycles = (Tick)(DISK_LATENCY * cpuGhz); // DISK_LATENCY in CPU cycles

static const unsigned MAX_CONCURRENT_TRANSFERS = 15000; //10000;
static const unsigned nThreadsPerCore = 1;
static const unsigned tlbSize = 128; // TLB entries = 128 entries per core

//#define HOLD_DEFAULT   0x00 // do not hold this access
//#define HOLD_PAGEFAULT 0x01 // hold current acces and issue southbridge DMA

typedef vector <bool> Bitmap;

class MemFeatures {
  private:
    Addr rk_size;
    char mname[8];
 
  public:
    // burstSize: burst length divided by 2 (DDR3)
    static const byte burstLength = ((cacheBlockSize + (BUS_WIDTH_BYTES - 1)) / BUS_WIDTH_BYTES); 
    static const byte tBurst = (burstLength / 2); // tBurst: burst length divided by 2 (DDR3)
    sbyte tRTP, tWR; // 5 and 10 respectivelly

    // Power and timing parameters for DDR3-1333 -15 667MHz (1GB), memory-independent parameters
    static const double VDD = 1.5, VDDmax = 1.5; //1.5; //1.65; // volts
    static const double DD2P=40, DD2N=65, DD3P=40, DD3N=62, DD4W=220, DD4R=200; // mA: assuming fast exit for precharge

    // Power and timing parameters for DDR3-1333 -15 667MHz (1GB), memory-dependent parameters
    double DD0, DD0R, DD5A, VDDR, VDDW; // array read/write avg current (mA), refresh current (mA) read/write voltages (V)

    static const double tCK = (1.0/DEV_GHZ); //1.5; // nanoseconds
    static const sbyte DQ = CHIP_WIDTH, DQS = 2, DQM = 1;
    
    // circuitry-dependent timing parameters in clock cycles (tCK) (forMICRON 1333 MT/s)
    static const sbyte tCMD=1, tDQ=1;
    static const sbyte tPD=3, tXP=4; // entry and exit latency for the fast-exit power down mode

    static const sbyte tCL=10, tCWD=tCL-3; // tDAL = tWR + tRP (in auto precharge mode)

    double tREFI;
    static const sbyte tFAW=20, tWTR=5;

    // memory-dependent timing parameters
    int tRCD, tRP, tRRDact, tRRDpre, tRFCmin, tRC, tRAS; 
    byte mtype;

    //#if OP_ENERGY // pre-calculated operation energy
    //double arr_r, arr_w, rb_r, rb_w; // array and row buffer operation energy in nanoJoules
    //double vs, fs, chips, specScale;
    //#endif

    #if (RANKS_PCH > 1)
    static const double PdqRD = 6.6, PdqWR = 0, PdqRDoth = 15.2, PdqWRoth = 12.5; // mW
    #else
    static const double PdqRD = 4.9, PdqWR = 9.3, PdqRDoth = 0, PdqWRoth = 0; //mW
    #endif

    MemFeatures(int, Addr, bool);
    Addr RK_SIZE() { return rk_size; };
    char *MNAME() { return mname; };

    void print () {
      printf("  %s (type %d) power_and_timing (x%d DQ %d DQS %d DQM %d)\n",mname,mtype,CHIP_WIDTH,DQ,DQS,DQM);
      printf("  VDD %.2lf VDDR %.2lf VDDW %.2lf VDDmax %.2lf tCK_ns %.2lf DEV_GHZ %.2lf \n",VDD,VDDR,VDDW,VDDmax,tCK,DEV_GHZ);
      printf("  DD0 %.0lf DD2P %.0lf DD2N %.0lf DD3P %.0lf DD3N %.0lf DD4W %.0lf DD4R %.0lf DD5A %.0lf\n",DD0,DD2P,DD2N,DD3P,DD3N,DD4W,DD4R,DD5A);
      printf("  cell_independent_memcyc tCMD %d tDQ %d tFAW %d tCL %d tCWD %d tWR %d\n", tCMD, tDQ, tFAW, tCL, tCWD, tWR);
      printf("  cell_dependent_memcyc tRCD %d tRP %d tRDDact %d tRDDpre %d tRFCmin %d tRC %d tRAS %d tRFCmin:%d tREFI:%f\n", tRCD, tRP, tRRDact, tRRDpre, tRFCmin, tRC, tRAS, tRFCmin, tREFI);

      //#if OP_ENERGY
      //printf("\n");
      //printf("  arrayReadEnergy(J/block)   %12.9lf  nJ/bit %10.7lf\n", arr_r, (arr_r * 1000000000.0)/(double)(cacheBlockSize*8));
      //printf("  arrayWriteEnergy(J/block)  %12.9lf  nJ/bit %10.7lf\n", arr_w, (arr_w * 1000000000.0)/(double)(cacheBlockSize*8));
      //double tmp = getBufferReadJoules(tBurst);
      //printf("  bufferReadEnergy(J/block)  %12.9lf  nJ/bit %10.7lf\n", tmp,  (tmp  * 1000000000.0)/(double)(cacheBlockSize*8));
      //tmp = getBufferWriteJoules(tBurst);
      //printf("  bufferWriteEnergy(J/block) %12.9lf  nJ/bit %10.7lf\n", tmp,  (tmp  * 1000000000.0)/(double)(cacheBlockSize*8));
      //#endif

      printf("\n");
    };

    //#if OP_ENERGY
    //double getBufferReadJoules (Tick);
    //double getBufferWriteJoules (Tick);
    //double memToNanoSec (Tick memCyc) {
    //  // memCyc / 10^9 cycles / second
    //  return ((double)memCyc*tCK); //(memCyc/MEM_GHZ);
    //};
    //#endif
};

#if PAGESTATS
class PageStatsEntry {
  public:
    Counter cpuReads, cpuWrites; // CPU-originated row hits or misses to this page
    Counter arrayReads;
    Counter cbArrayWrites[cacheBlocksPerPage];
    byte wasSrc, wasDst, wasRnd, wasRep; // counters for swaps and replacements: not too often
    bool pristine;

    PageStatsEntry(){
      cpuReads = cpuWrites = arrayReads = 0;
      wasSrc = wasDst = wasRnd = wasRep = 0;
      bzero(cbArrayWrites,cacheBlocksPerPage * sizeof(Counter));
      pristine = true;
    };

    void print(FILE *f, FrameNum n) {
      Counter sum=cbArrayWrites[0]; // sum of the writes to the page (all cache lines summed up)
      Counter worst=cbArrayWrites[0]; // most written cache block within this page
      for(byte i=1; i<cacheBlocksPerPage; i++) {
        sum += cbArrayWrites[i];
        if(worst < cbArrayWrites[i]) 
          worst = cbArrayWrites[i];
      }
      fprintf(f,"%8llu %8llu %8llu %8llu %8llu %8llu %8u %8u %8u %8u\n", 
              n, worst, sum, arrayReads, cpuReads, cpuWrites, wasSrc, wasDst, wasRnd, wasRep);
    };
};

typedef map<FrameNum,PageStatsEntry*> PSmap;
typedef pair <FrameNum,PageStatsEntry*> PSpair;
typedef PSmap::iterator PSit;

class MemoryArrayStats {
  private: 
    Counter totalZoneWrites[MAXZONES]; // number of writes to this zone
    Counter zoneWrittenPages[MAXZONES]; // number of pages of this zone that have been written
    Counter hiWritesToZoneCB[MAXZONES]; // number of writes to the most written cache block (UID) of each zone
    Addr hiWritesCBUID[MAXZONES]; // CBUID = cach block unique ID of the most written cacheBlocks
    bool newPage; // indicates whether the last searched page is new or not
    PSmap pages; // for every page that has been written/migrated/replaced, we keep an entry
    PageStatsEntry *find (FrameNum);

  public:
    MemoryArrayStats() {
      bzero(totalZoneWrites,MAXZONES*sizeof(Counter));
      bzero(zoneWrittenPages,MAXZONES*sizeof(Counter));
      bzero(hiWritesToZoneCB,MAXZONES*sizeof(Counter));
    };

    void countCpuRead (Addr);
    void countCpuWrite (Addr);
    void countArrayRead (Addr);
    void countArrayWrite (Addr);
    void countWasSrc (FrameNum);
    void countWasDst (FrameNum);
    void countWasRnd (FrameNum);
    void countWasRep (FrameNum);
    void savePageStats (char*); 
    void printStats(double);
    double getAvgBlockWrites (byte z) { return ((double)totalZoneWrites[z])/cacheBlocksPerPage; };
};
#endif

typedef map <Addr, Counter> CBWrites; // counts writebacks to a CacheBlockUID (unique accross entire memory)
typedef pair <Addr, Counter> CBWritesPair;
typedef CBWrites::iterator CBWritesIt;

class CoreStats {
 private:
    double getRuntime () {
      return ((double)(finTick-iniTick)) / (cpuGhz * 1000000000.0);
    };

    double getAvgRequestNanoSeconds() {
      return (nAccesses > 0) ? (((double)totalLatency / (double)nAccesses) / cpuGhz) : 0.0;
    };

    double getProcTime () {
      return (nAccesses > 0) ? ((lastTraceTick-iniTick) / (cpuGhz * 1000000000.0)) : 0.0;
    };

  public:
    Tick iniTick, finTick, lastTraceTick;
    Counter pageFaults;
    Tick totalLatency;
    Counter nAccesses;
    Counter tlbHits, tlbMisses;
    Tick timeProcessing, timeAccessing, timeDisk; // all CoreStats initialized in OsStats with a bzero

    void printStats (byte i) {
      double rtime = getRuntime();
      double procPct = 100.0 * (getProcTime() / rtime);
      printf("%3d   %.7lf %8.2lf %8.2lf %12llu %12llu %12llu %12llu %12llu %12llu %8.2lf\n",
            i, rtime, procPct, 100.0-procPct,
            iniTick, finTick, pageFaults, nAccesses, tlbHits, tlbMisses, getAvgRequestNanoSeconds());
    };    
};

class LatencyCounter {
  private:
    Tick rLat[MAXZONES], wLat[MAXZONES];
    Counter rCount[MAXZONES], wCount[MAXZONES];

  public:
    LatencyCounter () {
      bzero(rLat,MAXZONES*sizeof(Tick));
      bzero(wLat,MAXZONES*sizeof(Tick));
      bzero(rCount,MAXZONES*sizeof(Counter));
      bzero(wCount,MAXZONES*sizeof(Counter));
    };

    void inc (bool isRead, byte z, Tick diff) {
      if(isRead) {
        rLat[z] += diff;
        rCount[z]++;
      } else {
        wLat[z] += diff;
        wCount[z]++;
      }
    };
    
    void print (const char *label) {
      for(byte z=0; z<MAXZONES; z++) {
        printf("%s zone%d read(ns) %.4lf write(ns) %.4lf overall(ns) %.4lf\n",
               label, z, 
               ceil((double)rLat[z] / (double)rCount[z]) * (1.0/cpuGhz),
               ceil((double)wLat[z] / (double)wCount[z]) * (1.0/cpuGhz),
               ceil((double)(rLat[z]+wLat[z]) / ((double)(rCount[z]+wCount[z]))) * (1.0/cpuGhz) );
      }
    };   
};

#if 1 
class RBStatsCounter {
  public:
    static const byte nres=5, nops=2; // number of resouces, number of options per resource
    Counter c[nres][nops]; // [resource] x [missBkOpen/missBkClosed/hit]

    RBStatsCounter () { bzero(c, (nres*nops)*sizeof(Counter)); };

    void increment (byte res, byte opt) {
      #if VALIDATE
      assert(res < nres);
      assert(opt < nops);
      #endif
      c[res][opt]++;
    };

    Counter getResMisses (unsigned r) { return (c[r][0]); };
    Counter getResHits (unsigned r) { return (c[r][1]); };

    double getResourceHitRate (unsigned r) {
      double den = (double)(getResMisses(r) + getResHits(r));
      return (den > 0) ? ((double)getResHits(r))/den : 0.0;
    }; 

    void printStats(const char *label){
      printf("%23s %9s %9s %10s\n","row_hit_rate","hits","misses","total");
      printf("%s.cpur %10lf %10llu %10llu %10llu\n", label, getResourceHitRate(0), getResHits(0), getResMisses(0), getResHits(0)+getResMisses(0));
      printf("%s.cpuw %10lf %10llu %10llu %10llu\n", label, getResourceHitRate(1), getResHits(1), getResMisses(1), getResHits(1)+getResMisses(1));
      printf("%s.sb   %10lf %10llu %10llu %10llu\n", label, getResourceHitRate(2), getResHits(2), getResMisses(2), getResHits(2)+getResMisses(2));
      printf("%s.nb   %10lf %10llu %10llu %10llu\n", label, getResourceHitRate(3), getResHits(3), getResMisses(3), getResHits(3)+getResMisses(3));
    };
};

enum rbs_type { cpur=0, cpuw=1, sb=2, nb=3, sram=4 }; // nres=5;
enum rbs_op { miss=0, hit=1 }; // nops=2; 

class RowBufferStats {
  private:
    //static const byte cpur=0, cpuw=1, sb=2, nb=3, sram=4; // nres=5;
    //static const byte miss=0, hit=1; // nops=2; 
    RBStatsCounter zone[MAXZONES];

    byte getResourceName (unsigned id, bool isDMA, bool isRead) {
      if(id<XFER_ENGINE_ID) {
        return isDMA ? sb : (isRead ? cpur : cpuw);
      } else {
        return nb;
      }
    };

  public:
    Counter retired;
    RowBufferStats() { retired=0; };

    void incRowMissBkOpen (unsigned id, byte z, bool isDMA, bool isRead) {
      zone[z].increment(getResourceName(id,isDMA,isRead), miss); // row miss with bank open
    };

    void incRowMissBkClosed (unsigned id, byte z, bool isDMA, bool isRead) {
      zone[z].increment(getResourceName(id,isDMA,isRead), miss); // row miss, because the bank was closed
    };

    void incRowHit (unsigned id, byte z, bool isDMA, bool isRead) {
      zone[z].increment(getResourceName(id,isDMA,isRead), hit); // row hit
    };

    void printStats () {
      printf("Row Hit Rates\n");
      zone[0].printStats("zone0");
      printf("\n");
      zone[1].printStats("zone1");
      printf("\nTotal_retired_accesses %llu\n",retired);
    };
};
#endif

class OsStats {
  public:
    Counter completed, nonDma;
    Counter writes;
    Counter reads;
    Counter pageFaults;
    Counter tlbHits;
    Counter tlbMisses;
    Counter fromDisk;
    Counter toDisk;

    Counter writeQueueHits; // now part of the WRITEQ stats

    Counter migReads;
    Counter migWrites;
    Counter migsScheduled;
    Counter migsCompleted;
    Counter xferMaxSize;

    Counter buffer_hits, offending_writes; // 0: base swaps and retirements from buffer hits

    CoreStats core[MAXCORES];
    RowBufferStats rbs; 
    Counter rowMissBkOpen, rowMissBkClosed, rowHit;

    // for 0
    Counter aborted, ignored, hzFullIgnored;
    Counter candidateLeftRank, candidateIsCold, candidateStillHot;
    Counter victimLeftRank, victimIsCold, victimIsHot; // at the end of the swap op
    Counter randomLeftRank, randomIsCold, randomIsHot; // at the end of the swap op

    OsStats() {
      completed = nonDma = writes = reads =
      pageFaults = tlbHits = tlbMisses = fromDisk = toDisk =
      migReads = migWrites = migsScheduled = migsCompleted = xferMaxSize = buffer_hits = offending_writes = 0;

      bzero(core,MAXCORES*sizeof(CoreStats));
      rowMissBkOpen = rowMissBkClosed = rowHit = 0;
      aborted = ignored = hzFullIgnored = 0;
      candidateLeftRank = candidateIsCold = candidateStillHot = 0;
      victimLeftRank = victimIsCold = victimIsHot = 0; 
      randomLeftRank = randomIsCold = randomIsHot = 0;
      writeQueueHits = 0;
    };

    void print0SwapStats () {
      printf("candidateLeftRank %llu candidateIsCold %llu candidateStillHot %llu\n",candidateLeftRank,candidateIsCold,candidateStillHot);
      printf("victimLeftRank %llu victimIsCold %llu victimIsHot %llu\n",victimLeftRank,victimIsCold,victimIsHot);
    }

    void printCoreStats (unsigned nCores) {
      printf("%5s %9s %9s %7s %10s %12s %12s %12s %12s %12s %8s\n",
             "Core#","runtime","proc(%)","mem/IO(%)","ini","fin","p_faults","accesses","tlb_hit","tlb_miss","avg_lat(ns)");
      //printf("Core#   runtime   proc(%%) mem/IO(%%) ini fin p_faults accesses tlb_hits tlb_misses avg_latency(ns)\n");
      for(byte i=0; i<nCores; i++) 
        core[i].printStats(i);
    };
};

class MultiDimAddr { // multi-dimensional address
  public:
    byte ch, rk, bk, zn;
    //unsigned row;
    unsigned bkUID;

    void copy (MultiDimAddr *md) {
      this->ch = md->ch;
      this->rk = md->rk;
      this->bk = md->bk;
      this->zn = md->zn;
      //this->row = md->row;
      this->bkUID = md->bkUID;
    };
};

class MemRequest {
  public:
    unsigned id;
    Tick arrTime; // when it was supposed to be scheduled
    Tick schTime; // when it was actually scheduled
    Tick finTime; // when it finished
    Addr addr; // untranslated address
    MultiDimAddr mda;

    bool isRead;
    bool isDMA;
    bool started;
    bool isBlocking;
    bool addToAvgMemLatency; 

    #if LATDEBUG
    unsigned bkSizeOnArrival; // size of the bank's queue when this request arrived (should be valid only for CPU accesses)
    unsigned wbufQueueTime; // how long the access waited in the write buffer (if applicable)
    unsigned rrdTime; // accumulated row-to-row activation delays for this access
    unsigned bkBusyTime; // accumulated time the access waited for the bank to become free (e.g, due to exit PD, another access etc)
    #endif

    MemRequest(unsigned id, Tick arrTime, Addr addr, MultiDimAddr *mda, bool isRead, bool isDMA) {
      this->id = id;
      this->arrTime = arrTime;
      this->schTime = arrTime; // the initial schedule time is at least the arrival time
      this->finTime = (Tick)INVALID;
      this->addr = addr;

      if(mda != NULL)
        this->mda.copy(mda);

      this->isRead = isRead;
      this->isDMA = isDMA;
      this->started = false;
      this->isBlocking = true;

      addToAvgMemLatency = false;

      #if (0 && 0)
      shbuf_index = 0;
      #endif

      #if LATDEBUG
      bkSizeOnArrival = (unsigned)INVALID;
      wbufQueueTime = 0;
      rrdTime = 0;
      bkBusyTime = 0;
      #endif
    }

    void setMultiDimAddr (MultiDimAddr *md) {
      this->mda.copy(md);
    };

    FrameNum getFrameNum () { return (FrameNum)(addr>>pageSizeBits); };
    unsigned bkUID () { return mda.bkUID; };

    //#if TIME_DEBUG || XFER_DEBUG
    void debug_req_detailed (const char *label, MultiDimAddr *mda) {
      if(id>=XFER_ENGINE_ID) printf("####### ");
      //printf("%s: bkUID[%d] id %d is[R DMA] [%d %d] addr[%016llx]{ch%d rk%d bk%d row%u} arr %llu sch %llu fin %llu\n",
      //       label, bkUID(), id, isRead, isDMA, addr, mda->ch, mda->rk, mda->bk, mda->row, arrTime, schTime, finTime);
      printf("%s: bkUID[%d] id %d is[R DMA] [%d %d] addr[%016llx]{ch%d rk%d bk%d} arr %llu sch %llu fin %llu\n",
             label, bkUID(), id, isRead, isDMA, addr, mda->ch, mda->rk, mda->bk, arrTime, schTime, finTime);
    };

    void debug_req_detailed (const char *label) { debug_req_detailed(label, &mda); }; 
    
    void debug_req_simple (const char *label) {
      if(id>=XFER_ENGINE_ID) printf("####### ");
      printf("%s: bkUID[%d] id %d isRead %d isDMA %d addr[%016llx] arr %llu sch %llu fin %llu\n",
             label, bkUID(), id, isRead, isDMA, addr, arrTime, schTime, finTime);
    };

    void debug_req_min (const char *label, Tick now) {
      printf("%s now %llu id %d ppn %llu bkUID %u %c time(cyc) %llu\n",label, now, id, getFrameNum(), bkUID(), isRead?'R':'W',finTime-arrTime);
    };
    //#endif
};

class MemRank;
class MemPowerCounter { // memory circuitry power
  private:
    double PPD, PSB, APD, ASB; // static power
    double REF, WR, RD; // dynamic power (mem device dependent)
    double DQ, termW, termRDoth, termWRoth; // dynamic power (mem device independent)
    double mW;
    double MICRON_ACT, DETAIL_ACT;

    //#if OP_ENERGY
    //double arrayReadJoules, arrayWriteJoules;
    //double rowBufferReadJoules, rowBufferWriteJoules;
    //#endif

    Counter powerDownTransitions, powerUpTransitions;

    #if (RANKS_PCH > 1)
    static const double PdqRD = 6.6, PdqWR = 0, PdqRDoth = 15.2, PdqWRoth = 12.5;
    #else
    static const double PdqRD = 4.9, PdqWR = 9.3, PdqRDoth = 0, PdqWRoth = 0;
    #endif
    double getCircuitryJoules (double sec); // using power in mW and time in msec we have Joules

  public:
    MemPowerCounter();
    void reset();
    void addToPower(MemFeatures*, double, double, double, double, double, double, double, double, double, double, double);
    void print(double);
    void addRankStats (MemRank *crk); 
};

class RowBuffer {
  private:
    bool rowStatus[cacheBlocksPerRow]; // dirty or not
    Addr lastRowUID; // base address of the row (unique ID across the whole memory)
    byte lastRowZone; // zone of the row
    bool open, dirty; // open = row still open; dirty = any bit dirty in rowStatus

    void reset () {
      #if CLOSE_PAGE
      open = false;
      #else
      open = true;
      #endif
      dirty = false;
      bzero(rowStatus,cacheBlocksPerRow*sizeof(bool));
    };

  public:
    RowBuffer () {
      lastRowUID = INVALID_ADDR;
      lastRowZone = (byte) INVALID;
      reset();
    };

    byte precharge (bool prechargeAlways); 

    Addr getRowUID (Addr addr) { return (addr / ROW_SIZE); }; // unique across entire memory
    Addr getCBOffset (Addr addr) { return ((addr&(ROW_SIZE-1)) >> cacheBlockSizeBits); }; // return (addr%ROW_SIZE) / cacheBlockSize
    
    void touch (Addr addr, byte zone, bool isWrite) {
      open = true;
      lastRowUID = getRowUID(addr);
      lastRowZone = zone;
      if(isWrite) { // reads do not change the status
        //if(zone > 0 )
        //  printf("WRITE_TO_ROW_BUF addr %llu rowUID %llu cacheBlock(offset) %llu\n",addr, lastRowUID, getCBOffset(addr));
        dirty = true;
        rowStatus[getCBOffset(addr)] = true;
      } 
    };

    bool isHit (Addr addr) { return (getRowUID(addr) == lastRowUID); };
    bool isOpen () { return open; };
    bool isDirty () { return dirty; };
};

class MemBank {
  public:
    Tick busy_until;
    bool lastCmdIsRead;
    RowBuffer rb;
    Tick time_last_act; // timestamp of the beginning of the last array activation of this bank (in CPU cycles)
    Tick prechargeEndsAt;
    bool isPrecharging;

    MemBank() {
      busy_until = 0;
      lastCmdIsRead = true;
      time_last_act = 0;
      prechargeEndsAt = INVALID_TICK;
      isPrecharging = false;
    };
    
    bool isOpen () { return rb.isOpen(); };
    bool isDirty () { return rb.isDirty(); };
    bool isHit (Addr addr) { return rb.isHit(addr); };
};

class FawVerifier { // activation window (every rank needs one)
   private:
     typedef list <Tick> ActWindow;
     typedef ActWindow :: iterator AWit;
     Tick tFAW, tRRDact, tRRDpre; // four-activation window constraint and activation latency
     ActWindow win; // circular buffer of at most BANKS_PRK (window)

   public:
     FawVerifier(Tick tFAW, Tick tRRDact, Tick tRRDpre) { this->tFAW = tFAW; this->tRRDact=tRRDact; this->tRRDpre=tRRDpre; };

     // 'actTick' is the current time (IN CPU CYCLES) at which we attempt a row activation; 
     // 'check' returns the additional delay (IN MEMORY CYCLES), for the current activation (zero if no delay)

     Tick check (Tick actTick, bool lastCmdIsRead) {
       #if 1
       Tick now = (Tick)ceil((double)actTick/CPU_RATIO); // current activation tick in memory cycles
       Tick rrdDelay=0, fawDelay = 0; // extra delays if any (in memory cycles)

       #if 1 
       //1) remove expired entries from the head
       while(!win.empty()) {
         if((win.front()) >= (now-tFAW)) { break; }
         //printf("*** act_expired %llu < now(%llu) - tFAW(%llu)\n",win.front(),now,tFAW);
         win.pop_front(); // remove all expired activations from the front
       }

       //2) dealing with unordered activations
       if(!win.empty()) {
         if(win.back() >= now) {
           //printf("CONFLICT: tail %llu > now %llu => now ",win.back(),now);
           rrdDelay = (win.back()-now) + (lastCmdIsRead ? tRRDact : tRRDpre);
           //printf("+ RRDdelay %llu ",rrdDelay);
         }
       }

       //3) check when activation will be authorized if it violates FAW
       if(win.size() >= 4) { // 
         if((now+rrdDelay) < (win.front()+tFAW)) {
           fawDelay = (win.front()+tFAW) - (now+rrdDelay);
           //printf("+ FAWdelay %llu",fawDelay);
         }
       }

       //4) add the time of the actual activation (when it will be allowed) to the window
       win.push_back((Tick)(now + rrdDelay + fawDelay));

       if(win.size() > 4) {
         // activations in this rank should have unique timestamps, since no other activation will use
         // the 'front' as a reference, we roll the window, so it keeps the size 4
         win.pop_front();
       }

       //printf("\nACT_ATTEMPT cpu_cyc %llu [ mem_cyc %llu + rrd %llu + faw %llu = %llu ] actsInWindow %u\n", 
       //       actTick, now, rrdDelay, fawDelay, (now+rrdDelay+fawDelay), (unsigned)win.size());
       //printf("[ "); for(AWit it=win.begin(); it!=win.end(); it++) printf(" %llu ",(*it)); printf("] memCycDelay = %llu\n\n", rrdDelay+fawDelay);

       #if VALIDATE
       assert(win.size() <= 4);
       #endif
       #endif

       return (rrdDelay+fawDelay); // returning only the delay (in MEM_CYC)
       #else
       return 0;
       #endif
     };
};

class MemRank {
  private:
    unsigned currActivityCounter; // incremented when curTick == schTick; decremented when an access is retired
    unsigned prevActivityCounter; // defines the power state of the interval before curTick

  public:
    Tick time_last_access;
    Tick busy_until; // this is the rank's busy milestone
    int last_bank; // last bank accessed in this rank
    bool lastCmdIsRead;
    MemFeatures *f; // timing and memory features

    //Tick cyc_CKElopre; // PPd cycles (when the bank has not been used yet)
    double cyc_CKElopre; // PPd cycles (when the bank has not been used yet)
    Tick cyc_CKEhipre; // PSB cycles (assuming open_page)
    Tick cyc_CKEloact; // APD cycles (gaps between accesses)
    Tick cyc_CKEhiact; // ASB cycles (bank performing access)

    Tick cyc_RD; // sum_per_RD
    Tick cyc_WR; // sum_per_WR

    Counter total_access;
    // tracking the avg time between activations
    Counter total_act; // total activation commands
    Counter total_awr; // cache blocks written to the arrays in this rank

    //Tick cyc_bet_act; // sum of all cycles between the cycles where ACTIVATE commands are issued
    Tick time_first_act;// = 0;  // used for keeping track of times between activations (page misses)
    Tick time_last_act;// = 0;  // used for keeping track of times between activations (page misses)
    Tick sumAct; // sum of active cycles

    // state variables
    bool power_down; // all banks are precharged + no accesses to rank
    bool exitingPowerDown; // rank was in power mode but got accessed and started leaving POWER_DOWN (finishes at rank's busy_until)
    bool waiting; // rank is waiting busy for the next access, forbidding it from entering POWER_DOWN mode
    Tick powerDownAt; // DEBUG: instant at which this rank entered POWER_DOWN (before entry latency)

    Counter powerUpTransitions;
    Counter powerDownTransitions;

    FawVerifier *faw;

    #if (0 && 0)
    unsigned swapLeft[2]; // one for src and one for rnd
    Addr lastRowUID[2]; // rowUID of the bank that is active
    bool swapBufferBusy () { return ((swapLeft[0]+swapLeft[1]) > 0); };
    #endif

    MemRank(MemFeatures *f);
    Tick getRRD();
    Tick checkFAW (Tick activationTick, bool lastCmdIsRead) { return faw->check(activationTick, lastCmdIsRead); };
    Tick getTXP() { return (Tick)ceil(f->tXP * CPU_RATIO); };
    Tick getTPD() { return (Tick)ceil(f->tPD * CPU_RATIO); };

    // power accounting functions
    void incCurrActivityCounter () { currActivityCounter++; };
    void decCurrActivityCounter () { currActivityCounter--; };
    unsigned getCurrActivityCounter () { return currActivityCounter; };
    unsigned getPrevActivityCounter () { return prevActivityCounter; };
    void copyCurrToPrevActivityCounter () { prevActivityCounter=currActivityCounter; };

    double getDetailDD0 () {
      double wb_pct = ((double)total_awr/(double)total_act)/(double)cacheBlocksPerRow;
      double act_fraction = (double)f->tRAS/(double)f->tRC; 
      return (f->DD0*act_fraction) + (f->DD0*(1.0-act_fraction)*wb_pct);
    };

    #if (0 && 0)
    bool hitSharedBuffer (Addr rowUID) { return ((rowUID==lastRowUID[0] && swapLeft[0]>0) || (rowUID==lastRowUID[1] && swapLeft[1]>0)); }
    #endif

    //void debug (const char *label, Tick curTick, byte channel, byte rank, sbyte bank, bool isBeg) {
    //  #if PSTATE_DEBUG 
    //  if(isBeg)
    //    printf("###### BEG_%s at %llu CH %d RK %d BK %d currActivityCounter++ (%u)\n", 
    //           label, curTick, channel, rank, bank, currActivityCounter); 
    //  else
    //    printf("###### END_%s at %llu CH %d RK %d BK %d currActivityCounter-- (%u)\n", 
    //           label, curTick, channel, rank, bank, currActivityCounter);
    //  #else
    //  (void)label, (void)curTick, (void)channel, (void)rank, (void)bank, (void)isBeg;
    //  #endif
    //}
};

class MemChannel {
  private:
    bool done; // after done, finishTick (getFinishTime) is not recalculated

    Tick endLastBurst;
    byte lastRank;
    bool lastCmdIsRead;
    Tick time_last_access;
    Tick prechargeBankInRank(MemBank *cbk, MemRank *crk);

  public:
    MemBank *bk[BANKS_PCH];
    MemRank *rk[RANKS_PCH];

    MemChannel();
    void addRank(byte, MemFeatures *);
    void validate();
    void activateBank(Tick, MemRank*);
    void prechargeBank(Tick curTick, MemBank *cbk, MemRank *crk);

    void calculateFinishTick(Tick, MemRequest*); 
    void getPowerStats (Tick, MemPowerCounter *);
    Tick toCpuCycles (Tick);
};

class MemZone {
  public:
    Addr size, chan_size, rank_size, bank_size, base_addr, max_addr;
    byte type;
    unsigned rows_per_bank;
    char name[5];
    MemFeatures *feats;
    MemZone (byte t) {
      type = t;
    };
};

class AddrTranslator {
  private:
    unsigned chPages[NCHANNELS]; // number of pages in each channel
    Addr fixAddr (Addr paddr, byte zn) {
      return (zn > 0  && phantomZone > 0) ? (paddr+phantomZone) : paddr; // re-expanding the phantom zone for correct translation
    };

  public:
    //luramos: when zoneLimit or (totRanks-zoneLimit) is not perfectly divisible by chan_size
    // there is a phantom range of addresses that must be added to the address on translation.
    // phantomZone > 0 if we go from a low to a high density rank or channel
    // phantomZone < 0 if we go from a high to a low density rank or channel
    // phantomZone = 0 if all memory has the same density
    Addr phantomZone;
    byte zoneLimit, totRanks;
    MemZone *mz[MAXZONES];

    AddrTranslator () { bzero(chPages,sizeof(unsigned)*NCHANNELS); };
    
    void addChPages (byte ch, unsigned pages) { // adds this number of pages to the channel
      chPages[ch] += pages;
    };

    unsigned getChPages (byte ch) { // return channel size in number of pages
      return chPages[ch];
    };

    Addr setZone (byte z, MemFeatures *f, byte type, Addr base, Addr znSize) { // znSize in number of ranks
      mz[z] = new MemZone(type); // DRAM, STT or PCM for address translation
      mz[z]->size = f->RK_SIZE() * znSize; // size in bytes (= rank size x number of ranks)
      mz[z]->chan_size = f->RK_SIZE() * RANKS_PCH; // this would be the size of the channel if the entire memory was of this size
      mz[z]->rank_size = mz[z]->chan_size / RANKS_PCH;
      mz[z]->bank_size = mz[z]->chan_size / BANKS_PCH;
      mz[z]->base_addr = base;
      mz[z]->max_addr = mz[z]->base_addr + mz[z]->size -1;
      mz[z]->rows_per_bank = mz[z]->bank_size / ROW_SIZE;
      strcpy(mz[z]->name, f->MNAME());
      mz[z]->feats = f;
      return mz[z]->size;
    };

    void adjustPhantomZone (bool flag) {
      phantomZone = (flag ? ((zoneLimit * mz[1]->rank_size) - mz[0]->size) : 0);
      printf("* zoneLimit %d phantomZone %llu (0x%016llx)\n", zoneLimit, phantomZone, phantomZone);
    };

    Addr getZoneSize (byte z) {
      #if VALIDATE
      if(z >= MAXZONES) { printf("FATAL ERROR: zone requested is beyond maximum %u %u\n",z,MAXZONES); exit(1); };
      #endif 
      return mz[z]->size; // in bytes
    };

    FrameNum getZonePages (byte z) {
      return (FrameNum)(getZoneSize(z)/pageSize);
    };

    byte getZoneType(byte z) {
      #if VALIDATE
      if(z >= MAXZONES) { printf("FATAL ERROR: zone requested is beyond maximum %u %u\n",z,MAXZONES); exit(1); };
      #endif 
      return mz[z]->type;
    };

    const char *getZoneName(byte z) {
      #if VALIDATE
      if(z >= MAXZONES) { printf("FATAL ERROR: zone requested is beyond maximum %u %u\n",z,MAXZONES); exit(1); };
      #endif 
      return mz[z]->name;
    };

    MemFeatures *getFeaturesByZone(byte z) {
      return mz[z]->feats;
    };

    byte getZoneByAddress(Addr addr) { return (byte)(addr >= mz[0]->size); };
    byte getZoneByRank(byte rank) { return (byte)(rank >= zoneLimit); };
    byte getZoneByFrameNum(FrameNum ppn) { return (byte)(((Addr)ppn<<pageSizeBits) >= mz[0]->size); };
    
    byte translateChannel (Addr paddr) {
      byte zn = getZoneByAddress(paddr);
      return ((byte)(fixAddr(paddr,zn) / mz[zn]->chan_size));
    };

    void translate (Addr paddr, MultiDimAddr *mda) {//byte *cch, byte *crk, byte *cbk, unsigned *crow){

	  //
      // [CH | RK | ROW | BK | pageBits ]
      mda->zn = getZoneByAddress(paddr); 
      Addr temp = fixAddr(paddr, mda->zn);

	  //mbchoi
	  //printf("[mbchoi] funname:translate file:base.hh paddr:%llu mda->zn:%d \n", paddr, mda->zn);
	  //embchoi
	  //
      mda->ch = (byte)(temp / mz[mda->zn]->chan_size); // shifting left
      //assert(mda->ch == getChannel(temp,mz[mda->zn])); // OK

      temp %= mz[mda->zn]->chan_size; // stripping channel bits (most signifficant)
      mda->rk = (byte)(temp / mz[mda->zn]->rank_size); // shifting left
      temp %= mz[mda->zn]->rank_size; // stripping rank bits (currently most signifficant)
      temp /= pageSize; // stripping page bits with right shift

      // bank interleaving: strip 4KB page ID (consecutive pages map to different banks) 
      mda->bk = (byte)(temp % BANKS_PRK); // temporary value: bk is unique whithin a rank
      //mda->row = (unsigned) (temp / BANKS_PRK); // stripping bank bits with right shift
      mda->bkUID = (((mda->ch * BANKS_PCH) + (mda->rk * BANKS_PRK) +  mda->bk));
      mda->bk = (byte) (BANKS_PRK * mda->rk) + mda->bk; // final value: bk is unique whithin a channel

      //assert(mda->ch == (mda->bkUID / BANKS_PCH));
      //assert(mda->bk == (mda->bkUID % BANKS_PCH));
    };

    #if XFER_DEBUG
    unsigned getBkUID (Addr paddr) {
      MultiDimAddr mda;
      mda.zn = getZoneByAddress(paddr); 
	  //mbchoi
	  //printf("[mbchoi]: paddr: %llu mda.zn:%llu\n", paddr, mda.zn);
	  //embchoi
      Addr temp = fixAddr(paddr, mda.zn);
      mda.ch = (byte)(temp / mz[mda.zn]->chan_size); // shifting left
      temp %= mz[mda.zn]->chan_size; // stripping channel bits (most signifficant)
      mda.rk = (byte)(temp / mz[mda.zn]->rank_size); // shifting left
      temp %= mz[mda.zn]->rank_size; // stripping rank bits (currently most signifficant)
      temp /= pageSize; // stripping page bits with right shift
      mda.bk = (byte)(temp % BANKS_PRK); // temporary value: bk is unique whithin a rank
      mda.bkUID = (((mda.ch * BANKS_PCH) + (mda.rk * BANKS_PRK) +  mda.bk));
      return mda.bkUID;
    };
    #endif

    byte log2 (unsigned n) {
      byte log=0;
      while(n>1) { n>>=1; log++; }
      return log;
    };

    Addr getRowUID (Addr addr) { return (addr / ROW_SIZE); };
};
#endif
