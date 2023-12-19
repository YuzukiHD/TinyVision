// SPDX-License-Identifier: GPL-2.0-only
/*
 * drivers/i2c/busses/i2c-sunxi.c
 *
 * Copyright (C) 2013 Allwinner.
 * Pan Nan <pannan@reuuimllatech.com>
 *
 * SUNXI TWI Controller Driver
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * 2013.5.3 Mintow <duanmintao@allwinnertech.com>
 *    Adapt to all the new chip of Allwinner. Support sun8i/sun9i
 *
 * 2021.8.15 Lewis <liuyu@allwinnertech.com>
 *	Optimization the probe function and all the function involved code
 *
 * 2021.10.13 Lewis <liuyu@allwinnertec.com>
 *	Refactored i2c xfer code
 */

/* #define DEBUG */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/clk-provider.h>
#include <linux/clk/sunxi.h>
#include <linux/reset.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/pinctrl/consumer.h>
#include <linux/pm_runtime.h>
#include <linux/dma-mapping.h>
#include <linux/dmaengine.h>
#include <linux/dmapool.h>
#include <linux/dma/sunxi-dma.h>
#include <asm/uaccess.h>
#include <linux/time.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/regulator/consumer.h>

/* I2C Register Offset */
/*  31:8bit reserved,7-1bit for slave addr,0 bit for GCE */
#define I2C_ADDR		(0x00)
/*  31:8bit reserved,7-0bit for second addr in 10bit addr */
#define I2C_XADDR		(0x04)
/*  31:8bit reserved, 7-0bit send or receive data byte */
#define I2C_DATA		(0x08)
/*  INT_EN,BUS_EN,M_STA,INT_FLAG,A_ACK */
#define I2C_CNTR		(0x0C)
/*  28 interrupt types + 0xF8 normal type = 29  */
#define I2C_STAT		(0x10)
/*  31:7bit reserved,6-3bit,CLK_M,2-0bit CLK_N */
#define I2C_CCR			(0x14)
/*  31:1bit reserved;0bit,write 1 to clear 0. */
#define I2C_SRST		(0x18)
/*  31:2bit reserved,1:0 bit data byte follow read command */
#define I2C_EFR			(0x1C)
/*  31:6bits reserved  5:0bit for sda&scl control*/
#define I2C_LCR			(0x20)
/* 23:16 VER_BIG 7:0:VER_SMALL */
#define I2C_VERSION		(0xFC)

/* I2C_DATA */
#define I2C_DATA_MASK	(0xff << 0)

/* I2C_CNTR */
#define CLK_COUNT_MODE	(0x1 << 0)
/* set 1 to send A_ACK,then low level on SDA */
#define A_ACK		(0x1 << 2)
/* INT_FLAG,interrupt status flag: set '1' when interrupt coming */
#define INT_FLAG	(0x1 << 3)
/* M_STP,Automatic clear 0 */
#define M_STP		(0x1 << 4)
/* M_STA,atutomatic clear 0 */
#define M_STA		(0x1 << 5)
/* BUS_EN, master mode should be set 1.*/
#define BUS_EN		(0x1 << 6)
/* INT_EN, set 1 to enable interrupt.*/
#define INT_EN		(0x1 << 7)	/* INT_EN */
/* 31:8 bit reserved */

/* I2C_STAT */
/*
 * -------------------------------------------------------------------
 * Code   Status
 * 00h    Bus error
 * 08h    START condition transmitted
 * 10h    Repeated START condition transmitted
 * 18h    Address + Write bit transmitted, ACK received
 * 20h    Address + Write bit transmitted, ACK not received
 * 28h    Data byte transmitted in master mode, ACK received
 * 30h    Data byte transmitted in master mode, ACK not received
 * 38h    Arbitration lost in address or data byte
 * 40h    Address + Read bit transmitted, ACK received
 * 48h    Address + Read bit transmitted, ACK not received
 * 50h    Data byte received in master mode, ACK transmitted
 * 58h    Data byte received in master mode, not ACK transmitted
 * 60h    Slave address + Write bit received, ACK transmitted
 * 68h    Arbitration lost in address as master,
 *	  slave address + Write bit received, ACK transmitted
 * 70h    General Call address received, ACK transmitted
 * 78h    Arbitration lost in address as master,
 *	  General Call address received, ACK transmitted
 * 80h    Data byte received after slave address received, ACK transmitted
 * 88h    Data byte received after slave address received, not ACK transmitted
 * 90h    Data byte received after General Call received, ACK transmitted
 * 98h    Data byte received after General Call received, not ACK transmitted
 * A0h    STOP or repeated START condition received in slave mode
 * A8h    Slave address + Read bit received, ACK transmitted
 * B0h    Arbitration lost in address as master,
 *	  slave address + Read bit received, ACK transmitted
 * B8h    Data byte transmitted in slave mode, ACK received
 * C0h    Data byte transmitted in slave mode, ACK not received
 * C8h    Last byte transmitted in slave mode, ACK received
 * D0h    Second Address byte + Write bit transmitted, ACK received
 * D8h    Second Address byte + Write bit transmitted, ACK not received
 * F8h    No relevant status information or no interrupt
 *--------------------------------------------------------------------------
 */
#define I2C_STAT_MASK		(0xff)
/* 7:0 bits use only,default is 0xF8 */
#define I2C_STAT_BUS_ERR	(0x00)	/* BUS ERROR */
/* timeout when sending 9th scl clk*/
#define I2C_STAT_TIMEOUT_9CLK  (0x01)
/*start can't send out*/
#define I2C_STAT_TX_NSTA	(0x02)	//defined by us not spec
/* Master mode use only */
#define I2C_STAT_TX_STA		(0x08)	/* START condition transmitted */
/* Repeated START condition transmitted */
#define I2C_STAT_TX_RESTA	(0x10)
/* Address+Write bit transmitted, ACK received */
#define I2C_STAT_TX_AW_ACK	(0x18)
/* Address+Write bit transmitted, ACK not received */
#define I2C_STAT_TX_AW_NAK	(0x20)
/* data byte transmitted in master mode,ack received */
#define I2C_STAT_TXD_ACK	(0x28)
/* data byte transmitted in master mode ,ack not received */
#define I2C_STAT_TXD_NAK	(0x30)
/* arbitration lost in address or data byte */
#define I2C_STAT_ARBLOST	(0x38)
/* Address+Read bit transmitted, ACK received */
#define I2C_STAT_TX_AR_ACK	(0x40)
/* Address+Read bit transmitted, ACK not received */
#define I2C_STAT_TX_AR_NAK	(0x48)
/* data byte received in master mode ,ack transmitted */
#define I2C_STAT_RXD_ACK	(0x50)
/* date byte received in master mode,not ack transmitted */
#define I2C_STAT_RXD_NAK	(0x58)
/* Slave mode use only */
/* Slave address+Write bit received, ACK transmitted */
#define I2C_STAT_RXWS_ACK	(0x60)
#define I2C_STAT_ARBLOST_RXWS_ACK	(0x68)
/* General Call address received, ACK transmitted */
#define I2C_STAT_RXGCAS_ACK		(0x70)
#define I2C_STAT_ARBLOST_RXGCAS_ACK	(0x78)
#define I2C_STAT_RXDS_ACK		(0x80)
#define I2C_STAT_RXDS_NAK		(0x88)
#define I2C_STAT_RXDGCAS_ACK		(0x90)
#define I2C_STAT_RXDGCAS_NAK		(0x98)
#define I2C_STAT_RXSTPS_RXRESTAS	(0xA0)
#define I2C_STAT_RXRS_ACK		(0xA8)
#define I2C_STAT_ARBLOST_SLAR_ACK	(0xB0)
/* 10bit Address, second part of address */
/* Second Address byte+Write bit transmitted,ACK received */
#define I2C_STAT_TX_SAW_ACK		(0xD0)
/* Second Address byte+Write bit transmitted,ACK not received */
#define I2C_STAT_TX_SAW_NAK		(0xD8)
/* No relevant status information,INT_FLAG = 0 */
#define I2C_STAT_IDLE			(0xF8)
/* status erro*/
#define I2C_STAT_ERROR			(0xF9)

/*I2C_CCR*/
/*
 * Fin is APB CLOCK INPUT;
 * Fsample = F0 = Fin/2^CLK_N;
 *	F1 = F0/(CLK_M+1);
 *
 * Foscl = F1/10 = Fin/(2^CLK_N * (CLK_M+1)*10);
 * Foscl is clock SCL;standard mode:100KHz or fast mode:400KHz
 */
#define I2C_CLK_DUTY		(0x1 << 7)	/* 7bit  */
#define I2C_CLK_DIV_M_OFFSET	3
#define I2C_CLK_DIV_M		(0xf << I2C_CLK_DIV_M_OFFSET)	/* 6:3bit  */
#define I2C_CLK_DIV_N_OFFSET	0
#define I2C_CLK_DIV_N		(0x7 << I2C_CLK_DIV_N_OFFSET)	/* 2:0bit */

/* I2C_SRST */
/* write 1 to clear 0, when complete soft reset clear 0 */
#define I2C_SOFT_RST		(0x1 << 0)

/* I2C_EFR*/
/* default -- 0x0 */
/* 00:no,01: 1byte, 10:2 bytes, 11: 3bytes */
#define I2C_EFR_MASK		(0x3 << 0)
#define NO_DATA_WROTE		(0x0 << 0)
#define BYTE_DATA_WROTE		(0x1 << 0)
#define BYTES_DATA1_WROTE	(0x2 << 0)
#define BYTES_DATA2_WROTE	(0x3 << 0)

/* I2C_LCR*/
#define SCL_STATE		(0x1 << 5)
#define SDA_STATE		(0x1 << 4)
#define SCL_CTL			(0x1 << 3)
#define SCL_CTL_EN		(0x1 << 2)
#define SDA_CTL			(0x1 << 1)
#define SDA_CTL_EN		(0x1 << 0)

#define I2C_DRV_CTRL	(0x200)
#define I2C_DRV_CFG		(0x204)
#define I2C_DRV_SLV		(0x208)
#define I2C_DRV_FMT		(0x20C)
#define I2C_DRV_BUS_CTRL	(0x210)
#define I2C_DRV_INT_CTRL	(0x214)
#define I2C_DRV_DMA_CFG		(0x218)
#define I2C_DRV_FIFO_CON	(0x21C)
#define I2C_DRV_SEND_FIFO_ACC	(0x300)
#define I2C_DRV_RECV_FIFO_ACC	(0x304)

/* I2C_DRV_CTRL */
/* 0:module disable; 1:module enable; only use in I2C master Mode */
#define I2C_DRV_EN		(0x01 << 0)
/* 0:normal; 1:reset */
#define SOFT_RESET		(0x01 << 1)
#define TIMEOUT_N_OFFSET	8
#define TIMEOUT_N		(0xff << TIMEOUT_N_OFFSET)
#define I2C_DRV_STAT_OFFSET	16
#define I2C_DRV_STAT_MASK	(0xff << I2C_DRV_STAT_OFFSET)

#define TRAN_RESULT	(0x07 << 24)
/* 0:send slave_id + W; 1:do not send slave_id + W */
#define READ_TRAN_MODE	(0x01 << 28)
/* 0:restart; 1:STOP + START */
#define RESTART_MODE	(0x01 << 29)
/* 0:transmission idle; 1:start transmission */
#define START_TRAN	(0x01 << 31)

/* I2C_DRV_CFG */
#define PACKET_CNT_OFFSET	0
#define PACKET_CNT	(0xffff << PACKET_CNT_OFFSET)
#define PACKET_INTERVAL_OFFSET	16
#define PACKET_INTERVAL	(0xffff << PACKET_INTERVAL_OFFSET)

/* I2C_DRV_SLV */
#define SLV_ID_X_OFFSET	0
#define SLV_ID_X	(0xff << SLV_ID_X_OFFSET)
#define CMD		(0x01 << 8)
#define SLV_ID_OFFSET	9
#define SLV_ID		(0x7f << SLV_ID_OFFSET)

/* I2C_DRV_FMT */
/* how many bytes be sent/received as data */
#define DATA_BYTE_OFFSET 0
#define DATA_BYTE	(0xffff << DATA_BYTE_OFFSET)
/* how many btyes be sent as slave device reg address */
#define ADDR_BYTE_OFFSET 16
#define ADDR_BYTE	(0xff << ADDR_BYTE_OFFSET)

/* I2C_DRV_BUS_CTRL */
/* SDA manual output en */
#define SDA_MOE		(0x01 << 0)
/* SCL manual output en */
#define SCL_MOE		(0x01 << 1)
/* SDA manual output value */
#define SDA_MOV		(0x01 << 2)
/* SCL manual output value */
#define SCL_MOV		(0x01 << 3)
/* SDA current status */
#define SDA_STA		(0x01 << 6)
/* SCL current status */
#define SCL_STA		(0x01 << 7)
#define I2C_DRV_CLK_M_OFFSET	8
#define I2C_DRV_CLK_M		(0x0f << I2C_DRV_CLK_M_OFFSET)
#define I2C_DRV_CLK_N_OFFSET	12
#define I2C_DRV_CLK_N		(0x07 << I2C_DRV_CLK_N_OFFSET)
#define I2C_DRV_CLK_DUTY	(0x01 << 15)
#define I2C_DRV_COUNT_MODE	(0x01 << 16)

/* I2C_DRV_INT_CTRL */
#define TRAN_COM_PD	(0x1 << 0)
#define TRAN_ERR_PD	(0x1 << 1)
#define TX_REQ_PD	(0x1 << 2)
#define RX_REQ_PD	(0x1 << 3)
#define TRAN_COM_INT_EN	(0x1 << 16)
#define TRAN_ERR_INT_EN	(0x1 << 17)
#define TX_REQ_INT_EN	(0x1 << 18)
#define RX_REQ_INT_EN	(0x1 << 19)
#define I2C_DRV_INT_EN_MASK	(0x0f << 16)
#define I2C_DRV_INT_STA_MASK	(0x0f << 0)

/* I2C_DRV_DMA_CFG */
#define TX_TRIG_OFFSET 0
#define TX_TRIG		(0x3f << TX_TRIG_OFFSET)
#define DMA_TX_EN	(0x01 << 8)
#define RX_TRIG_OFFSET	16
#define RX_TRIG		(0x3f << RX_TRIG_OFFSET)
#define DMA_RX_EN	(0x01 << 24)
#define I2C_DRQEN_MASK	(DMA_TX_EN | DMA_RX_EN)

/* I2C_DRV_FIFO_CON */
/* the number of data in SEND_FIFO */
#define SEND_FIFO_CONTENT_OFFSET	0
#define SEND_FIFO_CONTENT	(0x3f << SEND_FIFO_CONTENT_OFFSET)
/* Set this bit to clear SEND_FIFO pointer, and this bit cleared automatically */
#define SEND_FIFO_CLEAR		(0x01 << 5)
#define RECV_FIFO_CONTENT_OFFSET	16
#define RECV_FIFO_CONTENT	(0x3f << RECV_FIFO_CONTENT_OFFSET)
#define RECV_FIFO_CLEAR		(0x01 << 22)

/* I2C_DRV_SEND_FIFO_ACC */
#define SEND_DATA_FIFO	(0xff << 0)
/* I2C_DRV_RECV_FIFO_ACC */
#define RECV_DATA_FIFO	(0xff << 0)
/* end of i2c regiter offset */

#define LOOP_TIMEOUT	1024
#define STANDDARD_FREQ	100000
#define AUTOSUSPEND_TIMEOUT 5000
#define HEXADECIMAL		(0x10)
#define REG_INTERVAL	(0x04)
#define REG_CL			(0x0c)
#define DMA_THRESHOLD	32
#define MAX_FIFO		32
#define DMA_TIMEOUT		1000

#define I2C_READ	true
#define I2C_WRITE	false

/* i2c driver result i2c->result */
enum SUNXI_I2C_XFER_RESULT {
	SUNXI_I2C_XFER_RESULT_NULL	= -1,
	SUNXI_I2C_XFER_RESULT_RUNNING,
	SUNXI_I2C_XFER_RESULT_COMPLETE,
	SUNXI_I2C_XFER_RESULT_ERROR,
};

/* i2c transfer status i2c->status */
enum SUNXI_I2C_XFER_STATUS {
	SUNXI_I2C_XFER_STATUS_IDLE	= 0x1,
	SUNXI_I2C_XFER_STATUS_START	= 0x2,
	SUNXI_I2C_XFER_STATUS_RUNNING	= 0x4,
	SUNXI_I2C_XFER_STATUS_SHUTDOWN	= 0x8,
};

struct sunxi_i2c_dma {
	struct dma_chan *chan;
	dma_addr_t dma_buf;
	unsigned int dma_len;
	enum dma_transfer_direction dma_transfer_dir;
	enum dma_data_direction dma_data_dir;
};

struct sunxi_i2c {
	/* i2c framework datai */
	struct i2c_adapter adap;
	struct platform_device *pdev;
	struct device *dev;
	struct i2c_msg *msg;
	/* the total num of msg */
	unsigned int msg_num;
	/* the current msg index -> msg[msg_idx] */
	unsigned int msg_idx;
	/* the current msg's buf data index -> msg->buf[buf_idx] */
	unsigned int buf_idx;
	/* for i2c core bus lock */
	struct mutex bus_lock;

	/* dts data */
	struct resource *res;
	void __iomem *base_addr;
	struct clk *bus_clk;
	struct reset_control    *reset;
	unsigned int bus_freq;
	struct regulator *regulator;
	char regulator_id[16];
	struct pinctrl *pctrl;
	int irq;
	int irq_flag;
	unsigned int twi_drv_used;
	unsigned int no_suspend;
	unsigned int pkt_interval;
	struct sunxi_i2c_dma *dma_tx;
	struct sunxi_i2c_dma *dma_rx;
	struct sunxi_i2c_dma *dma_using;
	u8 *dma_buf;
	u32 vol;  /* the i2c io voltage */

	/* other data */
	int bus_num;
	enum SUNXI_I2C_XFER_RESULT result; /* null, running, complete, error */
	enum SUNXI_I2C_XFER_STATUS status; /* start, running, idle, shutdown */
	unsigned int debug_state; /* log the twi machine state */

	spinlock_t lock; /* syn */
	wait_queue_head_t wait;
	struct completion cmd_complete;

	unsigned int reg1[16]; /* store the i2c engined mode resigter status */
	unsigned int reg2[16]; /* store the i2c drv mode regiter status */
};

#ifdef DEBUG
void sunxi_i2c_dump_reg(struct sunxi_i2c *i2c, u32 offset, u32 len)
{
	u32 i;
	u8 buf[64], cnt = 0;

	for (i = 0; i < len; i = i + REG_INTERVAL) {
		if (i%HEXADECIMAL == 0)
			cnt += sprintf(buf + cnt, "0x%08x: ",
				       (u32)(i2c->res->start + offset + i));

		cnt += sprintf(buf + cnt, "%08x ",
			       readl(i2c->base_addr + offset + i));

		if (i%HEXADECIMAL == REG_CL) {
			pr_warn("%s\n", buf);
			cnt = 0;
		}
	}
}
#else
void sunxi_i2c_dump_reg(struct sunxi_i2c *i2c, u32 offset, u32 len) { }
#endif

/* clear the interrupt flag,
 * the i2c bus xfer status (register I2C_STAT) will changed as following
 */
static inline void sunxi_i2c_engine_clear_irq(void __iomem *base_addr)
{
	u32 reg_val = readl(base_addr + I2C_CNTR);

	/* start and stop bit should be 0 */
	reg_val |= INT_FLAG;
	reg_val &= ~(M_STA | M_STP);

	writel(reg_val, base_addr + I2C_CNTR);
}

/* only when get the last data, we will clear the flag when stop */
static inline void
sunxi_i2c_engine_get_byte(void __iomem *base_addr, unsigned char *buffer)
{
	*buffer = (unsigned char)(I2C_DATA_MASK & readl(base_addr + I2C_DATA));
}

/* write data and clear irq flag to trigger send flow */
static inline void
sunxi_i2c_engine_put_byte(struct sunxi_i2c *i2c, const unsigned char *buffer)
{
	unsigned int reg_val;

	reg_val = *buffer & I2C_DATA_MASK;
	writel(reg_val, i2c->base_addr + I2C_DATA);

	dev_dbg(i2c->dev, "engine-mode: data 0x%x xfered\n", reg_val);
}

static inline void sunxi_i2c_engine_enable_irq(void __iomem *base_addr)
{
	unsigned int reg_val = readl(base_addr + I2C_CNTR);
	/*
	 * 1 when enable irq for next operation, set intflag to 0 to prevent
	 * to clear it by a mistake (intflag bit is write-1-to-clear bit)
	 * 2 Similarly, mask START bit and STOP bit to prevent to set it
	 * twice by a mistake (START bit and STOP bit are self-clear-to-0 bits)
	 */
	reg_val |= INT_EN;
	reg_val &= ~(INT_FLAG) | M_STA | M_STP;
	writel(reg_val, base_addr + I2C_CNTR);
}

static inline void sunxi_i2c_engine_disable_irq(void __iomem *base_addr)
{
	unsigned int reg_val = readl(base_addr + I2C_CNTR);

	reg_val &= ~INT_EN;
	reg_val &= ~(INT_FLAG | M_STA | M_STP);
	writel(reg_val, base_addr + I2C_CNTR);
}

static inline void sunxi_i2c_bus_enable(struct sunxi_i2c *i2c)
{
	unsigned int reg_val;

	if (i2c->twi_drv_used) {
		reg_val = readl(i2c->base_addr + I2C_DRV_CTRL);
		reg_val |= I2C_DRV_EN;
		writel(reg_val, i2c->base_addr + I2C_DRV_CTRL);
	} else {
		reg_val = readl(i2c->base_addr + I2C_CNTR);
		reg_val |= BUS_EN;
		writel(reg_val, i2c->base_addr + I2C_CNTR);
	}
}

static inline void sunxi_i2c_bus_disable(struct sunxi_i2c *i2c)
{
	unsigned int reg_val;

	if (i2c->twi_drv_used) {
		reg_val = readl(i2c->base_addr + I2C_DRV_CTRL);
		reg_val &= ~I2C_DRV_EN;
		writel(reg_val, i2c->base_addr + I2C_DRV_CTRL);
	} else {
		reg_val = readl(i2c->base_addr + I2C_CNTR);
		reg_val &= ~BUS_EN;
		writel(reg_val, i2c->base_addr + I2C_CNTR);
	}
}

/* trigger start signal, the start bit will be cleared automatically */
static inline void sunxi_i2c_engine_set_start(void __iomem *base_addr)
{
	u32 reg_val = readl(base_addr + I2C_CNTR);

	reg_val |= M_STA;
	reg_val &= ~(INT_FLAG);

	writel(reg_val, base_addr + I2C_CNTR);
}

/* get start bit status, poll if start signal is sent
 * return 0 on success, return 1 on failed
 */
static inline unsigned int sunxi_i2c_engine_get_start(void __iomem *base_addr)
{
	unsigned int reg_val = readl(base_addr + I2C_CNTR);

	return reg_val & M_STA;
}

/* trigger stop signal and clear int flag
 * the stop bit will be cleared automatically
 */
static inline void sunxi_i2c_engine_set_stop(void __iomem *base_addr)
{
	u32 reg_val = readl(base_addr + I2C_CNTR);

	reg_val |= M_STP;
	reg_val &= ~INT_FLAG;
	writel(reg_val, base_addr + I2C_CNTR);
}

/* get stop bit status, poll if stop signal is sent */
static inline unsigned int sunxi_i2c_engine_get_stop(void __iomem *base_addr)
{
	unsigned int reg_val = readl(base_addr + I2C_CNTR);

	return reg_val & M_STP;
}

/* when sending ack or nack, it will send ack automatically */
static inline void sunxi_i2c_engine_enable_ack(void __iomem  *base_addr)
{
	u32 reg_val = readl(base_addr + I2C_CNTR);

	reg_val |= A_ACK;

	writel(reg_val, base_addr + I2C_CNTR);
}

static inline void sunxi_i2c_engine_disable_ack(void __iomem  *base_addr)
{
	u32 reg_val = readl(base_addr + I2C_CNTR);

	reg_val &= ~A_ACK;

	writel(reg_val, base_addr + I2C_CNTR);
}

/* check the engine-mode or drv-mode irq coming or not with irq enable or not
 * return 0 on engine-mode interrupt is coming with irq is enabled
 * return 1 on drv-mode interrupt is coming with irq is enabled
 * otherwise return the error num
 */
static inline unsigned int sunxi_i2c_check_irq(struct sunxi_i2c *i2c)
{
	u32 status;

	if (i2c->twi_drv_used) {
		status = readl(i2c->base_addr + I2C_DRV_INT_CTRL);
		if ((status & I2C_DRV_INT_EN_MASK) && (status & I2C_DRV_INT_STA_MASK))
			return 1;
	} else {
		status = readl(i2c->base_addr + I2C_CNTR);
		if ((status & INT_EN) && (status & INT_FLAG))
			return 0;
	}

	return -EINVAL;
}

/* get the i2c controller current xfer status */
static inline unsigned int sunxi_i2c_get_xfer_sta(struct sunxi_i2c *i2c)
{
	u32 status;

	if (i2c->twi_drv_used) {
		status = readl(i2c->base_addr + I2C_DRV_CTRL) & I2C_DRV_STAT_MASK;
		status >>=  I2C_DRV_STAT_OFFSET;
	} else {
		status = readl(i2c->base_addr + I2C_STAT) & I2C_STAT_MASK;
	}

	return status;
}

/* set i2c controller clock
 * clk_n: clock divider factor n
 * clk_m: clock divider factor m
 */
static void sunxi_i2c_set_clock(struct sunxi_i2c *i2c, u8 clk_m, u8 clk_n)
{
	u32 clk_n_mask, clk_n_offset, clk_m_offset, clk_m_mask;
	u32 reg, duty, reg_val;

	/* @IP-TODO
	 * drv-mode set clkm and clkn bit to adjust frequency, finally the
	 * I2C_DRV_BUS_CTRL register will not be changed, the value of the I2C_CCR
	 * register will represent the current drv-mode operating frequency
	 */
	if (i2c->twi_drv_used) {
		reg = I2C_DRV_BUS_CTRL;
		clk_n_mask = I2C_DRV_CLK_N;
		clk_n_offset = I2C_DRV_CLK_N_OFFSET;
		clk_m_mask = I2C_DRV_CLK_M;
		clk_m_offset = I2C_DRV_CLK_M_OFFSET;
		duty = I2C_DRV_CLK_DUTY;
	} else {
		reg = I2C_CCR;
		clk_n_mask = I2C_CLK_DIV_N;
		clk_n_offset = I2C_CLK_DIV_N_OFFSET;
		clk_m_mask = I2C_CLK_DIV_M;
		clk_m_offset = I2C_CLK_DIV_M_OFFSET;
		duty = I2C_CLK_DUTY;
	}

	reg_val = readl(i2c->base_addr + reg);

	reg_val &= ~(clk_m_mask | clk_n_mask);
	reg_val |= ((clk_m << clk_m_offset) & clk_m_mask);
	reg_val |= ((clk_n << clk_n_offset) & clk_n_mask);
	if (i2c->bus_freq > STANDDARD_FREQ)
		reg_val |= duty;
	else
		reg_val &= ~(duty);

	writel(reg_val, i2c->base_addr + reg);
}

static int sunxi_i2c_set_frequency(struct sunxi_i2c *i2c)
{
	u8 clk_m = 0, clk_n = 0, _2_pow_clk_n = 1;
	unsigned int clk_in, clk_src, divider, clk_real;

	if (i2c->twi_drv_used) {
		/* the clk_in of the drv mode specified in the spec is fixed to 24M */
		clk_in = 24000000;
	} else {
		clk_in  = clk_get_rate(i2c->bus_clk);
		if (clk_in == 0) {
			dev_err(i2c->dev, "get clocksource clock freq failed!\n");
			return -1;
		}
	}

	clk_src = clk_in / 10;
	divider = clk_src / i2c->bus_freq;

	if (!divider) {
		clk_m = 1;
		goto i2c_set_clk;
	}

	/*
	 * search clk_n and clk_m,from large to small value so
	 * that can quickly find suitable m & n.
	 */
	while (clk_n < 8) { /* 3bits max value is 8 */
		/* (m+1)*2^n = divider -->m = divider/2^n -1 */
		clk_m = (divider / _2_pow_clk_n) - 1;
		/* clk_m = (divider >> (_2_pow_clk_n>>1))-1 */
		while (clk_m < 16) { /* 4bits max value is 16 */
			/* src_clk/((m+1)*2^n) */
			clk_real = clk_src / (clk_m + 1) / _2_pow_clk_n;
			if (clk_real <= i2c->bus_freq)
				goto i2c_set_clk;
			else
				clk_m++;
		}
		clk_n++;
		_2_pow_clk_n *= 2; /* mutilple by 2 */
	}

i2c_set_clk:
	sunxi_i2c_set_clock(i2c, clk_m, clk_n);

	return 0;
}

/*
 * i2c controller soft_reset can only clear flag bit inside of ip, include the
 * state machine parameters, counters, various flags, fifo, fifo-cnt.
 *
 * But the internal configurations or external register configurations of ip
 * will not be changed.
 */
static inline void sunxi_i2c_soft_reset(struct sunxi_i2c *i2c)
{
	u32 reg_val, reg, mask;

	if (i2c->twi_drv_used) {
		reg = I2C_DRV_CTRL;
		mask = SOFT_RESET;
	} else {
		reg = I2C_SRST;
		mask = I2C_SOFT_RST;
	}

	reg_val = readl(i2c->base_addr + reg);
	reg_val |= mask;
	writel(reg_val, i2c->base_addr + reg);

	/*
	 * drv-mode soft_reset bit will not clear automatically, write 0 to unreset.
	 * The reset only takes one or two CPU clk cycle.
	 */
	if (i2c->twi_drv_used) {
		usleep_range(20, 25);

		reg_val &= (~mask);
		writel(reg_val, i2c->base_addr + reg);
	}
}

/* iset the data byte number follow read command control */
static inline void sunxi_i2c_engine_set_efr(void __iomem *base_addr, u32 efr)
{
	u32 reg_val;

	reg_val = readl(base_addr + I2C_EFR);

	reg_val &= ~I2C_EFR_MASK;
	efr &= I2C_EFR_MASK;
	reg_val |= efr;

	writel(reg_val, base_addr + I2C_EFR);
}

static int sunxi_i2c_engine_start(struct sunxi_i2c *i2c)
{
	unsigned int timeout = 0xff;

	sunxi_i2c_engine_set_start(i2c->base_addr);
	while ((sunxi_i2c_engine_get_start(i2c->base_addr) == 1) && (--timeout))
		;
	if (timeout == 0) {
		dev_err(i2c->dev, "engine-mode: START can't sendout!\n");
		return -EINVAL;
	}

	dev_dbg(i2c->dev, "engine-mode: start signal xfered\n");
	return 0;
}

static int sunxi_i2c_engine_restart(struct sunxi_i2c *i2c)
{
	unsigned int timeout = 0xff;

	sunxi_i2c_engine_set_start(i2c->base_addr);

	while ((sunxi_i2c_engine_get_start(i2c->base_addr) == 1) && (--timeout))
		;
	if (timeout == 0) {
		dev_err(i2c->dev, "engine-mode: Restart can't sendout!\n");
		return -EINVAL;
	}

	dev_dbg(i2c->dev, "engine-mode: restart signal xfered\n");
	return 0;
}

static int sunxi_i2c_engine_stop(struct sunxi_i2c *i2c)
{
	unsigned int timeout = 0xff;
	void __iomem *base_addr = i2c->base_addr;

	sunxi_i2c_engine_set_stop(base_addr);
	/* i2c bus xfer status will chaned after irq flag cleared */
	sunxi_i2c_engine_clear_irq(base_addr);

	/* it must delay 1 nop to check stop bit */
	sunxi_i2c_engine_get_stop(base_addr);
	while ((sunxi_i2c_engine_get_stop(base_addr) == 1) && (--timeout))
		;
	if (timeout == 0) {
		dev_err(i2c->dev, "engine-mode: STOP can't sendout!\n");
		return -EINVAL;
	}

	timeout = 0xff;
	while ((sunxi_i2c_get_xfer_sta(i2c) != I2C_STAT_IDLE) && (--timeout))
		;
	if (timeout == 0) {
		dev_err(i2c->dev, "engine-mode: bus state: 0x%0x, isn't idle\n",
			sunxi_i2c_get_xfer_sta(i2c));
		return -EINVAL;
	}

	dev_dbg(i2c->dev, "engine-mode: stop signal xfered");
	return 0;
}

static void sunxi_i2c_scl_control_enable(struct sunxi_i2c *i2c)
{
	u32 reg_val, reg_ctrl, reg;

	if (i2c->twi_drv_used) {
		reg = I2C_DRV_BUS_CTRL;
		reg_ctrl = SCL_MOE;
	} else {
		reg = I2C_LCR;
		reg_ctrl = SCL_CTL_EN;
	}

	reg_val = readl(i2c->base_addr + reg);

	reg_val |= reg_ctrl;

	writel(reg_val, i2c->base_addr + reg);
}

static void sunxi_i2c_scl_control_disable(struct sunxi_i2c *i2c)
{
	u32 reg_val, reg_ctrl, reg;

	if (i2c->twi_drv_used) {
		reg = I2C_DRV_BUS_CTRL;
		reg_ctrl = SCL_MOE;
	} else {
		reg = I2C_LCR;
		reg_ctrl = SCL_CTL_EN;
	}

	reg_val = readl(i2c->base_addr + reg);

	reg_val &= ~(reg_ctrl);

	writel(reg_val, i2c->base_addr + reg);
}

static int sunxi_i2c_get_scl(struct i2c_adapter *adap)
{
	struct sunxi_i2c *i2c = (struct sunxi_i2c *)adap->algo_data;

	if (i2c->twi_drv_used)
		return !!(readl(i2c->base_addr + I2C_DRV_BUS_CTRL) & SCL_STA);
	else
		return !!(readl(i2c->base_addr + I2C_LCR) & SCL_STATE);
}

static void sunxi_i2c_set_scl(struct i2c_adapter *adap, int val)
{
	struct sunxi_i2c *i2c = (struct sunxi_i2c *)adap->algo_data;
	u32 reg_val, status, reg;

	sunxi_i2c_scl_control_enable(i2c);

	if (i2c->twi_drv_used) {
		reg = I2C_DRV_BUS_CTRL;
		status = SCL_MOV;
	} else {
		reg = I2C_LCR;
		status = SCL_CTL;
	}

	reg_val = readl(i2c->base_addr + reg);
	dev_dbg(i2c->dev, "set scl, val:%x, val:%d\n", reg_val, val);

	if (val)
		reg_val |= status;
	else
		reg_val &= ~(status);

	writel(reg_val, i2c->base_addr + reg);

	sunxi_i2c_scl_control_disable(i2c);
}

static int sunxi_i2c_get_sda(struct i2c_adapter *adap)
{
	struct sunxi_i2c *i2c = (struct sunxi_i2c *)adap->algo_data;

	if (i2c->twi_drv_used)
		return !!(readl(i2c->base_addr + I2C_DRV_BUS_CTRL) & SDA_STA);
	else
		return !!(readl(i2c->base_addr + I2C_LCR) & SDA_STATE);
}

static int sunxi_i2c_get_bus_free(struct i2c_adapter *adap)
{
	return sunxi_i2c_get_sda(adap);
}

/* get the irq enabled */
static unsigned int sunxi_i2c_drv_get_irq(void __iomem *base_addr)
{
	unsigned int sta, irq;

	irq = (readl(base_addr + I2C_DRV_INT_CTRL) & I2C_DRV_INT_EN_MASK) >> 16;
	sta = readl(base_addr + I2C_DRV_INT_CTRL) & I2C_DRV_INT_STA_MASK;

	return irq & sta;
}

static void sunxi_i2c_drv_clear_irq(void __iomem *base_addr, u32 bitmap)
{
	unsigned int reg_val = readl(base_addr + I2C_DRV_INT_CTRL);

	reg_val |= (bitmap & I2C_DRV_INT_STA_MASK);

	writel(reg_val, base_addr + I2C_DRV_INT_CTRL);
}

static void sunxi_i2c_drv_enable_irq(void __iomem *base_addr, u32 bitmap)
{
	u32 reg_val = readl(base_addr + I2C_DRV_INT_CTRL);

	reg_val |= bitmap;
	reg_val &= ~I2C_DRV_INT_STA_MASK;

	writel(reg_val, base_addr + I2C_DRV_INT_CTRL);
}

static void sunxi_i2c_drv_disable_irq(void __iomem *base_addr, u32 bitmap)
{
	u32 reg_val = readl(base_addr + I2C_DRV_INT_CTRL);

	reg_val &= ~bitmap;
	reg_val &= ~I2C_DRV_INT_STA_MASK;

	writel(reg_val, base_addr + I2C_DRV_INT_CTRL);
}

static void sunxi_i2c_drv_enable_dma_irq(void __iomem *base_addr, u32 bitmap)
{
	u32 reg_val = readl(base_addr + I2C_DRV_DMA_CFG);

	bitmap &= I2C_DRQEN_MASK;
	reg_val |= bitmap;

	writel(reg_val, base_addr + I2C_DRV_DMA_CFG);
}

static void sunxi_i2c_drv_disable_dma_irq(void __iomem *base_addr, u32 bitmap)
{
	u32 reg_val = readl(base_addr + I2C_DRV_DMA_CFG);

	bitmap &= I2C_DRQEN_MASK;
	reg_val &= ~bitmap;

	writel(reg_val, base_addr + I2C_DRV_DMA_CFG);
}

/*
 * The engine-mode bus_en bit is automatically set to 1 after the drv-mode START_TRAN bit set to 1.
 * (because drv-mode rely on engine-mode)
 */
static inline void sunxi_i2c_drv_start_xfer(struct sunxi_i2c *i2c)
{
	u32 reg_val = readl(i2c->base_addr + I2C_DRV_CTRL);

	reg_val |= START_TRAN;

	writel(reg_val, i2c->base_addr + I2C_DRV_CTRL);

	dev_dbg(i2c->dev, "drv-mode: start signal xfered\n");
}

static void sunxi_i2c_drv_set_tx_trig(void __iomem *base_addr, u32 trig)
{
	u32 reg_val = readl(base_addr + I2C_DRV_DMA_CFG);

	reg_val &= ~TX_TRIG;
	reg_val |= (trig << TX_TRIG_OFFSET);

	writel(reg_val, base_addr + I2C_DRV_DMA_CFG);
}

/* When one of the following conditions is met:
 * 1. The number of data (in bytes) in RECV_FIFO reaches RX_TRIG;
 * 2. Packet read done and RECV_FIFO not empty.
 * If RX_REQ is enabled,  the rx-pending-bit will be set to 1 and the interrupt will be triggered;
 * If RX_REQ is disabled, the rx-pending-bit will be set to 1 but the interrupt will NOT be triggered.
 */
static void sunxi_i2c_drv_set_rx_trig(void __iomem *base_addr, u32 trig)
{
	u32 reg_val;

	reg_val = readl(base_addr + I2C_DRV_DMA_CFG);

	reg_val &= ~RX_TRIG;
	reg_val |= (trig << RX_TRIG_OFFSET);

	writel(reg_val, base_addr + I2C_DRV_DMA_CFG);
}


/* bytes be send as slave device reg address */
static void sunxi_i2c_drv_set_addr_byte(void __iomem *base_addr, u32 len)
{
	u32 reg_val = readl(base_addr + I2C_DRV_FMT);

	reg_val &= ~ADDR_BYTE;
	reg_val |= (len << ADDR_BYTE_OFFSET);

	writel(reg_val, base_addr + I2C_DRV_FMT);
}

/* bytes be send/received as data */
static void sunxi_i2c_drv_set_data_byte(void __iomem *base_addr, u32 len)
{
	u32 val = readl(base_addr + I2C_DRV_FMT);

	val &= ~DATA_BYTE;
	val |= (len << DATA_BYTE_OFFSET);

	writel(val, base_addr + I2C_DRV_FMT);
}

/* interval between each packet in 32*Fscl cycles */
/*
static void sunxi_i2c_drv_set_packet_interval(void __iomem *base_addr, u32 val)
{
	u32 reg_val = readl(base_addr + I2C_DRV_CFG);

	reg_val &= ~PACKET_INTERVAL;
	reg_val |= (val << PACKET_INTERVAL_OFFSET);

	writel(reg_val, base_addr + I2C_DRV_CFG);
}
*/

/* FIFO data be transmitted as PACKET_CNT packets in current format */
static void sunxi_i2c_drv_set_packet_cnt(void __iomem *base_addr, u32 val)
{
	u32 reg_val = readl(base_addr + I2C_DRV_CFG);

	reg_val &= ~PACKET_CNT;
	reg_val |= (val << PACKET_CNT_OFFSET);

	writel(reg_val, base_addr + I2C_DRV_CFG);
}

/* do not send slave_id +W */
static void sunxi_i2c_drv_enable_read_mode(void __iomem *base_addr)
{
	u32 reg_val = readl(base_addr + I2C_DRV_CTRL);

	reg_val |= READ_TRAN_MODE;

	writel(reg_val, base_addr + I2C_DRV_CTRL);
}

/* send slave_id + W */
static void sunxi_i2c_drv_disable_read_mode(void __iomem *base_addr)
{
	u32 reg_val = readl(base_addr + I2C_DRV_CTRL);

	reg_val &= ~READ_TRAN_MODE;

	writel(reg_val, base_addr + I2C_DRV_CTRL);
}

static void
sunxi_i2c_drv_set_slave_addr(struct sunxi_i2c *i2c, struct i2c_msg *msgs)
{
	unsigned int reg_val = 0;

	/* read, default value is write */
	if (msgs->flags & I2C_M_RD)
		reg_val |= CMD;
	else
		reg_val &= ~CMD;

	if (msgs->flags & I2C_M_TEN) {
		/* SLV_ID | CMD | SLV_ID_X */
		reg_val |= ((0x78 | ((msgs->addr >> 8) & 0x03)) << 9);
		reg_val |= (msgs->addr & 0xff);
		dev_dbg(i2c->dev, "drv-mode: first 10bit(0x%x) xfered\n", msgs->addr);
	} else {
		reg_val |= ((msgs->addr & 0x7f) << 9);
		dev_dbg(i2c->dev, "drv-mode: 7bits(0x%x) + r/w xfered", msgs->addr);
	}

	writel(reg_val, i2c->base_addr + I2C_DRV_SLV);
}

/* the number of data in SEND_FIFO */
static int sunxi_i2c_drv_get_txfifo_cnt(void __iomem *base_addr)
{
	u32 reg_val = readl(base_addr + I2C_DRV_FIFO_CON) & SEND_FIFO_CONTENT;

	return reg_val >> SEND_FIFO_CONTENT_OFFSET;
}

/* the number of data in RECV_FIFO */
static int sunxi_i2c_drv_get_rxfifo_cnt(void __iomem *base_addr)
{
	u32 reg_val = readl(base_addr + I2C_DRV_FIFO_CON)& RECV_FIFO_CONTENT;

	return reg_val >> RECV_FIFO_CONTENT_OFFSET;
}

static void sunxi_i2c_drv_clear_txfifo(void __iomem *base_addr)
{
	u32 reg_val = readl(base_addr + I2C_DRV_FIFO_CON);

	reg_val |= SEND_FIFO_CLEAR;

	writel(reg_val, base_addr + I2C_DRV_FIFO_CON);
}

static void sunxi_i2c_drv_clear_rxfifo(void __iomem *base_addr)
{
	u32 reg_val = readl(base_addr + I2C_DRV_FIFO_CON);

	reg_val |= RECV_FIFO_CLEAR;

	writel(reg_val, base_addr + I2C_DRV_FIFO_CON);
}

static int sunxi_i2c_drv_send_msg(struct sunxi_i2c *i2c, struct i2c_msg *msg)
{
	u16 i;
	u8 time = 0xff;

	dev_dbg(i2c->dev, "drv-mode: tx msg len is %d\n", msg->len);

	for (i = 0; i < msg->len; i++) {
		while ((sunxi_i2c_drv_get_txfifo_cnt(i2c->base_addr) >= MAX_FIFO)
				&& time--)
			;
		if (time) {
			writeb(msg->buf[i], i2c->base_addr +  I2C_DRV_SEND_FIFO_ACC);
			dev_dbg(i2c->dev, "drv-mode: write Byte[%u]=0x%x,tx fifo len=%d\n",
				i, msg->buf[i], sunxi_i2c_drv_get_txfifo_cnt(i2c->base_addr));
		} else {
			dev_err(i2c->dev, "drv-mode: SEND FIFO overflow, timeout\n");
			return -EINVAL;
		}
	}

	return 0;
}

static u32 sunxi_i2c_drv_recv_msg(struct sunxi_i2c *i2c, struct i2c_msg *msg)
{
	u16 i;
	u8 time = 0xff;

	dev_dbg(i2c->dev, "drv-mode: rx msg len is %d\n", msg->len);

	for (i = 0; i < msg->len; i++) {
		while (!sunxi_i2c_drv_get_rxfifo_cnt(i2c->base_addr) && time--)
			;

		if (time) {
			msg->buf[i] = readb(i2c->base_addr + I2C_DRV_RECV_FIFO_ACC);
			dev_dbg(i2c->dev, "drv-mode: readb: Byte[%d] = 0x%x\n",
					i, msg->buf[i]);
		} else {
			dev_err(i2c->dev, "drv-mode: rerceive fifo empty. timeout\n");
			return -EINVAL;
		}
	}

	return 0;
}

/* Functions for DMA support */
static int sunxi_i2c_dma_request(struct sunxi_i2c *i2c, dma_addr_t phy_addr)
{
	struct sunxi_i2c_dma *dma_tx, *dma_rx;
	dma_cap_mask_t mask_tx, mask_rx;
	struct dma_slave_config dma_sconfig;
	int err;

	dma_tx = devm_kzalloc(i2c->dev, sizeof(*dma_tx), GFP_KERNEL);
	dma_rx = devm_kzalloc(i2c->dev, sizeof(*dma_rx), GFP_KERNEL);
	if (IS_ERR_OR_NULL(dma_tx) || IS_ERR_OR_NULL(dma_rx)) {
		dev_err(i2c->dev, "dma kzalloc failed\n");
		return -EINVAL;
	}

	dma_cap_zero(mask_tx);
	dma_cap_set(DMA_SLAVE, mask_tx);
	dma_tx->chan = dma_request_channel(mask_tx, NULL, NULL);
	if (IS_ERR(dma_tx->chan)) {
		dev_err(i2c->dev, "can't request DMA tx channel\n");
		err = PTR_ERR(dma_tx->chan);
		goto err0;
	}

	dma_sconfig.dst_addr = phy_addr + I2C_DRV_SEND_FIFO_ACC;
	dma_sconfig.src_addr_width = DMA_SLAVE_BUSWIDTH_1_BYTE;
	dma_sconfig.dst_addr_width = DMA_SLAVE_BUSWIDTH_1_BYTE;
	dma_sconfig.src_maxburst = 16;
	dma_sconfig.dst_maxburst = 16;
	dma_sconfig.direction = DMA_MEM_TO_DEV;

#ifndef DRQDST_TWI0_TX
	dev_err(i2c->dev, "[i2c%d] can't susport DMA for TX\n", i2c->bus_num);
#else
	dma_sconfig.slave_id = sunxi_slave_id(DRQDST_TWI0_TX + i2c->bus_num,
			DRQSRC_SDRAM);
#endif
	err = dmaengine_slave_config(dma_tx->chan, &dma_sconfig);
	if (err < 0) {
		dev_err(i2c->dev, "can't configure tx channel\n");
		goto err1;
	}
	i2c->dma_tx = dma_tx;

	dma_cap_zero(mask_rx);
	dma_cap_set(DMA_SLAVE, mask_rx);
	dma_rx->chan = dma_request_channel(mask_rx, NULL, NULL);
	if (IS_ERR(dma_rx->chan)) {
		dev_err(i2c->dev, "can't request DMA rx channel\n");
		goto err1;
	}
	dma_sconfig.src_addr = phy_addr + I2C_DRV_RECV_FIFO_ACC;
	dma_sconfig.src_addr_width = DMA_SLAVE_BUSWIDTH_1_BYTE;
	dma_sconfig.dst_addr_width = DMA_SLAVE_BUSWIDTH_1_BYTE;
	dma_sconfig.src_maxburst = 16;
	dma_sconfig.dst_maxburst = 16;
	dma_sconfig.direction = DMA_DEV_TO_MEM;

#ifndef DRQSRC_TWI0_RX
	dev_err(i2c->dev, "[i2c%d] can't susport DMA for RX\n", i2c->bus_num);
#else
	dma_sconfig.slave_id = sunxi_slave_id(DRQSRC_SDRAM,
			DRQSRC_TWI0_RX + i2c->bus_num);
#endif
	err = dmaengine_slave_config(dma_rx->chan, &dma_sconfig);
	if (err < 0) {
		dev_err(i2c->dev, "can't configure rx channel\n");
		goto err2;
	}
	i2c->dma_rx = dma_rx;

	init_completion(&i2c->cmd_complete);
	dev_dbg(i2c->dev, "using %s (tx) and %s (rx) for DMA transfers\n",
		dma_chan_name(i2c->dma_tx->chan), dma_chan_name(i2c->dma_rx->chan));

	return 0;

err2:
	dma_release_channel(i2c->dma_rx->chan);

err1:
	dma_release_channel(i2c->dma_tx->chan);

err0:
	return err;
}

static void sunxi_i2c_dma_release(struct sunxi_i2c *i2c)
{
	if (i2c->dma_tx) {
		i2c->dma_tx->dma_buf = 0;
		i2c->dma_tx->dma_len = 0;
		dma_release_channel(i2c->dma_tx->chan);
		i2c->dma_tx->chan = NULL;
	}

	if (i2c->dma_rx) {
		i2c->dma_rx->dma_buf = 0;
		i2c->dma_rx->dma_len = 0;
		dma_release_channel(i2c->dma_rx->chan);
		i2c->dma_rx->chan = NULL;
	}
}

static void sunxi_i2c_dma_callback(void *arg)
{
	struct sunxi_i2c *i2c = (struct sunxi_i2c *)arg;

	if (i2c->dma_using == i2c->dma_tx)
		dev_err(i2c->dev, "drv-mode: dma write data end\n");
	else if (i2c->dma_using == i2c->dma_rx)
		dev_err(i2c->dev, "drv-mode: dma read data end\n");

	complete(&i2c->cmd_complete);
}

/* make preparetions for dma transfer
 * then wait tx fifo level trig unitl dma callback
 */
static int sunxi_i2c_drv_dma_xfer_init(struct sunxi_i2c *i2c, bool read)
{
	unsigned long time_left;
	struct sunxi_i2c_dma *dma;
	struct device *chan_dev;
	struct dma_async_tx_descriptor *dma_desc;
	int ret;

	if (read) {
		i2c->dma_using = i2c->dma_rx;
		i2c->dma_using->dma_transfer_dir = DMA_DEV_TO_MEM;
		i2c->dma_using->dma_data_dir = DMA_FROM_DEVICE;
		i2c->dma_using->dma_len = i2c->msg->len;
	} else {
		i2c->dma_using = i2c->dma_tx;
		i2c->dma_using->dma_transfer_dir = DMA_MEM_TO_DEV;
		i2c->dma_using->dma_data_dir = DMA_TO_DEVICE;
		i2c->dma_using->dma_len = i2c->msg->len;
	}
	dma = i2c->dma_using;
	chan_dev = dma->chan->device->dev;

	dma->dma_buf = dma_map_single(chan_dev, i2c->msg->buf,
					dma->dma_len, dma->dma_data_dir);
	if (dma_mapping_error(chan_dev, dma->dma_buf)) {
		dev_err(i2c->dev, "DMA mapping failed\n");
		ret = -EINVAL;
		goto err0;
	}
	dma_desc = dmaengine_prep_slave_single(dma->chan, dma->dma_buf,
					       dma->dma_len, dma->dma_transfer_dir,
					       DMA_PREP_INTERRUPT | DMA_CTRL_ACK);
	if (!dma_desc) {
		dev_err(i2c->dev, "Not able to get desc for DMA xfer\n");
		ret = -EINVAL;
		goto err0;
	}
	dma_desc->callback = sunxi_i2c_dma_callback;
	dma_desc->callback_param = i2c;
	if (dma_submit_error(dmaengine_submit(dma_desc))) {
		dev_err(i2c->dev, "DMA submit failed\n");
		ret = -EINVAL;
		goto err0;
	}

	reinit_completion(&i2c->cmd_complete);
	dma_async_issue_pending(dma->chan);
	dev_dbg(i2c->dev, "dma issue pending\n");

	time_left = wait_for_completion_timeout(
						&i2c->cmd_complete,
						msecs_to_jiffies(DMA_TIMEOUT));
	dev_dbg(i2c->dev, "[i2c%d] time_left = %lu\n", i2c->bus_num, time_left);

	return 0;

err0:
	return ret;
}

static int sunxi_i2c_drv_dma_xfer_deinit(struct sunxi_i2c *i2c)
{
	struct sunxi_i2c_dma *dma = i2c->dma_using;
	struct device *chan_dev = dma->chan->device->dev;

	dma_unmap_single(chan_dev, dma->dma_buf, dma->dma_len, dma->dma_data_dir);

	return 0;
}


/* Description:
 *	7bits addr: 7-1bits addr+0 bit r/w
 *	10bits addr: 1111_11xx_xxxx_xxxx-->1111_0xx_rw,xxxx_xxxx
 *	send the 7 bits addr,or the first part of 10 bits addr
 **/
static void sunxi_i2c_engine_addr_byte(struct sunxi_i2c *i2c)
{
	unsigned char addr = 0;
	unsigned char tmp  = 0;

	if (i2c->msg[i2c->msg_idx].flags & I2C_M_TEN) {
		/* 0111_10xx,ten bits address--9:8bits */
		tmp = 0x78 | (((i2c->msg[i2c->msg_idx].addr)>>8) & 0x03);
		addr = tmp << 1;	/*1111_0xx0*/
		/* how about the second part of ten bits addr? */
		/* Answer: deal at twi_core_process() */
	} else {
		/* 7-1bits addr, xxxx_xxx0 */
		addr = (i2c->msg[i2c->msg_idx].addr & 0x7f) << 1;
	}

	/* read, default value is write */
	if (i2c->msg[i2c->msg_idx].flags & I2C_M_RD)
		addr |= 1;

	if (i2c->msg[i2c->msg_idx].flags & I2C_M_TEN)
		dev_dbg(i2c->dev, "first part of 10bits = 0x%x\n", addr);
	else
		dev_dbg(i2c->dev, "engine-mode: 7bits+r/w = 0x%x xfered\n", addr);

	/* send 7bits+r/w or the first part of 10bits */
	sunxi_i2c_engine_put_byte(i2c, &addr);
}

static int sunxi_i2c_bus_barrier(struct i2c_adapter *adap)
{
	int i, ret;
	struct sunxi_i2c *i2c = (struct sunxi_i2c *)adap->algo_data;

	for (i = 0; i < LOOP_TIMEOUT; i++) {
		if (sunxi_i2c_get_bus_free(adap))
			return 0;

		udelay(1);
	}

	ret = i2c_recover_bus(adap);
	sunxi_i2c_soft_reset(i2c);

	return ret;
}

/* return 0 on success, otherwise return the negative error num */
static int sunxi_i2c_drv_complete(struct sunxi_i2c *i2c)
{
	unsigned long timeout;
	int ret = 0;

	timeout = wait_event_timeout(i2c->wait, i2c->result, i2c->adap.timeout);
	if (timeout == 0) {
		dev_err(i2c->dev, "drv-mode: xfer timeout (dev addr:0x%x)\n",
				i2c->msg->addr);
		sunxi_i2c_dump_reg(i2c, 0x200, 0x20);
		ret = -ETIME;
	} else {
		if (i2c->result == SUNXI_I2C_XFER_RESULT_ERROR) {
			dev_err(i2c->dev, "drv-mode: xfer failed (dev addr:0x%x)\n",
					i2c->msg->addr);
			sunxi_i2c_dump_reg(i2c, 0x200, 0x20);
			ret = -EINVAL;
		} else if (i2c->result == SUNXI_I2C_XFER_RESULT_COMPLETE) {
			dev_dbg(i2c->dev, "drv-mode: xfer complete\n");
		} else {
			dev_err(i2c->dev, "drv-mode: result err\n");
			ret = -EINVAL;
		}
	}

	return ret;
}

/* return the xfer msgs num or the negative error num */
static int sunxi_i2c_engine_complete(struct sunxi_i2c *i2c)
{
	unsigned long timeout;
	int ret;

	timeout = wait_event_timeout(i2c->wait, i2c->result, i2c->adap.timeout);
	if (timeout == 0) {
		dev_err(i2c->dev, "engine-mode: xfer timeout(dev addr:0x%x)\n",
				i2c->msg->addr);
		sunxi_i2c_dump_reg(i2c, 0x00, 0x20);
		ret = -ETIME;
	} else {
		if (i2c->result == SUNXI_I2C_XFER_RESULT_ERROR) {
			dev_err(i2c->dev, "engine-mode: xfer failed(dev addr:0x%x)\n",
					i2c->msg->addr);
			sunxi_i2c_dump_reg(i2c, 0x00, 0x20);
			ret = -EINVAL;
		} else if (i2c->result == SUNXI_I2C_XFER_RESULT_COMPLETE) {
			if (i2c->msg_idx != i2c->msg_num) {
				dev_err(i2c->dev, "engine-mode: xfer incomplete(dev addr:0x%x\n",
					i2c->msg->addr);
				ret = -EINVAL;
			} else {
				dev_dbg(i2c->dev, "engine-mode: xfer complete\n");
				ret = i2c->msg_idx;
			}
		} else {
			dev_err(i2c->dev, "engine-mode: result err\n");
			ret = -EINVAL;
		}
	}

	return ret;
}

static int sunxi_i2c_drv_core_process(struct sunxi_i2c *i2c)
{
	void __iomem *base_addr = i2c->base_addr;
	unsigned long flags;
	unsigned int irq, err_sta;

	spin_lock_irqsave(&i2c->lock, flags);
	irq = sunxi_i2c_drv_get_irq(base_addr);
	dev_dbg(i2c->dev, "drv-mode: the enabled irq 0x%x coming\n", irq);
	sunxi_i2c_drv_clear_irq(i2c->base_addr, irq);

	if (irq & TRAN_COM_PD) {
		dev_dbg(i2c->dev, "drv-mode: packet transmission completed\n");
		sunxi_i2c_drv_disable_irq(i2c->base_addr, I2C_DRV_INT_EN_MASK);
		i2c->result = SUNXI_I2C_XFER_RESULT_COMPLETE;

		/* cpu read tiggered by the interrupt tiggered
		 * dma read tiggered by the fifo level(dma_callback when success)
		 * */
		if ((irq & RX_REQ_PD) && (i2c->msg->len < DMA_THRESHOLD))
			if (sunxi_i2c_drv_recv_msg(i2c, i2c->msg))
				i2c->result = SUNXI_I2C_XFER_RESULT_ERROR;
		spin_unlock_irqrestore(&i2c->lock, flags);
		wake_up(&i2c->wait);
		return 0;
	} else if (irq & TRAN_ERR_PD) {
		sunxi_i2c_drv_disable_irq(i2c->base_addr, I2C_DRV_INT_EN_MASK);

		/* The correct way to get the bus status in drv-mode is:
		 * err_sta = sunxi_i2c_get_xfer_sta(i2c)
		 * however, for the current TWI controller, when using drv-mode,
		 * the bus status is also obtained through the interrupt status register of engine-mode.
		 * therefore, we temporarily use the engine-mode interrupt status register
		 * to get the real bus status. this way is:
		 * err_sta = readl(i2c->base_addr + I2C_STAT) & I2C_STAT_MASK
		 */
#if 0
		err_sta = sunxi_i2c_get_xfer_sta(i2c);
#else
		err_sta = readl(i2c->base_addr + I2C_STAT) & I2C_STAT_MASK;
#endif
		switch (err_sta) {
		case 0x00:
			dev_err(i2c->dev, "drv-mode: bus error\n");
			break;
		case 0x01:
			dev_err(i2c->dev, "drv-mode: Timeout when sending 9th SCL clk\n");
			break;
		case 0x20:
			dev_err(i2c->dev, "drv-mode: Address + Write bit transmitted,"
					"ACK not received\n");
			break;
		case 0x30:
			dev_err(i2c->dev, "drv-mode: Data byte transmitted in master mode,"
				"ACK not received\n");
			break;
		case 0x38:
			dev_err(i2c->dev, "drv-mode: Arbitration lost in address, or data byte\n");
			break;
		case 0x48:
			dev_err(i2c->dev, "drv-mode: Address + Read bit transmitted,"
				"ACK not received\n");
			break;
		case 0x58:
			dev_err(i2c->dev, "drv-mode: Data byte received in master mode,"
				"ACK not received\n");
			break;
		default:
			dev_err(i2c->dev, "drv-mode: unknown error\n");
			break;
		}

		dev_err(i2c->dev, "drv mode: I2C BUS error state is 0x%x\n", err_sta);
		i2c->msg_idx = err_sta;
		i2c->result = SUNXI_I2C_XFER_RESULT_ERROR;
		spin_unlock_irqrestore(&i2c->lock, flags);
		wake_up(&i2c->wait);
		return -err_sta;
	}

	spin_unlock_irqrestore(&i2c->lock, flags);
	return -ENXIO;
}


static int sunxi_i2c_engine_core_process(struct sunxi_i2c *i2c)
{
	unsigned long flags;
	unsigned char state;
	unsigned char tmp;
	void __iomem *base_addr = i2c->base_addr;

	sunxi_i2c_engine_disable_irq(base_addr);
	state = sunxi_i2c_get_xfer_sta(i2c);
	dev_dbg(i2c->dev, "engine-mode: [slave address:(0x%x),irq state:(0x%x)]\n",
		i2c->msg->addr, state);

	spin_lock_irqsave(&i2c->lock, flags);
	switch (state) {
	case 0xf8: /* On reset or stop the bus is idle, use only at poll method */
		goto out_break;
	case 0x08: /* A START condition has been transmitted */
	case 0x10: /* A repeated start condition has been transmitted */
		sunxi_i2c_engine_addr_byte(i2c);/* send slave address */
		break; /* break and goto to normal function, */
	case 0x18: /* slave_addr + write has been transmitted; ACK received */
		/* send second part of 10 bits addr, the remaining 8 bits of address */
		if (i2c->msg[i2c->msg_idx].flags & I2C_M_TEN) {
			tmp = i2c->msg[i2c->msg_idx].addr & 0xff;
			sunxi_i2c_engine_put_byte(i2c, &tmp);
			break;
		}
		/* for 7 bit addr, then directly send data byte--case 0xd0:  */
		/* fall through */
	case 0xd0: /* SLA+W has transmitted,ACK received! */
	case 0x28: /* then continue send data or current xfer end */
		/* send register address and the write data  */
		if (i2c->buf_idx < i2c->msg[i2c->msg_idx].len) {
			sunxi_i2c_engine_put_byte(i2c,
					&(i2c->msg[i2c->msg_idx].buf[i2c->buf_idx]));
			i2c->buf_idx++;
		} else { /* the other msg */
			i2c->msg_idx++;
			i2c->buf_idx = 0;

			/* all the write msgs xfer success, then wakeup */
			if (i2c->msg_idx == i2c->msg_num) {
				goto out_success;
			} else if (i2c->msg_idx < i2c->msg_num) {
				/* for restart pattern, read spec, two msgs */
				if (sunxi_i2c_engine_restart(i2c)) {
					dev_err(i2c->dev, "when the %d msg xfering, start failed",
							i2c->msg_idx);
					goto out_failed;
				}
			} else {
				goto out_failed;
			}
		}
		break;
	/* SLA+R has been transmitted; ACK has been received, is ready to receive
	 * with Restart, needn't to send second part of 10 bits addr
	 */
	case 0x40:
		/* refer-"I2C-SPEC v2.1" */
		/* enable A_ACK need it(receive data len) more than 1. */
		if (i2c->msg[i2c->msg_idx].len > 1)
			sunxi_i2c_engine_enable_ack(base_addr);/* then jump to case 0x50 */
		break;
	case 0x50: /* Data bytes has been received; ACK has been transmitted */
		/* receive first data byte */
		if (i2c->buf_idx < i2c->msg[i2c->msg_idx].len) {
			/* more than 2 bytes, the last byte need not to send ACK */
			if ((i2c->buf_idx + 2) == i2c->msg[i2c->msg_idx].len)
				sunxi_i2c_engine_disable_ack(base_addr);

			/* get data then clear flag,then next data coming */
			sunxi_i2c_engine_get_byte(base_addr,
					&i2c->msg[i2c->msg_idx].buf[i2c->buf_idx]);
			i2c->buf_idx++;
			break;
		} else { /* err process, the last byte should be @case 0x58 */
			goto out_failed;
		}
	case 0x58: /* Data byte has been received; NOT ACK has been transmitted */
		/* received the last byte  */
		if (i2c->buf_idx == (i2c->msg[i2c->msg_idx].len - 1)) {
			sunxi_i2c_engine_get_byte(base_addr,
					&i2c->msg[i2c->msg_idx].buf[i2c->buf_idx]);
			i2c->msg_idx++;
			i2c->buf_idx = 0;

			/* all the read mags xfer succeed,wakeup the thread */
			if (i2c->msg_idx == i2c->msg_num) {
				goto out_success;
			} else if (i2c->msg_idx < i2c->msg_num) { /* repeat start */
				if (sunxi_i2c_engine_restart(i2c)) {
					dev_err(i2c->dev, "when the %d msg xfering, start failed",
							i2c->msg_idx);
					goto out_failed;
				}
				break;
			} else {
				goto out_failed;
			}
		} else {
			goto out_failed;
		}
	case 0xd8:
		dev_err(i2c->dev, "second addr has transmitted, ACK not received!");
		goto out_failed;
	case 0x20:
		dev_err(i2c->dev, "SLA+W has been transmitted; ACK not received\n");
		goto out_failed;
	case 0x30:
		dev_err(i2c->dev, "DATA byte transmitted, ACK not receive\n");
		goto out_failed;
	case 0x38:
		dev_err(i2c->dev, "Arbitration lost in SLA+W, SLA+R or data bytes\n");
		goto out_failed;
	case 0x48:
		dev_err(i2c->dev, "Address + Read bit transmitted, ACK not received\n");
		goto out_failed;
	case 0x00:
		dev_err(i2c->dev, "Bus error\n");
		goto out_failed;
	default:
		goto out_failed;
	}

	/* just for debug */
	i2c->debug_state = state;

/* this time xfer is't completed,return and enable irq
 * then wait new interrupt coming to continue xfer
 */
out_break:
	sunxi_i2c_engine_clear_irq(base_addr);
	sunxi_i2c_engine_enable_irq(base_addr);
	spin_unlock_irqrestore(&i2c->lock, flags);
	return 0;

/* xfer failed, then send stop and wakeup */
out_failed:
	dev_err(i2c->dev, "engine mode: I2C BUS error state is 0x%x\n", state);
	if (sunxi_i2c_engine_stop(i2c))
		dev_err(i2c->dev, "STOP failed!\n");

	i2c->msg_idx = -state;
	i2c->result = SUNXI_I2C_XFER_RESULT_ERROR;
	i2c->debug_state = state;
	spin_unlock_irqrestore(&i2c->lock, flags);
	wake_up(&i2c->wait);
	return -state;

/* xfer success, then send wtop and wakeup */
out_success:
	if (sunxi_i2c_engine_stop(i2c))
		dev_err(i2c->dev, "STOP failed!\n");

	i2c->result = SUNXI_I2C_XFER_RESULT_COMPLETE;
	i2c->debug_state = state;
	spin_unlock_irqrestore(&i2c->lock, flags);
	wake_up(&i2c->wait);
	return 0;
}

static irqreturn_t sunxi_i2c_handler(int this_irq, void *dev_id)
{
	struct sunxi_i2c *i2c = (struct sunxi_i2c *)dev_id;
	u32 ret = sunxi_i2c_check_irq(i2c);

	if (ret == 0) {
		sunxi_i2c_engine_core_process(i2c);
	} else if (ret == 1) {
		sunxi_i2c_drv_core_process(i2c);
	} else {
		dev_err(i2c->dev, "wrong irq, check irq number!!\n");
		return IRQ_NONE;
	}

	return IRQ_HANDLED;
}


/* i2c controller tx xfer function
 * return the xfered msgs num on success,or the negative error num on failed
 */
static int sunxi_i2c_drv_tx_one_msg(struct sunxi_i2c *i2c, struct i2c_msg *msg)
{
	unsigned long flags;
	int ret;

	dev_dbg(i2c->dev, "drv-mode: one-msg write slave_addr=0x%x, data_len=0x%x\n",
				msg->addr, msg->len);

	spin_lock_irqsave(&i2c->lock, flags);
	i2c->msg = msg;
	i2c->buf_idx = 0;
	spin_unlock_irqrestore(&i2c->lock, flags);

	sunxi_i2c_drv_disable_read_mode(i2c->base_addr);
	sunxi_i2c_drv_set_slave_addr(i2c, msg);
	if (msg->len == 1) {
		sunxi_i2c_drv_set_addr_byte(i2c->base_addr, 0);
		sunxi_i2c_drv_set_data_byte(i2c->base_addr, msg->len);
	} else {
		sunxi_i2c_drv_set_addr_byte(i2c->base_addr, 1);
		sunxi_i2c_drv_set_data_byte(i2c->base_addr, msg->len - 1);
	}
	sunxi_i2c_drv_set_packet_cnt(i2c->base_addr, 1);
	sunxi_i2c_drv_enable_irq(i2c->base_addr, TRAN_ERR_INT_EN | TRAN_COM_INT_EN);

	if (i2c->dma_tx && (msg->len >= MAX_FIFO)) {
		dev_dbg(i2c->dev, "drv-mode: dma write\n");
		sunxi_i2c_drv_set_tx_trig(i2c->base_addr, MAX_FIFO/2);
		sunxi_i2c_drv_enable_dma_irq(i2c->base_addr, DMA_TX_EN);

		ret = sunxi_i2c_drv_dma_xfer_init(i2c, I2C_WRITE);
		if (ret) {
			dev_err(i2c->dev, "dma tx xfer init failed\n");
			goto err_dma;
		}

		sunxi_i2c_drv_start_xfer(i2c);
	} else {
		dev_dbg(i2c->dev, "drv-mode: cpu write\n");
		sunxi_i2c_drv_set_tx_trig(i2c->base_addr, msg->len);
		 /*
		  * if now fifo can't store all the msg data buf
		  * enable the TX_REQ_INT_EN, and wait TX_REQ_INT,
		  * then continue send the remaining data to fifo
		  */
		if (sunxi_i2c_drv_send_msg(i2c, i2c->msg))
			sunxi_i2c_drv_enable_irq(i2c->base_addr, TX_REQ_INT_EN);

		sunxi_i2c_drv_start_xfer(i2c);
	}

	ret = sunxi_i2c_drv_complete(i2c);

	if (i2c->dma_tx && (msg->len > MAX_FIFO)) {
		sunxi_i2c_drv_dma_xfer_deinit(i2c);
		sunxi_i2c_drv_disable_dma_irq(i2c->base_addr, DMA_TX_EN | DMA_RX_EN);
	}
	sunxi_i2c_drv_disable_irq(i2c->base_addr, I2C_DRV_INT_EN_MASK);

	return ret;

err_dma:
	sunxi_i2c_drv_disable_dma_irq(i2c->base_addr, DMA_TX_EN | DMA_RX_EN);
	sunxi_i2c_drv_disable_irq(i2c->base_addr, I2C_DRV_INT_EN_MASK);

	return ret;
}

/* read xfer msg function
 * msg : the msg queue
 * num : the number of msg, usually is one or two:
 *
 * num = 1 : some i2c slave device support one msg to read, need't reg_addr
 *
 * num = 2 : normally i2c reead need two msg, msg example is as following :
 * msg[0]->flag express write, msg[0]->buf store the reg data,
 * msg[1]->flag express read, msg[1]-<buf store the data recived
 * return 0 on succes.
 */
static int
sunxi_i2c_drv_rx_msgs(struct sunxi_i2c *i2c, struct i2c_msg *msgs, int num)
{
	struct i2c_msg *wmsg, *rmsg;
	int ret;

	if (num == 1) {
		wmsg = NULL;
		rmsg = msgs;
		dev_dbg(i2c->dev, "drv-mode: one-msg read slave_addr=0x%x, data_len=0x%x\n",
				rmsg->addr, rmsg->len);
		sunxi_i2c_drv_enable_read_mode(i2c->base_addr);
		sunxi_i2c_drv_set_addr_byte(i2c->base_addr, 0);
	} else if (num == 2) {
		wmsg = msgs;
		rmsg = msgs + 1;
		dev_dbg(i2c->dev, "drv-mode: two-msg read slave_addr=0x%x, data_len=0x%x\n",
				rmsg->addr,  rmsg->len);
		if (wmsg->addr != rmsg->addr) {
			dev_err(i2c->dev, "drv-mode: two msg's addr must be the same\n");
			return -EINVAL;
		}
		sunxi_i2c_drv_disable_read_mode(i2c->base_addr);
		sunxi_i2c_drv_set_addr_byte(i2c->base_addr, wmsg->len);
	} else {
		dev_err(i2c->dev, "i2c read xfer can not transfer %d msgs once", num);
		return -EINVAL;
	}

	i2c->msg = rmsg;

	sunxi_i2c_drv_set_slave_addr(i2c, rmsg);
	sunxi_i2c_drv_set_packet_cnt(i2c->base_addr, 1);
	sunxi_i2c_drv_set_data_byte(i2c->base_addr, rmsg->len);
	sunxi_i2c_drv_enable_irq(i2c->base_addr, TRAN_ERR_INT_EN | TRAN_COM_INT_EN);

	if (wmsg)
		sunxi_i2c_drv_send_msg(i2c, wmsg);

	if (i2c->dma_rx && (rmsg->len > MAX_FIFO)) {
		dev_dbg(i2c->dev, "drv-mode: rx msgs by dma\n");
		sunxi_i2c_drv_set_rx_trig(i2c->base_addr, MAX_FIFO/2);
		sunxi_i2c_drv_enable_dma_irq(i2c->base_addr, DMA_RX_EN);

		ret = sunxi_i2c_drv_dma_xfer_init(i2c, I2C_READ);
		if (ret) {
			dev_err(i2c->dev, "dma rx xfer init failed\n");
			goto err_dma;
		}

		sunxi_i2c_drv_start_xfer(i2c);
	} else {
		dev_dbg(i2c->dev, "drv-mode: rx msgs by cpu\n");
		/* set the rx_trigger_level max to avoid the RX_REQ com before COM_REQ */
		sunxi_i2c_drv_set_rx_trig(i2c->base_addr, MAX_FIFO);
		sunxi_i2c_drv_enable_irq(i2c->base_addr, RX_REQ_INT_EN);
		sunxi_i2c_drv_start_xfer(i2c);
	}

	ret = sunxi_i2c_drv_complete(i2c);

	if (rmsg->len > MAX_FIFO) {
		sunxi_i2c_drv_dma_xfer_deinit(i2c);
		sunxi_i2c_drv_disable_dma_irq(i2c->base_addr, DMA_TX_EN | DMA_RX_EN);
	}
	sunxi_i2c_drv_disable_irq(i2c->base_addr, I2C_DRV_INT_EN_MASK);

	return ret;

err_dma:
	sunxi_i2c_drv_disable_dma_irq(i2c->base_addr, DMA_TX_EN | DMA_RX_EN);
	sunxi_i2c_drv_disable_irq(i2c->base_addr, I2C_DRV_INT_EN_MASK);

	return ret;
}

/**
 * @i2c: struct of sunxi_i2c
 * @msgs: One or more messages to execute before STOP is issued to terminate
 * the operation,and each message begins with a START.
 * @num: the Number of messages to be executed.
 *
 * return the number of xfered or the negative error num
 */
static int
sunxi_i2c_drv_xfer(struct sunxi_i2c *i2c, struct i2c_msg *msgs, int num)
{
	int i = 0;
	unsigned long flags;

	spin_lock_irqsave(&i2c->lock, flags);
	i2c->result = SUNXI_I2C_XFER_RESULT_RUNNING;  /* xfer means that the i2c result is in the running state */
	spin_unlock_irqrestore(&i2c->lock, flags);

	sunxi_i2c_drv_clear_irq(i2c->base_addr, I2C_DRV_INT_STA_MASK);
	sunxi_i2c_drv_disable_irq(i2c->base_addr, I2C_DRV_INT_STA_MASK);
	sunxi_i2c_drv_disable_dma_irq(i2c->base_addr, DMA_TX_EN | DMA_RX_EN);

	sunxi_i2c_drv_clear_txfifo(i2c->base_addr);
	sunxi_i2c_drv_clear_rxfifo(i2c->base_addr);

	while (i < num) {
		dev_dbg(i2c->dev, "drv-mode: addr: 0x%x, flag:%x, len:%d\n",
			msgs[i].addr, msgs[i].flags, msgs[i].len);
		if (msgs[i].flags & I2C_M_RD) {
			if (sunxi_i2c_drv_rx_msgs(i2c, &msgs[i], 1))
				return -EINVAL;
			i++;
		} else if (i + 1 < num && msgs[i + 1].flags & I2C_M_RD) {
			if (sunxi_i2c_drv_rx_msgs(i2c, &msgs[i], 2))
				return -EINVAL;
			i += 2;
		} else {
			if (sunxi_i2c_drv_tx_one_msg(i2c, &msgs[i]))
				return -EINVAL;
			i++;
		}
	}

	return i;
}

static int
sunxi_i2c_engine_xfer(struct sunxi_i2c *i2c, struct i2c_msg *msgs, int num)
{
	unsigned long flags;
	int ret;
	void __iomem *base_addr = i2c->base_addr;

	sunxi_i2c_engine_disable_ack(base_addr);
	sunxi_i2c_engine_set_efr(base_addr, NO_DATA_WROTE);
	sunxi_i2c_engine_clear_irq(base_addr);

	/* may conflict with xfer_complete */
	spin_lock_irqsave(&i2c->lock, flags);
	i2c->msg = msgs;
	i2c->msg_num = num;
	i2c->msg_idx = 0;
	i2c->result = SUNXI_I2C_XFER_RESULT_RUNNING;  /* xfer means that the i2c result is in the running state */
	i2c->status = SUNXI_I2C_XFER_STATUS_START;
	spin_unlock_irqrestore(&i2c->lock, flags);

	sunxi_i2c_engine_enable_irq(base_addr);
	/* then send START signal, and needn't clear int flag */
	ret = sunxi_i2c_engine_start(i2c);
	if (ret) {
		dev_err(i2c->dev, "i2c failed to send start signal\n");
		sunxi_i2c_soft_reset(i2c);
		sunxi_i2c_engine_disable_irq(i2c->base_addr);
		i2c->status = SUNXI_I2C_XFER_STATUS_IDLE;
		return -EINVAL;
	}

	spin_lock_irqsave(&i2c->lock, flags);
	i2c->status = SUNXI_I2C_XFER_STATUS_RUNNING;
	spin_unlock_irqrestore(&i2c->lock, flags);

	/*
	 * sleep and wait, timeput = 5*HZ
	 * then do the transfer at function : sunxi_i2c_engine_core_process
	 */
	return sunxi_i2c_engine_complete(i2c);
}

static int
sunxi_i2c_xfer(struct i2c_adapter *adap, struct i2c_msg *msgs, int num)
{
	struct sunxi_i2c *i2c = (struct sunxi_i2c *)adap->algo_data;
	int ret;

	if (i2c->status == SUNXI_I2C_XFER_STATUS_SHUTDOWN) {
		dev_err(i2c->dev, "i2c bus is shutdown and shouldn't be working anymore\n");
		return -EBUSY;
	}

	if (IS_ERR_OR_NULL(msgs) || (num <= 0)) {
		dev_err(i2c->dev, "invalid argument\n");
		return -EINVAL;
	}

	/* then the sunxi_i2c_runtime_reseme() call back */
	ret = pm_runtime_get_sync(i2c->dev);
	if (ret < 0)
		goto out;

	sunxi_i2c_soft_reset(i2c);

	ret = sunxi_i2c_bus_barrier(&i2c->adap);
	if (ret) {
		dev_err(i2c->dev, "i2c bus barrier failed, sda is still low!\n");
		goto out;
	}

	if (i2c->twi_drv_used)
		ret = sunxi_i2c_drv_xfer(i2c, msgs, num);
	else
		ret = sunxi_i2c_engine_xfer(i2c, msgs, num);

out:
	pm_runtime_mark_last_busy(i2c->dev);
	/* asnyc release to ensure other module all suspend */
	pm_runtime_put_autosuspend(i2c->dev);

	return ret;
}

static unsigned int sunxi_i2c_functionality(struct i2c_adapter *adap)
{
	return I2C_FUNC_I2C|I2C_FUNC_10BIT_ADDR|I2C_FUNC_SMBUS_EMUL;
}

static const struct i2c_algorithm sunxi_i2c_algorithm = {
	.master_xfer	  = sunxi_i2c_xfer,
	.functionality	  = sunxi_i2c_functionality,
};

static struct i2c_bus_recovery_info sunxi_i2c_bus_recovery = {
	.get_scl = sunxi_i2c_get_scl,
	.set_scl = sunxi_i2c_set_scl,
	.get_sda = sunxi_i2c_get_sda,
	.recover_bus = i2c_generic_scl_recovery,
};

static void sunxi_i2c_adap_lock_bus(struct i2c_adapter *adapter, unsigned int flags)
{
	struct sunxi_i2c *i2c = (struct sunxi_i2c *)adapter->algo_data;

	mutex_lock(&i2c->bus_lock);
}

static int sunxi_i2c_adap_trylock_bus(struct i2c_adapter *adapter, unsigned int flags)
{
	struct sunxi_i2c *i2c = (struct sunxi_i2c *)adapter->algo_data;

	return mutex_trylock(&i2c->bus_lock);
}

static void sunxi_i2c_adap_unlock_bus(struct i2c_adapter *adapter, unsigned int flags)
{
	struct sunxi_i2c *i2c = (struct sunxi_i2c *)adapter->algo_data;

	mutex_unlock(&i2c->bus_lock);
}

static const struct i2c_lock_operations sunxi_i2c_adap_lock_ops = {
	.lock_bus =    sunxi_i2c_adap_lock_bus,
	.trylock_bus = sunxi_i2c_adap_trylock_bus,
	.unlock_bus =  sunxi_i2c_adap_unlock_bus,
};

static int sunxi_i2c_regulator_request(struct sunxi_i2c *i2c)
{
	struct regulator *regu = NULL;

#ifdef CONFIG_SUNXI_REGULATOR_DT
	regu = regulator_get_optional(i2c->dev, "twi");
	if (IS_ERR(regu)) {
		dev_err(i2c->dev, "regulator not found(isn't configured in dts)!\n");
		return -EPROBE_DEFER;
	}
#else
	/* Consider "n*" as nocare. Support "none", "nocare", "null", "" etc. */
	if ((i2c->regulator_id[0] == 'n') || (i2c->regulator_id[0] == 0))
		return 0;

	regu = regulator_get(NULL, i2c->regulator_id);
	if (IS_ERR(regu)) {
		dev_err(i2c->dev, "get regulator %s failed!\n", i2c->regulator_id);
		return -1;
	}
#endif
	i2c->regulator = regu;

	return 0;
}

static int sunxi_i2c_regulator_release(struct sunxi_i2c *i2c)
{
	if (!i2c->regulator)
		return 0;

	regulator_put(i2c->regulator);

	i2c->regulator = NULL;

	return 0;
}

static int sunxi_i2c_regulator_enable(struct sunxi_i2c *i2c)
{
	if (!i2c->regulator)
		return 0;

	/* set output voltage to the dts config */
	if (i2c->vol)
		regulator_set_voltage(i2c->regulator, i2c->vol, i2c->vol);

	if (regulator_enable(i2c->regulator)) {
		dev_err(i2c->dev, "enable regulator failed!\n");
		return -EINVAL;
	}

	return 0;
}

static int sunxi_i2c_regulator_disable(struct sunxi_i2c *i2c)
{
	if (!i2c->regulator)
		return 0;

	if (regulator_is_enabled(i2c->regulator))
		regulator_disable(i2c->regulator);

	return 0;
}

static int sunxi_i2c_clk_request(struct sunxi_i2c *i2c, struct device_node *np)
{
	i2c->bus_clk = of_clk_get(np, 0);
	if (IS_ERR(i2c->bus_clk)) {
		dev_err(i2c->dev, "request clock failed\n");
		return -EINVAL;
	}

	return 0;
}

static int sunxi_i2c_resource_get(struct device_node *np, struct sunxi_i2c *i2c)
{
	int err;
#ifndef CONFIG_SUNXI_REGULATOR_DT
	const char *str_vcc_twi;
#endif

	i2c->bus_num = of_alias_get_id(np, "twi");
	if (i2c->bus_num < 0) {
		dev_err(i2c->dev, "I2C failed to get alias id\n");
		return -EINVAL;
	}
	i2c->pdev->id = i2c->bus_num;

	i2c->res = platform_get_resource(i2c->pdev, IORESOURCE_MEM, 0);
	if (!i2c->res) {
		dev_err(i2c->dev, "failed to get MEM res\n");
		return -ENXIO;
	}
	i2c->base_addr = devm_ioremap_resource(i2c->dev, i2c->res);
	if (IS_ERR(i2c->base_addr)) {
		dev_err(i2c->dev, "unable to ioremap\n");
		return PTR_ERR(i2c->base_addr);
	}

	err = sunxi_i2c_clk_request(i2c, np);
	if (err) {
		dev_err(i2c->dev, "request i2c clk failed!\n");
		return err;
	}

	i2c->pctrl = devm_pinctrl_get(i2c->dev);
	if (IS_ERR(i2c->pctrl)) {
		dev_err(i2c->dev, "pinctrl_get failed\n");
		return PTR_ERR(i2c->pctrl);
	}

	i2c->no_suspend = 0;
	of_property_read_u32(np, "no_suspend", &i2c->no_suspend);
	i2c->irq_flag = (i2c->no_suspend) ? IRQF_NO_SUSPEND : 0;

	i2c->irq = platform_get_irq(i2c->pdev, 0);
	if (i2c->irq < 0) {
		dev_err(i2c->dev, "failed to get irq\n");
		return i2c->irq;
	}
	err = devm_request_irq(i2c->dev, i2c->irq, sunxi_i2c_handler,
			i2c->irq_flag, dev_name(i2c->dev), i2c);
	if (err) {
		dev_err(i2c->dev, "request irq failed!\n");
		return err;
	}

	i2c->vol = 0;
	of_property_read_u32(np, "twi_vol", &i2c->vol);
#ifndef CONFIG_SUNXI_REGULATOR_DT
	err = of_property_read_string(np, "twi_regulator", &str_vcc_twi);
	if (err)
		dev_err(i2c->dev, "warning: failed to get regulator id\n");
	else if (strlen(str_vcc_twi) >= sizeof(i2c->regulator_id))
		dev_err(i2c->dev, "illegal regulator id\n");
	else {
		strcpy(i2c->regulator_id, str_vcc_twi);
		dev_info(i2c->dev, "twi_regulator: %s\n", i2c->regulator_id);
	}
#endif

	err = of_property_read_u32(np, "clock-frequency", &i2c->bus_freq);
	if (err) {
		dev_err(i2c->dev, "failed to get clock frequency\n");
		goto err0;
	}

	i2c->pkt_interval = 0;
	of_property_read_u32(np, "twi_pkt_interval", &i2c->pkt_interval);
	i2c->twi_drv_used = 0;
	of_property_read_u32(np, "twi_drv_used", &i2c->twi_drv_used);

	if (i2c->twi_drv_used) {
		err = sunxi_i2c_dma_request(i2c, (dma_addr_t)i2c->res->start);
		if (err)
			dev_info(i2c->dev, "dma channel request failed, "
					"or no dma comfiguration information in dts\n");
	}

	return 0;

err0:
	sunxi_i2c_regulator_release(i2c);
	return err;
}

static void sunxi_i2c_resource_put(struct sunxi_i2c *i2c)
{
	if (i2c->twi_drv_used)
		sunxi_i2c_dma_release(i2c);

	sunxi_i2c_regulator_release(i2c);
}

static int sunxi_i2c_select_pin_state(struct sunxi_i2c *i2c, char *name)
{
	int ret;
	struct pinctrl_state *pctrl_state = NULL;

	pctrl_state = pinctrl_lookup_state(i2c->pctrl, name);
	if (IS_ERR(pctrl_state)) {
		dev_err(i2c->dev, "pinctrl_lookup_state(%s) failed! return %p\n",
			name, pctrl_state);
		return -EINVAL;
	}

	ret = pinctrl_select_state(i2c->pctrl, pctrl_state);
	if (ret < 0)
		dev_err(i2c->dev, "pinctrl select state(%s) failed! return %d\n",
				name, ret);

	return ret;
}

static int sunxi_i2c_clk_init(struct sunxi_i2c *i2c)
{
	/*
	 * i2c will be used in the uboot stage, so it must be reset before
	 * configuring the module registers to clean up the configuration
	 * information of the uboot stage
	 */
	sunxi_periph_reset_assert(i2c->bus_clk);
	sunxi_periph_reset_deassert(i2c->bus_clk);

	if (clk_prepare_enable(i2c->bus_clk)) {
		dev_err(i2c->dev, "enable apb_twi clock failed!\n");
		return -EINVAL;
	}

	/* i2c ctroller module clock is controllerd by self */
	sunxi_i2c_set_frequency(i2c);

	return 0;
}

static void sunxi_i2c_clk_exit(struct sunxi_i2c *i2c)
{
	/* disable clk */
	if (!IS_ERR_OR_NULL(i2c->bus_clk) && __clk_is_enabled(i2c->bus_clk)) {
		clk_disable_unprepare(i2c->bus_clk);
	}

	sunxi_periph_reset_assert(i2c->bus_clk);
}

static int sunxi_i2c_hw_init(struct sunxi_i2c *i2c)
{
	int err;

	err = sunxi_i2c_regulator_request(i2c);
	if (!err) {
		dev_dbg(i2c->dev, "request regulator failed!\n");
		err = sunxi_i2c_regulator_enable(i2c);
		if (err) {
			dev_err(i2c->dev, "enable regulator failed!\n");
			goto err0;
		}
	}

	err = sunxi_i2c_select_pin_state(i2c, PINCTRL_STATE_DEFAULT);
	if (err) {
		dev_err(i2c->dev, "request i2c gpio failed!\n");
		goto err1;
	}

	err = sunxi_i2c_clk_init(i2c);
	if (err) {
		dev_err(i2c->dev, "init i2c clock failed!\n");
		goto err2;
	}

	sunxi_i2c_soft_reset(i2c);

	sunxi_i2c_bus_enable(i2c);

	return 0;

err2:
	sunxi_i2c_select_pin_state(i2c, PINCTRL_STATE_SLEEP);

err1:
	sunxi_i2c_regulator_disable(i2c);

err0:
	return err;
}

static void sunxi_i2c_hw_exit(struct sunxi_i2c *i2c)
{
	sunxi_i2c_bus_disable(i2c);

	sunxi_i2c_clk_exit(i2c);

	sunxi_i2c_select_pin_state(i2c, PINCTRL_STATE_SLEEP);

	sunxi_i2c_regulator_disable(i2c);
}

static ssize_t sunxi_i2c_info_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sunxi_i2c *i2c = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE,
			"i2c->bus_num = %d\n"
			"i2c->name = %s\n"
			"i2c->irq = %d\n"
			"i2c->freqency = %u\n",
			i2c->bus_num,
			dev_name(i2c->dev),
			i2c->irq,
			i2c->bus_freq);
}

static struct device_attribute sunxi_i2c_info_attr =
	__ATTR(info, S_IRUGO, sunxi_i2c_info_show, NULL);

static ssize_t sunxi_i2c_status_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sunxi_i2c *i2c = dev_get_drvdata(dev);
	static char const *i2c_status[] = {"Unknown", "Idle", "Start",
						"Unknown", "Running"};

	if (i2c == NULL)
		return snprintf(buf, PAGE_SIZE, "%s\n", "sunxi_i2c is NULL!");

	return snprintf(buf, PAGE_SIZE,
			"i2c->bus_num = %d\n"
			"i2c->status  = [%d] %s\n"
			"i2c->msg_num   = %u, ->msg_idx = %u, ->buf_idx = %u\n"
			"i2c->bus_freq  = %u\n"
			"i2c->irq       = %d\n"
			"i2c->debug_state = %u\n"
			"i2c->base_addr = 0x%p, the TWI control register:\n"
			"[ADDR] 0x%02x = 0x%08x, [XADDR] 0x%02x = 0x%08x\n"
			"[DATA] 0x%02x = 0x%08x, [CNTR] 0x%02x = 0x%08x\n"
			"[STAT]  0x%02x = 0x%08x, [CCR]  0x%02x = 0x%08x\n"
			"[SRST] 0x%02x = 0x%08x, [EFR]   0x%02x = 0x%08x\n"
			"[LCR]  0x%02x = 0x%08x\n",
			i2c->bus_num, i2c->status, i2c_status[i2c->status],
			i2c->msg_num, i2c->msg_idx, i2c->buf_idx,
			i2c->bus_freq, i2c->irq, i2c->debug_state,
			i2c->base_addr,
			I2C_ADDR,	readl(i2c->base_addr + I2C_ADDR),
			I2C_XADDR,	readl(i2c->base_addr + I2C_XADDR),
			I2C_DATA,	readl(i2c->base_addr + I2C_DATA),
			I2C_CNTR,	readl(i2c->base_addr + I2C_CNTR),
			I2C_STAT,	readl(i2c->base_addr + I2C_STAT),
			I2C_CCR,	readl(i2c->base_addr + I2C_CCR),
			I2C_SRST,	readl(i2c->base_addr + I2C_SRST),
			I2C_EFR,	readl(i2c->base_addr + I2C_EFR),
			I2C_LCR,	readl(i2c->base_addr + I2C_LCR));
}

static struct device_attribute sunxi_i2c_status_attr =
	__ATTR(status, S_IRUGO, sunxi_i2c_status_show, NULL);

static void sunxi_i2c_create_sysfs(struct platform_device *_pdev)
{
	device_create_file(&_pdev->dev, &sunxi_i2c_info_attr);
	device_create_file(&_pdev->dev, &sunxi_i2c_status_attr);
}

static void sunxi_i2c_remove_sysfs(struct platform_device *_pdev)
{
	device_remove_file(&_pdev->dev, &sunxi_i2c_info_attr);
	device_remove_file(&_pdev->dev, &sunxi_i2c_status_attr);
}

static int sunxi_i2c_probe(struct platform_device *pdev)
{
	struct sunxi_i2c *i2c = NULL;
	int err;

	i2c = devm_kzalloc(&pdev->dev, sizeof(struct sunxi_i2c), GFP_KERNEL);
	if (IS_ERR_OR_NULL(i2c))
		return -ENOMEM;  /* Do not print prompts after kzalloc errors */

	i2c->pdev = pdev;
	i2c->dev = &pdev->dev;
	i2c->result = SUNXI_I2C_XFER_RESULT_NULL;  /* initialize i2c result to NULL */

	/* get dts resource */
	err = sunxi_i2c_resource_get(pdev->dev.of_node, i2c);
	if (err) {
		dev_err(i2c->dev, "I2C failed to get resource\n");
		goto err0;
	}

	/* i2c controller hardware init */
	err = sunxi_i2c_hw_init(i2c);
	if (err) {
		dev_err(i2c->dev, "hw init failed! try again!!\n");
		goto err1;
	}

	spin_lock_init(&i2c->lock);
	init_waitqueue_head(&i2c->wait);

	sunxi_i2c_create_sysfs(pdev);

	pm_runtime_set_active(i2c->dev);
	if (i2c->no_suspend)
		pm_runtime_get_noresume(i2c->dev);
	pm_runtime_set_autosuspend_delay(i2c->dev, AUTOSUSPEND_TIMEOUT);
	pm_runtime_use_autosuspend(i2c->dev);
	pm_runtime_enable(i2c->dev);

	snprintf(i2c->adap.name, sizeof(i2c->adap.name), "SUNXI I2C(%pa)", &i2c->res->start);
	mutex_init(&i2c->bus_lock);
	i2c->status = SUNXI_I2C_XFER_STATUS_IDLE;;
	i2c->adap.owner = THIS_MODULE;
	i2c->adap.nr = i2c->bus_num;
	i2c->adap.retries = 3;
	i2c->adap.timeout = 3 * HZ;
	i2c->adap.class = I2C_CLASS_HWMON | I2C_CLASS_SPD;
	i2c->adap.algo = &sunxi_i2c_algorithm;
	i2c->adap.bus_recovery_info = &sunxi_i2c_bus_recovery;
	i2c->adap.lock_ops = &sunxi_i2c_adap_lock_ops;
	i2c->adap.algo_data = i2c;
	i2c->adap.dev.parent = &pdev->dev;
	i2c->adap.dev.of_node = pdev->dev.of_node;
	platform_set_drvdata(pdev, i2c);

	/*
	 * register i2c adapter should be the ending of probe
	 * before register all the resouce i2c controller need be ready
	 * (i2c_xfer may occur at any time when register)
	 */
	err = i2c_add_numbered_adapter(&i2c->adap);
	if (err) {
		dev_err(i2c->dev, "failed to add adapter\n");
		goto err2;
	}

	if (!i2c->no_suspend) {
		pm_runtime_mark_last_busy(i2c->dev);
		pm_runtime_put_autosuspend(i2c->dev);
	}

	dev_info(i2c->dev, "probe success\n");
	return 0;

err2:
	pm_runtime_set_suspended(i2c->dev);
	pm_runtime_disable(i2c->dev);
	sunxi_i2c_remove_sysfs(pdev);
	sunxi_i2c_hw_exit(i2c);

err1:
	sunxi_i2c_resource_put(i2c);

err0:
	return err;
}

static int sunxi_i2c_remove(struct platform_device *pdev)
{
	struct sunxi_i2c *i2c = platform_get_drvdata(pdev);

	i2c_del_adapter(&i2c->adap);

	platform_set_drvdata(pdev, NULL);

	pm_runtime_set_suspended(i2c->dev);
	pm_runtime_disable(i2c->dev);
	sunxi_i2c_remove_sysfs(pdev);
	sunxi_i2c_hw_exit(i2c);

	sunxi_i2c_resource_put(i2c);

	dev_dbg(i2c->dev, "remove\n");

	return 0;
}

static void sunxi_i2c_shutdown(struct platform_device *pdev)
{
	struct sunxi_i2c *i2c = platform_get_drvdata(pdev);
	unsigned long flags, timeout;

	spin_lock_irqsave(&i2c->lock, flags);
	/*
	 * @TODO
	 * in the future, the i2c->status and i2c->result variables need to be unified
	 */
	i2c->status = SUNXI_I2C_XFER_STATUS_SHUTDOWN;
	spin_unlock_irqrestore(&i2c->lock, flags);

	if (i2c->result != SUNXI_I2C_XFER_RESULT_RUNNING) {
		dev_info(i2c->dev, "xfer completed, shutdown i2c directly\n");
		return;
	}

	timeout = wait_event_timeout(i2c->wait, i2c->result, i2c->adap.timeout);
	if (timeout == 0)
		dev_err(i2c->dev, "shutdown i2c timeout, should be reinitialized when used in new stage\n");

	dev_info(i2c->dev, "shutdown i2c after waiting for the transfer to complete\n");

	return;
}

#if IS_ENABLED(CONFIG_PM)
static int sunxi_i2c_runtime_suspend(struct device *dev)
{
	struct sunxi_i2c *i2c = dev_get_drvdata(dev);

	sunxi_i2c_hw_exit(i2c);
	dev_dbg(i2c->dev, "runtime suspend finish\n");

	return 0;
}

static int sunxi_i2c_runtime_resume(struct device *dev)
{
	struct sunxi_i2c *i2c = dev_get_drvdata(dev);

	sunxi_i2c_hw_init(i2c);
	dev_dbg(i2c->dev, "runtime resume finish\n");

	return 0;
}

static int sunxi_i2c_suspend_noirq(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct sunxi_i2c *i2c = platform_get_drvdata(pdev);

	if (i2c->no_suspend) {
		dev_dbg(i2c->dev, "doesn't need to suspend\n");
		return 0;
	}

	if (i2c->twi_drv_used)
		sunxi_i2c_bus_disable(i2c);

	dev_dbg(i2c->dev, "suspend noirq\n");
	return pm_runtime_force_suspend(dev);
}

static int sunxi_i2c_resume_noirq(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct sunxi_i2c *i2c = platform_get_drvdata(pdev);

	if (i2c->no_suspend) {
		dev_dbg(i2c->dev, "doesn't need to resume\n");
		return 0;
	}

	if (i2c->twi_drv_used) {
		sunxi_i2c_set_frequency(i2c);
		sunxi_i2c_bus_enable(i2c);
	}

	dev_dbg(i2c->dev, "resume noirq\n");
	return pm_runtime_force_resume(dev);
}

static const struct dev_pm_ops sunxi_i2c_dev_pm_ops = {
	.suspend_noirq	 = sunxi_i2c_suspend_noirq,
	.resume_noirq	 = sunxi_i2c_resume_noirq,
	.runtime_suspend = sunxi_i2c_runtime_suspend,
	.runtime_resume  = sunxi_i2c_runtime_resume,
};

#define SUNXI_I2C_DEV_PM_OPS (&sunxi_i2c_dev_pm_ops)
#else
#define SUNXI_I2C_DEV_PM_OPS NULL
#endif

static const struct of_device_id sunxi_i2c_match[] = {
	{ .compatible = "allwinner,sunxi-twi-v100", },
	{},
};
MODULE_DEVICE_TABLE(of, sunxi_i2c_match);

static struct platform_driver sunxi_i2c_driver = {
	.probe		= sunxi_i2c_probe,
	.remove		= sunxi_i2c_remove,
	.shutdown	= sunxi_i2c_shutdown,
	.driver		= {
		.name	= "sunxi-i2c",
		.owner	= THIS_MODULE,
		.pm		= SUNXI_I2C_DEV_PM_OPS,
		.of_match_table = sunxi_i2c_match,
	},
};

static int __init sunxi_i2c_adap_init(void)
{
	return platform_driver_register(&sunxi_i2c_driver);
}

static void __exit sunxi_i2c_adap_exit(void)
{
	platform_driver_unregister(&sunxi_i2c_driver);
}

subsys_initcall_sync(sunxi_i2c_adap_init);

module_exit(sunxi_i2c_adap_exit);

MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:i2c-sunxi");
MODULE_VERSION("2.1.3");
MODULE_DESCRIPTION("SUNXI I2C Bus Driver");
MODULE_AUTHOR("pannan");
MODULE_AUTHOR("shaosidi <shaosidi@allwinnertech.com>");
