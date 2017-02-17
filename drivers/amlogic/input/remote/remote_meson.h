/*
 * drivers/amlogic/input/remote/remote_meson.c
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

#ifndef _REMOTE_AML_H
#define _REMOTE_AML_H
#include <linux/cdev.h>
#include "remote_core.h"

#define IR_DATA_IS_VALID(data) (data & 0x8)
#define IR_CONTROLLER_BUSY(x) ((x >> 7) & 0x1)

enum IR_CONTR_NUMBER {
	MULTI_IR_ID = 0,
	LEGACY_IR_ID,
	IR_ID_MAX
};

struct remote_range {
	struct range active;
	struct range idle;
	struct range repeat;
	struct range bit_zero_zero;
	struct range bit_zero_one;
	struct range bit_one_zero;
	struct range bit_one_one;
};

struct remote_reg_map {
	unsigned int reg;
	unsigned int val;
};


struct custom_table {
	struct list_head list;
	u8 id;
	const char *custom_name;
	u32 custom_code;

	u16 map_size;
#define MAX_KEYMAP_SIZE 256
	union _codemap {
		struct remote_key_map {
			u16 keycode;
			u16 scancode;
		} map;
		u32 code;
	} *codemap;
};

struct cdev;
struct remote_chip;

/**
  *struct remote_contr_desc - describe the different properties and methods
  *for the Legacy IR controller and multi-format IR controller.
  *TODO: compatible with the "struct aml_remote_reg_proto"
  */
struct remote_contr_desc {
	void __iomem *remote_regs;
	char *proto_name;
	int (*get_scancode)(struct remote_chip *chip);
	int (*get_decode_status)(struct remote_chip *chip);
	u32 (*get_custom_code)(struct remote_chip *chip);
	bool (*set_custom_code)(struct remote_chip *chip, u32 code);
};
/**
  *struct remote_chip - describe the common properties and methods
  * for the Legacy IR controller and multi-format IR controller.
  */
struct remote_chip {
	struct device *dev;
	struct remote_dev *r_dev;
	struct remote_range reg_duration;
	char *dev_name;
	int protocol;
	int release_delay;

	dev_t chr_devno;
	struct class  *chr_class;
	struct cdev chrdev;
	struct mutex  file_lock;
	spinlock_t slock;

	bool debug_enable;
#define CUSTOM_TABLES_SIZE 20
#define CUSTOM_NUM_MAX 20
	struct custom_table *custom_tables;
	struct custom_table *cur_custom;
	struct custom_table *sys_custom;
	int custom_num;
	int decode_status;

	const char *keymap_name;
	int	irqno;       /*irq number*/
	int	irq_cpumask;
	/**
	  *indicate which ir controller working.
	  *0: multi format IR
	  *1: legacy IR
	  */
	unsigned char ir_work;
	/**
	  *multi_format IR controller register saved to ir_contr[0]
	  *legacy IR controller register saved to ir_contr[1]
	  */
	struct remote_contr_desc ir_contr[2];

	int (*report_key)(struct remote_chip *chip);
	int (*release_key)(struct remote_chip *chip);
	int (*set_register_config)(struct remote_chip *chip, int type);
	int (*debug_printk)(const char *, ...);
};

struct aml_remote_reg_proto {
	int protocol;
	char *name;
	struct aml_remote_reg *reg;
	struct remote_reg_map *reg_map;
	int reg_map_size;
	int (*get_scancode)(struct remote_chip *chip);
	int (*get_decode_status)(struct remote_chip *chip);
	u32 (*get_custom_code)(struct remote_chip *chip);
	bool (*set_custom_code)(struct remote_chip *chip, u32 code);
};

enum {
	DECODE_MODE_NEC			= 0x00,
	DECODE_MODE_SKIP_LEADER = 0x01,
	DECODE_MODE_SOFTWARE    = 0x02,
	DECODE_MODE_MITSUBISHI_OR_50560 = 0x03,
	DECODE_MODE_DUOKAN = 0x0B
};

enum remote_reg {
	REG_LDR_ACTIVE = 0x00<<2,
	REG_LDR_IDLE   = 0x01<<2,
	REG_LDR_REPEAT = 0x02<<2,
	REG_BIT_0      = 0x03<<2,
	REG_REG0       = 0x04<<2,
	REG_FRAME      = 0x05<<2,
	REG_STATUS     = 0x06<<2,
	REG_REG1       = 0x07<<2,
	REG_REG2       = 0x08<<2,
	REG_DURATN2    = 0x09<<2,
	REG_DURATN3    = 0x0a<<2,
	REG_FRAME1     = 0x0b<<2,
	REG_STATUS1    = 0x0c<<2,
	REG_STATUS2    = 0x0d<<2,
	REG_REG3       = 0x0e<<2,
	REG_FRAME_RSV0 = 0x0f<<2,
	REG_FRAME_RSV1 = 0x10<<2
};

/*remote config  ioctl  cmd*/
#define REMOTE_IOC_INFCODE_CONFIG       _IOW('I', 13, u32)
#define REMOTE_IOC_RESET_KEY_MAPPING        _IOW('I', 3, u32)
#define REMOTE_IOC_SET_KEY_MAPPING          _IOW('I', 4, u32)
#define REMOTE_IOC_SET_REPEAT_KEY_MAPPING   _IOW('I', 20, u32)
#define REMOTE_IOC_SET_MOUSE_MAPPING        _IOW('I', 5, u32)
#define REMOTE_IOC_SET_REPEAT_DELAY         _IOW('I', 6, u32)
#define REMOTE_IOC_SET_REPEAT_PERIOD        _IOW('I', 7, u32)

#define REMOTE_IOC_SET_REPEAT_ENABLE        _IOW('I', 8, u32)
#define REMOTE_IOC_SET_DEBUG_ENABLE         _IOW('I', 9, u32)
#define REMOTE_IOC_SET_MODE                 _IOW('I', 10, u32)

#define REMOTE_IOC_SET_CUSTOMCODE       _IOW('I', 100, u32)
#define REMOTE_IOC_SET_RELEASE_DELAY        _IOW('I', 99, u32)

/*reg*/
#define REMOTE_IOC_SET_REG_BASE_GEN         _IOW('I', 101, u32)
#define REMOTE_IOC_SET_REG_CONTROL          _IOW('I', 102, u32)
#define REMOTE_IOC_SET_REG_LEADER_ACT       _IOW('I', 103, u32)
#define REMOTE_IOC_SET_REG_LEADER_IDLE      _IOW('I', 104, u32)
#define REMOTE_IOC_SET_REG_REPEAT_LEADER    _IOW('I', 105, u32)
#define REMOTE_IOC_SET_REG_BIT0_TIME         _IOW('I', 106, u32)

/*sw*/
#define REMOTE_IOC_SET_BIT_COUNT            _IOW('I', 107, u32)
#define REMOTE_IOC_SET_TW_LEADER_ACT        _IOW('I', 108, u32)
#define REMOTE_IOC_SET_TW_BIT0_TIME         _IOW('I', 109, u32)
#define REMOTE_IOC_SET_TW_BIT1_TIME         _IOW('I', 110, u32)
#define REMOTE_IOC_SET_TW_REPEATE_LEADER    _IOW('I', 111, u32)

#define REMOTE_IOC_GET_TW_LEADER_ACT        _IOR('I', 112, u32)
#define REMOTE_IOC_GET_TW_BIT0_TIME         _IOR('I', 113, u32)
#define REMOTE_IOC_GET_TW_BIT1_TIME         _IOR('I', 114, u32)
#define REMOTE_IOC_GET_TW_REPEATE_LEADER    _IOR('I', 115, u32)

#define REMOTE_IOC_GET_REG_BASE_GEN         _IOR('I', 121, u32)
#define REMOTE_IOC_GET_REG_CONTROL          _IOR('I', 122, u32)
#define REMOTE_IOC_GET_REG_LEADER_ACT       _IOR('I', 123, u32)
#define REMOTE_IOC_GET_REG_LEADER_IDLE      _IOR('I', 124, u32)
#define REMOTE_IOC_GET_REG_REPEAT_LEADER    _IOR('I', 125, u32)
#define REMOTE_IOC_GET_REG_BIT0_TIME        _IOR('I', 126, u32)
#define REMOTE_IOC_GET_REG_FRAME_DATA       _IOR('I', 127, u32)
#define REMOTE_IOC_GET_REG_FRAME_STATUS     _IOR('I', 128, u32)

#define REMOTE_IOC_SET_TW_BIT2_TIME         _IOW('I', 129, u32)
#define REMOTE_IOC_SET_TW_BIT3_TIME         _IOW('I', 130, u32)

#define   REMOTE_IOC_SET_FN_KEY_SCANCODE     _IOW('I', 131, u32)
#define   REMOTE_IOC_SET_LEFT_KEY_SCANCODE   _IOW('I', 132, u32)
#define   REMOTE_IOC_SET_RIGHT_KEY_SCANCODE  _IOW('I', 133, u32)
#define   REMOTE_IOC_SET_UP_KEY_SCANCODE     _IOW('I', 134, u32)
#define   REMOTE_IOC_SET_DOWN_KEY_SCANCODE   _IOW('I', 135, u32)
#define   REMOTE_IOC_SET_OK_KEY_SCANCODE     _IOW('I', 136, u32)
#define   REMOTE_IOC_SET_PAGEUP_KEY_SCANCODE _IOW('I', 137, u32)
#define REMOTE_IOC_SET_PAGEDOWN_KEY_SCANCODE _IOW('I', 138, u32)
#define   REMOTE_IOC_SET_RELT_DELAY     _IOW('I', 140, u32)

int ir_register_default_config(struct remote_chip *chip, int type);
int remote_reg_read(struct remote_chip *chip, unsigned char id,
	unsigned int reg, unsigned int *val);
int remote_reg_write(struct remote_chip *chip, unsigned char id,
	unsigned int reg, unsigned int val);


#endif



