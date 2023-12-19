/*
 * drivers/spi/spi-sunxi.c
 *
 * Copyright (C) 2012 - 2016 Reuuimlla Limited
 * Pan Nan <pannan@reuuimllatech.com>
 *
 * SUNXI SPI Controller Driver
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * 2013.5.7 Mintow <duanmintao@allwinnertech.com>
 *    Adapt to support sun8i/sun9i of Allwinner.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/pinctrl/consumer.h>
#include <linux/spi/spi.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/spi/spi.h>
#include <linux/spi/spi_bitbang.h>
#include <asm/cacheflush.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/highmem.h>

#ifdef CONFIG_DMA_ENGINE
#include <linux/dmaengine.h>
#include <linux/dma-mapping.h>
#include <linux/dma/sunxi-dma.h>
#endif

#include <linux/clk/sunxi.h>

#include "spi-sunxi.h"
#include "spi-slave-protocol.h"

#include <linux/regulator/consumer.h>
#ifdef CONFIG_MTD_SPI_NOR
#include <linux/memblock.h>
#include <boot_param.h>
#include <linux/mtd/mtd.h>
#endif

/* For debug */
#define SPI_ERR(fmt, arg...)	pr_warn("%s()%d - "fmt, __func__, __LINE__, ##arg)

static u32 debug_mask = 1;
#define dprintk(level_mask, fmt, arg...)				\
do {									\
	if (unlikely(debug_mask & level_mask))				\
		pr_warn("%s()%d - "fmt, __func__, __LINE__, ##arg);	\
} while (0)

#define SUNXI_SPI_OK   0
#define SUNXI_SPI_FAIL -1

#define XFER_TIMEOUT	5000

enum spi_mode_type {
	SINGLE_HALF_DUPLEX_RX,		/* single mode, half duplex read */
	SINGLE_HALF_DUPLEX_TX,		/* single mode, half duplex write */
	SINGLE_FULL_DUPLEX_RX_TX,	/* single mode, full duplex read and write */
	DUAL_HALF_DUPLEX_RX,		/* dual mode, half duplex read */
	DUAL_HALF_DUPLEX_TX,		/* dual mode, half duplex write */
	QUAD_HALF_DUPLEX_RX,		/* quad mode, half duplex read */
	QUAD_HALF_DUPLEX_TX,		/* quad mode, half duplex write */
	MODE_TYPE_NULL,
};

#ifdef CONFIG_DMA_ENGINE

enum spi_dma_dir {
	SPI_DMA_RWNULL,
	SPI_DMA_WDEV = DMA_TO_DEVICE,
	SPI_DMA_RDEV = DMA_FROM_DEVICE,
};

u64 sunxi_spi_dma_mask = DMA_BIT_MASK(32);

#endif

/*
 *#ifdef CONFIG_AW_MTD_SPINAND
 *struct sunxi_spi_ttransfer {
 *        struct spi_transfer tx;
 *#define PREALLOC_LEN  (4096+64)
 *        unsigned len;
 *        [>-1: no use 1: transfer 2: not transfer <]
 *#define INIT_STATE (-1)
 *#define TRANSFER_STATE (1)
 *#define NTRANSFER_STATE (2)
 *        int state;
 *};
 *#endif
 */

struct sunxi_spi {
	struct platform_device *pdev;
	struct spi_master *master;/* kzalloc */
	void __iomem *base_addr; /* register */
	u32 base_addr_phy;
	struct spi_device *spi;

	struct clk *pclk;  /* PLL clock */
	struct clk *mclk;  /* spi module clock */

	enum spi_mode_type mode_type;

	char dev_name[48];

	int result; /* 0: succeed -1:fail */

	struct task_struct *task;
	int task_flag;

	spinlock_t lock;

	struct completion done;  /* wakup another spi transfer */

	u32 mode; /* 0: master mode, 1: slave mode */
	int dbi_enabled;
	struct sunxi_slave *slave;

	struct pinctrl		 *pctrl;
	u32 sample_mode;
	u32 sample_delay;
/*
 *#ifdef CONFIG_AW_MTD_SPINAND
 *        struct sunxi_spi_ttransfer stx;
 *#endif
 */

};

void spi_dump_reg(struct sunxi_spi *sspi, u32 offset, u32 len)
{
	u32 i;
	u8 buf[64], cnt = 0;

	for (i = 0; i < len; i = i + REG_INTERVAL) {
		if (i%HEXADECIMAL == 0)
			cnt += sprintf(buf + cnt, "0x%08x: ",
					(u32)(sspi->base_addr_phy  + offset + i));

		cnt += sprintf(buf + cnt, "%08x ",
				readl(sspi->base_addr + offset + i));

		if (i%HEXADECIMAL == REG_CL) {
			pr_warn("%s\n", buf);
			cnt = 0;
		}
	}
}

void spi_dump_data(u8 *buf, u32 len)
{
	u32 i, cnt = 0;
	u8 *tmp;

	tmp = kzalloc(len, GFP_KERNEL);
	if (!tmp)
		return;

	for (i = 0; i < len; i++) {
		if (i%HEXADECIMAL == 0)
			cnt += sprintf(tmp + cnt, "0x%08x: ", i);

		cnt += sprintf(tmp + cnt, "%02x ", buf[i]);

		if ((i%HEXADECIMAL == REG_END) || (i == (len - 1))) {
			pr_warn("%s\n", tmp);
			cnt = 0;
		}
	}

	kfree(tmp);
}

static void dbi_disable_irq(u32 bitmap, void __iomem *base_addr)
{
	u32 reg_val = readl(base_addr + SPI_DBI_INT_REG);

	bitmap &= DBI_INT_STA_MASK;
	reg_val &= ~bitmap;
	writel(reg_val, base_addr + SPI_DBI_INT_REG);
}

static void dbi_enable_irq(u32 bitmap, void __iomem *base_addr)
{
	u32 reg_val = readl(base_addr + SPI_DBI_INT_REG);


	bitmap &= DBI_INT_STA_MASK;
	reg_val |= bitmap;
	writel(reg_val, base_addr + SPI_DBI_INT_REG);
}

/* config chip select */
static s32 spi_set_cs(u32 chipselect, void __iomem *base_addr)
{
	char ret;
	u32 reg_val = readl(base_addr + SPI_TC_REG);

	if (chipselect < 4) {
		reg_val &= ~SPI_TC_SS_MASK;/* SS-chip select, clear two bits */
		reg_val |= chipselect << SPI_TC_SS_BIT_POS;/* set chip select */
		writel(reg_val, base_addr + SPI_TC_REG);
		ret = SUNXI_SPI_OK;
	} else {
		SPI_ERR("Chip Select set fail! cs = %d\n", chipselect);
		ret = SUNXI_SPI_FAIL;
	}

	return ret;
}

static s32 set_dbi_timer_param(struct spi_device *spi, void __iomem *base_addr)
{
	u32 timer_val = 0, pixel_cycle = 0;
	s32 ret = -1;

	if (!spi || !base_addr)
		goto OUT;

	goto OUT; /*not use */

	if (spi->dbi_te_en || !spi->dbi_fps) {
		writel(0x0, base_addr + SPI_DBI_TIMER_REG);
		goto OUT;
	}

	if (spi->dbi_interface == D2LI) {
		switch (spi->dbi_format) {
		case DBI_RGB111:
			pixel_cycle = 8;
			break;
		case DBI_RGB444:
		case DBI_RGB565:
			pixel_cycle = 9;
			break;
		case DBI_RGB666:
			pixel_cycle = 10;
			break;
		case DBI_RGB888:
			pixel_cycle = 13;
			break;
		default:
			break;
		}
	} else {
		switch (spi->dbi_format) {
		case DBI_RGB111:
			pixel_cycle = 8;
			break;
		case DBI_RGB444:
			pixel_cycle = 12;
			break;
		case DBI_RGB565:
			pixel_cycle = 16;
			break;
		case DBI_RGB666:
		case DBI_RGB888:
			pixel_cycle = 24;
			break;
		default:
			break;
		}
	}
	timer_val = spi->max_speed_hz / spi->dbi_fps -
		    pixel_cycle * spi->dbi_video_h * spi->dbi_video_v;

	timer_val |= 0x80000000;
	writel(timer_val, base_addr + SPI_DBI_TIMER_REG);
	ret = 0;

OUT:
	return ret;
}

/* config dbi */
static void spi_config_dbi(struct spi_device *spi, void __iomem *base_addr)
{
	u32 config = spi->dbi_mode;
	u32 reg_val = 0;
	u32 reg_tmp = 0;

	/*1. command type */
	if (config & SPI_DBI_COMMAND_READ_) {
		reg_val |= DBI_CR_READ;
		reg_tmp = readl(base_addr + SPI_DBI_CR_REG1);
		writel(reg_tmp | spi->dbi_read_bytes,
			base_addr + SPI_DBI_CR_REG1);
	} else
		reg_val &= ~DBI_CR_READ;

	/*3. output data sequence */
	if (config & SPI_DBI_LSB_FIRST_)
		reg_val |= DBI_CR_LSB_FIRST;
	else
		reg_val &= ~DBI_CR_LSB_FIRST;

	/*4. transmit data type */
	if (config & SPI_DBI_TRANSMIT_VIDEO_) {
		reg_val |= DBI_CR_TRANSMIT_MODE;
		writel((spi->dbi_video_v << 16)|(spi->dbi_video_h),
			base_addr + SPI_DBI_VIDEO_SIZE);
		if (spi->dbi_te_en)
			dbi_enable_irq(DBI_TE_INT_EN, base_addr);
		else
			dbi_enable_irq(DBI_FRAM_DONE_INT_EN, base_addr);
	} else {
		reg_val &= ~DBI_CR_TRANSMIT_MODE;

		writel(0x0, base_addr + SPI_DBI_VIDEO_SIZE);
		dbi_disable_irq(DBI_FRAM_DONE_INT_EN | DBI_TE_INT_EN, base_addr);
		dbi_enable_irq(DBI_FIFO_EMPTY_INT_EN, base_addr);
	}


	/*5. output data format */
	reg_val &= ~(DBI_CR_FORMAT_MASK);
	if (spi->dbi_format == DBI_RGB111)
		reg_val &= ~(0x7 << DBI_CR_FORMAT);
	else
		reg_val |= ((spi->dbi_format) << DBI_CR_FORMAT);

	/*6. dbi interface select */
	reg_val &= ~(DBI_CR_INTERFACE_MASK);

	if (spi->dbi_interface == L3I1)
		reg_val &= ~((0x7) << DBI_CR_INTERFACE);
	else
		reg_val |= ((spi->dbi_interface) << DBI_CR_INTERFACE);

	if (spi->dbi_format <= DBI_RGB565)
		reg_val |= 0x1;
	else
		reg_val &= ~0x1;

	if (spi->dbi_out_sequence == DBI_OUT_RGB)
		reg_val &= ~((0x7) << 16);
	else
		reg_val |= ((spi->dbi_out_sequence) << 16);

	if (spi->dbi_src_sequence == DBI_SRC_RGB)
		reg_val &= ~((0xf) << 4);
	else
		reg_val |= ((spi->dbi_src_sequence) << 4);

	if (spi->dbi_rgb_bit_order == 1)
		reg_val |= ((0x1) << 2);
	else
		reg_val &= ~((0x1) << 2);

	if (spi->dbi_rgb32_alpha_pos == 1)
		reg_val |= ((0x1) << 1);
	else
		reg_val &= ~((0x1) << 1);

	writel(reg_val, base_addr + SPI_DBI_CR_REG);

	reg_val = 0;

	if (spi->dbi_interface == D2LI) {
		reg_val |= DBI_CR2_DCX_PIN;
		reg_val &= ~DBI_CR2_SDI_PIN;
	} else {
		reg_val |= DBI_CR2_SDI_PIN;
		reg_val &= ~DBI_CR2_DCX_PIN;
	}

	if ((spi->dbi_te_en == DBI_TE_DISABLE) ||
	    !(config & SPI_DBI_TRANSMIT_VIDEO_)) {
		reg_val &= ~(0x3 << 0); // te disable
	} else {
		/*te enable*/
		reg_val |= 0x1;
		if (spi->dbi_te_en == DBI_TE_FALLING_EDGE)
			reg_val |= (0x1 << 1);
		else
			reg_val &= ~(0x1 << 1);
	}

	writel(reg_val, base_addr + SPI_DBI_CR_REG2);


	dprintk(DEBUG_INFO, "DBI mode configurate : %x\n", reg_val);

	reg_val = 0;
	if (config & SPI_DBI_DCX_DATA_)
		reg_val |= DBI_CR1_DCX_DATA;
	else
		reg_val &= ~DBI_CR1_DCX_DATA;

	if (spi->dbi_rgb16_pixel_endian == 1)
		reg_val |= ((0x1) << 21);
	else
		reg_val &= ~((0x1) << 21);

	/* dbi en mode sel */
	if ((spi->dbi_te_en == DBI_TE_DISABLE) ||
	    !(config & SPI_DBI_TRANSMIT_VIDEO_)) {
		if (!set_dbi_timer_param(spi, base_addr))
			reg_val |= (0x2 << 29); // timer trigger mode
		else
			reg_val &= ~(0x3 << 29); // always on mode
	} else {
		/*te trigger mode */
		reg_val |= ((0x3) << 29);
	}

	/* config dbi clock mode: auto gating */
	if (spi->dbi_clk_out_mode == SPI_DBI_CLK_ALWAYS_ON)
		reg_val &= ~(DBI_CR1_CLK_AUTO);
	else
		reg_val |= DBI_CR1_CLK_AUTO;

	writel(reg_val, base_addr + SPI_DBI_CR_REG1);
}

/* config spi */
static void spi_config_tc(u32 config, void __iomem *base_addr)
{
	u32 reg_val = readl(base_addr + SPI_TC_REG);

	/*1. POL */
	if (config & SPI_POL_ACTIVE_)
		reg_val |= SPI_TC_POL;/*default POL = 1 */
	else
		reg_val &= ~SPI_TC_POL;

	/*2. PHA */
	if (config & SPI_PHA_ACTIVE_)
		reg_val |= SPI_TC_PHA;/*default PHA = 1 */
	else
		reg_val &= ~SPI_TC_PHA;

	/*3. SSPOL,chip select signal polarity */
	if (config & SPI_CS_HIGH_ACTIVE_)
		reg_val &= ~SPI_TC_SPOL;
	else
		reg_val |= SPI_TC_SPOL; /*default SSPOL = 1,Low level effect */

	/*4. LMTF--LSB/MSB transfer first select */
	if (config & SPI_LSB_FIRST_ACTIVE_)
		reg_val |= SPI_TC_FBS;
	else
		reg_val &= ~SPI_TC_FBS;/*default LMTF =0, MSB first */

	/*5. dummy burst type */
	if (config & SPI_DUMMY_ONE_ACTIVE_)
		reg_val |= SPI_TC_DDB;
	else
		reg_val &= ~SPI_TC_DDB;/*default DDB =0, ZERO */

	/*6.discard hash burst-DHB */
	if (config & SPI_RECEIVE_ALL_ACTIVE_)
		reg_val &= ~SPI_TC_DHB;
	else
		reg_val |= SPI_TC_DHB;/*default DHB =1, discard unused burst */

	/*7. set SMC = 1 , SSCTL = 0 ,TPE = 1 */
	reg_val &= ~SPI_TC_SSCTL;
	writel(reg_val, base_addr + SPI_TC_REG);
}

/* set spi clock */
static void spi_set_clk(u32 spi_clk, u32 ahb_clk, struct sunxi_spi *sspi)
{
	dprintk(DEBUG_INFO, "set spi clock %d, mclk %d\n", spi_clk, ahb_clk);

	clk_set_rate(sspi->mclk, spi_clk);
	if (clk_get_rate(sspi->mclk) != spi_clk) {
		clk_set_rate(sspi->mclk, ahb_clk);
		SPI_ERR("[spi%d] set spi clock failed, use clk:%d\n",
				sspi->master->bus_num, ahb_clk);
	}
}

/* delay internal read sample point*/
static void spi_sample_delay(u32 sdm, u32 sdc, u32 sdc1,
					void __iomem *base_addr)
{
	u32 reg_val = readl(base_addr + SPI_TC_REG);
	u32 org_val = reg_val;

	if (sdm)
		reg_val |= SPI_TC_SDM;
	else
		reg_val &= ~SPI_TC_SDM;

	if (sdc)
		reg_val |= SPI_TC_SDC;
	else
		reg_val &= ~SPI_TC_SDC;

	if (sdc1)
		reg_val |= SPI_TC_SDC1;
	else
		reg_val &= ~SPI_TC_SDC1;

	if (reg_val != org_val)
		writel(reg_val, base_addr + SPI_TC_REG);
}

static void spi_set_sample_mode(unsigned int mode, void __iomem *base_addr)
{
	unsigned int sample_mode[7] = {
		DELAY_NORMAL_SAMPLE, DELAY_0_5_CYCLE_SAMPLE,
		DELAY_1_CYCLE_SAMPLE, DELAY_1_5_CYCLE_SAMPLE,
		DELAY_2_CYCLE_SAMPLE, DELAY_2_5_CYCLE_SAMPLE,
		DELAY_3_CYCLE_SAMPLE
	};
	spi_sample_delay((sample_mode[mode] >> DELAY_SDM_POS) & 0xf,
			(sample_mode[mode] >> DELAY_SDC_POS) & 0xf,
			(sample_mode[mode] >>  DELAY_SDC1_POS)& 0xf,
			base_addr);
}

static void spi_samp_dl_sw_status(unsigned int status, void __iomem *base_addr)
{
	unsigned int rval = readl(base_addr + SPI_SAMPLE_DELAY_REG);

	if (status)
		rval |= SPI_SAMP_DL_SW_EN;
	else
		rval &= ~SPI_SAMP_DL_SW_EN;

	writel(rval, base_addr + SPI_SAMPLE_DELAY_REG);
}

static void spi_samp_mode_enable(unsigned int status, void __iomem *base_addr)
{
	unsigned int rval = readl(base_addr + SPI_GC_REG);

	if (status)
		rval |= SPI_SAMP_MODE_EN;
	else
		rval &= ~SPI_SAMP_MODE_EN;

	writel(rval, base_addr + SPI_GC_REG);
}

static void spi_set_sample_delay(unsigned int sample_delay,
		void __iomem *base_addr)
{
	unsigned int rval = readl(base_addr + SPI_SAMPLE_DELAY_REG)
					& (~(0x3f << 0));

	rval |= sample_delay;
	writel(rval, base_addr + SPI_SAMPLE_DELAY_REG);
}

/* start spi transfer */
static void spi_start_xfer(void __iomem *base_addr)
{
	u32 reg_val = readl(base_addr + SPI_TC_REG);

	reg_val |= SPI_TC_XCH;
	writel(reg_val, base_addr + SPI_TC_REG);
}

/* enable spi dbi */
static void spi_enable_dbi(void __iomem *base_addr)
{
	u32 reg_val = readl(base_addr + SPI_GC_REG);

	reg_val |= SPI_GC_DBI_EN;
	writel(reg_val, base_addr + SPI_GC_REG);
}

/* enable spi bus */
static void spi_enable_bus(void __iomem *base_addr)
{
	u32 reg_val = readl(base_addr + SPI_GC_REG);

	reg_val |= SPI_GC_EN;
	writel(reg_val, base_addr + SPI_GC_REG);
}

/* disbale spi bus */
static void spi_disable_bus(void __iomem *base_addr)
{
	u32 reg_val = readl(base_addr + SPI_GC_REG);

	reg_val &= ~SPI_GC_EN;
	writel(reg_val, base_addr + SPI_GC_REG);
}

/* set dbi mode */
static void spi_set_dbi(void __iomem *base_addr)
{
	u32 reg_val = readl(base_addr + SPI_GC_REG);

	reg_val |= SPI_GC_DBI_MODE_SEL;
	writel(reg_val, base_addr + SPI_GC_REG);
}

/* set master mode */
static void spi_set_master(void __iomem *base_addr)
{
	u32 reg_val = readl(base_addr + SPI_GC_REG);

	reg_val |= SPI_GC_MODE;
	writel(reg_val, base_addr + SPI_GC_REG);
}

/* set slaev mode */
static void spi_set_slave(void __iomem *base_addr)
{
	u32 reg_val = readl(base_addr + SPI_GC_REG);
	u32 val = SPI_GC_MODE;

	reg_val &= ~val;
	writel(reg_val, base_addr + SPI_GC_REG);
}

/* enable transmit pause */
static void spi_enable_tp(void __iomem *base_addr)
{
	u32 reg_val = readl(base_addr + SPI_GC_REG);

	reg_val |= SPI_GC_TP_EN;
	writel(reg_val, base_addr + SPI_GC_REG);
}

/* soft reset spi controller */
static void spi_soft_reset(void __iomem *base_addr)
{
	u32 reg_val = readl(base_addr + SPI_GC_REG);

	reg_val |= SPI_GC_SRST;
	writel(reg_val, base_addr + SPI_GC_REG);
}

/* enable irq type */
static void spi_enable_irq(u32 bitmap, void __iomem *base_addr)
{
	u32 reg_val = readl(base_addr + SPI_INT_CTL_REG);

	bitmap &= SPI_INTEN_MASK;
	reg_val |= bitmap;
	writel(reg_val, base_addr + SPI_INT_CTL_REG);
}

/* disable irq type */
static void spi_disable_irq(u32 bitmap, void __iomem *base_addr)
{
	u32 reg_val = readl(base_addr + SPI_INT_CTL_REG);

	bitmap &= SPI_INTEN_MASK;
	reg_val &= ~bitmap;
	writel(reg_val, base_addr + SPI_INT_CTL_REG);
}

#ifdef CONFIG_DMA_ENGINE
/* enable dma irq */
static void spi_enable_dma_irq(u32 bitmap, void __iomem *base_addr)
{
	u32 reg_val = readl(base_addr + SPI_FIFO_CTL_REG);

	bitmap &= SPI_FIFO_CTL_DRQEN_MASK;
	reg_val |= bitmap;
	writel(reg_val, base_addr + SPI_FIFO_CTL_REG);

	spi_set_dma_mode(base_addr);
}

/* disable dma irq */
static void spi_disable_dma_irq(u32 bitmap, void __iomem *base_addr)
{
	u32 reg_val = readl(base_addr + SPI_FIFO_CTL_REG);

	bitmap &= SPI_FIFO_CTL_DRQEN_MASK;
	reg_val &= ~bitmap;
	writel(reg_val, base_addr + SPI_FIFO_CTL_REG);
}

static void spi_enable_dbi_dma(void __iomem *base_addr)
{
	u32 reg_val = readl(base_addr + SPI_DBI_CR_REG2);

	reg_val |= DBI_CR2_DMA_ENABLE;
	writel(reg_val, base_addr + SPI_DBI_CR_REG2);
}

static void spi_disable_dbi_dma(void __iomem *base_addr)
{
	u32 reg_val = readl(base_addr + SPI_DBI_CR_REG2);

	reg_val &= ~(DBI_CR2_DMA_ENABLE);
	writel(reg_val, base_addr + SPI_DBI_CR_REG2);
}
#endif

/* query irq enable */
static u32 spi_qry_irq_enable(void __iomem *base_addr)
{
	return (SPI_INTEN_MASK & readl(base_addr + SPI_INT_CTL_REG));
}

/* query dbi irq pending */
static u32 dbi_qry_irq_pending(void __iomem *base_addr)
{
	return (DBI_INT_STA_MASK & readl(base_addr + SPI_DBI_INT_REG));
}

/* clear irq pending */
static void dbi_clr_irq_pending(u32 pending_bit, void __iomem *base_addr)
{
	pending_bit &= DBI_INT_STA_MASK;
	writel(pending_bit, base_addr + SPI_DBI_INT_REG);
}

/* query irq pending */
static u32 spi_qry_irq_pending(void __iomem *base_addr)
{
	return (SPI_INT_STA_MASK & readl(base_addr + SPI_INT_STA_REG));
}

/* clear irq pending */
static void spi_clr_irq_pending(u32 pending_bit, void __iomem *base_addr)
{
	pending_bit &= SPI_INT_STA_MASK;
	writel(pending_bit, base_addr + SPI_INT_STA_REG);
}

/* query txfifo bytes */
static u32 spi_query_txfifo(void __iomem *base_addr)
{
	u32 reg_val = (SPI_FIFO_STA_TX_CNT & readl(base_addr + SPI_FIFO_STA_REG));

	reg_val >>= SPI_TXCNT_BIT_POS;
	return reg_val;
}

/* query rxfifo bytes */
static u32 spi_query_rxfifo(void __iomem *base_addr)
{
	u32 reg_val = (SPI_FIFO_STA_RX_CNT & readl(base_addr + SPI_FIFO_STA_REG));

	reg_val >>= SPI_RXCNT_BIT_POS;
	return reg_val;
}

/* reset fifo */
static void spi_reset_fifo(void __iomem *base_addr)
{
	int time_out = 0xffff;
	u32 reg_val = readl(base_addr + SPI_FIFO_CTL_REG);

	reg_val |= (SPI_FIFO_CTL_RX_RST|SPI_FIFO_CTL_TX_RST);
	/* Set the trigger level of RxFIFO/TxFIFO. */
	reg_val &= ~(SPI_FIFO_CTL_RX_LEVEL|SPI_FIFO_CTL_TX_LEVEL);
	reg_val |= (0x20<<16) | 0x20;
	writel(reg_val, base_addr + SPI_FIFO_CTL_REG);

	while (readl(base_addr + SPI_FIFO_CTL_REG) &
			(SPI_FIFO_CTL_RX_RST | SPI_FIFO_CTL_TX_RST)) {
		time_out--;
		if (time_out <= 0)
			SPI_ERR("reset fifo time out\n");
	}
}

static void spi_set_rx_trig(u32 val, void __iomem *base_addr)
{
	u32 reg_val = readl(base_addr + SPI_FIFO_CTL_REG);

	reg_val &= ~SPI_FIFO_CTL_RX_LEVEL;
	reg_val |= val & SPI_FIFO_CTL_RX_LEVEL;
	writel(reg_val, base_addr + SPI_FIFO_CTL_REG);

}

/* set transfer total length BC, transfer length TC and single transmit length STC */
static void spi_set_bc_tc_stc(u32 tx_len, u32 rx_len, u32 stc_len, u32 dummy_cnt, void __iomem *base_addr)
{
	u32 reg_val = readl(base_addr + SPI_BURST_CNT_REG);

	/* set MBC(0x30) = tx_len + rx_len + dummy_cnt */
	reg_val &= ~SPI_BC_CNT_MASK;
	reg_val |= (SPI_BC_CNT_MASK & (tx_len + rx_len + dummy_cnt));
	writel(reg_val, base_addr + SPI_BURST_CNT_REG);

	/* set MTC(0x34) = tx_len */
	reg_val = readl(base_addr + SPI_TRANSMIT_CNT_REG);
	reg_val &= ~SPI_TC_CNT_MASK;
	reg_val |= (SPI_TC_CNT_MASK & tx_len);
	writel(reg_val, base_addr + SPI_TRANSMIT_CNT_REG);

	/* set BBC(0x38) = dummy cnt & single mode transmit counter */
	reg_val = readl(base_addr + SPI_BCC_REG);
	reg_val &= ~SPI_BCC_STC_MASK;
	reg_val |= (SPI_BCC_STC_MASK & stc_len);
	reg_val &= ~(0xf << 24);
	reg_val |= (dummy_cnt << 24);
	writel(reg_val, base_addr + SPI_BCC_REG);
}

/* set ss control */
static void spi_ss_ctrl(void __iomem *base_addr, u8 on_off)
{
	u32 reg_val = readl(base_addr + SPI_TC_REG);

	on_off &= 0x1;
	if (on_off)
		reg_val |= SPI_TC_SS_OWNER;
	else
		reg_val &= ~SPI_TC_SS_OWNER;
	writel(reg_val, base_addr + SPI_TC_REG);
}

/* set ss level */
static void spi_ss_level(void __iomem *base_addr, u8 hi_lo)
{
	u32 reg_val = readl(base_addr + SPI_TC_REG);

	hi_lo &= 0x1;
	if (hi_lo)
		reg_val |= SPI_TC_SS_LEVEL;
	else
		reg_val &= ~SPI_TC_SS_LEVEL;
	writel(reg_val, base_addr + SPI_TC_REG);
}

/* set dhb, 1: discard unused spi burst; 0: receiving all spi burst */
static void spi_set_all_burst_received(void __iomem *base_addr)
{
	u32 reg_val = readl(base_addr+SPI_TC_REG);

	reg_val &= ~SPI_TC_DHB;
	writel(reg_val, base_addr + SPI_TC_REG);
}

static void spi_disable_dual(void __iomem  *base_addr)
{
	u32 reg_val = readl(base_addr+SPI_BCC_REG);
	reg_val &= ~SPI_BCC_DUAL_MODE;
	writel(reg_val, base_addr + SPI_BCC_REG);
}

static void spi_enable_dual(void __iomem  *base_addr)
{
	u32 reg_val = readl(base_addr+SPI_BCC_REG);
	reg_val &= ~SPI_BCC_QUAD_MODE;
	reg_val |= SPI_BCC_DUAL_MODE;
	writel(reg_val, base_addr + SPI_BCC_REG);
}

static void spi_disable_quad(void __iomem  *base_addr)
{
	u32 reg_val = readl(base_addr+SPI_BCC_REG);

	reg_val &= ~SPI_BCC_QUAD_MODE;
	writel(reg_val, base_addr + SPI_BCC_REG);
}

static void spi_enable_quad(void __iomem  *base_addr)
{
	u32 reg_val = readl(base_addr+SPI_BCC_REG);

	reg_val |= SPI_BCC_QUAD_MODE;
	writel(reg_val, base_addr + SPI_BCC_REG);
}

static int spi_regulator_request(struct sunxi_spi_platform_data *pdata,
			struct device *dev)
{
	struct regulator *regu = NULL;

	if (pdata->regulator != NULL)
		return 0;

#ifdef CONFIG_SUNXI_REGULATOR_DT
	regu = regulator_get(dev, "spi");
	if (IS_ERR(regu)) {
		SPI_ERR("%s: spi get supply failed!\n", __func__);
		return -1;
	}
#else
	/* Consider "n*" as nocare. Support "none", "nocare", "null", "" etc. */
	if ((pdata->regulator_id[0] == 'n') || (pdata->regulator_id[0] == 0))
		return 0;

	regu = regulator_get(NULL, pdata->regulator_id);
	if (IS_ERR(regu)) {
		SPI_ERR("get regulator %s failed!\n", pdata->regulator_id);
		return -1;
	}
#endif

	pdata->regulator = regu;
	return 0;
}

static void spi_regulator_release(struct sunxi_spi_platform_data *pdata)
{
	if (pdata->regulator == NULL)
		return;

	regulator_put(pdata->regulator);
	pdata->regulator = NULL;
}

static int spi_regulator_enable(struct sunxi_spi_platform_data *pdata)
{
	if (pdata->regulator == NULL)
		return 0;

	if (regulator_enable(pdata->regulator) != 0) {
		SPI_ERR("enable regulator %s failed!\n", pdata->regulator_id);
		return -1;
	}
	return 0;
}

static int spi_regulator_disable(struct sunxi_spi_platform_data *pdata)
{
	if (pdata->regulator == NULL)
		return 0;

	if (regulator_disable(pdata->regulator) != 0) {
		SPI_ERR("enable regulator %s failed!\n", pdata->regulator_id);
		return -1;
	}
	return 0;
}

#ifdef CONFIG_MTD_SPI_NOR
void dump_spinor_info(boot_spinor_info_t *spinor_info)
{
	pr_debug("\n"
		"----------------------\n"
		"magic:%s\n"
		"readcmd:%x\n"
		"read_mode:%d\n"
		"write_mode:%d\n"
		"flash_size:%dM\n"
		"addr4b_opcodes:%d\n"
		"erase_size:%d\n"
		"frequency:%d\n"
		"sample_mode:%x\n"
		"sample_delay:%x\n"
		"read_proto:%x\n"
		"write_proto:%x\n"
		"read_dummy:%d\n"
		"----------------------\n",
		spinor_info->magic, spinor_info->readcmd,
		spinor_info->read_mode, spinor_info->write_mode,
		spinor_info->flash_size, spinor_info->addr4b_opcodes,
		spinor_info->erase_size, spinor_info->frequency,
		spinor_info->sample_mode, spinor_info->sample_delay,
		spinor_info->read_proto, spinor_info->write_proto,
		spinor_info->read_dummy);
}

static int update_boot_param(struct mtd_info *mtd, struct sunxi_spi *sspi)
{
	struct spi_nor *nor = mtd->priv;
	u8 erase_opcode = nor->erase_opcode;
	uint32_t erasesize = mtd->erasesize;
	size_t retlen = 0;
	int ret;
	struct erase_info instr;
	boot_spinor_info_t *boot_info = NULL;
	struct sunxi_boot_param_region *boot_param = NULL;
	boot_param = kmalloc(BOOT_PARAM_SIZE, GFP_KERNEL);
	memset(boot_param, 0, BOOT_PARAM_SIZE);

	strncpy((char *)boot_param->header.magic,
			(const char *)BOOT_PARAM_MAGIC,
			sizeof(boot_param->header.magic));

	boot_param->header.check_sum = CHECK_SUM;

	boot_info = (boot_spinor_info_t *)boot_param->spiflash_info;

	strncpy((char *)boot_info->magic, (const char *)SPINOR_BOOT_PARAM_MAGIC,
			sizeof(boot_info->magic));
	boot_info->readcmd = nor->read_opcode;
	boot_info->flash_size = mtd->size / 1024 / 1024;
	boot_info->erase_size = mtd->erasesize;
	boot_info->read_proto = nor->read_proto;
	boot_info->write_proto = nor->write_proto;
	boot_info->read_dummy = nor->read_dummy;

	boot_info->frequency = sspi->spi->max_speed_hz;
	boot_info->sample_mode = sspi->sample_mode;
	boot_info->sample_delay = sspi->sample_delay;

	if (nor->read_proto == SNOR_PROTO_1_1_4)
		boot_info->read_mode = 4;
	else if (nor->read_proto == SNOR_PROTO_1_1_2)
		boot_info->read_mode = 2;
	else
		boot_info->read_mode = 1;

	if (nor->write_proto == SNOR_PROTO_1_1_4)
		boot_info->write_mode = 4;
	else if (nor->write_proto == SNOR_PROTO_1_1_2)
		boot_info->write_mode = 2;
	else
		boot_info->write_mode = 1;

	if (nor->flags & SNOR_F_4B_OPCODES)
		boot_info->addr4b_opcodes = 1;

	/*
	 * To not break boot0, switch bits 4K erasing
	 */
	if (nor->addr_width == 4)
		nor->erase_opcode = SPINOR_OP_BE_4K_4B;
	else
		nor->erase_opcode = SPINOR_OP_BE_4K;
	mtd->erasesize = 4096;

	instr.mtd = mtd;
	instr.addr = (CONFIG_SPINOR_UBOOT_OFFSET << 9) - BOOT_PARAM_SIZE;
	instr.len = BOOT_PARAM_SIZE;
	instr.callback = NULL;
	mtd->_erase(mtd, &instr);
	nor->erase_opcode = erase_opcode;
	mtd->erasesize = erasesize;

	ret = mtd->_write(mtd, (CONFIG_SPINOR_UBOOT_OFFSET << 9) - BOOT_PARAM_SIZE,
			BOOT_PARAM_SIZE, &retlen, (u_char *)boot_param);
	if (ret < 0)
		return -1;

	dump_spinor_info(boot_info);
	kfree(boot_param);
	return BOOT_PARAM_SIZE == retlen ? 0 : -1;
}

static void sunxi_spi_try_sample_param(struct sunxi_spi *sspi, struct mtd_info *mtd,
				boot_spinor_info_t *boot_info)
{
	unsigned int start_ok = 0, end_ok = 0, len_ok = 0, mode_ok = 0;
	unsigned int start_backup = 0, end_backup = 0, len_backup = 0;
	unsigned int mode = 0, startry_mode = 0, endtry_mode = 1;
	unsigned int sample_delay = 0;
	size_t retlen = 0, len = 512;
	void __iomem *base_addr = sspi->base_addr;
	boot0_file_head_t *boot0_head;
	boot0_head = kmalloc(len, GFP_KERNEL);

	spi_set_clk(sspi->spi->max_speed_hz, clk_get_rate(sspi->mclk), sspi);

	spi_samp_mode_enable(1, base_addr);
	spi_samp_dl_sw_status(1, base_addr);
	for (mode = startry_mode; mode <= endtry_mode; mode++) {
		sspi->sample_mode = mode;
		spi_set_sample_mode(mode, base_addr);
		for (sample_delay = 0; sample_delay < 64; sample_delay++) {
			sspi->sample_delay = sample_delay;
			spi_set_sample_delay(sample_delay, base_addr);
			memset(boot0_head, 0, len);
			mtd->_read(mtd, 0, len, &retlen, (u_char *)boot0_head);

			if (strncmp((char *)boot0_head->boot_head.magic,
				(char *)BOOT0_MAGIC,
				sizeof(boot0_head->boot_head.magic)) == 0) {
				dev_dbg(&sspi->pdev->dev, "mode:%d delay:%d [OK]\n",
						mode, sample_delay);
				if (!len_backup) {
					start_backup = sample_delay;
					end_backup = sample_delay;
				} else
					end_backup = sample_delay;
				len_backup++;
			} else {
				dev_dbg(&sspi->pdev->dev, "mode:%d delay:%d [ERROR]\n",
						mode, sample_delay);
				if (!start_backup)
					continue;
				else {
					if (len_backup > len_ok) {
						len_ok = len_backup;
						start_ok = start_backup;
						end_ok = end_backup;
						mode_ok = mode;
					}

					len_backup = 0;
					start_backup = 0;
					end_backup = 0;
				}
			}
		}
		if (len_backup > len_ok) {
			len_ok = len_backup;
			start_ok = start_backup;
			end_ok = end_backup;
			mode_ok = mode;
		}
		len_backup = 0;
		start_backup = 0;
		end_backup = 0;
	}

	if (!len_ok) {
		sspi->sample_delay = SAMP_MODE_DL_DEFAULT;
		sspi->sample_mode = SAMP_MODE_DL_DEFAULT;
		spi_samp_mode_enable(0, base_addr);
		spi_samp_dl_sw_status(0, base_addr);
		/* default clock */
		spi_set_clk(25000000, clk_get_rate(sspi->mclk), sspi);

		dev_err(&sspi->pdev->dev, "spif update delay param error\n");
	} else {
		sspi->sample_delay = (start_ok + end_ok) / 2;
		sspi->sample_mode = mode_ok;
		spi_set_sample_mode(sspi->sample_mode, base_addr);
		spi_set_sample_delay(sspi->sample_delay, base_addr);
	}
	dev_info(&sspi->pdev->dev,
			"Sample mode:%d start:%d end:%d right_sample_delay:0x%x\n",
			mode_ok, start_ok, end_ok, sspi->sample_delay);

	boot_info->sample_mode = sspi->sample_mode;
	boot_info->sample_delay = sspi->sample_delay;
	kfree(boot0_head);
	return;
}

void sunxi_spi_update_sample_delay_para(struct mtd_info *mtd, struct spi_device *spi)
{
	struct sunxi_spi *sspi = spi_master_get_devdata(spi->master);
	void __iomem *base_addr = sspi->base_addr;
	boot_spinor_info_t *boot_info = NULL;
	struct sunxi_boot_param_region *boot_param = NULL;

	if (!sspi) {
		dev_err(&sspi->pdev->dev, "spi controller is not initialized\n");
		return;
	}

	if (sspi->sample_mode == SAMP_MODE_DL_DEFAULT) {
#ifdef CONFIG_SUNXI_FASTBOOT
		int boot_param_addr = 0x42FFF000;
		boot_param = (struct sunxi_boot_param_region *)__va(boot_param_addr);
		boot_info = (boot_spinor_info_t *)boot_param->spiflash_info;
#else
		size_t retlen;
		spi_set_clk(25000000, clk_get_rate(sspi->mclk), sspi);

		boot_param = kmalloc(BOOT_PARAM_SIZE, GFP_KERNEL);
		mtd->_read(mtd, (CONFIG_SPINOR_UBOOT_OFFSET << 9) - BOOT_PARAM_SIZE,
			BOOT_PARAM_SIZE, &retlen, (u_char *)boot_param);
		boot_info = (boot_spinor_info_t *)boot_param->spiflash_info;
#endif

		if (strncmp((const char *)boot_param->header.magic,
					(const char *)BOOT_PARAM_MAGIC,
					sizeof(boot_param->header.magic)) ||
			strncmp((const char *)boot_info->magic,
					(const char *)SPINOR_BOOT_PARAM_MAGIC,
					sizeof(boot_info->magic))) {
			dev_err(&sspi->pdev->dev, "boot param magic abnormity go ot retey\n");
			sunxi_spi_try_sample_param(sspi, mtd, boot_info);
			if (update_boot_param(mtd, sspi))
				dev_err(&sspi->pdev->dev, "update boot param error\n");
		}

		if (boot_info->sample_delay == SAMP_MODE_DL_DEFAULT) {
			dev_err(&sspi->pdev->dev, "boot smple delay abnormity go ot retey\n");
			sunxi_spi_try_sample_param(sspi, mtd, boot_info);
			if (update_boot_param(mtd, sspi))
				dev_err(&sspi->pdev->dev, "update boot param error\n");
		} else {
			sspi->sample_mode = boot_info->sample_mode;
			sspi->sample_delay = boot_info->sample_delay;

			spi_set_clk(sspi->spi->max_speed_hz, clk_get_rate(sspi->mclk), sspi);
			spi_samp_mode_enable(1, base_addr);
			spi_samp_dl_sw_status(1, base_addr);
			spi_set_sample_mode(sspi->sample_mode, base_addr);
			spi_set_sample_delay(sspi->sample_delay, base_addr);
			dev_info(&sspi->pdev->dev, "Read boot param[mode:%x delay:%x]\n",
					sspi->sample_mode, sspi->sample_delay);
		}
#ifdef CONFIG_SUNXI_FASTBOOT
		memblock_free(boot_param_addr, BOOT_PARAM_SIZE);
		free_reserved_area(__va(boot_param_addr),
				__va(boot_param_addr + BOOT_PARAM_SIZE), -1, "boot_param");
#else
		kfree(boot_param);
#endif
	}

	return;
}
EXPORT_SYMBOL_GPL(sunxi_spi_update_sample_delay_para);
#endif

#ifdef CONFIG_DMA_ENGINE

/* ------------------------------- dma operation start----------------------- */
/* dma full done callback for spi rx */
static void sunxi_spi_dma_cb_rx(void *data)
{
	struct sunxi_spi *sspi = (struct sunxi_spi *)data;
	unsigned long flags = 0;
	void __iomem *base_addr = sspi->base_addr;

	spin_lock_irqsave(&sspi->lock, flags);
	dprintk(DEBUG_INFO, "[spi%d] dma read data end\n", sspi->master->bus_num);

	if (spi_query_rxfifo(base_addr) > 0) {
		SPI_ERR("[spi%d] DMA end, but RxFIFO isn't empty! FSR: %#x\n",
			sspi->master->bus_num, spi_query_rxfifo(base_addr));
		sspi->result = -1; /* failed */
	} else {
		sspi->result = 0;
	}

	complete(&sspi->done);
	spin_unlock_irqrestore(&sspi->lock, flags);
}

/* dma full done callback for spi tx */
static void sunxi_spi_dma_cb_tx(void *data)
{
	struct sunxi_spi *sspi = (struct sunxi_spi *)data;
	unsigned long flags = 0;

	spin_lock_irqsave(&sspi->lock, flags);
	dprintk(DEBUG_INFO, "[spi%d] dma write data end\n", sspi->master->bus_num);
	spin_unlock_irqrestore(&sspi->lock, flags);
}

/* request dma channel and set callback function */
static int sunxi_spi_prepare_dma(struct dma_chan **chan, enum spi_dma_dir _dir)
{
	dma_cap_mask_t mask;

	dprintk(DEBUG_INFO, "Init DMA, dir %d\n", _dir);

	/* Try to acquire a generic DMA engine slave channel */
	dma_cap_zero(mask);
	dma_cap_set(DMA_SLAVE, mask);

	if (*chan == NULL) {
		*chan = dma_request_channel(mask, NULL, NULL);
		if (*chan == NULL) {
			SPI_ERR("Request DMA(dir %d) failed!\n", _dir);
			return -EINVAL;
		}
	}

	return 0;
}

static int sunxi_spi_config_dma_rx(struct sunxi_spi *sspi, struct spi_transfer *t)
{
	struct dma_slave_config dma_conf = {0};
	struct dma_async_tx_descriptor *dma_desc = NULL;
	unsigned int i, j;
	u8 buf[64], cnt = 0;

	if (debug_mask & DEBUG_INFO3) {
		dprintk(DEBUG_INIT, "t->len = %d\n", t->len);
		if (debug_mask & DEBUG_DATA) {
			for (i = 0; i < t->len; i += 16) {
				cnt = 0;
				cnt += sprintf(buf + cnt, "%03x: ", i);
				for (j = 0; ((i + j) < t->len) && (j < 16); j++)
					cnt += sprintf(buf + cnt, "%02x ",
							((unsigned char *)(t->rx_buf))[i+j]);
				pr_warn("%s\n", buf);
			}
		}
	}

	dma_conf.direction = DMA_DEV_TO_MEM;
	dma_conf.src_addr = sspi->base_addr_phy + SPI_RXDATA_REG;
	if (t->len%DMA_SLAVE_BUSWIDTH_4_BYTES) {
		dma_conf.src_addr_width = DMA_SLAVE_BUSWIDTH_1_BYTE;
		dma_conf.dst_addr_width = DMA_SLAVE_BUSWIDTH_1_BYTE;
	} else {
		dma_conf.src_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
		dma_conf.dst_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
	}
	dma_conf.src_maxburst = 4;
	dma_conf.dst_maxburst = 4;
	dma_conf.slave_id = sunxi_slave_id(DRQDST_SDRAM, SUNXI_SPI_DRQ_RX(sspi->master->bus_num));
	dmaengine_slave_config(sspi->master->dma_rx, &dma_conf);

	dma_desc = dmaengine_prep_slave_sg(sspi->master->dma_rx, t->rx_sg.sgl,
					   t->rx_sg.nents, DMA_FROM_DEVICE,
					   DMA_PREP_INTERRUPT|DMA_CTRL_ACK);
	if (!dma_desc) {
		SPI_ERR("[spi%d] dmaengine_prep_slave_sg() failed!\n",
				sspi->master->bus_num);
		return -1;
	}

	dma_desc->callback = sunxi_spi_dma_cb_rx;
	dma_desc->callback_param = (void *)sspi;
	dmaengine_submit(dma_desc);

	return 0;
}



static int sunxi_spi_config_dma_tx(struct sunxi_spi *sspi, struct spi_transfer *t)
{
	struct dma_slave_config dma_conf = {0};
	struct dma_async_tx_descriptor *dma_desc = NULL;
	unsigned int i, j;
	u8 buf[64], cnt = 0;

	if (debug_mask & DEBUG_INFO4) {
		dprintk(DEBUG_INIT, "t->len = %d\n", t->len);
		if (debug_mask & DEBUG_DATA) {
			for (i = 0; i < t->len; i += 16) {
				cnt = 0;
				cnt += sprintf(buf + cnt, "%03x: ", i);
				for (j = 0; ((i + j) < t->len) && (j < 16); j++)
					cnt += sprintf(buf + cnt, "%02x ",
							((unsigned char *)(t->tx_buf))[i+j]);
				pr_warn("%s\n", buf);
			}
		}
	}

	dma_conf.direction = DMA_MEM_TO_DEV;
	dma_conf.dst_addr = sspi->base_addr_phy + SPI_TXDATA_REG;
	if (t->len%DMA_SLAVE_BUSWIDTH_4_BYTES) {
		dma_conf.src_addr_width = DMA_SLAVE_BUSWIDTH_1_BYTE;
		dma_conf.dst_addr_width = DMA_SLAVE_BUSWIDTH_1_BYTE;
	} else {
		dma_conf.src_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
		dma_conf.dst_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
	}
	dma_conf.src_maxburst = 4;
	dma_conf.dst_maxburst = 4;
	dma_conf.slave_id = sunxi_slave_id(SUNXI_SPI_DRQ_TX(sspi->master->bus_num), DRQSRC_SDRAM);
	dmaengine_slave_config(sspi->master->dma_tx, &dma_conf);

	dma_desc = dmaengine_prep_slave_sg(sspi->master->dma_tx, t->tx_sg.sgl,
			t->tx_sg.nents, DMA_TO_DEVICE,
			DMA_PREP_INTERRUPT|DMA_CTRL_ACK);
	if (!dma_desc) {
		SPI_ERR("[spi%d] dmaengine_prep_slave_sg() failed!\n",
				sspi->master->bus_num);
		return -1;
	}

	dma_desc->callback = sunxi_spi_dma_cb_tx;
	dma_desc->callback_param = (void *)sspi;
	dmaengine_submit(dma_desc);
	return 0;
}

/* set dma start flag, if queue, it will auto restart to transfer next queue */
static int sunxi_spi_start_dma(struct dma_chan *chan)
{
	dma_async_issue_pending(chan);
	return 0;
}
#endif
/* ------------------------------dma operation end--------------------------- */

/* spi device on or off control */
static void sunxi_spi_cs_control(struct spi_device *spi, bool enable)
{
	struct sunxi_spi *sspi = spi_master_get_devdata(spi->master);

	spi_set_cs(spi->chip_select, sspi->base_addr);
	spi_ss_level(sspi->base_addr, enable);
}

/* change the properties of spi device with spi transfer.
 * every spi transfer must call this interface to update
 * the master to the excute transfer set clock frequecy
 * return:  >= 0 : succeed;    < 0: failed.
 */
static int sunxi_spi_xfer_setup(struct spi_device *spi, struct spi_transfer *t)
{
	/* get at the setup function, the properties of spi device */
	struct sunxi_spi *sspi = spi_master_get_devdata(spi->master);
	u32 spi_speed_hz;
	void __iomem *base_addr = sspi->base_addr;

	spi_speed_hz  = (t && t->speed_hz) ? t->speed_hz : spi->max_speed_hz;

	if (sspi->sample_delay == SAMP_MODE_DL_DEFAULT) {
		if (spi_speed_hz >= SPI_HIGH_FREQUENCY)
			spi_sample_delay(0, 1, 0, base_addr);
		else if (spi_speed_hz <= SPI_LOW_FREQUENCY)
			spi_sample_delay(1, 0, 0, base_addr);
		else
			spi_sample_delay(0, 0, 0, base_addr);
	} else {
		spi_samp_mode_enable(1, base_addr);
		spi_samp_dl_sw_status(1, base_addr);
		spi_set_sample_mode(sspi->sample_mode, base_addr);
		spi_set_sample_delay(sspi->sample_delay, base_addr);
	}
#ifdef CONFIG_EVB_PLATFORM
	spi_set_clk(spi_speed_hz, clk_get_rate(sspi->mclk), sspi);
#else
	spi_set_clk(spi_speed_hz, 24000000, sspi);
#endif

	spi_config_tc(spi->mode, sspi->base_addr);
	return 0;
}

static int sunxi_spi_mode_check(struct sunxi_spi *sspi, struct spi_device *spi, struct spi_transfer *t)
{
	unsigned long flags = 0;

	if (sspi->mode_type != MODE_TYPE_NULL)
		return -EINVAL;

	/* full duplex */
	spin_lock_irqsave(&sspi->lock, flags);
	if (t->tx_buf && t->rx_buf) {
		spi_set_all_burst_received(sspi->base_addr);
		spi_set_bc_tc_stc(t->len, 0, t->len, 0, sspi->base_addr);
		sspi->mode_type = SINGLE_FULL_DUPLEX_RX_TX;
		dprintk(DEBUG_INFO, "[spi%d] Single mode Full duplex tx & rx\n", sspi->master->bus_num);
	} /* half duplex transmit */
	else if (t->tx_buf) {
		if (t->tx_nbits == SPI_NBITS_QUAD) {
			spi_disable_dual(sspi->base_addr);
			spi_enable_quad(sspi->base_addr);
			spi_set_bc_tc_stc(t->len, 0, 0, 0, sspi->base_addr);
			sspi->mode_type = QUAD_HALF_DUPLEX_TX;
			dprintk(DEBUG_INFO, "[spi%d] Quad mode Half duplex tx\n", sspi->master->bus_num);
		} else if (t->tx_nbits == SPI_NBITS_DUAL) {
			spi_disable_quad(sspi->base_addr);
			spi_enable_dual(sspi->base_addr);
			spi_set_bc_tc_stc(t->len, 0, 0, 0, sspi->base_addr);
			sspi->mode_type = DUAL_HALF_DUPLEX_TX;
			dprintk(DEBUG_INFO, "[spi%d] Dual mode Half duplex tx\n", sspi->master->bus_num);
		} else {
			spi_disable_quad(sspi->base_addr);
			spi_disable_dual(sspi->base_addr);
			spi_set_bc_tc_stc(t->len, 0, t->len, 0, sspi->base_addr);
			sspi->mode_type = SINGLE_HALF_DUPLEX_TX;
			dprintk(DEBUG_INFO, "[spi%d] Single mode Half duplex tx\n", sspi->master->bus_num);
		}
	} /* half duplex receive */
	else if (t->rx_buf) {
		if (t->rx_nbits == SPI_NBITS_QUAD) {
			spi_disable_dual(sspi->base_addr);
			spi_enable_quad(sspi->base_addr);
			spi_set_bc_tc_stc(0, t->len, 0, 0, sspi->base_addr);
			sspi->mode_type = QUAD_HALF_DUPLEX_RX;
			dprintk(DEBUG_INFO, "[spi%d] Quad mode Half duplex rx\n", sspi->master->bus_num);
		} else if (t->rx_nbits == SPI_NBITS_DUAL) {
			spi_disable_quad(sspi->base_addr);
			spi_enable_dual(sspi->base_addr);
			spi_set_bc_tc_stc(0, t->len, 0, 0, sspi->base_addr);
			sspi->mode_type = DUAL_HALF_DUPLEX_RX;
			dprintk(DEBUG_INFO, "[spi%d] Dual mode Half duplex rx\n", sspi->master->bus_num);
		} else {
			spi_disable_quad(sspi->base_addr);
			spi_disable_dual(sspi->base_addr);
			spi_set_bc_tc_stc(0, t->len, 0, 0, sspi->base_addr);
			sspi->mode_type = SINGLE_HALF_DUPLEX_RX;
			dprintk(DEBUG_INFO, "[spi%d] Single mode Half duplex rx\n", sspi->master->bus_num);
		}
	}
	spin_unlock_irqrestore(&sspi->lock, flags);

	return 0;

}

static int sunxi_spi_cpu_readl(struct spi_device *spi, struct spi_transfer *t)
{
	struct sunxi_spi *sspi = spi_master_get_devdata(spi->master);
	void __iomem *base_addr = sspi->base_addr;
	unsigned rx_len = t->len;	/* number of bytes sent */
	unsigned char *rx_buf = (unsigned char *)t->rx_buf;
	unsigned int poll_time = 0x7ffffff;
	unsigned int i, j;
	u8 buf[64], cnt = 0;

	while (rx_len) {
	/* rxFIFO counter */
		if (spi_query_rxfifo(base_addr) && (--poll_time > 0)) {
			*rx_buf++ =  readb(base_addr + SPI_RXDATA_REG);
			--rx_len;
		}
	}
	if (poll_time <= 0) {
		SPI_ERR("[spi%d] cpu receive data time out!\n", sspi->master->bus_num);
		return -1;
	}

	if (debug_mask & DEBUG_INFO1) {
		dprintk(DEBUG_INIT, "t->len = %d\n", t->len);
		if (debug_mask & DEBUG_DATA) {
			for (i = 0; i < t->len; i += 16) {
				cnt = 0;
				cnt += sprintf(buf + cnt, "%03x: ", i);
				for (j = 0; ((i + j) < t->len) && (j < 16); j++)
					cnt += sprintf(buf + cnt, "%02x ",
							((unsigned char *)(t->rx_buf))[i+j]);
				pr_warn("%s\n", buf);
			}
		}
	}

	return 0;
}

static int sunxi_spi_cpu_writel(struct spi_device *spi, struct spi_transfer *t)
{
	struct sunxi_spi *sspi = spi_master_get_devdata(spi->master);
	void __iomem *base_addr = sspi->base_addr;
	unsigned long flags = 0;
#ifndef CONFIG_DMA_ENGINE
	unsigned char time;
#endif
	unsigned tx_len = t->len;	/* number of bytes receieved */
	unsigned char *tx_buf = (unsigned char *)t->tx_buf;
	unsigned int poll_time = 0x7ffffff;
	unsigned int i, j;
	u8 buf[64], cnt = 0;

	if (debug_mask & DEBUG_INFO2) {
		dprintk(DEBUG_INIT, "t->len = %d\n", t->len);
		if (debug_mask & DEBUG_DATA) {
			for (i = 0; i < t->len; i += 16) {
				cnt = 0;
				cnt += sprintf(buf + cnt, "%03x: ", i);
				for (j = 0; ((i + j) < t->len) && (j < 16); j++)
					cnt += sprintf(buf + cnt, "%02x ",
							((unsigned char *)(t->tx_buf))[i+j]);
				pr_warn("%s\n", buf);
			}
		}
	}

	spin_lock_irqsave(&sspi->lock, flags);
	for (; tx_len > 0; --tx_len) {
		writeb(*tx_buf++, base_addr + SPI_TXDATA_REG);
#ifndef CONFIG_DMA_ENGINE
		if (spi_query_txfifo(base_addr) >= MAX_FIFU)
			for (time = 2; 0 < time; --time)
				;
#endif
	}
	spin_unlock_irqrestore(&sspi->lock, flags);
	while (spi_query_txfifo(base_addr) && (--poll_time > 0))
		;
	if (poll_time <= 0) {
		SPI_ERR("[spi%d] cpu transfer data time out!\n", sspi->master->bus_num);
		return -1;
	}

	return 0;
}

#ifdef CONFIG_DMA_ENGINE
static int sunxi_spi_dma_rx_config(struct spi_device *spi, struct spi_transfer *t)
{
	struct sunxi_spi *sspi = spi_master_get_devdata(spi->master);
	void __iomem *base_addr = sspi->base_addr;
	int ret = 0;

	/* rxFIFO reday dma request enable */
	spi_enable_dma_irq(SPI_FIFO_CTL_RX_DRQEN, base_addr);
	ret = sunxi_spi_prepare_dma(&sspi->master->dma_rx, SPI_DMA_RDEV);
	if (ret < 0) {
		spi_disable_dma_irq(SPI_FIFO_CTL_RX_DRQEN, base_addr);
		spi_disable_irq(SPI_INTEN_TC|SPI_INTEN_ERR, base_addr);
		SPI_ERR("[spi%d] dma rx fail\n", sspi->master->bus_num);
		return -EINVAL;
	}
	sunxi_spi_config_dma_rx(sspi, t);
	sunxi_spi_start_dma(sspi->master->dma_rx);

	return ret;
}

static int sunxi_spi_dma_tx_config(struct spi_device *spi, struct spi_transfer *t)
{
	struct sunxi_spi *sspi = spi_master_get_devdata(spi->master);
	void __iomem *base_addr = sspi->base_addr;
	int ret = 0;

	spi_enable_dma_irq(SPI_FIFO_CTL_TX_DRQEN, base_addr);

	ret = sunxi_spi_prepare_dma(&sspi->master->dma_tx, SPI_DMA_WDEV);
	if (ret < 0) {
		spi_disable_dma_irq(SPI_FIFO_CTL_TX_DRQEN, base_addr);
		spi_disable_irq(SPI_INTEN_TC|SPI_INTEN_ERR, base_addr);
		SPI_ERR("[spi%d] dma tx fail\n", sspi->master->bus_num);
		return -EINVAL;
	}
	sunxi_spi_config_dma_tx(sspi, t);
	sunxi_spi_start_dma(sspi->master->dma_tx);

	return ret;
}
#endif

static int sunxi_spi_xfer(struct spi_device *spi, struct spi_transfer *t)
{
	struct sunxi_spi *sspi = spi_master_get_devdata(spi->master);
	void __iomem *base_addr = sspi->base_addr;
	unsigned tx_len = t->len;	/* number of bytes receieved */
	unsigned rx_len = t->len;	/* number of bytes sent */


	switch (sspi->mode_type) {
	case SINGLE_HALF_DUPLEX_RX:
	case DUAL_HALF_DUPLEX_RX:
	case QUAD_HALF_DUPLEX_RX:
	{
#ifdef CONFIG_DMA_ENGINE
		/* >64 use DMA transfer, or use cpu */
		if (t->len > BULK_DATA_BOUNDARY) {
			dprintk(DEBUG_INFO, "[spi%d] rx -> by dma\n", sspi->master->bus_num);
			/* For Rx mode, the DMA end(not TC flag) is real end. */
			spi_disable_irq(SPI_INTEN_TC, base_addr);
			sunxi_spi_dma_rx_config(spi, t);
			if (!sspi->dbi_enabled)
				spi_start_xfer(base_addr);
		} else {
#else
		{
#endif
			dprintk(DEBUG_INFO, "[spi%d] rx -> by ahb\n", sspi->master->bus_num);
			/* SMC=1,XCH trigger the transfer */
			if (!sspi->dbi_enabled)
				spi_start_xfer(base_addr);
			sunxi_spi_cpu_readl(spi, t);
		}
		break;
	}
	case SINGLE_HALF_DUPLEX_TX:
	case DUAL_HALF_DUPLEX_TX:
	case QUAD_HALF_DUPLEX_TX:
	{
#ifdef CONFIG_DMA_ENGINE
		/* >64 use DMA transfer, or use cpu */
		if (t->len > BULK_DATA_BOUNDARY) {
			dprintk(DEBUG_INFO, "[spi%d] tx -> by dma\n", sspi->master->bus_num);
			if (!sspi->dbi_enabled)
				spi_start_xfer(base_addr);
			/* txFIFO empty dma request enable */
			sunxi_spi_dma_tx_config(spi, t);
		} else {
#else
		{
#endif
			dprintk(DEBUG_INFO, "[spi%d] tx -> by ahb\n", sspi->master->bus_num);

			if (!sspi->dbi_enabled)
				spi_start_xfer(base_addr);
			sunxi_spi_cpu_writel(spi, t);
		}
		break;
	}
	case SINGLE_FULL_DUPLEX_RX_TX:
	{
#ifdef CONFIG_DMA_ENGINE
		/* >64 use DMA transfer, or use cpu */
		if (t->len > BULK_DATA_BOUNDARY) {
			dprintk(DEBUG_INFO, "[spi%d] rx and tx -> by dma\n", sspi->master->bus_num);
			/* For Rx mode, the DMA end(not TC flag) is real end. */
			spi_disable_irq(SPI_INTEN_TC, base_addr);
			sunxi_spi_dma_rx_config(spi, t);
			if (!sspi->dbi_enabled)
				spi_start_xfer(base_addr);
			sunxi_spi_dma_tx_config(spi, t);
		} else {
#else
		{
#endif
			dprintk(DEBUG_INFO, "[spi%d] rx and tx -> by ahb\n", sspi->master->bus_num);
			if ((rx_len == 0) || (tx_len == 0))
				return -EINVAL;

			if (!sspi->dbi_enabled)
				spi_start_xfer(base_addr);
			sunxi_spi_cpu_writel(spi, t);
			sunxi_spi_cpu_readl(spi, t);
		}
		break;
	}
	default:
		return -1;
	}

	return 0;
}

/*
 * <= 64 : cpu ;  > 64 : dma
 * wait for done completion in this function, wakup in the irq hanlder
 */
static int sunxi_spi_transfer_one(struct spi_master *master, struct spi_device *spi,
			    struct spi_transfer *t)
{
	struct sunxi_spi *sspi = spi_master_get_devdata(spi->master);
	void __iomem *base_addr = sspi->base_addr;
	unsigned char *tx_buf = (unsigned char *)t->tx_buf;
	unsigned char *rx_buf = (unsigned char *)t->rx_buf;
	unsigned long timeout = 0;
	int ret = 0;
	static int xfer_setup;

	dprintk(DEBUG_INFO, "[spi%d] begin transfer, txbuf %p, rxbuf %p, len %d\n",
		spi->master->bus_num, tx_buf, rx_buf, t->len);
	if ((!t->tx_buf && !t->rx_buf) || !t->len)
		return -EINVAL;

	if (!xfer_setup || spi->master->bus_num) {
		if (sunxi_spi_xfer_setup(spi, t) < 0)
			return -EINVAL;
		xfer_setup = 1;
	}

	/* write 1 to clear 0 */
	spi_clr_irq_pending(SPI_INT_STA_MASK, base_addr);
#ifdef CONFIG_DMA_ENGINE
	/* disable all DRQ */
	spi_disable_dma_irq(SPI_FIFO_CTL_DRQEN_MASK, base_addr);
#endif

	if (sunxi_spi_mode_check(sspi, spi, t))
		return -EINVAL;

	if (sspi->dbi_enabled) {
		spi_config_dbi(spi, sspi->base_addr);
#ifdef CONFIG_DMA_ENGINE
		spi_enable_dbi_dma(base_addr);
#endif
	} else {
		spi_reset_fifo(base_addr);
		spi_enable_irq(SPI_INTEN_TC|SPI_INTEN_ERR, base_addr);
	}

	sunxi_spi_xfer(spi, t);

	if ((debug_mask & DEBUG_INIT) && (debug_mask & DEBUG_DATA)) {
		dprintk(DEBUG_DATA, "[spi%d] dump reg:\n", sspi->master->bus_num);
		spi_dump_reg(sspi, 0, 0x40);
	}

	/* wait for xfer complete in the isr. */
	timeout = wait_for_completion_timeout(
				&sspi->done,
				msecs_to_jiffies(XFER_TIMEOUT));
	if (timeout == 0) {
		SPI_ERR("[spi%d] xfer timeout\n", spi->master->bus_num);
		ret = -1;
	} else if (sspi->result < 0) {
		SPI_ERR("[spi%d] xfer failed...\n", spi->master->bus_num);
		ret = -1;
	}

#ifdef CONFIG_DMA_ENGINE
	if (sspi->dbi_enabled)
		spi_disable_dbi_dma(base_addr);
#endif

	if (sspi->mode_type != MODE_TYPE_NULL)
		sspi->mode_type = MODE_TYPE_NULL;

	return ret;
}

#ifdef CONFIG_AW_MTD_SPINAND

/* tx_len : all data to transfer(single io tx data + quad io tx data)
 * stc_len: single io tx data*/
static void spi_set_bc_tc_stc2(u32 tx_len, u32 rx_len, u32 stc_len, u8 nbits, void __iomem *base_addr)
{
	u32 reg_val = readl(base_addr + SPI_BURST_CNT_REG);
	reg_val &= ~SPI_BC_CNT_MASK;
	reg_val |= (SPI_BC_CNT_MASK & (tx_len + rx_len));
	writel(reg_val, base_addr + SPI_BURST_CNT_REG);
	//SPI_DBG("\n-- BC = %d --\n", readl(base_addr + SPI_BURST_CNT_REG));

	reg_val = readl(base_addr + SPI_TRANSMIT_CNT_REG);
	reg_val &= ~SPI_TC_CNT_MASK;
	reg_val |= (SPI_TC_CNT_MASK & tx_len);
	writel(reg_val, base_addr + SPI_TRANSMIT_CNT_REG);
	//SPI_DBG("\n-- TC = %d --\n", readl(base_addr + SPI_TRANSMIT_CNT_REG));

	reg_val = readl(base_addr + SPI_BCC_REG);
	reg_val &= ~SPI_BCC_STC_MASK;
	reg_val |= (SPI_BCC_STC_MASK & stc_len);
	if (nbits == 2)
		reg_val |= SPI_BCC_DUAL_MODE;
	else
		reg_val &= ~SPI_BCC_DUAL_MODE;

	if (nbits == 4)
		reg_val |= SPI_BCC_QUAD_MODE;
	else
		reg_val &= ~SPI_BCC_QUAD_MODE;

	writel(reg_val, base_addr + SPI_BCC_REG);
	//SPI_DBG("\n-- STC = %d --\n", readl(base_addr + SPI_BCC_REG));
}

static int sunxi_spi_xfer_tx_rx(struct spi_device *spi, struct spi_transfer *tx, struct spi_transfer *rx)
{
#define SPI_FIFO_SIZE (64)
	struct sunxi_spi *sspi = spi_master_get_devdata(spi->master);
	void __iomem *base_addr = sspi->base_addr;
	unsigned int xcnt = 0;
	unsigned int poll_time = 0;
	int ret = 0;

	unsigned tcnt = tx->len;
	unsigned rcnt = rx->len;

	char *txbuf = (char *)tx->tx_buf;
	char *rxbuf = (char *)rx->rx_buf;

	u8 nbits = 0;

	if (rx->rx_nbits == SPI_NBITS_DUAL)
		nbits = 2;
	else if (rx->rx_nbits == SPI_NBITS_QUAD)
		nbits = 4;
	else
		nbits = 1;


	spi_disable_irq(SPI_INTEN_MASK, base_addr);

	/* write 1 to clear 0 */
	spi_clr_irq_pending(SPI_INT_STA_MASK, base_addr);

	spi_set_bc_tc_stc2(tcnt, rcnt, tcnt, nbits, base_addr);

	/*
	* 1. Tx/Rx error irq,process in IRQ;
	* 2. Transfer Complete Interrupt Enable
	*/
	spi_enable_irq(SPI_INTEN_TC|SPI_INTEN_ERR, base_addr);

	spi_start_xfer(base_addr);

	if (tcnt) {

		/* >64 use DMA transfer, or use cpu */
		if (tcnt <= BULK_DATA_BOUNDARY) {
			xcnt = 0;
			poll_time = 0xfffff;
			dprintk(DEBUG_DATA, "[spi-%d]xfer2 tx --> by ahb\n", spi->master->bus_num);
			while (xcnt < tcnt) {
				while (((readl(base_addr + SPI_FIFO_STA_REG) >> 16) & 0x7f) >= SPI_FIFO_SIZE) {
					if (--poll_time < 0)
						return -ETIMEDOUT;
				}
				writeb(*(txbuf + xcnt), (base_addr + SPI_TXDATA_REG));
				xcnt++;
			}

		} else {
			dprintk(DEBUG_DATA, "[spi-%d]xfer2 tx -> by dma\n", spi->master->bus_num);
			/* txFIFO empty dma request enable */
			sunxi_spi_dma_tx_config(spi, tx);

		}
	}

	if (rcnt) {
		if (rcnt <= BULK_DATA_BOUNDARY) {
			xcnt = 0;
			poll_time = 0xfffff;
			dprintk(DEBUG_DATA, "[spi-%d]xfer2 rx --> by ahb\n", spi->master->bus_num);
			while (xcnt < rcnt) {
				if (((readl(base_addr + SPI_FIFO_STA_REG)) & 0x7f) && (--poll_time > 0)) {
					*(rxbuf + xcnt) = readb((base_addr + SPI_RXDATA_REG));
					xcnt++;
				}
			}
			if (poll_time <= 0) {
				SPI_ERR("cpu receive data timeout!\n");
				return -ETIMEDOUT;
			}
		} else {
			dprintk(DEBUG_INFO, "[spi-%d]xfer2 rx -> by dma\n", spi->master->bus_num);

			/* For Rx mode, the DMA end(not TC flag) is real end. */
			spi_disable_irq(SPI_INTEN_TC, base_addr);
			sunxi_spi_dma_rx_config(spi, rx);
		}
	}

	return ret;
}

/*
 * <= 64 : cpu ;  > 64 : dma
 * wait for done completion in this function, wakup in the irq hanlder
 */
static int sunxi_spi_transfer_two(struct spi_master *master, struct spi_device *spi,
			    struct spi_transfer *t, struct spi_transfer *t2)
{
	struct sunxi_spi *sspi = spi_master_get_devdata(spi->master);
	unsigned long timeout = 0;
	int ret = 0;
	static int xfer_setup;

	if (!xfer_setup || spi->master->bus_num) {
		if (sunxi_spi_xfer_setup(spi, t) < 0)
			return -EINVAL;
		xfer_setup = 1;
	}

	if (t->tx_buf && t2->rx_buf)
		sunxi_spi_xfer_tx_rx(spi, t, t2);
	else {

		SPI_ERR("[spi%d] begin transfer, t1: txbuf %p, rxbuf %p, len %d t2:  txbuf %p, rxbuf %p, len %d t2:\n",
		spi->master->bus_num, t->tx_buf, t->rx_buf, t->len, t2->tx_buf, t2->rx_buf, t2->len);
		SPI_ERR("[spi%d] xfer mode not support\n", spi->master->bus_num);
		return -EINVAL;
	}


	if ((debug_mask & DEBUG_INIT) && (debug_mask & DEBUG_DATA)) {
		dprintk(DEBUG_DATA, "[spi%d] dump reg:\n", sspi->master->bus_num);
		spi_dump_reg(sspi, 0, 0x40);
	}

	/* wait for xfer complete in the isr. */
	timeout = wait_for_completion_timeout(
				&sspi->done,
				msecs_to_jiffies(XFER_TIMEOUT));
	if (timeout == 0) {
		SPI_ERR("[spi%d] xfer timeout\n", spi->master->bus_num);
		ret = -1;
	} else if (sspi->result < 0) {
		SPI_ERR("[spi%d] xfer failed...\n", spi->master->bus_num);
		ret = -1;
	}

	if (sspi->mode_type != MODE_TYPE_NULL)
		sspi->mode_type = MODE_TYPE_NULL;

	/*
	 *if (sspi->stx.state == TRANSFER_STATE) {
	 *        sunxi_spi_unmap_sg(spi->master, &sspi->stx.tx);
	 *        sspi->stx.state = INIT_STATE;
	 *}
	 */

	return ret;
}

/*
 * sunxi spi flash_transfer_one_message - Default implementation of transfer_one_message()
 *
 * This is a standard implementation of transfer_one_message() for
 * drivers which implement a transfer_one() operation.  It provides
 * standard handling of delays and chip select management.
 */
static int sunxi_spi_transfer_one_message(struct spi_master *master,
				    struct spi_message *msg)
{
	struct sunxi_spi *sspi = spi_master_get_devdata(master);
	void __iomem *base_addr = sspi->base_addr;
	struct spi_transfer *xfer;
	struct spi_transfer *cur_xfer = NULL;
	struct spi_transfer *prev_xfer = NULL;
	int xfer_cnt = 0;
	int xfer_n = 0;
	int ret = 0;
	struct spi_statistics *statm = &master->statistics;
	struct spi_statistics *stats = &msg->spi->statistics;


	SPI_STATISTICS_INCREMENT_FIELD(statm, messages);
	SPI_STATISTICS_INCREMENT_FIELD(stats, messages);


	list_for_each_entry(xfer, &msg->transfers, transfer_list) {

		spi_statistics_add_transfer_stats(statm, xfer, master);
		spi_statistics_add_transfer_stats(stats, xfer, master);

		xfer_cnt++;
		cur_xfer = xfer;

		if (prev_xfer && prev_xfer->tx_buf && !prev_xfer->rx_buf &&
				cur_xfer && cur_xfer->rx_buf && !cur_xfer->tx_buf) {
			/*tx->rx*/
			reinit_completion(&master->xfer_completion);

			ret = master->transfer_two(master, msg->spi, prev_xfer, cur_xfer);
			if (ret < 0) {
				SPI_STATISTICS_INCREMENT_FIELD(statm,
							       errors);
				SPI_STATISTICS_INCREMENT_FIELD(stats,
							       errors);
				SPI_ERR("[spi%d]  SPI transfer failed: %d\n", sspi->master->bus_num, ret);
				goto out;
			}

			if (msg->status != -EINPROGRESS)
				goto out;

			msg->actual_length += prev_xfer->len;
			msg->actual_length += cur_xfer->len;
			prev_xfer = NULL;
			cur_xfer = NULL;
			xfer_cnt = 0;

		} else if (!prev_xfer) {
			prev_xfer = xfer;

		} else {
			/*single handle*/

			spi_ss_ctrl(base_addr, 1);
			master->set_cs(msg->spi, false);
			for (xfer_n = 0; xfer_n < xfer_cnt; xfer_n++) {
				if (xfer_n == 0)
					xfer = prev_xfer;
				else
					xfer = cur_xfer;

				reinit_completion(&master->xfer_completion);

				ret = master->transfer_one(master, msg->spi, xfer);
				if (ret < 0) {
					SPI_STATISTICS_INCREMENT_FIELD(statm,
							errors);
					SPI_STATISTICS_INCREMENT_FIELD(stats,
							errors);
					SPI_ERR("[spi%d]  SPI transfer failed: %d\n", sspi->master->bus_num, ret);
					goto out;
				}

				msg->actual_length += xfer->len;

			}

			master->set_cs(msg->spi, true);
			spi_ss_ctrl(base_addr, 0);
		}
	}

	/*do last xfer*/
	{
		if (prev_xfer && prev_xfer == cur_xfer) {

			reinit_completion(&master->xfer_completion);

			ret = master->transfer_one(master, msg->spi, cur_xfer);
			if (ret < 0) {
				SPI_STATISTICS_INCREMENT_FIELD(statm,
						errors);
				SPI_STATISTICS_INCREMENT_FIELD(stats,
						errors);
				SPI_ERR("[spi%d]  SPI transfer failed: %d\n", sspi->master->bus_num, ret);
				goto out;
			}

			msg->actual_length += cur_xfer->len;
			prev_xfer = NULL;
			cur_xfer = NULL;
			xfer_cnt = 0;
		}
	}

out:

	if (msg->status == -EINPROGRESS)
		msg->status = ret;

	if (msg->status && master->handle_err)
		master->handle_err(master, msg);

	spi_res_release(master, msg);

	spi_finalize_current_message(master);

	return ret;
}
#endif

/* wake up the sleep thread, and give the result code */
static irqreturn_t sunxi_spi_handler(int irq, void *dev_id)
{
	struct sunxi_spi *sspi = (struct sunxi_spi *)dev_id;
	void __iomem *base_addr = sspi->base_addr;
	unsigned int status = 0, enable = 0;
	unsigned long flags = 0;

	spin_lock_irqsave(&sspi->lock, flags);

	enable = spi_qry_irq_enable(base_addr);
	status = spi_qry_irq_pending(base_addr);
	spi_clr_irq_pending(status, base_addr);
	dprintk(DEBUG_INFO, "[spi%d] irq status = %x\n", sspi->master->bus_num, status);

	sspi->result = 0; /* assume succeed */

	if (sspi->mode) {
		if ((enable & SPI_INTEN_RX_RDY) && (status & SPI_INT_STA_RX_RDY)) {
			dprintk(DEBUG_INFO, "[spi%d] spi data is ready\n", sspi->master->bus_num);
			spi_disable_irq(SPI_INT_STA_RX_RDY, base_addr);
			wake_up_process(sspi->task);
			spin_unlock_irqrestore(&sspi->lock, flags);
			return IRQ_HANDLED;
		}
	}

	/* master mode, Transfer Complete Interrupt */
	if (status & SPI_INT_STA_TC) {
		dprintk(DEBUG_INFO, "[spi%d] SPI TC comes\n", sspi->master->bus_num);
		spi_disable_irq(SPI_INT_STA_TC | SPI_INT_STA_ERR, base_addr);

		/*wakup uplayer, by the sem */
		complete(&sspi->done);
		spin_unlock_irqrestore(&sspi->lock, flags);
		return IRQ_HANDLED;
	} else if (status & SPI_INT_STA_ERR) { /* master mode:err */
		SPI_ERR("[spi%d]  SPI ERR %#x comes\n", sspi->master->bus_num, status);
		/* error process, release dma in the workqueue,should not be here */
		spi_disable_irq(SPI_INT_STA_TC | SPI_INT_STA_ERR, base_addr);
		spi_soft_reset(base_addr);
		sspi->result = -1;
		complete(&sspi->done);
		spin_unlock_irqrestore(&sspi->lock, flags);
		return IRQ_HANDLED;
	}
	if (sspi->dbi_enabled) {
		status = dbi_qry_irq_pending(base_addr);
		dbi_clr_irq_pending(status, base_addr);
		dprintk(DEBUG_INFO, "[dbi%d] irq status = %x\n",
				sspi->master->bus_num, status);

		if ((status & DBI_INT_FIFO_EMPTY) && !(sspi->spi->dbi_mode & SPI_DBI_TRANSMIT_VIDEO_)) {
			dprintk(DEBUG_INFO, "[spi%d] DBI Fram TC comes\n",
					sspi->master->bus_num);
			dbi_disable_irq(DBI_FIFO_EMPTY_INT_EN, base_addr);
			/*wakup uplayer, by the sem */
			complete(&sspi->done);
			spin_unlock_irqrestore(&sspi->lock, flags);
			return IRQ_HANDLED;
		} else if (((status & DBI_INT_TE_INT) ||
			    (status & DBI_INT_STA_FRAME)) &&
			   (sspi->spi->dbi_mode & SPI_DBI_TRANSMIT_VIDEO_)) {
			if (sspi->spi->dbi_vsync_handle &&
			    (status & DBI_INT_TE_INT))
				sspi->spi->dbi_vsync_handle(
				    (unsigned long)sspi->spi);
			else
				dbi_disable_irq(DBI_FRAM_DONE_INT_EN, base_addr);
			complete(&sspi->done);
			spin_unlock_irqrestore(&sspi->lock, flags);
			return IRQ_HANDLED;
		} else {
			//TODO: Adapt to other states
			spin_unlock_irqrestore(&sspi->lock, flags);
			return IRQ_HANDLED;
		}
	}
	dprintk(DEBUG_INFO, "[spi%d] SPI NONE comes\n", sspi->master->bus_num);
	spin_unlock_irqrestore(&sspi->lock, flags);
	return IRQ_NONE;
}

static int sunxi_spi_setup(struct spi_device *spi)
{
	struct sunxi_spi *sspi = spi_master_get_devdata(spi->master);

	sspi->spi = spi;

	if (sunxi_spi_xfer_setup(spi, NULL) < 0)
		SPI_ERR("failed to xfer setup\n");

	return 0;
}

static bool sunxi_spi_can_dma(struct spi_master *master, struct spi_device *spi,
				 struct spi_transfer *xfer)
{
	return (xfer->len > BULK_DATA_BOUNDARY);
}

static struct device_data *sunxi_spi_slave_set_txdata(struct sunxi_spi_slave_head *head)
{
	u8 *buf, i;
	struct device_data *data;

	buf = kzalloc(head->len, GFP_KERNEL);
	if (IS_ERR_OR_NULL(buf)) {
		SPI_ERR("failed to alloc mem\n");
		goto err0;
	}

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (IS_ERR_OR_NULL(data)) {
		SPI_ERR("failed to alloc mem\n");
		goto err1;
	}

	for (i = 0; i < head->len; i++) {
		buf[i] = i + SAMPLE_NUMBER;
	}
	udelay(100);

	dprintk(DEBUG_DATA, "[debugging only] send data:\n");
	if (debug_mask & DEBUG_DATA)
		spi_dump_data(buf, head->len);

	data->tx_buf = buf;
	data->len = head->len;

	return data;

err1:
	kfree(buf);
err0:
	return NULL;
}

static int sunxi_spi_slave_cpu_tx_config(struct sunxi_spi *sspi)
{
	int ret = 0, i;
	u32 poll_time = 0x7ffffff;
	unsigned long timeout = 0;
	unsigned long flags = 0;

	dprintk(DEBUG_INFO, "[spi%d] receive pkt head ok\n", sspi->master->bus_num);
	if (sspi->slave->set_up_txdata) {
		sspi->slave->data = sspi->slave->set_up_txdata(sspi->slave->head);
		if (IS_ERR_OR_NULL(sspi->slave->data)) {
			SPI_ERR("[spi%d] null data\n", sspi->master->bus_num);
			ret = -1;
			goto err;
		}
	} else {
		SPI_ERR("[spi%d] none define set_up_txdata\n", sspi->master->bus_num);
		ret = -1;
		goto err;
	}

	sspi->done.done = 0;
	spi_clr_irq_pending(SPI_INT_STA_MASK, sspi->base_addr);
	spi_disable_irq(SPI_INTEN_RX_RDY, sspi->base_addr);
	spi_reset_fifo(sspi->base_addr);
	spi_set_bc_tc_stc(0, 0, 0, 0, sspi->base_addr);
	spi_enable_irq(SPI_INTEN_TC|SPI_INTEN_ERR, sspi->base_addr);

	dprintk(DEBUG_INFO, "[spi%d] to be send data init ok\n", sspi->master->bus_num);
	spin_lock_irqsave(&sspi->lock, flags);
	for (i = 0; i < sspi->slave->head->len; i++) {
		while ((spi_query_txfifo(sspi->base_addr) >= MAX_FIFU) && (--poll_time))
			;
		if (poll_time == 0) {
			dprintk(DEBUG_INFO, "[spi%d]cpu send data timeout\n", sspi->master->bus_num);
			goto err;
		}

		writeb(sspi->slave->data->tx_buf[i], sspi->base_addr + SPI_TXDATA_REG);
	}
	spin_unlock_irqrestore(&sspi->lock, flags);

	dprintk(DEBUG_INFO, "[spi%d] already send data to fifo\n", sspi->master->bus_num);

	/* wait for xfer complete in the isr. */
	timeout = wait_for_completion_timeout(
				&sspi->done,
				msecs_to_jiffies(XFER_TIMEOUT));
	if (timeout == 0) {
		SPI_ERR("[spi%d] xfer timeout\n", sspi->master->bus_num);
		ret = -1;
		goto err;
	} else if (sspi->result < 0) {
		SPI_ERR("[spi%d] xfer failed...\n", sspi->master->bus_num);
		ret = -1;
		goto err;
	}

err:
	spi_clr_irq_pending(SPI_INT_STA_MASK, sspi->base_addr);
	spi_disable_irq(SPI_INTEN_TC|SPI_INTEN_ERR, sspi->base_addr);
#ifdef CONFIG_DMA_ENGINE
	spi_disable_dma_irq(SPI_FIFO_CTL_DRQEN_MASK, sspi->base_addr);
#endif
	spi_reset_fifo(sspi->base_addr);
	kfree(sspi->slave->data->tx_buf);
	kfree(sspi->slave->data);

	return ret;
}

static int sunxi_spi_slave_cpu_rx_config(struct sunxi_spi *sspi)
{
	int ret = 0, i;
	u32 poll_time = 0x7ffffff;

	dprintk(DEBUG_INFO, "[spi%d] receive pkt head ok\n", sspi->master->bus_num);
	sspi->slave->data = kzalloc(sizeof(struct device_data), GFP_KERNEL);
	if (IS_ERR_OR_NULL(sspi->slave->data)) {
		SPI_ERR("failed to alloc mem\n");
		ret = -ENOMEM;
		goto err0;
	}

	sspi->slave->data->len = sspi->slave->head->len;
	sspi->slave->data->rx_buf = kzalloc(sspi->slave->data->len, GFP_KERNEL);
	if (IS_ERR_OR_NULL(sspi->slave->data->rx_buf)) {
		SPI_ERR("failed to alloc mem\n");
		ret = -ENOMEM;
		goto err1;
	}

	sspi->done.done = 0;

	spi_set_rx_trig(sspi->slave->data->len/2, sspi->base_addr);
	spi_enable_irq(SPI_INTEN_ERR|SPI_INTEN_RX_RDY, sspi->base_addr);
	spi_set_bc_tc_stc(0, 0, 0, 0, sspi->base_addr);

	dprintk(DEBUG_INFO, "[spi%d] to be receive data init ok\n", sspi->master->bus_num);
	for (i = 0; i < sspi->slave->data->len; i++) {
		while (!spi_query_rxfifo(sspi->base_addr) && (--poll_time > 0))
			;
		sspi->slave->data->rx_buf[i] =  readb(sspi->base_addr + SPI_RXDATA_REG);
	}


	if (poll_time <= 0) {
		SPI_ERR("[spi%d] cpu receive pkt head time out!\n", sspi->master->bus_num);
		spi_reset_fifo(sspi->base_addr);
		ret = -1;
		goto err2;
	} else if (sspi->result < 0) {
		SPI_ERR("[spi%d] xfer failed...\n", sspi->master->bus_num);
		spi_reset_fifo(sspi->base_addr);
		ret = -1;
		goto err2;
	}

	dprintk(DEBUG_DATA, "[debugging only] receive data:\n");
	if (debug_mask & DEBUG_DATA)
		spi_dump_data(sspi->slave->data->rx_buf, sspi->slave->data->len);

err2:
	spi_clr_irq_pending(SPI_INT_STA_MASK, sspi->base_addr);
	spi_disable_irq(SPI_INTEN_TC|SPI_INTEN_ERR, sspi->base_addr);
#ifdef CONFIG_DMA_ENGINE
	spi_disable_dma_irq(SPI_FIFO_CTL_DRQEN_MASK, sspi->base_addr);
#endif
	spi_reset_fifo(sspi->base_addr);
	kfree(sspi->slave->data->rx_buf);
err1:
	kfree(sspi->slave->data);
err0:
	return ret;
}
static int sunxi_spi_slave_handle_head(struct sunxi_spi *sspi, u8 *buf)
{
	struct sunxi_spi_slave_head *head;
	int ret = 0;

	head = kzalloc(sizeof(*head), GFP_KERNEL);
	if (IS_ERR_OR_NULL(head)) {
		SPI_ERR("failed to alloc mem\n");
		ret = -ENOMEM;
		goto err0;
	}

	head->op_code = buf[OP_MASK];
	head->addr = (buf[ADDR_MASK_0] << 16) | (buf[ADDR_MASK_1] << 8) | buf[ADDR_MASK_2];
	head->len = buf[LENGTH_MASK];

	dprintk(DEBUG_INFO, "[spi%d] op=0x%x addr=0x%x len=0x%x\n",
			sspi->master->bus_num, head->op_code, head->addr, head->len);

	sspi->slave->head = head;

	if (head->len > 64) {
		dprintk(DEBUG_INFO, "[spi%d] length must less than 64 bytes\n", sspi->master->bus_num);
		ret = -1;
		goto err1;
	}

	if (head->op_code == SUNXI_OP_WRITE) {
		sunxi_spi_slave_cpu_rx_config(sspi);
	} else if (head->op_code == SUNXI_OP_READ) {
		sunxi_spi_slave_cpu_tx_config(sspi);
	} else {
		dprintk(DEBUG_INFO, "[spi%d] pkt head opcode err\n", sspi->master->bus_num);
		ret = -1;
		goto err1;
	}
err1:
	kfree(head);
err0:
	spi_clr_irq_pending(SPI_INT_STA_MASK, sspi->base_addr);
	spi_disable_irq(SPI_INTEN_RX_RDY, sspi->base_addr);
	spi_disable_irq(SPI_INTEN_TC|SPI_INTEN_ERR, sspi->base_addr);
	spi_reset_fifo(sspi->base_addr);

	return ret;
}

static int sunxi_spi_slave_task(void *data)
{
	u8 *pkt_head, i;
	u32 poll_time = 0x7ffffff;
	unsigned long flags = 0;
	struct sunxi_spi *sspi = (struct sunxi_spi *)data;

	pkt_head = kzalloc(HEAD_LEN, GFP_KERNEL);
	if (IS_ERR_OR_NULL(pkt_head)) {
		SPI_ERR("[spi%d] failed to alloc mem\n", sspi->master->bus_num);
		return -ENOMEM;
	}

	allow_signal(SIGKILL);

	while (!kthread_should_stop()) {
		spi_reset_fifo(sspi->base_addr);
		spi_clr_irq_pending(SPI_INT_STA_MASK, sspi->base_addr);
#ifdef CONFIG_DMA_ENGINE
		spi_disable_dma_irq(SPI_FIFO_CTL_DRQEN_MASK, sspi->base_addr);
#endif
		spi_enable_irq(SPI_INTEN_ERR|SPI_INTEN_RX_RDY, sspi->base_addr);
		spi_set_rx_trig(HEAD_LEN, sspi->base_addr);
		spi_set_bc_tc_stc(0, 0, 0, 0, sspi->base_addr);

		dprintk(DEBUG_INFO, "[spi%d] receive pkt head init ok, sleep and wait for data\n", sspi->master->bus_num);
		set_current_state(TASK_INTERRUPTIBLE);
		schedule();

		dprintk(DEBUG_INFO, "[spi%d] data is come, wake up and receive pkt head\n", sspi->master->bus_num);

		for (i = 0; i < HEAD_LEN ; i++) {
			while (!spi_query_rxfifo(sspi->base_addr) && (--poll_time > 0))
				;
			pkt_head[i] =  readb(sspi->base_addr + SPI_RXDATA_REG);
		}

		if (poll_time <= 0) {
			SPI_ERR("[spi%d] cpu receive pkt head time out!\n", sspi->master->bus_num);
			spi_reset_fifo(sspi->base_addr);
			continue;
		} else if (sspi->result < 0) {
			SPI_ERR("[spi%d] xfer failed...\n", sspi->master->bus_num);
			spi_reset_fifo(sspi->base_addr);
			continue;
		}

		sunxi_spi_slave_handle_head(sspi, pkt_head);
	}

	spin_lock_irqsave(&sspi->lock, flags);
	sspi->task_flag = 1;
	spin_unlock_irqrestore(&sspi->lock, flags);

	kfree(pkt_head);

	return 0;
}

static int sunxi_spi_select_gpio_state(struct pinctrl *pctrl, char *name, u32 no)
{
	int ret = 0;
	struct pinctrl_state *pctrl_state = NULL;

	pctrl_state = pinctrl_lookup_state(pctrl, name);
	if (IS_ERR(pctrl_state)) {
		SPI_ERR("[spi%d] pinctrl_lookup_state(%s) failed! return %p\n", no, name, pctrl_state);
		return -1;
	}

	ret = pinctrl_select_state(pctrl, pctrl_state);
	if (ret < 0)
		SPI_ERR("[spi%d] pinctrl_select_state(%s) failed! return %d\n", no, name, ret);

	return ret;
}

static int sunxi_spi_request_gpio(struct sunxi_spi *sspi)
{
	int bus_no = sspi->pdev->id;

	if (sspi->pctrl != NULL)
		return sunxi_spi_select_gpio_state(sspi->pctrl, PINCTRL_STATE_DEFAULT, bus_no);

	dprintk(DEBUG_INIT, "[spi%d] Pinctrl init %s\n", sspi->master->bus_num, sspi->pdev->dev.init_name);

	sspi->pctrl = devm_pinctrl_get(&sspi->pdev->dev);
	if (IS_ERR(sspi->pctrl)) {
		SPI_ERR("[spi%d] devm_pinctrl_get() failed! return %ld\n",
				sspi->master->bus_num, PTR_ERR(sspi->pctrl));
		return -1;
	}

	return sunxi_spi_select_gpio_state(sspi->pctrl, PINCTRL_STATE_DEFAULT, bus_no);
}

static void sunxi_spi_release_gpio(struct sunxi_spi *sspi)
{
	devm_pinctrl_put(sspi->pctrl);
	sspi->pctrl = NULL;
}

static int sunxi_spi_resource_get(struct sunxi_spi *sspi)
{
	int ret;
	struct device_node *np = sspi->pdev->dev.of_node;

	ret = of_property_read_u32(np, "sample_mode", &sspi->sample_mode);
	if (ret) {
		SPI_ERR("Failed to get sample mode\n");
		sspi->sample_mode = SAMP_MODE_DL_DEFAULT;
	}
	ret = of_property_read_u32(np, "sample_delay", &sspi->sample_delay);
	if (ret) {
		SPI_ERR("Failed to get sample delay\n");
		sspi->sample_delay = SAMP_MODE_DL_DEFAULT;
	}
	dprintk(DEBUG_INIT, "sample_mode:%x sample_delay:%x\n",
				sspi->sample_mode, sspi->sample_delay);

	return 0;

}
static int sunxi_spi_clk_init(struct sunxi_spi *sspi, u32 mod_clk)
{
	int ret = 0;
	long rate = 0;

	sspi->pclk = of_clk_get(sspi->pdev->dev.of_node, 0);
	if (IS_ERR_OR_NULL(sspi->pclk)) {
		SPI_ERR("[spi%d] Unable to acquire module clock '%s', return %x\n",
			sspi->master->bus_num, sspi->dev_name, PTR_RET(sspi->pclk));
		return -1;
	}

	sspi->mclk = of_clk_get(sspi->pdev->dev.of_node, 1);
	if (IS_ERR_OR_NULL(sspi->mclk)) {
		SPI_ERR("[spi%d] Unable to acquire module clock '%s', return %x\n",
			sspi->master->bus_num, sspi->dev_name, PTR_RET(sspi->mclk));
		return -1;
	}

	ret = clk_set_parent(sspi->mclk, sspi->pclk);
	if (ret != 0) {
		SPI_ERR("[spi%d] clk_set_parent() failed! return %d\n",
			sspi->master->bus_num, ret);
		return -1;
	}

	rate = clk_round_rate(sspi->mclk, mod_clk);
	if (clk_set_rate(sspi->mclk, rate)) {
		SPI_ERR("[spi%d] spi clk_set_rate failed\n", sspi->master->bus_num);
		return -1;
	}

	dprintk(DEBUG_INIT, "[spi%d] mclk %u\n", sspi->master->bus_num, (unsigned)clk_get_rate(sspi->mclk));

	if (clk_prepare_enable(sspi->mclk)) {
		SPI_ERR("[spi%d] Couldn't enable module clock 'spi'\n", sspi->master->bus_num);
		return -EBUSY;
	}

	return clk_get_rate(sspi->mclk);
}

static int sunxi_spi_clk_exit(struct sunxi_spi *sspi)
{
	if (IS_ERR_OR_NULL(sspi->mclk)) {
		SPI_ERR("[spi%d] SPI mclk handle is invalid!\n", sspi->master->bus_num);
		return -1;
	}

	clk_disable_unprepare(sspi->mclk);
	clk_put(sspi->mclk);
	clk_put(sspi->pclk);
	sspi->mclk = NULL;
	sspi->pclk = NULL;
	return 0;
}

static int sunxi_spi_hw_init(struct sunxi_spi *sspi,
		struct sunxi_spi_platform_data *pdata, struct device *dev)
{
	void __iomem *base_addr = sspi->base_addr;
	u32 sclk_freq_def = 0;
	int sclk_freq = 0;
	int ret;

	ret = spi_regulator_request(pdata, dev);
	if (ret < 0) {
		SPI_ERR("[spi%d] request regulator failed!\n", sspi->master->bus_num);
		return ret;
	}
	spi_regulator_enable(pdata);

	if (sunxi_spi_request_gpio(sspi) < 0) {
		SPI_ERR("[spi%d] Request GPIO failed!\n", sspi->master->bus_num);
		return -1;
	}

	ret = of_property_read_u32(sspi->pdev->dev.of_node, "clock-frequency", &sclk_freq_def);
	if (ret) {
		SPI_ERR("[spi%d] Get clock-frequency property failed\n", sspi->master->bus_num);
		return -1;
	}

	sclk_freq = sunxi_spi_clk_init(sspi, sclk_freq_def);
	if (sclk_freq < 0) {
		SPI_ERR("[spi%d] sunxi_spi_clk_init(%s) failed!\n", sspi->master->bus_num, sspi->dev_name);
		return -1;
	}

	/* enable the spi module */
	if (!sspi->dbi_enabled)
		spi_enable_bus(base_addr);

	if (sspi->dbi_enabled) {
		spi_set_slave(base_addr);
		spi_set_dbi(base_addr);
		spi_enable_dbi(base_addr);
		spi_set_clk(10000000, sclk_freq, sspi);
		spi_enable_tp(base_addr);
	} else if (!sspi->mode) {
		/* master: set spi module clock;
		 * set the default frequency	10MHz
		 */
		spi_set_master(base_addr);
		spi_set_clk(10000000, sclk_freq, sspi);
		/* master : set POL,PHA,SSOPL,LMTF,DDB,DHB; default: SSCTL=0,SMC=1,TBW=0. */
		spi_config_tc(SPI_MODE_0, base_addr);
		spi_enable_tp(base_addr);
		/* manual control the chip select */
#ifndef CONFIG_AW_MTD_SPINAND
		spi_ss_ctrl(base_addr, 1);
#else
		if (strcmp(dev->of_node->child->name, "spi-nand"))
			spi_ss_ctrl(base_addr, 1);

#endif
	} else {
		//slave
		spi_set_slave(base_addr);
		/* master : set POL,PHA,SSOPL,LMTF,DDB,DHB; default: SSCTL=0,SMC=1,TBW=0. */
		spi_config_tc(SPI_MODE_0, base_addr);
		spi_set_clk(sclk_freq_def, sclk_freq, sspi);

		if (sspi->sample_delay == SAMP_MODE_DL_DEFAULT) {
			if (sclk_freq_def >= SPI_HIGH_FREQUENCY)
				spi_sample_delay(0, 1, 0, base_addr);
			else if (sclk_freq_def <= SPI_LOW_FREQUENCY)
				spi_sample_delay(1, 0, 0, base_addr);
			else
				spi_sample_delay(0, 0, 0, base_addr);
		} else {
			spi_samp_mode_enable(1, base_addr);
			spi_samp_dl_sw_status(1, base_addr);
			spi_set_sample_mode(sspi->sample_mode, base_addr);
			spi_set_sample_delay(sspi->sample_delay, base_addr);
		}
	}

	/* reset fifo */
	spi_reset_fifo(base_addr);

	return 0;
}

static int sunxi_spi_hw_exit(struct sunxi_spi *sspi, struct sunxi_spi_platform_data *pdata)
{
	/* disable the spi controller */
	spi_disable_bus(sspi->base_addr);

	/* disable module clock */
	sunxi_spi_clk_exit(sspi);
	sunxi_spi_release_gpio(sspi);

	spi_regulator_disable(pdata);
	spi_regulator_release(pdata);

	return 0;
}

#if (!defined(CONFIG_ARCH_SUN8IW16) && !defined(CONFIG_ARCH_SUN8IW19))
static ssize_t sunxi_spi_info_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = container_of(dev, struct platform_device, dev);
	struct sunxi_spi_platform_data *pdata = dev->platform_data;

	return snprintf(buf, PAGE_SIZE,
		"pdev->id   = %d\n"
		"pdev->name = %s\n"
		"pdev->num_resources = %u\n"
		"pdev->resource.mem = [%pa, %pa]\n"
		"pdev->resource.irq = %pa\n"
		"pdev->dev.platform_data.cs_num    = %d\n"
		"pdev->dev.platform_data.regulator = 0x%p\n"
		"pdev->dev.platform_data.regulator_id = %s\n",
		pdev->id, pdev->name, pdev->num_resources,
		&pdev->resource[0].start, &pdev->resource[0].end, &pdev->resource[1].start,
		 pdata->cs_num, pdata->regulator, pdata->regulator_id);
}
static struct device_attribute sunxi_spi_info_attr =
	__ATTR(info, S_IRUGO, sunxi_spi_info_show, NULL);

static ssize_t sunxi_spi_status_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct spi_master *master = dev_get_drvdata(dev);
	struct sunxi_spi *sspi = (struct sunxi_spi *)&master[1];
	char const *spi_mode[] = {"Single mode, half duplex read",
				  "Single mode, half duplex write",
				  "Single mode, full duplex read and write",
				  "Dual mode, half duplex read",
				  "Dual mode, half duplex write",
				  "Null"};
	char const *result_str[] = {"Success", "Fail"};

	if (master == NULL)
		return snprintf(buf, PAGE_SIZE, "%s\n", "spi_master is NULL!");

	return snprintf(buf, PAGE_SIZE,
			"master->bus_num = %d\n"
			"master->num_chipselect = %d\n"
			"master->dma_alignment  = %d\n"
			"master->mode_bits = %d\n"
			"master->flags = 0x%x, ->bus_lock_flag = 0x%x\n"
			"master->running = %d, ->rt = %d\n"
			"sspi->mode_type = %d [%s]\n"
			"sspi->result = %d [%s]\n"
			"sspi->base_addr = 0x%p, the SPI control register:\n"
			"[VER] 0x%02x = 0x%08x, [GCR] 0x%02x = 0x%08x, [TCR] 0x%02x = 0x%08x\n"
			"[ICR] 0x%02x = 0x%08x, [ISR] 0x%02x = 0x%08x, [FCR] 0x%02x = 0x%08x\n"
			"[FSR] 0x%02x = 0x%08x, [WCR] 0x%02x = 0x%08x, [CCR] 0x%02x = 0x%08x\n"
			"[BCR] 0x%02x = 0x%08x, [TCR] 0x%02x = 0x%08x, [BCC] 0x%02x = 0x%08x\n"
			"[DMA] 0x%02x = 0x%08x, [TXR] 0x%02x = 0x%08x, [RXD] 0x%02x = 0x%08x\n",
			master->bus_num, master->num_chipselect, master->dma_alignment,
			master->mode_bits, master->flags, master->bus_lock_flag,
			 master->running, master->rt,
			sspi->mode_type, spi_mode[sspi->mode_type],
			sspi->result, result_str[sspi->result],
			sspi->base_addr,
			SPI_VER_REG, readl(sspi->base_addr + SPI_VER_REG),
			SPI_GC_REG, readl(sspi->base_addr + SPI_GC_REG),
			SPI_TC_REG, readl(sspi->base_addr + SPI_TC_REG),
			SPI_INT_CTL_REG, readl(sspi->base_addr + SPI_INT_CTL_REG),
			SPI_INT_STA_REG, readl(sspi->base_addr + SPI_INT_STA_REG),

			SPI_FIFO_CTL_REG, readl(sspi->base_addr + SPI_FIFO_CTL_REG),
			SPI_FIFO_STA_REG, readl(sspi->base_addr + SPI_FIFO_STA_REG),
			SPI_WAIT_CNT_REG, readl(sspi->base_addr + SPI_WAIT_CNT_REG),
			SPI_CLK_CTL_REG, readl(sspi->base_addr + SPI_CLK_CTL_REG),
			SPI_BURST_CNT_REG, readl(sspi->base_addr + SPI_BURST_CNT_REG),

			SPI_TRANSMIT_CNT_REG, readl(sspi->base_addr + SPI_TRANSMIT_CNT_REG),
			SPI_BCC_REG, readl(sspi->base_addr + SPI_BCC_REG),
			SPI_DMA_CTL_REG, readl(sspi->base_addr + SPI_DMA_CTL_REG),
			SPI_TXDATA_REG, readl(sspi->base_addr + SPI_TXDATA_REG),
			SPI_RXDATA_REG, readl(sspi->base_addr + SPI_RXDATA_REG));
}
static struct device_attribute sunxi_spi_status_attr =
	__ATTR(status, S_IRUGO, sunxi_spi_status_show, NULL);

static void sunxi_spi_sysfs(struct platform_device *_pdev)
{
	device_create_file(&_pdev->dev, &sunxi_spi_info_attr);
	device_create_file(&_pdev->dev, &sunxi_spi_status_attr);
}
#endif

static int sunxi_spi_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct resource	*mem_res;
	struct sunxi_spi *sspi;
	struct sunxi_spi_platform_data *pdata;
	struct spi_master *master;
	struct sunxi_slave *slave = NULL;
	char spi_para[16] = {0};
	int ret = 0, err = 0, irq;

	if (np == NULL) {
		SPI_ERR("SPI failed to get of_node\n");
		return -ENODEV;
	}

	pdev->id = of_alias_get_id(np, "spi");
	if (pdev->id < 0) {
		SPI_ERR("SPI failed to get alias id\n");
		return -EINVAL;
	}

#ifdef CONFIG_DMA_ENGINE
	pdev->dev.dma_mask = &sunxi_spi_dma_mask;
	pdev->dev.coherent_dma_mask = DMA_BIT_MASK(32);
#endif

	pdata = kzalloc(sizeof(struct sunxi_spi_platform_data), GFP_KERNEL);
	if (pdata == NULL) {
		SPI_ERR("SPI failed to alloc mem\n");
		return -ENOMEM;
	}
	pdev->dev.platform_data = pdata;

	mem_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (mem_res == NULL) {
		SPI_ERR("Unable to get spi MEM resource\n");
		ret = -ENXIO;
		goto err0;
	}

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		SPI_ERR("No spi IRQ specified\n");
		ret = -ENXIO;
		goto err0;
	}

	snprintf(spi_para, sizeof(spi_para), "spi%d_cs_number", pdev->id);
	ret = of_property_read_u32(np, spi_para, &pdata->cs_num);
	if (ret) {
		SPI_ERR("Failed to get cs_number property\n");
		ret = -EINVAL;
		goto err0;
	}

	/* create spi master */
	master = spi_alloc_master(&pdev->dev, sizeof(struct sunxi_spi));
	if (master == NULL) {
		SPI_ERR("Unable to allocate SPI Master\n");
		ret = -ENOMEM;
		goto err0;
	}

	platform_set_drvdata(pdev, master);
	sspi = spi_master_get_devdata(master);
	memset(sspi, 0, sizeof(struct sunxi_spi));

	sspi->master		= master;
	sspi->mode_type	        = MODE_TYPE_NULL;

	master->dev.of_node     = pdev->dev.of_node;
	master->bus_num         = pdev->id;
	master->max_speed_hz	= SPI_MAX_FREQUENCY;
	master->setup           = sunxi_spi_setup;
	master->can_dma		= sunxi_spi_can_dma;
	master->transfer_one    = sunxi_spi_transfer_one;
	master->set_cs          = sunxi_spi_cs_control;
	master->num_chipselect  = pdata->cs_num;
	master->bits_per_word_mask = SPI_BPW_MASK(8);
	/* the spi->mode bits understood by this driver: */
	master->mode_bits       = SPI_CPOL | SPI_CPHA | SPI_CS_HIGH | SPI_LSB_FIRST |
				SPI_TX_DUAL | SPI_TX_QUAD | SPI_RX_DUAL | SPI_RX_QUAD;
#ifdef CONFIG_AW_MTD_SPINAND
	/*spi0 for flash exclusive*/
	if (!strcmp(np->child->name, "spi-nand")) {

		master->transfer_one_message = sunxi_spi_transfer_one_message;
		master->transfer_two = sunxi_spi_transfer_two;

	}
#endif

	ret = of_property_read_u32(np, "spi_dbi_enable", &sspi->dbi_enabled);
	if (ret == 0)
		dprintk(DEBUG_INIT, "[spi%d] SPI DBI INTERFACE\n",
				sspi->master->bus_num);
	else
		sspi->dbi_enabled = 0;


	ret = of_property_read_u32(np, "spi_slave_mode", &sspi->mode);
	if (sspi->mode)
		dprintk(DEBUG_INIT, "[spi%d] SPI SLAVE MODE\n", sspi->master->bus_num);
	else
		dprintk(DEBUG_INIT, "[spi%d] SPI MASTER MODE\n", sspi->master->bus_num);

	snprintf(sspi->dev_name, sizeof(sspi->dev_name), SUNXI_SPI_DEV_NAME"%d", pdev->id);
	err = devm_request_irq(&pdev->dev, irq, sunxi_spi_handler, 0, sspi->dev_name, sspi);
	if (err) {
		SPI_ERR("[spi%d] Cannot request IRQ\n", sspi->master->bus_num);
		ret = -EINVAL;
		goto err1;
	}

	if (request_mem_region(mem_res->start,
			resource_size(mem_res), pdev->name) == NULL) {
		SPI_ERR("[spi%d] Req mem region failed\n", sspi->master->bus_num);
		ret = -ENXIO;
		goto err2;
	}

	sspi->base_addr = ioremap(mem_res->start, resource_size(mem_res));
	if (sspi->base_addr == NULL) {
		SPI_ERR("[spi%d] Unable to remap IO\n", sspi->master->bus_num);
		ret = -ENXIO;
		goto err3;
	}
	sspi->base_addr_phy = mem_res->start;

	sspi->pdev = pdev;
	pdev->dev.init_name = sspi->dev_name;

	err = sunxi_spi_resource_get(sspi);
	if (err) {
		SPI_ERR("[spi%d] resource get error\n", sspi->master->bus_num);
		ret = -EINVAL;
		goto err1;
	}

	/* Setup Deufult Mode */
	ret = sunxi_spi_hw_init(sspi, pdata, &pdev->dev);
	if (ret != 0) {
		SPI_ERR("[spi%d] spi hw init failed!\n", sspi->master->bus_num);
		ret = -EINVAL;
		goto err4;
	}

	spin_lock_init(&sspi->lock);
	init_completion(&sspi->done);

	if (sspi->mode) {
		slave = kzalloc(sizeof(*slave), GFP_KERNEL);
		if (IS_ERR_OR_NULL(slave)) {
			SPI_ERR("[spi%d] failed to alloc mem\n", sspi->master->bus_num);
			ret = -ENOMEM;
			goto err5;
		}
		sspi->slave = slave;
		sspi->slave->set_up_txdata = sunxi_spi_slave_set_txdata;
		sspi->task = kthread_create(sunxi_spi_slave_task, sspi, "spi_slave");
		if (IS_ERR(sspi->task)) {
			SPI_ERR("[spi%d] unable to start kernel thread.\n", sspi->master->bus_num);
			ret = PTR_ERR(sspi->task);
			sspi->task = NULL;
			ret = -EINVAL;
			goto err6;
		}

		wake_up_process(sspi->task);
	} else {
		if (spi_register_master(master)) {
			SPI_ERR("[spi%d] cannot register SPI master\n", sspi->master->bus_num);
			ret = -EBUSY;
			goto err6;
		}
	}

#if (!defined(CONFIG_ARCH_SUN8IW16) && !defined(CONFIG_ARCH_SUN8IW19))
	sunxi_spi_sysfs(pdev);
#endif

	dprintk(DEBUG_INFO, "[spi%d] loaded for Bus with %d Slaves at most\n",
		master->bus_num, master->num_chipselect);
	dprintk(DEBUG_INIT, "[spi%d]: driver probe succeed, base %p, irq %d\n",
		master->bus_num, sspi->base_addr, irq);
	return 0;

err6:
	if (sspi->mode && slave)
		kfree(slave);
err5:
	sunxi_spi_hw_exit(sspi, pdata);

err4:
	iounmap(sspi->base_addr);
err3:
	release_mem_region(mem_res->start, resource_size(mem_res));
err2:
err1:
/*
 *#ifdef CONFIG_AW_MTD_SPINAND
 *        [>spi0 for flash exclusive<]
 *        if (master->bus_num == 0) {
 *                sspi->stx.len = 0;
 *                sspi->stx.state = INIT_STATE;
 *                kfree(sspi->stx.tx.tx_buf);
 *        }
 *#endif
 */
	platform_set_drvdata(pdev, NULL);
	spi_master_put(master);
err0:
	kfree(pdata);

	return ret;
}

static int sunxi_spi_remove(struct platform_device *pdev)
{
	struct spi_master *master = spi_master_get(platform_get_drvdata(pdev));
	struct sunxi_spi *sspi = spi_master_get_devdata(master);
	struct resource	*mem_res;

	if ((sspi->mode) && (!sspi->task_flag))
		if (!IS_ERR(sspi->task))
			kthread_stop(sspi->task);

	sunxi_spi_hw_exit(sspi, pdev->dev.platform_data);
	iounmap(sspi->base_addr);
	mem_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (mem_res != NULL)
		release_mem_region(mem_res->start, resource_size(mem_res));
#ifdef CONFIG_DMA_ENGINE
	if (master->dma_tx)
		dma_release_channel(master->dma_tx);
	if (master->dma_rx)
		dma_release_channel(master->dma_rx);
#endif
	spi_unregister_master(master);
	platform_set_drvdata(pdev, NULL);
	spi_master_put(master);
	kfree(pdev->dev.platform_data);

/*
 *#ifdef CONFIG_AW_MTD_SPINAND
 *        [>spi0 for flash exclusive<]
 *        if (master->bus_num == 0) {
 *                sspi->stx.len = 0;
 *                sspi->stx.state = INIT_STATE;
 *                kfree(sspi->stx.tx.tx_buf);
 *        }
 *#endif
 */

	return 0;
}

#ifdef CONFIG_PM
static int sunxi_spi_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct spi_master *master = spi_master_get(platform_get_drvdata(pdev));
	struct sunxi_spi *sspi = spi_master_get_devdata(master);
	int ret;

	ret = spi_master_suspend(master);
	if (ret < 0)
		return ret;

	spi_disable_bus(sspi->base_addr);
	sunxi_spi_clk_exit(sspi);

	ret = sunxi_spi_select_gpio_state(sspi->pctrl, PINCTRL_STATE_SLEEP, master->bus_num);
	spi_regulator_disable(dev->platform_data);

	dprintk(DEBUG_SUSPEND, "[spi%d] finish\n", master->bus_num);

	return 0;
}

static int sunxi_spi_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct spi_master *master = spi_master_get(platform_get_drvdata(pdev));
	struct sunxi_spi  *sspi = spi_master_get_devdata(master);
	int ret;

	ret = spi_master_resume(master);
	if (ret < 0)
		return ret;
	ret = sunxi_spi_hw_init(sspi, pdev->dev.platform_data, dev);

	dprintk(DEBUG_SUSPEND, "[spi%d] finish\n", master->bus_num);

	return ret;
}

static const struct dev_pm_ops sunxi_spi_dev_pm_ops = {
	.suspend = sunxi_spi_suspend,
	.resume  = sunxi_spi_resume,
};

#define SUNXI_SPI_DEV_PM_OPS (&sunxi_spi_dev_pm_ops)
#else
#define SUNXI_SPI_DEV_PM_OPS NULL
#endif /* CONFIG_PM */

static const struct of_device_id sunxi_spi_match[] = {
	{ .compatible = "allwinner,sun8i-spi", },
	{ .compatible = "allwinner,sun50i-spi", },
	{},
};
MODULE_DEVICE_TABLE(of, sunxi_spi_match);


static struct platform_driver sunxi_spi_driver = {
	.probe   = sunxi_spi_probe,
	.remove  = sunxi_spi_remove,
	.driver = {
		.name	= SUNXI_SPI_DEV_NAME,
		.owner	= THIS_MODULE,
		.pm		= SUNXI_SPI_DEV_PM_OPS,
		.of_match_table = sunxi_spi_match,
	},
};

static int __init sunxi_spi_init(void)
{
	return platform_driver_register(&sunxi_spi_driver);
}

static void __exit sunxi_spi_exit(void)
{
	platform_driver_unregister(&sunxi_spi_driver);
}

fs_initcall_sync(sunxi_spi_init);
module_exit(sunxi_spi_exit);
module_param_named(debug, debug_mask, int, 0664);

MODULE_AUTHOR("pannan");
MODULE_DESCRIPTION("SUNXI SPI BUS Driver");
MODULE_ALIAS("platform:"SUNXI_SPI_DEV_NAME);
MODULE_LICENSE("GPL");
