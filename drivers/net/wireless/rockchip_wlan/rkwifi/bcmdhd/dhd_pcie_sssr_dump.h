/*
* DHD Silicon Save Simulation Restore (SSSR)
* dump interface
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

#ifndef __DHD_PCIE_SSSR_DUMP_H__
#define __DHD_PCIE_SSSR_DUMP_H__

#ifdef DHD_SSSR_DUMP
void dhdpcie_set_pmu_fisctrlsts(struct dhd_bus *bus);
int dhd_bus_fis_trigger(dhd_pub_t *dhd);
int dhd_bus_fis_dump(dhd_pub_t *dhd);
bool dhd_bus_fis_fw_triggered_check(dhd_pub_t *dhd);
bool dhdpcie_set_collect_fis(struct dhd_bus *bus);
int dhdpcie_sssr_dump(dhd_pub_t *dhd);
int dhdpcie_fis_recover(dhd_pub_t *dhd);
int dhd_append_sssr_tlv(uint8 *buf_dst, int type_idx, int buf_remain);

#define SSSR_REG_INFO_VER_MAX	128u

#ifdef DHD_SSSR_DUMP_BEFORE_SR
#define DHD_SSSR_MEMPOOL_SIZE	(2 * 1024 * 1024) /* 2MB size */
#else
#define DHD_SSSR_MEMPOOL_SIZE	(1 * 1024 * 1024) /* 1MB size */
#endif /* DHD_SSSR_DUMP_BEFORE_SR */

/* used in sssr_dump_mode */
#define SSSR_DUMP_MODE_SSSR	0	/* dump both *before* and *after* files */
#define SSSR_DUMP_MODE_FIS	1	/* dump *after* files only */

#ifndef SSSR_HEADER_MAGIC
typedef struct sssr_header {
	uint32 magic; /* should be 53535352 = 'SSSR' */
	uint16 header_version; /* version number of this SSSR header */
	uint16 sr_version; /* version of SR version. This is to differentiate changes in SR ASM. */
	/*
	 * Header length from the next field ?data_len? and upto the start of
	 * binary_data[]. This is 20 bytes for version 0
	 */
	uint32 header_len;
	uint32 data_len;  /* number of bytes in binary_data[] */
	uint16 chipid;     /* chipid */
	uint16 chiprev;    /* chiprev */
	/*
	 * For D11 MAC/sAQM cores, the coreid, coreunit &  WAR_signature in the dump belong
	 * to respective cores. For the DIG SSSR dump these fields are extracted from the ARM core.
	 */
	uint16 coreid;
	uint16 coreunit;

	uint32 war_reg; /* Value of WAR register */
	uint32 flags;	/* For future use */

	uint8  binary_data[];
} sssr_header_t;
#define SSSR_HEADER_MAGIC 0x53535352u /* SSSR */
#endif /* SSSR_HEADER_MAGIC */

typedef enum sssr_subtype {
	SSSR_SAQM_DUMP = 0,
	SSSR_SRCB_DUMP = 1,
	SSSR_CMN_DUMP = 2
} sssr_subtype_t;

extern int dhd_sssr_mempool_init(dhd_pub_t *dhd);
extern void dhd_sssr_mempool_deinit(dhd_pub_t *dhd);
extern int dhd_sssr_dump_init(dhd_pub_t *dhd, bool fis_dump);
extern void dhd_sssr_dump_deinit(dhd_pub_t *dhd);
extern void dhd_sssr_print_filepath(dhd_pub_t *dhd, char *path);
extern int dhd_sssr_reg_info_init(dhd_pub_t *dhd);
extern void dhd_sssr_reg_info_deinit(dhd_pub_t *dhd);
extern uint dhd_sssr_dig_buf_size(dhd_pub_t *dhdp);
extern uint dhd_sssr_dig_buf_addr(dhd_pub_t *dhdp);
extern uint dhd_sssr_mac_buf_size(dhd_pub_t *dhdp, uint8 core_idx);
extern uint dhd_sssr_mac_xmtaddress(dhd_pub_t *dhdp, uint8 core_idx);
extern uint dhd_sssr_mac_xmtdata(dhd_pub_t *dhdp, uint8 core_idx);
extern int dhd_sssr_mac_war_reg(dhd_pub_t *dhdp, uint8 core_idx, uint32 *war_reg);
extern int dhd_sssr_arm_war_reg(dhd_pub_t *dhdp, uint32 *war_reg);
extern int dhd_sssr_saqm_war_reg(dhd_pub_t *dhdp, uint32 *war_reg);
extern int dhd_sssr_srcb_war_reg(dhd_pub_t *dhdp, uint32 *war_reg);
extern int dhd_sssr_sr_asm_version(dhd_pub_t *dhdp, uint16 *sr_asm_version);
extern uint dhd_sssr_saqm_buf_size(dhd_pub_t *dhdp);
extern uint dhd_sssr_saqm_buf_addr(dhd_pub_t *dhdp);
extern uint dhd_sssr_srcb_buf_size(dhd_pub_t *dhdp);
extern uint dhd_sssr_srcb_buf_addr(dhd_pub_t *dhdp);
extern uint dhd_sssr_cmn_buf_size(dhd_pub_t *dhdp);
extern uint dhd_sssr_cmn_buf_addr(dhd_pub_t *dhdp);
#define DHD_SSSR_MEMPOOL_INIT(dhdp)	dhd_sssr_mempool_init(dhdp)
#define DHD_SSSR_MEMPOOL_DEINIT(dhdp) dhd_sssr_mempool_deinit(dhdp)
#define DHD_SSSR_DUMP_INIT(dhdp)	dhd_sssr_dump_init(dhdp, FALSE)
#define DHD_SSSR_DUMP_DEINIT(dhdp) dhd_sssr_dump_deinit(dhdp)
#define DHD_SSSR_PRINT_FILEPATH(dhdp, path) dhd_sssr_print_filepath(dhdp, path)
#define DHD_SSSR_REG_INFO_INIT(dhdp)	dhd_sssr_reg_info_init(dhdp)
#define DHD_SSSR_REG_INFO_DEINIT(dhdp) dhd_sssr_reg_info_deinit(dhdp)

#ifdef DHD_SSSR_DUMP_BEFORE_SR
int dhd_sssr_dump_d11_buf_before(void *dhd_pub, const void *user_buf,
	uint32 len, int core);
int dhd_sssr_dump_dig_buf_before(void *dhd_pub, const void *user_buf,
	uint32 len);
#endif /* DHD_SSSR_DUMP_BEFORE_SR */
int dhd_sssr_dump_d11_buf_after(void *dhd_pub, const void *user_buf,
	uint32 len, int core);
int dhd_sssr_dump_dig_buf_after(void *dhd_pub, const void *user_buf,
	uint32 len);

bool dhd_is_fis_enabled(void);
#endif /* DHD_SSSR_DUMP */

#endif /* __DHD_PCIE_SSSR_DUMP_H__ */
