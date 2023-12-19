/*
 * Based on the TPS65218 driver and the previous TPS65912 driver
 */

#include <linux/acpi.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/mfd/pmu-ext.h>
#include <linux/of.h>
#include <linux/regmap.h>
#include <linux/slab.h>


static const struct of_device_id pmu_ext_i2c_of_match_table[] = {
	{ .compatible = "ext,tcs4838", .data = (void *)TCS4838_ID },
	{ .compatible = "ext,sy8827g", .data = (void *)SY8827G_ID },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, pmu_ext_i2c_of_match_table);

static int pmu_ext_i2c_probe(struct i2c_client *client,
			      const struct i2c_device_id *ids)
{
	struct pmu_ext_dev *ext;
	int ret;

	ext = devm_kzalloc(&client->dev, sizeof(*ext), GFP_KERNEL);
	if (!ext)
		return -ENOMEM;

	ext->dev = &client->dev;
	dev_set_drvdata(ext->dev, ext);

	ret = pmu_ext_match_device(ext);
	if (ret)
		return ret;

	ext->regmap = devm_regmap_init_i2c(client, ext->regmap_cfg);
	if (IS_ERR(ext->regmap)) {
		dev_err(ext->dev, "Failed to initialize register map\n");
		return PTR_ERR(ext->regmap);
	}

	return pmu_ext_device_init(ext);
}

static int pmu_ext_i2c_remove(struct i2c_client *client)
{
	struct pmu_ext_dev *ext = i2c_get_clientdata(client);

	return pmu_ext_device_exit(ext);
}

static const struct i2c_device_id pmu_ext_i2c_id_table[] = {
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(i2c, pmu_ext_i2c_id_table);

static const struct acpi_device_id pmu_ext_i2c_acpi_match[] = {
	{ },
};
MODULE_DEVICE_TABLE(acpi, pmu_ext_i2c_acpi_match);

static struct i2c_driver pmu_ext_i2c_driver = {
	.driver		= {
		.name	= "pmu-ext-i2c",
		.of_match_table = of_match_ptr(pmu_ext_i2c_of_match_table),
		.acpi_match_table = ACPI_PTR(pmu_ext_i2c_acpi_match),
	},
	.probe		= pmu_ext_i2c_probe,
	.remove		= pmu_ext_i2c_remove,
	.id_table       = pmu_ext_i2c_id_table,
};

static int __init pmu_ext_i2c_init(void)
{
	int ret;

	ret = i2c_add_driver(&pmu_ext_i2c_driver);
	if (ret != 0) {
		pr_err("pmu_ext i2c registration failed %d\n", ret);
		return ret;
	}

	return 0;
}
subsys_initcall(pmu_ext_i2c_init);

static void __exit pmu_ext_i2c_exit(void)
{
	i2c_del_driver(&pmu_ext_i2c_driver);
}
module_exit(pmu_ext_i2c_exit);

MODULE_AUTHOR("Andrew F. Davis <afd@ti.com>");
MODULE_DESCRIPTION("pmu_ext I2C Interface Driver");
MODULE_LICENSE("GPL v2");
