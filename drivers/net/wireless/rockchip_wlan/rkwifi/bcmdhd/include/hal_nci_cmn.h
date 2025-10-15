/*
 * Shared code between the legacy nci implementation which depended on the EROM and the
 * implementation which depends on vlsi_data.
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
/**
 * @file
 * @brief Shared code between EROM dependent and vlsi dependent implementations.
 */

#ifndef _hal_nci_cmn_h_
#define _hal_nci_cmn_h_

#include <typedefs.h>

#define IDM_ERRSTATUS_ILLEGAL_ADDR_DECODE	13u	/* Illegal address decode error */
#define IDM_ERRSTATUS_SLAVE_ERROR_RESP		18u	/* Error response from slave */
#define IDM_ERRSTATUS_INTERNAL_TIMEOUT		20u	/* Internal timeout */

#define IDM_ERRSTATUS_ERROR_VALID	(1u << 30)	/* error status register is valid */
#define IDM_ERRSTATUS_ERR_ADDR_VALID	(1u << 31)	/* error addresses  MSB/LSB are valid */
#define IDM_ERRSTATUS_UNCORRECTED_ERR	(1u << 29)	/* error addresses  un corrected error */
#define IDM_ERRSTATUS_SECOND_ERR	(1u << 27)	/* error addresses  un corrected error */
#define IDM_ERRSTATUS_MISC_ERROR	(1u << 26)	/* MISC0 and MISC1 are valid */
#define IDM_ERRSTATUS_ERROR_CODE_MASK	0xffu		/* Indicates the type of error */

#define IDM_ERRCTLR_ENABLE		(1u << 0u)	/*  Global enable */
#define IDM_ERRCTLR_BUS			(1u << 1u)	/*  Enable Bus error detection */
#define IDM_ERRCTLR_UE			(1u << 2u)	/*  uncorrected error */

static const char BCMPOST_TRAP_RODATA(nci_axi_err_str[][16u]) = {
	 "AXI Decode Err ",
	 "AXI Slave Err ",
	 "AXI Timeout "
};

/* err_types */
#define NCI_TYPE_STR_DECODE_ERR		0u
#define NCI_TYPE_STR_SLAVE_ERR		1u
#define NCI_TYPE_STR_TIMEOUT		2u

#define NI_IDM_RESET_ENTRY 0x1
/* SpinWait for 5000us */
#define NCI_SPINWAIT_TIMEOUT		(500u * 10u)

/**
 * @struct idm_regs
 * @brief Network Interface IDM registers summary.
 * Common for ASNI, AMNI, PMNI etc. The address offset of these are at 0x100 from wrapper space.
 */
typedef volatile struct idm_regs {
	/* Check asni_regs/amni_regs for usage. */
	uint32 idm_device_id;		/* 0x100 */
	uint32 idm_config;		/* 0x104 */
	uint32 idm_errctlr;		/* 0x108 */
	uint32 PAD[1];
	uint32 idm_errstatus;		/* 0x110 */
	uint32 idm_erraddr_lsb;		/* 0x114 */
	uint32 idm_erraddr_msb;		/* 0x118 */
	uint32 PAD[3];
	uint32 idm_errmisc0;		/* 0x128 */
	uint32 idm_errmisc1;		/* 0x12C */
	uint32 idm_access_control;	/* 0x130 */
	uint32 idm_access_status;	/* 0x134 */
	uint32 idm_access_readid;	/* 0x138 */
	uint32 idm_access_writeid;	/* 0x13C */
	uint32 idm_reset_control;	/* 0x140 */
	uint32 idm_reset_status;	/* 0x144 */
	uint32 idm_reset_readid;	/* 0x148 */
	uint32 idm_reset_writeid;	/* 0x14C */
	uint32 idm_timeout_control;	/* 0x150 */
	uint32 idm_timeout_value;	/* 0x154 */
	uint32 idm_interrupt_status;	/* 0x158 */
	uint32 idm_interrupt_mask;	/* 0x15C */
	uint32 idm_errstatus_ns;	/* 0x160 */
	uint32 idm_erraddr_lsb_ns;	/* 0x164 */
	uint32 idm_erraddr_msb_ns;	/* 0x168 */
	uint32 PAD[3];
	uint32 idm_errmisc0_ns;		/* 0x178 */
	uint32 idm_errmisc1_ns;		/* 0x17C */
	uint32 PAD[1];
	uint32 idm_access_status_ns;	/* 0x184 */
	uint32 idm_access_readid_ns;	/* 0x188 */
	uint32 idm_access_writeid_ns;	/* 0x18C */
	uint32 PAD[1];
	uint32 idm_reset_status_ns;	/* 0x194 */
	uint32 idm_reset_readid_ns;	/* 0x198 */
	uint32 idm_reset_writeid_ns;	/* 0x19C */
	uint32 PAD[2];
	uint32 idm_interrupt_status_ns;	/* 0x1A8 */
	uint32 idm_interrupt_mask_ns;	/* 0x1AC */
} idm_regs_t;

/**
 * @struct asni_regs
 * @brief AXI Slave Network Interface registers
 */
typedef volatile struct asni_regs {
	uint32 node_type;		/* 0x000 */
	uint32 node_info;		/* 0x004 */
	uint32 secr_acc;		/* 0x008 */
	uint32 pmusela;			/* 0x00c */
	uint32 pmuselb;			/* 0x010 */
	uint32 PAD[11];
	uint32 node_feat;		/* 0x040 */
	uint32 bursplt;			/* 0x044 */
	uint32 addr_remap;		/* 0x048 */
	uint32 PAD[13];
	uint32 sildbg;			/* 0x080 */
	uint32 qosctl;			/* 0x084 */
	uint32 wdatthrs;		/* 0x088 */
	uint32 arqosovr;		/* 0x08c */
	uint32 awqosovr;		/* 0x090 */
	uint32 atqosot;			/* 0x094 */
	uint32 arqosot;			/* 0x098 */
	uint32 awqosot;			/* 0x09c */
	uint32 axqosot;			/* 0x0a0 */
	uint32 qosrdpk;			/* 0x0a4 */
	uint32 qosrdbur;		/* 0x0a8 */
	uint32 qosrdavg;		/* 0x0ac */
	uint32 qoswrpk;			/* 0x0b0 */
	uint32 qoswrbur;		/* 0x0b4 */
	uint32 qoswravg;		/* 0x0b8 */
	uint32 qoscompk;		/* 0x0bc */
	uint32 qoscombur;		/* 0x0c0 */
	uint32 qoscomavg;		/* 0x0c4 */
	uint32 qosrbbqv;		/* 0x0c8 */
	uint32 qoswrbqv;		/* 0x0cc */
	uint32 qoscombqv;		/* 0x0d0 */
	uint32 PAD[11];
	idm_regs_t ni;			/* 0x100 - 0x1ac */
} asni_regs_t;

/**
 * @struct amni_regs
 * @brief AXI Master Network Interface registers
 */
typedef volatile struct amni_regs {
	uint32 node_type;		/* 0x000 */
	uint32 node_info;		/* 0x004 */
	uint32 secr_acc;		/* 0x008 */
	uint32 pmusela;			/* 0x00c */
	uint32 pmuselb;			/* 0x010 */
	uint32 PAD[11];
	uint32 node_feat;		/* 0x040 */
	uint32 PAD[15];
	uint32 sildbg;			/* 0x080 */
	uint32 qosacc;			/* 0x084 */
	uint32 PAD[26];
	uint32 interrupt_status;	/* 0x0f0 */
	uint32 interrupt_mask;		/* 0x0f4 */
	uint32 interrupt_status_ns;	/* 0x0f8 */
	uint32 interrupt_mask_ns;	/* 0x0FC */
	idm_regs_t ni;			/* 0x100 - 0x1ac */
} amni_regs_t;

/**
 * @struct nci_error_container
 * @brief A container for pointers to last error ints.
 */
typedef struct nci_error_container {
	uint32 error_log_status;      /**< The axi error log status. */
	uint32 errlog_lo;             /**< The axi errlog lo. */
	uint32 errlog_hi;             /**< The axi errlog hi. */
	uint32 errlog_id;             /**< The axi errlog id. */
	uint32 errlog_trans_sts;      /**< The axi errlog trans sts. */
	uint32 idm_access_status;     /**< The idm access status. */
	uint32 idm_access_readid;     /**< The idm read id. */
	uint32 idm_access_writeid;    /**< The idm write id. */
	uint32 idm_reset_status;      /**< The idm reset status. */
	uint32 idm_interrupt_status;  /**< The idm interrupt status. */
	uint32 wrapper_addr;          /**< The address of the wrapper that had an error. */
} nci_error_container_t;

/**
 * @struct nci_error_logger_container
 * @brief A container for arguments passed to nci_error_logger.
 */
typedef struct nci_error_logger_container {
	void *osh;         /**< OS handle. */
	uint32 wrapper;     /**< AXI wrapper address for this error. */
	volatile idm_regs_t *idm_regs;  /**< Pointer to the container with e error information. */
	nci_error_container_t *error_cont;  /**< Pointer to error container. */
	uint32 error_sts;  /**< error_sts from the idm register. */
	uint16 err_type;   /**< An err_type from the macro list above. */
	bool is_master;    /**< True if this error is from a master wrapper, false otherwise. */
	bool is_secure;    /**< True if the error is from secure idm registers, false otherwise. */
} nci_error_logger_container_t;

/* Helper for logging parsed errors. */
void nci_error_logger(nci_error_logger_container_t *cont);
/* Helper for reading nci errors from a wrapper. */
uint32 nci_error_helper(void *osh, void *wrapper, bool is_master,
	nci_error_container_t *error_cont, uint32 wrapper_daddr);

#endif  /* _hal_nci_cmn_h_ */
