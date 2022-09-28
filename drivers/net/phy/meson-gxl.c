// SPDX-License-Identifier: GPL-2.0+
/*
 * Amlogic Meson GXL Internal PHY Driver
 *
 * Copyright (C) 2015 Amlogic, Inc. All rights reserved.
 * Copyright (C) 2016 BayLibre, SAS. All rights reserved.
 * Author: Neil Armstrong <narmstrong@baylibre.com>
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mii.h>
#include <linux/ethtool.h>
#include <linux/phy.h>
#include <linux/netdevice.h>
#include <linux/bitfield.h>
#ifdef CONFIG_AMLOGIC_ETH_PRIVE
#include "../../amlogic/ethernet/phy/phy_debug.h"
#define MAILBOX_SUPPORT	0x0
#endif

#define TSTCNTL		20
#define  TSTCNTL_READ		BIT(15)
#define  TSTCNTL_WRITE		BIT(14)
#define  TSTCNTL_REG_BANK_SEL	GENMASK(12, 11)
#define  TSTCNTL_TEST_MODE	BIT(10)
#define  TSTCNTL_READ_ADDRESS	GENMASK(9, 5)
#define  TSTCNTL_WRITE_ADDRESS	GENMASK(4, 0)
#define TSTREAD1	21
#define TSTWRITE	23
#define INTSRC_FLAG	29
#define  INTSRC_ANEG_PR		BIT(1)
#define  INTSRC_PARALLEL_FAULT	BIT(2)
#define  INTSRC_ANEG_LP_ACK	BIT(3)
#define  INTSRC_LINK_DOWN	BIT(4)
#define  INTSRC_REMOTE_FAULT	BIT(5)
#define  INTSRC_ANEG_COMPLETE	BIT(6)
#define INTSRC_MASK	30

#define BANK_ANALOG_DSP		0
#define BANK_WOL		1
#define BANK_BIST		3

/* WOL Registers */
#define LPI_STATUS	0xc
#define  LPI_STATUS_RSV12	BIT(12)

/* BIST Registers */
#define FR_PLL_CONTROL	0x1b
#define FR_PLL_DIV0	0x1c
#define FR_PLL_DIV1	0x1d

static int meson_gxl_open_banks(struct phy_device *phydev)
{
	int ret;

	/* Enable Analog and DSP register Bank access by
	 * toggling TSTCNTL_TEST_MODE bit in the TSTCNTL register
	 */
	ret = phy_write(phydev, TSTCNTL, 0);
	if (ret)
		return ret;
	ret = phy_write(phydev, TSTCNTL, TSTCNTL_TEST_MODE);
	if (ret)
		return ret;
	ret = phy_write(phydev, TSTCNTL, 0);
	if (ret)
		return ret;
	return phy_write(phydev, TSTCNTL, TSTCNTL_TEST_MODE);
}

static void meson_gxl_close_banks(struct phy_device *phydev)
{
	phy_write(phydev, TSTCNTL, 0);
}

static int meson_gxl_read_reg(struct phy_device *phydev,
			      unsigned int bank, unsigned int reg)
{
	int ret;

	ret = meson_gxl_open_banks(phydev);
	if (ret)
		goto out;

	ret = phy_write(phydev, TSTCNTL, TSTCNTL_READ |
			FIELD_PREP(TSTCNTL_REG_BANK_SEL, bank) |
			TSTCNTL_TEST_MODE |
			FIELD_PREP(TSTCNTL_READ_ADDRESS, reg));
	if (ret)
		goto out;

	ret = phy_read(phydev, TSTREAD1);
out:
	/* Close the bank access on our way out */
	meson_gxl_close_banks(phydev);
	return ret;
}

static int meson_gxl_write_reg(struct phy_device *phydev,
			       unsigned int bank, unsigned int reg,
			       uint16_t value)
{
	int ret;

	ret = meson_gxl_open_banks(phydev);
	if (ret)
		goto out;

	ret = phy_write(phydev, TSTWRITE, value);
	if (ret)
		goto out;

	ret = phy_write(phydev, TSTCNTL, TSTCNTL_WRITE |
			FIELD_PREP(TSTCNTL_REG_BANK_SEL, bank) |
			TSTCNTL_TEST_MODE |
			FIELD_PREP(TSTCNTL_WRITE_ADDRESS, reg));

out:
	/* Close the bank access on our way out */
	meson_gxl_close_banks(phydev);
	return ret;
}

static int meson_gxl_config_init(struct phy_device *phydev)
{
	int ret;

	/* Enable fractional PLL */
	ret = meson_gxl_write_reg(phydev, BANK_BIST, FR_PLL_CONTROL, 0x5);
	if (ret)
		return ret;

	/* Program fraction FR_PLL_DIV1 */
	ret = meson_gxl_write_reg(phydev, BANK_BIST, FR_PLL_DIV1, 0x029a);
	if (ret)
		return ret;

	/* Program fraction FR_PLL_DIV1 */
	ret = meson_gxl_write_reg(phydev, BANK_BIST, FR_PLL_DIV0, 0xaaaa);
	if (ret)
		return ret;

	return 0;
}

#ifdef CONFIG_AMLOGIC_ETH_PRIVE
/*tx_amp*/
static int custom_internal_config(struct phy_device *phydev)
{
	unsigned int efuse_valid = 0;
	unsigned int efuse_amp = 0;
	unsigned int setup_amp = 0;

	pr_info("custom_internal_config\n");
	/*we will setup env tx_amp first to debug,
	 *if env tx_amp ==0 we will use the efuse
	 */
	/*use reg from bl2 sc2 t5 use this method*/
	if (enet_type == 3 || enet_type == 4 || enet_type == 5)
		efuse_amp = tx_amp_bl2;
/*mailbox scpi_get_ethernet_calc() function*/
/*only support sc2 t5d tx_amp method by now*/
#if MAILBOX_SUPPORT
	else
		efuse_amp = scpi_get_ethernet_calc();

	pr_info("efuse tx_amp = %x\n", efuse_amp);
	if (is_meson_g12b_cpu() && is_meson_rev_a()) {
		efuse_valid = (efuse_amp >> 3);
		efuse_amp = efuse_amp & 0x7;
	} else {
		efuse_valid = ((efuse_amp >> 4) & 0x3);
		efuse_amp = efuse_amp & 0xf;
	}
#else
	if (enet_type == ETH_PHY_T5)
		efuse_valid = ((efuse_amp >> 5) & 0x1);
	else
		efuse_valid = ((efuse_amp >> 4) & 0x3);
	/*0715-2021 define bit3 as enhance bit as HW requirement
	 *reduce steps down to 8 from 16
	 */
	efuse_amp = efuse_amp & 0x7;
#endif
	if (efuse_valid) {
		/* efuse is valid but env not*/
		setup_amp = efuse_amp;
		/*Enable Analog and DSP register Bank access by*/
		phy_write(phydev, 0x14, 0x0000);
		phy_write(phydev, 0x14, 0x0400);
		phy_write(phydev, 0x14, 0x0000);
		phy_write(phydev, 0x14, 0x0400);
		phy_write(phydev, 0x17, setup_amp);
		phy_write(phydev, 0x14, 0x4418);
		pr_info("set phy setup_amp = %d\n", setup_amp);
	} else {
		/*env not set, efuse not valid return*/
		pr_info("env not set, efuse also invalid\n");
	}
	/*rx currents for t5/t5d only by now*/
	if (enet_type == ETH_PHY_T5) {
		phy_write(phydev, 0x14, 0x0000);
		phy_write(phydev, 0x14, 0x0400);
		phy_write(phydev, 0x14, 0x0000);
		phy_write(phydev, 0x14, 0x0400);
		phy_write(phydev, 0x17, 0x1000);
		phy_write(phydev, 0x14, 0x4413);
	}
	return 0;
}
#endif
/* This function is provided to cope with the possible failures of this phy
 * during aneg process. When aneg fails, the PHY reports that aneg is done
 * but the value found in MII_LPA is wrong:
 *  - Early failures: MII_LPA is just 0x0001. if MII_EXPANSION reports that
 *    the link partner (LP) supports aneg but the LP never acked our base
 *    code word, it is likely that we never sent it to begin with.
 *  - Late failures: MII_LPA is filled with a value which seems to make sense
 *    but it actually is not what the LP is advertising. It seems that we
 *    can detect this using a magic bit in the WOL bank (reg 12 - bit 12).
 *    If this particular bit is not set when aneg is reported being done,
 *    it means MII_LPA is likely to be wrong.
 *
 * In both case, forcing a restart of the aneg process solve the problem.
 * When this failure happens, the first retry is usually successful but,
 * in some cases, it may take up to 6 retries to get a decent result
 */
static int meson_gxl_read_status(struct phy_device *phydev)
{
	int ret, wol, lpa, exp;

	if (phydev->autoneg == AUTONEG_ENABLE) {
		ret = genphy_aneg_done(phydev);
		if (ret < 0)
			return ret;
		else if (!ret)
			goto read_status_continue;

		/* Aneg is done, let's check everything is fine */
		wol = meson_gxl_read_reg(phydev, BANK_WOL, LPI_STATUS);
		if (wol < 0)
			return wol;

		lpa = phy_read(phydev, MII_LPA);
		if (lpa < 0)
			return lpa;

		exp = phy_read(phydev, MII_EXPANSION);
		if (exp < 0)
			return exp;

		if (!(wol & LPI_STATUS_RSV12) ||
		    ((exp & EXPANSION_NWAY) && !(lpa & LPA_LPACK))) {
			/* Looks like aneg failed after all */
			phydev_dbg(phydev, "LPA corruption - aneg restart\n");
			return genphy_restart_aneg(phydev);
		}
	}

read_status_continue:
	return genphy_read_status(phydev);
}

static int meson_gxl_ack_interrupt(struct phy_device *phydev)
{
	int ret = phy_read(phydev, INTSRC_FLAG);

	return ret < 0 ? ret : 0;
}

static int meson_gxl_config_intr(struct phy_device *phydev)
{
	u16 val;
	int ret;

	if (phydev->interrupts == PHY_INTERRUPT_ENABLED) {
		val = INTSRC_ANEG_PR
			| INTSRC_PARALLEL_FAULT
			| INTSRC_ANEG_LP_ACK
			| INTSRC_LINK_DOWN
			| INTSRC_REMOTE_FAULT
			| INTSRC_ANEG_COMPLETE;
	} else {
		val = 0;
	}

	/* Ack any pending IRQ */
	ret = meson_gxl_ack_interrupt(phydev);
	if (ret)
		return ret;

	return phy_write(phydev, INTSRC_MASK, val);
}

static struct phy_driver meson_gxl_phy[] = {
	{
		PHY_ID_MATCH_EXACT(0x01814400),
		.name		= "Meson GXL Internal PHY",
		/* PHY_BASIC_FEATURES */
		.flags		= PHY_IS_INTERNAL,
		.soft_reset     = genphy_soft_reset,
		.config_init	= meson_gxl_config_init,
		.read_status	= meson_gxl_read_status,
		.ack_interrupt	= meson_gxl_ack_interrupt,
		.config_intr	= meson_gxl_config_intr,
		.suspend        = genphy_suspend,
		.resume         = genphy_resume,
	}, {
		PHY_ID_MATCH_EXACT(0x01803301),
		.name		= "Meson G12A Internal PHY",
		/* PHY_BASIC_FEATURES */
		.flags		= PHY_IS_INTERNAL,
		.soft_reset     = genphy_soft_reset,
#ifdef CONFIG_AMLOGIC_ETH_PRIVE
		.config_init	= custom_internal_config,
#endif
		.ack_interrupt	= meson_gxl_ack_interrupt,
		.config_intr	= meson_gxl_config_intr,
		.suspend        = genphy_suspend,
		.resume         = genphy_resume,
	},
};

static struct mdio_device_id __maybe_unused meson_gxl_tbl[] = {
	{ PHY_ID_MATCH_VENDOR(0x01814400) },
	{ PHY_ID_MATCH_VENDOR(0x01803301) },
	{ }
};

module_phy_driver(meson_gxl_phy);

MODULE_DEVICE_TABLE(mdio, meson_gxl_tbl);

MODULE_DESCRIPTION("Amlogic Meson GXL Internal PHY driver");
MODULE_AUTHOR("Baoqi wang");
MODULE_AUTHOR("Neil Armstrong <narmstrong@baylibre.com>");
MODULE_AUTHOR("Jerome Brunet <jbrunet@baylibre.com>");
MODULE_LICENSE("GPL");
