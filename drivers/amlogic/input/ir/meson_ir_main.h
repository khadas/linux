/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __IR_MAIN_MESON_H__
#define __IR_MAIN_MESON_H__
#include <linux/cdev.h>
#include <linux/regmap.h>
#include "meson_ir_common.h"
#include "meson_ir_core.h"

#define DRIVER_NAME "meson-ir"

#define IR_DATA_IS_VALID(data) ((data) & 0x8)
#define IR_CONTROLLER_BUSY(x) (((x) >> 7) & 0x1)

#define CURSOR_MOVE_ACCELERATE {0, 2, 2, 4, 4, 6, 8, 10, 12, 14, 16, 18}
#define DEBUG_BUFFER_SIZE  4096
#define DECIDE_VENDOR_CHIP_ID (chip->vendor && chip->product && chip->version)
#define DECIDE_VENDOR_TA_ID (ct->tab.vendor && ct->tab.product && ct->tab.version)

enum IR_CONTR_NUMBER {
	MULTI_IR_ID = 0,
	LEGACY_IR_ID,
	IR_ID_MAX
};

enum IR_WORK_MODE {
	NORMAL_MODE = 0,
	MOUSE_MODE = 1
};

struct meson_ir_reg_map {
	unsigned int reg;
	unsigned int val;
};

/*
 *struct meson_ir_map_tab_list
 *
 *@ir_dev_mode: 0: normal mode; 1: mouse mode
 *@list:
 *@tab:
 */
struct meson_ir_map_tab_list {
	u8 ir_dev_mode;
	struct list_head list;
	struct ir_map_tab tab;
};

struct cdev;
struct meson_ir_chip;

/**
 *struct key_number - to save the number of key for map table
 *
 *@update_flag: to ensure get key number before map table
 *@value:
 */
struct key_number {
	u8 update_flag;
	int  value;
};

/**
 *struct meson_ir_contr_desc - describe the different properties and methods
 *for the Legacy IR controller and multi-format IR controller.
 *TODO: compatible with the "struct meson_ir_reg_proto"
 */
struct meson_ir_contr_desc {
	void __iomem *remote_regs;
	struct regmap *base;
	char *proto_name;
	int (*get_scancode)(struct meson_ir_chip *chip);
	int (*get_decode_status)(struct meson_ir_chip *chip);
	u32 (*get_custom_code)(struct meson_ir_chip *chip);
	bool (*set_custom_code)(struct meson_ir_chip *chip, u32 code);
};

/**
 *struct meson_ir_chip - describe the common properties and methods
 * for the Legacy IR controller and multi-format IR controller.
 */
#define RX_BUFFER_MAX_SIZE 8
#define CUSTOM_TABLES_SIZE 20
#define CUSTOM_NUM_MAX 20
struct meson_ir_chip {
	struct device *dev;
	struct meson_ir_dev *r_dev;
	struct class  *chr_class;
	struct cdev chrdev;
	struct mutex  file_lock; /*mutex for ioctl*/
	struct list_head map_tab_head;
	struct meson_ir_map_tab_list *cur_tab;
	struct key_number key_num;
	/**
	 *multi_format IR controller register saved to ir_contr[0]
	 *legacy IR controller register saved to ir_contr[1]
	 */
	struct meson_ir_contr_desc ir_contr[2];

	char *dev_name;
	const char *keymap_name;

	dev_t chr_devno;
	spinlock_t slock; /*spinlock for ir data*/

	unsigned int rx_count;
	unsigned int rx_buffer[RX_BUFFER_MAX_SIZE];
	int custom_num;
	int decode_status;
	int sys_custom_code;
	int irqno;/*irq number*/
	int irq_cpumask;
	int protocol;
	unsigned int vendor;
	unsigned int product;
	unsigned int version;
	unsigned int input_cnt;
	unsigned int search_id[3];
	int (*report_key)(struct meson_ir_chip *chip);
	int (*release_key)(struct meson_ir_chip *chip);
	int (*set_register_config)(struct meson_ir_chip *chip, int type);
	int (*debug_printk)(const char *buf, ...);

	u8 debug_enable;
	unsigned char bit_count;
	/**
	 *indicate which ir controller working.
	 *0: multi format IR
	 *1: legacy IR
	 */
	unsigned char ir_work;
};

struct meson_ir_reg_proto {
	struct meson_ir_reg_map *reg_map;
	int reg_map_size;
	int protocol;
	int (*get_scancode)(struct meson_ir_chip *chip);
	int (*get_decode_status)(struct meson_ir_chip *chip);
	u32 (*get_custom_code)(struct meson_ir_chip *chip);
	bool (*set_custom_code)(struct meson_ir_chip *chip, u32 code);
	char *name;
};

enum meson_ir_reg {
	REG_LDR_ACTIVE		= 0x00 << 2,
	REG_LDR_IDLE		= 0x01 << 2,
	REG_LDR_REPEAT		= 0x02 << 2,
	REG_BIT_0		= 0x03 << 2,
	REG_REG0		= 0x04 << 2,
	REG_FRAME		= 0x05 << 2,
	REG_STATUS		= 0x06 << 2,
	REG_REG1		= 0x07 << 2,
	REG_REG2		= 0x08 << 2,
	REG_DURATN2		= 0x09 << 2,
	REG_DURATN3		= 0x0a << 2,
	REG_FRAME1		= 0x0b << 2,
	REG_STATUS1		= 0x0c << 2,
	REG_STATUS2		= 0x0d << 2,
	REG_REG3		= 0x0e << 2,
	REG_FRAME_RSV0		= 0x0f << 2,
	REG_FRAME_RSV1		= 0x10 << 2,
	REG_FILTE		= 0x11 << 2,
	REG_IRQ_CTL		= 0x12 << 2,
	REG_FIFO_CTL		= 0x13 << 2,
	REG_WIDTH_NEW		= 0x14 << 2,
	REG_REPEAT_DET		= 0x15 << 2,
/*for demodulation*/
	REG_DEMOD_CNTL0		= 0x20 << 2,
	REG_DEMOD_CNTL1		= 0x21 << 2,
	REG_DEMOD_IIR_THD	= 0x22 << 2,
	REG_DEMOD_THD0		= 0x23 << 2,
	REG_DEMOD_THD1		= 0x24 << 2,
	REG_DEMOD_SUM_CNT0	= 0x25 << 2,
	REG_DEMOD_SUM_CNT1	= 0x26 << 2,
	REG_DEMOD_CNT0		= 0x27 << 2,
	REG_DEMOD_CNT1		= 0x28 << 2
};

int meson_ir_register_default_config(struct meson_ir_chip *chip, int type);
int meson_ir_cdev_init(struct meson_ir_chip *chip);
void meson_ir_cdev_free(struct meson_ir_chip *chip);
int meson_ir_scancode_sort(struct ir_map_tab *ir_map);
struct meson_ir_map_tab_list *meson_ir_seek_map_tab(struct meson_ir_chip *chip,
						    int custom_code);
void meson_ir_input_ots_configure(struct meson_ir_dev *dev, int cnt0);
void meson_ir_input_device_ots_init(struct input_dev *dev, struct device *parent,
		struct meson_ir_chip *chip, const char *name, int cnt);
void meson_ir_timer_keyup(struct timer_list *t);
void meson_ir_tab_free(struct meson_ir_map_tab_list *ir_map_list);
int meson_ir_pulses_malloc(struct meson_ir_chip *chip);
void meson_ir_pulses_free(struct meson_ir_chip *chip);

int meson_ir_xmp_decode_init(void);
void meson_ir_xmp_decode_exit(void);
#endif
