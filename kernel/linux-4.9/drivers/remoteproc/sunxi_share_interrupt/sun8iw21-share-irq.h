/* SPDX-License-Identifier: GPL-2.0 */
/*
 * sunxi's Remote Processor Share Interrupt Head File
 *
 * Copyright (C) 2022 Allwinnertech - All Rights Reserved
 *
 * Author: lijiajian <lijiajian@allwinnertech.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/*
 * ARM GIC first 32 interrupt numbers are in core, we need
 * to subtract 32.
 * RISCV CLIC first 16 interrupt numbers are in core, we need
 * to subtract 16.
 */
static struct share_irq_res_table share_irq_table[] = {
		/* major  host coreq hwirq  remore core irq */
		{1,       99  - 32,         99 - 16}, /* GPIOA */
		{2,       103 - 32,         103 - 16}, /* GPIOC */
		{3,       105 - 32,         105 - 16}, /* GPIOD */
		{4,       107 - 32,         107 - 16}, /* GPIOE */
		{5,       109 - 32,         109 - 16}, /* GPIOF */
		{6,       111 - 32,         111 - 16}, /* GPIOG */
		{7,       113 - 32,         113 - 16}, /* GPIOH */
		{8,       115 - 32,         115 - 16}, /* GPIOI */
};
