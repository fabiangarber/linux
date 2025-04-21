KDIR  := /home/kserver/sources/raspberry_pi/kernel_rpi/linux/
CROSS_COMPILE := aarch64-linux-gnu-

# Add optimization flags
EXTRA_CFLAGS += -O3

obj-m += test_module.o

all:
	make -C $(KDIR) M=$(PWD) ARCH=arm64 CROSS_COMPILE=$(CROSS_COMPILE) EXTRA_CFLAGS=$(EXTRA_CFLAGS) modules

clean:
	make -C $(KDIR) M=$(PWD) ARCH=arm64 CROSS_COMPILE=$(CROSS_COMPILE) clean

