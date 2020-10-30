/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __VPU_H__
#define __VPU_H__

/* ************************************************ */
/* VPU module define */
/* ************************************************ */
enum vpu_mod_e {
	VPU_VIU_OSD1 = 0,     /* reg0[1:0], common */
	VPU_VIU_OSD2,         /* reg0[3:2], common */
	VPU_VIU_VD1,          /* reg0[5:4], common */
	VPU_VIU_VD2,          /* reg0[7:6], common */
	VPU_VIU_CHROMA,       /* reg0[9:8], common */
	VPU_VIU_OFIFO,        /* reg0[11:10], common */
	VPU_VIU_SCALE,        /* reg0[13:12], common */
	VPU_VIU_OSD_SCALE,    /* reg0[15:14], common */
	VPU_VIU_VDIN0,        /* reg0[17:16], common */
	VPU_VIU_VDIN1,        /* reg0[19:18], common */
	VPU_VIU_SRSCL,        /* reg0[21:20], GXBB, GXTVBB, TXLX */
	VPU_VIU_OSDSR,        /* reg0[23:22], GXBB */
	VPU_AFBC_DEC1,        /* reg0[23:22], GXTVBB, TXLX */
	VPU_VIU_DI_SCALE,     /* reg0[25:24], G12A */
	VPU_DI_PRE,           /* reg0[27:26], common */
	VPU_DI_POST,          /* reg0[29:28], common */
	VPU_SHARP,            /* reg0[31:30], common */

	VPU_VIU2,             /* reg1[3:0], reg2[23:22], SM1 */
	VPU_VIU2_OSD1,        /* reg1[1:0] */
	VPU_VIU2_OSD2,        /* reg1[3:2] */
	VPU_VIU2_VD1,         /* reg1[5:4] */
	VPU_VIU2_CHROMA,      /* reg1[7:6] */
	VPU_VIU2_OFIFO,       /* reg1[9:8] */
	VPU_VIU2_SCALE,       /* reg1[11:10] */
	VPU_VIU2_OSD_SCALE,   /* reg1[13:12] */
	VPU_VKSTONE,          /* reg1[5:4], TXLX */
	VPU_DOLBY_CORE3,      /* reg1[7:6], TXLX */
	VPU_DOLBY0,           /* reg1[9:8], TXLX */
	VPU_DOLBY1A,          /* reg1[11:10], TXLX */
	VPU_DOLBY1B,          /* reg1[13:12], TXLX */
	VPU_DOLBY2,
	VPU_VPU_ARB,          /* reg1[15:14], GXBB, GXTVBB, GXL, TXLX */
	VPU_AFBC_DEC,         /* reg1[17:16], GXBB, GXTVBB, TXL, TXLX */
	VPU_OSD_AFBCD,        /* reg1[19:18], TXLX */
	VPU_VD2_SCALE,        /* reg1[19:18], G12A */
	VPU_VENCP,            /* reg1[21:20], common */
	VPU_VENCL,            /* reg1[23:22], common */
	VPU_VENCI,            /* reg1[25:24], common */
	VPU_LC_STTS,          /* reg1[27:26], tl1 */
	VPU_LDIM_STTS,        /* reg1[29:28], GXTVBB, GXL, TXL, TXLX */
	VPU_TV_DEC_CVD2,      /* reg1[29:28] */
	VPU_XVYCC_LUT,        /* reg1[31:30], GXTVBB, GXL, TXL, TXLX */
	VPU_VD2_OSD2_SCALE,   /* reg1[31:30], G12A */

	VPU_VIU_WM,           /* reg2[1:0], GXL, TXL, TXLX */
	VPU_TCON,             /* reg2[3:2], TXHD, TL1 */
	VPU_VIU_OSD3,         /* reg2[5:4], G12A */
	VPU_VIU_OSD4,         /* reg2[7:6], G12A */
	VPU_MAIL_AFBCD,       /* reg2[9:8], G12A */
	VPU_VD1_SCALE,        /* reg2[11:10], G12A */
	VPU_OSD_BLD34,        /* reg2[13:12], G12A */
	VPU_PRIME_DOLBY_RAM,  /* reg2[15:14], G12A */
	VPU_VD2_OFIFO,        /* reg2[17:16], G12A */
	VPU_DS,               /* reg2[19:18], TL1 */
	VPU_LUT3D,            /* reg2[21:20], G12B */
	VPU_VIU2_OSD_ROT,     /* reg2[23:22], G12B */
	VPU_DOLBY_S0,         /* reg2[27:26], TM2 */
	VPU_DOLBY_S1,         /* reg2[29:28], TM2 */
	VPU_RDMA,             /* reg2[31:30], G12A */

	VPU_AXI_WR1,          /* reg4[1:0], TL1 */
	VPU_AXI_WR0,          /* reg4[3:2], TL1 */
	VPU_AFBCE,            /* reg4[5:4], TL1 */
	VPU_VDIN_WR_MIF2,     /* reg4[7:6], TM2 */
	VPU_DMA,              /* reg4[11:8], TM2 */
	VPU_HDMI,             /* reg4[13:12], TM2B */
	VPU_FGRAIN0,          /* reg4[15:14], TM2B */
	VPU_FGRAIN1,          /* reg4[17:16], TM2B */
	VPU_DI_AFBCD,         /* reg9[23:18], SC2 */
	VPU_DI_AFBCE,         /* reg9[27:24], SC2 */
	VPU_DI_DOLBY,         /* reg9[29:28], SC2 */
	VPU_DECONTOUR,        /* reg4[19:18], T5 */

	VPU_MOD_MAX,

	/* for clk_gate */
	VPU_VPU_TOP,
	VPU_VPU_CLKB,
	VPU_CLK_VIB,
	VPU_CLK_B_REG_LATCH,
	VPU_MISC,      /* hs,vs,interrupt */
	VPU_VENC_DAC,
	VPU_VLOCK,
	VPU_DI,
	VPU_VPP,

	VPU_MAX,
};

struct vpu_dev_s {
	unsigned int index;
	char owner_name[30];
	unsigned int dev_id;
	unsigned char mem_pd_state;
	unsigned char clk_gate_state;
};

#define VPU_MEM_POWER_ON                0
#define VPU_MEM_POWER_DOWN              1

struct vpu_dev_s *vpu_dev_get(unsigned int vmod, char *owner_name);
struct vpu_dev_s *vpu_dev_register(unsigned int vmod, char *owner_name);
int vpu_dev_unregister(struct vpu_dev_s *vpu_dev);

unsigned int vpu_clk_get(void);
unsigned int vpu_dev_clk_get(struct vpu_dev_s *vpu_dev);
int vpu_dev_clk_request(struct vpu_dev_s *vpu_dev, unsigned int vclk);
int vpu_dev_clk_release(struct vpu_dev_s *vpu_dev);

void vpu_dev_mem_power_on(struct vpu_dev_s *vpu_dev);
void vpu_dev_mem_power_down(struct vpu_dev_s *vpu_dev);
int vpu_dev_mem_pd_get(struct vpu_dev_s *vpu_dev);

void vpu_dev_clk_gate_on(struct vpu_dev_s *vpu_dev);
void vpu_dev_clk_gate_off(struct vpu_dev_s *vpu_dev);

/* vcbus reg access api */
unsigned int vpu_vcbus_read(unsigned int _reg);
void vpu_vcbus_write(unsigned int _reg, unsigned int _value);
void vpu_vcbus_setb(unsigned int _reg, unsigned int _value,
		    unsigned int _start, unsigned int _len);
unsigned int vpu_vcbus_getb(unsigned int _reg,
			    unsigned int _start, unsigned int _len);
void vpu_vcbus_set_mask(unsigned int _reg, unsigned int _mask);
void vpu_vcbus_clr_mask(unsigned int _reg, unsigned int _mask);

#endif
