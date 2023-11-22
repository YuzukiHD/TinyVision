/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright(c) 2020 - 2023 Allwinner Technology Co.,Ltd. All rights reserved. */
/*
 * Allwinner SoCs pmic driver.
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#define pr_fmt(x) KBUILD_MODNAME ": " x "\n"

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include "linux/irq.h"
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/param.h>
#include <linux/jiffies.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/ktime.h>
#include <linux/of.h>
#include <linux/timekeeping.h>
#include <linux/types.h>
#include <linux/string.h>
#include <asm/irq.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/pm_runtime.h>
#include <linux/kthread.h>
#include <linux/freezer.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include "power/axp2101.h"

#include <linux/err.h>
#include "axp2202_charger.h"

struct axp2202_gpio_power {
	char                      *name;
	struct device             *dev;
	struct regmap             *regmap;
	struct power_supply       *gpio_supply;
	struct power_supply       *usb_psy;
	struct axp_config_info  dts_info;
	struct delayed_work        gpio_supply_mon;
	struct delayed_work        gpio_chg_state;
	struct gpio_config axp_acin_det;
};


static enum power_supply_property axp2202_gpio_props[] = {
	POWER_SUPPLY_PROP_MODEL_NAME,
	POWER_SUPPLY_PROP_ONLINE,
};

static int axp2202_gpio_get_property(struct power_supply *psy,
				       enum power_supply_property psp,
				       union power_supply_propval *val)
{
	struct axp2202_gpio_power *gpio_power = power_supply_get_drvdata(psy);

	int ret = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_MODEL_NAME:
		val->strval = psy->desc->name;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = __gpio_get_value(gpio_power->axp_acin_det.gpio);
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

static const struct power_supply_desc axp2202_gpio_desc = {
	.name = "axp2202-gpio",
	.type = POWER_SUPPLY_TYPE_MAINS,
	.get_property = axp2202_gpio_get_property,
	.properties = axp2202_gpio_props,
	.num_properties = ARRAY_SIZE(axp2202_gpio_props),
};

int axp2202_irq_handler_gpio_in(void *data)
{
	struct axp2202_gpio_power *gpio_power = data;
	struct device_node *np = NULL;
	union power_supply_propval temp;

	pr_info("[acin_irq]axp2202_gpio_in\n");
	power_supply_changed(gpio_power->gpio_supply);
	if (gpio_power->usb_psy && (!IS_ERR(gpio_power->usb_psy))) {
		temp.intval = 2500;
		np = of_parse_phandle(gpio_power->dev->of_node, "det_usb_supply", 0);
		if (np)
			of_property_read_u32(np, "pmu_usbad_cur", &temp.intval);

		pr_info("[acin_irq] ad_current_limit = %d\n", temp.intval);
		power_supply_set_property(gpio_power->usb_psy,
						POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT, &temp);
	}
	return 0;
}

int axp2202_irq_handler_gpio_out(void *data)
{
	struct axp2202_gpio_power *gpio_power = data;
	struct device_node *np = NULL;
	union power_supply_propval temp;

	pr_info("[acout_irq]axp2202_gpio_out\n");
	power_supply_changed(gpio_power->gpio_supply);
	if (gpio_power->usb_psy && (!IS_ERR(gpio_power->usb_psy))) {
		power_supply_get_property(gpio_power->usb_psy, POWER_SUPPLY_PROP_ONLINE, &temp);
		if (temp.intval) {
			power_supply_get_property(gpio_power->usb_psy, POWER_SUPPLY_PROP_USB_TYPE, &temp);
			if (temp.intval == POWER_SUPPLY_USB_TYPE_SDP) {
				temp.intval = 500;
				np = of_parse_phandle(gpio_power->dev->of_node, "det_usb_supply", 0);
				if (np)
					of_property_read_u32(np, "pmu_usbpc_cur", &temp.intval);

				pr_info("[acout_irq] pc_current_limit = %d\n", temp.intval);
				power_supply_set_property(gpio_power->usb_psy,
							POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT, &temp);
			}
		}
	}
	return 0;
}

static irqreturn_t axp_acin_gpio_isr(int irq, void *data)
{
	struct axp2202_gpio_power *gpio_power = data;

	cancel_delayed_work_sync(&gpio_power->gpio_chg_state);
	schedule_delayed_work(&gpio_power->gpio_chg_state, 0);

	return IRQ_HANDLED;
}

enum axp2202_gpio_virq_index {
	AXP2202_VIRQ_GPIO,
};

static struct axp_interrupts axp_gpio_irq[] = {
	[AXP2202_VIRQ_GPIO] = { "pmu_acin_det_gpio", axp_acin_gpio_isr },
};

static int axp_acin_gpio_init(struct axp2202_gpio_power *gpio_power)
{
	unsigned long int config_set;
	int pull = 0;

	int id_irq_num = 0;
	unsigned long irq_flags = 0;
	int ret = 0, i;

	if (!gpio_is_valid(gpio_power->axp_acin_det.gpio)) {
		pr_warn("get pmu_acin_det_gpio is fail\n");
		return -EPROBE_DEFER;
	}

	ret = gpio_request(
			gpio_power->axp_acin_det.gpio,
			"pmu_acin_det_gpio");
	if (ret != 0) {
		pr_warn("pmu_acin_det gpio_request failed\n");
		return -EPROBE_DEFER;
	}
	/* set acin input */
	gpio_direction_input(gpio_power->axp_acin_det.gpio);

	/* irq config setting */
	config_set = SUNXI_PINCFG_PACK(
				PIN_CONFIG_BIAS_PULL_UP,
				pull);
	irq_flags = IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING |
			IRQF_ONESHOT | IRQF_NO_SUSPEND;
	/* set id gpio pull up */

	pinctrl_gpio_set_config(gpio_power->axp_acin_det.gpio,
			config_set);
	id_irq_num = gpio_to_irq(gpio_power->axp_acin_det.gpio);
	axp_gpio_irq[0].irq = id_irq_num;

	for (i = 0; i < ARRAY_SIZE(axp_gpio_irq); i++) {
		ret = devm_request_any_context_irq(gpio_power->dev, axp_gpio_irq[i].irq, axp_gpio_irq[i].isr, irq_flags,
				axp_gpio_irq[i].name, gpio_power);
		if (IS_ERR_VALUE((unsigned long)ret)) {
			pr_warn("Requested %s IRQ %d failed, err %d\n",
				axp_gpio_irq[i].name, axp_gpio_irq[i].irq, ret);
			return -EINVAL;
		}
		dev_dbg(gpio_power->dev, "Requested %s IRQ %d: %d\n",
			axp_gpio_irq[i].name, axp_gpio_irq[i].irq, ret);
	}

	return 0;
}
static void axp2202_gpio_power_set_current_fsm(struct work_struct *work)
{
	struct axp2202_gpio_power *gpio_power =
		container_of(work, typeof(*gpio_power), gpio_chg_state.work);
	int acin_det_gpio_value;

	acin_det_gpio_value = __gpio_get_value(gpio_power->axp_acin_det.gpio);

	pr_info("[ac_irq] ac_in_flag :%d\n", acin_det_gpio_value);
	if (acin_det_gpio_value == 1) {
		axp2202_irq_handler_gpio_in(gpio_power);
	} else {
		axp2202_irq_handler_gpio_out(gpio_power);
	}
}
static void axp2202_gpio_power_monitor(struct work_struct *work)
{
	struct axp2202_gpio_power *gpio_power =
		container_of(work, typeof(*gpio_power), gpio_supply_mon.work);

	schedule_delayed_work(&gpio_power->gpio_supply_mon, msecs_to_jiffies(1000));
}

int axp2202_gpio_dt_parse(struct device_node *node,
			 struct axp_config_info *axp_config)
{
	return 0;
}

static void axp2202_gpio_parse_device_tree(struct axp2202_gpio_power *gpio_power)
{
	int ret;
	struct axp_config_info *cfg;

	if (!gpio_power->dev->of_node) {
		pr_info("can not find device tree\n");
		return;
	}

	cfg = &gpio_power->dts_info;
	ret = axp2202_gpio_dt_parse(gpio_power->dev->of_node, cfg);
	if (ret) {
		pr_info("can not parse device tree err\n");
		return;
	}
}

static int axp2202_gpio_probe(struct platform_device *pdev)
{
	int ret = 0;

	struct axp2202_gpio_power *gpio_power;

	struct device_node *node = pdev->dev.of_node;
	struct axp20x_dev *axp_dev = dev_get_drvdata(pdev->dev.parent);
	struct power_supply_config psy_cfg = {};

	if (!of_device_is_available(node)) {
		pr_err("axp2202-acin device is not configed\n");
		return -ENODEV;
	}

	if (!axp_dev->irq) {
		pr_err("can not register axp2202-acin without irq\n");
		return -EINVAL;
	}

	gpio_power = devm_kzalloc(&pdev->dev, sizeof(*gpio_power), GFP_KERNEL);
	if (gpio_power == NULL) {
		pr_err("axp2202_gpio_power alloc failed\n");
		ret = -ENOMEM;
		goto err;
	}

	gpio_power->name = "axp2202_gpio";
	gpio_power->dev = &pdev->dev;
	gpio_power->regmap = axp_dev->regmap;

	/* parse device tree and set register */
	axp2202_gpio_parse_device_tree(gpio_power);

	psy_cfg.of_node = pdev->dev.of_node;
	psy_cfg.drv_data = gpio_power;

	gpio_power->gpio_supply = devm_power_supply_register(gpio_power->dev,
			&axp2202_gpio_desc, &psy_cfg);

	if (IS_ERR(gpio_power->gpio_supply)) {
		pr_err("axp2202 failed to register acin power-sypply\n");
		ret = PTR_ERR(gpio_power->gpio_supply);
		return ret;
	}

	if (of_find_property(gpio_power->dev->of_node, "det_usb_supply", NULL)) {
		gpio_power->usb_psy = devm_power_supply_get_by_phandle(gpio_power->dev, "det_usb_supply");
	} else {
		pr_err("axp2202 acin-sypply failed to find usb power\n");
		gpio_power->usb_psy =  NULL;
	}

	INIT_DELAYED_WORK(&gpio_power->gpio_supply_mon, axp2202_gpio_power_monitor);
	INIT_DELAYED_WORK(&gpio_power->gpio_chg_state, axp2202_gpio_power_set_current_fsm);

	gpio_power->axp_acin_det.gpio =
		of_get_named_gpio(pdev->dev.of_node,
				"pmu_acin_det_gpio", 0);

	ret = axp_acin_gpio_init(gpio_power);
	if (ret != 0) {
		pr_err("axp acin init failed\n");
		return ret;
	}

	schedule_delayed_work(&gpio_power->gpio_supply_mon, msecs_to_jiffies(1000));
	platform_set_drvdata(pdev, gpio_power);
	pr_info("axp2202_gpio_probe finish: %s , %d \n", __func__, __LINE__);

	return ret;

err:
	pr_err("%s,probe fail, ret = %d\n", __func__, ret);

	return ret;
}

static int axp2202_gpio_remove(struct platform_device *pdev)
{
	struct axp2202_gpio_power *gpio_power = platform_get_drvdata(pdev);

	cancel_delayed_work_sync(&gpio_power->gpio_supply_mon);
	cancel_delayed_work_sync(&gpio_power->gpio_chg_state);

	dev_dbg(&pdev->dev, "==============AXP2202 gpio unegister==============\n");
	if (gpio_power->gpio_supply)
		power_supply_unregister(gpio_power->gpio_supply);
	dev_dbg(&pdev->dev, "axp2202 teardown gpio dev\n");

	return 0;
}

static void axp2202_gpio_shutdown(struct platform_device *pdev)
{
	struct axp2202_gpio_power *gpio_power = platform_get_drvdata(pdev);

	cancel_delayed_work_sync(&gpio_power->gpio_supply_mon);
	cancel_delayed_work_sync(&gpio_power->gpio_chg_state);
}

static int axp2202_gpio_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct axp2202_gpio_power *gpio_power = platform_get_drvdata(pdev);

	cancel_delayed_work_sync(&gpio_power->gpio_supply_mon);
	cancel_delayed_work_sync(&gpio_power->gpio_chg_state);

	return 0;
}

static int axp2202_gpio_resume(struct platform_device *pdev)
{
	struct axp2202_gpio_power *gpio_power = platform_get_drvdata(pdev);

	schedule_delayed_work(&gpio_power->gpio_chg_state, 0);
	schedule_delayed_work(&gpio_power->gpio_supply_mon, 0);

	return 0;
}

static const struct of_device_id axp2202_gpio_power_match[] = {
	{
		.compatible = "x-powers,gpio-supply",
		.data = (void *)AXP2202_ID,
	}, {/* sentinel */}
};
MODULE_DEVICE_TABLE(of, axp2202_gpio_power_match);

static struct platform_driver axp2202_gpio_power_driver = {
	.driver = {
		.name = "gpio-supply",
		.of_match_table = axp2202_gpio_power_match,
	},
	.probe = axp2202_gpio_probe,
	.remove = axp2202_gpio_remove,
	.shutdown = axp2202_gpio_shutdown,
	.suspend = axp2202_gpio_suspend,
	.resume = axp2202_gpio_resume,
};

module_platform_driver(axp2202_gpio_power_driver);

MODULE_VERSION("1.0.0");
MODULE_AUTHOR("xinouyang <xinouyang@allwinnertech.com>");
MODULE_DESCRIPTION("axp2202 gpio driver");
MODULE_LICENSE("GPL");
