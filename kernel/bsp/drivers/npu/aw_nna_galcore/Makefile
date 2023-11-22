# SPDX-License-Identifier: GPL-2.0
#
# Makefile for the npu unified drivers.
#

HOST := $(shell hostname)
EXTRA_CFLAGS += -DHOST=\"$(HOST)\"

EXTRA_CFLAGS += \
	-DNO__DMA_COHERENT=1 -Werror -Wno-implicit-fallthrough -Wno-unused-variable -DLINUX -DDRIVER \
	-DDBG=0 -DUSE_PLATFORM_DRIVER=1 -DVIVANTE_PROFILER=1 -DVIVANTE_PROFILER_CONTEXT=1 \
	-DENABLE_GPU_CLOCK_BY_DRIVER=0 -DUSE_NEW_LINUX_SIGNAL=0 -DUSE_LINUX_PCIE=0 \
	-DgcdCACHE_FUNCTION_UNIMPLEMENTED=0 -DgcdENABLE_3D=1 -DgcdENABLE_2D=0 \
	-DgcdENABLE_VG=0 -DgcdFPGA_BUILD=0 -DgcdENABLE_DRM=0 -DgcdENABLE_TRUST_APPLICATION=1

EXTRA_CFLAGS += \
	-I$(srctree)/$(src)/hal/inc/\
	-I$(srctree)/$(src)/hal/inc/shared/\
	-I$(srctree)/$(src)/hal/ \
	-I$(srctree)/$(src)/hal/arch/ \
	-I$(srctree)/$(src)/os/ \
	-I$(srctree)/$(src)/os/allocator/default/ \
	-I$(srctree)/$(src)/ta/security_v1/ \
	-I$(srctree)/$(src)/os/platform/allwinner/


obj-$(CONFIG_AW_NNA_GALCORE) += galcore.o
galcore-y += \
	os/gc_hal_kernel_device.o os/gc_hal_kernel_linux.o \
	os/gc_hal_kernel_math.o os/gc_hal_kernel_os.o \
	os/gc_hal_kernel_iommu.o os/gc_hal_kernel_debug.o \
	os/gc_hal_kernel_debugfs.o os/gc_hal_kernel_allocator.o \
	os/allocator/default/gc_hal_kernel_allocator_user_memory.o \
	os/allocator/default/gc_hal_kernel_allocator_dma.o \
	os/allocator/default/gc_hal_kernel_allocator_gfp.o \
	os/allocator/default/gc_hal_kernel_allocator_reserved_mem.o \
	os/gc_hal_kernel_driver.o \
	os/platform/allwinner/gc_hal_kernel_platform_allwinner.o \
	os/allocator/default/gc_hal_kernel_allocator_dmabuf.o

galcore-y += 	hal/gc_hal_kernel.o hal/gc_hal_kernel_command.o \
				hal/gc_hal_kernel_db.o hal/gc_hal_kernel_event.o \
				hal/gc_hal_kernel_heap.o hal/gc_hal_kernel_mmu.o \
				hal/gc_hal_kernel_video_memory.o hal/gc_hal_kernel_power.o \
				hal/gc_hal_kernel_security_v1.o hal/gc_hal_kernel_preemption.o \
				hal/arch/gc_hal_kernel_context.o hal/arch/gc_hal_kernel_hardware.o \
				hal/arch/gc_hal_kernel_hardware_func.o \
				hal/arch/gc_hal_kernel_hardware_func_flop_reset.o \
				hal/arch/gc_hal_kernel_hardware_async_fe.o \
				hal/arch/gc_hal_kernel_hardware_mc_fe.o \
				hal/arch/gc_hal_kernel_hardware_waitlink_fe.o \
				hal/arch/gc_hal_kernel_recorder.o \
				os/gc_hal_kernel_security_channel_emulator.o \
				ta/security_v1/gc_hal_ta.o \
				ta/security_v1/gc_hal_ta_hardware.o \
				ta/security_v1/gc_hal_ta_mmu.o \
				ta/security_v1/os/emulator/gc_hal_ta_emulator.o
