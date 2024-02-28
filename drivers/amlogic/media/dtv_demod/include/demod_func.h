/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
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
#define D_HHI_DADC_RDBK0_I			(0x29 << 2)
#define D_HHI_DADC_CNTL4			(0x2b << 2)

/*redefine*/
#define HHI_DEMOD_MEM_PD_REG		(0x43)
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

#define DEMOD_FRONT_AFIFO_ADC	(0x20)
#define DEMOD_FRONT_AGC_CFG1	(0x21)
#define DEMOD_FRONT_AGC_CFG2	(0x22)
#define DEMOD_FRONT_AGC_CFG3	(0x23)
#define DEMOD_FRONT_AGC_CFG6	(0x26)
#define DEMOD_FRONT_DC_CFG1		(0x28)

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
#define DBG_TIME	BIT(9)

#define PR_INFO(fmt, args ...)	pr_info("dtv_dmd:" fmt, ##args)

#define PR_TIME(fmt, args ...) \
	do { \
		if (aml_demod_debug & DBG_TIME) { \
			pr_info("dtv_dmd:" fmt, ##args); \
		} \
	} while (0)

#define PR_DBG(fmt, args ...) \
	do { \
		if (aml_demod_debug & DBG_INFO) { \
			pr_info("dtv_dmd:" fmt, ##args); \
		} \
	} while (0)

#define PR_ATSC(fmt, args ...) \
	do { \
		if (aml_demod_debug & DBG_ATSC) { \
			pr_info("dtv_dmd:" fmt, ##args); \
		} \
	} while (0)

#define PR_DVBC(fmt, args ...) \
	do { \
		if (aml_demod_debug & DBG_DVBC) { \
			pr_info("dtv_dmd:" fmt, ##args); \
		} \
	} while (0)

#define PR_DVBT(fmt, args ...) \
	do { \
		if (aml_demod_debug & DBG_DVBT) { \
			pr_info("dtv_dmd:" fmt, ##args); \
		} \
	} while (0)

#define PR_DVBS(fmt, args ...) \
	do { \
		if (aml_demod_debug & DBG_DVBS) { \
			pr_info("dtv_dmd:" fmt, ##args); \
		} \
	} while (0)

#define PR_DTMB(fmt, args ...) \
	do { \
		if (aml_demod_debug & DBG_DTMB) { \
			pr_info("dtv_dmd:" fmt, ##args); \
		} \
	} while (0)

#define PR_ISDBT(fmt, args ...) \
	do { \
		if (aml_demod_debug & DBG_ISDBT) { \
			pr_info("dtv_dmd:" fmt, ##args); \
		} \
	} while (0)

/*polling*/
#define PR_DBGL(fmt, args ...) \
	do { \
		if (aml_demod_debug & DBG_LOOP) { \
			pr_info("dtv_dmd:" fmt, ##args); \
		} \
	} while (0)

#define PR_ERR(fmt, args ...) pr_err("dtv_dmd:" fmt, ##args)
#define PR_WAR(fmt, args...)  pr_warn("dtv_dmd:" fmt, ##args)

#define ST_DRIVER_ID   0
#define ST_DRIVER_BASE (ST_DRIVER_ID << 16)
#define MAXNAMESIZE 30
#define POWOF2(number) (1 << (number))		/* was: s32 PowOf2(s32 number); */
#define POWOF4(number) (1 << ((number) << 1))	/* was: s32 PowOf2(s32 number); */
#define MAKEWORD(X, Y) (((X) << 8) + (Y))

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

struct stchip_register_t {
	unsigned short addr;     /* Address */
	unsigned char value;    /* Current value */
};

enum fe_sat_search_algo {
	FE_SAT_BLIND_SEARCH, /* offset freq and SR are Unknown */
	FE_SAT_COLD_START,   /* only the SR is known */
	FE_SAT_WARM_START    /* offset freq and SR are known */
};

enum fe_sat_search_standard {
	FE_SAT_AUTO_SEARCH,
	FE_SAT_SEARCH_DVBS1, /* Search Standard*/
	FE_SAT_SEARCH_DVBS2,
	FE_SAT_SEARCH_DSS,
	FE_SAT_SEARCH_TURBOCODE
};

enum fe_sat_rate { /*DVBS1, DSS and turbo code puncture rate*/
	FE_SAT_PR_1_2 = 0,
	FE_SAT_PR_2_3,
	FE_SAT_PR_3_4,
	FE_SAT_PR_4_5, /*for turbo code only*/
	FE_SAT_PR_5_6,
	FE_SAT_PR_6_7, /*for DSS only */
	FE_SAT_PR_7_8,
	FE_SAT_PR_8_9, /*for turbo code only*/
	FE_SAT_PR_UNKNOWN
};

enum fe_sat_search_iq_inv {
	FE_SAT_IQ_AUTO,
	FE_SAT_IQ_AUTO_NORMAL_FIRST,
	FE_SAT_IQ_FORCE_NORMAL,
	FE_SAT_IQ_FORCE_SWAPPED
};

enum fe_sat_modcode {
	/* 0x00..1F: Legacy list, DVBS2
	 * 0x20..3F: DVBS1
	 * 0x40..7F: DVBS2-X With: _S: Short frame (only here to make it work compile, otherwise
	 *		there will be a redefinition of enumerator)
	 *		_L: Linear constellation,
	 *		_R_xx: reserved + Modcode number hex Pon, Poff Pilots configuration
	 */
	FE_SAT_DUMMY_PLF  = 0x00,
	FE_SAT_QPSK_14   = 0x01,
	FE_SAT_QPSK_13   = 0x02,
	FE_SAT_QPSK_25    = 0x03,
	FE_SAT_QPSK_12    = 0x04,
	FE_SAT_QPSK_35   = 0x05,
	FE_SAT_QPSK_23   = 0x06,
	FE_SAT_QPSK_34    = 0x07,
	FE_SAT_QPSK_45    = 0x08,
	FE_SAT_QPSK_56   = 0x09,
	FE_SAT_QPSK_89   = 0x0A,
	FE_SAT_QPSK_910   = 0x0B,
	FE_SAT_8PSK_35    = 0x0C,
	FE_SAT_8PSK_23   = 0x0D,
	FE_SAT_8PSK_34   = 0x0E,
	FE_SAT_8PSK_56    = 0x0F,
	FE_SAT_8PSK_89    = 0x10,
	FE_SAT_8PSK_910  = 0x11,
	FE_SAT_16APSK_23 = 0x12,
	FE_SAT_16APSK_34  = 0x13,
	FE_SAT_16APSK_45  = 0x14,
	FE_SAT_16APSK_56 = 0x15,
	FE_SAT_16APSK_89 = 0x16,
	FE_SAT_16APSK_910 = 0x17,
	FE_SAT_32APSK_34  = 0x18,
	FE_SAT_32APSK_45 = 0x19,
	FE_SAT_32APSK_56 = 0x1A,
	FE_SAT_32APSK_89  = 0x1B,
	FE_SAT_32APSK_910 = 0x1C,
	FE_SAT_MODCODE_UNKNOWN = 0x1D,
	FE_SAT_MODCODE_UNKNOWN_1E = 0x1E,
	FE_SAT_MODCODE_UNKNOWN_1F  = 0x1F,
/* ---------------------------------------- */
	FE_SAT_DVBS1_QPSK_12 = 0x20,
	FE_SAT_DVBS1_QPSK_23 = 0x21,
	FE_SAT_DVBS1_QPSK_34 = 0x22,
	FE_SAT_DVBS1_QPSK_56 = 0x23,
	FE_SAT_DVBS1_QPSK_67 = 0x24,
	FE_SAT_DVBS1_QPSK_78 = 0x25,
	FE_SAT_MODCODE_UNKNOWN_26 = 0x26,
	FE_SAT_MODCODE_UNKNOWN_27 = 0x27,
	FE_SAT_MODCODE_UNKNOWN_28 = 0x28,
	FE_SAT_MODCODE_UNKNOWN_29 = 0x29,
	FE_SAT_MODCODE_UNKNOWN_2A = 0x2A,
	FE_SAT_MODCODE_UNKNOWN_2B = 0x2B,
	FE_SAT_MODCODE_UNKNOWN_2C = 0x2C,
	FE_SAT_MODCODE_UNKNOWN_2D = 0x2D,
	FE_SAT_MODCODE_UNKNOWN_2E = 0x2E,
	FE_SAT_MODCODE_UNKNOWN_2F = 0x2F,
	FE_SAT_MODCODE_UNKNOWN_30 = 0x30,
	FE_SAT_MODCODE_UNKNOWN_31 = 0x31,
	FE_SAT_MODCODE_UNKNOWN_32 = 0x32,
	FE_SAT_MODCODE_UNKNOWN_33 = 0x33,
	FE_SAT_MODCODE_UNKNOWN_34 = 0x34,
	FE_SAT_MODCODE_UNKNOWN_35 = 0x35,
	FE_SAT_MODCODE_UNKNOWN_36 = 0x36,
	FE_SAT_MODCODE_UNKNOWN_37 = 0x37,
	FE_SAT_MODCODE_UNKNOWN_38 = 0x38,
	FE_SAT_MODCODE_UNKNOWN_39 = 0x39,
	FE_SAT_MODCODE_UNKNOWN_3A = 0x3A,
	FE_SAT_MODCODE_UNKNOWN_3B = 0x3B,
	FE_SAT_MODCODE_UNKNOWN_3C = 0x3C,
	FE_SAT_MODCODE_UNKNOWN_3D = 0x3D,
	FE_SAT_MODCODE_UNKNOWN_3E = 0x3E,
	FE_SAT_MODCODE_UNKNOWN_3F = 0x3F,
/* ---------------------------------------- */
	FE_SATX_VLSNR1          = 0x40,
	FE_SATX_VLSNR2        = 0x41,
	FE_SATX_QPSK_13_45      = 0x42,
	FE_SATX_QPSK_9_20     = 0x43,
	FE_SATX_QPSK_11_20      = 0x44,
	FE_SATX_8APSK_5_9_L   = 0x45,
	FE_SATX_8APSK_26_45_L   = 0x46,
	FE_SATX_8PSK_23_36    = 0x47,
	FE_SATX_8PSK_25_36      = 0x48,
	FE_SATX_8PSK_13_18    = 0x49,
	FE_SATX_16APSK_1_2_L    = 0x4A,
	FE_SATX_16APSK_8_15_L = 0x4B,
	FE_SATX_16APSK_5_9_L    = 0x4C,
	FE_SATX_16APSK_26_45  = 0x4D,
	FE_SATX_16APSK_3_5      = 0x4E,
	FE_SATX_16APSK_3_5_L  = 0x4F,
	FE_SATX_16APSK_28_45    = 0x50,
	FE_SATX_16APSK_23_36  = 0x51,
	FE_SATX_16APSK_2_3_L    = 0x52,
	FE_SATX_16APSK_25_36  = 0x53,
	FE_SATX_16APSK_13_18    = 0x54,
	FE_SATX_16APSK_7_9    = 0x55,
	FE_SATX_16APSK_77_90    = 0x56,
	FE_SATX_32APSK_2_3_L  = 0x57,
	FE_SATX_32APSK_R_58     = 0x58,
	FE_SATX_32APSK_32_45  = 0x59,
	FE_SATX_32APSK_11_15    = 0x5A,
	FE_SATX_32APSK_7_9    = 0x5B,
	FE_SATX_64APSK_32_45_L  = 0x5C,
	FE_SATX_64APSK_11_15  = 0x5D,
	FE_SATX_64APSK_R_5E     = 0x5E,
	FE_SATX_64APSK_7_9    = 0x5F,
	FE_SATX_64APSK_R_60     = 0x60,
	FE_SATX_64APSK_4_5    = 0x61,
	FE_SATX_64APSK_R_62     = 0x62,
	FE_SATX_64APSK_5_6    = 0x63,
	FE_SATX_128APSK_3_4     = 0x64,
	FE_SATX_128APSK_7_9   = 0x65,
	FE_SATX_256APSK_29_45_L = 0x66,
	FE_SATX_256APSK_2_3_L = 0x67,
	FE_SATX_256APSK_31_45_L = 0x68,
	FE_SATX_256APSK_32_45 = 0x69,
	FE_SATX_256APSK_11_15_L = 0x6A,
	FE_SATX_256APSK_3_4   = 0x6B,
	FE_SATX_QPSK_11_45      = 0x6C,
	FE_SATX_QPSK_4_15     = 0x6D,
	FE_SATX_QPSK_14_45      = 0x6E,
	FE_SATX_QPSK_7_15     = 0x6F,
	FE_SATX_QPSK_8_15       = 0x70,
	FE_SATX_QPSK_32_45    = 0x71,
	FE_SATX_8PSK_7_15       = 0x72,
	FE_SATX_8PSK_8_15     = 0x73,
	FE_SATX_8PSK_26_45      = 0x74,
	FE_SATX_8PSK_32_45    = 0x75,
	FE_SATX_16APSK_7_15     = 0x76,
	FE_SATX_16APSK_8_15   = 0x77,
	FE_SATX_16APSK_26_45_S  = 0x78,
	FE_SATX_16APSK_3_5_S  = 0x79,
	FE_SATX_16APSK_32_45    = 0x7A,
	FE_SATX_32APSK_2_3    = 0x7B,
	FE_SATX_32APSK_32_45_S  = 0x7C,
/* ---------------------------------------- */
	FE_SATX_8PSK            = 0x7D,
	FE_SATX_32APSK        = 0x7E,
	FE_SATX_256APSK       = 0x7F,  /* POFF Modes */
	FE_SATX_16APSK        = 0x80,
	FE_SATX_64APSK        = 0x81,
	FE_SATX_1024APSK      = 0x82,  /* PON Modes  */
	FE_SAT_MODCODE_MAX      /* Only used for range checking */
};

enum fe_sat_modulation {
	FE_SAT_MOD_QPSK,
	FE_SAT_MOD_8PSK,
	FE_SAT_MOD_16APSK,
	FE_SAT_MOD_32APSK,
	FE_SAT_MOD_VLSNR,
	FE_SAT_MOD_64APSK,
	FE_SAT_MOD_128APSK,
	FE_SAT_MOD_256APSK,
	FE_SAT_MOD_8PSK_L,
	FE_SAT_MOD_16APSK_L,
	FE_SAT_MOD_32APSK_L,
	FE_SAT_MOD_64APSK_L,
	FE_SAT_MOD_256APSK_L,
	FE_SAT_MOD_1024APSK,
	FE_SAT_MOD_UNKNOWN
};	/* sat modulation type*/

enum stchip_error {
	CHIPERR_NO_ERROR = 0x0,       /* No error encountered */
	CHIPERR_INVALID_HANDLE = 0x1,     /* Using of an invalid chip handle */
	CHIPERR_INVALID_REG_ID = 0x2,     /* Using of an invalid register */
	CHIPERR_INVALID_FIELD_ID = 0x4,   /* Using of an Invalid field */
	CHIPERR_INVALID_FIELD_SIZE = 0x8, /* Using of a field with an invalid size */
	CHIPERR_I2C_NO_ACK = 0x10,         /* No acknowledge from the chip */
	CHIPERR_I2C_BURST = 0x20,           /* Two many registers accessed in burst mode */
	CHIPERR_TYPE_ERR = 0x40,
	CHIPERR_CONFIG_ERR = 0x80
};

enum fe_sat_tracking_standard {
	FE_SAT_DVBS1_STANDARD, /* Found Standard*/
	FE_SAT_DVBS2_STANDARD,
	FE_SAT_DSS_STANDARD,
	FE_SAT_TURBOCODE_STANDARD,
	FE_SAT_UNKNOWN_STANDARD
};

enum fe_sat_pilots {
	FE_SAT_PILOTS_OFF,
	FE_SAT_PILOTS_ON,
	FE_SAT_UNKNOWN_PILOTS
};

enum fe_sat_rolloff {
	/* same order than MATYPE_ROLLOFF field */
	FE_SAT_35,
	FE_SAT_25,
	FE_SAT_20,
	FE_SAT_10,
	FE_SAT_05,
	FE_SAT_15,
};

enum fe_sat_frame {
	FE_SAT_NORMAL_FRAME,
	FE_SAT_SHORT_FRAME,
	FE_SAT_LONG_FRAME  = FE_SAT_NORMAL_FRAME,
	FE_SAT_UNKNOWN_FRAME
};

enum fe_sat_iq_inversion {
	FE_SAT_IQ_NORMAL,
	FE_SAT_IQ_SWAPPED
};

struct fe_sat_search_result {
	bool		 locked;	/* Transponder found */
	unsigned int frequency;	/* Found frequency */
	unsigned int symbol_rate;/* Found Symbol rate*/
	enum fe_sat_tracking_standard	standard;	/* Found Standard DVBS1,DVBS2 or DSS*/
	enum fe_sat_rate			puncture_rate;/* Found Puncture rate  For DVBS1*/
	enum fe_sat_modcode			modcode;/* Found Modcode only for DVBS2*/
	enum fe_sat_modulation		modulation;	/* Found modulation type*/
	enum fe_sat_pilots			pilots;	/* pilots Found for DVBS2*/
	enum fe_sat_frame			frame_length;/* Found frame length for DVBS2*/
	enum fe_sat_iq_inversion	spectrum;/* IQ specrum swap setting*/
	enum fe_sat_rolloff		roll_off;/* Rolloff factor (0.2, 0.25 or 0.35)*/
};

enum {
	ST_NO_ERROR = ST_DRIVER_BASE,
	ST_ERROR_BAD_PARAMETER,             /* Bad parameter passed       */
	ST_ERROR_NO_MEMORY,                 /* Memory allocation failed   */
	ST_ERROR_UNKNOWN_DEVICE,            /* Unknown device name        */
	ST_ERROR_ALREADY_INITIALIZED,       /* Device already initialized */
	ST_ERROR_NO_FREE_HANDLES,           /* Cannot open device again   */
	ST_ERROR_OPEN_HANDLE,               /* At least one open handle   */
	ST_ERROR_INVALID_HANDLE,            /* Handle is not valid        */
	ST_ERROR_FEATURE_NOT_SUPPORTED,     /* Feature unavailable        */
	ST_ERROR_INTERRUPT_INSTALL,         /* Interrupt install failed   */
	ST_ERROR_INTERRUPT_UNINSTALL,       /* Interrupt uninstall failed */
	ST_ERROR_TIMEOUT,                   /* Timeout occurred            */
	ST_ERROR_DEVICE_BUSY,               /* Device is currently busy   */
	STFRONTEND_ERROR_I2C
};

enum fe_lla_error {
	FE_LLA_NO_ERROR				= ST_NO_ERROR,
	FE_LLA_INVALID_HANDLE		= ST_ERROR_INVALID_HANDLE,
	FE_LLA_ALLOCATION			= ST_ERROR_NO_MEMORY,
	FE_LLA_BAD_PARAMETER		= ST_ERROR_BAD_PARAMETER,
	FE_LLA_ALREADY_INITIALIZED	= ST_ERROR_BAD_PARAMETER,
	FE_LLA_I2C_ERROR			= STFRONTEND_ERROR_I2C,
	FE_LLA_SEARCH_FAILED,
	FE_LLA_TRACKING_FAILED,
	FE_LLA_NODATA,
	FE_LLA_TUNER_NOSIGNAL,
	FE_LLA_TUNER_JUMP,
	FE_LLA_TUNER_4_STEP,
	FE_LLA_TUNER_8_STEP,
	FE_LLA_TUNER_16_STEP,
	FE_LLA_TERM_FAILED,
	FE_LLA_DISEQC_FAILED,
	FE_LLA_NOT_SUPPORTED
}; /*Error Type*/

enum stchip_fieldtype {
	CHIP_UNSIGNED,
	CHIP_SIGNED
};

struct stchip_field {
	unsigned short reg;      /* Register index */
	unsigned char pos;      /* Bit position */
	unsigned char bits;     /* Bit width */
	unsigned int mask;     /* Mask compute with width and position */
	enum stchip_fieldtype type;     /* Signed or unsigned */
	char name[48]; /* Name */
};

struct stchip_instindex {
	unsigned short regidx;     /* Register reference index */
	unsigned int fldidx;     /* Field reference index */
};

struct stchip_instance {
	unsigned short path;           /* Path index */
	struct stchip_instindex index[16];	/* 1st Register/Field index */
};

/* how to access I2C bus */
enum stchip_mode {
	/* <trans><addr><addr...><data><data...>  (e.g. oxford stbus bridge, fixed transaction) */
	STCHIP_MODE_I2C2STBUS,
	STCHIP_MODE_SUBADR_8,       /* <addr><reg8><data><data>        (e.g. demod chip) */
	STCHIP_MODE_SUBADR_16,      /* <addr><reg8><data><data>        (e.g. demod chip) */
	STCHIP_MODE_NOSUBADR,       /* <addr><data>|<data><data><data> (e.g. tuner chip) */
	/* <addr><data>|<data><data><data> (e.g. tuner chip) only for read */
	STCHIP_MODE_NOSUBADR_RD,
	STCHIP_MODE_STI5197,        /* 5197: base address FDE11000 for QAM IP registers */
	STCHIP_MODE_MXL,		    /* Access mode for MXL201RF tuner */
	STCHIP_MODE_STIH273,		/* whatever */
	STCHIP_MODE_STV0368,		/* 0368: specific use of PAGESHIFT register */
	/* 0368: specific use of MAILBOX communication scheme (!!!requires xp70 F/W!!!)*/
	STCHIP_MODE_STV0368MAILBOX,
	STCHIP_MODE_SUBADR_8_SR,
	STCHIP_MODE_STIH237
};

struct stchip_info {
	unsigned char	i2caddr;          /* Chip I2C address */
	char			name[MAXNAMESIZE];/* Name of the chip */
	unsigned int	nb_insts;          /* Number of instances in the chip */
	int				nb_regs;           /* Number of registers in the chip */
	unsigned int	nb_fields;         /* Number of fields in the chip */
	struct stchip_register_t *p_reg_map_image; /* Pointer to register map */
	struct stchip_field		*p_field_map_image;   /* Pointer to field map */
	struct stchip_instance	*p_inst_map;         /* Pointer to an Instances Map */
	enum stchip_error		error;            /* Error state */
	enum stchip_mode chipmode;/* Access bus in demod (SubAdr) or tuner (NoSubAdr) mode*/
	unsigned char		chipid;           /* Chip cut ID */

	bool				repeater;         /* Is repeater enabled or not ? */
	struct stchip_info *repeater_host;   /* Owner of the repeater */
	/* Pointer to repeater routine */
	enum stchip_error		(*repeater_fn)(struct stchip_info *hchip, bool state);

	/* Parameters needed for non sub address devices */
	unsigned int	wr_start;		  /* Id of the first writable register */
	unsigned int	wr_size;           /* Number of writable registers */
	unsigned short	rd_start;		  /* Id of the first readable register */
	unsigned int	rd_size;			  /* Number of readable registers */
	/* Last accessed register index in the register map Image */
	signed int		last_reg_index;
	/* Abort flag when set to on no register access and no wait are done*/
	bool			abort;

	void			*p_data;			/* pointer to chip data */
	unsigned char	tuner_nb;/* number of tuner from 0 to 3, field added to match Oxford */
	//struct io_access		io_fcn;
	unsigned char	*p_reg_mem_base;	/* Pointer to register map base*/
};

//typedef stchip_info_t  stchip_handle_t;  /* Handle to a chip */

struct fe_l2a_internal_param {
	unsigned int	tuner_bw;
	signed int		tuner_frequency; /* Current tuner frequency (KHz) */
	enum fe_lla_error	demod_error;
	signed int		tuner_index_jump;
	signed int		tuner_index_jump1;
	unsigned int	sr;
	unsigned int	state;
	struct stchip_info handle_demod; /*  Handle to a demodulator */
	struct stchip_info handle_anafe; /*  Handle to AFE */
	struct stchip_info handle_soc; /*  Handle to SOC */
	struct stchip_info handle_tuner; /* Handle to tuner */
	struct stchip_info handle_vglna; /* Handle to the chip VGLNA*/
	enum fe_sat_rolloff roll_off; /* manual RollOff for DVBS1/DSS only */

	/* Demod */
	/* Search standard:Auto, DVBS1/DSS only or DVBS2 only*/
	enum fe_sat_search_standard	demod_search_standard;
	/* Algorithm for search Blind, Cold or Warm*/
	enum fe_sat_search_algo		demod_search_algo;
	/* I,Q inversion search : auto, auto normal first, normal or inverted */
	enum fe_sat_search_iq_inv	demod_search_iq_inv;
	enum fe_sat_rate			demod_puncture_rate;
	enum fe_sat_modcode			demod_modcode;
	enum fe_sat_modulation		demod_modulation;
	struct fe_sat_search_result	demod_results; /* Results of the search */
	/* Last error encountered */
	unsigned int				demod_symbol_rate; /* Symbol rate (Bds) */
	unsigned int				demod_search_range; /* Search range (Hz) */
	unsigned int				quartz; /* Demod reference frequency (Hz)*/
	unsigned int				master_clock; /* Master clock frequency (Hz) */
	/* Temporary definition for LO frequency*/
	unsigned int				lo_frequency;

	/* Global I,Q inversion I,Q connection from tuner to demod*/
	enum fe_sat_iq_inversion	tuner_global_iqv_inv;

	//struct ram_byte			pid_flt;
	//struct gse_ram_byte		gse_flt;
	//struct tuner_access		tuner_fcn;
	//struct io_access		demod_io_fcn;
	//struct io_access		tuner_io_fcn;
};

void st_dvbs2_init(void);
bool tuner_find_by_name(struct dvb_frontend *fe, const char *name);
void tuner_set_params(struct dvb_frontend *fe);
int tuner_get_ch_power(struct dvb_frontend *fe);

int dtmb_get_power_strength(int agc_gain);
int dvbc_get_power_strength(int agc_gain, int tuner_strength);
int j83b_get_power_strength(int agc_gain, int tuner_strength);
int atsc_get_power_strength(int agc_gain, int tuner_strength);

/* dvbt */
int dvbt_isdbt_set_ch(struct aml_dtvdemod *demod,
		struct aml_demod_dvbt *demod_dvbt);
unsigned int dvbt_set_ch(struct aml_dtvdemod *demod,
		struct aml_demod_dvbt *demod_dvbt, struct dvb_frontend *fe);
int dvbt2_set_ch(struct aml_dtvdemod *demod, struct dvb_frontend *fe);

/* dvbc */
int dvbc_set_ch(struct aml_dtvdemod *demod, struct aml_demod_dvbc *demod_dvbc,
		struct dvb_frontend *fe);
int dvbc_status(struct aml_dtvdemod *demod, struct aml_demod_sts *demod_sts,
		struct seq_file *seq);
int dvbc_isr_islock(void);
void dvbc_isr(struct aml_demod_sta *demod_sta);
u32 dvbc_set_qam_mode(unsigned char mode);
u32 dvbc_get_status(struct aml_dtvdemod *demod);
u32 dvbc_set_auto_symtrack(struct aml_dtvdemod *demod);
int dvbc_timer_init(void);
void dvbc_timer_exit(void);
int dvbc_cci_task(void *data);
int dvbc_get_cci_task(struct aml_dtvdemod *demod);
void dvbc_create_cci_task(struct aml_dtvdemod *demod);
void dvbc_kill_cci_task(struct aml_dtvdemod *demod);

/* DVBC */
/*gxtvbb*/

void dvbc_reg_initial_old(struct aml_dtvdemod *demod);

/*txlx*/
enum dvbc_sym_speed {
	SYM_SPEED_NORMAL,
	SYM_SPEED_MIDDLE,
	SYM_SPEED_HIGH
};
void dvbc_reg_initial(struct aml_dtvdemod *demod, struct dvb_frontend *fe);
void demod_dvbc_qam_reset(struct aml_dtvdemod *demod);
void demod_dvbc_fsm_reset(struct aml_dtvdemod *demod);
void demod_dvbc_store_qam_cfg(struct aml_dtvdemod *demod);
void demod_dvbc_restore_qam_cfg(struct aml_dtvdemod *demod);
void demod_dvbc_set_qam(struct aml_dtvdemod *demod, unsigned int qam, bool auto_sr);
void dvbc_init_reg_ext(struct aml_dtvdemod *demod);
u32 dvbc_get_ch_sts(struct aml_dtvdemod *demod);
u32 dvbc_get_qam_mode(struct aml_dtvdemod *demod);
void dvbc_cfg_sr_cnt(struct aml_dtvdemod *demod, enum dvbc_sym_speed spd);
void dvbc_cfg_sr_scan_speed(struct aml_dtvdemod *demod, enum dvbc_sym_speed spd);
void dvbc_cfg_tim_sweep_range(struct aml_dtvdemod *demod, enum dvbc_sym_speed spd);
void dvbc_cfg_sw_hw_sr_max(struct aml_dtvdemod *demod, unsigned int max_sr);
int dvbc_auto_qam_process(struct aml_dtvdemod *demod, unsigned int *qam_mode);
int dvbc_blind_scan_process(struct aml_dtvdemod *demod);

/* dtmb */
int dtmb_set_ch(struct aml_dtvdemod *demod,
		struct aml_demod_dtmb *demod_dtmb);
void dtmb_reset(void);
int dtmb_check_status_gxtv(struct dvb_frontend *fe);
int dtmb_check_status_txl(struct dvb_frontend *fe);
int dtmb_bch_check(struct dvb_frontend *fe);
void dtmb_bch_check_new(struct dvb_frontend *fe, bool reset);
void dtmb_write_reg(unsigned int reg_addr, unsigned int reg_data);
unsigned int dtmb_read_reg(unsigned int reg_addr);
void dtmb_write_reg_bits(u32 addr, const u32 data, const u32 start, const u32 len);
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

void dtmb_set_mem_st(unsigned int mem_start);
int dtmb_read_agc(enum REG_DTMB_D9 type, unsigned int *buf);
unsigned int dtmb_reg_r_che_snr(void);
unsigned int dtmb_reg_r_fec_lock(void);
unsigned int dtmb_reg_r_bch(void);
int check_dtmb_fec_lock(void);
int dtmb_constell_check(void);

void dtmb_no_signal_check_v3(struct aml_dtvdemod *demod);
void dtmb_no_signal_check_finishi_v3(struct aml_dtvdemod *demod);
unsigned int dtmb_detect_first(void);

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
void demod_set_tvfe_reg(unsigned int data, unsigned int addr);
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
void demod_set_reg(struct aml_dtvdemod *demod, struct aml_demod_reg *demod_reg);
void demod_get_reg(struct aml_dtvdemod *demod, struct aml_demod_reg *demod_reg);

/* void demod_calc_clk(struct aml_demod_sta *demod_sta); */
int demod_set_sys(struct aml_dtvdemod *demod, struct aml_demod_sys *demod_sys);
void set_j83b_filter_reg_v4(struct aml_dtvdemod *demod);


/* for g9tv */
int adc_dpll_setup(int clk_a, int clk_b, int clk_sys, struct aml_demod_sta *demod_sta);
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
int demod_set_adc_core_clk(int adc_clk, int sys_clk, struct aml_demod_sta *demod_sta);
void demod_set_adc_core_clk_fix(int clk_adc, int clk_dem);
void calculate_cordic_para(void);
/*extern int aml_fe_analog_set_frontend(struct dvb_frontend *fe);*/
int get_dtvpll_init_flag(void);
void demod_set_mode_ts(struct aml_dtvdemod *demod, enum fe_delivery_system delsys);
void qam_write_reg(struct aml_dtvdemod *demod,
		unsigned int reg_addr, unsigned int reg_data);
unsigned int qam_read_reg(struct aml_dtvdemod *demod, unsigned int reg_addr);
void qam_write_bits(struct aml_dtvdemod *demod,
		u32 reg_addr, const u32 reg_data,
		const u32 start, const u32 len);
void dvbc_write_reg(unsigned int addr, unsigned int data);
unsigned int dvbc_read_reg(unsigned int addr);
void demod_top_write_reg(unsigned int addr, unsigned int data);
void demod_top_write_bits(u32 reg_addr, const u32 reg_data, const u32 start, const u32 len);
unsigned int demod_top_read_reg(unsigned int addr);
void demod_init_mutex(void);
void front_write_reg(unsigned int addr, unsigned int data);
void front_write_bits(u32 reg_addr, const u32 reg_data,
		    const u32 start, const u32 len);
unsigned int front_read_reg(unsigned int addr);
unsigned int isdbt_read_reg_v4(unsigned int addr);
void  isdbt_write_reg_v4(unsigned int addr, unsigned int data);
int dd_hiu_reg_write(unsigned int reg, unsigned int val);
unsigned int dd_hiu_reg_read(unsigned int addr);
void dtvdemod_dmc_reg_write(unsigned int reg, unsigned int val);
unsigned int dtvdemod_dmc_reg_read(unsigned int addr);
void dtvdemod_ddr_reg_write(unsigned int reg, unsigned int val);
unsigned int dtvdemod_ddr_reg_read(unsigned int addr);
int reset_reg_write(unsigned int reg, unsigned int val);
unsigned int reset_reg_read(unsigned int addr);

int clocks_set_sys_defaults(struct aml_dtvdemod *demod, unsigned int adc_clk);
void demod_set_demod_default(void);
unsigned int demod_get_adc_clk(struct aml_dtvdemod *demod);
unsigned int demod_get_sys_clk(struct aml_dtvdemod *demod);

/*register access api new*/
void dvbt_isdbt_wr_reg(unsigned int addr, unsigned int data);
void dvbt_isdbt_wr_reg_new(unsigned int addr, unsigned int data);
void dvbt_isdbt_wr_bits_new(u32 reg_addr, const u32 reg_data,
		    const u32 start, const u32 len);
unsigned int dvbt_isdbt_rd_reg(unsigned int addr);
unsigned int dvbt_isdbt_rd_reg_new(unsigned int addr);
void dvbt_t2_wrb(unsigned int addr, unsigned char data);
void dvbt_t2_write_w(unsigned int addr, unsigned int data);
void dvbt_t2_wr_byte_bits(u32 addr, const u32 data, const u32 start, const u32 len);
void dvbt_t2_wr_word_bits(u32 addr, const u32 data, const u32 start, const u32 len);
unsigned int dvbt_t2_read_w(unsigned int addr);
char dvbt_t2_rdb(unsigned int addr);
void riscv_ctl_write_reg(unsigned int addr, unsigned int data);
unsigned int riscv_ctl_read_reg(unsigned int addr);
void dvbs_write_bits(u32 reg_addr, const u32 reg_data,
		    const u32 start, const u32 len);
void dvbs_wr_byte(unsigned int addr, unsigned char data);
unsigned char dvbs_rd_byte(unsigned int addr);
int aml_demod_init(void);
void aml_demod_exit(void);
unsigned int write_riscv_ram(void);
unsigned int dvbs_get_quality(void);
void dvbs2_reg_initial(unsigned int symb_rate_kbs, unsigned int is_blind_scan);
int dvbs_get_signal_strength_off(void);

void t3_revb_set_ambus_state(bool enable, bool is_t2);
void t5w_write_ambus_reg(u32 addr,
	const u32 data, const u32 start, const u32 len);
unsigned int t5w_read_ambus_reg(unsigned int addr);

void demod_enable_frontend_agc(struct aml_dtvdemod *demod,
		enum fe_delivery_system delsys, bool enable);
void fe_l2a_set_symbol_rate(struct fe_l2a_internal_param *pparams, unsigned int symbol_rate);
void fe_l2a_get_agc2accu(struct fe_l2a_internal_param *pparams, unsigned int *pintegrator);

#endif
