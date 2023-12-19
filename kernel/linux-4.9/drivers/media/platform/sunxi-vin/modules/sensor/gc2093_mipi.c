/*
 * A V4L2 driver for Raw cameras.
 *
 * Copyright (c) 2017 by Allwinnertech Co., Ltd.  http://www.allwinnertech.com
 *
 * Authors:  Zhao Wei <zhaowei@allwinnertech.com>
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

MODULE_AUTHOR("hzh");
MODULE_DESCRIPTION("A low-level driver for GC2093 sensors");
MODULE_LICENSE("GPL");

#define MCLK              (24*1000*1000)
#define V4L2_IDENT_SENSOR  0x2093

//define the registers
#define EXP_HIGH		0xff
#define EXP_MID			0x03
#define EXP_LOW			0x04
#define GAIN_HIGH		0xff
#define GAIN_LOW		0x24
/*
 * Our nominal (default) frame rate.
 */
#define ID_REG_HIGH		0x03f0
#define ID_REG_LOW		0x03f1
#define ID_VAL_HIGH		((V4L2_IDENT_SENSOR) >> 8)
#define ID_VAL_LOW		((V4L2_IDENT_SENSOR) & 0xff)
#define SENSOR_FRAME_RATE 30

#define HDR_RATIO 16
#define short_exp_max 124
/*
 * The GC2093 i2c address
 */
#define I2C_ADDR 0x6e

#define SENSOR_NUM 0x2
#define SENSOR_NAME "gc2093_mipi"
#define SENSOR_NAME_2 "gc2093_mipi_2"

/*
 * The default register settings
 */

static struct regval_list sensor_default_regs[] = {

};


static struct regval_list sensor_1080p30_regs[] = {
	/*MIPI clk:384Mbps/lane
	pclk:47.25Mbyte
	Wpclk:384MHz
	Rpclk:192MHz
	row time:29.625us
	window height:1125
	size:1920*1080*/
	/****system****/
	{0x03fe, 0xf0},
	{0x03fe, 0xf0},
	{0x03fe, 0xf0},
	{0x03fe, 0x00},
	{0x03f2, 0x00},
	{0x03f3, 0x00},
	{0x03f4, 0x36},
	{0x03f5, 0xc0},
	{0x03f6, 0x0B},
	{0x03f7, 0x11},
	{0x03f8, 0x30},
	{0x03f9, 0x42},
	{0x03fc, 0x8e},
	/****CISCTL & ANALOG****/
	{0x0087, 0x18},
	{0x00ee, 0x30},
	{0x00d0, 0xbf},
	{0x01a0, 0x00},
	{0x01a4, 0x40},
	{0x01a5, 0x40},
	{0x01a6, 0x40},
	{0x01af, 0x09},
	{0x0003, 0x04},
	{0x0004, 0x65},
	{0x0005, 0x05},
	{0x0006, 0x8e},
	{0x0007, 0x00},
	{0x0008, 0x11},
	{0x0009, 0x00},
	{0x000a, 0x02},
	{0x000b, 0x00},
	{0x000c, 0x04},
	{0x000d, 0x04},
	{0x000e, 0x40},
	{0x000f, 0x07},
	{0x0010, 0x8c},
	{0x0013, 0x15},
	{0x0019, 0x0c},
	{0x0041, 0x04},
	{0x0042, 0x65},
	{0x0053, 0x60},
	{0x008d, 0x92},
	{0x0090, 0x00},
	{0x00c7, 0xe1},
	{0x001b, 0x73},
	{0x0028, 0x0d},
	{0x0029, 0x40},
	{0x002b, 0x04},
	{0x002e, 0x23},
	{0x0037, 0x03},
	{0x0043, 0x04},
	{0x0044, 0x30},
	{0x004a, 0x01},
	{0x004b, 0x28},
	{0x0055, 0x30},
	{0x0066, 0x3f},
	{0x0068, 0x3f},
	{0x006b, 0x44},
	{0x0077, 0x00},
	{0x0078, 0x20},
	{0x007c, 0xa1},
	{0x00ce, 0x7c},
	{0x00d3, 0xd4},
	{0x00e6, 0x50},
	/*gain*/
	{0x00b6, 0xc0},
	{0x00b0, 0x68},
	{0x00b3, 0x00},
	{0x00b8, 0x01},
	{0x00b9, 0x00},
	{0x00b1, 0x01},
	{0x00b2, 0x00},
	/*isp*/
	{0x0101, 0x0c},
	{0x0102, 0x89},
	{0x0104, 0x01},
	{0x0107, 0xa6},
	{0x0108, 0xa9},
	{0x0109, 0xa8},
	{0x010a, 0xa7},
	{0x010b, 0xff},
	{0x010c, 0xff},
	{0x010f, 0x00},
	{0x0158, 0x00},
	{0x0428, 0x86},
	{0x0429, 0x86},
	{0x042a, 0x86},
	{0x042b, 0x68},
	{0x042c, 0x68},
	{0x042d, 0x68},
	{0x042e, 0x68},
	{0x042f, 0x68},
	{0x0430, 0x4f},
	{0x0431, 0x68},
	{0x0432, 0x67},
	{0x0433, 0x66},
	{0x0434, 0x66},
	{0x0435, 0x66},
	{0x0436, 0x66},
	{0x0437, 0x66},
	{0x0438, 0x62},
	{0x0439, 0x62},
	{0x043a, 0x62},
	{0x043b, 0x62},
	{0x043c, 0x62},
	{0x043d, 0x62},
	{0x043e, 0x62},
	{0x043f, 0x62},
	/*dark sun*/
	{0x0123, 0x08},
	{0x0123, 0x00},
	{0x0120, 0x01},
	{0x0121, 0x04},
	{0x0122, 0x65},
	{0x0124, 0x03},
	{0x0125, 0xff},
	{0x001a, 0x8c},
	{0x00c6, 0xe0},
	/*blk*/
	{0x0026, 0x30},
	{0x0142, 0x00},
	{0x0149, 0x1e},
	{0x014a, 0x0f},
	{0x014b, 0x00},
	{0x0155, 0x07},
	{0x0414, 0x78},
	{0x0415, 0x78},
	{0x0416, 0x78},
	{0x0417, 0x78},
	{0x04e0, 0x18},
	/*window*/
	{0x0192, 0x02},
	{0x0194, 0x03},
	{0x0195, 0x04},
	{0x0196, 0x38},
	{0x0197, 0x07},
	{0x0198, 0x80},
	/****DVP & MIPI****/
	{0x019a, 0x06},
	{0x007b, 0x2a},
	{0x0023, 0x2d},
	{0x0201, 0x27},
	{0x0202, 0x56},
	{0x0203, 0x8e},
	{0x0212, 0x80},
	{0x0213, 0x07},
	{0x0215, 0x10},
	{0x003e, 0x91},
};

static struct regval_list sensor_1920x1080p25_wdr_regs[] = {
	//mclk=24mhz
	//MIPI clk:704Mbps/lane
	//pclk:176mhz
	//HB:704*2
	//Linelength:2816
	//Wpclk:396Mbps
	//Rpclk:198Mbps
	//row time:16us
	//frame length:1250
	//size:1920*1080
	/****system****/
	{0x03fe, 0x80},
	{0x03fe, 0x80},
	{0x03fe, 0x80},
	{0x03fe, 0x00},
	{0x03f2, 0x00},
	{0x03f3, 0x00},
	{0x03f4, 0x36},
	{0x03f5, 0xc0},
	{0x03f6, 0x0B},
	{0x03f7, 0x01},
	{0x03f8, 0x58},
	{0x03f9, 0x40},
	{0x03fc, 0x8e},
	/****CISCTL & ANALOG****/
	{0x0087, 0x18},
	{0x00ee, 0x30},
	{0x00d0, 0xbf},
	{0x01a0, 0x00},
	{0x01a4, 0x40},
	{0x01a5, 0x40},
	{0x01a6, 0x40},
	{0x01af, 0x09},
	{0x0001, 0x00},
	{0x0002, 0x02},
	{0x0003, 0x04},
	{0x0004, 0x02},
	{0x0005, 0x02},
	{0x0006, 0xc0},
	{0x0007, 0x00},
	{0x0008, 0x11},
	{0x0009, 0x00},
	{0x000a, 0x02},
	{0x000b, 0x00},
	{0x000c, 0x04},
	{0x000d, 0x04},
	{0x000e, 0x40},
	{0x000f, 0x07},
	{0x0010, 0x8c},
	{0x0013, 0x15},
	{0x0019, 0x0c},
	{0x0041, 0x04},
	{0x0042, 0xe2},
	{0x0053, 0x60},
	{0x008d, 0x92},
	{0x0090, 0x00},
	{0x00c7, 0xe1},
	{0x001b, 0x73},
	{0x0028, 0x0d},
	{0x0029, 0x24},
	{0x002b, 0x04},
	{0x002e, 0x23},
	{0x0037, 0x03},
	{0x0043, 0x04},
	{0x0044, 0x20},
	{0x004a, 0x01},
	{0x004b, 0x20},
	{0x0055, 0x30},
	{0x006b, 0x44},
	{0x0077, 0x00},
	{0x0078, 0x20},
	{0x007c, 0xa1},
	{0x00ce, 0x7c},
	{0x00d3, 0xd4},
	{0x00e6, 0x50},
	/*gain*/
	{0x00b6, 0xc0},
	//{0x00b0, 0x60},
	{0x00b0, 0x68},
	/*isp*/
	{0x0102, 0x89},
	{0x0104, 0x01},
	{0x010e, 0x01},
	{0x010f, 0x00},
	{0x0158, 0x00},
	/*dark sun*/
	{0x0123, 0x08},
	{0x0123, 0x00},
	{0x0120, 0x01},
	{0x0121, 0x00},
	{0x0122, 0x10},
	{0x0124, 0x03},
	{0x0125, 0xff},
	{0x0126, 0x3c},
	{0x001a, 0x8c},
	{0x00c6, 0xe0},
	/*blk*/
	{0x0026, 0x30},
	{0x0142, 0x00},
	{0x0149, 0x1e},
	{0x014a, 0x0f},
	{0x014b, 0x00},
	{0x0155, 0x00},
	{0x0414, 0x78},
	{0x0415, 0x78},
	{0x0416, 0x78},
	{0x0417, 0x78},
	{0x0454, 0x78},
	{0x0455, 0x78},
	{0x0456, 0x78},
	{0x0457, 0x78},
	{0x04e0, 0x18},
	/*window*/
	{0x0192, 0x02},
	{0x0194, 0x03},
	{0x0195, 0x04},
	{0x0196, 0x38},
	{0x0197, 0x07},
	{0x0198, 0x80},
	#if SENSOR_FLIP
	{0x0017, 0x03},//h-v
	#endif
	/****DVP & MIPI****/
	{0x019a, 0x06},
	{0x007b, 0x2a},
	{0x0023, 0x2d},
	{0x0201, 0x27},
	{0x0202, 0x56},
	{0x0203, 0x8e},
	{0x0212, 0x80},
	{0x0213, 0x07},
	{0x0215, 0x12},
	{0x003e, 0x91},
	/****HDR EN****/
	{0x0027, 0x71},
	{0x0215, 0x92},
	{0x024d, 0x01},

	//pink
	{0x0183, 0x01},
	{0x0187, 0x50},
};

static struct regval_list sensor_1920x1080p30_wdr_regs[] = {
	//mclk=24mhz
	//MIPI clk:792Mbps/lane
	//pclk:158.4mhz
	//Wpclk:396MHz
	//Rpclk:198MHz
	//row time:13.3333us
	//window height:1250
	//hts=2111
	//size:1920*1080*/
	/****system****/
	{0x03fe, 0x80},
	{0x03fe, 0x80},
	{0x03fe, 0x80},
	{0x03fe, 0x00},
	{0x03f2, 0x00},
	{0x03f3, 0x00},
	{0x03f4, 0x36},
	{0x03f5, 0xc0},
	{0x03f6, 0x0B},
	{0x03f7, 0x01},
	{0x03f8, 0x63},
	{0x03f9, 0x40},
	{0x03fc, 0x8e},
	/****CISCTL & ANALOG****/
	{0x0087, 0x18},
	{0x00ee, 0x30},
	{0x00d0, 0xbf},
	{0x01a0, 0x00},
	{0x01a4, 0x40},
	{0x01a5, 0x40},
	{0x01a6, 0x40},
	{0x01af, 0x09},
	{0x0001, 0x00},
	{0x0002, 0x02},
	{0x0003, 0x04},
	{0x0004, 0x02},
	{0x0005, 0x02},
	{0x0006, 0x94},
	{0x0007, 0x00},
	{0x0008, 0x11},
	{0x0009, 0x00},
	{0x000a, 0x02},
	{0x000b, 0x00},
	{0x000c, 0x04},
	{0x000d, 0x04},
	{0x000e, 0x40},
	{0x000f, 0x07},
	{0x0010, 0x8c},
	{0x0013, 0x15},
	{0x0019, 0x0c},
	{0x0041, 0x04},
	{0x0042, 0xe2},
	{0x0053, 0x60},
	{0x008d, 0x92},
	{0x0090, 0x00},
	{0x00c7, 0xe1},
	{0x001b, 0x73},
	{0x0028, 0x0d},
	{0x0029, 0x24},
	{0x002b, 0x04},
	{0x002e, 0x23},
	{0x0037, 0x03},
	{0x0043, 0x04},
	{0x0044, 0x20},
	{0x004a, 0x01},
	{0x004b, 0x20},
	{0x0055, 0x30},
	{0x006b, 0x44},
	{0x0077, 0x00},
	{0x0078, 0x20},
	{0x007c, 0xa1},
	{0x00ce, 0x7c},
	{0x00d3, 0xd4},
	{0x00e6, 0x50},
	/*gain*/
	{0x00b6, 0xc0},
	{0x00b0, 0x78},
	/*isp*/
	{0x0102, 0x89},
	{0x0104, 0x01},
	{0x010e, 0x01},
	{0x010f, 0x00},
	{0x0158, 0x00},
	/*dark sun*/
	{0x0123, 0x08},
	{0x0123, 0x00},
	{0x0120, 0x01},
	{0x0121, 0x00},
	{0x0122, 0x10},
	{0x0124, 0x03},
	{0x0125, 0xff},
	{0x0126, 0x3c},
	{0x001a, 0x8c},
	{0x00c6, 0xe0},
	/*blk*/
	{0x0026, 0x30},
	{0x0142, 0x00},
	{0x0149, 0x1e},
	{0x014a, 0x0f},
	{0x014b, 0x00},
	{0x0155, 0x00},
	{0x0414, 0x78},
	{0x0415, 0x78},
	{0x0416, 0x78},
	{0x0417, 0x78},
	{0x0454, 0x78},
	{0x0455, 0x78},
	{0x0456, 0x78},
	{0x0457, 0x78},
	{0x04e0, 0x18},
	/*window*/
	{0x0192, 0x02},
	{0x0194, 0x03},
	{0x0195, 0x04},
	{0x0196, 0x38},
	{0x0197, 0x07},
	{0x0198, 0x80},
//	{0x0195, 0x04},
//	{0x0196, 0x40},
//	{0x0197, 0x07},
//	{0x0198, 0x80},
	#if SENSOR_FLIP
	{0x0017, 0x03},//h-v
	#endif
	/****DVP & MIPI****/
	{0x019a, 0x06},
	{0x007b, 0x2a},
	{0x0023, 0x2d},
	{0x0201, 0x27},
	{0x0202, 0x56},
	{0x0203, 0x8e},
	{0x0212, 0x80},
	{0x0213, 0x07},
	{0x0215, 0x12},
	{0x003e, 0x91},
	/****HDR EN****/
	{0x0027, 0x71},
	{0x0215, 0x92},
	{0x024d, 0x01},

	//pink
	{0x0183, 0x01},
	{0x0187, 0x50},
};

static struct regval_list sensor_1280x720p30_wdr_regs[] = {
	//mclk=24mhz
	//MIPI clk:792Mbps/lane
	//pclk:158.4mhz
	//Wpclk:396MHz
	//Rpclk:198MHz
	//row time:13.3333us
	//window height:1250
	//hts=2111
	//size:1280*720*/
	/****system****/
	{0x03fe, 0x80},
	{0x03fe, 0x80},
	{0x03fe, 0x80},
	{0x03fe, 0x00},
	{0x03f2, 0x00},
	{0x03f3, 0x00},
	{0x03f4, 0x36},
	{0x03f5, 0xc0},
	{0x03f6, 0x0B},
	{0x03f7, 0x01},
	{0x03f8, 0x63},
	{0x03f9, 0x40},
	{0x03fc, 0x8e},
	/****CISCTL & ANALOG****/
	{0x0087, 0x18},
	{0x00ee, 0x30},
	{0x00d0, 0xbf},
	{0x01a0, 0x00},
	{0x01a4, 0x40},
	{0x01a5, 0x40},
	{0x01a6, 0x40},
	{0x01af, 0x09},
	{0x0001, 0x00},
	{0x0002, 0x02},
	{0x0003, 0x04},
	{0x0004, 0x02},
	{0x0005, 0x02},
	{0x0006, 0x94},
	{0x0007, 0x00},
	{0x0008, 0x11},
	{0x0009, 0x00},
	{0x000a, 0x02},
	{0x000b, 0x00},
	{0x000c, 0x04},
	{0x000d, 0x04},
	{0x000e, 0x40},
	{0x000f, 0x07},
	{0x0010, 0x8c},
	{0x0013, 0x15},
	{0x0019, 0x0c},
	{0x0041, 0x04},
	{0x0042, 0xe2},
	{0x0053, 0x60},
	{0x008d, 0x92},
	{0x0090, 0x00},
	{0x00c7, 0xe1},
	{0x001b, 0x73},
	{0x0028, 0x0d},
	{0x0029, 0x24},
	{0x002b, 0x04},
	{0x002e, 0x23},
	{0x0037, 0x03},
	{0x0043, 0x04},
	{0x0044, 0x20},
	{0x004a, 0x01},
	{0x004b, 0x20},
	{0x0055, 0x30},
	{0x006b, 0x44},
	{0x0077, 0x00},
	{0x0078, 0x20},
	{0x007c, 0xa1},
	{0x00ce, 0x7c},
	{0x00d3, 0xd4},
	{0x00e6, 0x50},
	/*gain*/
	{0x00b6, 0xc0},
	{0x00b0, 0x78},
	/*isp*/
	{0x0102, 0x89},
	{0x0104, 0x01},
	{0x010e, 0x01},
	{0x010f, 0x00},
	{0x0158, 0x00},
	/*dark sun*/
	{0x0123, 0x08},
	{0x0123, 0x00},
	{0x0120, 0x01},
	{0x0121, 0x00},
	{0x0122, 0x10},
	{0x0124, 0x03},
	{0x0125, 0xff},
	{0x0126, 0x3c},
	{0x001a, 0x8c},
	{0x00c6, 0xe0},
	/*blk*/
	{0x0026, 0x30},
	{0x0142, 0x00},
	{0x0149, 0x1e},
	{0x014a, 0x0f},
	{0x014b, 0x00},
	{0x0155, 0x00},
	{0x0414, 0x78},
	{0x0415, 0x78},
	{0x0416, 0x78},
	{0x0417, 0x78},
	{0x0454, 0x78},
	{0x0455, 0x78},
	{0x0456, 0x78},
	{0x0457, 0x78},
	{0x04e0, 0x18},
	/*window*/
	{0x0192, 0x02},
	{0x0194, 0x03},
	{0x0195, 0x02},
	{0x0196, 0xd0},
	{0x0197, 0x05},
	{0x0198, 0x00},
//	{0x0195, 0x04},
//	{0x0196, 0x40},
//	{0x0197, 0x07},
//	{0x0198, 0x80},
	#if SENSOR_FLIP
	{0x0017, 0x03},//h-v
	#endif
	/****DVP & MIPI****/
	{0x019a, 0x06},
	{0x007b, 0x2a},
	{0x0023, 0x2d},
	{0x0201, 0x27},
	{0x0202, 0x56},
	{0x0203, 0x8e},
	{0x0212, 0x80},
	{0x0213, 0x07},
	{0x0215, 0x12},
	{0x003e, 0x91},
	/****HDR EN****/
	{0x0027, 0x71},
	{0x0215, 0x92},
	{0x024d, 0x01},

	//pink
	{0x0183, 0x01},
	{0x0187, 0x50},
};

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
 * if not support the follow function , retrun -EINVAL
 */

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
	int tmp_exp_val = exp_val / 16;
	int exp_short = 0;
	unsigned int intt_long_h, intt_long_l, intt_short_h, intt_short_l;

	if (info->isp_wdr_mode == ISP_DOL_WDR_MODE) {
		//sensor_dbg("Sensor in WDR mode, HDR_RATIO = %d\n", HDR_RATIO);
		if (tmp_exp_val < 1*HDR_RATIO) {
			 tmp_exp_val = 1*HDR_RATIO;
		}

		if (tmp_exp_val > 1100) {
			tmp_exp_val = 1100;
		}
		exp_short    = tmp_exp_val / HDR_RATIO;
		intt_long_l  = tmp_exp_val & 0xff;
		intt_long_h  = (tmp_exp_val >> 8) & 0x3f;
		intt_short_l = exp_short & 0xff;
		intt_short_h = (exp_short >> 8) & 0x3f;

		if (exp_short < 10) {
			sensor_write(sd, 0x0032, 0xfd);
		} else {
			sensor_write(sd, 0x0032, 0xf8);
		}
		sensor_write(sd, 0x0003, intt_long_h);
		sensor_write(sd, 0x0004, intt_long_l);
		sensor_dbg("sensor_set_long_exp = %d line Done!\n", intt_long);
		sensor_write(sd, 0x0001, intt_short_h);
		sensor_write(sd, 0x0002, intt_short_l);
		sensor_dbg("sensor_set_short_exp = %d line Done!\n", intt_short);
	} else {
		sensor_dbg("exp_val:%d\n", exp_val);
		sensor_write(sd, 0x0003, (tmp_exp_val >> 8) & 0xFF);
		sensor_write(sd, 0x0004, (tmp_exp_val & 0xFF));
	}

	info->exp = exp_val;
	return 0;
}

static int sensor_g_gain(struct v4l2_subdev *sd, __s32 *value)
{
	struct sensor_info *info = to_state(sd);
	*value = info->gain;
	sensor_dbg("sensor_get_gain = %d\n", info->gain);
	return 0;
}

unsigned char regValTable[25][7] = {
	//   0xb3 0xb8 0xb9 0x155 0xc2 0xcf 0xd9
	{0x00, 0x01, 0x00, 0x08, 0x10, 0x08, 0x0a},
	{0x10, 0x01, 0x0c, 0x08, 0x10, 0x08, 0x0a},
	{0x20, 0x01, 0x1b, 0x08, 0x10, 0x08, 0x0a},
	{0x30, 0x01, 0x2c, 0x08, 0x11, 0x08, 0x0c},
	{0x40, 0x01, 0x3f, 0x08, 0x12, 0x08, 0x0e},
	{0x50, 0x02, 0x16, 0x08, 0x14, 0x08, 0x12},
	{0x60, 0x02, 0x35, 0x08, 0x15, 0x08, 0x14},
	{0x70, 0x03, 0x16, 0x08, 0x17, 0x08, 0x18},
	{0x80, 0x04, 0x02, 0x08, 0x18, 0x08, 0x1a},
	{0x90, 0x04, 0x31, 0x08, 0x19, 0x08, 0x1c},
	{0xa0, 0x05, 0x32, 0x08, 0x1b, 0x08, 0x20},
	{0xb0, 0x06, 0x35, 0x08, 0x1c, 0x08, 0x22},
	{0xc0, 0x08, 0x04, 0x08, 0x1e, 0x08, 0x26},
	{0x5a, 0x09, 0x19, 0x08, 0x1c, 0x08, 0x26},
	{0x83, 0x0b, 0x0f, 0x08, 0x1c, 0x08, 0x26},
	{0x93, 0x0d, 0x12, 0x08, 0x1f, 0x08, 0x28},
	{0x84, 0x10, 0x00, 0x0b, 0x20, 0x08, 0x2a},
	{0x94, 0x12, 0x3a, 0x0b, 0x22, 0x08, 0x2e},
	{0x5d, 0x1a, 0x02, 0x0b, 0x27, 0x08, 0x38},
	{0x9b, 0x1b, 0x20, 0x0b, 0x28, 0x08, 0x3a},
	{0x8c, 0x20, 0x0f, 0x0b, 0x2a, 0x08, 0x3e},
	{0x9c, 0x26, 0x07, 0x12, 0x2d, 0x08, 0x44},
	{0xB6, 0x36, 0x21, 0x12, 0x2d, 0x08, 0x44},
	{0xad, 0x37, 0x3a, 0x12, 0x2d, 0x08, 0x44},
	{0xbd, 0x3d, 0x02, 0x12, 0x2d, 0x08, 0x44},
};
unsigned char regValTable_wdr[25][7] = {
	//   0xb3 0xb8 0xb9 0x155 0xc2 0xcf 0xd9
	{0x00, 0x01, 0x00, 0x08, 0x10, 0x08, 0x0a},
	{0x10, 0x01, 0x0c, 0x08, 0x10, 0x08, 0x0a},
	{0x20, 0x01, 0x1b, 0x08, 0x11, 0x08, 0x0c},
	{0x30, 0x01, 0x2c, 0x08, 0x12, 0x08, 0x0e},
	{0x40, 0x01, 0x3f, 0x08, 0x14, 0x08, 0x12},
	{0x50, 0x02, 0x16, 0x08, 0x15, 0x08, 0x14},
	{0x60, 0x02, 0x35, 0x08, 0x17, 0x08, 0x18},
	{0x70, 0x03, 0x16, 0x08, 0x18, 0x08, 0x1a},
	{0x80, 0x04, 0x02, 0x08, 0x1a, 0x08, 0x1e},
	{0x90, 0x04, 0x31, 0x08, 0x1b, 0x08, 0x20},
	{0xa0, 0x05, 0x32, 0x08, 0x1d, 0x08, 0x24},
	{0xb0, 0x06, 0x35, 0x08, 0x1e, 0x08, 0x26},
	{0xc0, 0x08, 0x04, 0x08, 0x20, 0x08, 0x2a},
	{0x5a, 0x09, 0x19, 0x08, 0x1e, 0x08, 0x2a},
	{0x83, 0x0b, 0x0f, 0x08, 0x1f, 0x08, 0x2a},
	{0x93, 0x0d, 0x12, 0x08, 0x21, 0x08, 0x2e},
	{0x84, 0x10, 0x00, 0x0b, 0x22, 0x08, 0x30},
	{0x94, 0x12, 0x3a, 0x0b, 0x24, 0x08, 0x34},
	{0x5d, 0x1a, 0x02, 0x0b, 0x26, 0x08, 0x34},
	{0x9b, 0x1b, 0x20, 0x0b, 0x26, 0x08, 0x34},
	{0x8c, 0x20, 0x0f, 0x0b, 0x26, 0x08, 0x34},
	{0x9c, 0x26, 0x07, 0x12, 0x26, 0x08, 0x34},
	{0xB6, 0x36, 0x21, 0x12, 0x26, 0x08, 0x34},
	{0xad, 0x37, 0x3a, 0x12, 0x26, 0x08, 0x34},
	{0xbd, 0x3d, 0x02, 0x12, 0x26, 0x08, 0x34},
};

unsigned int analog_gain_table[26] = {
	64,
	76,
	91,
	107,
	125,
	147,
	177,
	211,
	248,
	297,
	356,
	425,
	504,
	599,
	709,
	836,
	978,
	1153,
	1647,
	1651,
	1935,
	2292,
	3239,
	3959,
	4686,
	0xffffffff,
};

static int setSensorGain(struct v4l2_subdev *sd, unsigned int gain)
{
	struct sensor_info *info = to_state(sd);
	int i, total;
	unsigned int tol_dig_gain = 0;

	total = sizeof(analog_gain_table) / sizeof(unsigned int);
	for (i = 0; i < total; i++) {
		if (i ==  0) {
			if (analog_gain_table[i] > gain)
				break;
		}
		if (i < total - 1) {
			if ((analog_gain_table[i] <= gain) && (gain < analog_gain_table[i + 1]))
				break;
		}
		if (i == total - 1) {
			if (gain >= analog_gain_table[i]) {
				break;
			}
		}
	}
	if (i >= total) {
		i = total - 1;
	}
	tol_dig_gain = gain*64/analog_gain_table[i];

	if (info->isp_wdr_mode == ISP_DOL_WDR_MODE) {
		//WDR
		sensor_write(sd, 0x00b3, regValTable_wdr[i][0]);
		sensor_write(sd, 0x00b8, regValTable_wdr[i][1]);
		sensor_write(sd, 0x00b9, regValTable_wdr[i][2]);
		sensor_write(sd, 0x0155, regValTable_wdr[i][3]);
		sensor_write(sd, 0x031d, 0x2d);
		sensor_write(sd, 0x00c2, regValTable_wdr[i][4]);
		sensor_write(sd, 0x00cf, regValTable_wdr[i][5]);
		sensor_write(sd, 0x00d9, regValTable_wdr[i][6]);
		sensor_write(sd, 0x031d, 0x28);

	} else {
		sensor_write(sd, 0x00b3, regValTable[i][0]);
		sensor_write(sd, 0x00b8, regValTable[i][1]);
		sensor_write(sd, 0x00b9, regValTable[i][2]);
		sensor_write(sd, 0x0155, regValTable[i][3]);
		sensor_write(sd, 0x031d, 0x2d);
		sensor_write(sd, 0x00c2, regValTable[i][4]);
		sensor_write(sd, 0x00cf, regValTable[i][5]);
		sensor_write(sd, 0x00d9, regValTable[i][6]);
		sensor_write(sd, 0x031d, 0x28);
	}
	sensor_write(sd, 0x00B1, (tol_dig_gain >> 6));
	sensor_write(sd, 0x00B2, ((tol_dig_gain & 0x3f) << 2));

	return 0;
}

static int sensor_s_gain(struct v4l2_subdev *sd, int gain_val)
{
	struct sensor_info *info = to_state(sd);

	if (gain_val == info->gain) {
		return 0;
	}

	sensor_dbg("gain_val:%d\n", gain_val);
	setSensorGain(sd, gain_val * 4);
	info->gain = gain_val;

	return 0;
}

static int sensor_s_exp_gain(struct v4l2_subdev *sd,
				 struct sensor_exp_gain *exp_gain)
{
	int exp_val, gain_val;
	struct sensor_info *info = to_state(sd);

	exp_val = exp_gain->exp_val;
	gain_val = exp_gain->gain_val;

	if (gain_val < (1 * 16)) {
		gain_val = 16;
	}

	if (exp_val > 0xfffff)
		exp_val = 0xfffff;

	sensor_s_exp(sd, exp_val);
	sensor_s_gain(sd, gain_val);

	info->exp = exp_val;
	info->gain = gain_val;
	return 0;
}

static int sensor_flip_status;
static int sensor_s_vflip(struct v4l2_subdev *sd, int enable)
{
	/*data_type get_value;
	data_type set_value;

	if (!(enable == 0 || enable == 1))
		return -1;

	sensor_read(sd, 0x17, &get_value);
	sensor_dbg("ready to vflip, regs_data = 0x%x\n", get_value);

	if (enable) {
		set_value = get_value | 0x02;
		sensor_flip_status |= 0x02;
	} else {
		set_value = get_value & 0xFD;
		sensor_flip_status &= 0xFD;
	}
	sensor_write(sd, 0x17, set_value);
	usleep_range(80000, 100000);
	sensor_read(sd, 0x17, &get_value);
	sensor_dbg("after vflip, regs_data = 0x%x, sensor_flip_status = %d\n",
				get_value, sensor_flip_status);
*/
	return 0;
}

static int sensor_s_hflip(struct v4l2_subdev *sd, int enable)
{
	/*data_type get_value;
	data_type set_value;

	if (!(enable == 0 || enable == 1))
		return -1;

	sensor_read(sd, 0x17, &get_value);
	sensor_dbg("ready to hflip, regs_data = 0x%x\n", get_value);

	if (enable) {
		set_value = get_value | 0x01;
		sensor_flip_status |= 0x01;
	} else {
		set_value = get_value & 0xFE;
		sensor_flip_status &= 0xFE;
	}
	sensor_write(sd, 0x17, set_value);
	usleep_range(80000, 100000);
	sensor_read(sd, 0x17, &get_value);
	sensor_dbg("after hflip, regs_data = 0x%x, sensor_flip_status = %d\n",
				get_value, sensor_flip_status);
*/
	return 0;
}

static int sensor_get_fmt_mbus_core(struct v4l2_subdev *sd, int *code)
{
/*
	struct sensor_info *info = to_state(sd);
	data_type get_value = 0, check_value = 0;

	sensor_read(sd, 0x17, &get_value);
	check_value = get_value & 0x03;
	check_value = sensor_flip_status & 0x3;
	sensor_dbg("0x17 = 0x%x, check_value = 0x%x\n", get_value, check_value);

	switch (check_value) {
	case 0x00:
		sensor_dbg("RGGB\n");
		*code = MEDIA_BUS_FMT_SRGGB10_1X10;
		break;
	case 0x01:
		sensor_dbg("GRBG\n");
		*code = MEDIA_BUS_FMT_SGRBG10_1X10;
		break;
	case 0x02:
		sensor_dbg("GBRG\n");
		*code = MEDIA_BUS_FMT_SGBRG10_1X10;
		break;
	case 0x03:
		sensor_dbg("BGGR\n");
		*code = MEDIA_BUS_FMT_SBGGR10_1X10;
		break;
	default:
		 *code = info->fmt->mbus_code;
	}
*/
	*code = MEDIA_BUS_FMT_SRGGB10_1X10; // gc2053 support change the rgb format by itself

	return 0;
}


/*
* Stuff that knows about the sensor.
*/
static int sensor_power(struct v4l2_subdev *sd, int on)
{
	switch (on) {
	case STBY_ON:
		sensor_dbg("STBY_ON!\n");
		cci_lock(sd);
		vin_gpio_write(sd, PWDN, CSI_GPIO_HIGH);
		cci_unlock(sd);
		break;
	case STBY_OFF:
		sensor_dbg("STBY_OFF!\n");
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
		sensor_dbg("PWR_ON!\n");
		cci_lock(sd);
		vin_set_mclk(sd, ON);
		usleep_range(1000, 1200);
		vin_set_mclk_freq(sd, MCLK);
		usleep_range(1000, 1200);
		vin_gpio_set_status(sd, PWDN, 1);
		vin_gpio_set_status(sd, RESET, 1);
		vin_gpio_write(sd, PWDN, CSI_GPIO_LOW);
		vin_gpio_write(sd, RESET, CSI_GPIO_LOW);
		usleep_range(1000, 1200);
		vin_set_pmu_channel(sd, IOVDD, ON);
		usleep_range(1000, 1200);
		vin_set_pmu_channel(sd, DVDD, ON);
		usleep_range(1000, 1200);
		vin_set_pmu_channel(sd, AVDD, ON);
		usleep_range(1000, 1200);
		vin_gpio_write(sd, PWDN, CSI_GPIO_HIGH);
		usleep_range(10000, 12000);
		vin_gpio_write(sd, RESET, CSI_GPIO_HIGH);
		usleep_range(10000, 12000);
		cci_unlock(sd);
		break;
	case PWR_OFF:
		sensor_dbg("PWR_OFF!do nothing\n");
		cci_lock(sd);
		vin_set_mclk(sd, OFF);
		vin_set_pmu_channel(sd, AVDD, OFF);
		vin_set_pmu_channel(sd, DVDD, OFF);
		vin_set_pmu_channel(sd, IOVDD, OFF);
		usleep_range(10000, 12000);
		vin_gpio_write(sd, PWDN, CSI_GPIO_LOW);
		vin_gpio_write(sd, RESET, CSI_GPIO_LOW);
		cci_unlock(sd);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int sensor_reset(struct v4l2_subdev *sd, u32 val)
{

	sensor_dbg("%s: val=%d\n", __func__);
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
	int eRet;
	int times_out = 3;
	do {
		eRet = sensor_read(sd, ID_REG_HIGH, &rdval);
		sensor_dbg("eRet:%d, ID_VAL_HIGH:0x%x, times_out:%d\n", eRet, rdval, times_out);
		usleep_range(200000, 220000);
		times_out--;
	} while (eRet < 0  &&  times_out > 0);

	sensor_read(sd, ID_REG_HIGH, &rdval);
	sensor_dbg("ID_VAL_HIGH = %2x, Done!\n", rdval);
	/* if (rdval != ID_VAL_HIGH)
		return -ENODEV; */

	sensor_read(sd, ID_REG_LOW, &rdval);
	sensor_dbg("ID_VAL_LOW = %2x, Done!\n", rdval);
	/* if (rdval != ID_VAL_LOW)
		return -ENODEV; */

	sensor_dbg("Done!\n");
	return 0;
}

static int sensor_init(struct v4l2_subdev *sd, u32 val)
{
	int ret;
	struct sensor_info *info = to_state(sd);

	sensor_dbg("sensor_init\n");

	/*Make sure it is a target sensor */
	ret = sensor_detect(sd);
	if (ret) {
		sensor_err("chip found is not an target chip.\n");
		return ret;
	}

	info->focus_status = 0;
	info->low_speed    = 0;
	info->width        = 1920;
	info->height       = 1080;
	info->hflip        = 0;
	info->vflip        = 0;
	info->gain         = 0;
	info->exp          = 0;

	info->tpf.numerator      = 1;
	info->tpf.denominator    = 30;	/* 30fps */
	info->preview_first_flag = 1;
	return 0;
}

static long sensor_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	int ret = 0;
	struct sensor_info *info = to_state(sd);

	switch (cmd) {
	case GET_CURRENT_WIN_CFG:
		if (info->current_wins != NULL) {
			memcpy(arg, info->current_wins,
				sizeof(struct sensor_win_size));
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
//	case VIDIOC_VIN_SENSOR_SET_FPS:
//		ret = sensor_s_fps(sd, (struct sensor_fps *)arg);
//		break;
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
		.desc      = "Raw RGB Bayer",
		.mbus_code = MEDIA_BUS_FMT_SRGGB10_1X10, /*.mbus_code = MEDIA_BUS_FMT_SBGGR10_1X10, */
		.regs      = sensor_fmt_raw,
		.regs_size = ARRAY_SIZE(sensor_fmt_raw),
		.bpp       = 1
	},
};
#define N_FMTS ARRAY_SIZE(sensor_formats)

/*
 * Then there is the issue of window sizes.  Try to capture the info here.
 */

static struct sensor_win_size sensor_win_sizes[] = {

	{
		.width      = 1920,
		.height     = 1080,
		.hoffset    = 0,
		.voffset    = 0,
		.hts        = 2800,
		.vts        = 1125,
		.pclk       = 94500000,
		.mipi_bps   = 384 * 1000 * 1000,
		.fps_fixed  = 30,
		.bin_factor = 1,
		.intg_min   = 1 << 4,
		.intg_max   = 1125 << 4,
		.gain_min   = 1 << 4,
		.gain_max   = 110 << 4,
		.regs       = sensor_1080p30_regs,
		.regs_size  = ARRAY_SIZE(sensor_1080p30_regs),
		.set_size   = NULL,
		.vipp_w     = 1920,
		.vipp_h     = 1080,
		.vipp_hoff  = 4,
		.vipp_voff  = 3,
	},

	/*{
		.width      = 1920,
		.height     = 1080,
		.hoffset    = 0,
		.voffset    = 0,
		.hts        = 2816,
		.vts        = 1250,
		.pclk       = 176 * 1000 * 1000,
		.mipi_bps   = 704 * 1000 * 1000,
		.fps_fixed  = 25,
		.bin_factor = 1,
		.if_mode    = MIPI_VC_WDR_MODE,
		.wdr_mode   = ISP_DOL_WDR_MODE,
		.intg_min   = 1 << 4,
		.intg_max   = 1250 << 4,
		.gain_min   = 1 << 4,
		.gain_max   = 48 << 4,
		.regs       = sensor_1920x1080p25_wdr_regs,
		.regs_size  = ARRAY_SIZE(sensor_1920x1080p25_wdr_regs),
		.set_size   = NULL,
		.top_clk    = 300*1000*1000,
		.isp_clk    = 351*1000*1000,
	},
     */
	{
		.width      = 1920,
		.height     = 1080,
		.hoffset    = 0,
		.voffset    = 0,
		.hts        = 2111,
		.vts        = 1250,
		.pclk       = 158 * 1000 * 1000,
		.mipi_bps   = 792 * 1000 * 1000,
		.fps_fixed  = 30,
		.bin_factor = 1,
		.if_mode    = MIPI_VC_WDR_MODE,
		.wdr_mode   = ISP_DOL_WDR_MODE,
		.intg_min   = 1 << 4,
		.intg_max   = 1250 << 4,
		.gain_min   = 1 << 4,
		.gain_max   = 48 << 4,
		.regs       = sensor_1920x1080p30_wdr_regs,
		.regs_size  = ARRAY_SIZE(sensor_1920x1080p30_wdr_regs),
		.set_size   = NULL,
		.top_clk    = 300*1000*1000,
		.isp_clk    = 351*1000*1000,
	},

	{
		.width	    = 1280,
		.height     = 720,
		.hoffset    = 0,
		.voffset    = 0,
		.hts	    = 2111,
		.vts	    = 1250,
		.pclk	    = 158 * 1000 * 1000,
		.mipi_bps   = 792 * 1000 * 1000,
		.fps_fixed  = 30,
		.bin_factor = 1,
		.if_mode    = MIPI_VC_WDR_MODE,
		.wdr_mode   = ISP_DOL_WDR_MODE,
		.intg_min   = 1 << 4,
		.intg_max   = 1250 << 4,
		.gain_min   = 1 << 4,
		.gain_max   = 48 << 4,
		.regs	    = sensor_1280x720p30_wdr_regs,
		.regs_size  = ARRAY_SIZE(sensor_1280x720p30_wdr_regs),
		.set_size   = NULL,
		.top_clk    = 300*1000*1000,
		.isp_clk    = 351*1000*1000,
	},
};

#define N_WIN_SIZES (ARRAY_SIZE(sensor_win_sizes))

static int sensor_g_mbus_config(struct v4l2_subdev *sd,
				struct v4l2_mbus_config *cfg)
{
	struct sensor_info *info = to_state(sd);
	cfg->type  = V4L2_MBUS_CSI2;

	if (info->isp_wdr_mode == ISP_DOL_WDR_MODE)
		cfg->flags = 0 | V4L2_MBUS_CSI2_2_LANE | V4L2_MBUS_CSI2_CHANNEL_0 | V4L2_MBUS_CSI2_CHANNEL_1;
	else
		cfg->flags = 0 | V4L2_MBUS_CSI2_2_LANE | V4L2_MBUS_CSI2_CHANNEL_0;

	return 0;
}

static int sensor_g_ctrl(struct v4l2_ctrl *ctrl)
{
	struct sensor_info *info =
			container_of(ctrl->handler, struct sensor_info, handler);
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
	struct sensor_info *info =
			container_of(ctrl->handler, struct sensor_info, handler);
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

	ret = sensor_write_array(sd, sensor_default_regs,
				 ARRAY_SIZE(sensor_default_regs));
	if (ret < 0) {
		sensor_err("write sensor_default_regs error\n");
		return ret;
	}

	sensor_write_array(sd, sensor_fmt->regs, sensor_fmt->regs_size);

	if (wsize->regs)
		sensor_write_array(sd, wsize->regs, wsize->regs_size);

	if (wsize->set_size)
		wsize->set_size(sd);

	info->width = wsize->width;
	info->height = wsize->height;

	sensor_dbg("s_fmt set width = %d, height = %d\n", wsize->width,
			 wsize->height);

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
static struct cci_driver cci_drv[] = {
	{
		.name = SENSOR_NAME,
		//.addr_width = CCI_BITS_16,
		.addr_width = CCI_BITS_16,
		.data_width = CCI_BITS_8,
	}, {
		.name = SENSOR_NAME_2,
		//.addr_width = CCI_BITS_16,
		.addr_width = CCI_BITS_16,
		.data_width = CCI_BITS_8,
	}
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

	// v4l2_ctrl_new_std(handler, ops, V4L2_CID_HFLIP, 0, 1, 1, 0);
	// v4l2_ctrl_new_std(handler, ops, V4L2_CID_VFLIP, 0, 1, 1, 0);

	if (ctrl != NULL)
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE;

	if (handler->error) {
		ret = handler->error;
		v4l2_ctrl_handler_free(handler);
	}

	sd->ctrl_handler = handler;

	return ret;
}
static int sensor_dev_id;

static int sensor_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct v4l2_subdev *sd;
	struct sensor_info *info;
	int i;

	info = kzalloc(sizeof(struct sensor_info), GFP_KERNEL);
	if (info == NULL)
		return -ENOMEM;
	sd = &info->sd;

	if (client) {
		for (i = 0; i < SENSOR_NUM; i++) {
			if (!strcmp(cci_drv[i].name, client->name))
				break;
		}
		cci_dev_probe_helper(sd, client, &sensor_ops, &cci_drv[i]);
	} else {
		cci_dev_probe_helper(sd, client, &sensor_ops, &cci_drv[sensor_dev_id++]);
	}

	sensor_init_controls(sd, &sensor_ctrl_ops);

	mutex_init(&info->lock);

	info->fmt = &sensor_formats[0];
	info->fmt_pt = &sensor_formats[0];
	info->win_pt = &sensor_win_sizes[0];
	info->fmt_num = N_FMTS;
	info->win_size_num = N_WIN_SIZES;
	info->sensor_field = V4L2_FIELD_NONE;
	// use CMB_PHYA_OFFSET2  also ok
	info->combo_mode = CMB_TERMINAL_RES | CMB_PHYA_OFFSET3 | MIPI_NORMAL_MODE;
	//info->combo_mode = CMB_PHYA_OFFSET2 | MIPI_NORMAL_MODE;
	info->stream_seq = MIPI_BEFORE_SENSOR;
	info->af_first_flag = 1;
	info->exp = 0;
	info->gain = 0;

	return 0;
}

static int sensor_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd;
	int i;

	if (client) {
		for (i = 0; i < SENSOR_NUM; i++) {
			if (!strcmp(cci_drv[i].name, client->name))
				break;
		}
		sd = cci_dev_remove_helper(client, &cci_drv[i]);
	} else {
		sd = cci_dev_remove_helper(client, &cci_drv[sensor_dev_id++]);
	}

	kfree(to_state(sd));
	return 0;
}

static const struct i2c_device_id sensor_id[] = {
	{SENSOR_NAME, 0},
	{}
};

static const struct i2c_device_id sensor_id_2[] = {
	{SENSOR_NAME_2, 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, sensor_id);
MODULE_DEVICE_TABLE(i2c, sensor_id_2);

static struct i2c_driver sensor_driver[] = {
	{
		.driver = {
			   .owner = THIS_MODULE,
			   .name = SENSOR_NAME,
			   },
		.probe = sensor_probe,
		.remove = sensor_remove,
		.id_table = sensor_id,
	}, {
		.driver = {
			   .owner = THIS_MODULE,
			   .name = SENSOR_NAME_2,
			   },
		.probe = sensor_probe,
		.remove = sensor_remove,
		.id_table = sensor_id_2,
	},
};
static __init int init_sensor(void)
{
	int i, ret = 0;

	sensor_dev_id = 0;

	for (i = 0; i < SENSOR_NUM; i++)
		ret = cci_dev_init_helper(&sensor_driver[i]);

	return ret;
}

static __exit void exit_sensor(void)
{
	int i;

	sensor_dev_id = 0;

	for (i = 0; i < SENSOR_NUM; i++)
		cci_dev_exit_helper(&sensor_driver[i]);
}

module_init(init_sensor);
module_exit(exit_sensor);
