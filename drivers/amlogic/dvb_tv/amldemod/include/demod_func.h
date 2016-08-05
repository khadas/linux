#ifdef DEMOD_FUNC_H
#else
#define DEMOD_FUNC_H

#include <linux/types.h>
/* #include <mach/am_regs.h> */
/*#include <mach/register.h>
 * #include <mach/avosstyle_io.h>
 #include <mach/io.h>*/
#include <linux/dvb/aml_demod.h>
#include "aml_fe.h"
#include "amlfrontend.h"
#include "addr_dtmb_top.h"
#include "c_stb_define.h"
#include "c_stb_regs_define.h"
#include <linux/io.h>

/* #define G9_TV */
#define GX_TV
#define safe_addr

#define PWR_ON    1
#define PWR_OFF   0

#define dtmb_mobile_mode


/* void __iomem *meson_reg_demod_map[1024]; */

#define IO_CBUS_PHY_BASE        (0xc0800000)

#ifdef safe_addr
#define IO_DEMOD_BASE           (0xc8844000)
#define IO_AOBUS_BASE           (0xc8100000)
#define IO_HIU_BASE                     (0xc883c000)
#else
#define IO_DEMOD_BASE           (0xda844000)
#define IO_AOBUS_BASE           (0xda100000)
#define IO_HIU_BASE                     (0xda83c000)
#endif

#define DEMOD_REG_OFFSET(reg)           (reg & 0xfffff)
#define DEMOD_REG_ADDR(reg)             (IO_DEMOD_BASE + DEMOD_REG_OFFSET(reg))

#define DEMOD_CBUS_REG_OFFSET(reg)              (reg << 2)
#define DEMOD_CBUS_REG_ADDR(reg)                (IO_CBUS_PHY_BASE + \
						 DEMOD_CBUS_REG_OFFSET(reg))

#define DEMOD_AOBUS_REG_OFFSET(reg)             ((reg))
#define DEMOD_AOBUS_REG_ADDR(reg)               (IO_AOBUS_BASE + \
						 DEMOD_AOBUS_REG_OFFSET(reg))

/* #define DEMOD_BASE     APB_REG_ADDR(0x20000) */
#define DEMOD_BASE DEMOD_REG_ADDR(0x0)	/* 0xc8020000 */

/* #define DEMOD_BASE 0xc8020000 */
#define DTMB_BASE  (DEMOD_BASE + 0x000)
#define DVBT_BASE  (DEMOD_BASE + 0x000)
#define ISDBT_BASE (DEMOD_BASE + 0x000)
#define QAM_BASE   (DEMOD_BASE + 0x400)
#define ATSC_BASE  (DEMOD_BASE + 0x800)
#define DEMOD_CFG_BASE  (DEMOD_BASE + 0xC00)

/* #ifdef TXL_TV */
#define TXLTV_ADC_RESET_VALUE          0xca6a2110	/* 0xce7a2110 */
#define TXLTV_ADC_REG1_VALUE           0x5d414260
#define TXLTV_ADC_REG2_VALUE           0x5ba00384	/* 0x34e0bf81 */
#define TXLTV_ADC_REG2_VALUE_CRY       0x34e0bf81
#define TXLTV_ADC_REG3_VALUE           0x4a6a2110	/* 0x4e7a2110 */
#define TXLTV_ADC_REG4_VALUE           0x02913004
#define TXLTV_ADC_REG4_CRY_VALUE 0x301
#define TXLTV_ADC_REG7_VALUE           0x00102038
#define TXLTV_ADC_REG8_VALUE           0x00000406
#define TXLTV_ADC_REG9_VALUE           0x00082183
#define TXLTV_ADC_REGA_VALUE           0x80480240
#define TXLTV_ADC_REGB_VALUE           0x22000442
#define TXLTV_ADC_REGC_VALUE           0x00034a00
#define TXLTV_ADC_REGD_VALUE           0x00005000
#define TXLTV_ADC_REGE_VALUE           0x00000200


/* DADC DPLL */
#define ADC_REG1         (IO_HIU_BASE + (0xaa << 2))
#define ADC_REG2         (IO_HIU_BASE + (0xab << 2))
#define ADC_REG3         (IO_HIU_BASE + (0xac << 2))
#define ADC_REG4         (IO_HIU_BASE + (0xad << 2))

#define ADC_REG5         (IO_HIU_BASE + (0x73 << 2))
#define ADC_REG6         (IO_HIU_BASE + (0x74 << 2))

#define ADC_REGB         (IO_HIU_BASE + (0xaf << 2))
#define ADC_REGC         (IO_HIU_BASE + (0x9e << 2))
#define ADC_REGD         (IO_HIU_BASE + (0x9f << 2))

/* DADC REG */
#define ADC_REG7         (IO_HIU_BASE + (0x27 << 2))
#define ADC_REG8         (IO_HIU_BASE + (0x28 << 2))
#define ADC_REG9         (IO_HIU_BASE + (0x2a << 2))
#define ADC_REGA         (IO_HIU_BASE + (0x2b << 2))
#define ADC_REGE         (IO_HIU_BASE + (0xbd << 2))

/* #endif  */


/* #ifdef GX_TV */

#define ADC_RESET_VALUE          0x8a2a2110	/* 0xce7a2110 */
#define ADC_REG1_VALUE           0x00100228
#define ADC_REG2_VALUE           0x34e0bf80	/* 0x34e0bf81 */
#define ADC_REG2_VALUE_CRY       0x34e0bf81
#define ADC_REG3_VALUE           0x0a2a2110	/* 0x4e7a2110 */
#define ADC_REG4_VALUE           0x02933800
#define ADC_REG4_CRY_VALUE 0x301
#define ADC_REG7_VALUE           0x01411036
#define ADC_REG8_VALUE           0x00000000
#define ADC_REG9_VALUE           0x00430036
#define ADC_REGA_VALUE           0x80480240
#if 0
/* DADC DPLL */
#define ADC_REG1         (IO_HIU_BASE + (0xaa << 2))
#define ADC_REG2         (IO_HIU_BASE + (0xab << 2))
#define ADC_REG3         (IO_HIU_BASE + (0xac << 2))
#define ADC_REG4         (IO_HIU_BASE + (0xad << 2))

#define ADC_REG5         (IO_HIU_BASE + (0x73 << 2))
#define ADC_REG6         (IO_HIU_BASE + (0x74 << 2))

/* DADC REG */
#define ADC_REG7         (IO_HIU_BASE + (0x27 << 2))
#define ADC_REG8         (IO_HIU_BASE + (0x28 << 2))
#define ADC_REG9         (IO_HIU_BASE + (0x2a << 2))
#define ADC_REGA         (IO_HIU_BASE + (0x2b << 2))
#endif
/* #endif */

#ifdef G9_TV

#define ADC_RESET_VALUE          0x8a2a2110	/* 0xce7a2110 */
#define ADC_REG1_VALUE           0x00100228
#define ADC_REG2_VALUE           0x34e0bf80	/* 0x34e0bf81 */
#define ADC_REG2_VALUE_CRY       0x34e0bf81
#define ADC_REG3_VALUE           0x0a2a2110	/* 0x4e7a2110 */
#define ADC_REG4_VALUE           0x02933800
#define ADC_REG4_CRY_VALUE 0x301
#define ADC_REG7_VALUE           0x01411036
#define ADC_REG8_VALUE           0x00000000
#define ADC_REG9_VALUE           0x00430036
#define ADC_REGA_VALUE           0x80480240

/* DADC DPLL */
#define ADC_REG1         0x10aa
#define ADC_REG2         0x10ab
#define ADC_REG3         0x10ac
#define ADC_REG4         0x10ad

#define ADC_REG5         0x1073
#define ADC_REG6         0x1074

/* DADC REG */
#define ADC_REG7         0x1027
#define ADC_REG8         0x1028
#define ADC_REG9         0x102a
#define ADC_REGA         0x102b
#endif

#ifdef M6_TV
#define ADC_REG1_VALUE           0x003b0232
#define ADC_REG2_VALUE           0x814d3928
#define ADC_REG3_VALUE           0x6b425012
#define ADC_REG4_VALUE           0x101
#define ADC_REG4_CRY_VALUE 0x301
#define ADC_REG5_VALUE           0x70b
#define ADC_REG6_VALUE           0x713

#define ADC_REG1         0x10aa
#define ADC_REG2         0x10ab
#define ADC_REG3         0x10ac
#define ADC_REG4         0x10ad
#define ADC_REG5         0x1073
#define ADC_REG6         0x1074
#endif

#define DEMOD_REG1_VALUE                 0x0000d007
#define DEMOD_REG2_VALUE                 0x2e805400
#define DEMOD_REG3_VALUE                 0x201

#define DEMOD_REG1               (DEMOD_BASE + 0xc00)
#define DEMOD_REG2               (DEMOD_BASE + 0xc04)
#define DEMOD_REG3               (DEMOD_BASE + 0xc08)
#define DEMOD_REG4               (DEMOD_BASE + 0xc0c)

/* #define Wr(addr, data)   WRITE_CBUS_REG(addr, data)*/
/* #define Rd(addr)             READ_CBUS_REG(addr)  */

/*#define Wr(addr, data) *(volatile unsigned long *)(addr) = (data)*/
/*#define Rd(addr) *(volatile unsigned long *)(addr)*/

enum {
	enable_mobile,
	disable_mobile
};

enum {
	OPEN_TIME_EQ,
	CLOSE_TIME_EQ
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

/* i2c functions */
/* int aml_i2c_sw_test_bus(struct aml_demod_i2c *adap, char *name); */
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
int dtmb_get_power_strength(int agc_gain);


int tuner_set_ch(struct aml_demod_sta *demod_sta,
		 struct aml_demod_i2c *demod_i2c);

/* dvbt */
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

/* dvbc */

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

/* atsc */

int atsc_set_ch(struct aml_demod_sta *demod_sta,
		struct aml_demod_i2c *demod_i2c,
		struct aml_demod_atsc *demod_atsc);
int check_atsc_fsm_status(void);

void atsc_write_reg(int reg_addr, int reg_data);

unsigned long atsc_read_reg(int reg_addr);

unsigned long atsc_read_iqr_reg(void);

int atsc_qam_set(fe_modulation_t mode);

void qam_initial(int qam_id);

/* dtmb */

int dtmb_set_ch(struct aml_demod_sta *demod_sta,
		struct aml_demod_i2c *demod_i2c,
		struct aml_demod_dtmb *demod_atsc);

void dtmb_reset(void);

int dtmb_check_status_gxtv(struct dvb_frontend *fe);
int dtmb_check_status_txl(struct dvb_frontend *fe);


void dtmb_write_reg(int reg_addr, int reg_data);
int dtmb_read_reg(int reg_addr);
void dtmb_register_reset(void);

/* demod functions */
unsigned long apb_read_reg_collect(unsigned long addr);
void apb_write_reg_collect(unsigned int addr, unsigned int data);
void apb_write_reg(unsigned int reg, unsigned int val);
unsigned long apb_read_reg_high(unsigned long addr);
unsigned long apb_read_reg(unsigned long reg);
int app_apb_write_reg(int addr, int data);
int app_apb_read_reg(int addr);

void demod_set_cbus_reg(unsigned data, unsigned addr);
unsigned demod_read_cbus_reg(unsigned addr);
void demod_set_demod_reg(unsigned data, unsigned addr);
unsigned demod_read_demod_reg(unsigned addr);

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

void monitor_isdbt(void);
void demod_set_reg(struct aml_demod_reg *demod_reg);
void demod_get_reg(struct aml_demod_reg *demod_reg);

/* void demod_calc_clk(struct aml_demod_sta *demod_sta); */
int demod_set_sys(struct aml_demod_sta *demod_sta,
		  struct aml_demod_i2c *demod_i2c,
		  struct aml_demod_sys *demod_sys);
/* int demod_get_sys(struct aml_demod_i2c *demod_i2c, */
/* struct aml_demod_sys *demod_sys); */
/* int dvbt_set_ch(struct aml_demod_sta *demod_sta, */
/* struct aml_demod_i2c *demod_i2c, */
/* struct aml_demod_dvbt *demod_dvbt); */
/* int tuner_set_ch (struct aml_demod_sta *demod_sta, */
/* struct aml_demod_i2c *demod_i2c); */

/* typedef char               int8_t; */
/* typedef short int          int16_t; */
/* typedef int                int32_t; */
/* typedef long               int64_t; */
/*typedef unsigned char      uint8_t;
 * typedef unsigned short int uint16_t;
 * typedef unsigned int       uint32_t;
 * typedef unsigned long      uint64_t;*/

/*typedef unsigned   char    u8_t;
 * typedef signed     char    s8_t;
 * typedef unsigned   short   u16_t;
 * typedef signed     short   s16_t;
 * typedef unsigned   int     u32_t;
 * typedef signed     int     s32_t;
 * typedef unsigned   long    u64_t;
 * typedef signed     long    s64_t;*/

/* #define extadc */

/* for g9tv */
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
void demod_set_adc_core_clk(int, int, int);
void demod_set_adc_core_clk_fix(int clk_adc, int clk_dem);
void calculate_cordic_para(void);
void ofdm_read_all_regs(void);
extern int aml_fe_analog_set_frontend(struct dvb_frontend *fe);

#endif
