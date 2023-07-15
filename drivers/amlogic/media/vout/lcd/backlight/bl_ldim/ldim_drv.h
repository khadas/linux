/* SPDX-License-Identifier: GPL-2.0+ */
/*
 *
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
 */

#ifndef _AML_LDIM_DRV_H_
#define _AML_LDIM_DRV_H_
#include <linux/dma-contiguous.h>
#include <linux/dma-mapping.h>
#include <linux/mm.h>
#include <linux/amlogic/media/vout/lcd/ldim_fw.h>
#include <uapi/linux/amlogic/ldim.h>

/*20200215: init version */
/*20210201: fix compiler mistake */
/*20210602: support t7/t3 new ldc */
/*20210730: basic function run ok */
/*20210806: add fw support */
/*20220815: correct ldim spi dma config */
/*20221115: new driver architecture */
/*20230208: update local dimming reserved memory init method */
/*20230619: optimize reserved memory alloc and vaddr usage */
/*20230620: add t3x support */
#define LDIM_DRV_VER    "20230620"

extern unsigned char ldim_debug_print;

extern int ld_remap_lut[16][32];
extern unsigned int ldc_gain_lut_array[16][64];
extern unsigned int ldc_min_gain_lut[64];
extern unsigned int ldc_dither_lut[32][16];

#define AML_LDIM_MODULE_NAME "aml_ldim"
#define AML_LDIM_DRIVER_NAME "aml_ldim"
#define AML_LDIM_DEVICE_NAME "aml_ldim"
#define AML_LDIM_CLASS_NAME  "aml_ldim"

/*ldim mem*/
void aml_ldim_rmem_info(void);

void ldc_mem_dump(unsigned char *vaddr, unsigned int size);
void ldc_mem_save(char *path, unsigned long mem_paddr, unsigned int mem_size);
void ldc_mem_write(char *path, unsigned long mem_paddr, unsigned int mem_size);
void ldc_mem_clear(unsigned long mem_paddr, unsigned int mem_size);
void ldc_mem_set(unsigned long mem_paddr, unsigned int mem_size);
void ldc_mem_write_profile(unsigned char *buf, unsigned long mem_paddr, unsigned int size);

/*==============debug=================*/
void ldim_remap_ctrl(unsigned char status);
int aml_ldim_debug_probe(struct class *ldim_class);
void aml_ldim_debug_remove(struct class *ldim_class);

#endif
