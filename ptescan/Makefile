KERNELDIR = /lib/modules/$(shell uname -r)/build

obj-m = pte_scan.o

TARGETS = 

KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

default: ${TARGET}
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) modules

CC := /usr/bin/gcc

%.c%:
	${CC} -o $@ $^

clean:
	rm -rf *.ko
	rm -rf *.mod.*
	rm -rf .*.cmd
	rm -rf *.o
