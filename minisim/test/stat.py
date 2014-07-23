import sys
import string

f = open("./def/core0.trc")

dram = 0;
pcm = 0;
dma_cnt = 0;
normal_cnt = 0;
pcm_cnt = 0;
out_cnt = 0;

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

	if(phy_addr < 2*1024*1024*1024):
		dram = dram+1;
	else:
		pcm = pcm + 1;

	if(phy_addr < 1*1024*1024*1024):
		dma_cnt = dma_cnt + 1;
	elif(phy_addr < 2*1024*1024*1024):
		normal_cnt = normal_cnt + 1;
	elif(phy_addr < 4*1024*1024*1024):
		pcm_cnt = pcm_cnt + 1;
	else:
		out_cnt = out_cnt + 1;
  	#if(phy_addr < 4 * 1024*1024*1024 and phy_addr >= 3.5 * 1024*1024*1024):
  	#  print phy_addr;
	if(phy_addr > 4.5 * 1024*1024*1024):
		print "4.5giga out : ", phy_addr;


print "dram write:", dram;
print "pcm write:", pcm;
print "\n";
print "dma_cnt:", dma_cnt;
print "normal_cnt:", normal_cnt;
print "pcm_cnt:", pcm_cnt;
print "out_cnt", out_cnt;
f.close()
