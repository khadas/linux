/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __AO_CEC_H__
#define __AO_CEC_H__
#include <linux/amlogic/media/vout/hdmi_tx_ext.h>
#include <linux/clk.h>
#include "hdmi_tx_cec_20.h"

#define CEC_DRIVER_VERSION     "2022/04/21: switch to cec_b and use cec_a pin on t7"

#define CEC_DEV_NAME		"cec"

#define CEC_FRAME_DELAY		msecs_to_jiffies(30)
/* delay for framework to receive & handle uevent */
#define CEC_NOTIFY_DELAY	msecs_to_jiffies(10)
#define CEC_CHK_BUS_CNT		20

#define CEC_PHY_PORT_NUM	4
#define HR_DELAY(n)		(ktime_set(0, (n) * 1000 * 1000))

#define MAX_INT				0x7ffffff

/* cec driver function config */
#define CEC_FREEZE_WAKE_UP
#define CEC_MAIL_BOX

enum cec_chip_ver {
	CEC_CHIP_GXL = 0,
	CEC_CHIP_GXM,
	CEC_CHIP_TXL,
	CEC_CHIP_TXLX,
	CEC_CHIP_TXHD,
	CEC_CHIP_G12A,
	CEC_CHIP_G12B,
	CEC_CHIP_SM1,
	CEC_CHIP_TL1,
	CEC_CHIP_TM2,
	CEC_CHIP_A1 = 10,
	CEC_CHIP_SC2,
	CEC_CHIP_T5,	/*only have cecb, first bringup at 4.9-dev*/
	CEC_CHIP_T5D,	/*only have cecb, first bringup at 4.9-dev*/
	CEC_CHIP_T7,
	CEC_CHIP_S4,/*base on sc2*/
	CEC_CHIP_T3,	/* only have cecb */
	CEC_CHIP_T5W,	/* from T5D */
};

enum cecaver {
	CECA_NONE,
	/*
	 * first version, only support one logical addr
	 * "0xf" broadcast addr is default on
	 */
	CECA_VER_0,

	/*
	 * support multi logical address, "0xf" broadcast
	 * addr is default on
	 */
	CECA_VER_1,
};

enum cecbver {
	CECB_NONE,

	/* first version
	 * support multi logical address, "0xf" broadcast
	 * addr is default on
	 */
	CECB_VER_0,
	/*ee to ao */
	CECB_VER_1,
	/*
	 * 1.fix bug: cts 7-1
	 * 2.fix bug: Do not signal initiator error, when it's
	 *   myself who pulled down the line when functioning as a follower
	 * 3.fix bug: Receive messages are ignored and not acknowledge
	 * 4.add status reg
	 */
	CECB_VER_2,
	/*
	 * After and equal A1, register read/write changed
	 */
	CECB_VER_3,
};

/* from android cec hal */
enum {
	HDMI_OPTION_WAKEUP = 1,
	HDMI_OPTION_ENABLE_CEC = 2,
	/*frame work pw on, 1:pw on 0:suspend*/
	HDMI_OPTION_SYSTEM_CEC_CONTROL = 3,
	HDMI_OPTION_SET_LANG = 5,
	/*have cec framework*/
	HDMI_OPTION_SERVICE_FLAG = 16,
};

#define L_1		1
#define L_2		2
#define L_3		3
#define L_4		4

#define CEC_A	0
#define CEC_B	1

#define ENABLE_ONE_CEC	1
#define ENABLE_TWO_CEC	2

#define CEC_B_ARB_TIME 8

#define CEC_MSG_BUFF_MAX	30
#define PHY_ADDR_LEN 4

#define SIGNAL_FREE_TIME_RETRY 3
#define SIGNAL_FREE_TIME_NEW_INITIATOR 5
#define SIGNAL_FREE_TIME_NEXT_XFER 7

struct cec_platform_data_s {
	enum cec_chip_ver chip_id;
	unsigned char line_reg;/*cec gpio_i reg:0  ao;1 periph*/
	unsigned int line_bit;/*cec gpio position in reg*/
	bool ee_to_ao;/*ee cec hw module mv to ao;ao cec delete*/
	bool ceca_sts_reg;/*add new internal status register*/
	enum cecbver cecb_ver;/* detail discription ref enum cecbver */
	enum cecaver ceca_ver;
	bool share_io;
	unsigned int reg_tab_group;
};

struct cec_wakeup_t {
	unsigned int wk_logic_addr:8;
	unsigned int wk_phy_addr:16;
	unsigned int wk_port_id:8;
};

struct dbgflg {
	unsigned int hal_cmd_bypass:1;

};

struct st_rx_msg {
	unsigned char len;
	unsigned char msg[16];
};

struct st_cec_mailbox_data {
	unsigned int cec_config;
	unsigned int phy_addr;
	unsigned int vendor_id;
	unsigned char osd_name[16];
} __packed;

struct vendor_info {
	unsigned char *vendor_name; /* Max Chars: 8 */
	/* vendor_id, 3 Bytes, Refer to
	 * http://standards.ieee.org/develop/regauth/oui/oui.txt
	 */
	unsigned char *product_desc; /* Max Chars: 16 */
	unsigned char *cec_osd_string; /* Max Chars: 14 */
	unsigned int cec_config; /* 4 bytes: use to control cec switch on/off */
	unsigned int vendor_id;
};

/* global struct for tx and rx */
struct ao_cec_dev {
	bool probe_finish;
	unsigned long dev_type;
	struct device_node *node;
	unsigned int port_num;	/*total input hdmi port number*/
	unsigned int cec_num;
	unsigned int arc_port;
	unsigned int output;
	unsigned int hal_flag;
	unsigned int phy_addr;
	unsigned int port_seq;
	unsigned int cpu_type;
	unsigned int irq_ceca;
	unsigned int irq_cecb;
	void __iomem *exit_reg;
	void __iomem *cec_reg;
	void __iomem *hdmi_rxreg;
	void __iomem *hhi_reg;
	void __iomem *periphs_reg;
	void __iomem *clk_reg;
	/* struct hdmitx_dev *tx_dev; */
	struct workqueue_struct *cec_thread;
	struct workqueue_struct *hdmi_plug_wq;
	struct workqueue_struct *cec_tx_event_wq;
	struct workqueue_struct *cec_rx_event_wq;
	struct device *dbg_dev;
	const char *pin_name;
	struct delayed_work cec_work;
	struct delayed_work work_hdmi_plug;
	struct delayed_work work_cec_rx;
	struct completion rx_ok;
	struct completion tx_ok;
	spinlock_t cec_reg_lock;/*cec register access*/
	struct mutex cec_tx_mutex;/*pretect tx cec msg*/
	struct mutex cec_ioctl_mutex;
	struct mutex cec_uevent_mutex; /* cec uevent */
	struct cec_wakeup_t wakup_data;
	/* msg_len + maximum msg len */
	unsigned char cec_wk_otp_msg[17];
	unsigned char cec_wk_as_msg[17];
	unsigned int wakeup_reason;
#ifdef CONFIG_PM
	int cec_suspend;
#endif
	struct vendor_info v_data;
	struct cec_global_info_t cec_info;
	struct cec_platform_data_s *plat_data;

	unsigned int cfg;
	/* unsigned int wakeup_st; */

	unsigned int msg_idx;
	unsigned int msg_num;
	struct st_rx_msg msgbuff[CEC_MSG_BUFF_MAX];

	struct clk *ceca_clk;
	struct clk *cecb_clk;
	bool framework_on;
	bool chk_sig_free_time;
	/* default not enable SW check, for debug */
	bool sw_chk_bus;
};

struct cec_msg_last {
	unsigned char msg[MAX_MSG];
	int len;
	int last_result;
	unsigned long last_jiffies;
};

#define CEC_FUNC_CFG_CEC_ON			0x01
#define CEC_FUNC_CFG_OTP_ON			0x02
#define CEC_FUNC_CFG_AUTO_STANDBY	0x04
#define CEC_FUNC_CFG_AUTO_POWER_ON	0x08
#define CEC_FUNC_CFG_ALL			0x2f
#define CEC_FUNC_CFG_NONE			0x0

#define PREG_PAD_GPIO3_I			(0x01b << 2)

enum cec_reg_group {
	cec_reg_group_old = 0,
	cec_reg_group_a1,
	cec_reg_group_max,
};

enum {
	AO_CEC_CLK_CNTL_REG0 = 0,
	AO_CEC_CLK_CNTL_REG1,	/*1*/
	AO_CEC_GEN_CNTL,	/*2*/
	AO_CEC_RW_REG,
	AO_CEC_INTR_MASKN,
	AO_CEC_INTR_CLR,
	AO_CEC_INTR_STAT,

	AO_CECB_CLK_CNTL_REG0,	/*7*/
	AO_CECB_CLK_CNTL_REG1,	/*8*/
	AO_CECB_GEN_CNTL,	/*9*/
	AO_CECB_RW_REG,		/*0xa*/
	AO_CECB_INTR_MASKN,	/*0xb*/
	AO_CECB_INTR_CLR,	/*0xc*/
	AO_CECB_INTR_STAT,	/*0xd*/

	/* only for old chip CECA clk */
	AO_RTI_STATUS_REG1,
	AO_RTI_PWR_CNTL_REG0,
	AO_CRT_CLK_CNTL1,
	AO_RTC_ALT_CLK_CNTL0,
	AO_RTC_ALT_CLK_CNTL1,

	AO_DEBUG_REG0,
	AO_DEBUG_REG1,
	AO_GPIO_I,

	AO_REG_DEF_END
};

#define REG_MASK_ADDR	0x0000ffff
#define REG_MASK_PR	0x01000000/*periphs register*/

/*
 * AOCEC_A internal register
 * read/write tx register list
 */
#define CEC_TX_MSG_0_HEADER        0x00
#define CEC_TX_MSG_1_OPCODE        0x01
#define CEC_TX_MSG_2_OP1           0x02
#define CEC_TX_MSG_3_OP2           0x03
#define CEC_TX_MSG_4_OP3           0x04
#define CEC_TX_MSG_5_OP4           0x05
#define CEC_TX_MSG_6_OP5           0x06
#define CEC_TX_MSG_7_OP6           0x07
#define CEC_TX_MSG_8_OP7           0x08
#define CEC_TX_MSG_9_OP8           0x09
#define CEC_TX_MSG_A_OP9           0x0A
#define CEC_TX_MSG_B_OP10          0x0B
#define CEC_TX_MSG_C_OP11          0x0C
#define CEC_TX_MSG_D_OP12          0x0D
#define CEC_TX_MSG_E_OP13          0x0E
#define CEC_TX_MSG_F_OP14          0x0F
#define CEC_TX_MSG_LENGTH          0x10
#define CEC_TX_MSG_CMD             0x11
#define CEC_TX_WRITE_BUF           0x12
#define CEC_TX_CLEAR_BUF           0x13
#define CEC_RX_MSG_CMD             0x14
#define CEC_RX_CLEAR_BUF           0x15
#define CEC_LOGICAL_ADDR0          0x16
#define CEC_LOGICAL_ADDR1          0x17/*ADDR L tm2 later*/
#define CEC_LOGICAL_ADDR2          0x18/*ADDR H tm2 later*/
#define CEC_LOGICAL_ADDR3          0x19
#define CEC_LOGICAL_ADDR4          0x1A
#define CEC_CLOCK_DIV_H            0x1B
#define CEC_CLOCK_DIV_L            0x1C

/*
 * AOCEC_A internal register
 * The following registers are for fine tuning CEC bit timing parameters.
 * They are only valid in AO CEC, NOT valid in HDMITX CEC.
 * The AO CEC's timing parameters are already set default to work with
 * 32768Hz clock, so hopefully SW never need to program these registers.
 * The timing registers are made programmable just in case.
 */
#define AO_CEC_QUIESCENT_25MS_BIT7_0            0x20
#define AO_CEC_QUIESCENT_25MS_BIT11_8           0x21
#define AO_CEC_STARTBITMINL2H_3MS5_BIT7_0       0x22
#define AO_CEC_STARTBITMINL2H_3MS5_BIT8         0x23
#define AO_CEC_STARTBITMAXL2H_3MS9_BIT7_0       0x24
#define AO_CEC_STARTBITMAXL2H_3MS9_BIT8         0x25
#define AO_CEC_STARTBITMINH_0MS6_BIT7_0         0x26
#define AO_CEC_STARTBITMINH_0MS6_BIT8           0x27
#define AO_CEC_STARTBITMAXH_1MS0_BIT7_0         0x28
#define AO_CEC_STARTBITMAXH_1MS0_BIT8           0x29
#define AO_CEC_STARTBITMINTOTAL_4MS3_BIT7_0     0x2A
#define AO_CEC_STARTBITMINTOTAL_4MS3_BIT9_8     0x2B
#define AO_CEC_STARTBITMAXTOTAL_4MS7_BIT7_0     0x2C
#define AO_CEC_STARTBITMAXTOTAL_4MS7_BIT9_8     0x2D
#define AO_CEC_LOGIC1MINL2H_0MS4_BIT7_0         0x2E
#define AO_CEC_LOGIC1MINL2H_0MS4_BIT8           0x2F
#define AO_CEC_LOGIC1MAXL2H_0MS8_BIT7_0         0x30
#define AO_CEC_LOGIC1MAXL2H_0MS8_BIT8           0x31
#define AO_CEC_LOGIC0MINL2H_1MS3_BIT7_0         0x32
#define AO_CEC_LOGIC0MINL2H_1MS3_BIT8           0x33
#define AO_CEC_LOGIC0MAXL2H_1MS7_BIT7_0         0x34
#define AO_CEC_LOGIC0MAXL2H_1MS7_BIT8           0x35
#define AO_CEC_LOGICMINTOTAL_2MS05_BIT7_0       0x36
#define AO_CEC_LOGICMINTOTAL_2MS05_BIT9_8       0x37
#define AO_CEC_LOGICMAXHIGH_2MS8_BIT7_0         0x38
#define AO_CEC_LOGICMAXHIGH_2MS8_BIT8           0x39
#define AO_CEC_LOGICERRLOW_3MS4_BIT7_0          0x3A
#define AO_CEC_LOGICERRLOW_3MS4_BIT8            0x3B
#define AO_CEC_NOMSMPPOINT_1MS05                0x3C
#define AO_CEC_DELCNTR_LOGICERR                 0x3E
#define AO_CEC_TXTIME_17MS_BIT7_0               0x40
#define AO_CEC_TXTIME_17MS_BIT10_8              0x41
#define AO_CEC_TXTIME_2BIT_BIT7_0               0x42
#define AO_CEC_TXTIME_2BIT_BIT10_8              0x43
#define AO_CEC_TXTIME_4BIT_BIT7_0               0x44
#define AO_CEC_TXTIME_4BIT_BIT10_8              0x45
#define AO_CEC_STARTBITNOML2H_3MS7_BIT7_0       0x46
#define AO_CEC_STARTBITNOML2H_3MS7_BIT8         0x47
#define AO_CEC_STARTBITNOMH_0MS8_BIT7_0         0x48
#define AO_CEC_STARTBITNOMH_0MS8_BIT8           0x49
#define AO_CEC_LOGIC1NOML2H_0MS6_BIT7_0         0x4A
#define AO_CEC_LOGIC1NOML2H_0MS6_BIT8           0x4B
#define AO_CEC_LOGIC0NOML2H_1MS5_BIT7_0         0x4C
#define AO_CEC_LOGIC0NOML2H_1MS5_BIT8           0x4D
#define AO_CEC_LOGIC1NOMH_1MS8_BIT7_0           0x4E
#define AO_CEC_LOGIC1NOMH_1MS8_BIT8             0x4F
#define AO_CEC_LOGIC0NOMH_0MS9_BIT7_0           0x50
#define AO_CEC_LOGIC0NOMH_0MS9_BIT8             0x51
#define AO_CEC_LOGICERRLOW_3MS6_BIT7_0          0x52
#define AO_CEC_LOGICERRLOW_3MS6_BIT8            0x53
#define AO_CEC_CHKCONTENTION_0MS1               0x54
#define AO_CEC_PREPARENXTBIT_0MS05_BIT7_0       0x56
#define AO_CEC_PREPARENXTBIT_0MS05_BIT8         0x57
#define AO_CEC_NOMSMPACKPOINT_0MS45             0x58
#define AO_CEC_ACK0NOML2H_1MS5_BIT7_0           0x5A
#define AO_CEC_ACK0NOML2H_1MS5_BIT8             0x5B
#define AO_CEC_BUGFIX_DISABLE_0                 0x60
#define AO_CEC_BUGFIX_DISABLE_1                 0x61

/*
 * AOCEC_A internal register
 * read only register list
 */
#define CEC_RX_MSG_0_HEADER        0x80
#define CEC_RX_MSG_1_OPCODE        0x81
#define CEC_RX_MSG_2_OP1           0x82
#define CEC_RX_MSG_3_OP2           0x83
#define CEC_RX_MSG_4_OP3           0x84
#define CEC_RX_MSG_5_OP4           0x85
#define CEC_RX_MSG_6_OP5           0x86
#define CEC_RX_MSG_7_OP6           0x87
#define CEC_RX_MSG_8_OP7           0x88
#define CEC_RX_MSG_9_OP8           0x89
#define CEC_RX_MSG_A_OP9           0x8A
#define CEC_RX_MSG_B_OP10          0x8B
#define CEC_RX_MSG_C_OP11          0x8C
#define CEC_RX_MSG_D_OP12          0x8D
#define CEC_RX_MSG_E_OP13          0x8E
#define CEC_RX_MSG_F_OP14          0x8F
#define CEC_RX_MSG_LENGTH          0x90
#define CEC_RX_MSG_STATUS          0x91
#define CEC_RX_NUM_MSG             0x92
#define CEC_TX_MSG_STATUS          0x93
#define CEC_TX_NUM_MSG             0x94
/*
 * AOCEC_A internal register
 * read only (tl1 later)
 */
#define CEC_STAT_0_0				0xA0
#define CEC_STAT_0_1				0xA1
#define CEC_STAT_0_2				0xA2
#define CEC_STAT_0_3				0xA3
#define CEC_STAT_1_0				0xA4
#define CEC_STAT_1_1				0xA5
#define CEC_STAT_1_2				0xA6

/* tx_msg_cmd definition */
#define TX_NO_OP                0  /* No transaction */
#define TX_REQ_CURRENT          1  /* Transmit earliest message in buffer */
#define TX_ABORT                2  /* Abort transmitting earliest message */
/* Overwrite earliest message in buffer and transmit next message */
#define TX_REQ_NEXT             3

/* tx_msg_status definition */
#define TX_IDLE                 0  /* No transaction */
#define TX_BUSY                 1  /* Transmitter is busy */
/* Message has been successfully transmitted */
#define TX_DONE                 2
#define TX_ERROR                3  /* Message has been transmitted with error */

/* rx_msg_cmd */
#define RX_NO_OP                0  /* No transaction */
#define RX_ACK_CURRENT          1  /* Read earliest message in buffer */
#define RX_DISABLE              2  /* Disable receiving latest message */
/* Clear earliest message from buffer and read next message */
#define RX_ACK_NEXT             3

/* rx_msg_status */
#define RX_IDLE                 0  /* No transaction */
#define RX_BUSY                 1  /* Receiver is busy */
#define RX_DONE                 2  /* Message has been received successfully */
#define RX_ERROR                3  /* Message has been received with error */

/*  AO CECA INT flag */
#define AO_CEC_HOST_ACK			0x01
#define AO_CEC_TX_INT			0x02
#define AO_CEC_RX_INT			0x04

#define TOP_HPD_PWR5V           0x002
#define TOP_ARCTX_CNTL          0x010
#define TOP_CLK_CNTL			0x001
#define TOP_EDID_GEN_CNTL		0x004
#define TOP_EDID_ADDR_CEC		0x005

/** Register address: audio clock interrupt clear enable */
#define DWC_AUD_CEC_IEN_CLR		(0xF90UL)
/** Register address: audio clock interrupt set enable */
#define DWC_AUD_CEC_IEN_SET		(0xF94UL)
/** Register address: audio clock interrupt status */
#define DWC_AUD_CEC_ISTS		(0xF98UL)
/** Register address: audio clock interrupt enable */
#define DWC_AUD_CEC_IEN			(0xF9CUL)
/** Register address: audio clock interrupt clear status */
#define DWC_AUD_CEC_ICLR		(0xFA0UL)
/** Register address: audio clock interrupt set status */
#define DWC_AUD_CEC_ISET		(0xFA4UL)
/** Register address: DMI disable interface */
#define DWC_DMI_DISABLE_IF		(0xFF4UL)

/*
 * AOCEC_B internal register
 * for EE CEC
 */
#define DWC_CEC_CTRL                     0x1F00
#define DWC_CEC_CTRL2                    0x1F04/*tl1 later*/
#define DWC_CEC_MASK                     0x1F08
#define DWC_CEC_POLARITY                 0x1F0C
#define DWC_CEC_INT                      0x1F10
#define DWC_CEC_ADDR_L                   0x1F14
#define DWC_CEC_ADDR_H                   0x1F18
#define DWC_CEC_TX_CNT                   0x1F1C
#define DWC_CEC_RX_CNT                   0x1F20
#define DWC_CEC_STAT0                    0x1F24/*tl1 later*/
#define DWC_CEC_TX_DATA0                 0x1F40
#define DWC_CEC_TX_DATA1                 0x1F44
#define DWC_CEC_TX_DATA2                 0x1F48
#define DWC_CEC_TX_DATA3                 0x1F4C
#define DWC_CEC_TX_DATA4                 0x1F50
#define DWC_CEC_TX_DATA5                 0x1F54
#define DWC_CEC_TX_DATA6                 0x1F58
#define DWC_CEC_TX_DATA7                 0x1F5C
#define DWC_CEC_TX_DATA8                 0x1F60
#define DWC_CEC_TX_DATA9                 0x1F64
#define DWC_CEC_TX_DATA10                0x1F68
#define DWC_CEC_TX_DATA11                0x1F6C
#define DWC_CEC_TX_DATA12                0x1F70
#define DWC_CEC_TX_DATA13                0x1F74
#define DWC_CEC_TX_DATA14                0x1F78
#define DWC_CEC_TX_DATA15                0x1F7C
#define DWC_CEC_RX_DATA0                 0x1F80
#define DWC_CEC_RX_DATA1                 0x1F84
#define DWC_CEC_RX_DATA2                 0x1F88
#define DWC_CEC_RX_DATA3                 0x1F8C
#define DWC_CEC_RX_DATA4                 0x1F90
#define DWC_CEC_RX_DATA5                 0x1F94
#define DWC_CEC_RX_DATA6                 0x1F98
#define DWC_CEC_RX_DATA7                 0x1F9C
#define DWC_CEC_RX_DATA8                 0x1FA0
#define DWC_CEC_RX_DATA9                 0x1FA4
#define DWC_CEC_RX_DATA10                0x1FA8
#define DWC_CEC_RX_DATA11                0x1FAC
#define DWC_CEC_RX_DATA12                0x1FB0
#define DWC_CEC_RX_DATA13                0x1FB4
#define DWC_CEC_RX_DATA14                0x1FB8
#define DWC_CEC_RX_DATA15                0x1FBC
#define DWC_CEC_LOCK                     0x1FC0
#define DWC_CEC_WKUPCTRL                 0x1FC4

/*
 * AOCEC B CEC_STAT0
 */
enum {
	CECB_STAT0_S2P_IDLE = 0,
	CECB_STAT0_S2P_SBITLOWER = 1,
	CECB_STAT0_S2P_SBH = 2,
	CECB_STAT0_S2P_L1LOWER = 5,
	CECB_STAT0_S2P_SMP1 = 6,
	CECB_STAT0_S2P_SMP0 = 7,
	CECB_STAT0_S2P_L0H = 8,
	CECB_STAT0_S2P_ERRLMIN = 9,
	CECB_STAT0_S2P_ERRLMAX = 0xe,
};

enum {
	CECB_STAT0_P2S_TIDLE = 0,
	CECB_STAT0_P2S_SEND_SBIT = 1,
	CECB_STAT0_P2S_SEND_DBIT = 2,
	CECB_STAT0_P2S_SEND_EOM = 3,
	CECB_STAT0_P2S_SEND_ACK = 4,
	CECB_STAT0_P2S_FBACK_ACK = 5,
	CECB_STAT0_P2S_FBACK_RX_ERR = 6,
};

/* suspend uevent(only for deep suspend)
 * under early suspend, cec msg is handled by framework
 */
#define CEC_SUSPEND 0
#define CEC_WAKEUP_NORMAL 1
/* wake up by CEC, one touch play or
 * active source msg received from uboot
 */
#define CEC_WAKEUP_OTP 2
#define CEC_WAKEUP_AS 3

enum cec_event_type {
	CEC_NONE_EVENT = 0,
	HDMI_PLUG_EVENT = 1,
	CEC_RX_MSG = 2,
	CEC_PWR_UEVENT = 3
};

#define MAX_UEVENT_LEN 64

struct cec_uevent {
	enum cec_event_type type;
	unsigned int state;
	const char *env;
};

/* cec ip irq flags bit discription */
#define EECEC_IRQ_TX_DONE		BIT(16)
#define EECEC_IRQ_RX_EOM		BIT(17)
#define EECEC_IRQ_TX_NACK		BIT(18)
#define EECEC_IRQ_TX_ARB_LOST		BIT(19)
#define EECEC_IRQ_TX_ERR_INITIATOR	BIT(20)
#define EECEC_IRQ_RX_ERR_FOLLOWER	BIT(21)
#define EECEC_IRQ_RX_WAKEUP		BIT(22)
#define EE_CEC_IRQ_EN_MASK			(0x3f00)

/* cec irq bit flags for AO_CEC_B */
#define CECB_IRQ_TX_DONE		BIT(0)
#define CECB_IRQ_RX_EOM			BIT(1)
#define CECB_IRQ_TX_NACK		BIT(2)
#define CECB_IRQ_TX_ARB_LOST		BIT(3)
#define CECB_IRQ_TX_ERR_INITIATOR	BIT(4)
#define CECB_IRQ_RX_ERR_FOLLOWER	BIT(5)
#define CECB_IRQ_RX_WAKEUP		BIT(6)
#define CECB_IRQ_EN_MASK			(0x3f)

/* common mask */
#define CEC_IRQ_TX_DONE			(1 << (16 - shift))
#define CEC_IRQ_RX_EOM			(1 << (17 - shift))
#define CEC_IRQ_TX_NACK			(1 << (18 - shift))
#define CEC_IRQ_TX_ARB_LOST		(1 << (19 - shift))
#define CEC_IRQ_TX_ERR_INITIATOR	(1 << (20 - shift))
#define CEC_IRQ_RX_ERR_FOLLOWER		(1 << (21 - shift))
#define CEC_IRQ_RX_WAKEUP		(1 << (22 - shift))

/* wakeup mask */
#define WAKEUP_OP_86_EN			BIT(7)
#define WAKEUP_OP_82_EN			BIT(6)
#define WAKEUP_OP_70_EN			BIT(5)
#define WAKEUP_OP_44_EN			BIT(4)
#define WAKEUP_OP_42_EN			BIT(3)
#define WAKEUP_OP_41_EN			BIT(2)
#define WAKEUP_OP_0D_EN			BIT(1)
#define WAKEUP_OP_04_EN			BIT(0)
#define WAKEUP_DIS_MASK			0
#define WAKEUP_EN_MASK			(WAKEUP_OP_86_EN | \
					 WAKEUP_OP_0D_EN | \
					 WAKEUP_OP_04_EN)

#define EDID_CEC_ID_ADDR		0x00a100a0
#define EDID_AUTO_CEC_EN		0

#define HHI_32K_CLK_CNTL		(0x89 << 2)
#define HHI_HDMIRX_ARC_CNTL		(0xe8 << 2)

#define CEC_IOC_MAGIC                   'C'
#define CEC_IOC_GET_PHYSICAL_ADDR       _IOR(CEC_IOC_MAGIC, 0x00, uint16_t)
#define CEC_IOC_GET_VERSION             _IOR(CEC_IOC_MAGIC, 0x01, int)
#define CEC_IOC_GET_VENDOR_ID           _IOR(CEC_IOC_MAGIC, 0x02, uint32_t)
#define CEC_IOC_GET_PORT_INFO           _IOR(CEC_IOC_MAGIC, 0x03, int)
#define CEC_IOC_GET_PORT_NUM            _IOR(CEC_IOC_MAGIC, 0x04, int)
#define CEC_IOC_GET_SEND_FAIL_REASON    _IOR(CEC_IOC_MAGIC, 0x05, uint32_t)
#define CEC_IOC_SET_OPTION_WAKEUP       _IOW(CEC_IOC_MAGIC, 0x06, uint32_t)
#define CEC_IOC_SET_OPTION_ENALBE_CEC   _IOW(CEC_IOC_MAGIC, 0x07, uint32_t)
#define CEC_IOC_SET_OPTION_SYS_CTRL     _IOW(CEC_IOC_MAGIC, 0x08, uint32_t)
#define CEC_IOC_SET_OPTION_SET_LANG     _IOW(CEC_IOC_MAGIC, 0x09, uint32_t)
#define CEC_IOC_GET_CONNECT_STATUS      _IOR(CEC_IOC_MAGIC, 0x0A, uint32_t)
#define CEC_IOC_ADD_LOGICAL_ADDR        _IOW(CEC_IOC_MAGIC, 0x0B, uint32_t)
#define CEC_IOC_CLR_LOGICAL_ADDR        _IOW(CEC_IOC_MAGIC, 0x0C, uint32_t)
#define CEC_IOC_SET_DEV_TYPE            _IOW(CEC_IOC_MAGIC, 0x0D, uint32_t)
#define CEC_IOC_SET_ARC_ENABLE          _IOW(CEC_IOC_MAGIC, 0x0E, uint32_t)
#define CEC_IOC_SET_AUTO_DEVICE_OFF     _IOW(CEC_IOC_MAGIC, 0x0F, uint32_t)
#define CEC_IOC_GET_BOOT_ADDR           _IOW(CEC_IOC_MAGIC, 0x10, uint32_t)
#define CEC_IOC_GET_BOOT_REASON         _IOW(CEC_IOC_MAGIC, 0x11, uint32_t)
#define CEC_IOC_SET_FREEZE_MODE         _IOW(CEC_IOC_MAGIC, 0x12, uint32_t)
#define CEC_IOC_GET_BOOT_PORT           _IOW(CEC_IOC_MAGIC, 0x13, uint32_t)
#define CEC_IOC_SET_DEBUG_EN		_IOW(CEC_IOC_MAGIC, 0x14, uint32_t)
#define CEC_IOC_GET_WK_OTP_MSG	_IOR(CEC_IOC_MAGIC, 0x17, struct st_rx_msg)
#define CEC_IOC_GET_WK_AS_MSG	_IOR(CEC_IOC_MAGIC, 0x18, struct st_rx_msg)
#define CEC_IOC_KEY_EVENT _IOW(CEC_IOC_MAGIC, 0x15, uint32_t)

#ifdef CONFIG_AMLOGIC_MEDIA_TVIN_HDMI
unsigned long hdmirx_rd_top(unsigned long addr);
void hdmirx_wr_top(unsigned long addr, unsigned long data);
uint32_t hdmirx_rd_dwc(u16 addr);
void hdmirx_wr_dwc(u16 addr, u32 data);
unsigned int rd_reg_hhi(unsigned int offset);
void wr_reg_hhi(unsigned int offset, unsigned int val);
int __attribute__((weak))cec_set_dev_info(uint8_t dev_idx);
#else

static inline unsigned long hdmirx_rd_top(unsigned long addr)
{
	return 0;
}

static inline void hdmirx_wr_top(unsigned long addr, unsigned long data)
{
}

static inline uint32_t hdmirx_rd_dwc(u16 addr)
{
	return 0;
}

static inline void hdmirx_wr_dwc(u16 addr, u16 data)
{
}

unsigned int rd_reg_hhi(u32 offset)
{
	return 0;
}

void wr_reg_hhi(unsigned int offset, unsigned int val)
{
}

#endif
void cecb_irq_handle(void);
int cec_ll_tx(const unsigned char *msg, unsigned char len, unsigned char signal_free_time);

#endif	/* __AO_CEC_H__ */
