# SPDX-License-Identifier: GPL-2.0
#
# Makefile for the Linux/SuperH SH-4 backends.
#

obj-y	:= probe.o common.o
common-y	+= $(addprefix ../sh3/, entry.o ex.o)

obj-$(CONFIG_HIBERNATION)		+= $(addprefix ../sh3/, swsusp.o)
obj-$(CONFIG_SH_FPU)			+= fpu.o softfloat.o
obj-$(CONFIG_SH_STORE_QUEUES)		+= sq.o

# Perf events
perf-$(CONFIG_CPU_SUBTYPE_SH7750)	:= perf_event.o
perf-$(CONFIG_CPU_SUBTYPE_SH7750S)	:= perf_event.o
perf-$(CONFIG_CPU_SUBTYPE_SH7091)	:= perf_event.o

# CPU subtype setup
obj-$(CONFIG_CPU_SUBTYPE_SH7750)	+= setup-sh7750.o
obj-$(CONFIG_CPU_SUBTYPE_SH7750R)	+= setup-sh7750.o
obj-$(CONFIG_CPU_SUBTYPE_SH7750S)	+= setup-sh7750.o
obj-$(CONFIG_CPU_SUBTYPE_SH7091)	+= setup-sh7750.o
obj-$(CONFIG_CPU_SUBTYPE_SH7751)	+= setup-sh7750.o
obj-$(CONFIG_CPU_SUBTYPE_SH7751R)	+= setup-sh7750.o
obj-$(CONFIG_CPU_SUBTYPE_SH7760)	+= setup-sh7760.o

# Primary on-chip clocks (common)
ifndef CONFIG_CPU_SH4A
clock-$(CONFIG_CPU_SH4)			:= clock-sh4.o
endif

obj-y					+= $(clock-y)
obj-$(CONFIG_PERF_EVENTS)		+= $(perf-y)
