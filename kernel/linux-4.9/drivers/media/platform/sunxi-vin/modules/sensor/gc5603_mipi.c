/*
 * A V4L2 driver for gc5603_mipi cameras.
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
MODULE_DESCRIPTION("A low-level driver for gc5603 sensors");
MODULE_LICENSE("GPL");

#define MCLK              (27 * 1000 * 1000)
#define V4L2_IDENT_SENSOR 0x5603

/*
 * Our nominal (default) frame rate.
 */
#define SENSOR_FRAME_RATE 20

/*
 * The gc5025_mipi sits on i2c with ID 0x6e
 */
#define I2C_ADDR 0x62

#define SENSOR_NAME "gc5603_mipi"

struct cfg_array { /* coming later */
	struct regval_list *regs;
	int size;
};

/*
 * The default register settings
 *
 */
static struct regval_list sensor_default_regs[] = {
/*version 1.0
*mclk 27Mhz
*mipi 2lane 846Mbps/lane
*
*vts = 2625, frame rate = 20fps
*window 2196 * 1966
*GBRG
*/
	{0x03fe, 0xf0},
	{0x03fe, 0x00},
	{0x03fe, 0x10},
	{0x03fe, 0x00},
	{0x0a38, 0x02},
	{0x0a38, 0x03},
	{0x0a20, 0x07},
	{0x06ab, 0x03},
	{0x061c, 0x50},
	{0x061d, 0x05},
	{0x061e, 0x70},
	{0x061f, 0x03},
	{0x0a21, 0x08},
	{0x0a34, 0x40},
	{0x0a35, 0x11},
	{0x0a36, 0x5e},
	{0x0a37, 0x03},
	{0x0314, 0x50},
	{0x0315, 0x32},
	{0x031c, 0xce},
	{0x0219, 0x47},
	{0x0342, 0x04},
	{0x0343, 0xb0},
	{0x0340, 0x0a},
	{0x0341, 0x41},
	{0x0345, 0x02},
	{0x0347, 0x02},
	{0x0348, 0x0b},
	{0x0349, 0x98},
	{0x034a, 0x06},
	{0x034b, 0x8a},
	{0x0094, 0x0b},
	{0x0095, 0x90},
	{0x0096, 0x06},
	{0x0097, 0x82},
	{0x0099, 0x04},
	{0x009b, 0x04},
	{0x060c, 0x01},
	{0x060e, 0xd2},
	{0x060f, 0x05},
	{0x070c, 0x01},
	{0x070e, 0xd2},
	{0x070f, 0x05},
	{0x0909, 0x07},
	{0x0902, 0x04},
	{0x0904, 0x0b},
	{0x0907, 0x54},
	{0x0908, 0x06},
	{0x0903, 0x9d},
	{0x072a, 0x1c},
	{0x072b, 0x1c},
	{0x0724, 0x2b},
	{0x0727, 0x2b},
	{0x1466, 0x18},
	{0x1467, 0x08},
	{0x1468, 0x10},
	{0x1469, 0x80},
	{0x146a, 0xe8},
	{0x1412, 0x20},
	{0x0707, 0x07},
	{0x0737, 0x0f},
	{0x0704, 0x01},
	{0x0706, 0x03},
	{0x0716, 0x03},
	{0x0708, 0xc8},
	{0x0718, 0xc8},

	{0x061a, 0x02},/*03*/
	{0x1430, 0x80},
	{0x1407, 0x10},
	{0x1408, 0x16},
	{0x1409, 0x03},
	{0x1438, 0x01},
	{0x02ce, 0x03},
	{0x0245, 0xc9},

	{0x023a, 0x08},/*3B*/
	{0x02cd, 0x88},
	{0x0612, 0x02},
	{0x0613, 0xc7},

	{0x0243, 0x03},/*06*/
	{0x0089, 0x03},
	{0x0002, 0xab},
	{0x0040, 0xa3},
	{0x0075, 0x64},
	{0x0004, 0x0f},
	{0x0053, 0x0a},
	{0x0205, 0x0c},

	{0x0052, 0x02},
	{0x0076, 0x01},
	{0x021a, 0x10},
	{0x0049, 0x0f},
	{0x004a, 0x3c},
	{0x004b, 0x00},
	{0x0430, 0x25},

	{0x0431, 0x25},
	{0x0432, 0x25},
	{0x0433, 0x25},
	{0x0434, 0x59},
	{0x0435, 0x59},
	{0x0436, 0x59},
	{0x0437, 0x59},
	/*auto_load*/
	{0x0a67, 0x80},
	{0x0a54, 0x0e},
	{0x0a65, 0x10},
	{0x0a98, 0x04},
	{0x05be, 0x00},
	{0x05a9, 0x01},
	{0x0023, 0x00},
	{0x0022, 0x00},
	{0x0025, 0x00},
	{0x0024, 0x00},
	{0x0028, 0x0b},
	{0x0029, 0x98},
	{0x002a, 0x06},
	{0x002b, 0x86},
	{0x0a83, 0xe0},
	{0x0a72, 0x02},
	{0x0a73, 0x60},
	{0x0a75, 0x41},
	{0x0a70, 0x03},
	{0x0a5a, 0x80},
	/*EXP*/
	{0x0202, 0x05},
	{0x0203, 0xd0},
	/*GAIN*/
	{0x0205, 0xc0},
	{0x02b0, 0x60},
	/*est image*/
/*	{0x008c, 0x14}, */
	/*MIPI*/
	{0x0181, 0x30},
	{0x0182, 0x05},
	{0x0185, 0x01},
	{0x0180, 0x46},
	{0x0100, 0x08},
	{0x010d, 0x74},
	{0x010e, 0x0e},
	{0x0113, 0x02},
	{0x0114, 0x01},
	{0x0115, 0x10},
	{0x0100, 0x09},
	{REG_DLY, 0x14},
	{0x0a70, 0x00},
	{0x0080, 0x02},
	{0x0a67, 0x00},
};

static struct regval_list sensor_qsxga_regs[] = {
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

static int sensor_s_exp(struct v4l2_subdev *sd, unsigned int exp_val)
{

	struct sensor_info *info = to_state(sd);
	int tmp_exp_val;

	if (exp_val > 0xa390)
		exp_val = 0xa390;
	else if (exp_val < 16)
		exp_val = 16;

	tmp_exp_val = exp_val / 16;

	sensor_dbg("sensor_s_exp exp_val:%d\n", exp_val);

	sensor_write(sd, 0x202, (tmp_exp_val >> 8) & 0xFF);
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

unsigned char regValTable[26][7] = {
   //0614, 0615, 0225, 1467  1468, 00b8, 00b9
	{0x00, 0x00, 0x04, 0x19, 0x19, 0x01, 0x00},
	{0x90, 0x02, 0x04, 0x1b, 0x1b, 0x01, 0x0A},
	{0x00, 0x00, 0x00, 0x19, 0x19, 0x01, 0x12},
	{0x90, 0x02, 0x00, 0x1b, 0x1b, 0x01, 0x20},
	{0x01, 0x00, 0x00, 0x19, 0x19, 0x01, 0x30},
	{0x91, 0x02, 0x00, 0x1b, 0x1b, 0x02, 0x05},
	{0x02, 0x00, 0x00, 0x1a, 0x1a, 0x02, 0x19},
	{0x92, 0x02, 0x00, 0x1c, 0x1c, 0x02, 0x3F},
	{0x03, 0x00, 0x00, 0x1a, 0x1a, 0x03, 0x20},
	{0x93, 0x02, 0x00, 0x1d, 0x1d, 0x04, 0x0A},
	{0x00, 0x00, 0x01, 0x1d, 0x1d, 0x05, 0x02},
	{0x90, 0x02, 0x01, 0x20, 0x20, 0x05, 0x39},
	{0x01, 0x00, 0x01, 0x1e, 0x1e, 0x06, 0x3C},
	{0x91, 0x02, 0x01, 0x20, 0x20, 0x08, 0x0D},
	{0x02, 0x00, 0x01, 0x20, 0x20, 0x09, 0x21},
	{0x92, 0x02, 0x01, 0x22, 0x22, 0x0B, 0x0F},
	{0x03, 0x00, 0x01, 0x21, 0x21, 0x0D, 0x17},
	{0x93, 0x02, 0x01, 0x23, 0x23, 0x0F, 0x33},
	{0x04, 0x00, 0x01, 0x23, 0x23, 0x12, 0x30},
	{0x94, 0x02, 0x01, 0x25, 0x25, 0x16, 0x10},
	{0x05, 0x00, 0x01, 0x24, 0x24, 0x1A, 0x19},
	{0x95, 0x02, 0x01, 0x26, 0x26, 0x1F, 0x13},
	{0x06, 0x00, 0x01, 0x26, 0x26, 0x25, 0x08},
	{0x96, 0x02, 0x01, 0x28, 0x28, 0x2C, 0x03},
	{0xb6, 0x04, 0x01, 0x28, 0x28, 0x34, 0x0F},
	{0x86, 0x06, 0x01, 0x2a, 0x2a, 0x3D, 0x3D},
};

unsigned int gainLevelTable[26] = {
	64, 74, 82, 96, 112, 133,
	153, 191, 224, 266, 322, 377,
	444, 525, 609, 719, 855, 1011,
	1200, 1424, 1689, 2003, 2376, 2819,
	3343, 3965,
};

static int SetSensorGain(struct v4l2_subdev *sd, int gain)
{
	int i;
	int total;
	unsigned int tol_dig_gain = 0;

	total = sizeof(gainLevelTable) / sizeof(unsigned int);
	for (i = 0; i < total - 1; i++) {
		if ((gainLevelTable[i] <= gain) && (gain < gainLevelTable[i+1]))
			break;
	}

	tol_dig_gain = gain * 64 / gainLevelTable[i];
	sensor_write(sd, 0x031d, 0x2d);
	sensor_write(sd, 0x0614, regValTable[i][0]);
	sensor_write(sd, 0x0615, regValTable[i][1]);
	sensor_write(sd, 0x0225, regValTable[i][2]);
	sensor_write(sd, 0x031d, 0x28);

	sensor_write(sd, 0x1467, regValTable[i][3]);
	sensor_write(sd, 0x1468, regValTable[i][4]);
	sensor_write(sd, 0x00b8, regValTable[i][5]);
	sensor_write(sd, 0x00b9, regValTable[i][6]);

	sensor_write(sd, 0x0064, (tol_dig_gain>>6));
	sensor_write(sd, 0x0065, (tol_dig_gain&0x3f)<<2);

	return 0;

}

static int sensor_s_gain(struct v4l2_subdev *sd, unsigned int gain_val)
{

	struct sensor_info *info = to_state(sd);

	if (gain_val == info->gain) {
		return 0;
	}

	if (gain_val < (1 * 16))
		gain_val = 16;
	else if (gain_val > 991)
		gain_val = 991;

	sensor_dbg("sensor_s_gain info->gain %d\n", gain_val);
	SetSensorGain(sd, gain_val * 4);

	info->gain = gain_val;

	return 0;

}

static int sensor_s_exp_gain(struct v4l2_subdev *sd,
			     struct sensor_exp_gain *exp_gain)
{
	/*struct sensor_info *info = to_state(sd);*/

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
	int ret;

	ret = 0;
	switch (on) {
	case STBY_ON:
		ret = sensor_s_sw_stby(sd, STBY_ON);
		if (ret < 0)
			sensor_err("soft stby falied!\n");
		usleep_range(10000, 12000);

		vin_gpio_write(sd, PWDN, CSI_GPIO_LOW);

		cci_lock(sd);
		/* inactive mclk after stadby in */
		vin_set_mclk(sd, OFF);
		cci_unlock(sd);
		break;
	case STBY_OFF:
		cci_lock(sd);

		vin_set_mclk_freq(sd, MCLK);
		vin_set_mclk(sd, ON);
		usleep_range(10000, 12000);

		vin_gpio_write(sd, PWDN, CSI_GPIO_HIGH);

		cci_unlock(sd);
		ret = sensor_s_sw_stby(sd, STBY_OFF);
		if (ret < 0)
			sensor_err("soft stby off falied!\n");
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
	sensor_print("%s val %d\n", __func__, val);

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
	if (rdval != (V4L2_IDENT_SENSOR >> 8)) {
		sensor_err(" read 0x03f0 return 0x%x\n", rdval);
		return -ENODEV;
	}

	sensor_read(sd, 0x03f1, &rdval);
	if (rdval != (V4L2_IDENT_SENSOR & 0xff))
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
	info->width = 2960;
	info->height = 1666;
	info->hflip = 0;
	info->vflip = 0;
	info->exp = 0;
	info->gain = 0;

	info->tpf.numerator = 1;
	info->tpf.denominator = SENSOR_FRAME_RATE; /* 20 fps */
	info->preview_first_flag = 1;

	return 0;
}

static long sensor_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	int ret = 0;
	struct sensor_info *info = to_state(sd);

	switch (cmd) {
	case GET_CURRENT_WIN_CFG:
		if (info->current_wins) {
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
		.desc = "Raw RGB Bayer",
		.mbus_code = MEDIA_BUS_FMT_SGBRG10_1X10,
		.regs = sensor_fmt_raw,
		.regs_size = ARRAY_SIZE(sensor_fmt_raw),
		.bpp = 1
	},
};

#define N_FMTS ARRAY_SIZE(sensor_formats)

/*
 * Then there is the issue of window sizes.
 * Try to capture the info here.
 */
static struct sensor_win_size sensor_win_sizes[] = {
	{
		.width      = 2960,
		.height     = 1666,
		.hoffset    = 0,
		.voffset    = 0,
		.hts        = 1200,
		.vts        = 2625,
		.pclk       = 63 * 1000 * 1000,
		.mipi_bps   = 864 * 1000 * 1000,
		.fps_fixed  = 20,
		.bin_factor = 1,
		.intg_min   = 1 << 4,
		.intg_max   = (2625 - 8) << 4,
		.gain_min   = 1 << 4,
		.gain_max   = 247 << 4,
		.regs       = sensor_default_regs,
		.regs_size  = ARRAY_SIZE(sensor_default_regs),
		.set_size   = NULL,
	},
};

#define N_WIN_SIZES (ARRAY_SIZE(sensor_win_sizes))

static int sensor_g_mbus_config(struct v4l2_subdev *sd,
				struct v4l2_mbus_config *cfg)
{
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

	 sensor_dbg("sensor_reg_init\n");

	 sensor_write_array(sd, sensor_fmt->regs, sensor_fmt->regs_size);

	if (wsize->regs)
		sensor_write_array(sd, wsize->regs, wsize->regs_size);

	if (wsize->set_size)
		wsize->set_size(sd);

	info->width = wsize->width;
	info->height = wsize->height;

	sensor_print("s_fmt set width = %d, height = %d\n", wsize->width,
		     wsize->height);

	return 0;
}

static int sensor_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct sensor_info *info = to_state(sd);

	sensor_print("%s on = %d, %d*%d fps: %d code: %x\n", __func__, enable,
		     info->current_wins->width, info->current_wins->height,
		     info->current_wins->fps_fixed, info->fmt->mbus_code);

	return sensor_reg_init(info);
}

/* ----------------------------------------------------------------------- */

static const struct v4l2_ctrl_ops sensor_ctrl_ops = {
	.g_volatile_ctrl = sensor_g_ctrl,
	.s_ctrl = sensor_s_ctrl,
};

static const struct v4l2_subdev_core_ops sensor_core_ops = {
	.reset = sensor_reset,
	.s_power = sensor_power,
	.init = sensor_init,
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

static int sensor_init_controls(struct v4l2_subdev *sd,
				const struct v4l2_ctrl_ops *ops)
{
	struct sensor_info *info = to_state(sd);
	struct v4l2_ctrl_handler *handler = &info->handler;
	struct v4l2_ctrl *ctrl;
	int ret = 0;

	v4l2_ctrl_handler_init(handler, 2);

	v4l2_ctrl_new_std(handler, ops, V4L2_CID_GAIN,
			  1 * 16, 256 * 16, 1, 16);
	ctrl = v4l2_ctrl_new_std(handler, ops, V4L2_CID_EXPOSURE,
				 3 * 16, 65536 * 16, 1, 3 * 16);
	if (ctrl)
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
	if (!info)
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
