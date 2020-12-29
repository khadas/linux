/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __AML_DEMOD_COMMON_H__
#define __AML_DEMOD_COMMON_H__

#include <linux/i2c.h>
#include <uapi/linux/videodev2.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/of_gpio.h>
#include <linux/dvb/frontend.h>

enum tuner_type {
	AM_TUNER_NONE = 0,
	AM_TUNER_SI2176 = 1,
	AM_TUNER_SI2196 = 2,
	AM_TUNER_FQ1216 = 3,
	AM_TUNER_HTM = 4,
	AM_TUNER_CTC703 = 5,
	AM_TUNER_SI2177 = 6,
	AM_TUNER_R840 = 7,
	AM_TUNER_SI2157 = 8,
	AM_TUNER_SI2151 = 9,
	AM_TUNER_MXL661 = 10,
	AM_TUNER_MXL608 = 11,
	AM_TUNER_SI2159 = 12,
	AM_TUNER_R842 = 13,
	AM_TUNER_ATBM2040 = 14,
	AM_TUNER_ATBM253 = 15,
	AM_TUNER_SI2124 = 16,
	AM_TUNER_AV2011 = 17,
	AM_TUNER_AV2012 = 18,
	AM_TUNER_AV2018 = 19,
	AM_TUNER_R836 = 20,
	AM_TUNER_R848 = 21,
	AM_TUNER_MXL603 = 22,
	AM_TUNER_RT710 = 23
};

enum atv_demod_type {
	AM_ATV_DEMOD_NONE   = 0,
	AM_ATV_DEMOD_SI2176 = 1,
	AM_ATV_DEMOD_SI2196 = 2,
	AM_ATV_DEMOD_FQ1216 = 3,
	AM_ATV_DEMOD_HTM    = 4,
	AM_ATV_DEMOD_CTC703 = 5,
	AM_ATV_DEMOD_SI2177 = 6,
	AM_ATV_DEMOD_AMLATV = 7
};

enum dtv_demod_type {
	AM_DTV_DEMOD_NONE     = 0,
	AM_DTV_DEMOD_AMLDTV   = 1,
	AM_DTV_DEMOD_M1       = 2,
	AM_DTV_DEMOD_MXL101   = 3,
	AM_DTV_DEMOD_AVL6211  = 4,
	AM_DTV_DEMOD_SI2168   = 5,
	AM_DTV_DEMOD_ITE9133  = 6,
	AM_DTV_DEMOD_ITE9173  = 7,
	AM_DTV_DEMOD_DIB8096  = 8,
	AM_DTV_DEMOD_ATBM8869 = 9,
	AM_DTV_DEMOD_MXL241   = 10,
	AM_DTV_DEMOD_AVL68xx  = 11,
	AM_DTV_DEMOD_MXL683   = 12,
	AM_DTV_DEMOD_ATBM8881 = 13,
	AM_DTV_DEMOD_ATBM7821 = 14,
	AM_DTV_DEMOD_AVL6762  = 15,
	AM_DTV_DEMOD_CXD2856  = 16,
	AM_DTV_DEMOD_MXL248   = 17,
	AM_DTV_DEMOD_M88DM6K  = 18,
	AM_DTV_DEMOD_MN88436  = 19,
	AM_DTV_DEMOD_MXL212C  = 20,
	AM_DTV_DEMOD_MXL213C  = 21,
	AM_DTV_DEMOD_MXL214C  = 22,
	AM_DTV_DEMOD_MXL252C  = 23,
	AM_DTV_DEMOD_MXL254C  = 24,
	AM_DTV_DEMOD_MXL256C  = 25,
	AM_DTV_DEMOD_MXL258C  = 26,
	AM_DTV_DEMOD_SI2169   = 27,
	AM_DTV_DEMOD_AVL6221C = 28
};

enum aml_fe_dev_type {
	AM_DEV_TUNER,
	AM_DEV_ATV_DEMOD,
	AM_DEV_DTV_DEMOD
};

/* For configure gpio */
struct gpio_config {
	s32 pin; /* gpio pin */
	u8 dir; /* direction, 0: out, 1: in. */
	u32 value; /* input or output active value */
	u32 reserved0;
	u32 reserved1;
};

/* For configure different tuners */
/* It can add fields as extensions */
struct tuner_config {
	char *name;
	u32 code; /* tuner chip code */
	u8 id; /* enum tuner type */
	u8 i2c_addr;
	u8 i2c_id;
	struct i2c_adapter *i2c_adap;
	u8 xtal; /* 0: 16MHz, 1: 24MHz, 3: 27MHz */
	u32 xtal_cap; /* load capacitor */
	u8 xtal_mode;
	u8 lt_out; /* loop through out, 0: off, 1: on. */
	u8 dual_power; /* 0: 3.3v, 1: 1.8v and 3.3v. */
	u8 if_agc; /* 0: self, 1: external. */
	u8 if_hz; /* output intermediate frequency. */
	u8 if_invert; /* spectrum, 0: normal, 1: inverted. */
	u8 if_amp; /* output intermediate frequency amplitude. */

	u8 detect; /* whether detect is required, 0 - off, 1 - on. */

	/* tuner reset gpio, it can be implemented by RC circuit. */
	struct gpio_config reset;
	struct gpio_config power;

	u32 reserved0;
	u32 reserved1;
};

/* For configure different demod */
struct demod_config {
	char *name;
	u32 code; /* demod chip code. */
	u8 id; /* enum demod type. */
	u8 mode; /* 0: internal, 1: external. */
	u8 xtal; /* demod xtal. */
	u8 ts; /* demux ts in port. */
	u8 ts_out_mode; /* serial or parallel; 0: serial, 1: parallel. */
	u8 ts_data_pin; /* serial output pin of TS data. */
	u8 ts_wire_mode; /* serial ts wire mode, 0: four-wire, 1: three-wire. */

	/* serial ts multiplexer mode,
	 * 0: NO_MUX_4, 1: MUX_1, 2: MUX_2, 3: MUX_3, 4: MUX_4, 5: MUX_2_B, 6: NO_MUX_2.
	 */
	u8 ts_mux_mode;

	u8 ts_out_order; /* parallel/serial output bit order; 0: LSB, 1: MSB. */
	u8 ts_out_bits; /* parallel output bits */
	u8 ts_sync_width; /* 0: bit, 1: byte. */

	/* for mixed ts packet stream. */
	u8 ts_packet_mode; /* 0: add header, 1: remapping pid. */
	u8 ts_header_bytes; /* add header to every packet, 4/8/12 bytes. */
	u8 ts_header_data[12];
	u8 ts_remap_cnt; /* remapping pid. */
	u16 *ts_pid;
	u16 *ts_remap_pid;

	/* ts output clock, 0: self-adaption, clock frequency
	 * is set according to the output bit rate.
	 */
	u32 ts_clk;

	/* ts clock polarity.
	 * 0: Falling / Negative edge.
	 * 1: Rising / Positive edge.
	 */
	u8 ts_clk_pol;

	u8 detect; /* whether detect is required, 0 - off, 1 - on. */

	u8 i2c_addr;
	u8 i2c_id;
	struct i2c_adapter *i2c_adap;

	struct tuner_config tuner0; /* TC */
	struct tuner_config tuner1; /* S */

	struct gpio_config reset;
	struct gpio_config ant_power;
	struct gpio_config lnb_en;
	struct gpio_config lnb_sel;
	struct gpio_config other;

	u8 reserved0;
	u8 reserved1;
};

/* For configure multi-tuner */
struct aml_tuner {
	struct tuner_config cfg;
	unsigned int i2c_adapter_id;
};

/** generic AML DVB attach function. */
#if (defined CONFIG_AMLOGIC_DVB_EXTERN ||\
		defined CONFIG_AMLOGIC_DVB_EXTERN_MODULE)
#define aml_dvb_attach(FUNCTION, ARGS...) ({ \
	void *__r = NULL; \
	typeof(&FUNCTION) __a = symbol_request(FUNCTION); \
	if (__a) { \
		__r = (void *) __a(ARGS); \
		if (__r == NULL) \
			symbol_put(FUNCTION); \
	} else { \
		printk(KERN_ERR "AML DVB: Unable to find symbol " \
			#FUNCTION"()\n"); \
	} \
	__r; \
})

#define aml_dvb_detach(FUNC) symbol_put_addr(FUNC)

#define AML_DEMOD_ATTACH_FUNCTION(name) \
static inline struct dvb_frontend *name##_attach(\
		const struct demod_config *cfg)\
{\
	return NULL;\
}

#define AML_TUNER_ATTACH_FUNCTION(name) \
static inline struct dvb_frontend *name##_attach(struct dvb_frontend *fe,\
		const struct tuner_config *cfg)\
{\
	return NULL;\
}

#else
#define aml_dvb_attach(FUNCTION, ARGS...) ({ \
	FUNCTION(ARGS); \
})

#define aml_dvb_detach(FUNC) {}

#define AML_DEMOD_ATTACH_FUNCTION(name)

#define AML_TUNER_ATTACH_FUNCTION(name)
#endif

/*COLOR MODULATION TYPE*/
#define V4L2_COLOR_STD_PAL    ((v4l2_std_id) 0x04000000)
#define V4L2_COLOR_STD_NTSC   ((v4l2_std_id) 0x08000000)
#define V4L2_COLOR_STD_SECAM  ((v4l2_std_id) 0x10000000)

#define AML_FE_UNDEFINED (0xFF)
#define AML_MAX_DELSYS   (32)
#define AML_MAX_FE       (32)

#if (defined CONFIG_AMLOGIC_DVB_EXTERN ||\
		defined CONFIG_AMLOGIC_DVB_EXTERN_MODULE)
const char *v4l2_std_to_str(v4l2_std_id std);

void aml_ktime_get_ts(struct timespec *ts);

int aml_gpio_direction_output(int gpio, int value);
int aml_gpio_direction_input(int gpio);
bool aml_gpio_is_valid(int number);
int aml_gpio_get_value(int gpio);
void aml_gpio_set_value(int gpio, int value);
void aml_gpio_free(int gpio);
int aml_gpio_request(int gpio, const char *label);
int aml_demod_gpio_set(int gpio, int dir, int value, const char *label);
int aml_demod_gpio_config(struct gpio_config *cfg, const char *label);

struct class *aml_class_create(struct module *owner, const char *name);
void aml_class_destroy(struct class *cls);
int aml_class_create_file(struct class *class,
		const struct class_attribute *attr);
int aml_class_register(struct class *class);
void aml_class_unregister(struct class *class);

int aml_platform_driver_register(struct platform_driver *drv);
void aml_platform_driver_unregister(struct platform_driver *drv);
int aml_platform_device_register(struct platform_device *pdev);
void aml_platform_device_unregister(struct platform_device *pdev);
#else
static inline __maybe_unused const char *v4l2_std_to_str(v4l2_std_id std)
{
	return NULL;
}

static inline __maybe_unused void aml_ktime_get_ts(struct timespec *ts)
{

}

static inline __maybe_unused int aml_gpio_direction_output(int gpio, int value)
{
	return 0;
}

static inline __maybe_unused int aml_gpio_direction_input(int gpio)
{
	return 0;
}

static inline __maybe_unused bool aml_gpio_is_valid(int number)
{
	return false;
}

static inline __maybe_unused int aml_gpio_get_value(int gpio)
{
	return 0;
}

static inline __maybe_unused void aml_gpio_set_value(int gpio, int value)
{
}

static inline __maybe_unused void aml_gpio_free(int gpio)
{
}

static inline __maybe_unused int aml_gpio_request(int gpio, const char *label)
{
	return 0;
}

static inline __maybe_unused int aml_demod_gpio_set(int gpio, int dir,
		int value, const char *label)
{
	return 0;
}

static inline __maybe_unused int aml_demod_gpio_config(struct gpio_config *cfg,
		const char *label)
{
	return 0;
}

static inline __maybe_unused struct class *aml_class_create(
		struct module *owner, const char *name)
{
	return NULL;
}

static inline __maybe_unused void aml_class_destroy(struct class *cls)
{
}

static inline __maybe_unused int aml_class_create_file(struct class *class,
		const struct class_attribute *attr)
{
	return 0;
}

static inline __maybe_unused int aml_class_register(struct class *class)
{
	return 0;
}

static inline __maybe_unused void aml_class_unregister(struct class *class)
{
}

static inline __maybe_unused int aml_platform_driver_register(
		struct platform_driver *drv)
{
	return 0;
}

static inline __maybe_unused void aml_platform_driver_unregister(
		struct platform_driver *drv)
{
}

static inline __maybe_unused int aml_platform_device_register(
		struct platform_device *pdev)
{
	return 0;
}

static inline __maybe_unused void aml_platform_device_unregister(
		struct platform_device *pdev)
{
}

#endif

#endif /* __AML_DEMOD_COMMON_H__ */
