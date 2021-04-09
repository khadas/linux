// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include "system_log.h"

#include <linux/io.h>
#include <linux/of_address.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include "gdc_config.h"

static void *p_arm_hw_base;
static void *p_aml_hw_base;
static void *p_ext_8g_msb_base;

#define GDC_REG_NAME(dev_type) (((dev_type) == ARM_GDC) ? "arm" : "aml")
#define GDC_REG_BASE(dev_type) (((dev_type) == ARM_GDC) ? &p_arm_hw_base : \
							  &p_aml_hw_base)
#define GDC_REG_TYPE(addr) (((addr) & ISP_DWAP_REG_MARK) ? AML_GDC : ARM_GDC)
#define GDC_REG_ADDR(addr) ((addr) & ~ISP_DWAP_REG_MARK)

int32_t init_gdc_io(struct device_node *dn, u32 dev_type)
{
	void *tmp_hw_base;
	void **pp_hw_base = (dev_type == ARM_GDC) ? &p_arm_hw_base :
						    &p_aml_hw_base;

	*pp_hw_base = of_iomap(dn, 0);

	gdc_log(LOG_INFO, "reg base = %p\n", *pp_hw_base);
	if (!(*pp_hw_base)) {
		gdc_log(LOG_ERR, "failed to map register, %p\n", *pp_hw_base);
		return -1;
	}

	tmp_hw_base = of_iomap(dn, 1);

	if (tmp_hw_base) {
		gdc_log(LOG_INFO, "ext_8g_msb base = %p\n", tmp_hw_base);
		p_ext_8g_msb_base = tmp_hw_base;
	}

	return 0;
}

void close_gdc_io(struct device_node *dn)
{
	gdc_log(LOG_DEBUG, "IO functionality has been closed");
}

u32 system_gdc_read_32(u32 addr)
{
	u32 result = 0;
	u32 dev_type = GDC_REG_TYPE(addr);
	void **pp_hw_base = GDC_REG_BASE(dev_type);

	if (!(*pp_hw_base)) {
		gdc_log(LOG_ERR, "Failed to base address %d\n", __LINE__);
		return 0;
	}

	result = ioread32(*pp_hw_base + GDC_REG_ADDR(addr));
	gdc_log(LOG_DEBUG, "%s r [0x%04x]= %08x\n", GDC_REG_NAME(dev_type),
		(u32)GDC_REG_ADDR(addr), result);
	return result;
}

void system_gdc_write_32(u32 addr, u32 data)
{
	u32 dev_type = GDC_REG_TYPE(addr);
	void **pp_hw_base = GDC_REG_BASE(dev_type);

	if (!(*pp_hw_base)) {
		gdc_log(LOG_ERR, "Failed to base address %d\n", __LINE__);
		return;
	}

	iowrite32(data, *pp_hw_base + GDC_REG_ADDR(addr));
	gdc_log(LOG_DEBUG, "%s w [0x%04x]= %08x\n", GDC_REG_NAME(dev_type),
		(u32)GDC_REG_ADDR(addr), data);
}

u32 system_ext_8g_msb_read_32(void)
{
	if (!p_ext_8g_msb_base) {
		gdc_log(LOG_ERR, "ext_8g_msb_base address is NULL\n");
		return 0;
	}

	return ioread32(p_ext_8g_msb_base);
}

void system_ext_8g_msb_write_32(u32 data)
{
	if (!p_ext_8g_msb_base) {
		gdc_log(LOG_ERR, "ext_8g_msb_base address is NULL\n");
		return;
	}

	iowrite32(data, p_ext_8g_msb_base);
	gdc_log(LOG_DEBUG, "arm w ext_8g_msb= %08x\n", data);
}

