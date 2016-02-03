import sys
import string

f = open("./def/core0.trc")

dma_cnt = 0;
normal_cnt = 0;
pcm_cnt = 0;

while 1:
	line = f.readline()  
	if not line:
		break;
	
	fields = line.strip().split(' ');
	phy_addr = string.atoi(fields[1]);

	if(fields[2] == "R"):
		continue;

	if(len(fields[1]) > 10 and phy_addr >= 4*1024*1024*1024):
		print fields[1], " ",fields[2];
		continue;


	if(phy_addr < 1*1024*1024*1024):
		dma_cnt = dma_cnt + 1;
	elif(phy_addr < 2*1024*1024*1024):
		pcm_cnt = pcm_cnt + 1;
	elif(phy_addr < 4.5*1024*1024*1024):
		normal_cnt = normal_cnt + 1;
	else:
		print "4.5giga out : ", phy_addr;


print "=========== INFORMATION ===========";
print "DRAM_NORMAL COUNT : ", normal_cnt;
print "PCM COUNT : ", pcm_cnt + dma_cnt;
print "===================================";
f.close()
