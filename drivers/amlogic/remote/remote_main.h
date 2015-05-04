/*
 * drivers/amlogic/remote/remote_main.h
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

#ifndef _REMOTE_H
#define _REMOTE_H
#include <asm/ioctl.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/io.h>
#include <linux/amlogic/iomap.h>
#define AO_RTI_PIN_MUX_REG (0x14)
#define AO_RTI_STATUS_REG2        (((0x00 << 8) | (0x02))<<2)
/*remote register*/

#define LDR_ACTIVE  ((0x01 << 10) | (0x60 << 2))

#define LDR_IDLE  ((0x01 << 10) | (0x61 << 2))

#define  LDR_REPEAT ((0x01 << 10) | (0x62 << 2))

#define  DURATION_REG0  ((0x01 << 10) | (0x63 << 2))

#define   OPERATION_CTRL_REG0 ((0x01 << 10) | (0x64 << 2))

#define  FRAME_BODY ((0x01 << 10) | (0x65 << 2))

#define  DURATION_REG1_AND_STATUS ((0x01 << 10) | (0x66 << 2))

#define  OPERATION_CTRL_REG1 ((0x01 << 10) | (0x67 << 2))

#define   OPERATION_CTRL_REG2 ((0x01 << 10) | (0x68 << 2))

#define   DURATION_REG2 ((0x01 << 10) | (0x69 << 2))

#define  DURATION_REG3 ((0x01 << 10) | (0x6a << 2))

#define FRAME_BODY1  ((0x01 << 10) | (0x6b << 2))
#define CONFIG_END  ((0x01 << 10) | (0x6d << 2))



extern char *remote_log_buf;
/*config remote register val*/
struct reg_s {
	int reg;
	unsigned int val;
};
enum {
	NORMAL = 0,
	TIMER = 1 ,
};

/*
   Decode_mode.(format selection)
   0x0 =NEC
   0x1= skip leader (just bits)
   0x2=measure width (software decode)
   0x3=MITSUBISHI
   0x4=Thomson
   0x5=Toshiba
   0x6=Sony SIRC
   0x7=RC5
   0x8=Reserved
   0x9=RC6
   0xA=RCMM
   0xB=Duokan
   0xC=Reserved
   0xD=Reserved
   0xE=Comcast
   0xF=Sanyo
 */
enum {
	DECODEMODE_NEC = 0,
	DECODEMODE_DUOKAN = 1 ,
	DECODEMODE_RCMM ,
	DECODEMODE_SONYSIRC,
	DECODEMODE_SKIPLEADER ,
	DECODEMODE_MITSUBISHI,
	DECODEMODE_THOMSON,
	DECODEMODE_TOSHIBA,
	DECODEMODE_RC5,
	DECODEMODE_RESERVED,
	DECODEMODE_RC6,
	DECODEMODE_RCA,
	DECODEMODE_COMCAST,
	DECODEMODE_SANYO,
	DECODEMODE_NEC_RCA_2IN1 = 14,
	DECODEMODE_NEC_TOSHIBA_2IN1 = 15,
	DECODEMODE_SW,
	DECODEMODE_MAX ,
	DECODEMODE_SW_NEC,
	DECODEMODE_SW_DUOKAN

};


#define REMOTE_IOC_INFCODE_CONFIG       _IOW_BAD('I', 13 , sizeof(short))
#define REMOTE_IOC_RESET_KEY_MAPPING        _IOW_BAD('I', 3 , sizeof(short))
#define REMOTE_IOC_SET_KEY_MAPPING          _IOW_BAD('I' , 4 , sizeof(short))
#define REMOTE_IOC_SET_REPEAT_KEY_MAPPING   _IOW_BAD('I' , 20 , sizeof(short))
#define REMOTE_IOC_SET_MOUSE_MAPPING        _IOW_BAD('I' , 5 , sizeof(short))
#define REMOTE_IOC_SET_REPEAT_DELAY         _IOW_BAD('I' , 6 , sizeof(short))
#define REMOTE_IOC_SET_REPEAT_PERIOD        _IOW_BAD('I' , 7 , sizeof(short))

#define REMOTE_IOC_SET_REPEAT_ENABLE        _IOW_BAD('I' , 8 , sizeof(short))
#define REMOTE_IOC_SET_DEBUG_ENABLE         _IOW_BAD('I' , 9 , sizeof(short))
#define REMOTE_IOC_SET_MODE                 _IOW_BAD('I' , 10 , sizeof(short))

#define REMOTE_IOC_SET_CUSTOMCODE       _IOW_BAD('I' , 100 , sizeof(short))
#define REMOTE_IOC_SET_RELEASE_DELAY        _IOW_BAD('I' , 99 , sizeof(short))

#define REMOTE_IOC_SET_REG_BASE_GEN         _IOW_BAD('I' , 101 , sizeof(short))
#define REMOTE_IOC_SET_REG_CONTROL          _IOW_BAD('I' , 102 , sizeof(short))
#define REMOTE_IOC_SET_REG_LEADER_ACT       _IOW_BAD('I' , 103 , sizeof(short))
#define REMOTE_IOC_SET_REG_LEADER_IDLE      _IOW_BAD('I' , 104 , sizeof(short))
#define REMOTE_IOC_SET_REG_REPEAT_LEADER    _IOW_BAD('I' , 105 , sizeof(short))
#define REMOTE_IOC_SET_REG_BIT0_TIME         _IOW_BAD('I' , 106 , sizeof(short))

#define REMOTE_IOC_SET_BIT_COUNT            _IOW_BAD('I' , 107 , sizeof(short))
#define REMOTE_IOC_SET_TW_LEADER_ACT        _IOW_BAD('I' , 108 , sizeof(short))
#define REMOTE_IOC_SET_TW_BIT0_TIME         _IOW_BAD('I' , 109 , sizeof(short))
#define REMOTE_IOC_SET_TW_BIT1_TIME         _IOW_BAD('I' , 110 , sizeof(short))
#define REMOTE_IOC_SET_TW_REPEATE_LEADER    _IOW_BAD('I' , 111 , sizeof(short))

#define REMOTE_IOC_GET_TW_LEADER_ACT        _IOR_BAD('I' , 112 , sizeof(short))
#define REMOTE_IOC_GET_TW_BIT0_TIME         _IOR_BAD('I' , 113 , sizeof(short))
#define REMOTE_IOC_GET_TW_BIT1_TIME         _IOR_BAD('I' , 114 , sizeof(short))
#define REMOTE_IOC_GET_TW_REPEATE_LEADER    _IOR_BAD('I' , 115 , sizeof(short))

#define REMOTE_IOC_GET_REG_BASE_GEN         _IOR_BAD('I' , 121 , sizeof(short))
#define REMOTE_IOC_GET_REG_CONTROL          _IOR_BAD('I' , 122 , sizeof(short))
#define REMOTE_IOC_GET_REG_LEADER_ACT       _IOR_BAD('I' , 123 , sizeof(short))
#define REMOTE_IOC_GET_REG_LEADER_IDLE      _IOR_BAD('I' , 124 , sizeof(short))
#define REMOTE_IOC_GET_REG_REPEAT_LEADER    _IOR_BAD('I' , 125 , sizeof(short))
#define REMOTE_IOC_GET_REG_BIT0_TIME        _IOR_BAD('I' , 126 , sizeof(short))
#define REMOTE_IOC_GET_REG_FRAME_DATA       _IOR_BAD('I' , 127 , sizeof(short))
#define REMOTE_IOC_GET_REG_FRAME_STATUS     _IOR_BAD('I' , 128 , sizeof(short))

#define REMOTE_IOC_SET_TW_BIT2_TIME         _IOW_BAD('I' , 129 , sizeof(short))
#define REMOTE_IOC_SET_TW_BIT3_TIME         _IOW_BAD('I' , 130 , sizeof(short))

#define REMOTE_IOC_SET_FN_KEY_SCANCODE     _IOW_BAD('I' , 131 , sizeof(short))
#define REMOTE_IOC_SET_LEFT_KEY_SCANCODE   _IOW_BAD('I' , 132 , sizeof(short))
#define REMOTE_IOC_SET_RIGHT_KEY_SCANCODE  _IOW_BAD('I' , 133 , sizeof(short))
#define REMOTE_IOC_SET_UP_KEY_SCANCODE     _IOW_BAD('I' , 134 , sizeof(short))
#define REMOTE_IOC_SET_DOWN_KEY_SCANCODE   _IOW_BAD('I' , 135 , sizeof(short))
#define REMOTE_IOC_SET_OK_KEY_SCANCODE     _IOW_BAD('I' , 136 , sizeof(short))
#define REMOTE_IOC_SET_PAGEUP_KEY_SCANCODE _IOW_BAD('I' , 137 , sizeof(short))
#define REMOTE_IOC_SET_PAGEDOWN_KEY_SCANCODE _IOW_BAD('I' , 138 , sizeof(short))
#define REMOTE_IOC_SET_RELT_DELAY     _IOW_BAD('I' , 140 , sizeof(short))

#define REMOTE_HW_DECODER_STATUS_MASK       (0xf<<4)
#define REMOTE_HW_DECODER_STATUS_OK         (0<<4)
#define REMOTE_HW_DECODER_STATUS_TIMEOUT    (1<<4)
#define REMOTE_HW_DECODER_STATUS_LEADERERR  (2<<4)
#define REMOTE_HW_DECODER_STATUS_REPEATERR  (3<<4)

/* software  decode status*/
#define REMOTE_STATUS_WAIT       0
#define REMOTE_STATUS_LEADER     1
#define REMOTE_STATUS_DATA       2
#define REMOTE_STATUS_SYNC       3

#define REPEARTFLAG 0x1
#define KEYDOMIAN 1
#define CUSTOMDOMAIN 0
/*phy page user debug*/
#define REMOTE_LOG_BUF_LEN       4098
#define REMOTE_LOG_BUF_ORDER        1


typedef int (*type_printk)(const char *fmt, ...);
/* this is a message of IR input device,include release timer repeat timer*/
/*
 */
struct remote {
	struct input_dev *input;
	struct timer_list timer;
	struct timer_list repeat_timer;
	struct timer_list rel_timer;
	unsigned long repeat_tick;
	void __iomem *mux_ioaddr;
	void __iomem *ioaddr;
	int irq;
	int save_mode;
	int work_mode;
	int temp_work_mode;
	int frame_mode;
	unsigned int register_data;
	unsigned int frame_status;
	unsigned int cur_keycode;
	unsigned int cur_lsbkeycode;
	unsigned int cur_msbkeycode;
	unsigned int repeat_release_code;
	unsigned int last_keycode;
	unsigned int repeate_flag;
	unsigned int repeat_enable;
	unsigned int debounce;
	unsigned int status;
	int map_num;
	struct reg_s *reg;
	unsigned int reg_size;
	int ig_custom_enable;
	int enable_repeat_falg;
	unsigned int custom_code[20];
	unsigned int release_fdelay;
	unsigned int release_sdelay;
	unsigned int release_delay[20];
	unsigned int debug_enable;
	unsigned int sleep;
	unsigned int delay;
	unsigned int step;
	unsigned int send_data;
	int want_repeat_enable;
	unsigned int key_repeat_map[20][256];
	unsigned int bit_count;
	unsigned int bit_num;
	unsigned int last_jiffies;
	unsigned int time_window[18];
	int last_pulse_width;
	int repeat_time_count;
	int config_major;
	char config_name[20];
	struct class *config_class;
	struct device *config_dev;
	unsigned int repeat_delay[20];
	unsigned int relt_delay[20];
	unsigned int repeat_peroid[20];
	int (*remote_reprot_press_key)(struct remote *);
	int (*key_report)(struct remote *);
	void (*key_release_report)(struct remote *);
	void (*remote_send_key)(struct input_dev *, unsigned int ,
		 unsigned int , int);
};

extern type_printk input_dbg;

int set_remote_mode(struct remote *remote_data, int mode);
void set_remote_init(struct remote *remote_data);
void kdb_send_key(struct input_dev *dev, unsigned int scancode ,
		unsigned int type , int event);
void remote_send_key(struct input_dev *dev, unsigned int scancode ,
		unsigned int type , int event);
extern int remote_hw_reprot_null_key(struct remote *remote_data);
extern int remote_hw_reprot_key(struct remote *remote_data);
extern int remote_hw_nec_rca_2in1_reprot_key(
		struct remote *remote_data);
extern int remote_hw_nec_toshiba_2in1_reprot_key(struct remote *remote_data);
extern int remote_sw_reprot_key(struct remote *remote_data);
extern void remote_nec_report_release_key(struct remote *remote_data);
extern void remote_nec_rca_2in1_report_release_key(
		struct remote *remote_data);
extern void remote_nec_toshiba_2in1_report_release_key(
		struct remote *remote_data);
extern void remote_duokan_report_release_key(struct remote *remote_data);
extern void remote_sw_reprot_release_key(struct remote *remote_data);
extern void remote_null_reprot_release_key(struct remote *remote_data);

#endif
