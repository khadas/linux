#ifndef __TSL2571_H__
#define __TSL2571_H__

#include <linux/sched.h>
#include <linux/time.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/earlysuspend.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/amlogic/sensor/sensor_common.h>


#define DRIVER_VERSION		"1.0"
#define TSL2571_DEVICE_NAME	"tsl2571"
#define OWNER_NAME			"tsl2571"
#define PON_DELAY			600
#define WAKEUP_DELAY		10
#define TSL2571_SCHE_DELAY	500
#if 1
#define dprintk(x...) printk(x)
#else
#define dprintk(x...)
#endif


/* Cal defs*/
#define PROX_STAT_CAL		0
#define PROX_STAT_SAMP		1
#define MAX_SAMPLES_CAL		200

/* TSL2X7X Device ID */
#define TRITON_ID    0x00
#define SWORDFISH_ID 0x30
#define HALIBUT_ID   0x20

/* Lux calculation constants */
#define TSL2X7X_LUX_CALC_OVER_FLOW     65535

/* TAOS Register definitions - note:
 * depending on device, some of these register are not used and the
 * register address is benign.
 */
/* 2X7X register offsets */
#define TSL2X7X_MAX_CONFIG_REG         16

/* Device Registers and Masks */
#define TSL2X7X_CNTRL                  0x00
#define TSL2X7X_ALS_TIME               0X01
#define TSL2X7X_PRX_TIME               0x02
#define TSL2X7X_WAIT_TIME              0x03
#define TSL2X7X_ALS_MINTHRESHLO        0X04
#define TSL2X7X_ALS_MINTHRESHHI        0X05
#define TSL2X7X_ALS_MAXTHRESHLO        0X06
#define TSL2X7X_ALS_MAXTHRESHHI        0X07
#define TSL2X7X_PRX_MINTHRESHLO        0X08
#define TSL2X7X_PRX_MINTHRESHHI        0X09
#define TSL2X7X_PRX_MAXTHRESHLO        0X0A
#define TSL2X7X_PRX_MAXTHRESHHI        0X0B
#define TSL2X7X_PERSISTENCE            0x0C
#define TSL2X7X_PRX_CONFIG             0x0D
#define TSL2X7X_PRX_COUNT              0x0E
#define TSL2X7X_GAIN                   0x0F
#define TSL2X7X_NOTUSED                0x10
#define TSL2X7X_REVID                  0x11
#define TSL2X7X_CHIPID                 0x12
#define TSL2X7X_STATUS                 0x13
#define TSL2X7X_ALS_CHAN0LO            0x14
#define TSL2X7X_ALS_CHAN0HI            0x15
#define TSL2X7X_ALS_CHAN1LO            0x16
#define TSL2X7X_ALS_CHAN1HI            0x17
#define TSL2X7X_PRX_LO                 0x18
#define TSL2X7X_PRX_HI                 0x19

/* tsl2X7X cmd reg masks */
#define TSL2X7X_CMD_REG                0x80
#define TSL2X7X_CMD_SPL_FN             0x60

#define TSL2X7X_CMD_PROX_INT_CLR       0X05
#define TSL2X7X_CMD_ALS_INT_CLR        0x06
#define TSL2X7X_CMD_PROXALS_INT_CLR    0X07

/* tsl2X7X cntrl reg masks */
#define TSL2X7X_CNTL_ADC_ENBL          0x02
#define TSL2X7X_CNTL_PWR_ON            0x01

/* tsl2X7X status reg masks */
#define TSL2X7X_STA_ADC_VALID          0x01
#define TSL2X7X_STA_PRX_VALID          0x02
#define TSL2X7X_STA_ADC_PRX_VALID      (TSL2X7X_STA_ADC_VALID |\
					TSL2X7X_STA_PRX_VALID)
#define TSL2X7X_STA_ALS_INTR           0x10
#define TSL2X7X_STA_PRX_INTR           0x20

/* tsl2X7X cntrl reg masks */
#define TSL2X7X_CNTL_REG_CLEAR         0x00
#define TSL2X7X_CNTL_PROX_INT_ENBL     0X20
#define TSL2X7X_CNTL_ALS_INT_ENBL      0X10
#define TSL2X7X_CNTL_WAIT_TMR_ENBL     0X08
#define TSL2X7X_CNTL_PROX_DET_ENBL     0X04
#define TSL2X7X_CNTL_PWRON             0x01
#define TSL2X7X_CNTL_ALSPON_ENBL       0x03
#define TSL2X7X_CNTL_INTALSPON_ENBL    0x13
#define TSL2X7X_CNTL_PROXPON_ENBL      0x0F
#define TSL2X7X_CNTL_INTPROXPON_ENBL   0x2F

/*Prox diode to use */
#define TSL2X7X_DIODE0                 0x10
#define TSL2X7X_DIODE1                 0x20
#define TSL2X7X_DIODE_BOTH             0x30

/* LED Power */
#define TSL2X7X_mA100                  0x00
#define TSL2X7X_mA50                   0x40
#define TSL2X7X_mA25                   0x80
#define TSL2X7X_mA13                   0xD0
#define TSL2X7X_MAX_TIMER_CNT          (0xFF)

/*input*/
#define ABSMIN				0
#define ABSMAX				65535
#define TSL2X7X_MIN_ITIME 3

/*LUX coefficients*/
#define LUX_COEFF		53

enum {
	TSL2X7X_CHIP_UNKNOWN = 0,
	TSL2X7X_CHIP_WORKING = 1,
	TSL2X7X_CHIP_SUSPENDED = 2
};
u8 tsl2571_config[TSL2X7X_MAX_CONFIG_REG];
/*struct data*/
struct tsl2x7x_lux {
	unsigned int ratio;
	unsigned int ch0;
	unsigned int ch1;
};
struct tsl2x7x_als_info {
	u16 als_ch0;
	u16 als_ch1;
	u16 lux;
};
static const s16 tsl2X7X_als_gainadj[] = {
	1,
	8,
	16,
	120
};
/**
 * struct tsl2x7x_default_settings - power on defaults unless
 *                                   overridden by platform data.
 *  @als_time:              ALS Integration time - multiple of 50mS
 *  @als_gain:              Index into the ALS gain table.
 *  @als_gain_trim:         default gain trim to account for
 *                          aperture effects.
 *  @als_ga:			glass attenuation (GA=1) als_ga =100
 *  @wait_time:             Time between PRX and ALS cycles
 *                          in 2.7 periods
 *  @prx_time:              5.2ms prox integration time -
 *                          decrease in 2.7ms periods
 *  @prx_gain:              Proximity gain index
 *  @prox_config:           Prox configuration filters.
 *  @als_cal_target:        Known external ALS reading for
 *                          calibration.
 *  @interrupts_en:         Enable/Disable - 0x00 = none, 0x10 = als,
 *                                           0x20 = prx,  0x30 = bth
 *  @persistence:           H/W Filters, Number of 'out of limits'
 *                          ADC readings PRX/ALS.
 *  @als_thresh_low:        CH0 'low' count to trigger interrupt.
 *  @als_thresh_high:       CH0 'high' count to trigger interrupt.
 *  @prox_thres_low:        Low threshold proximity detection.
 *  @prox_thres_high:       High threshold proximity detection
 *  @prox_pulse_count:      Number if proximity emitter pulses
 *  @prox_max_samples_cal:  Used for prox cal.
 */
struct tsl2571_settings {
	int als_time;
	int als_gain;
	int als_gain_trim;
	int als_ga;
	int wait_time;
	int prox_config;
	int als_cal_target;
	u8  interrupts_en;
	u8  persistence;
	int als_thresh_low;
	int als_thresh_high;
};
struct tsl2571_data {
	struct i2c_client *client;
	struct delayed_work work;
	struct input_dev *input_dev;
	struct class ltr_cls;
	struct tsl2x7x_als_info als_cur_info;
	struct mutex tsl2571_mutex;
	atomic_t delay;
	int	als_enabled;
	int	tsl2x7x_chip_status;
	int	als_saturation;
	int	als_time_scale;
	struct mutex als_mutex;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif
};
#endif
