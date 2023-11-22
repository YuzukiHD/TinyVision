// SPDX-License-Identifier: GPL-2.0-only
/* Copyright(c) 2020 - 2023 Allwinner Technology Co.,Ltd. All rights reserved. */
/*
 * (C) Copyright 2017-2023
 * Reuuimlla Technology Co., Ltd. <www.allwinnertech.com>
 * Miujiu <zhangjunjie@allwinnertech.com>
 ****************************************************************************
 *
 *	The MIT License (MIT)
 *
 *	Copyright (c) 2017 - 2023 Vivante Corporation
 *
 *	Permission is hereby granted, free of charge, to any person obtaining a
 *	copy of this software and associated documentation files (the "Software"),
 *	to deal in the Software without restriction, including without limitation
 *	the rights to use, copy, modify, merge, publish, distribute, sublicense,
 *	and/or sell copies of the Software, and to permit persons to whom the
 *	Software is furnished to do so, subject to the following conditions:
 *
 *	The above copyright notice and this permission notice shall be included in
 *	all copies or substantial portions of the Software.
 *
 *	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *	FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 *	DEALINGS IN THE SOFTWARE.
 *
 *****************************************************************************
 */

#include "../../gc_vip_kernel_port.h"
#include "../../gc_vip_kernel.h"
#include "../gc_vip_kernel_drv.h"
#include "../../gc_vip_kernel_heap.h"
#include "gc_vip_platform_config.h"
#include "gc_vip_kernel_drv_platform.h"
#include <sunxi-sid.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 16, 0))
#include <linux/stdarg.h>
#else
#include <stdarg.h>
#endif
#include <linux/time.h>
#include <linux/mm.h>
#include <linux/clk.h>
#include <linux/devfreq.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/pm_runtime.h>
#include <linux/pm_opp.h>
#include <linux/reset.h>

typedef enum _clk_status {
	CLK_ON  = 0x0,
	CLK_OFF = 0x1,
} clk_status_e;

aw_driver_t aw_driver = {
	.rst = NULL,
	.mclk = NULL,
	.pclk = NULL,
	.aclk = NULL,
	.hclk = NULL,
	.regulator = NULL,
	.vol = 0,
};

/*
 * @brief regulator the VIP vol.
 */
static vip_status_e npu_regulator_enable(void)
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

static vip_status_e npu_regulator_disable(void)
{
	if (!aw_driver.regulator)
		return 0;

	if (regulator_is_enabled(aw_driver.regulator))
		regulator_disable(aw_driver.regulator);

	return 0;
}

#if IS_ENABLED(CONFIG_AW_PM_DOMAINS)
/*
* @brief turn on the VIP clock.
* */
static vip_status_e npu_clk_init(void)
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

	return VIP_SUCCESS;
}

/*
* @brief turn off the VIP clock.
* */
static vip_status_e npu_clk_uninit(void)
{
	clk_disable_unprepare(aw_driver.hclk);
	clk_disable_unprepare(aw_driver.aclk);
	clk_disable_unprepare(aw_driver.mclk);
	reset_control_assert(aw_driver.rst);

	return VIP_SUCCESS;
}
#endif

/*
* @brief configure the power supply and clock of the VIP.
* @param kdriver, vip device object.
* */
static vip_status_e set_vip_power_clk(gckvip_driver_t *kdriver, uint32_t status)
{
	__attribute__((unused)) struct device *dev = &(kdriver->pdev->dev);
	switch (status) {
	case CLK_ON:
#if IS_ENABLED(CONFIG_AW_PM_DOMAINS)
		 pm_runtime_get_sync(dev);
		 npu_clk_init();
#endif
		break;
	case CLK_OFF:
#if IS_ENABLED(CONFIG_AW_PM_DOMAINS)
		pm_runtime_put(dev);
		npu_clk_uninit();
#endif
		break;
	default:
		printk("Unsupport clk status");
		break;
	}
	return VIP_SUCCESS;
}

/*
* @brief convert CPU physical to VIP physical.
* @param cpu_physical. the physical address of CPU domain.
* */

vip_address_t gckvip_drv_get_vipphysical(
	vip_address_t cpu_physical
	)
{
	vip_address_t vip_hysical = cpu_physical;
	return vip_hysical;
}

/*
 * @brief convert VIP physical to CPU physical.
 * @param vip_physical. the physical address of VIP domain.
 * */

vip_address_t gckvip_drv_get_cpuphysical(
	vip_address_t vip_physical
	)
{
	vip_address_t cpu_hysical = vip_physical;
	return cpu_hysical;
}

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
		PRINTK("DVFS Get Fail, use default VF1!");
	}
	PRINTK("NPU Use VF%u\n", vf_index);

	npu_table_node = of_find_node_by_name(NULL, "npu-opp-table");

	if (npu_vf == 0) {
		sprintf(temp, "%s%d", microvolt, vf_index);
		strcpy(microvolt, temp);
		opp_node = of_find_node_by_name(npu_table_node, "opp-546");

		err = of_property_read_u32_index(opp_node, "opp-hz", 0, &opp_hz);
		if (err != 0)
			PRINTK("Get NPU CLK FAIL!\n");
		err = of_property_read_u32_index(opp_node, microvolt, 0, &opp_vol);
		if (err != 0)
			PRINTK("Get NPU VOL FAIL!\n");
		*npu_clk = opp_hz;
		*npu_vol = opp_vol;
	} else if (npu_vf == 1) {
		sprintf(temp, "%s%d", microvolt, vf_index);
		strcpy(microvolt, temp);
		opp_node = of_find_node_by_name(npu_table_node, "opp-696");

		err = of_property_read_u32_index(opp_node, "opp-hz", 0, &opp_hz);
		if (err != 0)
			PRINTK("Get NPU CLK FAIL!\n");
		err = of_property_read_u32_index(opp_node, microvolt, 0, &opp_vol);
		if (err != 0)
			PRINTK("Get NPU VOL FAIL!\n");
		*npu_clk = opp_hz;
		*npu_vol = opp_vol;
	}

	return 0;
}

static const struct of_device_id vipcore_dev_match[] = {
	{
		.compatible = "allwinner,npu"
	},
	{ },
};

/*
@brief platfrom(vendor) initialize. prepare environmnet for running VIP hardware.
@param pdrv vip drvier device object.
*/
vip_int32_t gckvip_drv_platform_init(
	gckvip_driver_t *kdriver
	)
{
	kdriver->pdrv->driver.of_match_table = vipcore_dev_match;

	return 0;
}

MODULE_DEVICE_TABLE(of, vipcore_dev_match);
/*
@brief 1. platfrom(vendor) un-initialize.
	   2. uninitialzie linux platform device.
	   have removed device/driver, so kdriver->pdev can't be used in this function.
@param pdrv vip drvier device object.
*/
vip_int32_t gckvip_drv_platform_uninit(
	gckvip_driver_t *kdriver
	)
{
	struct device *dev = &(kdriver->pdev->dev);
	if (dev != NULL) {
		printk("%s %d SUCCESS\n", __func__, __LINE__);
#if IS_ENABLED(CONFIG_AW_PM_DOMAINS)
		pm_runtime_disable(dev);
#endif
	} else {
		printk("%s %d ERR\n", __func__, __LINE__);
	}
	return 0;
}

/*
@brief adjust parameters of vip devices. such as irq, SRAM, video memory heap.
	 you can overwrite insmod command line in gckvip_drv_adjust_param() function.
@param kdriver, vip device object.
*/
vip_int32_t gckvip_drv_adjust_param(
	gckvip_driver_t *kdriver
	)
{
	struct platform_device *pdev = kdriver->pdev;
	struct device *dev = &(kdriver->pdev->dev);
	unsigned long rate, real_rate;
	unsigned int mod_clk = 0, vf_index;
	struct resource *res;
	int ret, err, vol, npu_vf = 0, npu_vol, npu_clk;

	if (VIP_NULL == pdev) {
		PRINTK("platform device is NULL \n");
		return -1;
	}

	if (0 == kdriver->cpu_physical) {
		kdriver->cpu_physical = VIDEO_MEMORY_HEAP_BASE_ADDRESS;
	}
	if (0 == kdriver->vip_memsize) {
		kdriver->vip_memsize = VIDEO_MEMORY_HEAP_SIZE;
	}
	kdriver->irq_line[0] = platform_get_irq(pdev, 0);
	PRINTK("vipcore irq number is %d.\n", kdriver->irq_line[0]);
	/*
	 * if (kdriver->irq_line[0] < 0) {
	 *	PRINTK("get vipcore irq resource error\n");
	 *	return -1;
	 * }
	 */

	/* IRQ line */
	if (0 == kdriver->irq_line[0]) {
		kdriver->irq_line[0] = IRQ_LINE_NUMBER[0];
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		PRINTK("no resource for registers\n");
		ret = -ENOENT;
		return -1;
	}

	aw_driver.regulator = regulator_get(dev, "npu");
	if (IS_ERR(aw_driver.regulator))
		PRINTK("Don`t Set NPU regulator!\n");

	/* Get NPU VF */
	ret = of_property_read_u32(pdev->dev.of_node, "npu-vf", &npu_vf);
	if (!ret) {
		vf_index = sun55iw3_nvmem_xlate();
		err = get_npu_clk_vol(vf_index, npu_vf, &npu_vol, &npu_clk);
		if (!err) {
			aw_driver.vol = npu_vol;
			mod_clk = npu_clk;
		} else {
			PRINTK("Get NPU CLK and Vol Failed!\n");
		}
	}

	/* Set NPU Vol */
	if (aw_driver.vol) {
		err = npu_regulator_enable();
		if (err) {
			PRINTK("enable regulator failed!\n");
			return -1;
		}
		vol = regulator_get_voltage(aw_driver.regulator);
		PRINTK("Want set npu vol(%d) now vol(%d)", aw_driver.vol, vol);
	}

	/* VIP AHB register memory base physical address */
	if (0 == kdriver->vip_reg_phy[0]) {
		kdriver->vip_reg_phy[0] = res->start;
	}

	/* VIP AHB register memory size */
	if (0 == kdriver->vip_reg_size[0]) {
		kdriver->vip_reg_size[0] = resource_size(res);
	}
	if (0 == kdriver->axi_sram_base) {
		kdriver->axi_sram_base = AXI_SRAM_BASE_ADDRESS;
	}
	if (0 == kdriver->axi_sram_size[0]) {
		kdriver->axi_sram_size[0] = AXI_SRAM_SIZE;
	}
	if (0 == kdriver->vip_sram_size[0]) {
		kdriver->vip_sram_size[0] = VIP_SRAM_SIZE;
	}
	if (0 == kdriver->vip_sram_base) {
		kdriver->vip_sram_base = VIP_SRAM_BASE_ADDRESS;
	}

	kdriver->device_core_number[0] = 1;
	kdriver->core_count = 1;

	kdriver->sys_heap_size = SYSTEM_MEMORY_HEAP_SIZE;
	if (dev != NULL) {
		printk("%s %d SUCCESS\n", __func__, __LINE__);
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
				printk("Want set pclk rate(%d) support(%ld) real(%ld)\n", mod_clk, rate, real_rate);
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
			printk("Want set mclk rate(%d) support(%ld) real(%ld)\n", mod_clk, rate, real_rate);
		} else {
			PRINTK("CLK Frequency Get Failed! Use parent clk!\n");
			if (!IS_ERR_OR_NULL(aw_driver.pclk)) {
				printk("%s rate:%d\n", __func__, mod_clk);
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
		aw_driver.rst = devm_reset_control_get(&pdev->dev, NULL);
		if (IS_ERR_OR_NULL(aw_driver.rst)) {
			pr_err("failed to get NPU reset handle\n");
			return -1;
		}
		reset_control_deassert(aw_driver.rst);
#endif
	} else {
		printk("%s %d ERR\n", __func__, __LINE__);
	}

	return 0;
}

/*
@brief release resources created in gckvip_drv_adjust_param if neeed.
this function called before remove device/driver, so kdriver->pdev can be used.
@param kdriver, vip device object.
*/
vip_int32_t gckvip_drv_unadjust_param(
	gckvip_driver_t *kdriver
	)
{
	if (aw_driver.vol) {
		npu_regulator_disable();
		if (aw_driver.regulator) {
			regulator_put(aw_driver.regulator);
			aw_driver.regulator = NULL;
		}
	}
	return 0;
}

/*
@brief customer SOC initialize. power on and rise up clock for VIP hardware
@param kdriver, vip device object.
*/
vip_int32_t gckvip_drv_device_init(
	gckvip_driver_t *kdriver,
	vip_uint32_t core
	)
{
	vip_int32_t ret = 0;
	struct platform_device *pdev = kdriver->pdev;
	/* PRINTK("vipcore, device init..\n"); */
	if (VIP_NULL == pdev) {
		PRINTK("platform device is NULL \n");
		return -1;
	}

	/* power on VIP and make sure power stable before this function return */
	ret = set_vip_power_clk(kdriver, CLK_ON);
	gckvip_os_delay(2);

	return ret;
}

/*
@brief customer SOC un-initialize. power off
@param kdriver, vip device object.
*/
vip_int32_t gckvip_drv_device_uninit(
	gckvip_driver_t *kdriver,
	vip_uint32_t core
	)
{
	struct platform_device *pdev = kdriver->pdev;
	vip_int32_t ret = -1;
	/* PRINTK("vipcore, device un-init..\n"); */
	if (VIP_NULL == pdev) {
		PRINTK("platform device is NULL \n");
		return -1;
	}
	/* power off VIP and make sure power stable before this function return */
	ret = 0;
	ret = set_vip_power_clk(kdriver, CLK_OFF);

	return ret;
}

MODULE_VERSION("1.13.0");
MODULE_AUTHOR("Miujiu <zhangjunjie@allwinnertech.com>");
