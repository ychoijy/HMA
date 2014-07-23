#ifndef PAGE_TABLE_HH
#define PAGE_TABLE_HH

#include <map>
#include <bitset>
#include <vector>
#include <list>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include "msim.hh"
#include "event.hh"
#include "config.hh"

class MainMemory;

#define LINEAR  0 // channel 0 until full, then channel 1 etc
#define SHUFFLE 1 // takes a linear address and shuffles the allocation order
#define RROBIN  2 // one page ch0, one ch1, ... one chN, then back to ch0 etc

typedef list<FrameNum> AllocList;
typedef AllocList::iterator ALit;

class Allocator {
  private:
    unsigned maxFrames;
    bool *locked;
    AllocList freeList;

  public:
    Allocator(unsigned reservedFrames, unsigned maxFrames); 

    bool isMemFull () { return freeList.empty(); }; // if no more free frames, the memory is full

    bool getFreeFrame (FrameNum *ppn);

    bool allocateNew (FrameNum *ppn, bool *dirty);

    ~Allocator();// { freeList.clear(); delete this; };

    unsigned long long getOccupation (); // { return (unsigned long long)(maxFrames-freeList.size()); };
    double getPercentOccupation (); // { return (100.0*(double)getOccupation()/(double)maxFrames); };
    FrameNum size (); // { return (FrameNum)maxFrames; };

    void setLocked(FrameNum ppn, bool status);

    bool isLocked(FrameNum ppn);

    bool *getLockPerm(FrameNum ppn);

    void printStats();

   void debugLock();

   void debug();

   #if (ALLOC==SHUFFLE)
   void shuffle (FrameNum *tmp, unsigned size);
   #endif

   #if (ALLOC==RROBIN)
     void fillFreeList (FrameNum pages, FrameNum sum, FrameNum chBeg[], FrameNum chEnd[]);
   #endif
};

class PTE {
  public:
    byte pid; // process id (virtual addresses of p0 and p1 map to different frames)
    FrameNum ppn; // address = (inMemory ? physical address : offset in swap space)
    FrameNum vpn; // virtual address for the inverted page table
    bool dirty; // has the page been written to?
    list <PTE*>::iterator pos; // position in LRU list (so we know where to remove it from when it is touched)

    ~PTE() { delete this; };
};

class TLB;
typedef multimap<FrameNum, PTE*> page_table_t;
typedef pair<FrameNum,PTE*> PTPair;
typedef map<FrameNum, PTE*> invertedPT;
typedef page_table_t::iterator PTit;
typedef invertedPT::iterator IPTit;


class PageTable{
  private:
    page_table_t pt; // regular page table: maps virtual into physical addresses
    invertedPT ipt; // inverted map of the page table: maps physical into virtual and pid
    Allocator *alloc;
    TLB *tlb;
    typedef list<PTE*> LRUlist;
    typedef list<PTE*>::iterator LRUit;
    LRUlist lruList;

    void addEntry(PTE*);
    void touchEntry(PTE*);
    PTE *evictLRU();

  public:
    PageTable(MainMemory*);
    ~PageTable();
  
    void addTableEntry(FrameNum, PTE*);
    PTE getPTE(FrameNum);
    bool lookUp(byte, FrameNum, FrameNum*, bool*); // returns false if a PAGE_FAULT occurred
    void touch(FrameNum);
    void setLocked(FrameNum, bool);
    bool isLocked(FrameNum);

    void printStats ();
    void debug() { alloc->debug(); };
    void debugLock() { alloc->debugLock(); };
    bool isMemFull() { return alloc->isMemFull(); };

    void getFreeFrame(unsigned, FrameNum, FrameNum*, bool*);
    void removePhysicalAddress(FrameNum);
    bool startSwapping(); // if the memory is full, swapping will start

    bool *getLockPerm(FrameNum ppn) { return alloc->getLockPerm(ppn); };
};

class TLB_Entry{
  private:
    typedef list<TLB_Entry*>::iterator LLit;

  public:
    byte pid;
    FrameNum ppn;
    FrameNum vpn; // key
    LLit pos;
    TLB_Entry(byte pid, FrameNum ppn, FrameNum vpn) { 
      this->pid = pid; 
      this->ppn = ppn;
      this->vpn = vpn;
    };
};

class TLB {
  private:
    typedef multimap<FrameNum, TLB_Entry*> cachemap_t;
    typedef cachemap_t::iterator CMit;
    typedef cachemap_t::const_iterator CMcit;
    typedef pair<FrameNum, TLB_Entry*> CMPair;

    typedef list<TLB_Entry*> vector_t;
    vector_t LRUlist; // for LRU simulation
    cachemap_t cachemap; // for lookup
    int size;
    int max_size;

    void init(int maxSize);
    TLB_Entry *get (FrameNum,byte);
    void touch (FrameNum,byte);
    void add (FrameNum, TLB_Entry * dObj);
    void remove (FrameNum,byte);
    void evictLRU();
  
  public:
    TLB();
    ~TLB();
    FrameNum translate(FrameNum,byte) throw (bool);
    void associate(FrameNum,FrameNum,byte);
    void removeByPPN(FrameNum);
};

#endif
