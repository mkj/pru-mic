obj-m := bulk_samp.o

KSRC = /lib/modules/$(shell uname -r)/build

all: bulk_samp.ko

bulk_samp.ko: bulk_samp.c ../bulk_samp_common.h
	@make -C $(KSRC) M=$(PWD) ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- modules

clean:
	@make -C $(KSRC) M=$(PWD) ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- clean

load: 
	rmmod bulk_samp
	insmod bulk_samp.ko
	
.PHONY: all clean load
