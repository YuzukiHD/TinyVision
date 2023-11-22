/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright(c) 2020 - 2023 Allwinner Technology Co.,Ltd. All rights reserved. */
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
#include "power/axp2101.h"
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/extcon.h>
#include <linux/extcon-provider.h>
#include <linux/err.h>
#include <linux/notifier.h>
#include "axp2202_charger.h"

struct axp2202_usb_power {
	char                      *name;
	struct device             *dev;
	struct regmap             *regmap;
	struct power_supply       *usb_supply;
	struct axp_config_info  dts_info;
	struct delayed_work        usb_supply_mon;
	struct delayed_work        usb_chg_state;
	struct delayed_work        usb_det_mon;
	struct extcon_dev         *edev;
	struct gpio_config axp_vbus_det;
	struct gpio_config usbid_drv;
	int vbus_det_used;

	atomic_t set_current_limit;
};

static enum power_supply_property axp2202_usb_props[] = {
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_MANUFACTURER,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN,
	POWER_SUPPLY_PROP_SCOPE,
};

static const unsigned int axp2202_usb_extcon_cable[] = {
	EXTCON_JACK_HEADPHONE,
	EXTCON_NONE,
};

ATOMIC_NOTIFIER_HEAD(usb_power_notifier_list);
EXPORT_SYMBOL(usb_power_notifier_list);

static int axp2202_get_vbus_vol(struct power_supply *ps,
			     union power_supply_propval *val)
{
	struct axp2202_usb_power *usb_power = power_supply_get_drvdata(ps);
	struct regmap *regmap = usb_power->regmap;

	uint8_t data[2];
	uint16_t vol;
	int ret = 0;

	ret = regmap_bulk_read(regmap, AXP2202_VBUS_H, data, 2);
	if (ret < 0)
		return ret;
	vol = (((data[0] & GENMASK(5, 0)) << 8) | (data[1])); /* mA */
	val->intval = vol;

	return 0;
}

static int axp2202_get_vbus_online(struct power_supply *ps,
				   union power_supply_propval *val)
{
	struct axp2202_usb_power *usb_power = power_supply_get_drvdata(ps);
	struct regmap *regmap = usb_power->regmap;

	unsigned int data;
	int ret = 0;

	if (usb_power->vbus_det_used) {
		val->intval = __gpio_get_value(usb_power->axp_vbus_det.gpio);
		return ret;
	}

	ret = regmap_read(regmap, AXP2202_COMM_STAT0, &data);
	if (ret < 0)
		return ret;

	if (data & AXP2202_MASK_VBUS_STAT)
		val->intval = 1;
	else
		val->intval = 0;

	return ret;
}

static int axp2202_get_vbus_state(struct power_supply *ps,
				   union power_supply_propval *val)
{
	struct axp2202_usb_power *usb_power = power_supply_get_drvdata(ps);
	struct regmap *regmap = usb_power->regmap;
	unsigned int data;
	int ret = 0;

	if (usb_power->vbus_det_used) {
		val->intval = __gpio_get_value(usb_power->axp_vbus_det.gpio);
		return ret;
	}

	ret = regmap_read(regmap, AXP2202_COMM_STAT0, &data);
	if (ret < 0)
		return ret;

	/* vbus is good when vbus state set */
	val->intval = !!(data & AXP2202_MASK_VBUS_STAT);

	return ret;
}

static int axp2202_get_iin_limit(struct power_supply *ps,
				   union power_supply_propval *val)
{
	struct axp2202_usb_power *usb_power = power_supply_get_drvdata(ps);
	struct regmap *regmap = usb_power->regmap;
	unsigned int data;
	int ret = 0;

	ret = regmap_read(regmap, AXP2202_IIN_LIM, &data);
	if (ret < 0)
		return ret;
	data &= 0x3F;
	data = (data * 50) + 100;
	val->intval = data;
	return ret;
}

static int axp2202_get_vindpm(struct power_supply *ps,
				   union power_supply_propval *val)
{
	struct axp2202_usb_power *usb_power = power_supply_get_drvdata(ps);
	struct regmap *regmap = usb_power->regmap;
	unsigned int data;
	int ret = 0;

	ret = regmap_read(regmap, AXP2202_VINDPM_CFG, &data);
	if (ret < 0)
		return ret;

	data = (data * 80) + 3880;
	val->intval = data;

	return ret;
}

static int axp2202_get_usb_type(struct power_supply *ps,
				   union power_supply_propval *val)
{
	struct axp2202_usb_power *usb_power = power_supply_get_drvdata(ps);
	int ret = 0;

	if (atomic_read(&usb_power->set_current_limit)) {
		val->intval = POWER_SUPPLY_USB_TYPE_SDP;
	} else {
		val->intval = POWER_SUPPLY_USB_TYPE_DCP;
	}

	return ret;
}

static int axp2202_get_cc_status(struct power_supply *ps,
				   union power_supply_propval *val)
{
	struct axp2202_usb_power *usb_power = power_supply_get_drvdata(ps);
	struct axp_config_info *dinfo = &usb_power->dts_info;
	struct regmap *regmap = usb_power->regmap;
	unsigned int data;
	int ret = 0;

	if (!dinfo->pmu_usb_typec_used) {
		val->intval = POWER_SUPPLY_SCOPE_UNKNOWN;
		return ret;
	}

	ret = regmap_read(regmap, AXP2202_CC_STAT0, &data);
	if (ret < 0)
		return ret;

	data &= 0x0f;

	/* intval bit[1:0]: 0x0 - unknown, 0x1 - source, 0x2 - sink */
	if (data == 5 || data == 6 || data == 9 || data == 12) {
		val->intval = POWER_SUPPLY_SCOPE_SYSTEM;//source/host
	} else if (data == 2 || data == 3 || data == 10 || data == 11) {
		val->intval = POWER_SUPPLY_SCOPE_DEVICE;//sink/
	} else {
		val->intval = POWER_SUPPLY_SCOPE_UNKNOWN;//disable
	}

	ret = regmap_read(regmap, AXP2202_CC_STAT4, &data);
	if (ret < 0)
		return ret;

	data &= BIT(6);

	/* intval bit[2]: 0x0 - cc2, 0x1 - cc1 */
	val->intval |= (data >> 4);

	return ret;
}


/* read temperature */
static int axp2202_get_ic_temp(struct power_supply *ps,
			     union power_supply_propval *val)
{
	struct axp2202_usb_power *usb_power = power_supply_get_drvdata(ps);
	struct regmap *regmap = usb_power->regmap;

	uint8_t data[2];
	int temp;
	int ret = 0;

	ret = regmap_update_bits(regmap, AXP2202_ADC_DATA_SEL, 0x03, AXP2202_ADC_TDIE_SEL); /* ADC channel select */
	if (ret < 0)
		return ret;
	mdelay(1);

	ret = regmap_bulk_read(regmap, AXP2202_ADC_DATA_H, data, 2);
	if (ret < 0)
		return ret;
	temp = (753 - ((data[0] << 8) + data[1])) * 1000 / 173;
	val->intval = temp;

	return 0;
}


static int axp2202_set_iin_limit(struct regmap *regmap, int mA)
{
	unsigned int data;
	int ret = 0;

	data = mA;
	if (data > 3250)
		data = 3250;
	if	(data < 100)
		data = 100;
	data = ((data - 100) / 50);
	ret = regmap_update_bits(regmap, AXP2202_IIN_LIM, GENMASK(5, 0),
				 data);
	if (ret < 0)
		return ret;

	return 0;
}

static int axp2202_set_vindpm(struct regmap *regmap, int mV)
{
	unsigned int data;
	int ret = 0;

	data = mV;

	if (data > 5080)
		data = 5080;
	if	(data < 3880)
		data = 3880;
	data = ((data - 3880) / 80);
	ret = regmap_update_bits(regmap, AXP2202_VINDPM_CFG, GENMASK(3, 0),
				 data);
	if (ret < 0)
		return ret;

	return 0;
}

static int axp2202_set_cc_status(struct power_supply *ps, int type)
{
	struct axp2202_usb_power *usb_power = power_supply_get_drvdata(ps);
	struct axp_config_info *dinfo = &usb_power->dts_info;
	struct regmap *regmap = usb_power->regmap;
	unsigned int data = 0x03;
	int ret = 0;

	if (!dinfo->pmu_usb_typec_used) {
		return ret;
	}

	switch (type) {
	case POWER_SUPPLY_SCOPE_SYSTEM://source/host
		regmap_update_bits(regmap, AXP2202_CC_MODE_CTRL, BIT(5), BIT(5));
		regmap_update_bits(regmap, AXP2202_CC_MODE_CTRL, BIT(4), 0);
		data = 0x02;
		break;
	case POWER_SUPPLY_SCOPE_DEVICE://sink/
		regmap_update_bits(regmap, AXP2202_CC_MODE_CTRL, BIT(4), BIT(4));
		regmap_update_bits(regmap, AXP2202_CC_MODE_CTRL, BIT(5), 0);
		data = 0x01;
		break;
	case POWER_SUPPLY_SCOPE_UNKNOWN:
		regmap_update_bits(regmap, AXP2202_CC_MODE_CTRL, BIT(4), BIT(4));
		regmap_update_bits(regmap, AXP2202_CC_MODE_CTRL, BIT(5), 0);
		break;
	default:
		break;
	}

	regmap_update_bits(regmap, AXP2202_CC_MODE_CTRL, GENMASK(1, 0), data);
	return ret;
}

static int axp2202_usb_get_property(struct power_supply *psy,
				       enum power_supply_property psp,
				       union power_supply_propval *val)
{
	int ret = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		ret = axp2202_get_vbus_online(psy, val);
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		ret = axp2202_get_vbus_state(psy, val);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		ret = axp2202_get_vbus_vol(psy, val);
		break;
	case POWER_SUPPLY_PROP_MANUFACTURER:
		val->strval = AXP2202_MANUFACTURER;
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		ret = axp2202_get_iin_limit(psy, val);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN:
		ret = axp2202_get_vindpm(psy, val);
		break;
	case POWER_SUPPLY_PROP_USB_TYPE:
		ret = axp2202_get_usb_type(psy, val);
		break;
	case POWER_SUPPLY_PROP_SCOPE:
		ret = axp2202_get_cc_status(psy, val);
		break;
	case POWER_SUPPLY_PROP_TEMP:
		ret = axp2202_get_ic_temp(psy, val);
		if (ret < 0)
			return ret;
		break;
	default:
		break;
	}

	return ret;
}

static int axp2202_usb_set_property(struct power_supply *psy,
				enum power_supply_property psp,
				const union power_supply_propval *val)
{
	struct axp2202_usb_power *usb_power = power_supply_get_drvdata(psy);

	struct regmap *regmap = usb_power->regmap;
	struct power_supply *ps = NULL;
	union power_supply_propval temp;
	int ret = 0, usb_cur;

	switch (psp) {
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		usb_cur = val->intval;

		if (usb_cur < usb_power->dts_info.pmu_usbad_cur) {
			atomic_set(&usb_power->set_current_limit, 1);
		}

		if (of_find_property(usb_power->dev->of_node, "det_acin_supply", NULL))
			ps = devm_power_supply_get_by_phandle(usb_power->dev,
								"det_acin_supply");
		if (ps && (!IS_ERR(ps))) {
			if (of_device_is_available(ps->of_node)) {
				power_supply_get_property(ps, POWER_SUPPLY_PROP_ONLINE, &temp);
				if (temp.intval) {
					usb_cur = usb_power->dts_info.pmu_usbad_cur;
				}
			}
		}
		ret = axp2202_set_iin_limit(regmap, usb_cur);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN:
		ret = axp2202_set_vindpm(regmap, val->intval);
		break;
	case POWER_SUPPLY_PROP_SCOPE:
		ret = axp2202_set_cc_status(psy, val->intval);
		break;
	default:
		ret = -EINVAL;
	}
	return ret;
}

static int axp2202_usb_power_property_is_writeable(struct power_supply *psy,
			     enum power_supply_property psp)
{
	int ret = 0;
	switch (psp) {
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		ret = 0;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN:
		ret = 0;
		break;
	case POWER_SUPPLY_PROP_SCOPE:
		ret = 0;
		break;
	default:
		ret = -EINVAL;
	}
	return ret;

}

static const struct power_supply_desc axp2202_usb_desc = {
	.name = "axp2202-usb",
	.type = POWER_SUPPLY_TYPE_USB,
	.get_property = axp2202_usb_get_property,
	.properties = axp2202_usb_props,
	.set_property = axp2202_usb_set_property,
	.num_properties = ARRAY_SIZE(axp2202_usb_props),
	.property_is_writeable = axp2202_usb_power_property_is_writeable,
};

static irqreturn_t axp2202_irq_handler_usb_in(int irq, void *data)
{
	struct axp2202_usb_power *usb_power = data;
	struct axp_config_info *axp_config = &usb_power->dts_info;

	atomic_notifier_call_chain(&usb_power_notifier_list, 1, NULL);
	if (!usb_power->vbus_det_used) {
		power_supply_changed(usb_power->usb_supply);
		if (!axp_config->pmu_bc12_en) {
			axp2202_set_iin_limit(usb_power->regmap, axp_config->pmu_usbpc_cur);
			atomic_set(&usb_power->set_current_limit, 0);
			cancel_delayed_work_sync(&usb_power->usb_chg_state);
			schedule_delayed_work(&usb_power->usb_chg_state, msecs_to_jiffies(5 * 1000));
		}
	}

	return IRQ_HANDLED;
}

static irqreturn_t axp2202_irq_handler_usb_out(int irq, void *data)
{
	struct axp2202_usb_power *usb_power = data;

	atomic_notifier_call_chain(&usb_power_notifier_list, 1, NULL);
	if (!usb_power->vbus_det_used) {
		power_supply_changed(usb_power->usb_supply);
	}

	return IRQ_HANDLED;
}

static irqreturn_t axp2202_irq_handler_typec_in(int irq, void *data)
{
	struct axp2202_usb_power *usb_power = data;
	unsigned int reg_val;

	regmap_read(usb_power->regmap, AXP2202_CC_STAT0, &reg_val);
	atomic_notifier_call_chain(&usb_power_notifier_list, 0, NULL);

	switch (reg_val & 0xf) {
	case 0x07:
		extcon_set_state_sync(usb_power->edev, EXTCON_JACK_HEADPHONE, true);
		break;
	case 0x5:
	case 0x6:
	case 0x9:
	case 0xc:
		if (usb_power->vbus_det_used)
			gpio_direction_output(usb_power->usbid_drv.gpio, 0);
		break;
	default:
		pr_info("No operation cc_status\n");
	}

	return IRQ_HANDLED;
}

static irqreturn_t axp2202_irq_handler_typec_out(int irq, void *data)
{
	struct axp2202_usb_power *usb_power = data;

	atomic_notifier_call_chain(&usb_power_notifier_list, 0, NULL);
	extcon_set_state_sync(usb_power->edev, EXTCON_JACK_HEADPHONE, false);
	if (usb_power->vbus_det_used)
		gpio_direction_output(usb_power->usbid_drv.gpio, 1);

	return IRQ_HANDLED;
}

static irqreturn_t axp2202_acin_vbus_det_isr(int irq, void *data)
{
	struct axp2202_usb_power *usb_power = data;

	cancel_delayed_work_sync(&usb_power->usb_det_mon);
	schedule_delayed_work(&usb_power->usb_det_mon, 0);

	return IRQ_HANDLED;
}

enum axp2202_usb_virq_index {
	AXP2202_VIRQ_USB_IN,
	AXP2202_VIRQ_USB_OUT,
	AXP2202_VIRQ_TYPEC_IN,
	AXP2202_VIRQ_TYPEC_OUT,

	AXP2202_USB_VIRQ_MAX_VIRQ,
};

static struct axp_interrupts axp_usb_irq[] = {
	[AXP2202_VIRQ_USB_IN] = { "vbus_insert", axp2202_irq_handler_usb_in },
	[AXP2202_VIRQ_USB_OUT] = { "vbus_remove", axp2202_irq_handler_usb_out },
	[AXP2202_VIRQ_TYPEC_IN] = { "type-c_insert", axp2202_irq_handler_typec_in },
	[AXP2202_VIRQ_TYPEC_OUT] = { "type-c_remove", axp2202_irq_handler_typec_out },

};

static void axp2202_usb_power_monitor(struct work_struct *work)
{
	struct axp2202_usb_power *usb_power =
		container_of(work, typeof(*usb_power), usb_supply_mon.work);

	schedule_delayed_work(&usb_power->usb_supply_mon, msecs_to_jiffies(500));
}

static void axp2202_usb_set_current_fsm(struct work_struct *work)
{
	struct axp2202_usb_power *usb_power =
		container_of(work, typeof(*usb_power), usb_chg_state.work);
	struct axp_config_info *axp_config = &usb_power->dts_info;

	if (atomic_read(&usb_power->set_current_limit)) {
		pr_info("current limit setted: usb pc type\n");
	} else {
		axp2202_set_iin_limit(usb_power->regmap, axp_config->pmu_usbad_cur);
		pr_info("current limit not set: usb adapter type\n");
	}
}

static void axp2202_usb_det_monitor(struct work_struct *work)
{
	struct axp2202_usb_power *usb_power =
		container_of(work, typeof(*usb_power), usb_det_mon.work);
	struct axp_config_info *axp_config = &usb_power->dts_info;
	int vbus_det_gpio_value;

	if (!usb_power->vbus_det_used) {
		pr_info("[usb_det] acin_usb_det not used\n");
		return;
	}

	vbus_det_gpio_value = __gpio_get_value(usb_power->axp_vbus_det.gpio);

	pr_info("[usb_det] vbus_dev_flag = %d\n", vbus_det_gpio_value);
	power_supply_changed(usb_power->usb_supply);
	if (vbus_det_gpio_value) {
		if (!axp_config->pmu_bc12_en) {
			axp2202_set_iin_limit(usb_power->regmap, axp_config->pmu_usbpc_cur);
			atomic_set(&usb_power->set_current_limit, 0);
			cancel_delayed_work_sync(&usb_power->usb_chg_state);
			schedule_delayed_work(&usb_power->usb_chg_state, msecs_to_jiffies(5 * 1000));
		}
	}
}

static int axp2202_acin_vbus_det_init(struct axp2202_usb_power *usb_power)
{
	unsigned long int config_set;
	int pull = 0, ret = 0, vbus_det_irq_num = 0;
	unsigned long irq_flags = 0;

	usb_power->vbus_det_used = 0;

	usb_power->axp_vbus_det.gpio =
		of_get_named_gpio(usb_power->dev->of_node,
				"pmu_vbus_det_gpio", 0);
	if (!gpio_is_valid(usb_power->axp_vbus_det.gpio)) {
		pr_err("get axp_vbus_det_gpio is fail\n");
		usb_power->axp_vbus_det.gpio = 0;
		return -EPROBE_DEFER;
	}

	usb_power->usbid_drv.gpio =
		of_get_named_gpio(usb_power->dev->of_node,
				"pmu_acin_usbid_drv", 0);
	if (!gpio_is_valid(usb_power->usbid_drv.gpio)) {
		pr_err("get pmu_usbid_drv_gpio is fail\n");
		usb_power->usbid_drv.gpio = 0;
		return -EPROBE_DEFER;
	}

	ret = gpio_request(
			usb_power->axp_vbus_det.gpio,
			"pmu_vbus_det_gpio");
	if (ret != 0) {
		pr_err("pmu_vbus_det_gpio gpio_request failed\n");
		return -EINVAL;
	}

	ret = gpio_request(
			usb_power->usbid_drv.gpio,
			"pmu_acin_usbid_drv");
	if (ret != 0) {
		pr_err("pmu_usbid_drv gpio_request failed\n");
		return -EINVAL;
	}
	/* set vbus_det input usbid output */
	gpio_direction_input(usb_power->axp_vbus_det.gpio);
	gpio_direction_output(usb_power->usbid_drv.gpio, 1);

	/* init delay work */
	INIT_DELAYED_WORK(&usb_power->usb_det_mon, axp2202_usb_det_monitor);

	/* irq config setting */
	config_set = SUNXI_PINCFG_PACK(
				PIN_CONFIG_BIAS_PULL_UP,
				pull);
	irq_flags = IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING |
			IRQF_ONESHOT | IRQF_NO_SUSPEND;
	/* set id gpio pull up */
	pinctrl_gpio_set_config(usb_power->axp_vbus_det.gpio,
			config_set);
	vbus_det_irq_num = gpio_to_irq(usb_power->axp_vbus_det.gpio);

	ret = devm_request_any_context_irq(usb_power->dev, vbus_det_irq_num, axp2202_acin_vbus_det_isr, irq_flags,
				"pmu_vbus_det_gpio", usb_power);
	if (IS_ERR_VALUE((unsigned long)ret)) {
		cancel_delayed_work_sync(&usb_power->usb_det_mon);
		pr_err("Requested pmu_vbus_det_gpio IRQ failed, err %d\n", ret);
		return -EINVAL;
	}
	dev_dbg(usb_power->dev, "Requested pmu_vbus_det_gpio IRQ successed: %d\n", ret);

	usb_power->vbus_det_used = 1;
	return 0;
}

static void axp2202_usb_power_init(struct axp2202_usb_power *usb_power)
{
	struct regmap *regmap = usb_power->regmap;
	struct axp_config_info *dinfo = &usb_power->dts_info;

	unsigned int data = 0;

	/* set vindpm value */
	axp2202_set_vindpm(regmap, dinfo->pmu_usbad_vol);

	/* set bc12 en/disable  */
	if (dinfo->pmu_bc12_en) {
		regmap_update_bits(regmap, AXP2202_CLK_EN, BIT(4), BIT(4));
		regmap_update_bits(regmap, AXP2202_IIN_LIM, BIT(7), 0);
	} else {
		regmap_update_bits(regmap, AXP2202_CLK_EN, BIT(4), 0);
		regmap_update_bits(regmap, AXP2202_IIN_LIM, BIT(7), BIT(7));
	}

	/* set boost vol  */
	data = (dinfo->pmu_boost_vol - 4550) / 64;
	regmap_update_bits(regmap, AXP2202_BST_CFG0, GENMASK(7, 4), data << 4);
	regmap_write(regmap, AXP2202_BST_CFG1, 0x03);

	/* set type-c en/disable & mode */
	if (dinfo->pmu_usb_typec_used) {
		regmap_update_bits(regmap, AXP2202_CLK_EN, BIT(3), BIT(3));
		regmap_update_bits(regmap, AXP2202_CC_GLB_CTRL, BIT(2), 0);
		regmap_update_bits(regmap, AXP2202_CC_GLB_CTRL, BIT(5), BIT(5));
		regmap_update_bits(regmap, AXP2202_CC_MODE_CTRL, BIT(1), BIT(1));
		regmap_update_bits(regmap, AXP2202_CC_MODE_CTRL, BIT(0), BIT(0));
	} else {
		regmap_update_bits(regmap, AXP2202_CLK_EN, BIT(3), 0);
		regmap_update_bits(regmap, AXP2202_CC_MODE_CTRL, BIT(1), 0);
		regmap_update_bits(regmap, AXP2202_CC_MODE_CTRL, BIT(0), 0);
	}
}

static void axp2202_extcon_state_init(struct axp2202_usb_power *usb_power)
{
	unsigned int reg_val;

	regmap_read(usb_power->regmap, AXP2202_CC_STAT0, &reg_val);

	if ((reg_val & 0xf) == 0x7)
		extcon_set_state_sync(usb_power->edev, EXTCON_JACK_HEADPHONE, true);
	else
		extcon_set_state_sync(usb_power->edev, EXTCON_JACK_HEADPHONE, false);
}

static int axp2202_usb_dt_parse(struct device_node *node,
			 struct axp_config_info *axp_config)
{
	if (!of_device_is_available(node)) {
		pr_err("%s: failed\n", __func__);
		return -1;
	}


	AXP_OF_PROP_READ(pmu_usbpc_vol,                    4600);
	AXP_OF_PROP_READ(pmu_usbpc_cur,                    500);
	AXP_OF_PROP_READ(pmu_usbad_vol,                    4600);
	AXP_OF_PROP_READ(pmu_usbad_cur,                    1500);
	AXP_OF_PROP_READ(pmu_bc12_en,                         0);
	AXP_OF_PROP_READ(pmu_cc_logic_en,                     1);
	AXP_OF_PROP_READ(pmu_boost_en,                        0);
	AXP_OF_PROP_READ(pmu_boost_vol,                    5126);
	AXP_OF_PROP_READ(pmu_usb_typec_used,                  0);

	axp_config->wakeup_usb_in =
		of_property_read_bool(node, "wakeup_usb_in");
	axp_config->wakeup_usb_out =
		of_property_read_bool(node, "wakeup_usb_out");

	axp_config->wakeup_typec_in =
		of_property_read_bool(node, "wakeup_typec_in");
	axp_config->wakeup_typec_out =
		of_property_read_bool(node, "wakeup_typec_out");

	return 0;
}

static void axp2202_usb_parse_device_tree(struct axp2202_usb_power *usb_power)
{
	int ret;
	struct axp_config_info *cfg;

	/* set input current limit */
	if (!usb_power->dev->of_node) {
		pr_info("can not find device tree\n");
		return;
	}

	cfg = &usb_power->dts_info;
	ret = axp2202_usb_dt_parse(usb_power->dev->of_node, cfg);
	if (ret) {
		pr_info("can not parse device tree err\n");
		return;
	}

	/*init axp2202 usb by device tree*/
	axp2202_usb_power_init(usb_power);
}

static int axp2202_usb_probe(struct platform_device *pdev)
{
	int ret = 0;
	int i = 0, irq;

	struct axp2202_usb_power *usb_power;

	struct axp20x_dev *axp_dev = dev_get_drvdata(pdev->dev.parent);
	struct power_supply_config psy_cfg = {};
	struct device_node *np = NULL;

	if (!axp_dev->irq) {
		pr_err("can not register axp2202-usb without irq\n");
		return -EINVAL;
	}

	usb_power = devm_kzalloc(&pdev->dev, sizeof(*usb_power), GFP_KERNEL);
	if (usb_power == NULL) {
		pr_err("axp2202_usb_power alloc failed\n");
		ret = -ENOMEM;
		goto err;
	}

	usb_power->name = "axp2202_usb";
	usb_power->dev = &pdev->dev;
	usb_power->regmap = axp_dev->regmap;

	/* parse device tree and set register */
	axp2202_usb_parse_device_tree(usb_power);

	psy_cfg.of_node = pdev->dev.of_node;
	psy_cfg.drv_data = usb_power;

	usb_power->usb_supply = devm_power_supply_register(usb_power->dev,
			&axp2202_usb_desc, &psy_cfg);

	if (IS_ERR(usb_power->usb_supply)) {
		pr_err("axp2202 failed to register usb power\n");
		ret = PTR_ERR(usb_power->usb_supply);
		return ret;
	}

	usb_power->edev = devm_extcon_dev_allocate(usb_power->dev, axp2202_usb_extcon_cable);
	if (IS_ERR(usb_power->edev)) {
		dev_err(usb_power->dev, "failed to allocate extcon device\n");
		return -ENOMEM;
	}

	ret = devm_extcon_dev_register(usb_power->dev, usb_power->edev);
	if (ret < 0) {
		dev_err(usb_power->dev, "failed to register extcon device\n");
		return ret;
	}

	axp2202_extcon_state_init(usb_power);

	if (!usb_power->dts_info.pmu_bc12_en) {
		INIT_DELAYED_WORK(&usb_power->usb_supply_mon, axp2202_usb_power_monitor);
		INIT_DELAYED_WORK(&usb_power->usb_chg_state, axp2202_usb_set_current_fsm);
	}

	np = of_parse_phandle(usb_power->dev->of_node, "det_acin_supply", 0);
	if (!of_device_is_available(np)) {
		pr_err("axp2202-acin device is not configed, not use vbus-det\n");
	} else {
		ret = axp2202_acin_vbus_det_init(usb_power);
		if (ret < 0) {
			pr_err("failed to register axp2202-acin function\n");
			return ret;
		}
	}

	for (i = 0; i < ARRAY_SIZE(axp_usb_irq); i++) {
		irq = platform_get_irq_byname(pdev, axp_usb_irq[i].name);
		if (irq < 0)
			continue;

		irq = regmap_irq_get_virq(axp_dev->regmap_irqc, irq);
		if (irq < 0) {
			dev_err(&pdev->dev, "can not get irq\n");
			ret = irq;
			goto cancel_work;
		}
		/* we use this variable to suspend irq */
		axp_usb_irq[i].irq = irq;
		ret = devm_request_any_context_irq(&pdev->dev, irq,
						   axp_usb_irq[i].isr, 0,
						   axp_usb_irq[i].name, usb_power);
		if (ret < 0) {
			dev_err(&pdev->dev, "failed to request %s IRQ %d: %d\n",
				axp_usb_irq[i].name, irq, ret);
			goto cancel_work;
		} else {
			ret = 0;
		}

		dev_dbg(&pdev->dev, "Requested %s IRQ %d: %d\n",
			axp_usb_irq[i].name, irq, ret);
	}

	if (!usb_power->dts_info.pmu_bc12_en) {
		schedule_delayed_work(&usb_power->usb_supply_mon, msecs_to_jiffies(500));
		schedule_delayed_work(&usb_power->usb_chg_state, msecs_to_jiffies(20 * 1000));
	}

	platform_set_drvdata(pdev, usb_power);

	return ret;

cancel_work:
	if (!usb_power->dts_info.pmu_bc12_en) {
		cancel_delayed_work_sync(&usb_power->usb_supply_mon);
		cancel_delayed_work_sync(&usb_power->usb_chg_state);
	}


err:
	pr_err("%s,probe fail, ret = %d\n", __func__, ret);

	return ret;
}

static int axp2202_usb_remove(struct platform_device *pdev)
{
	struct axp2202_usb_power *usb_power = platform_get_drvdata(pdev);

	if (!usb_power->dts_info.pmu_bc12_en) {
		cancel_delayed_work_sync(&usb_power->usb_supply_mon);
		cancel_delayed_work_sync(&usb_power->usb_chg_state);
	}
	if (usb_power->vbus_det_used)
		cancel_delayed_work_sync(&usb_power->usb_det_mon);

	dev_dbg(&pdev->dev, "==============AXP2202 usb unegister==============\n");
	if (usb_power->usb_supply)
		power_supply_unregister(usb_power->usb_supply);
	dev_dbg(&pdev->dev, "axp2202 teardown usb dev\n");

	return 0;
}

static inline void axp2202_usb_irq_set(unsigned int irq, bool enable)
{
	if (enable)
		enable_irq(irq);
	else
		disable_irq(irq);
}

static void axp2202_usb_virq_dts_set(struct axp2202_usb_power *usb_power, bool enable)
{
	struct axp_config_info *dts_info = &usb_power->dts_info;

	if (!dts_info->wakeup_usb_in)
		axp2202_usb_irq_set(axp_usb_irq[AXP2202_VIRQ_USB_IN].irq,
				enable);

	if (!dts_info->wakeup_usb_out)
		axp2202_usb_irq_set(axp_usb_irq[AXP2202_VIRQ_USB_OUT].irq,
				enable);

	if (dts_info->pmu_usb_typec_used) {
		if (!dts_info->wakeup_typec_in)
			axp2202_usb_irq_set(axp_usb_irq[AXP2202_VIRQ_TYPEC_IN].irq,
				enable);
		if (!dts_info->wakeup_typec_out)
			axp2202_usb_irq_set(axp_usb_irq[AXP2202_VIRQ_TYPEC_OUT].irq,
					enable);
	}
}

static void axp2202_usb_shutdown(struct platform_device *pdev)
{
	struct axp2202_usb_power *usb_power = platform_get_drvdata(pdev);

	cancel_delayed_work_sync(&usb_power->usb_supply_mon);
	cancel_delayed_work_sync(&usb_power->usb_chg_state);
}

static int axp2202_usb_suspend(struct platform_device *p, pm_message_t state)
{
	struct axp2202_usb_power *usb_power = platform_get_drvdata(p);

	axp2202_usb_virq_dts_set(usb_power, false);

	if (!usb_power->dts_info.pmu_bc12_en) {
		cancel_delayed_work_sync(&usb_power->usb_supply_mon);
		cancel_delayed_work_sync(&usb_power->usb_chg_state);
	}
	if (usb_power->vbus_det_used)
		cancel_delayed_work_sync(&usb_power->usb_det_mon);

	return 0;
}

static int axp2202_usb_resume(struct platform_device *p)
{
	struct axp2202_usb_power *usb_power = platform_get_drvdata(p);

	if (!usb_power->dts_info.pmu_bc12_en) {
		schedule_delayed_work(&usb_power->usb_supply_mon, 0);
		schedule_delayed_work(&usb_power->usb_chg_state, 0);
	}
	if (usb_power->vbus_det_used)
		schedule_delayed_work(&usb_power->usb_det_mon, 0);

	axp2202_usb_virq_dts_set(usb_power, true);

	return 0;
}

static const struct of_device_id axp2202_usb_power_match[] = {
	{
		.compatible = "x-powers,axp2202-usb-power-supply",
		.data = (void *)AXP2202_ID,
	}, {/* sentinel */}
};
MODULE_DEVICE_TABLE(of, axp2202_usb_power_match);

static struct platform_driver axp2202_usb_power_driver = {
	.driver = {
		.name = "axp2202-usb-power-supply",
		.of_match_table = axp2202_usb_power_match,
	},
	.probe = axp2202_usb_probe,
	.remove = axp2202_usb_remove,
	.shutdown = axp2202_usb_shutdown,
	.suspend = axp2202_usb_suspend,
	.resume = axp2202_usb_resume,
};

module_platform_driver(axp2202_usb_power_driver);

MODULE_AUTHOR("wangxiaoliang <wangxiaoliang@x-powers.com>");
MODULE_DESCRIPTION("axp2202 usb driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0.0");
