// SPDX-License-Identifier: GPL-2.0+

#include "spi_camera.h"

#define SPI_SENSOR_NAME "gc032a"

static int spi_sensor_open(struct file *file)
{
	struct spi_sensor *gc032a = video_drvdata(file);

	pr_info("%s\n", __func__);
	set_bit(DRIVER_BUSY, &gc032a->state);

	return 0;
}

static int spi_sensor_close(struct file *file)
{
	struct spi_sensor *gc032a = video_drvdata(file);
	int ret = 0;

	pr_info("%s\n", __func__);

	if (!driver_busy(gc032a)) {
		pr_err("%s: video have been closed!\n", __func__);
		return 0;
	}

	if (driver_streaming(gc032a)) {
		disable_irq(gc032a->cb_irq);
		//wait all work done before streamoff
		flush_work(&gc032a->spi_rx_work);
		ret = vb2_ioctl_streamoff(file, NULL, V4L2_CAP_VIDEO_CAPTURE_MPLANE);
		if (ret != 0)
			pr_err("%s: vb2_ioctl_streamoff error\n", __func__);
		clear_bit(DRIVER_STREAM, &gc032a->state);
	}
	v4l2_subdev_call(gc032a->sd, core, s_power, PWR_OFF);
	clear_bit(DRIVER_BUSY, &gc032a->state);

	return 0;
}

static int queue_setup(struct vb2_queue *vq,
		       unsigned int *nbuffers, unsigned int *nplanes,
		       unsigned int sizes[], struct device *alloc_devs[])
{
	struct spi_sensor *gc032a = vb2_get_drv_priv(vq);
	struct spi_device *spi = (struct spi_device *)gc032a->spidev;
	static u64 dma_mask = DMA_BIT_MASK(32);
	int size;

	*nplanes = 1;
	//YUV422, head and tail
	size = gc032a->height * gc032a->width * 2 +
		(gc032a->frame_unit_size * 2) +
		((gc032a->line_unit_size * 2) * gc032a->height);

	//gc032a frame data contains YUV422 data/frmae head/tail/due and line head/tail
	sizes[0] = size;
	if (sizes[0] == 0) {
		pr_info("%s size not right for YUV422\n", __func__);
		//sizes[0] =  640 * 480 * 2;
		sizes[0] = size;
	}

	spi->dev.dma_mask = &dma_mask;
	spi->dev.coherent_dma_mask = DMA_BIT_MASK(32);
	alloc_devs[0] = &spi->dev;
	return 0;
}

static int buffer_prepare(struct vb2_buffer *vb)
{
	struct spi_sensor *gc032a = vb2_get_drv_priv(vb->vb2_queue);
	struct vb2_v4l2_buffer *vvb = container_of(vb, struct vb2_v4l2_buffer, vb2_buf);
	struct spi_buffer *buf = container_of(vvb, struct spi_buffer, vb);

	unsigned long size;

	//gc032a frame data contains YUV422 data/frmae head/tail and line head/tail
	size = gc032a->height * gc032a->width * 2 +
		(gc032a->frame_unit_size * 2) +
		((gc032a->line_unit_size * 2) * gc032a->height);

	vb2_set_plane_payload(&buf->vb.vb2_buf, 0, size);
	vb->planes[0].m.offset = vb2_dma_contig_plane_dma_addr(vb, 0);

	return 0;
}

static void buffer_queue(struct vb2_buffer *vb)
{
	struct spi_sensor *gc032a = vb2_get_drv_priv(vb->vb2_queue);
	struct vb2_v4l2_buffer *vvb = container_of(vb, struct vb2_v4l2_buffer, vb2_buf);
	struct spi_buffer *buf = container_of(vvb, struct spi_buffer, vb);
	struct spi_dmaqueue *vidq = &gc032a->vidq;

	spin_lock(&gc032a->slock);
	list_add_tail(&buf->list, &vidq->active);
	spin_unlock(&gc032a->slock);
}

static void stop_streaming(struct vb2_queue *vq)
{
	struct spi_sensor *gc032a = vb2_get_drv_priv(vq);
	struct spi_dmaqueue *dma_q = &gc032a->vidq;
	unsigned long flags = 0;

	spin_lock_irqsave(&gc032a->slock, flags);

	/* Release all active buffers*/
	while (!list_empty(&dma_q->active)) {
		struct spi_buffer *spi_buf;

		spi_buf = list_entry(dma_q->active.next, struct spi_buffer, list);
		list_del(&spi_buf->list);
		vb2_buffer_done(&spi_buf->vb.vb2_buf, VB2_BUF_STATE_ERROR);
		pr_info("stop streaming buffer %d stop\n", spi_buf->vb.vb2_buf.index);
	}

	spin_unlock_irqrestore(&gc032a->slock, flags);
}

static void strip_buffer(struct spi_sensor *gc032a, void *start, int *length)
{
	char *front = (char *)start;
	char *behind = (char *)start;
	const int FRAME_UNIT = gc032a->frame_unit_size;  //frame head and tail size
	const int LINE_UNIT = gc032a->line_unit_size;   //line head and tail size
	const int WIDTH = gc032a->width;
	const int HEIGHT = gc032a->height;
	int LINE_DATA_LENGTH = WIDTH * 2; //YUV422
	int data_length = 0;
	int i;

	//front prt skip frame head
	front = front + FRAME_UNIT;
	for (i = 0; i < HEIGHT; i++) {
		front = front + LINE_UNIT; //skip line head
		strncpy(behind, front, LINE_DATA_LENGTH);
		front += LINE_DATA_LENGTH;
		behind += LINE_DATA_LENGTH;
		data_length += LINE_DATA_LENGTH;
		front = front + LINE_UNIT;  //skip line end
	}
	front = front + FRAME_UNIT;
	*length = data_length;
}

static void buffer_finish(struct vb2_buffer *vb)
{
	struct spi_sensor *gc032a = vb2_get_drv_priv(vb->vb2_queue);
	void *buffer = vb2_plane_vaddr(vb, 0);
	int length = 0;

	if (gc032a->strip_buffer) {
		strip_buffer(gc032a, buffer, &length);
		vb2_set_plane_payload(vb, 0, length);
	}
}

static struct vb2_ops spi_video_qops = {
	.queue_setup  = queue_setup,
	.buf_prepare  = buffer_prepare,
	.buf_queue    = buffer_queue,
	.stop_streaming	= stop_streaming,
	.buf_finish = buffer_finish,
};

/*
 * IOCTL vidioc handling
 */
static int vidioc_querycap(struct file *file, void  *priv, struct v4l2_capability *cap)
{
	struct spi_sensor *gc032a = video_drvdata(file);

	strlcpy(cap->driver, "spi-sensor", sizeof(cap->driver));

	strlcpy(cap->card, "spi-sensor", sizeof(cap->card));

	strlcpy(cap->bus_info, gc032a->v4l2_dev.name, sizeof(cap->bus_info));

	cap->version = 1;
	cap->capabilities = V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_STREAMING |
			V4L2_CAP_READWRITE | V4L2_CAP_DEVICE_CAPS;

	cap->device_caps |= V4L2_CAP_EXT_PIX_FORMAT;

	return 0;
}

static int vidioc_enum_framesizes(struct file *file, void *fh, struct v4l2_frmsizeenum *fsize)
{
	struct spi_sensor *gc032a = video_drvdata(file);
	struct v4l2_subdev_frame_size_enum fse;
	int ret;

	if (!gc032a || !gc032a->sd->ops->pad->enum_frame_size)
		return -EINVAL;

	fse.index = fsize->index;
	ret = v4l2_subdev_call(gc032a->sd, pad, enum_frame_size, NULL, &fse);
	if (ret >= 0) {
		fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
		fsize->discrete.width = fse.max_height;
		fsize->discrete.height = fse.max_width;
	}

	return ret;
}

static int vidioc_s_input(struct file *file, void *priv, unsigned int i)
{
	struct spi_sensor *gc032a = video_drvdata(file);
	int ret = 0;

	ret = v4l2_subdev_call(gc032a->sd, core, s_power, PWR_ON);
	pr_info("%s: ################  sensor power_on______________________\n", __func__);
	ret = v4l2_subdev_call(gc032a->sd, core, init, 0);

	return ret;
}

static int vidioc_s_parm(struct file *file, void *priv, struct v4l2_streamparm *parms)
{
	struct spi_sensor *gc032a = video_drvdata(file);
	int ret = 0;

	pr_err("enter %s\n", __func__);

	if (parms->parm.capture.capturemode != V4L2_MODE_VIDEO &&
	    parms->parm.capture.capturemode != V4L2_MODE_IMAGE &&
	    parms->parm.capture.capturemode != V4L2_MODE_PREVIEW) {
		parms->parm.capture.capturemode = V4L2_MODE_PREVIEW;
	}
	ret = v4l2_subdev_call(gc032a->sd, video, s_parm, parms);
	if (ret < 0)
		pr_err("v4l2 sub device s_parm error!\n");

	return ret;
}

static int vidioc_g_fmt_vid_cap(struct file *file, void *priv, struct v4l2_format *f)
{
	struct v4l2_pix_format_mplane *pixm;

	pixm = &f->fmt.pix_mp;
	pixm->field = V4L2_FIELD_NONE;
	pixm->num_planes = 1;

	return 0;
}

static int vidioc_s_fmt_vid_cap(struct file *file, void *priv, struct v4l2_format *f)
{
	struct spi_sensor *gc032a = video_drvdata(file);
	struct v4l2_subdev_format format;
	int ret;

	format.format.width = f->fmt.pix.width;
	format.format.height = f->fmt.pix.height;
	ret = v4l2_subdev_call(gc032a->sd, pad, set_fmt, NULL, &format);

	gc032a->width = format.format.width;
	gc032a->height = format.format.height;
	gc032a->code = format.format.code;
	f->fmt.pix.width = format.format.width;
	f->fmt.pix.height = format.format.height;
	pr_info("%s get width:%d, height:%d\n", __func__, gc032a->width, gc032a->height);

	return ret;
}

static int vidioc_streamon(struct file *file, void *priv, enum v4l2_buf_type i)
{
	struct spi_sensor *gc032a = video_drvdata(file);
	struct spi_dmaqueue *dma_q = &gc032a->vidq;
	int ret = 0;

	vb2_ioctl_streamon(file, priv, i);

	if (driver_streaming(gc032a)) {
		pr_err("%s: video has already stream on\n", __func__);
		return -EBUSY;
	}

	set_bit(DRIVER_STREAM, &gc032a->state);
	if (!list_empty(&dma_q->active)) {
		/* need to queue buffer first before stream on */
		schedule_work(&gc032a->spi_rx_work); /* spi rx data */
		usleep_range(10000, 12000);
		ret = v4l2_subdev_call(gc032a->sd, video, s_stream, 1);
	} else {
		pr_info("enter %s, dma_q->active is empty\n", __func__);
	}
	//enable dma callback irq
	enable_irq(gc032a->cb_irq);

	return 0;
}

static int vidioc_streamoff(struct file *file, void *priv, enum v4l2_buf_type i)
{
	struct spi_sensor *gc032a = video_drvdata(file);
	int ret = 0;

	if (!driver_streaming(gc032a)) {
		pr_err("%s: video already streamoff\n", __func__);
		return -1;
	}

	disable_irq(gc032a->cb_irq);

	//wait all work done before streamoff
	flush_work(&gc032a->spi_rx_work);

	//videobuf_streamoff(&gc032a->vb_vidq);
	ret = vb2_ioctl_streamoff(file, priv, i);
	if (ret != 0)
		pr_err("%s vb2_ioctl_streamoff error\n", __func__);
	clear_bit(DRIVER_STREAM, &gc032a->state);
	return 0;
}

static const struct v4l2_file_operations spi_sensor_fops = {
	.owner = THIS_MODULE,
	.open = spi_sensor_open,
	.release = spi_sensor_close,
	.unlocked_ioctl = video_ioctl2,
	.mmap = vb2_fop_mmap,
	.poll = vb2_fop_poll,
};

static const struct v4l2_ioctl_ops spi_sensor_ioctl_ops = {
	.vidioc_querycap = vidioc_querycap,
	.vidioc_enum_framesizes = vidioc_enum_framesizes,
	.vidioc_s_input = vidioc_s_input,
	.vidioc_s_parm = vidioc_s_parm,
	.vidioc_g_fmt_vid_cap_mplane = vidioc_g_fmt_vid_cap,
	.vidioc_s_fmt_vid_cap_mplane = vidioc_s_fmt_vid_cap,
	.vidioc_reqbufs = vb2_ioctl_reqbufs,
	.vidioc_querybuf = vb2_ioctl_querybuf,
	.vidioc_streamon = vidioc_streamon,
	.vidioc_streamoff = vidioc_streamoff,
	.vidioc_qbuf = vb2_ioctl_qbuf,
	.vidioc_dqbuf = vb2_ioctl_dqbuf,
};

unsigned int os_gpio_request(struct gpio_config *pin_cfg)
{
	unsigned int ret = 0;

	if (pin_cfg->gpio == GPIO_INDEX_INVALID)
		return 0;

	ret = gpio_request(pin_cfg->gpio, NULL);
	if (ret != 0) {
		pr_err("%s failed, gpio=%d, ret=0x%x\n", __func__, pin_cfg->gpio, ret);
		return 0;
	}

	return pin_cfg->gpio;
}

int os_gpio_release(unsigned int p_handler)
{
	gpio_free(p_handler);

	return 0;
}

int os_gpio_set(struct gpio_config *pin_cfg)
{
	int ret = 0;
	char   pin_name[32];
	__u32 config;

	if (pin_cfg->gpio == GPIO_INDEX_INVALID)
		return 0;

	/* valid pin of sunxi-pinctrl,
	 * config pin attributes individually.
	 */
	sunxi_gpio_to_name(pin_cfg->gpio, pin_name);
	config = SUNXI_PINCFG_PACK(SUNXI_PINCFG_TYPE_FUNC, pin_cfg->mul_sel);
	pin_config_set(SUNXI_PINCTRL, pin_name, config);
	if (pin_cfg->pull != GPIO_PULL_DEFAULT) {
		config = SUNXI_PINCFG_PACK(SUNXI_PINCFG_TYPE_PUD, pin_cfg->pull);
		pin_config_set(SUNXI_PINCTRL, pin_name, config);
	}
	if (pin_cfg->drv_level != GPIO_DRVLVL_DEFAULT) {
		config = SUNXI_PINCFG_PACK(SUNXI_PINCFG_TYPE_DRV, pin_cfg->drv_level);
		pin_config_set(SUNXI_PINCTRL, pin_name, config);
	}
	if (pin_cfg->data != GPIO_DATA_DEFAULT) {
		config = SUNXI_PINCFG_PACK(SUNXI_PINCFG_TYPE_DAT, pin_cfg->data);
		pin_config_set(SUNXI_PINCTRL, pin_name, config);
	}

	return ret;
}

static irqreturn_t spi_sensor_isr(int irq, void *dev_id)
{
	struct spi_sensor *gc032a = (struct spi_sensor *)dev_id;
	struct spi_dmaqueue *dma_q = &gc032a->vidq;

	if (!list_empty(&dma_q->active)) {
		/* spi_sync */
		schedule_work(&gc032a->spi_rx_work); /* spi rx data */
	} else {
		pr_err("dma_q->active is empty\n");
	}

	return  IRQ_HANDLED;
}

static void spi_rx_work_handle(struct work_struct *work)
{
	struct spi_sensor *gc032a = container_of(work, struct spi_sensor, spi_rx_work);
	struct spi_device *spi = (struct spi_device *)gc032a->spidev;
	struct spi_dmaqueue *dma_q = &gc032a->vidq;
	struct spi_transfer transfer;
	struct spi_message message;
	struct spi_buffer *spi_buf;
	int ret;

	mutex_lock(&gc032a->spi_rx_mutex);

	//fetch buffer
	spi_buf = list_entry(dma_q->active.next, struct spi_buffer, list);

	//configure message and transfer
	memset(&transfer, 0, sizeof(transfer));
	transfer.rx_buf = vb2_plane_vaddr(&spi_buf->vb.vb2_buf, 0);
	transfer.rx_dma = vb2_dma_contig_plane_dma_addr(&spi_buf->vb.vb2_buf, 0);
	transfer.len = spi_buf->vb.vb2_buf.planes[0].bytesused;
#if defined SPI_4_WIRE
	transfer.rx_nbits = SPI_NBITS_QUAD;
#elif defined SPI_2_WIRE
	transfer.rx_nbits = SPI_NBITS_DUAL;
#endif

	memset(&message, 0, sizeof(message));
	spi_message_init(&message);
	message.is_dma_mapped = 1;
	spi_message_add_tail(&transfer, &message);

	ret = spi_sync(spi, &message);
	if (ret < 0)
		pr_err("%s: spi_sync return error %d\n", __func__, ret);

	if (&dma_q->active != dma_q->active.next->next) {
		spi_buf = list_entry(dma_q->active.next, struct spi_buffer, list);
		list_del(&spi_buf->list);
		vb2_buffer_done(&spi_buf->vb.vb2_buf, VB2_BUF_STATE_DONE);
	} else {
		pr_info("enter %s, only one buffer left for active\n", __func__);
	}

	mutex_unlock(&gc032a->spi_rx_mutex);
}

static int spi_sensor_probe(struct spi_device *spi)
{
	struct device_node *np = spi->dev.of_node;
	struct spi_sensor *gc032a;
	struct i2c_adapter *i2c_adap;
	struct v4l2_device *v4l2_dev;
	struct video_device *vdev;
	struct vb2_queue *q;
	int ret = 0;

	gc032a = kzalloc(sizeof(*gc032a), GFP_KERNEL);

	memset(gc032a, 0, sizeof(*gc032a));

	//i2c
	ret = of_property_read_u32(np, "sensor_twi_id", &gc032a->twi_id);
	if (ret) {
		gc032a->twi_id = 0;
		pr_err("fetch sensor_twi_id from device_tree failed\n");
	}

	ret = of_property_read_u32(np, "sensor_twi_addr", &gc032a->twi_addr);
	if (ret) {
		gc032a->twi_addr = 0;
		pr_err("fetch sensor_twi_addr from device_tree failed\n");
	}
	//sensor_pwdn
	gc032a->sensor_pwdn.gpio = of_get_named_gpio_flags(np, "sensor_pwdn", 0,
		(enum of_gpio_flags *)(&gc032a->sensor_pwdn));
	if (!gpio_is_valid(gc032a->sensor_pwdn.gpio))
		pr_err("%s: sensor_pwdn is invalid.\n", __func__);

	//sensor_cs
	gc032a->sensor_cs.gpio = of_get_named_gpio_flags(np, "sensor_cs", 0,
		(enum of_gpio_flags *)(&gc032a->sensor_cs));
	if (!gpio_is_valid(gc032a->sensor_cs.gpio))
		pr_err("%s: sensor_cs is invalid.\n", __func__);

	//cb interrupt
	gc032a->sensor_cb.gpio = of_get_named_gpio_flags(np, "sensor_cb", 0,
		(enum of_gpio_flags *)(&gc032a->sensor_cb));
	if (!gpio_is_valid(gc032a->sensor_cb.gpio))
		pr_err("%s: sensor_cb is invalid.\n", __func__);

	//to strip buffer or not
	ret = of_property_read_u32(np, "sensor_strip_buffer", &gc032a->strip_buffer);
	if (ret) {
		gc032a->strip_buffer = 0;
		pr_err("fetch strip_buffer config from device_tree failed\n");
	}

	//frame head and tail size
	ret = of_property_read_u32(np, "sensor_frame_unit_size", &gc032a->frame_unit_size);
	if (ret) {
		gc032a->frame_unit_size = 0;
		pr_err("fetch frame unit size from device_tree failed\n");
	}

	//line head and tail size
	ret = of_property_read_u32(np, "sensor_line_unit_size", &gc032a->line_unit_size);
	if (ret) {
		gc032a->line_unit_size = 0;
		pr_err("fetch line unit size from device_tree failed\n");
	}

	/* request gpio */
	gc032a->sensor_pwdn_io = os_gpio_request(&gc032a->sensor_pwdn);
	os_gpio_set(&gc032a->sensor_pwdn);

	gc032a->sensor_cs_io = os_gpio_request(&gc032a->sensor_cs);
	os_gpio_set(&gc032a->sensor_cs);

	gc032a->sensor_cb_io = os_gpio_request(&gc032a->sensor_cb);
	gpio_direction_input(gc032a->sensor_cb.gpio);
	gc032a->cb_irq = gpio_to_irq(gc032a->sensor_cb.gpio);

	/* request irq */
	if (devm_request_irq(&spi->dev, gc032a->cb_irq, spi_sensor_isr,
						   IRQF_TRIGGER_FALLING, "spi-sensor-cb", gc032a)) {
		pr_err("%s request irq %d failure\n", __func__, gc032a->cb_irq);
	} else {
		pr_info("%s request irq success, cb_irq:%d\n", __func__, gc032a->cb_irq);
	}
	disable_irq(gc032a->cb_irq);

	spin_lock_init(&gc032a->slock);
	mutex_init(&gc032a->buf_lock);
	INIT_LIST_HEAD(&gc032a->vidq.active);

	/* receive work task init */
	INIT_WORK(&gc032a->spi_rx_work, spi_rx_work_handle);

	/* v4l2 device register */
	v4l2_dev = &gc032a->v4l2_dev;
	strlcpy(v4l2_dev->name, "spi-sensor", sizeof(v4l2_dev->name));
	ret = v4l2_device_register(&spi->dev, v4l2_dev);
	if (ret)
		pr_err("%s: error registering v4l2 device\n", __func__);

	dev_set_drvdata(&spi->dev, gc032a);

	i2c_adap = i2c_get_adapter(gc032a->twi_id);

	gc032a->sensor_i2c_board.addr = (unsigned short)(gc032a->twi_addr >> 1);
	strncpy(gc032a->sensor_i2c_board.type,
		spi->modalias, sizeof(gc032a->sensor_i2c_board.type));
	pr_info("%s: sub device register %s i2c_addr = 0x%x start\n",
			__func__, gc032a->sensor_i2c_board.type, gc032a->twi_addr);
	gc032a->sd = v4l2_i2c_new_subdev_board(&gc032a->v4l2_dev, i2c_adap,
		&gc032a->sensor_i2c_board, NULL);
	if (IS_ERR_OR_NULL(gc032a->sd)) {
		i2c_put_adapter(i2c_adap);
		pr_err("%s: error registering v4l2 subdevice No such device!\n", __func__);
	}

	/* subdev register is OK, check sensor init */
	ret = v4l2_subdev_call(gc032a->sd, core, s_power, PWR_ON);
	pr_info("%s: sensor power_on______________________\n", __func__);
	ret = v4l2_subdev_call(gc032a->sd, core, init, 0);
	v4l2_subdev_call(gc032a->sd, core, s_power, PWR_OFF);
	pr_info("%s: sensor power_off______________________\n", __func__);

	q = &gc032a->vb_vidq;
	q->type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	q->io_modes = VB2_MMAP | VB2_USERPTR | VB2_DMABUF | VB2_READ;
	q->drv_priv = gc032a;
	q->buf_struct_size = sizeof(struct spi_buffer);
	q->ops = &spi_video_qops;
	q->mem_ops = &vb2_dma_contig_memops;
	q->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;
	q->lock = &gc032a->buf_lock;

	ret = vb2_queue_init(q);
	if (ret) {
		pr_err("vb2_queue_init() failed\n");
		return ret;
	}
	//video_device, put vb2_queue in
	vdev = &gc032a->video;
	vdev->v4l2_dev = &gc032a->v4l2_dev;
	dev_set_name(&vdev->dev, "spi-sensor");
	vdev->fops = &spi_sensor_fops;
	vdev->ioctl_ops = &spi_sensor_ioctl_ops;
	vdev->release = video_device_release;
	vdev->queue = &gc032a->vb_vidq;
	vdev->lock = &gc032a->buf_lock;
	ret = video_register_device(vdev, VFL_TYPE_GRABBER, 0);
	if (ret < 0)
		pr_err("%s: video register device fail\n", __func__);

	video_set_drvdata(vdev, gc032a);

	mutex_init(&gc032a->spi_rx_mutex);
	spi->bits_per_word = 8;
	spi_setup(spi);

	gc032a->spidev = spi;
	gc032a->state = 0;

	return ret;
}

static int spi_sensor_remove(struct spi_device *spi)
{
	struct spi_sensor *gc032a =  (struct spi_sensor *)dev_get_drvdata(&spi->dev);

	os_gpio_release(gc032a->sensor_pwdn_io);
	os_gpio_release(gc032a->sensor_cs_io);
	os_gpio_release(gc032a->sensor_cb_io);

	devm_free_irq(&spi->dev, gc032a->cb_irq, (void *)gc032a);

	video_unregister_device(&gc032a->video);
	v4l2_device_unregister(&gc032a->v4l2_dev);

	//spin_destroy(gc032a->slock);
	mutex_destroy(&gc032a->spi_rx_mutex);
	mutex_destroy(&gc032a->buf_lock);

	dev_set_drvdata(&spi->dev, NULL);
	kfree(gc032a);

	return 0;
}

static const struct spi_device_id spi_sensor_ids[] = {
	{SPI_SENSOR_NAME, 0}
};

static const struct of_device_id of_match_spi_sensor[] = {
	{.compatible = SPI_SENSOR_NAME,},
};

static struct spi_driver spi_sensor_driver = {
	.probe = spi_sensor_probe,
	.remove = spi_sensor_remove,
	.driver = {
		.name = SPI_SENSOR_NAME,
		.owner = THIS_MODULE,
		.of_match_table = of_match_spi_sensor,
	},
	.id_table = spi_sensor_ids,
};

static int spi_sensor_init(void)
{
	pr_info("%s\n", __func__);
	spi_register_driver(&spi_sensor_driver);

	return 0;
}

static void spi_sensor_exit(void)
{
	pr_info("%s\n", __func__);
	spi_unregister_driver(&spi_sensor_driver);
}

module_init(spi_sensor_init);
module_exit(spi_sensor_exit);

MODULE_LICENSE("GPL");
