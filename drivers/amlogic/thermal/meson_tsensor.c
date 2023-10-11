// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/cpu_cooling.h>
#include <linux/pm_domain.h>
#include <linux/pm_runtime.h>
#include <linux/arm-smccc.h>
#include <linux/amlogic/secmon.h>
#include "../../thermal/thermal_core.h"
#include "../../thermal/thermal_hwmon.h"
#include <linux/reset.h>

/*r1p1 thermal sensor version*/
#define R1P1_TS_CFG_REG1	(0x1 * 4)
#define R1P1_TS_CFG_REG2	(0x2 * 4)
#define R1P1_TS_CFG_REG3	(0x3 * 4)
#define R1P1_TS_CFG_REG4	(0x4 * 4)
#define R1P1_TS_CFG_REG5	(0x5 * 4)
#define R1P1_TS_CFG_REG6	(0x6 * 4)
#define R1P1_TS_CFG_REG7	(0x7 * 4)
#define R1P1_TS_CFG_REG8	(0x8 * 4)
#define R1P1_TS_STAT0		(0x10 * 4)
#define R1P1_TS_STAT1		(0x11 * 4)
#define R1P1_TS_STAT2		(0x12 * 4)
#define R1P1_TS_STAT3		(0x13 * 4)
#define R1P1_TS_STAT4		(0x14 * 4)
#define R1P1_TS_STAT5		(0x15 * 4)
#define R1P1_TS_STAT6		(0x16 * 4)
#define R1P1_TS_STAT7		(0x17 * 4)
#define R1P1_TS_STAT8		(0x18 * 4)
#define R1P1_TS_STAT9		(0x19 * 4)

#define R1P1_TS_VALUE_CONT	0x1
#define	R1P1_TRIM_INFO		0x0
#define R1P1_TS_TEMP_MASK	0xfff
#define R1P1_TS_IRQ_MASK	0xff

#define R1P1_TS_IRQ_LOGIC_EN_SHIT	15
#define R1P1_TS_IRQ_FALL3_EN_SHIT	31
#define R1P1_TS_IRQ_FALL2_EN_SHIT	30
#define R1P1_TS_IRQ_FALL1_EN_SHIT	29
#define R1P1_TS_IRQ_FALL0_EN_SHIT	28
#define R1P1_TS_IRQ_RISE3_EN_SHIT	27
#define R1P1_TS_IRQ_RISE2_EN_SHIT	26
#define R1P1_TS_IRQ_RISE1_EN_SHIT	25
#define R1P1_TS_IRQ_RISE0_EN_SHIT	24
#define R1P1_TS_IRQ_FALL3_CLR_SHIT	23
#define R1P1_TS_IRQ_FALL2_CLR_SHIT	22
#define R1P1_TS_IRQ_FALL1_CLR_SHIT	21
#define R1P1_TS_IRQ_FALL0_CLR_SHIT	20
#define R1P1_TS_IRQ_RISE3_CLR_SHIT	19
#define R1P1_TS_IRQ_RISE2_CLR_SHIT	18
#define R1P1_TS_IRQ_RISE1_CLR_SHIT	17
#define R1P1_TS_IRQ_RISE0_CLR_SHIT	16
#define R1P1_TS_IRQ_ALL_CLR		(0xff << 16)
#define R1P1_TS_IRQ_ALL_EN		(0xff << 24)
#define R1P1_TS_IRQ_ALL_CLR_SHIT	16

#define R1P1_TS_RSET_VBG	BIT(12)
#define R1P1_TS_RSET_ADC	BIT(11)
#define R1P1_TS_VCM_EN		BIT(10)
#define R1P1_TS_VBG_EN		BIT(9)
#define R1P1_TS_OUT_CTL		BIT(6)
#define R1P1_TS_FILTER_EN	BIT(5)
#define R1P1_TS_IPTAT_EN	BIT(4) /*for debug, no need enable*/
#define R1P1_TS_DEM_EN		BIT(3)
#define R1P1_TS_CH_SEL		0x3 /*set 3'b011 for work*/

#define R1P1_TS_HITEMP_EN	BIT(31)
#define R1P1_TS_REBOOT_ALL_EN	BIT(30)
#define R1P1_TS_REBOOT_TIME	(0xff << 16)

/*for all thermal sensor*/
#define MCELSIUS	1000
#define MAX_TS_NUM	3
#define	TS_DEF_RTEMP	125
#define	TEMP_CAL	1
#define	R1P1_CAL_NUM	4
#define	R1P1_TS_MAX	0x3500
#define	R1P1_TS_MIN	0x1500
#define	R1P1_TS_CLK_RATE	500000
#define R1P1_TS_WAIT		10
#define R1P1_PM_MIN_TIMEOUT	5
#define R1P1_PM_MAX_TIMEOUT	250
#define TSENSOR_CALI_READ	0x82000047

enum soc_type {
	SOC_ARCH_TS_R1P0 = 1,
	SOC_ARCH_TS_R1P1 = 2,
};

/**
 * struct meson_tsensor_platform_data
 * @cal_a, b, c, d: cali data
 * @cal_type: calibration type for temperature
 * @reboot_temp: high temperature reboot soc
 * This structure is required for configuration of exynos_tmu driver.
 */
struct meson_tsensor_platform_data {
	u32 cal_type;
	int tsensor_id;
	u32 cal_coeff[4];
	int reboot_temp;
};

/**
 * struct meson_tsensor_data :
 * A structure to hold the private data of the tsensor driver.
 */
struct meson_tsensor_data {
	int id;
	struct device		*dev;
	struct meson_tsensor_platform_data *pdata;
	void __iomem *base_c;
	void __iomem *base_e;
	int irq;
	enum soc_type soc;
	struct work_struct irq_work;
	struct mutex lock;/*mutex lock for set tsensor reg*/
	struct clk *clk;
	u32	trim_info;
	struct thermal_zone_device *tzd;
	unsigned int ntrip;
	struct reset_control *rst;
	int (*tsensor_hw_initialize)(struct platform_device *pdev);
	int (*tsensor_trips_initialize)(struct platform_device *pdev);
	void (*tsensor_control)(struct platform_device *pdev,
				bool on);
	int (*tsensor_read)(struct meson_tsensor_data *data);
	void (*tsensor_set_emulation)(struct meson_tsensor_data *data,
				      int temp);
	void (*tsensor_clear_irqs)(struct meson_tsensor_data *data);
	void (*tsensor_update_irqs)(struct meson_tsensor_data *data);
};

static void meson_report_trigger(struct meson_tsensor_data *p)
{
	char data[10], *envp[] = { data, NULL };
	struct thermal_zone_device *tz = p->tzd;
	int temp;
	unsigned int i;

	if (!tz) {
		pr_err("No thermal zone device defined\n");
		return;
	}
	/*
	 *if passive delay and polling delay all is zero
	 *mean thermal mode disabled, disable update envent
	 */
	if (0 == (tz->passive_delay || tz->polling_delay))
		return;

	thermal_zone_device_update(tz, THERMAL_EVENT_UNSPECIFIED);

	mutex_lock(&tz->lock);
	/* Find the level for which trip happened */
	for (i = 0; i < of_thermal_get_ntrips(tz); i++) {
		tz->ops->get_trip_temp(tz, i, &temp);
		if (tz->last_temperature < temp)
			break;
	}

	snprintf(data, sizeof(data), "%u", i);
	kobject_uevent_env(&tz->device.kobj, KOBJ_CHANGE, envp);
	mutex_unlock(&tz->lock);
}

/*
 * tsensor treats temperature as a mapped temperature code.
 * The temperature is converted differently depending on the calibration type.
 */
static u32 temp_to_code(struct meson_tsensor_data *data, int temp, bool trend)
{
	struct meson_tsensor_platform_data *pdata = data->pdata;
	s64 div_tmp1, div_tmp2;
	u32 uefuse, reg_code;
	u32 cal_a, cal_b, cal_c, cal_d, cal_type;

	uefuse = data->trim_info;
	uefuse = uefuse & 0xffff;

	/* T = 727.8*(u_real+u_efuse/(1<<16)) - 274.7 */
	/* u_readl = (5.05*YOUT)/((1<<16)+ 4.05*YOUT) */
	/*u_readl = (T + 274.7) / 727.8 - u_efuse / (1 << 16)*/
	/*Yout =  (u_readl / (5.05 - 4.05u_readl)) *(1 << 16)*/
	cal_type = pdata->cal_type;
	cal_a = pdata->cal_coeff[0];
	cal_b = pdata->cal_coeff[1];
	cal_c = pdata->cal_coeff[2];
	cal_d = pdata->cal_coeff[3];
	switch (cal_type & 0xf) {
	case 0x1:
		div_tmp2 = cal_c;
		div_tmp2 = div_tmp2 + temp * 10;
		div_tmp2 = (1 << 16) * div_tmp2;
		div_tmp2 = div_s64(div_tmp2, cal_d);
		if (uefuse & 0x8000)
			div_tmp2 = div_tmp2 + (uefuse & 0x7fff);
		else
			div_tmp2 = div_tmp2 - (uefuse & 0x7fff);
		div_tmp1 = cal_a * div_tmp2;
		div_tmp1 = div_s64(div_tmp1, 1 << 16);
		div_tmp1 = cal_b - div_tmp1;
		div_tmp2 = div_tmp2 * 100;
		div_tmp2 = div_s64(div_tmp2, div_tmp1);
		if (trend)
			reg_code = ((div_tmp2 >> 0x4) & R1P1_TS_TEMP_MASK)
				+ TEMP_CAL;
		else
			reg_code = ((div_tmp2 >> 0x4) & R1P1_TS_TEMP_MASK);
		break;
	default:
		pr_info("Cal_type not supported\n");
		return -EINVAL;
	}
	return reg_code;
}

/*
 * Calculate a temperature value from a temperature code.
 * The unit of the temperature is degree Celsius.
 */
static int code_to_temp(struct meson_tsensor_data *data, int temp_code)
{
	struct meson_tsensor_platform_data *pdata = data->pdata;
	u32 cal_type, cal_a, cal_b, cal_c, cal_d;
	s64 temp, div_tmp1, div_tmp2;
	u32 uefuse;

	uefuse = data->trim_info;
	uefuse = uefuse & 0xffff;
	temp = temp_code;

	cal_type = (pdata->cal_type) & 0xf;
	cal_a = pdata->cal_coeff[0];
	cal_b = pdata->cal_coeff[1];
	cal_c = pdata->cal_coeff[2];
	cal_d = pdata->cal_coeff[3];
	switch (cal_type) {
	case 0x1:
		/* T = 727.8*(u_real+u_efuse/(1<<16)) - 274.7 */
		/* u_readl = (5.05*YOUT)/((1<<16)+ 4.05*YOUT) */
		/*u_readl = (T + 274.7) / 727.8 - u_efuse / (1 << 16)*/
		/*Yout =  (u_readl / (5.05 - 4.05u_readl)) *(1 << 16)*/
		div_tmp1 = cal_a * temp;
		div_tmp1 = div_s64(div_tmp1, 100);
		div_tmp2 = temp * cal_b;
		div_tmp2 = div_s64(div_tmp2, 100);
		div_tmp2 = div_tmp2 * (1 << 16);
		div_tmp2 = div_s64(div_tmp2, (1 << 16) + div_tmp1);
		if (uefuse & 0x8000) {
			div_tmp1 = (div_tmp2 - (uefuse & (0x7fff))) * cal_d;
			div_tmp1 = div_s64(div_tmp1, 1 << 16);
			temp = (div_tmp1 - cal_c) * 100;

		} else {
			div_tmp1 = (div_tmp2 + uefuse) * cal_d;
			div_tmp1 = div_s64(div_tmp1, 1 << 16);
			temp = (div_tmp1 - cal_c) * 100;
		}
		break;
	default:
		pr_info("Cal_type not supported\n");
		return -EINVAL;
	}
	return temp;
}

static int meson_tsensor_hw_initialize(struct platform_device *pdev)
{
	struct meson_tsensor_data *data = platform_get_drvdata(pdev);
	int ret;

	mutex_lock(&data->lock);
	ret = data->tsensor_hw_initialize(pdev);
	mutex_unlock(&data->lock);
	return ret;
}

static int meson_tsensor_trips_initialize(struct platform_device *pdev)
{
	struct meson_tsensor_data *data = platform_get_drvdata(pdev);
	int ret;

	if (of_thermal_get_ntrips(data->tzd) > data->ntrip) {
		dev_info_once(&pdev->dev,
			 "More trip points than supported by this tsensor.\n");
		dev_info_once(&pdev->dev,
			 "%d trip points should be configured in polling mode.\n",
			 (of_thermal_get_ntrips(data->tzd) - data->ntrip));
	}
	mutex_lock(&data->lock);
	ret = data->tsensor_trips_initialize(pdev);
	mutex_unlock(&data->lock);
	return ret;
}

static void meson_tsensor_control(struct platform_device *pdev, bool on)
{
	struct meson_tsensor_data *data = platform_get_drvdata(pdev);

	mutex_lock(&data->lock);
	data->tsensor_control(pdev, on);
	mutex_unlock(&data->lock);
}

static void r1p1_tsensor_control(struct platform_device *pdev, bool on)
{
	struct meson_tsensor_data *data = platform_get_drvdata(pdev);
	unsigned int con;
	int ret;

	con = readl(data->base_c + R1P1_TS_CFG_REG1);

	if (on) {
		con |= (0x1 << R1P1_TS_IRQ_LOGIC_EN_SHIT);
		con |= (R1P1_TS_FILTER_EN | R1P1_TS_VCM_EN | R1P1_TS_VBG_EN
			| R1P1_TS_DEM_EN | R1P1_TS_CH_SEL);
		con &= ~(R1P1_TS_RSET_VBG | R1P1_TS_RSET_ADC);
		ret = clk_set_rate(data->clk, R1P1_TS_CLK_RATE);
		if (ret)
			dev_err(&pdev->dev,
				"Failed to set clk rate: %d\n", ret);
		ret = clk_prepare_enable(data->clk);
		if (ret)
			dev_err(&pdev->dev,
				"Failed to perpare clk enable: %d\n", ret);
	} else {
		clk_disable_unprepare(data->clk);
		con &= ~((1 << R1P1_TS_IRQ_LOGIC_EN_SHIT)
			| (R1P1_TS_IRQ_ALL_CLR));
		con &= ~(R1P1_TS_FILTER_EN | R1P1_TS_VCM_EN | R1P1_TS_VBG_EN
			| R1P1_TS_IPTAT_EN | R1P1_TS_DEM_EN);
	}
	writel(con, data->base_c + R1P1_TS_CFG_REG1);
}

static int r1p1_tsensor_hw_initialize(struct platform_device *pdev)
{
	struct meson_tsensor_data *data = platform_get_drvdata(pdev);
	struct meson_tsensor_platform_data *pdata = data->pdata;
	u32 reboot_reg = 0xffff, con = 0;
	int ret = 0, reboot_temp;
	int ver;

	/*first get the r1p1 trim info*/
	ver = (data->trim_info >> 24) & 0xff;
	/*r1p1 tsensor ver to doing*/
	if (((ver & 0xf) >> 2) == 0)
		return -EINVAL;
	if ((ver & 0x80)  == 0)
		return -EINVAL;

	/*r1p1 init the ts reboot soc function*/
	reboot_temp = pdata->reboot_temp;
	if (reboot_temp) {
		reboot_reg = temp_to_code(data, reboot_temp / MCELSIUS, true);
		con = (readl(data->base_c + R1P1_TS_CFG_REG2) |
			(reboot_reg << 4));
		con |= (R1P1_TS_HITEMP_EN | R1P1_TS_REBOOT_ALL_EN);
		con |= (R1P1_TS_REBOOT_TIME);
		writel(con, data->base_c + R1P1_TS_CFG_REG2);
	} else {
		pr_info("tsensor rtemp is zore no enable hireboot\n");
	}

	return ret;
}

static int r1p1_tsensor_trips_initialize(struct platform_device *pdev)
{
	struct meson_tsensor_data *data = platform_get_drvdata(pdev);
	struct thermal_zone_device *tz = data->tzd;
	u32 rising_threshold = 0, falling_threshold = 0;
	int ret = 0, threshold_code, i;
	int temp, temp_hist;
	unsigned int reg_off, bit_off;

	/*
	 * Write temperature code for rising and falling threshold
	 * On r1p1 tsensor there are 4 rising and 4 falling threshold
	 * registers (0x50-0x5c and 0x60-0x6c respectively). Each
	 * register holds the value of two threshold levels (at bit
	 * offsets 0 and 16). Based on the fact that there are atmost
	 * eight possible trigger levels, calculate the register and
	 * bit offsets where the threshold levels are to be written.
	 *
	 * e.g.
	 * R1P1_TS_CFG_REG4 (0x4 << 2)
	 * [23:12] - rise_th0
	 * [11:0] - rise_th1
	 * R1P1_TS_CFG_REG5 (0x5 << 2)
	 * [23:12] - rise_th2
	 * [11:0] - rise_th3
	 * R1P1_TS_CFG_REG6 (0x6 << 2)
	 * [23:12] - fall_th0
	 * [11:0] - fall_th1
	 * R1P1_TS_CFG_REG7 (0x7 << 2)
	 * [23:12] - fall_th2
	 * [11:0] - fall_th3
	 */
	for (i = (data->ntrip - 1); i >= 0; i--) {
		reg_off = (i / 2) << 2;
		bit_off = ((i + 1) % 2);
		tz->ops->get_trip_temp(tz, i, &temp);
		temp /= MCELSIUS;
		tz->ops->get_trip_hyst(tz, i, &temp_hist);
		temp_hist = temp - (temp_hist / MCELSIUS);

		/* Set 12-bit temperature code for rising threshold levels */
		threshold_code = temp_to_code(data, temp, true);
		rising_threshold = readl(data->base_c +
				R1P1_TS_CFG_REG4 + reg_off);
		rising_threshold &= ~(R1P1_TS_TEMP_MASK << (12 * bit_off));
		rising_threshold |= threshold_code << (12 * bit_off);
		writel(rising_threshold,
		       data->base_c + R1P1_TS_CFG_REG4 + reg_off);

		/* Set 12-bit temperature code for falling threshold levels */
		threshold_code = temp_to_code(data, temp_hist, false);
		falling_threshold = readl(data->base_c +
					  R1P1_TS_CFG_REG6 + reg_off);
		falling_threshold &= ~(R1P1_TS_TEMP_MASK << (12 * bit_off));
		falling_threshold |= threshold_code << (12 * bit_off);
		writel(falling_threshold,
		       data->base_c + R1P1_TS_CFG_REG6 + reg_off);
	}
	data->tsensor_update_irqs(data);
	data->tsensor_clear_irqs(data);

	return ret;
}

static int r1p1_tsensor_read(struct meson_tsensor_data *data)
{
	int j, cnt = 0;
	unsigned int tvalue = 0;
	unsigned int value_all = 0;

	/*
	 *r1p1 tsensor get 16 times value.
	 */
	for (j = 0; j < R1P1_TS_VALUE_CONT; j++) {
		tvalue = readl(data->base_c + R1P1_TS_STAT0);
		tvalue = tvalue & 0xffff;
		if (tvalue >= R1P1_TS_MIN && tvalue <= R1P1_TS_MAX) {
			cnt++;
			value_all += (tvalue & 0xffff);
		}
	}

	if (cnt) {
		tvalue = value_all / cnt;
		pr_debug("%s  vall: %u, cnt: %u\n",
			 __func__, value_all, cnt);
	} else {
		pr_debug("[%s %s]valid cnt is 0, tvalue:%d\n", __func__, data->tzd->type, tvalue);
		tvalue = 0;
	}
	return tvalue;
}

static void r1p1_tsensor_set_emulation(struct meson_tsensor_data *data,
				       int temp)
{
	pr_info("r1p1 ts no emulation\n");
}

static void r1p1_tsensor_clear_irqs(struct meson_tsensor_data *data)
{
	unsigned int val_irq;

	val_irq = (readl(data->base_c + R1P1_TS_STAT1)
			& R1P1_TS_IRQ_MASK);
	val_irq = (readl(data->base_c + R1P1_TS_CFG_REG1)
			| (val_irq << R1P1_TS_IRQ_ALL_CLR_SHIT));
	/* clear the interrupts */
	writel(val_irq, data->base_c + R1P1_TS_CFG_REG1);
	/* restore clear enable*/
	val_irq = (readl(data->base_c + R1P1_TS_CFG_REG1)
			& (~R1P1_TS_IRQ_ALL_CLR));
	writel(val_irq, data->base_c + R1P1_TS_CFG_REG1);
}

static void r1p1_tsensor_update_irqs(struct meson_tsensor_data *data)
{
	struct thermal_zone_device *tz = data->tzd;
	int temp;
	unsigned int i, con;

	/* Find the level for which trip happened */
	for (i = 0; i < of_thermal_get_ntrips(tz); i++) {
		tz->ops->get_trip_temp(tz, i, &temp);
		if (tz->last_temperature < temp)
			break;
	}

	con = readl(data->base_c + R1P1_TS_CFG_REG1);
	con &= ~(R1P1_TS_IRQ_ALL_EN);
	con |= (of_thermal_is_trip_valid(tz, i)
			<< (R1P1_TS_IRQ_RISE0_EN_SHIT + i));
	con |= (of_thermal_is_trip_valid(tz, i - 1)
			<< (R1P1_TS_IRQ_FALL0_EN_SHIT + i - 1));
	con &= ~(R1P1_TS_IRQ_ALL_CLR);
	writel(con, data->base_c + R1P1_TS_CFG_REG1);
}

static int meson_get_temp(void *p, int *temp)
{
	struct meson_tsensor_data *data = p;

	if (!data->tsensor_read)
		return -EINVAL;

	pm_runtime_get_sync(data->dev);
	msleep(R1P1_TS_WAIT);
	mutex_lock(&data->lock);
	*temp = code_to_temp(data, data->tsensor_read(data));
	mutex_unlock(&data->lock);
	pm_runtime_put_sync(data->dev);

	return 0;
}

static void meson_tsensor_work(struct work_struct *work)
{
	struct meson_tsensor_data *data = container_of(work,
			struct meson_tsensor_data, irq_work);

	meson_report_trigger(data);
	mutex_lock(&data->lock);
	/* TODO: take action based on particular interrupt */
	data->tsensor_update_irqs(data);
	data->tsensor_clear_irqs(data);
	mutex_unlock(&data->lock);
	enable_irq(data->irq);
}

static irqreturn_t meson_tsensor_irq(int irq, void *id)
{
	struct meson_tsensor_data *data = id;

	disable_irq_nosync(irq);
	schedule_work(&data->irq_work);

	return IRQ_HANDLED;
}

static const struct of_device_id meson_tsensor_match[] = {
	{ .compatible = "amlogic, r1p1-tsensor", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, meson_tsensor_match);

static int meson_of_get_soc_type(struct device_node *np)
{
	if (of_device_is_compatible(np, "amlogic, r1p1-tsensor"))
		return SOC_ARCH_TS_R1P1;
	return -EINVAL;
}

static int meson_of_sensor_conf(struct platform_device *pdev,
				struct meson_tsensor_platform_data *pdata)
{
	if (of_property_read_u32(pdev->dev.of_node, "tsensor_id",
				 &pdata->tsensor_id)) {
		pdata->tsensor_id = -1;
	}
	if (of_property_read_u32(pdev->dev.of_node, "cal_type",
				 &pdata->cal_type)) {
		dev_warn(&pdev->dev,
			 "Missing cal_type using default %d\n",
			 0x0);
		pdata->cal_type = 0x0;
	}
	if (of_property_read_u32_array(pdev->dev.of_node, "cal_coeff",
				       &pdata->cal_coeff[0], R1P1_CAL_NUM)) {
		dev_warn(&pdev->dev,
			 "Missing cal_coeff using default %d\n",
			 0x0);
	}
	if (of_property_read_u32(pdev->dev.of_node, "rtemp",
				 &pdata->reboot_temp)) {
		dev_warn(&pdev->dev,
			 "Missing rtemp using default %d\n",
			 TS_DEF_RTEMP);
		pdata->reboot_temp = TS_DEF_RTEMP;
	}
	pr_debug("tsensor rtemp :%d\n", pdata->reboot_temp);
	return 0;
}

static void of_thermal_destroy_one_zone(int id)
{
	struct device_node *np, *child;

	np = of_find_node_by_name(NULL, "thermal-zones");
	if (!np) {
		pr_debug("unable to find thermal zones\n");
		return;
	}

	for_each_available_child_of_node(np, child) {
		struct thermal_zone_device *zone;

		zone = thermal_zone_get_zone_by_name(child->name);
		if (IS_ERR(zone))
			continue;

		if (zone->id == id - 1) {
			thermal_zone_device_unregister(zone);
		}
	}
		of_node_put(np);
}

static int meson_map_dt_data(struct platform_device *pdev)
{
	struct meson_tsensor_data *data = platform_get_drvdata(pdev);
	struct meson_tsensor_platform_data *pdata;
	struct resource res;
	struct arm_smccc_res smc_res;
	void *sharemem_outbuf_base;

	if (!data || !pdev->dev.of_node)
		return -ENODEV;
	data->id = of_alias_get_id(pdev->dev.of_node, "tsensor");
	pr_debug("tsensor id: %d\n", data->id);
	if (data->id < 0)
		data->id = 0;

	data->irq = irq_of_parse_and_map(pdev->dev.of_node, 0);
	if (data->irq <= 0) {
		dev_err(&pdev->dev, "failed to get IRQ\n");
		return -ENODEV;
	}

	data->rst = devm_reset_control_get(&pdev->dev, "ts_rst");
	if (IS_ERR(data->rst))
		dev_warn(&pdev->dev, "Does not support reset func..\n");

	if (of_address_to_resource(pdev->dev.of_node, 0, &res)) {
		dev_err(&pdev->dev, "failed to get Resource 0\n");
		return -ENODEV;
	}

	data->base_c = devm_ioremap(&pdev->dev, res.start, resource_size(&res));
	if (!data->base_c) {
		dev_err(&pdev->dev, "Failed to ioremap memory\n");
		return -EADDRNOTAVAIL;
	}

	pdata = devm_kzalloc(&pdev->dev,
			     sizeof(struct meson_tsensor_platform_data),
			     GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;
	meson_of_sensor_conf(pdev, pdata);

	if (!(pdata->cal_type & 0xf0)) {
		if (of_address_to_resource(pdev->dev.of_node, 1, &res)) {
			dev_err(&pdev->dev, "failed to get Resource 1\n");
			return -ENODEV;
		}
		data->base_e = devm_ioremap(&pdev->dev, res.start, resource_size(&res));
		if (!data->base_e) {
			dev_err(&pdev->dev, "Failed to ioremap memory\n");
			return -ENOMEM;
		}
		data->trim_info = readl(data->base_e + R1P1_TRIM_INFO);
	} else {
		if (pdata->tsensor_id == -1) {
			dev_err(&pdev->dev, "Failed to get tsensor id\n");
			return -EINVAL;
		}

		arm_smccc_smc(TSENSOR_CALI_READ, pdata->tsensor_id, 0, 0, 0, 0, 0, 0, &smc_res);
		if (smc_res.a0) {
			dev_err(&pdev->dev, "Failed to get thermal cali data from bl31\n");
			return -EINVAL;
		}

		meson_sm_mutex_lock();
		sharemem_outbuf_base = get_meson_sm_output_base();
		meson_sm_mutex_unlock();
		if (!sharemem_outbuf_base) {
			dev_err(&pdev->dev, "Failed to get thermal cali data address\n");
			return -EINVAL;
		}
		memcpy(&data->trim_info, (const void *)sharemem_outbuf_base, 4);
		if ((data->trim_info & 0x80000000)  == 0) {
			dev_warn(&pdev->dev, "temp sensor not cali, unregister thermal zone%d.\n",
				 pdata->tsensor_id - 1);
			of_thermal_destroy_one_zone(pdata->tsensor_id);
			return -EINVAL;
		}
		pr_info("trim info = %x\n", data->trim_info);
	}

	data->pdata = pdata;
	data->soc = meson_of_get_soc_type(pdev->dev.of_node);
	switch (data->soc) {
	case SOC_ARCH_TS_R1P1:
		data->tsensor_hw_initialize = r1p1_tsensor_hw_initialize;
		data->tsensor_trips_initialize = r1p1_tsensor_trips_initialize;
		data->tsensor_control = r1p1_tsensor_control;
		data->tsensor_read = r1p1_tsensor_read;
		data->tsensor_set_emulation = r1p1_tsensor_set_emulation;
		data->tsensor_clear_irqs = r1p1_tsensor_clear_irqs;
		data->tsensor_update_irqs =  r1p1_tsensor_update_irqs;
		data->ntrip = 4;
		break;
	default:
		dev_err(&pdev->dev, "Platform not supported\n");
		return -EINVAL;
	}
	return 0;
}

static struct thermal_zone_of_device_ops meson_sensor_ops = {
	.get_temp = meson_get_temp,

};

static int meson_tsensor_probe(struct platform_device *pdev)
{
	struct meson_tsensor_data *data;
	int ret;

	pr_debug("meson ts init\n");
	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;
	data->dev = &pdev->dev;
	platform_set_drvdata(pdev, data);
	mutex_init(&data->lock);
	data->clk = devm_clk_get(&pdev->dev, "ts_comp");
	if (IS_ERR(data->clk)) {
		dev_err(&pdev->dev, "Failed to get tsclock\n");
		ret = PTR_ERR(data->clk);
		return ret;
	}

	ret = meson_map_dt_data(pdev);
	if (ret)
		return ret;

	INIT_WORK(&data->irq_work, meson_tsensor_work);

	data->tzd = devm_thermal_zone_of_sensor_register(&pdev->dev,
							 data->id,
							 data,
							 &meson_sensor_ops);
	if (IS_ERR(data->tzd)) {
		ret = PTR_ERR(data->tzd);
		dev_err(&pdev->dev, "Failed to register tsensor: %d\n", ret);
		return ret;
	}

	if (thermal_add_hwmon_sysfs(data->tzd))
		dev_warn(&pdev->dev, "Failed to add hwmon sysfs attributes\n");

	pm_runtime_enable(data->dev);
	ret = devm_request_irq(&pdev->dev, data->irq, meson_tsensor_irq,
			       IRQF_TRIGGER_RISING | IRQF_SHARED,
			       dev_name(&pdev->dev), data);
	if (ret) {
		dev_err(&pdev->dev, "Failed to request irq: %d\n", data->irq);
		goto err_thermal;
	}

	meson_tsensor_hw_initialize(pdev);
	meson_tsensor_trips_initialize(pdev);

	/*if no pm pd only need enable control*/
	meson_tsensor_control(pdev, true);
	/*wait tsensor work*/
	msleep(R1P1_TS_WAIT);

	return 0;

err_thermal:
	thermal_zone_of_sensor_unregister(&pdev->dev, data->tzd);

	return ret;
}

static int meson_tsensor_remove(struct platform_device *pdev)
{
	struct meson_tsensor_data *data = platform_get_drvdata(pdev);
	struct thermal_zone_device *tzd = data->tzd;

	thermal_zone_of_sensor_unregister(&pdev->dev, tzd);
	meson_tsensor_control(pdev, false);
	clk_unprepare(data->clk);
	devm_kfree(&pdev->dev, data);
	return 0;
}

static int __maybe_unused meson_tsensor_hw_resume(struct device *dev)
{
	/*if power domain power down, need hw/trips init when resume*/
	meson_tsensor_hw_initialize(to_platform_device(dev));
	meson_tsensor_trips_initialize(to_platform_device(dev));

	/*if no pm pd only need enable control*/
	meson_tsensor_control(to_platform_device(dev), true);
	/*wait tsensor work*/
	msleep(R1P1_TS_WAIT);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int meson_tsensor_suspend(struct device *dev)
{
	struct meson_tsensor_data *data =
		platform_get_drvdata(to_platform_device(dev));

	meson_tsensor_control(to_platform_device(dev), false);
	if (!IS_ERR(data->rst)) {
		dev_warn(dev, "reset sensor...\n");
		reset_control_assert(data->rst);
	}

	return 0;
}

static int meson_tsensor_resume(struct device *dev)
{
	struct meson_tsensor_data *data =
		platform_get_drvdata(to_platform_device(dev));

	if (!IS_ERR(data->rst)) {
		dev_warn(dev, "Release sensor's reset...\n");
		if (reset_control_deassert(data->rst))
			dev_err(dev, "Release sensor's reset failed!!!...\n");
	}
	meson_tsensor_hw_resume(dev);

	return 0;
}
#endif

static const struct dev_pm_ops meson_tsensor_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(meson_tsensor_suspend,
				meson_tsensor_resume)
};

static void meson_tsensor_shutdown(struct platform_device *pdev)
{
	meson_tsensor_suspend(&pdev->dev);
}

struct platform_driver meson_tsensor_driver = {
	.driver = {
		.name   = "meson-tsensor",
		.pm     = &meson_tsensor_pm_ops,
		.of_match_table = meson_tsensor_match,
#if IS_ENABLED(CONFIG_AMLOGIC_BOOT_TIME)
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
#endif
	},
	.probe	= meson_tsensor_probe,
	.remove	= meson_tsensor_remove,
	.shutdown = meson_tsensor_shutdown,
};
