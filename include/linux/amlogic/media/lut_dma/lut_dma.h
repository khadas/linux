/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef LUT_DMA_H_
#define LUT_DMA_H_

#define LOCAL_DIMMING_CHAN     0
#define FILM_GRAIN0_CHAN       1
#define FILM_GRAIN1_CHAN       2
#define FILM_GRAIN2_CHAN       4
#define FILM_GRAIN_DI_CHAN     3

enum LUT_DMA_CHANNEL_e {
	LOCAL_DIMMING_CHAN_S5,
	FILM_GRAIN_DI_CHAN_S5,
	FILM_GRAIN1_VD1S0_CHAN_S5,
	FILM_GRAIN1_VD1S1_CHAN_S5,
	FILM_GRAIN1_VD1S2_CHAN_S5,
	FILM_GRAIN1_VD1S3_CHAN_S5,
	FILM_GRAIN2_CHAN_S5,
	HDR_DMA_CHAN_S5,
	VPP_LUT3D_DMA_CHAN_S5,
};

enum LUT_DMA_DIR_e {
	LUT_DMA_RD,
	LUT_DMA_WR,
};

enum LUT_DMA_MODE_e {
	LUT_DMA_AUTO,
	LUT_DMA_MANUAL,
};

enum IRQ_TYPE_e {
	VIU1_VSYNC,
	ENCL_GO_FIELD,
	ENCP_GO_FIELD,
	ENCI_GO_FIELD,
	VENC2_GO_FIELD = 1,
	VENC1_GO_FIELD,
	VENC0_GO_FIELD,
	VIU2_VSYNC,
	VIU1_LINE_N,
	VDIN0_VSYNC,
	DI_VSYNC,
};

struct lut_dma_set_t {
	u32 dma_dir;
	u32 table_size;
	u32 channel;
	u32 irq_source;
	u32 mode;
};

int lut_dma_register(struct lut_dma_set_t *lut_dma_set);
void lut_dma_unregister(u32 dma_dir, u32 channel);
int lut_dma_read(u32 channel, void *paddr);
int lut_dma_write(u32 channel, void *paddr, u32 size);
int lut_dma_write_phy_addr(u32 channel, ulong phy_addr, u32 size);
void lut_dma_update_irq_source(u32 channel, u32 irq_source);
#endif
