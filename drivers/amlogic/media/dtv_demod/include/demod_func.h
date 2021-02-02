/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __DEMOD_FUNC_H__

#define __DEMOD_FUNC_H__

#include <linux/types.h>

#include "aml_demod.h"
#include "dvb_frontend.h" /**/
#include "amlfrontend.h"
#include "addr_dtmb_top.h"
#include "addr_atsc_demod.h"
#include "addr_atsc_eq.h"
#include "addr_atsc_cntr.h"
#include "dtv_demod_regs.h"

/*#include "c_stb_define.h"*/
/*#include "c_stb_regs_define.h"*/
#include <linux/io.h>
#include <linux/jiffies.h>
#include "atsc_func.h"
#if defined DEMOD_FPGA_VERSION
#include "fpga_func.h"
#endif

#define PWR_ON    1
#define PWR_OFF   0

/* offset define */
#define DEMOD_REG_ADDR_OFFSET(reg)	(reg & 0xfffff)
#define QAM_BASE			(0x400)	/* is offset */
#define DEMOD_CFG_BASE			(0xC00)	/* is offset */

/* adc */
#define D_HHI_DADC_CNTL				(0x27 << 2)
#define D_HHI_DADC_CNTL2			(0x28 << 2)
#define D_HHI_DADC_RDBK0_I			(0x29 << 2)
#define D_HHI_DADC_CNTL3			(0x2a << 2)
#define D_HHI_DADC_CNTL4			(0x2b << 2)

#define D_HHI_VDAC_CNTL0			(0xbd << 2)

/* PLL */
#define D_HHI_ADC_PLL_CNTL5			(0x9e << 2)
#define D_HHI_ADC_PLL_CNTL6			(0x9f << 2)
#define D_HHI_ADC_PLL_CNTL			(0xaa << 2)
#define D_HHI_ADC_PLL_CNTL2			(0xab << 2)
#define D_HHI_ADC_PLL_CNTL3			(0xac << 2)
#define D_HHI_ADC_PLL_CNTL4			(0xad << 2)
#define D_HHI_ADC_PLL_CNTL1			(0xaf << 2)

/**/
#define D_HHI_HDMI_CLK_CNTL			(0x73 << 2)
#define D_HHI_DEMOD_CLK_CNTL			(0x74 << 2)
/*redefine*/
#define HHI_DEMOD_MEM_PD_REG		(0x43 << 2)
/*redefine*/
#define RESET_RESET0_LEVEL			(0x80)

/*----------------------------------*/
/*register offset define in C_stb_regs_define.h*/
#define AO_RTI_GEN_PWR_SLEEP0 ((0x00 << 10) | (0x3a << 2))
#define AO_RTI_GEN_PWR_ISO0 ((0x00 << 10) | (0x3b << 2))
/*----------------------------------*/


/* demod register */
#define DEMOD_TOP_REG0		(0x00)
#define DEMOD_TOP_REG4		(0x04)
#define DEMOD_TOP_REG8		(0x08)
#define DEMOD_TOP_REGC		(0x0C)
#define DEMOD_TOP_CFG_REG_4	(0x10)

/*reset register*/
#define reg_reset			(0x1c)

#define IO_CBUS_PHY_BASE        (0xc0800000)

#define DEMOD_REG0_VALUE                 0x0000d007
#define DEMOD_REG4_VALUE                 0x2e805400
#define DEMOD_REG8_VALUE                 0x201

/* for table end */
#define TABLE_FLG_END		0xffffffff

/* debug info=====================================================*/
extern int aml_demod_debug;

#define DBG_INFO	BIT(0)
#define DBG_REG		BIT(1)
#define DBG_ATSC	BIT(2)
#define DBG_DTMB	BIT(3)
#define DBG_LOOP	BIT(4)
#define DBG_DVBC	BIT(5)
#define DBG_DVBT	BIT(6)
#define DBG_DVBS	BIT(7)
#define DBG_ISDBT	BIT(8)

#define PR_INFO(fmt, args ...)	printk("dtv_dmd:"fmt, ##args)

#define PR_DBG(fmt, args ...) \
	do { \
		if (aml_demod_debug & DBG_INFO) { \
			printk("dtv_dmd:"fmt, ##args); \
		} \
	} while (0)

#define PR_ATSC(fmt, args ...) \
	do { \
		if (aml_demod_debug & DBG_ATSC) { \
			printk("dtv_dmd:"fmt, ##args); \
		} \
	} while (0)

#define PR_DVBC(fmt, args ...) \
	do { \
		if (aml_demod_debug & DBG_DVBC) { \
			pr_info("dtv_dmd:"fmt, ##args); \
		} \
	} while (0)

#define PR_DVBT(fmt, args ...) \
	do { \
		if (aml_demod_debug & DBG_DVBT) { \
			pr_info("dtv_dmd:"fmt, ##args); \
		} \
	} while (0)

#define PR_DVBS(fmt, args ...) \
	do { \
		if (aml_demod_debug & DBG_DVBS) { \
			pr_info("dtv_dmd:"fmt, ##args); \
		} \
	} while (0)

#define PR_DTMB(fmt, args ...) \
	do { \
		if (aml_demod_debug & DBG_DTMB) { \
			pr_info("dtv_dmd:"fmt, ##args); \
		} \
	} while (0)

#define PR_ISDBT(fmt, args ...) \
	do { \
		if (aml_demod_debug & DBG_ISDBT) { \
			pr_info("dtv_dmd:"fmt, ##args); \
		} \
	} while (0)

/*polling*/
#define PR_DBGL(fmt, args ...) \
	do { \
		if (aml_demod_debug & DBG_LOOP) { \
			pr_info("dtv_dmd:"fmt, ##args); \
		} \
	} while (0)

#define PR_ERR(fmt, args ...) pr_err("dtv_dmd:"fmt, ## args)
#define PR_WAR(fmt, args...)  pr_warn("dtv_dmd:" fmt, ## args)


enum demod_reg_mode {
	REG_MODE_DTMB,
	REG_MODE_DVBT_ISDBT,
	REG_MODE_DVBT_T2,
	REG_MODE_ATSC,
	REG_MODE_DVBC_J83B,
	REG_MODE_FRONT,
	REG_MODE_TOP,
	REG_MODE_CFG,
	REG_MODE_BASE,
	REG_MODE_FPGA,
	REG_MODE_COLLECT_DATA,
	REG_MODE_OTHERS
};

enum {
	AMLOGIC_DTMB_STEP0,
	AMLOGIC_DTMB_STEP1,
	AMLOGIC_DTMB_STEP2,
	AMLOGIC_DTMB_STEP3,
	AMLOGIC_DTMB_STEP4,
	AMLOGIC_DTMB_STEP5,	/* time eq */
	AMLOGIC_DTMB_STEP6,	/* set normal mode sc */
	AMLOGIC_DTMB_STEP7,
	AMLOGIC_DTMB_STEP8,	/* set time eq mode */
	AMLOGIC_DTMB_STEP9,	/* reset */
	AMLOGIC_DTMB_STEP10,	/* set normal mode mc */
	AMLOGIC_DTMB_STEP11,
};

enum {
	DTMB_IDLE = 0,
	DTMB_AGC_READY = 1,
	DTMB_TS1_READY = 2,
	DTMB_TS2_READY = 3,
	DTMB_FE_READY = 4,
	DTMB_PNPHASE_READY = 5,
	DTMB_SFO_INIT_READY = 6,
	DTMB_TS3_READY = 7,
	DTMB_PM_INIT_READY = 8,
	DTMB_CHE_INIT_READY = 9,
	DTMB_FEC_READY = 10
};

enum {
	REG_M_DEMOD = 0,
	/*REG_M_TVAFE,*/
	REG_M_HIU,
	REG_M_NONE
};

enum dtv_demod_reg_access_mode_e {
	ACCESS_WORD,
	ACCESS_BITS,
	ACCESS_BYTE,
	ACCESS_NUM
};

struct aml_demod_reg {
	enum demod_reg_mode mode;
	u8_t rw;/* 0: read, 1: write. */
	u32_t addr;
	u32_t val;
	enum dtv_demod_reg_access_mode_e access_mode;
	u32_t start_bit;
	u32_t bit_width;
};

enum st_chip_regsize_t {
	STCHIP_REGSIZE_8 = 1, /*  8 bits */
	STCHIP_REGSIZE_16 = 2, /* 16 bits */
	STCHIP_REGSIZE_24 = 3, /* 24 bits */
	STCHIP_REGSIZE_32 = 4, /* 32 bits */
	STCHIP_REGSIZE_64 = 8  /* 64 bits */
};

struct stchip_register_t {
	unsigned short addr;     /* Address */
	char value;    /* Current value */
	enum st_chip_regsize_t size;	         /* register size         */
};

void st_dvbs2_init(void);
struct dvb_frontend *aml_get_fe(void);
void tuner_set_params(struct dvb_frontend *fe);
int tuner_get_ch_power(struct dvb_frontend *fe);
int tuner_get_ch_power2(void);

u16 tuner_get_ch_power3(void);

int dtmb_get_power_strength(int agc_gain);

extern int dvbc_get_power_strength(int agc_gain, int tuner_strength);
extern int j83b_get_power_strength(int agc_gain, int tuner_strength);
extern int atsc_get_power_strength(int agc_gain, int tuner_strength);

/* dvbt */
int dvbt_isdbt_set_ch(struct aml_demod_sta *demod_sta, struct aml_demod_dvbt *demod_dvbt);
unsigned int dvbt_set_ch(struct aml_demod_sta *demod_sta, struct aml_demod_dvbt *demod_dvbt);
int dvbt2_set_ch(struct amldtvdemod_device_s *devp);

struct demod_status_ops {
	int (*get_status)(void);
	int (*get_ber)(void);
	int (*get_snr)(struct aml_demod_sta *demod_sta);
	int (*get_strength)(struct aml_demod_sta *demod_sta);
	int (*get_ucblocks)(struct aml_demod_sta *demod_sta);
};

struct demod_status_ops *dvbt_get_status_ops(void);

/* dvbc */

int dvbc_set_ch(struct aml_demod_sta *demod_sta, struct aml_demod_dvbc *demod_dvbc);
int dvbc_status(struct aml_demod_sta *demod_sta, struct aml_demod_sts *demod_sts);
int dvbc_isr_islock(void);
void dvbc_isr(struct aml_demod_sta *demod_sta);
u32 dvbc_set_qam_mode(unsigned char mode);
u32 dvbc_get_status(void);
u32 dvbc_set_auto_symtrack(void);
int dvbc_timer_init(void);
void dvbc_timer_exit(void);
int dvbc_cci_task(void *data);
int dvbc_get_cci_task(void);
void dvbc_create_cci_task(void);
void dvbc_kill_cci_task(void);

/* DVBC */
/*gxtvbb*/

extern void dvbc_reg_initial_old(struct aml_demod_sta *demod_sta);

/*txlx*/
extern void dvbc_reg_initial(struct aml_demod_sta *demod_sta);
extern void demod_dvbc_set_qam(unsigned int qam);
extern void dvbc_init_reg_ext(void);
extern u32 dvbc_get_ch_sts(void);
extern u32 dvbc_get_qam_mode(void);


/* dtmb */

int dtmb_set_ch(struct aml_demod_sta *demod_sta,
		/*struct aml_demod_i2c *demod_i2c,*/
		struct aml_demod_dtmb *demod_atsc);

void dtmb_reset(void);

int dtmb_check_status_gxtv(struct dvb_frontend *fe);
int dtmb_check_status_txl(struct dvb_frontend *fe);
int dtmb_bch_check(void);
void dtmb_write_reg(int reg_addr, int reg_data);
unsigned int dtmb_read_reg(unsigned int reg_addr);
void dtmb_register_reset(void);

/*
 * dtmb register write / read
 */

/*test only*/
enum REG_DTMB_D9 {
	DTMB_D9_IF_GAIN,
	DTMB_D9_RF_GAIN,
	DTMB_D9_POWER,
	DTMB_D9_ALL,
};

extern void dtmb_set_mem_st(int mem_start);
extern int dtmb_read_agc(enum REG_DTMB_D9 type, unsigned int *buf);
extern unsigned int dtmb_reg_r_che_snr(void);
extern unsigned int dtmb_reg_r_fec_lock(void);
extern unsigned int dtmb_reg_r_bch(void);
extern int check_dtmb_fec_lock(void);
extern int dtmb_constell_check(void);

extern void dtmb_no_signal_check_v3(void);
extern void dtmb_no_signal_check_finishi_v3(void);
extern unsigned int dtmb_detect_first(void);


/* demod functions */
unsigned long apb_read_reg_collect(unsigned long addr);
void apb_write_reg_collect(unsigned int addr, unsigned int data);
void apb_write_reg(unsigned int reg, unsigned int val);
unsigned long apb_read_reg_high(unsigned long addr);
unsigned long apb_read_reg(unsigned long reg);
int app_apb_write_reg(int addr, int data);
int app_apb_read_reg(int addr);

void demod_set_cbus_reg(unsigned int data, unsigned int addr);
unsigned int demod_read_cbus_reg(unsigned int addr);
void demod_set_demod_reg(unsigned int data, unsigned int addr);
extern void demod_set_tvfe_reg(unsigned int data, unsigned int addr);
unsigned int demod_read_demod_reg(unsigned int addr);

/* extern int clk_measure(char index); */

void ofdm_initial(int bandwidth,
		  /* 00:8M 01:7M 10:6M 11:5M */
		  int samplerate,
		  /* 00:45M 01:20.8333M 10:20.7M 11:28.57 */
		  int IF,
		  /* 000:36.13M 001:-5.5M 010:4.57M 011:4M 100:5M */
		  int mode,
		  /* 00:DVBT,01:ISDBT */
		  int tc_mode
		  /* 0: Unsigned, 1:TC */);

/*no use void monitor_isdbt(void);*/
void demod_set_reg(struct aml_demod_reg *demod_reg);
void demod_get_reg(struct aml_demod_reg *demod_reg);

/* void demod_calc_clk(struct aml_demod_sta *demod_sta); */
int demod_set_sys(struct amldtvdemod_device_s *devp, struct aml_demod_sta *demod_sta,
		  struct aml_demod_sys *demod_sys);
extern void set_j83b_filter_reg_v4(void);


/* for g9tv */
void adc_dpll_setup(int clk_a, int clk_b, int clk_sys, struct aml_demod_sta *demod_sta);
void demod_power_switch(int pwr_cntl);

union adc_pll_cntl {
	/** raw register data */
	uint32_t d32;
	/** register bits */
	struct {
		unsigned pll_m:9;
		unsigned pll_n:5;
		unsigned pll_od0:2;
		unsigned pll_od1:2;
		unsigned pll_od2:2;
		unsigned pll_xd0:6;
		unsigned pll_xd1:6;
	} b;
};

union adc_pll_cntl2 {
	/** raw register data */
	uint32_t d32;
	/** register bits */
	struct {
		unsigned output_mux_ctrl:4;
		unsigned div2_ctrl:1;
		unsigned b_polar_control:1;
		unsigned a_polar_control:1;
		unsigned gate_ctrl:6;
		unsigned tdc_buf:8;
		unsigned lm_s:6;
		unsigned lm_w:4;
		unsigned reserved:1;
	} b;
};

union adc_pll_cntl3 {
	/** raw register data */
	uint32_t d32;
	/** register bits */
	struct {
		unsigned afc_dsel_in:1;
		unsigned afc_dsel_bypass:1;
		unsigned dco_sdmck_sel:2;
		unsigned dc_vc_in:2;
		unsigned dco_m_en:1;
		unsigned dpfd_lmode:1;
		unsigned filter_acq1:11;
		unsigned enable:1;
		unsigned filter_acq2:11;
		unsigned reset:1;
	} b;
};

union adc_pll_cntl4 {
	/** raw register data */
	uint32_t d32;
	/** register bits */
	struct {
		unsigned reve:12;
		unsigned tdc_en:1;
		unsigned dco_sdm_en:1;
		unsigned dco_iup:2;
		unsigned pvt_fix_en:1;
		unsigned iir_bypass_n:1;
		unsigned pll_od3:2;
		unsigned filter_pvt1:4;
		unsigned filter_pvt2:4;
		unsigned reserved:4;
	} b;
};

/* ///////////////////////////////////////////////////////////////// */

union demod_dig_clk {
	/** raw register data */
	uint32_t d32;
	/** register bits */
	struct {
		unsigned demod_clk_div:7;
		unsigned reserved0:1;
		unsigned demod_clk_en:1;
		unsigned demod_clk_sel:2;
		unsigned reserved1:5;
		unsigned adc_extclk_div:7;	/* 34 */
		unsigned use_adc_extclk:1;	/* 1 */
		unsigned adc_extclk_en:1;	/* 1 */
		unsigned adc_extclk_sel:3;	/* 1 */
		unsigned reserved2:4;
	} b;
};

union demod_adc_clk {
	/** raw register data */
	uint32_t d32;
	/** register bits */
	struct {
		unsigned pll_m:9;
		unsigned pll_n:5;
		unsigned pll_od:2;
		unsigned pll_xd:5;
		unsigned reserved0:3;
		unsigned pll_ss_clk:4;
		unsigned pll_ss_en:1;
		unsigned reset:1;
		unsigned pll_pd:1;
		unsigned reserved1:1;
	} b;
};

union demod_cfg0 {
	/** raw register data */
	uint32_t d32;
	/** register bits */
	struct {
		unsigned mode:4;
		unsigned ts_sel:4;
		unsigned test_bus_clk:1;
		unsigned adc_ext:1;
		unsigned adc_rvs:1;
		unsigned adc_swap:1;
		unsigned adc_format:1;
		unsigned adc_regout:1;
		unsigned adc_regsel:1;
		unsigned adc_regadj:5;
		unsigned adc_value:10;
		unsigned adc_test:1;
		unsigned ddr_sel:1;
	} b;
};

union demod_cfg1 {
	/** raw register data */
	uint32_t d32;
	/** register bits */
	struct {
		unsigned reserved:8;
		unsigned ref_top:2;
		unsigned ref_bot:2;
		unsigned cml_xs:2;
		unsigned cml_1s:2;
		unsigned vdda_sel:2;
		unsigned bias_sel_sha:2;
		unsigned bias_sel_mdac2:2;
		unsigned bias_sel_mdac1:2;
		unsigned fast_chg:1;
		unsigned rin_sel:3;
		unsigned en_ext_vbg:1;
		unsigned en_cmlgen_res:1;
		unsigned en_ext_vdd12:1;
		unsigned en_ext_ref:1;
	} b;
};

union demod_cfg2 {
	/** raw register data */
	uint32_t d32;
	/** register bits */
	struct {
		unsigned en_adc:1;
		unsigned biasgen_ibipt_sel:2;
		unsigned biasgen_ibic_sel:2;
		unsigned biasgen_rsv:4;
		unsigned biasgen_en:1;
		unsigned biasgen_bias_sel_adc:2;
		unsigned biasgen_bias_sel_cml1:2;
		unsigned biasgen_bias_sel_ref_op:2;
		unsigned clk_phase_sel:1;
		unsigned reserved:15;
	} b;
};

union demod_cfg3 {
	/** raw register data */
	uint32_t d32;
	/** register bits */
	struct {
		unsigned dc_arb_mask:3;
		unsigned dc_arb_enable:1;
		unsigned reserved:28;
	} b;
};

struct atsc_cfg {
	int adr;
	int dat;
	int rw;
};

struct agc_power_tab {
	char name[128];
	int level;
	int ncalcE;
	int *calcE;
};

struct dtmb_cfg {
	int dat;
	int adr;
	int rw;
};

void dtvpll_lock_init(void);
void dtvpll_init_flag(int on);
void demod_set_irq_mask(void);
void demod_clr_irq_stat(void);
void demod_set_adc_core_clk(int adc_clk, int sys_clk, struct aml_demod_sta *demod_sta);
void demod_set_adc_core_clk_fix(int clk_adc, int clk_dem);
void calculate_cordic_para(void);
/*extern int aml_fe_analog_set_frontend(struct dvb_frontend *fe);*/
int get_dtvpll_init_flag(void);
void demod_set_mode_ts(enum fe_delivery_system delsys);
void qam_write_reg(unsigned int reg_addr, unsigned int reg_data);
unsigned int qam_read_reg(unsigned int reg_addr);
void qam_write_bits(u32 reg_addr, const u32 reg_data,
		    const u32 start, const u32 len);
extern void dvbc_write_reg(unsigned int addr, unsigned int data);
extern unsigned int dvbc_read_reg(unsigned int addr);
void demod_top_write_reg(unsigned int addr, unsigned int data);
void demod_top_write_bits(u32 reg_addr, const u32 reg_data, const u32 start, const u32 len);
unsigned int demod_top_read_reg(unsigned int addr);
void demod_init_mutex(void);
void front_write_reg(unsigned int addr, unsigned int data);
void front_write_bits(u32 reg_addr, const u32 reg_data,
		    const u32 start, const u32 len);
unsigned int front_read_reg(unsigned int addr);
extern unsigned int isdbt_read_reg_v4(unsigned int addr);
extern void  isdbt_write_reg_v4(unsigned int addr, unsigned int data);
extern int dd_tvafe_hiu_reg_write(unsigned int reg, unsigned int val);
extern unsigned int dd_tvafe_hiu_reg_read(unsigned int addr);
void dtvdemod_dmc_reg_write(unsigned int reg, unsigned int val);
unsigned int dtvdemod_dmc_reg_read(unsigned int addr);
extern int reset_reg_write(unsigned int reg, unsigned int val);
extern unsigned int reset_reg_read(unsigned int addr);


void clocks_set_sys_defaults(struct dvb_frontend *fe, struct aml_demod_sta *demod_sta,
				unsigned int adc_clk);
extern void demod_set_demod_default(void);
extern unsigned int demod_get_adc_clk(void);
extern unsigned int demod_get_sys_clk(void);
extern void debug_adc_pll(void);
extern void debug_check_reg_val(unsigned int reg_mode, unsigned int reg);

/*register access api new*/
void dvbt_isdbt_wr_reg(unsigned int addr, unsigned int data);
void dvbt_isdtb_wr_reg_new(unsigned int addr, unsigned int data);
void dvbt_isdbt_wr_bits_new(u32 reg_addr, const u32 reg_data,
		    const u32 start, const u32 len);
unsigned int dvbt_isdbt_rd_reg(unsigned int addr);
unsigned int dvbt_isdbt_rd_reg_new(unsigned int addr);
void dvbt_t2_wrb(unsigned int addr, char data);
void dvbt_t2_write_w(unsigned int addr, unsigned int data);
void dvbt_t2_wr_byte_bits(u32 addr, const u32 data, const u32 start, const u32 len);
void dvbt_t2_wr_word_bits(u32 addr, const u32 data, const u32 start, const u32 len);
unsigned int dvbt_t2_read_w(unsigned int addr);
char dvbt_t2_rdb(unsigned int addr);
void riscv_ctl_write_reg(unsigned int addr, unsigned int data);
void dvbs_write_bits(u32 reg_addr, const u32 reg_data,
		    const u32 start, const u32 len);
void dvbs_wr_byte(unsigned int addr, char data);
char dvbs_rd_byte(unsigned int addr);
void dvbs_write_reg(unsigned int addr, unsigned int data);
unsigned int dvbs_read_reg(unsigned int addr);
int aml_demod_init(void);
void aml_demod_exit(void);
void write_riscv_ram(void);
void dvbs2_reg_initial(unsigned int symb_rate);
#endif
