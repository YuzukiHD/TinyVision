xradio_core-y := \
	common.o \
	fwio.o \
	txrx.o \
	main.o \
	queue.o \
	hwio.o \
	bh.o \
	wsm.o \
	sta.o \
	ap.o \
	scan.o \
	platform.o \
	vendor.o \
	fw_dbg_inf.o

xradio_core-$(CONFIG_PM)            += pm.o
xradio_core-$(CONFIG_XRADIO_SDIO)   += sdio.o
xradio_core-$(CONFIG_XRADIO_DEBUG)  += debug.o
xradio_core-$(CONFIG_XRADIO_ITP)    += itp.o
xradio_core-$(CONFIG_XRADIO_ETF)    += etf.o

ldflags-y += --strip-debug

xradio_wlan-y := wlan_init.o

obj-$(CONFIG_XRADIO) += xradio_core.o
obj-m += xradio_wlan.o

##=======  User Options =======
## Use vfs for firmware load when request_firmware
# can't work on other platform.
ccflags-y += -DUSE_VFS_FIRMWARE
## Mac addr config, disable hex for default.
#ccflags-y += -DXRADIO_MACPARAM_HEX
ccflags-y += -DMONITOR_MODE

##======= Follows just for xradio internal, Don't change these macro if not ensure. =======
ccflags-y += -DCONFIG_XRADIO_USE_EXTENSIONS
ccflags-y += -DP2P_MULTIVIF
ccflags-y += -DMCAST_FWDING
ccflags-y += -DXRADIO_SUSPEND_RESUME_FILTER_ENABLE
ccflags-y += -DAP_AGGREGATE_FW_FIX
ccflags-y += -DAP_HT_CAP_UPDATE
ccflags-y += -DAP_HT_COMPAT_FIX
#ccflags-y += -DAP_ARP_COMPAT_FIX
ccflags-y += -DENHANCE_ANTI_INTERFERE
#ccflags-y += -DUSE_RSSI_OFFSET
ccflags-y += -DSCAN_FAILED_WORKAROUND_OF_FW_EXCEPTION
ccflags-y += -DHW_RESTART
ccflags-y += -DHW_ERROR_WIFI_RESET

## Use semaphore to sync bh txrx.
#ccflags-y += -DBH_USE_SEMAPHORE
ccflags-y += -DBH_PROC_THREAD
ccflags-y += -DBH_COMINGRX_FORECAST

# Modified for power save.
#ccflags-y += -DXRADIO_USE_LONG_DTIM_PERIOD
ccflags-y += -DXRADIO_USE_LONG_KEEP_ALIVE_PERIOD

## Extra IE for probe response from upper layer is needed in P2P GO
## For offloading probe response to FW, the extra IE must be included
## in the probe response template
ccflags-y += -DPROBE_RESP_EXTRA_IE
ccflags-y += -DIPV6_FILTERING

## Modified for P2P stability.
ccflags-y += -DTES_P2P_0002_ROC_RESTART
ccflags-y += -DTES_P2P_000B_EXTEND_INACTIVITY_CNT
ccflags-y += -DTES_P2P_000B_DISABLE_EAPOL_FILTER

## for chips.
ccflags-y += -DSUPPORT_HT40
ccflags-y += -DSUPPORT_EPTA
ccflags-y += -DSUPPORT_DPLL_CHECK
#ccflags-y += -DSUPPORT_NON_HT40_CHIP
ccflags-y += -DBOOT_NOT_READY_FIX
ccflags-y += -DSUPPORT_STA_AP_COEX

#This is use for old p2p mode, say yes to add P2P0 interface
#ccflags-y += -DOLD_P2P_MODE

## for interal debug.
ccflags-y += -DSUPPORT_FW_DBG_INF
#ccflags-y += -DERROR_HANG_DRIVER

# Debug for hwx_BUG12:P2P setting crash
ccflags-y += -DDEBUG_P2P_SETTING_CRASH

#Solve __DATA__ and __TIME__ error
EXTRA_CFLAGS += -Wno-error=date-time
EXTRA_CFLAGS +=-Wno-date-time
