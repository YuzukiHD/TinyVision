/*
 * A V4L2 driver for Raw cameras.
 *
 * Copyright (c) 2022 by Allwinnertech Co., Ltd.  http://www.allwinnertech.com
 *
 * Authors:  Liu chensheng<liuchensheng@allwinnertech.com>
 *    Liang WeiJie <liangweijie@allwinnertech.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/videodev2.h>
#include <linux/clk.h>
#include <media/v4l2-device.h>
#include <media/v4l2-mediabus.h>
#include <linux/io.h>

#include "camera.h"
#include "sensor_helper.h"

MODULE_AUTHOR("lcs");
MODULE_DESCRIPTION("A low-level driver for k302p sensors");
MODULE_LICENSE("GPL");

#define LANE_CHOICE 1
#define WDR_MODE 1

#if LANE_CHOICE
#define K302P_2LANE 1
#define K302P_4LANE 0
#define K302P_4LANE_WDR 0
#define MCLK              (24*1000*1000)
#else
#if WDR_MODE
#define K302P_2LANE 0
#define K302P_4LANE 0
#define K302P_4LANE_WDR 1
#define MCLK              (27*1000*1000)
#else
#define K302P_2LANE 0
#define K302P_4LANE 1
#define K302P_4LANE_WDR 0
#define MCLK              (24*1000*1000)
#endif
#endif

#define V4L2_IDENT_SENSOR  0x0845

/*
 * Our nominal (default) frame rate.
 */
#define ID_REG_HIGH		0x0A
#define ID_REG_LOW		0x0B
#define ID_VAL_HIGH		0x08
#define ID_VAL_LOW		0x45
#define SENSOR_FRAME_RATE 30

#define HDR_RATIO 16

/*
 * The f37p i2c address
 */
#define I2C_ADDR 0x80

#define SENSOR_NAME "k302p_mipi"

#define		_SOI_MIRROR_FLIP
/* #define	_SOI_BSUN_OPT */
/* #define	_SOI_PRECHG_OPT */

#ifdef _SOI_BSUN_OPT
static unsigned char	BSunMode = 0xFF;					/* Uninitialized */
#endif
#ifdef _SOI_PRECHG_OPT
static unsigned char	PreChgMode = 0xFF;					/* Uninitialized */
#endif

data_type		sensor_flip_status;

static unsigned short settleTime = 0xa0;
module_param(settleTime, ushort, 0644);
/*
 * The default register settings
 */

static struct regval_list sensor_default_regs[] = {

};

#if K302P_2LANE
static struct regval_list sensor_2866x1520p30_2lane_regs[] = {
	{0x12, 0x40},
	{0x48, 0x8A},
	{0x48, 0x0A},
	{0x0E, 0x11},
	{0x0F, 0x04},
	{0x10, 0x3C},
	{0x11, 0x80},
	{0x0D, 0x50},
	{0x57, 0x60},
	{0x58, 0x18},
	{0x5F, 0x41},
	{0x60, 0x24},
	{0x20, 0xEE},
	{0x21, 0x02},
	{0x22, 0x40},
	{0x23, 0x06},
	{0x24, 0xA0},
	{0x25, 0xF0},
	{0x26, 0x52},
	{0x27, 0xEA},
	{0x28, 0x21},
	{0x29, 0x02},
	{0x2A, 0xDE},
	{0x2B, 0x12},
	{0x2C, 0x00},
	{0x2D, 0x00},
	{0x2E, 0x85},
	{0x2F, 0x44},
	{0x41, 0xA2},
	{0x42, 0x22},
	{0x46, 0x18},
	{0x47, 0x72},
	{0x80, 0x02},
	{0xAF, 0x22},
	{0xBD, 0x80},
	{0xBE, 0x0A},
	{0x1D, 0x00},
	{0x1E, 0x04},
	{0x6C, 0x40},
	{0x70, 0xD5},
	{0x71, 0x9B},
	{0x72, 0x6D},
	{0x73, 0x49},
	{0x75, 0x96},
	{0x74, 0x12},
	{0x89, 0x0F},
	{0x0C, 0x80},
	{0x6B, 0x20},
	{0x86, 0x00},
	{0x9E, 0x80},
	{0x78, 0x44},
	{0x30, 0x88},
	{0x31, 0x08},
	{0x32, 0x30},
	{0x33, 0x54},
	{0x34, 0x30},
	{0x35, 0x40},
	{0x3A, 0xA0},
	{0x45, 0x87},
	{0x56, 0x00},
	{0x59, 0x40},
	{0x61, 0x10},
	{0x85, 0x28},
	{0x8A, 0x04},
	{0x91, 0x22},
	{0x9B, 0x83},
	{0x5B, 0xB2},
	{0x5C, 0xF6},
	{0x5D, 0xE7},
	{0x5E, 0xB0},
	{0x64, 0xC0},
	{0x65, 0x32},
	{0x66, 0x00},
	{0x67, 0x46},
	{0x68, 0x20},
	{0x69, 0xF4},
	{0x6A, 0x28},
	{0x7A, 0x80},
	{0x82, 0x30},
	{0x8F, 0x90},
	{0x9D, 0x71},
	{0xBF, 0x01},
	{0x57, 0x22},
	{0x5E, 0x21},
	{0xBF, 0x00},
	{0x97, 0xFA},
	{0x13, 0x01},
	{0x96, 0x04},
	{0x4A, 0x01},
	{0x7E, 0xCC},
	{0x50, 0x02},
	{0x49, 0x10},
	{0x7F, 0x57},
	{0x8E, 0x00},
	{0xA7, 0x80},
	{0xBF, 0x01},
	{0x4E, 0x11},
	{0x50, 0x00},
	{0x51, 0x0F},
	{0x55, 0x20},
	{0x58, 0x00},
	{0xBF, 0x00},
	{0x19, 0x20},
	{0x79, 0x00},
	{0x12, 0x00},

	{0x02, 0x16},
	{0x01, 0x62},
	{0x00, 0x10},
};

static struct regval_list sensor_2866x1520p15_2lane_regs[] = {
	{0x12, 0x40},
	{0x48, 0x8A},
	{0x48, 0x0A},
	{0x0E, 0x11},
	{0x0F, 0x04},
	{0x10, 0x3C},
	{0x11, 0x80},
	{0x0D, 0x50},
	{0x57, 0x60},
	{0x58, 0x18},
	{0x5F, 0x41},
	{0x60, 0x24},
	{0x20, 0xEE},
	{0x21, 0x02},
	{0x22, 0x80},
	{0x23, 0x0c},
	{0x24, 0xA0},
	{0x25, 0xF0},
	{0x26, 0x52},
	{0x27, 0xEA},
	{0x28, 0x21},
	{0x29, 0x02},
	{0x2A, 0xDE},
	{0x2B, 0x12},
	{0x2C, 0x00},
	{0x2D, 0x00},
	{0x2E, 0x85},
	{0x2F, 0x44},
	{0x41, 0xA2},
	{0x42, 0x22},
	{0x46, 0x18},
	{0x47, 0x72},
	{0x80, 0x82}, //0x02 0x82
	{0xAF, 0x22},
	{0xBD, 0x80},
	{0xBE, 0x0A},
	{0x1D, 0x00},
	{0x1E, 0x08}, //0x04 0x08
	{0x6C, 0x40},
	{0x70, 0xD5},
	{0x71, 0x9B},
	{0x72, 0x6D},
	{0x73, 0x49},
	{0x75, 0x96},
	{0x74, 0x12},
	{0x89, 0x0F},
	{0x0C, 0x80},
	{0x6B, 0x20},
	{0x86, 0x00},
	{0x9E, 0x80},
	{0x78, 0x44},
	{0x30, 0x88},
	{0x31, 0x08},
	{0x32, 0x30},
	{0x33, 0x54},
	{0x34, 0x30},
	{0x35, 0x40},
	{0x3A, 0xA0},
	{0x45, 0x87},
	{0x56, 0x00},
	{0x59, 0x40},
	{0x61, 0x10},
	{0x85, 0x28},
	{0x8A, 0x04},
	{0x91, 0x22},
	{0x9B, 0x83},
	{0x5B, 0xB2},
	{0x5C, 0xF6},
	{0x5D, 0xE7},
	{0x5E, 0xB0},
	{0x64, 0xC0},
	{0x65, 0x32},
	{0x66, 0x00},
	{0x67, 0x46},
	{0x68, 0x20},
	{0x69, 0xF4},
	{0x6A, 0x28},
	{0x7A, 0x80},
	{0x82, 0x30},
	{0x8F, 0x90},
	{0x9D, 0x71},
	{0xBF, 0x01},
	{0x57, 0x22},
	{0x5E, 0x21},
	{0xBF, 0x00},
	{0x97, 0xFA},
	{0x13, 0x01},
	{0x96, 0x04},
	{0x4A, 0x01},
	{0x7E, 0xCC},
	{0x50, 0x02},
	{0x49, 0x10},
	{0x7F, 0x57},
	{0x8E, 0x00},
	{0xA7, 0x80},
	{0xBF, 0x01},
	{0x4E, 0x11},
	{0x50, 0x00},
	{0x51, 0x0F},
	{0x55, 0x20},
	{0x58, 0x00},
	{0xBF, 0x00},
	{0x19, 0x20},
	{0x79, 0x00},
	{0x12, 0x00},

	{0x02, 0x16},
	{0x01, 0x62},
	{0x00, 0x10},
	{REG_DLY, 0xc8},//160ms
};

static struct regval_list sensor_2866x1520p12_2lane_regs[] = {
	{0x12, 0x40},
	{0x48, 0x8A},
	{0x48, 0x0A},
	{0x0E, 0x11},
	{0x0F, 0x04},
	{0x10, 0x3C},
	{0x11, 0x80},
	{0x0D, 0x50},
	{0x57, 0x60},
	{0x58, 0x18},
	{0x5F, 0x41},
	{0x60, 0x24},
	{0x20, 0xEE},
	{0x21, 0x02},
	{0x22, 0x00},
	{0x23, 0x0f},
	{0x24, 0xA0},
	{0x25, 0xF0},
	{0x26, 0x52},
	{0x27, 0xEA},
	{0x28, 0x21},
	{0x29, 0x02},
	{0x2A, 0xDE},
	{0x2B, 0x12},
	{0x2C, 0x00},
	{0x2D, 0x00},
	{0x2E, 0x85},
	{0x2F, 0x44},
	{0x41, 0xA2},
	{0x42, 0x22},
	{0x46, 0x18},
	{0x47, 0x72},
	{0x80, 0x82}, //0x02 0x82
	{0xAF, 0x22},
	{0xBD, 0x80},
	{0xBE, 0x0A},
	{0x1D, 0x00},
	{0x1E, 0x08}, //0x04 0x08
	{0x6C, 0x40},
	{0x70, 0xD5},
	{0x71, 0x9B},
	{0x72, 0x6D},
	{0x73, 0x49},
	{0x75, 0x96},
	{0x74, 0x12},
	{0x89, 0x0F},
	{0x0C, 0x80},
	{0x6B, 0x20},
	{0x86, 0x00},
	{0x9E, 0x80},
	{0x78, 0x44},
	{0x30, 0x88},
	{0x31, 0x08},
	{0x32, 0x30},
	{0x33, 0x54},
	{0x34, 0x30},
	{0x35, 0x40},
	{0x3A, 0xA0},
	{0x45, 0x87},
	{0x56, 0x00},
	{0x59, 0x40},
	{0x61, 0x10},
	{0x85, 0x28},
	{0x8A, 0x04},
	{0x91, 0x22},
	{0x9B, 0x83},
	{0x5B, 0xB2},
	{0x5C, 0xF6},
	{0x5D, 0xE7},
	{0x5E, 0xB0},
	{0x64, 0xC0},
	{0x65, 0x32},
	{0x66, 0x00},
	{0x67, 0x46},
	{0x68, 0x20},
	{0x69, 0xF4},
	{0x6A, 0x28},
	{0x7A, 0x80},
	{0x82, 0x30},
	{0x8F, 0x90},
	{0x9D, 0x71},
	{0xBF, 0x01},
	{0x57, 0x22},
	{0x5E, 0x21},
	{0xBF, 0x00},
	{0x97, 0xFA},
	{0x13, 0x01},
	{0x96, 0x04},
	{0x4A, 0x01},
	{0x7E, 0xCC},
	{0x50, 0x02},
	{0x49, 0x10},
	{0x7F, 0x57},
	{0x8E, 0x00},
	{0xA7, 0x80},
	{0xBF, 0x01},
	{0x4E, 0x11},
	{0x50, 0x00},
	{0x51, 0x0F},
	{0x55, 0x20},
	{0x58, 0x00},
	{0xBF, 0x00},
	{0x19, 0x20},
	{0x79, 0x00},
	{0x12, 0x00},

	{0x02, 0x16},
	{0x01, 0x62},
	{0x00, 0x10},
	{REG_DLY, 0xc8},//160ms
};
#endif

#if K302P_4LANE
#if 0
static struct regval_list sensor_2866x1520p30_4lane_regs[] = {
	{0x12, 0x40},
	{0x48, 0x8F},
	{0x48, 0x0F},
	{0x0E, 0x11},
	{0x0F, 0x04},
	{0x10, 0x1E},
	{0x11, 0x80},
	{0x0D, 0xA0},
	{0x57, 0x60},
	{0x58, 0x18},
	{0x5F, 0x41},
	{0x60, 0x24},
	{0x20, 0xEE},
	{0x21, 0x02},
	{0x22, 0x40},
	{0x23, 0x06},
	{0x24, 0xA0},
	{0x25, 0xF0},
	{0x26, 0x52},
	{0x27, 0xEA},
	{0x28, 0x21},
	{0x29, 0x02},
	{0x2A, 0xDE},
	{0x2B, 0x12},
	{0x2C, 0x00},
	{0x2D, 0x00},
	{0x2E, 0x85},
	{0x2F, 0x44},
	{0x41, 0xA2},
	{0x42, 0x22},
	{0x46, 0x18},
	{0x47, 0x72},
	{0x80, 0x02},
	{0xAF, 0x22},
	{0xBD, 0x80},
	{0xBE, 0x0A},
	{0x1D, 0x00},
	{0x1E, 0x04},
	{0x6C, 0x40},
	{0x70, 0x8D},
	{0x71, 0x4D},
	{0x72, 0x2D},
	{0x73, 0x39},
	{0x75, 0xA3},
	{0x74, 0x12},
	{0x89, 0x1B},
	{0x0C, 0x80},
	{0x6B, 0x20},
	{0x86, 0x40},
	{0x9E, 0x80},
	{0x78, 0x44},
	{0x30, 0x88},
	{0x31, 0x08},
	{0x32, 0x30},
	{0x33, 0x54},
	{0x34, 0x30},
	{0x35, 0x40},
	{0x3A, 0xA0},
	{0x45, 0x87},
	{0x56, 0x00},
	{0x59, 0x40},
	{0x61, 0x10},
	{0x85, 0x28},
	{0x8A, 0x04},
	{0x91, 0x22},
	{0x9B, 0x83},
	{0x5B, 0xB2},
	{0x5C, 0xF6},
	{0x5D, 0xE7},
	{0x5E, 0xB0},
	{0x64, 0xC0},
	{0x65, 0x32},
	{0x66, 0x00},
	{0x67, 0x46},
	{0x68, 0x20},
	{0x69, 0xF4},
	{0x6A, 0x28},
	{0x7A, 0x80},
	{0x82, 0x30},
	{0x8F, 0x90},
	{0x9D, 0x71},
	{0xBF, 0x01},
	{0x57, 0x22},
	{0x5E, 0x21},
	{0xBF, 0x00},
	{0x97, 0xFA},
	{0x13, 0x01},
	{0x96, 0x04},
	{0x4A, 0x01},
	{0x7E, 0xCC},
	{0x50, 0x02},
	{0x49, 0x10},
	{0x7F, 0x57},
	{0x8E, 0x00},
	{0xA7, 0x80},
	{0xBF, 0x01},
	{0x4E, 0x11},
	{0x50, 0x00},
	{0x51, 0x0F},
	{0x55, 0x20},
	{0x58, 0x00},
	{0xBF, 0x00},
	{0x19, 0x20},
	{0x79, 0x00},
	{0x12, 0x00},

	{0x02, 0x16},
	{0x01, 0x62},
	{0x00, 0x10},
};
#endif
static struct regval_list sensor_2866x1520p60_4lane_regs[] = {
	{0x12, 0x40},
	{0x48, 0x8B},
	{0x48, 0x0B},
	{0x0E, 0x11},
	{0x0F, 0x04},
	{0x10, 0x3a},
	{0x11, 0x80},
	{0x0D, 0x50},
	{0x57, 0x60},
	{0x58, 0x18},
	{0x5F, 0x41},
	{0x60, 0x24},
	{0x20, 0x77},
	{0x21, 0x01},
	{0x22, 0x40},
	{0x23, 0x06},
	{0x24, 0x50},
	{0x25, 0xF0},
	{0x26, 0x51},
	{0x27, 0x6F},
	{0x28, 0x21},
	{0x29, 0x01},
	{0x2A, 0x69},
	{0x2B, 0x11},
	{0x2C, 0x00},
	{0x2D, 0x00},
	{0x2E, 0x85},
	{0x2F, 0x44},
	{0x41, 0xA2},
	{0x42, 0x22},
	{0x46, 0x18},
	{0x47, 0x72},
	{0x80, 0x02},
	{0xAF, 0x12},
	{0xBD, 0x80},
	{0xBE, 0x0A},
	{0x1D, 0x00},
	{0x1E, 0x04},
	{0x6C, 0x40},
	{0x70, 0xD5},
	{0x71, 0x97},
	{0x72, 0x4B},
	{0x73, 0x36},
	{0x75, 0xA4},
	{0x74, 0x12},
	{0x89, 0x17},
	{0x0C, 0x80},
	{0x6B, 0x20},
	{0x86, 0x40},
	{0x9E, 0x80},
	{0x78, 0x44},
	{0x6F, 0x80},
	{0x30, 0x88},
	{0x31, 0x08},
	{0x32, 0x30},
	{0x33, 0x54},
	{0x34, 0x30},
	{0x35, 0x40},
	{0x3A, 0xA0},
	{0x45, 0x87},
	{0x56, 0x00},
	{0x59, 0x40},
	{0x61, 0x10},
	{0x85, 0x28},
	{0x8A, 0x04},
	{0x91, 0x22},
	{0x9B, 0x83},
	{0x5B, 0xB2},
	{0x5C, 0xF6},
	{0x5D, 0xE7},
	{0x5E, 0xB0},
	{0x64, 0xC0},
	{0x65, 0x32},
	{0x66, 0x00},
	{0x67, 0x46},
	{0x68, 0x20},
	{0x69, 0xF4},
	{0x6A, 0x28},
	{0x7A, 0x80},
	{0x82, 0x30},
	{0x8F, 0x90},
	{0x9D, 0x71},
	{0xBF, 0x01},
	{0x57, 0x22},
	{0x5E, 0x21},
	{0xBF, 0x00},
	{0x97, 0xFA},
	{0x13, 0x01},
	{0x96, 0x04},
	{0x4A, 0x01},
	{0x7E, 0xCC},
	{0x50, 0x02},
	{0x49, 0x10},
	{0x7F, 0x57},
	{0x8E, 0x00},
	{0xA7, 0x80},
	{0xBF, 0x01},
	{0x4E, 0x11},
	{0x50, 0x00},
	{0x51, 0x0F},
	{0x55, 0x20},
	{0x58, 0x00},
	{0xBF, 0x00},
	{0x19, 0x20},
	{0x79, 0x00},
	{0x12, 0x00},
	{0x00, 0x10},
};
#endif

#if K302P_4LANE_WDR
static struct regval_list sensor_2866x1520p30_4lane_wdr_regs[] = {
	{0x12, 0x48},
	{0x48, 0x8B},
	{0x48, 0x0B},
	{0x0E, 0x12},
	{0x0F, 0x04},
	{0x10, 0x43},
	{0x11, 0x80},
	{0x0D, 0x50},
	{0x57, 0x60},
	{0x58, 0x18},
	{0x5F, 0x41},
	{0x60, 0x20},
	{0x20, 0x77},
	{0x21, 0x01},
	{0x22, 0x80},
	{0x23, 0x0C},
	{0x24, 0x50},
	{0x25, 0xF0},
	{0x26, 0x51},
	{0x27, 0x6F},
	{0x28, 0x41},
	{0x29, 0x01},
	{0x2A, 0x69},
	{0x2B, 0x11},
	{0x2C, 0x00},
	{0x2D, 0x00},
	{0x2E, 0x85},
	{0x2F, 0x44},
	{0x41, 0xA2},
	{0x42, 0x22},
	{0x46, 0x1C},
	{0x47, 0x72},
	{0x80, 0x02},
	{0xAF, 0x12},
	{0xBD, 0x80},
	{0xBE, 0x0A},
	{0x1D, 0x00},
	{0x1E, 0x04},
	{0x6C, 0x40},
	{0x70, 0xD5},
	{0x71, 0x97},
	{0x72, 0x4B},
	{0x73, 0x36},
	{0x75, 0xA4},
	{0x74, 0x12},
	{0x89, 0x17},
	{0x0C, 0x80},
	{0x6B, 0x20},
	{0x86, 0x40},
	{0x9E, 0x80},
	{0x78, 0x44},
	{0x6F, 0x80},
	{0x30, 0x88},
	{0x31, 0x08},
	{0x32, 0x30},
	{0x33, 0x54},
	{0x34, 0x30},
	{0x35, 0x40},
	{0x3A, 0xA0},
	{0x45, 0x87},
	{0x56, 0x00},
	{0x59, 0x40},
	{0x61, 0x10},
	{0x85, 0x28},
	{0x8A, 0x04},
	{0x91, 0x22},
	{0x9B, 0x83},
	{0x5B, 0xB2},
	{0x5C, 0xF6},
	{0x5D, 0xE7},
	{0x5E, 0xB0},
	{0x64, 0xC0},
	{0x65, 0x32},
	{0x66, 0x00},
	{0x67, 0x46},
	{0x68, 0x20},
	{0x69, 0xF4},
	{0x6A, 0x28},
	{0x7A, 0x80},
	{0x82, 0x30},
	{0x8F, 0x90},
	{0x9D, 0x71},
	{0xBF, 0x01},
	{0x57, 0x22},
	{0x5E, 0x21},
	{0xBF, 0x00},
	{0x97, 0xFA},
	{0x13, 0x01},
	{0x96, 0x04},
	{0x4A, 0x01},
	{0x7E, 0xCC},
	{0x50, 0x02},
	{0x49, 0x10},
	{0x7F, 0x57},
	{0x8E, 0x00},
	{0xA7, 0x80},
	{0xBF, 0x01},
	{0x4E, 0x11},
	{0x50, 0x00},
	{0x51, 0x0F},
	{0x55, 0x20},
	{0x58, 0x00},
	{0xBF, 0x00},
	{0x19, 0x20},
	{0x07, 0x03},
	{0x1B, 0x4F},
	{0x06, 0x23},
	{0x03, 0xFF},
	{0x04, 0xFF},
	{0x79, 0x00},
	{0x12, 0x08},

	{0x02, 0x01},
	{0x01, 0x01},
	{0x00, 0x10},
};
#endif
/*
 * Here we'll try to encapsulate the changes for just the output
 * video format.
 *
 */

static struct regval_list sensor_fmt_raw[] = {

};


/*
 * Code for dealing with controls.
 * fill with different sensor module
 * different sensor module has different settings here
 * if not support the follow function ,retrun -EINVAL
 */

static int k302p_sensor_vts;

static int sensor_g_exp(struct v4l2_subdev *sd, __s32 *value)
{
	struct sensor_info *info = to_state(sd);
	*value = info->exp;
	sensor_dbg("sensor_get_exposure = %d\n", info->exp);
	return 0;
}

static int sensor_s_exp(struct v4l2_subdev *sd, unsigned int exp_val)
{

	struct sensor_info *info = to_state(sd);
	int		tmp_exp_val;

	tmp_exp_val = exp_val / 16;
	if (info->isp_wdr_mode == ISP_DOL_WDR_MODE) {
		sensor_write(sd, 0x02, (tmp_exp_val >> 8) & 0xFF);
		sensor_write(sd, 0x01, (tmp_exp_val) & 0xFF);
		sensor_print("sensor_set_long_exp:%d\n", tmp_exp_val);

		tmp_exp_val = tmp_exp_val / HDR_RATIO;
		sensor_write(sd, 0x06, 0x7F);
		sensor_write(sd, 0x02, (tmp_exp_val >> 8) & 0xFF);
		sensor_write(sd, 0x01, (tmp_exp_val) & 0xFF);
		sensor_print("sensor_set_short_exp:%d\n", tmp_exp_val);
	} else {
		sensor_write(sd, 0x02, (tmp_exp_val >> 8) & 0xFF);
		sensor_write(sd, 0x01, (tmp_exp_val) & 0xFF);
		sensor_print("exp_val:%d\n", exp_val);
		info->exp = exp_val;
	}

	return 0;
}

static int sensor_g_gain(struct v4l2_subdev *sd, __s32 *value)
{
	struct sensor_info *info = to_state(sd);
	*value = info->gain;
	sensor_dbg("sensor_get_gain = %d\n", info->gain);
	return 0;
}


static int setSensorGain(struct v4l2_subdev *sd, int gain)
{
	int	again = 0;
	int	tmp = 0;
	unsigned char	regdata = 0;
	data_type gain_tmp;
	/* gain: 16=1X, 32=2X, 48=3X, 64=4X, ......, 240=15X, 256=16X, ...... */
	again = gain;
	while (again > 31) {
		again = again >> 1;
		tmp++;
	}
	if (again > 15)
		again = again - 16;
	regdata = (unsigned char)((tmp<<4) | again);
	sensor_write(sd, 0x00, regdata & 0xFF);
	sensor_read(sd, 0x00, &gain_tmp);
	sensor_print("gain=%d, tmp=%d, gain_tmp=0x%02x\n", gain, tmp, gain_tmp);
	return 0;
}
static int sensor_s_gain(struct v4l2_subdev *sd, int gain_val)
{
	struct sensor_info *info = to_state(sd);
	/*	sensor_print("f37p sensor gain value is %d\n",gain_val); */

	if (gain_val == info->gain) {
		return 0;
	}

	sensor_print("gain_val:%d\n", gain_val);
	setSensorGain(sd, gain_val);
	info->gain = gain_val;

#ifdef _SOI_BSUN_OPT
	{
		if (gain_val >= 32) { /*	When AGain >= 2X */
			if (BSunMode != 1) {
				sensor_write(sd, 0xC0, 0x2F);	/*	Reg2F[5]=0 */
				sensor_write(sd, 0xC1, 0x44);
				sensor_write(sd, 0xC2, 0x0C);	/*	Reg0C[6]=0 */
				sensor_write(sd, 0xC3, 0x00);
				sensor_write(sd, 0xC4, 0x82);	/*	Reg82[1]=0 */
				sensor_write(sd, 0xC5, 0x20);
				sensor_write(sd, 0x1F, 0x80);	/*	Trigger group write. */
				BSunMode = 1;
			}
		} else { /*	When AGain < 2X */
			if (BSunMode != 0) {
				sensor_write(sd, 0xC0, 0x2F);	/*	Reg2F[5]=1 */
				sensor_write(sd, 0xC1, 0x64);
				sensor_write(sd, 0xC2, 0x0C);	/*	Reg0C[6]=1 */
				sensor_write(sd, 0xC3, 0x40);
				sensor_write(sd, 0xC4, 0x82);	/*	Reg82[1]=1 */
				sensor_write(sd, 0xC5, 0x22);
				sensor_write(sd, 0x1F, 0x80);	/* Trigger group write. */
				BSunMode = 0;
			}
		}
	}
#endif
#ifdef _SOI_PRECHG_OPT
	{
		if (gain_val > 16) { /* When AGain >= 1X */
			if (PreChgMode != 1) {
				sensor_write(sd, 0xC0, 0x8A);
				sensor_write(sd, 0xC1, 0x06);
				sensor_write(sd, 0x1F, 0x80);	/* Trigger group write. */
				PreChgMode = 1;
			}
		} else { /* When AGain = 1X */
			if (PreChgMode != 0) {
				sensor_write(sd, 0xC0, 0x8A);
				sensor_write(sd, 0xC1, 0x04);
				sensor_write(sd, 0x1F, 0x80);	/* Trigger group write. */
				PreChgMode = 0;
			}
		}
	}
#endif

	return 0;
}

static int sensor_s_exp_gain(struct v4l2_subdev *sd, struct sensor_exp_gain *exp_gain)

{

	int exp_val, gain_val;
	int shutter, frame_length;
	struct sensor_info *info = to_state(sd);

	exp_val = exp_gain->exp_val;
	gain_val = exp_gain->gain_val;

	if (gain_val < 1 * 16)
		gain_val = 16;

	if (exp_val > 0xfffff)
		exp_val = 0xfffff;

	shutter = exp_val >> 4;
	if (shutter > k302p_sensor_vts - 4)
		frame_length = shutter + 4;
	else
		frame_length = k302p_sensor_vts;

	sensor_print("frame_length = %d\n", frame_length);
	/* write vts */
	sensor_write(sd, 0x22, frame_length & 0xff);
	sensor_write(sd, 0x23, frame_length >> 8);

	sensor_s_exp(sd, exp_val);
	sensor_s_gain(sd, gain_val);

	info->exp = exp_val;
	info->gain = gain_val;
	return 0;
}

/*
 *set && get sensor flip
 */

static int sensor_get_fmt_mbus_core(struct v4l2_subdev *sd, int *code)
{
	struct sensor_info *info = to_state(sd);

	*code = info->fmt->mbus_code;

	return 0;
}


static int sensor_s_hflip(struct v4l2_subdev *sd, int enable)
{
	data_type get_value;
	data_type set_value;

	if (!(enable == 0 || enable == 1))
		return -1;

	get_value = sensor_flip_status;
	/* sensor_print("--sensor hfilp set[%d] read value:0x%X --\n", enable, get_value); */
	if (enable)
		set_value = get_value | 0x10;
	else
		set_value = get_value & 0xEF;

	sensor_flip_status = set_value;
	/* sensor_print("--set sensor hfilp the value:0x%X \n--", set_value); */
	sensor_write(sd, 0x12, set_value);
	return 0;
}

static int sensor_s_vflip(struct v4l2_subdev *sd, int enable)
{
	data_type get_value;
	data_type set_value;

	if (!(enable == 0 || enable == 1))
		return -1;

	get_value = sensor_flip_status;
	/* sensor_print("--sensor vfilp set[%d] read value:0x%X --\n", enable, get_value); */
	if (enable)
		set_value = get_value | 0x20;
	else
		set_value = get_value & 0xDF;

	sensor_flip_status = set_value;
	/* sensor_print("--set sensor vfilp the value:0x%X --\n", set_value); */
	sensor_write(sd, 0x12, set_value);
	return 0;

}


/*
 * Stuff that knows about the sensor.
 */
static int sensor_power(struct v4l2_subdev *sd, int on)
{
	switch (on) {
	case STBY_ON:
		sensor_print("STBY_ON!\n");
		cci_lock(sd);
		vin_gpio_write(sd, PWDN, CSI_GPIO_HIGH);
		cci_unlock(sd);
		break;
	case STBY_OFF:
		sensor_print("STBY_OFF!\n");
		cci_lock(sd);
		vin_set_mclk_freq(sd, MCLK);
		vin_set_mclk(sd, ON);
		usleep_range(10000, 12000);
		vin_gpio_write(sd, PWDN, CSI_GPIO_LOW);
		usleep_range(10000, 12000);
		cci_unlock(sd);
		usleep_range(10000, 12000);
		break;
	case PWR_ON:
		sensor_print("PWR_ON!\n");
		cci_lock(sd);
		vin_gpio_set_status(sd, PWDN, 1);
		vin_gpio_set_status(sd, RESET, 1);
		vin_gpio_set_status(sd, POWER_EN, 1);
		vin_gpio_write(sd, PWDN, CSI_GPIO_HIGH);        /* Pull up PWDN pin initially. */
		vin_gpio_write(sd, RESET, CSI_GPIO_LOW);        /* Pull down RESET# pin initially. */
		usleep_range(1000, 1200);
		#if 0/* sk */
		vin_gpio_write(sd, POWER_EN, CSI_GPIO_HIGH);
		#endif
		/* vin_set_pmu_channel(sd, CMBCSI, ON); */
		vin_set_pmu_channel(sd, AVDD, ON);              /* Turn on AVDD. */
		usleep_range(100, 120);
		vin_set_pmu_channel(sd, DVDD, ON);             /* DVDD is controlled internally. */
		usleep_range(1000, 1200);
		vin_set_pmu_channel(sd, IOVDD, ON);              /* Turn on DOVDD. */
		vin_gpio_write(sd, RESET, CSI_GPIO_HIGH);	    /* Pull up RESET# pin. */
		usleep_range(1000, 1200);
		vin_set_mclk(sd, ON);
		usleep_range(1000, 1200);
		vin_set_mclk_freq(sd, MCLK);
		usleep_range(1000, 1200);
		vin_gpio_write(sd, RESET, CSI_GPIO_LOW);
		usleep_range(10000, 12000);
		vin_gpio_write(sd, RESET, CSI_GPIO_HIGH);
		usleep_range(1000, 1200);
		vin_gpio_write(sd, PWDN, CSI_GPIO_LOW);
		usleep_range(10000, 12000);
		cci_unlock(sd);
		break;
	case PWR_OFF:
		sensor_print("PWR_OFF!do nothing\n");
		cci_lock(sd);

		usleep_range(10000, 12000);					/* > 512 MCLK cycles */
		vin_gpio_write(sd, PWDN, CSI_GPIO_HIGH);		/* Pull up PWDN pin. */
		usleep_range(1000, 1200);						/* Add delay. */
		vin_gpio_write(sd, RESET, CSI_GPIO_LOW);		/* Pull down RESET# pin. */
		usleep_range(1000, 1200);						/* Add delay. */

		vin_set_mclk(sd, OFF);						/* Disable MCLK. */
		vin_gpio_write(sd, POWER_EN, CSI_GPIO_LOW);

		usleep_range(1000, 1200);						/* Add delay. */
		vin_set_pmu_channel(sd, IOVDD, OFF);			/* Turn off DOVDD. */
		vin_set_pmu_channel(sd, DVDD, OFF);

		vin_set_pmu_channel(sd, AVDD, OFF);			/* urn off AVDD. */
		usleep_range(10000, 12000);
		vin_gpio_write(sd, PWDN, CSI_GPIO_HIGH);
		vin_gpio_write(sd, RESET, CSI_GPIO_LOW);
		vin_gpio_set_status(sd, RESET, 0);
		vin_gpio_set_status(sd, PWDN, 0);
		cci_unlock(sd);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int sensor_reset(struct v4l2_subdev *sd, u32 val)
{

	switch (val) {
	case 0:
		vin_gpio_write(sd, RESET, CSI_GPIO_HIGH);
		usleep_range(10000, 12000);
		break;
	case 1:
		vin_gpio_write(sd, RESET, CSI_GPIO_LOW);
		usleep_range(10000, 12000);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int sensor_detect(struct v4l2_subdev *sd)
{
	data_type rdval;

	sensor_read(sd, ID_REG_HIGH, &rdval);
	sensor_print("ID_VAL_HIGH = %2x, Done!\n", rdval);
	if (rdval != ID_VAL_HIGH)
		return -ENODEV;
	sensor_read(sd, ID_REG_LOW, &rdval);
	sensor_print("ID_VAL_LOW = %2x, Done!\n", rdval);
	if (rdval != ID_VAL_LOW)
		return -ENODEV;

	sensor_print("Done!\n");
	return 0;
}

static int sensor_init(struct v4l2_subdev *sd, u32 val)
{
	int ret;
	struct sensor_info *info = to_state(sd);

	sensor_dbg("sensor_init\n");

	/* Make sure it is a target sensor */
	ret = sensor_detect(sd);
	if (ret) {
		sensor_err("chip found is not an target chip.\n");
		return ret;
	}

	info->focus_status = 0;
	info->low_speed = 0;
	info->width = 2688;
	info->height = 1520;
	info->hflip = 0;
	info->vflip = 0;
	info->gain = 0;
	info->exp = 0;
	info->tpf.numerator = 1;
	info->tpf.denominator = 30;	/* 30fps */
	info->preview_first_flag = 1;
	return 0;
}

static long sensor_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	int ret = 0;
	struct sensor_info *info = to_state(sd);

	switch (cmd) {
	case GET_CURRENT_WIN_CFG:
		sensor_dbg("%s: GET_CURRENT_WIN_CFG, info->current_wins=%p\n", __func__, info->current_wins);

		if (info->current_wins != NULL) {
			memcpy(arg, info->current_wins, sizeof(struct sensor_win_size));
			ret = 0;
		} else {
			sensor_err("empty wins!\n");
			ret = -1;
		}
		break;
	case SET_FPS:
		break;
	case VIDIOC_VIN_SENSOR_EXP_GAIN:
		sensor_s_exp_gain(sd, (struct sensor_exp_gain *)arg);
		break;
/*	case VIDIOC_VIN_SENSOR_SET_FPS:
		ret = sensor_s_fps(sd, (struct sensor_fps *)arg);
		break; */
	case VIDIOC_VIN_SENSOR_CFG_REQ:
		sensor_cfg_req(sd, (struct sensor_config *)arg);
		break;
	case VIDIOC_VIN_GET_SENSOR_CODE:
		sensor_get_fmt_mbus_core(sd, (int *)arg);
		break;
	default:
		return -EINVAL;
	}
	return ret;
}

/*
 * Store information about the video data format.
 */
static struct sensor_format_struct sensor_formats[] = {
	{
		.desc = "Raw RGB Bayer",
		.mbus_code = MEDIA_BUS_FMT_SBGGR10_1X10,/* .mbus_code = MEDIA_BUS_FMT_SBGGR10_1X10, */
		.regs = sensor_fmt_raw,
		.regs_size = ARRAY_SIZE(sensor_fmt_raw),
		.bpp = 1
	},
};
#define N_FMTS ARRAY_SIZE(sensor_formats)

/*
 * Then there is the issue of window sizes.  Try to capture the info here.
 */

static struct sensor_win_size sensor_win_sizes[] = {
#if K302P_2LANE
	{
	.width      = 2688,
	.height     = 1520,
	.hoffset    = 0,
	.voffset    = 0,
	.hts        = 3000,
	.vts        = 1600,
	.pclk       = 144 * 1000 * 1000,
	.mipi_bps   = 360 * 1000 * 1000,
	.fps_fixed  = 30,
	.bin_factor = 1,
	.intg_min   = 1 << 4,
	.intg_max   = 1600 << 4,
	.gain_min   = 1 << 4,
	.gain_max	 = 15 << 4,
	.regs = sensor_2866x1520p30_2lane_regs,
	.regs_size = ARRAY_SIZE(sensor_2866x1520p30_2lane_regs),
	.set_size = NULL,
	},

	{/*slave mode*/
	.width      = 1920,
	.height     = 1088,
	.hoffset    = 384,
	.voffset    = 216,
	.hts        = 3000,
	.vts        = 3200,
	.pclk       = 144 * 1000 * 1000,
	.mipi_bps   = 360 * 1000 * 1000,
	.fps_fixed  = 15,
	.bin_factor = 1,
	.intg_min   = 1 << 4,
	.intg_max   = 3200 << 4,
	.gain_min   = 1 << 4,
	.gain_max	 = 15 << 4,
	.regs = sensor_2866x1520p15_2lane_regs,
	.regs_size = ARRAY_SIZE(sensor_2866x1520p15_2lane_regs),
	.set_size = NULL,
	},

	{/*slave mode*/
	.width      = 1920,
	.height     = 1088,
	.hoffset    = 384,
	.voffset    = 216,
	.hts        = 3000,
	.vts        = 3840,
	.pclk       = 144 * 1000 * 1000,
	.mipi_bps   = 360 * 1000 * 1000,
	.fps_fixed  = 12, //12.5
	.bin_factor = 1,
	.intg_min   = 1 << 4,
	.intg_max   = 3840 << 4,
	.gain_min   = 1 << 4,
	.gain_max	 = 15 << 4,
	.regs = sensor_2866x1520p12_2lane_regs,
	.regs_size = ARRAY_SIZE(sensor_2866x1520p12_2lane_regs),
	.set_size = NULL,
	},
#endif
#if K302P_4LANE
	{
	.width      = 2688,
	.height     = 1520,
	.hoffset    = 0,
	.voffset    = 0,
	.hts        = 3000,
	.vts        = 1600,
	.pclk       = 144 * 1000 * 1000,
	.mipi_bps   = 180 * 1000 * 1000,
	.fps_fixed  = 30,
	.bin_factor = 1,
	.intg_min   = 1 << 4,
	.intg_max   = 1600 << 4,
	.gain_min   = 1 << 4,
	.gain_max	 = 15 << 4,
	.regs = sensor_2866x1520p60_4lane_regs,
	.regs_size = ARRAY_SIZE(sensor_2866x1520p60_4lane_regs),
	.set_size = NULL,
	},
#endif
#if K302P_4LANE_WDR
	{
	.width      = 2688,
	.height     = 1520,
	.hoffset    = 0,
	.voffset    = 0,
	.hts        = 3600,
	.vts        = 3200,
	.pclk       = 288 * 1000 * 1000,
	.mipi_bps   = 360 * 1000 * 1000,
	.if_mode    = MIPI_VC_WDR_MODE,
	.wdr_mode   = ISP_DOL_WDR_MODE,
	.fps_fixed  = 25,
	.bin_factor = 1,
	.intg_min   = 1 << 4,
	.intg_max   = 3200 << 4,
	.gain_min   = 1 << 4,
	.gain_max	 = 15 << 4,
	.regs = sensor_2866x1520p30_4lane_wdr_regs,
	.regs_size = ARRAY_SIZE(sensor_2866x1520p30_4lane_wdr_regs),
	.set_size = NULL,
	},
#endif
};

#define N_WIN_SIZES (ARRAY_SIZE(sensor_win_sizes))

static int sensor_g_mbus_config(struct v4l2_subdev *sd, struct v4l2_mbus_config *cfg)
{
	__maybe_unused struct sensor_info *info = to_state(sd);

	cfg->type = V4L2_MBUS_CSI2;

#if K302P_2LANE
	cfg->flags = 0 | V4L2_MBUS_CSI2_2_LANE | V4L2_MBUS_CSI2_CHANNEL_0;
#else
	if (info->isp_wdr_mode == ISP_DOL_WDR_MODE)
		cfg->flags = 0 | V4L2_MBUS_CSI2_4_LANE | V4L2_MBUS_CSI2_CHANNEL_0 | V4L2_MBUS_CSI2_CHANNEL_1;
	else
		cfg->flags = 0 | V4L2_MBUS_CSI2_4_LANE | V4L2_MBUS_CSI2_CHANNEL_0;
#endif
	return 0;
}

static int sensor_g_ctrl(struct v4l2_ctrl *ctrl)
{
	struct sensor_info *info = container_of(ctrl->handler, struct sensor_info, handler);
	struct v4l2_subdev *sd = &info->sd;

	switch (ctrl->id) {
	case V4L2_CID_GAIN:
		return sensor_g_gain(sd, &ctrl->val);
	case V4L2_CID_EXPOSURE:
		return sensor_g_exp(sd, &ctrl->val);
	}
	return -EINVAL;
}

static int sensor_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct sensor_info *info = container_of(ctrl->handler, struct sensor_info, handler);
	struct v4l2_subdev *sd = &info->sd;

	switch (ctrl->id) {
	case V4L2_CID_GAIN:
		return sensor_s_gain(sd, ctrl->val);
	case V4L2_CID_EXPOSURE:
		return sensor_s_exp(sd, ctrl->val);
	case V4L2_CID_HFLIP:
		return sensor_s_hflip(sd, ctrl->val);
	case V4L2_CID_VFLIP:
		return sensor_s_vflip(sd, ctrl->val);
	}
	return -EINVAL;
}

static int sensor_reg_init(struct sensor_info *info)
{
	int ret;
	struct v4l2_subdev *sd = &info->sd;
	struct sensor_format_struct *sensor_fmt = info->fmt;
	struct sensor_win_size *wsize = info->current_wins;

	sensor_dbg("sensor_reg_init, ARRAY_SIZE(sensor_default_regs)=%d\n", ARRAY_SIZE(sensor_default_regs));

	ret = sensor_write_array(sd, sensor_default_regs,
				 ARRAY_SIZE(sensor_default_regs));
	if (ret < 0) {
		sensor_err("write sensor_default_regs error\n");
		return ret;
	}

	sensor_dbg("sensor_reg_init, wsize=%p, wsize->regs=0x%x, wsize->regs_size=%d\n", wsize, wsize->regs, wsize->regs_size);

	sensor_write_array(sd, sensor_fmt->regs, sensor_fmt->regs_size);
#if K302P_4LANE_WDR
	sensor_flip_status = 0x28;			/* Normal mode (mirror off, flip off) */
#else
	sensor_flip_status = 0x20;
#endif

	if (wsize->regs)
		sensor_write_array(sd, wsize->regs, wsize->regs_size);

	info->width = wsize->width;
	info->height = wsize->height;
	k302p_sensor_vts = wsize->vts;
	sensor_dbg("k302p_sensor_vts = %d\n", k302p_sensor_vts);

	sensor_dbg("s_fmt set width = %d, height = %d\n", wsize->width, wsize->height);

	return 0;
}

static int sensor_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct sensor_info *info = to_state(sd);

	sensor_dbg("%s on = %d, %d*%d fps: %d code: %x\n", __func__, enable,
		     info->current_wins->width, info->current_wins->height,
		     info->current_wins->fps_fixed, info->fmt->mbus_code);

	if (!enable)
		return 0;

	return sensor_reg_init(info);
}

/* ----------------------------------------------------------------------- */
static const struct v4l2_ctrl_ops sensor_ctrl_ops = {
	.g_volatile_ctrl = sensor_g_ctrl,
	.s_ctrl = sensor_s_ctrl,
	.try_ctrl = sensor_try_ctrl,
};

static const struct v4l2_subdev_core_ops sensor_core_ops = {

	.reset = sensor_reset,
	.init = sensor_init,
	.s_power = sensor_power,
	.ioctl = sensor_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = sensor_compat_ioctl32,
#endif
};

static const struct v4l2_subdev_video_ops sensor_video_ops = {
	.s_parm = sensor_s_parm,
	.g_parm = sensor_g_parm,
	.s_stream = sensor_s_stream,
	.g_mbus_config = sensor_g_mbus_config,
};

static const struct v4l2_subdev_pad_ops sensor_pad_ops = {
	.enum_mbus_code = sensor_enum_mbus_code,
	.enum_frame_size = sensor_enum_frame_size,
	.get_fmt = sensor_get_fmt,
	.set_fmt = sensor_set_fmt,
};

static const struct v4l2_subdev_ops sensor_ops = {
	.core = &sensor_core_ops,
	.video = &sensor_video_ops,
	.pad = &sensor_pad_ops,
};

/* ----------------------------------------------------------------------- */
static struct cci_driver cci_drv = {
	.name = SENSOR_NAME,
	/* .addr_width = CCI_BITS_16, */
	.addr_width = CCI_BITS_8,
	.data_width = CCI_BITS_8,
};

static int sensor_init_controls(struct v4l2_subdev *sd, const struct v4l2_ctrl_ops *ops)
{
	struct sensor_info *info = to_state(sd);
	struct v4l2_ctrl_handler *handler = &info->handler;
	struct v4l2_ctrl *ctrl;
	int ret = 0;

	v4l2_ctrl_handler_init(handler, 4);

	ctrl = v4l2_ctrl_new_std(handler, ops, V4L2_CID_GAIN, 1 * 1600,
			      256 * 1600, 1, 1 * 1600);

	if (ctrl != NULL)
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE;

	ctrl = v4l2_ctrl_new_std(handler, ops, V4L2_CID_EXPOSURE, 1,
			      65536 * 16, 1, 1);
	v4l2_ctrl_new_std(handler, ops, V4L2_CID_HFLIP, 0, 1, 1, 0);
	v4l2_ctrl_new_std(handler, ops, V4L2_CID_VFLIP, 0, 1, 1, 0);

	if (ctrl != NULL)
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE;

	if (handler->error) {
		ret = handler->error;
		v4l2_ctrl_handler_free(handler);
	}

	sd->ctrl_handler = handler;

	return ret;
}

static int sensor_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct v4l2_subdev *sd;
	struct sensor_info *info;

	info = kzalloc(sizeof(struct sensor_info), GFP_KERNEL);
	if (info == NULL)
		return -ENOMEM;
	sd = &info->sd;

	cci_dev_probe_helper(sd, client, &sensor_ops, &cci_drv);
	sensor_init_controls(sd, &sensor_ctrl_ops);

	mutex_init(&info->lock);

	info->fmt = &sensor_formats[0];
	info->fmt_pt = &sensor_formats[0];
	info->win_pt = &sensor_win_sizes[0];
	info->fmt_num = N_FMTS;
	info->win_size_num = N_WIN_SIZES;
	info->sensor_field = V4L2_FIELD_NONE;
	info->combo_mode = CMB_TERMINAL_RES | CMB_PHYA_OFFSET3 | MIPI_NORMAL_MODE;
	info->stream_seq = MIPI_BEFORE_SENSOR;
	info->af_first_flag = 1;
	info->time_hs = 0x20;
	info->exp = 0;
	info->gain = 0;

	return 0;
}

static int sensor_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd;
	sd = cci_dev_remove_helper(client, &cci_drv);

	kfree(to_state(sd));
	return 0;
}

static const struct i2c_device_id sensor_id[] = {
	{SENSOR_NAME, 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, sensor_id);

static struct i2c_driver sensor_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = SENSOR_NAME,
		},
	.probe = sensor_probe,
	.remove = sensor_remove,
	.id_table = sensor_id,
};
static __init int init_sensor(void)
{
	int ret;
	ret = cci_dev_init_helper(&sensor_driver);

	return ret;
}

static __exit void exit_sensor(void)
{

	cci_dev_exit_helper(&sensor_driver);
}

#ifdef CONFIG_SUNXI_FASTBOOT
subsys_initcall_sync(init_sensor);
#else
module_init(init_sensor);
#endif
module_exit(exit_sensor);
