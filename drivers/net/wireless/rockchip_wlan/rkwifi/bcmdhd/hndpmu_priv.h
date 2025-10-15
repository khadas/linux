/*
 * PMU support interface private to hndpmu.c and hndpmu_dhd.c.
 *
 * Copyright (C) 2025 Synaptics Incorporated. All rights reserved.
 *
 * This software is licensed to you under the terms of the
 * GNU General Public License version 2 (the "GPL") with Broadcom special exception.
 *
 * INFORMATION CONTAINED IN THIS DOCUMENT IS PROVIDED "AS-IS," AND SYNAPTICS
 * EXPRESSLY DISCLAIMS ALL EXPRESS AND IMPLIED WARRANTIES, INCLUDING ANY
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE,
 * AND ANY WARRANTIES OF NON-INFRINGEMENT OF ANY INTELLECTUAL PROPERTY RIGHTS.
 * IN NO EVENT SHALL SYNAPTICS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, PUNITIVE, OR CONSEQUENTIAL DAMAGES ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OF THE INFORMATION CONTAINED IN THIS DOCUMENT, HOWEVER CAUSED
 * AND BASED ON ANY THEORY OF LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, AND EVEN IF SYNAPTICS WAS ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE. IF A TRIBUNAL OF COMPETENT JURISDICTION
 * DOES NOT PERMIT THE DISCLAIMER OF DIRECT DAMAGES OR ANY OTHER DAMAGES,
 * SYNAPTICS' TOTAL CUMULATIVE LIABILITY TO ANY PARTY SHALL NOT
 * EXCEED ONE HUNDRED U.S. DOLLARS
 *
 * Copyright (C) 2025, Broadcom.
 *
 *      Unless you and Broadcom execute a separate written software license
 * agreement governing use of this software, this software is licensed to you
 * under the terms of the GNU General Public License version 2 (the "GPL"),
 * available at http://www.broadcom.com/licenses/GPLv2.php, with the
 * following added to such license:
 *
 *      As a special exception, the copyright holders of this software give you
 * permission to link this software with independent modules, and to copy and
 * distribute the resulting executable under terms of your choice, provided that
 * you also meet, for each linked independent module, the terms and conditions of
 * the license of that module.  An independent module is a module which is not
 * derived from this software.  The special exception does not apply to any
 * modifications of the software.
 *
 *
 * <<Broadcom-WL-IPTag/Dual:>>
 */

#ifndef _hndpmu_priv_h_
#define _hndpmu_priv_h_

#include <typedefs.h>
#include <siutils.h>

/* ******************************************************************** */
/* **** The following are common to both hndpmu.c and hndpmu_dhd.c **** */
/* ******************************************************************** */
#ifdef EVENT_LOG_COMPILE
#include <event_log.h>
#endif

#if defined(EVENT_LOG_COMPILE) && defined(BCMDBG_ERR) && defined(ERR_USE_EVENT_LOG)
#if defined(ERR_USE_EVENT_LOG_RA)
#define PMU_ERROR(args) EVENT_LOG_RA(EVENT_LOG_TAG_PMU_ERROR, args)
#else
#define PMU_ERROR(args) EVENT_LOG_COMPACT_CAST_PAREN_ARGS(EVENT_LOG_TAG_PMU_ERROR, args)
#endif /* ERR_USE_EVENT_LOG_RA */
#elif defined(BCMDBG_ERR)
#define	PMU_ERROR(args)	printf args
#else
#define	PMU_ERROR(args)
#endif	/* defined(BCMDBG_ERR) && defined(ERR_USE_EVENT_LOG) */

#ifdef BCMDBG
//#define BCMDBG_PMU
#endif

#ifdef BCMDBG_PMU
#define	PMU_MSG(args)	printf args
#else
#define	PMU_MSG(args)
#endif	/* BCMDBG_MPU */

/* To check in verbose debugging messages not intended
 * to be on except on private builds.
 */
#define	PMU_NONE(args)

#ifndef BCMDONGLEHOST
/* ******************************************************************************************** */
/* **** The following APIs are implemented in hndpmu.c but for chip specific files to call **** */
/* ******************************************************************************************** */
/* setup pll and query clock speed */
typedef struct {
	uint32	fref;	/* x-tal frequency in [hz] */
	uint8	xf;	/* x-tal index as contained in PMU control reg, see PMU programmers guide */
	uint8	p1div;
	uint8	p2div;
	uint8	ndiv_int;
	uint32	ndiv_frac;
} pmu1_xtaltab0_t;

/* For having the pllcontrol data (info)
 * The table with the values of the registers will have one - one mapping.
 */
typedef struct {
	uint16	clock;	/**< x-tal frequency in [KHz] */
	uint8	mode;	/**< spur mode */
	uint8	xf;	/**< corresponds with xf bitfield in PMU control register */
} pllctrl_data_t;

extern const pmu1_xtaltab0_t pmu1_xtaltab0_960[];
extern const pmu1_xtaltab0_t pmu1_xtaltab0_1292[];

void si_pmu_dynamic_clk_switch_enab(si_t *sih);
void si_set_lv_sleep_mode(si_t *sih, osl_t *osh);
void si_set_lv_sleep_mode_pmu(si_t *sih, osl_t *osh);
void si_pmu_fll_preload_enable(si_t *sih);
void si_pmu_fast_lpo_enable(si_t *sih, osl_t *osh);
uint8 si_pmu_pll28nm_calc_ndiv(uint32 fvco, uint32 xtal, uint32 *ndiv_int, uint32 *ndiv_frac);
uint8 si_pmu_pllctrlreg_update(si_t *sih, osl_t *osh, pmuregs_t *pmu, uint32 xtal,
	uint8 spur_mode, const pllctrl_data_t *pllctrlreg_update, uint32 array_size,
	const uint32 *pllctrlreg_val);
void si_pmu_armpll_write(si_t *sih, uint32 xtal, uint32 pdiv);
uint32 si_pmu_res_deps(si_t *sih, osl_t *osh, pmuregs_t *pmu, uint32 rsrcs, bool all);

#define MISC_LDO_STEPPING_DELAY	(150u)	/* 150 us, includes 50us additional margin */

extern const char rstr_Invalid_Unsupported_xtal_value_D[];
extern const char rstr_btldo3p3pu[];
extern const char rstr_memlpldo_volt[];
extern const char rstr_lpldo_volt[];
extern const char rstr_abuck_volt[];
extern const char rstr_cbuck_volt[];
#ifdef BCM_PMU_TRIM
extern const char rstr_pmu_trim_dis[];
#endif

/* **************************************************************************************** */
/* **** The following APIs are implemented in chip specific files e.g. hndpmu_<chip>.c **** */
/* **************************************************************************************** */

/* defines to make the code more readable */
/* But 0 is a valid resource number! */
#define NO_SUCH_RESOURCE	0	/**< means: chip does not have such a PMU resource */

/** contains resource bit positions for a specific chip */
struct rsc_per_chip {
	uint8 ht_avail;
	uint8 macphy_clkavail;
	uint8 ht_start;
	uint8 otp_pu;
	uint8 macphy_aux_clkavail;
	uint8 macphy_scan_clkavail;
	uint8 cb_ready;
	uint8 dig_ready;
	uint8 xtal;
};

typedef struct rsc_per_chip rsc_per_chip_t;

const rsc_per_chip_t *si_pmu_get_rsc_positions(si_t *sih);

void si_pmu_min_max_res_masks(si_t *sih, uint32 *pmin, uint32 *pmax);
void si_pmu_res_init(si_t *sih, osl_t *osh);

/* FVCO frequency in [KHz] */
#define FVCO_640	640000u		/**< 640MHz */
#define FVCO_880	880000u		/**< 880MHz */
#define FVCO_1760	1760000u	/**< 1760MHz */
#define FVCO_1440	1440000u	/**< 1440MHz */
#define FVCO_960	960000u		/**< 960MHz */
#define FVCO_960p1	960100u		/**< 960.1MHz */
#define FVCO_960010	960010u		/**< 960.0098MHz */
#define FVCO_961	961000u		/**< 961MHz */
#define FVCO_960p5	960500u		/**< 960.5MHz */
#define FVCO_963	963000u		/**< 963MHz */
#define FVCO_963p01	963010u		/**< 963.01MHz */
#define FVCO_1000	1000000u	/**< 1000MHz */
#define FVCO_1600	1600000u	/**< 1600MHz */
#define FVCO_1920	1920000u	/**< 1920MHz */
#define FVCO_1938	1938000u	/**< 1938MHz */
#define FVCO_385	385000u		/**< 385MHz */
#define FVCO_400	400000u		/**< 400MHz */
#define FVCO_720	720000u		/**< 720MHz */
#define FVCO_2880	2880000u	/**< 2880 MHz */
#define FVCO_2946	2946000u	/**< 2946 MHz */
#define FVCO_3000	3000000u	/**< 3000 MHz */
#define FVCO_3200	3200000u	/**< 3200 MHz */
#define FVCO_1002p8	1002823u	/**< 1002.823MHz */
#define FVCO_1400	1400000u	/**< 1400MHz */
#define FVCO_1292	1292000u	/**< 1292MHz for BBPLL used in 4397B0/C0 */

/* uses these defines instead of 'magic' values when writing to register pllcontrol_addr */
#if defined(VLSI_CTRL_REGS) && defined(SKIP_LEGACY_CC_API)
#define PMU_PLL_CTRL_REG0	SI_CTRLREGS_INVALID
#define PMU_PLL_CTRL_REG1	SI_CTRLREGS_INVALID
#define PMU_PLL_CTRL_REG2	SI_CTRLREGS_INVALID
#define PMU_PLL_CTRL_REG3	SI_CTRLREGS_INVALID
#define PMU_PLL_CTRL_REG4	SI_CTRLREGS_INVALID
#define PMU_PLL_CTRL_REG5	SI_CTRLREGS_INVALID
#define PMU_PLL_CTRL_REG6	SI_CTRLREGS_INVALID
#define PMU_PLL_CTRL_REG7	SI_CTRLREGS_INVALID
#define PMU_PLL_CTRL_REG8	SI_CTRLREGS_INVALID
#define PMU_PLL_CTRL_REG9	SI_CTRLREGS_INVALID
#define PMU_PLL_CTRL_REG10	SI_CTRLREGS_INVALID
#define PMU_PLL_CTRL_REG11	SI_CTRLREGS_INVALID
#define PMU_PLL_CTRL_REG12	SI_CTRLREGS_INVALID
#define PMU_PLL_CTRL_REG13	SI_CTRLREGS_INVALID
#define PMU_PLL_CTRL_REG14	SI_CTRLREGS_INVALID
#define PMU_PLL_CTRL_REG15	SI_CTRLREGS_INVALID
#else /* !VLSI_CTRL_REGS || !SKIP_LEGACY_CC_API */
#define PMU_PLL_CTRL_REG0	0u
#define PMU_PLL_CTRL_REG1	1u
#define PMU_PLL_CTRL_REG2	2u
#define PMU_PLL_CTRL_REG3	3u
#define PMU_PLL_CTRL_REG4	4u
#define PMU_PLL_CTRL_REG5	5u
#define PMU_PLL_CTRL_REG6	6u
#define PMU_PLL_CTRL_REG7	7u
#define PMU_PLL_CTRL_REG8	8u
#define PMU_PLL_CTRL_REG9	9u
#define PMU_PLL_CTRL_REG10	10u
#define PMU_PLL_CTRL_REG11	11u
#define PMU_PLL_CTRL_REG12	12u
#define PMU_PLL_CTRL_REG13	13u
#define PMU_PLL_CTRL_REG14	14u
#define PMU_PLL_CTRL_REG15	15u
#endif /* !VLSI_CTRL_REGS || !SKIP_LEGACY_CC_API */

/* Setup resource up/down timers */
typedef struct {
	uint8 resnum;
	uint32 updown;
} pmu_res_updown_t;

#define PMU_RES_SUBSTATE_SHIFT		8

const pmu_res_updown_t *si_pmu_res_updown_table(si_t *sih, uint32 *pmu_res_updown_table_sz);
const pmu_res_updown_t *si_pmu_res_updown_fixup_table(si_t *sih,
	uint32 *pmu_res_updown_fixup_table_sz);

/* Setup resource substate transition timer value */
typedef struct {
	uint8 resnum;
	uint8 substate;
	uint32 tmr;
} pmu_res_subst_trans_tmr_t;

const pmu_res_subst_trans_tmr_t *si_pmu_res_subst_trans_tmr_table(si_t *sih,
	uint32 *pmu_res_subst_trans_tmr_table_sz);

/* Resource dependencies mask change action */
#define RES_DEPEND_SET		0	/* Override the dependencies mask */
#define RES_DEPEND_ADD		1	/* Add to the  dependencies mask */
#define RES_DEPEND_REMOVE	-1	/* Remove from the dependencies mask */

/* Change resource dependencies masks */
typedef struct {
	uint32 res_mask;		/* resources (chip specific) */
	int8 action;			/* action, e.g. RES_DEPEND_SET */
	uint32 depend_mask;		/* changes to the dependencies mask */
	bool (*filter)(si_t *sih);	/* action is taken when filter is NULL or return TRUE */
} pmu_res_depend_t;

const pmu_res_depend_t *si_pmu_res_depend_table(si_t *sih, uint32 *pmu_res_depend_table_sz);

#define XTAL_FREQ_24000MHZ		24000
#define XTAL_FREQ_29985MHZ		29985
#define XTAL_FREQ_30000MHZ		30000
#define XTAL_FREQ_37400MHZ		37400
#define XTAL_FREQ_48000MHZ		48000
#define XTAL_FREQ_59970MHZ		59970
#define XTAL_FREQ_76800MHZ		76800
#define XTAL_FREQ_79960MHZ		79960

/* 'xf' values corresponding to the 'xf' definition in the PMU control register */
/* unclear why this enum contains '_960_' since the PMU prog guide says nothing about that */
enum xtaltab0_960 {
	XTALTAB0_960_12000K = 1,
	XTALTAB0_960_13000K,
	XTALTAB0_960_14400K,
	XTALTAB0_960_15360K,
	XTALTAB0_960_16200K,
	XTALTAB0_960_16800K,
	XTALTAB0_960_19200K,
	XTALTAB0_960_19800K,
	XTALTAB0_960_20000K,
	XTALTAB0_960_24000K, /* warning: unknown in PMU programmers guide. seems incorrect. */
	XTALTAB0_960_25000K,
	XTALTAB0_960_26000K,
	XTALTAB0_960_30000K,
	XTALTAB0_960_33600K, /* warning: unknown in PMU programmers guide. seems incorrect. */
	XTALTAB0_960_37400K,
	XTALTAB0_960_38400K, /* warning: unknown in PMU programmers guide. seems incorrect. */
	XTALTAB0_960_40000K,
	XTALTAB0_960_48000K, /* warning: unknown in PMU programmers guide. seems incorrect. */
	XTALTAB0_960_52000K,
	XTALTAB0_960_59970K
};

/* Indices into array pmu1_xtaltab0_960[]. Keep array and these defines synchronized. */
#define PMU1_XTALTAB0_960_12000K	0
#define PMU1_XTALTAB0_960_13000K	1
#define PMU1_XTALTAB0_960_14400K	2
#define PMU1_XTALTAB0_960_15360K	3
#define PMU1_XTALTAB0_960_16200K	4
#define PMU1_XTALTAB0_960_16800K	5
#define PMU1_XTALTAB0_960_19200K	6
#define PMU1_XTALTAB0_960_19800K	7
#define PMU1_XTALTAB0_960_20000K	8
#define PMU1_XTALTAB0_960_24000K	9
#define PMU1_XTALTAB0_960_25000K	10
#define PMU1_XTALTAB0_960_26000K	11
#define PMU1_XTALTAB0_960_30000K	12
#define PMU1_XTALTAB0_960_33600K	13
#define PMU1_XTALTAB0_960_37400K	14
#define PMU1_XTALTAB0_960_38400K	15
#define PMU1_XTALTAB0_960_40000K	16
#define PMU1_XTALTAB0_960_48000K	17
#define PMU1_XTALTAB0_960_52000K	18
#define PMU1_XTALTAB0_960_59970K	19
#define PMU1_XTALTAB0_960_76800K	20

/* Indices into array pmu1_xtaltab0_1292[]. Keep array and these defines synchronized. */
#define PMU1_XTALTAB0_1292_79920K	0

const pmu1_xtaltab0_t *si_pmu1_xtaltab0(si_t *sih);
const pmu1_xtaltab0_t *si_pmu1_xtaldef0(si_t *sih);

uint32 si_pmu1_pllfvco0(si_t *sih);
uint32 si_pmu1_pllfvco0_pll2(si_t *sih);
void si_pmu_ndiv_p_q(si_t *sih, uint32 *ndiv_p, uint32 *ndiv_q);
void si_pmu_armpll_freq_upd(si_t *sih, uint8 p1div, uint32 ndiv_int, uint32 ndiv_frac);
/* The return value 0 indicates to the caller to bail out */
bool si_pmu_armpll_upd(si_t *sih, osl_t *osh, pmuregs_t *pmu,
	uint32 xtal, bool upd, bool *write_en);
void si_pmu_ndiv_upd(si_t *sih, const pmu1_xtaltab0_t *xt);
uint32 si_pmu_mdiv(si_t *sih);
uint32 si_pmu_macdiv(si_t *sih);
int si_pmu_fast_lpo_locked(si_t *sih, osl_t *osh);
uint32 si_pmu_fll_dac_out(si_t *sih);
void si_pmu_wait_ht_avail(si_t *sih, pmuregs_t *pmu, uint32 ht_req);

void si_pmu_fis_ctrl_setup(si_t *sih, pmuregs_t *pmu);

void si_pmu_chip_doinit(si_t *sih, osl_t *osh);
void si_pmu_swreg_doinit(si_t *sih);

int si_pmu_soft_start_params(si_t *sih, uint32 bt_or_wl, uint *en_reg, uint32 *en_shift,
	uint32 *en_mask, uint32 *en_val, uint *val_reg, uint32 *val_shift, uint32 *val_mask);

int si_pmu_mem_pwr_dooff(si_t *sih, uint core_idx);
int si_pmu_mem_pwr_doon(si_t *sih);

void si_pmu_intr_pwrreq_disa(si_t *sih);
void si_pmu_lvm_csr_upd(si_t *sih, bool lvm);

void si_pmu_WAR_HW4389_836(si_t *sih);
void si_pmu_fll_preload_upd(si_t *sih);

#endif /* !BCMDONGLEHOST */

#endif /* _hndpmu_priv_h_ */
