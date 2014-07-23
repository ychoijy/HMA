CPP  := g++
#CPP  := g++-4.2
#CPP  := g++-4.3

SPATH := src

IFLAGS := -I${SPATH} -I${SPATH}/dc 
CFLAGS := -Wall -W  -DLONG -DPARCODE -D_LARGEFILE64_SOURCE ${IFLAGS} ${OFLAGS} ${EXTRA_FLAGS}
OFLAGS := -O6 -fbounds-check -funroll-all-loops -ffast-math

XFLAGS := ${CFLAGS} ${OFLAGS} 

# DEBUG & SPECIAL_STAT FLAGS
XFLAGS := ${XFLAGS} -DXFER_DEBUG=0 #0 # transfer start/end
XFLAGS := ${XFLAGS} -DXFERMON=-1 #30247 #-1 #35807 # if XFER_DEBUG is enabled, monitor swaps containing this ppn (-1 = ANY swap)
XFLAGS := ${XFLAGS} -DRBSTATS=0 # counts the total number of reads and writes (row buffer stats)

XFLAGS := ${XFLAGS} -DPSTATE_DEBUG=0 # debugs power transition events
XFLAGS := ${XFLAGS} -DLATDEBUG=0 -DWP_DEBUG=0 -DMONITOR=0 -DMONITORED=133266 -DTIME_DEBUG=0 -DVALIDATE=1 #DEBUG FLAGS
XFLAGS := ${XFLAGS} -DEVTDEBUG=0 # shows memory accesses that have started
XFLAGS := ${XFLAGS} -DRETIRE_DEBUG=0 # shows memory accesses that have been retired
XFLAGS := ${XFLAGS} -DCPU_RW_DEBUG=0 # 

# SYSTEM FLAGS
XFLAGS := ${XFLAGS} -DALLOC=RROBIN # LINEAR, SHUFFLE, RROBIN
XFLAGS := ${XFLAGS} -DALLOC_OFFSET=0 # offsets the beginning of the allocation (with wrap-around)
XFLAGS := ${XFLAGS} -DPAGESTATS=1
XFLAGS := ${XFLAGS} -DCH=4
XFLAGS := ${XFLAGS} -DCLOSE_PAGE=1 
XFLAGS := ${XFLAGS} -DSWAPPING=1
XFLAGS := ${XFLAGS} -DWARMSTART=1
XFLAGS := ${XFLAGS} -DBURSTY_DMA=1
XFLAGS := ${XFLAGS} -DDRAM_MULTIPLIER=1.0 #0.5 #0.25 #1.0 #0.5 # multiplies (> 1.0) or divides (< 1.0) DRAM's size (default should be 1.0)
#mbchoi
XFLAGS := ${XFLAGS} -D_PCM_MULTIPLIER=4.0 #4.0 # default should be (4x), could be 2x or 16x
#embchoi
XFLAGS := ${XFLAGS} -DWRITEQ=0
XFLAGS := ${XFLAGS} -DENABLE_TLB=1
XFLAGS := ${XFLAGS} -DSWAP_OVERHEAD=1 # after the memory is full, add DISK_LATENCY if page fault occurs

# increase power to 1.5 when using values smaller than 1.0 or the micron model will be incoherent
XFLAGS := ${XFLAGS} -DPCM_RD_MULT=1.0 #1.25  #0.4 # multiplies the read latency of PCM by some ratio
XFLAGS := ${XFLAGS} -DPCM_WR_MULT=1.0 #2.50  #1.0 # multiplies the write latency of PCM by some ratio
XFLAGS := ${XFLAGS} -DPCMPOW_MULT=1.0 #1.5  #1.5 # multiplies the array power of PCM by some ratio

LIBS := -lm 

all:	clean_obj
	${CPP} ${XFLAGS} -c ${SPATH}/msim.cc -o ${SPATH}/msim.o
	${CPP} ${XFLAGS} -c ${SPATH}/base.cc -o ${SPATH}/base.o
	${CPP} ${XFLAGS} -c ${SPATH}/cpu_sim.cc -o ${SPATH}/cpu_sim.o
	${CPP} ${XFLAGS} -c ${SPATH}/main.cc -o ${SPATH}/main.o
	${CPP} ${XFLAGS} -c ${SPATH}/page_table.cc -o ${SPATH}/page_table.o
	${CPP} ${XFLAGS} -c ${SPATH}/sim_os.cc -o ${SPATH}/sim_os.o
	${CPP} ${OFLAGS} ${SPATH}/*.o ${LIBS} -o MINISIM

clean_obj:
	rm -f ${SPATH}/*.o 

clean:	clean_obj
	rm -f MINISIM
