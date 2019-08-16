// SPDX-License-Identifier: GPL-2.0+
/*
 * apds9960.c - Support for Avago APDS9960 gesture/RGB/ALS/proximity sensor
 *
 * Copyright (C) 2015, 2018
 * Author: Matt Ranostay <matt.ranostay@konsulko.com>
 *
 * TODO: gesture + proximity calib offsets
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/err.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>
#include <linux/iio/iio.h>
#include <linux/iio/buffer.h>
#include <linux/platform_device.h>
#include <linux/iio/events.h>
#include <linux/iio/kfifo_buf.h>
#include <linux/iio/sysfs.h>
#include <linux/of_gpio.h>
#include <linux/input.h>
#include <linux/gpio/consumer.h>
#include <linux/pinctrl/consumer.h>

#define APDS9960_REGMAP_NAME	"apds9960_regmap"
#define APDS9960_DRV_NAME	"apds9960"

#define APDS9960_REG_RAM_START	0x00
#define APDS9960_REG_RAM_END	0x7f

#define APDS9960_REG_ENABLE	0x80
#define APDS9960_REG_ATIME	0x81
#define APDS9960_REG_WTIME	0x83

#define APDS9960_REG_AILTL	0x84
#define APDS9960_REG_AILTH	0x85
#define APDS9960_REG_AIHTL	0x86
#define APDS9960_REG_AIHTH	0x87

#define APDS9960_REG_PILT	0x89
#define APDS9960_REG_PIHT	0x8b
#define APDS9960_REG_PERS	0x8c

#define APDS9960_REG_CONFIG_1	0x8d
#define APDS9960_REG_PPULSE	0x8e

#define APDS9960_REG_CONTROL	0x8f
#define APDS9960_REG_CONTROL_AGAIN_MASK		0x03
#define APDS9960_REG_CONTROL_PGAIN_MASK		0x0c
#define APDS9960_REG_CONTROL_AGAIN_MASK_SHIFT	0
#define APDS9960_REG_CONTROL_PGAIN_MASK_SHIFT	2

#define APDS9960_REG_CONFIG_2	0x90
#define APDS9960_REG_CONFIG_2_GGAIN_MASK	0x60
#define APDS9960_REG_CONFIG_2_GGAIN_MASK_SHIFT	5

#define APDS9960_REG_ID		0x92

#define APDS9960_REG_STATUS	0x93
#define APDS9960_REG_STATUS_PS_INT	BIT(5)
#define APDS9960_REG_STATUS_ALS_INT	BIT(4)
#define APDS9960_REG_STATUS_GINT	BIT(2)

#define APDS9960_REG_PDATA	0x9c
#define APDS9960_REG_POFFSET_UR	0x9d
#define APDS9960_REG_POFFSET_DL 0x9e
#define APDS9960_REG_CONFIG_3	0x9f

#define APDS9960_REG_GPENTH	0xa0
#define APDS9960_REG_GEXTH	0xa1

#define APDS9960_REG_GCONF_1	0xa2
#define APDS9960_REG_GCONF_1_GFIFO_THRES_MASK		0xc0
#define APDS9960_REG_GCONF_1_GFIFO_THRES_MASK_SHIFT	6

#define APDS9960_REG_GCONF_2	0xa3
#define APDS9960_REG_GOFFSET_U	0xa4
#define APDS9960_REG_GOFFSET_D	0xa5
#define APDS9960_REG_GPULSE	0xa6
#define APDS9960_REG_GOFFSET_L	0xa7
#define APDS9960_REG_GOFFSET_R	0xa9
#define APDS9960_REG_GCONF_3	0xaa

#define APDS9960_REG_GCONF_4	0xab
#define APDS9960_REG_GFLVL	0xae
#define APDS9960_REG_GSTATUS	0xaf

#define APDS9960_REG_IFORCE	0xe4
#define APDS9960_REG_PICLEAR	0xe5
#define APDS9960_REG_CICLEAR	0xe6
#define APDS9960_REG_AICLEAR	0xe7

#define APDS9960_DEFAULT_PERS	0x33
#define APDS9960_DEFAULT_GPENTH	0x50
#define APDS9960_DEFAULT_GEXTH	0x40

#define APDS9960_MAX_PXS_THRES_VAL	255
#define APDS9960_MAX_ALS_THRES_VAL	0xffff
#define APDS9960_MAX_INT_TIME_IN_US	1000000

extern int get_board_type(void);
static int board_type;

enum apds9960_als_channel_idx {
	IDX_ALS_CLEAR, IDX_ALS_RED, IDX_ALS_GREEN, IDX_ALS_BLUE,
};

#define APDS9960_REG_ALS_BASE	0x94
#define APDS9960_REG_ALS_CHANNEL(_colour) \
	(APDS9960_REG_ALS_BASE + (IDX_ALS_##_colour * 2))

enum apds9960_gesture_channel_idx {
	IDX_DIR_UP, IDX_DIR_DOWN, IDX_DIR_LEFT, IDX_DIR_RIGHT,
};

#define APDS9960_REG_GFIFO_BASE	0xfc
#define APDS9960_REG_GFIFO_DIR(_dir) \
	(APDS9960_REG_GFIFO_BASE + IDX_DIR_##_dir)

struct apds9960_data {
	struct i2c_client *client;
	struct iio_dev *indio_dev;
	struct mutex lock;
	struct gpio_desc *led_r_gpio;
	int led_r_gpio_pin;

	/* regmap fields */
	struct regmap *regmap;
	struct regmap_field *reg_int_als;
	struct regmap_field *reg_int_ges;
	struct regmap_field *reg_int_pxs;

	struct regmap_field *reg_enable_als;
	struct regmap_field *reg_enable_ges;
	struct regmap_field *reg_enable_pxs;

	/* state */
	int als_int;
	int pxs_int;
	int gesture_mode_running;

	/* gain values */
	int als_gain;
	int pxs_gain;

	/* integration time value in us */
	int als_adc_int_us;

	/* gesture buffer */
	u8 buffer[4]; /* 4 8-bit channels */
};
struct apds9960_data *data_p;

static const struct reg_default apds9960_reg_defaults[] = {
	/* Default ALS integration time = 2.48ms */
	{ APDS9960_REG_ATIME, 0xff },
};

static const struct regmap_range apds9960_volatile_ranges[] = {
	regmap_reg_range(APDS9960_REG_STATUS,
				APDS9960_REG_PDATA),
	regmap_reg_range(APDS9960_REG_GFLVL,
				APDS9960_REG_GSTATUS),
	regmap_reg_range(APDS9960_REG_GFIFO_DIR(UP),
				APDS9960_REG_GFIFO_DIR(RIGHT)),
	regmap_reg_range(APDS9960_REG_IFORCE,
				APDS9960_REG_AICLEAR),
};

static const struct regmap_access_table apds9960_volatile_table = {
	.yes_ranges	= apds9960_volatile_ranges,
	.n_yes_ranges	= ARRAY_SIZE(apds9960_volatile_ranges),
};

static const struct regmap_range apds9960_precious_ranges[] = {
	regmap_reg_range(APDS9960_REG_RAM_START, APDS9960_REG_RAM_END),
};

static const struct regmap_access_table apds9960_precious_table = {
	.yes_ranges	= apds9960_precious_ranges,
	.n_yes_ranges	= ARRAY_SIZE(apds9960_precious_ranges),
};

static const struct regmap_range apds9960_readable_ranges[] = {
	regmap_reg_range(APDS9960_REG_ENABLE,
				APDS9960_REG_GSTATUS),
	regmap_reg_range(APDS9960_REG_GFIFO_DIR(UP),
				APDS9960_REG_GFIFO_DIR(RIGHT)),
};

static const struct regmap_access_table apds9960_readable_table = {
	.yes_ranges	= apds9960_readable_ranges,
	.n_yes_ranges	= ARRAY_SIZE(apds9960_readable_ranges),
};

static const struct regmap_range apds9960_writeable_ranges[] = {
	regmap_reg_range(APDS9960_REG_ENABLE, APDS9960_REG_CONFIG_2),
	regmap_reg_range(APDS9960_REG_POFFSET_UR, APDS9960_REG_GCONF_4),
	regmap_reg_range(APDS9960_REG_IFORCE, APDS9960_REG_AICLEAR),
};

static const struct regmap_access_table apds9960_writeable_table = {
	.yes_ranges	= apds9960_writeable_ranges,
	.n_yes_ranges	= ARRAY_SIZE(apds9960_writeable_ranges),
};

static const struct regmap_config apds9960_regmap_config = {
	.name = APDS9960_REGMAP_NAME,
	.reg_bits = 8,
	.val_bits = 8,
	.use_single_rw = 1,

	.volatile_table = &apds9960_volatile_table,
	.precious_table = &apds9960_precious_table,
	.rd_table = &apds9960_readable_table,
	.wr_table = &apds9960_writeable_table,

	.reg_defaults = apds9960_reg_defaults,
	.num_reg_defaults = ARRAY_SIZE(apds9960_reg_defaults),
	.max_register = APDS9960_REG_GFIFO_DIR(RIGHT),
	.cache_type = REGCACHE_RBTREE,
};

static const struct iio_event_spec apds9960_pxs_event_spec[] = {
	{
		.type = IIO_EV_TYPE_THRESH,
		.dir = IIO_EV_DIR_RISING,
		.mask_separate = BIT(IIO_EV_INFO_VALUE) |
			BIT(IIO_EV_INFO_ENABLE),
	},
	{
		.type = IIO_EV_TYPE_THRESH,
		.dir = IIO_EV_DIR_FALLING,
		.mask_separate = BIT(IIO_EV_INFO_VALUE) |
			BIT(IIO_EV_INFO_ENABLE),
	},
};

static const struct iio_event_spec apds9960_als_event_spec[] = {
	{
		.type = IIO_EV_TYPE_THRESH,
		.dir = IIO_EV_DIR_RISING,
		.mask_separate = BIT(IIO_EV_INFO_VALUE) |
			BIT(IIO_EV_INFO_ENABLE),
	},
	{
		.type = IIO_EV_TYPE_THRESH,
		.dir = IIO_EV_DIR_FALLING,
		.mask_separate = BIT(IIO_EV_INFO_VALUE) |
			BIT(IIO_EV_INFO_ENABLE),
	},
};

#define APDS9960_GESTURE_CHANNEL(_dir, _si) { \
	.type = IIO_PROXIMITY, \
	.channel = _si + 1, \
	.scan_index = _si, \
	.indexed = 1, \
	.scan_type = { \
		.sign = 'u', \
		.realbits = 8, \
		.storagebits = 8, \
	}, \
}

#define APDS9960_INTENSITY_CHANNEL(_colour) { \
	.type = IIO_INTENSITY, \
	.info_mask_separate = BIT(IIO_CHAN_INFO_RAW), \
	.info_mask_shared_by_type = BIT(IIO_CHAN_INFO_SCALE) | \
			BIT(IIO_CHAN_INFO_INT_TIME), \
	.channel2 = IIO_MOD_LIGHT_##_colour, \
	.address = APDS9960_REG_ALS_CHANNEL(_colour), \
	.modified = 1, \
	.scan_index = -1, \
}

static const unsigned long apds9960_scan_masks[] = {0xf, 0};

static const struct iio_chan_spec apds9960_channels[] = {
	{
		.type = IIO_PROXIMITY,
		.address = APDS9960_REG_PDATA,
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),
		.info_mask_shared_by_type = BIT(IIO_CHAN_INFO_SCALE),
		.channel = 0,
		.indexed = 0,
		.scan_index = -1,

		.event_spec = apds9960_pxs_event_spec,
		.num_event_specs = ARRAY_SIZE(apds9960_pxs_event_spec),
	},
	/* Gesture Sensor */
	APDS9960_GESTURE_CHANNEL(UP, 0),
	APDS9960_GESTURE_CHANNEL(DOWN, 1),
	APDS9960_GESTURE_CHANNEL(LEFT, 2),
	APDS9960_GESTURE_CHANNEL(RIGHT, 3),
	/* ALS */
	{
		.type = IIO_INTENSITY,
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),
		.info_mask_shared_by_type = BIT(IIO_CHAN_INFO_SCALE) |
			BIT(IIO_CHAN_INFO_INT_TIME),
		.channel2 = IIO_MOD_LIGHT_CLEAR,
		.address = APDS9960_REG_ALS_CHANNEL(CLEAR),
		.modified = 1,
		.scan_index = -1,

		.event_spec = apds9960_als_event_spec,
		.num_event_specs = ARRAY_SIZE(apds9960_als_event_spec),
	},
	/* RGB Sensor */
	APDS9960_INTENSITY_CHANNEL(RED),
	APDS9960_INTENSITY_CHANNEL(GREEN),
	APDS9960_INTENSITY_CHANNEL(BLUE),
};

/* integration time in us */
static const int apds9960_int_time[][2] = {
	{ 28000, 246},
	{100000, 219},
	{200000, 182},
	{700000,   0}
};

/* gain mapping */
static const int apds9960_pxs_gain_map[] = {1, 2, 4, 8};
static const int apds9960_als_gain_map[] = {1, 4, 16, 64};

static unsigned int khadas_gesture_test_mode = 0;
/* Acceptable device IDs */
#define APDS9960_ID_1           0xAB
#define APDS9960_ID_2           0x9C 

/* Misc parameters */
#define FIFO_PAUSE_TIME         30      // Wait period (ms) between FIFO reads

/* APDS-9960 register addresses */
#define APDS9960_ENABLE         0x80
#define APDS9960_ATIME          0x81
#define APDS9960_WTIME          0x83
#define APDS9960_AILTL          0x84
#define APDS9960_AILTH          0x85
#define APDS9960_AIHTL          0x86
#define APDS9960_AIHTH          0x87
#define APDS9960_PILT           0x89
#define APDS9960_PIHT           0x8B
#define APDS9960_PERS           0x8C
#define APDS9960_CONFIG1        0x8D
#define APDS9960_PPULSE         0x8E
#define APDS9960_CONTROL        0x8F
#define APDS9960_CONFIG2        0x90
#define APDS9960_ID             0x92
#define APDS9960_STATUS         0x93
#define APDS9960_CDATAL         0x94
#define APDS9960_CDATAH         0x95
#define APDS9960_RDATAL         0x96
#define APDS9960_RDATAH         0x97
#define APDS9960_GDATAL         0x98
#define APDS9960_GDATAH         0x99
#define APDS9960_BDATAL         0x9A
#define APDS9960_BDATAH         0x9B
#define APDS9960_PDATA          0x9C
#define APDS9960_POFFSET_UR     0x9D
#define APDS9960_POFFSET_DL     0x9E
#define APDS9960_CONFIG3        0x9F
#define APDS9960_GPENTH         0xA0
#define APDS9960_GEXTH          0xA1
#define APDS9960_GCONF1         0xA2
#define APDS9960_GCONF2         0xA3
#define APDS9960_GOFFSET_U      0xA4
#define APDS9960_GOFFSET_D      0xA5
#define APDS9960_GOFFSET_L      0xA7
#define APDS9960_GOFFSET_R      0xA9
#define APDS9960_GPULSE         0xA6
#define APDS9960_GCONF3         0xAA
#define APDS9960_GCONF4         0xAB
#define APDS9960_GFLVL          0xAE
#define APDS9960_GSTATUS        0xAF
#define APDS9960_IFORCE         0xE4
#define APDS9960_PICLEAR        0xE5
#define APDS9960_CICLEAR        0xE6
#define APDS9960_AICLEAR        0xE7
#define APDS9960_GFIFO_U        0xFC
#define APDS9960_GFIFO_D        0xFD
#define APDS9960_GFIFO_L        0xFE
#define APDS9960_GFIFO_R        0xFF

/* Bit fields */
#define APDS9960_PON            0b00000001
#define APDS9960_AEN            0b00000010
#define APDS9960_PEN            0b00000100
#define APDS9960_WEN            0b00001000
#define APSD9960_AIEN           0b00010000
#define APDS9960_PIEN           0b00100000
#define APDS9960_GEN            0b01000000
#define APDS9960_GVALID         0b00000001

/* On/Off definitions */
#define OFF                     0
#define ON                      1

/* Acceptable parameters for setMode */
#define POWER                   0
#define AMBIENT_LIGHT           1
#define PROXIMITY               2
#define WAIT                    3
#define AMBIENT_LIGHT_INT       4
#define PROXIMITY_INT           5
#define GESTURE                 6
#define ALL                     7

/* LED Drive values */
#define LED_DRIVE_100MA         0
#define LED_DRIVE_50MA          1
#define LED_DRIVE_25MA          2
#define LED_DRIVE_12_5MA        3

/* Proximity Gain (PGAIN) values */
#define PGAIN_1X                0
#define PGAIN_2X                1
#define PGAIN_4X                2
#define PGAIN_8X                3

/* ALS Gain (AGAIN) values */
#define AGAIN_1X                0
#define AGAIN_4X                1
#define AGAIN_16X               2
#define AGAIN_64X               3

/* Gesture Gain (GGAIN) values */
#define GGAIN_1X                0
#define GGAIN_2X                1
#define GGAIN_4X                2
#define GGAIN_8X                3

/* LED Boost values */
#define LED_BOOST_100           0
#define LED_BOOST_150           1
#define LED_BOOST_200           2
#define LED_BOOST_300           3    

/* Gesture wait time values */
#define GWTIME_0MS              0
#define GWTIME_2_8MS            1
#define GWTIME_5_6MS            2
#define GWTIME_8_4MS            3
#define GWTIME_14_0MS           4
#define GWTIME_22_4MS           5
#define GWTIME_30_8MS           6
#define GWTIME_39_2MS           7

/* Default values */
#define DEFAULT_ATIME           219     // 103ms
#define DEFAULT_WTIME           246     // 27ms
#define DEFAULT_PROX_PPULSE     0x87    // 16us, 8 pulses
#define DEFAULT_GESTURE_PPULSE  0x89    // 16us, 10 pulses
#define DEFAULT_POFFSET_UR      0       // 0 offset
#define DEFAULT_POFFSET_DL      0       // 0 offset      
#define DEFAULT_CONFIG1         0x60    // No 12x wait (WTIME) factor
#define DEFAULT_LDRIVE          LED_DRIVE_100MA
#define DEFAULT_PGAIN           PGAIN_4X
#define DEFAULT_AGAIN           AGAIN_4X
#define DEFAULT_PILT            0       // Low proximity threshold
#define DEFAULT_PIHT            50      // High proximity threshold
#define DEFAULT_AILT            0xFFFF  // Force interrupt for calibration
#define DEFAULT_AIHT            0
#define DEFAULT_PERS            0x11    // 2 consecutive prox or ALS for int.
#define DEFAULT_CONFIG2         0x01    // No saturation interrupts or LED boost  
#define DEFAULT_CONFIG3         0       // Enable all photodiodes, no SAI
#define DEFAULT_GPENTH          40      // Threshold for entering gesture mode
#define DEFAULT_GEXTH           30      // Threshold for exiting gesture mode    
#define DEFAULT_GCONF1          0x40    // 4 gesture events for int., 1 for exit
#define DEFAULT_GGAIN           GGAIN_4X
#define DEFAULT_GLDRIVE         LED_DRIVE_100MA
#define DEFAULT_GWTIME          GWTIME_0MS//GWTIME_2_8MS
#define DEFAULT_GOFFSET         0       // No offset scaling for gesture mode
#define DEFAULT_GPULSE          0x40//0xC9    // 32us, 10 pulses
#define DEFAULT_GCONF3          0       // All photodiodes active during gesture
#define DEFAULT_GIEN            0       // Disable gesture interrupts

#define APDS9960_GVALID             0b00000001
/* Gesture parameters */
#define GESTURE_THRESHOLD_OUT   10
#define GESTURE_SENSITIVITY_1   15//50
#define GESTURE_SENSITIVITY_2   15//20

/* Direction definitions */
enum {
  DIR_NONE,
  DIR_LEFT,//1
  DIR_RIGHT,//2
  DIR_UP,//3
  DIR_DOWN,//4
  DIR_NEAR,//5
  DIR_FAR,//6
  DIR_ALL
};

/* State definitions */
enum {
  NA_STATE,
  NEAR_STATE,
  FAR_STATE,
  ALL_STATE
};

/* Container for gesture data */
typedef struct gesture_data_type {
    uint8_t u_data[32];
    uint8_t d_data[32];
    uint8_t l_data[32];
    uint8_t r_data[32];
    uint8_t index;
    uint8_t total_gestures;
    uint8_t in_threshold;
    uint8_t out_threshold;
} gesture_data_type;

/* Members */
static gesture_data_type gesture_data_;
static int gesture_ud_delta_;
static int gesture_lr_delta_;
static int gesture_ud_count_;
static int gesture_lr_count_;
static int gesture_near_count_;
static int gesture_far_count_;
static int gesture_state_;
static int gesture_motion_;

static IIO_CONST_ATTR(proximity_scale_available, "1 2 4 8");
static IIO_CONST_ATTR(intensity_scale_available, "1 4 16 64");
static IIO_CONST_ATTR_INT_TIME_AVAIL("0.028 0.1 0.2 0.7");

static struct attribute *apds9960_attributes[] = {
	&iio_const_attr_proximity_scale_available.dev_attr.attr,
	&iio_const_attr_intensity_scale_available.dev_attr.attr,
	&iio_const_attr_integration_time_available.dev_attr.attr,
	NULL,
};

static const struct attribute_group apds9960_attribute_group = {
	.attrs = apds9960_attributes,
};

static const struct reg_field apds9960_reg_field_int_als =
				REG_FIELD(APDS9960_REG_ENABLE, 4, 4);

static const struct reg_field apds9960_reg_field_int_ges =
				REG_FIELD(APDS9960_REG_GCONF_4, 1, 1);

static const struct reg_field apds9960_reg_field_int_pxs =
				REG_FIELD(APDS9960_REG_ENABLE, 5, 5);

static const struct reg_field apds9960_reg_field_enable_als =
				REG_FIELD(APDS9960_REG_ENABLE, 1, 1);

static const struct reg_field apds9960_reg_field_enable_ges =
				REG_FIELD(APDS9960_REG_ENABLE, 6, 6);

static const struct reg_field apds9960_reg_field_enable_pxs =
				REG_FIELD(APDS9960_REG_ENABLE, 2, 2);

static int apds9960_set_it_time(struct apds9960_data *data, int val2)
{
	int ret = -EINVAL;
	int idx;

	for (idx = 0; idx < ARRAY_SIZE(apds9960_int_time); idx++) {
		if (apds9960_int_time[idx][0] == val2) {
			mutex_lock(&data->lock);
			ret = regmap_write(data->regmap, APDS9960_REG_ATIME,
						 apds9960_int_time[idx][1]);
			if (!ret)
				data->als_adc_int_us = val2;
			mutex_unlock(&data->lock);
			break;
		}
	}

	return ret;
}

static int apds9960_set_pxs_gain(struct apds9960_data *data, int val)
{
	int ret = -EINVAL;
	int idx;

	for (idx = 0; idx < ARRAY_SIZE(apds9960_pxs_gain_map); idx++) {
		if (apds9960_pxs_gain_map[idx] == val) {
			/* pxs + gesture gains are mirrored */
			mutex_lock(&data->lock);
			ret = regmap_update_bits(data->regmap,
				APDS9960_REG_CONTROL,
				APDS9960_REG_CONTROL_PGAIN_MASK,
				idx << APDS9960_REG_CONTROL_PGAIN_MASK_SHIFT);
			if (ret) {
				mutex_unlock(&data->lock);
				break;
			}

			ret = regmap_update_bits(data->regmap,
				APDS9960_REG_CONFIG_2,
				APDS9960_REG_CONFIG_2_GGAIN_MASK,
				idx << APDS9960_REG_CONFIG_2_GGAIN_MASK_SHIFT);
			if (!ret)
				data->pxs_gain = idx;
			mutex_unlock(&data->lock);
			break;
		}
	}

	return ret;
}

static int apds9960_set_als_gain(struct apds9960_data *data, int val)
{
	int ret = -EINVAL;
	int idx;

	for (idx = 0; idx < ARRAY_SIZE(apds9960_als_gain_map); idx++) {
		if (apds9960_als_gain_map[idx] == val) {
			mutex_lock(&data->lock);
			ret = regmap_update_bits(data->regmap,
					APDS9960_REG_CONTROL,
					APDS9960_REG_CONTROL_AGAIN_MASK, idx);
			if (!ret)
				data->als_gain = idx;
			mutex_unlock(&data->lock);
			break;
		}
	}

	return ret;
}

#ifdef CONFIG_PM
static int apds9960_set_power_state(struct apds9960_data *data, bool on)
{
	struct device *dev = &data->client->dev;
	int ret = 0;

	mutex_lock(&data->lock);

	if (on) {
		int suspended;

		suspended = pm_runtime_suspended(dev);
		ret = pm_runtime_get_sync(dev);

		/* Allow one integration cycle before allowing a reading */
		if (suspended)
			usleep_range(data->als_adc_int_us,
				     APDS9960_MAX_INT_TIME_IN_US);
	} else {
		pm_runtime_mark_last_busy(dev);
		ret = pm_runtime_put_autosuspend(dev);
	}

	mutex_unlock(&data->lock);

	return ret;
}
#else
static int apds9960_set_power_state(struct apds9960_data *data, bool on)
{
	return 0;
}
#endif

static int apds9960_read_raw(struct iio_dev *indio_dev,
			     struct iio_chan_spec const *chan,
			     int *val, int *val2, long mask)
{
	struct apds9960_data *data = iio_priv(indio_dev);
	__le16 buf;
	int ret = -EINVAL;

	if (data->gesture_mode_running)
		return -EBUSY;

	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		apds9960_set_power_state(data, true);
		switch (chan->type) {
		case IIO_PROXIMITY:
			ret = regmap_read(data->regmap, chan->address, val);
			if (!ret)
				ret = IIO_VAL_INT;
			break;
		case IIO_INTENSITY:
			ret = regmap_bulk_read(data->regmap, chan->address,
					       &buf, 2);
			if (!ret) {
				ret = IIO_VAL_INT;
				*val = le16_to_cpu(buf);
			}
			break;
		default:
			ret = -EINVAL;
		}
		apds9960_set_power_state(data, false);
		break;
	case IIO_CHAN_INFO_INT_TIME:
		/* RGB + ALS sensors only have integration time */
		mutex_lock(&data->lock);
		switch (chan->type) {
		case IIO_INTENSITY:
			*val = 0;
			*val2 = data->als_adc_int_us;
			ret = IIO_VAL_INT_PLUS_MICRO;
			break;
		default:
			ret = -EINVAL;
		}
		mutex_unlock(&data->lock);
		break;
	case IIO_CHAN_INFO_SCALE:
		mutex_lock(&data->lock);
		switch (chan->type) {
		case IIO_PROXIMITY:
			*val = apds9960_pxs_gain_map[data->pxs_gain];
			ret = IIO_VAL_INT;
			break;
		case IIO_INTENSITY:
			*val = apds9960_als_gain_map[data->als_gain];
			ret = IIO_VAL_INT;
			break;
		default:
			ret = -EINVAL;
		}
		mutex_unlock(&data->lock);
		break;
	}

	return ret;
};

static int apds9960_write_raw(struct iio_dev *indio_dev,
			     struct iio_chan_spec const *chan,
			     int val, int val2, long mask)
{
	struct apds9960_data *data = iio_priv(indio_dev);

	switch (mask) {
	case IIO_CHAN_INFO_INT_TIME:
		/* RGB + ALS sensors only have int time */
		switch (chan->type) {
		case IIO_INTENSITY:
			if (val != 0)
				return -EINVAL;
			return apds9960_set_it_time(data, val2);
		default:
			return -EINVAL;
		}
	case IIO_CHAN_INFO_SCALE:
		if (val2 != 0)
			return -EINVAL;
		switch (chan->type) {
		case IIO_PROXIMITY:
			return apds9960_set_pxs_gain(data, val);
		case IIO_INTENSITY:
			return apds9960_set_als_gain(data, val);
		default:
			return -EINVAL;
		}
	default:
		return -EINVAL;
	};

	return 0;
}

static inline int apds9960_get_thres_reg(const struct iio_chan_spec *chan,
					 enum iio_event_direction dir,
					 u8 *reg)
{
	switch (dir) {
	case IIO_EV_DIR_RISING:
		switch (chan->type) {
		case IIO_PROXIMITY:
			*reg = APDS9960_REG_PIHT;
			break;
		case IIO_INTENSITY:
			*reg = APDS9960_REG_AIHTL;
			break;
		default:
			return -EINVAL;
		}
		break;
	case IIO_EV_DIR_FALLING:
		switch (chan->type) {
		case IIO_PROXIMITY:
			*reg = APDS9960_REG_PILT;
			break;
		case IIO_INTENSITY:
			*reg = APDS9960_REG_AILTL;
			break;
		default:
			return -EINVAL;
		}
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int apds9960_read_event(struct iio_dev *indio_dev,
			       const struct iio_chan_spec *chan,
			       enum iio_event_type type,
			       enum iio_event_direction dir,
			       enum iio_event_info info,
			       int *val, int *val2)
{
	u8 reg;
	__le16 buf;
	int ret = 0;
	struct apds9960_data *data = iio_priv(indio_dev);

	if (info != IIO_EV_INFO_VALUE)
		return -EINVAL;

	ret = apds9960_get_thres_reg(chan, dir, &reg);
	if (ret < 0)
		return ret;

	if (chan->type == IIO_PROXIMITY) {
		ret = regmap_read(data->regmap, reg, val);
		if (ret < 0)
			return ret;
	} else if (chan->type == IIO_INTENSITY) {
		ret = regmap_bulk_read(data->regmap, reg, &buf, 2);
		if (ret < 0)
			return ret;
		*val = le16_to_cpu(buf);
	} else
		return -EINVAL;

	*val2 = 0;

	return IIO_VAL_INT;
}

static int apds9960_write_event(struct iio_dev *indio_dev,
				const struct iio_chan_spec *chan,
				enum iio_event_type type,
				enum iio_event_direction dir,
				enum iio_event_info info,
				int val, int val2)
{
	u8 reg;
	__le16 buf;
	int ret = 0;
	struct apds9960_data *data = iio_priv(indio_dev);

	if (info != IIO_EV_INFO_VALUE)
		return -EINVAL;

	ret = apds9960_get_thres_reg(chan, dir, &reg);
	if (ret < 0)
		return ret;

	if (chan->type == IIO_PROXIMITY) {
		if (val < 0 || val > APDS9960_MAX_PXS_THRES_VAL)
			return -EINVAL;
		ret = regmap_write(data->regmap, reg, val);
		if (ret < 0)
			return ret;
	} else if (chan->type == IIO_INTENSITY) {
		if (val < 0 || val > APDS9960_MAX_ALS_THRES_VAL)
			return -EINVAL;
		buf = cpu_to_le16(val);
		ret = regmap_bulk_write(data->regmap, reg, &buf, 2);
		if (ret < 0)
			return ret;
	} else
		return -EINVAL;

	return 0;
}

static int apds9960_read_event_config(struct iio_dev *indio_dev,
				      const struct iio_chan_spec *chan,
				      enum iio_event_type type,
				      enum iio_event_direction dir)
{
	struct apds9960_data *data = iio_priv(indio_dev);

	switch (chan->type) {
	case IIO_PROXIMITY:
		return data->pxs_int;
	case IIO_INTENSITY:
		return data->als_int;
	default:
		return -EINVAL;
	}

	return 0;
}

static int apds9960_write_event_config(struct iio_dev *indio_dev,
				       const struct iio_chan_spec *chan,
				       enum iio_event_type type,
				       enum iio_event_direction dir,
				       int state)
{
	struct apds9960_data *data = iio_priv(indio_dev);
	int ret;

	state = !!state;

	switch (chan->type) {
	case IIO_PROXIMITY:
		if (data->pxs_int == state)
			return -EINVAL;

		ret = regmap_field_write(data->reg_int_pxs, state);
		if (ret)
			return ret;
		data->pxs_int = state;
		apds9960_set_power_state(data, state);
		break;
	case IIO_INTENSITY:
		if (data->als_int == state)
			return -EINVAL;

		ret = regmap_field_write(data->reg_int_als, state);
		if (ret)
			return ret;
		data->als_int = state;
		apds9960_set_power_state(data, state);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static const struct iio_info apds9960_info = {
	.attrs = &apds9960_attribute_group,
	.read_raw = apds9960_read_raw,
	.write_raw = apds9960_write_raw,
	.read_event_value = apds9960_read_event,
	.write_event_value = apds9960_write_event,
	.read_event_config = apds9960_read_event_config,
	.write_event_config = apds9960_write_event_config,

};

static int num_flag = 0;
static inline int apds9660_fifo_is_empty(struct apds9960_data *data)
{
	//int cnt;
	int ret;

	ret = regmap_read(data->regmap, APDS9960_REG_GFLVL, &num_flag);
	//printk("apds9960 num = %d\n",num_flag);
	if (ret)
		return ret;

	return num_flag;
}

/*Processes the raw gesture data to determine swipe direction*/
static bool processGestureData(void)
{
    uint8_t u_first = 0;
    uint8_t d_first = 0;
    uint8_t l_first = 0;
    uint8_t r_first = 0;
    uint8_t u_last = 0;
    uint8_t d_last = 0;
    uint8_t l_last = 0;
    uint8_t r_last = 0;
    int ud_ratio_first;
    int lr_ratio_first;
    int ud_ratio_last;
    int lr_ratio_last;
    int ud_delta;
    int lr_delta;
    int i;
    
    /* If we have less than 4 total gestures, that's not enough */
    if( gesture_data_.total_gestures <= 4 ) {
		//printk("gesture_data_.total_gestures <= 4\n");
        return false;
    }
    
    /* Check to make sure our data isn't out of bounds */
    if( (gesture_data_.total_gestures <= 32) && \
        (gesture_data_.total_gestures > 0) ) {
        
        /* Find the first value in U/D/L/R above the threshold */
        for( i = 0; i < gesture_data_.total_gestures; i++ ) {
            if( (gesture_data_.u_data[i] > GESTURE_THRESHOLD_OUT) &&
                (gesture_data_.d_data[i] > GESTURE_THRESHOLD_OUT) &&
                (gesture_data_.l_data[i] > GESTURE_THRESHOLD_OUT) &&
                (gesture_data_.r_data[i] > GESTURE_THRESHOLD_OUT) ) {
                
                u_first = gesture_data_.u_data[i];
                d_first = gesture_data_.d_data[i];
                l_first = gesture_data_.l_data[i];
                r_first = gesture_data_.r_data[i];
                break;
            }
        }
        
        /* If one of the _first values is 0, then there is no good data */
        if( (u_first == 0) || (d_first == 0) || \
            (l_first == 0) || (r_first == 0) ) {
            
			//printk("*_first == 0\n");
            return false;
        }
        /* Find the last value in U/D/L/R above the threshold */
        for( i = gesture_data_.total_gestures - 1; i >= 0; i-- ) {

            if( (gesture_data_.u_data[i] > GESTURE_THRESHOLD_OUT) &&
                (gesture_data_.d_data[i] > GESTURE_THRESHOLD_OUT) &&
                (gesture_data_.l_data[i] > GESTURE_THRESHOLD_OUT) &&
                (gesture_data_.r_data[i] > GESTURE_THRESHOLD_OUT) ) {
                
                u_last = gesture_data_.u_data[i];
                d_last = gesture_data_.d_data[i];
                l_last = gesture_data_.l_data[i];
                r_last = gesture_data_.r_data[i];
                break;
            }
        }
    }
    
    /* Calculate the first vs. last ratio of up/down and left/right */
    ud_ratio_first = ((u_first - d_first) * 100) / (u_first + d_first);
    lr_ratio_first = ((l_first - r_first) * 100) / (l_first + r_first);
    ud_ratio_last = ((u_last - d_last) * 100) / (u_last + d_last);
    lr_ratio_last = ((l_last - r_last) * 100) / (l_last + r_last);
            
    /* Determine the difference between the first and last ratios */
    ud_delta = ud_ratio_last - ud_ratio_first;
    lr_delta = lr_ratio_last - lr_ratio_first;
   
    /* Accumulate the UD and LR delta values */
    gesture_ud_delta_ += ud_delta;
    gesture_lr_delta_ += lr_delta;
    // printk("ud= %d lr= %d\n",gesture_ud_delta_,gesture_lr_delta_);
    /* Determine U/D gesture */
    if( gesture_ud_delta_ >= GESTURE_SENSITIVITY_1 ) {
        gesture_ud_count_ = 1;
    } else if( gesture_ud_delta_ <= -GESTURE_SENSITIVITY_1 ) {
        gesture_ud_count_ = -1;
    } else {
        gesture_ud_count_ = 0;
    }
    
    /* Determine L/R gesture */
    if( gesture_lr_delta_ >= GESTURE_SENSITIVITY_1 ) {
        gesture_lr_count_ = 1;
    } else if( gesture_lr_delta_ <= -GESTURE_SENSITIVITY_1 ) {
        gesture_lr_count_ = -1;
    } else {
        gesture_lr_count_ = 0;
    }
    
    /* Determine Near/Far gesture */
    if( (gesture_ud_count_ == 0) && (gesture_lr_count_ == 0) ) {
        if( (abs(ud_delta) < GESTURE_SENSITIVITY_2) && \
            (abs(lr_delta) < GESTURE_SENSITIVITY_2) ) {
            
            if( (ud_delta == 0) && (lr_delta == 0) ) {
                gesture_near_count_++;
            } else if( (ud_delta != 0) || (lr_delta != 0) ) {
                gesture_far_count_++;
            }
            
            if( (gesture_near_count_ >= 10) && (gesture_far_count_ >= 2) ) {
                if( (ud_delta == 0) && (lr_delta == 0) ) {
                    gesture_state_ = NEAR_STATE;
                } else if( (ud_delta != 0) && (lr_delta != 0) ) {
                    gesture_state_ = FAR_STATE;
                }
                return true;
            }
        }
    } else {
        if( (abs(ud_delta) < GESTURE_SENSITIVITY_2) && \
            (abs(lr_delta) < GESTURE_SENSITIVITY_2) ) {
                
            if( (ud_delta == 0) && (lr_delta == 0) ) {
                gesture_near_count_++;
            }
            
            if( gesture_near_count_ >= 10 ) {
                gesture_ud_count_ = 0;
                gesture_lr_count_ = 0;
                gesture_ud_delta_ = 0;
                gesture_lr_delta_ = 0;
				//printk("all == 0\n");
            }
        }
    }
      
    return false;
}

/*Determines swipe direction or near/far state*/
static bool decodeGesture(void)
{
    /* Return if near or far event is detected */
    if( gesture_state_ == NEAR_STATE ) {
        gesture_motion_ = DIR_NEAR;
        return true;
    } else if ( gesture_state_ == FAR_STATE ) {
        gesture_motion_ = DIR_FAR;
        return true;
    }
    
    /* Determine swipe direction */
    if( (gesture_ud_count_ == -1) && (gesture_lr_count_ == 0) ) {
        gesture_motion_ = DIR_UP;
    } else if( (gesture_ud_count_ == 1) && (gesture_lr_count_ == 0) ) {
        gesture_motion_ = DIR_DOWN;
    } else if( (gesture_ud_count_ == 0) && (gesture_lr_count_ == 1) ) {
        gesture_motion_ = DIR_RIGHT;
    } else if( (gesture_ud_count_ == 0) && (gesture_lr_count_ == -1) ) {
        gesture_motion_ = DIR_LEFT;
    } else if( (gesture_ud_count_ == -1) && (gesture_lr_count_ == 1) ) {
        if( abs(gesture_ud_delta_) > abs(gesture_lr_delta_) ) {
            gesture_motion_ = DIR_UP;
        } else {
            gesture_motion_ = DIR_RIGHT;
        }
    } else if( (gesture_ud_count_ == 1) && (gesture_lr_count_ == -1) ) {
        if( abs(gesture_ud_delta_) > abs(gesture_lr_delta_) ) {
            gesture_motion_ = DIR_DOWN;
        } else {
            gesture_motion_ = DIR_LEFT;
        }
    } else if( (gesture_ud_count_ == -1) && (gesture_lr_count_ == -1) ) {
        if( abs(gesture_ud_delta_) > abs(gesture_lr_delta_) ) {
            gesture_motion_ = DIR_UP;
        } else {
            gesture_motion_ = DIR_LEFT;
        }
    } else if( (gesture_ud_count_ == 1) && (gesture_lr_count_ == 1) ) {
        if( abs(gesture_ud_delta_) > abs(gesture_lr_delta_) ) {
            gesture_motion_ = DIR_DOWN;
        } else {
            gesture_motion_ = DIR_RIGHT;
        }
    } else {
        return false;
    }
    
    return true;
}

/*Resets all the parameters in the gesture data member*/
static void resetGestureParameters(void)
{
   
    gesture_data_.index = 0;
    gesture_data_.total_gestures = 0;
    
    gesture_ud_delta_ = 0;
    gesture_lr_delta_ = 0;
    
    gesture_ud_count_ = 0;
    gesture_lr_count_ = 0;
    
    gesture_near_count_ = 0;
    gesture_far_count_ = 0;
    
    gesture_state_ = 0;
    gesture_motion_ = DIR_NONE;
}

/*Reads and returns the contents of the ENABLE register*/
static unsigned int getMode(struct apds9960_data *data)
{
    unsigned int enable_value;
    
    /* Read current ENABLE register */
	regmap_read(data->regmap, APDS9960_ENABLE, &enable_value);

    return enable_value;
}

/*Determines if there is a gesture available for reading*/
static bool isGestureAvailable(struct apds9960_data *data)
{
    unsigned int val;
    
    /* Read value from GSTATUS register */
	regmap_read(data->regmap, APDS9960_GSTATUS, &val);
    /* Shift and mask out GVALID bit */
    val &= APDS9960_GVALID;
    
    /* Return true/false based on GVALID bit */
    if( val == 1) {
        return true;
    } else {
        return false;
    }
}

extern struct input_dev *input_key_dev;
void Khadas_apds9960_test(int code)
{
	int key_code;

	/*static bool first_flag = 1;
	if(first_flag){
		input_set_capability(input_key_dev, EV_KEY, KEY_1);
		input_set_capability(input_key_dev, EV_KEY, KEY_2);
		input_set_capability(input_key_dev, EV_KEY, KEY_3);
		input_set_capability(input_key_dev, EV_KEY, KEY_4);
		first_flag = 0;
	}*/
	switch (code)
	{
	   case 1://DIR_LEFT
		key_code = KEY_LEFT;
	   break; 
	   
	   case 2://DIR_RIGHT
	   	key_code = KEY_RIGHT;
	   break; 	
	   
	   case 3://DIR_UP
		key_code = KEY_UP;
	   break; 

	   case 4://DIR_DOWN
	   	key_code = KEY_DOWN;
	   break; 
	   
	   case 5://DIR_NEAR
		key_code = KEY_5;
	   break; 
	   
	   case 6://DIR_FAR
	   	key_code = KEY_6;
	   break; 
	   
	   default:
	   return;
	}
	input_report_key(input_key_dev, key_code, 1);
	input_sync(input_key_dev);
	input_report_key(input_key_dev, key_code, 0);
	input_sync(input_key_dev);
}

/*Processes a gesture event and returns best guessed gesture*/
static int readGesture(struct apds9960_data *data)
{
	int motion;
	int num = 0;
	int cnt = 0;
	int ret;

	while (cnt || (cnt = (apds9660_fifo_is_empty(data) > 0))) {
		ret = regmap_bulk_read(data->regmap, APDS9960_REG_GFIFO_BASE, &data->buffer, 4);

		if (ret)
			return -1;
		//iio_push_to_buffers(data->indio_dev, data->buffer);
		
		if(!num)
			num = num_flag;
		gesture_data_.u_data[gesture_data_.index] = data->buffer[0];
		gesture_data_.d_data[gesture_data_.index] = data->buffer[1];
		gesture_data_.l_data[gesture_data_.index] = data->buffer[2];
		gesture_data_.r_data[gesture_data_.index] = data->buffer[3];
		gesture_data_.index++;
		gesture_data_.total_gestures++; 			
		cnt--;
		if(--num)
			continue;
		
        /* Filter and process gesture data. Decode near/far state */
        if( processGestureData() ) 
        {                     
            if( decodeGesture() ) { 
                //***TODO: U-Turn Gestures 
                Khadas_apds9960_test(gesture_motion_);
				printk("near/far state = %d\n",gesture_motion_);
			}
        }
        
        /* Reset data */
        gesture_data_.index = 0;
        gesture_data_.total_gestures = 0;
	} 

    decodeGesture();
    motion = gesture_motion_;
    resetGestureParameters();
    return motion;
 }

static void apds9960_read_gesture_fifo(struct apds9960_data *data)
{
	int ret, cnt = 0;

	mutex_lock(&data->lock);
	data->gesture_mode_running = 1;
	
	if(khadas_gesture_test_mode){
	    /* Make sure that power and gesture is on and data is valid */
	    if( !isGestureAvailable(data) || !(getMode(data) & 0b01000001) ) 
			goto err_read;

		ret = readGesture(data);
		Khadas_apds9960_test(ret);
		printk("l/r/u/p state = %d\n",ret);
	}		
	else{	
		while (cnt || (cnt = apds9660_fifo_is_empty(data) > 0)) {
			ret = regmap_bulk_read(data->regmap, APDS9960_REG_GFIFO_BASE,
					      &data->buffer, 4);

			if (ret)
				goto err_read;

			iio_push_to_buffers(data->indio_dev, data->buffer);
			cnt--;
		}
	}
	
err_read:
	data->gesture_mode_running = 0;
	mutex_unlock(&data->lock);
}

static irqreturn_t apds9960_interrupt_handler(int irq, void *private)
{
	struct iio_dev *indio_dev = private;
	struct apds9960_data *data = iio_priv(indio_dev);
	int ret, status;

	ret = regmap_read(data->regmap, APDS9960_REG_STATUS, &status);

	if (ret < 0) {
		dev_err(&data->client->dev, "irq status reg read failed\n");
		return IRQ_HANDLED;
	}

	if ((status & APDS9960_REG_STATUS_ALS_INT)) {
		iio_push_event(indio_dev,
			       IIO_UNMOD_EVENT_CODE(IIO_INTENSITY, 0,
						    IIO_EV_TYPE_THRESH,
						    IIO_EV_DIR_EITHER),
			       iio_get_time_ns(indio_dev));
		regmap_write(data->regmap, APDS9960_REG_CICLEAR, 1);
	}

	if ((status & APDS9960_REG_STATUS_PS_INT)) {
		iio_push_event(indio_dev,
			       IIO_UNMOD_EVENT_CODE(IIO_PROXIMITY, 0,
						    IIO_EV_TYPE_THRESH,
						    IIO_EV_DIR_EITHER),
			       iio_get_time_ns(indio_dev));
		regmap_write(data->regmap, APDS9960_REG_PICLEAR, 1);
	}
	//printk("apds9960 status = 0x%x\n",status);
	if (status & APDS9960_REG_STATUS_GINT) {
		apds9960_read_gesture_fifo(data);
	}

	return IRQ_HANDLED;
}

static int apds9960_set_powermode(struct apds9960_data *data, bool state)
{
	return regmap_update_bits(data->regmap, APDS9960_REG_ENABLE, 1, state);
}

static int apds9960_buffer_postenable(struct iio_dev *indio_dev)
{
	struct apds9960_data *data = iio_priv(indio_dev);
	int ret;

	ret = regmap_field_write(data->reg_int_ges, 1);
	if (ret)
		return ret;

	ret = regmap_field_write(data->reg_enable_ges, 1);
	if (ret)
		return ret;

	pm_runtime_get_sync(&data->client->dev);

	return 0;
}

static int apds9960_buffer_predisable(struct iio_dev *indio_dev)
{
	struct apds9960_data *data = iio_priv(indio_dev);
	int ret;

	ret = regmap_field_write(data->reg_enable_ges, 0);
	if (ret)
		return ret;

	ret = regmap_field_write(data->reg_int_ges, 0);
	if (ret)
		return ret;

	pm_runtime_put_autosuspend(&data->client->dev);

	return 0;
}

static const struct iio_buffer_setup_ops apds9960_buffer_setup_ops = {
	.postenable = apds9960_buffer_postenable,
	.predisable = apds9960_buffer_predisable,
};

static int apds9960_regfield_init(struct apds9960_data *data)
{
	struct device *dev = &data->client->dev;
	struct regmap *regmap = data->regmap;

	data->reg_int_als = devm_regmap_field_alloc(dev, regmap,
						apds9960_reg_field_int_als);
	if (IS_ERR(data->reg_int_als)) {
		dev_err(dev, "INT ALS reg field init failed\n");
		return PTR_ERR(data->reg_int_als);
	}

	data->reg_int_ges = devm_regmap_field_alloc(dev, regmap,
						apds9960_reg_field_int_ges);
	if (IS_ERR(data->reg_int_ges)) {
		dev_err(dev, "INT gesture reg field init failed\n");
		return PTR_ERR(data->reg_int_ges);
	}

	data->reg_int_pxs = devm_regmap_field_alloc(dev, regmap,
						apds9960_reg_field_int_pxs);
	if (IS_ERR(data->reg_int_pxs)) {
		dev_err(dev, "INT pxs reg field init failed\n");
		return PTR_ERR(data->reg_int_pxs);
	}

	data->reg_enable_als = devm_regmap_field_alloc(dev, regmap,
						apds9960_reg_field_enable_als);
	if (IS_ERR(data->reg_enable_als)) {
		dev_err(dev, "Enable ALS reg field init failed\n");
		return PTR_ERR(data->reg_enable_als);
	}

	data->reg_enable_ges = devm_regmap_field_alloc(dev, regmap,
						apds9960_reg_field_enable_ges);
	if (IS_ERR(data->reg_enable_ges)) {
		dev_err(dev, "Enable gesture reg field init failed\n");
		return PTR_ERR(data->reg_enable_ges);
	}

	data->reg_enable_pxs = devm_regmap_field_alloc(dev, regmap,
						apds9960_reg_field_enable_pxs);
	if (IS_ERR(data->reg_enable_pxs)) {
		dev_err(dev, "Enable PXS reg field init failed\n");
		return PTR_ERR(data->reg_enable_pxs);
	}

	return 0;
}

/*Writes a single byte to the I2C device and specified register*/
static int wireWriteDataByte(unsigned char reg, unsigned char val)
{
	int ret=0;
    ret = regmap_write(data_p->regmap, reg, val);
    return !ret;
}

/*Enables or disables a feature in the APDS-9960*/
static bool setMode(struct apds9960_data *data, uint8_t mode, uint8_t enable)
{
    unsigned int reg_val;

    /* Read current ENABLE register */
    reg_val = getMode(data);
    if( reg_val == 0xff) {
        return false;
    }
    
    /* Change bit(s) in ENABLE register */
    enable = enable & 0x01;
    if( mode >= 0 && mode <= 6 ) {
        if (enable) {
            reg_val |= (1 << mode);
        } else {
            reg_val &= ~(1 << mode);
        }
    } else if( mode == ALL ) {
        if (enable) {
            reg_val = 0x7F;
        } else {
            reg_val = 0x00;
        }
    }
        
    /* Write value back to ENABLE register */
    if( !wireWriteDataByte(APDS9960_ENABLE, reg_val) ) {
        return false;
    }
        
    return true;
}

/*Sets the LED drive strength for proximity and ambient light sensor (ALS)*/
static bool setLEDDrive(struct apds9960_data *data, uint8_t drive)
{
    unsigned int val;
    
    /* Read value from CONTROL register */  
	regmap_read(data->regmap, APDS9960_CONTROL, &val);
    /* Set bits in register to given value */
    drive &= 0b00000011;
    drive = drive << 6;
    val &= 0b00111111;
    val |= drive;
    
    /* Write register value back into CONTROL register */
    if( !wireWriteDataByte(APDS9960_CONTROL, val) ) {
        return false;
    }
    
    return true;
}

/*Sets the receiver gain for proximity detection*/
static bool setProximityGain(struct apds9960_data *data, uint8_t drive)
{
    unsigned int val;
    
    /* Read value from CONTROL register */  
	regmap_read(data->regmap, APDS9960_CONTROL, &val);
    /* Set bits in register to given value */
    drive &= 0b00000011;
    drive = drive << 2;
    val &= 0b11110011;
    val |= drive;
    
    /* Write register value back into CONTROL register */
    if( !wireWriteDataByte(APDS9960_CONTROL, val) ) {
        return false;
    }
    
    return true;
}

/*Sets the receiver gain for the ambient light sensor (ALS)*/
static bool setAmbientLightGain(struct apds9960_data *data, uint8_t drive)
{
    unsigned int val;
    
    /* Read value from CONTROL register */
	regmap_read(data->regmap, APDS9960_CONTROL, &val);
    /* Set bits in register to given value */
    drive &= 0b00000011;
    val &= 0b11111100;
    val |= drive;
    
    /* Write register value back into CONTROL register */
    if( !wireWriteDataByte(APDS9960_CONTROL, val) ) {
        return false;
    }
    
    return true;
}

/*Sets the lower threshold for proximity detection*/
static bool setProxIntLowThresh(uint8_t threshold)
{
    if( !wireWriteDataByte(APDS9960_PILT, threshold) ) {
        return false;
    }
    
    return true;
}

/*Sets the high threshold for proximity detection*/
static bool setProxIntHighThresh(uint8_t threshold)
{
    if( !wireWriteDataByte(APDS9960_PIHT, threshold) ) {
        return false;
    }
    
    return true;
}

/*Sets the low threshold for ambient light interrupts*/
static bool setLightIntLowThreshold(uint16_t threshold)
{
    uint8_t val_low;
    uint8_t val_high;
    
    /* Break 16-bit threshold into 2 8-bit values */
    val_low = threshold & 0x00FF;
    val_high = (threshold & 0xFF00) >> 8;
    
    /* Write low byte */
    if( !wireWriteDataByte(APDS9960_AILTL, val_low) ) {
        return false;
    }
    
    /* Write high byte */
    if( !wireWriteDataByte(APDS9960_AILTH, val_high) ) {
        return false;
    }
    
    return true;
}

/*Sets the high threshold for ambient light interrupts*/
static bool setLightIntHighThreshold(uint16_t threshold)
{
    uint8_t val_low;
    uint8_t val_high;
    
    /* Break 16-bit threshold into 2 8-bit values */
    val_low = threshold & 0x00FF;
    val_high = (threshold & 0xFF00) >> 8;
    
    /* Write low byte */
    if( !wireWriteDataByte(APDS9960_AIHTL, val_low) ) {
        return false;
    }
    
    /* Write high byte */
    if( !wireWriteDataByte(APDS9960_AIHTH, val_high) ) {
        return false;
    }
    
    return true;
}

/*Sets the entry proximity threshold for gesture sensing*/
static bool setGestureEnterThresh(uint8_t threshold)
{
    if( !wireWriteDataByte(APDS9960_GPENTH, threshold) ) {
        return false;
    }
    
    return true;
}

/*Sets the exit proximity threshold for gesture sensing*/
static bool setGestureExitThresh(uint8_t threshold)
{
    if( !wireWriteDataByte(APDS9960_GEXTH, threshold) ) {
        return false;
    }
    
    return true;
}

/*Sets the gain of the photodiode during gesture mode*/
static bool setGestureGain(struct apds9960_data *data, uint8_t gain)
{
    unsigned int val;
    
    /* Read value from GCONF2 register */
	regmap_read(data->regmap, APDS9960_GCONF2, &val);
    /* Set bits in register to given value */
    gain &= 0b00000011;
    gain = gain << 5;
    val &= 0b10011111;
    val |= gain;
    
    /* Write register value back into GCONF2 register */
    if( !wireWriteDataByte(APDS9960_GCONF2, val) ) {
        return false;
    }
    
    return true;
}

/*Sets the LED drive current during gesture mode*/
static bool setGestureLEDDrive(struct apds9960_data *data, uint8_t drive)
{
    unsigned int val;
    
    /* Read value from GCONF2 register */
	regmap_read(data->regmap, APDS9960_GCONF2, &val);
    /* Set bits in register to given value */
    drive &= 0b00000011;
    drive = drive << 3;
    val &= 0b11100111;
    val |= drive;
    
    /* Write register value back into GCONF2 register */
    if( !wireWriteDataByte(APDS9960_GCONF2, val) ) {
        return false;
    }
    
    return true;
}

/*Sets the time in low power mode between gesture detections*/
static bool setGestureWaitTime(struct apds9960_data *data, uint8_t time)
{
    unsigned int val;
    
    /* Read value from GCONF2 register */
	regmap_read(data->regmap, APDS9960_GCONF2, &val);
    /* Set bits in register to given value */
    time &= 0b00000111;
    val &= 0b11111000;
    val |= time;
    
    /* Write register value back into GCONF2 register */
    if( !wireWriteDataByte(APDS9960_GCONF2, val) ) {
        return false;
    }
    
    return true;
}

/* Turns gesture-related interrupts on or off*/
static bool setGestureIntEnable(struct apds9960_data *data, uint8_t enable)
{
    unsigned int val;
    
    /* Read value from GCONF4 register */
	regmap_read(data->regmap, APDS9960_GCONF4, &val);
    /* Set bits in register to given value */
    enable &= 0b00000001;
    enable = enable << 1;
    val &= 0b11111101;
    val |= enable;
    
    /* Write register value back into GCONF4 register */
    if( !wireWriteDataByte(APDS9960_GCONF4, val) ) {
        return false;
    }
    
    return true;
}

/*Sets the LED current boost value
 *
 * Value  Boost Current
 *   0        100%
 *   1        150%
 *   2        200%
 *   3        300%
 */
static bool setLEDBoost(struct apds9960_data *data,uint8_t boost)
{
    unsigned int val;
    
    /* Read value from CONFIG2 register */
	regmap_read(data->regmap, APDS9960_CONFIG2, &val);
    /* Set bits in register to given value */
    boost &= 0b00000011;
    boost = boost << 4;
    val &= 0b11001111;
    val |= boost;
    
    /* Write register value back into CONFIG2 register */
    if( !wireWriteDataByte(APDS9960_CONFIG2, val) ) {
        return false;
    }
    
    return true;
}    

/*Tells the state machine to either enter or exit gesture state machine*/
static bool setGestureMode(struct apds9960_data *data,uint8_t mode)
{
    unsigned int val;
    
    /* Read value from GCONF4 register */
	regmap_read(data->regmap, APDS9960_GCONF4, &val);
    /* Set bits in register to given value */
    mode &= 0b00000001;
    val &= 0b11111110;
    val |= mode;
    
    /* Write register value back into GCONF4 register */
    if( !wireWriteDataByte(APDS9960_GCONF4, val) ) {
        return false;
    }
    
    return true;
}

/*Turn the APDS-9960 on*/
static bool enablePower(struct apds9960_data *data)
{
    if( !setMode(data,POWER, 1) ) {
        return false;
    }
    
    return true;
}

/**
 * @brief Starts the gesture recognition engine on the APDS-9960
 *
 * @param[in] interrupts true to enable hardware external interrupt on gesture
 * @return True if engine enabled correctly. False on error.
 */
static bool SparkFun_APDS9960_enableGestureSensor(struct apds9960_data *data)
{
    /* Enable gesture mode
       Set ENABLE to 0 (power off)
       //Set WTIME to 0xFF
       Set AUX to LED_BOOST_300
       Enable PON, WEN, PEN, GEN in ENABLE 
    */
    resetGestureParameters();
    if( !wireWriteDataByte(APDS9960_WTIME, 0xFF) ) {
        return false;
    }
    if( !wireWriteDataByte(APDS9960_PPULSE, DEFAULT_GESTURE_PPULSE) ) {
        return false;
    }
    if( !setLEDBoost(data,LED_BOOST_300) ) {
        return false;
    }
    if( !setGestureIntEnable(data,1) ) {
        return false;
    }
    if( !setGestureMode(data,1) ) {
        return false;
    }
    if( !enablePower(data) ){
        return false;
    }
    if( !setMode(data,AMBIENT_LIGHT, 1) ) {//WAIT
        return false;
    }
    if( !setMode(data,PROXIMITY, 1) ) {
        return false;
    }
    if( !setMode(data,GESTURE, 1) ) {
        return false;
    }
    return true;
}

/**
 * @brief Configures I2C communications and initializes registers to defaults
 *
 * @return True if initialized successfully. False otherwise.
 */
static bool APDS9960_init(struct apds9960_data *data)
{
    unsigned int id;
	
    /* Read ID register and check against known values for APDS-9960 */
    regmap_read(data->regmap, APDS9960_ID, &id);

    printk("apds9960 id=0x%x\n",id);   
    if( !(id == APDS9960_ID_1 ||id == APDS9960_ID_2 ) ) 
    {    
       printk("APDS9960_ID error\n");   
       return 0;
    }
	
    /* Set ENABLE register to 0 (disable all features) */
    if( !setMode(data,ALL, OFF) ) {
        return false;
    }
    
    /* Set default values for ambient light and proximity registers */
    if( !wireWriteDataByte(APDS9960_ATIME, DEFAULT_ATIME) ) {
        return false;
    }
    if( !wireWriteDataByte(APDS9960_WTIME, DEFAULT_WTIME) ) {
        return false;
    }
    if( !wireWriteDataByte(APDS9960_PPULSE, DEFAULT_PROX_PPULSE) ) {
        return false;
    }
    if( !wireWriteDataByte(APDS9960_POFFSET_UR, DEFAULT_POFFSET_UR) ) {
        return false;
    }
    if( !wireWriteDataByte(APDS9960_POFFSET_DL, DEFAULT_POFFSET_DL) ) {
        return false;
    }
    if( !wireWriteDataByte(APDS9960_CONFIG1, DEFAULT_CONFIG1) ) {
        return false;
    }
    if( !setLEDDrive(data,DEFAULT_LDRIVE) ) {
        return false;
    }
    if( !setProximityGain(data,DEFAULT_PGAIN) ) {
        return false;
    }
    if( !setAmbientLightGain(data,DEFAULT_AGAIN) ) {
        return false;
    }
    if( !setProxIntLowThresh(DEFAULT_PILT) ) {
        return false;
    }
    if( !setProxIntHighThresh(DEFAULT_PIHT) ) {
        return false;
    }
    if( !setLightIntLowThreshold(DEFAULT_AILT) ) {
        return false;
    }
    if( !setLightIntHighThreshold(DEFAULT_AIHT) ) {
        return false;
    }
    if( !wireWriteDataByte(APDS9960_PERS, DEFAULT_PERS) ) {
        return false;
    }
    if( !wireWriteDataByte(APDS9960_CONFIG2, DEFAULT_CONFIG2) ) {
        return false;
    }
    if( !wireWriteDataByte(APDS9960_CONFIG3, DEFAULT_CONFIG3) ) {
        return false;
    }
    /* Set default values for gesture sense registers */
    if( !setGestureEnterThresh(DEFAULT_GPENTH) ) {
        return false;
    }
    if( !setGestureExitThresh(DEFAULT_GEXTH) ) {
        return false;
    }
    if( !wireWriteDataByte(APDS9960_GCONF1, DEFAULT_GCONF1) ) {
        return false;
    }
    if( !setGestureGain(data,DEFAULT_GGAIN) ) {
        return false;
    }
    if( !setGestureLEDDrive(data,DEFAULT_GLDRIVE) ) {
        return false;
    }
    if( !setGestureWaitTime(data,DEFAULT_GWTIME) ) {
        return false;
    }
    if( !wireWriteDataByte(APDS9960_GOFFSET_U, DEFAULT_GOFFSET) ) {
        return false;
    }
    if( !wireWriteDataByte(APDS9960_GOFFSET_D, DEFAULT_GOFFSET) ) {
        return false;
    }
    if( !wireWriteDataByte(APDS9960_GOFFSET_L, DEFAULT_GOFFSET) ) {
        return false;
    }
    if( !wireWriteDataByte(APDS9960_GOFFSET_R, DEFAULT_GOFFSET) ) {
        return false;
    }
    if( !wireWriteDataByte(APDS9960_GPULSE, DEFAULT_GPULSE) ) {
        return false;
    }
    if( !wireWriteDataByte(APDS9960_GCONF3, DEFAULT_GCONF3) ) {
        return false;
    }
    if( !setGestureIntEnable(data,DEFAULT_GIEN) ) {
        return false;
    }
    return SparkFun_APDS9960_enableGestureSensor(data);
}

static int apds9960_chip_init(struct apds9960_data *data)
{
	int ret;

	/* Default IT for ALS of 28 ms */
	ret = apds9960_set_it_time(data, 28000);
	if (ret)
		return ret;
	/* Ensure gesture interrupt is OFF */
	ret = regmap_field_write(data->reg_int_ges, 0);
	if (ret)
		return ret;
	/* Disable gesture sensor, since polling is useless from user-space */
	ret = regmap_field_write(data->reg_enable_ges, 0);
	if (ret)
		return ret;
	/* Ensure proximity interrupt is OFF */
	ret = regmap_field_write(data->reg_int_pxs, 0);
	if (ret)
		return ret;
	/* Enable proximity sensor for polling */
	ret = regmap_field_write(data->reg_enable_pxs, 1);
	if (ret)
		return ret;
	/* Ensure ALS interrupt is OFF */
	ret = regmap_field_write(data->reg_int_als, 0);
	if (ret)
		return ret;
	/* Enable ALS sensor for polling */
	ret = regmap_field_write(data->reg_enable_als, 1);
	if (ret)
		return ret;
	/*
	 * When enabled trigger an interrupt after 3 readings
	 * outside threshold for ALS + PXS
	 */
	ret = regmap_write(data->regmap, APDS9960_REG_PERS,
			   APDS9960_DEFAULT_PERS);
	if (ret)
		return ret;
	
	/*
	 * Wait for 4 event outside gesture threshold to prevent interrupt
	 * flooding.
	 */
	ret = regmap_update_bits(data->regmap, APDS9960_REG_GCONF_1,
			APDS9960_REG_GCONF_1_GFIFO_THRES_MASK,
			BIT(0) << APDS9960_REG_GCONF_1_GFIFO_THRES_MASK_SHIFT);
	if (ret)
		return ret;

	/* Default ENTER and EXIT thresholds for the GESTURE engine. */
	ret = regmap_write(data->regmap, APDS9960_REG_GPENTH,
			   APDS9960_DEFAULT_GPENTH);
	if (ret)
		return ret;

	ret = regmap_write(data->regmap, APDS9960_REG_GEXTH,
			   APDS9960_DEFAULT_GEXTH);
	if (ret)
		return ret;

	return apds9960_set_powermode(data, 1);
}

static ssize_t show_apds9960_mode(struct class *cls, struct class_attribute *attr, char *buf)
{
#if 1
		unsigned int i, val;

		for(i=0x80;i<256;i++){
			regmap_read(data_p->regmap, i, &val);
			printk("apds9960 0x%x=0x%x\n",i,val);  
		}
#endif

	return sprintf(buf, "%d\n", khadas_gesture_test_mode);
}

static ssize_t store_apds9960_mode(struct class *cls, struct class_attribute *attr,
		        const char *buf, size_t count)
{
	if (kstrtoint(buf, 0, &khadas_gesture_test_mode))
		return -EINVAL;

	printk("khadas_gesture_test_mode=%d\n",khadas_gesture_test_mode);
	if(khadas_gesture_test_mode){
		 if (KHADAS_EDGEV == board_type)
			gpiod_set_value_cansleep(data_p->led_r_gpio, 0);//GPIO4_D0 LED_R
		 APDS9960_init(data_p);
	}		
	else{
		setMode(data_p,ALL, OFF);
		apds9960_regfield_init(data_p);
		apds9960_chip_init(data_p);
		if (KHADAS_EDGEV == board_type)
			gpiod_set_value_cansleep(data_p->led_r_gpio, 1);//GPIO4_D0 LED_R
	}	
	return count;
}

static struct class_attribute apds9960_class_attrs[] = {
	__ATTR(apds9960_mode, 0644, show_apds9960_mode, store_apds9960_mode),
};

static void create_apds9960_attrs(void)
{
	int i;
	struct class *apds9960_class;
	
	apds9960_class = class_create(THIS_MODULE, "apds9960");
	if (IS_ERR(apds9960_class)) {
		pr_err("create apds9960_class debug class fail\n");
		return;
	}
	for (i = 0; i < ARRAY_SIZE(apds9960_class_attrs); i++) {
		if (class_create_file(apds9960_class, &apds9960_class_attrs[i]))
			pr_err("create apds9960 attribute %s fail\n", apds9960_class_attrs[i].attr.name);
	}
}

static int apds9960_probe(struct i2c_client *client,
			  const struct i2c_device_id *id)
{
	struct apds9960_data *data;
	struct iio_buffer *buffer;
	struct iio_dev *indio_dev;
	int ret;
	enum of_gpio_flags flags;
	struct device *dev = &client->dev;
	struct device_node *node = dev->of_node;
    //unsigned int i, val;

	board_type = get_board_type();
	if (board_type == KHADAS_EDGE)
		return -1;

	printk("%s ...\n", __func__);

	indio_dev = devm_iio_device_alloc(&client->dev, sizeof(*data));
	if (!indio_dev)
		return -ENOMEM;

	buffer = devm_iio_kfifo_allocate(&client->dev);
	if (!buffer)
		return -ENOMEM;

	iio_device_attach_buffer(indio_dev, buffer);

	indio_dev->dev.parent = &client->dev;
	indio_dev->info = &apds9960_info;
	indio_dev->name = APDS9960_DRV_NAME;
	indio_dev->channels = apds9960_channels;
	indio_dev->num_channels = ARRAY_SIZE(apds9960_channels);
	indio_dev->available_scan_masks = apds9960_scan_masks;
	indio_dev->modes = (INDIO_BUFFER_SOFTWARE | INDIO_DIRECT_MODE);
	indio_dev->setup_ops = &apds9960_buffer_setup_ops;

	data = iio_priv(indio_dev);
	i2c_set_clientdata(client, indio_dev);

	data->regmap = devm_regmap_init_i2c(client, &apds9960_regmap_config);
	if (IS_ERR(data->regmap)) {
		dev_err(&client->dev, "regmap initialization failed.\n");
		return PTR_ERR(data->regmap);
	}

	data->client = client;
	data->indio_dev = indio_dev;
	mutex_init(&data->lock);

	ret = pm_runtime_set_active(&client->dev);
	if (ret)
		goto error_power_down;

	pm_runtime_enable(&client->dev);
	pm_runtime_set_autosuspend_delay(&client->dev, 5000);
	pm_runtime_use_autosuspend(&client->dev);

	data_p = data;
	if (KHADAS_EDGEV == board_type){
		data->led_r_gpio_pin = of_get_named_gpio_flags(node, "led-r-gpio", 0, &flags);
		data->led_r_gpio = gpio_to_desc(data->led_r_gpio_pin);
		if (IS_ERR(data->led_r_gpio)) {
			printk("Failed to request GPIO led_r_gpio\n");
		}
		if(gpio_request(data->led_r_gpio_pin, "led-r-gpio")){
			printk("Failed to request GPIO led_r_gpio\n");
			gpio_free(data->led_r_gpio_pin);
		}
		gpio_direction_output(data->led_r_gpio_pin, 1);
		//printk("led_r_gpio = %d \n",gpiod_get_value_cansleep(data->led_r_gpio));
	}
	apds9960_set_power_state(data, true);
	
	if(khadas_gesture_test_mode){
		ret = APDS9960_init(data);
		if (!ret)
			goto error_power_down;
	}		
	else{
		ret = apds9960_regfield_init(data);
		if (ret)
			goto error_power_down;

		ret = apds9960_chip_init(data);
		if (ret)
			goto error_power_down;
	}	

#if 0
	for(i=0x80;i<256;i++){
		regmap_read(data->regmap, i, &val);
		printk("apds9960 0x%x=0x%x\n",i,val);  
	}
#endif

	if (client->irq <= 0) {
		dev_err(&client->dev, "no valid irq defined\n");
		ret = -EINVAL;
		goto error_power_down;
	}
	ret = devm_request_threaded_irq(&client->dev, client->irq,
					NULL, apds9960_interrupt_handler,
					IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
					"apds9960_event",
					indio_dev);
	if (ret) {
		dev_err(&client->dev, "request irq (%d) failed\n", client->irq);
		goto error_power_down;
	}

	ret = iio_device_register(indio_dev);
	if (ret)
		goto error_power_down;

	apds9960_set_power_state(data, true);
	
	create_apds9960_attrs();
	
	printk("%s done!\n", __func__);

	return 0;

error_power_down:
	apds9960_set_power_state(data, false);

	return ret;
}

static int apds9960_remove(struct i2c_client *client)
{
	struct iio_dev *indio_dev = i2c_get_clientdata(client);
	struct apds9960_data *data = iio_priv(indio_dev);

	iio_device_unregister(indio_dev);
	pm_runtime_disable(&client->dev);
	pm_runtime_set_suspended(&client->dev);
	apds9960_set_powermode(data, 0);

	return 0;
}

#ifdef CONFIG_PM
static int apds9960_runtime_suspend(struct device *dev)
{
	struct apds9960_data *data =
			iio_priv(i2c_get_clientdata(to_i2c_client(dev)));

	return apds9960_set_powermode(data, 0);
}

static int apds9960_runtime_resume(struct device *dev)
{
	struct apds9960_data *data =
			iio_priv(i2c_get_clientdata(to_i2c_client(dev)));

	return apds9960_set_powermode(data, 1);
}
#endif

static const struct dev_pm_ops apds9960_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(pm_runtime_force_suspend,
				pm_runtime_force_resume)
	SET_RUNTIME_PM_OPS(apds9960_runtime_suspend,
			   apds9960_runtime_resume, NULL)
};

static const struct i2c_device_id apds9960_id[] = {
	{ "apds9960", 0 },
	{}
};
MODULE_DEVICE_TABLE(i2c, apds9960_id);

static const struct of_device_id apds9960_of_match[] = {
	{ .compatible = "avago,apds9960" },
	{ }
};
MODULE_DEVICE_TABLE(of, apds9960_of_match);

static struct i2c_driver apds9960_driver = {
	.driver = {
		.name	= APDS9960_DRV_NAME,
		.of_match_table = apds9960_of_match,
		.pm	= &apds9960_pm_ops,
	},
	.probe		= apds9960_probe,
	.remove		= apds9960_remove,
	.id_table	= apds9960_id,
};
module_i2c_driver(apds9960_driver);

MODULE_AUTHOR("Matt Ranostay <matt.ranostay@konsulko.com>,goenjoy@khadas.com");
MODULE_DESCRIPTION("ADPS9960 Gesture/RGB/ALS/Proximity sensor");
MODULE_LICENSE("GPL");
