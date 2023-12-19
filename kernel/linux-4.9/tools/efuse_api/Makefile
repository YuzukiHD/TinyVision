# Makefile for vm tools


SRCTREE		:= $(CURDIR)
TOPDIR		:= $(SRCTREE)

include $(TOPDIR)/config.mk

CFLAGS := -I $(ARCH_DIR)
TARGET := test_efuse
ROOTFS_PATH:=$(TINA_DIR)/out/v853s-fastboot_emmc/compile_dir/target/rootfs/bin


all: main.c efuse.c
ifeq ($(notdir $(shell find ./ -name lib-efuse_mmap)), lib-efuse_mmap)
	make -C lib-efuse_mmap
else
	@echo "lib-efuse_mmap exist"
endif
	@$(CC) -o ${TARGET} main.c efuse.c libefuse_mmap.o
	mv ${TARGET} ${ROOTFS_PATH}/test_efuse

clean:
	@rm -rf *.out *.o test_efuse
	@rm -rf lib-efuse_mmap/*.out lib-efuse_mmap/*.o
