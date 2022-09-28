/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

/*******************************************************************/
#include <linux/types.h>

/* RTC_CTRL Bit[8]: 0 - select 32K oscillator, 1 - select 24M oscillator */
#define RTC_OSC_SEL_BIT		(8)
/* RTC_CTRL Bit[12]: 0 - disable rtc, 1 - enable rtc */
#define RTC_ENABLE_BIT		(12)
/* RTC_CTRL Bit[3:0]: 0 - disable rtc alarm, 1 - enable rtc alarm */
#define RTC_ALRM0_EN_BIT	(0)
#define RTC_ALRM1_EN_BIT	(1)
#define RTC_ALRM2_EN_BIT	(2)
#define RTC_ALRM3_EN_BIT	(3)
/* RTC_INT_MASK Bit[3:0]: 0 - enable alarm irq, 1 - disable alarm irq */
#define RTC_ALRM0_IRQ_MSK_BIT	(0)
#define RTC_ALRM1_IRQ_MSK_BIT	(1)
#define RTC_ALRM2_IRQ_MSK_BIT	(2)
#define RTC_ALRM3_IRQ_MSK_BIT	(3)
/* RTC_INT_STATUS Bit[7:4]: 0 - alarm is disable, 1 - alarm is enable */
#define RTC_ALRM0_STATUS_BIT	(4)
#define RTC_ALRM1_STATUS_BIT	(5)
#define RTC_ALRM2_STATUS_BIT	(6)
#define RTC_ALRM3_STATUS_BIT	(7)

/* RTC register address offset */
#define RTC_CTRL		(0)	 /* Control RTC -RW*/
#define RTC_COUNTER_REG		BIT(2)   /* Program RTC counter initial value -RW*/
#define RTC_ALARM0_REG		(2 << 2) /* Program RTC alarm0 value -RW*/
#define RTC_ALARM1_REG		(3 << 2) /* Program RTC alarm1 value -RW*/
#define RTC_ALARM2_REG		(4 << 2) /* Program RTC alarm2 value -RW*/
#define RTC_ALARM3_REG		(5 << 2) /* Program RTC alarm3 value -RW*/
#define RTC_SEC_ADJUST_REG	(6 << 2) /* Control second-based timing adjustment -RW*/
#define RTC_WIDEN_VAL		(7 << 2) /* Cross clock domain widen val -RW*/
#define RTC_INT_MASK		(8 << 2) /* RTC interrupt mask -RW*/
#define RTC_INT_CLR		(9 << 2) /* Clear RTC interrupt -RW*/
#define RTC_OSCIN_CTRL0		(10 << 2)/* Control RTC clk from 24M -RW*/
#define RTC_OSCIN_CTRL1		(11 << 2)/* Control RTC clk from 24M -RW*/
#define RTC_INT_STATUS		(12 << 2)/* RTC interrupt status -R*/
#define RTC_REAL_TIME		(13 << 2)/* RTC counter value -R*/

enum meson_rtc_cmd {
	RTC_CMD_TIME		= 0,
	RTC_CMD_ALARM		= 1,
	RTC_CMD_SUSPEND		= 2,
	RTC_CMD_SHUTDOWN	= 3,
};

enum meson_rtc_adj {
	ADJUST_NONE		= 0,
	ADJUST_DROP		= 1,
	ADJUST_INSERT		= 2,
	ADJUST_MAX		= 3,
};

struct meson_rtc_message {
	enum meson_rtc_cmd  cmd;
	u32  data;
};

struct meson_rtc_data {
	void __iomem *reg_base;
	struct rtc_device *rtc_dev;
	struct meson_rtc_message msg;
	bool find_mboxes;
	bool alarm_enabled;
	struct clk *clock;
	int irq;
	u32 freq;
};
