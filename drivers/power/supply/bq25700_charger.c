// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/mfd/core.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/of_device.h>
#include <linux/delay.h>
#include <linux/usb/phy.h>
#include <linux/extcon.h>
#include <linux/delay.h>
#include <linux/power_supply.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/of.h>
#include <linux/irq.h>

static int dbg_enable = 1;
//module_param_named(dbg_level, dbg_enable, int, 0644);

#define DBG(args...) \
	do { \
		if (dbg_enable) { \
			pr_debug(args); \
		} \
	} while (0)

#define bq25700_info(fmt, args...) pr_info("bq25700: " fmt, ##args)

#define BQ25700_MANUFACTURER		"Texas Instruments"
#define BQ25700_ID			0x59

#define DEFAULT_INPUTVOL		((5000 - 1280) * 1000)
#define MAX_INPUTVOLTAGE		24000000
#define MAX_INPUTCURRENT		6350000
#define MAX_CHARGEVOLTAGE		16800000
#define MAX_CHARGECURRETNT		8128000
#define MAX_OTGVOLTAGE			20800000
#define MAX_OTGCURRENT			6350000

#define AC_STAT   0x20
#define AC_STAT_MASK   BIT(15)
#define AC_STAT_SHIFT  15

#define ICO_DONE 0x20
#define ICO_DONE_MASK  BIT(14)
#define ICO_DONE_SHIFT 14

#define IN_VINDPM 0x20
#define IN_VINDPM_MASK BIT(12)
#define IN_VINDPM_SHIFT 12

#define IN_IINDPM 0x20
#define IN_IINDPM_MASK  BIT(11)
#define IN_IINDPM_SHIFT 11

#define IN_FCHRG  0x20
#define IN_FCHRG_MASK  BIT(10)
#define IN_FCHRG_SHIFT  10

#define IN_PCHRG  0x20
#define IN_PCHRG_MASK BIT(9)
#define IN_PCHRG_SHIFT 9

#define IN_OTG 0x20
#define IN_OTG_MASK BIT(8)
#define IN_OTG_SHIFT 8

#define F_ACOV  0x20
#define F_ACOV_MASK  BIT(7)
#define F_ACOV_SHIFT 7

#define F_BATOC 0x20
#define F_BATOC_MASK BIT(6)
#define F_BATOC_SHIFT 6

#define F_ACOC 0x20
#define F_ACOC_MASK BIT(5)
#define F_ACOC_SHIFT 5

#define SYSOVP_STAT 0x20
#define SYSOVP_STAT_MASK  BIT(4)
#define SYSOVP_STAT_SHIFT 4

#define F_LATCHOFF 0x20
#define F_LATCHOFF_MASK  BIT(2)
#define F_LATCHOFF_SHIFT 2

#define F_OTG_OVP  0x20
#define F_OTG_OVP_MASK BIT(1)
#define F_OTG_OVP_SHIFT 1

#define F_OTG_OCP 0x20
#define F_OTG_OCP_MASK BIT(0)
#define F_OTG_OCP_SHIFT 0

#define DEVICE_ID 0x2e
#define DEVICE_ID_MASK (BIT(7) | BIT(6) | BIT(5) | \
						BIT(4) | BIT(3) | BIT(2) | \
						BIT(1) | BIT(0))
#define DEVICE_ID_SHIFT 0

#define OUTPUT_BAT_VOL 0x2c
#define OUTPUT_BAT_VOL_MASK (BIT(6) | BIT(5) | BIT(4) | \
							BIT(3) | BIT(2) | \
							BIT(1) | BIT(0))
#define OUTPUT_BAT_VOL_SHIFT 0

#define OUTPUT_CHG_CUR 0x24
#define OUTPUT_CHG_CUR_MASK  (BIT(6) | BIT(5) | BIT(4) |\
							BIT(3) | BIT(2) | \
							BIT(1) | BIT(0))
#define OUTPUT_CHG_CUR_SHIFT 0

#define CHARGE_CURRENT  0x02
#define CHARGE_CURRENT_MASK (BIT(12) | BIT(11) | BIT(10) |\
							BIT(9) | BIT(8) | \
							BIT(7) | BIT(6))
#define CHARGE_CURRENT_SHIFT 6

#define EN_ADC_VBAT 0x3a
#define EN_ADC_VBAT_MASK BIT(0)
#define EN_ADC_VBAT_SHIFT 0

#define EN_ADC_VSYS 0x3a
#define EN_ADC_VSYS_MASK  BIT(1)
#define EN_ADC_VSYS_SHIFT 1

#define EN_ADC_ICHG 0x3a
#define EN_ADC_ICHG_MASK BIT(2)
#define EN_ADC_ICHG_SHIFT 2

#define EN_ADC_IDCHG 0x3a
#define EN_ADC_IDCHG_MASK BIT(3)
#define EN_ADC_IDCHG_SHIFT 3

#define EN_ADC_IIN 0x3a
#define EN_ADC_IIN_MASK BIT(4)
#define EN_ADC_IIN_SHIFT 4

#define EN_ADC_PSYS 0x3a
#define EN_ADC_PSYS_MASK BIT(5)
#define EN_ADC_PSYS_SHIFT 5

#define EN_ADC_VBUS 0x3a
#define EN_ADC_VBUS_MASK BIT(6)
#define EN_ADC_VBUS_SHIFT 6

#define EN_ADC_CMPIN 0x3a
#define EN_ADC_CMPIN_MASK BIT(7)
#define EN_ADC_CMPIN_SHIFT 7

#define ADC_FULLSCALE 0x3a
#define ADC_FULLSCALE_MASK BIT(13)
#define ADC_FULLSCALE_SHIFT 13

#define ADC_START 0x3a
#define ADC_START_MASK BIT(14)
#define ADC_START_SHIFT 14

#define ADC_CONV 0x3a
#define ADC_CONV_MASK BIT(15)
#define ADC_CONV_SHIFT 15

#define EN_LWPWR 0x00
#define EN_LWPWR_MASK BIT(15)
#define EN_LWPWR_SHIFT 15

#define MIN_SYS_VOTAGE 0x0c
#define MIN_SYS_VOTAGE_MASK (BIT(13) | BIT(12) | BIT(11) | \
							BIT(10) | BIT(9) | \
							BIT(8))
#define MIN_SYS_VOTAGE_SHIFT 8

#define INPUT_CURRENT 0x0e
#define INPUT_CURRENT_MASK (BIT(14) | BIT(13) | BIT(12) | \
							BIT(11) | BIT(10) | \
							BIT(9) | BIT(8))
#define INPUT_CURRENT_SHIFT 8

#define INPUT_VOLTAGE 0x0a
#define INPUT_VOLTAGE_MASK (BIT(13) | BIT(12) | BIT(11) | \
							BIT(10) | BIT(9) | \
							BIT(8) | BIT(7) | \
							BIT(6))
#define INPUT_VOLTAGE_SHIFT 6

#define MAX_CHARGE_VOLTAGE 0x04
#define MAX_CHARGE_VOLTAGE_MASK (BIT(14) | BIT(13) | \
							BIT(12) | BIT(11) | \
							BIT(10) | BIT(9) | \
							BIT(8) | BIT(7) | \
							BIT(6) | BIT(5) | \
							BIT(4))
#define MAX_CHARGE_VOLTAGE_SHIFT 4

#define CHARGE_CURRENT 0x02
#define CHARGE_CURRENT_MASK (BIT(12) | BIT(11) | BIT(10) | \
							BIT(9) | BIT(8) | \
							BIT(7) | BIT(6))
#define CHARGE_CURRENT_SHIFT 6

#define OTG_VOLTAGE 0x06
#define OTG_VOLTAGE_MASK (BIT(13) | BIT(12) | BIT(11) | \
							BIT(10) | BIT(9) | \
							BIT(8) | BIT(7) | \
							BIT(6))
#define OTG_VOLTAGE_SHIFT 6

#define OTG_CURRENT 0x08
#define OTG_CURRENT_MASK (BIT(14) | BIT(13) | BIT(12) | \
							BIT(11) | BIT(10) | \
							BIT(9) | BIT(8))
#define OTG_CURRENT_SHIFT 8

#define WDTWR_ADJ 0x00
#define WDTWR_ADJ_MASK (BIT(14) | BIT(13))
#define WDTWR_ADJ_SHIFT 13

//extern void send_power_key(int state);
#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
#include <linux/amlogic/pm.h>
static struct early_suspend charger_early_suspend;
#endif
bool is_suspend;

enum charger_t {
	USB_TYPE_UNKNOWN_CHARGER,
	USB_TYPE_NONE_CHARGER,
	USB_TYPE_USB_CHARGER,
	USB_TYPE_AC_CHARGER,
	USB_TYPE_CDP_CHARGER,
	DC_TYPE_DC_CHARGER,
	DC_TYPE_NONE_CHARGER,
};

/* initial field values, converted to register values */
struct bq25700_init_data {
	u32 ichg;	/* charge current		*/
	u32 max_chg_vol;	/*max charge voltage*/
	u32 input_voltage;	/*input voltage*/
	u32 input_current;	/*input current*/
	u32 input_current_sdp;
	u32 input_current_dcp;
	u32 input_current_cdp;
	u32 sys_min_voltage;	/*mininum system voltage*/
	u32 otg_voltage;	/*OTG voltage*/
	u32 otg_current;	/*OTG current*/
};

struct bq25700_state {
	u8 ac_stat;
	u8 ico_done;
	u8 in_vindpm;
	u8 in_iindpm;
	u8 in_fchrg;
	u8 in_pchrg;
	u8 in_otg;
	u8 fault_acov;
	u8 fault_batoc;
	u8 fault_acoc;
	u8 sysovp_stat;
	u8 fault_latchoff;
	u8 fault_otg_ovp;
	u8 fault_otg_ocp;
};

struct bq25700_device {
	struct i2c_client			*client;
	struct device				*dev;
	struct power_supply			*supply_charger;
	char				model_name[I2C_NAME_SIZE];
	unsigned int			irq;
	bool				first_time;
	bool				charger_health_valid;
	bool				battery_health_valid;
	bool				battery_status_valid;

	struct workqueue_struct		*usb_charger_wq;
	struct workqueue_struct		*ac_charger_wq;
	struct workqueue_struct		*dc_charger_wq;
	struct workqueue_struct		*finish_sig_wq;
	struct delayed_work		usb_work;
	struct delayed_work		pd_work;
	struct delayed_work		host_work;
	struct delayed_work		discnt_work;
	struct delayed_work		irq_work;
	struct delayed_work             ac_work;
	struct notifier_block		cable_cg_nb;
	struct notifier_block		cable_pd_nb;
	struct notifier_block		cable_host_nb;
	struct notifier_block		cable_discnt_nb;
	struct extcon_dev		*cable_edev;

	struct regmap			*regmap;
	int				chip_id;
	struct bq25700_init_data	init_data;
	struct bq25700_state		state;
};

/*
 * Most of the val -> idx conversions can be computed, given the minimum,
 * maximum and the step between values. For the rest of conversions, we use
 * lookup tables.
 */
enum bq25700_table_ids {
	/* range tables */
	TBL_ICHG,
	TBL_CHGMAX,
	TBL_INPUTVOL,
	TBL_INPUTCUR,
	TBL_SYSVMIN,
	TBL_OTGVOL,
	TBL_OTGCUR,
	TBL_EXTCON,
};

struct bq25700_range {
	u32 min;
	u32 max;
	u32 step;
};

struct bq25700_lookup {
	const u32 *tbl;
	u32 size;
};

static const union {
	struct bq25700_range  rt;
	struct bq25700_lookup lt;
} bq25700_tables[] = {
	/* range tables */
	[TBL_ICHG] =	{ .rt = {0,	  8128000, 64000} },
	/* uA */
	[TBL_CHGMAX] = { .rt = {0, 19200000, 16000} },
	/* uV  max charge voltage*/
	[TBL_INPUTVOL] = { .rt = {3200000, 19520000, 64000} },
	/* uV  input charge voltage*/
	[TBL_INPUTCUR] = {.rt = {0, 6350000, 50000} },
	/*uA input current*/
	[TBL_SYSVMIN] = { .rt = {1024000, 16182000, 256000} },
	/* uV min system voltage*/
	[TBL_OTGVOL] = {.rt = {4480000, 20800000, 64000} },
	/*uV OTG volage*/
	[TBL_OTGCUR] = {.rt = {0, 6350000, 50000} },
};

static int bq25700_read(struct bq25700_device *charger, u8 reg, u16 *data)
{
	int ret;

	ret = i2c_smbus_read_word_data(charger->client, reg);
	if (ret < 0)
		return ret;

	*data = ret;
	return 0;
}

static int bq25700_write(struct bq25700_device *charger,
			       u8 reg, u16 data)
{
	int ret;

	ret = i2c_smbus_write_word_data(charger->client, reg, data);

	if (ret < 0) {
		pr_debug("%s reg=0x%x error: %d\n", __func__, reg, ret);
		return -1;
	}

	return 0;
}

static u16 bq25700_read_mask(struct bq25700_device *charger,
				u8 reg, u16 mask, u8 shift, u16 *data)
{
	int ret;
	u16 val;

	ret = bq25700_read(charger, reg, &val);
	if (ret < 0)
		return -1;

	val &= mask;
	val >>= shift;
	*data = val;
	return 0;
}

static int bq25700_write_mask(struct bq25700_device *charger,
				u8 reg, u16 mask, u8 shift, u16 data)
{
	int ret;
	u16 val;

	ret = bq25700_read(charger, reg, &val);
	if (ret < 0)
		return -1;

	val &= ~mask;
	val |= ((data << shift) & mask);

	ret = bq25700_write(charger, reg, val);
	if (ret < 0)
		return -1;
	return 0;
}

static int bq25700_get_chip_state(struct bq25700_device *charger,
				  struct bq25700_state *state)
{
	int ret;
	u16 val = 0;

	ret = bq25700_read_mask(charger, AC_STAT, AC_STAT_MASK,
							AC_STAT_SHIFT, &val);
	if (ret < 0)
		return ret;
	state->ac_stat = val;
	ret = bq25700_read_mask(charger, ICO_DONE, ICO_DONE_MASK,
							ICO_DONE_SHIFT, &val);
	if (ret < 0)
		return ret;
	state->ico_done = val;
	ret = bq25700_read_mask(charger, IN_VINDPM, IN_VINDPM_MASK,
							IN_VINDPM_SHIFT, &val);
	if (ret < 0)
		return ret;
	state->in_vindpm = val;
	ret = bq25700_read_mask(charger, IN_IINDPM, IN_IINDPM_MASK,
							IN_IINDPM_SHIFT, &val);
	if (ret < 0)
		return ret;
	state->in_iindpm = val;
	ret = bq25700_read_mask(charger, IN_FCHRG, IN_FCHRG_MASK,
							IN_FCHRG_SHIFT, &val);
	if (ret < 0)
		return ret;
	state->in_fchrg = val;
	ret = bq25700_read_mask(charger, IN_PCHRG, IN_PCHRG_MASK,
							IN_PCHRG_SHIFT, &val);
	if (ret < 0)
		return ret;
	state->in_pchrg = val;
	ret = bq25700_read_mask(charger, IN_OTG, IN_OTG_MASK,
							IN_OTG_SHIFT, &val);
	if (ret < 0)
		return ret;
	state->in_otg = val;
	ret = bq25700_read_mask(charger, F_ACOV, F_ACOV_MASK,
							F_ACOV_SHIFT, &val);
	if (ret < 0)
		return ret;
	state->fault_acov = val;
	ret = bq25700_read_mask(charger, F_BATOC, F_BATOC_MASK,
							F_BATOC_SHIFT, &val);
	if (ret < 0)
		return ret;
	state->fault_batoc = val;
	ret = bq25700_read_mask(charger, F_ACOC, F_ACOC_MASK,
							F_ACOC_SHIFT, &val);
	if (ret < 0)
		return ret;
	state->fault_acoc = val;
	ret = bq25700_read_mask(charger, SYSOVP_STAT, SYSOVP_STAT_MASK,
						SYSOVP_STAT_SHIFT, &val);
	if (ret < 0)
		return ret;
	state->sysovp_stat = val;
	ret = bq25700_read_mask(charger, F_LATCHOFF, F_LATCHOFF_MASK,
							F_LATCHOFF_SHIFT, &val);
	if (ret < 0)
		return ret;
	state->fault_latchoff = val;
	ret = bq25700_read_mask(charger, F_OTG_OVP, F_OTG_OVP_MASK,
							F_OTG_OVP_SHIFT, &val);
	if (ret < 0)
		return ret;
	state->fault_otg_ovp = val;

	ret = bq25700_read_mask(charger, F_OTG_OCP, F_OTG_OCP_MASK,
							F_OTG_OCP_SHIFT, &val);
	if (ret < 0)
		return ret;
	state->fault_otg_ocp = val;

	DBG("status:\n");
	DBG("AC_STAT:  %d\n", state->ac_stat);
	DBG("ICO_DONE: %d\n", state->ico_done);
	DBG("IN_VINDPM: %d\n", state->in_vindpm);
	DBG("IN_IINDPM: %d\n", state->in_iindpm);
	DBG("IN_FCHRG: %d\n", state->in_fchrg);
	DBG("IN_PCHRG: %d\n", state->in_pchrg);
	DBG("IN_OTG: %d\n", state->in_otg);
	DBG("F_ACOV: %d\n", state->fault_acov);
	DBG("F_BATOC: %d\n", state->fault_batoc);
	DBG("F_ACOC: %d\n", state->fault_acoc);
	DBG("SYSOVP_STAT: %d\n", state->sysovp_stat);
	DBG("F_LATCHOFF: %d\n", state->fault_latchoff);
	DBG("F_OTGOVP: %d\n", state->fault_otg_ovp);
	DBG("F_OTGOCP: %d\n", state->fault_otg_ocp);
	return 0;
}

static u32 bq25700_find_idx(u32 value, enum bq25700_table_ids id)
{
	u32 idx;
	u32 rtbl_size;
	const struct bq25700_range *rtbl = &bq25700_tables[id].rt;

	rtbl_size = (rtbl->max - rtbl->min) / rtbl->step + 1;

	for (idx = 1;
	     idx < rtbl_size && (idx * rtbl->step + rtbl->min <= value);
	     idx++)
		;

	return idx - 1;
}

static int bq25700_fw_read_u32_props(struct bq25700_device *charger)
{
	int ret;
	u32 property;
	int i;
	struct bq25700_init_data *init = &charger->init_data;
	struct {
		char *name;
		bool optional;
		enum bq25700_table_ids tbl_id;
		u32 *conv_data; /* holds converted value from given property */
	} props[] = {
		/* required properties */
		{"ti,charge-current", false, TBL_ICHG,
		 &init->ichg},
		{"ti,max-charge-voltage", false, TBL_CHGMAX,
		 &init->max_chg_vol},
		{"ti,input-current-sdp", false, TBL_INPUTCUR,
		 &init->input_current_sdp},
		{"ti,input-current-dcp", false, TBL_INPUTCUR,
		 &init->input_current_dcp},
		{"ti,input-current-cdp", false, TBL_INPUTCUR,
		 &init->input_current_cdp},
		{"ti,minimum-sys-voltage", false, TBL_SYSVMIN,
		 &init->sys_min_voltage},
		{"ti,otg-voltage", false, TBL_OTGVOL,
		 &init->otg_voltage},
		{"ti,otg-current", false, TBL_OTGCUR,
		 &init->otg_current},
	};

	/* initialize data for optional properties */
	for (i = 0; i < ARRAY_SIZE(props); i++) {
		ret = device_property_read_u32(charger->dev, props[i].name,
					       &property);
		if (ret < 0) {
			if (props[i].optional)
				continue;

			return ret;
		}

		if (props[i].tbl_id == TBL_ICHG &&
		    property > MAX_CHARGECURRETNT) {
			dev_err(charger->dev, "ti,charge-current is error\n");
			return -ENODEV;
		}
		if (props[i].tbl_id == TBL_CHGMAX &&
		    property > MAX_CHARGEVOLTAGE) {
			dev_err(charger->dev, "ti,max-charge-voltage is error\n");
			return -ENODEV;
		}
		if (props[i].tbl_id == TBL_INPUTCUR &&
		    property > MAX_INPUTCURRENT) {
			dev_err(charger->dev, "ti,input-current is error\n");
			return -ENODEV;
		}
		if (props[i].tbl_id == TBL_OTGVOL &&
		    property > MAX_OTGVOLTAGE) {
			dev_err(charger->dev, "ti,ti,otg-voltage is error\n");
			return -ENODEV;
		}
		if (props[i].tbl_id == TBL_OTGVOL &&
		    property > MAX_OTGCURRENT) {
			dev_err(charger->dev, "ti,otg-current is error\n");
			return -ENODEV;
		}

		*props[i].conv_data = bq25700_find_idx(property,
						       props[i].tbl_id);
		DBG("%s, val: %d, tbl_id =%d\n", props[i].name, property,
		    *props[i].conv_data);
	}

	return 0;
}

static int bq25700_hw_init(struct bq25700_device *charger)
{
	int ret;
	struct bq25700_state state;
	u16 val = 0;

	/* disable watchdog */
	ret = bq25700_write_mask(charger, WDTWR_ADJ, WDTWR_ADJ_MASK,
					WDTWR_ADJ_SHIFT, 0);
	if (ret < 0)
		return ret;
	ret = bq25700_write_mask(charger, CHARGE_CURRENT, CHARGE_CURRENT_MASK,
			CHARGE_CURRENT_SHIFT, charger->init_data.ichg);
	if (ret < 0)
		return ret;
	ret = bq25700_write_mask(charger, MAX_CHARGE_VOLTAGE,
						MAX_CHARGE_VOLTAGE_MASK,
						MAX_CHARGE_VOLTAGE_SHIFT,
						charger->init_data.max_chg_vol);
	if (ret < 0)
		return ret;
	ret = bq25700_write_mask(charger, MIN_SYS_VOTAGE,
						MIN_SYS_VOTAGE_MASK,
						MIN_SYS_VOTAGE_SHIFT,
					charger->init_data.sys_min_voltage);
	if (ret < 0)
		return ret;
	ret = bq25700_write_mask(charger, OTG_VOLTAGE,
						OTG_VOLTAGE_MASK,
						OTG_VOLTAGE_SHIFT,
						charger->init_data.otg_current);
	if (ret < 0)
		return ret;
	ret = bq25700_write_mask(charger, OTG_CURRENT,
						OTG_CURRENT_MASK,
						OTG_CURRENT_SHIFT,
						charger->init_data.otg_current);
	if (ret < 0)
		return ret;

	bq25700_read_mask(charger, CHARGE_CURRENT,
						CHARGE_CURRENT_MASK,
						CHARGE_CURRENT_SHIFT,
						&val);
	DBG("    CHARGE_CURRENT: %dmA\n", val * 64);
	bq25700_read_mask(charger, MAX_CHARGE_VOLTAGE,
						MAX_CHARGE_VOLTAGE_MASK,
						MAX_CHARGE_VOLTAGE_SHIFT,
						&val);
	DBG("MAX_CHARGE_VOLTAGE: %dmV\n", val * 16);
	bq25700_read_mask(charger, INPUT_VOLTAGE,
						INPUT_VOLTAGE_MASK,
						INPUT_VOLTAGE_SHIFT,
						&val);
	DBG("	  INPUT_VOLTAGE: %dmV\n", 3200 + val * 64);
	bq25700_read_mask(charger, INPUT_CURRENT,
						INPUT_CURRENT_MASK,
						INPUT_CURRENT_SHIFT,
						&val);
	DBG("	  INPUT_CURRENT: %dmA\n", val * 50);
	bq25700_read_mask(charger, MIN_SYS_VOTAGE,
						MIN_SYS_VOTAGE_MASK,
						MIN_SYS_VOTAGE_SHIFT,
						&val);
	DBG("	 MIN_SYS_VOTAGE: %dmV\n", 1024 + val * 256);
	/* Configure ADC for continuous conversions. This does not enable it. */

	ret = bq25700_write_mask(charger, EN_LWPWR,
						EN_LWPWR_MASK,
						EN_LWPWR_SHIFT,
						0);
	if (ret < 0) {
		DBG("error: EN_LWPWR\n");
		return ret;
	}

	ret = bq25700_write_mask(charger, ADC_CONV,
						ADC_CONV_MASK,
						ADC_CONV_SHIFT,
						1);
	if (ret < 0) {
		DBG("error: ADC_CONV\n");
		return ret;
	}

	ret = bq25700_write_mask(charger, ADC_START,
						ADC_START_MASK,
						ADC_START_SHIFT,
						1);
	if (ret < 0) {
		DBG("error: ADC_START\n");
		return ret;
	}

	ret = bq25700_write_mask(charger, ADC_FULLSCALE,
						ADC_FULLSCALE_MASK,
						ADC_FULLSCALE_SHIFT,
						1);
	if (ret < 0) {
		DBG("error: ADC_FULLSCALE\n");
		return ret;
	}

	ret = bq25700_write_mask(charger, EN_ADC_CMPIN,
						EN_ADC_CMPIN_MASK,
						EN_ADC_CMPIN_SHIFT,
						1);
	if (ret < 0) {
		DBG("error: EN_ADC_CMPIN\n");
		return ret;
	}

	ret = bq25700_write_mask(charger, EN_ADC_VBUS,
						EN_ADC_VBUS_MASK,
						EN_ADC_VBUS_SHIFT,
						1);
	if (ret < 0) {
		DBG("error: EN_ADC_VBUS\n");
		return ret;
	}

	ret = bq25700_write_mask(charger, EN_ADC_PSYS,
						EN_ADC_PSYS_MASK,
						EN_ADC_PSYS_SHIFT,
						1);
	if (ret < 0) {
		DBG("error: EN_ADC_PSYS\n");
		return ret;
	}

	ret = bq25700_write_mask(charger, EN_ADC_IIN,
						EN_ADC_IIN_MASK,
						EN_ADC_IIN_SHIFT,
						1);
	if (ret < 0) {
		DBG("error: EN_ADC_IIN\n");
		return ret;
	}

	ret = bq25700_write_mask(charger, EN_ADC_IDCHG,
						EN_ADC_IDCHG_MASK,
						EN_ADC_IDCHG_SHIFT,
						1);
	if (ret < 0) {
		DBG("error: EN_ADC_IDCHG\n");
		return ret;
	}

	ret = bq25700_write_mask(charger, EN_ADC_ICHG,
						EN_ADC_ICHG_MASK,
						EN_ADC_ICHG_SHIFT,
						1);
	if (ret < 0) {
		DBG("error: EN_ADC_ICHG\n");
		return ret;
	}

	ret = bq25700_write_mask(charger, EN_ADC_VSYS,
						EN_ADC_VSYS_MASK,
						EN_ADC_VSYS_SHIFT,
						1);
	if (ret < 0) {
		DBG("error: EN_ADC_VSYS\n");
		return ret;
	}

	ret = bq25700_write_mask(charger, EN_ADC_VBAT,
						EN_ADC_VBAT_MASK,
						EN_ADC_VBAT_SHIFT,
						1);
	if (ret < 0) {
		DBG("error: EN_ADC_VBAT\n");
		return ret;
	}

	bq25700_get_chip_state(charger, &state);
	charger->state = state;

	return 0;
}

static int bq25700_fw_probe(struct bq25700_device *charger)
{
	int ret;

	ret = bq25700_fw_read_u32_props(charger);
	if (ret < 0)
		return ret;

	return 0;
}

static void bq25700_enable_charger(struct bq25700_device *charger,
				   u32 input_current)
{
	bq25700_write_mask(charger, INPUT_CURRENT,
						INPUT_CURRENT_MASK,
						INPUT_CURRENT_SHIFT,
						input_current);
	bq25700_write_mask(charger, CHARGE_CURRENT,
						CHARGE_CURRENT_MASK,
						CHARGE_CURRENT_SHIFT,
						charger->init_data.ichg);
}

static enum power_supply_property bq25700_power_supply_props[] = {
	POWER_SUPPLY_PROP_MANUFACTURER,
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
};

static int bq25700_power_supply_get_property(struct power_supply *psy,
					     enum power_supply_property psp,
					     union power_supply_propval *val)
{
	int ret;
	u16 v = 0;
	struct bq25700_device *bq = power_supply_get_drvdata(psy);
	struct bq25700_state state;

	state = bq->state;

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		if (!state.ac_stat)
			val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
		else if (state.in_fchrg == 1 ||
			 state.in_pchrg == 1)
			val->intval = POWER_SUPPLY_STATUS_CHARGING;
		else
			val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
		break;

	case POWER_SUPPLY_PROP_MANUFACTURER:
		val->strval = BQ25700_MANUFACTURER;
		break;

	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = state.ac_stat;
		break;

	case POWER_SUPPLY_PROP_HEALTH:
		if (!state.fault_acoc &&
		    !state.fault_acov && !state.fault_batoc)
			val->intval = POWER_SUPPLY_HEALTH_GOOD;
		else if (state.fault_batoc)
			val->intval = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
		break;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		/* read measured value */
		ret = bq25700_read_mask(bq, OUTPUT_CHG_CUR,
							OUTPUT_CHG_CUR_MASK,
							OUTPUT_CHG_CUR_SHIFT,
							&v);
		if (ret < 0)
			return ret;

		/* converted_val = ADC_val * 64mA  */
		val->intval = v * 64000;
		break;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		val->intval = bq25700_tables[TBL_ICHG].rt.max;
		break;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		if (!state.ac_stat) {
			val->intval = 0;
			break;
		}

		/* read measured value */
		ret = bq25700_read_mask(bq, OUTPUT_BAT_VOL,
							OUTPUT_BAT_VOL_MASK,
							OUTPUT_BAT_VOL_SHIFT,
							&v);
		if (ret < 0)
			return ret;

		/* converted_val = 2.88V + ADC_val * 64mV */
		val->intval = 2880000 + v * 64000;
		break;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX:
		val->intval = bq25700_tables[TBL_CHGMAX].rt.max;
		break;

	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX:
		val->intval = bq25700_tables[TBL_INPUTVOL].rt.max;
		break;

	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		val->intval = bq25700_tables[TBL_INPUTCUR].rt.max;
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

static char *bq25700_charger_supplied_to[] = {
	"charger",
};

static const struct power_supply_desc bq25700_power_supply_desc = {
	.name = "bq25700-charger",
	.type = POWER_SUPPLY_TYPE_MAINS,
	.properties = bq25700_power_supply_props,
	.num_properties = ARRAY_SIZE(bq25700_power_supply_props),
	.get_property = bq25700_power_supply_get_property,
};

static int bq25700_power_supply_init(struct bq25700_device *charger)
{
	struct power_supply_config psy_cfg = { .drv_data = charger, };

	psy_cfg.supplied_to = bq25700_charger_supplied_to;
	psy_cfg.num_supplicants = ARRAY_SIZE(bq25700_charger_supplied_to);

	charger->supply_charger =
		power_supply_register(charger->dev,
				      &bq25700_power_supply_desc,
				      &psy_cfg);

	return PTR_ERR_OR_ZERO(charger->supply_charger);
}

static irqreturn_t bq25700_irq_handler_thread(int irq, void *private)
{
	struct bq25700_device *charger = private;
	int ret, irq_flag;
	u16 val = 0;
	struct bq25700_state state;

	ret = bq25700_read_mask(charger, AC_STAT,
							AC_STAT_MASK,
							AC_STAT_SHIFT,
							&val);
	if (val) {
		irq_flag = IRQF_TRIGGER_LOW;
		bq25700_enable_charger(charger,
				charger->init_data.input_current_dcp);
		bq25700_get_chip_state(charger, &state);
		charger->state = state;
		power_supply_changed(charger->supply_charger);
	} else {
		irq_flag = IRQF_TRIGGER_HIGH;
		bq25700_get_chip_state(charger, &state);
		charger->state = state;
		power_supply_changed(charger->supply_charger);
	}
	if (is_suspend) {
		//send_power_key(1);
		//send_power_key(0);
	}

	irq_set_irq_type(irq, irq_flag | IRQF_ONESHOT);
	return IRQ_HANDLED;
}

#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
static void charger_earlysuspend(struct early_suspend *h)
{
	is_suspend = true;
}

static void charger_lateresume(struct early_suspend *h)
{
	is_suspend = false;
}
#endif

static int bq25700_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	u16 val = 0;
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct device *dev = &client->dev;
	struct bq25700_device *charger;
	struct device_node *np = client->dev.of_node;
	unsigned long irq_flags;
	int irq_pin;
	int ret = 0;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_WORD_DATA))
		return -EIO;

	charger = devm_kzalloc(&client->dev, sizeof(*charger), GFP_KERNEL);
	if (!charger)
		return -EINVAL;

	charger->client = client;
	charger->dev = dev;
	is_suspend = false;
	//charger->regmap = devm_regmap_init_i2c(client,
	//				       &bq25700_regmap_config);

	irq_pin = of_get_named_gpio_flags(np, "irq-gpio", 0,
			(enum of_gpio_flags *)&irq_flags);
	if (irq_pin < 0) {
		dev_err(dev, "irq_pin error\n");
		return -1;
	}
	client->irq = gpio_to_irq(irq_pin);
	i2c_set_clientdata(client, charger);

	/*read chip id. Confirm whether to support the chip*/
	ret = bq25700_read_mask(charger, DEVICE_ID,
							DEVICE_ID_MASK,
							DEVICE_ID_SHIFT,
							&val);
	if (ret < 0) {
		dev_err(dev, "Cannot read chip ID.\n");
		return ret;
	}
	charger->chip_id = val;
	if (!dev->platform_data) {
		ret = bq25700_fw_probe(charger);
		if (ret < 0) {
			dev_err(dev, "Cannot read device properties.\n");
			return ret;
		}
	} else {
		return -ENODEV;
	}

	ret = bq25700_hw_init(charger);
	if (ret < 0) {
		dev_err(dev, "Cannot initialize the chip.\n");
		return ret;
	}

	bq25700_power_supply_init(charger);
	//bq25700_init_ac();

	ret = gpio_request(irq_pin, "bq25700_irq_pin");
	if (ret) {
		dev_err(dev, "fail to request gpio :%d\n", client->irq);
		return ret;
	}

	bq25700_read_mask(charger, AC_STAT, AC_STAT_MASK, AC_STAT_SHIFT, &val);
	if (val)
		irq_flags = IRQF_TRIGGER_LOW;
	else
		irq_flags = IRQF_TRIGGER_HIGH;

	ret = devm_request_threaded_irq(dev, client->irq, NULL,
					bq25700_irq_handler_thread,
					irq_flags | IRQF_ONESHOT,
					"bq25700_irq", charger);
#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
	charger_early_suspend.level = EARLY_SUSPEND_LEVEL_DISABLE_FB;
	charger_early_suspend.suspend = charger_earlysuspend;
	charger_early_suspend.resume = charger_lateresume;
#endif
	register_early_suspend(&charger_early_suspend);
	if (ret)
		goto irq_fail;

irq_fail:
	return ret;
}

#ifdef CONFIG_PM_SLEEP
static int bq25700_pm_suspend(struct device *dev)
{
	return 0;
}

static int bq25700_pm_resume(struct device *dev)
{
	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(bq25700_pm_ops, bq25700_pm_suspend, bq25700_pm_resume);

static const struct i2c_device_id bq25700_i2c_ids[] = {
	{ "bq25703", 0},
	{ },
};
MODULE_DEVICE_TABLE(i2c, bq25700_i2c_ids);

static const struct of_device_id bq25700_of_match[] = {
	{ .compatible = "bq25703", },
	{ },
};
MODULE_DEVICE_TABLE(i2c, bq25700_of_match);

static struct i2c_driver bq25700_driver = {
	.probe		= bq25700_probe,
	.id_table	= bq25700_i2c_ids,
	.driver = {
		.name		= "bq25703",
		.owner  = THIS_MODULE,
		.pm		= &bq25700_pm_ops,
		.of_match_table	= of_match_ptr(bq25700_of_match),
	},
};
module_i2c_driver(bq25700_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("shengfeixu <xsf@rock-chips.com>");
MODULE_DESCRIPTION("TI bq25700 Charger Driver");
