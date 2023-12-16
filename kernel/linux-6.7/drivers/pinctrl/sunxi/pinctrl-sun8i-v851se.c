// SPDX-License-Identifier: GPL-2.0
/*
 * Allwinner V851s SoC pinctrl driver.
 *
 * Copyright (c) 2016-2021  weidonghui <weidonghui@allwinnertech.com>
 * Copyright (c) 2023 Andras Szemzo <szemzo.andras@gmail.com>
 
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/pinctrl/pinctrl.h>

#include "pinctrl-sunxi.h"

static const struct sunxi_desc_pin v851se_pins[] = {
	/* PA */
	SUNXI_PIN(SUNXI_PINCTRL_PIN(A, 0),
		SUNXI_FUNCTION(0x0, "gpio_in"),		/* gpio_in */
		SUNXI_FUNCTION(0x1, "gpio_out"),	/* gpio_out */
		SUNXI_FUNCTION(0x2, "mipia_rx"),	/* mipia_rx_ckop */
		SUNXI_FUNCTION(0x3, "ncsi"),		/* ncsi_d8 */
		SUNXI_FUNCTION(0x5, "test"),		/* test */
		SUNXI_FUNCTION_IRQ_BANK(0xe, 0, 0)),	/* eint */
	SUNXI_PIN(SUNXI_PINCTRL_PIN(A, 1),
		SUNXI_FUNCTION(0x0, "gpio_in"),		/* gpio_in */
		SUNXI_FUNCTION(0x1, "gpio_out"),	/* gpio_out */
		SUNXI_FUNCTION(0x2, "mipia_rx"),	/* mipia_rx_ckon */
		SUNXI_FUNCTION(0x3, "ncsi"),		/* ncsi_d9 */
		SUNXI_FUNCTION(0x5, "test"),		/* test */
		SUNXI_FUNCTION_IRQ_BANK(0xe, 0, 1)),	/* eint */
	SUNXI_PIN(SUNXI_PINCTRL_PIN(A, 2),
		SUNXI_FUNCTION(0x0, "gpio_in"),		/* gpio_in */
		SUNXI_FUNCTION(0x1, "gpio_out"),	/* gpio_out */
		SUNXI_FUNCTION(0x2, "mipia_rx"),	/* mipia_rx_d1n */
		SUNXI_FUNCTION(0x3, "ncsi"),		/* ncsi_d10 */
		SUNXI_FUNCTION_IRQ_BANK(0xe, 0, 2)),	/* eint */
	SUNXI_PIN(SUNXI_PINCTRL_PIN(A, 3),
		SUNXI_FUNCTION(0x0, "gpio_in"),		/* gpio_in */
		SUNXI_FUNCTION(0x1, "gpio_out"),	/* gpio_out */
		SUNXI_FUNCTION(0x2, "mipia_rx"),	/* mipia_rx_d1p */
		SUNXI_FUNCTION(0x3, "ncsi"),		/* ncsi_d11 */
		SUNXI_FUNCTION_IRQ_BANK(0xe, 0, 3)),	/* eint */
	SUNXI_PIN(SUNXI_PINCTRL_PIN(A, 4),
		SUNXI_FUNCTION(0x0, "gpio_in"),		/* gpio_in */
		SUNXI_FUNCTION(0x1, "gpio_out"),	/* gpio_out */
		SUNXI_FUNCTION(0x2, "mipia_rx"),	/* mipia_rx_d0p */
		SUNXI_FUNCTION(0x3, "ncsi"),		/* ncsi_d12 */
		SUNXI_FUNCTION_IRQ_BANK(0xe, 0, 4)),	/* eint */
	SUNXI_PIN(SUNXI_PINCTRL_PIN(A, 5),
		SUNXI_FUNCTION(0x0, "gpio_in"),		/* gpio_in */
		SUNXI_FUNCTION(0x1, "gpio_out"),	/* gpio_out */
		SUNXI_FUNCTION(0x2, "mipia_rx"),	/* mipia_rx_d0n */
		SUNXI_FUNCTION(0x3, "ncsi"),		/* ncsi_d13 */
		SUNXI_FUNCTION_IRQ_BANK(0xe, 0, 5)),	/* eint */
	SUNXI_PIN(SUNXI_PINCTRL_PIN(A, 6),
		SUNXI_FUNCTION(0x0, "gpio_in"),		/* gpio_in */
		SUNXI_FUNCTION(0x1, "gpio_out"),	/* gpio_out */
		SUNXI_FUNCTION(0x2, "mipib_rx"),	/* mipib_rx_d0n */
		SUNXI_FUNCTION(0x3, "ncsi"),		/* ncsi_d14 */
		SUNXI_FUNCTION(0x4, "twi1"),		/* twi1_sck */
		SUNXI_FUNCTION(0x5, "pwm0"),		/* pwm0 */
		SUNXI_FUNCTION_IRQ_BANK(0xe, 0, 6)),	/* eint */
	SUNXI_PIN(SUNXI_PINCTRL_PIN(A, 7),
		SUNXI_FUNCTION(0x0, "gpio_in"),		/* gpio_in */
		SUNXI_FUNCTION(0x1, "gpio_out"),	/* gpio_out */
		SUNXI_FUNCTION(0x2, "mipib_rx"),	/* mipib_rx_d0p */
		SUNXI_FUNCTION(0x3, "ncsi"),		/* ncsi_d15 */
		SUNXI_FUNCTION(0x4, "twi1"),		/* twi1_sda */
		SUNXI_FUNCTION(0x5, "pwm1"),		/* pwm1 */
		SUNXI_FUNCTION_IRQ_BANK(0xe, 0, 7)),	/* eint */
	SUNXI_PIN(SUNXI_PINCTRL_PIN(A, 8),
		SUNXI_FUNCTION(0x0, "gpio_in"),		/* gpio_in */
		SUNXI_FUNCTION(0x1, "gpio_out"),	/* gpio_out */
		SUNXI_FUNCTION(0x2, "mipib_rx"),	/* mipib_rx_d1n */
		SUNXI_FUNCTION(0x3, "twi4"),		/* twi4_sck */
		SUNXI_FUNCTION(0x4, "twi3"),		/* twi3_sck */
		SUNXI_FUNCTION(0x5, "pwm2"),		/* pwm2 */
		SUNXI_FUNCTION(0x6, "uart2"),		/* uart2_tx */
		SUNXI_FUNCTION_IRQ_BANK(0xe, 0, 8)),	/* eint */
	SUNXI_PIN(SUNXI_PINCTRL_PIN(A, 9),
		SUNXI_FUNCTION(0x0, "gpio_in"),		/* gpio_in */
		SUNXI_FUNCTION(0x1, "gpio_out"),	/* gpio_out */
		SUNXI_FUNCTION(0x2, "mipib_rx"),	/* mipib_rx_d1p */
		SUNXI_FUNCTION(0x3, "twi4"),		/* twi4_sda */
		SUNXI_FUNCTION(0x4, "twi3"),		/* twi3_sda */
		SUNXI_FUNCTION(0x5, "pwm3"),		/* pwm3 */
		SUNXI_FUNCTION(0x6, "uart2"),		/* uart2_rx */
		SUNXI_FUNCTION_IRQ_BANK(0xe, 0, 9)),	/* eint */
	SUNXI_PIN(SUNXI_PINCTRL_PIN(A, 10),
		SUNXI_FUNCTION(0x0, "gpio_in"),		/* gpio_in */
		SUNXI_FUNCTION(0x1, "gpio_out"),	/* gpio_out */
		SUNXI_FUNCTION(0x2, "mipib_rx"),	/* mipib_rx_ckon */
		SUNXI_FUNCTION(0x3, "ncsi"),		/* ncsi_hsync */
		SUNXI_FUNCTION(0x4, "mipi_csi_mclk0"),	/* mipi_csi_mclk0 */
		SUNXI_FUNCTION(0x5, "twi0"),		/* twi0_sck */
		SUNXI_FUNCTION(0x6, "clk"),		/* clk_fanout0 */
		SUNXI_FUNCTION_IRQ_BANK(0xe, 0, 10)),	/* eint */
	SUNXI_PIN(SUNXI_PINCTRL_PIN(A, 11),
		SUNXI_FUNCTION(0x0, "gpio_in"),		/* gpio_in */
		SUNXI_FUNCTION(0x1, "gpio_out"),	/* gpio_out */
		SUNXI_FUNCTION(0x2, "mipib_rx"),	/* mipib_rx_ckop */
		SUNXI_FUNCTION(0x3, "ncsi"),		/* ncsi_vsync */
		SUNXI_FUNCTION(0x4, "mipi_csi_mclk1"),	/* mipi_csi_mclk1 */
		SUNXI_FUNCTION(0x5, "twi0"),		/* twi0_sda */
		SUNXI_FUNCTION(0x6, "clk"),		/* clk_fanout1 */
		SUNXI_FUNCTION_IRQ_BANK(0xe, 0, 11)),	/* eint */
	/* PC */
	SUNXI_PIN(SUNXI_PINCTRL_PIN(C, 0),
		SUNXI_FUNCTION(0x0, "gpio_in"),		/* gpio_in */
		SUNXI_FUNCTION(0x1, "gpio_out"),	/* gpio_out */
		SUNXI_FUNCTION(0x2, "spif"),		/* spif_clk */
		SUNXI_FUNCTION(0x3, "sdc2"),		/* sdc2_clk */
		SUNXI_FUNCTION(0x4, "spi0"),		/* spi0_clk */
		SUNXI_FUNCTION_IRQ_BANK(0xe, 1, 0)),	/* eint */
	SUNXI_PIN(SUNXI_PINCTRL_PIN(C, 1),
		SUNXI_FUNCTION(0x0, "gpio_in"),		/* gpio_in */
		SUNXI_FUNCTION(0x1, "gpio_out"),	/* gpio_out */
		SUNXI_FUNCTION(0x2, "spif"),		/* spif_cs0 */
		SUNXI_FUNCTION(0x3, "sdc2"),		/* sdc2_cmd */
		SUNXI_FUNCTION(0x4, "spi0"),		/* spi0_cs0 */
		SUNXI_FUNCTION_IRQ_BANK(0xe, 1, 1)),	/* eint */
	SUNXI_PIN(SUNXI_PINCTRL_PIN(C, 2),
		SUNXI_FUNCTION(0x0, "gpio_in"),		/* gpio_in */
		SUNXI_FUNCTION(0x1, "gpio_out"),	/* gpio_out */
		SUNXI_FUNCTION(0x2, "spif"),		/* spif_mosi_io0 */
		SUNXI_FUNCTION(0x3, "sdc2"),		/* sdc2_d2 */
		SUNXI_FUNCTION(0x4, "spi0"),		/* spi0_mosi */
		SUNXI_FUNCTION(0x5, "boot_sel0"),	/* boot_sel0 */
		SUNXI_FUNCTION_IRQ_BANK(0xe, 1, 2)),	/* eint */
	SUNXI_PIN(SUNXI_PINCTRL_PIN(C, 3),
		SUNXI_FUNCTION(0x0, "gpio_in"),		/* gpio_in */
		SUNXI_FUNCTION(0x1, "gpio_out"),	/* gpio_out */
		SUNXI_FUNCTION(0x2, "spif"),		/* spif_mosi_io1 */
		SUNXI_FUNCTION(0x3, "sdc2"),		/* sdc2_d1 */
		SUNXI_FUNCTION(0x4, "spi0"),		/* spi0_miso */
		SUNXI_FUNCTION(0x5, "boot_sel1"),	/* boot_sel1 */
		SUNXI_FUNCTION_IRQ_BANK(0xe, 1, 3)),	/* eint */
	SUNXI_PIN(SUNXI_PINCTRL_PIN(C, 4),
		SUNXI_FUNCTION(0x0, "gpio_in"),		/* gpio_in */
		SUNXI_FUNCTION(0x1, "gpio_out"),	/* gpio_out */
		SUNXI_FUNCTION(0x2, "spif"),		/* spif_wp_io2 */
		SUNXI_FUNCTION(0x3, "sdc2"),		/* sdc2_d0 */
		SUNXI_FUNCTION(0x4, "spi0"),		/* spi0_wp */
		SUNXI_FUNCTION(0x5, "pwm4"),		/* pwm4 */
		SUNXI_FUNCTION(0x6, "twi1"),		/* twi1_sck */
		SUNXI_FUNCTION_IRQ_BANK(0xe, 1, 4)),	/* eint */
	SUNXI_PIN(SUNXI_PINCTRL_PIN(C, 5),
		SUNXI_FUNCTION(0x0, "gpio_in"),		/* gpio_in */
		SUNXI_FUNCTION(0x1, "gpio_out"),	/* gpio_out */
		SUNXI_FUNCTION(0x2, "spif"),		/* spif_hold_io3 */
		SUNXI_FUNCTION(0x3, "sdc2"),		/* sdc2_d3 */
		SUNXI_FUNCTION(0x4, "spi0"),		/* spi0_hold */
		SUNXI_FUNCTION(0x5, "pwm4"),		/* pwm5 */
		SUNXI_FUNCTION(0x6, "twi1"),		/* twi1_sda */
		SUNXI_FUNCTION_IRQ_BANK(0xe, 1, 5)),	/* eint */
	/* PD */
	SUNXI_PIN(SUNXI_PINCTRL_PIN(D, 0),
		SUNXI_FUNCTION(0x0, "gpio_in"),		/* gpio_in */
		SUNXI_FUNCTION(0x1, "gpio_out"),	/* gpio_out */
		SUNXI_FUNCTION(0x2, "lcd"),		/* lcd_d2 */
		SUNXI_FUNCTION_IRQ_BANK(0xe, 2, 0)),	/* eint */
	SUNXI_PIN(SUNXI_PINCTRL_PIN(D, 1),
		SUNXI_FUNCTION(0x0, "gpio_in"),		/* gpio_in */
		SUNXI_FUNCTION(0x1, "gpio_out"),	/* gpio_out */
		SUNXI_FUNCTION(0x2, "lcd"),		/* lcd_d3*/
		SUNXI_FUNCTION(0x3, "pwm0"),		/* pwm0 */
		SUNXI_FUNCTION(0x5, "dsi"),		/* dsi_d0n */
		SUNXI_FUNCTION(0x6, "spi1"),		/* spi1_cs0/dbi_csx */
		SUNXI_FUNCTION(0x7, "rmii"),		/* rmii_txd0 */
		SUNXI_FUNCTION_IRQ_BANK(0xe, 2, 1)),	/* eint */
	SUNXI_PIN(SUNXI_PINCTRL_PIN(D, 2),
		SUNXI_FUNCTION(0x0, "gpio_in"),		/* gpio_in */
		SUNXI_FUNCTION(0x1, "gpio_out"),	/* gpio_out */
		SUNXI_FUNCTION(0x2, "lcd"),		/* lcd_d4*/
		SUNXI_FUNCTION(0x3, "pwm1"),		/* pwm1 */
		SUNXI_FUNCTION(0x5, "dsi"),		/* dsi_d0p */
		SUNXI_FUNCTION(0x6, "spi1"),		/* spi1_clk/dbi_sclk */
		SUNXI_FUNCTION(0x7, "rmii"),		/* rmii_txd1 */
		SUNXI_FUNCTION_IRQ_BANK(0xe, 2, 2)),	/* eint */
	SUNXI_PIN(SUNXI_PINCTRL_PIN(D, 3),
		SUNXI_FUNCTION(0x0, "gpio_in"),		/* gpio_in */
		SUNXI_FUNCTION(0x1, "gpio_out"),	/* gpio_out */
		SUNXI_FUNCTION(0x2, "lcd"),		/* lcd_d5*/
		SUNXI_FUNCTION(0x3, "pwm2"),		/* pwm2 */
		SUNXI_FUNCTION(0x5, "dsi"),		/* dsi_d1n */
		SUNXI_FUNCTION(0x6, "spi1"),		/* spi1_mosi/dbi_sdo */
		SUNXI_FUNCTION(0x7, "rmii"),		/* rmii_rxer */
		SUNXI_FUNCTION_IRQ_BANK(0xe, 2, 3)),	/* eint */
	SUNXI_PIN(SUNXI_PINCTRL_PIN(D, 4),
		SUNXI_FUNCTION(0x0, "gpio_in"),		/* gpio_in */
		SUNXI_FUNCTION(0x1, "gpio_out"),	/* gpio_out */
		SUNXI_FUNCTION(0x2, "lcd"),		/* lcd_d6*/
		SUNXI_FUNCTION(0x3, "pwm3"),		/* pwm3 */
		SUNXI_FUNCTION(0x5, "dsi"),		/* dsi_d1p */
		SUNXI_FUNCTION(0x6, "spi1"),		/* spi1_miso/dbi_sdi/dbi_te/dbi_dcx */
		SUNXI_FUNCTION(0x7, "rmii"),		/* rmii_crs_dv */
		SUNXI_FUNCTION_IRQ_BANK(0xe, 2, 4)),	/* eint */
	SUNXI_PIN(SUNXI_PINCTRL_PIN(D, 5),
		SUNXI_FUNCTION(0x0, "gpio_in"),		/* gpio_in */
		SUNXI_FUNCTION(0x1, "gpio_out"),	/* gpio_out */
		SUNXI_FUNCTION(0x2, "lcd"),		/* lcd_d7*/
		SUNXI_FUNCTION(0x3, "pwm4"),		/* pwm4 */
		SUNXI_FUNCTION(0x5, "dsi"),		/* dsi_ckn */
		SUNXI_FUNCTION(0x6, "spi1"),		/* spi1_hold/dbi_dcx/dbi_wrx */
		SUNXI_FUNCTION(0x7, "rmii"),		/* rmii_rxd1 */
		SUNXI_FUNCTION_IRQ_BANK(0xe, 2, 5)),	/* eint */
	SUNXI_PIN(SUNXI_PINCTRL_PIN(D, 6),
		SUNXI_FUNCTION(0x0, "gpio_in"),		/* gpio_in */
		SUNXI_FUNCTION(0x1, "gpio_out"),	/* gpio_out */
		SUNXI_FUNCTION(0x2, "lcd"),		/* lcd_d10*/
		SUNXI_FUNCTION(0x3, "pwm5"),		/* pwm5 */
		SUNXI_FUNCTION(0x5, "dsi"),		/* dsi_ckp */
		SUNXI_FUNCTION(0x6, "spi1"),		/* spi1_wp/dbi_te */
		SUNXI_FUNCTION(0x7, "rmii"),		/* rmii_rxd0 */
		SUNXI_FUNCTION_IRQ_BANK(0xe, 2, 6)),	/* eint */
	SUNXI_PIN(SUNXI_PINCTRL_PIN(D, 7),
		SUNXI_FUNCTION(0x0, "gpio_in"),		/* gpio_in */
		SUNXI_FUNCTION(0x1, "gpio_out"),	/* gpio_out */
		SUNXI_FUNCTION(0x2, "lcd"),		/* lcd_d11*/
		SUNXI_FUNCTION(0x3, "pwm6"),		/* pwm6 */
		SUNXI_FUNCTION(0x5, "dsi"),		/* dsi_d2n */
		SUNXI_FUNCTION(0x6, "spi1"),		/* spi1_cs1 */
		SUNXI_FUNCTION(0x7, "rmii"),		/* mdc */
		SUNXI_FUNCTION_IRQ_BANK(0xe, 2, 7)),	/* eint */
	SUNXI_PIN(SUNXI_PINCTRL_PIN(D, 8),
		SUNXI_FUNCTION(0x0, "gpio_in"),		/* gpio_in */
		SUNXI_FUNCTION(0x1, "gpio_out"),	/* gpio_out */
		SUNXI_FUNCTION(0x2, "lcd"),		/* lcd_d12*/
		SUNXI_FUNCTION(0x3, "pwm7"),		/* pwm7 */
		SUNXI_FUNCTION(0x4, "rmii"),		/* rmii_txen */
		SUNXI_FUNCTION_IRQ_BANK(0xe, 2, 8)),	/* eint */
	SUNXI_PIN(SUNXI_PINCTRL_PIN(D, 18),
		SUNXI_FUNCTION(0x0, "gpio_in"),		/* gpio_in */
		SUNXI_FUNCTION(0x1, "gpio_out"),	/* gpio_out */
		SUNXI_FUNCTION(0x2, "lcd"),		/* lcd_clk*/
		SUNXI_FUNCTION(0x4, "ephy_25m"),	/* ephy_25m */
		SUNXI_FUNCTION(0x5, "spi2"),		/* spi2_clk */
		SUNXI_FUNCTION(0x6, "twi3"),		/* twi3_sck */
		SUNXI_FUNCTION(0x7, "uart2"),		/* uart2_tx */
		SUNXI_FUNCTION_IRQ_BANK(0xe, 2, 18)),	/* eint */
	SUNXI_PIN(SUNXI_PINCTRL_PIN(D, 19),
		SUNXI_FUNCTION(0x0, "gpio_in"),		/* gpio_in */
		SUNXI_FUNCTION(0x1, "gpio_out"),	/* gpio_out */
		SUNXI_FUNCTION(0x2, "lcd"),		/* lcd_de*/
		SUNXI_FUNCTION(0x3, "pwm9"),		/* pwm9*/
		SUNXI_FUNCTION(0x4, "tcon"),		/* tcon_trig */
		SUNXI_FUNCTION(0x5, "spi2"),		/* spi2_mosi */
		SUNXI_FUNCTION(0x6, "twi3"),		/* twi3_sda */
		SUNXI_FUNCTION(0x7, "uart2"),		/* uart2_rx */
		SUNXI_FUNCTION_IRQ_BANK(0xe, 2, 19)),	/* eint */
	SUNXI_PIN(SUNXI_PINCTRL_PIN(D, 20),
		SUNXI_FUNCTION(0x0, "gpio_in"),		/* gpio_in */
		SUNXI_FUNCTION(0x1, "gpio_out"),	/* gpio_out */
		SUNXI_FUNCTION(0x2, "lcd"),		/* lcd_hsync*/
		SUNXI_FUNCTION(0x3, "pwm10"),		/* pwm10*/
		SUNXI_FUNCTION(0x4, "mdc"),		/* mdc */
		SUNXI_FUNCTION(0x5, "spi2"),		/* spi2_miso */
		SUNXI_FUNCTION(0x6, "twi2"),		/* twi2_sck */
		SUNXI_FUNCTION(0x7, "uart2"),		/* uart2_rts */
		SUNXI_FUNCTION_IRQ_BANK(0xe, 2, 20)),	/* eint */
	SUNXI_PIN(SUNXI_PINCTRL_PIN(D, 21),
		SUNXI_FUNCTION(0x0, "gpio_in"),		/* gpio_in */
		SUNXI_FUNCTION(0x1, "gpio_out"),	/* gpio_out */
		SUNXI_FUNCTION(0x2, "lcd"),		/* lcd_vsync*/
		SUNXI_FUNCTION(0x4, "mdio"),		/* mdio */
		SUNXI_FUNCTION(0x5, "spi2"),		/* spi2_cs0 */
		SUNXI_FUNCTION(0x6, "twi2"),		/* twi2_sda */
		SUNXI_FUNCTION(0x7, "uart2"),		/* uart2_cts */
		SUNXI_FUNCTION_IRQ_BANK(0xe, 2, 21)),	/* eint */
	/* PE */
	SUNXI_PIN(SUNXI_PINCTRL_PIN(E, 0),
		SUNXI_FUNCTION(0x0, "gpio_in"),		/* gpio_in */
		SUNXI_FUNCTION(0x1, "gpio_out"),	/* gpio_out */
		SUNXI_FUNCTION(0x2, "ncsi"),		/* ncsi_pclk*/
		SUNXI_FUNCTION(0x3, "rgmii"),		/* rgmii_rxd/rmii_rxd1*/
		SUNXI_FUNCTION(0x4, "i2s1"),		/* i2s1_mclk */
		SUNXI_FUNCTION(0x5, "pwm0"),		/* pwm0 */
		SUNXI_FUNCTION(0x6, "sdc1"),		/* sdc_clk */
		SUNXI_FUNCTION(0x7, "uart3"),		/* uart3_tx */
		SUNXI_FUNCTION(0x8, "twi3"),		/* twi3_sck */
		SUNXI_FUNCTION_IRQ_BANK(0xe, 3, 0)),	/* eint */
	SUNXI_PIN(SUNXI_PINCTRL_PIN(E, 1),
		SUNXI_FUNCTION(0x0, "gpio_in"),		/* gpio_in */
		SUNXI_FUNCTION(0x1, "gpio_out"),	/* gpio_out */
		SUNXI_FUNCTION(0x2, "ncsi_mclk"),		/* ncsi_mclk*/
		SUNXI_FUNCTION(0x3, "rgmii"),		/* rgmii_rxck/rmii_txck*/
		SUNXI_FUNCTION(0x4, "i2s1"),		/* i2s1_bclk */
		SUNXI_FUNCTION(0x5, "pwm1"),		/* pwm1 */
		SUNXI_FUNCTION(0x6, "sdc1"),		/* sdc_cmd */
		SUNXI_FUNCTION(0x7, "uart3"),		/* uart3_rx */
		SUNXI_FUNCTION(0x8, "twi3"),		/* twi3_sda */
		SUNXI_FUNCTION_IRQ_BANK(0xe, 3, 1)),	/* eint */
	SUNXI_PIN(SUNXI_PINCTRL_PIN(E, 2),
		SUNXI_FUNCTION(0x0, "gpio_in"),		/* gpio_in */
		SUNXI_FUNCTION(0x1, "gpio_out"),	/* gpio_out */
		SUNXI_FUNCTION(0x2, "ncsi"),		/* ncsi_hsync*/
		SUNXI_FUNCTION(0x3, "rgmii"),		/* rgmii_rxctl/rmii_crs_dv*/
		SUNXI_FUNCTION(0x4, "i2s1"),		/* i2s1_lclk */
		SUNXI_FUNCTION(0x5, "pwm2"),		/* pwm2 */
		SUNXI_FUNCTION(0x6, "sdc1"),		/* sdc_d0 */
		SUNXI_FUNCTION(0x7, "uart3"),		/* uart3_cts */
		SUNXI_FUNCTION(0x8, "twi1"),		/* twi1_sck */
		SUNXI_FUNCTION_IRQ_BANK(0xe, 3, 2)),	/* eint */
	SUNXI_PIN(SUNXI_PINCTRL_PIN(E, 3),
		SUNXI_FUNCTION(0x0, "gpio_in"),		/* gpio_in */
		SUNXI_FUNCTION(0x1, "gpio_out"),	/* gpio_out */
		SUNXI_FUNCTION(0x2, "ncsi"),		/* ncsi_vsync*/
		SUNXI_FUNCTION(0x3, "rgmii"),		/* rgmii_rxd0/rmii_rxd0*/
		SUNXI_FUNCTION(0x4, "i2s1"),		/* i2s1_din0 */
		SUNXI_FUNCTION(0x5, "pwm3"),		/* pwm3 */
		SUNXI_FUNCTION(0x6, "sdc1"),		/* sdc_d1 */
		SUNXI_FUNCTION(0x7, "uart3"),		/* uart3_rts */
		SUNXI_FUNCTION(0x8, "twi1"),		/* twi1_sda */
		SUNXI_FUNCTION_IRQ_BANK(0xe, 3, 3)),	/* eint */
	SUNXI_PIN(SUNXI_PINCTRL_PIN(E, 4),
		SUNXI_FUNCTION(0x0, "gpio_in"),		/* gpio_in */
		SUNXI_FUNCTION(0x1, "gpio_out"),	/* gpio_out */
		SUNXI_FUNCTION(0x2, "ncsi"),		/* ncsi_d0*/
		SUNXI_FUNCTION(0x3, "rgmii"),		/* rgmii_txd0/rmii_txd0*/
		SUNXI_FUNCTION(0x4, "i2s1"),		/* i2s1_dout0 */
		SUNXI_FUNCTION(0x5, "pwm4"),		/* pwm4 */
		SUNXI_FUNCTION(0x6, "sdc1"),		/* sdc_d2 */
		SUNXI_FUNCTION(0x7, "uart3"),		/* uart3_sck */
		SUNXI_FUNCTION(0x8, "twi0"),		/* twi0_sck */
		SUNXI_FUNCTION_IRQ_BANK(0xe, 3, 4)),	/* eint */
	SUNXI_PIN(SUNXI_PINCTRL_PIN(E, 5),
		SUNXI_FUNCTION(0x0, "gpio_in"),		/* gpio_in */
		SUNXI_FUNCTION(0x1, "gpio_out"),	/* gpio_out */
		SUNXI_FUNCTION(0x2, "ncsi"),		/* ncsi_d1*/
		SUNXI_FUNCTION(0x3, "rgmii"),		/* rgmii_txd1/rmii_txd1*/
		SUNXI_FUNCTION(0x5, "pwm5"),		/* pwm5 */
		SUNXI_FUNCTION(0x6, "sdc1"),		/* sdc_d3 */
		SUNXI_FUNCTION(0x7, "uart3"),		/* uart3_sda */
		SUNXI_FUNCTION(0x8, "twi0"),		/* twi0_sda */
		SUNXI_FUNCTION_IRQ_BANK(0xe, 3, 5)),	/* eint */
	SUNXI_PIN(SUNXI_PINCTRL_PIN(E, 6),
		SUNXI_FUNCTION(0x0, "gpio_in"),		/* gpio_in */
		SUNXI_FUNCTION(0x1, "gpio_out"),	/* gpio_out */
		SUNXI_FUNCTION(0x2, "ncsi"),		/* ncsi_d2*/
		SUNXI_FUNCTION(0x3, "rgmii"),		/* rgmii_txctl/rmii_txen*/
		SUNXI_FUNCTION(0x4, "lcd"),		/* lcd_d2 */
		SUNXI_FUNCTION(0x5, "pwm6"),		/* pwm6 */
		SUNXI_FUNCTION(0x6, "uart1"),		/* uart1_tx */
		SUNXI_FUNCTION(0x8, "twi4"),		/* twi4_sck */
		SUNXI_FUNCTION_IRQ_BANK(0xe, 3, 6)),	/* eint */
	SUNXI_PIN(SUNXI_PINCTRL_PIN(E, 7),
		SUNXI_FUNCTION(0x0, "gpio_in"),		/* gpio_in */
		SUNXI_FUNCTION(0x1, "gpio_out"),	/* gpio_out */
		SUNXI_FUNCTION(0x2, "ncsi"),		/* ncsi_d3*/
		SUNXI_FUNCTION(0x3, "rgmii"),		/* rgmii_clkin/rmii_rxer*/
		SUNXI_FUNCTION(0x4, "lcd"),		/* lcd_d15 */
		SUNXI_FUNCTION(0x5, "pwm7"),		/* pwm7 */
		SUNXI_FUNCTION(0x6, "uart1"),		/* uart1_tx */
		SUNXI_FUNCTION(0x7, "i2s1"),		/* i2s1_dout0 */
		SUNXI_FUNCTION(0x8, "twi4"),		/* twi4_sda */
		SUNXI_FUNCTION_IRQ_BANK(0xe, 3, 7)),	/* eint */
	SUNXI_PIN(SUNXI_PINCTRL_PIN(E, 8),
		SUNXI_FUNCTION(0x0, "gpio_in"),		/* gpio_in */
		SUNXI_FUNCTION(0x1, "gpio_out"),	/* gpio_out */
		SUNXI_FUNCTION(0x2, "ncsi"),		/* ncsi_d4*/
		SUNXI_FUNCTION(0x3, "rgmii"),		/* mdc*/
		SUNXI_FUNCTION(0x4, "lcd"),		/* lcd_d18 */
		SUNXI_FUNCTION(0x5, "pwm8"),		/* pwm8 */
		SUNXI_FUNCTION(0x6, "wiegand"),		/* wiegand_d0 */
		SUNXI_FUNCTION(0x7, "i2s1"),		/* i2s1_din0 */
		SUNXI_FUNCTION(0x8, "twi1"),		/* twi1_sck */
		SUNXI_FUNCTION_IRQ_BANK(0xe, 3, 8)),	/* eint */
	SUNXI_PIN(SUNXI_PINCTRL_PIN(E, 9),
		SUNXI_FUNCTION(0x0, "gpio_in"),		/* gpio_in */
		SUNXI_FUNCTION(0x1, "gpio_out"),	/* gpio_out */
		SUNXI_FUNCTION(0x2, "ncsi"),		/* ncsi_d5*/
		SUNXI_FUNCTION(0x3, "rgmii"),		/* mdio*/
		SUNXI_FUNCTION(0x4, "lcd"),		/* lcd_d19 */
		SUNXI_FUNCTION(0x5, "pwm9"),		/* pwm9 */
		SUNXI_FUNCTION(0x6, "wiegand"),		/* wiegand_d1 */
		SUNXI_FUNCTION(0x7, "i2s1"),		/* i2s1_lrck */
		SUNXI_FUNCTION(0x8, "twi1"),		/* twi1_sda */
		SUNXI_FUNCTION_IRQ_BANK(0xe, 3, 9)),	/* eint */
	SUNXI_PIN(SUNXI_PINCTRL_PIN(E, 10),
		SUNXI_FUNCTION(0x0, "gpio_in"),		/* gpio_in */
		SUNXI_FUNCTION(0x1, "gpio_out"),	/* gpio_out */
		SUNXI_FUNCTION(0x2, "ncsi"),		/* ncsi_d6 */
		SUNXI_FUNCTION(0x3, "rgmii"),		/* ephy_25m */
		SUNXI_FUNCTION(0x4, "lcd"),		/* lcd_d20 */
		SUNXI_FUNCTION(0x5, "pwm10"),		/* pwm10 */
		SUNXI_FUNCTION(0x6, "uart2"),		/* uart2_rts */
		SUNXI_FUNCTION(0x7, "i2s1"),		/* i2s1_bclk */
		SUNXI_FUNCTION(0x8, "wiegand"),		/* wiegand_d0 */
		SUNXI_FUNCTION_IRQ_BANK(0xe, 3, 10)),	/* eint */
	SUNXI_PIN(SUNXI_PINCTRL_PIN(E, 11),
		SUNXI_FUNCTION(0x0, "gpio_in"),		/* gpio_in */
		SUNXI_FUNCTION(0x1, "gpio_out"),	/* gpio_out */
		SUNXI_FUNCTION(0x2, "ncsi"),		/* ncsi_d7 */
		SUNXI_FUNCTION(0x3, "rgmii"),		/* rgmii_rxd3 */
		SUNXI_FUNCTION(0x4, "lcd"),		/* lcd_d21 */
		SUNXI_FUNCTION(0x5, "csi"),		/* csi_sm_vs */
		SUNXI_FUNCTION(0x6, "uart2"),		/* uart2_cts */
		SUNXI_FUNCTION(0x7, "i2s1"),		/* i2s1_mclk */
		SUNXI_FUNCTION(0x8, "wiegand"),		/* wiegand_d1 */
		SUNXI_FUNCTION_IRQ_BANK(0xe, 3, 11)),	/* eint */
	SUNXI_PIN(SUNXI_PINCTRL_PIN(E, 12),
		SUNXI_FUNCTION(0x0, "gpio_in"),		/* gpio_in */
		SUNXI_FUNCTION(0x1, "gpio_out"),	/* gpio_out */
		SUNXI_FUNCTION(0x2, "ncsi"),		/* ncsi_d8 */
		SUNXI_FUNCTION(0x3, "rgmii"),		/* rgmii_rxd2 */
		SUNXI_FUNCTION(0x4, "lcd"),		/* lcd_d22 */
		SUNXI_FUNCTION(0x5, "mipi_csi_mclk0"),	/* mipi_csi_mclk0 */
		SUNXI_FUNCTION(0x6, "uart2"),		/* uart2_tx */
		SUNXI_FUNCTION(0x7, "uart3"),		/* uart3_tx */
		SUNXI_FUNCTION_IRQ_BANK(0xe, 3, 12)),	/* eint */
	SUNXI_PIN(SUNXI_PINCTRL_PIN(E, 13),
		SUNXI_FUNCTION(0x0, "gpio_in"),		/* gpio_in */
		SUNXI_FUNCTION(0x1, "gpio_out"),	/* gpio_out */
		SUNXI_FUNCTION(0x2, "ncsi"),		/* ncsi_d9 */
		SUNXI_FUNCTION(0x3, "rgmii"),		/* rgmii_rxck */
		SUNXI_FUNCTION(0x4, "lcd"),		/* lcd_d23 */
		SUNXI_FUNCTION(0x5, "mipi_csi_mclk1"),	/* mipi_csi_mclk1 */
		SUNXI_FUNCTION(0x6, "uart2"),		/* uart2_rx */
		SUNXI_FUNCTION(0x7, "uart3"),		/* uart3_rx */
		SUNXI_FUNCTION_IRQ_BANK(0xe, 3, 13)),	/* eint */
	/* PF */
	SUNXI_PIN(SUNXI_PINCTRL_PIN(F, 0),
		SUNXI_FUNCTION(0x0, "gpio_in"),		/* gpio_in */
		SUNXI_FUNCTION(0x1, "gpio_out"),	/* gpio_out */
		SUNXI_FUNCTION(0x2, "sdc0"),		/* sdc0_d1 */
		SUNXI_FUNCTION(0x3, "uart0_jtag"),	/* jtag_ms */
		SUNXI_FUNCTION(0x4, "spi0"),		/* spi0_clk */
		SUNXI_FUNCTION(0x5, "spi2"),		/* spi2_clk */
		SUNXI_FUNCTION(0x6, "r_jtag"),		/* r_jtag_ms */
		SUNXI_FUNCTION(0x7, "cpu"),		/* cpu_bist0 */
		SUNXI_FUNCTION_IRQ_BANK(0xe, 4, 0)),	/* eint */
	SUNXI_PIN(SUNXI_PINCTRL_PIN(F, 1),
		SUNXI_FUNCTION(0x0, "gpio_in"),		/* gpio_in */
		SUNXI_FUNCTION(0x1, "gpio_out"),	/* gpio_out */
		SUNXI_FUNCTION(0x2, "sdc0"),		/* sdc0_d0 */
		SUNXI_FUNCTION(0x3, "uart0_jtag"),	/* jtag_di */
		SUNXI_FUNCTION(0x4, "spi0"),		/* spi0_mosi */
		SUNXI_FUNCTION(0x5, "spi2"),		/* spi2_mosi */
		SUNXI_FUNCTION(0x6, "r_jtag"),		/* r_jtag_di */
		SUNXI_FUNCTION(0x7, "cpu"),		/* cpu_bist1 */
		SUNXI_FUNCTION_IRQ_BANK(0xe, 4, 1)),	/* eint */
	SUNXI_PIN(SUNXI_PINCTRL_PIN(F, 2),
		SUNXI_FUNCTION(0x0, "gpio_in"),		/* gpio_in */
		SUNXI_FUNCTION(0x1, "gpio_out"),	/* gpio_out */
		SUNXI_FUNCTION(0x2, "sdc0"),		/* sdc0_clk */
		SUNXI_FUNCTION(0x3, "uart0_jtag"),	/* uart0_tx */
		SUNXI_FUNCTION(0x4, "spi0"),		/* spi0_miso */
		SUNXI_FUNCTION(0x5, "spi2"),		/* spi2_miso */
		SUNXI_FUNCTION_IRQ_BANK(0xe, 4, 2)),	/* eint */
	SUNXI_PIN(SUNXI_PINCTRL_PIN(F, 3),
		SUNXI_FUNCTION(0x0, "gpio_in"),		/* gpio_in */
		SUNXI_FUNCTION(0x1, "gpio_out"),	/* gpio_out */
		SUNXI_FUNCTION(0x2, "sdc0"),		/* sdc0_cmd */
		SUNXI_FUNCTION(0x3, "uart0_jtag"),	/* jtag_do */
		SUNXI_FUNCTION(0x4, "spi0"),		/* spi0_cs0 */
		SUNXI_FUNCTION(0x5, "spi2"),		/* spi2_cs0 */
		SUNXI_FUNCTION(0x6, "r_jtag"),		/* r_jtag_do */
		SUNXI_FUNCTION_IRQ_BANK(0xe, 4, 3)),	/* eint */
	SUNXI_PIN(SUNXI_PINCTRL_PIN(F, 4),
		SUNXI_FUNCTION(0x0, "gpio_in"),		/* gpio_in */
		SUNXI_FUNCTION(0x1, "gpio_out"),	/* gpio_out */
		SUNXI_FUNCTION(0x2, "sdc0"),		/* sdc0_d3 */
		SUNXI_FUNCTION(0x3, "uart0_jtag"),	/* uart0_rx */
		SUNXI_FUNCTION(0x4, "spi0"),		/* spi0_cs1 */
		SUNXI_FUNCTION(0x5, "spi2"),		/* spi2_cs1 */
		SUNXI_FUNCTION_IRQ_BANK(0xe, 4, 4)),	/* eint */
	SUNXI_PIN(SUNXI_PINCTRL_PIN(F, 5),
		SUNXI_FUNCTION(0x0, "gpio_in"),		/* gpio_in */
		SUNXI_FUNCTION(0x1, "gpio_out"),	/* gpio_out */
		SUNXI_FUNCTION(0x2, "sdc0"),		/* sdc0_d2 */
		SUNXI_FUNCTION(0x3, "uart0_jtag"),	/* jtag_ck */
		SUNXI_FUNCTION(0x6, "r_jtag"),		/* r_jtag_ck */
		SUNXI_FUNCTION_IRQ_BANK(0xe, 4, 5)),	/* eint */
	SUNXI_PIN(SUNXI_PINCTRL_PIN(F, 6),
		SUNXI_FUNCTION(0x0, "gpio_in"),		/* gpio_in */
		SUNXI_FUNCTION(0x1, "gpio_out"),	/* gpio_out */
		SUNXI_FUNCTION(0x2, "dbg_clk"),		/* dbg_clk */
		SUNXI_FUNCTION_IRQ_BANK(0xe, 4, 6)),	/* eint */
	/* PH */
	SUNXI_PIN(SUNXI_PINCTRL_PIN(H, 0),
		SUNXI_FUNCTION(0x0, "gpio_in"),		/* gpio_in */
		SUNXI_FUNCTION(0x1, "gpio_out"),	/* gpio_out */
		SUNXI_FUNCTION(0x2, "pwm0"),		/* pwm0 */
		SUNXI_FUNCTION(0x3, "i2s0"),		/* i2s0_mclk */
		SUNXI_FUNCTION(0x4, "spi1"),		/* spi1_clk */
		SUNXI_FUNCTION(0x5, "uart3"),		/* uart3_tx */
		SUNXI_FUNCTION(0x6, "dmic"),		/* dmic_data3 */
		SUNXI_FUNCTION_IRQ_BANK(0xe, 6, 0)),	/* eint */
	SUNXI_PIN(SUNXI_PINCTRL_PIN(H, 9),
		SUNXI_FUNCTION(0x0, "gpio_in"),		/* gpio_in */
		SUNXI_FUNCTION(0x1, "gpio_out"),	/* gpio_out */
		SUNXI_FUNCTION(0x2, "pwm9"),		/* pwm9 */
		SUNXI_FUNCTION(0x3, "rmii"),		/* rmii_txd1 */
		SUNXI_FUNCTION(0x4, "twi3"),		/* twi3_sck */
		SUNXI_FUNCTION(0x5, "uart0"),		/* uart0_tx */
		SUNXI_FUNCTION(0x6, "i2s1"),		/* i2s1_din0 */
		SUNXI_FUNCTION(0x7, "dmic"),		/* dmic_data0 */
		SUNXI_FUNCTION_IRQ_BANK(0xe, 6, 9)),	/* eint */
	SUNXI_PIN(SUNXI_PINCTRL_PIN(H, 10),
		SUNXI_FUNCTION(0x0, "gpio_in"),		/* gpio_in */
		SUNXI_FUNCTION(0x1, "gpio_out"),	/* gpio_out */
		SUNXI_FUNCTION(0x2, "pwm10"),		/* pwm10 */
		SUNXI_FUNCTION(0x3, "rmii"),		/* rmii_txd0 */
		SUNXI_FUNCTION(0x4, "twi3"),		/* twi3_sda */
		SUNXI_FUNCTION(0x5, "uart0"),		/* uart0_rx */
		SUNXI_FUNCTION(0x6, "i2s1"),		/* i2s1_dout0 */
		SUNXI_FUNCTION(0x7, "dmic"),		/* dmic_clk */
		SUNXI_FUNCTION_IRQ_BANK(0xe, 6, 10)),	/* eint */
	SUNXI_PIN(SUNXI_PINCTRL_PIN(H, 11),
		SUNXI_FUNCTION(0x0, "gpio_in"),		/* gpio_in */
		SUNXI_FUNCTION(0x1, "gpio_out"),	/* gpio_out */
		SUNXI_FUNCTION(0x2, "jtag"),		/* jtag_ms */
		SUNXI_FUNCTION(0x3, "rmii"),		/* rmii_txck */
		SUNXI_FUNCTION(0x4, "r_jtag"),		/* r_jtag_ms */
		SUNXI_FUNCTION(0x5, "twi2"),		/* twi2_sck */
		SUNXI_FUNCTION(0x6, "spi3"),		/* spi3_clk */
		SUNXI_FUNCTION(0x7, "clk"),		/* clk_fanout0 */
		SUNXI_FUNCTION(0x8, "pwm4"),		/* pwm4 */
		SUNXI_FUNCTION_IRQ_BANK(0xe, 6, 11)),	/* eint */
	SUNXI_PIN(SUNXI_PINCTRL_PIN(H, 12),
		SUNXI_FUNCTION(0x0, "gpio_in"),		/* gpio_in */
		SUNXI_FUNCTION(0x1, "gpio_out"),	/* gpio_out */
		SUNXI_FUNCTION(0x2, "jtag"),		/* jtag_ck */
		SUNXI_FUNCTION(0x3, "rmii"),		/* rmii_txen */
		SUNXI_FUNCTION(0x4, "r_jtag"),		/* r_jtag_ck */
		SUNXI_FUNCTION(0x5, "twi2"),		/* twi2_sda */
		SUNXI_FUNCTION(0x6, "spi3"),		/* spi3_mosi */
		SUNXI_FUNCTION(0x7, "clk"),		/* clk_fanout1 */
		SUNXI_FUNCTION(0x8, "pwm5"),		/* pwm5 */
		SUNXI_FUNCTION_IRQ_BANK(0xe, 6, 12)),	/* eint */
	SUNXI_PIN(SUNXI_PINCTRL_PIN(H, 13),
		SUNXI_FUNCTION(0x0, "gpio_in"),		/* gpio_in */
		SUNXI_FUNCTION(0x1, "gpio_out"),	/* gpio_out */
		SUNXI_FUNCTION(0x2, "jtag"),		/* jtag_do */
		SUNXI_FUNCTION(0x3, "mdc"),		/* mdc */
		SUNXI_FUNCTION(0x4, "r_jtag"),		/* r_jtag_do */
		SUNXI_FUNCTION(0x5, "twi3"),		/* twi3_sck */
		SUNXI_FUNCTION(0x6, "spi3"),		/* spi3_miso */
		SUNXI_FUNCTION(0x7, "wiegand"),		/* wiegand_d0 */
		SUNXI_FUNCTION(0x8, "pwm6"),		/* pwm6 */
		SUNXI_FUNCTION_IRQ_BANK(0xe, 6, 13)),	/* eint */
	SUNXI_PIN(SUNXI_PINCTRL_PIN(H, 14),
		SUNXI_FUNCTION(0x0, "gpio_in"),		/* gpio_in */
		SUNXI_FUNCTION(0x1, "gpio_out"),	/* gpio_out */
		SUNXI_FUNCTION(0x2, "jtag"),		/* jtag_di */
		SUNXI_FUNCTION(0x3, "mdio"),		/* mdio */
		SUNXI_FUNCTION(0x4, "r_jtag"),		/* r_jtag_di */
		SUNXI_FUNCTION(0x5, "twi3"),		/* twi3_sda */
		SUNXI_FUNCTION(0x6, "spi3"),		/* spi3_cs0 */
		SUNXI_FUNCTION(0x7, "wiegand"),		/* wiegand_d1 */
		SUNXI_FUNCTION(0x8, "pwm7"),		/* pwm7 */
		SUNXI_FUNCTION_IRQ_BANK(0xe, 6, 14)),	/* eint */
};

static const unsigned int v851se_irq_bank_map[] = { 0, 2, 3, 4, 5, 7 };

static const struct sunxi_pinctrl_desc v851se_pinctrl_data = {
	.pins			= v851se_pins,
	.npins			= ARRAY_SIZE(v851se_pins),
	.irq_banks		= ARRAY_SIZE(v851se_irq_bank_map),
	.irq_bank_map		= v851se_irq_bank_map,
	.io_bias_cfg_variant	= BIAS_VOLTAGE_PIO_POW_MODE_CTL,
};

static int v851se_pinctrl_probe(struct platform_device *pdev)
{
	unsigned long variant = (unsigned long)of_device_get_match_data(&pdev->dev);

	return sunxi_pinctrl_init_with_variant(pdev, &v851se_pinctrl_data, variant);
}

static const struct of_device_id v851se_pinctrl_match[] = {
	{
		.compatible = "allwinner,sun8i-v851se-pinctrl",
		.data = (void *)PINCTRL_SUN20I_D1
	},
	{}
};

static struct platform_driver v851se_pinctrl_driver = {
	.probe	= v851se_pinctrl_probe,
	.driver	= {
		.name		= "sun8i-v851se-pinctrl",
		.of_match_table	= v851se_pinctrl_match,
	},
};

builtin_platform_driver(v851se_pinctrl_driver);
