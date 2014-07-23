#########################################################################
# File Name: mount.sh
# Author: charlie_chen
# mail: charliecqc@dcslab.snu.ac.kr
# Created Time: Tue Jan  7 23:12:56 2014
#########################################################################
#!/bin/bash
umount /home/ychoijy/image/ramdisk.img
mount -o loop /home/ychoijy/image/ramdisk.img /home/ychoijy/image/rootfs
