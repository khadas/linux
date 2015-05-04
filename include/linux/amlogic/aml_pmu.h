/*
 * include/linux/amlogic/aml_pmu.h
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

#ifndef __AML_PMU_H__
#define __AML_PMU_H__

#include <linux/power_supply.h>
#include <linux/amlogic/aml_pmu_common.h>
#include <linux/notifier.h>
/* battery capability is very low */
#define BATCAPCORRATE		5
#define ABS(x)			((x) > 0 ? (x) : -(x))
#define AML_PMU_WORK_CYCLE	2000	/* PMU work cycle */

/*
 * debug message control
 */
#define AML_PMU_DBG(format, args...) \
	pr_debug("[AML_PMU]"format, ##args)

#define AML_PMU_INFO(format, args...) \
	pr_info("[AML_PMU]"format, ##args)

#define AML_PMU_ERR(format, args...) \
	pr_err("[AML_PMU]"format, ##args)

#define AML_PMU_CHG_ATTR(_name) \
	{ \
		.attr = { .name = #_name, .mode = 0644 }, \
		.show =  _name##_show, \
		.store = _name##_store, \
	}

/*
 * @soft_limit_to99: flag for if we need to restrict battery capacity to 99%
 *                   when have charge current, even battery voltage is over
 *                   ocv_full;
 * @para:            parameters for call back funtions, user implement;
 * @pmu_call_back:   call back function for axp_charging_monitor, you can add
 *                   anything you want to do in this function, this funtion
 *                   will be called every 2 seconds by default
 */
struct amlogic_pmu_init {
	int   soft_limit_to99;
	int   charge_timeout_retry;
	int   vbus_dcin_short_connect;
	struct battery_parameter *board_battery;
};

#ifdef CONFIG_AML1212
#define AML1212_ADDR				0x35
#define AML1212_SUPPLY_NAME			"aml1212"
#define AML1212_IRQ_NAME			"aml1212-irq"
#define AML_PMU_IRQ_NUM				INT_GPIO_2
#define AML1212_SUPPLY_ID			0
#define AML1212_DRIVER_VERSION			"v0.2"

#define AML1212_OTP_GEN_CONTROL0		0x17

#define AML1212_CHG_CTRL0			0x29
#define AML1212_CHG_CTRL1			0x2A
#define AML1212_CHG_CTRL2			0x2B
#define AML1212_CHG_CTRL3			0x2C
#define AML1212_CHG_CTRL4			0x2D
#define AML1212_CHG_CTRL5			0x2E
#define AML1212_SAR_ADJ				0x73

#define AML1212_GEN_CNTL0			0x80
#define AML1212_GEN_CNTL1			0x81
#define AML1212_PWR_UP_SW_ENABLE		0x82
#define AML1212_PWR_DN_SW_ENABLE		0x84
#define AML1212_GEN_STATUS0			0x86
#define AML1212_GEN_STATUS1			0x87
#define AML1212_GEN_STATUS2			0x88
#define AML1212_GEN_STATUS3			0x89
#define AML1212_GEN_STATUS4			0x8A
#define AML1212_WATCH_DOG			0x8F
#define AML1212_PWR_KEY_ADDR			0x90
#define AML1212_SAR_SW_EN_FIELD			0x9A
#define AML1212_SAR_CNTL_REG0			0x9B
#define AML1212_SAR_CNTL_REG2			0x9D
#define AML1212_SAR_CNTL_REG3			0x9E
#define AML1212_SAR_CNTL_REG5			0xA0
#define AML1212_SAR_RD_IBAT_LAST		0xAB
#define AML1212_SAR_RD_VBAT_ACTIVE		0xAF
#define AML1212_SAR_RD_MANUAL			0xB1
#define AML1212_SAR_RD_IBAT_ACC			0xB5
#define AML1212_SAR_RD_IBAT_CNT			0xB9
#define AML1212_GPIO_OUTPUT_CTRL		0xC3
#define AML1212_GPIO_INPUT_STATUS		0xC4
#define AML1212_IRQ_MASK_0			0xC8
#define AML1212_IRQ_STATUS_CLR_0		0xCF
#define AML1212_SP_CHARGER_STATUS0		0xDE
#define AML1212_SP_CHARGER_STATUS1		0xDF
#define AML1212_SP_CHARGER_STATUS2		0xE0
#define AML1212_SP_CHARGER_STATUS3		0xE1
#define AML1212_SP_CHARGER_STATUS4		0xE2
#define AML1212_PIN_MUX4			0xF4

#define AML_PMU_DCDC1				0
#define AML_PMU_DCDC2				1
#define AML_PMU_DCDC3				2
#define AML_PMU_BOOST				3
#define AML_PMU_LDO1				4
#define AML_PMU_LDO2				5
#define AML_PMU_LDO3				6
#define AML_PMU_LDO4				7
#define AML_PMU_LDO5				8

struct aml1212_supply {
	struct aml_charger aml_charger;
	int32_t  interval;
	int32_t  usb_connect_type;
	int32_t  irq;

	struct power_supply batt;
	struct power_supply ac;
	struct power_supply usb;
	struct power_supply_info *battery_info;
	struct delayed_work work;
	struct work_struct  irq_work;

	struct device *master;
};

/*
 * function declaration here
 */
int aml_pmu_write(int add, uint8_t  val);
int aml_pmu_write16(int add, uint16_t val);
int aml_pmu_writes(int add, uint8_t *buff, int len);
int aml_pmu_read(int add, uint8_t  *val);
int aml_pmu_read16(int add, uint16_t *val);
int aml_pmu_reads(int add, uint8_t *buff, int len);
int aml_pmu_set_bits(int32_t addr, uint8_t bits, uint8_t mask);

int aml_pmu_set_dcin(int enable);
int aml_pmu_set_gpio(int pin, int val);
int aml_pmu_get_gpio(int pin, int *val);

int aml_pmu_get_voltage(void);
int aml_pmu_get_current(void);
int aml_pmu_get_battery_percent(void);

/*
 * bc_mode, indicate usb is conneted to PC or adatper,
 * please see <mach/usbclock.h>
 */
int aml_pmu_set_usb_current_limit(int curr, int bc_mode);
int aml_pmu_set_usb_voltage_limit(int voltage);
int aml_pmu_set_charge_current(int chg_cur);
int aml_pmu_set_charge_voltage(int voltage);
int aml_pmu_set_charge_end_rate(int rate);
int aml_pmu_set_adc_freq(int freq);
int aml_pmu_set_precharge_time(int minute);
int aml_pmu_set_fastcharge_time(int minute);
int aml_pmu_set_charge_enable(int en);

void aml_pmu_set_voltage(int dcdc, int voltage);
void aml_pmu_poweroff(void);

void aml_pmu_clear_coulomb(void);
extern struct i2c_client *g_aml1212_client;
extern struct aml_pmu_driver aml1212_driver;
extern int aml1212_usb_charger(struct notifier_block *nb,
			       unsigned long value, void *pdata);
extern int aml1212_otg_change(struct notifier_block *nb,
			      unsigned long value, void *pdata);
#endif  /* CONFIG_AML1212 */

#ifdef CONFIG_AML1216
#define AML1216_ADDR				0x35

#define AML1216_DRIVER_VERSION			"v1.0"
#define AML1216_DRIVER_NAME			"aml1216_pmu"
#define AML1216_IRQ_NUM				INT_GPIO_2
#define AML1216_IRQ_NAME			"aml1216_irq"
#define AML1216_WORK_CYCLE			2000

/* add for AML1216 ctroller. */
#define AML1216_OTP_GEN_CONTROL0		0x17

#define AML1216_CHG_CTRL0			0x29
#define AML1216_CHG_CTRL1			0x2A
#define AML1216_CHG_CTRL2			0x2B
#define AML1216_CHG_CTRL3			0x2C
#define AML1216_CHG_CTRL4			0x2D
#define AML1216_CHG_CTRL5			0x2E
#define AML1216_CHG_CTRL6			0x129
#define AML1216_SAR_ADJ				0x73


#define AML1216_GEN_CNTL0			0x80
#define AML1216_GEN_CNTL1			0x81
#define AML1216_PWR_UP_SW_ENABLE		0x82
#define AML1216_PWR_DN_SW_ENABLE		0x84
#define AML1216_GEN_STATUS0			0x86
#define AML1216_GEN_STATUS1			0x87
#define AML1216_GEN_STATUS2			0x88
#define AML1216_GEN_STATUS3			0x89
#define AML1216_GEN_STATUS4			0x8A
#define AML1216_WATCH_DOG			0x8F
#define AML1216_PWR_KEY_ADDR			0x90
#define AML1216_SAR_SW_EN_FIELD			0x9A
#define AML1216_SAR_CNTL_REG0			0x9B
#define AML1216_SAR_CNTL_REG2			0x9D
#define AML1216_SAR_CNTL_REG3			0x9E
#define AML1216_SAR_CNTL_REG5			0xA0
#define AML1216_SAR_RD_IBAT_LAST		0xAB
#define AML1216_SAR_RD_VBAT_ACTIVE		0xAF
#define AML1216_SAR_RD_MANUAL			0xB1
#define AML1216_SAR_RD_IBAT_ACC			0xB5
#define AML1216_SAR_RD_IBAT_CNT			0xB9
#define AML1216_GPIO_OUTPUT_CTRL		0xC3
#define AML1216_GPIO_INPUT_STATUS		0xC4
#define AML1216_IRQ_MASK_0			0xC8
#define AML1216_IRQ_STATUS_CLR_0		0xCF
#define AML1216_SP_CHARGER_STATUS0		0xDE
#define AML1216_SP_CHARGER_STATUS1		0xDF
#define AML1216_SP_CHARGER_STATUS2		0xE0
#define AML1216_SP_CHARGER_STATUS3		0xE1
#define AML1216_SP_CHARGER_STATUS4		0xE2
#define AML1216_PIN_MUX4			0xF4

#define AML1216_DCDC1				0
#define AML1216_DCDC2				1
#define AML1216_DCDC3				2
#define AML1216_BOOST				3
#define AML1216_LDO1				4
#define AML1216_LDO2				5
#define AML1216_LDO3				6
#define AML1216_LDO4				7
#define AML1216_LDO5				8

#define AML1216_CHARGER_CHARGING		1
#define AML1216_CHARGER_DISCHARGING		2
#define AML1216_CHARGER_NONE			3

#define AML1216_DBG(format, args...) \
	pr_debug("[AML1216]"format, ##args)

#define AML1216_INFO(format, args...) \
	pr_info("[AML1216]"format, ##args)

#define AML1216_ERR(format, args...) \
	pr_err("[AML1216]"format, ##args)

#define ABS(x)					((x) > 0 ? (x) : -(x))

#define AML_ATTR(_name)  \
	{ \
		.attr = { .name = #_name, .mode = 0644 }, \
		.show =  _name##_show, \
		.store = _name##_store, \
	}

struct aml1216_supply {
	struct   aml_charger aml_charger;
	int32_t  interval;
	int32_t  usb_connect_type;
	int32_t  charge_timeout_retry;
	int32_t  vbus_dcin_short_connect;
	int32_t  irq;

	struct power_supply batt;
	struct power_supply ac;
	struct power_supply usb;
	struct power_supply_info *battery_info;
	struct delayed_work work;
	struct work_struct  irq_work;
	struct notifier_block nb;
	struct device *master;
};

/*
 * API export of pmu base operation
 */
extern int  aml1216_write(int32_t add, uint8_t val);
extern int  aml1216_write16(int32_t add, uint16_t val);
extern int  aml1216_writes(int32_t add, uint8_t *buff, int len);
extern int  aml1216_read(int add, uint8_t *val);
extern int  aml1216_read16(int add, uint16_t *val);
extern int  aml1216_reads(int add, uint8_t *buff, int len);
extern int  aml1216_set_bits(int addr, uint8_t bits, uint8_t mask);
extern int  aml1216_get_battery_voltage(void);
extern int  aml1216_get_battery_temperature(void);
extern int  aml1216_get_battery_current(void);
extern int  aml1216_get_charge_status(void);
extern int  aml1216_set_gpio(int gpio, int output);
extern int  aml1216_get_gpio(int gpio, int *val);
extern int  aml1216_set_dcin_current_limit(int limit);
extern int  aml1216_set_usb_current_limit(int limit);
extern int  aml1216_set_usb_voltage_limit(int voltage);
extern int  aml1216_set_dcdc_voltage(int dcdc, uint32_t voltage);
extern int  aml1216_get_dcdc_voltage(int dcdc, uint32_t *uV);
extern int  aml1216_set_charge_enable(int enable);
extern int  aml1216_set_charge_current(int curr);
extern int  aml1216_get_battery_percent(void);
extern int  aml1216_cal_ocv(int ibat, int vbat, int dir);
extern void aml1216_power_off(void);
/*
 * Global variable declaration
 */
extern struct aml1216_supply *g_aml1216_supply;
extern struct i2c_client *g_aml1216_client;
extern struct aml_pmu_driver aml1216_pmu_driver;
extern int aml1216_otg_change(struct notifier_block *nb,
			      unsigned long value, void *pdata);
extern int aml1216_usb_charger(struct notifier_block *nb,
			       unsigned long value, void *pdata);
#endif  /* CONFIG_AML1216 */

#ifdef CONFIG_AML1218
#define AML1218_ADDR				0x35

#define AML1218_DRIVER_VERSION			"v1.0"
#define AML1218_DRIVER_NAME			"aml1218_pmu"
#define AML1218_IRQ_NUM				INT_GPIO_2
#define AML1218_IRQ_NAME			"aml1218_irq"
#define AML1218_WORK_CYCLE			2000

/* add for AML1218 ctroller. */
#define AML1218_OTP_GEN_CONTROL0		0x17

#define AML1218_CHG_CTRL0			0x29
#define AML1218_CHG_CTRL1			0x2A
#define AML1218_CHG_CTRL2			0x2B
#define AML1218_CHG_CTRL3			0x2C
#define AML1218_CHG_CTRL4			0x2D
#define AML1218_CHG_CTRL5			0x2E
#define AML1218_CHG_CTRL6			0x129
#define AML1218_SAR_ADJ				0x73


#define AML1218_GEN_CNTL0			0x80
#define AML1218_GEN_CNTL1			0x81
#define AML1218_PWR_UP_SW_ENABLE		0x82
#define AML1218_PWR_DN_SW_ENABLE		0x84
#define AML1218_GEN_STATUS0			0x86
#define AML1218_GEN_STATUS1			0x87
#define AML1218_GEN_STATUS2			0x88
#define AML1218_GEN_STATUS3			0x89
#define AML1218_GEN_STATUS4			0x8A
#define AML1218_WATCH_DOG			0x8F
#define AML1218_PWR_KEY_ADDR			0x90
#define AML1218_SAR_SW_EN_FIELD			0x9A
#define AML1218_SAR_CNTL_REG0			0x9B
#define AML1218_SAR_CNTL_REG2			0x9D
#define AML1218_SAR_CNTL_REG3			0x9E
#define AML1218_SAR_CNTL_REG5			0xA0
#define AML1218_SAR_RD_IBAT_LAST		0xAB
#define AML1218_SAR_RD_VBAT_ACTIVE		0xAF
#define AML1218_SAR_RD_MANUAL			0xB1
#define AML1218_SAR_RD_IBAT_ACC			0xB5
#define AML1218_SAR_RD_IBAT_CNT			0xB9
#define AML1218_GPIO_OUTPUT_CTRL		0xC3
#define AML1218_GPIO_INPUT_STATUS		0xC4
#define AML1218_IRQ_MASK_0			0xC8
#define AML1218_IRQ_STATUS_CLR_0		0xCF
#define AML1218_SP_CHARGER_STATUS0		0xDE
#define AML1218_SP_CHARGER_STATUS1		0xDF
#define AML1218_SP_CHARGER_STATUS2		0xE0
#define AML1218_SP_CHARGER_STATUS3		0xE1
#define AML1218_SP_CHARGER_STATUS4		0xE2
#define AML1218_PIN_MUX4			0xF4

#define AML1218_DCDC1				0
#define AML1218_DCDC2				1
#define AML1218_DCDC3				2
#define AML1218_BOOST				3
#define AML1218_LDO1				4
#define AML1218_LDO2				5
#define AML1218_LDO3				6
#define AML1218_LDO4				7
#define AML1218_LDO5				8

#define AML1218_CHARGER_CHARGING		1
#define AML1218_CHARGER_DISCHARGING		2
#define AML1218_CHARGER_NONE			3

#define AML1218_DBG(format, args...) \
	pr_debug("[AML1218]"format, ##args)

#define AML1218_INFO(format, args...) \
	pr_info("[AML1218]"format, ##args)

#define AML1218_ERR(format, args...) \
	pr_err("[AML1218]"format, ##args)

#define ABS(x)					((x) > 0 ? (x) : -(x))

#define AML_ATTR(_name) \
	{ \
		.attr = { .name = #_name, .mode = 0644 }, \
		.show =  _name##_show, \
		.store = _name##_store, \
	}

struct aml1218_supply {
	struct   aml_charger aml_charger;
	int32_t  interval;
	int32_t  usb_connect_type;
	int32_t  charge_timeout_retry;
	int32_t  vbus_dcin_short_connect;
	int32_t  irq;

	struct power_supply batt;
	struct power_supply ac;
	struct power_supply usb;
	struct power_supply_info *battery_info;
	struct delayed_work work;
	struct work_struct  irq_work;
	struct notifier_block nb;
	struct device *master;
};

/*
 * API export of pmu base operation
 */
extern int  aml1218_write(int32_t add, uint8_t val);
extern int  aml1218_write16(int32_t add, uint16_t val);
extern int  aml1218_writes(int32_t add, uint8_t *buff, int len);
extern int  aml1218_read(int add, uint8_t *val);
extern int  aml1218_read16(int add, uint16_t *val);
extern int  aml1218_reads(int add, uint8_t *buff, int len);
extern int  aml1218_set_bits(int addr, uint8_t bits, uint8_t mask);
extern int  aml1218_get_battery_voltage(void);
extern int  aml1218_get_battery_temperature(void);
extern int  aml1218_get_battery_current(void);
extern int  aml1218_get_charge_status(void);
extern int  aml1218_set_gpio(int gpio, int output);
extern int  aml1218_get_gpio(int gpio, int *val);
extern int  aml1218_set_dcin_current_limit(int limit);
extern int  aml1218_set_usb_current_limit(int limit);
extern int  aml1218_set_usb_voltage_limit(int voltage);
extern int  aml1218_set_dcdc_voltage(int dcdc, uint32_t voltage);
extern int  aml1218_get_dcdc_voltage(int dcdc, uint32_t *uV);
extern int  aml1218_set_charge_enable(int enable);
extern int  aml1218_set_charge_current(int curr);
extern int  aml1218_get_battery_percent(void);
extern int  aml1218_cal_ocv(int ibat, int vbat, int dir);
extern void aml1218_power_off(void);
/*
 * Global variable declaration
 */
extern struct aml1218_supply *g_aml1218_supply;
extern struct i2c_client *g_aml1218_client;
extern struct aml_pmu_driver aml1218_pmu_driver;
extern int aml1218_otg_change(struct notifier_block *nb,
			      unsigned long value, void *pdata);
extern int aml1218_usb_charger(struct notifier_block *nb,
			       unsigned long value, void *pdata);
#endif  /* CONFIG_AML1218 */

#ifdef CONFIG_AML1220

/* add aml1220 pmu4 power register */

#define AML1220_PMU_CTR_04			0x05

extern int  aml1220_write(int32_t add, uint8_t val);
extern int  aml1220_write16(int32_t add, uint16_t val);
extern int  aml1220_read(int add, uint8_t *val);
extern int  aml1220_read16(int add, uint16_t *val);

/* #define AML1220 */
#endif

#endif /* __AML_PMU_H__ */

