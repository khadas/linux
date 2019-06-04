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
#include <linux/bitops.h>
#include <linux/phy.h>
#include <linux/module.h>

#define RTL821x_PHYSR				0x11
#define RTL821x_PHYSR_DUPLEX			BIT(13)
#define RTL821x_PHYSR_SPEED			GENMASK(15, 14)

#define RTL821x_INER				0x12
#define RTL8211B_INER_INIT			0x6400
#define RTL8211E_INER_LINK_STATUS		BIT(10)
#define RTL8211E_INER_ANEG_COMPLETED		BIT(11)
#define RTL8211E_INER_PAGE_RECEIVED		BIT(12)
#define RTL8211E_INER_ANEG_ERROR		BIT(15)
#define RTL8211F_INER_LINK_STATUS		BIT(4)
#define RTL8211F_INER_PHY_REGISTER_ACCESSIBLE	BIT(5)
#define RTL8211F_INER_WOL_PME			BIT(7)
#define RTL8211F_INER_ALDPS_STATE_CHANGE	BIT(9)
#define RTL8211F_INER_JABBER			BIT(10)

#define RTL821x_INSR				0x13

#define RTL821x_PAGE_SELECT			0x1f

#define RTL8211F_INSR				0x1d

#define RTL8211F_RX_DELAY_REG			0x15
#define RTL8211F_RX_DELAY_EN			BIT(3)
#define RTL8211F_TX_DELAY_REG			0x11
#define RTL8211F_TX_DELAY_EN			BIT(8)

#define RTL8201F_ISR				0x1e
#define RTL8201F_IER				0x13

#define RTL8211F_INTBCR				0x16
#define RTL8211F_INTBCR_INTB_PMEB		BIT(5)

#define RTL8211F_MAC_ADDR_CTRL0 0x10
#define RTL8211F_MAC_ADDR_CTRL1 0x11
#define RTL8211F_MAC_ADDR_CTRL2 0x12
#define RTL8211F_WOL_CTRL 0x10
#define RTL8211F_WOL_RST 0x11
#define RTL8211F_MAX_PACKET_CTRL 0x11
#define RTL8211F_BMCR   0x00
#define RTL821x_EPAGSR      0x1f

MODULE_DESCRIPTION("Realtek PHY driver");
MODULE_AUTHOR("Johnson Leung");
MODULE_LICENSE("GPL");

static void rtl8211f_config_mac_addr(struct phy_device *phydev);
static void rtl8211f_config_pin_as_pmeb(struct phy_device *phydev);
static void rtl8211f_config_wakeup_frame_mask(struct phy_device *phydev);
static void rtl8211f_config_max_packet(struct phy_device *phydev);
static void rtl8211f_config_pad_isolation(struct phy_device *phydev, int enable);
static void rtl8211f_config_wol(struct phy_device *phydev, int enable);
static void rtl8211f_config_speed(struct phy_device *phydev, int mode);

static u8 mac_addr[] = {0, 0, 0, 0, 0, 0};
struct phy_device *g_phydev;
static int factory_test = 0;
static u8 test_mac_addr[] = { 0x22, 0x22, 0x22, 0x22, 0x22, 0x22 };


void set_test(int flag)
{
   factory_test = flag;
}

int get_test(void)
{
   return factory_test;
}

extern int mcu_get_wol_status(void);
int get_wol_state(void){
   return mcu_get_wol_status();
}

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

void realtek_enable_wol(int enable, bool is_shutdown)
{
   enable_wol(enable, is_shutdown);
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

#ifdef CONFIG_PM
void rtl8211f_shutdown(void) {
   enable_wol(get_wol_state(), true);
}
#endif

#ifdef CONFIG_PM_SLEEP
void rtl8211f_suspend(void) {
   if (get_wol_state() && g_phydev) {
       rtl8211f_config_pin_as_pmeb(g_phydev);
       rtl8211f_config_mac_addr(g_phydev);
       rtl8211f_config_max_packet(g_phydev);
       rtl8211f_config_wol(g_phydev, 1);
       rtl8211f_config_wakeup_frame_mask(g_phydev);
       rtl8211f_config_pad_isolation(g_phydev, 1);
   }
}

void rtl8211f_resume(void) {
   if (get_wol_state() && g_phydev) {
       rtl8211f_config_speed(g_phydev, 1);
       rtl8211f_config_wol(g_phydev, 0);
       rtl8211f_config_pad_isolation(g_phydev, 0);
   }
}
#endif

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
   if (get_test() == 1) {
     printk("=================factory test===============\n");
     phy_write(phydev, RTL8211F_MAC_ADDR_CTRL0, test_mac_addr[1] << 8 | test_mac_addr[0]);
     phy_write(phydev, RTL8211F_MAC_ADDR_CTRL1, test_mac_addr[3] << 8 | test_mac_addr[2]);
     phy_write(phydev, RTL8211F_MAC_ADDR_CTRL2, test_mac_addr[5] << 8 | test_mac_addr[4]);

   } else {
     phy_write(phydev, RTL8211F_MAC_ADDR_CTRL0, mac_addr[1] << 8 | mac_addr[0]);
     phy_write(phydev, RTL8211F_MAC_ADDR_CTRL1, mac_addr[3] << 8 | mac_addr[2]);
     phy_write(phydev, RTL8211F_MAC_ADDR_CTRL2, mac_addr[5] << 8 | mac_addr[4]);
   }
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


static int rtl8211x_page_read(struct phy_device *phydev, u16 page, u16 address)
{
	int ret;

	ret = phy_write(phydev, RTL821x_PAGE_SELECT, page);
	if (ret)
		return ret;

	ret = phy_read(phydev, address);

	/* restore to default page 0 */
	phy_write(phydev, RTL821x_PAGE_SELECT, 0x0);

	return ret;
}

static int rtl8211x_page_write(struct phy_device *phydev, u16 page,
			       u16 address, u16 val)
{
	int ret;

	ret = phy_write(phydev, RTL821x_PAGE_SELECT, page);
	if (ret)
		return ret;

	ret = phy_write(phydev, address, val);

	/* restore to default page 0 */
	phy_write(phydev, RTL821x_PAGE_SELECT, 0x0);

	return ret;
}

static int rtl8211x_page_mask_bits(struct phy_device *phydev, u16 page,
				   u16 address, u16 mask, u16 set)
{
	int ret;
	u16 val;

	ret = rtl8211x_page_read(phydev, page, address);
	if (ret < 0)
		return ret;

	val = ret & 0xffff;
	val &= ~mask;
	val |= (set & mask);

	return rtl8211x_page_write(phydev, page, address, val);
}

static int rtl8201_ack_interrupt(struct phy_device *phydev)
{
	int err;

	err = phy_read(phydev, RTL8201F_ISR);

	return (err < 0) ? err : 0;
}

static int rtl821x_ack_interrupt(struct phy_device *phydev)
{
	int err;

	err = phy_read(phydev, RTL821x_INSR);

	return (err < 0) ? err : 0;
}

static int rtl8211f_ack_interrupt(struct phy_device *phydev)
{
	int err;

	err = rtl8211x_page_read(phydev, 0xa43, RTL8211F_INSR);

	return (err < 0) ? err : 0;
}

static int rtl8201_config_intr(struct phy_device *phydev)
{
	u16 val;

	if (phydev->interrupts == PHY_INTERRUPT_ENABLED)
		val = BIT(13) | BIT(12) | BIT(11);
	else
		val = 0;

	return rtl8211x_page_write(phydev, 0x7, RTL8201F_IER, val);
}

static int rtl8211b_config_intr(struct phy_device *phydev)
{
	int err;

	if (phydev->interrupts == PHY_INTERRUPT_ENABLED)
		err = phy_write(phydev, RTL821x_INER,
				RTL8211B_INER_INIT);
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

static int rtl8211f_config_intr(struct phy_device *phydev)
{
	int err;
	u16 val;

	if (phydev->interrupts == PHY_INTERRUPT_ENABLED) {
		/*
		 * The interrupt pin has two functions:
		 * 0: INTB: it acts as interrupt pin which can be configured
		 *    through RTL821x_INER and the status can be read through
		 *    RTL8211F_INSR
		 * 1: PMEB: a special "Power Management Event" mode for
		 *    Wake-on-LAN operation (with support for a "pulse low"
		 *    wave format). Interrupts configured through RTL821x_INER
		 *    will not work in this mode
		 *
		 * select INTB mode in the "INTB pin control" register to
		 * ensure that the interrupt pin is in the correct mode.
		 */
		err = rtl8211x_page_mask_bits(phydev, 0xd40, RTL8211F_INTBCR,
					      RTL8211F_INTBCR_INTB_PMEB, 0);
		if (err)
			return err;

		val = RTL8211F_INER_LINK_STATUS;
	} else {
		val = 0;
	}

	return rtl8211x_page_write(phydev, 0xa42, RTL821x_INER, val);
}

static int rtl8211f_config_init(struct phy_device *phydev)
{
	int ret;
	u16 val;

	ret = genphy_config_init(phydev);
	if (ret < 0)
		return ret;

	/*
	 * enable TX-delay for rgmii-id and rgmii-txid, otherwise disable it.
	 * this is needed because it can be enabled by pin strapping and
	 * conflict with the TX-delay configured by the MAC.
	 */
	if (phydev->interface == PHY_INTERFACE_MODE_RGMII_ID ||
	    phydev->interface == PHY_INTERFACE_MODE_RGMII_TXID)
		val = RTL8211F_TX_DELAY_EN;
	else
		val = 0;

	ret = rtl8211x_page_mask_bits(phydev, 0xd08, RTL8211F_TX_DELAY_REG,
				      RTL8211F_TX_DELAY_EN, val);
	if (ret)
		return ret;

	/*
	 * enable RX-delay for rgmii-id and rgmii-rxid, otherwise disable it.
	 * this is needed because it can be enabled by pin strapping and
	 * conflict with the RX-delay configured by the MAC.
	 */
	if (phydev->interface == PHY_INTERFACE_MODE_RGMII_ID ||
	    phydev->interface == PHY_INTERFACE_MODE_RGMII_RXID)
		val = RTL8211F_RX_DELAY_EN;
	else
		val = 0;

	ret = rtl8211x_page_mask_bits(phydev, 0xd08, RTL8211F_RX_DELAY_REG,
				      RTL8211F_RX_DELAY_EN, val);
	if (ret)
		return ret;

	rtl8211f_config_pin_as_pmeb(phydev);
	rtl8211f_config_speed(phydev, 1);
	g_phydev = kzalloc(sizeof(struct phy_device), GFP_KERNEL);
	if (g_phydev == NULL)
		return -ENOMEM;
	g_phydev = phydev;

	if (3 == mcu_get_wol_status())
		enable_wol(3, false);

	return 0;
}

static struct phy_driver realtek_drvs[] = {
	{
		.phy_id         = 0x00008201,
		.name           = "RTL8201CP Ethernet",
		.phy_id_mask    = 0x0000ffff,
		.features       = PHY_BASIC_FEATURES,
		.flags          = PHY_HAS_INTERRUPT,
		.config_aneg    = &genphy_config_aneg,
		.read_status    = &genphy_read_status,
		.driver         = { .owner = THIS_MODULE,},
	}, {
		.phy_id		= 0x001cc816,
		.name		= "RTL8201F 10/100Mbps Ethernet",
		.phy_id_mask	= 0x001fffff,
		.features	= PHY_BASIC_FEATURES,
		.flags		= PHY_HAS_INTERRUPT,
		.config_aneg	= &genphy_config_aneg,
		.read_status	= &genphy_read_status,
		.ack_interrupt	= &rtl8201_ack_interrupt,
		.config_intr	= &rtl8201_config_intr,
		.suspend	= genphy_suspend,
		.resume		= genphy_resume,
	}, {
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
	}, {
		.phy_id		= 0x001cc914,
		.name		= "RTL8211DN Gigabit Ethernet",
		.phy_id_mask	= 0x001fffff,
		.features	= PHY_GBIT_FEATURES,
		.flags		= PHY_HAS_INTERRUPT,
		.config_aneg	= genphy_config_aneg,
		.read_status	= genphy_read_status,
		.ack_interrupt	= rtl821x_ack_interrupt,
		.config_intr	= rtl8211e_config_intr,
		.suspend	= genphy_suspend,
		.resume		= genphy_resume,
		.driver		= { .owner = THIS_MODULE,},
	}, {
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
	}, {
		.phy_id		= 0x001cc916,
		.name		= "RTL8211F Gigabit Ethernet",
		.phy_id_mask	= 0x001fffff,
		.features	= PHY_GBIT_FEATURES,
		.flags		= PHY_HAS_INTERRUPT,
		.config_aneg	= &genphy_config_aneg,
		.config_init	= &rtl8211f_config_init,
		.read_status	= &genphy_read_status,
		.ack_interrupt	= &rtl8211f_ack_interrupt,
		.config_intr	= &rtl8211f_config_intr,
		.suspend	= genphy_suspend,
		.resume		= genphy_resume,
		.driver		= { .owner = THIS_MODULE },
	},
};

module_phy_driver(realtek_drvs);

static struct mdio_device_id __maybe_unused realtek_tbl[] = {
	{ 0x001cc816, 0x001fffff },
	{ 0x001cc912, 0x001fffff },
	{ 0x001cc914, 0x001fffff },
	{ 0x001cc915, 0x001fffff },
	{ 0x001cc916, 0x001fffff },
	{ }
};

MODULE_DEVICE_TABLE(mdio, realtek_tbl);
