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
#include <linux/types.h>

#include "camera.h"
#include "sensor_helper.h"

MODULE_AUTHOR("syq");
MODULE_DESCRIPTION("A low-level driver for SC2355 sensors");
MODULE_LICENSE("GPL");

#define MCLK            (24*1000*1000)
#define V4L2_IDENT_SENSOR   0x2355

//define the registers
#define EXP_HIGH    0x3e00
#define EXP_MID     0x3e01
#define EXP_LOW     0x3e02
#define DIG_GAIN    0x3e06
#define DIG_FINE_GAIN   0x3e07
#define ANA_GAIN    0x3e09
#define GAIN_STEP_BASE 128 //mean gain min step is 1/128gain

#define ID_REG_HIGH 0x3e07
#define ID_REG_LOW  0x3e08
#define ID_VAL_HIGH 0x80
#define ID_VAL_LOW  0x03
#define SENSOR_FRAME_RATE   30

#define I2C_ADDR 0x60
//#define I2C_ADDR_2 0x32

#define SENSOR_NUM 0x2
#define SENSOR_NAME "sc2355_mipi"
#define SENSOR_NAME_2 "sc2355_mipi_2"

#define SC2355_1600X1200_30FPS
#define SC2355_800X600_30FPS

#define SHARE_RESET_PWDN
static struct mutex light_lock;

#ifdef SC2355_800X600_30FPS
static struct regval_list sensor_800x600_30_regs[] = {
	{0x0103, 0x01},
	{0x0100, 0x00},
	{0x36e9, 0x80},
	{0x36ea, 0x0f},
	{0x36eb, 0x24},
	{0x36ed, 0x14},
	{0x36e9, 0x01},
	{0x301f, 0x0c},
	{0x303f, 0x82},
	{0x3208, 0x03},
	{0x3209, 0x20},
	{0x320a, 0x02},
	{0x320b, 0x58},
	{0x320c, 0x07},//hts=1920
	{0x320d, 0x80},
	{0x320e, 0x04},//vts=1250	1920*1250*30=72
	{0x320f, 0xe2},
	{0x3211, 0x02},
	{0x3213, 0x02},
	{0x3215, 0x31},
	{0x3220, 0x01},
	{0x3248, 0x02},
	{0x3253, 0x0a},
	{0x3301, 0xff},
	{0x3302, 0xff},
	{0x3303, 0x10},
	{0x3306, 0x28},
	{0x3307, 0x02},
	{0x330a, 0x00},
	{0x330b, 0xb0},
	{0x3318, 0x02},
	{0x3320, 0x06},
	{0x3321, 0x02},
	{0x3326, 0x12},
	{0x3327, 0x0e},
	{0x3328, 0x03},
	{0x3329, 0x0f},
	{0x3364, 0x4f},
	{0x33b3, 0x40},
	{0x33f9, 0x2c},
	{0x33fb, 0x38},
	{0x33fc, 0x0f},
	{0x33fd, 0x1f},
	{0x349f, 0x03},
	{0x34a6, 0x01},
	{0x34a7, 0x1f},
	{0x34a8, 0x40},
	{0x34a9, 0x30},
	{0x34ab, 0xa6},
	{0x34ad, 0xa6},
	{0x3622, 0x60},
	{0x3623, 0x40},
	{0x3624, 0x61},
	{0x3625, 0x08},
	{0x3626, 0x03},
	{0x3630, 0xa8},
	{0x3631, 0x84},
	{0x3632, 0x90},
	{0x3633, 0x43},
	{0x3634, 0x09},
	{0x3635, 0x82},
	{0x3636, 0x48},
	{0x3637, 0xe4},
	{0x3641, 0x22},
	{0x3670, 0x0f},
	{0x3674, 0xc0},
	{0x3675, 0xc0},
	{0x3676, 0xc0},
	{0x3677, 0x86},
	{0x3678, 0x88},
	{0x3679, 0x8c},
	{0x367c, 0x01},
	{0x367d, 0x0f},
	{0x367e, 0x01},
	{0x367f, 0x0f},
	{0x3690, 0x63},
	{0x3691, 0x63},
	{0x3692, 0x73},
	{0x369c, 0x01},
	{0x369d, 0x1f},
	{0x369e, 0x8a},
	{0x369f, 0x9e},
	{0x36a0, 0xda},
	{0x36a1, 0x01},
	{0x36a2, 0x03},
	{0x3900, 0x0d},
	{0x3904, 0x04},
	{0x3905, 0x98},
	{0x391b, 0x81},
	{0x391c, 0x10},
	{0x391d, 0x19},
	{0x3933, 0x01},
	{0x3934, 0x82},
	{0x3940, 0x5d},
	{0x3942, 0x01},
	{0x3943, 0x82},
	{0x3949, 0xc8},
	{0x394b, 0x64},
	{0x3952, 0x02},
	{0x3e00, 0x00},
	{0x3e01, 0x4d},
	{0x3e02, 0xe0},
	{0x4502, 0x34},
	{0x4509, 0x30},
	{0x450a, 0x71},
	{0x4819, 0x09},
	{0x481b, 0x05},
	{0x481d, 0x13},
	{0x481f, 0x04},
	{0x4821, 0x0a},
	{0x4823, 0x05},
	{0x4825, 0x04},
	{0x4827, 0x05},
	{0x4829, 0x08},
	{0x5000, 0x46},
	{0x5900, 0x01},
	{0x5901, 0x04},
	{0x0100, 0x01},
};
#endif

#ifdef SC2355_1600X1200_30FPS
static struct regval_list sensor_1600x1200_30_regs[] = {
	{0x0103, 0x01},
	{0x0100, 0x00},
	{0x36e9, 0x80},
	{0x36ea, 0x0f},
	{0x36eb, 0x25},
	{0x36ed, 0x04},
	{0x36e9, 0x01},
	{0x301f, 0x0d},
	{0x320c, 0x07},//HTS=1900
	{0x320d, 0x6c},
	{0x320e, 0x04},//vts=1250
	{0x320f, 0xe2},
	{0x3248, 0x02},
	{0x3253, 0x0a},
	{0x3301, 0xff},
	{0x3302, 0xff},
	{0x3303, 0x10},
	{0x3306, 0x28},
	{0x3307, 0x02},
	{0x330a, 0x00},
	{0x330b, 0xb0},
	{0x3318, 0x02},
	{0x3320, 0x06},
	{0x3321, 0x02},
	{0x3326, 0x12},
	{0x3327, 0x0e},
	{0x3328, 0x03},
	{0x3329, 0x0f},
	{0x3364, 0x4f},
	{0x33b3, 0x40},
	{0x33f9, 0x2c},
	{0x33fb, 0x38},
	{0x33fc, 0x0f},
	{0x33fd, 0x1f},
	{0x349f, 0x03},
	{0x34a6, 0x01},
	{0x34a7, 0x1f},
	{0x34a8, 0x40},
	{0x34a9, 0x30},
	{0x34ab, 0xa6},
	{0x34ad, 0xa6},
	{0x3622, 0x60},
	{0x3623, 0x40},
	{0x3624, 0x61},
	{0x3625, 0x08},
	{0x3626, 0x03},
	{0x3630, 0xa8},
	{0x3631, 0x84},
	{0x3632, 0x90},
	{0x3633, 0x43},
	{0x3634, 0x09},
	{0x3635, 0x82},
	{0x3636, 0x48},
	{0x3637, 0xe4},
	{0x3641, 0x22},
	{0x3670, 0x0f},
	{0x3674, 0xc0},
	{0x3675, 0xc0},
	{0x3676, 0xc0},
	{0x3677, 0x86},
	{0x3678, 0x88},
	{0x3679, 0x8c},
	{0x367c, 0x01},
	{0x367d, 0x0f},
	{0x367e, 0x01},
	{0x367f, 0x0f},
	{0x3690, 0x63},
	{0x3691, 0x63},
	{0x3692, 0x73},
	{0x369c, 0x01},
	{0x369d, 0x1f},
	{0x369e, 0x8a},
	{0x369f, 0x9e},
	{0x36a0, 0xda},
	{0x36a1, 0x01},
	{0x36a2, 0x03},
	{0x3900, 0x0d},
	{0x3904, 0x06},
	{0x3905, 0x98},
	{0x391b, 0x81},
	{0x391c, 0x10},
	{0x391d, 0x19},
	{0x3933, 0x01},
	{0x3934, 0x82},
	{0x3940, 0x5d},
	{0x3942, 0x01},
	{0x3943, 0x82},
	{0x3949, 0xc8},
	{0x394b, 0x64},
	{0x3952, 0x02},
	{0x3e00, 0x00},
	{0x3e01, 0x4d},
	{0x3e02, 0xe0},
	{0x4502, 0x34},
	{0x4509, 0x30},
	{0x450a, 0x71},
	{0x0100, 0x01},
};
#endif

/*
 * Here we'll try to encapsulate the changes for just the output
 * video format.
 *
 */

static struct regval_list sensor_fmt_raw[] = {

};

static int sensor_g_exp(struct v4l2_subdev *sd, __s32 *value)
{
	struct sensor_info *info = to_state(sd);
	*value = info->exp;

	//data_type exp_high, exp_mid, exp_low;
	//sensor_read(sd, EXP_HIGH, &exp_high);
	//sensor_read(sd, EXP_MID, &exp_mid);
	//sensor_read(sd, EXP_LOW, &exp_low);
	//sensor_print("sensor_g_exp: %d", (exp_high & 0x0f << 12) + (exp_mid & 0xff << 4) + (exp_low & 0xf0 >> 4));
	sensor_dbg("sensor get_exposure = %d\n", info->exp);
	return 0;
}

static int sensor_s_exp(struct v4l2_subdev *sd, unsigned int exp_val)
{
	struct sensor_info *info = to_state(sd);
	unsigned int tmp_exp_val = exp_val;
	data_type exp_high, exp_mid, exp_low;
	int high_exp, mid_exp, low_exp;
	data_type vts_val_high, vts_val_low;

	high_exp = (tmp_exp_val >> 12) & 0x0f;
	mid_exp = (tmp_exp_val >> 4) & 0xff;
	low_exp = (tmp_exp_val << 4) & 0xf0;

	//sensor_print("in-exp_val: 0x%x, high_exp: 0x%x, mid_exp: 0x%x, low_wxp: 0x%x\n", exp_val, high_exp, mid_exp, low_exp);

	sensor_write(sd, EXP_HIGH, (tmp_exp_val >> 12) & 0x0f);
	sensor_write(sd, EXP_MID, (tmp_exp_val >> 4) & 0xFF);
	sensor_write(sd, EXP_LOW, (tmp_exp_val << 4) & 0xf0);

//	high_exp = (tmp_exp_val >> 16) & 0x0f;
//	mid_exp = (tmp_exp_val >> 8) & 0xff;
//	low_exp = (tmp_exp_val << 0) & 0xf0;
//	sensor_print("in-exp_val: 0x%x, high_exp: 0x%x, mid_exp: 0x%x, low_wxp: 0x%x\n", exp_val, high_exp, mid_exp, low_exp);
//	sensor_write(sd, EXP_HIGH, (tmp_exp_val >> 16) & 0x0f);
//	sensor_write(sd, EXP_MID, (tmp_exp_val >> 8) & 0xFF);
//	sensor_write(sd, EXP_LOW, (tmp_exp_val << 0) & 0xf0);

	sensor_read(sd, 0x320e, &vts_val_high);
	sensor_read(sd, 0x320f, &vts_val_low);
	//sensor_print("0x320e: 0x%x, 0x320f: 0x%x\n", vts_val_high, vts_val_low);

	sensor_read(sd, EXP_HIGH, &exp_high);
	sensor_read(sd, EXP_MID, &exp_mid);
	sensor_read(sd, EXP_LOW, &exp_low);
	//sensor_print("out-exp_val: 0x%x, exp_high: 0x%x, exp_mid: 0x%x, exp_low: 0x%x\n", exp_val, exp_high, exp_mid, exp_low);
	info->exp = exp_val;

	return 0;
}

static int sensor_g_gain(struct v4l2_subdev *sd, __s32 *value)
{
	struct sensor_info *info = to_state(sd);
	data_type ana_gain, dig_gain, fine_gain;

	*value = info->gain;

	sensor_read(sd, ANA_GAIN, &ana_gain);
	sensor_read(sd, DIG_GAIN, &dig_gain);
	sensor_read(sd, DIG_FINE_GAIN, &fine_gain);
	//sensor_print("sensor_g_gain ana_gain:%d, dig_gain:%d, fine_fain:%d", ana_gain, dig_gain, fine_gain);
	return 0;
}

static unsigned char analog_Gain_Reg[] = {0x00, 0x01, 0x03, 0x07, 0x0f, 0x1f};

static int setSensorGain(struct v4l2_subdev *sd, int gain)
{
	int ana_gain = gain / GAIN_STEP_BASE;
	int dig_Gain;
	int gain_flag = 1;

	if (ana_gain >= 32 * 2) {
		gain_flag = 5;
	} else if (ana_gain >= 32) {
		gain_flag = 5;
	} else if (ana_gain >= 16) {
		gain_flag = 4;
	} else if (ana_gain >= 8) {
		gain_flag = 3;
	} else if (ana_gain >= 4) {
		gain_flag = 2;
	} else if (ana_gain >= 2) {
		gain_flag = 1;
	} else {
		gain_flag = 0;
	}

	sensor_write(sd, ANA_GAIN, analog_Gain_Reg[gain_flag]);
	dig_Gain = gain >> gain_flag; //dig_Gain min mean 1/128gain
	if (dig_Gain < 2 * GAIN_STEP_BASE) {
		//step1/128
		sensor_write(sd, DIG_GAIN, 0x00);
		sensor_write(sd, DIG_FINE_GAIN, dig_Gain - 128 + 0x80);
		//sensor_print("sensor set analog_gain:0x%02x, dig_gain:0x%02x, dig_fine_gain:0x%02x", analog_Gain_Reg[gain_flag], 0x00, dig_Gain - 128 + 0x80);
	} else if (dig_Gain < 4 * GAIN_STEP_BASE) {
		//step1/64
		sensor_write(sd, DIG_GAIN, 0x01);
		sensor_write(sd, DIG_FINE_GAIN, (dig_Gain - 128 * 2) / 2 + 0x80);
		//sensor_print("sensor set analog_gain:0x%02x, dig_gain:0x%02x, dig_fine_gain:0x%02x", analog_Gain_Reg[gain_flag], 0x01, (dig_Gain - 128 * 2) / 2 + 0x80);
	} else if (dig_Gain < 8 * GAIN_STEP_BASE) {
		//step1/32
		sensor_write(sd, DIG_GAIN, 0x03);
		sensor_write(sd, DIG_FINE_GAIN, (dig_Gain - 128 * 4) / 4 + 0x80);
		//sensor_print("sensor set analog_gain:0x%02x, dig_gain:0x%02x, dig_fine_gain:0x%02x", analog_Gain_Reg[gain_flag], 0x03, (dig_Gain - 128 * 4) / 4 + 0x80);
	} else {
		sensor_write(sd, DIG_GAIN, 0x03);
		sensor_write(sd, DIG_FINE_GAIN, 0xfe);
	}

	return 0;
}

static int sensor_s_gain(struct v4l2_subdev *sd, int gain_val)
{
	struct sensor_info *info = to_state(sd);

	if (gain_val == info->gain)
		return 0;

	sensor_dbg("gain_val:%d\n", gain_val);
	setSensorGain(sd, gain_val);
	info->gain = gain_val;

	return 0;
}

static int sc2355_sensor_vts;
static int sensor_s_exp_gain(struct v4l2_subdev *sd, struct sensor_exp_gain *exp_gain)
{
	int exp_val, gain_val;
	//int shutter = 0, frame_length = 0;
	struct sensor_info *info = to_state(sd);

	info->exp = exp_gain->exp_val;
	info->gain = exp_gain->gain_val;
	sensor_print("exp = %d, gain = %d\n\n", exp_gain->exp_val, exp_gain->gain_val);

	if ((exp_gain->exp_val - exp_gain->exp_val / 16 * 16) > 0) {
		exp_val = exp_gain->exp_val / 16 + 1;
	} else {
		exp_val = exp_gain->exp_val / 16;
	}
	exp_val = exp_val & 0xffff;

	//gain min step is 1/128gain
	gain_val = exp_gain->gain_val * GAIN_STEP_BASE;
	if ((gain_val - gain_val / 16 * 16) > 0) {
		gain_val = gain_val / 16 + 1;
	} else {
		gain_val = gain_val / 16;
	}

//	sensor_write(sd, 0x3812, 0x00);
	sensor_s_exp(sd, exp_val);
	sensor_s_gain(sd, gain_val);
//	sensor_write(sd, 0x3802, 0x00);
//	sensor_write(sd, 0x3812, 0x30);

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
		set_value = get_value & 0x9F;
		sensor_flip_status &= 0x9F;
	}
	sensor_write(sd, 0x3221, set_value);
	usleep_range(8000, 100000);
	sensor_read(sd, 0x3221, &get_value);
	sensor_dbg("after vflip, regs_data = 0x%x, sensor_flip_status = %d\n", get_value, sensor_flip_status);

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
		set_value = get_value & 0xF9;
		sensor_flip_status &= 0xF9;
	}
	sensor_write(sd, 0x3221, set_value);
	usleep_range(80000, 100000);
	sensor_read(sd, 0x3221, &get_value);
	sensor_dbg("after hflip, regs_data = 0x%x, sensor_flip_status = %d\n", get_value, sensor_flip_status);

	return 0;
}

static int sensor_get_fmt_mbus_core(struct v4l2_subdev *sd, int *code)
{
	*code = MEDIA_BUS_FMT_SBGGR10_1X10;
	return 0;
}

static int sensor_power(struct v4l2_subdev *sd, int on)
{
	switch (on) {
	case STBY_ON:
		sensor_dbg("STBY_ON!\n");
		cci_lock(sd);
		vin_gpio_write(sd, PWDN, CSI_GPIO_LOW);
		cci_unlock(sd);
		break;
	case STBY_OFF:
		sensor_dbg("STBY_OFF!\n");
		cci_lock(sd);
		vin_set_mclk_freq(sd, MCLK);
		vin_set_mclk(sd, ON);
		usleep_range(10000, 12000);
		vin_gpio_write(sd, PWDN, CSI_GPIO_HIGH);
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
		//vin_gpio_set_status(sd, POWER_EN, 1);
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
	do {
		eRet = sensor_read(sd, ID_REG_HIGH, &rdval);
		sensor_dbg("eRet:%d, ID_VAL_HIGH:0x%x,times_out:%d\n", eRet, rdval, times_out);
		usleep_range(200, 220);
		times_out--;
	} while (eRet < 0 && times_out > 0);

	sensor_read(sd, ID_REG_HIGH, &rdval);
	sensor_dbg("ID_VAL_HIGH = %2x, Done!\n", rdval);
	if (rdval != ID_VAL_HIGH)
		return -ENODEV;

	sensor_read(sd, ID_REG_LOW, &rdval);
	sensor_dbg("ID_VAL_LOW = %2x, Done!\n", rdval);
	if (rdval != ID_VAL_LOW)
		return -ENODEV;

	sensor_dbg("Done!\n");
#endif
	return 0;
}

static int sensor_init(struct v4l2_subdev *sd, u32 val)
{
	int ret;
	struct sensor_info *info = to_state(sd);

	sensor_dbg("sensor_init\n");

	ret = sensor_detect(sd);
	if (ret) {
		sensor_err("chip found is not an target chip.\n");
		return ret;
	}

	info->focus_status = 0;
	info->low_speed = 0;
	info->width = 1600;
	info->height = 1200;
	info->hflip = 0;
	info->vflip = 0;
	info->gain = 0;
	info->exp = 0;

	info->tpf.numerator = 1;
	info->tpf.denominator = 30;
	//info->preview_first_flag = 1;
	return 0;
}

static long sensor_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	int ret = 0;
	struct sensor_info *info = to_state(sd);

	switch (cmd) {
	case GET_CURRENT_WIN_CFG:
		if (info->current_wins != NULL) {
			memcpy(arg, info->current_wins, sizeof(struct sensor_win_size));
			ret = 0;
		} else {
			sensor_err("empyt wins!\n");
			ret = -1;
		}
		break;
	case SET_FPS:
		break;
	case VIDIOC_VIN_SENSOR_EXP_GAIN:
		sensor_s_exp_gain(sd, (struct sensor_exp_gain *)arg);
		break;
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

static struct sensor_win_size sensor_win_sizes[] = {
#ifdef SC2355_800X600_30FPS
	{
		.width = 800,
		.height = 600,
		.hoffset = 0,
		.voffset = 0,
		.hts = 1920,
		.vts = 1250,
		.pclk = 72000000,
		.mipi_bps = 297 * 1000 * 1000,
		.fps_fixed = 30,
		.bin_factor = 1,
		.intg_min = 1 << 4,
		.intg_max = 1125 << 4,
		.gain_min = 1 << 4,
		.gain_max = 1370,
		.regs = sensor_800x600_30_regs,
		.regs_size = ARRAY_SIZE(sensor_800x600_30_regs),
		.set_size = NULL,
	},
#endif

#ifdef SC2355_1600X1200_30FPS
	{
		.width = 1600,
		.height = 1200,
		.hoffset = 0,
		.voffset = 0,
		.hts = 1900,
		.vts = 1250,
		.pclk = 72000000,
		.mipi_bps = 297 * 1000 * 1000,
		.fps_fixed = 30,
		.bin_factor = 1,
		.intg_min = 1 << 4,
		.intg_max = 1125 << 4,
		.gain_min = 1 << 4,
		.gain_max = 1370,
		.regs = sensor_1600x1200_30_regs,
		.regs_size = ARRAY_SIZE(sensor_1600x1200_30_regs),
		.set_size = NULL,
	},
#endif
};

#define N_WIN_SIZES ARRAY_SIZE(sensor_win_sizes)

static int sensor_g_mbus_config(struct v4l2_subdev *sd, struct v4l2_mbus_config *cfg)
{
	cfg->type = V4L2_MBUS_CSI2;
	cfg->flags = 0 | V4L2_MBUS_CSI2_1_LANE | V4L2_MBUS_CSI2_CHANNEL_0;

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
		break;
	case V4L2_CID_EXPOSURE:
		return sensor_s_exp(sd, ctrl->val);
		break;
	case V4L2_CID_HFLIP:
		return  sensor_s_hflip(sd, ctrl->val);
		break;
	case V4L2_CID_VFLIP:
		return sensor_s_vflip(sd, ctrl->val);
		break;
	}
	return -EINVAL;
}

static int sensor_reg_init(struct sensor_info *info)
{
	struct v4l2_subdev *sd  = &info->sd;
	struct sensor_win_size *wsize = info->current_wins;
	__maybe_unused struct sensor_exp_gain exp_gain;

#if defined CONFIG_VIN_INIT_MELIS
	if (info->preview_first_flag) {
		info->preview_first_flag = 0;
	} else {
		if (wsize->regs)
			sensor_write_array(sd, wsize->regs, wsize->regs_size);
		if (info->exp && info->gain) {
			exp_gain.exp_val = info->exp;
			exp_gain.gain_val = info->gain;
		} else {
			exp_gain.exp_val = 18000;
			exp_gain.gain_val = 87;
		}
		sensor_s_exp_gain(sd, &exp_gain);
	}
#else
	if (wsize->regs) {
		sensor_write_array(sd, wsize->regs, wsize->regs_size);
	}
#endif
	if (wsize->set_size)
		wsize->set_size(sd);

	info->width = wsize->width;
	info->height = wsize->height;
	sensor_flip_status = 0x0;
	sc2355_sensor_vts = wsize->vts;
	sensor_dbg("sc2355_sensor_vts = %d\n", sc2355_sensor_vts);
	sensor_dbg("s_fmt set width = %d, height = %d\n", wsize->width, wsize->height);

	return 0;
}

static int sensor_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct sensor_info *info = to_state(sd);
	sensor_dbg("%s on = %d, %d*%d fps: %d code: %x\n", __func__, enable, info->current_wins->width, info->current_wins->height, info->current_wins->fps_fixed, info->fmt->mbus_code);

	if (!enable)
		return 0;
	return sensor_reg_init(info);
}

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

static struct cci_driver cci_drv[] = {
	{
		.name = SENSOR_NAME,
		.addr_width = CCI_BITS_16,
		.data_width = CCI_BITS_8,
	},
	{
		.name = SENSOR_NAME_2,
		.addr_width = CCI_BITS_16,
		.data_width = CCI_BITS_8,
	},
};

static int sensor_init_controls(struct v4l2_subdev *sd, const struct v4l2_ctrl_ops *ops)
{
	struct sensor_info *info = to_state(sd);
	struct v4l2_ctrl_handler *handler = &info->handler;
	struct v4l2_ctrl *ctrl;
	int ret = 0;

	v4l2_ctrl_handler_init(handler, 4);

	ctrl = v4l2_ctrl_new_std(handler, ops, V4L2_CID_GAIN, 1 * 1600, 256 * 1600, 1, 1 * 1600);

	if (ctrl != NULL)
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE;

	ctrl = v4l2_ctrl_new_std(handler, ops, V4L2_CID_EXPOSURE, 1, 65536 * 16, 1, 1);

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

static int sensor_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct v4l2_subdev *sd;
	struct sensor_info *info;
	int i = 0;

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
	//use CMB_PHYA_OFFSET2 also ok
	info->combo_mode = CMB_TERMINAL_RES | CMB_PHYA_OFFSET3 | MIPI_NORMAL_MODE;
	//info->combo_mode = CMB_PHYA_OFFSET2 | MIPI_NORMAL_MODE;
	info->stream_seq = MIPI_BEFORE_SENSOR;
	info->af_first_flag = 1;
	info->exp = 0;
	info->gain = 0;
	info->preview_first_flag = 1;
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
	},
	{
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

#ifdef CONFIG_SUNXI_FASTBOOT
subsys_initcall_sync(init_sensor);
#else
module_init(init_sensor);
#endif
module_exit(exit_sensor);
