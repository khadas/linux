/*
 *  GPIO driven matrix keyboard driver
 *
 *  Copyright (c) 2018 Khadas <www.khadas.com>
 *
 *  Based on corgikbd.c
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 */

#include <linux/types.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/jiffies.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/input/matrix_keypad.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_platform.h>

#define COL_GPIO_NUM  3
#define ROW_GPIO_NUM  3
#define KEYS_NUM  COL_GPIO_NUM*ROW_GPIO_NUM

extern int get_board_type(void);
struct khadas_key {
	int key;
	int code;
};

enum KEY_INDEX {
	KEY1 = 0,
	KEY2,
	KEY3,
	KEY4,
	KEY5,
	KEY6,
	KEY7,
	KEY8,
	KEY9,
};

static int Khadas_Code[KEYS_NUM] = {
	KEY_REPLY,
	KEY_REPLY,
	KEY_BACK,
	KEY_DELETE,
	KEY_RIGHT,
	KEY_LEFT,
	KEY_DOWN,
	KEY_UP,
	KEY_SPACE
};

static int key_Code_test[KEYS_NUM+2] = {
	KEY_1,
	KEY_3,
	KEY_2,
	KEY_4,
	KEY_RIGHT,
	KEY_LEFT,
	KEY_DOWN,
	KEY_UP,
	KEY_5,
	KEY_6,
	KEY_7
};
struct matrix_keypad {
	struct platform_device *pdev;
	struct input_dev *input_dev;
	struct delayed_work work;
	struct khadas_key *key;
	int last_key;
	bool scan_pending;
	bool stopped;
	unsigned int *col_gpios;
	unsigned int *row_gpios;
	int col_scan_delay_us;
	bool no_autorepeat;
	int debounce_ms;
};
struct input_dev *input_key_dev;
void Khadas_key_test(u32 code,u32 state)
{
	//printk("hlm code=%d, state=%d\n",code, state);
	input_report_key(input_key_dev, code, state);
	input_sync(input_key_dev);
}

static void __activate_col(struct matrix_keypad *keypad,
			   int col, bool on)
{

	if (on) {
		gpio_direction_output(keypad->col_gpios[col], 1);
	} else {
		gpio_direction_output(keypad->col_gpios[col], 0);
	}
}

static void activate_col(struct matrix_keypad *keypad,
			 int col, bool on)
{
	__activate_col(keypad, col, on);

	if (on && keypad->col_scan_delay_us)
		udelay(keypad->col_scan_delay_us);
}

static void activate_all_cols(struct matrix_keypad *keypad, bool on)
{
	int col;

	for (col = 0; col < COL_GPIO_NUM; col++)
		__activate_col(keypad, col, on);
}

static int row_asserted(struct matrix_keypad *keypad, int row)
{
	gpio_set_value_cansleep(keypad->row_gpios[row], 1);
	gpio_direction_input(keypad->row_gpios[row]);
	return gpio_get_value_cansleep(keypad->row_gpios[row]);
}

/*
 * This gets the keys from keyboard and reports it to input subsystem
 */
static void matrix_keypad_scan(struct work_struct *work)
{
	struct matrix_keypad *keypad =
		container_of(work, struct matrix_keypad, work.work);
	struct input_dev *input_dev = keypad->input_dev;
	int row, col, index, level, key_num;

	col = 0;
	row = 0;
	key_num = -1;
	activate_all_cols(keypad, true);
	for (row = 0; row < ROW_GPIO_NUM; row++) {
		level = row_asserted(keypad, row);
		if (level == 0) {
			if (row == 0)
				key_num = KEY3;
			else if (row == 1)
				key_num = KEY6;
			else
				key_num = KEY9;
			goto end;
		}
	}
	for (col = 0; col < COL_GPIO_NUM; col++) {
		activate_col(keypad, col, false);
		for (row = 0; row < ROW_GPIO_NUM; row++) {
			if (col == row)
				continue;
			index = row*3 + col;
			level = row_asserted(keypad, row);
			if (level == 0) {
				udelay(keypad->debounce_ms);
				level = gpio_get_value_cansleep(keypad->row_gpios[row]);
				if (level == 0) {
					if (col == 0) {
						if (row == 1)
							key_num = KEY4;
						else if (row == 2)
							key_num = KEY7;
					} else if (col == 2) {
						if (row == 0)
							key_num = KEY2;
						else if (row == 1)
							key_num = KEY5;
					} else if (col == 1){
						if (row == 0)
							key_num = KEY1;
						else if (row == 2)
							key_num = KEY8;
					}
				goto end;
				}
			}
		}
		activate_col(keypad, col, true);
	}

end:
	if (keypad->last_key != key_num) {
extern int key_test_flag;
		if(key_test_flag){
			if(key_num != -1)
				input_report_key(input_dev, key_Code_test[key_num], 1);
			else
				input_report_key(input_dev, key_Code_test[keypad->last_key], 0);
		}
		else{
			if(key_num != -1)
				input_report_key(input_dev, keypad->key[key_num].code, 1);
			else
				input_report_key(input_dev, keypad->key[keypad->last_key].code, 0);
		}
		input_sync(input_dev);
	}
	keypad->last_key = key_num;
	schedule_delayed_work(&keypad->work, 10);
}

static int matrix_keypad_start(struct input_dev *dev)
{
	struct matrix_keypad *keypad = input_get_drvdata(dev);

	keypad->stopped = false;
	activate_all_cols(keypad, true);

	/*
	 * Schedule an immediate key scan to capture current key state;
	 * columns will be activated and IRQs be enabled after the scan.
	 */
	schedule_delayed_work(&keypad->work, 0);

	return 0;
}

static void matrix_keypad_stop(struct input_dev *dev)
{
	struct matrix_keypad *keypad = input_get_drvdata(dev);

	keypad->stopped = true;

	flush_work(&keypad->work.work);
}


static int matrix_keypad_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct matrix_keypad *keypad = platform_get_drvdata(pdev);

	matrix_keypad_stop(keypad->input_dev);


	return 0;
}

static int matrix_keypad_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct matrix_keypad *keypad = platform_get_drvdata(pdev);


	matrix_keypad_start(keypad->input_dev);

	return 0;
}

static SIMPLE_DEV_PM_OPS(matrix_keypad_pm_ops,
			 matrix_keypad_suspend, matrix_keypad_resume);

static int matrix_keypad_init_gpio(struct platform_device *pdev,
				   struct matrix_keypad *keypad)
{
	int i, err;

	/* initialized strobe lines as outputs, activated */
	for (i = 0; i < COL_GPIO_NUM; i++) {
		err = gpio_request(keypad->col_gpios[i], "matrix_kbd_row_col");
		if (err) {
			dev_err(&pdev->dev,
				"failed to request GPIO%d for COL%d\n",
				keypad->col_gpios[i], i);
			goto err_free_cols;
		}

		gpio_direction_output(keypad->col_gpios[i], 1);
	}

	return 0;

err_free_cols:
	while (--i >= 0)
		gpio_free(keypad->col_gpios[i]);

	return err;
}

static void matrix_keypad_free_gpio(struct matrix_keypad *keypad)
{
	int i;
	for (i = 0; i < COL_GPIO_NUM; i++)
		gpio_free(keypad->col_gpios[i]);
}

static int matrix_keypad_parse_dt(struct device *dev, struct matrix_keypad *keypad, struct input_dev *input_dev)
{
	struct device_node *np = dev->of_node;
	unsigned int *gpios;
	struct khadas_key *keys;
	int i, nrow, ncol;

	if (!np) {
		dev_err(dev, "device lacks DT data\n");
		return -ENODEV;
	}
	memset(keypad, 0, sizeof(*keypad));

	nrow = ROW_GPIO_NUM;
	ncol = COL_GPIO_NUM;
	if (nrow <= 0 || ncol <= 0) {
		dev_err(dev, "number of keypad rows/columns not specified\n");
		return -EINVAL;
	}

	if (of_get_property(np, "linux,no-autorepeat", NULL))
		keypad->no_autorepeat = true;


	of_property_read_u32(np, "debounce-delay-ms", &keypad->debounce_ms);
	of_property_read_u32(np, "col-scan-delay-us",
						&keypad->col_scan_delay_us);

	gpios = devm_kzalloc(dev,
			     sizeof(unsigned int) *
				(ROW_GPIO_NUM + COL_GPIO_NUM),
			     GFP_KERNEL);


	if (!gpios) {
		dev_err(dev, "could not allocate memory for gpios\n");
		return -ENOMEM;
	}

	keys = devm_kzalloc(dev, sizeof(*(keypad->key))*(KEYS_NUM+2) , GFP_KERNEL);

	if (!keys) {
		dev_err(dev, "could not allocate memory for keys\n");
		kfree(gpios);
		return -ENOMEM;
	}

	for (i = 0; i < ROW_GPIO_NUM; i++)
		gpios[i] = of_get_named_gpio(np, "row_col_gpio", i);

	for (i = 0; i < COL_GPIO_NUM; i++)
		gpios[ROW_GPIO_NUM + i] =
			of_get_named_gpio(np, "row_col_gpio", i);

	keypad->row_gpios = gpios;
	keypad->col_gpios = &gpios[ROW_GPIO_NUM];
	
	for (i =0; i < (KEYS_NUM+2); i++) {
		keys[i].key = i;
		keys[i].code = key_Code_test[i];
		input_set_capability(input_dev, EV_KEY, keys[i].code);
	}
	for (i =0; i < KEYS_NUM; i++) {
		keys[i].key = i;
		keys[i].code = Khadas_Code[i];
		input_set_capability(input_dev, EV_KEY, keys[i].code);
	}
	keypad->key = keys;
	return 0;
}

static int matrix_keypad_probe(struct platform_device *pdev)
{
	struct matrix_keypad *keypad;
	struct input_dev *input_dev;
	int err, ret;

//    type = get_board_type();
//    if (type != KHADAS_CAPTAIN)
//       return -1;

	printk("%s start\n", __func__);
	keypad = kzalloc(sizeof(*keypad), GFP_KERNEL);
	input_dev = input_allocate_device();
	if (!keypad || !input_dev) {
		err = -ENOMEM;
		goto err_free_mem;
	}
	input_key_dev=input_dev;
	ret = matrix_keypad_parse_dt(&pdev->dev, keypad, input_dev);
	if (ret < 0)
		goto err_free_mem;




	keypad->input_dev = input_dev;
	keypad->pdev = pdev;
	keypad->stopped = true;
	keypad->last_key = -1;

	INIT_DELAYED_WORK(&keypad->work, matrix_keypad_scan);

	input_dev->name		= pdev->name;
	input_dev->id.bustype	= BUS_HOST;
	input_dev->dev.parent	= &pdev->dev;
	input_dev->open		= matrix_keypad_start;
	input_dev->close	= matrix_keypad_stop;
	input_dev->id.vendor = 0x0001;
	input_dev->id.product = 0x0001;
	input_dev->id.version = 0x0100;

	if (!keypad->no_autorepeat)
		__set_bit(EV_REP, input_dev->evbit);

	input_set_drvdata(input_dev, keypad);

	err = matrix_keypad_init_gpio(pdev, keypad);
	if (err)
		goto err_free_mem;

	err = input_register_device(keypad->input_dev);
	if (err)
		goto err_free_gpio;

	platform_set_drvdata(pdev, keypad);

	matrix_keypad_start(keypad->input_dev);

	printk("%s end\n", __func__);
	return 0;

err_free_gpio:
	matrix_keypad_free_gpio(keypad);
err_free_mem:
	input_free_device(input_dev);
	kfree(keypad);
	return err;
}

static int matrix_keypad_remove(struct platform_device *pdev)
{
	struct matrix_keypad *keypad = platform_get_drvdata(pdev);

	matrix_keypad_free_gpio(keypad);
	input_unregister_device(keypad->input_dev);
	kfree(keypad);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id matrix_keypad_dt_match[] = {
	{ .compatible = "khadas-matrix-keypad" },
	{ }
};
MODULE_DEVICE_TABLE(of, matrix_keypad_dt_match);
#endif

static struct platform_driver matrix_keypad_driver = {
	.probe		= matrix_keypad_probe,
	.remove		= matrix_keypad_remove,
	.driver		= {
		.name	= "khadas-matrix-keypad",
		.pm	= &matrix_keypad_pm_ops,
		.of_match_table = of_match_ptr(matrix_keypad_dt_match),
	},
};
module_platform_driver(matrix_keypad_driver);

MODULE_AUTHOR("Terry <terry@szwesion.com>");
MODULE_DESCRIPTION("GPIO Driven Matrix Keypad Driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:matrix-keypad");
