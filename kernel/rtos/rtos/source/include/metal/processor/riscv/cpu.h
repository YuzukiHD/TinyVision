/*
 * Copyright (c) 2018, Pinecone Inc. and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * @file	cpu.h
 * @brief	CPU specific primitives
 */

#ifndef __METAL_RISCV_CPU__H__
#define __METAL_RISCV_CPU__H__

#include <hal_thread.h>

#define metal_cpu_yield()	kthread_msleep(10);

#endif /* __METAL_RISCV_CPU__H__ */
