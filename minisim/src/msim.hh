/*
 * Copyright (c) 2003-2004 The Regents of The University of Michigan All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer; redistributions in binary
 * form must reproduce the above copyright notice, this list of conditions and
 * the following disclaimer in the documentation and/or other materials
 * provided with the distribution; neither the name of the copyright holders
 * nor the names of its contributors may be used to endorse or promote products
 * derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Authors: Ron Dreslinski Ali Saidi

   luramos: I'm modifying this class to represent one channel with constant
   number of ranks and banks to speed computations up. If we want multiple
   channels, then we create multiple instances of this class.
 */

#ifndef __MEM_DRAM_HH__
#define __MEM_DRAM_HH__
#include <map>
#include <list>
#include <queue>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

using namespace std;

#include "base.hh"
#include "queues.hh"
#include "page_table.hh"
#include "mman.hh"

#define NORMAL 0
#define HIGH 1
#define LOW 2

class MemManager;

class MainMemory {
  private:
    static const int memctrladdr_type = LINEAR; // first level address mapping
    Addr TOT_MEM_SIZE; // (NCHANNELS*RANKS_PCH*DRAM_RANK_SIZE) 
    MemChannel *mc[NCHANNELS];
    Tick finalTick;
    bool done; // after done, finishTick (getFinishTime) is not recalculated

    SouthBridgeController sbridge;

    void schedule (MemRequest*);
    void schedule(Tick, MemRequest*);
    void lightWeightSchedule (MemRequest*);

    ReadyQueue preReady; // keeps accesses that hit the buffer before they can actually be retired
    void retire (MemRequest *r) { 
      r->finTime += bufDelay; // buffer hit delay

      #if CPU_RW_DEBUG
        if(r->isBlocking) r->debug_req_min("RET_B",r->schTime); 
        else r->debug_req_min("RET_N",r->schTime);
      #elif RETIRE_DEBUG
        if(r->isBlocking) r->debug_req_detailed("RET_B"); 
        else r->debug_req_detailed("RET_N");
      #endif

      preReady.push(r); 
    };

    byte getRankByBank (byte bid) { return (bid/BANKS_PRK); }; // bid is unique within a channel
    byte getChannelBankWithinRank (byte bid) { return (bid%BANKS_PRK); }; // bid is unique within a channel
    void fixCKE (Tick penalty, MemRank *crk);
    bool anyBankHasAccesses (byte channel, byte rank);
    void adjustByState (Tick curTick, MemRequest *r, MemRank *);
    void exitPowerDown (Tick curTick, byte c, byte r, MemRank *crk);
    void setRankBusyUntil (byte c, byte r, Tick busyUntil);
    void debugRank (byte channel, byte rank);
    void debugState (Tick curTick, MemRequest *r, MemRank *crk, const char *label);
    bool relocate(Tick, MemRequest*);

    #if LATDEBUG
    void testBankMove (MemRequest *r);
    void testBankExec (MemRequest *r);
    #endif

  public:
    SchedQueue *sched[NBANKS];
    ReadyQueue ready;
    unsigned inFlightAccesses;

    PageTable *pt;

    MainMemory(int, int, float, Addr);
    #if WRITEQ
    void initBanks(byte);
    #endif
    Addr getMemSize(); // calculate memory size based on bank density
    Tick getFinishTime(Tick);
    void getPowerStats (MemPowerCounter *, Tick);
    bool scheduleCpuAccess(unsigned, Tick, Addr, bool, bool);
    byte translateChannel(Addr);

    void lockSB (unsigned id, bool *perm) { sbridge.lock(id,perm); };

    void scheduleDMA(byte, Addr, bool);
    void setMMU(PageTable *pt) { this->pt = pt; };

    Tick updateRankStates(Tick);

    Tick update(Tick,Tick);
    void updateCKE (Tick curTick);
    void startAccess(Tick, MemRequest*);
    void tryAdvanceDMA (Tick, MemRequest*);
    MemRequest *retireAccess(); // tries to remove one access from the ready queue or returns null
    void debug();
};

#endif
