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
#include <hndsoc.h>
#include <sbchipc.h>
#include <pcicfg.h>
#include <pcie_core.h>

#include "siutils_priv.h"

#if !defined(BCMDONGLEHOST) || defined(AXI_TIMEOUTS)
#include "aiutils_priv.h"
#endif
#if defined(ETD)
#include <etd.h>
#endif

#if !defined(BCMDONGLEHOST)
#define PMU_DMP(sii) ((sii)->cores_info->coreid[(sii)->curidx] == PMU_CORE_ID)
#define GCI_DMP(sii) ((sii)->cores_info->coreid[(sii)->curidx] == GCI_CORE_ID)
#else
#define PMU_DMP(sii) (0)
#define GCI_DMP(sii) (0)
#endif /* !defined(BCMDONGLEHOST) */

/* EROM parsing */

static uint32
get_erom_ent(const si_t *sih, uint32 **eromptr, uint32 mask, uint32 match)
{
	uint32 ent;
	uint inv = 0, nom = 0;
	uint32 size = 0;

	while (TRUE) {
		ent = R_REG(SI_INFO(sih)->osh, *eromptr);
		(*eromptr)++;

		if (mask == 0)
			break;

		if ((ent & ER_VALID) == 0) {
			inv++;
			continue;
		}

		if (ent == (ER_END | ER_VALID))
			break;

		if ((ent & mask) == match)
			break;

		/* escape condition related EROM size if it has invalid values */
		size += sizeof(*eromptr);
		if (size >= ER_SZ_MAX) {
			SI_ERROR(("Failed to find end of EROM marker\n"));
			break;
		}

		nom++;
	}

	SI_VMSG(("get_erom_ent: Returning ent 0x%08x\n", ent));
	if (inv + nom) {
		SI_VMSG(("  after %d invalid and %d non-matching entries\n", inv, nom));
	}
	return ent;
}

static uint32
get_asd(const si_t *sih, uint32 **eromptr, uint sp, uint ad, uint st, uint32 *addrl, uint32 *addrh,
	uint32 *sizel, uint32 *sizeh)
{
	uint32 asd, sz, szd;

	BCM_REFERENCE(ad);

	asd = get_erom_ent(sih, eromptr, ER_VALID, ER_VALID);
	if (((asd & ER_TAG1) != ER_ADD) ||
		(((asd & AD_SP_MASK) >> AD_SP_SHIFT) != sp) ||
		((asd & AD_ST_MASK) != st)) {
		/* This is not what we want, "push" it back */
		(*eromptr)--;
		return 0;
	}
	*addrl = asd & AD_ADDR_MASK;
	if (asd & AD_AG32)
		*addrh = get_erom_ent(sih, eromptr, 0, 0);
	else
		*addrh = 0;
	*sizeh = 0;
	sz = asd & AD_SZ_MASK;
	if (sz == AD_SZ_SZD) {
		szd = get_erom_ent(sih, eromptr, 0, 0);
		*sizel = szd & SD_SZ_MASK;
		if (szd & SD_SG32)
			*sizeh = get_erom_ent(sih, eromptr, 0, 0);
	} else
		*sizel = AD_SZ_BASE << (sz >> AD_SZ_SHIFT);

	SI_VMSG(("  SP %d, ad %d: st = %d, 0x%08x_0x%08x @ 0x%08x_0x%08x\n",
		sp, ad, st, *sizeh, *sizel, *addrh, *addrl));

	return asd;
}

/* If OOBR_TYPE is NIC-400 (bits [2:0] value 0x1) in EromPtrOffset,
 * it means that Erom is in OOBR
 */
bool
BCMATTACHFN(ai_erom_in_oobr)(si_info_t *sii, void *regs)
{
	chipcregs_t *cc = (chipcregs_t *)regs;
	uint32 erombase = R_REG(sii->osh, CC_REG_ADDR(cc, EromPtrOffset));

	return (erombase & ID_NODETYPE_MASK) ? TRUE : FALSE;
}

/* Parse the enumeration rom to identify all cores */
void
BCMATTACHFN(ai_scan)(si_t *sih, void *regs, uint devid)
{
	si_info_t *sii = SI_INFO(sih);
	si_cores_info_t *cores_info = (si_cores_info_t *)sii->cores_info;
	chipcregs_t *cc = (chipcregs_t *)regs;
	uint32 erombase, *eromptr, *eromlim;
	axi_wrapper_t *axi_wrapper = sii->axi_wrapper;

	SI_MSG_DBG_REG(("%s: Enter\n", __FUNCTION__));
	BCM_REFERENCE(devid);

	erombase = R_REG(sii->osh, CC_REG_ADDR(cc, EromPtrOffset));

	switch (BUSTYPE(sih->bustype)) {
	case SI_BUS:
		eromptr = (uint32 *)REG_MAP(erombase, SI_CORE_SIZE);
		break;

	case PCI_BUS:
		/* Set wrappers address */
		sii->curwrap = (void *)((uintptr)regs + SI_CORE_SIZE);

		/* Now point the window at the erom */
		OSL_PCI_WRITE_CONFIG(sii->osh, PCI_BAR0_WIN, 4, erombase);
		eromptr = regs;
		break;

#ifdef BCMSDIO
	case SPI_BUS:
	case SDIO_BUS:
		eromptr = (uint32 *)(uintptr)erombase;
		break;
#endif	/* BCMSDIO */

	default:
		SI_ERROR(("Don't know how to do AXI enumeration on bus %d\n", sih->bustype));
		ASSERT(0);
		return;
	}
	eromlim = eromptr + (ER_REMAPCONTROL / sizeof(uint32));
	sii->axi_num_wrappers = 0;

	SI_VMSG(("ai_scan: regs = 0x%p, erombase = 0x%08x, eromptr = 0x%p, eromlim = 0x%p\n",
		OSL_OBFUSCATE_BUF(regs), erombase,
		OSL_OBFUSCATE_BUF(eromptr), OSL_OBFUSCATE_BUF(eromlim)));
	while (eromptr < eromlim) {
		uint32 cia, cib, cid, mfg, crev, nmw, nsw, nmp, nsp;
		uint32 mpd, asd, addrl, addrh, sizel, sizeh;
		uint i, j, idx;
		bool br;

		br = FALSE;

		/* Grok a component */
		cia = get_erom_ent(sih, &eromptr, ER_TAG, ER_CI);
		if (cia == (ER_END | ER_VALID)) {
			SI_VMSG(("Found END of erom after %d cores\n", sii->numcores));
			SI_MSG_DBG_REG(("%s: Exit\n", __FUNCTION__));
			return;
		}

		cib = get_erom_ent(sih, &eromptr, 0, 0);

		if ((cib & ER_TAG) != ER_CI) {
			SI_ERROR(("CIA not followed by CIB\n"));
			goto error;
		}

		cid = (cia & CIA_CID_MASK) >> CIA_CID_SHIFT;
		mfg = (cia & CIA_MFG_MASK) >> CIA_MFG_SHIFT;
		crev = (cib & CIB_REV_MASK) >> CIB_REV_SHIFT;
		nmw = (cib & CIB_NMW_MASK) >> CIB_NMW_SHIFT;
		nsw = (cib & CIB_NSW_MASK) >> CIB_NSW_SHIFT;
		nmp = (cib & CIB_NMP_MASK) >> CIB_NMP_SHIFT;
		nsp = (cib & CIB_NSP_MASK) >> CIB_NSP_SHIFT;

#ifdef BCMDBG_SI
		SI_VMSG(("Found component 0x%04x/0x%04x rev %d at erom addr 0x%p, with nmw = %d, "
			"nsw = %d, nmp = %d & nsp = %d\n",
			mfg, cid, crev, OSL_OBFUSCATE_BUF(eromptr - 1), nmw, nsw, nmp, nsp));
#else
		BCM_REFERENCE(crev);
#endif

		/* Include Default slave wrapper for timeout monitoring */
		if ((nsp == 0 && nsw == 0) ||
			((mfg == MFGID_ARM) && (cid == DEF_AI_COMP))) {
			continue;
		}

		if ((nmw + nsw == 0)) {
			/* A component which is not a core */
			/* Should record some info */
			if (cid == OOB_ROUTER_CORE_ID) {
				asd = get_asd(sih, &eromptr, 0, 0, AD_ST_SLAVE,
					&addrl, &addrh, &sizel, &sizeh);
				if (asd != 0) {
					if ((sii->oob_router != 0) && (sii->oob_router != addrl)) {
						sii->oob_router1 = addrl;
					} else {
						sii->oob_router = addrl;
					}
				}
			}
			if ((cid != NS_CCB_CORE_ID) && (cid != PMU_CORE_ID) &&
				(cid != GCI_CORE_ID) && (cid != SR_CORE_ID) &&
				(cid != HUB_CORE_ID) && (cid != HND_OOBR_CORE_ID) &&
				(cid != CCI400_CORE_ID) && (cid != SPMI_SLAVE_CORE_ID) &&
#if defined(__ARM_ARCH_7R__)
				(cid != SDTC_CORE_ID) &&
#endif /* __ARM_ARCH_7R__ */
				TRUE) {
				continue;
			}
		}

		idx = sii->numcores;

		cores_info->cia[idx] = cia;
		cores_info->cib[idx] = cib;
		cores_info->coreid[idx] = cid;

		/* workaround the fact the variable buscoretype is used in _ai_set_coreidx()
		 * when checking PCIE_GEN2() for PCI_BUS case before it is setup later...,
		 * both use and setup happen in si_buscore_setup().
		 */
		if (BUSTYPE(sih->bustype) == PCI_BUS &&
			(cid == PCI_CORE_ID || cid == PCIE_CORE_ID || cid == PCIE2_CORE_ID)) {
			sii->pub.buscoretype = (uint16)cid;
		}

		for (i = 0; i < nmp; i++) {
			mpd = get_erom_ent(sih, &eromptr, ER_VALID, ER_VALID);
			if ((mpd & ER_TAG) != ER_MP) {
				SI_ERROR(("Not enough MP entries for component 0x%x\n", cid));
				goto error;
			}
			/* Record something? */
			SI_VMSG(("  Master port %d, mp: %d id: %d\n", i,
				(mpd & MPD_MP_MASK) >> MPD_MP_SHIFT,
				(mpd & MPD_MUI_MASK) >> MPD_MUI_SHIFT));
		}

		/* First Slave Address Descriptor should be port 0:
		 * the main register space for the core
		 */
		asd = get_asd(sih, &eromptr, 0, 0, AD_ST_SLAVE, &addrl, &addrh, &sizel, &sizeh);
		if (asd == 0) {
			do {
			/* Try again to see if it is a bridge */
			asd = get_asd(sih, &eromptr, 0, 0, AD_ST_BRIDGE, &addrl, &addrh,
				&sizel, &sizeh);
			if (asd != 0)
				br = TRUE;
			else {
					break;
				}
			} while (1);
		} else {
			if (addrl == 0 || sizel == 0) {
				SI_ERROR((" Invalid ASD %x for slave port\n", asd));
				goto error;
			}
			cores_info->coresba[idx] = addrl;
			cores_info->coresba_size[idx] = sizel;
		}

		/* Get any more ASDs in first port */
		j = 1;
		do {
			asd = get_asd(sih, &eromptr, 0, j, AD_ST_SLAVE, &addrl, &addrh,
				&sizel, &sizeh);
			/* Support ARM debug core ASD with address space > 4K */
			if ((asd != 0) && (j == 1)) {
				SI_VMSG(("Warning: sizel > 0x1000\n"));
				cores_info->coresba2[idx] = addrl;
				cores_info->coresba2_size[idx] = sizel;
			}
			j++;
		} while (asd != 0);

		/* Go through the ASDs for other slave ports */
		for (i = 1; i < nsp; i++) {
			j = 0;
			do {
				asd = get_asd(sih, &eromptr, i, j, AD_ST_SLAVE, &addrl, &addrh,
					&sizel, &sizeh);
				/* To get the first base address of second slave port */
				if ((asd != 0) && (i == 1) && (j == 0)) {
					cores_info->csp2ba[idx] = addrl;
					cores_info->csp2ba_size[idx] = sizel;
				}
				if (asd == 0)
					break;
				j++;
			} while (1);
			if (j == 0) {
				SI_ERROR((" SP %d has no address descriptors\n", i));
				goto error;
			}
		}

		/* Now get master wrappers */
		for (i = 0; i < nmw; i++) {
			asd = get_asd(sih, &eromptr, i, 0, AD_ST_MWRAP, &addrl, &addrh,
				&sizel, &sizeh);
			if (asd == 0) {
				SI_ERROR(("Missing descriptor for MW %d\n", i));
				goto error;
			}
			if ((sizeh != 0) || (sizel != SI_CORE_SIZE)) {
				SI_ERROR(("Master wrapper %d is not 4KB\n", i));
				goto error;
			}
			if (i == 0) {
				cores_info->wrapba[idx] = addrl;
			} else if (i == 1) {
				cores_info->wrapba2[idx] = addrl;
			} else if (i == 2) {
				cores_info->wrapba3[idx] = addrl;
			}

			if (axi_wrapper && (sii->axi_num_wrappers < SI_MAX_AXI_WRAPPERS)) {
				axi_wrapper[sii->axi_num_wrappers].mfg = mfg;
				axi_wrapper[sii->axi_num_wrappers].cid = cid;
				axi_wrapper[sii->axi_num_wrappers].rev = crev;
				axi_wrapper[sii->axi_num_wrappers].wrapper_type = AI_MASTER_WRAPPER;
				axi_wrapper[sii->axi_num_wrappers].wrapper_addr = addrl;
				sii->axi_num_wrappers++;
				SI_VMSG(("MASTER WRAPPER: %d, mfg:%x, cid:%x,"
					"rev:%x, addr:%x, size:%x\n",
					sii->axi_num_wrappers, mfg, cid, crev, addrl, sizel));
			}
		}

		/* And finally slave wrappers */
		for (i = 0; i < nsw; i++) {
			uint fwp = (nsp <= 1) ? 0 : 1;
			asd = get_asd(sih, &eromptr, fwp + i, 0, AD_ST_SWRAP, &addrl, &addrh,
				&sizel, &sizeh);
			if (asd == 0) {
				SI_ERROR(("Missing descriptor for SW %d cid %x eromp %p fwp %d \n",
					i, cid, eromptr, fwp));
				goto error;
			}

			if ((sizeh != 0) || (sizel != SI_CORE_SIZE)) {
				SI_ERROR(("Slave wrapper %d is not 4KB\n", i));
				goto error;
			}

			/* cache APB bridge wrapper address for set/clear timeout */
			if ((mfg == MFGID_ARM) && (cid == APB_BRIDGE_ID)) {
				ASSERT(sii->num_br < SI_MAXBR);
				if (sii->num_br >= SI_MAXBR) {
					SI_ERROR(("bridge number %d is overflowed\n", sii->num_br));
					goto error;
				}

				sii->br_wrapba[sii->num_br++] = addrl;
			}

			if ((mfg == MFGID_ARM) && (cid == ADB_BRIDGE_ID)) {
				br = TRUE;
			}

			BCM_REFERENCE(br);

			if ((nmw == 0) && (i == 0)) {
				cores_info->wrapba[idx] = addrl;
			} else if ((nmw == 0) && (i == 1)) {
				cores_info->wrapba2[idx] = addrl;
			} else if ((nmw == 0) && (i == 2)) {
				cores_info->wrapba3[idx] = addrl;
			}

			/* Include all slave wrappers to the list to
			 * enable and monitor watchdog timeouts
			 */

			if (axi_wrapper && (sii->axi_num_wrappers < SI_MAX_AXI_WRAPPERS)) {
				axi_wrapper[sii->axi_num_wrappers].mfg = mfg;
				axi_wrapper[sii->axi_num_wrappers].cid = cid;
				axi_wrapper[sii->axi_num_wrappers].rev = crev;
				axi_wrapper[sii->axi_num_wrappers].wrapper_type = AI_SLAVE_WRAPPER;
				axi_wrapper[sii->axi_num_wrappers].wrapper_addr = addrl;

				sii->axi_num_wrappers++;

				SI_VMSG(("SLAVE WRAPPER: %d,  mfg:%x, cid:%x,"
					"rev:%x, addr:%x, size:%x\n",
					sii->axi_num_wrappers,  mfg, cid, crev, addrl, sizel));
			}
		}

		/* Don't record bridges and core with 0 slave ports */
		if (br || (nsp == 0)) {
			continue;
		}

		/* Done with core */
		sii->numcores++;
	}

	SI_ERROR(("Reached end of erom without finding END\n"));

error:
	sii->numcores = 0;
	SI_MSG_DBG_REG(("%s: Exit\n", __FUNCTION__));
	return;
}

#define AI_SETCOREIDX_MAPSIZE(coreid) \
	(((coreid) == NS_CCB_CORE_ID) ? 15 * SI_CORE_SIZE : SI_CORE_SIZE)

/* This function changes the logical "focus" to the indicated core.
 * Return the current core's virtual address.
 */
static volatile void *
BCMPOSTTRAPFN(_ai_setcoreidx)(si_t *sih, uint coreidx, uint use_wrapn)
{
	si_info_t *sii = SI_INFO(sih);
	si_cores_info_t *cores_info = (si_cores_info_t *)sii->cores_info;
	uint32 addr, wrap, wrap2, wrap3;
	volatile void *regs;

	if (coreidx >= MIN(sii->numcores, SI_MAXCORES))
		return (NULL);

	addr = cores_info->coresba[coreidx];
	wrap = cores_info->wrapba[coreidx];
	wrap2 = cores_info->wrapba2[coreidx];
	wrap3 = cores_info->wrapba3[coreidx];

	/*
	 * If the user has provided an interrupt mask enabled function,
	 * then assert interrupts are disabled before switching the core.
	 */
	ASSERT((sii->intrsenabled_fn == NULL) || !(*(sii)->intrsenabled_fn)((sii)->intr_arg));

	switch (BUSTYPE(sih->bustype)) {
	case SI_BUS:
		/* map new one */
		if (!cores_info->regs[coreidx]) {
			cores_info->regs[coreidx] = REG_MAP(addr,
				AI_SETCOREIDX_MAPSIZE(cores_info->coreid[coreidx]));
			ASSERT(GOODREGS(cores_info->regs[coreidx]));
		}
		sii->curmap = regs = cores_info->regs[coreidx];
		if (!cores_info->wrappers[coreidx] && (wrap != 0)) {
			cores_info->wrappers[coreidx] = REG_MAP(wrap, SI_CORE_SIZE);
			ASSERT(GOODREGS(cores_info->wrappers[coreidx]));
		}
		if (!cores_info->wrappers2[coreidx] && (wrap2 != 0)) {
			cores_info->wrappers2[coreidx] = REG_MAP(wrap2, SI_CORE_SIZE);
			ASSERT(GOODREGS(cores_info->wrappers2[coreidx]));
		}
		if (!cores_info->wrappers3[coreidx] && (wrap3 != 0)) {
			cores_info->wrappers3[coreidx] = REG_MAP(wrap3, SI_CORE_SIZE);
			ASSERT(GOODREGS(cores_info->wrappers3[coreidx]));
		}

		if (use_wrapn == 2) {
			sii->curwrap = cores_info->wrappers3[coreidx];
		} else if (use_wrapn == 1) {
			sii->curwrap = cores_info->wrappers2[coreidx];
		} else {
			sii->curwrap = cores_info->wrappers[coreidx];
		}
		break;

	case PCI_BUS:
		regs = sii->curmap;

		/* point bar0 2nd 4KB window to the primary wrapper */
		if (use_wrapn == 2) {
			wrap = wrap3;
		} else if (use_wrapn == 1) {
			wrap = wrap2;
		}

		/* Use BAR0 Window to support dual mac chips... */

		/* TODO: the other mac unit can't be supportd by the current BAR0 window.
		 * need to find other ways to access these cores.
		 */

		switch (sii->slice) {
		case 0: /* main/first slice */
			/* point bar0 window */
			OSL_PCI_WRITE_CONFIG(sii->osh, PCI_BAR0_WIN, 4, addr);

			if (PCIE_GEN2(sii))
				OSL_PCI_WRITE_CONFIG(sii->osh, PCIE2_BAR0_WIN2, 4, wrap);
			else
				OSL_PCI_WRITE_CONFIG(sii->osh, PCI_BAR0_WIN2, 4, wrap);

			break;

		case 1: /* aux/second slice */
			/* PCIE GEN2 only for other slices */
			if (!PCIE_GEN2(sii)) {
				/* other slices not supported */
				SI_ERROR(("PCI GEN not supported for slice %d\n", sii->slice));
				OSL_SYS_HALT();
				break;
			}

			/* 0x4000 - 0x4fff: enum space 0x5000 - 0x5fff: wrapper space */
			regs = (volatile uint8 *)regs + PCI_SEC_BAR0_WIN_OFFSET;
			sii->curwrap = (void *)((uintptr)regs + SI_CORE_SIZE);

			/* point bar0 window */
			OSL_PCI_WRITE_CONFIG(sii->osh, PCIE2_BAR0_CORE2_WIN, 4, addr);
			OSL_PCI_WRITE_CONFIG(sii->osh, PCIE2_BAR0_CORE2_WIN2, 4, wrap);
			break;

		case 2: /* scan/third slice */
			/* PCIE GEN2 only for other slices */
			if (!PCIE_GEN2(sii)) {
				/* other slices not supported */
				SI_ERROR(("PCI GEN not supported for slice %d\n", sii->slice));
				OSL_SYS_HALT();
				break;
			}

			/* 0x9000 - 0x9fff: enum space 0xa000 - 0xafff: wrapper space */
			regs = (volatile uint8 *)regs + PCI_TER_BAR0_WIN_OFFSET;
			sii->curwrap = (void *)((uintptr)regs + SI_CORE_SIZE);

			/* point bar0 window */
			ai_corereg(sih, sih->buscoreidx,
				PCIE_TER_BAR0_WIN_REG(sih->buscorerev), ~0, addr);
			ai_corereg(sih, sih->buscoreidx,
				PCIE_TER_BAR0_WRAPPER_REG(sih->buscorerev), ~0, wrap);
			break;

		default: /* other slices */
			SI_ERROR(("BAR0 Window not supported for slice %d\n", sii->slice));
			OSL_SYS_HALT();
			break;
		}

		break;

#ifdef BCMSDIO
	case SPI_BUS:
	case SDIO_BUS:
		sii->curmap = regs = (void *)((uintptr)addr);
		if (use_wrapn)
			sii->curwrap = (void *)((uintptr)wrap2);
		else
			sii->curwrap = (void *)((uintptr)wrap);
		break;
#endif	/* BCMSDIO */

	default:
		OSL_SYS_HALT();
		sii->curmap = regs = NULL;
		break;
	}

	sii->curidx = coreidx;

	if (regs) {
		SI_MSG_DBG_REG(("%s: %d\n", __FUNCTION__, coreidx));
	}

	return regs;
}

volatile void *
BCMPOSTTRAPFN(ai_setcoreidx)(si_t *sih, uint coreidx)
{
	return _ai_setcoreidx(sih, coreidx, 0);
}

volatile void *
BCMPOSTTRAPFN(ai_setcoreidx_2ndwrap)(si_t *sih, uint coreidx)
{
	return _ai_setcoreidx(sih, coreidx, 1);
}

volatile void *
BCMPOSTTRAPFN(ai_setcoreidx_3rdwrap)(si_t *sih, uint coreidx)
{
	return _ai_setcoreidx(sih, coreidx, 2);
}

void
ai_coreaddrspaceX(const si_t *sih, uint asidx, uint32 *addr, uint32 *size)
{
	const si_info_t *sii = SI_INFO(sih);
	const si_cores_info_t *cores_info = (const si_cores_info_t *)sii->cores_info;
	chipcregs_t *cc = NULL;
	uint32 erombase, *eromptr, *eromlim;
	uint i, j, cidx;
	uint32 cia, cib, nmp, nsp;
	uint32 asd, addrl, addrh, sizel, sizeh;

	for (i = 0; i < sii->numcores; i++) {
		if (cores_info->coreid[i] == CC_CORE_ID) {
			cc = (chipcregs_t *)cores_info->regs[i];
			break;
		}
	}
	if (cc == NULL)
		goto error;

	BCM_REFERENCE(erombase);
	erombase = R_REG(sii->osh, CC_REG_ADDR(cc, EromPtrOffset));
	eromptr = (uint32 *)REG_MAP(erombase, SI_CORE_SIZE);
	eromlim = eromptr + (ER_REMAPCONTROL / sizeof(uint32));

	cidx = sii->curidx;
	cia = cores_info->cia[cidx];
	cib = cores_info->cib[cidx];

	nmp = (cib & CIB_NMP_MASK) >> CIB_NMP_SHIFT;
	nsp = (cib & CIB_NSP_MASK) >> CIB_NSP_SHIFT;

	/* scan for cores */
	while (eromptr < eromlim) {
		if ((get_erom_ent(sih, &eromptr, ER_TAG, ER_CI) == cia) &&
			(get_erom_ent(sih, &eromptr, 0, 0) == cib)) {
			break;
		}
	}

	/* skip master ports */
	for (i = 0; i < nmp; i++)
		get_erom_ent(sih, &eromptr, ER_VALID, ER_VALID);

	/* Skip ASDs in port 0 */
	asd = get_asd(sih, &eromptr, 0, 0, AD_ST_SLAVE, &addrl, &addrh, &sizel, &sizeh);
	if (asd == 0) {
		/* Try again to see if it is a bridge */
		asd = get_asd(sih, &eromptr, 0, 0, AD_ST_BRIDGE, &addrl, &addrh,
			&sizel, &sizeh);
	}

	j = 1;
	do {
		asd = get_asd(sih, &eromptr, 0, j, AD_ST_SLAVE, &addrl, &addrh,
			&sizel, &sizeh);
		j++;
	} while (asd != 0);

	/* Go through the ASDs for other slave ports */
	for (i = 1; i < nsp; i++) {
		j = 0;
		do {
			asd = get_asd(sih, &eromptr, i, j, AD_ST_SLAVE, &addrl, &addrh,
				&sizel, &sizeh);
			if (asd == 0)
				break;

			if (!asidx--) {
				*addr = addrl;
				*size = sizel;
				return;
			}
			j++;
		} while (1);

		if (j == 0) {
			SI_ERROR((" SP %d has no address descriptors\n", i));
			break;
		}
	}

error:
	*size = 0;
	return;
}

/* Return the number of address spaces in current core */
int
ai_numaddrspaces(const si_t *sih)
{
	/* TODO: Either save it or parse the EROM on demand, currently hardcode 2 */
	BCM_REFERENCE(sih);

	return 2;
}

/* Return the address of the nth address space in the current core
 * Arguments:
 * sih : Pointer to struct si_t
 * spidx : slave port index
 * baidx : base address index
 */
uint32
BCMPOSTTRAPFN(ai_addrspace)(const si_t *sih, uint spidx, uint baidx)
{
	const si_info_t *sii = SI_INFO(sih);
	const si_cores_info_t *cores_info = (const si_cores_info_t *)sii->cores_info;
	uint cidx;

	cidx = sii->curidx;

	if (spidx == CORE_SLAVE_PORT_0) {
		if (baidx == CORE_BASE_ADDR_0)
			return cores_info->coresba[cidx];
		else if (baidx == CORE_BASE_ADDR_1)
			return cores_info->coresba2[cidx];
	} else if (spidx == CORE_SLAVE_PORT_1) {
		if (baidx == CORE_BASE_ADDR_0)
			return cores_info->csp2ba[cidx];
	}

	SI_ERROR(("ai_addrspace: Need to parse the erom again to find %d"
		" base addr in %d slave port\n", baidx, spidx));

	return 0;

}

/* Return the size of the nth address space in the current core
* Arguments:
* sih : Pointer to struct si_t
* spidx : slave port index
* baidx : base address index
*/
uint32
BCMPOSTTRAPFN(ai_addrspacesize)(const si_t *sih, uint spidx, uint baidx)
{
	const si_info_t *sii = SI_INFO(sih);
	const si_cores_info_t *cores_info = (const si_cores_info_t *)sii->cores_info;
	uint cidx;

	cidx = sii->curidx;
	if (spidx == CORE_SLAVE_PORT_0) {
		if (baidx == CORE_BASE_ADDR_0)
			return cores_info->coresba_size[cidx];
		else if (baidx == CORE_BASE_ADDR_1)
			return cores_info->coresba2_size[cidx];
	} else if (spidx == CORE_SLAVE_PORT_1) {
		if (baidx == CORE_BASE_ADDR_0)
			return cores_info->csp2ba_size[cidx];
	}

	SI_ERROR(("ai_addrspacesize: Need to parse the erom again to find %d"
		" base addr in %d slave port\n",
		baidx, spidx));

	return 0;
}

uint
ai_flag(si_t *sih)
{
	const si_info_t *sii = SI_INFO(sih);
	aidmp_t *ai;

	if (PMU_DMP(sii)) {
		uint idx, flag;
		idx = sii->curidx;
		ai_setcoreidx(sih, SI_CC_IDX);
		flag = ai_flag_alt(sih);
		ai_setcoreidx(sih, idx);
		return flag;
	}

	ai = sii->curwrap;
	ASSERT(ai != NULL);

	return (R_REG(sii->osh, &ai->oobselouta30) & 0x1f);
}

uint
ai_flag_alt(const si_t *sih)
{
	const si_info_t *sii = SI_INFO(sih);
	aidmp_t *ai = sii->curwrap;

	return ((R_REG(sii->osh, &ai->oobselouta30) >> AI_OOBSEL_1_SHIFT) & AI_OOBSEL_MASK);
}

void
ai_setint(const si_t *sih, int siflag)
{
	BCM_REFERENCE(sih);
	BCM_REFERENCE(siflag);

	/* TODO: Figure out how to set interrupt mask in ai */
}

uint
BCMPOSTTRAPFN(ai_wrap_reg)(const si_t *sih, uint32 offset, uint32 mask, uint32 val)
{
	const si_info_t *sii = SI_INFO(sih);
	uint32 *addr = (uint32 *) ((uintptr)(sii->curwrap) + offset);

	if (mask || val) {
		uint32 w = R_REG(sii->osh, addr);
		w &= ~mask;
		w |= val;
		W_REG(sii->osh, addr, w);
	}
	return (R_REG(sii->osh, addr));
}

uint
ai_corevendor(const si_t *sih)
{
	const si_info_t *sii = SI_INFO(sih);
	const si_cores_info_t *cores_info = (const si_cores_info_t *)sii->cores_info;
	uint32 cia;

	cia = cores_info->cia[sii->curidx];
	return ((cia & CIA_MFG_MASK) >> CIA_MFG_SHIFT);
}

uint
BCMPOSTTRAPFN(ai_corerev)(const si_t *sih)
{
	const si_info_t *sii = SI_INFO(sih);
	const si_cores_info_t *cores_info = (const si_cores_info_t *)sii->cores_info;
	uint32 cib;

	cib = cores_info->cib[sii->curidx];
	return ((cib & CIB_REV_MASK) >> CIB_REV_SHIFT);
}

uint
ai_corerev_minor(const si_t *sih)
{
	return (ai_core_sflags(sih, 0, 0) >> SISF_MINORREV_D11_SHIFT) &
			SISF_MINORREV_D11_MASK;
}

bool
BCMPOSTTRAPFN(ai_iscoreup)(const si_t *sih)
{
	const si_info_t *sii = SI_INFO(sih);
	aidmp_t *ai = sii->curwrap;

	return (((R_REG(sii->osh, &ai->ioctrl) & (SICF_FGC | SICF_CLOCK_EN)) == SICF_CLOCK_EN) &&
		((R_REG(sii->osh, &ai->resetctrl) & AIRC_RESET) == 0));
}

/*
 * Switch to 'coreidx', issue a single arbitrary 32bit register mask&set operation,
 * switch back to the original core, and return the new value.
 *
 * When using the silicon backplane, no fiddling with interrupts or core switches is needed.
 *
 * Also, when using pci/pcie, we can optimize away the core switching for pci registers
 * and (on newer pci cores) chipcommon registers.
 */
uint
BCMPOSTTRAPFN(ai_corereg)(si_t *sih, uint coreidx, uint regoff, uint mask, uint val)
{
	uint origidx = 0;
	volatile uint32 *r = NULL;
	uint w;
	bcm_int_bitmask_t intr_val;
	bool fast = FALSE;
	si_info_t *sii = SI_INFO(sih);
	si_cores_info_t *cores_info = (si_cores_info_t *)sii->cores_info;

	ASSERT_FP(GOODIDX(coreidx, sii->numcores) &&
		(regoff < SI_CORE_SIZE) &&
		((val & ~mask) == 0));

	if (coreidx >= SI_MAXCORES)
		return 0;

	if (BUSTYPE(sih->bustype) == SI_BUS) {
		/* If internal bus, we can always get at everything */
		fast = TRUE;
		/* map if does not exist */
		if (!cores_info->regs[coreidx]) {
			cores_info->regs[coreidx] = REG_MAP(cores_info->coresba[coreidx],
				SI_CORE_SIZE);
			ASSERT_FP(GOODREGS(cores_info->regs[coreidx]));
		}
		r = (volatile uint32 *)((volatile uchar *)cores_info->regs[coreidx] + regoff);
	} else if (BUSTYPE(sih->bustype) == PCI_BUS) {
		/* If pci/pcie, we can get at pci/pcie regs and on newer cores to chipc */

		if ((cores_info->coreid[coreidx] == CC_CORE_ID) && SI_FAST(sii)) {
			/* Chipc registers are mapped at 12KB */

			fast = TRUE;
			r = (volatile uint32 *)((volatile char *)sii->curmap +
				PCI_16KB0_CCREGS_OFFSET + regoff);
		} else if (sii->pub.buscoreidx == coreidx) {
			/* pci registers are at either in the last 2KB of an 8KB window
			 * or, in pcie and pci rev 13 at 8KB
			 */
			fast = TRUE;
			if (SI_FAST(sii))
				r = (volatile uint32 *)((volatile char *)sii->curmap +
				PCI_16KB0_PCIREGS_OFFSET + regoff);
			else
				r = (volatile uint32 *)((volatile char *)sii->curmap +
					((regoff >= SBCONFIGOFF) ?
					PCI_BAR0_PCISBR_OFFSET : PCI_BAR0_PCIREGS_OFFSET) +
					regoff);
		}
	}

	if (!fast) {
		INTR_OFF(sii, &intr_val);

		/* save current core index */
		origidx = si_coreidx(&sii->pub);

		/* switch core */
		r = (volatile uint32 *) ((volatile uchar *) ai_setcoreidx(&sii->pub, coreidx) +
			regoff);
	}

	/* mask and set */
	if (mask || val) {
		w = (R_REG(sii->osh, r) & ~mask) | val;
		W_REG(sii->osh, r, w);
	}

	/* readback */
	w = R_REG(sii->osh, r);

	if (!fast) {
		/* restore core index */
		if (origidx != coreidx)
			ai_setcoreidx(&sii->pub, origidx);

		INTR_RESTORE(sii, &intr_val);
	}

	return (w);
}

uint
BCMPOSTTRAPFN(ai_corereg_writearr)(si_t *sih, uint coreidx, uint regoff, uint *mask, uint *val,
		uint num_vals)
{
	volatile uint32 *r = NULL;
	uint w, i = 0;
	si_info_t *sii = SI_INFO(sih);
	si_cores_info_t *cores_info = (si_cores_info_t *)sii->cores_info;

	if (coreidx >= SI_MAXCORES) {
		return 0;
	}

	ASSERT_FP(GOODIDX(coreidx, sii->numcores) &&
		(regoff < SI_CORE_SIZE));

	for (i = 0; i < num_vals; i++) {
		ASSERT_FP(((val[i] & ~mask[i]) == 0));
	}

	if (BUSTYPE(sih->bustype) == SI_BUS) {
		/* map if does not exist */
		if (!cores_info->regs[coreidx]) {
			cores_info->regs[coreidx] = REG_MAP(cores_info->coresba[coreidx],
				SI_CORE_SIZE);
			ASSERT_FP(GOODREGS(cores_info->regs[coreidx]));
		}
		r = (volatile uint32 *)((volatile uchar *)cores_info->regs[coreidx] + regoff);
	} else {
		ASSERT(0);
	}

	/* mask and set */
	/* This is to allow for back to back 'n' writes to the same address.
	 * This helps in toggling a few bits and then restoring the same value.
	 * The goal is to eliminate any overhead due to function calls between 'n' writes.
	 * This new implementation saves 1 us for each additional write over existing method of
	 * calling si_gci_direct for each write from top level
	 */
	for (i = 0; i < num_vals; i++) {
		if (mask[i] || val[i]) {
			if (~mask[i] != 0) {
				w = (R_REG(sii->osh, r) & ~mask[i]) | val[i];
			} else {
				w = val[i];
			}
			W_REG(sii->osh, r, w);
		}
	}

	/* readback */
	w = R_REG(sii->osh, r);

	return w;
}

/*
 * Switch to 'coreidx', issue a single arbitrary 32bit register mask&set operation,
 * switch back to the original core, and return the new value.
 *
 * When using the silicon backplane, no fiddling with interrupts or core switches is needed.
 *
 * Also, when using pci/pcie, we can optimize away the core switching for pci registers
 * and (on newer pci cores) chipcommon registers.
 */
uint
ai_corereg_writeonly(si_t *sih, uint coreidx, uint regoff, uint mask, uint val)
{
	uint origidx = 0;
	volatile uint32 *r = NULL;
	uint w = 0;
	bcm_int_bitmask_t intr_val;
	bool fast = FALSE;
	si_info_t *sii = SI_INFO(sih);
	si_cores_info_t *cores_info = (si_cores_info_t *)sii->cores_info;

	ASSERT(GOODIDX(coreidx, sii->numcores));
	ASSERT(regoff < SI_CORE_SIZE);
	ASSERT((val & ~mask) == 0);

	if (coreidx >= SI_MAXCORES)
		return 0;

	if (BUSTYPE(sih->bustype) == SI_BUS) {
		/* If internal bus, we can always get at everything */
		fast = TRUE;
		/* map if does not exist */
		if (!cores_info->regs[coreidx]) {
			cores_info->regs[coreidx] = REG_MAP(cores_info->coresba[coreidx],
				SI_CORE_SIZE);
			ASSERT(GOODREGS(cores_info->regs[coreidx]));
		}
		r = (volatile uint32 *)((volatile uchar *)cores_info->regs[coreidx] + regoff);
	} else if (BUSTYPE(sih->bustype) == PCI_BUS) {
		/* If pci/pcie, we can get at pci/pcie regs and on newer cores to chipc */

		if ((cores_info->coreid[coreidx] == CC_CORE_ID) && SI_FAST(sii)) {
			/* Chipc registers are mapped at 12KB */

			fast = TRUE;
			r = (volatile uint32 *)((volatile char *)sii->curmap +
				PCI_16KB0_CCREGS_OFFSET + regoff);
		} else if (sii->pub.buscoreidx == coreidx) {
			/* pci registers are at either in the last 2KB of an 8KB window
			 * or, in pcie and pci rev 13 at 8KB
			 */
			fast = TRUE;
			if (SI_FAST(sii))
				r = (volatile uint32 *)((volatile char *)sii->curmap +
					PCI_16KB0_PCIREGS_OFFSET + regoff);
			else
				r = (volatile uint32 *)((volatile char *)sii->curmap +
					((regoff >= SBCONFIGOFF) ?
					PCI_BAR0_PCISBR_OFFSET : PCI_BAR0_PCIREGS_OFFSET) +
					regoff);
		}
	}

	if (!fast) {
		INTR_OFF(sii, &intr_val);

		/* save current core index */
		origidx = si_coreidx(&sii->pub);

		/* switch core */
		r = (volatile uint32 *) ((volatile uchar *) ai_setcoreidx(&sii->pub, coreidx) +
			regoff);
	}
	ASSERT(r != NULL);

	/* mask and set */
	if (mask || val) {
		w = (R_REG(sii->osh, r) & ~mask) | val;
		W_REG(sii->osh, r, w);
	}

	if (!fast) {
		/* restore core index */
		if (origidx != coreidx)
			ai_setcoreidx(&sii->pub, origidx);

		INTR_RESTORE(sii, &intr_val);
	}

	return (w);
}

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
BCMPOSTTRAPFN(ai_corereg_addr)(si_t *sih, uint coreidx, uint regoff)
{
	volatile uint32 *r = NULL;
	bool fast = FALSE;
	si_info_t *sii = SI_INFO(sih);
	si_cores_info_t *cores_info = (si_cores_info_t *)sii->cores_info;

	ASSERT(GOODIDX(coreidx, sii->numcores));
	ASSERT(regoff < SI_CORE_SIZE);

	if (coreidx >= SI_MAXCORES)
		return 0;

	if (BUSTYPE(sih->bustype) == SI_BUS) {
		/* If internal bus, we can always get at everything */
		fast = TRUE;
		/* map if does not exist */
		if (!cores_info->regs[coreidx]) {
			cores_info->regs[coreidx] = REG_MAP(cores_info->coresba[coreidx],
				SI_CORE_SIZE);
			ASSERT(GOODREGS(cores_info->regs[coreidx]));
		}
		r = (volatile uint32 *)((volatile uchar *)cores_info->regs[coreidx] + regoff);
	} else if (BUSTYPE(sih->bustype) == PCI_BUS) {
		/* If pci/pcie, we can get at pci/pcie regs and on newer cores to chipc */

		if ((cores_info->coreid[coreidx] == CC_CORE_ID) && SI_FAST(sii)) {
			/* Chipc registers are mapped at 12KB */

			fast = TRUE;
			r = (volatile uint32 *)((volatile char *)sii->curmap +
				PCI_16KB0_CCREGS_OFFSET + regoff);
		} else if (sii->pub.buscoreidx == coreidx) {
			/* pci registers are at either in the last 2KB of an 8KB window
			 * or, in pcie and pci rev 13 at 8KB
			 */
			fast = TRUE;
			if (SI_FAST(sii))
				r = (volatile uint32 *)((volatile char *)sii->curmap +
					PCI_16KB0_PCIREGS_OFFSET + regoff);
			else
				r = (volatile uint32 *)((volatile char *)sii->curmap +
					((regoff >= SBCONFIGOFF) ?
					PCI_BAR0_PCISBR_OFFSET : PCI_BAR0_PCIREGS_OFFSET) +
					regoff);
		}
	}

	if (!fast) {
		ASSERT(sii->curidx == coreidx);
		r = (volatile uint32 *) ((volatile uchar *)sii->curmap + regoff);
	}

	return (r);
}

void
ai_core_disable(const si_t *sih, uint32 bits)
{
	const si_info_t *sii = SI_INFO(sih);
	volatile uint32 dummy;
	uint32 status;
	aidmp_t *ai;

	ASSERT(GOODREGS(sii->curwrap));
	ai = sii->curwrap;

	/* if core is already in reset, just return */
	if (R_REG(sii->osh, &ai->resetctrl) & AIRC_RESET) {
		return;
	}

	/* ensure there are no pending backplane operations */
	SPINWAIT(((status = R_REG(sii->osh, &ai->resetstatus)) != 0), 300);

	/* if pending backplane ops still, try waiting longer */
	if (status != 0) {
		/* 300usecs was sufficient to allow backplane ops to clear for big hammer */
		/* during driver load we may need more time */
		SPINWAIT(((status = R_REG(sii->osh, &ai->resetstatus)) != 0), 10000);
		/* if still pending ops, continue on and try disable anyway */
		/* this is in big hammer path, so don't call wl_reinit in this case... */
#ifdef BCMDBG_ERR
		if (status != 0) {
			SI_ERROR(("ai_core_disable: WARN: %p resetstatus=%0x on core disable\n", ai,
				status));
		}
#endif /* BCMDBG_ERR */
	}

	W_REG(sii->osh, &ai->resetctrl, AIRC_RESET);
	dummy = R_REG(sii->osh, &ai->resetctrl);
	BCM_REFERENCE(dummy);
	OSL_DELAY(1);

	W_REG(sii->osh, &ai->ioctrl, bits);
	dummy = R_REG(sii->osh, &ai->ioctrl);
	BCM_REFERENCE(dummy);
	OSL_DELAY(10);
}

/* reset and re-enable a core
 * inputs:
 * bits - core specific bits that are set during and after reset sequence
 * resetbits - core specific bits that are set only during reset sequence
 */

static bool
BCMPOSTTRAPFN(_ai_core_reset)(const si_t *sih, uint32 bits, uint32 resetbits)
{
	const si_info_t *sii = SI_INFO(sih);
	aidmp_t *ai;
	volatile uint32 dummy;
	uint loop_counter = 10;

	ASSERT(GOODREGS(sii->curwrap));
	ai = sii->curwrap;

	if (R_REG(sii->osh, &ai->resetstatus) == 0xffffffff &&
		R_REG(sii->osh, &ai->ioctrl) == 0xffffffff &&
		R_REG(sii->osh, &ai->resetctrl) == 0xffffffff) {
		SI_ERROR(("%s: fail, resetstatus&ioctrl&resetctrl is 0xffffffff\n", __func__));
		return FALSE;
	}
	/* ensure there are no pending backplane operations */
	SPINWAIT(((dummy = R_REG(sii->osh, &ai->resetstatus)) != 0), 300);

#ifdef BCMDBG_ERR
	if (dummy != 0) {
		SI_ERROR(("_ai_core_reset: WARN%d: %p resetstatus=0x%0x\n", 1, ai, dummy));
	}
#endif /* BCMDBG_ERR */

	SI_ERROR(("%s: &ai->ioctrl = 0x%x, &ai->resetctrl = 0x%x, &ai->resetstatus = 0x%x\n",
			__func__,
			R_REG(sii->osh, &ai->ioctrl),
			R_REG(sii->osh, &ai->resetctrl),
			R_REG(sii->osh, &ai->resetstatus)));

	/* put core into reset state */
	W_REG(sii->osh, &ai->resetctrl, AIRC_RESET);
	OSL_DELAY(10);

	/* ensure there are no pending backplane operations */
	SPINWAIT((R_REG(sii->osh, &ai->resetstatus) != 0), 300);

	W_REG(sii->osh, &ai->ioctrl, (bits | resetbits | SICF_FGC | SICF_CLOCK_EN));
	dummy = R_REG(sii->osh, &ai->ioctrl);
	BCM_REFERENCE(dummy);
#ifdef UCM_CORRUPTION_WAR
	if (si_coreid(sih) == D11_CORE_ID) {
		/* Reset FGC */
		OSL_DELAY(1);
		W_REG(sii->osh, &ai->ioctrl, (dummy & (~SICF_FGC)));
	}
#endif /* UCM_CORRUPTION_WAR */
	/* ensure there are no pending backplane operations */
	SPINWAIT(((dummy = R_REG(sii->osh, &ai->resetstatus)) != 0), 300);

#ifdef BCMDBG_ERR
	if (dummy != 0)
		SI_ERROR(("_ai_core_reset: WARN%d: %p resetstatus=0x%0x\n", 2, ai, dummy));
#endif

	while (R_REG(sii->osh, &ai->resetctrl) != 0 && --loop_counter != 0) {
		/* ensure there are no pending backplane operations */
		SPINWAIT(((dummy = R_REG(sii->osh, &ai->resetstatus)) != 0), 300);

#ifdef BCMDBG_ERR
		if (dummy != 0)
			SI_ERROR(("_ai_core_reset: WARN%d: %p resetstatus=0x%0x\n", 3, ai, dummy));
#endif

		/* take core out of reset */
		W_REG(sii->osh, &ai->resetctrl, 0);

		/* ensure there are no pending backplane operations */
		SPINWAIT((R_REG(sii->osh, &ai->resetstatus) != 0), 300);
	}

#ifdef BCMDBG_ERR
	if (loop_counter == 0) {
		SI_ERROR(("_ai_core_reset: %p Failed to take core 0x%x out of reset\n", ai,
			si_coreid(sih)));
	}
#endif /* BCMDBG_ERR */

#ifdef UCM_CORRUPTION_WAR
	/* Pulse FGC after lifting Reset */
	W_REG(sii->osh, &ai->ioctrl, (bits | SICF_FGC | SICF_CLOCK_EN));
#else
	W_REG(sii->osh, &ai->ioctrl, (bits | SICF_CLOCK_EN));
#endif /* UCM_CORRUPTION_WAR */
	dummy = R_REG(sii->osh, &ai->ioctrl);
	BCM_REFERENCE(dummy);
#ifdef UCM_CORRUPTION_WAR
	if (si_coreid(sih) == D11_CORE_ID) {
		/* Reset FGC */
		OSL_DELAY(1);
		W_REG(sii->osh, &ai->ioctrl, (dummy & (~SICF_FGC)));
	}
#endif /* UCM_CORRUPTION_WAR */
	OSL_DELAY(1);
	return TRUE;
}

bool
ai_core_reset(si_t *sih, uint32 bits, uint32 resetbits)
{
	si_info_t *sii = SI_INFO(sih);
	const si_cores_info_t *cores_info = (const si_cores_info_t *)sii->cores_info;
	uint idx = sii->curidx;
	bool ret = TRUE;

	if (cores_info->wrapba3[idx] != 0) {
		ai_setcoreidx_3rdwrap(sih, idx);
		ret = _ai_core_reset(sih, bits, resetbits);
		ai_setcoreidx(sih, idx);
	}

	if (cores_info->wrapba2[idx] != 0) {
		ai_setcoreidx_2ndwrap(sih, idx);
		ret = _ai_core_reset(sih, bits, resetbits);
		ai_setcoreidx(sih, idx);
	}

	ret = _ai_core_reset(sih, bits, resetbits);
	return ret;
}

void
ai_core_cflags_wo(const si_t *sih, uint32 mask, uint32 val)
{
	const si_info_t *sii = SI_INFO(sih);
	aidmp_t *ai;
	uint32 w;

	if (PMU_DMP(sii)) {
		SI_ERROR(("ai_core_cflags_wo: Accessing PMU DMP register (ioctrl)\n"));
		return;
	}

	ASSERT(GOODREGS(sii->curwrap));
	ai = sii->curwrap;

	ASSERT((val & ~mask) == 0);

	if (mask || val) {
		w = ((R_REG(sii->osh, &ai->ioctrl) & ~mask) | val);
		W_REG(sii->osh, &ai->ioctrl, w);
	}
}

uint32
BCMPOSTTRAPFN(ai_core_cflags)(const si_t *sih, uint32 mask, uint32 val)
{
	const si_info_t *sii = SI_INFO(sih);
	aidmp_t *ai;
	uint32 w;

	if (PMU_DMP(sii)) {
		SI_ERROR(("ai_core_cflags: Accessing PMU DMP register (ioctrl)\n"));
		return 0;
	}
	ASSERT(GOODREGS(sii->curwrap));
	ai = sii->curwrap;

	ASSERT((val & ~mask) == 0);

	if (mask || val) {
		w = ((R_REG(sii->osh, &ai->ioctrl) & ~mask) | val);
		W_REG(sii->osh, &ai->ioctrl, w);
	}

	return R_REG(sii->osh, &ai->ioctrl);
}

uint32
ai_core_sflags(const si_t *sih, uint32 mask, uint32 val)
{
	const si_info_t *sii = SI_INFO(sih);
	aidmp_t *ai;
	uint32 w;

	if (PMU_DMP(sii)) {
		SI_ERROR(("ai_core_sflags: Accessing PMU DMP register (ioctrl)\n"));
		return 0;
	}

	ASSERT(GOODREGS(sii->curwrap));
	ai = sii->curwrap;

	ASSERT((val & ~mask) == 0);
	ASSERT((mask & ~SISF_CORE_BITS) == 0);

	if (mask || val) {
		w = ((R_REG(sii->osh, &ai->iostatus) & ~mask) | val);
		W_REG(sii->osh, &ai->iostatus, w);
	}

	return R_REG(sii->osh, &ai->iostatus);
}

#if defined(BCMDBG) || defined(BCMDBG_DUMP) || defined(BCMDBG_PHYDUMP)
/* print interesting aidmp registers */
void
ai_dumpregs(const si_t *sih, struct bcmstrbuf *b)
{
	const si_info_t *sii = SI_INFO(sih);
	osl_t *osh;
	aidmp_t *ai;
	uint i;
	uint32 prev_value = 0;
	const axi_wrapper_t *axi_wrapper = sii->axi_wrapper;
	uint32 cfg_reg = 0;
	uint bar0_win_offset = 0;

	osh = sii->osh;

	/* Save and restore wrapper access window */
	if (BUSTYPE(sii->pub.bustype) == PCI_BUS) {
		if (PCIE_GEN2(sii)) {
			cfg_reg = PCIE2_BAR0_CORE2_WIN2;
			bar0_win_offset = PCIE2_BAR0_CORE2_WIN2_OFFSET;
		} else {
			cfg_reg = PCI_BAR0_WIN2;
			bar0_win_offset = PCI_BAR0_WIN2_OFFSET;
		}

		prev_value = OSL_PCI_READ_CONFIG(osh, cfg_reg, 4);

		if (prev_value == ID32_INVALID) {
			SI_PRINT(("ai_dumpregs, PCI_BAR0_WIN2 - %x\n", prev_value));
			return;
		}
	}

	bcm_bprintf(b, "ChipNum:%x, ChipRev;%x, BusType:%x, BoardType:%x, BoardVendor:%x\n\n",
		sih->chip, sih->chiprev, sih->bustype, sih->boardtype, sih->boardvendor);

	for (i = 0; i < sii->axi_num_wrappers; i++) {

		if (BUSTYPE(sii->pub.bustype) == PCI_BUS) {
			/* Set BAR0 window to bridge wapper base address */
			OSL_PCI_WRITE_CONFIG(osh,
				cfg_reg, 4, axi_wrapper[i].wrapper_addr);

			ai = (aidmp_t *) ((volatile uint8*)sii->curmap + bar0_win_offset);
		} else {
			ai = (aidmp_t *)(uintptr) axi_wrapper[i].wrapper_addr;
		}

		bcm_bprintf(b, "core 0x%x: core_rev:%d, %s_WR ADDR:%x \n", axi_wrapper[i].cid,
			axi_wrapper[i].rev,
			axi_wrapper[i].wrapper_type == AI_SLAVE_WRAPPER ? "SLAVE" : "MASTER",
			axi_wrapper[i].wrapper_addr);

		bcm_bprintf(b, "ioctrlset 0x%x ioctrlclear 0x%x ioctrl 0x%x iostatus 0x%x "
			"ioctrlwidth 0x%x iostatuswidth 0x%x\n"
			"resetctrl 0x%x resetstatus 0x%x resetreadid 0x%x resetwriteid 0x%x\n"
			"errlogctrl 0x%x errlogdone 0x%x errlogstatus 0x%x "
			"errlogaddrlo 0x%x errlogaddrhi 0x%x\n"
			"errlogid 0x%x errloguser 0x%x errlogflags 0x%x\n"
			"intstatus 0x%x config 0x%x itcr 0x%x\n\n",
			R_REG(osh, &ai->ioctrlset),
			R_REG(osh, &ai->ioctrlclear),
			R_REG(osh, &ai->ioctrl),
			R_REG(osh, &ai->iostatus),
			R_REG(osh, &ai->ioctrlwidth),
			R_REG(osh, &ai->iostatuswidth),
			R_REG(osh, &ai->resetctrl),
			R_REG(osh, &ai->resetstatus),
			R_REG(osh, &ai->resetreadid),
			R_REG(osh, &ai->resetwriteid),
			R_REG(osh, &ai->errlogctrl),
			R_REG(osh, &ai->errlogdone),
			R_REG(osh, &ai->errlogstatus),
			R_REG(osh, &ai->errlogaddrlo),
			R_REG(osh, &ai->errlogaddrhi),
			R_REG(osh, &ai->errlogid),
			R_REG(osh, &ai->errloguser),
			R_REG(osh, &ai->errlogflags),
			R_REG(osh, &ai->intstatus),
			R_REG(osh, &ai->config),
			R_REG(osh, &ai->itcr));
	}

	/* Restore the initial wrapper space */
	if (BUSTYPE(sii->pub.bustype) == PCI_BUS) {
		if (prev_value && cfg_reg) {
			OSL_PCI_WRITE_CONFIG(osh, cfg_reg, 4, prev_value);
		}
	}
}
#endif	/* BCMDBG || BCMDBG_DUMP || BCMDBG_PHYDUMP */

#ifdef BCMDBG
static void
_ai_view(osl_t *osh, aidmp_t *ai, uint32 cid, uint32 addr, bool verbose)
{
	uint32 config;

	config = R_REG(osh, &ai->config);
	SI_PRINT(("\nCore ID: 0x%x, addr 0x%x, config 0x%x\n", cid, addr, config));

	if (config & AICFG_RST)
		SI_PRINT(("resetctrl 0x%x, resetstatus 0x%x, resetreadid 0x%x, resetwriteid 0x%x\n",
			R_REG(osh, &ai->resetctrl), R_REG(osh, &ai->resetstatus),
			R_REG(osh, &ai->resetreadid), R_REG(osh, &ai->resetwriteid)));

	if (config & AICFG_IOC)
		SI_PRINT(("ioctrl 0x%x, width %d\n", R_REG(osh, &ai->ioctrl),
			R_REG(osh, &ai->ioctrlwidth)));

	if (config & AICFG_IOS)
		SI_PRINT(("iostatus 0x%x, width %d\n", R_REG(osh, &ai->iostatus),
			R_REG(osh, &ai->iostatuswidth)));

	if (config & AICFG_ERRL) {
		SI_PRINT(("errlogctrl 0x%x, errlogdone 0x%x, errlogstatus 0x%x, intstatus 0x%x\n",
			R_REG(osh, &ai->errlogctrl), R_REG(osh, &ai->errlogdone),
			R_REG(osh, &ai->errlogstatus), R_REG(osh, &ai->intstatus)));
		SI_PRINT(("errlogid 0x%x, errloguser 0x%x, errlogflags 0x%x, errlogaddr "
			"0x%x/0x%x\n",
			R_REG(osh, &ai->errlogid), R_REG(osh, &ai->errloguser),
			R_REG(osh, &ai->errlogflags), R_REG(osh, &ai->errlogaddrhi),
			R_REG(osh, &ai->errlogaddrlo)));
	}

	if (verbose && (config & AICFG_OOB)) {
		SI_PRINT(("oobselina30 0x%x, oobselina74 0x%x\n",
			R_REG(osh, &ai->oobselina30), R_REG(osh, &ai->oobselina74)));
		SI_PRINT(("oobselinb30 0x%x, oobselinb74 0x%x\n",
			R_REG(osh, &ai->oobselinb30), R_REG(osh, &ai->oobselinb74)));
		SI_PRINT(("oobselinc30 0x%x, oobselinc74 0x%x\n",
			R_REG(osh, &ai->oobselinc30), R_REG(osh, &ai->oobselinc74)));
		SI_PRINT(("oobselind30 0x%x, oobselind74 0x%x\n",
			R_REG(osh, &ai->oobselind30), R_REG(osh, &ai->oobselind74)));
		SI_PRINT(("oobselouta30 0x%x, oobselouta74 0x%x\n",
			R_REG(osh, &ai->oobselouta30), R_REG(osh, &ai->oobselouta74)));
		SI_PRINT(("oobseloutb30 0x%x, oobseloutb74 0x%x\n",
			R_REG(osh, &ai->oobseloutb30), R_REG(osh, &ai->oobseloutb74)));
		SI_PRINT(("oobseloutc30 0x%x, oobseloutc74 0x%x\n",
			R_REG(osh, &ai->oobseloutc30), R_REG(osh, &ai->oobseloutc74)));
		SI_PRINT(("oobseloutd30 0x%x, oobseloutd74 0x%x\n",
			R_REG(osh, &ai->oobseloutd30), R_REG(osh, &ai->oobseloutd74)));
		SI_PRINT(("oobsynca 0x%x, oobseloutaen 0x%x\n",
			R_REG(osh, &ai->oobsynca), R_REG(osh, &ai->oobseloutaen)));
		SI_PRINT(("oobsyncb 0x%x, oobseloutben 0x%x\n",
			R_REG(osh, &ai->oobsyncb), R_REG(osh, &ai->oobseloutben)));
		SI_PRINT(("oobsyncc 0x%x, oobseloutcen 0x%x\n",
			R_REG(osh, &ai->oobsyncc), R_REG(osh, &ai->oobseloutcen)));
		SI_PRINT(("oobsyncd 0x%x, oobseloutden 0x%x\n",
			R_REG(osh, &ai->oobsyncd), R_REG(osh, &ai->oobseloutden)));
		SI_PRINT(("oobaextwidth 0x%x, oobainwidth 0x%x, oobaoutwidth 0x%x\n",
			R_REG(osh, &ai->oobaextwidth), R_REG(osh, &ai->oobainwidth),
			R_REG(osh, &ai->oobaoutwidth)));
		SI_PRINT(("oobbextwidth 0x%x, oobbinwidth 0x%x, oobboutwidth 0x%x\n",
			R_REG(osh, &ai->oobbextwidth), R_REG(osh, &ai->oobbinwidth),
			R_REG(osh, &ai->oobboutwidth)));
		SI_PRINT(("oobcextwidth 0x%x, oobcinwidth 0x%x, oobcoutwidth 0x%x\n",
			R_REG(osh, &ai->oobcextwidth), R_REG(osh, &ai->oobcinwidth),
			R_REG(osh, &ai->oobcoutwidth)));
		SI_PRINT(("oobdextwidth 0x%x, oobdinwidth 0x%x, oobdoutwidth 0x%x\n",
			R_REG(osh, &ai->oobdextwidth), R_REG(osh, &ai->oobdinwidth),
			R_REG(osh, &ai->oobdoutwidth)));
	}
}

void
ai_view(const si_t *sih, bool verbose)
{
	const si_info_t *sii = SI_INFO(sih);
	const si_cores_info_t *cores_info = (const si_cores_info_t *)sii->cores_info;
	osl_t *osh;
	aidmp_t *ai;
	uint32 cid, addr;

	ai = sii->curwrap;
	osh = sii->osh;

	if (PMU_DMP(sii)) {
		SI_ERROR(("Cannot access pmu DMP\n"));
		return;
	}
	cid = cores_info->coreid[sii->curidx];
	addr = cores_info->wrapba[sii->curidx];
	_ai_view(osh, ai, cid, addr, verbose);
}

void
ai_viewall(si_t *sih, bool verbose)
{
	const si_info_t *sii = SI_INFO(sih);
	const si_cores_info_t *cores_info = (const si_cores_info_t *)sii->cores_info;
	osl_t *osh;
	aidmp_t *ai;
	uint32 cid, addr;
	uint i;

	osh = sii->osh;
	for (i = 0; i < sii->numcores; i++) {
		si_setcoreidx(sih, i);

		if (PMU_DMP(sii)) {
			SI_ERROR(("Skipping pmu DMP\n"));
			continue;
		}
		ai = sii->curwrap;
		cid = cores_info->coreid[sii->curidx];
		addr = cores_info->wrapba[sii->curidx];
		_ai_view(osh, ai, cid, addr, verbose);
	}
}
#endif	/* BCMDBG */

void
BCMPOSTTRAPFN(ai_update_backplane_timeouts)(const si_t *sih, bool enable, uint32 timeout_exp,
	uint32 cid)
{
#if defined(AXI_TIMEOUTS)
	const si_info_t *sii = SI_INFO(sih);
	volatile aidmp_t *ai;
	uint32 i;
	axi_wrapper_t *axi_wrapper = sii->axi_wrapper;
	uint32 errlogctrl = (enable << AIELC_TO_ENAB_SHIFT) |
		((timeout_exp << AIELC_TO_EXP_SHIFT) & AIELC_TO_EXP_MASK);
	uint32 pcie_wrapper_addr = 0;

#ifdef FIQ_ON_AXI_ERR
	if (enable) {
		errlogctrl |= (AIELC_TO_INT_MASK | AIELC_BUSERR_INT_MASK);
	}
#endif /* FIQ_ON_AXI_ERR */

	if (sii->axi_num_wrappers == 0) {
		SI_VMSG((" iai_update_backplane_timeouts, axi_num_wrappers:%d, Is_PCIE:%d,"
			" BUS_TYPE:%d, ID:%x\n",
			sii->axi_num_wrappers, PCIE(sii),
			BUSTYPE(sii->pub.bustype), sii->pub.buscoretype));
		return;
	}

	for (i = 0; i < sii->axi_num_wrappers; ++i) {
		if (axi_wrapper[i].cid == PCIE2_CORE_ID) {
			pcie_wrapper_addr = axi_wrapper[i].wrapper_addr;
			break;
		}
	}

	/* PCIE wrapper address should be valid */
	ASSERT(pcie_wrapper_addr != 0);

	for (i = 0; i < sii->axi_num_wrappers; ++i) {
		/* WAR for wrong EROM entries w.r.t slave and master wrapper
		 * for ADB bridge core...so checking actual wrapper config to determine type
		 * http://jira.broadcom.com/browse/HW4388-905
		*/
		if ((cid == 0 || cid == ADB_BRIDGE_ID) &&
				(axi_wrapper[i].cid == ADB_BRIDGE_ID)) {
			/* WAR is applicable only to 89B0 and 89C0 */
			if (CCREV(sih->ccrev) == 70) {
				ai = (aidmp_t *)(uintptr)axi_wrapper[i].wrapper_addr;
				if (R_REG(sii->osh, &ai->config) & WRAPPER_TIMEOUT_CONFIG) {
					axi_wrapper[i].wrapper_type  = AI_SLAVE_WRAPPER;
				} else {
					axi_wrapper[i].wrapper_type  = AI_MASTER_WRAPPER;
				}
			}
		}
		if (axi_wrapper[i].wrapper_type != AI_SLAVE_WRAPPER ||
				(axi_wrapper[i].cid == ADB_BRIDGE_ID &&
				((axi_wrapper[i].wrapper_addr & 0xFFFF0000) !=
				(pcie_wrapper_addr & 0xFFFF0000)))) {
			SI_VMSG(("SKIP ENABLE BPT: MFG:%x, CID:%x, ADDR:%x\n",
				axi_wrapper[i].mfg,
				axi_wrapper[i].cid,
				axi_wrapper[i].wrapper_addr));
			continue;
		}

		/* Update only given core if requested */
		if ((cid != 0) && (axi_wrapper[i].cid != cid)) {
			continue;
		}

		ai = (volatile aidmp_t *)axi_wrapper[i].wrapper_addr;
		W_REG(sii->osh, &ai->errlogctrl, errlogctrl);

		SI_VMSG(("ENABLED BPT: MFG:%x, CID:%x, ADDR:%x, ERR_CTRL:%x\n",
			axi_wrapper[i].mfg,
			axi_wrapper[i].cid,
			axi_wrapper[i].wrapper_addr,
			R_REG(sii->osh, &ai->errlogctrl)));
	}

#endif /* AXI_TIMEOUTS */
}

/*
 * This API polls all slave wrappers for errors and returns bit map of
 * all reported errors.
 * return - bit map of
 *	AXI_WRAP_STS_NONE
 *	AXI_WRAP_STS_TIMEOUT
 *	AXI_WRAP_STS_SLAVE_ERR
 *	AXI_WRAP_STS_DECODE_ERR
 *	AXI_WRAP_STS_PCI_RD_ERR
 *	AXI_WRAP_STS_WRAP_RD_ERR
 *	AXI_WRAP_STS_SET_CORE_FAIL
 * On timeout detection, correspondign bridge will be reset to
 * unblock the bus.
 */
uint32
BCMPOSTTRAPFN(ai_clear_backplane_to)(si_t *sih)
{
	uint32 ret = 0;
#if defined(AXI_TIMEOUTS)
	const si_info_t *sii = SI_INFO(sih);
	volatile aidmp_t *ai;
	uint32 i;
	axi_wrapper_t *axi_wrapper = sii->axi_wrapper;

	if (sii->axi_num_wrappers == 0) {
		SI_VMSG(("ai_clear_backplane_to, axi_num_wrappers:%d, Is_PCIE:%d, BUS_TYPE:%d,"
			" ID:%x\n",
			sii->axi_num_wrappers, PCIE(sii),
			BUSTYPE(sii->pub.bustype), sii->pub.buscoretype));
		return AXI_WRAP_STS_NONE;
	}

	for (i = 0; i < sii->axi_num_wrappers; ++i) {
		uint32 tmp;

		if (axi_wrapper[i].wrapper_type != AI_SLAVE_WRAPPER) {
			continue;
		}

		ai = (volatile aidmp_t *)axi_wrapper[i].wrapper_addr;
		tmp = ai_clear_backplane_to_per_core(sih, axi_wrapper[i].cid, 0, ai);

		ret |= tmp;
	}

#endif /* AXI_TIMEOUTS */

	return ret;
}

uint
ai_num_slaveports(const si_t *sih, uint coreidx)
{
	const si_info_t *sii = SI_INFO(sih);
	const si_cores_info_t *cores_info = (const si_cores_info_t *)sii->cores_info;
	uint32 cib;

	cib = cores_info->cib[coreidx];
	return ((cib & CIB_NSP_MASK) >> CIB_NSP_SHIFT);
}

#ifdef UART_TRAP_DBG
void
ai_dump_APB_Bridge_registers(const si_t *sih)
{
	aidmp_t *ai;
	const si_info_t *sii = SI_INFO(sih);

	ai = (aidmp_t *)sii->br_wrapba[0];
	printf("APB Bridge 0\n");
	printf("lo 0x%08x, hi 0x%08x, id 0x%08x, flags 0x%08x",
		R_REG(sii->osh, &ai->errlogaddrlo),
		R_REG(sii->osh, &ai->errlogaddrhi),
		R_REG(sii->osh, &ai->errlogid),
		R_REG(sii->osh, &ai->errlogflags));
	printf("\n status 0x%08x\n", R_REG(sii->osh, &ai->errlogstatus));
}
#endif /* UART_TRAP_DBG */

void
ai_force_clocks(const si_t *sih, uint clock_state)
{
	const si_info_t *sii = SI_INFO(sih);
	aidmp_t *ai, *ai_sec = NULL;
	volatile uint32 dummy;
	uint32 ioctrl;
	const si_cores_info_t *cores_info = (const si_cores_info_t *)sii->cores_info;

	ASSERT(GOODREGS(sii->curwrap));
	ai = sii->curwrap;
	if (cores_info->wrapba2[sii->curidx])
		ai_sec = REG_MAP(cores_info->wrapba2[sii->curidx], SI_CORE_SIZE);

	/* ensure there are no pending backplane operations */
	SPINWAIT((R_REG(sii->osh, &ai->resetstatus) != 0), 300);

	if (clock_state == FORCE_CLK_ON) {
		ioctrl = R_REG(sii->osh, &ai->ioctrl);
		W_REG(sii->osh, &ai->ioctrl, (ioctrl | SICF_FGC));
		dummy = R_REG(sii->osh, &ai->ioctrl);
		BCM_REFERENCE(dummy);
		if (ai_sec) {
			ioctrl = R_REG(sii->osh, &ai_sec->ioctrl);
			W_REG(sii->osh, &ai_sec->ioctrl, (ioctrl | SICF_FGC));
			dummy = R_REG(sii->osh, &ai_sec->ioctrl);
			BCM_REFERENCE(dummy);
		}
	} else {
		ioctrl = R_REG(sii->osh, &ai->ioctrl);
		W_REG(sii->osh, &ai->ioctrl, (ioctrl & (~SICF_FGC)));
		dummy = R_REG(sii->osh, &ai->ioctrl);
		BCM_REFERENCE(dummy);
		if (ai_sec) {
			ioctrl = R_REG(sii->osh, &ai_sec->ioctrl);
			W_REG(sii->osh, &ai_sec->ioctrl, (ioctrl & (~SICF_FGC)));
			dummy = R_REG(sii->osh, &ai_sec->ioctrl);
			BCM_REFERENCE(dummy);
		}
	}
	/* ensure there are no pending backplane operations */
	SPINWAIT((R_REG(sii->osh, &ai->resetstatus) != 0), 300);
}
