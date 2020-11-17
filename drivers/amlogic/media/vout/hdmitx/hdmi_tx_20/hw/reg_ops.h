/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __REG_OPS_H__
#define __REG_OPS_H__

#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/major.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <linux/io.h>
//#include <linux/amlogic/iomap.h>

/*
 * RePacket HDMI related registers rd/wr
 */
struct reg_map {
	unsigned int phy_addr;
	unsigned int size;
	void __iomem *p;
};

enum map_addr_idx_e {
	CBUS_REG_IDX = 0,
	VCBUS_REG_IDX,
	HHI_REG_IDX,
	HDMITX_REG_IDX,
	HDMITX_SEC_REG_IDX,
	ELP_ESM_REG_IDX,
	/* new added in SC2 */
	ANACTRL_REG_IDX,
	PWRCTRL_REG_IDX,
	RESETCTRL_REG_IDX,
	SYSCTRL_REG_IDX,
	CLKCTRL_REG_IDX,
	REG_IDX_END
};

#define BASE_REG_OFFSET		24

#define CBUS_REG_ADDR(reg) \
	((CBUS_REG_IDX << BASE_REG_OFFSET) + ((reg) << 2))
#define VCBUS_REG_ADDR(reg) \
	((VCBUS_REG_IDX << BASE_REG_OFFSET) + ((reg) << 2))
#define HHI_REG_ADDR(reg) \
	((HHI_REG_IDX << BASE_REG_OFFSET) + ((reg) << 2))
#define HDMITX_SEC_REG_ADDR(reg) \
	((HDMITX_SEC_REG_IDX << BASE_REG_OFFSET) + (reg))/*DWC*/
#define HDMITX_REG_ADDR(reg) \
	((HDMITX_REG_IDX << BASE_REG_OFFSET) + (reg))/*TOP*/
#define ELP_ESM_REG_ADDR(reg) \
	((ELP_ESM_REG_IDX << BASE_REG_OFFSET) + ((reg) << 2))
#define ANACTRL_REG_ADDR(reg) \
	((ANACTRL_REG_IDX << BASE_REG_OFFSET) + ((reg) << 2))
#define PWRCTRL_REG_ADDR(reg) \
	((PWRCTRL_REG_IDX << BASE_REG_OFFSET) + ((reg) << 2))
#define RESETCTRL_REG_ADDR(reg) \
	((RESETCTRL_REG_IDX << BASE_REG_OFFSET) + ((reg) << 2))
#define SYSCTRL_REG_ADDR(reg) \
	((SYSCTRL_REG_IDX << BASE_REG_OFFSET) + ((reg) << 2))
#define CLKCTRL_REG_ADDR(reg) \
	((CLKCTRL_REG_IDX << BASE_REG_OFFSET) + ((reg) << 2))

extern struct reg_map reg_maps[REG_IDX_END];

unsigned int TO_PHY_ADDR(unsigned int addr);
void __iomem *TO_PMAP_ADDR(unsigned int addr);

unsigned int hd_read_reg(unsigned int addr);
void hd_write_reg(unsigned int addr, unsigned int val);
void hd_set_reg_bits(unsigned int addr, unsigned int value,
		unsigned int offset, unsigned int len);
void init_reg_map(unsigned int type);

unsigned int hd_read_reg(unsigned int addr);
void hd_write_reg(unsigned int addr, unsigned int val);
void hd_set_reg_bits(unsigned int addr, unsigned int value,
		     unsigned int offset, unsigned int len);
int hdmitx_init_reg_map(struct platform_device *pdev);
#endif
