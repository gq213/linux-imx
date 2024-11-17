// SPDX-License-Identifier: GPL-2.0

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/regmap.h>
#include <linux/err.h>
#include <linux/kthread.h>

/*
 * lt8912b have three i2c module inside.
 * lt8912b i2c address: [0x48, 0x49, 0x4a]
 */

#define I2C_1ST		0
#define I2C_2ND		1
#define I2C_3RD		2

struct lt8912 {
	struct device *dev;
	struct gpio_desc *reset_gpiod;
	struct regmap *regmap[3];
	struct task_struct *thd;
};

#include "panel-lt8912b-reg.c"

static const struct regmap_config config = {
	.reg_bits	= 8,
	.val_bits	= 8,
	.max_register	= 0xff,
};

static int lt8912_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	static struct lt8912 *lt;
	struct i2c_client *dummy;
	int ret;

	lt = devm_kzalloc(dev, sizeof(*lt), GFP_KERNEL);
	if (!lt) {
		dev_err(dev, "failed to allocate memory\n");
		return -ENOMEM;
	}

	i2c_set_clientdata(client, lt);

	lt->dev = dev;

	lt->reset_gpiod = devm_gpiod_get(dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(lt->reset_gpiod)) {
		return dev_err_probe(dev, PTR_ERR(lt->reset_gpiod),
					"failed to get reset-gpios\n");
	}

	lt->regmap[0] = devm_regmap_init_i2c(client, &config);
	if (IS_ERR(lt->regmap[0])) {
		return dev_err_probe(dev, PTR_ERR(lt->regmap[0]),
					"failed to allocate regmap[0]\n");
	}
	
	dummy = i2c_new_dummy_device(client->adapter, 0x49);
	if (IS_ERR(dummy)) {
		return dev_err_probe(dev, PTR_ERR(dummy),
					"failed to allocate dummy_49\n");
	}
	lt->regmap[1] = devm_regmap_init_i2c(dummy, &config);
	if (IS_ERR(lt->regmap[1])) {
		return dev_err_probe(dev, PTR_ERR(lt->regmap[1]),
					"failed to allocate regmap[1]\n");
	}
	
	dummy = i2c_new_dummy_device(client->adapter, 0x4a);
	if (IS_ERR(dummy)) {
		return dev_err_probe(dev, PTR_ERR(dummy),
					"failed to allocate dummy_4a\n");
	}
	lt->regmap[2] = devm_regmap_init_i2c(dummy, &config);
	if (IS_ERR(lt->regmap[2])) {
		return dev_err_probe(dev, PTR_ERR(lt->regmap[2]),
					"failed to allocate regmap[2]\n");
	}
	
	ret = lt8912_hw_init(lt);
	if (ret) {
		return -1;
	}

	dev_info(dev, "%s: done\n", __func__);

	return 0;
}

static void lt8912_remove(struct i2c_client *client)
{
	struct lt8912 *lt = i2c_get_clientdata(client);

	gpiod_set_value_cansleep(lt->reset_gpiod, 0);
}

static const struct of_device_id lt8912_dt_match[] = {
	{.compatible = "lontium,lt8912b"},
	{}
};
MODULE_DEVICE_TABLE(of, lt8912_dt_match);

static struct i2c_driver lt8912_i2c_driver = {
	.driver = {
		.name = "lt8912",
		.of_match_table = lt8912_dt_match,
	},
	.probe_new = lt8912_probe,
	.remove = lt8912_remove,
};

module_i2c_driver(lt8912_i2c_driver);

MODULE_LICENSE("GPL v2");
