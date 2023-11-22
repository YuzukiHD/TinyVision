/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright(c) 2020 - 2023 Allwinner Technology Co.,Ltd. All rights reserved. */
/*
 * Allwinner's log functions
 *
 * Copyright (c) 2023, lvda <lvda@allwinnertech.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#ifndef __SUNXI_LOG_H__
#define __SUNXI_LOG_H__

/* Allow user to define their own MODNAME with `SUNXI_MODNAME` */
#ifndef SUNXI_MODNAME
#define SUNXI_MODNAME		KBUILD_MODNAME
#endif

#define pr_fmt(fmt)		"sunxi:" SUNXI_MODNAME ":" fmt
#define dev_fmt			pr_fmt

#include <linux/kernel.h>
#include <linux/device.h>

/*
 * Parameter Description:
 * 1. dev: Optional parameter. If the context cannot obtain dev, fill in NULL
 * 2. fmt: Format specifier
 * 3. err_code: Error code. Only used in sunxi_err_std()
 * 4. ...: Variable arguments
 */

#if IS_ENABLED(CONFIG_AW_LOG_VERBOSE)

/* void sunxi_err(struct device *dev, char *fmt, ...); */
#define sunxi_err(dev, fmt, ...)											\
	do { if (dev)													\
		dev_err(dev, "[ERR]:%s +%d %s(): "fmt, __FILE__, __LINE__, __func__, ## __VA_ARGS__);			\
	     else													\
		pr_err("[ERR]:%s +%d %s(): "fmt, __FILE__, __LINE__, __func__, ## __VA_ARGS__);				\
	} while (0)

/* void sunxi_err_std(struct device *dev, int err_code, char *fmt, ...); */
#define sunxi_err_std(dev, err_code, fmt, ...)										\
	do { if (dev)													\
		dev_err(dev, "[ERR%d]:%s +%d %s(): "fmt, err_code, __FILE__, __LINE__, __func__, ## __VA_ARGS__);	\
	     else													\
		pr_err("[ERR%d]:%s +%d %s(): "fmt, err_code, __FILE__, __LINE__, __func__, ## __VA_ARGS__);		\
	} while (0)

/* void sunxi_warn(struct device *dev, char *fmt, ...); */
#define sunxi_warn(dev, fmt, ...)											\
	do { if (dev)													\
		dev_warn(dev, "[WARN]:%s +%d %s(): "fmt, __FILE__, __LINE__, __func__, ## __VA_ARGS__);			\
	     else													\
		pr_warn("[WARN]:%s +%d %s(): "fmt, __FILE__, __LINE__, __func__, ## __VA_ARGS__);			\
	} while (0)

/* void sunxi_info(struct device *dev, char *fmt, ...); */
#define sunxi_info(dev, fmt, ...)											\
	do { if (dev)													\
		dev_info(dev, "[INFO]:%s +%d %s(): "fmt, __FILE__, __LINE__, __func__, ## __VA_ARGS__);			\
	     else													\
		pr_info("[INFO]:%s +%d %s(): "fmt, __FILE__, __LINE__, __func__, ## __VA_ARGS__);			\
	} while (0)

/* void sunxi_debug(struct device *dev, char *fmt, ...); */
#define sunxi_debug(dev, fmt, ...)											\
	do { if (dev)													\
		dev_dbg(dev, "[DEBUG]:%s +%d %s(): "fmt, __FILE__, __LINE__, __func__, ## __VA_ARGS__);			\
	     else													\
		pr_debug("[DEBUG]:%s +%d %s(): "fmt, __FILE__, __LINE__, __func__, ## __VA_ARGS__);			\
	} while (0)

#else  /* !CONFIG_AW_LOG_VERBOSE */

/* void sunxi_err(struct device *dev, char *fmt, ...); */
#define sunxi_err(dev, fmt, ...)						\
	do { if (dev)								\
		dev_err(dev, "[ERR]: "fmt, ## __VA_ARGS__);			\
	     else								\
		pr_err("[ERR]: "fmt, ## __VA_ARGS__);				\
	} while (0)

/* void sunxi_err_std(struct device *dev, int err_code, char *fmt, ...); */
#define sunxi_err_std(dev, err_code, fmt, ...)					\
	do { if (dev)								\
		dev_err(dev, "[ERR%d]: "fmt, err_code, ## __VA_ARGS__);		\
	     else								\
		pr_err("[ERR%d]: "fmt, err_code, ## __VA_ARGS__);		\
	} while (0)

/* void sunxi_warn(struct device *dev, char *fmt, ...); */
#define sunxi_warn(dev, fmt, ...)						\
	do { if (dev)								\
		dev_warn(dev, "[WARN]: "fmt, ## __VA_ARGS__);			\
	     else								\
		pr_warn("[WARN]: "fmt, ## __VA_ARGS__);				\
	} while (0)

/* void sunxi_info(struct device *dev, char *fmt, ...); */
#define sunxi_info(dev, fmt, ...)						\
	do { if (dev)								\
		dev_info(dev, "[INFO]: "fmt, ## __VA_ARGS__);			\
	     else								\
		pr_info("[INFO]: "fmt, ## __VA_ARGS__);				\
	} while (0)

/* void sunxi_debug(struct device *dev, char *fmt, ...); */
#define sunxi_debug(dev, fmt, ...)						\
	do { if (dev)								\
		dev_dbg(dev, "[DEBUG]: "fmt, ## __VA_ARGS__);			\
	     else								\
		pr_debug("[DEBUG]: "fmt, ## __VA_ARGS__);			\
	} while (0)

#endif  /* CONFIG_AW_LOG_VERBOSE */

/* void sunxi_debug_verbose(struct device *dev, char *fmt, ...); */
#define sunxi_debug_verbose(dev, fmt, ...)									\
	do { if (dev)												\
		dev_dbg(dev, "[DEBUG]:%s +%d %s(): "fmt, __FILE__, __LINE__, __func__, ## __VA_ARGS__);		\
	     else												\
		pr_debug("[DEBUG]:%s +%d %s(): "fmt, __FILE__, __LINE__, __func__, ## __VA_ARGS__);		\
	} while (0)

/* void sunxi_debug_line(struct device *dev); */
#define sunxi_debug_line(dev)									\
	do { if (dev)												\
		dev_dbg(dev, "[DEBUG]:%s +%d %s()\n", __FILE__, __LINE__, __func__);				\
	     else												\
		pr_debug("[DEBUG]:%s +%d %s()\n", __FILE__, __LINE__, __func__);				\
	} while (0)

/*
 * TODO:
 * print_hex_dump_debug
 * print_hex_dump_bytes
 * trace_printk
 * printk_ratelimited
*/

#endif
