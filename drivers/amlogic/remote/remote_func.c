/*
 * drivers/amlogic/remote/remote_func.c
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

#include <asm/ioctl.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/types.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/errno.h>
#include <asm/irq.h>
#include "remote_main.h"

static int auto_repeat_count, repeat_count;
static void remote_rel_timer_sr(unsigned long data);
static void remote_repeat_sr(unsigned long data);
static void remote_rca_repeat_sr(unsigned long data);
/*remote config val*/
/****************************************************************/
struct reg_s RDECODEMODE_NEC[] = {
	{LDR_ACTIVE , ((unsigned)477 << 16) | ((unsigned) 400 << 0)},
	{LDR_IDLE , (248 << 16) | (202 << 0)},
	{LDR_REPEAT , (130 << 16) | (110 << 0)},
	{DURATION_REG0 , (60 << 16) | (48 << 0) },
	{OPERATION_CTRL_REG0 , (3 << 28) | (0xFA0 << 12) | 0x13},
	{DURATION_REG1_AND_STATUS , (111<<20) | (100<<10)},
	{OPERATION_CTRL_REG1 , 0x9f40},
	{DURATION_REG2 , 0},
	{DURATION_REG2 , 0},
	{DURATION_REG3 , 0}
};
/****************************************************************/
struct reg_s RDECODEMODE_DUOKAN[] = {
	{LDR_ACTIVE , 53<<16 | 50<<0},
	{LDR_IDLE , 31<<16 | 25<<0},
	{LDR_REPEAT , 30<<16 | 26<<0},
	{DURATION_REG0 , 61<<16 | 55<<0 },
	{OPERATION_CTRL_REG0 , 3<<28 | (0x5DC<<12) | 0x13},
	{DURATION_REG1_AND_STATUS , (76<<20) | 69<<10},
	{OPERATION_CTRL_REG1 , 0x9300},
	{OPERATION_CTRL_REG2 , 0x10b},
	{DURATION_REG2 , 91<<16 | 79<<0},
	{DURATION_REG3 , 111<<16 | 99<<0}
};
/****************************************************************/
struct reg_s RDECODEMODE_RCMM[] = {
	{LDR_ACTIVE , 25<<16 | 22<<0},
	{LDR_IDLE , 14<<16 | 13<<0},
	{LDR_REPEAT , 14<<16 | 13<<0},
	{DURATION_REG0 , 25<<16 | 21<<0 },
	{OPERATION_CTRL_REG0 , 3<<28 | (0x708<<12) | 0x13},
	{DURATION_REG1_AND_STATUS , 33<<20 | 29<<10},
	{OPERATION_CTRL_REG1 , 0xbe40},
	{OPERATION_CTRL_REG2 , 0xa},
	{DURATION_REG2 , 39<<16 | 36<<0},
	{DURATION_REG3 , 50<<16 | 46<<0}
};
/****************************************************************/
struct reg_s RDECODEMODE_SONYSIRC[] = {
	{LDR_ACTIVE , 130<<16 | 110<<0},
	{LDR_IDLE , 33<<16 | 27<<0},
	{LDR_REPEAT , 33<<16 | 27<<0},
	{DURATION_REG0 , 63<<16 | 56<<0 },
	{OPERATION_CTRL_REG0 , 3<<28 | (0x8ca<<12) | 0x13},
	{DURATION_REG1_AND_STATUS , 94<<20 | 82<<10},
	{OPERATION_CTRL_REG1 , 0xbe40},
	{OPERATION_CTRL_REG2 , 0x6},
	{DURATION_REG2 , 0},
	{DURATION_REG3 , 0}
};

/**************************************************************/

struct reg_s RDECODEMODE_MITSUBISHI[] = {
	{LDR_ACTIVE , 410<<16 | 390<<0},
	{LDR_IDLE , 225<<16 | 200<<0},
	{LDR_REPEAT , 225<<16 | 200<<0},
	{DURATION_REG0 , 60<<16 | 48<<0 },
	{OPERATION_CTRL_REG0 , 3<<28 | (0xBB8<<12) | 0x13},
	{DURATION_REG1_AND_STATUS , 110<<20 | 95<<10},
	{OPERATION_CTRL_REG1 , 0xbe40},
	{OPERATION_CTRL_REG2 , 0x3},
	{DURATION_REG2 , 0},
	{DURATION_REG3 , 0}

};
/**********************************************************/
struct reg_s RDECODEMODE_TOSHIBA[] = {
	{LDR_ACTIVE , 477<<16 | 389<<0},
	{LDR_IDLE , 477<<16 | 389<<0},
	{LDR_REPEAT , 460<<16 | 389<<0},
	{DURATION_REG0 , 60<<16 | 40<<0 },
	{OPERATION_CTRL_REG0 , 3<<28|(0xFA0<<12)|0x13},
	{DURATION_REG1_AND_STATUS , 111<<20|100<<10},
	{OPERATION_CTRL_REG1 , 0xbe40},
	{OPERATION_CTRL_REG2 , 0x5},
	{DURATION_REG2 , 0},
	{DURATION_REG3 , 0}

};
/*****************************************************************/
struct reg_s RDECODEMODE_THOMSON[] = {
	{LDR_ACTIVE , 477<<16 | 390<<0},
	{LDR_IDLE , 477<<16 | 390<<0},
	{LDR_REPEAT , 460<<16 | 390<<0},
	{DURATION_REG0 , 80<<16 | 60<<0 },
	{OPERATION_CTRL_REG0 , 3<<28 | (0xFA0<<12) | 0x13},
	{DURATION_REG1_AND_STATUS , 140<<20 | 120<<10},
	{OPERATION_CTRL_REG1 , 0xbe40},
	{OPERATION_CTRL_REG2 , 0x4},
	{DURATION_REG2 , 0},
	{DURATION_REG3 , 0}
};
/*********************************************************************/
struct reg_s RDECODEMODE_COMCAST[] = {
	{LDR_ACTIVE , 0   },
	{LDR_IDLE , 0  },
	{LDR_REPEAT , 0   },
	{DURATION_REG0 , 0},
	{OPERATION_CTRL_REG0 , 0},
	{DURATION_REG1_AND_STATUS ,},
	{OPERATION_CTRL_REG1 ,},
	{OPERATION_CTRL_REG2 ,},
	{DURATION_REG2 ,},
	{DURATION_REG3 ,}
};
struct reg_s RDECODEMODE_SKIPLEADER[] = {
	{LDR_ACTIVE ,    },
	{LDR_IDLE ,     },
	{LDR_REPEAT ,    },
	{DURATION_REG0 , },
	{OPERATION_CTRL_REG0 ,},
	{DURATION_REG1_AND_STATUS ,},
	{OPERATION_CTRL_REG1 ,},
	{OPERATION_CTRL_REG2 ,},
	{DURATION_REG2 ,},
	{DURATION_REG3 ,}
};
struct reg_s RDECODEMODE_SW[] = {
	{LDR_ACTIVE ,    },
	{LDR_IDLE ,     },
	{LDR_REPEAT ,    },
	{DURATION_REG0 , },
	{OPERATION_CTRL_REG0 ,},
	{DURATION_REG1_AND_STATUS ,},
	{OPERATION_CTRL_REG1 ,},
	{OPERATION_CTRL_REG2 ,},
	{DURATION_REG2 ,},
	{DURATION_REG3 ,}
};
struct reg_s RDECODEMODE_SW_NEC[] = {
	{LDR_ACTIVE , ((unsigned)477<<16) | ((unsigned)400<<0)},
	{LDR_IDLE , 248<<16 | 202<<0},
	{LDR_REPEAT , 130<<16 | 110<<0},
	{DURATION_REG0 , 60<<16 | 48<<0 },
	{OPERATION_CTRL_REG0 , 3<<28 | (0xFA0<<12) | 0x13},
	{DURATION_REG1_AND_STATUS , (111<<20) | (100<<10)},
	{OPERATION_CTRL_REG1 , 0x8578},
	{OPERATION_CTRL_REG2 , 0x2},
	{DURATION_REG2 , 0},
	{DURATION_REG3 , 0}
};
struct reg_s RDECODEMODE_SW_DUOKAN[] = {
	{LDR_ACTIVE , 52<<16 | 49<<0},
	{LDR_IDLE , 30<<16 | 26<<0},
	{LDR_REPEAT , 30<<16 | 26<<0},
	{DURATION_REG0 , 60<<16 | 56<<0 },
	{OPERATION_CTRL_REG0 , 3<<28 | (0x5DC<<12) | 0x13},
	{DURATION_REG1_AND_STATUS , (75<<20) | 70<<10},
	{OPERATION_CTRL_REG1 , 0x8578},
	{OPERATION_CTRL_REG2 , 0x2},
	{DURATION_REG2 , 0},
	{DURATION_REG3 , 0}
};
struct reg_s RDECODEMODE_NEC_RCA_2IN1[] = {
	{LDR_ACTIVE-0x100 , ((unsigned)477<<16) | ((unsigned)400<<0)},
	{LDR_IDLE-0x100 , 248<<16 | 202<<0},
	{LDR_REPEAT-0x100 , 130<<16|110<<0},
	{DURATION_REG0-0x100 , 60<<16|48<<0 },
	{OPERATION_CTRL_REG0-0x100 , 3<<28|(0xFA0<<12)|0x13},
	{DURATION_REG1_AND_STATUS-0x100 , (111<<20)|(100<<10)},
	{OPERATION_CTRL_REG1-0x100 , 0xbe40},
	{LDR_ACTIVE , ((unsigned)250<<16) | ((unsigned)160<<0)},
	{LDR_IDLE , 250<<16 | 160<<0},
	{LDR_REPEAT , 250<<16|160<<0},
	{DURATION_REG0 , 100<<16|48<<0 },
	{OPERATION_CTRL_REG0 , 3<<28|(0xFA0<<12)|0x13},
	{DURATION_REG1_AND_STATUS , (150<<20)|(110<<10)},
	{OPERATION_CTRL_REG1 , 0x9740},
	/*it may get the wrong customer value and key value from regiter
	if the value is set to 0x4,so the register value must set to 0x104*/
	{OPERATION_CTRL_REG2 , 0x104},
	{DURATION_REG2 , 0},
	{DURATION_REG3 , 0}
};
struct reg_s RDECODEMODE_NEC_TOSHIBA_2IN1[] = {
	{LDR_ACTIVE-0x100 , ((unsigned)477<<16) | ((unsigned)400<<0)},
	{LDR_IDLE-0x100 , 248<<16 | 202<<0},
	{LDR_REPEAT-0x100 , 130<<16|110<<0},
	{DURATION_REG0-0x100 , 60<<16|48<<0 },
	{OPERATION_CTRL_REG0-0x100 , 3<<28|(0xFA0<<12)|0x13},
	{DURATION_REG1_AND_STATUS-0x100 , (111<<20)|(100<<10)},
	{OPERATION_CTRL_REG1-0x100 , 0xbe00},
	{LDR_ACTIVE , ((unsigned)300<<16) | ((unsigned)160<<0)},
	{LDR_IDLE , 300<<16 | 160<<0},
	{LDR_REPEAT , 300<<16|160<<0},
	{DURATION_REG0 , 90<<16|48<<0 },
	{OPERATION_CTRL_REG0 , 3<<28|(0xFA0<<12)|0x13},
	{DURATION_REG1_AND_STATUS , (150<<20)|(100<<10)},
	{OPERATION_CTRL_REG1 , 0x9f40},
	{OPERATION_CTRL_REG2 , 0x5},
	{DURATION_REG2 , 0},
	{DURATION_REG3 , 0}
};
struct reg_s RDECODEMODE_RC5[] = {
	{LDR_ACTIVE ,    },
	{LDR_IDLE ,     },
	{LDR_REPEAT ,    },
	{DURATION_REG0 , },
	{OPERATION_CTRL_REG0 ,},
	{DURATION_REG1_AND_STATUS ,},
	{OPERATION_CTRL_REG1 ,},
	{OPERATION_CTRL_REG2 ,},
	{DURATION_REG2 ,},
	{DURATION_REG3 ,}
};
struct reg_s RDECODEMODE_RESERVED[] = {
	{LDR_ACTIVE ,    },
	{LDR_IDLE ,     },
	{LDR_REPEAT ,    },
	{DURATION_REG0 , },
	{OPERATION_CTRL_REG0 ,},
	{DURATION_REG1_AND_STATUS ,},
	{OPERATION_CTRL_REG1 ,},
	{OPERATION_CTRL_REG2 ,},
	{DURATION_REG2 ,},
	{DURATION_REG3 ,}
};
struct reg_s RDECODEMODE_RC6[] = {
	{LDR_ACTIVE ,    },
	{LDR_IDLE ,     },
	{LDR_REPEAT ,    },
	{DURATION_REG0 , },
	{OPERATION_CTRL_REG0 ,},
	{DURATION_REG1_AND_STATUS ,},
	{OPERATION_CTRL_REG1 ,},
	{OPERATION_CTRL_REG2 ,},
	{DURATION_REG2 ,},
	{DURATION_REG3 ,}
};



struct reg_s RDECODEMODE_SANYO[] = {
	{LDR_ACTIVE ,    },
	{LDR_IDLE ,     },
	{LDR_REPEAT ,    },
	{DURATION_REG0 , },
	{OPERATION_CTRL_REG0 ,},
	{DURATION_REG1_AND_STATUS ,},
	{OPERATION_CTRL_REG1 ,},
	{OPERATION_CTRL_REG2 ,},
	{DURATION_REG2 ,},
	{DURATION_REG3 ,}
};
struct reg_s RDECODEMODE_RCA[] = {
	{LDR_ACTIVE ,    },
	{LDR_IDLE ,     },
	{LDR_REPEAT ,    },
	{DURATION_REG0 , },
	{OPERATION_CTRL_REG0 ,},
	{DURATION_REG1_AND_STATUS ,},
	{OPERATION_CTRL_REG1 ,},
	{OPERATION_CTRL_REG2 ,},
	{DURATION_REG2 ,},
	{DURATION_REG3 ,}
};
static int dbg_printk(const char *fmt, ...)
{
	char buf[100];
	va_list args;
	va_start(args, fmt);
	vscnprintf(buf, 100, fmt, args);
	if (strlen(remote_log_buf) + (strlen(buf) + 64) > REMOTE_LOG_BUF_LEN)
		remote_log_buf[0] = '\0';
	strcat(remote_log_buf , buf);
	va_end(args);
	return 0;
}
int set_remote_mode(struct remote *remote_data, int mode)
{
	int i;
	switch (mode) {
	case DECODEMODE_NEC:
		remote_data->reg = RDECODEMODE_NEC;
		remote_data->reg_size = ARRAY_SIZE(RDECODEMODE_NEC);
		break;
	case DECODEMODE_DUOKAN:
		remote_data->reg = RDECODEMODE_DUOKAN;
		remote_data->reg_size = ARRAY_SIZE(RDECODEMODE_DUOKAN);
		break;
	case DECODEMODE_RCMM:
		remote_data->reg = RDECODEMODE_RCMM;
		remote_data->reg_size = ARRAY_SIZE(RDECODEMODE_RCMM);
		break;
	case DECODEMODE_SONYSIRC:
		remote_data->reg = RDECODEMODE_SONYSIRC;
		remote_data->reg_size = ARRAY_SIZE(RDECODEMODE_SONYSIRC);
		break;
	case DECODEMODE_SKIPLEADER:
		remote_data->reg = RDECODEMODE_SKIPLEADER;
		remote_data->reg_size = ARRAY_SIZE(RDECODEMODE_SKIPLEADER);
		break;
	case DECODEMODE_MITSUBISHI:
		remote_data->reg = RDECODEMODE_MITSUBISHI;
		remote_data->reg_size = ARRAY_SIZE(RDECODEMODE_MITSUBISHI);
		break;
	case DECODEMODE_THOMSON:
		remote_data->reg = RDECODEMODE_THOMSON;
		remote_data->reg_size = ARRAY_SIZE(RDECODEMODE_THOMSON);
		break;
	case DECODEMODE_TOSHIBA:
		remote_data->reg = RDECODEMODE_TOSHIBA;
		remote_data->reg_size = ARRAY_SIZE(RDECODEMODE_TOSHIBA);
		break;
	case DECODEMODE_RC5:
		remote_data->reg = RDECODEMODE_RC5;
		remote_data->reg_size = ARRAY_SIZE(RDECODEMODE_RC5);
		break;
	case DECODEMODE_RESERVED:
		remote_data->reg = RDECODEMODE_RESERVED;
		remote_data->reg_size = ARRAY_SIZE(RDECODEMODE_RESERVED);
		break;
	case DECODEMODE_RC6:
		remote_data->reg = RDECODEMODE_RC6;
		remote_data->reg_size = ARRAY_SIZE(RDECODEMODE_RC6);
		break;
	case DECODEMODE_RCA:
		remote_data->reg = RDECODEMODE_RCA;
		remote_data->reg_size = ARRAY_SIZE(RDECODEMODE_RCA);
		break;
	case DECODEMODE_COMCAST:
		remote_data->reg = RDECODEMODE_COMCAST;
		remote_data->reg_size = ARRAY_SIZE(RDECODEMODE_COMCAST);
		break;
	case DECODEMODE_SANYO:
		remote_data->reg = RDECODEMODE_SANYO;
		remote_data->reg_size = ARRAY_SIZE(RDECODEMODE_SANYO);
		break;
	case DECODEMODE_NEC_RCA_2IN1:
		remote_data->reg = RDECODEMODE_NEC_RCA_2IN1;
		remote_data->reg_size = ARRAY_SIZE(RDECODEMODE_NEC_RCA_2IN1);
		break;
	case DECODEMODE_NEC_TOSHIBA_2IN1:
		remote_data->reg = RDECODEMODE_NEC_TOSHIBA_2IN1;
		remote_data->reg_size =
			ARRAY_SIZE(RDECODEMODE_NEC_TOSHIBA_2IN1);
		break;
	case DECODEMODE_SW:
		remote_data->reg = RDECODEMODE_SW;
		remote_data->reg_size = ARRAY_SIZE(RDECODEMODE_SW);
		break;
	case DECODEMODE_SW_NEC:
		remote_data->reg = RDECODEMODE_SW_NEC;
		remote_data->reg_size = ARRAY_SIZE(RDECODEMODE_SW_NEC);
		break;
	case DECODEMODE_SW_DUOKAN:
		remote_data->reg = RDECODEMODE_SW_DUOKAN;
		remote_data->reg_size = ARRAY_SIZE(RDECODEMODE_SW_DUOKAN);
		break;
	}

	for (i = 0; i < remote_data->reg_size; i++) {
		aml_write_aobus(remote_data->reg[i].reg,
				remote_data->reg[i].val);
		input_dbg("[0x%x] = 0x%x\n",
			remote_data->reg[i].reg, remote_data->reg[i].val);
	}
	input_dbg("%s[%d]\n", __func__, __LINE__);
	return 0;

}
void config_sw_init_window(struct remote *remote_data)
{
	switch (remote_data->work_mode) {
	case DECODEMODE_SW_NEC:
		remote_data->bit_count = 32;
		remote_data->debug_enable = 1;
		remote_data->release_delay[remote_data->map_num]  = 108;
		remote_data->repeat_enable  = 0;
		remote_data->time_window[0] = 500;
		remote_data->time_window[1] = 700;
		remote_data->time_window[2] = 50;
		remote_data->time_window[3] = 80;
		remote_data->time_window[4] = 100;
		remote_data->time_window[5] = 130;
		remote_data->time_window[6] = 800;
		remote_data->time_window[7] = 900;
		break;
	case DECODEMODE_SW_DUOKAN:
		remote_data->bit_count = 20;
		remote_data->debug_enable = 1;
		remote_data->repeat_enable = 0;
		remote_data->time_window[0] = 79;
		remote_data->time_window[1] = 83;
		remote_data->time_window[2] = 54;
		remote_data->time_window[3] = 61;
		remote_data->time_window[4] = 70;
		remote_data->time_window[5] = 78;
		remote_data->time_window[6] = 256;
		remote_data->time_window[7] = 768;
		remote_data->time_window[8] = 84;
		remote_data->time_window[9] = 93;
		remote_data->time_window[10] = 99;
		remote_data->time_window[11] = 106;
		break;
	default:
		break;

	}

}
void kdb_send_key(struct input_dev *dev, unsigned int scancode,
		unsigned int type, int event);

void set_remote_init(struct remote *remote_data)
{
	if (remote_data->work_mode <= DECODEMODE_MAX) {
		if (remote_data->work_mode > DECODEMODE_NEC) {
			if (remote_data->work_mode == DECODEMODE_NEC_RCA_2IN1)
				setup_timer(&remote_data->repeat_timer,
				remote_rca_repeat_sr, 0);
			else
				setup_timer(&remote_data->repeat_timer,
				remote_repeat_sr, 0);
		}
		input_dbg("enter in sw repeat mode\n");
		return;
	}
	config_sw_init_window(remote_data);
}
void changeduokandecodeorder(struct remote *remote_data)
{
	unsigned int scancode = remote_data->cur_lsbkeycode;
	remote_data->cur_lsbkeycode = ((scancode & 0x3) << 18) |
	((scancode & 0xc) << 14) | ((scancode & 0x30) << 10) |
	((scancode & 0xc0) << 6) | ((scancode & 0x300) << 2) |
	((scancode & 0xc00) >> 2) | ((scancode & 0x3000) >> 6) |
	((scancode & 0xc000) >> 10) | ((scancode & 0x30000) >> 14) |
	((scancode & 0xc0000) >> 18);
	if (remote_data->cur_lsbkeycode == 0x0003cccf) {
		remote_data->cur_lsbkeycode =
		((remote_data->custom_code[0] & 0xff) << 12) | 0xa0;
	}
}
void get_cur_scancode(struct remote *remote_data)
{
	int temp_cur_lsbkeycode = 0;
	if (remote_data->work_mode == DECODEMODE_SANYO) {
		remote_data->cur_lsbkeycode = aml_read_aobus(FRAME_BODY);
		remote_data->cur_msbkeycode =
		aml_read_aobus(FRAME_BODY1) & 0x2ff;
	} else if (remote_data->work_mode ==  DECODEMODE_NEC_RCA_2IN1) {
		temp_cur_lsbkeycode = aml_read_aobus(FRAME_BODY);
		if (temp_cur_lsbkeycode != 0) {
			remote_data->temp_work_mode =
					DECODEMODE_RCA;
			remote_data->cur_lsbkeycode =
					temp_cur_lsbkeycode;
			temp_cur_lsbkeycode = 0;
		}
		temp_cur_lsbkeycode = aml_read_aobus(FRAME_BODY - 0x100);
		if ((aml_read_aobus(DURATION_REG1_AND_STATUS - 0x100) >> 3
		& 0x1) && (temp_cur_lsbkeycode != 0)) {
			remote_data->temp_work_mode = DECODEMODE_NEC;
			remote_data->cur_lsbkeycode =  temp_cur_lsbkeycode;
			temp_cur_lsbkeycode = 0;
		}
	} else if (remote_data->work_mode ==  DECODEMODE_NEC_TOSHIBA_2IN1) {
		temp_cur_lsbkeycode = aml_read_aobus(FRAME_BODY);
		if (temp_cur_lsbkeycode != 0) {
			remote_data->temp_work_mode = DECODEMODE_TOSHIBA;
			remote_data->cur_lsbkeycode = temp_cur_lsbkeycode;
			temp_cur_lsbkeycode = 0;
		}
		temp_cur_lsbkeycode = aml_read_aobus(FRAME_BODY - 0x100);
		if ((aml_read_aobus(DURATION_REG1_AND_STATUS - 0x100) >> 3
			& 0x1) && (temp_cur_lsbkeycode != 0)) {
			remote_data->temp_work_mode = DECODEMODE_NEC;
			remote_data->cur_lsbkeycode = temp_cur_lsbkeycode;
			temp_cur_lsbkeycode = 0;
		}
	} else if (remote_data->work_mode > DECODEMODE_MAX) {
		remote_data->cur_lsbkeycode = remote_data->cur_keycode;
		if (remote_data->work_mode == DECODEMODE_SW_DUOKAN)
			changeduokandecodeorder(remote_data);
	} else
		remote_data->cur_lsbkeycode = aml_read_aobus(FRAME_BODY);
}
void get_cur_scanstatus(struct remote *remote_data)
{
	if (remote_data->work_mode == DECODEMODE_NEC_RCA_2IN1) {
		if (remote_data->temp_work_mode == DECODEMODE_RCA)
			remote_data->frame_status =
			aml_read_aobus(DURATION_REG1_AND_STATUS);
		if (remote_data->temp_work_mode == DECODEMODE_NEC)
			remote_data->frame_status =
			aml_read_aobus(DURATION_REG1_AND_STATUS - 0x100);

	} else if (remote_data->work_mode == DECODEMODE_NEC_TOSHIBA_2IN1) {
		if (remote_data->temp_work_mode == DECODEMODE_TOSHIBA) {
			remote_data->frame_status =
			aml_read_aobus(DURATION_REG1_AND_STATUS);
			if (remote_data->cur_lsbkeycode == 0x1 ||
				remote_data->cur_lsbkeycode == 0x0) {
				remote_data->frame_status = 0x1;
				remote_data->cur_lsbkeycode =  0x0;
			}
		}
		if (remote_data->temp_work_mode == DECODEMODE_NEC)
			remote_data->frame_status =
			aml_read_aobus(DURATION_REG1_AND_STATUS - 0x100);
	} else
		remote_data->frame_status =
		aml_read_aobus(DURATION_REG1_AND_STATUS);

}
/*
   DECODEMODE_NEC = 0,
   DECODEMODE_SKIPLEADER,
   DECODEMODE_SW,
   DECODEMODE_MITSUBISHI,
   DECODEMODE_THOMSON,
   DECODEMODE_TOSHIBA,
   DECODEMODE_SONYSIRC,
   DECODEMODE_RC5,
   DECODEMODE_RESERVED,
   DECODEMODE_RC6,
   DECODEMODE_RCMM,
   DECODEMODE_DUOKAN,
   DECODEMODE_RESERVED,
   DECODEMODE_RESERVED,
   DECODEMODE_COMCAST,
   DECODEMODE_SANYO,
   DECODEMODE_MAX*/

unsigned int COMCAST_DOMAIN(struct remote *remote_data, int domain)
{
	unsigned int data = 0;
	data = (remote_data->cur_lsbkeycode & 0xff);
	return data;
}
/*SANYO frame body
  Leader + 13bit Address + 13bit (~Address) + 8bit Data + 8bit (~Data)
 */
unsigned int SANYO_DOMAIN(struct remote *remote_data, int domain)
{
	unsigned int data = 0;
	if (domain) {
		remote_data->frame_mode = 0;
		data = ((remote_data->cur_lsbkeycode >> 8) & 0xff);
	} else {
		remote_data->frame_mode = 0;
		data = (((remote_data->cur_lsbkeycode >> 29) & 0x7) |
		((remote_data->cur_msbkeycode << 3) & 0x1fff));
	}
	return data;
}
/*


 */
unsigned int RCMM_DOMAIN(struct remote *remote_data, int domain)
{
	unsigned int data = 0;
	if (domain) {
		if (((remote_data->cur_lsbkeycode >> 12) & 0xfff)) {
			switch ((remote_data->cur_lsbkeycode >> 20) & 0xf) {
			case 0x0:
				remote_data->frame_mode = 0;
				data = (remote_data->cur_lsbkeycode & 0xff);
				return data;
			case 0x1:
				remote_data->frame_mode = 1;
				break;
			case 0x2:
				remote_data->frame_mode = 2;
				break;
			case 0x3:
				remote_data->frame_mode = 3;
				break;
			}
			data = (remote_data->cur_lsbkeycode & 0xfffff);
			return data;
		} else {
			switch ((remote_data->cur_lsbkeycode >> 10) & 0x3) {
			case 0x0:/*OEM mode*/
				remote_data->frame_mode = 0;
				data = (remote_data->cur_lsbkeycode & 0xff);
				return data;
			case 0x1:/*Extended Mouse mode*/
				remote_data->frame_mode = 1;
				break;
			case 0x2:/*Extended Keyboard mode*/
				remote_data->frame_mode = 2;
				break;
			case 0x3:/*Extended Game pad mode*/
				remote_data->frame_mode = 3;
				break;
			}
			data = (remote_data->cur_lsbkeycode & 0xff);
			return data;
		}
	} else {
		if (((remote_data->cur_lsbkeycode >> 12) & 0xfff)) {
			switch ((remote_data->cur_lsbkeycode >> 20) & 0xf) {
			case 0x0:
				remote_data->frame_mode = 0;
				data = ((remote_data->cur_lsbkeycode >> 12)
					& 0x3f);
				return data;
			case 0x1:
				remote_data->frame_mode = 1;
				break;
			case 0x2:
				remote_data->frame_mode = 2;
				break;
			case 0x3:
				remote_data->frame_mode = 3;
				break;
			}
			return 0;
		} else {
			switch ((remote_data->cur_lsbkeycode >> 20) & 0xf) {
			case 0x0:
				remote_data->frame_mode = 0;
			case 0x1:
				remote_data->frame_mode = 1;
				break;
			case 0x2:
				remote_data->frame_mode = 2;
				break;
			case 0x3:
				remote_data->frame_mode = 3;
				break;
			}
			data = ((remote_data->cur_lsbkeycode >> 8) & 0x3);
			return data;
		}
	}

}
/*
   8 bit address and 8 bit command length
   Address and command are transmitted twice for reliability
   Pulse distance modulation
   Carrier frequency of 38kHz
   Bit time of 1.125ms or 2.25ms
   NEC frame body
   C15 ~ C8      C7 ~ C0    D15~D8      D7~D0
   Header    ~Custom code   Custom code    Data Code ~Data Code
 */
unsigned int NEC_DOMAIN(struct remote *remote_data, int domain)
{
	unsigned int data = 0;
	if (domain)
		data = ((remote_data->cur_lsbkeycode >> 16) & 0xff);
		/*D15 ~ D8*/
	else
		data = ((remote_data->cur_lsbkeycode) & 0xffff);
	/*C7 ~ C0*/
	return data;
}
/*
   8 bit address and 8 bit command length
   Pulse distance modulation
   Carrier frequency of 38kHz
   Bit time of 1ms or 2ms
 */
unsigned int MITSUBISHI_DOMAIN(struct remote *remote_data, int domain)
{
	unsigned int data = 0;
	if (domain)
		data = (remote_data->cur_keycode & 0xff);
	else
		data = ((remote_data->cur_lsbkeycode >> 8) & 0xff);
	return data;
}
unsigned int TOSHIBA_DOMAIN(struct remote *remote_data, int domain)
{
	unsigned int data = 0;
	if (domain)
		data = ((remote_data->cur_lsbkeycode >> 16) & 0xff);
	else
		data = ((remote_data->cur_lsbkeycode) & 0xffff);
	return data;
}
/*
   Pulse width modulation
   Carrier frequency of 40kHz
   Bit time of 1.2ms or 0.6ms
   5-bit address and 7-bit command length (12-bit protocol)
 */

unsigned int SONYSIRC_DOMAIN(struct remote *remote_data, int domain)
{
	unsigned int data = 0;
	if (domain)
		data = ((remote_data->cur_lsbkeycode >> 5) & 0x7f);
	else
		data = (remote_data->cur_lsbkeycode & 0x1f);
	return data;
}
unsigned int RC5_DOMAIN(struct remote *remote_data, int domain)
{
	unsigned int data = 0;
	if (domain)
		data = ((remote_data->cur_lsbkeycode >> 5) & 0x7f);
	else
		data = (remote_data->cur_lsbkeycode & 0x1f);
	return data;

}

unsigned int RC6_DOMAIN(struct remote *remote_data, int domain)
{
	unsigned int data = 0;
	if (domain)
		data = ((remote_data->cur_lsbkeycode >> 5) & 0x7f);
	else
		data = (remote_data->cur_lsbkeycode & 0x1f);
	return data;

}

unsigned int RCA_DOMAIN(struct remote *remote_data, int domain)
{
	unsigned int data = 0;
	if (domain)
		data = ((remote_data->cur_lsbkeycode) & 0xff);
	else
		data = ((remote_data->cur_lsbkeycode >> 8) & 0xf);
	return data;

}
/*DUOKAN
  C7 ~ C4    C3~C0      D7 ~ D4    D3~D0      P3~P0
  Header   Custom code    Data Code      Parity Code Stop Bit  */

unsigned int DUOKAN_DOMAIN(struct remote *remote_data, int domain)
{
	unsigned int data = 0;
	if (remote_data->cur_lsbkeycode == 0x0003cccf) /* power key*/
		remote_data->cur_lsbkeycode =
		((remote_data->custom_code[0] & 0xff) << 12) | 0xa0;
	if (domain)
		data = ((remote_data->cur_lsbkeycode >> 4) & 0xff);
	else
		data = ((remote_data->cur_lsbkeycode >> 12) & 0xff);
	return data;
}
unsigned int KDB_NEC_DOMAIN(struct remote *remote_data, int domain)
{
	unsigned int data = 0;
	if (domain)
		data = ((remote_data->cur_lsbkeycode >> 4) & 0xff);
	else
		data = ((remote_data->cur_lsbkeycode >> 12) & 0xff);
	return data;
}
unsigned int KDB_DUOKAN_DOMAIN(struct remote *remote_data, int domain)
{
	unsigned int data = 0;
	if (domain)
		data = ((remote_data->cur_lsbkeycode >> 4) & 0xff);
	else
		data = ((remote_data->cur_lsbkeycode >> 12) & 0xff);
	return data;
}
unsigned int NULL_DUOKAN_DOMAIN(struct remote *remote_data, int domain)
{
	return 0;
}
unsigned int (*get_cur_key_domian[])(struct remote *remote_data, int domain) = {
	NEC_DOMAIN,
	DUOKAN_DOMAIN,
	KDB_NEC_DOMAIN,
	RCMM_DOMAIN,
	COMCAST_DOMAIN,
	MITSUBISHI_DOMAIN,
	SONYSIRC_DOMAIN,
	TOSHIBA_DOMAIN,
	RC5_DOMAIN,
	RC6_DOMAIN,
	NULL_DUOKAN_DOMAIN,
	RCA_DOMAIN,
	NULL_DUOKAN_DOMAIN,
	NULL_DUOKAN_DOMAIN,
	NULL_DUOKAN_DOMAIN,
	NULL_DUOKAN_DOMAIN,
	NULL_DUOKAN_DOMAIN,
	NULL_DUOKAN_DOMAIN,
	KDB_DUOKAN_DOMAIN
};

int remote_hw_reprot_null_key(struct remote *remote_data)
{
	input_dbg("%s,it is a null key\n", __func__);
	get_cur_scancode(remote_data);
	get_cur_scanstatus(remote_data);
	return 0;
}

int remote_hw_reprot_key(struct remote *remote_data)
{
	static int last_scan_code;
	int i;
	get_cur_scancode(remote_data);
	get_cur_scanstatus(remote_data);
	if (remote_data->status)
		return 0;
	if (remote_data->cur_lsbkeycode) {  /*key first press*/
		if (remote_data->ig_custom_enable) {
			for (i = 0; i < ARRAY_SIZE(remote_data->custom_code);) {
				if (remote_data->custom_code[i] !=
				get_cur_key_domian[remote_data->work_mode]
				(remote_data, CUSTOMDOMAIN))
					i++;
				else {
					remote_data->map_num = i;
					break;
				}
				if (i == ARRAY_SIZE(remote_data->custom_code)) {
					input_dbg("Wrong custom code is 0x%08x\n",
						remote_data->cur_lsbkeycode);
					return -1;
				}
			}
		}
		repeat_count = 0;
		if (remote_data->timer.expires > msecs_to_jiffies(0) + jiffies)
			remote_data->remote_send_key(remote_data->input,
				remote_data->repeat_release_code , 0, 0);

		remote_data->remote_send_key(remote_data->input,
		get_cur_key_domian[remote_data->work_mode](remote_data,
		KEYDOMIAN), 1, 0);
		remote_data->repeat_release_code =
		get_cur_key_domian[remote_data->work_mode]
		(remote_data, KEYDOMIAN);
		remote_data->enable_repeat_falg = 1;
		if ((remote_data->work_mode > DECODEMODE_NEC)
			&& remote_data->enable_repeat_falg) {
			if (remote_data->repeat_enable) {
				remote_data->repeat_timer.data =
				(unsigned long)remote_data;
	/*here repeat  delay is time
	interval from the first frame end to first repeat end.*/
				remote_data->repeat_tick = jiffies;
				mod_timer(&remote_data->repeat_timer, jiffies +
				msecs_to_jiffies(remote_data->repeat_delay
				[remote_data->map_num]));
				remote_data->status = TIMER;
			} else {
				setup_timer(&remote_data->rel_timer,
					remote_rel_timer_sr, 0);
				mod_timer(&remote_data->timer,  jiffies);
				remote_data->rel_timer.data =
				(unsigned long)remote_data;
				mod_timer(&remote_data->rel_timer,
				jiffies +
			msecs_to_jiffies(remote_data->relt_delay
			[remote_data->map_num]));
				remote_data->status = TIMER;
			}
		}
		for (i = 0; i < ARRAY_SIZE(remote_data->key_repeat_map
		[remote_data->map_num]); i++) {
			if (remote_data->key_repeat_map
				[remote_data->map_num][i] ==
				remote_data->repeat_release_code)
				remote_data->want_repeat_enable = 1;
			else
				remote_data->want_repeat_enable = 0;
		}

		if (remote_data->repeat_enable &&
			remote_data->want_repeat_enable)
			remote_data->repeat_tick = jiffies +
			msecs_to_jiffies(remote_data->repeat_delay
				[remote_data->map_num]);
		if (remote_data->repeat_enable)
			mod_timer(&remote_data->timer,
			jiffies + msecs_to_jiffies(remote_data->release_delay
			[remote_data->map_num] +
			remote_data->repeat_delay[remote_data->map_num]));
		else
			mod_timer(&remote_data->timer,
			jiffies + msecs_to_jiffies
			(remote_data->release_delay[remote_data->map_num] +
			remote_data->repeat_delay[remote_data->map_num]));
	} else if ((remote_data->frame_status & REPEARTFLAG) &&
		remote_data->enable_repeat_falg) { /*repeate key*/
		if (remote_data->repeat_enable) {
			repeat_count++;
			if (remote_data->repeat_tick < msecs_to_jiffies(0)
				+ jiffies) {
				if (repeat_count > 1)
					remote_data->remote_send_key(
				remote_data->input,
				remote_data->repeat_release_code, 2, 0);
				remote_data->repeat_tick +=
				msecs_to_jiffies(
				remote_data->repeat_peroid
				[remote_data->map_num]);
			}
		} else {
			if (remote_data->timer.expires >
				 msecs_to_jiffies(0) + jiffies)
				mod_timer(&remote_data->timer,
				jiffies + msecs_to_jiffies
		(remote_data->release_delay[remote_data->map_num]));
			return -1;
		}
		mod_timer(&remote_data->timer,
		jiffies + msecs_to_jiffies
		(remote_data->release_delay[remote_data->map_num])
		+ msecs_to_jiffies(110));
	}
	last_scan_code = remote_data->cur_lsbkeycode;
	remote_data->cur_keycode = last_scan_code;
	remote_data->cur_lsbkeycode = 0;
	remote_data->timer.data = (unsigned long)remote_data;
	return 0;
}
int remote_hw_nec_rca_2in1_reprot_key(struct remote *remote_data)
{
	static int last_scan_code;
	int i;
	get_cur_scancode(remote_data);
	get_cur_scanstatus(remote_data);
	if (remote_data->status)/*repeat enable & come in S timer is open*/
	return 0;
	if (remote_data->cur_lsbkeycode) {
		/*key first press*/
		if (remote_data->ig_custom_enable) {
			for (i = 0; i < ARRAY_SIZE(remote_data->custom_code);) {
				if (remote_data->custom_code[i] !=
				get_cur_key_domian[remote_data->temp_work_mode]
				(remote_data, CUSTOMDOMAIN))
					i++;
				else {
					remote_data->map_num = i;
					break;
				}
				if (i == ARRAY_SIZE(remote_data->custom_code)) {
					input_dbg
			("Wrong custom code is 0x%08x,temp_work_mode is %d\n",
					remote_data->cur_lsbkeycode,
					remote_data->temp_work_mode);
					return -1;
				}
		}
		}
		repeat_count = 0;
		if (remote_data->timer.expires > msecs_to_jiffies(0)
			+jiffies)
			remote_data->remote_send_key(remote_data->input,
			remote_data->repeat_release_code , 0, 0);
			remote_data->remote_send_key(remote_data->input,
			get_cur_key_domian[remote_data->temp_work_mode]
			(remote_data, KEYDOMIAN), 1, 0);
			remote_data->repeat_release_code =
			get_cur_key_domian[remote_data->temp_work_mode]
			(remote_data, KEYDOMIAN);
			remote_data->enable_repeat_falg = 1;
		if ((remote_data->temp_work_mode  == DECODEMODE_RCA)
			&& (remote_data->enable_repeat_falg)) {
			if (remote_data->repeat_enable) {
				remote_data->repeat_timer.data =
				(unsigned long)remote_data;
/*here repeat  delay is time interval
	from the first frame end to first repeat end.*/
				remote_data->repeat_tick = jiffies;
				mod_timer(&remote_data->repeat_timer,
				jiffies +
				msecs_to_jiffies(
				remote_data->repeat_delay
				[remote_data->map_num]));
				remote_data->status = TIMER;
			} else {
				setup_timer(&remote_data->rel_timer,
				remote_rel_timer_sr, 0);
				mod_timer(&remote_data->timer,
				jiffies);
				remote_data->rel_timer.data =
					(unsigned long)remote_data;
				mod_timer(&remote_data->rel_timer,
				jiffies + msecs_to_jiffies
				(remote_data->relt_delay
				[remote_data->map_num]));
				remote_data->status = TIMER;
			}
		}
		for (i = 0; i < ARRAY_SIZE(remote_data->key_repeat_map
			[remote_data->map_num]); i++) {
			if (remote_data->key_repeat_map
				[remote_data->map_num][i] ==
				remote_data->repeat_release_code)
				remote_data->want_repeat_enable = 1;
			else
				remote_data->want_repeat_enable = 0;
			}

		if (remote_data->repeat_enable &&
			remote_data->want_repeat_enable)
			remote_data->repeat_tick = jiffies +
		msecs_to_jiffies(remote_data->repeat_delay
			[remote_data->map_num]);
		if (remote_data->repeat_enable)
			mod_timer(&remote_data->timer,
			jiffies + msecs_to_jiffies(
			remote_data->release_delay[remote_data->map_num] +
			remote_data->repeat_delay[remote_data->map_num]));
		else {
			mod_timer(&remote_data->timer,
			jiffies + msecs_to_jiffies
			(remote_data->release_delay[remote_data->map_num] +
			remote_data->repeat_delay[remote_data->map_num]));
		}
	} else if ((remote_data->frame_status & REPEARTFLAG)
		&& remote_data->enable_repeat_falg) { /*repeate key*/
		if (remote_data->repeat_enable) {
			repeat_count++;
			if (remote_data->repeat_tick < msecs_to_jiffies(0)
				+jiffies) {
				if (repeat_count > 1)
					remote_data->remote_send_key
					(remote_data->input,
				remote_data->repeat_release_code, 2, 0);
				remote_data->repeat_tick +=
				msecs_to_jiffies
				(remote_data->repeat_peroid
				[remote_data->map_num]);
			}
		} else{
			if (remote_data->timer.expires > msecs_to_jiffies(0)
				+ jiffies)
				mod_timer(&remote_data->timer,
			jiffies + msecs_to_jiffies
			(remote_data->release_delay[remote_data->map_num]));
			return -1;
		}
		mod_timer(&remote_data->timer,
		jiffies +
		msecs_to_jiffies(remote_data->release_delay
		[remote_data->map_num])
		+ msecs_to_jiffies(110));
	}
	last_scan_code = remote_data->cur_lsbkeycode;
	remote_data->cur_keycode = last_scan_code;
	remote_data->cur_lsbkeycode = 0;
	remote_data->timer.data = (unsigned long)remote_data;
	return 0;
}

int remote_hw_nec_toshiba_2in1_reprot_key(struct remote *remote_data)
{
	static int last_scan_code;
	int i;
	get_cur_scancode(remote_data);
	get_cur_scanstatus(remote_data);
	if (remote_data->cur_lsbkeycode) {/*key first press*/
		if (remote_data->ig_custom_enable) {
			for (i = 0; i < ARRAY_SIZE(remote_data->custom_code);) {
				if (remote_data->custom_code[i] !=
				get_cur_key_domian[remote_data->temp_work_mode]
				(remote_data, CUSTOMDOMAIN))
					i++;
				else {
					remote_data->map_num = i;
					break;
				}

				if (i == ARRAY_SIZE(remote_data->custom_code)) {
					input_dbg
			("Wrong custom code is 0x%08x,temp_work_mode is %d\n",
				remote_data->cur_lsbkeycode,
				remote_data->temp_work_mode);
					return -1;
				}
			}
		}
		repeat_count = 0;
		if (remote_data->timer.expires > msecs_to_jiffies(0) + jiffies)
			remote_data->remote_send_key(remote_data->input,
		remote_data->repeat_release_code , 0, 0);
		remote_data->remote_send_key(remote_data->input,
		get_cur_key_domian[remote_data->temp_work_mode]
		(remote_data, KEYDOMIAN), 1, 0);
		remote_data->repeat_release_code =
		get_cur_key_domian[remote_data->temp_work_mode]
		(remote_data, KEYDOMIAN);
		remote_data->enable_repeat_falg = 1;
		if (remote_data->temp_work_mode  == DECODEMODE_TOSHIBA)
			/*setting frame bit = 1;*/
			aml_write_aobus(OPERATION_CTRL_REG1, 0x8000);
		for (i = 0; i < ARRAY_SIZE(remote_data->key_repeat_map
			[remote_data->map_num]);
			i++) {
			if (remote_data->key_repeat_map
			[remote_data->map_num][i] ==
			remote_data->repeat_release_code)
				remote_data->want_repeat_enable = 1;
			else
				remote_data->want_repeat_enable = 0;
		}

		if (remote_data->repeat_enable &&
				remote_data->want_repeat_enable)
			remote_data->repeat_tick = jiffies +
			msecs_to_jiffies(remote_data->repeat_delay
				[remote_data->map_num]);
		if (remote_data->repeat_enable)
			mod_timer(&remote_data->timer,
			jiffies + msecs_to_jiffies
			(remote_data->release_delay[remote_data->map_num]
			+ remote_data->repeat_delay[remote_data->map_num]));
		else
			mod_timer(&remote_data->timer,
			jiffies + msecs_to_jiffies
			(remote_data->release_delay[remote_data->map_num]
			+ remote_data->repeat_delay[remote_data->map_num]));
	} else if ((remote_data->frame_status & REPEARTFLAG) &&
		remote_data->enable_repeat_falg) {
		if (remote_data->repeat_enable) {
			repeat_count++;
			if (remote_data->repeat_tick < msecs_to_jiffies(0)
				+ jiffies) {
				if (repeat_count > 1)
					remote_data->remote_send_key
					(remote_data->input,
				remote_data->repeat_release_code, 2, 0);
					remote_data->repeat_tick +=
				msecs_to_jiffies
			(remote_data->repeat_peroid[remote_data->map_num]);
			}
		} else {
			if (remote_data->timer.expires > msecs_to_jiffies(0)
				+ jiffies)
				mod_timer(&remote_data->timer,
			jiffies + msecs_to_jiffies
		(remote_data->release_delay[remote_data->map_num]));
			return -1;
		}
		mod_timer(&remote_data->timer,
		jiffies + msecs_to_jiffies
		(remote_data->release_delay[remote_data->map_num])
		+ msecs_to_jiffies(110));
	}
	last_scan_code = remote_data->cur_lsbkeycode;
	remote_data->cur_keycode = last_scan_code;
	remote_data->cur_lsbkeycode = 0;
	remote_data->timer.data = (unsigned long)remote_data;
	return 0;
}
static inline void kbd_software_mode_remote_send_key(unsigned long data)
{
	struct remote *remote_data = (struct remote *)data;
	int i;
	get_cur_scancode(remote_data);
	remote_data->step = REMOTE_STATUS_SYNC;
	if (remote_data->repeate_flag) {
		if ((remote_data->repeat_tick <
			msecs_to_jiffies(0) + jiffies) &&
		(remote_data->enable_repeat_falg == 1)) {
			remote_data->remote_send_key(remote_data->input,
			remote_data->repeat_release_code, 2, 0);
			remote_data->repeat_tick +=
			msecs_to_jiffies(remote_data->input->rep[REP_PERIOD]);
		}
	} else {
		if (remote_data->ig_custom_enable) {
			for (i = 0; i < ARRAY_SIZE(remote_data->custom_code);) {
				if (remote_data->custom_code[i] !=
			get_cur_key_domian[remote_data->work_mode]
				(remote_data, CUSTOMDOMAIN))
					i++;
				else {
					remote_data->map_num = i;
					break;
				}
				if (i == ARRAY_SIZE(remote_data->custom_code)) {
					input_dbg("Wrong custom code is 0x%08x\n",
					remote_data->cur_lsbkeycode);
					return;
				}
			}
		}
		remote_data->remote_send_key(remote_data->input,
		get_cur_key_domian[remote_data->work_mode]
			(remote_data, KEYDOMIAN), 1, 0);
		remote_data->repeat_release_code =
		get_cur_key_domian[remote_data->work_mode]
			(remote_data, KEYDOMIAN);
		remote_data->enable_repeat_falg = 1;
		for (i = 0; i < ARRAY_SIZE
			(remote_data->key_repeat_map[remote_data->map_num]);
				i++) {
			if (remote_data->key_repeat_map
					[remote_data->map_num][i] ==
				remote_data->repeat_release_code)
				remote_data->want_repeat_enable = 1;
			else
				remote_data->want_repeat_enable = 0;
		}
		if (remote_data->repeat_enable &&
			remote_data->want_repeat_enable)
			remote_data->repeat_tick = jiffies +
			msecs_to_jiffies(remote_data->input->rep[REP_DELAY]);
	}
}
static void remote_rca_repeat_sr(unsigned long data)
{
	struct remote *remote_data = (struct remote *)data;
	if (remote_data->cur_keycode == remote_data->cur_lsbkeycode) {
		repeat_count++;
		if (repeat_count > 2)
			remote_data->remote_send_key(remote_data->input,
			remote_data->repeat_release_code, 2, 0);
		remote_data->cur_lsbkeycode = 0;
		remote_data->repeat_timer.data = (unsigned long)remote_data;
		remote_data->timer.data = (unsigned long)remote_data;
		mod_timer(&remote_data->timer, jiffies +
		msecs_to_jiffies(remote_data->release_delay
		[remote_data->map_num] +
		remote_data->repeat_peroid[remote_data->map_num]));
		mod_timer(&remote_data->repeat_timer,  jiffies +
		msecs_to_jiffies(remote_data->repeat_peroid
		[remote_data->map_num]));
		remote_data->status = TIMER;
	} else {
		remote_data->status = NORMAL;
		remote_data->timer.data = (unsigned long)remote_data;
		mod_timer(&remote_data->timer, jiffies + msecs_to_jiffies(1));
	}
}
static void remote_repeat_sr(unsigned long data)
{
	struct remote *remote_data = (struct remote *)data;
	if (remote_data->cur_keycode == remote_data->cur_lsbkeycode) {
		auto_repeat_count++;
		if (auto_repeat_count > 1)
			remote_data->remote_send_key(remote_data->input,
			remote_data->repeat_release_code, 2, 0);
		remote_data->cur_lsbkeycode = 0;
		remote_data->repeat_timer.data = (unsigned long)remote_data;
		remote_data->timer.data = (unsigned long)remote_data;
		mod_timer(&remote_data->repeat_timer,
		jiffies + msecs_to_jiffies
		(remote_data->repeat_peroid[remote_data->map_num]));
		remote_data->status = TIMER;
	} else {
		remote_data->status = NORMAL;
		remote_data->timer.data = (unsigned long)remote_data;
		mod_timer(&remote_data->timer, jiffies + msecs_to_jiffies(1));
	}
}
static void remote_rel_timer_sr(unsigned long data)
{
	struct remote *remote_data = (struct remote *)data;
	if (remote_data->cur_keycode == remote_data->cur_lsbkeycode) {
		remote_data->cur_lsbkeycode = 0;
		remote_data->rel_timer.data = (unsigned long)remote_data;
		mod_timer(&remote_data->rel_timer,
		jiffies + msecs_to_jiffies
		(remote_data->relt_delay[remote_data->map_num]));
		remote_data->status = TIMER;
	} else
		remote_data->status = NORMAL;
}
int remote_bridge_isr(struct remote *remote_data)
{

	if (remote_data->send_data) {
		kbd_software_mode_remote_send_key((unsigned long)remote_data);
		remote_data->send_data = 0;
	}
	remote_data->timer.data = (unsigned long)remote_data;
	mod_timer(&remote_data->timer,
	jiffies + msecs_to_jiffies
		(remote_data->release_delay[remote_data->map_num]));
	return 0;
}
static int get_pulse_width(struct remote *remote_data)
{
	unsigned int pulse_width;
	const char *state;

	pulse_width = (aml_read_aobus(OPERATION_CTRL_REG1) & 0x1FFF0000) >> 16;
	state = remote_data->step == REMOTE_STATUS_WAIT ? "wait" :
	       remote_data->step == REMOTE_STATUS_LEADER ? "leader" :
	       remote_data->step == REMOTE_STATUS_DATA ? "data" :
	       remote_data->step == REMOTE_STATUS_SYNC ? "sync" : NULL;
	dbg_printk("%02d:pulse_wdith:%d==>%s\r\n",
		remote_data->bit_count - remote_data->bit_num,
			pulse_width, state);
	if (pulse_width == 0) {
		switch (remote_data->step) {
		case REMOTE_STATUS_LEADER:
			pulse_width = remote_data->time_window[0] + 1;
			break;
		case REMOTE_STATUS_DATA:
			pulse_width = remote_data->time_window[2] + 1;
			break;
		}
	}
	return pulse_width;
}

static inline void kbd_software_mode_remote_wait(struct remote *remote_data)
{
	remote_data->step = REMOTE_STATUS_LEADER;
	remote_data->cur_keycode = 0;
	remote_data->bit_num = remote_data->bit_count;
}

static inline void kbd_software_mode_remote_leader(struct remote *remote_data)
{
	unsigned int pulse_width;
	pulse_width = get_pulse_width(remote_data);
	if ((pulse_width > remote_data->time_window[0])
	   && (pulse_width < remote_data->time_window[1]))
		remote_data->step = REMOTE_STATUS_DATA;
	else
		remote_data->step = REMOTE_STATUS_WAIT;

	remote_data->cur_keycode = 0;
	remote_data->bit_num = remote_data->bit_count;
}


static inline void kbd_software_mode_remote_data(struct remote *remote_data)
{
	unsigned int pulse_width;

	pulse_width = get_pulse_width(remote_data);
	remote_data->step = REMOTE_STATUS_DATA;
	switch (remote_data->work_mode) {
	case DECODEMODE_SW_NEC:
		if ((pulse_width > remote_data->time_window[2])
		&& (pulse_width < remote_data->time_window[3]))
			remote_data->bit_num--;
		else if ((pulse_width > remote_data->time_window[4])
		&& (pulse_width < remote_data->time_window[5])) {
			remote_data->bit_num--;
			remote_data->cur_keycode |=
		1 << (remote_data->bit_count - remote_data->bit_num);
		} else
			remote_data->step = REMOTE_STATUS_WAIT;
		if (remote_data->bit_num == 0) {
			remote_data->repeate_flag = 0;
			remote_data->send_data = 1;
			remote_bridge_isr(remote_data);
		}
		break;
	case DECODEMODE_SW_DUOKAN:
		if ((pulse_width > remote_data->time_window[2])
		&& (pulse_width < remote_data->time_window[3]))
			remote_data->bit_num -= 2;
		else if ((pulse_width > remote_data->time_window[4])
		&& (pulse_width < remote_data->time_window[5])) {
			remote_data->cur_keycode |=
			1 << (remote_data->bit_count - remote_data->bit_num);
			remote_data->bit_num -= 2;
		} else if ((pulse_width > remote_data->time_window[8])
		&& (pulse_width < remote_data->time_window[9])) {
			remote_data->cur_keycode |=
			2 << (remote_data->bit_count - remote_data->bit_num);
			remote_data->bit_num -= 2;
		} else if ((pulse_width > remote_data->time_window[10])
		&& (pulse_width < remote_data->time_window[11])) {
			remote_data->cur_keycode |=
			3 << (remote_data->bit_count - remote_data->bit_num);
			remote_data->bit_num -= 2;
		} else
			remote_data->step = REMOTE_STATUS_WAIT;
		if (remote_data->bit_num == 0) {
			remote_data->repeate_flag = 0;
			remote_data->send_data = 1;
			remote_bridge_isr(remote_data);

		}
		break;
	}
}

static inline void kbd_software_mode_remote_sync(struct remote *remote_data)
{
	unsigned int pulse_width;
	pulse_width = get_pulse_width(remote_data);
	if ((pulse_width > remote_data->time_window[6])
	   && (pulse_width < remote_data->time_window[7])) {
		remote_data->repeate_flag = 1;
		if (remote_data->repeat_enable)
			remote_data->send_data = 1;
		else {
			remote_data->step = REMOTE_STATUS_SYNC;
			return;
		}
	}
	remote_data->step = REMOTE_STATUS_SYNC;
	remote_bridge_isr(remote_data);
}

int remote_sw_reprot_key(struct remote *remote_data)
{
	int current_jiffies = jiffies;

	if (((current_jiffies - remote_data->last_jiffies) > 20)
	   && (remote_data->step <= REMOTE_STATUS_SYNC))
		remote_data->step = REMOTE_STATUS_WAIT;
	remote_data->last_jiffies = current_jiffies;
	switch (remote_data->step) {
	case REMOTE_STATUS_WAIT:
		kbd_software_mode_remote_wait(remote_data);
		break;
	case REMOTE_STATUS_LEADER:
		kbd_software_mode_remote_leader(remote_data);
		break;
	case REMOTE_STATUS_DATA:
		kbd_software_mode_remote_data(remote_data);
		break;
	case REMOTE_STATUS_SYNC:
		kbd_software_mode_remote_sync(remote_data);
		break;
	default:
		break;
	}
	return 0;
}


void kdb_send_key(struct input_dev *dev, unsigned int scancode,
		unsigned int type, int event)
{
	return;
}
void remote_nec_report_release_key(struct remote *remote_data)
{
	if (remote_data->enable_repeat_falg) {
		remote_data->remote_send_key(remote_data->input,
			remote_data->repeat_release_code, 0, 0);
		remote_data->enable_repeat_falg = 0;
	}
}
void remote_duokan_report_release_key(struct remote *remote_data)
{
	if (remote_data->enable_repeat_falg) {
		remote_data->remote_send_key(remote_data->input,
			remote_data->repeat_release_code, 0, 0);
		remote_data->enable_repeat_falg = 0;
		auto_repeat_count = 0;
	}
}
void remote_sw_reprot_release_key(struct remote *remote_data)
{
	if (remote_data->enable_repeat_falg) {
		remote_data->remote_send_key(remote_data->input,
			remote_data->repeat_release_code, 0, 0);
		remote_data->enable_repeat_falg = 0;
	}
}
void remote_nec_rca_2in1_report_release_key(struct remote *remote_data)
{
	if (remote_data->enable_repeat_falg) {
		remote_data->remote_send_key(remote_data->input,
			remote_data->repeat_release_code, 0, 0);
		remote_data->enable_repeat_falg = 0;
	}
}
void remote_nec_toshiba_2in1_report_release_key(struct remote *remote_data)
{
	if (remote_data->enable_repeat_falg) {
		remote_data->remote_send_key(remote_data->input,
			remote_data->repeat_release_code, 0, 0);
		remote_data->enable_repeat_falg = 0;
		aml_write_aobus(OPERATION_CTRL_REG1, 0x9f40);
	}
}

void remote_null_reprot_release_key(struct remote *remote_data)
{

}

