/*
 * A V4L2 driver for sc5336 Raw cameras.
 *
 * Copyright (c) 2022 by Allwinnertech Co., Ltd.  http://www.allwinnertech.com
 *
 * Authors:  Liu Chensheng <liuchensheng@allwinnertech.com>
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
MODULE_DESCRIPTION("A low-level driver for S5336_MIPI sensors");
MODULE_LICENSE("GPL");

#define MCLK              (27*1000*1000)
#define V4L2_IDENT_SENSOR 0xce50

/*
 * Our nominal (default) frame rate.
 */

#define SENSOR_FRAME_RATE 20

/*
 * The SC4353_MIPI i2c address
 */
#define I2C_ADDR 0x60

#define SENSOR_NUM 0x2
#define SENSOR_NAME "sc5336_mipi"
#define SENSOR_NAME_2 "sc5336_mipi_2"

/*
 * The default register settings
 */

static struct regval_list sensor_default_regs[] = {

};

static struct regval_list sensor_2880_1620p20_regs[] = {
	{0x0103, 0x01},
	{0x36e9, 0x80},
	{0x37f9, 0x80},
	{0x301f, 0x09},
	{0x320c, 0x09},
	{0x320d, 0x60},
	{0x320e, 0x06},
	{0x320f, 0x72},
	{0x3213, 0x04},
	{0x3241, 0x00},
	{0x3243, 0x01},
	{0x3248, 0x02},
	{0x3249, 0x0b},
	{0x3250, 0xff},
	{0x3253, 0x10},
	{0x3258, 0x0c},
	{0x3301, 0x0a},
	{0x3305, 0x00},
	{0x3306, 0x58},
	{0x3308, 0x08},
	{0x3309, 0xb0},
	{0x330a, 0x00},
	{0x330b, 0xc8},
	{0x3314, 0x14},
	{0x331f, 0xa1},
	{0x3321, 0x10},
	{0x3327, 0x14},
	{0x3328, 0x0b},
	{0x3329, 0x0e},
	{0x3333, 0x10},
	{0x3334, 0x40},
	{0x3356, 0x10},
	{0x3364, 0x5e},
	{0x3390, 0x09},
	{0x3391, 0x0b},
	{0x3392, 0x0f},
	{0x3393, 0x10},
	{0x3394, 0x18},
	{0x3395, 0x98},
	{0x3396, 0x08},
	{0x3397, 0x09},
	{0x3398, 0x0f},
	{0x3399, 0x0a},
	{0x339a, 0x18},
	{0x339b, 0x60},
	{0x339c, 0xff},
	{0x33ad, 0x0c},
	{0x33ae, 0x68},
	{0x33b2, 0x48},
	{0x33b3, 0x28},
	{0x33f8, 0x00},
	{0x33f9, 0x70},
	{0x33fa, 0x00},
	{0x33fb, 0x88},
	{0x33fc, 0x0b},
	{0x33fd, 0x1f},
	{0x349f, 0x03},
	{0x34a6, 0x0b},
	{0x34a7, 0x1f},
	{0x34a8, 0x18},
	{0x34a9, 0x08},
	{0x34aa, 0x00},
	{0x34ab, 0xe0},
	{0x34ac, 0x00},
	{0x34ad, 0xf8},
	{0x34f8, 0x1f},
	{0x34f9, 0x08},
	{0x3630, 0xc0},
	{0x3631, 0x83},
	{0x3632, 0x54},
	{0x3633, 0x33},
	{0x3641, 0x20},
	{0x3670, 0x56},
	{0x3674, 0xc0},
	{0x3675, 0xa0},
	{0x3676, 0xa0},
	{0x3677, 0x83},
	{0x3678, 0x86},
	{0x3679, 0x8a},
	{0x367c, 0x08},
	{0x367d, 0x0f},
	{0x367e, 0x08},
	{0x367f, 0x0f},
	{0x3696, 0x23},
	{0x3697, 0x33},
	{0x3698, 0x43},
	{0x36a0, 0x09},
	{0x36a1, 0x0f},
	{0x36b0, 0x88},
	{0x36b1, 0x92},
	{0x36b2, 0xa4},
	{0x36b3, 0xc7},
	{0x36b4, 0x09},
	{0x36b5, 0x0b},
	{0x36b6, 0x0f},
	{0x36ea, 0x0b},
	{0x36eb, 0x0c},
	{0x36ec, 0x0c},
	{0x36ed, 0x96},
	{0x370f, 0x01},
	{0x3721, 0x6c},
	{0x3722, 0x89},
	{0x3724, 0x21},
	{0x3725, 0xb4},
	{0x3727, 0x14},
	{0x3771, 0x89},
	{0x3772, 0x85},
	{0x3773, 0x85},
	{0x377a, 0x0b},
	{0x377b, 0x1f},
	{0x37fa, 0x0b},
	{0x37fb, 0x24},
	{0x37fc, 0x01},
	{0x37fd, 0x16},
	{0x3901, 0x00},
	{0x3904, 0x04},
	{0x3905, 0x8c},
	{0x391d, 0x04},
	{0x391f, 0x49},
	{0x3926, 0x21},
	{0x3933, 0x80},
	{0x3934, 0x0a},
	{0x3935, 0x00},
	{0x3936, 0xff},
	{0x3937, 0x75},
	{0x3938, 0x74},
	{0x393c, 0x1e},
	{0x39dc, 0x02},
	{0x3e00, 0x00},
	{0x3e01, 0x66},
	{0x3e02, 0xa0},
	{0x3e09, 0x00},
	{0x440d, 0x10},
	{0x440e, 0x02},
	{0x450d, 0x18},
	{0x4800, 0x44},
	{0x5780, 0x66},
	{0x5787, 0x08},
	{0x5788, 0x03},
	{0x5789, 0x00},
	{0x578a, 0x08},
	{0x578b, 0x03},
	{0x578c, 0x00},
	{0x578d, 0x40},
	{0x5790, 0x08},
	{0x5791, 0x04},
	{0x5792, 0x01},
	{0x5793, 0x08},
	{0x5794, 0x04},
	{0x5795, 0x01},
	{0x5799, 0x46},
	{0x57aa, 0x2a},
	{0x5ae0, 0xfe},
	{0x5ae1, 0x40},
	{0x5ae2, 0x38},
	{0x5ae3, 0x30},
	{0x5ae4, 0x0c},
	{0x5ae5, 0x38},
	{0x5ae6, 0x30},
	{0x5ae7, 0x28},
	{0x5ae8, 0x3f},
	{0x5ae9, 0x34},
	{0x5aea, 0x2c},
	{0x5aeb, 0x3f},
	{0x5aec, 0x34},
	{0x5aed, 0x2c},
	{0x36e9, 0x44},
	{0x37f9, 0x44},
	//{0x0100, 0x01},
};

static struct regval_list sensor_2880_1620p15_regs[] = {
	{0x0103, 0x01},
	{0x36e9, 0x80},
	{0x37f9, 0x80},
	{0x301f, 0x09},
	{0x320c, 0x09},
	{0x320d, 0x60},
	{0x320e, 0x08},
	{0x320f, 0x98},
	{0x3213, 0x04},
	{0x3241, 0x00},
	{0x3243, 0x01},
	{0x3248, 0x02},
	{0x3249, 0x0b},
	{0x3250, 0xff},
	{0x3253, 0x10},
	{0x3258, 0x0c},
	{0x3301, 0x0a},
	{0x3305, 0x00},
	{0x3306, 0x58},
	{0x3308, 0x08},
	{0x3309, 0xb0},
	{0x330a, 0x00},
	{0x330b, 0xc8},
	{0x3314, 0x14},
	{0x331f, 0xa1},
	{0x3321, 0x10},
	{0x3327, 0x14},
	{0x3328, 0x0b},
	{0x3329, 0x0e},
	{0x3333, 0x10},
	{0x3334, 0x40},
	{0x3356, 0x10},
	{0x3364, 0x5e},
	{0x3390, 0x09},
	{0x3391, 0x0b},
	{0x3392, 0x0f},
	{0x3393, 0x10},
	{0x3394, 0x18},
	{0x3395, 0x98},
	{0x3396, 0x08},
	{0x3397, 0x09},
	{0x3398, 0x0f},
	{0x3399, 0x0a},
	{0x339a, 0x18},
	{0x339b, 0x60},
	{0x339c, 0xff},
	{0x33ad, 0x0c},
	{0x33ae, 0x68},
	{0x33b2, 0x48},
	{0x33b3, 0x28},
	{0x33f8, 0x00},
	{0x33f9, 0x70},
	{0x33fa, 0x00},
	{0x33fb, 0x88},
	{0x33fc, 0x0b},
	{0x33fd, 0x1f},
	{0x349f, 0x03},
	{0x34a6, 0x0b},
	{0x34a7, 0x1f},
	{0x34a8, 0x18},
	{0x34a9, 0x08},
	{0x34aa, 0x00},
	{0x34ab, 0xe0},
	{0x34ac, 0x00},
	{0x34ad, 0xf8},
	{0x34f8, 0x1f},
	{0x34f9, 0x08},
	{0x3630, 0xc0},
	{0x3631, 0x83},
	{0x3632, 0x54},
	{0x3633, 0x33},
	{0x3641, 0x20},
	{0x3670, 0x56},
	{0x3674, 0xc0},
	{0x3675, 0xa0},
	{0x3676, 0xa0},
	{0x3677, 0x83},
	{0x3678, 0x86},
	{0x3679, 0x8a},
	{0x367c, 0x08},
	{0x367d, 0x0f},
	{0x367e, 0x08},
	{0x367f, 0x0f},
	{0x3696, 0x23},
	{0x3697, 0x33},
	{0x3698, 0x43},
	{0x36a0, 0x09},
	{0x36a1, 0x0f},
	{0x36b0, 0x88},
	{0x36b1, 0x92},
	{0x36b2, 0xa4},
	{0x36b3, 0xc7},
	{0x36b4, 0x09},
	{0x36b5, 0x0b},
	{0x36b6, 0x0f},
	{0x36ea, 0x0b},
	{0x36eb, 0x0c},
	{0x36ec, 0x0c},
	{0x36ed, 0x96},
	{0x370f, 0x01},
	{0x3721, 0x6c},
	{0x3722, 0x89},
	{0x3724, 0x21},
	{0x3725, 0xb4},
	{0x3727, 0x14},
	{0x3771, 0x89},
	{0x3772, 0x85},
	{0x3773, 0x85},
	{0x377a, 0x0b},
	{0x377b, 0x1f},
	{0x37fa, 0x0b},
	{0x37fb, 0x24},
	{0x37fc, 0x01},
	{0x37fd, 0x16},
	{0x3901, 0x00},
	{0x3904, 0x04},
	{0x3905, 0x8c},
	{0x391d, 0x04},
	{0x391f, 0x49},
	{0x3926, 0x21},
	{0x3933, 0x80},
	{0x3934, 0x0a},
	{0x3935, 0x00},
	{0x3936, 0xff},
	{0x3937, 0x75},
	{0x3938, 0x74},
	{0x393c, 0x1e},
	{0x39dc, 0x02},
	{0x3e00, 0x00},
	{0x3e01, 0x66},
	{0x3e02, 0xa0},
	{0x3e09, 0x00},
	{0x440d, 0x10},
	{0x440e, 0x02},
	{0x450d, 0x18},
	{0x4800, 0x44},
	{0x5780, 0x66},
	{0x5787, 0x08},
	{0x5788, 0x03},
	{0x5789, 0x00},
	{0x578a, 0x08},
	{0x578b, 0x03},
	{0x578c, 0x00},
	{0x578d, 0x40},
	{0x5790, 0x08},
	{0x5791, 0x04},
	{0x5792, 0x01},
	{0x5793, 0x08},
	{0x5794, 0x04},
	{0x5795, 0x01},
	{0x5799, 0x46},
	{0x57aa, 0x2a},
	{0x5ae0, 0xfe},
	{0x5ae1, 0x40},
	{0x5ae2, 0x38},
	{0x5ae3, 0x30},
	{0x5ae4, 0x0c},
	{0x5ae5, 0x38},
	{0x5ae6, 0x30},
	{0x5ae7, 0x28},
	{0x5ae8, 0x3f},
	{0x5ae9, 0x34},
	{0x5aea, 0x2c},
	{0x5aeb, 0x3f},
	{0x5aec, 0x34},
	{0x5aed, 0x2c},
	{0x36e9, 0x44},
	{0x37f9, 0x44},
	//{0x0100, 0x01},
};

static struct regval_list sensor_1440_400p130_regs[] = {
	//h sum + v binning
	{0x0103, 0x01},
	{0x36e9, 0x80},
	{0x37f9, 0x80},
	{0x301f, 0x13},
	{0x3200, 0x00},
	{0x3201, 0x00},
	{0x3202, 0x01},
	{0x3203, 0x9a},
	{0x3204, 0x0b},
	{0x3205, 0x47},
	{0x3206, 0x04},
	{0x3207, 0xc1},
	{0x3208, 0x05},
	{0x3209, 0xa0},
	{0x320a, 0x01},
	{0x320b, 0x90},
	{0x320c, 0x05},
	{0x320d, 0xea},
	{0x320e, 0x01},
	{0x320f, 0xb7},
	{0x3210, 0x00},
	{0x3211, 0x02},
	{0x3212, 0x00},
	{0x3213, 0x02},
	{0x3215, 0x31},
	{0x3220, 0x01},
	{0x3241, 0x00},
	{0x3243, 0x01},
	{0x3248, 0x02},
	{0x3249, 0x0b},
	{0x3253, 0x10},
	{0x3258, 0x0c},
	{0x3301, 0x0a},
	{0x3305, 0x00},
	{0x3306, 0x58},
	{0x3308, 0x08},
	{0x3309, 0xb0},
	{0x330a, 0x00},
	{0x330b, 0xc8},
	{0x3314, 0x14},
	{0x331f, 0xa1},
	{0x3321, 0x10},
	{0x3327, 0x14},
	{0x3328, 0x0b},
	{0x3329, 0x0e},
	{0x3333, 0x10},
	{0x3334, 0x40},
	{0x3356, 0x10},
	{0x3364, 0x5e},
	{0x3390, 0x09},
	{0x3391, 0x0b},
	{0x3392, 0x0f},
	{0x3393, 0x10},
	{0x3394, 0x18},
	{0x3395, 0x98},
	{0x3396, 0x08},
	{0x3397, 0x09},
	{0x3398, 0x0f},
	{0x3399, 0x0a},
	{0x339a, 0x18},
	{0x339b, 0x60},
	{0x339c, 0xff},
	{0x33ad, 0x0c},
	{0x33ae, 0x68},
	{0x33b2, 0x48},
	{0x33b3, 0x28},
	{0x33f8, 0x00},
	{0x33f9, 0x70},
	{0x33fa, 0x00},
	{0x33fb, 0x90},
	{0x33fc, 0x0b},
	{0x33fd, 0x1f},
	{0x349f, 0x03},
	{0x34a6, 0x0b},
	{0x34a7, 0x1f},
	{0x34a8, 0x18},
	{0x34a9, 0x08},
	{0x34aa, 0x00},
	{0x34ab, 0xe8},
	{0x34ac, 0x01},
	{0x34ad, 0x00},
	{0x34f8, 0x1f},
	{0x34f9, 0x08},
	{0x3630, 0xc0},
	{0x3631, 0x83},
	{0x3632, 0x54},
	{0x3633, 0x33},
	{0x3641, 0x20},
	{0x3670, 0x56},
	{0x3674, 0xc0},
	{0x3675, 0xa0},
	{0x3676, 0xa0},
	{0x3677, 0x83},
	{0x3678, 0x86},
	{0x3679, 0x8a},
	{0x367c, 0x08},
	{0x367d, 0x0f},
	{0x367e, 0x08},
	{0x367f, 0x0f},
	{0x3696, 0x23},
	{0x3697, 0x33},
	{0x3698, 0x43},
	{0x36a0, 0x09},
	{0x36a1, 0x0f},
	{0x36b0, 0x88},
	{0x36b1, 0x92},
	{0x36b2, 0xa4},
	{0x36b3, 0xc7},
	{0x36b4, 0x09},
	{0x36b5, 0x0b},
	{0x36b6, 0x0f},
	{0x36ea, 0x0c},
	{0x36eb, 0x0c},
	{0x36ec, 0x1c},
	{0x36ed, 0x16},
	{0x370f, 0x01},
	{0x3721, 0x6c},
	{0x3722, 0x89},
	{0x3724, 0x21},
	{0x3725, 0xb4},
	{0x3727, 0x14},
	{0x3771, 0x89},
	{0x3772, 0x85},
	{0x3773, 0x85},
	{0x377a, 0x0b},
	{0x377b, 0x1f},
	{0x37fa, 0x0c},
	{0x37fb, 0x24},
	{0x37fc, 0x01},
	{0x37fd, 0x16},
	{0x3901, 0x00},
	{0x3904, 0x04},
	{0x3905, 0x8c},
	{0x3908, 0x20},
	{0x391d, 0x04},
	{0x391f, 0x49},
	{0x3926, 0x21},
	{0x3933, 0x80},
	{0x3934, 0x0a},
	{0x3935, 0x00},
	{0x3936, 0xff},
	{0x3937, 0x75},
	{0x3938, 0x74},
	{0x393c, 0x1e},
	{0x39dc, 0x02},
	{0x3e00, 0x00},
	{0x3e01, 0x1a},
	{0x3e02, 0xf0},
	{0x3e09, 0x00},
	{0x440d, 0x10},
	{0x440e, 0x02},
	{0x450d, 0x18},
	{0x4800, 0x44},
	{0x4819, 0x06},
	{0x481b, 0x03},
	{0x481d, 0x0c},
	{0x481f, 0x03},
	{0x4821, 0x08},
	{0x4823, 0x03},
	{0x4825, 0x03},
	{0x4827, 0x03},
	{0x4829, 0x05},
	{0x5000, 0x76},
	{0x5780, 0x66},
	{0x5787, 0x08},
	{0x5788, 0x03},
	{0x5789, 0x00},
	{0x578a, 0x08},
	{0x578b, 0x03},
	{0x578c, 0x00},
	{0x578d, 0x40},
	{0x5790, 0x08},
	{0x5791, 0x04},
	{0x5792, 0x01},
	{0x5793, 0x08},
	{0x5794, 0x04},
	{0x5795, 0x01},
	{0x5799, 0x46},
	{0x57aa, 0x2a},
	{0x5900, 0xf2},
	{0x5901, 0x04},
	{0x5ae0, 0xfe},
	{0x5ae1, 0x40},
	{0x5ae2, 0x38},
	{0x5ae3, 0x30},
	{0x5ae4, 0x0c},
	{0x5ae5, 0x38},
	{0x5ae6, 0x30},
	{0x5ae7, 0x28},
	{0x5ae8, 0x3f},
	{0x5ae9, 0x34},
	{0x5aea, 0x2c},
	{0x5aeb, 0x3f},
	{0x5aec, 0x34},
	{0x5aed, 0x2c},
	{0x36e9, 0x44},
	{0x37f9, 0x44},
	//{0x0100, 0x01},
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

static int sc5336_sensor_vts;
//static int fps_change_flag;

static int sensor_s_exp(struct v4l2_subdev *sd, unsigned int exp_val)
{
	data_type explow, expmid, exphigh;
	struct sensor_info *info = to_state(sd);

	exphigh = (unsigned char) (0x0f & (exp_val>>16));
	expmid = (unsigned char) (0xff & (exp_val>>8));
	explow = (unsigned char) (0xf0 & (exp_val));

	sensor_write(sd, 0x3e02, explow);
	sensor_write(sd, 0x3e01, expmid);
	sensor_write(sd, 0x3e00, exphigh);

	sensor_dbg("sensor_set_exp = %d line Done!\n", exp_val);
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
	data_type anagain = 0x00;
	data_type gaindiglow = 0x80;
	data_type gaindighigh = 0x00;
	int gain_tmp;

	gain_tmp = gain_val << 3;

	if (gain_val < 32) {// 16 * 2
		anagain = 0x00;
		gaindighigh = 0x00;
		gaindiglow = gain_tmp;
	} else if (gain_val < 64) {//16 * 4
		anagain = 0x08;
		gaindighigh = 0x00;
		gaindiglow = gain_tmp * 100 / 200 / 1;
	} else if (gain_val < 128) {//16 * 8
		anagain = 0x09;
		gaindighigh = 0x00;
		gaindiglow = gain_tmp * 100 / 200 / 2;
	} else if (gain_val < 256) {//16 * 16
		anagain = 0x0b;
		gaindighigh = 0x00;
		gaindiglow = gain_tmp * 100 / 200 / 4;
	} else if (gain_val < 512) {//16 * 32
		anagain = 0x0f;
		gaindighigh = 0x00;
		gaindiglow = gain_tmp * 100 / 200 / 8;
	} else if (gain_val < 1024) {//16 * 32 * 2
		anagain = 0x1f;
		gaindighigh = 0x00;
		gaindiglow = gain_tmp * 100 / 200 / 16;
	} else if (gain_val < 2048) {//16 * 32 * 4
		anagain = 0x1f;
		gaindighigh = 0x01;
		gaindiglow = gain_tmp * 100 / 200 / 32;
	} else if (gain_val < 4096) {//16 * 32 * 8
		anagain = 0x1f;
		gaindighigh = 0x03;
		gaindiglow = gain_tmp * 100 / 200 / 64;
	} else if (gain_val < 8192) {//16 * 32 * 16
		anagain = 0x1f;
		gaindighigh = 0x07;
		gaindiglow = gain_tmp * 100 / 200 / 128;
	} else {
		anagain = 0x1f;
		gaindighigh = 0x07;
		gaindiglow = 0xfc;
	}

	sensor_write(sd, 0x3e09, (unsigned char)anagain);
	sensor_write(sd, 0x3e07, (unsigned char)gaindiglow);
	sensor_write(sd, 0x3e06, (unsigned char)gaindighigh);

	sensor_dbg("sensor_set_anagain = %d, 0x%x, 0x%x, 0x%x Done!\n", gain_val, anagain, gaindighigh, gaindiglow);
	//sensor_dbg("digital_gain = 0x%x, 0x%x Done!\n", gaindighigh, gaindiglow);
	info->gain = gain_val;

	return 0;

}

static int sensor_s_exp_gain(struct v4l2_subdev *sd,
			     struct sensor_exp_gain *exp_gain)
{
	struct sensor_info *info = to_state(sd);
	int shutter, frame_length;
	int exp_val, gain_val;

	exp_val = exp_gain->exp_val;
	gain_val = exp_gain->gain_val;
	if (gain_val < 1 * 16)
		gain_val = 16;
	if (exp_val > 0xfffff)
		exp_val = 0xfffff;

	shutter = exp_val >> 4;
	if (shutter > sc5336_sensor_vts - 8) {
		frame_length = shutter + 8;
	} else
		frame_length = sc5336_sensor_vts;
	sensor_write(sd, 0x320f, (frame_length & 0xff));
	sensor_write(sd, 0x320e, (frame_length >> 8));

	//sensor_write(sd, 0x3812, 0x00);//group_hold
	sensor_s_exp(sd, exp_val);
	sensor_s_gain(sd, gain_val);
	//sensor_write(sd, 0x3812, 0x30);
	sensor_dbg("sensor_set_gain exp = %d, %d Done!\n", gain_val, exp_val);

	info->exp = exp_val;
	info->gain = gain_val;
	return 0;
}

static int sensor_flip_status;
static int sensor_s_vflip(struct v4l2_subdev *sd, int enable)
{
	data_type get_value;
	data_type set_value;

	if (!(enable == 0 || enable == 1))
		return -1;

	sensor_read(sd, 0x3221, &get_value);
	sensor_dbg("ready to vflip, regs_data = 0x%x\n", get_value);

	if (enable) {
		set_value = get_value | 0x60;
		sensor_flip_status |= 0x60;
	} else {
		set_value = get_value & 0x9f;
		sensor_flip_status &= 0x9f;
	}
	sensor_write(sd, 0x3221, set_value);
	return 0;
}

static int sensor_s_hflip(struct v4l2_subdev *sd, int enable)
{
	data_type get_value;
	data_type set_value;

	if (!(enable == 0 || enable == 1))
		return -1;

	sensor_read(sd, 0x3221, &get_value);
	sensor_dbg("ready to hflip, regs_data = 0x%x\n", get_value);

	if (enable) {
		set_value = get_value | 0x06;
		sensor_flip_status |= 0x06;
	} else {
		set_value = get_value & 0xf9;
		sensor_flip_status &= 0xf9;
	}
	sensor_write(sd, 0x3221, set_value);
	return 0;
}

static int sensor_get_fmt_mbus_core(struct v4l2_subdev *sd, int *code)
{
	*code = MEDIA_BUS_FMT_SBGGR10_1X10;

	return 0;
}

static int sensor_s_fps(struct v4l2_subdev *sd,
			struct sensor_fps *fps)
{
#if 0
	data_type rdval1, rdval2, rdval3;
	struct sensor_info *info = to_state(sd);
	struct sensor_win_size *wsize = info->current_wins;
	int sc5336_sensor_target_vts = 0;

	sc5336_sensor_vts = wsize->pclk/fps->fps/wsize->hts;
	fps_change_flag = 1;

	sensor_dbg("sc5336_sensor_vts: %d, vts: %d.\n", sc5336_sensor_vts, (rdval1 | (rdval2<<8) | (rdval3<<16)));
#endif
	return 0;
}
#if 0
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
#endif
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
		usleep_range(1000, 1200);
		vin_gpio_write(sd, PWDN, CSI_GPIO_LOW);
		usleep_range(1000, 1200);
		cci_unlock(sd);
		usleep_range(1000, 1200);
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
		//vin_gpio_set_status(sd, POWER_EN, 1);
		vin_gpio_write(sd, PWDN, CSI_GPIO_LOW);
		vin_gpio_write(sd, RESET, CSI_GPIO_LOW);
		usleep_range(1000, 1200);
		//vin_gpio_write(sd, POWER_EN, CSI_GPIO_HIGH);
		//vin_set_pmu_channel(sd, CMBCSI, ON);
		vin_set_pmu_channel(sd, IOVDD, ON);
		usleep_range(1000, 1200);
		vin_set_pmu_channel(sd, DVDD, ON);
		usleep_range(1000, 1200);
		vin_set_pmu_channel(sd, AVDD, ON);
		usleep_range(1000, 1200);
		vin_gpio_write(sd, PWDN, CSI_GPIO_HIGH);
		usleep_range(1000, 1200);
		vin_gpio_write(sd, RESET, CSI_GPIO_HIGH);
		usleep_range(1000, 1200);
		cci_unlock(sd);
		break;

	case PWR_OFF:
		sensor_dbg("PWR_OFF!do nothing\n");
		cci_lock(sd);
		vin_set_mclk(sd, OFF);
		//vin_gpio_write(sd, POWER_EN, CSI_GPIO_LOW);
		//vin_set_pmu_channel(sd, CMBCSI, OFF);
		vin_set_pmu_channel(sd, AVDD, OFF);
		vin_set_pmu_channel(sd, DVDD, OFF);
		vin_set_pmu_channel(sd, IOVDD, OFF);
		usleep_range(1000, 1200);
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
	switch (val) {
	case 0:
		vin_gpio_write(sd, RESET, CSI_GPIO_HIGH);
		usleep_range(100, 120);
		break;
	case 1:
		vin_gpio_write(sd, RESET, CSI_GPIO_LOW);
		usleep_range(100, 120);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int sensor_detect(struct v4l2_subdev *sd)
{
#if !defined CONFIG_VIN_INIT_MELIS
	data_type rdval = 0;

	sensor_read(sd, 0x3107, &rdval);
	sensor_print("0x3107 = 0x%x\n", rdval);
	if (rdval != 0xce)
		return -ENODEV;
	sensor_read(sd, 0x3108, &rdval);
	sensor_print("0x3108 = 0x%x\n", rdval);
	if (rdval != 0x50)
		return -ENODEV;

#endif
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
	info->width = 2880;
	info->height = 1620;
	info->hflip = 0;
	info->vflip = 0;
	info->gain = 0;
	info->exp = 0;

	info->tpf.numerator = 1;
	info->tpf.denominator = 20;	/* 30fps */

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
	case VIDIOC_VIN_SET_IR:
		sensor_set_ir(sd, (struct ir_switch *)arg);
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
		.mbus_code = MEDIA_BUS_FMT_SBGGR10_1X10,
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
	{
	 .width = 2880,
	 .height = 1620,
	 .hoffset = 0,
	 .voffset = 0,
	 .hts = 4800,
	 .vts = 1650,
	 .pclk = 158400000,
	 .mipi_bps = 792 * 1000 * 1000,
	 .fps_fixed = 20,
	 .bin_factor = 1,
	 .intg_min = 1 << 4,
	 .intg_max = (1650 - 8) << 4,
	 .gain_min = 1 << 4,
	 .gain_max = 512 << 4,
	 .regs = sensor_2880_1620p20_regs,
	 .regs_size = ARRAY_SIZE(sensor_2880_1620p20_regs),
	 .set_size = NULL,
	},

	{
	 .width = 2880,
	 .height = 1620,
	 .hoffset = 0,
	 .voffset = 0,
	 .hts = 4800,
	 .vts = 2200,
	 .pclk = 158400000,
	 .mipi_bps = 792 * 1000 * 1000,
	 .fps_fixed = 15,
	 .bin_factor = 1,
	 .intg_min = 1 << 4,
	 .intg_max = (2200 - 8) << 4,
	 .gain_min = 1 << 4,
	 .gain_max = 512 << 4,
	 .regs = sensor_2880_1620p15_regs,
	 .regs_size = ARRAY_SIZE(sensor_2880_1620p15_regs),
	 .set_size = NULL,
	},

	{
	 .width = 1440,
	 .height = 400,
	 .hoffset = 0,
	 .voffset = 0,
	 .hts = 3028,
	 .vts = 439,
	 .pclk = 172800000,
	 .mipi_bps = 432 * 1000 * 1000,
	 .fps_fixed = 130,
	 .bin_factor = 1,
	 .intg_min = 1 << 4,
	 .intg_max = (439 - 8) << 4,
	 .gain_min = 1 << 4,
	 .gain_max = 512 << 4,
	 .regs = sensor_1440_400p130_regs,
	 .regs_size = ARRAY_SIZE(sensor_1440_400p130_regs),
	 .set_size = NULL,
	},
};

#define N_WIN_SIZES (ARRAY_SIZE(sensor_win_sizes))

static int sensor_g_mbus_config(struct v4l2_subdev *sd,
				struct v4l2_mbus_config *cfg)
{
	//struct sensor_info *info = to_state(sd);

	cfg->type = V4L2_MBUS_CSI2;
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
	data_type rdval;
	struct v4l2_subdev *sd = &info->sd;
	struct sensor_format_struct *sensor_fmt = info->fmt;
	struct sensor_win_size *wsize = info->current_wins;
	struct sensor_exp_gain exp_gain;

	ret = sensor_write_array(sd, sensor_default_regs,
				 ARRAY_SIZE(sensor_default_regs));
	if (ret < 0) {
		sensor_err("write sensor_default_regs error\n");
		return ret;
	}

	sensor_dbg("sensor_reg_init\n");

	sensor_write_array(sd, sensor_fmt->regs, sensor_fmt->regs_size);

#if defined CONFIG_VIN_INIT_MELIS
	if (info->preview_first_flag) {
		info->preview_first_flag = 0;
	} else {
		if (wsize->regs) {
			sensor_write_array(sd, wsize->regs, wsize->regs_size);
			sensor_read(sd, 0x3040, &rdval);
			if (rdval == 0x00) {
				sensor_write(sd, 0x3258, 0x0c);
				sensor_write(sd, 0x3249, 0x0b);
				sensor_write(sd, 0x3934, 0x0a);
				sensor_write(sd, 0x3935, 0x00);
				sensor_write(sd, 0x3937, 0x75);
				sensor_write(sd, 0x393d, 0x07);
				sensor_write(sd, 0x393e, 0xff);
			} else if (rdval == 0x03) {
				sensor_write(sd, 0x3258, 0x08);
				sensor_write(sd, 0x3249, 0x07);
				sensor_write(sd, 0x3934, 0x05);
				sensor_write(sd, 0x3935, 0x07);
				sensor_write(sd, 0x3937, 0x74);
				sensor_write(sd, 0x393d, 0x01);
				sensor_write(sd, 0x393e, 0xa4);
			}
			sensor_write(sd, 0x0100, 0x01);
		}
		if (info->exp && info->gain) {
			exp_gain.exp_val = info->exp;
			exp_gain.gain_val = info->gain;
		} else {
			exp_gain.exp_val = 15408;
			exp_gain.gain_val = 32;
		}
		sensor_s_exp_gain(sd, &exp_gain);
	}
#else
	if (wsize->regs) {
		sensor_write_array(sd, wsize->regs, wsize->regs_size);
		sensor_read(sd, 0x3040, &rdval);
		if (rdval == 0x00) {
			sensor_write(sd, 0x3258, 0x0c);
			sensor_write(sd, 0x3249, 0x0b);
			sensor_write(sd, 0x3934, 0x0a);
			sensor_write(sd, 0x3935, 0x00);
			sensor_write(sd, 0x3937, 0x75);
			sensor_write(sd, 0x393d, 0x07);
			sensor_write(sd, 0x393e, 0xff);
		} else if (rdval == 0x03) {
			sensor_write(sd, 0x3258, 0x08);
			sensor_write(sd, 0x3249, 0x07);
			sensor_write(sd, 0x3934, 0x05);
			sensor_write(sd, 0x3935, 0x07);
			sensor_write(sd, 0x3937, 0x74);
			sensor_write(sd, 0x393d, 0x01);
			sensor_write(sd, 0x393e, 0xa4);
		}
		sensor_write(sd, 0x0100, 0x01);
	}
#endif
	if (wsize->set_size)
		wsize->set_size(sd);

	info->width = wsize->width;
	info->height = wsize->height;
	sensor_flip_status = 0x0;
	sc5336_sensor_vts = wsize->vts;

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
	if (ctrl != NULL)
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE;

	v4l2_ctrl_new_std(handler, ops, V4L2_CID_HFLIP, 0, 1, 1, 0);
	v4l2_ctrl_new_std(handler, ops, V4L2_CID_VFLIP, 0, 1, 1, 0);

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
	info->combo_mode = CMB_TERMINAL_RES | CMB_PHYA_OFFSET1 | MIPI_NORMAL_MODE;
	info->stream_seq = MIPI_BEFORE_SENSOR;
	info->af_first_flag = 1;
	info->preview_first_flag = 1;
	info->first_power_flag = 1;
	info->exp = 0;
	info->gain = 0;
	info->first_power_flag = 1;

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

#ifdef CONFIG_SUNXI_FASTBOOT
subsys_initcall_sync(init_sensor);
#else
module_init(init_sensor);
#endif
module_exit(exit_sensor);

