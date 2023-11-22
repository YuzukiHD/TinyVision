# SPDX-License-Identifier: GPL-2.0
#
# Makefile for the kernel mixer device drivers.
#

include ${obj}/g2d_rcq/Makefile

obj-$(CONFIG_AW_G2D) += g2d_sunxi.o

g2d_sunxi-$(CONFIG_G2D_RCQ) += ${rcq_obj}

g2d_sunxi-$(CONFIG_G2D_SYNCFENCE) += syncfence.o
