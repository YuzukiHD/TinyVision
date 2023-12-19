/*
 * A V4L2 driver for gc4023_mipi cameras.
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
MODULE_DESCRIPTION("A low-level driver for GC4023 sensors");
MODULE_LICENSE("GPL");

#define MCLK              (24*1000*1000)
#define V4L2_IDENT_SENSOR  0x4023

/*
 * Our nominal (default) frame rate.
 */
#define SENSOR_FRAME_RATE 25

/*
 * The GC4023 i2c address
 */
#define I2C_ADDR 0x52

#define SENSOR_NAME  "gc4023_mipi"

#define GC4023_MIRROR_NORMAL
//#define GC4023_MIRROR_H
//#define GC4023_MIRROR_V
//#define GC4023_MIRROR_HV

#if defined(GC4023_MIRROR_NORMAL)
#define     MIRROR 0x00
#define OTP_MIRROR 0x60
#elif defined(GC4023_MIRROR_H)
#define     MIRROR 0x01
#define OTP_MIRROR 0x61
#elif defined(GC4023_MIRROR_V)
#define     MIRROR 0x02
#define OTP_MIRROR 0x62
#elif defined(GC4023_MIRROR_HV)
#define     MIRROR 0x03
#define OTP_MIRROR 0x63
#else
#define     MIRROR 0x00
#define OTP_MIRROR 0x60
#endif

/*
 * The default register settings
 */

static struct regval_list sensor_default_regs[] = {

};

static struct regval_list sensor_2560x1400p25_regs[] = {
/* 2560 x 1400 @25fps */
	{0x03fe, 0xf0},
	{0x03fe, 0x00},
	{0x03fe, 0x10},
	{0x03fe, 0x00},
	{0x0a38, 0x00},
	{0x0a38, 0x01},
	{0x0a20, 0x07},
	{0x061c, 0x50},
	{0x061d, 0x22},
	{0x061e, 0x87},
	{0x061f, 0x06},
	{0x0a21, 0x10},
	{0x0a34, 0x40},
	{0x0a35, 0x01},
	{0x0a36, 0x58},
	{0x0a37, 0x06},
	{0x0314, 0x50},
	{0x0315, 0x00},
	{0x031c, 0xce},
	{0x0219, 0x47},
	{0x0342, 0x04},
	{0x0343, 0xb0},
	{0x0259, 0x05},
	{0x025a, 0xa0},
	{0x0340, 0x08},
	{0x0341, 0xca},
	{0x0347, 0x02},
	{0x0348, 0x0a},
	{0x0349, 0x08},
	{0x034a, 0x05},
	{0x034b, 0xa8},
/* out_win */
	{0x0094, 0x0a},
	{0x0095, 0x00},
	{0x0096, 0x05},
	{0x0097, 0xa0},
/* test image */
/* 	{0x008c, 0x10}, */

	{0x0099, 0x04},
	{0x009b, 0x04},
	{0x060c, 0x01},
	{0x060e, 0x08},
	{0x060f, 0x05},
	{0x070c, 0x01},
	{0x070e, 0x08},
	{0x070f, 0x05},
	{0x0909, 0x03},
	{0x0902, 0x04},
	{0x0904, 0x0b},
	{0x0907, 0x54},
	{0x0908, 0x06},
	{0x0903, 0x9d},
	{0x072a, 0x18},
	{0x0724, 0x0a},
	{0x0727, 0x0a},
	{0x072a, 0x1c},
	{0x072b, 0x0a},
	{0x1466, 0x10},
	{0x1468, 0x0b},
	{0x1467, 0x13},
	{0x1469, 0x80},
	{0x146a, 0xe8},
	{0x0707, 0x07},
	{0x0737, 0x0f},
	{0x0704, 0x01},
	{0x0706, 0x03},
	{0x0716, 0x03},
	{0x0708, 0xc8},
	{0x0718, 0xc8},
	{0x061a, 0x00},
	{0x1430, 0x80},
	{0x1407, 0x10},
	{0x1408, 0x16},
	{0x1409, 0x03},
	{0x146d, 0x0e},
	{0x146e, 0x42},
	{0x146f, 0x43},
	{0x1470, 0x3c},
	{0x1471, 0x3d},
	{0x1472, 0x3a},
	{0x1473, 0x3a},
	{0x1474, 0x40},
	{0x1475, 0x46},
	{0x1420, 0x14},
	{0x1464, 0x15},
	{0x146c, 0x40},
	{0x146d, 0x40},
	{0x1423, 0x08},
	{0x1428, 0x10},
	{0x1462, 0x18},
	{0x02ce, 0x04},
	{0x143a, 0x0f},
	{0x142b, 0x88},
	{0x0245, 0xc9},
	{0x023a, 0x08},
	{0x02cd, 0x99},
	{0x0612, 0x02},
	{0x0613, 0xc7},
	{0x0243, 0x03},
	{0x021b, 0x09},
	{0x0089, 0x03},
	{0x0040, 0xa3},
	{0x0075, 0x64},
	{0x0004, 0x0f},
	{0x0002, 0xab},
	{0x0053, 0x0a},
	{0x0205, 0x0c},
	{0x0202, 0x06},
	{0x0203, 0x27},
	{0x0614, 0x00},
	{0x0615, 0x00},
	{0x0181, 0x0c},
	{0x0182, 0x05},
	{0x0185, 0x01},
	{0x0180, 0x46},
	{0x0100, 0x08},
	{0x0106, 0x38},
	{0x010d, 0x80},
	{0x010e, 0x0c},
	{0x0113, 0x02},
	{0x0114, 0x01},
	{0x0115, 0x10},
	{0x022c, MIRROR},
	{0x0100, 0x09},
	{0x0a67, 0x80},
	{0x0a54, 0x0e},
	{0x0a65, 0x10},
	{0x0a98, 0x10},
	{0x05be, 0x00},
	{0x05a9, 0x01},
	{0x0029, 0x08},
	{0x002b, 0xa8},
	{0x0a83, 0xe0},
	{0x0a72, 0x02},
	{0x0a73, OTP_MIRROR},
	{0x0a75, 0x41},
	{0x0a70, 0x03},
	{0x0a5a, 0x80},
	{REG_DLY, 0x14},
	{0x05be, 0x01},
	{0x0a70, 0x00},
	{0x0080, 0x02},
	{0x0a67, 0x00},
};

static struct regval_list sensor_2560x1400p20_regs[] = {
/* 2560 x 1400 @20fps */
	{0x03fe, 0xf0},
	{0x03fe, 0x00},
	{0x03fe, 0x10},
	{0x03fe, 0x00},
	{0x0a38, 0x00},
	{0x0a38, 0x01},
	{0x0a20, 0x07},
	{0x061c, 0x50},
	{0x061d, 0x22},
	{0x061e, 0x87},
	{0x061f, 0x06},
	{0x0a21, 0x10},
	{0x0a34, 0x40},
	{0x0a35, 0x01},
	{0x0a36, 0x58},
	{0x0a37, 0x06},
	{0x0314, 0x50},
	{0x0315, 0x00},
	{0x031c, 0xce},
	{0x0219, 0x47},
	{0x0342, 0x04},
	{0x0343, 0xb0},
	{0x0259, 0x05},
	{0x025a, 0xa0},
	{0x0340, 0x08},
	{0x0341, 0xca},
	{0x0347, 0x02},
	{0x0348, 0x0a},
	{0x0349, 0x08},
	{0x034a, 0x05},
	{0x034b, 0xa8},
/* out_win */
	{0x0094, 0x0a},
	{0x0095, 0x00},
	{0x0096, 0x05},
	{0x0097, 0xa0},
/* test image */
/* 	{0x008c, 0x10}, */

	{0x0099, 0x04},
	{0x009b, 0x04},
	{0x060c, 0x01},
	{0x060e, 0x08},
	{0x060f, 0x05},
	{0x070c, 0x01},
	{0x070e, 0x08},
	{0x070f, 0x05},
	{0x0909, 0x03},
	{0x0902, 0x04},
	{0x0904, 0x0b},
	{0x0907, 0x54},
	{0x0908, 0x06},
	{0x0903, 0x9d},
	{0x072a, 0x18},
	{0x0724, 0x0a},
	{0x0727, 0x0a},
	{0x072a, 0x1c},
	{0x072b, 0x0a},
	{0x1466, 0x10},
	{0x1468, 0x0b},
	{0x1467, 0x13},
	{0x1469, 0x80},
	{0x146a, 0xe8},
	{0x0707, 0x07},
	{0x0737, 0x0f},
	{0x0704, 0x01},
	{0x0706, 0x03},
	{0x0716, 0x03},
	{0x0708, 0xc8},
	{0x0718, 0xc8},
	{0x061a, 0x00},
	{0x1430, 0x80},
	{0x1407, 0x10},
	{0x1408, 0x16},
	{0x1409, 0x03},
	{0x146d, 0x0e},
	{0x146e, 0x42},
	{0x146f, 0x43},
	{0x1470, 0x3c},
	{0x1471, 0x3d},
	{0x1472, 0x3a},
	{0x1473, 0x3a},
	{0x1474, 0x40},
	{0x1475, 0x46},
	{0x1420, 0x14},
	{0x1464, 0x15},
	{0x146c, 0x40},
	{0x146d, 0x40},
	{0x1423, 0x08},
	{0x1428, 0x10},
	{0x1462, 0x18},
	{0x02ce, 0x04},
	{0x143a, 0x0f},
	{0x142b, 0x88},
	{0x0245, 0xc9},
	{0x023a, 0x08},
	{0x02cd, 0x99},
	{0x0612, 0x02},
	{0x0613, 0xc7},
	{0x0243, 0x03},
	{0x021b, 0x09},
	{0x0089, 0x03},
	{0x0040, 0xa3},
	{0x0075, 0x64},
	{0x0004, 0x0f},
	{0x0002, 0xab},
	{0x0053, 0x0a},
	{0x0205, 0x0c},
	{0x0202, 0x06},
	{0x0203, 0x27},
	{0x0614, 0x00},
	{0x0615, 0x00},
	{0x0181, 0x0c},
	{0x0182, 0x05},
	{0x0185, 0x01},
	{0x0180, 0x46},
	{0x0100, 0x08},
	{0x0106, 0x38},
	{0x010d, 0x80},
	{0x010e, 0x0c},
	{0x0113, 0x02},
	{0x0114, 0x01},
	{0x0115, 0x10},
	{0x022c, MIRROR},
	{0x0100, 0x09},
	{0x0a67, 0x80},
	{0x0a54, 0x0e},
	{0x0a65, 0x10},
	{0x0a98, 0x10},
	{0x05be, 0x00},
	{0x05a9, 0x01},
	{0x0029, 0x08},
	{0x002b, 0xa8},
	{0x0a83, 0xe0},
	{0x0a72, 0x02},
	{0x0a73, OTP_MIRROR},
	{0x0a75, 0x41},
	{0x0a70, 0x03},
	{0x0a5a, 0x80},
	{REG_DLY, 0x14},
	{0x05be, 0x01},
	{0x0a70, 0x00},
	{0x0080, 0x02},
	{0x0a67, 0x00},

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
	unsigned int frame_length;
	unsigned int tmp_exp_val = exp_val / 16;

	if (tmp_exp_val > 0xfff6)
		tmp_exp_val = 0xfff6;
	else if (tmp_exp_val < 1)
		tmp_exp_val = 1;

	frame_length = (tmp_exp_val + 8) > 2612 ? (tmp_exp_val + 8) : 2612;
	if (frame_length > 0xffff)
		frame_length = 0xffff;

	sensor_write(sd, 0x0340, ((frame_length >> 8) & 0xff));
	sensor_write(sd, 0x0341, (frame_length & 0xff));

	sensor_dbg("exp_val:%d\n", exp_val);
	sensor_write(sd, 0x202, ((tmp_exp_val >> 8) & 0xFF));
	sensor_write(sd, 0x203, (tmp_exp_val & 0xFF));

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
/* 0x0614 0x0615 0x0218 0x1467 0x1468 0x00b8 0x00b9 */
	{0x00, 0x00, 0x00, 0x0D, 0x15, 0x01, 0x00},
	{0x80, 0x02, 0x00, 0x0D, 0x15, 0x01, 0x0B},
	{0x01, 0x00, 0x00, 0x0D, 0x15, 0x01, 0x19},
	{0x81, 0x02, 0x00, 0x0E, 0x16, 0x01, 0x2A},
	{0x02, 0x00, 0x00, 0x0E, 0x16, 0x02, 0x00},
	{0x82, 0x02, 0x00, 0x0F, 0x17, 0x02, 0x17},
	{0x03, 0x00, 0x00, 0x10, 0x18, 0x02, 0x33},
	{0x83, 0x02, 0x00, 0x11, 0x19, 0x03, 0x14},
	{0x04, 0x00, 0x00, 0x12, 0x1a, 0x04, 0x00},
	{0x80, 0x02, 0x20, 0x13, 0x1b, 0x04, 0x2F},
	{0x01, 0x00, 0x20, 0x14, 0x1c, 0x05, 0x26},
	{0x81, 0x02, 0x20, 0x15, 0x1d, 0x06, 0x28},
	{0x02, 0x00, 0x20, 0x16, 0x1e, 0x08, 0x00},
	{0x82, 0x02, 0x20, 0x16, 0x1e, 0x09, 0x1E},
	{0x03, 0x00, 0x20, 0x18, 0x20, 0x0B, 0x0C},
	{0x83, 0x02, 0x20, 0x18, 0x20, 0x0D, 0x11},
	{0x04, 0x00, 0x20, 0x18, 0x20, 0x10, 0x00},
	{0x84, 0x02, 0x20, 0x19, 0x21, 0x12, 0x3D},
	{0x05, 0x00, 0x20, 0x19, 0x21, 0x16, 0x19},
	{0x85, 0x02, 0x20, 0x1A, 0x22, 0x1A, 0x22},
	{0xb5, 0x04, 0x20, 0x1B, 0x23, 0x20, 0x00},
	{0x85, 0x05, 0x20, 0x1B, 0x23, 0x25, 0x3A},
	{0x05, 0x08, 0x20, 0x1C, 0x24, 0x2C, 0x33},
	{0x45, 0x09, 0x20, 0x1D, 0x25, 0x35, 0x05},
	{0x55, 0x0a, 0x20, 0x1F, 0x27, 0x40, 0x00},
};

unsigned int gainLevelTable[25] = {
	64, 76, 90, 106, 128, 152,
	179, 212, 256, 303, 358, 425,
	512, 607, 717, 849, 1024, 1213,
	1434, 1699, 2048, 2208, 2867,
	3398, 4096,
};

static int SetSensorGain(struct v4l2_subdev *sd, int gain)
{
	int i, total;
	unsigned int tol_dig_gain = 0;

	total = sizeof(gainLevelTable) / sizeof(unsigned int);
	for (i = 0; i < total - 1; i++) {
	   if ((gainLevelTable[i] <= gain) && (gain < gainLevelTable[i + 1]))
	     break;
	}

	tol_dig_gain = gain * 64 / gainLevelTable[i];

	sensor_write(sd, 0x0614, regValTable[i][0]);
	sensor_write(sd, 0x0615, regValTable[i][1]);
	sensor_write(sd, 0x0218, regValTable[i][2]);
	sensor_write(sd, 0x1467, regValTable[i][3]);
	sensor_write(sd, 0x1468, regValTable[i][4]);
	sensor_write(sd, 0x00b8, regValTable[i][5]);
	sensor_write(sd, 0x00b9, regValTable[i][6]);

	sensor_write(sd, 0x0064, (tol_dig_gain>>6));
	sensor_write(sd, 0x0065, (tol_dig_gain&0x3f)<<2);

	return 0;
}

static int sensor_s_gain(struct v4l2_subdev *sd, int gain_val)
{
	struct sensor_info *info = to_state(sd);

	if (gain_val == info->gain) {
		return 0;
	}

	if (gain_val < (1 * 16))
		gain_val = 16;
	else if (gain_val > 1024)
		gain_val = 1024;

	sensor_dbg("gain_val:%d\n", gain_val);
	SetSensorGain(sd, gain_val * 4);

	info->gain = gain_val;

	return 0;
}


static int sensor_s_exp_gain(struct v4l2_subdev *sd,
				 struct sensor_exp_gain *exp_gain)
{
	/* struct sensor_info *info = to_state(sd); */

	sensor_s_exp(sd, exp_gain->exp_val);
	sensor_s_gain(sd, exp_gain->gain_val);

	return 0;
}

static int sensor_get_fmt_mbus_core(struct v4l2_subdev *sd, int *code)
{
	struct sensor_info *info = to_state(sd);

	*code = info->fmt->mbus_code;

	return 0;
}

static int sensor_s_sw_stby(struct v4l2_subdev *sd, int on_off)
{
	int ret = 0;

	return ret;
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
		sensor_print("PWR_ON!\n");

		cci_lock(sd);

		vin_gpio_set_status(sd, PWDN, 1);
		vin_gpio_set_status(sd, RESET, 1);

		vin_gpio_write(sd, PWDN, CSI_GPIO_LOW);
		vin_gpio_write(sd, RESET, CSI_GPIO_LOW);

		usleep_range(30000, 31000);

		vin_set_mclk_freq(sd, MCLK);
		vin_set_mclk(sd, ON);
		usleep_range(10000, 12000);

		vin_set_pmu_channel(sd, IOVDD, ON);
		usleep_range(10000, 12000);
		vin_set_pmu_channel(sd, DVDD, ON);
		usleep_range(10000, 12000);
		vin_set_pmu_channel(sd, AVDD, ON);
		usleep_range(10000, 12000);

		vin_gpio_write(sd, PWDN, CSI_GPIO_HIGH);
		usleep_range(10000, 12000);

		vin_gpio_write(sd, RESET, CSI_GPIO_HIGH);
		usleep_range(30000, 31000);
		cci_unlock(sd);
		break;
	case PWR_OFF:
		sensor_print("PWR_OFF!\n");
		cci_lock(sd);

		vin_set_mclk(sd, OFF);
		vin_gpio_write(sd, PWDN, CSI_GPIO_LOW);
		vin_gpio_write(sd, RESET, CSI_GPIO_LOW);

		vin_set_pmu_channel(sd, AVDD, OFF);
		vin_set_pmu_channel(sd, DVDD, OFF);
		vin_set_pmu_channel(sd, IOVDD, OFF);

		vin_gpio_set_status(sd, PWDN, 0);
		vin_gpio_set_status(sd, RESET, 0);

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
	data_type rdval = 0;
	sensor_read(sd, 0x03f0, &rdval);
	sensor_dbg(" read 0x03f0 return 0x%x\n", rdval);
	if (rdval != (V4L2_IDENT_SENSOR >> 8)) {
		sensor_err(" read 0x03f0 return 0x%x\n", rdval);
		return -ENODEV;
	}

	sensor_read(sd, 0x03f1, &rdval);
	sensor_dbg(" read 0x03f1 return 0x%x\n", rdval);
	if (rdval != (V4L2_IDENT_SENSOR & 0xff)) {
		sensor_err(" read 0x03f1 return 0x%x\n", rdval);
		return -ENODEV;
	}

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
	info->low_speed    = 0;
	info->width        = 2560;
	info->height       = 1440;
	info->hflip        = 0;
	info->vflip        = 0;
	info->gain         = 0;
	info->exp          = 0;

	info->tpf.numerator      = 1;
	info->tpf.denominator    = SENSOR_FRAME_RATE;	/* 25fps */
	info->preview_first_flag = 1;
	return 0;
}

static long sensor_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	int ret = 0;
	struct sensor_info *info = to_state(sd);
	printk("%u\n", cmd);
	switch (cmd) {

	case GET_CURRENT_WIN_CFG:
		sensor_print("%s: GET_CURRENT_WIN_CFG, info->current_wins=%p\n", __func__, info->current_wins);

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
		sensor_s_exp_gain(sd, (struct sensor_exp_gain *)arg);
		break;
	case VIDIOC_VIN_SENSOR_CFG_REQ:
		sensor_cfg_req(sd, (struct sensor_config *)arg);
		break;
	case VIDIOC_VIN_ACT_SET_CODE:
		actuator_set_code(sd, (struct actuator_ctrl *)arg);
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
		.mbus_code = MEDIA_BUS_FMT_SRGGB10_1X10, /* .mbus_code = MEDIA_BUS_FMT_SBGGR10_1X10, */
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
		.width      = 2560,
		.height     = 1440,
		.hoffset    = 0,
		.voffset    = 0,
		.hts        = 1200,
		.vts        = 1800,
		.pclk       = 54 * 1000 * 1000,
		.mipi_bps   = 704 * 1000 * 1000,
		.fps_fixed  = 25,
		.bin_factor = 1,
		.intg_min   = 1 << 4,
		.intg_max   = 4095 << 4,
		.gain_min   = 1 << 4,
		.gain_max   = 256 << 4,
		.regs       = sensor_2560x1400p25_regs,
		.regs_size  = ARRAY_SIZE(sensor_2560x1400p25_regs),
		.set_size   = NULL,
	},
	{
		.width      = 2560,
		.height     = 1440,
		.hoffset    = 0,
		.voffset    = 0,
		.hts        = 1200,
		.vts        = 2250,
		.pclk       = 54 * 1000 * 1000,
		.mipi_bps   = 704 * 1000 * 1000,
		.fps_fixed  = 20,
		.bin_factor = 1,
		.intg_min   = 1 << 4,
		.intg_max   = 4095 << 4,
		.gain_min   = 1 << 4,
		.gain_max   = 256 << 4,
		.regs       = sensor_2560x1400p20_regs,
		.regs_size  = ARRAY_SIZE(sensor_2560x1400p20_regs),
		.set_size   = NULL,
	},
};

#define N_WIN_SIZES (ARRAY_SIZE(sensor_win_sizes))

static int sensor_g_mbus_config(struct v4l2_subdev *sd,
				struct v4l2_mbus_config *cfg)
{

	cfg->type  = V4L2_MBUS_CSI2;
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

static int sensor_queryctrl(struct v4l2_subdev *sd, struct v4l2_queryctrl *qc)
{
	/* Fill in min, max, step and default value for these controls. */
	/* see include/linux/videodev2.h for details */
	switch (qc->id) {
	case V4L2_CID_GAIN:
		return v4l2_ctrl_query_fill(qc, 1 * 16, 128 * 16 - 1, 1, 16);
	case V4L2_CID_EXPOSURE:
		return v4l2_ctrl_query_fill(qc, 1, 65536 * 16, 1, 1);
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
	}
	return -EINVAL;
}

static int sensor_reg_init(struct sensor_info *info)
{
	int ret;
	struct v4l2_subdev *sd = &info->sd;
	struct sensor_format_struct *sensor_fmt = info->fmt;
	struct sensor_win_size *wsize = info->current_wins;

	sensor_dbg("ARRAY_SIZE(sensor_default_regs)=%d\n",
			(unsigned int)ARRAY_SIZE(sensor_default_regs));

	ret = sensor_write_array(sd, sensor_default_regs,
				 ARRAY_SIZE(sensor_default_regs));
	if (ret < 0) {
		sensor_err("write sensor_default_regs error\n");
		return ret;
	}

	sensor_write_array(sd, sensor_fmt->regs, sensor_fmt->regs_size);

	if (wsize->regs) {
		sensor_write_array(sd, wsize->regs, wsize->regs_size);
	}

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
	.addr_width = CCI_BITS_16,
	.data_width = CCI_BITS_8,
};

static int sensor_init_controls(struct v4l2_subdev *sd, const struct v4l2_ctrl_ops *ops)
{
	struct sensor_info *info = to_state(sd);
	struct v4l2_ctrl_handler *handler = &info->handler;
	struct v4l2_ctrl *ctrl;
	int ret = 0;

	v4l2_ctrl_handler_init(handler, 2);

	ctrl = v4l2_ctrl_new_std(handler, ops, V4L2_CID_GAIN, 1 * 1600,
				  256 * 1600, 1, 1 * 1600);

	if (ctrl != NULL)
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE;

	ctrl = v4l2_ctrl_new_std(handler, ops, V4L2_CID_EXPOSURE, 1,
				  65536 * 16, 1, 1);
	if (ctrl != NULL)
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE;

	if (handler->error) {
		ret = handler->error;
		v4l2_ctrl_handler_free(handler);
	}

	sd->ctrl_handler = handler;

	return ret;
}

static int sensor_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct v4l2_subdev *sd;
	struct sensor_info *info;

	info = kzalloc(sizeof(*info), GFP_KERNEL);
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
	return cci_dev_init_helper(&sensor_driver);
}

static __exit void exit_sensor(void)
{
	cci_dev_exit_helper(&sensor_driver);
}

module_init(init_sensor);
module_exit(exit_sensor);
