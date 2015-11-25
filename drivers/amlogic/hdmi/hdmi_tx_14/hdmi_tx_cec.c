/*
 * drivers/amlogic/hdmi/hdmi_tx_14/hdmi_tx_cec.c
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


#include <linux/version.h>
#include <linux/module.h>
#include <linux/irq.h>
#include <linux/types.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/major.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <asm/irq.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/spinlock_types.h>
#include <linux/switch.h>
#include <linux/workqueue.h>
#include <linux/timer.h>

#include <linux/uaccess.h>
#include <linux/delay.h>

#include <linux/amlogic/hdmi_tx/hdmi_tx_cec_14.h>
#include <linux/amlogic/hdmi_tx/hdmi_tx_module.h>

static struct hdmitx_dev *hdmitx_device;
static void cec_do_tasklet(unsigned long data);
DECLARE_TASKLET(cec_tasklet, cec_do_tasklet, 0);
static unsigned char rx_msg[MAX_MSG];
static unsigned char rx_len;

DEFINE_SPINLOCK(cec_input_key);

/* global variables */
static unsigned char gbl_msg[MAX_MSG];
struct cec_global_info_t cec_global_info;
unsigned char rc_long_press_pwr_key = 0;
EXPORT_SYMBOL(rc_long_press_pwr_key);
bool cec_msg_dbg_en = 1;

ssize_t cec_lang_config_state(struct switch_dev *sdev, char *buf)
{
	int pos = 0;
	unsigned char index = cec_global_info.my_node_index;

	pos += snprintf(buf+pos, PAGE_SIZE, "%c%c%c\n",
	(cec_global_info.cec_node_info[index].menu_lang >> 16) & 0xff,
	(cec_global_info.cec_node_info[index].menu_lang >> 8) & 0xff,
	(cec_global_info.cec_node_info[index].menu_lang >> 0) & 0xff);
	return pos;
};

struct switch_dev lang_dev = {	/* android ics switch device */
	.name = "lang_config",
	.print_state = cec_lang_config_state,
	};
EXPORT_SYMBOL(lang_dev);

static DEFINE_SPINLOCK(p_tx_list_lock);
static unsigned long cec_tx_list_flags;
static unsigned int tx_msg_cnt;
static struct list_head cec_tx_msg_phead = LIST_HEAD_INIT(cec_tx_msg_phead);

unsigned int menu_lang_array[] = {
	(((unsigned int)'c')<<16)|(((unsigned int)'h')<<8)|((unsigned int)'i'),
	(((unsigned int)'e')<<16)|(((unsigned int)'n')<<8)|((unsigned int)'g'),
	(((unsigned int)'j')<<16)|(((unsigned int)'p')<<8)|((unsigned int)'n'),
	(((unsigned int)'k')<<16)|(((unsigned int)'o')<<8)|((unsigned int)'r'),
	(((unsigned int)'f')<<16)|(((unsigned int)'r')<<8)|((unsigned int)'a'),
	(((unsigned int)'g')<<16)|(((unsigned int)'e')<<8)|((unsigned int)'r')
	};

/* CEC default setting */
static unsigned char *osd_name = "MBox";
static unsigned int vendor_id = 0x00;

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
static struct early_suspend hdmitx_cec_early_suspend_handler;
static void hdmitx_cec_early_suspend(struct early_suspend *h)
{
	hdmi_print(INF, CEC "early suspend!\n");
	if (!hdmitx_device->hpd_state) { /* if none HDMI out,no CEC features. */
		hdmi_print(INF, CEC "HPD low!\n");
		return;
	}

	if (hdmitx_device->cec_func_config & (1 << CEC_FUNC_MSAK)) {
		cec_menu_status_smp(DEVICE_MENU_INACTIVE);
		cec_inactive_source();

		if (rc_long_press_pwr_key == 1) {
			cec_set_standby();
			msleep(100);
			hdmi_print(INF, CEC "get power-off CMD Fr RC\n");
			rc_long_press_pwr_key = 0;
		}
	}
	cec_disable_irq();
}

static void hdmitx_cec_late_resume(struct early_suspend *h)
{
	cec_enable_irq();
	cec_rx_buf_check();
	hdmitx_device->hpd_state = hdmitx_device->HWOp.CntlMisc(hdmitx_device,
	    MISC_HPD_GPI_ST, 0);
	if (!hdmitx_device->hpd_state) { /* if none HDMI out,no CEC features. */
		hdmi_print(INF, CEC "HPD low!\n");
		return;
	}

	if (hdmitx_device->cec_func_config & (1 << CEC_FUNC_MSAK)) {
		cec_hw_reset();/* for M8 CEC standby. */
		msleep(20);
		cec_imageview_on_smp();
		msleep(30);
		cec_active_source_smp();
		/* msleep(100); */
		/* cec_active_source_smp(); */
		/* msleep(100); */
		/* cec_menu_status_smp(DEVICE_MENU_ACTIVE); */
	}
	hdmi_print(INF, CEC "late resume\n");
}

#endif

void cec_isr_post_process(void)
{
	if (!hdmitx_device->hpd_state) { /* if no HDMI out,no CEC features. */
		return;
	}
	/* isr post process */
	while (cec_global_info.cec_rx_msg_buf.rx_read_pos
	    != cec_global_info.cec_rx_msg_buf.rx_write_pos) {
		cec_handle_message(&(cec_global_info.cec_rx_msg_buf
			.cec_rx_message[cec_global_info.cec_rx_msg_buf
			.rx_read_pos]));
		(cec_global_info.cec_rx_msg_buf.rx_read_pos
		    == cec_global_info.cec_rx_msg_buf.rx_buf_size - 1)
		    ? (cec_global_info.cec_rx_msg_buf.rx_read_pos = 0)
		    : (cec_global_info.cec_rx_msg_buf.rx_read_pos++);
	}
}

void cec_usr_cmd_post_process(void)
{
	struct cec_usr_message_list_t *p, *ptmp;
	/* usr command post process */
	list_for_each_entry_safe(p, ptmp, &cec_tx_msg_phead, list)
	{
		cec_ll_tx(p->msg, p->length);
		unregister_cec_tx_msg(p);
	}
}

static int detect_tv_support_cec(unsigned addr)
{
	unsigned int ret = 0;
	unsigned char msg[1];
	msg[0] = (addr<<4) | 0x0;	   /* 0x0, TV's root address */
	ret = cec_ll_tx_polling(msg, 1);
	cec_hw_reset();
	hdmi_print(INF, CEC "tv%s have CEC feature\n", ret ? " " : " don\'t ");
	return hdmitx_device->tv_cec_support = ret;
}

int cec_node_init(struct hdmitx_dev *hdmitx_device)
{
	unsigned char a, b, c, d;
	struct vendor_info_data *vend_data = NULL;

	int i, bool = 0;
	const enum _cec_log_dev_addr_e player_dev[3] = {
		CEC_PLAYBACK_DEVICE_1_ADDR,
		CEC_PLAYBACK_DEVICE_2_ADDR,
		CEC_PLAYBACK_DEVICE_3_ADDR,
	};

	unsigned long cec_phy_addr;

	/* If no connect, return directly */
	if ((hdmitx_device->cec_init_ready == 0)
	    || (hdmitx_device->hpd_state == 0)) {
		hdmi_print(INF, CEC "CEC not ready\n");
		return -1;
	} else
		hdmi_print(INF, CEC "CEC node init\n");

	if (hdmitx_device->config_data.vend_data)
		vend_data = hdmitx_device->config_data.vend_data;
#if 0
	if ((vend_data) && (vend_data->cec_config)) {
		hdmitx_device->cec_func_config = vend_data->cec_config;
		cec_config(vend_data->cec_config, 1);
		hdmi_print(INF, CEC "cec_func_config: 0x%x; cec_config:0x%x\n",
			hdmitx_device->cec_func_config, cec_config(0, 0));
	}
#endif
	/* hdmi_print(INF, CEC "cec_config: 0x%x; ao_cec:0x%x\n", */
	/* vend_data->cec_config, vend_data->ao_cec); */

	if ((vend_data) && (vend_data->cec_osd_string)) {
		i = strlen(vend_data->cec_osd_string);
		if (i > 14) {
			/* OSD string length must be less than 14 bytes */
			vend_data->cec_osd_string[14] = '\0';
		}
		/* osd_name = vend_data->cec_osd_string; */
	}

	if ((vend_data) && (vend_data->vendor_id))
		vendor_id = (vend_data->vendor_id) & 0xffffff;

	if (!(hdmitx_device->cec_func_config & (1 << CEC_FUNC_MSAK)))
		return -1;

	hdmi_print(INF, CEC "cec_func_config: 0x%x; cec_config:0x%x\n",
	    hdmitx_device->cec_func_config, cec_config(0, 0));

	cec_pinmux_set();
	ao_cec_init();

	cec_arbit_bit_time_set(3, 0x118, 0);
	cec_arbit_bit_time_set(5, 0x000, 0);
	cec_arbit_bit_time_set(7, 0x2aa, 0);

	a = hdmitx_device->hdmi_info.vsdb_phy_addr.a;
	b = hdmitx_device->hdmi_info.vsdb_phy_addr.b;
	c = hdmitx_device->hdmi_info.vsdb_phy_addr.c;
	d = hdmitx_device->hdmi_info.vsdb_phy_addr.d;

	cec_phy_addr = ((a << 12) | (b << 8) | (c << 4) | (d << 0));

	for (i = 0; i < 3; i++) {
		hdmi_print(INF, CEC "CEC: start poll dev\n");
		cec_polling_online_dev(player_dev[i], &bool);
		hdmi_print(INF, CEC "player_dev[%d]:0x%x\n", i, player_dev[i]);
		if (bool == 0) {   /* 0 means that no any respond */
/* If VSDB is not valid, use last or default physical address. */
			if (hdmitx_device->hdmi_info.vsdb_phy_addr.valid == 0) {
				hdmi_print(INF, CEC "invalid cec PhyAddr\n");
				if (cec_phyaddr_config(0, 0))
					hdmi_print(INF, CEC
						"use last physical address\n");
				else {
					cec_phyaddr_config(0x1000, 1);
					hdmi_print(INF, CEC
						"use Def Phy address\n");
				}
			} else
				cec_phyaddr_config(cec_phy_addr, 1);

			hdmi_print(INF, CEC "physical address:0x%x\n",
			    cec_phyaddr_config(0, 0));

			cec_global_info.cec_node_info[cec_global_info.
				my_node_index].power_status =
				TRANS_STANDBY_TO_ON;
			cec_global_info.my_node_index = player_dev[i];
			cec_logicaddr_config(cec_global_info.my_node_index, 1);
			cec_global_info.cec_node_info[player_dev[i]].log_addr
			    = player_dev[i];
			/* Set Physical address */
			cec_global_info.cec_node_info[player_dev[i]].
				phy_addr.phy_addr_4 = cec_phy_addr;

			cec_global_info.cec_node_info[player_dev[i]].
				specific_info.audio.sys_audio_mode = OFF;
			cec_global_info.cec_node_info[player_dev[i]].
				specific_info.audio.audio_status.
				audio_mute_status = OFF;
			cec_global_info.cec_node_info[player_dev[i]].
				specific_info.audio.audio_status.
				audio_volume_status = 0;

			cec_global_info.cec_node_info[player_dev[i]].cec_version
			    = CEC_VERSION_14A;
			cec_global_info.cec_node_info[player_dev[i]].vendor_id
			    = vendor_id;
			cec_global_info.cec_node_info[player_dev[i]].dev_type
			    = cec_log_addr_to_dev_type(player_dev[i]);
			cec_global_info.cec_node_info[player_dev[i]].dev_type
			    = cec_log_addr_to_dev_type(player_dev[i]);
			/* Max num: 14Bytes */
			strcpy(cec_global_info.cec_node_info[
				player_dev[i]].osd_name, osd_name);

			cec_logicaddr_set(player_dev[i]);

			hdmi_print(INF, CEC "Set logical address: %d\n",
				player_dev[i]);

			if (cec_global_info.cec_node_info[
				cec_global_info.my_node_index].
				menu_status == DEVICE_MENU_INACTIVE)
				break;
			msleep(100);
			cec_report_physical_address_smp();
			msleep(150);
			cec_device_vendor_id((struct cec_rx_message_t *)0);

			msleep(150);
			cec_imageview_on_smp();
			msleep(100);

			cec_get_menu_language_smp();
			msleep(350);

			cec_active_source_smp();
			msleep(120);

			cec_menu_status_smp(DEVICE_MENU_ACTIVE);
			msleep(100);

			cec_global_info.cec_node_info[cec_global_info.
				my_node_index].menu_status =
					DEVICE_MENU_ACTIVE;
			cec_global_info.cec_node_info[cec_global_info.
				my_node_index].power_status = POWER_ON;
			break;
		}
	}
	if (bool == 1) {
		hdmi_print(INF, CEC "Can't get a valid logical address\n");
		return -1;
	} else {
		hdmi_print(INF, CEC "cec node init: cec features ok !\n");
		return 0;
	}
}

void cec_node_uninit(struct hdmitx_dev *hdmitx_device)
{
	if (!(hdmitx_device->cec_func_config & (1 << CEC_FUNC_MSAK)))
		return;
	cec_global_info.cec_node_info[cec_global_info.my_node_index].
		power_status = TRANS_ON_TO_STANDBY;
	hdmi_print(INF, CEC "cec node uninit!\n");
	cec_global_info.cec_node_info[cec_global_info.my_node_index].
		power_status = POWER_STANDBY;
}

static int cec_task(void *data)
{
	unsigned int i;
	struct hdmitx_dev *hdmitx_device = (struct hdmitx_dev *) data;
	cec_global_info.cec_flag.cec_init_flag = 1;

#ifdef CONFIG_HAS_EARLYSUSPEND
	hdmitx_cec_early_suspend_handler.level
	    = EARLY_SUSPEND_LEVEL_BLANK_SCREEN - 20;
	hdmitx_cec_early_suspend_handler.suspend = hdmitx_cec_early_suspend;
	hdmitx_cec_early_suspend_handler.resume = hdmitx_cec_late_resume;
	hdmitx_cec_early_suspend_handler.param = hdmitx_device;

	register_early_suspend(&hdmitx_cec_early_suspend_handler);
#endif

	/* Get logical address */

	hdmi_print(INF, CEC "CEC task process\n");
	if (hdmitx_device->cec_func_config & (1 << CEC_FUNC_MSAK)) {
		msleep_interruptible(15000);
		cec_node_init(hdmitx_device);
	}
	while (1) {
		/* cec_rx_buf_check(); */
		i = 30;
		do {
			msleep_interruptible(10);
			if (cec_rx_buf_check())
				break;
		} while (i--);
		if (kthread_should_stop())
			break;
		wait_event_interruptible(hdmitx_device->cec_wait_rx,
		    cec_global_info.cec_rx_msg_buf.rx_read_pos
		    != cec_global_info.cec_rx_msg_buf.rx_write_pos);
		cec_isr_post_process();
		/* cec_usr_cmd_post_process(); */
	}

	return 0;
}

/* cec low level code end */

/* cec middle level code */

void register_cec_rx_msg(unsigned char *msg, unsigned char len)
{
	unsigned long flags;
	int pos = cec_global_info.cec_rx_msg_buf.rx_write_pos;
	spin_lock_irqsave(&cec_input_key, flags);
	memset((void *)(&(cec_global_info.cec_rx_msg_buf.
		cec_rx_message[pos])), 0, sizeof(struct cec_rx_message_t));
	memcpy(cec_global_info.cec_rx_msg_buf.cec_rx_message[pos].
		content.buffer,	msg, len);

	cec_global_info.cec_rx_msg_buf.cec_rx_message[pos].operand_num
	    = len >= 2 ? len - 2 : 0;
	cec_global_info.cec_rx_msg_buf.cec_rx_message[pos].msg_length = len;

	cec_input_handle_message();

	(pos == cec_global_info.cec_rx_msg_buf.rx_buf_size - 1)
	? (cec_global_info.cec_rx_msg_buf.rx_write_pos = 0)
	: (cec_global_info.cec_rx_msg_buf.rx_write_pos++);
	spin_unlock_irqrestore(&cec_input_key, flags);
}

void register_cec_tx_msg(unsigned char *msg, unsigned char len)
{
	struct cec_usr_message_list_t *cec_usr_message_list
	    = kmalloc(sizeof(struct cec_usr_message_list_t), GFP_ATOMIC);

	if (cec_usr_message_list != NULL) {
		memset(cec_usr_message_list, 0,
			sizeof(struct cec_usr_message_list_t));
		memcpy(cec_usr_message_list->msg, msg, len);
		cec_usr_message_list->length = len;

		spin_lock_irqsave(&p_tx_list_lock, cec_tx_list_flags);
		list_add_tail(&cec_usr_message_list->list, &cec_tx_msg_phead);
		spin_unlock_irqrestore(&p_tx_list_lock, cec_tx_list_flags);

		tx_msg_cnt++;
	}
}

static void cec_do_tasklet(unsigned long data)
{
	/* cec_rx_buf_check(); */
	register_cec_rx_msg(rx_msg, rx_len);
	/* udelay(1000); */
	wake_up(&hdmitx_device->cec_wait_rx);
	/* cec_rx_buf_check(); */
}

void cec_input_handle_message(void)
{
	unsigned char opernum;
	unsigned char follower;
	int pos = cec_global_info.cec_rx_msg_buf.rx_write_pos;
	unsigned char opcode =
	cec_global_info.cec_rx_msg_buf.cec_rx_message[pos].content.msg.opcode;

	if (NULL == hdmitx_device) {
		hdmitx_device = get_hdmitx_device();
		hdmi_print(INF, CEC "Error:hdmitx_device NULL!\n");
		return;
	}
	/* process key event messages from tv */
	if (hdmitx_device->cec_func_config & (1 << CEC_FUNC_MSAK)) {
		switch (opcode) {
		case CEC_OC_USER_CONTROL_PRESSED:
			/* check valid msg */
			opernum  = cec_global_info.cec_rx_msg_buf.cec_rx_message
			    [pos].operand_num;
			follower = cec_global_info.cec_rx_msg_buf.cec_rx_message
			[pos].content.msg.header & 0x0f;
			if (opernum != 1 || follower == 0xf)
				break;
			cec_user_control_pressed_irq();
			break;
		default:
			break;
		}
	}
}

void unregister_cec_tx_msg(struct cec_usr_message_list_t *cec_tx_message_list)
{

	if (cec_tx_message_list != NULL) {
		list_del(&cec_tx_message_list->list);
		kfree(cec_tx_message_list);
		cec_tx_message_list = NULL;

		if (tx_msg_cnt > 0)
			tx_msg_cnt--;
	}
}

unsigned char check_cec_msg_valid(const struct cec_rx_message_t *pcec_message)
{
	unsigned char rt = 0;
	unsigned char opcode;
	unsigned char opernum;
	unsigned char follower;
	if (!pcec_message)
		return rt;

	opcode = pcec_message->content.msg.opcode;
	opernum = pcec_message->operand_num;

	switch (opcode) {
	case CEC_OC_VENDOR_REMOTE_BUTTON_UP:
	case CEC_OC_STANDBY:
	case CEC_OC_RECORD_OFF:
	case CEC_OC_RECORD_TV_SCREEN:
	case CEC_OC_TUNER_STEP_DECREMENT:
	case CEC_OC_TUNER_STEP_INCREMENT:
	case CEC_OC_GIVE_AUDIO_STATUS:
	case CEC_OC_GIVE_SYSTEM_AUDIO_MODE_STATUS:
	case CEC_OC_USER_CONTROL_RELEASED:
	case CEC_OC_GIVE_OSD_NAME:
	case CEC_OC_GIVE_PHYSICAL_ADDRESS:
	case CEC_OC_GET_CEC_VERSION:
	case CEC_OC_GET_MENU_LANGUAGE:
	case CEC_OC_GIVE_DEVICE_VENDOR_ID:
	case CEC_OC_GIVE_DEVICE_POWER_STATUS:
	case CEC_OC_TEXT_VIEW_ON:
	case CEC_OC_IMAGE_VIEW_ON:
	case CEC_OC_ABORT_MESSAGE:
	case CEC_OC_REQUEST_ACTIVE_SOURCE:
		if (opernum == 0)
			rt = 1;
		break;
	case CEC_OC_SET_SYSTEM_AUDIO_MODE:
	case CEC_OC_RECORD_STATUS:
	case CEC_OC_DECK_CONTROL:
	case CEC_OC_DECK_STATUS:
	case CEC_OC_GIVE_DECK_STATUS:
	case CEC_OC_GIVE_TUNER_DEVICE_STATUS:
	case CEC_OC_PLAY:
	case CEC_OC_MENU_REQUEST:
	case CEC_OC_MENU_STATUS:
	case CEC_OC_REPORT_AUDIO_STATUS:
	case CEC_OC_TIMER_CLEARED_STATUS:
	case CEC_OC_SYSTEM_AUDIO_MODE_STATUS:
	case CEC_OC_USER_CONTROL_PRESSED:
	case CEC_OC_CEC_VERSION:
	case CEC_OC_REPORT_POWER_STATUS:
	case CEC_OC_SET_AUDIO_RATE:
		if (opernum == 1)
			rt = 1;
		break;
	case CEC_OC_INACTIVE_SOURCE:
	case CEC_OC_SYSTEM_AUDIO_MODE_REQUEST:
	case CEC_OC_FEATURE_ABORT:
	case CEC_OC_ACTIVE_SOURCE:
	case CEC_OC_ROUTING_INFORMATION:
	case CEC_OC_SET_STREAM_PATH:
		if (opernum == 2)
			rt = 1;
		break;
	case CEC_OC_REPORT_PHYSICAL_ADDRESS:
	case CEC_OC_SET_MENU_LANGUAGE:
	case CEC_OC_DEVICE_VENDOR_ID:
		if (opernum == 3)
			rt = 1;
		break;
	case CEC_OC_ROUTING_CHANGE:
	case CEC_OC_SELECT_ANALOGUE_SERVICE:
		if (opernum == 4)
			rt = 1;
		break;
	case CEC_OC_VENDOR_COMMAND_WITH_ID:
		if ((opernum > 3) && (opernum < 15))
			rt = 1;
		break;
	case CEC_OC_VENDOR_REMOTE_BUTTON_DOWN:
		if (opernum < 15)
			rt = 1;
		break;
	case CEC_OC_SELECT_DIGITAL_SERVICE:
		if (opernum == 7)
			rt = 1;
		break;
	case CEC_OC_SET_ANALOGUE_TIMER:
	case CEC_OC_CLEAR_ANALOGUE_TIMER:
		if (opernum == 11)
			rt = 1;
		break;
	case CEC_OC_SET_DIGITAL_TIMER:
	case CEC_OC_CLEAR_DIGITAL_TIMER:
		if (opernum == 14)
			rt = 1;
		break;
	case CEC_OC_TIMER_STATUS:
		if ((opernum == 1 || opernum == 3))
			rt = 1;
		break;
	case CEC_OC_TUNER_DEVICE_STATUS:
		if ((opernum == 5 || opernum == 8))
			rt = 1;
		break;
	case CEC_OC_RECORD_ON:
		if (opernum > 0 && opernum < 9)
			rt = 1;
		break;
	case CEC_OC_CLEAR_EXTERNAL_TIMER:
	case CEC_OC_SET_EXTERNAL_TIMER:
		if ((opernum == 9 || opernum == 10))
			rt = 1;
		break;
	case CEC_OC_SET_TIMER_PROGRAM_TITLE:
	case CEC_OC_SET_OSD_NAME:
		if (opernum > 0 && opernum < 15)
			rt = 1;
		break;
	case CEC_OC_SET_OSD_STRING:
		if (opernum > 1 && opernum < 15)
			rt = 1;
		break;
	case CEC_OC_VENDOR_COMMAND:
		if (opernum < 15)
			rt = 1;
		break;
	default:
		rt = 1;
		break;
	}

 /* for CTS12.2 */
	follower = pcec_message->content.msg.header & 0x0f;
	switch (opcode) {
	case CEC_OC_ACTIVE_SOURCE:
	case CEC_OC_REQUEST_ACTIVE_SOURCE:
	case CEC_OC_ROUTING_CHANGE:
	case CEC_OC_ROUTING_INFORMATION:
	case CEC_OC_SET_STREAM_PATH:
	case CEC_OC_REPORT_PHYSICAL_ADDRESS:
	case CEC_OC_SET_MENU_LANGUAGE:
	case CEC_OC_DEVICE_VENDOR_ID:
		/* broadcast only */
		if (follower != 0xf)
			rt = 0;
		break;

	case CEC_OC_IMAGE_VIEW_ON:
	case CEC_OC_TEXT_VIEW_ON:
	case CEC_OC_INACTIVE_SOURCE:
	case CEC_OC_RECORD_OFF:
	case CEC_OC_RECORD_ON:
	case CEC_OC_RECORD_STATUS:
	case CEC_OC_RECORD_TV_SCREEN:
	case CEC_OC_CLEAR_ANALOGUE_TIMER:
	case CEC_OC_CLEAR_DIGITAL_TIMER:
	case CEC_OC_CLEAR_EXTERNAL_TIMER:
	case CEC_OC_SET_ANALOGUE_TIMER:
	case CEC_OC_SET_DIGITAL_TIMER:
	case CEC_OC_SET_EXTERNAL_TIMER:
	case CEC_OC_SET_TIMER_PROGRAM_TITLE:
	case CEC_OC_TIMER_CLEARED_STATUS:
	case CEC_OC_TIMER_STATUS:
	case CEC_OC_CEC_VERSION:
	case CEC_OC_GET_CEC_VERSION:
	case CEC_OC_GIVE_PHYSICAL_ADDRESS:
	case CEC_OC_GET_MENU_LANGUAGE:
	case CEC_OC_DECK_CONTROL:
	case CEC_OC_DECK_STATUS:
	case CEC_OC_GIVE_DECK_STATUS:
	case CEC_OC_PLAY:
	case CEC_OC_GIVE_TUNER_DEVICE_STATUS:
	case CEC_OC_SELECT_ANALOGUE_SERVICE:
	case CEC_OC_SELECT_DIGITAL_SERVICE:
	case CEC_OC_TUNER_DEVICE_STATUS:
	case CEC_OC_TUNER_STEP_DECREMENT:
	case CEC_OC_TUNER_STEP_INCREMENT:
	case CEC_OC_GIVE_DEVICE_VENDOR_ID:
	case CEC_OC_VENDOR_COMMAND:
	case CEC_OC_SET_OSD_STRING:
	case CEC_OC_GIVE_OSD_NAME:
	case CEC_OC_SET_OSD_NAME:
	case CEC_OC_MENU_REQUEST:
	case CEC_OC_MENU_STATUS:
	case CEC_OC_USER_CONTROL_PRESSED:
	case CEC_OC_USER_CONTROL_RELEASED:
	case CEC_OC_GIVE_DEVICE_POWER_STATUS:
	case CEC_OC_REPORT_POWER_STATUS:
	case CEC_OC_FEATURE_ABORT:
	case CEC_OC_ABORT_MESSAGE:
	case CEC_OC_GIVE_AUDIO_STATUS:
	case CEC_OC_GIVE_SYSTEM_AUDIO_MODE_STATUS:
	case CEC_OC_REPORT_AUDIO_STATUS:
	case CEC_OC_SYSTEM_AUDIO_MODE_REQUEST:
	case CEC_OC_SYSTEM_AUDIO_MODE_STATUS:
	case CEC_OC_SET_AUDIO_RATE:
		/* directly addressed only */
		if (follower == 0xf)
			rt = 0;
		break;

	case CEC_OC_STANDBY:
	case CEC_OC_VENDOR_COMMAND_WITH_ID:
	case CEC_OC_VENDOR_REMOTE_BUTTON_DOWN:
	case CEC_OC_VENDOR_REMOTE_BUTTON_UP:
	case CEC_OC_SET_SYSTEM_AUDIO_MODE:
		/* both broadcast and directly addressed */
		break;

	default:
		break;
	}

	if ((rt == 0) & (opcode != 0)) {
		hdmi_print(INF, CEC "CEC: opcode & opernum not match: %x, %x\n",
		    opcode, opernum);
	}
	return rt;
}

irqreturn_t cec_isr_handler(int irq, void *dev_instance)
{
	unsigned int intr_stat = 0;

	if (cec_msg_dbg_en  == 1) {
		intr_stat = cec_intr_stat();
		hdmi_print(INF, CEC "cec intr_stat %x\n", intr_stat);
	}
	if (intr_stat & (1<<1)) {   /* aocec tx intr */
		tx_irq_handle();
		/* cec_enable_irq(); */
		return IRQ_HANDLED;
	}
	if ((-1) == cec_ll_rx(rx_msg, &rx_len)) {
		/* cec_enable_irq(); */
		/* cec_rx_buf_check(); */
		return IRQ_HANDLED;
	}

	/* register_cec_rx_msg(rx_msg, rx_len); */
	/* wake_up(&hdmitx_device->cec_wait_rx); */
	tasklet_schedule(&cec_tasklet);
	/* cec_rx_buf_check(); */

	/* cec_enable_irq(); */

	return IRQ_HANDLED;
}

unsigned short cec_log_addr_to_dev_type(unsigned char log_addr)
{
	unsigned short us = CEC_UNREGISTERED_DEVICE_TYPE;
	if ((1 << log_addr) & CEC_DISPLAY_DEVICE)
		us = CEC_DISPLAY_DEVICE_TYPE;
	else if ((1 << log_addr) & CEC_RECORDING_DEVICE)
		us = CEC_RECORDING_DEVICE_TYPE;
	else if ((1 << log_addr) & CEC_PLAYBACK_DEVICE)
		us = CEC_PLAYBACK_DEVICE_TYPE;
	else if ((1 << log_addr) & CEC_TUNER_DEVICE)
		us = CEC_TUNER_DEVICE_TYPE;
	else if ((1 << log_addr) & CEC_AUDIO_SYSTEM_DEVICE)
		us = CEC_AUDIO_SYSTEM_DEVICE_TYPE;
	else
		;
	return us;
}
/* -------------- command from cec devices --------------------- */
/* *************************************************************** */
void cec_device_vendor_id(struct cec_rx_message_t *pcec_message)
{
	unsigned char index = cec_global_info.my_node_index;
	unsigned char msg[5];

	msg[0] = ((index & 0xf) << 4) | CEC_BROADCAST_ADDR;
	msg[1] = CEC_OC_DEVICE_VENDOR_ID;
	msg[2] = (vendor_id >> 16) & 0xff;
	msg[3] = (vendor_id >> 8) & 0xff;
	msg[4] = (vendor_id >> 0) & 0xff;

	cec_ll_tx(msg, 5);
}

void cec_report_power_status(struct cec_rx_message_t *pcec_message)
{
	unsigned char index = cec_global_info.my_node_index;
	unsigned char msg[3];

	msg[0] = ((index & 0xf) << 4) | CEC_TV_ADDR;
	msg[1] = CEC_OC_REPORT_POWER_STATUS;
	msg[2] = cec_global_info.cec_node_info[index].power_status;
	cec_ll_tx(msg, 3);

}

void cec_feature_abort(struct cec_rx_message_t *pcec_message)
{
	unsigned char index = cec_global_info.my_node_index;
	unsigned char opcode = pcec_message->content.msg.opcode;
	unsigned char src_log_addr =
		(pcec_message->content.msg.header >> 4) & 0xf;
	unsigned char dst_log_addr =
		pcec_message->content.msg.header & 0xf;

	if (dst_log_addr != 0xf) {
		unsigned char msg[4];

		msg[0] = ((index & 0xf) << 4) | src_log_addr;
		msg[1] = CEC_OC_FEATURE_ABORT;
		msg[2] = opcode;
		msg[3] = CEC_UNRECONIZED_OPCODE;

		cec_ll_tx(msg, 4);
	}
}

void cec_report_version(struct cec_rx_message_t *pcec_message)
{
	;/* todo */
}


void cec_report_physical_address_smp(void)
{
	unsigned char msg[5];
	unsigned char index = cec_global_info.my_node_index;
	unsigned char phy_addr_ab = (cec_phyaddr_config(0, 0) >> 8) & 0xff;
	unsigned char phy_addr_cd = cec_phyaddr_config(0, 0) & 0xff;

	msg[0] = ((index & 0xf) << 4) | CEC_BROADCAST_ADDR;
	msg[1] = CEC_OC_REPORT_PHYSICAL_ADDRESS;
	msg[2] = phy_addr_ab;
	msg[3] = phy_addr_cd;
	msg[4] = cec_global_info.cec_node_info[index].dev_type;

	cec_ll_tx(msg, 5);
}

void cec_imageview_on_smp(void)
{
	unsigned char msg[2];
	unsigned char index = cec_global_info.my_node_index;
	unsigned int mask = (1 << CEC_FUNC_MSAK) | (1 << ONE_TOUCH_PLAY_MASK);

	if ((hdmitx_device->cec_func_config & mask) == mask) {
		msg[0] = ((index & 0xf) << 4) | CEC_TV_ADDR;
		msg[1] = CEC_OC_IMAGE_VIEW_ON;
		cec_ll_tx(msg, 2);
	}
}

void cec_get_menu_language_smp(void)
{
	unsigned char msg[2];
	unsigned char index = cec_global_info.my_node_index;

	msg[0] = ((index & 0xf) << 4) | CEC_TV_ADDR;
	msg[1] = CEC_OC_GET_MENU_LANGUAGE;

	cec_ll_tx(msg, 2);
}

void cec_menu_status(struct cec_rx_message_t *pcec_message)
{
	unsigned char msg[3];
	unsigned char index = cec_global_info.my_node_index;
	unsigned char src_log_addr =
		(pcec_message->content.msg.header >> 4) & 0xf;

	if (0xf != src_log_addr) {
		msg[0] = ((index & 0xf) << 4) | src_log_addr;
		msg[1] = CEC_OC_MENU_STATUS;
		msg[2] = cec_global_info.cec_node_info[index].menu_status;
		cec_ll_tx(msg, 3);
	}
}

void cec_menu_status_smp(enum cec_device_menu_state_e status)
{
	unsigned char msg[3];
	unsigned char index = cec_global_info.my_node_index;

	msg[0] = ((index & 0xf) << 4) | CEC_TV_ADDR;
	msg[1] = CEC_OC_MENU_STATUS;
	if (status == DEVICE_MENU_ACTIVE) {
		msg[2] = DEVICE_MENU_ACTIVE;
		cec_global_info.cec_node_info[index].menu_status
			= DEVICE_MENU_ACTIVE;
	} else {
		msg[2] = DEVICE_MENU_INACTIVE;
		cec_global_info.cec_node_info[index].menu_status
			= DEVICE_MENU_INACTIVE;
	}
	cec_ll_tx(msg, 3);
}

void cec_menu_status_smp_irq(struct cec_rx_message_t *pcec_message)
{
	unsigned char index = cec_global_info.my_node_index;

	if (1 == pcec_message->content.msg.operands[0]) {
		cec_global_info.cec_node_info[index].menu_status
			= DEVICE_MENU_INACTIVE;
	} else if (0 == pcec_message->content.msg.operands[0]) {
		cec_global_info.cec_node_info[index].menu_status
			= DEVICE_MENU_ACTIVE;
	}
}

void cec_active_source_rx(struct cec_rx_message_t *pcec_message)
{
	unsigned int phy_addr_active;

	phy_addr_active =
	    (unsigned int)((pcec_message->content.msg.operands[0] << 8)
	    | (pcec_message->content.msg.operands[1] << 0));

	if (phy_addr_active == (cec_phyaddr_config(0, 0) & 0xffff)) {
		cec_global_info.cec_node_info[cec_global_info.my_node_index]
		    .menu_status = DEVICE_MENU_ACTIVE;
	} else {
		cec_global_info.cec_node_info[cec_global_info.my_node_index]
		    .menu_status = DEVICE_MENU_INACTIVE;
	}
}

void cec_active_source_smp(void)
{
	unsigned char msg[4];
	unsigned char index = cec_global_info.my_node_index;
	unsigned char phy_addr_ab = (cec_phyaddr_config(0, 0) >> 8) & 0xff;
	unsigned char phy_addr_cd = cec_phyaddr_config(0, 0) & 0xff;
	unsigned int mask = (1 << CEC_FUNC_MSAK) | (1 << ONE_TOUCH_PLAY_MASK);

	if ((hdmitx_device->cec_func_config & mask) == mask) {
		msg[0] = ((index & 0xf) << 4) | CEC_BROADCAST_ADDR;
		msg[1] = CEC_OC_ACTIVE_SOURCE;
		msg[2] = phy_addr_ab;
		msg[3] = phy_addr_cd;
		cec_ll_tx(msg, 4);
	}
	cec_global_info.cec_node_info[cec_global_info.my_node_index].menu_status
	    = DEVICE_MENU_ACTIVE;
}
void cec_active_source(struct cec_rx_message_t *pcec_message)
{
	unsigned char msg[4];
	unsigned char index = cec_global_info.my_node_index;
	unsigned char phy_addr_ab = (cec_phyaddr_config(0, 0) >> 8) & 0xff;
	unsigned char phy_addr_cd = cec_phyaddr_config(0, 0) & 0xff;

	msg[0] = ((index & 0xf) << 4) | CEC_BROADCAST_ADDR;
	msg[1] = CEC_OC_ACTIVE_SOURCE;
	msg[2] = phy_addr_ab;
	msg[3] = phy_addr_cd;
	cec_ll_tx(msg, 4);
	cec_global_info.cec_node_info[cec_global_info.my_node_index].menu_status
	    = DEVICE_MENU_ACTIVE;
}


void cec_set_stream_path(struct cec_rx_message_t *pcec_message)
{
	unsigned int phy_addr_active;

	phy_addr_active =
	    (unsigned int)((pcec_message->content.msg.operands[0] << 8)
	    | (pcec_message->content.msg.operands[1] << 0));

	if (phy_addr_active == (cec_phyaddr_config(0, 0) & 0xffff)) {
		cec_active_source_smp();
		cec_global_info.cec_node_info[cec_global_info.my_node_index]
		    .menu_status = DEVICE_MENU_ACTIVE;
	} else {
		cec_global_info.cec_node_info[cec_global_info.my_node_index]
		    .menu_status = DEVICE_MENU_INACTIVE;
	}
}
void cec_set_system_audio_mode(void)
{
	unsigned char index = cec_global_info.my_node_index;

	MSG_P1(index, CEC_TV_ADDR,
			CEC_OC_SET_SYSTEM_AUDIO_MODE,
			cec_global_info.cec_node_info[index]
			    .specific_info.audio.sys_audio_mode
			);

	cec_ll_tx(gbl_msg, 3);
	if (cec_global_info.cec_node_info[index].specific_info.
		audio.sys_audio_mode == ON) {
		cec_global_info.cec_node_info[index].specific_info.
			audio.sys_audio_mode = OFF;
	} else {
		cec_global_info.cec_node_info[index].specific_info.
			audio.sys_audio_mode = ON;
	}
}

void cec_system_audio_mode_request(void)
{
	unsigned char index = cec_global_info.my_node_index;
	unsigned char phy_addr_ab = (cec_phyaddr_config(0, 0) >> 8) & 0xff;
	unsigned char phy_addr_cd = cec_phyaddr_config(0, 0) & 0xff;

	if (cec_global_info.cec_node_info[index].specific_info.
		audio.sys_audio_mode == OFF) {
		MSG_P2(index, CEC_AUDIO_SYSTEM_ADDR,/* CEC_TV_ADDR, */
				CEC_OC_SYSTEM_AUDIO_MODE_REQUEST,
				phy_addr_ab,
				phy_addr_cd
				);
		cec_ll_tx(gbl_msg, 4);
		cec_global_info.cec_node_info[index].specific_info.
			audio.sys_audio_mode = ON;
	} else {
		MSG_P0(index, CEC_AUDIO_SYSTEM_ADDR,/* CEC_TV_ADDR, */
				CEC_OC_SYSTEM_AUDIO_MODE_REQUEST
				);
		cec_ll_tx(gbl_msg, 2);
		cec_global_info.cec_node_info[index].specific_info.
			audio.sys_audio_mode = OFF;
	}
}

void cec_report_audio_status(void)
{
	unsigned char index = cec_global_info.my_node_index;

	MSG_P1(index, CEC_TV_ADDR,
			CEC_OC_REPORT_AUDIO_STATUS,
			cec_global_info.cec_node_info[index].specific_info
			    .audio.audio_status.audio_mute_status |
			cec_global_info.cec_node_info[index].specific_info
			    .audio.audio_status.audio_volume_status
			);

	cec_ll_tx(gbl_msg, 3);
}
void cec_request_active_source(struct cec_rx_message_t *pcec_message)
{
	cec_set_stream_path(pcec_message);
}

void cec_set_imageview_on_irq(void)
{
	unsigned char index = cec_global_info.my_node_index;
	unsigned char msg[2];

	msg[0] = ((index & 0xf) << 4) | CEC_TV_ADDR;
	msg[1] = CEC_OC_IMAGE_VIEW_ON;

	cec_ll_tx(msg, 2);
}

void cec_inactive_source(void)
{
	unsigned char index = cec_global_info.my_node_index;
	unsigned char msg[4];
	unsigned char phy_addr_ab = (cec_phyaddr_config(0, 0) >> 8) & 0xff;
	unsigned char phy_addr_cd = cec_phyaddr_config(0, 0) & 0xff;

	msg[0] = ((index & 0xf) << 4) | CEC_TV_ADDR;
	msg[1] = CEC_OC_INACTIVE_SOURCE;
	msg[2] = phy_addr_ab;
	msg[3] = phy_addr_cd;

	cec_ll_tx(msg, 4);
	cec_global_info.cec_node_info[cec_global_info.my_node_index].menu_status
	    = DEVICE_MENU_INACTIVE;
}

void cec_inactive_source_rx(struct cec_rx_message_t *pcec_message)
{
	cec_global_info.cec_node_info[cec_global_info.my_node_index].menu_status
	    = DEVICE_MENU_INACTIVE;
}

void cec_get_version(struct cec_rx_message_t *pcec_message)
{
	unsigned char dest_log_addr = pcec_message->content.msg.header&0xf;
	unsigned char index = cec_global_info.my_node_index;
	unsigned char msg[3];

	if (0xf != dest_log_addr) {
		msg[0] = ((index & 0xf) << 4) | CEC_TV_ADDR;
		msg[1] = CEC_OC_CEC_VERSION;
		msg[2] = CEC_VERSION_14A;
		cec_ll_tx(msg, 3);
	}
}

void cec_give_deck_status(struct cec_rx_message_t *pcec_message)
{
	unsigned char index = cec_global_info.my_node_index;
	unsigned char msg[3];

	msg[0] = ((index & 0xf) << 4) | CEC_TV_ADDR;
	msg[1] = CEC_OC_DECK_STATUS;
	msg[2] = 0x1a;
	cec_ll_tx(msg, 3);
}


void cec_deck_status(struct cec_rx_message_t *pcec_message)
{
	unsigned char index = cec_global_info.my_node_index;

	if (cec_global_info.dev_mask & (1 << index)) {
		cec_global_info.cec_node_info[index].specific_info.
			playback.deck_info =
				pcec_message->content.msg.operands[0];
		cec_global_info.cec_node_info[index].real_info_mask
		    |= INFO_MASK_DECK_INfO;
		hdmi_print(INF, CEC "cec_deck_status: %x\n", cec_global_info
		    .cec_node_info[index].specific_info.playback.deck_info);
	}
}

/* STANDBY: long press our remote control, send STANDBY to TV */
void cec_set_standby(void)
{
	unsigned char msg[2];
	unsigned char index = cec_global_info.my_node_index;
	unsigned int mask;

	mask = (1 << CEC_FUNC_MSAK) | (1 << ONE_TOUCH_STANDBY_MASK);
	msg[0] = ((index & 0xf) << 4) | CEC_BROADCAST_ADDR;
	msg[1] = CEC_OC_STANDBY;

	if ((hdmitx_device->cec_func_config & mask) == mask)
		cec_ll_tx(msg, 2);
}

void cec_set_osd_name(struct cec_rx_message_t *pcec_message)
{
	unsigned char index = cec_global_info.my_node_index;
	unsigned osd_len = strlen(cec_global_info.cec_node_info[index].
		osd_name);
	unsigned src_log_addr = (pcec_message->content.msg.header >> 4) & 0xf;
	unsigned char msg[16];

	if (0xf != src_log_addr) {
		msg[0] = ((index & 0xf) << 4) | src_log_addr;
		msg[1] = CEC_OC_SET_OSD_NAME;
		memcpy(&msg[2], cec_global_info.cec_node_info[index].osd_name,
		    osd_len);

		cec_ll_tx(msg, 2 + osd_len);
	}
}

void cec_set_osd_name_init(void)
{
	unsigned index = cec_global_info.my_node_index;
	unsigned osd_len = strlen(cec_global_info.cec_node_info[index].
		osd_name);
	unsigned char msg[16];

	msg[0] = ((index & 0xf) << 4) | 0;
	msg[1] = CEC_OC_SET_OSD_NAME;
	memcpy(&msg[2], cec_global_info.cec_node_info[index].osd_name, osd_len);

	cec_ll_tx(msg, 2 + osd_len);
}

void cec_vendor_cmd_with_id(struct cec_rx_message_t *pcec_message)
{
	;/* todo */
}


void cec_set_menu_language(struct cec_rx_message_t *pcec_message)
{
	unsigned char a, b, c;
	unsigned int lang;
	unsigned char index = cec_global_info.my_node_index;
	unsigned char src_log_addr = (pcec_message->content.msg.header
		>> 4) & 0xf;

	lang = cec_global_info.cec_node_info[index].menu_lang;
	a = (lang >> 16) & 0xff;
	b = (lang >> 8) & 0xff;
	c = (lang >> 0) & 0xff;
	if (0x0 == src_log_addr) {
		cec_global_info.cec_node_info[index].menu_lang =
		    (int)((pcec_message->content.msg.operands[0] << 16)
		    | (pcec_message->content.msg.operands[1] <<  8)
		    | (pcec_message->content.msg.operands[2]));
		switch_set_state(&lang_dev,
		    cec_global_info.cec_node_info[index].menu_lang);
		cec_global_info.cec_node_info[index].real_info_mask
		    |= INFO_MASK_MENU_LANGUAGE;
		hdmi_print(INF, CEC "cec_set_menu_language:%c.%c.%c\n",
			a, b, c);
	}
}

void cec_handle_message(struct cec_rx_message_t *pcec_message)
{
	unsigned char	brdcst, opcode;
	unsigned char	initiator, follower;
	unsigned char   operand_num;
	unsigned char   msg_length;

	/* parse message */
	if ((!pcec_message) || (check_cec_msg_valid(pcec_message) == 0))
		return;

	initiator	= pcec_message->content.msg.header >> 4;
	follower	= pcec_message->content.msg.header & 0x0f;
	opcode		= pcec_message->content.msg.opcode;
	operand_num = pcec_message->operand_num;
	brdcst	  = (follower == 0x0f);
	msg_length  = pcec_message->msg_length;

	if (0 == pcec_message->content.msg.header)
		return;

	/* process messages from tv polling and cec devices */
	if (CEC_OC_GIVE_OSD_NAME == opcode)
		cec_set_osd_name(pcec_message);

	if (hdmitx_device->cec_func_config & (1 << CEC_FUNC_MSAK)) {
		switch (opcode) {
		case CEC_OC_ACTIVE_SOURCE:
			cec_active_source_rx(pcec_message);
			break;
		case CEC_OC_INACTIVE_SOURCE:
			break;
		case CEC_OC_CEC_VERSION:
			break;
		case CEC_OC_DECK_STATUS:
			break;
		case CEC_OC_DEVICE_VENDOR_ID:
			break;
		case CEC_OC_FEATURE_ABORT:
			break;
		case CEC_OC_GET_CEC_VERSION:
			cec_get_version(pcec_message);
			break;
		case CEC_OC_GIVE_DECK_STATUS:
			cec_give_deck_status(pcec_message);
			break;
		case CEC_OC_MENU_STATUS:
			cec_menu_status_smp_irq(pcec_message);
			break;
		case CEC_OC_REPORT_PHYSICAL_ADDRESS:
			break;
		case CEC_OC_REPORT_POWER_STATUS:
			break;
		case CEC_OC_SET_OSD_NAME:
			break;
		case CEC_OC_VENDOR_COMMAND_WITH_ID:
			break;
		case CEC_OC_SET_MENU_LANGUAGE:
			cec_set_menu_language(pcec_message);
			break;
		case CEC_OC_GIVE_PHYSICAL_ADDRESS:
			cec_report_physical_address_smp();
			break;
		case CEC_OC_GIVE_DEVICE_VENDOR_ID:
			cec_device_vendor_id(pcec_message);
			break;
		case CEC_OC_GIVE_OSD_NAME:
			break;
		case CEC_OC_STANDBY:
			cec_inactive_source_rx(pcec_message);
			cec_standby(pcec_message);
			break;
		case CEC_OC_SET_STREAM_PATH:
			cec_set_stream_path(pcec_message);
			break;
		case CEC_OC_REQUEST_ACTIVE_SOURCE:
			if (cec_global_info.cec_node_info[cec_global_info
			    .my_node_index].menu_status != DEVICE_MENU_ACTIVE)
				break;
			cec_active_source_smp();
			break;
		case CEC_OC_GIVE_DEVICE_POWER_STATUS:
			cec_report_power_status(pcec_message);
			break;
		case CEC_OC_USER_CONTROL_PRESSED:
			break;
		case CEC_OC_USER_CONTROL_RELEASED:
			break;
		case CEC_OC_IMAGE_VIEW_ON:/* not support in source */
			/* Wakeup TV */
			cec_usrcmd_set_imageview_on(CEC_TV_ADDR);
			break;
		case CEC_OC_ROUTING_CHANGE:
			cec_routing_change(pcec_message);
			break;
		case CEC_OC_ROUTING_INFORMATION:
			cec_routing_information(pcec_message);
			break;
		case CEC_OC_GIVE_AUDIO_STATUS:
			cec_report_audio_status();
			break;
		case CEC_OC_MENU_REQUEST:
			cec_menu_status(pcec_message);
			break;
		case CEC_OC_PLAY:
			hdmi_print(INF, CEC "CEC_OC_PLAY:0x%x\n",
			    pcec_message->content.msg.operands[0]);
			switch (pcec_message->content.msg.operands[0]) {
			case 0x24:
				input_event(cec_global_info.remote_cec_dev,
				    EV_KEY, KEY_PLAYPAUSE, 1);
				input_sync(cec_global_info.remote_cec_dev);
				input_event(cec_global_info.remote_cec_dev,
				    EV_KEY, KEY_PLAYPAUSE, 0);
				input_sync(cec_global_info.remote_cec_dev);
				break;
			case 0x25:
				input_event(cec_global_info.remote_cec_dev,
				    EV_KEY, KEY_PLAYPAUSE, 1);
				input_sync(cec_global_info.remote_cec_dev);
				input_event(cec_global_info.remote_cec_dev,
				    EV_KEY, KEY_PLAYPAUSE, 0);
				input_sync(cec_global_info.remote_cec_dev);
				break;
			default:
				break;
			}
			break;
		case CEC_OC_DECK_CONTROL:
			hdmi_print(INF, CEC "CEC_OC_DECK_CONTROL:0x%x\n",
			    pcec_message->content.msg.operands[0]);
			switch (pcec_message->content.msg.operands[0]) {
			case 0x3:
				input_event(cec_global_info.remote_cec_dev,
				    EV_KEY, KEY_STOP, 1);
				input_sync(cec_global_info.remote_cec_dev);
				input_event(cec_global_info.remote_cec_dev,
				    EV_KEY, KEY_STOP, 0);
				input_sync(cec_global_info.remote_cec_dev);
				break;
			default:
				break;
			}
			break;
		case CEC_OC_GET_MENU_LANGUAGE:
		case CEC_OC_VENDOR_REMOTE_BUTTON_DOWN:
		case CEC_OC_VENDOR_REMOTE_BUTTON_UP:
		case CEC_OC_CLEAR_ANALOGUE_TIMER:
		case CEC_OC_CLEAR_DIGITAL_TIMER:
		case CEC_OC_CLEAR_EXTERNAL_TIMER:
		case CEC_OC_GIVE_SYSTEM_AUDIO_MODE_STATUS:
		case CEC_OC_GIVE_TUNER_DEVICE_STATUS:
		case CEC_OC_SET_OSD_STRING:
		case CEC_OC_SET_SYSTEM_AUDIO_MODE:
		case CEC_OC_SET_TIMER_PROGRAM_TITLE:
		case CEC_OC_SYSTEM_AUDIO_MODE_REQUEST:
		case CEC_OC_SYSTEM_AUDIO_MODE_STATUS:
		case CEC_OC_TEXT_VIEW_ON:
		case CEC_OC_TIMER_CLEARED_STATUS:
		case CEC_OC_TIMER_STATUS:
		case CEC_OC_TUNER_DEVICE_STATUS:
		case CEC_OC_TUNER_STEP_DECREMENT:
		case CEC_OC_TUNER_STEP_INCREMENT:
		case CEC_OC_VENDOR_COMMAND:
		case CEC_OC_SELECT_ANALOGUE_SERVICE:
		case CEC_OC_SELECT_DIGITAL_SERVICE:
		case CEC_OC_SET_ANALOGUE_TIMER:
		case CEC_OC_SET_AUDIO_RATE:
		case CEC_OC_SET_DIGITAL_TIMER:
		case CEC_OC_SET_EXTERNAL_TIMER:
		case CEC_OC_RECORD_OFF:
		case CEC_OC_RECORD_ON:
		case CEC_OC_RECORD_STATUS:
		case CEC_OC_RECORD_TV_SCREEN:
		case CEC_OC_REPORT_AUDIO_STATUS:
		case CEC_OC_ABORT_MESSAGE:
			cec_feature_abort(pcec_message);
			break;
		default:
			break;
		}
	}
}


/* --------------- cec command from user application -------------------- */

void cec_usrcmd_parse_all_dev_online(void)
{
/* todo; */
}

void cec_usrcmd_get_cec_version(unsigned char log_addr)
{
	MSG_P0(cec_global_info.my_node_index, log_addr,
			CEC_OC_GET_CEC_VERSION);

	cec_ll_tx(gbl_msg, 2);
}

void cec_usrcmd_get_audio_status(unsigned char log_addr)
{
	MSG_P0(cec_global_info.my_node_index,
		log_addr,
		CEC_OC_GIVE_AUDIO_STATUS);

	cec_ll_tx(gbl_msg, 2);
}

void cec_usrcmd_get_deck_status(unsigned char log_addr)
{
	MSG_P1(cec_global_info.my_node_index, log_addr, CEC_OC_GIVE_DECK_STATUS,
	    STATUS_REQ_ON);

	cec_ll_tx(gbl_msg, 3);
}

void cec_usrcmd_set_deck_cnt_mode(unsigned char log_addr,
	enum deck_cnt_mode_e deck_cnt_mode)
{
	MSG_P1(cec_global_info.my_node_index, log_addr,
	    CEC_OC_DECK_CONTROL, deck_cnt_mode);

	cec_ll_tx(gbl_msg, 3);
}

void cec_usrcmd_get_device_power_status(unsigned char log_addr)
{
	MSG_P0(cec_global_info.my_node_index, log_addr,
	    CEC_OC_GIVE_DEVICE_POWER_STATUS);

	cec_ll_tx(gbl_msg, 2);
}

void cec_usrcmd_get_device_vendor_id(unsigned char log_addr)
{
	MSG_P0(cec_global_info.my_node_index, log_addr,
	    CEC_OC_GIVE_DEVICE_VENDOR_ID);

	cec_ll_tx(gbl_msg, 2);
}

void cec_usrcmd_get_osd_name(unsigned char log_addr)
{
	MSG_P0(cec_global_info.my_node_index, log_addr,
	    CEC_OC_GIVE_OSD_NAME);

	cec_ll_tx(gbl_msg, 2);
}

void cec_usrcmd_get_physical_address(unsigned char log_addr)
{
	MSG_P0(cec_global_info.my_node_index, log_addr,
	    CEC_OC_GIVE_PHYSICAL_ADDRESS);

	cec_ll_tx(gbl_msg, 2);
}

void cec_usrcmd_get_system_audio_mode_status(unsigned char log_addr)
{
	MSG_P0(cec_global_info.my_node_index,
	    log_addr, CEC_OC_GIVE_SYSTEM_AUDIO_MODE_STATUS);

	cec_ll_tx(gbl_msg, 2);
}

void cec_usrcmd_set_standby(unsigned char log_addr)
{
	MSG_P0(cec_global_info.my_node_index, log_addr, CEC_OC_STANDBY);

	cec_ll_tx(gbl_msg, 2);
}


void cec_usrcmd_set_imageview_on(unsigned char log_addr)
{
	MSG_P0(cec_global_info.my_node_index, log_addr,
			CEC_OC_IMAGE_VIEW_ON);

	cec_ll_tx(gbl_msg, 2);
}

void cec_usrcmd_text_view_on(unsigned char log_addr)
{
	MSG_P0(cec_global_info.my_node_index, log_addr,
			CEC_OC_TEXT_VIEW_ON);

	cec_ll_tx(gbl_msg, 2);
}

void cec_usrcmd_get_tuner_device_status(unsigned char log_addr)
{
	MSG_P0(cec_global_info.my_node_index, log_addr,
	    CEC_OC_GIVE_TUNER_DEVICE_STATUS);

	cec_ll_tx(gbl_msg, 2);
}

void cec_usrcmd_set_play_mode(unsigned char log_addr,
	enum play_mode_e play_mode)
{
	MSG_P1(cec_global_info.my_node_index, log_addr, CEC_OC_PLAY, play_mode);

	cec_ll_tx(gbl_msg, 3);
}

void cec_usrcmd_get_menu_state(unsigned char log_addr)
{
	MSG_P1(cec_global_info.my_node_index, log_addr,
	    CEC_OC_MENU_REQUEST, MENU_REQ_QUERY);

	cec_ll_tx(gbl_msg, 3);
}

void cec_usrcmd_set_menu_state(unsigned char log_addr,
	enum menu_req_type_e menu_req_type)
{
	MSG_P1(cec_global_info.my_node_index, log_addr,
	    CEC_OC_MENU_REQUEST, menu_req_type);

	cec_ll_tx(gbl_msg, 3);
}

void cec_usrcmd_get_menu_language(unsigned char log_addr)
{
	MSG_P0(cec_global_info.my_node_index,
		log_addr,
		CEC_OC_GET_MENU_LANGUAGE);

	cec_ll_tx(gbl_msg, 2);
}

void cec_usrcmd_get_active_source(void)
{
	MSG_P0(cec_global_info.my_node_index,
		0xF,
		CEC_OC_REQUEST_ACTIVE_SOURCE);

	cec_ll_tx(gbl_msg, 2);
}

void cec_usrcmd_set_active_source(void)
{
	unsigned char index = cec_global_info.my_node_index;
	unsigned char phy_addr_ab = (cec_phyaddr_config(0, 0) >> 8) & 0xff;
	unsigned char phy_addr_cd = cec_phyaddr_config(0, 0) & 0xff;

	MSG_P2(index, CEC_BROADCAST_ADDR,
			CEC_OC_ACTIVE_SOURCE,
			phy_addr_ab,
			phy_addr_cd);

	cec_ll_tx(gbl_msg, 4);
}

void cec_usrcmd_set_deactive_source(unsigned char log_addr)
{
	unsigned char phy_addr_ab = (cec_phyaddr_config(0, 0) >> 8) & 0xff;
	unsigned char phy_addr_cd = cec_phyaddr_config(0, 0) & 0xff;

	MSG_P2(cec_global_info.my_node_index, log_addr, CEC_OC_INACTIVE_SOURCE,
		   phy_addr_ab,
		   phy_addr_cd);

	cec_ll_tx(gbl_msg, 4);
}

void cec_usrcmd_clear_node_dev_real_info_mask(unsigned char log_addr,
	unsigned long mask)
{
	cec_global_info.cec_node_info[log_addr].real_info_mask &= ~mask;
}


void cec_usrcmd_set_osd_name(struct cec_rx_message_t *pcec_message)
{

	unsigned char log_addr = pcec_message->content.msg.header >> 4;
	unsigned char index = cec_global_info.my_node_index;

	MSG_P14(index, log_addr,
			CEC_OC_SET_OSD_NAME,
			cec_global_info.cec_node_info[index].osd_name[0],
			cec_global_info.cec_node_info[index].osd_name[1],
			cec_global_info.cec_node_info[index].osd_name[2],
			cec_global_info.cec_node_info[index].osd_name[3],
			cec_global_info.cec_node_info[index].osd_name[4],
			cec_global_info.cec_node_info[index].osd_name[5],
			cec_global_info.cec_node_info[index].osd_name[6],
			cec_global_info.cec_node_info[index].osd_name[7],
			cec_global_info.cec_node_info[index].osd_name[8],
			cec_global_info.cec_node_info[index].osd_name[9],
			cec_global_info.cec_node_info[index].osd_name[10],
			cec_global_info.cec_node_info[index].osd_name[11],
			cec_global_info.cec_node_info[index].osd_name[12],
			cec_global_info.cec_node_info[index].osd_name[13]);

	cec_ll_tx(gbl_msg, 16);
}



void cec_usrcmd_set_device_vendor_id(void)
{
	unsigned char index = cec_global_info.my_node_index;

	MSG_P3(index,
		CEC_BROADCAST_ADDR,
		CEC_OC_DEVICE_VENDOR_ID,
		(cec_global_info.cec_node_info[index].vendor_id >> 16) & 0xff,
		(cec_global_info.cec_node_info[index].vendor_id >> 8) & 0xff,
		(cec_global_info.cec_node_info[index].vendor_id >> 0) & 0xff);

	cec_ll_tx(gbl_msg, 5);
}
void cec_usrcmd_set_report_physical_address(void)
{
	unsigned char index = cec_global_info.my_node_index;
	unsigned char phy_addr_ab = (cec_phyaddr_config(0, 0) >> 8) & 0xff;
	unsigned char phy_addr_cd = cec_phyaddr_config(0, 0) & 0xff;

	MSG_P3(index, CEC_BROADCAST_ADDR,
		   CEC_OC_REPORT_PHYSICAL_ADDRESS,
		   phy_addr_ab,
		   phy_addr_cd,
		   CEC_PLAYBACK_DEVICE_TYPE);

	cec_ll_tx(gbl_msg, 5);
}

void cec_routing_change(struct cec_rx_message_t *pcec_message)
{
	unsigned int phy_addr_origin;
	unsigned int phy_addr_destination;

	phy_addr_origin =
	    (unsigned int)((pcec_message->content.msg.operands[0] << 8)
	    | (pcec_message->content.msg.operands[1] << 0));
	phy_addr_destination =
	    (unsigned int)((pcec_message->content.msg.operands[2] << 8)
	    | (pcec_message->content.msg.operands[3] << 0));

	if (phy_addr_destination == (cec_phyaddr_config(0, 0) & 0xffff)) {
		cec_global_info.cec_node_info[cec_global_info.my_node_index]
		    .menu_status = DEVICE_MENU_ACTIVE;
	} else {
		cec_global_info.cec_node_info[cec_global_info.my_node_index]
		    .menu_status = DEVICE_MENU_INACTIVE;
	}
}

void cec_routing_information(struct cec_rx_message_t *pcec_message)
{
	unsigned char index = cec_global_info.my_node_index;
	unsigned char phy_addr_ab = (cec_phyaddr_config(0, 0) >> 8) & 0xff;
	unsigned char phy_addr_cd = cec_phyaddr_config(0, 0) & 0xff;
	unsigned int phy_addr_destination;
	unsigned char msg[4];

	phy_addr_destination =
	    (unsigned int)((pcec_message->content.msg.operands[0] << 8)
	    | (pcec_message->content.msg.operands[1] << 0));

	if (phy_addr_destination == (cec_phyaddr_config(0, 0) & 0xffff)) {
		cec_global_info.cec_node_info[cec_global_info.my_node_index]
		    .menu_status = DEVICE_MENU_ACTIVE;
		msg[0] = ((index & 0xf) << 4) | CEC_BROADCAST_ADDR;
		msg[1] = CEC_OC_ROUTING_INFORMATION;
		msg[2] = phy_addr_ab;
		msg[3] = phy_addr_cd;
		cec_ll_tx(msg, 4);
	} else {
		cec_global_info.cec_node_info[cec_global_info.my_node_index]
		    .menu_status = DEVICE_MENU_INACTIVE;
	}
}
/******************** cec middle level code end ***************************/


/************************ cec high level code *****************************/

static int __init cec_init(void)
{
	int i;

	hdmitx_device = get_hdmitx_device();
	switch_dev_register(&lang_dev);
	init_waitqueue_head(&hdmitx_device->cec_wait_rx);
	cec_key_init();
	hdmi_print(INF, CEC "CEC init\n");

	memset(&cec_global_info, 0, sizeof(struct cec_global_info_t));
	cec_global_info.cec_rx_msg_buf.rx_buf_size =
	    sizeof(cec_global_info.cec_rx_msg_buf.cec_rx_message)
	    /sizeof(cec_global_info.cec_rx_msg_buf.cec_rx_message[0]);

	cec_global_info.hdmitx_device = hdmitx_device;

	hdmitx_device->task_cec = kthread_run(cec_task,
	    (void *)hdmitx_device, "kthread_cec");

	hdmitx_setup_cecirq(hdmitx_device);

	cec_global_info.remote_cec_dev = input_allocate_device();
	if (!cec_global_info.remote_cec_dev)
		hdmi_print(INF, CEC "No enough memory\n");

	cec_global_info.remote_cec_dev->name = "cec_input";

	cec_global_info.remote_cec_dev->evbit[0] = BIT_MASK(EV_KEY);
	cec_global_info.remote_cec_dev->keybit[BIT_WORD(BTN_0)] =
		BIT_MASK(BTN_0);
	cec_global_info.remote_cec_dev->id.bustype = BUS_ISA;
	cec_global_info.remote_cec_dev->id.vendor = 0x1b8e;
	cec_global_info.remote_cec_dev->id.product = 0x0cec;
	cec_global_info.remote_cec_dev->id.version = 0x0001;

	for (i = 0; i < 128; i++)
		set_bit(cec_key_map[i], cec_global_info.remote_cec_dev->keybit);

	if (input_register_device(cec_global_info.remote_cec_dev)) {
		hdmi_print(INF, CEC "Failed to register device\n");
		input_free_device(cec_global_info.remote_cec_dev);
	}

	hdmitx_device->cec_init_ready = 1;
	hdmi_print(INF, CEC "hdmitx_device->cec_init_ready:0x%x",
	    hdmitx_device->cec_init_ready);
	return 0;
}

static void __exit cec_uninit(void)
{
	if (!(hdmitx_device->cec_func_config & (1 << CEC_FUNC_MSAK)))
		return;

	hdmi_print(INF, CEC "cec uninit!\n");
	if (cec_global_info.cec_flag.cec_init_flag == 1) {
		free_irq(hdmitx_device->irq_cec, (void *)hdmitx_device);
		kthread_stop(hdmitx_device->task_cec);
		cec_global_info.cec_flag.cec_init_flag = 0;
	}

	hdmitx_device->cec_init_ready = 0;
	input_unregister_device(cec_global_info.remote_cec_dev);
	cec_global_info.cec_flag.cec_fiq_flag = 0;
}

size_t cec_usrcmd_get_global_info(char *buf)
{
	int i = 0;
	int dev_num = 0;

	struct cec_node_info_t *buf_node_addr = (struct cec_node_info_t *)(buf +
		(unsigned int)(((struct cec_global_info_to_usr_t *)0)->
		cec_node_info_online));

	for (i = 0; i < MAX_NUM_OF_DEV; i++) {
		if (cec_global_info.dev_mask & (1 << i)) {
			memcpy(&(buf_node_addr[dev_num]),
				&(cec_global_info.cec_node_info[i]),
				sizeof(struct cec_node_info_t));
			dev_num++;
		}
	}

	buf[0] = dev_num;
	buf[1] = cec_global_info.active_log_dev;
#if 0
	hdmi_print(INF, CEC "\n");
	hdmi_print(INF, CEC "%x\n",
		(unsigned int)(((struct cec_global_info_to_usr_t *)0)->
		cec_node_info_online));
	hdmi_print(INF, CEC "%x\n", ((struct cec_global_info_to_usr_t *)buf)->
		dev_number);
	hdmi_print(INF, CEC "%x\n",
	    ((struct cec_global_info_to_usr_t *)buf)->active_log_dev);
	hdmi_print(INF, CEC "%x\n", ((struct cec_global_info_to_usr_t *)buf)->
		cec_node_info_online[0].hdmi_port);
	for (i = 0; i < (sizeof(struct cec_node_info_t) * dev_num) + 2; i++)
		hdmi_print(INF, CEC "%x,", buf[i]);

	hdmi_print(INF, CEC "\n");
#endif
	return (size_t)(sizeof(struct cec_node_info_t) * dev_num
		+ (unsigned int)((struct cec_global_info_to_usr_t *)0)->
		cec_node_info_online);
}

void cec_usrcmd_set_lang_config(const char *buf, size_t count)
{
	int ret;
	unsigned long val;
	char tmpbuf[128];
	int i = 0;

	while ((buf[i]) && (buf[i] != ',') && (buf[i] != ' ')) {
		tmpbuf[i] = buf[i];
		i++;
	}

	ret = kstrtoul(tmpbuf, 16, &val);
	cec_global_info.cec_node_info[cec_global_info.my_node_index].
		menu_lang = val;

}
void cec_usrcmd_set_config(const char *buf, size_t count)
{
	int ret;
	int i = 0;
	int j = 0;
	unsigned long value;
	char param[16] = {0};

	if (count > 32)
		hdmi_print(INF, CEC "too many args\n");

	for (i = 0; i < count; i++) {
		if ((buf[i] >= '0') && (buf[i] <= 'f')) {
			ret = kstrtoul(&buf[i], 16, (unsigned long *)&param[j]);
			j++;
		}
		while (buf[i] != ' ')
			i++;
	}
	value = cec_config(0, 0) & 0x1;
	cec_config(param[0], 1);
	hdmitx_device->cec_func_config = cec_config(0, 0);
	if (!(hdmitx_device->cec_func_config
	    & (1 << CEC_FUNC_MSAK)) || !hdmitx_device->hpd_state)
		return;

	if ((0 == (value & 0x1)) && (1 == (param[0] & 1))) {
		hdmitx_device->cec_init_ready = 1;
		hdmitx_device->hpd_state = 1;

		cec_node_init(hdmitx_device);
	}
	if ((1 == (param[0] & 1)) && (0x2 == (value & 0x2))
	    && (0x0 == (param[0] & 0x2)))
		cec_menu_status_smp(DEVICE_MENU_INACTIVE);

	if ((1 == (param[0] & 1)) && (0x0 == (value & 0x2))
	    && (0x2 == (param[0] & 0x2)))
		cec_active_source_smp();

	if ((0x20 == (param[0] & 0x20)) && (0x0 == (value & 0x20)))
		cec_get_menu_language_smp();

	hdmi_print(INF, CEC "cec_func_config:0x%x : 0x%x\n",
		hdmitx_device->cec_func_config, cec_config(0, 0));
}

unsigned int dispatch_buffer_parse(const char *buf, const char *string,
	char *param, size_t count)
{
	int ret;
	unsigned int i;
	unsigned int j;
	unsigned int len;

	j = 0;
	len = ('\0' == *string) ? 0 : 1;
	for (i = strlen(string) + len; i < count; i++) {
		ret = kstrtoul(&buf[i], 16, (unsigned long *)&param[j]);
		pr_info("param[%d]:0x%x\n", j, param[j]);
		j++;
		while ((buf[i] != ' ') && (buf[i] != ','))
			i++;
	}
	param[j] = 0;
	return j;
}

void cec_usrcmd_set_dispatch(const char *buf, size_t count)
{
	int ret;
	int bool = 0;
	unsigned int addr;
	unsigned int value;
	unsigned bit_set;
	unsigned time_set;
	char msg[4] = {0};
	char param[32] = {0};

	hdmi_print(INF, CEC "cec usrcmd set dispatch start:\n");
	/* if none HDMI out,no CEC features. */
	if (!hdmitx_device->hpd_state) {
		hdmi_print(INF, CEC "HPD low!\n");
		return;
	}

	if (!(hdmitx_device->cec_func_config & (1 << CEC_FUNC_MSAK))) {
		hdmi_print(INF, CEC "cec function masked!\n");
		return;
	}

	if (count > 32)
		hdmi_print(INF, CEC "too many args\n");

	if (!strncmp(buf, "settiming", 9)) {
		ret = kstrtoul(buf+9, 16, (unsigned long *)&bit_set);
		ret = kstrtoul(buf+11, 16, (unsigned long *)&time_set);
		cec_arbit_bit_time_set(bit_set, time_set, 1);
		return;
	} else if (!strncmp(buf, "getttiming", 9)) {
		cec_arbit_bit_time_read();
		return;
	} else if (!strncmp(buf, "dumpaocecreg", 12)) {
		dumpaocecreg();
		return;
	} else if (!strncmp(buf, "waocec", 6)) {
		ret = kstrtoul(buf+6, 16, (unsigned long *)&addr);
		ret = kstrtoul(buf+9, 16, (unsigned long *)&value);
		waocec(addr, value);
		return;
	} else if (!strncmp(buf, "raocec", 6)) {
		ret = kstrtoul(buf+6, 16, (unsigned long *)&addr);
		raocec(addr);
		return;
	}

	dispatch_buffer_parse(buf, "", param, count);
	switch (param[0]) {
	case GET_CEC_VERSION:   /* 0 LA */
		cec_usrcmd_get_cec_version(param[1]);
		break;
	case GET_DEV_POWER_STATUS:
		cec_usrcmd_get_device_power_status(param[1]);
		break;
	case GET_DEV_VENDOR_ID:
		cec_usrcmd_get_device_vendor_id(param[1]);
		break;
	case GET_OSD_NAME:
		cec_usrcmd_get_osd_name(param[1]);
		break;
	case GET_PHYSICAL_ADDR:
		cec_usrcmd_get_physical_address(param[1]);
		break;
	case SET_STANDBY:	   /* d LA */
		cec_usrcmd_set_standby(param[1]);
		break;
	case SET_IMAGEVIEW_ON:  /* e LA */
		cec_usrcmd_set_imageview_on(param[1]);
		break;
	case GIVE_DECK_STATUS:
		cec_usrcmd_get_deck_status(param[1]);
		break;
	case SET_DECK_CONTROL_MODE:
		cec_usrcmd_set_deck_cnt_mode(param[1], param[2]);
		break;
	case SET_PLAY_MODE:
		cec_usrcmd_set_play_mode(param[1], param[2]);
		break;
	case GET_SYSTEM_AUDIO_MODE:
		cec_usrcmd_get_system_audio_mode_status(param[1]);
		break;
	case GET_TUNER_DEV_STATUS:
		cec_usrcmd_get_tuner_device_status(param[1]);
		break;
	case GET_AUDIO_STATUS:
		cec_usrcmd_get_audio_status(param[1]);
		break;
	case GET_OSD_STRING:
		break;
	case GET_MENU_STATE:
		cec_usrcmd_get_menu_state(param[1]);
		break;
	case SET_MENU_STATE:
		cec_usrcmd_set_menu_state(param[1], param[2]);
		break;
	case SET_MENU_LANGAGE:
		break;
	case GET_MENU_LANGUAGE:
		cec_usrcmd_get_menu_language(param[1]);
		break;
	case GET_ACTIVE_SOURCE:	 /* 13 */
		cec_usrcmd_get_active_source();
		break;
	case SET_ACTIVE_SOURCE:
		cec_usrcmd_set_active_source();
		break;
	case SET_DEACTIVE_SOURCE:
		cec_usrcmd_set_deactive_source(param[1]);
		break;
	case REPORT_PHYSICAL_ADDRESS:	/* 17 */
		cec_usrcmd_set_report_physical_address();
		break;
	case SET_TEXT_VIEW_ON:		  /* 18 LA */
		cec_usrcmd_text_view_on(param[1]);
		break;
	case POLLING_ONLINE_DEV:	/* 19 LA */
		cec_polling_online_dev(param[1], &bool);
		break;
	case CEC_OC_MENU_STATUS:
		cec_menu_status_smp(DEVICE_MENU_INACTIVE);
		break;
	case CEC_OC_ABORT_MESSAGE:
		msg[0] = 0x40;
		msg[1] = CEC_OC_FEATURE_ABORT;
		msg[2] = 0;
		msg[3] = CEC_UNRECONIZED_OPCODE;
		cec_ll_tx(msg, 4);
		break;
	case PING_TV:	/* 0x1a LA : For TV CEC detected. */
		detect_tv_support_cec(param[1]);
		break;
	default:
		break;
	}
	hdmi_print(INF, CEC "cec usrcmd set dispatch end!\n");
}

/*************************** cec high level code end ************************/

late_initcall(cec_init);
module_exit(cec_uninit);
MODULE_DESCRIPTION("AMLOGIC HDMI TX CEC driver");
MODULE_LICENSE("GPL");
/* MODULE_LICENSE("Dual BSD/GPL"); */
/* MODULE_VERSION("1.0.0"); */

MODULE_PARM_DESC(cec_msg_dbg_en, "\n cec_msg_dbg_en\n");
module_param(cec_msg_dbg_en, bool, 0664);





