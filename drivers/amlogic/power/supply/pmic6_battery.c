// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/power_supply.h>
#include <linux/list.h>
#include <asm/div64.h>
#include <linux/regmap.h>

#include <linux/amlogic/pmic/meson_pmic6.h>

#define SAR_IRQ_TIMEOUT		5000

struct pmic6_bat {
	struct regmap_irq_chip_data *regmap_irq;
	struct device *dev;
	struct regmap *regmap;
	struct mutex lock; /*protect operation propval*/

	struct power_supply *battery;
	struct power_supply *charger;
	struct work_struct char_work;
	struct delayed_work work;
	int status;
	int soc;
	atomic_t sar_irq;

	int internal_resist;
	int total_cap;
	int init_cap;
	int alarm_cap;
	int init_clbcnt;
	int max_volt;
	int min_volt;
	int table_len;
	struct power_supply_battery_ocv_table *cap_table;
};

static char *pmic6_bat_irqs[] = {
	"NTC_UNT",
	"NTC_OVT",
	"CHG_DCIN_UV_LV",
	"CHG_DCIN_OV_LV",
	"CHG_OTP",
	"CHG_OCP",
	"CHG_OVP",
	"CHG_DCIN_OK",
	"CHG_TIMEOUT",
	"CHG_CHGEND",
	"SAR",
};

static int pmic6_bat_irq_bits[] = {
	PMIC6_IRQ_NTC_UNT,
	PMIC6_IRQ_NTC_OVT,
	PMIC6_IRQ_CHG_DCIN_UV_LV,
	PMIC6_IRQ_CHG_DCIN_OV_LV,
	PMIC6_IRQ_CHG_OTP,
	PMIC6_IRQ_CHG_OCP,
	PMIC6_IRQ_CHG_OVP,
	PMIC6_IRQ_CHG_DCIN_OK,
	PMIC6_IRQ_CHG_TIMEOUT,
	PMIC6_IRQ_CHG_CHGEND,
	PMIC6_IRQ_SAR,
};

static irqreturn_t pmic6_ntc_unt_irq_handle(int irq, void *data)
{
	struct pmic6_bat *bat = data;
	int ret;

	/*clear ntc_unt irq*/
	ret = regmap_update_bits(bat->regmap,
				 PMIC6_IRQ_MASK0,
				 BIT(0), 0);
	if (ret)
		dev_err(bat->dev,
			"Failed to read status: %d\n", __LINE__);

	printk_once("battery temperature too low.\n");

	return IRQ_HANDLED;
}

static irqreturn_t pmic6_ntc_ovt_irq_handle(int irq, void *data)
{
	struct pmic6_bat *bat = data;
	int ret;

	/*clear ntc_ovt irq*/
	ret = regmap_update_bits(bat->regmap,
				 PMIC6_IRQ_MASK0,
				 BIT(1), 0);
	if (ret)
		dev_err(bat->dev,
			"Failed to read status: %d\n", __LINE__);

	printk_once("battery temperature too high.\n");

	return IRQ_HANDLED;
}

static irqreturn_t pmic6_chg_dcin_uv_lv_irq_handle(int irq, void *data)
{
	struct pmic6_bat *bat = data;
	int ret;

	/*clear chg_dcin_uv_lv irq*/
	ret = regmap_update_bits(bat->regmap,
				 PMIC6_IRQ_MASK0,
				 BIT(2), 0);
	if (ret)
		dev_err(bat->dev,
			"Failed to read status: %d\n", __LINE__);

	printk_once("dcin voltage too low.\n");

	return IRQ_HANDLED;
}

static irqreturn_t pmic6_chg_dcin_ov_lv_irq_handle(int irq, void *data)
{
	struct pmic6_bat *bat = data;
	int ret;

	/*clear chg_dcin_ov_lv irq*/
	ret = regmap_update_bits(bat->regmap,
				 PMIC6_IRQ_MASK0,
				 BIT(3), 0);
	if (ret)
		dev_err(bat->dev,
			"Failed to read status: %d\n", __LINE__);

	printk_once("dcin voltage too high.\n");

	return IRQ_HANDLED;
}

static irqreturn_t pmic6_chg_otp_irq_handle(int irq, void *data)
{
	struct pmic6_bat *bat = data;
	int ret;

	/*clear chg_otp irq*/
	ret = regmap_update_bits(bat->regmap,
				 PMIC6_IRQ_MASK0,
				 BIT(4), 0);
	if (ret)
		dev_err(bat->dev,
			"Failed to read status: %d\n", __LINE__);

	printk_once("charger temperature too high.\n");

	return IRQ_HANDLED;
}

static irqreturn_t pmic6_chg_ocp_irq_handle(int irq, void *data)
{
	struct pmic6_bat *bat = data;
	int ret;

	/*clear chg_ocp irq*/
	ret = regmap_update_bits(bat->regmap,
				 PMIC6_IRQ_MASK0,
				 BIT(5), 0);
	if (ret)
		dev_err(bat->dev,
			"Failed to read status: %d\n", __LINE__);

	printk_once("charger current too high.\n");

	return IRQ_HANDLED;
}

static irqreturn_t pmic6_chg_ovp_irq_handle(int irq, void *data)
{
	struct pmic6_bat *bat = data;
	int ret;

	/*clear chg_ovp irq*/
	ret = regmap_update_bits(bat->regmap,
				 PMIC6_IRQ_MASK0,
				 BIT(6), 0);
	if (ret)
		dev_err(bat->dev,
			"Failed to read status: %d\n", __LINE__);

	printk_once("charger voltage too high.\n");

	return IRQ_HANDLED;
}

static irqreturn_t pmic6_chg_dcin_ok_irq_handle(int irq, void *data)
{
	struct pmic6_bat *bat = data;
	int ret;

	/*clear chg_dcin_ok irq*/
	ret = regmap_update_bits(bat->regmap,
				 PMIC6_IRQ_MASK0,
				 BIT(7), 0);
	if (ret)
		dev_err(bat->dev,
			"Failed to read status: %d\n", __LINE__);

	/*schedule_work(&bat->char_work);*/

	return IRQ_HANDLED;
}

static irqreturn_t pmic6_chg_timeout_irq_handle(int irq, void *data)
{
	struct pmic6_bat *bat = data;
	int ret;

	/*clear chg_timeout irq*/
	ret = regmap_update_bits(bat->regmap,
				 PMIC6_IRQ_MASK1,
				 BIT(0), 0);
	if (ret)
		dev_err(bat->dev,
			"Failed to read status: %d\n", __LINE__);

	printk_once("charger timeout.\n");

	return IRQ_HANDLED;
}

static irqreturn_t pmic6_chg_end_irq_handle(int irq, void *data)
{
	struct pmic6_bat *bat = data;
	int ret;

	/*clear chg_end irq*/
	ret = regmap_update_bits(bat->regmap,
				 PMIC6_IRQ_MASK1,
				 BIT(1), 0);
	if (ret)
		dev_err(bat->dev,
			"Failed to read status: %d\n", __LINE__);

	return IRQ_HANDLED;
}

static irqreturn_t pmic6_sar_irq_handle(int irq, void *data)
{
	struct pmic6_bat *bat = data;
	int ret;

	/*clear sar irq*/
	ret = regmap_update_bits(bat->regmap,
				 PMIC6_IRQ_MASK3,
				 BIT(7), 0);
	if (ret)
		dev_err(bat->dev,
			"Failed to read status: %d\n", __LINE__);

	atomic_inc(&bat->sar_irq);

	return IRQ_HANDLED;
}

static irq_handler_t pmic6_bat_irq_handle[] = {
	pmic6_ntc_unt_irq_handle,
	pmic6_ntc_ovt_irq_handle,
	pmic6_chg_dcin_uv_lv_irq_handle,
	pmic6_chg_dcin_ov_lv_irq_handle,
	pmic6_chg_otp_irq_handle,
	pmic6_chg_ocp_irq_handle,
	pmic6_chg_ovp_irq_handle,
	pmic6_chg_dcin_ok_irq_handle,
	pmic6_chg_timeout_irq_handle,
	pmic6_chg_end_irq_handle,
	pmic6_sar_irq_handle,
};

static int pmic6_bat_enable_fg(struct pmic6_bat *bat)
{
	int ret;

	/* Enable the FGU module */
	ret = regmap_write(bat->regmap, PMIC6_SAR_CNTL_REG5, 0x05);
	if (ret < 0) {
		dev_dbg(bat->dev,
			"failed in func: %s line: %d\n", __func__, __LINE__);
		return ret;
	}

	ret = regmap_write(bat->regmap, PMIC6_SAR_CNTL_REG1, 0x40);
	if (ret < 0) {
		dev_dbg(bat->dev,
			"failed in func: %s line: %d\n", __func__, __LINE__);
		return ret;
	}

	ret = regmap_write(bat->regmap, PMIC6_SAR_CNTL_REG0, 0x1e);
	if (ret < 0) {
		dev_dbg(bat->dev,
			"failed in func: %s line: %d\n", __func__, __LINE__);
		return ret;
	}

	ret = regmap_write(bat->regmap, PMIC6_SAR_CNTL_REG6, 0x01);
	if (ret < 0) {
		dev_dbg(bat->dev,
			"failed in func: %s line: %d\n", __func__, __LINE__);
		return ret;
	}

	return 0;
}

static int pmic6_bat_present(struct pmic6_bat *bat,
			     union power_supply_propval *val)
{
	u32 value;
	int ret = 0;

	/*update sp_dcdc_status*/
	ret = regmap_write(bat->regmap, PMIC6_GEN_CNTL0, 0x8);
	if (ret < 0) {
		dev_dbg(bat->dev,
			"failed in func: %s line: %d\n", __func__, __LINE__);
		return ret;
	}

	ret = regmap_read(bat->regmap, PMIC6_SP_CHARGER_STATUS1,
			  &value);
	if (ret < 0)
		dev_dbg(bat->dev,
			"failed in func: %s line: %d\n", __func__, __LINE__);
	if (value & BIT(7))
		val->intval = 1;
	else
		val->intval = 0;

	return ret;
}

static int pmic6_bat_voltage_now(struct pmic6_bat *bat, int *val)
{
	u8 reg[2] = {0};
	u32 value;
	u64 temp;
	int ret;
	u64 div;
	int count = 0;
	unsigned long delay = 0;

	ret = regmap_write(bat->regmap, PMIC6_SAR_CNTL_REG1, 0x4f);
	if (ret < 0) {
		dev_dbg(bat->dev,
			"failed in func: %s line: %d\n", __func__, __LINE__);
		return ret;
	}

	ret = regmap_write(bat->regmap, PMIC6_SAR_CNTL_REG0, 0x14);
	if (ret < 0) {
		dev_dbg(bat->dev,
			"failed in func: %s line: %d\n", __func__, __LINE__);
		return ret;
	}

	ret = regmap_write(bat->regmap, PMIC6_SAR_CNTL_REG16, 0x01);
	if (ret < 0) {
		dev_dbg(bat->dev,
			"failed in func: %s line: %d\n", __func__, __LINE__);
		return ret;
	}

	ret = regmap_write(bat->regmap, PMIC6_SAR_CNTL_REG14, 0xcf);
	if (ret < 0) {
		dev_dbg(bat->dev,
			"failed in func: %s line: %d\n", __func__, __LINE__);
		return ret;
	}

	ret = regmap_write(bat->regmap, PMIC6_SAR_CNTL_REG15, 0xc0);
	if (ret < 0) {
		dev_dbg(bat->dev,
			"failed in func: %s line: %d\n", __func__, __LINE__);
		return ret;
	}

	ret = regmap_write(bat->regmap, PMIC6_SAR_CNTL_REG2, 0x04);
	if (ret < 0) {
		dev_dbg(bat->dev,
			"failed in func: %s line: %d\n", __func__, __LINE__);
		return ret;
	}

	/*walk round disturb the coulombmeter counter*/
	atomic_set(&bat->sar_irq, 0);
	/*enable sar irq*/
	ret = regmap_read(bat->regmap,
			  PMIC6_IRQ_MASK3, &value);
	if (ret < 0)
		dev_dbg(bat->dev, "failed in line: %d\n", __LINE__);

	value |= BIT(7);
	ret = regmap_write(bat->regmap, PMIC6_IRQ_MASK3, value);
	if (ret)
		dev_err(bat->dev,
			"Failed to read status: %d\n", __LINE__);

	/*ensure sar_irq interrupt after*/
	delay = jiffies + msecs_to_jiffies(SAR_IRQ_TIMEOUT);
	while (1) {
		if (time_after(jiffies, delay)) {
			dev_err(bat->dev, "Failed read sar_irq\n");
			break;
		}

		count = atomic_read(&bat->sar_irq);
		if (count) {
			ret = regmap_write(bat->regmap,
					   PMIC6_SAR_SW_EN_FIELD, 0x08);
			if (ret)
				dev_err(bat->dev,
					"Failed in line: %d\n", __LINE__);
			break;
		}
	}

	ret = regmap_bulk_read(bat->regmap, PMIC6_SAR_RD_MANUAL, reg, 2);
	if (ret < 0) {
		dev_dbg(bat->dev,
			"failed in func: %s line: %d\n", __func__, __LINE__);
		return ret;
	}
	temp = ((reg[1] & 0x1f) << 8) | reg[0];
	div = temp * 4800UL;
	div >>= 12;
	*val = (int)(div); //mV

	return 0;
}

static int pmic6_bat_health(struct pmic6_bat *bat,
			    union power_supply_propval *val)
{
	int vol;
	int ret;

	ret = pmic6_bat_voltage_now(bat, &vol);
	if (ret)
		return ret;

	if (vol > bat->max_volt)
		val->intval = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
	else
		val->intval = POWER_SUPPLY_HEALTH_GOOD;

	return 0;
}

static int pmic6_bat_temp(struct pmic6_bat *bat,
			  union power_supply_propval *val)
{
	u8 reg[2] = {0};
	u32 value;
	int ret;
	int count = 0;
	unsigned long delay = 0;

	ret = regmap_write(bat->regmap, PMIC6_SAR_CNTL_REG0, 0x16);
	if (ret < 0) {
		dev_dbg(bat->dev,
			"failed in func: %s line: %d\n", __func__, __LINE__);
		return ret;
	}

	ret = regmap_write(bat->regmap, PMIC6_SAR_CNTL_REG2, 0x04);
	if (ret < 0) {
		dev_dbg(bat->dev,
			"failed in func: %s line: %d\n", __func__, __LINE__);
		return ret;
	}

	ret = regmap_write(bat->regmap, PMIC6_ANALOG_REG4, 0x20);
	if (ret < 0) {
		dev_dbg(bat->dev,
			"failed in func: %s line: %d\n", __func__, __LINE__);
		return ret;
	}

	ret = regmap_write(bat->regmap, PMIC6_SAR_CNTL_REG19, 0x10);
	if (ret < 0) {
		dev_dbg(bat->dev,
			"failed in func: %s line: %d\n", __func__, __LINE__);
		return ret;
	}

	ret = regmap_write(bat->regmap, PMIC6_SAR_CNTL_REG15, 0xac);
	if (ret < 0) {
		dev_dbg(bat->dev,
			"failed in func: %s line: %d\n", __func__, __LINE__);
		return ret;
	}

	/*walk round disturb the coulombmeter counter*/
	atomic_set(&bat->sar_irq, 0);
	/*enable sar irq*/
	ret = regmap_read(bat->regmap,
			  PMIC6_IRQ_MASK3, &value);
	if (ret < 0)
		dev_dbg(bat->dev, "failed in line: %d\n", __LINE__);

	value |= BIT(7);
	ret = regmap_write(bat->regmap, PMIC6_IRQ_MASK3, value);
	if (ret)
		dev_err(bat->dev,
			"Failed to read status: %d\n", __LINE__);

	/*ensure sar_irq interrupt after*/
	delay = jiffies + msecs_to_jiffies(SAR_IRQ_TIMEOUT);
	while (1) {
		if (time_after(jiffies, delay)) {
			dev_err(bat->dev, "Failed read sar_irq\n");
			break;
		}

		count = atomic_read(&bat->sar_irq);
		if (count) {
			ret = regmap_write(bat->regmap,
					   PMIC6_SAR_SW_EN_FIELD, 0x08);
			if (ret)
				dev_err(bat->dev,
					"Failed in line: %d\n", __LINE__);
			break;
		}
	}
	ret = regmap_bulk_read(bat->regmap, PMIC6_SAR_RD_RAW, reg, 2);
	if (ret < 0) {
		dev_dbg(bat->dev,
			"failed in func: %s line: %d\n", __func__, __LINE__);
		return ret;
	}
	value = ((reg[1] & 0x1f) << 8) | reg[0];
	dev_info(bat->dev,  "bat temp 0x%x\n", value);

	return 0;
}

static int pmic6_bat_current_now(struct pmic6_bat *bat, int *val)
{
	u8 reg[2] = {0};
	u32 value;
	int ret;
	int sign_bit;
	u64 temp;
	u64 div;
	int count = 0;
	unsigned long delay = 0;

	ret = regmap_write(bat->regmap, PMIC6_SAR_CNTL_REG1, 0x4f);
	if (ret < 0) {
		dev_dbg(bat->dev,
			"failed in func: %s line: %d\n", __func__, __LINE__);
		return ret;
	}

	ret = regmap_write(bat->regmap, PMIC6_SAR_CNTL_REG0, 0x14);
	if (ret < 0) {
		dev_dbg(bat->dev,
			"failed in func: %s line: %d\n", __func__, __LINE__);
		return ret;
	}

	ret = regmap_write(bat->regmap, PMIC6_SAR_CNTL_REG16, 0x01);
	if (ret < 0) {
		dev_dbg(bat->dev,
			"failed in func: %s line: %d\n", __func__, __LINE__);
		return ret;
	}

	ret = regmap_write(bat->regmap, PMIC6_SAR_CNTL_REG9, 0xe0);
	if (ret < 0) {
		dev_dbg(bat->dev,
			"failed in func: %s line: %d\n", __func__, __LINE__);
		return ret;
	}

	ret = regmap_write(bat->regmap, PMIC6_SAR_CNTL_REG2, 0x04);
	if (ret < 0) {
		dev_dbg(bat->dev,
			"failed in func: %s line: %d\n", __func__, __LINE__);
		return ret;
	}

	ret = regmap_write(bat->regmap, PMIC6_SAR_CNTL_REG9, 0xe0);
	if (ret < 0) {
		dev_dbg(bat->dev,
			"failed in func: %s line: %d\n", __func__, __LINE__);
		return ret;
	}

	/*walk round disturb the coulombmeter counter*/
	atomic_set(&bat->sar_irq, 0);
	/*enable sar irq*/
	ret = regmap_read(bat->regmap,
			  PMIC6_IRQ_MASK3, &value);
	if (ret < 0)
		dev_dbg(bat->dev, "failed in line: %d\n", __LINE__);

	value |= BIT(7);
	ret = regmap_write(bat->regmap, PMIC6_IRQ_MASK3, value);
	if (ret)
		dev_err(bat->dev,
			"Failed to read status: %d\n", __LINE__);

	/*ensure sar_irq interrupt after*/
	delay = jiffies + msecs_to_jiffies(SAR_IRQ_TIMEOUT);
	while (1) {
		if (time_after(jiffies, delay)) {
			dev_err(bat->dev, "Failed read sar_irq\n");
			break;
		}

		count = atomic_read(&bat->sar_irq);
		if (count) {
			ret = regmap_write(bat->regmap,
					   PMIC6_SAR_SW_EN_FIELD, 0x21);
			if (ret)
				dev_err(bat->dev,
					"Failed in line: %d\n", __LINE__);
			break;
		}
	}
	ret = regmap_bulk_read(bat->regmap, PMIC6_SAR_RD_IBAT_LAST, reg, 2);
	if (ret < 0) {
		dev_dbg(bat->dev,
			"failed in func: %s line: %d\n", __func__, __LINE__);
		return ret;
	}
	temp = ((reg[1] & 0x1f) << 8) | reg[0];
	sign_bit = temp & 0x1000;
	if (temp & 0x1000)
		temp = (temp ^ 0x1fff) + 1;
	div = (u64)(temp * 5333UL);
	div >>= 12;
	*val = (int)(div) * (sign_bit ? 1 : -1); //mV

	return 0;
}

static int pmic6_bat_voltage_ocv(struct pmic6_bat *bat, int *val)
{
	int value;
	int ret;
	int vol, cur;

	ret = pmic6_bat_voltage_now(bat, &vol);
	if (ret)
		return ret;
	ret = pmic6_bat_current_now(bat, &cur);
	if (ret)
		return ret;
	value = vol - cur * bat->internal_resist;
	*val = value;	//mV
	return 0;
}

static int pmic6_bat_capacity(struct pmic6_bat *bat,
			      union power_supply_propval *val)
{
	struct power_supply_battery_ocv_table *table = bat->cap_table;
	int vbat_ocv;
	int ocv_lower = 0;
	int ocv_upper = 0;
	int capacity_lower = 0;
	int capacity_upper = 0;
	int ret, i, tmp, flag;

	ret = pmic6_bat_voltage_ocv(bat, &vbat_ocv);
	if (ret)
		return ret;

	if ((vbat_ocv * 1000) >= table[0].ocv) {
		val->intval = 100;
		return 0;
	}

	if ((vbat_ocv * 1000) <= table[bat->table_len - 1].ocv) {
		val->intval = 0;
		return 0;
	}

	flag = 0;
	for (i = 0; i < bat->table_len - 1; i++) {
		if (((vbat_ocv * 1000) < table[i].ocv) &&
		    ((vbat_ocv * 1000) > table[i + 1].ocv)) {
			ocv_lower = table[i + 1].ocv;
			ocv_upper = table[i].ocv;
			capacity_lower = table[i + 1].capacity;
			capacity_upper = table[i].capacity;
			flag = 1;
			break;
		}
	}

	if (!flag)
		return -EAGAIN;

	tmp = capacity_upper - capacity_lower;
	tmp = DIV_ROUND_CLOSEST(((vbat_ocv * 1000 - ocv_lower) * tmp),
				(ocv_upper - ocv_lower));
	val->intval = capacity_lower + tmp;
	return 0;
}

static enum power_supply_property pmic6_bat_props[] = {
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_OCV,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_TEMP,
};

static int pmic6_bat_get_prop(struct power_supply *psy,
			      enum power_supply_property psp,
			      union power_supply_propval *val)
{
	struct pmic6_bat *bat = dev_get_drvdata(psy->dev.parent);
	int value;
	int ret;

	mutex_lock(&bat->lock);

	switch (psp) {
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		ret = pmic6_bat_capacity(bat, val);
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		ret = pmic6_bat_present(bat, val);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		ret = pmic6_bat_voltage_now(bat, &value);
		if (ret)
			return ret;
		val->intval = value * 1000; //uV
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		ret = pmic6_bat_health(bat, val);
		break;
	case POWER_SUPPLY_PROP_TEMP:
		ret = pmic6_bat_temp(bat, val);
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		ret = pmic6_bat_current_now(bat, &value);
		if (ret)
			return ret;
		val->intval = value * 1000; //uA
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_OCV:
		ret = pmic6_bat_voltage_ocv(bat, &value);
		if (ret)
			return ret;
		val->intval = value * 1000; //uV
		break;
	default:
		ret = -EINVAL;
		break;
	}

	mutex_unlock(&bat->lock);
	return 0;
}

static inline int pmic6_charger_online(struct pmic6_bat *charger,
				       union power_supply_propval *val)
{
	u32 value;
	int ret = 0;

	/*update sp_dcdc_status*/
	ret = regmap_write(charger->regmap, PMIC6_GEN_CNTL0, 0x8);
	if (ret < 0) {
		dev_dbg(charger->dev,
			"failed in func: %s line: %d\n", __func__, __LINE__);
		return ret;
	}

	ret = regmap_read(charger->regmap, PMIC6_SP_CHARGER_STATUS2,
			  &value);
	if (ret < 0)
		dev_dbg(charger->dev,
			"failed in func: %s line: %d\n", __func__, __LINE__);
	if (value & BIT(4))
		val->intval = 1;
	else
		val->intval = 0;

	return ret;
}

static inline int pmic6_get_charger_voltage(struct pmic6_bat *charger,
					    union power_supply_propval *val)
{
	u8 reg[2] = {0};
	u32 value;
	int ret;
	u64 temp;
	u64 div;
	int count = 0;
	unsigned long delay = 0;

	ret = regmap_write(charger->regmap, PMIC6_SAR_CNTL_REG0, 0x16);
	if (ret < 0) {
		dev_dbg(charger->dev,
			"failed in func: %s line: %d\n", __func__, __LINE__);
		return ret;
	}

	ret = regmap_write(charger->regmap, PMIC6_SAR_CNTL_REG2, 0x04);
	if (ret < 0) {
		dev_dbg(charger->dev,
			"failed in func: %s line: %d\n", __func__, __LINE__);
		return ret;
	}

	ret = regmap_write(charger->regmap, PMIC6_SAR_CNTL_REG14, 0x8f);
	if (ret < 0) {
		dev_dbg(charger->dev,
			"failed in func: %s line: %d\n", __func__, __LINE__);
		return ret;
	}

	ret = regmap_write(charger->regmap, PMIC6_SAR_CNTL_REG15, 0xc1);
	if (ret < 0) {
		dev_dbg(charger->dev,
			"failed in func: %s line: %d\n", __func__, __LINE__);
		return ret;
	}

	/*walk round disturb the coulombmeter counter*/
	atomic_set(&charger->sar_irq, 0);
	/*enable sar irq*/
	ret = regmap_read(charger->regmap,
			  PMIC6_IRQ_MASK3, &value);
	if (ret < 0)
		dev_dbg(charger->dev, "failed in line: %d\n", __LINE__);

	value |= BIT(7);
	ret = regmap_write(charger->regmap, PMIC6_IRQ_MASK3, value);
	if (ret)
		dev_err(charger->dev,
			"Failed to read status: %d\n", __LINE__);

	/*ensure sar_irq interrupt after*/
	delay = jiffies + msecs_to_jiffies(SAR_IRQ_TIMEOUT);
	while (1) {
		if (time_after(jiffies, delay)) {
			dev_err(charger->dev, "Failed read sar_irq\n");
			break;
		}
		count = atomic_read(&charger->sar_irq);
		if (count) {
			ret = regmap_write(charger->regmap,
					   PMIC6_SAR_SW_EN_FIELD, 0x08);
			if (ret)
				dev_err(charger->dev,
					"Failed in line: %d\n", __LINE__);
			break;
		}
	}

	ret = regmap_bulk_read(charger->regmap, PMIC6_SAR_RD_RAW, reg, 2);
	if (ret < 0) {
		dev_dbg(charger->dev,
			"failed in func: %s line: %d\n", __func__, __LINE__);
		return ret;
	}
	temp = ((reg[1] & 0x1f) << 8) | reg[0];
	div = temp * 1600UL;
	div *= 1000 * 8;
	div >>= 12;
	val->intval = (int)(div); //uV

	return 0;
}

static inline int pmic6_get_charger_current(struct pmic6_bat *charger,
					    union power_supply_propval *val)
{
	u32 value;
	int ret;

	ret = regmap_read(charger->regmap, PMIC6_OTP_REG_0x57, &value);
	if (ret < 0) {
		dev_dbg(charger->dev,
			"failed in func: %s line: %d\n", __func__, __LINE__);
		return ret;
	}

	switch (value & 0xf8) {
	case 0x0:
		val->intval = 200 * 1000; /*uA*/
		break;
	case 0x8:
		val->intval = 350 * 1000;
		break;
	case 0x10:
		val->intval = 500 * 1000;
		break;
	case 0x18:
		val->intval = 650 * 1000;
		break;
	case 0x40:
		val->intval = 800 * 1000;
		break;
	case 0x48:
		val->intval = 950 * 1000;
		break;
	case 0x50:
		val->intval = 1100 * 1000;
		break;
	case 0x58:
		val->intval = 1250 * 1000;
		break;
	case 0x60:
		val->intval = 1400 * 1000;
		break;
	case 0x68:
		val->intval = 1550 * 1000;
		break;
	case 0x70:
		val->intval = 1700 * 1000;
		break;
	case 0x78:
		val->intval = 1850 * 1000;
		break;
	case 0x80:
		val->intval = 2000 * 1000;
		break;
	default:
		dev_dbg(charger->dev, "argument not true: %d\n", __LINE__);
		break;
	}

	return 0;
}

static inline int pmic6_get_charger_status(struct pmic6_bat *charger,
					   union power_supply_propval *val)
{
	u32 value;
	int ret = 0;

	/*update sp_dcdc_status*/
	ret = regmap_write(charger->regmap, PMIC6_GEN_CNTL0, 0x8);
	if (ret < 0) {
		dev_dbg(charger->dev,
			"failed in func: %s line: %d\n", __func__, __LINE__);
		return ret;
	}

	ret = regmap_read(charger->regmap, PMIC6_SP_CHARGER_STATUS2,
			  &value);
	if (ret < 0)
		dev_dbg(charger->dev,
			"failed in func: %s line: %d\n", __func__, __LINE__);
	if (value & BIT(2))
		val->intval = POWER_SUPPLY_STATUS_CHARGING;
	else if (value & BIT(1))
		val->intval = POWER_SUPPLY_STATUS_FULL;
	else if (!(value & BIT(4)))
		val->intval = POWER_SUPPLY_STATUS_NOT_CHARGING;
	else
		val->intval = POWER_SUPPLY_STATUS_UNKNOWN;

	return ret;
}

static inline
int pmic6_get_charger_current_limit(struct pmic6_bat *charger,
				    union power_supply_propval *val)
{
	u8 reg[2] = {0};
	u32 value;
	int ret;
	u64 temp;
	u64 div;
	int count = 0;
	unsigned long delay = 0;

	ret = regmap_write(charger->regmap, PMIC6_SAR_CNTL_REG0, 0x16);
	if (ret < 0) {
		dev_dbg(charger->dev,
			"failed in func: %s line: %d\n", __func__, __LINE__);
		return ret;
	}

	ret = regmap_write(charger->regmap, PMIC6_SAR_CNTL_REG19, 0x0a);
	if (ret < 0) {
		dev_dbg(charger->dev,
			"failed in func: %s line: %d\n", __func__, __LINE__);
		return ret;
	}

	ret = regmap_write(charger->regmap, PMIC6_SAR_CNTL_REG2, 0x04);
	if (ret < 0) {
		dev_dbg(charger->dev,
			"failed in func: %s line: %d\n", __func__, __LINE__);
		return ret;
	}

	ret = regmap_write(charger->regmap, PMIC6_SAR_CNTL_REG3, 0x02);
	if (ret < 0) {
		dev_dbg(charger->dev,
			"failed in func: %s line: %d\n", __func__, __LINE__);
		return ret;
	}

	ret = regmap_write(charger->regmap, PMIC6_SAR_CNTL_REG15, 0x20);
	if (ret < 0) {
		dev_dbg(charger->dev,
			"failed in func: %s line: %d\n", __func__, __LINE__);
		return ret;
	}

	ret = regmap_write(charger->regmap, PMIC6_OTP_REG_0x66, 0x77);
	if (ret < 0) {
		dev_dbg(charger->dev,
			"failed in func: %s line: %d\n", __func__, __LINE__);
		return ret;
	}

	/*walk round disturb the coulombmeter counter*/
	atomic_set(&charger->sar_irq, 0);
	/*enable sar irq*/
	ret = regmap_read(charger->regmap,
			  PMIC6_IRQ_MASK3, &value);
	if (ret < 0)
		dev_dbg(charger->dev, "failed in line: %d\n", __LINE__);

	value |= BIT(7);
	ret = regmap_write(charger->regmap, PMIC6_IRQ_MASK3, value);
	if (ret)
		dev_err(charger->dev,
			"Failed to read status: %d\n", __LINE__);

	/*ensure sar_irq interrupt after*/
	delay = jiffies + msecs_to_jiffies(SAR_IRQ_TIMEOUT);
	while (1) {
		if (time_after(jiffies, delay)) {
			dev_err(charger->dev, "Failed read sar_irq\n");
			break;
		}

		count = atomic_read(&charger->sar_irq);
		if (count) {
			ret = regmap_write(charger->regmap,
					   PMIC6_SAR_SW_EN_FIELD, 0x08);
			if (ret)
				dev_err(charger->dev,
					"Failed in line: %d\n", __LINE__);
			break;
		}
	}

	ret = regmap_bulk_read(charger->regmap, PMIC6_SAR_RD_RAW, reg, 2);
	if (ret < 0) {
		dev_dbg(charger->dev,
			"failed in func: %s line: %d\n", __func__, __LINE__);
		return ret;
	}
	temp = ((reg[1] & 0x1f) << 8) | reg[0];
	div = temp * 1600UL;
	div *= 1000 * 1000;
	div >>= 12;
	do_div(div, 43120);
	div = div * 10000;
	do_div(div, 85);
	val->intval = (int)(div); //uA

	return 0;
}

static int pmic6_charger_get_prop(struct power_supply *psy,
				  enum power_supply_property psp,
				  union power_supply_propval *val)
{
	struct pmic6_bat *charger = dev_get_drvdata(psy->dev.parent);
	int ret;

	mutex_lock(&charger->lock);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		ret = pmic6_charger_online(charger, val);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		ret = pmic6_get_charger_current(charger, val);
		break;
	case POWER_SUPPLY_PROP_STATUS:
		ret = pmic6_get_charger_status(charger, val);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		ret = pmic6_get_charger_voltage(charger, val);
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		ret = pmic6_get_charger_current_limit(charger, val);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	mutex_unlock(&charger->lock);
	return ret;
}

static inline int pmic6_set_charger_current(struct pmic6_bat *charger,
					    int val)
{
	int ret;
	u32 temp;

	ret = regmap_read(charger->regmap, PMIC6_OTP_REG_0x57, &temp);
	if (ret < 0) {
		dev_dbg(charger->dev,
			"failed in func: %s line: %d\n", __func__, __LINE__);
		return ret;
	}
	/* Convert from uA to mA */
	val /= 1000;
	switch (val) {
	case 200:
		temp = (temp & (~0xf8)) | (0x0); /*uA*/
		break;
	case 350:
		temp = (temp & (~0xf8)) | (0x8);
		break;
	case 500:
		temp = (temp & (~0xf8)) | (0x10);
		break;
	case 650:
		temp = (temp & (~0xf8)) | (0x18);
		break;
	case 800:
		temp = (temp & (~0xf8)) | (0x40);
		break;
	case 950:
		temp = (temp & (~0xf8)) | (0x48);
		break;
	case 1100:
		temp = (temp & (~0xf8)) | (0x50);
		break;
	case 1250:
		temp = (temp & (~0xf8)) | (0x58);
		break;
	case 1400:
		temp = (temp & (~0xf8)) | (0x60);
		break;
	case 1550:
		temp = (temp & (~0xf8)) | (0x68);
		break;
	case 1700:
		temp = (temp & (~0xf8)) | (0x70);
		break;
	case 1850:
		temp = (temp & (~0xf8)) | (0x78);
		break;
	case 2000:
		temp = (temp & (~0xf8)) | (0x80);
		break;
	default:
		dev_dbg(charger->dev,
			"set current value false %d\n", __LINE__);
		break;
	}

	ret = regmap_write(charger->regmap, PMIC6_OTP_REG_0x57, temp);
	if (ret < 0) {
		dev_dbg(charger->dev,
			"failed in func: %s line: %d\n", __func__, __LINE__);
		return ret;
	}

	return 0;
}

static int pmic6_charger_set_prop(struct power_supply *psy,
				  enum power_supply_property psp,
				  const union power_supply_propval *val)
{
	struct pmic6_bat *charger = dev_get_drvdata(psy->dev.parent);
	int ret;

	mutex_lock(&charger->lock);

	switch (psp) {
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		ret = pmic6_set_charger_current(charger, val->intval);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	mutex_unlock(&charger->lock);
	return ret;
}

static int pmic6_charger_writable_property(struct power_supply *psy,
					   enum power_supply_property psp)
{
	int ret;

	switch (psp) {
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		ret = 1;
		break;

	default:
		ret = 0;
	}

	return ret;
}

static enum power_supply_property pmic6_charger_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
};

static const struct power_supply_desc pmic6_charger_desc = {
	.name		= "pmic6-charger",
	.type		= POWER_SUPPLY_TYPE_MAINS,
	.properties	= pmic6_charger_props,
	.num_properties	= ARRAY_SIZE(pmic6_charger_props),
	.get_property	= pmic6_charger_get_prop,
	.set_property	= pmic6_charger_set_prop,
	.property_is_writeable = pmic6_charger_writable_property,
};

static const struct power_supply_desc pmic6_bat_desc = {
	.name		= "pmic6-bat",
	.type		= POWER_SUPPLY_TYPE_BATTERY,
	.properties	= pmic6_bat_props,
	.num_properties	= ARRAY_SIZE(pmic6_bat_props),
	.get_property	= pmic6_bat_get_prop,
};

int pmic6_bat_hw_init(struct pmic6_bat *bat)
{
	int ret = 0;
	struct power_supply_battery_info info = { };
	struct power_supply_battery_ocv_table *table;

	ret = power_supply_get_battery_info(bat->battery, &info);
	if (ret) {
		dev_err(bat->dev, "failed to get battery information\n");
		return ret;
	}
	bat->total_cap = info.charge_full_design_uah / 1000;
	bat->max_volt = info.constant_charge_voltage_max_uv / 1000;
	bat->internal_resist = info.factory_internal_resistance_uohm / 1000;
	bat->min_volt = info.voltage_min_design_uv;

	table = power_supply_find_ocv2cap_table(&info, 20, &bat->table_len);
	if (!table)
		return -EINVAL;

	bat->cap_table = devm_kmemdup(bat->dev, table,
				      bat->table_len * sizeof(*table),
				      GFP_KERNEL);
	if (!bat->cap_table) {
		power_supply_put_battery_info(bat->battery, &info);
		return -ENOMEM;
	}

	bat->alarm_cap = power_supply_ocv2cap_simple(bat->cap_table,
						     bat->table_len,
						     bat->min_volt);

	power_supply_put_battery_info(bat->battery, &info);

	return ret;
}

static int pmic6_map_irq(struct pmic6_bat *bat, int irq)
{
	return regmap_irq_get_virq(bat->regmap_irq, irq);
}

int pmic6_request_irq(struct pmic6_bat *bat, int irq, char *name,
		      irq_handler_t handler, void *data)
{
	int virq;

	virq = pmic6_map_irq(bat, irq);
	if (virq < 0)
		return virq;

	return devm_request_threaded_irq(bat->dev, virq, NULL, handler,
					 IRQF_TRIGGER_HIGH | IRQF_ONESHOT,
					 name, data);
}

static int pmic6_bat_probe(struct platform_device *pdev)
{
	struct meson_pmic *pmic6 = dev_get_drvdata(pdev->dev.parent);
	struct device_node *np = pdev->dev.of_node;
	struct pmic6_bat *bat;
	int ret = 0;
	int i;

	ret = of_device_is_available(np);
	if (!ret)
		return -ENODEV;

	bat = devm_kzalloc(&pdev->dev, sizeof(struct pmic6_bat), GFP_KERNEL);
	if (!bat)
		return -ENOMEM;

	platform_set_drvdata(pdev, bat);
	bat->regmap_irq = pmic6->regmap_irq;
	bat->regmap = pmic6->regmap;
	bat->dev = &pdev->dev;
	atomic_set(&bat->sar_irq, 0);
	mutex_init(&bat->lock);

	/* Enable the FGU module */
	ret = pmic6_bat_enable_fg(bat);
	if (ret) {
		dev_err(&pdev->dev,
			"Failed to pmic6_bat_enable_fg\n");
		return ret;
	}

	for (i = 0; i < ARRAY_SIZE(pmic6_bat_irqs); i++) {
		ret = pmic6_request_irq(bat, pmic6_bat_irq_bits[i],
					pmic6_bat_irqs[i],
					pmic6_bat_irq_handle[i],
					bat);
		if (ret != 0) {
			dev_err(bat->dev,
				"PMIC6 failed to request %s IRQ: %d\n",
				pmic6_bat_irqs[i], ret);
			return ret;
		}
	}

	bat->charger = devm_power_supply_register(&pdev->dev,
						  &pmic6_charger_desc,
						  NULL);
	if (IS_ERR(bat->charger)) {
		ret = PTR_ERR(bat->charger);
		dev_err(&pdev->dev, "failed to register charger: %d\n", ret);
		return ret;
	}

	bat->battery = devm_power_supply_register(&pdev->dev,
						  &pmic6_bat_desc, NULL);
	if (IS_ERR(bat->battery)) {
		ret = PTR_ERR(bat->battery);
		dev_err(&pdev->dev, "failed to register battery: %d\n", ret);
		return ret;
	}

	bat->battery->of_node = np;
	bat->charger->of_node = np;
	ret = pmic6_bat_hw_init(bat);
	if (ret < 0) {
		dev_err(&pdev->dev,
			"Failed to pmic6_dev_of_bat_init\n");
		return ret;
	}

	return 0;
}

static int pmic6_bat_remove(struct platform_device *pdev)
{
	struct pmic6_bat *bat = platform_get_drvdata(pdev);

	cancel_delayed_work(&bat->work);

	return 0;
}

static const struct of_device_id pmic6_bat_match_table[] = {
	{ .compatible = "amlogic,pmic6-battery" },
	{ },
};
MODULE_DEVICE_TABLE(of, pmic6_bat_match_table);

static struct platform_driver pmic6_bat_driver = {
	.driver = {
		.name = "pmic6-battery",
		.of_match_table = pmic6_bat_match_table,
	},
	.probe = pmic6_bat_probe,
	.remove = pmic6_bat_remove,
};

module_platform_driver(pmic6_bat_driver);

MODULE_DESCRIPTION("Battery Driver for PMIC6");
MODULE_AUTHOR("Amlogic");
MODULE_LICENSE("GPL");
