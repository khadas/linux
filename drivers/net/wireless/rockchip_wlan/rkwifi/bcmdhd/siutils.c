/*
 * Misc utility routines for accessing chip-specific features
 * of the SiliconBackplane-based Broadcom chips.
 * Note: this file is used for both dongle and DHD builds.
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

#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <bcmdevs.h>
#if defined(BCMDONGLEHOST)
#include <bcmdevs_legacy.h>
#endif
#include <hndsoc.h>
#include <sbchipc.h>
#include <sbgci.h>
#ifndef BCMSDIO
#include <pcie_core.h>
#endif
#if !defined(BCMDONGLEHOST)
#include <nicpci.h>
#include <bcmnvram.h>
#include <bcmsrom.h>
#endif
#include <pcicfg.h>
#include <sbpcmcia.h>
#include <sbsysmem.h>
#include <sbsocram.h>
#if defined(BCMECICOEX)
#include <bcmotp.h>
#endif /* BCMECICOEX */
#ifdef BCMSDIO
#include <bcmsdh.h>
#include <sdio.h>
#include <sbsdio.h>
#include <sbhnddma.h>
#include <sbsdpcmdev.h>
#include <bcmsdpcm.h>
#endif /* BCMSDIO */
#ifdef BCMSPI
#include <spid.h>
#endif /* BCMSPI */

#ifdef BCM_SDRBL
#include <hndcpu.h>
#endif /* BCM_SDRBL */
#ifdef HNDGCI
#include <hndgci.h>
#endif /* HNDGCI */
#ifdef DONGLEBUILD
#include <hnd_gci.h>
#include <hndjtag.h>
#endif /* DONGLEBUILD */
#include <hndlhl.h>
#include <hndoobr.h>
#include <lpflags.h>
#ifdef BCM_SFLASH
#include <sflash.h>
#endif
#ifdef BCM_SH_SFLASH
#include <sh_sflash.h>
#endif
#ifdef BCMGCISHM
#include <hnd_gcishm.h>
#endif
#include "siutils_priv.h"
#include "sbhndarm.h"
#ifdef SOCI_NCI_BUS
#include <nci.h>
#endif /* SOCI_NCI_BUS */
#ifdef BCMSPMIS
#include <bcmspmi.h>
#endif /* BCMSPMIS */

#ifdef BCM_MMU_DEVMEM_PROTECT
#include <rte_mmu.h>
#endif /* BCM_MMU_DEVMEM_PROTECT */

#ifdef HND_DVFS_REORG
#include <hnd_dvfs.h>
#endif /* HND_DVFS_REORG */

/* minimum corerevs supported by firmware */
#define BCM_MIN_SUPPORTED_COREREV_CHIPCOMMON	68
#define BCM_MIN_SUPPORTED_COREREV_ACPHY_MAIN	50
#define BCM_MIN_SUPPORTED_COREREV_MAC		85
#define BCM_MIN_SUPPORTED_COREREV_GCI		18
#define BCM_MIN_SUPPORTED_COREREV_PCIEGEN2	69
#define BCM_MIN_SUPPORTED_COREREV_PMU		38

#ifndef AXI_TO_VAL
#define AXI_TO_VAL 19
#endif	/* AXI_TO_VAL */

#ifndef AXI_TO_VAL_25
/*
 * Increase BP timeout for fast clock and short PCIe timeouts
 * New timeout: 2 ** 25 cycles
 */
#define AXI_TO_VAL_25	25
#endif /* AXI_TO_VAL_25 */

/* local prototypes */
static int32 BCMATTACHFN(si_alloc_wrapper)(si_info_t *sii);
static bool si_buscore_setup(si_info_t *sii, chipcregs_t *cc, uint bustype, uint32 savewin,
	uint *origidx, volatile const void *regs);

#ifdef BCM_MMU_DEVMEM_PROTECT
void si_core_devmem_protect(const si_t *sih, bool set);
#endif /* BCM_MMU_DEVMEM_PROTECT */

/* global variable to indicate reservation/release of gpio's */
static uint32 si_gpioreservation;

#ifdef SR_DEBUG
static const uint32 si_power_island_test_array[] = {
	0x0000, 0x0001, 0x0010, 0x0011,
	0x0100, 0x0101, 0x0110, 0x0111,
	0x1000, 0x1001, 0x1010, 0x1011,
	0x1100, 0x1101, 0x1110, 0x1111
};
#endif /* SR_DEBUG */

/* global kernel resource */
si_info_t ksii;
si_cores_info_t ksii_cores_info;

uint32	wd_msticks;		/**< watchdog timer ticks normalized to ms */

/** Returns the backplane address of the chipcommon core for a particular chip */
uint32
BCMATTACHFN(si_enum_base)(uint devid)
{
	return SI_ENUM_BASE_DEFAULT;
}

/**
 * Allocate an si handle. This function may be called multiple times.
 *
 * devid - pci device id (used to determine chip#)
 * osh - opaque OS handle
 * regs - virtual address of initial core registers
 * bustype - pci/sb/sdio/etc
 * vars - pointer to a to-be created pointer area for "environment" variables. Some callers of this
 *        function set 'vars' to NULL, making dereferencing of this parameter undesired.
 * varsz - pointer to int to return the size of the vars
 */
si_t *
BCMATTACHFN(si_attach)(uint devid, osl_t *osh, volatile void *regs,
	uint bustype, void *sdh, char **vars, uint *varsz)
{
	si_info_t *sii;

	SI_MSG_DBG_REG(("%s: Enter\n", __FUNCTION__));
	/* alloc si_info_t */
	/* freed after ucode download for firmware builds */
	sii = MALLOCZ_NOPERSIST(osh, sizeof(si_info_t));
	if (sii == NULL) {
		SI_ERROR(("si_attach: malloc failed! malloced %d bytes\n", MALLOCED(osh)));
		return NULL;
	}

#ifdef BCMDVFS
	if (BCMDVFS_ENAB() && si_dvfs_info_init((si_t *)sii, osh) == NULL) {
		SI_ERROR(("si_dvfs_info_init failed\n"));
		MFREE(osh, sii, sizeof(si_info_t));
		return NULL;
	}
#endif /* BCMDVFS */

	if (si_buscore_init(sii, devid, osh, regs, bustype, sdh) != BCME_OK) {
		MFREE(osh, sii, sizeof(si_info_t));
		return NULL;
	}

	if (si_doattach(sii, devid, osh, regs, bustype, sdh, vars, varsz) == NULL) {
		MFREE(osh, sii, sizeof(si_info_t));
		return NULL;
	}
	sii->vars = vars ? *vars : NULL;
	sii->varsz = varsz ? *varsz : 0;

#if defined(BCM_SH_SFLASH) && !defined(BCM_SH_SFLASH_DISABLED)
	sh_sflash_attach(osh, (si_t *)sii);
#endif

	/* ensure minimum corerevs supported by the build */
#ifdef VLSI_CHIP_DEFS_H	/* we have corerevs generated from vlsi_data */
	STATIC_ASSERT(BCMCHIPCOMMONREV >= BCM_MIN_SUPPORTED_COREREV_CHIPCOMMON);
	STATIC_ASSERT(BCMDOT11ACPHY_MAINREV >= BCM_MIN_SUPPORTED_COREREV_ACPHY_MAIN);
	STATIC_ASSERT(BCMDOT11MACREV >= BCM_MIN_SUPPORTED_COREREV_MAC);
	STATIC_ASSERT(BCMGCIREV >= BCM_MIN_SUPPORTED_COREREV_GCI);
	STATIC_ASSERT(BCMPCIEGEN2REV >= BCM_MIN_SUPPORTED_COREREV_PCIEGEN2);
	STATIC_ASSERT(BCMPMUREV >= BCM_MIN_SUPPORTED_COREREV_PMU);
#endif

	SI_MSG_DBG_REG(("%s: Exit\n", __FUNCTION__));
	return (si_t *)sii;
}

static bool
BCMATTACHFN(si_buscore_setup)(si_info_t *sii, chipcregs_t *cc, uint bustype, uint32 savewin,
	uint *origidx, volatile const void *regs)
{
	const si_cores_info_t *cores_info = sii->cores_info;
	bool pci, pcie, pcie_gen2 = FALSE;
	uint i;
	uint pciidx, pcieidx, pcirev, pcierev;

#if defined(AXI_TIMEOUTS)
	/* first, enable backplane timeouts */
	si_slave_wrapper_add(&sii->pub);
#endif
	sii->curidx = 0;

	cc = si_setcoreidx(&sii->pub, SI_CC_IDX);
	ASSERT((uintptr)cc);

	/* get chipcommon rev */
	sii->pub.ccrev = (int)si_corerev(&sii->pub);

	/* get chipcommon chipstatus */
	sii->pub.chipst = R_REG(sii->osh, CC_REG_ADDR(cc, ChipStatus));

	/* get chipcommon capabilites */
	sii->pub.cccaps = R_REG(sii->osh, CC_REG_ADDR(cc, CoreCapabilities));
	/* get chipcommon extended capabilities */

	sii->pub.cccaps_ext = R_REG(sii->osh, CC_REG_ADDR(cc, CapabilitiesExtension));

	/* get pmu rev and caps */
	if (sii->pub.cccaps & CC_CAP_PMU) {
		if (AOB_ENAB(&sii->pub)) {
			uint pmucoreidx;
			pmuregs_t *pmu;

			pmucoreidx = si_findcoreidx(&sii->pub, PMU_CORE_ID, 0);
			if (!GOODIDX(pmucoreidx, sii->numcores)) {
				SI_ERROR(("si_buscore_setup: si_findcoreidx failed\n"));
				return FALSE;
			}

			pmu = si_setcoreidx(&sii->pub, pmucoreidx);
			sii->pub.pmucaps = R_REG(sii->osh, PMU_REG_ADDR(pmu, CoreCapabilities));
			si_setcoreidx(&sii->pub, SI_CC_IDX);

			sii->pub.gcirev = si_corereg(&sii->pub, GCI_CORE_IDX(&sii->pub),
				GCI_OFFSETOF(&sii->pub, gci_corecaps0), 0, 0) & GCI_CAP0_REV_MASK;

			sii->pub.lhlrev = si_corereg(&sii->pub, GCI_CORE_IDX(&sii->pub),
				OFFSETOF(gciregs_t, lhl_core_capab_adr), 0, 0) & LHL_CAP_REV_MASK;

		} else {
			sii->pub.pmucaps = R_REG(sii->osh, PMU_REG_ADDR(cc, CoreCapabilities));
		}
		sii->pub.pmurev = sii->pub.pmucaps & PCAP_REV_MASK;
	}

	SI_MSG(("Chipc: rev %d, caps 0x%x, chipst 0x%x pmurev %d, pmucaps 0x%x\n",
		CCREV(sii->pub.ccrev), sii->pub.cccaps, sii->pub.chipst, sii->pub.pmurev,
		sii->pub.pmucaps));

	/* figure out bus/orignal core idx */
	/* note for PCI_BUS the buscoretype variable is setup in ai_scan() */
	if (BUSTYPE(sii->pub.bustype) != PCI_BUS) {
		sii->pub.buscoretype = NODEV_CORE_ID;
	}
	sii->pub.buscorerev = NOREV;
	sii->pub.buscoreidx = BADIDX;

	pci = pcie = FALSE;
	pcirev = pcierev = NOREV;
	pciidx = pcieidx = BADIDX;

	/* This loop can be optimized */
	for (i = 0; i < sii->numcores; i++) {
		uint cid, crev;

		si_setcoreidx(&sii->pub, i);
		cid = si_coreid(&sii->pub);
		crev = si_corerev(&sii->pub);

		/* Display cores found */
		if (CHIPTYPE(sii->pub.socitype) != SOCI_NCI) {
			SI_VMSG(("CORE[%d]: id 0x%x rev %d base 0x%x size:%x regs 0x%p\n",
				i, cid, crev, cores_info->coresba[i], cores_info->coresba_size[i],
				OSL_OBFUSCATE_BUF(cores_info->regs[i])));
		}

		if (BUSTYPE(bustype) == SI_BUS) {
			/* now look at the chipstatus register to figure the pacakge */
			/* this shoudl be a general change to cover all teh chips */
			/* this also shoudl validate the build where the dongle is built */
			/* for SDIO but downloaded on PCIE dev */
#ifdef BCMPCIEDEV_ENABLED
			if (cid == PCIE2_CORE_ID) {
				pcieidx = i;
				pcierev = crev;
				pcie = TRUE;
				pcie_gen2 = TRUE;
			}
#endif
			/* rest fill it up here */

		} else if (BUSTYPE(bustype) == PCI_BUS) {
			if (cid == PCI_CORE_ID) {
				pciidx = i;
				pcirev = crev;
				pci = TRUE;
			} else if ((cid == PCIE_CORE_ID) || (cid == PCIE2_CORE_ID)) {
				pcieidx = i;
				pcierev = crev;
				pcie = TRUE;
				if (cid == PCIE2_CORE_ID)
					pcie_gen2 = TRUE;
			}
		}
#ifdef BCMSDIO
		else if (((BUSTYPE(bustype) == SDIO_BUS) ||
			(BUSTYPE(bustype) == SPI_BUS)) &&
			(cid == SDIOD_CORE_ID)) {
			sii->pub.buscorerev = (int16)crev;
			sii->pub.buscoretype = (uint16)cid;
			sii->pub.buscoreidx = (uint16)i;
		}
#endif /* BCMSDIO */
		if (cid == SMB_CORE_ID) {
			sii->smb.present = TRUE;
			sii->smb.rev = crev;
			sii->smb.base_addr =
				si_addrspace(&sii->pub, CORE_SLAVE_PORT_1, CORE_BASE_ADDR_0);
			sii->smb.size =
				si_addrspacesize(&sii->pub, CORE_SLAVE_PORT_1, CORE_BASE_ADDR_0);
			SI_MSG(("SMB: rev %d base 0x%08x size %u\n", sii->smb.rev,
				sii->smb.base_addr, sii->smb.size));
		}

		/* find the core idx before entering this func. */
		if (CHIPTYPE(sii->pub.socitype) == SOCI_NCI) {
			if (regs == sii->curmap) {
				*origidx = i;
			}
		} else {
			/* find the core idx before entering this func. */
			if ((savewin && (savewin == cores_info->coresba[i])) ||
			(regs == cores_info->regs[i])) {
				*origidx = i;
			}
		}
	}

#if !defined(BCMDONGLEHOST)
	if (pci && pcie) {
		if (si_ispcie(&sii->pub))
			pci = FALSE;
		else
			pcie = FALSE;
	}
#endif /* !defined(BCMDONGLEHOST) */

#if defined(PCIE_FULL_DONGLE)
	if (pcie) {
		if (pcie_gen2)
			sii->pub.buscoretype = PCIE2_CORE_ID;
		else
			sii->pub.buscoretype = PCIE_CORE_ID;
		sii->pub.buscorerev = (int16)pcierev;
		sii->pub.buscoreidx = (uint16)pcieidx;
	}
	BCM_REFERENCE(pci);
	BCM_REFERENCE(pcirev);
	BCM_REFERENCE(pciidx);
#else
	if (pci) {
		sii->pub.buscoretype = PCI_CORE_ID;
		sii->pub.buscorerev = (int16)pcirev;
		sii->pub.buscoreidx = (uint16)pciidx;
	} else if (pcie) {
		if (pcie_gen2)
			sii->pub.buscoretype = PCIE2_CORE_ID;
		else
			sii->pub.buscoretype = PCIE_CORE_ID;
		sii->pub.buscorerev = (int16)pcierev;
		sii->pub.buscoreidx = (uint16)pcieidx;
	}
#endif /* defined(PCIE_FULL_DONGLE) */

	SI_VMSG(("Buscore id/type/rev %d/0x%x/%d\n", sii->pub.buscoreidx, sii->pub.buscoretype,
		sii->pub.buscorerev));

#if !defined(BCMDONGLEHOST)
	/* fixup necessary chip/core configurations */
	if (BUSTYPE(sii->pub.bustype) == PCI_BUS) {
		if (SI_FAST(sii)) {
			if (!sii->pch) {
				sii->pch = (void *)(uintptr)pcicore_init(&sii->pub, sii->osh,
					(volatile void *)PCIEREGS(sii));
				if (sii->pch == NULL)
					return FALSE;
			}
		}
	}
#endif /* !defined(BCMDONGLEHOST) */

#if defined(BCMSDIO) && defined(BCMDONGLEHOST)
	/* Make sure any on-chip ARM is off (in case strapping is wrong), or downloaded code was
	 * already running.
	 */
	if ((BUSTYPE(bustype) == SDIO_BUS) || (BUSTYPE(bustype) == SPI_BUS)) {
		if (si_setcore(&sii->pub, ARM7S_CORE_ID, 0) ||
		    si_setcore(&sii->pub, ARMCM3_CORE_ID, 0))
			si_core_disable(&sii->pub, 0);
	}
#endif /* BCMSDIO && BCMDONGLEHOST */

	if (si_setcore(&sii->pub, SDTC_CORE_ID, 0) != NULL) {
		sii->pub.sdtcrev = (int16)si_corerev(&sii->pub);
	}

	/* return to the original core */
	si_setcoreidx(&sii->pub, *origidx);

	return TRUE;
}

#if defined(CONFIG_XIP) && defined(BCMTCAM)
extern uint8 patch_pair;
#endif /* CONFIG_XIP && BCMTCAM */

/**
 * A given GCI pin needs to be converted to a GCI FunctionSel register offset and the bit position
 * in this register.
 * @param[in]  input   pin number, see respective chip Toplevel Arch page, GCI chipstatus regs
 * @param[out] regidx  chipcontrol reg(ring_index base) and
 * @param[out] pos     bits to shift for pin first regbit
 *
 * eg: gpio9 will give regidx: 1 and pos 4
 */
void
BCMPOSTTRAPFN(si_gci_get_chipctrlreg_ringidx_base4)(uint32 pin, uint32 *regidx, uint32 *pos)
{
	*regidx = (pin / 8);
	*pos = (pin % 8) * 4; // each pin occupies 4 FunctionSel register bits

	SI_MSG(("si_gci_get_chipctrlreg_ringidx_base4:%d:%d:%d\n", pin, *regidx, *pos));
}

/** setup a given pin for fnsel function */
void
BCMPOSTTRAPFN(si_gci_set_functionsel)(si_t *sih, uint32 pin, uint8 fnsel)
{
	uint32 reg = 0, pos = 0;

	SI_MSG(("si_gci_set_functionsel:%d\n", pin));

	si_gci_get_chipctrlreg_ringidx_base4(pin, &reg, &pos);
	si_gci_chipcontrol(sih, reg, GCIMASK_4B(pos), GCIPOSVAL_4B(fnsel, pos));
}

/* Returns a given pin's fnsel value */
uint32
si_gci_get_functionsel(si_t *sih, uint32 pin)
{
	uint32 reg = 0, pos = 0, temp;

	SI_MSG(("si_gci_get_functionsel: %d\n", pin));

	si_gci_get_chipctrlreg_ringidx_base4(pin, &reg, &pos);
	temp = si_gci_chipstatus(sih, reg);
	return GCIGETNBL(temp, pos);
}

/* Sets fnsel value to IND for all the GPIO pads that have fnsel set to given argument */
void
si_gci_clear_functionsel(si_t *sih, uint8 fnsel)
{
	uint32 i;
	SI_MSG(("si_gci_clear_functionsel: %d\n", fnsel));
	for (i = 0; i <= CC_PIN_GPIO_LAST; i++)	{
		if (si_gci_get_functionsel(sih, i) == fnsel)
			si_gci_set_functionsel(sih, i, CC_FNSEL_IND);
	}
}

/* The code under "#if !(defined(VLSI_CTRL_REGS)) ... #endif" below
 * is aplicable only for chips which do not support vlsi2sw flow:
 * BCM4387, BCM4389. This code needs to be deleted once
 * the trunk support for these chips gets deprecated
 */
#if !defined(VLSI_CTRL_REGS)
/* reg, field_mask, reg_shift */
static CONST gci_cc_map_vlsi2sw_to_legacy_t BCMPOST_TRAP_RODATA(gci_cc_map)[] = {
	{CC_GCI_CHIPCTRL_09, 0x3ffu, 16u}, /* clb_swctrl_smask_coresel_ant0 = 0u */
	{CC_GCI_CHIPCTRL_10, 0x3ffu, 20u}, /* clb_swctrl_smask_coresel_ant1 = 1u */
	{CC_GCI_CHIPCTRL_11, 0x3u, 26u}, /* btcx_prisel_ant_mask_ovr = 2u */
	{CC_GCI_CHIPCTRL_23, 0x1u, 16u}, /* main_wlsc_prisel_force = 3u */
	{CC_GCI_CHIPCTRL_23, 0x1u, 17u}, /* main_wlsc_prisel_force_val = 4u */
	{CC_GCI_CHIPCTRL_23, 0x1u, 24u}, /* btmain_btsc_prisel_force = 5u */
	{CC_GCI_CHIPCTRL_23, 0x1u, 25u}, /* btmain_btsc_prisel_force_val = 6u */
	{CC_GCI_CHIPCTRL_23, 0x1u, 20u}, /* wlsc_btsc_prisel_force = 7u */
	{CC_GCI_CHIPCTRL_23, 0x1u, 21u}, /* wlsc_btsc_prisel_force_val = 8u */
	{CC_GCI_CHIPCTRL_23, 0x1u, 22u}, /* wlsc_btmain_prisel_force = 9u */
	{CC_GCI_CHIPCTRL_23, 0x1u, 23u}, /* wlsc_btmain_prisel_force_val = 10u */
	{CC_GCI_CHIPCTRL_23, 0x1u, 18u}, /* aux_wlsc_prisel_force = 11u */
	{CC_GCI_CHIPCTRL_23, 0x1u, 19u}, /* aux_wlsc_prisel_force_val = 12u */
	{CC_GCI_CHIPCTRL_11, 0x1u, 25u}, /* bt_only_force_wl_coex_iso = 13u */
	{CC_GCI_CHIPCTRL_08, 0x3u, 27u}, /* btcx_prisel_mask = 14u */
	{CC_GCI_CHIPCTRL_17, 0x3FFu, 0u}, /* clb_swctrl_smask_scan_core0sel = 15u */
	{CC_GCI_CHIPCTRL_18, 0x3FFu, 0u}, /* clb_swctrl_smask_btsc = 16u */
	{CC_GCI_CHIPCTRL_18, 0x3FFu, 16u}, /* clb_swctrl_smask_wlsc = 17u */
	{CC_GCI_CHIPCTRL_19, 0x3FFu, 0u}, /* clb_swctrl_smask_fprime0 = 18u */
	{CC_GCI_CHIPCTRL_20, 0x3FFu, 0u}, /* clb_swctrl_smask_fscan0 = 19u */
	{CC_GCI_CHIPCTRL_17, 0x3FFu, 16u}, /* clb_swctrl_smask_scan_core1sel = 20u */
	{CC_GCI_CHIPCTRL_19, 0x3FFu, 16u}, /* clb_swctrl_smask_fprime1 = 21u */
	{CC_GCI_CHIPCTRL_20, 0x3FFu, 16u}, /* clb_swctrl_smask_fscan1 = 22u */
	{CC_GCI_CHIPCTRL_08, 0x1u, 29u}, /* clb_swctrl_smask_coresel_ant0_en = 23u */
	{CC_GCI_CHIPCTRL_08, 0x1u, 30u}, /* clb_swctrl_smask_coresel_ant1_en = 24u */
	{CC_GCI_CHIPCTRL_08, 0x3ffu, 17u}, /* clb_swctrl_dmask_bt_ant0 = 25u */
	{CC_GCI_CHIPCTRL_07, 0x3u, 18u}, /* bt_aoa_ant_mask = 26u */
	{CC_GCI_CHIPCTRL_07, 0x3ffu, 20u}, /* clb_swctrl_smask_wlan_ant0 = 27u */
	{CC_GCI_CHIPCTRL_10, 0x3ffu, 10u}, /* clb_swctrl_dmask_bt_ant1 = 28u */
	{CC_GCI_CHIPCTRL_11, 0x3ffu, 0u}, /* clb_swctrl_smask_wlan_ant1 = 29u */
	{CC_GCI_CHIPCTRL_08, 0x1u, 31u}, /* global_debug_soft_reset = 30u */
	{CC_GCI_CHIPCTRL_25, 0xfu, 0u}, /* xtal_acl_ibias_ctrl_normal_hq = 31u */
	{CC_GCI_CHIPCTRL_25, 0x3u, 4u}, /* xtal_acl_ibias_startup_ctrl_normal_hq = 32u */
	{CC_GCI_CHIPCTRL_25, 0x7u, 11u}, /* xtal_ref_ibias_acl_ctrl_normal_hq = 33u */
	{CC_GCI_CHIPCTRL_26, 0xfu, 0u}, /* xtal_acl_ibias_ctrl_normal_lq = 34u */
	{CC_GCI_CHIPCTRL_26, 0x1fu, 6u}, /* xtal_acl_vref_ctrl_rladder_normal_lq = 35u */
	{CC_GCI_CHIPCTRL_28, 0x1u, 24u}, /* phy_1x1_scan_ihrp_sel = 36u */
	{CC_GCI_CHIPCTRL_28, 0x1u, 25u}, /* phy_2x2_bw20_ihrp_sel = 37u */
	{CC_GCI_CHIPCTRL_28, 0x1u, 26u}, /* phy_2x2_bw80_ihrp_sel = 38u */
	{CC_GCI_CHIPCTRL_27, 0xfu, 20u}, /* xtal_vbuck_ctrl_rladder_normal_lq = 39u */
	{CC_GCI_CHIPCTRL_27, 0x3u, 26u}, /* xtal_reset_delay_normal_hq = 40u */
	{CC_GCI_CHIPCTRL_27, 0xfu, 28u}, /* xtal_vbuck_ctrl_rladder_startup = 41u */
	{CC_GCI_CHIPCTRL_28, 0xfu, 3u}, /* xtal_vbuck_ctrl_rladder_normal_hq = 42u */
	{CC_GCI_CHIPCTRL_13, 0x1u, 31u}, /* ext_pmcr_clkreqb_in = 43u */
	{CC_GCI_CHIPCTRL_07, 0xfu, 2u}, /* bt2clb_swctrl_bt_default_value = 44u */
	{0u, 0u, 0u}, /* nci_error_immediate_en = 45u */
	{CC_GCI_CHIPCTRL_15, 0x1u, 29u}, /* rffe_clk_en = 46u */
	{CC_GCI_CHIPCTRL_15, 0x1u, 30u}, /* rffe_clk_force = 47u */
	{CC_GCI_CHIPCTRL_23, 0x3u, 29u}, /* rffe_sdata_pdn = 48u */
	{CC_GCI_CHIPCTRL_16, 0x1u, 8u}, /* btcx_prisel_ant_mask_ovr_disable = 49u */
	{CC_GCI_CHIPCTRL_23, 0x1u, 26u}, /* otp_lvm_mode = 50u */
	{CC_GCI_CHIPCTRL_03, 0xfu, 0u}, /* maincore_rdy_2gci_sel = 51u */
	{CC_GCI_CHIPCTRL_03, 0xfu, 4u} /* auxcore_rdy_2gci_sel = 52u */
};

static CONST uint32 BCMPOST_TRAP_RODATA(gci_cc_map_len) = sizeof(gci_cc_map)/sizeof(gci_cc_map[0]);

uint32
BCMPOSTTRAPFN(si_gci_chipcontrol_rd_api)(si_t *sih,
	gci_cc_map_vlsi2sw_to_legacy_idx_t field)
{
	const gci_cc_map_vlsi2sw_to_legacy_t *p_gci_cc_map;
	uint8 reg, reg_shift;
	uint32 field_mask, val;

	/* retaining the below print as the migration to new API is still WIP */
	//posttrap_printf("[si_gci_chipcontrol_rd_api]: field %d"
	//	" gci_cc_map_len: %d \n", field, gci_cc_map_len);

	BCM_REFERENCE(gci_cc_map_len);
	ASSERT((uint32)field < gci_cc_map_len);
	p_gci_cc_map = &gci_cc_map[(uint32)field];

	reg = p_gci_cc_map->reg;
	field_mask = p_gci_cc_map->field_mask;
	reg_shift = p_gci_cc_map->reg_shift;

	/* Handling registers that do not exist for some chips */
	if (field_mask == 0u) {
		return 0u;
	}

	val = si_gci_chipcontrol(sih, reg, 0u, 0u);
	val = (val >> reg_shift) & field_mask;

	/* retaining the below print as the migration to new API is still WIP */
	//posttrap_printf("[si_gci_chipcontrol_rd_api]: reg %d "
	//	"field_mask: 0x%x reg_shift: %d val: 0x%x\n",
	//	reg, field_mask, reg_shift, val);
	return val;
}

uint32
BCMPOSTTRAPFN(si_gci_chipcontrol_wr_api)(si_t *sih,
	gci_cc_map_vlsi2sw_to_legacy_idx_t field, uint32 val)
{
	const gci_cc_map_vlsi2sw_to_legacy_t *p_gci_cc_map;
	uint8 reg, reg_shift;
	uint32 field_mask, ret_val;

	/* retaining the below print as the migration to new API is still WIP */
	//posttrap_printf("[si_gci_chipcontrol_rd_api]: field %d"
	//	" gci_cc_map_len: %d \n", field, gci_cc_map_len);

	ASSERT((uint32)field < gci_cc_map_len);
	BCM_REFERENCE(gci_cc_map_len);
	p_gci_cc_map = &gci_cc_map[(uint32)field];

	reg = p_gci_cc_map->reg;
	field_mask = p_gci_cc_map->field_mask;
	reg_shift = p_gci_cc_map->reg_shift;

	/* Handling registers that do not exist for some chips */
	if (field_mask == 0u) {
		return 0u;
	}

	ret_val = si_gci_chipcontrol(sih, reg, field_mask << reg_shift, val << reg_shift);

	/* retaining the below print as the migration to new API is still WIP */
	//posttrap_printf("[si_gci_chipcontrol_wr_api]: reg %d"
	//	" field_mask: 0x%x reg_shift: %d val: 0x%x\n",
	//	reg, field_mask, reg_shift, ret_val);

	return ret_val;
}
#endif /* !(defined(VLSI_CTRL_REGS)) */

/** write 'val' to the gci chip control register indexed by 'reg' */
uint32
BCMPOSTTRAPFN(si_gci_chipcontrol)(si_t *sih, uint reg, uint32 mask, uint32 val)
{
	si_corereg(sih, GCI_CORE_IDX(sih), GCI_OFFSETOF(sih, gci_indirect_addr), ~0, reg);
	return si_corereg(sih, GCI_CORE_IDX(sih), GCI_OFFSETOF(sih, gci_chipctrl), mask, val);
}

/* Read the gci chip status register indexed by 'reg' */
uint32
BCMPOSTTRAPFN(si_gci_chipstatus)(si_t *sih, uint reg)
{
	si_corereg(sih, GCI_CORE_IDX(sih), GCI_OFFSETOF(sih, gci_indirect_addr), ~0, reg);
	/* setting mask and value to '0' to use si_corereg for read only purpose */
	return si_corereg(sih, GCI_CORE_IDX(sih), GCI_OFFSETOF(sih, gci_chipsts), 0, 0);
}

/* USED BY DHD AND FW! */
void
sflash_gpio_config(si_t *sih)
{
	ASSERT(sih);

	if (!si_is_sflash_available(sih)) {
		return;
	}

	switch (CHIPID((sih)->chip)) {
	case BCM4383_CHIP_GRPID:
	case BCM4384_CHIP_GRPID:
	case BCM4387_CHIP_GRPID:
	case BCM4388_CHIP_GRPID:
		/* set funsel 8 to 4387/4388 sflash gpio lines 12,15,18,19 */
		si_gci_set_functionsel(sih, CC_PIN_GPIO_12, CC_FNSEL_8);
		si_gci_set_functionsel(sih, CC_PIN_GPIO_15, CC_FNSEL_8);
		si_gci_set_functionsel(sih, CC_PIN_GPIO_18, CC_FNSEL_8);
		si_gci_set_functionsel(sih, CC_PIN_GPIO_19, CC_FNSEL_8);
		break;
	default:
		break;
	}

	return;
}

uint16
BCMINITFN(si_chipid)(const si_t *sih)
{
	const si_info_t *sii = SI_INFO(sih);

	return (sii->chipnew) ? sii->chipnew : sih->chip;
}

/* CHIP_ID's being mapped here should not be used anywhere else in the code */
static void
BCMATTACHFN(si_chipid_fixup)(si_t *sih)
{
	si_info_t *sii = SI_INFO(sih);

	BCM_REFERENCE(sii);
	ASSERT(sii->chipnew == 0);

	switch (sih->chip) {
	default:
		break;
	}
}

/* TODO: Can we allocate only one instance? */
static int32
BCMATTACHFN(si_alloc_wrapper)(si_info_t *sii)
{
	if (sii->osh) {
		sii->axi_wrapper = (axi_wrapper_t *)MALLOCZ(sii->osh,
			(sizeof(axi_wrapper_t) * SI_MAX_AXI_WRAPPERS));

		if (sii->axi_wrapper == NULL) {
			return BCME_NOMEM;
		}
	} else {
		sii->axi_wrapper = NULL;
		return BCME_ERROR;
	}
	return BCME_OK;
}

static void
BCMATTACHFN(si_free_wrapper)(si_info_t *sii)
{
	if (sii->axi_wrapper) {

		MFREE(sii->osh, sii->axi_wrapper, (sizeof(axi_wrapper_t) * SI_MAX_AXI_WRAPPERS));
	}
}

static void *
BCMATTACHFN(si_alloc_coresinfo)(si_info_t *sii, osl_t *osh, chipcregs_t *cc)
{
	if (CHIPTYPE(sii->pub.socitype) == SOCI_NCI) {
		sii->nci_info = nci_init(&sii->pub, (void *)(uintptr)cc, sii->pub.bustype);

		return sii->nci_info;

	} else {

#ifdef _RTE_
		sii->cores_info = (si_cores_info_t *)&ksii_cores_info;
#else
		if (sii->cores_info == NULL) {
			/* alloc si_cores_info_t */
			sii->cores_info = (si_cores_info_t *)MALLOCZ(osh,
				sizeof(si_cores_info_t));
			if (sii->cores_info == NULL) {
				SI_ERROR(("si_attach: malloc failed for cores_info! malloced"
					" %d bytes\n", MALLOCED(osh)));
				return NULL;
			}
		} else {
			ASSERT(sii->cores_info == &ksii_cores_info);

		}
#endif /* _RTE_ */
		return sii->cores_info;
	}

}

static void
BCMATTACHFN(si_free_coresinfo)(si_info_t *sii, osl_t *osh)
{

	if (CHIPTYPE(sii->pub.socitype) == SOCI_NCI) {
		if (sii->nci_info) {
			nci_uninit(sii->nci_info);
			sii->nci_info = NULL;
		}
	} else {
		if (sii->cores_info && (sii->cores_info != &ksii_cores_info)) {
			MFREE(osh, sii->cores_info, sizeof(si_cores_info_t));
		}
	}
}

int
BCMATTACHFN(si_buscore_init)(si_info_t *sii, uint devid, osl_t *osh, volatile void *regs,
	uint bustype, void *sdh)
{
	struct si_pub *sih = &sii->pub;
	uint32 w, savewin;
	chipcregs_t *cc;
	uint origidx;
	uint err_at = 0;

	ASSERT(GOODREGS(regs));

	savewin = 0;

	sih->buscoreidx = BADIDX;
	sii->device_removed = FALSE;

	sii->curmap = regs;
	sii->sdh = sdh;
	sii->osh = osh;
	sii->second_bar0win = ~0x0;
	sih->enum_base = si_enum_base(devid);

	/* check to see if we are a si core mimic'ing a pci core */
	if ((bustype == PCI_BUS) &&
	    (OSL_PCI_READ_CONFIG(sii->osh, PCI_SPROM_CONTROL, sizeof(uint32)) == 0xffffffff)) {
		SI_ERROR(("si_doattach: incoming bus is PCI but it's a lie, switching to SI "
			"devid:0x%x\n", devid));
		cc = (chipcregs_t *)REG_MAP(SI_ENUM_BASE(sih), SI_CORE_SIZE);
		bustype = SI_BUS;
	} else if (bustype == PCI_BUS) {
		/* find Chipcommon address */
		savewin = OSL_PCI_READ_CONFIG(sii->osh, PCI_BAR0_WIN, sizeof(uint32));
		/* PR 29857: init to core0 if bar0window is not programmed properly */
		if (!GOODCOREADDR(savewin, SI_ENUM_BASE(sih)))
			savewin = SI_ENUM_BASE(sih);
		OSL_PCI_WRITE_CONFIG(sii->osh, PCI_BAR0_WIN, 4, SI_ENUM_BASE(sih));
		if (!regs) {
			err_at = 1;
			goto exit;
		}
		cc = (chipcregs_t *)regs;
#ifdef BCMSDIO
	} else if ((bustype == SDIO_BUS) || (bustype == SPI_BUS)) {
		cc = (chipcregs_t *)sii->curmap;
#endif
	} else {
		cc = (chipcregs_t *)REG_MAP(SI_ENUM_BASE(sih), SI_CORE_SIZE);
	}

	sih->bustype = (uint16)bustype;
#ifdef BCMBUSTYPE
	if (bustype != BUSTYPE(bustype)) {
		SI_ERROR(("si_doattach: bus type %d does not match configured bus type %d\n",
			bustype, BUSTYPE(bustype)));
		err_at = 2;
		goto exit;
	}
#endif
	/* bus/core/clk setup for register access */
	if (!si_buscore_prep(sih, bustype, devid, sdh)) {
		SI_ERROR(("si_doattach: si_core_clk_prep failed %d\n", bustype));
		err_at = 3;
		goto exit;
	}

	/* ChipID recognition.
	*   We assume we can read chipid at offset 0 from the regs arg.
	*   If we add other chiptypes (or if we need to support old sdio hosts w/o chipcommon),
	*   some way of recognizing them needs to be added here.
	*/
	w = R_REG(osh, CC_REG_ADDR(cc, ChipID));
#if defined(BCMDONGLEHOST)
	/* plz refer to RB:13157 */
	if ((w & 0xfffff) == 148277) {
		w -= 65532;
	}
#endif /* defined(BCMDONGLEHOST) */
	sih->socitype = (w & CID_TYPE_MASK) >> CID_TYPE_SHIFT;
	/* Might as wll fill in chip id rev & pkg */
	sih->chip = w & CID_ID_MASK;
	sih->chiprev = (w & CID_REV_MASK) >> CID_REV_SHIFT;
	sih->chippkg = (w & CID_PKG_MASK) >> CID_PKG_SHIFT;

	si_chipid_fixup(sih);

	sih->issim = IS_SIM(sih->chippkg);

	if (MULTIBP_CAP(sih)) {
		sih->_multibp_enable = TRUE;
	}

	if (si_alloc_coresinfo(sii, osh, cc) == NULL) {
		err_at = 4;
		goto exit;
	}

	/* scan for cores */
	if (CHIPTYPE(sii->pub.socitype) == SOCI_NCI) {
		ASSERT(sii->nci_info);

		SI_MSG(("Found chip type NCI (0x%08x)\n", w));
		sii->numcores = nci_scan(sih);
		if (sii->numcores == 0u) {
			err_at = 6;
			goto exit;
		} else {
#ifndef BCM_BOOTLOADER
			nci_dump_erom(sii->nci_info);
#endif /* BCM_BOOTLOADER */
		}
	} else if (CHIPTYPE(sii->pub.socitype) == SOCI_AI) {
		bool erom_in_oobr = ai_erom_in_oobr(sii, (void *)(uintptr)cc);

		SI_MSG(("Found chip type AI (0x%08x) EROM from OOBR %d\n", w, erom_in_oobr));

		/* pass chipc address instead of original core base */
		if ((si_alloc_wrapper(sii)) != BCME_OK) {
			err_at = 8;
			goto exit;
		}

		/* AI Chips with erom in OOBR, parse erom with nci_scan()
		 * and convert nci->cores to sii->cores_info
		 */
		if (erom_in_oobr) {
			/* alloc nci->info for erom parsing from OOBR */
			sii->nci_info = nci_init(&sii->pub, (void *)(uintptr)cc, sii->pub.bustype);
			if (nci_scan(sih) == 0u) {
				err_at = 12;
				goto exit;
			}
			/* convert nci->cores to sii->cores_info */
			nci_cores_to_ai_cores(sih);

			/* release nci->info after erom parsing from OOBR */
			if (sii->nci_info) {
				nci_uninit(sii->nci_info);
				sii->nci_info = NULL;
			}
		} else {
			ai_scan(&sii->pub, (void *)(uintptr)cc, devid);
		}
		/* make sure the wrappers are properly accounted for */
		if (sii->axi_num_wrappers == 0) {
			SI_ERROR(("FATAL: Wrapper count 0\n"));
			err_at = 16;
			goto exit;
		}
	} else {
		SI_ERROR(("Found chip of unknown type (0x%08x)\n", w));
		err_at = 9;
		goto exit;
	}
	/* no cores found, bail out */
	if (sii->numcores == 0) {
		err_at = 10;
		goto exit;
	}
	/* bus/core/clk setup */
	origidx = SI_CC_IDX;
	if (!si_buscore_setup(sii, cc, bustype, savewin, &origidx, regs)) {
		err_at = 11;
		goto exit;
	}

	/* JIRA: SWWLAN-98321: SPROM read showing wrong values */
	/* Set the clkdiv2 divisor bits (2:0) to 0x4 if srom is present */
	if (bustype == SI_BUS) {
		uint32 clkdiv2, sromprsnt, capabilities, srom_supported;
		capabilities =	R_REG(osh, CC_REG_ADDR(cc, CoreCapabilities));
		srom_supported = capabilities & SROM_SUPPORTED;
		if (srom_supported) {
			sromprsnt = R_REG(osh, CC_REG_ADDR(cc, SpromCtrl));
			sromprsnt = sromprsnt & SROM_PRSNT_MASK;
			if (sromprsnt) {
				/* SROM clock come from backplane clock/div2. Must <= 1Mhz */
				clkdiv2 = (R_REG(osh, CC_REG_ADDR(cc, ClkDiv2)) & ~CLKD2_SROM);
				clkdiv2 |= CLKD2_SROMDIV_192;
				W_REG(osh, CC_REG_ADDR(cc, ClkDiv2), clkdiv2);
			}
		}
	}

	return BCME_OK;
exit:
#if !defined(BCMDONGLEHOST)
	if (BUSTYPE(sih->bustype) == PCI_BUS) {
		if (sii->pch)
			pcicore_deinit(sii->pch);
		sii->pch = NULL;
	}
#endif /* !defined(BCMDONGLEHOST) */

	if (err_at) {
		SI_ERROR(("si_buscore_init Failed. Error at %d\n", err_at));
		si_free_coresinfo(sii, osh);
		si_free_wrapper(sii);
	}
	return BCME_ERROR;
}

/** may be called with core in reset */
void
BCMATTACHFN(si_detach)(si_t *sih)
{
	si_info_t *sii = SI_INFO(sih);
	si_cores_info_t *cores_info = (si_cores_info_t *)sii->cores_info;
	uint idx;

#if !defined(BCMDONGLEHOST)
	struct si_pub *si_local = NULL;
	bcopy(&sih, &si_local, sizeof(si_t *));
#endif /* !BCMDONGLEHOST */

#ifdef BCM_SH_SFLASH
	if (BCM_SH_SFLASH_ENAB()) {
		sh_sflash_detach(sii->osh, sih);
	}
#endif
	if (BUSTYPE(sih->bustype) == SI_BUS) {
		if (CHIPTYPE(sii->pub.socitype) == SOCI_NCI) {
			if (sii->nci_info) {
				nci_uninit(sii->nci_info);
				sii->nci_info = NULL;

				/* TODO: REG_UNMAP */
			}
		} else {
			for (idx = 0; idx < SI_MAXCORES; idx++) {
				if (cores_info->regs[idx]) {
					REG_UNMAP(cores_info->regs[idx]);
					cores_info->regs[idx] = NULL;
				}
			}
		}
	}

#if !defined(BCMDONGLEHOST)
	srom_var_deinit(si_local);
	nvram_exit(si_local); /* free up nvram buffers */

	if (BUSTYPE(sih->bustype) == PCI_BUS) {
		if (sii->pch)
			pcicore_deinit(sii->pch);
		sii->pch = NULL;
	}
#endif /* !defined(BCMDONGLEHOST) */

	si_free_coresinfo(sii, sii->osh);
	si_free_wrapper(sii);

#ifdef BCMDVFS
	if (BCMDVFS_ENAB()) {
		si_dvfs_info_deinit(sih, sii->osh);
	}
#endif /* BCMDVFS */

	if (sii != &ksii) {
		MFREE(sii->osh, sii, sizeof(si_info_t));
	}
}

void *
BCMPOSTTRAPFN(si_osh)(si_t *sih)
{
	const si_info_t *sii;

	sii = SI_INFO(sih);
	return sii->osh;
}

void
si_setosh(si_t *sih, osl_t *osh)
{
	si_info_t *sii;

	sii = SI_INFO(sih);
	if (sii->osh != NULL) {
		SI_ERROR(("osh is already set....\n"));
		ASSERT(!sii->osh);
	}
	sii->osh = osh;
}

/** register driver interrupt disabling and restoring callback functions */
void
BCMATTACHFN(si_register_intr_callback)(si_t *sih, void *intrsoff_fn, void *intrsrestore_fn,
	void *intrsenabled_fn, void *intr_arg)
{
	si_info_t *sii = SI_INFO(sih);
	sii->intr_arg = intr_arg;
	sii->intrsoff_fn = (si_intrsoff_t)intrsoff_fn;
	sii->intrsrestore_fn = (si_intrsrestore_t)intrsrestore_fn;
	sii->intrsenabled_fn = (si_intrsenabled_t)intrsenabled_fn;
	/* save current core id.  when this function called, the current core
	 * must be the core which provides driver functions(il, et, wl, etc.)
	 */
	sii->dev_coreid = si_coreid(sih);
}

void
BCMPOSTTRAPFN(si_deregister_intr_callback)(si_t *sih)
{
	si_info_t *sii;

	sii = SI_INFO(sih);
	sii->intrsoff_fn = NULL;
	sii->intrsrestore_fn = NULL;
	sii->intrsenabled_fn = NULL;
}

uint
BCMPOSTTRAPFN(si_intflag)(si_t *sih)
{
	const si_info_t *sii = SI_INFO(sih);

	if (CHIPTYPE(sih->socitype) == SOCI_AI)
		return R_REG(sii->osh, ((uint32 *)(uintptr)
			    (sii->oob_router + OOB_STATUSA)));
	else if (CHIPTYPE(sih->socitype) == SOCI_NCI)
		return nci_intflag(sih);
	else {
		ASSERT(0);
		return 0;
	}
}

uint
si_flag(si_t *sih)
{
	if (CHIPTYPE(sih->socitype) == SOCI_AI)
		return ai_flag(sih);
	else if (CHIPTYPE(sih->socitype) == SOCI_NCI)
		return nci_flag(sih);
	else {
		ASSERT(0);
		return 0;
	}
}

uint
si_flag_alt(const si_t *sih)
{
	if (CHIPTYPE(sih->socitype) == SOCI_AI)
		return ai_flag_alt(sih);
	else if (CHIPTYPE(sih->socitype) == SOCI_NCI)
		return nci_flag_alt(sih);
	else {
		ASSERT(0);
		return 0;
	}
}

void
BCMATTACHFN(si_setint)(const si_t *sih, int siflag)
{
	if (CHIPTYPE(sih->socitype) == SOCI_AI)
		ai_setint(sih, siflag);
	else if (CHIPTYPE(sih->socitype) == SOCI_NCI)
		nci_setint(sih, siflag);
	else
		ASSERT(0);
}

uint32
si_oobr_baseaddr(const si_t *sih, bool second)
{
	const si_info_t *sii = SI_INFO(sih);

	if (CHIPTYPE(sih->socitype) == SOCI_AI)
		return (second ? sii->oob_router1 : sii->oob_router);
	else if (CHIPTYPE(sih->socitype) == SOCI_NCI)
		return nci_oobr_baseaddr(sih, second);
	else {
		ASSERT(0);
		return 0;
	}
}

uint
BCMPOSTTRAPFN(si_coreid)(const si_t *sih)
{
	const si_info_t *sii = SI_INFO(sih);
	const si_cores_info_t *cores_info = (const si_cores_info_t *)sii->cores_info;
	if (CHIPTYPE(sii->pub.socitype) == SOCI_NCI) {
		return nci_coreid(sih, sii->curidx);
	} else {
		return cores_info->coreid[sii->curidx];
	}
}

uint
BCMPOSTTRAPFN(si_coreidx)(const si_t *sih)
{
	const si_info_t *sii;

	sii = SI_INFO(sih);
	return sii->curidx;
}

uint
si_get_num_cores(const si_t *sih)
{
	const si_info_t *sii = SI_INFO(sih);
	return sii->numcores;
}

volatile void *
si_d11_switch_addrbase(si_t *sih, uint coreunit)
{
	return si_setcore(sih,  D11_CORE_ID, coreunit);
}

/** return the core-type instantiation # of the current core */
uint
si_coreunit(const si_t *sih)
{
	const si_info_t *sii = SI_INFO(sih);
	const si_cores_info_t *cores_info = (const si_cores_info_t *)sii->cores_info;
	uint idx;
	uint coreid;
	uint coreunit;
	uint i;

	if (CHIPTYPE(sii->pub.socitype) == SOCI_NCI) {
		return nci_coreunit(sih);
	}

	coreunit = 0;

	idx = sii->curidx;

	ASSERT(GOODREGS(sii->curmap));
	coreid = si_coreid(sih);

	/* count the cores of our type */
	for (i = 0; i < idx; i++)
		if (cores_info->coreid[i] == coreid)
			coreunit++;

	return coreunit;
}

uint
BCMATTACHFN(si_corevendor)(const si_t *sih)
{
	if (CHIPTYPE(sih->socitype) == SOCI_AI)
		return ai_corevendor(sih);
	else if (CHIPTYPE(sih->socitype) == SOCI_NCI)
		return nci_corevendor(sih);
	else {
		ASSERT(0);
		return 0;
	}
}

bool
BCMINITFN(si_backplane64)(const si_t *sih)
{
	return ((sih->cccaps & CC_CAP_BKPLN64) != 0);
}

uint
BCMPOSTTRAPFN(si_corerev)(const si_t *sih)
{
	if (CHIPTYPE(sih->socitype) == SOCI_AI)
		return ai_corerev(sih);
	else if (CHIPTYPE(sih->socitype) == SOCI_NCI)
		return nci_corerev(sih);
	else {
		ASSERT(0);
		return 0;
	}
}

uint
si_corerev_minor(const si_t *sih)
{
	if (CHIPTYPE(sih->socitype) == SOCI_AI) {
		return ai_corerev_minor(sih);
	} else if (CHIPTYPE(sih->socitype) == SOCI_NCI)
		return nci_corerev_minor(sih);
	else {
		return 0;
	}
}

/* return index of coreid or BADIDX if not found */
uint
BCMPOSTTRAPFN(si_findcoreidx)(const si_t *sih, uint coreid, uint coreunit)
{
	const si_info_t *sii = SI_INFO(sih);
	const si_cores_info_t *cores_info = (const si_cores_info_t *)sii->cores_info;
	uint found;
	uint i;

	if (CHIPTYPE(sih->socitype) == SOCI_NCI) {
		return nci_findcoreidx(sih, coreid, coreunit);
	}

	found = 0;

	for (i = 0; i < sii->numcores; i++) {
		if (cores_info->coreid[i] == coreid) {
			if (found == coreunit)
				return (i);
			found++;
		}
	}

	return (BADIDX);
}

bool
BCMPOSTTRAPFN(si_sysmem_present)(const si_t *sih)
{
	if (si_findcoreidx(sih, SYSMEM_CORE_ID, 0) != BADIDX) {
		return TRUE;
	}
	return FALSE;
}

bool
BCMPOSTTRAPFN(si_saqm_present)(const si_t *sih)
{
	if (si_findcoreidx(sih, D11_SAQM_CORE_ID, 0) != BADIDX) {
		return TRUE;
	}
	return FALSE;
}

/*
 * si_get_smb_info:
 * Returns TRUE if SMB core is present, and also fills the base_addr and size.
 * Returns FALSE if SMB core is not present , sets base_addr and size to 0.
 */
bool
BCMPOSTTRAPFN(si_get_smb_info)(const si_t *sih, uint32 *base_addr, uint32 *size)
{
	const si_info_t *sii = SI_INFO(sih);
	if (sii->smb.present) {
		*base_addr = sii->smb.base_addr;
		*size = sii->smb.size;
		return TRUE;
	}

	*base_addr = 0;
	*size = 0;
	return FALSE;
}

bool
BCMPOSTTRAPFN(si_saqm_power_domain_present)(const si_t *sih)
{
	/*
	 * TBD: Need to look into generic method (e.g: query HW) to avoid chipid checks.
	 *
	 * 4384 chip is AI first chip with SAQM. It doesn't have SAQM power domain
	 * 43109/43110 - SAQM doesn't have separate powerdomain, it is part of WL pwrdomain(1)
	 */
	if (CHIPID((sih)->chip) == BCM4384_CHIP_GRPID ||
		BCM43109_CHIP((sih)->chip) ||
		BCM43110_CHIP((sih)->chip)) {
		return FALSE;
	}
	return TRUE;
}

/* return the coreid of the core at index */
uint
BCMPOSTTRAPFN(si_findcoreid)(const si_t *sih, uint coreidx)
{
	const si_info_t *sii = SI_INFO(sih);
	const si_cores_info_t *cores_info = sii->cores_info;

	if (coreidx >= sii->numcores) {
		return NODEV_CORE_ID;
	}
	if (CHIPTYPE(sih->socitype) == SOCI_NCI) {
		return nci_coreid(sih, coreidx);
	}
	return cores_info->coreid[coreidx];
}

/** return total coreunit of coreid or zero if not found */
uint
BCMPOSTTRAPFN(si_numcoreunits)(const si_t *sih, uint coreid)
{
	const si_info_t *sii = SI_INFO(sih);
	const si_cores_info_t *cores_info = (si_cores_info_t *)sii->cores_info;
	uint found = 0;
	uint i;

	if (CHIPTYPE(sih->socitype) == SOCI_NCI) {
		return nci_numcoreunits(sih, coreid);
	}
	for (i = 0; i < sii->numcores; i++) {
		if (cores_info->coreid[i] == coreid) {
			found++;
		}
	}

	return found;
}

/** return total D11 coreunits */
uint
BCMPOSTTRAPRAMFN(si_numd11coreunits)(const si_t *sih)
{
	if (CHIPTYPE(sih->socitype) == SOCI_NCI) {
		return nci_numcoreunits(sih, D11_CORE_ID);
	}
	return si_numcoreunits(sih, D11_CORE_ID);
}

/** return list of found cores */
uint
si_corelist(const si_t *sih, uint coreid[])
{
	const si_info_t *sii = SI_INFO(sih);
	const si_cores_info_t *cores_info = (const si_cores_info_t *)sii->cores_info;

	if (CHIPTYPE(sih->socitype) == SOCI_NCI) {
		return nci_corelist(sih, coreid);
	}
	(void)memcpy_s(coreid, (sii->numcores * sizeof(uint)), cores_info->coreid,
		(sii->numcores * sizeof(uint)));
	return sii->numcores;
}

/** return current wrapper mapping */
volatile void *
BCMPOSTTRAPFN(si_wrapperregs)(const si_t *sih)
{
	const si_info_t *sii = SI_INFO(sih);

	ASSERT(GOODREGS(sii->curwrap));

	return sii->curwrap;
}

/** return current register mapping */
volatile void *
BCMPOSTTRAPFN(si_coreregs)(const si_t *sih)
{
	const si_info_t *sii = SI_INFO(sih);

	ASSERT(GOODREGS(sii->curmap));

	return sii->curmap;
}

/**
 * This function changes logical "focus" to the indicated core;
 * must be called with interrupts off.
 * Moreover, callers should keep interrupts off during switching out of and back to d11 core
 */
volatile void *
BCMPOSTTRAPFN(si_setcore)(si_t *sih, uint coreid, uint coreunit)
{
	si_info_t *sii = SI_INFO(sih);
	uint idx;

	idx = si_findcoreidx(sih, coreid, coreunit);
	if (!GOODIDX(idx, sii->numcores)) {
		return NULL;
	}

	if (CHIPTYPE(sih->socitype) == SOCI_AI)
		return ai_setcoreidx(sih, idx);
	else if (CHIPTYPE(sih->socitype) == SOCI_NCI)
		return nci_setcoreidx(sih, idx);
	else {
		ASSERT(0);
		return NULL;
	}
}

volatile void *
BCMPOSTTRAPFN(si_setcoreidx)(si_t *sih, uint coreidx)
{
	if (CHIPTYPE(sih->socitype) == SOCI_AI)
		return ai_setcoreidx(sih, coreidx);
	else if (CHIPTYPE(sih->socitype) == SOCI_NCI)
		return nci_setcoreidx(sih, coreidx);
	else {
		ASSERT(0);
		return NULL;
	}
}

/** Turn off interrupt as required by sb_setcore, before switch core */
volatile void *
BCMPOSTTRAPFN(si_switch_core)(si_t *sih, uint coreid, uint *origidx, bcm_int_bitmask_t *intr_val)
{
	volatile void *cc;
	si_info_t *sii = SI_INFO(sih);

	if (SI_FAST(sii)) {
		/* Overloading the origidx variable to remember the coreid,
		 * this works because the core ids cannot be confused with
		 * core indices.
		 */
		*origidx = coreid;
		if (coreid == CC_CORE_ID)
			return (volatile void *)CCREGS_FAST(sii);
		else if (coreid == BUSCORETYPE(sih->buscoretype))
			return (volatile void *)PCIEREGS(sii);
	}
	INTR_OFF(sii, intr_val);
	*origidx = sii->curidx;
	cc = si_setcore(sih, coreid, 0);
	ASSERT(cc != NULL);

	return cc;
}

/* restore coreidx and restore interrupt */
void
	BCMPOSTTRAPFN(si_restore_core)(si_t *sih, uint coreid, bcm_int_bitmask_t *intr_val)
{
	si_info_t *sii = SI_INFO(sih);

	if (SI_FAST(sii) && ((coreid == CC_CORE_ID) || (coreid == BUSCORETYPE(sih->buscoretype))))
		return;

	si_setcoreidx(sih, coreid);
	INTR_RESTORE(sii, intr_val);
}

/* Switch to particular core and get corerev */
uint
BCMPOSTTRAPFN(si_corerev_ext)(si_t *sih, uint coreid, uint coreunit)
{
	uint coreidx;
	uint corerev;

	coreidx = si_coreidx(sih);
	(void)si_setcore(sih, coreid, coreunit);

	corerev = si_corerev(sih);

	si_setcoreidx(sih, coreidx);
	return corerev;
}

int
BCMATTACHFN(si_numaddrspaces)(const si_t *sih)
{
	if (CHIPTYPE(sih->socitype) == SOCI_AI)
		return ai_numaddrspaces(sih);
	else if (CHIPTYPE(sih->socitype) == SOCI_NCI)
		return nci_numaddrspaces(sih);
	else {
		ASSERT(0);
		return 0;
	}
}

/* Return the address of the nth address space in the current core
 * Arguments:
 * sih : Pointer to struct si_t
 * spidx : slave port index
 * baidx : base address index
 */

uint32
BCMPOSTTRAPFN(si_addrspace)(const si_t *sih, uint spidx, uint baidx)
{
	if (CHIPTYPE(sih->socitype) == SOCI_AI)
		return ai_addrspace(sih, spidx, baidx);
	else if (CHIPTYPE(sih->socitype) == SOCI_NCI)
		return nci_addrspace(sih, spidx, baidx);
	else {
		ASSERT(0);
		return 0;
	}
}

/* Return the size of the nth address space in the current core
 * Arguments:
 * sih : Pointer to struct si_t
 * spidx : slave port index
 * baidx : base address index
 */
uint32
BCMPOSTTRAPFN(si_addrspacesize)(const si_t *sih, uint spidx, uint baidx)
{
	if (CHIPTYPE(sih->socitype) == SOCI_AI)
		return ai_addrspacesize(sih, spidx, baidx);
	else if (CHIPTYPE(sih->socitype) == SOCI_NCI)
		return nci_addrspacesize(sih, spidx, baidx);
	else {
		ASSERT(0);
		return 0;
	}
}

void
si_coreaddrspaceX(const si_t *sih, uint asidx, uint32 *addr, uint32 *size)
{
	/* Only supported for SOCI_AI */
	if (CHIPTYPE(sih->socitype) == SOCI_AI)
		ai_coreaddrspaceX(sih, asidx, addr, size);
	else
		*size = 0;
}

uint32
BCMPOSTTRAPFN(si_core_cflags)(const si_t *sih, uint32 mask, uint32 val)
{
	if (CHIPTYPE(sih->socitype) == SOCI_AI)
		return ai_core_cflags(sih, mask, val);
	else if (CHIPTYPE(sih->socitype) == SOCI_NCI)
		return nci_core_cflags(sih, mask, val);
	else {
		ASSERT(0);
		return 0;
	}
}

void
si_core_cflags_wo(const si_t *sih, uint32 mask, uint32 val)
{
	if (CHIPTYPE(sih->socitype) == SOCI_AI)
		ai_core_cflags_wo(sih, mask, val);
	else if (CHIPTYPE(sih->socitype) == SOCI_NCI)
		nci_core_cflags_wo(sih, mask, val);
	else
		ASSERT(0);
}

uint32
si_core_sflags(const si_t *sih, uint32 mask, uint32 val)
{
	if (CHIPTYPE(sih->socitype) == SOCI_AI)
		return ai_core_sflags(sih, mask, val);
	else if (CHIPTYPE(sih->socitype) == SOCI_NCI)
		return nci_core_sflags(sih, mask, val);
	else {
		ASSERT(0);
		return 0;
	}
}

void
si_commit(si_t *sih)
{
	if (CHIPTYPE(sih->socitype) == SOCI_AI)
		;
	else if (CHIPTYPE(sih->socitype) == SOCI_NCI)
		;
	else {
		ASSERT(0);
	}
}

bool
BCMPOSTTRAPFN(si_iscoreup)(const si_t *sih)
{
	if (CHIPTYPE(sih->socitype) == SOCI_AI)
		return ai_iscoreup(sih);
	else if (CHIPTYPE(sih->socitype) == SOCI_NCI)
		return nci_iscoreup(sih);
	else {
		ASSERT(0);
		return FALSE;
	}
}

/** Caller should make sure it is on the right core, before calling this routine */
uint
BCMPOSTTRAPFN(si_wrapperreg)(const si_t *sih, uint32 offset, uint32 mask, uint32 val)
{
	/* only for AI back plane chips */
	if (CHIPTYPE(sih->socitype) == SOCI_AI)
		return (ai_wrap_reg(sih, offset, mask, val));
	else if	(CHIPTYPE(sih->socitype) == SOCI_NCI)
		return (nci_get_wrap_reg(sih, offset, mask, val));
	return 0;
}

/* si_backplane_access is used to read full backplane address from host for PCIE FD
 * it uses secondary bar-0 window which lies at an offset of 16K from primary bar-0
 * Provides support for read/write of 1/2/4 bytes of backplane address
 * Can be used to read/write
 *	1. core regs
 *	2. Wrapper regs
 *	3. memory
 *	4. BT area
 * For accessing any 32 bit backplane address, [31 : 12] of backplane should be given in "region"
 * [11 : 0] should be the "regoff"
 * for reading  4 bytes from reg 0x200 of d11 core use it like below
 * : si_backplane_access(sih, 0x18001000, 0x200, 4, 0, TRUE)
 */
static int si_backplane_addr_sane(uint addr, uint size)
{
	int bcmerror = BCME_OK;

	/* For 2 byte access, address has to be 2 byte aligned */
	if (size == 2) {
		if (addr & 0x1) {
			bcmerror = BCME_ERROR;
		}
	}
	/* For 4 byte access, address has to be 4 byte aligned */
	if (size == 4) {
		if (addr & 0x3) {
			bcmerror = BCME_ERROR;
		}
	}

	return bcmerror;
}

void
si_invalidate_second_bar0win(si_t *sih)
{
	si_info_t *sii = SI_INFO(sih);
	sii->second_bar0win = ~0x0;
}

int
si_backplane_access(si_t *sih, uint addr, uint size, uint *val, bool read)
{
	volatile uint32 *r = NULL;
	uint32 region = 0;
	si_info_t *sii = SI_INFO(sih);

	/* Valid only for pcie bus */
	if (BUSTYPE(sih->bustype) != PCI_BUS) {
		SI_ERROR(("Valid only for pcie bus \n"));
		return BCME_ERROR;
	}

	/* Split adrr into region and address offset */
	region = (addr & (0xFFFFF << 12));
	addr = addr & 0xFFF;

	/* check for address and size sanity */
	if (si_backplane_addr_sane(addr, size) != BCME_OK)
		return BCME_ERROR;

	/* Update window if required */
	if (sii->second_bar0win != region) {
		OSL_PCI_WRITE_CONFIG(sii->osh, PCIE2_BAR0_CORE2_WIN, 4, region);
		sii->second_bar0win = region;
	}

	/* Estimate effective address
	 * sii->curmap   : bar-0 virtual address
	 * PCI_SECOND_BAR0_OFFSET  : secondar bar-0 offset
	 * regoff : actual reg offset
	 */
	r = (volatile uint32 *)((volatile char *)sii->curmap + PCI_SECOND_BAR0_OFFSET + addr);

	SI_VMSG(("si curmap %p  region %x regaddr %x effective addr %p READ %d\n",
		(volatile char *)sii->curmap, region, addr, r, read));

	switch (size) {
	case sizeof(uint8):
		if (read)
			*val = R_REG(sii->osh, (volatile uint8 *)r);
		else
			W_REG(sii->osh, (volatile uint8 *)r, *val);
		break;
	case sizeof(uint16):
		if (read)
			*val = R_REG(sii->osh, (volatile uint16 *)r);
		else
			W_REG(sii->osh, (volatile uint16 *)r, *val);
		break;
	case sizeof(uint32):
		if (read)
			*val = R_REG(sii->osh, (volatile uint32 *)r);
		else
			W_REG(sii->osh, (volatile uint32 *)r, *val);
		break;
	default:
		SI_ERROR(("Invalid  size %d \n", size));
		return BCME_ERROR;
		break;
	}

	return BCME_OK;
}

uint
BCMPOSTTRAPFN(si_corereg)(si_t *sih, uint coreidx, uint regoff, uint mask, uint val)
{
	if (CHIPTYPE(sih->socitype) == SOCI_AI)
		return ai_corereg(sih, coreidx, regoff, mask, val);
	else if (CHIPTYPE(sih->socitype) == SOCI_NCI)
		return nci_corereg(sih, coreidx, regoff, mask, val);
	else {
		ASSERT(0);
		return 0;
	}
}

uint
BCMPOSTTRAPFN(si_corereg_writeonly)(si_t *sih, uint coreidx, uint regoff, uint mask, uint val)
{
	if (CHIPTYPE(sih->socitype) == SOCI_NCI) {
		return nci_corereg_writeonly(sih, coreidx, regoff, mask, val);
	} else {
		return ai_corereg_writeonly(sih, coreidx, regoff, mask, val);
	}
}

uint
BCMPOSTTRAPFN(si_corereg_writearr)(si_t *sih, uint coreidx, uint regoff, uint *mask, uint *val,
		uint num_vals)
{
	if (CHIPTYPE(sih->socitype) == SOCI_AI) {
		return ai_corereg_writearr(sih, coreidx, regoff, mask, val, num_vals);
	} else {
		ASSERT(0);
		return 0;
	}
}

/** 'idx' should refer either to the chipcommon core or the PMU core */
uint
BCMPOSTTRAPFN(si_pmu_corereg)(si_t *sih, uint32 idx, uint regoff, uint mask, uint val)
{
	int pmustatus_offset;

	/* prevent backplane stall on double write to 'ILP domain' registers in the PMU */
	if (mask != 0 && PMUREGS_ILP_SENSITIVE(regoff)) {
		pmustatus_offset = PMU_REG_OFF(PMUStatus);

		while (si_corereg(sih, idx, pmustatus_offset, 0, 0) & PST_SLOW_WR_PENDING) {
			/* empty */
		};
	}

	return si_corereg(sih, idx, regoff, mask, val);
}

#ifdef BCM_MMU_DEVMEM_PROTECT
void
BCMPOSTTRAPFN(si_core_devmem_protect)(const si_t *sih, bool set)
{
	const si_info_t *sii = SI_INFO(sih);
	uint32 axi_addr = si_addrspace(sih, CORE_SLAVE_PORT_1, CORE_BASE_ADDR_0);
	uint32 axi_size = si_addrspacesize(sih, CORE_SLAVE_PORT_1, CORE_BASE_ADDR_0);
	uint32 mb;
	uint32 access, l1_access;

	if (set) {
		access = L2S_PAGE_ATTR_NO_ACC;
		l1_access = L1S_PAGE_ATTR_NO_ACC;
	} else {
		access = L2S_PAGE_ATTR_DEVICE_MEM;
		l1_access = L1S_PAGE_ATTR_DEVICE_MEM;
	}

	HND_MMU_SET_L2_PAGE_ATTRIBUTES(sii->curmap, sii->curmap, SI_CORE_SIZE,
		access, FALSE);
	/* Protection is in terms of MB. In case the range cross MB, strip it */
	axi_addr = ROUNDUP(axi_addr, MB);
	axi_size = ROUNDDN(axi_size, MB);

	SI_MSG(("si_core_devmem_protect curidx %d coreid 0x%x axi_addr 0x%x size(MB) %d\n",
		sii->curidx, si_coreid(sih), axi_addr, axi_size/MB));

	if (axi_addr && axi_size > MB) {
		for (mb = axi_addr; mb < (axi_addr + axi_size); mb = mb + MB) {
			HND_MMU_SET_L1_PAGE_ATTRIBUTES(mb, mb, MB, l1_access, FALSE);
		}
	}

	HND_MMU_FLUSH_TLB();
}
#endif /* BCM_MMU_DEVMEM_PROTECT */

/*
 * If there is no need for fiddling with interrupts or core switches (typically silicon
 * back plane registers, pci registers and chipcommon registers), this function
 * returns the register offset on this core to a mapped address. This address can
 * be used for W_REG/R_REG directly.
 *
 * For accessing registers that would need a core switch, this function will return
 * NULL.
 */
volatile uint32 *
BCMPOSTTRAPFN(si_corereg_addr)(si_t *sih, uint coreidx, uint regoff)
{
	if (CHIPTYPE(sih->socitype) == SOCI_AI)
		return ai_corereg_addr(sih, coreidx, regoff);
	else if (CHIPTYPE(sih->socitype) == SOCI_NCI)
		return nci_corereg_addr(sih, coreidx, regoff);
	else {
		return 0;
	}
}

void
si_core_disable(const si_t *sih, uint32 bits)
{
	if (CHIPTYPE(sih->socitype) == SOCI_AI) {
		ai_core_disable(sih, bits);
	} else if (CHIPTYPE(sih->socitype) == SOCI_NCI) {
		nci_core_disable(sih, bits);
	}

#ifdef BCM_MMU_DEVMEM_PROTECT
	si_core_devmem_protect(sih, TRUE);
#endif /* BCM_MMU_DEVMEM_PROTECT */
}

bool
BCMPOSTTRAPFN(si_core_reset)(si_t *sih, uint32 bits, uint32 resetbits)
{
	bool ret = TRUE;
	if (CHIPTYPE(sih->socitype) == SOCI_AI) {
		ret = ai_core_reset(sih, bits, resetbits);
	} else if (CHIPTYPE(sih->socitype) == SOCI_NCI) {
		nci_core_reset(sih, bits, resetbits);
	}

#ifdef BCM_MMU_DEVMEM_PROTECT
	si_core_devmem_protect(sih, FALSE);
#endif /* BCM_MMU_DEVMEM_PROTECT */
	return ret;
}

/** Run bist on current core. Caller needs to take care of core-specific bist hazards */
int
si_corebist(const si_t *sih)
{
	uint32 cflags;
	int result = 0;

	/* Read core control flags */
	cflags = si_core_cflags(sih, 0, 0);

	/* Set bist & fgc */
	si_core_cflags(sih, ~0, (SICF_BIST_EN | SICF_FGC));

	/* Wait for bist done */
	SPINWAIT(((si_core_sflags(sih, 0, 0) & SISF_BIST_DONE) == 0), 100000);

	if (si_core_sflags(sih, 0, 0) & SISF_BIST_ERROR)
		result = BCME_ERROR;

	/* Reset core control flags */
	si_core_cflags(sih, 0xffff, cflags);

	return result;
}

uint
si_num_slaveports(const si_t *sih, uint coreid)
{
	uint idx = si_findcoreidx(sih, coreid, 0);
	uint num = 0;

	if (idx != BADIDX) {
		if (CHIPTYPE(sih->socitype) == SOCI_AI) {
			num = ai_num_slaveports(sih, idx);
		} else if (CHIPTYPE(sih->socitype) == SOCI_NCI) {
			num = nci_num_slaveports(sih, idx);
		}
	}
	return num;
}

/* TODO: Check if NCI has a slave port address */
uint32
si_get_slaveport_addr(si_t *sih, uint spidx, uint baidx, uint core_id, uint coreunit)
{
	const si_info_t *sii = SI_INFO(sih);
	uint origidx = sii->curidx;
	uint32 addr = 0x0;

	if (!((CHIPTYPE(sih->socitype) == SOCI_AI) ||
		(CHIPTYPE(sih->socitype) == SOCI_NCI)))
		goto done;

	si_setcore(sih, core_id, coreunit);

	addr = si_addrspace(sih, spidx, baidx);

	si_setcoreidx(sih, origidx);

done:
	return addr;
}

/* TODO: Check if NCI has a d11 slave port address */
uint32
si_get_d11_slaveport_addr(si_t *sih, uint spidx, uint baidx, uint coreunit)
{
	const si_info_t *sii = SI_INFO(sih);
	uint origidx = sii->curidx;
	uint32 addr = 0x0;

	if (!((CHIPTYPE(sih->socitype) == SOCI_AI) ||
		(CHIPTYPE(sih->socitype) == SOCI_NCI)))
		goto done;

	si_setcore(sih, D11_CORE_ID, coreunit);

	addr = si_addrspace(sih, spidx, baidx);

	si_setcoreidx(sih, origidx);

done:
	return addr;
}

static uint32
BCMINITFN(factor6)(uint32 x)
{
	switch (x) {
	case CC_F6_2:	return 2;
	case CC_F6_3:	return 3;
	case CC_F6_4:	return 4;
	case CC_F6_5:	return 5;
	case CC_F6_6:	return 6;
	case CC_F6_7:	return 7;
	default:	return 0;
	}
}

/*
 * Divide the clock by the divisor with protection for
 * a zero divisor.
 */
static uint32
divide_clock(uint32 clock, uint32 div)
{
	return div ? clock / div : 0;
}

/** calculate the speed the SI would run at given a set of clockcontrol values */
uint32
BCMINITFN(si_clock_rate)(uint32 pll_type, uint32 n, uint32 m)
{
	uint32 n1, n2, clock, m1, m2, m3, mc;

	n1 = n & CN_N1_MASK;
	n2 = (n & CN_N2_MASK) >> CN_N2_SHIFT;

	if (pll_type == PLL_TYPE6) {
		if (m & CC_T6_MMASK)
			return CC_T6_M1;
		else
			return CC_T6_M0;
	} else if ((pll_type == PLL_TYPE1) ||
			(pll_type == PLL_TYPE3) ||
			(pll_type == PLL_TYPE4) ||
			(pll_type == PLL_TYPE7)) {
		n1 = factor6(n1);
		n2 += CC_F5_BIAS;
	} else if (pll_type == PLL_TYPE2) {
		n1 += CC_T2_BIAS;
		n2 += CC_T2_BIAS;
		ASSERT((n1 >= 2) && (n1 <= 7));
		ASSERT((n2 >= 5) && (n2 <= 23));
	} else if (pll_type == PLL_TYPE5) {
		/* 5365 */
		return 100000000;
	} else
		ASSERT(0);
	/* PLL types 3 and 7 use BASE2 (25Mhz) */
	if ((pll_type == PLL_TYPE3) ||
	    (pll_type == PLL_TYPE7)) {
		clock = CC_CLOCK_BASE2 * n1 * n2;
	} else
		clock = CC_CLOCK_BASE1 * n1 * n2;

	if (clock == 0)
		return 0;

	m1 = m & CC_M1_MASK;
	m2 = (m & CC_M2_MASK) >> CC_M2_SHIFT;
	m3 = (m & CC_M3_MASK) >> CC_M3_SHIFT;
	mc = (m & CC_MC_MASK) >> CC_MC_SHIFT;

	if ((pll_type == PLL_TYPE1) ||
	    (pll_type == PLL_TYPE3) ||
	    (pll_type == PLL_TYPE4) ||
	    (pll_type == PLL_TYPE7)) {
		m1 = factor6(m1);
		if ((pll_type == PLL_TYPE1) || (pll_type == PLL_TYPE3))
			m2 += CC_F5_BIAS;
		else
			m2 = factor6(m2);
		m3 = factor6(m3);

		switch (mc) {
		case CC_MC_BYPASS:	return (clock);
		case CC_MC_M1:		return divide_clock(clock, m1);
		case CC_MC_M1M2:	return divide_clock(clock, m1 * m2);
		case CC_MC_M1M2M3:	return divide_clock(clock, m1 * m2 * m3);
		case CC_MC_M1M3:	return divide_clock(clock, m1 * m3);
		default:		return (0);
		}
	} else {
		ASSERT(pll_type == PLL_TYPE2);

		m1 += CC_T2_BIAS;
		m2 += CC_T2M2_BIAS;
		m3 += CC_T2_BIAS;
		ASSERT((m1 >= 2) && (m1 <= 7));
		ASSERT((m2 >= 3) && (m2 <= 10));
		ASSERT((m3 >= 2) && (m3 <= 7));

		if ((mc & CC_T2MC_M1BYP) == 0)
			clock /= m1;
		if ((mc & CC_T2MC_M2BYP) == 0)
			clock /= m2;
		if ((mc & CC_T2MC_M3BYP) == 0)
			clock /= m3;

		return clock;
	}
}

/**
 * Some chips could have multiple host interfaces, however only one will be active.
 * For a given chip. Depending pkgopt and cc_chipst return the active host interface.
 */
uint
si_chip_hostif(const si_t *sih)
{
	uint hosti = CHIP_HOSTIF_PCIEMODE;

	return hosti;
}

/** set chip watchdog reset timer to fire in 'ticks' */
void
si_watchdog(si_t *sih, uint ticks)
{
	uint maxt;
	uint pmu_wdt = 1;

	if (PMUCTL_ENAB(sih) && pmu_wdt) {
		/* The mips compiler uses the sllv instruction,
		 * so we specially handle the 32-bit case.
		 */
		maxt = 0xffffffff;

		/* PR43821: PMU watchdog timer needs min. of 2 ticks */
		if (ticks == 1) {
			ticks = 2;
		} else if (ticks > maxt) {
			ticks = maxt;
		}
		pmu_corereg(sih, SI_CC_IDX, PmuWatchdogCounter, ~0, ticks);
	} else {
#if !defined(BCMDONGLEHOST)
		/* make sure we come up in fast clock mode; or if clearing, clear clock */
		si_clkctl_cc(sih, ticks ? CLK_FAST : CLK_DYNAMIC);
#endif /* !defined(BCMDONGLEHOST) */
		maxt = (1 << 28) - 1;
		if (ticks > maxt)
			ticks = maxt;

		if (CCREV(sih->ccrev) >= 65) {
			si_corereg(sih, SI_CC_IDX, CC_REG_OFF(WatchdogCounter), ~0,
				(ticks & WD_COUNTER_MASK) | WD_SSRESET_PCIE_F0_EN |
					WD_SSRESET_PCIE_ALL_FN_EN);
		} else {
			si_corereg(sih, SI_CC_IDX, CC_REG_OFF(WatchdogCounter), ~0, ticks);
		}
	}
}

/** trigger watchdog reset after ms milliseconds */
void
si_watchdog_ms(si_t *sih, uint32 ms)
{
	si_watchdog(sih, wd_msticks * ms);
}

uint32 si_watchdog_msticks(void)
{
	return wd_msticks;
}

bool
si_taclear(si_t *sih, bool details)
{
#if defined(BCMDBG_ERR) || defined(BCMASSERT_SUPPORT) || defined(BCMDBG_DUMP)
	if ((CHIPTYPE(sih->socitype) == SOCI_AI) ||
		(CHIPTYPE(sih->socitype) == SOCI_NCI))
		return FALSE;
	else {
		ASSERT(0);
		return FALSE;
	}
#else
	return FALSE;
#endif /* BCMDBG_ERR || BCMASSERT_SUPPORT || BCMDBG_DUMP */
}

#ifdef BCMDBG
void
si_view(si_t *sih, bool verbose)
{
	if (CHIPTYPE(sih->socitype) == SOCI_AI)
		ai_view(sih, verbose);
	else if (CHIPTYPE(sih->socitype) == SOCI_NCI)
		nci_view(sih, verbose);
	else
		ASSERT(0);
}

void
si_viewall(si_t *sih, bool verbose)
{
	si_info_t *sii = SI_INFO(sih);
	uint curidx, i;
	bcm_int_bitmask_t intr_val;

	curidx = sii->curidx;

	INTR_OFF(sii, &intr_val);
	if (CHIPTYPE(sih->socitype) == SOCI_AI)
		ai_viewall(sih, verbose);
	else if (CHIPTYPE(sih->socitype) == SOCI_NCI)
		nci_viewall(sih, verbose);
	else {
		SI_ERROR(("si_viewall: num_cores %d\n", sii->numcores));
		for (i = 0; i < sii->numcores; i++) {
			si_setcoreidx(sih, i);
			si_view(sih, verbose);
		}
	}
	si_setcoreidx(sih, curidx);
	INTR_RESTORE(sii, &intr_val);
}
#endif	/* BCMDBG */

/** return the slow clock source - LPO, XTAL, or PCI */
static uint
si_slowclk_src(si_info_t *sii)
{
	ASSERT(SI_FAST(sii) || si_coreid(&sii->pub) == CC_CORE_ID);

	return SCC_SS_XTAL;
}

/** return the ILP (slowclock) min or max frequency */
uint
si_slowclk_freq(si_info_t *sii, chipcregs_t *cc)
{
	uint div;

	ASSERT(SI_FAST(sii) || si_coreid(&sii->pub) == CC_CORE_ID);

	/* shouldn't be here unless we've established the chip has dynamic clk control */
	ASSERT(R_REG(sii->osh, CC_REG_ADDR(cc, CoreCapabilities)) & CC_CAP_PWR_CTL);

	div = R_REG(sii->osh, CC_REG_ADDR(cc, system_clk_ctl)) >> SYCC_CD_SHIFT;
	div = 4 * (div + 1);

	return (XTALMINFREQ / div);
}

static void
BCMINITFN(si_clkctl_setdelay)(si_info_t *sii, void *chipcregs)
{
	chipcregs_t *cc = (chipcregs_t *)chipcregs;
	uint slowmaxfreq, pll_delay, slowclk;
	uint pll_on_delay, fref_sel_delay;

	pll_delay = PLL_DELAY;

	/* If the slow clock is not sourced by the xtal then add the xtal_on_delay
	 * since the xtal will also be powered down by dynamic clk control logic.
	 */

	slowclk = si_slowclk_src(sii);
	if (slowclk != SCC_SS_XTAL)
		pll_delay += XTAL_ON_DELAY;

	/* Starting with 4318 it is ILP that is used for the delays */
	slowmaxfreq = si_slowclk_freq(sii, cc);

	pll_on_delay = ((slowmaxfreq * pll_delay) + 999999) / 1000000;
	fref_sel_delay = ((slowmaxfreq * FREF_DELAY) + 999999) / 1000000;

	W_REG(sii->osh, CC_REG_ADDR(cc, pll_on_delay), pll_on_delay);
	W_REG(sii->osh, CC_REG_ADDR(cc, fref_sel_delay), fref_sel_delay);
}

/** initialize power control delay registers */
void
BCMINITFN(si_clkctl_init)(si_t *sih)
{
	si_info_t *sii;
	uint origidx = 0;
	chipcregs_t *cc;
	bool fast;

	if (!CCCTL_ENAB(sih))
		return;

	sii = SI_INFO(sih);
	fast = SI_FAST(sii);
	if (!fast) {
		origidx = sii->curidx;
		cc = (chipcregs_t *)si_setcore(sih, CC_CORE_ID, 0);
		if (cc == NULL)
			return;
	} else {
		cc = (chipcregs_t *)CCREGS_FAST(sii);
		if (cc == NULL)
			return;
	}
	ASSERT(cc != NULL);

	/* set all Instaclk chip ILP to 1 MHz */
	SET_REG(sii->osh, CC_REG_ADDR(cc, system_clk_ctl), SYCC_CD_MASK,
		(ILP_DIV_1MHZ << SYCC_CD_SHIFT));

	si_clkctl_setdelay(sii, (void *)(uintptr)cc);

	/* PR 110294 */
	OSL_DELAY(20000);

	if (!fast)
		si_setcoreidx(sih, origidx);
}

/** change logical "focus" to the gpio core for optimized access */
volatile void *
si_gpiosetcore(si_t *sih)
{
	return si_setcoreidx(sih, SI_CC_IDX);
}

/**
 * mask & set gpiocontrol bits.
 * If a gpiocontrol bit is set to 0, chipcommon controls the corresponding GPIO pin.
 * If a gpiocontrol bit is set to 1, the GPIO pin is no longer a GPIO and becomes dedicated
 *   to some chip-specific purpose.
 */
uint32
BCMPOSTTRAPFN(si_gpiocontrol)(si_t *sih, uint32 mask, uint32 val, uint8 priority)
{
	uint regoff;

	regoff = 0;

	/* gpios could be shared on router platforms
	 * ignore reservation if it's high priority (e.g., test apps)
	 */
	if ((priority != GPIO_HI_PRIORITY) &&
	    (BUSTYPE(sih->bustype) == SI_BUS) && (val || mask)) {
		mask = priority ? (si_gpioreservation & mask) :
			((si_gpioreservation | mask) & ~(si_gpioreservation));
		val &= mask;
	}

	regoff = CC_REG_OFF(GPIOCtrl);
	return si_corereg(sih, SI_CC_IDX, regoff, mask, val);
}

/** mask&set gpio output enable bits */
uint32
BCMPOSTTRAPFN(si_gpioouten)(si_t *sih, uint32 mask, uint32 val, uint8 priority)
{
	uint regoff;

	regoff = 0;

	/* gpios could be shared on router platforms
	 * ignore reservation if it's high priority (e.g., test apps)
	 */
	if ((priority != GPIO_HI_PRIORITY) &&
	    (BUSTYPE(sih->bustype) == SI_BUS) && (val || mask)) {
		mask = priority ? (si_gpioreservation & mask) :
			((si_gpioreservation | mask) & ~(si_gpioreservation));
		val &= mask;
	}

	regoff = CC_REG_OFF(GPIOOutputEn);
	return si_corereg(sih, SI_CC_IDX, regoff, mask, val);
}

/** mask&set gpio output bits */
uint32
BCMPOSTTRAPFN(si_gpioout)(si_t *sih, uint32 mask, uint32 val, uint8 priority)
{
	uint regoff;

	regoff = 0;

	/* gpios could be shared on router platforms
	 * ignore reservation if it's high priority (e.g., test apps)
	 */
	if ((priority != GPIO_HI_PRIORITY) &&
	    (BUSTYPE(sih->bustype) == SI_BUS) && (val || mask)) {
		mask = priority ? (si_gpioreservation & mask) :
			((si_gpioreservation | mask) & ~(si_gpioreservation));
		val &= mask;
	}

	regoff = CC_REG_OFF(GPIOOutput);
	return si_corereg(sih, SI_CC_IDX, regoff, mask, val);
}

/** reserve one gpio */
uint32
si_gpioreserve(const si_t *sih, uint32 gpio_bitmask, uint8 priority)
{
	/* only cores on SI_BUS share GPIO's and only applcation users need to
	 * reserve/release GPIO
	 */
	if ((BUSTYPE(sih->bustype) != SI_BUS) || (!priority)) {
		ASSERT((BUSTYPE(sih->bustype) == SI_BUS) && (priority));
		return 0xffffffff;
	}
	/* make sure only one bit is set */
	if ((!gpio_bitmask) || ((gpio_bitmask) & (gpio_bitmask - 1))) {
		ASSERT((gpio_bitmask) && !((gpio_bitmask) & (gpio_bitmask - 1)));
		return 0xffffffff;
	}

	/* already reserved */
	if (si_gpioreservation & gpio_bitmask)
		return 0xffffffff;
	/* set reservation */
	si_gpioreservation |= gpio_bitmask;

	return si_gpioreservation;
}

/**
 * release one gpio.
 *
 * releasing the gpio doesn't change the current value on the GPIO last write value
 * persists till someone overwrites it.
 */
uint32
si_gpiorelease(const si_t *sih, uint32 gpio_bitmask, uint8 priority)
{
	/* only cores on SI_BUS share GPIO's and only applcation users need to
	 * reserve/release GPIO
	 */
	if ((BUSTYPE(sih->bustype) != SI_BUS) || (!priority)) {
		ASSERT((BUSTYPE(sih->bustype) == SI_BUS) && (priority));
		return 0xffffffff;
	}
	/* make sure only one bit is set */
	if ((!gpio_bitmask) || ((gpio_bitmask) & (gpio_bitmask - 1))) {
		ASSERT((gpio_bitmask) && !((gpio_bitmask) & (gpio_bitmask - 1)));
		return 0xffffffff;
	}

	/* already released */
	if (!(si_gpioreservation & gpio_bitmask))
		return 0xffffffff;

	/* clear reservation */
	si_gpioreservation &= ~gpio_bitmask;

	return si_gpioreservation;
}

/* return the current gpioin register value */
uint32
si_gpioin(si_t *sih)
{
	uint regoff;

	regoff = CC_REG_OFF(GPIOInput);
	return si_corereg(sih, SI_CC_IDX, regoff, 0, 0);
}

/* mask&set gpio interrupt polarity bits */
uint32
si_gpiointpolarity(si_t *sih, uint32 mask, uint32 val, uint8 priority)
{
	uint regoff;

	/* gpios could be shared on router platforms */
	if ((BUSTYPE(sih->bustype) == SI_BUS) && (val || mask)) {
		mask = priority ? (si_gpioreservation & mask) :
			((si_gpioreservation | mask) & ~(si_gpioreservation));
		val &= mask;
	}

	regoff = CC_REG_OFF(GPIOIntPolarity);
	return si_corereg(sih, SI_CC_IDX, regoff, mask, val);
}

/* mask&set gpio interrupt mask bits */
uint32
si_gpiointmask(si_t *sih, uint32 mask, uint32 val, uint8 priority)
{
	uint regoff;

	/* gpios could be shared on router platforms */
	if ((BUSTYPE(sih->bustype) == SI_BUS) && (val || mask)) {
		mask = priority ? (si_gpioreservation & mask) :
			((si_gpioreservation | mask) & ~(si_gpioreservation));
		val &= mask;
	}

	regoff = CC_REG_OFF(GPIOIntMask);
	return si_corereg(sih, SI_CC_IDX, regoff, mask, val);
}

uint32
si_gpioeventintmask(si_t *sih, uint32 mask, uint32 val, uint8 priority)
{
	uint regoff;
	/* gpios could be shared on router platforms */
	if ((BUSTYPE(sih->bustype) == SI_BUS) && (val || mask)) {
		mask = priority ? (si_gpioreservation & mask) :
			((si_gpioreservation | mask) & ~(si_gpioreservation));
		val &= mask;
	}
	regoff = CC_REG_OFF(GPIOEventIntMask);
	return si_corereg(sih, SI_CC_IDX, regoff, mask, val);
}

uint32
si_gpiopull(si_t *sih, bool updown, uint32 mask, uint32 val)
{
	uint offs;

	offs = (updown ? CC_REG_OFF(GPIOPulldown) : CC_REG_OFF(GPIOPullup));
	return si_corereg(sih, SI_CC_IDX, offs, mask, val);
}

uint32
si_gpioevent(si_t *sih, uint regtype, uint32 mask, uint32 val)
{
	uint offs;

	if (regtype == GPIO_REGEVT)
		offs = CC_REG_OFF(GPIOEvent);
	else if (regtype == GPIO_REGEVT_INTMSK)
		offs = CC_REG_OFF(GPIOEventIntMask);
	else if (regtype == GPIO_REGEVT_INTPOL)
		offs = CC_REG_OFF(GPIOEventIntPolarity);
	else
		return 0xffffffff;

	return si_corereg(sih, SI_CC_IDX, offs, mask, val);
}

uint32
BCMATTACHFN(si_gpio_int_enable)(si_t *sih, bool enable)
{
	uint offs;

	offs = CC_REG_OFF(IntMask);
	return si_corereg(sih, SI_CC_IDX, offs, CI_GPIO, (enable ? CI_GPIO : 0));
}

/** Return the size of the specified SYSMEM bank */
static uint
sysmem_banksize(const si_info_t *sii, sysmemregs_t *regs, uint8 idx)
{
	uint banksize, bankinfo;
	uint bankidx = idx;

	W_REG(sii->osh, SYSMEM_REG_ADDR(regs, BankIndex), bankidx);
	bankinfo = R_REG(sii->osh, SYSMEM_REG_ADDR(regs, BankInfo));
	banksize = SYSMEM_BANKINFO_SZBASE * ((bankinfo & SYSMEM_BANKINFO_SZMASK) + 1);
	return banksize;
}

/** Return the RAM size of the SYSMEM core */
uint32
si_sysmem_size(si_t *sih)
{
	const si_info_t *sii = SI_INFO(sih);
	uint origidx;
	bcm_int_bitmask_t intr_val;

	sysmemregs_t *regs;
	bool wasup;
	uint32 coreinfo;
	uint memsize = 0;
	uint8 i;
	uint nb, nrb;

	SI_MSG_DBG_REG(("%s: Enter\n", __FUNCTION__));
	/* Block ints and save current core */
	INTR_OFF(sii, &intr_val);
	origidx = si_coreidx(sih);

	/* Switch to SYSMEM core */
	regs = si_setcore(sih, SYSMEM_CORE_ID, 0);
	if (!regs)
		goto done;

	/* Get info for determining size */
	wasup = si_iscoreup(sih);
	if (!wasup) {
		si_core_reset(sih, 0, 0);
	}
	coreinfo = R_REG(sii->osh, SYSMEM_REG_ADDR(regs, CoreInfo));

	/* Number of ROM banks, SW need to skip the ROM banks. */
	nrb = SYSMEM_ROMNB(coreinfo, si_corerev(sih));
	nb = SYSMEM_SRNB(coreinfo, si_corerev(sih));
	for (i = 0; i < nb; i++)
		memsize += sysmem_banksize(sii, regs, i + nrb);

	si_setcoreidx(sih, origidx);

done:
	INTR_RESTORE(sii, &intr_val);

	SI_MSG_DBG_REG(("%s: Exit memsize=%d\n", __FUNCTION__, memsize));
	return memsize;
}

/** Return the size of the specified SOCRAM bank */
static uint
socram_banksize(const si_info_t *sii, sbsocramregs_t *regs, uint8 idx, uint8 mem_type)
{
	uint banksize, bankinfo;
	uint bankidx = idx | (mem_type << SOCRAM_BANKIDX_MEMTYPE_SHIFT);

	ASSERT(mem_type <= SOCRAM_MEMTYPE_DEVRAM);

	W_REG(sii->osh, &regs->bankidx, bankidx);
	bankinfo = R_REG(sii->osh, &regs->bankinfo);
	banksize = SOCRAM_BANKINFO_SZBASE * ((bankinfo & SOCRAM_BANKINFO_SZMASK) + 1);
	return banksize;
}

void si_socram_set_bankpda(si_t *sih, uint32 bankidx, uint32 bankpda)
{
	const si_info_t *sii = SI_INFO(sih);
	uint origidx;
	bcm_int_bitmask_t intr_val;
	sbsocramregs_t *regs;
	bool wasup;
	uint corerev;

	/* Block ints and save current core */
	INTR_OFF(sii, &intr_val);
	origidx = si_coreidx(sih);

	/* Switch to SOCRAM core */
	regs = si_setcore(sih, SOCRAM_CORE_ID, 0);
	if (!regs)
		goto done;

	wasup = si_iscoreup(sih);
	if (!wasup)
		si_core_reset(sih, 0, 0);

	corerev = si_corerev(sih);
	if (corerev >= 16) {
		W_REG(sii->osh, &regs->bankidx, bankidx);
		W_REG(sii->osh, &regs->bankpda, bankpda);
	}

	/* Return to previous state and core */
	if (!wasup)
		si_core_disable(sih, 0);
	si_setcoreidx(sih, origidx);

done:
	INTR_RESTORE(sii, &intr_val);
}

/** Return the RAM size of the SOCRAM core */
uint32
si_socram_size(si_t *sih)
{
	const si_info_t *sii = SI_INFO(sih);
	uint origidx;
	bcm_int_bitmask_t intr_val;

	sbsocramregs_t *regs;
	bool wasup;
	uint corerev;
	uint32 coreinfo;
	uint memsize = 0;

	/* Block ints and save current core */
	INTR_OFF(sii, &intr_val);
	origidx = si_coreidx(sih);

	/* Switch to SOCRAM core */
	regs = si_setcore(sih, SOCRAM_CORE_ID, 0);
	if (!regs)
		goto done;

	/* Get info for determining size */
	wasup = si_iscoreup(sih);
	if (!wasup)
		si_core_reset(sih, 0, 0);
	corerev = si_corerev(sih);
	coreinfo = R_REG(sii->osh, &regs->coreinfo);

	/* Calculate size from coreinfo based on rev */
	if (corerev == 0)
		memsize = 1 << (16 + (coreinfo & SRCI_MS0_MASK));
	else if (corerev < 3) {
		memsize = 1 << (SR_BSZ_BASE + (coreinfo & SRCI_SRBSZ_MASK));
		memsize *= (coreinfo & SRCI_SRNB_MASK) >> SRCI_SRNB_SHIFT;
	} else if ((corerev <= 7) || (corerev == 12)) {
		uint nb = (coreinfo & SRCI_SRNB_MASK) >> SRCI_SRNB_SHIFT;
		uint bsz = (coreinfo & SRCI_SRBSZ_MASK);
		uint lss = (coreinfo & SRCI_LSS_MASK) >> SRCI_LSS_SHIFT;
		if (lss != 0)
			nb--;
		memsize = nb * (1 << (bsz + SR_BSZ_BASE));
		if (lss != 0)
			memsize += (1 << ((lss - 1) + SR_BSZ_BASE));
	} else {
		uint8 i;
		uint nb;
		/* length of SRAM Banks increased for corerev greater than 23 */
		if (corerev >= 23) {
			nb = (coreinfo & (SRCI_SRNB_MASK | SRCI_SRNB_MASK_EXT)) >> SRCI_SRNB_SHIFT;
		} else {
			nb = (coreinfo & SRCI_SRNB_MASK) >> SRCI_SRNB_SHIFT;
		}
		for (i = 0; i < nb; i++)
			memsize += socram_banksize(sii, regs, i, SOCRAM_MEMTYPE_RAM);
	}

	/* Return to previous state and core */
	if (!wasup)
		si_core_disable(sih, 0);
	si_setcoreidx(sih, origidx);

done:
	INTR_RESTORE(sii, &intr_val);

	return memsize;
}

/* Return true if bus MPU is present */
bool
si_is_bus_mpu_present(si_t *sih)
{
	uint origidx, newidx = NODEV_CORE_ID;
	sysmemregs_t *sysmemregs = NULL;
	cr4regs_t *cr4regs;
	const si_info_t *sii = SI_INFO(sih);
	uint ret = 0;
	bool wasup;

	origidx = si_coreidx(sih);

	cr4regs = si_setcore(sih, ARMCR4_CORE_ID, 0);
	if (cr4regs) {
		/* ARMCR4 */
		newidx = ARMCR4_CORE_ID;
	} else {
		sysmemregs = si_setcore(sih, SYSMEM_CORE_ID, 0);
		if (sysmemregs) {
			/* ARMCA7 */
			newidx = SYSMEM_CORE_ID;
		}
	}

	if (newidx != NODEV_CORE_ID) {
		wasup = si_iscoreup(sih);
		if (!wasup) {
			si_core_reset(sih, 0, 0);
		}
		if (newidx == ARMCR4_CORE_ID) {
			/* ARMCR4 */
			ret = R_REG(sii->osh, &cr4regs->corecapabilities) & CAP_MPU_MASK;
		} else {
			/* ARMCA7 */
			ret = R_REG(sii->osh, SYSMEM_REG_ADDR(sysmemregs, MpuCap)) &
				ACC_MPU_REGION_CNT_MASK;
		}
		if (!wasup) {
			si_core_disable(sih, 0);
		}
	}

	si_setcoreidx(sih, origidx);

	return ret ? TRUE : FALSE;
}

uint32
si_socram_srmem_size(si_t *sih)
{
	const si_info_t *sii = SI_INFO(sih);
	uint origidx;
	bcm_int_bitmask_t intr_val;

	sbsocramregs_t *regs;
	bool wasup;
	uint corerev;
	uint32 coreinfo;
	uint memsize = 0;

	/* Block ints and save current core */
	INTR_OFF(sii, &intr_val);
	origidx = si_coreidx(sih);

	/* Switch to SOCRAM core */
	regs = si_setcore(sih, SOCRAM_CORE_ID, 0);
	if (!regs)
		goto done;

	/* Get info for determining size */
	wasup = si_iscoreup(sih);
	if (!wasup)
		si_core_reset(sih, 0, 0);
	corerev = si_corerev(sih);
	coreinfo = R_REG(sii->osh, &regs->coreinfo);

	/* Calculate size from coreinfo based on rev */
	if (corerev >= 16) {
		uint8 i;
		uint nb = (coreinfo & SRCI_SRNB_MASK) >> SRCI_SRNB_SHIFT;
		for (i = 0; i < nb; i++) {
			W_REG(sii->osh, &regs->bankidx, i);
			if (R_REG(sii->osh, &regs->bankinfo) & SOCRAM_BANKINFO_RETNTRAM_MASK)
				memsize += socram_banksize(sii, regs, i, SOCRAM_MEMTYPE_RAM);
		}
	}

	/* Return to previous state and core */
	if (!wasup)
		si_core_disable(sih, 0);
	si_setcoreidx(sih, origidx);

done:
	INTR_RESTORE(sii, &intr_val);

	return memsize;
}

/**
 * For boards that use GPIO(8) is used for Bluetooth Coex TX_WLAN pin,
 * when GPIOControl for Pin 8 is with ChipCommon core,
 * if UART_TX_1 (bit 5: Chipc capabilities) strapping option is set, then
 * GPIO pin 8 is driven by Uart0MCR:2 rather than GPIOOut:8. To drive this pin
 * low, one has to set Uart0MCR:2 to 1. This is required when the BTC is disabled,
 * or the driver goes down. Refer to PR35488.
 */
void
si_btcgpiowar(si_t *sih)
{
	const si_info_t *sii = SI_INFO(sih);
	uint origidx;
	bcm_int_bitmask_t intr_val;
	chipcregs_t *cc;

	/* Make sure that there is ChipCommon core present &&
	 * UART_TX is strapped to 1
	 */
	if (!(sih->cccaps & CC_CAP_UARTGPIO))
		return;

	/* si_corereg cannot be used as we have to guarantee 8-bit read/writes */
	INTR_OFF(sii, &intr_val);

	origidx = si_coreidx(sih);

	cc = (chipcregs_t *)si_setcore(sih, CC_CORE_ID, 0);
	ASSERT(cc != NULL);

	W_REG(sii->osh, CC_REG_ADDR(cc, uart0mcr),
	      R_REG(sii->osh, CC_REG_ADDR(cc, uart0mcr)) | 0x04);

	/* restore the original index */
	si_setcoreidx(sih, origidx);

	INTR_RESTORE(sii, &intr_val);
}

void
si_chipcontrl_restore(si_t *sih, uint32 val)
{
	const si_info_t *sii = SI_INFO(sih);
	chipcregs_t *cc;
	uint origidx = si_coreidx(sih);

	cc = (chipcregs_t *)si_setcore(sih, CC_CORE_ID, 0);
	if (cc == NULL) {
		SI_ERROR(("si_chipcontrl_restore: Failed to find CORE ID!\n"));
		return;
	}
	W_REG(sii->osh, CC_REG_ADDR(cc, ChipControl), val);
	si_setcoreidx(sih, origidx);
}

uint32
si_chipcontrl_read(si_t *sih)
{
	const si_info_t *sii = SI_INFO(sih);
	chipcregs_t *cc;
	uint origidx = si_coreidx(sih);
	uint32 val;

	cc = (chipcregs_t *)si_setcore(sih, CC_CORE_ID, 0);
	if (cc == NULL) {
		SI_ERROR(("si_chipcontrl_read: Failed to find CORE ID!\n"));
		return -1;
	}
	val = R_REG(sii->osh, CC_REG_ADDR(cc, ChipControl));
	si_setcoreidx(sih, origidx);
	return val;
}

/**
 * The SROM clock is derived from the backplane clock. For chips having a fast
 * backplane clock that requires a higher-than-POR-default clock divisor ratio for the SROM clock.
 */
void
si_srom_clk_set(si_t *sih)
{
	const si_info_t *sii = SI_INFO(sih);
	chipcregs_t *cc;
	uint origidx = si_coreidx(sih);
	uint32 val;
	uint32 divisor = 1;

	cc = (chipcregs_t *)si_setcore(sih, CC_CORE_ID, 0);
	if (cc == NULL) {
		SI_ERROR(("si_srom_clk_set: Failed to find CORE ID!\n"));
		return;
	}

	val = R_REG(sii->osh, CC_REG_ADDR(cc, ClkDiv2));
	ASSERT(0);

	W_REG(sii->osh, CC_REG_ADDR(cc, ClkDiv2), ((val & ~CLKD2_SROM) | divisor));
	si_setcoreidx(sih, origidx);
}

void
si_btc_enable_chipcontrol(si_t *sih)
{
	const si_info_t *sii = SI_INFO(sih);
	chipcregs_t *cc;
	uint origidx = si_coreidx(sih);

	cc = (chipcregs_t *)si_setcore(sih, CC_CORE_ID, 0);
	if (cc == NULL) {
		SI_ERROR(("si_btc_enable_chipcontrol: Failed to find CORE ID!\n"));
		return;
	}

	/* BT fix */
	W_REG(sii->osh, CC_REG_ADDR(cc, ChipControl),
		R_REG(sii->osh, CC_REG_ADDR(cc, ChipControl)) | CC_BTCOEX_EN_MASK);

	si_setcoreidx(sih, origidx);
}

/** cache device removed state */
void si_set_device_removed(si_t *sih, bool status)
{
	si_info_t *sii = SI_INFO(sih);

	sii->device_removed = status;
}

/** check if the device is removed */
bool
si_deviceremoved(const si_t *sih)
{
	uint32 w;
	const si_info_t *sii = SI_INFO(sih);

	if (sii->device_removed) {
		return TRUE;
	}

	switch (BUSTYPE(sih->bustype)) {
	case PCI_BUS:
		ASSERT(SI_INFO(sih)->osh != NULL);
		w = OSL_PCI_READ_CONFIG(SI_INFO(sih)->osh, PCI_CFG_VID, sizeof(uint32));
		if ((w & 0xFFFF) != VENDOR_BROADCOM)
			return TRUE;
		break;
	default:
		break;
	}
	return FALSE;
}

bool
si_is_warmboot(void)
{

	return FALSE;
}

bool
si_is_sprom_available(si_t *sih)
{
	const si_info_t *sii;
	uint origidx;
	chipcregs_t *cc;
	uint32 sromctrl;

	if ((sih->cccaps & CC_CAP_SROM) == 0)
		return FALSE;

	sii = SI_INFO(sih);
	origidx = sii->curidx;
	cc = si_setcoreidx(sih, SI_CC_IDX);
	ASSERT(cc);
	sromctrl = R_REG(sii->osh, CC_REG_ADDR(cc, SpromCtrl));
	si_setcoreidx(sih, origidx);

	return (sromctrl & SRC_PRESENT);
}

bool
si_is_sflash_available(const si_t *sih)
{
	return (sih->chipst & CST_SFLASH_PRESENT) != 0;
}

uint32 BCMATTACHFN(si_get_sromctl)(si_t *sih)
{
	chipcregs_t *cc;
	uint origidx = si_coreidx(sih);
	uint32 sromctl;
	osl_t *osh = si_osh(sih);

	cc = si_setcoreidx(sih, SI_CC_IDX);
	ASSERT((uintptr)cc);

	sromctl = R_REG(osh, CC_REG_ADDR(cc, SpromCtrl));

	/* return to the original core */
	si_setcoreidx(sih, origidx);
	return sromctl;
}

int BCMATTACHFN(si_set_sromctl)(si_t *sih, uint32 value)
{
	chipcregs_t *cc;
	uint origidx = si_coreidx(sih);
	osl_t *osh = si_osh(sih);
	int ret = BCME_OK;

	cc = si_setcoreidx(sih, SI_CC_IDX);
	ASSERT((uintptr)cc);

	/* get chipcommon rev */
	if (si_corerev(sih) >= 32) {
		/* SpromCtrl is only accessible if CoreCapabilities.SpromSupported and
		 * SpromPresent is 1.
		 */
		if ((R_REG(osh, CC_REG_ADDR(cc, CoreCapabilities)) & CC_CAP_SROM) != 0 &&
		     (R_REG(osh, CC_REG_ADDR(cc, SpromCtrl)) & SRC_PRESENT)) {
			W_REG(osh, CC_REG_ADDR(cc, SpromCtrl), value);
		} else {
			ret = BCME_NODEVICE;
		}
	} else {
		ret = BCME_UNSUPPORTED;
	}

	/* return to the original core */
	si_setcoreidx(sih, origidx);

	return ret;
}

uint
BCMPOSTTRAPFN(si_core_wrapperreg)(si_t *sih, uint32 coreidx, uint32 offset, uint32 mask, uint32 val)
{
	uint origidx;
	bcm_int_bitmask_t intr_val;
	uint ret_val;
	const si_info_t *sii = SI_INFO(sih);

	origidx = si_coreidx(sih);

	INTR_OFF(sii, &intr_val);
	/* Validate the core idx */
	si_setcoreidx(sih, coreidx);

	ret_val = si_wrapperreg(sih, offset, mask, val);

	/* return to the original core */
	si_setcoreidx(sih, origidx);
	INTR_RESTORE(sii, &intr_val);
	return ret_val;
}

/* cleanup the timer from the host when ARM is been halted
 * without a chance for ARM cleanup its resources
 * If left not cleanup, Intr from a software timer can still
 * request HT clk when ARM is halted.
 */
uint32
si_pmu_res_req_timer_clr(si_t *sih)
{
	uint32 mask;

	mask = PRRT_REQ_ACTIVE | PRRT_INTEN | PRRT_HT_REQ;
	mask <<= 14;
	/* clear mask bits */
	pmu_corereg(sih, SI_CC_IDX, ResourceReqTimer0, mask, 0);
	/* readback to ensure write completes */
	return pmu_corereg(sih, SI_CC_IDX, ResourceReqTimer0, 0, 0);
}

/* Caller of this function should make sure is on PCIE core
 * Used in pciedev.c.
 */
void
si_pcie_disable_oobselltr(const si_t *sih)
{
	ASSERT(si_coreid(sih) == PCIE2_CORE_ID);
	si_wrapperreg(sih, AI_OOBSELIND74, ~0, 0);
}

uint32
BCMPOSTTRAPFN(si_clear_backplane_to_per_core)(si_t *sih, uint coreid, uint coreunit, void *wrap)
{
	uint32 ret = AXI_WRAP_STS_NONE;

#ifdef AXI_TIMEOUTS
	if (CHIPTYPE(sih->socitype) == SOCI_AI) {
		ret = ai_clear_backplane_to_per_core(sih, coreid, coreunit, wrap);
	} else
#endif /* AXI_TIMEOUTS */
	if (CHIPTYPE(sih->socitype) == SOCI_NCI) {
		ret = nci_clear_backplane_to_per_core(sih, coreid, coreunit);
	}

	return ret;
}

#if !defined(FIQ_ON_AXI_ERR) && !(defined(SOCI_NCI_BUS) && defined(DONGLEBUILD))
/* This API will be called to check backplane errors periodically. Not relevent when
 * FIQ on AXI error is enabled.
 */
uint32
BCMPOSTTRAPFN(si_clear_backplane_to)(si_t *sih)
{
	uint32 ret = 0;
	if (CHIPTYPE(sih->socitype) == SOCI_AI) {
		ret = ai_clear_backplane_to(sih);
	}
	return ret;
}
#endif /* !defined(FIQ_ON_AXI_ERR) && !(defined(SOCI_NCI_BUS) && defined(DONGLEBUILD)) */

uint32
BCMPOSTTRAPFN(si_clear_backplane_to_fiq)(si_t *sih)
{
	uint32 ret = 0;

	if (CHIPTYPE(sih->socitype) == SOCI_AI) {
		ret = ai_clear_backplane_to(sih);
	} else if (CHIPTYPE(sih->socitype) == SOCI_NCI) {
#ifdef AXI_TIMEOUTS
		ret = nci_clear_backplane_to(sih);
#endif /* AXI_TIMEOUTS */
	}

	return ret;
}

void
BCMPOSTTRAPFN(si_update_backplane_timeouts)(const si_t *sih, bool enable, uint32 timeout_exp,
	uint32 cid)
{
#if defined(AXI_TIMEOUTS)
	if (CHIPTYPE(sih->socitype) == SOCI_AI) {
		ai_update_backplane_timeouts(sih, enable, timeout_exp, cid);
	} else if (CHIPTYPE(sih->socitype) == SOCI_NCI) {
		nci_update_backplane_timeouts(sih, enable, timeout_exp, cid);
	}
#endif /* AXI_TIMEOUTS */
	return;
}

/*
 * This routine adds the AXI timeouts for
 * chipcommon, pcie and ARM slave wrappers
 */
void
si_slave_wrapper_add(si_t *sih)
{
#if defined(AXI_TIMEOUTS)
	uint32 axi_to = 0;

	axi_to = AXI_TO_VAL;

	if (CHIPTYPE(sih->socitype) == SOCI_NCI) {
		nci_update_backplane_timeouts(sih, TRUE, axi_to, 0);
	} else {
		/* All required slave wrappers are added in ai_scan */
		ai_update_backplane_timeouts(sih, TRUE, axi_to, 0);

#ifdef DISABLE_PCIE2_AXI_TIMEOUT
		ai_update_backplane_timeouts(sih, FALSE, 0, PCIE_CORE_ID);
		ai_update_backplane_timeouts(sih, FALSE, 0, PCIE2_CORE_ID);
#endif
	}
#endif /* AXI_TIMEOUTS */
}

void
si_pll_sr_reinit(si_t *sih)
{
}

uint32
BCMATTACHFN(si_wrapper_dump_buf_size)(const si_t *sih)
{
#ifdef DONGLEBUILD
	if (CHIPTYPE(sih->socitype) == SOCI_AI) {
		return ai_wrapper_dump_buf_size(sih);
	} else
#endif /* DONGLEBUILD */
	if (CHIPTYPE(sih->socitype) == SOCI_NCI) {
		return nci_wrapper_dump_buf_size(sih);
	}
	return 0;
}

uint32
BCMPOSTTRAPFN(si_wrapper_dump_binary)(const si_t *sih, uchar *p)
{
#ifdef DONGLEBUILD
	if (CHIPTYPE(sih->socitype) == SOCI_AI) {
		return ai_wrapper_dump_binary(sih, p);
	} else
#endif /* DONGLEBUILD */
	if (CHIPTYPE(sih->socitype) == SOCI_NCI) {
		return nci_wrapper_dump_binary(sih, p);
	}
	return 0;
}

void
BCMPOSTTRAPFN(si_wrapper_get_last_error)(const si_t *sih, uint32 *error_status, uint32 *core,
	uint32 *lo, uint32 *hi, uint32 *id)
{
#if defined(AXI_TIMEOUTS)
	if (CHIPTYPE(sih->socitype) == SOCI_AI) {
		ai_wrapper_get_last_error(sih, error_status, core, lo, hi, id);
	} else if (CHIPTYPE(sih->socitype) == SOCI_NCI) {
		nci_wrapper_get_last_error(sih, error_status, core, lo, hi, id);
	}
#endif /* AXI_TIMEOUTS */
	return;
}

uint32
BCMPOSTTRAPFN(si_get_axi_timeout_reg)(const si_t *sih)
{
#if defined(AXI_TIMEOUTS)
	if (CHIPTYPE(sih->socitype) == SOCI_AI) {
		return ai_get_axi_timeout_reg();
	} else if (CHIPTYPE(sih->socitype) == SOCI_NCI) {
		return nci_get_axi_timeout_reg();
	}
#endif /* AXI_TIMEOUTS */
	return 0;
}

#if defined(BCMSRPWR) && !defined(BCMSRPWR_DISABLED)
bool _bcmsrpwr = TRUE;
#else
bool _bcmsrpwr = FALSE;
#endif

#define PWRREQ_OFFSET(sih)	CC_REG_OFF(PowerControl)

static void
BCMPOSTTRAPFN(si_corereg_pciefast_write)(const si_t *sih, uint regoff, uint val)
{
	volatile uint32 *r = NULL;
	const si_info_t *sii = SI_INFO(sih);

	ASSERT((BUSTYPE(sih->bustype) == PCI_BUS));

	r = (volatile uint32 *)((volatile char *)sii->curmap +
		PCI_16KB0_PCIREGS_OFFSET + regoff);

	W_REG(sii->osh, r, val);
}

static uint
BCMPOSTTRAPFN(si_corereg_pciefast_read)(const si_t *sih, uint regoff)
{
	volatile uint32 *r = NULL;
	const si_info_t *sii = SI_INFO(sih);

	ASSERT((BUSTYPE(sih->bustype) == PCI_BUS));

	r = (volatile uint32 *)((volatile char *)sii->curmap +
		PCI_16KB0_PCIREGS_OFFSET + regoff);

	return R_REG(sii->osh, r);
}

uint32
BCMPOSTTRAPFN(si_srpwr_request)(const si_t *sih, uint32 mask, uint32 val)
{
	const si_info_t *sii = SI_INFO(sih);
	uint32 r, offset = (BUSTYPE(sih->bustype) == SI_BUS) ?
		CC_REG_OFF(PowerControl) : PWRREQ_OFFSET(sih);
	uint32 mask2 = mask;
	uint32 val2 = val;
	volatile uint32 *fast_srpwr_addr = (volatile uint32 *)((uintptr)SI_ENUM_BASE(sih)
					 + (uintptr)offset);

	if (mask || val) {
		mask <<= SRPWR_REQON_SHIFT;
		val  <<= SRPWR_REQON_SHIFT;

		/* Return if requested power request is already set */
		if ((BUSTYPE(sih->bustype) == SI_BUS) || (BUSTYPE(sih->bustype) == SDIO_BUS)) {
			r = R_REG(sii->osh, fast_srpwr_addr);
		} else {
			r = si_corereg_pciefast_read(sih, offset);
		}

		if ((r & mask) == val) {
			return r;
		}

		r = (r & ~mask) | val;

		if ((BUSTYPE(sih->bustype) == SI_BUS) || (BUSTYPE(sih->bustype) == SDIO_BUS)) {
			W_REG(sii->osh, fast_srpwr_addr, r);
			r = R_REG(sii->osh, fast_srpwr_addr);
		} else {
			si_corereg_pciefast_write(sih, offset, r);
			r = si_corereg_pciefast_read(sih, offset);
		}

		if (val2) {
			if ((r & (mask2 << SRPWR_STATUS_SHIFT)) ==
			(val2 << SRPWR_STATUS_SHIFT)) {
				return r;
			}
			si_srpwr_stat_spinwait(sih, mask2, val2);
		}
	} else {
		if ((BUSTYPE(sih->bustype) == SI_BUS) || (BUSTYPE(sih->bustype) == SDIO_BUS)) {
			r = R_REG(sii->osh, fast_srpwr_addr);
		} else {
			r = si_corereg_pciefast_read(sih, offset);
		}
	}

	return r;
}

uint32
BCMPOSTTRAPFN(si_srpwr_stat_spinwait)(const si_t *sih, uint32 mask, uint32 val)
{
	const si_info_t *sii = SI_INFO(sih);
	uint32 r, offset = (BUSTYPE(sih->bustype) == SI_BUS) ?
		CC_REG_OFF(PowerControl) : PWRREQ_OFFSET(sih);
	volatile uint32 *fast_srpwr_addr = (volatile uint32 *)((uintptr)SI_ENUM_BASE(sih)
					 + (uintptr)offset);

	/* spinwait on pwrstatus */
	mask <<= SRPWR_STATUS_SHIFT;
	val <<= SRPWR_STATUS_SHIFT;

	if ((BUSTYPE(sih->bustype) == SI_BUS) || (BUSTYPE(sih->bustype) == SDIO_BUS)) {
		SPINWAIT(((R_REG(sii->osh, fast_srpwr_addr) & mask) != val),
			PMU_MAX_TRANSITION_DLY);
		r = R_REG(sii->osh, fast_srpwr_addr) & mask;
		ROMMABLE_ASSERT(r == val);
	} else {
		SPINWAIT(((si_corereg_pciefast_read(sih, offset) & mask) != val),
			PMU_MAX_TRANSITION_DLY);
		r = si_corereg_pciefast_read(sih, offset) & mask;
		ROMMABLE_ASSERT(r == val);
	}

	r = (r >> SRPWR_STATUS_SHIFT) & SRPWR_DMN_ALL_MASK(sih);

	return r;
}

uint32
si_srpwr_stat(si_t *sih)
{
	uint32 r, offset = (BUSTYPE(sih->bustype) == SI_BUS) ?
		CC_REG_OFF(PowerControl) : PWRREQ_OFFSET(sih);
	uint cidx = (BUSTYPE(sih->bustype) == SI_BUS) ? SI_CC_IDX : sih->buscoreidx;

	if (BUSTYPE(sih->bustype) == SI_BUS) {
		r = si_corereg(sih, cidx, offset, 0, 0);
	} else {
		r = si_corereg_pciefast_read(sih, offset);
	}

	r = (r >> SRPWR_STATUS_SHIFT) & SRPWR_DMN_ALL_MASK(sih);

	return r;
}

uint32
si_srpwr_domain(si_t *sih)
{
	uint32 r, offset = (BUSTYPE(sih->bustype) == SI_BUS) ?
		CC_REG_OFF(PowerControl) : PWRREQ_OFFSET(sih);
	uint cidx = (BUSTYPE(sih->bustype) == SI_BUS) ? SI_CC_IDX : sih->buscoreidx;

	if (BUSTYPE(sih->bustype) == SI_BUS) {
		r = si_corereg(sih, cidx, offset, 0, 0);
	} else {
		r = si_corereg_pciefast_read(sih, offset);
	}

	r = (r >> SRPWR_DMN_ID_SHIFT) & SRPWR_DMN_ID_MASK;

	return r;
}

uint8
si_srpwr_domain_wl(si_t *sih)
{
	return SRPWR_DMN1_ARMBPSD;
}

bool
si_srpwr_cap(si_t *sih)
{
	/* If domain ID is non-zero, chip supports power domain control */
	return si_srpwr_domain(sih) != 0 ? TRUE : FALSE;
}

uint32
BCMPOSTTRAPFN(si_srpwr_domain_all_mask)(const si_t *sih)
{
	uint32 mask = SRPWR_DMN0_PCIE_MASK |
		SRPWR_DMN1_ARMBPSD_MASK |
		SRPWR_DMN2_MACAUX_MASK |
		SRPWR_DMN3_MACMAIN_MASK;

	if (si_scan_core_present(sih)) {
		mask |= SRPWR_DMN4_MACSCAN_MASK;
	}

	if (si_saqm_present(sih) && si_saqm_power_domain_present(sih)) {
		mask |= SRPWR_DMN6_SAQM_MASK;
	}

	return mask;
}

uint32
si_srpwr_bt_status(si_t *sih)
{
	uint32 r;
	uint32 offset = (BUSTYPE(sih->bustype) == SI_BUS) ?
		CC_REG_OFF(PowerControl) : PWRREQ_OFFSET(sih);
	uint32 cidx = (BUSTYPE(sih->bustype) == SI_BUS) ? SI_CC_IDX : sih->buscoreidx;

	if (BUSTYPE(sih->bustype) == SI_BUS) {
		r = si_corereg(sih, cidx, offset, 0, 0);
	} else {
		r = si_corereg_pciefast_read(sih, offset);
	}

	r = (r >> SRPWR_BT_STATUS_SHIFT) & SRPWR_BT_STATUS_MASK;

	return r;
}

/* Utility API to read/write the raw registers with absolute address.
 * This function can be invoked from either FW or host driver.
 */
uint32
si_raw_reg(const si_t *sih, uint32 reg, uint32 val, uint32 wrire_req)
{
	const si_info_t *sii = SI_INFO(sih);
	uint32 address_space = reg & ~0xFFF;
	volatile uint32 *addr = (void *)(uintptr)(reg);
	uint32 prev_value = 0;
	uint32 cfg_reg = 0;

	if (sii == NULL) {
		return 0;
	}

	/* No need to translate the absolute address on SI bus */
	if (BUSTYPE(sih->bustype) == SI_BUS) {
		goto skip_cfg;
	}

	/* This API supports only the PCI host interface */
	if (BUSTYPE(sih->bustype) != PCI_BUS) {
		return ID32_INVALID;
	}

	if (PCIE_GEN2(sii)) {
		/* Use BAR0 Secondary window is PCIe Gen2.
		 * Set the secondary BAR0 Window to current register of interest
		 */
		addr = (volatile uint32 *)(((volatile uint8 *)sii->curmap) +
			PCI_SEC_BAR0_WIN_OFFSET + (reg & 0xfff));
		cfg_reg = PCIE2_BAR0_CORE2_WIN;

	} else {
		/* PCIe Gen1 do not have secondary BAR0 window.
		 * reuse the BAR0 WIN2
		 */
		addr = (volatile uint32 *)(((volatile uint8 *)sii->curmap) +
			PCI_BAR0_WIN2_OFFSET + (reg & 0xfff));
		cfg_reg = PCI_BAR0_WIN2;
	}

	prev_value = OSL_PCI_READ_CONFIG(sii->osh, cfg_reg, 4);

	if (prev_value != address_space) {
		OSL_PCI_WRITE_CONFIG(sii->osh, cfg_reg,
			sizeof(uint32), address_space);
	} else {
		prev_value = 0;
	}

skip_cfg:
	if (wrire_req) {
		W_REG(sii->osh, addr, val);
	} else {
		val = R_REG(sii->osh, addr);
	}

	if (prev_value) {
		/* Restore BAR0 WIN2 for PCIE GEN1 devices */
		OSL_PCI_WRITE_CONFIG(sii->osh,
			cfg_reg, sizeof(uint32), prev_value);
	}

	return val;
}

uint8
si_lhl_ps_mode(const si_t *sih)
{
	const si_info_t *sii = SI_INFO(sih);
	return sii->lhl_ps_mode;
}

uint8
BCMPOSTTRAPFN(si_hib_ext_wakeup_isenab)(const si_t *sih)
{
	const si_info_t *sii = SI_INFO(sih);
	return sii->hib_ext_wakeup_enab;
}

#if defined(BCMSDIODEV_ENABLED) && defined(ATE_BUILD)
bool
si_chipcap_sdio_ate_only(const si_t *sih)
{
	bool ate_build;
	ate_build = TRUE;
	return ate_build;
}
#endif /* BCMSDIODEV_ENABLED && ATE_BUILD */

#ifdef UART_TRAP_DBG
void
si_dump_APB_Bridge_registers(const si_t *sih)
{
	if (CHIPTYPE(sih->socitype) == SOCI_AI) {
		ai_dump_APB_Bridge_registers(sih);
	}
}
#endif /* UART_TRAP_DBG */

void
si_force_clocks(const si_t *sih, uint clock_state)
{
	if (CHIPTYPE(sih->socitype) == SOCI_AI) {
		ai_force_clocks(sih, clock_state);
	}
}

/* Indicates to the siutils how the PICe BAR0 is mappend,
 * used for siutils to arrange BAR0 window management,
 * for PCI NIC driver.
 *
 * Here is the current scheme, which are all using BAR0:
 *
 * id     enum       wrapper
 * ====   =========  =========
 *    0   0000-0FFF  1000-1FFF
 *    1   4000-4FFF  5000-5FFF
 *    2   9000-9FFF  A000-AFFF
 * >= 3   not supported
 */
void
si_set_slice_id(si_t *sih, uint8 slice)
{
	si_info_t *sii = SI_INFO(sih);

	sii->slice = slice;
}

uint8
si_get_slice_id(const si_t *sih)
{
	const si_info_t *sii = SI_INFO(sih);

	return sii->slice;
}

bool
BCMPOSTTRAPRAMFN(si_scan_core_present)(const si_t *sih)
{
	return (si_numcoreunits(sih, D11_CORE_ID) > 2u);
}

bool
BCMPOSTTRAPRAMFN(si_aux_core_present)(const si_t *sih)
{
	return (si_numcoreunits(sih, D11_CORE_ID) > 1u);
}

#if defined(USING_PMU_TIMER)
#ifdef USE_LHL_TIMER
/* Get current HIB time API */
uint32
BCMPOSTTRAPFN(si_cur_hib_time)(si_t *sih)
{
	uint32 hib_time;

	hib_time = LHL_REG(sih, lhl_hibtim_adr, 0, 0);

	/* there is no HW sync on the read path for LPO regs,
	 * so SW should read twice and if values are same,
	 * then use the value, else read again and use the
	 * latest value
	 */
	if (hib_time != LHL_REG(sih, lhl_hibtim_adr, 0, 0)) {
		hib_time = LHL_REG(sih, lhl_hibtim_adr, 0, 0);
	}

	return (hib_time);
}
#endif /* USE_LHL_TIMER */
#endif /* USING_PMU_TIMER */
