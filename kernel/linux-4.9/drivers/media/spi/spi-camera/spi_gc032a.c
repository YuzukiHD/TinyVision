// SPDX-License-Identifier: GPL-2.0+

#include "spi_camera.h"

#define REG_DLY 0xffff
#define V4L2_IDENT_SENSOR 0x232a
#define SPI_SENSOR_NAME "gc032a"
#define MCLK              (24*1000*1000)

struct spi_sensor_win_size {
	int width;
	int height;
	unsigned int pclk;        //pixel clock in Hz

	void *regs; /* Regs to tweak */
	int regs_size;

	int code; /* Encoded data */
};

struct reg_list_a8_d8 {
	unsigned char addr;
	unsigned char data;
};

#if defined SPI_4_WIRE
static struct reg_list_a8_d8 sensor_resolution_regs[] = {
};
#elif defined SPI_2_WIRE
static struct reg_list_a8_d8 sensor_resolution_regs[] = {
};
#else
//1w sdr 48M
static struct reg_list_a8_d8 sensor_resolution_regs[] = {
	{0xf3, 0x81},
	{0xf5, 0x0c},
	{0xf7, 0x01},
	{0xf8, 0x03},
	{0xf9, 0x4e},
	{0xfa, 0x30},
	{0xfc, 0x02},
	{0xfe, 0x02},
	{0x81, 0x03},
	/*Analog&Cisctl*/
	{0xfe, 0x00},
	{0x03, 0x01},
	{0x04, 0xc2},
	{0x05, 0x01},
	{0x06, 0xb7},
	{0x07, 0x00},
	{0x08, 0x10},
	{0x0a, 0x04},
	{0x0c, 0x04},
	{0x0d, 0x01},
	{0x0e, 0xe8},
	{0x0f, 0x02},
	{0x10, 0x88},
	{0x17, 0x54},
	{0x19, 0x04},
	{0x1a, 0x0a},
	{0x1f, 0x40},
	{0x20, 0x30},
	{0x2e, 0x80},
	{0x2f, 0x2b},
	{0x30, 0x1a},
	{0xfe, 0x02},
	{0x03, 0x02},
	{0x05, 0xd7},
	{0x06, 0x60},
	{0x08, 0x80},
	{0x12, 0x89},

	/*blk*/
	{0xfe, 0x00},
	{0x18, 0x02},
	{0xfe, 0x02},
	{0x40, 0x22},
	{0x45, 0x00},
	{0x46, 0x00},
	{0x49, 0x20},
	{0x4b, 0x3c},
	{0x50, 0x20},
	{0x42, 0x10},
	/*isp*/
	{0xfe, 0x01},
	{0x0a, 0xc5},
	{0x45, 0x00},
	{0xfe, 0x00},
	{0x40, 0xff},
	{0x41, 0x25},
	{0x42, 0x83},
	{0x43, 0x10},
	{0x44, 0x83},
	{0x46, 0x26},
	{0x49, 0x03},
	{0x4f, 0x01},
	{0xde, 0x84},
	{0xfe, 0x02},
	{0x22, 0xf6},
	/*Shading*/
	{0xfe, 0x00},
	{0x77, 0x65},
	{0x78, 0x40},
	{0x79, 0x52},
	{0xfe, 0x01},
	{0xc1, 0x3c},
	{0xc2, 0x50},
	{0xc3, 0x00},
	{0xc4, 0x32},
	{0xc5, 0x24},
	{0xc6, 0x16},
	{0xc7, 0x08},
	{0xc8, 0x08},
	{0xc9, 0x00},
	{0xca, 0x20},
	{0xdc, 0x7a},
	{0xdd, 0x7a},
	{0xde, 0xa6},
	{0xdf, 0x60},
	/*AWB*/
	{0xfe, 0x01},
	{0x7c, 0x09},
	{0x65, 0x06},
	{0x7c, 0x08},
	{0x56, 0xf4},
	{0x66, 0x0f},
	{0x67, 0x84},
	{0x6b, 0x80},
	{0x6d, 0x12},
	{0x6e, 0xb0},
	{0x86, 0x00},
	{0x87, 0x00},
	{0x88, 0x00},
	{0x89, 0x00},
	{0x8a, 0x00},
	{0x8b, 0x00},
	{0x8c, 0x00},
	{0x8d, 0x00},
	{0x8e, 0x00},
	{0x8f, 0x00},
	{0x90, 0xef},
	{0x91, 0xe1},
	{0x92, 0x0c},
	{0x93, 0xef},
	{0x94, 0x65},
	{0x95, 0x1f},
	{0x96, 0x0c},
	{0x97, 0x2d},
	{0x98, 0x20},
	{0x99, 0xaa},
	{0x9a, 0x3f},
	{0x9b, 0x2c},
	{0x9c, 0x5f},
	{0x9d, 0x3e},
	{0x9e, 0xaa},
	{0x9f, 0x67},
	{0xa0, 0x60},
	{0xa1, 0x00},
	{0xa2, 0x00},
	{0xa3, 0x0a},
	{0xa4, 0xb6},
	{0xa5, 0xac},
	{0xa6, 0xc1},
	{0xa7, 0xac},
	{0xa8, 0x55},
	{0xa9, 0xc3},
	{0xaa, 0xa4},
	{0xab, 0xba},
	{0xac, 0xa8},
	{0xad, 0x55},
	{0xae, 0xc8},
	{0xaf, 0xb9},
	{0xb0, 0xd4},
	{0xb1, 0xc3},
	{0xb2, 0x55},
	{0xb3, 0xd8},
	{0xb4, 0xce},
	{0xb5, 0x00},
	{0xb6, 0x00},
	{0xb7, 0x05},
	{0xb8, 0xd6},
	{0xb9, 0x8c},
	/*CC*/
	{0xfe, 0x01},
	{0xd0, 0x40},//3a
	{0xd1, 0xf8},
	{0xd2, 0x00},
	{0xd3, 0xfa},
	{0xd4, 0x45},
	{0xd5, 0x02},
	{0xd6, 0x30},
	{0xd7, 0xfa},
	{0xd8, 0x08},
	{0xd9, 0x08},
	{0xda, 0x58},
	{0xdb, 0x02},
	{0xfe, 0x00},
	/*Gamma*/
	{0xfe, 0x00},
	{0xba, 0x00},
	{0xbb, 0x04},
	{0xbc, 0x0a},
	{0xbd, 0x0e},
	{0xbe, 0x22},
	{0xbf, 0x30},
	{0xc0, 0x3d},
	{0xc1, 0x4a},
	{0xc2, 0x5d},
	{0xc3, 0x6b},
	{0xc4, 0x7a},
	{0xc5, 0x85},
	{0xc6, 0x90},
	{0xc7, 0xa5},
	{0xc8, 0xb5},
	{0xc9, 0xc2},
	{0xca, 0xcc},
	{0xcb, 0xd5},
	{0xcc, 0xde},
	{0xcd, 0xea},
	{0xce, 0xf5},
	{0xcf, 0xff},
	/*Auto Gamma*/
	{0xfe, 0x00},
	{0x5a, 0x08},
	{0x5b, 0x0f},
	{0x5c, 0x15},
	{0x5d, 0x1c},
	{0x5e, 0x28},
	{0x5f, 0x36},
	{0x60, 0x45},
	{0x61, 0x51},
	{0x62, 0x6a},
	{0x63, 0x7d},
	{0x64, 0x8d},
	{0x65, 0x98},
	{0x66, 0xa2},
	{0x67, 0xb5},
	{0x68, 0xc3},
	{0x69, 0xcd},
	{0x6a, 0xd4},
	{0x6b, 0xdc},
	{0x6c, 0xe3},
	{0x6d, 0xf0},
	{0x6e, 0xf9},
	{0x6f, 0xff},
	/*Gain*/
	{0xfe, 0x00},
	{0x70, 0x50},
	/*AEC*/
	{0xfe, 0x00},
	{0x4f, 0x01},

	{0xfe, 0x01},
	{0x12, 0xa0},
	{0x13, 0x3a},
	{0x44, 0x04},
	{0x1f, 0x30},
	{0x20, 0x40},
	{0x26, 0x26},
	{0x27, 0x01},
	{0x28, 0xee},
	{0x29, 0x02},
	{0x2a, 0x3a},
	{0x2b, 0x02},
	{0x2c, 0xac},
	{0x2d, 0x02},
	{0x2e, 0xf8},
	{0x2f, 0x0b},
	{0x30, 0x6e},
	{0x31, 0x0e},
	{0x32, 0x70},
	{0x33, 0x12},
	{0x34, 0x0c},
	{0x3c, 0x20}, //[5:4] Max level setting
	{0x3e, 0x20},
	{0x3f, 0x2d},
	{0x40, 0x40},
	{0x41, 0x5b},
	{0x42, 0x82},
	{0x43, 0xb7},
	{0x04, 0x0a},
	{0x02, 0x79},
	{0x03, 0xc0},
	/*measure window*/
	{0xcc, 0x08},
	{0xcd, 0x08},
	{0xce, 0xa4},
	{0xcf, 0xec},
	/*DNDD*/
	{0xfe, 0x00},
	{0x81, 0xb8},//f8
	{0x82, 0x12},
	{0x83, 0x0a},
	{0x84, 0x01},
	{0x86, 0x50},
	{0x87, 0x18},
	{0x88, 0x10},
	{0x89, 0x70},
	{0x8a, 0x20},
	{0x8b, 0x10},
	{0x8c, 0x08},
	{0x8d, 0x0a},
	/*Intpee*/
	{0xfe, 0x00},
	{0x8f, 0xaa},
	{0x90, 0x9c},
	{0x91, 0x52},
	{0x92, 0x03},
	{0x93, 0x03},
	{0x94, 0x08},
	{0x95, 0x44},
	{0x97, 0x00},
	{0x98, 0x00},
	/*ASDE*/
	{0xfe, 0x00},
	{0xa1, 0x30},
	{0xa2, 0x41},
	{0xa4, 0x30},
	{0xa5, 0x20},
	{0xaa, 0x30},
	{0xac, 0x32},
	/*YCP*/
	{0xfe, 0x00},
	{0xd1, 0x3c},
	{0xd2, 0x3c},
	{0xd3, 0x38},
	{0xd6, 0xf4},
	{0xd7, 0x1d},
	{0xdd, 0x73},
	{0xde, 0x84},
	/*SPI*/
	{0xfe, 0x03},
	{0x51, 0x01},
	{0x52, 0xda},
	{0x53, 0x20},
	{0x54, 0x20},
	{0x55, 0x00},
	{0x59, 0x10},
	{0x5a, 0x00},
	{0x5b, 0x80},
	{0x5c, 0x02},
	{0x5d, 0xe0},
	{0x5e, 0x01},
	{0x64, 0x01},
};
#endif

static struct spi_sensor_win_size sensor_win_sizes[] = {
	{
		.width = 640,
		.height = 480,
		.regs = sensor_resolution_regs,
		.regs_size = ARRAY_SIZE(sensor_resolution_regs),
		.code = 1,
	},
};

#define N_WIN_SIZES (ARRAY_SIZE(sensor_win_sizes))

static int sensor_read(struct v4l2_subdev *sd, unsigned char addr, unsigned char *value)
{
	unsigned char data[2];
	struct i2c_msg msg[2];
	int ret;
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	data[0] = addr;
	data[1] = 0xee;

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = 1;
	msg[0].buf = &data[0];

	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 1;
	msg[1].buf = &data[1];

	ret = i2c_transfer(client->adapter, msg, 2);
	if (ret >= 0) {
		*value = data[1];
		ret = 0;
	} else {
		pr_err("%s error! slave = 0x%x, addr = 0x%2x, value = 0x%2x\n",
			__func__, client->addr, addr, *value);
	}
	return ret;
}

static int sensor_write(struct v4l2_subdev *sd, unsigned char addr, unsigned char value)
{
	unsigned char data[2];
	struct i2c_msg msg;
	int ret;
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	data[0] = addr;
	data[1] = value;

	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = 2;
	msg.buf = data;

	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret >= 0)
		ret = 0;
	else
		pr_err("%s error! slave = 0x%x, addr = 0x%2x, value = 0x%2x\n",
			__func__, client->addr, addr, value);

	return ret;
}

static int sensor_write_array(struct v4l2_subdev *sd, struct reg_list_a8_d8 *regs, int array_size)
{
	int i = 0;
	int ret = 0;

	if (!regs)
		return -EINVAL;

	while (i < array_size) {
		if (regs->addr == REG_DLY) {
			msleep(regs->data);
		} else {
			ret = sensor_write(sd, regs->addr, regs->data);
			if (ret < 0)
				return ret;
		}

		i++;
		regs++;
	}

	return 0;
}

static int sensor_detect(struct v4l2_subdev *sd)
{
	unsigned char rdval = 0x10;

	if (sensor_read(sd, 0xf0, &rdval) < 0) {
		pr_err("%s read 0xf0 return error\n", __func__);
		return -ENODEV;
	}
	if ((V4L2_IDENT_SENSOR >> 8) != rdval) {
		pr_err("%s read 0xf0 return 0x%x, gc032a id is 0x%x\n",
			__func__, rdval, V4L2_IDENT_SENSOR);
		return -ENODEV;
	}

	if (sensor_read(sd, 0xf1, &rdval) < 0) {
		pr_err("%s read 0xf1 return error\n", __func__);
		return -ENODEV;
	}
	if ((V4L2_IDENT_SENSOR & 0xff) != rdval) {
		pr_err("%s read 0xf1 return 0x%x, gc032a id is 0x%x\n",
			__func__, rdval, V4L2_IDENT_SENSOR);
		return -ENODEV;
	}

	return 0;
}

static int sensor_init(struct v4l2_subdev *sd, u32 val)
{
	int ret;
	/*Make sure it is a target sensor*/
	ret = sensor_detect(sd);
	if (ret) {
		pr_err("chip found is not an target chip.\n");
		return ret;
	}

	return 0;
}

static int sensor_power(struct v4l2_subdev *sd, int on)
{
	struct spi_sensor *gc032a = (struct spi_sensor *)dev_get_drvdata(sd->v4l2_dev->dev);

	switch (on) {
	case PWR_ON:
		gpio_direction_output(gc032a->sensor_pwdn_io, 0);
		__gpio_set_value(gc032a->sensor_pwdn_io, 0); /* set low */
		usleep_range(10000, 12000);
		__gpio_set_value(gc032a->sensor_pwdn_io, 1); /* set high */
		usleep_range(10000, 12000);
		__gpio_set_value(gc032a->sensor_pwdn_io, 0); /* set low */
		break;
	case PWR_OFF:
		__gpio_set_value(gc032a->sensor_pwdn_io, 1); /* set low */
		usleep_range(10000, 12000);
		__gpio_set_value(gc032a->sensor_pwdn_io, 0);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int sensor_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	return 0;
}

static int sensor_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	return 0;
}

static int sensor_enum_size(struct v4l2_subdev *sd, struct v4l2_subdev_pad_config *cfg,
	struct v4l2_subdev_frame_size_enum *fse)
{
	if (fse->index > N_WIN_SIZES - 1)
		return -EINVAL;

	fse->max_height = sensor_win_sizes[fse->index].width;
	fse->max_width = sensor_win_sizes[fse->index].height;

	return 0;
}

static int sensor_try_fmt_internal(struct v4l2_subdev *sd, struct v4l2_mbus_framefmt *fmt,
	struct spi_sensor_win_size **ret_wsize)
{
	struct spi_sensor_win_size *wsize;

	for (wsize = sensor_win_sizes; wsize < sensor_win_sizes + N_WIN_SIZES; wsize++)
		if (fmt->width >= wsize->width && fmt->height >= wsize->height)
			break;

	if (wsize >= sensor_win_sizes + N_WIN_SIZES)
		wsize--;   /* Take the smallest one */
	if (ret_wsize != NULL)
		*ret_wsize = wsize;

	fmt->width = wsize->width;
	fmt->height = wsize->height;
	fmt->code = wsize->code;

	return 0;
}

static struct v4l2_mbus_framefmt sensor_fmt;
static int sensor_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct spi_sensor_win_size *wsize;
	int ret = 0;

	sensor_try_fmt_internal(sd, &sensor_fmt, &wsize);

	//write resolution setting when streamon
	if (wsize->regs) {
		ret = sensor_write_array(sd, wsize->regs, wsize->regs_size);
		if (ret < 0)
			pr_err("%s call sensor_write_array return %d\n", __func__, ret);
	}

	return ret;
}

static int sensor_set_fmt(struct v4l2_subdev *sd, struct v4l2_subdev_pad_config *cfg,
	struct v4l2_subdev_format *format)
{
	struct spi_sensor_win_size *wsize;
	struct v4l2_mbus_framefmt *fmt = &format->format;
	int ret = 0;

	memcpy(&sensor_fmt, fmt, sizeof(sensor_fmt));
	pr_info("enter %s\n", __func__);
	ret = sensor_try_fmt_internal(sd, fmt, &wsize);

	return ret;
}

static const struct v4l2_subdev_core_ops sensor_core_ops = {
	.init = sensor_init,
	.s_power = sensor_power,
};

static const struct v4l2_subdev_video_ops sensor_video_ops = {
	.s_parm = sensor_s_parm,
	.g_parm = sensor_g_parm,
	.s_stream = sensor_s_stream,
};

static const struct v4l2_subdev_pad_ops sensor_pad_ops = {
	.set_fmt = sensor_set_fmt,
	.enum_frame_size = sensor_enum_size,
};

static const struct v4l2_subdev_ops sensor_ops = {
	.core = &sensor_core_ops,
	.video = &sensor_video_ops,
	.pad = &sensor_pad_ops,
};

static int sensor_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct v4l2_subdev *sd;

	sd = kzalloc(sizeof(*sd), GFP_KERNEL);
	if (!sd)
		return -ENOMEM;

	v4l2_i2c_subdev_init(sd, client, &sensor_ops);
	i2c_set_clientdata(client, sd);

	return 0;
}

static int sensor_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd;

	sd = (struct v4l2_subdev *)i2c_get_clientdata(client);
	v4l2_device_unregister_subdev(sd);
	kfree(sd);

	return 0;
}

static const struct i2c_device_id sensor_id[] = {
		{SPI_SENSOR_NAME, 0},
};

MODULE_DEVICE_TABLE(i2c, sensor_id);
static struct i2c_driver sensor_driver = {
		.driver = {
			.owner = THIS_MODULE,
			.name = SPI_SENSOR_NAME,
		},
		.probe = sensor_probe,
		.remove = sensor_remove,
		.id_table = sensor_id,
};

static int i2c_sensor_init(void)
{
	pr_info("__func__\n", __func__);
	i2c_add_driver(&sensor_driver);

	return 0;
}

static void i2c_sensor_exit(void)
{
	pr_info("%s exit!\n", __func__);
	i2c_del_driver(&sensor_driver);
}

device_paralell_initcall(i2c_sensor_init);
module_exit(i2c_sensor_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("allwinner");


