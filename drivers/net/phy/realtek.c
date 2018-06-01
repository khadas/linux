/*
 * drivers/net/phy/realtek.c
 *
 * Driver for Realtek PHYs
 *
 * Author: Terry <terry@szwesion.com>
 *
 * Copyright (c) 2004 Freescale Semiconductor, Inc.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */
#include <linux/phy.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <linux/err.h>


#define RTL821x_PHYSR		0x11
#define RTL821x_PHYSR_DUPLEX	0x2000
#define RTL821x_PHYSR_SPEED	0xc000
#define RTL821x_INER		0x12
#define RTL821x_INER_INIT	0x6400
#define RTL821x_INSR		0x13
#define RTL8211F_MMD_CTRL   0x0D
#define RTL8211F_MMD_DATA   0x0E
#define RTL821x_PHYCR2		0x19
#define RTL821x_CLKOUT_EN	0x1
#define RTL821x_EPAGSR		0x1f
#define RTL821x_LCR		    0x10

#define	RTL8211E_INER_LINK_STATUS	0x400
#define RTL8211F_MAC_ADDR_CTRL0 0x10
#define RTL8211F_MAC_ADDR_CTRL1 0x11
#define RTL8211F_MAC_ADDR_CTRL2 0x12
#define RTL8211F_WOL_CTRL 0x10
#define RTL8211F_WOL_RST 0x11
#define RTL8211F_MAX_PACKET_CTRL 0x11
#define RTL8211F_BMCR   0x00

#define WOL_REG    0x21

MODULE_DESCRIPTION("Realtek PHY driver");
MODULE_AUTHOR("Terry");
MODULE_LICENSE("GPL");
static void rtl8211f_config_mac_addr(struct phy_device *phydev);
static void rtl8211f_config_pin_as_pmeb(struct phy_device *phydev);
static void rtl8211f_config_wakeup_frame_mask(struct phy_device *phydev);
static void rtl8211f_config_max_packet(struct phy_device *phydev);
static void rtl8211f_config_pad_isolation(struct phy_device *phydev, int enable);
static void rtl8211f_config_wol(struct phy_device *phydev, int enable);
static void rtl8211f_config_speed(struct phy_device *phydev, int mode);

static u8 mac_addr[] = {0, 0, 0, 0, 0, 0};

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
struct phy_device *g_phydev;

struct wol_data {
    int enable;
	struct i2c_client *client;
	struct class *wol_class;
};

struct wol_data *g_wol_data;

int get_wol_state(void){
	if (g_wol_data != NULL) {
		return g_wol_data->enable;
	}
	return 0;
}

static unsigned char chartonum(char c)
{
    if (c >= '0' && c <= '9')
         return c - '0';
    if (c >= 'A' && c <= 'F')
         return (c - 'A') + 10;
    if (c >= 'a' && c <= 'f')
         return (c - 'a') + 10;
    return 0;
}

static int __init init_mac_addr(char *line)
{
	unsigned char mac[6];
	int i = 0;
	for (i = 0; i < 6 && line[0] != '\0' && line[1] != '\0'; i++) {
		mac[i] = chartonum(line[0]) << 4 | chartonum(line[1]);
		line += 3;
	}
	memcpy(mac_addr, mac, 6);
	printk("realtek init mac-addr: %x:%x:%x:%x:%x:%x\n",
		mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4],
		mac_addr[5]);

	return 1;
}
__setup("androidboot.mac=",init_mac_addr);


void enable_wol(int enable, bool is_shutdown)
{
	if (g_phydev != NULL) {
		if (enable == 1 || enable == 3) {
			rtl8211f_config_speed(g_phydev, is_shutdown ? 0:1);
			rtl8211f_config_mac_addr(g_phydev);
			rtl8211f_config_max_packet(g_phydev);
			rtl8211f_config_wol(g_phydev, 1);
			rtl8211f_config_wakeup_frame_mask(g_phydev);
			rtl8211f_config_pad_isolation(g_phydev, 1);
		} else {
			rtl8211f_config_wol(g_phydev, 0);
		}
	}
}
void rtl8211f_shutdown(void)
{
	enable_wol(get_wol_state(), true);
}

static void rtl8211f_early_suspend(struct early_suspend *h)
{
	if (get_wol_state()) {
		rtl8211f_config_mac_addr(g_phydev);
		rtl8211f_config_max_packet(g_phydev);
		rtl8211f_config_wol(g_phydev, 1);
		rtl8211f_config_wakeup_frame_mask(g_phydev);
		rtl8211f_config_pad_isolation(g_phydev, 1);
	}

}

static void rtl8211f_late_resume(struct early_suspend *h)
{
	if (get_wol_state()) {
		rtl8211f_config_speed(g_phydev, 1);
		rtl8211f_config_wol(g_phydev, 0);
		rtl8211f_config_pad_isolation(g_phydev, 0);
	}
}

static struct early_suspend rtl8211f_early_suspend_handler = {
	.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN - 15,
	.suspend = rtl8211f_early_suspend,
	.resume = rtl8211f_late_resume,
};

static void rtl8211f_config_speed(struct phy_device *phydev, int mode)
{
	phy_write(phydev, RTL821x_EPAGSR, 0x0); /*set page 0x0*/
	if (mode == 1) {
		phy_write(phydev, RTL8211F_BMCR, 0x1040);  /* 1000Mbps */
	} else {
		phy_write(phydev, RTL8211F_BMCR, 0x0); /* 10Mbps */
	}
}
static void rtl8211f_config_mac_addr(struct phy_device *phydev)
{
	phy_write(phydev, RTL821x_EPAGSR, 0xd8c); /*set page 0xd8c*/
	phy_write(phydev, RTL8211F_MAC_ADDR_CTRL0, mac_addr[1] << 8 | mac_addr[0]);
	phy_write(phydev, RTL8211F_MAC_ADDR_CTRL1, mac_addr[3] << 8 | mac_addr[2]);
	phy_write(phydev, RTL8211F_MAC_ADDR_CTRL2, mac_addr[5] << 8 | mac_addr[4]);
	phy_write(phydev, RTL821x_EPAGSR, 0); /*set page 0*/
}

static void rtl8211f_config_pin_as_pmeb(struct phy_device *phydev)
{
	int val;
	phy_write(phydev, RTL821x_EPAGSR, 0xd40); /*set page 0xd40*/
	val = phy_read(phydev, 0x16);
	val = val | 0x20;
	phy_write(phydev, 0x16, val);
	phy_write(phydev, RTL821x_EPAGSR, 0); /*set page 0*/
}

static void rtl8211f_config_wakeup_frame_mask(struct phy_device *phydev)
{
	phy_write(phydev, RTL821x_EPAGSR, 0xd80); /*set page 0xd80*/
	phy_write(phydev, 0x10, 0x3000);
	phy_write(phydev, 0x11, 0x0020);
	phy_write(phydev, 0x12, 0x03c0);
	phy_write(phydev, 0x13, 0x0000);
	phy_write(phydev, 0x14, 0x0000);
	phy_write(phydev, 0x15, 0x0000);
	phy_write(phydev, 0x16, 0x0000);
	phy_write(phydev, 0x17, 0x0000);
	phy_write(phydev, RTL821x_EPAGSR, 0); /*set page 0*/
}

static void rtl8211f_config_max_packet(struct phy_device *phydev)
{
	phy_write(phydev, RTL821x_EPAGSR, 0xd8a); /*set page 0xd8a*/
	phy_write(phydev, RTL8211F_MAX_PACKET_CTRL, 0x9fff);
	phy_write(phydev, RTL821x_EPAGSR, 0); /*set page 0*/
}

static void rtl8211f_config_pad_isolation(struct phy_device *phydev, int enable)
{
	int val;
	phy_write(phydev, RTL821x_EPAGSR, 0xd8a); /*set page 0xd8a*/
	val = phy_read(phydev, 0x13);
	if (enable)
		val = val | 0x1000;
	else
		val = val & 0x7fff;
	phy_write(phydev, 0x13, val);
	phy_write(phydev, RTL821x_EPAGSR, 0); /*set page 0*/
}

static void rtl8211f_config_wol(struct phy_device *phydev, int enable)
{
	int val;
	phy_write(phydev, RTL821x_EPAGSR, 0xd8a); /*set page 0xd8a*/
	if (enable)
		phy_write(phydev, RTL8211F_WOL_CTRL, 0x1000);
	else {
		phy_write(phydev, RTL8211F_WOL_CTRL, 0);
		val =  phy_read(phydev,  RTL8211F_WOL_RST);
		phy_write(phydev, RTL8211F_WOL_RST, val & 0x7fff);
	}
	phy_write(phydev, RTL821x_EPAGSR, 0); /*set page 0*/
}
#endif

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

static int wol_i2c_read_regs(struct i2c_client *client,
		u8 reg, u8 buf[], unsigned len)
{
	int ret;
	ret = i2c_master_reg8_recv(client, reg, buf, len);
	return ret;
}

static int wol_i2c_write_regs(struct i2c_client *client,
		u8 reg, u8 const buf[], __u16 len)
{
	int ret;
	ret = i2c_master_reg8_send(client, reg, buf, (int)len);
	return ret;
}


static ssize_t store_enable(struct class *cls, struct class_attribute *attr,
		        const char *buf, size_t count)
{
	u8 reg[2];
	int ret;
	int enable;
	int state;

	if (kstrtoint(buf, 0, &enable))
		return -EINVAL;

	ret = wol_i2c_read_regs(g_wol_data->client, WOL_REG, reg, 1);
	if (ret < 0) {
		printk("write wol state err\n");
		return ret;
	}
	state = (int)reg[0];
	reg[0] = enable|(state&0x02);
	ret = wol_i2c_write_regs(g_wol_data->client, WOL_REG, reg, 1);
	if (ret < 0) {
		printk("write wol state err\n");
		return ret;
	}

	g_wol_data->enable = reg[0];
	enable_wol(g_wol_data->enable,false);

	printk("write wol state: %d\n", g_wol_data->enable);
	return count;
}

static ssize_t show_enable(struct class *cls,
		        struct class_attribute *attr, char *buf)
{
	int enable;
	enable = g_wol_data->enable&0x01;
	return sprintf(buf, "%d\n", enable);
}

static struct class_attribute wol_class_attrs[] = {
	__ATTR(enable, 0666, show_enable, store_enable),
};

void create_wol_enable_attr(void)
{
	int i;
	printk("%s\n",__func__);
	g_wol_data->wol_class = class_create(THIS_MODULE, "wol");
	if (IS_ERR(g_wol_data->wol_class)) {
		pr_err("create wol_class debug class fail\n");
		return;
	}
	for (i = 0; i < ARRAY_SIZE(wol_class_attrs); i++) {
		if (class_create_file(g_wol_data->wol_class, &wol_class_attrs[i]))
			pr_err("create wol attribute %s fail\n", wol_class_attrs[i].attr.name);
	}
}

static int wol_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	u8 reg[2];
	int ret;

    if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
		return -ENODEV;
	g_wol_data = kzalloc(sizeof(struct wol_data), GFP_KERNEL);

	if (g_wol_data == NULL)
		return -ENOMEM;

	g_wol_data->client = client;
	ret = wol_i2c_read_regs(client, WOL_REG, reg, 1);
	if (ret < 0)
		goto exit;
	g_wol_data->enable = (int)reg[0];
	create_wol_enable_attr();
	printk("%s,wol enable=%d\n",__func__ ,g_wol_data->enable);

	if (g_wol_data->enable == 3)
	enable_wol(g_wol_data->enable, false);


	reg[0] = 0x01;
	ret = wol_i2c_write_regs(client, 0x87, reg, 1);
	if (ret < 0) {
		printk("write mcu err\n");
		goto  exit;
	}
	return 0;
exit:
	return ret;

}
static const struct i2c_device_id wol_id[] = {
	{ "khadas-wol", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, wol_id);

static struct of_device_id wol_dt_ids[] = {
	{ .compatible = "khadas-wol" },
	{},
};

struct i2c_driver wol_driver = {
    .driver     = {
         .name   = "khadas-wol",
		 .owner  = THIS_MODULE,
		 .of_match_table = of_match_ptr(wol_dt_ids),
	},
	.probe      = wol_probe,
	.id_table   = wol_id,
};

static int rtl8211f_config_init(struct phy_device *phydev)
{
	int val;
	int bmcr = 0;
	int ret;

	/* close CLOCK output */
	val = phy_read(phydev, RTL821x_PHYCR2);
	if (val < 0)
		return val;
	phy_write(phydev, RTL821x_EPAGSR, 0xa43);
	phy_write(phydev, RTL821x_PHYCR2,
			(val & (~RTL821x_CLKOUT_EN)));
	phy_write(phydev, RTL821x_EPAGSR, 0x0);
	phy_write(phydev, MII_BMCR,
			BMCR_RESET|BMCR_ANENABLE|BMCR_ANRESTART);

	/* wait for ready */
	do {
		bmcr = phy_read(phydev, MII_BMCR);
		if (bmcr < 0)
			return bmcr;
	} while (bmcr & BMCR_RESET);

	/* we want to disable eee */
	phy_write(phydev, RTL8211F_MMD_CTRL, 0x7);
	phy_write(phydev, RTL8211F_MMD_DATA, 0x3c);
	phy_write(phydev, RTL8211F_MMD_CTRL, 0x4007);
	phy_write(phydev, RTL8211F_MMD_DATA, 0x0);

	/* disable 1000m adv*/
	val = phy_read(phydev, 0x9);
	phy_write(phydev, 0x9, val&(~(1<<9)));

	phy_write(phydev, RTL821x_EPAGSR, 0xd04); /*set page 0xd04*/
	phy_write(phydev, RTL821x_LCR, 0XC171); /*led configuration*/
	phy_write(phydev, RTL821x_EPAGSR, 0x0);

	rtl8211f_config_pin_as_pmeb(phydev);
	rtl8211f_config_speed(phydev, 1);
	/* rx reg 21 bit 3 tx reg 17 bit 8*/
	/* phy_write(phydev, 0x1f, 0xd08);
	 * val =  phy_read(phydev, 0x15);
	 * phy_write(phydev, 0x15,val| 1<<21);
	 */

	if (g_phydev == NULL) {
		g_phydev = kzalloc(sizeof(struct phy_device), GFP_KERNEL);
		if (g_phydev == NULL)
			return -ENOMEM;
		g_phydev = phydev;
#ifdef CONFIG_HAS_EARLYSUSPEND
		register_early_suspend(&rtl8211f_early_suspend_handler);
#endif
		ret = i2c_add_driver(&wol_driver);
		if (ret) {
			pr_err("error khadas-wol probe: add i2c_driver failed\n");
		}
	}
	else {
		if (g_wol_data->enable == 3)
			enable_wol(g_wol_data->enable, false);
	}
	return 0;
}

static int rtl821x_ack_interrupt(struct phy_device *phydev)
{
	int err;

	err = phy_read(phydev, RTL821x_INSR);

	return (err < 0) ? err : 0;
}

static int rtl8211b_config_intr(struct phy_device *phydev)
{
	int err;

	if (phydev->interrupts == PHY_INTERRUPT_ENABLED)
		err = phy_write(phydev, RTL821x_INER,
				RTL821x_INER_INIT);
	else
		err = phy_write(phydev, RTL821x_INER, 0);

	return err;
}

static int rtl8211e_config_intr(struct phy_device *phydev)
{
	int err;

	if (phydev->interrupts == PHY_INTERRUPT_ENABLED)
		err = phy_write(phydev, RTL821x_INER,
				RTL8211E_INER_LINK_STATUS);
	else
		err = phy_write(phydev, RTL821x_INER, 0);

	return err;
}

/* RTL8201CP */
static struct phy_driver rtl8201cp_driver = {
	.phy_id         = 0x00008201,
	.name           = "RTL8201CP Ethernet",
	.phy_id_mask    = 0x0000ffff,
	.features       = PHY_BASIC_FEATURES,
	.flags          = PHY_HAS_INTERRUPT,
	.config_aneg    = &genphy_config_aneg,
	.read_status    = &genphy_read_status,
	.driver         = { .owner = THIS_MODULE,},
};

/* RTL8211B */
static struct phy_driver rtl8211b_driver = {
	.phy_id		= 0x001cc912,
	.name		= "RTL8211B Gigabit Ethernet",
	.phy_id_mask	= 0x001fffff,
	.features	= PHY_GBIT_FEATURES,
	.flags		= PHY_HAS_INTERRUPT,
	.config_aneg	= &genphy_config_aneg,
	.read_status	= &genphy_read_status,
	.ack_interrupt	= &rtl821x_ack_interrupt,
	.config_intr	= &rtl8211b_config_intr,
	.driver		= { .owner = THIS_MODULE,},
};

/* RTL8211E */
static struct phy_driver rtl8211e_driver = {
	.phy_id		= 0x001cc915,
	.name		= "RTL8211E Gigabit Ethernet",
	.phy_id_mask	= 0x001fffff,
	.features	= PHY_GBIT_FEATURES,
	.flags		= PHY_HAS_INTERRUPT,
	.config_aneg	= &genphy_config_aneg,
	.read_status	= &genphy_read_status,
	.ack_interrupt	= &rtl821x_ack_interrupt,
	.config_intr	= &rtl8211e_config_intr,
	.suspend	= genphy_suspend,
	.resume		= genphy_resume,
	.driver		= { .owner = THIS_MODULE,},
};

/* RTL8211F */
static struct phy_driver rtl8211f_driver = {
	.phy_id		= 0x001cc916,
	.name		= "RTL8211F Gigabit Ethernet",
	.phy_id_mask	= 0x001fffff,
	.features	= PHY_GBIT_FEATURES | SUPPORTED_Pause |
			  SUPPORTED_Asym_Pause,
	.flags		= PHY_HAS_INTERRUPT | PHY_HAS_MAGICANEG,
	.config_init	= rtl8211f_config_init,
	.config_aneg	= &genphy_config_aneg,
	.read_status	= &genphy_read_status,
	.suspend	= genphy_suspend,
	.resume		= genphy_resume,
	.driver		= { .owner = THIS_MODULE,},
};

static int __init realtek_init(void)
{
	int ret;

	ret = phy_driver_register(&rtl8201cp_driver);
	if (ret < 0)
		return -ENODEV;
	ret = phy_driver_register(&rtl8211b_driver);
	if (ret < 0)
		return -ENODEV;
	ret = phy_driver_register(&rtl8211e_driver);
	if (ret < 0)
		return -ENODEV;
	return phy_driver_register(&rtl8211f_driver);
}

static void __exit realtek_exit(void)
{
	phy_driver_unregister(&rtl8211b_driver);
	phy_driver_unregister(&rtl8211e_driver);
	phy_driver_unregister(&rtl8211f_driver);
}

module_init(realtek_init);
module_exit(realtek_exit);

static struct mdio_device_id __maybe_unused realtek_tbl[] = {
	{ 0x001cc912, 0x001fffff },
	{ 0x001cc915, 0x001fffff },
	{ 0x001cc916, 0x001fffff },
};

MODULE_DEVICE_TABLE(mdio, realtek_tbl);
