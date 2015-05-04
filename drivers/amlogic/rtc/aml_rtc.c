/*
 * drivers/amlogic/rtc/aml_rtc.c
 *
 * Copyright (C) 2015 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
*/


#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/rtc.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/workqueue.h>
#include <linux/reset.h>
#include <linux/amlogic/cpu_version.h>

int c_dbg_lvl = 0;
#define RTC_DBG(lvl, x...) do { if (c_dbg_lvl & lvl) pr_info(x); } while (0)
#define RTC_DBG_VAL (1 << 0)
#define RTC_DBG_WR (1 << 1)

/* RTC registers */
#define AO_RTC_ADDR0    0x00
#define AO_RTC_ADDR1    0x04
#define AO_RTC_ADDR2    0x08
#define AO_RTC_ADDR3    0x0c
#define AO_RTC_REG4     0x10

/* Define register AO_RTC_ADDR0 bit map */
#define RTC_REG0_BIT_sclk_static     20
#define RTC_REG0_BIT_ildo_ctrl_1      7
#define RTC_REG0_BIT_ildo_ctrl_0      6
#define RTC_REG0_BIT_test_mode	5
#define RTC_REG0_BIT_test_clk         4
#define RTC_REG0_BIT_test_bypass	3
#define RTC_REG0_BIT_sdi              2
#define RTC_REG0_BIT_sen              1
#define RTC_REG0_BIT_sclk             0

/* Define register AO_RTC_ADDR1 bit map */
#define RTC_REG1_BIT_gpo_to_dig	3
#define RTC_REG1_BIT_gpi_to_dig	2
#define RTC_REG1_BIT_s_ready          1
#define RTC_REG1_BIT_sdo              0

/* Define register AO_RTC_ADDR3 bit map */
#define RTC_REG3_BIT_count_always	17

/* Define RTC serial protocal */
#define RTC_SER_DATA_BITS		32
#define RTC_SER_ADDR_BITS		3


#define s_ready					(1 << RTC_REG1_BIT_s_ready)
#define s_do				(1 << RTC_REG1_BIT_sdo)
#define RESET_RETRY_TIMES		3

static void __iomem	*rtc_base;
static void __iomem *ao_ctrl_reg; /* AO_RTI_GEN_CNTL_REG0 */
/*
#if MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON6
#if (defined(CONFIG_MESON_TRUSTZONE) && defined(CONFIG_ARCH_MESON6))
#include <mach/meson-secure.h>
#define WR_RTC(addr, data)         meson_secure_reg_write(P_##addr, data)
#define RD_RTC(addr)               meson_secure_reg_read(P_##addr)
#else
*/
/*only support M8M2*/
#define WR_RTC(addr, data)         writel(data, rtc_base + (addr))
#define RD_RTC(addr)               readl(rtc_base + (addr))
/*
#endif
#else
#define WR_RTC(addr, data)         WRITE_AOBUS_REG(addr, data)
#define RD_RTC(addr)                   READ_AOBUS_REG(addr)
#endif
*/
#define RTC_sbus_LOW(x)			WR_RTC(AO_RTC_ADDR0, \
		(RD_RTC(AO_RTC_ADDR0) & ~((1<<RTC_REG0_BIT_sen)| \
		(1<<RTC_REG0_BIT_sclk) | (1<<RTC_REG0_BIT_sdi))))

#define RTC_sdi_HIGH(x)			WR_RTC(AO_RTC_ADDR0, \
		(RD_RTC(AO_RTC_ADDR0) | (1<<RTC_REG0_BIT_sdi)))

#define RTC_sdi_LOW(x)			WR_RTC(AO_RTC_ADDR0, \
		(RD_RTC(AO_RTC_ADDR0) & ~(1<<RTC_REG0_BIT_sdi)))

#define RTC_sen_HIGH(x)			WR_RTC(AO_RTC_ADDR0, \
		(RD_RTC(AO_RTC_ADDR0) | (1<<RTC_REG0_BIT_sen)))

#define RTC_sen_LOW(x)			WR_RTC(AO_RTC_ADDR0, \
		(RD_RTC(AO_RTC_ADDR0) & ~(1<<RTC_REG0_BIT_sen)))

#define RTC_sclk_HIGH(x)		WR_RTC(AO_RTC_ADDR0, \
		(RD_RTC(AO_RTC_ADDR0) | (1<<RTC_REG0_BIT_sclk)))

#define RTC_sclk_LOW(x)			WR_RTC(AO_RTC_ADDR0, \
		(RD_RTC(AO_RTC_ADDR0) & ~(1<<RTC_REG0_BIT_sclk)))

#define RTC_sdo_READBIT			(RD_RTC(AO_RTC_ADDR1) \
		&(1<<RTC_REG1_BIT_sdo))

#define RTC_sclk_static_HIGH(x) WR_RTC(AO_RTC_ADDR0, \
		(RD_RTC(AO_RTC_ADDR0) | (1<<RTC_REG0_BIT_sclk_static)))

#define RTC_sclk_static_LOW(x)  WR_RTC(AO_RTC_ADDR0, \
		(RD_RTC(AO_RTC_ADDR0) & ~(1<<RTC_REG0_BIT_sclk_static)))

#define RTC_count_always_HIGH(x)  WR_RTC(AO_RTC_ADDR3, \
		(RD_RTC(AO_RTC_ADDR3) | (1<<RTC_REG3_BIT_count_always)))
#define RTC_count_always_LOW(x)   WR_RTC(AO_RTC_ADDR3, \
		(RD_RTC(AO_RTC_ADDR3) & ~(1<<RTC_REG3_BIT_count_always)))

/* #define RTC_SER_REG_DATA_NOTIFIER   0xb41b*/
/*Define RTC register address mapping */

/* #define P_ISA_TIMERE		(volatile unsigned long *)0xc1109954 */

/* Define RTC register address mapping */
#define RTC_COUNTER_ADDR            0
#define RTC_GPO_COUNTER_ADDR        1
#define RTC_SEC_ADJUST_ADDR         2
#define RTC_UNUSED_ADDR_0           3
#define RTC_REGMEM_ADDR_0           4
#define RTC_REGMEM_ADDR_1           5
#define RTC_REGMEM_ADDR_2           6
#define RTC_REGMEM_ADDR_3           7

static int  check_osc_clk(void);

int get_rtc_status(void)
{
	static int rtc_fail = -1;
		if (check_osc_clk() < 0) {
			pr_info("rtc clock error\n");
			rtc_fail = 1;
		} else
			rtc_fail = 0;
	return rtc_fail;
}
static DEFINE_SPINLOCK(com_lock);

struct aml_rtc_priv {
	struct	rtc_device *rtc;
	unsigned long base_addr;
	struct timer_list timer;
	struct work_struct work;
	struct workqueue_struct *rtc_work_queue;
};

static void reset_gpo_work(struct work_struct *work);
static int get_gpo_flag(void);

static void delay_us(int us)
{
	udelay(us);
}

static void rtc_comm_delay(void)
{
	delay_us(5);
}

static void rtc_sclk_pulse(void)
{
	/* local_irq_save(flags); */
	rtc_comm_delay();
	RTC_sclk_HIGH(1);
	rtc_comm_delay();
	RTC_sclk_LOW(0);
	/* local_irq_restore(flags); */
}

static int  check_osc_clk(void)
{
	unsigned long   osc_clk_count1;
	unsigned long   osc_clk_count2;

	/* Enable count always */
	WR_RTC(AO_RTC_ADDR3, RD_RTC(AO_RTC_ADDR3) | (1 << 17));

	RTC_DBG(RTC_DBG_VAL, "aml_rtc -- check os clk_1\n");
	/* Wait for 50uS.  32.768khz is 30.5uS.  This should be long
	   enough for one full cycle of 32.768 khz
	*/
	osc_clk_count1 = RD_RTC(AO_RTC_ADDR2);

	RTC_DBG(RTC_DBG_VAL, "the aml_rtc os clk 1 is %d\n",
				(unsigned int)osc_clk_count1);
	delay_us(50);
	osc_clk_count2 = RD_RTC(AO_RTC_ADDR2);
	RTC_DBG(RTC_DBG_VAL, "the aml_rtc os clk 2 is %d\n",
				(unsigned int)osc_clk_count2);

	RTC_DBG(RTC_DBG_VAL, "aml_rtc -- check os clk_2\n");
	/* disable count always */
	WR_RTC(AO_RTC_ADDR3, RD_RTC(AO_RTC_ADDR3) & ~(1 << 17));

	if (osc_clk_count1 == osc_clk_count2) {
		RTC_DBG(RTC_DBG_VAL,
		"The osc_clk is not running now! need to invcrease the power!\n");
		return -1;
	}

	RTC_DBG(RTC_DBG_VAL, "aml_rtc : check_os_clk\n");

	return 0;

}

void rtc_ser_static_write_auto(unsigned long static_reg_data_in)
{
	unsigned long data32;

	/* Program MSB 15-8 */
	data32  = (static_reg_data_in >> 8) & 0xff;
	WR_RTC(AO_RTC_REG4, data32);
	/* Program LSB 7-0, and start serializing */
	data32  = RD_RTC(AO_RTC_ADDR0);
	data32 |= 1                           << 17; /* auto_serialize_start */
	data32 &= ~(0xff << 24);
	data32 |= (static_reg_data_in & 0xff) << 24; /* auto_static_reg */
	WR_RTC(AO_RTC_ADDR0, data32);
	/* Poll auto_serializer_busy bit until it's low (IDLE) */
	while ((RD_RTC(AO_RTC_ADDR0)) & 1<<22)
		;
}


static void rtc_reset_s_ready(void)
{
	/* RTC_RESET_BIT_HIGH(1); */
	delay_us(100);
	return;
}

static int rtc_wait_s_ready(void)
{
	int i = 40000;
	int try_cnt = 0;
	if (get_rtc_status())
		return i;
	/*
	while (i--) {
		if ((*(volatile unsigned *)AO_RTC_ADDR1)&s_ready)
			break;
		}
	return i;
	*/
	while (!(RD_RTC(AO_RTC_ADDR1)&s_ready)) {
		i--;
		if (i == 0) {
			if (try_cnt > RESET_RETRY_TIMES)
				break;
			rtc_reset_s_ready();
			try_cnt++;
			i = 40000;
		}
	}

	return i;
}


static int rtc_comm_init(void)
{
	RTC_sbus_LOW(0);
	if (rtc_wait_s_ready() > 0) {
		RTC_sen_HIGH(1);
		return 0;
	}
	return -1;
}


static void rtc_send_bit(unsigned val)
{
	if (val)
		RTC_sdi_HIGH(1);
	else
		RTC_sdi_LOW(0);
	rtc_sclk_pulse();
}

static void rtc_send_addr_data(unsigned type, unsigned val)
{
	unsigned cursor = (type ? (1<<(RTC_SER_ADDR_BITS-1))
					 : (1<<(RTC_SER_DATA_BITS-1)));

	while (cursor) {
		rtc_send_bit(val&cursor);
		cursor >>= 1;
	}
}

static void rtc_get_data(unsigned *val)
{
	int i;
	RTC_DBG(RTC_DBG_VAL, "rtc-aml -- rtc get data\n");
	for (i = 0; i < RTC_SER_DATA_BITS; i++) {
		rtc_sclk_pulse();
		*val <<= 1;
		*val  |= RTC_sdo_READBIT;
	}
}

static void rtc_set_mode(unsigned mode)
{
	RTC_sen_LOW(0);
	if (mode)
		RTC_sdi_HIGH(1);/* WRITE */
	else
		RTC_sdi_LOW(0);  /* READ */
		rtc_sclk_pulse();
		RTC_sdi_LOW(0);
}

static void static_register_write(unsigned data);
static void	 _ser_access_write_locked(unsigned long addr,
		unsigned long data);
static void aml_rtc_reset(void)
{
	if (get_rtc_status())
		return;
	pr_err("error, the rtc serial communication abnormal,reset the rtc!\n");

/*if (get_cpu_type() > MESON_CPU_MAJOR_ID_M6){*/
	writel((readl(ao_ctrl_reg)|1<<21), ao_ctrl_reg);
	udelay(5);
	writel((readl(ao_ctrl_reg)&(~(1<<21))), ao_ctrl_reg);
	static_register_write(0x180a);
	_ser_access_write_locked(RTC_GPO_COUNTER_ADDR, 0x500000);
/*}else
	WRITE_CBUS_REG(RESET3_REGISTER, 0x1<<3);
*/
}

/*#if MESON_CPU_TYPE == MESON_CPU_TYPE_MESON8B
extern int run_arc_program(void);
extern int stop_ao_cpu(void);
#endif*/

static unsigned int _ser_access_read_locked(unsigned long addr)
{
	unsigned val = 0;
	int s_nrdy_cnt = 0;
	int rst_times = 0;
/*	int ret = 0;*/

	if (get_rtc_status())
		return 0;
/*	if (is_meson_m8b_cpu())
		ret = stop_ao_cpu();*/

	while (rtc_comm_init() < 0) {
		RTC_DBG(RTC_DBG_VAL, "aml_rtc -- rtc_common_init fail\n");
		if (s_nrdy_cnt > RESET_RETRY_TIMES) {
			s_nrdy_cnt = 0;
			rst_times++;
			if (rst_times > 3) {
				pr_info("_ser_access_read_locked error\n");
				goto out;
			}
			aml_rtc_reset();
		}
		rtc_reset_s_ready();
		s_nrdy_cnt++;
	}

	RTC_DBG(RTC_DBG_VAL, "aml_rtc -- ser_access_read_3\n");
	rtc_send_addr_data(1, addr);
	rtc_set_mode(0); /* Read */
	rtc_get_data(&val);
out:
/*	if (is_meson_m8b_cpu()){
		if (ret >= 0)
			run_arc_program();
	}*/
	return val;
}

static void _ser_access_write_locked(unsigned long addr, unsigned long data)
{
	int s_nrdy_cnt = 0;
	int rst_times = 0;
/*	int ret = 0;*/

	if (get_rtc_status())
		return;
/*	if (is_meson_m8b_cpu())
		ret = stop_ao_cpu();*/

	while (rtc_comm_init() < 0) {

		if (s_nrdy_cnt > RESET_RETRY_TIMES) {
			s_nrdy_cnt = 0;
			rst_times++;
			if (rst_times > 3) {
				pr_info("_ser_access_write_locked error\n");
				goto out;
			}
			aml_rtc_reset();
			pr_info("error: rtc serial communication abnormal!\n");
			/* return -1; */
		}
		rtc_reset_s_ready();
		s_nrdy_cnt++;
	}
	rtc_send_addr_data(0, data);
	rtc_send_addr_data(1, addr);
	rtc_set_mode(1); /* Write */
out:
/*	if (is_meson_m8b_cpu()){
		if (ret >= 0)
			run_arc_program();
	}*/
	return;
}

static unsigned int ser_access_read(unsigned long addr)
{
	unsigned val = 0;
	unsigned long flags;

	RTC_DBG(RTC_DBG_VAL, "aml_rtc --ser_access_read_1\n");
	/*if(check_osc_clk() < 0){
		RTC_DBG(RTC_DBG_VAL, "aml_rtc -- the osc clk does not work\n");
		return val;
	}*/

	RTC_DBG(RTC_DBG_VAL, "aml_rtc -- ser_access_read_2\n");
	spin_lock_irqsave(&com_lock, flags);
	val = _ser_access_read_locked(addr);
	spin_unlock_irqrestore(&com_lock, flags);

	return val;
}

static int ser_access_write(unsigned long addr, unsigned long data)
{
	unsigned long flags;

	spin_lock_irqsave(&com_lock, flags);
	_ser_access_write_locked(addr, data);
	spin_unlock_irqrestore(&com_lock, flags);
	rtc_wait_s_ready();

	return 0;
}

/***************************************************************************/
int rtc_reset_gpo(struct device *dev, unsigned level)
{
	unsigned data = 0;
	data |= 1<<20;
	if (get_cpu_type() > MESON_CPU_MAJOR_ID_M6) {
		if (level)
			data |= 1<<22;         /* gpo pin level high */
	} else {
	/* reset mode */
		if (!level)
			data |= 1<<22;         /* gpo pin level high */
	}

	ser_access_write(RTC_GPO_COUNTER_ADDR, data);
	rtc_wait_s_ready();

	return 0;
}

struct alarm_data_t {
	int level;
	unsigned alarm_sec;   /* in s */
};

/*
 * Return RTC_GPO_COUNTER bit-24 value.
 */
int aml_rtc_alarm_status(void)
{
	u32 data32 = ser_access_read(RTC_GPO_COUNTER_ADDR);
	RTC_DBG(RTC_DBG_VAL, "%s() RTC_GPO_COUNTER=%x\n", __func__, data32);
	data32 &= (1 << 24);
	return data32;
}

/* set the rtc alarm */
/* after alarm_data->alarm_sec, the gpo lvl will be //alarm_data->level */
int rtc_set_alarm_aml(struct device *dev, struct alarm_data_t *alarm_data)
{
	unsigned data = 0;
	/* reset the gpo level */

	rtc_reset_gpo(dev, !(alarm_data->level));

	data |= 2 << 20;    /* output defined level after time */

	if (get_cpu_type() > MESON_CPU_MAJOR_ID_M6)
		data |= (alarm_data->level & 1) << 22;    /*  */
	else
		data |= (!(alarm_data->level & 1)) << 22;    /*  */

	if (alarm_data->alarm_sec >= 1024*1024)
		return -1;

	data |= alarm_data->alarm_sec - 1;

	pr_info("write alarm data: %u\n", data);
	ser_access_write(RTC_GPO_COUNTER_ADDR, data);
	rtc_wait_s_ready();
	rtc_comm_delay();

	data = ser_access_read(RTC_GPO_COUNTER_ADDR);

	pr_info("read alarm data: %u\n", data);
	pr_info("read alarm count: %u\n", ser_access_read(RTC_COUNTER_ADDR));

	return 0;
}

/*************************************************************************/


/* ------------------------------------------------------------------------- */
/* Function: rtc_ser_static_write_manual */
/* Use part of the serial bus: sclk_static, sdi and sen to shift */
/* in a 16-bit static data. Manual mode. */
/* ------------------------------------------------------------------------- */
/* static void rtc_ser_static_write_manual (unsigned int static_reg_data_in) */
/* { */
/* int i; */
/* RTC_DBG(RTC_DBG_VAL, "rtc_ser_static_write_manual: data=0x%0/x\n", */
/* static_reg_data_in); */
/*  */
/* // Initialize: sen low for 1 clock cycle */
/* RTC_sen_LOW(0); */
/* RTC_sclk_static_LOW(0); */
/* RTC_sclk_static_HIGH(1); */
/* RTC_sen_HIGH(1); */
/* RTC_sclk_static_LOW(0); */
/*  */
/* // Shift in 16-bit known sequence */
/* for (i = 15; i >= 0; i --) { */
/*  */
/* if ((RTC_SER_REG_DATA_NOTIFIER >> i) & 0x1) { */
/* RTC_sdi_HIGH(1); */
/* } */
/* else { */
/* RTC_sdi_LOW(0); */
/* } */
/*  */
/* RTC_sclk_static_HIGH(1); */
/* RTC_sclk_static_LOW(0); */
/* } */
/*  */
/* // 1 clock cycle turn around */
/* RTC_sdi_LOW(0); */
/* RTC_sclk_static_HIGH(1); */
/* RTC_sclk_static_LOW(0); */
/*  */
/* // Shift in 16-bit static register data */
/* for (i = 15; i >= 0; i --) { */
/* if ((static_reg_data_in >> i) & 0x1) { */
/* RTC_sdi_HIGH(1); */
/* } */
/* else { */
/* RTC_sdi_LOW(0); */
/* } */
/* RTC_sclk_static_HIGH(1); */
/* RTC_sclk_static_LOW(0); */
/* } */
/*  */
/* // One more clock cycle to complete write */
/* RTC_sen_LOW(0); */
/* RTC_sdi_LOW(0); */
/* RTC_sclk_static_HIGH(1); */
/* RTC_sclk_static_LOW(0); */
/* } */


static void static_register_write(unsigned data)
{
	rtc_ser_static_write_auto(data);
}

static int aml_rtc_read_time(struct device *dev, struct rtc_time *tm)
{
	unsigned int time_t;

	RTC_DBG(RTC_DBG_VAL, "aml_rtc: read rtc time\n");
	time_t = ser_access_read(RTC_COUNTER_ADDR);
	RTC_DBG(RTC_DBG_VAL,
		"aml_rtc: have read the rtc time, time is %d\n",
		time_t);
	if ((int)time_t < 0) {
		RTC_DBG(RTC_DBG_VAL,
			"aml_rtc: time(%d) < 0, reset to 0", time_t);
		time_t = 0;
	}
	rtc_time_to_tm(time_t, tm);

	return 0;
}

static int aml_rtc_write_time(struct device *dev, struct rtc_time *tm)
{
	unsigned long time_t;

	rtc_tm_to_time(tm, &time_t);
	RTC_DBG(RTC_DBG_VAL,
		"aml_rtc : write the rtc time, time is %ld\n",
		time_t);
	ser_access_write(RTC_COUNTER_ADDR, time_t);
	RTC_DBG(RTC_DBG_VAL, "aml_rtc : the time has been written\n");

	return 0;
}

static int aml_rtc_set_alarm(struct device *dev, struct rtc_wkalrm *alarm)
{
	struct alarm_data_t alarm_data;
	unsigned long alarm_secs, cur_secs;
	struct rtc_time cur_time;
	int ret;
	struct aml_rtc_priv *priv;

	priv = dev_get_drvdata(dev);
	/* rtc_tm_to_time(&alarm->time, &secs); */

	if (alarm->enabled) {
		alarm_data.level = 0;
		ret = rtc_tm_to_time(&alarm->time, &alarm_secs);
		if (ret)
			return ret;
		aml_rtc_read_time(dev, &cur_time);
		ret = rtc_tm_to_time(&cur_time, &cur_secs);
		if (alarm_secs >= cur_secs) {
			/*3 seconds later then we real wanted,
			  we do not need the alarm very acurate.*/
			alarm_data.alarm_sec = alarm_secs - cur_secs + 3;
		} else
			alarm_data.alarm_sec =  0;

		rtc_set_alarm_aml(dev, &alarm_data);
	} else {
		if (get_cpu_type() > MESON_CPU_MAJOR_ID_M6)
			ser_access_write(RTC_GPO_COUNTER_ADDR, 0x500000);
		else
			ser_access_write(RTC_GPO_COUNTER_ADDR, 0x100000);

		queue_work(priv->rtc_work_queue, &priv->work);
	}
	return 0;
}

#define AUTO_RESUME_INTERVAL 8
static int aml_rtc_suspend(struct platform_device *pdev, pm_message_t state)
{
	#ifdef CONFIG_MESON_SUSPEND_TEST
	alarm_data_t alarm_data;
	static int suspend_time;
	alarm_data.alarm_sec = AUTO_RESUME_INTERVAL;
	alarm_data.level = 0;
	rtc_set_alarm_aml(&pdev->dev, &alarm_data);
	pr_info("suspend %d times, system will up %ds later!\n",
			++suspend_time, alarm_data.alarm_sec);
	#endif /* CONFIG_MESON_SUSPEND_TEST */
	return 0;
}
int aml_rtc_resume(struct platform_device *pdev)
{
	if (get_cpu_type() > MESON_CPU_MAJOR_ID_M6)
		ser_access_write(RTC_GPO_COUNTER_ADDR, 0x500000);
	else
		ser_access_write(RTC_GPO_COUNTER_ADDR, 0x100000);

	pr_info("resume reset gpo !\n");
	return 0;
}

static char *rtc_reg[8] = {
	"RTC_COUNTER    ",
	"RTC_GPO_COUNTER",
	"RTC_SEC_ADJUST ",
	"UNUSED         ",
	"RTC_REGMEM_0   ",
	"RTC_REGMEM_1   ",
	"RTC_REGMEM_2   ",
	"RTC_REGMEM_3   "
};

static ssize_t show_rtc_reg(struct class *class,
			struct class_attribute *attr,	char *buf)
{
	int i;

	pr_info("enter function: %s\n", __func__);
	for (i = 0; i < 8; i++)
		pr_info(" %20s : 0x%x\n", rtc_reg[i], ser_access_read(i));

	return 0;
}

static const struct rtc_class_ops aml_rtc_ops = {
	.read_time = aml_rtc_read_time,
	.set_time = aml_rtc_write_time,
	.set_alarm = aml_rtc_set_alarm,
};

static struct class_attribute rtc_class_attrs[] = {
	__ATTR(rtc_reg_log, S_IRUGO | S_IWUSR, show_rtc_reg, NULL),
	__ATTR_NULL
};

static struct class aml_rtc_class = {
	.name = "aml_rtc",
	.class_attrs = rtc_class_attrs,
};

static int aml_rtc_probe(struct platform_device *pdev)
{
	struct aml_rtc_priv *priv;
	struct device_node *aml_rtc_node = pdev->dev.of_node;
	struct resource		*res;
	int ret;
	int sec_adjust = 0;

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);

	if (!priv)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	rtc_base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(rtc_base))
		return PTR_ERR(rtc_base);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	ao_ctrl_reg = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(ao_ctrl_reg))
		return PTR_ERR(ao_ctrl_reg);

	if (get_cpu_type() > MESON_CPU_MAJOR_ID_M6)
		static_register_write(0x180a);
	else
		static_register_write(0x3c0a);

	INIT_WORK(&priv->work, reset_gpo_work);
	platform_set_drvdata(pdev, priv);

	priv->rtc_work_queue = create_singlethread_workqueue("rtc");
	if (priv->rtc_work_queue == NULL) {
		ret = -ENOMEM;
		goto out;
	}

	/* platform setup code should have handled this; sigh */
	if (!device_can_wakeup(&pdev->dev))
		device_init_wakeup(&pdev->dev, 1);

	priv->rtc = rtc_device_register("aml_rtc", &pdev->dev, &
						aml_rtc_ops, THIS_MODULE);

	if (IS_ERR(priv->rtc)) {
		ret = PTR_ERR(priv->rtc);
		goto out;
	}

	/* ser_access_write(RTC_GPO_COUNTER_ADDR,0x100000); */
	/* static_register_write(0x0004); */
	if (get_cpu_type() > MESON_CPU_MAJOR_ID_M6)
		ser_access_write(RTC_GPO_COUNTER_ADDR, 0x500000);
	else
		ser_access_write(RTC_GPO_COUNTER_ADDR, 0x100000);

	rtc_wait_s_ready();

	ret = of_property_read_u32(aml_rtc_node, "sec_adjust", &sec_adjust);
	if (!ret) {
		ser_access_write(RTC_SEC_ADJUST_ADDR, 1<<23 | 10<<19 | 1735);
		rtc_wait_s_ready();
	}

	/* check_osc_clk(); */
	/* ser_access_write(RTC_COUNTER_ADDR, 0); */
	ret = class_register(&aml_rtc_class);
	if (ret)
		pr_info(" class register rtc_class fail!\n");

	return 0;

out:
	if (priv->rtc_work_queue)
		destroy_workqueue(priv->rtc_work_queue);
	kfree(priv);
	return ret;
}

static int get_gpo_flag(void)
{
	u32 data32 = 0;
	int ret = 0;

	data32 = ser_access_read(RTC_GPO_COUNTER_ADDR);

	RTC_DBG(RTC_DBG_VAL, "%s() RTC_GPO_COUNTER=%x\n", __func__, data32);
	if (get_cpu_type() > MESON_CPU_MAJOR_ID_M6)
		ret = !(data32 & (1 << 24));
	else
	ret = !!(data32 & (1 << 24));

	return ret;
}

unsigned int aml_read_rtc_mem_reg(unsigned char reg_id)
{
	unsigned char reg_array[] = {
		RTC_REGMEM_ADDR_0,
		RTC_REGMEM_ADDR_1,
		RTC_REGMEM_ADDR_2,
		RTC_REGMEM_ADDR_3,
	};
	if (reg_id > 4)
		return 0;
	return  ser_access_read(reg_array[reg_id]);
}
EXPORT_SYMBOL(aml_read_rtc_mem_reg);

int aml_write_rtc_mem_reg(unsigned char reg_id, unsigned int data)
{
	unsigned char reg_array[] = {
		RTC_REGMEM_ADDR_0,
		RTC_REGMEM_ADDR_1,
		RTC_REGMEM_ADDR_2,
		RTC_REGMEM_ADDR_3,
	};
	if (reg_id > 4)
		return 0;
	return  ser_access_write(reg_array[reg_id], data);
}
EXPORT_SYMBOL(aml_write_rtc_mem_reg);

unsigned int aml_get_rtc_counter(void)
{
	unsigned int val;
	val = ser_access_read(RTC_COUNTER_ADDR);
	return val;
}
EXPORT_SYMBOL(aml_get_rtc_counter);

static void reset_gpo_work(struct work_struct *work)
{
	int count = 5;

	while (get_gpo_flag()) {
		if (get_cpu_type() > MESON_CPU_MAJOR_ID_M6)
			ser_access_write(RTC_GPO_COUNTER_ADDR, 0x500000);
		else
			ser_access_write(RTC_GPO_COUNTER_ADDR, 0x100000);

		count--;
		if (count <= 0) {
			pr_err("error: can not reset gpo !\n");
			/* count = 5; */
			return;
			/* panic("gpo can not be reset"); */
		}
	}

	pr_info("reset gpo !\n");

}

#if 0
static int power_down_gpo(unsigned long data)
{
	struct aml_rtc_priv *priv = (struct aml_rtc_priv *)data;
	queue_work(priv->rtc_work_queue, &priv->work);

	return 0;
}
#endif

static void aml_rtc_shutdown(struct platform_device *pdev)
{
	if (get_cpu_type() > MESON_CPU_MAJOR_ID_M6)
		ser_access_write(RTC_GPO_COUNTER_ADDR, 0x500000);
	else
		ser_access_write(RTC_GPO_COUNTER_ADDR, 0x100000);
}

static int aml_rtc_remove(struct platform_device *dev)
{
	struct aml_rtc_priv *priv = platform_get_drvdata(dev);
	rtc_device_unregister(priv->rtc);
	if (priv->rtc_work_queue)
		destroy_workqueue(priv->rtc_work_queue);
	del_timer(&priv->timer);
	kfree(priv);
	return 0;
}

static const struct of_device_id meson_rtc_dt_match[] = {
	{ .compatible = "amlogic,meson-rtc"},
	{},
};

struct platform_driver aml_rtc_driver = {
	.driver = {
		.name = "aml_rtc",
		.owner = THIS_MODULE,
		.of_match_table = meson_rtc_dt_match,
	},
	.probe = aml_rtc_probe,
	.remove = (aml_rtc_remove),
	.suspend = aml_rtc_suspend,
	.resume = aml_rtc_resume,
	.shutdown = aml_rtc_shutdown,
};

static int  __init aml_rtc_init(void)
{
	RTC_DBG(RTC_DBG_VAL, "aml_rtc --aml_rtc_init\n");
	return platform_driver_register(&aml_rtc_driver);
}

static void __init aml_rtc_exit(void)
{
	return platform_driver_unregister(&aml_rtc_driver);
}

module_init(aml_rtc_init);
module_exit(aml_rtc_exit);

MODULE_DESCRIPTION("Amlogic internal rtc driver");
MODULE_LICENSE("GPL");
