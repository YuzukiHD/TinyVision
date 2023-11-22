/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Regulator driver for TI TCS4838x PMICs
 *
 * Copyright (C) 2015 Texas Instruments Incorporated - http://www.ti.com/
 *	Andrew F. Davis <afd@ti.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether expressed or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License version 2 for more details.
 *
 * Based on the TPS65218 driver and the previous TCS4838 driver by
 * Margarita Olaya Cabrera <magi@slimlogic.co.uk>
 */

#include <linux/err.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <power/pmu-ext.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>

#define PMU_EXT_REGULATOR_STEP_DELAY(_family, _id, _match, _supply, _min, _max, _step, _vreg,	\
		 _vmask, _ereg, _emask)								\
	[_family##_##_id] = {									\
		.name		= (_match),							\
		.supply_name	= (_supply),							\
		.of_match	= of_match_ptr(_match),						\
		.regulators_node = of_match_ptr("regulators"),					\
		.type		= REGULATOR_VOLTAGE,						\
		.id		= _family##_##_id,						\
		.n_voltages	= (((_max) - (_min)) / (_step) + 1),				\
		.owner		= THIS_MODULE,							\
		.min_uV		= (_min) * 1000,						\
		.uV_step	= (_step) * 1000,						\
		.vsel_reg	= (_vreg),							\
		.vsel_mask	= (_vmask),							\
		.enable_reg	= (_ereg),							\
		.enable_mask	= (_emask),							\
		.ops		= &pmu_ext_ops_step_delay,					\
	}

#define PMU_EXT_REGULATOR_RANGE_VOL_DELAY(_family, _id, _match, _supply, _ranges, _n_voltages,	\
			_vreg, _vmask, _ereg, _emask)						\
	[_family##_##_id] = {									\
		.name		= (_match),							\
		.supply_name	= (_supply),							\
		.of_match	= of_match_ptr(_match),						\
		.regulators_node = of_match_ptr("regulators"),					\
		.type		= REGULATOR_VOLTAGE,						\
		.id		= _family##_##_id,						\
		.n_voltages	= (_n_voltages),						\
		.owner		= THIS_MODULE,							\
		.vsel_reg	= (_vreg),							\
		.vsel_mask	= (_vmask),							\
		.enable_reg	= (_ereg),							\
		.enable_mask	= (_emask),							\
		.linear_ranges	= (_ranges),							\
		.n_linear_ranges = ARRAY_SIZE(_ranges),						\
		.ops		= &pmu_ext_ops_range_vol_delay,					\
	}

#define PMU_EXT_REGULATOR_RANGE_STEP_DELAY(_family, _id, _match, _supply, _ranges, _n_voltages,	\
			_vreg, _vmask, _ereg, _emask)						\
	[_family##_##_id] = {									\
		.name		= (_match),							\
		.supply_name	= (_supply),							\
		.of_match	= of_match_ptr(_match),						\
		.regulators_node = of_match_ptr("regulators"),					\
		.type		= REGULATOR_VOLTAGE,						\
		.id		= _family##_##_id,						\
		.n_voltages	= (_n_voltages),						\
		.owner		= THIS_MODULE,							\
		.vsel_reg	= (_vreg),							\
		.vsel_mask	= (_vmask),							\
		.enable_reg	= (_ereg),							\
		.enable_mask	= (_emask),							\
		.linear_ranges	= (_ranges),							\
		.n_linear_ranges = ARRAY_SIZE(_ranges),						\
		.ops		= &pmu_ext_ops_range_step_delay,				\
	}

struct regulator_delay {
	u32 step;
	u32 final;
};

static int pmu_ext_set_voltage_time_sel(struct regulator_dev *rdev,
		unsigned int old_selector, unsigned int new_selector)
{
	struct regulator_delay *delay = (struct regulator_delay *)rdev->reg_data;
	int delay_time;

	delay_time = (abs(new_selector - old_selector) * delay->step + delay->final + 999) / 1000;

	return delay_time * 1000;
};

static struct regulator_ops pmu_ext_ops_step_delay = {
	.set_voltage_sel	= regulator_set_voltage_sel_regmap,
	.get_voltage_sel	= regulator_get_voltage_sel_regmap,
	.list_voltage		= regulator_list_voltage_linear,
	.enable			= regulator_enable_regmap,
	.disable		= regulator_disable_regmap,
	.is_enabled		= regulator_is_enabled_regmap,
	.set_voltage_time_sel	= pmu_ext_set_voltage_time_sel,
};

static struct regulator_ops pmu_ext_ops_range_step_delay = {
	.set_voltage_sel	= regulator_set_voltage_sel_regmap,
	.get_voltage_sel	= regulator_get_voltage_sel_regmap,
	.list_voltage		= regulator_list_voltage_linear_range,
	.enable			= regulator_enable_regmap,
	.disable		= regulator_disable_regmap,
	.is_enabled		= regulator_is_enabled_regmap,
	.set_voltage_time_sel	= pmu_ext_set_voltage_time_sel,
};

/* Operations permitted on DCDCx */
static struct regulator_ops pmu_ext_ops_range_vol_delay = {
	.is_enabled		= regulator_is_enabled_regmap,
	.enable			= regulator_enable_regmap,
	.disable		= regulator_disable_regmap,
	.get_voltage_sel	= regulator_get_voltage_sel_regmap,
	.set_voltage_sel	= regulator_set_voltage_sel_regmap,
	.list_voltage		= regulator_list_voltage_linear_range,
};

static const struct linear_range axp1530_dcdc1_ranges[] = {
	REGULATOR_LINEAR_RANGE(500000, 0x0, 0x46, 10000),
	REGULATOR_LINEAR_RANGE(1220000, 0x47, 0x57, 20000),
	REGULATOR_LINEAR_RANGE(1600000, 0x58, 0x6A, 100000),
};

static const struct linear_range axp1530_dcdc2_ranges[] = {
	REGULATOR_LINEAR_RANGE(500000, 0x0, 0x46, 10000),
	REGULATOR_LINEAR_RANGE(1220000, 0x47, 0x57, 20000),
};

static const struct linear_range axp1530_dcdc3_ranges[] = {
	REGULATOR_LINEAR_RANGE(500000, 0x0, 0x46, 10000),
	REGULATOR_LINEAR_RANGE(1220000, 0x47, 0x66, 20000),
};

static const struct regulator_desc axp1530_ext_regulators[] = {
	PMU_EXT_REGULATOR_RANGE_STEP_DELAY(AXP1530, DCDC1, "dcdc1", "vin1", axp1530_dcdc1_ranges,
			0x6B, AXP1530_DCDC1_CONRTOL, 0x7f, AXP1530_OUTPUT_CONTROL, BIT(0)),
	PMU_EXT_REGULATOR_RANGE_STEP_DELAY(AXP1530, DCDC2, "dcdc2", "vin2", axp1530_dcdc2_ranges,
			0x58, AXP1530_DCDC2_CONRTOL, 0x7f, AXP1530_OUTPUT_CONTROL, BIT(1)),
	PMU_EXT_REGULATOR_RANGE_STEP_DELAY(AXP1530, DCDC3, "dcdc3", "vin3", axp1530_dcdc3_ranges,
			0x58, AXP1530_DCDC3_CONRTOL, 0x7f, AXP1530_OUTPUT_CONTROL, BIT(2)),
	PMU_EXT_REGULATOR_STEP_DELAY(AXP1530, LDO1, "ldo1", "ldo1in", 500, 3500, 100,
		AXP1530_ALDO1_CONRTOL, 0x1f, AXP1530_OUTPUT_CONTROL, BIT(3)),
	PMU_EXT_REGULATOR_STEP_DELAY(AXP1530, LDO2, "ldo2", "ldo2in", 500, 3500, 100,
		AXP1530_DLDO1_CONRTOL, 0x1f, AXP1530_OUTPUT_CONTROL, BIT(4)),
};

static const struct linear_range tcs4838_dcdc_ranges[] = {
	REGULATOR_LINEAR_RANGE(712500, 0x0, 0x3F, 12500),
};

static const struct regulator_desc tcs4838_regulators[] = {
	PMU_EXT_REGULATOR_RANGE_VOL_DELAY(TCS4838, DCDC0, "dcdc0", "vin1", tcs4838_dcdc_ranges,
			0x40, TCS4838_VSEL0, GENMASK(5, 0), TCS4838_VSEL0, BIT(7)),
	PMU_EXT_REGULATOR_RANGE_VOL_DELAY(TCS4838, DCDC1, "dcdc1", "vin1", tcs4838_dcdc_ranges,
			0x40, TCS4838_VSEL1, GENMASK(5, 0), TCS4838_VSEL1, BIT(7)),
};

static const struct linear_range sy8827g_dcdc_ranges[] = {
	REGULATOR_LINEAR_RANGE(600000, 0x0, 0x3F, 12500),
};

static const struct regulator_desc sy8827g_regulators[] = {
	PMU_EXT_REGULATOR_RANGE_VOL_DELAY(SY8827G, DCDC0, "dcdc0", "vin1", sy8827g_dcdc_ranges,
			0x40, SY8827G_VSEL0, GENMASK(5, 0), SY8827G_VSEL0, BIT(7)),
	PMU_EXT_REGULATOR_RANGE_VOL_DELAY(SY8827G, DCDC1, "dcdc1", "vin1", sy8827g_dcdc_ranges,
			0x40, SY8827G_VSEL1, GENMASK(5, 0), SY8827G_VSEL1, BIT(7)),
};

static int pmu_ext_regulator_probe(struct platform_device *pdev)
{
	struct regulator_dev *rdev;
	struct regulator_delay *rdev_delay;
	struct pmu_ext_dev *ext = dev_get_drvdata(pdev->dev.parent);
	const struct regulator_desc *regulators;
	struct regulator_config config = {
		.dev = pdev->dev.parent,
		.regmap = ext->regmap,
		.driver_data = ext,
	};
	int i, nregulators;
	u32 dval;

	switch (ext->variant) {
	case AXP1530_ID:
		regulators = axp1530_ext_regulators;
		nregulators = AXP1530_EXT_REG_ID_MAX;
		break;
	case TCS4838_ID:
		regulators = tcs4838_regulators;
		nregulators = TCS4838_REG_ID_MAX;
		break;
	case SY8827G_ID:
		regulators = sy8827g_regulators;
		nregulators = SY8827G_REG_ID_MAX;
		break;
	default:
		dev_err(&pdev->dev, "Unsupported pmu_ext variant: %ld\n",
			ext->variant);
		return -EINVAL;
	}


	for (i = 0; i < nregulators; i++) {
		const struct regulator_desc *desc = &regulators[i];
		rdev = devm_regulator_register(&pdev->dev, desc, &config);
		if (IS_ERR(rdev)) {
			dev_err(&pdev->dev, "Failed to register %s\n",
				regulators[i].name);

			return PTR_ERR(rdev);
		}


		rdev_delay = devm_kzalloc(&pdev->dev, sizeof(*rdev_delay), GFP_KERNEL);
		if (!of_property_read_u32(rdev->dev.of_node,
			"regulator-step-delay-us", &dval))
			rdev_delay->step = dval;
		else
			rdev_delay->step = 0;

		if (!of_property_read_u32(rdev->dev.of_node,
			"regulator-final-delay-us", &dval))
			rdev_delay->final = dval;
		else
			rdev_delay->final = 0;

		rdev->reg_data = rdev_delay;

	}

	return 0;
}

static int pmu_ext_regulator_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver pmu_ext_regulator_driver = {
	.driver = {
		.name = "pmu-ext-regulator",
	},
	.probe = pmu_ext_regulator_probe,
	.remove	= pmu_ext_regulator_remove,
};

static int __init pmu_ext_regulator_init(void)
{
	return platform_driver_register(&pmu_ext_regulator_driver);
}

static void __exit pmu_ext_regulator_exit(void)
{
	platform_driver_unregister(&pmu_ext_regulator_driver);
}

subsys_initcall(pmu_ext_regulator_init);
module_exit(pmu_ext_regulator_exit);

MODULE_AUTHOR("Andrew F. Davis <afd@ti.com>");
MODULE_DESCRIPTION("pmu_ext voltage regulator driver");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("1.0.0");
