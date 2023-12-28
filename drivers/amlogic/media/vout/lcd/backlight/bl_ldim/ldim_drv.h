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
/*20230710: add power on/off state protect */
/*20230915: add cus_fw set pq */
/*20231108: remove ldim_off_vs_brightness */
/*20231208: support spi dma trig interface */

#define LDIM_DRV_VER    "20231208"

enum spi_sync_type_e {
	SPI_SYNC	= 0x00,
	SPI_ASYNC	= 0x01,
	SPI_DMA_TRIG	= 0x02,
	SPI_SYNC_MAX,
};

enum spi_controller_type_e {
	SPI_T3		= 0x00,
	SPI_T5M		= 0x01,
	SPI_T3X		= 0x02,
	SPI_CONT_MAX,
};

enum spiout_type_e {
	SPIOUT_PWM_VS		= 0x00,
	SPIOUT_VSYNC		= 0x01,
	SPIOUT_MAX,
};

#define LDIM_DBG_PR_VSYNC_ISR		0x80
#define LDIM_DBG_PR_PWM_VS_ISR		0x40
#define LDIM_DBG_PR_DEV_DBG_INFO	0x20
#define LDIM_DBG_PR_PWM			0x10
#define LDIM_DBG_PR_SPI			0x08
#define LDIM_DBG_PR_SWAP_64BIT		0x04

extern unsigned char ldim_debug_print;

extern int ld_remap_lut[16][32];
extern unsigned int ldc_gain_lut_array[16][64];
extern unsigned int ldc_min_gain_lut[64];
extern unsigned int ldc_dither_lut[32][16];
extern struct fw_pqdata_s ldim_pq;
extern struct fw_pq_s fw_pq;

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
