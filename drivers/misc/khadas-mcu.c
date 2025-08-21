/*
 * Khadas Edge MCU control driver
 *
 * Written by: Nick <nick@khadas.com>
 *
 * Copyright (C) 2024 Khadas Technologies Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
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
#include <linux/wakelock.h>
#include <linux/delay.h>

#define MCU_AGEING_TEST	0x16
#define MCU_USID        0x06

/* Device registers */
#define MCU_PWR_OFF_CMD_REG             0x80
#define MCU_SHUTDOWN_NORMAL_REG         0x2c
#define MCU_RGB_LED_ON_CTRL_REG         0x23
#define MCU_RGB_LED_OFF_CTRL_REG        0x24
#define MCU_R_LED_ON_CTRL_REG           0x25
#define MCU_G_LED_ON_CTRL_REG           0x26
#define MCU_B_LED_ON_CTRL_REG           0x27
#define MCU_R_LED_OFF_CTRL_REG          0x28
#define MCU_G_LED_OFF_CTRL_REG          0x29
#define MCU_B_LED_OFF_CTRL_REG          0x2A


/*Fan device*/
#define MCU_CMD_FAN_STATUS_CTRL_REGv2   0x8A

#define MCU_FAN_TRIG_TEMP_LEVEL0        50  // 50 degree if not set
#define MCU_FAN_TRIG_TEMP_LEVEL1        60  // 60 degree if not set
#define MCU_FAN_TRIG_TEMP_LEVEL2        65 // 70 degree if not set
#define MCU_FAN_TRIG_TEMP_LEVEL3        70  // 60 degree if not set
#define MCU_FAN_TRIG_TEMP_LEVEL4        75 // 70 degree if not set
#define MCU_FAN_TRIG_TEMP_LEVEL5        80 // 70 degree if not set
#define MCU_FAN_TRIG_MAXTEMP            105
#define MCU_FAN_LOOP_SECS               (30 * HZ)   // 30 seconds
#define MCU_FAN_TEST_LOOP_SECS          (5 * HZ)  // 5 seconds
#define MCU_FAN_LOOP_NODELAY_SECS       0
#define MCU_FAN_SPEED_OFF               0
#define MCU_FAN_SPEED_1                 0x23
#define MCU_FAN_SPEED_2                 0x33
#define MCU_FAN_SPEED_3                 0x43
#define MCU_FAN_SPEED_4                 0x53
#define MCU_FAN_SPEED_5                 0x63
#define MCU_FAN_SPEED_LOW_V2            0x32
#define MCU_FAN_SPEED_MID_V2            0x48
#define MCU_FAN_SPEED_HIGH_V2           0x64

enum khadas_board {
	KHADAS_BOARD_NONE = 0,
	KHADAS_BOARD_EDGE2L
};

enum khadas_board_hwver {
	KHADAS_BOARD_HWVER_NONE = 0,
	KHADAS_BOARD_HWVER_V11
};

enum mcu_fan_mode {
	MCU_FAN_MODE_MANUAL = 0,
	MCU_FAN_MODE_AUTO
};

enum mcu_fan_level {
	MCU_FAN_LEVEL_0 = 0,
	MCU_FAN_LEVEL_1,
	MCU_FAN_LEVEL_2,
	MCU_FAN_LEVEL_3,
	MCU_FAN_LEVEL_4,
	MCU_FAN_LEVEL_5
};

enum mcu_fan_status {
	MCU_FAN_STATUS_DISABLE = 0,
	MCU_FAN_STATUS_ENABLE,
};

struct mcu_fan_data {
	struct platform_device *pdev;
	struct class *fan_class;
	struct delayed_work work;
	struct delayed_work fan_test_work;
	enum mcu_fan_status enable;
	enum mcu_fan_mode mode;
	enum mcu_fan_level level;
	int trig_temp_level0;
	int trig_temp_level1;
	int trig_temp_level2;
	int trig_temp_level3;
	int trig_temp_level4;
	int trig_temp_level5;
};

struct mcu_data {
	struct i2c_client *client;
	struct class *mcu_class;
	enum khadas_board board;
	enum khadas_board_hwver hwver;
	struct mcu_fan_data fan_data;
};

struct mcu_data *g_mcu_data;
int ageing_test_flag = 0;
int key_test_flag = 0;

int StringToHex(char *str, unsigned char *out, unsigned int *outlen)
{
	char *p = str;
	char high = 0, low = 0;
	int tmplen = strlen(p), cnt = 0;
	tmplen = strlen(p);
	while(cnt < (tmplen / 2)) {
		high = ((*p > '9') && ((*p <= 'F') || (*p <= 'f'))) ? *p - 48 - 7 : *p - 48;
		low = (*(++ p) > '9' && ((*p <= 'F') || (*p <= 'f'))) ? *(p) - 48 - 7 : *(p) - 48;
		out[cnt] = ((high & 0x0f) << 4 | (low & 0x0f));
		p ++;
		cnt ++;
	}
	if(tmplen % 2 != 0) out[cnt] = ((*p > '9') && ((*p <= 'F') || (*p <= 'f'))) ? *p - 48 - 7 : *p - 48;

	if(outlen != NULL) *outlen = tmplen / 2 + tmplen % 2;
	return tmplen / 2 + tmplen % 2;
}

void HexToAscii(unsigned char *pHex, unsigned char *pAscii, int nLen)
{
    unsigned char Nibble[2];
    unsigned int i,j;
    for (i = 0; i < nLen; i++) {
        Nibble[0] = (pHex[i] & 0xF0) >> 4;
        Nibble[1] = pHex[i] & 0x0F;
        for (j = 0; j < 2; j++) {
            if (Nibble[j] < 10) {
                Nibble[j] += 0x30;
            } else {
                if (Nibble[j] < 16)
                    Nibble[j] = Nibble[j] - 10 + 'A';
            }

            *pAscii++ = Nibble[j];
            if (i==(nLen-1) && j==1) {
               *pAscii = '\0';
            }
        }
    }
}

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
	memcpy(tx_buf+1, buf, count);

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
		u8 reg, u8 buf[], unsigned len)
{
	int ret;
	ret = i2c_master_reg8_recv(client, reg, buf, len);
	return ret;
}

static int mcu_i2c_write_regs(struct i2c_client *client,
		u8 reg, u8 const buf[], __u16 len)
{
	int ret;

	ret = i2c_master_reg8_send(client, reg, buf, (int)len);

	return ret;
}

int mcu_reboot_boot_mode(void)
{
	int ret;
	u8 sendbuf=0;
	sendbuf =2;

	ret = mcu_i2c_write_regs(g_mcu_data->client, 0x91, &sendbuf, 1);
	if(ret < 0){
		printk("write mcu boot control err\r\n");
		return ret;
	}
	return 0;
}
EXPORT_SYMBOL(mcu_reboot_boot_mode);

static int is_mcu_fan_control_supported(void)
{
	// MCU FAN control is supported for:
	// 1. Khadas EDGE2
	if (g_mcu_data->board == KHADAS_BOARD_EDGE2L) {
		if (g_mcu_data->hwver >= KHADAS_BOARD_HWVER_V11)
			return 1;
		else
			return 0;
	} else {
			return 0;
	}

}

static int is_mcu_mculed_control_supported(void)
{
	// MCU MCU LED control is supported for:
	// 1. Khadas EDGE2 and later
		//if (g_mcu_data->hwver >= KHADAS_BOARD_HWVER_V10)
			return 1;
		//else
			//return 0;
}

static void mcu_fan_level_set(struct mcu_fan_data *fan_data, int level)
{
	if (is_mcu_fan_control_supported()) {
		int ret;
		u8 data = 0;

		g_mcu_data->fan_data.level = level;

		if (g_mcu_data->board == KHADAS_BOARD_EDGE2L) {
			if (level == 0)
				data = MCU_FAN_SPEED_OFF;
			else if (level == 1)
				data = MCU_FAN_SPEED_1;
			else if (level == 2)
				data = MCU_FAN_SPEED_2;
			else if (level == 3)
				data = MCU_FAN_SPEED_3;
			else if (level == 4)
				data = MCU_FAN_SPEED_4;
			else if (level == 5)
				data = MCU_FAN_SPEED_5;
			ret = mcu_i2c_write_regs(g_mcu_data->client,
					MCU_CMD_FAN_STATUS_CTRL_REGv2,
					&data, 1);
			if (ret < 0) {
				pr_debug("write fan control err\n");
				return;
			}
		}
	}
}

extern int rk_get_temperature(void);

static void fan_work_func(struct work_struct *_work)
{
	if (is_mcu_fan_control_supported()) {
		int temp = -EINVAL;
		struct mcu_fan_data *fan_data = &g_mcu_data->fan_data;

		if (g_mcu_data->board == KHADAS_BOARD_EDGE2L) {
			temp = rk_get_temperature();
		} else {
			temp = fan_data->trig_temp_level0;
		}

		if (temp != -EINVAL) {
			if (temp < fan_data->trig_temp_level0)
				mcu_fan_level_set(fan_data, 0);
			else if (temp < fan_data->trig_temp_level1)
				mcu_fan_level_set(fan_data, 1);
			else if (temp < fan_data->trig_temp_level2)
				mcu_fan_level_set(fan_data, 2);
			else if (temp < fan_data->trig_temp_level3)
				mcu_fan_level_set(fan_data, 3);
			else if (temp < fan_data->trig_temp_level4)
				mcu_fan_level_set(fan_data, 4);
			else
				mcu_fan_level_set(fan_data, 5);
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
				case MCU_FAN_LEVEL_4:
					mcu_fan_level_set(fan_data, 4);
					break;
				case MCU_FAN_LEVEL_5:
					mcu_fan_level_set(fan_data, 5);
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

	if (level >= 0 && level < 6) {
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

	if (g_mcu_data->board == KHADAS_BOARD_EDGE2L) {
		temp = rk_get_temperature();
	} else {
		temp = fan_data->trig_temp_level0;
	}

	return sprintf(buf,
			"cpu_temp:%d\nFan trigger temperature: level0:%d level1:%d level2:%d\n",
			temp, g_mcu_data->fan_data.trig_temp_level0,
			g_mcu_data->fan_data.trig_temp_level1,
			g_mcu_data->fan_data.trig_temp_level2);
}

extern char dp_out_mode[32];
static ssize_t show_dpmode_temp(struct class *cls,
             struct class_attribute *attr, char *buf)
{
    return sprintf(buf, "%s\n", dp_out_mode);
}

void fan_level_set(struct mcu_data *ug_mcu_data)
{
	struct mcu_fan_data *fan_data = &g_mcu_data->fan_data;
	int temp = -EINVAL;

	if (ug_mcu_data->board == KHADAS_BOARD_EDGE2L) {
		temp = rk_get_temperature();
	} else {
		temp = fan_data->trig_temp_level0;
	}

	if (temp != -EINVAL) {
		if (temp < ug_mcu_data->fan_data.trig_temp_level0)
			mcu_fan_level_set(fan_data, 0);
		else if (temp < ug_mcu_data->fan_data.trig_temp_level1)
			mcu_fan_level_set(fan_data, 1);
		else if (temp < ug_mcu_data->fan_data.trig_temp_level2)
			mcu_fan_level_set(fan_data, 2);
		else if (temp < ug_mcu_data->fan_data.trig_temp_level3)
			mcu_fan_level_set(fan_data, 3);
		else if (temp < ug_mcu_data->fan_data.trig_temp_level4)
			mcu_fan_level_set(fan_data, 4);
		else
			mcu_fan_level_set(fan_data, 5);
	}
}


static void mcu_mculed_set(int addr, int mode)
{
	int ret;
	char reg;
	u8 addr_val;

	if (addr >= 0x23 && addr <= 0x24) {
		reg = (char)mode;
		addr_val = (u8)addr;
		if (is_mcu_mculed_control_supported()) {
			    ret = mcu_i2c_write_regs(g_mcu_data->client,addr_val,&reg, 1);
				if (ret < 0)
					pr_debug("write mcu led cmd error\n");
				else
					pr_debug("write mcu led cmd mode=%d\n",mode);
		}
	}else if(addr >= 0x25 && addr <= 0x2a) {
		reg = (char)mode;
		addr_val = (u8)addr;
		if (is_mcu_mculed_control_supported()) {
			    ret = mcu_i2c_write_regs(g_mcu_data->client,addr_val,&reg, 1);
				if (ret < 0)
					pr_debug("write mcu led cmd error\n");
				else
					pr_debug("write mcu led cmd mode=%d\n",mode);
		}
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

static ssize_t store_mcu_poweroff(struct class *cls,struct class_attribute *attr,
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

	ret = mcu_i2c_write_regs(g_mcu_data->client,MCU_PWR_OFF_CMD_REG,
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
	ret = mcu_i2c_write_regs(g_mcu_data->client,MCU_SHUTDOWN_NORMAL_REG,
							&reg, 1);
	if (ret < 0) {
		pr_debug("write poweroff cmd error\n");
		return ret;
	}

	return count;
}

static ssize_t store_mculed_mode(struct class *cls,
		struct class_attribute *attr,
		const char *buf, size_t count)
{
	int reg;
	int val;
	int reg16;

	if (kstrtoint(buf, 0, &reg16))
		return -EINVAL;

	//printk("mcu===>reg16=0x%x\n",reg16);
	reg = reg16>>8;
	val = (int)((u8)reg16);

	//printk("mcu===>reg=0x%x,val=0x%x\n",reg,val);
	mcu_mculed_set(reg,val);
	return count;
}

static ssize_t show_ageing_test(struct class *cls,
				struct class_attribute *attr, char *buf)
{
	int ret;
	unsigned char addr[1]={0};

	ret = mcu_i2c_read_regs(g_mcu_data->client, MCU_AGEING_TEST, addr, 1);
	if (ret < 0)
		printk("%s: AGEING_TEST failed (%d)",__func__, ret);

	return sprintf(buf, "%d\n", addr[0]);
}

static ssize_t store_ageing_test(struct class *cls, struct class_attribute *attr,
				const char *buf, size_t count)
{
	u8 reg[2];
	int ret;
	int enable;

	if (kstrtoint(buf, 0, &enable))
		return -EINVAL;
	reg[0] = enable;
	ret = mcu_i2c_write_regs(g_mcu_data->client, MCU_AGEING_TEST, reg, 1);
	if (ret < 0) {
		printk("ageing_test state err\n");
		return ret;
	}
	printk("ageing_test state: %d\n", enable);
	ageing_test_flag = 1;
	return count;
}

static ssize_t store_key_test(struct class *cls, struct class_attribute *attr,
				const char *buf, size_t count)
{
	if (kstrtoint(buf, 0, &key_test_flag))
		return -EINVAL;
	printk("key_test_flag: %d\n", key_test_flag);
	return count;
}

static ssize_t show_usid_addr(struct class *cls,
				struct class_attribute *attr, char *buf)
{
	int ret;
	unsigned char addr_usid[15]={0};
	unsigned char addr[7]={0};
	int i;

	for(i=0; i<=6; i++){
		ret = mcu_i2c_read_regs(g_mcu_data->client, MCU_USID+i, &addr[i], 1);
		if (ret < 0)
			printk("%s: usid address failed (%d)",__func__, ret);
			//printk("%s: mac address: %02x\n",__func__, addr[i]);
	}
	HexToAscii(addr,addr_usid,7);
	printk("usid address (%s)\n", addr_usid);

	return sprintf(buf, "%s\n", addr_usid);
}

static ssize_t store_usid_addr(struct class *cls, struct class_attribute *attr,
				const char *buf, size_t count)
{
	int ret;
	char addr_usid[15]={0};
	unsigned char addr[7]={0};
	int outlen = 0;
	int i;
	unsigned char cmd[2];
	unsigned char password[] = {
		0x01,
		0x73,
		0x61,
		0x64,
		0x61,
		0x68,
		0x4B,
		0x00
	};

	sscanf(buf,"%s",addr_usid);

	ret=StringToHex(addr_usid,addr,&outlen);
	if (ret <=0){
		printk("%s: input usid String error (%d)\n",__func__, ret);
		return ret;
	}

	cmd[0] = 0x81;
	for (i = 0; i < sizeof(password); i++) {
		cmd[1] = password[i];
		ret = mcu_i2c_write_regs(g_mcu_data->client, cmd[0], &cmd[1], 1);
		if (ret < 0) {
			printk("%s: write password failed (%d)\n", __func__, ret);
			return ret;
		}

		if (i == 0) {
			cmd[0] = 0x82;
		} else if (i == 6) {
			cmd[0] = 0x81;
		}
	}

	printk("usid address: %02x:%02x:%02x:%02x:%02x:%02x:%02x\n",
			addr[0], addr[1], addr[2],
			addr[3], addr[4], addr[5], addr[6]);
	for (i=0; i<=6; i++) {
		ret = mcu_i2c_write_regs(g_mcu_data->client, MCU_USID+i, &addr[i], 1);
		if (ret < 0) {
			printk("%s: usid address failed (%d)\n",__func__, ret);
			return ret;
		}
	}
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

static struct class_attribute mcu_class_attrs[] = {
	__ATTR(poweroff, 0644, NULL, store_mcu_poweroff),
	__ATTR(rst, 0644, NULL, store_mcu_rst),
	__ATTR(sn_addr, 0644, show_usid_addr, store_usid_addr),
	__ATTR(ageing_test, 0644, show_ageing_test, store_ageing_test),
	__ATTR(key_test, 0644, NULL, store_key_test),
	__ATTR(mculed, 0644, NULL, store_mculed_mode),
	__ATTR(dpmode, 0644, show_dpmode_temp, NULL),
};

static void create_mcu_attrs(void) {
	int i;

	g_mcu_data->mcu_class = class_create(THIS_MODULE, "mcu");
	if (IS_ERR(g_mcu_data->mcu_class)) {
		pr_err("create mcu_class debug class fail\n");

		return;
	}
	for (i = 0; i < ARRAY_SIZE(mcu_class_attrs); i++) {
		if (class_create_file(g_mcu_data->mcu_class, &mcu_class_attrs[i]))
			pr_err("create mcu attribute %s fail\n", mcu_class_attrs[i].attr.name);
	}

	if (is_mcu_fan_control_supported()) {
		g_mcu_data->fan_data.fan_class = class_create(THIS_MODULE, "fan");
			if (IS_ERR(g_mcu_data->fan_data.fan_class)) {
				pr_err("create fan_class debug class fail\n");
				return;
			}
			for (i = 0; i < ARRAY_SIZE(fan_class_attrs); i++) {
				if (class_create_file(g_mcu_data->fan_data.fan_class,
						&fan_class_attrs[i]))
					pr_err("create fan attribute %s fail\n", fan_class_attrs[i].attr.name);
		}
	}
}

static int mcu_parse_dt(struct device *dev)
{
	int ret = 0;
	const char *hwver = NULL;

	if (NULL == dev) return -EINVAL;

	ret = of_property_read_string(dev->of_node, "hwver", &hwver);
	if (ret < 0) {
			return 0;
	} else {
			if (strstr(hwver, "EDGE2"))
					g_mcu_data->board = KHADAS_BOARD_EDGE2L;
			else
					g_mcu_data->board = KHADAS_BOARD_NONE;

			if (g_mcu_data->board == KHADAS_BOARD_EDGE2L) {
					if (strcmp(hwver, "EDGE2.V11") == 0)
							g_mcu_data->hwver = KHADAS_BOARD_HWVER_V11;
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
			"fan,trig_temp_level2",&g_mcu_data->fan_data.trig_temp_level2);
	if (ret < 0){
		g_mcu_data->fan_data.trig_temp_level2 =MCU_FAN_TRIG_TEMP_LEVEL2;
	}
    ret = of_property_read_u32(dev->of_node,
            "fan,trig_temp_level3",&g_mcu_data->fan_data.trig_temp_level3);
    if (ret < 0){
        g_mcu_data->fan_data.trig_temp_level3 =MCU_FAN_TRIG_TEMP_LEVEL3;
	}
    ret = of_property_read_u32(dev->of_node,
            "fan,trig_temp_level4",&g_mcu_data->fan_data.trig_temp_level4);
    if (ret < 0){
        g_mcu_data->fan_data.trig_temp_level4 =MCU_FAN_TRIG_TEMP_LEVEL4;
	}
    ret = of_property_read_u32(dev->of_node,
            "fan,trig_temp_level5",&g_mcu_data->fan_data.trig_temp_level5);
    if (ret < 0){
        g_mcu_data->fan_data.trig_temp_level5 =MCU_FAN_TRIG_TEMP_LEVEL5;
	}
	return ret;
}

static int mcu_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	printk("%s\n", __func__);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
		return -ENODEV;

	g_mcu_data = kzalloc(sizeof(struct mcu_data), GFP_KERNEL);

	if (g_mcu_data == NULL)
		return -ENOMEM;

	mcu_parse_dt(&client->dev);

	g_mcu_data->client = client;

	g_mcu_data->fan_data.mode = MCU_FAN_MODE_AUTO;
	g_mcu_data->fan_data.level = MCU_FAN_LEVEL_0;
	g_mcu_data->fan_data.enable = MCU_FAN_STATUS_ENABLE;

	INIT_DELAYED_WORK(&g_mcu_data->fan_data.work, fan_work_func);
	mcu_fan_level_set(&g_mcu_data->fan_data, 0);
	schedule_delayed_work(&g_mcu_data->fan_data.work, MCU_FAN_LOOP_SECS);
	create_mcu_attrs();

	return 0;
}

static void mcu_remove(struct i2c_client *client)
{
	kfree(g_mcu_data);
}

static void khadas_fan_shutdown(struct i2c_client *client)
{
	g_mcu_data->fan_data.enable = MCU_FAN_STATUS_DISABLE;
	khadas_fan_set(&g_mcu_data->fan_data);
}

#ifdef CONFIG_PM_SLEEP
static int khadas_fan_suspend(struct device *dev)
{
	cancel_delayed_work(&g_mcu_data->fan_data.work);
	mcu_fan_level_set(&g_mcu_data->fan_data, 0);

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


static struct of_device_id mcu_dt_ids[] = {
	{ .compatible = "khadas-mcu" },
	{},
};
MODULE_DEVICE_TABLE(i2c, mcu_dt_ids);

struct i2c_driver mcu_driver = {
	.driver  = {
		.name   = "khadas-mcu",
		.owner  = THIS_MODULE,
		.of_match_table = of_match_ptr(mcu_dt_ids),
#ifdef CONFIG_PM_SLEEP
		.pm = FAN_PM_OPS,
#endif
	},
	.probe		= mcu_probe,
	.remove 	= mcu_remove,
	.shutdown = khadas_fan_shutdown,
	.id_table	= mcu_id,
};
module_i2c_driver(mcu_driver);

MODULE_AUTHOR("Nick <nick@khadas.com>");
MODULE_DESCRIPTION("Khadas Edge2l MCU control driver");
MODULE_LICENSE("GPL");
