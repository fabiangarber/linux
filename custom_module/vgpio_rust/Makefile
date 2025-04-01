DIR := /home/kserver/sources/raspberry_pi/kernel_rpi/linux/
CROSS_COMPILE := aarch64-linux-gnu-
RUSTFLAGS := -C target-feature=+neon -C target-cpu=cortex-a72

export RUSTFLAGS

rust_sample-y := src/vgpio_rust.rs
obj-m += vgpio_rust.o

# Ensure Clang is used for compilation
CC := clang
HOSTCC := clang

all:
	@echo "Building with RUSTFLAGS=$(RUSTFLAGS)"
	make -C $(DIR) M=$(PWD) ARCH=arm64 CROSS_COMPILE=$(CROSS_COMPILE) CC=$(CC) HOSTCC=$(HOSTCC) RUSTFLAGS="$(RUSTFLAGS)" modules

clean:
	make -C $(DIR) M=$(PWD) ARCH=arm64 CROSS_COMPILE=$(CROSS_COMPILE) clean

