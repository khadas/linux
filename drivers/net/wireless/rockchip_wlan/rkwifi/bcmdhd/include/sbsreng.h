/*
 * Header file for save-restore HW
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

#ifndef _sbsreng_h_
#define _sbsreng_h_

#include <typedefs.h>

/* Include Regs from vlsi_xxx files only for Dongle FW builds */
#if defined(DONGLEBUILD)
#include <vlsi_sr_eng_all_regs.h>
#endif /* DONGLEBUILD */

/* For DHD builds define only those registers that needs to be accessed from Host */
#if !defined(DONGLEBUILD)
#define sr_eng_control_reg1_ADDR					0x504u
#define sr_eng_Capabilities_ADDR					0x00u
#define sr_eng_SrControl0_ADDR						0x04u
#define sr_eng_SrControl1_ADDR						0x08u
#define sr_eng_GpioControl_ADDR						0x0Cu
#define sr_eng_Status0_ADDR						0x10u
#define sr_eng_Status1_ADDR						0x14u
#define sr_eng_IllegalInstrAddr_ADDR					0x20u
#define sr_eng_IllegalInstrData0_ADDR					0x24u
#define sr_eng_IllegalInstrData1_ADDR					0x28u
#define sr_eng_Status2_ADDR						0x2Cu
#define sr_eng_SrControl2_ADDR						0x30u
#define sr_eng_PowerDomainInfoReg_ADDR					0x34u
#define sr_eng_DataStartAddr0_ADDR					0x38u
#define sr_eng_DataStartAddr1_ADDR					0x3Cu
#define sr_eng_SRIntStatus_ADDR						0x40u
#define sr_eng_SRIntMask_ADDR						0x44u
#define sr_eng_Status1_ADDR						0x14u
#define INVALID_ADDRESS_sr_eng						0xffffu
#define sr_eng_DataStartAddr_ADDR					INVALID_ADDRESS_sr_eng

#define SR_ENG_MAX_UNITS						4u
#endif /* !DONGLEBUILD */

typedef volatile struct srregs srregs_t;

#define SR_ENG_REG_OFF(regname) \
	sr_eng_##regname##_ADDR

#define SR_ENG_REG_FIELD_MASK(regname, regfield) \
	sr_eng_##regname##__##regfield##_MASK
#define SR_ENG_REG_FIELD_SHIFT(regname, regfield) \
	sr_eng_##regname##__##regfield##_SHIFT

#define SR_ENG_REG_ADDR(regbase, regname) \
	((volatile uint32 *)((uintptr)(regbase) + SR_ENG_REG_OFF(regname)))

/* For some sr_eng revs (e.g. 12) a register may have changed just its name, so to avoid invalid
 * accesses, map the register's address to the changed name.
 */
#if (sr_eng_DataStartAddr_ADDR == INVALID_ADDRESS_sr_eng)
#undef sr_eng_DataStartAddr_ADDR
#define sr_eng_DataStartAddr_ADDR sr_eng_DataStartAddr0_ADDR
#endif /* DataStartAddr invalid */

/* Force a compile error if any register is referenced that does not exist in the built sr_eng's
 * register set.
 */
#undef INVALID_ADDRESS_sr_eng
#define INVALID_ADDRESS_sr_eng hnd_invalid_reg_sr_eng()
#undef INVALID_SHIFT_sr_eng
#define INVALID_SHIFT_sr_eng hnd_invalid_reg_sr_eng()

#endif /* _sbsreng_h_ */
