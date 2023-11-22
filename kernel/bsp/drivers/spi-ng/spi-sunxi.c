// SPDX-License-Identifier: GPL-2.0-or-later
/* Copyright(c) 2020 - 2023 Allwinner Technology Co.,Ltd. All rights reserved. */
/*
 * Copyright (C) 2022 Allwinner.
 *
 * SUNXI SPI Controller Driver
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 */

#define SUNXI_MODNAME "spi"
#include <sunxi-log.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/dmaengine.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/gpio.h>

#include "spi-sunxi.h"
#include "spi-sunxi-debug.h"

#define SUNXI_SPI_DEV_NAME "sunxi-spi-ng"
#define SUNXI_SPI_MODULE_VERSION "2.3.19"

static int sunxi_spi_debug_mask;
module_param_named(spi_debug_mask, sunxi_spi_debug_mask, int, 0664);

static const u32 sunxi_spi_sample_mode[] = {
	0x100, /* SUNXI_SPI_SAMP_DELAY_CYCLE_0_0 */
	0x000, /* SUNXI_SPI_SAMP_DELAY_CYCLE_0_5 */
	0x010, /* SUNXI_SPI_SAMP_DELAY_CYCLE_1_0 */
	0x110, /* SUNXI_SPI_SAMP_DELAY_CYCLE_1_5 */
	0x101, /* SUNXI_SPI_SAMP_DELAY_CYCLE_2_0 */
	0x001, /* SUNXI_SPI_SAMP_DELAY_CYCLE_2_5 */
	0x011  /* SUNXI_SPI_SAMP_DELAY_CYCLE_3_0 */
};

static void sunxi_spi_dump_regs(struct sunxi_spi *sspi)
{
	sunxi_spi_dump_reg(sspi->dev, sspi->base_addr, sspi->base_addr_phy, SUNXI_SPI_DUMP_BASE_SIZE);
	if (sspi->bus_mode == SUNXI_SPI_BUS_DBI)
		sunxi_spi_dump_reg(sspi->dev, sspi->base_addr + 0x100, sspi->base_addr_phy + 0x100, SUNXI_SPI_DUMP_DBI_SIZE);
	if (sspi->data->quirk_flag & INDEPENDENT_BSR)
		sunxi_spi_dump_reg(sspi->dev, sspi->base_addr + 0x400, sspi->base_addr_phy + 0x400, SUNXI_SPI_DUMP_BSR_SIZE);
}

/* SPI Controller Hardware Register Operation Start */

static inline u32 sunxi_spi_get_ver_h(struct sunxi_spi *sspi)
{
	return FIELD_GET(SUNXI_SPI_VER_H, readl(sspi->base_addr + SUNXI_SPI_VER_REG));
}

static inline u32 sunxi_spi_get_ver_l(struct sunxi_spi *sspi)
{
	return FIELD_GET(SUNXI_SPI_VER_L, readl(sspi->base_addr + SUNXI_SPI_VER_REG));
}

static void sunxi_spi_enable_tp(struct sunxi_spi *sspi)
{
	u32 reg_val = readl(sspi->base_addr + SUNXI_SPI_GC_REG);
	reg_val |= SUNXI_SPI_GC_TP_EN;
	writel(reg_val, sspi->base_addr + SUNXI_SPI_GC_REG);
}

static void sunxi_spi_disable_tp(struct sunxi_spi *sspi)
{
	u32 reg_val = readl(sspi->base_addr + SUNXI_SPI_GC_REG);
	reg_val &= ~SUNXI_SPI_GC_TP_EN;
	writel(reg_val, sspi->base_addr + SUNXI_SPI_GC_REG);
}

static void sunxi_spi_enable_dbi(struct sunxi_spi *sspi)
{
	u32 reg_val = readl(sspi->base_addr + SUNXI_SPI_GC_REG);
	reg_val |= SUNXI_SPI_GC_DBI_EN;
	writel(reg_val, sspi->base_addr + SUNXI_SPI_GC_REG);
}

static void sunxi_spi_disable_dbi(struct sunxi_spi *sspi)
{
	u32 reg_val = readl(sspi->base_addr + SUNXI_SPI_GC_REG);
	reg_val &= ~SUNXI_SPI_GC_DBI_EN;
	writel(reg_val, sspi->base_addr + SUNXI_SPI_GC_REG);
}

static void sunxi_spi_set_dbi(struct sunxi_spi *sspi)
{
	u32 reg_val = readl(sspi->base_addr + SUNXI_SPI_GC_REG);
	reg_val |= SUNXI_SPI_GC_DBI_MODE_SEL;
	writel(reg_val, sspi->base_addr + SUNXI_SPI_GC_REG);
}

static void sunxi_spi_set_spi(struct sunxi_spi *sspi)
{
	u32 reg_val = readl(sspi->base_addr + SUNXI_SPI_GC_REG);
	reg_val &= ~SUNXI_SPI_GC_DBI_MODE_SEL;
	writel(reg_val, sspi->base_addr + SUNXI_SPI_GC_REG);
}

static void sunxi_spi_bus_sample_mode(struct sunxi_spi *sspi, u32 mode)
{
	u32 reg_val = readl(sspi->base_addr + SUNXI_SPI_GC_REG);

	switch (mode) {
	case SUNXI_SPI_SAMP_MODE_OLD:
		reg_val &= ~SUNXI_SPI_GC_MODE_SEL;
		break;
	case SUNXI_SPI_SAMP_MODE_NEW:
		reg_val |= SUNXI_SPI_GC_MODE_SEL;
		break;
	}

	writel(reg_val, sspi->base_addr + SUNXI_SPI_GC_REG);
}

static void sunxi_spi_set_master(struct sunxi_spi *sspi)
{
	u32 reg_val = readl(sspi->base_addr + SUNXI_SPI_GC_REG);
	reg_val |= SUNXI_SPI_GC_MODE;
	writel(reg_val, sspi->base_addr + SUNXI_SPI_GC_REG);
}

static void sunxi_spi_set_slave(struct sunxi_spi *sspi)
{
	u32 reg_val = readl(sspi->base_addr + SUNXI_SPI_GC_REG);
	reg_val &= ~SUNXI_SPI_GC_MODE;
	writel(reg_val, sspi->base_addr + SUNXI_SPI_GC_REG);
}

static void sunxi_spi_enable_bus(struct sunxi_spi *sspi)
{
	u32 reg_val = readl(sspi->base_addr + SUNXI_SPI_GC_REG);
	reg_val |= SUNXI_SPI_GC_EN;
	writel(reg_val, sspi->base_addr + SUNXI_SPI_GC_REG);
}

static void sunxi_spi_disable_bus(struct sunxi_spi *sspi)
{
	u32 reg_val = readl(sspi->base_addr + SUNXI_SPI_GC_REG);
	reg_val &= ~SUNXI_SPI_GC_EN;
	writel(reg_val, sspi->base_addr + SUNXI_SPI_GC_REG);
}

static void sunxi_spi_start_xfer(struct sunxi_spi *sspi)
{
	u32 reg_val = readl(sspi->base_addr + SUNXI_SPI_TC_REG);
	reg_val |= SUNXI_SPI_TC_XCH;
	writel(reg_val, sspi->base_addr + SUNXI_SPI_TC_REG);
}

static void sunxi_spi_set_sample_mode(struct sunxi_spi *sspi, u32 mode)
{
	u32 reg_val = readl(sspi->base_addr + SUNXI_SPI_TC_REG);
	u32 org_val = reg_val;
	u32 sdm, sdc, sdc1;

	sdm = (sunxi_spi_sample_mode[mode] >> 8) & 0xf;
	sdc = (sunxi_spi_sample_mode[mode] >> 4) & 0xf;
	sdc1 = (sunxi_spi_sample_mode[mode] >> 0) & 0xf;

	if (sdm)
		reg_val |= SUNXI_SPI_TC_SDM;
	else
		reg_val &= ~SUNXI_SPI_TC_SDM;

	if (sdc)
		reg_val |= SUNXI_SPI_TC_SDC;
	else
		reg_val &= ~SUNXI_SPI_TC_SDC;

	if (sdc1)
		reg_val |= SUNXI_SPI_TC_SDC1;
	else
		reg_val &= ~SUNXI_SPI_TC_SDC1;

	if (reg_val != org_val)
		writel(reg_val, sspi->base_addr + SUNXI_SPI_TC_REG);
}

static void sunxi_spi_set_dummy_type(struct sunxi_spi *sspi, bool ddb)
{
	u32 reg_val = readl(sspi->base_addr + SUNXI_SPI_TC_REG);

	if (ddb)
		reg_val |= SUNXI_SPI_TC_DDB;
	else
		reg_val &= ~SUNXI_SPI_TC_DDB;

	writel(reg_val, sspi->base_addr + SUNXI_SPI_TC_REG);
}

static void sunxi_spi_set_discard_burst(struct sunxi_spi *sspi, bool dhb)
{
	u32 reg_val = readl(sspi->base_addr + SUNXI_SPI_TC_REG);

	if (dhb)
		reg_val |= SUNXI_SPI_TC_DHB;
	else
		reg_val &= ~SUNXI_SPI_TC_DHB;

	writel(reg_val, sspi->base_addr + SUNXI_SPI_TC_REG);
}

static void sunxi_spi_ss_level(struct sunxi_spi *sspi, bool status)
{
	u32 reg_val = readl(sspi->base_addr + SUNXI_SPI_TC_REG);

	if (status)
		reg_val |= SUNXI_SPI_TC_SS_LEVEL;
	else
		reg_val &= ~SUNXI_SPI_TC_SS_LEVEL;

	writel(reg_val, sspi->base_addr + SUNXI_SPI_TC_REG);
}

static void sunxi_spi_ss_owner(struct sunxi_spi *sspi, u32 owner)
{
	u32 reg_val = readl(sspi->base_addr + SUNXI_SPI_TC_REG);

	switch (owner) {
	case SUNXI_SPI_CS_AUTO:
		reg_val &= ~SUNXI_SPI_TC_SS_OWNER;
		break;
	case SUNXI_SPI_CS_SOFT:
		reg_val |= SUNXI_SPI_TC_SS_OWNER;
		break;
	}

	writel(reg_val, sspi->base_addr + SUNXI_SPI_TC_REG);
}

static int sunxi_spi_ss_select(struct sunxi_spi *sspi, u16 cs)
{
	int ret = 0;
	u32 reg_val;

	if (cs < SUNXI_SPI_CS_MAX) {
		reg_val = readl(sspi->base_addr + SUNXI_SPI_TC_REG);
		reg_val &= ~SUNXI_SPI_TC_SS_SEL;
		reg_val |= FIELD_PREP(SUNXI_SPI_TC_SS_SEL, cs);
		writel(reg_val, sspi->base_addr + SUNXI_SPI_TC_REG);
	} else {
		ret = -EINVAL;
	}

	return ret;
}

static void sunxi_spi_ss_ctrl(struct sunxi_spi *sspi, bool ssctl)
{
	u32 reg_val = readl(sspi->base_addr + SUNXI_SPI_TC_REG);

	if (ssctl)
		reg_val |= SUNXI_SPI_TC_SSCTL;
	else
		reg_val &= ~SUNXI_SPI_TC_SSCTL;

	writel(reg_val, sspi->base_addr + SUNXI_SPI_TC_REG);
}

static void sunxi_spi_ss_polarity(struct sunxi_spi *sspi, bool pol)
{
	u32 reg_val = readl(sspi->base_addr + SUNXI_SPI_TC_REG);

	if (pol)
		reg_val |= SUNXI_SPI_TC_SPOL;
	else
		reg_val &= ~SUNXI_SPI_TC_SPOL;

	writel(reg_val, sspi->base_addr + SUNXI_SPI_TC_REG);
}

static void sunxi_spi_config_tc(struct sunxi_spi *sspi, u32 config)
{
	u32 reg_val = readl(sspi->base_addr + SUNXI_SPI_TC_REG);

	if (config & SPI_CPOL)
		reg_val |= SUNXI_SPI_TC_CPOL;
	else
		reg_val &= ~SUNXI_SPI_TC_CPOL;

	if (config & SPI_CPHA)
		reg_val |= SUNXI_SPI_TC_CPHA;
	else
		reg_val &= ~SUNXI_SPI_TC_CPHA;

	if (config & SPI_LSB_FIRST)
		reg_val |= SUNXI_SPI_TC_FBS;
	else
		reg_val &= ~SUNXI_SPI_TC_FBS;

	writel(reg_val, sspi->base_addr + SUNXI_SPI_TC_REG);
}

static void sunxi_spi_enable_irq(struct sunxi_spi *sspi, u32 bitmap)
{
	u32 reg_val = readl(sspi->base_addr + SUNXI_SPI_INT_CTL_REG);
	bitmap &= SUNXI_SPI_INT_CTL_MASK;
	reg_val |= bitmap;
	writel(reg_val, sspi->base_addr + SUNXI_SPI_INT_CTL_REG);
}

static void sunxi_spi_disable_irq(struct sunxi_spi *sspi, u32 bitmap)
{
	u32 reg_val = readl(sspi->base_addr + SUNXI_SPI_INT_CTL_REG);
	bitmap &= SUNXI_SPI_INT_CTL_MASK;
	reg_val &= ~bitmap;
	writel(reg_val, sspi->base_addr + SUNXI_SPI_INT_CTL_REG);
}

static inline u32 sunxi_spi_qry_irq_enable(struct sunxi_spi *sspi)
{
	return (SUNXI_SPI_INT_CTL_MASK & readl(sspi->base_addr + SUNXI_SPI_INT_CTL_REG));
}

static inline u32 sunxi_spi_qry_irq_pending(struct sunxi_spi *sspi)
{
	return (SUNXI_SPI_INT_STA_MASK & readl(sspi->base_addr + SUNXI_SPI_INT_STA_REG));
}

static void sunxi_spi_clr_irq_pending(struct sunxi_spi *sspi, u32 bitmap)
{
	bitmap &= SUNXI_SPI_INT_STA_MASK;
	writel(bitmap, sspi->base_addr + SUNXI_SPI_INT_STA_REG);
}

static void sunxi_spi_reset_txfifo(struct sunxi_spi *sspi)
{
	u32 reg_val = readl(sspi->base_addr + SUNXI_SPI_FIFO_CTL_REG);
	int ret;

	reg_val |= SUNXI_SPI_FIFO_CTL_TX_RST;
	writel(reg_val, sspi->base_addr + SUNXI_SPI_FIFO_CTL_REG);

	/* Hardware will auto clear this bit when fifo reset
	 * Before return, driver must wait reset opertion complete
	 */
	ret = readl_poll_timeout_atomic(sspi->base_addr + SUNXI_SPI_FIFO_CTL_REG, reg_val, !(reg_val & SUNXI_SPI_FIFO_CTL_TX_RST), 5, 1000);
	if (ret) {
		sunxi_err(sspi->dev, "timeout for waiting txfifo reset (%#x)\n", reg_val);
		return ;
	}
}

static void sunxi_spi_reset_rxfifo(struct sunxi_spi *sspi)
{
	u32 reg_val = readl(sspi->base_addr + SUNXI_SPI_FIFO_CTL_REG);
	int ret;

	reg_val |= SUNXI_SPI_FIFO_CTL_RX_RST;
	writel(reg_val, sspi->base_addr + SUNXI_SPI_FIFO_CTL_REG);

	/* Hardware will auto clear this bit when fifo reset
	 * Before return, driver must wait reset opertion complete
	 */
	ret = readl_poll_timeout_atomic(sspi->base_addr + SUNXI_SPI_FIFO_CTL_REG, reg_val, !(reg_val & SUNXI_SPI_FIFO_CTL_RX_RST), 5, 1000);
	if (ret) {
		sunxi_err(sspi->dev, "timeout for waiting rxfifo reset (%#x)\n", reg_val);
		return ;
	}
}

static inline void sunxi_spi_reset_fifo(struct sunxi_spi *sspi)
{
	sunxi_spi_reset_txfifo(sspi);
	sunxi_spi_reset_rxfifo(sspi);
}

static void sunxi_spi_enable_dma_irq(struct sunxi_spi *sspi, u32 bitmap)
{
	u32 reg_val = readl(sspi->base_addr + SUNXI_SPI_FIFO_CTL_REG);
	bitmap &= SUNXI_SPI_FIFO_CTL_DRQ_EN;
	reg_val |= bitmap;
	writel(reg_val, sspi->base_addr + SUNXI_SPI_FIFO_CTL_REG);
}

static void sunxi_spi_disable_dma_irq(struct sunxi_spi *sspi, u32 bitmap)
{
	u32 reg_val = readl(sspi->base_addr + SUNXI_SPI_FIFO_CTL_REG);
	bitmap &= SUNXI_SPI_FIFO_CTL_DRQ_EN;
	reg_val &= ~bitmap;
	writel(reg_val, sspi->base_addr + SUNXI_SPI_FIFO_CTL_REG);
}

static void sunxi_spi_set_fifo_trig_level_rx(struct sunxi_spi *sspi, u32 level)
{
	u32 reg_val = readl(sspi->base_addr + SUNXI_SPI_FIFO_CTL_REG);
	reg_val &= ~SUNXI_SPI_FIFO_CTL_RX_TRIG_LEVEL;
	reg_val |= FIELD_PREP(SUNXI_SPI_FIFO_CTL_RX_TRIG_LEVEL, level);
	writel(reg_val, sspi->base_addr + SUNXI_SPI_FIFO_CTL_REG);
}

static void sunxi_spi_set_fifo_trig_level_tx(struct sunxi_spi *sspi, u32 level)
{
	u32 reg_val = readl(sspi->base_addr + SUNXI_SPI_FIFO_CTL_REG);
	reg_val &= ~SUNXI_SPI_FIFO_CTL_TX_TRIG_LEVEL;
	reg_val |= FIELD_PREP(SUNXI_SPI_FIFO_CTL_TX_TRIG_LEVEL, level);
	writel(reg_val, sspi->base_addr + SUNXI_SPI_FIFO_CTL_REG);
}

static u32 sunxi_spi_get_txbuf_cnt(struct sunxi_spi *sspi)
{
	u32 reg_val, ret;

	if (sspi->data->quirk_flag & INDEPENDENT_BSR) {
		reg_val = readl(sspi->base_addr + SUNXI_SPI_BUF_STA_REG);
		ret = FIELD_GET(SUNXI_SPI_BUF_STA_TB_CNT, reg_val);
	} else {
		reg_val = readl(sspi->base_addr + SUNXI_SPI_FIFO_STA_REG);
		ret = FIELD_GET(SUNXI_SPI_FIFO_STA_TB_CNT, reg_val);
	}

	return ret;
}

static inline u32 sunxi_spi_get_txfifo_cnt(struct sunxi_spi *sspi)
{
	return FIELD_GET(SUNXI_SPI_FIFO_STA_TX_CNT, readl(sspi->base_addr + SUNXI_SPI_FIFO_STA_REG));
}

static u32 sunxi_spi_get_rxbuf_cnt(struct sunxi_spi *sspi)
{
	u32 reg_val, ret;

	if (sspi->data->quirk_flag & INDEPENDENT_BSR) {
		reg_val = readl(sspi->base_addr + SUNXI_SPI_BUF_STA_REG);
		ret = FIELD_GET(SUNXI_SPI_BUF_STA_RB_CNT, reg_val);
	} else {
		reg_val = readl(sspi->base_addr + SUNXI_SPI_FIFO_STA_REG);
		ret = FIELD_GET(SUNXI_SPI_FIFO_STA_RB_CNT, reg_val);
	}

	return ret;
}

static inline u32 sunxi_spi_get_rxfifo_cnt(struct sunxi_spi *sspi)
{
	return FIELD_GET(SUNXI_SPI_FIFO_STA_RX_CNT, readl(sspi->base_addr + SUNXI_SPI_FIFO_STA_REG));
}

static void sunxi_spi_set_sample_delay_sw(struct sunxi_spi *sspi, u32 status)
{
	u32 reg_val = readl(sspi->base_addr + SUNXI_SPI_SAMP_DL_REG);

	if (status)
		reg_val |= SUNXI_SPI_SAMP_DL_SW_EN;
	else
		reg_val &= ~SUNXI_SPI_SAMP_DL_SW_EN;

	writel(reg_val, sspi->base_addr + SUNXI_SPI_SAMP_DL_REG);
}

static void sunxi_spi_set_sample_delay(struct sunxi_spi *sspi, u32 sample_delay)
{
	u32 reg_val = readl(sspi->base_addr + SUNXI_SPI_SAMP_DL_REG);
	reg_val &= ~SUNXI_SPI_SAMP_DL_SW;
	reg_val |= FIELD_PREP(SUNXI_SPI_SAMP_DL_SW, sample_delay);
	writel(reg_val, sspi->base_addr + SUNXI_SPI_SAMP_DL_REG);
}

static void sunxi_spi_set_bc_tc_stc(struct sunxi_spi *sspi, u32 tx_len, u32 rx_len, u32 stc_len, u32 dummy_cnt)
{
	u32 reg_val;

	/* set total burst number into MBC register */
	reg_val = readl(sspi->base_addr + SUNXI_SPI_MBC_REG);
	reg_val &= ~SUNXI_SPI_MBC;
	reg_val |= FIELD_PREP(SUNXI_SPI_MBC, tx_len + rx_len + dummy_cnt);
	writel(reg_val, sspi->base_addr + SUNXI_SPI_MBC_REG);

	/* set write transmit counter into MTC register */
	reg_val = readl(sspi->base_addr + SUNXI_SPI_MTC_REG);
	reg_val &= ~SUNXI_SPI_MWTC;
	reg_val |= FIELD_PREP(SUNXI_SPI_MWTC, tx_len);
	writel(reg_val, sspi->base_addr + SUNXI_SPI_MTC_REG);

	/* set dummy burst counter and single mode transmit counter into BCC register */
	reg_val = readl(sspi->base_addr + SUNXI_SPI_BCC_REG);
	reg_val &= ~SUNXI_SPI_BCC_STC;
	reg_val |= FIELD_PREP(SUNXI_SPI_BCC_STC, stc_len);
	reg_val &= ~SUNXI_SPI_BCC_DBC;
	reg_val |= FIELD_PREP(SUNXI_SPI_BCC_DBC, dummy_cnt);
	writel(reg_val, sspi->base_addr + SUNXI_SPI_BCC_REG);
}

static void sunxi_spi_enable_dual(struct sunxi_spi *sspi)
{
	u32 reg_val = readl(sspi->base_addr + SUNXI_SPI_BCC_REG);
	reg_val |= SUNXI_SPI_BCC_DRM;
	writel(reg_val, sspi->base_addr + SUNXI_SPI_BCC_REG);
}

static void sunxi_spi_disable_dual(struct sunxi_spi *sspi)
{
	u32 reg_val = readl(sspi->base_addr + SUNXI_SPI_BCC_REG);
	reg_val &= ~SUNXI_SPI_BCC_DRM;
	writel(reg_val, sspi->base_addr + SUNXI_SPI_BCC_REG);
}

static void sunxi_spi_enable_quad(struct sunxi_spi *sspi)
{
	u32 reg_val = readl(sspi->base_addr + SUNXI_SPI_BCC_REG);
	reg_val |= SUNXI_SPI_BCC_QUAD_EN;
	writel(reg_val, sspi->base_addr + SUNXI_SPI_BCC_REG);
}

static void sunxi_spi_disable_quad(struct sunxi_spi *sspi)
{
	u32 reg_val = readl(sspi->base_addr + SUNXI_SPI_BCC_REG);
	reg_val &= ~SUNXI_SPI_BCC_QUAD_EN;
	writel(reg_val, sspi->base_addr + SUNXI_SPI_BCC_REG);
}

static u32 sunxi_spi_get_tx_shift_buf_cnt(struct sunxi_spi *sspi)
{
	u32 reg_val;
	u32 cnt = 0;

	if (sspi->data->quirk_flag & INDEPENDENT_BSR) {
		reg_val = readl(sspi->base_addr + SUNXI_SPI_BUF_STA_REG);
		cnt = FIELD_GET(SUNXI_SPI_BUF_STA_TX_SHIFT_CNT, reg_val);
	}

	return cnt;
}

static void sunxi_spi_soft_reset(struct sunxi_spi *sspi)
{
	u32 reg_val = readl(sspi->base_addr + SUNXI_SPI_GC_REG);
	int ret;

	reg_val |= SUNXI_SPI_GC_SRST;
	writel(reg_val, sspi->base_addr + SUNXI_SPI_GC_REG);

	/* Hardware will auto clear this bit when soft reset
	 * Before return, driver must wait reset opertion complete */
	ret = readl_poll_timeout_atomic(sspi->base_addr + SUNXI_SPI_GC_REG, reg_val, !(reg_val & SUNXI_SPI_GC_SRST), 5, 1000);
	if (ret) {
		sunxi_err(sspi->dev, "timeout for waiting soft reset (%#x)\n", reg_val);
		return ;
	}

	if ((sspi->data->quirk_flag & NEW_SAMPLE_MODE) && (sspi->data->quirk_flag & NEW_SAMPLE_MODE_RST)) {
		u32 wait_cycle = max(SUNXI_SPI_MAX_FREQUENCY / sspi->bus_freq, SUNXI_SPI_MAX_FREQUENCY / sspi->pre_speed_hz);
		/* Wait at least 32 clk cycle after srst for invisible fifo synchronized into rxfifo
		 * Max 100MHz is 10ns per cycle
		 */
		ndelay(10 * (wait_cycle + 1) * 32);
		if (sunxi_spi_get_rxfifo_cnt(sspi) > 0) {
			sunxi_spi_reset_fifo(sspi);
			sunxi_debug(sspi->dev, "get quirk %#x and fixed\n", NEW_SAMPLE_MODE_RST);
		}
	}
}

/* SPI Controller Hardware Register Operation End */

static void sunxi_spi_delay_chain_init(struct sunxi_spi *sspi)
{
	if ((sspi->bus_mode & (SUNXI_SPI_BUS_MASTER | SUNXI_SPI_BUS_NOR | SUNXI_SPI_BUS_NAND)) &&
		(sspi->data->quirk_flag & NEW_SAMPLE_MODE)) {
		sunxi_spi_bus_sample_mode(sspi, SUNXI_SPI_SAMP_MODE_NEW);
		if (sspi->bus_sample_mode == SUNXI_SPI_SAMP_MODE_MANUAL) {
			sunxi_spi_set_sample_mode(sspi, sspi->spi_sample_mode);
			if (sspi->spi_sample_delay) {
				sunxi_spi_set_sample_delay_sw(sspi, true);
				sunxi_spi_set_sample_delay(sspi, sspi->spi_sample_delay);
			}
		}
	} else {
		sunxi_spi_bus_sample_mode(sspi, SUNXI_SPI_SAMP_MODE_OLD);
		if (sspi->bus_sample_mode == SUNXI_SPI_SAMP_MODE_MANUAL)
			sunxi_spi_set_sample_mode(sspi, sspi->spi_sample_mode);
	}
}

static void sunxi_spi_set_delay_chain(struct sunxi_spi *sspi, u32 clk)
{
	switch (sspi->bus_sample_mode) {
	case SUNXI_SPI_SAMP_MODE_AUTO:
		if ((sspi->bus_mode & (SUNXI_SPI_BUS_MASTER | SUNXI_SPI_BUS_NOR | SUNXI_SPI_BUS_NAND)) &&
			(sspi->data->quirk_flag & NEW_SAMPLE_MODE)) {
			if (clk >= SUNXI_SPI_SAMP_HIGH_FREQ)
				sspi->spi_sample_mode = SUNXI_SPI_SAMP_DELAY_CYCLE_0_5;
			else
				sspi->spi_sample_mode = SUNXI_SPI_SAMP_DELAY_CYCLE_0_0;
		} else {
			if (clk >= SUNXI_SPI_SAMP_HIGH_FREQ)
				sspi->spi_sample_mode = SUNXI_SPI_SAMP_DELAY_CYCLE_1_0;
			else if (clk <= SUNXI_SPI_SAMP_LOW_FREQ)
				sspi->spi_sample_mode = SUNXI_SPI_SAMP_DELAY_CYCLE_0_0;
			else
				sspi->spi_sample_mode = SUNXI_SPI_SAMP_DELAY_CYCLE_0_5;
		}
		sunxi_spi_set_sample_mode(sspi, sspi->spi_sample_mode);
		break;
	case SUNXI_SPI_SAMP_MODE_MANUAL:
		break;
	default:
		sunxi_err(sspi->dev, "unsupport sample mode %d\n", sspi->bus_sample_mode);
	}
}

static int sunxi_spi_set_clk(struct sunxi_spi *sspi, u32 clk)
{
	u32 new_clk;
	u32 old_clk = clk_get_rate(sspi->mclk);

	new_clk = clk_round_rate(sspi->mclk, clk);
	if (new_clk != clk) {
		sunxi_warn(sspi->dev, "clk %d not support, round to nearly %d\n", clk, new_clk);
	}

	if ((new_clk > sspi->ctlr->max_speed_hz) || (new_clk < sspi->ctlr->min_speed_hz)) {
		sunxi_err(sspi->dev, "set clk freq %d not support", new_clk);
		return -EINVAL;
	}

	if (clk_set_rate(sspi->mclk, new_clk)) {
		sunxi_err(sspi->dev, "set clk freq %d failed\n", new_clk);
		clk_set_rate(sspi->mclk, old_clk);
		return -EINVAL;
	}

	sunxi_debug(sspi->dev, "set clk freq %d success\n", new_clk);

	return new_clk;
}

static void sunxi_spi_set_ready_status(struct sunxi_spi *sspi, bool status)
{
	if (sspi->ready_gpio > 0)
		gpio_set_value(sspi->ready_gpio, status);
}

static int sunxi_spi_resource_get(struct sunxi_spi *sspi)
{
	struct device_node *np = sspi->pdev->dev.of_node;
	int ret = 0;

	sspi->bus_num = of_alias_get_id(np, "spi");
	if (sspi->bus_num < 0) {
		sunxi_err(sspi->dev, "failed to get alias id\n");
		ret = -EINVAL;
		goto out;
	}
	sspi->pdev->id = sspi->bus_num;

	sspi->mem_res = platform_get_resource(sspi->pdev, IORESOURCE_MEM, 0);
	if (IS_ERR_OR_NULL(sspi->mem_res)) {
		sunxi_err(sspi->dev, "failed to get mem resource\n");
		ret = -EINVAL;
		goto out;
	}

	sspi->irq = platform_get_irq(sspi->pdev, 0);
	if (sspi->irq < 0) {
		ret = sspi->irq;
		goto out;
	}

#if IS_ENABLED(CONFIG_ARCH_SUN55IW3)
	/* For this platform, R_SPI use DSP_DMA port as default.
	 * If need use SYS_DMA, pls. change the select port reg. under PRCM.
	 * Other SPI port can only use SYS_DMA, no need to change.
	 */
	if (of_property_read_bool(np, "sunxi,rspi-use-sys-dma")) {
		sspi->dma_sel_base = devm_ioremap(sspi->dev, sspi->data->dma_sel_addr + sspi->data->dma_sel_offset, 4);
		if (IS_ERR_OR_NULL(sspi->dma_sel_base)) {
			sunxi_err(sspi->dev, "failed to get dma select resource\n");
			ret = -EINVAL;
			goto out;
		}
		/* R_SPI DMA SEL : 0-DSP_DMA(default) 1-SYS_DMA */
		writel(readl(sspi->dma_sel_base) | (1 << sspi->data->dma_sel_bit), sspi->dma_sel_base);
		devm_iounmap(sspi->dev, sspi->dma_sel_base);
	}
#endif

	ret = of_property_read_u32(np, "clock-frequency", &sspi->bus_freq);
	if (ret) {
		sspi->bus_freq = SUNXI_SPI_MAX_FREQUENCY;
		sunxi_warn(sspi->dev, "failed to get clock-frequency property, use default value %d\n", sspi->bus_freq);
	} else if (sspi->bus_freq > SUNXI_SPI_MAX_FREQUENCY || sspi->bus_freq < SUNXI_SPI_MIN_FREQUENCY) {
		sspi->bus_freq = SUNXI_SPI_MAX_FREQUENCY;
		sunxi_warn(sspi->dev, "frequency no in range, use default value %d\n", sspi->bus_freq);
	}

	ret = of_property_read_u32(np, "sunxi,spi-num-cs", &sspi->cs_num);
	if (ret) {
		sspi->cs_num = SUNXI_SPI_CS_MAX;
		sunxi_warn(sspi->dev, "failed to get sunxi,spi-num-cs property, use default value %d\n", sspi->cs_num);
	} else if (sspi->cs_num < 0 || sspi->cs_num > SUNXI_SPI_CS_MAX) {
		sspi->cs_num = SUNXI_SPI_CS_MAX;
		sunxi_warn(sspi->dev, "cs number no in range, use default value %d\n", sspi->cs_num);
	}

	ret = of_property_read_u32(np, "sunxi,spi-bus-mode", &sspi->bus_mode);
	if (ret) {
		sspi->bus_mode = SUNXI_SPI_BUS_MASTER;
		sunxi_warn(sspi->dev, "failed to get sunxi,spi-bus-mode property, use default value %d\n", sspi->bus_mode);
	}

	ret = of_property_read_u32(np, "sunxi,spi-cs-mode", &sspi->cs_mode);
	if (ret) {
		sspi->cs_mode = SUNXI_SPI_CS_AUTO;
		sunxi_warn(sspi->dev, "failed to get sunxi,spi-cs-mode property, use default value %d\n", sspi->cs_mode);
	}

	if (sspi->bus_mode & sspi->data->hw_bus_mode) {
		if (sspi->bus_mode == SUNXI_SPI_BUS_SLAVE && !IS_ENABLED(CONFIG_SPI_SLAVE)) {
			sunxi_err(sspi->dev, "CONFIG_SPI_SLAVE not enable under slave mode\n");
			ret = -EINVAL;
			goto out;
		} else if (sspi->bus_mode == SUNXI_SPI_BUS_NOR || sspi->bus_mode == SUNXI_SPI_BUS_NAND) {
			if (sspi->cs_mode != SUNXI_SPI_CS_SOFT) {
				sspi->cs_mode = SUNXI_SPI_CS_SOFT;
				sunxi_info(sspi->dev, "bus in flash mode, cs force use software control\n");
			}
		}
	} else {
		sunxi_err(sspi->dev, "unsupport hw bus mode %#x\n", sspi->bus_mode);
		ret = -EINVAL;
		goto out;
	}

	ret = of_property_read_u32(np, "sample_mode", &sspi->spi_sample_mode);
	if (ret)
		sspi->spi_sample_mode = SUNXI_SPI_SAMP_MODE_DL_DEFAULT;
	ret = of_property_read_u32(np, "sample_delay", &sspi->spi_sample_delay);
	if (ret)
		sspi->spi_sample_delay = SUNXI_SPI_SAMP_MODE_DL_DEFAULT;
	if (sspi->spi_sample_mode == SUNXI_SPI_SAMP_MODE_DL_DEFAULT && sspi->spi_sample_delay == SUNXI_SPI_SAMP_MODE_DL_DEFAULT) {
		sspi->bus_sample_mode = SUNXI_SPI_SAMP_MODE_AUTO;
		sspi->spi_sample_mode = sspi->spi_sample_delay = 0;
	} else {
		sspi->bus_sample_mode = SUNXI_SPI_SAMP_MODE_MANUAL;
		if ((sspi->bus_mode & (SUNXI_SPI_BUS_MASTER | SUNXI_SPI_BUS_NOR | SUNXI_SPI_BUS_NAND)) &&
			(sspi->data->quirk_flag & NEW_SAMPLE_MODE)) {
			if (sspi->spi_sample_mode > SUNXI_SPI_SAMP_DELAY_CYCLE_3_0) {
				sunxi_warn(sspi->dev, "sample mode %d over new delay cycle\n", sspi->spi_sample_mode);
				sspi->spi_sample_mode = SUNXI_SPI_SAMP_DELAY_CYCLE_0_0;
			}
			if (sspi->spi_sample_delay > SUNXI_SPI_SAMPLE_DELAY_CHAIN_MAX) {
				sunxi_warn(sspi->dev, "sample delay %d over new delay chain\n", sspi->spi_sample_delay);
				sspi->spi_sample_delay = SUNXI_SPI_SAMPLE_DELAY_CHAIN_MIN;
			}
		} else {
			if (sspi->spi_sample_mode > SUNXI_SPI_SAMP_DELAY_CYCLE_1_0) {
				sunxi_warn(sspi->dev, "sample mode %d over old delay cycle\n", sspi->spi_sample_mode);
				sspi->spi_sample_mode = SUNXI_SPI_SAMP_DELAY_CYCLE_0_0;
			}
			sspi->spi_sample_delay = 0;
		}
	}

	/* ioremap */
	sspi->base_addr = devm_ioremap_resource(sspi->dev, sspi->mem_res);
	if (IS_ERR_OR_NULL(sspi->base_addr)) {
		ret = PTR_ERR(sspi->base_addr);
		sunxi_err(sspi->dev, "failed to ioremap resource %d\n", ret);
		goto out;
	}
	sspi->base_addr_phy = sspi->mem_res->start;

	/* regulator */
	sspi->regulator = devm_regulator_get(sspi->dev, "spi");
	if (IS_ERR(sspi->regulator)) {
		ret = PTR_ERR(sspi->regulator);
		sunxi_err(sspi->dev, "failed to get regulator %d\n", ret);
		goto out;
	}

	/* clock */
	sspi->pclk = devm_clk_get(sspi->dev, "pll");
	if (IS_ERR_OR_NULL(sspi->pclk)) {
		ret = PTR_ERR(sspi->pclk);
		sunxi_err(sspi->dev, "failed to get pll clk %d\n", ret);
		goto out;
	}
	sspi->mclk = devm_clk_get(sspi->dev, "mod");
	if (IS_ERR_OR_NULL(sspi->mclk)) {
		ret = PTR_ERR(sspi->mclk);
		sunxi_err(sspi->dev, "failed to get mod clk %d\n", ret);
		goto out;
	}
	sspi->bus_clk = devm_clk_get(sspi->dev, "bus");
	if (IS_ERR_OR_NULL(sspi->bus_clk)) {
		ret = PTR_ERR(sspi->bus_clk);
		sunxi_err(sspi->dev, "failed to get bus clk %d\n", ret);
		goto out;
	}

	/* reset */
	sspi->reset = devm_reset_control_get(sspi->dev, NULL);
	if (IS_ERR_OR_NULL(sspi->reset)) {
		ret = PTR_ERR(sspi->reset);
		sunxi_err(sspi->dev, "failed to get reset control %d\n", ret);
		goto out;
	}

	/* gpio */
	sspi->ready_gpio = of_get_named_gpio(np, "ready-gpio", 0);
	if (sspi->ready_gpio > 0) {
		sspi->ready_irq = gpio_to_irq(sspi->ready_gpio);
		sspi->ready_label = devm_kasprintf(sspi->dev, GFP_KERNEL, "%s ready", dev_name(sspi->dev));
		sunxi_info(sspi->dev, "get ready gpio %d\n", sspi->ready_gpio);
	}

	/* resource data dump */
	sunxi_info(sspi->dev, "bus num_%d mode_%d freq_%d\n", sspi->bus_num, sspi->bus_mode, sspi->bus_freq);
	sunxi_info(sspi->dev, "cs num_%d mode_%d\n", sspi->cs_num, sspi->cs_mode);
	if (sspi->bus_sample_mode == SUNXI_SPI_SAMP_MODE_MANUAL)
		sunxi_info(sspi->dev, "spi manual set sample mode_%d, delay_%d\n", sspi->spi_sample_mode, sspi->spi_sample_delay);

	return 0;

out:
	return ret;
}

static int sunxi_spi_request_dma(struct sunxi_spi *sspi)
{
	int ret = 0;

	sspi->ctlr->dma_tx = dma_request_chan(sspi->dev, "tx");
	if (IS_ERR_OR_NULL(sspi->ctlr->dma_tx)) {
		ret = PTR_ERR(sspi->ctlr->dma_tx);
		sunxi_err(sspi->dev, "failed to request dma tx channel %d\n", ret);
		goto err0;
	}

	sspi->ctlr->dma_rx = dma_request_chan(sspi->dev, "rx");
	if (IS_ERR_OR_NULL(sspi->ctlr->dma_rx)) {
		ret = PTR_ERR(sspi->ctlr->dma_rx);
		sunxi_err(sspi->dev, "failed to request dma rx channel %d\n", ret);
		goto err1;
	}

	sunxi_debug(sspi->dev, "request dma channel %s(tx) and %s(rx) success\n",
			dma_chan_name(sspi->ctlr->dma_tx), dma_chan_name(sspi->ctlr->dma_rx));

	return 0;

err1:
	dma_release_channel(sspi->ctlr->dma_tx);
err0:
	return ret;
}

static int sunxi_spi_release_dma(struct sunxi_spi *sspi)
{
	if (sspi->ctlr->dma_tx)
		dma_release_channel(sspi->ctlr->dma_tx);
	if (sspi->ctlr->dma_rx)
		dma_release_channel(sspi->ctlr->dma_rx);

	return 0;
}

static int sunxi_spi_bus_setup(struct spi_device *spi, struct spi_transfer *t)
{
	struct sunxi_spi *sspi = spi_controller_get_devdata(spi->controller);
	u32 spi_speed_hz = min(spi->max_speed_hz, sspi->ctlr->max_speed_hz);
	int ret = 0;

	if (t && t->speed_hz) {
		spi_speed_hz = t->speed_hz;

		spi_speed_hz = max(spi_speed_hz, sspi->ctlr->min_speed_hz);
		spi_speed_hz = min(spi_speed_hz, sspi->ctlr->max_speed_hz);
		if (spi_speed_hz != t->speed_hz)
			sunxi_warn(sspi->dev, "speed %d not in range\n", t->speed_hz);
	}

	if (sspi->pre_speed_hz != spi_speed_hz) {
		switch (sspi->bus_mode) {
		case SUNXI_SPI_BUS_MASTER:
		case SUNXI_SPI_BUS_NOR:
		case SUNXI_SPI_BUS_NAND:
			sunxi_spi_set_delay_chain(sspi, spi_speed_hz);
			break;
		case SUNXI_SPI_BUS_BIT:
			sunxi_spi_bit_sample_delay(sspi, spi_speed_hz);
			break;
		default:
			break;
		}

		if (!spi_controller_is_slave(sspi->ctlr)) {
			spi_speed_hz = sunxi_spi_set_clk(sspi, spi_speed_hz);
			if (spi_speed_hz > 0)
				sspi->pre_speed_hz = spi_speed_hz;
		}
	}

	return ret;
}

static int sunxi_spi_xfer_setup(struct spi_device *spi, struct spi_transfer *t)
{
	struct sunxi_spi *sspi = spi_controller_get_devdata(spi->controller);
	int ret = 0;

	ret = sunxi_spi_bus_setup(spi, t);

	switch (sspi->bus_mode) {
	case SUNXI_SPI_BUS_DBI:
		sspi->dbi_dev = spi;
	fallthrough;
	case SUNXI_SPI_BUS_MASTER:
	case SUNXI_SPI_BUS_NOR:
	case SUNXI_SPI_BUS_NAND:
	case SUNXI_SPI_BUS_SLAVE:
		sunxi_spi_config_tc(sspi, spi->mode);
		break;
	case SUNXI_SPI_BUS_BIT:
		sunxi_spi_bit_config_tc(sspi, spi->mode);
		break;
	default:
		sunxi_err(sspi->dev, "setup bus mode %d unsupport\n", sspi->bus_mode);
		ret = -EINVAL;
	}

	return ret;
}

static int sunxi_spi_setup(struct spi_device *spi)
{
	return sunxi_spi_xfer_setup(spi, NULL);
}

static void sunxi_spi_set_cs(struct spi_device *spi, bool status)
{
	struct sunxi_spi *sspi = spi_controller_get_devdata(spi->controller);
	int ret;

	ret = sunxi_spi_ss_select(sspi, spi->chip_select);
	if (ret < 0) {
		sunxi_warn(sspi->dev, "cs %d over range, need control by software\n", spi->chip_select);
		return ;
	}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 18, 0))
	if (spi->cs_gpiod)
#else
	if (spi->cs_gpiod || gpio_is_valid(spi->cs_gpio))
#endif
		return ;

	sunxi_spi_ss_polarity(sspi, !(spi->mode & SPI_CS_HIGH));

	if (sspi->cs_mode == SUNXI_SPI_CS_SOFT)
		sunxi_spi_ss_level(sspi, status);
}

static int sunxi_spi_prepare_message(struct spi_controller *ctlr, struct spi_message *message)
{
	struct sunxi_spi *sspi = spi_controller_get_devdata(ctlr);

	if (spi_controller_is_slave(ctlr)) {
		if (sspi->ready_gpio > 0) {
			/* delay some times to make sure gpio irq can detect the reverse */
			sunxi_spi_set_ready_status(sspi, 0);
			udelay(100);
		}
		sunxi_spi_enable_bus(sspi);
	}

	return 0;
}

static int sunxi_spi_unprepare_message(struct spi_controller *ctlr, struct spi_message *message)
{
	struct sunxi_spi *sspi = spi_controller_get_devdata(ctlr);

	if (spi_controller_is_slave(ctlr)) {
		sunxi_spi_disable_bus(sspi);
		if (sspi->ready_gpio > 0) {
			/* delay some times to make sure gpio irq can detect the reverse */
			udelay(100);
			sunxi_spi_set_ready_status(sspi, 0);
		}
	}

	return 0;
}

static int sunxi_spi_slave_abort(struct spi_controller *ctlr)
{
	struct sunxi_spi *sspi = spi_controller_get_devdata(ctlr);

	sspi->slave_aborted = true;
	complete(&sspi->done);

	return 0;
}

static void sunxi_spi_handle_err(struct spi_controller *ctlr, struct spi_message *message)
{
	struct sunxi_spi *sspi = spi_controller_get_devdata(ctlr);

	sunxi_spi_reset_fifo(sspi);
	sunxi_spi_soft_reset(sspi);
}

static bool sunxi_spi_can_dma(struct spi_controller *ctlr, struct spi_device *spi, struct spi_transfer *xfer)
{
	struct sunxi_spi *sspi = spi_controller_get_devdata(ctlr);

	return !!(sspi->use_dma && xfer->len >= min(sspi->data->rx_fifosize, sspi->data->tx_fifosize));
}

static int sunxi_spi_mode_check_master(struct sunxi_spi *sspi, struct spi_transfer *t)
{
	if (t->tx_buf && t->rx_buf) {
		/* full duplex */
		sunxi_spi_set_discard_burst(sspi, false);
		sunxi_spi_disable_quad(sspi);
		sunxi_spi_disable_dual(sspi);
		sunxi_spi_set_bc_tc_stc(sspi, t->len, 0, t->len, 0);
		sspi->mode_type = SINGLE_FULL_DUPLEX_TX_RX;
	} else {
		/* half duplex transmit */
		sunxi_spi_set_discard_burst(sspi, true);
		if (t->tx_buf) {
			switch (t->tx_nbits) {
			case SPI_NBITS_QUAD:
				sunxi_spi_disable_dual(sspi);
				sunxi_spi_enable_quad(sspi);
				sunxi_spi_set_bc_tc_stc(sspi, t->len, 0, 0, 0);
				sspi->mode_type = QUAD_HALF_DUPLEX_TX;
				break;
			case SPI_NBITS_DUAL:
				sunxi_spi_disable_quad(sspi);
				sunxi_spi_enable_dual(sspi);
				sunxi_spi_set_bc_tc_stc(sspi, t->len, 0, 0, 0);
				sspi->mode_type = DUAL_HALF_DUPLEX_TX;
				break;
			default:
				sunxi_spi_disable_quad(sspi);
				sunxi_spi_disable_dual(sspi);
				sunxi_spi_set_bc_tc_stc(sspi, t->len, 0, t->len, 0);
				sspi->mode_type = SINGLE_HALF_DUPLEX_TX;
			}
		} else if (t->rx_buf) {
			switch (t->rx_nbits) {
			case SPI_NBITS_QUAD:
				sunxi_spi_disable_dual(sspi);
				sunxi_spi_enable_quad(sspi);
				sunxi_spi_set_bc_tc_stc(sspi, 0, t->len, 0, 0);
				sspi->mode_type = QUAD_HALF_DUPLEX_RX;
				break;
			case SPI_NBITS_DUAL:
				sunxi_spi_disable_quad(sspi);
				sunxi_spi_enable_dual(sspi);
				sunxi_spi_set_bc_tc_stc(sspi, 0, t->len, 0, 0);
				sspi->mode_type = DUAL_HALF_DUPLEX_RX;
				break;
			default:
				sunxi_spi_disable_quad(sspi);
				sunxi_spi_disable_dual(sspi);
				sunxi_spi_set_bc_tc_stc(sspi, 0, t->len, 0, 0);
				sspi->mode_type = SINGLE_HALF_DUPLEX_RX;
			}
		}
	}
	sunxi_debug(sspi->dev, "master mode type %d\n", sspi->mode_type);

	return 0;
}

static int sunxi_spi_mode_check_slave(struct sunxi_spi *sspi, struct spi_transfer *t)
{
	int ret = 0;

	if (t->tx_buf && t->rx_buf) {
		sunxi_err(sspi->dev, "slave mode not support full duplex mode\n");
		ret = -EINVAL;
	} else {
		sunxi_spi_set_discard_burst(sspi, true);
		sunxi_spi_disable_quad(sspi);
		sunxi_spi_disable_dual(sspi);
		sunxi_spi_set_bc_tc_stc(sspi, 0, 0, 0, 0);
		if (t->tx_buf) {
			sspi->mode_type = SINGLE_HALF_DUPLEX_TX;
		} else if (t->rx_buf) {
			sspi->mode_type = SINGLE_HALF_DUPLEX_RX;
		}
		sunxi_debug(sspi->dev, "slave mode type %d\n", sspi->mode_type);
	}

	return ret;
}

static int sunxi_spi_mode_check(struct sunxi_spi *sspi, struct spi_transfer *t)
{
	int ret = 0;

	if (sspi->mode_type != MODE_TYPE_NULL)
		return -EINVAL;

	switch (sspi->bus_mode) {
	case SUNXI_SPI_BUS_BIT:
		ret = sunxi_spi_mode_check_bit(sspi, t);
		break;
	default:
		if (spi_controller_is_slave(sspi->ctlr)) {
			ret = sunxi_spi_mode_check_slave(sspi, t);
		} else {
			ret = sunxi_spi_mode_check_master(sspi, t);
		}
	}

	return ret;
}

static int sunxi_spi_cpu_rx(struct sunxi_spi *sspi, struct spi_transfer *t)
{
	unsigned rx_len = t->len;
	u8 *rx_buf = (u8 *)t->rx_buf;
	int poll_time = SUNXI_SPI_POLL_TIMEOUT;

	while (rx_len && poll_time) {
		if (sunxi_spi_get_rxfifo_cnt(sspi)) {
			*rx_buf++ =  readb(sspi->base_addr + SUNXI_SPI_RXDATA_REG);
			--rx_len;
			poll_time = SUNXI_SPI_POLL_TIMEOUT;
		} else {
			--poll_time;
		}
	}

	if (poll_time <= 0) {
		sunxi_err(sspi->dev, "cpu receive data time out\n");
		sspi->result = -1;
		return -ETIME;
	}

	return 0;
}

static int sunxi_spi_cpu_tx(struct sunxi_spi *sspi, struct spi_transfer *t)
{
	unsigned tx_len = t->len;
	u8 *tx_buf = (u8 *)t->tx_buf;
	int poll_time = SUNXI_SPI_POLL_TIMEOUT;
	unsigned long flags = 0;

	raw_spin_lock_irqsave(&sspi->lock, flags);

	while (tx_len && poll_time) {
		if (sunxi_spi_get_txfifo_cnt(sspi) >= sspi->data->tx_fifosize) {
			--poll_time;
		} else {
			writeb(*tx_buf++, sspi->base_addr + SUNXI_SPI_TXDATA_REG);
			--tx_len;
			poll_time = SUNXI_SPI_POLL_TIMEOUT;
		}
	}

	raw_spin_unlock_irqrestore(&sspi->lock, flags);

	if (poll_time <= 0) {
		sunxi_err(sspi->dev, "cpu transfer data time out\n");
		sspi->result = -1;
		return -ETIME;
	}

	return 0;
}

static int sunxi_spi_cpu_tx_rx(struct sunxi_spi *sspi, struct spi_transfer *t)
{
	unsigned len = t->len;
	u8 *rx_buf = (u8 *)t->rx_buf;
	u8 *tx_buf = (u8 *)t->tx_buf;
	u32 align_loop = 0, left_loop = 0;
	u32 i = 0;
	u8 fifosize;
	int ret = 0;

	fifosize = min(sspi->data->rx_fifosize, sspi->data->tx_fifosize);

	align_loop = len / fifosize;
	left_loop = len % fifosize;

	if (align_loop) {
		t->len = fifosize;
		for (i = 0; i < align_loop; i++) {
			ret = sunxi_spi_cpu_tx(sspi, t);
			if (ret)
				goto out;
			ret = sunxi_spi_cpu_rx(sspi, t);
			if (ret)
				goto out;

			t->tx_buf += fifosize;
			t->rx_buf += fifosize;
		}
	}

	if (left_loop) {
		t->len = left_loop;
		ret = sunxi_spi_cpu_tx(sspi, t);
		if (ret)
			goto out;
		ret = sunxi_spi_cpu_rx(sspi, t);
		if (ret)
			goto out;
	}

out:
	t->len = len;
	t->tx_buf = tx_buf;
	t->rx_buf = rx_buf;
	if (ret)
		sunxi_err(sspi->dev, "cpu tx rx error with align_%d left_%d i_%d\n", align_loop, left_loop, i);
	return ret;
}

static void sunxi_spi_dma_cb_rx(void *data)
{
	struct sunxi_spi *sspi = (struct sunxi_spi *)data;
	u32 cnt = sunxi_spi_get_rxfifo_cnt(sspi);;

	sunxi_debug(sspi->dev, "dma rx callback\n");

	if (cnt) {
		sunxi_err(sspi->dev, "dma done but rxfifo not empty %d\n", cnt);
		sspi->result = -1;
	}

	complete(&sspi->done);
}

static void sunxi_spi_dma_cb_tx(void *data)
{
	struct sunxi_spi *sspi = (struct sunxi_spi *)data;

	sunxi_debug(sspi->dev, "dma tx callback\n");
}

static void sunxi_spi_config_dma(struct dma_slave_config *config, int len, u32 triglevel, bool dma_force_fixed)
{
	int width, burst;

	if (dma_force_fixed) {
		/* if dma is force fixed, use old configuration to make sure the stability and compatibility */
		if (len % DMA_SLAVE_BUSWIDTH_4_BYTES == 0)
			width = DMA_SLAVE_BUSWIDTH_4_BYTES;
		else
			width = DMA_SLAVE_BUSWIDTH_1_BYTE;
		burst = 4;
	} else {
		if (len % DMA_SLAVE_BUSWIDTH_4_BYTES == 0) {
			width = DMA_SLAVE_BUSWIDTH_4_BYTES;
			if (triglevel < SUNXI_SPI_FIFO_DEFAULT)
				burst = 8;
			else
				burst = 16;
		} else if (len % DMA_SLAVE_BUSWIDTH_2_BYTES == 0) {
			width = DMA_SLAVE_BUSWIDTH_2_BYTES;
			burst = 16;
		} else {
			width = DMA_SLAVE_BUSWIDTH_1_BYTE;
			burst = 16;
		}
	}

	config->src_addr_width = width;
	config->dst_addr_width = width;
	config->src_maxburst = burst;
	config->dst_maxburst = burst;
}

static int sunxi_spi_config_dma_rx(struct sunxi_spi *sspi, struct spi_transfer *t)
{
	struct dma_slave_config dma_conf = {0};
	struct dma_async_tx_descriptor *dma_desc = NULL;

	dma_conf.direction = DMA_DEV_TO_MEM;
	dma_conf.src_addr = sspi->base_addr_phy + SUNXI_SPI_RXDATA_REG;

	/* @BugFix :
	 * There is an issue in controller that RX_RDY trigger is not controllable before 1886
	 * If is in slave mode, we use the DMA RX configuration by fixed value
	 */
	if (spi_controller_is_slave(sspi->ctlr) && (sspi->data->quirk_flag & DMA_FORCE_FIXED)) {
		dma_conf.src_addr_width = DMA_SLAVE_BUSWIDTH_1_BYTE;
		dma_conf.dst_addr_width = DMA_SLAVE_BUSWIDTH_1_BYTE;
		dma_conf.src_maxburst = 1;
		dma_conf.dst_maxburst = 1;
	} else {
		sunxi_spi_config_dma(&dma_conf, t->len, sspi->rx_triglevel, sspi->data->quirk_flag & DMA_FORCE_FIXED);
	}
	sunxi_debug(sspi->dev, "dma rx config width_%d burst_%d\n", dma_conf.src_addr_width, dma_conf.src_maxburst);

	dmaengine_slave_config(sspi->ctlr->dma_rx, &dma_conf);

	dma_desc = dmaengine_prep_slave_sg(sspi->ctlr->dma_rx, t->rx_sg.sgl,
			t->rx_sg.nents, DMA_DEV_TO_MEM, DMA_PREP_INTERRUPT | DMA_CTRL_ACK);
	if (!dma_desc) {
		sunxi_err(sspi->dev, "dma rx prepare failed\n");
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

	dma_conf.direction = DMA_MEM_TO_DEV;
	dma_conf.dst_addr = sspi->base_addr_phy + SUNXI_SPI_TXDATA_REG;
	sunxi_spi_config_dma(&dma_conf, t->len, sspi->tx_triglevel, sspi->data->quirk_flag & DMA_FORCE_FIXED);
	sunxi_debug(sspi->dev, "dma tx config width_%d burst_%d\n", dma_conf.dst_addr_width, dma_conf.dst_maxburst);

	dmaengine_slave_config(sspi->ctlr->dma_tx, &dma_conf);

	dma_desc = dmaengine_prep_slave_sg(sspi->ctlr->dma_tx, t->tx_sg.sgl,
			t->tx_sg.nents, DMA_MEM_TO_DEV, DMA_PREP_INTERRUPT | DMA_CTRL_ACK);
	if (!dma_desc) {
		sunxi_err(sspi->dev, "dma tx dmaengine_prep_slave_sg desc failed\n");
		return -1;
	}

	dma_desc->callback = sunxi_spi_dma_cb_tx;
	dma_desc->callback_param = (void *)sspi;
	dmaengine_submit(dma_desc);
	return 0;
}

static int sunxi_spi_dma_rx(struct sunxi_spi *sspi, struct spi_transfer *t)
{
	int ret = 0;

	sunxi_spi_enable_dma_irq(sspi, SUNXI_SPI_FIFO_CTL_RX_DRQ_EN);
	ret = sunxi_spi_config_dma_rx(sspi, t);
	dma_async_issue_pending(sspi->ctlr->dma_rx);

	return ret;
}

static int sunxi_spi_dma_tx(struct sunxi_spi *sspi, struct spi_transfer *t)
{
	int ret = 0;

	sunxi_spi_enable_dma_irq(sspi, SUNXI_SPI_FIFO_CTL_TX_DRQ_EN);
	ret = sunxi_spi_config_dma_tx(sspi, t);
	dma_async_issue_pending(sspi->ctlr->dma_tx);

	return ret;
}

static int sunxi_spi_xfer_master(struct spi_device *spi, struct spi_transfer *t)
{
	struct sunxi_spi *sspi = spi_controller_get_devdata(spi->controller);
	unsigned long timeout = 0;
	int ret = 0;

	/* ready gpio sync logic between master & slave */
	if ((sspi->ready_gpio > 0) && (spi->mode & SPI_READY)) {
		sunxi_debug(sspi->dev, "waiting for ready single %d\n", sspi->ready_done.done);
		if (wait_for_completion_interruptible(&sspi->ready_done)) {
			sunxi_warn(sspi->dev, "master xfer interrupted\n");
			ret = -EINTR;
			goto out;
		}
	}

	sunxi_spi_enable_irq(sspi, SUNXI_SPI_INT_CTL_TC_EN);

	switch (sspi->mode_type) {
	case SINGLE_HALF_DUPLEX_RX:
	case DUAL_HALF_DUPLEX_RX:
	case QUAD_HALF_DUPLEX_RX:
		if (sunxi_spi_can_dma(spi->controller, spi, t)) {
			sunxi_debug(sspi->dev, "master xfer rx by dma %d\n", t->len);
			sunxi_spi_disable_irq(sspi, SUNXI_SPI_INT_CTL_TC_EN);
			sunxi_spi_dma_rx(sspi, t);
			sunxi_spi_start_xfer(sspi);
		} else {
			sunxi_debug(sspi->dev, "master xfer rx by cpu %d\n", t->len);
			sunxi_spi_start_xfer(sspi);
			ret = sunxi_spi_cpu_rx(sspi, t);
		}
		break;
	case SINGLE_HALF_DUPLEX_TX:
	case DUAL_HALF_DUPLEX_TX:
	case QUAD_HALF_DUPLEX_TX:
		if (sunxi_spi_can_dma(spi->controller, spi, t)) {
			sunxi_debug(sspi->dev, "master xfer tx by dma %d\n", t->len);
			sunxi_spi_start_xfer(sspi);
			sunxi_spi_dma_tx(sspi, t);
		} else {
			sunxi_debug(sspi->dev, "master xfer tx by cpu %d\n", t->len);
			sunxi_spi_start_xfer(sspi);
			ret = sunxi_spi_cpu_tx(sspi, t);
		}
		break;
	case SINGLE_FULL_DUPLEX_TX_RX:
		if (sunxi_spi_can_dma(spi->controller, spi, t)) {
			sunxi_debug(sspi->dev, "master xfer tx & rx by dma %d\n", t->len);
			sunxi_spi_disable_irq(sspi, SUNXI_SPI_INT_CTL_TC_EN);
			sunxi_spi_dma_rx(sspi, t);
			sunxi_spi_start_xfer(sspi);
			sunxi_spi_dma_tx(sspi, t);
		} else {
			sunxi_debug(sspi->dev, "master xfer tx & rx by cpu %d\n", t->len);
			sunxi_spi_start_xfer(sspi);
			sunxi_spi_cpu_tx_rx(sspi, t);
		}
		break;
	default:
		sunxi_err(sspi->dev, "unknown master transfer mode type %d\n", sspi->mode_type);
		ret = -EINVAL;
		goto out;
	}

	timeout = wait_for_completion_timeout(&sspi->done, msecs_to_jiffies(SUNXI_SPI_XFER_TIMEOUT));
	if (timeout == 0) {
		sunxi_err(sspi->dev, "master transfer timeout type %d\n", sspi->mode_type);
		ret = -ETIME;
		goto out;
	} else if (sspi->result < 0) {
		sunxi_err(sspi->dev, "master transfer failed %d\n", sspi->result);
		ret = -EINVAL;
		goto out;
	}

out:
	if (ret < 0 && sunxi_spi_can_dma(spi->controller, spi, t)) {
		dmaengine_terminate_sync(spi->controller->dma_tx);
		dmaengine_terminate_sync(spi->controller->dma_rx);
	}
	return ret;
}

static int sunxi_spi_xfer_slave(struct spi_device *spi, struct spi_transfer *t)
{
	struct sunxi_spi *sspi = spi_controller_get_devdata(spi->controller);
	int poll_time = SUNXI_SPI_POLL_TIMEOUT;
	int ret = 0;

	sspi->slave_aborted = false;

	/* In slave mode we didn't care about fifo overflow or not */
	sunxi_spi_disable_irq(sspi, SUNXI_SPI_INT_CTL_TX_OVF_EN);
	sunxi_spi_disable_irq(sspi, SUNXI_SPI_INT_CTL_RX_OVF_EN);

	switch (sspi->mode_type) {
	case SINGLE_HALF_DUPLEX_RX:
		if (sunxi_spi_can_dma(spi->controller, spi, t)) {
			sunxi_debug(sspi->dev, "slave xfer rx by dma %d\n", t->len);
			if (sspi->data->quirk_flag & DMA_FORCE_FIXED)
				sunxi_spi_set_fifo_trig_level_rx(sspi, 1);
			sunxi_spi_dma_rx(sspi, t);
			sunxi_spi_set_ready_status(sspi, 1);
		} else {
			sunxi_debug(sspi->dev, "slave xfer rx by cpu %d\n", t->len);
			sunxi_spi_set_fifo_trig_level_rx(sspi, t->len - 1);
			sunxi_spi_clr_irq_pending(sspi, SUNXI_SPI_INT_STA_RX_RDY);
			sunxi_spi_enable_irq(sspi, SUNXI_SPI_INT_CTL_RX_RDY_EN);
			sunxi_spi_set_ready_status(sspi, 1);
			if (wait_for_completion_interruptible(&sspi->done) || sspi->slave_aborted) {
				sunxi_warn(sspi->dev, "slave xfer aborted or interrupted\n");
				ret = -EINTR;
				goto out;
			}
			if (sspi->result < 0) {
				sunxi_err(sspi->dev, "slave transfer failed %d\n", sspi->result);
				ret = -EINVAL;
				goto out;
			}
			ret = sunxi_spi_cpu_rx(sspi, t);
			goto out;
		}
		break;
	case SINGLE_HALF_DUPLEX_TX:
		if (sunxi_spi_can_dma(spi->controller, spi, t)) {
			sunxi_debug(sspi->dev, "slave xfer tx by dma %d\n", t->len);
			sunxi_spi_dma_tx(sspi, t);
			/* Waiting for DMA fill data into fifo */
			while (!sunxi_spi_get_txfifo_cnt(sspi) && poll_time--)
				;
			if (poll_time <= 0) {
				sunxi_err(sspi->dev, "timeout for waiting fill data into fifo\n");
				ret = -ETIME;
				goto out;
			}
		} else {
			sunxi_debug(sspi->dev, "slave xfer tx by cpu %d\n", t->len);
			ret = sunxi_spi_cpu_tx(sspi, t);
		}
		sunxi_spi_clr_irq_pending(sspi, SUNXI_SPI_INT_STA_TX_EMP);
		sunxi_spi_enable_irq(sspi, SUNXI_SPI_INT_CTL_TX_EMP_EN);
		sunxi_spi_set_ready_status(sspi, 1);
		break;
	default:
		sunxi_err(sspi->dev, "unknown slave transfer mode type %d\n", sspi->mode_type);
		ret = -EINVAL;
		goto out;
	}

	if (wait_for_completion_interruptible(&sspi->done) || sspi->slave_aborted) {
		sunxi_warn(sspi->dev, "slave xfer aborted or interrupted\n");
		sspi->result = -1;
	}
	if (sspi->result < 0) {
		sunxi_err(sspi->dev, "slave transfer failed %d\n", sspi->result);
		ret = -EINVAL;
		goto out;
	}

out:
	if (ret < 0 && sunxi_spi_can_dma(spi->controller, spi, t)) {
		dmaengine_terminate_sync(spi->controller->dma_tx);
		dmaengine_terminate_sync(spi->controller->dma_rx);
	}
	return ret;
}

static int sunxi_spi_xfer_dbi(struct spi_device *spi, struct spi_transfer *t)
{
	struct sunxi_spi *sspi = spi_controller_get_devdata(spi->controller);
	unsigned long timeout = 0;
	int ret = 0;

	sunxi_dbi_config(sspi);
	sunxi_spi_enable_dbi(sspi);
	sunxi_dbi_enable_dma(sspi);

	switch (sspi->mode_type) {
	case SINGLE_HALF_DUPLEX_RX:
	case DUAL_HALF_DUPLEX_RX:
	case QUAD_HALF_DUPLEX_RX:
		if (sunxi_spi_can_dma(spi->controller, spi, t)) {
			sunxi_debug(sspi->dev, "dbi xfer rx by dma %d\n", t->len);
			sunxi_spi_dma_rx(sspi, t);
		} else {
			sunxi_debug(sspi->dev, "dbi xfer rx by cpu %d\n", t->len);
			ret = sunxi_spi_cpu_rx(sspi, t);
		}
		break;
	case SINGLE_HALF_DUPLEX_TX:
	case DUAL_HALF_DUPLEX_TX:
	case QUAD_HALF_DUPLEX_TX:
		if (sunxi_spi_can_dma(spi->controller, spi, t)) {
			sunxi_debug(sspi->dev, "dbi xfer tx by dma %d\n", t->len);
			sunxi_spi_dma_tx(sspi, t);
		} else {
			sunxi_debug(sspi->dev, "dbi xfer tx by cpu %d\n", t->len);
			ret = sunxi_spi_cpu_tx(sspi, t);
			if (!(sspi->dbi_config->dbi_mode & SUNXI_DBI_TRANSMIT_VIDEO)) {
				sunxi_dbi_clr_irq_pending(sspi, SUNXI_DBI_FIFO_EMPTY_INT);
				sunxi_dbi_enable_irq(sspi, SUNXI_DBI_FIFO_EMPTY_INT_EN);
			}
		}
		break;
	case SINGLE_FULL_DUPLEX_TX_RX:
		if (sunxi_spi_can_dma(spi->controller, spi, t)) {
			sunxi_debug(sspi->dev, "dbi xfer tx & rx by dma %d\n", t->len);
			sunxi_spi_dma_rx(sspi, t);
			sunxi_spi_dma_tx(sspi, t);
		} else {
			sunxi_debug(sspi->dev, "dbi xfer tx & rx by cpu %d\n", t->len);
			sunxi_spi_cpu_tx_rx(sspi, t);
			if (!(sspi->dbi_config->dbi_mode & SUNXI_DBI_TRANSMIT_VIDEO)) {
				sunxi_dbi_clr_irq_pending(sspi, SUNXI_DBI_FIFO_EMPTY_INT);
				sunxi_dbi_enable_irq(sspi, SUNXI_DBI_FIFO_EMPTY_INT_EN);
			}
		}
		break;
	default:
		sunxi_err(sspi->dev, "unknown dbi transfer mode type %d\n", sspi->mode_type);
		ret = -EINVAL;
		goto out;
	}

	timeout = wait_for_completion_timeout(&sspi->done, msecs_to_jiffies(SUNXI_SPI_XFER_TIMEOUT));
	if (timeout == 0) {
		sunxi_err(sspi->dev, "dbi transfer timeout type %d\n", sspi->mode_type);
		ret = -ETIME;
		goto out;
	} else if (sspi->result < 0) {
		sunxi_err(sspi->dev, "dbi transfer failed %d\n", sspi->result);
		ret = -EINVAL;
		goto out;
	}

out:
	sunxi_spi_disable_dbi(sspi);
	sunxi_dbi_disable_dma(sspi);
	if (ret < 0 && sunxi_spi_can_dma(spi->controller, spi, t)) {
		dmaengine_terminate_sync(spi->controller->dma_tx);
		dmaengine_terminate_sync(spi->controller->dma_rx);
	}
	return ret;
}

static int sunxi_spi_transfer_one(struct spi_controller *ctlr, struct spi_device *spi, struct spi_transfer *t)
{
	struct sunxi_spi *sspi = spi_controller_get_devdata(ctlr);
	u8 *tx_buf = (u8 *)t->tx_buf;
	u8 *rx_buf = (u8 *)t->rx_buf;
	int ret = 0;

	if ((!t->tx_buf && !t->rx_buf) || !t->len)
		return -EINVAL;

	sunxi_debug(sspi->dev, "spi transfer, txbuf %p, rxbuf %p, len %d\n", tx_buf, rx_buf, t->len);

	ret = sunxi_spi_xfer_setup(spi, t);
	if (ret < 0) {
		sunxi_err(sspi->dev, "failed to setup spi xfer %d\n", ret);
		return ret;
	}

	switch (sspi->bus_mode) {
	case SUNXI_SPI_BUS_BIT:
		sunxi_spi_bit_disable_irq(sspi);
		sunxi_spi_bit_clr_irq_pending(sspi);
		sunxi_spi_bit_enable_irq(sspi);
		break;
	case SUNXI_SPI_BUS_DBI:
		sunxi_dbi_disable_irq(sspi, SUNXI_DBI_INT_EN_MASK);
		sunxi_dbi_clr_irq_pending(sspi, SUNXI_DBI_INT_STA_MASK);
		sunxi_dbi_disable_dma(sspi);
		break;
	default:
		/* reset fifo */
		sunxi_spi_reset_fifo(sspi);
		sunxi_spi_set_fifo_trig_level_rx(sspi, sspi->rx_triglevel);
		sunxi_spi_set_fifo_trig_level_tx(sspi, sspi->tx_triglevel);

		sunxi_spi_disable_irq(sspi, SUNXI_SPI_INT_CTL_MASK);
		sunxi_spi_clr_irq_pending(sspi, SUNXI_SPI_INT_STA_MASK);
		sunxi_spi_disable_dma_irq(sspi, SUNXI_SPI_FIFO_CTL_DRQ_EN);
		sunxi_spi_enable_irq(sspi, SUNXI_SPI_INT_CTL_ERR);
	}

	ret = sunxi_spi_mode_check(sspi, t);
	if (ret < 0) {
		sunxi_err(sspi->dev, "failed to mode check spi xfer %d\n", ret);
		return ret;
	}

	sspi->result = 0;
	reinit_completion(&sspi->done);

	if (sunxi_spi_debug_mask & SUNXI_SPI_DEBUG_DUMP_DATA)
		sunxi_spi_dump_data(sspi->dev, t->tx_buf, t->len);
	if (sunxi_spi_debug_mask & SUNXI_SPI_DEBUG_DUMP_REG)
		sunxi_spi_dump_regs(sspi);

	switch (sspi->bus_mode) {
	case SUNXI_SPI_BUS_DBI:
		ret = sunxi_spi_xfer_dbi(spi, t);
		break;
	case SUNXI_SPI_BUS_BIT:
		ret = sunxi_spi_xfer_bit(spi, t);
		break;
	default:
		if (spi_controller_is_slave(ctlr))
			ret = sunxi_spi_xfer_slave(spi, t);
		else {
			ret = sunxi_spi_xfer_master(spi, t);
			reinit_completion(&sspi->ready_done);
		}
	}

	if (ret < 0 || sunxi_spi_debug_mask & SUNXI_SPI_DEBUG_DUMP_REG)
		sunxi_spi_dump_regs(sspi);
	if (sunxi_spi_debug_mask & SUNXI_SPI_DEBUG_DUMP_DATA)
		sunxi_spi_dump_data(sspi->dev, t->rx_buf, t->len);

	if (sspi->mode_type != MODE_TYPE_NULL)
		sspi->mode_type = MODE_TYPE_NULL;

	return ret;
}

static int sunxi_spi_xfer_tx_rx(struct spi_device *spi, struct spi_transfer *tx, struct spi_transfer *rx)
{
	struct sunxi_spi *sspi = spi_controller_get_devdata(spi->controller);
	unsigned long timeout = 0;
	int ret = 0;

	/* Reset spi burst cause in flash mode it will send tx/rx in one transfer */
	sunxi_spi_set_bc_tc_stc(sspi, tx->len, rx->len, tx->len, 0);

	sunxi_spi_enable_irq(sspi, SUNXI_SPI_INT_CTL_TC_EN);
	sunxi_spi_start_xfer(sspi);

	if (sunxi_spi_can_dma(spi->controller, spi, tx)) {
		sunxi_debug(sspi->dev, "flash xfer2 tx by dma %d\n", tx->len);
		sunxi_spi_dma_tx(sspi, tx);
	} else {
		sunxi_debug(sspi->dev, "flash xfer2 tx by cpu %d\n", tx->len);
		ret = sunxi_spi_cpu_tx(sspi, tx);
	}

	if (sunxi_spi_can_dma(spi->controller, spi, rx)) {
		sunxi_debug(sspi->dev, "flash xfer2 rx by dma %d\n", rx->len);
		sunxi_spi_disable_irq(sspi, SUNXI_SPI_INT_CTL_TC_EN);
		sunxi_spi_dma_rx(sspi, rx);
	} else {
		sunxi_debug(sspi->dev, "flash xfer2 rx by cpu %d\n", rx->len);
		ret = sunxi_spi_cpu_rx(sspi, rx);
	}

	timeout = wait_for_completion_timeout(&sspi->done, msecs_to_jiffies(SUNXI_SPI_XFER_TIMEOUT));
	if (timeout == 0) {
		sunxi_err(sspi->dev, "flash transfer timeout type %d\n", sspi->mode_type);
		ret = -ETIME;
		goto out;
	} else if (sspi->result < 0) {
		sunxi_err(sspi->dev, "flash transfer failed %d\n", sspi->result);
		ret = -EINVAL;
		goto out;
	}

out:
	if (ret < 0 && sunxi_spi_can_dma(spi->controller, spi, rx)) {
		dmaengine_terminate_sync(spi->controller->dma_rx);
	}
	return ret;
}

static int sunxi_spi_transfer_more(struct spi_controller *ctlr, struct spi_device *spi,
				struct spi_transfer **tx, struct spi_transfer *rx, int tx_xfer_cnt)
{
	struct sunxi_spi *sspi = spi_controller_get_devdata(ctlr);
	int ret = 0;
	u32 tmpbufsize = 0;
	int xfer_n = 0;
	struct spi_transfer tx_sum = { };
	u8 *tmpbuf;

	if (!tx[0]->tx_buf || !rx->rx_buf) {
		sunxi_err(sspi->dev, "flash xfer mode not support\n");
		sunxi_err(sspi->dev, "tx: txbuf %p, rxbuf %p, len %d\n", tx[0]->tx_buf, tx[0]->rx_buf, tx[0]->len);
		sunxi_err(sspi->dev, "rx: txbuf %p, rxbuf %p, len %d\n", rx->tx_buf, rx->rx_buf, rx->len);
		return -EINVAL;
	}

	ret = sunxi_spi_xfer_setup(spi, tx[0]);
	if (ret < 0) {
		sunxi_err(sspi->dev, "failed to setup flash tx %d\n", ret);
		return ret;
	}
	ret = sunxi_spi_xfer_setup(spi, rx);
	if (ret < 0) {
		sunxi_err(sspi->dev, "failed to setup flash rx %d\n", ret);
		return ret;
	}

	if (tx_xfer_cnt > 1) {
		for (xfer_n = 0; xfer_n < tx_xfer_cnt; xfer_n++)
			tmpbufsize += tx[xfer_n]->len;
		tmpbuf = devm_kzalloc(sspi->dev, tmpbufsize, GFP_KERNEL | GFP_DMA);
		if (!tmpbuf)
			return -ENOMEM;
		tx_sum.tx_buf = tmpbuf;
		tx_sum.len = tmpbufsize;
		tx_sum.tx_nbits = tx[0]->tx_nbits;
		for (xfer_n = 0; xfer_n < tx_xfer_cnt; xfer_n++) {
			memcpy(tmpbuf, tx[xfer_n]->tx_buf, tx[xfer_n]->len);
			tmpbuf += tx[xfer_n]->len;
		}
	}

	/* reset fifo */
	sunxi_spi_reset_fifo(sspi);
	sunxi_spi_set_fifo_trig_level_rx(sspi, sspi->rx_triglevel);
	sunxi_spi_set_fifo_trig_level_tx(sspi, sspi->tx_triglevel);

	sunxi_spi_disable_irq(sspi, SUNXI_SPI_INT_CTL_MASK);
	sunxi_spi_clr_irq_pending(sspi, SUNXI_SPI_INT_STA_MASK);
	sunxi_spi_disable_dma_irq(sspi, SUNXI_SPI_FIFO_CTL_DRQ_EN);
	sunxi_spi_enable_irq(sspi, SUNXI_SPI_INT_CTL_ERR);

	ret = sunxi_spi_mode_check(sspi, rx);
	if (ret < 0) {
		sunxi_err(sspi->dev, "failed to mode check flash xfer %d\n", ret);
		goto err;
	}

	sspi->result = 0;
	reinit_completion(&sspi->done);

	if (sunxi_spi_debug_mask & SUNXI_SPI_DEBUG_DUMP_REG)
		sunxi_spi_dump_regs(sspi);

	if (tx_xfer_cnt > 1) {
		if (sunxi_spi_debug_mask & SUNXI_SPI_DEBUG_DUMP_DATA)
			sunxi_spi_dump_data(sspi->dev, tx_sum.tx_buf, tx_sum.len);
		ret = sunxi_spi_xfer_tx_rx(spi, &tx_sum, rx);
	} else {
		if (sunxi_spi_debug_mask & SUNXI_SPI_DEBUG_DUMP_DATA)
			sunxi_spi_dump_data(sspi->dev, tx[0]->tx_buf, tx[0]->len);
		ret = sunxi_spi_xfer_tx_rx(spi, tx[0], rx);
	}

	if (ret < 0 || sunxi_spi_debug_mask & SUNXI_SPI_DEBUG_DUMP_REG)
		sunxi_spi_dump_regs(sspi);
	if (sunxi_spi_debug_mask & SUNXI_SPI_DEBUG_DUMP_DATA)
		sunxi_spi_dump_data(sspi->dev, rx->rx_buf, rx->len);

	if (sspi->mode_type != MODE_TYPE_NULL)
		sspi->mode_type = MODE_TYPE_NULL;

err:
	if (tx_xfer_cnt > 1)
		devm_kfree(sspi->dev, tmpbuf);
	return ret;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)) && defined(CONFIG_HAS_DMA)
static void sunxi_spi_dma_sync_for_device(struct spi_controller *ctlr, struct spi_transfer *xfer)
{
	struct device *rx_dev = ctlr->cur_rx_dma_dev;
	struct device *tx_dev = ctlr->cur_tx_dma_dev;

	if (!ctlr->cur_msg_mapped)
		return;

	if (xfer->tx_sg.orig_nents)
		dma_sync_sgtable_for_device(tx_dev, &xfer->tx_sg, DMA_TO_DEVICE);
	if (xfer->rx_sg.orig_nents)
		dma_sync_sgtable_for_device(rx_dev, &xfer->rx_sg, DMA_FROM_DEVICE);
}

static void sunxi_spi_dma_sync_for_cpu(struct spi_controller *ctlr, struct spi_transfer *xfer)
{
	struct device *rx_dev = ctlr->cur_rx_dma_dev;
	struct device *tx_dev = ctlr->cur_tx_dma_dev;

	if (!ctlr->cur_msg_mapped)
		return;

	if (xfer->rx_sg.orig_nents)
		dma_sync_sgtable_for_cpu(rx_dev, &xfer->rx_sg, DMA_FROM_DEVICE);
	if (xfer->tx_sg.orig_nents)
		dma_sync_sgtable_for_cpu(tx_dev, &xfer->tx_sg, DMA_TO_DEVICE);
}
#else
static void sunxi_spi_dma_sync_for_device(struct spi_controller *ctrl, struct spi_transfer *xfer) { }
static void sunxi_spi_dma_sync_for_cpu(struct spi_controller *ctrl, struct spi_transfer *xfer) { }
#endif

static int sunxi_spi_transfer_one_message(struct spi_controller *ctlr, struct spi_message *msg)
{
	struct sunxi_spi *sspi = spi_controller_get_devdata(ctlr);

	struct spi_transfer *xfer;
	struct spi_transfer *cur_xfer = NULL;
	struct spi_transfer *prev_xfer[3] = {NULL};
	int xfer_cnt = 0;
	int xfer_n = 0;
	int ret = 0;

	sunxi_spi_set_cs(msg->spi, false);

	list_for_each_entry(xfer, &msg->transfers, transfer_list) {
		cur_xfer = xfer;
		if (prev_xfer[0] && prev_xfer[0]->tx_buf && !prev_xfer[0]->rx_buf &&
				cur_xfer && cur_xfer->rx_buf && !cur_xfer->tx_buf) {
			/* tx->rx */
			reinit_completion(&ctlr->xfer_completion);
			for (xfer_n = 0; xfer_n < xfer_cnt; xfer_n++)
				sunxi_spi_dma_sync_for_device(ctlr, prev_xfer[xfer_n]);
			sunxi_spi_dma_sync_for_device(ctlr, cur_xfer);
			ret = sunxi_spi_transfer_more(ctlr, msg->spi, prev_xfer, cur_xfer, xfer_cnt);
			if (ret < 0) {
				for (xfer_n = 0; xfer_n < xfer_cnt; xfer_n++)
					sunxi_spi_dma_sync_for_cpu(ctlr, prev_xfer[xfer_n]);
				sunxi_spi_dma_sync_for_cpu(ctlr, cur_xfer);
				sunxi_err(sspi->dev, "flash transfer more failed %d\n", ret);
				goto out;
			}
			for (xfer_n = 0; xfer_n < xfer_cnt; xfer_n++)
				sunxi_spi_dma_sync_for_cpu(ctlr, prev_xfer[xfer_n]);
			sunxi_spi_dma_sync_for_cpu(ctlr, cur_xfer);
			if (msg->status != -EINPROGRESS)
				goto out;
			msg->actual_length += cur_xfer->len;
			for (xfer_n = 0; xfer_n < xfer_cnt; xfer_n++) {
				msg->actual_length += prev_xfer[xfer_n]->len;
				prev_xfer[xfer_n] = NULL;
			}
			cur_xfer = NULL;
			xfer_cnt = 0;
		} else if (cur_xfer && !list_is_last(&cur_xfer->transfer_list, &msg->transfers)) {
			prev_xfer[xfer_cnt] = xfer;
			xfer_cnt++;
			if (xfer_cnt > 3) {
				sunxi_err(sspi->dev, "the number of transfers exceeds the processing range\n");
				goto out;
			}
		} else {
			/* single handle */
			for (xfer_n = 0; xfer_n <= xfer_cnt; xfer_n++) {
				if (xfer_n < xfer_cnt)
					xfer = prev_xfer[xfer_n];
				else
					xfer = cur_xfer;
				reinit_completion(&ctlr->xfer_completion);
				sunxi_spi_dma_sync_for_device(ctlr, xfer);
				ret = sunxi_spi_transfer_one(ctlr, msg->spi, xfer);
				if (ret < 0) {
					sunxi_spi_dma_sync_for_cpu(ctlr, xfer);
					sunxi_err(sspi->dev, "flash transfer one failed %d\n", ret);
					goto out;
				}
				sunxi_spi_dma_sync_for_cpu(ctlr, xfer);
				if (msg->status != -EINPROGRESS)
					goto out;
				msg->actual_length += xfer->len;
			}

			for (xfer_n = 0; xfer_n < xfer_cnt; xfer_n++)
				prev_xfer[xfer_n] = NULL;
			cur_xfer = NULL;
			xfer_cnt = 0;
		}
	}

	sunxi_spi_set_cs(msg->spi, true);

out:
	if (msg->status == -EINPROGRESS)
		msg->status = ret;
	if (msg->status && ctlr->handle_err)
		ctlr->handle_err(ctlr, msg);
	spi_finalize_current_message(ctlr);
	return ret;
}

#if IS_ENABLED(CONFIG_AW_SPI_NG_ATOMIC_XFER)
static int sunxi_spi_xfer_atomic(struct spi_device *spi, struct spi_transfer *t)
{
	struct sunxi_spi *sspi = spi_controller_get_devdata(spi->controller);
	u32 status = 0;
	int poll_time = SUNXI_SPI_POLL_TIMEOUT;
	int ret = 0;

	switch (sspi->mode_type) {
	case SINGLE_HALF_DUPLEX_RX:
	case DUAL_HALF_DUPLEX_RX:
	case QUAD_HALF_DUPLEX_RX:
		sunxi_debug(sspi->dev, "xfer atomic rx by cpu %d\n", t->len);
		sunxi_spi_start_xfer(sspi);
		ret = sunxi_spi_cpu_rx(sspi, t);
		break;
	case SINGLE_HALF_DUPLEX_TX:
	case DUAL_HALF_DUPLEX_TX:
	case QUAD_HALF_DUPLEX_TX:
		sunxi_debug(sspi->dev, "xfer atomic tx by cpu %d\n", t->len);
		sunxi_spi_start_xfer(sspi);
		ret = sunxi_spi_cpu_tx(sspi, t);
		break;
	case SINGLE_FULL_DUPLEX_TX_RX:
		sunxi_debug(sspi->dev, "xfer atomic tx & rx by cpu %d\n", t->len);
		sunxi_spi_start_xfer(sspi);
		sunxi_spi_cpu_tx_rx(sspi, t);
		break;
	default:
		sunxi_err(sspi->dev, "unknown xfer atomic mode type %d\n", sspi->mode_type);
		ret = -EINVAL;
		goto out;
	}

	while (--poll_time) {
		status = sunxi_spi_qry_irq_pending(sspi);
		sunxi_spi_clr_irq_pending(sspi, status);
		sunxi_debug(sspi->dev, "spi irq handler status(%#x)\n", status);
		if (status & SUNXI_SPI_INT_STA_TC) {
			sunxi_debug(sspi->dev, "xfer atomic irq tc comes\n");
			break;
		} else if (status & SUNXI_SPI_INT_STA_ERR) {
			sunxi_err(sspi->dev, "xfer atomic irq status error %#x\n", status);
			sspi->result = -1;
			break;
		}
	}

	if (poll_time <= 0) {
		sunxi_err(sspi->dev, "xfer atomic wait for irq timeout\n");
		ret = -ETIME;
		goto out;
	} else if (sspi->result < 0) {
		sunxi_err(sspi->dev, "xfer atomic failed %d\n", sspi->result);
		ret = -EINVAL;
		goto out;
	}

out:
	return ret;
}

static int sunxi_spi_transfer_one_atomic(struct spi_controller *ctlr, struct spi_device *spi, struct spi_transfer *t)
{
	struct sunxi_spi *sspi = spi_controller_get_devdata(ctlr);
	u8 *tx_buf = (u8 *)t->tx_buf;
	u8 *rx_buf = (u8 *)t->rx_buf;
	int ret = 0;

	if ((!t->tx_buf && !t->rx_buf) || !t->len)
		return -EINVAL;

	sunxi_debug(sspi->dev, "spi transfer atomic, txbuf %p, rxbuf %p, len %d\n", tx_buf, rx_buf, t->len);

	ret = sunxi_spi_xfer_setup(spi, t);
	if (ret < 0) {
		sunxi_err(sspi->dev, "failed to setup atomic xfer\n");
		return ret;
	}

	switch (sspi->bus_mode) {
	case SUNXI_SPI_BUS_MASTER:
	case SUNXI_SPI_BUS_NOR:
	case SUNXI_SPI_BUS_NAND:
		/* reset fifo */
		sunxi_spi_reset_fifo(sspi);
		sunxi_spi_set_fifo_trig_level_rx(sspi, sspi->rx_triglevel);
		sunxi_spi_set_fifo_trig_level_tx(sspi, sspi->tx_triglevel);

		sunxi_spi_disable_irq(sspi, SUNXI_SPI_INT_CTL_MASK);
		sunxi_spi_clr_irq_pending(sspi, SUNXI_SPI_INT_STA_MASK);
		sunxi_spi_disable_dma_irq(sspi, SUNXI_SPI_FIFO_CTL_DRQ_EN);
		break;
	default:
		sunxi_err(sspi->dev, "unsupport xfer atomic bus mode %d\n", sspi->bus_mode);
		ret = -EINVAL;
		goto out;
	}

	ret = sunxi_spi_mode_check(sspi, t);
	if (ret < 0) {
		sunxi_err(sspi->dev, "failed to mode check atomic xfer %d\n", ret);
		return ret;
	}

	sspi->result = 0;

	if (sunxi_spi_debug_mask & SUNXI_SPI_DEBUG_DUMP_DATA)
		sunxi_spi_dump_data(sspi->dev, t->tx_buf, t->len);
	if (sunxi_spi_debug_mask & SUNXI_SPI_DEBUG_DUMP_REG)
		sunxi_spi_dump_regs(sspi);

	ret = sunxi_spi_xfer_atomic(spi, t);

	if (ret < 0 || sunxi_spi_debug_mask & SUNXI_SPI_DEBUG_DUMP_REG)
		sunxi_spi_dump_regs(sspi);
	if (sunxi_spi_debug_mask & SUNXI_SPI_DEBUG_DUMP_DATA)
		sunxi_spi_dump_data(sspi->dev, t->rx_buf, t->len);

out:
	if (sspi->mode_type != MODE_TYPE_NULL)
		sspi->mode_type = MODE_TYPE_NULL;
	return ret;
}

int sunxi_spi_sync_atomic(struct spi_device *spi, struct spi_message *message)
{
	struct sunxi_spi *sspi = spi_controller_get_devdata(spi->controller);
	struct spi_transfer *xfer;
	int ret = 0;

	sunxi_spi_set_cs(spi, false);

	list_for_each_entry(xfer, &message->transfers, transfer_list) {
		ret = sunxi_spi_transfer_one_atomic(sspi->ctlr, spi, xfer);
		if (ret < 0) {
			sunxi_err(sspi->dev, "transfer one atomic failed %d\n", ret);
			sunxi_spi_handle_err(sspi->ctlr, message);
			goto out;
		}
	}

out:
	sunxi_spi_set_cs(spi, true);
	return ret;
}
EXPORT_SYMBOL_GPL(sunxi_spi_sync_atomic);
#endif

static void sunxi_spi_bus_handler(struct sunxi_spi *sspi)
{
	u32 status = 0, enable = 0;
	bool compelte = false;

	enable = sunxi_spi_qry_irq_enable(sspi);
	status = sunxi_spi_qry_irq_pending(sspi);
	sunxi_spi_clr_irq_pending(sspi, status);
	sunxi_debug(sspi->dev, "spi irq handler enable(%#x) status(%#x)\n", enable, status);

	if ((enable & SUNXI_SPI_INT_CTL_SS_EN) && (status & SUNXI_SPI_INT_STA_SSI)) {
		sunxi_debug(sspi->dev, "irq bus cs invalid detect\n");
		sunxi_spi_disable_irq(sspi, SUNXI_SPI_INT_CTL_SS_EN);
		sspi->result = -1;
		compelte = true;
	}
	if ((enable & SUNXI_SPI_INT_CTL_TC_EN) && (status & SUNXI_SPI_INT_STA_TC)) {
		sunxi_debug(sspi->dev, "irq bus tc comes\n");
		sunxi_spi_disable_irq(sspi, SUNXI_SPI_INT_CTL_TC_EN);
		compelte = true;
	}
	if ((enable & SUNXI_SPI_INT_CTL_TX_UDR_EN) && (status & SUNXI_SPI_INT_STA_TX_UDR)) {
		sunxi_err(sspi->dev, "irq bus txfifo underrun\n");
		sunxi_spi_disable_irq(sspi, SUNXI_SPI_INT_CTL_TX_UDR_EN);
		sspi->result = -1;
		compelte = true;
	}
	if ((enable & SUNXI_SPI_INT_CTL_TX_OVF_EN) && (status & SUNXI_SPI_INT_STA_TX_OVF)) {
		sunxi_err(sspi->dev, "irq bus txfifo overflow\n");
		sunxi_spi_disable_irq(sspi, SUNXI_SPI_INT_CTL_TX_OVF_EN);
		sunxi_spi_reset_txfifo(sspi);
		sspi->result = -1;
		compelte = true;
	}
	if ((enable & SUNXI_SPI_INT_CTL_RX_UDR_EN) && (status & SUNXI_SPI_INT_STA_RX_UDR)) {
		sunxi_err(sspi->dev, "irq bus rxfifo underrun\n");
		sunxi_spi_disable_irq(sspi, SUNXI_SPI_INT_CTL_RX_UDR_EN);
		sspi->result = -1;
		compelte = true;
	}
	if ((enable & SUNXI_SPI_INT_CTL_RX_OVF_EN) && (status & SUNXI_SPI_INT_STA_RX_OVF)) {
		sunxi_err(sspi->dev, "irq bus rxfifo overflow\n");
		sunxi_spi_disable_irq(sspi, SUNXI_SPI_INT_CTL_RX_OVF_EN);
		sunxi_spi_reset_rxfifo(sspi);
		sspi->result = -1;
		compelte = true;
	}
	if ((enable & SUNXI_SPI_INT_CTL_TX_FUL_EN) && (status & SUNXI_SPI_INT_STA_TX_FULL)) {
		sunxi_debug(sspi->dev, "irq bus txfifo full\n");
		sunxi_spi_disable_irq(sspi, SUNXI_SPI_INT_CTL_TX_FUL_EN);
	}
	if ((enable & SUNXI_SPI_INT_CTL_TX_EMP_EN) && (status & SUNXI_SPI_INT_STA_TX_EMP)) {
		sunxi_debug(sspi->dev, "irq bus txfifo empty\n");
		sunxi_spi_disable_irq(sspi, SUNXI_SPI_INT_CTL_TX_EMP_EN);
		compelte = true;
	}
	if ((enable & SUNXI_SPI_INT_CTL_TX_ERQ_EN) && (status & SUNXI_SPI_INT_STA_TX_RDY)) {
		sunxi_debug(sspi->dev, "irq bus txfifo ready\n");
		sunxi_spi_disable_irq(sspi, SUNXI_SPI_INT_CTL_TX_ERQ_EN);
	}
	if ((enable & SUNXI_SPI_INT_CTL_RX_FUL_EN) && (status & SUNXI_SPI_INT_STA_RX_FULL)) {
		sunxi_debug(sspi->dev, "irq bus rxfifo full\n");
		sunxi_spi_disable_irq(sspi, SUNXI_SPI_INT_CTL_RX_FUL_EN);
	}
	if ((enable & SUNXI_SPI_INT_CTL_RX_EMP_EN) && (status & SUNXI_SPI_INT_STA_RX_EMP)) {
		sunxi_debug(sspi->dev, "irq bus rxfifo empty\n");
		sunxi_spi_disable_irq(sspi, SUNXI_SPI_INT_CTL_RX_EMP_EN);
	}
	if ((enable & SUNXI_SPI_INT_CTL_RX_RDY_EN) && (status & SUNXI_SPI_INT_STA_RX_RDY)) {
		sunxi_debug(sspi->dev, "irq bus rxfifo ready\n");
		sunxi_spi_disable_irq(sspi, SUNXI_SPI_INT_CTL_RX_RDY_EN);
		compelte = true;
	}

	if (compelte)
		complete(&sspi->done);
}

static irqreturn_t sunxi_spi_handler(int irq, void *dev_id)
{
	struct sunxi_spi *sspi = (struct sunxi_spi *)dev_id;

	switch (sspi->bus_mode) {
	case SUNXI_SPI_BUS_MASTER:
	case SUNXI_SPI_BUS_NOR:
	case SUNXI_SPI_BUS_NAND:
	case SUNXI_SPI_BUS_SLAVE:
		sunxi_spi_bus_handler(sspi);
		break;
	case SUNXI_SPI_BUS_DBI:
		sunxi_dbi_handler(sspi);
		break;
	case SUNXI_SPI_BUS_BIT:
		sunxi_spi_bit_handler(sspi);
		break;
	default:
		sunxi_err(sspi->dev, "irq bus mode %d unsupport\n", sspi->bus_mode);
	}

	return IRQ_HANDLED;
}

static irqreturn_t sunxi_spi_ready_handler(int irq, void *dev_id)
{
	struct sunxi_spi *sspi = (struct sunxi_spi *)dev_id;
	int ready_status;

	ready_status = gpio_get_value(sspi->ready_gpio);
	if ((ready_status != sspi->ready_status) && (ready_status == 1)) {
		sunxi_debug(sspi->dev, "ready single is coming\n");
		complete(&sspi->ready_done);
	}

	sspi->ready_status = ready_status;

	return IRQ_HANDLED;
}

static int sunxi_spi_clk_init(struct sunxi_spi *sspi, u32 clk)
{
	int ret = 0;
	long rate = 0;

	ret = reset_control_assert(sspi->reset);
	if (ret) {
		sunxi_err(sspi->dev, "failed to assert reset %d\n", ret);
		goto err0;
	}
	ret = reset_control_deassert(sspi->reset);
	if (ret) {
		sunxi_err(sspi->dev, "failed to deassert reset %d\n", ret);
		goto err0;
	}

	ret = clk_prepare_enable(sspi->pclk);
	if (ret) {
		sunxi_err(sspi->dev, "failed to enable pclk %d\n", ret);
		goto err0;
	}

	ret = clk_set_parent(sspi->mclk, sspi->pclk);
	if (ret) {
		sunxi_err(sspi->dev, "failed set mclk parent to pclk %d\n", ret);
		goto err1;
	}

	if (clk < sspi->ctlr->min_speed_hz || clk > sspi->ctlr->max_speed_hz) {
		sunxi_warn(sspi->dev, "spi init clk %d not support, try %d as default\n", clk, sspi->ctlr->max_speed_hz);
		clk = sspi->ctlr->max_speed_hz;
	}

	rate = clk_round_rate(sspi->mclk, clk);
	if (rate < 0) {
		sunxi_err(sspi->dev, "failed find clk round rate of %d\n", clk);
		goto err1;
	} else if (rate != clk) {
		sunxi_warn(sspi->dev, "clk %d not support, round to nearly %ld\n", clk, rate);
	}

	ret = clk_set_rate(sspi->mclk, rate);
	if (ret) {
		sunxi_err(sspi->dev, "failed set mclk rate to %ld\n", rate);
		goto err1;
	}

	ret = clk_prepare_enable(sspi->mclk);
	if (ret) {
		sunxi_err(sspi->dev, "failed to enable mclk %d\n", ret);
		goto err1;
	}

	ret = clk_prepare_enable(sspi->bus_clk);
	if (ret) {
		sunxi_err(sspi->dev, "failed to enable bus_clk %d\n", ret);
		goto err2;
	}

	sunxi_debug(sspi->dev, "init clock rate %ld success\n", rate);
	return 0;

err2:
	clk_disable_unprepare(sspi->mclk);
err1:
	clk_disable_unprepare(sspi->pclk);
err0:
	return ret;
}

static int sunxi_spi_clk_exit(struct sunxi_spi *sspi)
{
	clk_disable_unprepare(sspi->bus_clk);
	clk_disable_unprepare(sspi->mclk);
	clk_disable_unprepare(sspi->pclk);
	return 0;
}

static int sunxi_spi_hw_init(struct sunxi_spi *sspi, u32 freq)
{
	int ret = 0;

	ret = regulator_enable(sspi->regulator);
	if (ret) {
		sunxi_err(sspi->dev, "failed enable regulator %d\n", ret);
		goto err0;
	}

	ret = sunxi_spi_clk_init(sspi, freq);
	if (ret < 0) {
		sunxi_err(sspi->dev, "init clock rate %d failed %d\n", freq, ret);
		goto err1;
	}

	sunxi_spi_soft_reset(sspi);

	switch (sspi->bus_mode) {
	case SUNXI_SPI_BUS_MASTER:
	case SUNXI_SPI_BUS_NOR:
	case SUNXI_SPI_BUS_NAND:
		sunxi_spi_enable_bus(sspi);
		sunxi_spi_set_master(sspi);
		sunxi_spi_set_dummy_type(sspi, false);
		sunxi_spi_ss_ctrl(sspi, false);
		sunxi_spi_enable_tp(sspi);
		sunxi_spi_ss_owner(sspi, sspi->cs_mode);
		sunxi_spi_delay_chain_init(sspi);
		break;
	case SUNXI_SPI_BUS_SLAVE:
		sunxi_spi_set_slave(sspi);
		sunxi_spi_disable_tp(sspi);
		sunxi_spi_delay_chain_init(sspi);
		break;
	case SUNXI_SPI_BUS_DBI:
		sunxi_spi_set_dbi(sspi);
		sunxi_spi_enable_tp(sspi);
		sunxi_spi_ss_owner(sspi, sspi->cs_mode);
		break;
	case SUNXI_SPI_BUS_BIT:
		sunxi_spi_bit_ss_owner(sspi, sspi->cs_mode);
		break;
	default:
		sunxi_err(sspi->dev, "init bus mode %d unsupport\n", sspi->bus_mode);
		ret = -EINVAL;
		goto err2;
	}

	/* reset fifo */
	sunxi_spi_reset_fifo(sspi);
	sunxi_spi_set_fifo_trig_level_rx(sspi, sspi->rx_triglevel);
	sunxi_spi_set_fifo_trig_level_tx(sspi, sspi->tx_triglevel);

	return 0;

err2:
	sunxi_spi_clk_exit(sspi);
err1:
	regulator_disable(sspi->regulator);
err0:
	return ret;
}

static int sunxi_spi_hw_exit(struct sunxi_spi *sspi)
{
	switch (sspi->bus_mode) {
	case SUNXI_SPI_BUS_MASTER:
	case SUNXI_SPI_BUS_NOR:
	case SUNXI_SPI_BUS_NAND:
		sunxi_spi_disable_bus(sspi);
		sunxi_spi_disable_tp(sspi);
		break;
	case SUNXI_SPI_BUS_SLAVE:
		sunxi_spi_set_master(sspi);
		sunxi_spi_disable_tp(sspi);
		break;
	case SUNXI_SPI_BUS_DBI:
		sunxi_spi_set_spi(sspi);
		sunxi_spi_disable_tp(sspi);
		break;
	case SUNXI_SPI_BUS_BIT:
		break;
	default:
		sunxi_err(sspi->dev, "exit bus mode %d unsupport\n", sspi->bus_mode);
	}

	sunxi_spi_clk_exit(sspi);
	regulator_disable(sspi->regulator);

	return 0;
}

static ssize_t sunxi_spi_info_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct spi_controller *ctlr = dev_get_drvdata(dev);
	struct sunxi_spi *sspi = spi_controller_get_devdata(ctlr);

	return scnprintf(buf, PAGE_SIZE,
		"IP Version = V%d.%d\n"
		"pdev->id   = %d\n"
		"pdev->name = %s\n"
		"pdev->num_resources = %u\n"
		"pdev->resource = %pr\n"
		"ctlr->bus_num = %d\n"
		"ctlr->num_chipselect = %d\n"
		"ctlr->dma_alignment  = %d\n"
		"ctlr->mode_bits = %d\n"
		"ctlr->dma_tx = [%s]\n"
		"ctlr->dma_rx = [%s]\n"
		"ctlr->min_speed_hz = %d\n"
		"ctlr->max_speed_hz = %d\n"
		"sspi->base_addr = 0x%px\n"
		"sspi->irq = %d\n"
		"sspi->bus_mode = %d\n"
		"sspi->bus_num  = %d\n"
		"sspi->bus_freq = %d\n"
		"sspi->cs_mode  = %d\n"
		"sspi->cs_num   = %d\n"
		"sspi->pre_speed_hz = %d\n"
		"sspi->bus_sample_mode  = %d [%s]\n"
		"sspi->spi_sample_mode  = %#x\n"
		"sspi->spi_sample_delay = %#x\n",
		sunxi_spi_get_ver_h(sspi), sunxi_spi_get_ver_l(sspi),
		pdev->id, pdev->name, pdev->num_resources, sspi->mem_res,
		ctlr->bus_num, ctlr->num_chipselect, ctlr->dma_alignment, ctlr->mode_bits,
		sspi->use_dma ? dma_chan_name(ctlr->dma_tx) : "NULL",
		sspi->use_dma ? dma_chan_name(ctlr->dma_rx) : "NULL",
		ctlr->min_speed_hz, ctlr->max_speed_hz,
		sspi->base_addr, sspi->irq, sspi->bus_mode, sspi->bus_num, sspi->bus_freq,
		sspi->cs_mode, sspi->cs_num, sspi->pre_speed_hz,
		sspi->bus_sample_mode, sspi->bus_sample_mode == SUNXI_SPI_SAMP_MODE_AUTO ? "auto" : "manual",
		sspi->spi_sample_mode, sspi->spi_sample_delay);
}

static ssize_t sunxi_spi_status_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct spi_controller *ctlr = dev_get_drvdata(dev);
	struct sunxi_spi *sspi = spi_controller_get_devdata(ctlr);

	return scnprintf(buf, PAGE_SIZE,
			"ctlr->flags = %#x\n"
			"ctlr->busy = %d\n"
			"ctlr->running = %d\n"
			"sspi->mode_type = %d\n"
			"sspi->result = %d\n",
			ctlr->flags, ctlr->busy, ctlr->running,
			sspi->mode_type, sspi->result);
}

static ssize_t sunxi_spi_fifo_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sunxi_spi *sspi = spi_controller_get_devdata(dev_get_drvdata(dev));

	return scnprintf(buf, PAGE_SIZE,
			"tx fifo size      : %d\n"
			"rx fifo size      : %d\n"
			"tx trigger level  : %d\n"
			"rx trigger level  : %d\n\n"
			"tx shift buf count: %d\n"
			"tx buffer count   : %d\n"
			"tx fifo   count   : %d\n"
			"rx buffer count   : %d\n"
			"rx fifo   count   : %d\n",
			sspi->data->tx_fifosize, sspi->data->rx_fifosize, sspi->tx_triglevel, sspi->rx_triglevel,
			sunxi_spi_get_tx_shift_buf_cnt(sspi),
			sunxi_spi_get_txbuf_cnt(sspi), sunxi_spi_get_txfifo_cnt(sspi),
			sunxi_spi_get_rxbuf_cnt(sspi), sunxi_spi_get_rxfifo_cnt(sspi));
}

static ssize_t sunxi_spi_dump_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sunxi_spi *sspi = spi_controller_get_devdata(dev_get_drvdata(dev));
	sunxi_spi_dump_regs(sspi);
	return 0;
}

static struct device_attribute sunxi_spi_debug_attr[] = {
	__ATTR(info, 0444, sunxi_spi_info_show, NULL),
	__ATTR(status, 0444, sunxi_spi_status_show, NULL),
	__ATTR(fifo, 0444, sunxi_spi_fifo_show, NULL),
	__ATTR(dump, 0444, sunxi_spi_dump_show, NULL),
};

static void sunxi_spi_create_sysfs(struct sunxi_spi *sspi)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(sunxi_spi_debug_attr); i++)
		device_create_file(sspi->dev, &sunxi_spi_debug_attr[i]);
}

static void sunxi_spi_remove_sysfs(struct sunxi_spi *sspi)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(sunxi_spi_debug_attr); i++)
		device_remove_file(sspi->dev, &sunxi_spi_debug_attr[i]);
}

static int sunxi_spi_probe(struct platform_device *pdev)
{
	struct sunxi_spi *sspi;
	int ret = 0;

	sspi = devm_kzalloc(&pdev->dev, sizeof(*sspi), GFP_KERNEL);
	if (IS_ERR_OR_NULL(sspi))
		return -ENOMEM;

	sspi->pdev = pdev;
	sspi->dev = &pdev->dev;
	dma_set_mask_and_coherent(sspi->dev, DMA_BIT_MASK(32));
	dma_set_max_seg_size(sspi->dev, PAGE_SIZE);

	sspi->data = of_device_get_match_data(&pdev->dev);
	if (!sspi->data) {
		sunxi_err(sspi->dev, "failed to get platform device data\n");
		return -EINVAL;
	}

	ret = sunxi_spi_resource_get(sspi);
	if (ret < 0) {
		sunxi_err(sspi->dev, "failed to get dts resource %d\n", ret);
		goto err0;
	}

	sspi->rx_triglevel = sspi->data->rx_fifosize / 2;
	sspi->tx_triglevel = sspi->data->tx_fifosize / 2;

	switch (sspi->bus_mode) {
	case SUNXI_SPI_BUS_SLAVE:
		sspi->ctlr = devm_spi_alloc_slave(sspi->dev, 0);
		break;
	case SUNXI_SPI_BUS_DBI:
		sspi->dbi_config = devm_kzalloc(sspi->dev, sizeof(*sspi->dbi_config), GFP_KERNEL);
	fallthrough;
	default:
		sspi->ctlr = devm_spi_alloc_master(sspi->dev, 0);
	}
	if (IS_ERR_OR_NULL(sspi->ctlr)) {
		sunxi_err(sspi->dev, "failed alloc spi controller\n");
		ret = -ENOMEM;
		goto err0;
	}
	platform_set_drvdata(sspi->pdev, sspi->ctlr);
	spi_controller_set_devdata(sspi->ctlr, sspi);

	if (sunxi_spi_request_dma(sspi))
		sspi->use_dma = false;
	else
		sspi->use_dma = true;

	/* spi controller configuration */
	sspi->mode_type			= MODE_TYPE_NULL;
	sspi->ctlr->dev.of_node	= sspi->dev->of_node;
	sspi->ctlr->bus_num		= sspi->bus_num;
	sspi->ctlr->num_chipselect	= sspi->cs_num;
	sspi->ctlr->min_speed_hz	= SUNXI_SPI_MIN_FREQUENCY;
	sspi->ctlr->max_speed_hz	= sspi->bus_freq;
	sspi->ctlr->use_gpio_descriptors = true;
	sspi->ctlr->max_native_cs	= SUNXI_SPI_CS_MAX;
	sspi->ctlr->max_dma_len		= PAGE_SIZE;
	sspi->ctlr->dma_alignment	= dma_get_cache_alignment();
	switch (sspi->bus_mode) {
	case SUNXI_SPI_BUS_SLAVE:
		sspi->ctlr->bits_per_word_mask = SPI_BPW_MASK(8);
		sspi->ctlr->mode_bits = SPI_CPOL | SPI_CPHA | SPI_CS_HIGH | SPI_LSB_FIRST | sspi->data->slave_mode_extra;
		sspi->ctlr->flags = SPI_CONTROLLER_HALF_DUPLEX;
		break;
	case SUNXI_SPI_BUS_MASTER:
	case SUNXI_SPI_BUS_NOR:
	case SUNXI_SPI_BUS_NAND:
	case SUNXI_SPI_BUS_DBI:
		sspi->ctlr->bits_per_word_mask = SPI_BPW_MASK(8);
		sspi->ctlr->mode_bits = SPI_CPOL | SPI_CPHA | SPI_CS_HIGH | SPI_LSB_FIRST | sspi->data->master_mode_extra;
		sspi->ctlr->flags = SPI_MASTER_GPIO_SS;
		break;
	case SUNXI_SPI_BUS_BIT:
		sspi->use_dma = false; /* Bit-Aligned only support cpu mode */
		sspi->ctlr->bits_per_word_mask = SPI_BPW_RANGE_MASK(1, 32);
		sspi->ctlr->mode_bits = SPI_CS_HIGH | SPI_3WIRE;
		sspi->ctlr->flags = SPI_CONTROLLER_HALF_DUPLEX | SPI_MASTER_GPIO_SS;
		break;
	default:
		sunxi_err(sspi->dev, "spi bus mode %d unsupport\n", sspi->bus_mode);
		ret = -EINVAL;
		goto err1;
	}

	/* spi controller function */
	sspi->ctlr->prepare_message		= sunxi_spi_prepare_message;
	sspi->ctlr->unprepare_message	= sunxi_spi_unprepare_message;
	sspi->ctlr->setup			= sunxi_spi_setup;
	sspi->ctlr->set_cs			= sunxi_spi_set_cs;
	if (sspi->use_dma)
		sspi->ctlr->can_dma		= sunxi_spi_can_dma;
	sspi->ctlr->transfer_one	= sunxi_spi_transfer_one;
	sspi->ctlr->handle_err		= sunxi_spi_handle_err;
	switch (sspi->bus_mode) {
	case SUNXI_SPI_BUS_SLAVE:
		sspi->ctlr->slave_abort = sunxi_spi_slave_abort;
		break;
	case SUNXI_SPI_BUS_NAND:
		/* Optimize transfer logic for nand flash to get faster speed and performance */
		sspi->ctlr->transfer_one_message = sunxi_spi_transfer_one_message;
		break;
	case SUNXI_SPI_BUS_MASTER:
	case SUNXI_SPI_BUS_NOR:
	case SUNXI_SPI_BUS_DBI:
		break;
	case SUNXI_SPI_BUS_BIT:
		sspi->ctlr->set_cs = sunxi_spi_bit_set_cs;
		break;
	default:
		sunxi_err(sspi->dev, "spi bus mode %d unsupport\n", sspi->bus_mode);
		ret = -EINVAL;
		goto err1;
	}

	ret = devm_request_irq(sspi->dev, sspi->irq, sunxi_spi_handler, 0, dev_name(sspi->dev), sspi);
	if (ret) {
		sunxi_err(sspi->dev, "failed request irq_%d %d\n", sspi->irq, ret);
		goto err1;
	}

	if (sspi->ready_gpio > 0) {
		if (spi_controller_is_slave(sspi->ctlr)) {
			devm_gpio_request(sspi->dev, sspi->ready_gpio, sspi->ready_label);
			gpio_direction_output(sspi->ready_gpio, 0);
		} else {
			sspi->ctlr->mode_bits |= SPI_READY;
			sspi->ready_status = 0;
			init_completion(&sspi->ready_done);
			ret = devm_request_irq(sspi->dev, sspi->ready_irq, sunxi_spi_ready_handler, IRQ_TYPE_EDGE_BOTH, sspi->ready_label, sspi);
			if (ret) {
				sunxi_err(sspi->dev, "failed request ready_irq_%d %d\n", sspi->ready_irq, ret);
				goto err1;
			}
		}
	}

	ret = sunxi_spi_hw_init(sspi, sspi->bus_freq);
	if (ret) {
		sunxi_err(sspi->dev, "hw init failed\n");
		goto err1;
	}

	raw_spin_lock_init(&sspi->lock);
	init_completion(&sspi->done);

	ret = devm_spi_register_controller(sspi->dev, sspi->ctlr);
	if (ret) {
		sunxi_err(sspi->dev, "failed register spi controller %d\n", ret);
		goto err2;
	}

	sunxi_spi_create_sysfs(sspi);

	sunxi_info(sspi->dev, "probe success (Version %s)\n", SUNXI_SPI_MODULE_VERSION);

	return 0;

err2:
	sunxi_spi_hw_exit(sspi);
err1:
	sunxi_spi_release_dma(sspi);
err0:
	return ret;
}

static int sunxi_spi_remove(struct platform_device *pdev)
{
	struct sunxi_spi *sspi = spi_controller_get_devdata(platform_get_drvdata(pdev));

	sunxi_spi_remove_sysfs(sspi);
	sunxi_spi_hw_exit(sspi);
	sunxi_spi_release_dma(sspi);

	return 0;
}

static int sunxi_spi_suspend(struct device *dev)
{
	struct spi_controller *ctlr = dev_get_drvdata(dev);
	struct sunxi_spi *sspi = spi_controller_get_devdata(ctlr);
	int ret;

	ret = spi_controller_suspend(ctlr);
	if (ret < 0) {
		sunxi_err(sspi->dev, "failed suspend spi controller %d\n", ret);
		return ret;
	}

	sunxi_spi_hw_exit(sspi);

	sunxi_info(sspi->dev, "suspend success\n");
	return 0;
}

static int sunxi_spi_resume(struct device *dev)
{
	struct spi_controller *ctlr = dev_get_drvdata(dev);
	struct sunxi_spi *sspi = spi_controller_get_devdata(ctlr);
	int ret;

	ret = sunxi_spi_hw_init(sspi, sspi->pre_speed_hz);
	if (ret) {
		sunxi_err(sspi->dev, "spi hw resume failed\n");
		return ret;
	}
	sspi->pre_speed_hz = 0;

	ret = spi_controller_resume(ctlr);
	if (ret < 0) {
		sunxi_err(sspi->dev, "failed resume spi controller %d\n", ret);
		return ret;
	}

	sunxi_info(sspi->dev, "resume success\n");
	return 0;
}

static const struct dev_pm_ops sunxi_spi_dev_pm_ops = {
	.suspend = sunxi_spi_suspend,
	.resume  = sunxi_spi_resume,
};

static struct sunxi_spi_hw_data sunxi_sun8iw11_spi_data = {
	.hw_bus_mode = SUNXI_SPI_BUS_MASTER | SUNXI_SPI_BUS_SLAVE,
	.quirk_flag = DMA_FORCE_FIXED,
	.rx_fifosize = 64,
	.tx_fifosize = 64,
};

static struct sunxi_spi_hw_data sunxi_spi_data_v1_1 = {
	.hw_bus_mode = SUNXI_SPI_BUS_MASTER | SUNXI_SPI_BUS_SLAVE | SUNXI_SPI_BUS_BIT | SUNXI_SPI_BUS_NOR | SUNXI_SPI_BUS_NAND,
	.master_mode_extra = SPI_TX_DUAL | SPI_TX_QUAD | SPI_RX_DUAL | SPI_RX_QUAD,
	.quirk_flag = NEW_SAMPLE_MODE | NEW_SAMPLE_MODE_RST,
	.rx_fifosize = 64,
	.tx_fifosize = 64,
};

static struct sunxi_spi_hw_data sunxi_spi_data_v1_2 = {
	.hw_bus_mode = SUNXI_SPI_BUS_MASTER | SUNXI_SPI_BUS_SLAVE | SUNXI_SPI_BUS_DBI | SUNXI_SPI_BUS_BIT | SUNXI_SPI_BUS_NOR | SUNXI_SPI_BUS_NAND,
	.master_mode_extra = SPI_TX_DUAL | SPI_TX_QUAD | SPI_RX_DUAL | SPI_RX_QUAD,
	.quirk_flag = NEW_SAMPLE_MODE | NEW_SAMPLE_MODE_RST,
	.rx_fifosize = 64,
	.tx_fifosize = 64,
};

static struct sunxi_spi_hw_data sunxi_spi_data_v1_3 = {
	.hw_bus_mode = SUNXI_SPI_BUS_MASTER | SUNXI_SPI_BUS_SLAVE | SUNXI_SPI_BUS_BIT | SUNXI_SPI_BUS_NOR | SUNXI_SPI_BUS_NAND,
	.master_mode_extra = SPI_TX_DUAL | SPI_TX_QUAD | SPI_RX_DUAL | SPI_RX_QUAD,
	.quirk_flag = INDEPENDENT_BSR | NEW_SAMPLE_MODE | NEW_SAMPLE_MODE_RST,
	.rx_fifosize = 128,
	.tx_fifosize = 64,
#if IS_ENABLED(CONFIG_ARCH_SUN55IW3)
	.dma_sel_addr = 0x07010000,
	.dma_sel_offset = 0x370,
	.dma_sel_bit = 4,
#endif
};

static struct sunxi_spi_hw_data sunxi_spi_data_v1_4 = {
	.hw_bus_mode = SUNXI_SPI_BUS_MASTER | SUNXI_SPI_BUS_SLAVE | SUNXI_SPI_BUS_DBI | SUNXI_SPI_BUS_BIT | SUNXI_SPI_BUS_NOR | SUNXI_SPI_BUS_NAND,
	.master_mode_extra = SPI_TX_DUAL | SPI_TX_QUAD | SPI_RX_DUAL | SPI_RX_QUAD,
	.quirk_flag = INDEPENDENT_BSR | NEW_SAMPLE_MODE | NEW_SAMPLE_MODE_RST,
	.rx_fifosize = 128,
	.tx_fifosize = 64,
#if IS_ENABLED(CONFIG_ARCH_SUN55IW3)
	.dma_sel_addr = 0x07010000,
	.dma_sel_offset = 0x370,
	.dma_sel_bit = 4,
#endif
};

static const struct of_device_id sunxi_spi_match[] = {
	/* Old platfrom */
	{ .compatible = "allwinner,sun8iw11-spi", .data = &sunxi_sun8iw11_spi_data },
	/* 1886 */
	{ .compatible = "allwinner,sunxi-spi-v1.1", .data = &sunxi_spi_data_v1_1 },
	{ .compatible = "allwinner,sunxi-spi-v1.2", .data = &sunxi_spi_data_v1_2 },
	/* 1890/1885/1903 */
	{ .compatible = "allwinner,sunxi-spi-v1.3", .data = &sunxi_spi_data_v1_3 },
	{ .compatible = "allwinner,sunxi-spi-v1.4", .data = &sunxi_spi_data_v1_4 },
	{},
};
MODULE_DEVICE_TABLE(of, sunxi_spi_match);

static struct platform_driver sunxi_spi_driver = {
	.probe   = sunxi_spi_probe,
	.remove  = sunxi_spi_remove,
	.driver = {
		.name	= SUNXI_SPI_DEV_NAME,
		.pm		= &sunxi_spi_dev_pm_ops,
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

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("SUNXI SPI BUS Driver");
MODULE_ALIAS("platform:"SUNXI_SPI_DEV_NAME);
MODULE_VERSION(SUNXI_SPI_MODULE_VERSION);
MODULE_AUTHOR("jingyanliang <jingyanliang@allwinnertech.com>");
