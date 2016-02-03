import sys
import string

one_gb = 1024*1024*1024;

for line in sys.stdin.readlines():
	if not line:
		break;

	fields = line.strip().split(' ');
	phy_addr = string.atoi(fields[1]);

	if(phy_addr >= (4*one_gb) + (one_gb/2)):
		continue;
	if(phy_addr >= 4*one_gb):
		phy_addr = phy_addr - (one_gb/2);

	if (phy_addr >= 0 and phy_addr < (2*one_gb)):
		phy_addr = phy_addr + (2*one_gb);
	elif (phy_addr >= (2*one_gb) and phy_addr < (4*one_gb)):
		phy_addr = phy_addr - (2*one_gb);
	else:
		continue;

	print fields[0], phy_addr, fields[2];

