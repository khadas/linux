/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __DVBS_FUNC_H__
#define __DVBS_FUNC_H__
#include "dvb_frontend.h"

#define ALIGN_24	16777216
#define SR_LOW_THRD	11000000

#define BLIND_SEARCH_AGC2BANDWIDTH_40 (40)
#define BLIND_SEARCH_AGC2BANDWIDTH_80 (80)
#define BLIND_SEARCH_POW_TH           (25000)
#define BLIND_SEARCH_BW_MIN           (6)

#define CNR_HIGH	0xcae
#define CNR_LOW		0xcad
#define AUTOSR_REG 0x922
#define AUTOSR_OFF 0xcc
#define AUTOSR_ON  0xdc

enum diseq_irq_flag {
	DISEQ_IRQGAPBURST = 0x01,
	DISEQ_IRQFIFO64B = 0x02,
	DISEQ_IRQTXEND = 0x4,
	DISEQ_IRQTIMEOUT = 0x8,
	DISEQ_IRQTRFINISH = 0x10,
	DISEQ_IRQRXFIFO8B = 0x20,
	DISEQ_IRQRXEND = 0x40
};

struct fe_lla_lookpoint_t {
	s32 realval; //real value
	s32 regval; //binary value
};

struct fe_lla_lookup_t {
	s32 size;	//Size of the lookup table
	struct fe_lla_lookpoint_t table[500];	//Lookup table
};

struct fft_threadcontrols {
	s32 flow;
	s32 fup;
	u32 range;
	u32 mode;
	u32 acc;
	u32 in_bw_center_frc;
};

struct fft_search_result {
	s32 found_ok;
	u32 result_frc;
	s32 result_bw;
};

struct fft_in_bw_result {
	s32 tp_num;
	u32 freq[45];
	u32 bw[45];
};

struct fft_total_result {
	s32 tp_num;
	u32 freq[1000];
	u32 bw[1000];
};

#define DVBS_REG_MRELEASE	0x100
#define DVBS_REG_DEVICE_ID	0x101

/* t5d dvbs reg */
#define DVBS_REG_SYS_IRQMSK1	0x150
#define DVBS_REG_SYS_IRQMSK0	0x151
#define DVBS_REG_SYS_IRQSTATUS1	0x152
#define DVBS_REG_SYS_IRQSTATUS0	0x153
#define DVBS_REG_SYS_IRQFORCE1	0x154
#define DVBS_REG_SYS_IRQFORCE0	0x155

#define DVBS_REG_GPIO0CFG	0x180
#define DVBS_REG_GPIO1CFG	0x181
#define DVBS_REG_GPIO2CFG	0x182
#define DVBS_REG_GPIO3CFG	0x183
#define DVBS_REG_GPIO4CFG	0x184
#define DVBS_REG_GPIO5CFG	0x185
#define DVBS_REG_GPIO6CFG	0x186
#define DVBS_REG_GPIO7CFG	0x187
#define DVBS_REG_GPIOOBS0	0x1A3
#define DVBS_REG_TSTOUT		0x1F0
#define DVBS_REG_TSTIN		0x1F1
#define DVBS_REG_TSTSYS		0x1F2
#define DVBS_REG_TBUSXXX2	0x1F4
#define DVBS_REG_TBUSXXX1	0x1F5
#define DVBS_REG_TBUSXXX0	0x1F6
#define DVBS_REG_BITSPEED1	0x1F7
#define DVBS_REG_BITSPEED0	0x1F8
#define DVBS_REG_TBUSBIT1	0x1F9
#define DVBS_REG_TBUSBIT2	0x1FA
#define DVBS_REG_I2CMAPOCFG	0x1FB
#define DVBS_REG_I2CMAPO11	0x1FC
#define DVBS_REG_I2CMAPO10	0x1FD
#define DVBS_REG_I2CMAPO21	0x1FE
#define DVBS_REG_I2CMAPO20	0x1FF

#define DVBS_REG_DISIRQCFG	0x300
#define DVBS_REG_DISIRQSTAT	0x301
#define DVBS_REG_DISTXCFG	0x302
#define DVBS_REG_DISTXSTATUS	0x303
#define DVBS_REG_DISTXBYTES	0x304
#define DVBS_REG_DISTXFIFO	0x305
#define DVBS_REG_DISTXF22	0x306
#define DVBS_REG_DISTXWAIT	0x307
#define DVBS_REG_DISTIMEOCFG	0x308
#define DVBS_REG_DISTIMEOUT	0x309
#define DVBS_REG_DISRXCFG	0x30A
#define DVBS_REG_DISRXSTAT1	0x30B
#define DVBS_REG_DISRXSTAT0	0x30C
#define DVBS_REG_DISRXBYTES	0x30D
#define DVBS_REG_DISRXPARITY1	0x30E
#define DVBS_REG_DISRXPARITY0	0x30F
#define DVBS_REG_DISRXFIFO	0x310
#define DVBS_REG_DISRXDC1	0x311
#define DVBS_REG_DISRXDC0	0x312
#define DVBS_REG_DISRXKOFF	0x313
#define DVBS_REG_DISRXF221	0x314
#define DVBS_REG_DISRXF220	0x315
#define DVBS_REG_DISRXF100	0x316
#define DVBS_REG_DISRXK1	0x317
#define DVBS_REG_DISRXK2	0x318
#define DVBS_REG_DISRXPK	0x319
#define DVBS_REG_DISRXENV	0x31A
#define DVBS_REG_DISRXVMIN	0x31B
#define DVBS_REG_DISRXSHORT22K	0x31C
#define DVBS_REG_ACRPRESC	0x31E
#define DVBS_REG_ACRDIV		0x31F

#define DVBS_REG_DSQADCINCFG	0x121

#define DSTATUS		0x934

/* carrier offset */
#define	CFR12	0x9cf
#define	CFR11	0x9d0
#define	CFR10	0x9d1

#define DVBS_AGC_LEVEL_ADDR     0x91a

void dvbs2_diseqc_send_msg(unsigned int len, unsigned char *msg);
unsigned int dvbs2_diseqc_read_msg(unsigned int len, unsigned char *msg);
unsigned int dvbs2_diseqc_irq_check(void);
unsigned int dvbs2_diseqc_rx_check(void);
void dvbs2_diseqc_reset(void);
void dvbs2_diseqc_send_irq_en(bool onoff);
void dvbs2_diseqc_recv_irq_en(bool onoff);
void dvbs2_diseqc_init(void);
void dvbs2_diseqc_recv_en(bool onoff);
void dvbs2_diseqc_continuous_tone(bool onoff);
void dvbs_check_status(struct seq_file *seq);
unsigned int dvbs_get_freq_offset(unsigned int *polarity);
unsigned int dvbs_get_symbol_rate(void);
void dvbs_fft_reg_init(unsigned int *reg_val);
void dvbs_fft_reg_term(unsigned int reg_val[60]);
void dvbs_blind_fft_work(struct fft_threadcontrols *spectr_ana_data,
	int frq, struct fft_search_result *search_result);
void dvbs_blind_fft_result_handle(struct fft_total_result *total_result);
unsigned int dvbs_blind_check_AGC2_bandwidth_new(int *next_step_khz);
unsigned int dvbs_blind_check_AGC2_bandwidth_old(int *next_step_khz);

#endif
