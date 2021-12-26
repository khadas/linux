// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/types.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/errno.h>
#include <asm/irq.h>
#include <linux/io.h>
#include <linux/major.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/uaccess.h>
#include <linux/pinctrl/consumer.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/pm_wakeup.h>
#include <linux/pm_wakeirq.h>
#include <linux/amlogic/pm.h>
#include "meson_ir_main.h"
#include <linux/amlogic/gki_module.h>

static void meson_ir_tasklet(unsigned long data);
DECLARE_TASKLET_DISABLED(tasklet, meson_ir_tasklet, 0);

static int disable_ir;
static int get_irenv(char *str)
{
	int ret;

	ret = kstrtouint(str, 10, &disable_ir);
	if (ret)
		return -EINVAL;
	return 0;
}

__setup("disable_ir=", get_irenv);

int meson_ir_pulses_malloc(struct meson_ir_chip *chip)
{
	struct meson_ir_dev *r_dev = chip->r_dev;
	int len = r_dev->max_learned_pulse;
	int ret = 0;

	if (r_dev->pulses) {
		dev_info(chip->dev, "ir learning pulse already exists\n");
		return ret;
	}

	r_dev->pulses = kzalloc(sizeof(*r_dev->pulses) +
				len * sizeof(unsigned int), GFP_KERNEL);

	if (!r_dev->pulses) {
		dev_err(chip->dev, "ir learning pulse alloc err\n");
		ret = -ENOMEM;
	}

	return ret;
}

void meson_ir_pulses_free(struct meson_ir_chip *chip)
{
	struct meson_ir_dev *r_dev = chip->r_dev;

	kfree(r_dev->pulses);
	r_dev->pulses = NULL;
}

int meson_ir_scancode_sort(struct ir_map_tab *ir_map)
{
	bool is_sorted;
	u32 tmp;
	int i;
	int j;

	for (i = 0; i < ir_map->map_size - 1; i++) {
		is_sorted = true;
		for (j = 0; j < ir_map->map_size - i - 1; j++) {
			if (ir_map->codemap[j].map.scancode >
			   ir_map->codemap[j + 1].map.scancode) {
				is_sorted = false;
				tmp = ir_map->codemap[j].code;
				ir_map->codemap[j].code  =
						ir_map->codemap[j + 1].code;
				ir_map->codemap[j + 1].code = tmp;
			}
		}
		if (is_sorted)
			break;
	}

	return 0;
}

struct meson_ir_map_tab_list *meson_ir_seek_map_tab(struct meson_ir_chip *chip,
						    int custom_code)
{
	struct meson_ir_map_tab_list *ir_map = NULL;

	list_for_each_entry(ir_map, &chip->map_tab_head, list) {
		if (ir_map->tab.custom_code == custom_code)
			return ir_map;
	}
	return NULL;
}

void meson_ir_map_tab_list_free(struct meson_ir_chip *chip)
{
	struct meson_ir_map_tab_list *ir_map, *t_map;

	list_for_each_entry_safe(ir_map, t_map, &chip->map_tab_head, list) {
		list_del(&ir_map->list);
		kfree((void *)ir_map);
	}
}

void meson_ir_tab_free(struct meson_ir_map_tab_list *ir_map_list)
{
	kfree((void *)ir_map_list);
	ir_map_list = NULL;
}

static int meson_ir_lookup_by_scancode(struct ir_map_tab *ir_map,
				       unsigned int scancode)
{
	int start = 0;
	int end = ir_map->map_size - 1;
	int mid;

	while (start <= end) {
		mid = (start + end) >> 1;
		if (ir_map->codemap[mid].map.scancode < scancode)
			start = mid + 1;
		else if (ir_map->codemap[mid].map.scancode > scancode)
			end = mid - 1;
		else
			return mid;
	}

	return -ENODATA;
}

static int meson_ir_report_rel(struct meson_ir_dev *dev, u32 scancode,
			       int status)
{
	struct meson_ir_chip *chip = (struct meson_ir_chip *)dev->platform_data;
	struct meson_ir_map_tab_list *ct = chip->cur_tab;
	static u32 repeat_count;
	s32 cursor_value = 0;
	u32 valid_scancode;
	u16 mouse_code;
	int cnt;
	s32 move_accelerate[] = CURSOR_MOVE_ACCELERATE;

	/*nothing need to do in normal mode*/
	if (!ct || ct->ir_dev_mode != MOUSE_MODE)
		return -EINVAL;

	if (status == IR_STATUS_REPEAT) {
		valid_scancode = dev->last_scancode;
		repeat_count++;
		if (repeat_count > ARRAY_SIZE(move_accelerate) - 1)
			repeat_count = ARRAY_SIZE(move_accelerate) - 1;
	} else {
		valid_scancode = scancode;
		dev->last_scancode = scancode;
		repeat_count = 0;
	}
	if (valid_scancode == ct->tab.cursor_code.cursor_left_scancode) {
		cursor_value = -(1 + move_accelerate[repeat_count]);
		mouse_code = REL_X;
	} else if (valid_scancode ==
			ct->tab.cursor_code.cursor_right_scancode) {
		cursor_value = 1 + move_accelerate[repeat_count];
		mouse_code = REL_X;
	} else if (valid_scancode ==
			ct->tab.cursor_code.cursor_up_scancode) {
		cursor_value = -(1 + move_accelerate[repeat_count]);
		mouse_code = REL_Y;
	} else if (valid_scancode ==
			ct->tab.cursor_code.cursor_down_scancode) {
		cursor_value = 1 + move_accelerate[repeat_count];
		mouse_code = REL_Y;
	} else {
		return -EINVAL;
	}

	if (DECIDE_VENDOR_TA_ID) {
		for (cnt = 0; cnt <= chip->input_cnt; cnt++) {
			if (chip->search_id[cnt] == ct->tab.vendor)
				break;
		}
		if (cnt > chip->input_cnt) {
			dev_err(chip->dev, "vdndor ID Configuration error\n");
			dev_err(chip->dev, "vendor = %x, product = %x, version = %x\n",
					ct->tab.vendor, ct->tab.product, ct->tab.version);
			return 0;
		}
		input_event(chip->r_dev->input_device_ots[cnt], EV_REL,
					mouse_code, cursor_value);
		input_sync(chip->r_dev->input_device_ots[cnt]);
	} else {
		input_event(chip->r_dev->input_device, EV_REL,
		    mouse_code, cursor_value);
		input_sync(chip->r_dev->input_device);
	}
	meson_ir_dbg(chip->r_dev, "mouse cursor be %s moved %d.\n",
		     mouse_code == REL_X ? "horizontal" : "vertical",
		     cursor_value);

	return 0;
}

static u32 meson_ir_getkeycode(struct meson_ir_dev *dev, u32 scancode)
{
	struct meson_ir_chip *chip = (struct meson_ir_chip *)dev->platform_data;
	struct meson_ir_map_tab_list *ct = chip->cur_tab;
	int index;

	if (!ct) {
		dev_err(chip->dev, "cur_custom is nulll\n");
		return KEY_RESERVED;
	}
	/*return BTN_LEFT in mouse mode*/
	if (ct->ir_dev_mode == MOUSE_MODE &&
	    scancode == ct->tab.cursor_code.cursor_ok_scancode) {
		meson_ir_dbg(chip->r_dev, "mouse left button scancode: 0x%x",
			     BTN_LEFT);
		return BTN_LEFT;
	}

	index = meson_ir_lookup_by_scancode(&ct->tab, scancode);
	if (index < 0) {
		dev_err(chip->dev, "scancode %d undefined\n", scancode);
		return KEY_RESERVED;
	}

	/*save IR remote-control work mode*/
	if (!dev->keypressed &&
	    scancode == ct->tab.cursor_code.fn_key_scancode) {
		if (ct->ir_dev_mode == NORMAL_MODE)
			ct->ir_dev_mode = MOUSE_MODE;
		else
			ct->ir_dev_mode = NORMAL_MODE;
		dev_info(chip->dev, "IR remote control[ID: 0x%x] switch to %s\n",
			 ct->tab.custom_code, ct->ir_dev_mode ?
			 "mouse mode" : "normal mode");
	}

	return ct->tab.codemap[index].map.keycode;
}

static bool meson_ir_is_valid_custom(struct meson_ir_dev *dev)
{
	struct meson_ir_chip *chip = (struct meson_ir_chip *)dev->platform_data;
	int custom_code;

	if (!chip->ir_contr[chip->ir_work].get_custom_code)
		return true;
	custom_code = chip->ir_contr[chip->ir_work].get_custom_code(chip);
	chip->cur_tab = meson_ir_seek_map_tab(chip, custom_code);
	if (chip->cur_tab) {
		dev->keyup_delay = chip->cur_tab->tab.release_delay;
		return true;
	}
	return false;
}

static bool meson_ir_is_next_repeat(struct meson_ir_dev *dev)
{
	unsigned int val = 0;
	unsigned char fbusy = 0;
	unsigned char cnt;

	struct meson_ir_chip *chip = (struct meson_ir_chip *)dev->platform_data;

	for (cnt = 0; cnt < (ENABLE_LEGACY_IR(chip->protocol) ? 2 : 1); cnt++) {
		regmap_read(chip->ir_contr[cnt].base, REG_STATUS, &val);
		fbusy |= IR_CONTROLLER_BUSY(val);
	}
	meson_ir_dbg(chip->r_dev, "ir controller busy flag = %d\n", fbusy);
	if (!dev->wait_next_repeat && fbusy)
		return true;
	else
		return false;
}

static bool meson_ir_set_custom_code(struct meson_ir_dev *dev, u32 code)
{
	struct meson_ir_chip *chip = (struct meson_ir_chip *)dev->platform_data;

	return chip->ir_contr[chip->ir_work].set_custom_code(chip, code);
}

static void meson_ir_tasklet(unsigned long data)
{
	struct meson_ir_chip *chip = (struct meson_ir_chip *)data;
	unsigned long flags;
	int status = -1;
	int scancode = -1;

	/**
	 *need first get_scancode, then get_decode_status, the status
	 *may was set flag from get_scancode function
	 */
	spin_lock_irqsave(&chip->slock, flags);
	if (chip->ir_contr[chip->ir_work].get_scancode)
		scancode = chip->ir_contr[chip->ir_work].get_scancode(chip);
	if (chip->ir_contr[chip->ir_work].get_decode_status)
		status = chip->ir_contr[chip->ir_work].get_decode_status(chip);

	switch (status) {
	case IR_STATUS_NORMAL:
		meson_ir_dbg(chip->r_dev, "receive scancode=0x%x\n", scancode);
		meson_ir_keydown(chip->r_dev, scancode, status);
		break;
	case IR_STATUS_REPEAT:
		meson_ir_dbg(chip->r_dev, "receive repeat\n");
		meson_ir_keydown(chip->r_dev, scancode, status);
		break;
	case IR_STATUS_CUSTOM_DATA:
	case IR_STATUS_CHECKSUM_ERROR:
		dev_err(chip->dev, "decoder error %d\n", status);
		break;
	default:
		dev_err(chip->dev, "receive error %d\n", status);
		break;
	}
	spin_unlock_irqrestore(&chip->slock, flags);
}

static irqreturn_t meson_ir_interrupt(int irq, void *dev_id)
{
	struct meson_ir_chip *rc = (struct meson_ir_chip *)dev_id;
	struct meson_ir_dev *r_dev = rc->r_dev;
	int contr_status = 0;
	int val = 0;
	u32 duration;
	unsigned char cnt;
	enum raw_event_type type = RAW_SPACE;

	regmap_read(rc->ir_contr[MULTI_IR_ID].base, REG_REG1, &val);
	val = (val & GENMASK(28, 16)) >> 16;

	if (r_dev->ir_learning_on) {
		if (r_dev->pulses->len >= r_dev->max_learned_pulse)
			return IRQ_HANDLED;

		/*get pulse durations*/
		r_dev->pulses->pulse[r_dev->pulses->len] = val & GENMASK(30, 0);

		/*get pulse level*/
		regmap_read(rc->ir_contr[MULTI_IR_ID].base, REG_STATUS, &val);
		val = !!((val >> 8) & BIT(1));
		r_dev->pulses->pulse[r_dev->pulses->len] &= ~BIT(31);

		if (val)
			r_dev->pulses->pulse[r_dev->pulses->len] |= BIT(31);
		r_dev->pulses->len++;

		return IRQ_HANDLED;
	}

	/**
	 *software decode multiple protocols by using Time Measurement of
	 *multif-format IR controller
	 */
	if (MULTI_IR_SOFTWARE_DECODE(rc->protocol)) {
		rc->ir_work = MULTI_IR_ID;
		duration = val * 10 * 1000;
		type = RAW_PULSE;
		meson_ir_raw_event_store_edge(rc->r_dev, type, duration);
		meson_ir_raw_event_handle(rc->r_dev);
	} else {
		for (cnt = 0; cnt < (ENABLE_LEGACY_IR(rc->protocol)
		     ? 2 : 1); cnt++) {
			regmap_read(rc->ir_contr[cnt].base, REG_STATUS,
				    &contr_status);
			if (IR_DATA_IS_VALID(contr_status)) {
				rc->ir_work = cnt;
				tasklet_schedule(&tasklet);
				return IRQ_HANDLED;
			}
		}

		if (cnt == (ENABLE_LEGACY_IR(rc->protocol) ? 2 : 1)) {
			dev_err(rc->dev, "invalid interrupt.\n");
			return IRQ_HANDLED;
		}

	}
	return IRQ_HANDLED;
}

static int meson_ir_get_custom_tables(struct device_node *node,
				      struct meson_ir_chip *chip)
{
	const phandle *phandle;
	struct device_node *custom_maps, *map;
	u32 value;
	int ret = -1;
	int index;
	int cnt = 0;
	int cnl;
	char *propname;
	const char *uname;
	unsigned long flags;
	struct meson_ir_map_tab_list *ptable;

	phandle = of_get_property(node, "map", NULL);
	if (!phandle) {
		dev_err(chip->dev, "%s:can't find match custom\n", __func__);
		return -EINVAL;
	}

	custom_maps = of_find_node_by_phandle(be32_to_cpup(phandle));
	if (!custom_maps) {
		dev_err(chip->dev, "can't find device node key\n");
		return -ENODATA;
	}

	ret = of_property_read_u32(custom_maps, "mapnum", &value);
	if (ret) {
		dev_err(chip->dev, "please config correct mapnum item\n");
		return -ENODATA;
	}
	chip->custom_num = value;
	if (chip->custom_num > CUSTOM_NUM_MAX)
		chip->custom_num = CUSTOM_NUM_MAX;

	for (index = 0; index < chip->custom_num; index++) {
		propname = kasprintf(GFP_KERNEL, "map%d", index);
		phandle = of_get_property(custom_maps, propname, NULL);
		/* propname is never used just use to find phandle once
		 * To avoid kmemleak warning, should be freed
		 */
		kfree(propname);
		propname = NULL;

		if (!phandle) {
			dev_err(chip->dev, "can't find match map%d\n", index);
			return -ENODATA;
		}
		map = of_find_node_by_phandle(be32_to_cpup(phandle));
		if (!map) {
			dev_err(chip->dev, "can't find device node key\n");
			return -ENODATA;
		}

		ret = of_property_read_u32(map, "size", &value);
		if (ret || value > MAX_KEYMAP_SIZE) {
			dev_err(chip->dev, "no config size item or err\n");
			return -ENODATA;
		}

		/*alloc memory*/
		ptable = kzalloc(sizeof(*ptable) +
				 value * sizeof(union _codemap), GFP_KERNEL);
		if (!ptable)
			return -ENOMEM;

		ptable->tab.map_size = value;

		ret = of_property_read_string(map, "mapname", &uname);
		if (ret) {
			dev_err(chip->dev, "please config mapname item\n");
			goto err;
		}
		snprintf(ptable->tab.custom_name, CUSTOM_NAME_LEN, "%s", uname);

		ret = of_property_read_u32(map, "customcode", &value);
		if (ret) {
			dev_err(chip->dev, "please config customcode item\n");
			goto err;
		}
		ptable->tab.custom_code = value;

		ret = of_property_read_u32(map, "release_delay", &value);
		if (ret) {
			dev_err(chip->dev,
				"can't find the node <release_delay>\n");
			goto err;
		}
		ptable->tab.release_delay = value;

		ret = of_property_read_u32(map, "vendor", &value);
		if (ret)
			value = 0;
		chip->vendor = value;
		ptable->tab.vendor = chip->vendor;

		ret = of_property_read_u32(map, "product", &value);
		if (ret)
			value = 0;
		chip->product = value;
		ptable->tab.product = chip->product;

		ret = of_property_read_u32(map, "version", &value);
		if (ret)
			value = 0;
		chip->version = value;
		ptable->tab.version = chip->version;

		if (DECIDE_VENDOR_CHIP_ID) {
			if (cnt == 0 || cnl != chip->vendor) {
				chip->r_dev->input_device_ots[cnt] =
					devm_input_allocate_device(chip->dev);
				input_set_drvdata(chip->r_dev->input_device_ots[cnt], chip->r_dev);
				meson_ir_input_device_ots_init(chip->r_dev->input_device_ots[cnt],
					chip->dev, chip, "ir_keypad1", cnt);
				meson_ir_input_ots_configure(chip->r_dev, cnt);
				chip->input_cnt = cnt;
				chip->search_id[cnt] = chip->vendor;
				ret = input_register_device(chip->r_dev->input_device_ots[cnt]);
				if (ret < 0)
					return ret;
			}
			cnl = chip->vendor;
			cnt++;
		}

		ret = of_property_read_u32_array(map, "keymap",
						 (u32 *)&ptable->tab.codemap[0],
						 ptable->tab.map_size);
		if (ret) {
			dev_err(chip->dev, "please config keymap item\n");
			goto err;
		}

		memset(&ptable->tab.cursor_code, 0xff,
		       sizeof(struct cursor_codemap));
		meson_ir_scancode_sort(&ptable->tab);
		/*insert list*/
		spin_lock_irqsave(&chip->slock, flags);
		list_add_tail(&ptable->list, &chip->map_tab_head);
		spin_unlock_irqrestore(&chip->slock, flags);
	}
	return 0;
err:
	meson_ir_tab_free(ptable);
	return -ENODATA;
}

static struct regmap_config meson_ir_regmap_config = {
	.reg_bits = 32,
	.val_bits = 32,
	.reg_stride = 4,
};

static int meson_ir_get_devtree_pdata(struct platform_device *pdev)
{
	struct resource *res_irq;
	struct resource *res_mem;
	struct regmap *reg_base;
	resource_size_t *res_start[2];
	int ret;
	int value = 0;
	unsigned char i;

	struct meson_ir_chip *chip = platform_get_drvdata(pdev);

	ret = of_property_read_u32(pdev->dev.of_node,
				   "protocol", &chip->protocol);
	if (ret) {
		dev_err(chip->dev, "can't find the node <protocol>\n");
		chip->protocol = 1;
	}

	ret = of_property_read_u32(pdev->dev.of_node,
				   "led_blink", &chip->r_dev->led_blink);
	if (ret)
		chip->r_dev->led_blink = 0;

	ret = of_property_read_u32(pdev->dev.of_node,
				   "led_blink_frq", &value);
	if (ret) {
		chip->r_dev->delay_on = DEFAULT_LED_BLINK_FRQ;
		chip->r_dev->delay_off = DEFAULT_LED_BLINK_FRQ;
	} else {
		chip->r_dev->delay_off = value;
		chip->r_dev->delay_on = value;
	}

	for (i = 0; i < 2; i++) {
		res_mem = platform_get_resource(pdev, IORESOURCE_MEM, i);
		if (IS_ERR_OR_NULL(res_mem)) {
			dev_err(chip->dev, "get IORESOURCE_MEM error, %ld\n",
				PTR_ERR(res_mem));
			return PTR_ERR(res_mem);
		}
		res_start[i] = devm_ioremap_resource(&pdev->dev, res_mem);
		chip->ir_contr[i].remote_regs = (void __iomem *)res_start[i];
		meson_ir_regmap_config.max_register =
			resource_size(res_mem) - 4;
		meson_ir_regmap_config.name = devm_kasprintf(&pdev->dev,
							     GFP_KERNEL,
							     "%d", i);
		reg_base = devm_regmap_init_mmio(&pdev->dev, res_start[i],
						 &meson_ir_regmap_config);

		if (IS_ERR(reg_base)) {
			dev_err(chip->dev, "regmap init error, %ld\n",
				PTR_ERR(reg_base));

			return PTR_ERR(reg_base);
		}
		chip->ir_contr[i].base = reg_base;
	}

	res_irq = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (IS_ERR_OR_NULL(res_irq)) {
		dev_err(chip->dev, "get IORESOURCE_IRQ error, %ld\n",
			PTR_ERR(res_irq));
		return PTR_ERR(res_irq);
	}

	chip->irqno = res_irq->start;

	ret = of_property_read_u32(pdev->dev.of_node,
				   "max_frame_time", &value);
	if (ret) {
		dev_err(chip->dev, "can't find the node <max_frame_time>\n");
		value = 200; /*default value*/
	}

	chip->r_dev->max_frame_time = value;

	/*create map table */
	ret = meson_ir_get_custom_tables(pdev->dev.of_node, chip);
	if (ret < 0)
		return ret;

	return 0;
}

static int meson_ir_hardware_init(struct platform_device *pdev)
{
	int ret;

	struct meson_ir_chip *chip = platform_get_drvdata(pdev);

	if (!pdev->dev.of_node) {
		dev_err(chip->dev, "pdev->dev.of_node == NULL!\n");
		return -ENODATA;
	}

	ret = meson_ir_get_devtree_pdata(pdev);
	if (ret < 0)
		return ret;

	chip->set_register_config(chip, chip->protocol);
	ret = devm_request_irq(&pdev->dev, chip->irqno, meson_ir_interrupt,
			       IRQF_SHARED | IRQF_NO_SUSPEND, "meson_ir", (void *)chip);
	if (ret < 0) {
		dev_err(chip->dev, "request_irq error %d\n", ret);
		return -ENODEV;
	}

	chip->irq_cpumask = 1;
	irq_set_affinity_hint(chip->irqno, cpumask_of(chip->irq_cpumask));

	tasklet.data = (unsigned long)chip;
	tasklet_enable(&tasklet);

	return 0;
}

static void meson_ir_input_device_init(struct input_dev *dev,
				       struct device *parent, const char *name)
{
	dev->name = name;
	dev->phys = "keypad/input0";
	dev->dev.parent = parent;
	dev->id.bustype = BUS_ISA;
	dev->id.vendor  = 0x0001;
	dev->id.product = 0x0001;
	dev->id.version = 0x0100;
	dev->rep[REP_DELAY] = 0xffffffff;  /*close input repeat*/
	dev->rep[REP_PERIOD] = 0xffffffff; /*close input repeat*/
}

void meson_ir_input_device_ots_init(struct input_dev *dev,
		struct device *parent,  struct meson_ir_chip *chip, const char *name, int cnt0)

{
	chip->r_dev->input_device_ots[cnt0]->name = "ir_keypad1";
	chip->r_dev->input_device_ots[cnt0]->phys = "keypad/input0";
	chip->r_dev->input_device_ots[cnt0]->dev.parent = chip->dev;
	chip->r_dev->input_device_ots[cnt0]->id.bustype = BUS_ISA;
	chip->r_dev->input_device_ots[cnt0]->id.vendor  = chip->vendor;
	chip->r_dev->input_device_ots[cnt0]->id.product = chip->product;
	chip->r_dev->input_device_ots[cnt0]->id.version = chip->version;
	chip->r_dev->input_device_ots[cnt0]->rep[REP_DELAY] = 0xffffffff;
	chip->r_dev->input_device_ots[cnt0]->rep[REP_PERIOD] = 0xffffffff;
}

static int meson_ir_probe(struct platform_device *pdev)
{
	struct meson_ir_dev *dev;
	struct meson_ir_chip *chip;
	int ret;
	int id;

	chip = devm_kzalloc(&pdev->dev, sizeof(struct meson_ir_chip),
			    GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	dev = devm_kzalloc(&pdev->dev, sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	dev->input_device = devm_input_allocate_device(&pdev->dev);
	if (!dev->input_device)
		return -ENOMEM;

	input_set_drvdata(dev->input_device, dev);
	spin_lock_init(&dev->keylock);
	dev->wait_next_repeat = 0;
	dev->enable = 1;

	mutex_init(&chip->file_lock);
	spin_lock_init(&chip->slock);
	INIT_LIST_HEAD(&chip->map_tab_head);

	chip->r_dev = dev;
	chip->dev = &pdev->dev;
	chip->rx_count = 0;

	chip->r_dev->dev = &pdev->dev;
	chip->r_dev->platform_data = (void *)chip;
	chip->r_dev->getkeycode = meson_ir_getkeycode;
	chip->r_dev->report_rel = meson_ir_report_rel;
	chip->r_dev->set_custom_code = meson_ir_set_custom_code;
	chip->r_dev->is_valid_custom = meson_ir_is_valid_custom;
	chip->r_dev->is_next_repeat  = meson_ir_is_next_repeat;
	chip->r_dev->max_learned_pulse = MAX_LEARNED_PULSE;
	chip->set_register_config = meson_ir_register_default_config;
	platform_set_drvdata(pdev, chip);

	meson_ir_input_device_init(dev->input_device, &pdev->dev, "ir_keypad");
	meson_ir_input_configure(dev);

	ret = input_register_device(dev->input_device);
	if (ret < 0)
		return ret;

	ret = meson_ir_hardware_init(pdev);
	if (ret < 0)
		return ret;

	timer_setup(&dev->timer_keyup, meson_ir_timer_keyup, 0);

	ret = meson_ir_cdev_init(chip);
	if (ret < 0)
		return ret;

	dev->rc_type = chip->protocol;

	device_init_wakeup(&pdev->dev, true);
	dev_pm_set_wake_irq(&pdev->dev, chip->irqno);

	led_trigger_register_simple("ir_led", &dev->led_feedback);

	if (MULTI_IR_SOFTWARE_DECODE(dev->rc_type))
		meson_ir_raw_event_register(dev);

	if (disable_ir) {
		for (id = 0; id < (ENABLE_LEGACY_IR(chip->protocol) ? 2 : 1); id++) {
			regmap_update_bits(chip->ir_contr[id].base, REG_REG1,
					BIT(15), 0);
		}
	}
	return 0;
}

static int meson_ir_remove(struct platform_device *pdev)
{
	struct meson_ir_chip *chip = platform_get_drvdata(pdev);

	tasklet_disable(&tasklet);
	tasklet_kill(&tasklet);

	if (MULTI_IR_SOFTWARE_DECODE(chip->r_dev->rc_type))
		meson_ir_raw_event_unregister(chip->r_dev);

	led_trigger_unregister_simple(chip->r_dev->led_feedback);

	dev_pm_clear_wake_irq(&pdev->dev);
	device_init_wakeup(&pdev->dev, false);

	meson_ir_cdev_free(chip);
	input_unregister_device(chip->r_dev->input_device);
	meson_ir_map_tab_list_free(chip);
	irq_set_affinity_hint(chip->irqno, NULL);

	return 0;
}

static int meson_ir_resume(struct device *dev)
{
	struct meson_ir_chip *chip = dev_get_drvdata(dev);
	unsigned int val;
	unsigned long flags;
	unsigned char cnt;

	/*resume register config*/
	spin_lock_irqsave(&chip->slock, flags);
	chip->set_register_config(chip, chip->protocol);
	/* read REG_STATUS and REG_FRAME to clear status */
	for (cnt = 0; cnt < (ENABLE_LEGACY_IR(chip->protocol) ? 2 : 1); cnt++) {
		regmap_read(chip->ir_contr[cnt].base, REG_STATUS, &val);
		regmap_read(chip->ir_contr[cnt].base, REG_FRAME, &val);
	}
	spin_unlock_irqrestore(&chip->slock, flags);

#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
	if (get_resume_method() == REMOTE_WAKEUP) {
		input_event(chip->r_dev->input_device,
			    EV_KEY, KEY_POWER, 1);
		input_sync(chip->r_dev->input_device);
		input_event(chip->r_dev->input_device,
			    EV_KEY, KEY_POWER, 0);
		input_sync(chip->r_dev->input_device);
	}

	if (get_resume_method() == REMOTE_CUS_WAKEUP) {
		input_event(chip->r_dev->input_device, EV_KEY, 133, 1);
		input_sync(chip->r_dev->input_device);
		input_event(chip->r_dev->input_device, EV_KEY, 133, 0);
		input_sync(chip->r_dev->input_device);
	}
#endif

	irq_set_affinity_hint(chip->irqno, cpumask_of(chip->irq_cpumask));

	if (is_pm_s2idle_mode())
		return 0;
	enable_irq(chip->irqno);
	return 0;
}

static int meson_ir_suspend(struct device *dev)
{
	struct meson_ir_chip *chip = dev_get_drvdata(dev);

	if (is_pm_s2idle_mode())
		return 0;
	disable_irq(chip->irqno);
	return 0;
}

static const struct of_device_id meson_ir_dt_match[] = {
	{
		.compatible     = "amlogic, meson-ir",
	},
	{},
};

#ifdef CONFIG_PM
static const struct dev_pm_ops meson_ir_pm_ops = {
	.suspend_late = meson_ir_suspend,
	.resume_early = meson_ir_resume,
};
#endif

static struct platform_driver meson_ir_driver = {
	.probe = meson_ir_probe,
	.remove = meson_ir_remove,
	.driver = {
		.name = DRIVER_NAME,
		.of_match_table = meson_ir_dt_match,
#ifdef CONFIG_PM
		.pm = &meson_ir_pm_ops,
#endif
	},
};

static int __init meson_ir_driver_init(void)
{
	int ret;

	ret = meson_ir_xmp_decode_init();
	if (ret)
		return ret;

	return platform_driver_register(&meson_ir_driver);
}
module_init(meson_ir_driver_init);

static void __exit meson_ir_driver_exit(void)
{
	meson_ir_xmp_decode_exit();
	platform_driver_unregister(&meson_ir_driver);
}
module_exit(meson_ir_driver_exit);

MODULE_AUTHOR("AMLOGIC");
MODULE_DESCRIPTION("AMLOGIC IR DRIVER");
MODULE_LICENSE("GPL v2");
