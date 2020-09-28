#ifndef	_LINUX_YK_SPLY_H_
#define	_LINUX_YK_SPLY_H_

static 	struct input_dev * powerkeydev;

/*      YK      */
#define YK_CHARGE_STATUS		YK_STATUS
#define YK_IN_CHARGE			(1 << 6)
#define YK_PDBC			(0x32)
#define YK_CHARGE_CONTROL1		YK_CHARGE1
#define YK_CHARGER_ENABLE		(1 << 7)
#define YK_CHARGE_CONTROL2		YK_CHARGE2
#define YK_CHARGE_VBUS		YK_IPS_SET
#define YK_CAP			(0xB9)
#define YK_BATCAP0			(0xe0)
#define YK_BATCAP1			(0xe1)
#define YK_RDC0			(0xba)
#define YK_RDC1			(0xbb)
#define YK_WARNING_LEVEL		(0xe6)
#define YK_ADJUST_PARA		(0xe8)
#define YK_FAULT_LOG1		YK_MODE_CHGSTATUS
#define YK_FAULT_LOG_CHA_CUR_LOW	(1 << 2)
#define YK_FAULT_LOG_BATINACT	(1 << 3)
#define YK_FAULT_LOG_OVER_TEMP	(1 << 7)
#define YK_FAULT_LOG2		YK_INTSTS2
#define YK_FAULT_LOG_COLD		(1 << 0)
#define YK_FINISH_CHARGE		(1 << 2)
#define YK_COULOMB_CONTROL		YK_COULOMB_CTL
#define YK_COULOMB_ENABLE		(1 << 7)
#define YK_COULOMB_SUSPEND		(1 << 6)
#define YK_COULOMB_CLEAR		(1 << 5)

#define YK_ADC_CONTROL				YK_ADC_EN
#define YK_ADC_BATVOL_ENABLE				(1 << 7)
#define YK_ADC_BATCUR_ENABLE				(1 << 6)
#define YK_ADC_DCINVOL_ENABLE			(1 << 5)
#define YK_ADC_DCINCUR_ENABLE			(1 << 4)
#define YK_ADC_USBVOL_ENABLE				(1 << 3)
#define YK_ADC_USBCUR_ENABLE				(1 << 2)
#define YK_ADC_APSVOL_ENABLE				(1 << 1)
#define YK_ADC_TSVOL_ENABLE				(1 << 0)
#define YK_ADC_INTERTEM_ENABLE			(1 << 7)
#define YK_ADC_GPIO0_ENABLE				(1 << 3)
#define YK_ADC_GPIO1_ENABLE				(1 << 2)
#define YK_ADC_GPIO2_ENABLE				(1 << 1)
#define YK_ADC_GPIO3_ENABLE				(1 << 0)
#define YK_ADC_CONTROL3				(0x84)
#define YK_VBATH_RES					(0x78)
#define YK_VTS_RES					(0x58)
#define YK_VBATL_RES					(0x79)
#define YK_OCVBATH_RES				(0xBC)
#define YK_OCVBATL_RES				(0xBD)
#define YK_INTTEMP					(0x56)
#define YK_DATA_BUFFER0				YK_BUFFER1
#define YK_DATA_BUFFER1				YK_BUFFER2
#define YK_DATA_BUFFER2				YK_BUFFER3
#define YK_DATA_BUFFER3				YK_BUFFER4
#define YK_DATA_BUFFER4				YK_BUFFER5
#define YK_DATA_BUFFER5				YK_BUFFER6
#define YK_DATA_BUFFER6				YK_BUFFER7
#define YK_DATA_BUFFER7				YK_BUFFER8
#define YK_DATA_BUFFER8				YK_BUFFER9
#define YK_DATA_BUFFER9				YK_BUFFERA
#define YK_DATA_BUFFERA				YK_BUFFERB
#define YK_DATA_BUFFERB				YK_BUFFERC


#define YK_CHG_ATTR(_name)					\
{								\
	.attr = { .name = #_name,.mode = 0644 },		\
	.show =  _name##_show,					\
	.store = _name##_store,					\
}

struct yk_adc_res {//struct change
	uint16_t vbat_res;
	uint16_t ocvbat_res;
	uint16_t ibat_res;
	uint16_t ichar_res;
	uint16_t idischar_res;
	uint16_t vac_res;
	uint16_t iac_res;
	uint16_t vusb_res;
	uint16_t iusb_res;
	uint16_t ts_res;
};

struct yk_charger {
	/*power supply sysfs*/
	struct power_supply *batt;
	struct power_supply	*ac;
	struct power_supply	*usb;
	struct power_supply bubatt;
	
	struct power_supply_desc batt_desc;
	struct power_supply_desc ac_desc;
	struct power_supply_desc usb_desc;

	/*i2c device*/
	struct device *master;

	/* adc */
	struct yk_adc_res *adc;
	unsigned int sample_time;

	/*monitor*/
	struct delayed_work work;
	unsigned int interval;

	/*battery info*/
	struct power_supply_info *battery_info;

	/*charger control*/
	bool chgen;
	bool limit_on;
	unsigned int chgcur;
	unsigned int chgvol;
	unsigned int chgend;

	/*charger time */
	int chgpretime;
	int chgcsttime;

	/*external charger*/
	bool chgexten;
	int chgextcur;

	/* charger status */
	bool bat_det;
	bool is_on;
	bool is_finish;
	bool ac_not_enough;
	bool ac_det;
	bool usb_det;
	bool ac_valid;
	bool usb_valid;
	bool ext_valid;
	bool bat_current_direction;
	bool in_short;
	bool batery_active;
	bool low_charge_current;
	bool int_over_temp;
	uint8_t fault;
	int charge_on;

	int vbat;
	int ibat;
	int pbat;
	int vac;
	int iac;
	int vusb;
	int iusb;
	int ocv;

	int disvbat;
	int disibat;

	/*rest time*/
	int rest_vol;
	int ocv_rest_vol;
	int base_restvol;
	int rest_time;

	/*ic temperature*/
	int ic_temp;
	int bat_temp;

	/*irq*/
	struct notifier_block nb;

	/* platform callbacks for battery low and critical events */
	void (*battery_low)(void);
	void (*battery_critical)(void);

	struct dentry *debug_file;
};

static struct task_struct *main_task;
static struct yk_charger *yk_charger;

#endif
