/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright(c) 2020 - 2023 Allwinner Technology Co.,Ltd. All rights reserved. */
#ifndef _DWC3_SUNXI_H
#define _DWC3_SUNXI_H

#include <linux/notifier.h>

#include "core.h"
#include "io.h"
#include "sunxi-gpio.h"
#include "sunxi-inno.h"

#define DRIVER_NAME "sunxi-plat-dwc3"
#define DRIVER_VERSION "v1.0.13 2023-07-10 12:00"
#define DRIVER_INFOMATION "DesignWare USB3 Allwinner Glue Layer Driver(" DRIVER_VERSION ")"

/* Link Registers */
#define DWC3_LLUCTL		0xd024

/* Bit fields */

/* TX TS1 COUNT Register */
#define DWC3_LLUCTL_FORCE_GEN1	BIT(10) /* Force Gen1 */

extern struct atomic_notifier_head usb_power_notifier_list;
extern struct atomic_notifier_head usb_pm_notifier_list;

#endif	/* _DWC3_SUNXI_H */
