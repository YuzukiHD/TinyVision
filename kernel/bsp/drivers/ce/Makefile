# SPDX-License-Identifier: GPL-2.0
#
# Makefile for the ce drivers.
#

obj-$(CONFIG_AW_CE_SOCKET) += sunxi-ce.o
obj-$(CONFIG_AW_CE_IOCTL) += sunxi-ce-ioctl.o

sunxi-ce-ioctl-$(CONFIG_AW_CE_IOCTL) += sunxi_ce_cdev.o
sunxi-ce-$(CONFIG_AW_CE_SOCKET) += sunxi_ce.o sunxi_ce_proc_comm.o

ifdef CONFIG_ARCH_SUN8IW21
	AW_CE_VER = v5
endif

sunxi-ce-$(CONFIG_AW_CE_SOCKET) +=  $(AW_CE_VER)/sunxi_ce_reg.o $(AW_CE_VER)/sunxi_ce_proc.o
sunxi-ce-ioctl-$(CONFIG_AW_CE_IOCTL) += $(AW_CE_VER)/sunxi_ce_reg.o $(AW_CE_VER)/sunxi_ce_cdev_comm.o

#ccflags-y += -I$(srctree)/drivers/crypto/sunxi-ce/$(AW_CE_VER)
#ccflags-y += -DDEBUG
