/*
 * drivers/net/phy/realtek.c
 *
 * Driver for Realtek PHYs
 *
 * Author: Johnson Leung <r58129@freescale.com>
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

MODULE_DESCRIPTION("Realtek PHY driver");
MODULE_AUTHOR("Johnson Leung");
MODULE_LICENSE("GPL");

static int rtl8211f_config_init(struct phy_device *phydev)
{
	int val;
	int bmcr = 0;

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

	/* rx reg 21 bit 3 tx reg 17 bit 8*/
	/* phy_write(phydev, 0x1f, 0xd08);
	 * val =  phy_read(phydev, 0x15);
	 * phy_write(phydev, 0x15,val| 1<<21);
	 */

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
	{ }
};

MODULE_DEVICE_TABLE(mdio, realtek_tbl);
