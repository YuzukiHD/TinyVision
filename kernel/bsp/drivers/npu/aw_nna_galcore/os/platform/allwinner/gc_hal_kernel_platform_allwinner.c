// SPDX-License-Identifier: GPL-2.0-only
/* Copyright(c) 2020 - 2023 Allwinner Technology Co.,Ltd. All rights reserved. */
/****************************************************************************
*
*    Copyright (c) 2005 - 2023 by Vivante Corp.  All rights reserved.
*
*    The material in this file is confidential and contains trade secrets
*    of Vivante Corporation. This is proprietary information owned by
*    Vivante Corporation. No part of this work may be disclosed,
*    reproduced, copied, transmitted, or used in any way for any purpose,
*    without the express written permission of Vivante Corporation.
*
*    This program is free software; you can redistribute it and/or modify
*    it under the terms of the GNU General Public License version 2 as
*    published by the Free Software Foundation.
*

*****************************************************************************/

#include "gc_hal_kernel_linux.h"
#include "gc_hal_kernel_platform.h"
#include <linux/clk-provider.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/regulator/consumer.h>
#include <linux/pm_runtime.h>
#include <linux/pm_opp.h>
#include <linux/reset.h>
#include "gc_hal_kernel_platform_allwinner.h"
#include <sunxi-sid.h>

aw_driver_t aw_driver = {
	.rst = NULL,
	.mclk = NULL,
	.pclk = NULL,
	.aclk = NULL,
	.hclk = NULL,
	.regulator = NULL,
	.vol = 0,
};

/* Get SID DVFS */
static int match_vf_table(u32 combi, u32 *index)
{
	struct device_node *np = NULL;
	int nsels, ret, i;
	u32 tmp;

	np = of_find_node_by_name(NULL, "npu_vf_mapping_table");
	if (!np) {
		pr_err("Unable to find node\n");
		return -EINVAL;
	}

	if (!of_get_property(np, "table", &nsels))
		return -EINVAL;

	nsels /= sizeof(u32);
	if (!nsels) {
		pr_err("invalid table property size\n");
		return -EINVAL;
	}

	for (i = 0; i < nsels / 2; i++) {
		ret = of_property_read_u32_index(np, "table", i * 2, &tmp);
		if (ret) {
			pr_err("could not retrieve table property: %d\n", ret);
			return ret;
		}

		if (tmp == combi) {
			ret = of_property_read_u32_index(np, "table", i * 2 + 1, &tmp);
			if (ret) {
				pr_err("could not retrieve table property: %d\n", ret);
				return ret;
			}
			*index = tmp;
			break;
		}
	}
	if (i == nsels / 2)
		pr_notice("%s %d, could not match vf table, i:%d", __func__, __LINE__, i);

	return 0;
}

#define SUN55IW3_MARKETID_EFUSE_OFF (0x00)
#define SUN55IW3_DVFS_EFUSE_OFF     (0x48)
static int sun55iw3_nvmem_xlate(void)
{
	u32 marketid, bak_dvfs, dvfs, combi;
	u32 index = 1;

	sunxi_get_module_param_from_sid(&marketid, SUN55IW3_MARKETID_EFUSE_OFF, 4);
	marketid &= 0xffff;
	sunxi_get_module_param_from_sid(&dvfs, SUN55IW3_DVFS_EFUSE_OFF, 4);
	bak_dvfs = (dvfs >> 12) & 0xff;
	if (bak_dvfs)
		combi = bak_dvfs;
	else
		combi = (dvfs >> 4) & 0xff;

	if (marketid == 0x5200 && combi == 0x00)
		index = 0;
	else
		match_vf_table(combi, &index);

	/* printk("NPU Use VF%u, dvfs: 0x%x\n", index, dvfs); */

	return index;
}

/* Get NPU CLK and Vol */
static int get_npu_clk_vol(unsigned int vf_index, int npu_vf, int *npu_vol, int *npu_clk)
{
	struct device_node *npu_table_node;
	struct device_node *opp_node;
	int opp_vol = 0, opp_hz = 0;
	int err;
	char temp[40];
	char microvolt[40] = "opp-microvolt-vf";

	if (vf_index == 0) {
		vf_index = 1;
		pr_notice("DVFS Get Fail, use default VF1!");
	}
	pr_notice("NPU Use VF%u\n", vf_index);

	npu_table_node = of_find_node_by_name(NULL, "npu-opp-table");

	if (npu_vf == 0) {
		sprintf(temp, "%s%d", microvolt, vf_index);
		strcpy(microvolt, temp);
		opp_node = of_find_node_by_name(npu_table_node, "opp-546");

		err = of_property_read_u32_index(opp_node, "opp-hz", 0, &opp_hz);
		if (err != 0)
			pr_notice("Get NPU CLK FAIL!\n");
		err = of_property_read_u32_index(opp_node, microvolt, 0, &opp_vol);
		if (err != 0)
			pr_notice("Get NPU VOL FAIL!\n");
		*npu_clk = opp_hz;
		*npu_vol = opp_vol;
	} else if (npu_vf == 1) {
		sprintf(temp, "%s%d", microvolt, vf_index);
		strcpy(microvolt, temp);
		opp_node = of_find_node_by_name(npu_table_node, "opp-696");

		err = of_property_read_u32_index(opp_node, "opp-hz", 0, &opp_hz);
		if (err != 0)
			pr_notice("Get NPU CLK FAIL!\n");
		err = of_property_read_u32_index(opp_node, microvolt, 0, &opp_vol);
		if (err != 0)
			pr_notice("Get NPU VOL FAIL!\n");
		*npu_clk = opp_hz;
		*npu_vol = opp_vol;
	}

	return 0;
}

/*
 * @brief regulator the VIP vol.
 */
gceSTATUS npu_regulator_enable(void)
{
	if (!aw_driver.regulator)
		return 0;

	/* set output voltage to the dts config */
	if (aw_driver.vol)
		regulator_set_voltage(aw_driver.regulator, aw_driver.vol, aw_driver.vol);

	if (regulator_enable(aw_driver.regulator)) {
		printk("enable regulator failed!\n");
		return -1;
	}

	return 0;
}

gceSTATUS npu_regulator_disable(void)
{
	if (!aw_driver.regulator)
		return 0;

	if (regulator_is_enabled(aw_driver.regulator))
		regulator_disable(aw_driver.regulator);

	return 0;
}

gceSTATUS _AdjustParam(IN gcsPLATFORM *Platform, OUT gcsMODULE_PARAMETERS *Args)
{
	struct platform_device *pdev = Platform->device;
	int irqLine = platform_get_irq(pdev, 0);
	int ret, err, vol, npu_vf = 0, npu_vol, npu_clk;
	unsigned long rate, real_rate;
	unsigned int mod_clk = 0, vf_index;
	struct device *dev = &(pdev->dev);
	struct resource *res;

	printk("galcore: irq line = %d\n", irqLine);
	printk("galcore: ####################enter _AdjustParam "
		   "######################\n");
	printk("galcore: galcore irq number is %d.\n", irqLine);
	if (irqLine < 0) {
		printk("galcore: get galcore irq resource error\n");
		irqLine = platform_get_irq_byname(pdev, "galcore");
		printk("galcore: galcore irq number is %d\n", irqLine);
	}
	if (irqLine < 0)
		return gcvSTATUS_OUT_OF_RESOURCES;

	Args->irqs[gcvCORE_MAJOR] = irqLine;
	/* Args->irqs[gcvCORE_MAJOR] = 65; */
	printk("galcore: xp galcore irq number is %d.\n", Args->irqs[gcvCORE_MAJOR]);
	/*
	 * Args->contiguousBase = 0x41000000;
	 * Args->contiguousSize = 0x1000000;
	 */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		printk("galcore: no resource for registers\n");
		ret = -ENOENT;
	}

	aw_driver.regulator = regulator_get(dev, "npu");
	if (IS_ERR(aw_driver.regulator))
		pr_notice("Don`t Set NPU regulator!\n");

	/* Get NPU VF */
	ret = of_property_read_u32(pdev->dev.of_node, "npu-vf", &npu_vf);
	if (!ret) {
		vf_index = sun55iw3_nvmem_xlate();
		err = get_npu_clk_vol(vf_index, npu_vf, &npu_vol, &npu_clk);
		if (!err) {
			aw_driver.vol = npu_vol;
			mod_clk = npu_clk;
		} else {
			pr_notice("Get NPU CLK and Vol Failed!\n");
		}
	}

	/* Set NPU Vol */
	if (aw_driver.vol) {
		err = npu_regulator_enable();
		if (err) {
			pr_notice("enable regulator failed!\n");
			return -1;
		}
		vol = regulator_get_voltage(aw_driver.regulator);
		pr_notice("galcore: Want set npu vol(%d) now vol(%d)", aw_driver.vol, vol);
	}

	/* Args->extSRAMBases[0] = 0xFF000000; */
	Args->extSRAMSizes[0] =  0x80000;

	/*
	 * Args->registerBases[0] = 0xFF100000;
	 * Args->registerSizes[0] = 0x20000;
	 */
	Args->registerBases[0] = res->start;
	/* printk("Args->registerBases:%u\n",res->start); */
	Args->registerSizes[0] = resource_size(res);
	/* printk("Args->registerSize:%u\n",Args->registerSizes[0]); */

	printk("galcore: %s %d SUCCESS\n", __func__, __LINE__);
	aw_driver.mclk = of_clk_get(pdev->dev.of_node, 0);
	aw_driver.pclk = of_clk_get(pdev->dev.of_node, 1);
	aw_driver.aclk = of_clk_get(pdev->dev.of_node, 2);
	aw_driver.hclk = of_clk_get(pdev->dev.of_node, 3);

	/* Set NPU CLK */
	if (mod_clk) {
		if (!IS_ERR_OR_NULL(aw_driver.pclk)) {
			rate = clk_round_rate(aw_driver.pclk, mod_clk);
			if (clk_set_rate(aw_driver.pclk, rate)) {
				pr_err("clk_set_rate:%ld  mod_clk:%d failed\n", rate, mod_clk);
				return -1;
			}
			real_rate = clk_get_rate(aw_driver.pclk);
			printk("galcore: Want set pclk rate(%d) support(%ld) real(%ld)\n", mod_clk, rate, real_rate);
			ret  = clk_set_parent(aw_driver.mclk, aw_driver.pclk);
			if (ret != 0) {
				pr_err("clk_set_parent() failed! return\n");
				return -1;
			}
		}
		rate = clk_round_rate(aw_driver.mclk, mod_clk);
		if (clk_set_rate(aw_driver.mclk, rate)) {
			pr_err("clk_set_rate:%ld  mod_clk:%d failed\n", rate, mod_clk);
			return -1;
		}
		real_rate = clk_get_rate(aw_driver.mclk);
		printk("galcore: Want set mclk rate(%d) support(%ld) real(%ld)\n", mod_clk, rate, real_rate);
	} else {
		pr_notice("CLK Frequency Get Failed! Use parent clk!\n");
		if (!IS_ERR_OR_NULL(aw_driver.pclk)) {
			printk("galcore: %s rate:%d\n", __func__, mod_clk);
			ret  = clk_set_parent(aw_driver.mclk, aw_driver.pclk);
			if (ret != 0) {
				pr_err("clk_set_parent() failed! return\n");
				return -1;
			}
		}
	}
#if IS_ENABLED(CONFIG_AW_PM_DOMAINS)
		pm_runtime_enable(dev);
		aw_driver.rst = devm_reset_control_get(&pdev->dev, NULL);
		if (IS_ERR_OR_NULL(aw_driver.rst)) {
			pr_err("failed to get NPU reset handle\n");
			return -1;
		}
		reset_control_deassert(aw_driver.rst);
#else
		if (clk_prepare_enable(aw_driver.hclk)) {
			printk("galcore: Couldn't enable AHB clock\n");
			return -EBUSY;
		}
		if (clk_prepare_enable(aw_driver.aclk)) {
			printk("galcore: Couldn't enable AXI clock\n");
			return -EBUSY;
		}
		if (clk_prepare_enable(aw_driver.mclk)) {
			printk("galcore: Couldn't enable module clock\n");
			return -EBUSY;
		}
		aw_driver.rst = devm_reset_control_get(&pdev->dev, NULL);
		if (IS_ERR_OR_NULL(aw_driver.rst)) {
			pr_err("failed to get NPU reset handle\n");
			return -1;
		}
		reset_control_deassert(aw_driver.rst);
#endif
	return gcvSTATUS_OK;
}

#if IS_ENABLED(CONFIG_AW_PM_DOMAINS)
/*
 * @brief turn on the VIP clock.
 */
gceSTATUS npu_clk_init(void)
{
	if (clk_prepare_enable(aw_driver.hclk)) {
		printk("Couldn't enable AHB clock\n");
		return -EBUSY;
	}
	if (clk_prepare_enable(aw_driver.aclk)) {
		printk("Couldn't enable AXI clock\n");
		return -EBUSY;
	}
	if (clk_prepare_enable(aw_driver.mclk)) {
		printk("Couldn't enable module clock\n");
		return -EBUSY;
	}
	reset_control_deassert(aw_driver.rst);

	return gcvSTATUS_OK;
}

/*
 * @brief turn off the VIP clock.
 */
gceSTATUS npu_clk_uninit(void)
{
	clk_disable_unprepare(aw_driver.hclk);
	clk_disable_unprepare(aw_driver.aclk);
	clk_disable_unprepare(aw_driver.mclk);
	reset_control_assert(aw_driver.rst);

	return gcvSTATUS_OK;
}
#endif

gceSTATUS _SetPower(IN gcsPLATFORM *Platform, IN gctUINT32 DevIndex, IN gceCORE GPU, IN gctBOOL Enable)
{
	struct platform_device *pdev = Platform->device;
	struct device *dev = &(pdev->dev);
	switch (Enable) {
	case gcvTRUE:
		printk("galcore: %s %d ON\n", __func__, GPU);
#if IS_ENABLED(CONFIG_AW_PM_DOMAINS)
		pm_runtime_get_sync(dev);
		npu_clk_init();
#endif
		break;
	case gcvFALSE:
		printk("galcore: %s %d OFF\n", __func__, GPU);
#if IS_ENABLED(CONFIG_AW_PM_DOMAINS)
		pm_runtime_put(dev);
		npu_clk_uninit();
#endif
		break;
	default:
		printk("galcore: Unsupport clk status");
		break;
	}

	return gcvSTATUS_OK;
}


gceSTATUS _SetClock(IN gcsPLATFORM *Platform, IN gctUINT32 DevIndex, IN gceCORE GPU, IN gctBOOL Enable)
{
	/*
	 * void *ccmu_vaddr = ioremap(0x02001000, 0xff0);
	 * printk("#enter _SetClock#\n");
	 * writel(0x80000000, ccmu_vaddr + 0x6e0);
	 * writel(0x10001, ccmu_vaddr + 0x6ec);
	 * iounmap(ccmu_vaddr);
	 */
	return gcvSTATUS_OK;
}

gceSTATUS _GetPower(IN gcsPLATFORM *Platform)
{
	printk("galcore: enter _GetPower \n");
	return gcvSTATUS_OK;
}

gceSTATUS _PutPower(IN gcsPLATFORM *Platform)
{
	printk("galcore: enter _PutPower \n");
	return gcvSTATUS_OK;
}

static struct _gcsPLATFORM_OPERATIONS default_ops = {
	.adjustParam = _AdjustParam,
	.getPower = _GetPower,
	.putPower = _PutPower,
	.setPower = _SetPower,
	.setClock = _SetClock,
};

static struct _gcsPLATFORM default_platform = {
	.name = __FILE__, .ops = &default_ops,
};

static struct platform_device *default_dev;

static const struct of_device_id galcore_dev_match[] = {
	{.compatible = "allwinner,npu"}, {},
};

int gckPLATFORM_Init(struct platform_driver *pdrv,
			 struct _gcsPLATFORM **platform)
{
	printk("galcore: enter gckPLATFORM_Init from allwinenertech\n");
	pdrv->driver.of_match_table = galcore_dev_match;

	*platform = &default_platform;

	return 0;
}

int gckPLATFORM_Terminate(struct _gcsPLATFORM *platform)
{
	struct platform_device *pdev = platform->device;
	struct device *dev = &(pdev->dev);
	if (dev != NULL) {
		if (aw_driver.vol) {
			npu_regulator_disable();
			if (aw_driver.regulator) {
				regulator_put(aw_driver.regulator);
				aw_driver.regulator = NULL;
			}
		}
		printk("%s %d SUCCESS\n", __func__, __LINE__);
		#if IS_ENABLED(CONFIG_AW_PM_DOMAINS)
		pm_runtime_disable(dev);
		#endif
	} else {
		printk("%s %d ERR\n", __func__, __LINE__);
	}

	if (default_dev) {
		platform_device_unregister(default_dev);
		default_dev = NULL;
	}

	return 0;
}

MODULE_VERSION("6.4.15");
MODULE_AUTHOR("Miujiu <zhangjunjie@allwinnertech.com>");
