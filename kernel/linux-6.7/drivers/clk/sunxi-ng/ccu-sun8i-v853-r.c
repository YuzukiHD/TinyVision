// SPDX-License-Identifier: GPL-3.0
/* Copyright(c) 2020 - 2023 Allwinner Technology Co.,Ltd. All rights reserved. */
/*
 * Copyright (c) 2023 rengaomin@allwinnertech.com
 */

#include <linux/clk-provider.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>

#include "ccu_common.h"
#include "ccu_reset.h"

#include "ccu_div.h"
#include "ccu_gate.h"
#include "ccu_mp.h"
#include "ccu_mult.h"
#include "ccu_nk.h"
#include "ccu_nkm.h"
#include "ccu_nkmp.h"
#include "ccu_nm.h"

#include "ccu-sun8i-v853-r.h"


static SUNXI_CCU_GATE(r_twd_clk, "r-twd", "osc24M", 0x012C, BIT(0), 0);
static SUNXI_CCU_GATE(r_ppu_clk, "r-ppu", "osc24M", 0x01AC, BIT(0), 0);
static SUNXI_CCU_GATE(r_rtc_clk, "r-rtc", "osc24M", 0x020C, BIT(0), 0);
static SUNXI_CCU_GATE(r_cpucfg_clk, "r-cpucfg", "osc24M",	
			0x022C, BIT(0), 0);

static struct ccu_reset_map sun8i_v853_r_ccu_resets[] = {
	[RST_BUS_R_PPU]		= { 0x01ac, BIT(16) },
	[RST_BUS_R_RTC]		= { 0x020c, BIT(16) },
	[RST_BUS_R_CPUCFG]	= { 0x022c, BIT(16) },
};

static struct clk_hw_onecell_data sun8i_v853_r_hw_clks = {
	.hws    = {
		[CLK_R_TWD]			= &r_twd_clk.common.hw,
		[CLK_R_PPU]			= &r_ppu_clk.common.hw,
		[CLK_R_RTC]			= &r_rtc_clk.common.hw,
		[CLK_R_CPUCFG]			= &r_cpucfg_clk.common.hw,
	},
	.num = CLK_NUMBER,
};

static struct ccu_common *sun8i_v853_r_ccu_clks[] = {
	&r_twd_clk.common,
	&r_ppu_clk.common,
	&r_rtc_clk.common,
	&r_cpucfg_clk.common,
};


static const struct sunxi_ccu_desc sun8i_v853_r_ccu_desc = {
	.ccu_clks	= sun8i_v853_r_ccu_clks,
	.num_ccu_clks	= ARRAY_SIZE(sun8i_v853_r_ccu_clks),

	.hw_clks	= &sun8i_v853_r_hw_clks,

	.resets		= sun8i_v853_r_ccu_resets,
	.num_resets	= ARRAY_SIZE(sun8i_v853_r_ccu_resets),
};

static int sun8i_v853_r_ccu_probe(struct platform_device *pdev)
{
	void __iomem *reg;

	reg = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(reg))
		return PTR_ERR(reg);

	return devm_sunxi_ccu_probe(&pdev->dev, reg, &sun8i_v853_r_ccu_desc);
}

static const struct of_device_id sun8i_v853_r_ccu_ids[] = {
	{ .compatible = "allwinner,sun8i-v853-r-ccu" },
	{ }
};

static struct platform_driver sun8i_v853_r_ccu_driver = {
	.probe	= sun8i_v853_r_ccu_probe,
	.driver	= {
		.name			= "sun8i-v853-r-ccu",
		.suppress_bind_attrs	= true,
		.of_match_table		= sun8i_v853_r_ccu_ids,
	},
};
module_platform_driver(sun8i_v853_r_ccu_driver);

MODULE_IMPORT_NS(SUNXI_CCU);
MODULE_LICENSE("GPL");
