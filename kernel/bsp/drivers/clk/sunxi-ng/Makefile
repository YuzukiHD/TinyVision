# SPDX-License-Identifier: GPL-2.0

# Common objects
ccu-sunxi-ng-y					+= ccu_common.o
ccu-sunxi-ng-y					+= ccu_reset.o

# trace objects
CFLAGS_ccu-sunxi-trace.o			:= -I$(src)
ccu-sunxi-ng-y					+= ccu-sunxi-trace.o

# Base clock types
ccu-sunxi-ng-y					+= ccu_div.o
ccu-sunxi-ng-y					+= ccu_frac.o
ccu-sunxi-ng-y					+= ccu_gate.o
ccu-sunxi-ng-y					+= ccu_mux.o
ccu-sunxi-ng-y					+= ccu_mult.o
ccu-sunxi-ng-y					+= ccu_phase.o
ccu-sunxi-ng-y					+= ccu_sdm.o

# Multi-factor clocks
ccu-sunxi-ng-y					+= ccu_nk.o
ccu-sunxi-ng-y					+= ccu_nkm.o
ccu-sunxi-ng-y					+= ccu_nkmp.o
ccu-sunxi-ng-y					+= ccu_nm.o
ccu-sunxi-ng-y					+= ccu_mp.o

obj-$(CONFIG_AW_CCU)				+= ccu-sunxi-ng.o
obj-$(CONFIG_AW_RTC_CCU)			+= ccu-sunxi-rtc.o
obj-$(CONFIG_AW_DDR_CCU)			+= ccu-ddr.o

# SoC support
obj-$(CONFIG_AW_SUN8IW20_CCU)		+= ccu-sun8iw20.o
obj-$(CONFIG_AW_SUN8IW20_CCU)		+= ccu-sun8iw20-rtc.o
obj-$(CONFIG_AW_SUN8IW20_R_CCU)		+= ccu-sun8iw20-r.o
obj-$(CONFIG_AW_SUN8IW20_TEST)		+= ccu-sun8iw20-test.o

obj-$(CONFIG_AW_SUN8IW21_CCU)		+= ccu-sun8iw21.o
obj-$(CONFIG_AW_SUN8IW21_CCU)		+= ccu-sun8iw21-rtc.o
obj-$(CONFIG_AW_SUN8IW21_R_CCU)		+= ccu-sun8iw21-r.o
