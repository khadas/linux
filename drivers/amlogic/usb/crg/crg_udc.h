/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __CRG_UDC_H__
#define __CRG_UDC_H__

#define CRG_UCCR_OFFSET (0x2400)
#define CRG_UICR_OFFSET (0x2500)
#define CRG_UICR_STRIDE (0x20)

/* corigine usb 3.1 device core register macros */
struct crg_uccr {
	u32 capability;	/*0x00*/
	u32 resv0[3];

	u32 config0;	/*0x10*/
	u32 config1;
	u32 resv1[2];

	u32 control;	/*0x20*/
	u32 status;
	u32 dcbaplo;
	u32 dcbaphi;
	u32 portsc;
	u32 u3portpmsc;
	u32 u2portpmsc;
	u32 u3portli;

	u32 doorbell;	/*0x40*/
	u32 mfindex;
	u32 speed_select;
	u32 resv3[5];

	u32 ep_enable;	/*0x60*/
	u32 ep_running;
	u32 resv4[2];

	u32 cmd_param0;	/*0x70*/
	u32 cmd_param1;
	u32 cmd_control;
	u32 resv5[1];

	u32 odb_capability;	/*0x80*/
	u32 resv6[3];
	u32 odb_config[8];

	u32 debug0;	/*0xB0*/
};

struct crg_uicr {
	u32 iman;
	u32 imod;
	u32 erstsz;
	u32 resv0;

	u32 erstbalo; /*0x10*/
	u32 erstbahi;
	u32 erdplo;
	u32 erdphi;
};

#define SETF_VAR(field, var, fieldval) \
	(((var) & ~(field ## _MASK)) | \
			(((fieldval) << field ## _SHIFT) & (field ## _MASK)))

#define GETF(field, val) \
		(((val) & (field ## _MASK)) >> (field ## _SHIFT))

#define MAKEF_VAR(field, fieldval) \
		(((fieldval) << field ## _SHIFT) & (field ## _MASK))

/* interrupt registers array */
#define	CRG_U3DC_IRS_BASE		(0x100L)
#define	CRG_U3DC_IRS_IMAN(x)		(CRG_U3DC_IRS_BASE + (x) * 0x20L + 0x00L)
#define	CRG_U3DC_IRS_IMOD(x)		(CRG_U3DC_IRS_BASE + (x) * 0x20L + 0x04L)
#define	CRG_U3DC_IRS_ERSTSZ(x)		(CRG_U3DC_IRS_BASE + (x) * 0x20L + 0x08L)
#define	CRG_U3DC_IRS_ERSTBALO(x)	(CRG_U3DC_IRS_BASE + (x) * 0x20L + 0x10L)
#define	CRG_U3DC_IRS_ERSTBAHI(x)	(CRG_U3DC_IRS_BASE + (x) * 0x20L + 0x14L)
#define	CRG_U3DC_IRS_ERDPLO(x)		(CRG_U3DC_IRS_BASE + (x) * 0x20L + 0x18L)
#define	CRG_U3DC_IRS_ERDPHI(x)		(CRG_U3DC_IRS_BASE + (x) * 0x20L + 0x20L)

/* reg config 0 & 1*/
#define	CRG_U3DC_CFG0_MAXSPEED_MASK		(0xfL << 0)
#define	CRG_U3DC_CFG0_MAXSPEED_FS		(0x1L << 0)
#define	CRG_U3DC_CFG0_MAXSPEED_HS		(0x3L << 0)
#define	CRG_U3DC_CFG0_MAXSPEED_SS		(0x4L << 0)
#define	CRG_U3DC_CFG0_MAXSPEED_SSP		(0x5L << 0)

#define	CRG_U3DC_CFG1_CSC_EVENT_EN		BIT(0)
#define	CRG_U3DC_CFG1_PEC_EVENT_EN		BIT(1)
#define	CRG_U3DC_CFG1_PPC_EVENT_EN		BIT(3)
#define	CRG_U3DC_CFG1_PRC_EVENT_EN		BIT(4)
#define	CRG_U3DC_CFG1_PLC_EVENT_EN		BIT(5)
#define	CRG_U3DC_CFG1_CEC_EVENT_EN		BIT(6)

#define	CRG_U3DC_CFG1_U3_ENTRY_EN		BIT(8)
#define	CRG_U3DC_CFG1_L1_ENTRY_EN		BIT(9)
#define	CRG_U3DC_CFG1_U3_RESUME_EN		BIT(10)
#define	CRG_U3DC_CFG1_L1_RESUME_EN		BIT(11)
#define	CRG_U3DC_CFG1_INACTIVE_PLC_EN	BIT(12)
#define	CRG_U3DC_CFG1_U3_RESUME_NORESP_PLC_EN		BIT(13)
#define	CRG_U3DC_CFG1_U2_RESUME_NORESP_PLC_EN		BIT(14)

#define	CRG_U3DC_CFG1_SETUP_EVENT_EN	BIT(16)

/* ctrl register*/
#define	CRG_U3DC_CTRL_RUN			BIT(0)
#define	CRG_U3DC_CTRL_STOP			(0)
#define	CRG_U3DC_CTRL_SWRST			BIT(1)
#define	CRG_U3DC_CTRL_INT_EN		BIT(2)
#define	CRG_U3DC_CTRL_SYSERR_EN		BIT(3)
#define	CRG_U3DC_CTRL_EWE			BIT(10)
#define	CRG_U3DC_CTRL_KP_CNCT		BIT(11)

/*status register*/
#define CRG_U3DC_STATUS_DEV_CTRL_HALT	BIT(0)
#define CRG_U3DC_STATUS_SYS_ERR			BIT(2)
#define CRG_U3DC_STATUS_EINT			BIT(3)

/*portsc register*/
#define CRG_U3DC_PORTSC_CCS			BIT(0)
#define CRG_U3DC_PORTSC_PED			BIT(1)
#define CRG_U3DC_PORTSC_PP			BIT(3)
#define CRG_U3DC_PORTSC_PR			BIT(4)

#define CRG_U3DC_PORTSC_PLS_SHIFT	(5)
#define CRG_U3DC_PORTSC_PLS_MASK	(0xf << CRG_U3DC_PORTSC_PLS_SHIFT)
#define CRG_U3DC_PORTSC_PLS(fv)		(MAKEF_VAR(CRG_U3DC_PORTSC_PLS, (fv)))
#define CRG_U3DC_PORTSC_PLS_GET(v)	(GETF(CRG_U3DC_PORTSC_PLS, (v)))

#define CRG_U3DC_PORTSC_SPEED_SHIFT	(10)
#define CRG_U3DC_PORTSC_SPEED_MASK	\
		(0xf << CRG_U3DC_PORTSC_SPEED_SHIFT)
#define CRG_U3DC_PORTSC_SPEED(fv)	\
		(MAKEF_VAR(CRG_U3DC_PORTSC_SPEED, (fv)))
#define CRG_U3DC_PORTSC_SPEED_GET(v)	\
		(GETF(CRG_U3DC_PORTSC_SPEED, (v)))
#define CRG_U3DC_PORTSC_SPEED_FS	(0x1)
#define CRG_U3DC_PORTSC_SPEED_LS	(0x2)
#define CRG_U3DC_PORTSC_SPEED_HS	(0x3)
#define CRG_U3DC_PORTSC_SPEED_SS	(0x4)
#define CRG_U3DC_PORTSC_SPEED_SSP	(0x5)

#define CRG_U3DC_PORTSC_LWS			BIT(16)
#define CRG_U3DC_PORTSC_CSC			BIT(17)
#define CRG_U3DC_PORTSC_PEC			BIT(18)
#define CRG_U3DC_PORTSC_PPC			BIT(20)
#define CRG_U3DC_PORTSC_PRC			BIT(21)
#define CRG_U3DC_PORTSC_PLC			BIT(22)
#define CRG_U3DC_PORTSC_CEC			BIT(23)
#define CRG_U3DC_PORTSC_WCE			BIT(25)
#define CRG_U3DC_PORTSC_WDE			BIT(26)
#define CRG_U3DC_PORTSC_WPR			BIT(31)

#define PORTSC_W1C_MASK		(CRG_U3DC_PORTSC_CSC |	\
					CRG_U3DC_PORTSC_PEC |	\
					CRG_U3DC_PORTSC_PPC |	\
					CRG_U3DC_PORTSC_PRC |	\
					CRG_U3DC_PORTSC_PLC |	\
					CRG_U3DC_PORTSC_CEC)
#define PORTSC_WRITE_MASK	(~PORTSC_W1C_MASK)

/* u3portpmsc */
#define CRG_U3DC_U3PORTPM_U1TMOUT_SHIFT		(0)
#define CRG_U3DC_U3PORTPM_U1TMOUT_MASK	\
		(0xff << CRG_U3DC_U3PORTPM_U1TMOUT_SHIFT)
#define CRG_U3DC_U3PORTPM_U1TMOUT(fv)	\
		(MAKEF_VAR(CRG_U3DC_U3PORTPM_U1TMOUT, (fv)))

#define CRG_U3DC_U3PORTPM_U2TMOUT_SHIFT		(8)
#define CRG_U3DC_U3PORTPM_U2TMOUT_MASK	\
		(0xff << CRG_U3DC_U3PORTPM_U2TMOUT_SHIFT)
#define CRG_U3DC_U3PORTPM_U2TMOUT(fv)	\
		(MAKEF_VAR(CRG_U3DC_U3PORTPM_U2TMOUT, (fv)))

#define CRG_U3DC_U3PORTPM_FLA			BIT(16)

#define CRG_U3DC_U3PORTPM_U1IEN_SHIFT		(20)
#define CRG_U3DC_U3PORTPM_U1IEN		\
		(1L << CRG_U3DC_U3PORTPM_U1IEN_SHIFT)

#define CRG_U3DC_U3PORTPM_U2IEN_SHIFT		(21)
#define CRG_U3DC_U3PORTPM_U2IEN		\
		(1L << CRG_U3DC_U3PORTPM_U2IEN_SHIFT)

#define CRG_U3DC_U3PORTPM_U1AEN_SHIFT		(22)
#define CRG_U3DC_U3PORTPM_U1AEN		\
		(1L << CRG_U3DC_U3PORTPM_U1AEN_SHIFT)

#define CRG_U3DC_U3PORTPM_U2AEN_SHIFT		(23)
#define CRG_U3DC_U3PORTPM_U2AEN		\
		(1L << CRG_U3DC_U3PORTPM_U2AEN_SHIFT)

#define CRG_U3DC_U3PORTPM_U1U2TMOUT_SHIFT	(24)
#define CRG_U3DC_U3PORTPM_U1U2TMOUT_MASK \
		(0xff << CRG_U3DC_U3PORTPM_U1U2TMOUT_SHIFT)

/* u2portpmsc */
#define CRG_U3DC_U2PORTPM_RJ_TH_SHIFT		(0)
#define CRG_U3DC_U2PORTPM_RJ_TH_MASK	\
	(0xf << CRG_U3DC_U2PORTPM_RJ_TH_SHIFT)
#define CRG_U3DC_U2PORTPM_RJ_TH(fv)	\
	(MAKEF_VAR(CRG_U3DC_U2PORTPM_RJ_TH, (fv)))

#define CRG_U3DC_U2PORTPM_DS_TH_SHIFT		(4)
#define CRG_U3DC_U2PORTPM_DS_TH_MASK	\
		(0xf << CRG_U3DC_U2PORTPM_DS_TH_SHIFT)
#define CRG_U3DC_U2PORTPM_DS_TH(fv)	\
		(MAKEF_VAR(CRG_U3DC_U2PORTPM_DS_TH, (fv)))

#define CRG_U3DC_U2PORTPM_LPM_EN		BIT(8)
#define CRG_U3DC_U2PORTPM_RJ_TH_EN		BIT(9)
#define CRG_U3DC_U2PORTPM_DS_EN			BIT(10)
#define CRG_U3DC_U2PORTPM_SLP_EN		BIT(11)
#define CRG_U3DC_U2PORTPM_LPM_FACK		BIT(12)
#define CRG_U3DC_U2PORTPM_L1_AEX		BIT(13)
#define CRG_U3DC_U2PORTPM_H_B_SHIFT		(16)
#define CRG_U3DC_U2PORTPM_H_B_MASK	\
		(0xf << CRG_U3DC_U2PORTPM_H_B_SHIFT)
#define CRG_U3DC_U2PORTPM_H_B(fv)	\
		(MAKEF_VAR(CRG_U3DC_U2PORTPM_H_B, (fv)))

#define CRG_U3DC_U2PORTPM_RWE			BIT(20)

#define CRG_U3DC_U2PORTPM_TM_SHIFT		(28)
#define CRG_U3DC_U2PORTPM_TM_MASK	\
		(0xf << CRG_U3DC_U2PORTPM_TM_SHIFT)
#define CRG_U3DC_U2PORTPM_TM(fv)	\
		(MAKEF_VAR(CRG_U3DC_U2PORTPM_TM, (fv)))

/* doorbell register*/
#define CRG_U3DC_DB_TARGET_SHIFT		(0)
#define CRG_U3DC_DB_TARGET_MASK		\
		(0x1f << CRG_U3DC_DB_TARGET_SHIFT)
#define CRG_U3DC_DB_TARGET(fv)		\
		(MAKEF_VAR(CRG_U3DC_DB_TARGET, (fv)))

/* odb registers*/
#define CRG_U3DC_ODBCFG_2N_OFFSET_SHIFT		(0)
#define CRG_U3DC_ODBCFG_2N_OFFSET_MASK		\
		(0x3ff << CRG_U3DC_ODBCFG_2N_OFFSET_SHIFT)
#define CRG_U3DC_ODBCFG_2N_OFFSET(fv)		\
		(MAKEF_VAR(CRG_U3DC_ODBCFG_2N_OFFSET, (fv)))

#define CRG_U3DC_ODBCFG_2N_SIZE_SHIFT		(10)
#define CRG_U3DC_ODBCFG_2N_SIZE_MASK		\
		(0x7 << CRG_U3DC_ODBCFG_2N_SIZE_SHIFT)
#define CRG_U3DC_ODBCFG_2N_SIZE(fv)		\
		(MAKEF_VAR(CRG_U3DC_ODBCFG_2N_SIZE, (fv)))

#define CRG_U3DC_ODBCFG_2N1_OFFSET_SHIFT	(16)
#define CRG_U3DC_ODBCFG_2N1_OFFSET_MASK		\
		(0x3ff << CRG_U3DC_ODBCFG_2N1_OFFSET_SHIFT)
#define CRG_U3DC_ODBCFG_2N1_OFFSET(fv)		\
		(MAKEF_VAR(CRG_U3DC_ODBCFG_2N1_OFFSET, (fv)))

#define CRG_U3DC_ODBCFG_2N1_SIZE_SHIFT		(26)
#define CRG_U3DC_ODBCFG_2N1_SIZE_MASK		\
		(0x7 << CRG_U3DC_ODBCFG_2N1_SIZE_SHIFT)
#define CRG_U3DC_ODBCFG_2N1_SIZE(fv)		\
		(MAKEF_VAR(CRG_U3DC_ODBCFG_2N1_SIZE, (fv)))

/* command control register*/
#define	CRG_U3DC_CMD_CTRL_ACTIVE_SHIFT		(0)
#define	CRG_U3DC_CMD_CTRL_ACTIVE	\
		(1L << CRG_U3DC_CMD_CTRL_ACTIVE_SHIFT)
#define	CRG_U3DC_CMD_CTRL_IOC_SHIFT		(1)
#define	CRG_U3DC_CMD_CTRL_IOC_EN	\
		(1L << CRG_U3DC_CMD_CTRL_IOC_SHIFT)

#define	CRG_U3DC_CMD_CTRL_TYPE_SHIFT		(4)
#define	CRG_U3DC_CMD_CTRL_TYPE_MASK	\
		(0xf << CRG_U3DC_CMD_CTRL_TYPE_SHIFT)
#define CRG_U3DC_CMD_CTRL_TYPE(fv)	\
		(MAKEF_VAR(CRG_U3DC_CMD_CTRL_TYPE, (fv)))

#define	CRG_U3DC_CMD_CTRL_STATUS_SHIFT		(16)
#define	CRG_U3DC_CMD_CTRL_STATUS_MASK	\
		(0xf << CRG_U3DC_CMD_CTRL_STATUS_SHIFT)
#define CRG_U3DC_CMD_CTRL_STATUS(fv)	\
		(MAKEF_VAR(CRG_U3DC_CMD_CTRL_STATUS, (fv)))
#define CRG_U3DC_CMD_CTRL_STATUS_GET(v)	\
		(GETF(CRG_U3DC_CMD_CTRL_STATUS, (v)))

#define	CRG_U3DC_CMD_INIT_EP0			(0L)
#define	CRG_U3DC_CMD_UPDATE_EP0			(1L)
#define	CRG_U3DC_CMD_SET_ADDRESS		(2L)
#define	CRG_U3DC_CMD_SEND_DEV_NOTIFY		(3L)
#define	CRG_U3DC_CMD_CONFIG_EP			(4L)
#define	CRG_U3DC_CMD_SET_HALT			(5L)
#define	CRG_U3DC_CMD_CLR_HALT			(6L)
#define	CRG_U3DC_CMD_RST_SEQNUM			(7L)
#define	CRG_U3DC_CMD_STOP_EP			(8L)
#define	CRG_U3DC_CMD_SET_TR_DQPTR		(9L)
#define	CRG_U3DC_CMD_FORCE_FLOW_CTRL		(10L)
#define	CRG_U3DC_CMD_REQ_LDM_EXCHAG		(11L)

/* int register*/
/* iman bits*/
#define	CRG_U3DC_IMAN_INT_PEND			BIT(0)
#define	CRG_U3DC_IMAN_INT_EN			BIT(1)

/* erdp bits*/
#define CRG_U3DC_ERDPLO_EHB			BIT(3)
#define CRG_U3DC_ERDPLO_ADDRLO(fv)	((fv) & 0xfffffff0)
/**/

/*command params*/
/*command0 init ep0*/
#define CRG_CMD0_0_DQPTRLO_SHIFT	(4)
#define CRG_CMD0_0_DQPTRLO_MASK	\
		(0x0fffffff << CRG_CMD0_0_DQPTRLO_SHIFT)

#define CRG_CMD0_0_DCS_SHIFT		(0)
#define CRG_CMD0_0_DCS_MASK		(0x1 << CRG_CMD0_0_DCS_SHIFT)
#define CRG_CMD0_0_DCS(fv)		(MAKEF_VAR(CRG_CMD0_0_DCS, (fv)))

/*command1 update ep0 */
#define CRG_CMD1_0_MPS_SHIFT		(16)
#define CRG_CMD1_0_MPS_MASK		(0xffff << CRG_CMD1_0_MPS_SHIFT)
#define CRG_CMD1_0_MPS(fv)		(MAKEF_VAR(CRG_CMD1_0_MPS, (fv)))

/*command2 set addr */
#define CRG_CMD2_0_DEV_ADDR_SHIFT	(0)
#define CRG_CMD2_0_DEV_ADDR_MASK	(0xff << CRG_CMD2_0_DEV_ADDR_SHIFT)
#define CRG_CMD2_0_DEV_ADDR(fv)		(MAKEF_VAR(CRG_CMD2_0_DEV_ADDR, (fv)))

/*command type*/
enum crg_cmd_type {
	CRG_CMD_INIT_EP0 = 0,
	CRG_CMD_UPDATE_EP0_CFG = 1,
	CRG_CMD_SET_ADDR = 2,
	CRG_CMD_SEND_DEV_NOTIFICATION = 3,
	CRG_CMD_CONFIG_EP = 4,
	CRG_CMD_SET_HALT = 5,
	CRG_CMD_CLEAR_HALT = 6,
	CRG_CMD_RESET_SEQNUM = 7,
	CRG_CMD_STOP_EP = 8,
	CRG_CMD_SET_TR_DQPTR = 9,
	CRG_CMD_FORCE_FLOW_CONTROL = 10,
	CRG_CMD_REQ_LDM_EXCHANGE = 11
};

#define reg_read readl
#define reg_write(a, v) writel(v, a)

struct crg_gadget_dev;

struct crg_udc_platdata {
	int num_out_eps;
	int num_in_eps;
};

#endif /* __CRG_UDC_H__ */
