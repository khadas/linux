/*
 * Misc utility routines for accessing chip-specific features
 * of the BOOKER NCI (non coherent interconnect) based Broadcom chips.
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
 *
 */

#ifndef _NCI_H
#define _NCI_H

#include <siutils.h>

#ifdef SOCI_NCI_BUS
struct nci_info;	/* public opaque type, contents private to nciutils.c */

void nci_uninit(void *nci);
uint32 nci_scan(si_t *sih);
void nci_dump_erom(void *nci);
void nci_cores_to_ai_cores(si_t *sih);
void *nci_init(si_t *sih, chipcregs_t *cc, uint bustype);
volatile void *nci_setcore(si_t *sih, uint coreid, uint coreunit);
volatile void *nci_setcoreidx(const si_t *sih, uint coreidx);
volatile void *nci_setcoreidx_wrap(const si_t *sih, uint coreidx, uint wrapper_idx);
uint nci_findcoreidx(const si_t *sih, uint coreid, uint coreunit);
volatile uint32 *nci_corereg_addr(si_t *sih, uint coreidx, uint regoff);
uint nci_corereg_writeonly(si_t *sih, uint coreidx, uint regoff, uint mask, uint val);
uint nci_corereg(const si_t *sih, uint coreidx, uint regoff, uint mask, uint val);
uint nci_corerev_minor(const si_t *sih);
uint nci_corerev(const si_t *sih);
uint nci_corevendor(const si_t *sih);
uint nci_get_wrap_reg(const si_t *sih, uint32 offset, uint32 mask, uint32 val);
void nci_core_reset(si_t *sih, uint32 bits, uint32 resetbits);
void nci_core_disable(const si_t *sih, uint32 bits);
bool nci_iscoreup(const si_t *sih);
uint32 nci_coreid(const si_t *sih, uint coreidx);
uint nci_numcoreunits(const si_t *sih, uint coreid);
uint32 nci_addr_space(const si_t *sih, uint spidx, uint baidx);
uint32 nci_addr_space_size(const si_t *sih, uint spidx, uint baidx);
bool nci_iscoreup(const si_t *sih);
uint nci_intflag(si_t *sih);
uint nci_flag(si_t *sih);
uint nci_flag_alt(const si_t *sih);
void nci_setint(const si_t *sih, int siflag);
uint32 nci_oobr_baseaddr(const si_t *sih, bool second);
uint nci_coreunit(const si_t *sih);
uint nci_corelist(const si_t *sih, uint coreid[]);
int nci_numaddrspaces(const si_t *sih);
uint32 nci_addrspace(const si_t *sih, uint spidx, uint baidx);
uint32 nci_addrspacesize(const si_t *sih, uint spidx, uint baidx);
void nci_coreaddrspaceX(const si_t *sih, uint asidx, uint32 *addr, uint32 *size);
uint32 nci_core_cflags(const si_t *sih, uint32 mask, uint32 val);
void nci_core_cflags_wo(const si_t *sih, uint32 mask, uint32 val);
uint32 nci_core_sflags(const si_t *sih, uint32 mask, uint32 val);
uint nci_wrapperreg(const si_t *sih, uint32 offset, uint32 mask, uint32 val);
void nci_invalidate_second_bar0win(si_t *sih);
int nci_backplane_access(si_t *sih, uint addr, uint size, uint *val, bool read);
int nci_backplane_access_64(si_t *sih, uint addr, uint size, uint64 *val, bool read);
uint nci_num_slaveports(const si_t *sih, uint coreid);
#if defined(BCMDBG) || defined(BCMDBG_DUMP) || defined(BCMDBG_PHYDUMP)
void nci_dumpregs(const si_t *sih, struct bcmstrbuf *b);
#endif  /* BCMDBG || BCMDBG_DUMP || BCMDBG_PHYDUMP */
#ifdef BCMDBG
void nci_view(si_t *sih, bool verbose);
void nci_viewall(si_t *sih, bool verbose);
#endif /* BCMDBG */
uint32 nci_get_nth_wrapper(const si_t *sih, int32 wrap_pos);
uint32 nci_get_axi_addr(const si_t *sih, uint32 *size, uint32 baidx);
uint32 *nci_wrapper_dump_binary_one(const si_info_t *sii, uint32 *p32, uint32 wrap_ba);
uint32 nci_wrapper_dump_binary(const si_t *sih, uchar *p);
uint32 nci_wrapper_dump_last_timeout(const si_t *sih, uint32 *error,
	uint32 *core, uint32 *ba, uchar *p);
uint32 nci_get_core_baaddr(const si_t *sih, uint32 *size, int32 baidx);
#ifdef AXI_TIMEOUTS
uint32 nci_clear_backplane_to(si_t *sih);
#endif /* AXI_TIMEOUTS */
uint32 nci_clear_backplane_to_per_core(si_t *sih, uint coreid, uint coreunit);
bool nci_ignore_errlog(const si_info_t *sii, const aidmp_t *ai,
	uint32 lo_addr, uint32 hi_addr, uint32 err_axi_id, uint32 errsts);
void nci_wrapper_get_last_error(const si_t *sih, uint32 *error_status, uint32 *core, uint32 *lo,
	uint32 *hi, uint32 *id);
uint32 nci_get_axi_timeout_reg(void);
uint32 *nci_wrapper_dump_binary_one(const si_info_t *sii, uint32 *p32, uint32 wrap_ba);
uint32 nci_wrapper_dump_binary(const si_t *sih, uchar *p);
uint32 nci_wrapper_dump_last_timeout(const si_t *sih, uint32 *error,
	uint32 *core, uint32 *ba, uchar *p);
bool nci_check_enable_backplane_log(const si_t *sih);
uint32 ai_wrapper_dump_buf_size(const si_t *sih);
uint32 nci_wrapper_dump_buf_size(const si_t *sih);
uint32 nci_get_sizeof_wrapper_offsets_to_dump(void);
void nci_update_backplane_timeouts(const si_t *sih, bool enable, uint32 idm_timeout_value,
	uint32 cid);
uint32 nci_get_coreaddr(const si_t *sih, uint coreidx);
bool nci_is_backplane_logs_enabled(void);
uint32 nci_get_curmap(struct nci_info *nci_info, uint coreidx);
int nci_get_amni_slave_cfg_cc_reg_addrs(si_t *sih, volatile uint32 **idm_errstatus_addr,
	volatile uint32 **idm_intstatus_addr);
#endif /* SOCI_NCI_BUS */
#endif /* _NCI_H */
