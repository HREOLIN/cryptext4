obj-m += fscryptext4.o

fscryptext4-objs := cryptext4.o file.o inode.o aops.o

KDIR := /lib/modules/$(shell uname -r)/build
PWD  := $(shell pwd)

all:
	make -C $(KDIR) M=$(PWD) modules

clean:
	make -C $(KDIR) M=$(PWD) clean
	rm -f *.ko *.mod *.mod.c *.o modules.order Module.symvers

.PHONY: all clean
