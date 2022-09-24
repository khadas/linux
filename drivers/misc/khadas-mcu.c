/*
 * Khadas Edge MCU control driver
 *
 * Written by: Nick <nick@khadas.com>
 *
 * Copyright (C) 2022 Wesion Technologies Ltd.
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

/* Device registers */
#define MCU_PWR_OFF_CMD_REG       0x80
#define MCU_SHUTDOWN_NORMAL_REG   0x2c
#define MCU_RGB_LED_ON_CTRL_REG         	0x23
#define MCU_RGB_LED_OFF_CTRL_REG         	0x24
#define MCU_R_LED_ON_CTRL_REG         	0x25
#define MCU_G_LED_ON_CTRL_REG         	0x26
#define MCU_B_LED_ON_CTRL_REG         	0x27
#define MCU_R_LED_OFF_CTRL_REG         	0x28
#define MCU_G_LED_OFF_CTRL_REG         	0x29
#define MCU_B_LED_OFF_CTRL_REG         	0x2A


/*Fan device*/
#define MCU_CMD_FAN_STATUS_CTRL_REGv2   0x8A

#define MCU_FAN_TRIG_TEMP_LEVEL0        60  // 50 degree if not set
#define MCU_FAN_TRIG_TEMP_LEVEL1        80  // 60 degree if not set
#define MCU_FAN_TRIG_TEMP_LEVEL2        100 // 70 degree if not set
#define MCU_FAN_TRIG_MAXTEMP            105
#define MCU_FAN_LOOP_SECS               (30 * HZ)   // 30 seconds
#define MCU_FAN_TEST_LOOP_SECS          (5 * HZ)  // 5 seconds
#define MCU_FAN_LOOP_NODELAY_SECS       0
#define MCU_FAN_SPEED_OFF               0
#define MCU_FAN_SPEED_LOW               1
#define MCU_FAN_SPEED_MID               2
#define MCU_FAN_SPEED_HIGH              3
#define MCU_FAN_SPEED_LOW_V2            0x32
#define MCU_FAN_SPEED_MID_V2            0x48
#define MCU_FAN_SPEED_HIGH_V2           0x64

enum khadas_board {
	KHADAS_BOARD_NONE = 0,
	KHADAS_BOARD_EDGE2
};

enum khadas_board_hwver {
	KHADAS_BOARD_HWVER_NONE = 0,
	KHADAS_BOARD_HWVER_V11
};

enum khadas_mculed_mode {
	KHADAS_MCU_LED_OFF = 0,
	KHADAS_MCU_LED_ON,
	KHADAS_RED_LED_BREATH,
	KHADAS_GREEN_LED_BREATH,
	KHADAS_BLUE_LED_BREATH,
	KHADAS_RG_LED_BREATH,
	KHADAS_RB_LED_BREATH,
	KHADAS_GB_LED_BREATH,
	KHADAS_RGB_LED_BREATH,
	KHADAS_RED_LED_HEARTBEAT,
	KHADAS_GREEN_LED_HEARTBEAT,
	KHADAS_BLUE_LED_HEARTBEAT,
	KHADAS_RG_LED_HEARTBEAT,
	KHADAS_RB_LED_HEARTBEAT,
	KHADAS_GB_LED_HEARTBEAT,
	KHADAS_RGB_LED_HEARTBEAT,
};

enum khadas_mculed_bri_mode {
	KHADAS_MCU_LED_BRI0 = 0,
	KHADAS_MCU_LED_BRI1,
	KHADAS_MCU_LED_BRI2,
	KHADAS_MCU_LED_BRI3,
	KHADAS_MCU_LED_BRI4,
	KHADAS_MCU_LED_BRI5,
	KHADAS_MCU_LED_BRI6,
	KHADAS_MCU_LED_BRI7,
	KHADAS_MCU_LED_BRI8,
	KHADAS_MCU_LED_BRI9,
	KHADAS_MCU_LED_BRI10,
	KHADAS_MCU_LED_BRI11,
	KHADAS_MCU_LED_BRI12,
	KHADAS_MCU_LED_BRI13,
	KHADAS_MCU_LED_BRI14,
	KHADAS_MCU_LED_BRI15,
	KHADAS_MCU_LED_BRI16,
	KHADAS_MCU_LED_BRI17,
	KHADAS_MCU_LED_BRI18,
	KHADAS_MCU_LED_BRI19,
	KHADAS_MCU_LED_BRI20,
	KHADAS_MCU_LED_BRI21,
	KHADAS_MCU_LED_BRI22,
	KHADAS_MCU_LED_BRI23,
	KHADAS_MCU_LED_BRI24,
	KHADAS_MCU_LED_BRI25,
	KHADAS_MCU_LED_BRI26,
	KHADAS_MCU_LED_BRI27,
	KHADAS_MCU_LED_BRI28,
	KHADAS_MCU_LED_BRI29,
	KHADAS_MCU_LED_BRI30,
	KHADAS_MCU_LED_BRI31,
	KHADAS_MCU_LED_BRI32,
	KHADAS_MCU_LED_BRI33,
	KHADAS_MCU_LED_BRI34,
	KHADAS_MCU_LED_BRI35,
	KHADAS_MCU_LED_BRI36,
	KHADAS_MCU_LED_BRI37,
	KHADAS_MCU_LED_BRI38,
	KHADAS_MCU_LED_BRI39,
	KHADAS_MCU_LED_BRI40,
	KHADAS_MCU_LED_BRI41,
	KHADAS_MCU_LED_BRI42,
	KHADAS_MCU_LED_BRI43,
	KHADAS_MCU_LED_BRI44,
	KHADAS_MCU_LED_BRI45,
	KHADAS_MCU_LED_BRI46,
	KHADAS_MCU_LED_BRI47,
	KHADAS_MCU_LED_BRI48,
	KHADAS_MCU_LED_BRI49,
	KHADAS_MCU_LED_BRI50,
	KHADAS_MCU_LED_BRI51,
	KHADAS_MCU_LED_BRI52,
	KHADAS_MCU_LED_BRI53,
	KHADAS_MCU_LED_BRI54,
	KHADAS_MCU_LED_BRI55,
	KHADAS_MCU_LED_BRI56,
	KHADAS_MCU_LED_BRI57,
	KHADAS_MCU_LED_BRI58,
	KHADAS_MCU_LED_BRI59,
	KHADAS_MCU_LED_BRI60,
	KHADAS_MCU_LED_BRI61,
	KHADAS_MCU_LED_BRI62,
	KHADAS_MCU_LED_BRI63,
	KHADAS_MCU_LED_BRI64,
	KHADAS_MCU_LED_BRI65,
	KHADAS_MCU_LED_BRI66,
	KHADAS_MCU_LED_BRI67,
	KHADAS_MCU_LED_BRI68,
	KHADAS_MCU_LED_BRI69,
	KHADAS_MCU_LED_BRI70,
	KHADAS_MCU_LED_BRI71,
	KHADAS_MCU_LED_BRI72,
	KHADAS_MCU_LED_BRI73,
	KHADAS_MCU_LED_BRI74,
	KHADAS_MCU_LED_BRI75,
	KHADAS_MCU_LED_BRI76,
	KHADAS_MCU_LED_BRI77,
	KHADAS_MCU_LED_BRI78,
	KHADAS_MCU_LED_BRI79,
	KHADAS_MCU_LED_BRI80,
	KHADAS_MCU_LED_BRI81,
	KHADAS_MCU_LED_BRI82,
	KHADAS_MCU_LED_BRI83,
	KHADAS_MCU_LED_BRI84,
	KHADAS_MCU_LED_BRI85,
	KHADAS_MCU_LED_BRI86,
	KHADAS_MCU_LED_BRI87,
	KHADAS_MCU_LED_BRI88,
	KHADAS_MCU_LED_BRI89,
	KHADAS_MCU_LED_BRI90,
	KHADAS_MCU_LED_BRI91,
	KHADAS_MCU_LED_BRI92,
	KHADAS_MCU_LED_BRI93,
	KHADAS_MCU_LED_BRI94,
	KHADAS_MCU_LED_BRI95,
	KHADAS_MCU_LED_BRI96,
	KHADAS_MCU_LED_BRI97,
	KHADAS_MCU_LED_BRI98,
	KHADAS_MCU_LED_BRI99,
	KHADAS_MCU_LED_BRI100,
	KHADAS_MCU_LED_BRI101,
	KHADAS_MCU_LED_BRI102,
	KHADAS_MCU_LED_BRI103,
	KHADAS_MCU_LED_BRI104,
	KHADAS_MCU_LED_BRI105,
	KHADAS_MCU_LED_BRI106,
	KHADAS_MCU_LED_BRI107,
	KHADAS_MCU_LED_BRI108,
	KHADAS_MCU_LED_BRI109,
	KHADAS_MCU_LED_BRI110,
	KHADAS_MCU_LED_BRI111,
	KHADAS_MCU_LED_BRI112,
	KHADAS_MCU_LED_BRI113,
	KHADAS_MCU_LED_BRI114,
	KHADAS_MCU_LED_BRI115,
	KHADAS_MCU_LED_BRI116,
	KHADAS_MCU_LED_BRI117,
	KHADAS_MCU_LED_BRI118,
	KHADAS_MCU_LED_BRI119,
	KHADAS_MCU_LED_BRI120,
	KHADAS_MCU_LED_BRI121,
	KHADAS_MCU_LED_BRI122,
	KHADAS_MCU_LED_BRI123,
	KHADAS_MCU_LED_BRI124,
	KHADAS_MCU_LED_BRI125,
	KHADAS_MCU_LED_BRI126,
	KHADAS_MCU_LED_BRI127,
	KHADAS_MCU_LED_BRI128,
	KHADAS_MCU_LED_BRI129,
	KHADAS_MCU_LED_BRI130,
	KHADAS_MCU_LED_BRI131,
	KHADAS_MCU_LED_BRI132,
	KHADAS_MCU_LED_BRI133,
	KHADAS_MCU_LED_BRI134,
	KHADAS_MCU_LED_BRI135,
	KHADAS_MCU_LED_BRI136,
	KHADAS_MCU_LED_BRI137,
	KHADAS_MCU_LED_BRI138,
	KHADAS_MCU_LED_BRI139,
	KHADAS_MCU_LED_BRI140,
	KHADAS_MCU_LED_BRI141,
	KHADAS_MCU_LED_BRI142,
	KHADAS_MCU_LED_BRI143,
	KHADAS_MCU_LED_BRI144,
	KHADAS_MCU_LED_BRI145,
	KHADAS_MCU_LED_BRI146,
	KHADAS_MCU_LED_BRI147,
	KHADAS_MCU_LED_BRI148,
	KHADAS_MCU_LED_BRI149,
	KHADAS_MCU_LED_BRI150,
	KHADAS_MCU_LED_BRI151,
	KHADAS_MCU_LED_BRI152,
	KHADAS_MCU_LED_BRI153,
	KHADAS_MCU_LED_BRI154,
	KHADAS_MCU_LED_BRI155,
	KHADAS_MCU_LED_BRI156,
	KHADAS_MCU_LED_BRI157,
	KHADAS_MCU_LED_BRI158,
	KHADAS_MCU_LED_BRI159,
	KHADAS_MCU_LED_BRI160,
	KHADAS_MCU_LED_BRI161,
	KHADAS_MCU_LED_BRI162,
	KHADAS_MCU_LED_BRI163,
	KHADAS_MCU_LED_BRI164,
	KHADAS_MCU_LED_BRI165,
	KHADAS_MCU_LED_BRI166,
	KHADAS_MCU_LED_BRI167,
	KHADAS_MCU_LED_BRI168,
	KHADAS_MCU_LED_BRI169,
	KHADAS_MCU_LED_BRI170,
	KHADAS_MCU_LED_BRI171,
	KHADAS_MCU_LED_BRI172,
	KHADAS_MCU_LED_BRI173,
	KHADAS_MCU_LED_BRI174,
	KHADAS_MCU_LED_BRI175,
	KHADAS_MCU_LED_BRI176,
	KHADAS_MCU_LED_BRI177,
	KHADAS_MCU_LED_BRI178,
	KHADAS_MCU_LED_BRI179,
	KHADAS_MCU_LED_BRI180,
	KHADAS_MCU_LED_BRI181,
	KHADAS_MCU_LED_BRI182,
	KHADAS_MCU_LED_BRI183,
	KHADAS_MCU_LED_BRI184,
	KHADAS_MCU_LED_BRI185,
	KHADAS_MCU_LED_BRI186,
	KHADAS_MCU_LED_BRI187,
	KHADAS_MCU_LED_BRI188,
	KHADAS_MCU_LED_BRI189,
	KHADAS_MCU_LED_BRI190,
	KHADAS_MCU_LED_BRI191,
	KHADAS_MCU_LED_BRI192,
	KHADAS_MCU_LED_BRI193,
	KHADAS_MCU_LED_BRI194,
	KHADAS_MCU_LED_BRI195,
	KHADAS_MCU_LED_BRI196,
	KHADAS_MCU_LED_BRI197,
	KHADAS_MCU_LED_BRI198,
	KHADAS_MCU_LED_BRI199,
	KHADAS_MCU_LED_BRI200,
	KHADAS_MCU_LED_BRI201,
	KHADAS_MCU_LED_BRI202,
	KHADAS_MCU_LED_BRI203,
	KHADAS_MCU_LED_BRI204,
	KHADAS_MCU_LED_BRI205,
	KHADAS_MCU_LED_BRI206,
	KHADAS_MCU_LED_BRI207,
	KHADAS_MCU_LED_BRI208,
	KHADAS_MCU_LED_BRI209,
	KHADAS_MCU_LED_BRI210,
	KHADAS_MCU_LED_BRI211,
	KHADAS_MCU_LED_BRI212,
	KHADAS_MCU_LED_BRI213,
	KHADAS_MCU_LED_BRI214,
	KHADAS_MCU_LED_BRI215,
	KHADAS_MCU_LED_BRI216,
	KHADAS_MCU_LED_BRI217,
	KHADAS_MCU_LED_BRI218,
	KHADAS_MCU_LED_BRI219,
	KHADAS_MCU_LED_BRI220,
	KHADAS_MCU_LED_BRI221,
	KHADAS_MCU_LED_BRI222,
	KHADAS_MCU_LED_BRI223,
	KHADAS_MCU_LED_BRI224,
	KHADAS_MCU_LED_BRI225,
	KHADAS_MCU_LED_BRI226,
	KHADAS_MCU_LED_BRI227,
	KHADAS_MCU_LED_BRI228,
	KHADAS_MCU_LED_BRI229,
	KHADAS_MCU_LED_BRI230,
	KHADAS_MCU_LED_BRI231,
	KHADAS_MCU_LED_BRI232,
	KHADAS_MCU_LED_BRI233,
	KHADAS_MCU_LED_BRI234,
	KHADAS_MCU_LED_BRI235,
	KHADAS_MCU_LED_BRI236,
	KHADAS_MCU_LED_BRI237,
	KHADAS_MCU_LED_BRI238,
	KHADAS_MCU_LED_BRI239,
	KHADAS_MCU_LED_BRI240,
	KHADAS_MCU_LED_BRI241,
	KHADAS_MCU_LED_BRI242,
	KHADAS_MCU_LED_BRI243,
	KHADAS_MCU_LED_BRI244,
	KHADAS_MCU_LED_BRI245,
	KHADAS_MCU_LED_BRI246,
	KHADAS_MCU_LED_BRI247,
	KHADAS_MCU_LED_BRI248,
	KHADAS_MCU_LED_BRI249,
	KHADAS_MCU_LED_BRI250,
	KHADAS_MCU_LED_BRI251,
	KHADAS_MCU_LED_BRI252,
	KHADAS_MCU_LED_BRI253,
	KHADAS_MCU_LED_BRI254,
	KHADAS_MCU_LED_BRI255,

};

enum mcu_fan_mode {
	MCU_FAN_MODE_MANUAL = 0,
	MCU_FAN_MODE_AUTO
};

enum mcu_fan_level {
	MCU_FAN_LEVEL_0 = 0,
	MCU_FAN_LEVEL_1,
	MCU_FAN_LEVEL_2,
	MCU_FAN_LEVEL_3
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
};

struct mcu_data {
	struct i2c_client *client;
	struct class *mcu_class;
	enum khadas_board board;
	enum khadas_board_hwver hwver;
	struct mcu_fan_data fan_data;
	enum khadas_mculed_mode mculed_mode;
	enum khadas_mculed_bri_mode mculed_bri_mode;
};

struct mcu_data *g_mcu_data;
int ageing_test_flag = 0;

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
	if (g_mcu_data->board == KHADAS_BOARD_EDGE2) {
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

		if (g_mcu_data->board == KHADAS_BOARD_EDGE2) {
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
	      }
    }
}

extern int rk_get_temperature(void);

static void fan_work_func(struct work_struct *_work)
{
    if (is_mcu_fan_control_supported()) {
        int temp = -EINVAL;
        struct mcu_fan_data *fan_data = &g_mcu_data->fan_data;

		if (g_mcu_data->board == KHADAS_BOARD_EDGE2) {
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

    if (g_mcu_data->board == KHADAS_BOARD_EDGE2)
        temp = rk_get_temperature();
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

    if (ug_mcu_data->board == KHADAS_BOARD_EDGE2)
        temp = rk_get_temperature();
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


static void mcu_mculed_set(int addr, int mode)
{
	int ret;
	char reg;
	u8 addr_val;

	if (addr >= 0x23 && addr <= 0x24) {
		g_mcu_data->mculed_mode = mode;
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
		g_mcu_data->mculed_bri_mode = mode;
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

	printk("mcu===>reg16=0x%x\n",reg16);
	reg = reg16>>8;
	val = (int)((u8)reg16);

	printk("mcu===>reg=0x%x,val=0x%x\n",reg,val);
	mcu_mculed_set(reg,val);
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
	__ATTR(mculed, 0644, NULL, store_mculed_mode),
};

static void create_mcu_attrs(void) {
	int i;

	g_mcu_data->mcu_class = class_create(THIS_MODULE, "mcu");
	if (IS_ERR(g_mcu_data->mcu_class)) {
		pr_err("create mcu_class debug class fail\n");

		return;
	}
	for (i = 0; i < ARRAY_SIZE(mcu_class_attrs); i++) {
		if (class_create_file(g_mcu_data->mcu_class, &mcu_class_attrs[i]));
			pr_err("create mcu attribute %s fail\n",
							mcu_class_attrs[i].attr.name);
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
					g_mcu_data->board = KHADAS_BOARD_EDGE2;
			else
					g_mcu_data->board = KHADAS_BOARD_NONE;

			if (g_mcu_data->board == KHADAS_BOARD_EDGE2) {
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

static int mcu_remove(struct i2c_client *client)
{
	kfree(g_mcu_data);
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
MODULE_DESCRIPTION("Khadas Edge2 MCU control driver");
MODULE_LICENSE("GPL");
