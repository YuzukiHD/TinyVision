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
#include <linux/mfd/pmu-ext.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>

#define PMU_EXT_REGULATOR(_family, _id, _match, _supply, _ranges, _n_voltages,	\
			_vreg, _vmask, _ereg, _emask)				\
	[_family##_##_id] = {							\
		.name		= (_match),					\
		.supply_name	= (_supply),					\
		.of_match	= of_match_ptr(_match),				\
		.regulators_node = of_match_ptr("regulators"),			\
		.type		= REGULATOR_VOLTAGE,				\
		.id		= _family##_##_id,				\
		.n_voltages	= (_n_voltages),				\
		.owner		= THIS_MODULE,					\
		.vsel_reg	= (_vreg),					\
		.vsel_mask	= (_vmask),					\
		.enable_reg	= (_ereg),					\
		.enable_mask	= (_emask),					\
		.linear_ranges	= (_ranges),					\
		.n_linear_ranges = ARRAY_SIZE(_ranges),				\
		.ops		= &pmu_ext_ops_dcdc,			\
	}

/* Operations permitted on DCDCx */
static struct regulator_ops pmu_ext_ops_dcdc = {
	.is_enabled		= regulator_is_enabled_regmap,
	.enable			= regulator_enable_regmap,
	.disable		= regulator_disable_regmap,
	.get_voltage_sel	= regulator_get_voltage_sel_regmap,
	.set_voltage_sel	= regulator_set_voltage_sel_regmap,
	.list_voltage		= regulator_list_voltage_linear_range,
};

static const struct regulator_linear_range tcs4838_dcdc_ranges[] = {
	REGULATOR_LINEAR_RANGE(712500, 0x0, 0x3F, 12500),
};

static const struct regulator_desc tcs4838_regulators[] = {
	PMU_EXT_REGULATOR(TCS4838, DCDC0, "dcdc0", "vin1", tcs4838_dcdc_ranges,
			0x40, TCS4838_VSEL0, GENMASK(5, 0), TCS4838_VSEL0, BIT(7)),
	PMU_EXT_REGULATOR(TCS4838, DCDC1, "dcdc1", "vin1", tcs4838_dcdc_ranges,
			0x40, TCS4838_VSEL1, GENMASK(5, 0), TCS4838_VSEL1, BIT(7)),
};

static const struct regulator_linear_range sy8827g_dcdc_ranges[] = {
	REGULATOR_LINEAR_RANGE(600000, 0x0, 0x3F, 12500),
};

static const struct regulator_desc sy8827g_regulators[] = {
	PMU_EXT_REGULATOR(SY8827G, DCDC0, "dcdc0", "vin1", sy8827g_dcdc_ranges,
			0x40, SY8827G_VSEL0, GENMASK(5, 0), SY8827G_VSEL0, BIT(7)),
	PMU_EXT_REGULATOR(SY8827G, DCDC1, "dcdc1", "vin1", sy8827g_dcdc_ranges,
			0x40, SY8827G_VSEL1, GENMASK(5, 0), SY8827G_VSEL1, BIT(7)),
};

static int pmu_ext_regulator_probe(struct platform_device *pdev)
{
	struct regulator_dev *rdev;
	struct pmu_ext_dev *ext = dev_get_drvdata(pdev->dev.parent);
	const struct regulator_desc *regulators;
	struct regulator_config config = {
		.dev = pdev->dev.parent,
		.regmap = ext->regmap,
		.driver_data = ext,
	};
	int i, nregulators;

	switch (ext->variant) {
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
