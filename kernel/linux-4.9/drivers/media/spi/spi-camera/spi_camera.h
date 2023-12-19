/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __SPI_CAMERA_H__
#define __SPI_CAMERA_H__

#include <linux/init.h>
#include <linux/module.h>
#include <linux/ioctl.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/compat.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/of_gpio.h>
#include <linux/sunxi-gpio.h>
#include <linux/irqreturn.h>
#include <linux/interrupt.h>
#include <media/videobuf2-dma-contig.h>
#include<asm/cacheflush.h>
#include <media/videobuf2-core.h>
#include <media/v4l2-dev.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
#include <media/videobuf2-v4l2.h>
#include <media/sunxi_camera_v2.h>
#include <linux/i2c.h>
#include <linux/regulator/consumer.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/rtc.h>
#include <asm/io.h>

enum driver_state_flags {
	DRIVER_LPM,
	DRIVER_STREAM,
	DRIVER_BUSY,
	DRIVER_STATE_MAX,
};

enum power_seq_cmd {
	PWR_OFF = 0,
	PWR_ON = 1,
	STBY_OFF = 2,
	STBY_ON = 3,
};

/* buffer for one video frame */
struct spi_buffer {
	struct vb2_v4l2_buffer vb;
	struct videobuf_dmabuf *dma;
	int image_quality;
	struct list_head list;
};

struct spi_dmaqueue {
	struct list_head  active;
	/* Counters to control fps rate */
	int               frame;
	int               ini_jiffies;
};

struct spi_sensor {
	struct spi_device *spidev;
	struct v4l2_device v4l2_dev;
	struct video_device video;
	struct i2c_board_info sensor_i2c_board;
	struct v4l2_subdev *sd;
	struct vb2_queue vb_vidq;
	struct spi_dmaqueue vidq;
	spinlock_t slock;
	struct work_struct spi_rx_work;
	struct mutex spi_rx_mutex;
	int twi_id;
	int twi_addr;
	struct gpio_config sensor_pck;
	struct gpio_config sensor_mck;
	struct gpio_config sensor_hsync;
	struct gpio_config sensor_vsync;
	struct gpio_config sensor_pwdn;
	struct gpio_config sensor_cs;
	struct gpio_config sensor_cb;
	unsigned int sensor_pck_io;
	unsigned int sensor_mck_io;
	unsigned int sensor_hsync_io;
	unsigned int sensor_vsync_io;
	unsigned int sensor_pwdn_io;
	unsigned int sensor_cs_io;
	unsigned int sensor_cb_io;
	int cb_irq;
	struct clk *sensor_master_clk;
	struct clk *sensor_master_clk_src;
	struct mutex buf_lock;
	int width;
	int height;
	int code;
	struct spi_transfer transfer;
	struct spi_message message;
	unsigned long state;
	int strip_buffer;
	int frame_unit_size;
	int line_unit_size;
};

#define driver_busy(dev) test_bit(DRIVER_BUSY, &(dev)->state)
#define driver_streaming(dev) test_bit(DRIVER_STREAM, &(dev)->state)

#endif /* __SPI_CAMERA_H__ */

