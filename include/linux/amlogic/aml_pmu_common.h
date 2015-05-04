/*
 * include/linux/amlogic/aml_pmu_common.h
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

#ifndef __AML_PMU_COMMON_H__
#define __AML_PMU_COMMON_H__

#include <linux/amlogic/battery_parameter.h>
/* battery is charging */
#define CHARGER_CHARGING			1
/* battery is discharging */
#define CHARGER_DISCHARGING			2
/* battery is in rest, this status can be seen when battery is full charged */
#define CHARGER_NONE				3

/* PMU has inspective coulomb counter for charge and discharge */
#define COULOMB_BOTH				0
/* PMU has only one coulomb counter, value increase when charging */
#define COULOMB_SINGLE_CHG_INC			1
/* PMU has only one coulomb counter, value decrease when charging */
#define COULOMB_SINGLE_CHG_DEC			2
/* PMU has only one coulomb counter, value decrease always */
#define COULOMB_ALWAYS_DEC			3

#define PMU_GPIO_OUTPUT_LOW			0
#define PMU_GPIO_OUTPUT_HIGH			1
#define PMU_GPIO_OUTPUT_HIGHZ			2
#define PMU_GPIO_INPUT				3

struct aml_charger {
	int rest_vol;
	int ocv;
	int ibat;
	int vbat;
	int ocv_full;
	int ocv_empty;
	int ocv_rest_vol;
	int charge_cc;
	int discharge_cc;
	int v_usb;
	int i_usb;
	int v_dcin;
	int i_dcin;
	uint32_t fault;

	uint8_t  charge_status;
	uint8_t  resume;
	uint8_t  soft_limit_to99;
	uint8_t  ext_valid;
	uint8_t  usb_valid;
	uint8_t  dcin_valid;
	uint8_t  coulomb_type;
	uint8_t  bat_det;
	uint8_t  charge_timeout;
	uint8_t  serial_batteries;
};

typedef int (*pmu_callback)(struct aml_charger *charger, void *pdata);

struct aml_pmu_driver {
	char  *name;
	int (*pmu_get_coulomb)(struct aml_charger *charger);
	int (*pmu_clear_coulomb)(struct aml_charger *charger);
	int (*pmu_update_status)(struct aml_charger *charger);
	int (*pmu_set_rdc)(int rdc);
	int (*pmu_set_gpio)(int gpio, int value);
	int (*pmu_get_gpio)(int gpio, int *value);
	int (*pmu_reg_read)(int addr, uint8_t *buf);
	int (*pmu_reg_write)(int addr, uint8_t value);
	int (*pmu_reg_reads)(int addr, uint8_t *buf, int count);
	int (*pmu_reg_writes)(int addr, uint8_t *buf, int count);
	int (*pmu_set_bits)(int addr, uint8_t bits, uint8_t mask);
	int (*pmu_set_usb_current_limit)(int curr);
	int (*pmu_set_charge_current)(int curr);
	void (*pmu_power_off)(void);
};

extern void *pmu_alloc_mutex(void);
extern void pmu_mutex_lock(void *mutex);
extern void pmu_mutex_unlock(void *mutex);
extern int  pmu_rtc_device_init(void);
extern int  pmu_rtc_set_alarm(unsigned long seconds);

struct aml_pmu_api { /* for battery capacity */
	int (*pmu_get_ocv_filter)(void);
	int (*pmu_get_report_delay)(void);
	int (*pmu_set_report_delay)(int);
	void (*pmu_update_battery_capacity)(struct aml_charger *,
					    struct battery_parameter *);
	void (*pmu_probe_process)(struct aml_charger *,
				  struct battery_parameter *);
	void (*pmu_suspend_process)(struct aml_charger *);
	void (*pmu_resume_process)(struct aml_charger *,
				   struct battery_parameter *);
	void (*pmu_update_battery_by_ocv)(struct aml_charger *,
					  struct battery_parameter *);
	void (*pmu_calibrate_probe_process)(struct aml_charger *);
	ssize_t (*pmu_format_dbg_buffer)(struct aml_charger *, char *);
};

extern int    aml_pmu_register_callback(pmu_callback callback,
					void *pdata, char *name);
extern int    aml_pmu_unregister_callback(char *name);
extern int    aml_pmu_register_driver(struct aml_pmu_driver *driver);
extern int    aml_pmu_register_api(struct aml_pmu_api *);
extern void   aml_pmu_clear_api(void);
extern struct aml_pmu_api *aml_pmu_get_api(void);
extern void   aml_pmu_clear_driver(void);
extern void   aml_pmu_do_callbacks(struct aml_charger *charger);
extern struct aml_pmu_driver *aml_pmu_get_driver(void);

extern struct aml_pmu_api *aml_pmu_get_api(void);

#ifdef CONFIG_AMLOGIC_USB
extern int dwc_otg_power_register_notifier(struct notifier_block *);
extern int dwc_otg_power_unregister_notifier(struct notifier_block *);
extern int dwc_otg_charger_detect_register_notifier(struct notifier_block *);
extern int dwc_otg_charger_detect_unregister_notifier(struct notifier_block *);
#endif

#endif /* __AML_PMU_COMMON_H__ */
