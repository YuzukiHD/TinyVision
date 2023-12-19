# Makefile for vm tools
#
SRCTREE		:= $(CURDIR)
TOPDIR		:= $(SRCTREE)
TARGET		:= test_ce
ifeq ($(VARIANT_CHOICES), tina)
$(info "platform is" $(VARIANT_CHOICES))
TINA_DIR:=$(SRCTREE)/../../../..
CC = $(TINA_DIR)/prebuilt/gcc/linux-x86/arm/toolchain-sunxi-musl/toolchain/bin/arm-openwrt-linux-muslgnueabi-gcc-6.4.1
AR=$(TINA_DIR)/prebuilt/gcc/linux-x86/arm/toolchain-sunxi-musl/toolchain/bin/arm-openwrt-linux-muslgnueabi-gcc-ar
ROOTFS_PATH:=$(TINA_DIR)/out/${TARGET_BOARD}/compile_dir/target/rootfs/bin
else
buildconfig = ../../../../.buildconfig
ifeq ($(buildconfig), $(wildcard $(buildconfig)))
        LICHEE_TOOLCHAIN_PATH=$(shell cat $(buildconfig) | grep -w "LICHEE_TOOLCHAIN_PATH" | awk -F= '{printf $$2}')
        LICHEE_PLAT_OUT=$(shell cat $(buildconfig) | grep -w "LICHEE_PLAT_OUT" | awk -F= '{printf $$2}')
        LICHEE_CROSS_COMPILER=$(shell cat $(buildconfig) | grep -w "LICHEE_CROSS_COMPILER" | awk -F= '{printf $$2}')
		export LICHEE_BUSSINESS LICHEE_CHIP_CONFIG_DIR LICHEE_IC
endif
CC=$(LICHEE_TOOLCHAIN_PATH)/bin/$(LICHEE_CROSS_COMPILER)-gcc
ROOTFS_PATH:=$(LICHEE_PLAT_OUT)/rootfs_def/bin/
endif
all:
	$(CC) -o $(TARGET) ce_test.c
	@-cp $(TARGET) $(ROOTFS_PATH)/$(TARGET) -v
clean:
	@rm -rf *.out *.o

