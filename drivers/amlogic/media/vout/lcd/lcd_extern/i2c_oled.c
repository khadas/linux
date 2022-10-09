// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>
#include <linux/compat.h>
#include <linux/i2c.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/sched/clock.h>
#include <linux/amlogic/media/vout/lcd/lcd_extern.h>
#include "lcd_extern.h"
#include "i2c_oled.h"

#define LCD_EXTERN_NAME     "ext_oled"

static struct lcd_ext_oled_s *ext_oled;
static struct mutex ext_oled_mutex;
static struct mutex ext_oled_power_mutex;

static int ext_oled_reg_read(struct lcd_extern_driver_s *edrv,
			     struct lcd_extern_dev_s *edev,
			     unsigned short reg, unsigned char *rbuf, unsigned char rlen)
{
	struct lcd_extern_i2c_dev_s *i2c_dev;
	unsigned char reg_buf[2];
	int ret = 0;

	if (!rbuf) {
		EXTERR("%s: rbuf is null\n", __func__);
		return -1;
	}
	if (edev->addr_sel >= 2) {
		EXTERR("%s: invalid addr_sel %d\n", __func__, edev->addr_sel);
		return -1;
	}
	i2c_dev = edev->i2c_dev[edev->addr_sel];
	if (!i2c_dev) {
		EXTERR("%s: i2c_dev[%d] is null\n", __func__, edev->addr_sel);
		return -1;
	}

	reg_buf[0] = (reg >> 8) & 0xff;
	reg_buf[1] = reg & 0xff;
	ret = lcd_extern_i2c_read(i2c_dev->client, reg_buf, 2, rbuf, rlen);
	return ret;
}

static int ext_oled_reg_write(struct lcd_extern_driver_s *edrv,
				struct lcd_extern_dev_s *edev,
				unsigned char *buf, unsigned int len)
{
	struct lcd_extern_i2c_dev_s *i2c_dev;
	int ret = 0;

	if (!buf) {
		EXTERR("%s: buf is full\n", __func__);
		return -1;
	}
	if (len == 0) {
		EXTERR("%s: invalid len\n", __func__);
		return -1;
	}
	if (edev->addr_sel >= 2) {
		EXTERR("%s: invalid addr_sel %d\n", __func__, edev->addr_sel);
		return -1;
	}
	i2c_dev = edev->i2c_dev[edev->addr_sel];
	if (!i2c_dev) {
		EXTERR("%s: i2c_dev[%d] is null\n", __func__, edev->addr_sel);
		return -1;
	}

	ret = lcd_extern_i2c_write(i2c_dev->client, buf, len);
	return ret;
}

static int ext_oled_temperature_get(struct lcd_extern_driver_s *edrv,
				    struct lcd_extern_dev_s *edev,
				    int flag)
{
	unsigned short addr, val;
	unsigned char rbuf[2];
	int ret;

	if (flag)
		addr = 0x5e;
	else
		addr = 0x60;

	ret = ext_oled_reg_read(edrv, edev, addr, rbuf, 2);
	if (ret) {
		EXTERR("%s: error\n", __func__);
		return 0;
	}

	val = (rbuf[0] << 8) | rbuf[1];
	return val;
}

static int ext_oled_off_rs_set(struct lcd_extern_driver_s *edrv,
			       struct lcd_extern_dev_s *edev,
			       int flag)
{
	unsigned short addr;
	unsigned char val, buf[3];
	int ret;

	addr = 0x01;

	ret = ext_oled_reg_read(edrv, edev, addr, &val, 1);
	if (ret) {
		EXTERR("%s: error\n", __func__);
		return 0;
	}

	if (flag) {
		val |= (1 << 4);
		edev->state |= EXT_OLED_STATE_OFF_RS_EN;
	} else {
		val &= ~(1 << 4);
		edev->state &= ~EXT_OLED_STATE_OFF_RS_EN;
	}
	buf[0] = (addr >> 8) & 0xff;
	buf[1] = addr & 0xff;
	buf[2] = val;
	ret = ext_oled_reg_write(edrv, edev, buf, 3);
	EXTPR("%s: %d %s\n", __func__, flag, ret ? "fail" : "ok");
	return ret;
}

static int ext_oled_jb_set(struct lcd_extern_driver_s *edrv,
			   struct lcd_extern_dev_s *edev,
			   int flag)
{
	unsigned short addr;
	unsigned char val, buf[3];
	int ret;

	addr = 0x5d;

	ret = ext_oled_reg_read(edrv, edev, addr, &val, 1);
	if (ret) {
		EXTERR("%s: error\n", __func__);
		return 0;
	}

	if (flag) {
		val |= (1 << 7);
		edev->state |= EXT_OLED_STATE_JB_EN;
	} else {
		val &= ~(1 << 7);
		edev->state &= ~EXT_OLED_STATE_JB_EN;
	}
	buf[0] = (addr >> 8) & 0xff;
	buf[1] = addr & 0xff;
	buf[2] = val;
	ret = ext_oled_reg_write(edrv, edev, buf, 3);
	EXTPR("%s: %d %s\n", __func__, flag, ret ? "fail" : "ok");
	return ret;
}

static int ext_oled_hdr_set(struct lcd_extern_driver_s *edrv,
			    struct lcd_extern_dev_s *edev,
			    int flag)
{
	unsigned short addr;
	unsigned char val, buf[3];
	int ret;

	addr = 0x0b;

	ret = ext_oled_reg_read(edrv, edev, addr, &val, 1);
	if (ret) {
		EXTERR("%s: error\n", __func__);
		return 0;
	}

	if (flag) {
		val |= (1 << 3);
		edev->state |= EXT_OLED_STATE_HDR_EN;
	} else {
		val &= ~(1 << 3);
		edev->state &= ~EXT_OLED_STATE_HDR_EN;
	}
	buf[0] = (addr >> 8) & 0xff;
	buf[1] = addr & 0xff;
	buf[2] = val;
	ret = ext_oled_reg_write(edrv, edev, buf, 3);
	EXTPR("%s: %d %s\n", __func__, flag, ret ? "fail" : "ok");
	return ret;
}

static int ext_oled_orbit_set(struct lcd_extern_driver_s *edrv,
				struct lcd_extern_dev_s *edev,
				int flag)
{
	unsigned short addr;
	unsigned char buf[5];
	int ret;

	addr = 0x36;

	ret = ext_oled_reg_read(edrv, edev, addr, &buf[2], 3);
	if (ret) {
		EXTERR("%s: error\n", __func__);
		return 0;
	}

	if (flag) {
		buf[2] |= (1 << 7);
		edev->state |= EXT_OLED_STATE_ORBIT_EN;
	} else {
		buf[2] &= ~(1 << 7);
		edev->state &= ~EXT_OLED_STATE_ORBIT_EN;
	}
	buf[0] = (addr >> 8) & 0xff;
	buf[1] = addr & 0xff;
	ret = ext_oled_reg_write(edrv, edev, buf, 5);
	EXTPR("%s: %d %s\n", __func__, flag, ret ? "fail" : "ok");
	return ret;
}

static unsigned int ext_oled_operate_time_get_ms(void)
{
	unsigned long long cur_time, duration;
	unsigned int time_ms;

	if (!ext_oled) {
		EXTERR("%s: ext_oled is null\n", __func__);
		return 0;
	}

	cur_time = sched_clock();
	duration = cur_time - ext_oled->power_on_time;
	time_ms = lcd_do_div(duration, 1000000);
	return time_ms;
}

static int ext_oled_work_process(struct lcd_extern_driver_s *edrv,
				 struct lcd_extern_dev_s *edev)
{
	//todo
	return 0;
}

static int lcd_extern_power_cmd_dynamic_size(struct lcd_extern_driver_s *edrv,
					     struct lcd_extern_dev_s *edev,
					     unsigned char *table, int flag)
{
	int i = 0, j, step = 0, max_len = 0;
	unsigned char type, cmd_size;
	int delay_ms, ret = 0;

	if (flag)
		max_len = edev->config.table_init_on_cnt;
	else
		max_len = edev->config.table_init_off_cnt;

	while ((i + 1) < max_len) {
		type = table[i];
		if (type == LCD_EXT_CMD_TYPE_END)
			break;
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
			EXTPR("%s: step %d: type=0x%02x, cmd_size=%d\n",
			      __func__, step, type, table[i + 1]);
		}
		cmd_size = table[i + 1];
		if (cmd_size == 0)
			goto power_cmd_dynamic_next;
		if ((i + 2 + cmd_size) > max_len)
			break;

		if (type == LCD_EXT_CMD_TYPE_NONE) {
			/* do nothing */
		} else if (type == LCD_EXT_CMD_TYPE_GPIO) {
			if (cmd_size < 2) {
				EXTERR("step %d: invalid cmd_size %d for GPIO\n",
				       step, cmd_size);
				goto power_cmd_dynamic_next;
			}
			if (table[i + 2] < LCD_GPIO_MAX)
				lcd_extern_gpio_set(edrv, table[i + 2], table[i + 3]);
			if (cmd_size > 2) {
				if (table[i + 4] > 0)
					lcd_delay_ms(table[i + 4]);
			}
		} else if (type == LCD_EXT_CMD_TYPE_DELAY) {
			delay_ms = 0;
			for (j = 0; j < cmd_size; j++)
				delay_ms += table[i + 2 + j];
			if (delay_ms > 0)
				lcd_delay_ms(delay_ms);
		} else if (type == LCD_EXT_CMD_TYPE_CMD) {
			edev->addr_sel = 0;
			ret = ext_oled_reg_write(edrv, edev, &table[i + 2], cmd_size);
		} else if (type == LCD_EXT_CMD_TYPE_CMD2) {
			edev->addr_sel = 1;
			ret = ext_oled_reg_write(edrv, edev, &table[i + 2], cmd_size);
		} else {
			EXTERR("%s: %s(%d): type 0x%02x invalid\n",
			       __func__, edev->config.name,
			       edev->config.index, type);
		}
power_cmd_dynamic_next:
		i += (cmd_size + 2);
		step++;
	}

	return ret;
}

static int lcd_extern_power_ctrl(struct lcd_extern_driver_s *edrv,
				 struct lcd_extern_dev_s *edev, int flag)
{
	unsigned char *table;
	unsigned char cmd_size;
	int ret = 0;

	cmd_size = edev->config.cmd_size;
	if (flag)
		table = edev->config.table_init_on;
	else
		table = edev->config.table_init_off;
	if (!table) {
		EXTERR("%s: init_table %d is NULL\n", __func__, flag);
		return -1;
	}
	if (cmd_size != LCD_EXT_CMD_SIZE_DYNAMIC) {
		EXTERR("%s: cmd_size %d is invalid\n", __func__, cmd_size);
		return -1;
	}

	ret = lcd_extern_power_cmd_dynamic_size(edrv, edev, table, flag);
	EXTPR("%s: %s(%d): %d\n",
	      __func__, edev->config.name, edev->config.index, flag);
	return ret;
}

static int ext_oled_power_on(struct lcd_extern_driver_s *edrv,
			     struct lcd_extern_dev_s *edev)
{
	int ret;

	if (!ext_oled) {
		EXTERR("%s: ext_oled is null\n", __func__);
		return -1;
	}

	mutex_lock(&ext_oled_power_mutex);
	ret = lcd_extern_power_ctrl(edrv, edev, 1);
	ext_oled->power_on_time = sched_clock();
	edev->state |= EXT_OLED_STATE_ON;
	mutex_unlock(&ext_oled_power_mutex);

	return ret;
}

static int ext_oled_power_off(struct lcd_extern_driver_s *edrv,
			      struct lcd_extern_dev_s *edev)
{
	int ret;

	if (!ext_oled) {
		EXTERR("%s: ext_oled is null\n", __func__);
		return -1;
	}

	mutex_lock(&ext_oled_power_mutex);
	edev->state &= ~EXT_OLED_STATE_ON;
	ret = lcd_extern_power_ctrl(edrv, edev, 0);
	mutex_unlock(&ext_oled_power_mutex);

	return ret;
}

static int ext_oled_init(struct lcd_extern_driver_s *edrv,
			    struct lcd_extern_dev_s *edev)
{
	if (!ext_oled) {
		EXTERR("%s: ext_oled is null\n", __func__);
		return -1;
	}

	mutex_lock(&ext_oled_power_mutex);
	EXTPR("%s: %s(%d)\n",
	      __func__, edev->config.name, edev->config.index);

	ext_oled->power_on_time = sched_clock();
	edev->state |= EXT_OLED_STATE_ON;
	mutex_unlock(&ext_oled_power_mutex);

	ext_oled_work_process(edrv, edev);
	return 0;
}

/* ************************************************************* */
static int ext_oled_get_config_dts(struct lcd_extern_driver_s *edrv,
				   struct lcd_extern_dev_s *edev)
{
	//todo
	return 0;
}

static int ext_oled_get_config_ukey(struct lcd_extern_driver_s *edrv,
				    struct lcd_extern_dev_s *edev)
{
	//todo
	return 0;
}

static int ext_oled_get_config(struct lcd_extern_driver_s *edrv, struct lcd_extern_dev_s *edev)
{
	int ret;

	if (edrv->config_load)
		ret = ext_oled_get_config_ukey(edrv, edev);
	else
		ret = ext_oled_get_config_dts(edrv, edev);

	return ret;
}

/* ************************************************************* */
static const char *ext_oled_debug_usage_str = {
"Usage:\n"
"    echo test <on/off> > debug ; test power on/off for extern device\n"
"        <on/off>: 1 for power on, 0 for power off\n"
"    echo r <addr_sel> <reg> > debug ; read reg for extern device\n"
"    echo d <addr_sel> <reg> <cnt> > debug ; dump regs for extern device\n"
"    echo w <addr_sel> <reg> <value> > debug ; write reg for extern device\n"
};

static ssize_t ext_oled_debug_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", ext_oled_debug_usage_str);
}

static ssize_t ext_oled_debug_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct lcd_extern_driver_s *edrv = dev_get_drvdata(dev);
	struct lcd_extern_dev_s *edev = NULL;
	unsigned int ret, i;
	unsigned int val[3];
	unsigned short reg;
	unsigned char value, *i2c_buf;

	if (!ext_oled || !edrv) {
		pr_err("%s: ext_oled drv is null\n", __func__);
		return count;
	}

	edev = ext_oled->ext_dev;
	if (!edev) {
		pr_err("%s: ext_oled edev is null\n", __func__);
		return count;
	}

	mutex_lock(&ext_oled_mutex);
	switch (buf[0]) {
	case 'i':
		ext_oled_init(edrv, edev);
		break;
	case 't':
		if (buf[1] == 'e') {
			if (buf[2] == 's') { //test
				ret = sscanf(buf, "test %d", &val[0]);
				if (ret == 1) {
					if (val[0])
						ext_oled_power_on(edrv, edev);
					else
						ext_oled_power_off(edrv, edev);
				} else {
					goto ext_oled_debug_store_err;
				}
			} else if (buf[2] == 'm') { //temp
				ret = sscanf(buf, "temp %d", &val[0]);
				if (ret == 1) {
					val[1] = ext_oled_temperature_get(edrv, edev, val[0]);
					pr_info("temperature[%d]: %d\n", val[0], val[1]);
				} else {
					goto ext_oled_debug_store_err;
				}
			} else {
				goto ext_oled_debug_store_err;
			}
		} else if (buf[1] == 'i') { //time
			val[0] = ext_oled_operate_time_get_ms();
			pr_info("operate_time: %dms\n", val[0]);
		} else {
			goto ext_oled_debug_store_err;
		}
		break;
	case 'r':
		ret = sscanf(buf, "r %d %x", &val[0], &val[1]);
		if (ret == 2) {
			edev->addr_sel = (unsigned char)val[0];
			reg = (unsigned short)val[1];
			ext_oled_reg_read(edrv, edev, reg, &value, 1);
			pr_info("reg read: 0x%03x = 0x%02x\n", reg, value);
		} else {
			goto ext_oled_debug_store_err;
		}
		break;
	case 'd':
		ret = sscanf(buf, "d %d %x %d", &val[0], &val[1], &val[2]);
		if (ret == 3) {
			edev->addr_sel = (unsigned char)val[0];
			reg = (unsigned short)val[1];
			if (val[2] == 0) {
				pr_info("invalid reg dump len 0\n");
				goto ext_oled_debug_store_err_no_message;
			}

			i2c_buf = kzalloc(val[2], GFP_KERNEL);
			if (!i2c_buf)
				goto ext_oled_debug_store_err_no_message;
			ext_oled_reg_read(edrv, edev, reg, i2c_buf, val[2]);
			pr_info("reg dump:\n");
			for (i = 0; i < val[2]; i++)
				pr_info("  0x%02x = 0x%02x\n", reg + i, i2c_buf[i]);
			kfree(i2c_buf);
		} else {
			goto ext_oled_debug_store_err;
		}
		break;
	case 'w':
		ret = sscanf(buf, "w %d %x %x", &val[0], &val[1], &val[2]);
		if (ret == 3) {
			edev->addr_sel = (unsigned char)val[0];
			reg = (unsigned short)val[1];
			value = (unsigned char)val[2];

			i2c_buf = kzalloc(3, GFP_KERNEL);
			if (!i2c_buf)
				goto ext_oled_debug_store_err_no_message;
			i2c_buf[1] = (unsigned char)(val[1] & 0xff);
			i2c_buf[0] = (unsigned char)((val[1] >> 8) & 0xff);
			i2c_buf[2] =  (unsigned char)val[2];
			ext_oled_reg_write(edrv, edev, i2c_buf, 3);
			ext_oled_reg_read(edrv, edev, reg, &value, 1);
			pr_info("reg write 0x%03x = 0x%02x, readback: 0x%02x\n",
				reg, val[2], value);
			kfree(i2c_buf);
		} else {
			goto ext_oled_debug_store_err;
		}
		break;
	case 'o': //orbit
		if (buf[1] == 'r') { //orbit
			ret = sscanf(buf, "orbit %d", &val[0]);
			if (ret == 1)
				ext_oled_orbit_set(edrv, edev, val[0]);
			else
				goto ext_oled_debug_store_err;
		} else if (buf[1] == 'f') { //offrs
			ret = sscanf(buf, "offrs %d", &val[0]);
			if (ret == 1)
				ext_oled_off_rs_set(edrv, edev, val[0]);
			else
				goto ext_oled_debug_store_err;
		} else {
			goto ext_oled_debug_store_err;
		}
		break;
	case 'j': //jb
		ret = sscanf(buf, "jb %d", &val[0]);
		if (ret == 1)
			ext_oled_jb_set(edrv, edev, val[0]);
		else
			goto ext_oled_debug_store_err;
		break;
	case 'h': //hdr
		ret = sscanf(buf, "hdr %d", &val[0]);
		if (ret == 1)
			ext_oled_hdr_set(edrv, edev, val[0]);
		else
			goto ext_oled_debug_store_err;
		break;
	default:
		pr_info("invalid data\n");
		break;
	}
	mutex_unlock(&ext_oled_mutex);

	return count;

ext_oled_debug_store_err:
	pr_info("invalid data\n");
ext_oled_debug_store_err_no_message:
	mutex_unlock(&ext_oled_mutex);
	return -EINVAL;
}

static ssize_t ext_oled_info_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct lcd_extern_dev_s *edev = NULL;
	ssize_t len;

	if (!ext_oled)
		return sprintf(buf, "ext_oled drv is null\n");

	edev = ext_oled->ext_dev;
	if (!edev)
		return sprintf(buf, "ext_oled edev is null\n");

	mutex_lock(&ext_oled_mutex);
	len = sprintf(buf, "i2c_oled info:\n");
	len += sprintf(buf + len,
		"i2c addr:     0x%02x\n"
		"i2c addr2:    0x%02x\n"
		"state:        0x%x\n",
		edev->config.i2c_addr,
		edev->config.i2c_addr2,
		edev->state);
	mutex_unlock(&ext_oled_mutex);

	return len;
}

static struct device_attribute ext_oled_debug_attrs[] = {
	__ATTR(debug, 0644, ext_oled_debug_show, ext_oled_debug_store),
	__ATTR(info, 0444, ext_oled_info_show, NULL)
};

static int ext_oled_debug_file_add(struct device *dev)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(ext_oled_debug_attrs); i++) {
		if (device_create_file(dev, &ext_oled_debug_attrs[i])) {
			EXTERR("%s: create debug attribute %s fail\n",
			       __func__, ext_oled_debug_attrs[i].attr.name);
		}
	}

	return 0;
}

static int ext_oled_debug_file_remove(struct device *dev)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(ext_oled_debug_attrs); i++)
		device_remove_file(dev, &ext_oled_debug_attrs[i]);

	return 0;
}

static int ext_oled_io_open(struct inode *inode, struct file *file)
{
	struct lcd_extern_driver_s *edrv;
	struct lcd_ext_oled_s *e_oled;

	e_oled = container_of(inode->i_cdev, struct lcd_ext_oled_s, cdev);
	edrv = dev_get_drvdata(e_oled->dev);
	file->private_data = edrv;

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		EXTPR("%s\n", __func__);

	return 0;
}

static int ext_oled_io_release(struct inode *inode, struct file *file)
{
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		EXTPR("%s\n", __func__);
	file->private_data = NULL;
	return 0;
}

static long ext_oled_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct lcd_extern_driver_s *edrv;
	struct lcd_extern_dev_s *edev = NULL;
	void __user *argp;
	unsigned int mcd_nr, temp;
	int ret = 0;

	edrv = (struct lcd_extern_driver_s *)file->private_data;
	if (!ext_oled || !edrv) {
		pr_err("%s: ext_oled drv is null\n", __func__);
		return -EFAULT;
	}
	edev = ext_oled->ext_dev;
	if (!edev) {
		pr_err("%s: ext_oled edev is null\n", __func__);
		return -EFAULT;
	}

	mutex_lock(&ext_oled_mutex);
	mcd_nr = _IOC_NR(cmd);
	EXTPR("%s: cmd_dir = 0x%x, cmd_nr = 0x%x\n",
	      __func__, _IOC_DIR(cmd), mcd_nr);

	argp = (void __user *)arg;
	switch (mcd_nr) {
	case EXT_OLED_IOC_GET_OP_TIME:
		temp = ext_oled_operate_time_get_ms();
		if (copy_to_user(argp, &temp, sizeof(unsigned int)))
			ret = -EFAULT;
		break;
	case EXT_OLED_IOC_GET_TEMP:
		temp = ext_oled_temperature_get(edrv, edev, 0);
		if (copy_to_user(argp, &temp, sizeof(unsigned int)))
			ret = -EFAULT;
		break;
	case EXT_OLED_IOC_GET_OFF_RS:
		temp = (edev->state & EXT_OLED_STATE_OFF_RS_EN) ? 1 : 0;
		if (copy_to_user(argp, &temp, sizeof(unsigned int)))
			ret = -EFAULT;
		break;
	case EXT_OLED_IOC_SET_OFF_RS:
		if (copy_from_user(&temp, argp, sizeof(unsigned int))) {
			ret = -EFAULT;
			break;
		}
		ret = ext_oled_off_rs_set(edrv, edev, temp);
		break;
	case EXT_OLED_IOC_GET_JB:
		temp = (edev->state & EXT_OLED_STATE_JB_EN) ? 1 : 0;
		if (copy_to_user(argp, &temp, sizeof(unsigned int)))
			ret = -EFAULT;
		break;
	case EXT_OLED_IOC_SET_JB:
		if (copy_from_user(&temp, argp, sizeof(unsigned int))) {
			ret = -EFAULT;
			break;
		}
		ret = ext_oled_jb_set(edrv, edev, temp);
		break;
	case EXT_OLED_IOC_GET_ORBIT:
		if (copy_to_user(argp, &ext_oled->orbit_param,
			sizeof(struct ext_oled_orbit_s)))
			ret = -EFAULT;
		break;
	case EXT_OLED_IOC_SET_ORBIT:
		if (copy_from_user(&ext_oled->orbit_param, argp,
			sizeof(struct ext_oled_orbit_s))) {
			ret = -EFAULT;
			break;
		}
		ret = ext_oled_orbit_set(edrv, edev, ext_oled->orbit_param.en);
		break;
	case EXT_OLED_IOC_GET_HDR:
		temp = (edev->state & EXT_OLED_STATE_HDR_EN) ? 1 : 0;
		if (copy_to_user(argp, &temp, sizeof(unsigned int)))
			ret = -EFAULT;
		break;
	case EXT_OLED_IOC_SET_HDR:
		if (copy_from_user(&temp, argp, sizeof(unsigned int))) {
			ret = -EFAULT;
			break;
		}
		ret = ext_oled_hdr_set(edrv, edev, temp);
		break;
	default:
		EXTERR("not support ioctl cmd_nr: 0x%x\n", mcd_nr);
		ret = -EINVAL;
		break;
	}
	mutex_unlock(&ext_oled_mutex);

	return ret;
}

#ifdef CONFIG_COMPAT
static long ext_oled_compat_ioctl(struct file *file, unsigned int cmd,
				  unsigned long arg)
{
	unsigned long ret;

	arg = (unsigned long)compat_ptr(arg);
	ret = ext_oled_ioctl(file, cmd, arg);
	return ret;
}
#endif

static const struct file_operations ext_oled_fops = {
	.owner          = THIS_MODULE,
	.open           = ext_oled_io_open,
	.release        = ext_oled_io_release,
	.unlocked_ioctl = ext_oled_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl   = ext_oled_compat_ioctl,
#endif
};

static int ext_oled_cdev_add(struct lcd_extern_driver_s *edrv)
{
	int ret = 0;

	ret = alloc_chrdev_region(&ext_oled->devno, 0, 1, LCD_EXTERN_NAME);
	if (ret < 0) {
		EXTERR("%s: failed to alloc major number\n", __func__);
		return -1;
	}

	ext_oled->clsp = class_create(THIS_MODULE, LCD_EXTERN_NAME);
	if (IS_ERR(ext_oled->clsp)) {
		EXTERR("%s: failed to create class\n", __func__);
		goto ext_cdev_add_err;
	}

	/* connect the file operations with cdev */
	cdev_init(&ext_oled->cdev, &ext_oled_fops);
	ext_oled->cdev.owner = THIS_MODULE;

	/* connect the major/minor number to the cdev */
	ret = cdev_add(&ext_oled->cdev, ext_oled->devno, 1);
	if (ret) {
		EXTERR("%s: failed to connect cdev\n", __func__);
		goto ext_cdev_add_err1;
	}

	ext_oled->dev = device_create(ext_oled->clsp, NULL, ext_oled->devno,
			NULL, LCD_EXTERN_NAME);
	if (IS_ERR(ext_oled->dev)) {
		EXTERR("%s: failed to create device\n", __func__);
		goto ext_cdev_add_err1;
	}
	dev_set_drvdata(ext_oled->dev, edrv);

	ret = ext_oled_debug_file_add(ext_oled->dev);
	if (ret)
		goto ext_cdev_add_err3;

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		EXTPR("%s OK\n", __func__);
	return 0;

ext_cdev_add_err3:
	device_destroy(ext_oled->clsp, ext_oled->devno);
ext_cdev_add_err1:
	class_destroy(ext_oled->clsp);
ext_cdev_add_err:
	unregister_chrdev_region(ext_oled->devno, 1);
	EXTERR("%s: failed\n", __func__);
	return -1;
}

static void ext_oled_cdev_remove(struct lcd_extern_driver_s *edrv)
{
	ext_oled_debug_file_remove(ext_oled->dev);
	device_destroy(ext_oled->clsp, ext_oled->devno);
	class_destroy(ext_oled->clsp);
	unregister_chrdev_region(ext_oled->devno, 1);
}

static int lcd_extern_driver_update(struct lcd_extern_driver_s *edrv,
				    struct lcd_extern_dev_s *edev)
{
	if (edev->config.table_init_loaded == 0) {
		EXTERR("[%d]: dev_%d(%s): table_init is invalid\n",
		       edrv->index, edev->dev_index, edev->config.name);
		return -1;
	}

	edev->init  = ext_oled_init;
	edev->power_on  = ext_oled_power_on;
	edev->power_off = ext_oled_power_off;
	edev->reg_read = NULL;
	edev->reg_write = NULL;

	return 0;
}

int lcd_extern_i2c_oled_probe(struct lcd_extern_driver_s *edrv, struct lcd_extern_dev_s *edev)
{
	int ret = 0;

	if (!edrv || !edev) {
		EXTERR("%s: %s dev is null\n", __func__, LCD_EXTERN_NAME);
		return -1;
	}

	edev->i2c_dev[0] = lcd_extern_get_i2c_device(edev->config.i2c_addr);
	if (!edev->i2c_dev[0]) {
		EXTERR("%s: dev_%d invalid i2c_addr 0x%02x\n",
		       __func__, edev->dev_index, edev->config.i2c_addr);
		return -1;
	}
	EXTPR("[%d]: %s: get i2c_dev[0]: %s, addr 0x%02x OK\n",
	      edrv->index, __func__,
	      edev->i2c_dev[0]->name, edev->i2c_dev[0]->client->addr);

	edev->i2c_dev[1] = lcd_extern_get_i2c_device(edev->config.i2c_addr2);
	if (!edev->i2c_dev[1]) {
		EXTERR("%s: dev_%d invalid i2c_addr2 0x%02x\n",
		       __func__, edev->dev_index, edev->config.i2c_addr2);
		return -1;
	}
	EXTPR("[%d]: %s: get i2c_dev[1]: %s, addr 0x%02x OK\n",
	      edrv->index, __func__,
	      edev->i2c_dev[1]->name, edev->i2c_dev[1]->client->addr);

	ret = lcd_extern_driver_update(edrv, edev);
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
		EXTPR("[%d]: %s: dev_%d %d\n",
		      edrv->index, __func__, edev->dev_index, ret);
	}

	ext_oled = kzalloc(sizeof(*ext_oled), GFP_KERNEL);
	if (!ext_oled)
		return -1;

	ext_oled->dev_index = edev->dev_index;
	ext_oled->ext_dev = edev;
	mutex_init(&ext_oled_mutex);
	mutex_init(&ext_oled_power_mutex);

	ret = ext_oled_get_config(edrv, edev);
	if (ret)
		return -1;

	ret = ext_oled_cdev_add(edrv);

	return ret;
}

int lcd_extern_i2c_oled_remove(struct lcd_extern_driver_s *edrv, struct lcd_extern_dev_s *edev)
{
	if (ext_oled) {
		ext_oled_cdev_remove(edrv);
		kfree(ext_oled);
		ext_oled = NULL;
	}

	return 0;
}
