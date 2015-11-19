/*
 * drivers/amlogic/hdmi/hdmi_tx_20/hdmi_tx_cec.c
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
#include <linux/amlogic/tvin/tvin.h>
#include <linux/wakelock_android.h>

#include <linux/amlogic/hdmi_tx/hdmi_tx_cec.h>
#include <linux/amlogic/hdmi_tx/hdmi_tx_module.h>
#include <linux/of.h>
#include <linux/pinctrl/consumer.h>
#include <linux/of_irq.h>
#include "hw/mach_reg.h"
#include "hw/hdmi_tx_reg.h"
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/amlogic/pm.h>
#include <linux/of_address.h>

#define CEC_FRAME_DELAY		msecs_to_jiffies(400)
#define CEC_DEV_NAME		"cec"

static void __iomem *exit_reg;
static struct hdmitx_dev *hdmitx_device;
static struct workqueue_struct *cec_workqueue;
static unsigned char rx_msg[MAX_MSG];
static unsigned char rx_len;
static struct completion menu_comp;
static struct completion rx_complete;
#ifdef CONFIG_PM
static int cec_suspend;
static int cec_enter_standby;
#endif

static struct device *dbg_dev;
#define CEC_DEBUG	1

#define CEC_ERR(format, args...)			\
	{if (CEC_DEBUG && dbg_dev)			\
		dev_err(dbg_dev, format, ##args);	\
	}

#define CEC_INFO(format, args...)			\
	{if (CEC_DEBUG && dbg_dev)			\
		dev_info(dbg_dev, format, ##args);	\
	}

DEFINE_SPINLOCK(cec_input_key);

/* global variables */
static unsigned char gbl_msg[MAX_MSG];
struct cec_global_info_t cec_global_info;
unsigned char rc_long_press_pwr_key = 0;
EXPORT_SYMBOL(rc_long_press_pwr_key);
bool cec_msg_dbg_en = 0;

ssize_t cec_lang_config_state(struct switch_dev *sdev, char *buf)
{
	int pos = 0;
	int retry = 0;
	int idx = cec_global_info.my_node_index;
	int ret = 0, language;

	/*
	 * try to get tv menu language if we don't know which language
	 * current is
	 */
	if (!(hdmitx_device->cec_func_config & (1 << CEC_FUNC_MSAK)))
		return 0;
	while (retry < 5 && !cec_global_info.cec_node_info[idx].menu_lang) {
		cec_get_menu_language_smp();
		ret = wait_for_completion_timeout(&menu_comp, CEC_FRAME_DELAY);
		if (ret)
			break;
		retry++;
	}

	language = cec_global_info.cec_node_info[idx].menu_lang;
	pos += snprintf(buf + pos, PAGE_SIZE, "%c%c%c\n",
			(language >> 16) & 0xff,
			(language >>  8) & 0xff,
			(language >>  0) & 0xff);
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
	int phy_addr = 0;
	cec_global_info.cec_node_info[cec_global_info.my_node_index].
		power_status = POWER_STANDBY;
	cec_enter_standby = 1;
	phy_addr = cec_phyaddr_config(0, 0) & 0xffff;
	/*
	 * if phy_addr show device is serial connected to tv
	 * (such as TV <-> hdmi switch <-> mbox), we should try to wake up
	 * other devices connected between tv and mbox
	 */
	if (!hdmitx_device->hpd_state && !(phy_addr & 0xfff)) {
		CEC_INFO("HPD low!\n");
		return;
	}
	if (hdmitx_device->cec_func_config & (1 << CEC_FUNC_MSAK)) {
		cec_report_power_status(NULL);
		cec_menu_status_smp(DEVICE_MENU_INACTIVE);
		cec_inactive_source();
		if (rc_long_press_pwr_key == 1) {
			cec_set_standby();
			CEC_INFO("power-off from Romote Control\n");
			rc_long_press_pwr_key = 0;
		}
	}
	return;
}

static void hdmitx_cec_late_resume(struct early_suspend *h)
{
	int phy_addr = 0;
	cec_enable_irq();
	if (cec_rx_buf_check())
		cec_rx_buf_clear();

	hdmitx_device->hpd_state =
		hdmitx_device->HWOp.CntlMisc(hdmitx_device, MISC_HPD_GPI_ST, 0);
	cec_suspend = 0;
	cec_enter_standby = 0;
	cec_global_info.cec_node_info[cec_global_info.my_node_index].
		power_status = POWER_ON;
	phy_addr = cec_phyaddr_config(0, 0) & 0xffff;
	/*
	 * if phy_addr show device is serial connected to tv
	 * (such as TV <-> hdmi switch <-> mbox), we should try to wake up
	 * other devices connected between tv and mbox
	 */
	if (!hdmitx_device->hpd_state && !(phy_addr & 0xfff)) {
		CEC_INFO("HPD low!\n");
		return;
	}

	if (hdmitx_device->cec_func_config & (1 << CEC_FUNC_MSAK)) {
		cec_hw_reset();/* for M8 CEC standby. */
		cec_imageview_on_smp();
		cec_active_source_smp();
		cec_menu_status_smp(DEVICE_MENU_ACTIVE);
	}
}

#endif

struct wake_lock cec_lock;
void cec_wake_lock(void)
{
	if (!cec_suspend)
		wake_lock(&cec_lock);
}

void cec_wake_unlock(void)
{
	if (!cec_suspend)
		wake_unlock(&cec_lock);
}

void cec_isr_post_process(void)
{
	int phy_addr = 0;

	phy_addr = cec_phyaddr_config(0, 0) & 0xffff;
	/*
	 * if phy_addr show device is serial connected to tv
	 * (such as TV <-> hdmi switch <-> mbox), we should try to wake up
	 * other devices connected between tv and mbox
	 */
	if (!hdmitx_device->hpd_state && !(phy_addr & 0xfff))
		return;

	/* isr post process */
	while (cec_global_info.cec_rx_msg_buf.rx_read_pos !=
	       cec_global_info.cec_rx_msg_buf.rx_write_pos) {
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
	list_for_each_entry_safe(p, ptmp, &cec_tx_msg_phead, list) {
		cec_ll_tx(p->msg, p->length);
		unregister_cec_tx_msg(p);
	}
}

static int detect_tv_support_cec(unsigned addr)
{
	unsigned int ret = 0;
	unsigned char msg[1];
	msg[0] = (addr << 4) | 0x0;	/* 0x0, TV's root address */
	cec_polling_online_dev(msg[0], &ret);
	hdmi_print(LOW, CEC "TV %s support CEC\n", ret ? "is" : "not");
	hdmitx_device->tv_cec_support = ret;
	return hdmitx_device->tv_cec_support;
}

/*
 * cec hw module init befor allocate logical address
 */
static void cec_pre_init(void)
{
	cec_pinmux_set();
	ao_cec_init();

	cec_arbit_bit_time_set(3, 0x118, 0);
	cec_arbit_bit_time_set(5, 0x000, 0);
	cec_arbit_bit_time_set(7, 0x2aa, 0);
}

static int cec_try_to_wakeup_tv(void)
{
	int retry = 0;

	while (retry < 3) {
		retry++;
		cec_imageview_on_smp();
		msleep(5 * 1000);
		if (hdmitx_device->hdmi_info.vsdb_phy_addr.valid != 0)
			return 1;
	}
	return 0;
}

/*
 * should only used to allocate logical address
 */
int cec_node_init(struct hdmitx_dev *hdmitx_device)
{
	unsigned char a, b, c, d;
	struct vendor_info_data *vend_data = NULL;

	int i, bool = 0;
	int phy_addr_ok = 1;
	const enum _cec_log_dev_addr_e player_dev[3] = {
		CEC_PLAYBACK_DEVICE_1_ADDR,
		CEC_PLAYBACK_DEVICE_2_ADDR,
		CEC_PLAYBACK_DEVICE_3_ADDR,
	};

	unsigned long cec_phy_addr;

	/* If no connect, return directly */
	if ((hdmitx_device->cec_init_ready == 0) ||
		(hdmitx_device->hpd_state == 0)) {
		return -1;
	}

	cec_pre_init();
	if (cec_global_info.hal_ctl)
		return 0;

	if (hdmitx_device->config_data.vend_data)
		vend_data = hdmitx_device->config_data.vend_data;

	if ((vend_data) && (vend_data->cec_osd_string)) {
		i = strlen(vend_data->cec_osd_string);
		if (i > 14) {
			/* OSD string length must be less than 14 bytes */
			vend_data->cec_osd_string[14] = '\0';
		}
		osd_name = vend_data->cec_osd_string;
	}

	if ((vend_data) && (vend_data->vendor_id))
		vendor_id = (vend_data->vendor_id) & 0xffffff;

	/*
	 * if TV is not support CEC, we do not need to try allocate CEC
	 * logical address
	 */
	if (!(hdmitx_device->tv_cec_support) && (!detect_tv_support_cec(0xE)))
		return -1;

	if (!(hdmitx_device->cec_func_config & (1 << CEC_FUNC_MSAK)))
		return -1;

	CEC_INFO("cec_func_config: 0x%x; cec_config:0x%x\n",
		 hdmitx_device->cec_func_config, cec_config(0, 0));

	a = hdmitx_device->hdmi_info.vsdb_phy_addr.a;
	b = hdmitx_device->hdmi_info.vsdb_phy_addr.b;
	c = hdmitx_device->hdmi_info.vsdb_phy_addr.c;
	d = hdmitx_device->hdmi_info.vsdb_phy_addr.d;

	cec_phy_addr = ((a << 12) | (b << 8) | (c << 4) | (d << 0));

	for (i = 0; i < 3; i++) {
		CEC_INFO("CEC: start poll dev\n");
		cec_polling_online_dev(player_dev[i], &bool);
		CEC_INFO("player_dev[%d]:0x%x\n", i, player_dev[i]);
		if (bool == 0) {   /* 0 means that no any respond */
	/* If VSDB is not valid, use last or default physical address. */
			cec_logicaddr_set(player_dev[i]);
			if (hdmitx_device->hdmi_info.vsdb_phy_addr.valid == 0) {
				phy_addr_ok = 0;
				CEC_INFO("invalid cec PhyAddr\n");
				if (cec_try_to_wakeup_tv()) {
					CEC_INFO("try to wake up tv success\n");
				} else if (cec_phyaddr_config(0, 0)) {
					CEC_INFO("use last physical address\n");
				} else {
					cec_phyaddr_config(0x2000, 1);
					CEC_INFO("use Def Phy address\n");
				}
			} else
				cec_phyaddr_config(cec_phy_addr, 1);

			CEC_INFO("physical address:0x%x\n",
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

			CEC_INFO("Set logical address: %d\n", player_dev[i]);
			cec_global_info.cec_node_info[cec_global_info.
				my_node_index].power_status = POWER_ON;
			if (cec_global_info.cec_node_info[
				cec_global_info.my_node_index].
				menu_status == DEVICE_MENU_INACTIVE)
				break;
			msleep(100);
			if (phy_addr_ok) {
				cec_report_physical_address_smp();
				msleep(150);
			}
			cec_device_vendor_id((struct cec_rx_message_t *)0);

			cec_imageview_on_smp();

			cec_active_source_smp();

			cec_menu_status_smp(DEVICE_MENU_ACTIVE);

			msleep(100);
			cec_get_menu_language_smp();

			cec_global_info.cec_node_info[cec_global_info.
				my_node_index].menu_status =
					DEVICE_MENU_ACTIVE;
			break;
		}
	}
	if (bool == 1) {
		CEC_INFO("Can't get a valid logical address\n");
		return -1;
	} else {
		CEC_INFO("cec node init: cec features ok !\n");
		return 0;
	}
}

void cec_node_uninit(struct hdmitx_dev *hdmitx_device)
{
	if (!(hdmitx_device->cec_func_config & (1 << CEC_FUNC_MSAK)))
		return;
	cec_global_info.cec_node_info[cec_global_info.my_node_index].
		power_status = TRANS_ON_TO_STANDBY;
	CEC_INFO("cec node uninit!\n");
	cec_global_info.cec_node_info[cec_global_info.my_node_index].
		power_status = POWER_STANDBY;
}

static int cec_late_check_rx_buffer(void)
{
	int ret;
	struct delayed_work *dwork = &hdmitx_device->cec_work;

	ret = cec_rx_buf_check();
	if (!ret)
		return 0;
	/*
	 * start another check if rx buffer is full
	 */
	if ((-1) == cec_ll_rx(rx_msg, &rx_len)) {
		CEC_INFO("buffer got unrecorgnized msg\n");
		cec_rx_buf_clear();
		return 0;
	} else {
		register_cec_rx_msg(rx_msg, rx_len);
		mod_delayed_work(cec_workqueue, dwork, 0);
		return 1;
	}
}

static void cec_task(struct work_struct *work)
{
	struct hdmitx_dev *hdmitx_device;
	struct delayed_work *dwork;
	int    ret;

	hdmitx_device = container_of(work, struct hdmitx_dev, cec_work.work);
	dwork = &hdmitx_device->cec_work;
	/*
	 * some tv can't be ping OK if it's CEC funtion is disabled by
	 * TV settings, but when CEC function is reopened, we should
	 * init CEC logical address
	 */
	if (!cec_global_info.my_node_index) {
		ret = cec_node_init(hdmitx_device);
		if (ret < 0) {
			queue_delayed_work(cec_workqueue, dwork,
					   msecs_to_jiffies(20 * 1000));
			return;
		}
	}

	/*
	 * do not process cec task if not enabled
	 */
	if (!(hdmitx_device->cec_func_config & (1 << CEC_FUNC_MSAK)))
		return;

	cec_isr_post_process();
	if (!cec_late_check_rx_buffer())
		queue_delayed_work(cec_workqueue, dwork, CEC_FRAME_DELAY);
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

void cec_input_handle_message(void)
{
	unsigned char opernum;
	unsigned char follower;
	int pos = cec_global_info.cec_rx_msg_buf.rx_write_pos;
	unsigned char opcode =
	cec_global_info.cec_rx_msg_buf.cec_rx_message[pos].content.msg.opcode;

	if (NULL == hdmitx_device) {
		hdmitx_device = get_hdmitx_device();
		CEC_INFO("Error:hdmitx_device NULL!\n");
		return;
	}
	/* process key event messages from tv if not suspend */
	if ((hdmitx_device->cec_func_config & (1 << CEC_FUNC_MSAK)) &&
	    (!cec_suspend)) {
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
		case CEC_OC_USER_CONTROL_RELEASED:
			cec_user_control_released_irq();
			break;
		default:
			break;
		}
	} else if (cec_suspend) {
		/*
		 * Panasonic tv will send key power and power on for resume
		 * devices we should avoid send key again when device is
		 * resuming from uboot to prevent devicese suspend again
		 */
		CEC_INFO("under suspend state, ignore key\n");
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
		CEC_INFO("CEC: opcode & opernum not match: %x, %x\n",
			 opcode, opernum);
	}
	return rt;
}

irqreturn_t cec_isr_handler(int irq, void *dev_instance)
{
	struct hdmitx_dev *hdmitx;
	unsigned int intr_stat = 0;
	intr_stat = cec_intr_stat();

	if (intr_stat & (1<<1)) {   /* aocec tx intr */
		tx_irq_handle();
		return IRQ_HANDLED;
	}
	if ((-1) == cec_ll_rx(rx_msg, &rx_len))
		return IRQ_HANDLED;

	complete(&rx_complete);
	if (!cec_suspend && !cec_global_info.hal_ctl) {
		/* do not process messages if suspend and controled by hal */
		hdmitx = (struct hdmitx_dev *)dev_instance;
		register_cec_rx_msg(rx_msg, rx_len);
		mod_delayed_work(cec_workqueue, &hdmitx->cec_work, 0);
	}
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
		/*
		 * some types of TV such as panasonic need to send menu status,
		 * otherwise it will not send remote key event to control
		 * device's menu
		 */
		cec_menu_status_smp(DEVICE_MENU_ACTIVE);
		cec_global_info.cec_node_info[cec_global_info.my_node_index].
			menu_status = DEVICE_MENU_ACTIVE;
		if (cec_global_info.cec_node_info[cec_global_info.
			my_node_index].power_status == POWER_STANDBY) {
			CEC_INFO("exit standby by set stream path\n");
			input_event(cec_global_info.remote_cec_dev,
				    EV_KEY, KEY_POWER, 1);
			input_sync(cec_global_info.remote_cec_dev);
			input_event(cec_global_info.remote_cec_dev,
				    EV_KEY, KEY_POWER, 0);
			input_sync(cec_global_info.remote_cec_dev);
		}
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
	/*
	 * if device is not set one-touch-standby, but tv goto standby, we
	 * should also tell tv that device is inactive.
	 */
	if (!(hdmitx_device->cec_func_config & (1 << ONE_TOUCH_STANDBY_MASK))) {
		cec_menu_status_smp(DEVICE_MENU_INACTIVE);
		cec_inactive_source();
	}
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
		CEC_INFO("cec_deck_status: %x\n", cec_global_info
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
	unsigned osd_len = strlen(osd_name);
	unsigned src_log_addr = (pcec_message->content.msg.header >> 4) & 0xf;
	unsigned char msg[16];

	if (0xf != src_log_addr) {
		msg[0] = ((index & 0xf) << 4) | src_log_addr;
		msg[1] = CEC_OC_SET_OSD_NAME;
		memcpy(&msg[2], osd_name, osd_len);

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

	if (0x0 == src_log_addr) {
		cec_global_info.cec_node_info[index].menu_lang =
		    (int)((pcec_message->content.msg.operands[0] << 16)
		    | (pcec_message->content.msg.operands[1] <<  8)
		    | (pcec_message->content.msg.operands[2]));
		lang = cec_global_info.cec_node_info[index].menu_lang;
		a = (lang >> 16) & 0xff;
		b = (lang >> 8) & 0xff;
		c = (lang >> 0) & 0xff;
		switch_set_state(&lang_dev,
		    cec_global_info.cec_node_info[index].menu_lang);
		cec_global_info.cec_node_info[index].real_info_mask
		    |= INFO_MASK_MENU_LANGUAGE;
		CEC_INFO("cec_set_menu_language:%c.%c.%c\n",
			a, b, c);
		complete(&menu_comp);
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
			/*
			 * platform already enter standby state,
			 * ignore this meesage
			 */
			if (!cec_enter_standby) {
				cec_enter_standby = 1;
				cec_standby(pcec_message);
			}
			cec_global_info.cec_node_info[cec_global_info.
				my_node_index].power_status = POWER_STANDBY;
			break;
		case CEC_OC_SET_STREAM_PATH:
			cec_set_stream_path(pcec_message);
			break;
		case CEC_OC_REQUEST_ACTIVE_SOURCE:
			if (cec_global_info.cec_node_info[cec_global_info
			    .my_node_index].menu_status != DEVICE_MENU_ACTIVE)
				break;
			/* do not active soruce when standby */
			if (cec_global_info.cec_node_info[cec_global_info.
				my_node_index].power_status == POWER_STANDBY)
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
			CEC_INFO("CEC_OC_PLAY:0x%x\n",
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
			CEC_INFO("CEC_OC_DECK_CONTROL:0x%x\n",
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

static struct notifier_block cec_reboot_nb;
static int cec_reboot(struct notifier_block *nb, unsigned long state, void *cmd)
{
	if (!hdmitx_device->hpd_state)
		return 0;
	if (hdmitx_device->cec_func_config & (1 << CEC_FUNC_MSAK)) {
		CEC_INFO("%s, notify reboot\n", __func__);
		cec_menu_status_smp(DEVICE_MENU_INACTIVE);
		cec_inactive_source();
	}
	return 0;
}

#if 0
static unsigned int set_cec_code(const char *buf, size_t count)
{
	char tmpbuf[128];
	int i = 0;
	int ret;
	/* int j; */
	unsigned int cec_code;
	/* unsigned int value=0; */

	while ((buf[i]) && (buf[i] != ',') && (buf[i] != ' ')) {
		tmpbuf[i] = buf[i];
	i++;
	}
	tmpbuf[i] = 0;

	ret = kstrtoul(tmpbuf, NULL, 16, &cec_code);

	input_event(remote_cec_dev, EV_KEY, cec_code, 1);
	input_event(remote_cec_dev, EV_KEY, cec_code, 0);
	input_sync(remote_cec_dev);
	return cec_code;
}
#endif

static ssize_t show_cec(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t t = 0;  /* cec_usrcmd_get_global_info(buf); */
	return t;
}

static ssize_t store_cec(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	cec_usrcmd_set_dispatch(buf, count);
	return count;
}

static ssize_t show_cec_config(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int pos = 0;
	pos += snprintf(buf+pos, PAGE_SIZE, "0x%x\n", cec_config(0, 0));

	return pos;
}

static ssize_t store_cec_config(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	if (cec_global_info.hal_ctl) {
		CEC_INFO("controled by hal, abort\n");
		return count;
	}
	cec_usrcmd_set_config(buf, count);
	return count;
}

static ssize_t store_cec_lang_config(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;
	unsigned long val;

	ret = kstrtoul(buf, 16, &val);
	cec_global_info.cec_node_info[cec_global_info.my_node_index].
		menu_lang = (int)val;
	cec_usrcmd_set_lang_config(buf, count);
	return count;
}

static ssize_t show_cec_lang_config(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int pos = 0;

	pos += snprintf(buf+pos, PAGE_SIZE, "%x\n", cec_global_info.
		cec_node_info[cec_global_info.my_node_index].menu_lang);
	return pos;
}

static ssize_t show_cec_active_status(struct device *dev,
				      struct device_attribute *attr,
				      char *buf)
{
	int pos = 0;
	int active = DEVICE_MENU_INACTIVE;
	char *str = "is not";

	if ((hdmitx_device->hpd_state) &&
	    (hdmitx_device->cec_func_config & (1 << CEC_FUNC_MSAK)) &&
	   (!cec_global_info.cec_node_info[cec_global_info.
		my_node_index].power_status)) {
		active = cec_global_info.cec_node_info[cec_global_info.
			 my_node_index].menu_status;
		if (active == DEVICE_MENU_ACTIVE)
			str = "is";
	}
	pr_debug("Mbox %s display on TV.\n", str);
	pos += snprintf(buf + pos, PAGE_SIZE, "%x\n", active);

	return pos;
}

static ssize_t show_tv_support_cec(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", hdmitx_device->tv_cec_support);
}

static DEVICE_ATTR(cec, S_IWUSR | S_IRUGO, show_cec, store_cec);
static DEVICE_ATTR(cec_config, S_IWUSR | S_IRUGO | S_IWGRP,
	show_cec_config, store_cec_config);
static DEVICE_ATTR(cec_active_status, S_IRUGO ,
	show_cec_active_status, NULL);
static DEVICE_ATTR(tv_support_cec, S_IRUGO ,
	show_tv_support_cec, NULL);
static DEVICE_ATTR(cec_lang_config, S_IWUSR | S_IRUGO | S_IWGRP,
	show_cec_lang_config, store_cec_lang_config);

/*
 * file operations for hal
 */

static int hdmitx_cec_open(struct inode *inode, struct file *file)
{
	cec_global_info.open_count++;
	if (cec_global_info.open_count) {
		cec_global_info.hal_ctl = 1;
		/* enable all cec features */
		cec_config(0x2f, 1);
	}
	return 0;
}

static int hdmitx_cec_release(struct inode *inode, struct file *file)
{
	cec_global_info.open_count--;
	if (!cec_global_info.open_count) {
		cec_global_info.hal_ctl = 0;
		/* disable all cec features */
		cec_config(0x0, 1);
	}
	return 0;
}

static ssize_t hdmitx_cec_read(struct file *f, char __user *buf,
			   size_t size, loff_t *p)
{
	int ret;

	rx_len = 0;
	ret = wait_for_completion_timeout(&rx_complete, msecs_to_jiffies(50));
	if (ret <= 0)
		return ret;
	if (rx_len == 0)
		return 0;

	if (copy_to_user(buf, rx_msg, rx_len))
		return -EINVAL;
	return rx_len;
}

static ssize_t hdmitx_cec_write(struct file *f, const char __user *buf,
			    size_t size, loff_t *p)
{
	unsigned char tempbuf[16] = {};

	if (size > 16)
		size = 16;
	if (size <= 0)
		return 0;

	if (copy_from_user(tempbuf, buf, size))
		return -EINVAL;

	return cec_ll_tx(tempbuf, size);
}

static long hdmitx_cec_ioctl(struct file *f,
			     unsigned int cmd, unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	unsigned long tmp;
	int idx = cec_global_info.my_node_index;
	struct hdmi_port_info port_info;
	int a, b, c, d;

	switch (cmd) {
	case CEC_IOC_GET_PHYSICAL_ADDR:
		if (hdmitx_device->hdmi_info.vsdb_phy_addr.valid == 0)
			return -EINVAL;
		a = hdmitx_device->hdmi_info.vsdb_phy_addr.a;
		b = hdmitx_device->hdmi_info.vsdb_phy_addr.b;
		c = hdmitx_device->hdmi_info.vsdb_phy_addr.c;
		d = hdmitx_device->hdmi_info.vsdb_phy_addr.d;
		tmp = ((a << 12) | (b << 8) | (c << 4) | (d << 0));
		cec_phyaddr_config(tmp, 1);
		if (copy_to_user(argp, &tmp, _IOC_SIZE(cmd)))
			return -EINVAL;
		break;

	case CEC_IOC_GET_VERSION:
		tmp = CEC_VERSION_14A;
		if (copy_to_user(argp, &tmp, _IOC_SIZE(cmd)))
			return -EINVAL;
		break;

	case CEC_IOC_GET_VENDOR_ID:
		tmp = vendor_id;
		if (copy_to_user(argp, &tmp, _IOC_SIZE(cmd)))
			return -EINVAL;
		break;

	case CEC_IOC_GET_PORT_NUM:
		tmp = 1;
		if (copy_to_user(argp, &tmp, _IOC_SIZE(cmd)))
			return -EINVAL;
		break;

	case CEC_IOC_GET_PORT_INFO:
		/* for tx only 1 port */
		tmp  = ((hdmitx_device->hdmi_info.vsdb_phy_addr.a << 12) |
			(hdmitx_device->hdmi_info.vsdb_phy_addr.b <<  8) |
			(hdmitx_device->hdmi_info.vsdb_phy_addr.c <<  4) |
			(hdmitx_device->hdmi_info.vsdb_phy_addr.d <<  0));
		port_info.type = HDMI_OUTPUT;
		port_info.port_id = 1;
		port_info.cec_supported = 1;
		/* not support arc */
		port_info.arc_supported = 0;
		port_info.physical_address = tmp & 0xffff;
		if (copy_to_user(argp, &port_info, sizeof(port_info)))
			return -EINVAL;
		break;

	case CEC_IOC_GET_SEND_FAIL_REASON:
		tmp = get_cec_tx_fail();
		if (copy_to_user(argp, &tmp, _IOC_SIZE(cmd)))
			return -EINVAL;
		break;

	case CEC_IOC_SET_OPTION_WAKEUP:
		/* TODO: */
		break;

	case CEC_IOC_SET_OPTION_ENALBE_CEC:
		/* TODO: */
		break;

	case CEC_IOC_SET_OPTION_SYS_CTRL:
		/* TODO: */
		break;

	case CEC_IOC_SET_OPTION_SET_LANG:
		cec_global_info.cec_node_info[idx].menu_lang = arg;
		break;

	case CEC_IOC_GET_CONNECT_STATUS:
		tmp = hdmitx_device->hpd_state;
		if (copy_to_user(argp, &tmp, _IOC_SIZE(cmd)))
			return -EINVAL;
		break;

	case CEC_IOC_ADD_LOGICAL_ADDR:
		tmp = arg & 0xf;
		cec_logicaddr_set(tmp);
		/* add by hal, to init some data structure */
		cec_global_info.my_node_index = tmp;
		cec_global_info.cec_node_info[tmp].power_status = POWER_ON;
		cec_logicaddr_config(cec_global_info.my_node_index, 1);
		cec_global_info.cec_node_info[tmp].log_addr = tmp;

		cec_global_info.cec_node_info[tmp].
			specific_info.audio.sys_audio_mode = OFF;
		cec_global_info.cec_node_info[tmp].
			specific_info.audio.audio_status.
			audio_mute_status = OFF;
		cec_global_info.cec_node_info[tmp].
			specific_info.audio.audio_status.
			audio_volume_status = 0;

		cec_global_info.cec_node_info[tmp].
			cec_version = CEC_VERSION_14A;
		cec_global_info.cec_node_info[tmp].vendor_id = vendor_id;
		cec_global_info.cec_node_info[tmp].dev_type =
			cec_log_addr_to_dev_type(tmp);
		cec_global_info.cec_node_info[tmp].dev_type =
			cec_log_addr_to_dev_type(tmp);
		strcpy(cec_global_info.cec_node_info[tmp].osd_name, osd_name);

		cec_global_info.cec_node_info[tmp].menu_status =
					DEVICE_MENU_ACTIVE;
		break;

	case CEC_IOC_CLR_LOGICAL_ADDR:
		/* TODO: clear global info */
		break;

	default:
		break;
	}
	return 0;
}

#ifdef CONFIG_COMPAT
static long hdmitx_cec_compat_ioctl(struct file *f,
				    unsigned int cmd, unsigned long arg)
{
	arg = (unsigned long)compat_ptr(arg);
	return hdmitx_cec_ioctl(f, cmd, arg);
}
#endif

/* for improve rw permission */
static char *aml_cec_class_devnode(struct device *dev, umode_t *mode)
{
	if (mode)
		*mode = 0664;
	hdmi_print(INF, "mode is %x\n", *mode);
	return NULL;
}

static const struct file_operations hdmitx_cec_fops = {
	.owner          = THIS_MODULE,
	.open           = hdmitx_cec_open,
	.read           = hdmitx_cec_read,
	.write          = hdmitx_cec_write,
	.release        = hdmitx_cec_release,
	.unlocked_ioctl = hdmitx_cec_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl   = hdmitx_cec_compat_ioctl,
#endif
};

/************************ cec high level code *****************************/

static int aml_cec_probe(struct platform_device *pdev)
{
	int i;
	struct device *dev, *cdev;
#ifdef CONFIG_OF
	struct device_node *node = pdev->dev.of_node;
	int irq_idx = 0, r;
	const char *pin_name = NULL, *irq_name = NULL;
	unsigned int reg;
	struct pinctrl *p;
	struct vendor_info_data *vend;
	struct resource *res;
	resource_size_t *base;
#endif

	device_rename(&pdev->dev, "cectx");
	dbg_dev = &pdev->dev;
	hdmitx_device = get_hdmitx_device();
	switch_dev_register(&lang_dev);
	cec_key_init();

	memset(&cec_global_info, 0, sizeof(struct cec_global_info_t));
	cec_global_info.cec_rx_msg_buf.rx_buf_size =
	    sizeof(cec_global_info.cec_rx_msg_buf.cec_rx_message)
	    /sizeof(cec_global_info.cec_rx_msg_buf.cec_rx_message[0]);

	dev = dev_get_platdata(&pdev->dev);
	if (dev) {
		/* cdev registe */
		cec_global_info.dev_no = register_chrdev(0, CEC_DEV_NAME,
							 &hdmitx_cec_fops);
		if (cec_global_info.dev_no < 0) {
			hdmi_print(INF, "alloc chrdev failed\n");
			return -EINVAL;
		}
		hdmi_print(INF, "alloc chrdev %x\n", cec_global_info.dev_no);
		if (!dev->class->devnode)
			dev->class->devnode = aml_cec_class_devnode;
		cdev = device_create(dev->class, &pdev->dev,
				     MKDEV(cec_global_info.dev_no, 0),
				     NULL, CEC_DEV_NAME);
		if (IS_ERR(cdev)) {
			hdmi_print(INF, "create chrdev failed, dev:%p\n", cdev);
			unregister_chrdev(cec_global_info.dev_no, CEC_DEV_NAME);
			return -EINVAL;
		}
	}

	init_completion(&menu_comp);
	init_completion(&rx_complete);
	wake_lock_init(&cec_lock, WAKE_LOCK_SUSPEND, "hdmi_cec");
	CEC_INFO("CEC probe, fun_config:%x\n",
		 hdmitx_device->cec_func_config);
	/*
	 * save default configs parsed from boot args
	 */
	cec_config(hdmitx_device->cec_func_config, 1);
	if (!(hdmitx_device->cec_func_config & (1 << CEC_FUNC_MSAK))) {
		/* keep reset state if not enabled */
		cec_keep_reset();
	}
	cec_workqueue = create_workqueue("cec_work");
	if (cec_workqueue == NULL) {
		pr_info("create work queue failed\n");
		return -EFAULT;
	}
	INIT_DELAYED_WORK(&hdmitx_device->cec_work, cec_task);
	hrtimer_init(&cec_key_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	cec_key_timer.function = cec_key_up;
	cec_global_info.remote_cec_dev = input_allocate_device();
	if (!cec_global_info.remote_cec_dev)
		CEC_INFO("No enough memory\n");

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
		CEC_INFO("Failed to register device\n");
		input_free_device(cec_global_info.remote_cec_dev);
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	hdmitx_cec_early_suspend_handler.level
	    = EARLY_SUSPEND_LEVEL_BLANK_SCREEN - 20;
	hdmitx_cec_early_suspend_handler.suspend = hdmitx_cec_early_suspend;
	hdmitx_cec_early_suspend_handler.resume = hdmitx_cec_late_resume;
	hdmitx_cec_early_suspend_handler.param = hdmitx_device;

	register_early_suspend(&hdmitx_cec_early_suspend_handler);
#endif
#ifdef CONFIG_OF
	/* pinmux set */
	if (of_get_property(node, "pinctrl-names", NULL)) {
		r = of_property_read_string(node,
					    "pinctrl-names",
					    &pin_name);
		if (!r) {
			p = devm_pinctrl_get_select(&pdev->dev, pin_name);
			reg = hd_read_reg(P_AO_RTI_PIN_MUX_REG);
			reg = hd_read_reg(P_AO_RTI_PIN_MUX_REG2);
		}
	}

	/* irq set */
	irq_idx = of_irq_get(node, 0);
	hdmitx_device->irq_cec = irq_idx;
	if (of_get_property(node, "interrupt-names", NULL)) {
		r = of_property_read_string(node, "interrupt-names", &irq_name);
		if (!r) {
			r = request_irq(irq_idx, &cec_isr_handler, IRQF_SHARED,
					irq_name, (void *)hdmitx_device);
		}
	}
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res) {
		base = devm_ioremap_resource(&pdev->dev, res);
		exit_reg = (void *)base;
	} else {
		CEC_INFO("no memory resource\n");
		exit_reg = NULL;
	}

	vend = hdmitx_device->config_data.vend_data;
	r = of_property_read_string(node, "vendor_name",
		(const char **)&(vend->vendor_name));
	if (r)
		hdmi_print(INF, SYS "not find vendor name\n");

	r = of_property_read_u32(node, "vendor_id", &(vend->vendor_id));
	if (r)
		hdmi_print(INF, SYS "not find vendor id\n");

	r = of_property_read_string(node, "product_desc",
		(const char **)&(vend->product_desc));
	if (r)
		hdmi_print(INF, SYS "not find product desc\n");

	r = of_property_read_string(node, "cec_osd_string",
		(const char **)&(vend->cec_osd_string));
	if (r)
		hdmi_print(INF, SYS "not find cec osd string\n");
	r = of_property_read_u32(node, "cec_config",
		&(vend->cec_config));
	r = of_property_read_u32(node, "ao_cec", &(vend->ao_cec));

	if (dev) {
		r = device_create_file(dev, &dev_attr_cec);
		r = device_create_file(dev, &dev_attr_cec_config);
		r = device_create_file(dev, &dev_attr_cec_lang_config);
		r = device_create_file(dev, &dev_attr_cec_active_status);
		r = device_create_file(dev, &dev_attr_tv_support_cec);
	}
#endif
	hdmitx_device->cec_init_ready = 1;
	cec_global_info.cec_flag.cec_init_flag = 0;
	/* for init */
	queue_delayed_work(cec_workqueue, &hdmitx_device->cec_work, 0);
	cec_reboot_nb.notifier_call = cec_reboot;
	register_reboot_notifier(&cec_reboot_nb);
	return 0;
}

static int aml_cec_remove(struct platform_device *pdev)
{
	struct device *dev;
	if (!(hdmitx_device->cec_func_config & (1 << CEC_FUNC_MSAK)))
		return 0;

	CEC_INFO("cec uninit!\n");
	if (cec_global_info.cec_flag.cec_init_flag == 1) {
		free_irq(hdmitx_device->irq_cec, (void *)hdmitx_device);
		cec_global_info.cec_flag.cec_init_flag = 0;
	}

	if (cec_workqueue) {
		cancel_delayed_work_sync(&hdmitx_device->cec_work);
		destroy_workqueue(cec_workqueue);
	}
	dev = dev_get_platdata(&pdev->dev);
	if (dev) {
		device_remove_file(dev, &dev_attr_cec);
		device_remove_file(dev, &dev_attr_cec_config);
		device_remove_file(dev, &dev_attr_cec_lang_config);
		device_remove_file(dev, &dev_attr_cec_active_status);
		device_remove_file(dev, &dev_attr_tv_support_cec);
	}
	hdmitx_device->cec_init_ready = 0;
	input_unregister_device(cec_global_info.remote_cec_dev);
	cec_global_info.cec_flag.cec_fiq_flag = 0;
	unregister_reboot_notifier(&cec_reboot_nb);
	unregister_chrdev(cec_global_info.dev_no, CEC_DEV_NAME);
	return 0;
}

#ifdef CONFIG_PM
static int aml_cec_pm_prepare(struct device *dev)
{
	if ((hdmitx_device->cec_func_config & (1 << CEC_FUNC_MSAK))) {
		cec_suspend = 1;
		CEC_INFO("cec prepare suspend!\n");
	}
	return 0;
}

static void aml_cec_pm_complete(struct device *dev)
{
	int exit = 0;

	if (exit_reg) {
		exit = readl(exit_reg);
		CEC_INFO("wake up flag:%x\n", exit);
	}
	if (((exit >> 28) & 0xf) == CEC_WAKEUP) {
		input_event(cec_global_info.remote_cec_dev,
			    EV_KEY, KEY_POWER, 1);
		input_sync(cec_global_info.remote_cec_dev);
		input_event(cec_global_info.remote_cec_dev,
			    EV_KEY, KEY_POWER, 0);
		input_sync(cec_global_info.remote_cec_dev);
		CEC_INFO("== WAKE UP BY CEC ==\n");
	}
}

static int aml_cec_resume_noirq(struct device *dev)
{
	if ((hdmitx_device->cec_func_config & (1 << CEC_FUNC_MSAK))) {
		CEC_INFO("cec resume noirq!\n");
		cec_global_info.cec_node_info[cec_global_info.my_node_index].
			power_status = TRANS_STANDBY_TO_ON;
	}
	return 0;
}

static const struct dev_pm_ops aml_cec_pm = {
	.prepare  = aml_cec_pm_prepare,
	.complete = aml_cec_pm_complete,
	.resume_noirq = aml_cec_resume_noirq,
};
#endif

#ifdef CONFIG_OF
static const struct of_device_id aml_cec_dt_match[] = {
	{
		.compatible = "amlogic, amlogic-cec",
	},
	{},
};
#endif

static struct platform_driver aml_cec_driver = {
	.driver = {
		.name  = "cec",
		.owner = THIS_MODULE,
	#ifdef CONFIG_PM
		.pm     = &aml_cec_pm,
	#endif
	#ifdef CONFIG_OF
		.of_match_table = aml_cec_dt_match,
	#endif
	},
	.probe  = aml_cec_probe,
	.remove = aml_cec_remove,
};

static int __init cec_init(void)
{
	int ret;
	ret = platform_driver_register(&aml_cec_driver);
	return ret;
}

static void __exit cec_uninit(void)
{
	platform_driver_unregister(&aml_cec_driver);
}

size_t cec_usrcmd_get_global_info(char *buf)
{
	int i = 0;
	int dev_num = 0;

	struct cec_node_info_t *buf_node_addr = (struct cec_node_info_t *)(buf +
		(unsigned long)(((struct cec_global_info_to_usr_t *)0)->
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
	return (size_t)(sizeof(struct cec_node_info_t) * dev_num
		+ (unsigned long)((struct cec_global_info_to_usr_t *)0)->
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
		CEC_INFO("too many args\n");

	for (i = 0; i < count; i++) {
		if ((buf[i] >= '0') && (buf[i] <= 'f')) {
			ret = kstrtoul(&buf[i], 16, (unsigned long *)&param[j]);
			j++;
		}
		while (i < count && buf[i] != ' ')
			i++;
	}
	value = cec_config(0, 0);
	cec_config(param[0], 1);
	CEC_INFO("old value:%lx, new para:%x\n", value, param[0]);
	hdmitx_device->cec_func_config = cec_config(0, 0);
	if ((0 == (value & 0x1)) && (1 == (param[0] & 1))) {
		hdmitx_device->cec_init_ready = 1;
		hdmitx_device->hpd_state = 1;
		cec_global_info.cec_flag.cec_init_flag = 1;
		cec_node_init(hdmitx_device);
	} else if ((value & 0x01) && !(param[0] & 0x01)) {
		/* toggle off cec funtion by user */
		CEC_INFO("user disable cec\n");
		/* disable irq to stop rx/tx process */
		cec_keep_reset();
	}
	if (!(hdmitx_device->cec_func_config
	    & (1 << CEC_FUNC_MSAK)) || !hdmitx_device->hpd_state)
		return;

	if ((1 == (param[0] & 1)) && (0x2 == (value & 0x2))
	    && (0x0 == (param[0] & 0x2)))
		cec_menu_status_smp(DEVICE_MENU_INACTIVE);

	if ((1 == (param[0] & 1)) && (0x0 == (value & 0x2))
	    && (0x2 == (param[0] & 0x2)))
		cec_active_source_smp();

	if ((0x20 == (param[0] & 0x20)) && (0x0 == (value & 0x20)))
		cec_get_menu_language_smp();

	CEC_INFO("cec_func_config:0x%x : 0x%x\n",
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

	CEC_INFO("cec usrcmd set dispatch start:\n");
	/* if none HDMI out,no CEC features. */
	if (!hdmitx_device->hpd_state) {
		CEC_INFO("HPD low!\n");
		return;
	}

	if (!(hdmitx_device->cec_func_config & (1 << CEC_FUNC_MSAK))) {
		CEC_INFO("cec function masked!\n");
		return;
	}

	if (count > 32)
		CEC_INFO("too many args\n");

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
	CEC_INFO("cec usrcmd set dispatch end!\n");
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
