# SPDX-License-Identifier: GPL-2.0
sunxi_di_dir := drv_div3x
sunxi_di := $(sunxi_di_dir)/di_driver.o  \
			$(sunxi_di_dir)/di_client.o \
			$(sunxi_di_dir)/di_fops.o \
#			$(sunxi_di_dir)/di_utils.o

ifeq ($(CONFIG_ARCH_SUN50IW9),y)
sunxi_di += $(sunxi_di_dir)/lowlevel_v3x/di300.o \
			$(sunxi_di_dir)/lowlevel_v3x/di300_alg.o
endif

ifeq ($(CONFIG_SUNXI_DI_AMP_SERVER),y)
sunxi_di += $(sunxi_di_dir)/rpmsg_deinterlace.o
endif

ifeq ($(CONFIG_ARCH_SUN55IW3),y)
sunxi_di += $(sunxi_di_dir)/lowlevel_v3x/di300.o \
			$(sunxi_di_dir)/lowlevel_v3x/di300_alg.o
endif
