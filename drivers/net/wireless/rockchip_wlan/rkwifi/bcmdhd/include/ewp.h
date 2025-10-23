/*
 * Broadcom EWP
 *
 * Software-specific EWP definitions shared between device and host side
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

#ifndef	_ewp_h_
#define	_ewp_h_

#include <typedefs.h>
#include <event_log.h>

#ifdef EWP_HW
#ifdef EVENT_LOG_COMPILE
#define EWP_LOG(args) EVENT_LOG_COMPACT_CAST_PAREN_ARGS(EVENT_LOG_TAG_EWP_HW, args)
#define EWP_ERR(args) EVENT_LOG_COMPACT_CAST_PAREN_ARGS(EVENT_LOG_TAG_EWP_HW, args)
#else
#define EWP_LOG(args)
#define EWP_ERR(args)
#endif /* EVENT_LOG_COMPILE */
#else
#define EWP_LOG(args)
#define EWP_ERR(args)
#endif /* EWP_HW */

enum {
	EWP_HW_STATE_INIT_PRESTART	= 0,		// f/w init has not yet started
	EWP_HW_STATE_INIT_START		= 1,		// f/w init has started
	EWP_HW_STATE_INIT_END		= 2,		// f/w init has ended successfully
	EWP_HW_STATE_INIT_TRAP		= 3,		// f/w init trapped
	EWP_HW_STATE_INIT_MAX
};

typedef struct ewp_hw_blk {
	uint PHYS_ADDR_N(addr);				// Start address of the buffer
	uint size;					// Size of the the buffer
} ewp_hw_blk_t;

/* Element indices to set elements in ewp_info structure using ewp_info_set() */
#define EWP_HW_INFO_INIT_LOG_BUF	1u		// init_log_buf
#define EWP_HW_INFO_MOD_DUMP_BUF	2u		// mod_dump_buf
#define EWP_HW_INFO_REG_DUMP_BUF	3u		// reg_dump_buf

#define EWP_HW_INFO_VER 0x1u
typedef struct ewp_hw_info {
	uint8 version;
	uint8 pad[2];
	uint8 pcie_hwhdr_rev;
	ewp_hw_blk_t init_log_buf;			// Info about buffer to save init logs
	ewp_hw_blk_t mod_dump_buf;			// Info about buffer to save module dumps
	ewp_hw_blk_t reg_dump_buf;			// Info about uffer to save reg dumps
} ewp_hw_info_t;

/* Element indices to set elements in ewp_info structure using ewp_info_set() */
#define EWP_INFO_HND_DEBUG		1u		// hnd_debug_addr
#define EWP_INFO_HND_DEBUG_PTR		2u		// hnd_debug_ptr_addr
#define EWP_INFO_SSSR_INFO		3u		// sssr_info_addr
#define EWP_INFO_SDTC_INFO		4u		// sdtc_info_addr

#ifdef COEX_CPU
#define EWP_INFO_COEX_CPU_INFO		5u		// coex_cpu_info_addr
#endif /* COEX_CPU */

#define EWP_INFO_ETB_CONFIG		6u		//etb_config_info_addr

#define EWP_INFO_VER 0x1u
typedef struct ewp_info {
	uint8 version;
	uint8 init_state;				// Init state
	uint16 flags;
	uint32 PHYS_ADDR_N(ewp_hw_info_addr);		// Ptr to ewp_hw_info_t
	uint32 PHYS_ADDR_N(hnd_debug_addr);		// Ptr to hnd_debug_t
	uint32 PHYS_ADDR_N(hnd_debug_ptr_addr);		// Ptr to hnd_debug_ptr_t
	uint32 PHYS_ADDR_N(sssr_info_addr);		// Ptr to sssr_reg_info_cmn_t
	uint32 PHYS_ADDR_N(sdtc_info_addr);		// Ptr to sdtc info
	uint32 PHYS_ADDR_N(coex_cpu_info_addr);		// Ptr to coex cpu info
	uint32 PHYS_ADDR_N(etb_config_info_addr);	// Ptr to etb_config_info_t
} ewp_info_t;

#ifdef COEX_CPU
#define EWP_COEX_CPU_INFO_VER 0x1u
typedef struct ewp_coex_cpu_info {
	uint8 version;
	uint8 pad[3];
	uint32 itcm_sz;
	uint32 dtcm_sz;
	uint32 PHYS_ADDR_N(itcm_base);
	uint32 PHYS_ADDR_N(dtcm_base);
} ewp_coex_cpu_info_t;
#endif /* COEX_CPU */

#define ETB_USER_SDTC_MAX_SIZE (32 * 1024)
#define ETB_USER_ETM_MAX_SIZE (32 * 1024)
#define ETB_USER_ETMCOEX_MAX_SIZE (16 * 1024)

typedef enum etb_user {
	ETB_USER_SDTC =         0,
	ETB_USER_ETM =          1,
	ETB_USER_ETM_COEX =     2,
	ETB_USER_MAC_SDTC =     3,
	ETB_USER_MAX
} etb_user_t;

typedef struct etb_block {
	bool inited;					// ETB block inited ?
	uint8 type;					// Size of the blk
	uint16 size;					// type: ETM CA7/CM7, SDTC
	uint32 addr;					// Base addr of the blk
	uint32 rwp;					// TMC read/write pointer
} etb_block_t;

#define EWP_ETB_CONFIG_INFO_VER_1 0x1u
typedef struct etb_config_info_cmn {
	uint8 version;					// Version
	uint8 num_etb;					// Max no. of ETBs
	uint8 pad[2];
} etb_config_info_cmn_t;

typedef struct etb_config_info_v1 {
	etb_config_info_cmn_t hdr;
	etb_block_t eblk[];				// Individual ETB blocks
} etb_config_info_v1_t;

#define EWP_ETB_CONFIG_INFO_VER_2 0x2u
typedef struct etb_config_info_v2 {
	etb_config_info_cmn_t hdr;
	uint32	ram_start_pa;				// RAM start address
	uint32	rom_start_pa;				// ROM start address
	uint32	aslr_ram_offset;			// ASLR RAM offset
	uint32	aslr_rom_offset;			// ASLR ROM offset
	uint32	ram_size;				// RAM Size
	uint32	rom_size;				// ROM size
	etb_block_t eblk[];				// Individual ETB blocks
} etb_config_info_v2_t;
#define EWP_ETB_CONFIG_INFO_VER	EWP_ETB_CONFIG_INFO_VER_2
typedef etb_config_info_v2_t etb_config_info_t;

#endif	/* _bcmpcie_h_ */
