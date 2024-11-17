// SPDX-License-Identifier: GPL-2.0

#include <linux/bitfield.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/extcon-provider.h>
#include <linux/gpio/consumer.h>
#include <linux/usb/role.h>

#define WUSB3801_REG_DEVICE_ID		0x01
#define WUSB3801_REG_CTRL0		0x02
#define WUSB3801_REG_INT		0x03
#define WUSB3801_REG_STAT		0x04

#define WUSB3801_DEVICE_ID_VERSION_ID	GENMASK(7, 3)
#define WUSB3801_DEVICE_ID_VENDOR_ID	GENMASK(2, 0)

#define WUSB3801_CTRL0_DIS_ACC_SUPPORT	BIT(7)
#define WUSB3801_CTRL0_TRY		GENMASK(6, 5)
#define WUSB3801_CTRL0_TRY_NONE		(0x0 << 5)
#define WUSB3801_CTRL0_TRY_SNK		(0x1 << 5)
#define WUSB3801_CTRL0_TRY_SRC		(0x2 << 5)
#define WUSB3801_CTRL0_CURRENT		GENMASK(4, 3) /* SRC */
#define WUSB3801_CTRL0_CURRENT_DEFAULT	(0x0 << 3)
#define WUSB3801_CTRL0_CURRENT_1_5A	(0x1 << 3)
#define WUSB3801_CTRL0_CURRENT_3_0A	(0x2 << 3)
#define WUSB3801_CTRL0_ROLE		GENMASK(2, 1)
#define WUSB3801_CTRL0_ROLE_SNK		(0x0 << 1)
#define WUSB3801_CTRL0_ROLE_SRC		(0x1 << 1)
#define WUSB3801_CTRL0_ROLE_DRP		(0x2 << 1)
#define WUSB3801_CTRL0_INT_MASK		BIT(0)

#define WUSB3801_INT_ATTACHED		BIT(0)
#define WUSB3801_INT_DETACHED		BIT(1)

#define WUSB3801_STAT_VBUS_DETECTED	BIT(7)
#define WUSB3801_STAT_CURRENT		GENMASK(6, 5) /* SNK */
#define WUSB3801_STAT_CURRENT_STANDBY	(0x0 << 5)
#define WUSB3801_STAT_CURRENT_DEFAULT	(0x1 << 5)
#define WUSB3801_STAT_CURRENT_1_5A	(0x2 << 5)
#define WUSB3801_STAT_CURRENT_3_0A	(0x3 << 5)
#define WUSB3801_STAT_PARTNER		GENMASK(4, 2)
#define WUSB3801_STAT_PARTNER_STANDBY	(0x0 << 2)
#define WUSB3801_STAT_PARTNER_SNK	(0x1 << 2)
#define WUSB3801_STAT_PARTNER_SRC	(0x2 << 2)
#define WUSB3801_STAT_PARTNER_AUDIO	(0x3 << 2)
#define WUSB3801_STAT_PARTNER_DEBUG	(0x4 << 2)
#define WUSB3801_STAT_ORIENTATION	GENMASK(1, 0)
#define WUSB3801_STAT_ORIENTATION_NONE	(0x0 << 0)
#define WUSB3801_STAT_ORIENTATION_CC1	(0x1 << 0)
#define WUSB3801_STAT_ORIENTATION_CC2	(0x2 << 0)
#define WUSB3801_STAT_ORIENTATION_BOTH	(0x3 << 0)

struct wusb3801 {
	struct device *dev;
	struct gpio_desc *id_gpiod;
	struct gpio_desc *vbus_gpiod;
	struct gpio_desc *sel_gpiod;
	struct usb_role_switch *role_sw;
	struct extcon_dev *edev;
	struct regmap *regmap;
	struct work_struct irq_work;
	int vbus_val;
	int sel_val;
};

static void wusb3801_hw_update(struct wusb3801 *wusb3801)
{
	struct device *dev = wusb3801->dev;
	unsigned int status;
	int usb_id;
	int ret;

	ret = regmap_read(wusb3801->regmap, WUSB3801_REG_STAT, &status);
	if (ret) {
		dev_err(dev, "Failed to read port status: %d\n", ret);
		return;
	}
	dev_info(dev, "status = 0x%02x\n", status);

	usb_id = gpiod_get_value_cansleep(wusb3801->id_gpiod);
	dev_info(dev, "usb_id = %d\n", usb_id);

	if ((status & WUSB3801_STAT_ORIENTATION) == WUSB3801_STAT_ORIENTATION_CC2) {
		if (!wusb3801->sel_val) {
			wusb3801->sel_val = 1;
			gpiod_set_value_cansleep(wusb3801->sel_gpiod, 1);
			dev_info(dev, "sel on\n");
		}
	} else {
		if (wusb3801->sel_val) {
			wusb3801->sel_val = 0;
			gpiod_set_value_cansleep(wusb3801->sel_gpiod, 0);
			dev_info(dev, "sel off\n");
		}
	}

	if ((status & WUSB3801_STAT_PARTNER) == WUSB3801_STAT_PARTNER_SNK) {
		if (!wusb3801->vbus_val) {
			wusb3801->vbus_val = 1;
			if (wusb3801->role_sw)
				usb_role_switch_set_role(wusb3801->role_sw, USB_ROLE_HOST);
			gpiod_set_value_cansleep(wusb3801->vbus_gpiod, 1);
			extcon_set_state_sync(wusb3801->edev, EXTCON_USB, false);
			extcon_set_state_sync(wusb3801->edev, EXTCON_USB_HOST, true);
			dev_info(dev, "vbus on\n");
		}
	} else {
		if (wusb3801->vbus_val) {
			wusb3801->vbus_val = 0;
			if (wusb3801->role_sw)
				usb_role_switch_set_role(wusb3801->role_sw, USB_ROLE_DEVICE);
			gpiod_set_value_cansleep(wusb3801->vbus_gpiod, 0);
			extcon_set_state_sync(wusb3801->edev, EXTCON_USB_HOST, false);
			extcon_set_state_sync(wusb3801->edev, EXTCON_USB, true);
			dev_info(dev, "vbus off\n");
		}
	}
}

static void wusb3801_irq_work(struct work_struct *work)
{
	struct wusb3801 *wusb3801 = container_of(work,
			struct wusb3801, irq_work);

	wusb3801_hw_update(wusb3801);
}

static irqreturn_t wusb3801_irq(int irq, void *data)
{
	struct wusb3801 *wusb3801 = data;
	unsigned int dummy;

	regmap_read(wusb3801->regmap, WUSB3801_REG_INT, &dummy);
	schedule_work(&wusb3801->irq_work);

	return IRQ_HANDLED;
}

static int wusb3801_hw_init(struct wusb3801 *wusb3801)
{
	unsigned int val;
	int ret;

	regmap_read(wusb3801->regmap, WUSB3801_REG_DEVICE_ID, &val);
	dev_info(wusb3801->dev, "device_id = 0x%02x\n", val);

	val = WUSB3801_CTRL0_TRY_SRC |
		WUSB3801_CTRL0_CURRENT_DEFAULT |
		WUSB3801_CTRL0_ROLE_DRP;
	ret = regmap_write(wusb3801->regmap, WUSB3801_REG_CTRL0, val);
	dev_info(wusb3801->dev, "%s: 0x%02x, ret=%d\n",
		__func__, val, ret);

	return ((ret < 0) ? -1 : 0);
}

static const unsigned int extcon_cable[] = {
	EXTCON_USB,
	EXTCON_USB_HOST,
	EXTCON_NONE,
};

static const struct regmap_config config = {
	.reg_bits	= 8,
	.val_bits	= 8,
	.max_register	= WUSB3801_REG_STAT,
};

static int wusb3801_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct wusb3801 *wusb3801;
	int ret;

	wusb3801 = devm_kzalloc(dev, sizeof(*wusb3801), GFP_KERNEL);
	if (!wusb3801) {
		dev_err(dev, "failed to allocate memory\n");
		return -ENOMEM;
	}

	i2c_set_clientdata(client, wusb3801);

	wusb3801->dev = dev;

	wusb3801->id_gpiod = devm_gpiod_get(dev, "id", GPIOD_IN);
	if (IS_ERR(wusb3801->id_gpiod)) {
		return dev_err_probe(dev, PTR_ERR(wusb3801->id_gpiod),
					"failed to get id-gpios\n");
	}
	wusb3801->vbus_gpiod = devm_gpiod_get(dev, "vbus", GPIOD_OUT_LOW);
	if (IS_ERR(wusb3801->vbus_gpiod)) {
		return dev_err_probe(dev, PTR_ERR(wusb3801->vbus_gpiod),
					"failed to get vbus-gpios\n");
	}
	wusb3801->sel_gpiod = devm_gpiod_get(dev, "sel", GPIOD_OUT_LOW);
	if (IS_ERR(wusb3801->sel_gpiod)) {
		return dev_err_probe(dev, PTR_ERR(wusb3801->sel_gpiod),
					"failed to get sel-gpios\n");
	}

	wusb3801->role_sw = usb_role_switch_get(dev);
	if (IS_ERR(wusb3801->role_sw)) {
		dev_warn(dev, "failed to get role switch, ignore\n");
		wusb3801->role_sw = NULL;
	}

	wusb3801->edev = devm_extcon_dev_allocate(dev, extcon_cable);
	if (IS_ERR(wusb3801->edev)) {
		dev_err(dev, "failed to allocate extcon device\n");
		return -ENOMEM;
	}
	ret = devm_extcon_dev_register(dev, wusb3801->edev);
	if (ret < 0) {
		dev_err(dev, "failed to register extcon device\n");
		return -1;
	}

	wusb3801->regmap = devm_regmap_init_i2c(client, &config);
	if (IS_ERR(wusb3801->regmap)) {
		return dev_err_probe(dev, PTR_ERR(wusb3801->regmap),
					"failed to allocate regmap\n");
	}
	ret = wusb3801_hw_init(wusb3801);
	if (ret) {
		return -1;
	}
	wusb3801_hw_update(wusb3801);

	INIT_WORK(&wusb3801->irq_work, wusb3801_irq_work);
	ret = request_threaded_irq(client->irq, NULL, wusb3801_irq,
					IRQF_ONESHOT, dev_name(dev), wusb3801);
	if (ret) {
		dev_err(dev, "failed to request irq\n");
		return -1;
	}

	dev_info(dev, "%s: done\n", __func__);

	return 0;
}

static void wusb3801_remove(struct i2c_client *client)
{
	struct wusb3801 *wusb3801 = i2c_get_clientdata(client);

	free_irq(client->irq, wusb3801);
	cancel_work_sync(&wusb3801->irq_work);

	gpiod_set_value_cansleep(wusb3801->vbus_gpiod, 0);

	if (wusb3801->role_sw) {
		usb_role_switch_set_role(wusb3801->role_sw, USB_ROLE_DEVICE);
		usb_role_switch_put(wusb3801->role_sw);
	}
	extcon_set_state_sync(wusb3801->edev, EXTCON_USB_HOST, false);
	extcon_set_state_sync(wusb3801->edev, EXTCON_USB, true);

	gpiod_set_value_cansleep(wusb3801->sel_gpiod, 0);
}

static const struct of_device_id wusb3801_of_match[] = {
	{ .compatible = "extcon,wusb3801" },
	{}
};
MODULE_DEVICE_TABLE(of, wusb3801_of_match);

static struct i2c_driver wusb3801_driver = {
	.probe_new	= wusb3801_probe,
	.remove		= wusb3801_remove,
	.driver		= {
		.name		= "wusb3801",
		.of_match_table	= wusb3801_of_match,
	},
};

// module_i2c_driver(wusb3801_driver);
static int __init wusb3801_driver_init(void)
{
	return i2c_add_driver(&wusb3801_driver);
}
late_initcall(wusb3801_driver_init);
static void __exit wusb3801_driver_exit(void)
{
	i2c_del_driver(&wusb3801_driver);
}
module_exit(wusb3801_driver_exit);

MODULE_LICENSE("GPL v2");
