SRCPATH := $(shell dirname $(lastword $(MAKEFILE_LIST)))
ccflags-y += -I$(SRCPATH)/../aic8800_bsp
obj-$(CONFIG_AIC8800_BTLPM_SUPPORT) := aic8800_btlpm.o

CONFIG_AUTO_PM ?= y

ccflags-$(CONFIG_AUTO_PM) += -DCONFIG_AUTO_PM
