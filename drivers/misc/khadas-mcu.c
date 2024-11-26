// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2023 Shenzhen Wesion Technology Co.,Ltd. All rights reserved.
 */

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/string.h>
#include <linux/list.h>
#include <linux/sysfs.h>
#include <linux/ctype.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/amlogic/pm.h>

/* Device registers */
#define MCU_BOOT_EN_WOL_REG             0x21
#define MCU_CMD_FAN_STATUS_CTRL_REG     0x88
#define MCU_CMD_FAN_STATUS_CTRL_REGv2   0x8A
#define MCU_USB_PCIE_SWITCH_REG         0x33 /* VIM3/VIM3L only */
#define MCU_PWR_OFF_CMD_REG             0x80
#define MCU_SHUTDOWN_NORMAL_REG         0x2c

#define MCU_FAN_TRIG_TEMP_LEVEL0        60	// 50 degree if not set
#define MCU_FAN_TRIG_TEMP_LEVEL1        80	// 60 degree if not set
#define MCU_FAN_TRIG_TEMP_LEVEL2        100	// 70 degree if not set
#define MCU_FAN_TRIG_MAXTEMP            105
#define MCU_FAN_LOOP_SECS               (30 * HZ)	// 30 seconds
#define MCU_FAN_TEST_LOOP_SECS          (5 * HZ)  // 5 seconds
#define MCU_FAN_LOOP_NODELAY_SECS       0
#define MCU_FAN_SPEED_OFF               0
#define MCU_FAN_SPEED_LOW               1
#define MCU_FAN_SPEED_MID               2
#define MCU_FAN_SPEED_HIGH              3
#define MCU_FAN_SPEED_LOW_V2            0x32
#define MCU_FAN_SPEED_MID_V2            0x48
#define MCU_FAN_SPEED_HIGH_V2           0x64

enum mcu_fan_mode {
	MCU_FAN_MODE_MANUAL = 0,
	MCU_FAN_MODE_AUTO,
};

enum mcu_fan_level {
	MCU_FAN_LEVEL_0 = 0,
	MCU_FAN_LEVEL_1,
	MCU_FAN_LEVEL_2,
	MCU_FAN_LEVEL_3,
};

enum mcu_fan_status {
	MCU_FAN_STATUS_DISABLE = 0,
	MCU_FAN_STATUS_ENABLE,
};

enum khadas_board_hwver {
	KHADAS_BOARD_HWVER_NONE = 0,
	KHADAS_BOARD_HWVER_V10,
	KHADAS_BOARD_HWVER_V11,
	KHADAS_BOARD_HWVER_V12,
	KHADAS_BOARD_HWVER_V13,
	KHADAS_BOARD_HWVER_V14
};

enum khadas_board {
	KHADAS_BOARD_NONE = 0,
	KHADAS_BOARD_VIM1,
	KHADAS_BOARD_VIM2,
	KHADAS_BOARD_VIM3,
	KHADAS_BOARD_VIM4,
	KHADAS_BOARD_VIM1S
};

enum mcu_usb_pcie_switch_mode {
	MCU_USB_PCIE_SWITCH_MODE_USB3 = 0,
	MCU_USB_PCIE_SWITCH_MODE_PCIE
};

struct mcu_fan_data {
	struct platform_device *pdev;
	struct class *fan_class;
	struct delayed_work work;
	struct delayed_work fan_test_work;
	enum mcu_fan_status enable;
	enum mcu_fan_mode mode;
	enum mcu_fan_level level;
	int	trig_temp_level0;
	int	trig_temp_level1;
	int	trig_temp_level2;
};

struct mcu_data {
	struct i2c_client *client;
	struct class *wol_class;
	struct class *mcu_class;
	struct class *usb_pcie_switch_class;
	int wol_enable;
	u8 usb_pcie_switch_mode;

	enum khadas_board board;
	enum khadas_board_hwver hwver;
	struct mcu_fan_data fan_data;
};

struct mcu_data *g_mcu_data;

/*extern void realtek_enable_wol(int enable, bool suspend);
void mcu_enable_wol(int enable, bool suspend)
{
	realtek_enable_wol(enable, suspend);
}*/
static int i2c_master_reg8_send(const struct i2c_client *client,
		const char reg, const char *buf, int count)
{
	struct i2c_adapter *adap = client->adapter;
	struct i2c_msg msg;
	int ret;
	char *tx_buf = kzalloc(count + 1, GFP_KERNEL);

	if (!tx_buf)
		return -ENOMEM;
	tx_buf[0] = reg;
	memcpy(tx_buf + 1, buf, count);

	msg.addr = client->addr;
	msg.flags = client->flags;
	msg.len = count + 1;
	msg.buf = (char *)tx_buf;

	ret = i2c_transfer(adap, &msg, 1);
	kfree(tx_buf);
	return (ret == 1) ? count : ret;
}

static int i2c_master_reg8_recv(const struct i2c_client *client,
		const char reg, char *buf, int count)
{
	struct i2c_adapter *adap = client->adapter;
	struct i2c_msg msgs[2];
	int ret;
	char reg_buf = reg;

	msgs[0].addr = client->addr;
	msgs[0].flags = client->flags;
	msgs[0].len = 1;
	msgs[0].buf = &reg_buf;

	msgs[1].addr = client->addr;
	msgs[1].flags = client->flags | I2C_M_RD;
	msgs[1].len = count;
	msgs[1].buf = (char *)buf;

	ret = i2c_transfer(adap, msgs, 2);

	return (ret == 2) ? count : ret;
}

static int mcu_i2c_read_regs(struct i2c_client *client,
		char reg, char buf[], unsigned int len)
{
	int ret;

	ret = i2c_master_reg8_recv(client, reg, buf, len);

	return ret;
}

static int mcu_i2c_write_regs(struct i2c_client *client,
		char reg, char buf[], int len)
{
	int ret;

	ret = i2c_master_reg8_send(client, reg, buf, (int)len);

	return ret;
}

static int is_mcu_fan_control_supported(void)
{
	// MCU FAN control is supported for:
	// 1. Khadas VIM1 V13 and later
	// 2. Khadas VIM2 V13 and later
	// 3. Khadas VIM3 V11 and later
	// 4. Khadas VIM4 V11 and later
	if (g_mcu_data->board == KHADAS_BOARD_VIM1) {
		if (g_mcu_data->hwver >= KHADAS_BOARD_HWVER_V13)
			return 1;
		else
			return 0;
	} else if (g_mcu_data->board == KHADAS_BOARD_VIM2) {
		if (g_mcu_data->hwver > KHADAS_BOARD_HWVER_V12)
			return 1;
		else
			return 0;
	} else if (g_mcu_data->board == KHADAS_BOARD_VIM3) {
		if (g_mcu_data->hwver >= KHADAS_BOARD_HWVER_V11)
			return 1;
		else
			return 0;
	} else if (g_mcu_data->board == KHADAS_BOARD_VIM4) {
		if (g_mcu_data->hwver >= KHADAS_BOARD_HWVER_V11)
			return 1;
		else
			return 0;
	} else if (g_mcu_data->board == KHADAS_BOARD_VIM1S) {
		if (g_mcu_data->hwver >= KHADAS_BOARD_HWVER_V10)
			return 1;
		else
			return 0;
	} else {
		return 0;
	}
}

static bool is_mcu_wol_supported(void)
{
	// WOL is supported for:
	// 1. Khadas VIM2
	// 2. Khadas VIM3
	// 3. khadas VIM4
	if ((KHADAS_BOARD_VIM2 == g_mcu_data->board) ||
			(KHADAS_BOARD_VIM3 == g_mcu_data->board) ||
			(KHADAS_BOARD_VIM4 == g_mcu_data->board))
		return 1;
	else
		return 0;
}

static bool is_mcu_usb_pcie_switch_supported(void) {
	// MCU USB PCIe switch is supported for:
	// 1. Khadas VIM3
	if (KHADAS_BOARD_VIM3 == g_mcu_data->board)
		return 1;
	else
		return 0;
}

static void mcu_fan_level_set(struct mcu_fan_data *fan_data, int level)
{
	if (is_mcu_fan_control_supported()) {
		int ret;
		u8 data = 0;

		g_mcu_data->fan_data.level = level;

		if (g_mcu_data->board == KHADAS_BOARD_VIM4 || g_mcu_data->board == KHADAS_BOARD_VIM1S) {
			if (level == 0)
				data = MCU_FAN_SPEED_OFF;
			else if (level == 1)
				data = MCU_FAN_SPEED_LOW_V2;
			else if (level == 2)
				data = MCU_FAN_SPEED_MID_V2;
			else if (level == 3)
				data = MCU_FAN_SPEED_HIGH_V2;
			ret = mcu_i2c_write_regs(g_mcu_data->client,
					MCU_CMD_FAN_STATUS_CTRL_REGv2,
					&data, 1);
			if (ret < 0) {
				pr_debug("write fan control err\n");
				return;
			}
		} else {
			if (level == 0)
				data = MCU_FAN_SPEED_OFF;
			else if (level == 1)
				data = MCU_FAN_SPEED_LOW;
			else if (level == 2)
				data = MCU_FAN_SPEED_MID;
			else if (level == 3)
				data = MCU_FAN_SPEED_HIGH;
			ret = mcu_i2c_write_regs(g_mcu_data->client,
					MCU_CMD_FAN_STATUS_CTRL_REG,
					&data, 1);
			if (ret < 0) {
				pr_debug("write fan control err\n");
				return;
			}
		}
	}
}

extern int meson_get_temperature(void);
static void fan_work_func(struct work_struct *_work)
{
	if (is_mcu_fan_control_supported()) {
		int temp = -EINVAL;
		struct mcu_fan_data *fan_data = &g_mcu_data->fan_data;

		if (g_mcu_data->board == KHADAS_BOARD_VIM3 ||
				g_mcu_data->board == KHADAS_BOARD_VIM4 ||
				g_mcu_data->board == KHADAS_BOARD_VIM1S)
			temp = meson_get_temperature();
		else
			temp = fan_data->trig_temp_level0;

		if (temp != -EINVAL) {
			if (temp < fan_data->trig_temp_level0)
				mcu_fan_level_set(fan_data, 0);
			else if (temp < fan_data->trig_temp_level1)
				mcu_fan_level_set(fan_data, 1);
			else if (temp < fan_data->trig_temp_level2)
				mcu_fan_level_set(fan_data, 2);
			else
				mcu_fan_level_set(fan_data, 3);
		}

		schedule_delayed_work(&fan_data->work, MCU_FAN_LOOP_SECS);
	}
}

static void khadas_fan_set(struct mcu_fan_data  *fan_data)
{
	if (is_mcu_fan_control_supported()) {
		cancel_delayed_work(&fan_data->work);
		if (fan_data->enable == MCU_FAN_STATUS_DISABLE) {
			mcu_fan_level_set(fan_data, 0);
			return;
		}
		switch (fan_data->mode) {
		case MCU_FAN_MODE_MANUAL:
			switch (fan_data->level) {
			case MCU_FAN_LEVEL_0:
				mcu_fan_level_set(fan_data, 0);
				break;
			case MCU_FAN_LEVEL_1:
				mcu_fan_level_set(fan_data, 1);
				break;
			case MCU_FAN_LEVEL_2:
				mcu_fan_level_set(fan_data, 2);
				break;
			case MCU_FAN_LEVEL_3:
				mcu_fan_level_set(fan_data, 3);
				break;
			default:
				break;
			}
			break;
		case MCU_FAN_MODE_AUTO:
			// FIXME: achieve with a better way
			schedule_delayed_work(&fan_data->work,
					MCU_FAN_LOOP_NODELAY_SECS);
			break;
		default:
			break;
		}
	}
}

static ssize_t show_fan_enable(struct class *cls,
			 struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "Fan enable: %d\n", g_mcu_data->fan_data.enable);
}

static ssize_t store_fan_enable(struct class *cls, struct class_attribute *attr,
		       const char *buf, size_t count)
{
	int enable;

	if (kstrtoint(buf, 0, &enable))
		return -EINVAL;

	// 0: manual, 1: auto
	if (enable >= 0 && enable < 2) {
		g_mcu_data->fan_data.enable = enable;
		khadas_fan_set(&g_mcu_data->fan_data);
	}

	return count;
}

static ssize_t show_fan_mode(struct class *cls,
			 struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "Fan mode: %d\n", g_mcu_data->fan_data.mode);
}

static ssize_t store_fan_mode(struct class *cls, struct class_attribute *attr,
		       const char *buf, size_t count)
{
	int mode;

	if (kstrtoint(buf, 0, &mode))
		return -EINVAL;

	// 0: manual, 1: auto
	if (mode >= 0 && mode < 2) {
		g_mcu_data->fan_data.mode = mode;
		khadas_fan_set(&g_mcu_data->fan_data);
	}

	return count;
}

static ssize_t show_fan_level(struct class *cls,
			 struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "Fan level: %d\n", g_mcu_data->fan_data.level);
}

static ssize_t store_fan_level(struct class *cls, struct class_attribute *attr,
		       const char *buf, size_t count)
{
	int level;

	if (kstrtoint(buf, 0, &level))
		return -EINVAL;

	if (level >= 0 && level < 4) {
		g_mcu_data->fan_data.level = level;
		khadas_fan_set(&g_mcu_data->fan_data);
	}

	return count;
}

static ssize_t show_fan_temp(struct class *cls,
			 struct class_attribute *attr, char *buf)
{
	struct mcu_fan_data *fan_data = &g_mcu_data->fan_data;
	int temp = -EINVAL;

	if (g_mcu_data->board == KHADAS_BOARD_VIM3 ||
			g_mcu_data->board == KHADAS_BOARD_VIM4 ||
			g_mcu_data->board == KHADAS_BOARD_VIM1S)
		temp = meson_get_temperature();
	else
		temp = fan_data->trig_temp_level0;

	return sprintf(buf,
			"cpu_temp:%d\nFan trigger temperature: level0:%d level1:%d level2:%d\n",
			temp, g_mcu_data->fan_data.trig_temp_level0,
			g_mcu_data->fan_data.trig_temp_level1,
			g_mcu_data->fan_data.trig_temp_level2);
}

void fan_level_set(struct mcu_data *ug_mcu_data)
{
	struct mcu_fan_data *fan_data = &g_mcu_data->fan_data;
	int temp = -EINVAL;

	if (ug_mcu_data->board == KHADAS_BOARD_VIM3 ||
			ug_mcu_data->board == KHADAS_BOARD_VIM4 ||
			ug_mcu_data->board == KHADAS_BOARD_VIM1S)
		temp = meson_get_temperature();
	else
		temp = fan_data->trig_temp_level0;

	if (temp != -EINVAL) {
		if (temp < ug_mcu_data->fan_data.trig_temp_level0)
			mcu_fan_level_set(fan_data, 0);
		else if (temp < ug_mcu_data->fan_data.trig_temp_level1)
			mcu_fan_level_set(fan_data, 1);
		else if (temp < ug_mcu_data->fan_data.trig_temp_level2)
			mcu_fan_level_set(fan_data, 2);
		else
			mcu_fan_level_set(fan_data, 3);
	}
}

static ssize_t show_fan_trigger_low(struct class *cls,
		struct class_attribute *attr, char *buf)
{
	return sprintf(buf,
			"Fan trigger low speed temperature:%d\n",
			g_mcu_data->fan_data.trig_temp_level0);
}

static ssize_t store_fan_trigger_low(struct class *cls,
		struct class_attribute *attr,
		const char *buf, size_t count)
{
	int trigger;

	if (kstrtoint(buf, 0, &trigger))
		return -EINVAL;

	if (trigger >= g_mcu_data->fan_data.trig_temp_level1) {
		pr_err("Invalid parameter\n");
		return -EINVAL;
	}

	g_mcu_data->fan_data.trig_temp_level0 = trigger;

	fan_level_set(g_mcu_data);

	return count;
}

static ssize_t show_fan_trigger_mid(struct class *cls,
		struct class_attribute *attr, char *buf)
{
	return sprintf(buf,
			"Fan trigger mid speed temperature:%d\n",
			g_mcu_data->fan_data.trig_temp_level1);
}

static ssize_t store_fan_trigger_mid(struct class *cls,
		struct class_attribute *attr,
		const char *buf, size_t count)
{
	int trigger;

	if (kstrtoint(buf, 0, &trigger))
		return -EINVAL;

	if (trigger >= g_mcu_data->fan_data.trig_temp_level2 ||
			trigger <= g_mcu_data->fan_data.trig_temp_level0){
		pr_err("Invalid parameter\n");
		return -EINVAL;
	}

	g_mcu_data->fan_data.trig_temp_level1 = trigger;

	fan_level_set(g_mcu_data);

	return count;
}

static ssize_t show_fan_trigger_high(struct class *cls,
		struct class_attribute *attr, char *buf)
{
	return sprintf(buf,
			"Fan trigger high speed temperature:%d\n",
			g_mcu_data->fan_data.trig_temp_level2);
}

static ssize_t store_fan_trigger_high(struct class *cls,
		struct class_attribute *attr,
		const char *buf, size_t count)
{
	int trigger;

	if (kstrtoint(buf, 0, &trigger))
		return -EINVAL;

	if (trigger <= g_mcu_data->fan_data.trig_temp_level1) {
		pr_err("Invalid parameter\n");
		return -EINVAL;
	}

	g_mcu_data->fan_data.trig_temp_level2 = trigger;

	fan_level_set(g_mcu_data);

	return count;
}

static ssize_t store_mcu_poweroff(struct class *cls,
		struct class_attribute *attr,
		const char *buf, size_t count)
{
	int ret;
	int val;
	char reg;

	if (kstrtoint(buf, 0, &val))
		return -EINVAL;

	if (val != 1)
		return -EINVAL;

	reg = (char)val;
	ret = mcu_i2c_write_regs(g_mcu_data->client,
			MCU_PWR_OFF_CMD_REG,
			&reg, 1);
	if (ret < 0) {
		pr_debug("write poweroff cmd error\n");

		return ret;
	}

	return count;
}

static ssize_t store_mcu_rst(struct class *cls, struct class_attribute *attr,
		const char *buf, size_t count)
{
	char reg;
	int ret;
	int rst;

	if (kstrtoint(buf, 0, &rst))
		return -EINVAL;

	reg = rst;
	ret = mcu_i2c_write_regs(g_mcu_data->client,
			MCU_SHUTDOWN_NORMAL_REG,
			&reg, 1);
	if (ret < 0)
		pr_debug("rst mcu err\n");

	return count;
}

static struct class_attribute fan_class_attrs[] = {
	__ATTR(enable, 0644, show_fan_enable, store_fan_enable),
	__ATTR(mode, 0644, show_fan_mode, store_fan_mode),
	__ATTR(level, 0644, show_fan_level, store_fan_level),
	__ATTR(trigger_temp_low, 0644,
			show_fan_trigger_low, store_fan_trigger_low),
	__ATTR(trigger_temp_mid, 0644,
			show_fan_trigger_mid, store_fan_trigger_mid),
	__ATTR(trigger_temp_high, 0644,
			show_fan_trigger_high, store_fan_trigger_high),
	__ATTR(temp, 0644, show_fan_temp, NULL),
};

static ssize_t store_wol_enable(struct class *cls, struct class_attribute *attr,
								const char *buf, size_t count)
{
	u8 reg[2];
	int ret;
	int enable;
	int state;

	if (kstrtoint(buf, 0, &enable))
		return -EINVAL;

	ret = mcu_i2c_read_regs(g_mcu_data->client, MCU_BOOT_EN_WOL_REG,
					reg, 1);
	if (ret < 0) {
		printk("write wol state err\n");
		return ret;
	}
	state = (int)reg[0];
	reg[0] = enable | (state & 0x02);
	ret = mcu_i2c_write_regs(g_mcu_data->client, MCU_BOOT_EN_WOL_REG,
								reg, 1);
	if (ret < 0) {
		printk("write wol state err\n");
		return ret;
	}

	g_mcu_data->wol_enable = reg[0];
//	mcu_enable_wol(g_mcu_data->wol_enable, false);

	printk("write wol state: %d\n", g_mcu_data->wol_enable);
	return count;
}

static ssize_t show_wol_enable(struct class *cls,
				struct class_attribute *attr, char *buf)
{
	int enable;

	enable = g_mcu_data->wol_enable & 0x01;
	return sprintf(buf, "%d\n", enable);
}

static struct class_attribute wol_class_attrs[] = {
	__ATTR(enable, 0644, show_wol_enable, store_wol_enable),
};

static ssize_t store_usb_pcie_switch_mode(struct class *cls, struct class_attribute *attr,
				const char *buf, size_t count)
{
	int ret;
	int mode;

	if (kstrtoint(buf, 0, &mode))
		return -EINVAL;

	if (0 != mode && 1 != mode)
		return -EINVAL;

	if ((mode < MCU_USB_PCIE_SWITCH_MODE_USB3) || (mode > MCU_USB_PCIE_SWITCH_MODE_PCIE))
		return -EINVAL;

	g_mcu_data->usb_pcie_switch_mode = (u8)mode;
	ret = mcu_i2c_write_regs(g_mcu_data->client, MCU_USB_PCIE_SWITCH_REG, &g_mcu_data->usb_pcie_switch_mode, 1);
	if (ret < 0) {
		printk("write USB PCIe switch error\n");
		return ret;
	};

	printk("Set USB PCIe Switch Mode: %s\n", g_mcu_data->usb_pcie_switch_mode ? "PCIe" : "USB3.0");
	return count;
};

static ssize_t show_usb_pcie_switch_mode(struct class *cls,
				struct class_attribute *attr, char *buf)
{
	printk("USB PCIe Switch Mode: %s\n", g_mcu_data->usb_pcie_switch_mode ? "PCIe" : "USB3.0");
	return sprintf(buf, "%d\n", g_mcu_data->usb_pcie_switch_mode);
}

static struct class_attribute mcu_class_attrs[] = {
	__ATTR(usb_pcie_switch_mode, 0644, show_usb_pcie_switch_mode, store_usb_pcie_switch_mode),
	__ATTR(poweroff, 0644, NULL, store_mcu_poweroff),
	__ATTR(rst, 0644, NULL, store_mcu_rst),
};

static void create_mcu_attrs(void)
{
	int i;

	if (is_mcu_wol_supported()) {
			g_mcu_data->wol_class = class_create(THIS_MODULE, "wol");
			if (IS_ERR(g_mcu_data->wol_class)) {
					pr_err("create wol_class debug class fail\n");
					return;
			}
			for (i = 0; i < ARRAY_SIZE(wol_class_attrs); i++) {
				if (class_create_file(g_mcu_data->wol_class, &wol_class_attrs[i]))
					pr_err("create wol attribute %s fail\n", wol_class_attrs[i].attr.name);
			}
	}

	g_mcu_data->mcu_class = class_create(THIS_MODULE, "mcu");
	if (IS_ERR(g_mcu_data->mcu_class)) {
		pr_err("create mcu_class debug class fail\n");
		return;
	}

	for (i = 0; i < ARRAY_SIZE(mcu_class_attrs); i++) {
		if (strstr(mcu_class_attrs[i].attr.name,
					"usb_pcie_switch_mode")) {
			if (!is_mcu_usb_pcie_switch_supported())
				continue;
		}

		if (class_create_file(g_mcu_data->mcu_class,
					&mcu_class_attrs[i]))
			pr_err("create mcu attribute %s fail\n",
					mcu_class_attrs[i].attr.name);
	}

	if (is_mcu_fan_control_supported()) {
		g_mcu_data->fan_data.fan_class =
			class_create(THIS_MODULE, "fan");
		if (IS_ERR(g_mcu_data->fan_data.fan_class)) {
			pr_err("create fan_class debug class fail\n");
			return;
		}

		for (i = 0; i < ARRAY_SIZE(fan_class_attrs); i++) {
			if (class_create_file(g_mcu_data->fan_data.fan_class,
						&fan_class_attrs[i]))
				pr_err("create fan attribute %s fail\n",
						fan_class_attrs[i].attr.name);
		}
	}
}

static int mcu_parse_dt(struct device *dev)
{
	int ret;
	const char *hwver = NULL;

	if (!dev)
		return -EINVAL;

	// Get hardwere version
	ret = of_property_read_string(dev->of_node, "hwver", &hwver);
	if (ret < 0) {
		g_mcu_data->hwver = KHADAS_BOARD_HWVER_V12;
		g_mcu_data->board = KHADAS_BOARD_VIM2;
	} else {
		if (strstr(hwver, "VIM1S"))
			g_mcu_data->board = KHADAS_BOARD_VIM1S;
		else if (strstr(hwver, "VIM2"))
			g_mcu_data->board = KHADAS_BOARD_VIM2;
		else if (strstr(hwver, "VIM3"))
			g_mcu_data->board = KHADAS_BOARD_VIM3;
		else if (strstr(hwver, "VIM4"))
			g_mcu_data->board = KHADAS_BOARD_VIM4;
		else if (strstr(hwver, "VIM1"))
			g_mcu_data->board = KHADAS_BOARD_VIM1;
		else
			g_mcu_data->board = KHADAS_BOARD_NONE;

		if (g_mcu_data->board == KHADAS_BOARD_VIM1) {
			if (strcmp(hwver, "VIM1.V13") == 0)
				g_mcu_data->hwver = KHADAS_BOARD_HWVER_V13;
			else if (strcmp(hwver, "VIM1.V14") == 0)
				g_mcu_data->hwver = KHADAS_BOARD_HWVER_V14;
			else
				g_mcu_data->hwver = KHADAS_BOARD_HWVER_NONE;
		} else if (g_mcu_data->board == KHADAS_BOARD_VIM2) {
			if (strcmp(hwver, "VIM2.V12") == 0)
				g_mcu_data->hwver = KHADAS_BOARD_HWVER_V12;
			else if (strcmp(hwver, "VIM2.V13") == 0)
				g_mcu_data->hwver = KHADAS_BOARD_HWVER_V13;
			else if (strcmp(hwver, "VIM2.V14") == 0)
				g_mcu_data->hwver = KHADAS_BOARD_HWVER_V14;
			else
				g_mcu_data->hwver = KHADAS_BOARD_HWVER_NONE;
		} else if (g_mcu_data->board == KHADAS_BOARD_VIM3) {
			if (strcmp(hwver, "VIM3.V11") == 0)
				g_mcu_data->hwver = KHADAS_BOARD_HWVER_V11;
			else if (strcmp(hwver, "VIM3.V12") == 0)
				g_mcu_data->hwver = KHADAS_BOARD_HWVER_V12;
			else if (strcmp(hwver, "VIM3.V13") == 0)
				g_mcu_data->hwver = KHADAS_BOARD_HWVER_V13;
			else if (strcmp(hwver, "VIM3.V14") == 0)
				g_mcu_data->hwver = KHADAS_BOARD_HWVER_V14;
			else
				g_mcu_data->hwver = KHADAS_BOARD_HWVER_NONE;
		} else if (g_mcu_data->board == KHADAS_BOARD_VIM4) {
			if (strcmp(hwver, "VIM4.V11") == 0)
				g_mcu_data->hwver = KHADAS_BOARD_HWVER_V11;
			else if (strcmp(hwver, "VIM4.V12") == 0)
				g_mcu_data->hwver = KHADAS_BOARD_HWVER_V12;
		} else if (g_mcu_data->board == KHADAS_BOARD_VIM1S) {
			if (strcmp(hwver, "VIM1S.V10") == 0)
				g_mcu_data->hwver = KHADAS_BOARD_HWVER_V10;
		}
	}

	ret = of_property_read_u32(dev->of_node,
			"fan,trig_temp_level0",
			&g_mcu_data->fan_data.trig_temp_level0);
	if (ret < 0)
		g_mcu_data->fan_data.trig_temp_level0 =
			MCU_FAN_TRIG_TEMP_LEVEL0;
	ret = of_property_read_u32(dev->of_node,
			"fan,trig_temp_level1",
			&g_mcu_data->fan_data.trig_temp_level1);
	if (ret < 0)
		g_mcu_data->fan_data.trig_temp_level1 =
			MCU_FAN_TRIG_TEMP_LEVEL1;
	ret = of_property_read_u32(dev->of_node,
			"fan,trig_temp_level2",
			&g_mcu_data->fan_data.trig_temp_level2);
	if (ret < 0)
		g_mcu_data->fan_data.trig_temp_level2 =
			MCU_FAN_TRIG_TEMP_LEVEL2;

	return ret;
}

static int mcu_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	u8 reg[2];
	int ret;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
		return -ENODEV;

	g_mcu_data = kzalloc(sizeof(*g_mcu_data), GFP_KERNEL);

	if (!g_mcu_data)
		return -ENOMEM;

	mcu_parse_dt(&client->dev);

	pr_debug("%s: board: %d, hwver: %d\n",
			__func__,
			(int)g_mcu_data->board,
			(int)g_mcu_data->hwver);

	g_mcu_data->client = client;
	ret = mcu_i2c_read_regs(client, MCU_BOOT_EN_WOL_REG, reg, 1);
	if (ret < 0)
		goto exit;

	g_mcu_data->wol_enable = (int)reg[0] & 0x01;
	//printk("hlm wol_enable=%d\n", g_mcu_data->wol_enable);
/*	if (g_mcu_data->wol_enable)
                mcu_enable_wol(g_mcu_data->wol_enable, false);*/

	if (is_mcu_usb_pcie_switch_supported()) {
		ret = mcu_i2c_read_regs(client, MCU_USB_PCIE_SWITCH_REG, reg, 1);
		if (ret < 0)
			goto exit;
		g_mcu_data->usb_pcie_switch_mode = (u8)reg[0];
	}

	if (is_mcu_fan_control_supported()) {
		g_mcu_data->fan_data.mode = MCU_FAN_MODE_AUTO;
		g_mcu_data->fan_data.level = MCU_FAN_LEVEL_0;
		g_mcu_data->fan_data.enable = MCU_FAN_STATUS_ENABLE;

		INIT_DELAYED_WORK(&g_mcu_data->fan_data.work, fan_work_func);
		mcu_fan_level_set(&g_mcu_data->fan_data, 0);
		schedule_delayed_work(&g_mcu_data->fan_data.work, MCU_FAN_LOOP_SECS);
	}
	create_mcu_attrs();

	return 0;
exit:
	kfree(g_mcu_data);
	return ret;
}

static int mcu_remove(struct i2c_client *client)
{
	return 0;
}

static void khadas_fan_shutdown(struct i2c_client *client)
{
	g_mcu_data->fan_data.enable = MCU_FAN_STATUS_DISABLE;
	khadas_fan_set(&g_mcu_data->fan_data);
}

#ifdef CONFIG_PM_SLEEP
static int khadas_fan_suspend(struct device *dev)
{
	int ret;
	u8 data = MCU_FAN_SPEED_OFF;

	cancel_delayed_work(&g_mcu_data->fan_data.work);
	if (g_mcu_data->board == KHADAS_BOARD_VIM4 || g_mcu_data->board == KHADAS_BOARD_VIM1S){
		ret = mcu_i2c_write_regs(g_mcu_data->client,
				MCU_CMD_FAN_STATUS_CTRL_REGv2,
				&data, 1);
		if (ret < 0) {
			pr_debug("write fan control err\n");
		}
	} else {
		ret = mcu_i2c_write_regs(g_mcu_data->client,
				MCU_CMD_FAN_STATUS_CTRL_REG,
				&data, 1);
		if (ret < 0) {
			pr_debug("write fan control err\n");
		}
	}

	return 0;
}

static int khadas_fan_resume(struct device *dev)
{
	khadas_fan_set(&g_mcu_data->fan_data);

	return 0;
}

static const struct dev_pm_ops fan_dev_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(khadas_fan_suspend, khadas_fan_resume)
};

#define FAN_PM_OPS (&(fan_dev_pm_ops))

#endif

static const struct i2c_device_id mcu_id[] = {
	{ "khadas-mcu", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, mcu_id);

const struct of_device_id mcu_dt_ids[] = {
	{ .compatible = "khadas-mcu" },
	{},
};
MODULE_DEVICE_TABLE(of, mcu_dt_ids);

struct i2c_driver mcu_driver = {
	.driver  = {
		.name   = "khadas-mcu",
		.owner  = THIS_MODULE,
		.of_match_table = of_match_ptr(mcu_dt_ids),
#ifdef CONFIG_PM_SLEEP
		.pm = FAN_PM_OPS,
#endif
	},
	.probe      = mcu_probe,
	.remove     = mcu_remove,
	.shutdown   = khadas_fan_shutdown,
	.id_table   = mcu_id,
};
module_i2c_driver(mcu_driver);

MODULE_AUTHOR("Yan <yan-wyb@foxmail.com>");
MODULE_DESCRIPTION("Khadas MCU control driver");
MODULE_LICENSE("GPL");
