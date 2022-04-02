/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _AMLFRONTEND_H
#define _AMLFRONTEND_H
/**/
#include "depend.h" /**/
#include "dvbc_func.h"
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/aml_dtvdemod.h>

/****************************************************/
/*  V1.0.22  no audio output after random source switch */
/*  V1.0.23  fixed code and dts CMA config          */
/*  V1.0.24  dvbt2 add reset when unlocked for 3s   */
/*  V1.0.25  add demod version and t2 fw version node*/
/*  V1.0.26  weak signal sidplay after dvbs search*/
/*  V1.0.27  s4d remove dvbs blindscan fastsearch threadhold*/
/*  V1.0.28  fix the hang when read mplp info in DVBT system*/
/*  V1.0.29  add auto recognition t/t2 r842 config*/
/*  V1.0.30  DVBT2 no audio output after random source switch*/
/*  V1.1.31  t5w back trunk,version fix              */
/*  V1.1.32  dvbc frc offset optimization              */
/*  V1.1.33  dvbs qpsk 1/4 C/N worse             */
/*  V1.1.34  dvbs blind scan new work plan with fft   */
/*  V1.1.35  fixed s4/s4d 2 * dvbc agc control  */
/*  V1.1.36  Stuck during the blindscan process  */
/*  V1.1.37  dvbt2 change channel will return to home  */
/*  V1.1.38  Redistribution blind scan progress value reporting  */
/*  V1.1.39  fixed DVBS blind scan workqueue quit */
/*  V1.1.40  t5w change list check abus audio problem */
/*  V1.1.41  support auto qam in j83b */
/*  V1.1.42  optimization of DVBS blind scan  */
/*  V1.1.43  rebuild dvbc to fix autosr and recovery is slowly */
/*  V1.1.44  t3 revb change list check abus audio problem */
/*  V1.1.45  optimize dvb-c auto symbol rate(all) and auto qam(t5w) */
/*  V1.1.46  use the codec_mm cma for DTMB(8M)/DVB-T2(40M)/ISDB-T(8M) */
/****************************************************/
/****************************************************************/
/*               AMLDTVDEMOD_VER  Description:                  */
/*->The first digit indicates:                                  */
/*    In case of kernel version replacement                     */
/*    or driver schema update, increase the corresponding value */
/*->The second digit indicates:                                 */
/*    The changes relate to electrical performance and          */
/*    important function development                            */
/*->The third digit indicates:                                  */
/*    Minor changes and bug fixes                               */
/*             AMLDTVDEMOD_T2_FW_VER  Description:              */
/*->The first four digits indicate the fw version number        */
/*->The last four digits indicate the release time              */
/****************************************************************/
#define KERNEL_4_9_EN		1
#define AMLDTVDEMOD_VER "V1.1.46"
#define DTVDEMOD_VER	"2022/04/18: use the codec_mm cma for DTMB(8M)/DVB-T2(40M)/ISDB-T(8M)"
#define AMLDTVDEMOD_T2_FW_VER "V1417.0909"
#define DEMOD_DEVICE_NAME  "dtvdemod"

#define THRD_TUNER_STRENTH_ATSC (-87)
#define THRD_TUNER_STRENTH_J83 (-76)
/* tested on BTC, sensertivity of demod is -100dBm */
#define THRD_TUNER_STRENTH_DVBT (-101)
#define THRD_TUNER_STRENTH_DVBS (-79)
#define THRD_TUNER_STRENTH_DTMB (-92)

#define TIMEOUT_ATSC		2000
#define TIMEOUT_DVBT		3000
#define TIMEOUT_DVBS		2000
#define TIMEOUT_DVBC		3000
#define TIMEOUT_DVBT2		5000
#define TIMEOUT_DDR_LEAVE   50

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
	unsigned int off_dvbc_2;
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
	DTVDEMOD_HW_S4,
	DTVDEMOD_HW_T5D_B,
	DTVDEMOD_HW_T3,
	DTVDEMOD_HW_S4D,
	DTVDEMOD_HW_T5W,
};

struct ddemod_dig_clk_addr {
	unsigned int demod_clk_ctl;
	unsigned int demod_clk_ctl_1;
};

struct meson_ddemod_data {
	struct ddemod_dig_clk_addr dig_clk;
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

struct aml_demod_para_real {
	u32_t modulation;
	u32_t coderate;
	u32_t symbol;
	u32_t snr;
	u32_t plp_num;
};

#define CAP_NAME_LEN	100
struct dtvdemod_capture_s {
	char cap_dev_name[CAP_NAME_LEN];
	unsigned int cap_size;
};

struct timer_t {
	int enable;
	unsigned int start;
	unsigned int max;
};

enum ddemod_timer_s {
	D_TIMER_DETECT,
	D_TIMER_SET,
	D_TIMER_DBG1,
	D_TIMER_DBG2,
};

struct aml_dtvdemod {
	bool act_dtmb;

	struct poll_machie_s poll_machie;

	unsigned int en_detect;

	enum fe_delivery_system last_delsys;

	struct dvb_frontend frontend;

	/* only for tm2,first time of pwr on,reset after signal locked begin */
	unsigned int atsc_rst_needed;
	unsigned int atsc_rst_done;
	unsigned int atsc_rst_wait_cnt;
	/* only for tm2,first time of pwr on,reset after signal locked end */

	unsigned int symbol_rate_manu;
	unsigned int sr_val_hw;
	unsigned int sr_val_hw_stable;
	unsigned int sr_val_hw_count;
	unsigned int symb_rate_en;
	unsigned int auto_sr;
	unsigned int auto_sr_done;
	unsigned int freq;
	unsigned int freq_dvbc;
	enum fe_modulation atsc_mode;
	struct aml_demod_para para_demod;
	unsigned int timeout_atsc_ms;
	unsigned int timeout_dvbt_ms;
	unsigned int timeout_dvbs_ms;
	unsigned int time_start;
	unsigned int time_passed;
	enum fe_status last_status;
	unsigned int timeout_dvbc_ms;
	int autoflags;
	int auto_flags_trig;
	unsigned int p1_peak;

	unsigned int times;
	unsigned int no_sig_cnt;

	enum qam_md_e auto_qam_mode;
	enum qam_md_e last_qam_mode;
	unsigned int auto_times;
	unsigned int auto_qam_done;
	unsigned int auto_no_sig_cnt;
	unsigned int fast_search_finish;

	enum fe_status atsc_dbg_lst_status;

	unsigned int t_cnt;

	/* select dvbc module for s4 */
	unsigned int dvbc_sel;

	unsigned int bw;
	unsigned int plp_id;
	int last_lock;

	struct aml_demod_sta demod_status;

	struct task_struct *cci_task;
	int cciflag;

	struct timer_t gtimer[4];

	struct list_head list;
	void *priv;
	int id;
	bool inited;
	bool suspended;
	bool reseted;
	unsigned int ci_mode;
	unsigned int timeout_ddr_leave;

	struct aml_demod_para_real real_para;

	struct pinctrl *pin_agc;    /*agc pintrcl*/
	struct pinctrl *pin_diseqc; /*diseqc out pin*/

	u32 blind_result_frequency;
	u32 blind_result_symbol_rate;
};

struct amldtvdemod_device_s {

	struct class *clsp;
	struct device *dev;
	enum DTVDEMOD_ST state;
	struct mutex lock;	/*aml_lock*/
	struct ss_reg_phy reg_p[ES_MAP_ADDR_NUM];
	struct ss_reg_vt reg_v[ES_MAP_ADDR_NUM];
	unsigned int dmc_phy_addr;
	unsigned int dmc_saved;
	void __iomem *dmc_v_addr;
	unsigned int ddr_phy_addr;
	void __iomem *ddr_v_addr;

	struct ddemod_reg_off ireg;
	struct meson_ddemod_data *data;

#ifdef KERNEL_4_9_EN
	/* clktree */
	unsigned int clk_gate_state;
	struct clk *vdac_clk_gate;
	unsigned int clk_demod_32k_state;
	struct clk *demod_32k;
#endif

	bool agc_direction;

#if 1 /*move to aml_dtv_demod*/
	/*for mem reserved*/
	int			mem_start;
	int			mem_end;
	int			mem_size;
	int			cma_flag;
	bool		flg_cma_allc;

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

	const struct amlfe_exp_config *afe_cfg;
	struct dentry *demod_root;

	struct dtvdemod_capture_s capture_para;
	unsigned int stop_reg_wr;
	struct delayed_work fw_dwork;
	char firmware_path[PATH_MAX_LEN];
	char *fw_buf;

	struct work_struct blind_scan_work;

	/* diseqc */
	const char *diseqc_name;
	unsigned int demod_irq_num;
	unsigned int demod_irq_en;
	struct mutex mutex_tx_msg;/*pretect tx cec msg*/
	unsigned int print_on;
	int tuner_strength_limit;
	unsigned int debug_on;

	unsigned int demod_thread;
	unsigned int fw_wr_done;
	unsigned int blind_min_fre;
	unsigned int blind_max_fre;
	unsigned int blind_min_srate;
	unsigned int blind_max_srate;
	unsigned int blind_fre_range;
	unsigned int blind_fre_step;
	unsigned int blind_timeout;
	unsigned int blind_scan_stop;
	unsigned int blind_debug_max_frc;
	unsigned int blind_debug_min_frc;
	unsigned int blind_same_frec;

	bool vdac_enable;
	bool dvbc_inited;
	int peak[2048];
	unsigned int ber_base;
	unsigned int atsc_cr_step_size_dbg;
	unsigned char index;
	struct list_head demod_list;
};

/*int M6_Demod_Dtmb_Init(struct aml_fe_dev *dev);*/
int convert_snr(int in_snr);

extern unsigned int ats_thread_flg;

struct amldtvdemod_device_s *dtvdemod_get_dev(void);

static inline void __iomem *gbase_dvbt_isdbt(void)
{
	struct amldtvdemod_device_s *devp = dtvdemod_get_dev();

	return devp->reg_v[ES_MAP_ADDR_DEMOD].v + devp->data->regoff.off_dvbt_isdbt;
}

static inline void __iomem *gbase_dvbt_t2(void)
{
	struct amldtvdemod_device_s *devp = dtvdemod_get_dev();

	return devp->reg_v[ES_MAP_ADDR_DEMOD].v + devp->data->regoff.off_dvbt_t2;
}

static inline void __iomem *gbase_dvbs(void)
{
	struct amldtvdemod_device_s *devp = dtvdemod_get_dev();

	return devp->reg_v[ES_MAP_ADDR_DEMOD].v + devp->data->regoff.off_dvbs;
}

static inline void __iomem *gbase_dvbc(void)
{
	struct amldtvdemod_device_s *devp = dtvdemod_get_dev();

	return devp->reg_v[ES_MAP_ADDR_DEMOD].v + devp->data->regoff.off_dvbc;
}

static inline void __iomem *gbase_dvbc_2(void)
{
	struct amldtvdemod_device_s *devp = dtvdemod_get_dev();

	return devp->reg_v[ES_MAP_ADDR_DEMOD].v + devp->data->regoff.off_dvbc_2;
}

static inline void __iomem *gbase_dtmb(void)
{
	struct amldtvdemod_device_s *devp = dtvdemod_get_dev();

	return devp->reg_v[ES_MAP_ADDR_DEMOD].v + devp->data->regoff.off_dtmb;
}

static inline void __iomem *gbase_atsc(void)
{
	struct amldtvdemod_device_s *devp = dtvdemod_get_dev();

	return devp->reg_v[ES_MAP_ADDR_DEMOD].v + devp->data->regoff.off_atsc;
}

static inline void __iomem *gbase_demod(void)
{
	struct amldtvdemod_device_s *devp = dtvdemod_get_dev();

	return devp->reg_v[ES_MAP_ADDR_DEMOD].v + devp->data->regoff.off_demod_top;
}

static inline void __iomem *gbase_isdbt(void)
{
	struct amldtvdemod_device_s *devp = dtvdemod_get_dev();

	return devp->reg_v[ES_MAP_ADDR_DEMOD].v + devp->data->regoff.off_isdbt;
}

static inline void __iomem *gbase_front(void)
{
	struct amldtvdemod_device_s *devp = dtvdemod_get_dev();

	return devp->reg_v[ES_MAP_ADDR_DEMOD].v + devp->data->regoff.off_front;
}

static inline void __iomem *gbase_ambus(void)
{
	struct amldtvdemod_device_s *devp = dtvdemod_get_dev();

	return devp->reg_v[ES_MAP_ADDR_DEMOD].v + devp->data->regoff.off_isdbt;
}

static inline void __iomem *gbase_aobus(void)
{
	struct amldtvdemod_device_s *devp = dtvdemod_get_dev();

	return devp->reg_v[ES_MAP_ADDR_AOBUS].v;
}

static inline void __iomem *gbase_iohiu(void)
{
	struct amldtvdemod_device_s *devp = dtvdemod_get_dev();

	return devp->reg_v[ES_MAP_ADDR_IOHIU].v;
}

static inline void __iomem *gbase_reset(void)
{
	struct amldtvdemod_device_s *devp = dtvdemod_get_dev();

	return devp->reg_v[ES_MAP_ADDR_RESET].v;
}

static inline void __iomem *gbase_dmc(void)
{
	struct amldtvdemod_device_s *devp = dtvdemod_get_dev();

	return devp->dmc_v_addr;
}

static inline void __iomem *gbase_ddr(void)
{
	struct amldtvdemod_device_s *devp = dtvdemod_get_dev();

	return devp->ddr_v_addr;
}

static inline unsigned int gphybase_demod(void)
{
	struct amldtvdemod_device_s *devp = dtvdemod_get_dev();

	return devp->reg_p[ES_MAP_ADDR_DEMOD].phy_addr;
}

static inline unsigned int gphybase_demodcfg(void)
{
	struct amldtvdemod_device_s *devp = dtvdemod_get_dev();

	return devp->reg_p[ES_MAP_ADDR_DEMOD].phy_addr +
			devp->data->regoff.off_demod_top;
}

static inline unsigned int gphybase_hiu(void)
{
	struct amldtvdemod_device_s *devp = dtvdemod_get_dev();

	return devp->reg_p[ES_MAP_ADDR_IOHIU].phy_addr;
}

/*poll*/
void dtmb_poll_start(struct aml_dtvdemod *demod);
void dtmb_poll_stop(struct aml_dtvdemod *demod);
unsigned int dtmb_is_update_delay(struct aml_dtvdemod *demod);
unsigned int dtmb_get_delay_clear(struct aml_dtvdemod *demod);
unsigned int dtmb_is_have_check(void);
void dtmb_poll_v3(struct aml_dtvdemod *demod);
unsigned int demod_dvbc_get_fast_search(void);
void demod_dvbc_set_fast_search(unsigned int en);
unsigned int dtvdemod_get_atsc_lock_sts(struct aml_dtvdemod *demod);
const char *dtvdemod_get_cur_delsys(enum fe_delivery_system delsys);
void aml_dtv_demode_isr_en(struct amldtvdemod_device_s *devp, u32 en);
u32 dvbc_get_symb_rate(struct aml_dtvdemod *demod);
unsigned int demod_is_t5d_cpu(struct amldtvdemod_device_s *devp);
int dtmb_information(struct seq_file *seq);

#ifdef MODULE
struct dvb_frontend *aml_dtvdm_attach(const struct demod_config *config);
#endif
void cci_run_new(struct amldtvdemod_device_s *devp);
void atsc_reset_new(void);
unsigned int cfo_run_new(void);
void set_cr_ck_rate_new(void);

#endif
