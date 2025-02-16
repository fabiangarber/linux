KDIR := /home/kserver/sources/raspberry_pi/kernel_rpi/linux/
CROSS_COMPILE := aarch64-linux-gnu-

rust_sample-y := vgpio_rust.rs
obj-m += vgpio_rust.o

all:
	make -C $(KDIR) M=$(PWD) ARCH=arm64 CROSS_COMPILE=$(CROSS_COMPILE) modules

clean:
	make -C $(KDIR) M=$(PWD) ARCH=arm64 CROSS_COMPILE=$(CROSS_COMPILE) clean
