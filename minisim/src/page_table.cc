#include <time.h>
#include <assert.h>
#include "msim.hh"
#include "config.hh"
#include "mman.hh"
#include "page_table.hh"

extern OsStats *stats;
extern AddrTranslator atrans;
#if PAGESTATS
extern MemoryArrayStats arrayStats;
#endif

/******************************************************************************
 Allocator
 ******************************************************************************/
#if (ALLOC==SHUFFLE)
void Allocator :: shuffle (FrameNum *arr, unsigned size) {
  FrameNum tmp;
  unsigned n, i;

  for(n=size; n>0; n--) {
    i = (((((unsigned)rand())<<16) | ((unsigned)rand())) % n); // note that n reduces to zero
    tmp = arr[i];
    arr[i] = arr[n-1];
    arr[n-1] = tmp;
  }
}
#endif

#if (ALLOC==RROBIN)
void Allocator :: fillFreeList (FrameNum pg, FrameNum sum, FrameNum chBeg[], FrameNum chEnd[]) {
  byte c;
  FrameNum i = 0;
  FrameNum pages = pg;
  //printf("rese %u sum %llu maxF %u\n",reservedFrames, sum, maxFrames); //exit(0);
  while(pages < sum) {
    c = (byte)(i%(FrameNum)NCHANNELS);
    if(chBeg[c] <= chEnd[c]) {
      freeList.push_back(chBeg[c]);
      //printf("RROBIN chan %d alloc %llu (pages %llu/%llu)\n",c,chBeg[c], pages, sum);
      chBeg[c]++;
      pages++;
    }
    i++;
  }
}
#endif

Allocator :: Allocator(unsigned reservedFrames, unsigned maxFrames) {
  #if (ALLOC==SHUFFLE)
  FrameNum *buf, *p;
  #endif

  this->maxFrames = maxFrames;
  if( (locked=(bool*)calloc(maxFrames,sizeof(bool))) == NULL) {
    printf("FATAL: no memory for locked bitmap\n"); exit(1);
  } 
  
  #if (ALLOC==LINEAR)
    printf("LINEAR ALLOC\n");
    for(FrameNum ppn=reservedFrames; ppn<maxFrames; ppn++) freeList.push_back(ppn);

  #elif (ALLOC==SHUFFLE)
    printf("SHUFFLE ALLOC\n");
    srand(666);
    unsigned size = maxFrames-reservedFrames; // eg: 10-0 = 10 [0 - 9]; 10-5 = 5 [5 - 9]
    if((buf = (FrameNum*)malloc(sizeof(FrameNum)*size)) == NULL) {
      printf("FATAL: not enough memory to randomize\n");
      exit(1);
    }

    p = buf;
    for(FrameNum ppn=reservedFrames; ppn<maxFrames; ppn++) { *p = ppn; /*printf("%llu,",*p);*/  p++; }
    //printf("chk:%llu\n",buf[size-1]);
    shuffle(buf, size);

    for(p=buf; p != (buf+size); p++) { freeList.push_back(*p); /*printf("%llu,",*p);*/ }
    free(buf);
  #elif (ALLOC==RROBIN)
    FrameNum pages, sum=0;
    FrameNum chBeg[NCHANNELS];
    FrameNum chEnd[NCHANNELS];
    byte c;

    printf("ROUND-ROBIN ALLOC\n");
    for(c=0; c<NCHANNELS; c++) {
      pages = atrans.getChPages(c);
      chBeg[c] = sum;
      sum += pages;
      chEnd[c] = sum-1;
      printf("CH[%d].pages = %8llu [%8llu to %8llu]\n", c, pages, chBeg[c], chEnd[c]);
    }

    #if ALLOC_OFFSET
      FrameNum offset = (FrameNum)atrans.getZonePages(0)/2; // whatever zone 0 is HOT/CACHE ZONE we start in the middle of that zone

      if(offset == 0 || offset >= (FrameNum)atrans.getZonePages(0)) {
        fillFreeList (0, sum, chBeg, chEnd);
        printf("ALLOC_OFFSET %llu (INVALID RANGE, RESETTING TO ZERO)\n",offset);
      } else {
        printf("ALLOC_OFFSET %llu\n",offset);
        FrameNum tmpBeg[NCHANNELS];
        FrameNum tmpEnd[NCHANNELS];

        pages = 0;
        //printf("----------\n");
        for(c=0; c<NCHANNELS; c++) {
          tmpBeg[c] = tmpEnd[c] = ((chBeg[c]+offset) >= chEnd[c]) ? (chEnd[c]+1) : (chBeg[c]+offset);

          if(chBeg[c] <= chEnd[c]) {
            if(tmpBeg[c] > chBeg[c] && tmpBeg[c] < chEnd[c])
              pages += (tmpBeg[c]-chBeg[c]);
            else
              pages += (chEnd[c]-chBeg[c]) + 1;
          } else {
            tmpBeg[c] = chBeg[c];
            tmpEnd[c] = chEnd[c];
          }

          //printf("pages = %llu sum = %llu\n",pages,sum);
        }

        fillFreeList(pages, sum, tmpBeg, chEnd); //exit(0);

        if(pages < sum) {
          #if 0
          printf("----------\n");
          for(c=0; c<NCHANNELS; c++) 
            printf("CH[%d].pages = %8llu [%8llu to %8llu]\n", c, tmpEnd[c]<chBeg[c] ? 0 : tmpEnd[c]-chBeg[c], chBeg[c], tmpEnd[c]);
          #endif

          fillFreeList(0, pages, chBeg, tmpEnd); //exit(0);
        }
      }
    #else
      fillFreeList (0, sum, chBeg, chEnd);
    #endif

    assert(freeList.size() == maxFrames);
  #endif

  (void)reservedFrames;
}

bool Allocator :: getFreeFrame (FrameNum *ppn) { 
  ALit it;
  for(it=freeList.begin(); it!=freeList.end(); it++) {
    if(!locked[(*it)])
    {
      *ppn = (*it);
	  //mbchoi
	  //printf("[mbchoi] funname:getFreeFrame file:page_table.cc ppn:%llu\n", *ppn);
	  //embchoi
      freeList.erase(it);
      return true;
    }
  }
  return false;
}

bool Allocator :: allocateNew (FrameNum *ppn, bool *dirty) {
  if(!freeList.empty()) { // memory is fully allocated
    if(getFreeFrame(ppn)) {
      *dirty = false; // since it wasn't written dirty=false (it will be fetched from memory)
      //printf("ALLOC %llu (remaining %u/%u\n",*ppn,(unsigned)freeList.size(),maxFrames); //exit(0);
      return true;
    }
  }
  return false;
}

Allocator :: ~Allocator() { freeList.clear(); delete this; }
inline unsigned long long Allocator :: getOccupation () { return (unsigned long long)(maxFrames-freeList.size()); }
inline double Allocator :: getPercentOccupation () { return (100.0*(double)getOccupation()/(double)maxFrames); }
inline FrameNum Allocator :: size () { return (FrameNum)maxFrames; }

void Allocator :: setLocked(FrameNum ppn, bool status) {
  //#if VALIDATE
  //assert(ppn < maxFrames);
  //#endif
  //if(status) printf("LOCKING ppn %llu\n",ppn); else printf("UNLOCKING ppn %llu\n",ppn);
  locked[ppn] = status;
}

bool Allocator :: isLocked(FrameNum ppn) {
  //#if VALIDATE
  //assert(ppn < maxFrames);
  //#endif
  return (locked[ppn]);
}

bool *Allocator :: getLockPerm(FrameNum ppn) {
  //#if VALIDATE
  //assert(ppn < maxFrames);
  //#endif
  return &locked[ppn];
}

void Allocator :: printStats() {
  printf("PT.Pages allocated: %llu\n", getOccupation());
  printf("PT.Memory footprint (MB): %lf\n",(double)(getOccupation() * pageSize)/(1024.0*1024.0));
  printf("PT.Memory occupation: %0.2lf percent\n",getPercentOccupation());
}

void Allocator :: debugLock() {
  for(FrameNum i=0; i<maxFrames; i++) {
    if(locked[i])
      printf("ppn %llu locked %d\n",i,locked[i]);
  }
}

void Allocator :: debug() {
  unsigned i, count;
  count = 0; for(i=0; i<maxFrames; i++) if(locked[i]) count++; printf("\nPT.locked_pages %u",count);
  printf("\n");
}

/******************************************************************************
 Page Table 
 ******************************************************************************/
PageTable::PageTable(MainMemory *mem) {
  (void)mem;
  unsigned maxFrames = mem->getMemSize() >> pageSizeBits; 
  unsigned reservedFrames = 0;
  alloc = new Allocator(reservedFrames, maxFrames);
}

PageTable::~PageTable() {
  pt.clear();
  ipt.clear();
  lruList.clear();
}

void PageTable :: printStats () {
  alloc->printStats();
}

/* lookUp:
 *
 * (a) returns the physical address of a page corresponding to the virtual
 * address passed by value if it can be found in memory at that point (PAGE HIT). 
 */
void PageTable::touch(FrameNum ppn) {
  touchEntry(ipt[ppn]); // lookup entry before touching it
}

inline void PageTable::touchEntry (PTE *e) {
  lruList.erase(e->pos); // remove it from its current position in the LRU list
  addEntry(e);
}

inline void PageTable :: addEntry (PTE *e) {
  e->pos = lruList.insert(lruList.end(), e); // put it at the end and update its position
}

void PageTable::setLocked(FrameNum ppn, bool locked) { alloc->setLocked(ppn,locked); }
bool PageTable::isLocked(FrameNum ppn) { return alloc->isLocked(ppn); }

// true = PAGE HIT; false = PAGE FAULT
bool PageTable::lookUp(byte pid, FrameNum vpn, FrameNum *ppn, bool *dirty) {
  PTE *e;
  PTit it; // 'it' is a map <first,second>

  if((it=pt.find(vpn)) != pt.end()){
    while(it->first == vpn){ // first = VIRTUAL ADDRESS
      e = it->second; // second = page table entry (PTE)
      if(e->pid == pid){ // PAGE HIT
        *ppn = e->ppn;
        if(*dirty) e->dirty = true; // cannot make: entry->dirty = *dirty, due to possible future overwrites
        touchEntry(e); 
        return true; // page was found in memory
      } else {
        if(++it == pt.end()) break; // stop searching, the entry was not found: return false
      }
    }
  }

  return false;
}

/* getFreeFrame
 * (b) allocates a new memory page if the memory is not full and creates an
 * entry in the page table (and reverse page table) corresponding to the
 * association between virtual and physical address (COLD PAGE FAULT).
 *
 * (c) if the memory is full, it replaces the virtual address of the LRU page
 * with the new virtual address, and flags a PAGE FAULT. It also returns the
 * dirty status of the freed physical page by reference.
 *
 * dirty is simultaneously an input parameter (if set, means that the page
 * being looked up is dirty) and an output parameter returning the status of
 * the page being looked up. This is important when we swap pages to from the
 * disk. I a page to be swapped is not dirty it does not have to be
 * written-back, only replaced.
 **/
void PageTable::getFreeFrame(unsigned pid, FrameNum vpn, FrameNum *ppn, bool *dirty){
  PTE *e;
  // PAGE_FAULT, and allocating new memory
  e = new PTE();
  e->dirty = false;
  e->pid = pid;
  e->vpn = vpn;

  if(alloc->allocateNew(ppn,dirty) == false) { // if the memory is full, evict LRU entry and reuse its frame (ppn)
    PTE *x = evictLRU(); // a new virtual address will replace the old one in this (physical) frame address

    //printf("ACTION REPLACED: ppn %llu (vpn %llu, pid %d)\n",x->ppn, x->vpn,x->pid);
    //if(x->ppn == 8 || x->ppn == 9) exit(0);

    *ppn = x->ppn;
    *dirty = x->dirty;
    #if PAGESTATS
    arrayStats.countWasRep(*ppn);
    #endif
    PTit lru = pt.find(x->vpn);
    while(lru != pt.end() && lru->second->pid != x->pid) lru++;
    pt.erase(lru);
    ipt.erase(*ppn); // remove by physical addr

    //printf(" in getFreeFrame "); mqassert(*ppn);
  } 
  //else printf("ACTION ALLOC ppn %llu (vpn %llu, pid %d)\n",*ppn,vpn,pid);

  e->ppn = *ppn;
  pt.insert (pair<Addr,PTE*>( vpn,e)); // vpn|pid -> entry
  ipt.insert(pair<Addr,PTE*>(*ppn,e)); // ppn -> entry
  addEntry(e); 

// DEBUG
//  it = pt.insert (pair<Addr,PTE*>( vaddr,entry)); // vaddr|pid -> entry
//  if(ipt[*paddr] == it->second) printf(":-)\n"); else printf(":-(\n"); exit(0);
  assert(pt.size() == ipt.size()); // DEBUG
}

PTE *PageTable :: evictLRU () {
  LRUit lru = lruList.begin(); // first non-locked LRU element

  while(lru != lruList.end() && isLocked((*lru)->ppn)==true) lru++;
  assert(lru != lruList.end()); // DEBUG
  assert(isLocked((*lru)->ppn) == false); // DEBUG

  PTE *e = (*lru);
  lruList.erase(lru); 

  //printf(" in evictLRU "); mqassert(e->ppn);

  return e;
}

/******************************************************************************
 TLB
 ******************************************************************************/

TLB::TLB() {
  size=0;
  max_size=tlbSize;
  //curr=LRUlist.rend(); // curr = ?
}

TLB::~TLB() { 
  cachemap.clear();
  LRUlist.clear();
}

FrameNum TLB::translate(FrameNum vpn, byte pid) throw (bool) {
  TLB_Entry* entry = get(vpn,pid);
  if(entry == NULL) { // TLB MISS
    throw false;
  } else { // TLB HIT
    return entry->ppn;
  }
}

TLB_Entry * TLB::get (FrameNum vpn, byte pid) {
  cachemap_t::const_iterator it;
  it = cachemap.find(vpn);
  if(it!=cachemap.end()){
    while(it->first == vpn){
      if(it->second->pid == pid)
        return it->second;
      it++;
    }
  }
  return NULL;
}

void TLB::associate(FrameNum vpn, FrameNum ppn, byte pid) {
  TLB_Entry *entry = get(vpn,pid);
  if(entry != NULL){
    entry->ppn = ppn;
    touch(vpn,pid); // for LRU bookeping (necessary in case of eviction)
  }else{
    if (size >= max_size) evictLRU();
    vector_t::iterator it = LRUlist.insert(LRUlist.end(),new TLB_Entry(pid,ppn,vpn));
    (*it)->pos = it;
    cachemap.insert(CMPair(vpn,*it));
    size++;
  }
}

void TLB::touch (FrameNum vpn, byte pid){
  CMcit it;
  TLB_Entry *nObj;

  if((it = cachemap.find(vpn))!=cachemap.end()){
    while(it->first == vpn){
      nObj = it->second;
      if(nObj->pid == pid){
        LRUlist.erase(nObj->pos);
        nObj->pos = LRUlist.insert(LRUlist.end(),nObj);
      }
      it++;
    }
  }
}

void TLB::evictLRU (){
  //printf("EVICT\n");
  vector_t::iterator lru = LRUlist.begin(); 
  TLB_Entry *victim = *lru;
  cachemap_t::iterator it;

  if((it = cachemap.find(victim->vpn)) != cachemap.end()){
    while(it->first == victim->vpn){
      if(it->second->pid == victim->pid) {
        //printf("Evicting: key %llu pid %llu\n",victim->vpn,victim->pid);

        delete it->second;
        cachemap.erase(it);
        //delete victim;
        LRUlist.erase(lru);
        size--;
        return;
      }
      it++;
    }
  }
}
