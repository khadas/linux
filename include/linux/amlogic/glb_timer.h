/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __AML_GLB_TIMER_H__
#define __AML_GLB_TIMER_H__
#include <linux/types.h>

/* used for mipi0~4 */
#define GLBT_MIPI0_SOF		0
#define GLBT_MIPI0_EOF		1
#define GLBT_MIPI1_SOF		2
#define GLBT_MIPI1_EOF		3
#define GLBT_MIPI2_SOF		4
#define GLBT_MIPI2_EOF		5
#define GLBT_MIPI3_SOF		6
#define GLBT_MIPI3_EOF		7
#define GLBT_MIPI4_SOF		8
#define GLBT_MIPI4_EOF		9

/* used for 32 gpio irq lines and need to set pinmux to rt_gpiox*/
#define GLBT_GPIO0_IRQ		10
#define GLBT_GPIO1_IRQ		11
#define GLBT_GPIO2_IRQ		12
#define GLBT_GPIO3_IRQ		13
#define GLBT_GPIO4_IRQ		14
#define GLBT_GPIO5_IRQ		15
#define GLBT_GPIO6_IRQ		16
#define GLBT_GPIO7_IRQ		17
#define GLBT_GPIO8_IRQ		18
#define GLBT_GPIO9_IRQ		19
#define GLBT_GPIO10_IRQ		20
#define GLBT_GPIO11_IRQ		21
#define GLBT_GPIO12_IRQ		22
#define GLBT_GPIO13_IRQ		23
#define GLBT_GPIO14_IRQ		24
#define GLBT_GPIO15_IRQ		25
#define GLBT_GPIO16_IRQ		26
#define GLBT_GPIO17_IRQ		27
#define GLBT_GPIO18_IRQ		28
#define GLBT_GPIO19_IRQ		29
#define GLBT_GPIO20_IRQ		30
#define GLBT_GPIO21_IRQ		31
#define GLBT_GPIO22_IRQ		32
#define GLBT_GPIO23_IRQ		33
#define GLBT_GPIO24_IRQ		34
#define GLBT_GPIO25_IRQ		35
#define GLBT_GPIO26_IRQ		36
#define GLBT_GPIO27_IRQ		37
#define GLBT_GPIO28_IRQ		38
#define GLBT_GPIO29_IRQ		39
#define GLBT_GPIO30_IRQ		40
#define GLBT_GPIO31_IRQ		41

#define SRC_OFFSET(REG, SRCN)	((REG) + (((SRCN) * 0x08) << 2))

struct meson_glb_timer_output_dev;

struct global_timer_output_gpio {
	const char *name;
	struct device *owner_dev;
	struct meson_glb_timer_output_dev *glb_output_dev;
};

/* TRIG_TYPE_** | TRIG_ONE_SHOT */
enum meson_glb_srcsel_flag {
	TRIG_TYPE_SW_SET		= 0,
	TRIG_TYPE_RISING		= 1,
	TRIG_TYPE_FALLING		= 2,
	TRIG_TYPE_BOTH			= 3,
	TRIG_ONE_SHOT			= 4,
};

enum meson_glb_srcsel_reg {
	TRIG_SRC0_CTRL0			= 0x00 << 2,
	TRIG_SRC0_TS_L			= 0x01 << 2,
	TRIG_SRC0_TS_H			= 0x02 << 2,
};

enum meson_glb_outctl_reg {
	OUTPUT_CTRL0			= 0x00 << 2,
	OUTPUT_PULSE_WDITH		= 0x01 << 2,
	OUTPUT_INTERVAL			= 0x02 << 2,
	OUTPUT_STOP_NUM			= 0x03 << 2,
	OUTPUT_EXPIRES_TS_L		= 0x04 << 2,
	OUTPUT_EXPIRES_TS_H		= 0x05 << 2,
	OUTPUT_ST0			= 0x06 << 2,
	OUTPUT_ST1			= 0x07 << 2,
};

#ifdef CONFIG_AMLOGIC_GLOBAL_TIMER
int glb_timer_mipi_config(u8 srcn, unsigned int trig);
unsigned long long glb_timer_get_counter(u8 srcn);

u64 meson_global_timer_global_snapshot(void);
int meson_global_timer_reset(void);

u64 ns_to_global_timer_count(u64 time_in_ns);
u64 global_timer_count_to_ns(u64 time_in_global_timer_ticks);

int meson_global_timer_isp_event_snapshot_configure(u8 isp_event_src,
				enum meson_glb_srcsel_flag trigger_type);
u64 meson_global_timer_isp_snapshot(u8 isp_event_src);

u64 meson_global_timer_input_gpio_get_snapshot(int id);
int meson_global_timer_input_gpio_get_source_index(int virq);
int meson_global_timer_input_gpio_configure(u8 id,
				enum meson_glb_srcsel_flag trigger_type);
unsigned int gpio_irq_get_channel_idx(int irq);

struct global_timer_output_gpio*
global_timer_output_gpio_get_from_index(struct device *dev, int index);
int global_timer_output_start(struct global_timer_output_gpio *gtod,
			      u64 expires);
int global_timer_output_gpio_setup(struct global_timer_output_gpio *gtod,
				   bool oneshot, u64 pulse_width, u64 interval,
				   u8 init_val);
int global_timer_output_stop(struct global_timer_output_gpio *gtod);
#else
unsigned int gpio_irq_get_channel_idx(int irq);
int glb_timer_mipi_config(u8 srcn, unsigned int trig)
{
	return 0;
}

unsigned long long glb_timer_get_counter(u8 srcn)
{
	return 0;
}

u64 meson_global_timer_global_snapshot(void)
{
	return 0;
}

int meson_global_timer_reset(void)
{
	return 0;
}

u64 ns_to_global_timer_count(u64 time_in_ns)
{
	return 0;
}

u64 global_timer_count_to_ns(u64 time_in_global_timer_ticks)
{
	return 0;
}

int meson_global_timer_isp_event_snapshot_configure(u8 isp_event_src,
				enum meson_glb_srcsel_flag trigger_type)
{
	return 0;
}

u64 meson_global_timer_isp_snapshot(u8 isp_event_src)
{
	return 0;
}

int meson_global_timer_input_gpio_get_source_index(int virq)
{
	return 0;
}

u64 meson_global_timer_input_gpio_get_snapshot(int id)
{
	return 0;
}

int meson_global_timer_input_gpio_configure(u8 id,
				enum meson_glb_srcsel_flag trigger_type)
{
	return 0;
}

struct global_timer_output_gpio*
global_timer_output_gpio_get_from_index(struct device *dev, int index)
{
	return NULL;
}

int global_timer_output_start(struct global_timer_output_gpio *gtod,
			      u64 expires)
{
	return 0;
}

int global_timer_output_gpio_setup(struct global_timer_output_gpio *gtod,
				   bool oneshot, u64 pulse_width, u64 interval,
				   u8 init_val)
{
	return 0;
}

int global_timer_output_stop(struct global_timer_output_gpio *gtod)
{
	return 0;
}
#endif

#endif //__AML_GLB_TIMER_H__
