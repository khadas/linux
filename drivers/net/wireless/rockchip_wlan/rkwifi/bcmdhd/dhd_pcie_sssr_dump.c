/*
* DHD Silicon Save Simulation Restore (SSSR)
* dump module for PCIE
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

#ifdef DHD_SSSR_DUMP
/* include files */
#include <typedefs.h>
#include <bcmutils.h>
#include <bcmdevs.h>
#include <bcmdevs_legacy.h>    /* need to still support chips no longer in trunk firmware */
#include <siutils.h>
#include <sbgci.h>
#include <hndoobr.h>
#include <hndsoc.h>
#include <hndpmu_dhd.h>
#include <etd.h>
#include <hnd_debug.h>
#include <sbchipc.h>
#include <sbhndarm.h>
#include <sbsysmem.h>
#include <sbsreng.h>
#include <pcie_core.h>
#include <dhd.h>
#include <dhd_bus.h>
#include <dhd_flowring.h>
#include <dhd_proto.h>
#include <dhd_dbg.h>
#include <dhd_debug.h>
#if defined(__linux__)
#include <dhd_plat.h>
#endif /* __linux__ */
#include <dhd_pcie.h>
#include <pcicfg.h>
#include <bcmpcie.h>
#include <bcmutils.h>
#include <bcmendian.h>
#include <bcmstdlib_s.h>
#if defined(__linux__)
#include <dhd_linux.h>
#endif /* __linux__ */

#include <dhd_pcie_sssr_dump.h>

/* This can be overwritten by module parameter defined in dhd_linux.c */
#ifdef GDB_PROXY
/* GDB Proxy can't connect to crashed firmware after SSSR dump is generated.
 * SSSR dump generation disabled for GDB Proxy enabled firmware by default.
 * Still it can be explicitly enabled by echo 1 > /sys/wifi/sssr_enab or by
 * sssr_enab=1 in insmod command line
 */
uint sssr_enab = FALSE;
#else /* GDB_PROXY */
uint sssr_enab = TRUE;
#endif /* else GDB_PROXY */

/* If defined collect FIS dump for all cases */
#ifdef DHD_FIS_DUMP
uint fis_enab = TRUE;
#else
uint fis_enab = FALSE;
#endif /* DHD_FIS_DUMP */

#ifdef DHD_COREDUMP
extern dhd_coredump_t dhd_coredump_types[];
#endif /* DHD_COREDUMP */

static int
dhdpcie_get_sssr_fifo_dump(dhd_pub_t *dhd, uint *buf, uint fifo_size,
	uint addr_reg, uint data_reg)
{
	uint addr;
	uint val = 0;
	int i;

	DHD_PRINT(("%s addr = 0x%x, data_reg = 0x%x\n", __FUNCTION__, addr_reg, data_reg));

	if (!buf) {
		DHD_ERROR(("%s: buf is NULL\n", __FUNCTION__));
		return BCME_ERROR;
	}

	if (!fifo_size) {
		DHD_ERROR(("%s: fifo_size is 0\n", __FUNCTION__));
		return BCME_ERROR;
	}

	/* Set the base address offset to 0 */
	addr = addr_reg;
	val = 0;
	dhd_sbreg_op(dhd, addr, &val, FALSE);

	addr = data_reg;
	/* Read 4 bytes at once and loop for fifo_size / 4 */
	for (i = 0; i < fifo_size / 4; i++) {
		if (serialized_backplane_access(dhd->bus, addr,
				sizeof(uint), &val, TRUE) != BCME_OK) {
			DHD_ERROR(("%s: error in serialized_backplane_access\n", __FUNCTION__));
			return BCME_ERROR;
		}
		buf[i] = val;
		OSL_DELAY(1);
	}
	return BCME_OK;
}

static int
dhdpcie_get_sssr_dig_dump(dhd_pub_t *dhd, uint *buf, uint fifo_size,
	uint addr_reg)
{
	uint addr;
	uint val = 0;
	int i;
	si_t *sih = dhd->bus->sih;
	bool vasip_enab, dig_mem_check;
	uint32 ioctrl_addr = 0;

	DHD_PRINT(("%s addr_reg=0x%x size=0x%x\n", __FUNCTION__, addr_reg, fifo_size));

	if (!buf) {
		DHD_ERROR(("%s: buf is NULL\n", __FUNCTION__));
		return BCME_ERROR;
	}

	if (!fifo_size) {
		DHD_ERROR(("%s: fifo_size is 0\n", __FUNCTION__));
		return BCME_ERROR;
	}

	vasip_enab = FALSE;
	dig_mem_check = FALSE;
	/* SSSR register information structure v0 and v1 shares most except dig_mem */
	switch (dhd->sssr_reg_info->rev2.version) {
	case SSSR_REG_INFO_VER_6:
	case SSSR_REG_INFO_VER_5:
		if ((dhd->sssr_reg_info->rev5.length > OFFSETOF(sssr_reg_info_v5_t,
		dig_mem_info)) && dhd->sssr_reg_info->rev5.dig_mem_info.dig_sssr_size) {
			dig_mem_check = TRUE;
		}
		break;
	case SSSR_REG_INFO_VER_4:
		if ((dhd->sssr_reg_info->rev4.length > OFFSETOF(sssr_reg_info_v4_t,
		dig_mem_info)) && dhd->sssr_reg_info->rev4.dig_mem_info.dig_sssr_size) {
			dig_mem_check = TRUE;
		}
		break;
	case SSSR_REG_INFO_VER_3:
		/* intentional fall through */
	case SSSR_REG_INFO_VER_2:
		if ((dhd->sssr_reg_info->rev2.length > OFFSETOF(sssr_reg_info_v2_t,
		dig_mem_info)) && dhd->sssr_reg_info->rev2.dig_mem_info.dig_sr_size) {
			dig_mem_check = TRUE;
		}
		break;
	case SSSR_REG_INFO_VER_1:
		if (dhd->sssr_reg_info->rev1.vasip_regs.vasip_sr_size) {
			vasip_enab = TRUE;
		} else if ((dhd->sssr_reg_info->rev1.length > OFFSETOF(sssr_reg_info_v1_t,
			dig_mem_info)) && dhd->sssr_reg_info->rev1.
			dig_mem_info.dig_sr_size) {
			dig_mem_check = TRUE;
		}
		ioctrl_addr = dhd->sssr_reg_info->rev1.vasip_regs.wrapper_regs.ioctrl;
		break;
	case SSSR_REG_INFO_VER_0:
		if (dhd->sssr_reg_info->rev0.vasip_regs.vasip_sr_size) {
			vasip_enab = TRUE;
		}
		ioctrl_addr = dhd->sssr_reg_info->rev0.vasip_regs.wrapper_regs.ioctrl;
		break;
	default:
		DHD_ERROR(("invalid sssr_reg_ver"));
		return BCME_UNSUPPORTED;
	}
	if (addr_reg) {
		DHD_PRINT(("dig_mem_check=%d vasip_enab=%d\n", dig_mem_check, vasip_enab));
		if (!vasip_enab && dig_mem_check) {
			int err = dhdpcie_bus_membytes(dhd->bus, FALSE, DHD_PCIE_MEM_BAR1, addr_reg,
					(uint8 *)buf, fifo_size);
			if (err != BCME_OK) {
				DHD_ERROR(("%s: Error reading dig dump from dongle !\n",
					__FUNCTION__));
			}
		} else {
			/* Check if vasip clk is disabled, if yes enable it */
			addr = ioctrl_addr;
			dhd_sbreg_op(dhd, addr, &val, TRUE);
			if (!val) {
				val = 1;
				dhd_sbreg_op(dhd, addr, &val, FALSE);
			}

			addr = addr_reg;
			/* Read 4 bytes at once and loop for fifo_size / 4 */
			for (i = 0; i < fifo_size / 4; i++, addr += 4) {
				if (serialized_backplane_access(dhd->bus, addr, sizeof(uint),
					&val, TRUE) != BCME_OK) {
					DHD_ERROR(("%s: Invalid uint addr: 0x%x \n", __FUNCTION__,
						addr));
					return BCME_ERROR;
				}
				buf[i] = val;
				OSL_DELAY(1);
			}
		}
	} else {
		uint cur_coreid;
		uint chipc_corerev;
		chipcregs_t *chipcregs;

		/* Save the current core */
		cur_coreid = si_coreid(sih);

		/* Switch to ChipC */
		chipcregs = (chipcregs_t *)si_setcore(sih, CC_CORE_ID, 0);
		if (!chipcregs) {
			DHD_ERROR(("%s: si_setcore returns NULL for core id %u \n",
				__FUNCTION__, CC_CORE_ID));
			return BCME_ERROR;
		}

		chipc_corerev = si_corerev(sih);

		if ((chipc_corerev == 64) || (chipc_corerev == 65)) {
			W_REG(si_osh(sih), CC_REG_ADDR(chipcregs, SRMemRWAddr), 0);

			/* Read 4 bytes at once and loop for fifo_size / 4 */
			for (i = 0; i < fifo_size / 4; i++) {
				buf[i] = R_REG(si_osh(sih), CC_REG_ADDR(chipcregs, SRMemRWData));
				OSL_DELAY(1);
			}
		}

		/* Switch back to the original core */
		si_setcore(sih, cur_coreid, 0);
	}

	return BCME_OK;
}

static int
dhd_sssr_chk_version_support(int cur_ver, int *supported_vers)
{
	int i = 0;
	if (cur_ver < (int)SSSR_REG_INFO_VER_0 || cur_ver > SSSR_REG_INFO_VER_MAX) {
		return BCME_ERROR;
	}
	for (i = 0; i < SSSR_REG_INFO_VER_MAX && supported_vers[i] != -1; ++i) {
		if (cur_ver == supported_vers[i]) {
			return BCME_OK;
		}
	}
	return BCME_UNSUPPORTED;
}

static int
dhdpcie_get_sssr_subtype_dump(dhd_pub_t *dhd, uint *buf, uint fifo_size,
	uint addr_reg, sssr_subtype_t subtype, int *supported_vers)
{
	bool check = FALSE;
	int ret = 0;

	DHD_PRINT(("%s: subtype=%u addr_reg=0x%x size=0x%x\n", __FUNCTION__,
		subtype, addr_reg, fifo_size));

	if (!buf) {
		DHD_ERROR(("%s: buf is NULL\n", __FUNCTION__));
		return BCME_ERROR;
	}

	if (!fifo_size) {
		DHD_ERROR(("%s: fifo_size is 0\n", __FUNCTION__));
		return BCME_ERROR;
	}

	ret = dhd_sssr_chk_version_support(dhd->sssr_reg_info->rev2.version, supported_vers);
	if (ret == BCME_ERROR) {
		DHD_ERROR(("%s:invalid sssr_reg_ver (%d) !\n", __FUNCTION__,
			dhd->sssr_reg_info->rev2.version));
		return BCME_UNSUPPORTED;
	} else if (ret == BCME_OK) {
		switch (subtype) {
		case SSSR_SAQM_DUMP:
			if ((dhd->sssr_reg_info->rev5.length > OFFSETOF(sssr_reg_info_v5_t,
				saqm_sssr_info)) && dhd->sssr_reg_info->rev5.saqm_sssr_info.
				saqm_sssr_size) {
				check = TRUE;
			}
			break;
		case SSSR_SRCB_DUMP:
			if ((dhd->sssr_reg_info->rev5.length > OFFSETOF(sssr_reg_info_v5_t,
				srcb_mem_info)) && dhd->sssr_reg_info->rev5.srcb_mem_info.
				srcb_sssr_size) {
				check = TRUE;
			}
			break;
		case SSSR_CMN_DUMP:
			if ((dhd->sssr_reg_info->rev5.length > OFFSETOF(sssr_reg_info_v5_t,
				fis_mem_info)) && dhd->sssr_reg_info->rev5.fis_mem_info.
				fis_size) {
				check = TRUE;
			}
			break;
		default:
			DHD_ERROR(("%s: invalid subtype %u!\n", __FUNCTION__, subtype));
			return BCME_UNSUPPORTED;
		}
	}

	if (addr_reg && check) {
		int err = dhdpcie_bus_membytes(dhd->bus, FALSE, DHD_PCIE_MEM_BAR1, addr_reg,
				(uint8 *)buf, fifo_size);
		if (err != BCME_OK) {
			DHD_ERROR(("%s: Error reading dump subtype %u from dongle !\n",
				__FUNCTION__, subtype));
			return BCME_ERROR;
		}
	} else {
		DHD_PRINT(("%s: check fails for subtype %u !\n", __FUNCTION__, subtype));
		return BCME_ERROR;
	}

	return BCME_OK;
}

static uint32
dhdpcie_resume_chipcommon_powerctrl(dhd_pub_t *dhd, uint32 reg_val)
{
	uint addr;
	uint val = 0;
	uint powerctrl_mask;

	DHD_PRINT(("%s\n", __FUNCTION__));

	/* SSSR register information structure v0 and v1 shares most except dig_mem */
	switch (dhd->sssr_reg_info->rev2.version) {
	case SSSR_REG_INFO_VER_6:
	case SSSR_REG_INFO_VER_5:
		/* Handled using MaxRsrcMask for rev5 and above */
		goto exit;
	case SSSR_REG_INFO_VER_4:
		addr = dhd->sssr_reg_info->rev4.chipcommon_regs.base_regs.powerctrl;
		powerctrl_mask = dhd->sssr_reg_info->rev4.
			chipcommon_regs.base_regs.powerctrl_mask;
		break;
	case SSSR_REG_INFO_VER_3:
		/* intentional fall through */
	case SSSR_REG_INFO_VER_2:
		addr = dhd->sssr_reg_info->rev2.chipcommon_regs.base_regs.powerctrl;
		powerctrl_mask = dhd->sssr_reg_info->rev2.
			chipcommon_regs.base_regs.powerctrl_mask;
		break;
	case SSSR_REG_INFO_VER_1:
	case SSSR_REG_INFO_VER_0:
		addr = dhd->sssr_reg_info->rev1.chipcommon_regs.base_regs.powerctrl;
		powerctrl_mask = dhd->sssr_reg_info->rev1.
			chipcommon_regs.base_regs.powerctrl_mask;
		break;
	default:
		DHD_ERROR(("invalid sssr_reg_ver"));
		return BCME_UNSUPPORTED;
	}

	/* conditionally clear bits [11:8] of PowerCtrl */
	dhd_sbreg_op(dhd, addr, &val, TRUE);

	if (!(val & powerctrl_mask)) {
		dhd_sbreg_op(dhd, addr, &reg_val, FALSE);
	}
exit:
	return BCME_OK;
}

static uint32
dhdpcie_suspend_chipcommon_powerctrl(dhd_pub_t *dhd)
{
	uint addr;
	uint val = 0, reg_val = 0;
	uint powerctrl_mask;

	DHD_PRINT(("%s\n", __FUNCTION__));

	/* SSSR register information structure v0 and v1 shares most except dig_mem */
	switch (dhd->sssr_reg_info->rev2.version) {
	case SSSR_REG_INFO_VER_4:
		addr = dhd->sssr_reg_info->rev4.chipcommon_regs.base_regs.powerctrl;
		powerctrl_mask = dhd->sssr_reg_info->rev4.
			chipcommon_regs.base_regs.powerctrl_mask;
		break;
	case SSSR_REG_INFO_VER_3:
		/* intentional fall through */
	case SSSR_REG_INFO_VER_2:
		addr = dhd->sssr_reg_info->rev2.chipcommon_regs.base_regs.powerctrl;
		powerctrl_mask = dhd->sssr_reg_info->rev2.
			chipcommon_regs.base_regs.powerctrl_mask;
		break;
	case SSSR_REG_INFO_VER_1:
	case SSSR_REG_INFO_VER_0:
		addr = dhd->sssr_reg_info->rev1.chipcommon_regs.base_regs.powerctrl;
		powerctrl_mask = dhd->sssr_reg_info->rev1.
			chipcommon_regs.base_regs.powerctrl_mask;
		break;
	default:
		DHD_ERROR(("invalid sssr_reg_ver"));
		return BCME_UNSUPPORTED;
	}

	/* conditionally clear bits [11:8] of PowerCtrl */
	dhd_sbreg_op(dhd, addr, &reg_val, TRUE);
	if (reg_val & powerctrl_mask) {
		val = 0;
		dhd_sbreg_op(dhd, addr, &val, FALSE);
	}
	return reg_val;
}

static int
dhdpcie_clear_intmask_and_timer(dhd_pub_t *dhd)
{
	uint addr;
	uint val;
	uint32 cc_intmask, pmuintmask0, pmuintmask1, resreqtimer, macresreqtimer,
	 macresreqtimer1, vasip_sr_size = 0;

	DHD_PRINT(("%s\n", __FUNCTION__));

	/* SSSR register information structure v0 and v1 shares most except dig_mem */
	switch (dhd->sssr_reg_info->rev2.version) {
	case SSSR_REG_INFO_VER_4:
		cc_intmask = dhd->sssr_reg_info->rev4.chipcommon_regs.base_regs.intmask;
		pmuintmask0 = dhd->sssr_reg_info->rev4.pmu_regs.base_regs.pmuintmask0;
		pmuintmask1 = dhd->sssr_reg_info->rev4.pmu_regs.base_regs.pmuintmask1;
		resreqtimer = dhd->sssr_reg_info->rev4.pmu_regs.base_regs.resreqtimer;
		macresreqtimer = dhd->sssr_reg_info->rev4.pmu_regs.base_regs.macresreqtimer;
		macresreqtimer1 = dhd->sssr_reg_info->rev4.pmu_regs.
			base_regs.macresreqtimer1;
		break;
	case SSSR_REG_INFO_VER_3:
		/* intentional fall through */
	case SSSR_REG_INFO_VER_2:
		cc_intmask = dhd->sssr_reg_info->rev2.chipcommon_regs.base_regs.intmask;
		pmuintmask0 = dhd->sssr_reg_info->rev2.pmu_regs.base_regs.pmuintmask0;
		pmuintmask1 = dhd->sssr_reg_info->rev2.pmu_regs.base_regs.pmuintmask1;
		resreqtimer = dhd->sssr_reg_info->rev2.pmu_regs.base_regs.resreqtimer;
		macresreqtimer = dhd->sssr_reg_info->rev2.pmu_regs.base_regs.macresreqtimer;
		macresreqtimer1 = dhd->sssr_reg_info->rev2.
			pmu_regs.base_regs.macresreqtimer1;
		break;
	case SSSR_REG_INFO_VER_1:
	case SSSR_REG_INFO_VER_0:
		cc_intmask = dhd->sssr_reg_info->rev1.chipcommon_regs.base_regs.intmask;
		pmuintmask0 = dhd->sssr_reg_info->rev1.pmu_regs.base_regs.pmuintmask0;
		pmuintmask1 = dhd->sssr_reg_info->rev1.pmu_regs.base_regs.pmuintmask1;
		resreqtimer = dhd->sssr_reg_info->rev1.pmu_regs.base_regs.resreqtimer;
		macresreqtimer = dhd->sssr_reg_info->rev1.pmu_regs.base_regs.macresreqtimer;
		macresreqtimer1 = dhd->sssr_reg_info->rev1.
			pmu_regs.base_regs.macresreqtimer1;
		vasip_sr_size = dhd->sssr_reg_info->rev1.vasip_regs.vasip_sr_size;
		break;
	default:
		DHD_ERROR(("invalid sssr_reg_ver"));
		return BCME_UNSUPPORTED;
	}

	/* clear chipcommon intmask */
	val = 0x0;
	dhd_sbreg_op(dhd, cc_intmask, &val, FALSE);

	/* clear PMUIntMask0 */
	val = 0x0;
	dhd_sbreg_op(dhd, pmuintmask0, &val, FALSE);

	/* clear PMUIntMask1 */
	val = 0x0;
	dhd_sbreg_op(dhd, pmuintmask1, &val, FALSE);

	/* clear res_req_timer */
	val = 0x0;
	dhd_sbreg_op(dhd, resreqtimer, &val, FALSE);

	/* clear macresreqtimer */
	val = 0x0;
	dhd_sbreg_op(dhd, macresreqtimer, &val, FALSE);

	/* clear macresreqtimer1 */
	val = 0x0;
	dhd_sbreg_op(dhd, macresreqtimer1, &val, FALSE);

	/* clear VasipClkEn */
	if (vasip_sr_size) {
		addr = dhd->sssr_reg_info->rev1.vasip_regs.wrapper_regs.ioctrl;
		val = 0x0;
		dhd_sbreg_op(dhd, addr, &val, FALSE);
	}

	return BCME_OK;
}

static void
dhdpcie_update_d11_status_from_trapdata(dhd_pub_t *dhd)
{
#define TRAP_DATA_MAIN_CORE_BIT_MASK	(1 << 1)
#define TRAP_DATA_AUX_CORE_BIT_MASK	(1 << 4)
	uint trap_data_mask[MAX_NUM_D11CORES] =	{
		TRAP_DATA_MAIN_CORE_BIT_MASK,
		TRAP_DATA_AUX_CORE_BIT_MASK
	};
	int i;
	/* Apply only for 4375 chip */
	if (dhd_bus_chip_id(dhd) == BCM4375_CHIP_ID) {
		for (i = 0; i < MAX_NUM_D11CORES; i++) {
			if (dhd->sssr_d11_outofreset[i] &&
				(dhd->dongle_trap_data & trap_data_mask[i])) {
				dhd->sssr_d11_outofreset[i] = TRUE;
			} else {
				dhd->sssr_d11_outofreset[i] = FALSE;
			}
			DHD_PRINT(("%s: sssr_d11_outofreset[%d] : %d after AND with "
				"trap_data:0x%x-0x%x\n",
				__FUNCTION__, i, dhd->sssr_d11_outofreset[i],
				dhd->dongle_trap_data, trap_data_mask[i]));
		}
	}
}

static int
dhdpcie_d11_check_outofreset(dhd_pub_t *dhd)
{
	int i = 0;
	uint8 num_d11cores = 0;
	int ret = BCME_OK;
	struct dhd_bus *bus = dhd->bus;
	uint save_idx = 0;

	DHD_PRINT(("%s\n", __FUNCTION__));

	num_d11cores = dhd_d11_slices_num_get(dhd);

	save_idx = si_coreidx(bus->sih);
	for (i = 0; i < num_d11cores; i++) {
		if (si_setcore(bus->sih, D11_CORE_ID, i)) {
			dhd->sssr_d11_outofreset[i] = si_iscoreup(bus->sih);
		} else {
			DHD_ERROR(("%s: setcore d11 fails !\n", __FUNCTION__));
			ret = BCME_ERROR;
			goto exit;
		}
	}
	si_setcoreidx(bus->sih, save_idx);

	dhdpcie_update_d11_status_from_trapdata(dhd);
exit:
	return ret;
}

#define SAQM_CLK_REQ_CLR_DELAY 1000u
static int
dhdpcie_saqm_clear_clk_req(dhd_pub_t *dhdp)
{
	uint32 clockcontrolstatus_val = 0, clockcontrolstatus = 0, saqm_extrsrcreq = 0;
	uint32 digsr_srcontrol2_addr = 0, pmuchip_ctl_addr_reg = 0, pmuchip_ctl_data_reg = 0;
	uint32 digsr_srcontrol2_setbit_val = 0, pmuchip_ctl_val = 0, pmuchip_ctl_setbit_val = 0;
	uint32 digsr_srcontrol1_addr = 0, digsr_srcontrol1_clrbit_val = 0;
	uint32 val = 0;
	uint save_idx = si_coreidx(dhdp->bus->sih);

	if ((si_setcore(dhdp->bus->sih, D11_SAQM_CORE_ID, 0) == NULL) ||
			!si_iscoreup(dhdp->bus->sih)) {
		goto exit;
	}

	DHD_PRINT(("%s\n", __FUNCTION__));
	switch (dhdp->sssr_reg_info->rev2.version) {
	case SSSR_REG_INFO_VER_6:
	case SSSR_REG_INFO_VER_5:
		saqm_extrsrcreq = dhdp->sssr_reg_info->rev5.saqm_sssr_info.
			oobr_regs.extrsrcreq;
		if (saqm_extrsrcreq) {
			/* read is for information purpose only.  */
			dhd_sbreg_op(dhdp, saqm_extrsrcreq, &clockcontrolstatus_val, TRUE);
			clockcontrolstatus = dhdp->sssr_reg_info->rev5.saqm_sssr_info.
				base_regs.clockcontrolstatus;
			dhd_sbreg_op(dhdp, clockcontrolstatus,
				&clockcontrolstatus_val, TRUE);
			clockcontrolstatus_val |=
				dhdp->sssr_reg_info->rev5.saqm_sssr_info.
				base_regs.clockcontrolstatus_val;
			dhd_sbreg_op(dhdp, clockcontrolstatus, &clockcontrolstatus_val,
				FALSE);
			OSL_DELAY(SAQM_CLK_REQ_CLR_DELAY);
		}
		/* set DIG force_sr_all bit */
		digsr_srcontrol2_addr =
			dhdp->sssr_reg_info->rev5.saqm_sssr_info.sssr_config_regs.
			digsr_srcontrol2_addr;
		if (digsr_srcontrol2_addr) {
			dhd_sbreg_op(dhdp, digsr_srcontrol2_addr, &val, TRUE);
			digsr_srcontrol2_setbit_val =
				dhdp->sssr_reg_info->rev5.saqm_sssr_info.sssr_config_regs.
				digsr_srcontrol2_setbit_val;
			val |= digsr_srcontrol2_setbit_val;
			dhd_sbreg_op(dhdp, digsr_srcontrol2_addr, &val, FALSE);
		}

		/* Disable SR self test */
		digsr_srcontrol1_addr =
			dhdp->sssr_reg_info->rev5.saqm_sssr_info.sssr_config_regs.
			digsr_srcontrol1_addr;
		digsr_srcontrol1_clrbit_val =
			dhdp->sssr_reg_info->rev5.saqm_sssr_info.sssr_config_regs.
			digsr_srcontrol1_clrbit_val;
		if (digsr_srcontrol1_addr) {
			dhd_sbreg_op(dhdp, digsr_srcontrol1_addr, &val, TRUE);
			val &= ~(digsr_srcontrol1_clrbit_val);
			dhd_sbreg_op(dhdp, digsr_srcontrol1_addr, &val, FALSE);
		}

		/* set PMU chip ctrl saqm_sr_enable bit */
		pmuchip_ctl_addr_reg = dhdp->sssr_reg_info->rev5.saqm_sssr_info.
			sssr_config_regs.pmuchip_ctl_addr_reg;
		pmuchip_ctl_val = dhdp->sssr_reg_info->rev5.saqm_sssr_info.
			sssr_config_regs.pmuchip_ctl_val;
		if (pmuchip_ctl_addr_reg) {
			dhd_sbreg_op(dhdp, pmuchip_ctl_addr_reg, &pmuchip_ctl_val, FALSE);
		}
		pmuchip_ctl_data_reg = dhdp->sssr_reg_info->rev5.saqm_sssr_info.
			sssr_config_regs.pmuchip_ctl_data_reg;
		pmuchip_ctl_setbit_val =
			dhdp->sssr_reg_info->rev5.saqm_sssr_info.sssr_config_regs.
			pmuchip_ctl_setbit_val;
		if (pmuchip_ctl_data_reg) {
			dhd_sbreg_op(dhdp, pmuchip_ctl_data_reg, &val, TRUE);
			val |= pmuchip_ctl_setbit_val;
			dhd_sbreg_op(dhdp, pmuchip_ctl_data_reg, &val, FALSE);
		}
		break;
	case SSSR_REG_INFO_VER_4:
		saqm_extrsrcreq = dhdp->sssr_reg_info->rev4.saqm_sssr_info.
			oobr_regs.extrsrcreq;
		if (saqm_extrsrcreq) {
			/* read is for information purpose only.  */
			dhd_sbreg_op(dhdp, saqm_extrsrcreq, &clockcontrolstatus_val, TRUE);
			clockcontrolstatus = dhdp->sssr_reg_info->rev4.saqm_sssr_info.
				base_regs.clockcontrolstatus;
			dhd_sbreg_op(dhdp, clockcontrolstatus, &clockcontrolstatus_val,
				TRUE);
			clockcontrolstatus_val |=
				dhdp->sssr_reg_info->rev4.saqm_sssr_info.
				base_regs.clockcontrolstatus_val;

			dhd_sbreg_op(dhdp, clockcontrolstatus, &clockcontrolstatus_val,
				FALSE);
			OSL_DELAY(SAQM_CLK_REQ_CLR_DELAY);
		}

		/* set DIG force_sr_all bit */
		digsr_srcontrol2_addr =
			dhdp->sssr_reg_info->rev4.saqm_sssr_info.sssr_config_regs.
			digsr_srcontrol2_addr;
		if (digsr_srcontrol2_addr) {
			dhd_sbreg_op(dhdp, digsr_srcontrol2_addr, &val, TRUE);
			digsr_srcontrol2_setbit_val =
				dhdp->sssr_reg_info->rev4.saqm_sssr_info.sssr_config_regs.
				digsr_srcontrol2_setbit_val;
			val |= digsr_srcontrol2_setbit_val;
			dhd_sbreg_op(dhdp, digsr_srcontrol2_addr, &val, FALSE);
		}

		/* Disable SR self test */
		digsr_srcontrol1_addr =
			dhdp->sssr_reg_info->rev4.saqm_sssr_info.sssr_config_regs.
			digsr_srcontrol1_addr;
		digsr_srcontrol1_clrbit_val =
			dhdp->sssr_reg_info->rev4.saqm_sssr_info.sssr_config_regs.
			digsr_srcontrol1_clrbit_val;
		if (digsr_srcontrol1_addr) {
			dhd_sbreg_op(dhdp, digsr_srcontrol1_addr, &val, TRUE);
			val &= ~(digsr_srcontrol1_clrbit_val);
			dhd_sbreg_op(dhdp, digsr_srcontrol1_addr, &val, FALSE);
		}

		/* set PMU chip ctrl saqm_sr_enable bit */
		pmuchip_ctl_addr_reg = dhdp->sssr_reg_info->rev4.saqm_sssr_info.
			sssr_config_regs.pmuchip_ctl_addr_reg;
		pmuchip_ctl_val = dhdp->sssr_reg_info->rev4.saqm_sssr_info.
			sssr_config_regs.pmuchip_ctl_val;
		if (pmuchip_ctl_addr_reg) {
			dhd_sbreg_op(dhdp, pmuchip_ctl_addr_reg, &pmuchip_ctl_val, FALSE);
		}
		pmuchip_ctl_data_reg = dhdp->sssr_reg_info->rev4.saqm_sssr_info.
			sssr_config_regs.pmuchip_ctl_data_reg;
		pmuchip_ctl_setbit_val =
			dhdp->sssr_reg_info->rev4.saqm_sssr_info.sssr_config_regs.
			pmuchip_ctl_setbit_val;
		if (pmuchip_ctl_data_reg) {
			dhd_sbreg_op(dhdp, pmuchip_ctl_data_reg, &val, TRUE);
			val |= pmuchip_ctl_setbit_val;
			dhd_sbreg_op(dhdp, pmuchip_ctl_data_reg, &val, FALSE);
		}
		break;
	default:
		DHD_ERROR(("invalid sssr_reg_ver"));
		return BCME_UNSUPPORTED;
	}
exit:
	si_setcoreidx(dhdp->bus->sih, save_idx);
	return BCME_OK;
}

static int
dhdpcie_saqm_clear_force_sr_all(dhd_pub_t *dhdp)
{
	uint32 val = 0, digsr_srcontrol2_addr = 0, digsr_srcontrol2_setbit_val = 0;
	uint save_idx = si_coreidx(dhdp->bus->sih);

	if ((si_setcore(dhdp->bus->sih, D11_SAQM_CORE_ID, 0) == NULL) ||
			!si_iscoreup(dhdp->bus->sih)) {
		goto exit;
	}

	DHD_PRINT(("%s\n", __FUNCTION__));
	switch (dhdp->sssr_reg_info->rev2.version) {
	case SSSR_REG_INFO_VER_6:
	case SSSR_REG_INFO_VER_5:
		/* clear DIG force_sr_all bit */
		digsr_srcontrol2_addr =
			dhdp->sssr_reg_info->rev5.saqm_sssr_info.sssr_config_regs.
			digsr_srcontrol2_addr;
		if (digsr_srcontrol2_addr) {
			dhd_sbreg_op(dhdp, digsr_srcontrol2_addr, &val, TRUE);
			digsr_srcontrol2_setbit_val =
				dhdp->sssr_reg_info->rev5.saqm_sssr_info.sssr_config_regs.
				digsr_srcontrol2_setbit_val;
			val &= ~digsr_srcontrol2_setbit_val;
			dhd_sbreg_op(dhdp, digsr_srcontrol2_addr, &val, FALSE);
		}

		break;
	case SSSR_REG_INFO_VER_4:
		/* clear DIG force_sr_all bit */
		digsr_srcontrol2_addr =
			dhdp->sssr_reg_info->rev4.saqm_sssr_info.sssr_config_regs.
			digsr_srcontrol2_addr;
		if (digsr_srcontrol2_addr) {
			dhd_sbreg_op(dhdp, digsr_srcontrol2_addr, &val, TRUE);
			digsr_srcontrol2_setbit_val =
				dhdp->sssr_reg_info->rev4.saqm_sssr_info.sssr_config_regs.
				digsr_srcontrol2_setbit_val;
			val &= ~digsr_srcontrol2_setbit_val;
			dhd_sbreg_op(dhdp, digsr_srcontrol2_addr, &val, FALSE);
		}

		break;
	default:
		DHD_ERROR(("invalid sssr_reg_ver"));
		return BCME_UNSUPPORTED;
	}
exit:
	si_setcoreidx(dhdp->bus->sih, save_idx);
	return BCME_OK;
}

static int
dhdpcie_d11_clear_clk_req(dhd_pub_t *dhd)
{
	int i;
	uint val = 0;
	uint8 num_d11cores;
	uint32 clockrequeststatus, clockcontrolstatus, clockcontrolstatus_val;

	DHD_PRINT(("%s\n", __FUNCTION__));

	num_d11cores = dhd_d11_slices_num_get(dhd);

	for (i = 0; i < num_d11cores; i++) {
		if (dhd->sssr_d11_outofreset[i]) {
			/* clear request clk only if itopoobb/extrsrcreq is non zero */
			/* SSSR register information structure v0 and
			 * v1 shares most except dig_mem
			 */
			switch (dhd->sssr_reg_info->rev2.version) {
			case SSSR_REG_INFO_VER_4:
				clockrequeststatus = dhd->sssr_reg_info->rev4.
					mac_regs[i].oobr_regs.extrsrcreq;
				clockcontrolstatus = dhd->sssr_reg_info->rev4.
					mac_regs[i].base_regs.clockcontrolstatus;
				clockcontrolstatus_val = dhd->sssr_reg_info->rev4.
					mac_regs[i].base_regs.clockcontrolstatus_val;
				break;
			case SSSR_REG_INFO_VER_3:
				/* intentional fall through */
			case SSSR_REG_INFO_VER_2:
				clockrequeststatus = dhd->sssr_reg_info->rev2.
					mac_regs[i].wrapper_regs.extrsrcreq;
				clockcontrolstatus = dhd->sssr_reg_info->rev2.
					mac_regs[i].base_regs.clockcontrolstatus;
				clockcontrolstatus_val = dhd->sssr_reg_info->rev2.
					mac_regs[i].base_regs.clockcontrolstatus_val;
				break;
			case SSSR_REG_INFO_VER_1:
			case SSSR_REG_INFO_VER_0:
				clockrequeststatus = dhd->sssr_reg_info->rev1.
					mac_regs[i].wrapper_regs.itopoobb;
				clockcontrolstatus = dhd->sssr_reg_info->rev1.
					mac_regs[i].base_regs.clockcontrolstatus;
				clockcontrolstatus_val = dhd->sssr_reg_info->rev1.
					mac_regs[i].base_regs.clockcontrolstatus_val;
				break;
			default:
				DHD_ERROR(("invalid sssr_reg_ver"));
				return BCME_UNSUPPORTED;
			}
			/* Read is for information purpose only */
			dhd_sbreg_op(dhd, clockrequeststatus, &val, TRUE);
			/* clear clockcontrolstatus */
			dhd_sbreg_op(dhd, clockcontrolstatus, &clockcontrolstatus_val, FALSE);
		}
	}
	return BCME_OK;
}

static int
dhdpcie_arm_clear_clk_req(dhd_pub_t *dhd)
{
	struct dhd_bus *bus = dhd->bus;
	uint val = 0;
	uint32 resetctrl = 0;
	uint32 clockrequeststatus, clockcontrolstatus, clockcontrolstatus_val;
	uint save_idx = si_coreidx(bus->sih);

	DHD_PRINT(("%s\n", __FUNCTION__));

	/* SSSR register information structure v0 and v1 shares most except dig_mem */
	switch (dhd->sssr_reg_info->rev2.version) {
	case SSSR_REG_INFO_VER_6:
	case SSSR_REG_INFO_VER_5:
		clockrequeststatus = dhd->sssr_reg_info->rev5.
			arm_regs.oobr_regs.extrsrcreq;
		clockcontrolstatus = dhd->sssr_reg_info->rev5.
			arm_regs.base_regs.clockcontrolstatus;
		clockcontrolstatus_val = dhd->sssr_reg_info->rev5.
			arm_regs.base_regs.clockcontrolstatus_val;
		break;
	case SSSR_REG_INFO_VER_4:
		clockrequeststatus = dhd->sssr_reg_info->rev4.
			arm_regs.oobr_regs.extrsrcreq;
		clockcontrolstatus = dhd->sssr_reg_info->rev4.
			arm_regs.base_regs.clockcontrolstatus;
		clockcontrolstatus_val = dhd->sssr_reg_info->rev4.
			arm_regs.base_regs.clockcontrolstatus_val;
		break;
	case SSSR_REG_INFO_VER_3:
		/* intentional fall through */
	case SSSR_REG_INFO_VER_2:
		resetctrl = dhd->sssr_reg_info->rev2.
			arm_regs.wrapper_regs.resetctrl;
		clockrequeststatus = dhd->sssr_reg_info->rev2.
			arm_regs.wrapper_regs.extrsrcreq;
		clockcontrolstatus = dhd->sssr_reg_info->rev2.
			arm_regs.base_regs.clockcontrolstatus;
		clockcontrolstatus_val = dhd->sssr_reg_info->rev2.
			arm_regs.base_regs.clockcontrolstatus_val;
		break;
	case SSSR_REG_INFO_VER_1:
	case SSSR_REG_INFO_VER_0:
		resetctrl = dhd->sssr_reg_info->rev1.
			arm_regs.wrapper_regs.resetctrl;
		clockrequeststatus = dhd->sssr_reg_info->rev1.
			arm_regs.wrapper_regs.itopoobb;
		clockcontrolstatus = dhd->sssr_reg_info->rev1.
			arm_regs.base_regs.clockcontrolstatus;
		clockcontrolstatus_val = dhd->sssr_reg_info->rev1.
			arm_regs.base_regs.clockcontrolstatus_val;
		break;
	default:
		DHD_ERROR(("invalid sssr_reg_ver"));
		return BCME_UNSUPPORTED;
	}

	/* Check if bit 0 of resetctrl is cleared */

	/* for chips having booker interface */
	if (CHIPTYPE(bus->sih->socitype) == SOCI_NCI) {
		if (si_setcore(bus->sih, ARMCA7_CORE_ID, 0)) {
			if (si_iscoreup(bus->sih))
				val = 0;
			else
				val = 1;
		} else {
			DHD_ERROR(("%s: Failed to set armca7 core !\n", __FUNCTION__));
			si_setcoreidx(bus->sih, save_idx);
			return BCME_ERROR;
		}
	} else {
		/* if arm resetctrl address is not provided in sssr info */
		if (resetctrl == 0) {
			si_setcore(bus->sih, ARMCR4_CORE_ID, 0);
			val = si_wrapperreg(bus->sih, AI_RESETCTRL, 0, 0);
		} else {
			dhd_sbreg_op(dhd, resetctrl, &val, TRUE);
		}
		val &= 1u;
	}

	if (!(val & 1)) {
		dhd_sbreg_op(dhd, clockrequeststatus, &val, TRUE);
		/* clear clockcontrolstatus */
		dhd_sbreg_op(dhd, clockcontrolstatus, &clockcontrolstatus_val, FALSE);

		if (MULTIBP_ENAB(bus->sih)) {
			uint cfgval = 0;

			/* Clear coherent bits for CA7 because CPU is halted */
			if (bus->coreid == ARMCA7_CORE_ID) {
				cfgval = dhdpcie_bus_cfg_read_dword(bus,
					PCIE_CFG_SUBSYSTEM_CONTROL, 4);
				dhdpcie_bus_cfg_write_dword(bus, PCIE_CFG_SUBSYSTEM_CONTROL, 4,
					(cfgval & ~PCIE_BARCOHERENTACCEN_MASK));
			}
		}
	}

	si_setcoreidx(bus->sih, save_idx);
	return BCME_OK;
}

static int
dhdpcie_arm_resume_clk_req(dhd_pub_t *dhd)
{
	struct dhd_bus *bus = dhd->bus;
	uint save_idx = si_coreidx(bus->sih);
	int ret = BCME_OK;

	if (!si_setcore(bus->sih, ARMCA7_CORE_ID, 0) &&
		!(si_setcore(bus->sih, ARMCR4_CORE_ID, 0)) &&
		!(si_setcore(bus->sih, ARMCM3_CORE_ID, 0)) &&
		!(si_setcore(bus->sih, ARM7S_CORE_ID, 0))) {
		DHD_ERROR(("%s: Failed to find ARM core!\n", __FUNCTION__));
		ret = BCME_ERROR;
		goto fail;
	}

fail:
	si_setcoreidx(bus->sih, save_idx);
	return ret;
}

static int
dhdpcie_pcie_clear_clk_req(dhd_pub_t *dhd)
{
	uint val = 0;
	uint32 clockrequeststatus, clockcontrolstatus_addr, clockcontrolstatus_val;

	DHD_PRINT(("%s\n", __FUNCTION__));

	/* SSSR register information structure v0 and v1 shares most except dig_mem */
	switch (dhd->sssr_reg_info->rev2.version) {
	case SSSR_REG_INFO_VER_4:
		clockrequeststatus = dhd->sssr_reg_info->rev4.
			pcie_regs.oobr_regs.extrsrcreq;
		clockcontrolstatus_addr = dhd->sssr_reg_info->rev4.
			pcie_regs.base_regs.clockcontrolstatus;
		clockcontrolstatus_val = dhd->sssr_reg_info->rev4.
			pcie_regs.base_regs.clockcontrolstatus_val;
		break;
	case SSSR_REG_INFO_VER_3:
		/* intentional fall through */
	case SSSR_REG_INFO_VER_2:
		clockrequeststatus = dhd->sssr_reg_info->rev2.
			pcie_regs.wrapper_regs.extrsrcreq;
		clockcontrolstatus_addr = dhd->sssr_reg_info->rev2.
			pcie_regs.base_regs.clockcontrolstatus;
		clockcontrolstatus_val = dhd->sssr_reg_info->rev2.
			pcie_regs.base_regs.clockcontrolstatus_val;
		break;
	case SSSR_REG_INFO_VER_1:
	case SSSR_REG_INFO_VER_0:
		clockrequeststatus = dhd->sssr_reg_info->rev1.
			pcie_regs.wrapper_regs.itopoobb;
		clockcontrolstatus_addr = dhd->sssr_reg_info->rev1.
			pcie_regs.base_regs.clockcontrolstatus;
		clockcontrolstatus_val = dhd->sssr_reg_info->rev1.
			pcie_regs.base_regs.clockcontrolstatus_val;
		break;
	default:
		DHD_ERROR(("invalid sssr_reg_ver"));
		return BCME_UNSUPPORTED;
	}

	dhd_sbreg_op(dhd, clockrequeststatus, &val, TRUE);
	/* clear clockcontrolstatus */
	dhd_sbreg_op(dhd, clockcontrolstatus_addr, &clockcontrolstatus_val, FALSE);

	return BCME_OK;
}

static int
dhdpcie_pcie_send_ltrsleep(dhd_pub_t *dhd)
{
	uint addr;
	uint val = 0;

	DHD_PRINT(("%s\n", __FUNCTION__));

	/* SSSR register information structure v0 and v1 shares most except dig_mem */
	switch (dhd->sssr_reg_info->rev2.version) {
	case SSSR_REG_INFO_VER_6:
	case SSSR_REG_INFO_VER_5:
		addr = dhd->sssr_reg_info->rev5.pcie_regs.base_regs.ltrstate;
		break;
	case SSSR_REG_INFO_VER_4:
		addr = dhd->sssr_reg_info->rev4.pcie_regs.base_regs.ltrstate;
		break;
	case SSSR_REG_INFO_VER_3:
		/* intentional fall through */
	case SSSR_REG_INFO_VER_2:
		addr = dhd->sssr_reg_info->rev2.pcie_regs.base_regs.ltrstate;
		break;
	case SSSR_REG_INFO_VER_1:
	case SSSR_REG_INFO_VER_0:
		addr = dhd->sssr_reg_info->rev1.pcie_regs.base_regs.ltrstate;
		break;
	default:
		DHD_ERROR(("invalid sssr_reg_ver"));
		return BCME_UNSUPPORTED;
	}

	val = LTR_ACTIVE;
	dhd_sbreg_op(dhd, addr, &val, FALSE);

	val = LTR_SLEEP;
	dhd_sbreg_op(dhd, addr, &val, FALSE);

	return BCME_OK;
}

static int
dhdpcie_clear_clk_req(dhd_pub_t *dhd)
{
	DHD_PRINT(("%s\n", __FUNCTION__));

	dhdpcie_arm_clear_clk_req(dhd);

	dhdpcie_d11_clear_clk_req(dhd);

	if (dhd->sssr_reg_info->rev2.version >= SSSR_REG_INFO_VER_4) {
		dhdpcie_saqm_clear_clk_req(dhd);
	}

	dhdpcie_pcie_clear_clk_req(dhd);

	return BCME_OK;
}

#define SICF_PCLKE              0x0004          /**< PHY clock enable */
#define SICF_PRST               0x0008          /**< PHY reset */

static int
dhdpcie_bring_d11_outofreset(dhd_pub_t *dhd)
{
	int i = 0;
	uint8 num_d11cores = 0;
	uint save_idx = 0;
	dhd_bus_t *bus = dhd->bus;

	DHD_PRINT(("%s\n", __FUNCTION__));

	num_d11cores = dhd_d11_slices_num_get(dhd);
	save_idx = si_coreidx(bus->sih);

	for (i = 0; i < num_d11cores; i++) {
		if (dhd->sssr_d11_outofreset[i]) {
			if (si_setcore(bus->sih, D11_CORE_ID, i)) {
				si_core_reset(bus->sih, SICF_PRST | SICF_PCLKE,
					SICF_PRST | SICF_PCLKE);
				DHD_PRINT(("dhdpcie_bring_d11_outofreset mac %d si_isup %d\n",
						i, si_iscoreup(bus->sih)));
			} else {
				DHD_ERROR(("%s: setcore d11 fails !\n",
					__FUNCTION__));
				return BCME_ERROR;
			}
		}
	}

	si_setcoreidx(bus->sih, save_idx);

	return BCME_OK;
}

static int
dhdpcie_bring_saqm_updown(dhd_pub_t *dhdp, bool down)
{
	dhd_bus_t *bus = dhdp->bus;
	uint save_idx, save_unit;
	save_idx = si_coreidx(bus->sih);
	save_unit = si_coreunit(bus->sih);

	if (si_setcore(bus->sih, D11_SAQM_CORE_ID, 0)) {
		if (down) {
			si_core_disable(bus->sih, SICF_PRST | SICF_PCLKE);
		} else {
			si_core_reset(bus->sih, SICF_PRST | SICF_PCLKE,
				SICF_PRST | SICF_PCLKE);
		}
		DHD_PRINT(("dhdpcie_bring_saqm_updown si_isup %d down %d\n",
				si_iscoreup(bus->sih), down));
		si_setcore(bus->sih, save_idx, save_unit);
	}
	return BCME_OK;
}

static void
dhdpcie_sssr_common_header(dhd_pub_t *dhd, sssr_header_t *sssr_header)
{
	int ret = 0;
	uint16 sr_asm_version = 0;

	sssr_header->magic = SSSR_HEADER_MAGIC;
	ret = dhd_sssr_sr_asm_version(dhd, &sr_asm_version);
	if (ret == BCME_OK) {
		sssr_header->sr_version = sr_asm_version;
	}
	sssr_header->header_len =
		OFFSETOF(sssr_header_t, flags) - OFFSETOF(sssr_header_t, header_len);
	sssr_header->chipid = dhd_bus_chip(dhd->bus);
	sssr_header->chiprev = dhd_bus_chiprev(dhd->bus);

}

static int
dhdpcie_sssr_d11_header(dhd_pub_t *dhd, uint *buf, uint32 data_len, uint16 coreunit, uint32 *len)
{
	int ret = 0;

	switch (dhd->sssr_reg_info->rev2.version) {
	case SSSR_REG_INFO_VER_6:
	case SSSR_REG_INFO_VER_5:
		{
			sssr_header_t sssr_header;
			uint32 war_reg = 0;
			bzero(&sssr_header, sizeof(sssr_header_t));
			dhdpcie_sssr_common_header(dhd, &sssr_header);
			sssr_header.data_len = data_len;
			sssr_header.coreid = D11_CORE_ID;
			sssr_header.coreunit = coreunit;
			ret = dhd_sssr_mac_war_reg(dhd, coreunit, &war_reg);
			if (ret == BCME_OK) {
				sssr_header.war_reg = war_reg;
			}

			ret = memcpy_s(buf, data_len, &sssr_header, sizeof(sssr_header_t));
			if (ret) {
				DHD_ERROR(("%s: D11 sssr_header memcpy_s failed: %d\n",
						__FUNCTION__, ret));
					return ret;
			}

			*len = sizeof(sssr_header_t);
		}
		break;
	default:
		*len = 0;
	}

	return BCME_OK;
}

static int
dhdpcie_sssr_dig_header(dhd_pub_t *dhd, uint *buf, uint32 data_len, uint32 *len)
{
	int ret = 0;

	switch (dhd->sssr_reg_info->rev2.version) {
	case SSSR_REG_INFO_VER_6:
	case SSSR_REG_INFO_VER_5:
		{
			sssr_header_t sssr_header;
			uint32 war_reg = 0;
			bzero(&sssr_header, sizeof(sssr_header_t));
			dhdpcie_sssr_common_header(dhd, &sssr_header);
			sssr_header.data_len = data_len;
			sssr_header.coreid = dhd->bus->coreid;
			ret = dhd_sssr_arm_war_reg(dhd, &war_reg);
			if (ret == BCME_OK) {
				sssr_header.war_reg = war_reg;
			}

			ret = memcpy_s(buf, data_len, &sssr_header, sizeof(sssr_header_t));
			if (ret) {
				DHD_ERROR(("%s: DIG sssr header memcpy_s failed: %d\n",
						__FUNCTION__, ret));
					return ret;
			}

			*len = sizeof(sssr_header_t);
		}
		break;
	default:
		*len = 0;
	}

	return BCME_OK;
}

static int
dhdpcie_sssr_saqm_header(dhd_pub_t *dhd, uint *buf, uint32 data_len, uint32 *len)
{
	int ret = 0;

	switch (dhd->sssr_reg_info->rev2.version) {
	case SSSR_REG_INFO_VER_6:
	case SSSR_REG_INFO_VER_5:
		{
			sssr_header_t sssr_header;
			uint32 war_reg = 0;
			bzero(&sssr_header, sizeof(sssr_header_t));
			dhdpcie_sssr_common_header(dhd, &sssr_header);
			sssr_header.data_len = data_len;
			sssr_header.coreid = D11_SAQM_CORE_ID;
			ret = dhd_sssr_saqm_war_reg(dhd, &war_reg);
			if (ret == BCME_OK) {
				sssr_header.war_reg = war_reg;
			}

			ret = memcpy_s(buf, data_len, &sssr_header, sizeof(sssr_header_t));
			if (ret) {
				DHD_ERROR(("%s: SAQM sssr header memcpy_s failed: %d\n",
						__FUNCTION__, ret));
					return ret;
			}

			*len = sizeof(sssr_header_t);
		}
		break;
	default:
		*len = 0;
	}

	return BCME_OK;
}

static int
dhdpcie_sssr_srcb_header(dhd_pub_t *dhd, uint *buf, uint32 data_len, uint32 *len)
{
	int ret = 0;

	switch (dhd->sssr_reg_info->rev2.version) {
	case SSSR_REG_INFO_VER_6:
		{
			sssr_header_t sssr_header;
			uint32 war_reg = 0;
			bzero(&sssr_header, sizeof(sssr_header_t));
			dhdpcie_sssr_common_header(dhd, &sssr_header);
			sssr_header.data_len = data_len;
			sssr_header.coreid = SRCB_CORE_ID;
			ret = dhd_sssr_srcb_war_reg(dhd, &war_reg);
			if (ret == BCME_OK) {
				sssr_header.war_reg = war_reg;
			}

			ret = memcpy_s(buf, data_len, &sssr_header, sizeof(sssr_header_t));
			if (ret) {
				DHD_ERROR(("%s: SRCB sssr header memcpy_s failed: %d\n",
						__FUNCTION__, ret));
					return ret;
			}

			*len = sizeof(sssr_header_t);
		}
		break;
	default:
		*len = 0;
	}

	return BCME_OK;
}

static int
dhdpcie_sssr_cmn_header(dhd_pub_t *dhd, uint *buf, uint32 data_len, uint32 *len)
{
	int ret = 0;

	switch (dhd->sssr_reg_info->rev2.version) {
	case SSSR_REG_INFO_VER_6:
	case SSSR_REG_INFO_VER_5:
		{
			sssr_header_t sssr_header = {0};
			dhdpcie_sssr_common_header(dhd, &sssr_header);
			sssr_header.data_len = data_len;
			sssr_header.coreid = CC_CORE_ID;
			ret = memcpy_s(buf, data_len, &sssr_header, sizeof(sssr_header_t));
			if (ret) {
				DHD_ERROR(("%s: CMN sssr header memcpy_s failed: %d\n",
						__FUNCTION__, ret));
					return ret;
			}

			*len = sizeof(sssr_header_t);
		}
		break;
	default:
		*len = 0;
	}

	return BCME_OK;
}

static bool
dhdpcie_saqm_check_outofreset(dhd_pub_t *dhdp)
{
	dhd_bus_t *bus = dhdp->bus;
	uint save_idx, save_unit;
	uint saqm_buf_size = 0;
	bool ret = FALSE;

	save_idx = si_coreidx(bus->sih);
	save_unit = si_coreunit(bus->sih);

	saqm_buf_size = dhd_sssr_saqm_buf_size(dhdp);

	if ((saqm_buf_size > 0) && si_setcore(bus->sih, D11_SAQM_CORE_ID, 0)) {
		ret = si_iscoreup(bus->sih);
		DHD_PRINT(("dhdpcie_saqm_check_outofreset si_isup %d\n",
				si_iscoreup(bus->sih)));
		si_setcore(bus->sih, save_idx, save_unit);
	}

	return ret;
}

#ifdef DHD_SSSR_DUMP_BEFORE_SR
static int
dhdpcie_sssr_dump_get_before_sr(dhd_pub_t *dhd)
{
	int i;
	uint32 sr_size, xmtaddress, xmtdata, dig_buf_size,
		dig_buf_addr, saqm_buf_size, saqm_buf_addr;
	uint8 num_d11cores;
	uint32 d11_header_len = 0;
	uint32 dig_header_len = 0;
	uint32 saqm_header_len = 0;
	uint *d11_buffer;
	uint *dig_buffer;
	uint *saqm_buffer;
	int sssr_header_populate_state = 0;

	DHD_PRINT(("%s\n", __FUNCTION__));

	num_d11cores = dhd_d11_slices_num_get(dhd);

	for (i = 0; i < num_d11cores; i++) {
		if (dhd->sssr_d11_outofreset[i]) {
			sr_size = dhd_sssr_mac_buf_size(dhd, i);
			xmtaddress = dhd_sssr_mac_xmtaddress(dhd, i);
			xmtdata = dhd_sssr_mac_xmtdata(dhd, i);
			d11_buffer = dhd->sssr_d11_before[i];
			sssr_header_populate_state = dhdpcie_sssr_d11_header(dhd, d11_buffer,
					sr_size, i, &d11_header_len);
			if (sssr_header_populate_state != BCME_OK) {
				DHD_ERROR(("%s: dhdpcie_sssr_d11_header failed\n", __FUNCTION__));
				return BCME_ERROR;
			}

			/* D11 buffer starts right after sssr d11 header */
			d11_buffer = (uint *)((char *)d11_buffer + d11_header_len);
			if (dhdpcie_get_sssr_fifo_dump(dhd, d11_buffer, sr_size, xmtaddress,
					xmtdata) != BCME_OK) {
				DHD_ERROR(("%s: dhdpcie_get_sssr_fifo_dump failed\n",
						__FUNCTION__));
				return BCME_ERROR;
			}
		}
	}

	dig_buf_size = dhd_sssr_dig_buf_size(dhd);
	dig_buf_addr = dhd_sssr_dig_buf_addr(dhd);
	if (dig_buf_size) {
		dig_buffer = dhd->sssr_dig_buf_before;
		sssr_header_populate_state = dhdpcie_sssr_dig_header(dhd, dig_buffer,
				dig_buf_size, &dig_header_len);
		if (sssr_header_populate_state != BCME_OK) {
			DHD_ERROR(("%s: dhdpcie_sssr_dig_header failed\n", __FUNCTION__));
			return BCME_ERROR;
		}
		/* Dig buffer starts right after sssr dig  header */
		dig_buffer = (uint *)((char *)dig_buffer + dig_header_len);
		if (dhdpcie_get_sssr_dig_dump(dhd, dig_buffer, dig_buf_size, dig_buf_addr) !=
				BCME_OK) {
			DHD_ERROR(("%s: Failed to get sssr dig dump!\n", __FUNCTION__));
			return BCME_ERROR;
		}
	}

	saqm_buf_size = dhd_sssr_saqm_buf_size(dhd);
	saqm_buf_addr = dhd_sssr_saqm_buf_addr(dhd);
	if (saqm_buf_size) {
		int supported_vers[SSSR_REG_INFO_VER_MAX] = {0};
		supported_vers[0] = SSSR_REG_INFO_VER_5;
		supported_vers[1] = SSSR_REG_INFO_VER_6;
		supported_vers[2] = -1;
		saqm_buffer = dhd->sssr_saqm_buf_before;
		sssr_header_populate_state = dhdpcie_sssr_saqm_header(dhd, saqm_buffer,
				saqm_buf_size, &saqm_header_len);
		if (sssr_header_populate_state != BCME_OK) {
			DHD_ERROR(("%s: dhdpcie_sssr_saqm_header failed\n", __FUNCTION__));
			return BCME_ERROR;
		}
		/* saqm buffer starts right after saqm header */
		saqm_buffer = (uint *)((char *)saqm_buffer + saqm_header_len);
		if (dhdpcie_get_sssr_subtype_dump(dhd, saqm_buffer, saqm_buf_size,
				saqm_buf_addr, SSSR_SAQM_DUMP, supported_vers) != BCME_OK) {
			DHD_ERROR(("%s: Failed to get sssr saqm dump!\n", __FUNCTION__));
			return BCME_ERROR;
		}
	}

	return BCME_OK;
}
#endif /* DHD_SSSR_DUMP_BEFORE_SR */

static int
dhdpcie_sssr_dump_get_after_sr(dhd_pub_t *dhd)
{
	int i;
	uint32 sr_size, xmtaddress, xmtdata, dig_buf_size,
		dig_buf_addr, saqm_buf_size, saqm_buf_addr,
		srcb_buf_size, srcb_buf_addr;
	uint32 cmn_buf_size = 0, cmn_buf_addr = 0;
	uint8 num_d11cores;
	uint32 d11_header_len = 0;
	uint32 dig_header_len = 0;
	uint32 saqm_header_len = 0;
	uint32 srcb_header_len = 0;
	uint32 cmn_header_len = 0;
	uint *d11_buffer = NULL;
	uint *dig_buffer = NULL;
	uint *saqm_buffer = NULL;
	uint *srcb_buffer = NULL;
	uint *cmn_buffer = NULL;
	int supported_vers[SSSR_REG_INFO_VER_MAX] = {0};
	int sssr_header_populate_state = 0;

	DHD_PRINT(("%s\n", __FUNCTION__));

	num_d11cores = dhd_d11_slices_num_get(dhd);

	for (i = 0; i < num_d11cores; i++) {
		if (dhd->sssr_d11_outofreset[i]) {
			sr_size = dhd_sssr_mac_buf_size(dhd, i);
			xmtaddress = dhd_sssr_mac_xmtaddress(dhd, i);
			xmtdata = dhd_sssr_mac_xmtdata(dhd, i);
			d11_buffer = dhd->sssr_d11_after[i];

			sssr_header_populate_state = dhdpcie_sssr_d11_header(dhd, d11_buffer,
					sr_size, i, &d11_header_len);
			if (sssr_header_populate_state != BCME_OK) {
				DHD_ERROR(("%s: dhdpcie_sssr_d11_header failed\n", __FUNCTION__));
				return BCME_ERROR;
			}

			/* D11 buffer starts right after sssr d11 header */
			d11_buffer = (uint *)((char *)d11_buffer + d11_header_len);
			if (dhdpcie_get_sssr_fifo_dump(dhd, d11_buffer, sr_size,
					xmtaddress, xmtdata) != BCME_OK) {
				DHD_ERROR(("%s: dhdpcie_get_sssr_fifo_dump failed\n",
						__FUNCTION__));
				return BCME_ERROR;
			}
		}
	}

	dig_buf_size = dhd_sssr_dig_buf_size(dhd);
	dig_buf_addr = dhd_sssr_dig_buf_addr(dhd);
	if (dig_buf_size) {
		dig_buffer = dhd->sssr_dig_buf_after;
		sssr_header_populate_state = dhdpcie_sssr_dig_header(dhd, dig_buffer,
				dig_buf_size, &dig_header_len);
		if (sssr_header_populate_state != BCME_OK) {
			DHD_ERROR(("%s: dhdpcie_sssr_dig_header failed\n", __FUNCTION__));
			return BCME_ERROR;
		}

		/* Dig buffer starts right after sssr dig  header */
		dig_buffer = (uint *)((char *)dig_buffer + dig_header_len);
		if (dhdpcie_get_sssr_dig_dump(dhd, dig_buffer, dig_buf_size, dig_buf_addr) !=
				BCME_OK) {
			DHD_ERROR(("%s: dhdpcie_get_sssr_dig_dump failed\n", __FUNCTION__));
			return BCME_ERROR;
		}
	}

	supported_vers[0] = SSSR_REG_INFO_VER_5;
	supported_vers[1] = SSSR_REG_INFO_VER_6;
	supported_vers[2] = -1;
	saqm_buf_size = dhd_sssr_saqm_buf_size(dhd);
	saqm_buf_addr = dhd_sssr_saqm_buf_addr(dhd);
	if (saqm_buf_size) {
		saqm_buffer = dhd->sssr_saqm_buf_after;
		sssr_header_populate_state = dhdpcie_sssr_saqm_header(dhd, saqm_buffer,
				saqm_buf_size, &saqm_header_len);
		if (sssr_header_populate_state != BCME_OK) {
			DHD_ERROR(("%s: dhdpcie_sssr_saqm_header failed\n", __FUNCTION__));
			return BCME_ERROR;
		}
		/* saqm buffer starts right after saqm header */
		saqm_buffer = (uint *)((char *)saqm_buffer + saqm_header_len);
		if (dhdpcie_get_sssr_subtype_dump(dhd, saqm_buffer, saqm_buf_size,
				saqm_buf_addr, SSSR_SAQM_DUMP, supported_vers) != BCME_OK) {
			DHD_ERROR(("%s: Failed to get sssr saqm dump!\n", __FUNCTION__));
			return BCME_ERROR;
		}
	}

	if (dhd->sssr_dump_mode == SSSR_DUMP_MODE_FIS) {
		supported_vers[0] = SSSR_REG_INFO_VER_6;
		supported_vers[1] = -1;
		srcb_buf_size = dhd_sssr_srcb_buf_size(dhd);
		srcb_buf_addr = dhd_sssr_srcb_buf_addr(dhd);
		if (srcb_buf_size > 0) {
			srcb_buffer = dhd->sssr_srcb_buf_after;
			sssr_header_populate_state = dhdpcie_sssr_srcb_header(dhd, srcb_buffer,
					srcb_buf_size, &srcb_header_len);
			if (sssr_header_populate_state != BCME_OK) {
				DHD_ERROR(("%s: dhdpcie_sssr_srcb_header failed\n", __FUNCTION__));
				return BCME_ERROR;
			}
			/* srcb buffer starts right after srcb header */
			srcb_buffer = (uint *)((char *)srcb_buffer + srcb_header_len);
			if (dhdpcie_get_sssr_subtype_dump(dhd, srcb_buffer, srcb_buf_size,
				srcb_buf_addr, SSSR_SRCB_DUMP, supported_vers) != BCME_OK) {
				DHD_ERROR(("%s: Failed to get sssr srcb dump!\n", __FUNCTION__));
				return BCME_ERROR;
			}
		}

		supported_vers[0] = SSSR_REG_INFO_VER_5;
		supported_vers[1] = SSSR_REG_INFO_VER_6;
		supported_vers[2] = -1;
		cmn_buf_size = dhd_sssr_cmn_buf_size(dhd);
		cmn_buf_addr = dhd_sssr_cmn_buf_addr(dhd);
		if (cmn_buf_size && cmn_buf_addr > 0) {
			cmn_buffer = dhd->sssr_cmn_buf_after;
			/* populate header */
			sssr_header_populate_state = dhdpcie_sssr_cmn_header(dhd, cmn_buffer,
					cmn_buf_size, &cmn_header_len);
			if (sssr_header_populate_state != BCME_OK) {
				DHD_ERROR(("%s: dhdpcie_sssr_cmn_header failed\n", __FUNCTION__));
				return BCME_ERROR;
			}
			cmn_buffer = (uint *)((char *)cmn_buffer + cmn_header_len);
			if (dhdpcie_get_sssr_subtype_dump(dhd, cmn_buffer, cmn_buf_size,
					cmn_buf_addr, SSSR_CMN_DUMP, supported_vers) != BCME_OK) {
				DHD_ERROR(("%s: Failed to get sssr cmn dump!\n", __FUNCTION__));
				return BCME_ERROR;
			}
		}
	}
	return BCME_OK;
}

#define GCI_CHIPSTATUS_AUX	GCI_CHIPSTATUS_10
#define GCI_CHIPSTATUS_MAIN	GCI_CHIPSTATUS_11
#define GCI_CHIPSTATUS_DIG	GCI_CHIPSTATUS_12
#define GCI_CHIPSTATUS_SCAN	GCI_CHIPSTATUS_13

#define GCI_CHIPSTATUS_ILLEGAL_INSTR_BITMASK	(1u << 3)
static int
dhdpcie_validate_gci_chip_intstatus(dhd_pub_t *dhd)
{
	int gci_intstatus;
	si_t *sih = dhd->bus->sih;

	/* For now validate only for 4389 chip */
	if (si_chipid(sih) != BCM4389_CHIP_ID) {
		DHD_ERROR(("%s: skipping for chipid:0x%x\n", __FUNCTION__, si_chipid(sih)));
		return BCME_OK;
	}

	gci_intstatus = si_gci_chipstatus(sih, GCI_CHIPSTATUS_MAIN);
	if (gci_intstatus & GCI_CHIPSTATUS_ILLEGAL_INSTR_BITMASK) {
		DHD_ERROR(("%s: Illegal instruction set for MAIN core 0x%x\n",
			__FUNCTION__, gci_intstatus));
		return BCME_ERROR;
	}

	gci_intstatus = si_gci_chipstatus(sih, GCI_CHIPSTATUS_AUX);
	if (gci_intstatus & GCI_CHIPSTATUS_ILLEGAL_INSTR_BITMASK) {
		DHD_ERROR(("%s: Illegal instruction set for AUX core 0x%x\n",
			__FUNCTION__, gci_intstatus));
		return BCME_ERROR;
	}

	gci_intstatus = si_gci_chipstatus(sih, GCI_CHIPSTATUS_SCAN);
	if (gci_intstatus & GCI_CHIPSTATUS_ILLEGAL_INSTR_BITMASK) {
		DHD_ERROR(("%s: Illegal instruction set for SCAN core 0x%x\n",
			__FUNCTION__, gci_intstatus));
		return BCME_ERROR;
	}

	gci_intstatus = si_gci_chipstatus(sih, GCI_CHIPSTATUS_DIG);
	if (gci_intstatus & GCI_CHIPSTATUS_ILLEGAL_INSTR_BITMASK) {
		DHD_ERROR(("%s: Illegal instruction set for DIG core 0x%x\n",
			__FUNCTION__, gci_intstatus));
		return BCME_ERROR;
	}

	return BCME_OK;
}

#define OOBR_DMP_FOR_D11	0x1u
#define OOBR_DMP_FOR_SAQM	0x2u
#define OOBR_DMP_D11_MAIN	0x1u
#define OOBR_DMP_D11_AUX	0x2u
#define OOBR_DMP_D11_SCAN	0x4u

#define OOBR_CAP2_NUMTOPEXTRSRC_MASK	0x1Fu
#define OOBR_CAP2_NUMTOPEXTRSRC_SHIFT	4u	 /* Bits 8:4 */

static int
dhdpcie_dump_oobr(dhd_pub_t *dhd, uint core_bmap, uint coreunit_bmap)
{
	si_t *sih = dhd->bus->sih;
	uint curcore = 0;
	int i = 0;
	hndoobr_reg_t *reg = NULL;
	uint mask = 0x1;
	uint val = 0, idx = 0;

	if (CHIPTYPE(sih->socitype) != SOCI_NCI) {
		return BCME_UNSUPPORTED;
	}

	if (dhd->bus->is_linkdown) {
		DHD_ERROR(("%s: PCIe link is down\n", __FUNCTION__));
		return BCME_NOTUP;
	}
	if (dhd->bus->link_state == DHD_PCIE_WLAN_BP_DOWN ||
		dhd->bus->link_state == DHD_PCIE_COMMON_BP_DOWN) {
		DHD_ERROR(("%s : wlan/common backplane is down (link_state=%u), skip.\n",
			__FUNCTION__, dhd->bus->link_state));
		return BCME_NOTUP;
	}

	curcore = si_coreid(dhd->bus->sih);

	reg = si_setcore(sih, HND_OOBR_CORE_ID, 0);
	if (reg != NULL) {
		uint corecap2 = R_REG(dhd->osh, &reg->capability2);
		uint numtopextrsrc = (corecap2 >> OOBR_CAP2_NUMTOPEXTRSRC_SHIFT) &
			OOBR_CAP2_NUMTOPEXTRSRC_MASK;
		if (corecap2 == (uint)-1) {
			DHD_ERROR(("%s:corecap2=0x%x ! Bad value, set linkdown\n",
				__FUNCTION__, corecap2));
			dhd_bus_set_linkdown(dhd, TRUE);
			return BCME_NOTUP;
		}
		/*
		 * Convert the value (8:4) to a loop count to dump topextrsrcmap.
		 * TopRsrcDestSel0 is accessible if NUM_TOP_EXT_RSRC > 0
		 * TopRsrcDestSel1 is accessible if NUM_TOP_EXT_RSRC > 4
		 * TopRsrcDestSel2 is accessible if NUM_TOP_EXT_RSRC > 8
		 * TopRsrcDestSel3 is accessible if NUM_TOP_EXT_RSRC > 12
		 * 0		--> 0
		 * 1-3		--> 1	(TopRsrcDestSel0)
		 * 4-7		--> 2	(TopRsrcDestSel1/0)
		 * 8 - 11		--> 3	(TopRsrcDestSel2/1/0)
		 * 12 - 15	--> 4	(TopRsrcDestSel3/2/1/0)
		 */
		numtopextrsrc = numtopextrsrc ? (numtopextrsrc / 4) + 1 : numtopextrsrc;
		DHD_PRINT(("reg: corecap2:0x%x numtopextrsrc: %d\n", corecap2, numtopextrsrc));
		for (i = 0; i < numtopextrsrc; ++i) {
			val = R_REG(dhd->osh, &reg->topextrsrcmap[i]);
			DHD_PRINT(("reg: hndoobr_reg->topextrsrcmap[%d] = 0x%x\n", i, val));
		}
		for (i = 0; i < 4; ++i) {
			val = R_REG(dhd->osh, &reg->intstatus[i]);
			DHD_PRINT(("reg: hndoobr_reg->intstatus[%d] = 0x%x\n", i, val));
		}
		if (core_bmap & OOBR_DMP_FOR_D11) {
			for (i = 0; coreunit_bmap != 0; ++i) {
				if (coreunit_bmap & mask) {
					idx = si_findcoreidx(sih, D11_CORE_ID, i);
					val = R_REG(dhd->osh,
						&reg->percore_reg[idx].clkpwrreq);
					DHD_PRINT(("reg: D11 core, coreunit %d, clkpwrreq=0x%x\n",
						i, val));
				}
				coreunit_bmap >>= 1;
			}
		}
		if (core_bmap & OOBR_DMP_FOR_SAQM) {
			idx = si_findcoreidx(sih, D11_SAQM_CORE_ID, 0);
			val = R_REG(dhd->osh, &reg->percore_reg[idx].clkpwrreq);
			DHD_PRINT(("reg: D11_SAQM core, coreunit 0, clkpwrreq=0x%x\n", val));
		}
	}

	si_setcore(sih, curcore, 0);
	return BCME_OK;
}

int
dhdpcie_sssr_dump(dhd_pub_t *dhd)
{
	uint32 powerctrl_val = 0, pwrctrl = 0;
	uint32 pwrreq_val = 0;
	dhd_bus_t *bus = dhd->bus;
	si_t *sih = bus->sih;
	uint core_bmap = 0, coreunit_bmap = 0;
	uint32 old_max_resmask = 0, min_resmask = 0, val = 0;
	uint32 sssr_max_res_mask = 0;
	bool saqm_isup = FALSE;
	ulong flags;
	int ret = BCME_OK;

	DHD_GENERAL_LOCK(bus->dhd, flags);
	DHD_BUS_BUSY_SET_IN_SSSR(bus->dhd);
	DHD_GENERAL_UNLOCK(bus->dhd, flags);

	if (!dhd->sssr_inited) {
		DHD_ERROR(("%s: SSSR not inited\n", __FUNCTION__));
		ret = BCME_ERROR;
		goto exit;
	}

	if (dhd->bus->is_linkdown) {
		DHD_ERROR(("%s: PCIe link is down\n", __FUNCTION__));
		ret = BCME_ERROR;
		goto exit;
	}

	if (dhd->bus->cto_triggered) {
		DHD_ERROR(("%s: CTO Triggered\n", __FUNCTION__));
		ret = BCME_ERROR;
		goto exit;
	}

	if (dhdpcie_validate_gci_chip_intstatus(dhd) != BCME_OK) {
		DHD_ERROR(("%s: ## Invalid GCI Chip intstatus, Abort SSSR ##\n",
			__FUNCTION__));
		ret = BCME_ERROR;
		goto exit;
	}

	bus->link_state = dhdpcie_get_link_state(bus);
	if (bus->link_state != DHD_PCIE_ALL_GOOD) {
		DHD_ERROR(("%s: PCIe Link is not good! link_state=%u, Abort\n",
			__FUNCTION__, bus->link_state));
		ret = BCME_ERROR;
		goto exit;
	}

	dhdpcie_print_amni_regs(bus, FALSE);

	DHD_PRINT(("%s: Before WL down (powerctl: pcie:0x%x chipc:0x%x) "
		"PMU rctl:0x%x res_state:0x%x\n", __FUNCTION__,
		si_corereg(sih, sih->buscoreidx,
			CC_REG_OFF(PowerControl), 0, 0),
		si_corereg(sih, 0, CC_REG_OFF(PowerControl), 0, 0),
		PMU_REG(sih, RetentionControl, 0, 0),
		PMU_REG(sih, RsrcState, 0, 0)));

	dhdpcie_d11_check_outofreset(dhd);
	saqm_isup = dhdpcie_saqm_check_outofreset(dhd);
	DHD_PRINT(("%s: Before WL down, SAQM core up state is %d\n",  __FUNCTION__, saqm_isup));

	dhd->sssr_dump_mode = SSSR_DUMP_MODE_SSSR;
#ifdef DHD_SSSR_DUMP_BEFORE_SR
	DHD_PRINT(("%s: Collecting Dump before SR\n", __FUNCTION__));
	if (dhdpcie_sssr_dump_get_before_sr(dhd) != BCME_OK) {
		DHD_ERROR(("%s: dhdpcie_sssr_dump_get_before_sr failed\n", __FUNCTION__));
		ret = BCME_ERROR;
		goto exit;
	}
#endif /* DHD_SSSR_DUMP_BEFORE_SR */

	/* Set the flag to block all membytes or bus dumps */
	bus->sssr_in_progress = TRUE;

	if (dhd->sssr_reg_info->rev2.version >= SSSR_REG_INFO_VER_5) {
		/* Read Min and Max resource mask */
		dhd_sbreg_op(dhd, dhd->sssr_reg_info->rev5.pmu_regs.base_regs.pmu_max_res_mask,
			&old_max_resmask, TRUE);
		dhd_sbreg_op(dhd, dhd->sssr_reg_info->rev5.pmu_regs.base_regs.pmu_min_res_mask,
			&min_resmask, TRUE);
		sssr_max_res_mask = dhd->sssr_reg_info->rev5.pmu_regs.base_regs.sssr_max_res_mask;
		dhdpcie_arm_clear_clk_req(dhd);
		dhdpcie_saqm_clear_clk_req(dhd);
		dhdpcie_pcie_send_ltrsleep(dhd);
		/* MaxRsrcMask is updated to bring down the resources for rev5 and above */
		val = sssr_max_res_mask | min_resmask;
		dhd_sbreg_op(dhd, dhd->sssr_reg_info->rev5.pmu_regs.base_regs.pmu_max_res_mask,
			&val, FALSE);
		/* Wait for some time before Restore */
		OSL_DELAY(100 * 1000);
	} else {
		dhdpcie_clear_intmask_and_timer(dhd);
		dhdpcie_clear_clk_req(dhd);
		powerctrl_val = dhdpcie_suspend_chipcommon_powerctrl(dhd);
		dhdpcie_pcie_send_ltrsleep(dhd);

		/* save current pwr req state and clear pwr req for all domains */
		pwrreq_val = si_srpwr_request(sih, 0, 0);
		pwrreq_val >>= SRPWR_REQON_SHIFT;
		pwrreq_val &= SRPWR_DMN_ALL_MASK(sih);
		DHD_PRINT(("%s: clear pwr req all domains\n", __FUNCTION__));
		si_srpwr_request(sih, SRPWR_DMN_ALL_MASK(sih), 0);

		if (MULTIBP_ENAB(sih)) {
			dhd_bus_pcie_pwr_req_wl_domain(dhd->bus, CC_REG_OFF(PowerControl), FALSE);
		}
		/* Wait for some time before Restore */
		OSL_DELAY(10000);
	}
	pwrctrl = si_corereg(sih, 0, CC_REG_OFF(PowerControl), 0, 0);

	DHD_PRINT(("%s: After WL down (powerctl: pcie:0x%x chipc:0x%x) "
		"PMU rctl:0x%x res_state:0x%x old_max_resmask:0x%x min_resmask:0x%x "
		"sssr_max_res_mask:0x%x max_resmask:0x%x\n", __FUNCTION__,
		si_corereg(sih, sih->buscoreidx, CC_REG_OFF(PowerControl), 0, 0),
		pwrctrl, PMU_REG(sih, RetentionControl, 0, 0),
		PMU_REG(sih, RsrcState, 0, 0), old_max_resmask, min_resmask,
		sssr_max_res_mask, PMU_REG(sih, MaxResourceMask, 0, 0)));

	/* again check if some regs are read as 0xffffs to avoid getting
	 * sssr from a bad pcie link
	 */
	if (pwrctrl == (uint32)-1) {
		DHD_ERROR(("%s: PCIe Link after WL down is not good! pwrctrl=%x, Abort\n",
			__FUNCTION__, pwrctrl));
		bus->link_state = DHD_PCIE_COMMON_BP_DOWN;
		dhd_bus_set_linkdown(dhd, TRUE);
		ret = BCME_ERROR;
		goto exit;
	}

	if (dhd->sssr_reg_info->rev2.version >= SSSR_REG_INFO_VER_5) {
		dhd_sbreg_op(dhd, dhd->sssr_reg_info->rev5.pmu_regs.base_regs.pmu_max_res_mask,
			&old_max_resmask, FALSE);
	}
	if (MULTIBP_ENAB(sih)) {

		if ((pwrctrl >> SRPWR_STATUS_SHIFT) & SRPWR_DMN1_ARMBPSD_MASK) {
			DHD_ERROR(("DIG Domain is not going down. The DIG SSSR is not valid.\n"));
		}

		if ((pwrctrl >> SRPWR_STATUS_SHIFT) & SRPWR_DMN2_MACAUX_MASK) {
			DHD_ERROR(("MAC AUX Domain is not going down.\n"));
			core_bmap |= OOBR_DMP_FOR_D11;
			coreunit_bmap |= OOBR_DMP_D11_AUX;
		}

		if ((pwrctrl >> SRPWR_STATUS_SHIFT) & SRPWR_DMN3_MACMAIN_MASK) {
			DHD_ERROR(("MAC MAIN Domain is not going down\n"));
			core_bmap |= OOBR_DMP_FOR_D11;
			coreunit_bmap |= OOBR_DMP_D11_MAIN;
		}

		if ((pwrctrl >> SRPWR_STATUS_SHIFT) & SRPWR_DMN4_MACSCAN_MASK) {
			DHD_ERROR(("MAC SCAN Domain is not going down.\n"));
			core_bmap |= OOBR_DMP_FOR_D11;
			coreunit_bmap |= OOBR_DMP_D11_SCAN;
		}

		if ((pwrctrl >> SRPWR_STATUS_SHIFT) & SRPWR_DMN6_SAQM_MASK) {
			DHD_ERROR(("SAQM Domain is not going down.\n"));
			core_bmap |= OOBR_DMP_FOR_SAQM;
		}

		if (core_bmap) {
			ret = dhdpcie_dump_oobr(dhd, core_bmap, coreunit_bmap);
			if (ret == BCME_NOTUP) {
				DHD_ERROR(("%s: dhdpcie_dump_oobr fails due to linkdown !\n",
					__FUNCTION__));
				goto exit;
			}
		}

		dhd_bus_pcie_pwr_req_wl_domain(dhd->bus, CC_REG_OFF(PowerControl), TRUE);
		/* Add delay for WL domain to power up */
		OSL_DELAY(15000);

		DHD_PRINT(("%s: After WL up again (powerctl: pcie:0x%x chipc:0x%x) "
				"PMU rctl:0x%x res_state:0x%x old_max_resmask:0x%x "
				"min_resmask:0x%x sssr_max_res_mask:0x%x "
				"max_resmask:0x%x\n", __FUNCTION__,
				si_corereg(sih, sih->buscoreidx,
					CC_REG_OFF(PowerControl), 0, 0),
				si_corereg(sih, 0, CC_REG_OFF(PowerControl), 0, 0),
				PMU_REG(sih, RetentionControl, 0, 0),
				PMU_REG(sih, RsrcState, 0, 0), old_max_resmask, min_resmask,
				sssr_max_res_mask, PMU_REG(sih, MaxResourceMask, 0, 0)));
	}

	dhdpcie_resume_chipcommon_powerctrl(dhd, powerctrl_val);
	dhdpcie_arm_resume_clk_req(dhd);

	if (dhd->sssr_reg_info->rev2.version <= SSSR_REG_INFO_VER_4) {
		/* Before collecting SSSR dump explicitly request power
		* for main and aux domains as per recommendation
		* of ASIC team
		*/
		si_srpwr_request(sih, SRPWR_DMN_ALL_MASK(sih), SRPWR_DMN_ALL_MASK(sih));
	}

	if (dhd->sssr_reg_info->rev2.version == SSSR_REG_INFO_VER_4) {
		dhdpcie_bring_saqm_updown(dhd, TRUE);
	} else if (dhd->sssr_reg_info->rev2.version >= SSSR_REG_INFO_VER_5) {
		dhdpcie_bring_saqm_updown(dhd, FALSE);
	}

	dhdpcie_bring_d11_outofreset(dhd);

	if (dhd->sssr_reg_info->rev2.version == SSSR_REG_INFO_VER_4) {
		dhdpcie_bring_saqm_updown(dhd, FALSE);
	}

	/* Add delay for d11 cores out of reset */
	OSL_DELAY(6000);

	saqm_isup = dhdpcie_saqm_check_outofreset(dhd);
	DHD_PRINT(("%s: After WL UP and out of reset, SAQM core up state is %d\n",
		__FUNCTION__, saqm_isup));
	if (saqm_isup && (dhd->sssr_reg_info->rev2.version >= SSSR_REG_INFO_VER_5)) {
		dhdpcie_saqm_clear_force_sr_all(dhd);
	}

	/* Clear the flag to unblock membytes or bus dumps */
	bus->sssr_in_progress = FALSE;

	DHD_PRINT(("%s: Collecting Dump after SR\n", __FUNCTION__));
	if (dhdpcie_sssr_dump_get_after_sr(dhd) != BCME_OK) {
		DHD_ERROR(("%s: dhdpcie_sssr_dump_get_after_sr failed\n", __FUNCTION__));
		ret = BCME_ERROR;
		goto exit;
	}
	dhd->sssr_dump_collected = TRUE;

	/* restore back previous pwr req values */
	DHD_PRINT(("%s: restore pwr req prev state 0x%x\n", __FUNCTION__, pwrreq_val));
	si_srpwr_request(sih, pwrreq_val, pwrreq_val);

	DHD_PRINT(("%s: restore done\n", __FUNCTION__));
	dhd_write_sssr_dump(dhd, SSSR_DUMP_MODE_SSSR);
	DHD_PRINT(("%s: sssr dump done\n", __FUNCTION__));

	dhdpcie_print_amni_regs(bus, FALSE);
exit:
	DHD_GENERAL_LOCK(bus->dhd, flags);
	DHD_BUS_BUSY_CLEAR_IN_SSSR(bus->dhd);
	DHD_GENERAL_UNLOCK(bus->dhd, flags);

	return ret;
}

static void
dhdpcie_clear_pmu_debug_mode(dhd_pub_t *dhd)
{
	uint32 vreg_ctrl_addr, vreg_ctrl_data_addr, vreg_num, vreg_offset;
	sssr_reg_info_cmn_t *sssr_reg_info_cmn = dhd->sssr_reg_info;
	sssr_reg_info_v6_t *sssr_reg_info = (sssr_reg_info_v6_t *)&sssr_reg_info_cmn->rev3;
	uint32 val = 0;

	if (sssr_reg_info->version < SSSR_REG_INFO_VER_6) {
		DHD_ERROR(("%s: not supported for version:%d\n",
			__FUNCTION__, sssr_reg_info->version));
		return;
	}

	vreg_ctrl_addr = sssr_reg_info->pmu_dbug_rst_regs.vreg_addr;
	vreg_ctrl_data_addr = sssr_reg_info->pmu_dbug_rst_regs.vreg_data_addr;
	vreg_num = sssr_reg_info->pmu_dbug_rst_regs.vreg_num;
	vreg_offset = sssr_reg_info->pmu_dbug_rst_regs.vreg_offset;

	if (IS_HWADDR_INVALID(vreg_ctrl_addr) || IS_HWADDR_INVALID(vreg_ctrl_data_addr)) {
		DHD_ERROR(("%s: Bad values ! vreg_ctrl_addr=0x%x; vreg_ctrl_data_addr=0x%x;\n",
			__FUNCTION__, vreg_ctrl_addr, vreg_ctrl_data_addr));
		return;
	}

	dhd_sbreg_op(dhd, vreg_ctrl_addr, &vreg_num, FALSE);
	dhd_sbreg_op(dhd, vreg_ctrl_data_addr, &val, TRUE);
	val |= 1 << vreg_offset;
	dhd_sbreg_op(dhd, vreg_ctrl_data_addr, &val, FALSE);
	OSL_DELAY(100);
	val &= ~(1 << vreg_offset);
	dhd_sbreg_op(dhd, vreg_ctrl_data_addr, &val, FALSE);
}

#define PCIE_CFG_DSTATE_MASK		0x11u
#define CHIPCOMMON_WAR_SIGNATURE	0xabcdu
#define FIS_DONE_DELAY			(100 * 1000) /* 100ms */

int
dhdpcie_fis_recover(dhd_pub_t *dhd)
{
	uint32 FISCtrlStatus = 0;
#if defined(FIS_WITH_CMN) || defined(FIS_WITHOUT_CMN)
	uint32 FISTrigRsrcState, RsrcState, MinResourceMask;
#endif /* FIS_WITH_CMN || FIS_WITHOUT_CMN */

#ifdef FIS_WITH_CMN
	uint32 cfg_status_cmd;
	uint32 cfg_pmcsr;
	/*
	 * For android built-in platforms need to perform REG ON/OFF
	 * to restore pcie link.
	 * dhd_download_fw_on_driverload will be FALSE for built-in.
	 */
	if (!dhd_download_fw_on_driverload) {
		DHD_PRINT(("%s: Toggle REG_ON and restore config space\n", __FUNCTION__));
#ifdef BOARD_STB
		dhd_plat_pcie_suspend_nosave(dhd->plat_info);
#else
		dhdpcie_bus_stop_host_dev(dhd->bus);
#endif /* BOARD_STB */
		dhd_wifi_platform_set_power(dhd, FALSE);
		dhd_wifi_platform_set_power(dhd, TRUE);
		dhd_bus_reset_link_state(dhd);
		dhdpcie_bus_start_host_dev(dhd->bus);
		/* Restore inited pcie cfg from pci_load_saved_state */
		dhdpcie_bus_enable_device(dhd->bus);
	}

	/* Use dhd restore function instead of kernel api */
	dhdpcie_config_restore(dhd->bus, TRUE);

	cfg_status_cmd = dhd_pcie_config_read(dhd->bus, PCIECFGREG_STATUS_CMD, sizeof(uint32));
	cfg_pmcsr = dhd_pcie_config_read(dhd->bus, PCIE_CFG_PMCSR, sizeof(uint32));
	DHD_PRINT(("after restore: Status Command(0x%x)=0x%x PCIE_CFG_PMCSR(0x%x)=0x%x\n",
		PCIECFGREG_STATUS_CMD, cfg_status_cmd, PCIE_CFG_PMCSR, cfg_pmcsr));

	DHD_PRINT(("after restore: PCI_BAR0_WIN(0x%x)=0x%x PCI_BAR1_WIN(0x%x)=0x%x\n",
		PCI_BAR0_WIN, dhd_pcie_config_read(dhd->bus, PCI_BAR0_WIN, sizeof(uint32)),
		PCI_BAR1_WIN, dhd_pcie_config_read(dhd->bus, PCI_BAR1_WIN, sizeof(uint32))));

	DHD_PRINT(("after restore: PCIE2_BAR0_WIN2(0x%x)=0x%x"
		" PCIE2_BAR0_CORE2_WIN(0x%x)=0x%x PCIE2_BAR0_CORE2_WIN2(0x%x)=0x%x\n",
		PCIE2_BAR0_WIN2, dhd_pcie_config_read(dhd->bus, PCIE2_BAR0_WIN2, sizeof(uint32)),
		PCIE2_BAR0_CORE2_WIN,
		dhd_pcie_config_read(dhd->bus, PCIE2_BAR0_CORE2_WIN, sizeof(uint32)),
		PCIE2_BAR0_CORE2_WIN2,
		dhd_pcie_config_read(dhd->bus, PCIE2_BAR0_CORE2_WIN2, sizeof(uint32))));

	/*
	 * To-Do: below is debug code, remove this if EP is in D0 after REG-ON restore
	 * in both MSM and LSI RCs
	 */
	if ((cfg_pmcsr & PCIE_CFG_DSTATE_MASK) != 0) {
		int ret = dhdpcie_set_master_and_d0_pwrstate(dhd->bus);
		if (ret != BCME_OK) {
			DHD_ERROR(("%s: Setting D0 failed, ABORT FIS collection\n", __FUNCTION__));
			return ret;
		}
		cfg_status_cmd =
			dhd_pcie_config_read(dhd->bus, PCIECFGREG_STATUS_CMD, sizeof(uint32));
		cfg_pmcsr = dhd_pcie_config_read(dhd->bus, PCIE_CFG_PMCSR, sizeof(uint32));
		DHD_PRINT(("after force-d0: Status Command(0x%x)=0x%x PCIE_CFG_PMCSR(0x%x)=0x%x\n",
			PCIECFGREG_STATUS_CMD, cfg_status_cmd, PCIE_CFG_PMCSR, cfg_pmcsr));
	}

	FISCtrlStatus = PMU_REG(dhd->bus->sih, FISCtrlStatus, 0, 0);
	FISTrigRsrcState = PMU_REG(dhd->bus->sih, FISTrigRsrcState, 0, 0);
	RsrcState = PMU_REG(dhd->bus->sih, RsrcState, 0, 0);
	MinResourceMask = PMU_REG(dhd->bus->sih, MinResourceMask, 0, 0);
	DHD_PRINT(("%s: After trigger & %u us delay: FISCtrlStatus=0x%x, FISTrigRsrcState=0x%x,"
		" RsrcState=0x%x MinResourceMask=0x%x\n",
		__FUNCTION__, FIS_DONE_DELAY, FISCtrlStatus, FISTrigRsrcState,
		RsrcState, MinResourceMask));
#endif /* FIS_WITH_CMN */

#ifdef FIS_WITHOUT_CMN
	FISCtrlStatus = PMU_REG(dhd->bus->sih, FISCtrlStatus, 0, 0);
	FISTrigRsrcState = PMU_REG(dhd->bus->sih, FISTrigRsrcState, 0, 0);
	RsrcState = PMU_REG(dhd->bus->sih, RsrcState, 0, 0);
	MinResourceMask = PMU_REG(dhd->bus->sih, MinResourceMask, 0, 0);
	DHD_PRINT(("%s: After trigger & %u us delay: FISCtrlStatus=0x%x, FISTrigRsrcState=0x%x,"
		" RsrcState=0x%x MinResourceMask=0x%x\n",
		__FUNCTION__, FIS_DONE_DELAY, FISCtrlStatus, FISTrigRsrcState,
		RsrcState, MinResourceMask));
#endif /* FIS_WITHOUT_CMN */

	if ((FISCtrlStatus & PMU_CLEAR_FIS_DONE_MASK) == 0) {
		DHD_ERROR(("%s: FIS Done bit not set. exit\n", __FUNCTION__));
		return BCME_ERROR;
	}

	dhdpcie_clear_pmu_debug_mode(dhd);

	/* Clear fis_triggered as REG OFF/ON recovered link */
	dhd->fis_triggered = FALSE;

	return BCME_OK;
}

static int
dhdpcie_fis_trigger(dhd_pub_t *dhd)
{
	uint32 cfg_status_cmd;
	uint32 cfg_pmcsr;

	BCM_REFERENCE(cfg_status_cmd);
	BCM_REFERENCE(cfg_pmcsr);

	if (!dhd->sssr_inited) {
		DHD_ERROR(("%s: SSSR not inited\n", __FUNCTION__));
		return BCME_ERROR;
	}

	if (dhd->bus->is_linkdown) {
		DHD_ERROR(("%s: PCIe link is down\n", __FUNCTION__));
		return BCME_ERROR;
	}

#ifdef DHD_PCIE_RUNTIMEPM
	/* Bring back to D0 */
	dhdpcie_runtime_bus_wake(dhd, CAN_SLEEP(), __builtin_return_address(0));
	/* Stop RPM timer so that even INB DW DEASSERT should not happen */
	DHD_STOP_RPM_TIMER(dhd);
#endif /* DHD_PCIE_RUNTIMEPM */

	/* Set fis_triggered flag to ignore link down callback from RC */
	dhd->fis_triggered = TRUE;

#ifdef FIS_WITH_CMN
	/* for android platforms, since they support WL_REG_ON toggle,
	 * trigger FIS with common subcore - which involves saving pcie
	 * config space, toggle REG_ON and restoring pcie config space
	 */
	cfg_status_cmd = dhd_pcie_config_read(dhd->bus, PCIECFGREG_STATUS_CMD, sizeof(uint32));
	cfg_pmcsr = dhd_pcie_config_read(dhd->bus, PCIE_CFG_PMCSR, sizeof(uint32));
	DHD_PRINT(("before save: Status Command(0x%x)=0x%x PCIE_CFG_PMCSR(0x%x)=0x%x\n",
		PCIECFGREG_STATUS_CMD, cfg_status_cmd, PCIE_CFG_PMCSR, cfg_pmcsr));

	DHD_PRINT(("before save: PCI_BAR0_WIN(0x%x)=0x%x PCI_BAR1_WIN(0x%x)=0x%x\n",
		PCI_BAR0_WIN, dhd_pcie_config_read(dhd->bus, PCI_BAR0_WIN, sizeof(uint32)),
		PCI_BAR1_WIN, dhd_pcie_config_read(dhd->bus, PCI_BAR1_WIN, sizeof(uint32))));

	DHD_PRINT(("before save: PCIE2_BAR0_WIN2(0x%x)=0x%x"
		" PCIE2_BAR0_CORE2_WIN(0x%x)=0x%x PCIE2_BAR0_CORE2_WIN2(0x%x)=0x%x\n",
		PCIE2_BAR0_WIN2, dhd_pcie_config_read(dhd->bus, PCIE2_BAR0_WIN2, sizeof(uint32)),
		PCIE2_BAR0_CORE2_WIN,
		dhd_pcie_config_read(dhd->bus, PCIE2_BAR0_CORE2_WIN, sizeof(uint32)),
		PCIE2_BAR0_CORE2_WIN2,
		dhd_pcie_config_read(dhd->bus, PCIE2_BAR0_CORE2_WIN2, sizeof(uint32))));

	/* Use dhd save function instead of kernel api */
	dhdpcie_config_save(dhd->bus);
#ifdef BOARD_STB
	dhd_plat_pcie_savestate(dhd->plat_info);
#endif /* BOARD_STB */

	/* Trigger FIS */
	si_corereg(dhd->bus->sih, dhd->bus->sih->buscoreidx,
		DAR_FIS_CTRL(dhd->bus->sih->buscorerev), ~0, DAR_FIS_START_MASK);
	OSL_DELAY(FIS_DONE_DELAY);
#endif /* FIS_WITH_CMN */

#ifdef FIS_WITHOUT_CMN
	/* for non-android platforms, since they do not support
	 * WL_REG_ON toggle, trigger FIS without common subcore
	 * the PcieSaveEn bit in PMU FISCtrlStatus reg would be
	 * set to 0 during init time
	 */
	si_corereg(dhd->bus->sih, dhd->bus->sih->buscoreidx,
		DAR_FIS_CTRL(dhd->bus->sih->buscorerev), DAR_FIS_START_MASK, DAR_FIS_START_MASK);
	/* wait for FIS done */
	OSL_DELAY(FIS_DONE_DELAY);
	/* clear the timeout interrupt in PCIE errlog register
	 * before reading any register on backplane
	 */
	si_corereg(dhd->bus->sih, dhd->bus->sih->buscoreidx,
		PCIE_REG_OFF(dar_errorlog), DAR_ERRLOG_MASK, DAR_ERRLOG_MASK);
#endif /* FIS_WITHOUT_CMN */

	return dhdpcie_fis_recover(dhd);
}

int
dhd_bus_fis_trigger(dhd_pub_t *dhd)
{
	return dhdpcie_fis_trigger(dhd);
}

bool
dhdpcie_set_collect_fis(dhd_bus_t *bus)
{
#if defined(DHD_FIS_DUMP) && (defined(FIS_WITH_CMN) || defined(FIS_WITHOUT_CMN))
	if (CHIPTYPE(bus->sih->socitype) == SOCI_NCI) {
		DHD_PRINT(("%s : Collect FIS dumps\n", __FUNCTION__));
		bus->dhd->collect_fis = TRUE;
		return TRUE;
	}
#endif /* DHD_FIS_DUMP */
	return FALSE;
}

static int
dhdpcie_reset_hwa(dhd_pub_t *dhd)
{
	int ret;
	sssr_reg_info_cmn_t *sssr_reg_info_cmn = dhd->sssr_reg_info;
	sssr_reg_info_v3_t *sssr_reg_info = (sssr_reg_info_v3_t *)&sssr_reg_info_cmn->rev3;

	/* HWA wrapper registers */
	uint32 ioctrl, resetctrl;
	/* HWA base registers */
	uint32 clkenable, clkgatingenable, clkext, clkctlstatus;
	uint32 hwa_resetseq_val[SSSR_HWA_RESET_SEQ_STEPS];
	int i = 0;

	if (sssr_reg_info->version < SSSR_REG_INFO_VER_3) {
		DHD_ERROR(("%s: not supported for version:%d\n",
			__FUNCTION__, sssr_reg_info->version));
		return BCME_UNSUPPORTED;
	}

	if (sssr_reg_info->hwa_regs.base_regs.clkenable == 0) {
		DHD_ERROR(("%s: hwa regs are not set\n", __FUNCTION__));
		return BCME_UNSUPPORTED;
	}

	DHD_PRINT(("%s: version:%d\n", __FUNCTION__, sssr_reg_info->version));

	ioctrl = sssr_reg_info->hwa_regs.wrapper_regs.ioctrl;
	resetctrl = sssr_reg_info->hwa_regs.wrapper_regs.resetctrl;

	clkenable = sssr_reg_info->hwa_regs.base_regs.clkenable;
	clkgatingenable = sssr_reg_info->hwa_regs.base_regs.clkgatingenable;
	clkext = sssr_reg_info->hwa_regs.base_regs.clkext;
	clkctlstatus = sssr_reg_info->hwa_regs.base_regs.clkctlstatus;

	ret = memcpy_s(hwa_resetseq_val, sizeof(hwa_resetseq_val),
		sssr_reg_info->hwa_regs.hwa_resetseq_val,
		sizeof(sssr_reg_info->hwa_regs.hwa_resetseq_val));
	if (ret) {
		DHD_ERROR(("%s: hwa_resetseq_val memcpy_s failed: %d\n",
			__FUNCTION__, ret));
		return ret;
	}

	dhd_sbreg_op(dhd, ioctrl, &hwa_resetseq_val[i++], FALSE);
	dhd_sbreg_op(dhd, resetctrl, &hwa_resetseq_val[i++], FALSE);
	dhd_sbreg_op(dhd, resetctrl, &hwa_resetseq_val[i++], FALSE);
	dhd_sbreg_op(dhd, ioctrl, &hwa_resetseq_val[i++], FALSE);

	dhd_sbreg_op(dhd, clkenable, &hwa_resetseq_val[i++], FALSE);
	dhd_sbreg_op(dhd, clkgatingenable, &hwa_resetseq_val[i++], FALSE);
	dhd_sbreg_op(dhd, clkext, &hwa_resetseq_val[i++], FALSE);
	dhd_sbreg_op(dhd, clkctlstatus, &hwa_resetseq_val[i++], FALSE);

	return BCME_OK;
}

static bool
dhdpcie_fis_fw_triggered_check(struct dhd_bus *bus)
{
	uint32 FISCtrlStatus;

	if (bus->link_state == DHD_PCIE_WLAN_BP_DOWN ||
		bus->link_state == DHD_PCIE_COMMON_BP_DOWN) {
		DHD_ERROR(("%s : wlan/common backplane is down (link_state=%u).\n",
			__FUNCTION__, bus->link_state));
		return FALSE;
	}

	FISCtrlStatus = PMU_REG(bus->sih, FISCtrlStatus, 0, 0);
	if (FISCtrlStatus == (uint32)-1) {
		DHD_ERROR(("%s: WARNING! invalid value of FISCtrlStatus(0x%x)\n", __FUNCTION__,
			FISCtrlStatus));
		return FALSE;
	}
	if ((FISCtrlStatus & PMU_CLEAR_FIS_DONE_MASK) == 0) {
		DHD_PRINT(("%s: FIS trigger done bit not set. FIS control status=0x%x\n",
		 __FUNCTION__, FISCtrlStatus));
		return FALSE;
	} else {
		DHD_PRINT(("%s: FIS trigger done bit set. FIS control status=0x%x\n",
		 __FUNCTION__, FISCtrlStatus));
		return TRUE;
	}
}

static int
dhdpcie_fis_dump(dhd_pub_t *dhd)
{
	int i;
	uint32 FISCtrlStatus,  FISTrigRsrcState, RsrcState;
	uint8 num_d11cores;
	struct dhd_bus *bus = dhd->bus;
	uint32 save_idx = 0;
	int hwa_reset_state;
	uint curcore = 0;
	uint val = 0;
	chipcregs_t *chipcregs = NULL;
	curcore = si_coreid(bus->sih);

	DHD_PRINT(("%s\n", __FUNCTION__));

	if (!dhd->sssr_inited) {
		DHD_ERROR(("%s: SSSR not inited\n", __FUNCTION__));
		return BCME_ERROR;
	}

	dhd->busstate = DHD_BUS_LOAD;

	FISCtrlStatus = PMU_REG(dhd->bus->sih, FISCtrlStatus, 0, 0);
	if ((FISCtrlStatus & PMU_CLEAR_FIS_DONE_MASK) == 0) {
		DHD_ERROR(("%s: FIS Done bit not set. exit\n", __FUNCTION__));
		return BCME_ERROR;
	}

	/* bring up all pmu resources */
	PMU_REG(dhd->bus->sih, MinResourceMask, ~0,
		PMU_REG(dhd->bus->sih, MaxResourceMask, 0, 0));
	OSL_DELAY(10 * 1000);

	num_d11cores = dhd_d11_slices_num_get(dhd);

	for (i = 0; i < num_d11cores; i++) {
		dhd->sssr_d11_outofreset[i] = TRUE;
	}

	if (dhd->sssr_reg_info->rev2.version >= SSSR_REG_INFO_VER_4) {
		dhdpcie_bring_saqm_updown(dhd, TRUE);
	}

	dhdpcie_bring_d11_outofreset(dhd);

	if (dhd->sssr_reg_info->rev2.version >= SSSR_REG_INFO_VER_4) {
		dhdpcie_bring_saqm_updown(dhd, FALSE);
	}

	/* Take DAP core out of reset so that ETB is readable again */
	chipcregs = (chipcregs_t *)si_setcore(bus->sih, CC_CORE_ID, 0);
	if (chipcregs != NULL) {
		val = R_REG(bus->osh, CC_REG_ADDR(chipcregs, JtagMasterCtrl));
		W_REG(bus->osh, CC_REG_ADDR(chipcregs, JtagMasterCtrl),
			val & ~(1 << 9));
	}
	si_setcore(bus->sih, curcore, 0);

	OSL_DELAY(6000);

	FISCtrlStatus = PMU_REG(dhd->bus->sih, FISCtrlStatus, 0, 0);
	FISTrigRsrcState = PMU_REG(dhd->bus->sih, FISTrigRsrcState, 0, 0);
	RsrcState = PMU_REG(dhd->bus->sih, RsrcState, 0, 0);
	DHD_PRINT(("%s: 0 ms before FIS_DONE clear: FISCtrlStatus=0x%x,"
		" FISTrigRsrcState=0x%x, RsrcState=0x%x\n",
		__FUNCTION__, FISCtrlStatus, FISTrigRsrcState, RsrcState));

	/* clear FIS Done */
	PMU_REG(dhd->bus->sih, FISCtrlStatus, PMU_CLEAR_FIS_DONE_MASK, PMU_CLEAR_FIS_DONE_MASK);

	FISCtrlStatus = PMU_REG(dhd->bus->sih, FISCtrlStatus, 0, 0);
	FISTrigRsrcState = PMU_REG(dhd->bus->sih, FISTrigRsrcState, 0, 0);
	RsrcState = PMU_REG(dhd->bus->sih, RsrcState, 0, 0);
	DHD_PRINT(("%s: 0 ms after FIS_DONE clear: FISCtrlStatus=0x%x,"
		" FISTrigRsrcState=0x%x, RsrcState=0x%x\n",
		__FUNCTION__, FISCtrlStatus, FISTrigRsrcState, RsrcState));

	hwa_reset_state = dhdpcie_reset_hwa(dhd);
	if (hwa_reset_state != BCME_OK && hwa_reset_state != BCME_UNSUPPORTED) {
		DHD_ERROR(("%s: dhdpcie_reset_hwa failed\n", __FUNCTION__));
		return BCME_ERROR;
	}

	dhdpcie_d11_check_outofreset(dhd);

	/* take sysmem out of reset - otherwise
	 * socram collected again will read only
	 * 0xffff
	 */
	save_idx = si_coreidx(bus->sih);
	if (si_setcore(bus->sih, SYSMEM_CORE_ID, 0)) {
		si_core_reset(bus->sih, 0, 0);
		si_setcoreidx(bus->sih, save_idx);
	}

	/* FIS trigger puts cores into reset including aximem
	 * so take out of reset again to dump content;
	 * otherwise, AERs with FFs
	 */
	save_idx = si_coreidx(bus->sih);
	if (si_setcore(bus->sih, AXIMEM_CORE_ID, 0)) {
		si_core_reset(bus->sih, 0, 0);
		si_setcoreidx(bus->sih, save_idx);
	}

	DHD_PRINT(("%s: Collecting Dump after SR\n", __FUNCTION__));
	dhd->sssr_dump_mode = SSSR_DUMP_MODE_FIS;
	if (dhdpcie_sssr_dump_get_after_sr(dhd) != BCME_OK) {
		DHD_ERROR(("%s: dhdpcie_sssr_dump_get_after_sr failed\n", __FUNCTION__));
		return BCME_ERROR;
	}
	dhd->sssr_dump_collected = TRUE;
	dhd_write_sssr_dump(dhd, SSSR_DUMP_MODE_FIS);

	if (dhd->bus->link_state != DHD_PCIE_ALL_GOOD) {
		/* reset link state and collect socram */
		dhd->bus->link_state = DHD_PCIE_ALL_GOOD;
		DHD_PRINT(("%s: recollect socram\n", __FUNCTION__));
		/* re-read socram into buffer */
		dhdpcie_get_mem_dump(bus);
	}

	return BCME_OK;
}

int
dhd_bus_fis_dump(dhd_pub_t *dhd)
{
	return dhdpcie_fis_dump(dhd);
}

bool
dhd_bus_fis_fw_triggered_check(dhd_pub_t *dhd)
{
	return dhdpcie_fis_fw_triggered_check(dhd->bus);
}

static void
dhd_fill_sssr_reg_info_4389(dhd_pub_t *dhd)
{
	sssr_reg_info_cmn_t *sssr_reg_info_cmn = dhd->sssr_reg_info;
	sssr_reg_info_v3_t *sssr_reg_info;

	sssr_reg_info = (sssr_reg_info_v3_t *)&sssr_reg_info_cmn->rev3;

	DHD_PRINT(("%s:\n", __FUNCTION__));
	sssr_reg_info->version = SSSR_REG_INFO_VER_3;
	sssr_reg_info->length = sizeof(sssr_reg_info_v3_t);

	sssr_reg_info->pmu_regs.base_regs.pmuintmask0 = 0x18012700;
	sssr_reg_info->pmu_regs.base_regs.pmuintmask1 = 0x18012704;
	sssr_reg_info->pmu_regs.base_regs.resreqtimer = 0x18012644;
	sssr_reg_info->pmu_regs.base_regs.macresreqtimer = 0x18012688;
	sssr_reg_info->pmu_regs.base_regs.macresreqtimer1 = 0x180126f0;
	sssr_reg_info->pmu_regs.base_regs.macresreqtimer2 = 0x18012738;

	sssr_reg_info->chipcommon_regs.base_regs.intmask = 0x18000024;
	sssr_reg_info->chipcommon_regs.base_regs.powerctrl = 0x180001e8;
	sssr_reg_info->chipcommon_regs.base_regs.clockcontrolstatus = 0x180001e0;
	sssr_reg_info->chipcommon_regs.base_regs.powerctrl_mask = 0x1f00;

	sssr_reg_info->arm_regs.base_regs.clockcontrolstatus = 0x180201e0;
	sssr_reg_info->arm_regs.base_regs.clockcontrolstatus_val = 0x20;
	sssr_reg_info->arm_regs.wrapper_regs.resetctrl = 0x18120800;
	sssr_reg_info->arm_regs.wrapper_regs.extrsrcreq = 0x18006234;

	sssr_reg_info->pcie_regs.base_regs.ltrstate = 0x18001c38;
	sssr_reg_info->pcie_regs.base_regs.clockcontrolstatus = 0x180011e0;
	sssr_reg_info->pcie_regs.base_regs.clockcontrolstatus_val = 0x0;
	sssr_reg_info->pcie_regs.wrapper_regs.extrsrcreq = 0x180061b4;

	sssr_reg_info->mac_regs[0].base_regs.xmtaddress = 0x18021130;
	sssr_reg_info->mac_regs[0].base_regs.xmtdata = 0x18021134;
	sssr_reg_info->mac_regs[0].base_regs.clockcontrolstatus = 0x180211e0;
	sssr_reg_info->mac_regs[0].base_regs.clockcontrolstatus_val = 0x20;
	sssr_reg_info->mac_regs[0].wrapper_regs.resetctrl = 0x18121800;
	sssr_reg_info->mac_regs[0].wrapper_regs.extrsrcreq = 0x180062b4;
	sssr_reg_info->mac_regs[0].wrapper_regs.ioctrl = 0x18121408;
	sssr_reg_info->mac_regs[0].wrapper_regs.ioctrl_resetseq_val[0] = 0xc7;
	sssr_reg_info->mac_regs[0].wrapper_regs.ioctrl_resetseq_val[1] = 0x15f;
	sssr_reg_info->mac_regs[0].wrapper_regs.ioctrl_resetseq_val[2] = 0x151;
	sssr_reg_info->mac_regs[0].wrapper_regs.ioctrl_resetseq_val[3] = 0x155;
	sssr_reg_info->mac_regs[0].wrapper_regs.ioctrl_resetseq_val[4] = 0xc5;
	sssr_reg_info->mac_regs[0].sr_size = 0x40000;

	sssr_reg_info->mac_regs[1].base_regs.xmtaddress = 0x18022130;
	sssr_reg_info->mac_regs[1].base_regs.xmtdata = 0x18022134;
	sssr_reg_info->mac_regs[1].base_regs.clockcontrolstatus = 0x180221e0;
	sssr_reg_info->mac_regs[1].base_regs.clockcontrolstatus_val = 0x20;
	sssr_reg_info->mac_regs[1].wrapper_regs.resetctrl = 0x18122800;
	sssr_reg_info->mac_regs[1].wrapper_regs.extrsrcreq = 0x18006334;
	sssr_reg_info->mac_regs[1].wrapper_regs.ioctrl = 0x18122408;
	sssr_reg_info->mac_regs[1].wrapper_regs.ioctrl_resetseq_val[0] = 0xc7;
	sssr_reg_info->mac_regs[1].wrapper_regs.ioctrl_resetseq_val[1] = 0x15f;
	sssr_reg_info->mac_regs[1].wrapper_regs.ioctrl_resetseq_val[2] = 0x151;
	sssr_reg_info->mac_regs[1].wrapper_regs.ioctrl_resetseq_val[3] = 0x155;
	sssr_reg_info->mac_regs[1].wrapper_regs.ioctrl_resetseq_val[4] = 0xc5;
	sssr_reg_info->mac_regs[1].sr_size = 0x30000;

	sssr_reg_info->mac_regs[2].base_regs.xmtaddress = 0x18023130;
	sssr_reg_info->mac_regs[2].base_regs.xmtdata = 0x18023134;
	sssr_reg_info->mac_regs[2].base_regs.clockcontrolstatus = 0x180231e0;
	sssr_reg_info->mac_regs[2].base_regs.clockcontrolstatus_val = 0x20;
	sssr_reg_info->mac_regs[2].wrapper_regs.resetctrl = 0x18123800;
	sssr_reg_info->mac_regs[2].wrapper_regs.extrsrcreq = 0x180063b4;
	sssr_reg_info->mac_regs[2].wrapper_regs.ioctrl = 0x18123408;
	sssr_reg_info->mac_regs[2].wrapper_regs.ioctrl_resetseq_val[0] = 0xc7;
	sssr_reg_info->mac_regs[2].wrapper_regs.ioctrl_resetseq_val[1] = 0x15f;
	sssr_reg_info->mac_regs[2].wrapper_regs.ioctrl_resetseq_val[2] = 0x151;
	sssr_reg_info->mac_regs[2].wrapper_regs.ioctrl_resetseq_val[3] = 0x155;
	sssr_reg_info->mac_regs[2].wrapper_regs.ioctrl_resetseq_val[4] = 0xc5;
	sssr_reg_info->mac_regs[2].sr_size = 0x30000;

	sssr_reg_info->dig_mem_info.dig_sr_addr = 0x18520000;
	sssr_reg_info->dig_mem_info.dig_sr_size = 0x10000;

	sssr_reg_info->fis_enab = 1;

	sssr_reg_info->hwa_regs.base_regs.clkenable = 0x180242d0;
	sssr_reg_info->hwa_regs.base_regs.clkgatingenable = 0x180242d4;
	sssr_reg_info->hwa_regs.base_regs.clkext = 0x180242e0;
	sssr_reg_info->hwa_regs.base_regs.clkctlstatus = 0x180241e0;
	sssr_reg_info->hwa_regs.wrapper_regs.ioctrl = 0x18124408;
	sssr_reg_info->hwa_regs.wrapper_regs.resetctrl = 0x18124800;

	sssr_reg_info->hwa_regs.hwa_resetseq_val[0] = 0x3;
	sssr_reg_info->hwa_regs.hwa_resetseq_val[1] = 0x1;
	sssr_reg_info->hwa_regs.hwa_resetseq_val[2] = 0x0;
	sssr_reg_info->hwa_regs.hwa_resetseq_val[3] = 0x1;
	sssr_reg_info->hwa_regs.hwa_resetseq_val[4] = 0x1ff;
	sssr_reg_info->hwa_regs.hwa_resetseq_val[5] = 0x1ff;
	sssr_reg_info->hwa_regs.hwa_resetseq_val[6] = 0x3;
	sssr_reg_info->hwa_regs.hwa_resetseq_val[7] = 0x20;

	dhd->sssr_inited = TRUE;
}

static void
dhdpcie_fill_sssr_reg_info(dhd_pub_t *dhd)
{
	if (dhd_get_chipid(dhd->bus) == BCM4389_CHIP_ID) {
		dhd_fill_sssr_reg_info_4389(dhd);
	}
}

void
dhdpcie_set_pmu_fisctrlsts(struct dhd_bus *bus)
{
#if defined(FIS_WITH_CMN) || defined(FIS_WITHOUT_CMN)
	uint32 FISCtrlStatus = 0;
#endif /* FIS_WITH_CMN || FIS_WITHOUT_CMN */
	bool   fis_fw_triggered = FALSE;

	if (CHIPTYPE(bus->sih->socitype) != SOCI_NCI) {
		return;
	}

	/* FIS might be triggered in firmware, so FIS collection
	 * should be done and fis control status reg should not be
	 * touched before FIS collection.
	 */
	fis_fw_triggered = dhdpcie_fis_fw_triggered_check(bus);
	if (fis_fw_triggered) {
		return;
	}

#ifdef FIS_WITH_CMN
	/* for platforms where reg on toggle support is present
	 * FIS with common subcore is collected, so set PcieSaveEn bit in
	 * PMU FISCtrlStatus reg
	 */
	FISCtrlStatus = PMU_REG(bus->sih, FISCtrlStatus, PMU_FIS_PCIE_SAVE_EN_VALUE,
		PMU_FIS_PCIE_SAVE_EN_VALUE);
	FISCtrlStatus = PMU_REG(bus->sih, FISCtrlStatus, 0, 0);
	DHD_PRINT(("%s: reg on support present, set PMU FISCtrlStatus=0x%x \n",
		__FUNCTION__, FISCtrlStatus));
#endif /* FIS_WITH_CMN */

#ifdef FIS_WITHOUT_CMN
	/* for platforms where reg on toggle support is absent
	 * FIS without common subcore is collected, so reset PcieSaveEn bit in
	 * PMU FISCtrlStatus reg
	 */
	FISCtrlStatus = PMU_REG(bus->sih, FISCtrlStatus, PMU_FIS_PCIE_SAVE_EN_VALUE, 0x0);
	FISCtrlStatus = PMU_REG(bus->sih, FISCtrlStatus, 0, 0);
	DHD_PRINT(("%s: reg on not supported, set PMU FISCtrlStatus=0x%x \n",
		__FUNCTION__, FISCtrlStatus));
#endif /* FIS_WITHOUT_CMN */
}

int
dhd_sssr_mempool_init(dhd_pub_t *dhd)
{
#ifdef BCMPCIE
	dhd->sssr_mempool = (uint8 *) VMALLOCZ(dhd->osh, DHD_SSSR_MEMPOOL_SIZE);
#else
	dhd->sssr_mempool = (uint8 *) MALLOCZ(dhd->osh, DHD_SSSR_MEMPOOL_SIZE);
#endif /* BCMPCIE */
	if (dhd->sssr_mempool == NULL) {
		DHD_ERROR(("%s: MALLOC of sssr_mempool failed\n",
			__FUNCTION__));
		return BCME_ERROR;
	}
	return BCME_OK;
}

void
dhd_sssr_mempool_deinit(dhd_pub_t *dhd)
{
	if (dhd->sssr_mempool) {
#ifdef BCMPCIE
		VMFREE(dhd->osh, dhd->sssr_mempool, DHD_SSSR_MEMPOOL_SIZE);
#else
		MFREE(dhd->osh, dhd->sssr_mempool, DHD_SSSR_MEMPOOL_SIZE);
#endif /* BCMPCIE */
		dhd->sssr_mempool = NULL;
	}
}

int
dhd_sssr_reg_info_init(dhd_pub_t *dhd)
{
	dhd->sssr_reg_info = (sssr_reg_info_cmn_t *) MALLOCZ(dhd->osh, sizeof(sssr_reg_info_cmn_t));
	if (dhd->sssr_reg_info == NULL) {
		DHD_ERROR(("%s: MALLOC of sssr_reg_info failed\n",
			__FUNCTION__));
		return BCME_ERROR;
	}
	return BCME_OK;
}

void
dhd_sssr_reg_info_deinit(dhd_pub_t *dhd)
{
	if (dhd->sssr_reg_info) {
		MFREE(dhd->osh, dhd->sssr_reg_info, sizeof(sssr_reg_info_cmn_t));
		dhd->sssr_reg_info = NULL;
	}
}

static void
dhd_dump_sssr_reg_info(dhd_pub_t *dhd)
{
}

static int
dhd_get_sssr_reg_info(dhd_pub_t *dhd)
{
	int ret;
	char *filepath_sssr = "/root/sssr_reginfo.dat";

	if (dhd->force_sssr_init) {
		dhdpcie_fill_sssr_reg_info(dhd);
		dhd->force_sssr_init = FALSE;
		goto done;
	}

	/* get sssr_reg_info from firmware */
	ret = dhd_iovar(dhd, 0, "sssr_reg_info", NULL, 0,  (char *)dhd->sssr_reg_info,
		sizeof(sssr_reg_info_cmn_t), FALSE);
	if (ret < 0) {
		DHD_ERROR(("%s: sssr_reg_info failed (error=%d)\n",
			__FUNCTION__, ret));
		return BCME_ERROR;
	}

	/* Write sssr reg info to output file */
	switch (dhd->sssr_reg_info->rev2.version) {
	case SSSR_REG_INFO_VER_6:
		ret = dhd_write_file_and_check(filepath_sssr,
			(char *)(&dhd->sssr_reg_info->rev6),
			sizeof(sssr_reg_info_v6_t));
		break;
	case SSSR_REG_INFO_VER_5:
		ret = dhd_write_file_and_check(filepath_sssr,
			(char *)(&dhd->sssr_reg_info->rev5),
			sizeof(sssr_reg_info_v5_t));
		break;
	case SSSR_REG_INFO_VER_4:
		ret = dhd_write_file_and_check(filepath_sssr,
			(char *)(&dhd->sssr_reg_info->rev4),
			sizeof(sssr_reg_info_v4_t));
		break;
	case SSSR_REG_INFO_VER_3:
		ret = dhd_write_file_and_check(filepath_sssr,
			(char *)(&dhd->sssr_reg_info->rev3),
			sizeof(sssr_reg_info_v3_t));
		break;
	case SSSR_REG_INFO_VER_2:
		ret = dhd_write_file_and_check(filepath_sssr,
			(char *)(&dhd->sssr_reg_info->rev2),
			sizeof(sssr_reg_info_v2_t));
		break;
	case SSSR_REG_INFO_VER_1:
		ret = dhd_write_file_and_check(filepath_sssr,
			(char *)(&dhd->sssr_reg_info->rev1),
			sizeof(sssr_reg_info_v1_t));
		break;
	case SSSR_REG_INFO_VER_0:
		ret = dhd_write_file_and_check(filepath_sssr,
			(char *)(&dhd->sssr_reg_info->rev0),
			sizeof(sssr_reg_info_v0_t));
		break;
	}

	if (ret < 0) {
		DHD_ERROR(("%s: SSSR REG INFO [%s] Failed to write into"
		" File: %s\n", __FUNCTION__, (char *)(&dhd->sssr_reg_info->rev0), filepath_sssr));
	}

done:
	dhd_dump_sssr_reg_info(dhd);
	return BCME_OK;
}

static uint32
dhd_get_sssr_bufsize(dhd_pub_t *dhd)
{
	int i;
	uint32 sssr_bufsize = 0;
	uint8 num_d11cores;

	num_d11cores = dhd_d11_slices_num_get(dhd);

	switch (dhd->sssr_reg_info->rev2.version) {
	case SSSR_REG_INFO_VER_6:
	case SSSR_REG_INFO_VER_5:
		sssr_bufsize += dhd->sssr_reg_info->rev5.sssr_all_mem_info.sysmem_sssr_size;
		break;
	case SSSR_REG_INFO_VER_4:
		sssr_bufsize += dhd->sssr_reg_info->rev4.sssr_all_mem_info.sysmem_sssr_size;
		break;
	case SSSR_REG_INFO_VER_3:
		for (i = 0; i < num_d11cores; i++) {
			sssr_bufsize += dhd->sssr_reg_info->rev3.mac_regs[i].sr_size;
		}
		if ((dhd->sssr_reg_info->rev3.length >
		 OFFSETOF(sssr_reg_info_v3_t, dig_mem_info)) &&
		 dhd->sssr_reg_info->rev3.dig_mem_info.dig_sr_addr) {
			sssr_bufsize += dhd->sssr_reg_info->rev3.dig_mem_info.dig_sr_size;
		}
		break;
	case SSSR_REG_INFO_VER_2:
		for (i = 0; i < num_d11cores; i++) {
			sssr_bufsize += dhd->sssr_reg_info->rev2.mac_regs[i].sr_size;
		}
		if ((dhd->sssr_reg_info->rev2.length >
		 OFFSETOF(sssr_reg_info_v2_t, dig_mem_info)) &&
		 dhd->sssr_reg_info->rev2.dig_mem_info.dig_sr_addr) {
			sssr_bufsize += dhd->sssr_reg_info->rev2.dig_mem_info.dig_sr_size;
		}
		break;
	case SSSR_REG_INFO_VER_1:
		for (i = 0; i < num_d11cores; i++) {
			sssr_bufsize += dhd->sssr_reg_info->rev1.mac_regs[i].sr_size;
		}
		if (dhd->sssr_reg_info->rev1.vasip_regs.vasip_sr_size) {
			sssr_bufsize += dhd->sssr_reg_info->rev1.vasip_regs.vasip_sr_size;
		} else if ((dhd->sssr_reg_info->rev1.length > OFFSETOF(sssr_reg_info_v1_t,
			dig_mem_info)) && dhd->sssr_reg_info->rev1.
			dig_mem_info.dig_sr_addr) {
			sssr_bufsize += dhd->sssr_reg_info->rev1.dig_mem_info.dig_sr_size;
		}
		break;
	case SSSR_REG_INFO_VER_0:
		for (i = 0; i < num_d11cores; i++) {
			sssr_bufsize += dhd->sssr_reg_info->rev0.mac_regs[i].sr_size;
		}
		if (dhd->sssr_reg_info->rev0.vasip_regs.vasip_sr_size) {
			sssr_bufsize += dhd->sssr_reg_info->rev0.vasip_regs.vasip_sr_size;
		}
		break;
	default:
		DHD_ERROR(("invalid sssr_reg_ver"));
		return BCME_UNSUPPORTED;
	}

#ifdef DHD_SSSR_DUMP_BEFORE_SR
	/* Double the size as different dumps will be saved before and after SR */
	sssr_bufsize = 2 * sssr_bufsize;
#endif /* DHD_SSSR_DUMP_BEFORE_SR */

	return sssr_bufsize;
}

int
dhd_sssr_dump_init(dhd_pub_t *dhd, bool fis_dump)
{
	int i;
	uint32 sssr_bufsize = 0;
	uint32 mempool_used = 0;
	uint8 num_d11cores = 0;
	bool alloc_sssr = FALSE;
	uint32 sr_size = 0;
	int supported_vers[SSSR_REG_INFO_VER_MAX] = {0};
	int ret = 0;

	dhd->sssr_inited = FALSE;
	if (!sssr_enab) {
		DHD_ERROR(("%s: sssr dump not inited as instructed by mod param\n", __FUNCTION__));
		return BCME_OK;
	}

	/* check if sssr mempool is allocated */
	if (dhd->sssr_mempool == NULL) {
		DHD_ERROR(("%s: sssr_mempool is not allocated\n",
			__FUNCTION__));
		return BCME_ERROR;
	}

	/* check if sssr mempool is allocated */
	if (dhd->sssr_reg_info == NULL) {
		DHD_ERROR(("%s: sssr_reg_info is not allocated\n",
			__FUNCTION__));
		return BCME_ERROR;
	}

	/* Get SSSR reg info */
	if (dhd_get_sssr_reg_info(dhd) != BCME_OK) {
		if (fis_dump) {
			int err = -1;
			char *filepath_sssr = "/root/sssr_reginfo.dat";
			err = dhd_read_file(filepath_sssr, (char *)(&dhd->sssr_reg_info->rev0),
				sizeof(sssr_reg_info_v0_t));
			switch (dhd->sssr_reg_info->rev2.version) {
			case SSSR_REG_INFO_VER_6:
				err = dhd_read_file(filepath_sssr,
					(char *)(&dhd->sssr_reg_info->rev6),
					sizeof(sssr_reg_info_v6_t));
				break;
			case SSSR_REG_INFO_VER_5:
				err = dhd_read_file(filepath_sssr,
					(char *)(&dhd->sssr_reg_info->rev5),
					sizeof(sssr_reg_info_v5_t));
				break;
			case SSSR_REG_INFO_VER_4:
				err = dhd_read_file(filepath_sssr,
					(char *)(&dhd->sssr_reg_info->rev4),
					sizeof(sssr_reg_info_v4_t));
				break;
			case SSSR_REG_INFO_VER_3:
				err = dhd_read_file(filepath_sssr,
					(char *)(&dhd->sssr_reg_info->rev3),
					sizeof(sssr_reg_info_v3_t));
				break;
			case SSSR_REG_INFO_VER_2:
				err = dhd_read_file(filepath_sssr,
					(char *)(&dhd->sssr_reg_info->rev2),
					sizeof(sssr_reg_info_v2_t));
				break;
			case SSSR_REG_INFO_VER_1:
				err = dhd_read_file(filepath_sssr,
					(char *)(&dhd->sssr_reg_info->rev1),
					sizeof(sssr_reg_info_v1_t));
				break;
			}
			if (err < 0) {
				DHD_ERROR(("%s: dhd_get_sssr_reg_info failed and there"
				" is no FIS cache\n", __FUNCTION__));
				return BCME_ERROR;
			} else {
				DHD_INFO(("%s: dhd_get_sssr_reg_info succeeds"
				"with FIS cache\n", __FUNCTION__));
			}
		} else {
			DHD_ERROR(("%s: dhd_get_sssr_reg_info failed\n", __FUNCTION__));
			DHD_CONS_ONLY(("DEBUG_SSSr: %s: dhd_get_sssr_reg_info failed\n",
				__FUNCTION__));
			return BCME_ERROR;
		}
	}

	num_d11cores = dhd_d11_slices_num_get(dhd);
	/* Validate structure version and length */
	switch (dhd->sssr_reg_info->rev2.version) {
	case SSSR_REG_INFO_VER_6:
		if (dhd->sssr_reg_info->rev6.length != sizeof(sssr_reg_info_v6_t)) {
			DHD_ERROR(("%s: dhd->sssr_reg_info->rev6.length (%d : %d)"
				 "mismatch on rev6\n", __FUNCTION__,
				 (int)dhd->sssr_reg_info->rev6.length,
				 (int)sizeof(sssr_reg_info_v6_t)));
			return BCME_ERROR;
		}
		break;
	case SSSR_REG_INFO_VER_5:
		if ((dhd->sssr_reg_info->rev5.length != sizeof(sssr_reg_info_v5_t)) &&
		(dhd->sssr_reg_info->rev5.length <
		OFFSETOF(sssr_reg_info_v5_t, srcb_mem_info))) {
			DHD_ERROR(("%s: dhd->sssr_reg_info->rev5.length (%d : %d)"
				 "mismatch on rev5\n", __FUNCTION__,
				 (int)dhd->sssr_reg_info->rev5.length,
				 (int)sizeof(sssr_reg_info_v5_t)));
			return BCME_ERROR;
		}
		break;
	case SSSR_REG_INFO_VER_4:
		if (dhd->sssr_reg_info->rev4.length != sizeof(sssr_reg_info_v4_t)) {
			DHD_ERROR(("%s: dhd->sssr_reg_info->rev4.length (%d : %d)"
				 "mismatch on rev4\n", __FUNCTION__,
				 (int)dhd->sssr_reg_info->rev4.length,
				 (int)sizeof(sssr_reg_info_v4_t)));
			return BCME_ERROR;
		}
		break;
	case SSSR_REG_INFO_VER_3:
		if (dhd->sssr_reg_info->rev3.length != sizeof(sssr_reg_info_v3_t)) {
			DHD_ERROR(("%s: dhd->sssr_reg_info->rev3.length (%d : %d)"
				 "mismatch on rev3\n", __FUNCTION__,
				 (int)dhd->sssr_reg_info->rev3.length,
				 (int)sizeof(sssr_reg_info_v3_t)));
			return BCME_ERROR;
		}
		break;
	case SSSR_REG_INFO_VER_2:
		if (dhd->sssr_reg_info->rev2.length != sizeof(sssr_reg_info_v2_t)) {
			DHD_ERROR(("%s: dhd->sssr_reg_info->rev2.length (%d : %d)"
				 "mismatch on rev2\n", __FUNCTION__,
				 (int)dhd->sssr_reg_info->rev2.length,
				 (int)sizeof(sssr_reg_info_v2_t)));
			return BCME_ERROR;
		}
		break;
	case SSSR_REG_INFO_VER_1:
		if (dhd->sssr_reg_info->rev1.length != sizeof(sssr_reg_info_v1_t)) {
			DHD_ERROR(("%s: dhd->sssr_reg_info->rev1.length (%d : %d)"
				 "mismatch on rev1\n", __FUNCTION__,
				 (int)dhd->sssr_reg_info->rev1.length,
				 (int)sizeof(sssr_reg_info_v1_t)));
			return BCME_ERROR;
		}
		break;
	case SSSR_REG_INFO_VER_0:
		if (dhd->sssr_reg_info->rev0.length != sizeof(sssr_reg_info_v0_t)) {
			DHD_ERROR(("%s: dhd->sssr_reg_info->rev0.length (%d : %d)"
				 "mismatch on rev0\n", __FUNCTION__,
				 (int)dhd->sssr_reg_info->rev0.length,
				 (int)sizeof(sssr_reg_info_v0_t)));
			return BCME_ERROR;
		}
		break;
	default:
		DHD_ERROR(("invalid sssr_reg_ver\n"));
		return BCME_UNSUPPORTED;
	}

	/* validate fifo size */
	sssr_bufsize = dhd_get_sssr_bufsize(dhd);
	if (sssr_bufsize > DHD_SSSR_MEMPOOL_SIZE) {
		DHD_ERROR(("%s: sssr_bufsize(%d) is greater than sssr_mempool(%d)\n",
			__FUNCTION__, (int)sssr_bufsize, DHD_SSSR_MEMPOOL_SIZE));
		return BCME_ERROR;
	}

	/* init all pointers to NULL */
	for (i = 0; i < num_d11cores; i++) {
#ifdef DHD_SSSR_DUMP_BEFORE_SR
		dhd->sssr_d11_before[i] = NULL;
#endif /* DHD_SSSR_DUMP_BEFORE_SR */
		dhd->sssr_d11_after[i] = NULL;
	}

#ifdef DHD_SSSR_DUMP_BEFORE_SR
	dhd->sssr_dig_buf_before = NULL;
#endif /* DHD_SSSR_DUMP_BEFORE_SR */
	dhd->sssr_dig_buf_after = NULL;

#ifdef DHD_SSSR_DUMP_BEFORE_SR
	dhd->sssr_saqm_buf_before = NULL;
#endif /* DHD_SSSR_DUMP_BEFORE_SR */
	dhd->sssr_saqm_buf_after = NULL;
	dhd->sssr_srcb_buf_after = NULL;
	dhd->sssr_cmn_buf_after = NULL;

	/* Allocate memory */
	for (i = 0; i < num_d11cores; i++) {
		alloc_sssr = FALSE;
		sr_size = 0;

		switch (dhd->sssr_reg_info->rev2.version) {
		case SSSR_REG_INFO_VER_6:
			/* intentional fall through */
		case SSSR_REG_INFO_VER_5:
			if (dhd->sssr_reg_info->rev5.mac_regs[i].sr_size) {
				alloc_sssr = TRUE;
				sr_size = dhd->sssr_reg_info->rev5.mac_regs[i].sr_size;
				sr_size += sizeof(sssr_header_t);
			}
			break;
		case SSSR_REG_INFO_VER_4:
			if (dhd->sssr_reg_info->rev4.mac_regs[i].sr_size) {
				alloc_sssr = TRUE;
				sr_size = dhd->sssr_reg_info->rev4.mac_regs[i].sr_size;
			}
			break;
		case SSSR_REG_INFO_VER_3:
			/* intentional fall through */
		case SSSR_REG_INFO_VER_2:
			if (dhd->sssr_reg_info->rev2.mac_regs[i].sr_size) {
				alloc_sssr = TRUE;
				sr_size = dhd->sssr_reg_info->rev2.mac_regs[i].sr_size;
			}
			break;
		case SSSR_REG_INFO_VER_1:
			if (dhd->sssr_reg_info->rev1.mac_regs[i].sr_size) {
				alloc_sssr = TRUE;
				sr_size = dhd->sssr_reg_info->rev1.mac_regs[i].sr_size;
			}
			break;
		case SSSR_REG_INFO_VER_0:
			if (dhd->sssr_reg_info->rev0.mac_regs[i].sr_size) {
				alloc_sssr = TRUE;
				sr_size = dhd->sssr_reg_info->rev0.mac_regs[i].sr_size;
			}
			break;
		default:
			DHD_ERROR(("invalid sssr_reg_ver"));
			return BCME_UNSUPPORTED;
		}

		if (alloc_sssr) {
#ifdef DHD_SSSR_DUMP_BEFORE_SR
			dhd->sssr_d11_before[i] = (uint32 *)(dhd->sssr_mempool + mempool_used);
			mempool_used += sr_size;
#endif /* DHD_SSSR_DUMP_BEFORE_SR */

			dhd->sssr_d11_after[i] = (uint32 *)(dhd->sssr_mempool + mempool_used);
			mempool_used += sr_size;
		}
	}

	/* Allocate dump memory for VASIP (version 0 or 1) or digital core (version 0, 1, or 2) */
	alloc_sssr = FALSE;
	sr_size = 0;
	switch (dhd->sssr_reg_info->rev2.version) {
	case SSSR_REG_INFO_VER_6:
		if ((dhd->sssr_reg_info->rev6.length >
		 OFFSETOF(sssr_reg_info_v6_t, sssr_all_mem_info)) &&
		 dhd->sssr_reg_info->rev6.sssr_all_mem_info.sysmem_sssr_addr) {
			alloc_sssr = TRUE;
			sr_size =
				dhd->sssr_reg_info->rev6.sssr_all_mem_info.sysmem_sssr_size;
			sr_size += sizeof(sssr_header_t);
		}
		break;
	case SSSR_REG_INFO_VER_5:
		if ((dhd->sssr_reg_info->rev5.length >
		 OFFSETOF(sssr_reg_info_v5_t, sssr_all_mem_info)) &&
		 dhd->sssr_reg_info->rev5.sssr_all_mem_info.sysmem_sssr_addr) {
			alloc_sssr = TRUE;
			sr_size =
				dhd->sssr_reg_info->rev5.sssr_all_mem_info.sysmem_sssr_size;
			sr_size += sizeof(sssr_header_t);
		}
		break;
	case SSSR_REG_INFO_VER_4:
		/* for v4 need to use sssr_all_mem_info instead of dig_mem_info */
		if ((dhd->sssr_reg_info->rev4.length >
		 OFFSETOF(sssr_reg_info_v4_t, sssr_all_mem_info)) &&
		 dhd->sssr_reg_info->rev4.sssr_all_mem_info.sysmem_sssr_addr) {
			alloc_sssr = TRUE;
			sr_size =
				dhd->sssr_reg_info->rev4.sssr_all_mem_info.sysmem_sssr_size;
		}
		break;
	case SSSR_REG_INFO_VER_3:
		/* intentional fall through */
	case SSSR_REG_INFO_VER_2:
		if ((dhd->sssr_reg_info->rev2.length >
		 OFFSETOF(sssr_reg_info_v2_t, dig_mem_info)) &&
		 dhd->sssr_reg_info->rev2.dig_mem_info.dig_sr_addr) {
			alloc_sssr = TRUE;
			sr_size = dhd->sssr_reg_info->rev2.dig_mem_info.dig_sr_size;
		}
		break;
	case SSSR_REG_INFO_VER_1:
		if (dhd->sssr_reg_info->rev1.vasip_regs.vasip_sr_size) {
			alloc_sssr = TRUE;
			sr_size = dhd->sssr_reg_info->rev1.vasip_regs.vasip_sr_size;
		} else if ((dhd->sssr_reg_info->rev1.length > OFFSETOF(sssr_reg_info_v1_t,
			dig_mem_info)) && dhd->sssr_reg_info->rev1.
			dig_mem_info.dig_sr_addr) {
			alloc_sssr = TRUE;
			sr_size = dhd->sssr_reg_info->rev1.dig_mem_info.dig_sr_size;
		}
		break;
	case SSSR_REG_INFO_VER_0:
		if (dhd->sssr_reg_info->rev0.vasip_regs.vasip_sr_size) {
			alloc_sssr = TRUE;
			sr_size = dhd->sssr_reg_info->rev0.vasip_regs.vasip_sr_size;
		}
		break;
	default:
		DHD_ERROR(("invalid sssr_reg_ver"));
		return BCME_UNSUPPORTED;
	}

	if (alloc_sssr) {
		dhd->sssr_dig_buf_after = (uint32 *)(dhd->sssr_mempool + mempool_used);
		mempool_used += sr_size;

#ifdef DHD_SSSR_DUMP_BEFORE_SR
		/* DIG dump before suspend is not applicable. */
		dhd->sssr_dig_buf_before = (uint32 *)(dhd->sssr_mempool + mempool_used);
		mempool_used += sr_size;
#endif /* DHD_SSSR_DUMP_BEFORE_SR */
	}

	/* Allocate dump memory for SAQM */
	sr_size = 0;
	supported_vers[0] = SSSR_REG_INFO_VER_5;
	supported_vers[1] = SSSR_REG_INFO_VER_6;
	supported_vers[2] = -1;
	ret = dhd_sssr_chk_version_support(dhd->sssr_reg_info->rev2.version, supported_vers);
	if (ret == BCME_ERROR) {
		DHD_ERROR(("%s:invalid sssr_reg_ver (%d), during saqm mem init\n", __FUNCTION__,
			dhd->sssr_reg_info->rev2.version));
		return BCME_UNSUPPORTED;
	} else if (ret == BCME_OK &&
			dhd->sssr_reg_info->rev5.saqm_sssr_info.saqm_sssr_size > 0) {
		dhd->sssr_saqm_buf_after = (uint32 *)(dhd->sssr_mempool + mempool_used);
		sr_size =
			dhd->sssr_reg_info->rev5.saqm_sssr_info.saqm_sssr_size;
		mempool_used += sr_size;
		DHD_PRINT(("%s: saqm mem init size=%u\n", __func__, sr_size));
#ifdef DHD_SSSR_DUMP_BEFORE_SR
		/* DIG dump before suspend is not applicable. */
		dhd->sssr_saqm_buf_before = (uint32 *)(dhd->sssr_mempool + mempool_used);
		mempool_used += sr_size;
#endif /* DHD_SSSR_DUMP_BEFORE_SR */

	}

	/* Allocate dump memory for SRCB */
	sr_size = 0;
	supported_vers[0] = SSSR_REG_INFO_VER_6;
	supported_vers[1] = -1;
	ret = dhd_sssr_chk_version_support(dhd->sssr_reg_info->rev2.version, supported_vers);
	if (ret == BCME_ERROR) {
		DHD_ERROR(("%s: sssr_reg_ver (%d) does not suppport SRCB FIS dump,"
				" during srcb mem init\n", __FUNCTION__,
				dhd->sssr_reg_info->rev2.version));
	} else if (ret == BCME_OK &&
			dhd->sssr_reg_info->rev6.srcb_mem_info.srcb_sssr_size > 0) {
		dhd->sssr_srcb_buf_after = (uint32 *)(dhd->sssr_mempool + mempool_used);
		sr_size =
			dhd->sssr_reg_info->rev6.srcb_mem_info.srcb_sssr_size;
		mempool_used += sr_size;
		DHD_PRINT(("%s: srcb mem init size=%u\n", __func__, sr_size));
	}

	/* Allocate dump memory for CMN */
	sr_size = 0;
	supported_vers[0] = SSSR_REG_INFO_VER_5;
	supported_vers[1] = SSSR_REG_INFO_VER_6;
	supported_vers[2] = -1;
	ret = dhd_sssr_chk_version_support(dhd->sssr_reg_info->rev2.version, supported_vers);
	if (ret == BCME_ERROR) {
		DHD_ERROR(("%s: sssr_reg_ver (%d) does not support cmn FIS dump,"
				" during cmn mem init\n", __FUNCTION__,
				dhd->sssr_reg_info->rev2.version));
	} else if (ret == BCME_OK &&
			dhd->sssr_reg_info->rev5.fis_mem_info.fis_size > 0) {
		dhd->sssr_cmn_buf_after = (uint32 *)(dhd->sssr_mempool + mempool_used);
		sr_size =
			dhd->sssr_reg_info->rev5.fis_mem_info.fis_size;
		mempool_used += sr_size;
		DHD_PRINT(("%s: cmn mem init size=%u\n", __func__, sr_size));
	}

	dhd->sssr_inited = TRUE;
	DHD_PRINT(("%s mempool_used:%d size:%d\n",
		__FUNCTION__, mempool_used, DHD_SSSR_MEMPOOL_SIZE));
	ASSERT(mempool_used <= DHD_SSSR_MEMPOOL_SIZE);

	return BCME_OK;

}

void
dhd_sssr_dump_deinit(dhd_pub_t *dhd)
{
	int i;

	dhd->sssr_inited = FALSE;
	/* init all pointers to NULL */
	for (i = 0; i < MAX_NUM_D11_CORES_WITH_SCAN; i++) {
#ifdef DHD_SSSR_DUMP_BEFORE_SR
		dhd->sssr_d11_before[i] = NULL;
#endif /* DHD_SSSR_DUMP_BEFORE_SR */
		dhd->sssr_d11_after[i] = NULL;
	}
#ifdef DHD_SSSR_DUMP_BEFORE_SR
	dhd->sssr_dig_buf_before = NULL;
#endif /* DHD_SSSR_DUMP_BEFORE_SR */
	dhd->sssr_dig_buf_after = NULL;

	return;
}

void
dhd_sssr_print_filepath(dhd_pub_t *dhd, char *path)
{
	bool print_info = FALSE;
	int dump_mode;

	if (!dhd || !path) {
		DHD_ERROR(("%s: dhd or memdump_path is NULL\n",
			__FUNCTION__));
		return;
	}

	if (!dhd->sssr_dump_collected) {
		/* SSSR dump is not collected */
		return;
	}

	dump_mode = dhd->sssr_dump_mode;

	if (bcmstrstr(path, "core_0_before")) {
		if (dhd->sssr_d11_outofreset[0] &&
			dump_mode == SSSR_DUMP_MODE_SSSR) {
			print_info = TRUE;
		}
	} else if (bcmstrstr(path, "core_0_after")) {
		if (dhd->sssr_d11_outofreset[0]) {
			print_info = TRUE;
		}
	} else if (bcmstrstr(path, "core_1_before")) {
		if (dhd->sssr_d11_outofreset[1] &&
			dump_mode == SSSR_DUMP_MODE_SSSR) {
			print_info = TRUE;
		}
	} else if (bcmstrstr(path, "core_1_after")) {
		if (dhd->sssr_d11_outofreset[1]) {
			print_info = TRUE;
		}
	} else if (bcmstrstr(path, "core_2_before")) {
		if (dhd->sssr_d11_outofreset[2] &&
			dump_mode == SSSR_DUMP_MODE_SSSR) {
			print_info = TRUE;
		}
	} else if (bcmstrstr(path, "core_2_after")) {
		if (dhd->sssr_d11_outofreset[2]) {
			print_info = TRUE;
		}
	} else {
		print_info = TRUE;
	}

	if (print_info) {
		DHD_ERROR(("%s: file_path = %s%s\n", __FUNCTION__,
			path, FILE_NAME_HAL_TAG));
	}
}

#ifdef DHD_COREDUMP
int dhd_append_sssr_tlv(uint8 *buf_dst, int type_idx, int buf_remain)
{
	uint32 type_val, length_val;
	uint32 *type, *length;
	void *buf_src;
	int total_size = 0, ret = 0;

	/* DHD_COREDUMP_TYPE_SSSRDUMP_[CORE[0|1|2]|DIG]_[BEFORE|AFTER] */
	type_val = dhd_coredump_types[type_idx].type;
	length_val = dhd_coredump_types[type_idx].length;

	if (length_val == 0) {
		return 0;
	}

	type = (uint32 *)buf_dst;
	*type = type_val;
	length = (uint32 *)(buf_dst + sizeof(*type));
	*length = length_val;

	buf_dst += TLV_TYPE_LENGTH_SIZE;
	total_size += TLV_TYPE_LENGTH_SIZE;

	buf_src = dhd_coredump_types[type_idx].bufptr;
	ret = memcpy_s(buf_dst, buf_remain, buf_src, *length);

	if (ret) {
		DHD_ERROR(("Failed to memcpy_s() for coredump.\n"));
		return BCME_ERROR;
	}

	DHD_INFO(("%s: type: %u, length: %u\n",	__FUNCTION__, *type, *length));

	total_size += *length;
	return total_size;
}
#endif /* DHD_COREDUMP */

uint
dhd_sssr_dig_buf_size(dhd_pub_t *dhdp)
{
	uint dig_buf_size = 0;

	/* SSSR register information structure v0 and v1 shares most except dig_mem */
	switch (dhdp->sssr_reg_info->rev2.version) {
	case SSSR_REG_INFO_VER_6:
		if ((dhdp->sssr_reg_info->rev6.length >
		 OFFSETOF(sssr_reg_info_v6_t, sssr_all_mem_info)) &&
		 dhdp->sssr_reg_info->rev6.sssr_all_mem_info.sysmem_sssr_size) {
			dig_buf_size =
			dhdp->sssr_reg_info->rev6.sssr_all_mem_info.sysmem_sssr_size;
		}
		break;
	case SSSR_REG_INFO_VER_5:
		if ((dhdp->sssr_reg_info->rev5.length >
		 OFFSETOF(sssr_reg_info_v5_t, sssr_all_mem_info)) &&
		 dhdp->sssr_reg_info->rev5.sssr_all_mem_info.sysmem_sssr_size) {
			dig_buf_size =
			dhdp->sssr_reg_info->rev5.sssr_all_mem_info.sysmem_sssr_size;
		}
		break;
	case SSSR_REG_INFO_VER_4:
		/* for v4 need to use sssr_all_mem_info instead of dig_mem_info */
		if ((dhdp->sssr_reg_info->rev4.length >
		 OFFSETOF(sssr_reg_info_v4_t, sssr_all_mem_info)) &&
		 dhdp->sssr_reg_info->rev4.sssr_all_mem_info.sysmem_sssr_size) {
			dig_buf_size =
			dhdp->sssr_reg_info->rev4.sssr_all_mem_info.sysmem_sssr_size;
		}
		break;
	case SSSR_REG_INFO_VER_3:
		/* intentional fall through */
	case SSSR_REG_INFO_VER_2:
		if ((dhdp->sssr_reg_info->rev2.length >
		 OFFSETOF(sssr_reg_info_v2_t, dig_mem_info)) &&
		 dhdp->sssr_reg_info->rev2.dig_mem_info.dig_sr_size) {
			dig_buf_size = dhdp->sssr_reg_info->rev2.dig_mem_info.dig_sr_size;
		}
		break;
	case SSSR_REG_INFO_VER_1:
		if (dhdp->sssr_reg_info->rev1.vasip_regs.vasip_sr_size) {
			dig_buf_size = dhdp->sssr_reg_info->rev1.vasip_regs.vasip_sr_size;
		} else if ((dhdp->sssr_reg_info->rev1.length >
		 OFFSETOF(sssr_reg_info_v1_t, dig_mem_info)) &&
		 dhdp->sssr_reg_info->rev1.dig_mem_info.dig_sr_size) {
			dig_buf_size = dhdp->sssr_reg_info->rev1.dig_mem_info.dig_sr_size;
		}
		break;
	case SSSR_REG_INFO_VER_0:
		if (dhdp->sssr_reg_info->rev0.vasip_regs.vasip_sr_size) {
			dig_buf_size = dhdp->sssr_reg_info->rev0.vasip_regs.vasip_sr_size;
		}
		break;
	default:
		DHD_ERROR(("invalid sssr_reg_ver"));
		return BCME_UNSUPPORTED;
	}

	return dig_buf_size;
}

uint
dhd_sssr_dig_buf_addr(dhd_pub_t *dhdp)
{
	uint dig_buf_addr = 0;

	/* SSSR register information structure v0 and v1 shares most except dig_mem */
	switch (dhdp->sssr_reg_info->rev2.version) {
	case SSSR_REG_INFO_VER_6:
		if ((dhdp->sssr_reg_info->rev6.length >
		 OFFSETOF(sssr_reg_info_v6_t, sssr_all_mem_info)) &&
		 dhdp->sssr_reg_info->rev6.sssr_all_mem_info.sysmem_sssr_size) {
			dig_buf_addr =
			dhdp->sssr_reg_info->rev6.sssr_all_mem_info.sysmem_sssr_addr;
		}
		break;
	case SSSR_REG_INFO_VER_5:
		if ((dhdp->sssr_reg_info->rev5.length >
		 OFFSETOF(sssr_reg_info_v5_t, sssr_all_mem_info)) &&
		 dhdp->sssr_reg_info->rev5.sssr_all_mem_info.sysmem_sssr_size) {
			dig_buf_addr =
			dhdp->sssr_reg_info->rev5.sssr_all_mem_info.sysmem_sssr_addr;
		}
		break;
	case SSSR_REG_INFO_VER_4:
		/* for v4 need to use sssr_all_mem_info instead of dig_mem_info */
		if ((dhdp->sssr_reg_info->rev4.length >
		 OFFSETOF(sssr_reg_info_v4_t, sssr_all_mem_info)) &&
		 dhdp->sssr_reg_info->rev4.sssr_all_mem_info.sysmem_sssr_size) {
			dig_buf_addr =
			dhdp->sssr_reg_info->rev4.sssr_all_mem_info.sysmem_sssr_addr;
		}
		break;
	case SSSR_REG_INFO_VER_3:
		/* intentional fall through */
	case SSSR_REG_INFO_VER_2:
		if ((dhdp->sssr_reg_info->rev2.length >
		 OFFSETOF(sssr_reg_info_v2_t, dig_mem_info)) &&
		 dhdp->sssr_reg_info->rev2.dig_mem_info.dig_sr_size) {
			dig_buf_addr = dhdp->sssr_reg_info->rev2.dig_mem_info.dig_sr_addr;
		}
		break;
	case SSSR_REG_INFO_VER_1:
		if (dhdp->sssr_reg_info->rev1.vasip_regs.vasip_sr_size) {
			dig_buf_addr = dhdp->sssr_reg_info->rev1.vasip_regs.vasip_sr_addr;
		} else if ((dhdp->sssr_reg_info->rev1.length >
		 OFFSETOF(sssr_reg_info_v1_t, dig_mem_info)) &&
		 dhdp->sssr_reg_info->rev1.dig_mem_info.dig_sr_size) {
			dig_buf_addr = dhdp->sssr_reg_info->rev1.dig_mem_info.dig_sr_addr;
		}
		break;
	case SSSR_REG_INFO_VER_0:
		if (dhdp->sssr_reg_info->rev0.vasip_regs.vasip_sr_size) {
			dig_buf_addr = dhdp->sssr_reg_info->rev0.vasip_regs.vasip_sr_addr;
		}
		break;
	default:
		DHD_ERROR(("invalid sssr_reg_ver"));
		return BCME_UNSUPPORTED;
	}

	return dig_buf_addr;
}

uint
dhd_sssr_mac_buf_size(dhd_pub_t *dhdp, uint8 core_idx)
{
	uint mac_buf_size = 0;
	uint8 num_d11cores;

	num_d11cores = dhd_d11_slices_num_get(dhdp);

	/* SSSR register information structure v0 and v1 shares most except dig_mem */
	if (core_idx < num_d11cores) {
		switch (dhdp->sssr_reg_info->rev2.version) {
		case SSSR_REG_INFO_VER_6:
		case SSSR_REG_INFO_VER_5:
			mac_buf_size = dhdp->sssr_reg_info->rev5.mac_regs[core_idx].sr_size;
			break;
		case SSSR_REG_INFO_VER_4:
			mac_buf_size = dhdp->sssr_reg_info->rev4.mac_regs[core_idx].sr_size;
			break;
		case SSSR_REG_INFO_VER_3:
			/* intentional fall through */
		case SSSR_REG_INFO_VER_2:
			mac_buf_size = dhdp->sssr_reg_info->rev2.mac_regs[core_idx].sr_size;
			break;
		case SSSR_REG_INFO_VER_1:
			mac_buf_size = dhdp->sssr_reg_info->rev1.mac_regs[core_idx].sr_size;
			break;
		case SSSR_REG_INFO_VER_0:
			mac_buf_size = dhdp->sssr_reg_info->rev0.mac_regs[core_idx].sr_size;
			break;
		default:
			DHD_ERROR(("invalid sssr_reg_ver"));
			return BCME_UNSUPPORTED;
		}
	}

	return mac_buf_size;
}

uint
dhd_sssr_mac_xmtaddress(dhd_pub_t *dhdp, uint8 core_idx)
{
	uint xmtaddress = 0;
	uint8 num_d11cores;

	num_d11cores = dhd_d11_slices_num_get(dhdp);

	/* SSSR register information structure v0 and v1 shares most except dig_mem */
	if (core_idx < num_d11cores) {
		switch (dhdp->sssr_reg_info->rev2.version) {
		case SSSR_REG_INFO_VER_6:
		case SSSR_REG_INFO_VER_5:
			xmtaddress = dhdp->sssr_reg_info->rev5.
				mac_regs[core_idx].base_regs.xmtaddress;
			break;
		case SSSR_REG_INFO_VER_4:
			xmtaddress = dhdp->sssr_reg_info->rev4.
				mac_regs[core_idx].base_regs.xmtaddress;
			break;
		case SSSR_REG_INFO_VER_3:
			/* intentional fall through */
		case SSSR_REG_INFO_VER_2:
			xmtaddress = dhdp->sssr_reg_info->rev2.
				mac_regs[core_idx].base_regs.xmtaddress;
			break;
		case SSSR_REG_INFO_VER_1:
			xmtaddress = dhdp->sssr_reg_info->rev1.
				mac_regs[core_idx].base_regs.xmtaddress;
			break;
		case SSSR_REG_INFO_VER_0:
			xmtaddress = dhdp->sssr_reg_info->rev0.
				mac_regs[core_idx].base_regs.xmtaddress;
			break;
		default:
			DHD_ERROR(("invalid sssr_reg_ver"));
			return BCME_UNSUPPORTED;
		}
	}

	return xmtaddress;
}

uint
dhd_sssr_mac_xmtdata(dhd_pub_t *dhdp, uint8 core_idx)
{
	uint xmtdata = 0;
	uint8 num_d11cores;

	num_d11cores = dhd_d11_slices_num_get(dhdp);

	/* SSSR register information structure v0 and v1 shares most except dig_mem */
	if (core_idx < num_d11cores) {
		switch (dhdp->sssr_reg_info->rev2.version) {
		case SSSR_REG_INFO_VER_6:
		case SSSR_REG_INFO_VER_5:
			xmtdata = dhdp->sssr_reg_info->rev5.
				mac_regs[core_idx].base_regs.xmtdata;
			break;
		case SSSR_REG_INFO_VER_4:
			xmtdata = dhdp->sssr_reg_info->rev4.
				mac_regs[core_idx].base_regs.xmtdata;
			break;
		case SSSR_REG_INFO_VER_3:
			/* intentional fall through */
		case SSSR_REG_INFO_VER_2:
			xmtdata = dhdp->sssr_reg_info->rev2.
				mac_regs[core_idx].base_regs.xmtdata;
			break;
		case SSSR_REG_INFO_VER_1:
			xmtdata = dhdp->sssr_reg_info->rev1.
				mac_regs[core_idx].base_regs.xmtdata;
			break;
		case SSSR_REG_INFO_VER_0:
			xmtdata = dhdp->sssr_reg_info->rev0.
				mac_regs[core_idx].base_regs.xmtdata;
			break;
		default:
			DHD_ERROR(("invalid sssr_reg_ver"));
			return BCME_UNSUPPORTED;
		}
	}

	return xmtdata;
}

int
dhd_sssr_sr_asm_version(dhd_pub_t *dhdp, uint16 *sr_asm_version)
{
	int supported_vers[SSSR_REG_INFO_VER_MAX] = {0};
	int ret = 0;
	supported_vers[0] = SSSR_REG_INFO_VER_5;
	supported_vers[1] = SSSR_REG_INFO_VER_6;
	supported_vers[2] = -1;
	ret = dhd_sssr_chk_version_support(dhdp->sssr_reg_info->rev2.version, supported_vers);
	if (ret == BCME_ERROR) {
		DHD_ERROR(("%s:invalid sssr_reg_ver (%d)\n", __FUNCTION__,
			dhdp->sssr_reg_info->rev2.version));
		return BCME_UNSUPPORTED;
	} else if (ret == BCME_OK) {
		*sr_asm_version = dhdp->sssr_reg_info->rev5.sr_asm_version;
	}
	return BCME_OK;
}

int
dhd_sssr_mac_war_reg(dhd_pub_t *dhdp, uint8 core_idx, uint32 *war_reg)
{
	uint8 num_d11cores;
	int supported_vers[SSSR_REG_INFO_VER_MAX] = {0};
	int ret = 0;
	num_d11cores = dhd_d11_slices_num_get(dhdp);
	supported_vers[0] = SSSR_REG_INFO_VER_5;
	supported_vers[1] = SSSR_REG_INFO_VER_6;
	supported_vers[2] = -1;

	if (core_idx < num_d11cores) {
		ret = dhd_sssr_chk_version_support(dhdp->sssr_reg_info->rev2.version,
			supported_vers);
		if (ret == BCME_ERROR) {
			DHD_ERROR(("%s:invalid sssr_reg_ver (%d)\n", __FUNCTION__,
				dhdp->sssr_reg_info->rev2.version));
			return BCME_UNSUPPORTED;
		} else if (ret == BCME_OK) {
			*war_reg = dhdp->sssr_reg_info->rev5.mac_regs[core_idx].war_reg;
		}
	}

	return BCME_OK;
}

int
dhd_sssr_arm_war_reg(dhd_pub_t *dhdp, uint32 *war_reg)
{
	int supported_vers[SSSR_REG_INFO_VER_MAX] = {0};
	int ret = 0;
	supported_vers[0] = SSSR_REG_INFO_VER_5;
	supported_vers[1] = SSSR_REG_INFO_VER_6;
	supported_vers[2] = -1;
	ret = dhd_sssr_chk_version_support(dhdp->sssr_reg_info->rev2.version, supported_vers);
	if (ret == BCME_ERROR) {
		DHD_ERROR(("%s:invalid sssr_reg_ver (%d)\n", __FUNCTION__,
			dhdp->sssr_reg_info->rev2.version));
		return BCME_UNSUPPORTED;
	} else if (ret == BCME_OK) {
		*war_reg = dhdp->sssr_reg_info->rev5.arm_regs.war_reg;
	}
	return BCME_OK;
}

int
dhd_sssr_saqm_war_reg(dhd_pub_t *dhdp, uint32 *war_reg)
{
	int supported_vers[SSSR_REG_INFO_VER_MAX] = {0};
	int ret = 0;
	supported_vers[0] = SSSR_REG_INFO_VER_5;
	supported_vers[1] = SSSR_REG_INFO_VER_6;
	supported_vers[2] = -1;
	ret = dhd_sssr_chk_version_support(dhdp->sssr_reg_info->rev2.version, supported_vers);
	if (ret == BCME_ERROR) {
		DHD_ERROR(("%s:invalid sssr_reg_ver (%d)\n", __FUNCTION__,
			dhdp->sssr_reg_info->rev2.version));
		return BCME_UNSUPPORTED;
	} else if (ret == BCME_OK) {
		*war_reg = dhdp->sssr_reg_info->rev5.saqm_sssr_info.war_reg;
	}
	return BCME_OK;
}

int
dhd_sssr_srcb_war_reg(dhd_pub_t *dhdp, uint32 *war_reg)
{
	int supported_vers[SSSR_REG_INFO_VER_MAX] = {0};
	int ret = 0;
	supported_vers[0] = SSSR_REG_INFO_VER_6;
	supported_vers[1] = -1;
	ret = dhd_sssr_chk_version_support(dhdp->sssr_reg_info->rev2.version, supported_vers);
	if (ret == BCME_ERROR) {
		DHD_ERROR(("%s:invalid sssr_reg_ver (%d)\n", __FUNCTION__,
			dhdp->sssr_reg_info->rev2.version));
		return BCME_UNSUPPORTED;
	} else if (ret == BCME_OK) {
		if (dhdp->sssr_reg_info->rev6.srcb_mem_info.srcb_sssr_size > 0) {
			*war_reg = dhdp->sssr_reg_info->rev6.srcb_mem_info.war_reg;
		}
	}
	return BCME_OK;
}

uint
dhd_sssr_saqm_buf_size(dhd_pub_t *dhdp)
{
	uint saqm_buf_size = 0;
	int supported_vers[SSSR_REG_INFO_VER_MAX] = {0};
	int ret = 0;
	supported_vers[0] = SSSR_REG_INFO_VER_5;
	supported_vers[1] = SSSR_REG_INFO_VER_6;
	supported_vers[2] = -1;
	ret = dhd_sssr_chk_version_support(dhdp->sssr_reg_info->rev2.version, supported_vers);
	if (ret == BCME_ERROR) {
		DHD_ERROR(("%s:invalid sssr_reg_ver (%d)\n", __FUNCTION__,
			dhdp->sssr_reg_info->rev2.version));
		return 0;
	} else if (ret == BCME_OK) {
		if (dhdp->sssr_reg_info->rev5.saqm_sssr_info.saqm_sssr_size > 0) {
			saqm_buf_size =
				dhdp->sssr_reg_info->rev5.saqm_sssr_info.saqm_sssr_size;
		}
	}
	return saqm_buf_size;
}

uint
dhd_sssr_saqm_buf_addr(dhd_pub_t *dhdp)
{
	uint saqm_buf_addr = 0;
	int supported_vers[SSSR_REG_INFO_VER_MAX] = {0};
	int ret = 0;
	supported_vers[0] = SSSR_REG_INFO_VER_5;
	supported_vers[1] = SSSR_REG_INFO_VER_6;
	supported_vers[2] = -1;
	ret = dhd_sssr_chk_version_support(dhdp->sssr_reg_info->rev2.version, supported_vers);
	if (ret == BCME_ERROR) {
		DHD_ERROR(("%s:invalid sssr_reg_ver (%d)\n", __FUNCTION__,
			dhdp->sssr_reg_info->rev2.version));
		return 0;
	} else if (ret == BCME_OK) {
		if (dhdp->sssr_reg_info->rev5.saqm_sssr_info.saqm_sssr_size > 0) {
			saqm_buf_addr =
				dhdp->sssr_reg_info->rev5.saqm_sssr_info.saqm_sssr_addr;
		}
	}

	return saqm_buf_addr;
}

uint
dhd_sssr_srcb_buf_size(dhd_pub_t *dhdp)
{
	uint srcb_buf_size = 0;
	int supported_vers[SSSR_REG_INFO_VER_MAX] = {0};
	int ret = 0;
	supported_vers[0] = SSSR_REG_INFO_VER_6;
	supported_vers[1] = -1;
	ret = dhd_sssr_chk_version_support(dhdp->sssr_reg_info->rev2.version, supported_vers);
	if (ret == BCME_ERROR) {
		DHD_ERROR(("%s:sssr_reg_ver (%d) does not support SRCB FIS dump\n", __FUNCTION__,
			dhdp->sssr_reg_info->rev2.version));
		return 0;
	} else if (ret == BCME_OK) {
		if (dhdp->sssr_reg_info->rev6.srcb_mem_info.srcb_sssr_size > 0) {
			srcb_buf_size =
				dhdp->sssr_reg_info->rev6.srcb_mem_info.srcb_sssr_size;
		}
	}
	return srcb_buf_size;
}

uint
dhd_sssr_srcb_buf_addr(dhd_pub_t *dhdp)
{
	uint srcb_buf_addr = 0;
	int supported_vers[SSSR_REG_INFO_VER_MAX] = {0};
	int ret = 0;
	supported_vers[0] = SSSR_REG_INFO_VER_6;
	supported_vers[1] = -1;
	ret = dhd_sssr_chk_version_support(dhdp->sssr_reg_info->rev2.version, supported_vers);
	if (ret == BCME_ERROR) {
		DHD_ERROR(("%s: sssr_reg_ver (%d) does not support SRCB FIS \n", __FUNCTION__,
			dhdp->sssr_reg_info->rev2.version));
		return 0;
	} else if (ret == BCME_OK) {
		if (dhdp->sssr_reg_info->rev6.srcb_mem_info.srcb_sssr_size > 0) {
			srcb_buf_addr =
				dhdp->sssr_reg_info->rev6.srcb_mem_info.srcb_sssr_addr;
		}
	}

	return srcb_buf_addr;
}

uint
dhd_sssr_cmn_buf_size(dhd_pub_t *dhdp)
{
	uint cmn_buf_size = 0;
	int supported_vers[SSSR_REG_INFO_VER_MAX] = {0};
	int ret = 0;
	supported_vers[0] = SSSR_REG_INFO_VER_5;
	supported_vers[1] = SSSR_REG_INFO_VER_6;
	supported_vers[2] = -1;
	ret = dhd_sssr_chk_version_support(dhdp->sssr_reg_info->rev2.version, supported_vers);
	if (ret == BCME_ERROR) {
		DHD_ERROR(("%s:invalid sssr_reg_ver (%d)\n", __FUNCTION__,
			dhdp->sssr_reg_info->rev2.version));
		return 0;
	} else if (ret == BCME_OK) {
		if (dhdp->sssr_reg_info->rev5.fis_mem_info.fis_size > 0 &&
			dhdp->sssr_reg_info->rev5.fis_mem_info.fis_size != (uint32)-1) {
			cmn_buf_size =
				dhdp->sssr_reg_info->rev5.fis_mem_info.fis_size;
		} else {
			DHD_ERROR(("%s:invalid cmn buf size %u !\n", __FUNCTION__,
				dhdp->sssr_reg_info->rev5.fis_mem_info.fis_size));
		}
	}
	return cmn_buf_size;
}

uint
dhd_sssr_cmn_buf_addr(dhd_pub_t *dhdp)
{
	uint cmn_buf_addr = 0;
	int supported_vers[SSSR_REG_INFO_VER_MAX] = {0};
	int ret = 0;
	supported_vers[0] = SSSR_REG_INFO_VER_5;
	supported_vers[1] = SSSR_REG_INFO_VER_6;
	supported_vers[2] = -1;
	ret = dhd_sssr_chk_version_support(dhdp->sssr_reg_info->rev2.version, supported_vers);
	if (ret == BCME_ERROR) {
		DHD_ERROR(("%s:invalid sssr_reg_ver (%d)\n", __FUNCTION__,
			dhdp->sssr_reg_info->rev2.version));
		return 0;
	} else if (ret == BCME_OK) {
		if (dhdp->sssr_reg_info->rev5.fis_mem_info.fis_size > 0 &&
			dhdp->sssr_reg_info->rev5.fis_mem_info.fis_addr &&
			dhdp->sssr_reg_info->rev5.fis_mem_info.fis_addr != (uint32)-1) {
			cmn_buf_addr =
				dhdp->sssr_reg_info->rev5.fis_mem_info.fis_addr;
		} else {
			DHD_ERROR(("%s:invalid cmn buf addr %x !\n", __FUNCTION__,
				dhdp->sssr_reg_info->rev5.fis_mem_info.fis_addr));
		}
	}

	return cmn_buf_addr;
}

#ifdef DHD_SSSR_DUMP_BEFORE_SR
int
dhd_sssr_dump_dig_buf_before(void *dhd_pub, const void *user_buf, uint32 len)
{
	dhd_pub_t *dhdp = (dhd_pub_t *)dhd_pub;
	int pos = 0, ret = BCME_ERROR;
	uint dig_buf_size = 0;

	dig_buf_size = dhd_sssr_dig_buf_size(dhdp);

	if (dhdp->sssr_dig_buf_before && (dhdp->sssr_dump_mode == SSSR_DUMP_MODE_SSSR)) {
		ret = dhd_export_debug_data((char *)dhdp->sssr_dig_buf_before,
			NULL, user_buf, dig_buf_size, &pos);
	}
	return ret;
}

int
dhd_sssr_dump_d11_buf_before(void *dhd_pub, const void *user_buf, uint32 len, int core)
{
	dhd_pub_t *dhdp = (dhd_pub_t *)dhd_pub;
	int pos = 0, ret = BCME_ERROR;

	if (dhdp->sssr_d11_before[core] &&
		dhdp->sssr_d11_outofreset[core] &&
		(dhdp->sssr_dump_mode == SSSR_DUMP_MODE_SSSR)) {
		ret = dhd_export_debug_data((char *)dhdp->sssr_d11_before[core],
			NULL, user_buf, len, &pos);
	}
	return ret;
}
#endif /* DHD_SSSR_DUMP_BEFORE_SR */

int
dhd_sssr_dump_dig_buf_after(void *dhd_pub, const void *user_buf, uint32 len)
{
	dhd_pub_t *dhdp = (dhd_pub_t *)dhd_pub;
	int pos = 0, ret = BCME_ERROR;
	uint dig_buf_size = 0;

	dig_buf_size = dhd_sssr_dig_buf_size(dhdp);

	if (dhdp->sssr_dig_buf_after) {
		ret = dhd_export_debug_data((char *)dhdp->sssr_dig_buf_after,
			NULL, user_buf, dig_buf_size, &pos);
	}
	return ret;
}

int
dhd_sssr_dump_d11_buf_after(void *dhd_pub, const void *user_buf, uint32 len, int core)
{
	dhd_pub_t *dhdp = (dhd_pub_t *)dhd_pub;
	int pos = 0, ret = BCME_ERROR;

	if (dhdp->sssr_d11_after[core] &&
		dhdp->sssr_d11_outofreset[core]) {
		ret = dhd_export_debug_data((char *)dhdp->sssr_d11_after[core],
			NULL, user_buf, len, &pos);
	}
	return ret;
}

#if defined(DHD_DUMP_FILE_WRITE_FROM_KERNEL)
static void
dhd_sssr_dump_to_file(dhd_pub_t *dhdp)
{
	int i;
#ifdef DHD_SSSR_DUMP_BEFORE_SR
	char before_sr_dump[128];
#endif /* DHD_SSSR_DUMP_BEFORE_SR */
	char after_sr_dump[128];
	unsigned long flags = 0;
	uint dig_buf_size = 0;
	uint8 num_d11cores = 0;
	uint d11_buf_size = 0;
	uint saqm_buf_size = 0;
	uint srcb_buf_size = 0;
	uint cmn_buf_size = 0;

	DHD_PRINT(("%s: ENTER \n", __FUNCTION__));

	if (!dhdp) {
		DHD_ERROR(("%s: dhdp is NULL\n", __FUNCTION__));
		return;
	}

	DHD_GENERAL_LOCK(dhdp, flags);
	DHD_BUS_BUSY_SET_IN_SSSRDUMP(dhdp);
	if (DHD_BUS_CHECK_DOWN_OR_DOWN_IN_PROGRESS(dhdp)) {
		DHD_GENERAL_UNLOCK(dhdp, flags);
		DHD_ERROR(("%s: bus is down! can't collect sssr dump. \n", __FUNCTION__));
		goto exit;
	}
	DHD_GENERAL_UNLOCK(dhdp, flags);

	num_d11cores = dhd_d11_slices_num_get(dhdp);

	for (i = 0; i < num_d11cores; i++) {
		/* Init file name */
#ifdef DHD_SSSR_DUMP_BEFORE_SR
		bzero(before_sr_dump, sizeof(before_sr_dump));
#endif /* DHD_SSSR_DUMP_BEFORE_SR */
		bzero(after_sr_dump, sizeof(after_sr_dump));

#ifdef DHD_SSSR_DUMP_BEFORE_SR
		snprintf(before_sr_dump, sizeof(before_sr_dump), "%s_%d_%s",
			"sssr_dump_core", i, "before_SR");
#endif /* DHD_SSSR_DUMP_BEFORE_SR */
		if (dhdp->sssr_dump_mode == SSSR_DUMP_MODE_FIS) {
			snprintf(after_sr_dump, sizeof(after_sr_dump), "%s_%d_%s",
				"sssr_dump_fis_core", i, "after_SR");
		} else {
			snprintf(after_sr_dump, sizeof(after_sr_dump), "%s_%d_%s",
				"sssr_dump_core", i, "after_SR");
		}

		d11_buf_size = dhd_sssr_mac_buf_size(dhdp, i);

#ifdef DHD_SSSR_DUMP_BEFORE_SR
		if (dhdp->sssr_d11_before[i] && dhdp->sssr_d11_outofreset[i] &&
			(dhdp->sssr_dump_mode == SSSR_DUMP_MODE_SSSR)) {
			if (write_dump_to_file(dhdp, (uint8 *)dhdp->sssr_d11_before[i],
					d11_buf_size, before_sr_dump)) {
				DHD_ERROR(("%s: writing SSSR MAIN dump before to the file failed\n",
						__FUNCTION__));
			}
		}
#endif /* DHD_SSSR_DUMP_BEFORE_SR */

		if (dhdp->sssr_d11_after[i] && dhdp->sssr_d11_outofreset[i]) {
			if (write_dump_to_file(dhdp, (uint8 *)dhdp->sssr_d11_after[i],
					d11_buf_size, after_sr_dump)) {
				DHD_ERROR(("%s: writing SSSR AUX dump after to the file failed\n",
						__FUNCTION__));
			}
		}
	}

	dig_buf_size = dhd_sssr_dig_buf_size(dhdp);

#ifdef DHD_SSSR_DUMP_BEFORE_SR
	if (dhdp->sssr_dig_buf_before && (dhdp->sssr_dump_mode == SSSR_DUMP_MODE_SSSR)) {
		if (write_dump_to_file(dhdp, (uint8 *)dhdp->sssr_dig_buf_before,
				dig_buf_size, "sssr_dump_dig_before_SR")) {
			DHD_ERROR(("%s: writing SSSR Dig dump before to the file failed\n",
					__FUNCTION__));
		}
	}
#endif /* DHD_SSSR_DUMP_BEFORE_SR */

	bzero(after_sr_dump, sizeof(after_sr_dump));
	if (dhdp->sssr_dump_mode == SSSR_DUMP_MODE_FIS) {
		snprintf(after_sr_dump, sizeof(after_sr_dump), "%s_%s",
			"sssr_dump_fis_dig", "after_SR");
	} else {
		snprintf(after_sr_dump, sizeof(after_sr_dump), "%s_%s",
			"sssr_dump_dig", "after_SR");
	}

	if (dhdp->sssr_dig_buf_after) {
		if (write_dump_to_file(dhdp, (uint8 *)dhdp->sssr_dig_buf_after,
				dig_buf_size, after_sr_dump)) {
			DHD_ERROR(("%s: writing SSSR Dig VASIP dump after to the file failed\n",
					__FUNCTION__));
		}
	}

	saqm_buf_size = dhd_sssr_saqm_buf_size(dhdp);

#ifdef DHD_SSSR_DUMP_BEFORE_SR
	if ((saqm_buf_size > 0) && dhdp->sssr_saqm_buf_before &&
	 (dhdp->sssr_dump_mode == SSSR_DUMP_MODE_SSSR)) {
		if (write_dump_to_file(dhdp, (uint8 *)dhdp->sssr_saqm_buf_before,
				saqm_buf_size, "sssr_dump_saqm_before_SR")) {
			DHD_ERROR(("%s: writing SSSR SAQM dump before to the file failed\n",
					__FUNCTION__));
		}
	}
#endif /* DHD_SSSR_DUMP_BEFORE_SR */

	bzero(after_sr_dump, sizeof(after_sr_dump));
	if (dhdp->sssr_dump_mode == SSSR_DUMP_MODE_FIS) {
		snprintf(after_sr_dump, sizeof(after_sr_dump), "%s_%s",
			"sssr_dump_fis_saqm", "after_SR");
	} else {
		snprintf(after_sr_dump, sizeof(after_sr_dump), "%s_%s",
			"sssr_dump_saqm", "after_SR");
	}

	if ((saqm_buf_size > 0) && dhdp->sssr_saqm_buf_after) {
		if (write_dump_to_file(dhdp, (uint8 *)dhdp->sssr_saqm_buf_after,
				saqm_buf_size, after_sr_dump)) {
			DHD_ERROR(("%s: writing SSSR SAQM dump after to the file failed\n",
					__FUNCTION__));
		}
	}

	if (dhdp->sssr_dump_mode == SSSR_DUMP_MODE_FIS) {
		srcb_buf_size = dhd_sssr_srcb_buf_size(dhdp);

		if ((srcb_buf_size > 0) && dhdp->sssr_srcb_buf_after) {
			bzero(after_sr_dump, sizeof(after_sr_dump));
			snprintf(after_sr_dump, sizeof(after_sr_dump), "%s_%s",
					"sssr_dump_fis_srcb", "after_SR");

			if (write_dump_to_file(dhdp, (uint8 *)dhdp->sssr_srcb_buf_after,
					srcb_buf_size, after_sr_dump)) {
				DHD_ERROR(("%s: writing FIS SRCB dump after to the file failed\n",
						__FUNCTION__));
			}
		}

		cmn_buf_size = dhd_sssr_cmn_buf_size(dhdp);

		if ((cmn_buf_size > 0) && dhdp->sssr_cmn_buf_after) {
			bzero(after_sr_dump, sizeof(after_sr_dump));
			snprintf(after_sr_dump, sizeof(after_sr_dump), "%s_%s",
					"sssr_dump_fis_cmn", "after_SR");

			if (write_dump_to_file(dhdp, (uint8 *)dhdp->sssr_cmn_buf_after,
					cmn_buf_size, after_sr_dump)) {
				DHD_ERROR(("%s: writing FIS CMN dump after to the file failed\n",
						__FUNCTION__));
			}
		}
	}

exit:
	DHD_GENERAL_LOCK(dhdp, flags);
	DHD_BUS_BUSY_CLEAR_IN_SSSRDUMP(dhdp);
	dhd_os_busbusy_wake(dhdp);
	DHD_GENERAL_UNLOCK(dhdp, flags);
}
#endif /* DHD_DUMP_FILE_WRITE_FROM_KERNEL */

void
dhd_write_sssr_dump(dhd_pub_t *dhdp, uint32 dump_mode)
{
#if defined(DHD_DUMP_FILE_WRITE_FROM_KERNEL)
#endif
	dhdp->sssr_dump_mode = dump_mode;

	/*
	 * If kernel does not have file write access enabled
	 * then skip writing dumps to files.
	 * The dumps will be pushed to HAL layer which will
	 * write into files
	 */
#if !defined(DHD_DUMP_FILE_WRITE_FROM_KERNEL)
	return;
#else
	/*
	 * dhd_mem_dump -> dhd_sssr_dump -> dhd_write_sssr_dump
	 * Without workqueue -
	 * DUMP_TYPE_DONGLE_INIT_FAILURE/DUMP_TYPE_DUE_TO_BT/DUMP_TYPE_SMMU_FAULT
	 * : These are called in own handler, not in the interrupt context
	 * With workqueue - all other DUMP_TYPEs : dhd_mem_dump is called in workqueue
	 * Thus, it doesn't neeed to dump SSSR in workqueue
	 */
	DHD_PRINT(("%s: writing sssr dump to file... \n", __FUNCTION__));
	dhd_sssr_dump_to_file(dhdp);
#endif /* !DHD_DUMP_FILE_WRITE_FROM_KERNEL */
}

bool
dhd_is_fis_enabled(void)
{
	return fis_enab;
}
#endif /* DHD_SSSR_DUMP */
