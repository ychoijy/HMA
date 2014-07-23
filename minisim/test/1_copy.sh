#########################################################################
# File Name: copy.sh
# Author: charlie_chen
# mail: charliecqc@dcslab.snu.ac.kr
# Created Time: Thu 13 Feb 2014 03:24:53 AM EST
#########################################################################
#!/bin/bash

cat ./../../marss.dramsim/sim_out_my_dram.ini.tmp | grep ychoijy | awk '{print $2 " " $3 " " $4}' > ./def/core0.trc;
python stat.py;
#cat /home/ychoijy/marss.dramsim/analyze/input_def > ./def/core0.trc
#cat /home/mbchoi/tool/marss.dramsim/analyze/input_def > ./def/core0.trc
#cat /home/mbchoi/tool/marss.dramsim/analyze/input_custom > ./custom/core0.trc
