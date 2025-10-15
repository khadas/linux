/*
 * DAP(Debug Access Port) interface.
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

#ifndef _hnddap_h_
#define _hnddap_h_
#ifdef BCM_DAP
#include <siutils.h>

enum etb {
	ETB0 = 0,
	ETB1 = 1,
	ETB2 = 2
};

#if defined(BCMCCREV) && ((BCMCCREV == 74) || (BCMCCREV == 76))
#define MAX_ETB 3
#else
#define MAX_ETB 1
#endif /* BCMCCREV */

void hnd_dap_attach(si_t *sih);
void hnd_dap_config_etm(si_t *sih);
void hnd_dap_clear_etm_tmc(si_t *sih);
void hnd_dap_flush_etm_tmc(si_t *sih);
void hnd_dap_flush_tmc(si_t *sih, uint8 etb, uint8 ccrev);
void hnd_dap_flush_sdtc_tmc(si_t *sih);
bool hnd_dap_is_sdtc_etb(uint8 etb);
void hnd_dap_config_sdtc(si_t *sih);
void hnd_dap_config_sdtc_etb(si_t *sih);
uint32 hnd_dap_get_rwp(osl_t *osh);
void hnd_dap_enable_sdtc_funnel(osl_t *osh, uint8 funnel_port, uint8 etb);
void hnd_dap_config_idfilter1(osl_t *osh, uint8 idfilter1, uint8 etb);
void hnd_dap_set_sdtc_pmucr(si_t *sih, uint8 etb, uint8 id);
void hnd_dap_set_sdtc_cc(si_t *sih, uint8 etb, uint8 trace_id);
void hnd_dap_config_axl_hwl_funnel(osl_t *osh, uint8 id);
void hnd_dap_etm_cxcpu_config_etb(si_t *sih);
int hnd_dap_regdump_collect(void *ptr, uint8 trap_type);
uint32 hnd_dap_handle_trap(void *ctx, uint8 trap_type);
uint32 hnd_dap_ewp_init(si_t *sih);
#else
#define hnd_dap_attach(sih)
#define hnd_dap_config_etm(sih)
#define hnd_dap_clear_etm_tmc(sih)
#define hnd_dap_flush_etm_tmc(sih)
#define hnd_dap_flush_tmc(a, b, c)
#define hnd_dap_flush_sdtc_tmc(a)
#define hnd_dap_config_sdtc(a)
#define hnd_dap_is_sdtc_etb(a)	0
#define hnd_dap_get_rwp(a)	0
#define hnd_dap_config_sdtc_etb(a)
#define hnd_dap_enable_sdtc_funnel(a, b, c)
#define hnd_dap_config_idfilter1(a, b, c)
#define hnd_dap_set_sdtc_pmucr(a, b, c)
#define hnd_dap_set_sdtc_cc(a, b, c)
#define hnd_dap_etm_coex_config_etb(sih)
#define hnd_dap_config_axl_hwl_funnel(a, b)
#define hnd_dap_regdump_collect(a, b) (BCME_ERROR)
#define hnd_dap_handle_trap(a, b) (0)
#define hnd_dap_ewp_init(a) (0)
#endif /* BCM_DAP */

#define DAP_TMC_ETMCPU_TRACEID			0x0u

/*
 * Redefining these offsets with ccrev prefixes as
 * its shared with DHD to reuse TMC flush code
 */
#define DAP_TMC0_OFFSET_CCREV_LT74              0x42000u
#define DAP_TMC0_OFFSET_CCREV_GE74              0x41000u
#define DAP_TMC1_OFFSET_CCREV_GE74              0x46000u
#define DAP_TMC2_OFFSET_CCREV_GE74              0x55000u

/* TMC registers */
typedef volatile struct tmcregs {
	uint32 PAD;
	uint32 rsz;
	uint32 PAD;
	uint32 sts;
	uint32 rrd;
	uint32 rrp;
	uint32 rwp;
	uint32 trg;
	uint32 ctl;
	uint32 rwd;
	uint32 mode;
	uint32 lbuflevel;
	uint32 cbuflevel;
	uint32 PAD[179];
	uint32 ffsr;
	uint32 ffcr;
	uint32 PAD[810];
	uint32 lar;
} tmcregs_t;

/* Register FFCR */
#define CORESIGHT_TMC_FFCR_STOPONFL_SHIFT	12u
#define CORESIGHT_TMC_FFCR_FLUSHMAN_SHIFT	6u

/* Register STS - status */
#define CORESIGHT_TMC_STS_FLUSHMAN_SHIFT	3u
#define CORESIGHT_TMC_STS_READY_SHIFT		2u

/* Magic word to access CoreSight registers */
#define CORESIGHT_UNLOCK	0xC5ACCE55u

/* ATB funnel registers */
typedef volatile struct funnelregs {
	uint32 ctrl_reg;
	uint32 pad[1003];
	uint32 lar;
} funnelregs_t;

/* Register Ctrl_Reg */
#define CORESIGHT_TMC_CTRL_REG_ENS0_SHIFT	0u
#define CORESIGHT_TMC_CTRL_REG_HT_SHIFT		8u
#define CORESIGHT_TMC_CTRL_REG_HT_MASK		(0b1111 << CORESIGHT_TMC_CTRL_REG_HT_SHIFT)

/* ATB replicator registers */
typedef volatile struct replicatorregs {
	uint32 idfilter0;
	uint32 idfilter1;
	uint32 PAD[1002];
	uint32 lar;
} replicatorregs_t;

#ifdef ARM_DEBUG_SECUREBOOT_WAR
extern bool _arm_etm_debug;
#define ARM_DEBUG_ENAB()	(_arm_etm_debug)
#else
#define ARM_DEBUG_ENAB()	(1)
#endif /* ARM_DEBUG_SECUREBOOT_WAR */
#endif /* _hnddap_h_ */
