/* SPDX-License-Identifier: GPL-2.0 */
#include <linux/clk.h>
#include <linux/clk/sunxi.h>
#include <linux/clk-provider.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/of_reserved_mem.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/reset.h>
#include <asm/io.h>

#include "../sunxi_rproc_boot_internal.h"

/* hardware related operations for sun8iw21-e907 core */
struct sunxi_e907 {
	struct device *dev;
	struct platform_device *pdev;
	struct clk *pclk;		/* e907 pll clk */
	struct clk *clk;		/* e907 clk */
	struct clk *gate;		/* e907 clk gating */
	struct clk *axi;		/* e907 axi clk */
	u32 freq;
	struct clk *mbox;		/* msgbox clk */
	void __iomem *addr;		/* e907 start addr register */
	u32 boot_addr;
};

static int sunxi_e907_init(struct platform_device *pdev,
				struct sunxi_core *core)
{
	struct sunxi_e907 *info;
	struct resource *res;
	struct device *dev = &pdev->dev;
	int ret;

	info = devm_kzalloc(dev, sizeof(*info), GFP_KERNEL);
	if (IS_ERR_OR_NULL(info))
		return -ENOMEM;

	core->priv = info;
	info->dev = dev;
	info->pdev = pdev;

	info->pclk = devm_clk_get(info->dev, "pll");
	if (IS_ERR(info->pclk)) {
		dev_err(info->dev, "no find pll in dts, ret=%ld\n", PTR_ERR(info->pclk));
		ret = PTR_ERR(info->pclk);
		goto err_pll;
	}

	ret = clk_prepare(info->pclk);
	if (ret) {
		dev_err(info->dev, "pll clk prepare failed\n");
		goto err_pll;
	}

	info->clk = devm_clk_get(info->dev, "clk");
	if (IS_ERR(info->clk)) {
		dev_err(info->dev, "no find clk in dts, ret=%ld\n", PTR_ERR(info->clk));
		ret = PTR_ERR(info->clk);
		goto err_clk;
	}

	ret = clk_prepare(info->clk);
	if (ret) {
		dev_err(info->dev, "clk prepare failed\n");
		goto err_clk;
	}

	info->axi = devm_clk_get(info->dev, "axi");
	if (IS_ERR(info->axi)) {
		dev_err(info->dev, "no find axi clk in dts, ret=%ld\n", PTR_ERR(info->axi));
		ret = PTR_ERR(info->axi);
		goto err_axi;
	}

	ret = clk_prepare(info->axi);
	if (ret) {
		dev_err(info->dev, "axi clk prepare failed\n");
		goto err_axi;
	}

	info->gate = devm_clk_get(info->dev, "gate");
	if (IS_ERR(info->gate)) {
		dev_err(info->dev, "no find gate in dts, ret=%ld\n", PTR_ERR(info->gate));
		ret = PTR_ERR(info->gate);
		goto err_gate;
	}

	ret = clk_prepare(info->gate);
	if (ret) {
		dev_err(info->dev, "gate prepare failed\n");
		goto err_gate;
	}

	info->mbox = devm_clk_get(info->dev, "mbox");
	if (IS_ERR(info->mbox)) {
		dev_err(info->dev, "no find mbox clk in dts, ret=%ld\n", PTR_ERR(info->gate));
		ret = PTR_ERR(info->mbox);
		goto err_mbox;
	}

	ret = of_property_read_u32(info->dev->of_node, "clock-frequency", &info->freq);
	if (ret) {
		dev_err(info->dev, "fail to get clock-frequency");
		ret = -ENXIO;
		goto err_freq;
	}

	res = platform_get_resource_byname(info->pdev,
					IORESOURCE_MEM, "riscv-addr");
	if (!res) {
		dev_err(info->dev, "no find riscv-addr reg in dts\n");
		ret = -ENODEV;
		goto err_addr;
	}

	info->addr = devm_ioremap_resource(info->dev, res);
	if (IS_ERR(info->addr)) {
		dev_err(info->dev, "ioreamp riscv-addr register "
						"failed,ret=%ld\n", PTR_ERR(info->addr));
		ret = PTR_ERR(info->addr);
		goto err_ioremap;
	}

	return 0;

err_ioremap:
err_addr:
err_freq:
	devm_clk_put(info->dev, info->mbox);
err_mbox:
	devm_clk_put(info->dev, info->gate);
err_gate:
	devm_clk_put(info->dev, info->axi);
err_axi:
	devm_clk_put(info->dev, info->clk);
err_clk:
	devm_clk_put(info->dev, info->pclk);
err_pll:
	return ret;
}

static void sunxi_e907_deinit(struct sunxi_core *core)
{
	struct sunxi_e907 *info = core->priv;

	clk_unprepare(info->gate);
	clk_unprepare(info->axi);
	clk_unprepare(info->clk);
	clk_unprepare(info->pclk);
	clk_unprepare(info->axi);

	devm_clk_put(info->dev, info->mbox);
	devm_clk_put(info->dev, info->gate);
	devm_clk_put(info->dev, info->axi);
	devm_clk_put(info->dev, info->clk);
	devm_clk_put(info->dev, info->pclk);
	devm_iounmap(info->dev, info->addr);
}

static int sunxi_e907_start(struct sunxi_core *core)
{
	struct sunxi_e907 *info = core->priv;
	int rate = 0;
	int ret;

	if (__clk_get_enable_count(info->mbox))
		clk_disable_unprepare(info->mbox);

	ret = clk_enable(info->pclk);
	if (ret < 0) {
		dev_err(info->dev, "pll clk enable failed.\n");
		return ret;
	}

	ret = clk_set_parent(info->clk, info->pclk);
	if (ret < 0) {
		dev_err(info->dev, "set clk parent to pll clk failed.\n");
		return ret;
	}

	rate = clk_round_rate(info->clk, info->freq);
	ret = clk_set_rate(info->clk, rate);
	if (ret < 0) {
		dev_err(info->dev, "set e907 clk rate to %d failed.\n", rate);
		return ret;
	}

	ret = clk_enable(info->clk);
	if (ret < 0) {
		dev_err(info->dev, "e907 clk enable failed.\n");
		return ret;
	}

	ret = clk_set_parent(info->axi, info->clk);
	if (ret < 0) {
		dev_err(info->dev, "set axi clk parent to clk failed.\n");
		return ret;
	}

	rate = clk_round_rate(info->axi, info->freq / 2);
	ret = clk_set_rate(info->axi, rate);
	if (ret < 0) {
		dev_err(info->dev, "set axi clk rate to %d failed.\n", rate);
		return ret;
	}

	ret = clk_enable(info->axi);
	if (ret < 0) {
		dev_err(info->dev, "axi clk enable failed.\n");
		return ret;
	}

	/* Set Cpu boot addr */
	writel(info->boot_addr, info->addr);

	/*
	 * In order to clear the msgbox data in the fifo,
	 * we need to reset the riscv msgbox
	 */
	clk_prepare_enable(info->mbox);

	clk_enable(info->gate);

	return 0;
}

static int sunxi_e907_is_start(struct sunxi_core *core)
{
	struct sunxi_e907 *info = core->priv;

	return __clk_is_enabled(info->gate);
}

static int sunxi_e907_stop(struct sunxi_core *core)
{
	struct sunxi_e907 *info = core->priv;

	if (__clk_get_enable_count(info->gate))
		clk_disable(info->gate);
	if (__clk_get_enable_count(info->clk))
		clk_disable(info->clk);

	return 0;
}

static void sunxi_e907_set_start_addr(struct sunxi_core *core, u32 addr)
{
	struct sunxi_e907 *info = core->priv;

	info->boot_addr = addr;
}

static struct sunxi_remote_core_ops core_ops_e907 = {
	.init = sunxi_e907_init,
	.deinit = sunxi_e907_deinit,
	.start = sunxi_e907_start,
	.is_start = sunxi_e907_is_start,
	.stop = sunxi_e907_stop,
	.set_start_addr = sunxi_e907_set_start_addr,
};

DEFINE_RPROC_CORE(e907, core_ops_e907, "allwinner,riscv-e907");
