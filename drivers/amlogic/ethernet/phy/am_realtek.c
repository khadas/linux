/*
 * drivers/amlogic/ethernet/phy/am_realtek.c
 *
 * Copyright (C) 2015 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
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
#define RTL8211F_MMD_CTRL       0x0D
#define RTL8211F_MMD_DATA       0x0E
#define	RTL8211E_INER_LINK_STAT	0x10

MODULE_DESCRIPTION("Realtek PHY driver");
MODULE_AUTHOR("Johnson Leung");
MODULE_LICENSE("GPL");

static int rtl8211f_config_init(struct phy_device *phydev)
{
	int val;
/* we want to disable eee */
	phy_write(phydev, RTL8211F_MMD_CTRL, 0x7);
	phy_write(phydev, RTL8211F_MMD_DATA, 0x3c);
	phy_write(phydev, RTL8211F_MMD_CTRL, 0x4007);
	phy_write(phydev, RTL8211F_MMD_DATA, 0x0);
/* disable 1000m adv*/
	val = phy_read(phydev, 0x9);
	phy_write(phydev, 0x9, val&(~(1<<9)));
  /* rx reg 21 bit 3 tx reg 17 bit 8*/
	/*phy_write(phydev, 0x1f, 0xd08);
	val =  phy_read(phydev, 0x15);
	phy_write(phydev, 0x15,val| 1<<21);*/
	return 0;
	/* Enable Auto Power Saving mode */
}
/* RTL8211F */
static struct phy_driver rtl8211f_driver = {
	.phy_id		= 0x001cc916,
	.name		= "RTL8211F Gigabit Ethernet",
	.phy_id_mask	= 0x001fffff,
#if 1
	.features	= PHY_GBIT_FEATURES | SUPPORTED_Pause |
			  SUPPORTED_Asym_Pause,
	.flags		= PHY_HAS_INTERRUPT | PHY_HAS_MAGICANEG,
#else
	.features	= PHY_BASIC_FEATURES | SUPPORTED_Pause |
			  SUPPORTED_Asym_Pause,
	.flags		= PHY_HAS_INTERRUPT | PHY_HAS_MAGICANEG,
#endif
	.config_aneg	= &genphy_config_aneg,
	.read_status	= &genphy_read_status,
	.config_init	= &rtl8211f_config_init,
	.suspend	= genphy_suspend,
	.resume		= genphy_resume,
	.driver		= { .owner = THIS_MODULE,},
};

static int __init realtek_init(void)
{
	return phy_driver_register(&rtl8211f_driver);
}

static void __exit realtek_exit(void)
{
	phy_driver_unregister(&rtl8211f_driver);
}

module_init(realtek_init);
module_exit(realtek_exit);

static struct mdio_device_id __maybe_unused realtek_tbl[] = {
	{ 0x001cc916, 0x001fffff },
	{ }
};

MODULE_DEVICE_TABLE(mdio, realtek_tbl);
