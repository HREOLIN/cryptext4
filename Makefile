obj-m += cryptext4.o

KDIR := /lib/modules/$(shell uname -r)/build

all:
	make -C $(KDIR) M=$(PWD) modules

clean:
	make -C $(KDIR) M=$(PWD) clean
	rm -f mkfs.cryptext4

mkfs: mkfs.c
	gcc -o mkfs.cryptext4 mkfs.c -Wall -O2

.PHONY: all clean mkfs
