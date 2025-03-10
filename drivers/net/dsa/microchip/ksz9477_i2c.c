// SPDX-License-Identifier: GPL-2.0
/*
 * Microchip KSZ9477 series register access through I2C
 *
 * Copyright (C) 2018-2024 Microchip Technology Inc.
 */

#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/regmap.h>

#include "ksz_common.h"

KSZ_REGMAP_TABLE(ksz9477, not_used, 16, 0, 0);

static int ksz9477_i2c_probe(struct i2c_client *i2c)
{
	const struct ksz_chip_data *chip;
	struct device *ddev = &i2c->dev;
	struct regmap_config rc;
	struct ksz_device *dev;
	int i, ret;

	dev = ksz_switch_alloc(&i2c->dev, i2c);
	if (!dev)
		return -ENOMEM;

	chip = device_get_match_data(ddev);
	if (!chip)
		return -EINVAL;

	/* Save chip id to do special initialization when probing. */
	dev->chip_id = chip->chip_id;
	for (i = 0; i < __KSZ_NUM_REGMAPS; i++) {
		rc = ksz9477_regmap_config[i];
		rc.lock_arg = &dev->regmap_mutex;
		dev->regmap[i] = devm_regmap_init_i2c(i2c, &rc);
		if (IS_ERR(dev->regmap[i])) {
			return dev_err_probe(&i2c->dev, PTR_ERR(dev->regmap[i]),
					     "Failed to initialize regmap%i\n",
					     ksz9477_regmap_config[i].val_bits);
		}
	}

	if (i2c->dev.platform_data)
		dev->pdata = i2c->dev.platform_data;

	dev->irq = i2c->irq;

	ret = ksz_switch_register(dev);

	/* Main DSA driver may not be started yet. */
	if (ret)
		return ret;

	i2c_set_clientdata(i2c, dev);

	return 0;
}

static void ksz9477_i2c_remove(struct i2c_client *i2c)
{
	struct ksz_device *dev = i2c_get_clientdata(i2c);

	if (dev)
		ksz_switch_remove(dev);
}

static void ksz9477_i2c_shutdown(struct i2c_client *i2c)
{
	struct ksz_device *dev = i2c_get_clientdata(i2c);

	if (!dev)
		return;

	ksz_switch_shutdown(dev);

	i2c_set_clientdata(i2c, NULL);
}

static const struct i2c_device_id ksz9477_i2c_id[] = {
	{ "ksz9477-switch" },
	{}
};

MODULE_DEVICE_TABLE(i2c, ksz9477_i2c_id);

static const struct of_device_id ksz9477_dt_ids[] = {
	{
		.compatible = "microchip,ksz9477",
		.data = &ksz_switch_chips[KSZ9477]
	},
	{
		.compatible = "microchip,ksz9896",
		.data = &ksz_switch_chips[KSZ9896]
	},
	{
		.compatible = "microchip,ksz9897",
		.data = &ksz_switch_chips[KSZ9897]
	},
	{
		.compatible = "microchip,ksz9893",
		.data = &ksz_switch_chips[KSZ9893]
	},
	{
		.compatible = "microchip,ksz9563",
		.data = &ksz_switch_chips[KSZ9563]
	},
	{
		.compatible = "microchip,ksz8563",
		.data = &ksz_switch_chips[KSZ8563]
	},
	{
		.compatible = "microchip,ksz8567",
		.data = &ksz_switch_chips[KSZ8567]
	},
	{
		.compatible = "microchip,ksz9567",
		.data = &ksz_switch_chips[KSZ9567]
	},
	{
		.compatible = "microchip,lan9646",
		.data = &ksz_switch_chips[LAN9646]
	},
	{},
};
MODULE_DEVICE_TABLE(of, ksz9477_dt_ids);

static DEFINE_SIMPLE_DEV_PM_OPS(ksz_i2c_pm_ops,
				ksz_switch_suspend, ksz_switch_resume);

static struct i2c_driver ksz9477_i2c_driver = {
	.driver = {
		.name	= "ksz9477-switch",
		.of_match_table = ksz9477_dt_ids,
		.pm = &ksz_i2c_pm_ops,
	},
	.probe = ksz9477_i2c_probe,
	.remove	= ksz9477_i2c_remove,
	.shutdown = ksz9477_i2c_shutdown,
	.id_table = ksz9477_i2c_id,
};

module_i2c_driver(ksz9477_i2c_driver);

MODULE_AUTHOR("Tristram Ha <Tristram.Ha@microchip.com>");
MODULE_DESCRIPTION("Microchip KSZ9477 Series Switch I2C access Driver");
MODULE_LICENSE("GPL v2");
