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
 * @brief Implementations for shared code between EROM dependent and vlsi dependent NCI
 * implementations.
 */
#include <typedefs.h>
#include <bcmdefs.h>
#include <bcmutils.h>
#include <osl.h>
#include <aidmp.h>  /* for AXI_WRAP_STS_DECODE_ERR */
#include <hal_nci_cmn.h>

/**
 * Helper for logging parsed errors.
 * @param[in,out]  cont  See documentation for struct nci_error_logger_container. This fills the
 *                       last_errors container.
 */
void
BCMPOSTTRAPFN(nci_error_logger)(nci_error_logger_container_t *cont)
{
	void *osh = cont->osh;
	volatile idm_regs_t *idm_regs = cont->idm_regs;

	cont->error_cont->error_log_status = cont->error_sts;
	if (cont->is_secure) {
		cont->error_cont->errlog_lo = R_REG(osh, &idm_regs->idm_erraddr_lsb);
		cont->error_cont->errlog_hi = R_REG(osh, &idm_regs->idm_erraddr_msb);
		cont->error_cont->errlog_id = R_REG(osh, &idm_regs->idm_errmisc0);
		cont->error_cont->errlog_trans_sts = R_REG(osh, &idm_regs->idm_errmisc1);
		cont->error_cont->idm_access_status = idm_regs->idm_access_status;
		cont->error_cont->idm_access_readid = idm_regs->idm_access_readid;
		cont->error_cont->idm_access_writeid = idm_regs->idm_access_writeid;
		cont->error_cont->idm_reset_status = idm_regs->idm_reset_status;
		cont->error_cont->idm_interrupt_status = idm_regs->idm_interrupt_status;
	} else {
		cont->error_cont->errlog_lo = R_REG(osh, &idm_regs->idm_erraddr_lsb_ns);
		cont->error_cont->errlog_hi = R_REG(osh, &idm_regs->idm_erraddr_msb_ns);
		cont->error_cont->errlog_id = R_REG(osh, &idm_regs->idm_errmisc0_ns);
		cont->error_cont->errlog_trans_sts = R_REG(osh, &idm_regs->idm_errmisc1_ns);
		cont->error_cont->idm_access_status = idm_regs->idm_access_status_ns;
		cont->error_cont->idm_access_readid = idm_regs->idm_access_readid_ns;
		cont->error_cont->idm_access_writeid = idm_regs->idm_access_writeid_ns;
		cont->error_cont->idm_reset_status = idm_regs->idm_reset_status_ns;
		cont->error_cont->idm_interrupt_status = idm_regs->idm_interrupt_status_ns;
	}

	posttrap_printf("%s on wrapper 0x%x: %sSecure %s\n",
			nci_axi_err_str[cont->err_type],
			cont->wrapper,
			(cont->is_secure ? "" : "Non-"),
			(cont->is_master ? "master (ASNI)" : "slave (xMNI)"));
	posttrap_printf("errorstatus 0x%x addr MSB 0x%x LSB 0x%x\n",
			cont->error_sts,
			cont->error_cont->errlog_hi,
			cont->error_cont->errlog_lo);
	posttrap_printf("errmisc0 0x%x errmisc1 0x%x \n",
			cont->error_cont->errlog_id,
			cont->error_cont->errlog_trans_sts);
	posttrap_printf("access_control 0x%x access_status 0x%x interrupt_status 0x%x \n",
			R_REG(osh, &cont->error_cont->idm_access_status),
			R_REG(osh, &idm_regs->idm_access_control),
			R_REG(osh, &cont->error_cont->idm_interrupt_status));
	posttrap_printf("access_readid 0x%x access_writeid 0x%x reset_control 0x%x "
			"reset_status 0x%x\n",
			R_REG(osh, &cont->error_cont->idm_access_readid),
			R_REG(osh, &cont->error_cont->idm_access_writeid),
			R_REG(osh, &idm_regs->idm_reset_control),
			R_REG(osh, &cont->error_cont->idm_reset_status));
}

/**
 * Helper for reading nci errors from a wrapper.
 * @param[in]      osh         OS handle for register operations, which is NULL for dongles using
 *                             vlsi_data to replace the EROM.
 * @param[in]      wrapper     Pointer whose value is the AXI wrapper address to query for errors.
 * @param[in]      is_master   True if this is a master(amni) wrapper, false otherwise.
 * @param[in,out]  error_cont  A container to store error information if it is found.
 * @return                     The value of the error if one was found, 0u otherwise.
 */
uint32
BCMPOSTTRAPFN(nci_error_helper)(void *osh, void *wrapper, bool is_master,
		nci_error_container_t *error_cont, uint32 wrapper_daddr)
{
	uint32 error_clr;
	uint32 reg_read;
	uint32 ret = 0u;
	uint err_type = 0u;
	uint err_type_ns = 0u;
	bool internal_timeout = FALSE;
	volatile idm_regs_t *idm_regs = is_master ?
		&((amni_regs_t *)wrapper)->ni :
		&((asni_regs_t *)wrapper)->ni;
	uint32 error_sts = R_REG(osh, &idm_regs->idm_errstatus);
	uint32 error_sts_ns = R_REG(osh, &idm_regs->idm_errstatus_ns);

	if ((error_sts & IDM_ERRSTATUS_ERROR_VALID) == 0u &&
			(error_sts_ns & IDM_ERRSTATUS_ERROR_VALID) == 0u) {
		/* No error */
		goto end;
	}

	error_cont->wrapper_addr = wrapper_daddr;

	switch (error_sts & IDM_ERRSTATUS_ERROR_CODE_MASK) {
	case IDM_ERRSTATUS_ILLEGAL_ADDR_DECODE:
		ret |= AXI_WRAP_STS_DECODE_ERR;
		err_type = NCI_TYPE_STR_DECODE_ERR;
		break;
	case IDM_ERRSTATUS_SLAVE_ERROR_RESP:
		ret |= AXI_WRAP_STS_SLAVE_ERR;
		err_type = NCI_TYPE_STR_SLAVE_ERR;
		break;
	case IDM_ERRSTATUS_INTERNAL_TIMEOUT:
		ret |= AXI_WRAP_STS_TIMEOUT;
		internal_timeout = TRUE;
		err_type = NCI_TYPE_STR_TIMEOUT;
		break;
	}
	switch (error_sts_ns & IDM_ERRSTATUS_ERROR_CODE_MASK) {
	case IDM_ERRSTATUS_ILLEGAL_ADDR_DECODE:
		ret |= (AXI_WRAP_STS_DECODE_ERR << AXI_WRAP_STS_NONSECURE_SHIFT);
		err_type_ns = NCI_TYPE_STR_DECODE_ERR;
		break;
	case IDM_ERRSTATUS_SLAVE_ERROR_RESP:
		ret |= (AXI_WRAP_STS_SLAVE_ERR << AXI_WRAP_STS_NONSECURE_SHIFT);
		err_type_ns = NCI_TYPE_STR_SLAVE_ERR;
		break;
	case IDM_ERRSTATUS_INTERNAL_TIMEOUT:
		ret |= (AXI_WRAP_STS_TIMEOUT << AXI_WRAP_STS_NONSECURE_SHIFT);
		err_type_ns = NCI_TYPE_STR_TIMEOUT;
		internal_timeout = TRUE;
		break;
	}

	if (error_sts & IDM_ERRSTATUS_ERROR_VALID) {
		nci_error_logger_container_t logger_container = {
			.osh = osh,
			.wrapper = wrapper_daddr,
			.idm_regs = idm_regs,
			.error_cont = error_cont,
			.error_sts = error_sts,
			.err_type = err_type,
			.is_master = is_master,
			.is_secure = TRUE,
		};
		nci_error_logger(&logger_container);
	}

	if (error_sts_ns & IDM_ERRSTATUS_ERROR_VALID) {
		nci_error_logger_container_t logger_container = {
			.osh = osh,
			.wrapper = wrapper_daddr,
			.idm_regs = idm_regs,
			.error_cont = error_cont,
			.error_sts = error_sts_ns,
			.err_type = err_type_ns,
			.is_master = is_master,
			.is_secure = FALSE,
		};
		nci_error_logger(&logger_container);
	}

	/* We are in error handler. Disable error detection before clearing error. */
	W_REG(osh, &idm_regs->idm_errctlr, 0u);

	/* Clear the error. The below bits are write 1 to clear. */
	error_clr = error_sts & (IDM_ERRSTATUS_ERROR_VALID|IDM_ERRSTATUS_ERR_ADDR_VALID|
			IDM_ERRSTATUS_UNCORRECTED_ERR|IDM_ERRSTATUS_SECOND_ERR|
			IDM_ERRSTATUS_MISC_ERROR);
	if (error_clr) {
		uint32 int_status;
		W_REG(osh, &idm_regs->idm_errstatus, error_clr);
		/* Clear interrupt status */
		int_status = R_REG(osh, &idm_regs->idm_interrupt_status);
		W_REG(osh, &idm_regs->idm_interrupt_status, int_status);
	}
	/* Clear the ns error. The below bits are write 1 to clear. */
	error_clr = error_sts_ns & (IDM_ERRSTATUS_ERROR_VALID|IDM_ERRSTATUS_ERR_ADDR_VALID|
			IDM_ERRSTATUS_UNCORRECTED_ERR|IDM_ERRSTATUS_SECOND_ERR|
			IDM_ERRSTATUS_MISC_ERROR);

	if (error_clr) {
		uint32 int_status;
		W_REG(osh, &idm_regs->idm_errstatus_ns, error_clr);
		/* Clear interrupt status */
		int_status = R_REG(osh, &idm_regs->idm_interrupt_status_ns);
		W_REG(osh, &idm_regs->idm_interrupt_status_ns, int_status);
	}

	/* if its an internal timeout exit recovery mode */
	if (internal_timeout) {
		AND_REG(osh, &idm_regs->idm_reset_control, ~NI_IDM_RESET_ENTRY);
		/* Wait for the exit of recovery */
		SPINWAIT_TRAP((reg_read = R_REG(osh, &idm_regs->idm_reset_control) &
			NI_IDM_RESET_ENTRY), NCI_SPINWAIT_TIMEOUT);
	}

	/* Enable back the error detection */
	W_REG(osh, &idm_regs->idm_errctlr,
		IDM_ERRCTLR_BUS | IDM_ERRCTLR_ENABLE | IDM_ERRCTLR_UE);

end:
	return ret;
}
