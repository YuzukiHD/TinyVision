/*
 * Based on the tcs4838 driver and the previous tcs4838 driver
 */

#include <linux/interrupt.h>
#include <linux/mfd/core.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/acpi.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/err.h>

#include <linux/mfd/pmu-ext.h>

static const char *const pmu_ext_model_names[] = {
	"TCS4838", "SY8827G",
};

#define PMU_EXT_DCDC0 "dcdc0"
#define PMU_EXT_DCDC1 "dcdc1"

static struct mfd_cell tcs4838_cells[] = {
	{ .name = "pmu-ext-regulator", },
	{
		.of_compatible = "xpower-vregulator,ext-dcdc0",
		.name = "reg-virt-consumer",
		.id = 100,
		.platform_data = PMU_EXT_DCDC0,
		.pdata_size = sizeof(PMU_EXT_DCDC0),

	},
	{
		.of_compatible = "xpower-vregulator,ext-dcdc1",
		.name = "reg-virt-consumer",
		.id = 101,
		.platform_data = PMU_EXT_DCDC1,
		.pdata_size = sizeof(PMU_EXT_DCDC1),

	},
};

static struct mfd_cell sy8827g_cells[] = {
	{ .name = "pmu-ext-regulator", },
	{
		.of_compatible = "xpower-vregulator,ext-dcdc0",
		.name = "reg-virt-consumer",
		.id = 100,
		.platform_data = PMU_EXT_DCDC0,
		.pdata_size = sizeof(PMU_EXT_DCDC0),

	},
	{
		.of_compatible = "xpower-vregulator,ext-dcdc1",
		.name = "reg-virt-consumer",
		.id = 101,
		.platform_data = PMU_EXT_DCDC1,
		.pdata_size = sizeof(PMU_EXT_DCDC1),

	},
};

static const struct regmap_range tcs4838_yes_ranges[] = {
	regmap_reg_range(TCS4838_VSEL0, TCS4838_PGOOD),
};

static const struct regmap_range sy8827g_yes_ranges[] = {
	regmap_reg_range(SY8827G_VSEL0, SY8827G_PGOOD),
};

static const struct regmap_access_table tcs4838_volatile_table = {
	.yes_ranges = tcs4838_yes_ranges,
	.n_yes_ranges = ARRAY_SIZE(tcs4838_yes_ranges),
};

static const struct regmap_access_table sy8827g_volatile_table = {
	.yes_ranges = sy8827g_yes_ranges,
	.n_yes_ranges = ARRAY_SIZE(sy8827g_yes_ranges),
};

const struct regmap_config tcs4838_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.volatile_table = &tcs4838_volatile_table,
	.max_register   = TCS4838_PGOOD,
	.cache_type     = REGCACHE_NONE,
};

const struct regmap_config sy8827g_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.volatile_table = &sy8827g_volatile_table,
	.max_register   = SY8827G_PGOOD,
	.cache_type     = REGCACHE_NONE,
};

static void tcs4838_dts_parse(struct pmu_ext_dev *ext)
{
	struct device_node *node = ext->dev->of_node;
	struct regmap *map = ext->regmap;
	u32 val;

	/* init powerok reset function */
	if (of_property_read_u32(node, "tcs4838_delay", &val))
		val = 0;
	if (val) {
		val = val << 4;
		regmap_update_bits(map, TCS4838_CTRL, GENMASK(6, 4), val);
	}
}

static void sy8827g_dts_parse(struct pmu_ext_dev *ext)
{
	struct device_node *node = ext->dev->of_node;
	struct regmap *map = ext->regmap;
	u32 val;

	/* init powerok reset function */
	if (of_property_read_u32(node, "sy8827g_delay", &val))
		val = 0;
	if (val) {
		val = val << 4;
		regmap_update_bits(map, SY8827G_CTRL, GENMASK(6, 4), val);
	}
}

int pmu_ext_match_device(struct pmu_ext_dev *ext)
{
	struct device *dev = ext->dev;
	const struct acpi_device_id *acpi_id;
	const struct of_device_id *of_id;

	if (dev->of_node) {
		of_id = of_match_device(dev->driver->of_match_table, dev);
		if (!of_id) {
			dev_err(dev, "Unable to match OF ID\n");
			return -ENODEV;
		}
		ext->variant = (long)of_id->data;
	} else {
		acpi_id = acpi_match_device(dev->driver->acpi_match_table, dev);
		if (!acpi_id || !acpi_id->driver_data) {
			dev_err(dev, "Unable to match ACPI ID and data\n");
			return -ENODEV;
		}
		ext->variant = (long)acpi_id->driver_data;
	}

	switch (ext->variant) {
/**************************************/
	case TCS4838_ID:
		ext->nr_cells = ARRAY_SIZE(tcs4838_cells);
		ext->cells = tcs4838_cells;
		ext->regmap_cfg = &tcs4838_regmap_config;
		ext->dts_parse = tcs4838_dts_parse;
		break;
/**************************************/
	case SY8827G_ID:
		ext->nr_cells = ARRAY_SIZE(sy8827g_cells);
		ext->cells = sy8827g_cells;
		ext->regmap_cfg = &sy8827g_regmap_config;
		ext->dts_parse = sy8827g_dts_parse;
		break;
/*-------------------*/
	default:
		dev_err(dev, "unsupported ext ID %lu\n", ext->variant);
		return -EINVAL;
	}
	dev_info(dev, "pmu_ext_dev variant %s found\n",
		 pmu_ext_model_names[ext->variant]);

	return 0;
}
EXPORT_SYMBOL(pmu_ext_match_device);

int pmu_ext_device_init(struct pmu_ext_dev *ext)
{
	int ret;
	if (ext->dts_parse)
		ext->dts_parse(ext);

	ret = mfd_add_devices(ext->dev, 0, ext->cells,
			      ext->nr_cells, NULL, 0, NULL);

	dev_info(ext->dev, "pmu_ext:%s driver loaded\n", pmu_ext_model_names[ext->variant]);

	return 0;
}
EXPORT_SYMBOL_GPL(pmu_ext_device_init);

int pmu_ext_device_exit(struct pmu_ext_dev *ext)
{
	mfd_remove_devices(ext->dev);
	return 0;
}
EXPORT_SYMBOL_GPL(pmu_ext_device_exit);

MODULE_AUTHOR("Andrew F. Davis <afd@ti.com>");
MODULE_DESCRIPTION("pmu_ext MFD Driver");
MODULE_LICENSE("GPL v2");
