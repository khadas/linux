/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _AMLFRONTEND_H
#define _AMLFRONTEND_H
/**/
#include "depend.h" /**/
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/aml_dtvdemod.h>

#define KERNEL_4_9_EN		1
#define DTVDEMOD_VER	"2021/01/04: workaround for T/T2 weak signal"
#define DEMOD_DEVICE_NAME  "dtvdemod"

#define THRD_TUNER_STRENTH_ATSC (-87)
#define THRD_TUNER_STRENTH_J83 (-76)
/* tested on BTC, sensertivity of demod is -100dBm */
#define THRD_TUNER_STRENTH_DVBT (-102)
#define THRD_TUNER_STRENTH_DVBS (-76)

#define TIMEOUT_ATSC		2000
#define TIMEOUT_DVBT		2000
#define TIMEOUT_DVBS		2000

enum Gxtv_Demod_Tuner_If {
	SI2176_5M_IF = 5,
	SI2176_6M_IF = 6
};

#define ADC_CLK_24M	24000
#define ADC_CLK_25M	25000
#define ADC_CLK_54M	54000
#define ADC_CLK_135M	135000

#define DEMOD_CLK_60M	60000
#define DEMOD_CLK_72M	72000
#define DEMOD_CLK_125M	125000
#define DEMOD_CLK_167M	167000
#define DEMOD_CLK_200M	200000
#define DEMOD_CLK_216M	216000
#define DEMOD_CLK_225M	225000
#define DEMOD_CLK_250M	250000
#define DEMOD_CLK_270M	270000

#define FIRMWARE_NAME	"dtvdemod_t2.bin"
#define FIRMWARE_DIR	"dtvdemod"
#define PATH_MAX_LEN	50
#define FW_BUFF_SIZE	(100 * 1024)

enum M6_Demod_Pll_Mode {
	CRY_MODE = 0,
	ADC_MODE = 1
};

/*
 * e: enum
 * s: system
 */
enum es_map_addr {
	ES_MAP_ADDR_DEMOD,
	ES_MAP_ADDR_IOHIU,
	ES_MAP_ADDR_AOBUS,
	ES_MAP_ADDR_RESET,
	ES_MAP_ADDR_NUM
};
struct ss_reg_phy {
	unsigned int phy_addr;
	unsigned int size;
	/*void __iomem *p;*/
	/*int flag;*/
};
struct ss_reg_vt {
	void __iomem *v;
	int flag;
};

struct ddemod_reg_off {
	unsigned int off_demod_top;
	unsigned int off_dvbc;
	unsigned int off_dtmb;
	unsigned int off_dvbt_isdbt;
	unsigned int off_dvbt_t2;
	unsigned int off_dvbs;
	unsigned int off_atsc;
	unsigned int off_front;
	unsigned int off_isdbt;
};

enum dtv_demod_hw_ver_e {
	DTVDEMOD_HW_ORG = 0,
	DTVDEMOD_HW_TXLX,
	DTVDEMOD_HW_SM1,
	DTVDEMOD_HW_TL1,
	DTVDEMOD_HW_TM2,
	DTVDEMOD_HW_TM2_B,
	DTVDEMOD_HW_T5,
	DTVDEMOD_HW_T5D,
};

struct meson_ddemod_data {
	struct ddemod_reg_off regoff;
	enum dtv_demod_hw_ver_e hw_ver;
};
enum DTVDEMOD_ST {
	DTVDEMOD_ST_NOT_INI,	/*driver is not init or init failed*/
	DTVDEMOD_ST_IDLE,	/*leave mode*/
	DTVDEMOD_ST_WORK,	/*enter_mode*/
};

/*polling*/
struct poll_machie_s {
	unsigned int flg_stop;	/**/
	unsigned int flg_restart;

	unsigned int state;	/*idel, work,*/


	/**/
	unsigned int delayms;
	unsigned int flg_updelay;

	unsigned int crrcnt;
	unsigned int maxcnt;

	enum fe_status last_s;
	unsigned int bch;


};

struct aml_demod_para {
	u32_t dvbc_symbol;
	u32_t dvbc_qam;
	u32_t dtmb_qam;
	u32_t dtmb_coderate;
};

#define CAP_NAME_LEN	100
struct dtvdemod_capture_s {
	char cap_dev_name[CAP_NAME_LEN];
	unsigned int cap_size;
};

struct amldtvdemod_device_s {

	struct class *clsp;
	struct device *dev;
	enum DTVDEMOD_ST state;
	struct mutex lock;	/*aml_lock*/
	struct ss_reg_phy reg_p[ES_MAP_ADDR_NUM];
	struct ss_reg_vt reg_v[ES_MAP_ADDR_NUM];
	unsigned int dmc_phy_addr;
	unsigned int dmc_size;
	unsigned int dmc_saved;
	void __iomem *dmc_v_addr;

	struct ddemod_reg_off ireg;
	struct meson_ddemod_data *data;
	bool flg_cma_allc;
	bool act_dtmb;

	struct poll_machie_s poll_machie;

	unsigned int en_detect;
#ifdef KERNEL_4_9_EN
	/* clktree */
	unsigned int clk_gate_state;
	struct clk *vdac_clk_gate;
#endif
	/*agc pin mux*/
	struct pinctrl *pin;	/*agc pintrcl*/
	const char *pin_name;	/*agc pin name*/
	struct pinctrl_state *pin_agc_st;/*agc pin state*/

#if 1 /*move to aml_dtv_demod*/
	/*for mem reserved*/
	int			mem_start;
	int			mem_end;
	int			mem_size;
	int			cma_flag;
#ifdef CONFIG_CMA
	struct platform_device	*this_pdev;
	struct page			*venc_pages;
	unsigned int			cma_mem_size;/* BYTE */
	unsigned int			cma_mem_alloc;
#endif

	/*for dtv spectrum*/
	int			spectrum;
	/*for atsc version*/
	int			atsc_version;
	/*for dtv priv*/
#endif
	enum fe_delivery_system last_delsys;

	struct dvb_frontend frontend;

	const struct amlfe_exp_config *afe_cfg;
	struct dentry *demod_root;

	/* only for tm2,first time of pwr on,reset after signal locked begin */
	unsigned int atsc_rst_needed;
	unsigned int atsc_rst_done;
	unsigned int atsc_rst_wait_cnt;
	/* only for tm2,first time of pwr on,reset after signal locked end */

	unsigned short symbol_rate;
	unsigned int symb_rate_en;
	unsigned int freq;
	enum fe_modulation atsc_mode;
	struct aml_demod_para para_demod;
	struct dtvdemod_capture_s capture_para;
	unsigned int stop_reg_wr;
	char firmware_path[PATH_MAX_LEN];
	char *fw_buf;
	struct delayed_work fw_dwork;
	unsigned int timeout_atsc_ms;
	unsigned int timeout_dvbt_ms;
	unsigned int timeout_dvbs_ms;

	/* diseqc */
	const char *diseqc_name;
	unsigned int demod_irq_num;
	unsigned int demod_irq_en;
	struct pinctrl_state *diseqc_pin_st;/*diseqc pin state*/
	struct mutex mutex_tx_msg;/*pretect tx cec msg*/
	unsigned int print_on;
	unsigned int dvbt_bw;
	int tuner_strength_limit;
};

extern struct amldtvdemod_device_s *dtvdd_devp;

/*int M6_Demod_Dtmb_Init(struct aml_fe_dev *dev);*/
int convert_snr(int in_snr);


extern unsigned  int ats_thread_flg;

static inline void __iomem *gbase_dvbt_isdbt(void)
{
	return dtvdd_devp->reg_v[ES_MAP_ADDR_DEMOD].v + dtvdd_devp->data->regoff.off_dvbt_isdbt;
}

static inline void __iomem *gbase_dvbt_t2(void)
{
	return dtvdd_devp->reg_v[ES_MAP_ADDR_DEMOD].v + dtvdd_devp->data->regoff.off_dvbt_t2;
}

static inline void __iomem *gbase_dvbs(void)
{
	return dtvdd_devp->reg_v[ES_MAP_ADDR_DEMOD].v + dtvdd_devp->data->regoff.off_dvbs;
}

static inline void __iomem *gbase_dvbc(void)
{
	return dtvdd_devp->reg_v[ES_MAP_ADDR_DEMOD].v + dtvdd_devp->data->regoff.off_dvbc;
}
static inline void __iomem *gbase_dtmb(void)
{
	return dtvdd_devp->reg_v[ES_MAP_ADDR_DEMOD].v + dtvdd_devp->data->regoff.off_dtmb;
}

static inline void __iomem *gbase_atsc(void)
{
	return dtvdd_devp->reg_v[ES_MAP_ADDR_DEMOD].v + dtvdd_devp->data->regoff.off_atsc;
}
static inline void __iomem *gbase_demod(void)
{
	return dtvdd_devp->reg_v[ES_MAP_ADDR_DEMOD].v + dtvdd_devp->data->regoff.off_demod_top;
}

static inline void __iomem *gbase_isdbt(void)
{
	return dtvdd_devp->reg_v[ES_MAP_ADDR_DEMOD].v + dtvdd_devp->data->regoff.off_isdbt;
}

static inline void __iomem *gbase_front(void)
{
	return dtvdd_devp->reg_v[ES_MAP_ADDR_DEMOD].v + dtvdd_devp->data->regoff.off_front;
}

static inline void __iomem *gbase_aobus(void)
{
	return dtvdd_devp->reg_v[ES_MAP_ADDR_AOBUS].v;
}

static inline void __iomem *gbase_iohiu(void)
{
	return dtvdd_devp->reg_v[ES_MAP_ADDR_IOHIU].v;
}

static inline void __iomem *gbase_reset(void)
{
	return dtvdd_devp->reg_v[ES_MAP_ADDR_RESET].v;
}

static inline void __iomem *gbase_dmc(void)
{
	return dtvdd_devp->dmc_v_addr;
}

static inline unsigned int gphybase_demod(void)
{
	return dtvdd_devp->reg_p[ES_MAP_ADDR_DEMOD].phy_addr;
}
static inline unsigned int gphybase_demodcfg(void)
{
	return dtvdd_devp->reg_p[ES_MAP_ADDR_DEMOD].phy_addr +
		dtvdd_devp->data->regoff.off_demod_top;
}

static inline unsigned int gphybase_hiu(void)
{
	return dtvdd_devp->reg_p[ES_MAP_ADDR_IOHIU].phy_addr;
}

/*poll*/
extern void dtmb_poll_start(void);
extern void dtmb_poll_stop(void);
extern unsigned int dtmb_is_update_delay(void);
extern unsigned int dtmb_get_delay_clear(void);
extern unsigned int dtmb_is_have_check(void);
extern void dtmb_poll_v3(void);
extern unsigned int demod_dvbc_get_fast_search(void);
extern void demod_dvbc_set_fast_search(unsigned int en);
unsigned int dtvdemod_get_atsc_lock_sts(void);
struct amldtvdemod_device_s *dtvdemod_get_dev(void);
const char *dtvdemod_get_cur_delsys(enum fe_delivery_system delsys);
void aml_dtv_demode_isr_en(struct amldtvdemod_device_s *devp, u32 en);

#ifdef MODULE
struct dvb_frontend *aml_dtvdm_attach(const struct demod_config *config);
#endif

#endif
