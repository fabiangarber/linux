KDIR  := /home/kserver/sources/raspberry_pi/kernel_rpi/linux/
CROSS_COMPILE := aarch64-linux-gnu-
obj-m += helloworld_c.o

all:
	make -C $(KDIR) M=$(PWD) ARCH=arm64 CROSS_COMPILE=$(CROSS_COMPILE) modules

clean:
	make -C $(KDIR) M=$(PWD) ARCH=arm64 CROSS_COMPILE=$(CROSS_COMPILE) clean
