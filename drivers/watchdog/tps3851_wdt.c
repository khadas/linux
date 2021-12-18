//#define DEBUG

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/module.h>
#include <linux/device.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/fs.h>
#include <linux/timer.h>
#include <linux/jiffies.h>

static struct timer_list mytimer;
static unsigned int hw_margin = 3;
static int tps3851_input_pin;
static unsigned int tps3851_enble = 1;

static void time_pre(struct timer_list *timer)
{
		static unsigned int flag = 0;

		flag = !flag;

		gpio_direction_output(tps3851_input_pin, flag);
		mytimer.expires = jiffies + hw_margin * HZ/1000; //500ms运行一次
		if (tps3851_enble) {
				mod_timer(&mytimer, mytimer.expires);
		}
}

static ssize_t show_enble(struct class *cls,
				struct class_attribute *attr, char *buf)
{
		return sprintf(buf, "%d\n", tps3851_enble);
}

static ssize_t store_enble(struct class *cls, struct class_attribute *attr,
				const char *buf, size_t count)
{
		int enable;

		if (kstrtoint(buf, 0, &enable)) {
				printk("tps3851_enble error\n");
				return -EINVAL;
		}
		printk("tps3851_enble=%d\n", enable);
		tps3851_enble = enable;
		return count;
}

static ssize_t store_pin_out(struct class *cls, struct class_attribute *attr,
				const char *buf, size_t count)
{
		int enable;

		if (kstrtoint(buf, 0, &enable)) {
				printk("tps3851_pin_out error\n");
				return -EINVAL;
		}
		printk("tps3851_pin_out=%d\n", enable);
		gpio_direction_output(tps3851_input_pin, enable);
		return count;
}

static struct class_attribute tps3851_attrs[] = {
		__ATTR(enble, 0644, show_enble, store_enble),
		__ATTR(pin_out, 0644, NULL, store_pin_out),
};

static void create_tps3851_attrs(void)
{
		int i;

		struct class *tps3851_class;
		printk("%s\n", __func__);
		tps3851_class = class_create(THIS_MODULE, "tps3851");
		if (IS_ERR(tps3851_class)) {
				pr_err("create tps3851_class debug class fail\n");
				return;
		}
		for (i = 0; i < ARRAY_SIZE(tps3851_attrs); i++) {
				if (class_create_file(tps3851_class, &tps3851_attrs[i]))
						pr_err("create tps3851 attribute %s fail\n", tps3851_attrs[i].attr.name);
		}
}

static int wdt_probe(struct platform_device *pdev)
{
		const char *value;
		int ret;

		printk("hw_wdt enter probe\n");
		ret = of_property_read_u32(pdev->dev.of_node, "hw_margin_ms", &hw_margin);
		if (ret) {
				return ret;
		}
		ret = of_property_read_string(pdev->dev.of_node,
						"hw-gpios", &value);
		if (ret) {
				printk("no hw-gpios");
				return -1;
		} else {
				tps3851_input_pin = of_get_named_gpio_flags
						(pdev->dev.of_node,
						 "hw-gpios",
						 0, NULL);
				printk("hlm hw-gpios: %d.\n", tps3851_input_pin);
				ret = gpio_request(tps3851_input_pin, "tps3851");
		}

		timer_setup(&mytimer, time_pre, 0);
		mytimer.expires = jiffies + hw_margin * HZ/1000;
		add_timer(&mytimer);
		create_tps3851_attrs();
		return 0;
}

static const struct of_device_id hw_tps3851_wdt_dt_ids[] = {
		{ .compatible = "linux,wdt-tps3851", },
		{ }
};
MODULE_DEVICE_TABLE(of, hw_tps3851_wdt_dt_ids);

static struct platform_driver tps3851_wdt_driver = {
		.driver	= {
				.name = "hw_tps3851_wdt",
				.of_match_table	= hw_tps3851_wdt_dt_ids,
		},
		.probe	= wdt_probe,
};

static int __init wdt_drv_init(void)
{
		return platform_driver_register(&tps3851_wdt_driver);
}
arch_initcall(wdt_drv_init);

MODULE_LICENSE("GPL");
