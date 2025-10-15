/*
 * Misc utility routines for accessing chip-specific features
 * of the BOOKER NCI (non coherent interconnect) based Broadcom chips.
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
#include <sbchipc.h>
#include <pcicfg.h>
#include <pcie_core.h>
#include <hndsoc.h>
#include "siutils_priv.h"
#include <nci.h>
#include <bcmdevs.h>
#include <hndoobr.h>
#include <bcmutils.h>
#include <hal_nci_cmn.h>

#if defined(EVENT_LOG_COMPILE)
#include <event_log.h>
#endif /* EVENT_LOG_COMPILE */

#define NCI_BAD_INDEX			-1		/* Bad Index */

#define OOBR_BASE_MASK			0x00001FFFu	/* Mask to get Base address of OOBR */
#define EROM1_BASE_MASK			0x00000FFFu	/* Mask to get Base address of EROM1 */
#define DOMAIN_AAON			0xffU		/* Domain id when core is in AAON domain */
/* Core Info */
#define COREINFO_COREID_MASK		0x00000FFFu	/* Bit-11 to 0 */
#define COREINFO_REV_MASK		0x000FF000u	/* Core Rev Mask */
#define COREINFO_REV_SHIFT		12u		/* Bit-12 */
#define COREINFO_MFG_MASK		0x00F00000u	/* Core Mfg Mask */
#define COREINFO_MFG_SHIFT		20u		/* Bit-20 */
#define COREINFO_BPID_MASK		0x07000000u	/* 26-24 Gives Backplane ID */
#define COREINFO_BPID_SHIFT		24u		/* Bit:26-24 */
#define COREINFO_BPID_FIELD2_MASK	0x10000000u	/* Backplaneid[3] = core_info[28] */
#define COREINFO_BPID_FIELD2_SHIFT	28u		/* Bit:28 */
#define COREINFO_ISBP_MASK		0x08000000u	/* Is Backplane or Bridge */
#define COREINFO_ISBP_SHIFT		27u		/* Bit:27 */

/* Interface Config */
#define IC_IFACECNT_MASK		0x0000F000u	/* No of Interface Descriptor Mask */
#define IC_IFACECNT_SHIFT		12u		/* Bit-12 */
#define IC_IFACEOFFSET_MASK		0x00000FFFu	/* OFFSET for 1st Interface Descriptor */

/* DMP Reg Offset */
#define DMP_DMPCTRL_REG_OFFSET		8u

/* Interface Descriptor Masks */
#define ID_NODEPTR_MASK			0xFFFFFFF8u	/* Master/Slave Network Interface Addr */
#define ID_NODETYPE_MASK		0x00000007u	/* 0:Booker 1:IDM 1-0xf:Reserved */
#define ID_WORDOFFSET_MASK		0xF0000000u	/* WordOffset to next Iface Desc in EROM2 */
#define ID_WORDOFFSET_SHIFT		28u		/* WordOffset bits 31-28 */
#define ID_CORETYPE_MASK		0x08000000u	/* CORE belongs to OOBR(0) or EROM(1) */
#define ID_CORETYPE_SHIFT		27u		/* Bit-27 */
#define ID_MI_MASK			0x04000000u	/* 0: Slave Interface, 1:Master Interface */
#define ID_MI_SHIFT			26u		/* Bit-26 */
#define ID_NADDR_MASK			0x03000000u	/* No of Slave Address Regions */
#define ID_NADDR_SHIFT			24u		/* Bit:25-24 */
#define ID_BPID_MASK			0x00F00000u	/* Give Backplane ID */
#define ID_BPID_SHIFT			20u		/* Bit:20-23 */
#define ID_COREINFOPTR_MASK		0x00001FFFu	/* OOBR or EROM Offset */
#define ID_ENDMARKER			0xFFFFFFFFu	/* End of EROM Part 2 */

/* Slave Port Address Descriptor Masks */
#define SLAVEPORT_BASE_ADDR_MASK	0xFFFFFF00u	/* Bits 31:8 is the base address */
#define SLAVEPORT_BOUND_ADDR_MASK	0x00000040u	/* Addr is not 2^n and with bound addr */
#define SLAVEPORT_BOUND_ADDR_SHIFT	6u		/* Bit-6 */
#define SLAVEPORT_64BIT_ADDR_MASK	0x00000020u	/* 64-bit base and bound fields */
#define SLAVEPORT_64BIT_ADDR_SHIFT	5u		/* Bit-5 */
#define SLAVEPORT_ADDR_SIZE_MASK	0x0000001Fu	/* Address Size mask */
#define SLAVEPORT_ADDR_TYPE_BOUND	0x1u		/* Bound Addr */
#define SLAVEPORT_ADDR_TYPE_64		0x2u		/* 64-Bit Addr */
#define SLAVEPORT_ADDR_MIN_SHIFT	0x8u
/* Address space Size of the slave port */
#define SLAVEPORT_ADDR_SIZE(adesc)	(1u << ((adesc & SLAVEPORT_ADDR_SIZE_MASK) + \
			SLAVEPORT_ADDR_MIN_SHIFT))

#define GET_NEXT_EROM_ADDR(addr)	((uint32 *)((uintptr)(addr) + 4u))

#define NCI_DEFAULT_CORE_UNIT		(0u)

#define IDM_RESET_ACC_IRQ_MASK		(1u << 0u)	/* Error interrupt when wrapper in reset */
#define IDM_ERR_IRQ_MASK		(1u << 2u)	/* Error detection event mask */
#define	IDM_TD_IRQ_MASK			(1u << 3u)	/* Timeout detection event mask */

/* Error Codes */
enum {
	NCI_OK				= 0,
	NCI_BACKPLANE_ID_MISMATCH	= -1,
	NCI_INVALID_EROM2PTR		= -2,
	NCI_WORDOFFSET_MISMATCH		= -3,
	NCI_NOMEM			= -4,
	NCI_MASTER_INVALID_ADDR		= -5
};

#define GET_OOBR_BASE(erom2base)	((erom2base) & ~OOBR_BASE_MASK)
#define GET_EROM1_BASE(erom2base)	((erom2base) & ~EROM1_BASE_MASK)
#define CORE_ID(core_info)		((core_info) & COREINFO_COREID_MASK)
#define GET_INFACECNT(iface_cfg)	(((iface_cfg) & IC_IFACECNT_MASK) >> IC_IFACECNT_SHIFT)
#define GET_NODEPTR(iface_desc_0)	((iface_desc_0) & ID_NODEPTR_MASK)
#define GET_NODETYPE(iface_desc_0)	((iface_desc_0) & ID_NODETYPE_MASK)
#define GET_WORDOFFSET(iface_desc_1)	(((iface_desc_1) & ID_WORDOFFSET_MASK) \
					>> ID_WORDOFFSET_SHIFT)
#define IS_MASTER(iface_desc_1)		(((iface_desc_1) & ID_MI_MASK) >> ID_MI_SHIFT)
#define GET_CORETYPE(iface_desc_1)	(((iface_desc_1) & ID_CORETYPE_MASK) >> ID_CORETYPE_SHIFT)
#define GET_NUM_ADDR_REG(iface_desc_1)	(((iface_desc_1) & ID_NADDR_MASK) >> ID_NADDR_SHIFT)
#define GET_COREOFFSET(iface_desc_1)	((iface_desc_1) & ID_COREINFOPTR_MASK)
#define ADDR_SIZE(sz)			((1u << ((sz) + 8u)) - 1u)

#define CORE_REV(core_info)		((core_info) & COREINFO_REV_MASK) >> COREINFO_REV_SHIFT
#define CORE_MFG(core_info)		((core_info) & COREINFO_MFG_MASK) >> COREINFO_MFG_SHIFT

/* BITs [2-0] is from core_info[26:24] & BIT[3] is from core_info[28] */
#define COREINFO_BPID(core_info)	((((core_info) & COREINFO_BPID_MASK) >> \
						COREINFO_BPID_SHIFT) | \
						((((core_info) & COREINFO_BPID_FIELD2_MASK) >> \
						COREINFO_BPID_FIELD2_SHIFT) << 3u))

#define IS_BACKPLANE(core_info)		(((core_info) & COREINFO_ISBP_MASK) >> COREINFO_ISBP_SHIFT)
#define ID_BPID(iface_desc_1)		(((iface_desc_1) & ID_BPID_MASK) >> ID_BPID_SHIFT)
#define IS_BACKPLANE_ID_SAME(core_info, iface_desc_1) \
					(COREINFO_BPID((core_info)) == ID_BPID((iface_desc_1)))

#define NCI_IS_INTERFACE_ERR_DUMP_ENAB(ifdesc) \
	((ifdesc)->node_type == NODE_TYPE_BOOKER && ((ifdesc)->node_ptr))

#define NCI_WORD_SIZE			(4u)
#define PCI_ACCESS_SIZE			(4u)
#define OOBR_CORE_SIZE			(0x80u)
#define OOBR_CORE0_OFFSET		(0x100u)
#define OOBR_COREINFO_OFFSET		(0x40u)

#define NCI_ADDR2NUM(addr)		((uintptr)(addr))
#define NCI_ADD_NUM(addr, size)		(NCI_ADDR2NUM(addr) + (size))
#define NCI_SUB_NUM(addr, size)		(NCI_ADDR2NUM(addr) - (size))
#ifdef DONGLEBUILD
#define NCI_ADD_ADDR(addr, size)	((uint32 *)REG_MAP(NCI_ADD_NUM((addr), (size)), 0u))
#define NCI_SUB_ADDR(addr, size)	((uint32 *)REG_MAP(NCI_SUB_NUM((addr), (size)), 0u))
#else /* !DONGLEBUILD */
#define NCI_ADD_ADDR(addr, size)	((uint32 *)(NCI_ADD_NUM((addr), (size))))
#define NCI_SUB_ADDR(addr, size)	((uint32 *)(NCI_SUB_NUM((addr), (size))))
#endif /* DONGLEBUILD */
#define NCI_INC_ADDR(addr, size)	((addr) = NCI_ADD_ADDR((addr), (size)))
#define NCI_DEC_ADDR(addr, size)	((addr) = NCI_SUB_ADDR((addr), (size)))

#define NODE_TYPE_BOOKER		0x0u
#define NODE_TYPE_NIC400		0x1u

/* Core's Backplane ID's */
#define BP_BOOKER			0x0u
/* 4397A0 Backplane ID's */
#define BP_NIC400			0x1u
#define BP_APB1                         0x2u
#define BP_APB2                         0x3u
#define BP_NIC_CCI400                   0x4u
/* 4397B0 Backplane ID's */
#define BP_BKR_CCI400			0x1u
#define BP_WL_PMNI			0x2u
#define BP_CMN_PMNI			0x3u
#define BP_AON_PMNI			0x4u
#define BP_MAIN_PMNI			0x5u
#define BP_AUX_PMNI			0x6u
#define BP_SCAN_PMNI			0x7u
#define BP_SAQM_PMNI			0x8u
/* AI Backplane ID's */
#define BP_NIC400_CB			0x0u
#define BP_NIC400_WL			0x1u
#define BP_APB_WL			0x2u
#define BP_APB_CB0			0x3u
#define BP_APB_CB1			0x4u
#define BP_CCI400			0x5u

#define PCIE_WRITE_SIZE			4u

#define BACKPLANE_ID_STR_SIZE		11u
char BACKPLANE_ID_NAME_AI[][BACKPLANE_ID_STR_SIZE] = {
	"NIC400_CB",
	"NIC400_WL",
	"APB_WL0",
	"APB_CB0",
	"APB_CB1",
	"CCI400",
	"\0"
};
char BACKPLANE_ID_NAME_4397A0[][BACKPLANE_ID_STR_SIZE] = {
	"BOOKER",
	"NIC400",
	"APB1",
	"APB2",
	"CCI400",
	"\0"
};
char BACKPLANE_ID_NAME[][BACKPLANE_ID_STR_SIZE] = {
	"BOOKER",
	"CCI400",
	"WL_PMNI",
	"CMN_PMNI",
	"AON_PMNI",
	"MAIN_PMNI",
	"AUX_PMNI",
	"SCAN_PMNI",
	"SAQM_PMNI",
	"\0"
};

/* Coreid's to skip for BP timeout/error config and wrapper dump */
static const uint16 BCMPOST_TRAP_RODATA(nci_wraperr_skip_coreids)[] = {
#ifdef BCMFPGA
	DAP_CORE_ID,
#endif /* BCMFPGA */
	BT_CORE_ID,
	};

/* BOOKER NCI LOG LEVEL */
#define NCI_LOG_LEVEL_ERROR		0x1u
#define NCI_LOG_LEVEL_TRACE		0x2u
#define NCI_LOG_LEVEL_INFO		0x4u
#define NCI_LOG_LEVEL_PRINT		0x8u

#ifndef NCI_DEFAULT_LOG_LEVEL
#define NCI_DEFAULT_LOG_LEVEL	(NCI_LOG_LEVEL_ERROR)
#endif /* NCI_DEFAULT_LOG_LEVEL */

uint32 nci_log_level = NCI_DEFAULT_LOG_LEVEL;

#if defined(EVENT_LOG_COMPILE)
#if defined(ERR_USE_EVENT_LOG_RA)
#define EVTLOG(x, y)		EVENT_LOG_RA(x, y)
#else /* !ERR_USE_EVENT_LOG_RA */
#define EVTLOG(x, y)		EVENT_LOG_COMPACT_CAST_PAREN_ARGS(x, y)
#endif /* ERR_USE_EVENT_LOG_RA */
#define NCI_ERROR(args)		EVTLOG(EVENT_LOG_TAG_BCMHAL_SOCI_NCI_ERROR, args)
#define NCI_TRACE(args)
#define NCI_INFO(args)		EVTLOG(EVENT_LOG_TAG_BCMHAL_SOCI_NCI_INFO, args)
#define NCI_PRINT(args)		posttrap_printf args
#else
#ifndef BCM_BOOTLOADER
#define NCI_ERROR(args) do { \
		if (nci_log_level & NCI_LOG_LEVEL_ERROR) { \
			posttrap_printf args; \
		} \
	} while (0u)
#define NCI_TRACE(args) do { \
		if (nci_log_level & NCI_LOG_LEVEL_TRACE) { \
			posttrap_printf args; \
		} \
	} while (0u)
#define NCI_INFO(args)  do { \
		if (nci_log_level & NCI_LOG_LEVEL_INFO) { \
			posttrap_printf args; \
		} \
	} while (0u)
#define NCI_PRINT(args) do { \
		if (nci_log_level & NCI_LOG_LEVEL_PRINT) { \
			posttrap_printf args; \
		} \
	} while (0u)
#else
#define NCI_ERROR(args)
#define NCI_TRACE(args)
#define NCI_INFO(args)
#define NCI_PRINT(args)
#endif /* BCM_BOOTLOADER */
#endif /* EVENT_LOG_COMPILE */

#define NCI_EROM_WORD_SIZEOF		4u
#define NCI_REGS_PER_CORE		2u

#define NCI_EROM1_LEN(erom2base)	(erom2base - GET_EROM1_BASE(erom2base))
#define NCI_NONOOBR_CORES(erom2base)	NCI_EROM1_LEN(erom2base) \
						/(NCI_REGS_PER_CORE * NCI_EROM_WORD_SIZEOF)

typedef struct slave_port {
	uint32		adesc;		/**< Address Descriptor 0 */
	uint32		addrl;		/**< Lower Base */
	uint32		addrh;		/**< Upper Base */
	uint32		extaddrl;	/**< Lower Bound */
	uint32		extaddrh;	/**< Ubber Bound */
} slave_port_t;

typedef struct interface_desc {
	slave_port_t	*sp;		/**< Slave Port Addr 0-3 */

	uint32		iface_desc_0;	/**< Interface-0 Descriptor Word0 */
	/* If Node Type 0-Booker xMNI/xSNI address. If Node Type 1-DMP wrapper Address */
	uint32		node_ptr;	/**< Core's Node pointer */

	uint32		iface_desc_1;	/**< Interface Descriptor Word1 */
	uint8		num_addr_reg;	/**< Number of Slave Port Addr (Valid only if master=0) */

	uint8           oobr_core : 1;	/**< 1:OOBR 0:NON-OOBR */
	uint8		master : 1;	/**< 1:Master 0:Slave */
	uint8		is_booker : 1;	/**< 1:TRUE 0:FALSE */
	uint8		is_nic400 : 1;	/**< 1:TRUE 0:FALSE */
	uint8		is_pmni : 1;	/**< APB/PMNI 1:TRUE 0:FALSE */
	uint8		is_axi : 1;	/**< 1:TRUE 0:FALSE */
	uint8		is_shared_pmni : 1;	/**< Set if PMNI is shared */

	uint8		node_type;	/**< 0:Booker , 1:IDM Wrapper, 2-0xf: Reserved */
} interface_desc_t;

typedef struct nci_cores {
	void		*regs;
	/* 2:0-Node type (0-booker,1-IDM Wrapper) 31:3-Interconnect registyer space */
	interface_desc_t *desc;		/**< Interface & Address Descriptors */
	/*
	 * 11:0-CoreID, 19:12-RevID 23:20-MFG 26:24-Backplane ID if
	 * bit 27 is 1 (Core is Backplane or Bridge )
	 */
	uint32		coreinfo;	/**< CoreInfo of each core */
	/*
	 * 11:0 - Offosewt of 1st Interface desc in EROM 15:12 - No.
	 * of interfaces attachedto this core
	 */
	uint32		iface_cfg;	/**< Interface config Reg */
	uint32		dmp_regs_off;	/**< DMP control & DMP status @ 0x48 from coreinfo */
	uint32		coreid;		/**< id of each core */
	uint8		coreunit;	/**< Unit differentiate same coreids */
	uint8		iface_cnt;	/**< no of Interface connected to each core */
	uint8		domain;		/**< power domain number, 0xff means its in AAON domain */
	uint8		PAD;
} nci_cores_t;

typedef struct nci_info {
	void		*osh;		/**< osl os handle */
	nci_cores_t	*cores;		/**< Cores Parsed */
	void		*pci_bar_addr;	/**< PCI BAR0 Window */
	uint32		cc_erom2base;	/**< Base of EROM2 from ChipCommon */
	uint32		*erom1base;	/**< Base of EROM1 */
	uint32		*erom2base;	/**< Base of EROM2 */
	uint32		*oobr_base;	/**< Base of OOBR */
	uint16		bustype;	/**< SI_BUS, PCI_BUS */
	uint8		max_cores;	/**< # Max cores indicated by Register */
	uint8		num_cores;	/**< # discovered cores */
	uint8		refcnt;		/**< Allocation reference count  */
	uint8		scan_done;	/**< Set to TRUE when erom scan is done. */
	uint8		PAD[2];
	uint32		exp_next_oobr_offset;
} nci_info_t;

/* DMP/io control and DMP/io status */
typedef struct dmp_regs {
	uint32 dmpctrl;			/* Exists only for OOBR Cores */
	uint32 dmpstatus;
} dmp_regs_t;

typedef struct dump_regs {
	uint32 offset;
	uint32 bit_map;
} dump_regs_t;

static const dump_regs_t nci_wrapper_regs[] = {
	{0x100, 0xc7fffc77},
	{0x180, 0xcdd}
};

#ifdef _RTE_
static nci_info_t *knci_info;
#endif /* _RTE_ */

static void nci_update_shared_pmni_iface(nci_info_t *nci);
static void nci_save_iface1_reg(si_t *sih, interface_desc_t *desc, uint32 iface_desc_1,
	uint32 *erom2ptr);
static uint32 *nci_save_slaveport_addr(nci_info_t *nci,
	interface_desc_t *desc, uint32 *erom2ptr);
static int nci_get_coreunit(nci_cores_t *cores, uint32 numcores, uint cid,
	uint32 iface_desc_1);
static nci_cores_t *nci_initial_parse(nci_info_t *nci, uint32 *erom2ptr, uint32 *core_idx);
static void nci_check_extra_core(nci_info_t *nci, uint32 *erom2ptr, uint32 *core_idx);
static void _nci_setcoreidx_pcie_bus(const si_t *sih, volatile void **regs, uint32 curmap,
	uint32 curwrap);
static volatile void *_nci_setcoreidx(const si_t *sih, uint coreidx, uint wrapper_idx);
static uint32 _nci_get_curwrap(nci_info_t *nci, uint coreidx, uint wrapper_idx);
static uint32 nci_get_curwrap(nci_info_t *nci, uint coreidx, uint wrapper_idx);
static uint32 _nci_get_curmap(nci_info_t *nci, uint coreidx, uint slave_port_idx, uint base_idx);
static void nci_core_reset_iface(const si_t *sih, uint32 bits, uint32 resetbits, uint iface_idx,
		bool enable);
static void nci_update_domain_id(si_t *sih);
void nci_core_reset_enable(const si_t *sih, uint32 bits, uint32 resetbits, bool enable);
bool nci_is_dump_err_disabled(const si_t *sih, nci_cores_t *core);

uint32 nci_dump_and_clr_err_log(si_t *sih, uint32 coreidx, uint32 iface_idx, void *wrapper,
	bool is_master, uint32 wrapper_daddr);
static uint32 nci_find_numcores(si_t *sih);
static int32 nci_find_first_wrapper_idx(nci_info_t *nci, uint32 coreidx);

/*
 * Description : This function will search for a CORE with matching 'core_id' and mismatching
 * 'wordoffset', if found then increments 'coreunit' by 1.
 */
/* TODO: Need to understand this. */
static int
BCMATTACHFN(nci_get_coreunit)(nci_cores_t *cores, uint32 numcores,
		uint core_id, uint32 iface_desc_1)
{
	uint32 core_idx;
	uint32 coreunit = NCI_DEFAULT_CORE_UNIT;

	for (core_idx = 0u; core_idx < numcores; core_idx++) {
		if ((cores[core_idx].coreid == core_id) &&
			(!cores[core_idx].desc ||
			(GET_COREOFFSET(cores[core_idx].desc->iface_desc_1) !=
			GET_COREOFFSET(iface_desc_1)))) {
			coreunit = cores[core_idx].coreunit + 1;
		}
	}

	return coreunit;
}

/*
 * OOBR Region
	+-------------------------------+
	+				+
	+	OOBR with EROM Data	+
	+				+
	+-------------------------------+
	+				+
	+	EROM1			+
	+				+
	+-------------------------------+  --> ChipCommon.EROMBASE
	+				+
	+	EROM2			+
	+				+
	+-------------------------------+
*/

/**
 * Function : nci_init
 * Description : Malloc's memory related to 'nci_info_t' and its internal elements.
 *
 * @paramter[in]
 * @regs : This is a ChipCommon Regster
 * @bustype : Bus Connect Type
 *
 * Return : On Succes 'nci_info_t' data structure is returned as void,
 *	where all EROM parsed Cores are saved,
 *	using this all EROM Cores are Freed.
 *	On Failure 'NULL' is returned by printing ERROR messages
 */

void*
BCMATTACHFN(nci_init)(si_t *sih, chipcregs_t *cc, uint bustype)
{
	si_info_t *sii = SI_INFO(sih);
	nci_cores_t *cores;
	nci_info_t *nci = NULL;
	uint8 err_at = 0u;

#ifdef _RTE_
	if (knci_info) {
		knci_info->refcnt++;
		nci = knci_info;

		goto end;
	}
#endif /* _RTE_ */

	/* It is used only when NCI_ERROR is used */
	BCM_REFERENCE(err_at);

	nci = MALLOCZ(sii->osh, sizeof(*nci));
	if (nci == NULL) {
		err_at = 1u;
		goto end;
	}
	sii->nci_info = nci;

	nci->osh = sii->osh;
	nci->refcnt++;

	nci->cc_erom2base = GET_NODEPTR(R_REG(nci->osh, CC_REG_ADDR(cc, EromPtrOffset)));
	nci->bustype = bustype;
	switch (nci->bustype) {
	case SI_BUS:
		nci->erom2base = (uint32 *)REG_MAP(nci->cc_erom2base, 0u);
		nci->oobr_base = (uint32 *)REG_MAP(GET_OOBR_BASE(nci->cc_erom2base), 0u);
		nci->erom1base = (uint32 *)REG_MAP(GET_EROM1_BASE(nci->cc_erom2base), 0u);

		break;

	case PCI_BUS:
		/* Set wrappers address */
		sii->curwrap = (void *)((uintptr)cc + SI_CORE_SIZE);
		/* Set access window to Erom Base(For NCI, EROM starts with OOBR) */
		OSL_PCI_WRITE_CONFIG(nci->osh, PCI_BAR0_WIN, PCI_ACCESS_SIZE,
			GET_EROM1_BASE(nci->cc_erom2base));
		nci->erom1base = (uint32 *)((uintptr)cc);
		nci->erom2base = (uint32 *)((uintptr)cc + NCI_EROM1_LEN(nci->cc_erom2base));

		break;

#ifdef BCMSDIO
	case SPI_BUS:
	case SDIO_BUS:
		nci->erom2base = (uint32 *)((uintptr)nci->cc_erom2base);
		nci->oobr_base = (uint32 *)((uintptr)GET_OOBR_BASE(nci->cc_erom2base));
		nci->erom1base = (uint32 *)((uintptr)GET_EROM1_BASE(nci->cc_erom2base));
		break;
#endif  /* BCMSDIO */

	default:
		err_at = 2u;
		ASSERT(0u);
		goto end;
	}

	nci->max_cores = nci_find_numcores(sih);
	if (!nci->max_cores) {
		err_at = 3u;
		goto end;
	}

	cores = MALLOCZ(nci->osh, sizeof(*cores) * nci->max_cores);
	if (cores == NULL) {
		err_at = 4u;
		goto end;
	}
	nci->cores = cores;

#ifdef _RTE_
	knci_info = nci;
#endif /* _RTE_ */

end:
	if (err_at) {
		NCI_ERROR(("nci_init: Failed err_at=%#x\n", err_at));
		nci_uninit(nci);
		nci = NULL;
	}

	return nci;
}

/**
 * Function : nci_uninit
 * Description : Free's memory related to 'nci_info_t' and its internal malloc'd elements.
 *
 * @paramter[in]
 * @nci : This is 'nci_info_t' data structure, where all EROM parsed Cores are saved, using this
 *	all EROM Cores are Freed.
 *
 * Return : void
 */
void
BCMATTACHFN(nci_uninit)(void *ctx)
{
	nci_info_t *nci = (nci_info_t *)ctx;
	uint8 core_idx, desc_idx;
	interface_desc_t *desc;
	nci_cores_t *cores;
	slave_port_t *sp;

	if (nci == NULL) {
		return;
	}

	nci->refcnt--;

#ifdef _RTE_
	if (nci->refcnt != 0) {
		return;
	}
#endif /* _RTE_ */

	cores = nci->cores;
	if (cores == NULL) {
		goto end;
	}

	for (core_idx = 0u; core_idx < nci->num_cores; core_idx++) {
		desc = cores[core_idx].desc;
		if (desc == NULL) {
			continue;
		}

		for (desc_idx = 0u; desc_idx < cores[core_idx].iface_cnt; desc_idx++) {
			sp = desc[desc_idx].sp;
			if (sp) {
				MFREE(nci->osh, sp, (sizeof(*sp) * desc[desc_idx].num_addr_reg));
			}
		}
		MFREE(nci->osh, desc, (sizeof(*desc) * cores[core_idx].iface_cnt));
	}
	MFREE(nci->osh, cores, sizeof(*cores) * nci->max_cores);

end:

#ifdef _RTE_
	knci_info = NULL;
#endif /* _RTE_ */

	MFREE(nci->osh, nci, sizeof(*nci));
}

/**
 * Function : nci_save_iface1_reg
 * Description : Interface1 Descriptor is obtained from the Reg and saved in
 * Internal data structures 'nci->cores'.
 *
 * @paramter[in]
 * @desc : Descriptor of Core which needs to be updated with obatained Interface1 Descritpor.
 * @iface_desc_1 : Obatained Interface1 Descritpor.
 *
 * Return : Void
 */
static void
BCMATTACHFN(nci_save_iface1_reg)(si_t *sih, interface_desc_t *desc, uint32 iface_desc_1,
	uint32 *erom2ptr)
{
	si_info_t *sii = SI_INFO(sih);
	nci_info_t *nci = sii->nci_info;
	char (*bpid_str)[BACKPLANE_ID_STR_SIZE];
	uint8 bpid, apb_start, apb_end;

	BCM_REFERENCE(sii);
	BCM_REFERENCE(BACKPLANE_ID_NAME_4397A0);
	BCM_REFERENCE(BACKPLANE_ID_NAME_AI);
	BCM_REFERENCE(BACKPLANE_ID_NAME);
	BCM_REFERENCE(bpid_str);

	/*
	 * From EROM Interface_Desc_1->coretype == 0:OOBR 1:NON-OOBR.
	 * For coding convinence we will save this in negated form
	 * i.e. 1:OOBR 0:NON-OOBR
	 */
	desc->oobr_core = !GET_CORETYPE(iface_desc_1);

	desc->master = IS_MASTER(iface_desc_1);

	desc->iface_desc_1 = iface_desc_1;
	desc->num_addr_reg = GET_NUM_ADDR_REG(iface_desc_1);
	if (desc->master) {
		if (desc->num_addr_reg) {
			NCI_ERROR(("nci_save_iface1_reg: Master NODEPTR Addresses is not zero "
				"i.e. %d\n", GET_NUM_ADDR_REG(iface_desc_1)));
			ASSERT(0u);
		}
	} else {
		/* SLAVE 'NumAddressRegion' one less than actual slave ports, so increment by 1
		 * if WORDOFFSET bigger then 1.
		 * In the last slave descriptor, WORDOFFSET is 0 and NUM_ADDR_REG is 0,
		 * increment by 1 if the next dword is not ID_ENDMARKER (0xFFFFFFFF).
		 */
		if (GET_WORDOFFSET(desc->iface_desc_1) > 1u) {
			desc->num_addr_reg++;
		} else if (GET_WORDOFFSET(desc->iface_desc_1) == 0) {
			uint32 adesc = R_REG(nci->osh, erom2ptr);
			if (adesc != ID_ENDMARKER) {
				desc->num_addr_reg++;
			}
		}
	}

	bpid = ID_BPID(desc->iface_desc_1);
	desc->is_booker = 0u;
	desc->is_nic400 = 0u;
	desc->is_pmni = 0u;
	desc->is_axi = 0u;

	/* 4397A0 has both BOOKER & NIC400 Backplane */
	if (BCM4397_CHIP(CHIPID(sii->pub.chip)) && (sih->chiprev == 0u)) {
		apb_start = BP_APB1;
		apb_end = BP_APB2;

		if (bpid == BP_BOOKER) {
			desc->is_booker = 1u;
			desc->is_axi = 1u;
		} else if (bpid == BP_NIC400) {
			desc->is_nic400 = 1u;
			desc->is_axi = 1u;
		}

		bpid_str = BACKPLANE_ID_NAME_4397A0;
	} else if (
		BCM4384_CHIP(CHIPID(sii->pub.chip))) {
		apb_start = BP_APB_WL;
		apb_end = BP_CCI400;

		if (bpid == BP_NIC400_CB || bpid == BP_NIC400_WL) {
			desc->is_nic400 = 1u;
			desc->is_axi = 1u;
		}
		bpid_str = BACKPLANE_ID_NAME_AI;
	} else {
		apb_start = BP_WL_PMNI;
		apb_end = BP_SAQM_PMNI;

		if (bpid == BP_BOOKER || bpid == BP_BKR_CCI400) {
			desc->is_axi = 1u;
			desc->is_booker = 1u;
		}

		bpid_str = BACKPLANE_ID_NAME;
	}

	if (!desc->master && ((bpid >= apb_start) && (bpid <= apb_end))) {
		desc->is_pmni = 1u;
	}

	NCI_INFO(("\tnci_save_iface1_reg: %s InterfaceDesc:%#x WordOffset=%#x "
		"NoAddrReg=%#x %s_Offset=%#x BackplaneID=%s\n",
		desc->master?"Master":"Slave", desc->iface_desc_1,
		GET_WORDOFFSET(desc->iface_desc_1),
		desc->num_addr_reg, desc->oobr_core?"OOBR":"EROM1",
		GET_COREOFFSET(desc->iface_desc_1),
		bpid_str[bpid]));
}

/**
 * Function : nci_save_slaveport_addr
 * Description : All Slave Port Addr of Interface Descriptor are saved.
 *
 * @paramter[in]
 * @nci : This is 'nci_info_t' data structure, where all EROM parsed Cores are saved
 * @desc : Current Interface Descriptor.
 * @erom2ptr : Pointer to Address Descriptor0.
 *
 * Return : On Success, this function returns Erom2 Ptr to Next Interface Descriptor,
 *	On Failure, NULL is returned.
 */
static uint32*
BCMATTACHFN(nci_save_slaveport_addr)(nci_info_t *nci,
		interface_desc_t *desc, uint32 *erom2ptr)
{
	slave_port_t *sp;
	uint32 adesc;
	uint32 sz;
	uint32 addr_idx;

	/* Allocate 'NumAddressRegion' of Slave Port */
	desc->sp = (slave_port_t *)MALLOCZ(nci->osh,
			(sizeof(*sp) * desc->num_addr_reg));
	if (desc->sp == NULL) {
		NCI_ERROR(("\tnci_save_slaveport_addr: Memory Allocation failed for Slave Port\n"));
		return NULL;
	}

	sp = desc->sp;
	/* Slave Port Addrs Desc */
	for (addr_idx = 0u; addr_idx < desc->num_addr_reg; addr_idx++) {
		adesc = R_REG(nci->osh, erom2ptr);
		NCI_INC_ADDR(erom2ptr, NCI_WORD_SIZE);
		sp[addr_idx].adesc = adesc;

		sp[addr_idx].addrl = adesc & SLAVEPORT_BASE_ADDR_MASK;
		if (adesc & SLAVEPORT_64BIT_ADDR_MASK) {
			sp[addr_idx].addrh = R_REG(nci->osh, erom2ptr);
			NCI_INC_ADDR(erom2ptr, NCI_WORD_SIZE);
			sp[addr_idx].extaddrl = R_REG(nci->osh, erom2ptr);
			NCI_INC_ADDR(erom2ptr, NCI_WORD_SIZE);
			sp[addr_idx].extaddrh = R_REG(nci->osh, erom2ptr);
			NCI_INC_ADDR(erom2ptr, NCI_WORD_SIZE);
			NCI_TRACE(("\tnci_save_slaveport_addr: SlavePortAddr[%#x]:0x%08x al=0x%08x "
				"ah=0x%08x extal=0x%08x extah=0x%08x\n", addr_idx, adesc,
				sp[addr_idx].addrl, sp[addr_idx].addrh, sp[addr_idx].extaddrl,
				sp[addr_idx].extaddrh));
			}
		else if (adesc & SLAVEPORT_BOUND_ADDR_MASK) {
			sp[addr_idx].addrh = R_REG(nci->osh, erom2ptr);
			NCI_INC_ADDR(erom2ptr, NCI_WORD_SIZE);
			NCI_TRACE(("\tnci_save_slaveport_addr: SlavePortAddr[%#x]:0x%08x al=0x%08x "
				"ah=0x%08x\n", addr_idx, adesc, sp[addr_idx].addrl,
				sp[addr_idx].addrh));
		} else {
			sz = adesc & SLAVEPORT_ADDR_SIZE_MASK;
			sp[addr_idx].addrh = sp[addr_idx].addrl + ADDR_SIZE(sz);
			NCI_TRACE(("\tnci_save_slaveport_addr: SlavePortAddr[%#x]:0x%08x al=0x%08x "
				"ah=0x%08x sz=0x%08x\n", addr_idx, adesc, sp[addr_idx].addrl,
				sp[addr_idx].addrh, sz));
		}
	}

	return erom2ptr;
}

static void
BCMATTACHFN(nci_check_extra_core)(nci_info_t *nci, uint32 *erom2ptr, uint32 *core_idx)
{
	uint32 iface_desc_1;
	nci_cores_t *core;
	uint32 dmp_regs_off = 0u;
	uint32 iface_cfg = 0u;
	uint32 core_info;
	uint32 *ptr;
	uint32 *orig_ptr;
	uint coreid;
	uint32 offset;

	iface_desc_1 = R_REG(nci->osh, erom2ptr);

	/* Get EROM1/OOBR Pointer based on CoreType */
	if (!GET_CORETYPE(iface_desc_1)) {
		if (nci->bustype == PCI_BUS) {
			OSL_PCI_WRITE_CONFIG(nci->osh, PCI_BAR0_WIN, PCI_ACCESS_SIZE,
				GET_OOBR_BASE(nci->cc_erom2base));
			nci->oobr_base = (uint32 *)((uintptr)nci->erom1base);
		}

		offset = GET_COREOFFSET(iface_desc_1);
		ptr = NCI_ADD_ADDR(nci->oobr_base, offset);
		if (nci->exp_next_oobr_offset == offset) {
			nci->exp_next_oobr_offset += OOBR_CORE_SIZE;
			goto done;
		} else {
			NCI_TRACE(("Parsing core w/o EROM entry off:0x%x exp_off:0x%x\n",
				offset, nci->exp_next_oobr_offset));
			orig_ptr = ptr;
			ptr = NCI_SUB_ADDR(ptr, (offset - nci->exp_next_oobr_offset));
		}
	} else {
		return;
	}

	do {
		dmp_regs_off = nci->exp_next_oobr_offset + DMP_DMPCTRL_REG_OFFSET;

		core_info = R_REG(nci->osh, ptr);
		iface_cfg = R_REG(nci->osh, (ptr + 1));

		*core_idx = nci->num_cores;
		core = &nci->cores[*core_idx];

		if (CORE_ID(core_info) < 0xFFu) {
			coreid = CORE_ID(core_info) | EROM_VENDOR_BRCM;
		} else {
			coreid = CORE_ID(core_info);
		}

		/* Get coreunit from previous cores i.e. num_cores */
		core->coreunit = 0;

		core->coreid = coreid;

		/* Increment the num_cores once proper coreunit is known */
		nci->num_cores++;

		NCI_TRACE(("nci_check_extra_core: core_idx:%d %s=%p \n",
			*core_idx, GET_CORETYPE(iface_desc_1)?"EROM1":"OOBR", ptr));

		/* Core Info Register */
		core->coreinfo = core_info;

		/* Save DMP register base address. */
		core->dmp_regs_off = dmp_regs_off;

		NCI_TRACE(("nci_check_extra_core: COREINFO:%#x CId:%#x CUnit=%#x CRev=%#x"
			" CMfg=%#x\n", core->coreinfo, core->coreid, core->coreunit,
			CORE_REV(core->coreinfo), CORE_MFG(core->coreinfo)));

		/* Interface Config Register */
		core->iface_cfg = iface_cfg;
		core->iface_cnt = GET_INFACECNT(iface_cfg);
		core->desc = NULL;

		NCI_TRACE(("nci_check_extra_core: IFACE_CFG:%#x IfaceCnt=%#x IfaceOffset=%#x \n",
			iface_cfg, core->iface_cnt, iface_cfg & IC_IFACEOFFSET_MASK));

		/* Update pointer and expected oobr offset should point to next core. */
		NCI_INC_ADDR(ptr, OOBR_CORE_SIZE);
		nci->exp_next_oobr_offset += OOBR_CORE_SIZE;
	} while (ptr < orig_ptr);

done:
	/* For PCI_BUS case set back BAR0 Window to EROM1 Base */
	if (nci->bustype == PCI_BUS) {
		OSL_PCI_WRITE_CONFIG(nci->osh, PCI_BAR0_WIN, PCI_ACCESS_SIZE,
			GET_EROM1_BASE(nci->cc_erom2base));
	}

	return;
}

/**
 * Function : nci_initial_parse
 * Description : This function does
 *	1. Obtains OOBR/EROM1 pointer based on CoreType
 *	2. Analysis right CoreUnit for this 'core'
 *	3. Saves CoreInfo & Interface Config in Coresponding 'core'
 *
 * @paramter[in]
 * @nci : This is 'nci_info_t' data structure, where all EROM parsed Cores are saved.
 * @erom2ptr : Pointer to Interface Descriptor0.
 * @core_idx : New core index needs to be populated in this pointer.
 *
 * Return : On Success, this function returns 'core' where CoreInfo & Interface Config are saved.
 */
static nci_cores_t*
BCMATTACHFN(nci_initial_parse)(nci_info_t *nci, uint32 *erom2ptr, uint32 *core_idx)
{
	uint32 iface_desc_1;
	nci_cores_t *core;
	uint32 dmp_regs_off = 0u;
	uint32 iface_cfg = 0u;
	uint32 core_info;
	uint32 *ptr;
	uint coreid;

	iface_desc_1 = R_REG(nci->osh, erom2ptr);

	/* Get EROM1/OOBR Pointer based on CoreType */
	if (!GET_CORETYPE(iface_desc_1)) {
		if (nci->bustype == PCI_BUS) {
			OSL_PCI_WRITE_CONFIG(nci->osh, PCI_BAR0_WIN, PCI_ACCESS_SIZE,
				GET_OOBR_BASE(nci->cc_erom2base));
			nci->oobr_base = (uint32 *)((uintptr)nci->erom1base);
		}

		ptr = NCI_ADD_ADDR(nci->oobr_base, GET_COREOFFSET(iface_desc_1));
	} else {
		ptr = NCI_ADD_ADDR(nci->erom1base, GET_COREOFFSET(iface_desc_1));
	}
	dmp_regs_off = GET_COREOFFSET(iface_desc_1) + DMP_DMPCTRL_REG_OFFSET;

	core_info = R_REG(nci->osh, ptr);
	NCI_INC_ADDR(ptr, NCI_WORD_SIZE);
	iface_cfg = R_REG(nci->osh, ptr);

	*core_idx = nci->num_cores;
	core = &nci->cores[*core_idx];

	if (CORE_ID(core_info) < 0xFFu) {
		coreid = CORE_ID(core_info) | EROM_VENDOR_BRCM;
	} else {
		coreid = CORE_ID(core_info);
	}

	/* Get coreunit from previous cores i.e. num_cores */
	core->coreunit = nci_get_coreunit(nci->cores, nci->num_cores,
		coreid, iface_desc_1);

	core->coreid = coreid;

	/* Increment the num_cores once proper coreunit is known */
	nci->num_cores++;

	NCI_TRACE(("\n\nnci_initial_parse: core_idx:%d %s=%p \n",
		*core_idx, GET_CORETYPE(iface_desc_1)?"EROM1":"OOBR", ptr));

	/* Core Info Register */
	core->coreinfo = core_info;

	/* Save DMP register base address. */
	core->dmp_regs_off = dmp_regs_off;

	NCI_INFO(("\tnci_initial_parse: COREINFO:%#x CId:%#x CUnit=%#x CRev=%#x CMfg=%#x\n",
		core->coreinfo, core->coreid, core->coreunit, CORE_REV(core->coreinfo),
		CORE_MFG(core->coreinfo)));

	/* Interface Config Register */
	core->iface_cfg = iface_cfg;
	core->iface_cnt = GET_INFACECNT(iface_cfg);

	NCI_TRACE(("\tnci_initial_parse: INTERFACE_CFG:%#x IfaceCnt=%#x IfaceOffset=%#x \n",
		iface_cfg, core->iface_cnt, iface_cfg & IC_IFACEOFFSET_MASK));

	/* For PCI_BUS case set back BAR0 Window to EROM1 Base */
	if (nci->bustype == PCI_BUS) {
		OSL_PCI_WRITE_CONFIG(nci->osh, PCI_BAR0_WIN, PCI_ACCESS_SIZE,
			GET_EROM1_BASE(nci->cc_erom2base));
	}

	return core;
}

static uint32
BCMATTACHFN(nci_find_numcores)(si_t *sih)
{
	const si_info_t *sii = SI_INFO(sih);
	nci_info_t *nci = sii->nci_info;
	volatile hndoobr_reg_t *oobr_reg = NULL;
	uint32 orig_bar0_win1 = 0u;
	uint32 num_oobr_cores = 0u;
	uint32 num_nonoobr_cores = 0u;

	/* No of Non-OOBR Cores */
	num_nonoobr_cores = NCI_NONOOBR_CORES(nci->cc_erom2base);
	if (num_nonoobr_cores <= 0u) {
		NCI_ERROR(("nci_find_numcores: Invalid Number of non-OOBR cores %d\n",
			num_nonoobr_cores));
		goto fail;
	}

	/* No of OOBR Cores */
	switch (BUSTYPE(sih->bustype)) {
	case SI_BUS:
		oobr_reg = (volatile hndoobr_reg_t *)REG_MAP(GET_OOBR_BASE(nci->cc_erom2base),
				SI_CORE_SIZE);
		break;

	case PCI_BUS:
		/* Save Original Bar0 Win1 */
		orig_bar0_win1 = OSL_PCI_READ_CONFIG(nci->osh, PCI_BAR0_WIN,
			PCI_ACCESS_SIZE);

		OSL_PCI_WRITE_CONFIG(nci->osh, PCI_BAR0_WIN, PCI_ACCESS_SIZE,
			GET_OOBR_BASE(nci->cc_erom2base));
		oobr_reg = (volatile hndoobr_reg_t *)sii->curmap;
		break;

#ifdef BCMSDIO
	case SPI_BUS:
	case SDIO_BUS:
		oobr_reg = (volatile hndoobr_reg_t *)nci->oobr_base;
		break;
#endif  /* BCMSDIO */

	default:
		NCI_ERROR(("nci_find_numcores: Invalid bustype %d\n", BUSTYPE(sih->bustype)));
		ASSERT(0);
		goto fail;
	}

	num_oobr_cores = R_REG(nci->osh, &oobr_reg->capability) & OOBR_CAP_CORECNT_MASK;
	if (num_oobr_cores <= 0u) {
		NCI_ERROR(("nci_find_numcores: Invalid Number of OOBR cores %d\n", num_oobr_cores));
		goto fail;
	}

	/* Point back to original base */
	if (BUSTYPE(sih->bustype) == PCI_BUS) {
		OSL_PCI_WRITE_CONFIG(nci->osh, PCI_BAR0_WIN, PCI_ACCESS_SIZE, orig_bar0_win1);
	}

	NCI_PRINT(("nci_find_numcores: Total Cores found %d\n",
		(num_oobr_cores + num_nonoobr_cores)));
	/* Total No of Cores */
	return (num_oobr_cores + num_nonoobr_cores);

fail:
	return 0u;
}

#define NCI_MAX_APB_COUNT 10u

static void
BCMATTACHFN(nci_update_shared_pmni_iface)(nci_info_t *nci)
{
	nci_cores_t *core;
	int iface_idx, coreidx;
	uint32 node_ptr_list[NCI_MAX_APB_COUNT];
	uint8 node_ptr_shared[NCI_MAX_APB_COUNT];
	uint node_idx = 0;

	bzero(node_ptr_list, sizeof(uint32) * NCI_MAX_APB_COUNT);
	bzero(node_ptr_shared, sizeof(uint8) * NCI_MAX_APB_COUNT);

	for (coreidx = 0; coreidx < nci->max_cores; coreidx++) {
		core = &nci->cores[coreidx];

		for (iface_idx = core->iface_cnt-1; iface_idx >= 0; iface_idx--) {
			if (core->desc[iface_idx].is_pmni && core->desc[iface_idx].node_ptr) {
				uint k;
				for (k = 0; k < node_idx; k++) {
					if (node_ptr_list[k] == core->desc[iface_idx].node_ptr) {
						core->desc[iface_idx].is_shared_pmni = 1u;
						node_ptr_shared[k] = 1u;
						break;
					}
				}
				/* This node_ptr is not present in the local list, add it. */
				if (k == node_idx) {
					node_ptr_list[node_idx++] = core->desc[iface_idx].node_ptr;
				}

				if (node_idx >= NCI_MAX_APB_COUNT) {
					OSL_SYS_HALT();
				}
			}
		}
	}

	for (coreidx = 0; coreidx < nci->max_cores; coreidx++) {
		core = &nci->cores[coreidx];

		for (iface_idx = core->iface_cnt-1; iface_idx >= 0; iface_idx--) {
			if (core->desc[iface_idx].is_pmni &&
					!core->desc[iface_idx].is_shared_pmni) {
				uint k;
				for (k = 0; k < node_idx; k++) {
					if (node_ptr_list[k] ==
							core->desc[iface_idx].node_ptr) {
						if (node_ptr_shared[k]) {
							core->desc[iface_idx].is_shared_pmni = 1u;
							break;
						}
					}
				}
			}
		}
	}

}

/**
 * Function : nci_scan
 * Description : Function parses EROM in BOOKER NCI Architecture and saves all inforamtion about
 *	Cores in 'nci_info_t' data structure.
 *
 * @paramter[in]
 * @nci : This is 'nci_info_t' data structure, where all EROM parsed Cores are saved.
 *
 * Return : On Success No of parsed Cores in EROM is returned,
 *	On Failure '0' is returned by printing ERROR messages
 *	in Console(If NCI_LOG_LEVEL is enabled).
 */
uint32
BCMATTACHFN(nci_scan)(si_t *sih)
{
	si_info_t *sii = SI_INFO(sih);
	nci_info_t *nci = (nci_info_t *)sii->nci_info;
	uint32 *cur_iface_desc_1_ptr;
	nci_cores_t *core;
	interface_desc_t *desc;
	uint32 wordoffset = 0u;
	uint32 iface_desc_0;
	uint32 iface_desc_1;
	uint32 *erom2ptr;
	uint8 iface_idx;
	uint32 core_idx;
	int err = 0;

	/* If scan was finished already */
	if (nci->scan_done) {
		goto end;
	}

	erom2ptr = nci->erom2base;
	sii->axi_num_wrappers = 0;
	nci->exp_next_oobr_offset = OOBR_CORE0_OFFSET + OOBR_COREINFO_OFFSET;

	while (TRUE) {
		iface_desc_0 = R_REG(nci->osh, erom2ptr);
		if (iface_desc_0 == ID_ENDMARKER) {
			NCI_INFO(("\nnci_scan: Reached end of EROM2 with total cores=%d \n",
				nci->num_cores));
			break;
		}

		/* Save current Iface1 Addr for comparision */
		cur_iface_desc_1_ptr = GET_NEXT_EROM_ADDR(erom2ptr);

		nci_check_extra_core(nci, cur_iface_desc_1_ptr, &core_idx);
		/* Get CoreInfo, InterfaceCfg, CoreIdx */
		core = nci_initial_parse(nci, cur_iface_desc_1_ptr, &core_idx);

		core->desc = (interface_desc_t *)MALLOCZ(
			nci->osh, (sizeof(*(core->desc)) * core->iface_cnt));
		if (core->desc == NULL) {
			NCI_ERROR(("nci_scan: Mem Alloc failed for Iface and Addr "
				"Descriptor\n"));
			err = NCI_NOMEM;
			break;
		}

		for (iface_idx = 0u; iface_idx < core->iface_cnt; iface_idx++) {
			desc = &core->desc[iface_idx];

			iface_desc_0 = R_REG(nci->osh, erom2ptr);
			NCI_INC_ADDR(erom2ptr, NCI_WORD_SIZE);
			iface_desc_1 = R_REG(nci->osh, erom2ptr);
			NCI_INC_ADDR(erom2ptr, NCI_WORD_SIZE);

			/* Interface Descriptor Register */
			nci_save_iface1_reg(sih, desc, iface_desc_1, erom2ptr);
			if (desc->master && desc->num_addr_reg) {
				err = NCI_MASTER_INVALID_ADDR;
				goto end;
			}

			wordoffset = GET_WORDOFFSET(iface_desc_1);

			/* NodePointer Register */
			desc->iface_desc_0 = iface_desc_0;
			desc->node_ptr = GET_NODEPTR(iface_desc_0);
			desc->node_type = GET_NODETYPE(iface_desc_0);

			NCI_INFO(("nci_scan: %s NodePointer:%#x Type=%s NODEPTR=%#x \n",
				desc->master?"Master":"Slave", desc->iface_desc_0,
				desc->node_type?"NIC-400":"BOOKER", desc->node_ptr));

			/* Slave Port Addresses */
			if (!desc->master && desc->num_addr_reg) {
				erom2ptr = nci_save_slaveport_addr(nci, desc, erom2ptr);
				if (erom2ptr == NULL) {
					NCI_ERROR(("nci_scan: Invalid EROM2PTR\n"));
					err = NCI_INVALID_EROM2PTR;
					goto end;
				}
			}

			/* Current loop ends with next iface_desc_0 */
		}

		if (wordoffset == 0u) {
			NCI_INFO(("\nnci_scan: EROM PARSING found END 'wordoffset=%#x' "
				"with total cores=%d \n", wordoffset, nci->num_cores));
			break;
		}
	}

	nci_update_shared_pmni_iface(nci);
	nci_update_domain_id(sih);
	nci->scan_done = TRUE;

end:
	if (err) {
		NCI_ERROR(("nci_scan: Failed with Code %d\n", err));
		nci->num_cores = 0;
		ASSERT(0u);
	}

	return nci->num_cores;
}

struct core2domain_t {
	uint16 coreid;
	uint8  coreunit;
	uint8  domain;		/* Domain id */
};
static struct core2domain_t BCMATTACHDATA(core2domain[]) = {
	{PCIE2_CORE_ID,		0u, SRPWR_DMN0_PCIE},
	{ARMCA7_CORE_ID,	0u, SRPWR_DMN1_ARMBPSD},
	{D11_CORE_ID,		1u, SRPWR_DMN2_MACAUX},
	{D11_CORE_ID,		0u, SRPWR_DMN3_MACMAIN},
	{D11_CORE_ID,		2u, SRPWR_DMN4_MACSCAN},
	{D11_SAQM_CORE_ID,	0u, SRPWR_DMN6_SAQM}
};

static void
BCMATTACHFN(nci_update_domain_id)(si_t *sih)
{
	si_info_t *sii = SI_INFO(sih);
	nci_info_t *nci = (nci_info_t *)sii->nci_info;
	nci_cores_t *core_info;
	uint i, j;

	for (i = 0; i < nci->num_cores; i++) {
		core_info = &nci->cores[i];
		/* If not listed in the core2domain table, then core is in AAON domain */
		core_info->domain = DOMAIN_AAON;

		for (j = 0; j < ARRAYSIZE(core2domain); j++) {
			if (core_info->coreid == core2domain[j].coreid &&
					core_info->coreunit == core2domain[j].coreunit) {
				core_info->domain = core2domain[j].domain;
				break;
			}
		}
	}

}

/**
 * Function : nci_dump_erom
 * Description : Function dumps EROM from inforamtion cores in 'nci_info_t' data structure.
 *
 * @paramter[in]
 * @nci : This is 'nci_info_t' data structure, where all EROM parsed Cores are saved.
 *
 * Return : void
 */
void
BCMATTACHFN(nci_dump_erom)(void *ctx)
{
	nci_info_t *nci = (nci_info_t *)ctx;
	nci_cores_t *core;
	interface_desc_t *desc;
	slave_port_t *sp;
	uint32 core_idx, addr_idx, iface_idx;
	uint32 core_info;

	BCM_REFERENCE(core_info);

	NCI_INFO(("\nnci_dump_erom: -- EROM Dump --\n"));
	for (core_idx = 0u; core_idx < nci->num_cores; core_idx++) {
		core = &nci->cores[core_idx];

		/* Core Info Register */
		core_info = core->coreinfo;
		NCI_INFO(("\nnci_dump_erom: core_idx=%d COREINFO:%#x CId:%#x CUnit:%#x CRev=%#x "
			"CMfg=%#x\n", core_idx, core_info, CORE_ID(core_info), core->coreunit,
			CORE_REV(core_info), CORE_MFG(core_info)));

		/* Interface Config Register */
		NCI_INFO(("nci_dump_erom: IfaceCfg=%#x IfaceCnt=%#x \n",
			core->iface_cfg, core->iface_cnt));

		for (iface_idx = 0u; iface_idx < core->iface_cnt; iface_idx++) {
			desc = &core->desc[iface_idx];
			/* NodePointer Register */
			NCI_INFO(("nci_dump_erom: %s iface_desc_0 Master=%#x MASTER_WRAP=%#x "
				"Type=%s \n", desc->master?"Master":"Slave", desc->iface_desc_0,
				desc->node_ptr,
				(desc->node_type)?"NIC-400":"BOOKER"));

			/* Interface Descriptor Register */
			NCI_INFO(("nci_dump_erom: %s InterfaceDesc:%#x WOffset=%#x NoAddrReg=%#x "
				"%s_Offset=%#x\n", desc->master?"Master":"Slave",
				desc->iface_desc_1, GET_WORDOFFSET(desc->iface_desc_1),
				desc->num_addr_reg, desc->oobr_core?"OOBR":"EROM1",
				GET_COREOFFSET(desc->iface_desc_1)));

			/* Slave Port Addresses */
			sp = desc->sp;
			if (!sp) {
				continue;
			}
			for (addr_idx = 0u; addr_idx < desc->num_addr_reg; addr_idx++) {
				if (sp[addr_idx].extaddrl) {
					NCI_INFO(("nci_dump_erom: SlavePortAddr[%#x]: AddrDesc=%#x"
						" al=%#x ah=%#x  extal=%#x extah=%#x\n", addr_idx,
						sp[addr_idx].adesc, sp[addr_idx].addrl,
						sp[addr_idx].addrh, sp[addr_idx].extaddrl,
						sp[addr_idx].extaddrh));
				} else {
					NCI_INFO(("nci_dump_erom: SlavePortAddr[%#x]: AddrDesc=%#x"
						" al=%#x ah=%#x\n", addr_idx, sp[addr_idx].adesc,
						sp[addr_idx].addrl, sp[addr_idx].addrh));
				}
			}
		}
	}

	return;
}

/**
 * Function : nci_cores_to_ai_cores
 * Description : Function converts nci->cores to sii->cores_info data structures.
 * Used in AI Chips only.
 *
 * @paramter[in]
 * @nci : This is 'nci_info_t' data structure, where all EROM parsed Cores are saved.
 *
 * Return : void
 */
void
BCMATTACHFN(nci_cores_to_ai_cores)(si_t *sih)
{
	si_info_t *sii = SI_INFO(sih);
	si_cores_info_t *cores_info = (si_cores_info_t *)sii->cores_info;
	axi_wrapper_t *axi_wrapper = sii->axi_wrapper;
	nci_info_t *nci = (nci_info_t *)sii->nci_info;
	nci_cores_t *nci_core;
	uint32 core_idx;

	SI_MSG_DBG_REG(("%s: Enter\n", __FUNCTION__));

	sii->axi_num_wrappers = 0;

	for (core_idx = 0u; core_idx < nci->num_cores; core_idx++) {
		interface_desc_t *desc;
		slave_port_t *sp;
		uint32 addr_idx, iface_idx, ai_idx;
		uint32 cid, mfg, crev, cib;
		bool is_core = FALSE;
		uint8 m_idx = 0, s_idx = 0;
		nci_core = &nci->cores[core_idx];

		cid = nci_core->coreid;
		mfg = CORE_MFG(nci_core->coreinfo);
		crev = CORE_REV(nci_core->coreinfo);

		ai_idx = sii->numcores;

		/* Find if componnet is a core */
		for (iface_idx = 0u; iface_idx < nci_core->iface_cnt; iface_idx++) {
			desc = &nci_core->desc[iface_idx];
			is_core = is_core || desc->is_axi;
		}

		SI_VMSG(("Found component 0x%04x/0x%04x rev %d\n", mfg, cid, crev));

		/* A component which is not a core */
		if (is_core == FALSE) {
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

		/* revision from cib used in ai functions */
		cib = (crev << CIB_REV_SHIFT) & CIB_REV_MASK;

		//cores_info->cia[ai_idx] = cia;
		cores_info->cib[ai_idx] = cib;
		cores_info->coreid[ai_idx] = cid;

		if (BUSTYPE(sih->bustype) == PCI_BUS &&
			(cid == PCI_CORE_ID || cid == PCIE_CORE_ID || cid == PCIE2_CORE_ID)) {
			sii->pub.buscoretype = (uint16)cid;
		}

		/* Master and Salve wrappers */
		for (iface_idx = 0u; iface_idx < nci_core->iface_cnt; iface_idx++) {
			desc = &nci_core->desc[iface_idx];

			if (desc->is_axi) {
				if (desc->master) {
					if (m_idx == 0) {
						cores_info->wrapba[ai_idx] = desc->node_ptr;
					} else if (m_idx == 1) {
						cores_info->wrapba2[ai_idx] = desc->node_ptr;
					} else if (m_idx == 2) {
						cores_info->wrapba3[ai_idx] = desc->node_ptr;
					}
					m_idx++;
				} else if (m_idx == 0) {
					if (s_idx == 0) {
						cores_info->wrapba[ai_idx] = desc->node_ptr;
					} else if (s_idx == 1) {
						cores_info->wrapba2[ai_idx] = desc->node_ptr;
					} else if (s_idx == 2) {
						cores_info->wrapba3[ai_idx] = desc->node_ptr;
					}
					s_idx++;
				}
				axi_wrapper[sii->axi_num_wrappers].mfg = mfg;
				axi_wrapper[sii->axi_num_wrappers].cid = cid;
				axi_wrapper[sii->axi_num_wrappers].rev = crev;
				axi_wrapper[sii->axi_num_wrappers].wrapper_type =
					(desc->master) ? AI_MASTER_WRAPPER : AI_SLAVE_WRAPPER;
				axi_wrapper[sii->axi_num_wrappers].wrapper_addr = desc->node_ptr;

				SI_VMSG(("%s WRAPPER: %d, mfg:%x, cid:%x, rev:%x, addr:%x\n",
					desc->master?"MASTER":"SLAVE", sii->axi_num_wrappers,
					mfg, cid, crev, desc->node_ptr));

				sii->axi_num_wrappers++;
			}

			/* Slave Port Addresses */
			sp = desc->sp;
			if (!sp) {
				continue;
			}
			for (addr_idx = 0u; addr_idx < desc->num_addr_reg; addr_idx++) {

				if (sp[addr_idx].extaddrl == 0u) {
					if (desc->is_axi) {
						/* First base address of second slave port */
						if (addr_idx == 0) {
							cores_info->csp2ba[ai_idx] =
								sp[addr_idx].addrl;
							cores_info->csp2ba_size[ai_idx] =
								(sp[addr_idx].addrh -
								sp[addr_idx].addrl + 1);
						}
					} else {
						/* First Slave Address Descriptor should be port 0:
						 * the main register space for the core
						 */
						if (addr_idx == 0) {
							cores_info->coresba[ai_idx] =
								sp[addr_idx].addrl;
							cores_info->coresba_size[ai_idx] =
								(sp[addr_idx].addrh -
								sp[addr_idx].addrl + 1);
						} else if (addr_idx == 1) {
							cores_info->coresba2[ai_idx] =
								sp[addr_idx].addrl;
							cores_info->coresba2_size[ai_idx] =
								(sp[addr_idx].addrh -
								sp[addr_idx].addrl + 1);
						}
					}
				}
			}
		}
		sii->numcores++;
	}
}

/*
 * Switch to 'coreidx', issue a single arbitrary 32bit register mask & set operation,
 * switch back to the original core, and return the new value.
 */
uint
BCMPOSTTRAPFN(nci_corereg)(const si_t *sih, uint coreidx, uint regoff, uint mask, uint val)
{
	uint origidx = 0;
	volatile uint32 *r = NULL;
	uint w;
	bcm_int_bitmask_t intr_val;
	bool fast = FALSE;
	si_info_t *sii = SI_INFO(sih);
	nci_info_t *nci = sii->nci_info;
	nci_cores_t *cores_info = &nci->cores[coreidx];

	NCI_TRACE(("nci_corereg coreidx %u coreid 0x%x regoff 0x%x mask 0x%x val 0x%x\n",
		coreidx, cores_info->coreid, regoff, mask, val));

	ASSERT(GOODIDX(coreidx, nci->num_cores));
	ASSERT((val & ~mask) == 0);

	if (coreidx >= SI_MAXCORES) {
		return 0;
	}

	if (BUSTYPE(sih->bustype) == SI_BUS) {
		/* If internal bus, we can always get at everything */
		uint32 curmap = nci_get_curmap(nci, coreidx);
		BCM_REFERENCE(curmap);

		fast = TRUE;
		/* map if does not exist */
		if (!cores_info->regs) {
			cores_info->regs = REG_MAP(curmap, SI_CORE_SIZE);
			ASSERT(GOODREGS(cores_info->regs));
		}
		r = (volatile uint32 *)((volatile uchar *)cores_info->regs + regoff);
	} else if (BUSTYPE(sih->bustype) == PCI_BUS) {
		/* If pci/pcie, we can get at pci/pcie regs and on newer cores to chipc */

		ASSERT(regoff < SI_CORE_SIZE);

		if ((cores_info->coreid == CC_CORE_ID) && SI_FAST(sii)) {
			/* Chipc registers are mapped at 12KB */

			fast = TRUE;
			r = (volatile uint32 *)((volatile char *)sii->curmap +
				PCI_16KB0_CCREGS_OFFSET + regoff);
		} else if (sii->pub.buscoreidx == coreidx) {
			/* pci registers are at either in the last 2KB of an 8KB window
			 * or, in pcie and pci rev 13 at 8KB
			 */
			fast = TRUE;
			if (SI_FAST(sii)) {
				r = (volatile uint32 *)((volatile char *)sii->curmap +
					PCI_16KB0_PCIREGS_OFFSET + regoff);
			} else {
				r = (volatile uint32 *)((volatile char *)sii->curmap +
					((regoff >= SBCONFIGOFF) ?
					PCI_BAR0_PCISBR_OFFSET : PCI_BAR0_PCIREGS_OFFSET) + regoff);
			}
		}
	}

	if (!fast) {
		INTR_OFF(sii, &intr_val);

		/* save current core index */
		origidx = si_coreidx(&sii->pub);

		/* switch core */
		r = (volatile uint32 *)((volatile uchar *)nci_setcoreidx(&sii->pub, coreidx) +
			regoff);
	}
	ASSERT(r != NULL);

	/* mask and set */
	if (mask || val) {
		w = (R_REG(sii->osh, r) & ~mask) | val;
		W_REG(sii->osh, r, w);
	}

	/* readback */
	w = R_REG(sii->osh, r);

	if (!fast) {
		/* restore core index */
		if (origidx != coreidx) {
			nci_setcoreidx(&sii->pub, origidx);
		}
		INTR_RESTORE(sii, &intr_val);
	}

	return (w);
}

uint
nci_corereg_writeonly(si_t *sih, uint coreidx, uint regoff, uint mask, uint val)
{
	uint origidx = 0;
	volatile uint32 *r = NULL;
	uint w = 0;
	bcm_int_bitmask_t intr_val;
	bool fast = FALSE;
	si_info_t *sii = SI_INFO(sih);
	nci_info_t *nci = sii->nci_info;
	nci_cores_t *cores_info = &nci->cores[coreidx];

	NCI_INFO(("nci_corereg_writeonly() coreidx %u regoff %u mask %u val %u\n",
		coreidx, regoff, mask, val));

	ASSERT(GOODIDX(coreidx, nci->num_cores));
	ASSERT(regoff < SI_CORE_SIZE);
	ASSERT((val & ~mask) == 0);

	if (coreidx >= SI_MAXCORES) {
		return 0;
	}

	if (BUSTYPE(sih->bustype) == SI_BUS) {
		/* If internal bus, we can always get at everything */
		uint32 curmap = nci_get_curmap(nci, coreidx);
		BCM_REFERENCE(curmap);
		fast = TRUE;
		/* map if does not exist */
		if (!cores_info->regs) {
			cores_info->regs = REG_MAP(curmap, SI_CORE_SIZE);
			ASSERT(GOODREGS(cores_info->regs));
		}
		r = (volatile uint32 *)((volatile uchar *)cores_info->regs + regoff);
	} else if (BUSTYPE(sih->bustype) == PCI_BUS) {
		/* If pci/pcie, we can get at pci/pcie regs and on newer cores to chipc */

		if ((cores_info->coreid == CC_CORE_ID) && SI_FAST(sii)) {
			/* Chipc registers are mapped at 12KB */

			fast = TRUE;
			r = (volatile uint32 *)((volatile char *)sii->curmap +
				PCI_16KB0_CCREGS_OFFSET + regoff);
		} else if (sii->pub.buscoreidx == coreidx) {
			/* pci registers are at either in the last 2KB of an 8KB window
			 * or, in pcie and pci rev 13 at 8KB
			 */
			fast = TRUE;
			if (SI_FAST(sii)) {
				r = (volatile uint32 *)((volatile char *)sii->curmap +
					PCI_16KB0_PCIREGS_OFFSET + regoff);
			} else {
				r = (volatile uint32 *)((volatile char *)sii->curmap +
					((regoff >= SBCONFIGOFF) ?
					PCI_BAR0_PCISBR_OFFSET : PCI_BAR0_PCIREGS_OFFSET) + regoff);
			}
		}
	}

	if (!fast) {
		INTR_OFF(sii, &intr_val);

		/* save current core index */
		origidx = si_coreidx(&sii->pub);

		/* switch core */
		r = (volatile uint32 *) ((volatile uchar *) nci_setcoreidx(&sii->pub, coreidx) +
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
		if (origidx != coreidx) {
			nci_setcoreidx(&sii->pub, origidx);
		}

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
nci_corereg_addr(si_t *sih, uint coreidx, uint regoff)
{
	volatile uint32 *r = NULL;
	bool fast = FALSE;
	si_info_t *sii = SI_INFO(sih);
	nci_info_t *nci = sii->nci_info;
	nci_cores_t *cores_info = &nci->cores[coreidx];

	NCI_TRACE(("nci_corereg_addr() coreidx %u regoff %u\n", coreidx, regoff));

	ASSERT(GOODIDX(coreidx, nci->num_cores));
	ASSERT(regoff < SI_CORE_SIZE);

	if (coreidx >= SI_MAXCORES) {
		return 0;
	}

	if (BUSTYPE(sih->bustype) == SI_BUS) {
		uint32 curmap = nci_get_curmap(nci, coreidx);
		BCM_REFERENCE(curmap);

		/* If internal bus, we can always get at everything */
		fast = TRUE;
		/* map if does not exist */
		if (!cores_info->regs) {
			cores_info->regs = REG_MAP(curmap, SI_CORE_SIZE);
			ASSERT(GOODREGS(cores_info->regs));
		}
		r = (volatile uint32 *)((volatile uchar *)cores_info->regs + regoff);

	} else if (BUSTYPE(sih->bustype) == PCI_BUS) {
		/* If pci/pcie, we can get at pci/pcie regs and on newer cores to chipc */

		if ((cores_info->coreid == CC_CORE_ID) && SI_FAST(sii)) {
			/* Chipc registers are mapped at 12KB */

			fast = TRUE;
			r = (volatile uint32 *)((volatile char *)sii->curmap +
				PCI_16KB0_CCREGS_OFFSET + regoff);
		} else if (sii->pub.buscoreidx == coreidx) {
			/* pci registers are at either in the last 2KB of an 8KB window
			 * or, in pcie and pci rev 13 at 8KB
			 */
			fast = TRUE;
			if (SI_FAST(sii)) {
				r = (volatile uint32 *)((volatile char *)sii->curmap +
					PCI_16KB0_PCIREGS_OFFSET + regoff);
			} else {
				r = (volatile uint32 *)((volatile char *)sii->curmap +
					((regoff >= SBCONFIGOFF) ?
					PCI_BAR0_PCISBR_OFFSET : PCI_BAR0_PCIREGS_OFFSET) + regoff);
			}
		}
	}

	if (!fast) {
		ASSERT(sii->curidx == coreidx);
		r = (volatile uint32 *) ((volatile uchar *)sii->curmap + regoff);
	}

	return (r);
}

uint
BCMPOSTTRAPFN(nci_findcoreidx)(const si_t *sih, uint coreid, uint coreunit)
{
	si_info_t *sii = SI_INFO(sih);
	nci_info_t *nci = sii->nci_info;
	uint core_idx;

	NCI_TRACE(("nci_findcoreidx() coreid 0x%x coreunit %u\n", coreid, coreunit));
	for (core_idx = 0; core_idx < nci->num_cores; core_idx++) {
		if ((nci->cores[core_idx].coreid == coreid) &&
			(nci->cores[core_idx].coreunit == coreunit)) {
			return core_idx;
		}
	}
	return BADIDX;
}

static uint32
_nci_get_slave_addr_size(nci_info_t *nci, uint coreidx, uint32 slave_port_idx, uint base_idx)
{
	uint32 size;
	uint32 add_desc;

	NCI_TRACE(("_nci_get_slave_addr_size() coreidx %u slave_port_idx %u base_idx %u\n",
		coreidx, slave_port_idx, base_idx));

	add_desc = nci->cores[coreidx].desc[slave_port_idx].sp[base_idx].adesc;

	size = add_desc & SLAVEPORT_ADDR_SIZE_MASK;
	return ADDR_SIZE(size);
}

static uint32
BCMPOSTTRAPFN(_nci_get_curmap)(nci_info_t *nci, uint coreidx, uint slave_port_idx, uint base_idx)
{
	/* TODO: Is handling of 64 bit addressing required */
	NCI_TRACE(("_nci_get_curmap coreidx %u slave_port_idx %u base_idx %u\n", coreidx,
		slave_port_idx, base_idx));
	return nci->cores[coreidx].desc[slave_port_idx].sp[base_idx].addrl;
}

/* Get the interface descriptor which is connected to APB and return its address */
uint32
BCMPOSTTRAPFN(nci_get_curmap)(nci_info_t *nci, uint coreidx)
{
	nci_cores_t *core_info = &nci->cores[coreidx];
	uint32 iface_idx;

	NCI_TRACE(("nci_get_curmap coreidx %u\n", coreidx));
	for (iface_idx = 0; iface_idx < core_info->iface_cnt; iface_idx++) {

		NCI_TRACE(("nci_get_curmap iface_idx %u BP_ID %u master %u\n", iface_idx,
			ID_BPID(core_info->desc[iface_idx].iface_desc_1),
			IS_MASTER(core_info->desc[iface_idx].iface_desc_1)));

		/* If core is a Backplane or Bridge, then its slave port
		 * will give Core Registers.
		 */
		if (!IS_MASTER(core_info->desc[iface_idx].iface_desc_1) &&
			(IS_BACKPLANE(core_info->coreinfo) || core_info->desc[iface_idx].is_pmni)) {
			return _nci_get_curmap(nci, coreidx, iface_idx, 0);
		}
	}

	/* no valid slave port address is found */
	return 0;
}

static uint32
BCMPOSTTRAPFN(_nci_get_curwrap)(nci_info_t *nci, uint coreidx, uint wrapper_idx)
{
	return nci->cores[coreidx].desc[wrapper_idx].node_ptr;
}

static uint32
BCMPOSTTRAPFN(nci_get_curwrap)(nci_info_t *nci, uint coreidx, uint wrapper_idx)
{
	nci_cores_t *core_info = &nci->cores[coreidx];
	uint32 iface_idx;
	NCI_TRACE(("nci_get_curwrap coreidx %u\n", coreidx));

	/* If wrapper_idx is more than iface_cnt, then return the first booker i/f wrapper */
	if (wrapper_idx > core_info->iface_cnt) {
		ASSERT(wrapper_idx == ID32_INVALID);
	}

	for (iface_idx = 0; iface_idx < core_info->iface_cnt; iface_idx++) {
		NCI_TRACE(("nci_get_curwrap iface_idx %u BP_ID %u master %u\n",
			iface_idx, ID_BPID(core_info->desc[iface_idx].iface_desc_1),
			IS_MASTER(core_info->desc[iface_idx].iface_desc_1)));
		if (wrapper_idx == iface_idx || ((wrapper_idx == ID32_INVALID) &&
				(core_info->desc[iface_idx].is_booker ||
				core_info->desc[iface_idx].is_nic400))) {
			return _nci_get_curwrap(nci, coreidx, iface_idx);
		}
	}

	/* no valid master wrapper found */
	return 0;
}

static void
_nci_setcoreidx_pcie_bus(const si_t *sih, volatile void **regs, uint32 curmap,
		uint32 curwrap)
{
	si_info_t *sii = SI_INFO(sih);

	*regs = sii->curmap;
	switch (sii->slice) {
	case 0: /* main/first slice */
		/* point bar0 window */
		OSL_PCI_WRITE_CONFIG(sii->osh, PCI_BAR0_WIN, PCIE_WRITE_SIZE, curmap);
		// TODO: why curwrap is zero i.e no master wrapper
		if (curwrap != 0) {
			if (PCIE_GEN2(sii)) {
				OSL_PCI_WRITE_CONFIG(sii->osh, PCIE2_BAR0_WIN2,
					PCIE_WRITE_SIZE, curwrap);
			} else {
				OSL_PCI_WRITE_CONFIG(sii->osh, PCI_BAR0_WIN2,
					PCIE_WRITE_SIZE, curwrap);
			}
		}
		break;
	case 1: /* aux/second slice */
		/* PCIE GEN2 only for other slices */
		if (!PCIE_GEN2(sii)) {
			/* other slices not supported */
			NCI_ERROR(("pci gen not supported for slice 1\n"));
			ASSERT(0);
			break;
		}

		/* 0x4000 - 0x4fff: enum space 0x5000 - 0x5fff: wrapper space */

		*regs = (volatile uint8 *)*regs + PCI_SEC_BAR0_WIN_OFFSET;
		sii->curwrap = (void *)((uintptr)*regs + SI_CORE_SIZE);

		/* point bar0 window */
		OSL_PCI_WRITE_CONFIG(sii->osh, PCIE2_BAR0_CORE2_WIN, PCIE_WRITE_SIZE,	curmap);
		OSL_PCI_WRITE_CONFIG(sii->osh, PCIE2_BAR0_CORE2_WIN2, PCIE_WRITE_SIZE, curwrap);
		break;

	case 2: /* scan/third slice */
		/* PCIE GEN2 only for other slices */
		if (!PCIE_GEN2(sii)) {
			/* other slices not supported */
			NCI_ERROR(("pci gen not supported for slice 1\n"));
			ASSERT(0);
			break;
		}
		/* 0x9000 - 0x9fff: enum space 0xa000 - 0xafff: wrapper space */
		*regs = (volatile uint8 *)*regs + PCI_SEC_BAR0_WIN_OFFSET;
		sii->curwrap = (void *)((uintptr)*regs + SI_CORE_SIZE);

		/* point bar0 window */
		nci_corereg(sih, sih->buscoreidx, PCIE_TER_BAR0_WIN, ~0, curmap);
		nci_corereg(sih, sih->buscoreidx, PCIE_TER_BAR0_WRAPPER, ~0, curwrap);
		break;
	default:
		ASSERT(0);
		break;
	}
}

static volatile void *
BCMPOSTTRAPFN(_nci_setcoreidx)(const si_t *sih, uint coreidx, uint wrapper_idx)
{
	si_info_t *sii = SI_INFO(sih);
	nci_info_t *nci = sii->nci_info;
	nci_cores_t *cores_info = &nci->cores[coreidx];
	uint32 curmap, curwrap;
	volatile void *regs = NULL;

	NCI_TRACE(("_nci_setcoreidx coreidx %u\n", coreidx));
	if (!GOODIDX(coreidx, nci->num_cores)) {
		return NULL;
	}
	/*
	 * If the user has provided an interrupt mask enabled function,
	 * then assert interrupts are disabled before switching the core.
	 */
	ASSERT((sii->intrsenabled_fn == NULL) ||
		!(*(sii)->intrsenabled_fn)((sii)->intr_arg));

	curmap = nci_get_curmap(nci, coreidx);
	curwrap = nci_get_curwrap(nci, coreidx, wrapper_idx);

	switch (BUSTYPE(sih->bustype)) {
	case SI_BUS:
		/* map if does not exist */
		if (!cores_info->regs) {
			cores_info->regs = REG_MAP(curmap, SI_CORE_SIZE);
		}
		sii->curmap = regs = cores_info->regs;
		sii->curwrap = REG_MAP(curwrap, SI_CORE_SIZE);
		break;

	case PCI_BUS:
		_nci_setcoreidx_pcie_bus(sih, &regs, curmap, curwrap);
		break;
#ifdef BCMSDIO
	case SPI_BUS:
	case SDIO_BUS:
		sii->curmap = regs = (void *)((uintptr)curmap);
		sii->curwrap = (void *)((uintptr)curwrap);
		break;
#endif  /* BCMSDIO */

	default:
		NCI_ERROR(("_nci_stcoreidx Invalid bustype %d\n", BUSTYPE(sih->bustype)));
		break;
	}
	sii->curidx = coreidx;
	return regs;
}

/* Set the curwrap to first booker interface's wrapper */
volatile void *
BCMPOSTTRAPFN(nci_setcoreidx)(const si_t *sih, uint coreidx)
{
	return _nci_setcoreidx(sih, coreidx, ID32_INVALID);
}

/* Set the curwrap to wrapper idx provided by caller */
volatile void *
BCMPOSTTRAPFN(nci_setcoreidx_wrap)(const si_t *sih, uint coreidx, uint wrapper_idx)
{
	return _nci_setcoreidx(sih, coreidx, wrapper_idx);
}

volatile void *
BCMPOSTTRAPFN(nci_setcore)(si_t *sih, uint coreid, uint coreunit)
{
	si_info_t *sii = SI_INFO(sih);
	nci_info_t *nci = sii->nci_info;
	uint core_idx;

	NCI_INFO(("nci_setcore coreidx %u coreunit %u\n", coreid, coreunit));
	core_idx = nci_findcoreidx(sih, coreid, coreunit);

	if (!GOODIDX(core_idx, nci->num_cores)) {
		return NULL;
	}
	return nci_setcoreidx(sih, core_idx);
}

/* Get the value of the register at offset "offset" of currently configured core */
uint
BCMPOSTTRAPFN(nci_get_wrap_reg)(const si_t *sih, uint32 offset, uint32 mask, uint32 val)
{
	const si_info_t *sii = SI_INFO(sih);
	uint32 *addr = (uint32 *) ((uintptr)(sii->curwrap) + offset);
	NCI_TRACE(("nci_wrap_reg offset %u mask %u val %u\n", offset, mask, val));

	if (mask || val) {
		uint32 w = R_REG(sii->osh, addr);
		w &= ~mask;
		w |= val;
		W_REG(sii->osh, addr, w);
	}
	return (R_REG(sii->osh, addr));
}

uint
nci_corevendor(const si_t *sih)
{
	const si_info_t *sii = SI_INFO(sih);
	nci_info_t *nci = sii->nci_info;

	NCI_TRACE(("nci_corevendor coreidx %u\n", sii->curidx));
	return (nci->cores[sii->curidx].coreinfo & COREINFO_MFG_MASK) >> COREINFO_MFG_SHIFT;
}

uint
BCMPOSTTRAPFN(nci_corerev)(const si_t *sih)
{
	const si_info_t *sii = SI_INFO(sih);
	nci_info_t *nci = sii->nci_info;
	uint coreidx = sii->curidx;

	NCI_TRACE(("nci_corerev coreidx %u\n", coreidx));

	return (nci->cores[coreidx].coreinfo & COREINFO_REV_MASK) >> COREINFO_REV_SHIFT;
}

uint
nci_corerev_minor(const si_t *sih)
{
	return (nci_core_sflags(sih, 0, 0) >> SISF_MINORREV_D11_SHIFT) &
			SISF_MINORREV_D11_MASK;
}

uint
BCMPOSTTRAPFN(nci_coreid)(const si_t *sih, uint coreidx)
{
	const si_info_t *sii = SI_INFO(sih);
	nci_info_t *nci = sii->nci_info;

	NCI_TRACE(("nci_coreid coreidx %u\n", coreidx));
	return nci->cores[coreidx].coreid;
}

/** return total coreunit of coreid or zero if not found */
uint
BCMPOSTTRAPFN(nci_numcoreunits)(const si_t *sih, uint coreid)
{
	const si_info_t *sii = SI_INFO(sih);
	nci_info_t *nci = sii->nci_info;
	uint found = 0;
	uint i;

	NCI_TRACE(("nci_numcoreunits coreidx %u\n", coreid));

	for (i = 0; i < nci->num_cores; i++) {
		if (nci->cores[i].coreid == coreid) {
			found++;
		}
	}

	return found;
}

/* Return the address of the nth address space in the current core
 * Arguments:
 * sih : Pointer to struct si_t
 * spidx : slave port index
 * baidx : base address index
 */
uint32
nci_addr_space(const si_t *sih, uint spidx, uint baidx)
{
	const si_info_t *sii = SI_INFO(sih);
	uint cidx;

	NCI_TRACE(("nci_addr_space spidx %u baidx %u\n", spidx, baidx));
	cidx = sii->curidx;
	return _nci_get_curmap(sii->nci_info, cidx, spidx, baidx);
}

/* Return the size of the nth address space in the current core
* Arguments:
* sih : Pointer to struct si_t
* spidx : slave port index
* baidx : base address index
*/
uint32
nci_addr_space_size(const si_t *sih, uint spidx, uint baidx)
{
	const si_info_t *sii = SI_INFO(sih);
	uint cidx;

	NCI_TRACE(("nci_addr_space_size spidx %u baidx %u\n", spidx, baidx));

	cidx = sii->curidx;
	return _nci_get_slave_addr_size(sii->nci_info, cidx, spidx, baidx);
}

/*
 * Performs soft reset of attached device.
 * Writes have the following effect:
 * 0b1 Request attached device to enter reset.
 * Write is ignored if it occurs before soft reset exit has occurred.
 *
 * 0b0 Request attached device to exit reset.
 * Write is ignored if it occurs before soft reset entry has occurred.
 *
 * Software can poll this register to determine whether soft reset entry or exit has occurred,
 * using the following values:
 * 0b1 Indicates that the device is in reset.
 * 0b0 Indicates that the device is not in reset.
 *
 *
 * Note
 * The register value updates to reflect a request for reset entry or reset exit,
 * but the update can only occur after required internal conditions are met.
 * Until these conditions are met, a read to the register returns the old value.
 * For example, outstanding transactions currently being handled must complete before
 * the register value updates.
 *
 * To ensure reset propagation within the device,
 * it is the responsibility of software to allow enough cycles after
 * soft reset assertion is reflected in the reset control register
 * before exiting soft reset by triggering a write of 0b0.
 * If this responsibility is not met, the behavior is undefined or unpredictable.
 *
 * When the register value is 0b1,
 * the external soft reset pin that connects to the attached AXI master or slave
 * device is asserted, using the correct polarity of the reset pin.
 * When the register value is 0b0, the external softreset
 * pin that connects to the attached AXI master or slave device is deasserted,
 * using the correct polarity of the reset pin.
 */

static void
BCMPOSTTRAPFN(nci_core_reset_iface)(const si_t *sih, uint32 bits, uint32 resetbits, uint iface_idx,
		bool enable)
{
	const si_info_t *sii = SI_INFO(sih);
	nci_info_t *nci = sii->nci_info;
	amni_regs_t *amni = (amni_regs_t *)(uintptr)sii->curwrap;
	uint32 orig_bar0_win1 = 0u;
	volatile uint32 reg_read;

	/* Point to OOBR base */
	switch (BUSTYPE(sih->bustype)) {
	case SI_BUS:
		break;

	case PCI_BUS:
		/*
		 * Save Original Bar0 Win1. In nci, the io registers dmpctrl & dmpstatus
		 * registers are implemented in the EROM section. REF -
		 * https://docs.google.com/document/d/1HE7hAmvdoNFSnMI7MKQV1qVrFBZVsgLdNcILNOA2C8c
		 * This requires addition BAR0 windows mapping to erom section in chipcommon.
		 */
		orig_bar0_win1 = OSL_PCI_READ_CONFIG(nci->osh, PCI_BAR0_WIN,
			PCI_ACCESS_SIZE);

		OSL_PCI_WRITE_CONFIG(nci->osh, PCI_BAR0_WIN, PCI_ACCESS_SIZE,
			GET_OOBR_BASE(nci->cc_erom2base));
		break;

#ifdef BCMSDIO
	case SPI_BUS:
	case SDIO_BUS:
		break;
#endif  /* BCMSDIO */

	default:
		NCI_ERROR(("nci_core_reset_iface Invalid bustype %d\n", BUSTYPE(sih->bustype)));
		break;
	}

	NCI_TRACE(("nci_core_reset_iface amni %p\n", amni));

	if (!enable) {
		/* Put core into reset */
		OR_REG(nci->osh, &amni->ni.idm_reset_control, NI_IDM_RESET_ENTRY);
		NCI_INFO(("nci_core_reset_iface coreid 0x%x.%d iface_idx %d wrapper 0x%x"
			" RESET_ENTRY\n", nci->cores[sii->curidx].coreid,
			nci->cores[sii->curidx].coreunit, iface_idx,
			nci->cores[sii->curidx].desc[iface_idx].node_ptr));

		/* poll for the reset to happen or wait for NCI_SPINWAIT_TIMEOUT */
		SPINWAIT_TRAP(((reg_read = R_REG(nci->osh, &amni->ni.idm_reset_control) &
			NI_IDM_RESET_ENTRY) != NI_IDM_RESET_ENTRY), NCI_SPINWAIT_TIMEOUT);

	} else {
		/* take core out of reset if enable is TRUE */
		AND_REG(nci->osh, &amni->ni.idm_reset_control, ~NI_IDM_RESET_ENTRY);

		NCI_INFO(("nci_core_reset_iface coreid 0x%x.%d iface_idx %d wrapper 0x%x "
			"RESET_EXIT\n", nci->cores[sii->curidx].coreid,
			nci->cores[sii->curidx].coreunit, iface_idx,
			nci->cores[sii->curidx].desc[iface_idx].node_ptr));
		/* poll for the reset to happen or wait for NCI_SPINWAIT_TIMEOUT */
		SPINWAIT_TRAP((reg_read = R_REG(nci->osh, &amni->ni.idm_reset_control) &
			NI_IDM_RESET_ENTRY), NCI_SPINWAIT_TIMEOUT);
	}

	/* Point back to original base */
	if (BUSTYPE(sih->bustype) == PCI_BUS) {
		OSL_PCI_WRITE_CONFIG(nci->osh, PCI_BAR0_WIN, PCI_ACCESS_SIZE, orig_bar0_win1);
	}
}

/* reset and re-enable a core
 */
void
BCMPOSTTRAPFN(nci_core_reset)(si_t *sih, uint32 bits, uint32 resetbits)
{
	nci_core_reset_enable(sih, bits, resetbits, TRUE);
}

/* reset & re enable interfaces belong current core.
 * If enable is FALSE, it will keep the core disabled.
 */
void
BCMPOSTTRAPFN(nci_core_reset_enable)(const si_t *sih, uint32 bits, uint32 resetbits, bool enable)
{
	const si_info_t *sii = SI_INFO(sih);
	int32 iface_idx = 0u;
	nci_info_t *nci = sii->nci_info;
	nci_cores_t *core = &nci->cores[sii->curidx];
	uint32 dmp_mask = ~(SICF_FGC | SICF_CLOCK_EN);

	if (SRPWR_ENAB() && core->domain != DOMAIN_AAON) {
		si_srpwr_request(sih, 1u << core->domain, 1u << core->domain);
	}

	NCI_INFO(("nci_core_reset_enable coreid.unit 0x%x.%d bits=0x%x resetbits=0x%x enable %d\n",
			core->coreid, core->coreunit, bits, resetbits, enable));

	/* Remove Clock enable and force gated clock ON before setting reset bit */
	nci_core_cflags(sih, SICF_FGC | SICF_CLOCK_EN, 0u);

	OSL_DELAY(1u);
	/* Set the core specific DMP flags */
	nci_core_cflags(sih, dmp_mask, bits | resetbits);

	/* If Wrapper is of NIC400, then call AI functionality */
	for (iface_idx = core->iface_cnt-1; iface_idx >= 0; iface_idx--) {
		if (core->desc[iface_idx].is_shared_pmni) {
			continue;
		}
		nci_setcoreidx_wrap(sih, sii->curidx, iface_idx);
		nci_core_reset_iface(sih, bits, resetbits, iface_idx, FALSE);
	}

	/* Force the clock enable. */
	nci_core_cflags(sih, SICF_CLOCK_EN, SICF_CLOCK_EN);
	/* Set force clock gated ON. */
	nci_core_cflags(sih, SICF_FGC, SICF_FGC);

	OSL_DELAY(1u);

	/* Remove Clock enable and force gated clock ON before taking core out of reset */
	nci_core_cflags(sih, SICF_FGC | SICF_CLOCK_EN, 0u);

	if (enable) {
		for (iface_idx = core->iface_cnt-1; iface_idx >= 0; iface_idx--) {
			if (core->desc[iface_idx].is_shared_pmni ||
				core->desc[iface_idx].is_nic400) {
				continue;
			}
			nci_setcoreidx_wrap(sih, sii->curidx, iface_idx);
			nci_core_reset_iface(sih, bits, resetbits, iface_idx, TRUE);

		}
		/* Force the clock enable. */
		nci_core_cflags(sih, SICF_CLOCK_EN, SICF_CLOCK_EN);
		/* Set force clock gated ON. */
		nci_core_cflags(sih, SICF_FGC, SICF_FGC);

		OSL_DELAY(1u);

		/* Remove the force gated Clock */
		nci_core_cflags(sih, SICF_FGC, 0);
	}
}

static int32
BCMPOSTTRAPFN(nci_find_first_wrapper_idx)(nci_info_t *nci, uint32 coreidx)
{
	nci_cores_t *core_info = &nci->cores[coreidx];
	uint32 iface_idx;

	for (iface_idx = 0; iface_idx < core_info->iface_cnt; iface_idx++) {

		NCI_TRACE(("nci_find_first_wrapper_idx: %u BP_ID %u master %u\n", iface_idx,
			ID_BPID(core_info->desc[iface_idx].iface_desc_1),
			IS_MASTER(core_info->desc[iface_idx].iface_desc_1)));

		if (core_info->desc[iface_idx].is_booker || core_info->desc[iface_idx].is_nic400) {
			return iface_idx;
		}
	}

	/* no valid master wrapper found */
	return NCI_BAD_INDEX;
}

/* Disable the core.
 */
void
nci_core_disable(const si_t *sih, uint32 bits)
{
	const si_info_t *sii = SI_INFO(sih);
	nci_info_t *nci = sii->nci_info;
	int32 iface_idx;

	iface_idx = nci_find_first_wrapper_idx(nci, sii->curidx);

	if (iface_idx < 0) {
		NCI_ERROR(("nci_core_disable: First Wrapper is not found\n"));
		ASSERT(0u);
		goto end;
	}

	nci_core_reset_enable(sih, bits, 0u, FALSE);
end:
	return;
}

bool
BCMPOSTTRAPFN(nci_iscoreup)(const si_t *sih)
{
	const si_info_t *sii = SI_INFO(sih);
	nci_info_t *nci = sii->nci_info;
	nci_cores_t *core = &nci->cores[sii->curidx];
	amni_regs_t *amni = (amni_regs_t *)(uintptr)sii->curwrap;
	uint32 reset_ctrl;

	if (SRPWR_ENAB() && core->domain != DOMAIN_AAON) {
		si_srpwr_request(sih, 1u << core->domain, 1u << core->domain);
	}

	NCI_TRACE(("nci_iscoreup\n"));
	reset_ctrl = R_REG(nci->osh, &amni->ni.idm_reset_control) & NI_IDM_RESET_ENTRY;

	return (reset_ctrl == NI_IDM_RESET_ENTRY) ? FALSE : TRUE;
}

/* TODO: OOB Router core is not available. Can be removed. */
uint
nci_intflag(si_t *sih)
{
	return 0;
}

uint
nci_flag(si_t *sih)
{
	/* TODO: will be implemented if required for NCI */
	return 0;
}

uint
nci_flag_alt(const si_t *sih)
{
	/* TODO: will be implemented if required for NCI */
	return 0;
}

void
BCMATTACHFN(nci_setint)(const si_t *sih, int siflag)
{
	BCM_REFERENCE(sih);
	BCM_REFERENCE(siflag);

	/* TODO: Figure out how to set interrupt mask in nci */
}

/* TODO: OOB Router core is not available. Can we remove or need an alternate implementation. */
uint32
nci_oobr_baseaddr(const si_t *sih, bool second)
{
	return 0;
}

uint
nci_coreunit(const si_t *sih)
{
	const si_info_t *sii = SI_INFO(sih);
	nci_info_t *nci = sii->nci_info;
	nci_cores_t *cores = nci->cores;
	uint idx;
	uint coreid;
	uint coreunit;
	uint i;

	coreunit = 0;

	idx = sii->curidx;

	ASSERT(GOODREGS(sii->curmap));
	coreid = nci_coreid(sih, sii->curidx);

	/* count the cores of our type */
	for (i = 0; i < idx; i++) {
		if (cores[i].coreid == coreid) {
			coreunit++;
		}
	}

	return (coreunit);
}

uint
nci_corelist(const si_t *sih, uint coreid[])
{
	const si_info_t *sii = SI_INFO(sih);
	nci_info_t *nci = sii->nci_info;
	nci_cores_t *cores = nci->cores;
	uint32 i;

	for (i = 0; i < sii->numcores; i++) {
		coreid[i] = cores[i].coreid;
	}

	return (sii->numcores);
}

/* Return the number of address spaces in current core */
int
BCMATTACHFN(nci_numaddrspaces)(const si_t *sih)
{
	/* TODO: Either save it or parse the EROM on demand, currently hardcode 2 */
	BCM_REFERENCE(sih);

	return 2;
}

/* The value of wrap_pos should be greater than 0 */
/* wrapba, wrapba2 and wrapba3 */
uint32
nci_get_nth_wrapper(const si_t *sih, int32 wrap_pos)
{
	const si_info_t *sii = SI_INFO(sih);
	nci_info_t *nci = sii->nci_info;
	const nci_cores_t *core_info = &nci->cores[sii->curidx];
	uint32 iface_idx;
	uint32 addr = 0;

	ASSERT(wrap_pos >= 0);
	if (wrap_pos < 0) {
		return addr;
	}

	NCI_TRACE(("nci_get_curmap coreidx %u\n", sii->curidx));
	for (iface_idx = 0; iface_idx < core_info->iface_cnt; iface_idx++) {
	NCI_TRACE(("nci_get_curmap iface_idx %u BP_ID %u master %u\n",
		iface_idx, ID_BPID(core_info->desc[iface_idx].iface_desc_1),
		IS_MASTER(core_info->desc[iface_idx].iface_desc_1)));
		/* hack for core idx 8, coreidx without APB Backplane ID */
		if (!IS_MASTER(core_info->desc[iface_idx].iface_desc_1)) {
			continue;
		}
		/* TODO: Should the interface be only BOOKER or NIC is also fine. */
		if (GET_NODETYPE(core_info->desc[iface_idx].iface_desc_0) != NODE_TYPE_BOOKER) {
			continue;
		}
		/* Iterate till we do not get a wrapper at nth (wrap_pos) position */
		if (wrap_pos == 0) {
			break;
		}
		wrap_pos--;
	}
	if (iface_idx < core_info->iface_cnt) {
		addr = GET_NODEPTR(core_info->desc[iface_idx].iface_desc_0);
	}
	return addr;
}

/* Get slave port address of the 0th slave (csp2ba) */
uint32
BCMPOSTTRAPFN(nci_get_axi_addr)(const si_t *sih, uint32 *size, uint32 baidx)
{
	const si_info_t *sii = SI_INFO(sih);
	nci_info_t *nci = sii->nci_info;
	const nci_cores_t *core_info = (const nci_cores_t *)&nci->cores[sii->curidx];
	uint32 iface_idx;
	uint32 addr = 0;

	NCI_TRACE(("nci_get_axi_addr coreidx %u\n", sii->curidx));
	for (iface_idx = 0; iface_idx < core_info->iface_cnt; iface_idx++) {
		NCI_TRACE(("nci_get_axi_addr iface_idx %u BP_ID %u master %u\n",
		iface_idx, ID_BPID(core_info->desc[iface_idx].iface_desc_1),
		IS_MASTER(core_info->desc[iface_idx].iface_desc_1)));

		if (core_info->desc[iface_idx].is_axi && !core_info->desc[iface_idx].master) {
			break;
		}
	}
	if (iface_idx < core_info->iface_cnt) {
		if ((core_info->desc[iface_idx].num_addr_reg > baidx) &&
			(core_info->desc[iface_idx].sp != NULL)) {
			addr = core_info->desc[iface_idx].sp[baidx].addrl;
			if (size) {
				uint32 addrh = core_info->desc[iface_idx].sp[baidx].addrh;
				uint32 adesc = core_info->desc[iface_idx].sp[baidx].adesc;
				if (adesc & SLAVEPORT_BOUND_ADDR_MASK) {
					*size = (addrh - addr + 1);
				} else {
					*size = SLAVEPORT_ADDR_SIZE(adesc);
				}
			}
		 }
	}

	return addr;
}

/*
 * Returns the APB address of the current core. The baidx is the n'th address in APB interface.
 */
uint32
BCMPOSTTRAPFN(nci_get_core_baaddr)(const si_t *sih, uint32 *size, int32 baidx)
{
	const si_info_t *sii = SI_INFO(sih);
	nci_info_t *nci = sii->nci_info;
	const nci_cores_t *core_info = (const nci_cores_t *)&nci->cores[sii->curidx];
	uint32 iface_idx;
	uint32 addr = 0;

	NCI_TRACE(("nci_get_core_baaddr coreidx %u\n", sii->curidx));
	for (iface_idx = 0; iface_idx < core_info->iface_cnt; iface_idx++) {

		NCI_TRACE(("nci_get_core_baaddr iface_idx %u BP_ID %u master %u\n",
			iface_idx, ID_BPID(core_info->desc[iface_idx].iface_desc_1),
			IS_MASTER(core_info->desc[iface_idx].iface_desc_1)));

		/* hack for core idx 8, coreidx without APB Backplane ID */
		if (IS_MASTER(core_info->desc[iface_idx].iface_desc_1)) {
			continue;
		}
		if (core_info->desc[iface_idx].is_pmni) {
			break;
		}
	}
	if (iface_idx < core_info->iface_cnt) {
		if ((core_info->desc[iface_idx].num_addr_reg > baidx) &&
			(core_info->desc[iface_idx].sp != NULL)) {
			addr = core_info->desc[iface_idx].sp[baidx].addrl;
			if (size) {
				uint32 adesc = core_info->desc[iface_idx].sp[baidx].adesc;
				uint32 addrh = core_info->desc[iface_idx].sp[baidx].addrh;
				if (adesc & SLAVEPORT_BOUND_ADDR_MASK) {
					*size = (addrh - addr + 1);
				} else {
					*size = SLAVEPORT_ADDR_SIZE(adesc);
				}
			}
		 }
	}

	return addr;
}

/*
 * Returns APB/AXI SP address,
 * spidx - CORE_SLAVE_PORT_0 for APB and CORE_SLAVE_PORT_1 for AXI
 * baidx - Base address within slave port
 */
uint32
BCMPOSTTRAPFN(nci_addrspace)(const si_t *sih, uint spidx, uint baidx)
{
	uint32 addr = 0;

	if (spidx == CORE_SLAVE_PORT_0) {
		/* Get APB address */
		addr = nci_get_core_baaddr(sih, NULL, baidx);
	} else if (spidx == CORE_SLAVE_PORT_1) {
		/* Get AXI slave base address */
		addr = nci_get_axi_addr(sih, NULL, baidx);
	}

	return addr;
}

/*
 * Returns APB/AXI SP address space size,
 * spidx - CORE_SLAVE_PORT_0 for APB and CORE_SLAVE_PORT_1 for AXI
 * baidx - Base address within slave port
 */
uint32
BCMPOSTTRAPFN(nci_addrspacesize)(const si_t *sih, uint spidx, uint baidx)
{
	uint32 size = 0;

	if (spidx == CORE_SLAVE_PORT_0) {
		/* Get APB address space size */
		nci_get_core_baaddr(sih, &size, baidx);
	} else if (spidx == CORE_SLAVE_PORT_1) {
		/* Get AXI slave space size */
		nci_get_axi_addr(sih, &size, baidx);
	}
	return size;
}

uint32
BCMPOSTTRAPFN(nci_core_cflags)(const si_t *sih, uint32 mask, uint32 val)
{
	const si_info_t *sii = SI_INFO(sih);
	nci_info_t *nci = sii->nci_info;
	nci_cores_t *core = &nci->cores[sii->curidx];
	volatile uint32 reg_read = 0u;
	uint32 orig_bar0_win1 = 0;
	int32 iface_idx;
	uint32 w;
	volatile dmp_regs_t *io = sii->curwrap;

	BCM_REFERENCE(iface_idx);

	ASSERT(GOODREGS(sii->curwrap));
	ASSERT((val & ~mask) == 0);

	iface_idx = nci_find_first_wrapper_idx(nci, sii->curidx);
	if (iface_idx < 0) {
		NCI_ERROR(("nci_core_cflags: First Wrapper is not found\n"));
		ASSERT(0u);
		goto end;
	}

	/* BOOKER */
	/* Point to OOBR base */
	switch (BUSTYPE(sih->bustype)) {
	case SI_BUS:
		io = (volatile dmp_regs_t *)
			REG_MAP(GET_OOBR_BASE(nci->cc_erom2base), SI_CORE_SIZE);
		break;

	case PCI_BUS:
		/* Save Original Bar0 Win1 */
		orig_bar0_win1 =
			OSL_PCI_READ_CONFIG(nci->osh, PCI_BAR0_WIN, PCI_ACCESS_SIZE);

		OSL_PCI_WRITE_CONFIG(nci->osh, PCI_BAR0_WIN, PCI_ACCESS_SIZE,
			GET_OOBR_BASE(nci->cc_erom2base));
		io = (volatile dmp_regs_t *)sii->curmap;
		break;

#ifdef BCMSDIO
	case SPI_BUS:
	case SDIO_BUS:
		io = (volatile dmp_regs_t *)nci->oobr_base;
		break;
#endif  /* BCMSDIO */

	default:
		NCI_ERROR(("nci_core_cflags Invalid bustype %d\n", BUSTYPE(sih->bustype)));
		goto end;
	}

	if (core->desc[iface_idx].oobr_core) {
		/* Point to DMP Control */
		io = (dmp_regs_t *)(NCI_ADD_ADDR(io, nci->cores[sii->curidx].dmp_regs_off));

		if (mask || val) {
			w = ((R_REG(sii->osh, &io->dmpctrl) & ~mask) | val);
			W_REG(sii->osh, &io->dmpctrl, w);
			NCI_INFO(("nci_core_cflags coreid.unit 0x%x.%d WREG dmpctrl "
				"value=0x%x\n", nci->cores[sii->curidx].coreid,
				nci->cores[sii->curidx].coreunit, w));

			/* poll for the reset to happen or wait for NCI_SPINWAIT_TIMEOUT */
			SPINWAIT_TRAP(((reg_read = R_REG(nci->osh, &io->dmpctrl)) !=
				w), NCI_SPINWAIT_TIMEOUT);
		}
		reg_read = R_REG(sii->osh, &io->dmpctrl);
	}

	/* Point back to original base */
	if (BUSTYPE(sih->bustype) == PCI_BUS) {
		OSL_PCI_WRITE_CONFIG(nci->osh, PCI_BAR0_WIN,
			PCI_ACCESS_SIZE, orig_bar0_win1);
	}
end:
	return reg_read;
}

void
BCMPOSTTRAPFN(nci_core_cflags_wo)(const si_t *sih, uint32 mask, uint32 val)
{
	const si_info_t *sii = SI_INFO(sih);
	nci_info_t *nci = sii->nci_info;
	nci_cores_t *core = &nci->cores[sii->curidx];
	volatile dmp_regs_t *io = sii->curwrap;
	uint32 orig_bar0_win1 = 0;
	int32 iface_idx;
	uint32 w;

	BCM_REFERENCE(iface_idx);

	if ((core[sii->curidx].coreid) == PMU_CORE_ID) {
		NCI_ERROR(("nci_core_cflags: Accessing PMU DMP register (ioctrl)\n"));
		goto end;
	}

	ASSERT(GOODREGS(sii->curwrap));
	ASSERT((val & ~mask) == 0);

	iface_idx = nci_find_first_wrapper_idx(nci, sii->curidx);

	if (iface_idx < 0) {
		NCI_ERROR(("nci_core_cflags_wo: First Wrapper is not found\n"));
		ASSERT(0u);
		goto end;
	}

	if (core->desc[iface_idx].oobr_core) {
		/* BOOKER */
		/* Point to OOBR base */
		switch (BUSTYPE(sih->bustype)) {
		case SI_BUS:
			io = (volatile dmp_regs_t *)
				REG_MAP(GET_OOBR_BASE(nci->cc_erom2base), SI_CORE_SIZE);
			break;

		case PCI_BUS:
			/* Save Original Bar0 Win1 */
			orig_bar0_win1 =
				OSL_PCI_READ_CONFIG(nci->osh, PCI_BAR0_WIN,
				PCI_ACCESS_SIZE);

			OSL_PCI_WRITE_CONFIG(nci->osh, PCI_BAR0_WIN, PCI_ACCESS_SIZE,
				GET_OOBR_BASE(nci->cc_erom2base));
			io = (volatile dmp_regs_t *)sii->curmap;
			break;

#ifdef BCMSDIO
		case SPI_BUS:
		case SDIO_BUS:
			io = (volatile dmp_regs_t *)nci->oobr_base;
			break;
#endif  /* BCMSDIO */

		default:
			NCI_ERROR(("nci_core_cflags_wo Invalid bustype %d\n",
				BUSTYPE(sih->bustype)));
			goto end;
		}

		/* Point to DMP Control */
		io = (dmp_regs_t *)(NCI_ADD_ADDR(io, nci->cores[sii->curidx].dmp_regs_off));

		if (mask || val) {
			w = ((R_REG(sii->osh, &io->dmpctrl) & ~mask) | val);
			W_REG(sii->osh, &io->dmpctrl, w);
		}

		/* Point back to original base */
		if (BUSTYPE(sih->bustype) == PCI_BUS) {
			OSL_PCI_WRITE_CONFIG(nci->osh, PCI_BAR0_WIN,
				PCI_ACCESS_SIZE, orig_bar0_win1);
		}
	} else {
		NCI_ERROR(("Coreidx=%d is Non-OOBR\n", sii->curidx));
	}

end:
	return;
}

uint32
nci_core_sflags(const si_t *sih, uint32 mask, uint32 val)
{
	const si_info_t *sii = SI_INFO(sih);
	nci_info_t *nci = sii->nci_info;
	nci_cores_t *core = &nci->cores[sii->curidx];
	uint32 orig_bar0_win1 = 0u;
	volatile uint32 reg_read = 0u;
	int32 iface_idx;
	uint32 w;

	BCM_REFERENCE(iface_idx);

	if ((core[sii->curidx].coreid) == PMU_CORE_ID) {
		NCI_ERROR(("nci_core_sflags: Accessing PMU DMP register (ioctrl)\n"));
		goto end;
	}

	ASSERT(GOODREGS(sii->curwrap));

	ASSERT((val & ~mask) == 0u);
	ASSERT((mask & ~SISF_CORE_BITS) == 0u);

	iface_idx = nci_find_first_wrapper_idx(nci, sii->curidx);
	if (iface_idx < 0) {
		NCI_ERROR(("nci_core_sflags: First Wrapper is not found\n"));
		ASSERT(0u);
		goto end;
	}

	if (core->desc[iface_idx].oobr_core) {
		volatile dmp_regs_t *io = sii->curwrap;

		/* BOOKER */
		/* Point to OOBR base */
		switch (BUSTYPE(sih->bustype)) {
		case SI_BUS:
			io = (volatile dmp_regs_t *)
				REG_MAP(GET_OOBR_BASE(nci->cc_erom2base), SI_CORE_SIZE);
			break;

		case PCI_BUS:
			/* Save Original Bar0 Win1 */
			orig_bar0_win1 =
				OSL_PCI_READ_CONFIG(nci->osh, PCI_BAR0_WIN, PCI_ACCESS_SIZE);

			OSL_PCI_WRITE_CONFIG(nci->osh, PCI_BAR0_WIN, PCI_ACCESS_SIZE,
			GET_OOBR_BASE(nci->cc_erom2base));
			io = (volatile dmp_regs_t *)sii->curmap;
			break;

#ifdef BCMSDIO
		case SPI_BUS:
		case SDIO_BUS:
			io = (volatile dmp_regs_t *)nci->oobr_base;
			break;
#endif  /* BCMSDIO */

		default:
			NCI_ERROR(("nci_core_sflags Invalid bustype %d\n", BUSTYPE(sih->bustype)));
			goto end;
		}

		/* Point to DMP Control */
		io = (dmp_regs_t *)(NCI_ADD_ADDR(io, nci->cores[sii->curidx].dmp_regs_off));

		if (mask || val) {
			w = ((R_REG(sii->osh, &io->dmpstatus) & ~mask) | val);
			W_REG(sii->osh, &io->dmpstatus, w);
		}

		reg_read = R_REG(sii->osh, &io->dmpstatus);

		/* Point back to original base */
		if (BUSTYPE(sih->bustype) == PCI_BUS) {
			OSL_PCI_WRITE_CONFIG(nci->osh, PCI_BAR0_WIN,
				PCI_ACCESS_SIZE, orig_bar0_win1);
		}
	} else {
		NCI_ERROR(("Coreidx=%d is Non-OOBR\n", sii->curidx));
	}
end:
	return reg_read;
}

/* TODO: Used only by host */
int
nci_backplane_access(si_t *sih, uint addr, uint size, uint *val, bool read)
{
	return 0;
}

int
nci_backplane_access_64(si_t *sih, uint addr, uint size, uint64 *val, bool read)
{
	return 0;
}

uint
nci_num_slaveports(const si_t *sih, uint coreidx)
{
const si_info_t *sii = SI_INFO(sih);
	nci_info_t *nci = sii->nci_info;
	const nci_cores_t *core_info = (const nci_cores_t *)&nci->cores[coreidx];
	uint32 iface_idx;
	uint32 numports = 0;

	NCI_TRACE(("nci_get_curmap coreidx %u\n", coreidx));
	for (iface_idx = 0; iface_idx < core_info->iface_cnt; iface_idx++) {
	NCI_TRACE(("nci_get_curmap iface_idx %u BP_ID %u master %u\n",
		iface_idx, ID_BPID(core_info->desc[iface_idx].iface_desc_1),
		IS_MASTER(core_info->desc[iface_idx].iface_desc_1)));
		/* hack for core idx 8, coreidx without APB Backplane ID */
		if (IS_MASTER(core_info->desc[iface_idx].iface_desc_1)) {
			continue;
		}
		if (core_info->desc[iface_idx].is_pmni) {
			break;
		}
	}
	if (iface_idx < core_info->iface_cnt) {
		numports = core_info->desc[iface_idx].num_addr_reg;
	}

	return numports;
}

#if defined(BCMDBG) || defined(BCMDBG_DUMP) || defined(BCMDBG_PHYDUMP)
void
nci_dumpregs(const si_t *sih, struct bcmstrbuf *b)
{
	const si_info_t *sii = SI_INFO(sih);

	bcm_bprintf(b, "ChipNum:%x, ChipRev;%x, BusType:%x, BoardType:%x, BoardVendor:%x\n\n",
			sih->chip, sih->chiprev, sih->bustype, sih->boardtype, sih->boardvendor);
	BCM_REFERENCE(sii);
	/* TODO: Implement dump regs for nci. */
}
#endif  /* BCMDBG || BCMDBG_DUMP || BCMDBG_PHYDUMP */

#ifdef BCMDBG
static void
_nci_view(osl_t *osh, aidmp_t *ai, uint32 cid, uint32 addr, bool verbose)
{
	/* TODO: This is WIP and will be developed once the
	 * implementation is done based on the NCI.
	 */
}

void
nci_view(si_t *sih, bool verbose)
{
	const si_info_t *sii = SI_INFO(sih);
	nci_info_t *nci = sii->nci_info;
	const nci_cores_t *core_info = (const nci_cores_t *)nci->cores;
	osl_t *osh;
	/* TODO: We need to do the structure mapping correctly based on the BOOKER/NIC type */
	aidmp_t *ai;
	uint32 cid, addr;

	ai = sii->curwrap;
	osh = sii->osh;

	if ((core_info[sii->curidx].coreid) == PMU_CORE_ID) {
		SI_ERROR(("Cannot access pmu DMP\n"));
		return;
	}
	cid = core_info[sii->curidx].coreid;
	addr = nci_get_nth_wrapper(sih, 0u);
	_nci_view(osh, ai, cid, addr, verbose);
}

void
nci_viewall(si_t *sih, bool verbose)
{
	const si_info_t *sii = SI_INFO(sih);
	nci_info_t *nci = sii->nci_info;
	const nci_cores_t *core_info = (const nci_cores_t *)nci->cores;
	osl_t *osh;
	aidmp_t *ai;
	uint32 cid, addr;
	uint i;

	osh = sii->osh;
	for (i = 0; i < sii->numcores; i++) {
		nci_setcoreidx(sih, i);

		if ((core_info[i].coreid) == PMU_CORE_ID) {
			SI_ERROR(("Skipping pmu DMP\n"));
			continue;
		}
		ai = sii->curwrap;
		cid = core_info[i].coreid;
		addr = nci_get_nth_wrapper(sih, 0u);
		_nci_view(osh, ai, cid, addr, verbose);
	}
}
#endif /* BCMDBG */

bool
BCMPOSTTRAPFN(nci_is_dump_err_disabled)(const si_t *sih, nci_cores_t *core)
{
	uint num_coreid = ARRAYSIZE(nci_wraperr_skip_coreids);
	uint i;
	bool ret = FALSE;

	if (IS_BACKPLANE(core->coreinfo)) {
		ret = TRUE;
		goto end;
	}

	for (i = 0; i < num_coreid; i++) {
		if (nci_wraperr_skip_coreids[i] == core->coreid) {
			ret = TRUE;
			break;
		}
	}

end:
	return ret;
}

uint32 last_axi_error_log_status;
uint32 last_axi_error_core;
uint32 last_axi_error_wrap;
uint32 last_axi_errlog_lo;
uint32 last_axi_errlog_hi;
uint32 last_axi_errlog_id;
uint32 last_axi_errlog_trans_sts;

#if defined(AXI_TIMEOUTS)
static bool g_backplane_logs_enabled = FALSE;
/*
 * Clears BP timeout/error if set on all cores.
 */
uint32
BCMPOSTTRAPFN(nci_clear_backplane_to)(si_t *sih)
{
	uint32 ret = 0u;
	const si_info_t *sii = SI_INFO(sih);
	nci_info_t *nci = sii->nci_info;
	uint32 i;
	uint32 tmp;

	if (SRPWR_ENAB()) {
		si_srpwr_request(sih, SRPWR_DMN_ALL_MASK(sih), SRPWR_DMN_ALL_MASK(sih));
	}
	for (i = 0u; i < nci->num_cores; i++) {

		if (nci_is_dump_err_disabled(sih, &nci->cores[i])) {
			continue;
		}

		tmp = nci_clear_backplane_to_per_core(sih, nci->cores[i].coreid,
				nci->cores[i].coreunit);
		ret |= tmp;
	}
	return ret;
}

/*
 * Enable/disable backplane timeouts. When enable is true, interrupt on AXI error/timeout will be
 * enable always.
 * When cid is zero, it will enable for all the available cores.
 */
void
nci_update_backplane_timeouts(const si_t *sih, bool enable, uint32 idm_timeout_value, uint32 cid)
{
	const si_info_t *sii = SI_INFO(sih);
	nci_info_t *nci = sii->nci_info;
	uint32 idm_errctlr, idm_interrupt_mask;
	uint32 i, iface_idx;
	nci_cores_t *core_info;
	volatile amni_regs_t *amni;
	TX_INTERRUPT_SAVE_AREA

	if ((g_backplane_logs_enabled && enable) || (!enable && !g_backplane_logs_enabled)) {
		goto exit;
	}

	TX_DISABLE

	si_srpwr_request(sih, SRPWR_DMN_ALL_MASK(sih), SRPWR_DMN_ALL_MASK(sih));

	if (enable) {
		idm_errctlr = IDM_ERRCTLR_BUS | IDM_ERRCTLR_ENABLE | IDM_ERRCTLR_UE;
		g_backplane_logs_enabled = TRUE;
		idm_interrupt_mask = 0u;
	} else {
		idm_timeout_value = 0u;
		idm_errctlr = 0u;
		idm_interrupt_mask = IDM_TD_IRQ_MASK | IDM_ERR_IRQ_MASK | IDM_RESET_ACC_IRQ_MASK;
		g_backplane_logs_enabled = FALSE;
	}
	for (i = 0u; i < nci->num_cores; i++) {

		core_info = &nci->cores[i];
		if (nci_is_dump_err_disabled(sih, core_info)) {
			continue;
		}

		for (iface_idx = 0u; iface_idx < core_info->iface_cnt; iface_idx++) {
			interface_desc_t *ifdesc = &core_info->desc[iface_idx];

			if (!NCI_IS_INTERFACE_ERR_DUMP_ENAB(ifdesc)) {
				continue;
			}
			/* use amni_regs_t for slave also. idm_regs is always fixed from wrapper */
			amni = (volatile amni_regs_t *)ifdesc->node_ptr;
			W_REG(sii->osh, &amni->ni.idm_interrupt_mask, idm_interrupt_mask);
			W_REG(sii->osh, &amni->ni.idm_interrupt_mask_ns, idm_interrupt_mask);
			W_REG(sii->osh, &amni->ni.idm_errctlr, idm_errctlr);

			/* Don't enable AXI timeuts on ARM and  PCIE ASNI/master. This is to avoid
			 * ARM and PCIE ASNI getting into reset/recovery if timeout is detected.
			 */
			if (!(ifdesc->master && (core_info->coreid == ARM_CORE_ID ||
					core_info->coreid == PCIE2_CORE_ID))) {
				W_REG(sii->osh, &amni->ni.idm_timeout_control,
					enable ? 1u : 0u);
				(void)R_REG(sii->osh, &amni->ni.idm_timeout_control);
				W_REG(sii->osh, &amni->ni.idm_timeout_value,
						idm_timeout_value);
			}
		}
	}

	TX_RESTORE
exit:
	return;
}

bool
BCMPOSTTRAPFN(nci_is_backplane_logs_enabled)(void)
{
	return g_backplane_logs_enabled;
}

/* Getting the last AXI error. Need enhancement when multiple wrappers set error. */
void
BCMPOSTTRAPFN(nci_wrapper_get_last_error)(const si_t *sih, uint32 *error_status, uint32 *core,
		uint32 *lo, uint32 *hi, uint32 *id)
{
	*error_status = last_axi_error_log_status;
	*core = last_axi_error_core;
	*lo = last_axi_errlog_lo;
	*hi = last_axi_errlog_hi;
	*id = last_axi_errlog_id;
}

/* Function to check whether AXI timeout has been registered on a core */
uint32
BCMPOSTTRAPFN(nci_get_axi_timeout_reg)(void)
{
	return GOODREGS(last_axi_errlog_lo) ? last_axi_errlog_lo : 0;
}
#endif /* AXI_TIMEOUTS */

/* Dumps and clear AXI error/timeout for a given wrapper if set. */
uint32
BCMPOSTTRAPFN(nci_dump_and_clr_err_log)(si_t *sih, uint32 coreidx, uint32 iface_idx,
		void *wrapper, bool is_master, uint32 wrapper_daddr)
{
	nci_error_container_t error_cont = {0};
	uint32 retval = nci_error_helper(SI_INFO(sih)->osh, wrapper, is_master,
		&error_cont, wrapper_daddr);
	if (retval) {
		last_axi_error_core = si_coreid(sih);
		last_axi_error_wrap = wrapper_daddr;
		last_axi_error_log_status = error_cont.error_log_status;
		last_axi_errlog_lo = error_cont.errlog_lo;
		last_axi_errlog_hi = error_cont.errlog_hi;
		last_axi_errlog_id = error_cont.errlog_id;
		last_axi_errlog_trans_sts = error_cont.errlog_trans_sts;
	}
	return retval;
}

/* API to clear the back plane timeout per core. */
uint32
BCMPOSTTRAPFN(nci_clear_backplane_to_per_core)(si_t *sih, uint coreid, uint coreunit)
{
	const si_info_t *sii = SI_INFO(sih);
	nci_info_t *nci = sii->nci_info;

	int ret = AXI_WRAP_STS_NONE;
	void *wrapper;
	uint32 iface_idx;
	nci_cores_t *core_info;
	uint32 current_coreidx = si_coreidx(sih);
	uint32 target_coreidx = nci_findcoreidx(sih, coreid, coreunit);
	bool restore_core = FALSE;
	uint32 wrapper_daddr = 0;

	if (coreid && (target_coreidx != current_coreidx)) {

		nci_setcoreidx(sih, target_coreidx);

		if (si_coreidx(sih) != target_coreidx) {
			/* Unable to set the core */
			posttrap_printf("Set Core Failed: coreid:%x, unit:%d, target_coreidx:%d\n",
					coreid, coreunit, target_coreidx);
			ret = AXI_WRAP_STS_SET_CORE_FAIL;
			goto end;
		}
		restore_core = TRUE;
	} else {
		/* Update CoreID to current Code ID */
		coreid = nci_coreid(sih, sii->curidx);
	}

	core_info = &nci->cores[target_coreidx];
	for (iface_idx = 0u; iface_idx < core_info->iface_cnt; iface_idx++) {

		if (!NCI_IS_INTERFACE_ERR_DUMP_ENAB(&core_info->desc[iface_idx])) {
			continue;
		}

		wrapper_daddr = _nci_get_curwrap(nci, target_coreidx, iface_idx);
		nci_setcoreidx_wrap(sih, target_coreidx, iface_idx);
		wrapper = (void *)(unsigned long)sii->curwrap;

		ret |= nci_dump_and_clr_err_log(sih, target_coreidx, iface_idx,
				wrapper, core_info->desc[iface_idx].master, wrapper_daddr);
	}
end:
	if (restore_core) {
		if (nci_setcoreidx(sih, current_coreidx) == NULL) {
			/* Unable to set the core */
			return ID32_INVALID;
		}
	}
	return ret;
}

uint32
BCMATTACHFN(nci_wrapper_dump_buf_size)(const si_t *sih)
{
	uint32 buf_size = 0;
	uint32 wrapper_count = 0;
	const si_info_t *sii = SI_INFO(sih);
	nci_info_t *nci = sii->nci_info;
	nci_cores_t *core_info;
	uint8 i = 0;

	for (i = 0; i < nci->num_cores; i++) {
		if (!nci_is_dump_err_disabled(sih, &nci->cores[i])) {
			core_info = &nci->cores[i];
			wrapper_count += core_info->iface_cnt;
		}
	}

	if (wrapper_count == 0) {
		return 0;
	}

	/* cnt indicates how many registers, tag_id 0 will say these are address/value */
	/* address/value pairs */
	buf_size = 2 * sizeof(uint32) * (nci_get_sizeof_wrapper_offsets_to_dump() * wrapper_count);

	return buf_size;
}

uint32
BCMATTACHFN(nci_get_sizeof_wrapper_offsets_to_dump)(void)
{
	uint8 j = 0;
	uint8 num_off = sizeof(nci_wrapper_regs) / sizeof(dump_regs_t);
	uint32 bitmap = 0, count = 0;
	for (j = 0; j < num_off; j++) {
		bitmap = nci_wrapper_regs[j].bit_map;
		while (bitmap) {
			if (bitmap & 0x1u) {
				count++;
			}
			bitmap = bitmap >> 0x1u;
		}
	}
	return (count);
}

uint32
BCMPOSTTRAPFN(nci_wrapper_dump_binary)(const si_t *sih, uchar *p)
{
	uint32 *ptr32 = (uint32 *)p;
	uint32 i, j;
	const si_info_t *sii = SI_INFO(sih);
	nci_info_t *nci = sii->nci_info;
	uint32 iface_idx;
	nci_cores_t *core_info;
	uint8 num_off = sizeof(nci_wrapper_regs) / sizeof(dump_regs_t);
	uint32 *addr;
	uint32 daddr;
	uint32 bitmap;
	uint8 *wrapper;

	if (SRPWR_ENAB()) {
		si_srpwr_request(sih, SRPWR_DMN_ALL_MASK(sih), SRPWR_DMN_ALL_MASK(sih));
	}

	for (i = 0; i < nci->num_cores; i++) {
		core_info = &nci->cores[i];

		if (nci_is_dump_err_disabled(sih, &nci->cores[i])) {
			continue;
		}

		for (iface_idx = 0; iface_idx < core_info->iface_cnt; iface_idx++) {
			interface_desc_t *ifdesc = &core_info->desc[iface_idx];
			/* Bypass if it is not a booker node OR not AMNI/ASNI OR not backplane */
			if (!NCI_IS_INTERFACE_ERR_DUMP_ENAB(ifdesc)) {
				continue;
			}
			nci_setcoreidx_wrap(sih, i, iface_idx);
			wrapper = (uint8 *)((unsigned long)sii->curwrap);
			for (j = 0; j < num_off; j++) {
				addr = (uint32 *)((wrapper + nci_wrapper_regs[j].offset));
				/* daddr is the dongle register address (ex:- 0x1851a100)
				 * for dongle compilation both addr and daddr will have the
				 * same value (0x1851a100), but for host compilation
				 * addr will have a different value i.e, a memory mapped
				 * kernel virtual address (something like 0xffffffc0100039c0)
				 * using which R_REG can be done.
				 * But in the output buffer (*ptr) we need only the
				 * dongle register address.
				 * Hence both daddr and addr are required.
				 */
				daddr = (uint32)((unsigned long)(ifdesc->node_ptr +
					nci_wrapper_regs[j].offset));
				bitmap = nci_wrapper_regs[j].bit_map;
				while (bitmap) {
					if ((bitmap & 0x1u)) {
						*ptr32++ = daddr;
						*ptr32++ = R_REG(sii->osh,
							(uint32 *)((unsigned long)addr));
					}
					bitmap = bitmap >> 0x1u;
					addr++;
					daddr += 4;
				}
			}
		}
	}

	return 0;
}

int nci_get_amni_slave_cfg_cc_reg_addrs(si_t *sih, volatile uint32 **idm_errstatus_addr,
	volatile uint32 **idm_intstatus_addr)
{
	const si_info_t *sii = SI_INFO(sih);
	amni_regs_t *amni = (amni_regs_t *)(uintptr)sii->curwrap;
	*idm_errstatus_addr = &amni->ni.idm_errstatus;
	*idm_intstatus_addr = &amni->ni.idm_interrupt_status;
	return 0;
}
