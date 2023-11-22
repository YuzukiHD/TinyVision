// SPDX-License-Identifier: GPL-2.0-or-later
/* Copyright(c) 2020 - 2023 Allwinner Technology Co.,Ltd. All rights reserved. */
/*
 * Copyright (C) 2022 Allwinner.
 *
 * SUNXI SPI Controller BIT Driver
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 */

#define SUNXI_MODNAME "spi"
#include <sunxi-log.h>
#include "../spi-sunxi-debug.h"
#include "spi-sunxi-bit.h"

/* SPI Controller Hardware Register Operation Start */

static void sunxi_spi_bit_start_xfer(struct sunxi_spi *sspi)
{
	u32 reg_val = readl(sspi->base_addr + SUNXI_SPI_BATC_REG);
	reg_val |= SUNXI_SPI_BATC_TCE;
	writel(reg_val, sspi->base_addr + SUNXI_SPI_BATC_REG);
}

static void sunxi_spi_bit_sample_mode(struct sunxi_spi *sspi, u8 mode)
{
	u32 reg_val = readl(sspi->base_addr + SUNXI_SPI_BATC_REG);

	if (mode)
		reg_val |= SUNXI_SPI_BATC_MSMS;
	else
		reg_val &= ~SUNXI_SPI_BATC_MSMS;

	writel(reg_val, sspi->base_addr + SUNXI_SPI_BATC_REG);
}

inline u32 sunxi_spi_bit_qry_irq_pending(struct sunxi_spi *sspi)
{
	return (SUNXI_SPI_BATC_TBC & readl(sspi->base_addr + SUNXI_SPI_BATC_REG));
}

void sunxi_spi_bit_clr_irq_pending(struct sunxi_spi *sspi)
{
	u32 reg_val = readl(sspi->base_addr + SUNXI_SPI_BATC_REG);
	reg_val |= SUNXI_SPI_BATC_TBC;
	writel(reg_val, sspi->base_addr + SUNXI_SPI_BATC_REG);
}

void sunxi_spi_bit_enable_irq(struct sunxi_spi *sspi)
{
	u32 reg_val = readl(sspi->base_addr + SUNXI_SPI_BATC_REG);
	reg_val |= SUNXI_SPI_BATC_TBC_INT_EN;
	writel(reg_val, sspi->base_addr + SUNXI_SPI_BATC_REG);
}

void sunxi_spi_bit_disable_irq(struct sunxi_spi *sspi)
{
	u32 reg_val = readl(sspi->base_addr + SUNXI_SPI_BATC_REG);
	reg_val &= ~SUNXI_SPI_BATC_TBC_INT_EN;
	writel(reg_val, sspi->base_addr + SUNXI_SPI_BATC_REG);
}

static void sunxi_spi_bit_set_bc(struct sunxi_spi *sspi, u32 tx_len, u32 rx_len)
{
	u32 reg_val = readl(sspi->base_addr + SUNXI_SPI_BATC_REG);
	reg_val &= ~(SUNXI_SPI_BATC_RX_FRM_LEN | SUNXI_SPI_BATC_TX_FRM_LEN);
	reg_val |= FIELD_PREP(SUNXI_SPI_BATC_TX_FRM_LEN, tx_len);
	reg_val |= FIELD_PREP(SUNXI_SPI_BATC_RX_FRM_LEN, rx_len);
	writel(reg_val, sspi->base_addr + SUNXI_SPI_BATC_REG);
}

static void sunxi_spi_bit_ss_level(struct sunxi_spi *sspi, u8 status)
{
	u32 reg_val = readl(sspi->base_addr + SUNXI_SPI_BATC_REG);

	if (status)
		reg_val |= SUNXI_SPI_BATC_SS_LEVEL;
	else
		reg_val &= ~SUNXI_SPI_BATC_SS_LEVEL;

	writel(reg_val, sspi->base_addr + SUNXI_SPI_BATC_REG);
}

void sunxi_spi_bit_ss_owner(struct sunxi_spi *sspi, u32 owner)
{
	u32 reg_val = readl(sspi->base_addr + SUNXI_SPI_BATC_REG);

	switch (owner) {
	case SUNXI_SPI_CS_AUTO:
		reg_val &= ~SUNXI_SPI_BATC_SS_OWNER;
		break;
	case SUNXI_SPI_CS_SOFT:
		reg_val |= SUNXI_SPI_BATC_SS_OWNER;
		break;
	}

	writel(reg_val, sspi->base_addr + SUNXI_SPI_BATC_REG);
}

void sunxi_spi_bit_ss_polarity(struct sunxi_spi *sspi, bool pol)
{
	u32 reg_val = readl(sspi->base_addr + SUNXI_SPI_BATC_REG);

	if (pol)
		reg_val |= SUNXI_SPI_BATC_SPOL;
	else
		reg_val &= ~SUNXI_SPI_BATC_SPOL;

	writel(reg_val, sspi->base_addr + SUNXI_SPI_BATC_REG);
}

static int sunxi_spi_bit_ss_select(struct sunxi_spi *sspi, u16 cs)
{
	int ret = 0;
	u32 reg_val;

	if (cs < SUNXI_SPI_CS_MAX) {
		reg_val = readl(sspi->base_addr + SUNXI_SPI_BATC_REG);
		reg_val &= ~SUNXI_SPI_BATC_SS_SEL;
		reg_val |= FIELD_PREP(SUNXI_SPI_BATC_SS_SEL, cs);
		writel(reg_val, sspi->base_addr + SUNXI_SPI_BATC_REG);
	} else {
		ret = -EINVAL;
	}

	return ret;
}

/* SPI Controller Hardware Register Operation End */

void sunxi_spi_bit_set_cs(struct spi_device *spi, bool status)
{
	struct sunxi_spi *sspi = spi_controller_get_devdata(spi->controller);
	int ret;

	ret = sunxi_spi_bit_ss_select(sspi, spi->chip_select);
	if (ret < 0) {
		sunxi_warn(sspi->dev, "bit cs %d over range, need control by software\n", spi->chip_select);
		return ;
	}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 18, 0))
	if (spi->cs_gpiod)
#else
	if (spi->cs_gpiod || gpio_is_valid(spi->cs_gpio))
#endif
		return ;

	sunxi_spi_bit_ss_polarity(sspi, !(spi->mode & SPI_CS_HIGH));

	if (sspi->cs_mode == SUNXI_SPI_CS_SOFT)
		sunxi_spi_bit_ss_level(sspi, status);
}

void sunxi_spi_bit_sample_delay(struct sunxi_spi *sspi, u32 clk)
{
	if (clk > SUNXI_SPI_SAMP_LOW_FREQ)
		sunxi_spi_bit_sample_mode(sspi, 1);
	else
		sunxi_spi_bit_sample_mode(sspi, 0);
}

void sunxi_spi_bit_config_tc(struct sunxi_spi *sspi, u32 config)
{
	u32 reg_val = readl(sspi->base_addr + SUNXI_SPI_BATC_REG);

	reg_val &= ~SUNXI_SPI_BATC_WMS;
	if (config & SPI_3WIRE)
		reg_val |= FIELD_PREP(SUNXI_SPI_BATC_WMS, SUNXI_SPI_BIT_3WIRE_MODE);
	else
		reg_val |= FIELD_PREP(SUNXI_SPI_BATC_WMS, SUNXI_SPI_BIT_STD_MODE);

	writel(reg_val, sspi->base_addr + SUNXI_SPI_BATC_REG);
}

static int sunxi_spi_bit_cpu_rx(struct sunxi_spi *sspi, struct spi_transfer *t)
{
	u8 bits = t->bits_per_word;
	u8 *buf = (u8 *)t->rx_buf;
	u32 data;

	if (bits <= 8) {
		data = readb(sspi->base_addr + SUNXI_SPI_RB_REG);
		*((u8 *)buf) = data & (BIT(bits) - 1);
	} else if (bits <= 16) {
		data = readw(sspi->base_addr + SUNXI_SPI_RB_REG);
		*((u16 *)buf) = data & (BIT(bits) - 1);
	} else {
		data = readl(sspi->base_addr + SUNXI_SPI_RB_REG);
		*((u32 *)buf) = data & (BIT(bits) - 1);
	}

	sunxi_debug(sspi->dev, "bit cpu rx data %#x\n", data);

	return 0;
}

static int sunxi_spi_bit_cpu_tx(struct sunxi_spi *sspi, struct spi_transfer *t)
{
	u8 bits = t->bits_per_word;
	u8 *buf = (u8 *)t->tx_buf;
	u32 data;

	if (bits <= 8) {
		data = *((u8 *)buf) & (BIT(bits) - 1);
		writeb(data, sspi->base_addr + SUNXI_SPI_TB_REG);
	} else if (bits <= 16) {
		data = *((u16 *)buf) & (BIT(bits) - 1);
		writew(data, sspi->base_addr + SUNXI_SPI_TB_REG);
	} else {
		data = *((u32 *)buf) & (BIT(bits) - 1);
		writel(data, sspi->base_addr + SUNXI_SPI_TB_REG);
	}

	sunxi_debug(sspi->dev, "bit cpu tx data %#x\n", data);

	return 0;
}

int sunxi_spi_mode_check_bit(struct sunxi_spi *sspi, struct spi_transfer *t)
{
	int ret = 0;

	if (t->tx_buf && t->rx_buf) {
		sunxi_err(sspi->dev, "bit mode not support full duplex mode\n");
		ret = -EINVAL;
	} else {
		if (t->bits_per_word > SUNXI_SPI_BIT_MAX_LEN) {
			sunxi_err(sspi->dev, "unsupport bits_per_word len %d\n", t->bits_per_word);
			ret = -EINVAL;
		} else if (t->tx_buf) {
			sspi->mode_type = SINGLE_HALF_DUPLEX_TX;
			sunxi_spi_bit_set_bc(sspi, t->bits_per_word, 0);
		} else if (t->rx_buf) {
			sspi->mode_type = SINGLE_HALF_DUPLEX_RX;
			sunxi_spi_bit_set_bc(sspi, 0, t->bits_per_word);
		}
		sunxi_debug(sspi->dev, "bit mode type %d\n", sspi->mode_type);
	}

	return ret;
}

void sunxi_spi_bit_handler(struct sunxi_spi *sspi)
{
	u32 status = 0;

	status = sunxi_spi_bit_qry_irq_pending(sspi);
	sunxi_spi_bit_clr_irq_pending(sspi);
	sunxi_debug(sspi->dev, "bit irq handler status(%#x)\n", status);

	if (status & SUNXI_SPI_BATC_TBC) {
		sunxi_debug(sspi->dev, "bit irq tc comes\n");
		sunxi_spi_bit_disable_irq(sspi);
		complete(&sspi->done);
	} else {
		sunxi_err(sspi->dev, "bit unkown status(%#x)\n", status);
	}
}

int sunxi_spi_xfer_bit(struct spi_device *spi, struct spi_transfer *t)
{
	struct sunxi_spi *sspi = spi_controller_get_devdata(spi->controller);
	unsigned long timeout = 0;
	int ret = 0;

	switch (sspi->mode_type) {
	case SINGLE_HALF_DUPLEX_RX:
		sunxi_debug(sspi->dev, "bit xfer rx by cpu %d\n", t->bits_per_word);
		sunxi_spi_bit_start_xfer(sspi);
		break;
	case SINGLE_HALF_DUPLEX_TX:
		sunxi_debug(sspi->dev, "bit xfer tx by cpu %d\n", t->bits_per_word);
		ret = sunxi_spi_bit_cpu_tx(sspi, t);
		sunxi_spi_bit_start_xfer(sspi);
		break;
	default:
		sunxi_err(sspi->dev, "unknown bit transfer mode type %d\n", sspi->mode_type);
		ret = -EINVAL;
		goto out;
	}

	timeout = wait_for_completion_timeout(&sspi->done, msecs_to_jiffies(SUNXI_SPI_XFER_TIMEOUT));
	if (timeout == 0) {
		sunxi_err(sspi->dev, "bit transfer timeout type %d\n", sspi->mode_type);
		ret = -ETIME;
		goto out;
	} else if (sspi->result < 0) {
		sunxi_err(sspi->dev, "bit transfer failed %d\n", sspi->result);
		ret = -EINVAL;
		goto out;
	}

	/* Bit-Aligned mode didn't have hw fifo, read the data immediately after transfer complete */
	if (sspi->mode_type == SINGLE_HALF_DUPLEX_RX)
		ret = sunxi_spi_bit_cpu_rx(sspi, t);

out:
	sunxi_spi_bit_disable_irq(sspi);
	return ret;
}
