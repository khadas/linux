/*
 * BCM43XX PCIE core hardware definitions.
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
#ifndef	_PCIE_CORE_H
#define	_PCIE_CORE_H

#include <sbhnddma.h>
#include <siutils.h>

#define REV_GE_135(rev) (PCIECOREREV((rev)) >= 135u)
#define REV_GE_74(rev) (PCIECOREREV((rev)) >= 74)
#define REV_GE_73(rev) (PCIECOREREV((rev)) >= 73)
#define REV_GE_71(rev) (PCIECOREREV((rev)) >= 71)
#define REV_GE_69(rev) (PCIECOREREV((rev)) >= 69)
#define REV_GE_68(rev) (PCIECOREREV((rev)) >= 68)
#define REV_GE_64(rev) (PCIECOREREV((rev)) >= 64)

/* different register spaces to access thr'u pcie indirect access */
#define PCIE_CONFIGREGS		1		/* Access to config space */
#define PCIE_PCIEREGS		2		/* Access to pcie registers */

#define PCIEDEV_HOSTADDR_MAP_BASE     0x8000000
#define PCIEDEV_HOSTADDR_MAP_WIN_MASK 0xFE000000

#define PCIEDEV_ARM_ADDR_SPACE 0x0FFFFFFF

/* PCIe translation windoes */
#define PCIEDEV_TRANS_WIN_0 0
#define PCIEDEV_TRANS_WIN_1 1
#define PCIEDEV_TRANS_WIN_2 2
#define PCIEDEV_TRANS_WIN_3 3

#define PCIEDEV_ARM_ADDR(host_addr, win) \
	(((host_addr) & 0x1FFFFFF) | ((win) << 25) | PCIEDEV_HOSTADDR_MAP_BASE)

/* Current mapping of PCIe translation windows to SW features */

#define PCIEDEV_TRANS_WIN_TRAP_HANDLER	PCIEDEV_TRANS_WIN_0
#define PCIEDEV_TRANS_WIN_HOSTMEM	PCIEDEV_TRANS_WIN_1
#define PCIEDEV_TRANS_WIN_SWPAGING	PCIEDEV_TRANS_WIN_1
#define PCIEDEV_TRANS_WIN_BT		PCIEDEV_TRANS_WIN_2
#define PCIEDEV_TRANS_WIN_FWTRACE	PCIEDEV_TRANS_WIN_3

/* dma regs to control the flow between host2dev and dev2host  */
typedef volatile struct pcie_devdmaregs {
	dma64regs_t	tx;
	uint32		PAD[2];
	dma64regs_t	rx;
	uint32		PAD[2];
} pcie_devdmaregs_t;

#define PCIE_DB_HOST2DEV_0		0x1
#define PCIE_DB_HOST2DEV_1		0x2
#define PCIE_DB_DEV2HOST_0		0x3
#define PCIE_DB_DEV2HOST_1		0x4
#define PCIE_DB_DEV2HOST1_0		0x5

/* Flow Ring Manager */
#define IFRM_FR_IDX_MAX		256
#define IFRM_FR_GID_MAX		4
#define IFRM_FR_TID_MAX		8

#define IFRM_VEC_REG_BITS	32

#define IFRM_FR_PER_VECREG		4
#define IFRM_FR_PER_VECREG_SHIFT	2
#define IFRM_FR_PER_VECREG_MASK		((0x1 << IFRM_FR_PER_VECREG_SHIFT) - 1)

#define IFRM_VEC_BITS_PER_FR	(IFRM_VEC_REG_BITS/IFRM_FR_PER_VECREG)

/* IFRM_DEV_0 : d11AC, IFRM_DEV_1 : d11AD */
#define IFRM_DEV_0	0
#define IFRM_DEV_1	1

#define IFRM_FR_GID_0 0

#define IFRM_TIDMASK 0xffffffff

/* ifrm_ctrlst register */
#define IFRM_EN (1<<0)
#define IFRM_BUFF_INIT_DONE (1<<1)
#define IFRM_COMPARE_EN0 (1<<4)
#define IFRM_COMPARE_EN1 (1<<5)
#define IFRM_COMPARE_EN2 (1<<6)
#define IFRM_COMPARE_EN3 (1<<7)

/* ifrm_msk_arr.addr, ifrm_tid_arr.addr register */
#define IFRM_ADDR_SHIFT 0
#define IFRM_FRG_ID_SHIFT 8

/* ifrm_vec.diff_lat register */
#define IFRM_DV_LAT		(1<<0)
#define IFRM_DV_LAT_DONE	(1<<1)
#define IFRM_SDV_OFFSET_SHIFT	4
#define IFRM_SDV_FRGID_SHIFT	8
#define IFRM_VECSTAT_MASK	0x3
#define IFRM_VEC_MASK		0xff

/* HMAP Windows */
#define HMAP_MAX_WINDOWS	8

/* HMAP window register set */
typedef volatile struct pcie_hmapwindow {
	uint32 baseaddr_lo; /* BaseAddrLower */
	uint32 baseaddr_hi; /* BaseAddrUpper */
	uint32 windowlength; /* Window Length */
	uint32	PAD[1];
} pcie_hmapwindow_t;

typedef struct pcie_hmapviolation {
	uint32 hmap_violationaddr_lo;	/* violating address lo */
	uint32 hmap_violationaddr_hi;	/* violating addr hi */
	uint32 hmap_violation_info;	/* violation info */
	uint32	PAD[1];
} pcie_hmapviolation_t;

#if !defined(DONGLEBUILD) || defined(BCMSTANDALONE_TEST) || defined(ATE_BUILD) || \
	defined(BCMDVFS)
/* SB side: PCIE core and host control registers */
typedef volatile struct sbpcieregs sbpcieregs_t;
#endif /* !defined(DONGLEBUILD) || defined(BCMSTANDALONE_TEST) || */
	/* defined(ATE_BUILD) defined(BCMDVFS) */

/* serdes address space */
typedef volatile struct pcie_serdes_regs {
	uint16		PAD[0x600];
	uint16		phy_pipe_cmn_ctrl_1;		/* 0xc00 */
	uint16		PAD[0xf];
	uint16		phy_refclk_det_thres_low;	/* 0xc20 */
	uint16		phy_refclk_det_thres_high;	/* 0xc22 */
	uint16		phy_refclk_det_interval;	/* 0xc24 */
	uint16		phy_refclk_det_op_delay;	/* 0xc26 */
} pcie_serdes_regs_t;

/* phy_pipe_cmn_ctrl_1 fields */
#define PCIE_PCS_COMMA_REALIGN_MASK	0x0400u
#define PCIE_PCS_COMMA_REALIGN_SHIFT	10u

/* RefClkSenseControl, Tolerance1MHz values */
#define PCIE_REFCLKSENSE_OPTION_1	(0x1u) /* CDNS Detect @1Mhz */
#define PCIE_REFCLKSENSE_OPTION_2	(0x2u) /* CDNS Detect @Xtal */
#define PCIE_REFCLKSENSE_OPTION_3	(0x3u) /* CDNS Detect @1MHz and BRCM Refclk Sense */

/* 10th and 11th 4KB BAR0 windows */
#define PCIE_TER_BAR0_WIN	0xc50
#define PCIE_TER_BAR0_WRAPPER	0xc54

#define PCIE_TER_BAR0_WIN_DAR	0xa78
#define PCIE_TER_BAR0_WRAPPER_DAR	0xa7c

#define PCIE_TER_BAR0_WIN_REG(rev) \
		REV_GE_74(rev) ? PCIE_TER_BAR0_WIN_DAR : PCIE_TER_BAR0_WIN
#define PCIE_TER_BAR0_WRAPPER_REG(rev) \
		REV_GE_74(rev) ? PCIE_TER_BAR0_WRAPPER_DAR : PCIE_TER_BAR0_WRAPPER

/* PCI control */
#define PCIE_RST_OE		0x01	/* When set, drives PCI_RESET out to pin */
#define PCIE_RST		0x02	/* Value driven out to pin */
#define PCIE_SPERST		0x04	/* SurvivePeRst */
#define PCIE_FORCECFGCLKON_ALP	0x08
#define PCIE_DISABLE_L1CLK_GATING 0x10
#define PCIE_DLYPERST		0x100	/* Delay PeRst to CoE Core */
#define PCIE_DISSPROMLD		0x200	/* DisableSpromLoadOnPerst */
#define PCIE_WakeModeL2		0x1000	/* Wake on L2 */
#define PCIE_MULTIMSI_EN	0x2000	/* enable multi-vector MSI messages */
#define PCIE_PipeIddqDisable0	0x8000	/* Disable assertion of pcie_pipe_iddq during L1.2 and L2 */
#define PCIE_PipeIddqDisable1	0x10000	/* Disable assertion of pcie_pipe_iddq during L2 */
#define PCIE_EN_MDIO_IN_PERST	0x20000 /* enable access to internal registers when PERST */
#define PCIE_HWDisableL1EntryEnable 0x40000 /* set, Hw requests can do entry/exit from L1 ASPM */
#define PCIE_MSI_B2B_EN		0x100000 /* enable back-to-back MSI messages */
#define PCIE_MSI_FIFO_CLEAR	0x200000 /* reset MSI FIFO */
#define PCIE_IDMA_MODE_EN(rev)	(REV_GE_64(rev) ? 0x1 : 0x800000) /* implicit M2M DMA mode */
#define PCIE_TL_CLK_DETCT	0x4000000 /* enable TL clk detection */
#define PCIE_REQ_PEND_DIS_L1	0x1000000 /* prevents entering L1 on pending requests from host */
#define PCIE_DIS_L23CLK_GATE	0x10000000 /* disable clk gating in L23(pcie_tl_clk) */

/* Function control (corerev > 64) */
#define PCIE_CPLCA_ENABLE	0x01
/* 1: send CPL with CA on BP error, 0: send CPLD with SC and data is FFFF */
#define PCIE_DLY_PERST_TO_COE	0x02
/* when set, PERST is holding asserted until sprom-related register updates has completed */

#define PCIE_SWPME_FN0		0x10000
#define PCIE_SWPME_FN0_SHF	16

/* Interrupt status/mask */
#define PCIE_INTA	0x01	/* PCIE INTA message is received */
#define PCIE_INTB	0x02	/* PCIE INTB message is received */
#define PCIE_INTFATAL	0x04	/* PCIE INTFATAL message is received */
#define PCIE_INTNFATAL	0x08	/* PCIE INTNONFATAL message is received */
#define PCIE_INTCORR	0x10	/* PCIE INTCORR message is received */
#define PCIE_INTPME	0x20	/* PCIE INTPME message is received */
#define PCIE_PERST	0x40	/* PCIE Reset Interrupt */

#define PCIE_INT_MB_FN0_0 0x0100 /* PCIE to SB Mailbox int Fn0.0 is received */
#define PCIE_INT_MB_FN0_1 0x0200 /* PCIE to SB Mailbox int Fn0.1 is received */
#define PCIE_INT_MB_FN1_0 0x0400 /* PCIE to SB Mailbox int Fn1.0 is received */
#define PCIE_INT_MB_FN1_1 0x0800 /* PCIE to SB Mailbox int Fn1.1 is received */
#define PCIE_INT_MB_FN2_0 0x1000 /* PCIE to SB Mailbox int Fn2.0 is received */
#define PCIE_INT_MB_FN2_1 0x2000 /* PCIE to SB Mailbox int Fn2.1 is received */
#define PCIE_INT_MB_FN3_0 0x4000 /* PCIE to SB Mailbox int Fn3.0 is received */
#define PCIE_INT_MB_FN3_1 0x8000 /* PCIE to SB Mailbox int Fn3.1 is received */

/* PCIE MSI Vector Assignment register */
#define MSIVEC_MB_0	(0x1 << 1) /* MSI Vector offset for mailbox0 is 2 */
#define MSIVEC_MB_1	(0x1 << 2) /* MSI Vector offset for mailbox1 is 3 */
#define MSIVEC_D2H0_DB0	(0x1 << 3) /* MSI Vector offset for interface0 door bell 0 is 4 */
#define MSIVEC_D2H0_DB1	(0x1 << 4) /* MSI Vector offset for interface0 door bell 1 is 5 */

/* PCIE MailboxInt/MailboxIntMask register */
#define PCIE_MB_TOSB_FN0_0	0x0001 /* write to assert PCIEtoSB Mailbox interrupt */
#define PCIE_MB_TOSB_FN0_1	0x0002
#define PCIE_MB_TOSB_FN1_0	0x0004
#define PCIE_MB_TOSB_FN1_1	0x0008
#define PCIE_MB_TOSB_FN2_0	0x0010
#define PCIE_MB_TOSB_FN2_1	0x0020
#define PCIE_MB_TOSB_FN3_0	0x0040
#define PCIE_MB_TOSB_FN3_1	0x0080
#define PCIE_MB_TOPCIE_FN0_0	0x0100 /* int status/mask for SBtoPCIE Mailbox interrupts */
#define PCIE_MB_TOPCIE_FN0_1	0x0200
#define PCIE_MB_TOPCIE_FN1_0	0x0400
#define PCIE_MB_TOPCIE_FN1_1	0x0800
#define PCIE_MB_TOPCIE_FN2_0	0x1000
#define PCIE_MB_TOPCIE_FN2_1	0x2000
#define PCIE_MB_TOPCIE_FN3_0	0x4000
#define PCIE_MB_TOPCIE_FN3_1	0x8000

#define PCIE_MB_TOPCIE_DB0_D2H0(rev)	(REV_GE_64(rev) ? 0x0001 : 0x010000)
#define PCIE_MB_TOPCIE_DB0_D2H1(rev)	(REV_GE_64(rev) ? 0x0002 : 0x020000)
#define PCIE_MB_TOPCIE_DB1_D2H0(rev)	(REV_GE_64(rev) ? 0x0004 : 0x040000)
#define PCIE_MB_TOPCIE_DB1_D2H1(rev)	(REV_GE_64(rev) ? 0x0008 : 0x080000)
#define PCIE_MB_TOPCIE_DB2_D2H0(rev)	(REV_GE_64(rev) ? 0x0010 : 0x100000)
#define PCIE_MB_TOPCIE_DB2_D2H1(rev)	(REV_GE_64(rev) ? 0x0020 : 0x200000)
#define PCIE_MB_TOPCIE_DB3_D2H0(rev)	(REV_GE_64(rev) ? 0x0040 : 0x400000)
#define PCIE_MB_TOPCIE_DB3_D2H1(rev)	(REV_GE_64(rev) ? 0x0080 : 0x800000)
#define PCIE_MB_TOPCIE_DB4_D2H0(rev)	(REV_GE_64(rev) ? 0x0100 : 0x0)
#define PCIE_MB_TOPCIE_DB4_D2H1(rev)	(REV_GE_64(rev) ? 0x0200 : 0x0)
#define PCIE_MB_TOPCIE_DB5_D2H0(rev)	(REV_GE_64(rev) ? 0x0400 : 0x0)
#define PCIE_MB_TOPCIE_DB5_D2H1(rev)	(REV_GE_64(rev) ? 0x0800 : 0x0)
#define PCIE_MB_TOPCIE_DB6_D2H0(rev)	(REV_GE_64(rev) ? 0x1000 : 0x0)
#define PCIE_MB_TOPCIE_DB6_D2H1(rev)	(REV_GE_64(rev) ? 0x2000 : 0x0)
#define PCIE_MB_TOPCIE_DB7_D2H0(rev)	(REV_GE_64(rev) ? 0x4000 : 0x0)
#define PCIE_MB_TOPCIE_DB7_D2H1(rev)	(REV_GE_64(rev) ? 0x8000 : 0x0)

#define PCIE_MB_D2H_MB_MASK(rev)		\
	(PCIE_MB_TOPCIE_DB0_D2H0(rev) | PCIE_MB_TOPCIE_DB0_D2H1(rev) |	\
	PCIE_MB_TOPCIE_DB1_D2H0(rev)  | PCIE_MB_TOPCIE_DB1_D2H1(rev) |	\
	PCIE_MB_TOPCIE_DB2_D2H0(rev)  | PCIE_MB_TOPCIE_DB2_D2H1(rev) |	\
	PCIE_MB_TOPCIE_DB3_D2H0(rev)  | PCIE_MB_TOPCIE_DB3_D2H1(rev) |	\
	PCIE_MB_TOPCIE_DB4_D2H0(rev)  | PCIE_MB_TOPCIE_DB4_D2H1(rev) |	\
	PCIE_MB_TOPCIE_DB5_D2H0(rev)  | PCIE_MB_TOPCIE_DB5_D2H1(rev) |	\
	PCIE_MB_TOPCIE_DB6_D2H0(rev)  | PCIE_MB_TOPCIE_DB6_D2H1(rev) |	\
	PCIE_MB_TOPCIE_DB7_D2H0(rev)  | PCIE_MB_TOPCIE_DB7_D2H1(rev))

#define SBTOPCIE0_BASE 0x08000000
#define SBTOPCIE1_BASE 0x0c000000

/* Protection Control register */
#define	PROTECT_CFG			(1 << 0)
#define	PROTECT_DMABADDR		(1 << 1)

#define	PROTECT_FN_CFG_WRITE		(1 << 0)
#define	PROTECT_FN_CFG_READ		(1 << 1)
#define	PROTECT_FN_ENUM_WRITE		(1 << 2)
#define	PROTECT_FN_ENUM_READ		(1 << 3)
#define	PROTECT_FN_DMABADDR		(1 << 4)

/* On chips with CCI-400, the small pcie 128 MB region base has shifted */
#define CCI400_SBTOPCIE0_BASE  0x20000000
#define CCI400_SBTOPCIE1_BASE  0x24000000

/* SB to PCIE translation masks */
#define SBTOPCIE0_MASK	0xfc000000
#define SBTOPCIE1_MASK	0xfc000000
#define SBTOPCIE2_MASK	0xc0000000

/* Access type bits (0:1) */
#define SBTOPCIE_MEM	0
#define SBTOPCIE_IO	1
#define SBTOPCIE_CFG0	2
#define SBTOPCIE_CFG1	3

/* Prefetch enable bit 2 */
#define SBTOPCIE_PF		4

/* Write Burst enable for memory write bit 3 */
#define SBTOPCIE_WR_BURST	8

/* config access */
#define CONFIGADDR_FUNC_MASK	0x7000
#define CONFIGADDR_FUNC_SHF	12
#define CONFIGADDR_REG_MASK	0x0FFF
#define CONFIGADDR_REG_SHF	0

#define PCIE_CONFIG_INDADDR(f, r)	((((f) & CONFIGADDR_FUNC_MASK) << CONFIGADDR_FUNC_SHF) | \
	(((r) & CONFIGADDR_REG_MASK) << CONFIGADDR_REG_SHF))

/* PCIE Config registers */
#define	PCIE_CFG_DEV_STS_CTRL_2		0x0d4u	/* "dev_sts_control_2  */
#define	PCIE_CFG_ADV_ERR_CAP		0x100u	/* adv_err_cap         */
#define	PCIE_CFG_UC_ERR_STS		0x104u	/* uc_err_status       */
#define	PCIE_CFG_UC_ERR_MASK		0x108u	/* ucorr_err_mask      */
#define	PCIE_CFG_UNCOR_ERR_SERV		0x10cu	/* ucorr_err_sevr      */
#define	PCIE_CFG_CORR_ERR_STS		0x110u	/* corr_err_status     */
#define	PCIE_CFG_CORR_ERR_MASK		0x114u	/* corr_err_mask       */
#define	PCIE_CFG_ADV_ERR_CTRL		0x118u	/* adv_err_cap_control */
#define	PCIE_CFG_HDR_LOG1		0x11Cu	/* header_log1         */
#define	PCIE_CFG_HDR_LOG2		0x120u	/* header_log2         */
#define	PCIE_CFG_HDR_LOG3		0x124u	/* header_log3         */
#define	PCIE_CFG_HDR_LOG4		0x128u	/* header_log4         */
#define	PCIE_CFG_PML1_SUB_CAP_ID	0x240u	/* PML1sub_capID       */
#define	PCIE_CFG_PML1_SUB_CAP_REG	0x244u	/* PML1_sub_Cap_reg    */
#define	PCIE_CFG_PML1_SUB_CTRL1		0x248u	/* PML1_sub_control1   */
#define	PCIE_CFG_PML1_SUB_CTRL3		0x24Cu	/* PML1_sub_control2   */
#define	PCIE_CFG_TL_CTRL_5		0x814u	/* tl_control_5        */
#define	PCIE_CFG_PHY_ERR_ATT_VEC	0x1820u	/* phy_err_attn_vec    */
#define	PCIE_CFG_PHY_ERR_ATT_MASK	0x1824u	/* phy_err_attn_mask   */

/* PCIE protocol DLLP diagnostic registers */
#define PCIE_DLLP_LCREG			0x100u /* Link Control */
#define PCIE_DLLP_LSREG			0x104u /* Link Status */
#define PCIE_DLLP_LAREG			0x108u /* Link Attention */
#define PCIE_DLLP_LAMASKREG		0x10Cu /* Link Attention Mask */
#define PCIE_DLLP_NEXTTXSEQNUMREG	0x110u /* Next Tx Seq Num */
#define PCIE_DLLP_ACKEDTXSEQNUMREG	0x114u /* Acked Tx Seq Num */
#define PCIE_DLLP_PURGEDTXSEQNUMREG	0x118u /* Purged Tx Seq Num */
#define PCIE_DLLP_RXSEQNUMREG		0x11Cu /* Rx Sequence Number */
#define PCIE_DLLP_LRREG			0x120u /* Link Replay */
#define PCIE_DLLP_LACKTOREG		0x124u /* Link Ack Timeout */
#define PCIE_DLLP_PMTHRESHREG		0x128u /* Power Management Threshold */
#define PCIE_DLLP_RTRYWPREG		0x12Cu /* Retry buffer write ptr */
#define PCIE_DLLP_RTRYRPREG		0x130u /* Retry buffer Read ptr */
#define PCIE_DLLP_RTRYPPREG		0x134u /* Retry buffer Purged ptr */
#define PCIE_DLLP_RTRRWREG		0x138u /* Retry buffer Read/Write */
#define PCIE_DLLP_ECTHRESHREG		0x13Cu /* Error Count Threshold */
#define PCIE_DLLP_TLPERRCTRREG		0x140u /* TLP Error Counter */
#define PCIE_DLLP_ERRCTRREG		0x144u /* Error Counter */
#define PCIE_DLLP_NAKRXCTRREG		0x148u /* NAK Received Counter */
#define PCIE_DLLP_TESTREG		0x14Cu /* Test */
#define PCIE_DLLP_PKTBIST		0x150u /* Packet BIST */
#define PCIE_DLLP_PCIE11		0x154u /* DLLP PCIE 1.1 reg */

#define PCIE_DLLP_LSREG_LINKUP		(1u << 16u)

/* MDIO control */
#define MDIOCTL_DIVISOR_MASK		0x7fu	/* clock to be used on MDIO */
#define MDIOCTL_DIVISOR_VAL		0x2u
#define MDIOCTL_PREAM_EN		0x80u	/* Enable preamble sequnce */
#define MDIOCTL_ACCESS_DONE		0x100u   /* Tranaction complete */

/* MDIO Data */
#define MDIODATA_MASK			0x0000ffff	/* data 2 bytes */
#define MDIODATA_TA			0x00020000	/* Turnaround */
#define MDIODATA_REGADDR_SHF_OLD	18		/* Regaddr shift (rev < 10) */
#define MDIODATA_REGADDR_MASK_OLD	0x003c0000	/* Regaddr Mask (rev < 10) */
#define MDIODATA_DEVADDR_SHF_OLD	22		/* Physmedia devaddr shift (rev < 10) */
#define MDIODATA_DEVADDR_MASK_OLD	0x0fc00000	/* Physmedia devaddr Mask (rev < 10) */
#define MDIODATA_REGADDR_SHF		18		/* Regaddr shift */
#define MDIODATA_REGADDR_MASK		0x007c0000	/* Regaddr Mask */
#define MDIODATA_DEVADDR_SHF		23		/* Physmedia devaddr shift */
#define MDIODATA_DEVADDR_MASK		0x0f800000	/* Physmedia devaddr Mask */
#define MDIODATA_WRITE			0x10000000	/* write Transaction */
#define MDIODATA_READ			0x20000000	/* Read Transaction */
#define MDIODATA_START			0x40000000	/* start of Transaction */

#define MDIODATA_DEV_ADDR		0x0		/* dev address for serdes */
#define	MDIODATA_BLK_ADDR		0x1F		/* blk address for serdes */

/* MDIO control/wrData/rdData register defines for PCIE Gen 2 */
#define MDIOCTL2_DIVISOR_MASK		0x7f	/* clock to be used on MDIO */
#define MDIOCTL2_DIVISOR_VAL		0x2
#define MDIOCTL2_REGADDR_SHF		8		/* Regaddr shift */
#define MDIOCTL2_REGADDR_MASK		0x00FFFF00	/* Regaddr Mask */
#define MDIOCTL2_DEVADDR_SHF		24		/* Physmedia devaddr shift */
#define MDIOCTL2_DEVADDR_MASK		0x0f000000	/* Physmedia devaddr Mask */
#define MDIOCTL2_SLAVE_BYPASS		0x10000000	/* IP slave bypass */
#define MDIOCTL2_READ			0x20000000	/* IP slave bypass */

#define MDIODATA2_DONE			0x80000000u	/* rd/wr transaction done */
#define MDIODATA2_MASK			0x7FFFFFFF	/* rd/wr transaction data */
#define MDIODATA2_DEVADDR_SHF		4		/* Physmedia devaddr shift */

/* MDIO devices (SERDES modules)
 *  unlike old pcie cores (rev < 10), rev10 pcie serde organizes registers into a few blocks.
 *  two layers mapping (blockidx, register offset) is required
 */
#define MDIO_DEV_IEEE0		0x000
#define MDIO_DEV_IEEE1		0x001
#define MDIO_DEV_BLK0		0x800
#define MDIO_DEV_BLK1		0x801
#define MDIO_DEV_BLK2		0x802
#define MDIO_DEV_BLK3		0x803
#define MDIO_DEV_BLK4		0x804
#define MDIO_DEV_TXPLL		0x808	/* TXPLL register block idx */
#define MDIO_DEV_TXCTRL0	0x820
#define MDIO_DEV_SERDESID	0x831
#define MDIO_DEV_RXCTRL0	0x840

/* XgxsBlk1_A Register Offsets */
#define BLK1_PWR_MGMT0		0x16
#define BLK1_PWR_MGMT1		0x17
#define BLK1_PWR_MGMT2		0x18
#define BLK1_PWR_MGMT3		0x19
#define BLK1_PWR_MGMT4		0x1A

/* serdes regs (rev < 10) */
#define MDIODATA_DEV_PLL	0x1d	/* SERDES PLL Dev */
#define MDIODATA_DEV_TX		0x1e	/* SERDES TX Dev */
#define MDIODATA_DEV_RX		0x1f	/* SERDES RX Dev */
	/* SERDES RX registers */
#define SERDES_RX_CTRL			1	/* Rx cntrl */
#define SERDES_RX_TIMER1		2	/* Rx Timer1 */
#define SERDES_RX_CDR			6	/* CDR */
#define SERDES_RX_CDRBW			7	/* CDR BW */

	/* SERDES RX control register */
#define SERDES_RX_CTRL_FORCE		0x80	/* rxpolarity_force */
#define SERDES_RX_CTRL_POLARITY		0x40	/* rxpolarity_value */

	/* SERDES PLL registers */
#define SERDES_PLL_CTRL                 1       /* PLL control reg */
#define PLL_CTRL_FREQDET_EN             0x4000  /* bit 14 is FREQDET on */

/* Power management threshold */
#define PCIE_L0THRESHOLDTIME_MASK       0xFF00u	/* bits 0 - 7 */
#define PCIE_L1THRESHOLDTIME_MASK       0xFF00u	/* bits 8 - 15 */
#define PCIE_L1THRESHOLDTIME_SHIFT      8	/* PCIE_L1THRESHOLDTIME_SHIFT */
#define PCIE_L1THRESHOLD_WARVAL         0x72	/* WAR value */
#define PCIE_ASPMTIMER_EXTEND		0x01000000	/* > rev7: enable extend ASPM timer */

/* SPROM offsets */
#define SRSH_ASPM_OFFSET		4	/* word 4 */
#define SRSH_ASPM_ENB			0x18	/* bit 3, 4 */
#define SRSH_ASPM_L1_ENB		0x10	/* bit 4 */
#define SRSH_ASPM_L0s_ENB		0x8	/* bit 3 */
#define SRSH_PCIE_MISC_CONFIG		5	/* word 5 */
#define SRSH_L23READY_EXIT_NOPERST	0x8000u	/* bit 15 */
#define SRSH_CLKREQ_OFFSET_REV5		20	/* word 20 for srom rev <= 5 */
#define SRSH_CLKREQ_OFFSET_REV8		52	/* word 52 for srom rev 8 */
#define SRSH_CLKREQ_ENB			0x0800	/* bit 11 */
#define SRSH_BD_OFFSET                  6       /* word 6 */
#define SRSH_AUTOINIT_OFFSET            18      /* auto initialization enable */

/* PCI Capability ID's
 * Reference include/linux/pci_regs.h
 * #define  PCI_CAP_LIST_ID	0       // Capability ID
 * #define  PCI_CAP_ID_PM		0x01    // Power Management
 * #define  PCI_CAP_ID_AGP		0x02    // Accelerated Graphics Port
 * #define  PCI_CAP_ID_VPD		0x03    // Vital Product Data
 * #define  PCI_CAP_ID_SLOTID	0x04    // Slot Identification
 * #define  PCI_CAP_ID_MSI		0x05    // Message Signalled Interrupts
 * #define  PCI_CAP_ID_CHSWP       0x06    // CompactPCI HotSwap
 * #define  PCI_CAP_ID_PCIX        0x07    // PCI-X
 * #define  PCI_CAP_ID_HT          0x08    // HyperTransport
 * #define  PCI_CAP_ID_VNDR        0x09    // Vendor-Specific
 * #define  PCI_CAP_ID_DBG         0x0A    // Debug port
 * #define  PCI_CAP_ID_CCRC        0x0B    // CompactPCI Central Resource Control
 * #define  PCI_CAP_ID_SHPC        0x0C    // PCI Standard Hot-Plug Controller
 * #define  PCI_CAP_ID_SSVID       0x0D    // Bridge subsystem vendor/device ID
 * #define  PCI_CAP_ID_AGP3        0x0E    // AGP Target PCI-PCI bridge
 * #define  PCI_CAP_ID_SECDEV      0x0F    // Secure Device
 * #define  PCI_CAP_ID_MSIX        0x11    // MSI-X
 * #define  PCI_CAP_ID_SATA        0x12    // SATA Data/Index Conf.
 * #define  PCI_CAP_ID_AF          0x13    // PCI Advanced Features
 * #define  PCI_CAP_ID_EA          0x14    // PCI Enhanced Allocation
 * #define  PCI_CAP_ID_MAX         PCI_CAP_ID_EA
 */

#define  PCIE_CAP_ID_EXP         0x10    // PCI Express

/* PCIe Capabilities Offsets
 * Reference include/linux/pci_regs.h
 * #define PCIE_CAP_FLAGS           2       // Capabilities register
 * #define PCIE_CAP_DEVCAP          4       // Device capabilities
 * #define PCIE_CAP_DEVCTL          8       // Device Control
 * #define PCIE_CAP_DEVSTA          10      // Device Status
 * #define PCIE_CAP_LNKCAP          12      // Link Capabilities
 * #define PCIE_CAP_LNKCTL          16      // Link Control
 * #define PCIE_CAP_LNKSTA          18      // Link Status
 * #define PCI_CAP_EXP_ENDPOINT_SIZEOF_V1  20      // v1 endpoints end here
 * #define PCIE_CAP_SLTCAP          20      // Slot Capabilities
 * #define PCIE_CAP_SLTCTL          24      // Slot Control
 * #define PCIE_CAP_SLTSTA          26      // Slot Status
 * #define PCIE_CAP_RTCTL           28      // Root Control
 * #define PCIE_CAP_RTCAP           30      // Root Capabilities
 * #define PCIE_CAP_RTSTA           32      // Root Status
 */

/* Linkcapability reg offset in PCIE Cap */
#define PCIE_CAP_LINKCAP_OFFSET         12      /* linkcap offset in pcie cap */
#define PCIE_CAP_LINKCAP_LNKSPEED_MASK	0xf     /* Supported Link Speeds */
#define PCIE_CAP_LINKCAP_GEN2           0x2     /* Value for GEN2 */

/* Uc_Err reg offset in AER Cap */
#define PCIE_EXTCAP_ID_ERR		0x01	/* Advanced Error Reporting */
#define PCIE_EXTCAP_AER_UCERR_OFFSET	4	/* Uc_Err reg offset in AER Cap */
#define PCIE_EXTCAP_ERR_HEADER_LOG_0	28
#define PCIE_EXTCAP_ERR_HEADER_LOG_1	32
#define PCIE_EXTCAP_ERR_HEADER_LOG_2	36
#define PCIE_EXTCAP_ERR_HEADER_LOG_3	40

/* L1SS reg offset in L1SS Ext Cap */
#define PCIE_EXTCAP_ID_L1SS		0x1e	/* PCI Express L1 PM Substates Capability */
#define PCIE_EXTCAP_L1SS_CAP_OFFSET	4	/* L1SSCap reg offset in L1SS Cap */
#define PCIE_EXTCAP_L1SS_CONTROL_OFFSET	8	/* L1SSControl reg offset in L1SS Cap */
#define PCIE_EXTCAP_L1SS_CONTROL2_OFFSET	0xc	/* L1SSControl reg offset in L1SS Cap */

/* Linkcontrol reg offset in PCIE Cap */
#define PCIE_CAP_LINKCTRL_OFFSET	16	/* linkctrl offset in pcie cap */
#define PCIE_CAP_LCREG_ASPML0s		0x01	/* ASPM L0s in linkctrl */
#define PCIE_CAP_LCREG_ASPML1		0x02	/* ASPM L1 in linkctrl */
#define PCIE_CLKREQ_ENAB		0x100	/* CLKREQ Enab in linkctrl */
#define PCIE_LINKSPEED_MASK		0xF0000u	/* bits 0 - 3 of high word */
#define PCIE_LINKSPEED_SHIFT		16	/* PCIE_LINKSPEED_SHIFT */
#define PCIE_LINK_STS_LINKSPEED_5Gbps	(0x2 << PCIE_LINKSPEED_SHIFT)	/* PCIE_LINKSPEED 5Gbps */

/* Devcontrol reg offset in PCIE Cap */
#define PCIE_CAP_DEVCTRL_OFFSET		8	/* devctrl offset in pcie cap */
#define PCIE_CAP_DEVCTRL_MRRS_MASK	0x7000	/* Max read request size mask */
#define PCIE_CAP_DEVCTRL_MRRS_SHIFT	12	/* Max read request size shift */
#define PCIE_CAP_DEVCTRL_MRRS_128B	0	/* 128 Byte */
#define PCIE_CAP_DEVCTRL_MRRS_256B	1	/* 256 Byte */
#define PCIE_CAP_DEVCTRL_MRRS_512B	2	/* 512 Byte */
#define PCIE_CAP_DEVCTRL_MRRS_1024B	3	/* 1024 Byte */
#define PCIE_CAP_DEVCTRL_MPS_MASK	0x00e0	/* Max payload size mask */
#define PCIE_CAP_DEVCTRL_MPS_SHIFT	5	/* Max payload size shift */
#define PCIE_CAP_DEVCTRL_MPS_128B	0	/* 128 Byte */
#define PCIE_CAP_DEVCTRL_MPS_256B	1	/* 256 Byte */
#define PCIE_CAP_DEVCTRL_MPS_512B	2	/* 512 Byte */
#define PCIE_CAP_DEVCTRL_MPS_1024B	3	/* 1024 Byte */

#define PCIE_ASPM_CTRL_MASK		3	/* bit 0 and 1 */
#define PCIE_ASPM_ENAB			3	/* ASPM L0s & L1 in linkctrl */
#define PCIE_ASPM_L1_ENAB		2	/* ASPM L0s & L1 in linkctrl */
#define PCIE_ASPM_L0s_ENAB		1	/* ASPM L0s & L1 in linkctrl */
#define PCIE_ASPM_DISAB			0	/* ASPM L0s & L1 in linkctrl */

#define PCIE_ASPM_L11_ENAB		8	/* ASPM L1.1 in PML1_sub_control2 */
#define PCIE_ASPM_L12_ENAB		4	/* ASPM L1.2 in PML1_sub_control2 */

#define PCIE_EXT_L1SS_MASK		0xf	/* Bits [3:0] of L1SSControl 0x248 */
#define PCIE_EXT_L1SS_ENAB		0xf	/* Bits [3:0] of L1SSControl 0x248 */
#define PCIE_LTR_THRESHOLD_SCALE_SHIFT	29u     /* LTR_L1_2_THRESHOLD_SCALE in PML1_sub_control1 */
#define PCIE_LTR_THRESHOLD_SCALE_MASK	0xE0000000u
#define PCIE_LTR_THRESHOLD_VALUE_SHIFT	16u	/* LTR_L1_2_THRESHOLD_VALUE in PML1_sub_control1 */
#define PCIE_LTR_THRESHOLD_VALUE_MASK	0x3FF0000u

/* NumMsg and NumMsgEn in PCIE MSI Cap */
#define MSICAP_NUM_MSG_SHF		17U
#define MSICAP_NUM_MSG_MASK		(0x7 << MSICAP_NUM_MSG_SHF)
#define MSICAP_NUM_MSG_EN_SHF	20U
#define MSICAP_NUM_MSG_EN_MASK	(0x7 << MSICAP_NUM_MSG_EN_SHF)

#define	MSI_MAX_VECTORS	32U
#define MSI_VECTOR_OFFSET_MAX	(MSI_MAX_VECTORS - 1U)

/* Devcontrol2 reg offset in PCIE Cap */
#define PCIE_CAP_DEVCTRL2_OFFSET	0x28	/* devctrl2 offset in pcie cap */
#define PCIE_CAP_DEVCTRL2_LTR_ENAB_MASK	0x400	/* Latency Tolerance Reporting Enable */
#define PCIE_CAP_DEVCTRL2_OBFF_ENAB_SHIFT 13	/* Enable OBFF mechanism, select signaling method */
#define PCIE_CAP_DEVCTRL2_OBFF_ENAB_MASK 0x6000	/* Enable OBFF mechanism, select signaling method */

/* LTR registers in PCIE Cap */
#define PCIE_LTR0_REG_OFFSET		0x844u		/* ltr0_reg offset in pcie cap */
#define PCIE_LTR1_REG_OFFSET		0x848u		/* ltr1_reg offset in pcie cap */
#define PCIE_LTR2_REG_OFFSET		0x84cu		/* ltr2_reg offset in pcie cap */
#define PCIE_LTR0_REG_DEFAULT_60	0x883c883cu	/* active latency default to 60usec */
#define PCIE_LTR0_REG_DEFAULT_150	0x88968896u	/* active latency default to 150usec */
#define PCIE_LTR1_REG_DEFAULT		0x88648864u	/* idle latency default to 100usec */
#define PCIE_LTR2_REG_DEFAULT		0x90039003u	/* sleep latency default to 3msec */
#define PCIE_LTR_LAT_VALUE_MASK		0x3FFu		/* LTR Latency mask */
#define PCIE_LTR_LAT_SCALE_SHIFT	10u		/* LTR Scale shift */
#define PCIE_LTR_LAT_SCALE_MASK		0x1C00u		/* LTR Scale mask */
#define PCIE_LTR_SNOOP_REQ_SHIFT	15u		/* LTR SNOOP REQ shift */
#define PCIE_LTR_SNOOP_REQ_MASK		0x8000u		/* LTR SNOOP REQ mask */

/* PCIE BRCM Vendor CAP REVID reg  bits */
#define BRCMCAP_PCIEREV_CT_MASK			0xF00u
#define BRCMCAP_PCIEREV_CT_SHIFT		8u
#define BRCMCAP_PCIEREV_REVID_MASK		0xFFu
#define BRCMCAP_PCIEREV_REVID_SHIFT		0

#define PCIE_REVREG_CT_PCIE1		0
#define PCIE_REVREG_CT_PCIE2		1

/* PCIE GEN2 specific defines */
/* PCIE BRCM Vendor Cap offsets w.r.t to vendor cap ptr */
#define PCIE2R0_BRCMCAP_REVID_OFFSET		4
#define PCIE2R0_BRCMCAP_BAR0_WIN0_WRAP_OFFSET	8
#define PCIE2R0_BRCMCAP_BAR0_WIN2_OFFSET	12
#define PCIE2R0_BRCMCAP_BAR0_WIN2_WRAP_OFFSET	16
#define PCIE2R0_BRCMCAP_BAR0_WIN_OFFSET		20
#define PCIE2R0_BRCMCAP_BAR1_WIN_OFFSET		24
#define PCIE2R0_BRCMCAP_SPROM_CTRL_OFFSET	28
#define PCIE2R0_BRCMCAP_BAR2_WIN_OFFSET		32
#define PCIE2R0_BRCMCAP_INTSTATUS_OFFSET	36
#define PCIE2R0_BRCMCAP_INTMASK_OFFSET		40
#define PCIE2R0_BRCMCAP_PCIE2SB_MB_OFFSET	44
#define PCIE2R0_BRCMCAP_BPADDR_OFFSET		48
#define PCIE2R0_BRCMCAP_BPDATA_OFFSET		52
#define PCIE2R0_BRCMCAP_CLKCTLSTS_OFFSET	56

/*
 * definition of configuration space registers of PCIe gen2
 */
#define PCIECFGREG_STATUS_CMD		0x4u
#define PCIECFGREG_PM_CSR		0x4Cu
#define PCIECFGREG_MSI_CAP		0x58u
#define PCIECFGREG_MSI_ADDR_L		0x5Cu
#define PCIECFGREG_MSI_ADDR_H		0x60u
#define PCIECFGREG_MSI_DATA		0x64u
#define PCIECFGREG_SPROM_CTRL           0x88u
#define PCIECFGREG_DEV_STATUS_CTRL	0xB4u
#define PCIECFGREG_LINK_STATUS_CTRL	0xBCu
#define PCIECFGGEN_DEV_STATUS_CTRL2	0xD4u
#define PCIECFGREG_LINK_STATUS_CTRL2	0xDCu
#define PCIECFGREG_PTM_CAP		0x204u
#define PCIECFGREG_PTM_CTRL		0x208u
#define PCIECFGREG_RBAR_CTRL		0x228u
#define PCIECFGREG_PML1_SUB_CTRL1	0x248u
#define PCIECFGREG_PML1_SUB_CTRL2	0x24Cu
#define PCIECFGREG_LANE_ERR_STAT	0x308u
#define PCIECFGREG_REG_BAR2_CONFIG	0x4E0u
#define PCIECFGREG_REG_BAR3_CONFIG	0x4F4u
#define PCIECFGREG_EXT2_CAP_ADDR	0x530u
#define PCIECFGREG_PTM_CTL0		0xA24u
#define PCIECFGREG_PTM_PMSTR_HI		0xA28u
#define PCIECFGREG_PTM_PMSTR_LO		0xA2Cu
#define PCIECFGREG_PTM_LOCAL_HI		0xA30u
#define PCIECFGREG_PTM_LOCAL_LO		0xA34u
#define PCIECFGREG_PTM_RES_LOCAL_HI	0xA38u
#define PCIECFGREG_PTM_RES_LOCAL_LO	0xA3Cu
#define PCIECFGREG_PTM_PROP_DLY		0xA40u
#define PCIECFGREG_PDL_CTRL1		0x1004u
#define PCIECFGREG_PDL_CTRL5		0x1014u
#define PCIECFGREG_PDL_IDDQ		0x1814u
#define PCIECFGREG_REG_PHY_CTL7		0x181cu
#define PCIECFGREG_PHY_DBG_CLKREQ0	0x1E10u
#define PCIECFGREG_PHY_DBG_CLKREQ1	0x1E14u
#define PCIECFGREG_PHY_DBG_CLKREQ2	0x1E18u
#define PCIECFGREG_PHY_DBG_CLKREQ3	0x1E1Cu
#define PCIECFGREG_PHY_LTSSM_HIST_0	0x1CECu
#define PCIECFGREG_PHY_LTSSM_HIST_1	0x1CF0u
#define PCIECFGREG_PHY_LTSSM_HIST_2	0x1CF4u
#define PCIECFGREG_PHY_LTSSM_HIST_3	0x1CF8u
#define PCIECFGREG_TREFUP		0x1814u

#define PCIECFGREG_TREFUP_EXT		0x1818u
#define PCIECFGREG_TREFUP_EXT_REFCLK_SENSE_MASK		0x8000u
#define PCIECFGREG_TREFUP_EXT_REFCLK_SENSE_SHIFT	15u

/* L1SS registers */
#define PCIECFGREG_L1SS_EXT_CNT_CTRL		0xAE8u
#define PCIECFGREG_L1SS_EXT_EVT_CNT		0xAECu
#define PCIECFGREG_L1SS_EXT_STATE_TMR		0xAF0u
#define PCIECFGREG_STAT_CTRL			0xA80u
#define PCIECFGREG_STAT_CTRL_VAL		0x12u

/* PCIECFGREG_STATUS_CMD reg bit definitions */
#define PCIECFG_STS_CMD_MEM_SPACE_SHIFT		(1u)
#define	PCIECFG_STS_CMD_BUS_MASTER_SHIFT	(2u)

/* PCIECFGREG_PML1_SUB_CTRL1 Bit Definition */
#define PCI_PM_L1_2_ENA_MASK		0x00000001	/* PCI-PM L1.2 Enabled */
#define PCI_PM_L1_1_ENA_MASK		0x00000002	/* PCI-PM L1.1 Enabled */

#define ASPM_L1_2_ENA_MASK		0x00000004	/* ASPM L1.2 Enabled */
#define ASPM_L1_1_ENA_MASK		0x00000008	/* ASPM L1.1 Enabled */

/* PCIECFGREG_PDL_CTRL1 reg bit definitions */
#define PCIECFG_PDL_CTRL1_RETRAIN_REQ_MASK		(0x4000u)
#define PCIECFG_PDL_CTRL1_RETRAIN_REQ_SHIFT		(14u)
#define PCIECFG_PDL_CTRL1_MAX_DLP_L1_ENTER_MASK		(0x7Fu)
#define PCIECFG_PDL_CTRL1_MAX_DLP_L1_ENTER_SHIFT	(16u)
#define PCIECFG_PDL_CTRL1_MAX_DLP_L1_ENTER_VAL		(0x6Fu)

/* PCIECFGREG_PDL_CTRL5 reg bit definitions */
#define PCIECFG_PDL_CTRL5_DOWNSTREAM_PORT_SHIFT		(8u)
#define	PCIECFG_PDL_CTRL5_GLOOPBACK_SHIFT		(9u)

/* PCIECFGREG_REG_BAR2_CONFIG reg bit definitions */
#define PCIECFGREG_BAR2_SIZE_MASK	(0xFu)
#define PCIECFGREG_BAR2_SIZE_4M		(0x7u)
#define PCIECFGREG_BAR2_SIZE_8M		(0x8u)
#define PCIECFGREG_BAR2_SIZE_16M	(0x9u)

/* PCIe gen2 mailbox interrupt masks */
#define I_MB    0x3
#define I_BIT0  0x1
#define I_BIT1  0x2

/* PCIE gen2 config regs */
#define PCIIntstatus	0x090
#define PCIIntmask	0x094
#define PCISBMbx	0x98

#define PCIControl(rev)	\
	(REV_GE_64(rev) ? PCIE_REG_OFF(functioncontrol) : PCIE_REG_OFF(pciecontrol))
/* for corerev < 64 idma_en is in PCIControl regsiter */
#define IDMAControl(rev) \
	(REV_GE_64(rev) ? PCIE_REG_OFF(idma_ctrl) : PCIE_REG_OFF(pciecontrol))
#define PCIMailBoxInt(rev) \
	(REV_GE_64(rev) ? PCIE_REG_OFF(mailboxint) : PCIE_REG_OFF(mailboxint_V0))
#define PCIMailBoxMask(rev) \
	(REV_GE_64(rev) ? PCIE_REG_OFF(mailboxintmask) : PCIE_REG_OFF(mailboxintmask_V0))
#define PCIFunctionIntstatus(rev) \
	(REV_GE_64(rev) ? PCIE_REG_OFF(intstatus) : PCIE_REG_OFF(intstatus_V0))
#define PCIFunctionIntmask(rev)	\
	(REV_GE_64(rev) ? PCIE_REG_OFF(intmask) : PCIE_REG_OFF(intmask_V0))
#define PCIPowerIntstatus(rev) \
	(REV_GE_64(rev) ? PCIE_REG_OFF(powerintstatus) : PCIE_REG_OFF(powerintstatus_V0))
#define PCIPowerIntmask(rev) \
	(REV_GE_64(rev) ? PCIE_REG_OFF(powerintmask) : PCIE_REG_OFF(powerintmask_V0))
#define PCIDARClkCtl(rev) \
	(REV_GE_64(rev) ? PCIE_REG_OFF(dar_clk_ctl_st) : PCIE_REG_OFF(dar_clk_ctl_st_V0))
#define PCIDARPwrCtl(rev) \
	(REV_GE_64(rev) ? PCIE_REG_OFF(dar_power_control) : PCIE_REG_OFF(dar_power_control_V0))
#define PCIDARFunctionIntstatus(rev) \
	(REV_GE_64(rev) ? PCIE_REG_OFF(dar_intstatus) : PCIE_REG_OFF(dar_intstatus_V0))
#define PCIDARH2D_DB0(rev) \
	(REV_GE_64(rev) ? PCIE_REG_OFF(dar_h2d0_doorbell0) : PCIE_REG_OFF(dar_h2d0_doorbell0_V0))
#define PCIDARErrlog(rev) \
	(REV_GE_64(rev) ? PCIE_REG_OFF(dar_errorlog) : PCIE_REG_OFF(dar_errorlog_V0))
#define PCIDARErrlog_Addr(rev) \
	(REV_GE_64(rev) ? PCIE_REG_OFF(dar_errorlog_addr) : PCIE_REG_OFF(dar_errorlog_addr_V0))
#define PCIDARMailboxint(rev) \
	(REV_GE_64(rev) ? PCIE_REG_OFF(dar_mailboxint) : PCIE_REG_OFF(dar_mailboxint_V0))

#define PCIMSIVecAssign	0x58

/* base of all HMAP window registers */
#define PCI_HMAP_WINDOW_BASE(rev) \
	(REV_GE_64(rev) ? PCIE_REG_OFF(hmapwindow0_baseaddrlower) : \
	 PCIE_REG_OFF(hmapwindow0_baseaddrlower_V0))
#define PCI_HMAP_VIOLATION_ADDR_L(rev) \
	(REV_GE_64(rev) ? PCIE_REG_OFF(hmapviolation_erroraddrlower) : \
	 PCIE_REG_OFF(hmapviolation_erroraddrlower_V0))
#define PCI_HMAP_VIOLATION_ADDR_U(rev) \
	(REV_GE_64(rev) ? PCIE_REG_OFF(hmapviolation_erroraddrupper) : \
	 PCIE_REG_OFF(hmapviolation_erroraddrupper_V0))
#define PCI_HMAP_VIOLATION_INFO(rev) \
	(REV_GE_64(rev) ? PCIE_REG_OFF(hmapviolation_errorinfo) : \
	 PCIE_REG_OFF(hmapviolation_errorinfo_V0))
#define PCI_HMAP_WINDOW_CONFIG(rev) \
	(REV_GE_64(rev) ? PCIE_REG_OFF(hmapwindowconfig) : \
	 PCIE_REG_OFF(hmapwindowconfig_V0))

/* HMAP Register related  offsets */
#define PCI_HMAP_NWINDOWS_SHIFT		8U
#define PCI_HMAP_NWINDOWS_MASK		0x0000ff00U /* bits 8:15 */
#define PCI_HMAP_VIO_ID_MASK		0x0000007fU /* 0:14 */
#define PCI_HMAP_VIO_ID_SHIFT		0U
#define PCI_HMAP_VIO_SRC_MASK		0x00008000U /* bit 15 */
#define PCI_HMAP_VIO_SRC_SHIFT		15U
#define PCI_HMAP_VIO_TYPE_MASK		0x00010000U /* bit 16 */
#define PCI_HMAP_VIO_TYPE_SHIFT		16U
#define PCI_HMAP_VIO_ERR_MASK		0x00060000U /* bit 17:18 */
#define PCI_HMAP_VIO_ERR_SHIFT		17U

#define I_F0_B0         (0x1 << 8) /* Mail box interrupt Function 0 interrupt, bit 0 */
#define I_F0_B1         (0x1 << 9) /* Mail box interrupt Function 0 interrupt, bit 1 */

#define PCIECFGREG_DEVCONTROL	0xB4
#define PCIECFGREG_BASEADDR0	0x10
#define PCIECFGREG_BASEADDR1	0x18
#define PCIECFGREG_BASEADDR2	0x20
#define PCIECFGREG_DEVCONTROL_MRRS_SHFT	12
#define PCIECFGREG_DEVCONTROL_MRRS_MASK	(0x7 << PCIECFGREG_DEVCONTROL_MRRS_SHFT)
#define PCIECFGREG_DEVCTRL_MPS_SHFT	5
#define PCIECFGREG_DEVCTRL_MPS_MASK (0x7 << PCIECFGREG_DEVCTRL_MPS_SHFT)
#define PCIECFGREG_PM_CSR_STATE_MASK 0x00000003
#define PCIECFGREG_PM_CSR_STATE_D0 0
#define PCIECFGREG_PM_CSR_STATE_D1 1
#define PCIECFGREG_PM_CSR_STATE_D2 2
#define PCIECFGREG_PM_CSR_STATE_D3_HOT 3
#define PCIECFGREG_PM_CSR_STATE_D3_COLD 4

/* Direct Access regs */
#define DAR_ERRLOG(rev)		(REV_GE_64(rev) ? \
				PCIE_REG_OFF(dar_errorlog) : \
				PCIE_REG_OFF(dar_errorlog_V0))
#define DAR_ERRADDR(rev)	(REV_GE_64(rev) ? \
				PCIE_REG_OFF(dar_errorlog_addr) : \
				PCIE_REG_OFF(dar_errorlog_addr_V0))
#define DAR_CLK_CTRL(rev)	(REV_GE_64(rev) ? \
				PCIE_REG_OFF(dar_clk_ctl_st) : \
				PCIE_REG_OFF(dar_clk_ctl_st_V0))
#define DAR_INTSTAT(rev)	(REV_GE_64(rev) ? \
				PCIE_REG_OFF(dar_intstatus) : \
				PCIE_REG_OFF(dar_intstatus_V0))
#define DAR_PCIH2D_DB0_0(rev)	(REV_GE_64(rev) ? \
				PCIE_REG_OFF(dar_h2d0_doorbell0) : \
				PCIE_REG_OFF(dar_h2d0_doorbell0_V0))
#define DAR_PCIH2D_DB0_1(rev)	(REV_GE_64(rev) ? \
				PCIE_REG_OFF(dar_h2d0_doorbell1) : \
				PCIE_REG_OFF(dar_h2d0_doorbell1_V0))
#define DAR_PCIH2D_DB1_0(rev)	(REV_GE_64(rev) ? \
				PCIE_REG_OFF(dar_h2d1_doorbell0) : \
				PCIE_REG_OFF(dar_h2d1_doorbell0_V0))
#define DAR_PCIH2D_DB1_1(rev)	(REV_GE_64(rev) ? \
				PCIE_REG_OFF(dar_h2d1_doorbell1) : \
				PCIE_REG_OFF(dar_h2d1_doorbell1_V0))
#define DAR_PCIH2D_DB2_0(rev)	(REV_GE_64(rev) ? \
				PCIE_REG_OFF(dar_h2d2_doorbell0) : \
				PCIE_REG_OFF(dar_h2d2_doorbell0_V0))
#define DAR_PCIH2D_DB2_1(rev)	(REV_GE_64(rev) ? \
				PCIE_REG_OFF(dar_h2d2_doorbell1) : \
				PCIE_REG_OFF(dar_h2d2_doorbell1_V0))
#define DAR_PCIH2D_DB3_0(rev)	PCIE_REG_OFF(dar_h2d3_doorbell0)
#define DAR_PCIH2D_DB3_1(rev)	PCIE_REG_OFF(dar_h2d3_doorbell1)
#define DAR_PCIH2D_DB4_0(rev)	PCIE_REG_OFF(dar_h2d4_doorbell0)
#define DAR_PCIH2D_DB4_1(rev)	PCIE_REG_OFF(dar_h2d4_doorbell1)
#define DAR_PCIH2D_DB5_0(rev)	PCIE_REG_OFF(dar_h2d5_doorbell0)
#define DAR_PCIH2D_DB5_1(rev)	PCIE_REG_OFF(dar_h2d5_doorbell1)
#define DAR_PCIH2D_DB6_0(rev)	PCIE_REG_OFF(dar_h2d6_doorbell0)
#define DAR_PCIH2D_DB6_1(rev)	PCIE_REG_OFF(dar_h2d6_doorbell1)
#define DAR_PCIH2D_DB7_0(rev)	PCIE_REG_OFF(dar_h2d7_doorbell0)
#define DAR_PCIH2D_DB7_1(rev)	PCIE_REG_OFF(dar_h2d7_doorbell1)
#if !defined(DONGLEBUILD) || defined(BCMSTANDALONE_TEST)
#define DAR_PCIMailBoxInt(rev)	(REV_GE_64(rev) ? \
				PCIE_REG_OFF(dar_mailboxint) : \
				PCIE_REG_OFF(dar_mailboxint_V0))
#define DAR_PCIE_PWR_CTRL(rev)	(REV_GE_64(rev) ? \
				PCIE_REG_OFF(dar_power_control) : \
				PCIE_REG_OFF(dar_power_control_V0))
#define DAR_PCIE_DAR_CTRL(rev)	(REV_GE_64(rev) ? \
				PCIE_REG_OFF(dar_control) : \
				PCIE_REG_OFF(dar_control_V0))
#else
#define DAR_PCIMailBoxInt(rev)	PCIE_dar_mailboxint_OFFSET(rev)
#define DAR_PCIE_PWR_CTRL(rev)	PCIE_dar_power_control_OFFSET(rev)
#define DAR_PCIE_DAR_CTRL(rev)	PCIE_dar_control_OFFSET(rev)
#endif
#define DAR_FIS_CTRL(rev)	PCIE_REG_OFF(FISCtrl)

#define DAR_FIS_START_SHIFT	0u
#define DAR_FIS_START_MASK	(1u << DAR_FIS_START_SHIFT)

#define DAR_ERRLOG_SHIFT	8u
#define DAR_ERRLOG_MASK		(1u << DAR_ERRLOG_SHIFT)

#define DAR_SEC_STATUS(rev)	PCIE_REG_OFF(dar_security_status)

#define DAR_SEC_JTAG_MASK	0x1u
#define DAR_SEC_JTAG_SHIFT	0u
#define DAR_SEC_SBOOT_MASK	0x2u
#define DAR_SEC_SBOOT_SHIFT	1u
#define DAR_SEC_ARM_DBG_MASK	0x4u
#define DAR_SEC_ARM_DBG_SHIFT	2u
#define DAR_SEC_UNLOCK_MASK	0x8u
#define DAR_SEC_UNLOCK_SHIFT	3u
#define DAR_SEC_ROM_PROT_MASK	0x10u
#define DAR_SEC_ROM_PROT_SHIFT	4u
#define DAR_SEC_NSEC_WR_MASK	0x20u
#define DAR_SEC_NSEC_WR_SHIFT	5u
#define DAR_SEC_NSEC_RD_MASK	0x40u
#define DAR_SEC_NSEC_RD_SHIFT	6u

#define DAR_BPDEBUGINFO_SEL(rev)	(REV_GE_135(rev) ? PCIE_REG_OFF(dar_bpDebugInfoSel) : 0u)
#define DAR_BPDEBUGINFO(rev)		(REV_GE_135(rev) ? PCIE_REG_OFF(dar_bpDebugInfo) : 0u)

/* DAR BP DebugInfo Select bits */
#define DAR_BPDI_SEL_DBGBUS1		0
#define DAR_BPDI_SEL_PMU_RSRC_CNTL	1u
#define DAR_BPDI_SEL_PMU_RSRC_AVAIL	2u
#define DAR_BPDI_SEL_PMU_TOP_GPIO_OUT	3u
#define DAR_BPDI_SEL_STDC_LPMUXOUT_LO	4u
#define DAR_BPDI_SEL_STDC_LPMUXOUT_HI	5u
#define DAR_BPDI_SEL_GCI_CHIPSTS	6u
#define DAR_BPDI_SEL_GCI_GPIO_OUT	7u

#define PCIE_PWR_REQ_PCIE	(0x1 << 8)

/* SROM hardware region */
#define SROM_OFFSET_BAR1_CTRL  52

#define BAR1_ENC_SIZE_MASK	0x000e
#define BAR1_ENC_SIZE_SHIFT	1

#define BAR1_ENC_SIZE_1M	0
#define BAR1_ENC_SIZE_2M	1
#define BAR1_ENC_SIZE_4M	2

#define PCIEGEN2_CAP_DEVSTSCTRL2_OFFSET		0xD4
#define PCIEGEN2_CAP_DEVSTSCTRL2_LTRENAB	0x400

/*
 * Latency Tolerance Reporting (LTR) states
 * Active has the least tolerant latency requirement
 * Sleep is most tolerant
 */
#define LTR_ACTIVE				2
#define LTR_ACTIVE_IDLE				1
#define LTR_SLEEP				0
#define LTR_FINAL_MASK				0x300
#define LTR_FINAL_SHIFT				8

/* pwrinstatus, pwrintmask regs */
#define PCIEGEN2_PWRINT_D0_STATE_SHIFT		0
#define PCIEGEN2_PWRINT_D1_STATE_SHIFT		1
#define PCIEGEN2_PWRINT_D2_STATE_SHIFT		2
#define PCIEGEN2_PWRINT_D3_STATE_SHIFT		3
#define PCIEGEN2_PWRINT_L0_LINK_SHIFT		4
#define PCIEGEN2_PWRINT_L0s_LINK_SHIFT		5
#define PCIEGEN2_PWRINT_L1_LINK_SHIFT		6
#define PCIEGEN2_PWRINT_L2_L3_LINK_SHIFT	7
#define PCIEGEN2_PWRINT_OBFF_CHANGE_SHIFT	8

#define PCIEGEN2_PWRINT_D0_STATE_MASK		(1 << PCIEGEN2_PWRINT_D0_STATE_SHIFT)
#define PCIEGEN2_PWRINT_D1_STATE_MASK		(1 << PCIEGEN2_PWRINT_D1_STATE_SHIFT)
#define PCIEGEN2_PWRINT_D2_STATE_MASK		(1 << PCIEGEN2_PWRINT_D2_STATE_SHIFT)
#define PCIEGEN2_PWRINT_D3_STATE_MASK		(1 << PCIEGEN2_PWRINT_D3_STATE_SHIFT)
#define PCIEGEN2_PWRINT_L0_LINK_MASK		(1 << PCIEGEN2_PWRINT_L0_LINK_SHIFT)
#define PCIEGEN2_PWRINT_L0s_LINK_MASK		(1 << PCIEGEN2_PWRINT_L0s_LINK_SHIFT)
#define PCIEGEN2_PWRINT_L1_LINK_MASK		(1 << PCIEGEN2_PWRINT_L1_LINK_SHIFT)
#define PCIEGEN2_PWRINT_L2_L3_LINK_MASK		(1 << PCIEGEN2_PWRINT_L2_L3_LINK_SHIFT)
#define PCIEGEN2_PWRINT_OBFF_CHANGE_MASK	(1 << PCIEGEN2_PWRINT_OBFF_CHANGE_SHIFT)

/* sbtopcie mail box */
#define SBTOPCIE_MB_FUNC0_SHIFT 8
#define SBTOPCIE_MB_FUNC1_SHIFT 10
#define SBTOPCIE_MB_FUNC2_SHIFT 12
#define SBTOPCIE_MB_FUNC3_SHIFT 14

#define SBTOPCIE_MB1_FUNC0_SHIFT 9
#define SBTOPCIE_MB1_FUNC1_SHIFT 11
#define SBTOPCIE_MB1_FUNC2_SHIFT 13
#define SBTOPCIE_MB1_FUNC3_SHIFT 15

/* pcieiostatus/functioniostatus */
#define PCIEGEN2_IOC_D0_STATE_SHIFT		8
#define PCIEGEN2_IOC_D1_STATE_SHIFT		9
#define PCIEGEN2_IOC_D2_STATE_SHIFT		10
#define PCIEGEN2_IOC_D3_STATE_SHIFT		11
#define PCIEGEN2_IOC_L0_LINK_SHIFT		12
#define PCIEGEN2_IOC_L1_LINK_SHIFT		13
#define PCIEGEN2_IOC_L1L2_LINK_SHIFT		14
#define PCIEGEN2_IOC_L2_L3_LINK_SHIFT		15
#define PCIEGEN2_IOC_BME_SHIFT			20

#define PCIEGEN2_IOC_D0_STATE_MASK		(1 << PCIEGEN2_IOC_D0_STATE_SHIFT)
#define PCIEGEN2_IOC_D1_STATE_MASK		(1 << PCIEGEN2_IOC_D1_STATE_SHIFT)
#define PCIEGEN2_IOC_D2_STATE_MASK		(1 << PCIEGEN2_IOC_D2_STATE_SHIFT)
#define PCIEGEN2_IOC_D3_STATE_MASK		(1 << PCIEGEN2_IOC_D3_STATE_SHIFT)
#define PCIEGEN2_IOC_L0_LINK_MASK		(1 << PCIEGEN2_IOC_L0_LINK_SHIFT)
#define PCIEGEN2_IOC_L1_LINK_MASK		(1 << PCIEGEN2_IOC_L1_LINK_SHIFT)
#define PCIEGEN2_IOC_L1L2_LINK_MASK		(1 << PCIEGEN2_IOC_L1L2_LINK_SHIFT)
#define PCIEGEN2_IOC_L2_L3_LINK_MASK		(1 << PCIEGEN2_IOC_L2_L3_LINK_SHIFT)
#define PCIEGEN2_IOC_BME_MASK			(1 << PCIEGEN2_IOC_BME_SHIFT)

/* stat_ctrl */
#define PCIE_STAT_CTRL_RESET		0x1
#define PCIE_STAT_CTRL_ENABLE		0x2
#define PCIE_STAT_CTRL_INTENABLE	0x4
#define PCIE_STAT_CTRL_INTSTATUS	0x8

/* cpl_timeout_ctrl_reg */
#define PCIE_CTO_TO_THRESHOLD_SHIFT	0
#define PCIE_CTO_TO_THRESHHOLD_MASK	(0xfffff << PCIE_CTO_TO_THRESHOLD_SHIFT)

#define PCIE_CTO_CLKCHKCNT_SHIFT		24
#define PCIE_CTO_CLKCHKCNT_MASK		(0xf << PCIE_CTO_CLKCHKCNT_SHIFT)

#define PCIE_CTO_ENAB_SHIFT			31
#define PCIE_CTO_ENAB_MASK			(0x1 << PCIE_CTO_ENAB_SHIFT)

/*
 * For corerev >= 69, core_fref is always 29.9MHz instead of 37.4MHz.
 * Use different default threshold value to have 10ms timeout (0x49FB6 * 33ns).
 * threshold value is in units of core_fref clock period.
 */
#define PCIE_CTO_TO_THRESH_DEFAULT		0x58000
#define PCIE_CTO_TO_THRESH_DEFAULT_REV69	0x49FB6
/* core_fref clock is 40Mhz in 4397b0, so change the
 * threshold value to maintain 10ms timeout value
 */
#define PCIE_CTO_TO_THRESH_DEFAULT_4397B0	0x61A80

#define PCIE_CTO_CLKCHKCNT_VAL		0xA

/* ErrLog */
#define PCIE_SROMRD_ERR_SHIFT			5
#define PCIE_SROMRD_ERR_MASK			(0x1 << PCIE_SROMRD_ERR_SHIFT)

#define PCIE_CTO_ERR_SHIFT			8
#define PCIE_CTO_ERR_MASK				(0x1 << PCIE_CTO_ERR_SHIFT)

#define PCIE_CTO_ERR_CODE_SHIFT		9
#define PCIE_CTO_ERR_CODE_MASK		(0x3 << PCIE_CTO_ERR_CODE_SHIFT)

#define PCIE_BP_CLK_OFF_ERR_SHIFT		12
#define PCIE_BP_CLK_OFF_ERR_MASK		(0x1 << PCIE_BP_CLK_OFF_ERR_SHIFT)

#define PCIE_BP_IN_RESET_ERR_SHIFT	13
#define PCIE_BP_IN_RESET_ERR_MASK		(0x1 << PCIE_BP_IN_RESET_ERR_SHIFT)

/* PCIE control per Function */
#define PCIE_FTN_DLYPERST_SHIFT		1
#define PCIE_FTN_DLYPERST_MASK		(1 << PCIE_FTN_DLYPERST_SHIFT)

#define PCIE_FTN_WakeModeL2_SHIFT	3
#define PCIE_FTN_WakeModeL2_MASK	(1 << PCIE_FTN_WakeModeL2_SHIFT)

#define PCIE_FTN_MSI_B2B_EN_SHIFT	4
#define PCIE_FTN_MSI_B2B_EN_MASK	(1 << PCIE_FTN_MSI_B2B_EN_SHIFT)

#define PCIE_FTN_MSI_FIFO_CLEAR_SHIFT	5
#define PCIE_FTN_MSI_FIFO_CLEAR_MASK	(1 << PCIE_FTN_MSI_FIFO_CLEAR_SHIFT)

#define PCIE_FTN_SWPME_SHIFT		6
#define PCIE_FTN_SWPME_MASK			(1 << PCIE_FTN_SWPME_SHIFT)

#ifdef BCMDRIVER
#if !defined(DONGLEBUILD) || defined(BCMSTANDALONE_TEST)
void pcie_watchdog_reset(osl_t *osh, si_t *sih, uint32 wd_mask, uint32 wd_val);
void pcie_serdes_iddqdisable(osl_t *osh, si_t *sih, sbpcieregs_t *sbpcieregs);
void pcie_set_trefup_time_100us(si_t *sih);
uint32 pcie_cto_to_thresh_default(uint corerev);
uint32 pcie_corereg(osl_t *osh, volatile void *regs, uint32 offset, uint32 mask, uint32 val);
#endif /* !defined(DONGLEBUILD) || defined(BCMSTANDALONE_TEST) */
#if defined(DONGLEBUILD)
void pcie_coherent_accenable(osl_t *osh, si_t *sih);
#endif /* DONGLEBUILD */
#endif /* BCMDRIVER */

/* DMA intstatus and intmask */
#define	I_PC		(1 << 10)	/* pci descriptor error */
#define	I_PD		(1 << 11)	/* pci data error */
#define	I_DE		(1 << 12)	/* descriptor protocol error */
#define	I_RU		(1 << 13)	/* receive descriptor underflow */
#define	I_RO		(1 << 14)	/* receive fifo overflow */
#define	I_XU		(1 << 15)	/* transmit fifo underflow */
#define	I_RI		(1 << 16)	/* receive interrupt */
#define	I_XI		(1 << 24)	/* transmit interrupt */

#define PD_DMA_INT_MASK_H2D		0x1DC00
#define PD_DMA_INT_MASK_D2H		0x1DC00
#define PD_DB_INT_MASK			0xFF0000

#if defined(DONGLEBUILD)
#if REV_GE_64(BCMPCIEREV)
#define PD_DEV0_DB_INTSHIFT		8u
#define PD_DEV1_DB_INTSHIFT		10u
#define PD_DEV2_DB_INTSHIFT		12u
#define PD_DEV3_DB_INTSHIFT		14u
#else
#define PD_DEV0_DB_INTSHIFT		16u
#define PD_DEV1_DB_INTSHIFT		18u
#define PD_DEV2_DB_INTSHIFT		20u
#define PD_DEV3_DB_INTSHIFT		22u
#endif /* BCMPCIEREV */
#endif /* DONGLEBUILD */

#define PCIE_INVALID_OFFSET		0x18003ffc /* Invalid Register Offset for Induce Error */
#define PCIE_INVALID_DATA		0x55555555 /* Invalid Data for Induce Error */

#define PD_DEV0_DB0_INTMASK       (0x1 << PD_DEV0_DB_INTSHIFT)
#define PD_DEV0_DB1_INTMASK       (0x2 << PD_DEV0_DB_INTSHIFT)
#define PD_DEV0_DB_INTMASK        ((PD_DEV0_DB0_INTMASK) | (PD_DEV0_DB1_INTMASK))

#define PD_DEV1_DB0_INTMASK       (0x1 << PD_DEV1_DB_INTSHIFT)
#define PD_DEV1_DB1_INTMASK       (0x2 << PD_DEV1_DB_INTSHIFT)
#define PD_DEV1_DB_INTMASK        ((PD_DEV1_DB0_INTMASK) | (PD_DEV1_DB1_INTMASK))

#define PD_DEV2_DB0_INTMASK       (0x1 << PD_DEV2_DB_INTSHIFT)
#define PD_DEV2_DB1_INTMASK       (0x2 << PD_DEV2_DB_INTSHIFT)
#define PD_DEV2_DB_INTMASK        ((PD_DEV2_DB0_INTMASK) | (PD_DEV2_DB1_INTMASK))

#define PD_DEV3_DB0_INTMASK       (0x1 << PD_DEV3_DB_INTSHIFT)
#define PD_DEV3_DB1_INTMASK       (0x2 << PD_DEV3_DB_INTSHIFT)
#define PD_DEV3_DB_INTMASK        ((PD_DEV3_DB0_INTMASK) | (PD_DEV3_DB1_INTMASK))

#define PD_DEV0_DMA_INTMASK       0x80

#define PD_FUNC0_MB_INTSHIFT		8u
#define PD_FUNC0_MB_INTMASK		(0x3 << PD_FUNC0_MB_INTSHIFT)

#define PD_FUNC0_PCIE_SB_INTSHIFT       0u
#define PD_FUNC0_PCIE_SB__INTMASK       (0x3 << PD_FUNC0_PCIE_SB_INTSHIFT)

#define PD_DEV0_PWRSTATE_INTSHIFT	24u
#define PD_DEV0_PWRSTATE_INTMASK	(0x1 << PD_DEV0_PWRSTATE_INTSHIFT)

#define PD_DEV0_PERST_INTSHIFT		6u
#define PD_DEV0_PERST_INTMASK		(0x1 << PD_DEV0_PERST_INTSHIFT)

#define PD_MSI_FIFO_OVERFLOW_INTSHIFT		28u
#define PD_MSI_FIFO_OVERFLOW_INTMASK		(0x1 << PD_MSI_FIFO_OVERFLOW_INTSHIFT)

#if defined(BCMPCIE_IFRM)
#define PD_IFRM_INTSHIFT		5u
#define PD_IFRM_INTMASK		(0x1 << PD_IFRM_INTSHIFT)
#endif /* BCMPCIE_IFRM */

/* HMAP related constants */
#define PD_HMAP_VIO_INTSHIFT	3u
#define PD_HMAP_VIO_INTMASK	(0x1 << PD_HMAP_VIO_INTSHIFT)
#define PD_HMAP_VIO_CLR_VAL	0x3 /* write 0b11 to clear HMAP violation */
#define PD_HMAP_VIO_SHIFT_VAL	17u  /* bits 17:18 clear HMAP violation */

#define PD_FLR0_IN_PROG_INTSHIFT		0u
#define PD_FLR0_IN_PROG_INTMASK			(0x1 << PD_FLR0_IN_PROG_INTSHIFT)
#define PD_FLR1_IN_PROG_INTSHIFT		1u
#define PD_FLR1_IN_PROG_INTMASK			(0x1 << PD_FLR1_IN_PROG_INTSHIFT)

#define PD_PTM_INTSHIFT		1u
#define PD_PTM_INTMASK		(0x1u << PD_PTM_INTSHIFT)

/* DMA channel 2 datapath use case
 * Implicit DMA uses DMA channel 2 (outbound only)
 */
#if defined(BCMPCIE_IDMA) && !defined(BCMPCIE_IDMA_DISABLED)
#define PD_DEV2_INTMASK PD_DEV2_DB0_INTMASK
#elif defined(BCMPCIE_IFRM) && !defined(BCMPCIE_IFRM_DISABLED)
#define PD_DEV2_INTMASK PD_DEV2_DB0_INTMASK
#elif defined(BCMPCIE_DMA_CH2)
#define PD_DEV2_INTMASK PD_DEV2_DB0_INTMASK
#else
#define PD_DEV2_INTMASK 0u
#endif /* BCMPCIE_IDMA || BCMPCIE_DMA_CH2 || BCMPCIE_IFRM */
/* DMA channel 1 datapath use case */
#ifdef BCMPCIE_DMA_CH1
#define PD_DEV1_INTMASK PD_DEV1_DB0_INTMASK
#else
#define PD_DEV1_INTMASK 0u
#endif /* BCMPCIE_DMA_CH1 */
#if defined(BCMPCIE_IDMA) || defined(BCMPCIE_IFRM)
#define PD_DEV1_IDMA_DW_INTMASK PD_DEV1_DB1_INTMASK
#else
#define PD_DEV1_IDMA_DW_INTMASK 0u
#endif /* BCMPCIE_IDMA || BCMPCIE_IFRM */

#define PD_DEV0_INTMASK		\
	(PD_DEV0_DMA_INTMASK | PD_DEV0_DB0_INTMASK | PD_DEV0_PWRSTATE_INTMASK | \
	PD_DEV0_PERST_INTMASK | PD_DEV1_INTMASK | PD_DEV2_INTMASK | PD_DEV0_DB1_INTMASK | \
	PD_DEV1_IDMA_DW_INTMASK)

/* implicit DMA index */
#define	PD_IDMA_COMP			0xf		/* implicit dma complete */
#define	PD_IDMA_IDX0_COMP		((uint32)1 << 0)	/* implicit dma index0 complete */
#define	PD_IDMA_IDX1_COMP		((uint32)1 << 1)	/* implicit dma index1 complete */
#define	PD_IDMA_IDX2_COMP		((uint32)1 << 2)	/* implicit dma index2 complete */
#define	PD_IDMA_IDX3_COMP		((uint32)1 << 3)	/* implicit dma index3 complete */

#define PCIE_D2H_DB0_VAL   (0x12345678)

#define PD_ERR_ATTN_INTMASK		(1u << 29)
#define PD_LINK_DOWN_INTMASK	(1u << 27)

#define PD_ERR_TTX_REQ_DURING_D3	(1u << 31)	/* Tx mem req on iface when in non-D0 */
#define PD_PRI_SIG_TARGET_ABORT_F1	(1u << 19)	/* Rcvd target Abort Err Status (CA) F1 */
#define PD_ERR_UNSPPORT_F1		(1u << 18)	/* Unsupported Request Error Status. F1 */
#define PD_ERR_ECRC_F1			(1u << 17)	/* ECRC Error TLP Status. F1 */
#define PD_ERR_MALF_TLP_F1		(1u << 16)	/* Malformed TLP Status. F1 */
#define PD_ERR_RX_OFLOW_F1		(1u << 15)	/* Receiver Overflow Status. */
#define PD_ERR_UNEXP_CPL_F1		(1u << 14)	/* Unexpected Completion Status. F1 */
#define PD_ERR_MASTER_ABRT_F1		(1u << 13)	/* Receive UR Completion Status. F1 */
#define PD_ERR_CPL_TIMEOUT_F1		(1u << 12)	/* Completer Timeout Status F1 */
#define PD_ERR_FC_PRTL_F1		(1u << 11)	/* Flow Control Protocol Error Status F1 */
#define PD_ERR_PSND_TLP_F1		(1u << 10)	/* Poisoned Error Status F1 */
#define PD_PRI_SIG_TARGET_ABORT		(1u << 9)	/* Received target Abort Error Status(CA) */
#define PD_ERR_UNSPPORT			(1u << 8)	/* Unsupported Request Error Status. */
#define PD_ERR_ECRC			(1u << 7)	/* ECRC Error TLP Status. */
#define PD_ERR_MALF_TLP			(1u << 6)	/* Malformed TLP Status. */
#define PD_ERR_RX_OFLOW			(1u << 5)	/* Receiver Overflow Status. */
#define PD_ERR_UNEXP_CPL		(1u << 4)	/* Unexpected Completion Status. */
#define PD_ERR_MASTER_ABRT		(1u << 3)	/* Receive UR Completion Status. */
#define PD_ERR_CPL_TIMEOUT		(1u << 2)	/* Completer Timeout Status */
#define PD_ERR_FC_PRTL			(1u << 1)	/* Flow Control Protocol Error Status */
#define PD_ERR_PSND_TLP			(1u << 0)	/* Poisoned Error Status */

/* All ERR_ATTN of F1 */
#define PD_ERR_FUNCTION1	\
	(PD_ERR_PSND_TLP_F1 | PD_ERR_FC_PRTL_F1 | PD_ERR_CPL_TIMEOUT_F1 | PD_ERR_MASTER_ABRT_F1 | \
	PD_ERR_UNEXP_CPL_F1 | PD_ERR_RX_OFLOW_F1 | PD_ERR_MALF_TLP_F1 | PD_ERR_ECRC_F1 | \
	PD_ERR_UNSPPORT_F1 | PD_PRI_SIG_TARGET_ABORT_F1)

#define PD_ERR_TTX_REQ_DURING_D3_FN0	(1u << 10)	/* Tx mem req on iface when in non-D0 */

/* H2D Doorbell Fields for IDMA / PWI */
#define PD_DB_FRG_ID_SHIFT		(0u)
#define PD_DB_FRG_ID_MASK		(0xFu)		/* bits 3:0 */
#define PD_DB_DMA_TYPE_SHIFT		(4u)
#define PD_DB_DMA_TYPE_MASK		(0xFu)		/* bits 7:4 */
#define PD_DB_RINGIDX_NUM_SHIFT		(8u)
#define PD_DB_RINGIDX_NUM_MASK		(0xFFu)		/* bits 15:8 */
#define PD_DB_INDEX_VAL_SHIFT		(16u)
#define PD_DB_INDEX_VAL_MASK		(0xFFFFu)	/* bits 31:16 */

/* PWI LUT entry fields */
#define PWI_FLOW_VALID_MASK		(0x1u)
#define PWI_FLOW_VALID_SHIFT		(22u)
#define PWI_FLOW_RING_GROUP_ID_MASK	(0x3u)
#define PWI_FLOW_RING_GROUP_ID_SHIFT	(20u)
#define PWI_HOST_RINGIDX_MASK	(0xFFu) /* Host Ring Index Number[19:12] */
#define PWI_HOST_RINGIDX_SHIFT	(12u)

/* DMA_TYPE Values */
#define PD_DB_DMA_TYPE_NO_IDMA	(0u)
#define PD_DB_DMA_TYPE_IDMA	(1u)
#define PD_DB_DMA_TYPE_PWI	(2u)
#define PD_DB_DMA_TYPE_RXPOST(rev)	(REV_GE_73((rev)) ? (1u) : (5u))
#define PD_DB_DMA_TYPE_TXCPL(rev)	(REV_GE_73((rev)) ? (2u) : (6u))
#define PD_DB_DMA_TYPE_RXCPL(rev)	(REV_GE_73((rev)) ? (3u) : (7u))

/* All ERR_ATTN of F0 */
#define PD_ERR_FUNCTION0	\
	(PD_ERR_PSND_TLP | PD_ERR_FC_PRTL | PD_ERR_CPL_TIMEOUT | PD_ERR_MASTER_ABRT | \
	PD_ERR_UNEXP_CPL | PD_ERR_RX_OFLOW | PD_ERR_MALF_TLP | PD_ERR_ECRC | \
	PD_ERR_UNSPPORT | PD_PRI_SIG_TARGET_ABORT)
/* Shift of F1 bits */
#define PD_ERR_FUNCTION1_SHIFT  10u

/* access to register offsets and fields defined in vlsi_pciegen2_F0_all_regs.h */

/* Include Regs from vlsi_xxx files only for Dongle FW builds */
#if defined(DONGLEBUILD)
#include <vlsi_pciegen2_F0_all_regs.h>
#endif /* DONGLEBUILD */

typedef volatile struct pcieregs pcieregs_t;

#define PCIE_REG_OFF(regname) \
	pciegen2_##regname##_ADDR
#define PCIE_REG_FIELD_MASK(regname, regfield) \
	pciegen2_##regname##__##regfield##_MASK
#define PCIE_REG_FIELD_SHIFT(regname, regfield) \
	pciegen2_##regname##__##regfield##_SHIFT

/* convert register offset to backplane address */

#define PCIE_REG_ADDR(regbase, regname) \
	(volatile uint32 *)((uintptr)(regbase) + PCIE_REG_OFF(regname))

/* SROM registers not in the new vlsi_pciegen2_F0_all_regs.h */
#define pciegen2_srom_ADDR			0x800u
#define pciegen2_srom_trefup_ADDR		0x820u
#define pciegen2_srom_trefup_ext_ADDR		0x824u
#define pciegen2_srom_mdio_seq_control_ADDR	0x830u

/* For revid >=64 DHD builds, define only those registers that needs to be accessed from Host */
#if !defined(DONGLEBUILD)
#define pciegen2_intstatus_ADDR                                                         0xc10u
#define pciegen2_dar_power_control_ADDR                                                 0xa0cu
#define pciegen2_dar_clk_ctl_st_ADDR                                                    0xa08u
#define pciegen2_dar_intstatus_ADDR                                                     0xa10u
#define pciegen2_dar_errorlog_ADDR                                                      0xa60u
#define pciegen2_dar_errorlog_addr_ADDR                                                 0xa64u
#define pciegen2_dar_h2d3_doorbell0_ADDR                                                0xa38u
#define pciegen2_dar_h2d3_doorbell1_ADDR                                                0xa3cu
#define pciegen2_mailboxint_ADDR                                                        0xc30u
#define pciegen2_mailboxintmask_ADDR                                                    0xc34u
#define pciegen2_FISCtrl_ADDR                                                           0xa6cu
#define pciegen2_idma_ctrl_ADDR                                                         0x480u
#define pciegen2_pciecontrol_ADDR                                                       0x0u
#define pciegen2_ConfigIndAddr_ADDR                                                     0x120u
#define pciegen2_ConfigIndData_ADDR                                                     0x124u
#define pciegen2_mdiocontrol_ADDR                                                       0x128u
#define pciegen2_mdiorddata_ADDR                                                        0x130u
#define pciegen2_mdiowrdata_ADDR                                                        0x12cu
#define pciegen2_functioncontrol_ADDR                                                   0xc00u
#define pciegen2_MSIVector_ADDR                                                         0xc20u
#define pciegen2_MSIIntMask_ADDR                                                        0xc24u
#define pciegen2_MSIIntStatus_ADDR                                                      0xc28u
#define pciegen2_powerintmask_ADDR                                                      0xc1cu
#define pciegen2_powerintstatus_ADDR                                                    0xc18u
#define pciegen2_mailboxintmask_ADDR                                                    0xc34u
#define pciegen2_mailboxint_ADDR                                                        0xc30u
#define pciegen2_ClkControl_ADDR                                                        0x1e0u
#define pciegen2_hosttodev1doorbell0_ADDR                                               0x150u
#define pciegen2_hosttodev2doorbell0_ADDR                                               0x160u
#define pciegen2_dar_h2d1_doorbell0_ADDR                                                0xa28u
#define pciegen2_dar_h2d2_doorbell0_ADDR                                                0xa30u
#define pciegen2_hosttodev2doorbell0_ADDR                                               0x160u
#define pciegen2_hosttodev0doorbell1_ADDR                                               0x144u
#define pciegen2_hosttodev3doorbell0_ADDR                                               0x170u
#define pciegen2_hosttodev3doorbell1_ADDR                                               0x174u
#define pciegen2_devtohost0doorbell0_ADDR                                               0x148u
#define pciegen2_error_header_reg1_ADDR                                                 0x1b0u
#define pciegen2_error_header_reg2_ADDR                                                 0x1b4u
#define pciegen2_error_header_reg3_ADDR                                                 0x1b8u
#define pciegen2_error_header_reg4_ADDR                                                 0x1bcu
#define pciegen2_error_code_ADDR                                                        0x1c0u
#define pciegen2_hosttodev0doorbell0_ADDR                                               0x140u
#define pciegen2_hmapwindow0_baseaddrlower_ADDR                                         0x580u
#define pciegen2_dar_h2d0_doorbell0_ADDR                                                0xa20u
#define pciegen2_hmapwindowconfig_ADDR                                                  0x610u
#define pciegen2_intmask_ADDR                                                           0xc14u
#define pciegen2_hmapwindowconfig_ADDR                                                  0x610u
#define pciegen2_hmapviolation_erroraddrupper_ADDR                                      0x604u
#define pciegen2_hmapviolation_erroraddrlower_ADDR                                      0x600u
#define pciegen2_dar_mailboxint_ADDR                                                    0xa68u
#define pciegen2_hmapviolation_errorinfo_ADDR                                           0x608u
#define pciegen2_dar_security_status_ADDR                                               0xa74u
#define pciegen2_dar_bpDebugInfo_ADDR							0xa88u
#define pciegen2_dar_bpDebugInfoSel_ADDR						0xa8cu
#define pciegen2_cpl_to_ctrl_ADDR                                                       0x3cu
#endif /* !DONGLEBUILD */

/* SPROM region access macro */
#define PCIE_SROM_ADDR(regbase, sromoff) \
	((volatile uint16 *)PCIE_REG_ADDR(regbase, srom) + sromoff)

/* For revid < 64, used by dhd only */
#define pciegen2_intstatus_V0_ADDR		0x20u
#define pciegen2_intmask_V0_ADDR		0x24u
#define pciegen2_errorlog_V0_ADDR		0x40u
#define pciegen2_errorlog_addr_V0_ADDR		0x44u
#define pciegen2_mailboxint_V0_ADDR		0x48u
#define pciegen2_mailboxintmask_V0_ADDR		0x4cu
#define pciegen2_powerintstatus_V0_ADDR		0x1a4u
#define pciegen2_powerintmask_V0_ADDR		0x1a8u
#define pciegen2_hmapwindow0_baseaddrlower_V0_ADDR	0x540u
#define pciegen2_hmapviolation_erroraddrlower_V0_ADDR	0x5c0u
#define pciegen2_hmapviolation_erroraddrupper_V0_ADDR	0x5c4u
#define pciegen2_hmapviolation_errorinfo_V0_ADDR	0x5c8u
#define pciegen2_hmapwindowconfig_V0_ADDR	0x5d0u
#define pciegen2_dar_control_V0_ADDR		0xa00u
#define pciegen2_dar_intstatus_V0_ADDR		0xa20u
#define pciegen2_dar_h2d0_doorbell1_ADDR	0xa24u
#define pciegen2_dar_h2d0_doorbell0_V0_ADDR	0xa28u
#define pciegen2_dar_h2d0_doorbell1_V0_ADDR	0xa2cu
#define pciegen2_dar_h2d1_doorbell0_V0_ADDR	0xa30u
#define pciegen2_dar_h2d1_doorbell1_V0_ADDR	0xa34u
#define pciegen2_dar_h2d2_doorbell0_V0_ADDR	0xa38u
#define pciegen2_dar_h2d2_doorbell1_V0_ADDR	0xa3cu
#define pciegen2_dar_errorlog_V0_ADDR		0xa40u
#define pciegen2_dar_errorlog_addr_V0_ADDR	0xa44u
#define pciegen2_dar_mailboxint_V0_ADDR		0xa48u
#define pciegen2_dar_clk_ctl_st_V0_ADDR		0xae0u
#define pciegen2_dar_power_control_V0_ADDR	0xae8u

/* For revid < ?, used by dhd/nic only */
#define pciegen2_ltr_state_ADDR			0x1a0u

/* Force a compile error if any register is referenced that does not exist in the built pciegen2's
 * register set.
 */
#undef INVALID_ADDRESS_pciegen2
#define INVALID_ADDRESS_pciegen2 hnd_invalid_reg_pciegen2()
#undef INVALID_SHIFT_pciegen2
#define INVALID_SHIFT_pciegen2 hnd_invalid_reg_pciegen2()

#endif	/* _PCIE_CORE_H */
