/*
* DHD PCIE for various dumps - all pcie related
* register, intr, counter etc.. dumps and sdtc dumps
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
* <<Broadcom-WL-IPTag/Dual:>>
*/

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
#ifdef BCM_BUZZZ
#include <bcm_buzzz.h>
#endif /* BCM_BUZZZ */
#if defined(FW_SIGNATURE)
#include <dngl_rtlv.h>
#include <bootrommem.h>
#include <fwpkg_utils.h>
#endif /* FW_SIGNATURE */
#include <hnddap.h>

/* Offset for 4375 work around register */
#define REG_WORK_AROUND		(0x1e4/sizeof(uint32))

#ifdef DHD_PCIE_WRAPPER_DUMP
const pcie_wrapper_t wrapper_base_4388[] = {
	{"chipcommon_mwrapper",                 0x18100000},
	{"pcie_mwrapper",                       0x18101000},
	{"pcie_swrapper",                       0x18102000},
	{"chipcommon_swrapper_for_sflash",      0x18103000},
	{"default_swrapper",                    0x18104000},
	{"enumeration_rom_swrapper",            0x18105000},
	{"apb_bridge_cb0_swrapper_apb_2",       0x18106000},
	{"apb_bridge_cb1_swrapper_apb_aaon",    0x18107000},
	{"adb400_swrapper_shared_bridge_s",     0x1810a000},
	{"adb400_mwrapper_shared_bridge1_m",    0x1810b000},
	{"adb400_mwrapper_shared_bridge2_m",    0x1810c000},
	{"armca7_mwrapper",                     0x18120000},
	{"dot11mac_2x2_bw80_mwrapper",          0x18121000},
	{"dot11mac_2x2_bw20_mwrapper",          0x18122000},
	{"dot11mac_1x1_scan_mwrapper",          0x18123000},
	{"sysmem_swrapper",                     0x18124000},
	{"dot11mac_2x2_bw80_i1_mwrapper",       0x18125000},
	{"dot11mac_2x2_bw80_i2_mwrapper",       0x18126000},
	{"dot11mac_2x2_bw80_swrapper",          0x18127000},
	{"dot11mac_2x2_bw20_i1_mwrapper",       0x18128000},
	{"dot11mac_2x2_bw20_i2_mwrapper",       0x18129000},
	{"dot11mac_2x2_bw20_swrapper",          0x1812a000},
	{"dot11mac_1x1_scan_swrapper",          0x1812b000},
	{"aximem_wl_swrapper",                  0x1812c000},
	{"adb400_swrapper_wl_bridge1_s",        0x1812d000},
	{"adb400_swrapper_wl_bridge2_s",        0x1812e000},
	{"default_swrapper",                    0x1812f000},
	{"enumeration_rom_swrapper",            0x18130000},
	{"adb400_mwrapper_wl_bridge_m",         0x18131000},
	{"adb0_bridge_wlb0_swrapper",           0x18132000}
};

const pcie_wrapper_offset_t wrapper_offset_4388[] = {
	{0x408, 4},
	{0x500, 4},
	{0x800, 16},
	{0x900, 32},
	{0xe00, 4}
};
#endif /* DHD_PCIE_WRAPPER_DUMP */

uint32 pcie_slave_wrapper_offsets[] = {
	AI_ERRLOGCTRL,
	AI_ERRLOGDONE,
	AI_ERRLOGSTATUS,
	AI_ERRLOGADDRLO,
	AI_ERRLOGADDRHI,
	AI_ERRLOGID,
	AI_ERRLOGUSER,
	AI_ERRLOGFLAGS
};

#if defined(FW_SIGNATURE)
extern int dhdpcie_read_fwstatus(dhd_bus_t *bus, bl_verif_status_t *status);
#endif /* FW_SIGNATURE */

void
dhd_bus_dump_imp_cfg_registers(struct dhd_bus *bus)
{
	uint32 status_cmd = dhd_pcie_config_read(bus, PCIECFGREG_STATUS_CMD, sizeof(uint32));
	uint32 pmcsr = dhd_pcie_config_read(bus, PCIE_CFG_PMCSR, sizeof(uint32));
	uint32 base_addr0 = dhd_pcie_config_read(bus, PCIECFGREG_BASEADDR0, sizeof(uint32));
	uint32 base_addr1 = dhd_pcie_config_read(bus, PCIECFGREG_BASEADDR1, sizeof(uint32));
	uint32 linkctl = dhd_pcie_config_read(bus, PCIECFGREG_LINK_STATUS_CTRL, sizeof(uint32));
	uint32 linkctl2 = dhd_pcie_config_read(bus, PCIECFGREG_LINK_STATUS_CTRL2, sizeof(uint32));
	uint32 l1ssctrl =
		dhd_pcie_config_read(bus, PCIECFGREG_PML1_SUB_CTRL1, sizeof(uint32));
	uint32 devctl = dhd_pcie_config_read(bus, PCIECFGREG_DEV_STATUS_CTRL, sizeof(uint32));
	uint32 devctl2 = dhd_pcie_config_read(bus, PCIECFGGEN_DEV_STATUS_CTRL2, sizeof(uint32));
	uint32 uc_err_status = dhd_pcie_config_read(bus, PCIE_CFG_UC_ERR_STS, sizeof(uint32));
	uint32 corr_err_status = dhd_pcie_config_read(bus, PCIE_CFG_CORR_ERR_STS, sizeof(uint32));
	uint32 err_cap_ctrl = dhd_pcie_config_read(bus, PCI_ERR_CAP_CTRL, sizeof(uint32));
	uint32 lane_err_status =
		dhd_pcie_config_read(bus, PCIECFGREG_LANE_ERR_STAT, sizeof(uint32));

	uint32 bar0_win_val = dhd_pcie_config_read(bus, PCI_BAR0_WIN, sizeof(uint32));
	uint32 bar1_win_val = dhd_pcie_config_read(bus, PCI_BAR1_WIN, sizeof(uint32));
	uint32 bar0_win2_val = dhd_pcie_config_read(bus, PCIE2_BAR0_WIN2, sizeof(uint32));
	uint32 bar0_core2_win_val =
		dhd_pcie_config_read(bus, PCIE2_BAR0_CORE2_WIN, sizeof(uint32));
	uint32 bar0_core2_win2_val =
		dhd_pcie_config_read(bus, PCIE2_BAR0_CORE2_WIN2, sizeof(uint32));

	DHD_PRINT(("PCIE CFG regs: status_cmd(0x%x)=0x%x, pmcsr(0x%x)=0x%x "
		"base_addr0(0x%x)=0x%x base_addr1(0x%x)=0x%x \n",
		PCIECFGREG_STATUS_CMD, status_cmd,
		PCIE_CFG_PMCSR, pmcsr,
		PCIECFGREG_BASEADDR0, base_addr0,
		PCIECFGREG_BASEADDR1, base_addr1));
	DHD_PRINT(("linkctl(0x%x)=0x%x linkctl2(0x%x)=0x%x l1ssctrl(0x%x)=0x%x "
		"devctl(0x%x)=0x%x devctl2(0x%x)=0x%x \n",
		PCIECFGREG_LINK_STATUS_CTRL, linkctl,
		PCIECFGREG_LINK_STATUS_CTRL2, linkctl2,
		PCIECFGREG_PML1_SUB_CTRL1, l1ssctrl,
		PCIECFGREG_DEV_STATUS_CTRL, devctl,
		PCIECFGGEN_DEV_STATUS_CTRL2, devctl2));
	DHD_PRINT(("uc_err_status(0x%x)=0x%x corr_err_status(0x%x)=0x%x err_cap_ctrl(0x%x)=0x%x "
		"lane_err_status(0x%x)=0x%x\n",
		PCIE_CFG_UC_ERR_STS, uc_err_status,
		PCIE_CFG_CORR_ERR_STS, corr_err_status,
		PCI_ERR_CAP_CTRL, err_cap_ctrl,
		PCIECFGREG_LANE_ERR_STAT, lane_err_status));

	DHD_PRINT(("PCIE BAR Window regs: PCI_BAR0_WIN(0x%x)=0x%x PCI_BAR1_WIN(0x%x)=0x%x\n",
		PCI_BAR0_WIN, bar0_win_val, PCI_BAR1_WIN, bar1_win_val));

	DHD_PRINT(("PCIE2_BAR0_WIN2(0x%x)=0x%x "
		"PCIE2_BAR0_CORE2_WIN(0x%x)=0x%x PCIE2_BAR0_CORE2_WIN2(0x%x)=0x%x\n",
		PCIE2_BAR0_WIN2, bar0_win2_val,
		PCIE2_BAR0_CORE2_WIN, bar0_core2_win_val,
		PCIE2_BAR0_CORE2_WIN2, bar0_core2_win2_val));
}

#define PCIE_SLAVER_WRAPPER_BASE	0x18102000u
static uint32
dhd_get_pcie_slave_wrapper(si_t *sih)
{
	uint32 pcie_slave_wrapper = 0;
	uint16 chipid = si_chipid(sih);

	switch (chipid) {
	case BCM4389_CHIP_ID:
	case BCM4388_CHIP_ID:
	case BCM4387_CHIP_ID:
		pcie_slave_wrapper = PCIE_SLAVER_WRAPPER_BASE;
		break;
	default:
		pcie_slave_wrapper = 0;
		break;
	}

	return pcie_slave_wrapper;
}

void
dhd_dump_pcie_slave_wrapper_regs(dhd_bus_t *bus)
{
	uint32 i, val;
	uint32 pcie_slave_wrapper_base = 0;
	uint32 total_wrapper_regs;

	if (bus->dhd == NULL) {
		return;
	}

	pcie_slave_wrapper_base = dhd_get_pcie_slave_wrapper(bus->sih);
	if (!pcie_slave_wrapper_base) {
		DHD_ERROR(("%s pcie slave wrapper base not populated\n", __FUNCTION__));
		return;
	}

	DHD_PRINT(("%s: ##### Dumping PCIe slave wrapper regs #####\n", __FUNCTION__));

	total_wrapper_regs =
		(uint32)sizeof(pcie_slave_wrapper_offsets) /
		(uint32)sizeof(pcie_slave_wrapper_offsets[0]);

	for (i = 0; i < total_wrapper_regs; i++) {
		dhd_sbreg_op(bus->dhd, (pcie_slave_wrapper_base + pcie_slave_wrapper_offsets[i]),
			&val, TRUE);
	}

	DHD_PRINT(("%s: ##### ##### #####\n", __FUNCTION__));
}

void
dhd_init_dpc_histos(dhd_pub_t *dhd)
{
	dhd_bus_t *bus = dhd->bus;
	if (!bus->dpc_time_histo) {
		bus->dpc_time_histo = dhd_histo_init(dhd);
	}
	if (!bus->ctrl_cpl_post_time_histo) {
		bus->ctrl_cpl_post_time_histo = dhd_histo_init(dhd);
	}
	if (!bus->tx_post_time_histo) {
		bus->tx_post_time_histo = dhd_histo_init(dhd);
	}
	if (!bus->tx_cpl_time_histo) {
		bus->tx_cpl_time_histo = dhd_histo_init(dhd);
	}
	if (!bus->rx_cpl_post_time_histo) {
		bus->rx_cpl_post_time_histo = dhd_histo_init(dhd);
	}
}

void
dhd_deinit_dpc_histos(dhd_pub_t *dhd)
{
	dhd_bus_t *bus = dhd->bus;
	if (bus->dpc_time_histo) {
		dhd_histo_deinit(dhd, bus->dpc_time_histo);
	}
	if (bus->ctrl_cpl_post_time_histo) {
		dhd_histo_deinit(dhd, bus->ctrl_cpl_post_time_histo);
	}
	if (bus->tx_post_time_histo) {
		dhd_histo_deinit(dhd, bus->tx_post_time_histo);
	}
	if (bus->tx_cpl_time_histo) {
		dhd_histo_deinit(dhd, bus->tx_cpl_time_histo);
	}
	if (bus->rx_cpl_post_time_histo) {
		dhd_histo_deinit(dhd, bus->rx_cpl_post_time_histo);
	}
}

void
dhd_dump_dpc_histos(dhd_pub_t *dhd, struct bcmstrbuf *strbuf)
{
	dhd_bus_t *bus = dhd->bus;
	bcm_bprintf(strbuf, "==== DPC Histograms in Usec ====\n");
	dhd_histo_tag_dump(dhd, strbuf, "usec/histo");
	dhd_histo_dump(dhd, strbuf, bus->dpc_time_histo, "dpc");
	dhd_histo_dump(dhd, strbuf, bus->ctrl_cpl_post_time_histo, "ctrl_cpl_post");
	dhd_histo_dump(dhd, strbuf, bus->tx_post_time_histo, "tx_post");
	dhd_histo_dump(dhd, strbuf, bus->tx_cpl_time_histo, "tx_cpl");
	dhd_histo_dump(dhd, strbuf, bus->rx_cpl_post_time_histo, "rx_cpl_post");
	bcm_bprintf(strbuf, "================================\n");
}

void
dhd_clear_dpc_histos(dhd_pub_t *dhd)
{
	dhd_bus_t *bus = dhd->bus;
	dhd_histo_clear(dhd, bus->dpc_time_histo);
	dhd_histo_clear(dhd, bus->ctrl_cpl_post_time_histo);
	dhd_histo_clear(dhd, bus->tx_post_time_histo);
	dhd_histo_clear(dhd, bus->tx_cpl_time_histo);
	dhd_histo_clear(dhd, bus->rx_cpl_post_time_histo);
}

void
dhd_bus_dump_console_buffer(dhd_bus_t *bus)
{
	uint32 n, i;
	uint32 addr;
	char *console_buffer = NULL;
	uint32 console_ptr, console_size, console_index;
	uint8 line[CONSOLE_LINE_MAX], ch;
	int rv;

	if (bus->is_linkdown) {
		DHD_ERROR(("%s: Skip dump Console Buffer due to PCIe link down\n", __FUNCTION__));
		return;
	}

	if (bus->link_state == DHD_PCIE_WLAN_BP_DOWN ||
		bus->link_state == DHD_PCIE_COMMON_BP_DOWN) {
		DHD_ERROR(("%s : wlan/common backplane is down (link_state=%u), skip.\n",
			__FUNCTION__, bus->link_state));
		return;
	}

	DHD_PRINT(("%s: Dump Complete Console Buffer console_addr:0x%x\n",
		__FUNCTION__, bus->pcie_sh->console_addr));

	if (!DHD_VALID_SYSMEM_ADDR(bus, bus->pcie_sh->console_addr)) {
		DHD_ERROR(("%s: Invalid console_addr:0x%x\n",
			__FUNCTION__, bus->pcie_sh->console_addr));
		return;
	}

	addr =	bus->pcie_sh->console_addr + OFFSETOF(hnd_cons_t, log);
	rv = dhdpcie_bus_membytes(bus, FALSE, DHD_PCIE_MEM_BAR1, addr,
		(uint8 *)&console_ptr, sizeof(console_ptr));
	if (rv < 0) {
		goto exit;
	}

	addr =	bus->pcie_sh->console_addr + OFFSETOF(hnd_cons_t, log.buf_size);
	rv = dhdpcie_bus_membytes(bus, FALSE, DHD_PCIE_MEM_BAR1, addr,
		(uint8 *)&console_size, sizeof(console_size));
	if (rv < 0) {
		goto exit;
	}

	addr =	bus->pcie_sh->console_addr + OFFSETOF(hnd_cons_t, log.idx);
	rv = dhdpcie_bus_membytes(bus, FALSE, DHD_PCIE_MEM_BAR1, addr,
		(uint8 *)&console_index, sizeof(console_index));
	if (rv < 0) {
		goto exit;
	}

	console_ptr = ltoh32(console_ptr);
	console_size = ltoh32(console_size);
	console_index = ltoh32(console_index);

	if (console_size > CONSOLE_BUFFER_MAX) {
		goto exit;
	}

	console_buffer = MALLOC(bus->dhd->osh, console_size);
	if (!console_buffer) {
		DHD_ERROR(("%s: Failed to alloc %u bytes for console buf\n",
			__FUNCTION__, console_size));
		goto exit;
	}

	rv = dhdpcie_bus_membytes(bus, FALSE, DHD_PCIE_MEM_BAR1, console_ptr,
		(uint8 *)console_buffer, console_size);
	if (rv < 0) {
		goto exit;
	}

	for (i = 0, n = 0; i < console_size; i += n + 1) {
		for (n = 0; n < CONSOLE_LINE_MAX - 2; n++) {
			ch = console_buffer[(console_index + i + n) % console_size];
			if (ch == '\n')
				break;
			line[n] = ch;
		}

		if (n > 0) {
			if (line[n - 1] == '\r')
				n--;
			line[n] = 0;
			/* Don't use DHD_ERROR macro since we print
			 * a lot of information quickly. The macro
			 * will truncate a lot of the printfs
			 */

			DHD_PRINT(("CONSOLE: %s\n", line));
		}
	}

exit:
	if (console_buffer)
		MFREE(bus->dhd->osh, console_buffer, console_size);
	return;
}

void
dhdpcie_schedule_log_dump(dhd_bus_t *bus)
{
#if defined(DHD_DUMP_FILE_WRITE_FROM_KERNEL) && defined(DHD_LOG_DUMP)
	log_dump_type_t *flush_type;

	/* flush_type is freed at do_dhd_log_dump function */
	flush_type = MALLOCZ(bus->dhd->osh, sizeof(log_dump_type_t));
	if (flush_type) {
		*flush_type = DLD_BUF_TYPE_ALL;
		dhd_schedule_log_dump(bus->dhd, flush_type);
	} else {
		DHD_ERROR(("%s Fail to malloc flush_type\n", __FUNCTION__));
	}
#endif /* DHD_DUMP_FILE_WRITE_FROM_KERNEL && DHD_LOG_DUMP */
}

void
dhd_bus_clearcounts(dhd_pub_t *dhdp)
{
	struct dhd_bus *bus = dhdp->bus;
	flow_ring_node_t *flow_ring_node = NULL;
	flow_info_t *flow_info = NULL;
	uint16 flowid = 0;
	unsigned long flags = 0;

	dhd_prot_clearcounts(dhdp);

	/* clear per flowring stats */
	bzero(bus->flowring_high_watermark, bus->max_submission_rings * sizeof(uint32));
	bzero(bus->flowring_cur_items, bus->max_submission_rings * sizeof(uint32));
	for (flowid = 0; flowid < dhdp->num_h2d_rings; flowid++) {
		flow_ring_node = DHD_FLOW_RING(dhdp, flowid);
		DHD_FLOWRING_LOCK(flow_ring_node->lock, flags);
		if (flow_ring_node->status != FLOW_RING_STATUS_OPEN) {
			DHD_FLOWRING_UNLOCK(flow_ring_node->lock, flags);
			continue;
		}
		flow_info = &flow_ring_node->flow_info;
		flow_info->num_tx_pkts = 0;
		flow_info->num_tx_dropped = 0;
		flow_info->num_tx_status = 0;
		DHD_FLOWRING_UNLOCK(flow_ring_node->lock, flags);
	}

#ifdef DHD_TREAT_D3ACKTO_AS_LINKDWN
	bus->d3ackto_as_linkdwn_cnt = 0;
	bus->iovarto_as_linkdwn_cnt = 0;
#endif
	dhdp->rx_pktgetpool_fail = 0;

	dhd_clear_dpc_histos(dhdp);

	dhd_prot_ptm_stats_clr(dhdp);
}

#ifdef BCM_BUZZZ
int
dhd_buzzz_dump_cntrs(char *p, uint32 *core, uint32 *log,
	const int num_counters)
{
	int bytes = 0;
	uint32 ctr;
	uint32 curr[BCM_BUZZZ_COUNTERS_MAX], prev[BCM_BUZZZ_COUNTERS_MAX];
	uint32 delta[BCM_BUZZZ_COUNTERS_MAX];

	/* Compute elapsed counter values per counter event type */
	for (ctr = 0U; ctr < num_counters; ctr++) {
		prev[ctr] = core[ctr];
		curr[ctr] = *log++;
		core[ctr] = curr[ctr];  /* saved for next log */

		if (curr[ctr] < prev[ctr])
			delta[ctr] = curr[ctr] + (~0U - prev[ctr]);
		else
			delta[ctr] = (curr[ctr] - prev[ctr]);

		bytes += sprintf(p + bytes, "%12u ", delta[ctr]);
	}

	return bytes;
}

typedef union cm3_cnts { /* export this in bcm_buzzz.h */
	uint32 u32;
	uint8  u8[4];
	struct {
		uint8 cpicnt;
		uint8 exccnt;
		uint8 sleepcnt;
		uint8 lsucnt;
	};
} cm3_cnts_t;

int
dhd_bcm_buzzz_dump_cntrs6(char *p, uint32 *core, uint32 *log)
{
	int bytes = 0;

	uint32 cyccnt, instrcnt;
	cm3_cnts_t cm3_cnts;
	uint8 foldcnt;

	{   /* 32bit cyccnt */
		uint32 curr, prev, delta;
		prev = core[0]; curr = *log++; core[0] = curr;
		if (curr < prev)
			delta = curr + (~0U - prev);
		else
			delta = (curr - prev);

		bytes += sprintf(p + bytes, "%12u ", delta);
		cyccnt = delta;
	}

	{	/* Extract the 4 cnts: cpi, exc, sleep and lsu */
		int i;
		uint8 max8 = ~0;
		cm3_cnts_t curr, prev, delta;
		prev.u32 = core[1];
		curr.u32 = *log++; core[1] = curr.u32;
		for (i = 0; i < 4; i++) {
			if (curr.u8[i] < prev.u8[i])
				delta.u8[i] = curr.u8[i] + (max8 - prev.u8[i]);
			else
				delta.u8[i] = (curr.u8[i] - prev.u8[i]);
			bytes += sprintf(p + bytes, "%4u ", delta.u8[i]);
		}
		cm3_cnts.u32 = delta.u32;
	}

	{   /* Extract the foldcnt from arg0 */
		uint8 curr, prev, delta, max8 = ~0;
		bcm_buzzz_arg0_t arg0; arg0.u32 = *log;
		prev = core[2]; curr = arg0.klog.cnt; core[2] = curr;
		if (curr < prev)
			delta = curr + (max8 - prev);
		else
			delta = (curr - prev);
		bytes += sprintf(p + bytes, "%4u ", delta);
		foldcnt = delta;
	}

	instrcnt = cyccnt - (cm3_cnts.u8[0] + cm3_cnts.u8[1] + cm3_cnts.u8[2]
		+ cm3_cnts.u8[3]) + foldcnt;
	if (instrcnt > 0xFFFFFF00)
		bytes += sprintf(p + bytes, "[%10s] ", "~");
	else
		bytes += sprintf(p + bytes, "[%10u] ", instrcnt);
	return bytes;
}

int
dhd_buzzz_dump_log(char *p, uint32 *core, uint32 *log, bcm_buzzz_t *buzzz)
{
	int bytes = 0;
	bcm_buzzz_arg0_t arg0;
	static uint8 *fmt[] = BCM_BUZZZ_FMT_STRINGS;

	if (buzzz->counters == 6) {
		bytes += dhd_bcm_buzzz_dump_cntrs6(p, core, log);
		log += 2; /* 32bit cyccnt + (4 x 8bit) CM3 */
	} else {
		bytes += dhd_buzzz_dump_cntrs(p, core, log, buzzz->counters);
		log += buzzz->counters; /* (N x 32bit) CR4=3, CA7=4 */
	}

	/* Dump the logged arguments using the registered formats */
	arg0.u32 = *log++;

	switch (arg0.klog.args) {
	case 0:
		bytes += sprintf(p + bytes, fmt[arg0.klog.id]);
		break;
	case 1:
	{
		uint32 arg1 = *log++;
		bytes += sprintf(p + bytes, fmt[arg0.klog.id], arg1);
		break;
	}
	case 2:
	{
		uint32 arg1, arg2;
		arg1 = *log++; arg2 = *log++;
		bytes += sprintf(p + bytes, fmt[arg0.klog.id], arg1, arg2);
		break;
	}
	case 3:
	{
		uint32 arg1, arg2, arg3;
		arg1 = *log++; arg2 = *log++; arg3 = *log++;
		bytes += sprintf(p + bytes, fmt[arg0.klog.id], arg1, arg2, arg3);
		break;
	}
	case 4:
	{
		uint32 arg1, arg2, arg3, arg4;
		arg1 = *log++; arg2 = *log++;
		arg3 = *log++; arg4 = *log++;
		bytes += sprintf(p + bytes, fmt[arg0.klog.id], arg1, arg2, arg3, arg4);
		break;
	}
	default:
		DHD_CONS_ONLY(("Maximum one argument supported\n"));
		break;
	}

	bytes += sprintf(p + bytes, "\n");

	return bytes;
}

void dhd_buzzz_dump(bcm_buzzz_t *buzzz_p, void *buffer_p, char *p)
{
	int i;
	uint32 total, part1, part2, log_sz, core[BCM_BUZZZ_COUNTERS_MAX];
	void *log;

	for (i = 0; i < BCM_BUZZZ_COUNTERS_MAX; i++) {
		core[i] = 0;
	}

	log_sz = buzzz_p->log_sz;

	part1 = ((uint32)buzzz_p->cur - (uint32)buzzz_p->log) / log_sz;

	if (buzzz_p->wrap == TRUE) {
		part2 = ((uint32)buzzz_p->end - (uint32)buzzz_p->cur) / log_sz;
		total = (buzzz_p->buffer_sz - BCM_BUZZZ_LOGENTRY_MAXSZ) / log_sz;
	} else {
		part2 = 0U;
		total = buzzz_p->count;
	}

	if (total == 0U) {
		DHD_CONS_ONLY(("bcm_buzzz_dump total<%u> done\n", total));
		return;
	} else {
		DHD_CONS_ONLY(("bcm_buzzz_dump total<%u> : part2<%u> + part1<%u>\n",
		       total, part2, part1));
	}

	if (part2) {   /* with wrap */
		log = (void *)((size_t)buffer_p + (buzzz_p->cur - buzzz_p->log));
		while (part2--) {   /* from cur to end : part2 */
			p[0] = '\0';
			dhd_buzzz_dump_log(p, core, (uint32 *)log, buzzz_p);
			printf("%s", p);
			log = (void *)((size_t)log + buzzz_p->log_sz);
		}
	}

	log = (void *)buffer_p;
	while (part1--) {
		p[0] = '\0';
		dhd_buzzz_dump_log(p, core, (uint32 *)log, buzzz_p);
		printf("%s", p);
		log = (void *)((size_t)log + buzzz_p->log_sz);
	}

	DHD_CONS_ONLY(("bcm_buzzz_dump done.\n"));
}

int dhd_buzzz_dump_dngl(dhd_bus_t *bus)
{
	bcm_buzzz_t *buzzz_p = NULL;
	void *buffer_p = NULL;
	char *page_p = NULL;
	pciedev_shared_t *sh;
	int ret = 0;

	if (bus->dhd->busstate != DHD_BUS_DATA) {
		return BCME_UNSUPPORTED;
	}
	page_p = (char *)MALLOC(bus->dhd->osh, 4096);
	if (page_p == NULL) {
		DHD_CONS_ONLY(("Page memory allocation failure\n"));
		goto done;
	}
	buzzz_p = MALLOC(bus->dhd->osh, sizeof(bcm_buzzz_t));
	if (buzzz_p == NULL) {
		DHD_CONS_ONLY(("BCM BUZZZ memory allocation failure\n"));
		goto done;
	}

	ret = dhdpcie_readshared(bus);
	if (ret < 0) {
		DHD_ERROR(("%s :Shared area read failed \n", __FUNCTION__));
		goto done;
	}

	sh = bus->pcie_sh;

	DHD_INFO(("%s buzzz:%08x\n", __FUNCTION__, sh->buzz_dbg_ptr));

	if (sh->buzz_dbg_ptr != 0U) {	/* Fetch and display dongle BUZZZ Trace */
		dhdpcie_bus_membytes(bus, FALSE, DHD_PCIE_MEM_BAR1, (ulong)sh->buzz_dbg_ptr,
			(uint8 *)buzzz_p, sizeof(bcm_buzzz_t));

		DHD_CONS_ONLY(("BUZZZ[0x%08x]: log<0x%08x> cur<0x%08x> end<0x%08x> "
			"count<%u> status<%u> wrap<%u>\n"
			"cpu<0x%02X> counters<%u> group<%u> buffer_sz<%u> log_sz<%u>\n",
			(int)sh->buzz_dbg_ptr,
			(int)buzzz_p->log, (int)buzzz_p->cur, (int)buzzz_p->end,
			buzzz_p->count, buzzz_p->status, buzzz_p->wrap,
			buzzz_p->cpu_idcode, buzzz_p->counters, buzzz_p->group,
			buzzz_p->buffer_sz, buzzz_p->log_sz));

		if (buzzz_p->count == 0) {
			DHD_CONS_ONLY(("Empty dongle BUZZZ trace\n\n"));
			goto done;
		}

		/* Allocate memory for trace buffer and format strings */
		buffer_p = MALLOC(bus->dhd->osh, buzzz_p->buffer_sz);
		if (buffer_p == NULL) {
			DHD_CONS_ONLY(("Buffer memory allocation failure\n"));
			goto done;
		}

		/* Fetch the trace. format strings are exported via bcm_buzzz.h */
		dhdpcie_bus_membytes(bus, FALSE, DHD_PCIE_MEM_BAR1, (uint32)buzzz_p->log,
			(uint8 *)buffer_p, buzzz_p->buffer_sz);

		/* Process and display the trace using formatted output */

		{
			int ctr;
			for (ctr = 0; ctr < buzzz_p->counters; ctr++) {
				printf("<Evt[%02X]> ", buzzz_p->eventid[ctr]);
			}
			DHD_CONS_ONLY(("<code execution point>\n"));
		}

		dhd_buzzz_dump(buzzz_p, buffer_p, page_p);

		DHD_CONS_ONLY(("----- End of dongle BCM BUZZZ Trace -----\n\n"));

		MFREE(bus->dhd->osh, buffer_p, buzzz_p->buffer_sz); buffer_p = NULL;
	}

done:

	if (page_p) {
		MFREE(bus->dhd->osh, page_p, 4096);
	}
	if (buzzz_p) {
		MFREE(bus->dhd->osh, buzzz_p, sizeof(bcm_buzzz_t));
	}
	if (buffer_p) {
		MFREE(bus->dhd->osh, buffer_p, buzzz_p->buffer_sz);
	}

	return BCME_OK;
}
#endif /* BCM_BUZZZ */

void
dhd_bus_dump_dar_registers(struct dhd_bus *bus)
{
	uint32 dar_clk_ctrl_val, dar_pwr_ctrl_val, dar_intstat_val,
		dar_errlog_val, dar_erraddr_val, dar_pcie_mbint_val;
	uint32 dar_clk_ctrl_reg, dar_pwr_ctrl_reg, dar_intstat_reg,
		dar_errlog_reg, dar_erraddr_reg, dar_pcie_mbint_reg;

	if (bus->is_linkdown) {
		DHD_ERROR(("%s: link is down\n", __FUNCTION__));
		return;
	}

	if (bus->sih == NULL) {
		DHD_ERROR(("%s: si_attach has not happened, cannot dump DAR registers\n",
			__FUNCTION__));
		return;
	}

	bus->dar_err_set = FALSE;

	if (DAR_PWRREQ(bus)) {
		dhd_bus_pcie_pwr_req(bus);
	}

	dar_clk_ctrl_reg = (uint32)DAR_CLK_CTRL(bus->sih->buscorerev);
	dar_pwr_ctrl_reg = (uint32)DAR_PCIE_PWR_CTRL(bus->sih->buscorerev);
	dar_intstat_reg = (uint32)DAR_INTSTAT(bus->sih->buscorerev);
	dar_errlog_reg = (uint32)DAR_ERRLOG(bus->sih->buscorerev);
	dar_erraddr_reg = (uint32)DAR_ERRADDR(bus->sih->buscorerev);
	dar_pcie_mbint_reg = (uint32)DAR_PCIMailBoxInt(bus->sih->buscorerev);

	if (bus->sih->buscorerev < 24) {
		DHD_ERROR(("%s: DAR not supported for corerev(%d) < 24\n",
			__FUNCTION__, bus->sih->buscorerev));
		return;
	}

	dar_clk_ctrl_val = si_corereg(bus->sih, bus->sih->buscoreidx, dar_clk_ctrl_reg, 0, 0);
	dar_pwr_ctrl_val = si_corereg(bus->sih, bus->sih->buscoreidx, dar_pwr_ctrl_reg, 0, 0);
	dar_intstat_val = si_corereg(bus->sih, bus->sih->buscoreidx, dar_intstat_reg, 0, 0);
	dar_errlog_val = si_corereg(bus->sih, bus->sih->buscoreidx, dar_errlog_reg, 0, 0);
	dar_erraddr_val = si_corereg(bus->sih, bus->sih->buscoreidx, dar_erraddr_reg, 0, 0);
	dar_pcie_mbint_val = si_corereg(bus->sih, bus->sih->buscoreidx, dar_pcie_mbint_reg, 0, 0);

	DHD_PRINT(("%s: dar_clk_ctrl(0x%x:0x%x) dar_pwr_ctrl(0x%x:0x%x) "
		"dar_intstat(0x%x:0x%x)\n",
		__FUNCTION__, dar_clk_ctrl_reg, dar_clk_ctrl_val,
		dar_pwr_ctrl_reg, dar_pwr_ctrl_val, dar_intstat_reg, dar_intstat_val));

	DHD_PRINT(("%s: dar_errlog(0x%x:0x%x) dar_erraddr(0x%x:0x%x) "
		"dar_pcie_mbint(0x%x:0x%x)\n",
		__FUNCTION__, dar_errlog_reg, dar_errlog_val,
		dar_erraddr_reg, dar_erraddr_val, dar_pcie_mbint_reg, dar_pcie_mbint_val));

	if (dar_errlog_val || dar_erraddr_val) {
		bus->dar_err_set = TRUE;
	}

}

#ifdef FW_SIGNATURE
/* Dump secure firmware status. */
int
dhd_bus_dump_fws(dhd_bus_t *bus, struct bcmstrbuf *strbuf)
{
	bl_verif_status_t status;
	bl_mem_info_t     meminfo;
	int               err = BCME_OK;

	err = dhdpcie_read_fwstatus(bus, &status);
	if (err != BCME_OK) {
		return err;
	}

	bzero(&meminfo, sizeof(meminfo));
	if (bus->fw_memmap_download_addr != 0) {
		err = dhdpcie_bus_membytes(bus, FALSE, DHD_PCIE_MEM_BAR1,
			bus->fw_memmap_download_addr, (uint8 *)&meminfo, sizeof(meminfo));
		if (err != BCME_OK) {
			DHD_ERROR(("%s: error %d on reading %zu membytes at 0x%08x\n",
				__FUNCTION__, err, sizeof(meminfo), bus->fw_memmap_download_addr));
			return err;
		}
	}

	bcm_bprintf(strbuf, "Firmware signing\nSignature: (%08x) len (%d)\n",
		bus->fwsig_download_addr, bus->fwsig_download_len);

	bcm_bprintf(strbuf,
		"Verification status: (%08x)\n"
		"\tstatus: %d\n"
		"\tstate: %u\n"
		"\talloc_bytes: %u\n"
		"\tmax_alloc_bytes: %u\n"
		"\ttotal_alloc_bytes: %u\n"
		"\ttotal_freed_bytes: %u\n"
		"\tnum_allocs: %u\n"
		"\tmax_allocs: %u\n"
		"\tmax_alloc_size: %u\n"
		"\talloc_failures: %u\n",
		bus->fwstat_download_addr,
		status.status,
		status.state,
		status.alloc_bytes,
		status.max_alloc_bytes,
		status.total_alloc_bytes,
		status.total_freed_bytes,
		status.num_allocs,
		status.max_allocs,
		status.max_alloc_size,
		status.alloc_failures);

	bcm_bprintf(strbuf,
		"Memory info: (%08x)\n"
		"\tfw   %08x-%08x\n\theap %08x-%08x\n\tsig  %08x-%08x\n\tvst  %08x-%08x\n",
		bus->fw_memmap_download_addr,
		meminfo.firmware.start,  meminfo.firmware.end,
		meminfo.heap.start,      meminfo.heap.end,
		meminfo.signature.start, meminfo.signature.end,
		meminfo.vstatus.start,   meminfo.vstatus.end);

	return err;
}
#endif /* FW_SIGNATURE */

void
dhd_dump_intr_counters(dhd_pub_t *dhd, struct bcmstrbuf *strbuf)
{
	dhd_bus_t *bus;
	uint64 current_time = OSL_LOCALTIME_NS();

	if (!dhd) {
		DHD_ERROR(("%s: dhd is NULL\n", __FUNCTION__));
		return;
	}

	bus = dhd->bus;
	if (!bus) {
		DHD_ERROR(("%s: bus is NULL\n", __FUNCTION__));
		return;
	}

	bcm_bprintf(strbuf, "\n ------- DUMPING INTR enable/disable counters-------\n");
	bcm_bprintf(strbuf, "host_irq_disable_count=%lu host_irq_enable_count=%lu\n"
		"dngl_intmask_disable_count=%lu dngl_intmask_enable_count=%lu\n"
		"dpc_return_busdown_count=%lu non_ours_irq_count=%lu rot_dpc_sched_count=%d\n",
		bus->host_irq_disable_count, bus->host_irq_enable_count,
		bus->dngl_intmask_disable_count, bus->dngl_intmask_enable_count,
		bus->dpc_return_busdown_count, bus->non_ours_irq_count, bus->rot_dpc_sched_count);
#ifdef BCMPCIE_OOB_HOST_WAKE
	bcm_bprintf(strbuf, "oob_intr_count=%lu oob_intr_enable_count=%lu"
		" oob_intr_disable_count=%lu\noob_irq_num=%d"
		" last_oob_irq_times="SEC_USEC_FMT":"SEC_USEC_FMT
		" last_oob_irq_enable_time="SEC_USEC_FMT"\nlast_oob_irq_disable_time="SEC_USEC_FMT
		" oob_irq_enabled=%d oob_gpio_level=%d\n",
		bus->oob_intr_count, bus->oob_intr_enable_count,
		bus->oob_intr_disable_count, dhdpcie_get_oob_irq_num(bus),
		GET_SEC_USEC(bus->last_oob_irq_isr_time),
		GET_SEC_USEC(bus->last_oob_irq_thr_time),
		GET_SEC_USEC(bus->last_oob_irq_enable_time),
		GET_SEC_USEC(bus->last_oob_irq_disable_time), dhdpcie_get_oob_irq_status(bus),
		dhdpcie_get_oob_irq_level(bus));
#endif /* BCMPCIE_OOB_HOST_WAKE */
	bcm_bprintf(strbuf, "\ncurrent_time="SEC_USEC_FMT" isr_entry_time="SEC_USEC_FMT
		" isr_exit_time="SEC_USEC_FMT"\n"
		"isr_sched_dpc_time="SEC_USEC_FMT" rpm_sched_dpc_time="SEC_USEC_FMT"\n"
		" last_non_ours_irq_time="SEC_USEC_FMT" dpc_entry_time="SEC_USEC_FMT"\n"
		"last_process_ctrlbuf_time="SEC_USEC_FMT " last_process_flowring_time="SEC_USEC_FMT
		" last_process_txcpl_time="SEC_USEC_FMT"\nlast_process_rxcpl_time="SEC_USEC_FMT
		" last_process_infocpl_time="SEC_USEC_FMT" last_process_edl_time="SEC_USEC_FMT
		"\ndpc_exit_time="SEC_USEC_FMT" resched_dpc_time="SEC_USEC_FMT"\n"
		"last_d3_inform_time="SEC_USEC_FMT" dpc_sched=%u\n",
		GET_SEC_USEC(current_time), GET_SEC_USEC(bus->isr_entry_time),
		GET_SEC_USEC(bus->isr_exit_time), GET_SEC_USEC(bus->isr_sched_dpc_time),
		GET_SEC_USEC(bus->rpm_sched_dpc_time),
		GET_SEC_USEC(bus->last_non_ours_irq_time), GET_SEC_USEC(bus->dpc_entry_time),
		GET_SEC_USEC(bus->last_process_ctrlbuf_time),
		GET_SEC_USEC(bus->last_process_flowring_time),
		GET_SEC_USEC(bus->last_process_txcpl_time),
		GET_SEC_USEC(bus->last_process_rxcpl_time),
		GET_SEC_USEC(bus->last_process_infocpl_time),
		GET_SEC_USEC(bus->last_process_edl_time),
		GET_SEC_USEC(bus->dpc_exit_time), GET_SEC_USEC(bus->resched_dpc_time),
		GET_SEC_USEC(bus->last_d3_inform_time), bus->dpc_sched);

	bcm_bprintf(strbuf, "\nlast_suspend_start_time="SEC_USEC_FMT" last_suspend_end_time="
		SEC_USEC_FMT" last_resume_start_time="SEC_USEC_FMT" last_resume_end_time="
		SEC_USEC_FMT"\n", GET_SEC_USEC(bus->last_suspend_start_time),
		GET_SEC_USEC(bus->last_suspend_end_time),
		GET_SEC_USEC(bus->last_resume_start_time),
		GET_SEC_USEC(bus->last_resume_end_time));

#if defined(SHOW_LOGTRACE) && defined(DHD_USE_KTHREAD_FOR_LOGTRACE)
	bcm_bprintf(strbuf, "logtrace_thread_entry_time="SEC_USEC_FMT
		" logtrace_thread_sem_down_time="SEC_USEC_FMT
		"\nlogtrace_thread_flush_time="SEC_USEC_FMT
		" logtrace_thread_unexpected_break_time="SEC_USEC_FMT
		"\nlogtrace_thread_complete_time="SEC_USEC_FMT"\n",
		GET_SEC_USEC(dhd->logtrace_thr_ts.entry_time),
		GET_SEC_USEC(dhd->logtrace_thr_ts.sem_down_time),
		GET_SEC_USEC(dhd->logtrace_thr_ts.flush_time),
		GET_SEC_USEC(dhd->logtrace_thr_ts.unexpected_break_time),
		GET_SEC_USEC(dhd->logtrace_thr_ts.complete_time));
#endif /* SHOW_LOGTRACE && DHD_USE_KTHREAD_FOR_LOGTRACE */

	bcm_bprintf(strbuf, "dhd_watchdog_ms: %d\n", dhd_watchdog_ms);
}

static void
dhd_dump_intr_registers(dhd_pub_t *dhd, struct bcmstrbuf *strbuf)
{
	uint32 intstatus = 0;
	uint32 intmask = 0;
	uint32 d2h_db0 = 0;
	uint32 d2h_mb_data = 0;

	intstatus = si_corereg(dhd->bus->sih, dhd->bus->sih->buscoreidx,
		dhd->bus->pcie_mailbox_int, 0, 0);

#ifdef DHD_MMIO_TRACE
	dhd_bus_mmio_trace(dhd->bus, dhd->bus->pcie_mailbox_int, intstatus, FALSE);
#endif /* defined(DHD_MMIO_TRACE) */

	intmask = si_corereg(dhd->bus->sih, dhd->bus->sih->buscoreidx,
		dhd->bus->pcie_mailbox_mask, 0, 0);

#ifdef DHD_MMIO_TRACE
	dhd_bus_mmio_trace(dhd->bus, dhd->bus->pcie_mailbox_mask, intmask, FALSE);
#endif /* defined(DHD_MMIO_TRACE) */

	d2h_db0 = si_corereg(dhd->bus->sih, dhd->bus->sih->buscoreidx,
		PCIE_REG_OFF(devtohost0doorbell0), 0, 0);
	dhd_bus_cmn_readshared(dhd->bus, &d2h_mb_data, D2H_MB_DATA, 0);

	bcm_bprintf(strbuf, "intstatus=0x%x intmask=0x%x d2h_db0=0x%x\n",
		intstatus, intmask, d2h_db0);
	bcm_bprintf(strbuf, "d2h_mb_data=0x%x def_intmask=0x%x\n",
		d2h_mb_data, dhd->bus->def_intmask);
}

void
dhd_bus_dump_flowring(dhd_pub_t *dhdp, struct bcmstrbuf *strbuf)
{
	uint16 flowid;
	int ix = 0;
	flow_ring_node_t *flow_ring_node;
	flow_info_t *flow_info;
#ifdef TX_STATUS_LATENCY_STATS
	uint8 ifindex;
	if_flow_lkup_t *if_flow_lkup;
	dhd_if_tx_status_latency_t if_tx_status_latency[DHD_MAX_IFS];
#endif /* TX_STATUS_LATENCY_STATS */
	struct dhd_bus *bus = dhdp->bus;
	unsigned long flags;

#ifdef TX_STATUS_LATENCY_STATS
	bzero(if_tx_status_latency, sizeof(if_tx_status_latency));
#endif /* TX_STATUS_LATENCY_STATS */

	bcm_bprintf(strbuf, "Flowring info:\n==============\n");
	bcm_bprintf(strbuf, "[RD=read ptr; WR=write ptr; T=TCM; H=Host; L=Local; D=DMA index'd]\n");
#ifndef BCM_ROUTER_DHD
	bcm_bprintf(strbuf,
		"%4s %4s %2s %4s %17s %4s %10s %17s %17s %17s %17s %14s %14s %10s ",
		"Num:", "Flow", "If", "Prio", ":Dest_MacAddress:", "Qlen",
		" Overflows", "TRD: HLRD: HDRD", "TWR: HLWR: HDWR", "BASE(VA)", "BASE(PA)",
		"WORK_ITEM_SIZE", "MAX_WORK_ITEMS", "TOTAL_SIZE");
#else
	bcm_bprintf(strbuf,
		"%4s %4s %2s %4s %17s %4s %4s %6s %10s %17s %17s %17s %17s %14s %14s %10s ",
		"Num:", "Flow", "If", "Prio", ":Dest_MacAddress:", "Qlen", "CLen", "L2CLen",
		" Overflows", "TRD: HLRD: HDRD", "TWR: HLWR: HDWR", "BASE(VA)", "BASE(PA)",
		"WORK_ITEM_SIZE", "MAX_WORK_ITEMS", "TOTAL_SIZE");
#endif /* !BCM_ROUTER_DHD */

#ifdef TX_STATUS_LATENCY_STATS
	/* Average Tx status/Completion Latency in micro secs */
	bcm_bprintf(strbuf, "%16s %16s", "       NumTxPkts", "    AvgTxCmpL_Us");
#endif /* TX_STATUS_LATENCY_STATS */

	bcm_bprintf(strbuf, "\n");

	for (flowid = 0; flowid < dhdp->num_h2d_rings; flowid++) {
		flow_ring_node = DHD_FLOW_RING(dhdp, flowid);
		DHD_FLOWRING_LOCK(flow_ring_node->lock, flags);
		if (flow_ring_node->status != FLOW_RING_STATUS_OPEN) {
			DHD_FLOWRING_UNLOCK(flow_ring_node->lock, flags);
			continue;
		}

		flow_info = &flow_ring_node->flow_info;
		bcm_bprintf(strbuf,
			"%4d %4d %2d %4d "MACDBG" %4d"
#ifdef BCM_ROUTER_DHD
			"%4d %6d"
#endif
			"%10u ", ix++,
			flow_ring_node->flowid, flow_info->ifindex, flow_info->tid,
			MAC2STRDBG(flow_info->da),
			DHD_FLOW_QUEUE_LEN(&flow_ring_node->queue),
#ifdef BCM_ROUTER_DHD
			DHD_CUMM_CTR_READ(DHD_FLOW_QUEUE_CLEN_PTR(&flow_ring_node->queue)),
			DHD_CUMM_CTR_READ(DHD_FLOW_QUEUE_L2CLEN_PTR(&flow_ring_node->queue)),
#endif
			DHD_FLOW_QUEUE_FAILURES(&flow_ring_node->queue));
		dhd_prot_print_flow_ring(dhdp, flow_ring_node->prot_info, TRUE, strbuf,
			"%5d:%5d:%5d %5d:%5d:%5d %17p %8x:%8x %14d %14d %10d");

#ifdef TX_STATUS_LATENCY_STATS
		bcm_bprintf(strbuf, "%16llu %16llu ",
			flow_info->num_tx_pkts,
			flow_info->num_tx_status ?
			DIV_U64_BY_U64(flow_info->cum_tx_status_latency,
			flow_info->num_tx_status) : 0);
		ifindex = flow_info->ifindex;
		ASSERT(ifindex < DHD_MAX_IFS);
		if (ifindex < DHD_MAX_IFS) {
			if_tx_status_latency[ifindex].num_tx_status += flow_info->num_tx_status;
			if_tx_status_latency[ifindex].cum_tx_status_latency +=
				flow_info->cum_tx_status_latency;
		} else {
			DHD_ERROR(("%s: Bad IF index: %d associated with flowid: %d\n",
				__FUNCTION__, ifindex, flowid));
		}
#endif /* TX_STATUS_LATENCY_STATS */
		bcm_bprintf(strbuf, "\n");
		DHD_FLOWRING_UNLOCK(flow_ring_node->lock, flags);
	}

	/* additional per flowring stats */
	bcm_bprintf(strbuf, "\nPer Flowring stats:\n");
	bcm_bprintf(strbuf, "%4s   %13s   %13s", "Flow", "High Watermark", "Cur Num Items");
	bcm_bprintf(strbuf, "%16s %16s %16s \n", "       NumTxPkts",
		"    NumTxDropped", "    NumTxStatus");
	for (flowid = 0; flowid < dhdp->num_h2d_rings; flowid++) {
		flow_ring_node = DHD_FLOW_RING(dhdp, flowid);
		DHD_FLOWRING_LOCK(flow_ring_node->lock, flags);
		if (flow_ring_node->status != FLOW_RING_STATUS_OPEN) {
			DHD_FLOWRING_UNLOCK(flow_ring_node->lock, flags);
			continue;
		}
		flow_info = &flow_ring_node->flow_info;
		bcm_bprintf(strbuf, "%4d   %13d   %13d", flowid,
			bus->flowring_high_watermark[flowid], bus->flowring_cur_items[flowid]);
		bcm_bprintf(strbuf, "%16llu %16llu %16llu \n", flow_info->num_tx_pkts,
			flow_info->num_tx_dropped, flow_info->num_tx_status);
		DHD_FLOWRING_UNLOCK(flow_ring_node->lock, flags);
	}

#ifdef TX_STATUS_LATENCY_STATS
	bcm_bprintf(strbuf, "\nInterface Tx latency:\n");
	bcm_bprintf(strbuf, "\n%s  %16s  %16s\n", "If", "AvgTxCmpL_Us", "NumTxStatus");
	if_flow_lkup = (if_flow_lkup_t *)dhdp->if_flow_lkup;
	for (ix = 0; ix < DHD_MAX_IFS; ix++) {
		if (!if_flow_lkup[ix].status) {
			continue;
		}
		bcm_bprintf(strbuf, "%2d  %16llu  %16llu\n",
			ix,
			if_tx_status_latency[ix].num_tx_status ?
			DIV_U64_BY_U64(if_tx_status_latency[ix].cum_tx_status_latency,
			if_tx_status_latency[ix].num_tx_status) : 0,
			if_tx_status_latency[ix].num_tx_status);
	}
#endif /* TX_STATUS_LATENCY_STATS */

#ifdef DHD_LIMIT_MULTI_CLIENT_FLOWRINGS
	bcm_bprintf(strbuf, "\nmulti_client_flow_rings:%u max_multi_client_flow_rings:%d\n",
		OSL_ATOMIC_READ(dhdp->osh, &dhdp->multi_client_flow_rings),
		dhdp->max_multi_client_flow_rings);
#endif /* DHD_LIMIT_MULTI_CLIENT_FLOWRINGS */

	bcm_bprintf(strbuf, "\n");
}

void
dhd_bus_counters(dhd_pub_t *dhdp, struct bcmstrbuf *strbuf)
{
	dhd_prot_counters(dhdp, strbuf, TRUE, TRUE);

	dhd_bus_dump_flowring(dhdp, strbuf);

	dhd_dump_dpc_histos(dhdp, strbuf);

	dhd_prot_ptm_stats_dump(dhdp, strbuf);

}

/** Add bus dump output to a buffer */
void
dhd_bus_dump(dhd_pub_t *dhdp, struct bcmstrbuf *strbuf)
{
	uint16 flowid;
#ifdef BCMDBG
	int ix = 0;
	flow_ring_node_t *flow_ring_node;
	flow_info_t *flow_info;
	flow_info_t *local_flow_info;
#endif /* BCMDBG */

	BCM_REFERENCE(flowid);
#if defined(FW_SIGNATURE)
	/* Dump secure firmware status. */
	if (dhdp->busstate <= DHD_BUS_LOAD) {
		dhd_bus_dump_fws(dhdp->bus, strbuf);
	}
#endif /* FW_SIGNATURE */

	if (dhdp->busstate != DHD_BUS_DATA)
		return;

#ifdef DHD_SSSR_DUMP
	if (dhdp->bus->sssr_in_progress) {
		DHD_ERROR_RLMT(("%s: SSSR in progress, skip\n", __FUNCTION__));
		return;
	}
#endif /* DHD_SSSR_DUMP */

#ifdef EWP_DACS
	bcm_bprintf(strbuf, "pcie_hwhdr_rev = %u\n", dhdp->bus->ewp_hw_info.pcie_hwhdr_rev);
#endif /* EWP_DACS */
#ifdef DHD_WAKE_STATUS
	bcm_bprintf(strbuf, "wake %u rxwake %u readctrlwake %u\n",
		bcmpcie_get_total_wake(dhdp->bus), dhdp->bus->wake_counts.rxwake,
		dhdp->bus->wake_counts.rcwake);
#ifdef DHD_WAKE_RX_STATUS
	bcm_bprintf(strbuf, " unicast %u muticast %u broadcast %u arp %u\n",
		dhdp->bus->wake_counts.rx_ucast, dhdp->bus->wake_counts.rx_mcast,
		dhdp->bus->wake_counts.rx_bcast, dhdp->bus->wake_counts.rx_arp);
	bcm_bprintf(strbuf, " multi4 %u multi6 %u icmp %u icmp6 %u multiother %u\n",
		dhdp->bus->wake_counts.rx_multi_ipv4, dhdp->bus->wake_counts.rx_multi_ipv6,
		dhdp->bus->wake_counts.rx_icmp, dhdp->bus->wake_counts.rx_icmpv6,
		dhdp->bus->wake_counts.rx_multi_other);
	bcm_bprintf(strbuf, " icmp6_ra %u, icmp6_na %u, icmp6_ns %u\n",
		dhdp->bus->wake_counts.rx_icmpv6_ra, dhdp->bus->wake_counts.rx_icmpv6_na,
		dhdp->bus->wake_counts.rx_icmpv6_ns);
#endif /* DHD_WAKE_RX_STATUS */
#ifdef DHD_WAKE_EVENT_STATUS
#ifdef CUSTOM_WAKE_REASON_STATS
	bcm_bprintf(strbuf, "rc_event_idx = %d, which indicates queue head\n",
		dhdp->bus->wake_counts.rc_event_idx);
	for (flowid = 0; flowid < MAX_WAKE_REASON_STATS; flowid++)
		if (dhdp->bus->wake_counts.rc_event[flowid] != -1)
#else
	for (flowid = 0; flowid < WLC_E_LAST; flowid++)
		if (dhdp->bus->wake_counts.rc_event[flowid] != 0)
#endif /* CUSTOM_WAKE_REASON_STATS */
			bcm_bprintf(strbuf, " %s = %u\n", bcmevent_get_name(flowid),
				dhdp->bus->wake_counts.rc_event[flowid]);
	bcm_bprintf(strbuf, "\n");
#endif /* DHD_WAKE_EVENT_STATUS */
#endif /* DHD_WAKE_STATUS */

#ifdef DHD_TREAT_D3ACKTO_AS_LINKDWN
	if (!dhdp->no_pcie_access_during_dump) {
		dhd_dump_intr_registers(dhdp, strbuf);
	} else {
		DHD_PRINT(("%s: no_pcie_access_during_dump is set,"
			" don't dump intr regs\n", __FUNCTION__));
	}
#else
	dhd_dump_intr_registers(dhdp, strbuf);
#endif /* DHD_TREAT_D3ACKTO_AS_LINKDWN */
	dhd_dump_intr_counters(dhdp, strbuf);
	bcm_bprintf(strbuf, "h2d_mb_data_ptr_addr 0x%x, d2h_mb_data_ptr_addr 0x%x\n",
		dhdp->bus->h2d_mb_data_ptr_addr, dhdp->bus->d2h_mb_data_ptr_addr);
	bcm_bprintf(strbuf, "dhd cumm_ctr %d\n", DHD_CUMM_CTR_READ(&dhdp->cumm_ctr));
	if (dhdp->htput_support) {
		bcm_bprintf(strbuf, "htput_flow_ring_start:%d total_htput:%d client_htput=%d\n",
			dhdp->htput_flow_ring_start, dhdp->htput_total_flowrings,
			dhdp->htput_client_flow_rings);
	}
	bcm_bprintf(strbuf, "D3 inform cnt %d\n", dhdp->bus->d3_inform_cnt);
	bcm_bprintf(strbuf, "D0 inform cnt %d\n", dhdp->bus->d0_inform_cnt);
	bcm_bprintf(strbuf, "D0 inform in use cnt %d\n", dhdp->bus->d0_inform_in_use_cnt);
	if (dhdp->d2h_hostrdy_supported) {
		bcm_bprintf(strbuf, "hostready count:%d\n", dhdp->bus->hostready_count);
	}
#ifdef DHD_TREAT_D3ACKTO_AS_LINKDWN
	bcm_bprintf(strbuf, "d3ackto_as_linkdwn_cnt: %d\n", dhdp->bus->d3ackto_as_linkdwn_cnt);
	bcm_bprintf(strbuf, "iovarto_as_linkdwn_cnt: %d\n", dhdp->bus->iovarto_as_linkdwn_cnt);
#endif

#ifdef PCIE_INB_DW
	/* Inband device wake counters */
	if (INBAND_DW_ENAB(dhdp->bus)) {
		bcm_bprintf(strbuf, "Inband device_wake assert count: %d\n",
			dhdp->bus->inband_dw_assert_cnt);
		bcm_bprintf(strbuf, "Inband device_wake deassert count: %d\n",
			dhdp->bus->inband_dw_deassert_cnt);
		bcm_bprintf(strbuf, "Inband DS-EXIT <host initiated> count: %d\n",
			dhdp->bus->inband_ds_exit_host_cnt);
		bcm_bprintf(strbuf, "Inband DS-EXIT <device initiated> count: %d\n",
			dhdp->bus->inband_ds_exit_device_cnt);
		bcm_bprintf(strbuf, "Inband DS-EXIT Timeout count: %d\n",
			dhdp->bus->inband_ds_exit_to_cnt);
		bcm_bprintf(strbuf, "Inband HOST_SLEEP-EXIT Timeout count: %d\n",
			dhdp->bus->inband_host_sleep_exit_to_cnt);
	}
#endif /* PCIE_INB_DW */
	bcm_bprintf(strbuf, "d2h_intr_method -> %s d2h_intr_control -> %s\n",
		dhdp->bus->d2h_intr_method ? "PCIE_MSI" : "PCIE_INTX",
		dhdp->bus->d2h_intr_control ? "HOST_IRQ" : "D2H_INTMASK");

	bcm_bprintf(strbuf, "\n\nDB7 stats - db7_send_cnt: %d, db7_trap_cnt: %d, "
		"max duration: %lld (%lld - %lld), db7_timing_error_cnt: %d\n",
		dhdp->db7_trap.debug_db7_send_cnt,
		dhdp->db7_trap.debug_db7_trap_cnt,
		dhdp->db7_trap.debug_max_db7_dur,
		dhdp->db7_trap.debug_max_db7_trap_time,
		dhdp->db7_trap.debug_max_db7_send_time,
		dhdp->db7_trap.debug_db7_timing_error_cnt);

	bcm_bprintf(strbuf, "Boot interrupt received:%s\n",
		dhdp->bus->fw_boot_intr ? "Yes":"NO");
	bcm_bprintf(strbuf, "ltr_active_set_during_init: %s\n",
		dhdp->bus->ltr_active_set_during_init ? "Yes":"NO");

	dhd_prot_print_info(dhdp, strbuf);
#ifdef BCMDBG
	if (!dhdp->d2h_sync_mode) {
		ix = 0;
		bcm_bprintf(strbuf, "\n%4s %4s %2s %10s %7s %6s %5s %5s %10s %7s %7s %7s %7s %7s\n",
			"Num:", "Flow", "If", "     ACKED", "D11SPRS", "WLSPRS", "TSDWL",
			"NOACK", "SPRS_ACKED", "EXPIRED", "DROPPED", "FWFREED",
			"SPRS_RETRY", "FORCED_EXPIRED");
		for (flowid = 0; flowid < dhdp->num_h2d_rings; flowid++) {
			flow_ring_node = DHD_FLOW_RING(dhdp, flowid);
			if (!flow_ring_node->active)
				continue;

			flow_info = &flow_ring_node->flow_info;
			bcm_bprintf(strbuf, "%4d %4d %2d ",
				ix++, flow_ring_node->flowid, flow_info->ifindex);
			local_flow_info = &flow_ring_node->flow_info;
			bcm_bprintf(strbuf, "%10d %7d %6d %5d %5d %10d %7d %7d %7d %7d %7d\n",
				local_flow_info->tx_status[WLFC_CTL_PKTFLAG_DISCARD],
				local_flow_info->tx_status[WLFC_CTL_PKTFLAG_D11SUPPRESS],
				local_flow_info->tx_status[WLFC_CTL_PKTFLAG_WLSUPPRESS],
				local_flow_info->tx_status[WLFC_CTL_PKTFLAG_TOSSED_BYWLC],
				local_flow_info->tx_status[WLFC_CTL_PKTFLAG_DISCARD_NOACK],
				local_flow_info->tx_status[WLFC_CTL_PKTFLAG_SUPPRESS_ACKED],
				local_flow_info->tx_status[WLFC_CTL_PKTFLAG_EXPIRED],
				local_flow_info->tx_status[WLFC_CTL_PKTFLAG_DROPPED],
				local_flow_info->tx_status[WLFC_CTL_PKTFLAG_MKTFREE],
				local_flow_info->tx_status[WLFC_CTL_PKTFLAG_MAX_SUP_RETR],
				local_flow_info->tx_status[WLFC_CTL_PKTFLAG_FORCED_EXPIRED]);
		}
	}
#endif /* BCMDBG */

	dhd_bus_dump_flowring(dhdp, strbuf);

	dhd_dump_dpc_histos(dhdp, strbuf);
	dhd_prot_print_traces(dhdp, strbuf);
}

int
dhd_dump_flowrings(dhd_pub_t *dhdp, char *buf, int buflen)
{
	struct bcmstrbuf b;
	struct bcmstrbuf *strbuf = &b;

	if (!dhdp || !dhdp->prot || !buf) {
		return BCME_ERROR;
	}

	bcm_binit(strbuf, buf, buflen);
	dhd_bus_dump_flowring(dhdp, strbuf);
	return (!strbuf->size ? BCME_BUFTOOSHORT : strbuf->size);
}

static const char *
dhd_bus_bandname(uint slice)
{
	static const char *slice_to_bandname[] = {"2G", "5G", "6G"};

	if (slice >= ARRAYSIZE(slice_to_bandname)) {
		DHD_ERROR(("%s:Wrong slice:%d\n", __FUNCTION__, slice));
		ASSERT(0);
		return "None";
	}
	return slice_to_bandname[slice];
}

void
dhd_bus_dump_txcpl_info(dhd_pub_t *dhdp, struct bcmstrbuf *strbuf)
{
	tx_cpl_info_t *txcpl_info = &dhdp->txcpl_info;
	int i;

	bcm_bprintf(strbuf, "\nTx Completion History\n");
	bcm_bprintf(strbuf, "Host(us)\t\tPTM_high(ns)\t\tPTM_low(ns)\t\t"
		"Latency(ms)\t\tTID\t\tFlowID\t\tProto\t\tTuple_1\t\tTuple_2\n");
	for (i = 0; i < MAX_TXCPL_HISTORY; i++) {
		bcm_bprintf(strbuf, "0x%x\t\t0x%x\t\t0x%x\t\t%u\t\t%u\t\t%u\t\t%d\t\t%d\t\t%d\n",
			txcpl_info->tx_history[i].host_time,
			txcpl_info->tx_history[i].ptm_high,
			txcpl_info->tx_history[i].ptm_low,
			txcpl_info->tx_history[i].latency,
			txcpl_info->tx_history[i].tid,
			txcpl_info->tx_history[i].flowid,
			txcpl_info->tx_history[i].proto,
			txcpl_info->tx_history[i].tuple_1,
			txcpl_info->tx_history[i].tuple_2);
	}
	bzero(txcpl_info->tx_history,
		sizeof(tx_cpl_history_t) * MAX_TXCPL_HISTORY);

	bcm_bprintf(strbuf, "\n");
}

void
dhd_bus_dump_mdring_info(dhd_pub_t *dhdp, struct bcmstrbuf *strbuf)
{
	int i;
	int count;

	bcm_bprintf(strbuf, "\nMetadata Ring Dump\n");
	for (i = 0, count = 0;
		i < MAX_MDRING_ITEM_DUMP * D2HRING_MDCMPLT_ITEMSIZE; i += 4) {
		if ((i % D2HRING_MDCMPLT_ITEMSIZE) == 0) {
			bcm_bprintf(strbuf, "\nEntry:%d:", ++count);
		}
		bcm_bprintf(strbuf, "0x%x%x%x%x:",
			dhdp->mdring_info[i], dhdp->mdring_info[i+1],
			dhdp->mdring_info[i+2], dhdp->mdring_info[i+3]);
	}
	bcm_bprintf(strbuf, "\n");
}

void
dhd_bus_dump_rxlat_info(dhd_pub_t *dhdp, struct bcmstrbuf *strbuf)
{
	if (dhdp->rx_cpl_lat_capable) {
		rx_cpl_lat_info_t *rxcpl_info = &dhdp->rxcpl_lat_info;
		int i;

		bcm_bprintf(strbuf, "\nRx Completion History\n");
		bcm_bprintf(strbuf, "Host(us)\t\tPTM_high(ns)\t\tPTM_low(ns)\t\tRspec\tTstamp\tBand"
			"\t\tPrio\t\tRSSI\t\tLatency(us)\t\tProto\t\tTuple_1\t\tTuple_2\n");
		for (i = 0; i < MAX_RXCPL_HISTORY; i++) {
			if (rxcpl_info->rx_history[i].rx_t1 || rxcpl_info->rx_history[i].ptm_low) {
				bcm_bprintf(strbuf,
					"0x%x\t\t0x%x\t\t0x%x\t\t0x%x\t0x%x\t\t%s\t\t%d\t\t%d"
					"\t\t%d\t\t%d\t\t%d\t\t%d\n",
					rxcpl_info->rx_history[i].host_time,
					rxcpl_info->rx_history[i].ptm_high,
					rxcpl_info->rx_history[i].ptm_low,
					rxcpl_info->rx_history[i].rx_t0,
					rxcpl_info->rx_history[i].rx_t1,
					dhd_bus_bandname(rxcpl_info->rx_history[i].slice),
					rxcpl_info->rx_history[i].priority,
					rxcpl_info->rx_history[i].rssi,
					rxcpl_info->rx_history[i].latency,
					rxcpl_info->rx_history[i].proto,
					rxcpl_info->rx_history[i].tuple_1,
					rxcpl_info->rx_history[i].tuple_2);
			}
		}
		bzero(rxcpl_info->rx_history,
			sizeof(rx_cpl_history_t) * MAX_RXCPL_HISTORY);
	}
	bcm_bprintf(strbuf, "\n");
}

void
dhd_bus_dump_rxlat_histo(dhd_pub_t *dhdp, struct bcmstrbuf *strbuf)
{
	int bin, prio;

	if (dhdp->rx_cpl_lat_capable) {
		rx_cpl_lat_info_t *rxcpl_info = &dhdp->rxcpl_lat_info;

		bcm_bprintf(strbuf,
			"\nRx:5G Latency histogram, each bin:%d us\n", RX_LAT_BIN_SCALE);
		bcm_bprintf(strbuf,
			"Bin\tPrio-0\tPrio-1\tPrio-2\tPrio-3\tPrio-4\tPrio-5\tPrio-6\tPrio-7\n");
		for (bin = 0; bin < MAX_RX_LAT_HIST_BIN; bin++) {
			bcm_bprintf(strbuf, "\n%d", bin);
			for (prio = 0; prio < MAX_RX_LAT_PRIO; prio++) {
				bcm_bprintf(strbuf, "\t");
				if (rxcpl_info->rx_dur_5g[prio][bin]) {
					bcm_bprintf(strbuf, "%d",
						rxcpl_info->rx_dur_5g[prio][bin]);
				}
			}
		}
		bcm_bprintf(strbuf,
			"\nRx:2G Latency histogram, each bin:%d us\n", RX_LAT_BIN_SCALE);
		bcm_bprintf(strbuf,
			"Bin\tPrio-0\tPrio-1\tPrio-2\tPrio-3\tPrio-4\tPrio-5\tPrio-6\tPrio-7\n");
		for (bin = 0; bin < MAX_RX_LAT_HIST_BIN; bin++) {
			bcm_bprintf(strbuf, "\n%d", bin);
			for (prio = 0; prio < MAX_RX_LAT_PRIO; prio++) {
				bcm_bprintf(strbuf, "\t");
				if (rxcpl_info->rx_dur_2g[prio][bin]) {
					bcm_bprintf(strbuf, "%d",
						rxcpl_info->rx_dur_2g[prio][bin]);
				}
			}
		}
		bcm_bprintf(strbuf,
			"\nRx:6G Latency histogram, each bin:%d us\n", RX_LAT_BIN_SCALE);
		bcm_bprintf(strbuf,
			"Bin\tPrio-0\tPrio-1\tPrio-2\tPrio-3\tPrio-4\tPrio-5\tPrio-6\tPrio-7\n");
		for (bin = 0; bin < MAX_RX_LAT_HIST_BIN; bin++) {
			bcm_bprintf(strbuf, "\n%d", bin);
			for (prio = 0; prio < MAX_RX_LAT_PRIO; prio++) {
				bcm_bprintf(strbuf, "\t");
				if (rxcpl_info->rx_dur_6g[prio][bin]) {
					bcm_bprintf(strbuf, "%d",
						rxcpl_info->rx_dur_6g[prio][bin]);
				}
			}
		}
		bzero(rxcpl_info->rx_dur_5g, MAX_RX_LAT_PRIO * MAX_RX_LAT_HIST_BIN);
		bzero(rxcpl_info->rx_dur_2g, MAX_RX_LAT_PRIO * MAX_RX_LAT_HIST_BIN);
		bzero(rxcpl_info->rx_dur_6g, MAX_RX_LAT_PRIO * MAX_RX_LAT_HIST_BIN);
	}
	bcm_bprintf(strbuf, "\n");
}

#ifdef DNGL_AXI_ERROR_LOGGING
void
dhdpcie_dump_axi_error(uint8 *axi_err)
{
	dma_dentry_v1_t dma_dentry;
	dma_fifo_v1_t dma_fifo;
	int i = 0, j = 0;

	if (*(uint8 *)axi_err == HND_EXT_TRAP_AXIERROR_VERSION_1) {
		hnd_ext_trap_axi_error_v1_t *axi_err_v1 = (hnd_ext_trap_axi_error_v1_t *)axi_err;
		DHD_PRINT(("%s: signature : 0x%x\n", __FUNCTION__, axi_err_v1->signature));
		DHD_PRINT(("%s: version : 0x%x\n", __FUNCTION__, axi_err_v1->version));
		DHD_PRINT(("%s: length : 0x%x\n", __FUNCTION__, axi_err_v1->length));
		DHD_PRINT(("%s: dma_fifo_valid_count : 0x%x\n",
			__FUNCTION__, axi_err_v1->dma_fifo_valid_count));
		DHD_PRINT(("%s: axi_errorlog_status : 0x%x\n",
			__FUNCTION__, axi_err_v1->axi_errorlog_status));
		DHD_PRINT(("%s: axi_errorlog_core : 0x%x\n",
			__FUNCTION__, axi_err_v1->axi_errorlog_core));
		DHD_PRINT(("%s: axi_errorlog_hi : 0x%x\n",
			__FUNCTION__, axi_err_v1->axi_errorlog_hi));
		DHD_PRINT(("%s: axi_errorlog_lo : 0x%x\n",
			__FUNCTION__, axi_err_v1->axi_errorlog_lo));
		DHD_PRINT(("%s: axi_errorlog_id : 0x%x\n",
			__FUNCTION__, axi_err_v1->axi_errorlog_id));

		for (i = 0; i < MAX_DMAFIFO_ENTRIES_V1; i++) {
			dma_fifo = axi_err_v1->dma_fifo[i];
			DHD_PRINT(("%s: valid:%d : 0x%x\n",
				__FUNCTION__, i, dma_fifo.valid));
			DHD_PRINT(("%s: direction:%d : 0x%x\n",
				__FUNCTION__, i, dma_fifo.direction));
			DHD_PRINT(("%s: index:%d : 0x%x\n",
				__FUNCTION__, i, dma_fifo.index));
			DHD_PRINT(("%s: dpa:%d : 0x%x\n",
				__FUNCTION__, i, dma_fifo.dpa));
			DHD_PRINT(("%s: desc_lo:%d : 0x%x\n",
				__FUNCTION__, i, dma_fifo.desc_lo));
			DHD_PRINT(("%s: desc_hi:%d : 0x%x\n",
				__FUNCTION__, i, dma_fifo.desc_hi));
			DHD_PRINT(("%s: din:%d : 0x%x\n",
				__FUNCTION__, i, dma_fifo.din));
			DHD_PRINT(("%s: dout:%d : 0x%x\n",
				__FUNCTION__, i, dma_fifo.dout));
			for (j = 0; j < MAX_DMAFIFO_DESC_ENTRIES_V1; j++) {
				dma_dentry = axi_err_v1->dma_fifo[i].dentry[j];
				DHD_PRINT(("%s: ctrl1:%d : 0x%x\n",
					__FUNCTION__, i, dma_dentry.ctrl1));
				DHD_PRINT(("%s: ctrl2:%d : 0x%x\n",
					__FUNCTION__, i, dma_dentry.ctrl2));
				DHD_PRINT(("%s: addrlo:%d : 0x%x\n",
					__FUNCTION__, i, dma_dentry.addrlo));
				DHD_PRINT(("%s: addrhi:%d : 0x%x\n",
					__FUNCTION__, i, dma_dentry.addrhi));
			}
		}
	} else {
		DHD_ERROR(("%s: Invalid AXI version: 0x%x\n", __FUNCTION__, (*(uint8 *)axi_err)));
	}
}
#endif /* DNGL_AXI_ERROR_LOGGING */

void
dhd_pcie_dump_core_regs(dhd_pub_t *pub, uint32 index, uint32 first_addr, uint32 last_addr)
{
	dhd_bus_t *bus = pub->bus;
	uint32	coreoffset = index << 12;
	uint32	core_addr = SI_ENUM_BASE(bus->sih) + coreoffset;
	uint32 value;

	while (first_addr <= last_addr) {
		core_addr = SI_ENUM_BASE(bus->sih) + coreoffset + first_addr;
		if (serialized_backplane_access(bus, core_addr, 4, &value, TRUE) != BCME_OK) {
			DHD_ERROR(("Invalid size/addr combination \n"));
		}
		DHD_PRINT(("[0x%08x]: 0x%08x\n", core_addr, value));
		first_addr = first_addr + 4;
	}
}

#ifdef DHD_MMIO_TRACE
void
dhd_bus_mmio_trace(dhd_bus_t *bus, uint32 addr, uint32 value, bool set)
{
	uint32 cnt = bus->mmio_trace_count % MAX_MMIO_TRACE_SIZE;
	bus->mmio_trace[cnt].timestamp = OSL_LOCALTIME_NS();
	bus->mmio_trace[cnt].addr = addr;
	bus->mmio_trace[cnt].set = set;
	bus->mmio_trace[cnt].value = value;
	bus->mmio_trace_count++;
}

void
dhd_dump_bus_mmio_trace(dhd_bus_t *bus, struct bcmstrbuf *strbuf)
{
	int dumpsz;
	int i;

	dumpsz = bus->mmio_trace_count < MAX_MMIO_TRACE_SIZE ?
		bus->mmio_trace_count : MAX_MMIO_TRACE_SIZE;
	if (dumpsz == 0) {
		bcm_bprintf(strbuf, "\nEmpty MMIO TRACE\n");
		return;
	}
	bcm_bprintf(strbuf, "---- MMIO TRACE ------\n");
	bcm_bprintf(strbuf, "Timestamp ns\t\tAddr\t\tW/R\tValue\n");
	for (i = 0; i < dumpsz; i++) {
		bcm_bprintf(strbuf, SEC_USEC_FMT"\t0x%08x\t%s\t0x%08x\n",
			GET_SEC_USEC(bus->mmio_trace[i].timestamp),
			bus->mmio_trace[i].addr,
			bus->mmio_trace[i].set ? "W" : "R",
			bus->mmio_trace[i].value);
	}
}
#endif /* defined(DHD_MMIO_TRACE) */

void
#ifdef PCIE_INB_DW
dhd_bus_ds_trace(dhd_bus_t *bus, uint32 dsval, bool d2h,
	enum dhd_bus_ds_state inbstate, const char *context)
#else
dhd_bus_ds_trace(dhd_bus_t *bus, uint32 dsval, bool d2h)
#endif /* PCIE_INB_DW */
{
	uint32 cnt = bus->ds_trace_count % MAX_DS_TRACE_SIZE;

	bus->ds_trace[cnt].timestamp = OSL_LOCALTIME_NS();
	bus->ds_trace[cnt].d2h = d2h;
	bus->ds_trace[cnt].dsval = dsval;
#ifdef PCIE_INB_DW
	bus->ds_trace[cnt].inbstate = inbstate;
	snprintf(bus->ds_trace[cnt].context, sizeof(bus->ds_trace[cnt].context), "%s", context);
#endif /* PCIE_INB_DW */
	bus->ds_trace_count++;
}

void
dhd_dump_bus_ds_trace(dhd_bus_t *bus, struct bcmstrbuf *strbuf)
{
	int dumpsz;
	int i;

	dumpsz = bus->ds_trace_count < MAX_DS_TRACE_SIZE ?
		bus->ds_trace_count : MAX_DS_TRACE_SIZE;
	if (dumpsz == 0) {
		bcm_bprintf(strbuf, "\nEmpty DS TRACE\n");
		return;
	}
	bcm_bprintf(strbuf, "---- DS TRACE ------\n");
#ifdef PCIE_INB_DW
	bcm_bprintf(strbuf, "%s %13s %33s %23s %5s\n",
		"Timestamp us", "Dir", "Value", "Inband-State", "Context");
	for (i = 0; i < dumpsz; i++) {
		bcm_bprintf(strbuf, "%llu %13s %33s %23s %5s\n",
		bus->ds_trace[i].timestamp,
		bus->ds_trace[i].d2h ? "D2H":"H2D",
		dhd_convert_dsval(bus->ds_trace[i].dsval, bus->ds_trace[i].d2h),
		dhd_convert_inb_state_names(bus->ds_trace[i].inbstate),
		bus->ds_trace[i].context);
	}
#else
	bcm_bprintf(strbuf, "Timestamp us\t\tDir\tValue\n");
	for (i = 0; i < dumpsz; i++) {
		bcm_bprintf(strbuf, "%llu\t%s\t%d\n",
		bus->ds_trace[i].timestamp,
		bus->ds_trace[i].d2h ? "D2H":"H2D",
		bus->ds_trace[i].dsval);
	}
#endif /* PCIE_INB_DW */
	bcm_bprintf(strbuf, "--------------------------\n");
}

void
dhd_dump_ds_trace_console(dhd_pub_t *dhdp)
{
#if defined(DHD_LOG_DUMP)
	struct bcmstrbuf b;
	struct bcmstrbuf *strbuf = &b;

	bzero(dhdp->concise_dbg_buf, CONCISE_DUMP_BUFLEN);
	bcm_binit(strbuf, (char *)dhdp->concise_dbg_buf, CONCISE_DUMP_BUFLEN);
	bcm_bprintf_bypass = TRUE;
	dhd_dump_bus_ds_trace(dhdp->bus, strbuf);
	bcm_bprintf_bypass = FALSE;
#endif /* DHD_LOG_DUMP */
}

void
dhd_bus_dump_trap_info(dhd_bus_t *bus, struct bcmstrbuf *strbuf)
{
	trap_t *tr = &bus->dhd->last_trap_info;
	bcm_bprintf(strbuf,
		"\nTRAP type 0x%x @ epc 0x%x, cpsr 0x%x, spsr 0x%x, sp 0x%x,"
		" lp 0x%x, rpc 0x%x"
		"\nTrap offset 0x%x, r0 0x%x, r1 0x%x, r2 0x%x, r3 0x%x, "
		"r4 0x%x, r5 0x%x, r6 0x%x, r7 0x%x, r8 0x%x, r9 0x%x, "
		"r10 0x%x, r11 0x%x, r12 0x%x\n\n",
		ltoh32(tr->type), ltoh32(tr->epc), ltoh32(tr->cpsr), ltoh32(tr->spsr),
		ltoh32(tr->r13), ltoh32(tr->r14), ltoh32(tr->pc),
		ltoh32(bus->pcie_sh->trap_addr),
		ltoh32(tr->r0), ltoh32(tr->r1), ltoh32(tr->r2), ltoh32(tr->r3),
		ltoh32(tr->r4), ltoh32(tr->r5), ltoh32(tr->r6), ltoh32(tr->r7),
		ltoh32(tr->r8), ltoh32(tr->r9), ltoh32(tr->r10),
		ltoh32(tr->r11), ltoh32(tr->r12));
}

void
dhd_pcie_intr_count_dump(dhd_pub_t *dhd)
{
	struct dhd_bus *bus = dhd->bus;
	uint64 current_time;

	DHD_PRINT(("\n ------- DUMPING INTR enable/disable counters  ------- \r\n"));
	DHD_PRINT(("dngl_intmask_enable_count=%lu host_irq_enable_count=%lu\n",
		bus->dngl_intmask_enable_count, bus->host_irq_enable_count));
	DHD_PRINT(("host_irq_disable_count=%lu dngl_intmask_disable_count=%lu\n",
		bus->host_irq_disable_count, bus->dngl_intmask_disable_count));
	DHD_PRINT(("rot_dpc_sched_count=%d\n", bus->rot_dpc_sched_count));
#ifdef BCMPCIE_OOB_HOST_WAKE
	DHD_PRINT(("oob_intr_count=%lu oob_intr_enable_count=%lu oob_intr_disable_count=%lu\n",
		bus->oob_intr_count, bus->oob_intr_enable_count,
		bus->oob_intr_disable_count));
	DHD_PRINT(("oob_irq_num=%d last_oob_irq_times="SEC_USEC_FMT":"SEC_USEC_FMT"\n",
		dhdpcie_get_oob_irq_num(bus),
		GET_SEC_USEC(bus->last_oob_irq_isr_time),
		GET_SEC_USEC(bus->last_oob_irq_thr_time)));
	DHD_PRINT(("last_oob_irq_enable_time="SEC_USEC_FMT
		" last_oob_irq_disable_time="SEC_USEC_FMT"\n",
		GET_SEC_USEC(bus->last_oob_irq_enable_time),
		GET_SEC_USEC(bus->last_oob_irq_disable_time)));
	DHD_PRINT(("oob_irq_enabled=%d oob_gpio_level=%d\n",
		dhdpcie_get_oob_irq_status(bus),
		dhdpcie_get_oob_irq_level(bus)));

#if defined(__linux__)
	dhd_plat_pin_dbg_show(bus->dhd->plat_info);
#endif /* __linux__ */
#endif /* BCMPCIE_OOB_HOST_WAKE */
	DHD_PRINT(("dpc_return_busdown_count=%lu non_ours_irq_count=%lu\n",
		bus->dpc_return_busdown_count, bus->non_ours_irq_count));

	current_time = OSL_LOCALTIME_NS();
	DHD_PRINT(("\ncurrent_time="SEC_USEC_FMT"\n",
		GET_SEC_USEC(current_time)));
	DHD_PRINT(("isr_entry_time="SEC_USEC_FMT
		" isr_exit_time="SEC_USEC_FMT" dpc_sched=%u\n",
		GET_SEC_USEC(bus->isr_entry_time),
		GET_SEC_USEC(bus->isr_exit_time), bus->dpc_sched));
	DHD_PRINT(("isr_sched_dpc_time="SEC_USEC_FMT
		" rpm_sched_dpc_time="SEC_USEC_FMT
		" last_non_ours_irq_time="SEC_USEC_FMT"\n",
		GET_SEC_USEC(bus->isr_sched_dpc_time),
		GET_SEC_USEC(bus->rpm_sched_dpc_time),
		GET_SEC_USEC(bus->last_non_ours_irq_time)));
	DHD_PRINT(("dpc_entry_time="SEC_USEC_FMT
		" last_process_ctrlbuf_time="SEC_USEC_FMT"\n",
		GET_SEC_USEC(bus->dpc_entry_time),
		GET_SEC_USEC(bus->last_process_ctrlbuf_time)));
	DHD_PRINT(("last_process_flowring_time="SEC_USEC_FMT
		" last_process_txcpl_time="SEC_USEC_FMT"\n",
		GET_SEC_USEC(bus->last_process_flowring_time),
		GET_SEC_USEC(bus->last_process_txcpl_time)));
	DHD_PRINT(("last_process_rxcpl_time="SEC_USEC_FMT
		" last_process_infocpl_time="SEC_USEC_FMT
		" last_process_edl_time="SEC_USEC_FMT"\n",
		GET_SEC_USEC(bus->last_process_rxcpl_time),
		GET_SEC_USEC(bus->last_process_infocpl_time),
		GET_SEC_USEC(bus->last_process_edl_time)));
	DHD_PRINT(("dpc_exit_time="SEC_USEC_FMT
		" resched_dpc_time="SEC_USEC_FMT"\n",
		GET_SEC_USEC(bus->dpc_exit_time),
		GET_SEC_USEC(bus->resched_dpc_time)));
	DHD_PRINT(("last_d3_inform_time="SEC_USEC_FMT"\n",
		GET_SEC_USEC(bus->last_d3_inform_time)));

	DHD_PRINT(("\nlast_suspend_start_time="SEC_USEC_FMT
		" last_suspend_end_time="SEC_USEC_FMT"\n",
		GET_SEC_USEC(bus->last_suspend_start_time),
		GET_SEC_USEC(bus->last_suspend_end_time)));
	DHD_PRINT(("last_resume_start_time="SEC_USEC_FMT
		" last_resume_end_time="SEC_USEC_FMT"\n",
		GET_SEC_USEC(bus->last_resume_start_time),
		GET_SEC_USEC(bus->last_resume_end_time)));

#if defined(SHOW_LOGTRACE) && defined(DHD_USE_KTHREAD_FOR_LOGTRACE)
	DHD_PRINT(("logtrace_thread_entry_time="SEC_USEC_FMT
		" logtrace_thread_sem_down_time="SEC_USEC_FMT
		"\nlogtrace_thread_flush_time="SEC_USEC_FMT
		" logtrace_thread_unexpected_break_time="SEC_USEC_FMT
		"\nlogtrace_thread_complete_time="SEC_USEC_FMT"\n",
		GET_SEC_USEC(dhd->logtrace_thr_ts.entry_time),
		GET_SEC_USEC(dhd->logtrace_thr_ts.sem_down_time),
		GET_SEC_USEC(dhd->logtrace_thr_ts.flush_time),
		GET_SEC_USEC(dhd->logtrace_thr_ts.unexpected_break_time),
		GET_SEC_USEC(dhd->logtrace_thr_ts.complete_time)));
#endif /* SHOW_LOGTRACE && DHD_USE_KTHREAD_FOR_LOGTRACE */
}

void
dhd_bus_intr_count_dump(dhd_pub_t *dhd)
{
	dhd_pcie_intr_count_dump(dhd);
}

#ifdef DHD_PCIE_WRAPPER_DUMP
void
dhd_pcie_get_wrapper_regs(dhd_pub_t *dhd)
{
	uint wrapper_base_len = 0;
	uint wrapper_offset_len = 0;
	uint wrapper_core, wrapper_reg;
	uint i, j, k;
	uint32 val;
	uint16 chipid;
	dhd_bus_t *bus = dhd->bus;

	struct bcmstrbuf b;
	struct bcmstrbuf *strbuf = &b;

	/* The procedure to read wrapper for SOCI_NCI is different compared to SOCI_AI */
	if (CHIPTYPE(bus->sih->socitype) == SOCI_NCI) {
		return;
	}

	/* TBD: To comeup with generic scheme to support all chips */
	chipid = dhd_get_chipid(bus);
	if (chipid != BCM4388_CHIP_ID) {
		return;
	}

	bcm_binit(strbuf, dhd->dbg->wrapper_buf.buf, dhd->dbg->wrapper_buf.len);
	wrapper_base_len = (uint32)sizeof(wrapper_base_4388) /
		(uint32)sizeof(wrapper_base_4388[0]);
	wrapper_offset_len = (uint32)sizeof(wrapper_offset_4388) /
		(uint32)sizeof(wrapper_offset_4388[0]);

	for (i = 0; i < wrapper_base_len; i++) {
		bcm_bprintf(strbuf, "\n%s <base val1 val2 ..>", wrapper_base_4388[i].core);
		for (j = 0; j < wrapper_offset_len; j++) {
			wrapper_core = wrapper_base_4388[i].base + wrapper_offset_4388[j].offset;
			bcm_bprintf(strbuf, "\n0x%x ", wrapper_core);
			for (k = 0; k < (wrapper_offset_4388[j].len / 4); k++) {
				wrapper_reg = wrapper_core + (k * 4);
				dhd_sbreg_op_silent(dhd, wrapper_reg, &val, TRUE);
				bcm_bprintf(strbuf, "0x%x ", val);
			}
		}

	}
	bcm_bprintf(strbuf, "\n");
	DHD_ERROR(("%s wrapper_buf: %d free: %d\n",
		__FUNCTION__, dhd->dbg->wrapper_buf.len, strbuf->size));

}
#endif /* DHD_PCIE_WRAPPER_DUMP */

int
dhd_pcie_nci_wrapper_dump(dhd_pub_t *dhd, bool dump_to_dmesg)
{
	uint size = 0;
	uint32 pwrval = 0;
	si_t *sih = dhd->bus->sih;
	uint nreg_pairs = 0;
	int i = 0;
	uint32 *reg = 0, *val = 0;
	int ret = 0;

	if (CHIPTYPE(sih->socitype) != SOCI_NCI) {
		return BCME_ERROR;
	}

	/* save current value */
	pwrval = si_srpwr_request(sih, 0, 0);
	pwrval >>= SRPWR_REQON_SHIFT;
	pwrval &= SRPWR_DMN_ALL_MASK(sih);
	/* request power for all domains */
	DHD_PRINT(("%s: req pwr all domains\n", __FUNCTION__));
	si_srpwr_request(sih, SRPWR_DMN_ALL_MASK(sih), SRPWR_DMN_ALL_MASK(sih));

	/* size is in terms of bytes */
	size = si_wrapper_dump_buf_size(sih);
	nreg_pairs = size/8;

	if (!size || size > dhd->dbg->wrapper_buf.len || !dhd->dbg->wrapper_buf.buf) {
		DHD_ERROR(("%s:invalid params! nci wrapper reg dump size (%u bytes),"
			" available bufsize (%u bytes), wrapper_buf=0x%p\n",
			__FUNCTION__, size, dhd->dbg->wrapper_buf.len,
			dhd->dbg->wrapper_buf.buf));
		return BCME_BADARG;
	} else {
		bzero(dhd->dbg->wrapper_buf.buf, size);
		dhd->dbg->wrapper_regdump_size = size;
		if (si_wrapper_dump_binary(sih,
			(uchar *)dhd->dbg->wrapper_buf.buf) == BCME_OK) {
			DHD_ERROR(("%s:read nci wrapper reg dump(%u bytes) success\n",
				__FUNCTION__, size));
			ret = BCME_OK;
		} else {
			DHD_ERROR(("%s: Error reading nci wrapper reg dump !\n",
				__FUNCTION__));
			ret = BCME_ERROR;
		}
	}
	/* restore earlier value */
	DHD_PRINT(("%s: restore prev pwr req val 0x%x \n", __FUNCTION__, pwrval));
	si_srpwr_request(sih, pwrval, pwrval);

	if (ret == BCME_OK) {
		if (dump_to_dmesg) {
			DHD_PRINT(("NCI Wrapper Reg Dump:\n=========================== \n"));
		} else {
			DHD_LOG_MEM(("NCI Wrapper Reg Dump:\n=========================== \n"));
		}
		for (i = 0; i < nreg_pairs; i += 2) {
			reg = (uint32 *)dhd->dbg->wrapper_buf.buf + i;
			val = reg + 1;
			if (dump_to_dmesg) {
				DHD_PRINT(("reg:0x%x = 0x%x\n", *reg, *val));
			} else {
				DHD_LOG_MEM(("reg:0x%x = 0x%x\n", *reg, *val));
			}
		}
	}

	return ret;
}

int
dhd_pcie_dump_wrapper_regs(dhd_pub_t *dhd)
{
	uint32 save_idx, val;
	si_t *sih = dhd->bus->sih;
	dhd_bus_t *bus = dhd->bus;
	uint32 oob_base, oob_base1;
	uint32 wrapper_dump_list[] = {
		AI_OOBSELOUTA30, AI_OOBSELOUTA74, AI_OOBSELOUTB30, AI_OOBSELOUTB74,
		AI_OOBSELOUTC30, AI_OOBSELOUTC74, AI_OOBSELOUTD30, AI_OOBSELOUTD74,
		AI_RESETSTATUS, AI_RESETCTRL,
		AI_ITIPOOBA, AI_ITIPOOBB, AI_ITIPOOBC, AI_ITIPOOBD,
		AI_ITIPOOBAOUT, AI_ITIPOOBBOUT, AI_ITIPOOBCOUT, AI_ITIPOOBDOUT
	};
	uint32 i;
	hndoobr_reg_t *reg;
	cr4regs_t *cr4regs;
	ca7regs_t *ca7regs;

	save_idx = si_coreidx(sih);

	if (CHIPTYPE(sih->socitype) != SOCI_NCI) {
		DHD_PRINT(("%s: Master wrapper Reg\n", __FUNCTION__));
		if (si_setcore(sih, PCIE2_CORE_ID, 0) != NULL) {
			for (i = 0; i < (uint32)sizeof(wrapper_dump_list) / 4; i++) {
				val = si_wrapperreg(sih, wrapper_dump_list[i], 0, 0);
				DHD_PRINT(("sbreg: addr:0x%x val:0x%x\n",
					wrapper_dump_list[i], val));
			}
		}

		dhd_dump_pcie_slave_wrapper_regs(dhd->bus);

		cr4regs = si_setcore(sih, ARMCR4_CORE_ID, 0);
		if (cr4regs != NULL) {
			DHD_ERROR(("%s: ARM CR4 wrapper Reg\n", __FUNCTION__));
			for (i = 0; i < (uint32)sizeof(wrapper_dump_list) / 4; i++) {
				val = si_wrapperreg(sih, wrapper_dump_list[i], 0, 0);
				DHD_ERROR(("sbreg: addr:0x%x val:0x%x\n",
					wrapper_dump_list[i], val));
			}
			DHD_ERROR(("%s: ARM CR4 core Reg\n", __FUNCTION__));
			val = R_REG(dhd->osh, ARM_CR4_REG(cr4regs, corecontrol));
			DHD_ERROR(("reg:0x%x val:0x%x\n",
				(uint)OFFSETOF(cr4regs_t, corecontrol), val));
			val = R_REG(dhd->osh, ARM_CR4_REG(cr4regs, corecapabilities));
			DHD_ERROR(("reg:0x%x val:0x%x\n",
				(uint)OFFSETOF(cr4regs_t, corecapabilities), val));
			val = R_REG(dhd->osh, ARM_CR4_REG(cr4regs, corestatus));
			DHD_ERROR(("reg:0x%x val:0x%x\n",
				(uint)OFFSETOF(cr4regs_t, corestatus), val));
			val = R_REG(dhd->osh, ARM_CR4_REG(cr4regs, nmiisrst));
			DHD_ERROR(("reg:0x%x val:0x%x\n",
				(uint)OFFSETOF(cr4regs_t, nmiisrst), val));
			val = R_REG(dhd->osh, ARM_CR4_REG(cr4regs, nmimask));
			DHD_ERROR(("reg:0x%x val:0x%x\n",
				(uint)OFFSETOF(cr4regs_t, nmimask), val));
			val = R_REG(dhd->osh, ARM_CR4_REG(cr4regs, isrmask));
			DHD_ERROR(("reg:0x%x val:0x%x\n",
				(uint)OFFSETOF(cr4regs_t, isrmask), val));
			val = R_REG(dhd->osh, ARM_CR4_REG(cr4regs, swintreg));
			DHD_ERROR(("reg:0x%x val:0x%x\n",
				(uint)OFFSETOF(cr4regs_t, swintreg), val));
			val = R_REG(dhd->osh, ARM_CR4_REG(cr4regs, intstatus));
			DHD_ERROR(("reg:0x%x val:0x%x\n",
				(uint)OFFSETOF(cr4regs_t, intstatus), val));
			val = R_REG(dhd->osh, ARM_CR4_REG(cr4regs, cyclecnt));
			DHD_ERROR(("reg:0x%x val:0x%x\n",
				(uint)OFFSETOF(cr4regs_t, cyclecnt), val));
			val = R_REG(dhd->osh, ARM_CR4_REG(cr4regs, inttimer));
			DHD_ERROR(("reg:0x%x val:0x%x\n",
				(uint)OFFSETOF(cr4regs_t, inttimer), val));
			val = R_REG(dhd->osh, ARM_CR4_REG(cr4regs, clk_ctl_st));
			DHD_ERROR(("reg:0x%x val:0x%x\n",
				(uint)OFFSETOF(cr4regs_t, clk_ctl_st), val));
			val = R_REG(dhd->osh, ARM_CR4_REG(cr4regs, powerctl));
			DHD_ERROR(("reg:0x%x val:0x%x\n",
				(uint)OFFSETOF(cr4regs_t, powerctl), val));
		}
		/* Currently dumping CA7 registers causing CTO, temporarily disabling it */
		BCM_REFERENCE(ca7regs);
#ifdef NOT_YET
		ca7regs = si_setcore(sih, ARMCA7_CORE_ID, 0);
		if (ca7regs != NULL) {
			DHD_ERROR(("%s: ARM CA7 core Reg\n", __FUNCTION__));
			val = R_REG(dhd->osh, ARM_CA7_REG(ca7regs, corecontrol));
			DHD_ERROR(("reg:0x%x val:0x%x\n",
				(uint)OFFSETOF(ca7regs_t, corecontrol), val));
			val = R_REG(dhd->osh, ARM_CA7_REG(ca7regs, corecapabilities));
			DHD_ERROR(("reg:0x%x val:0x%x\n",
				(uint)OFFSETOF(ca7regs_t, corecapabilities), val));
			val = R_REG(dhd->osh, ARM_CA7_REG(ca7regs, corestatus));
			DHD_ERROR(("reg:0x%x val:0x%x\n",
				(uint)OFFSETOF(ca7regs_t, corestatus), val));
			val = R_REG(dhd->osh, ARM_CA7_REG(ca7regs, tracecontrol));
			DHD_ERROR(("reg:0x%x val:0x%x\n",
				(uint)OFFSETOF(ca7regs_t, tracecontrol), val));
			val = R_REG(dhd->osh, ARM_CA7_REG(ca7regs, clk_ctl_st));
			DHD_ERROR(("reg:0x%x val:0x%x\n",
				(uint)OFFSETOF(ca7regs_t, clk_ctl_st), val));
			val = R_REG(dhd->osh, ARM_CA7_REG(ca7regs, powerctl));
			DHD_ERROR(("reg:0x%x val:0x%x\n",
				(uint)OFFSETOF(ca7regs_t, powerctl), val));
		}
#endif /* NOT_YET */
	} else if (CHIPTYPE(sih->socitype) == SOCI_NCI &&
		(bus->pcie_sh->flags & (PCIE_SHARED_ASSERT | PCIE_SHARED_TRAP))) {
			dhd_pcie_nci_wrapper_dump(dhd, FALSE);
	}

	DHD_ERROR(("%s: OOBR Reg\n", __FUNCTION__));

	oob_base = si_oobr_baseaddr(sih, FALSE);
	oob_base1 = si_oobr_baseaddr(sih, TRUE);
	if (oob_base) {
		dhd_sbreg_op(dhd, oob_base + OOB_STATUSA, &val, TRUE);
		dhd_sbreg_op(dhd, oob_base + OOB_STATUSB, &val, TRUE);
		dhd_sbreg_op(dhd, oob_base + OOB_STATUSC, &val, TRUE);
		dhd_sbreg_op(dhd, oob_base + OOB_STATUSD, &val, TRUE);
	} else {
		reg = si_setcore(sih, HND_OOBR_CORE_ID, 0);
		if (reg != NULL) {
			val = R_REG(dhd->osh, &reg->intstatus[0]);
			DHD_PRINT(("reg: addr:%p val:0x%x\n", reg, val));
			val = R_REG(dhd->osh, &reg->intstatus[1]);
			DHD_PRINT(("reg: addr:%p val:0x%x\n", reg, val));
			val = R_REG(dhd->osh, &reg->intstatus[2]);
			DHD_PRINT(("reg: addr:%p val:0x%x\n", reg, val));
			val = R_REG(dhd->osh, &reg->intstatus[3]);
			DHD_PRINT(("reg: addr:%p val:0x%x\n", reg, val));
		}
	}

	if (oob_base1) {
		DHD_PRINT(("%s: Second OOBR Reg\n", __FUNCTION__));

		dhd_sbreg_op(dhd, oob_base1 + OOB_STATUSA, &val, TRUE);
		dhd_sbreg_op(dhd, oob_base1 + OOB_STATUSB, &val, TRUE);
		dhd_sbreg_op(dhd, oob_base1 + OOB_STATUSC, &val, TRUE);
		dhd_sbreg_op(dhd, oob_base1 + OOB_STATUSD, &val, TRUE);
	}

	if (CHIPTYPE(sih->socitype) == SOCI_NCI) {
		gciregs_t *gciregs = si_setcore(sih, GCI_CORE_ID, 0);
		if (gciregs != NULL) {
			val = R_REG(dhd->osh, &gciregs->gci_nci_err_int_status);
			DHD_ERROR(("GCI NCI ERR INTSTATUS: 0x%x\n", val));
		}
	}

	si_setcoreidx(dhd->bus->sih, save_idx);

	return 0;
}

void
dhdpcie_hw_war_regdump(dhd_bus_t *bus)
{
	uint32 save_idx, val;
	volatile uint32 *reg;

	save_idx = si_coreidx(bus->sih);
	reg = si_setcore(bus->sih, CC_CORE_ID, 0);
	if (reg != NULL) {
		val = R_REG(bus->osh, reg + REG_WORK_AROUND);
		DHD_PRINT(("CC HW_WAR :0x%x\n", val));
	}

	reg = si_setcore(bus->sih, ARMCR4_CORE_ID, 0);
	if (reg != NULL) {
		val = R_REG(bus->osh, reg + REG_WORK_AROUND);
		DHD_PRINT(("ARM HW_WAR:0x%x\n", val));
	}

	reg = si_setcore(bus->sih, PCIE2_CORE_ID, 0);
	if (reg != NULL) {
		val = R_REG(bus->osh, reg + REG_WORK_AROUND);
		DHD_PRINT(("PCIE HW_WAR :0x%x\n", val));
	}
	si_setcoreidx(bus->sih, save_idx);

	val = PMU_REG_NEW(bus->sih, MinResourceMask, 0, 0);
	DHD_PRINT(("MINRESMASK :0x%x\n", val));
}

static int
dhd_pcie_dma_info_dump(dhd_pub_t *dhd)
{
	if (dhd->bus->is_linkdown) {
		DHD_ERROR(("\n ------- SKIP DUMPING DMA Registers "
			"due to PCIe link down ------- \r\n"));
		return 0;
	}

	if (dhd->bus->link_state == DHD_PCIE_WLAN_BP_DOWN ||
		dhd->bus->link_state == DHD_PCIE_COMMON_BP_DOWN) {
		DHD_ERROR(("%s : wlan/common backplane is down (link_state=%u), skip.\n",
			__FUNCTION__, dhd->bus->link_state));
		return 0;
	}

	DHD_PRINT(("\n ------- DUMPING DMA Registers ------- \r\n"));

	//HostToDev
	DHD_PRINT(("HostToDev TX: XmtCtrl=0x%08x XmtPtr=0x%08x\n",
		si_corereg(dhd->bus->sih, dhd->bus->sih->buscoreidx, 0x200, 0, 0),
		si_corereg(dhd->bus->sih, dhd->bus->sih->buscoreidx, 0x204, 0, 0)));
	DHD_PRINT(("            : XmtAddrLow=0x%08x XmtAddrHigh=0x%08x\n",
		si_corereg(dhd->bus->sih, dhd->bus->sih->buscoreidx, 0x208, 0, 0),
		si_corereg(dhd->bus->sih, dhd->bus->sih->buscoreidx, 0x20C, 0, 0)));
	DHD_PRINT(("            : XmtStatus0=0x%08x XmtStatus1=0x%08x\n",
		si_corereg(dhd->bus->sih, dhd->bus->sih->buscoreidx, 0x210, 0, 0),
		si_corereg(dhd->bus->sih, dhd->bus->sih->buscoreidx, 0x214, 0, 0)));

	DHD_PRINT(("HostToDev RX: RcvCtrl=0x%08x RcvPtr=0x%08x\n",
		si_corereg(dhd->bus->sih, dhd->bus->sih->buscoreidx, 0x220, 0, 0),
		si_corereg(dhd->bus->sih, dhd->bus->sih->buscoreidx, 0x224, 0, 0)));
	DHD_PRINT(("            : RcvAddrLow=0x%08x RcvAddrHigh=0x%08x\n",
		si_corereg(dhd->bus->sih, dhd->bus->sih->buscoreidx, 0x228, 0, 0),
		si_corereg(dhd->bus->sih, dhd->bus->sih->buscoreidx, 0x22C, 0, 0)));
	DHD_PRINT(("            : RcvStatus0=0x%08x RcvStatus1=0x%08x\n",
		si_corereg(dhd->bus->sih, dhd->bus->sih->buscoreidx, 0x230, 0, 0),
		si_corereg(dhd->bus->sih, dhd->bus->sih->buscoreidx, 0x234, 0, 0)));

	//DevToHost
	DHD_PRINT(("DevToHost TX: XmtCtrl=0x%08x XmtPtr=0x%08x\n",
		si_corereg(dhd->bus->sih, dhd->bus->sih->buscoreidx, 0x240, 0, 0),
		si_corereg(dhd->bus->sih, dhd->bus->sih->buscoreidx, 0x244, 0, 0)));
	DHD_PRINT(("            : XmtAddrLow=0x%08x XmtAddrHigh=0x%08x\n",
		si_corereg(dhd->bus->sih, dhd->bus->sih->buscoreidx, 0x248, 0, 0),
		si_corereg(dhd->bus->sih, dhd->bus->sih->buscoreidx, 0x24C, 0, 0)));
	DHD_PRINT(("            : XmtStatus0=0x%08x XmtStatus1=0x%08x\n",
		si_corereg(dhd->bus->sih, dhd->bus->sih->buscoreidx, 0x250, 0, 0),
		si_corereg(dhd->bus->sih, dhd->bus->sih->buscoreidx, 0x254, 0, 0)));

	DHD_PRINT(("DevToHost RX: RcvCtrl=0x%08x RcvPtr=0x%08x\n",
		si_corereg(dhd->bus->sih, dhd->bus->sih->buscoreidx, 0x260, 0, 0),
		si_corereg(dhd->bus->sih, dhd->bus->sih->buscoreidx, 0x264, 0, 0)));
	DHD_PRINT(("            : RcvAddrLow=0x%08x RcvAddrHigh=0x%08x\n",
		si_corereg(dhd->bus->sih, dhd->bus->sih->buscoreidx, 0x268, 0, 0),
		si_corereg(dhd->bus->sih, dhd->bus->sih->buscoreidx, 0x26C, 0, 0)));
	DHD_PRINT(("            : RcvStatus0=0x%08x RcvStatus1=0x%08x\n",
		si_corereg(dhd->bus->sih, dhd->bus->sih->buscoreidx, 0x270, 0, 0),
		si_corereg(dhd->bus->sih, dhd->bus->sih->buscoreidx, 0x274, 0, 0)));

	return 0;
}

bool
dhd_pcie_dump_int_regs(dhd_pub_t *dhd)
{
	uint32 intstatus = 0;
	uint32 intmask = 0;
	uint32 d2h_db0 = 0;
	uint32 d2h_mb_data = 0;

	DHD_PRINT(("\n ------- DUMPING INTR Status and Masks ------- \r\n"));
	intstatus = si_corereg(dhd->bus->sih, dhd->bus->sih->buscoreidx,
		dhd->bus->pcie_mailbox_int, 0, 0);
	if (intstatus == (uint32)-1) {
		DHD_ERROR(("intstatus=0x%x \n", intstatus));
		return FALSE;
	}

	intmask = si_corereg(dhd->bus->sih, dhd->bus->sih->buscoreidx,
		dhd->bus->pcie_mailbox_mask, 0, 0);
	if (intmask == (uint32) -1) {
		DHD_ERROR(("intstatus=0x%x intmask=0x%x \n", intstatus, intmask));
		return FALSE;
	}

	d2h_db0 = si_corereg(dhd->bus->sih, dhd->bus->sih->buscoreidx,
		PCIE_REG_OFF(devtohost0doorbell0), 0, 0);
	if (d2h_db0 == (uint32)-1) {
		DHD_ERROR(("intstatus=0x%x intmask=0x%x d2h_db0=0x%x\n",
		intstatus, intmask, d2h_db0));
		return FALSE;
	}

	DHD_PRINT(("intstatus=0x%x intmask=0x%x d2h_db0=0x%x\n",
		intstatus, intmask, d2h_db0));
	dhd_bus_cmn_readshared(dhd->bus, &d2h_mb_data, D2H_MB_DATA, 0);
	DHD_PRINT(("d2h_mb_data=0x%x def_intmask=0x%x \r\n", d2h_mb_data,
		dhd->bus->def_intmask));

	return TRUE;
}

void
dhd_pcie_dump_rc_conf_space_cap(dhd_pub_t *dhd)
{
	DHD_PRINT(("\n ------- DUMPING PCIE RC config space Registers ------- \r\n"));
	DHD_PRINT(("Pcie RC Uncorrectable Error Status Val=0x%x\n",
		dhdpcie_rc_access_cap(dhd->bus, PCIE_EXTCAP_ID_ERR,
		PCIE_EXTCAP_AER_UCERR_OFFSET, TRUE, FALSE, 0)));
#ifdef EXTENDED_PCIE_DEBUG_DUMP
	DHD_PRINT(("hdrlog0 =0x%08x hdrlog1 =0x%08x hdrlog2 =0x%08x hdrlog3 =0x%08x\n",
		dhdpcie_rc_access_cap(dhd->bus, PCIE_EXTCAP_ID_ERR,
		PCIE_EXTCAP_ERR_HEADER_LOG_0, TRUE, FALSE, 0),
		dhdpcie_rc_access_cap(dhd->bus, PCIE_EXTCAP_ID_ERR,
		PCIE_EXTCAP_ERR_HEADER_LOG_1, TRUE, FALSE, 0),
		dhdpcie_rc_access_cap(dhd->bus, PCIE_EXTCAP_ID_ERR,
		PCIE_EXTCAP_ERR_HEADER_LOG_2, TRUE, FALSE, 0),
		dhdpcie_rc_access_cap(dhd->bus, PCIE_EXTCAP_ID_ERR,
		PCIE_EXTCAP_ERR_HEADER_LOG_3, TRUE, FALSE, 0)));
#endif /* EXTENDED_PCIE_DEBUG_DUMP */
}

#ifdef WL_CFGVENDOR_SEND_HANG_EVENT
#define MAX_RC_REG_INFO_VAL 8
#define PCIE_EXTCAP_ERR_HD_SZ 4
void
dhd_dump_pcie_rc_regs_for_linkdown(dhd_pub_t *dhd, int *bytes_written)
{
	int i;
	int remain_len;

	/* dump link control & status */
	if (dhd->hang_info_cnt < HANG_FIELD_CNT_MAX) {
		remain_len = VENDOR_SEND_HANG_EXT_INFO_LEN - *bytes_written;
		*bytes_written += scnprintf(&dhd->hang_info[*bytes_written], remain_len, "%08x%c",
			dhdpcie_rc_access_cap(dhd->bus, PCIE_CAP_ID_EXP,
				PCIE_CAP_LINKCTRL_OFFSET, FALSE, FALSE, 0), HANG_KEY_DEL);
		dhd->hang_info_cnt++;
	}

	/* dump device control & status */
	if (dhd->hang_info_cnt < HANG_FIELD_CNT_MAX) {
		remain_len = VENDOR_SEND_HANG_EXT_INFO_LEN - *bytes_written;
		*bytes_written += scnprintf(&dhd->hang_info[*bytes_written], remain_len, "%08x%c",
			dhdpcie_rc_access_cap(dhd->bus, PCIE_CAP_ID_EXP,
				PCIE_CAP_DEVCTRL_OFFSET, FALSE, FALSE, 0), HANG_KEY_DEL);
		dhd->hang_info_cnt++;
	}

	/* dump uncorrectable error */
	if (dhd->hang_info_cnt < HANG_FIELD_CNT_MAX) {
		remain_len = VENDOR_SEND_HANG_EXT_INFO_LEN - *bytes_written;
		*bytes_written += scnprintf(&dhd->hang_info[*bytes_written], remain_len, "%08x%c",
			dhdpcie_rc_access_cap(dhd->bus, PCIE_EXTCAP_ID_ERR,
			PCIE_EXTCAP_AER_UCERR_OFFSET, TRUE, FALSE, 0), HANG_KEY_DEL);
		dhd->hang_info_cnt++;
	}

	/* dump correctable error */
	if (dhd->hang_info_cnt < HANG_FIELD_CNT_MAX) {
		remain_len = VENDOR_SEND_HANG_EXT_INFO_LEN - *bytes_written;
		*bytes_written += scnprintf(&dhd->hang_info[*bytes_written], remain_len, "%08x%c",
			dhdpcie_rc_access_cap(dhd->bus, PCIE_EXTCAP_ID_ERR,
			/* use definition in linux/pcie_regs.h */
			PCI_ERR_COR_STATUS, TRUE, FALSE, 0), HANG_KEY_DEL);
		dhd->hang_info_cnt++;
	}

	/* HG05/06 reserved */
	if (dhd->hang_info_cnt < HANG_FIELD_CNT_MAX) {
		remain_len = VENDOR_SEND_HANG_EXT_INFO_LEN - *bytes_written;
		*bytes_written += scnprintf(&dhd->hang_info[*bytes_written], remain_len, "%08x%c",
			0, HANG_KEY_DEL);
		dhd->hang_info_cnt++;
	}

	if (dhd->hang_info_cnt < HANG_FIELD_CNT_MAX) {
		remain_len = VENDOR_SEND_HANG_EXT_INFO_LEN - *bytes_written;
		*bytes_written += scnprintf(&dhd->hang_info[*bytes_written], remain_len, "%08x%c",
			0, HANG_KEY_DEL);
		dhd->hang_info_cnt++;
	}

	/* dump error header log in RAW */
	for (i = 0; i < PCIE_EXTCAP_ERR_HD_SZ; i++) {
		remain_len = VENDOR_SEND_HANG_EXT_INFO_LEN - *bytes_written;
		*bytes_written += scnprintf(&dhd->hang_info[*bytes_written], remain_len,
			"%c%08x", HANG_RAW_DEL, dhdpcie_rc_access_cap(dhd->bus, PCIE_EXTCAP_ID_ERR,
			PCIE_EXTCAP_ERR_HEADER_LOG_0 + i * PCIE_EXTCAP_ERR_HD_SZ,
			TRUE, FALSE, 0));
	}
	dhd->hang_info_cnt++;
}
#endif /* WL_CFGVENDOR_SEND_HANG_EVENT */

int
dhd_pcie_debug_info_dump(dhd_pub_t *dhd)
{
	int host_irq_disabled;
	uint32 uc_status;

	DHD_PRINT(("bus->bus_low_power_state = %d\n", dhd->bus->bus_low_power_state));
	host_irq_disabled = dhdpcie_irq_disabled(dhd->bus);
	DHD_PRINT(("host pcie_irq disabled = %d\n", host_irq_disabled));
	dhd_print_tasklet_status(dhd);
	dhd_pcie_intr_count_dump(dhd);

#if defined(__linux__)
	DHD_PRINT(("\n ------- DUMPING PCIE EP Resouce Info ------- \r\n"));
	dhdpcie_dump_resource(dhd->bus);
#endif /* __linux__ */

	dhd_pcie_dump_rc_conf_space_cap(dhd);
	dhd_plat_pcie_dump_debug();

	DHD_PRINT(("RootPort PCIe linkcap=0x%08x\n",
		dhd_debug_get_rc_linkcap(dhd->bus)));
#ifdef CUSTOMER_HW4_DEBUG
	if (dhd->bus->is_linkdown) {
		DHD_ERROR(("Skip dumping the PCIe Config and Core registers. "
			"link may be DOWN\n"));
		return 0;
	}
#endif /* CUSTOMER_HW4_DEBUG */
	DHD_PRINT(("\n ------- DUMPING PCIE EP config space Registers ------- \r\n"));
	dhd_bus_dump_imp_cfg_registers(dhd->bus);
#ifdef EXTENDED_PCIE_DEBUG_DUMP
	uc_status = dhdpcie_ep_access_cap(dhd->bus, PCIE_EXTCAP_ID_ERR,
		PCIE_EXTCAP_AER_UCERR_OFFSET, TRUE, FALSE, 0);
	DHD_PRINT(("Pcie EP Uncorrectable Error Status Val=0x%x\n", uc_status));
#ifdef DHD_COREDUMP
	dhd->uc_status = uc_status;
#endif /* DHD_COREDUMP */
	DHD_PRINT(("hdrlog0(0x%x)=0x%08x hdrlog1(0x%x)=0x%08x hdrlog2(0x%x)=0x%08x "
		"hdrlog3(0x%x)=0x%08x\n", PCI_TLP_HDR_LOG1,
		dhd_pcie_config_read(dhd->bus, PCI_TLP_HDR_LOG1, sizeof(uint32)),
		PCI_TLP_HDR_LOG2,
		dhd_pcie_config_read(dhd->bus, PCI_TLP_HDR_LOG2, sizeof(uint32)),
		PCI_TLP_HDR_LOG3,
		dhd_pcie_config_read(dhd->bus, PCI_TLP_HDR_LOG3, sizeof(uint32)),
		PCI_TLP_HDR_LOG4,
		dhd_pcie_config_read(dhd->bus, PCI_TLP_HDR_LOG4, sizeof(uint32))));
	if (dhd->bus->sih->buscorerev >= 24) {
		DHD_PRINT(("DeviceStatusControl(0x%x)=0x%x SubsystemControl(0x%x)=0x%x "
			"L1SSControl2(0x%x)=0x%x\n", PCIECFGREG_DEV_STATUS_CTRL,
			dhd_pcie_config_read(dhd->bus, PCIECFGREG_DEV_STATUS_CTRL,
			sizeof(uint32)), PCIE_CFG_SUBSYSTEM_CONTROL,
			dhd_pcie_config_read(dhd->bus, PCIE_CFG_SUBSYSTEM_CONTROL,
			sizeof(uint32)), PCIECFGREG_PML1_SUB_CTRL2,
			dhd_pcie_config_read(dhd->bus, PCIECFGREG_PML1_SUB_CTRL2,
			sizeof(uint32))));
		dhd_bus_dump_dar_registers(dhd->bus);
	}
#endif /* EXTENDED_PCIE_DEBUG_DUMP */

	if (dhd->bus->is_linkdown) {
		DHD_ERROR(("Skip dumping the PCIe Core registers. link may be DOWN\n"));
		return 0;
	}
	if (dhd->bus->link_state == DHD_PCIE_WLAN_BP_DOWN ||
		dhd->bus->link_state == DHD_PCIE_COMMON_BP_DOWN) {
		DHD_ERROR(("%s : wlan/common backplane is down (link_state=%u), "
			"skip dumping pcie core regs.\n", __FUNCTION__,
			dhd->bus->link_state));
		return 0;
	}

	if (MULTIBP_ENAB(dhd->bus->sih)) {
		dhd_bus_pcie_pwr_req(dhd->bus);
	}

	dhdpcie_print_amni_regs(dhd->bus, FALSE);
	dhd_pcie_dump_wrapper_regs(dhd);
#ifdef DHD_PCIE_WRAPPER_DUMP
	dhd_pcie_get_wrapper_regs(dhd);
#endif /* DHD_PCIE_WRAPPER_DUMP */

	if (dhd->bus->dar_err_set) {
		DHD_ERROR(("Skip dumping the PCIe Core registers. DAR error log set\n"));
		goto exit;
	}

	DHD_PRINT(("\n ------- DUMPING PCIE core Registers ------- \r\n"));

#ifdef EXTENDED_PCIE_DEBUG_DUMP
	if (dhd->bus->sih->buscorerev >= 24) {
		DHD_PRINT(("errlog(0x%x)=0x%x errlog_addr(0x%x)=0x%x "
			"Function_Intstatus(0x%x)=0x%x "
			"Function_Intmask(0x%x)=0x%x Power_Intstatus(0x%x)=0x%x "
			"Power_Intmask(0x%x)=0x%x\n",
			PCIE_REG_OFF(errorlog_V0),
			si_corereg(dhd->bus->sih, dhd->bus->sih->buscoreidx,
			PCIE_REG_OFF(errorlog_V0), 0, 0),
			PCIE_REG_OFF(errorlog_addr_V0),
			si_corereg(dhd->bus->sih, dhd->bus->sih->buscoreidx,
				PCIE_REG_OFF(errorlog_addr_V0), 0, 0),
			PCIFunctionIntstatus(dhd->bus->sih->buscorerev),
			si_corereg(dhd->bus->sih, dhd->bus->sih->buscoreidx,
				PCIFunctionIntstatus(dhd->bus->sih->buscorerev), 0, 0),
			PCIFunctionIntmask(dhd->bus->sih->buscorerev),
			si_corereg(dhd->bus->sih, dhd->bus->sih->buscoreidx,
				PCIFunctionIntmask(dhd->bus->sih->buscorerev), 0, 0),
			PCIPowerIntstatus(dhd->bus->sih->buscorerev),
			si_corereg(dhd->bus->sih, dhd->bus->sih->buscoreidx,
				PCIPowerIntstatus(dhd->bus->sih->buscorerev), 0, 0),
			PCIPowerIntmask(dhd->bus->sih->buscorerev),
			si_corereg(dhd->bus->sih, dhd->bus->sih->buscoreidx,
				PCIPowerIntmask(dhd->bus->sih->buscorerev), 0, 0)));
		DHD_PRINT(("err_hdrlog1(0x%x)=0x%x err_hdrlog2(0x%x)=0x%x "
			"err_hdrlog3(0x%x)=0x%x err_hdrlog4(0x%x)=0x%x\n",
			(uint)PCIE_REG_OFF(error_header_reg1),
			si_corereg(dhd->bus->sih, dhd->bus->sih->buscoreidx,
				PCIE_REG_OFF(error_header_reg1), 0, 0),
			(uint)PCIE_REG_OFF(error_header_reg2),
			si_corereg(dhd->bus->sih, dhd->bus->sih->buscoreidx,
				PCIE_REG_OFF(error_header_reg2), 0, 0),
			(uint)PCIE_REG_OFF(error_header_reg3),
			si_corereg(dhd->bus->sih, dhd->bus->sih->buscoreidx,
				PCIE_REG_OFF(error_header_reg3), 0, 0),
			(uint)PCIE_REG_OFF(error_header_reg4),
			si_corereg(dhd->bus->sih, dhd->bus->sih->buscoreidx,
				PCIE_REG_OFF(error_header_reg4), 0, 0)));
		DHD_PRINT(("err_code(0x%x)=0x%x PCIH2D_MailBox(%08x)=%08x\n",
			(uint)PCIE_REG_OFF(error_code),
			si_corereg(dhd->bus->sih, dhd->bus->sih->buscoreidx,
				PCIE_REG_OFF(error_code), 0, 0),
			dhd_bus_db0_addr_get(dhd->bus),
			si_corereg(dhd->bus->sih, dhd->bus->sih->buscoreidx,
				dhd_bus_db0_addr_get(dhd->bus), 0, 0)));

		dhdpcie_hw_war_regdump(dhd->bus);
	}
#endif /* EXTENDED_PCIE_DEBUG_DUMP */

	dhd_pcie_dma_info_dump(dhd);
exit:
	if (MULTIBP_ENAB(dhd->bus->sih)) {
		dhd_bus_pcie_pwr_req_clear(dhd->bus);
	}

	return 0;
}

static int
dhdpcie_get_cbcore_dmps(struct dhd_bus *bus)
{
	return 0;
}

static int
dhdpcie_get_aoncore_dmps(struct dhd_bus *bus)
{
	return 0;
}

int
dhdpcie_get_cbaon_coredumps(struct dhd_bus *bus)
{
	uint32 chipid = 0, idx = 0, core_addr = 0;
	uint32 gcichipid_addr = 0;
	si_t *sih = bus->sih;
	int ret = 0;

	/* read chipcommon chipid using config space indirect backplane addressing,
	 * if successful, dump CB core regs
	 */
	chipid = dhdpcie_cfg_indirect_bpaccess(bus, si_enum_base(0), TRUE, 0);
	chipid = chipid & CID_ID_MASK;
	DHD_INFO(("%s: chipcommon chipid from cfgspc ind-bp read 0x%x\n", __FUNCTION__, chipid));
	if (chipid == 0xffff) {
		DHD_ERROR(("%s: invalid chip id!\n", __FUNCTION__));
		return BCME_BADADDR;
	}

	/* TODO: dump CB core regs */
	ret = dhdpcie_get_cbcore_dmps(bus);
	if (ret) {
		DHD_ERROR(("%s: dhdpcie_get_cbcore_dmps failed !\n", __FUNCTION__));
		return ret;
	}

	/* read GCI chipid using config space indirect backplane addressing,
	 * if successful, dump AON core regs
	 */
	idx = si_findcoreidx(sih, GCI_CORE_ID, 0);
	core_addr = si_get_coreaddr(sih, idx);
	if (!core_addr) {
		DHD_ERROR(("%s: Failed to get core addr for idx 0x%x !\n",
			__FUNCTION__, idx));
		return BCME_ERROR;
	}
	gcichipid_addr = core_addr + OFFSETOF(gciregs_t, gci_chipid);
	chipid = dhdpcie_cfg_indirect_bpaccess(bus, gcichipid_addr, TRUE, 0);
	chipid = chipid & CID_ID_MASK;
	DHD_INFO(("%s: gci chipid from cfgspc ind-bp read 0x%x\n", __FUNCTION__, chipid));
	if (chipid == 0xffff) {
		DHD_ERROR(("%s: invalid chip id!\n", __FUNCTION__));
		return BCME_BADADDR;
	}

	/* TODO: dump AON core regs */
	ret = dhdpcie_get_aoncore_dmps(bus);
	if (ret) {
		DHD_ERROR(("%s: dhdpcie_get_aoncore_dmps failed !\n", __FUNCTION__));
		return ret;
	}

	return BCME_OK;
}

#if defined(DHD_SDTC_ETB_DUMP) || defined(EWP_DACS)
int
dhd_bus_get_etb_config_cmn(dhd_bus_t *bus, uint32 etb_config_info_addr)
{
	int ret = 0;
	int i = 0;
	etb_config_info_cmn_t etb_hdr;
	etb_block_t *etb = NULL;
	uint eblk_offset = 0;
	uint max_etb_size[ETB_USER_MAX] = {ETB_USER_SDTC_MAX_SIZE,
		ETB_USER_ETM_MAX_SIZE, ETB_USER_ETMCOEX_MAX_SIZE};
	uint size = 0, total_blksize = 0;

	bzero(&etb_hdr, sizeof(etb_hdr));
	ret = dhdpcie_bus_membytes(bus, FALSE, DHD_PCIE_MEM_BAR1,
			(ulong)etb_config_info_addr, (uint8 *)&etb_hdr,
			sizeof(etb_hdr));
	if (ret < 0) {
		DHD_ERROR(("%s: Error reading etb_config_info_cmn_t(etb_hdr)"
			"structure from dongle \n",
			__FUNCTION__));
		return BCME_ERROR;
	}

	/* validate and get offset of eblk */
	if (etb_hdr.version == EWP_ETB_CONFIG_INFO_VER_1) {
		eblk_offset = OFFSETOF(etb_config_info_v1_t, eblk);
	} else if (etb_hdr.version == EWP_ETB_CONFIG_INFO_VER_2) {
		eblk_offset = OFFSETOF(etb_config_info_v2_t, eblk);
	} else {
		DHD_ERROR(("%s: Unsupported version (%u) ! Expected <= %u \n",
			__FUNCTION__, etb_hdr.version,
			EWP_ETB_CONFIG_INFO_VER));
		return BCME_VERSION;
	}

	if (etb_hdr.num_etb > ETB_USER_MAX)  {
		DHD_ERROR(("%s: Bad num_etb (%u) ! max %u \n",
			__FUNCTION__, etb_hdr.num_etb,
			ETB_USER_MAX));
		return BCME_BADLEN;
	}

	if (!bus->eblk_buf) {
		DHD_ERROR(("%s: No mem alloc'd for etb blocks !\n",
			__FUNCTION__));
		return BCME_NOMEM;
	}

	/* copy contents */
	size = (etb_hdr.num_etb * sizeof(etb_block_t));
	bzero(bus->eblk_buf, size);
	ret = dhdpcie_bus_membytes(bus, FALSE, DHD_PCIE_MEM_BAR1,
			(ulong)(etb_config_info_addr + eblk_offset), (uint8 *)bus->eblk_buf, size);
	if (ret < 0) {
		DHD_ERROR(("%s: Error reading etb blocks from dongle,"
			" ETB CONFIG VER - %u \n", __FUNCTION__, etb_hdr.version));
		return BCME_ERROR;
	}

	/* validate the contents of each etb block */
	for (i = 0; i < etb_hdr.num_etb; ++i) {
		bus->etb_validity[i] = TRUE;
		etb = &bus->eblk_buf[i];
		if (!etb->inited) {
			DHD_ERROR(("%s: ETB%u not inited !\n", __FUNCTION__, i));
			bus->etb_validity[i] = FALSE;
			continue;
		}
		if (etb->type > ETB_USER_MAX) {
			DHD_ERROR(("%s: ETB%u bad type %u ! max %u\n",
				__FUNCTION__, i, etb->type, ETB_USER_MAX));
			bus->etb_validity[i] = FALSE;
			continue;
		}

		etb->size = ltoh16(etb->size);
		if (IS_BUFSIZE_INVALID(etb->size, max_etb_size[etb->type])) {
			DHD_ERROR(("%s: ETB%u bad size %u ! max %u\n",
				__FUNCTION__, i, etb->size, max_etb_size[etb->type]));
			bus->etb_validity[i] = FALSE;
			continue;
		} else {
			total_blksize += etb->size;
		}

		etb->addr = ltoh32(etb->addr);
		if (IS_HWADDR_INVALID(etb->addr)) {
			DHD_ERROR(("%s: ETB%u bad addr %u !n",
				__FUNCTION__, i, etb->addr));
			bus->etb_validity[i] = FALSE;
			continue;
		}
	}

	/* check if atleast one block is valid */
	for (i = 0; i < etb_hdr.num_etb; ++i) {
		if (bus->etb_validity[i])
			break;
	}
	if (i >= etb_hdr.num_etb) {
		DHD_ERROR(("%s:No valid etb blocks found !\n", __FUNCTION__));
		return BCME_BADARG;
	}

	/* check if total etb block size exceeds alloc'd memory */
	if (total_blksize > DHD_SDTC_ETB_MEMPOOL_SIZE) {
		DHD_ERROR(("%s: total blksize %u exceeds"
			" alloc'd memsize %u !\n", __FUNCTION__,
			total_blksize, DHD_SDTC_ETB_MEMPOOL_SIZE));
		return BCME_BADLEN;
	}

	DHD_PRINT(("%s: read etb_config_info and etb blocks(%d) info"
		" (%u bytes) from dongle \n", __FUNCTION__,
		etb_hdr.num_etb, size));

	return BCME_OK;
}
#endif /* DHD_SDTC_ETB_DUMP || EWP_DACS */

#ifdef DHD_SDTC_ETB_DUMP
int
dhd_bus_get_etb_config(dhd_bus_t *bus, uint32 etb_config_info_addr)
{
	int ret = 0;

	/* endianness */
	bus->etb_config_addr = ltoh32(etb_config_info_addr);

	/* check sanity of etb_config_info_addr */
	if (IS_HWADDR_INVALID(bus->etb_config_addr)) {
		DHD_ERROR(("%s: bad etb_config_info_addr(%x) \n", __FUNCTION__,
			bus->etb_config_addr));
		return BCME_BADADDR;
	}

	DHD_PRINT(("%s: FW supports etb config, etb_config_info_addr=0x%x\n",
		__FUNCTION__, bus->etb_config_addr));

	ret = dhd_bus_get_etb_config_cmn(bus, bus->etb_config_addr);

	return ret;
}

void
dhd_etb_dump_deinit(dhd_pub_t *dhd)
{
	dhd->etb_dump_inited = FALSE;
	printf("DEBUG: %s: etb is deactivated\n", __FUNCTION__);
}
#endif /* DHD_SDTC_ETB_DUMP */

#if defined(COEX_CPU) && defined(EWP_DACS)
static void
dhdpcie_update_coex_cpu_info(dhd_bus_t *bus, uint32 coex_cpu_info_addr)
{
	ewp_coex_cpu_info_t ewp_coex_info;
	int ret;

	ret = dhdpcie_bus_membytes(bus, FALSE, DHD_PCIE_MEM_BAR1, (ulong)coex_cpu_info_addr,
		(uint8 *)&ewp_coex_info, sizeof(ewp_coex_info));
	if (ret < 0) {
		DHD_ERROR(("%s: Error reading ewp_coex_cpu_info from dongle! \n",
			__FUNCTION__));
		return;
	}

	if (ewp_coex_info.version != EWP_COEX_CPU_INFO_VER) {
		DHD_ERROR(("%s: Unsupported EWP coex cpu info versiont %u\n",
			__FUNCTION__, ewp_coex_info.version));
		return;
	}

	bus->coex_itcm_base = ltoh32(ewp_coex_info.itcm_base);
	bus->coex_itcm_size = ltoh32(ewp_coex_info.itcm_sz);
	bus->coex_dtcm_base = ltoh32(ewp_coex_info.dtcm_base);
	bus->coex_dtcm_size = ltoh32(ewp_coex_info.dtcm_sz);

	DHD_PRINT(("COEX CPU itcm@0x%08x len %u dtcm@%08x len %u\n",
		bus->coex_itcm_base, bus->coex_itcm_size,
		bus->coex_dtcm_base, bus->coex_dtcm_size));
}
#endif /* COEX_CPU && EWP_DACS */

#ifdef EWP_DACS
int
dhdpcie_ewphw_get_initdumps(dhd_bus_t *bus)
{
	dhd_pub_t *dhdp = bus->dhd;
	pciedev_shared_t *sh = bus->pcie_sh;
	ewp_hw_info_t ewp_hw_info;
	int ret = 0;
	uint8 *regdump_buf = dhdp->ewphw_regdump_buf;

	/* endianness */
	bus->ewp_info.ewp_hw_info_addr = ltoh32(bus->ewp_info.ewp_hw_info_addr);
	bus->ewp_info.hnd_debug_addr = ltoh32(bus->ewp_info.hnd_debug_addr);
	bus->ewp_info.hnd_debug_ptr_addr = ltoh32(bus->ewp_info.hnd_debug_ptr_addr);
	/* For now don't use sssr addr */
	/* ewp_info.sssr_info_addr = ltoh32(ewp_info.sssr_info_addr); */

#ifdef COEX_CPU
	bus->ewp_info.coex_cpu_info_addr = ltoh32(bus->ewp_info.coex_cpu_info_addr);
	if (!IS_HWADDR_INVALID(bus->ewp_info.coex_cpu_info_addr)) {
		dhdpcie_update_coex_cpu_info(bus, bus->ewp_info.coex_cpu_info_addr);
	}
#endif

	/* validate the addresses */
	if (IS_HWADDR_INVALID(bus->ewp_info.hnd_debug_addr) ||
		IS_HWADDR_INVALID(bus->ewp_info.hnd_debug_ptr_addr)) {
		DHD_ERROR(("%s: ewp_info - bad addr ! "
			" hnd_debug_addr=%x; hnd_debug_ptr_addr=%x;\n",
			__FUNCTION__, bus->ewp_info.hnd_debug_addr,
			bus->ewp_info.hnd_debug_ptr_addr));
		return BCME_BADADDR;
	}

	/* in FW ewp_hw_info may not be populated, but ewp_info will
	 * always be there. Such a case is not a failure, so return ok
	 */
	if (IS_HWADDR_INVALID(bus->ewp_info.ewp_hw_info_addr)) {
		DHD_ERROR(("%s: ewp_hw_info - bad addr (%x)\n",
			__FUNCTION__, bus->ewp_info.ewp_hw_info_addr));
		ret = BCME_OK;
		goto exit;
	}

	bzero(&ewp_hw_info, sizeof(ewp_hw_info));
	/* read the ewp_hw_info structure */
	ret = dhdpcie_bus_membytes(bus, FALSE, DHD_PCIE_MEM_BAR1,
		(ulong)bus->ewp_info.ewp_hw_info_addr,
		(uint8 *)&ewp_hw_info, sizeof(ewp_hw_info));
	if (ret < 0) {
		DHD_ERROR(("%s: Error reading ewp_hw_info structure from dongle \n",
			__FUNCTION__));
		return ret;
	}

	/* validate the version */
	if (ewp_hw_info.version != EWP_HW_INFO_VER) {
		DHD_ERROR(("%s: ewp_hw_info - bad version(%u) ! expected %u \n",
			__FUNCTION__, ewp_hw_info.version, EWP_HW_INFO_VER));
		return BCME_VERSION;
	}

	DHD_PRINT(("%s: pcie_hwhdr_rev = %u\n", __FUNCTION__,
		ewp_hw_info.pcie_hwhdr_rev));
	/* Initial validations for EWP_DACS are done */
	dhdp->ewp_dacs_fw_enable = TRUE;
	/* endianness */
	ewp_hw_info.init_log_buf.addr = ltoh32(ewp_hw_info.init_log_buf.addr);
	ewp_hw_info.init_log_buf.size = ltoh32(ewp_hw_info.init_log_buf.size);
	ewp_hw_info.mod_dump_buf.addr = ltoh32(ewp_hw_info.mod_dump_buf.addr);
	ewp_hw_info.mod_dump_buf.size = ltoh32(ewp_hw_info.mod_dump_buf.addr);
	ewp_hw_info.reg_dump_buf.addr = ltoh32(ewp_hw_info.reg_dump_buf.addr);
	ewp_hw_info.reg_dump_buf.size = ltoh32(ewp_hw_info.reg_dump_buf.size);

	/* validate the addresses and size and copy to local mem */
	/* 1. INIT_LOGS */
	if (IS_HWADDR_INVALID(ewp_hw_info.init_log_buf.addr) ||
		IS_BUFSIZE_INVALID(ewp_hw_info.init_log_buf.size,
		EWP_HW_INIT_LOG_LEN)) {
		DHD_ERROR(("%s: ewp_hw_info - init_log_buf bad addr(%x) or size(%u)\n",
			__FUNCTION__, ewp_hw_info.init_log_buf.addr,
			ewp_hw_info.init_log_buf.size));
	} else {
		dhdp->ewphw_initlog_len = ewp_hw_info.init_log_buf.size;
		if (dhdp->ewphw_initlog_buf) {
			ret = dhdpcie_bus_membytes(bus, FALSE, DHD_PCIE_MEM_BAR1,
				(ulong)ewp_hw_info.init_log_buf.addr,
				dhdp->ewphw_initlog_buf,
				dhdp->ewphw_initlog_len);
			if (ret < 0) {
				DHD_ERROR(("%s: Error reading init_log_buf from dongle! \n",
					__FUNCTION__));
			}
			DHD_INFO(("%s: copied init_log_buf addr=%x size=%u bytes from dongle \n",
				__FUNCTION__, ewp_hw_info.init_log_buf.addr,
				dhdp->ewphw_initlog_len));
		}
	}

	/* 2. MOD_DUMP */
	if (IS_HWADDR_INVALID(ewp_hw_info.mod_dump_buf.addr) ||
		IS_BUFSIZE_INVALID(ewp_hw_info.mod_dump_buf.size,
			EWP_HW_MOD_DUMP_LEN)) {
		DHD_ERROR(("%s: ewp_hw_info - mod_dump_buf bad addr(%x) or size(%u)\n",
			__FUNCTION__, ewp_hw_info.mod_dump_buf.addr,
			ewp_hw_info.mod_dump_buf.size));
	} else {
		dhdp->ewphw_moddump_len = ewp_hw_info.mod_dump_buf.size;
		if (dhdp->ewphw_moddump_buf) {
			ret = dhdpcie_bus_membytes(bus, FALSE, DHD_PCIE_MEM_BAR1,
				(ulong)ewp_hw_info.mod_dump_buf.addr,
				dhdp->ewphw_moddump_buf,
				dhdp->ewphw_moddump_len);
			if (ret < 0) {
				DHD_ERROR(("%s: Error reading mod_dump_buf from dongle! \n",
					__FUNCTION__));
			}
		}
	}

	/* 3. REG_DUMP */
	/* For reg dump, first need to read hnd_debug_ptr_t and
	 * hnd_debug_t structures and copy them, then read the
	 * actual dumps. Reg dump structure will be as follows:
	 *
	 * +++++++++++++++++++++++++
	 * |reg_dump_hdr_t         |
	 * +++++++++++++++++++++++++
	 * |contents of            |
	 * |hnd_debug_ptr_t        |
	 * +++++++++++++++++++++++++
	 * |contents of            |
	 * |hnd_debug_t            |
	 * +++++++++++++++++++++++++
	 * |                       |
	 * | reg dumps             |
	 * |                       |
	 * |                       |
	 * +++++++++++++++++++++++++
	 */
	if (IS_HWADDR_INVALID(ewp_hw_info.reg_dump_buf.addr) ||
		!ewp_hw_info.reg_dump_buf.size ||
		((sizeof(hnd_debug_ptr_t) + sizeof(hnd_debug_t) +
		ewp_hw_info.reg_dump_buf.size) > EWP_HW_REG_DUMP_LEN)) {
		DHD_ERROR(("%s: ewp_hw_info - reg_dump_buf bad addr(%x) or size(%u)\n",
			__FUNCTION__, ewp_hw_info.reg_dump_buf.addr,
			ewp_hw_info.reg_dump_buf.size));
	} else {
		dhdp->ewphw_regdump_len = sizeof(reg_dump_hdr_t) +
			sizeof(hnd_debug_ptr_t) + sizeof(hnd_debug_t) +
			ewp_hw_info.reg_dump_buf.size;
		if (dhdp->ewphw_regdump_buf) {
			/* first copy the content (addr and lens)
			 * into reg_dump_hdr and put into regdump buf
			 */
			reg_dump_hdr_t *regdump_hdr = (reg_dump_hdr_t *)regdump_buf;
			regdump_hdr->hnd_debug_ptr_addr = bus->ewp_info.hnd_debug_ptr_addr;
			regdump_hdr->hnd_debug_ptr_len = sizeof(hnd_debug_ptr_t);
			regdump_hdr->hnd_debug_addr = bus->ewp_info.hnd_debug_addr;
			regdump_hdr->hnd_debug_len = sizeof(hnd_debug_t);
			regdump_hdr->device_fatal_logbuf_start_addr =
				ltoh32(sh->device_fatal_logbuf_start);
			regdump_hdr->regdump_len = ewp_hw_info.reg_dump_buf.size;
			regdump_buf += sizeof(reg_dump_hdr_t);

			/* now copy hnd_debug_ptr contents to regdump buf */
			ret = dhdpcie_bus_membytes(bus, FALSE, DHD_PCIE_MEM_BAR1,
				(ulong)bus->ewp_info.hnd_debug_ptr_addr,
				regdump_buf,
				sizeof(hnd_debug_ptr_t));
			if (ret < 0) {
				DHD_ERROR(("%s: Error reading hnd_debug_ptr from dongle! \n",
					__FUNCTION__));
				goto exit;
			}
			regdump_buf += sizeof(hnd_debug_ptr_t);

			/* next put the contents of hnd_debug_t */
			ret = dhdpcie_bus_membytes(bus, FALSE, DHD_PCIE_MEM_BAR1,
				(ulong)bus->ewp_info.hnd_debug_addr,
				regdump_buf,
				sizeof(hnd_debug_t));
			if (ret < 0) {
				DHD_ERROR(("%s: Error reading hnd_debug from dongle! \n",
					__FUNCTION__));
				goto exit;
			}
			regdump_buf += sizeof(hnd_debug_t);

			/* finally put the actual reg dump contents */
			ret = dhdpcie_bus_membytes(bus, FALSE, DHD_PCIE_MEM_BAR1,
				(ulong)ewp_hw_info.reg_dump_buf.addr,
				regdump_buf,
				ewp_hw_info.reg_dump_buf.size);
			if (ret < 0) {
				DHD_ERROR(("%s: Error reading reg_dump_buf from dongle! \n",
					__FUNCTION__));
			}
			DHD_INFO(("%s: copied reg_dump_buf(addr=%x size=%u bytes) from dongle \n",
				__FUNCTION__, ewp_hw_info.reg_dump_buf.addr,
				ewp_hw_info.reg_dump_buf.size));
		}
	}

	DHD_PRINT(("%s: ewphw - set actual lengths; initlog_len=%u; regdump_len=%u;"
		" moddump_len=%u\n", __FUNCTION__, dhdp->ewphw_initlog_len,
		dhdp->ewphw_regdump_len, dhdp->ewphw_moddump_len));

	/* copy ewp structs to local mem */
	ret = memcpy_s(&bus->ewp_hw_info, sizeof(bus->ewp_hw_info),
		&ewp_hw_info, sizeof(ewp_hw_info));

exit:
	return ret;
}
#endif /* EWP_DACS */

#ifdef DHD_SDTC_ETB_DUMP
int
dhd_bus_get_etb_info(dhd_pub_t *dhd, uint32 etbinfo_addr, etb_info_t *etb_info)
{

	int ret = 0;
	ret = dhdpcie_bus_membytes(dhd->bus, FALSE, DHD_PCIE_MEM_BAR1, etbinfo_addr,
			(unsigned char *)etb_info, sizeof(*etb_info));
	if (ret) {
		DHD_ERROR(("%s: Read Error membytes %d\n", __FUNCTION__, ret));
		return BCME_ERROR;
	}

	return BCME_OK;
}

int
dhd_bus_get_sdtc_etb(dhd_pub_t *dhd, uint8 *sdtc_etb_mempool, uint addr, uint read_bytes)
{
	int ret = 0;

	ret = dhdpcie_bus_membytes(dhd->bus, FALSE, DHD_PCIE_MEM_BAR1, addr,
			(unsigned char *)sdtc_etb_mempool, read_bytes);
	if (ret) {
		DHD_ERROR(("%s: Read Error membytes %d\n", __FUNCTION__, ret));
		return BCME_ERROR;
	}
	return BCME_OK;
}

int
dhd_bus_alloc_ewp_etb_config_mem(dhd_bus_t *bus)
{
	if (!bus->eblk_buf) {
		/* allocate the required memory for etb block info */
		bus->eblk_buf_size = (ETB_USER_MAX * sizeof(etb_block_t));
		bus->eblk_buf = MALLOCZ(bus->osh, bus->eblk_buf_size);
		if (!bus->eblk_buf) {
			DHD_ERROR(("%s: Failed to alloc mem for eblk_buf !\n",
				__FUNCTION__));
			return BCME_NOMEM;
		}
	}
	return BCME_OK;
}

void
dhd_bus_dealloc_ewp_etb_config_mem(dhd_bus_t *bus)
{
	if (bus->eblk_buf) {
		MFREE(bus->osh, bus->eblk_buf, bus->eblk_buf_size);
	}
}

/* Provides FW/DHD shared etb_config_info_t size based on version */
static int
dhd_bus_get_etb_cfg_size(dhd_bus_t *bus, uint8 ver, uint *size)
{
	int ret = BCME_OK;

	if (ver == EWP_ETB_CONFIG_INFO_VER_1) {
		*size = sizeof(etb_config_info_v1_t);
	} else if (ver == EWP_ETB_CONFIG_INFO_VER_2) {
		*size = sizeof(etb_config_info_v2_t);
	} else {
		DHD_ERROR(("%s: unsupported ETB config version %u\n",
			__FUNCTION__, ver));
		ret = BCME_VERSION;
	}

	return ret;
}

int
dhd_bus_get_ewp_etb_config(dhd_bus_t *bus)
{
	int ret = 0;
	ewp_info_t *ewp_info = &bus->ewp_info;
	uint curcore = 0;
	chipcregs_t *chipcregs = NULL;
	uint ccrev = 0;

	/* endianness */
	ewp_info->etb_config_info_addr = ltoh32(ewp_info->etb_config_info_addr);
	/* check if fw supports new ewp dacs method of collecting
	 * ETB dumps without iovar
	 */
	if (IS_HWADDR_INVALID(ewp_info->etb_config_info_addr)) {
		DHD_ERROR(("%s: FW does not support ewp etb config"
			" etb_config_info_addr=0x%x \n", __FUNCTION__,
			ewp_info->etb_config_info_addr));
		return BCME_UNSUPPORTED;
	}
	DHD_PRINT(("%s: FW supports ewp etb config, etb_config_info_addr=0x%x\n",
		__FUNCTION__, ewp_info->etb_config_info_addr));

	ret = dhd_bus_get_etb_config_cmn(bus, ewp_info->etb_config_info_addr);

	/* get chipcommon revision, based on which
	 * ETB DAP TMC flush support is decided.
	 * only if DAP TMC flush is possible
	 * then we can collect sdtc/etb dumps
	 * for non trap cases also
	 */
	curcore = si_coreid(bus->sih);
	chipcregs = (chipcregs_t *)si_setcore(bus->sih, CC_CORE_ID, 0);
	if (chipcregs != NULL) {
		ccrev = si_corerev(bus->sih);
		if (ccrev >= EWP_ETB_DAP_TMC_FLUSH_CCREV) {
			DHD_ERROR(("%s: ccrev = %u, ETB DAP flush support present\n",
				__FUNCTION__, ccrev));
			bus->dhd->etb_dap_flush_supported = TRUE;
		}
	}
	si_setcore(bus->sih, curcore, 0);

	return ret;
}

#define TMC_REG_OFF(regname) OFFSETOF(tmcregs_t, regname)
/* For host we cannot use hnd_dap_flush_tmc(), because of the below reason:
 * For host access of DAP TMC registers, it is not possible to directly
 * access like in case of FW, because the BAR0 window maps only upto 4k of
 * address. Whereas within the DAP_CORE the TMC registers are at offset
 * 0x41000 and above (i.e beyond 4k).
 * Further the EROM entry for DAP_CORE_ID does not have coreunit level
 * information for DAP_TMC0/1/2 and have only a single entry for the core.
 * Hence si_setcore only maps the DAP_CORE (0x1800D000) to host addr space.
 * Thus if we add the TMC0 offset of 0x41000 to the above address, it exceeds the
 * kernel page size and leads to page fault crash.
 * Hence, the only way to do the DAP flush from host is via si_backplane_access
 * until EROM entries are available.
 */
static int
dhd_bus_flush_dap_tmc(dhd_bus_t *bus, uint etb)
{
	uint32 val = 0;
	uint32 debug_base = 0;
	si_t *sih = bus->sih;
	uint32 addr = 0, offset = 0;
	uint curidx = si_coreidx(sih);
	uint max_retries = 3;
	uint idx = 0;

	switch (etb) {
	case 0:
		offset = DAP_TMC0_OFFSET_CCREV_GE74;
		break;
	case 1:
		offset = DAP_TMC1_OFFSET_CCREV_GE74;
		break;
	case 2:
		offset = DAP_TMC2_OFFSET_CCREV_GE74;
		break;
	default:
		DHD_ERROR(("%s: wrong etb %u !\n", __FUNCTION__, etb));
		return BCME_BADARG;
	}

	si_setcore(sih, DAP_CORE_ID, 0);

	/* get the DAP core backplane address */
	idx = si_findcoreidx(sih, DAP_CORE_ID, 0);
	debug_base = si_get_coreaddr(sih, idx) + offset;
	if (!debug_base) {
		DHD_ERROR(("%s: Failed to get core addr for idx 0x%x !\n",
			__FUNCTION__, idx));
		return BCME_ERROR;
	}
	addr = debug_base + TMC_REG_OFF(ffcr);

	/* set bit 6 in TMC FFCR register to flush */
	serialized_backplane_access(bus, addr, 4, &val, TRUE);
	val |= (1 << CORESIGHT_TMC_FFCR_FLUSHMAN_SHIFT);
	serialized_backplane_access(bus, addr, 4, &val, FALSE);

	/* poll bit 3 in TMC STS status register indicating flush is done */
	val = 0;
	addr = debug_base + TMC_REG_OFF(sts);
	while (!(val & (1 << CORESIGHT_TMC_STS_FLUSHMAN_SHIFT)) && max_retries) {
		serialized_backplane_access(bus, addr, 4, &val, TRUE);
		OSL_DELAY(100);
		--max_retries;
	}

	if (!(val & (1 << CORESIGHT_TMC_STS_FLUSHMAN_SHIFT)) && !max_retries) {
		DHD_ERROR(("%s: Failed to flush etb%u, TMC STS 0x%x = 0x%x\n",
			__FUNCTION__, etb, addr, val));
		return BCME_ERROR;
	}

	DHD_INFO(("%s: Flushed ETB DAP TMC %d, STS reg (0x%x) = 0x%x\n",
			__FUNCTION__, etb, addr, val));

	si_setcoreidx(sih, curidx);

	return BCME_OK;
}

int
dhd_bus_get_ewp_etb_dump(dhd_bus_t *bus, uint8 *buf, uint bufsize)
{
	int ret = 0;
	ewp_info_t *ewp_info = &bus->ewp_info;

	ret = dhd_bus_get_etb_dump_cmn(bus, buf, bufsize, ewp_info->etb_config_info_addr);

	return ret;
}

/*
 * dhd_bus_get_etb_dump_cmn - reads etb dumps over pcie bus into a buffer
 * buf - buffer to read into
 * bufsize - size of buffer
 * return value - total size of the dump read into the buffer,
 * -ve error value in case of failure
 *
 * The format of the etb dump in the buffer will be -
 * +++++++++++++++++++++
 * | etb_config_info_t |
 * +++++++++++++++++++++
 * | etb_block_t       |
 * +++++++++++++++++++++
 * | etb0 contents     |
 * |                   |
 * +++++++++++++++++++++
 * | etb_block_t       |
 * +++++++++++++++++++++
 * | etb1 contents     |
 * |                   |
 * +++++++++++++++++++++
 * | etb_block_t       |
 * +++++++++++++++++++++
 * | etb2 contents     |
 * |                   |
 * +++++++++++++++++++++
 *
 * Note - The above representation assumes that all 3 etb blocks are valid.
 * If there is only 1 or 2 valid etb blocks provided by dongle
 * then the above representation will change and have only a
 * single etb block or two etb blocks accordingly
*/
int
dhd_bus_get_etb_dump_cmn(dhd_bus_t *bus, uint8 *buf, uint bufsize, uint32 etb_config_info_addr)
{
	int ret = 0, i = 0;
	uint8 *etb_cfg_buf = NULL;
	uint etb_cfg_size = 0;
	etb_config_info_cmn_t etb_hdr;
	etb_block_t *etb = NULL; /* Points to single element of eblk[] */
	uint eblk_offset = 0;
	uint8 *ptr = buf;
	int totsize = 0;
	uint32 rwpaddr = 0;
	uint32 rwp = 0;
	uint curcore = 0;
	chipcregs_t *chipcregs = NULL;
	uint ccrev = 0;
	bool skip_flush_and_rwp_update = FALSE;
	dhd_pub_t *dhdp = bus->dhd;
	bool flushed[ETB_USER_MAX] = {TRUE, TRUE, TRUE};
	bool all_flushed = TRUE;
	uint32 dap_tmc_offset[ETB_USER_MAX] = {DAP_TMC0_OFFSET_CCREV_GE74,
		DAP_TMC1_OFFSET_CCREV_GE74, DAP_TMC2_OFFSET_CCREV_GE74};

	BCM_REFERENCE(chipcregs);

	/* read etb common hdr */
	bzero(&etb_hdr, sizeof(etb_hdr));
	ret = dhdpcie_bus_membytes(bus, FALSE, DHD_PCIE_MEM_BAR1,
			(ulong)etb_config_info_addr, (uint8 *)&etb_hdr,
			sizeof(etb_hdr));
	if (ret < 0) {
		DHD_ERROR(("%s: Error reading etb_config_info structure from dongle \n",
			__FUNCTION__));
		return BCME_ERROR;
	}

	/* Get ETB cfg size based on ETB cfg version */
	ret = dhd_bus_get_etb_cfg_size(bus, etb_hdr.version, &etb_cfg_size);
	if (ret != BCME_OK) {
		/* Unsupported version */
		return ret;
	}

	if (!buf || !bufsize || (bufsize < etb_cfg_size)) {
		return BCME_BADARG;
	}

	etb_cfg_buf = MALLOCZ(bus->osh, etb_cfg_size);
	if (!etb_cfg_buf) {
		DHD_ERROR(("%s: Failed to alloc mem for etb_config_info and etb blocks !\n",
			__FUNCTION__));
		return BCME_NOMEM;
	}

	/* read etm header */
	ret = dhdpcie_bus_membytes(bus, FALSE, DHD_PCIE_MEM_BAR1,
		(ulong)etb_config_info_addr, (uint8 *)etb_cfg_buf, etb_cfg_size);
	if (ret < 0) {
		DHD_ERROR(("%s: error reading ETB config header during dump\n",
			__FUNCTION__));
	}

	/* first write the etb_config_info_t structure */
	memcpy_s(ptr, etb_cfg_size, etb_cfg_buf, etb_cfg_size);
	ptr += etb_cfg_size;
	totsize += etb_cfg_size;

	/* etb_cfg_buf is valid; free it here */
	MFREE(bus->osh, etb_cfg_buf, etb_cfg_size);

	/* get chipcommon revision */
	curcore = si_coreid(bus->sih);
	chipcregs = (chipcregs_t *)si_setcore(bus->sih, CC_CORE_ID, 0);
	ASSERT(chipcregs != NULL);
	ccrev = si_corerev(bus->sih);
	si_setcore(bus->sih, curcore, 0);

#if defined(FIS_WITH_CMN)
	/* if FIS dump with common subcore is collected, which happens
	 * only on android platforms which support reg on,
	 * skip DAP TMC flush as recommended by ASIC. Also do not update RWP.
	 */
	 skip_flush_and_rwp_update = dhdp->fis_triggered;
#endif /* FIS_WITH_CMN */

	/* version already validated */
	if (etb_hdr.version == EWP_ETB_CONFIG_INFO_VER_1) {
		eblk_offset = OFFSETOF(etb_config_info_v1_t, eblk);
	} else if (etb_hdr.version == EWP_ETB_CONFIG_INFO_VER_2) {
		eblk_offset = OFFSETOF(etb_config_info_v2_t, eblk);
	}

	ret = dhdpcie_bus_membytes(bus, FALSE, DHD_PCIE_MEM_BAR1,
		(ulong)(etb_config_info_addr + eblk_offset), (uint8 *)bus->eblk_buf,
		bus->eblk_buf_size);
	if (ret < 0) {
		DHD_ERROR(("%s: error reading ETB config header during dump\n",
			__FUNCTION__));
	}

	if (skip_flush_and_rwp_update) {
		DHD_PRINT(("%s: skip DAP TMC flush and RWP update due to FIS\n", __FUNCTION__));
	} else if (ccrev >= EWP_ETB_DAP_TMC_FLUSH_CCREV) {
		/* ASIC advise is to flush the DAP TMC registers
		 * before reading ETB dumps, so that ETB
		 * dump can be collected for non trap cases also.
		 * DAP - Debug Access Protocol
		 * TMC - Trace Memory Controller
		 */
		for (i = 0; i < etb_hdr.num_etb; ++i) {
			if (bus->etb_validity[i]) {
				ret = dhd_bus_flush_dap_tmc(bus, i);
				if (ret != BCME_OK) {
					DHD_ERROR(("%s:etb%d DAP TMC flush fails, "
						"collect etb dump via DAP\n",
						__FUNCTION__, i));
					flushed[i] = FALSE;
					all_flushed = FALSE;
				} else {
					flushed[i] = TRUE;
				}
			}
		}
	}

	/* write each individual etb */
	for (i = 0; i < etb_hdr.num_etb; ++i) {
		if (bus->etb_validity[i]) {
			uint idx = 0, debug_base = 0, addr = 0, val = 0;
			int j = 0;
			uint32 *etbdata = NULL;
			etb = &bus->eblk_buf[i];

			if ((totsize + etb->size + sizeof(*etb)) > bufsize) {
				DHD_ERROR(("%s: insufficient buffer space !\n",
					__FUNCTION__));
				return BCME_NOMEM;
			}

			/* get the DAP core backplane address */
			si_setcore(bus->sih, DAP_CORE_ID, 0);
			idx = si_findcoreidx(bus->sih, DAP_CORE_ID, 0);
			debug_base = si_get_coreaddr(bus->sih, idx) + dap_tmc_offset[i];
			si_setcore(bus->sih, curcore, 0);
			if (!debug_base) {
				DHD_ERROR(("%s: Failed to get core addr for idx 0x%x !\n",
					__FUNCTION__, idx));
				continue;
			}

			/* update the rwp in etb block */
			if (dhdp->dongle_trap_occured) {
				/* If fw has trapped, fw would have updated rwp in the etb
				 * config info
				 */
				rwpaddr = etb_config_info_addr +
					etb_cfg_size +
					(i * sizeof(etb_block_t)) +
					OFFSETOF(etb_block_t, rwp);
				ret = dhdpcie_bus_membytes(bus, FALSE, DHD_PCIE_MEM_BAR1,
					rwpaddr, (uint8 *)&rwp, sizeof(rwp));
				if (ret < 0) {
					DHD_ERROR(("%s: error reading etb%d rwp!\n",
						__FUNCTION__, i));
				} else {
					etb->rwp = ltoh32(rwp);
				}
			} else if (!skip_flush_and_rwp_update) {
				/* if no trap, then read rwp via DAP register */
				addr = debug_base + TMC_REG_OFF(rwp);
				serialized_backplane_access(bus, addr, 4, &etb->rwp, TRUE);
			} else {
				DHD_PRINT(("%s: no RWP update due to FIS\n", __FUNCTION__));
			}

			/* first write etb_block_t */
			memcpy(ptr, etb, sizeof(*etb));
			ptr += sizeof(*etb);

			/* ETBs not flushed will only read 0xff and will have inconsistent
			 * data as per ASIC, so just fill those with zeros
			 */
			if (!flushed[i]) {
				bzero(ptr, etb->size);
				totsize += etb->size + sizeof(*etb);
				ptr += etb->size;
				continue;
			}

			/* now write the etb contents */
			if (all_flushed) {
				ret = dhdpcie_bus_membytes(bus, FALSE, DHD_PCIE_MEM_BAR1,
					(ulong)etb->addr, ptr, etb->size);
				if (ret < 0) {
					DHD_ERROR(("%s: error reading etb%d\n",
						__FUNCTION__, i));
					return BCME_ERROR;
				}
			} else {
				/* if flush fails for even one ETB, then the ETB data
				 * has to be read via DAP even for those ETBs where
				 * flush was successful - as per ASIC
				 */

				/* first put the TMC in disabled state - reset bit0 of CTL reg */
				addr = debug_base + TMC_REG_OFF(ctl);
				serialized_backplane_access(bus, addr, 4, &val, TRUE);
				val &= ~1u;
				serialized_backplane_access(bus, addr, 4, &val, FALSE);
				OSL_DELAY(1000);
				/* verify TMC is now in ready state by reading bit2 of STS reg */
				addr = debug_base + TMC_REG_OFF(sts);
				serialized_backplane_access(bus, addr, 4, &val, TRUE);
				if (!(val & (1 << CORESIGHT_TMC_STS_READY_SHIFT))) {
					DHD_PRINT(("%s: etb%d TMC sts = 0x%x is not READY !\n",
						__FUNCTION__, i, val));
					continue;
				}
				/* set TMC RRP read pointer to 0 so that data is read
				 * from the beginning of the etb memory
				 */
				addr = debug_base + TMC_REG_OFF(rrp);
				val = 0;
				serialized_backplane_access(bus, addr, 4, &val, FALSE);
				/* now read the TMC RRD register in a loop to get the data
				 * from the etb memory
				 */
				etbdata = (uint32 *)ptr;
				addr = debug_base + TMC_REG_OFF(rrd);
				for (j = 0; j < etb->size/sizeof(uint32); ++j) {
					serialized_backplane_access(bus, addr, 4, etbdata, TRUE);
					++etbdata;
				}
			}
			totsize += etb->size + sizeof(*etb);
			ptr += etb->size;
		}
	}

	return totsize;
}

/*
 * dhd_bus_get_etb_dump - reads etb dumps over pcie bus into a buffer
*/
int
dhd_bus_get_etb_dump(dhd_bus_t *bus, uint8 *buf, uint bufsize)
{
	int ret = 0;

	ret = dhd_bus_get_etb_dump_cmn(bus, buf, bufsize, bus->etb_config_addr);

	return ret;
}

#define SDTC_ETB_DUMP_FILENAME "sdtc_etb_dump"
static int
dhd_sdtc_write_ewp_etb_dump(dhd_pub_t *dhdp)
{
	int size = 0;

	size = dhd_bus_get_ewp_etb_dump(dhdp->bus, dhdp->sdtc_etb_mempool,
		DHD_SDTC_ETB_MEMPOOL_SIZE);
	if (size < 0) {
		dhdp->sdtc_etb_dump_len = 0;
		return size;
	}

	/* sdtc_etb_dump_len should be set for HAL pull
	 * of etb dump
	 */
	dhdp->sdtc_etb_dump_len = size;
#ifdef DHD_DUMP_FILE_WRITE_FROM_KERNEL
	if (write_dump_to_file(dhdp, dhdp->sdtc_etb_mempool,
		size, SDTC_ETB_DUMP_FILENAME)) {
		DHD_ERROR(("%s: failed to dump %s file\n",
			__FUNCTION__, SDTC_ETB_DUMP_FILENAME));
	}
#endif /* DHD_DUMP_FILE_WRITE_FROM_KERNEL */

	return BCME_OK;
}

static int
dhd_write_etb_dump(dhd_pub_t *dhdp)
{
	int size = 0;

	DHD_TRACE(("Enter %s \n", __FUNCTION__));
	size = dhd_bus_get_etb_dump(dhdp->bus, dhdp->sdtc_etb_mempool,
		DHD_SDTC_ETB_MEMPOOL_SIZE);
	if (size < 0) {
		dhdp->sdtc_etb_dump_len = 0;
		return size;
	}

	/* sdtc_etb_dump_len should be set for HAL pull
	 * of etb dump
	 */
	dhdp->sdtc_etb_dump_len = size;
#ifdef DHD_DUMP_FILE_WRITE_FROM_KERNEL
	if (write_dump_to_file(dhdp, dhdp->sdtc_etb_mempool,
		size, SDTC_ETB_DUMP_FILENAME)) {
		DHD_ERROR(("%s: failed to dump %s file\n",
			__FUNCTION__, SDTC_ETB_DUMP_FILENAME));
	}
#endif /* DHD_DUMP_FILE_WRITE_FROM_KERNEL */

	return BCME_OK;
}

void
dhd_sdtc_etb_dump(dhd_pub_t *dhd)
{
	etb_info_t etb_info;
	uint8 *sdtc_etb_dump;
	uint8 *sdtc_etb_mempool;
	int ret = 0;

	if (!dhd->sdtc_etb_inited) {
		DHD_ERROR(("%s, SDTC ETB dump not supported\n", __FUNCTION__));
		return;
	}

	/* if newer ewp etb method is enabled, use that */
	if (dhd->ewp_etb_enabled) {
		ret = dhd_sdtc_write_ewp_etb_dump(dhd);
		if (ret != BCME_OK) {
			DHD_ERROR(("%s: failed to write ewp etb dump err=%d\n",
				__FUNCTION__, ret));
		}
		return;
	}

#if defined(DHD_SDTC_ETB_DUMP)
	/* if dap etb iovar based dump is enabled */
	if (dhd->etb_dump_inited) {
		ret = dhd_write_etb_dump(dhd);
		if (ret != BCME_OK) {
			DHD_ERROR(("%s: failed to write etb dump err=%d\n",
				__FUNCTION__, ret));
		}
		return;
	}
#endif /* DHD_SDTC_ETB_DUMP */

	bzero(&etb_info, sizeof(etb_info));

	ret = dhd_bus_get_etb_info(dhd, dhd->etb_addr_info.etbinfo_addr, &etb_info);
	if (ret) {
		DHD_ERROR(("%s: failed to get etb info %d\n", __FUNCTION__, ret));
		return;
	}

	if (etb_info.addr == (uint32)-1) {
		DHD_ERROR(("%s: invalid etbinfo.addr 0x%x Hence donot collect SDTC ETB\n",
			__FUNCTION__, etb_info.addr));
		return;
	}

	if (etb_info.read_bytes == 0) {
		DHD_ERROR(("%s ETB is of zero size. Hence donot collect SDTC ETB\n", __FUNCTION__));
		return;
	}

	DHD_PRINT(("%s etb_info ver:%d len:%d rwp:%d etb_full:%d etb:addr:0x%x, len:%d\n",
		__FUNCTION__, etb_info.version, etb_info.len,
		etb_info.read_write_p, etb_info.etb_full,
		etb_info.addr, etb_info.read_bytes));

	/*
	 * etb mempool format = etb_info + etb
	 */
	dhd->sdtc_etb_dump_len = etb_info.read_bytes + sizeof(etb_info);
	if (dhd->sdtc_etb_dump_len > DHD_SDTC_ETB_MEMPOOL_SIZE) {
		DHD_ERROR(("%s etb_dump_len: %d is more than the alloced %d.Hence cannot collect\n",
			__FUNCTION__, dhd->sdtc_etb_dump_len, DHD_SDTC_ETB_MEMPOOL_SIZE));
		return;
	}
	sdtc_etb_mempool = dhd->sdtc_etb_mempool;
	memcpy(sdtc_etb_mempool, &etb_info, sizeof(etb_info));
	sdtc_etb_dump = sdtc_etb_mempool + sizeof(etb_info);
	ret = dhd_bus_get_sdtc_etb(dhd, sdtc_etb_dump, etb_info.addr, etb_info.read_bytes);
	if (ret) {
		DHD_ERROR(("%s: error to get SDTC ETB ret: %d\n", __FUNCTION__, ret));
		return;
	}

	dhd_print_buf_addr(dhd, SDTC_ETB_DUMP_FILENAME,
		(uint8 *)sdtc_etb_mempool, dhd->sdtc_etb_dump_len);
	/*
	 * If kernel does not have file write access enabled
	 * then skip writing dumps to files.
	 * The dumps will be pushed to HAL layer which will
	 * write into files
	 */
#ifdef DHD_DUMP_FILE_WRITE_FROM_KERNEL
	if (write_dump_to_file(dhd, (uint8 *)sdtc_etb_mempool,
		dhd->sdtc_etb_dump_len, SDTC_ETB_DUMP_FILENAME)) {
		DHD_ERROR(("%s: failed to dump sdtc_etb to file\n",
			__FUNCTION__));
	}
#endif /* DHD_DUMP_FILE_WRITE_FROM_KERNEL */
}

int
dhd_sdtc_etb_hal_file_dump(void *dhd_pub, const void *user_buf, uint32 len)
{
	dhd_pub_t *dhdp = (dhd_pub_t *)dhd_pub;
	int pos = 0, ret = BCME_ERROR;

	if (dhdp->sdtc_etb_dump_len) {
		ret = dhd_export_debug_data((char *)dhdp->sdtc_etb_mempool,
			NULL, user_buf, dhdp->sdtc_etb_dump_len, &pos);
	} else {
		DHD_ERROR(("%s ETB is of zero size. Hence donot collect SDTC ETB\n", __FUNCTION__));
	}
	DHD_PRINT(("%s, done ret: %d\n", __FUNCTION__, ret));
	return ret;
}
#endif /* DHD_SDTC_ETB_DUMP */

#define GCI_MASTER_CFG_ASNI_BASE 0x18527000u
#define GCI_SLAVE_CFG_HMNI_BASE 0x18528000u
#define BT_MASTER_CFG_HSNI_BASE 0x18523000u
#define BT_SLAVE_CFG_HMNI_BASE 0x18524000u
#define APB_CB_PMNI_BASE 0x1851e000u
#define APB_AAON_PMNI_BASE 0x1851f000u
#define CC_SLAVE_CFG_AMNI_BASE 0x1851c000u
#define CC_MASTER_CFG_ASNI_BASE 0x1851a000u
#define PCIE_MASTER_CFG_ASNI_BASE 0x1851b000u
#define PCIE_SLAVE_CFG_AMNI_BASE 0x1851d000u
#define IDM_ERRSTATUS 0x110u
#define IDM_INTSTATUS 0x158u
#define GCI_BASE 0x18010000u
#define GCI_NCI_ERR_INT_STATUS 0xA04u
void
dhdpcie_print_amni_regs(dhd_bus_t *bus, bool trap_or_rot)
{
#ifdef DBG_PRINT_AMNI
	uint32 val = 0, pcie_ssctrl = 0;
	uint32 idm_errstatus = -1, idm_intstatus = -1, gci_nci_err_intstatus = -1;
	osl_t *osh = bus->osh;
	uint32 bar0 = 0;
	uint16 chipid = dhd_get_chipid(bus);

	/* AMNI dumps are supported only for NCI chips.
	 * this function can be invoked even before si_attach.
	 * so one needs to exit based on chipid only.
	 */
	if (!(BCM4390_CHIP(chipid))) {
		return;
	}

	bar0 =  OSL_PCI_READ_CONFIG(osh, PCI_BAR0_WIN, sizeof(uint32));
	/* set bar0 win to point to -
	 * 'Slave CFG Registers for chipcommon' AMNI[0] space
	 */
	OSL_PCI_WRITE_CONFIG(osh, PCI_BAR0_WIN, sizeof(uint32), CC_SLAVE_CFG_AMNI_BASE);
	/* enable indirect bpaccess */
	pcie_ssctrl = OSL_PCI_READ_CONFIG(osh, PCIE_CFG_SUBSYSTEM_CONTROL, sizeof(uint32));
	val = pcie_ssctrl | BP_INDACCESS_SHIFT;
	OSL_PCI_WRITE_CONFIG(osh, PCIE_CFG_SUBSYSTEM_CONTROL, sizeof(uint32), val);

	/* read idm_errstatus */
	OSL_PCI_WRITE_CONFIG(osh, PCI_CFG_INDBP_ADDR, sizeof(uint32), IDM_ERRSTATUS);
	idm_errstatus = OSL_PCI_READ_CONFIG(osh, PCI_CFG_INDBP_DATA, sizeof(uint32));
	DHD_PRINT(("%s:cc slave cfg AMNI, idm_errstatus(0x%x)=0x%x\n", __FUNCTION__,
		CC_SLAVE_CFG_AMNI_BASE + IDM_ERRSTATUS, idm_errstatus));

	/* read idm_interrupt_status */
	OSL_PCI_WRITE_CONFIG(osh, PCI_CFG_INDBP_ADDR, sizeof(uint32), IDM_INTSTATUS);
	idm_intstatus = OSL_PCI_READ_CONFIG(osh, PCI_CFG_INDBP_DATA, sizeof(uint32));
	DHD_PRINT(("%s: cc slave cfg AMNI, idm_interrupt_status(0x%x)=0x%x\n", __FUNCTION__,
		CC_SLAVE_CFG_AMNI_BASE + IDM_INTSTATUS, idm_intstatus));

	/* read gci_nci_err_int_status */
	/* set bar0 win to point to GCI space */
	OSL_PCI_WRITE_CONFIG(osh, PCI_BAR0_WIN, sizeof(uint32), GCI_BASE);
	OSL_PCI_WRITE_CONFIG(osh, PCI_CFG_INDBP_ADDR, sizeof(uint32),
		GCI_NCI_ERR_INT_STATUS);
	gci_nci_err_intstatus = OSL_PCI_READ_CONFIG(osh, PCI_CFG_INDBP_DATA,
		sizeof(uint32));
	DHD_PRINT(("%s: gci_nci_err_intstatus(0x%x)=0x%x\n", __FUNCTION__,
		GCI_BASE + GCI_NCI_ERR_INT_STATUS, gci_nci_err_intstatus));

	if (!trap_or_rot) {
		goto exit;
	}

	/* set bar0 win to point to -
	 * 'Master CFG Registers for chipcommon' ASNI[0] space
	 */
	OSL_PCI_WRITE_CONFIG(osh, PCI_BAR0_WIN, sizeof(uint32), CC_MASTER_CFG_ASNI_BASE);
	/* read idm_errstatus */
	OSL_PCI_WRITE_CONFIG(osh, PCI_CFG_INDBP_ADDR, sizeof(uint32), IDM_ERRSTATUS);
	val = OSL_PCI_READ_CONFIG(osh, PCI_CFG_INDBP_DATA, sizeof(uint32));
	DHD_PRINT(("%s:cc master cfg ASNI, idm_errstatus(0x%x)=0x%x\n", __FUNCTION__,
		CC_MASTER_CFG_ASNI_BASE + IDM_ERRSTATUS, val));

	/* set bar0 win to point to 'Master CFG Registers for pcie' ASNI[10] */
	OSL_PCI_WRITE_CONFIG(osh, PCI_BAR0_WIN, sizeof(uint32), PCIE_MASTER_CFG_ASNI_BASE);
	OSL_PCI_WRITE_CONFIG(osh, PCI_CFG_INDBP_ADDR, sizeof(uint32), IDM_ERRSTATUS);
	val = OSL_PCI_READ_CONFIG(osh, PCI_CFG_INDBP_DATA, sizeof(uint32));
	DHD_PRINT(("%s: master cfg pcie ASNI idm_errstatus(0x%x)=0x%x\n", __FUNCTION__,
		PCIE_MASTER_CFG_ASNI_BASE + IDM_ERRSTATUS, val));

	/* set bar0 win to point to 'Slave CFG Registers for pcie' AMNI[4] */
	OSL_PCI_WRITE_CONFIG(osh, PCI_BAR0_WIN, sizeof(uint32), PCIE_SLAVE_CFG_AMNI_BASE);
	OSL_PCI_WRITE_CONFIG(osh, PCI_CFG_INDBP_ADDR, sizeof(uint32), IDM_ERRSTATUS);
	val = OSL_PCI_READ_CONFIG(osh, PCI_CFG_INDBP_DATA, sizeof(uint32));
	DHD_PRINT(("%s: slave cfg pcie AMNI idm_errstatus(0x%x)=0x%x\n", __FUNCTION__,
		PCIE_SLAVE_CFG_AMNI_BASE + IDM_ERRSTATUS, val));

	/* set bar0 win to point to 'PMNI for APB-CB' PMNI[11] */
	OSL_PCI_WRITE_CONFIG(osh, PCI_BAR0_WIN, sizeof(uint32), APB_CB_PMNI_BASE);
	OSL_PCI_WRITE_CONFIG(osh, PCI_CFG_INDBP_ADDR, sizeof(uint32), IDM_ERRSTATUS);
	val = OSL_PCI_READ_CONFIG(osh, PCI_CFG_INDBP_DATA, sizeof(uint32));
	DHD_PRINT(("%s: APB-CB PMNI idm_errstatus(0x%x)=0x%x\n", __FUNCTION__,
		APB_CB_PMNI_BASE + IDM_ERRSTATUS, val));

	/* set bar0 win to point to 'PMNI for APB-AAON' PMNI[12] */
	OSL_PCI_WRITE_CONFIG(osh, PCI_BAR0_WIN, sizeof(uint32), APB_AAON_PMNI_BASE);
	OSL_PCI_WRITE_CONFIG(osh, PCI_CFG_INDBP_ADDR, sizeof(uint32), IDM_ERRSTATUS);
	val = OSL_PCI_READ_CONFIG(osh, PCI_CFG_INDBP_DATA, sizeof(uint32));
	DHD_PRINT(("%s: APB-AAON PMNI idm_errstatus(0x%x)=0x%x\n", __FUNCTION__,
		APB_AAON_PMNI_BASE + IDM_ERRSTATUS, val));

	/* set bar0 win to point to 'Master CFG Registers for BT' HSNI[13] */
	OSL_PCI_WRITE_CONFIG(osh, PCI_BAR0_WIN, sizeof(uint32), BT_MASTER_CFG_HSNI_BASE);
	OSL_PCI_WRITE_CONFIG(osh, PCI_CFG_INDBP_ADDR, sizeof(uint32), IDM_ERRSTATUS);
	val = OSL_PCI_READ_CONFIG(osh, PCI_CFG_INDBP_DATA, sizeof(uint32));
	DHD_PRINT(("%s: BT master cfg HSNI idm_errstatus(0x%x)=0x%x\n", __FUNCTION__,
		BT_MASTER_CFG_HSNI_BASE + IDM_ERRSTATUS, val));

	/* set bar0 win to point to 'Slave CFG Registers for BT' HMNI[9] */
	OSL_PCI_WRITE_CONFIG(osh, PCI_BAR0_WIN, sizeof(uint32), BT_SLAVE_CFG_HMNI_BASE);
	OSL_PCI_WRITE_CONFIG(osh, PCI_CFG_INDBP_ADDR, sizeof(uint32), IDM_ERRSTATUS);
	val = OSL_PCI_READ_CONFIG(osh, PCI_CFG_INDBP_DATA, sizeof(uint32));
	DHD_PRINT(("%s: BT slave cfg HMNI idm_errstatus(0x%x)=0x%x\n", __FUNCTION__,
		BT_SLAVE_CFG_HMNI_BASE + IDM_ERRSTATUS, val));

	/* set bar0 win to point to 'Master CFG Registers for gci' ASNI[1] */
	OSL_PCI_WRITE_CONFIG(osh, PCI_BAR0_WIN, sizeof(uint32), GCI_MASTER_CFG_ASNI_BASE);
	OSL_PCI_WRITE_CONFIG(osh, PCI_CFG_INDBP_ADDR, sizeof(uint32), IDM_ERRSTATUS);
	val = OSL_PCI_READ_CONFIG(osh, PCI_CFG_INDBP_DATA, sizeof(uint32));
	DHD_PRINT(("%s: GCI master cfg ASNI idm_errstatus(0x%x)=0x%x\n", __FUNCTION__,
		GCI_MASTER_CFG_ASNI_BASE + IDM_ERRSTATUS, val));

	/* set bar0 win to point to 'Slave CFG Registers for gci' HMNI[10] */
	OSL_PCI_WRITE_CONFIG(osh, PCI_BAR0_WIN, sizeof(uint32), GCI_SLAVE_CFG_HMNI_BASE);
	OSL_PCI_WRITE_CONFIG(osh, PCI_CFG_INDBP_ADDR, sizeof(uint32), IDM_ERRSTATUS);
	val = OSL_PCI_READ_CONFIG(osh, PCI_CFG_INDBP_DATA, sizeof(uint32));
	DHD_PRINT(("%s: GCI slave cfg HMNI idm_errstatus(0x%x)=0x%x\n", __FUNCTION__,
		GCI_SLAVE_CFG_HMNI_BASE + IDM_ERRSTATUS, val));

exit:
	/* restore back values */
	/* restore bar0 */
	OSL_PCI_WRITE_CONFIG(bus->osh, PCI_BAR0_WIN, sizeof(uint32), bar0);
	/* disable indirect bpaccess */
	OSL_PCI_WRITE_CONFIG(osh, PCIE_CFG_SUBSYSTEM_CONTROL,
		sizeof(uint32), pcie_ssctrl);
#endif /* DBG_PRINT_AMNI */
}
