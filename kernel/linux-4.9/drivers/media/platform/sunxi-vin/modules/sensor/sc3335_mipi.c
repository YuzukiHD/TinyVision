/*
 * A V4L2 driver for sc3335 Raw cameras.
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

MODULE_AUTHOR("myf");
MODULE_DESCRIPTION("A low-level driver for SC3335 sensors");
MODULE_LICENSE("GPL");

#define MCLK              (27 * 1000 * 1000)
#define V4L2_IDENT_SENSOR 0xcc1a

/*
 * Our nominal (default) frame rate.
 */

#define SENSOR_FRAME_RATE 20

/*
 * The SC3335 i2c address
 */
#define I2C_ADDR 0x60

#define SENSOR_NUM 0x2
#define SENSOR_NAME "sc3335_mipi"
#define SENSOR_NAME_2 "sc3335_mipi_2"


#define SENSOR_HOR_VER_CFG0_REG1  (1)   // Sensor HOR and VER Config by write_hw g_productinfo_s

static struct regval_list sensor_default_regs[] = {
	{0x0103, 0x01},
	{0x0100, 0x00},
	{0x36e9, 0x80},
	{0x36f9, 0x80},
	{0x301f, 0x19},
	{0x3200, 0x00},
	{0x3201, 0xc0},
	{0x3202, 0x00},
	{0x3203, 0x6c},
	{0x3204, 0x08},
	{0x3205, 0x47},
	{0x3206, 0x04},
	{0x3207, 0xab},
	{0x3208, 0x07},
	{0x3209, 0x80},
	{0x320a, 0x04},
	{0x320b, 0x38},
	{0x320c, 0x04},
	{0x320d, 0x4c},
	{0x320e, 0x05}, //0A
	{0x320f, 0x46}, //FC
	{0x3210, 0x00},
	{0x3211, 0x04},
	{0x3212, 0x00},
	{0x3213, 0x04},
	{0x3253, 0x04},
	{0x3301, 0x04},
	{0x3302, 0x10},
	{0x3304, 0x38},
	{0x3306, 0x3c},
	{0x3308, 0x09},
	{0x3309, 0x40},
	{0x330b, 0xa8},
	{0x330d, 0x12},
	{0x330e, 0x29},
	{0x3310, 0x06},
	{0x3314, 0x96},
	{0x331e, 0x31},
	{0x331f, 0x39},
	{0x3320, 0x09},
	{0x3333, 0x10},
	{0x334c, 0x01},
	{0x3364, 0x17},
	{0x3367, 0x01},
	{0x3390, 0x04},
	{0x3391, 0x08},
	{0x3392, 0x38},
	{0x3393, 0x05},
	{0x3394, 0x09},
	{0x3395, 0x16},
	{0x33ac, 0x0c},
	{0x33ae, 0x1c},
	{0x3622, 0x16},
	{0x3637, 0x22},
	{0x363a, 0x1f},
	{0x363c, 0x05},
	{0x3670, 0x0e},
	{0x3674, 0xb0},
	{0x3675, 0x88},
	{0x3676, 0x68},
	{0x3677, 0x84},
	{0x3678, 0x85},
	{0x3679, 0x86},
	{0x367c, 0x18},
	{0x367d, 0x38},
	{0x367e, 0x08},
	{0x367f, 0x18},
	{0x3690, 0x43},
	{0x3691, 0x43},
	{0x3692, 0x44},
	{0x369c, 0x18},
	{0x369d, 0x38},
	{0x36ea, 0x35},
	{0x36eb, 0x0d},
	{0x36ec, 0x1c},
	{0x36ed, 0x24},
	{0x36fa, 0x35},
	{0x36fb, 0x00},
	{0x36fc, 0x10},
	{0x36fd, 0x24},
	{0x3908, 0x82},
	{0x391f, 0x18},
	{0x3e01, 0xa8},
	{0x3e02, 0x20},
	{0x3f00, 0x4c},
	{0x3f09, 0x48},
	{0x4505, 0x0a},
	{0x4509, 0x20},
	{0x4800, 0x44},
	{0x4819, 0x05},
	{0x481b, 0x03},
	{0x481d, 0x0a},
	{0x481f, 0x02},
	{0x4821, 0x08},
	{0x4823, 0x03},
	{0x4825, 0x02},
	{0x4827, 0x03},
	{0x4829, 0x04},
	{0x5799, 0x00},
	{0x59e0, 0x60},
	{0x59e1, 0x08},
	{0x59e2, 0x3f},
	{0x59e3, 0x18},
	{0x59e4, 0x18},
	{0x59e5, 0x3f},
	{0x59e6, 0x06},
	{0x59e7, 0x02},
	{0x59e8, 0x38},
	{0x59e9, 0x10},
	{0x59ea, 0x0c},
	{0x59eb, 0x10},
	{0x59ec, 0x04},
	{0x59ed, 0x02},
	{0x36e9, 0x20},
	{0x36f9, 0x20},
	{0x0100, 0x01},
};

#if 0
static struct regval_list sensor_1080p25_regs[] = {
//window_size=1920*1080
//pixel_line_total=2200,line_frame_total=1350
	{0x0103, 0x01},
	{0x0100, 0x00},
	{0x36e9, 0x80},
	{0x36f9, 0x80},
	{0x301f, 0x19},
	{0x3200, 0x00},
	{0x3201, 0xc0},
	{0x3202, 0x00},
	{0x3203, 0x6c},
	{0x3204, 0x08},
	{0x3205, 0x47},
	{0x3206, 0x04},
	{0x3207, 0xab},
	{0x3208, 0x07},
	{0x3209, 0x80},
	{0x320a, 0x04},
	{0x320b, 0x38},
	{0x320c, 0x04},
	{0x320d, 0x4c},
	{0x320e, 0x05},//0A
	{0x320f, 0x46},//FC
	{0x3210, 0x00},
	{0x3211, 0x04},
	{0x3212, 0x00},
	{0x3213, 0x04},
	{0x3253, 0x04},
	{0x3301, 0x04},
	{0x3302, 0x10},
	{0x3304, 0x38},
	{0x3306, 0x3c},
	{0x3308, 0x09},
	{0x3309, 0x40},
	{0x330b, 0xa8},
	{0x330d, 0x12},
	{0x330e, 0x29},
	{0x3310, 0x06},
	{0x3314, 0x96},
	{0x331e, 0x31},
	{0x331f, 0x39},
	{0x3320, 0x09},
	{0x3333, 0x10},
	{0x334c, 0x01},
	{0x3364, 0x17},
	{0x3367, 0x01},
	{0x3390, 0x04},
	{0x3391, 0x08},
	{0x3392, 0x38},
	{0x3393, 0x05},
	{0x3394, 0x09},
	{0x3395, 0x16},
	{0x33ac, 0x0c},
	{0x33ae, 0x1c},
	{0x3622, 0x16},
	{0x3637, 0x22},
	{0x363a, 0x1f},
	{0x363c, 0x05},
	{0x3670, 0x0e},
	{0x3674, 0xb0},
	{0x3675, 0x88},
	{0x3676, 0x68},
	{0x3677, 0x84},
	{0x3678, 0x85},
	{0x3679, 0x86},
	{0x367c, 0x18},
	{0x367d, 0x38},
	{0x367e, 0x08},
	{0x367f, 0x18},
	{0x3690, 0x43},
	{0x3691, 0x43},
	{0x3692, 0x44},
	{0x369c, 0x18},
	{0x369d, 0x38},
	{0x36ea, 0x35},
	{0x36eb, 0x0d},
	{0x36ec, 0x1c},
	{0x36ed, 0x24},
	{0x36fa, 0x35},
	{0x36fb, 0x00},
	{0x36fc, 0x10},
	{0x36fd, 0x24},
	{0x3908, 0x82},
	{0x391f, 0x18},
	{0x3e01, 0xa8},
	{0x3e02, 0x20},
	{0x3f00, 0x4c},
	{0x3f09, 0x48},
	{0x4505, 0x0a},
	{0x4509, 0x20},
	{0x4800, 0x44},
	{0x4819, 0x05},
	{0x481b, 0x03},
	{0x481d, 0x0a},
	{0x481f, 0x02},
	{0x4821, 0x08},
	{0x4823, 0x03},
	{0x4825, 0x02},
	{0x4827, 0x03},
	{0x4829, 0x04},
	{0x5799, 0x00},
	{0x59e0, 0x60},
	{0x59e1, 0x08},
	{0x59e2, 0x3f},
	{0x59e3, 0x18},
	{0x59e4, 0x18},
	{0x59e5, 0x3f},
	{0x59e6, 0x06},
	{0x59e7, 0x02},
	{0x59e8, 0x38},
	{0x59e9, 0x10},
	{0x59ea, 0x0c},
	{0x59eb, 0x10},
	{0x59ec, 0x04},
	{0x59ed, 0x02},
	{0x36e9, 0x20},
	{0x36f9, 0x20},
	{0x0100, 0x01},
};
#endif

#if 0
static struct regval_list sensor_1080p30_regs[] = {
//window_size=1920*1080 mipi@2lane
//row_time=35.55us,frame_rate=30fps
	{0x0103, 0x01},
	{0x0100, 0x00},
	{0x36e9, 0x80},
	{0x36f9, 0x80},
	{0x301f, 0x18},
	{0x3200, 0x00},
	{0x3201, 0xc0},
	{0x3202, 0x00},
	{0x3203, 0x6c},
	{0x3204, 0x08},
	{0x3205, 0x47},
	{0x3206, 0x04},
	{0x3207, 0xab},
	{0x3208, 0x07},
	{0x3209, 0x80},
	{0x320a, 0x04},
	{0x320b, 0x38},
	{0x320c, 0x04},
	{0x320d, 0x4c},
	{0x320e, 0x04},//A
	{0x320f, 0x65},//FC
	{0x3210, 0x00},
	{0x3211, 0x04},
	{0x3212, 0x00},
	{0x3213, 0x04},
	{0x3253, 0x04},
	{0x3301, 0x04},
	{0x3302, 0x10},
	{0x3304, 0x38},
	{0x3306, 0x3c},
	{0x3308, 0x09},
	{0x3309, 0x40},
	{0x330b, 0xa8},
	{0x330d, 0x12},
	{0x330e, 0x29},
	{0x3310, 0x06},
	{0x3314, 0x96},
	{0x331e, 0x31},
	{0x331f, 0x39},
	{0x3320, 0x09},
	{0x3333, 0x10},
	{0x334c, 0x01},
	{0x3364, 0x17},
	{0x3367, 0x01},
	{0x3390, 0x04},
	{0x3391, 0x08},
	{0x3392, 0x38},
	{0x3393, 0x05},
	{0x3394, 0x09},
	{0x3395, 0x16},
	{0x33ac, 0x0c},
	{0x33ae, 0x1c},
	{0x3622, 0x16},
	{0x3637, 0x22},
	{0x363a, 0x1f},
	{0x363c, 0x05},
	{0x3670, 0x0e},
	{0x3674, 0xb0},
	{0x3675, 0x88},
	{0x3676, 0x68},
	{0x3677, 0x84},
	{0x3678, 0x85},
	{0x3679, 0x86},
	{0x367c, 0x18},
	{0x367d, 0x38},
	{0x367e, 0x08},
	{0x367f, 0x18},
	{0x3690, 0x43},
	{0x3691, 0x43},
	{0x3692, 0x44},
	{0x369c, 0x18},
	{0x369d, 0x38},
	{0x36ea, 0x35},
	{0x36eb, 0x0d},
	{0x36ec, 0x1c},
	{0x36ed, 0x24},
	{0x36fa, 0x35},
	{0x36fb, 0x00},
	{0x36fc, 0x10},
	{0x36fd, 0x24},
	{0x3908, 0x82},
	{0x391f, 0x18},
	{0x3e01, 0x8c},
	{0x3e02, 0x00},
	{0x3f00, 0x4c},
	{0x3f09, 0x48},
	{0x4505, 0x0a},
	{0x4509, 0x20},
	{0x4800, 0x44},
	{0x4819, 0x05},
	{0x481b, 0x03},
	{0x481d, 0x0a},
	{0x481f, 0x02},
	{0x4821, 0x08},
	{0x4823, 0x03},
	{0x4825, 0x02},
	{0x4827, 0x03},
	{0x4829, 0x04},
	{0x5799, 0x00},
	{0x59e0, 0x60},
	{0x59e1, 0x08},
	{0x59e2, 0x3f},
	{0x59e3, 0x18},
	{0x59e4, 0x18},
	{0x59e5, 0x3f},
	{0x59e6, 0x06},
	{0x59e7, 0x02},
	{0x59e8, 0x38},
	{0x59e9, 0x10},
	{0x59ea, 0x0c},
	{0x59eb, 0x10},
	{0x59ec, 0x04},
	{0x59ed, 0x02},
	{0x36e9, 0x20},
	{0x36f9, 0x20},
	{0x0100, 0x01},
};
#endif

static struct regval_list sensor_300m_25fps_regs[] = {
//window_size=2304*1296 mipi@2lane
//row_time=35.55us,frame_rate=25fps
	{0x0103, 0x01},
	{0x0100, 0x00},
	{0x36e9, 0x80},
	{0x36f9, 0x80},
	{0x301f, 0x01},
	{0x320e, 0x06},//06
	{0x320f, 0x54},//54
	{0x3253, 0x04},
	{0x3301, 0x04},
	{0x3302, 0x10},
	{0x3304, 0x40},
	{0x3306, 0x40},
	{0x3309, 0x50},
	{0x330b, 0xb6},
	{0x330e, 0x29},
	{0x3310, 0x06},
	{0x3314, 0x96},
	{0x331e, 0x39},
	{0x331f, 0x49},
	{0x3320, 0x09},
	{0x3333, 0x10},
	{0x334c, 0x01},
	{0x3364, 0x17},
	{0x3367, 0x01},
	{0x3390, 0x04},
	{0x3391, 0x08},
	{0x3392, 0x38},
	{0x3393, 0x05},
	{0x3394, 0x09},
	{0x3395, 0x16},
	{0x33ac, 0x0c},
	{0x33ae, 0x1c},
	{0x3622, 0x16},
	{0x3637, 0x22},
	{0x363a, 0x1f},
	{0x363c, 0x05},
	{0x3670, 0x0e},
	{0x3674, 0xb0},
	{0x3675, 0x88},
	{0x3676, 0x68},
	{0x3677, 0x84},
	{0x3678, 0x85},
	{0x3679, 0x86},
	{0x367c, 0x18},
	{0x367d, 0x38},
	{0x367e, 0x08},
	{0x367f, 0x18},
	{0x3690, 0x43},
	{0x3691, 0x43},
	{0x3692, 0x44},
	{0x369c, 0x18},
	{0x369d, 0x38},
	{0x36ea, 0x3b},
	{0x36eb, 0x0d},
	{0x36ec, 0x1c},
	{0x36ed, 0x24},
	{0x36fa, 0x3b},
	{0x36fb, 0x00},
	{0x36fc, 0x10},
	{0x36fd, 0x24},
	{0x3908, 0x82},
	{0x391f, 0x18},
	{0x3e01, 0xa8},
	{0x3e02, 0x20},
	{0x3f09, 0x48},
	{0x4505, 0x08},
	{0x4509, 0x20},
	{0x4800, 0x44},
	{0x5799, 0x00},
	{0x59e0, 0x60},
	{0x59e1, 0x08},
	{0x59e2, 0x3f},
	{0x59e3, 0x18},
	{0x59e4, 0x18},
	{0x59e5, 0x3f},
	{0x59e6, 0x06},
	{0x59e7, 0x02},
	{0x59e8, 0x38},
	{0x59e9, 0x10},
	{0x59ea, 0x0c},
	{0x59eb, 0x10},
	{0x59ec, 0x04},
	{0x59ed, 0x02},
	{0x36e9, 0x23},
	{0x36f9, 0x23},
	{0x0100, 0x01},
};


static struct regval_list sensor_300m_30fps_regs[] = {
//window_size=2304*1296 mipi@2lane
//row_time=35.55us,frame_rate=30fps
	{0x0103, 0x01},
	{0x0100, 0x00},
	{0x36e9, 0x80},
	{0x36f9, 0x80},
	{0x301f, 0x01},
	{0x320e, 0x05},//06
	{0x320f, 0x46},//54{0x3253,0x04},
	{0x3301, 0x04},
	{0x3302, 0x10},
	{0x3304, 0x40},
	{0x3306, 0x40},
	{0x3309, 0x50},
	{0x330b, 0xb6},
	{0x330e, 0x29},
	{0x3310, 0x06},
	{0x3314, 0x96},
	{0x331e, 0x39},
	{0x331f, 0x49},
	{0x3320, 0x09},
	{0x3333, 0x10},
	{0x334c, 0x01},
	{0x3364, 0x17},
	{0x3367, 0x01},
	{0x3390, 0x04},
	{0x3391, 0x08},
	{0x3392, 0x38},
	{0x3393, 0x05},
	{0x3394, 0x09},
	{0x3395, 0x16},
	{0x33ac, 0x0c},
	{0x33ae, 0x1c},
	{0x3622, 0x16},
	{0x3637, 0x22},
	{0x363a, 0x1f},
	{0x363c, 0x05},
	{0x3670, 0x0e},
	{0x3674, 0xb0},
	{0x3675, 0x88},
	{0x3676, 0x68},
	{0x3677, 0x84},
	{0x3678, 0x85},
	{0x3679, 0x86},
	{0x367c, 0x18},
	{0x367d, 0x38},
	{0x367e, 0x08},
	{0x367f, 0x18},
	{0x3690, 0x43},
	{0x3691, 0x43},
	{0x3692, 0x44},
	{0x369c, 0x18},
	{0x369d, 0x38},
	{0x36ea, 0x3b},
	{0x36eb, 0x0d},
	{0x36ec, 0x1c},
	{0x36ed, 0x24},
	{0x36fa, 0x3b},
	{0x36fb, 0x00},
	{0x36fc, 0x10},
	{0x36fd, 0x24},
	{0x3908, 0x82},
	{0x391f, 0x18},
	{0x3e01, 0xa8},
	{0x3e02, 0x20},
	{0x3f09, 0x48},
	{0x4505, 0x08},
	{0x4509, 0x20},
	{0x4800, 0x44},
	{0x5799, 0x00},
	{0x59e0, 0x60},
	{0x59e1, 0x08},
	{0x59e2, 0x3f},
	{0x59e3, 0x18},
	{0x59e4, 0x18},
	{0x59e5, 0x3f},
	{0x59e6, 0x06},
	{0x59e7, 0x02},
	{0x59e8, 0x38},
	{0x59e9, 0x10},
	{0x59ea, 0x0c},
	{0x59eb, 0x10},
	{0x59ec, 0x04},
	{0x59ed, 0x02},
	{0x36e9, 0x23},
	{0x36f9, 0x23},
	{0x0100, 0x01},
};


/**2304*1296 20fps */
static struct regval_list sensor_300m_20fps_regs[] = {
//cleaned_0x01_FT_SC3335_MIPI_27Minput_506.25Mbps_2lane_10bit_2304x1296_20fps

	{0x0103, 0x01},
	{0x0100, 0x00},
	{0x36e9, 0x80},
	{0x36f9, 0x80},
	{0x301f, 0x01},
	{0x320e, 0x07},//05
	{0x320f, 0xE9},//46
	{0x3253, 0x04},
	{0x3301, 0x04},
	{0x3302, 0x10},
	{0x3304, 0x40},
	{0x3306, 0x40},
	{0x3309, 0x50},
	{0x330b, 0xb6},
	{0x330e, 0x29},
	{0x3310, 0x06},
	{0x3314, 0x96},
	{0x331e, 0x39},
	{0x331f, 0x49},
	{0x3320, 0x09},
	{0x3333, 0x10},
	{0x334c, 0x01},
	{0x3364, 0x17},
	{0x3367, 0x01},
	{0x3390, 0x04},
	{0x3391, 0x08},
	{0x3392, 0x38},
	{0x3393, 0x05},
	{0x3394, 0x09},
	{0x3395, 0x16},
	{0x33ac, 0x0c},
	{0x33ae, 0x1c},
	{0x3622, 0x16},
	{0x3637, 0x22},
	{0x363a, 0x1f},
	{0x363c, 0x05},
	{0x3670, 0x0e},
	{0x3674, 0xb0},
	{0x3675, 0x88},
	{0x3676, 0x68},
	{0x3677, 0x84},
	{0x3678, 0x85},
	{0x3679, 0x86},
	{0x367c, 0x18},
	{0x367d, 0x38},
	{0x367e, 0x08},
	{0x367f, 0x18},
	{0x3690, 0x43},
	{0x3691, 0x43},
	{0x3692, 0x44},
	{0x369c, 0x18},
	{0x369d, 0x38},
	{0x36ea, 0x3b},
	{0x36eb, 0x0d},
	{0x36ec, 0x1c},
	{0x36ed, 0x24},
	{0x36fa, 0x3b},
	{0x36fb, 0x00},
	{0x36fc, 0x10},
	{0x36fd, 0x24},
	{0x3908, 0x82},
	{0x391f, 0x18},
	{0x3e01, 0xa8},
	{0x3e02, 0x20},
	{0x3f00, 0x4c},
	{0x3f09, 0x48},
	{0x4505, 0x0a},
	{0x4509, 0x20},
	{0x4800, 0x44},
	{0x5799, 0x00},
	{0x59e0, 0x60},
	{0x59e1, 0x08},
	{0x59e2, 0x3f},
	{0x59e3, 0x18},
	{0x59e4, 0x18},
	{0x59e5, 0x3f},
	{0x59e6, 0x06},
	{0x59e7, 0x02},
	{0x59e8, 0x38},
	{0x59e9, 0x10},
	{0x59ea, 0x0c},
	{0x59eb, 0x10},
	{0x59ec, 0x04},
	{0x59ed, 0x02},
	{0x36e9, 0x23},
	{0x36f9, 0x23},
#if (SENSOR_HOR_VER_CFG0_REG1 == 1)
	{0x3221, 0x66},
#endif
	{0x0100, 0x01},
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
 * if not support the follow function ,retrun -EINVAL
 */

static int sensor_g_exp(struct v4l2_subdev *sd, __s32 *value)
{
	struct sensor_info *info = to_state(sd);
	*value = info->exp;
	sensor_dbg("sensor_get_exposure = %d\n", info->exp);
	return 0;
}

static int sc3335_sensor_vts;
static int sc3335_fps_change_flag;
static int sensor_s_exp(struct v4l2_subdev *sd, unsigned int exp_val)
{
	data_type explow, expmid, exphigh;
	struct sensor_info *info = to_state(sd);

	exphigh = (unsigned char) (0x0f & (exp_val>>15));
	expmid = (unsigned char) (0xff & (exp_val>>7));
	explow = (unsigned char) (0xf0 & (exp_val<<1));
	sensor_dbg("exp_val = %d\n", exp_val);
	sensor_write(sd, 0x3e02, explow);
	sensor_write(sd, 0x3e01, expmid);
	sensor_write(sd, 0x3e00, exphigh);
#if 0
	sensor_read(sd, 0x3e00, &exphigh);
	sensor_read(sd, 0x3e01, &expmid);
	sensor_read(sd, 0x3e02, &explow);
	sensor_print("exphigh = 0x%x, expmid = 0x%x, explow = 0x%x\n", exphigh, expmid, explow);
#endif
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

static int sensor_s_gain(struct v4l2_subdev *sd, int gain_val)
{
	struct sensor_info *info = to_state(sd);
	data_type gainlow = 0;
	data_type gainhigh = 0;
	data_type gaindiglow = 0x80;
	data_type gaindighigh = 0x00;
	data_type rdval;
	int gainana = gain_val;

	if (gainana < 0x20) {//2x
		gainhigh = 0x03;
		gainlow = gainana << 2;
	} else if (gainana < 2 * 0x20) {//2x2x
		gainhigh = 0x07;
		gainlow = gainana << 1;
	} else if (gainana < 4 * 0x20) {//4x2x
		gainhigh = 0x0f;
		gainlow = gainana;
	} else if (gainana < 8 * 0x20) {//8x2x
		gainhigh = 0x1f;
		gainlow = gainana >> 1;
	} else {
		gainhigh = 0x1f;	//again=16x
		gainlow = 0x7f;
		if (gainana < 16 * 0x20) {//16x2x
			gaindiglow = gainana >> 1;
			gaindighigh = 0x00;
		} else if (gainana < 32 * 0x20) {//32x2x
			gaindiglow = gainana >> 2;
			gaindighigh = 0x01;
		} else if (gainana < 64 * 0x20) {//64x2x
			gaindiglow = gainana >> 3;
			gaindighigh = 0x03;
		} else if (gainana < 128 * 0x20) {//128x2x
			gaindiglow = gainana >> 4;
			gaindighigh = 0x07;
		} else if (gainana < 256 * 0x20) {//256x2x
			gaindiglow = gainana >> 5;
			gaindighigh = 0x0f;
		} else {
			gaindiglow = 0xfc;
			gaindighigh = 0x0f;
		}
	}

	sensor_write(sd, 0x3e09, (unsigned char)gainlow);
	sensor_write(sd, 0x3e08, (unsigned char)gainhigh);
	sensor_write(sd, 0x3e07, (unsigned char)gaindiglow);
	sensor_write(sd, 0x3e06, (unsigned char)gaindighigh);
	sensor_dbg("sensor_set_anagain = %d, 0x%x, 0x%x Done!\n", gain_val, gainhigh << 2, gainlow);
	sensor_dbg("digital_gain = 0x%x, 0x%x Done!\n", gaindighigh, gaindiglow);


	//logic   update 20210224
	// sensor_write(0x3812,0x00);//sensor group hold start
	sensor_read(sd, 0x3109, &rdval);
	sensor_dbg("######## rdval = 0x%x \n", rdval);
	if (rdval == 1) {
		sensor_dbg("if gainana = %d\n", gainana);
		if (gainana < 0x20) {		//2x
			sensor_write(sd, 0x3812, 0x00);
			sensor_write(sd, 0x363c, 0x05);
			sensor_write(sd, 0x330e, 0x29);
			sensor_write(sd, 0x3812, 0x30);
		} else if (gainana < 4 * 0x20) {	//4x2x
			sensor_write(sd, 0x3812, 0x00);
			sensor_write(sd, 0x363c, 0x07);
			sensor_write(sd, 0x330e, 0x25);
			sensor_write(sd, 0x3812, 0x30);
		} else {
			sensor_write(sd, 0x3812, 0x00);
			sensor_write(sd, 0x363c, 0x07);
			sensor_write(sd, 0x330e, 0x18);
			sensor_write(sd, 0x3812, 0x30);
		}
	} else {
		sensor_dbg("else gainana = %d\n", gainana);
		if (gainana < 0x20) {
			sensor_write(sd, 0x3812, 0x00);
			sensor_write(sd, 0x363c, 0x06);
			sensor_write(sd, 0x330e, 0x29);
			sensor_write(sd, 0x3812, 0x30);
		} else if (gainana < 4 * 0x20) {
			sensor_write(sd, 0x3812, 0x00);
			sensor_write(sd, 0x363c, 0x08);
			sensor_write(sd, 0x330e, 0x25);
			sensor_write(sd, 0x3812, 0x30);
		} else {
			sensor_write(sd, 0x3812, 0x00);
			sensor_write(sd, 0x363c, 0x08);
			sensor_write(sd, 0x330e, 0x18);
			sensor_write(sd, 0x3812, 0x30);
		}
	}

	sensor_read(sd, 0x363c, &gainlow);
	sensor_read(sd, 0x330e, &gainhigh);
	sensor_dbg("0x363c = 0x%x, 0x330e = 0x%x\n", gainlow, gainhigh);
	info->gain = gain_val;

	return 0;
}

static int sensor_s_exp_gain(struct v4l2_subdev *sd,
			     struct sensor_exp_gain *exp_gain)
{
	struct sensor_info *info = to_state(sd);
	int shutter, frame_length;
	int exp_val, gain_val;
//	data_type read_high = 0, read_low = 0;

	exp_val = exp_gain->exp_val;
	gain_val = exp_gain->gain_val;
	if (gain_val < 1 * 16)
		gain_val = 16;
	if (exp_val > 0xfffff)
		exp_val = 0xfffff;

	if (!sc3335_fps_change_flag) {
		shutter = exp_val >> 4;
		if (shutter > sc3335_sensor_vts - 8) {
			frame_length = shutter + 8;
		} else
			frame_length = sc3335_sensor_vts;
		sensor_write(sd, 0x320f, (frame_length & 0xff));
		sensor_write(sd, 0x320e, (frame_length >> 8));

//		sensor_read(sd, 0x320e, &read_high);
//		sensor_read(sd, 0x320f, &read_low);
//		sensor_print("0x41 = 0x%x, 0x42 = 0x%x\n", read_high, read_low);
	}

	sensor_s_exp(sd, exp_val);
	sensor_s_gain(sd, gain_val);
	sensor_dbg("sensor_set_gain exp = %d, %d Done!\n", gain_val, exp_val);
	info->exp = exp_val;
	info->gain = gain_val;

	return 0;
}

static int sensor_s_fps(struct v4l2_subdev *sd, struct sensor_fps *fps)
{
	struct sensor_info *info = to_state(sd);
	struct sensor_win_size *wsize = info->current_wins;
//	data_type read_high = 0, read_low = 0;
	int sc3335_sensor_target_vts = 0;

	sc3335_fps_change_flag = 1;
	sc3335_sensor_target_vts = wsize->pclk/fps->fps/wsize->hts;

	if (sc3335_sensor_target_vts <= wsize->vts) {// the max fps = 20fps
		sc3335_sensor_target_vts = wsize->vts;
	} else if (sc3335_sensor_target_vts >= (wsize->pclk/wsize->hts)) {// the min fps = 1fps
		sc3335_sensor_target_vts = (wsize->pclk/wsize->hts) - 8;
	}

	sc3335_sensor_vts = sc3335_sensor_target_vts;
	sensor_dbg("target_fps = %d, sc3335_sensor_target_vts = %d, 0x320e = 0x%x, 0x320f = 0x%x\n", fps->fps,
		sc3335_sensor_target_vts, sc3335_sensor_target_vts >> 8, sc3335_sensor_target_vts & 0xff);
	sensor_write(sd, 0x320f, (sc3335_sensor_target_vts & 0xff));
	sensor_write(sd, 0x320e, (sc3335_sensor_target_vts >> 8));
//	sensor_read(sd, 0x320e, &read_high);
//	sensor_read(sd, 0x320f, &read_low);
//	sensor_print("[get write_times] 0x41 = 0x%x, 0x42 = 0x%x\n", read_high, read_low);
	sc3335_fps_change_flag = 0;

	return 0;
}

static int sensor_get_temp(struct v4l2_subdev *sd,  struct sensor_temp *temp)
{
	struct sensor_info *info = to_state(sd);
	data_type rdval_high = 0, rdval_low = 0;
	int temper_sum = 0, current_temper = 0;
	int ret = 0;

	ret = sensor_read(sd, 0x3911, &rdval_high);
	ret = sensor_read(sd, 0x3912, &rdval_low);
	temper_sum |= rdval_high << 8;
	temper_sum |= rdval_low;

	if ((temper_sum < 4000) || (info->gain < 16)) {
		current_temper = 55;
	} else {
		current_temper = (temper_sum - 4000) * (info->gain - 16) / 32768 + 55;
	}
	temp->temp = current_temper;
//	sensor_print("-------temperature_value = %d-------\n", temp->temp);

	return ret;
}

static int sensor_s_hflip(struct v4l2_subdev *sd, int enable)
{
	data_type get_value;
	data_type set_value;

	printk("into set sensor hfilp the value:%d \n", enable);
	if (!(enable == 0 || enable == 1))
		return -1;

	sensor_read(sd, 0x3221, &get_value);
	sensor_dbg("sensor_s_hflip -- 0x3221 = 0x%x\n", get_value);

#if (SENSOR_HOR_VER_CFG0_REG1 == 1)//h60ga change
	sensor_dbg("###### SENSOR_HOR_VER_CFG0_REG1 #####\n");
	if (enable)
		set_value = get_value & 0xf9;
	else
		set_value = get_value | 0x06;
#else
	if (enable)
		set_value = get_value | 0x06;
	else
		set_value = get_value & 0xf9;
#endif

	sensor_write(sd, 0x3221, set_value);
	return 0;
}

static int sensor_s_vflip(struct v4l2_subdev *sd, int enable)
{
	data_type get_value;
	data_type set_value;

	printk("into set sensor vfilp the value:%d \n", enable);
	if (!(enable == 0 || enable == 1))
		return -1;

	sensor_read(sd, 0x3221, &get_value);
	sensor_dbg("sensor_s_vflip -- 0x3221 = 0x%x\n", get_value);

#if (SENSOR_HOR_VER_CFG0_REG1 == 1)//h60ga change
	sensor_dbg("###### SENSOR_HOR_VER_CFG0_REG1 #####\n");
	if (enable)
		set_value = get_value & 0x9f;
	else
		set_value = get_value | 0x60;
#else
	if (enable)
		set_value = get_value | 0x60;
	else
		set_value = get_value & 0x9f;
#endif

	sensor_write(sd, 0x3221, set_value);
	return 0;

}

/*
 *set && get sensor flip
 */
 static int sensor_get_fmt_mbus_core(struct v4l2_subdev *sd, int *code)
{
	struct sensor_info *info = to_state(sd);
	data_type get_value;

	sensor_read(sd, 0x3221, &get_value);
	sensor_dbg("-- read value:0x%X --\n", get_value);
	switch (get_value & 0x66) {
	case 0x00:
		*code = MEDIA_BUS_FMT_SBGGR10_1X10;
//		printk("--BGGR hfilp set read value:0x%X --\n", get_value &  0x66);
		break;
	case 0x06:
		*code = MEDIA_BUS_FMT_SBGGR10_1X10;
//		printk("--GBRG hfilp set read value:0x%X --\n", get_value & 0x66);
		break;
	case 0x60:
		*code = MEDIA_BUS_FMT_SBGGR10_1X10;
//		printk("--GRBG hfilp set read value:0x%X --\n", get_value & 0x66);
		break;
	case 0x66:
		*code = MEDIA_BUS_FMT_SBGGR10_1X10;
//		printk("--RGGB hfilp set read value:0x%X --\n", get_value & 0x66);
		break;
	default:
		*code = info->fmt->mbus_code;
	}
	return 0;
}

static int sensor_s_sw_stby(struct v4l2_subdev *sd, int on_off)
{
	int ret;
	data_type rdval;

	ret = sensor_read(sd, 0x0100, &rdval);
	if (ret != 0)
		return ret;

	if (on_off == STBY_ON)
		ret = sensor_write(sd, 0x0100, rdval&0xfe);
	else
		ret = sensor_write(sd, 0x0100, rdval|0x01);
	return ret;
}

/*
 * Stuff that knows about the sensor.
 */
static int sensor_power(struct v4l2_subdev *sd, int on)
{
	int ret = 0;

	switch (on) {
	case STBY_ON:
		sensor_dbg("STBY_ON!\n");
		cci_lock(sd);
		ret = sensor_s_sw_stby(sd, STBY_ON);
		if (ret < 0)
			sensor_err("soft stby falied!\n");
		usleep_range(10000, 12000);
		cci_unlock(sd);
		break;
	case STBY_OFF:
		sensor_dbg("STBY_OFF!\n");
		cci_lock(sd);
		usleep_range(10000, 12000);
		ret = sensor_s_sw_stby(sd, STBY_OFF);
		if (ret < 0)
			sensor_err("soft stby off falied!\n");
		cci_unlock(sd);
		break;
	case PWR_ON:
		sensor_dbg("PWR_ON!\n");
		cci_lock(sd);
		vin_gpio_set_status(sd, PWDN, 1);
		vin_gpio_set_status(sd, RESET, 1);
		vin_gpio_set_status(sd, POWER_EN, 1);
		vin_gpio_write(sd, RESET, CSI_GPIO_LOW);
		vin_gpio_write(sd, PWDN, CSI_GPIO_LOW);
		vin_gpio_write(sd, POWER_EN, CSI_GPIO_HIGH);
		vin_set_pmu_channel(sd, IOVDD, ON);
		vin_set_pmu_channel(sd, DVDD, ON);
		vin_set_pmu_channel(sd, AVDD, ON);
		usleep_range(10000, 12000);
		vin_gpio_write(sd, RESET, CSI_GPIO_HIGH);
		vin_gpio_write(sd, PWDN, CSI_GPIO_HIGH);
		usleep_range(10000, 12000);
		vin_set_mclk(sd, ON);
		usleep_range(10000, 12000);
		vin_set_mclk_freq(sd, MCLK);
		usleep_range(30000, 32000);
		cci_unlock(sd);
		break;
	case PWR_OFF:
		sensor_dbg("PWR_OFF!\n");
		cci_lock(sd);
		vin_gpio_set_status(sd, PWDN, 1);
		vin_gpio_set_status(sd, RESET, 1);
		vin_gpio_write(sd, RESET, CSI_GPIO_LOW);
		vin_gpio_write(sd, PWDN, CSI_GPIO_LOW);
		vin_set_mclk(sd, OFF);
		vin_set_pmu_channel(sd, AFVDD, OFF);
		vin_set_pmu_channel(sd, AVDD, OFF);
		vin_set_pmu_channel(sd, IOVDD, OFF);
		vin_set_pmu_channel(sd, DVDD, OFF);
		vin_gpio_write(sd, POWER_EN, CSI_GPIO_LOW);
		vin_gpio_set_status(sd, RESET, 0);
		vin_gpio_set_status(sd, PWDN, 0);
		vin_gpio_set_status(sd, POWER_EN, 0);
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
		usleep_range(1000, 1200);
		break;
	case 1:
		vin_gpio_write(sd, RESET, CSI_GPIO_LOW);
		usleep_range(1000, 1200);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int sensor_detect(struct v4l2_subdev *sd)
{
	unsigned int SENSOR_ID = 0;
	data_type rdval;
	int cnt = 0;

	sensor_read(sd, 0x3107, &rdval);
	SENSOR_ID |= (rdval << 8);
	sensor_read(sd, 0x3108, &rdval);
	SENSOR_ID |= (rdval);
	sensor_print("V4L2_IDENT_SENSOR = 0x%x\n", SENSOR_ID);

	while ((SENSOR_ID != V4L2_IDENT_SENSOR) && (cnt < 5)) {
		sensor_read(sd, 0x3107, &rdval);
		SENSOR_ID |= (rdval << 8);
		sensor_read(sd, 0x3108, &rdval);
		SENSOR_ID |= (rdval);
		sensor_print("retry = %d, V4L2_IDENT_SENSOR = %x\n",
			cnt, SENSOR_ID);
		cnt++;
		}
	if (SENSOR_ID != V4L2_IDENT_SENSOR)
		return -ENODEV;

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
	info->low_speed = 0;
	info->width = 2304;
	info->height = 1296;
	info->hflip = 0;
	info->vflip = 0;
	info->gain = 0;
	info->exp = 0;

	info->tpf.numerator = 1;
	info->tpf.denominator = 25;/* 20fps */

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
		ret = 0;
		break;
	case VIDIOC_VIN_SENSOR_EXP_GAIN:
		ret = sensor_s_exp_gain(sd, (struct sensor_exp_gain *)arg);
		break;
	case VIDIOC_VIN_SENSOR_SET_FPS:
		ret = sensor_s_fps(sd, (struct sensor_fps *)arg);
		break;
	case VIDIOC_VIN_SENSOR_CFG_REQ:
		sensor_cfg_req(sd, (struct sensor_config *)arg);
		break;
	case VIDIOC_VIN_GET_SENSOR_CODE:
		sensor_get_fmt_mbus_core(sd, (int *)arg);
		break;
	case VIDIOC_VIN_SENSOR_GET_TEMP:
		ret = sensor_get_temp(sd, (struct sensor_temp *)arg);
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
#if 0
		.mbus_code = MEDIA_BUS_FMT_SRGGB10_1X10,
#else
		.mbus_code = MEDIA_BUS_FMT_SBGGR10_1X10,
#endif
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
#if 0
//1080_25fps
	{
		.width      = 1920,
		.height     = 1080,
		.hoffset    = 0,
		.voffset    = 0,
		.hts        = 2200,
		.vts        = 1350,
		.pclk       = 74250000,
		.mipi_bps	= 371250000,
		.fps_fixed  = 25,
		.bin_factor = 1,
		.intg_min   = 1<<4,
		.intg_max   = 1125<<4,
		.gain_min   = 1<<4,
		.gain_max   = 110<<4,
		.wdr_mode = ISP_COMANDING_MODE,
		.regs = sensor_1080p25_regs,
		.regs_size = ARRAY_SIZE(sensor_1080p25_regs),
		.set_size = NULL,
	},

//1080_30fps
	{
		.width      = 1920,
		.height     = 1080,
		.hoffset    = 0,
		.voffset    = 0,
		.hts        = 2200,
		.vts        = 1125,
		.pclk       = 74250000,
		.mipi_bps	= 371250000,
		.fps_fixed  = 30,
		.bin_factor = 1,
		.intg_min   = 1<<4,
		.intg_max   = 1363<<4,
		.gain_min   = 1<<4,
		.gain_max   = 110<<4,
		.wdr_mode = ISP_COMANDING_MODE,
		.regs = sensor_1080p30_regs,
		.regs_size = ARRAY_SIZE(sensor_1080p30_regs),
		.set_size = NULL,
	},
#endif
//300M_25fps
	{
		.width      = 2304,
		.height     = 1296,
		.hoffset    = 0,
		.voffset    = 0,
		.hts        = 2500,
		.vts        = 1620,
		.pclk       = 101250000,
		.mipi_bps	= 506250000,
		.fps_fixed  = 25,
		.bin_factor = 1,
		.intg_min   = 1<<4,
		.intg_max   = 1363<<4,
		.gain_min   = 1<<4,
		.gain_max   = 110<<4,
		.wdr_mode   = ISP_COMANDING_MODE,
		.regs       = sensor_300m_25fps_regs,
		.regs_size  = ARRAY_SIZE(sensor_300m_25fps_regs),
		.set_size   = NULL,
	},

//300M_30fps

	{
		.width      = 2304,
		.height     = 1296,
		.hoffset    = 0,
		.voffset    = 0,
		.hts        = 2500,
		.vts        = 1350,
		.pclk       = 101250000,
		.mipi_bps	= 506250000,
		.fps_fixed  = 30,
		.bin_factor = 1,
		.intg_min   = 1<<4,
		.intg_max   = 1350<<4,
		.gain_min   = 1<<4,
		.gain_max   = 110<<4,
		.wdr_mode   = ISP_COMANDING_MODE,
		.regs       = sensor_300m_30fps_regs,
		.regs_size  = ARRAY_SIZE(sensor_300m_30fps_regs),
		.set_size   = NULL,
	},

//300M_20fps

	{
		.width      = 2304,
		.height     = 1296,
		.hoffset    = 0,
		.voffset    = 0,
		.hts        = 2500,
		.vts        = 2025,
		.pclk       = 101250000,
		.mipi_bps   = 506250000,
		.fps_fixed  = 20,
		.bin_factor = 1,
		.intg_min   = 1<<4,
		.intg_max   = 2025<<4,
		.gain_min   = 1<<4,
		.gain_max   = 110<<4,
		.wdr_mode   = ISP_COMANDING_MODE,
		.regs       = sensor_300m_20fps_regs,
		.regs_size  = ARRAY_SIZE(sensor_300m_20fps_regs),
		.set_size   = NULL,
	},
};

#define N_WIN_SIZES (ARRAY_SIZE(sensor_win_sizes))

static int sensor_g_mbus_config(struct v4l2_subdev *sd,
				struct v4l2_mbus_config *cfg)
{
	struct sensor_info *info = to_state(sd);

	cfg->type = V4L2_MBUS_CSI2;
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
	data_type read_value;
	struct v4l2_subdev *sd = &info->sd;
	struct sensor_format_struct *sensor_fmt = info->fmt;
	struct sensor_win_size *wsize = info->current_wins;

	ret = sensor_write_array(sd, sensor_default_regs,
				 ARRAY_SIZE(sensor_default_regs));
	if (ret < 0) {
		sensor_err("write sensor_default_regs error\n");
		return ret;
	}

	sensor_dbg("sensor_reg_init\n");

	sensor_write_array(sd, sensor_fmt->regs, sensor_fmt->regs_size);

	if (wsize->regs)
		sensor_write_array(sd, wsize->regs, wsize->regs_size);

	//SENSOR_HOR_VER_CFG0_REG1
//	sensor_s_vflip(sd, 1);	//h60ga change
//	sensor_s_hflip(sd, 1);	//h60ga change

	if (wsize->set_size)
		wsize->set_size(sd);

	info->width = wsize->width;
	info->height = wsize->height;
	sc3335_sensor_vts = wsize->vts;
	sc3335_fps_change_flag = 0;
	sensor_read(sd, 0x3221, &read_value);
	sensor_print("######## 0x3221 = 0x%x\n", read_value);

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
		.addr_width = CCI_BITS_16,
		.data_width = CCI_BITS_8,
	}, {
		.name = SENSOR_NAME_2,
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
	info->combo_mode = CMB_TERMINAL_RES | CMB_PHYA_OFFSET2 | MIPI_NORMAL_MODE;
	info->time_hs = 0x23;
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
