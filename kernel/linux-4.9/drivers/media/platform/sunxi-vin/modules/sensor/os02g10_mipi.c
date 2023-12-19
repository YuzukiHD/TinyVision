/*
 * A V4L2 driver for Raw cameras.
 *
 * Copyright (c) 2022 by Allwinnertech Co., Ltd.  http://www.allwinnertech.com
 *
 * Authors: Liang WeiJie <liangweijie@allwinnertech.com>
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
MODULE_DESCRIPTION("A low-level driver for OS02G10 sensors");
MODULE_LICENSE("GPL");

#define MCLK              (24*1000*1000)
#define V4L2_IDENT_SENSOR 0x5602

#define EXP_HIGH		0x03
#define EXP_LOW			0x04
#define ID_REG_HIGH		0x02
#define ID_REG_LOW		0x03
#define ID_VAL_HIGH		((V4L2_IDENT_SENSOR) >> 8)
#define ID_VAL_LOW		((V4L2_IDENT_SENSOR) & 0xff)
#define SENSOR_FRAME_RATE 20

static int light_nums;  //reset,pwdn set status times
static struct mutex light_lock;

/*
 * The OS02G10 i2c address
 */
#define I2C_ADDR 0x78

#define SENSOR_NUM 0x2
#define SENSOR_NAME "os02g10_mipi"
#define SENSOR_NAME_2 "os02g10_mipi_2"

/*
 * The default register settings
 */

static struct regval_list sensor_default_regs[] = {
	{0xfd, 0x00},
	{0x36, 0x01},
	{0xfd, 0x00},
	{0x36, 0x00},
	{0xfd, 0x00},
	{0x20, 0x00},
	{0xffff, 0x85},
	{0xfd, 0x00},
	{0xfd, 0x00},
	{0x30, 0x0a},
	{0x35, 0x04},
	{0x38, 0x11},
	{0x41, 0x06},
	{0x44, 0x20},
	{0xfd, 0x01},
	{0x03, 0x02},
	{0x04, 0x4c},
	{0x06, 0x00},
	{0x24, 0x30},
	//{0x01, 0x01},
	{0x19, 0x50},
	{0x1a, 0x0c},
	{0x1b, 0x0d},
	{0x1c, 0x00},
	{0x1d, 0x75},
	{0x1e, 0x52},
	{0x22, 0x14},
	{0x25, 0x44},
	{0x26, 0x0f},
	{0x3c, 0xca},
	{0x3d, 0x4a},
	{0x40, 0x0f},
	{0x43, 0x38},
	{0x46, 0x00},
	{0x47, 0x00},
	{0x49, 0x32},
	{0x50, 0x01},
	{0x51, 0x28},
	{0x52, 0x20},
	{0x53, 0x03},
	{0x57, 0x16},
	{0x59, 0x01},
	{0x5a, 0x01},
	{0x5d, 0x04},
	{0x6a, 0x04},
	{0x6b, 0x03},
	{0x6e, 0x28},
	{0x71, 0xbe},
	{0x72, 0x06},
	{0x73, 0x38},
	{0x74, 0x06},
	{0x79, 0x00},
	{0x7a, 0xb2},
	{0x7b, 0x10},
	{0x8f, 0x80},
	{0x91, 0x38},
	{0x92, 0x02},
	{0x9d, 0x03},
	{0x9e, 0x55},
	{0xb8, 0x70},
	{0xb9, 0x70},
	{0xba, 0x70},
	{0xbb, 0x70},
	{0xbc, 0x00},
	{0xc0, 0x00},
	{0xc1, 0x00},
	{0xc2, 0x00},
	{0xc3, 0x00},
	{0xc4, 0x6e},
	{0xc5, 0x6e},
	{0xc6, 0x6b},
	{0xc7, 0x6b},
	{0xcc, 0x11},
	{0xcd, 0xe0},
	{0xd0, 0x1b},
	{0xd2, 0x76},
	{0xd3, 0x68},
	{0xd4, 0x68},
	{0xd5, 0x73},
	{0xd6, 0x73},
	{0xe8, 0x55},
	{0xf0, 0x40},
	{0xf1, 0x40},
	{0xf2, 0x40},
	{0xf3, 0x40},
	{0xf4, 0x00},
	{0xfa, 0x1c},
	{0xfb, 0x33},
	{0xfc, 0xff},
	{0xfe, 0x01},
	{0xfd, 0x03},
	{0x03, 0x67},
	{0x00, 0x59},
	{0x04, 0x11},
	{0x05, 0x04},
	{0x06, 0x0c},
	{0x07, 0x08},
	{0x08, 0x08},
	{0x09, 0x4f},
	{0x0b, 0x08},
	{0x0d, 0x26},
	{0x0f, 0x00},
	{0xfd, 0x02},
	{0x34, 0xfe},
	{0x5e, 0x22},
	{0xa1, 0x06},
	{0xa3, 0x38},
	{0xa5, 0x02},
	{0xa7, 0x80},
	{0xfd, 0x01},
	{0xa1, 0x05},
	{0x94, 0x44},
	{0x95, 0x44},
	{0x96, 0x09},
	{0x98, 0x44},
	{0x9c, 0x0e},
	{0xb1, 0x01},
	{0xfd, 0x01},
	{0x05, 0x01},
	{0x06, 0xEB},
	{0x09, 0x00},
	{0x0a, 0x63},
	{0xb1, 0x03},
	{0x01, 0x01},
};

static struct regval_list sensor_1080p20_regs[] = {
	{0xfd, 0x00},
	{0x36, 0x01},
	{0xfd, 0x00},
	{0x36, 0x00},
	{0xfd, 0x00},
	{0x20, 0x00},

	{0xffff, 0x85},
	{0xfd, 0x00},
	{0xfd, 0x00},
	{0x30, 0x0a},
	{0x35, 0x04},
	{0x38, 0x11},
	{0x41, 0x06},
	{0x44, 0x20},
	{0xfd, 0x01},
	{0x03, 0x02},
	{0x04, 0x4c},
	{0x06, 0x00},
	{0x24, 0x30},
	//{0x01, 0x01},
	{0x19, 0x50},
	{0x1a, 0x0c},
	{0x1b, 0x0d},
	{0x1c, 0x00},
	{0x1d, 0x75},
	{0x1e, 0x52},
	{0x22, 0x14},
	{0x25, 0x44},
	{0x26, 0x0f},
	{0x3c, 0xca},
	{0x3d, 0x4a},
	{0x40, 0x0f},
	{0x43, 0x38},
	{0x46, 0x00},
	{0x47, 0x00},
	{0x49, 0x32},
	{0x50, 0x01},
	{0x51, 0x28},
	{0x52, 0x20},
	{0x53, 0x03},
	{0x57, 0x16},
	{0x59, 0x01},
	{0x5a, 0x01},
	{0x5d, 0x04},
	{0x6a, 0x04},
	{0x6b, 0x03},
	{0x6e, 0x28},
	{0x71, 0xbe},
	{0x72, 0x06},
	{0x73, 0x38},
	{0x74, 0x06},
	{0x79, 0x00},
	{0x7a, 0xb2},
	{0x7b, 0x10},
	{0x8f, 0x80},
	{0x91, 0x38},
	{0x92, 0x02},
	{0x9d, 0x03},
	{0x9e, 0x55},
	{0xb8, 0x70},
	{0xb9, 0x70},
	{0xba, 0x70},
	{0xbb, 0x70},
	{0xbc, 0x00},
	{0xc0, 0x00},
	{0xc1, 0x00},
	{0xc2, 0x00},
	{0xc3, 0x00},
	{0xc4, 0x6e},
	{0xc5, 0x6e},
	{0xc6, 0x6b},
	{0xc7, 0x6b},
	{0xcc, 0x11},
	{0xcd, 0xe0},
	{0xd0, 0x1b},
	{0xd2, 0x76},
	{0xd3, 0x68},
	{0xd4, 0x68},
	{0xd5, 0x73},
	{0xd6, 0x73},
	{0xe8, 0x55},
	{0xf0, 0x40},
	{0xf1, 0x40},
	{0xf2, 0x40},
	{0xf3, 0x40},
	{0xf4, 0x00},
	{0xfa, 0x1c},
	{0xfb, 0x33},
	{0xfc, 0xff},
	{0xfe, 0x01},
	{0xfd, 0x03},
	{0x03, 0x67},
	{0x00, 0x59},
	{0x04, 0x11},
	{0x05, 0x04},
	{0x06, 0x0c},
	{0x07, 0x08},
	{0x08, 0x08},
	{0x09, 0x4f},
	{0x0b, 0x08},
	{0x0d, 0x26},
	{0x0f, 0x00},
	{0xfd, 0x02},
	{0x34, 0xfe},
	{0x5e, 0x22},
	{0xa1, 0x06},
	{0xa3, 0x38},
	{0xa5, 0x02},
	{0xa7, 0x80},
	{0xfd, 0x01},
	{0xa1, 0x05},
	{0x94, 0x44},
	{0x95, 0x44},
	{0x96, 0x09},
	{0x98, 0x44},
	{0x9c, 0x0e},
	{0xb1, 0x01},
	{0xfd, 0x01},
	{0x05, 0x01},
	{0x06, 0xEB},
	{0x09, 0x00},
	{0x0a, 0x63},
	{0xb1, 0x03},
	{0x01, 0x01},
};

static struct regval_list sensor_1080p12_5_regs[] = {
	{0xfd, 0x00},
	{0x36, 0x01},
	{0xfd, 0x00},
	{0x36, 0x00},
	{0xfd, 0x00},
	{0x20, 0x00},
//	{0xffff, 0x85},
	{0xfd, 0x00},
	{0xfd, 0x00},
	{0x30, 0x0a},
	{0x35, 0x04},
	{0x38, 0x11},
	{0x41, 0x06},
	{0x44, 0x20},
	{0xfd, 0x01},
	{0x03, 0x02},
	{0x04, 0x4c},
	{0x06, 0x00},
	{0x24, 0x30},
	//{0x01, 0x01},
	{0x19, 0x50},
	{0x1a, 0x0c},
	{0x1b, 0x0d},
	{0x1c, 0x00},
	{0x1d, 0x75},
	{0x1e, 0x52},
	{0x22, 0x14},
	{0x25, 0x44},
	{0x26, 0x0f},
	{0x3c, 0xca},
	{0x3d, 0x4a},
	{0x40, 0x0f},
	{0x43, 0x38},
	{0x46, 0x00},
	{0x47, 0x00},
	{0x49, 0x32},
	{0x50, 0x01},
	{0x51, 0x28},
	{0x52, 0x20},
	{0x53, 0x03},
	{0x57, 0x16},
	{0x59, 0x01},
	{0x5a, 0x01},
	{0x5d, 0x04},
	{0x6a, 0x04},
	{0x6b, 0x03},
	{0x6e, 0x28},
	{0x71, 0xbe},
	{0x72, 0x06},
	{0x73, 0x38},
	{0x74, 0x06},
	{0x79, 0x00},
	{0x7a, 0xb2},
	{0x7b, 0x10},
	{0x8f, 0x80},
	{0x91, 0x38},
	{0x92, 0x02},
	{0x9d, 0x03},
	{0x9e, 0x55},
	{0xb8, 0x70},
	{0xb9, 0x70},
	{0xba, 0x70},
	{0xbb, 0x70},
	{0xbc, 0x00},
	{0xc0, 0x00},
	{0xc1, 0x00},
	{0xc2, 0x00},
	{0xc3, 0x00},
	{0xc4, 0x6e},
	{0xc5, 0x6e},
	{0xc6, 0x6b},
	{0xc7, 0x6b},
	{0xcc, 0x11},
	{0xcd, 0xe0},
	{0xd0, 0x1b},
	{0xd2, 0x76},
	{0xd3, 0x68},
	{0xd4, 0x68},
	{0xd5, 0x73},
	{0xd6, 0x73},
	{0xe8, 0x55},
	{0xf0, 0x40},
	{0xf1, 0x40},
	{0xf2, 0x40},
	{0xf3, 0x40},
	{0xf4, 0x00},
	{0xfa, 0x1c},
	{0xfb, 0x33},
	{0xfc, 0xff},
	{0xfe, 0x01},
	{0xfd, 0x03},
	{0x03, 0x67},
	{0x00, 0x59},
	{0x04, 0x11},
	{0x05, 0x04},
	{0x06, 0x0c},
	{0x07, 0x08},
	{0x08, 0x08},
	{0x09, 0x4f},
	{0x0b, 0x08},
	{0x0d, 0x26},
	{0x0f, 0x00},
	{0xfd, 0x02},
	{0x34, 0xfe},
	{0x5e, 0x22},
	{0xa1, 0x06},
	{0xa3, 0x38},
	{0xa5, 0x02},
	{0xa7, 0x80},
	{0xfd, 0x01},
	{0xa1, 0x05},
	{0x94, 0x44},
	{0x95, 0x44},
	{0x96, 0x09},
	{0x98, 0x44},
	{0x9c, 0x0e},
	{0xb1, 0x01},
	{0xfd, 0x01},
	{0x05, 0x01},
	{0x06, 0xEB},
	{0x09, 0x03},
	{0x0a, 0x06},
	{0xb1, 0x03},
	{0x01, 0x01},
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
	unsigned char explow, exphigh;
	struct sensor_info *info = to_state(sd);

	sensor_write(sd, 0xfd, 0x01);

	exphigh  = (unsigned char)((exp_val >> 12) & 0xff);
	explow	= (unsigned char)((exp_val >> 4) & 0xff);

	sensor_write(sd, EXP_HIGH, exphigh);
	sensor_write(sd, EXP_LOW, explow);

	sensor_write(sd, 0x01, 0x01);
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

	data_type gainlow = 0x10;
	data_type gaindiglow = 0x40;
	data_type gaindighigh = 0x00;

	if (gain_val < 16*16) {
		gainlow = gain_val;
	} else if (gain_val < 16*16*32) {
		gainlow = 0xF8;
		gaindiglow = 0xff & (gain_val >> 2);
		gaindighigh = 0xff & (gain_val >> 10);
	} else {
	    gainlow = 0xF8;
		gaindiglow = 0xFF;
		gaindighigh = 0x07;
	}

	sensor_write(sd, 0xfd, 0x01);
	sensor_write(sd, 0x24, gainlow);
	sensor_write(sd, 0x37, gaindighigh);
	sensor_write(sd, 0x39, gaindiglow);
	sensor_write(sd, 0x01, 0x01);

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

	sensor_s_exp(sd, exp_val);
	sensor_s_gain(sd, gain_val);

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

	sensor_write(sd, 0xfd, 0x01);
	sensor_read(sd, 0x3F, &get_value);
	sensor_dbg("ready to vflip, regs_data = 0x%x\n", get_value);

	if (enable) {
		set_value = get_value | 0x02;
		sensor_flip_status |= 0x02;
	} else {
		set_value = get_value & 0xFD;
		sensor_flip_status &= 0xFD;
	}
	sensor_write(sd, 0x3F, set_value);
	usleep_range(80000, 100000);
	sensor_read(sd, 0x3F, &get_value);
	sensor_dbg("after vflip, regs_data = 0x%x, sensor_flip_status = %d\n",
				get_value, sensor_flip_status);

	return 0;
}

static int sensor_s_hflip(struct v4l2_subdev *sd, int enable)
{
	data_type get_value;
	data_type set_value;

	if (!(enable == 0 || enable == 1))
		return -1;

	sensor_write(sd, 0xfd, 0x01);
	sensor_read(sd, 0x3F, &get_value);
	sensor_dbg("ready to hflip, regs_data = 0x%x\n", get_value);

	if (enable) {
		set_value = get_value | 0x01;
		sensor_flip_status |= 0x01;
	} else {
		set_value = get_value & 0xFE;
		sensor_flip_status &= 0xFE;
	}
	sensor_write(sd, 0x3F, set_value);
	usleep_range(80000, 100000);
	sensor_read(sd, 0x3F, &get_value);
	sensor_dbg("after hflip, regs_data = 0x%x, sensor_flip_status = %d\n",
				get_value, sensor_flip_status);

	return 0;
}

static int sensor_get_fmt_mbus_core(struct v4l2_subdev *sd, int *code)
{
//	struct sensor_info *info = to_state(sd);
//	data_type get_value = 0, check_value = 0;

//	sensor_read(sd, 0x17, &get_value);
//	check_value = get_value & 0x03;
//	check_value = sensor_flip_status & 0x3;
//	sensor_dbg("0x17 = 0x%x, check_value = 0x%x\n", get_value, check_value);

//	switch (check_value) {
//	case 0x00:
//		sensor_dbg("RGGB\n");
//		*code = MEDIA_BUS_FMT_SRGGB10_1X10;
//		break;
//	case 0x01:
//		sensor_dbg("GRBG\n");
//		*code = MEDIA_BUS_FMT_SGRBG10_1X10;
//		break;
//	case 0x02:
//		sensor_dbg("GBRG\n");
//		*code = MEDIA_BUS_FMT_SGBRG10_1X10;
//		break;
//	case 0x03:
//		sensor_dbg("BGGR\n");
//		*code = MEDIA_BUS_FMT_SBGGR10_1X10;
//		break;
//	default:
//		 *code = info->fmt->mbus_code;
//	}
	*code = MEDIA_BUS_FMT_SRGGB10_1X10; // os02g10 support change the rgb format by itself

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
		sensor_print("PWR_ON!\n");
		cci_lock(sd);
		vin_set_pmu_channel(sd, IOVDD, ON);
		usleep_range(1000, 1200);
		vin_set_pmu_channel(sd, DVDD, ON);
		usleep_range(1000, 1200);
		vin_set_pmu_channel(sd, AVDD, ON);
		usleep_range(10000, 12000);
		vin_set_mclk(sd, ON);
		usleep_range(1000, 1200);
		vin_set_mclk_freq(sd, MCLK);
		usleep_range(50000, 55000);
		cci_unlock(sd);
		break;

	case PWR_OFF:
		sensor_print("PWR_OFF!do nothing\n");
		cci_lock(sd);
		vin_set_mclk(sd, OFF);
		vin_set_pmu_channel(sd, AVDD, OFF);
		vin_set_pmu_channel(sd, DVDD, OFF);
		vin_set_pmu_channel(sd, IOVDD, OFF);
		usleep_range(10000, 12000);
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
#if !defined CONFIG_VIN_INIT_MELIS
	data_type rdval;
	int eRet;
	int times_out = 3;

	sensor_write(sd, 0xfd, 0x00);
	do {
		eRet = sensor_read(sd, ID_REG_HIGH, &rdval);
		sensor_print("eRet:%d, ID_VAL_HIGH:0x%x, times_out:%d\n", eRet, rdval, times_out);
		usleep_range(200, 220);
		times_out--;
	} while (eRet < 0  &&  times_out > 0);

	sensor_read(sd, ID_REG_HIGH, &rdval);
	sensor_print("ID_VAL_HIGH = %2x, Done!\n", rdval);
	if (rdval != ID_VAL_HIGH)
		return -ENODEV;

	sensor_read(sd, ID_REG_LOW, &rdval);
	sensor_print("ID_VAL_LOW = %2x, Done!\n", rdval);
	if (rdval != ID_VAL_LOW)
		return -ENODEV;

	sensor_print("Done!\n");
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
	info->low_speed    = 0;
	info->width        = 1920;
	info->height       = 1080;
	info->hflip        = 0;
	info->vflip        = 0;
	info->gain         = 0;
	info->exp          = 0;

	info->tpf.numerator      = 1;
	info->tpf.denominator    = 20;	/* 30fps */
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
		.mbus_code = MEDIA_BUS_FMT_SBGGR10_1X10, /*.mbus_code = MEDIA_BUS_FMT_SBGGR10_1X10, */
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
		.hts        = 1125,
		.vts        = 1600,
		.pclk       = 36000000,
		.mipi_bps   = 360 * 1000 * 1000,
		.fps_fixed  = 20,
		.bin_factor = 1,
		.intg_min   = 4 << 4,
		.intg_max   = 1592 << 4,
		.gain_min   = 1 << 4,
		.gain_max   = 510 << 4,
		.regs       = sensor_1080p20_regs,
		.regs_size  = ARRAY_SIZE(sensor_1080p20_regs),
		.set_size   = NULL,
	},

	{
		.width	    = 1920,
		.height     = 1080,
		.hoffset    = 0,
		.voffset    = 0,
		.hts	    = 1800,
		.vts	    = 1600,
		.pclk	    = 36000000,
		.mipi_bps   = 360 * 1000 * 1000,
		.fps_fixed  = 12,
		.bin_factor = 1,
		.intg_min   = 4 << 4,
		.intg_max   = 1592 << 4,
		.gain_min   = 1 << 4,
		.gain_max   = 510 << 4,
		.regs	    = sensor_1080p12_5_regs,
		.regs_size  = ARRAY_SIZE(sensor_1080p12_5_regs),
		.set_size   = NULL,
	},

};

#define N_WIN_SIZES (ARRAY_SIZE(sensor_win_sizes))

static int sensor_g_mbus_config(struct v4l2_subdev *sd,
				struct v4l2_mbus_config *cfg)
{
	//struct sensor_info *info = to_state(sd);

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

	usleep_range(20000, 30000);

	sensor_write_array(sd, sensor_fmt->regs, sensor_fmt->regs_size);

	if (wsize->regs)
		sensor_write_array(sd, wsize->regs, wsize->regs_size);

	if (wsize->set_size)
		wsize->set_size(sd);
	info->width = wsize->width;
	info->height = wsize->height;
	sensor_flip_status = 0x0;
	sensor_dbg("os02g10_sensor_vts = %d\n", os02g10_sensor_vts);

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
		.addr_width = CCI_BITS_8,
		.data_width = CCI_BITS_8,
	}, {
		.name = SENSOR_NAME_2,
		//.addr_width = CCI_BITS_16,
		.addr_width = CCI_BITS_8,
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
	light_nums = 0;
	mutex_init(&light_lock);

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
