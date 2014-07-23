#########################################################################
# File Name: trans_phy_addr.sh
# Author: charlie_chen
# mail: charliecqc@dcslab.snu.ac.kr
# Created Time: Sun 05 Jan 2014 01:45:38 AM EST
#########################################################################
#!/bin/bash

#after copy physical memory trace from DramSim2 to def/ and custom/
#run this script
cat def/core0.trc | python modify.py > def/core1.trc
cat def/core1.trc > def/core0.trc
rm -rf def/core1.trc

