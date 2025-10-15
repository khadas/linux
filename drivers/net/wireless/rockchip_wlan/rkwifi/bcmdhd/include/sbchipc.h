/*
 * SiliconBackplane Chipcommon core hardware definitions.
 *
 * The chipcommon core provides chip identification, SB control,
 * JTAG, 0/1/2 UARTs, clock frequency control, a watchdog interrupt timer,
 * GPIO interface, extbus, and support for serial and parallel flashes.
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

#ifndef	_SBCHIPC_H
#define	_SBCHIPC_H

#if !defined(_LANGUAGE_ASSEMBLY) && !defined(__ASSEMBLY__)

#include <typedefs.h>

#define BCM_MASK32(msb, lsb)	((~0u >> (32u - (msb) - 1u)) & (~0u << (lsb)))

/* Include Regs from vlsi_xxx files only for Dongle FW builds */
#if defined(DONGLEBUILD) || defined(COEX_CPU_BUILD)
#include <vlsi_pmu_all_regs.h>

#ifdef VLSI_CTRL_REGS
#include <vlsi_control_regs.h>
#endif /* VLSI_CTRL_REGS */

#include <vlsi_pmu_resources.h>
#endif /* DONGLEBUILD || COEX_CPU_BUILD */

typedef volatile struct pmuregs pmuregs_t;

/**
 * A set of PMU registers is clocked in the ILP domain, which has an implication on register write
 * behavior: if such a register is written, it takes multiple ILP clocks for the PMU block to absorb
 * the write. During that time the 'SlowWritePending' bit in the PMUStatus register is set.
 */
#define PMUREGS_ILP_SENSITIVE(regoff) \
	((regoff) == PMU_REG_OFF(PMUTimer) ||	\
	 (regoff) == PMU_REG_OFF(PmuWatchdogCounter) || \
	 (regoff) == PMU_REG_OFF(ResourceReqTimer0))

/* Wait up to ten seconds for ILP sensitive PMU writes */
#define PMU_ILP_WAIT_TIME 10000000u

#define PMU_REG_OFF(regname) \
	pmu_##regname##_ADDR

#define PMU_REG_FIELD_MASK(regname, regfield) \
	pmu_##regname##__##regfield##_MASK
#define PMU_REG_FIELD_SHIFT(regname, regfield) \
	pmu_##regname##__##regfield##_SHIFT

#define PMU_REG_ADDR(regbase, regname) \
	(volatile uint32 *)((uintptr)(regbase) + PMU_REG_OFF(regname))

/* Force a compile error if any register is referenced that does not exist in the built pmu's
 * register set.
 */
#undef INVALID_ADDRESS_pmu
#define INVALID_ADDRESS_pmu hnd_invalid_reg_pmu()
#undef INVALID_SHIFT_pmu
#define INVALID_SHIFT_pmu hnd_invalid_reg_pmu()

/* registers missing from the new vlsi_pmu_all_regs.h */
/* NOTE: opportunity to remove related code since these registers are old */
#define pmu_ResourceReqTimerSel_ADDR		0x640U

/* For DHD builds define only those registers that needs to be accessed from Host */
#if !defined(DONGLEBUILD) && !defined(COEX_CPU_BUILD)
#define pmu_RsrcState_ADDR                                                              0x60cu
#define pmu_PMUTimer_ADDR                                                               0x614u
#define pmu_MinResourceMask_ADDR                                                        0x618u
#define pmu_MaxResourceMask_ADDR                                                        0x61cu
#define pmu_CoreCapabilitiesExt_ADDR                                                    0x64cu
#define pmu_ChipControlAddr_ADDR                                                        0x650u
#define pmu_ChipControlData_ADDR                                                        0x654u
#define pmu_XtalFreqRatio_ADDR                                                          0x66cu
#define pmu_RetentionControl_ADDR                                                       0x670u
#define pmu_SWScratch_ADDR                                                              0x750u
#define pmu_FISCtrlStatus_ADDR                                                          0x7b4u
#define pmu_FISTrigRsrcState_ADDR                                                       0x7bcu

#define pmu_CoreCapabilities_ADDR                                                       0x604u
#define pmu_RegulatorControlAddr_ADDR                                                   0x658u
#define pmu_RegulatorControlData_ADDR                                                   0x65cu
#define pmu_PmuWatchdogCounter_ADDR                                                     0x634u
#define pmu_PLLControlAddr_ADDR                                                         0x660u
#define pmu_ResourceReqTimer0_ADDR                                                      0x644u
#define pmu_PMUStatus_ADDR                                                              0x608u
#define pmu_PLLControlData_ADDR                                                         0x664u
#define pmu_PMUControlExt_ADDR                                                          0x6d8u
#define pmu_WatchdogRsrcMask_ADDR                                                       0x748u
#define pmu_PMUStatus_ADDR                                                              0x608u
#define pmu_PMUControlExt_ADDR                                                          0x6d8u
#define pmu_PmuWatchdogCounter_ADDR                                                     0x634u
#define pmu_ResourceReqTimer0_ADDR                                                      0x644u
#define pmu_FISMinRsrcMask_ADDR                                                         0x7b8u

#endif /* !DONGLEBUILD && !COEX_CPU_BUILD */

/* Include Regs from vlsi_xxx files only for Dongle FW builds */
#if defined(DONGLEBUILD) || defined(COEX_CPU_BUILD)
#include <vlsi_chipcommon_all_regs.h>

/**
 * Get the width of a field in a register.
 * @param  _regname    The name register that the field is defined in.
 * @param  _fieldname  The name of the field.
 * @return             An unsigned integer.
 */
#define CC_REG_FIELD_WIDTH(_regname, _fieldname) \
	(chipcommon_## _regname ##__## _fieldname ##_WIDTH)
#endif /* DONGLEBUILD || COEX_CPU_BUILD */

typedef volatile struct chipcregs chipcregs_t;

#define CC_REG_OFF(regname) \
	chipcommon_##regname##_ADDR

#define CC_REG_FIELD_MASK(regname, regfield) \
	chipcommon_##regname##__##regfield##_MASK
#define CC_REG_FIELD_SHIFT(regname, regfield) \
	chipcommon_##regname##__##regfield##_SHIFT

#define CC_REG_ADDR(regbase, regname) \
	(volatile uint32 *)((uintptr)(regbase) + CC_REG_OFF(regname))

/* registers missing from the new vlsi_chipcommon_all_regs.h */
/* NOTE: opportunity to remove related code since these registers are old */
#define chipcommon_corecontrol_ADDR		0x8U
#define chipcommon_broadcastaddress_ADDR	0x50U
#define chipcommon_broadcastdata_ADDR		0x54U
#define chipcommon_clockcontrol_n_ADDR		0x90U
#define chipcommon_clockcontrol_sb_ADDR		0x94U
#define chipcommon_clockcontrol_m2_ADDR		0x9cU
#define chipcommon_clockcontrol_m3_ADDR		0xa0U
#define chipcommon_pll_on_delay_ADDR		0xb0U
#define chipcommon_fref_sel_delay_ADDR		0xb4U
#define chipcommon_slow_clk_ctl_ADDR		0xb8U
#define chipcommon_system_clk_ctl_ADDR		0xc0U
#define chipcommon_clkstatestretch_ADDR		0xc4U
#define chipcommon_uart0data_ADDR		0x300U
#define chipcommon_uart0mcr_ADDR		0x310U
#define chipcommon_rng_ctrl_0_ADDR		0x3c0U
#define chipcommon_rng_rng_soft_reset_ADDR	0x3c4U
#define chipcommon_rng_rbg_soft_reset_ADDR	0x3c8U
#define chipcommon_rng_total_bit_cnt_ADDR	0x3ccU
#define chipcommon_rng_total_bit_thrshld_ADDR	0x3d0U
#define chipcommon_rng_rev_id_ADDR		0x3d4U
#define chipcommon_rng_int_status_0_ADDR	0x3d8U
#define chipcommon_rng_int_enable_0_ADDR	0x3dcU
#define chipcommon_rng_fifo_data_ADDR		0x3e0U
#define chipcommon_rng_fifo_cnt_ADDR		0x3e4U
#define chipcommon_rng_int_status_0_ADDR	0x3d8U
#define chipcommon_sr1_control0_ADDR		0x584U
#define chipcommon_sr1_control1_ADDR		0x588U
#define chipcommon_sromotp_ADDR			0x800U

/* For DHD builds define only those registers that needs to be accessed from Host */
#if !defined(DONGLEBUILD) && !defined(COEX_CPU_BUILD)
#define chipcommon_ChipID_ADDR                                                          0x0u
#define chipcommon_CoreCapabilities_ADDR                                                0x4u
#define chipcommon_PowerControl_ADDR                                                    0x1e8u
#define chipcommon_WorkAround_ADDR							0x1e4u
#define chipcommon_SpromCtrl_ADDR                                                       0x190u
#define chipcommon_SpromAddress_ADDR                                                    0x194u
#define chipcommon_SpromData_ADDR                                                       0x198u
#define chipcommon_SRMemRWAddr_ADDR                                                     0x4d0u
#define chipcommon_SRMemRWData_ADDR                                                     0x4d4u

#define chipcommon_ChipStatus_ADDR                                                      0x2cu
#define chipcommon_CapabilitiesExtension_ADDR                                           0xacu
#define chipcommon_WatchdogCounter_ADDR                                                 0x80u
#define chipcommon_EromPtrOffset_ADDR                                                   0xfcu
#define chipcommon_IntStatus_ADDR                                                       0x20u
#define chipcommon_ClkDiv2_ADDR                                                         0xf0u
#define chipcommon_EromPtrOffset_ADDR                                                   0xfcu
#define chipcommon_ClockCtlStatus_ADDR                                                  0x1e0u
#define chipcommon_BackplaneAddrLow_ADDR                                                0xd0u
#define chipcommon_BackplaneAddrHi_ADDR                                                 0xd4u
#define chipcommon_BackplaneData_ADDR                                                   0xd8u
#define chipcommon_BackplaneIndAccess_ADDR                                              0xe0u
#define chipcommon_WatchdogCounter_ADDR                                                 0x80u
#define chipcommon_IntMask_ADDR                                                         0x24u
#define chipcommon_ChipControl_ADDR                                                     0x28u
#define chipcommon_GPIOPullup_ADDR                                                      0x58u
#define chipcommon_GPIOPulldown_ADDR                                                    0x5cu
#define chipcommon_GPIOCtrl_ADDR                                                        0x6cu
#define chipcommon_GPIOOutputEn_ADDR                                                    0x68u
#define chipcommon_GPIOOutput_ADDR                                                      0x64u
#define chipcommon_GPIOInput_ADDR                                                       0x60u
#define chipcommon_GPIOIntPolarity_ADDR                                                 0x70u
#define chipcommon_GPIOIntMask_ADDR                                                     0x74u
#define chipcommon_GPIOEventIntMask_ADDR                                                0x7cu
#define chipcommon_GPIOPullup_ADDR                                                      0x58u
#define chipcommon_GPIOEventIntMask_ADDR                                                0x7cu
#define chipcommon_GPIOEventIntPolarity_ADDR                                            0x84u
#define chipcommon_GPIOEvent_ADDR                                                       0x78u
#define chipcommon_JtagMasterCtrl_ADDR							0x3cu
#endif /* !DONGLEBUILD && !COEX_CPU_BUILD */

#ifndef USE_NEW_GCI_REG_OFF
/* registers missing from the new vlsi_chipcommon_all_regs.h */
/* NOTE: OTP registers are moved to GCI core */
#define chipcommon_otpstatus_ADDR		0x10U
#define chipcommon_otpcontrol_ADDR		0x14U
#define chipcommon_otpprog_ADDR			0x18U
#define chipcommon_otplayout_ADDR		0x1cU
#define chipcommon_otplayoutextension_ADDR	0x4cU
#define chipcommon_otpcontrol1_ADDR		0xf4U
#define chipcommon_gci_corectrl_ADDR		0xc0cU
#define chipcommon_gci_seciauxtx_ADDR		0xda0U
#endif /* USE_NEW_GCI_REG_OFF */

#else

/* Use these in assembly files only */
#define	CC_CHIPID		0
#define CC_BP_ADRLOW            0xd0
#define CC_BP_ADRHI             0xd4
#define	CC_EROMPTR		0xfc
#define CC_SCR_DHD_TO_BL        CC_BP_ADRHI
#define CC_SCR_BL_TO_DHD        CC_BP_ADRLOW

#endif /* !_LANGUAGE_ASSEMBLY && !__ASSEMBLY__ */

#define GPIO_SEL_0	0x00001111
#define GPIO_SEL_1	0x11110000
#define GPIO_SEL_8	0x00001111
#define GPIO_SEL_9	0x11110000

#if defined(VLSI_CTRL_REGS) && defined(SKIP_LEGACY_CC_API)
#define CHIPCTRLREG0	SI_CTRLREGS_INVALID
#define CHIPCTRLREG1	SI_CTRLREGS_INVALID
#define CHIPCTRLREG2	SI_CTRLREGS_INVALID
#define CHIPCTRLREG3	SI_CTRLREGS_INVALID
#define CHIPCTRLREG4	SI_CTRLREGS_INVALID
#define CHIPCTRLREG5	SI_CTRLREGS_INVALID
#define CHIPCTRLREG6	SI_CTRLREGS_INVALID
#define CHIPCTRLREG13	SI_CTRLREGS_INVALID
#define CHIPCTRLREG16	SI_CTRLREGS_INVALID
#else /* !VLSI_CTRL_REGS || !SKIP_LEGACY_CC_API */
#define CHIPCTRLREG0	0x0u
#define CHIPCTRLREG1	0x1u
#define CHIPCTRLREG2	0x2u
#define CHIPCTRLREG3	0x3u
#define CHIPCTRLREG4	0x4u
#define CHIPCTRLREG5	0x5u
#define CHIPCTRLREG6	0x6u
#define CHIPCTRLREG13	0xdu
#define CHIPCTRLREG16	0x10u
#endif /* !VLSI_CTRL_REGS || !SKIP_LEGACY_CC_API */

#define EXT_LPO_AVAIL	0x100
#define LPO_SEL		(1 << 0)
#define CC_EXT_LPO_PU 0x200000
#define GC_EXT_LPO_PU 0x2
#define CC_INT_LPO_PU 0x100000
#define GC_INT_LPO_PU 0x1
#define EXT_LPO_SEL 0x8
#define INT_LPO_SEL 0x4
#define ENABLE_FINE_CBUCK_CTRL			(1 << 30)
#define REGCTRL5_PWM_AUTO_CTRL_MASK		0x007e0000
#define REGCTRL5_PWM_AUTO_CTRL_SHIFT		17
#define REGCTRL6_PWM_AUTO_CTRL_MASK		0x3fff0000
#define REGCTRL6_PWM_AUTO_CTRL_SHIFT		16
#define CC_BP_IND_ACCESS_START_SHIFT		9
#define CC_BP_IND_ACCESS_START_MASK		(1 << CC_BP_IND_ACCESS_START_SHIFT)
#define CC_BP_IND_ACCESS_RDWR_SHIFT		8
#define CC_BP_IND_ACCESS_RDWR_MASK		(1 << CC_BP_IND_ACCESS_RDWR_SHIFT)
#define CC_BP_IND_ACCESS_ERROR_SHIFT		10
#define CC_BP_IND_ACCESS_ERROR_MASK		(1 << CC_BP_IND_ACCESS_ERROR_SHIFT)
#define GC_BT_CTRL_UARTPADS_OVRD_EN		(1u << 1)

#define LPO_SEL_TIMEOUT 1000

#define LPO_FINAL_SEL_SHIFT 18

#define LHL_LPO1_SEL 0
#define LHL_LPO2_SEL 0x1
#define LHL_32k_SEL 0x2
#define LHL_EXT_SEL  0x3

#define EXTLPO_BUF_PD	0x40
#define LPO1_PD_EN	0x1
#define LPO1_PD_SEL	0x6
#define LPO1_PD_SEL_VAL	0x4
#define LPO2_PD_EN	0x8
#define LPO2_PD_SEL	0x30
#define LPO2_PD_SEL_VAL	0x20
#define OSC_32k_PD	0x80

#define LHL_CLK_DET_CTL_AD_CNTR_CLK_SEL	0x3

/*
 * SW states for lpo select
 * All the states can be overidden except LHL_LPO_HWDEFAULT_NO_OVERRIDE
 * 1. PMU Status EXT_LPOAvail
 * 2. NVRAM override
 */
#define LHL_LPO_AUTO		0x0	/* 0 - Auto remaps to OSC_32 */
#define LHL_LPO1_ENAB		0x1	/* 1 - Internal LPO 1 */
#define LHL_LPO2_ENAB		0x2	/* 2 - Internal LPO 2 */
#define LHL_OSC_32k_ENAB	0x3	/* 3 - OSC_32k */
#define LHL_EXT_LPO_ENAB	0x4	/* 4 - EXT LPO */
#define RADIO_LPO_ENAB		0x5	/* 5 - Used only for Power Down */
#define LHL_LPO_HWDEFAULT	0x6	/* 6 - Use HW default */

#define LHL_CLK_DET_CTL_ADR_LHL_CNTR_EN	0x4
#define LHL_CLK_DET_CTL_ADR_LHL_CNTR_CLR	0x8
#define LHL_CLK_DET_CNT		0xF0
#define LHL_CLK_DET_CNT_SHIFT   4
#define LPO_SEL_SHIFT		9

#define LHL_MAIN_CTL_ADR_FINAL_CLK_SEL	0x3C0000
#define LHL_MAIN_CTL_ADR_LHL_WLCLK_SEL	0x600

#define CLK_DET_CNT_THRESH	8

#ifdef SR_DEBUG
#define SUBCORE_POWER_ON 0x0001
#define PHY_POWER_ON 0x0010
#define VDDM_POWER_ON 0x0100
#define MEMLPLDO_POWER_ON 0x1000
#define SUBCORE_POWER_ON_CHK 0x00040000
#define PHY_POWER_ON_CHK 0x00080000
#define VDDM_POWER_ON_CHK 0x00100000
#define MEMLPLDO_POWER_ON_CHK 0x00200000
#endif /* SR_DEBUG */

#ifdef CCNFLASH_SUPPORT
/* NAND flash support */
#define CC_NAND_REVISION	0xC00
#define CC_NAND_CMD_START	0xC04
#define CC_NAND_CMD_ADDR	0xC0C
#define CC_NAND_SPARE_RD_0	0xC20
#define CC_NAND_SPARE_RD_4	0xC24
#define CC_NAND_SPARE_RD_8	0xC28
#define CC_NAND_SPARE_RD_C	0xC2C
#define CC_NAND_CONFIG		0xC48
#define CC_NAND_DEVID		0xC60
#define CC_NAND_DEVID_EXT	0xC64
#define CC_NAND_INTFC_STATUS	0xC6C
#endif /* CCNFLASH_SUPPORT */

/* chipid */
#define	CID_ID_MASK		0x0000ffff	/**< Chip Id mask */
#define	CID_REV_MASK		0x000f0000	/**< Chip Revision mask */
#define	CID_REV_SHIFT		16		/**< Chip Revision shift */
#define	CID_PKG_MASK		0x00f00000	/**< Package Option mask */
#define	CID_PKG_SHIFT		20		/**< Package Option shift */
#define	CID_CC_MASK		0x0f000000	/**< CoreCount (corerev >= 4) */
#define CID_CC_SHIFT		24
#define	CID_TYPE_MASK		0xf0000000	/**< Chip Type */
#define CID_TYPE_SHIFT		28

/* capabilities */
#define	CC_CAP_UARTS_MASK	0x00000003u	/**< Number of UARTs */
#define CC_CAP_MIPSEB		0x00000004u	/**< MIPS is in big-endian mode */
#define CC_CAP_UCLKSEL		0x00000018u	/**< UARTs clock select */
#define CC_CAP_UINTCLK		0x00000008u	/**< UARTs are driven by internal divided clock */
#define CC_CAP_UARTGPIO		0x00000020u	/**< UARTs own GPIOs 15:12 */
#define CC_CAP_EXTBUS_MASK	0x000000c0u	/**< External bus mask */
#define CC_CAP_EXTBUS_NONE	0x00000000u	/**< No ExtBus present */
#define CC_CAP_EXTBUS_FULL	0x00000040u	/**< ExtBus: PCMCIA, IDE & Prog */
#define CC_CAP_EXTBUS_PROG	0x00000080u	/**< ExtBus: ProgIf only */
#define	CC_CAP_FLASH_MASK	0x00000700u	/**< Type of flash */
#define	CC_CAP_PLL_MASK		0x00038000u	/**< Type of PLL */
#define CC_CAP_PWR_CTL		0x00040000u	/**< Power control */
#define CC_CAP_OTPSIZE		0x00380000u	/**< OTP Size (0 = none) */
#define CC_CAP_OTPSIZE_SHIFT	19		/**< OTP Size shift */
#define CC_CAP_OTPSIZE_BASE	5		/**< OTP Size base */
#define CC_CAP_JTAGP		0x00400000u	/**< JTAG Master Present */
#define CC_CAP_ROM		0x00800000u	/**< Internal boot rom active */
#define CC_CAP_BKPLN64		0x08000000u	/**< 64-bit backplane */
#define	CC_CAP_PMU		0x10000000u	/**< PMU Present, rev >= 20 */
#define	CC_CAP_ECI		0x20000000u	/**< ECI Present, rev >= 21 */
#define	CC_CAP_SROM		0x40000000u	/**< Srom Present, rev >= 32 */
#define	CC_CAP_NFLASH		0x80000000u	/**< Nand flash present, rev >= 35 */

#define	CC_CAP2_SECI		0x00000001u	/**< SECI Present, rev >= 36 */
#define	CC_CAP2_GSIO		0x00000002u	/**< GSIO (spi/i2c) present, rev >= 37 */

/* capabilities extension */
#define CC_CAP_EXT_SECI_PRESENT			0x00000001u	/**< SECI present */
#define CC_CAP_EXT_GSIO_PRESENT			0x00000002u	/**< GSIO present */
#define CC_CAP_EXT_GCI_PRESENT			0x00000004u	/**< GCI present */
#define CC_CAP_EXT_SECI_PUART_PRESENT		0x00000008u	/**< UART present */
#define CC_CAP_EXT_AOB_PRESENT			0x00000040u	/**< AOB present */
#define CC_CAP_EXT_SWD_PRESENT			0x00000400u	/**< SWD present */
#define CC_CAP_SR_AON_PRESENT			0x0001E000u	/**< SWD present */
#define CC_CAP_EXT1_DVFS_PRESENT		0x00001000u	/**< DVFS present */

/* DVFS core count */
#define	CC_CAP_EXT1_CORE_CNT_SHIFT	(7u)
#define	CC_CAP_EXT1_CORE_CNT_MASK	((0x1Fu) << CC_CAP_EXT1_CORE_CNT_SHIFT)

/* SpmCtrl (Chipcommon Offset 0x690)
 * Bits 27:16 AlpDiv
 *   Clock divider control for dividing ALP or TCK clock
 *   (bit 8 determines ALP vs TCK)
 * Bits 8 UseDivTck
 *   See UseDivAlp (bit 1) for more details
 * Bits 7:6 DebugMuxSel
 *   Controls the debug mux for SpmDebug register
 * Bits 5 IntPending
 *   This field is set to 1 when any of the bits in IntHiStatus or IntLoStatus
 *   is set. It is automatically cleared after reading and clearing the
 *   IntHiStatus and IntLoStatus registers. This bit is Read only.
 * Bits 4 SpmIdle
 *   Indicates whether the spm controller is running (SpmIdle=0) or in idle
 *   state (SpmIdle=1); Note that after setting Spmen=1 (or 0), it takes a
 *   few clock cycles (ILP or divided ALP) for SpmIdle to go to 0 (or 1).
 *   This bit is Read only.
 * Bits 3 RoDisOutput
 *   Debug register - gate off all the SPM ring oscillator clock outputs
 * Bits 2 RstSpm
 *   Reset spm controller.
 *   Put spm in reset before changing UseDivAlp and AlpDiv
 * Bits 1 UseDivAlp
 *   This field, along with UseDivTck, selects the clock as the reference clock
 *   Bits [UseDivTck,UseDivAlp]:
 *   00 - Use ILP clock as reference clock
 *   01 - Use divided ALP clock
 *   10 - Use divided jtag TCK
 * Bits 0 Spmen
 *   0 - SPM disabled
 *   1 - SPM enabled
 *   Program all the SPM controls before enabling spm. For one-shot operation,
 *   SpmIdle indicates when the one-shot run has completed. After one-shot
 *   completion, spmen needs to be disabled first before enabling again.
 */
#define SPMCTRL_ALPDIV_FUNC	0x1ffu
#define SPMCTRL_ALPDIV_RO	0xfffu
#define SPMCTRL_ALPDIV_SHIFT	16u
#define SPMCTRL_ALPDIV_MASK	(0xfffu << SPMCTRL_ALPDIV_SHIFT)
#define SPMCTRL_RSTSPM		0x1u
#define SPMCTRL_RSTSPM_SHIFT	2u
#define SPMCTRL_RSTSPM_MASK	(0x1u << SPMCTRL_RSTSPM_SHIFT)
#define SPMCTRL_USEDIVALP	0x1u
#define SPMCTRL_USEDIVALP_SHIFT	1u
#define SPMCTRL_USEDIVALP_MASK	(0x1u << SPMCTRL_USEDIVALP_SHIFT)
#define SPMCTRL_SPMEN		0x1u
#define SPMCTRL_SPMEN_SHIFT	0u
#define SPMCTRL_SPMEN_MASK	(0x1u << SPMCTRL_SPMEN_SHIFT)

/* SpmClkCtrl (Chipcommon Offset 0x698)
 * Bits 31 OneShot
 *   0 means Take periodic measurements based on IntervalValue
 *   1 means Take a one shot measurement
 *   when OneShot=1 IntervalValue determines the amount of time to wait before
 *   taking the measurement
 * Bits 30:28 ROClkprediv1
 *    ROClkprediv1 and ROClkprediv2 controls the clock dividers of the RO clk
 *    before it goes to the monitor
 *    The RO clk goes through prediv1, followed by prediv2
 *    prediv1:
 *      0 - no divide
 *      1 - divide by 2
 *      2 - divide by 4
 *      3 - divide by 8
 *      4 - divide by 16
 *      5 - divide by 32
 *    prediv2:
 *      0 - no divide
 *      1 to 15 - divide by (prediv2+1)
 */
#define SPMCLKCTRL_SAMPLETIME		0x2u
#define SPMCLKCTRL_SAMPLETIME_SHIFT	24u
#define SPMCLKCTRL_SAMPLETIME_MASK	(0xfu << SPMCLKCTRL_SAMPLETIME_SHIFT)
#define SPMCLKCTRL_ONESHOT		0x1u
#define SPMCLKCTRL_ONESHOT_SHIFT	31u
#define SPMCLKCTRL_ONESHOT_MASK		(0x1u << SPMCLKCTRL_ONESHOT_SHIFT)

/* MonCtrlN (Chipcommon Offset 0x6a8)
 * Bits 15:8 TargetRo
 *   The target ring oscillator to observe
 * Bits 7:6 TargetRoExt
 *   Extended select option to choose the target clock to monitor;
 *   00 - selects ring oscillator clock;
 *   10 - selects functional clock;
 *   11 - selects DFT clocks;
 *   Bits 15:8 (TargetRO) is used to select the specific RO or functional or
 *   DFT clock
 * Bits 3 intHiEn
 *   Interrupt hi enable (MonEn should be 1)
 * Bits 2 intLoEn
 *   Interrupt hi enable (MonEn should be 1)
 * Bits 1 HwEnable
 *   TBD
 * Bits 0 MonEn
 *   Enable monitor, interrupt and watermark functions
 */
#define MONCTRLN_TARGETRO_PMU_ALP_CLK		0u
#define MONCTRLN_TARGETRO_PCIE_ALP_CLK		1u
#define MONCTRLN_TARGETRO_CB_BP_CLK		2u
#define MONCTRLN_TARGETRO_ARMCR4_CLK_4387B0	3u
#define MONCTRLN_TARGETRO_ARMCR4_CLK_4387C0	20u
#define MONCTRLN_TARGETRO_SHIFT			8u
#define MONCTRLN_TARGETRO_MASK			(0xffu << MONCTRLN_TARGETRO_SHIFT)
#define MONCTRLN_TARGETROMAX			64u
#define MONCTRLN_TARGETROHI			32u
#define MONCTRLN_TARGETROEXT_RO			0x0u
#define MONCTRLN_TARGETROEXT_FUNC		0x2u
#define MONCTRLN_TARGETROEXT_DFT		0x3u
#define MONCTRLN_TARGETROEXT_SHIFT		6u
#define MONCTRLN_TARGETROEXT_MASK		(0x3u << MONCTRLN_TARGETROEXT_SHIFT)
#define MONCTRLN_MONEN				0x1u
#define MONCTRLN_MONEN_SHIFT			0u
#define MONCTRLN_MONEN_MASK			(0x1u << MONCTRLN_MONENEXT_SHIFT)

/* 4388B0 RO index defines */
#define MONCTRLN_TARGETROMAX_CCREV_GE_72			(76u)
#define MONCTRLN_TARGETRO_CCREV_GE_72_BBPLL_CLKOUT_0		(2u)
#define MONCTRLN_TARGETRO_CCREV_GE_72_BBPLL_CLKOUT_1		(3u)
#define MONCTRLN_TARGETRO_CCREV_GE_72_BBPLL_CLKOUT_2		(4u)
#define MONCTRLN_TARGETRO_CCREV_GE_72_BBPLL_CLKOUT_3		(5u)
#define MONCTRLN_TARGETRO_CCREV_GE_72_BBPLL_CLKOUT_4		(6u)
#define MONCTRLN_TARGETRO_CCREV_GE_72_BBPLL_CLKOUT_5		(7u)
#define MONCTRLN_TARGETRO_CCREV_GE_72_ARMPLL_CLKOUT_DIV4	(8u)

/* 4397B0 RO Index defines */
#define MONCTRLN_TARGETROMAX_4397				(119u)
#define MONCTRLN_TARGETRO_4397_ARMPLL_CLKOUT_DIV8		(15u)
#define MONCTRLN_4397_ARMPLL_CLKOUT_DIV8			(8u)

/* DvfsCoreCtrlN
 * Bits 10 Request_override_PDn
 *   When set, the dvfs_request logic for this core is overridden with the
 *   content in Request_val_PDn. This field is ignored when
 *   DVFSCtrl1.dvfs_req_override is set.
 * Bits 9:8 Request_val_PDn
 *   see Request_override_PDn description
 * Bits 4:0 DVFS_RsrcTrig_PDn
 *   Specifies the pmu resource that is used to trigger the DVFS request for
 *   this core request. Current plan is to use the appropriate PWRSW_* pmu
 *   resource each power domain / cores
 */
#define CTRLN_REQUEST_OVERRIDE_SHIFT	10u
#define CTRLN_REQUEST_OVERRIDE_MASK	(0x1u << CTRLN_REQUEST_OVERRIDE_SHIFT)
#define CTRLN_REQUEST_VAL_SHIFT		8u
#define CTRLN_REQUEST_VAL_MASK		(0x3u << CTRLN_REQUEST_VAL_SHIFT)
#define CTRLN_RSRC_TRIG_SHIFT           0u
#define CTRLN_RSRC_TRIG_MASK		(0x1Fu << CTRLN_RSRC_TRIG_SHIFT)
#define CTRLN_RSRC_TRIG_CHIPC		0x1Au
#define CTRLN_RSRC_TRIG_PCIE		0x1Au
#define CTRLN_RSRC_TRIG_ARM		0x8u
#define CTRLN_RSRC_TRIG_D11_MAIN	0xEu
#define CTRLN_RSRC_TRIG_D11_AUX		0xBu
#define CTRLN_RSRC_TRIG_D11_SCAN	0xCu
#define CTRLN_RSRC_TRIG_HWA		0x8u
#define CTRLN_RSRC_TRIG_BT_MAIN		0x9u
#define CTRLN_RSRC_TRIG_BT_SCAN		0xAu

/* DVFS core FW index	*/
#define DVFS_CORE_CHIPC			0u
#define DVFS_CORE_PCIE			1u
#define DVFS_CORE_ARM			2u
#define DVFS_CORE_D11_MAIN		3u
#define DVFS_CORE_D11_AUX		4u
#define DVFS_CORE_D11_SCAN		5u
#define DVFS_CORE_BT_MAIN		6u
#define DVFS_CORE_BT_SCAN		7u
#define DVFS_CORE_HWA			8u
#define DVFS_CORE_D11_SAQM		9u
#define DVFS_CORE_GCI			10u

#define DVFS_CORE_SYSMEM		((PMUREV((sih)->pmurev) < 43u) ? \
						9u : 8u)

#define DVFS_CORE_INVALID_IDX		0xFFu

/* DVFS_Ctrl2 (PMU_BASE + 0x814)
 * Bits 31:28 Voltage ramp down step
 *   Voltage increment amount during ramp down (10mv units)
 * Bits 27:24 Voltage ramp up step
 *   Voltage increment amount during ramp up (10mv units)
 * Bits 23:16 Voltage ramp down interval
 *   Number of clocks to wait during each voltage decrement
 * Bits 15:8 Voltage ramp up interval
 *   Number of clocks to wait during each voltage increment
 * Bits 7:0 Clock stable time
 *   Number of clocks to wait after dvfs_clk_sel is asserted
 */
#define DVFS_VOLTAGE_RAMP_DOWN_STEP		1u
#define DVFS_VOLTAGE_RAMP_DOWN_STEP_SHIFT	28u
#define DVFS_VOLTAGE_RAMP_DOWN_STEP_MASK	(0xFu << DVFS_VOLTAGE_RAMP_DOWN_STEP_SHIFT)
#define DVFS_VOLTAGE_RAMP_UP_STEP		1u
#define DVFS_VOLTAGE_RAMP_UP_STEP_SHIFT		24u
#define DVFS_VOLTAGE_RAMP_UP_STEP_MASK		(0xFu << DVFS_VOLTAGE_RAMP_UP_STEP_SHIFT)
#define DVFS_VOLTAGE_RAMP_DOWN_INTERVAL		1u
#define DVFS_VOLTAGE_RAMP_DOWN_INTERVAL_SHIFT	16u
#define DVFS_VOLTAGE_RAMP_DOWN_INTERVAL_MASK	(0xFFu << DVFS_VOLTAGE_RAMP_DOWN_INTERVAL_SHIFT)
#define DVFS_VOLTAGE_RAMP_UP_INTERVAL		1u
#define DVFS_VOLTAGE_RAMP_UP_INTERVAL_SHIFT	8u
#define DVFS_VOLTAGE_RAMP_UP_INTERVAL_MASK	(0xFFu << DVFS_VOLTAGE_RAMP_UP_INTERVAL_SHIFT)
#define DVFS_CLOCK_STABLE_TIME			3u
#define DVFS_CLOCK_STABLE_TIME_SHIFT		0
#define DVFS_CLOCK_STABLE_TIME_MASK		(0xFFu << DVFS_CLOCK_STABLE_TIME_SHIFT)

/* DVFS_Voltage (PMU_BASE + 0x818)
 * Bits 22:16 HDV Voltage
 *   Specifies the target HDV voltage in 10mv units
 * Bits 14:8 NDV Voltage
 *   Specifies the target NDV voltage in 10mv units
 * Bits 6:0 LDV Voltage
 *   Specifies the target LDV voltage in 10mv units
 */
#define DVFS_VOLTAGE_XDV		0u	/* Reserved */
#define DVFS_VOLTAGE_HDV		72u	/* 0.72V */
#define DVFS_VOLTAGE_HDV_MAX		80u	/* 0.80V */
#define DVFS_VOLTAGE_HDV_PWR_OPT	68u	/* 0.68V */
#define DVFS_VOLTAGE_HDV_SHIFT		16u
#define DVFS_VOLTAGE_HDV_MASK		(0x7Fu << DVFS_VOLTAGE_HDV_SHIFT)
#define DVFS_VOLTAGE_NDV		72u	/* 0.72V */
#define DVFS_VOLTAGE_NDV_NON_LVM	76u	/* 0.76V */
#define DVFS_VOLTAGE_NDV_MAX		80u	/* 0.80V */
#define DVFS_VOLTAGE_NDV_PWR_OPT	68u	/* 0.68V */

#ifdef BCMDVFS_NDV_CBUCK_98B0_WAR
#define DVFS_VOLTAGE_NDV_DEF_CBUCK	83u	/* 0.83V */
#define DVFS_VOLTAGE_NDV_MLO_CBUCK	83u	/* 0.83V */
#define DVFS_VOLTAGE_NDV_DEF_CLDO	71u	/* 0.71V */
#define DVFS_VOLTAGE_NDV_MLO_CLDO	71u	/* 0.71V */
#else
#define DVFS_VOLTAGE_NDV_DEF_CBUCK	67u	/* 0.67V */
#define DVFS_VOLTAGE_NDV_MLO_CBUCK	71u	/* 0.71V */
#define DVFS_VOLTAGE_NDV_DEF_CLDO	60u	/* 0.60V */
#define DVFS_VOLTAGE_NDV_MLO_CLDO	67u	/* 0.67V */
#endif /* BCMDVFS_NDV_CBUCK_98B0_WAR */

#define DVFS_VOLTAGE_NDV_SHIFT		8u
#define DVFS_VOLTAGE_NDV_MASK		(0x7Fu << DVFS_VOLTAGE_NDV_SHIFT)
#define DVFS_VOLTAGE_LDV		65u	/* 0.65V */
#define DVFS_VOLTAGE_LDV_PWR_OPT	65u	/* 0.65V */
#define DVFS_VOLTAGE_LDV_SHIFT		0u
#define DVFS_VOLTAGE_LDV_MASK		(0x7Fu << DVFS_VOLTAGE_LDV_SHIFT)

#define DVFS_VOLTAGE_CBUCK		0u
#define DVFS_VOLTAGE_CLDO		1u

/* DVFS_Status (PMU_BASE + 0x81C)
 * Bits 27:26 Raw_Core_Reqn
 * Bits 25:24 Active_Core_Reqn
 * Bits 12:11 Core_dvfs_status
 * Bits 9:8 Dvfs_clk_sel
 *   00 - LDV
 *   01 - NDV
 * Bits 6:0 Dvfs Voltage
 *   The real time voltage that is being output from the dvfs controller
 */
#define DVFS_RAW_CORE_REQ_SHIFT	26u
#define DVFS_RAW_CORE_REQ_MASK	(0x3u << DVFS_RAW_CORE_REQ_SHIFT)
#define DVFS_ACT_CORE_REQ_SHIFT	24u
#define DVFS_ACT_CORE_REQ_MASK	(0x3u << DVFS_ACT_CORE_REQ_SHIFT)
#define DVFS_CORE_STATUS_SHIFT	11u
#define DVFS_CORE_STATUS_MASK	(0x3u << DVFS_CORE_STATUS_SHIFT)
#define DVFS_CLK_SEL_SHIFT	8u
#define DVFS_CLK_SEL_MASK	(0x3u << DVFS_CLK_SEL_SHIFT)
#define DVFS_VOLTAGE_SHIFT	0u
#define DVFS_VOLTAGE_MASK	(0x7Fu << DVFS_VOLTAGE_SHIFT)

/* DVFS_Ctrl1 (PMU_BASE + 0x810)
 * Bits 0 Enable DVFS
 *   This bit will enable DVFS operation. When cleared, the complete DVFS
 *   controller is bypassed and DVFS_voltage output will be the contents of
 *   NDV voltage register
 */
#define DVFS_DISABLE_DVFS	0u
#define DVFS_ENABLE_DVFS	1u
#define DVFS_ENABLE_DVFS_SHIFT	0u
#define DVFS_ENABLE_DVFS_MASK	(1u << DVFS_ENABLE_DVFS_SHIFT)

#define DVFS_LPO_DELAY		40u	/* usec (1 LPO clock + margin) */
#define DVFS_FASTLPO_DELAY	2u	/* usec (1 FAST_LPO clock + margin) */
#define DVFS_NDV_LPO_DELAY	1500u
#define DVFS_NDV_FASTLPO_DELAY	50u

#if defined(BCM_FASTLPO) && !defined(BCM_FASTLPO_DISABLED)
#define DVFS_DELAY	DVFS_FASTLPO_DELAY
#define DVFS_NDV_DELAY	DVFS_NDV_FASTLPO_DELAY
#define DVFS_SWITCH_TIMEOUT	(200)
#else
#define DVFS_DELAY	DVFS_LPO_DELAY
#define DVFS_NDV_DELAY	DVFS_NDV_LPO_DELAY
#define DVFS_SWITCH_TIMEOUT	(10000)
#endif /* BCM_FASTLPO && !BCM_FASTLPO_DISABLED */

#define DVFS_LDV	0u
#define DVFS_NDV	1u
#define DVFS_HDV	3u

#define DVFS_CTRL1_DUR_EVENT_SHIFT	(13u)
#define DVFS_CTRL1_DUR_EVENT_MASK	(1u << DVFS_CTRL1_DUR_EVENT_SHIFT)

#define	DVFS_STAT_CTRL1_MODE_HDV	(0u)
#define	DVFS_STAT_CTRL1_MODE_NDV	(1u)
#define	DVFS_STAT_CTRL1_MODE_LDV	(2u)

/* 4388B0 LDV frequency WAR */
#define	DVFS_LDV_FREQ_4388B0		(160u)
/* if bit-15 "1 cnt" == 1 us; otherwise "1 cnt" == 30.5us */
#define	DVFS_CTRL1_DUR_COUNT_PERIOD_US(ctrl, cnt)	\
		((ctrl & PMU_REG_FIELD_MASK(DVFSControl1, DvfsDurationctrPeriod)) ? \
		(cnt) : ((cnt * 305)/10u))

/* PowerControl2 (Core Offset 0x1EC)
 * Bits 17:16 DVFSStatus
 *   This 2-bit field is the DVFS voltage status mapped as
 *   00 - LDV
 *   01 - NDV
 *   10 - HDV
 * Bits 1:0 DVFSRequest
 *   This 2-bit field is used to request DVFS voltage mapped as shown above
 */
#define DVFS_REQ_LDV		DVFS_LDV
#define DVFS_REQ_NDV		DVFS_NDV
#define DVFS_REQ_HDV		DVFS_HDV
#define DVFS_REQ_SHIFT		0u
#define DVFS_REQ_MASK		(0x3u << DVFS_REQ_SHIFT)
#define DVFS_STATUS_SHIFT	16u
#define DVFS_STATUS_MASK	(0x3u << DVFS_STATUS_SHIFT)

/* GCI Chip Control 14 Register
 * Bits
 * 24:0 - RF_SW_CTRL Override Value
 * 27:25 -Drive strength select for RFFE0 pads (package_option/jtag_sel)  (default 010 6mA)
 * 30:28 -Drive strength select for RFFE1 pads (package_option/jtag_sel)  (default 010 6mA)
 * 31 -Enable CNCB clock. Once enabled CNCB clock will only run if RFFE request/Radio ON
 */
#define GCI_CC14_RFFE_DRIVE_STRENGTH_MASK	0x7E000000u
#define GCI_CC14_RFFE0_DRIVE_STRENGTH_MASK	0x0E000000u
#define GCI_CC14_RFFE1_DRIVE_STRENGTH_MASK	0x70000000u

#define RFFE_CNTRLR_0			0u
#define RFFE_CNTRLR_1			1u
#define RF_SW_CTRL_DRIVE_STRENGTH_2MA	0x00u
#define RF_SW_CTRL_DRIVE_STRENGTH_4MA	0x01u
#define RF_SW_CTRL_DRIVE_STRENGTH_6MA	0x02u
#define RF_SW_CTRL_DRIVE_STRENGTH_8MA	0x03u
#define RF_SW_CTRL_DRIVE_STRENGTH_10MA	0x04u
#define RF_SW_CTRL_DRIVE_STRENGTH_12MA	0x05u
#define RF_SW_CTRL_DRIVE_STRENGTH_14MA	0x06u
#define RF_SW_CTRL_DRIVE_STRENGTH_16MA	0x07u
#define RF_SW_CTRL_DRIVE_STRENGTH_MAX	(RF_SW_CTRL_DRIVE_STRENGTH_16MA)

/* GCI Chip Control 15 Register
 * Bits
 * 24:0 - RF_SW_CTRL Override Enable
 * 27:25 -wl2bt_derst_dly_sel
 * 28 -Force RFFE clock to run at 80Mhz. Default is 106.7MHz
 * 29 -Enable RFFE clock. Once enabled RFFE clock will only run if RFFE request/Radio ON
 * 30 -Force RFFE clock to run always regardless of Radio ON or RFFE request
 * 31 -DVFS
 */
#define GCI_CC15_RFFE_CLK_EN		(0x1u << 29u)
#define GCI_CC15_RFFE_CLK_ALWAYS_ON	(0x1u << 30u)

/* GCI Chip Control 16 Register
 * Bits 0 CB Clock sel
 *   0 - 160MHz
 *   1 - 80Mhz  - BT can force CB backplane clock to 80Mhz when wl is down
 */
#define GCI_CC16_CB_CLOCK_SEL_160	0u
#define GCI_CC16_CB_CLOCK_SEL_80	1u
#define GCI_CC16_CB_CLOCK_SEL_SHIFT	0u
#define GCI_CC16_CB_CLOCK_SEL_MASK	(0x1u << GCI_CC16_CB_CLOCK_SEL_SHIFT)
#define GCI_CHIPCTRL_16_PRISEL_ANT_MASK_PSM_OVR	(1 << 8)

/* GCI Chip Control 23 Register
 * Bits
 * 30:29 -When set, enables pull down on IO pads rf_sw_ctr15 and 7, respectively (sdata pins).
 * Only applies if the function select for rf_sw_ctrl15 and 7 are set for RFFE.
 */
#define GCI_CC23_RF_SW_CTRL_PULL_DOWN	(0x3u << 29u)

/* WL Channel Info to BT via GCI - bits 40 - 47 */
#define GCI_WL_CHN_INFO_MASK	(0xFF00)
/* WL indication of MCHAN enabled/disabled to BT - bit 36 */
#define GCI_WL_MCHAN_BIT_MASK	(0x0010)

#ifdef WLC_SW_DIVERSITY
/* WL indication of SWDIV enabled/disabled to BT - bit 33 */
#define GCI_WL_SWDIV_ANT_VALID_BIT_MASK	(0x0002)
#define GCI_SWDIV_ANT_VALID_SHIFT 0x1
#define GCI_SWDIV_ANT_VALID_DISABLE 0x0
#endif

/* Indicate to BT that WL is scheduling ACL based ble scan grant */
#define GCI_WL2BT_ACL_BSD_BLE_SCAN_GRNT_MASK 0x8000000
/* WLAN is awake Indicate to BT */
#define GCI_WL2BT_2G_AWAKE_MASK	  (1u << 28u)

/* WLAN in low latency mode indicate to BT */
#define GCI_WL2BT_LL_IND_MASK	  (1u << 20u)

/* WL inidcation of Aux Core 2G hibernate status - bit 50 */
#define GCI_WL2BT_2G_HIB_STATE_MASK	(0x0040000u)

/* WL Traffic Indication  to BT */
#define GCI_WL2BT_TRAFFIC_IND_SHIFT	(12)
#define GCI_WL2BT_TRAFFIC_IND_MASK	(0x3 << GCI_WL2BT_TRAFFIC_IND_SHIFT)

/* WL Strobe to BT */
#define GCI_WL_STROBE_BIT_MASK	(0x0020)
/* bit [51] - BT alternate antenna configuration indication */
#define GCI_WL_BT_SHARED_ANT_BIT	(19)
#define GCI_WL_BT_SHARED_ANT_MASK	(1 << GCI_WL_BT_SHARED_ANT_BIT)
/* bits [55:52] btc mode indication */
#define GCI_WL_BTC_MODE_SHIFT	(20)
#define GCI_WL_BTC_MODE_MASK	(0xF << GCI_WL_BTC_MODE_SHIFT)
#define GCI_WL_ANT_BIT_MASK	(0x00c0)
#define GCI_WL_ANT_SHIFT_BITS	(6)

/* bit [40] - to indicate RC2CX mode to BT */
#define GCI_WL_RC2CX_PERCTS_MASK	0x00000100u

/* PLL type */
#define PLL_NONE		0x00000000
#define PLL_TYPE1		0x00010000	/**< 48MHz base, 3 dividers */
#define PLL_TYPE2		0x00020000	/**< 48MHz, 4 dividers */
#define PLL_TYPE3		0x00030000	/**< 25MHz, 2 dividers */
#define PLL_TYPE4		0x00008000	/**< 48MHz, 4 dividers */
#define PLL_TYPE5		0x00018000	/**< 25MHz, 4 dividers */
#define PLL_TYPE6		0x00028000	/**< 100/200 or 120/240 only */
#define PLL_TYPE7		0x00038000	/**< 25MHz, 4 dividers */

/* ILP clock */
#define	ILP_CLOCK		32000

/* ALP clock on pre-PMU chips */
#define	ALP_CLOCK		20000000

#ifdef CFG_SIM
#define NS_ALP_CLOCK		84922
#define NS_SLOW_ALP_CLOCK	84922
#define NS_CPU_CLOCK		534500
#define NS_SLOW_CPU_CLOCK	534500
#define NS_SI_CLOCK		271750
#define NS_SLOW_SI_CLOCK	271750
#define NS_FAST_MEM_CLOCK	271750
#define NS_MEM_CLOCK		271750
#define NS_SLOW_MEM_CLOCK	271750
#else
#define NS_ALP_CLOCK		125000000
#define NS_SLOW_ALP_CLOCK	100000000
#define NS_CPU_CLOCK		1000000000
#define NS_SLOW_CPU_CLOCK	800000000
#define NS_SI_CLOCK		250000000
#define NS_SLOW_SI_CLOCK	200000000
#define NS_FAST_MEM_CLOCK	800000000
#define NS_MEM_CLOCK		533000000
#define NS_SLOW_MEM_CLOCK	400000000
#endif /* CFG_SIM */

/* HT clock */
#define	HT_CLOCK		80000000

/* corecontrol */
#define CC_UARTCLKO		0x00000001	/**< Drive UART with internal clock */
#define	CC_SE			0x00000002	/**< sync clk out enable (corerev >= 3) */
#define CC_ASYNCGPIO	0x00000004	/**< 1=generate GPIO interrupt without backplane clock */
#define CC_UARTCLKEN		0x00000008	/**< enable UART Clock (corerev > = 21 */
#define CC_RBG_RESET		0x00000040	/**< Reset RBG block (corerev > = 65 */

/* retention_ctl */
#define RCTL_MEM_RET_SLEEP_LOG_SHIFT	29
#define RCTL_MEM_RET_SLEEP_LOG_MASK	(1 << RCTL_MEM_RET_SLEEP_LOG_SHIFT)

/* 4321 chipcontrol */
#define CHIPCTRL_4321_PLL_DOWN	0x800000	/**< serdes PLL down override */

/* Fields in the otpstatus register in rev >= 21 */
#define OTPS_OL_MASK		0x000000ff
#define OTPS_OL_MFG		0x00000001	/**< manuf row is locked */
#define OTPS_OL_OR1		0x00000002	/**< otp redundancy row 1 is locked */
#define OTPS_OL_OR2		0x00000004	/**< otp redundancy row 2 is locked */
#define OTPS_OL_GU		0x00000008	/**< general use region is locked */
#define OTPS_GUP_MASK		0x00000f00
#define OTPS_GUP_SHIFT		8
#define OTPS_GUP_HW		0x00000100	/**< h/w subregion is programmed */
#define OTPS_GUP_SW		0x00000200	/**< s/w subregion is programmed */
#define OTPS_GUP_CI		0x00000400	/**< chipid/pkgopt subregion is programmed */
#define OTPS_GUP_FUSE		0x00000800	/**< fuse subregion is programmed */
#define OTPS_READY		0x00001000
#define OTPS_RV(x)		(1 << (16 + (x)))	/**< redundancy entry valid */
#define OTPS_RV_MASK		0x0fff0000
#define OTPS_PROGOK     0x40000000

/* Fields in the otpcontrol register in rev >= 21 */
#define OTPC_PROGSEL		0x00000001
#define OTPC_PCOUNT_MASK	0x0000000e
#define OTPC_PCOUNT_SHIFT	1
#define OTPC_VSEL_MASK		0x000000f0
#define OTPC_VSEL_SHIFT		4
#define OTPC_TMM_MASK		0x00000700
#define OTPC_TMM_SHIFT		8
#define OTPC_ODM		0x00000800
#define OTPC_PROGEN		0x80000000

/* Fields in the 40nm otpcontrol register in rev >= 40 */
#define OTPC_40NM_PROGSEL_SHIFT	0
#define OTPC_40NM_PCOUNT_SHIFT	1
#define OTPC_40NM_PCOUNT_WR	0xA
#define OTPC_40NM_PCOUNT_V1X	0xB
#define OTPC_40NM_REGCSEL_SHIFT	5
#define OTPC_40NM_REGCSEL_DEF	0x4
#define OTPC_40NM_PROGIN_SHIFT	8
#define OTPC_40NM_R2X_SHIFT	10
#define OTPC_40NM_ODM_SHIFT	11
#define OTPC_40NM_DF_SHIFT	15
#define OTPC_40NM_VSEL_SHIFT	16
#define OTPC_40NM_VSEL_WR	0xA
#define OTPC_40NM_VSEL_V1X	0xA
#define OTPC_40NM_VSEL_R1X	0x5
#define OTPC_40NM_COFAIL_SHIFT	30

#define OTPC1_CPCSEL_SHIFT	0
#define OTPC1_CPCSEL_DEF	6
#define OTPC1_TM_SHIFT		8
#define OTPC1_TM_WR		0x84
#define OTPC1_TM_V1X		0x84
#define OTPC1_TM_R1X		0x4
#define OTPC1_CLK_EN_MASK	0x00020000
#define OTPC1_CLK_DIV_MASK	0x00FC0000

/* Fields in otpprog in rev >= 21 and HND OTP */
#define OTPP_COL_MASK		0x000000ff
#define OTPP_COL_SHIFT		0
#define OTPP_ROW_MASK		0x0000ff00
#define OTPP_ROW_MASK9		0x0001ff00		/* for ccrev >= 49 */
#define OTPP_ROW_SHIFT		8
#define OTPP_OC_MASK		0x0f000000
#define OTPP_OC_SHIFT		24
#define OTPP_READERR		0x10000000
#define OTPP_VALUE_MASK		0x20000000
#define OTPP_VALUE_SHIFT	29
#define OTPP_START_BUSY		0x80000000
#define	OTPP_READ		0x40000000	/* HND OTP */

/* Fields in otplayout register */
#define OTPL_HWRGN_OFF_MASK	0x00000FFF
#define OTPL_HWRGN_OFF_SHIFT	0
#define OTPL_WRAP_REVID_MASK	0x00F80000
#define OTPL_WRAP_REVID_SHIFT	19
#define OTPL_WRAP_TYPE_MASK	0x00070000
#define OTPL_WRAP_TYPE_SHIFT	16
#define OTPL_WRAP_TYPE_65NM	0
#define OTPL_WRAP_TYPE_40NM	1
#define OTPL_WRAP_TYPE_28NM	2
#define OTPL_WRAP_TYPE_16NM	3
#define OTPL_WRAP_TYPE_7NM	4
#define OTPL_ROW_SIZE_MASK	0x0000F000
#define OTPL_ROW_SIZE_SHIFT	12

/* otplayout reg corerev >= 36 */
#define OTP_CISFORMAT_NEW	0x80000000

/* Opcodes for OTPP_OC field */
#define OTPPOC_READ		0
#define OTPPOC_BIT_PROG		1
#define OTPPOC_VERIFY		3
#define OTPPOC_INIT		4
#define OTPPOC_SET		5
#define OTPPOC_RESET		6
#define OTPPOC_OCST		7
#define OTPPOC_ROW_LOCK		8
#define OTPPOC_PRESCN_TEST	9

/* Opcodes for OTPP_OC field (40NM) */
#define OTPPOC_READ_40NM	0
#define OTPPOC_PROG_ENABLE_40NM 1
#define OTPPOC_PROG_DISABLE_40NM	2
#define OTPPOC_VERIFY_40NM	3
#define OTPPOC_WORD_VERIFY_1_40NM	4
#define OTPPOC_ROW_LOCK_40NM	5
#define OTPPOC_STBY_40NM	6
#define OTPPOC_WAKEUP_40NM	7
#define OTPPOC_WORD_VERIFY_0_40NM	8
#define OTPPOC_PRESCN_TEST_40NM 9
#define OTPPOC_BIT_PROG_40NM	10
#define OTPPOC_WORDPROG_40NM	11
#define OTPPOC_BURNIN_40NM	12
#define OTPPOC_AUTORELOAD_40NM	13
#define OTPPOC_OVST_READ_40NM	14
#define OTPPOC_OVST_PROG_40NM	15

/* Opcodes for OTPP_OC field (28NM) */
#define OTPPOC_READ_28NM	0
#define OTPPOC_READBURST_28NM	1
#define OTPPOC_PROG_ENABLE_28NM 2
#define OTPPOC_PROG_DISABLE_28NM	3
#define OTPPOC_PRESCREEN_28NM	4
#define OTPPOC_PRESCREEN_RP_28NM	5
#define OTPPOC_FLUSH_28NM	6
#define OTPPOC_NOP_28NM	7
#define OTPPOC_PROG_ECC_28NM	8
#define OTPPOC_PROG_ECC_READ_28NM	9
#define OTPPOC_PROG_28NM	10
#define OTPPOC_PROGRAM_RP_28NM	11
#define OTPPOC_PROGRAM_OVST_28NM	12
#define OTPPOC_RELOAD_28NM	13
#define OTPPOC_ERASE_28NM	14
#define OTPPOC_LOAD_RF_28NM	15
#define OTPPOC_CTRL_WR_28NM 16
#define OTPPOC_CTRL_RD_28NM	17
#define OTPPOC_READ_HP_28NM	18
#define OTPPOC_READ_OVST_28NM	19
#define OTPPOC_READ_VERIFY0_28NM	20
#define OTPPOC_READ_VERIFY1_28NM	21
#define OTPPOC_READ_FORCE0_28NM	22
#define OTPPOC_READ_FORCE1_28NM	23
#define OTPPOC_BURNIN_28NM	24
#define OTPPOC_PROGRAM_LOCK_28NM	25
#define OTPPOC_PROGRAM_TESTCOL_28NM	26
#define OTPPOC_READ_TESTCOL_28NM	27
#define OTPPOC_READ_FOUT_28NM	28
#define OTPPOC_SFT_RESET_28NM	29

#define OTPP_OC_MASK_28NM	0x0f800000
#define OTPP_OC_SHIFT_28NM	23

/* OTPControl bitmap for GCI rev >= 7 */
#define OTPC_PROGEN_28NM	0x8
#define OTPC_DBLERRCLR		0x20
#define OTPC_CLK_EN_MASK	0x00000040
#define OTPC_CLK_DIV_MASK	0x00000F80
#define OTPC_FORCE_OTP_PWR_DIS	0x00008000

/* Fields in otplayoutextension */
#define OTPLAYOUTEXT_FUSE_MASK	0x3FF

/* Jtagm characteristics that appeared at a given corerev */
#define	JTAGM_CREV_OLD		10	/**< Old command set, 16bit max IR */
#define	JTAGM_CREV_IRP		22	/**< Able to do pause-ir */
#define	JTAGM_CREV_RTI		28	/**< Able to do return-to-idle */

/* jtagcmd */
#define JCMD_START		0x80000000
#define JCMD_BUSY		0x80000000
#define JCMD_STATE_MASK		0x60000000
#define JCMD_STATE_TLR		0x00000000	/**< Test-logic-reset */
#define JCMD_STATE_PIR		0x20000000	/**< Pause IR */
#define JCMD_STATE_PDR		0x40000000	/**< Pause DR */
#define JCMD_STATE_RTI		0x60000000	/**< Run-test-idle */
#define JCMD0_ACC_MASK		0x0000f000
#define JCMD0_ACC_IRDR		0x00000000
#define JCMD0_ACC_DR		0x00001000
#define JCMD0_ACC_IR		0x00002000
#define JCMD0_ACC_RESET		0x00003000
#define JCMD0_ACC_IRPDR		0x00004000
#define JCMD0_ACC_PDR		0x00005000
#define JCMD0_IRW_MASK		0x00000f00
#define JCMD_ACC_MASK		0x000f0000	/**< Changes for corerev 11 */
#define JCMD_ACC_IRDR		0x00000000
#define JCMD_ACC_DR		0x00010000
#define JCMD_ACC_IR		0x00020000
#define JCMD_ACC_RESET		0x00030000
#define JCMD_ACC_IRPDR		0x00040000
#define JCMD_ACC_PDR		0x00050000
#define JCMD_ACC_PIR		0x00060000
#define JCMD_ACC_IRDR_I		0x00070000	/**< rev 28: return to run-test-idle */
#define JCMD_ACC_DR_I		0x00080000	/**< rev 28: return to run-test-idle */
#define JCMD_IRW_MASK		0x00001f00
#define JCMD_IRW_SHIFT		8
#define JCMD_DRW_MASK		0x0000003f

/* jtagctrl */
#define JCTRL_FORCE_CLK			4u		/**< Force clock */
#define JCTRL_EXT_EN			2u		/**< Enable external targets */
#define JCTRL_EN			1u		/**< Enable Jtag master */
#define JCTRL_TAPSEL_BIT		0x00000008u	/**< JtagMasterCtrl tap_sel bit */
#define JCTRL_ATCLK160_EN		0x00000040u	/**< Enable ATCLK on 160 MHz */
#define JCTRL_DAPCLK_ALP_EN		0x00000010u	/**< Enable ALP as DAP CLK source */
#define JCTRL_SDTC_ATCLK_EN		0x00000020u	/**< Enable SDTC STCLK */
#define JCTRL_ATB_SOFT_RESET		(1u << 8u)	/**< ATB_SOFT_RESET */
#define JCTRL_SDTC_DAP_TRACE_COMP	(1u << 9u)	/**< Program trace components */

/* swdmasterctrl */
#define SWDCTRL_INT_EN		8		/**< Enable internal targets */
#define SWDCTRL_FORCE_CLK	4		/**< Force clock */
#define SWDCTRL_OVJTAG		2		/**< Enable shared SWD/JTAG pins */
#define SWDCTRL_EN		1		/**< Enable Jtag master */

/* Fields in clkdiv */
#define	CLKD_SFLASH		0x1f000000
#define	CLKD_SFLASH_SHIFT	24
#define	CLKD_OTP		0x000f0000
#define	CLKD_OTP_SHIFT		16
#define	CLKD_JTAG		0x00000f00
#define	CLKD_JTAG_SHIFT		8
#define	CLKD_UART		0x000000ff

#define	CLKD2_SROM		0x00000007
#define	CLKD2_SROMDIV_32	0
#define	CLKD2_SROMDIV_64	1
#define	CLKD2_SROMDIV_96	2
#define	CLKD2_SROMDIV_128	3
#define	CLKD2_SROMDIV_192	4
#define	CLKD2_SROMDIV_256	5
#define	CLKD2_SROMDIV_384	6
#define	CLKD2_SROMDIV_512	7
#define	CLKD2_SWD		0xf8000000
#define	CLKD2_SWD_SHIFT		27

/* intstatus/intmask */
#define	CI_GPIO			0x00000001	/**< gpio intr */
#define	CI_EI			0x00000002	/**< extif intr (corerev >= 3) */
#define	CI_TEMP			0x00000004	/**< temp. ctrl intr (corerev >= 15) */
#define	CI_SIRQ			0x00000008	/**< serial IRQ intr (corerev >= 15) */
#define	CI_ECI			0x00000010	/**< eci intr (corerev >= 21) */
#define	CI_PMU			0x00000020	/**< pmu intr (corerev >= 21) */
#define	CI_UART			0x00000040	/**< uart intr (corerev >= 21) */
#define	CI_WECI			0x00000080	/* eci wakeup intr (corerev >= 21) */
#define	CI_SPMI			0x00100000	/* SPMI (corerev >= 65) */
#define	CI_RNG			0x00200000	/**<  rng intr (corerev >= 65) */
#define	CI_SSRESET_F0	0x10000000	/**< ss reset occurred */
#define	CI_SSRESET_F1	0x20000000	/**< ss reset occurred */
#define	CI_SSRESET_F2	0x40000000	/**< ss reset occurred */
#define	CI_WDRESET		0x80000000	/**< watchdog reset occurred */

/* slow_clk_ctl */
#define SCC_SS_MASK		0x00000007	/**< slow clock source mask */
#define	SCC_SS_LPO		0x00000000	/**< source of slow clock is LPO */
#define	SCC_SS_XTAL		0x00000001	/**< source of slow clock is crystal */
#define	SCC_SS_PCI		0x00000002	/**< source of slow clock is PCI */
#define SCC_LF			0x00000200	/**< LPOFreqSel, 1: 160Khz, 0: 32KHz */
#define SCC_LP			0x00000400	/**< LPOPowerDown, 1: LPO is disabled,
						 * 0: LPO is enabled
						 */
#define SCC_FS			0x00000800 /**< ForceSlowClk, 1: sb/cores running on slow clock,
						 * 0: power logic control
						 */
#define SCC_IP			0x00001000 /**< IgnorePllOffReq, 1/0: power logic ignores/honors
						 * PLL clock disable requests from core
						 */
#define SCC_XC			0x00002000	/**< XtalControlEn, 1/0: power logic does/doesn't
						 * disable crystal when appropriate
						 */
#define SCC_XP			0x00004000	/**< XtalPU (RO), 1/0: crystal running/disabled */
#define SCC_CD_MASK		0xffff0000	/**< ClockDivider (SlowClk = 1/(4+divisor)) */
#define SCC_CD_SHIFT		16

/* system_clk_ctl */
#define	SYCC_IE			0x00000001	/**< ILPen: Enable Idle Low Power */
#define	SYCC_AE			0x00000002	/**< ALPen: Enable Active Low Power */
#define	SYCC_FP			0x00000004	/**< ForcePLLOn */
#define	SYCC_AR			0x00000008	/**< Force ALP (or HT if ALPen is not set */
#define	SYCC_HR			0x00000010	/**< Force HT */
#define SYCC_CD_MASK		0xffff0000	/**< ClkDiv  (ILP = 1/(4 * (divisor + 1)) */
#define SYCC_CD_SHIFT		16

/* watchdogcounter */
/* WL sub-system reset */
#define WD_SSRESET_PCIE_F0_EN			0x10000000
/* BT sub-system reset */
#define WD_SSRESET_PCIE_F1_EN			0x20000000
#define WD_SSRESET_PCIE_F2_EN			0x40000000
/* Both WL and BT sub-system reset */
#define WD_SSRESET_PCIE_ALL_FN_EN		0x80000000
#define WD_COUNTER_MASK				0x0fffffff
#define WD_ENABLE_MASK	\
	(WD_SSRESET_PCIE_F0_EN | WD_SSRESET_PCIE_F1_EN | \
	WD_SSRESET_PCIE_F2_EN | WD_SSRESET_PCIE_ALL_FN_EN)

/* Indirect backplane access */
#define	BPIA_BYTEEN		0x0000000f
#define	BPIA_SZ1		0x00000001
#define	BPIA_SZ2		0x00000003
#define	BPIA_SZ4		0x00000007
#define	BPIA_SZ8		0x0000000f
#define	BPIA_WRITE		0x00000100
#define	BPIA_START		0x00000200
#define	BPIA_BUSY		0x00000200
#define	BPIA_ERROR		0x00000400

/* pcmcia/prog/flash_config */
#define	CF_EN			0x00000001	/**< enable */
#define	CF_EM_MASK		0x0000000e	/**< mode */
#define	CF_EM_SHIFT		1
#define	CF_EM_FLASH		0		/**< flash/asynchronous mode */
#define	CF_EM_SYNC		2		/**< synchronous mode */
#define	CF_EM_PCMCIA		4		/**< pcmcia mode */
#define	CF_DS			0x00000010	/**< destsize:  0=8bit, 1=16bit */
#define	CF_BS			0x00000020	/**< byteswap */
#define	CF_CD_MASK		0x000000c0	/**< clock divider */
#define	CF_CD_SHIFT		6
#define	CF_CD_DIV2		0x00000000	/**< backplane/2 */
#define	CF_CD_DIV3		0x00000040	/**< backplane/3 */
#define	CF_CD_DIV4		0x00000080	/**< backplane/4 */
#define	CF_CE			0x00000100	/**< clock enable */
#define	CF_SB			0x00000200	/**< size/bytestrobe (synch only) */

/* pcmcia_memwait */
#define	PM_W0_MASK		0x0000003f	/**< waitcount0 */
#define	PM_W1_MASK		0x00001f00	/**< waitcount1 */
#define	PM_W1_SHIFT		8
#define	PM_W2_MASK		0x001f0000	/**< waitcount2 */
#define	PM_W2_SHIFT		16
#define	PM_W3_MASK		0x1f000000	/**< waitcount3 */
#define	PM_W3_SHIFT		24

/* pcmcia_attrwait */
#define	PA_W0_MASK		0x0000003f	/**< waitcount0 */
#define	PA_W1_MASK		0x00001f00	/**< waitcount1 */
#define	PA_W1_SHIFT		8
#define	PA_W2_MASK		0x001f0000	/**< waitcount2 */
#define	PA_W2_SHIFT		16
#define	PA_W3_MASK		0x1f000000	/**< waitcount3 */
#define	PA_W3_SHIFT		24

/* pcmcia_iowait */
#define	PI_W0_MASK		0x0000003f	/**< waitcount0 */
#define	PI_W1_MASK		0x00001f00	/**< waitcount1 */
#define	PI_W1_SHIFT		8
#define	PI_W2_MASK		0x001f0000	/**< waitcount2 */
#define	PI_W2_SHIFT		16
#define	PI_W3_MASK		0x1f000000	/**< waitcount3 */
#define	PI_W3_SHIFT		24

/* prog_waitcount */
#define	PW_W0_MASK		0x0000001f	/**< waitcount0 */
#define	PW_W1_MASK		0x00001f00	/**< waitcount1 */
#define	PW_W1_SHIFT		8
#define	PW_W2_MASK		0x001f0000	/**< waitcount2 */
#define	PW_W2_SHIFT		16
#define	PW_W3_MASK		0x1f000000	/**< waitcount3 */
#define	PW_W3_SHIFT		24

#define PW_W0			0x0000000c
#define PW_W1			0x00000a00
#define PW_W2			0x00020000
#define PW_W3			0x01000000

/* flash_waitcount */
#define	FW_W0_MASK		0x0000003f	/**< waitcount0 */
#define	FW_W1_MASK		0x00001f00	/**< waitcount1 */
#define	FW_W1_SHIFT		8
#define	FW_W2_MASK		0x001f0000	/**< waitcount2 */
#define	FW_W2_SHIFT		16
#define	FW_W3_MASK		0x1f000000	/**< waitcount3 */
#define	FW_W3_SHIFT		24

/* When Srom support present, fields in sromcontrol */
#define	SRC_START		0x80000000
#define	SRC_BUSY		0x80000000
#define	SRC_OPCODE		0x60000000
#define	SRC_OP_READ		0x00000000
#define	SRC_OP_WRITE		0x20000000
#define	SRC_OP_WRDIS		0x40000000
#define	SRC_OP_WREN		0x60000000
#define	SRC_OTPSEL		0x00000010
#define SRC_OTPPRESENT		0x00000020
#define	SRC_LOCK		0x00000008
#define	SRC_SIZE_MASK		0x00000006
#define	SRC_SIZE_1K		0x00000000
#define	SRC_SIZE_4K		0x00000002
#define	SRC_SIZE_16K		0x00000004
#define	SRC_SIZE_SHIFT		1
#define	SRC_PRESENT		0x00000001

/* Fields in pmucontrol */
#define	PCTL_ILP_DIV_MASK	0xffff0000
#define	PCTL_ILP_DIV_SHIFT	16
#define PCTL_LQ_REQ_EN		0x00008000
#define PCTL_PLL_PLLCTL_UPD	0x00000400	/**< rev 2 */
#define PCTL_NOILP_ON_WAIT	0x00000200	/**< rev 1 */
#define	PCTL_HT_REQ_EN		0x00000100
#define	PCTL_ALP_REQ_EN		0x00000080
#define	PCTL_XTALFREQ_MASK	0x0000007c
#define	PCTL_XTALFREQ_SHIFT	2
#define	PCTL_ILP_DIV_EN		0x00000002
#define	PCTL_LPO_SEL		0x00000001

/* Fields in pmucontrol_ext */
#define PCTL_EXT_FAST_TRANS_ENAB	0x00000001u
#define PCTL_EXT_USE_LHL_TIMER		0x00000010u
#define PCTL_EXT_FASTLPO_ENAB		0x00000080u
#define PCTL_EXT_FASTLPO_SWENAB		0x00000200u
#define PCTL_EXT_FASTSEQ_ENAB		0x00001000u
#define PCTL_EXT_FASTLPO_PCIE_SWENAB	0x00004000u  /**< rev33 for FLL1M */
#define PCTL_EXT_FASTLPO_SB_SWENAB	0x00008000u  /**< rev36 for FLL1M */
#define PCTL_EXT_REQ_MIRROR_ENAB	0x00010000u  /**< rev36 for ReqMirrorEn */

#define DEFAULT_43012_MIN_RES_MASK		0x0f8bfe77

/*  Retention Control */
#define PMU_RCTL_CLK_DIV_SHIFT		0
#define PMU_RCTL_CHAIN_LEN_SHIFT	12
#define PMU_RCTL_MACPHY_DISABLE_SHIFT	26
#define PMU_RCTL_MACPHY_DISABLE_MASK	(1 << 26)
#define PMU_RCTL_LOGIC_DISABLE_SHIFT	27
#define PMU_RCTL_LOGIC_DISABLE_MASK	(1 << 27)
#define PMU_RCTL_MEMSLP_LOG_SHIFT	28
#define PMU_RCTL_MEMSLP_LOG_MASK	(1 << 28)
#define PMU_RCTL_MEMRETSLP_LOG_SHIFT	29
#define PMU_RCTL_MEMRETSLP_LOG_MASK	(1 << 29)

/*  Retention Group Control */
#define PMU_RCTLGRP_CHAIN_LEN_SHIFT	0
#define PMU_RCTLGRP_RMODE_ENABLE_SHIFT	14
#define PMU_RCTLGRP_RMODE_ENABLE_MASK	(1 << 14)
#define PMU_RCTLGRP_DFT_ENABLE_SHIFT	15
#define PMU_RCTLGRP_DFT_ENABLE_MASK	(1 << 15)
#define PMU_RCTLGRP_NSRST_DISABLE_SHIFT	16
#define PMU_RCTLGRP_NSRST_DISABLE_MASK	(1 << 16)

/* Fields in clkstretch */
#define CSTRETCH_HT		0xffff0000
#define CSTRETCH_ALP		0x0000ffff
#define CSTRETCH_REDUCE_8		0x00080008

/* gpiotimerval */
#define GPIO_ONTIME_SHIFT	16

/* clockcontrol_n */
/* Some pll types use less than the number of bits in some of these (n or m) masks */
#define	CN_N1_MASK		0x3f		/**< n1 control */
#define	CN_N2_MASK		0x3f00		/**< n2 control */
#define	CN_N2_SHIFT		8
#define	CN_PLLC_MASK		0xf0000		/**< pll control */
#define	CN_PLLC_SHIFT		16

/* clockcontrol_sb/pci/uart */
#define	CC_M1_MASK		0x3f		/**< m1 control */
#define	CC_M2_MASK		0x3f00		/**< m2 control */
#define	CC_M2_SHIFT		8
#define	CC_M3_MASK		0x3f0000	/**< m3 control */
#define	CC_M3_SHIFT		16
#define	CC_MC_MASK		0x1f000000	/**< mux control */
#define	CC_MC_SHIFT		24

/* N3M Clock control magic field values */
#define	CC_F6_2			0x02		/**< A factor of 2 in */
#define	CC_F6_3			0x03		/**< 6-bit fields like */
#define	CC_F6_4			0x05		/**< N1, M1 or M3 */
#define	CC_F6_5			0x09
#define	CC_F6_6			0x11
#define	CC_F6_7			0x21

#define	CC_F5_BIAS		5		/**< 5-bit fields get this added */

#define	CC_MC_BYPASS		0x08
#define	CC_MC_M1		0x04
#define	CC_MC_M1M2		0x02
#define	CC_MC_M1M2M3		0x01
#define	CC_MC_M1M3		0x11

/* Type 2 Clock control magic field values */
#define	CC_T2_BIAS		2		/**< n1, n2, m1 & m3 bias */
#define	CC_T2M2_BIAS		3		/**< m2 bias */

#define	CC_T2MC_M1BYP		1
#define	CC_T2MC_M2BYP		2
#define	CC_T2MC_M3BYP		4

/* Type 6 Clock control magic field values */
#define	CC_T6_MMASK		1		/**< bits of interest in m */
#define	CC_T6_M0		120000000	/**< sb clock for m = 0 */
#define	CC_T6_M1		100000000	/**< sb clock for m = 1 */
#define	SB2MIPS_T6(sb)		(2 * (sb))

/* Common clock base */
#define	CC_CLOCK_BASE1		24000000	/**< Half the clock freq */
#define CC_CLOCK_BASE2		12500000	/**< Alternate crystal on some PLLs */

/* Flash types in the chipcommon capabilities register */
#define FLASH_NONE		0x000		/**< No flash */
#define SFLASH_ST		0x100		/**< ST serial flash */
#define SFLASH_AT		0x200		/**< Atmel serial flash */
#define NFLASH			0x300		/**< NAND flash */
#define	PFLASH			0x700		/**< Parallel flash */
#define QSPIFLASH_ST		0x800
#define QSPIFLASH_AT		0x900

/* Bits in the ExtBus config registers */
#define	CC_CFG_EN		0x0001		/**< Enable */
#define	CC_CFG_EM_MASK		0x000e		/**< Extif Mode */
#define	CC_CFG_EM_ASYNC		0x0000		/**<   Async/Parallel flash */
#define	CC_CFG_EM_SYNC		0x0002		/**<   Synchronous */
#define	CC_CFG_EM_PCMCIA	0x0004		/**<   PCMCIA */
#define	CC_CFG_EM_IDE		0x0006		/**<   IDE */
#define	CC_CFG_DS		0x0010		/**< Data size, 0=8bit, 1=16bit */
#define	CC_CFG_CD_MASK		0x00e0		/**< Sync: Clock divisor, rev >= 20 */
#define	CC_CFG_CE		0x0100		/**< Sync: Clock enable, rev >= 20 */
#define	CC_CFG_SB		0x0200		/**< Sync: Size/Bytestrobe, rev >= 20 */
#define	CC_CFG_IS		0x0400		/**< Extif Sync Clk Select, rev >= 20 */

/* ExtBus address space */
#define	CC_EB_BASE		0x1a000000	/**< Chipc ExtBus base address */
#define	CC_EB_PCMCIA_MEM	0x1a000000	/**< PCMCIA 0 memory base address */
#define	CC_EB_PCMCIA_IO		0x1a200000	/**< PCMCIA 0 I/O base address */
#define	CC_EB_PCMCIA_CFG	0x1a400000	/**< PCMCIA 0 config base address */
#define	CC_EB_IDE		0x1a800000	/**< IDE memory base */
#define	CC_EB_PCMCIA1_MEM	0x1a800000	/**< PCMCIA 1 memory base address */
#define	CC_EB_PCMCIA1_IO	0x1aa00000	/**< PCMCIA 1 I/O base address */
#define	CC_EB_PCMCIA1_CFG	0x1ac00000	/**< PCMCIA 1 config base address */
#define	CC_EB_PROGIF		0x1b000000	/**< ProgIF Async/Sync base address */

/* Start/busy bit in flashcontrol */
#define SFLASH_OPCODE		0x000000ff
#define SFLASH_ACTION		0x00000700
#define	SFLASH_CS_ACTIVE	0x00001000	/**< Chip Select Active, rev >= 20 */
#define SFLASH_START		0x80000000
#define SFLASH_BUSY		SFLASH_START

/* flashcontrol action codes */
#define	SFLASH_ACT_OPONLY	0x0000		/**< Issue opcode only */
#define	SFLASH_ACT_OP1D		0x0100		/**< opcode + 1 data byte */
#define	SFLASH_ACT_OP3A		0x0200		/**< opcode + 3 addr bytes */
#define	SFLASH_ACT_OP3A1D	0x0300		/**< opcode + 3 addr & 1 data bytes */
#define	SFLASH_ACT_OP3A4D	0x0400		/**< opcode + 3 addr & 4 data bytes */
#define	SFLASH_ACT_OP3A4X4D	0x0500		/**< opcode + 3 addr, 4 don't care & 4 data bytes */
#define	SFLASH_ACT_OP3A1X4D	0x0700		/**< opcode + 3 addr, 1 don't care & 4 data bytes */

/* flashcontrol action+opcodes for ST flashes */
#define SFLASH_ST_WREN		0x0006		/**< Write Enable */
#define SFLASH_ST_WRDIS		0x0004		/**< Write Disable */
#define SFLASH_ST_RDSR		0x0105		/**< Read Status Register */
#define SFLASH_ST_WRSR		0x0101		/**< Write Status Register */
#define SFLASH_ST_READ		0x0303		/**< Read Data Bytes */
#define SFLASH_ST_PP		0x0302		/**< Page Program */
#define SFLASH_ST_SE		0x02d8		/**< Sector Erase */
#define SFLASH_ST_BE		0x00c7		/**< Bulk Erase */
#define SFLASH_ST_DP		0x00b9		/**< Deep Power-down */
#define SFLASH_ST_RES		0x03ab		/**< Read Electronic Signature */
#define SFLASH_ST_CSA		0x1000		/**< Keep chip select asserted */
#define SFLASH_ST_SSE		0x0220		/**< Sub-sector Erase */

#define SFLASH_ST_READ4B	0x6313		/* Read Data Bytes in 4Byte address */
#define SFLASH_ST_PP4B		0x6312		/* Page Program in 4Byte address */
#define SFLASH_ST_SE4B		0x62dc		/* Sector Erase in 4Byte address */
#define SFLASH_ST_SSE4B		0x6221		/* Sub-sector Erase */

#define SFLASH_MXIC_RDID	0x0390		/* Read Manufacture ID */
#define SFLASH_MXIC_MFID	0xc2		/* MXIC Manufacture ID */

#define SFLASH_WINBOND_RDID	0x0390		/* Read Manufacture ID */
#define SFLASH_WINBOND_MFID	0xef		/* Winbond Manufacture ID */

/* Status register bits for ST flashes */
#define SFLASH_ST_WIP		0x01		/**< Write In Progress */
#define SFLASH_ST_WEL		0x02		/**< Write Enable Latch */
#define SFLASH_ST_BP_MASK	0x1c		/**< Block Protect */
#define SFLASH_ST_BP_SHIFT	2
#define SFLASH_ST_SRWD		0x80		/**< Status Register Write Disable */

/* flashcontrol action+opcodes for Atmel flashes */
#define SFLASH_AT_READ				0x07e8
#define SFLASH_AT_PAGE_READ			0x07d2
/* PR9631: impossible to specify Atmel Buffer Read command */
#define SFLASH_AT_BUF1_READ
#define SFLASH_AT_BUF2_READ
#define SFLASH_AT_STATUS			0x01d7
#define SFLASH_AT_BUF1_WRITE			0x0384
#define SFLASH_AT_BUF2_WRITE			0x0387
#define SFLASH_AT_BUF1_ERASE_PROGRAM		0x0283
#define SFLASH_AT_BUF2_ERASE_PROGRAM		0x0286
#define SFLASH_AT_BUF1_PROGRAM			0x0288
#define SFLASH_AT_BUF2_PROGRAM			0x0289
#define SFLASH_AT_PAGE_ERASE			0x0281
#define SFLASH_AT_BLOCK_ERASE			0x0250
#define SFLASH_AT_BUF1_WRITE_ERASE_PROGRAM	0x0382
#define SFLASH_AT_BUF2_WRITE_ERASE_PROGRAM	0x0385
#define SFLASH_AT_BUF1_LOAD			0x0253
#define SFLASH_AT_BUF2_LOAD			0x0255
#define SFLASH_AT_BUF1_COMPARE			0x0260
#define SFLASH_AT_BUF2_COMPARE			0x0261
#define SFLASH_AT_BUF1_REPROGRAM		0x0258
#define SFLASH_AT_BUF2_REPROGRAM		0x0259

/* Status register bits for Atmel flashes */
#define SFLASH_AT_READY				0x80
#define SFLASH_AT_MISMATCH			0x40
#define SFLASH_AT_ID_MASK			0x38
#define SFLASH_AT_ID_SHIFT			3

/* SPI register bits, corerev >= 37 */
#define GSIO_START			0x80000000u
#define GSIO_BUSY			GSIO_START

/* UART Function sel related */
#define MUXENAB_DEF_UART_MASK           0x0000000fu
#define MUXENAB_DEF_UART_SHIFT          0

/* HOST_WAKE Function sel related */
#define MUXENAB_DEF_HOSTWAKE_MASK	0x000000f0u	/**< configure GPIO for host_wake */
#define MUXENAB_DEF_HOSTWAKE_SHIFT	4u

/* GCI UART Function sel related */
#define MUXENAB_GCI_UART_MASK		0x00000f00u
#define MUXENAB_GCI_UART_SHIFT		8u
#define MUXENAB_GCI_UART_FNSEL_MASK	0x00003000u
#define MUXENAB_GCI_UART_FNSEL_SHIFT	12u

/* Mask used to decide whether MUX to be performed or not */
#define MUXENAB_DEF_GETIX(val, name) \
	((((val) & MUXENAB_DEF_ ## name ## _MASK) >> MUXENAB_DEF_ ## name ## _SHIFT) - 1)

/*
 * These are the UART port assignments, expressed as offsets from the base
 * register.  These assignments should hold for any serial port based on
 * a 8250, 16450, or 16550(A).
 */

#define UART_RX		0	/**< In:  Receive buffer (DLAB=0) */
#define UART_TX		0	/**< Out: Transmit buffer (DLAB=0) */
#define UART_DLL	0	/**< Out: Divisor Latch Low (DLAB=1) */
#define UART_IER	1	/**< In/Out: Interrupt Enable Register (DLAB=0) */
#define UART_DLM	1	/**< Out: Divisor Latch High (DLAB=1) */
#define UART_IIR	2	/**< In: Interrupt Identity Register  */
#define UART_FCR	2	/**< Out: FIFO Control Register */
#define UART_LCR	3	/**< Out: Line Control Register */
#define UART_MCR	4	/**< Out: Modem Control Register */
#define UART_LSR	5	/**< In:  Line Status Register */
#define UART_MSR	6	/**< In:  Modem Status Register */
#define UART_SCR	7	/**< I/O: Scratch Register */
#define UART_LCR_DLAB	0x80	/**< Divisor latch access bit */
#define UART_LCR_WLEN8	0x03	/**< Word length: 8 bits */
#define UART_MCR_OUT2	0x08	/**< MCR GPIO out 2 */
#define UART_MCR_LOOP	0x10	/**< Enable loopback test mode */
#define UART_LSR_RX_FIFO	0x80	/**< Receive FIFO error */
#define UART_LSR_TDHR		0x40	/**< Data-hold-register empty */
#define UART_LSR_THRE		0x20	/**< Transmit-hold-register empty */
#define UART_LSR_BREAK		0x10	/**< Break interrupt */
#define UART_LSR_FRAMING	0x08	/**< Framing error */
#define UART_LSR_PARITY		0x04	/**< Parity error */
#define UART_LSR_OVERRUN	0x02	/**< Overrun error */
#define UART_LSR_RXRDY		0x01	/**< Receiver ready */
#define UART_FCR_FIFO_ENABLE 1	/**< FIFO control register bit controlling FIFO enable/disable */

/* Interrupt Identity Register (IIR) bits */
#define UART_IIR_FIFO_MASK	0xc0	/**< IIR FIFO disable/enabled mask */
#define UART_IIR_INT_MASK	0xf	/**< IIR interrupt ID source */
#define UART_IIR_MDM_CHG	0x0	/**< Modem status changed */
#define UART_IIR_NOINT		0x1	/**< No interrupt pending */
#define UART_IIR_THRE		0x2	/**< THR empty */
#define UART_IIR_RCVD_DATA	0x4	/**< Received data available */
#define UART_IIR_RCVR_STATUS	0x6	/**< Receiver status */
#define UART_IIR_CHAR_TIME	0xc	/**< Character time */

/* Interrupt Enable Register (IER) bits */
#define UART_IER_PTIME	128	/**< Programmable THRE Interrupt Mode Enable */
#define UART_IER_EDSSI	8	/**< enable modem status interrupt */
#define UART_IER_ELSI	4	/**< enable receiver line status interrupt */
#define UART_IER_ETBEI  2	/**< enable transmitter holding register empty interrupt */
#define UART_IER_ERBFI	1	/**< enable data available interrupt */

/* pmustatus */
#define PST_SLOW_WR_PENDING 0x0400
#define PST_EXTLPOAVAIL	0x0100
#define PST_WDRESET	0x0080
#define	PST_INTPEND	0x0040
#define	PST_SBCLKST	0x0030
#define	PST_SBCLKST_ILP	0x0010
#define	PST_SBCLKST_ALP	0x0020
#define	PST_SBCLKST_HT	0x0030
#define	PST_ALPAVAIL	0x0008
#define	PST_HTAVAIL	0x0004
#define	PST_RESINIT	0x0003
#define	PST_ILPFASTLPO	0x00010000

/* pmucapabilities */
#define PCAP_REV_MASK	0x000000ff
#define PCAP_RC_MASK	0x00001f00
#define PCAP_RC_SHIFT	8
#define PCAP_TC_MASK	0x0001e000
#define PCAP_TC_SHIFT	13
#define PCAP_PC_MASK	0x001e0000
#define PCAP_PC_SHIFT	17
#define PCAP_VC_MASK	0x01e00000
#define PCAP_VC_SHIFT	21
#define PCAP_CC_MASK	0x1e000000
#define PCAP_CC_SHIFT	25
#define PCAP5_PC_MASK	0x003e0000	/**< PMU corerev >= 5 */
#define PCAP5_PC_SHIFT	17
#define PCAP5_VC_MASK	0x07c00000
#define PCAP5_VC_SHIFT	22
#define PCAP5_CC_MASK	0xf8000000	/**< PMU ChipControlCnt (PMUREV < 43) */
#define PCAP5_CC_SHIFT	27

/* pmucapabilities ext */
#define PCAP_EXT_ST_NUM_SHIFT			(8)	/* stat timer number */
#define PCAP_EXT_ST_NUM_MASK			(0xf << PCAP_EXT_ST_NUM_SHIFT)
#define PCAP_EXT_ST_SRC_NUM_SHIFT		(12)	/* stat timer source number */
#define PCAP_EXT_ST_SRC_NUM_MASK		(0xf << PCAP_EXT_ST_SRC_NUM_SHIFT)
#define PCAP_EXT_MAC_RSRC_REQ_TMR_CNT_SHIFT	(20u)	/* # of MAC rsrc req timers */
#define PCAP_EXT_MAC_RSRC_REQ_TMR_CNT_MASK	(7u << PCAP_EXT_MAC_RSRC_REQ_TMR_CNT_SHIFT)
#define PCAP_EXT_PMU_INTR_RCVR_CNT_SHIFT	(23u)	/* pmu int rcvr cnt */
#define PCAP_EXT_PMU_INTR_RCVR_CNT_MASK		(7u << PCAP_EXT_PMU_INTR_RCVR_CNT_SHIFT)

/* pmucapabilities ext1 */
#define PCAP_EXT1_CC_CNT_SHIFT			13u	/* PMU ChipControlCnt (PMUREV >= 43) */
#define PCAP_EXT1_CC_CNT_MASK			(0x3f << PCAP_EXT1_CC_CNT_SHIFT)

/* pmustattimer ctrl */
#define PMU_ST_SRC_SHIFT	(0)	/* stat timer source number */
#define PMU_ST_SRC_MASK		(0xff << PMU_ST_SRC_SHIFT)
#define PMU_ST_CNT_MODE_SHIFT	(10)	/* stat timer count mode */
#define PMU_ST_CNT_MODE_MASK	(0x3 << PMU_ST_CNT_MODE_SHIFT)
#define PMU_ST_EN_SHIFT		(8)	/* stat timer enable */
#define PMU_ST_EN_MASK		(0x1 << PMU_ST_EN_SHIFT)
#define PMU_ST_ENAB		1
#define PMU_ST_DISAB		0
#define PMU_ST_INT_EN_SHIFT	(9)	/* stat timer enable */
#define PMU_ST_INT_EN_MASK	(0x1 << PMU_ST_INT_EN_SHIFT)
#define PMU_ST_INT_ENAB		1
#define PMU_ST_INT_DISAB	0

/* CoreCapabilitiesExtension */
#define PCAP_EXT_USE_MUXED_ILP_CLK_MASK	0x04000000

/* PMU Resource Request Timer registers */
/* This is based on PmuRev0 */
#define	PRRT_TIME_MASK	0x03ff
#define	PRRT_INTEN	0x0400
/* ReqActive	25
 * The hardware sets this field to 1 when the timer expires.
 * Software writes this field to 1 to make immediate resource requests.
 */
#define	PRRT_REQ_ACTIVE	0x0800	/* To check h/w status */
#define	PRRT_IMMEDIATE_RES_REQ	0x0800	/* macro for sw immediate res req */
#define	PRRT_ALP_REQ	0x1000
#define	PRRT_HT_REQ	0x2000
#define PRRT_HQ_REQ 0x4000

/* PMU Int Control register bits */
#define PMU_INTC_ALP_REQ	0x1
#define PMU_INTC_HT_REQ		0x2
#define PMU_INTC_HQ_REQ		0x4

/* bit 0 of the PMU interrupt vector is asserted if this mask is enabled */
#define RSRC_INTR_MASK_TIMER_INT_0 1
#define PMU_INTR_MASK_EXTWAKE_REQ_ACTIVE_0 (1 << 20)

/* bit 1 of the PMU interrupt vector is asserted if this mask is enabled */
#define RSRC_INTR_MASK_TIMER_INT_1		0x00000002

#define PMU_INT_STAT_RSRC_EVENT_INT0_SHIFT	(8u)
#define PMU_INT_STAT_RSRC_EVENT_INT0_MASK	(1u << PMU_INT_STAT_RSRC_EVENT_INT0_SHIFT)

/* bit 16 of the PMU interrupt vector - Stats Timer Interrupt */
#define PMU_INT_STAT_TIMER_INT_SHIFT		(16u)
#define PMU_INT_STAT_TIMER_INT_MASK		(1u <<  PMU_INT_STAT_TIMER_INT_SHIFT)

/*
 * bit 18 of the PMU interrupt vector - S/R self test fails
 */
#define PMU_INT_STAT_SR_ERR_SHIFT		(18u)
#define PMU_INT_STAT_SR_ERR_MASK		(1u <<  PMU_INT_STAT_SR_ERR_SHIFT)

/* PMU resource bit position */
#define PMURES_BIT(bit)	(1u << (bit))

/* PMU resource number limit */
#define PMURES_MAX_RESNUM	30

#if defined(VLSI_CTRL_REGS) && defined(SKIP_LEGACY_CC_API)
#define	PMU_CHIPCTL0		SI_CTRLREGS_INVALID
#define	PMU_CHIPCTL1		SI_CTRLREGS_INVALID
#define	PMU_CHIPCTL2		SI_CTRLREGS_INVALID
#define	PMU_CHIPCTL3		SI_CTRLREGS_INVALID
#define	PMU_CHIPCTL4		SI_CTRLREGS_INVALID
#define	PMU_CHIPCTL5		SI_CTRLREGS_INVALID
#define	PMU_CHIPCTL6		SI_CTRLREGS_INVALID
#define	PMU_CHIPCTL7		SI_CTRLREGS_INVALID
#define	PMU_CHIPCTL8		SI_CTRLREGS_INVALID
#define	PMU_CHIPCTL9		SI_CTRLREGS_INVALID
#define	PMU_CHIPCTL10		SI_CTRLREGS_INVALID
#define	PMU_CHIPCTL11		SI_CTRLREGS_INVALID
#define	PMU_CHIPCTL12		SI_CTRLREGS_INVALID
#define	PMU_CHIPCTL13		SI_CTRLREGS_INVALID
#define	PMU_CHIPCTL14		SI_CTRLREGS_INVALID
#define	PMU_CHIPCTL15		SI_CTRLREGS_INVALID
#define	PMU_CHIPCTL16		SI_CTRLREGS_INVALID
#define	PMU_CHIPCTL17		SI_CTRLREGS_INVALID
#define	PMU_CHIPCTL18		SI_CTRLREGS_INVALID
#define	PMU_CHIPCTL19		SI_CTRLREGS_INVALID
#define	PMU_CHIPCTL20		SI_CTRLREGS_INVALID
#define	PMU_CHIPCTL21		SI_CTRLREGS_INVALID
#define	PMU_CHIPCTL22		SI_CTRLREGS_INVALID
#define	PMU_CHIPCTL23		SI_CTRLREGS_INVALID
#define	PMU_CHIPCTL24		SI_CTRLREGS_INVALID
#define	PMU_CHIPCTL25		SI_CTRLREGS_INVALID
#define	PMU_CHIPCTL26		SI_CTRLREGS_INVALID
#define	PMU_CHIPCTL27		SI_CTRLREGS_INVALID
#define	PMU_CHIPCTL28		SI_CTRLREGS_INVALID
#define	PMU_CHIPCTL29		SI_CTRLREGS_INVALID
#define	PMU_CHIPCTL30		SI_CTRLREGS_INVALID
#else /* !VLSI_CTRL_REGS || !SKIP_LEGACY_CC_API */
#define	PMU_CHIPCTL0		0u
#define	PMU_CHIPCTL1		1u
#define	PMU_CHIPCTL2		2u
#define	PMU_CHIPCTL3		3u
#define	PMU_CHIPCTL4		4u
#define	PMU_CHIPCTL5		5u
#define	PMU_CHIPCTL6		6u
#define	PMU_CHIPCTL7		7u
#define	PMU_CHIPCTL8		8u
#define	PMU_CHIPCTL9		9u
#define	PMU_CHIPCTL10		10u
#define	PMU_CHIPCTL11		11u
#define	PMU_CHIPCTL12		12u
#define	PMU_CHIPCTL13		13u
#define	PMU_CHIPCTL14		14u
#define	PMU_CHIPCTL15		15u
#define	PMU_CHIPCTL16		16u
#define	PMU_CHIPCTL17		17u
#define	PMU_CHIPCTL18		18u
#define	PMU_CHIPCTL19		19u
#define	PMU_CHIPCTL20		20u
#define	PMU_CHIPCTL21		21u
#define	PMU_CHIPCTL22		22u
#define	PMU_CHIPCTL23		23u
#define	PMU_CHIPCTL24		24u
#define	PMU_CHIPCTL25		25u
#define	PMU_CHIPCTL26		26u
#define	PMU_CHIPCTL27		27u
#define	PMU_CHIPCTL28		28u
#define	PMU_CHIPCTL29		29u
#define	PMU_CHIPCTL30		30u
#endif /* !VLSI_CTRL_REGS || !SKIP_LEGACY_CC_API */

/* PMU chip control0 register */
#define PMU_CC0_4387_XTALCORESIZE_BIAS_ADJ_START_VAL	(0x20 << 0)
#define PMU_CC0_4387_XTALCORESIZE_BIAS_ADJ_START_MASK	(0x3F << 0)
#define PMU_CC0_4387_XTALCORESIZE_BIAS_ADJ_NORMAL_VAL	(0x1A << 6)
#define PMU_CC0_4387_XTALCORESIZE_BIAS_ADJ_NORMAL_MASK	(0x3F << 6)
#define PMU_CC0_4387_XTAL_RES_BYPASS_START_VAL			(0 << 12)
#define PMU_CC0_4387_XTAL_RES_BYPASS_START_MASK			(0x7 << 12)
#define PMU_CC0_4387_XTAL_RES_BYPASS_NORMAL_VAL			(0x2 << 15)
#define PMU_CC0_4387_XTAL_RES_BYPASS_NORMAL_MASK		(0x7 << 15)
#define PMU_CC0_4387_BT_PU_WAKE_MASK				(0x3u << 30u)

/* clock req types */
#define PMU_CC1_CLKREQ_TYPE_SHIFT	19
#define PMU_CC1_CLKREQ_TYPE_MASK	(1 << PMU_CC1_CLKREQ_TYPE_SHIFT)

#define CLKREQ_TYPE_CONFIG_OPENDRAIN		0
#define CLKREQ_TYPE_CONFIG_PUSHPULL		1

/* Power Control */
#define PWRCTL_ENAB_MEM_CLK_GATE_SHIFT		5
#define PWRCTL_FORCE_HW_PWR_REQ_OFF_SHIFT	6
#define PWRCTL_AUTO_MEM_STBYRET			28

/* PMU chip control1 register */
#define	PMU_CC1_RXC_DLL_BYPASS		0x00010000
#define PMU_CC1_ENABLE_BBPLL_PWR_DOWN	0x00000010

#define PMU_CC1_IF_TYPE_MASK		0x00000030
#define PMU_CC1_IF_TYPE_RMII		0x00000000
#define PMU_CC1_IF_TYPE_MII		0x00000010
#define PMU_CC1_IF_TYPE_RGMII		0x00000020

#define PMU_CC1_SW_TYPE_MASK		0x000000c0
#define PMU_CC1_SW_TYPE_EPHY		0x00000000
#define PMU_CC1_SW_TYPE_EPHYMII		0x00000040
#define PMU_CC1_SW_TYPE_EPHYRMII	0x00000080
#define PMU_CC1_SW_TYPE_RGMII		0x000000c0

#define PMU_CC1_ENABLE_CLOSED_LOOP_MASK 0x00000080
#define PMU_CC1_ENABLE_CLOSED_LOOP      0x00000000

#define PMU_CC1_PWRSW_CLKSTRSTP_DELAY_MASK	0x00003F00u
#ifdef BCM_FASTLPO_PMU
#define PMU_CC1_PWRSW_CLKSTRSTP_DELAY		0x00002000u
#else
#define PMU_CC1_PWRSW_CLKSTRSTP_DELAY		0x00000400u
#endif /* BCM_FASTLPO_PMU */

/* PMU chip control2 register */
#define PMU_CC2_CB2WL_INTR_PWRREQ_EN		(1u << 13u)
#define PMU_CC2_RFLDO3P3_PU_FORCE_ON		(1u << 15u)
#define PMU_CC2_RFLDO3P3_PU_CLEAR		0x00000000u

#define PMU_CC2_WL2CDIG_I_PMU_SLEEP		(1u << 16u)
#define PMU_CC2_FORCE_SUBCORE_PWR_SWITCH_ON	(1u << 18u)
#define PMU_CC2_FORCE_PHY_PWR_SWITCH_ON		(1u << 19u)
#define PMU_CC2_FORCE_VDDM_PWR_SWITCH_ON	(1u << 20u)
#define PMU_CC2_FORCE_MEMLPLDO_PWR_SWITCH_ON	(1u << 21u)
#define PMU_CC2_MASK_WL_DEV_WAKE             (1u << 22u)
#define PMU_CC2_INV_GPIO_POLARITY_PMU_WAKE   (1u << 25u)
#define PMU_CC2_GCI2_WAKE                    (1u << 31u)

#define PMU_CC2_4387_XTALCORESIZE_BIAS_ADJ_START_VAL	(0x3u << 26u)
#define PMU_CC2_4387_XTALCORESIZE_BIAS_ADJ_START_MASK	(0x3u << 26u)
#define PMU_CC2_4387_XTALCORESIZE_BIAS_ADJ_NORMAL_VAL	(0x0u << 28u)
#define PMU_CC2_4387_XTALCORESIZE_BIAS_ADJ_NORMAL_MASK	(0x3u << 28u)

/* PMU chip control3 register */
#define PMU_CC3_ENABLE_SDIO_WAKEUP_SHIFT  19u
#define PMU_CC3_ENABLE_RF_SHIFT           22u
#define PMU_CC3_RF_DISABLE_IVALUE_SHIFT   23u

#define PMU_CC3_4387_XTALCORESIZE_PMOS_START_VAL	(0x3F << 0)
#define PMU_CC3_4387_XTALCORESIZE_PMOS_START_MASK	(0x3F << 0)
#define PMU_CC3_4387_XTALCORESIZE_PMOS_NORMAL_VAL	(0x3F << 15)
#define PMU_CC3_4387_XTALCORESIZE_PMOS_NORMAL_MASK	(0x3F << 15)
#define PMU_CC3_4387_XTALCORESIZE_NMOS_START_VAL	(0x3F << 6)
#define PMU_CC3_4387_XTALCORESIZE_NMOS_START_MASK	(0x3F << 6)
#define PMU_CC3_4387_XTALCORESIZE_NMOS_NORMAL_VAL	(0x3F << 21)
#define PMU_CC3_4387_XTALCORESIZE_NMOS_NORMAL_MASK	(0x3F << 21)
#define PMU_CC3_4387_XTALSEL_BIAS_RES_START_VAL		(0x2 << 12)
#define PMU_CC3_4387_XTALSEL_BIAS_RES_START_MASK	(0x7 << 12)
#define PMU_CC3_4387_XTALSEL_BIAS_RES_NORMAL_VAL	(0x5 << 27)
#define PMU_CC3_4387_XTALSEL_BIAS_RES_NORMAL_MASK	(0x7 << 27)

/* 53537 series moved switch_type and gmac_if_type to CC4 [15:14] and [13:12] */
#define PMU_CC4_IF_TYPE_MASK		0x00003000
#define PMU_CC4_IF_TYPE_RMII		0x00000000
#define PMU_CC4_IF_TYPE_MII		0x00001000
#define PMU_CC4_IF_TYPE_RGMII		0x00002000

#define PMU_CC4_SW_TYPE_MASK		0x0000c000
#define PMU_CC4_SW_TYPE_EPHY		0x00000000
#define PMU_CC4_SW_TYPE_EPHYMII		0x00004000
#define PMU_CC4_SW_TYPE_EPHYRMII	0x00008000
#define PMU_CC4_SW_TYPE_RGMII		0x0000c000
#define PMU_CC4_DISABLE_LQ_AVAIL	(1<<27)

#define PMU_CC4_4369_MAIN_PD_CBUCK2VDDB_ON	(1u << 15u)
#define PMU_CC4_4369_MAIN_PD_CBUCK2VDDRET_ON	(1u << 16u)
#define PMU_CC4_4369_MAIN_PD_MEMLPLDO2VDDB_ON	(1u << 17u)
#define PMU_CC4_4369_MAIN_PD_MEMLPLDO2VDDRET_ON	(1u << 18u)

#define PMU_CC4_4369_AUX_PD_CBUCK2VDDB_ON	(1u << 21u)
#define PMU_CC4_4369_AUX_PD_CBUCK2VDDRET_ON	(1u << 22u)
#define PMU_CC4_4369_AUX_PD_MEMLPLDO2VDDB_ON	(1u << 23u)
#define PMU_CC4_4369_AUX_PD_MEMLPLDO2VDDRET_ON	(1u << 24u)

#define PMU_CC4_4387_MAIN_PD_CBUCK2VDDB_ON	(1u << 15u)
#define PMU_CC4_4387_MAIN_PD_CBUCK2VDDRET_ON	(1u << 16u)
#define PMU_CC4_4387_MAIN_PD_MEMLPLDO2VDDB_ON	(1u << 17u)
#define PMU_CC4_4387_MAIN_PD_MEMLPDLO2VDDRET_ON	(1u << 18u)

#define PMU_CC4_4387_AUX_PD_CBUCK2VDDB_ON	(1u << 21u)
#define PMU_CC4_4387_AUX_PD_CBUCK2VDDRET_ON	(1u << 22u)
#define PMU_CC4_4387_AUX_PD_MEMLPLDO2VDDB_ON	(1u << 23u)
#define PMU_CC4_4387_AUX_PD_MEMLPLDO2VDDRET_ON	(1u << 24u)

/* PMU chip control5 register */
#define PMU_CC5_4369_SUBCORE_CBUCK2VDDB_ON	(1u << 9u)
#define PMU_CC5_4369_SUBCORE_CBUCK2VDDRET_ON	(1u << 10u)
#define PMU_CC5_4369_SUBCORE_MEMLPLDO2VDDB_ON	(1u << 11u)
#define PMU_CC5_4369_SUBCORE_MEMLPLDO2VDDRET_ON	(1u << 12u)

#define PMU_CC5_4387_SUBCORE_CBUCK2VDDB_ON	(1u << 9u)
#define PMU_CC5_4387_SUBCORE_CBUCK2VDDRET_ON	(1u << 10u)
#define PMU_CC5_4387_SUBCORE_MEMLPLDO2VDDB_ON	(1u << 11u)
#define PMU_CC5_4387_SUBCORE_MEMLPLDO2VDDRET_ON	(1u << 12u)

#define PMU_CC5_4388_SUBCORE_SDTCCLK0_ON	(1u << 3u)
#define PMU_CC5_4388_SUBCORE_SDTCCLK1_ON	(1u << 4u)

#define PMU_CC5_4389_SUBCORE_SDTCCLK0_ON	(1u << 3u)
#define PMU_CC5_4389_SUBCORE_SDTCCLK1_ON	(1u << 4u)

/* PMU chip control6 register */
#define PMU_CC6_RX4_CLK_SEQ_SELECT_MASK	BCM_MASK32(1u, 0u)
#define PMU_CC6_ENABLE_DMN1_WAKEUP		(1 << 3)
#define PMU_CC6_ENABLE_CLKREQ_WAKEUP		(1 << 4)
#define PMU_CC6_ENABLE_PMU_WAKEUP_ALP		(1 << 6)
#define PMU_CC6_ENABLE_PCIE_RETENTION		(1 << 12)
#define PMU_CC6_ENABLE_CMN_SUBCORE_RETENTION	(1 << 12)
#define PMU_CC6_ENABLE_PMU_EXT_PERST		(1 << 13)
#define PMU_CC6_ENABLE_PMU_WAKEUP_PERST		(1 << 14)
#define PMU_CC6_ENABLE_LEGACY_WAKEUP		(1 << 16)

/* PMU chip control7 register */
#define PMU_CC7_ENABLE_L2REFCLKPAD_PWRDWN	(1 << 25)
#define PMU_CC7_ENABLE_MDIO_RESET_WAR		(1 << 27)
/* 53537 series have gmca1 gmac_if_type in cc7 [7:6](defalut 0b01) */
#define PMU_CC7_IF_TYPE_MASK		0x000000c0
#define PMU_CC7_IF_TYPE_RMII		0x00000000
#define PMU_CC7_IF_TYPE_MII		0x00000040
#define PMU_CC7_IF_TYPE_RGMII		0x00000080

#define PMU_CC10_PCIE_PWRSW_RESET0_CNT_SHIFT		0
#define PMU_CC10_PCIE_PWRSW_RESET0_CNT_MASK		0x000000ff
#define PMU_CC10_PCIE_PWRSW_RESET1_CNT_SHIFT		8
#define PMU_CC10_PCIE_PWRSW_RESET1_CNT_MASK		0x0000ff00
#define PMU_CC10_PCIE_PWRSW_UP_DLY_SHIFT		16
#define PMU_CC10_PCIE_PWRSW_UP_DLY_MASK		0x000f0000
#define PMU_CC10_PCIE_PWRSW_FORCE_PWROK_DLY_SHIFT	20
#define PMU_CC10_PCIE_PWRSW_FORCE_PWROK_DLY_MASK	0x00f00000
#define PMU_CC10_FORCE_PCIE_ON		(1 << 24)
#define PMU_CC10_FORCE_PCIE_SW_ON	(1 << 25)
#define PMU_CC10_FORCE_PCIE_RETNT_ON	(1 << 26)

#define PMU_CC10_PCIE_PWRSW_RESET_CNT_4US		1
#define PMU_CC10_PCIE_PWRSW_RESET_CNT_8US		2

#define PMU_CC10_PCIE_PWRSW_UP_DLY_0US			0

#define PMU_CC10_PCIE_PWRSW_FORCE_PWROK_DLY_4US	1
#define PMU_CC10_PCIE_RESET0_CNT_SLOW_MASK	(0xFu << 4u)
#define PMU_CC10_PCIE_RESET1_CNT_SLOW_MASK	(0xFu << 12u)

/* PMU chip control12 register */
#define PMU_CC12_DISABLE_LQ_CLK_ON	(1u << 31u) /* HW4387-254 */

/* PMU chip control13 register */
#define PMU_CC13_SUBCORE_CBUCK2VDDB_OFF		(1u << 0u)
#define PMU_CC13_SUBCORE_CBUCK2VDDRET_OFF	(1u << 1u)
#define PMU_CC13_SUBCORE_MEMLPLDO2VDDB_OFF	(1u << 2u)
#define PMU_CC13_SUBCORE_MEMLPLDO2VDDRET_OFF	(1u << 3u)

#define PMU_CC13_MAIN_CBUCK2VDDB_OFF		(1u << 4u)
#define PMU_CC13_MAIN_CBUCK2VDDRET_OFF		(1u << 5u)
#define PMU_CC13_MAIN_MEMLPLDO2VDDB_OFF		(1u << 6u)
#define PMU_CC13_MAIN_MEMLPLDO2VDDRET_OFF	(1u << 7u)

#define PMU_CC13_AUX_CBUCK2VDDB_OFF		(1u << 8u)
#define PMU_CC13_AUX_MEMLPLDO2VDDB_OFF		(1u << 10u)
#define PMU_CC13_AUX_MEMLPLDO2VDDRET_OFF	(1u << 11u)
#define PMU_CC13_AUX_CBUCK2VDDRET_OFF		(1u << 12u)
#define PMU_CC13_CMN_MEMLPLDO2VDDRET_ON		(1u << 18u)

/* HW4368-331 */
#define PMU_CC13_MAIN_ALWAYS_USE_COHERENT_IF0	(1u << 13u)
#define PMU_CC13_MAIN_ALWAYS_USE_COHERENT_IF1	(1u << 14u)
#define PMU_CC13_AUX_ALWAYS_USE_COHERENT_IF0	(1u << 15u)
#define PMU_CC13_AUX_ALWAYS_USE_COHERENT_IF1	(1u << 19u)

#define PMU_CC13_LHL_TIMER_SELECT		(1u << 23u)

#define PMU_CC13_4387_ENAB_RADIO_REG_CLK	(1u << 9u)
#define PMU_CC13_4387_LHL_TIMER_SELECT		(1u << 23u)

#define PMU_CC13_4397_ENAB_RADIO_REG_CLK	(1u << 9u)
#define PMU_CC13_4397_LHL_TIMER_SELECT		(1u << 11u)
#define PMU_CC13_ENABLE_PCIE_RETENTION		(1 << 30)

/* PMU chip control14 register */
#define PMU_CC14_MAIN_VDDB2VDDRET_UP_DLY_MASK		(0xFu)
#define PMU_CC14_MAIN_VDDB2VDD_UP_DLY_MASK		(0xFu << 4u)
#define PMU_CC14_AUX_VDDB2VDDRET_UP_DLY_MASK		(0xFu << 8u)
#define PMU_CC14_AUX_VDDB2VDD_UP_DLY_MASK		(0xFu << 12u)
#define PMU_CC14_PCIE_VDDB2VDDRET_UP_DLY_MASK		(0xFu << 16u)
#define PMU_CC14_PCIE_VDDB2VDD_UP_DLY_MASK		(0xFu << 20u)

/* PMU chip control15 register */
#define PMU_CC15_PCIE_VDDB_CURRENT_LIMIT_DELAY_MASK	(0xFu << 4u)
#define PMU_CC15_PCIE_VDDB_FORCE_RPS_PWROK_DELAY_MASK	(0xFu << 8u)

#define PMU_CC16_CLK4M_DIS		(1u << 4u)
#define PMU_CC16_FF_ZERO_ADJ		(4u << 5u)

/* PMU chip control17 register */
#define PMU_CC17_SCAN_DIG_SR_CLK_SHIFT		(2u)
#define PMU_CC17_SCAN_DIG_SR_CLK_MASK		(3u << 2u)
#define PMU_CC17_SCAN_CBUCK2VDDB_OFF		(1u << 8u)
#define PMU_CC17_SCAN_MEMLPLDO2VDDB_OFF		(1u << 10u)
#define PMU_CC17_SCAN_MEMLPLDO2VDDRET_OFF	(1u << 11u)
#define PMU_CC17_SCAN_CBUCK2VDDB_ON		(1u << 24u)
#define PMU_CC17_SCAN_MEMLPLDO2VDDB_ON		(1u << 26u)
#define PMU_CC17_SCAN_MEMLPLDO2VDDRET_ON	(1u << 27u)

#define SCAN_DIG_SR_CLK_80_MHZ		(0)	/* 80 MHz */
#define SCAN_DIG_SR_CLK_53P35_MHZ	(1u)	/* 53.35 MHz */
#define SCAN_DIG_SR_CLK_40_MHZ		(2u)	/* 40 MHz */

/* Expiry time for wl_SSReset if P channel sleep handshake is not through */
#define PMU_CC18_WL_P_CHAN_TIMER_SEL_OFF	(1u << 1u)
#define PMU_CC18_WL_P_CHAN_TIMER_SEL_MASK	(7u << 1u)

#define PMU_CC18_WL_P_CHAN_TIMER_SEL_8ms	7u	/* (2^(7+1))*32us = 8ms */

/* Enable wl booker to force a P channel sleep handshake upon assertion of wl_SSReset */
#define PMU_CC18_WL_BOOKER_FORCEPWRDWN_EN	(1u << 4u)

/* PMU chip control 19 register */
#define PMU_CC19_ASYNC_ATRESETMN	(1u << 9u)

/* PMU chip control 23 register */
#define PMU_CC23_MACPHYCLK_MASK		(1u << 31u)

#define PMU_CC23_AT_CLK0_ON		(1u << 14u)
#define PMU_CC23_AT_CLK1_ON		(1u << 15u)

/* PMU chip control 30 register */
#define PMU_CC30_PTM_OR_PREC_SELECT_SHIFT	(11u)
#define PMU_CC30_PTM_OR_PREC_SELECT_MASK	(0x800u)
#define PMU_CC30_BBPLL_DIVCLK_GATE_CTRL_SHIFT	(13u)
#define PMU_CC30_BBPLL_DIVCLK_GATE_CTRL_MASK	(0x7FFFu << \
							PMU_CC30_BBPLL_DIVCLK_GATE_CTRL_SHIFT)
#define PMU_CC30_CLK_GATE_CTRL_BIT_UNKNOWN	(0xFFu)

/* PMU corerev and chip specific PLL controls.
 * PMU<rev>_PLL<num>_XX where <rev> is PMU corerev and <num> is an arbitrary number
 * to differentiate different PLLs controlled by the same PMU rev.
 */
/* pllcontrol registers */
/* PDIV, div_phy, div_arm, div_adc, dith_sel, ioff, kpd_scale, lsb_sel, mash_sel, lf_c & lf_r */
#define	PMU0_PLL0_PLLCTL0		0
#define	PMU0_PLL0_PC0_PDIV_MASK		1
#define	PMU0_PLL0_PC0_PDIV_FREQ		25000
#define PMU0_PLL0_PC0_DIV_ARM_MASK	0x00000038
#define PMU0_PLL0_PC0_DIV_ARM_SHIFT	3
#define PMU0_PLL0_PC0_DIV_ARM_BASE	8

/* PC0_DIV_ARM for PLLOUT_ARM */
#define PMU0_PLL0_PC0_DIV_ARM_110MHZ	0
#define PMU0_PLL0_PC0_DIV_ARM_97_7MHZ	1
#define PMU0_PLL0_PC0_DIV_ARM_88MHZ	2
#define PMU0_PLL0_PC0_DIV_ARM_80MHZ	3 /* Default */
#define PMU0_PLL0_PC0_DIV_ARM_73_3MHZ	4
#define PMU0_PLL0_PC0_DIV_ARM_67_7MHZ	5
#define PMU0_PLL0_PC0_DIV_ARM_62_9MHZ	6
#define PMU0_PLL0_PC0_DIV_ARM_58_6MHZ	7

/* Wildcard base, stop_mod, en_lf_tp, en_cal & lf_r2 */
#define	PMU0_PLL0_PLLCTL1		1
#define	PMU0_PLL0_PC1_WILD_INT_MASK	0xf0000000
#define	PMU0_PLL0_PC1_WILD_INT_SHIFT	28
#define	PMU0_PLL0_PC1_WILD_FRAC_MASK	0x0fffff00
#define	PMU0_PLL0_PC1_WILD_FRAC_SHIFT	8
#define	PMU0_PLL0_PC1_STOP_MOD		0x00000040

/* Wildcard base, vco_calvar, vco_swc, vco_var_selref, vso_ical & vco_sel_avdd */
#define	PMU0_PLL0_PLLCTL2		2
#define	PMU0_PLL0_PC2_WILD_INT_MASK	0xf
#define	PMU0_PLL0_PC2_WILD_INT_SHIFT	4

/* pllcontrol registers */
/* ndiv_pwrdn, pwrdn_ch<x>, refcomp_pwrdn, dly_ch<x>, p1div, p2div, _bypass_sdmod */
#define PMU1_PLL0_PLLCTL0		0
#define PMU1_PLL0_PC0_P1DIV_MASK	0x00f00000
#define PMU1_PLL0_PC0_P1DIV_SHIFT	20
#define PMU1_PLL0_PC0_P2DIV_MASK	0x0f000000
#define PMU1_PLL0_PC0_P2DIV_SHIFT	24

/* m<x>div */
#define PMU1_PLL0_PLLCTL1		1
#define PMU1_PLL0_PC1_M1DIV_MASK	0x000000ff
#define PMU1_PLL0_PC1_M1DIV_SHIFT	0
#define PMU1_PLL0_PC1_M2DIV_MASK	0x0000ff00
#define PMU1_PLL0_PC1_M2DIV_SHIFT	8
#define PMU1_PLL0_PC1_M3DIV_MASK	0x00ff0000
#define PMU1_PLL0_PC1_M3DIV_SHIFT	16
#define PMU1_PLL0_PC1_M4DIV_MASK	0xff000000
#define PMU1_PLL0_PC1_M4DIV_SHIFT	24
#define PMU1_PLL0_PC1_M4DIV_BY_9	9
#define PMU1_PLL0_PC1_M4DIV_BY_18	0x12
#define PMU1_PLL0_PC1_M4DIV_BY_36	0x24
#define PMU1_PLL0_PC1_M4DIV_BY_60	0x3C
#define PMU1_PLL0_PC1_M2_M4DIV_MASK     0xff00ff00
#define PMU1_PLL0_PC1_HOLD_LOAD_CH      0x28

#define DOT11MAC_880MHZ_CLK_DIVISOR_SHIFT 8
#define DOT11MAC_880MHZ_CLK_DIVISOR_MASK (0xFF << DOT11MAC_880MHZ_CLK_DIVISOR_SHIFT)
#define DOT11MAC_880MHZ_CLK_DIVISOR_VAL  (0xE << DOT11MAC_880MHZ_CLK_DIVISOR_SHIFT)

/* m<x>div, ndiv_dither_mfb, ndiv_mode, ndiv_int */
#define PMU1_PLL0_PLLCTL2		2
#define PMU1_PLL0_PC2_M5DIV_MASK	0x000000ff
#define PMU1_PLL0_PC2_M5DIV_SHIFT	0
#define PMU1_PLL0_PC2_M5DIV_BY_12	0xc
#define PMU1_PLL0_PC2_M5DIV_BY_18	0x12
#define PMU1_PLL0_PC2_M5DIV_BY_31	0x1f
#define PMU1_PLL0_PC2_M5DIV_BY_36	0x24
#define PMU1_PLL0_PC2_M5DIV_BY_42	0x2a
#define PMU1_PLL0_PC2_M5DIV_BY_60	0x3c
#define PMU1_PLL0_PC2_M6DIV_MASK	0x0000ff00
#define PMU1_PLL0_PC2_M6DIV_SHIFT	8
#define PMU1_PLL0_PC2_M6DIV_BY_18	0x12
#define PMU1_PLL0_PC2_M6DIV_BY_36	0x24
#define PMU1_PLL0_PC2_NDIV_MODE_MASK	0x000e0000
#define PMU1_PLL0_PC2_NDIV_MODE_SHIFT	17
#define PMU1_PLL0_PC2_NDIV_MODE_MASH	1
#define PMU1_PLL0_PC2_NDIV_MODE_MFB	2
#define PMU1_PLL0_PC2_NDIV_INT_MASK	0x1ff00000
#define PMU1_PLL0_PC2_NDIV_INT_SHIFT	20

/* ndiv_frac */
#define PMU1_PLL0_PLLCTL3		3
#define PMU1_PLL0_PC3_NDIV_FRAC_MASK	0x00ffffff
#define PMU1_PLL0_PC3_NDIV_FRAC_SHIFT	0

/* pll_ctrl */
#define PMU1_PLL0_PLLCTL4		4

/* pll_ctrl, vco_rng, clkdrive_ch<x> */
#define PMU1_PLL0_PLLCTL5		5
#define PMU1_PLL0_PC5_CLK_DRV_MASK	0xffffff00
#define PMU1_PLL0_PC5_CLK_DRV_SHIFT	8
#define PMU1_PLL0_PC5_ASSERT_CH_MASK	0x3f000000
#define PMU1_PLL0_PC5_ASSERT_CH_SHIFT	24
#define PMU1_PLL0_PC5_DEASSERT_CH_MASK	0xff000000

#define PMU1_PLL0_PLLCTL6		6
#define PMU1_PLL0_PLLCTL7		7
#define PMU1_PLL0_PLLCTL8		8

#define PMU1_PLLCTL8_OPENLOOP_MASK	(1 << 1)

#define PMU1_PLL0_PLLCTL9		9

#define PMU1_PLL0_PLLCTL10		10

/* PMU rev 5 (& 6) */
#define	PMU5_PLL_P1P2_OFF		0
#define	PMU5_PLL_P1_MASK		0x0f000000
#define	PMU5_PLL_P1_SHIFT		24
#define	PMU5_PLL_P2_MASK		0x00f00000
#define	PMU5_PLL_P2_SHIFT		20
#define	PMU5_PLL_M14_OFF		1
#define	PMU5_PLL_MDIV_MASK		0x000000ff
#define	PMU5_PLL_MDIV_WIDTH		8
#define	PMU5_PLL_NM5_OFF		2
#define	PMU5_PLL_NDIV_MASK		0xfff00000
#define	PMU5_PLL_NDIV_SHIFT		20
#define	PMU5_PLL_NDIV_MODE_MASK		0x000e0000
#define	PMU5_PLL_NDIV_MODE_SHIFT	17
#define	PMU5_PLL_FMAB_OFF		3
#define	PMU5_PLL_MRAT_MASK		0xf0000000
#define	PMU5_PLL_MRAT_SHIFT		28
#define	PMU5_PLL_ABRAT_MASK		0x08000000
#define	PMU5_PLL_ABRAT_SHIFT		27
#define	PMU5_PLL_FDIV_MASK		0x07ffffff
#define	PMU5_PLL_PLLCTL_OFF		4
#define	PMU5_PLL_PCHI_OFF		5
#define	PMU5_PLL_PCHI_MASK		0x0000003f

/* pmu XtalFreqRatio */
#define	PMU_XTALFREQ_REG_ILPCTR_MASK	0x00001FFF
#define	PMU_XTALFREQ_REG_MEASURE_MASK	0x80000000
#define	PMU_XTALFREQ_REG_MEASURE_SHIFT	31

/* Divider allocation in 5357 */
#define	PMU5_MAINPLL_CPU		1
#define	PMU5_MAINPLL_MEM		2
#define	PMU5_MAINPLL_SI			3

/* PLL usage in 4716/47162 */
#define	PMU4716_MAINPLL_PLL0		12

/* PLL Usages for 4368 */
#define PMU4368_P1DIV_LO_SHIFT			0
#define PMU4368_P1DIV_HI_SHIFT			2

#define PMU4368_PLL1_PC4_P1DIV_MASK		0xC0000000
#define PMU4368_PLL1_PC4_P1DIV_SHIFT		30
#define PMU4368_PLL1_PC5_P1DIV_MASK		0x00000003
#define PMU4368_PLL1_PC5_P1DIV_SHIFT		0
#define PMU4368_PLL1_PC5_NDIV_INT_MASK		0x00000ffc
#define PMU4368_PLL1_PC5_NDIV_INT_SHIFT		2
#define PMU4368_PLL1_PC5_NDIV_FRAC_MASK		0xfffff000
#define PMU4368_PLL1_PC5_NDIV_FRAC_SHIFT	12

/* PLL usage in 4387 */
#define PMU4387_PLL0_PC1_ICH2_MDIV_SHIFT	18
#define PMU4387_PLL0_PC1_ICH2_MDIV_MASK		0x07FC0000
#define PMU4387_PLL0_PC2_ICH3_MDIV_MASK		0x000001ff

/* PLL usage in 4388 */
#define PMU4388_APLL_NDIV_P			0x154u
#define PMU4388_APLL_NDIV_Q			0x1ffu
#define PMU4388_APLL_PDIV			0x3u
#define PMU4388_ARMPLL_I_NDIV_INT_MASK		0x01ff8000u
#define PMU4388_ARMPLL_I_NDIV_INT_SHIFT		15u

/* PLL usage in 4389 */
#define PMU4389_APLL_NDIV_P			0x154u
#define PMU4389_APLL_NDIV_Q			0x1ffu
#define PMU4389_APLL_PDIV			0x3u
#define PMU4389_ARMPLL_I_NDIV_INT_MASK		0x01ff8000u
#define PMU4389_ARMPLL_I_NDIV_INT_SHIFT		15u

#ifndef PMU_ARMPLL_PDIV
#define PMU_ARMPLL_PDIV				0xFFFFFFFFu
#endif /* PMU_ARMPLL_PDIV */

#define PMU_ARMPLL_NDIV_P_MASK			0xFFC00000u
#define PMU_ARMPLL_NDIV_Q_MASK			0x003FF000u

/* 5357 Chip specific ChipControl register bits */
#define CCTRL5357_EXTPA                 (1<<14) /* extPA in ChipControl 1, bit 14 */
#define CCTRL5357_ANT_MUX_2o3		(1<<15) /* 2o3 in ChipControl 1, bit 15 */
#define CCTRL5357_NFLASH		(1<<16) /* Nandflash in ChipControl 1, bit 16 */
/* 43217 Chip specific ChipControl register bits */
#define CCTRL43217_EXTPA_C0             (1<<13) /* core0 extPA in ChipControl 1, bit 13 */
#define CCTRL43217_EXTPA_C1             (1<<8)  /* core1 extPA in ChipControl 1, bit 8 */

#define PMU1_PLL0_CHIPCTL0		0
#define PMU1_PLL0_CHIPCTL1		1
#define PMU1_PLL0_CHIPCTL2		2

#define SOCDEVRAM_BP_ADDR		0x1E000000
#define SOCDEVRAM_ARM_ADDR		0x00800000

#define PMU_VREG0_I_SR_CNTL_EN_SHIFT		0
#define PMU_VREG0_DISABLE_PULLD_BT_SHIFT	2
#define PMU_VREG0_DISABLE_PULLD_WL_SHIFT	3
#define PMU_VREG0_CBUCKFSW_ADJ_SHIFT		8
#define PMU_VREG0_CBUCKFSW_ADJ_MASK			0x1F
#define PMU_VREG0_RAMP_SEL_SHIFT			13
#define PMU_VREG0_RAMP_SEL_MASK				0x7
#define PMU_VREG0_VFB_RSEL_SHIFT			17
#define PMU_VREG0_VFB_RSEL_MASK				3

#define PMU_VREG4_ADDR			4

#define PMU_VREG4_CLDO_PWM_SHIFT	4
#define PMU_VREG4_CLDO_PWM_MASK		0x7

#define PMU_VREG4_LPLDO1_SHIFT		15
#define PMU_VREG4_LPLDO1_MASK		0x7
#define PMU_VREG4_LPLDO1_1p20V		0
#define PMU_VREG4_LPLDO1_1p15V		1
#define PMU_VREG4_LPLDO1_1p10V		2
#define PMU_VREG4_LPLDO1_1p25V		3
#define PMU_VREG4_LPLDO1_1p05V		4
#define PMU_VREG4_LPLDO1_1p00V		5
#define PMU_VREG4_LPLDO1_0p95V		6
#define PMU_VREG4_LPLDO1_0p90V		7

#define PMU_VREG4_LPLDO2_LVM_SHIFT	18
#define PMU_VREG4_LPLDO2_LVM_MASK	0x7
#define PMU_VREG4_LPLDO2_HVM_SHIFT	21
#define PMU_VREG4_LPLDO2_HVM_MASK	0x7
#define PMU_VREG4_LPLDO2_LVM_HVM_MASK	0x3f
#define PMU_VREG4_LPLDO2_1p00V		0
#define PMU_VREG4_LPLDO2_1p15V		1
#define PMU_VREG4_LPLDO2_1p20V		2
#define PMU_VREG4_LPLDO2_1p10V		3
#define PMU_VREG4_LPLDO2_0p90V		4	/**< 4 - 7 is 0.90V */

#define PMU_VREG4_HSICLDO_BYPASS_SHIFT	27
#define PMU_VREG4_HSICLDO_BYPASS_MASK	0x1

#define PMU_VREG5_ADDR			5
#define PMU_VREG5_HSICAVDD_PD_SHIFT	6
#define PMU_VREG5_HSICAVDD_PD_MASK	0x1
#define PMU_VREG5_HSICDVDD_PD_SHIFT	11
#define PMU_VREG5_HSICDVDD_PD_MASK	0x1

/* 43228 chipstatus  reg bits */
#define	CST43228_OTP_PRESENT		0x2

/* 4360 Chip specific ChipControl register bits */
#define CCTRL4360_I2C_MODE			(1 << 0)
#define CCTRL4360_UART_MODE			(1 << 1)
#define CCTRL4360_SECI_MODE			(1 << 2)
#define CCTRL4360_BTSWCTRL_MODE			(1 << 3)
#define CCTRL4360_DISCRETE_FEMCTRL_MODE		(1 << 4)
#define CCTRL4360_DIGITAL_PACTRL_MODE		(1 << 5)
#define CCTRL4360_BTSWCTRL_AND_DIGPA_PRESENT	(1 << 6)
#define CCTRL4360_EXTRA_GPIO_MODE		(1 << 7)
#define CCTRL4360_EXTRA_FEMCTRL_MODE		(1 << 8)
#define CCTRL4360_BT_LGCY_MODE			(1 << 9)
#define CCTRL4360_CORE2FEMCTRL4_ON		(1 << 21)
#define CCTRL4360_SECI_ON_GPIO01		(1 << 24)

/* 4360 Chip specific Regulator Control register bits */
#define RCTRL4360_RFLDO_PWR_DOWN		(1 << 1)

/* 4360 PMU resources and chip status bits */
#define RES4360_REGULATOR          0
#define RES4360_ILP_AVAIL          1
#define RES4360_ILP_REQ            2
#define RES4360_XTAL_LDO_PU        3
#define RES4360_XTAL_PU            4
#define RES4360_ALP_AVAIL          5
#define RES4360_BBPLLPWRSW_PU      6
#define RES4360_HT_AVAIL           7
#define RES4360_OTP_PU             8
#define RES4360_AVB_PLL_PWRSW_PU   9
#define RES4360_PCIE_TL_CLK_AVAIL  10

#define CST4360_XTAL_40MZ                  0x00000001
#define CST4360_SFLASH                     0x00000002
#define CST4360_SPROM_PRESENT              0x00000004
#define CST4360_SFLASH_TYPE                0x00000004
#define CST4360_OTP_ENABLED                0x00000008
#define CST4360_REMAP_ROM                  0x00000010
#define CST4360_RSRC_INIT_MODE_MASK        0x00000060
#define CST4360_RSRC_INIT_MODE_SHIFT       5
#define CST4360_ILP_DIVEN                  0x00000080
#define CST4360_MODE_USB                   0x00000100
#define CST4360_SPROM_SIZE_MASK            0x00000600
#define CST4360_SPROM_SIZE_SHIFT           9
#define CST4360_BBPLL_LOCK                 0x00000800
#define CST4360_AVBBPLL_LOCK               0x00001000
#define CST4360_USBBBPLL_LOCK              0x00002000
#define CST4360_RSRC_INIT_MODE(cs)	((cs & CST4360_RSRC_INIT_MODE_MASK) >> \
						CST4360_RSRC_INIT_MODE_SHIFT)

#define CCTRL_4360_UART_SEL		0x2

#define CST4360_RSRC_INIT_MODE(cs)	((cs & CST4360_RSRC_INIT_MODE_MASK) >> \
						CST4360_RSRC_INIT_MODE_SHIFT)

#define PMU4360_CC1_GPIO7_OVRD          (1<<23) /* GPIO7 override */

/* GCI function sel values */
#define CC_FNSEL_HWDEF		(0u)
#define CC_FNSEL_SAMEASPIN	(1u)
#define CC_FNSEL_GPIO0		(2u)
#define CC_FNSEL_GPIO1		(3u)
#define CC_FNSEL_GCI0		(4u)
#define CC_FNSEL_GCI1		(5u)
#define CC_FNSEL_UART		(6u)
#define CC_FNSEL_SFLASH		(7u)
#define CC_FNSEL_SPROM		(8u)
#define CC_FNSEL_MISC0		(9u)
#define CC_FNSEL_MISC1		(10u)
#define CC_FNSEL_MISC2		(11u)
#define CC_FNSEL_IND		(12u)
#define CC_FNSEL_PDN		(13u)
#define CC_FNSEL_PUP		(14u)
#define CC_FNSEL_TRI		(15u)

#define CC_FNSEL_LAST	CC_FNSEL_TRI

/* 4387 GCI function sel values */
#define CC4387_FNSEL_FUART		(3u)
#define CC4387_FNSEL_DBG_UART		(6u)
#define CC4387_FNSEL_SPI		(7u)

/* FNSEL_8  corresponds to SFLASH for 4387/4388 */
#define CC_FNSEL_8		(8u)

/* Indices of PMU voltage regulator registers */
#if defined(VLSI_CTRL_REGS) && defined(SKIP_LEGACY_CC_API)
#define PMU_VREG_0	SI_CTRLREGS_INVALID
#define PMU_VREG_1	SI_CTRLREGS_INVALID
#define PMU_VREG_2	SI_CTRLREGS_INVALID
#define PMU_VREG_3	SI_CTRLREGS_INVALID
#define PMU_VREG_4	SI_CTRLREGS_INVALID
#define PMU_VREG_5	SI_CTRLREGS_INVALID
#define PMU_VREG_6	SI_CTRLREGS_INVALID
#define PMU_VREG_7	SI_CTRLREGS_INVALID
#define PMU_VREG_8	SI_CTRLREGS_INVALID
#define PMU_VREG_9	SI_CTRLREGS_INVALID
#define PMU_VREG_10	SI_CTRLREGS_INVALID
#define PMU_VREG_11	SI_CTRLREGS_INVALID
#define PMU_VREG_12	SI_CTRLREGS_INVALID
#define PMU_VREG_13	SI_CTRLREGS_INVALID
#define PMU_VREG_14	SI_CTRLREGS_INVALID
#define PMU_VREG_15	SI_CTRLREGS_INVALID
#define PMU_VREG_16	SI_CTRLREGS_INVALID
#else /* !VLSI_CTRL_REGS || !SKIP_LEGACY_CC_API */
#define PMU_VREG_0	(0u)
#define PMU_VREG_1	(1u)
#define PMU_VREG_2	(2u)
#define PMU_VREG_3	(3u)
#define PMU_VREG_4	(4u)
#define PMU_VREG_5	(5u)
#define PMU_VREG_6	(6u)
#define PMU_VREG_7	(7u)
#define PMU_VREG_8	(8u)
#define PMU_VREG_9	(9u)
#define PMU_VREG_10	(10u)
#define PMU_VREG_11	(11u)
#define PMU_VREG_12	(12u)
#define PMU_VREG_13	(13u)
#define PMU_VREG_14	(14u)
#define PMU_VREG_15	(15u)
#define PMU_VREG_16	(16u)
#endif /* !VLSI_CTRL_REGS || !SKIP_LEGACY_CC_API */

/* 4369 related VREG masks */
#define PMU_4369_VREG_5_MISCLDO_POWER_UP_MASK		(1u << 11u)
#define PMU_4369_VREG_5_MISCLDO_POWER_UP_SHIFT		11u
#define PMU_4369_VREG_5_LPLDO_POWER_UP_MASK		(1u << 27u)
#define PMU_4369_VREG_5_LPLDO_POWER_UP_SHIFT		27u
#define PMU_4369_VREG_5_LPLDO_OP_VLT_ADJ_CTRL_MASK	BCM_MASK32(23u, 20u)
#define PMU_4369_VREG_5_LPLDO_OP_VLT_ADJ_CTRL_SHIFT	20u
#define PMU_4369_VREG_5_MEMLPLDO_OP_VLT_ADJ_CTRL_MASK	BCM_MASK32(31, 28)
#define PMU_4369_VREG_5_MEMLPLDO_OP_VLT_ADJ_CTRL_SHIFT	28u

#define PMU_4369_VREG_6_MEMLPLDO_POWER_UP_MASK		(1u << 3u)
#define PMU_4369_VREG_6_MEMLPLDO_POWER_UP_SHIFT		3u

#define PMU_4369_VREG_7_PMU_FORCE_HP_MODE_MASK		(1u << 27u)
#define PMU_4369_VREG_7_PMU_FORCE_HP_MODE_SHIFT		27u
#define PMU_4369_VREG_7_WL_PMU_LP_MODE_MASK		(1u << 28u)
#define PMU_4369_VREG_7_WL_PMU_LP_MODE_SHIFT		28u
#define PMU_4369_VREG_7_WL_PMU_LV_MODE_MASK		(1u << 29u)
#define PMU_4369_VREG_7_WL_PMU_LV_MODE_SHIFT		29u

#define PMU_4388_VREG_7_ASR_PFM_FDWD_ZERO_ADJ_MASK	(0xfu << 27u)
#define PMU_4388_VREG_7_ASR_PFM_FDWD_ZERO_ADJ_SHIFT	27u

#define PMU_4369_VREG8_ASR_OVADJ_LPPFM_MASK		BCM_MASK32(4, 0)
#define PMU_4369_VREG8_ASR_OVADJ_LPPFM_SHIFT		0u

#define PMU_4369_VREG13_RSRC_EN0_ASR_MASK		BCM_MASK32(9, 9)
#define PMU_4369_VREG13_RSRC_EN0_ASR_SHIFT		9u
#define PMU_4369_VREG13_RSRC_EN1_ASR_MASK		BCM_MASK32(10, 10)
#define PMU_4369_VREG13_RSRC_EN1_ASR_SHIFT		10u
#define PMU_4369_VREG13_RSRC_EN2_ASR_MASK		BCM_MASK32(11, 11)
#define PMU_4369_VREG13_RSRC_EN2_ASR_SHIFT		11u

#define PMU_4369_VREG14_RSRC_EN_CSR_MASK0_MASK		(1u << 23u)
#define PMU_4369_VREG14_RSRC_EN_CSR_MASK0_SHIFT		23u

#define PMU_4369_VREG16_RSRC0_CBUCK_MODE_MASK		BCM_MASK32(2, 0)
#define PMU_4369_VREG16_RSRC0_CBUCK_MODE_SHIFT		0u
#define PMU_4369_VREG16_RSRC0_ABUCK_MODE_MASK		BCM_MASK32(17, 15)
#define PMU_4369_VREG16_RSRC0_ABUCK_MODE_SHIFT		15u
#define PMU_4369_VREG16_RSRC1_ABUCK_MODE_MASK		BCM_MASK32(20, 18)
#define PMU_4369_VREG16_RSRC1_ABUCK_MODE_SHIFT		18u
#define PMU_4369_VREG16_RSRC2_ABUCK_MODE_MASK		BCM_MASK32(23, 21)
#define PMU_4369_VREG16_RSRC2_ABUCK_MODE_SHIFT		21u

#define VREG0_4378_CSR_VOLT_ADJ_PWM_MASK		0x00001F00u
#define VREG0_4378_CSR_VOLT_ADJ_PWM_SHIFT		8u

// Please leave this UNRELEASEDCHIP MOG wrapper in place even if there is nothing inside it

#define PMU_4387_VREG1_CSR_OVERI_DIS_MASK		(1u << 22u)
#define PMU_4387_VREG6_WL_PMU_LV_MODE_MASK		(0x00000002u)
#define PMU_4387_VREG6_MEMLDO_PU_MASK			(0x00000008u)
#define PMU_4387_VREG8_ASR_OVERI_DIS_MASK		(1u << 7u)

#define PMU_4388_VREG6_WL_PMU_LV_MODE_SHIFT		(1u)
#define PMU_4388_VREG6_WL_PMU_LV_MODE_MASK		(1u << PMU_4388_VREG6_WL_PMU_LV_MODE_SHIFT)
#define PMU_4388_VREG6_MEMLDO_PU_SHIFT			(3u)
#define PMU_4388_VREG6_MEMLDO_PU_MASK			(1u << PMU_4388_VREG6_MEMLDO_PU_SHIFT)

#define PMU_4389_VREG6_WL_PMU_LV_MODE_SHIFT		(1u)
#define PMU_4389_VREG6_WL_PMU_LV_MODE_MASK		(1u << PMU_4389_VREG6_WL_PMU_LV_MODE_SHIFT)
#define PMU_4389_VREG6_MEMLDO_PU_SHIFT			(3u)
#define PMU_4389_VREG6_MEMLDO_PU_MASK			(1u << PMU_4389_VREG6_MEMLDO_PU_SHIFT)

#define PMU_4389_VREG1_CSR_OVERI_DIS_MASK		(1u << 22u)
#define PMU_4389_VREG8_ASR_OVERI_DIS_MASK		(1u << 13u)

#define PMU_4397_VREG7_ASR_OVADJ_PWM_SHIFT		(20u)
#define PMU_4397_VREG7_ASR_OVADJ_PWM_MASK		(31u << PMU_4397_VREG7_ASR_OVADJ_PWM_SHIFT)
#define PMU_4397_VREG6_MEMLDO_PU_SHIFT			(3u)
#define PMU_4397_VREG6_MEMLDO_PU_MASK			(1u << PMU_4397_VREG6_MEMLDO_PU_SHIFT)

#define PMU_VREG13_ASR_OVADJ_PWM_MASK			(0x001F0000u)
#define PMU_VREG13_ASR_OVADJ_PWM_SHIFT			(16u)

#define PMU_VREG14_RSRC_EN_ASR_PWM_PFM_MASK		(1u << 18u)
#define PMU_VREG14_RSRC_EN_ASR_PWM_PFM_SHIFT		(18u)

#define ABUCK_VOLT_SW_DEFAULT_4387			(0x1Fu) /* 1.00V */
#define CBUCK_VOLT_SW_DEFAULT_4387			(0xFu)  /* 0.68V */
#define CBUCK_VOLT_NON_LVM				(0x13u) /* 0.76V */

#define CBUCK_VOLT_HW_DEFAULT				(0x11u)	/* 0.72V */

/* TODO: 4397 Add InitVal approach for 4397 as well */
#define ABUCK_VOLT_SW_DEFAULT_4397_p92		(0x5u) /* 0.92V */
#define ABUCK_VOLT_SW_DEFAULT_4397			(0x0u) /* 0.87V */
#define CBUCK_VOLT_SW_DEFAULT_4397			(0x15u) /* 0.83V */

#define CC_GCI1_REG					(0x1)

#define FORCE_CLK_ON                                                    1
#define FORCE_CLK_OFF                                                   0

#define RES4381_ARMCLK_AVAIL		0x8000000	/* #27 bit */
#define RES4381_HT_AVAIL		0x10000000	/* #28 bit */

/*
 * PMU VREG Definitions:
 *   http://confluence.broadcom.com/display/WLAN/BCM4369+PMU+Vreg+Control+Register
 */
/* PMU VREG4 */
#define PMU_28NM_VREG4_WL_LDO_CNTL_EN				(0x1 << 10)

/* PMU VREG6 */
#define PMU_28NM_VREG6_BTLDO3P3_PU				(0x1 << 12)
#define PMU_4387_VREG6_BTLDO3P3_PU				(0x1 << 8)

/* PMU resources */
#define RES4347_XTAL_PU			6
#define RES4347_CORE_RDY_DIG		17
#define RES4347_CORE_RDY_AUX		18
#define RES4347_CORE_RDY_MAIN		22

// Please leave this UNRELEASEDCHIP MOG wrapper in place even if there is nothing inside it

/* 0: BToverPCIe, 1: BToverUART */
#define CST4378_CHIPMODE_BTOU(cs)	(((cs) & (1 << 6)) != 0)
#define CST4378_CHIPMODE_BTOP(cs)	(((cs) & (1 << 6)) == 0)

#define CST_SFLASH_PRESENT		0x00000010U

#define CST4387_CHIPMODE_BTOU(cs)	(((cs) & (1 << 6)) != 0)
#define CST4387_CHIPMODE_BTOP(cs)	(((cs) & (1 << 6)) == 0)
#define CST4387_SPROM_PRESENT		0x00000010

/* GCI chip status */
#define GCI_CS_4387_FLL1MHZ_LOCK_MASK	(1 << 1)

#define GCI_CS_4387_FLL1MHZ_DAC_OUT_SHIFT	(16u)
#define GCI_CS_4387_FLL1MHZ_DAC_OUT_MASK	(0x00ff0000u)
#define GCI_CS_4389_FLL1MHZ_DAC_OUT_SHIFT	(16u)
#define GCI_CS_4389_FLL1MHZ_DAC_OUT_MASK	(0x00ff0000u)

/* GCI chip control registers */
#define GCI_CC7_AAON_BYPASS_PWRSW_SEL          13
#define GCI_CC7_AAON_BYPASS_PWRSW_SEQ_ON       14

#define GCI_CC11_PRISEL_SHIFT			(26u)

/* 4368 GCI chip control registers */
#define GCI_CC7_PRISEL_MASK			(1 << 8 | 1 << 9)
#define GCI_CC12_PRISEL_MASK			(1 << 0 | 1 << 1)
#define GCI_CC12_PRISEL_SHIFT			0
#define GCI_CC12_DMASK_MASK			(0x3ff << 10)
#define GCI_CC16_ANT_SHARE_MASK		(1 << 16 | 1 << 17)

#define CC2_4378_USE_WLAN_BP_CLK_ON_REQ_MASK		(1u << 15u)
#define CC2_4378_USE_WLAN_BP_CLK_ON_REQ_SHIFT		15u
#define CC2_4378_USE_CMN_BP_CLK_ON_REQ_MASK		(1u << 16u)
#define CC2_4378_USE_CMN_BP_CLK_ON_REQ_SHIFT		16u

#define CC2_4387_MAIN_MEMLPLDO_VDDB_OFF_MASK		(1u << 12u)
#define CC2_4387_MAIN_MEMLPLDO_VDDB_OFF_SHIFT		12u
#define CC2_4387_AUX_MEMLPLDO_VDDB_OFF_MASK		(1u << 13u)
#define CC2_4387_AUX_MEMLPLDO_VDDB_OFF_SHIFT		13u
#define CC2_4387_MAIN_VDDRET_ON_MASK			(1u << 15u)
#define CC2_4387_MAIN_VDDRET_ON_SHIFT			15u
#define CC2_4387_AUX_VDDRET_ON_MASK			(1u << 16u)
#define CC2_4387_AUX_VDDRET_ON_SHIFT			16u
#define CC2_4387_GCI2WAKE_MASK				(1u << 31u)
#define CC2_4387_GCI2WAKE_SHIFT				31u
#define CC2_4387_SDIO_AOS_WAKEUP_MASK			(1u << 24u)
#define CC2_4387_SDIO_AOS_WAKEUP_SHIFT			24u
#define CC4_4387_LHL_TIMER_SELECT			(1u << 0u)
#define CC6_4387_PWROK_WDT_EN_IN_MASK			(1u << 6u)
#define CC6_4387_PWROK_WDT_EN_IN_SHIFT			6u
#define CC6_4387_SDIO_AOS_CHIP_WAKEUP_MASK		(1u << 24u)
#define CC6_4387_SDIO_AOS_CHIP_WAKEUP_SHIFT		24u

#define CC2_4387_USE_WLAN_BP_CLK_ON_REQ_MASK		(1u << 15u)
#define CC2_4387_USE_WLAN_BP_CLK_ON_REQ_SHIFT		15u
#define CC2_4387_USE_CMN_BP_CLK_ON_REQ_MASK		(1u << 16u)
#define CC2_4387_USE_CMN_BP_CLK_ON_REQ_SHIFT		16u

#define CC2_4388_MAIN_MEMLPLDO_VDDB_OFF_MASK		(1u << 12u)
#define CC2_4388_MAIN_MEMLPLDO_VDDB_OFF_SHIFT		(12u)
#define CC2_4388_AUX_MEMLPLDO_VDDB_OFF_MASK		(1u << 13u)
#define CC2_4388_AUX_MEMLPLDO_VDDB_OFF_SHIFT		(13u)
#define CC2_4388_MAIN_VDDRET_ON_MASK			(1u << 15u)
#define CC2_4388_MAIN_VDDRET_ON_SHIFT			(15u)
#define CC2_4388_AUX_VDDRET_ON_MASK			(1u << 16u)
#define CC2_4388_AUX_VDDRET_ON_SHIFT			(16u)
#define CC2_4388_GCI2WAKE_MASK				(1u << 31u)
#define CC2_4388_GCI2WAKE_SHIFT				(31u)
#define CC2_4388_SDIO_AOS_WAKEUP_MASK			(1u << 24u)
#define CC2_4388_SDIO_AOS_WAKEUP_SHIFT			(24u)
#define CC4_4388_LHL_TIMER_SELECT			(1u << 0u)
#define CC6_4388_PWROK_WDT_EN_IN_MASK			(1u << 6u)
#define CC6_4388_PWROK_WDT_EN_IN_SHIFT			(6u)
#define CC6_4388_SDIO_AOS_CHIP_WAKEUP_MASK		(1u << 24u)
#define CC6_4388_SDIO_AOS_CHIP_WAKEUP_SHIFT		(24u)

#define CC2_4388_USE_WLAN_BP_CLK_ON_REQ_MASK		(1u << 15u)
#define CC2_4388_USE_WLAN_BP_CLK_ON_REQ_SHIFT		(15u)
#define CC2_4388_USE_CMN_BP_CLK_ON_REQ_MASK		(1u << 16u)
#define CC2_4388_USE_CMN_BP_CLK_ON_REQ_SHIFT		(16u)

#define CC2_4389_MAIN_MEMLPLDO_VDDB_OFF_MASK		(1u << 12u)
#define CC2_4389_MAIN_MEMLPLDO_VDDB_OFF_SHIFT		(12u)
#define CC2_4389_AUX_MEMLPLDO_VDDB_OFF_MASK		(1u << 13u)
#define CC2_4389_AUX_MEMLPLDO_VDDB_OFF_SHIFT		(13u)
#define CC2_4389_MAIN_VDDRET_ON_MASK			(1u << 15u)
#define CC2_4389_MAIN_VDDRET_ON_SHIFT			(15u)
#define CC2_4389_AUX_VDDRET_ON_MASK			(1u << 16u)
#define CC2_4389_AUX_VDDRET_ON_SHIFT			(16u)
#define CC2_4389_GCI2WAKE_MASK				(1u << 31u)
#define CC2_4389_GCI2WAKE_SHIFT				(31u)
#define CC2_4389_SDIO_AOS_WAKEUP_MASK			(1u << 24u)
#define CC2_4389_SDIO_AOS_WAKEUP_SHIFT			(24u)
#define CC4_4389_LHL_TIMER_SELECT			(1u << 0u)
#define CC6_4389_PWROK_WDT_EN_IN_MASK			(1u << 6u)
#define CC6_4389_PWROK_WDT_EN_IN_SHIFT			(6u)
#define CC6_4389_SDIO_AOS_CHIP_WAKEUP_MASK		(1u << 24u)
#define CC6_4389_SDIO_AOS_CHIP_WAKEUP_SHIFT		(24u)

#define CC2_4389_USE_WLAN_BP_CLK_ON_REQ_MASK		(1u << 15u)
#define CC2_4389_USE_WLAN_BP_CLK_ON_REQ_SHIFT		(15u)
#define CC2_4389_USE_CMN_BP_CLK_ON_REQ_MASK		(1u << 16u)
#define CC2_4389_USE_CMN_BP_CLK_ON_REQ_SHIFT		(16u)

#define PCIE_GPIO1_GPIO_PIN    CC_GCI_GPIO_0
#define PCIE_PERST_GPIO_PIN	CC_GCI_GPIO_1
#define PCIE_CLKREQ_GPIO_PIN	CC_GCI_GPIO_2

#define VREG5_4387_MISCLDO_PU_MASK				(0x00000800u)
#define VREG5_4387_MISCLDO_PU_SHIFT				(11u)

#define VREG5_4387_MEMLPLDO_ADJ_MASK				0xF0000000
#define VREG5_4387_MEMLPLDO_ADJ_SHIFT				28
#define VREG5_4387_LPLDO_ADJ_MASK				0x00F00000
#define VREG5_4387_LPLDO_ADJ_SHIFT				20
#define VREG5_4387_MISC_LDO_ADJ_MASK				(0xfu)
#define VREG5_4387_MISC_LDO_ADJ_SHIFT				(0)

/* misc ldo voltage
 * https://drive.google.com/file/d/1JjvNhp-RIXJBtw99M4w5ww4MmDsBJbpD
 */
#define	PMU_VREG5_MISC_LDO_VOLT_0p931	(0x7u)		/* 0.93125 v */
#define	PMU_VREG5_MISC_LDO_VOLT_0p912	(0x6u)		/* 0.91250 v */
#define	PMU_VREG5_MISC_LDO_VOLT_0p893	(0x5u)		/* 0.89375 v */
#define	PMU_VREG5_MISC_LDO_VOLT_0p875	(0x4u)		/* 0.87500 v */
#define	PMU_VREG5_MISC_LDO_VOLT_0p856	(0x3u)		/* 0.85625 v */
#define	PMU_VREG5_MISC_LDO_VOLT_0p837	(0x2u)		/* 0.83750 v */
#define	PMU_VREG5_MISC_LDO_VOLT_0p818	(0x1u)		/* 0.81875 v */
#define	PMU_VREG5_MISC_LDO_VOLT_0p800	(0)		/* 0.80000 v */
#define	PMU_VREG5_MISC_LDO_VOLT_0p781	(0xfu)		/* 0.78125 v */
#define	PMU_VREG5_MISC_LDO_VOLT_0p762	(0xeu)		/* 0.76250 v */
#define	PMU_VREG5_MISC_LDO_VOLT_0p743	(0xdu)		/* 0.74375 v */
#define	PMU_VREG5_MISC_LDO_VOLT_0p725	(0xcu)		/* 0.72500 v */
#define	PMU_VREG5_MISC_LDO_VOLT_0p706	(0xbu)		/* 0.70625 v */
#define	PMU_VREG5_MISC_LDO_VOLT_0p687	(0xau)		/* 0.68750 v */
#define	PMU_VREG5_MISC_LDO_VOLT_0p668	(0x9u)		/* 0.66875 v */
#define	PMU_VREG5_MISC_LDO_VOLT_0p650	(0x8u)		/* 0.65000 v */

/* lpldo/memlpldo voltage */
#define	PMU_VREG5_LPLDO_VOLT_0_88	0xf	/* 0.88v */
#define	PMU_VREG5_LPLDO_VOLT_0_86	0xe	/* 0.86v */
#define	PMU_VREG5_LPLDO_VOLT_0_84	0xd	/* 0.84v */
#define	PMU_VREG5_LPLDO_VOLT_0_82	0xc	/* 0.82v */
#define	PMU_VREG5_LPLDO_VOLT_0_80	0xb	/* 0.80v */
#define	PMU_VREG5_LPLDO_VOLT_0_78	0xa	/* 0.78v */
#define	PMU_VREG5_LPLDO_VOLT_0_76	0x9	/* 0.76v */
#define	PMU_VREG5_LPLDO_VOLT_0_74	0x8	/* 0.74v */
#define	PMU_VREG5_LPLDO_VOLT_0_72	0x7	/* 0.72v */
#define	PMU_VREG5_LPLDO_VOLT_1_10	0x6	/* 1.10v */
#define	PMU_VREG5_LPLDO_VOLT_1_00	0x5	/* 1.00v */
#define	PMU_VREG5_LPLDO_VOLT_0_98	0x4	/* 0.98v */
#define	PMU_VREG5_LPLDO_VOLT_0_96	0x3	/* 0.96v */
#define	PMU_VREG5_LPLDO_VOLT_0_94	0x2	/* 0.94v */
#define	PMU_VREG5_LPLDO_VOLT_0_92	0x1	/* 0.92v */
#define	PMU_VREG5_LPLDO_VOLT_0_90	0x0	/* 0.90v */

/* =========== LHL regs =========== */
/* 4378 LHL register settings */
#define LHL4378_CSR_OVERI_DIS_DWN_CNT		5u
#define LHL4378_CSR_MODE_DWN_CNT		5u
#define LHL4378_CSR_ADJ_DWN_CNT		5u

#define LHL4378_CSR_OVERI_DIS_UP_CNT		1u
#define LHL4378_CSR_MODE_UP_CNT		1u
#define LHL4378_CSR_ADJ_UP_CNT			1u

#define LHL4378_VDDC_SW_DIS_DWN_CNT		3u
#define LHL4378_ASR_ADJ_DWN_CNT		3u
#define LHL4378_HPBG_CHOP_DIS_DWN_CNT		0

#define LHL4378_VDDC_SW_DIS_UP_CNT		3u
#define LHL4378_ASR_ADJ_UP_CNT			1u
#define LHL4378_HPBG_CHOP_DIS_UP_CNT		0

#define LHL4378_ASR_MANUAL_MODE_DWN_CNT	5u
#define LHL4378_ASR_MODE_SEL_DWN_CNT		5u
#define LHL4378_ASR_LPPFM_MODE_DWN_CNT		5u
#define LHL4378_ASR_CLK4M_DIS_DWN_CNT		0

#define LHL4378_ASR_MANUAL_MODE_UP_CNT		1u
#define LHL4378_ASR_MODE_SEL_UP_CNT		1u
#define LHL4378_ASR_LPPFM_MODE_UP_CNT		1u
#define LHL4378_ASR_CLK4M_DIS_UP_CNT		0

#define LHL4378_PFM_PWR_SLICE_DWN_CNT		5u
#define LHL4378_ASR_OVERI_DIS_DWN_CNT		5u
#define LHL4378_SRBG_REF_SEL_DWN_CNT		5u
#define LHL4378_HPBG_PU_EN_DWN_CNT		6u

#define LHL4378_PFM_PWR_SLICE_UP_CNT		1u
#define LHL4378_ASR_OVERI_DIS_UP_CNT		1u
#define LHL4378_SRBG_REF_SEL_UP_CNT		1u
#define LHL4378_HPBG_PU_EN_UP_CNT		0

#define	LHL4378_CSR_TRIM_ADJ_CNT_SHIFT		(16u)
#define	LHL4378_CSR_TRIM_ADJ_CNT_MASK		(0x3Fu << LHL4378_CSR_TRIM_ADJ_CNT_SHIFT)
#define LHL4378_CSR_TRIM_ADJ_DWN_CNT		0
#define LHL4378_CSR_TRIM_ADJ_UP_CNT		0

#define	LHL4378_ASR_TRIM_ADJ_CNT_SHIFT		(0u)
#define	LHL4378_ASR_TRIM_ADJ_CNT_MASK		(0x3Fu << LHL4378_ASR_TRIM_ADJ_CNT_SHIFT)
#define LHL4378_ASR_TRIM_ADJ_UP_CNT		0
#define LHL4378_ASR_TRIM_ADJ_DWN_CNT		0

#define LHL4378_PWRSW_EN_DWN_CNT		0
#define LHL4378_SLB_EN_DWN_CNT			2u
#define LHL4378_ISO_EN_DWN_CNT			1u

#define LHL4378_VMUX_ASR_SEL_DWN_CNT		4u

#define LHL4378_PWRSW_EN_UP_CNT		6u
#define LHL4378_SLB_EN_UP_CNT			4u
#define LHL4378_ISO_EN_UP_CNT			5u

#define LHL4378_VMUX_ASR_SEL_UP_CNT		2u

#define LHL4387_VMUX_ASR_SEL_DWN_CNT		(8u)
#define LHL4387_VMUX_ASR_SEL_UP_CNT		(0x14u)

/* 4387 LHL register settings for top off mode */
#define LHL4387_TO_CSR_OVERI_DIS_DWN_CNT	3u
#define LHL4387_TO_CSR_MODE_DWN_CNT		3u
#define LHL4387_TO_CSR_ADJ_DWN_CNT		0

#define LHL4387_TO_CSR_OVERI_DIS_UP_CNT	1u
#define LHL4387_TO_CSR_MODE_UP_CNT		1u
#define LHL4387_TO_CSR_ADJ_UP_CNT		0

#define LHL4387_TO_VDDC_SW_DIS_DWN_CNT		4u
#define LHL4387_TO_ASR_ADJ_DWN_CNT		3u
#define LHL4387_TO_LP_MODE_DWN_CNT		6u
#define LHL4387_TO_HPBG_CHOP_DIS_DWN_CNT	3u

#define LHL4387_TO_VDDC_SW_DIS_UP_CNT		0
#define LHL4387_TO_ASR_ADJ_UP_CNT		1u
#define LHL4387_TO_LP_MODE_UP_CNT		0
#define LHL4387_TO_HPBG_CHOP_DIS_UP_CNT	1u

#define LHL4387_TO_ASR_MANUAL_MODE_DWN_CNT	3u
#define LHL4387_TO_ASR_MODE_SEL_DWN_CNT	3u
#define LHL4387_TO_ASR_LPPFM_MODE_DWN_CNT	3u
#define LHL4387_TO_ASR_CLK4M_DIS_DWN_CNT	3u

#define LHL4387_TO_ASR_MANUAL_MODE_UP_CNT	1u
#define LHL4387_TO_ASR_MODE_SEL_UP_CNT		1u
#define LHL4387_TO_ASR_LPPFM_MODE_UP_CNT	1u
#define LHL4387_TO_ASR_CLK4M_DIS_UP_CNT	1u

#define LHL4387_TO_PFM_PWR_SLICE_DWN_CNT	3u
#define LHL4387_TO_ASR_OVERI_DIS_DWN_CNT	3u
#define LHL4387_TO_SRBG_REF_SEL_DWN_CNT	3u
#define LHL4387_TO_HPBG_PU_EN_DWN_CNT		4u

#define LHL4387_TO_PFM_PWR_SLICE_UP_CNT	1u
#define LHL4387_TO_ASR_OVERI_DIS_UP_CNT	1u
#define LHL4387_TO_SRBG_REF_SEL_UP_CNT		1u
#define LHL4387_TO_HPBG_PU_EN_UP_CNT		1u

#define LHL4387_TO_PWRSW_EN_DWN_CNT		0
#define LHL4387_TO_SLB_EN_DWN_CNT		4u
#define LHL4387_TO_ISO_EN_DWN_CNT		2u
#define LHL4387_TO_TOP_SLP_EN_DWN_CNT		0

#define LHL4387_TO_PWRSW_EN_UP_CNT		0x16u
#define LHL4387_TO_SLB_EN_UP_CNT		0xeu
#define LHL4387_TO_ISO_EN_UP_CNT		0x10u
#define LHL4387_TO_TOP_SLP_EN_UP_CNT		2u

/* MacResourceReqTimer0/1 */
#define MAC_RSRC_REQ_TIMER_INT_ENAB_SHIFT	24
#define MAC_RSRC_REQ_TIMER_FORCE_ALP_SHIFT	26
#define MAC_RSRC_REQ_TIMER_FORCE_HT_SHIFT	27
#define MAC_RSRC_REQ_TIMER_FORCE_HQ_SHIFT	28
#define MAC_RSRC_REQ_TIMER_CLKREQ_GRP_SEL_SHIFT	29

/* for pmu rev32 and higher */
#define PMU32_MAC_MAIN_RSRC_REQ_TIMER	((1u << MAC_RSRC_REQ_TIMER_INT_ENAB_SHIFT) |	\
					 (1u << MAC_RSRC_REQ_TIMER_FORCE_ALP_SHIFT) |	\
					 (1u << MAC_RSRC_REQ_TIMER_FORCE_HT_SHIFT) |	\
					 (1u << MAC_RSRC_REQ_TIMER_FORCE_HQ_SHIFT) |	\
					 (1u << MAC_RSRC_REQ_TIMER_CLKREQ_GRP_SEL_SHIFT))

#define PMU32_MAC_AUX_RSRC_REQ_TIMER	((1u << MAC_RSRC_REQ_TIMER_INT_ENAB_SHIFT) |	\
					 (1u << MAC_RSRC_REQ_TIMER_FORCE_ALP_SHIFT) |	\
					 (1u << MAC_RSRC_REQ_TIMER_FORCE_HT_SHIFT) |	\
					 (1u << MAC_RSRC_REQ_TIMER_FORCE_HQ_SHIFT) |	\
					 (1u << MAC_RSRC_REQ_TIMER_CLKREQ_GRP_SEL_SHIFT))

/* for pmu rev38 and higher */
#define PMU32_MAC_SCAN_RSRC_REQ_TIMER	((1u << MAC_RSRC_REQ_TIMER_INT_ENAB_SHIFT) |	\
					 (1u << MAC_RSRC_REQ_TIMER_FORCE_ALP_SHIFT) |	\
					 (1u << MAC_RSRC_REQ_TIMER_FORCE_HT_SHIFT) |	\
					 (1u << MAC_RSRC_REQ_TIMER_FORCE_HQ_SHIFT) |	\
					 (1u << MAC_RSRC_REQ_TIMER_CLKREQ_GRP_SEL_SHIFT))

#define PMU_MACCORE_0_RES_REQ_TIMER		0x1d000000
#define PMU_MACCORE_0_RES_REQ_MASK		0x5FF2364F

#define PMU_MACCORE_1_RES_REQ_TIMER		0x1d000000
#define PMU_MACCORE_1_RES_REQ_MASK		0x5FF2364F

/* defines to detect active host interface in use */
#define CHIP_HOSTIF_PCIEMODE	0x1
#define CHIP_HOSTIF_USBMODE	0x2
#define CHIP_HOSTIF_SDIOMODE	0x4
#define CHIP_HOSTIF_PCIE(sih)	(si_chip_hostif(sih) == CHIP_HOSTIF_PCIEMODE)
#define CHIP_HOSTIF_USB(sih)	(si_chip_hostif(sih) == CHIP_HOSTIF_USBMODE)
#define CHIP_HOSTIF_SDIO(sih)	(si_chip_hostif(sih) == CHIP_HOSTIF_SDIOMODE)

#define CR4_4335_RAM_BASE                    (0x180000)
#define CR4_4345_LT_C0_RAM_BASE              (0x1b0000)
#define CR4_4345_GE_C0_RAM_BASE              (0x198000)
#define CR4_4349_RAM_BASE                    (0x180000)
#define CR4_4349_RAM_BASE_FROM_REV_9         (0x160000)
#define CR4_4350_RAM_BASE                    (0x180000)
#define CR4_4360_RAM_BASE                    (0x0)
#define CR4_43602_RAM_BASE                   (0x180000)
#define CR4_4347_RAM_BASE                    (0x170000)
#define CR4_4362_RAM_BASE                    (0x170000)
#define CR4_4364_RAM_BASE                    (0x160000)
#define CR4_4369_RAM_BASE                    (0x170000)
#define CR4_4377_RAM_BASE                    (0x170000)
#define CR4_43751_RAM_BASE                   (0x170000)
#define CR4_43752_RAM_BASE                   (0x170000)
#define CR4_43756_RAM_BASE                   (0x170000)
#define CR4_43711_RAM_BASE                   (0x170000)
#define CR4_4376_RAM_BASE                    (0x352000)
#define CR4_4378_RAM_BASE                    (0x352000)
#define CR4_4381_RAM_BASE                    (0x740000)
#define CR4_4382_RAM_BASE                    (0x700000)
// Please leave this UNRELEASEDCHIP MOG wrapper in place even if there is nothing inside it
/* Based on the 4383 BackPlane Acrhitecture, RAM address base updated as 0x6E_0000 */
#define CR4_4381_RAM_BASE                    (0x740000)
#define CR4_4383_RAM_BASE                    (0x6E0000)
#define CR4_4384_RAM_BASE                    (0x700000)
#define CR4_4387_RAM_BASE                    (0x740000)
#define CR4_4385_RAM_BASE                    (0x740000)
#define CA7_4385_RAM_BASE                    (0x200000)
#define CA7_4388_RAM_BASE                    (0x200000)
#define CA7_4389_RAM_BASE                    (0x200000)
#define CA7_4390_RAM_BASE                    (0x2A0000)
#define CA7_4399_RAM_BASE                    (0x2A0000)

/* Coex CPU memory geometry */
#define CXCPU_4390_ITCM_BASE			(0x1a000000u)
#define CXCPU_4390_ITCM_SIZE			(98304u)
#define CXCPU_4390_DTCM_BASE			(0x1a018000u)
#define CXCPU_4390_DTCM_SIZE			(24576u)
// Please leave this UNRELEASEDCHIP MOG wrapper in place even if there is nothing inside it

/* Physical memory in 4388a0 HWA is 64KB (8192 x 64 bits) even though
 * the memory space allows 192KB (0x1850_0000 - 0x1852_FFFF)
 */
#define HWA_MEM_BASE_4388			(0x18520000u)
#define HWA_MEM_SIZE_4388			(0x10000u)

// Please leave this UNRELEASEDCHIP MOG wrapper in place even if there is nothing inside it

/* TODO: Update the right value after confirmation with RTL, Ucode.
*/
/* SR Control0 bits for 4383 */
#define SR0_4383_SR_ENG_EN_MASK		0x1
#define SR0_4383_SR_ENG_EN_SHIFT	0
#define SR0_4383_SR_ENG_CLK_EN		(1 << 1)
#define SR0_4383_RSRC_TRIGGER		(0xC << 2)
#define SR0_4383_WD_MEM_MIN_DIV		(0x2 << 6)
#define SR0_4383_WD_MEM_MIN_DIV_AUX	(0x4 << 6)
#define SR0_4383_INVERT_SR_CLK		(1 << 11)
#define SR0_4383_MEM_STBY_ALLOW		(1 << 16)
#define SR0_4383_ENABLE_SR_ILP		(1 << 17)
#define SR0_4383_ENABLE_SR_ALP		(1 << 18)
#define SR0_4383_ENABLE_SR_HT		(1 << 19)
#define SR0_4383_ALLOW_PIC		(3 << 20)
#define SR0_4383_ENB_PMU_MEM_DISABLE	(1 << 30)

/* srcontrol2 reg has to be programmed with asm download address
 * It is a 14 bit address but with RAM address range in 0x8yyyyy
 * additionally bit 15 gets set when address is derived as
 * addr >> 9 in sr_asm_addr. This by default enables sr self test
 * feature which is not yet ready for 4383. Hence masking it
 */
#define SR_ASM_ADDR_DIG_4383A0_SRCTRL2	(0x0b8000)

/* PMU Rev >= 30 */
#define PMU30_ALPCLK_ONEMHZ_ENAB			0x80000000u
#define PMU30_ALPCLK_ONEMHZ_FPGAMOD_ENAB    0x80022222u

/* 4387 PMU Chip Control Registers */
#define PMUCCTL03_4387_XTAL_CORESIZE_PMOS_NORMAL_MASK		0x001F8000
#define PMUCCTL03_4387_XTAL_CORESIZE_PMOS_NORMAL_SHIFT		15
#define PMUCCTL03_4387_XTAL_CORESIZE_PMOS_NORMAL_VAL		0x3F

#define PMUCCTL03_4387_XTAL_CORESIZE_NMOS_NORMAL_MASK		0x07E00000
#define PMUCCTL03_4387_XTAL_CORESIZE_NMOS_NORMAL_SHIFT		21
#define PMUCCTL03_4387_XTAL_CORESIZE_NMOS_NORMAL_VAL		0x3F

#define PMUCCTL03_4387_XTAL_SEL_BIAS_RES_NORMAL_MASK		0x38000000
#define PMUCCTL03_4387_XTAL_SEL_BIAS_RES_NORMAL_SHIFT		27
#define PMUCCTL03_4387_XTAL_SEL_BIAS_RES_NORMAL_VAL			0x0

#define PMUCCTL00_4387_XTAL_CORESIZE_BIAS_ADJ_NORMAL_MASK	0x00000FC0
#define PMUCCTL00_4387_XTAL_CORESIZE_BIAS_ADJ_NORMAL_SHIFT	6
#define PMUCCTL00_4387_XTAL_CORESIZE_BIAS_ADJ_NORMAL_VAL	0x5

#define PMUCCTL00_4387_XTAL_RES_BYPASS_NORMAL_MASK			0x00038000
#define PMUCCTL00_4387_XTAL_RES_BYPASS_NORMAL_SHIFT			15
#define PMUCCTL00_4387_XTAL_RES_BYPASS_NORMAL_VAL			0x7

/* GPIO pins */
#define CC_PIN_GPIO_00	(0u)
#define CC_PIN_GPIO_01	(1u)
#define CC_PIN_GPIO_02	(2u)
#define CC_PIN_GPIO_03	(3u)
#define CC_PIN_GPIO_04	(4u)
#define CC_PIN_GPIO_05	(5u)
#define CC_PIN_GPIO_06	(6u)
#define CC_PIN_GPIO_07	(7u)
#define CC_PIN_GPIO_08	(8u)
#define CC_PIN_GPIO_09	(9u)
#define CC_PIN_GPIO_10	(10u)
#define CC_PIN_GPIO_11	(11u)
#define CC_PIN_GPIO_12	(12u)
#define CC_PIN_GPIO_13	(13u)
#define CC_PIN_GPIO_14	(14u)
#define CC_PIN_GPIO_15	(15u)
#define CC_PIN_GPIO_16	(16u)
#define CC_PIN_GPIO_17	(17u)
#define CC_PIN_GPIO_18	(18u)
#define CC_PIN_GPIO_19	(19u)
#define CC_PIN_GPIO_20	(20u)
#define CC_PIN_GPIO_21	(21u)
#define CC_PIN_GPIO_22	(22u)
#define CC_PIN_GPIO_23	(23u)
#define CC_PIN_GPIO_24	(24u)
#define CC_PIN_GPIO_25	(25u)
#define CC_PIN_GPIO_26	(26u)
#define CC_PIN_GPIO_27	(27u)
#define CC_PIN_GPIO_28	(28u)
#define CC_PIN_GPIO_29	(29u)
#define CC_PIN_GPIO_30	(30u)
#define CC_PIN_GPIO_31	(31u)

/* Last GPIO Pad */
#define CC_PIN_GPIO_LAST CC_PIN_GPIO_31

/* GCI chipcontrol register indices */
#if defined(VLSI_CTRL_REGS) && defined(SKIP_LEGACY_CC_API)
#define CC_GCI_CHIPCTRL_00	SI_CTRLREGS_INVALID
#define CC_GCI_CHIPCTRL_01	SI_CTRLREGS_INVALID
#define CC_GCI_CHIPCTRL_02	SI_CTRLREGS_INVALID
#define CC_GCI_CHIPCTRL_03	SI_CTRLREGS_INVALID
#define CC_GCI_CHIPCTRL_04	SI_CTRLREGS_INVALID
#define CC_GCI_CHIPCTRL_05	SI_CTRLREGS_INVALID
#define CC_GCI_CHIPCTRL_06	SI_CTRLREGS_INVALID
#define CC_GCI_CHIPCTRL_07	SI_CTRLREGS_INVALID
#define CC_GCI_CHIPCTRL_08	SI_CTRLREGS_INVALID
#define CC_GCI_CHIPCTRL_09	SI_CTRLREGS_INVALID
#define CC_GCI_CHIPCTRL_10	SI_CTRLREGS_INVALID
#define CC_GCI_CHIPCTRL_10	SI_CTRLREGS_INVALID
#define CC_GCI_CHIPCTRL_11	SI_CTRLREGS_INVALID
#define CC_GCI_CHIPCTRL_12	SI_CTRLREGS_INVALID
#define CC_GCI_CHIPCTRL_13	SI_CTRLREGS_INVALID
#define CC_GCI_CHIPCTRL_14	SI_CTRLREGS_INVALID
#define CC_GCI_CHIPCTRL_15	SI_CTRLREGS_INVALID
#define CC_GCI_CHIPCTRL_16	SI_CTRLREGS_INVALID
#define CC_GCI_CHIPCTRL_17	SI_CTRLREGS_INVALID
#define CC_GCI_CHIPCTRL_18	SI_CTRLREGS_INVALID
#define CC_GCI_CHIPCTRL_19	SI_CTRLREGS_INVALID
#define CC_GCI_CHIPCTRL_20	SI_CTRLREGS_INVALID
#define CC_GCI_CHIPCTRL_21	SI_CTRLREGS_INVALID
#define CC_GCI_CHIPCTRL_22	SI_CTRLREGS_INVALID
#define CC_GCI_CHIPCTRL_23	SI_CTRLREGS_INVALID
#define CC_GCI_CHIPCTRL_24	SI_CTRLREGS_INVALID
#define CC_GCI_CHIPCTRL_25	SI_CTRLREGS_INVALID
#define CC_GCI_CHIPCTRL_26	SI_CTRLREGS_INVALID
#define CC_GCI_CHIPCTRL_27	SI_CTRLREGS_INVALID
#define CC_GCI_CHIPCTRL_28	SI_CTRLREGS_INVALID
#define CC_GCI_CHIPCTRL_29	SI_CTRLREGS_INVALID
#define CC_GCI_CHIPCTRL_30	SI_CTRLREGS_INVALID
#define CC_GCI_CHIPCTRL_31	SI_CTRLREGS_INVALID
#define CC_GCI_CHIPCTRL_32	SI_CTRLREGS_INVALID
#define CC_GCI_CHIPCTRL_33	SI_CTRLREGS_INVALID
#define CC_GCI_CHIPCTRL_34	SI_CTRLREGS_INVALID
#define CC_GCI_CHIPCTRL_35	SI_CTRLREGS_INVALID
#define CC_GCI_CHIPCTRL_36	SI_CTRLREGS_INVALID
#define CC_GCI_CHIPCTRL_37	SI_CTRLREGS_INVALID
#define CC_GCI_CHIPCTRL_38	SI_CTRLREGS_INVALID
#define CC_GCI_CHIPCTRL_39	SI_CTRLREGS_INVALID
#define CC_GCI_CHIPCTRL_40	SI_CTRLREGS_INVALID
#else /* !VLSI_CTRL_REGS || !SKIP_LEGACY_CC_API */
#define CC_GCI_CHIPCTRL_00	(0)
#define CC_GCI_CHIPCTRL_01	(1)
#define CC_GCI_CHIPCTRL_02	(2)
#define CC_GCI_CHIPCTRL_03	(3)
#define CC_GCI_CHIPCTRL_04	(4)
#define CC_GCI_CHIPCTRL_05	(5)
#define CC_GCI_CHIPCTRL_06	(6)
#define CC_GCI_CHIPCTRL_07	(7)
#define CC_GCI_CHIPCTRL_08	(8)
#define CC_GCI_CHIPCTRL_09	(9)
#define CC_GCI_CHIPCTRL_10	(10)
#define CC_GCI_CHIPCTRL_11	(11)
#define CC_GCI_CHIPCTRL_12	(12)
#define CC_GCI_CHIPCTRL_13	(13)
#define CC_GCI_CHIPCTRL_14	(14)
#define CC_GCI_CHIPCTRL_15	(15)
#define CC_GCI_CHIPCTRL_16	(16)
#define CC_GCI_CHIPCTRL_17	(17)
#define CC_GCI_CHIPCTRL_18	(18)
#define CC_GCI_CHIPCTRL_19	(19)
#define CC_GCI_CHIPCTRL_20	(20)
#define CC_GCI_CHIPCTRL_21	(21)
#define CC_GCI_CHIPCTRL_22	(22)
#define CC_GCI_CHIPCTRL_23	(23)
#define CC_GCI_CHIPCTRL_24	(24)
#define CC_GCI_CHIPCTRL_25	(25)
#define CC_GCI_CHIPCTRL_26	(26)
#define CC_GCI_CHIPCTRL_27	(27)
#define CC_GCI_CHIPCTRL_28	(28)
#define CC_GCI_CHIPCTRL_29	(29)
#define CC_GCI_CHIPCTRL_30	(30)
#define CC_GCI_CHIPCTRL_31	(31)
#define CC_GCI_CHIPCTRL_32	(32)
#define CC_GCI_CHIPCTRL_33	(33)
#define CC_GCI_CHIPCTRL_34	(34)
#define CC_GCI_CHIPCTRL_35	(35)
#define CC_GCI_CHIPCTRL_36	(36)
#define CC_GCI_CHIPCTRL_37	(37)
#define CC_GCI_CHIPCTRL_38	(38)
#define CC_GCI_CHIPCTRL_39	(39)
#define CC_GCI_CHIPCTRL_40	(40)
#define CC_GCI_CHIPCTRL_41	(41)
#endif /* !VLSI_CTRL_REGS || !SKIP_LEGACY_CC_API */

/* GCI chip ctrl SDTC Soft reset */
#define GCI_CHIP_CTRL_SDTC_SOFT_RESET       (1 << 31)

#define CC_GCI_XTAL_BUFSTRG_NFC (0xff << 12)

#define CC_GCI_04_SDIO_DRVSTR_SHIFT	15
#define CC_GCI_04_SDIO_DRVSTR_MASK	(0x0f << CC_GCI_04_SDIO_DRVSTR_SHIFT)	/* 0x00078000 */
#define CC_GCI_04_SDIO_DRVSTR_OVERRIDE_BIT	(1 << 18)
#define CC_GCI_04_SDIO_DRVSTR_DEFAULT_MA	14
#define CC_GCI_04_SDIO_DRVSTR_MIN_MA	2
#define CC_GCI_04_SDIO_DRVSTR_MAX_MA	16

#define CC_GCI_04_4387C0_XTAL_PM_CLK	(1u << 20u)

#define CC_GCI_05_4387C0_AFE_RET_ENB_MASK	(1u << 7u)

#define CC_GCI_CHIPCTRL_07_BTDEFLO_ANT0_NBIT	2u
#define CC_GCI_CHIPCTRL_07_BTDEFLO_ANT0_MASK	0xFu
#define CC_GCI_CHIPCTRL_07_BTDEFHI_ANT0_NBIT	11u
#define CC_GCI_CHIPCTRL_07_BTDEFHI_ANT0_MASK	1u

#define CC_GCI_CHIPCTRL_18_BTDEF_ANT0_NBIT		10u
#define CC_GCI_CHIPCTRL_18_BTDEF_ANT0_MASK		0x1Fu
#define CC_GCI_CHIPCTRL_18_BTDEFLO_ANT1_NBIT	15u
#define CC_GCI_CHIPCTRL_18_BTDEFLO_ANT1_MASK	1u
#define CC_GCI_CHIPCTRL_18_BTDEFHI_ANT1_NBIT	26u
#define CC_GCI_CHIPCTRL_18_BTDEFHI_ANT1_MASK	0x3Fu

#define CC_GCI_CHIPCTRL_19_BTDEF_ANT1_NBIT		10u
#define CC_GCI_CHIPCTRL_19_BTDEF_ANT1_MASK		0x7u

#ifdef VLSI_CTRL_REGS

#define CLB_PRISEL_WLSC_RFEM_DISABLE_MASK	0x100u

#define RF_SW_CTRL_ELNABYP_ANT_MASK	0xC0C00C0C00ull

/* These are the outputs to the rfem which go out via the CLB */
#define RF_SW_CTRL_ELNABYP_2G0_MASK     0x0000000400ull
#define RF_SW_CTRL_ELNABYP_5G0_MASK     0x0000000800ull
#define RF_SW_CTRL_ELNABYP_2G1_MASK     0x0040000000ull
#define RF_SW_CTRL_ELNABYP_5G1_MASK     0x0080000000ull

/* Feedback values go into the phy from CLB output
 * The polarity of the feedback is opposite to the elnabyp signal going out to the rfem
 */
#define RF_SW_CTRL_ELNABYP_2G0_MASK_FB  0x0000040000ull
#define RF_SW_CTRL_ELNABYP_5G0_MASK_FB  0x0000080000ull
#define RF_SW_CTRL_ELNABYP_2G1_MASK_FB  0x4000000000ull
#define RF_SW_CTRL_ELNABYP_5G1_MASK_FB  0x8000000000ull

#endif /* VLSI_CTRL_REGS */

#define CC_GCI_CHIPCTRL_21_WLSC_ANT0_2G_RFEM_DISABLE_NBIT	24u
#define CC_GCI_CHIPCTRL_21_WLSC_ANT0_2G_RFEM_DISABLE_MASK (1u <<\
				CC_GCI_CHIPCTRL_21_WLSC_ANT0_2G_RFEM_DISABLE_NBIT)

#define CC_GCI_CHIPCTRL_22_WLSC_ANT1_2G_RFEM_DISABLE_NBIT	24u
#define CC_GCI_CHIPCTRL_22_WLSC_ANT1_2G_RFEM_DISABLE_MASK (1u <<\
				CC_GCI_CHIPCTRL_22_WLSC_ANT1_2G_RFEM_DISABLE_NBIT)

#define CC_GCI_CHIPCTRL_28_WLSC_ANT0_5G_RFEM_DISABLE_NBIT	8u
#define CC_GCI_CHIPCTRL_28_WLSC_ANT0_5G_RFEM_DISABLE_MASK (1u <<\
				CC_GCI_CHIPCTRL_28_WLSC_ANT0_5G_RFEM_DISABLE_NBIT)

#define CC_GCI_CHIPCTRL_29_WLSC_ANT1_5G_RFEM_DISABLE_NBIT	8u
#define CC_GCI_CHIPCTRL_29_WLSC_ANT1_5G_RFEM_DISABLE_MASK (1u <<\
				CC_GCI_CHIPCTRL_29_WLSC_ANT1_5G_RFEM_DISABLE_NBIT)

#define CC_GCI_CHIPCTRL_23_MAIN_WLSC_PRISEL_FORCE_NBIT		16u
#define CC_GCI_CHIPCTRL_23_MAIN_WLSC_PRISEL_VAL_NBIT		17u
#define CC_GCI_CHIPCTRL_23_AUX_WLSC_PRISEL_FORCE_NBIT		18u
#define CC_GCI_CHIPCTRL_23_AUX_WLSC_PRISEL_VAL_NBIT		19u
#define CC_GCI_CHIPCTRL_23_WLSC_BTSC_PRISEL_FORCE_NBIT		20u
#define CC_GCI_CHIPCTRL_23_WLSC_BTSC_PRISEL_VAL_NBIT		21u
#define CC_GCI_CHIPCTRL_23_WLSC_BTMAIN_PRISEL_FORCE_NBIT	22u
#define CC_GCI_CHIPCTRL_23_WLSC_BTMAIN_PRISEL_VAL_NBIT		23u
#define CC_GCI_CHIPCTRL_23_BTMAIN_BTSC_PRISEL_FORCE_NBIT	24u
#define CC_GCI_CHIPCTRL_23_BTMAIN_BTSC_PRISEL_VAL_NBIT		25u
#define CC_GCI_CHIPCTRL_23_LVM_MODE_DISABLE_NBIT		26u

#define CC_GCI_CHIPCTRL_23_MAIN_WLSC_PRISEL_FORCE_MASK	(1u <<\
				CC_GCI_CHIPCTRL_23_MAIN_WLSC_PRISEL_FORCE_NBIT)
#define CC_GCI_CHIPCTRL_23_MAIN_WLSC_PRISEL_VAL_MASK	(1u <<\
				CC_GCI_CHIPCTRL_23_MAIN_WLSC_PRISEL_VAL_NBIT)
#define CC_GCI_CHIPCTRL_23_AUX_WLSC_PRISEL_FORCE_MASK	(1u <<\
				CC_GCI_CHIPCTRL_23_AUX_WLSC_PRISEL_FORCE_NBIT)
#define CC_GCI_CHIPCTRL_23_AUX_WLSC_PRISEL_VAL_MASK	(1u <<\
				CC_GCI_CHIPCTRL_23_AUX_WLSC_PRISEL_VAL_NBIT)
#define CC_GCI_CHIPCTRL_23_WLSC_BTSC_PRISEL_FORCE_MASK	(1u <<\
				CC_GCI_CHIPCTRL_23_WLSC_BTSC_PRISEL_FORCE_NBIT)
#define CC_GCI_CHIPCTRL_23_WLSC_BTSC_PRISEL_VAL_MASK	(1u <<\
				CC_GCI_CHIPCTRL_23_WLSC_BTSC_PRISEL_VAL_NBIT)
#define CC_GCI_CHIPCTRL_23_WLSC_BTMAIN_PRISEL_FORCE_MASK	(1u <<\
				CC_GCI_CHIPCTRL_23_WLSC_BTMAIN_PRISEL_FORCE_NBIT)
#define CC_GCI_CHIPCTRL_23_WLSC_BTMAIN_PRISEL_VAL_MASK	(1u <<\
				CC_GCI_CHIPCTRL_23_WLSC_BTMAIN_PRISEL_VAL_NBIT)
#define CC_GCI_CHIPCTRL_23_BTMAIN_BTSC_PRISEL_FORCE_MASK	(1u <<\
				CC_GCI_CHIPCTRL_23_BTMAIN_BTSC_PRISEL_FORCE_NBIT)
#define CC_GCI_CHIPCTRL_23_BTMAIN_BTSC_PRISEL_VAL_MASK	(1u <<\
				CC_GCI_CHIPCTRL_23_BTMAIN_BTSC_PRISEL_VAL_NBIT)
#define CC_GCI_CHIPCTRL_23_LVM_MODE_DISABLE_MASK	(1u <<\
				CC_GCI_CHIPCTRL_23_LVM_MODE_DISABLE_NBIT)

#define CC_GCI_CHIPCTRL_30_BTMN_5GSC_PRISEL_FORCE_NBIT		23u
#define CC_GCI_CHIPCTRL_30_BTMN_5GSC_PRISEL_VAL_NBIT		24u
#define CC_GCI_CHIPCTRL_30_BTMN_5GSC_PRISEL_FORCE_MASK	(1u <<\
				CC_GCI_CHIPCTRL_30_BTMN_5GSC_PRISEL_FORCE_NBIT)
#define CC_GCI_CHIPCTRL_30_BTMN_5GSC_PRISEL_VAL_MASK	(1u <<\
				CC_GCI_CHIPCTRL_30_BTMN_5GSC_PRISEL_VAL_NBIT)

#define CLB_LOWER_10BITS_MASK                  0x3FFu
#define CLB_UPPER_10BITS_MASK                  0xFFC00u
#define CLB_10BITS_MASK                        0x3FFu
#define CLB_LOWER_10BITS_WIDTH                 10u
#define CLB_SWCTRL_SMASK_CORESEL_ANT0_EN_SHIFT 29u
#define CLB_SWCTRL_SMASK_CORESEL_ANT1_EN_SHIFT 30u
#define CLB_SWCTRL_DMASK_BT_ANT0_L_SHIFT       17u
#define CLB_SWCTRL_DMASK_BT_ANT1_L_SHIFT       10u
#define CLB_SWCTRL_DMASK_BT_ANT0_H_SHIFT       20u
#define CLB_SWCTRL_DMASK_BT_ANT1_H_SHIFT       0u
#define CLB_SWCTRL_BTAOA_OVR_SHIFT             18u
#define CLB_SWCTRL_SMASK_WLAN_ANT0_L_SHIFT     20u
#define CLB_SWCTRL_SMASK_WLAN_ANT1_L_SHIFT     0u
#define CLB_SWCTRL_SMASK_WLAN_ANT0_H_SHIFT     10u
#define CLB_SWCTRL_SMASK_WLAN_ANT1_H_SHIFT     20u
#define CLB_SWCTRL_BTC_PRISEL_MASK_SHIFT       27u
#define CLB_SWCTRL_BTC_PRISEL_MASK             0x3u
#define CLB_SWCTRL_BTC_PRISEL_ANT_MASK_SHIFT   26u
#define CLB_SWCTRL_BT_DEFAULT_VAL_10_SHIFT     15u
#define CLB_SWCTRL_BT_DEFAULT_VAL_10_MASK      0x1u
#define CLB_SWCTRL_BT_DEFAULT_VAL_10_LSB       10u
#define CLB_SWCTRL_BT_DEFAULT_VAL_16TO11_SHIFT 26u
#define CLB_SWCTRL_BT_DEFAULT_VAL_16TO11_MASK  0x3Fu
#define CLB_SWCTRL_BT_DEFAULT_VAL_16TO11_LSB   11u
#define CLB_SWCTRL_BT_DEFAULT_VAL_19TO17_SHIFT 10u
#define CLB_SWCTRL_BT_DEFAULT_VAL_19TO17_MASK  0x7u
#define CLB_SWCTRL_BT_DEFAULT_VAL_19TO17_LSB   17u
#define CLB_SWCTRL_BT_DEFAULT_VAL_22TO20_SHIFT 13u
#define CLB_SWCTRL_BT_DEFAULT_VAL_22TO20_MASK  0x7u
#define CLB_SWCTRL_BT_DEFAULT_VAL_22TO20_LSB   0u
#define CLB_SWCTRL_BT_DEFAULT_VAL_24TO23_SHIFT 26u
#define CLB_SWCTRL_BT_DEFAULT_VAL_24TO23_MASK  0x3u
#define CLB_SWCTRL_BT_DEFAULT_VAL_24TO23_LSB   3u
#define CLB_SWCTRL_BT_DEFAULT_VAL_39TO25_MASK  0x7FFFu
#define CLB_SWCTRL_BT_DEFAULT_VAL_39TO25_LSB   5u
#define CLB_RFEM_VIO_CTRL_MUX_MASK             0xFu
#define CLB_RFEM_VIO_CTRL_MUX_CFEM_VAL         0u
#define CLB_SWCTRL_SMASK_SCAN_CORE0SEL_H_SHIFT 20u
#define CLB_SWCTRL_SMASK_BTSC_SHIFT            10u
#define CLB_SWCTRL_SMASK_FPRIME0_H_SHIFT       10u
#define CLB_SWCTRL_SMASK_FPRIME1_H_SHIFT       20u
#define CLB_SWCTRL_SMASK_FSCAN1_H_SHIFT        10u
#define CLB_SWCTRL_BTCX_5G_PRISEL_SHIFT        14u
#define CLB_SWCTRL_DMASK_BT5G_ANT0_L_SHIFT     16u
#define CLB_SWCTRL_DMASK_BT5G_ANT1_L_SHIFT     0u
#define CLB_SWCTRL_DMASK_BT5G_ANT0_H_SHIFT     0u
#define CLB_SWCTRL_DMASK_BT5G_ANT1_H_SHIFT     10u
#define CLB_SCAN5G_HW_PRISEL_EVAL_LUT_MASK     0xFFFFFFFFu
#define CLB_SCAN5G_HW_PRISEL_EVAL_LUT_VAL      0xCDCDCCCCu

/*	2G core0/core1 Pulse width register (offset : 0x47C)
*	wl_rx_long_pulse_width_2g_core0 [4:0];
*	wl_rx_short_pulse_width_2g_core0 [9:5];
*	wl_rx_long_pulse_width_2g_core1  [20:16];
*	wl_rx_short_pulse_width_2g_core1 [25:21];
*/
#define CC_GCI_CNCB_RESET_PULSE_WIDTH_2G_CORE1_NBIT	(16u)
#define CC_GCI_CNCB_RESET_PULSE_WIDTH_2G_CORE0_MASK	(0x1Fu)
#define CC_GCI_CNCB_RESET_PULSE_WIDTH_2G_CORE1_MASK	(0x1Fu <<\
				CC_GCI_CNCB_RESET_PULSE_WIDTH_2G_CORE1_NBIT)

#if !defined(BCMDONGLEHOST)
#if BCMGCIREV >= 24
/*	2G core0/core1 Pulse width register (offset : 0x47C)
*	wl_rx_long_pulse_width_2g_core0 [7:0];
*	wl_rx_short_pulse_width_2g_core0 [15:8];
*/
#define CC_GCI_CNCB_SHORT_RESET_PULSE_WIDTH_2G_CORE0_NBIT   8u
/*    wl_rx_long_pulse_width_2g_core1  [23:16]; */
#define CC_GCI_CNCB_LONG_RESET_PULSE_WIDTH_2G_CORE1_NBIT    16u
/*    wl_rx_short_pulse_width_2g_core1 [31:24]; */
#define CC_GCI_CNCB_SHORT_RESET_PULSE_WIDTH_2G_CORE1_NBIT   24u

/*    5G core0/Core1 (offset : 0x480)
*    wl_rx_long_pulse_width_5g[7:0];
*    wl_rx_short_pulse_width_5g[15:8]
*/
#define CC_GCI_CNCB_SHORT_RESET_PULSE_WIDTH_5G_NBIT         8u

#define CC_GCI_CNCB_LONG_RESET_PULSE_WIDTH_MASK             0xFFu
#define CC_GCI_CNCB_SHORT_RESET_PULSE_WIDTH_MASK            0xFFu

/*    Setting the wl_rx_long_pulse_width generated
 *    by CNCB used when more than one slice is up
 *    to a duration of 120 cycles which is 2us
 *    wl_rx_short_pulse_width is set to 118 cycles.
 *    A delta of ~ 20 ns or more
 *    between the long_pulse and short pulse is
 *    required as per the design spec
 *    1 cycle == 16.676 ns
 */
#define CC_GCI_CNCB_LONG_RESET_PULSE_VAL                    120u
#define CC_GCI_CNCB_SHORT_RESET_PULSE_VAL                   118u
#else
/*    2G core0/core1 Pulse width register (offset : 0x47C)
*    wl_rx_long_pulse_width_2g_core0 [4:0];
*    wl_rx_short_pulse_width_2g_core0 [9:5];
*/
#define CC_GCI_CNCB_SHORT_RESET_PULSE_WIDTH_2G_CORE0_NBIT   5u
/*    wl_rx_long_pulse_width_2g_core1  [20:16]; */
#define CC_GCI_CNCB_LONG_RESET_PULSE_WIDTH_2G_CORE1_NBIT    16u
/*    wl_rx_short_pulse_width_2g_core1 [25:21]; */
#define CC_GCI_CNCB_SHORT_RESET_PULSE_WIDTH_2G_CORE1_NBIT   21u

/*    5G core0/Core1 (offset : 0x480)
*    wl_rx_long_pulse_width_5g[4:0];
*    wl_rx_short_pulse_width_5g[9:5]
*/
#define CC_GCI_CNCB_SHORT_RESET_PULSE_WIDTH_5G_NBIT         5u

#define CC_GCI_CNCB_LONG_RESET_PULSE_WIDTH_MASK             0x1Fu
#define CC_GCI_CNCB_SHORT_RESET_PULSE_WIDTH_MASK            0x1Fu

/*    Setting the wl_rx_long_pulse_width generated
 *    by CNCB used when more than one slice is up
 *    to max duration of 31 cycles
 *    wl_rx_short_pulse_width is set to 29 cycles.
 *    A delta of ~ 20 ns or more
 *    between the long_pulse and short pulse is
 *    required as per the design spec
 *    1 cycle == 16.676 ns
 */
#define CC_GCI_CNCB_LONG_RESET_PULSE_VAL                    0x1Fu
#define CC_GCI_CNCB_SHORT_RESET_PULSE_VAL                   0x1Du
#endif /* BCMGCIREV */
#endif /* !defined(BCMDONGLEHOST) */

#define CC_GCI_CNCB_LONG_RESET_PULSE_WIDTH_2G_CORE0_MASK    CC_GCI_CNCB_LONG_RESET_PULSE_WIDTH_MASK
#define CC_GCI_CNCB_LONG_RESET_PULSE_WIDTH_2G_CORE1_MASK    (\
			CC_GCI_CNCB_LONG_RESET_PULSE_WIDTH_MASK <<\
			CC_GCI_CNCB_LONG_RESET_PULSE_WIDTH_2G_CORE1_NBIT)
#define CC_GCI_CNCB_SHORT_RESET_PULSE_WIDTH_2G_CORE0_MASK   (\
			CC_GCI_CNCB_SHORT_RESET_PULSE_WIDTH_MASK <<\
			CC_GCI_CNCB_SHORT_RESET_PULSE_WIDTH_2G_CORE0_NBIT)
#define CC_GCI_CNCB_SHORT_RESET_PULSE_WIDTH_2G_CORE1_MASK   (\
			CC_GCI_CNCB_SHORT_RESET_PULSE_WIDTH_MASK <<\
			CC_GCI_CNCB_SHORT_RESET_PULSE_WIDTH_2G_CORE1_NBIT)

/*	5G core0/Core1 (offset : 0x480)
*	wl_rx_long_pulse_width_5g[4:0];
*	wl_rx_short_pulse_width_5g[9:5]
*/

#define CC_GCI_CNCB_LONG_RESET_PULSE_WIDTH_5G_MASK          CC_GCI_CNCB_LONG_RESET_PULSE_WIDTH_MASK
#define CC_GCI_CNCB_SHORT_RESET_PULSE_WIDTH_5G_MASK         (\
			CC_GCI_CNCB_SHORT_RESET_PULSE_WIDTH_MASK <<\
			CC_GCI_CNCB_SHORT_RESET_PULSE_WIDTH_5G_NBIT)

#define CC_GCI_CNCB_GLITCH_FILTER_WIDTH_MASK	(0xFFu)

#define CC_GCI_RESET_OVERRIDE_NBIT	0x1u
#define CC_GCI_RESET_OVERRIDE_MASK	(0x1u << \
				CC_GCI_RESET_OVERRIDE_NBIT)

/*	2G core0/core1  register (offset : 0x81C) */
/*
cncb2rdig_scan2G_dedicated_path_en_core0_ovr	10
cncb2rdig_scan2G_dedicated_path_en_core0_ovr_en	11
cncb2rdig_scan2G_dedicated_path_en_core1_ovr	12
cncb2rdig_scan2G_dedicated_path_en_core1_ovr_en	13
*/

#define CC_GCI_dedicated_path_en_core0_ovr_NBIT (10u)
#define CC_GCI_dedicated_path_en_core0_ovr_MASK (1u <<\
		CC_GCI_dedicated_path_en_core0_ovr_NBIT)
#define CC_GCI_dedicated_path_en_core0_ovr_en_NBIT (11u)
#define CC_GCI_dedicated_path_en_core0_ovr_en_MASK (1u <<\
		CC_GCI_dedicated_path_en_core0_ovr_en_NBIT)
#define CC_GCI_dedicated_path_en_core1_ovr_NBIT (12u)
#define CC_GCI_dedicated_path_en_core1_ovr_MASK (1u <<\
		CC_GCI_dedicated_path_en_core1_ovr_NBIT)
#define CC_GCI_dedicated_path_en_core1_ovr_en_NBIT (13u)
#define CC_GCI_dedicated_path_en_core1_ovr_en_MASK (1u <<\
		CC_GCI_dedicated_path_en_core1_ovr_en_NBIT)

#define CC_GCI_06_JTAG_SEL_SHIFT	4u
#define CC_GCI_06_JTAG_SEL_MASK		(1u << 4u)

#define CC_GCI_NUMCHIPCTRLREGS(cap1)	((cap1 & 0xF00u) >> 8u)

#define CC_GCI_03_LPFLAGS_SFLASH_MASK		(0xFFFFFFu << 8u)
#define CC_GCI_03_LPFLAGS_SFLASH_VAL		(0xCCCCCCu << 8u)

#define CC_GCI_13_INSUFF_TREFUP_FIX_SHIFT	31u
/* Note: For 4368 B0 onwards, the shift offset remains the same,
* but the Chip Common Ctrl GCI register is 16
*/
#define CC_GCI_16_INSUFF_TREFUP_FIX_SHIFT	31u

#define GPIO_CTRL_REG_DISABLE_INTERRUPT		(3u << 9u)
#define GPIO_CTRL_REG_COUNT			40

#define XTAL_HQ_SETTING_4387	(0xFFF94D30u)
#define XTAL_LQ_SETTING_4387	(0xFFF94380u)

#define CC_GCI_16_BBPLL_CH_CTRL_GRP_PD_TRIG_1_MASK		(0x00000200u)
#define CC_GCI_16_BBPLL_CH_CTRL_GRP_PD_TRIG_1_SHIFT		(9u)
#define CC_GCI_16_BBPLL_CH_CTRL_GRP_PD_TRIG_24_3_MASK		(0xFFFFFC00u)
#define CC_GCI_16_BBPLL_CH_CTRL_GRP_PD_TRIG_24_3_SHIFT		(10u)

#define CC_GCI_17_BBPLL_CH_CTRL_GRP_PD_TRIG_30_25_MASK		(0x0000FC00u)
#define CC_GCI_17_BBPLL_CH_CTRL_GRP_PD_TRIG_30_25_SHIFT		(10u)
#define CC_GCI_17_BBPLL_CH_CTRL_EN_MASK				(0x04000000u)

#define CC_GCI_20_BBPLL_CH_CTRL_GRP_MASK			(0xFC000000u)
#define CC_GCI_20_BBPLL_CH_CTRL_GRP_SHIFT			(26u)

/* GCI Chip Ctrl Regs */
#define GCI_CC28_IHRP_SEL_MASK			(7 << 24)
#define GCI_CC28_IHRP_SEL_SHIFT			(24u)

#define GCI_CC27_BAND_MODE_SEL_SHIFT            (19u)
#define GCI_CC27_BAND_MODE_SEL_MASK             (1u << GCI_CC27_BAND_MODE_SEL_SHIFT)

#define GCI_CC27_IHRP_ACCESS_SEL_SHIFT          (23u)
#define GCI_CC27_IHRP_ACCESS_SEL_MASK           (0x1FFu << GCI_CC27_IHRP_ACCESS_SEL_SHIFT)

/* xtal_rst_delay_hw MASK and SHIFT */
#define GCI_CC27_XTAL_RST_DLY_HQ_SHFT	(26u)
#define GCI_CC27_XTAL_RST_DLY_HQ_MASK	(0x3u << GCI_CC27_XTAL_RST_DLY_HQ_SHFT)

/* xtal_vbuck_ctrl_Rladder_startup MASK and SHIFT */
#define GCI_CC27_XTAL_VBUCK_RLAD_STUP_SHFT	(28u)
#define GCI_CC27_XTAL_VBUCK_RLAD_STUP_MASK	(0xF << GCI_CC27_XTAL_VBUCK_RLAD_STUP_SHFT)

/* xtal_vbuck_ctrl_Rladder_normal_LQ MASK and SHIFT */
#define GCI_CC27_VBUCK_RLADDER_LQ_SEL_SHIFT	(20u)
#define GCI_CC27_VBUCK_RLADDER_LQ_SEL_MASK	(0xF << GCI_CC27_VBUCK_RLADDER_LQ_SEL_SHIFT)

/* xtal_vbuck_ctrl_Rladder_normal_HQ MASK and SHIFT */
#define GCI_CC28_VBUCK_RLADDER_HQ_SEL_SHIFT	(3u)
#define GCI_CC28_VBUCK_RLADDER_HQ_SEL_MASK	(0xF << GCI_CC28_VBUCK_RLADDER_HQ_SEL_SHIFT)

/* 30=MACPHY_CLK_MAIN, 29=MACPHY_CLK_AUX, 23=RADIO_PU_MAIN, 22=CORE_RDY_MAIN
 * 20=RADIO_PU_AUX, 18=CORE_RDY_AUX, 14=PWRSW_MAIN, 11=PWRSW_AUX
 */
#define GRP_PD_TRIGGER_MASK_4387	(0x60d44800u)

/* power down ch0=MAIN/AUX PHY_clk, ch2=MAIN/AUX MAC_clk, ch5=RFFE_clk */
#define GRP_PD_MASK_4387		(0x25u)

#define CC_GCI_CHIPCTRL_11_2x2_ANT_MASK		0x03
#define CC_GCI_CHIPCTRL_11_SHIFT_ANT_MASK	26

/* primary slice<>scan slice antmask configuration */
#define CC_GCI_CC41_SC_ANTMASK_NBIT		0x7u
#define CC_GCI_CC41_MAIN_SC_VAL_ANTMASK_MASK	0x3u
#define CC_GCI_CC41_AUX_SC_VAL_ANTMASK_MASK	0xCu
#define CC_GCI_CC41_BT2G_SC_VAL_ANTMASK_MASK	0x30u
#define CC_GCI_CC41_BT5G_SC_VAL_ANTMASK_MASK	0xC0u

/* GCI chipstatus register indices */
#if defined(VLSI_CTRL_REGS) && defined(SKIP_LEGACY_CC_API)
#define GCI_CHIPSTATUS_00	SI_CTRLREGS_INVALID
#define GCI_CHIPSTATUS_01	SI_CTRLREGS_INVALID
#define GCI_CHIPSTATUS_02	SI_CTRLREGS_INVALID
#define GCI_CHIPSTATUS_03	SI_CTRLREGS_INVALID
#define GCI_CHIPSTATUS_04	SI_CTRLREGS_INVALID
#define GCI_CHIPSTATUS_05	SI_CTRLREGS_INVALID
#define GCI_CHIPSTATUS_06	SI_CTRLREGS_INVALID
#define GCI_CHIPSTATUS_07	SI_CTRLREGS_INVALID
#define GCI_CHIPSTATUS_08	SI_CTRLREGS_INVALID
#define GCI_CHIPSTATUS_09	SI_CTRLREGS_INVALID
#define GCI_CHIPSTATUS_10	SI_CTRLREGS_INVALID
#define GCI_CHIPSTATUS_11	SI_CTRLREGS_INVALID
#define GCI_CHIPSTATUS_12	SI_CTRLREGS_INVALID
#define GCI_CHIPSTATUS_13	SI_CTRLREGS_INVALID
#define GCI_CHIPSTATUS_14	SI_CTRLREGS_INVALID
#define GCI_CHIPSTATUS_15	SI_CTRLREGS_INVALID
#else /* !VLSI_CTRL_REGS || !SKIP_LEGACY_CC_API */
#define GCI_CHIPSTATUS_00	(0u)
#define GCI_CHIPSTATUS_01	(1u)
#define GCI_CHIPSTATUS_02	(2u)
#define GCI_CHIPSTATUS_03	(3u)
#define GCI_CHIPSTATUS_04	(4u)
#define GCI_CHIPSTATUS_05	(5u)
#define GCI_CHIPSTATUS_06	(6u)
#define GCI_CHIPSTATUS_07	(7u)
#define GCI_CHIPSTATUS_08	(8u)
#define GCI_CHIPSTATUS_09	(9u)
#define GCI_CHIPSTATUS_10	(10u)
#define GCI_CHIPSTATUS_11	(11u)
#define GCI_CHIPSTATUS_12	(12u)
#define GCI_CHIPSTATUS_13	(13u)
#define GCI_CHIPSTATUS_14	(14u)
#define GCI_CHIPSTATUS_15	(15u)
#endif /* !VLSI_CTRL_REGS || !SKIP_LEGACY_CC_API */
#define GCI_CHIPSTATUS_16	(16)
#define GCI_CHIPSTATUS_17	(17)

/* GCI Core Control Reg */
#define	GCI_CORECTRL_SR_MASK			(1 << 0)	/**< SECI block Reset */
#define	GCI_CORECTRL_RSL_MASK			(1 << 1)	/**< ResetSECILogic */
#define	GCI_CORECTRL_ES_MASK			(1 << 2)	/**< EnableSECI */
#define	GCI_CORECTRL_FSL_MASK			(1 << 3)	/**< Force SECI Out Low */
#define	GCI_CORECTRL_SOM_MASK			(7 << 4)	/**< SECI Op Mode */
#define	GCI_CORECTRL_US_MASK			(1 << 7)	/**< Update SECI */
#define	GCI_CORECTRL_BOS_MASK			(1 << 8)	/**< Break On Sleep */
#define GCI_CORECTRL_SPMI_UART_MASK		(1 << 13)	/* 1- spmi 0- uart mode */
/* LTE GCI Bus Mapping: 1-Both SPMI and Legacy 0-Legacy LTE Coex Only */
#define GCI_CORECTRL_SPMI_UART_MODE_SEL2	(1 << 14)
#define	GCI_CORECTRL_FORCEREGCLK_MASK		(1 << 18)	/* ForceRegClk */

/* 4387 GCI AVS function */
#define GCI6_AVS_ENAB			1u
#define GCI6_AVS_ENAB_SHIFT		31u
#define GCI6_AVS_ENAB_MASK		(1u << GCI6_AVS_ENAB_SHIFT)
#define GCI6_AVS_CBUCK_VOLT_SHIFT	25u
#define GCI6_AVS_CBUCK_VOLT_MASK	(0x1Fu << GCI6_AVS_CBUCK_VOLT_SHIFT)

/* GCI GPIO for function sel GCI-0/GCI-1 */
#define CC_GCI_GPIO_0			(0)
#define CC_GCI_GPIO_1			(1)
#define CC_GCI_GPIO_2			(2)
#define CC_GCI_GPIO_3			(3)
#define CC_GCI_GPIO_4			(4)
#define CC_GCI_GPIO_5			(5)
#define CC_GCI_GPIO_6			(6)
#define CC_GCI_GPIO_7			(7)
#define CC_GCI_GPIO_8			(8)
#define CC_GCI_GPIO_9			(9)
#define CC_GCI_GPIO_10			(10)
#define CC_GCI_GPIO_11			(11)
#define CC_GCI_GPIO_12			(12)
#define CC_GCI_GPIO_13			(13)
#define CC_GCI_GPIO_14			(14)
#define CC_GCI_GPIO_15			(15)

/* indicates Invalid GPIO, e.g. when PAD GPIO doesn't map to GCI GPIO */
#define CC_GCI_GPIO_INVALID		0xFF

/* Plese do not these following defines */
/* find the 4 bit mask given the bit position */
#define GCIMASK(pos)  (((uint32)0xF) << pos)
/* get the value which can be used to directly OR with chipcontrol reg */
#define GCIPOSVAL(val, pos)  ((((uint32)val) << pos) & GCIMASK(pos))
/* Extract nibble from a given position */
#define GCIGETNBL(val, pos)	((val >> pos) & 0xF)

/* find the 8 bit mask given the bit position */
#define GCIMASK_8B(pos)  (((uint32)0xFF) << pos)
/* get the value which can be used to directly OR with chipcontrol reg */
#define GCIPOSVAL_8B(val, pos)  ((((uint32)val) << pos) & GCIMASK_8B(pos))
/* Extract nibble from a given position */
#define GCIGETNBL_8B(val, pos)	((val >> pos) & 0xFF)

/* find the 4 bit mask given the bit position */
#define GCIMASK_4B(pos)  (((uint32)0xF) << pos)
/* get the value which can be used to directly OR with chipcontrol reg */
#define GCIPOSVAL_4B(val, pos)  ((((uint32)val) << pos) & GCIMASK_4B(pos))
/* Extract nibble from a given position */
#define GCIGETNBL_4B(val, pos)	((val >> pos) & 0xF)

/* GCI Intstatus(Mask)/WakeMask Register bits. */
#define GCI_INTSTATUS_RBI	(1 << 0)	/**< Rx Break Interrupt */
#define GCI_INTSTATUS_UB	(1 << 1)	/**< UART Break Interrupt */
#define GCI_INTSTATUS_SPE	(1 << 2)	/**< SECI Parity Error Interrupt */
#define GCI_INTSTATUS_SFE	(1 << 3)	/**< SECI Framing Error Interrupt */
#define GCI_INTSTATUS_SRITI	(1 << 9)	/**< SECI Rx Idle Timer Interrupt */
#define GCI_INTSTATUS_STFF	(1 << 10)	/**< SECI Tx FIFO Full Interrupt */
#define GCI_INTSTATUS_STFAE	(1 << 11)	/**< SECI Tx FIFO Almost Empty Intr */
#define GCI_INTSTATUS_SRFAF	(1 << 12)	/**< SECI Rx FIFO Almost Full */
#define GCI_INTSTATUS_SRFNE	(1 << 14)	/**< SECI Rx FIFO Not Empty */
#define GCI_INTSTATUS_SRFOF	(1 << 15)	/**< SECI Rx FIFO Not Empty Timeout */
#define GCI_INTSTATUS_SRXFIFO	(1 << 16)	/**< SPMI coex rx fifo interrupt */
#define GCI_INTSTATUS_EVENT	(1 << 21)	/* GCI Event Interrupt */
#define GCI_INTSTATUS_LEVELWAKE (1 << 22)	/* GCI Wake Level Interrupt */
#define GCI_INTSTATUS_EVENTWAKE (1 << 23)	/* GCI Wake Event Interrupt */
#define GCI_INTSTATUS_SEMAPHORE (1 << 24)	/* HW Semaphore Interrupt */
#define GCI_INTSTATUS_GPIOINT	(1 << 25)	/**< GCIGpioInt */
#define GCI_INTSTATUS_GPIOWAKE	(1 << 26)	/**< GCIGpioWake */
#define GCI_INTSTATUS_LHLWLWAKE	(1 << 30)	/* LHL WL wake */

/* GCI IntMask Register bits. */
#define GCI_INTMASK_RBI		(1 << 0)	/**< Rx Break Interrupt */
#define GCI_INTMASK_UB		(1 << 1)	/**< UART Break Interrupt */
#define GCI_INTMASK_SPE		(1 << 2)	/**< SECI Parity Error Interrupt */
#define GCI_INTMASK_SFE		(1 << 3)	/**< SECI Framing Error Interrupt */
#define GCI_INTMASK_SRITI	(1 << 9)	/**< SECI Rx Idle Timer Interrupt */
#define GCI_INTMASK_STFF	(1 << 10)	/**< SECI Tx FIFO Full Interrupt */
#define GCI_INTMASK_STFAE	(1 << 11)	/**< SECI Tx FIFO Almost Empty Intr */
#define GCI_INTMASK_SRFAF	(1 << 12)	/**< SECI Rx FIFO Almost Full */
#define GCI_INTMASK_SRFNE	(1 << 14)	/**< SECI Rx FIFO Not Empty */
#define GCI_INTMASK_SRFOF	(1 << 15)	/**< SECI Rx FIFO Not Empty Timeout */
#define GCI_INTMASK_SRXFIFO	(1 << 16)	/**< SPMI coex rx fifo interrupt */
#define GCI_INTMASK_EVENT	(1 << 21)	/* GCI Event Interrupt */
#define GCI_INTMASK_LEVELWAKE   (1 << 22)	/* GCI Wake Level Interrupt */
#define GCI_INTMASK_EVENTWAKE   (1 << 23)	/* GCI Wake Event Interrupt */
#define GCI_INTMASK_SEMAPHORE   (1 << 24)	/* HW Semaphore Interrupt */
#define GCI_INTMASK_GPIOINT	(1 << 25)	/**< GCIGpioInt */
#define GCI_INTMASK_GPIOWAKE	(1 << 26)	/**< GCIGpioWake */
#define GCI_INTMASK_LHLWLWAKE	(1 << 30)	/* LHL WL wake */

/* GCI WakeMask Register bits. */
#define GCI_WAKEMASK_RBI	(1 << 0)	/**< Rx Break Interrupt */
#define GCI_WAKEMASK_UB		(1 << 1)	/**< UART Break Interrupt */
#define GCI_WAKEMASK_SPE	(1 << 2)	/**< SECI Parity Error Interrupt */
#define GCI_WAKEMASK_SFE	(1 << 3)	/**< SECI Framing Error Interrupt */
#define GCI_WAKE_SRITI		(1 << 9)	/**< SECI Rx Idle Timer Interrupt */
#define GCI_WAKEMASK_STFF	(1 << 10)	/**< SECI Tx FIFO Full Interrupt */
#define GCI_WAKEMASK_STFAE	(1 << 11)	/**< SECI Tx FIFO Almost Empty Intr */
#define GCI_WAKEMASK_SRFAF	(1 << 12)	/**< SECI Rx FIFO Almost Full */
#define GCI_WAKEMASK_SRFNE	(1 << 14)	/**< SECI Rx FIFO Not Empty */
#define GCI_WAKEMASK_SRFOF	(1 << 15)	/**< SECI Rx FIFO Not Empty Timeout */
#define GCI_WAKEMASK_EVENT   (1 << 21)   /* GCI Event Interrupt */
#define GCI_WAKEMASK_LEVELWAKE  (1 << 22)   /* GCI Wake Level Interrupt */
#define GCI_WAKEMASK_EVENTWAKE  (1 << 23)   /* GCI Wake Event Interrupt */
#define GCI_WAKEMASK_GPIOINT	(1 << 25)	/**< GCIGpioInt */
#define GCI_WAKEMASK_GPIOWAKE	(1 << 26)	/**< GCIGpioWake */
#define GCI_WAKEMASK_LHLWLWAKE	(1 << 30)	/* LHL WL wake */

#define	GCI_WAKE_ON_GCI_GPIO1	1
#define	GCI_WAKE_ON_GCI_GPIO2	2
#define	GCI_WAKE_ON_GCI_GPIO3	3
#define	GCI_WAKE_ON_GCI_GPIO4	4
#define	GCI_WAKE_ON_GCI_GPIO5	5
#define	GCI_WAKE_ON_GCI_GPIO6	6
#define	GCI_WAKE_ON_GCI_GPIO7	7
#define	GCI_WAKE_ON_GCI_GPIO8	8
#define	GCI_WAKE_ON_GCI_SECI_IN	9

#define	PMU_EXT_WAKE_MASK_0_SDIO		(1u << 2u)
#define	PMU_EXT_WAKE_MASK_0_PCIE_PERST		(1u << 5u)

/* =========== LHL regs =========== */
#define LHL_PWRSEQCTL_SLEEP_EN			(1 << 0)
#define LHL_PWRSEQCTL_PMU_SLEEP_MODE		(1 << 1)
#define LHL_PWRSEQCTL_PMU_FINAL_PMU_SLEEP_EN	(1 << 2)
#define LHL_PWRSEQCTL_PMU_TOP_ISO_EN		(1 << 3)
#define LHL_PWRSEQCTL_PMU_TOP_SLB_EN		(1 << 4)
#define LHL_PWRSEQCTL_PMU_TOP_PWRSW_EN		(1 << 5)
#define LHL_PWRSEQCTL_PMU_CLDO_PD		(1 << 6)
#define LHL_PWRSEQCTL_PMU_LPLDO_PD		(1 << 7)
#define LHL_PWRSEQCTL_PMU_RSRC6_EN		(1 << 8)

#define PMU_SLEEP_MODE_0	(LHL_PWRSEQCTL_SLEEP_EN |\
				LHL_PWRSEQCTL_PMU_FINAL_PMU_SLEEP_EN)

#define PMU_SLEEP_MODE_1	(LHL_PWRSEQCTL_SLEEP_EN |\
				  LHL_PWRSEQCTL_PMU_SLEEP_MODE |\
				  LHL_PWRSEQCTL_PMU_FINAL_PMU_SLEEP_EN |\
				  LHL_PWRSEQCTL_PMU_TOP_ISO_EN |\
				  LHL_PWRSEQCTL_PMU_TOP_SLB_EN |\
				  LHL_PWRSEQCTL_PMU_TOP_PWRSW_EN |\
				  LHL_PWRSEQCTL_PMU_CLDO_PD |\
				  LHL_PWRSEQCTL_PMU_RSRC6_EN)

#define PMU_SLEEP_MODE_2	(LHL_PWRSEQCTL_SLEEP_EN |\
				  LHL_PWRSEQCTL_PMU_SLEEP_MODE |\
				  LHL_PWRSEQCTL_PMU_FINAL_PMU_SLEEP_EN |\
				  LHL_PWRSEQCTL_PMU_TOP_ISO_EN |\
				  LHL_PWRSEQCTL_PMU_TOP_SLB_EN |\
				  LHL_PWRSEQCTL_PMU_TOP_PWRSW_EN |\
				  LHL_PWRSEQCTL_PMU_CLDO_PD |\
				  LHL_PWRSEQCTL_PMU_LPLDO_PD |\
				  LHL_PWRSEQCTL_PMU_RSRC6_EN)

#define LHL_PWRSEQ_CTL				(0x000000ff)

/* LHL Top Level Power Up Control Register (lhl_top_pwrup_ctl_adr, Offset 0xE78)
* Top Level Counter values for isolation, retention, Power Switch control
*/
#define LHL_PWRUP_ISOLATION_CNT			(0x6 << 8)
#define LHL_PWRUP_RETENTION_CNT			(0x5 << 16)
#define LHL_PWRUP_PWRSW_CNT			(0x7 << 24)
/* Mask is taken only for isolation 8:13 , Retention 16:21 ,
* Power Switch control 24:29
*/
#define LHL_PWRUP_CTL_MASK			(0x3F3F3F00)
#define LHL_PWRUP_CTL				(LHL_PWRUP_ISOLATION_CNT |\
						LHL_PWRUP_RETENTION_CNT |\
						LHL_PWRUP_PWRSW_CNT)

#define LHL_PWRUP2_CLDO_DN_CNT			(0x0)
#define LHL_PWRUP2_LPLDO_DN_CNT			(0x0 << 8)
#define LHL_PWRUP2_RSRC6_DN_CN			(0x4 << 16)
#define LHL_PWRUP2_RSRC7_DN_CN			(0x0 << 24)
#define LHL_PWRUP2_CTL_MASK			(0x3F3F3F3F)
#define LHL_PWRUP2_CTL				(LHL_PWRUP2_CLDO_DN_CNT |\
						LHL_PWRUP2_LPLDO_DN_CNT |\
						LHL_PWRUP2_RSRC6_DN_CN |\
						LHL_PWRUP2_RSRC7_DN_CN)

/* LHL Top Level Power Down Control Register (lhl_top_pwrdn_ctl_adr, Offset 0xE74) */
#define LHL_PWRDN_SLEEP_CNT			(0x4)
#define LHL_PWRDN_CTL_MASK			(0x3F)

/* LHL Top Level Power Down Control 2 Register (lhl_top_pwrdn2_ctl_adr, Offset 0xE80) */
#define LHL_PWRDN2_CLDO_DN_CNT			(0x4)
#define LHL_PWRDN2_LPLDO_DN_CNT			(0x4 << 8)
#define LHL_PWRDN2_RSRC6_DN_CN			(0x3 << 16)
#define LHL_PWRDN2_RSRC7_DN_CN			(0x0 << 24)
#define LHL_PWRDN2_CTL				(LHL_PWRDN2_CLDO_DN_CNT |\
						LHL_PWRDN2_LPLDO_DN_CNT |\
						LHL_PWRDN2_RSRC6_DN_CN |\
						LHL_PWRDN2_RSRC7_DN_CN)
#define LHL_PWRDN2_CTL_MASK			(0x3F3F3F3F)

#define LHL_FAST_WRITE_EN			(1 << 14)

#define LHL_WL_MACTIMER_MASK			0xFFFFFFFF
/* Write 1 to clear */
#define LHL_WL_MACTIMER_INT_ST_MASK		(0x1u)

/* WL ARM Timer0 Interrupt Mask (lhl_wl_armtim0_intrp_adr) */
#define LHL_WL_ARMTIM0_INTRP_EN			0x00000001
#define LHL_WL_ARMTIM0_INTRP_EDGE_TRIGGER	0x00000002

/* WL ARM Timer0 Interrupt Status (lhl_wl_armtim0_st_adr) */
#define LHL_WL_ARMTIM0_ST_WL_ARMTIM_INT_ST	0x00000001u

/* WL ARM Timer1 Interrupt Mask (lhl_wl_armtim1_intrp_adr) */
#define LHL_WL_ARMTIM1_INTRP_EN			0x00000001
#define LHL_WL_ARMTIM1_INTRP_EDGE_TRIGGER	0x00000002

/* WL ARM Timer1 Interrupt Status (lhl_wl_armtim1_st_adr) */
#define LHL_WL_ARMTIM1_ST_WL_ARMTIM_INT_ST	0x00000001

/* WL MAC TimerX Interrupt Mask (lhl_wl_mactimX_intrp_adr) */
#define LHL_WL_MACTIM_INTRP_EN			0x00000001
#define LHL_WL_MACTIM_INTRP_EDGE_TRIGGER	0x00000002

/* WL MAC TimerX Interrupt Status (lhl_wl_mactimX_st_adr) */
#define LHL_WL_MACTIM_ST_WL_MACTIM_INT_ST	0x00000001

/* LHL Wakeup Status (lhl_wkup_status_adr) */
#define LHL_WKUP_STATUS_WR_PENDING_ARMTIM0	0x00100000
#define LHL_WKUP_STATUS_WR_PENDING_ARMTIM1	0x00200000

/* LHL Wakeup Status (regon_intrp_st_adr) */
#define LHL_WL_REG_ON_INTR_ST			0x00000004

#define LHL_PS_MODE_0	0
#define LHL_PS_MODE_1	1

/* IOCFG_P_ADDR register bits */
#define LHL_IOCFG_P_ADDR_EDGE_TRIGGER_SHIFT           (0u)
#define LHL_IOCFG_P_ADDR_EDGE_TRIGGER_MASK            (1u << IOCFG_P_ADDR_EDGE_TRIGGER_SHIFT)
#define LHL_IOCFG_P_ADDR_NEG_EDGE_SHIFT               (1u)
#define LHL_IOCFG_P_ADDR_NEG_EDGE_MASK                (1u << LHL_IOCFG_P_ADDR_NEG_EDGE_SHIFT)
#define LHL_IOCFG_P_ADDR_DUAL_EDGE_SHIFT              (2u)
#define LHL_IOCFG_P_ADDR_DUAL_EDGE_MASK               (1u << LHL_IOCFG_P_ADDR_DUAL_EDGE_SHIFT)
#define LHL_IOCFG_P_ADDR_LHL_GPIO_DOUT_SEL_SHIFT      (3u)
#define LHL_IOCFG_P_ADDR_LHL_GPIO_DOUT_SEL_MASK \
	(0x1u << LHL_IOCFG_P_ADDR_LHL_GPIO_DOUT_SEL_SHIFT)
#define LHL_IOCFG_P_ADDR_LHL_GPIO_DOUT_SHIFT          (4u)
#define LHL_IOCFG_P_ADDR_LHL_GPIO_DOUT_MASK           (1u << LHL_IOCFG_P_ADDR_LHL_GPIO_DOUT_SHIFT)
#define LHL_IOCFG_P_ADDR_LHL_GPIO_OE_SHIFT            (5u)
#define LHL_IOCFG_P_ADDR_LHL_GPIO_OE_MASK             (1u << LHL_IOCFG_P_ADDR_LHL_GPIO_OE_SHIFT)
#define LHL_IOCFG_P_ADDR_SET_WL_DIN_AS_WAKE_SRC_SHIFT (6u)
#define LHL_IOCFG_P_ADDR_SET_WL_DIN_AS_WAKE_SRC_MASK \
	(1u << LHL_IOCFG_P_ADDR_SET_WL_DIN_AS_WAKE_SRC_SHIFT)
#define LHL_IOCFG_P_ADDR_PUP_PDN_SHIFT                (9u)
#define LHL_IOCFG_P_ADDR_PUP_PDN_MASK                 (3u << LHL_IOCFG_P_ADDR_PUP_PDN_SHIFT)
#define LHL_IOCFG_P_ADDR_DRIVE_STRENGTH_SEL_SHIFT     (11u)
#define LHL_IOCFG_P_ADDR_DRIVE_STRENGTH_SEL_MASK \
	(7u << LHL_IOCFG_P_ADDR_DRIVE_STRENGTH_SEL_SHIFT)
#define LHL_IOCFG_P_ADDR_SLEW_SHIFT                   (14u)
#define LHL_IOCFG_P_ADDR_SLEW_MASK                    (1u << LHL_IOCFG_P_ADDR_SLEW_SHIFT)
#define LHL_IOCFG_P_ADDR_HYSTERESIS_SHIFT             (15u)
#define LHL_IOCFG_P_ADDR_HYSTERESIS_MASK              (1u << LHL_IOCFG_P_ADDR_HYSTERESIS_SHIFT)

/* GCI EventIntMask Register SW bits */
#define GCI_MAILBOXDATA_TOWLAN	(1 << 0)
#define GCI_MAILBOXDATA_TOBT		(1 << 1)
#define GCI_MAILBOXDATA_TONFC		(1 << 2)
#define GCI_MAILBOXDATA_TOGPS		(1 << 3)
#define GCI_MAILBOXDATA_TOLTE		(1 << 4)
#define GCI_MAILBOXACK_TOWLAN		(1 << 8)
#define GCI_MAILBOXACK_TOBT		(1 << 9)
#define GCI_MAILBOXACK_TONFC		(1 << 10)
#define GCI_MAILBOXACK_TOGPS		(1 << 11)
#define GCI_MAILBOXACK_TOLTE		(1 << 12)
#define GCI_WAKE_TOWLAN				(1 << 16)
#define GCI_WAKE_TOBT				(1 << 17)
#define GCI_WAKE_TONFC				(1 << 18)
#define GCI_WAKE_TOGPS				(1 << 19)
#define GCI_WAKE_TOLTE				(1 << 20)
#define GCI_SWREADY					(1 << 24)

/* GCI SECI_OUT TX Status Regiser bits */
#define GCI_SECIOUT_TXSTATUS_TXHALT		(1 << 0)
#define GCI_SECIOUT_TXSTATUS_TI			(1 << 16)

/*
* Maximum delay for the PMU state transition in us.
* This is an upper bound intended for spinwaits etc.
*/
#if defined(BCMQT)
#define PMU_MAX_TRANSITION_DLY	1500000
#else
#define PMU_MAX_TRANSITION_DLY	15000
#endif /* BCMDONGLEHOST */

/* PMU resource up transition time in ILP cycles */
#define PMURES_UP_TRANSITION	2

#if !defined(BCMDONGLEHOST)
/*
* Information from BT to WLAN over eci_inputlo, eci_inputmi &
* eci_inputhi register.  Rev >=21
*/
/* Fields in eci_inputlo register - [0:31] */
#define	ECI_INLO_TASKTYPE_MASK	0x0000000f /* [3:0] - 4 bits */
#define ECI_INLO_TASKTYPE_SHIFT 0
#define	ECI_INLO_PKTDUR_MASK	0x000000f0 /* [7:4] - 4 bits */
#define ECI_INLO_PKTDUR_SHIFT	4
#define	ECI_INLO_ROLE_MASK	0x00000100 /* [8] - 1 bits */
#define ECI_INLO_ROLE_SHIFT	8
#define	ECI_INLO_MLP_MASK	0x00000e00 /* [11:9] - 3 bits */
#define ECI_INLO_MLP_SHIFT	9
#define	ECI_INLO_TXPWR_MASK	0x000ff000 /* [19:12] - 8 bits */
#define ECI_INLO_TXPWR_SHIFT	12
#define	ECI_INLO_RSSI_MASK	0x0ff00000 /* [27:20] - 8 bits */
#define ECI_INLO_RSSI_SHIFT	20
#define	ECI_INLO_VAD_MASK	0x10000000 /* [28] - 1 bits */
#define ECI_INLO_VAD_SHIFT	28

/*
* Register eci_inputlo bitfield values.
* - BT packet type information bits [7:0]
*/
/*  [3:0] - Task (link) type */
#define BT_ACL				0x00
#define BT_SCO				0x01
#define BT_eSCO				0x02
#define BT_A2DP				0x03
#define BT_SNIFF			0x04
#define BT_PAGE_SCAN			0x05
#define BT_INQUIRY_SCAN			0x06
#define BT_PAGE				0x07
#define BT_INQUIRY			0x08
#define BT_MSS				0x09
#define BT_PARK				0x0a
#define BT_RSSISCAN			0x0b
#define BT_MD_ACL			0x0c
#define BT_MD_eSCO			0x0d
#define BT_SCAN_WITH_SCO_LINK		0x0e
#define BT_SCAN_WITHOUT_SCO_LINK	0x0f
/* [7:4] = packet duration code */
/* [8] - Master / Slave */
#define BT_MASTER			0
#define BT_SLAVE			1
/* [11:9] - multi-level priority */
#define BT_LOWEST_PRIO			0x0
#define BT_HIGHEST_PRIO			0x3
/* [19:12] - BT transmit power */
/* [27:20] - BT RSSI */
/* [28] - VAD silence */
/* [31:29] - Undefined */
/* Register eci_inputmi values - [32:63] - none defined */
/* [63:32] - Undefined */

/* Information from WLAN to BT over eci_output register. */
/* Fields in eci_output register - [0:31] */
#define ECI48_OUT_MASKMAGIC_HIWORD 0x55550000
#define ECI_OUT_CHANNEL_MASK(ccrev) (ECI48_OUT_MASKMAGIC_HIWORD | 0xf000)
#define ECI_OUT_CHANNEL_SHIFT(ccrev) (12)
#define ECI_OUT_BW_MASK(ccrev) (ECI48_OUT_MASKMAGIC_HIWORD | 0xe00)
#define ECI_OUT_BW_SHIFT(ccrev) (9)
#define ECI_OUT_ANTENNA_MASK(ccrev) (ECI48_OUT_MASKMAGIC_HIWORD | 0x100)
#define ECI_OUT_ANTENNA_SHIFT(ccrev) (8)
#define ECI_OUT_SIMUL_TXRX_MASK(ccrev) (ECI48_OUT_MASKMAGIC_HIWORD | 0x80)
#define ECI_OUT_SIMUL_TXRX_SHIFT(ccrev) (7)
#define ECI_OUT_FM_DISABLE_MASK(ccrev) (ECI48_OUT_MASKMAGIC_HIWORD | 0x40)
#define ECI_OUT_FM_DISABLE_SHIFT(ccrev) (6)

/* Indicate control of ECI bits between s/w and dot11mac.
 * 0 => FW control, 1=> MAC/ucode control

 * Current assignment (ccrev >= 35):
 *  0 - TxConf (ucode)
 * 38 - FM disable (wl)
 * 39 - Allow sim rx (ucode)
 * 40 - Num antennas (wl)
 * 43:41 - WLAN channel exclusion BW (wl)
 * 47:44 - WLAN channel (wl)
 */
#define ECI_MACCTRL_BITS	0xbffb0000
#define ECI_MACCTRLLO_BITS	0x1
#define ECI_MACCTRLHI_BITS	0xFF

#endif /* !defined(BCMDONGLEHOST) */

/* SECI Status (0x134) & Mask (0x138) bits - Rev 35 */
#define SECI_STAT_BI	(1 << 0)	/* Break Interrupt */
#define SECI_STAT_SPE	(1 << 1)	/* Parity Error */
#define SECI_STAT_SFE	(1 << 2)	/* Parity Error */
#define SECI_STAT_SDU	(1 << 3)	/* Data Updated */
#define SECI_STAT_SADU	(1 << 4)	/* Auxiliary Data Updated */
#define SECI_STAT_SAS	(1 << 6)	/* AUX State */
#define SECI_STAT_SAS2	(1 << 7)	/* AUX2 State */
#define SECI_STAT_SRITI	(1 << 8)	/* Idle Timer Interrupt */
#define SECI_STAT_STFF	(1 << 9)	/* Tx FIFO Full */
#define SECI_STAT_STFAE	(1 << 10)	/* Tx FIFO Almost Empty */
#define SECI_STAT_SRFE	(1 << 11)	/* Rx FIFO Empty */
#define SECI_STAT_SRFAF	(1 << 12)	/* Rx FIFO Almost Full */
#define SECI_STAT_SFCE	(1 << 13)	/* Flow Control Event */

/* SECI configuration */
#define SECI_MODE_UART			0x0
#define SECI_MODE_SECI			0x1
#define SECI_MODE_LEGACY_3WIRE_BT	0x2
#define SECI_MODE_LEGACY_3WIRE_WLAN	0x3
#define SECI_MODE_HALF_SECI		0x4

#define SECI_RESET		(1 << 0)
#define SECI_RESET_BAR_UART	(1 << 1)
#define SECI_ENAB_SECI_ECI	(1 << 2)
#define SECI_ENAB_SECIOUT_DIS	(1 << 3)
#define SECI_MODE_MASK		0x7
#define SECI_MODE_SHIFT		4 /* (bits 5, 6, 7) */
#define SECI_UPD_SECI		(1 << 7)

#define SECI_AUX_TX_START       (1 << 31)
#define SECI_SLIP_ESC_CHAR	0xDB
#define SECI_SIGNOFF_0		SECI_SLIP_ESC_CHAR
#define SECI_SIGNOFF_1     0
#define SECI_REFRESH_REQ	0xDA

/* seci clk_ctl_st bits */
#define CLKCTL_STS_HT_AVAIL_REQ		(1 << 4)
#define CLKCTL_STS_SECI_CLK_REQ		(1 << 8)
#define CLKCTL_STS_SECI_CLK_AVAIL	(1 << 24)

#define SECI_UART_MSR_CTS_STATE		(1 << 0)
#define SECI_UART_MSR_RTS_STATE		(1 << 1)
#define SECI_UART_SECI_IN_STATE		(1 << 2)
#define SECI_UART_SECI_IN2_STATE	(1 << 3)

/* GCI RX FIFO Control Register */
#define	GCI_RXF_LVL_MASK	(0xFF << 0)
#define	GCI_RXF_TIMEOUT_MASK	(0xFF << 8)

/* GCI UART Registers' Bit definitions */
/* Seci Fifo Level Register */
#define	SECI_TXF_LVL_MASK	(0x3F << 8)
#define	TXF_AE_LVL_DEFAULT	0x4
#define	SECI_RXF_LVL_FC_MASK	(0x3F << 16)

/* SeciUARTFCR Bit definitions */
#define	SECI_UART_FCR_RFR		(1 << 0)
#define	SECI_UART_FCR_TFR		(1 << 1)
#define	SECI_UART_FCR_SR		(1 << 2)
#define	SECI_UART_FCR_THP		(1 << 3)
#define	SECI_UART_FCR_AB		(1 << 4)
#define	SECI_UART_FCR_ATOE		(1 << 5)
#define	SECI_UART_FCR_ARTSOE		(1 << 6)
#define	SECI_UART_FCR_ABV		(1 << 7)
#define	SECI_UART_FCR_ALM		(1 << 8)

/* SECI UART LCR register bits */
#define SECI_UART_LCR_STOP_BITS		(1 << 0) /* 0 - 1bit, 1 - 2bits */
#define SECI_UART_LCR_PARITY_EN		(1 << 1)
#define SECI_UART_LCR_PARITY		(1 << 2) /* 0 - odd, 1 - even */
#define SECI_UART_LCR_RX_EN		(1 << 3)
#define SECI_UART_LCR_LBRK_CTRL		(1 << 4) /* 1 => SECI_OUT held low */
#define SECI_UART_LCR_TXO_EN		(1 << 5)
#define SECI_UART_LCR_RTSO_EN		(1 << 6)
#define SECI_UART_LCR_SLIPMODE_EN	(1 << 7)
#define SECI_UART_LCR_RXCRC_CHK		(1 << 8)
#define SECI_UART_LCR_TXCRC_INV		(1 << 9)
#define SECI_UART_LCR_TXCRC_LSBF	(1 << 10)
#define SECI_UART_LCR_TXCRC_EN		(1 << 11)
#define	SECI_UART_LCR_RXSYNC_EN		(1 << 12)

#define SECI_UART_MCR_TX_EN		(1 << 0)
#define SECI_UART_MCR_PRTS		(1 << 1)
#define SECI_UART_MCR_SWFLCTRL_EN	(1 << 2)
#define SECI_UART_MCR_HIGHRATE_EN	(1 << 3)
#define SECI_UART_MCR_LOOPBK_EN		(1 << 4)
#define SECI_UART_MCR_AUTO_RTS		(1 << 5)
#define SECI_UART_MCR_AUTO_TX_DIS	(1 << 6)
#define SECI_UART_MCR_BAUD_ADJ_EN	(1 << 7)
#define SECI_UART_MCR_XONOFF_RPT	(1 << 9)

/* SeciUARTLSR Bit Mask */
#define	SECI_UART_LSR_RXOVR_MASK	(1 << 0)
#define	SECI_UART_LSR_RFF_MASK		(1 << 1)
#define	SECI_UART_LSR_TFNE_MASK		(1 << 2)
#define	SECI_UART_LSR_TI_MASK		(1 << 3)
#define	SECI_UART_LSR_TPR_MASK		(1 << 4)
#define	SECI_UART_LSR_TXHALT_MASK	(1 << 5)

/* SeciUARTMSR Bit Mask */
#define	SECI_UART_MSR_CTSS_MASK		(1 << 0)
#define	SECI_UART_MSR_RTSS_MASK		(1 << 1)
#define	SECI_UART_MSR_SIS_MASK		(1 << 2)
#define	SECI_UART_MSR_SIS2_MASK		(1 << 3)

/* SeciUARTData Bits */
#define SECI_UART_DATA_RF_NOT_EMPTY_BIT	(1 << 12)
#define SECI_UART_DATA_RF_FULL_BIT	(1 << 13)
#define SECI_UART_DATA_RF_OVRFLOW_BIT	(1 << 14)
#define	SECI_UART_DATA_FIFO_PTR_MASK	0xFF
#define	SECI_UART_DATA_RF_RD_PTR_SHIFT	16
#define	SECI_UART_DATA_RF_WR_PTR_SHIFT	24

/* LTECX: ltecxmux */
#define LTECX_EXTRACT_MUX(val, idx)	(getbit4(&(val), (idx)))

/* LTECX: ltecxmux MODE */
#define LTECX_MUX_MODE_IDX		0
#define LTECX_MUX_MODE_WCI2		0x0
#define LTECX_MUX_MODE_GPIO		0x1

/* LTECX GPIO Information Index */
#define LTECX_NVRAM_FSYNC_IDX	0
#define LTECX_NVRAM_LTERX_IDX	1
#define LTECX_NVRAM_LTETX_IDX	2
#define LTECX_NVRAM_WLPRIO_IDX	3

/* LTECX WCI2 Information Index */
#define LTECX_NVRAM_WCI2IN_IDX	0
#define LTECX_NVRAM_WCI2OUT_IDX	1

/* LTECX: Macros to get GPIO/FNSEL/GCIGPIO */
#define LTECX_EXTRACT_PADNUM(val, idx)	(getbit8(&(val), (idx)))
#define LTECX_EXTRACT_FNSEL(val, idx)	(getbit4(&(val), (idx)))
#define LTECX_EXTRACT_GCIGPIO(val, idx)	(getbit4(&(val), (idx)))

/* WLAN channel numbers - used from wifi.h */

/* WLAN BW */
#define ECI_BW_20   0x0
#define ECI_BW_25   0x1
#define ECI_BW_30   0x2
#define ECI_BW_35   0x3
#define ECI_BW_40   0x4
#define ECI_BW_45   0x5
#define ECI_BW_50   0x6
#define ECI_BW_ALL  0x7

/* WLAN - number of antenna */
#define WLAN_NUM_ANT1 TXANT_0
#define WLAN_NUM_ANT2 TXANT_1

/* otpctrl1 0xF4 */
#define OTPC_FORCE_PWR_OFF	0x02000000
/* chipcommon s/r registers introduced with cc rev >= 48 */
#define CC_SR_CTL0_ENABLE_MASK             0x1
#define CC_SR_CTL0_ENABLE_SHIFT              0
#define CC_SR_CTL0_EN_SR_ENG_CLK_SHIFT       1 /* sr_clk to sr_memory enable */
#define CC_SR_CTL0_RSRC_TRIGGER_SHIFT        2 /* Rising edge resource trigger 0 to sr_engine  */
#define CC_SR_CTL0_MIN_DIV_SHIFT             6 /* Min division value for fast clk in sr_engine */
#define CC_SR_CTL0_EN_SBC_STBY_SHIFT        16 /* Allow Subcore mem StandBy? */
#define CC_SR_CTL0_EN_SR_ALP_CLK_MASK_SHIFT 18
#define CC_SR_CTL0_EN_SR_HT_CLK_SHIFT       19
#define CC_SR_CTL0_ALLOW_PIC_SHIFT          20 /* Allow pic to separate power domains */
#define CC_SR_CTL0_MAX_SR_LQ_CLK_CNT_SHIFT  25
#define CC_SR_CTL0_EN_MEM_DISABLE_FOR_SLEEP 30

#define CC_SR_CTL1_SR_INIT_MASK             0x3FF
#define CC_SR_CTL1_SR_INIT_SHIFT            0

#define	ECI_INLO_PKTDUR_MASK	0x000000f0 /* [7:4] - 4 bits */
#define ECI_INLO_PKTDUR_SHIFT	4

/* gci chip control bits */
#define GCI_GPIO_CHIPCTRL_ENAB_IN_BIT		0u
#define GCI_GPIO_CHIPCTRL_ENAB_OP_BIT		1u
#define GCI_GPIO_CHIPCTRL_INVERT_BIT		2u
#define GCI_GPIO_CHIPCTRL_PULLUP_BIT		3u
#define GCI_GPIO_CHIPCTRL_PULLDN_BIT		4u
#define GCI_GPIO_CHIPCTRL_ENAB_BTSIG_BIT	5u
#define GCI_GPIO_CHIPCTRL_ENAB_OD_OP_BIT	6u
#define GCI_GPIO_CHIPCTRL_ENAB_EXT_GPIO_BIT	7u

/* gci GPIO input status bits */
#define GCI_GPIO_STS_VALUE_BIT			0u
#define GCI_GPIO_STS_POS_EDGE_BIT		1u
#define GCI_GPIO_STS_NEG_EDGE_BIT		2u
#define GCI_GPIO_STS_FAST_EDGE_BIT		3u
#define GCI_GPIO_STS_CLEAR			0xF
#define GCI_GPIO_STS_MASK			0xF

#define GCI_GPIO_STS_EDGE_TRIG_BIT		0u
#define GCI_GPIO_STS_NEG_EDGE_TRIG_BIT		1u
#define GCI_GPIO_STS_DUAL_EDGE_TRIG_BIT		2u
#define GCI_GPIO_STS_WL_DIN_SELECT		6u
#define GCI_GPIO_STS_TRANSPARENT_MODE		7u

#define GCI_GPIO_STS_VALUE			(1u << GCI_GPIO_STS_VALUE_BIT)
#define GCI_GPIO_STS_POS_EDGE_VALUE		(1u << GCI_GPIO_STS_POS_EDGE_BIT)
#define GCI_GPIO_STS_NEG_EDGE_VALUE		(1u << GCI_GPIO_STS_NEG_EDGE_BIT)

/* SR Power Control */
#define SRPWR_DMN0_PCIE			(0)				/* PCIE */
#define SRPWR_DMN0_PCIE_SHIFT		(SRPWR_DMN0_PCIE)		/* PCIE */
#define SRPWR_DMN0_PCIE_MASK		(1 << SRPWR_DMN0_PCIE_SHIFT)	/* PCIE */
#define SRPWR_DMN1_ARMBPSD		(1)				/* ARM/BP/SDIO */
#define SRPWR_DMN1_ARMBPSD_SHIFT	(SRPWR_DMN1_ARMBPSD)		/* ARM/BP/SDIO */
#define SRPWR_DMN1_ARMBPSD_MASK		(1 << SRPWR_DMN1_ARMBPSD_SHIFT)	/* ARM/BP/SDIO */
#define SRPWR_DMN2_MACAUX		(2)				/* MAC/Phy Aux */
#define SRPWR_DMN2_MACAUX_SHIFT		(SRPWR_DMN2_MACAUX)		/* MAC/Phy Aux */
#define SRPWR_DMN2_MACAUX_MASK		(1 << SRPWR_DMN2_MACAUX_SHIFT)	/* MAC/Phy Aux */
#define SRPWR_DMN3_MACMAIN		(3)				/* MAC/Phy Main */
#define SRPWR_DMN3_MACMAIN_SHIFT	(SRPWR_DMN3_MACMAIN)		/* MAC/Phy Main */
#define SRPWR_DMN3_MACMAIN_MASK		(1 << SRPWR_DMN3_MACMAIN_SHIFT)	/* MAC/Phy Main */

#define SRPWR_DMN4_MACSCAN		(4)				/* MAC/Phy Scan */
#define SRPWR_DMN4_MACSCAN_SHIFT	(SRPWR_DMN4_MACSCAN)		/* MAC/Phy Scan */
#define SRPWR_DMN4_MACSCAN_MASK		(1 << SRPWR_DMN4_MACSCAN_SHIFT)	/* MAC/Phy Scan */

#define SRPWR_DMN5_BT			(5)				/* BT */

#define SRPWR_DMN6_SAQM			(6)				/* sAQM */
#define SRPWR_DMN6_SAQM_SHIFT		(SRPWR_DMN6_SAQM)		/* sAQM */
#define SRPWR_DMN6_SAQM_MASK		(1 << SRPWR_DMN6_SAQM_SHIFT)	/* sAQM */

#define SRPWR_DMN_MAX			(7)	/* Domains 0-6 */

/* all power domain mask */
#define SRPWR_DMN_ALL_MASK(sih)		si_srpwr_domain_all_mask(sih)

#define SRPWR_REQON_SHIFT		(8)	/* PowerOnRequest[11:8] */
#define SRPWR_REQON_MASK(sih)		(SRPWR_DMN_ALL_MASK(sih) << SRPWR_REQON_SHIFT)

#define SRPWR_STATUS_SHIFT		(16)	/* ExtPwrStatus[19:16], RO */
#define SRPWR_STATUS_MASK(sih)		(SRPWR_DMN_ALL_MASK(sih) << SRPWR_STATUS_SHIFT)

#define SRPWR_BT_STATUS_SHIFT		(20)	/* PowerDomain[20:21], RO */
#define SRPWR_BT_STATUS_MASK		(0x3)

#define SRPWR_DMN_ID_SHIFT		(28)	/* PowerDomain[31:28], RO */
#define SRPWR_DMN_ID_MASK		(0xF)

#define SRPWR_UP_DOWN_DELAY		100	/* more than 3 ILP clocks */

/* PMU Precision Usec Timer */
#define PMU_PREC_USEC_TIMER_ENABLE	0x1

/* Random Number/Bit Generator defines */
#define	MASK_1BIT(offset)			(0x1u << offset)

#define	CC_RNG_CTRL_0_RBG_EN_SHIFT		(0u)
#define	CC_RNG_CTRL_0_RBG_EN_MASK		(0x1FFFu << CC_RNG_CTRL_0_RBG_EN_SHIFT)
#define	CC_RNG_CTRL_0_RBG_EN			(0x1FFFu)
#define CC_RNG_CTRL_0_RBG_DEV_CTRL_SHIFT	(13u)
#define CC_RNG_CTRL_0_RBG_DEV_CTRL_MASK		(0x3u << CC_RNG_CTRL_0_RBG_DEV_CTRL_SHIFT)
#define CC_RNG_CTRL_0_RBG_DEV_CTRL_1MHz		(0x3u << CC_RNG_CTRL_0_RBG_DEV_CTRL_SHIFT)
#define CC_RNG_CTRL_0_RBG_DEV_CTRL_2MHz		(0x2u << CC_RNG_CTRL_0_RBG_DEV_CTRL_SHIFT)
#define CC_RNG_CTRL_0_RBG_DEV_CTRL_4MHz		(0x1u << CC_RNG_CTRL_0_RBG_DEV_CTRL_SHIFT)
#define CC_RNG_CTRL_0_RBG_DEV_CTRL_8MHz		(0x0u << CC_RNG_CTRL_0_RBG_DEV_CTRL_SHIFT)

/* RNG_SOFT_RESET */
#define CC_RNG_SOFT_RESET_SHIFT		(0u)
#define CC_RNG_SOFT_RESET_MASK		(0x1u << CC_RNG_FIFO_COUNT_RFC_SHIFT)

/* RNG_FIFO_COUNT */
/* RFC - RNG FIFO COUNT */
#define CC_RNG_FIFO_COUNT_RFC_SHIFT		(0u)
#define CC_RNG_FIFO_COUNT_RFC_MASK		(0xFFu << CC_RNG_FIFO_COUNT_RFC_SHIFT)

/* RNG interrupt */
#define CC_RNG_TOT_BITS_CNT_IRQ_SHIFT		(0u)
#define CC_RNG_TOT_BITS_CNT_IRQ_MASK		(0x1u << CC_RNG_TOT_BITS_CNT_IRQ_SHIFT)
#define CC_RNG_TOT_BITS_MAX_IRQ_SHIFT		(1u)
#define CC_RNG_TOT_BITS_MAX_IRQ_MASK		(0x1u << CC_RNG_TOT_BITS_MAX_IRQ_SHIFT)
#define CC_RNG_FIFO_FULL_IRQ_SHIFT		(2u)
#define CC_RNG_FIFO_FULL_IRQ_MASK		(0x1u << CC_RNG_FIFO_FULL_IRQ_SHIFT)
#define CC_RNG_FIFO_OVER_RUN_IRQ_SHIFT		(3u)
#define CC_RNG_FIFO_OVER_RUN_IRQ_MASK		(0x1u << CC_RNG_FIFO_OVER_RUN_IRQ_SHIFT)
#define CC_RNG_FIFO_UNDER_RUN_IRQ_SHIFT		(4u)
#define CC_RNG_FIFO_UNDER_RUN_IRQ_MASK		(0x1u << CC_RNG_FIFO_UNDER_RUN_IRQ_SHIFT)
#define CC_RNG_NIST_FAIL_IRQ_SHIFT		(5u)
#define CC_RNG_NIST_FAIL_IRQ_MASK		(0x1u << CC_RNG_NIST_FAIL_IRQ_SHIFT)
#define	CC_RNG_STARTUP_TRANSITION_MET_IRQ_SHIFT	(17u)
#define	CC_RNG_STARTUP_TRANSITION_MET_IRQ_MASK	(0x1u << \
						CC_RNG_STARTUP_TRANSITION_MET_IRQ_SHIFT)
#define	CC_RNG_MASTER_FAIL_LOCKOUT_IRQ_SHIFT	(31u)
#define	CC_RNG_MASTER_FAIL_LOCKOUT_IRQ_MASK	(0x1u << \
						CC_RNG_MASTER_FAIL_LOCKOUT_IRQ_SHIFT)

/* FISCtrlStatus */
#define PMU_CLEAR_FIS_DONE_SHIFT	1u
#define PMU_CLEAR_FIS_DONE_MASK	(1u << PMU_CLEAR_FIS_DONE_SHIFT)

#define PMU_FIS_FORCEON_ALL_SHIFT	4u
#define PMU_FIS_FORCEON_ALL_MASK	(1u << PMU_FIS_FORCEON_ALL_SHIFT)

#define PMU_FIS_DN_TIMER_VAL_SHIFT	16u
#define PMU_FIS_DN_TIMER_VAL_MASK	0x7FFF0000u

#define PMU_FIS_DN_TIMER_VAL_4388	0x3f80u	/* micro second */
#define PMU_FIS_DN_TIMER_VAL_4397	0x3f80u	/* micro second */
#define PMU_FIS_DN_TIMER_VAL_4389	0x3f80u	/* micro second */

#define PMU_FIS_PCIE_SAVE_EN_SHIFT	5u
#define PMU_FIS_PCIE_SAVE_EN_VALUE	(1u << PMU_FIS_PCIE_SAVE_EN_SHIFT)

#define PMU_REG6_RFLDO_CTRL              0x000000E0
#define PMU_REG6_RFLDO_CTRL_SHFT         5

#define PMU_REG6_BTLDO_CTRL              0x0000E000
#define PMU_REG6_BTLDO_CTRL_SHFT         13

/* ETBMemCtrl */
#define CC_ETBMEMCTRL_FORCETMCINTFTOETB_SHIFT	1u
#define CC_ETBMEMCTRL_FORCETMCINTFTOETB_MASK	(1u << CC_ETBMEMCTRL_FORCETMCINTFTOETB_SHIFT)

/* SSSR dumps locations on the backplane space */
#define BCM4387_SSSR_DUMP_AXI_MAIN		0xE8C00000u
#define BCM4387_SSSR_DUMP_MAIN_SIZE		160000u
#define BCM4387_SSSR_DUMP_AXI_AUX		0xE8400000u
#define BCM4387_SSSR_DUMP_AUX_SIZE		160000u
#define BCM4387_SSSR_DUMP_AXI_SCAN		0xE9400000u
#define BCM4387_SSSR_DUMP_SCAN_SIZE		32768u

#define BCM4388_SSSR_DUMP_AXI_MAIN		0xE8C00000u
#define BCM4388_SSSR_DUMP_MAIN_SIZE		0x40000u
#define BCM4388_SSSR_DUMP_AXI_AUX		0xE8400000u
#define BCM4388_SSSR_DUMP_AUX_SIZE		0x30000u
#define BCM4388_SSSR_DUMP_AXI_SCAN		0xE9400000u
#define BCM4388_SSSR_DUMP_SCAN_SIZE		0xE000u

#define BCM4387_WLAN_SYS_MEMDOWN_FORCE_PMU	0x444
#define BCM4378_WLAN_SYS_MEMDOWN_FORCE_PMU	0x133

#define CC_GCI_scan5G_dedicated_path_en_core0_ovr_NBIT (14u)
#define CC_GCI_scan5G_dedicated_path_en_core0_ovr_MASK (1u <<\
		CC_GCI_scan5G_dedicated_path_en_core0_ovr_NBIT)

#define CC_GCI_scan5G_dedicated_path_en_core1_ovr_NBIT (16u)
#define CC_GCI_scan5G_dedicated_path_en_core1_ovr_MASK (1u <<\
		CC_GCI_scan5G_dedicated_path_en_core1_ovr_NBIT)

#define CC_GCI_scan5G_dedicated_path_en_core0_ovr_en_NBIT (15u)
#define CC_GCI_scan5G_dedicated_path_en_core0_ovr_en_MASK (1u <<\
		CC_GCI_scan5G_dedicated_path_en_core0_ovr_en_NBIT)

#define CC_GCI_scan5G_dedicated_path_en_core1_ovr_en_NBIT (17u)
#define CC_GCI_scan5G_dedicated_path_en_core1_ovr_en_MASK (1u <<\
		CC_GCI_scan5G_dedicated_path_en_core1_ovr_en_NBIT)

#ifndef BCMCHIPID
/* Enable linker error only when BCMCHIPID is defined.
 * This is to avoid compile error in 43xx build.
 */
#define SI_CTRLREGS_INVALID			0xFFFFFFFFu
#else /* BCMCHIPID */
#define SI_CTRLREGS_INVALID			hnd_invalid_ctrlreg()
#endif /* BCMCHIPID */

#ifdef VLSI_CTRL_REGS

#define VLSI2SW_CTRLREGS()	TRUE

#undef INVALID_REG_gci_chip_cntrl
#undef INVALID_MASK_gci_chip_cntrl
#undef INVALID_REG_gci_chip_status
#undef INVALID_MASK_gci_chip_status
#undef INVALID_REG_pmu_chip_cntrl
#undef INVALID_MASK_pmu_chip_cntrl
#undef INVALID_REG_pmu_pll_cntrl
#undef INVALID_MASK_pmu_pll_cntrl
#undef INVALID_REG_pmu_vreg_cntrl
#undef INVALID_MASK_pmu_vreg_cntrl

/* Force a compile error if any register is referenced that does not exist in the built ctrl
 * register set.
 */
#define INVALID_REG_gci_chip_cntrl		SI_CTRLREGS_INVALID
#define INVALID_MASK_gci_chip_cntrl		SI_CTRLREGS_INVALID
#define INVALID_REG_gci_chip_status		SI_CTRLREGS_INVALID
#define INVALID_MASK_gci_chip_status            SI_CTRLREGS_INVALID
#define INVALID_REG_pmu_chip_cntrl		SI_CTRLREGS_INVALID
#define INVALID_MASK_pmu_chip_cntrl		SI_CTRLREGS_INVALID
#define INVALID_REG_pmu_pll_cntrl		SI_CTRLREGS_INVALID
#define INVALID_MASK_pmu_pll_cntrl		SI_CTRLREGS_INVALID
#define INVALID_REG_pmu_vreg_cntrl		SI_CTRLREGS_INVALID
#define INVALID_MASK_pmu_vreg_cntrl		SI_CTRLREGS_INVALID

/* GCI Chipstatus doesn't use last two arguments for read as this API is read only. */
#define _si_gci_chipstatus_rd(a, b, c, d)	si_gci_chipstatus(a, b)

#define CTRL_REG_NUM_SLICES(regtype, name) \
		regtype##_##name##_SLICES

#define CTRL_REG_NUM(regtype, name, slicenum) \
		regtype##_##name##_##slicenum##_REG

#define CTRL_REG_MASK(regtype, name, slicenum) \
		regtype##_##name##_##slicenum##_REG_MASK

#define CTRL_REG_SHIFT(regtype, name, slicenum) \
		regtype##_##name##_##slicenum##_REG_SHIFT

#define CTRL_FIELD_MASK(regtype, name, slicenum) \
		regtype##_##name##_##slicenum##_FIELD_MASK

#define CTRL_FIELD_SHIFT(regtype, name, slicenum) \
		regtype##_##name##_##slicenum##_FIELD_SHIFT

#define CTRL_WR_VALUE(regtype, name, value, slice) \
	((((value) >> CTRL_FIELD_SHIFT(regtype, name, slice)) & \
	 CTRL_FIELD_MASK(regtype, name, slice)) << CTRL_REG_SHIFT(regtype, name, slice))

#define _SI_REG_WRITE_SLICE(si_fn, sih, regtype, name, value, slicenum) do { \
		if (CTRL_REG_NUM_SLICES(regtype, name) > (slicenum - 1)) { \
			si_fn(sih, CTRL_REG_NUM(regtype, name, slicenum), \
				CTRL_REG_MASK(regtype, name, slicenum), \
				CTRL_WR_VALUE(regtype, name, value, slicenum)); \
		} \
	} while (0)

#define _SI_REG_MOD_SLICE(si_fn, sih, regtype, name, mask, value, slicenum)  do { \
		if (CTRL_REG_NUM_SLICES(regtype, name) > (slicenum - 1)) { \
			si_fn(sih, CTRL_REG_NUM(regtype, name, slicenum), \
				CTRL_WR_VALUE(regtype, name, mask, slicenum), \
				CTRL_WR_VALUE(regtype, name, value, slicenum)); \
		} \
	} while (0)

#define _SI_REG_READ_SLICE_FIELD(si_fn, sih, regtype, name, slice) \
	((si_fn(sih, CTRL_REG_NUM(regtype, name, slice), 0, 0) & \
	  CTRL_REG_MASK(regtype, name, slice)) >> CTRL_REG_SHIFT(regtype, name, slice))

#define _SI_REG_READ_SLICE(si_fn, sih, regtype, name, ret, slicenum) \
	do { \
		if (CTRL_REG_NUM_SLICES(regtype, name) > (slicenum - 1)) { \
			ret |= ((_SI_REG_READ_SLICE_FIELD(si_fn, sih, regtype, name, \
				slicenum) & CTRL_FIELD_MASK(regtype, name, slicenum)) << \
				 CTRL_FIELD_SHIFT(regtype, name, slicenum)); \
		} \
	} while (0)

#define _SI_REG_READ_SLICE64(si_fn, sih, regtype, name, ret, slicenum) \
	do { \
		if (CTRL_REG_NUM_SLICES(regtype, name) > (slicenum - 1)) { \
			ret |= (((uint64) (_SI_REG_READ_SLICE_FIELD(si_fn, sih, regtype, name, \
				slicenum) & CTRL_FIELD_MASK(regtype, name, slicenum))) << \
				 CTRL_FIELD_SHIFT(regtype, name, slicenum)); \
		} \
	} while (0)

#define _SI_CHECK_MIN_SLICES(regtype, name) \
	do { \
		if ((int)CTRL_REG_NUM_SLICES(regtype, name) < 0) { \
			SI_CTRLREGS_INVALID; \
		} \
	} while (0)

#define _SI_CHECK_MAX_SLICES(regtype, name, slicenum) \
	do { \
		if (CTRL_REG_NUM_SLICES(regtype, name) > (slicenum - 1)) { \
			SI_CTRLREGS_INVALID; \
		} \
	} while (0)

#define _SI_CTRL_REG_WRITE(si_fn, regtype, sih, name, value) \
	do { \
		_SI_CHECK_MIN_SLICES(regtype, name); \
		_SI_REG_WRITE_SLICE(si_fn, sih, regtype, name, value, 1); \
		_SI_REG_WRITE_SLICE(si_fn, sih, regtype, name, value, 2); \
		_SI_REG_WRITE_SLICE(si_fn, sih, regtype, name, value, 3); \
		_SI_REG_WRITE_SLICE(si_fn, sih, regtype, name, value, 4); \
		_SI_REG_WRITE_SLICE(si_fn, sih, regtype, name, value, 5); \
		_SI_REG_WRITE_SLICE(si_fn, sih, regtype, name, value, 6); \
		_SI_REG_WRITE_SLICE(si_fn, sih, regtype, name, value, 7); \
		_SI_REG_WRITE_SLICE(si_fn, sih, regtype, name, value, 8); \
		_SI_REG_WRITE_SLICE(si_fn, sih, regtype, name, value, 9); \
		_SI_REG_WRITE_SLICE(si_fn, sih, regtype, name, value, 10); \
		_SI_CHECK_MAX_SLICES(regtype, name, 11); \
	} while (0)

#define _SI_CTRL_REG_READ(si_fn, regtype, sih, name) \
	({ \
		uint32 __retval__ = 0; \
		_SI_CHECK_MIN_SLICES(regtype, name); \
		_SI_REG_READ_SLICE(si_fn, sih, regtype, name, __retval__, 1); \
		_SI_REG_READ_SLICE(si_fn, sih, regtype, name, __retval__, 2); \
		_SI_REG_READ_SLICE(si_fn, sih, regtype, name, __retval__, 3); \
		_SI_REG_READ_SLICE(si_fn, sih, regtype, name, __retval__, 4); \
		_SI_REG_READ_SLICE(si_fn, sih, regtype, name, __retval__, 5); \
		_SI_REG_READ_SLICE(si_fn, sih, regtype, name, __retval__, 6); \
		_SI_REG_READ_SLICE(si_fn, sih, regtype, name, __retval__, 7); \
		_SI_REG_READ_SLICE(si_fn, sih, regtype, name, __retval__, 8); \
		_SI_REG_READ_SLICE(si_fn, sih, regtype, name, __retval__, 9); \
		_SI_REG_READ_SLICE(si_fn, sih, regtype, name, __retval__, 10); \
		_SI_CHECK_MAX_SLICES(regtype, name, 11); \
		__retval__; \
	})

#define _SI_CTRL_REG_READ64(si_fn, regtype, sih, name) \
	({ \
		uint64 __retval__ = 0; \
		_SI_CHECK_MIN_SLICES(regtype, name); \
		_SI_REG_READ_SLICE64(si_fn, sih, regtype, name, __retval__, 1); \
		_SI_REG_READ_SLICE64(si_fn, sih, regtype, name, __retval__, 2); \
		_SI_REG_READ_SLICE64(si_fn, sih, regtype, name, __retval__, 3); \
		_SI_REG_READ_SLICE64(si_fn, sih, regtype, name, __retval__, 4); \
		_SI_REG_READ_SLICE64(si_fn, sih, regtype, name, __retval__, 5); \
		_SI_REG_READ_SLICE64(si_fn, sih, regtype, name, __retval__, 6); \
		_SI_REG_READ_SLICE64(si_fn, sih, regtype, name, __retval__, 7); \
		_SI_REG_READ_SLICE64(si_fn, sih, regtype, name, __retval__, 8); \
		_SI_REG_READ_SLICE64(si_fn, sih, regtype, name, __retval__, 9); \
		_SI_REG_READ_SLICE64(si_fn, sih, regtype, name, __retval__, 10); \
		_SI_CHECK_MAX_SLICES(regtype, name, 11); \
		__retval__; \
	})

#define _SI_CTRL_REG_MOD(si_fn, regtype, sih, name, mask, value) \
	({ \
		_SI_CHECK_MIN_SLICES(regtype, name); \
		_SI_REG_MOD_SLICE(si_fn, sih, regtype, name, mask, value, 1); \
		_SI_REG_MOD_SLICE(si_fn, sih, regtype, name, mask, value, 2); \
		_SI_REG_MOD_SLICE(si_fn, sih, regtype, name, mask, value, 3); \
		_SI_REG_MOD_SLICE(si_fn, sih, regtype, name, mask, value, 4); \
		_SI_REG_MOD_SLICE(si_fn, sih, regtype, name, mask, value, 5); \
		_SI_REG_MOD_SLICE(si_fn, sih, regtype, name, mask, value, 6); \
		_SI_REG_MOD_SLICE(si_fn, sih, regtype, name, mask, value, 7); \
		_SI_REG_MOD_SLICE(si_fn, sih, regtype, name, mask, value, 8); \
		_SI_REG_MOD_SLICE(si_fn, sih, regtype, name, mask, value, 9); \
		_SI_REG_MOD_SLICE(si_fn, sih, regtype, name, mask, value, 10); \
		_SI_CHECK_MAX_SLICES(regtype, name, 11); \
	})

#define SI_PMU_CC_WRITE(sih, name, value) \
	_SI_CTRL_REG_WRITE(si_pmu_chipcontrol, pmu_chip_cntrl, sih, name, value)
#define SI_PMU_CC_READ(sih, name) \
	_SI_CTRL_REG_READ(si_pmu_chipcontrol, pmu_chip_cntrl, sih, name)

#define SI_PMU_PLL_WRITE(sih, name, value) \
	_SI_CTRL_REG_WRITE(si_pmu_pllcontrol, pmu_pll_cntrl, sih, name, value)
#define SI_PMU_PLL_READ(sih, name) \
	_SI_CTRL_REG_READ(si_pmu_pllcontrol, pmu_pll_cntrl, sih, name)

#define SI_GCI_CC_WRITE(sih, name, value) \
	_SI_CTRL_REG_WRITE(si_gci_chipcontrol, gci_chip_cntrl, sih, name, value)
#define SI_GCI_CC_READ(sih, name) \
	_SI_CTRL_REG_READ(si_gci_chipcontrol, gci_chip_cntrl, sih, name)
#define SI_GCI_CC_READ64(sih, name) \
	_SI_CTRL_REG_READ64(si_gci_chipcontrol, gci_chip_cntrl, sih, name)
#define SI_GCI_CC_MOD(sih, name, mask, value) \
	_SI_CTRL_REG_MOD(si_gci_chipcontrol, gci_chip_cntrl, sih, name, mask, value)

#define SI_GCI_CS_READ(sih, name) \
	_SI_CTRL_REG_READ(_si_gci_chipstatus_rd, gci_chip_status, sih, name)

#define SI_VREG_WRITE(sih, name, value) \
	_SI_CTRL_REG_WRITE(si_pmu_vreg_control, pmu_vreg_cntrl, sih, name, value)
#define SI_VREG_READ(sih, name) \
	_SI_CTRL_REG_READ(si_pmu_vreg_control, pmu_vreg_cntrl, sih, name)

#else /* VLSI_CTRL_REGS */

#define VLSI2SW_CTRLREGS()	FALSE

#ifdef BCMCHIPID
/* This is aplicable only for chips which do not support vlsi2sw flow:
 * BCM4387, BCM4389. This code needs to be deleted once
 * the trunk support for these chips gets deprecated. The below macro
 * provides a switch between vlsi2sw flow v/s legacy flow
 */
#define SI_GCI_CC_WRITE(sih, field, value) si_gci_chipcontrol_wr_api(sih, field, value)
#define SI_GCI_CC_READ(sih, field) si_gci_chipcontrol_rd_api(sih, field)
#else
#define SI_GCI_CC_WRITE(sih, name, value)	{ \
		BCM_REFERENCE(value); \
		(void)SI_CTRLREGS_INVALID; \
	}
#define SI_GCI_CC_MOD(sih, name, mask, value)	{ \
		BCM_REFERENCE(mask); \
		BCM_REFERENCE(value); \
		(void)SI_CTRLREGS_INVALID; \
	}
#define SI_GCI_CC_READ(sih, name)		SI_CTRLREGS_INVALID
#endif /* BCMCHIPID */

/* Use extern only for C file compile.
 * extern definitions doesn't work for assembly file compilation.
 */
#ifndef _LANGUAGE_ASSEMBLY
extern uint16 hnd_invalid_ctrlreg(void); // Note: this function must not be defined anywhere
#endif /* _LANGUAGE_ASSEMBLY */
#define SI_PMU_CC_WRITE(sih, name, value)	{ \
		BCM_REFERENCE(value); \
		(void)SI_CTRLREGS_INVALID; \
	}
#define SI_PMU_CC_READ(sih, name)		SI_CTRLREGS_INVALID
#define SI_PMU_PLL_WRITE(sih, name, value)	{ \
		BCM_REFERENCE(value); \
		(void)SI_CTRLREGS_INVALID; \
	}
#define SI_PMU_PLL_READ(sih, name)		SI_CTRLREGS_INVALID
#define SI_GCI_CC_READ64(sih, name)		SI_CTRLREGS_INVALID
#define SI_GCI_CS_READ(sih, name)               SI_CTRLREGS_INVALID
#define SI_VREG_WRITE(sih, name, value)		{ \
		BCM_REFERENCE(value); \
		(void)SI_CTRLREGS_INVALID; \
	}
#define SI_VREG_READ(sih, name)			SI_CTRLREGS_INVALID
#endif /* VLSI_CTRL_REGS */

#endif	/* _SBCHIPC_H */
