
#ifdef DEMOD_FUNC_H
#else
#define DEMOD_FUNC_H

#include <linux/types.h>
/*#include <mach/am_regs.h>
#include <mach/register.h>
#include <mach/avosstyle_io.h>
#include <mach/io.h>*/
#include <linux/dvb/aml_demod.h>
#include "../aml_fe.h"
#include "amlfrontend.h"
#define G9_TV

#define PWR_ON    1
#define PWR_OFF   0

#define dtmb_mobile_mode

/*#define DEMOD_BASE     APB_REG_ADDR(0x20000)*/
#define DEMOD_BASE DEMOD_REG_ADDR(0x0)
/*0xc8020000 //APB_REG_ADDR(0x20000)      0xd0020000*/

/*#define DEMOD_BASE 0xc8020000*/
#define DTMB_BASE  (DEMOD_BASE+0x000)
#define DVBT_BASE  (DEMOD_BASE+0x000)
#define ISDBT_BASE (DEMOD_BASE+0x000)
#define QAM_BASE   (DEMOD_BASE+0x400)
#define ATSC_BASE  (DEMOD_BASE+0x800)
#define DEMOD_CFG_BASE  (DEMOD_BASE+0xC00)

#ifndef G9_TV
#define ADC_REG1_VALUE		 0x003b0232
#define ADC_REG2_VALUE		 0x814d3928
#define ADC_REG3_VALUE		 0x6b425012
#define ADC_REG4_VALUE		 0x101
#define ADC_REG4_CRY_VALUE 0x301
#define ADC_REG5_VALUE		 0x70b
#define ADC_REG6_VALUE		 0x713

#define ADC_REG1	 0x10aa
#define ADC_REG2	 0x10ab
#define ADC_REG3	 0x10ac
#define ADC_REG4	 0x10ad
#define ADC_REG5	 0x1073
#define ADC_REG6	 0x1074
#else

#define ADC_RESET_VALUE		 0x8a2a2110	/*0xce7a2110 */
#define ADC_REG1_VALUE		 0x00100228
#define ADC_REG2_VALUE		 0x34e0bf80	/*0x34e0bf81 */
#define ADC_REG2_VALUE_CRY	 0x34e0bf81
#define ADC_REG3_VALUE		 0x0a2a2110	/*0x4e7a2110 */
#define ADC_REG4_VALUE		 0x02933800
#define ADC_REG4_CRY_VALUE 0x301
#define ADC_REG7_VALUE		 0x01411036
#define ADC_REG8_VALUE		 0x00000000
#define ADC_REG9_VALUE		 0x00430036
#define ADC_REGA_VALUE		 0x80480240

/*DADC DPLL*/
#define ADC_REG1	 0x10aa
#define ADC_REG2	 0x10ab
#define ADC_REG3	 0x10ac
#define ADC_REG4	 0x10ad

#define ADC_REG5	 0x1073
#define ADC_REG6	 0x1074

/*DADC REG*/
#define ADC_REG7	 0x1027
#define ADC_REG8	 0x1028
#define ADC_REG9	 0x102a
#define ADC_REGA	 0x102b

#endif

#define DEMOD_REG1_VALUE		 0x0000d007
#define DEMOD_REG2_VALUE		 0x2e805400
#define DEMOD_REG3_VALUE		 0x201

#define DEMOD_REG1		 0xc00
#define DEMOD_REG2		 0xc04
#define DEMOD_REG3		 0xc08

/*#define Wr(addr, data)   WRITE_CBUS_REG(addr, data) */
/**(volatile unsigned long *)(0xc1100000|(addr<<2))=data*/
/*#define Rd(addr)             READ_CBUS_REG(addr)    */
/**(volatile unsigned long *)(0xc1100000|(addr<<2))*/
/*
#define Wr(addr, data) *(volatile unsigned long *)(addr) = (data)
#define Rd(addr) *(volatile unsigned long *)(addr)
*/

#define Wr(addr, data) (*(unsigned long *)(addr) = (data))
#define Rd(addr) (*(unsigned long *)(addr))

/* i2c functions*/
/*int aml_i2c_sw_test_bus(struct aml_demod_i2c *adap, char *name);*/
int am_demod_i2c_xfer(struct aml_demod_i2c *adap, struct i2c_msg *msgs,
		      int num);
int init_tuner_fj2207(struct aml_demod_sta *demod_sta,
		      struct aml_demod_i2c *adap);
int set_tuner_fj2207(struct aml_demod_sta *demod_sta,
		     struct aml_demod_i2c *adap);

int get_fj2207_ch_power(void);
int tuner_get_ch_power(struct aml_fe_dev *adap);
int tda18273_tuner_set_frequncy(unsigned int dwFrequency,
				unsigned int dwStandard);

int tuner_set_ch(struct aml_demod_sta *demod_sta,
		 struct aml_demod_i2c *demod_i2c);

/*dvbt*/
int dvbt_set_ch(struct aml_demod_sta *demod_sta,
		struct aml_demod_i2c *demod_i2c,
		struct aml_demod_dvbt *demod_dvbt);

struct demod_status_ops {
	int (*get_status)(struct aml_demod_sta *demod_sta,
			   struct aml_demod_i2c *demod_i2c);
	int (*get_ber)(struct aml_demod_sta *demod_sta,
			struct aml_demod_i2c *demod_i2c);
	int (*get_snr)(struct aml_demod_sta *demod_sta,
			struct aml_demod_i2c *demod_i2c);
	int (*get_strength)(struct aml_demod_sta *demod_sta,
			     struct aml_demod_i2c *demod_i2c);
	int (*get_ucblocks)(struct aml_demod_sta *demod_sta,
			     struct aml_demod_i2c *demod_i2c);
};

struct demod_status_ops *dvbt_get_status_ops(void);

/*dvbc*/

int dvbc_set_ch(struct aml_demod_sta *demod_sta,
		struct aml_demod_i2c *demod_i2c,
		struct aml_demod_dvbc *demod_dvbc);
int dvbc_status(struct aml_demod_sta *demod_sta,
		struct aml_demod_i2c *demod_i2c,
		struct aml_demod_sts *demod_sts);
int dvbc_isr_islock(void);
void dvbc_isr(struct aml_demod_sta *demod_sta);
u32 dvbc_set_qam_mode(unsigned char mode);
u32 dvbc_get_status(void);
u32 dvbc_set_auto_symtrack(void);
int dvbc_timer_init(void);
void dvbc_timer_exit(void);
int dvbc_cci_task(void *);
int dvbc_get_cci_task(void);
void dvbc_create_cci_task(void);
void dvbc_kill_cci_task(void);

/*atsc*/

int atsc_set_ch(struct aml_demod_sta *demod_sta,
		struct aml_demod_i2c *demod_i2c,
		struct aml_demod_atsc *demod_atsc);
int check_atsc_fsm_status(void);

void atsc_write_reg(int reg_addr, int reg_data);

unsigned long atsc_read_reg(int reg_addr);

unsigned long atsc_read_iqr_reg(void);

int atsc_qam_set(fe_modulation_t mode);

void qam_initial(int qam_id);

/*dtmb*/

int dtmb_set_ch(struct aml_demod_sta *demod_sta,
		struct aml_demod_i2c *demod_i2c,
		struct aml_demod_dtmb *demod_atsc);

void dtmb_reset(void);

int dtmb_read_snr(void);

void dtmb_write_reg(int reg_addr, int reg_data);
long dtmb_read_reg(int reg_addr);
void dtmb_register_reset(void);

/* demod functions*/
void apb_write_reg(int reg, int val);
unsigned long apb_read_reg(int reg);
int app_apb_write_reg(int addr, int data);
int app_apb_read_reg(int addr);

void demod_set_cbus_reg(int data, int addr);
unsigned long demod_read_cbus_reg(int addr);
void demod_set_demod_reg(unsigned long data, unsigned long addr);
unsigned long demod_read_demod_reg(unsigned long addr);

extern int clk_measure(char index);

void ofdm_initial(int bandwidth,
		  /* 00:8M 01:7M 10:6M 11:5M */
		  int samplerate,
		  /* 00:45M 01:20.8333M 10:20.7M 11:28.57 */
		  int IF,
		  /* 000:36.13M 001:-5.5M 010:4.57M 011:4M 100:5M */
		  int mode,
		  /* 00:DVBT,01:ISDBT */
		  int tc_mode
		  /* 0: Unsigned, 1:TC */
);
void monitor_isdbt(void);
void demod_set_reg(struct aml_demod_reg *demod_reg);
void demod_get_reg(struct aml_demod_reg *demod_reg);

/*void demod_calc_clk(struct aml_demod_sta *demod_sta);*/
int demod_set_sys(struct aml_demod_sta *demod_sta,
		  struct aml_demod_i2c *demod_i2c,
		  struct aml_demod_sys *demod_sys);
/*int demod_get_sys(struct aml_demod_i2c *demod_i2c,*/
/*                struct aml_demod_sys *demod_sys);*/
/*int dvbt_set_ch(struct aml_demod_sta *demod_sta,*/
/*              struct aml_demod_i2c *demod_i2c,*/
/*              struct aml_demod_dvbt *demod_dvbt);*/
/*int tuner_set_ch (struct aml_demod_sta *demod_sta,*/
/*                struct aml_demod_i2c *demod_i2c);*/

/*typedef char               int8_t;*/
/*typedef short int          int16_t;*/
/*typedef int                int32_t;*/
/*typedef long               int64_t;*/
/*typedef unsigned char      uint8_t;
typedef unsigned short int uint16_t;
typedef unsigned int       uint32_t;
typedef unsigned long      uint64_t;*/

/*typedef unsigned   char    u8_t;
typedef signed     char    s8_t;
typedef unsigned   short   u16_t;
typedef signed     short   s16_t;
typedef unsigned   int     u32_t;
typedef signed     int     s32_t;
typedef unsigned   long    u64_t;
typedef signed     long    s64_t;*/

/*#define extadc*/

/*for g9tv*/
void adc_dpll_setup(int clk_a, int clk_b, int clk_sys);
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
} adc_pll_cntl_t;

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
} adc_pll_cntl2_t;

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
} adc_pll_cntl3_t;

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
} adc_pll_cntl4_t;

/*/////////////////////////////////////////////////////////////////*/

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
		unsigned adc_extclk_en:1;	/*  1 */
		unsigned adc_extclk_sel:3;	/*   1 */
		unsigned reserved2:4;
	} b;
} demod_dig_clk_t;

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
} demod_adc_clk_t;

/*
struct demod_cfg_regs {
	volatile uint32_t cfg0;
	volatile uint32_t cfg1;
	volatile uint32_t cfg2;
	volatile uint32_t cfg3;
	volatile uint32_t info0;
	volatile uint32_t info1;
} demod_cfg_regs_t;
*/

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
} demod_cfg0_t;

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
} demod_cfg1_t;

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
} demod_cfg2_t;

union demod_cfg3 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned dc_arb_mask:3;
		unsigned dc_arb_enable:1;
		unsigned reserved:28;
	} b;
} demod_cfg3_t;

/*
  struct dtmb_cfg_regs {
	volatile uint32_t dtmb_cfg_00;
	volatile uint32_t dtmb_cfg_01;
	volatile uint32_t dtmb_cfg_02;
	volatile uint32_t dtmb_cfg_03;
	volatile uint32_t dtmb_cfg_04;
	volatile uint32_t dtmb_cfg_05;
	volatile uint32_t dtmb_cfg_06;
	volatile uint32_t dtmb_cfg_07;
	volatile uint32_t dtmb_cfg_08;
	volatile uint32_t dtmb_cfg_09;
	volatile uint32_t dtmb_cfg_0a;
	volatile uint32_t dtmb_cfg_0b;
	volatile uint32_t dtmb_cfg_0c;
	volatile uint32_t dtmb_cfg_0d;
	volatile uint32_t dtmb_cfg_0e;
	volatile uint32_t dtmb_cfg_0f;
	volatile uint32_t dtmb_cfg_10;
	volatile uint32_t dtmb_cfg_11;
	volatile uint32_t dtmb_cfg_12;
	volatile uint32_t dtmb_cfg_13;
	volatile uint32_t dtmb_cfg_14;
	volatile uint32_t dtmb_cfg_15;
	volatile uint32_t dtmb_cfg_16;
	volatile uint32_t dtmb_cfg_17;
	volatile uint32_t dtmb_cfg_18;
	volatile uint32_t dtmb_cfg_19;
	volatile uint32_t dtmb_cfg_1a;
	volatile uint32_t dtmb_cfg_1b;
	volatile uint32_t dtmb_cfg_1c;
	volatile uint32_t dtmb_cfg_1d;
	volatile uint32_t dtmb_cfg_1e;
	volatile uint32_t dtmb_cfg_1f;
	volatile uint32_t dtmb_cfg_20;
	volatile uint32_t dtmb_cfg_21;
	volatile uint32_t dtmb_cfg_22;
	volatile uint32_t dtmb_cfg_23;
	volatile uint32_t dtmb_cfg_24;
	volatile uint32_t dtmb_cfg_25;
	volatile uint32_t dtmb_cfg_26;
	volatile uint32_t dtmb_cfg_27;
	volatile uint32_t dtmb_cfg_28;
	volatile uint32_t dtmb_cfg_29;
	volatile uint32_t dtmb_cfg_2a;
	volatile uint32_t dtmb_cfg_2b;
	volatile uint32_t dtmb_cfg_2c;
	volatile uint32_t dtmb_cfg_2d;
	volatile uint32_t dtmb_cfg_2e;
	volatile uint32_t dtmb_cfg_2f;
	volatile uint32_t dtmb_cfg_30;
	volatile uint32_t dtmb_cfg_31;
	volatile uint32_t dtmb_cfg_32;
	volatile uint32_t dtmb_cfg_33;
	volatile uint32_t dtmb_cfg_34;
	volatile uint32_t dtmb_cfg_35;
	volatile uint32_t dtmb_cfg_36;
	volatile uint32_t dtmb_cfg_37;
	volatile uint32_t dtmb_cfg_38;
	volatile uint32_t dtmb_cfg_39;
	volatile uint32_t dtmb_cfg_3a;
	volatile uint32_t dtmb_cfg_3b;
	volatile uint32_t dtmb_cfg_3c;
	volatile uint32_t dtmb_cfg_3d;
	volatile uint32_t dtmb_cfg_3e;
	volatile uint32_t dtmb_cfg_3f;
	volatile uint32_t dtmb_cfg_40;
	volatile uint32_t dtmb_cfg_41;
	volatile uint32_t dtmb_cfg_42;
	volatile uint32_t dtmb_cfg_43;
	volatile uint32_t dtmb_cfg_44;
	volatile uint32_t dtmb_cfg_45;
	volatile uint32_t dtmb_cfg_46;
	volatile uint32_t dtmb_cfg_47;
	volatile uint32_t dtmb_cfg_48;
	volatile uint32_t dtmb_cfg_49;
	volatile uint32_t dtmb_cfg_4a;
	volatile uint32_t dtmb_cfg_4b;
	volatile uint32_t dtmb_cfg_4c;
	volatile uint32_t dtmb_cfg_4d;
	volatile uint32_t dtmb_cfg_4e;
	volatile uint32_t dtmb_cfg_4f;
	volatile uint32_t dtmb_cfg_50;
	volatile uint32_t dtmb_cfg_51;
	volatile uint32_t dtmb_cfg_52;
	volatile uint32_t dtmb_cfg_53;
	volatile uint32_t dtmb_cfg_54;
	volatile uint32_t dtmb_cfg_55;
	volatile uint32_t dtmb_cfg_56;
	volatile uint32_t dtmb_cfg_57;
	volatile uint32_t dtmb_cfg_58;
	volatile uint32_t dtmb_cfg_59;
	volatile uint32_t dtmb_cfg_5a;
	volatile uint32_t dtmb_cfg_5b;
	volatile uint32_t dtmb_cfg_5c;
	volatile uint32_t dtmb_cfg_5d;
	volatile uint32_t dtmb_cfg_5e;
	volatile uint32_t dtmb_cfg_5f;
	volatile uint32_t dtmb_cfg_60;
	volatile uint32_t dtmb_cfg_61;
	volatile uint32_t dtmb_cfg_62;
	volatile uint32_t dtmb_cfg_63;
	volatile uint32_t dtmb_cfg_64;
	volatile uint32_t dtmb_cfg_65;
	volatile uint32_t dtmb_cfg_66;
	volatile uint32_t dtmb_cfg_67;
	volatile uint32_t dtmb_cfg_68;
	volatile uint32_t dtmb_cfg_69;
	volatile uint32_t dtmb_cfg_6a;
	volatile uint32_t dtmb_cfg_6b;
	volatile uint32_t dtmb_cfg_6c;
	volatile uint32_t dtmb_cfg_6d;
	volatile uint32_t dtmb_cfg_6e;
	volatile uint32_t dtmb_cfg_6f;
	volatile uint32_t dtmb_cfg_70;
	volatile uint32_t dtmb_cfg_71;
	volatile uint32_t dtmb_cfg_72;
	volatile uint32_t dtmb_cfg_73;
	volatile uint32_t dtmb_cfg_74;
	volatile uint32_t dtmb_cfg_75;
	volatile uint32_t dtmb_cfg_76;
	volatile uint32_t dtmb_cfg_77;
	volatile uint32_t dtmb_cfg_78;
	volatile uint32_t dtmb_cfg_79;
	volatile uint32_t dtmb_cfg_7a;
	volatile uint32_t dtmb_cfg_7b;
	volatile uint32_t dtmb_cfg_7c;
	volatile uint32_t dtmb_cfg_7d;
	volatile uint32_t dtmb_cfg_7e;
	volatile uint32_t dtmb_cfg_7f;
	volatile uint32_t dtmb_cfg_80;
	volatile uint32_t dtmb_cfg_81;
	volatile uint32_t dtmb_cfg_82;
	volatile uint32_t dtmb_cfg_83;
	volatile uint32_t dtmb_cfg_84;
	volatile uint32_t dtmb_cfg_85;
	volatile uint32_t dtmb_cfg_86;
	volatile uint32_t dtmb_cfg_87;
	volatile uint32_t dtmb_cfg_88;
	volatile uint32_t dtmb_cfg_89;
	volatile uint32_t dtmb_cfg_8a;
	volatile uint32_t dtmb_cfg_8b;
	volatile uint32_t dtmb_cfg_8c;
	volatile uint32_t dtmb_cfg_8d;
	volatile uint32_t dtmb_cfg_8e;
	volatile uint32_t dtmb_cfg_8f;
	volatile uint32_t dtmb_cfg_90;
	volatile uint32_t dtmb_cfg_91;
	volatile uint32_t dtmb_cfg_92;
	volatile uint32_t dtmb_cfg_93;
	volatile uint32_t dtmb_cfg_94;
	volatile uint32_t dtmb_cfg_95;
	volatile uint32_t dtmb_cfg_96;
	volatile uint32_t dtmb_cfg_97;
	volatile uint32_t dtmb_cfg_98;
	volatile uint32_t dtmb_cfg_99;
	volatile uint32_t dtmb_cfg_9a;
	volatile uint32_t dtmb_cfg_9b;
	volatile uint32_t dtmb_cfg_9c;
	volatile uint32_t dtmb_cfg_9d;
	volatile uint32_t dtmb_cfg_9e;
	volatile uint32_t dtmb_cfg_9f;
	volatile uint32_t dtmb_cfg_a0;
	volatile uint32_t dtmb_cfg_a1;
	volatile uint32_t dtmb_cfg_a2;
	volatile uint32_t dtmb_cfg_a3;
	volatile uint32_t dtmb_cfg_a4;
	volatile uint32_t dtmb_cfg_a5;
	volatile uint32_t dtmb_cfg_a6;
	volatile uint32_t dtmb_cfg_a7;
	volatile uint32_t dtmb_cfg_a8;
	volatile uint32_t dtmb_cfg_a9;
	volatile uint32_t dtmb_cfg_aa;
	volatile uint32_t dtmb_cfg_ab;
	volatile uint32_t dtmb_cfg_ac;
	volatile uint32_t dtmb_cfg_ad;
	volatile uint32_t dtmb_cfg_ae;
	volatile uint32_t dtmb_cfg_af;
	volatile uint32_t dtmb_cfg_b0;
	volatile uint32_t dtmb_cfg_b1;
	volatile uint32_t dtmb_cfg_b2;
	volatile uint32_t dtmb_cfg_b3;
	volatile uint32_t dtmb_cfg_b4;
	volatile uint32_t dtmb_cfg_b5;
	volatile uint32_t dtmb_cfg_b6;
	volatile uint32_t dtmb_cfg_b7;
	volatile uint32_t dtmb_cfg_b8;
	volatile uint32_t dtmb_cfg_b9;
	volatile uint32_t dtmb_cfg_ba;
	volatile uint32_t dtmb_cfg_bb;
	volatile uint32_t dtmb_cfg_bc;
	volatile uint32_t dtmb_cfg_bd;
	volatile uint32_t dtmb_cfg_be;
	volatile uint32_t dtmb_cfg_bf;
	volatile uint32_t dtmb_cfg_c0;
	volatile uint32_t dtmb_cfg_c1;
	volatile uint32_t dtmb_cfg_c2;
	volatile uint32_t dtmb_cfg_c3;
	volatile uint32_t dtmb_cfg_c4;
	volatile uint32_t dtmb_cfg_c5;
	volatile uint32_t dtmb_cfg_c6;
	volatile uint32_t dtmb_cfg_c7;
	volatile uint32_t dtmb_cfg_c8;
	volatile uint32_t dtmb_cfg_c9;
	volatile uint32_t dtmb_cfg_ca;
	volatile uint32_t dtmb_cfg_cb;
	volatile uint32_t dtmb_cfg_cc;
	volatile uint32_t dtmb_cfg_cd;
	volatile uint32_t dtmb_cfg_ce;
	volatile uint32_t dtmb_cfg_cf;
	volatile uint32_t dtmb_cfg_d0;
	volatile uint32_t dtmb_cfg_d1;
	volatile uint32_t dtmb_cfg_d2;
	volatile uint32_t dtmb_cfg_d3;
	volatile uint32_t dtmb_cfg_d4;
	volatile uint32_t dtmb_cfg_d5;
	volatile uint32_t dtmb_cfg_d6;
	volatile uint32_t dtmb_cfg_d7;
	volatile uint32_t dtmb_cfg_d8;
	volatile uint32_t dtmb_cfg_d9;
	volatile uint32_t dtmb_cfg_da;
	volatile uint32_t dtmb_cfg_db;
	volatile uint32_t dtmb_cfg_dc;
	volatile uint32_t dtmb_cfg_dd;
	volatile uint32_t dtmb_cfg_de;
	volatile uint32_t dtmb_cfg_df;
	volatile uint32_t dtmb_cfg_e0;
	volatile uint32_t dtmb_cfg_e1;
	volatile uint32_t dtmb_cfg_e2;
	volatile uint32_t dtmb_cfg_e3;
	volatile uint32_t dtmb_cfg_e4;
	volatile uint32_t dtmb_cfg_e5;
	volatile uint32_t dtmb_cfg_e6;
	volatile uint32_t dtmb_cfg_e7;
	volatile uint32_t dtmb_cfg_e8;
	volatile uint32_t dtmb_cfg_e9;
	volatile uint32_t dtmb_cfg_ea;
	volatile uint32_t dtmb_cfg_eb;
	volatile uint32_t dtmb_cfg_ec;
	volatile uint32_t dtmb_cfg_ed;
	volatile uint32_t dtmb_cfg_ee;
	volatile uint32_t dtmb_cfg_ef;
	volatile uint32_t dtmb_cfg_f0;
	volatile uint32_t dtmb_cfg_f1;
	volatile uint32_t dtmb_cfg_f2;
	volatile uint32_t dtmb_cfg_f3;
	volatile uint32_t dtmb_cfg_f4;
	volatile uint32_t dtmb_cfg_f5;
	volatile uint32_t dtmb_cfg_f6;
	volatile uint32_t dtmb_cfg_f7;
	volatile uint32_t dtmb_cfg_f8;
	volatile uint32_t dtmb_cfg_f9;
	volatile uint32_t dtmb_cfg_fa;
	volatile uint32_t dtmb_cfg_fb;
	volatile uint32_t dtmb_cfg_fc;
	volatile uint32_t dtmb_cfg_fd;
	volatile uint32_t dtmb_cfg_fe;
	volatile uint32_t dtmb_cfg_ff;
} dtmb_cfg_regs_t;
*/

union dtmb_cfg_00 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_00_t;

union dtmb_cfg_01 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned ctrl_sw_rst:1;
		unsigned reserved0:31;
	} b;
} dtmb_cfg_01_t;

union dtmb_cfg_02 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned testbus_addr:16;
		unsigned testbus_en:1;
		unsigned reserved0:15;
	} b;
} dtmb_cfg_02_t;

union dtmb_cfg_03 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned tb_act_width:5;
		unsigned reserved0:3;
		unsigned tb_dc_mk:3;
		unsigned reserved1:1;
		unsigned tb_capture_stop:1;
		unsigned tb_self_test:1;
		unsigned reserved2:18;
	} b;
} dtmb_cfg_03_t;

union dtmb_cfg_04 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned tb_v:32;
	} b;
} dtmb_cfg_04_t;

union dtmb_cfg_05 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned tb_addr_begin:32;
	} b;
} dtmb_cfg_05_t;

union dtmb_cfg_06 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned tb_addr_end:32;
	} b;
} dtmb_cfg_06_t;

union dtmb_cfg_07 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned ctrl_fsm_enable:1;
		unsigned ctrl_afifo_enable:1;
		unsigned ctrl_agc_enable:1;
		unsigned ctrl_ddc_enable:1;
		unsigned ctrl_dc_enable:1;
		unsigned ctrl_acf_enable:1;
		unsigned ctrl_src_enable:1;
		unsigned ctrl_dagc_enable:1;
		unsigned ctrl_sfifo_enable:1;
		unsigned ctrl_iqib_enable:1;
		unsigned ctrl_cci_enable:1;
		unsigned ctrl_fft2048_enable:1;
		unsigned ctrl_ts_enable:1;
		unsigned ctrl_corr_enable:1;
		unsigned ctrl_fe_enable:1;
		unsigned ctrl_fft512_enable:1;
		unsigned ctrl_pnphase_enable:1;
		unsigned ctrl_sfo_enable:1;
		unsigned ctrl_pm_enable:1;
		unsigned ctrl_che_enable:1;
		unsigned ctrl_fec_enable:1;
		unsigned ctrl_tps_enable:1;
		unsigned reserved0:10;
	} b;
} dtmb_cfg_07_t;

union dtmb_cfg_08 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned ctrl_src_pnphase_loop:1;
		unsigned ctrl_src_sfo_loop:1;
		unsigned ctrl_ddc_fcfo_loop:1;
		unsigned ctrl_ddc_icfo_loop:1;
		unsigned reserved0:28;
	} b;
} dtmb_cfg_08_t;

union dtmb_cfg_09 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned ctrl_fsm_state:5;
		unsigned reserved0:3;
		unsigned ctrl_fsm_v:1;
		unsigned reserved1:23;
	} b;
} dtmb_cfg_09_t;

union dtmb_cfg_0a {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned ctrl_fast_agc:1;
		unsigned ctrl_agc_bypass:1;
		unsigned ctrl_pm_hold:1;
		unsigned reserved0:29;
	} b;
} dtmb_cfg_0a_t;

union dtmb_cfg_0b {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned ctrl_ts_q:10;
		unsigned reserved0:2;
		unsigned ctrl_pnphase_q:7;
		unsigned reserved1:1;
		unsigned ctrl_sfo_q:4;
		unsigned ctrl_cfo_q:8;
	} b;
} dtmb_cfg_0b_t;

union dtmb_cfg_0c {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned ctrl_agc_to_th:8;
		unsigned ctrl_ts_to_th:4;
		unsigned ctrl_pnphase_to_th:4;
		unsigned ctrl_sfo_to_th:4;
		unsigned ctrl_fe_to_th:4;
		unsigned reserved0:8;
	} b;
} dtmb_cfg_0c_t;

union dtmb_cfg_0d {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_0d_t;

union dtmb_cfg_0e {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_0e_t;

union dtmb_cfg_0f {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_0f_t;

union dtmb_cfg_10 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned afifo_nco_rate:8;
		unsigned afifo_data_format:1;
		unsigned afifo_bypass:1;
		unsigned reserved0:22;
	} b;
} dtmb_cfg_10_t;

union dtmb_cfg_11 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned agc_target:4;
		unsigned agc_cal_intv:2;
		unsigned reserved0:2;
		unsigned agc_gain_step2:6;
		unsigned reserved1:2;
		unsigned agc_gain_step1:6;
		unsigned reserved2:2;
		unsigned agc_a_filter_coef2:3;
		unsigned reserved3:1;
		unsigned agc_a_filter_coef1:3;
		unsigned reserved4:1;
	} b;
} dtmb_cfg_11_t;

union dtmb_cfg_12 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned agc_imp_thresh:4;
		unsigned agc_imp_en:1;
		unsigned agc_iq_exchange:1;
		unsigned reserved0:2;
		unsigned agc_clip_ratio:5;
		unsigned reserved1:3;
		unsigned agc_signal_clip_thr:6;
		unsigned reserved2:2;
		unsigned agc_sd_rate:7;
		unsigned reserved3:1;
	} b;
} dtmb_cfg_12_t;

union dtmb_cfg_13 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned agc_rffb_value:11;
		unsigned reserved0:1;
		unsigned agc_iffb_value:11;
		unsigned reserved1:1;
		unsigned agc_gain_step_rf:1;
		unsigned agc_rfgain_freeze:1;
		unsigned agc_tuning_slope:1;
		unsigned agc_rffb_set:1;
		unsigned agc_gain_step_if:1;
		unsigned agc_ifgain_freeze:1;
		unsigned agc_if_only:1;
		unsigned agc_iffb_set:1;
	} b;
} dtmb_cfg_13_t;

union dtmb_cfg_14 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned agc_rffb_gain_sat_i:8;
		unsigned agc_rffb_gain_sat:8;
		unsigned agc_iffb_gain_sat_i:8;
		unsigned agc_iffb_gain_sat:8;
	} b;
} dtmb_cfg_14_t;

union dtmb_cfg_15 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned ddc_phase:15;
		unsigned reserved0:17;
	} b;
} dtmb_cfg_15_t;

union dtmb_cfg_16 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned ddc_delta_phase:25;
		unsigned reserved0:3;
		unsigned ddc_feedback_clear:1;
		unsigned reserved1:3;
	} b;
} dtmb_cfg_16_t;

union dtmb_cfg_17 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned ddc_bypass:1;
		unsigned reserved0:31;
	} b;
} dtmb_cfg_17_t;

union dtmb_cfg_18 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned dc_hold:1;
		unsigned dc_set_val:1;
		unsigned dc_alpha:2;
		unsigned reserved0:28;
	} b;
} dtmb_cfg_18_t;

union dtmb_cfg_19 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned dc_set_avg_q:16;
		unsigned dc_set_avg_i:16;
	} b;
} dtmb_cfg_19_t;

union dtmb_cfg_1a {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned coef65:11;
		unsigned reserved0:1;
		unsigned coef66:11;
		unsigned reserved1:1;
		unsigned acf_bypass:1;
		unsigned reserved2:7;
	} b;
} dtmb_cfg_1a_t;

union dtmb_cfg_1b {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned coef63:11;
		unsigned reserved0:1;
		unsigned coef64:11;
		unsigned reserved1:9;
	} b;
} dtmb_cfg_1b_t;

union dtmb_cfg_1c {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned coef62:10;
		unsigned reserved0:22;
	} b;
} dtmb_cfg_1c_t;

union dtmb_cfg_1d {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned coef60:10;
		unsigned reserved0:2;
		unsigned coef61:10;
		unsigned reserved1:10;
	} b;
} dtmb_cfg_1d_t;

union dtmb_cfg_1e {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned coef59:9;
		unsigned reserved0:23;
	} b;
} dtmb_cfg_1e_t;

union dtmb_cfg_1f {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned coef57:9;
		unsigned reserved0:3;
		unsigned coef58:9;
		unsigned reserved1:11;
	} b;
} dtmb_cfg_1f_t;

union dtmb_cfg_20 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned coef54:8;
		unsigned coef55:8;
		unsigned coef56:8;
		unsigned reserved0:8;
	} b;
} dtmb_cfg_20_t;

union dtmb_cfg_21 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned coef53:7;
		unsigned reserved0:25;
	} b;
} dtmb_cfg_21_t;

union dtmb_cfg_22 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned coef49:7;
		unsigned reserved0:1;
		unsigned coef50:7;
		unsigned reserved1:1;
		unsigned coef51:7;
		unsigned reserved2:1;
		unsigned coef52:7;
		unsigned reserved3:1;
	} b;
} dtmb_cfg_22_t;

union dtmb_cfg_23 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned coef45:7;
		unsigned reserved0:1;
		unsigned coef46:7;
		unsigned reserved1:1;
		unsigned coef47:7;
		unsigned reserved2:1;
		unsigned coef48:7;
		unsigned reserved3:1;
	} b;
} dtmb_cfg_23_t;

union dtmb_cfg_24 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned coef42:6;
		unsigned reserved0:2;
		unsigned coef43:6;
		unsigned reserved1:2;
		unsigned coef44:6;
		unsigned reserved2:10;
	} b;
} dtmb_cfg_24_t;

union dtmb_cfg_25 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned coef38:6;
		unsigned reserved0:2;
		unsigned coef39:6;
		unsigned reserved1:2;
		unsigned coef40:6;
		unsigned reserved2:2;
		unsigned coef41:6;
		unsigned reserved3:2;
	} b;
} dtmb_cfg_25_t;

union dtmb_cfg_26 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned coef34:6;
		unsigned reserved0:2;
		unsigned coef35:6;
		unsigned reserved1:2;
		unsigned coef36:6;
		unsigned reserved2:2;
		unsigned coef37:6;
		unsigned reserved3:2;
	} b;
} dtmb_cfg_26_t;

union dtmb_cfg_27 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned coef30:6;
		unsigned reserved0:2;
		unsigned coef31:6;
		unsigned reserved1:2;
		unsigned coef32:6;
		unsigned reserved2:2;
		unsigned coef33:6;
		unsigned reserved3:2;
	} b;
} dtmb_cfg_27_t;

union dtmb_cfg_28 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned coef27:5;
		unsigned reserved0:3;
		unsigned coef28:5;
		unsigned reserved1:3;
		unsigned coef29:5;
		unsigned reserved2:11;
	} b;
} dtmb_cfg_28_t;

union dtmb_cfg_29 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned coef23:5;
		unsigned reserved0:3;
		unsigned coef24:5;
		unsigned reserved1:3;
		unsigned coef25:5;
		unsigned reserved2:3;
		unsigned coef26:5;
		unsigned reserved3:3;
	} b;
} dtmb_cfg_29_t;

union dtmb_cfg_2a {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned coef19:5;
		unsigned reserved0:3;
		unsigned coef20:5;
		unsigned reserved1:3;
		unsigned coef21:5;
		unsigned reserved2:3;
		unsigned coef22:5;
		unsigned reserved3:3;
	} b;
} dtmb_cfg_2a_t;

union dtmb_cfg_2b {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned coef15:5;
		unsigned reserved0:3;
		unsigned coef16:5;
		unsigned reserved1:3;
		unsigned coef17:5;
		unsigned reserved2:3;
		unsigned coef18:5;
		unsigned reserved3:3;
	} b;
} dtmb_cfg_2b_t;

union dtmb_cfg_2c {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned coef08:4;
		unsigned coef09:4;
		unsigned coef10:4;
		unsigned coef11:4;
		unsigned coef12:4;
		unsigned coef13:4;
		unsigned coef14:4;
		unsigned reserved0:4;
	} b;
} dtmb_cfg_2c_t;

union dtmb_cfg_2d {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned coef00:4;
		unsigned coef01:4;
		unsigned coef02:4;
		unsigned coef03:4;
		unsigned coef04:4;
		unsigned coef05:4;
		unsigned coef06:4;
		unsigned coef07:4;
	} b;
} dtmb_cfg_2d_t;

union dtmb_cfg_2e {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned src_norm_inrate:23;
		unsigned reserved0:9;
	} b;
} dtmb_cfg_2e_t;

union dtmb_cfg_2f {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned src_init_phase:24;
		unsigned src_init_ini:1;
		unsigned reserved0:7;
	} b;
} dtmb_cfg_2f_t;

union dtmb_cfg_30 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned sfifo_out_len:4;
		unsigned reserved0:28;
	} b;
} dtmb_cfg_30_t;

union dtmb_cfg_31 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned dagc_target_pow_n:6;
		unsigned reserved0:2;
		unsigned dagc_target_pow_p:6;
		unsigned reserved1:2;
		unsigned dagc_gain_ctrl:8;
		unsigned dagc_bw:3;
		unsigned reserved2:1;
		unsigned dagc_hold:1;
		unsigned reserved3:3;
	} b;
} dtmb_cfg_31_t;

union dtmb_cfg_32 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned iqib_step_b:2;
		unsigned iqib_step_a:2;
		unsigned iqib_period:3;
		unsigned reserved0:25;
	} b;
} dtmb_cfg_32_t;

union dtmb_cfg_33 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned iqib_set_b:12;
		unsigned iqib_set_a:10;
		unsigned reserved0:2;
		unsigned iqib_set_val:1;
		unsigned iqib_hold:1;
		unsigned reserved1:6;
	} b;
} dtmb_cfg_33_t;

union dtmb_cfg_34 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned cci_rpsq_n:10;
		unsigned reserved0:2;
		unsigned cci_rp_n:13;
		unsigned reserved1:3;
		unsigned cci_det_en:1;
		unsigned cci_bypass:1;
		unsigned reserved2:2;
	} b;
} dtmb_cfg_34_t;

union dtmb_cfg_35 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned cci_avr_times:3;
		unsigned reserved0:1;
		unsigned cci_det_thres:3;
		unsigned reserved1:25;
	} b;
} dtmb_cfg_35_t;

union dtmb_cfg_36 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned cci_notch1_a1:10;
		unsigned reserved0:2;
		unsigned cci_notch1_en:1;
		unsigned reserved1:19;
	} b;
} dtmb_cfg_36_t;

union dtmb_cfg_37 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned cci_notch1_b1:10;
		unsigned reserved0:2;
		unsigned cci_notch1_a2:10;
		unsigned reserved1:10;
	} b;
} dtmb_cfg_37_t;

union dtmb_cfg_38 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned cci_notch2_a1:10;
		unsigned reserved0:2;
		unsigned cci_notch2_en:1;
		unsigned reserved1:3;
		unsigned cci_mpthres:16;
	} b;
} dtmb_cfg_38_t;

union dtmb_cfg_39 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned cci_notch2_b1:10;
		unsigned reserved0:2;
		unsigned cci_notch2_a2:10;
		unsigned reserved1:10;
	} b;
} dtmb_cfg_39_t;

union dtmb_cfg_3a {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned ts_gain:2;
		unsigned reserved0:2;
		unsigned ts_sat_shift:3;
		unsigned reserved1:1;
		unsigned ts_fixpn_en:1;
		unsigned ts_fixpn:2;
		unsigned reserved2:21;
	} b;
} dtmb_cfg_3a_t;

union dtmb_cfg_3b {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned fe_lock_len:4;
		unsigned fe_sat_shift:3;
		unsigned reserved0:1;
		unsigned fe_cut:4;
		unsigned reserved1:4;
		unsigned fe_modify:16;
	} b;
} dtmb_cfg_3b_t;

union dtmb_cfg_3c {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned pnphase_offset2:4;
		unsigned pnphase_offset1:4;
		unsigned pnphase_offset0:4;
		unsigned reserved0:20;
	} b;
} dtmb_cfg_3c_t;

union dtmb_cfg_3d {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned pnphase_gain:2;
		unsigned reserved0:2;
		unsigned pnphase_sat_shift:4;
		unsigned pnphase_cut:4;
		unsigned reserved1:4;
		unsigned pnphase_modify:16;
	} b;
} dtmb_cfg_3d_t;

union dtmb_cfg_3e {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned sfo_cfo_pn0_modify:16;
		unsigned sfo_sfo_pn0_modify:16;
	} b;
} dtmb_cfg_3e_t;

union dtmb_cfg_3f {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned sfo_cfo_pn1_modify:16;
		unsigned sfo_sfo_pn1_modify:16;
	} b;
} dtmb_cfg_3f_t;

union dtmb_cfg_40 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned sfo_cfo_pn2_modify:16;
		unsigned sfo_sfo_pn2_modify:16;
	} b;
} dtmb_cfg_40_t;

union dtmb_cfg_41 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned sfo_sat_shift:4;
		unsigned sfo_gain:2;
		unsigned sfo_timingoff_en:1;
		unsigned sfo_timing_offset:1;
		unsigned sfo_dist:2;
		unsigned reserved0:2;
		unsigned sfo_cfo_cut:4;
		unsigned sfo_sfo_cut:4;
		unsigned reserved1:12;
	} b;
} dtmb_cfg_41_t;

union dtmb_cfg_42 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned pm_gd_short_pst:5;
		unsigned reserved0:3;
		unsigned pm_gd_short_pre:5;
		unsigned reserved1:3;
		unsigned pm_gd_long_pst:6;
		unsigned reserved2:2;
		unsigned pm_gd_long_pre:6;
		unsigned reserved3:2;
	} b;
} dtmb_cfg_42_t;

union dtmb_cfg_43 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned pm_big_offset:5;
		unsigned reserved0:3;
		unsigned pm_small_offset:5;
		unsigned reserved1:3;
		unsigned pm_big_shift:4;
		unsigned pm_small_shift:4;
		unsigned pm_noise_gain:3;
		unsigned pm_select_gain:3;
		unsigned pm_select_ch_gain:2;
	} b;
} dtmb_cfg_43_t;

union dtmb_cfg_44 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned pm_accu_times:4;
		unsigned reserved0:28;
	} b;
} dtmb_cfg_44_t;

union dtmb_cfg_45 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned tps_run_tim_limit:10;
		unsigned reserved0:2;
		unsigned tps_suc_limit:7;
		unsigned reserved1:1;
		unsigned tps_q_th:7;
		unsigned reserved2:1;
		unsigned tps_alpha:3;
		unsigned reserved3:1;
	} b;
} dtmb_cfg_45_t;

union dtmb_cfg_46 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned tps_known:1;
		unsigned static_channel:1;
		unsigned reserved0:2;
		unsigned constell:2;
		unsigned reserved1:2;
		unsigned code_rate:2;
		unsigned intlv_mode:1;
		unsigned qam4_nr:1;
		unsigned freq_reverse:1;
		unsigned reserved2:19;
	} b;
} dtmb_cfg_46_t;

union dtmb_cfg_47 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned ldpc_it_min:8;
		unsigned ldpc_it_max:8;
		unsigned ldpc_it_auto:1;
		unsigned ldpc_it_dchk:1;
		unsigned bch_off:1;
		unsigned ts_clk_neg:1;
		unsigned ts_fast:1;
		unsigned dc_ugt:1;
		unsigned sw_reset_freq_di:1;
		unsigned sw_reset_4qam_nr:1;
		unsigned sw_reset_time_di:1;
		unsigned sw_reset_ldpc:1;
		unsigned sw_reset_bch:1;
		unsigned sw_reset_ber:1;
		unsigned tbus_cfg:3;
		unsigned fifo_base:1;
	} b;
} dtmb_cfg_47_t;

union dtmb_cfg_48 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned fec_debug_len:16;
		unsigned fec_debug_mode:1;
		unsigned fec_debug_on:1;
		unsigned fec_lock_cfg:3;
		unsigned fec_lost_cfg:3;
		unsigned bad_to_zero:1;
		unsigned fec_debug_spare:7;
	} b;
} dtmb_cfg_48_t;

union dtmb_cfg_49 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned data_ddr_adr:32;
	} b;
} dtmb_cfg_49_t;

union dtmb_cfg_4a {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned debug_ddr_adr:32;
	} b;
} dtmb_cfg_4a_t;

union dtmb_cfg_4b {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned sim_dat_b0:6;
		unsigned sim_dat_b1:6;
		unsigned sim_dat_b2:6;
		unsigned sim_dat_b3:6;
		unsigned sim_dat_b4:6;
		unsigned reserved:2;
	} b;
} dtmb_cfg_4b_t;

union dtmb_cfg_4c {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned sim_dat_b5:6;
		unsigned sim_end:3;
		unsigned sim_vld:1;
		unsigned sim_head:1;
		unsigned sim_ini:1;
		unsigned sim_mode:1;
		unsigned reserved:19;
	} b;
} dtmb_cfg_4c_t;

union dtmb_cfg_4d {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned che_eqin_f_mcci_thr:4;
		unsigned che_ch_mh_thr:12;
		unsigned che_tune_thr:4;
		unsigned che_tune_cnt_thr:12;
	} b;
} dtmb_cfg_4d_t;

union dtmb_cfg_4e {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned che_ch_noise_pow:15;
		unsigned che_ch_noise_pow_en:1;
		unsigned che_up_noise_pow:15;
		unsigned che_up_noise_pow_en:1;
	} b;
} dtmb_cfg_4e_t;

union dtmb_cfg_4f {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned che_up_mh_thr:13;
		unsigned reserved0:3;
		unsigned che_belta:5;
		unsigned che_belta_en:1;
		unsigned reserved1:2;
		unsigned che_alpha:5;
		unsigned che_alpha_en:1;
		unsigned reserved2:2;
	} b;
} dtmb_cfg_4f_t;

union dtmb_cfg_50 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned che_clk_fcy_indx:2;
		unsigned che_iter_time_en:1;
		unsigned reserved0:1;
		unsigned che_iter_time_mobile:4;
		unsigned che_iter_time_static:4;
		unsigned bp_last_itera:1;
		unsigned reserved1:19;
	} b;
} dtmb_cfg_50_t;

union dtmb_cfg_51 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned che_m_init_snr1:8;
		unsigned che_m_init_snr2:8;
		unsigned che_m_init_snr3:8;
		unsigned che_m_init_snr4:8;
	} b;
} dtmb_cfg_51_t;

union dtmb_cfg_52 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned frame_mc_header_dly:16;
		unsigned pn_mc_header_dly:16;
	} b;
} dtmb_cfg_52_t;

union dtmb_cfg_53 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned frame_sc_header_dly:16;
		unsigned pn_sc_header_dly:16;
	} b;
} dtmb_cfg_53_t;

union dtmb_cfg_54 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned seg_bypass:1;
		unsigned seg_num_1seg_log2:3;
		unsigned seg_alpha:3;
		unsigned seg_read_val:1;
		unsigned seg_read_addr:12;
		unsigned reserved:12;
	} b;
} dtmb_cfg_54_t;

union dtmb_cfg_55 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved:32;
	} b;
} dtmb_cfg_55_t;

union dtmb_cfg_56 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved:32;
	} b;
} dtmb_cfg_56_t;

union dtmb_cfg_57 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved:32;
	} b;
} dtmb_cfg_57_t;

union dtmb_cfg_58 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved:32;
	} b;
} dtmb_cfg_58_t;

union dtmb_cfg_59 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved:32;
	} b;
} dtmb_cfg_59_t;

union dtmb_cfg_5a {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved:32;
	} b;
} dtmb_cfg_5a_t;

union dtmb_cfg_5b {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved:32;
	} b;
} dtmb_cfg_5b_t;

union dtmb_cfg_5c {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved:32;
	} b;
} dtmb_cfg_5c_t;

union dtmb_cfg_5d {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved:32;
	} b;
} dtmb_cfg_5d_t;

union dtmb_cfg_5e {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved:32;
	} b;
} dtmb_cfg_5e_t;

union dtmb_cfg_5f {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved:32;
	} b;
} dtmb_cfg_5f_t;

union dtmb_cfg_60 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved:32;
	} b;
} dtmb_cfg_60_t;

union dtmb_cfg_61 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved:32;
	} b;
} dtmb_cfg_61_t;

union dtmb_cfg_62 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved:32;
	} b;
} dtmb_cfg_62_t;

union dtmb_cfg_63 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved:32;
	} b;
} dtmb_cfg_63_t;

union dtmb_cfg_64 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved:32;
	} b;
} dtmb_cfg_64_t;

union dtmb_cfg_65 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned che_alpha_portable_sc:16;
		unsigned reserved16:4;
		unsigned che_alpha_portable1_mc:12;
	} b;
} dtmb_cfg_65_t;

union dtmb_cfg_66 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned che_beta_portable0_mc:32;
	} b;
} dtmb_cfg_66_t;

union dtmb_cfg_67 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:20;
		unsigned che_beta_portable1_mc:12;
	} b;
} dtmb_cfg_67_t;

union dtmb_cfg_68 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned che_alpha_mobile0_mc:32;
	} b;
} dtmb_cfg_68_t;

union dtmb_cfg_69 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned che_alpha_mobile_sc:16;
		unsigned reserved16:4;
		unsigned che_alpha_mobile1_mc:12;
	} b;
} dtmb_cfg_69_t;

union dtmb_cfg_6a {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_6a_t;

union dtmb_cfg_6b {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_6b_t;

union dtmb_cfg_6c {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_6c_t;

union dtmb_cfg_6d {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_6d_t;

union dtmb_cfg_6e {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_6e_t;

union dtmb_cfg_6f {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_6f_t;

union dtmb_cfg_70 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_70_t;

union dtmb_cfg_71 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_71_t;

union dtmb_cfg_72 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_72_t;

union dtmb_cfg_73 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_73_t;

union dtmb_cfg_74 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_74_t;

union dtmb_cfg_75 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_75_t;

union dtmb_cfg_76 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_76_t;

union dtmb_cfg_77 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_77_t;

union dtmb_cfg_78 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_78_t;

union dtmb_cfg_79 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_79_t;

union dtmb_cfg_7a {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_7a_t;

union dtmb_cfg_7b {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_7b_t;

union dtmb_cfg_7c {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_7c_t;

union dtmb_cfg_7d {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_7d_t;

union dtmb_cfg_7e {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_7e_t;

union dtmb_cfg_7f {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_7f_t;

union dtmb_cfg_80 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_80_t;

union dtmb_cfg_81 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_81_t;

union dtmb_cfg_82 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_82_t;

union dtmb_cfg_83 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_83_t;

union dtmb_cfg_84 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_84_t;

union dtmb_cfg_85 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_85_t;

union dtmb_cfg_86 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_86_t;

union dtmb_cfg_87 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_87_t;

union dtmb_cfg_88 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_88_t;

union dtmb_cfg_89 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_89_t;

union dtmb_cfg_8a {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_8a_t;

union dtmb_cfg_8b {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_8b_t;

union dtmb_cfg_8c {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_8c_t;

union dtmb_cfg_8d {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_8d_t;

union dtmb_cfg_8e {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_8e_t;

union dtmb_cfg_8f {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_8f_t;

union dtmb_cfg_90 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_90_t;

union dtmb_cfg_91 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_91_t;

union dtmb_cfg_92 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_92_t;

union dtmb_cfg_93 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_93_t;

union dtmb_cfg_94 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_94_t;

union dtmb_cfg_95 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_95_t;

union dtmb_cfg_96 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_96_t;

union dtmb_cfg_97 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_97_t;

union dtmb_cfg_98 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_98_t;

union dtmb_cfg_99 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_99_t;

union dtmb_cfg_9a {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_9a_t;

union dtmb_cfg_9b {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_9b_t;

union dtmb_cfg_9c {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_9c_t;

union dtmb_cfg_9d {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_9d_t;

union dtmb_cfg_9e {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_9e_t;

union dtmb_cfg_9f {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_9f_t;

union dtmb_cfg_a0 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_a0_t;

union dtmb_cfg_a1 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_a1_t;

union dtmb_cfg_a2 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_a2_t;

union dtmb_cfg_a3 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_a3_t;

union dtmb_cfg_a4 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_a4_t;

union dtmb_cfg_a5 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_a5_t;

union dtmb_cfg_a6 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_a6_t;

union dtmb_cfg_a7 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_a7_t;

union dtmb_cfg_a8 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_a8_t;

union dtmb_cfg_a9 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_a9_t;

union dtmb_cfg_aa {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_aa_t;

union dtmb_cfg_ab {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_ab_t;

union dtmb_cfg_ac {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_ac_t;

union dtmb_cfg_ad {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_ad_t;

union dtmb_cfg_ae {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_ae_t;

union dtmb_cfg_af {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_af_t;

union dtmb_cfg_b0 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_b0_t;

union dtmb_cfg_b1 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_b1_t;

union dtmb_cfg_b2 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_b2_t;

union dtmb_cfg_b3 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_b3_t;

union dtmb_cfg_b4 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_b4_t;

union dtmb_cfg_b5 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_b5_t;

union dtmb_cfg_b6 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_b6_t;

union dtmb_cfg_b7 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_b7_t;

union dtmb_cfg_b8 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_b8_t;

union dtmb_cfg_b9 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_b9_t;

union dtmb_cfg_ba {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_ba_t;

union dtmb_cfg_bb {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_bb_t;

union dtmb_cfg_bc {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_bc_t;

union dtmb_cfg_bd {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_bd_t;

union dtmb_cfg_be {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_be_t;

union dtmb_cfg_bf {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_bf_t;

union dtmb_cfg_c0 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_c0_t;

union dtmb_cfg_c1 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_c1_t;

union dtmb_cfg_c2 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_c2_t;

union dtmb_cfg_c3 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_c3_t;

union dtmb_cfg_c4 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_c4_t;

union dtmb_cfg_c5 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_c5_t;

union dtmb_cfg_c6 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_c6_t;

union dtmb_cfg_c7 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_c7_t;

union dtmb_cfg_c8 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned testbus_out:32;
	} b;
} dtmb_cfg_c8_t;

union dtmb_cfg_c9 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned tbus_dc_addr:32;
	} b;
} dtmb_cfg_c9_t;

union dtmb_cfg_ca {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned front_iqib_check_b:12;
		unsigned front_iqib_check_a:10;
		unsigned reserved22:10;
	} b;
} dtmb_cfg_ca_t;

union dtmb_cfg_cb {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned sync_ts_idx:2;
		unsigned sync_ts_pos:13;
		unsigned sync_ts_q:10;
		unsigned reserved25:7;
	} b;
} dtmb_cfg_cb_t;

union dtmb_cfg_cc {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_cc_t;

union dtmb_cfg_cd {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned sync_pnphase_max_q_idx:2;
		unsigned sync_pnphase:8;
		unsigned sync_pnphase_max_q:7;
		unsigned reserved17:15;
	} b;
} dtmb_cfg_cd_t;

union dtmb_cfg_ce {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_ce_t;

union dtmb_cfg_cf {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_cf_t;

union dtmb_cfg_d0 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned sync_last_path_pos:10;
		unsigned sync_first_path_pos:10;
		unsigned sync_max_path_pos:10;
		unsigned reserved30:2;
	} b;
} dtmb_cfg_d0_t;

union dtmb_cfg_d1 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned sync_path_num:11;
		unsigned sync_timing_offset:11;
		unsigned reserved22:10;
	} b;
} dtmb_cfg_d1_t;

union dtmb_cfg_d2 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned ctrl_ddc_icfo:20;
		unsigned reserved20:12;
	} b;
} dtmb_cfg_d2_t;

union dtmb_cfg_d3 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned ctrl_src_sfo:12;
		unsigned reserved12:1;
		unsigned ctrl_ddc_fcfo:14;
		unsigned reserved27:5;
	} b;
} dtmb_cfg_d3_t;

union dtmb_cfg_d4 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned ctrl_fsm_state0:32;
	} b;
} dtmb_cfg_d4_t;

union dtmb_cfg_d5 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned ctrl_fsm_state1:32;
	} b;
} dtmb_cfg_d5_t;

union dtmb_cfg_d6 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned ctrl_fsm_state2:32;
	} b;
} dtmb_cfg_d6_t;

union dtmb_cfg_d7 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned ctrl_fsm_state3:32;
	} b;
} dtmb_cfg_d7_t;

union dtmb_cfg_d8 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned ctrl_ts2_workcnt:8;
		unsigned ctrl_pnphase_workcnt:8;
		unsigned ctrl_sfo_workcnt:8;
		unsigned sync_fe_workcnt:8;
	} b;
} dtmb_cfg_d8_t;

union dtmb_cfg_d9 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned front_agc_if_gain:11;
		unsigned front_agc_rf_gain:11;
		unsigned front_agc_power:9;
		unsigned reserved31:1;
	} b;
} dtmb_cfg_d9_t;

union dtmb_cfg_da {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned front_dagc_power:6;
		unsigned reserved6:2;
		unsigned front_dagc_gain:10;
		unsigned reserved18:14;
	} b;
} dtmb_cfg_da_t;

union dtmb_cfg_db {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned fec_time_sts:32;
	} b;
} dtmb_cfg_db_t;

union dtmb_cfg_dc {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned fec_ldpc_sts:32;
	} b;
} dtmb_cfg_dc_t;

union dtmb_cfg_dd {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned fec_ldpc_it_avg:16;
		unsigned fec_ldpc_per_rpt:13;
		unsigned reserved29:3;
	} b;
} dtmb_cfg_dd_t;

union dtmb_cfg_de {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned fec_ldpc_unc_acc:32;
	} b;
} dtmb_cfg_de_t;

union dtmb_cfg_df {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned fec_bch_acc:32;
	} b;
} dtmb_cfg_df_t;

union dtmb_cfg_e0 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned ctrl_icfo_all:20;
		unsigned reserved20:12;
	} b;
} dtmb_cfg_e0_t;

union dtmb_cfg_e1 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned ctrl_fcfo_all:20;
		unsigned reserved20:12;
	} b;
} dtmb_cfg_e1_t;

union dtmb_cfg_e2 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned ctrl_sfo_all:20;
		unsigned reserved20:12;
	} b;
} dtmb_cfg_e2_t;

union dtmb_cfg_e3 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned che_snr:12;
		unsigned fec_lock:1;
		unsigned che_snr_average:12;
		unsigned reserved25:7;
	} b;
} dtmb_cfg_e3_t;

union dtmb_cfg_e4 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned che_seg_factor:14;
		unsigned reserved14:18;
	} b;
} dtmb_cfg_e4_t;

union dtmb_cfg_e5 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned ctrl_che_workcnt:8;
		unsigned ctrl_fec_workcnt:8;
		unsigned ctrl_constell:2;
		unsigned ctrl_code_rate:2;
		unsigned ctrl_intlv_mode:1;
		unsigned ctrl_qam4_nr:1;
		unsigned ctrl_freq_reverse:1;
		unsigned reserved23:9;
	} b;
} dtmb_cfg_e5_t;

union dtmb_cfg_e6 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned che_obs_state1:32;
	} b;
} dtmb_cfg_e6_t;

union dtmb_cfg_e7 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned che_obs_state2:32;
	} b;
} dtmb_cfg_e7_t;

union dtmb_cfg_e8 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned che_obs_state3:32;
	} b;
} dtmb_cfg_e8_t;

union dtmb_cfg_e9 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned che_obs_state4:32;
	} b;
} dtmb_cfg_e9_t;

union dtmb_cfg_ea {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned che_obs_state5:32;
	} b;
} dtmb_cfg_ea_t;

union dtmb_cfg_eb {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned sync_pm_target0:24;
		unsigned reserved24:8;
	} b;
} dtmb_cfg_eb_t;

union dtmb_cfg_ec {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned sync_pm_target1:24;
		unsigned reserved24:8;
	} b;
} dtmb_cfg_ec_t;

union dtmb_cfg_ed {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned sync_pm_target2:24;
		unsigned sync_pm_gain_delta:2;
		unsigned reserved26:6;
	} b;
} dtmb_cfg_ed_t;

union dtmb_cfg_ee {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned front_cci_nf1_b1:10;
		unsigned front_cci_nf1_a2:10;
		unsigned front_cci_nf1_a1:10;
		unsigned reserved30:2;
	} b;
} dtmb_cfg_ee_t;

union dtmb_cfg_ef {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned front_cci_nf2_b1:10;
		unsigned front_cci_nf2_a2:10;
		unsigned front_cci_nf2_a1:10;
		unsigned reserved30:2;
	} b;
} dtmb_cfg_ef_t;

union dtmb_cfg_f0 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned front_cci_nf2_position:11;
		unsigned front_cci_nf1_position:11;
		unsigned front_cci_nf2_det:1;
		unsigned front_cci_nf1_det:1;
		unsigned reserved24:8;
	} b;
} dtmb_cfg_f0_t;

union dtmb_cfg_f1 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned ctrl_sys_ofdm_cnt:8;
		unsigned mobi_det_power_var:19;
		unsigned reserved27:1;
		unsigned ctrl_che_working_state:2;
		unsigned reserved30:2;
	} b;
} dtmb_cfg_f1_t;

union dtmb_cfg_f2 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned ctrl_tps_q_final:7;
		unsigned ctrl_tps_suc_cnt:7;
		unsigned reserved14:18;
	} b;
} dtmb_cfg_f2_t;

union dtmb_cfg_f3 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned front_dc_q:10;
		unsigned front_dc_i:10;
		unsigned reserved20:12;
	} b;
} dtmb_cfg_f3_t;

union dtmb_cfg_f4 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned clk_cnt_for_frame_min:32;
	} b;
} dtmb_cfg_f4_t;

union dtmb_cfg_f5 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned clk_cnt_for_frame_max:32;
	} b;
} dtmb_cfg_f5_t;

union dtmb_cfg_f6 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned che_debug:32;
	} b;
} dtmb_cfg_f6_t;

union dtmb_cfg_f7 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned corr_start_min:32;
	} b;
} dtmb_cfg_f7_t;

union dtmb_cfg_f8 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned corr_start_max:32;
	} b;
} dtmb_cfg_f8_t;

union dtmb_cfg_f9 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned che_start_min:32;
	} b;
} dtmb_cfg_f9_t;

union dtmb_cfg_fa {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned che_start_max:32;
	} b;
} dtmb_cfg_fa_t;

union dtmb_cfg_fb {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned corr_start_cnt:16;
		unsigned che_start_cnt:16;
	} b;
} dtmb_cfg_fb_t;

union dtmb_cfg_fc {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_fc_t;

union dtmb_cfg_fd {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved0:32;
	} b;
} dtmb_cfg_fd_t;

union dtmb_cfg_fe {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned sync_pm_prt_gd:32;
	} b;
} dtmb_cfg_fe_t;

union dtmb_cfg_ff {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned ctrl_dead_lock_det:1;
		unsigned ctrl_dead_lock:1;
		unsigned reserved2:2;
		unsigned ctrl_dead_cnt:4;
		unsigned reserved8:24;
	} b;
} dtmb_cfg_ff_t;

/* dvb-c -------------------------------------------------------------------*/

/*
  struct dvbc_cfg_regs {
	volatile uint32_t dvbc_cfg_00;
	volatile uint32_t dvbc_cfg_04;
	volatile uint32_t dvbc_cfg_08;
	volatile uint32_t dvbc_cfg_0c;
	volatile uint32_t dvbc_cfg_10;
	volatile uint32_t dvbc_cfg_14;
	volatile uint32_t dvbc_cfg_18;
	volatile uint32_t dvbc_cfg_1c;
	volatile uint32_t dvbc_cfg_20;
	volatile uint32_t dvbc_cfg_24;
	volatile uint32_t dvbc_cfg_28;
	volatile uint32_t dvbc_cfg_2c;
	volatile uint32_t dvbc_cfg_30;
	volatile uint32_t dvbc_cfg_34;
	volatile uint32_t dvbc_cfg_38;
	volatile uint32_t dvbc_cfg_3c;
	volatile uint32_t dvbc_cfg_40;
	volatile uint32_t dvbc_cfg_44;
	volatile uint32_t dvbc_cfg_48;
	volatile uint32_t dvbc_cfg_4c;
	volatile uint32_t dvbc_cfg_50;
	volatile uint32_t dvbc_cfg_54;
	volatile uint32_t dvbc_cfg_58;
	volatile uint32_t dvbc_cfg_5c;
	volatile uint32_t dvbc_cfg_60;
	volatile uint32_t dvbc_cfg_64;
	volatile uint32_t dvbc_cfg_68;
	volatile uint32_t dvbc_cfg_6c;
	volatile uint32_t dvbc_cfg_70;
	volatile uint32_t dvbc_cfg_74;
	volatile uint32_t dvbc_cfg_78;
	volatile uint32_t dvbc_cfg_7c;
	volatile uint32_t dvbc_cfg_80;
	volatile uint32_t dvbc_cfg_84;
	volatile uint32_t dvbc_cfg_88;
	volatile uint32_t dvbc_cfg_8c;
	volatile uint32_t dvbc_cfg_90;
	volatile uint32_t dvbc_cfg_94;
	volatile uint32_t dvbc_cfg_98;
	volatile uint32_t dvbc_cfg_9c;
	volatile uint32_t dvbc_cfg_a0;
	volatile uint32_t dvbc_cfg_a4;
	volatile uint32_t dvbc_cfg_a8;
	volatile uint32_t dvbc_cfg_ac;
	volatile uint32_t dvbc_cfg_b0;
	volatile uint32_t dvbc_cfg_b4;
	volatile uint32_t dvbc_cfg_b8;
	volatile uint32_t dvbc_cfg_bc;
	volatile uint32_t dvbc_cfg_c0;
	volatile uint32_t dvbc_cfg_c4;
	volatile uint32_t dvbc_cfg_c8;
	volatile uint32_t dvbc_cfg_cc;
	volatile uint32_t dvbc_cfg_d0;
	volatile uint32_t dvbc_cfg_d4;
	volatile uint32_t dvbc_cfg_d8;
	volatile uint32_t dvbc_cfg_dc;
	volatile uint32_t dvbc_cfg_e0;
	volatile uint32_t dvbc_cfg_e4;
	volatile uint32_t dvbc_cfg_e8;
	volatile uint32_t dvbc_cfg_ec;
} dvbc_cfg_regs_t;
*/
union dvbc_cfg_00 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved:32;
	} b;
} dvbc_cfg_00_t;

union dvbc_cfg_04 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned sw_qam_enable:1;
		unsigned qam_imq_cfg:1;
		unsigned reserved0:1;
		unsigned nyq_bypass_cfg:1;
		unsigned fsm_en:1;
		unsigned fast_agc:1;
		unsigned reserved1:2;
		unsigned dc_enable:1;
		unsigned dc_alpha:3;
		unsigned not_used:20;
	} b;
} dvbc_cfg_04_t;

union dvbc_cfg_08 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned qam_mode_cfg:3;
		unsigned qam_test_en:1;
		unsigned qam_test_addr:5;
		unsigned reserved:7;
		unsigned hcap_en:1;
		unsigned dvbc_topstate_ct1:3;
		unsigned fsm_state_d:3;
		unsigned fsm_state_v:1;
		unsigned not_used:8;
	} b;
} dvbc_cfg_08_t;

union dvbc_cfg_0c {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned symb_cnt_cfg:16;
		unsigned adc_cnt_cfg:16;
	} b;
} dvbc_cfg_0c_t;

union dvbc_cfg_10 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned rs_cnt_cfg:16;
		unsigned afifo_nco_rate:8;
		unsigned afifo_bypass:1;
		unsigned not_used:7;
	} b;
} dvbc_cfg_10_t;

union dvbc_cfg_14 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned eq_snr:12;
		unsigned ber_before_rs:20;
	} b;
} dvbc_cfg_14_t;

union dvbc_cfg_18 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned tst_sync:7;
		unsigned not_used:9;
		unsigned per_rs:16;
	} b;
} dvbc_cfg_18_t;

union dvbc_cfg_1c {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned eq_dov:1;
		unsigned eq_doq:12;
		unsigned eq_doi:12;
		unsigned not_used:7;
	} b;
} dvbc_cfg_1c_t;

union dvbc_cfg_20 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned phs_reset_cfg:1;
		unsigned tim_mu2_cfg_accurate:5;
		unsigned tim_mu1_cfg_accurate:5;
		unsigned tim_sync_cfg_accurate:4;
		unsigned tim_trk_cfg_accurate:4;
		unsigned tim_shr_cfg_accurate:4;
		unsigned sw_tim_select:1;
		unsigned phs_mu:4;
		unsigned phs_track_enable:1;
		unsigned phs_track_eqin_enable:1;
		unsigned phs_track_in_smma:1;
		unsigned not_used:1;
	} b;
} dvbc_cfg_20_t;

union dvbc_cfg_24 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned phs_offset_cfg:23;
		unsigned tim_mu1_min:5;
		unsigned tim_mu2_min:4;
	} b;
} dvbc_cfg_24_t;

union dvbc_cfg_28 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned phs_offset_act:23;
		unsigned not_used:9;
	} b;
} dvbc_cfg_28_t;

union dvbc_cfg_2c {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned eq_cfg_max_frq_off:30;
		unsigned not_used:2;
	} b;
} dvbc_cfg_2c_t;

union dvbc_cfg_30 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned sw_tim_sweep_onoff_cfg:1;
		unsigned tim_sweep_speed_cfg:5;
		unsigned tim_mu2_cfg_coarse:5;
		unsigned tim_mu1_cfg_coarse:5;
		unsigned tim_sync_cfg_coarse:4;
		unsigned tim_trk_cfg_coarse:4;
		unsigned tim_shr_cfg_coarse:4;
		unsigned tim_reset_cfg:1;
		unsigned hw_fsm_ctrl:3;
	} b;
} dvbc_cfg_30_t;

union dvbc_cfg_34 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned sw_symbol_rate:16;
		unsigned sampling_rate:16;
	} b;
} dvbc_cfg_34_t;

union dvbc_cfg_38 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned tim_sweep_range_cfg:24;
		unsigned not_used:8;
	} b;
} dvbc_cfg_38_t;

union dvbc_cfg_3c {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned q_uneven_report:32;
	} b;
} dvbc_cfg_3c_t;

union dvbc_cfg_40 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned sync_trk_cfg:4;
		unsigned sync_acq_cfg:4;
		unsigned sync_reset_cfg:1;
		unsigned hw_symbol_rate_step:7;
		unsigned not_used:16;
	} b;
} dvbc_cfg_40_t;

union dvbc_cfg_44 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned ts_ctrl_cfg:4;
		unsigned ts_serial_cfg:1;
		unsigned reserved:3;
		unsigned hw_symbol_rate_max:16;
		unsigned not_used:8;
	} b;
} dvbc_cfg_44_t;

union dvbc_cfg_48 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned ted_sel_cfg:3;
		unsigned reserved:5;
		unsigned hw_symbol_rate_min:16;
		unsigned not_used:8;
	} b;
} dvbc_cfg_48_t;

union dvbc_cfg_4c {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned eq_miu_test:8;
		unsigned eq_q_uneven_cfg:8;
		unsigned eq_extra_tag_conf:2;
		unsigned not_used:14;
	} b;
} dvbc_cfg_4c_t;

union dvbc_cfg_50 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned eq_cfg_seglen:3;
		unsigned sw_eq_enable:1;
		unsigned ted_disable:1;
		unsigned hw_eq_dfe_disable:1;
		unsigned reserved0:2;
		unsigned eq_cfg_cr_shift_time:4;
		unsigned input_state:12;
		unsigned input_state_en:1;
		unsigned reserved1:3;
		unsigned sw_eq_smma_reset_ctrl:1;
		unsigned reserved2:1;
		unsigned not_used:2;
	} b;
} dvbc_cfg_50_t;

union dvbc_cfg_54 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned eq_cfg_dfemiu2:4;
		unsigned eq_cfg_dfemiu1:4;
		unsigned eq_cfg_dfemiu0:4;
		unsigned reserved:4;
		unsigned eq_cfg_ffemiu2:4;
		unsigned eq_cfg_ffemiu1:4;
		unsigned eq_cfg_ffemiu0:4;
		unsigned eq_cfg_firbeta1:2;
		unsigned eq_cfg_firbeta0:2;
	} b;
} dvbc_cfg_54_t;

union dvbc_cfg_58 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned eq_cfg_initpos1:6;
		unsigned reserved0:2;
		unsigned eq_cfg_initpos0:6;
		unsigned reserved1:2;
		unsigned eq_cfg_phstr_lp1:8;
		unsigned eq_cfg_phstr_lp0:8;
	} b;
} dvbc_cfg_58_t;

union dvbc_cfg_5c {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned eq_cfg_initvalq0:10;
		unsigned reserved:6;
		unsigned eq_cfg_initvali0:10;
		unsigned not_used:6;
	} b;
} dvbc_cfg_5c_t;

union dvbc_cfg_60 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned eq_cfg_initvalq1:10;
		unsigned reserved:6;
		unsigned eq_cfg_initvali1:10;
		unsigned not_used:6;
	} b;
} dvbc_cfg_60_t;

union dvbc_cfg_64 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned eq_cfg_crth3tim:6;
		unsigned reserved0:2;
		unsigned eq_cfg_crth2tim:6;
		unsigned reserved1:2;
		unsigned eq_cfg_crth1:6;
		unsigned reserved2:2;
		unsigned eq_cfg_crth0:6;
		unsigned not_used:2;
	} b;
} dvbc_cfg_64_t;

union dvbc_cfg_68 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned eq_cfg_crth3snr:12;
		unsigned reserved:4;
		unsigned eq_cfg_crth2snr:12;
		unsigned not_used:4;
	} b;
} dvbc_cfg_68_t;

union dvbc_cfg_6c {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned eq_cfg_crppmth2:6;
		unsigned reserved0:2;
		unsigned eq_cfg_crppmth1:7;
		unsigned reserved1:1;
		unsigned eq_cfg_crppmth0:5;
		unsigned eq_cr_amp_th:11;
	} b;
} dvbc_cfg_6c_t;

union dvbc_cfg_70 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned eq_cfg_crlpk2:2;
		unsigned reserved:6;
		unsigned eq_cfg_crlpk1:8;
		unsigned eq_cfg_crlpk0_s:8;
		unsigned eq_cfg_crlpk0:8;
	} b;
} dvbc_cfg_70_t;

union dvbc_cfg_74 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned eq_cfg_mma2lms:12;
		unsigned reserved:4;
		unsigned eq_cfg_ddlms:12;
		unsigned not_used:4;
	} b;
} dvbc_cfg_74_t;

union dvbc_cfg_78 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned adc_in:10;
		unsigned reserved:22;
	} b;
} dvbc_cfg_78_t;

union dvbc_cfg_7c {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned eq_cfg_mma:12;
		unsigned eq_cfg_norm:12;
		unsigned not_used:8;
	} b;
} dvbc_cfg_7c_t;

union dvbc_cfg_80 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned eq_cfg_smma1:12;
		unsigned eq_cfg_smma0:12;
		unsigned not_used:8;
	} b;
} dvbc_cfg_80_t;

union dvbc_cfg_84 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned eq_cfg_smma3:12;
		unsigned eq_cfg_smma2:12;
		unsigned not_used:8;
	} b;
} dvbc_cfg_84_t;

union dvbc_cfg_88 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned eq_cfg_smma5:12;
		unsigned eq_cfg_smma4:12;
		unsigned not_used:8;
	} b;
} dvbc_cfg_88_t;

union dvbc_cfg_8c {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned eq_cfg_smma7:12;
		unsigned eq_cfg_smma6:12;
		unsigned not_used:8;
	} b;
} dvbc_cfg_8c_t;

union dvbc_cfg_90 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned eq_offset_cfg:11;
		unsigned agc_gain_step1:6;
		unsigned agc_gain_step2:6;
		unsigned agc_a_filter_coef1:3;
		unsigned agc_a_filter_coef2:3;
		unsigned not_used:3;
	} b;
} dvbc_cfg_90_t;

union dvbc_cfg_94 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned agc_target:4;
		unsigned agc_sd_rate:3;
		unsigned agc_cal_intv:2;
		unsigned agc_gain_step_if:1;
		unsigned agc_ifgain_freeze:1;
		unsigned agc_if_only:1;
		unsigned agc_iffb_set:1;
		unsigned agc_rfgain_freeze:1;
		unsigned agc_tuning_slope:1;
		unsigned agc_rffb_set:1;
		unsigned iffb_gain_sat_i:8;
		unsigned iffb_gain_sat:8;
	} b;
} dvbc_cfg_94_t;

union dvbc_cfg_98 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned agc_rffb_value:11;
		unsigned agc_iffb_value:11;
		unsigned rffb_gain_sat:8;
		unsigned agc_gain_step_rf:1;
		unsigned dagc_pow_det:1;
	} b;
} dvbc_cfg_98_t;

union dvbc_cfg_9c {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned agc_avg_power:9;
		unsigned agc_rffb_gain:11;
		unsigned agc_iffb_gain:11;
		unsigned not_used:1;
	} b;
} dvbc_cfg_9c_t;

union dvbc_cfg_a0 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned dagc_gain_ctrl:7;
		unsigned dagc_hold:1;
		unsigned target_pow_p:6;
		unsigned dagc_bw:3;
		unsigned dagc_rstn:1;
		unsigned rffb_gain_sat_i:8;
		unsigned sw_agc_enable:1;
		unsigned adc_format:1;
		unsigned agc_da_sw:1;
		unsigned agc_gain_rate:3;
	} b;
} dvbc_cfg_a0_t;

union dvbc_cfg_a4 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned dagc_gain_cnt:7;
		unsigned dagc_state:1;
		unsigned dagc_avg_pow:9;
		unsigned agc_stable:1;
		unsigned agc_in_target:1;
		unsigned reserved0:1;
		unsigned eq_state:4;
		unsigned not_used:8;
	} b;
} dvbc_cfg_a4_t;

union dvbc_cfg_a8 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned cci1_b1:10;
		unsigned cci1_a2:10;
		unsigned cci1_a1:10;
		unsigned reserved:1;
		unsigned cci1_enable:1;
	} b;
} dvbc_cfg_a8_t;

union dvbc_cfg_ac {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned cci2_b1:10;
		unsigned cci2_a2:10;
		unsigned cci2_a1:10;
		unsigned reserved:1;
		unsigned cci2_enable:1;
	} b;
} dvbc_cfg_ac_t;

union dvbc_cfg_b0 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned test:32;
	} b;
} dvbc_cfg_b0_t;

union dvbc_cfg_b4 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned eq_cr_angle:30;
		unsigned not_used:2;
	} b;
} dvbc_cfg_b4_t;

union dvbc_cfg_b8 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned tim_acc_pc0:32;
	} b;
} dvbc_cfg_b8_t;

union dvbc_cfg_bc {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned reserved:32;
	} b;
} dvbc_cfg_bc_t;

union dvbc_cfg_c0 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned fec_lost_th:4;
		unsigned eq_mma_th:4;
		unsigned search_symbol_rate_th:4;
		unsigned agc_stable_th:4;
		unsigned fine_symbol_rate_th:4;
		unsigned eq_smma_th:4;
		unsigned fec_lost_th_smma:4;
		unsigned reserved:4;
	} b;
} dvbc_cfg_c0_t;

union dvbc_cfg_c4 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned state_probe:32;
	} b;
} dvbc_cfg_c4_t;

union dvbc_cfg_c8 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned hw_symbol_rate:16;
		unsigned sync_fail:1;
		unsigned dc_offset:10;
		unsigned reserved:5;
	} b;
} dvbc_cfg_c8_t;

union dvbc_cfg_cc {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned err_occur_cnt:16;
		unsigned fec_eq_mma_cnt:8;
		unsigned reserved:8;
	} b;
} dvbc_cfg_cc_t;

union dvbc_cfg_d0 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned int_mask:32;
	} b;
} dvbc_cfg_d0_t;

union dvbc_cfg_d4 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned int_status:32;
	} b;
} dvbc_cfg_d4_t;

union dvbc_cfg_d8 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned eq_coefq_max:16;
		unsigned eq_coefi_max:16;
	} b;
} dvbc_cfg_d8_t;

union dvbc_cfg_dc {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned sum_phase_err_report:32;
	} b;
} dvbc_cfg_dc_t;

union dvbc_cfg_e0 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned eq_angle:30;
		unsigned reserved:2;
	} b;
} dvbc_cfg_e0_t;

union dvbc_cfg_e4 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned segm_cnt:16;
		unsigned symb_cnt_eq:16;
	} b;
} dvbc_cfg_e4_t;

union dvbc_cfg_e8 {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned sw_eq_smma_reset_n:1;
		unsigned err_occur_rst:1;
		unsigned demod_enable:1;
		unsigned soft_trigger_h9:9;
	} b;
} dvbc_cfg_e8_t;

union dvbc_cfg_ec {
    /** raw register data */
	uint32_t d32;
    /** register bits */
	struct {
		unsigned eq_restore_angle:30;
		unsigned reserved:2;
	} b;
} dvbc_cfg_ec_t;

struct atsc_cfg {
	int adr;
	int dat;
	int rw;
} atsc_cfg_t;

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
} dtmb_cfg_t;

void demod_reset(void);
void demod_set_irq_mask(void);
void demod_clr_irq_stat(void);
void demod_set_adc_core_clk(int, int, int);
void demod_set_adc_core_clk_fix(int clk_adc, int clk_dem);
void calculate_cordic_para(void);
void ofdm_read_all_regs(void);
void demod_set_adc_core_clk_quick(int clk_adc_cfg, int clk_dem_cfg);

extern int memstart;
extern long *mem_buf;
#endif
