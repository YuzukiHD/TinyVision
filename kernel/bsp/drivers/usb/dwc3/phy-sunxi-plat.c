// SPDX-License-Identifier: GPL-2.0+
/* Copyright(c) 2020 - 2023 Allwinner Technology Co.,Ltd. All rights reserved. */
/*
 * Allwinner sun55i(A523) USB 3.0 phy driver
 *
 * Copyright (c) 2022 Allwinner Technology Co., Ltd.
 *
 * Based on phy-sun50i-usb3.c, which is:
 *
 * Allwinner sun50i(H6) USB 3.0 phy driver
 *
 * Copyright (C) 2017 Icenowy Zheng <icenowy@aosc.io>
 */

#include <linux/clk.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <linux/reset.h>

/* USB Application Registers */
/* USB2.0 Interface Status and Control Register */
#define PHY_USB2_ISCR				0x00
#define   BC_ID_VALUE_STATUS			GENMASK(31, 27)
#define   USB_LINE_STATUS			GENMASK(26, 25)
#define   USB_VBUS_STATUS			BIT(24)
#define   FORCE_RID				GENMASK(23, 18)
#define   FORCE_ID				GENMASK(15, 14)
#define   FORCE_VBUS_VALID			GENMASK(13, 12)
#define   EXT_VBUS_SRC_SEL			GENMASK(11, 10)
#define   USB_WAKEUP_ENABLE			BIT(9)
#define   USB_WAKEUP_HOSC_EN			BIT(8)
/* maybe abbreviation is better */
#define   ID_MUL_VAL_C_INT_STA			BIT(7)
#define   VBUS_IN_C_DET_INT_STA		BIT(6)
#define   ID_IN_C_DET_INT_STA			BIT(5)
#define   DPDM_IN_C_DET_INT_STA		BIT(4)
#define   ID_MUL_VAL_DET_EN			BIT(3)
#define   VBUS_IN_C_DET_INT_EN			BIT(2)
#define   ID_IN_C_DET_INT_EN			BIT(1)
#define   DPDM_IN_C_DET_INT_EN			BIT(0)

/* USB2.0 PHY Control Register */
#define PHY_USB2_PHYCTL			0x10
#define   OTGDISABLE				BIT(10)
#define   DRVVBUS				BIT(9)
#define   VREGBYPASS				BIT(8)
#define   LOOPBACKENB				BIT(7)
#define   IDPULLUP				BIT(6)
#define   VBUSVLDEXT				BIT(5)
#define   VBUSVLDEXTSEL			BIT(4)
#define   SIDDQ				BIT(3)
#define   COMMONONN				BIT(2)
#define   VATESTENB				GENMASK(1, 0)

/* Registers */
#define  PHY_REG_U2_ISCR(u2phy_base_addr)		((u2phy_base_addr) \
							+ PHY_USB2_ISCR)
#define  PHY_REG_U2_PHYCTL(u2phy_base_addr)		((u2phy_base_addr) \
							+ PHY_USB2_PHYCTL)

struct sunxi_phy {
	struct phy *phy;
	void __iomem *u2;
	struct reset_control *reset;
	struct clk *clk;
};

static void phy_u2_set(struct sunxi_phy *phy, bool enable)
{
	u32 val, tmp = 0;

	/* maybe it's unused, only for device */
#if IS_ENABLED(CONFIG_USB_DWC3_GADGET)
	val = readl(PHY_REG_U2_ISCR(phy->u2));
	if (enable)
		val |= FORCE_VBUS_VALID;
	else
		val &= ~FORCE_VBUS_VALID;
	writel(val, PHY_REG_U2_ISCR(phy->u2));
#endif

	val = readl(PHY_REG_U2_PHYCTL(phy->u2));
	tmp = OTGDISABLE | VBUSVLDEXT;
	if (enable) {
		val |= tmp;
		val &= ~SIDDQ; /* write 0 to enable phy */
	} else {
		val &= ~tmp;
		val |= SIDDQ; /* write 1 to disable phy */
	}

	writel(val, PHY_REG_U2_PHYCTL(phy->u2));
}

static int sunxi_phy_init(struct phy *_phy)
{
	struct sunxi_phy *phy = phy_get_drvdata(_phy);
	int ret;

	if (!IS_ERR(phy->clk)) {
		ret = clk_prepare_enable(phy->clk);
		if (ret)
			return ret;
	}

	if (!IS_ERR(phy->reset)) {
		ret = reset_control_deassert(phy->reset);
		if (ret) {
			if (!IS_ERR(phy->clk))
				clk_disable_unprepare(phy->clk);
			return ret;
		}
	}

	phy_u2_set(phy, true);

	return 0;
}

static int sunxi_phy_exit(struct phy *_phy)
{
	struct sunxi_phy *phy = phy_get_drvdata(_phy);

	phy_u2_set(phy, false);

	if (!IS_ERR(phy->reset))
		reset_control_assert(phy->reset);

	if (!IS_ERR(phy->clk))
		clk_disable_unprepare(phy->clk);

	return 0;
}

static const struct phy_ops sunxi_phy_ops = {
	.init		= sunxi_phy_init,
	.exit		= sunxi_phy_exit,
	.owner		= THIS_MODULE,
};

static int phy_sunxi_plat_probe(struct platform_device *pdev)
{
	struct sunxi_phy *phy;
	struct device *dev = &pdev->dev;
	struct phy_provider *phy_provider;

	phy = devm_kzalloc(dev, sizeof(*phy), GFP_KERNEL);
	if (!phy)
		return -ENOMEM;

	phy->clk = devm_clk_get(dev, NULL);
	if (IS_ERR(phy->clk)) {
		if (PTR_ERR(phy->clk) != -EPROBE_DEFER)
			dev_dbg(dev, "failed to get phy clock\n");
	}

	phy->reset = devm_reset_control_get(dev, NULL);
	if (IS_ERR(phy->reset))
		dev_dbg(dev, "failed to get reset control\n");

	phy->u2 = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(phy->u2))
		return PTR_ERR(phy->u2);

	phy->phy = devm_phy_create(dev, NULL, &sunxi_phy_ops);
	if (IS_ERR(phy->phy)) {
		dev_err(dev, "failed to create PHY\n");
		return PTR_ERR(phy->phy);
	}

	phy_set_drvdata(phy->phy, phy);
	phy_provider = devm_of_phy_provider_register(dev, of_phy_simple_xlate);

	dev_info(dev, "phy provider register success\n");

	return PTR_ERR_OR_ZERO(phy_provider);
}

static const struct of_device_id phy_sunxi_plat_of_match[] = {
	{ .compatible = "allwinner,sunxi-plat-phy" },
	{ /* Sentinel */ }
};
MODULE_DEVICE_TABLE(of, phy_sunxi_plat_of_match);

static struct platform_driver phy_sunxi_plat_driver = {
	.probe	= phy_sunxi_plat_probe,
	.driver = {
		.of_match_table	= phy_sunxi_plat_of_match,
		.name  = "sunxi-plat-phy",
	}
};
module_platform_driver(phy_sunxi_plat_driver);

MODULE_ALIAS("platform:sunxi-plat-phy");
MODULE_DESCRIPTION("Allwinner Platform USB 3.0 PHY driver");
MODULE_AUTHOR("kanghoupeng<kanghoupeng@allwinnertech.com>");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("1.0.2");
