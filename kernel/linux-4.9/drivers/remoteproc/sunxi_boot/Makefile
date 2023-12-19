obj-$(CONFIG_SUNXI_RPROC)		+= sunxi_remoteproc_boot.o
sunxi_remoteproc_boot-y			+= sunxi_rproc_boot.o

ifeq ($(CONFIG_SUNXI_REMOTEPROC_HIFI4_BOOT), y)
sunxi_remoteproc_boot-y			+= hifi4/dsp.o
sunxi_remoteproc_boot-y			+= hifi4/standby.o
endif
ifeq ($(CONFIG_SUNXI_REMOTEPROC_E907_BOOT), y)
sunxi_remoteproc_boot-y		+= e907/e907.o
endif

obj-$(CONFIG_SUNXI_RPROC)		+= sunxi_remoteproc_standby.o
sunxi_remoteproc_standby-y		+= sunxi_rproc_standby.o
ifeq ($(CONFIG_SUNXI_REMOTEPROC_HIFI4_BOOT), y)
sunxi_remoteproc_standby-y		+= hifi4/standby.o
endif
