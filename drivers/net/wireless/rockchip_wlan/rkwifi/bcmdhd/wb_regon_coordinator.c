/*
 * DHD WiFi BT RegON Coordinator - WBRC
 *
 * Copyright (C) 2025 Synaptics Incorporated. All rights reserved.
 *
 * This software is licensed to you under the terms of the
 * GNU General Public License version 2 (the "GPL") with Broadcom special exception.
 *
 * INFORMATION CONTAINED IN THIS DOCUMENT IS PROVIDED "AS-IS," AND SYNAPTICS
 * EXPRESSLY DISCLAIMS ALL EXPRESS AND IMPLIED WARRANTIES, INCLUDING ANY
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE,
 * AND ANY WARRANTIES OF NON-INFRINGEMENT OF ANY INTELLECTUAL PROPERTY RIGHTS.
 * IN NO EVENT SHALL SYNAPTICS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, PUNITIVE, OR CONSEQUENTIAL DAMAGES ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OF THE INFORMATION CONTAINED IN THIS DOCUMENT, HOWEVER CAUSED
 * AND BASED ON ANY THEORY OF LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, AND EVEN IF SYNAPTICS WAS ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE. IF A TRIBUNAL OF COMPETENT JURISDICTION
 * DOES NOT PERMIT THE DISCLAIMER OF DIRECT DAMAGES OR ANY OTHER DAMAGES,
 * SYNAPTICS' TOTAL CUMULATIVE LIABILITY TO ANY PARTY SHALL NOT
 * EXCEED ONE HUNDRED U.S. DOLLARS
 *
 * Copyright (C) 2025, Broadcom.
 *
 *      Unless you and Broadcom execute a separate written software license
 * agreement governing use of this software, this software is licensed to you
 * under the terms of the GNU General Public License version 2 (the "GPL"),
 * available at http://www.broadcom.com/licenses/GPLv2.php, with the
 * following added to such license:
 *
 *      As a special exception, the copyright holders of this software give you
 * permission to link this software with independent modules, and to copy and
 * distribute the resulting executable under terms of your choice, provided that
 * you also meet, for each linked independent module, the terms and conditions of
 * the license of that module.  An independent module is a module which is not
 * derived from this software.  The special exception does not apply to any
 * modifications of the software.
 *
 *
 * <<Broadcom-WL-IPTag/Open:>>
 *
 */
#include <linux/cdev.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/poll.h>
#include <linux/eventpoll.h>
#include <linux/version.h>
#include <linux/delay.h>
#include <linux/vmalloc.h>
#include <linux/sched/clock.h>

#include <wb_regon_coordinator.h>
#include <bcmstdlib_s.h>

#define DESCRIPTION "Broadcom WiFi BT Regon coordinator Driver"
#define AUTHOR "Broadcom Corporation"

#define DEVICE_NAME "wbrc"
#define CLASS_NAME "bcm"

#ifndef BCM_REFERENCE
#define BCM_REFERENCE(data)	((void)(data))
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 16, 0))
typedef unsigned int __poll_t;
#endif

static struct wbrc_pvt_data *g_wbrc_data;
static struct mutex wbrc_mutex;
static wait_queue_head_t outmsg_waitq;

static int bt2wbrc_ack_bt_reset(struct wbrc_pvt_data *wbrc_data);
#ifdef BT_FW_DWNLD
static int bt2wbrc_bt_fw_dwnld(struct wbrc_pvt_data *wbrc_data, void *fwblob, uint len);
#endif /* BT_FW_DWNLD */
#ifdef WBRC_HW_QUIRKS
static void bt2wbrc_process_reg_onoff_cmds(int cmd);
#endif
static void wbrc2bt_cmd_bt_reset(struct wbrc_pvt_data *wbrc_data);

static int wbrc_bt_dev_open(struct inode *, struct file *);
static int wbrc_bt_dev_release(struct inode *, struct file *);
static ssize_t wbrc_bt_dev_read(struct file *, char *, size_t, loff_t *);
static ssize_t wbrc_bt_dev_write(struct file *, const char *, size_t, loff_t *);
static __poll_t wbrc_bt_dev_poll(struct file *filep, poll_table *wait);
static int wbrc_wait_on_condition(wait_queue_head_t *q, uint tmo_ms,
	uint *var, uint condition);

static struct file_operations wbrc_bt_dev_fops = {
	.open = wbrc_bt_dev_open,
	.read = wbrc_bt_dev_read,
	.write = wbrc_bt_dev_write,
	.release = wbrc_bt_dev_release,
	.poll = wbrc_bt_dev_poll,
};

typedef enum wbrc_state {
	IDLE = 0,
	WLAN_ON_IN_PROGRESS = 1,
	WLAN_OFF_IN_PROGRESS = 2,
	BT_ON_IN_PROGRESS = 3,
	BT_FW_DWNLD_IN_PROGRESS
} wbrc_state_t;

struct wbrc_pvt_data {
	int wbrc_bt_dev_major_number;		/* BT char dev major number */
	struct class *wbrc_bt_dev_class;	/* BT char dev class */
	struct device *wbrc_bt_dev_device;	/* BT char dev */
	bool bt_dev_opened;			/* To check if bt dev open is called */
	wait_queue_head_t bt_reset_waitq;	/* waitq to wait till bt reset is done */
	unsigned int bt_reset_ack;		/* condition variable to be check for bt reset */
	wbrc_msg_t wbrc2bt_msg;			/* message to send to BT stack */
#ifdef BT_FW_DWNLD
	/* buffer to store ext message sent from BT to WBRC */
	unsigned char bt2wbrc_ext_msgbuf[WBRC_MSG_BUF_MAXLEN];
#endif /* BT_FW_DWNLD */
	bool read_data_available;		/* condition to check if read data is present */
	wbrc_state_t state;			/* wbrc state machine */
	wait_queue_head_t state_change_waitq;	/* Q to wait on for state change events */
	struct mutex state_mutex;		/* mutex to synchronise on state changes */
#ifdef WBRC_HW_QUIRKS
	struct mutex onoff_mutex;	/* mutex to synchronise on wl/bt_regon/off_inprogress */
	uint wl_regon_inprogress;		/* indicates WL reg on is in progress */
	uint wl_regoff_inprogress;		/* indicates WL reg off is in progress */
	uint bt_regon_inprogress;		/* indicates BT reg on is in progress */
	uint bt_regoff_inprogress;		/* indicates BT reg off is in progress */
	wait_queue_head_t onoff_waitq;		/* Q to wait on for reg on/off events */
	uint chipid;
	unsigned long long bt_after_regoff_ts;	/* timestamp of the last BT reg off */
	unsigned long long wl_after_regoff_ts;	/* timestamp of the last WL reg off */
#endif /* WBRC_HW_QUIRKS */
	void *wl_hdl;				/* opaque handle to wlan host driver */
};

#define WBRC_LOCK(wbrc_mutex)           mutex_lock(&wbrc_mutex)
#define WBRC_UNLOCK(wbrc_mutex)         mutex_unlock(&wbrc_mutex)

#define WBRC_STATE_LOCK(wbrc_data)	{if (wbrc_data) mutex_lock(&(wbrc_data)->state_mutex);}
#define WBRC_STATE_UNLOCK(wbrc_data)	{if (wbrc_data) mutex_unlock(&(wbrc_data)->state_mutex);}

#define WBRC_ONOFF_LOCK(wbrc_data)	{if (wbrc_data) mutex_lock(&(wbrc_data)->onoff_mutex);}
#define WBRC_ONOFF_UNLOCK(wbrc_data)	{if (wbrc_data) mutex_unlock(&(wbrc_data)->onoff_mutex);}

#define WBRC_CHKCHIP(chipid) ((chipid) != (0x4390))

/* Function prototypes */
int wbrc_init(void);
void wbrc_exit(void);
int wbrc_wlan_on_started(void);
int wbrc_wlan_on_ack(void);
int wbrc_reset_wait_on_condition(wait_queue_head_t *reset_waitq, uint *var, uint condition);
int wbrc_wl2bt_reset(void);
int wbrc_bt_reset_ack(struct wbrc_pvt_data *wbrc_data);

#ifdef WBRC_HW_QUIRKS
static void
wbrc_delay(uint msec)
{
	msleep(msec);
}

unsigned long long
wbrc_sysuptime_ns(void)
{
	return local_clock();
}
#endif /* WBRC_HW_QUIRKS */

static ssize_t
wbrc_bt_dev_read(struct file *filep, char *buffer, size_t len,
                             loff_t *offset)
{
	struct wbrc_pvt_data *wbrc_data;
	int err_count = 0;
	int ret = 0;

	WBRC_LOCK(wbrc_mutex);
	pr_info("%s\n", __func__);
	wbrc_data = g_wbrc_data;
	if (!wbrc_data) {
		pr_err("%s: wbrc not inited !\n", __func__);
		ret = -EFAULT;
		goto exit;
	}

	if (wbrc_data->read_data_available == FALSE) {
		goto exit;
	}
	if (len < WBRC_MSG_LEN) {
		pr_err("%s: invalid length:%d\n", __func__, (int)len);
		ret = -EFAULT;
		goto exit;
	}
	err_count = copy_to_user(buffer, &wbrc_data->wbrc2bt_msg,
		sizeof(wbrc_data->wbrc2bt_msg));
	if (err_count == 0) {
		pr_info("Sent %d bytes\n",
			(int)sizeof(wbrc_data->wbrc2bt_msg));
		err_count = sizeof(wbrc_data->wbrc2bt_msg);
	} else {
		pr_err("Failed to send %d bytes\n", err_count);
		ret = -EFAULT;
	}
	wbrc_data->read_data_available = FALSE;

exit:
	WBRC_UNLOCK(wbrc_mutex);
	return ret;
}

static ssize_t
wbrc_bt_dev_write(struct file *filep, const char *buffer,
	size_t len, loff_t *offset)
{
	struct wbrc_pvt_data *wbrc_data;
	int err_count = 0;
	int ret = len;
	wbrc_msg_t msg;
#ifdef BT_FW_DWNLD
	int err = 0;
	wbrc_ext_msg_t *extmsg = NULL;
#endif /* BT_FW_DWNLD */

	WBRC_LOCK(wbrc_mutex);
	wbrc_data = g_wbrc_data;
	if (!wbrc_data) {
		pr_err("%s: wbrc not inited !\n", __func__);
		ret = -EFAULT;
		goto exit;
	}

	pr_info("%s Received %zu bytes\n", __func__, len);
	if (len < WBRC_MSG_LEN || len > WBRC_MSG_BUF_MAXLEN) {
		pr_err("%s: Invalid msg len %lu\n", __func__, len);
		ret = -EFAULT;
		goto exit;
	}

	err_count = copy_from_user(&msg, buffer, WBRC_MSG_LEN);
	if (err_count) {
		pr_err("%s: copy_from_user failed:%d\n", __func__, err_count);
		ret = -EFAULT;
		goto exit;
	}

	if (msg.hdr != WBRC_HEADER_DIR_BT2WBRC) {
		pr_err("%s: invalid header:%d\n", __func__, msg.hdr);
		ret = -EFAULT;
		goto exit;
	}

	/* check for extended len msg */
	if (msg.len & WBRC_EXT_MSG_LEN) {
#ifdef BT_FW_DWNLD
		/* copy the full ext msg including BT FW blob */
		err_count = copy_from_user(wbrc_data->bt2wbrc_ext_msgbuf, buffer, len);
		extmsg = (wbrc_ext_msg_t *)wbrc_data->bt2wbrc_ext_msgbuf;
		if (extmsg->type == WBRC_TYPE_BT2WBRC_CMD) {
			if (err_count) {
				pr_err("%s: copy_from_user failed:%d\n", __func__, err_count);
				ret = -EFAULT;
				goto exit;
			}
			if (extmsg->cmd == WBRC_CMD_BT_FW_DWNLD) {
				/* validate internal len field */
				if (extmsg->len > WBRC_MSG_BUF_MAXLEN) {
					pr_err("%s: Invalid ext msg len %u\n",
						__func__, extmsg->len);
					ret = -EFAULT;
					goto exit;
				}
				pr_err("%s: recd. valid CMD_BT_FW_DWNLD, bt fw len = %u bytes\n",
					__func__, extmsg->len);
				err = bt2wbrc_bt_fw_dwnld(wbrc_data, extmsg->val, extmsg->len);
				if (err != WBRC_OK) {
					ret = err;
				}
			} else {
				pr_err("%s: Unknown ext msg cmd 0x%x\n", __func__, extmsg->cmd);
				ret = -EFAULT;
			}
		} else {
			/* for now BT_FW_DWNLD is the only CMD from BT2WBRC */
			pr_err("%s: Unknown extmsg type = 0x%x\n", __func__, extmsg->type);
			ret = -EFAULT;
		}
#endif /* BT_FW_DWNLD */
	} else if (msg.type == WBRC_TYPE_BT2WBRC_ACK && msg.val == WBRC_ACK_BT_RESET_COMPLETE) {
		pr_info("RCVD ACK_RESET_BT_COMPLETE");
		bt2wbrc_ack_bt_reset(wbrc_data);
	} else if (msg.type == WBRC_TYPE_BT2WBRC_CMD) {
#ifdef WBRC_HW_QUIRKS
		if (msg.val >= WBRC_CMD_BT_BEFORE_REG_OFF &&
			msg.val <= WBRC_CMD_BT_AFTER_REG_ON) {
			if (WBRC_CHKCHIP(wbrc_data->chipid)) {
				ret = len;
			} else {
				bt2wbrc_process_reg_onoff_cmds(msg.val);
			}
		}
#endif /* WBRC_HW_QUIRKS */
	} else {
		pr_err("%s: Unknown msg type %u\n", __func__, msg.type);
		ret = -EFAULT;
	}

exit:
	WBRC_UNLOCK(wbrc_mutex);
	return ret;
}

static __poll_t
wbrc_bt_dev_poll(struct file *filep, poll_table *wait)
{
	struct wbrc_pvt_data *wbrc_data;
	__poll_t mask = 0;

	pr_info("%s\n", __func__);
	WBRC_LOCK(wbrc_mutex);
	wbrc_data = g_wbrc_data;
	if (!wbrc_data) {
		pr_err("%s: wbrc not inited !\n", __func__);
		WBRC_UNLOCK(wbrc_mutex);
		return EPOLLHUP;
	}
	WBRC_UNLOCK(wbrc_mutex);

	poll_wait(filep, &outmsg_waitq, wait);

	WBRC_LOCK(wbrc_mutex);
	wbrc_data = g_wbrc_data;
	if (!wbrc_data) {
		pr_err("%s: wbrc not inited after poll_wait\n", __func__);
		WBRC_UNLOCK(wbrc_mutex);
		return EPOLLHUP;
	}

	if (wbrc_data->read_data_available)
		mask |= EPOLLIN | EPOLLRDNORM;

	if (!wbrc_data->bt_dev_opened)
		mask |= EPOLLHUP;
	WBRC_UNLOCK(wbrc_mutex);

	return mask;
}

/* WL2WBRC - wlan host driver init */
#ifdef WBRC_HW_QUIRKS
void
wl2wbrc_wlan_init(void *wl_hdl, uint chipid)
#else
void
wl2wbrc_wlan_init(void *wl_hdl)
#endif /* WBRC_HW_QUIRKS */
{
	struct wbrc_pvt_data *wbrc_data = g_wbrc_data;
	wbrc_data->wl_hdl = wl_hdl;
#ifdef WBRC_HW_QUIRKS
	wbrc_data->chipid = chipid;
	pr_err("%s: called for chipid %x\n", __func__, chipid);
#endif /* WBRC_HW_QUIRKS */
}

static int
wbrc_bt_dev_open(struct inode *inodep, struct file *filep)
{
	struct wbrc_pvt_data *wbrc_data;
	int ret = 0, tmo_ret = 0;
	int state = 0;
	int err = 0;

	BCM_REFERENCE(err);

	WBRC_LOCK(wbrc_mutex);
	wbrc_data = g_wbrc_data;
	if (!wbrc_data) {
		pr_err("%s: wbrc not inited !\n", __func__);
		WBRC_UNLOCK(wbrc_mutex);
		return -EFAULT;
	}

	if (!wbrc_data->wl_hdl) {
		pr_err("%s: wl not inited !\n", __func__);
		WBRC_UNLOCK(wbrc_mutex);
		return -EFAULT;
	}

	if (wbrc_data->bt_dev_opened) {
		pr_err("%s already opened\n", __func__);
		WBRC_UNLOCK(wbrc_mutex);
		return -EFAULT;
	}
	WBRC_UNLOCK(wbrc_mutex);

	WBRC_STATE_LOCK(wbrc_data);

	state = wbrc_data->state;

	/* check for invalid states */
	if (state == BT_ON_IN_PROGRESS ||
		state == BT_FW_DWNLD_IN_PROGRESS) {
		WBRC_STATE_UNLOCK(wbrc_data);
		pr_err("%s: Unexpected state %u !\n", __func__, state);
		goto exit;
	}

	if (state == WLAN_ON_IN_PROGRESS ||
		state == WLAN_OFF_IN_PROGRESS) {
		WBRC_STATE_UNLOCK(wbrc_data);
		pr_err("%s: state=%u, wait for WLAN ON/OFF to finish \n", __func__, state);
		/* Wait till wlan is ON. i.e, state change to IDLE */
		tmo_ret = wbrc_wait_on_condition(&wbrc_data->state_change_waitq,
			WBRC_WAIT_TIMEOUT, &wbrc_data->state, IDLE);
		if (tmo_ret <= 0) {
			pr_err("%s: WLAN ON/OFF timed out !\n", __func__);
			/* return error if WLAN ON times out, else there is a chance
			 * that BTHal can proceed with BT FW dwnld without
			 * WLAN FW running first
			 */
			return -EFAULT;
		} else {
			pr_err("%s: WLAN ON/OFF finished\n", __func__);
		}
		WBRC_STATE_LOCK(wbrc_data);
	}

	wbrc_data->state = BT_ON_IN_PROGRESS;
	pr_err("%s: state -> BT_ON_IN_PROGRESS \n", __func__);

	WBRC_STATE_UNLOCK(wbrc_data);

#ifdef WBRC_WLAN_ON_FIRST_ALWAYS
	/* turn on wlan - will return immediately if wlan is already on */
	pr_err("%s: turn WLAN ON ... \n", __func__);
	err = wbrc2wl_wlan_on_request(wbrc_data->wl_hdl);
	if (err) {
		pr_err("%s: WLAN failed to turn ON ! err=%d\n", __func__, err);
	} else {
		pr_err("%s: WLAN is ON \n", __func__);
	}
#endif /* WBRC_WLAN_ON_FIRST_ALWAYS */

	/* change state back to IDLE */
	WBRC_STATE_LOCK(wbrc_data);
	wbrc_data->state = IDLE;
	pr_err("%s: state -> IDLE \n", __func__);
	WBRC_STATE_UNLOCK(wbrc_data);

#ifdef WBRC_WLAN_ON_FIRST_ALWAYS
	/* return error if wlan fails to turn on, else BTHal will
	 * proceed with BT FW dwnld without WLAN FW running first
	 */
	if (err) {
		return -EFAULT;
	}
#endif /* WBRC_WLAN_ON_FIRST_ALWAYS */

	wake_up(&wbrc_data->state_change_waitq);

exit:
	WBRC_LOCK(wbrc_mutex);
	wbrc_data = g_wbrc_data;
	if (!wbrc_data) {
		pr_err("%s: wbrc not inited in exit\n", __func__);
		WBRC_UNLOCK(wbrc_mutex);
		return ret;
	}

	wbrc_data->bt_dev_opened = TRUE;
	pr_err("%s Device opened %d time(s)\n", __func__,
		wbrc_data->bt_dev_opened);
	WBRC_UNLOCK(wbrc_mutex);

	pr_err("%s Done\n", __func__);
	return ret;
}

static int
wbrc_bt_dev_release(struct inode *inodep, struct file *filep)
{
	struct wbrc_pvt_data *wbrc_data;

	WBRC_LOCK(wbrc_mutex);
	wbrc_data = g_wbrc_data;
	if (!wbrc_data) {
		pr_err("%s: wbrc not inited !\n", __func__);
		WBRC_UNLOCK(wbrc_mutex);
		return -EFAULT;
	}

	pr_err("%s Device closed %d\n", __func__, wbrc_data->bt_dev_opened);
	wbrc_data->bt_dev_opened = FALSE;
	WBRC_UNLOCK(wbrc_mutex);
	wake_up_interruptible(&outmsg_waitq);
#ifdef BT_FW_DWNLD
	wake_up_interruptible(&wbrc_data->state_change_waitq);
#endif
#ifdef WBRC_HW_QUIRKS
	WBRC_ONOFF_LOCK(wbrc_data);
	wbrc_data->bt_regon_inprogress = FALSE;
	wbrc_data->bt_regoff_inprogress = FALSE;
	WBRC_ONOFF_UNLOCK(wbrc_data);
	wake_up_interruptible(&wbrc_data->onoff_waitq);
#endif /* WBRC_HW_QUIRKS */
	return 0;
}

/* WBRC_LOCK to be held by caller */
static void
wbrc2bt_cmd_bt_reset(struct wbrc_pvt_data *wbrc_data)
{
	pr_info("%s\n", __func__);

	wbrc_data->wbrc2bt_msg.hdr = WBRC_HEADER_DIR_WBRC2BT;
	wbrc_data->wbrc2bt_msg.type = WBRC_TYPE_WBRC2BT_CMD;
	wbrc_data->wbrc2bt_msg.val = WBRC_CMD_BT_RESET;
	wbrc_data->wbrc2bt_msg.len = WBRC_MSG_DEFAULT_LEN;

	wbrc_data->read_data_available = TRUE;
	smp_wmb();
	wake_up_interruptible(&outmsg_waitq);
}

int
wbrc_init(void)
{
	int err = 0;
	struct wbrc_pvt_data *wbrc_data;

	pr_info("%s\n", __func__);

	wbrc_data = vzalloc(sizeof(struct wbrc_pvt_data));
	if (wbrc_data == NULL) {
		return -ENOMEM;
	}

	mutex_init(&wbrc_mutex);
	mutex_init(&wbrc_data->state_mutex);
#ifdef WBRC_HW_QUIRKS
	mutex_init(&wbrc_data->onoff_mutex);
	init_waitqueue_head(&wbrc_data->onoff_waitq);
#endif /* WBRC_HW_QUIRKS */
	init_waitqueue_head(&wbrc_data->bt_reset_waitq);
	init_waitqueue_head(&outmsg_waitq);
	init_waitqueue_head(&wbrc_data->state_change_waitq);

	g_wbrc_data = wbrc_data;

	wbrc_data->wbrc_bt_dev_major_number = register_chrdev(0, DEVICE_NAME, &wbrc_bt_dev_fops);
	err = wbrc_data->wbrc_bt_dev_major_number;
	if (wbrc_data->wbrc_bt_dev_major_number < 0) {
		pr_alert("wbrc_sequencer failed to register a major number\n");
		goto err_register;
	}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0))
	wbrc_data->wbrc_bt_dev_class = class_create(CLASS_NAME);
#else
	wbrc_data->wbrc_bt_dev_class = class_create(THIS_MODULE, CLASS_NAME);
#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)) */
	err = PTR_ERR(wbrc_data->wbrc_bt_dev_class);
	if (IS_ERR(wbrc_data->wbrc_bt_dev_class)) {
		pr_alert("Failed to register device class\n");
		goto err_class;
	}

	wbrc_data->wbrc_bt_dev_device = device_create(
		wbrc_data->wbrc_bt_dev_class, NULL, MKDEV(wbrc_data->wbrc_bt_dev_major_number, 0),
		NULL, DEVICE_NAME);
	err = PTR_ERR(wbrc_data->wbrc_bt_dev_device);
	if (IS_ERR(wbrc_data->wbrc_bt_dev_device)) {
		pr_alert("Failed to create the device\n");
		goto err_device;
	}
	pr_info("device class created correctly\n");

	return 0;

err_device:
	class_destroy(wbrc_data->wbrc_bt_dev_class);
err_class:
	unregister_chrdev(wbrc_data->wbrc_bt_dev_major_number, DEVICE_NAME);
err_register:
	vfree(wbrc_data);
	g_wbrc_data = NULL;
	return err;
}

void
wbrc_exit(void)
{
	struct wbrc_pvt_data *wbrc_data;
	pr_info("%s\n", __func__);
	WBRC_LOCK(wbrc_mutex);
	wbrc_data = g_wbrc_data;
	if (!wbrc_data) {
		pr_err("%s: wbrc not inited !\n", __func__);
		WBRC_UNLOCK(wbrc_mutex);
		return;
	}
	WBRC_UNLOCK(wbrc_mutex);

	/*
	 * wake up blocking process if exists
	 * the process akwaken will process fully only if the wbrc_mutex hold prior to this context
	 * if not, will return error to the user level
	 */
	wake_up_interruptible(&outmsg_waitq);

	WBRC_LOCK(wbrc_mutex);
	wbrc_data = g_wbrc_data;
	if (!wbrc_data) {
		pr_err("%s: wbrc not inited after wakeup\n", __func__);
		WBRC_UNLOCK(wbrc_mutex);
		return;
	}

	device_destroy(wbrc_data->wbrc_bt_dev_class, MKDEV(wbrc_data->wbrc_bt_dev_major_number, 0));
	class_destroy(wbrc_data->wbrc_bt_dev_class);
	unregister_chrdev(wbrc_data->wbrc_bt_dev_major_number, DEVICE_NAME);
	vfree(wbrc_data);
	g_wbrc_data = NULL;
	WBRC_UNLOCK(wbrc_mutex);
}

#ifndef BCMDHD_MODULAR
/* Required only for Built-in DHD */
module_init(wbrc_init);
module_exit(wbrc_exit);
#endif /* BOARD_MODULAR */

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION(DESCRIPTION);
MODULE_AUTHOR(AUTHOR);

/*
 * Wait until the condition *var == condition is met.
 * Returns 0 if the @condition evaluated to false after the timeout elapsed
 * Returns 1 if the @condition evaluated to true
 */
int
wbrc_wait_on_condition(wait_queue_head_t *waitq, uint tmo_ms,
	uint *var, uint condition)
{
	int timeout;

	/* Convert timeout in millsecond to jiffies */
	timeout = msecs_to_jiffies(tmo_ms);

	timeout = wait_event_timeout(*waitq, (*var == condition), timeout);

	return timeout;
}

/* WL2WBRC - wlan FW downloaded */
void
wl2wbrc_wlan_on_finished(void)
{
	struct wbrc_pvt_data *wbrc_data = g_wbrc_data;
	int state = 0;

	pr_err("%s: enter \n", __func__);
	if (!wbrc_data) {
		pr_err("%s: not allocated mem\n", __func__);
		return;
	}

	WBRC_STATE_LOCK(wbrc_data);

	state = wbrc_data->state;

	if (state != WLAN_ON_IN_PROGRESS) {
		pr_err("%s: unexpected state %d !\n", __func__, state);
	} else {
		wbrc_data->state = IDLE;
		pr_err("%s: state -> IDLE \n", __func__);
		wake_up(&wbrc_data->state_change_waitq);
	}

	WBRC_STATE_UNLOCK(wbrc_data);
}

/* WL2WBRC - wlan ON start */
int
wl2wbrc_wlan_on(void)
{
	struct wbrc_pvt_data *wbrc_data = g_wbrc_data;
	int state = 0;
	int ret = WBRC_OK;
	int tmo_ret = 0;

	pr_err("%s: enter \n", __func__);
	if (!wbrc_data) {
		pr_err("%s: not allocated mem\n", __func__);
		return WBRC_ERR;
	}

	WBRC_STATE_LOCK(wbrc_data);

	state = wbrc_data->state;

	if (state == BT_ON_IN_PROGRESS ||
		state == BT_FW_DWNLD_IN_PROGRESS) {
		WBRC_STATE_UNLOCK(wbrc_data);
		/* wait for state to change to IDLE
		 * and return nop
		 */
		pr_err("%s: state=%u, wait for state to become IDLE \n",
			__func__, state);
		tmo_ret = wbrc_wait_on_condition(&wbrc_data->state_change_waitq,
			WBRC_WAIT_TIMEOUT, &wbrc_data->state, IDLE);
		if (tmo_ret <= 0) {
			pr_err("%s: wait for state to change to IDLE timed out !\n", __func__);
			ret = WBRC_ERR;
		} else {
			pr_err("%s: state is now IDLE \n", __func__);
			ret = WBRC_NOP;
		}
		WBRC_STATE_LOCK(wbrc_data);
	}

	wbrc_data->state = WLAN_ON_IN_PROGRESS;
	pr_err("%s: state -> WLAN_ON_IN_PROGRESS \n", __func__);

	WBRC_STATE_UNLOCK(wbrc_data);

	return ret;
}

/* WBRC_LOCK should be held from caller */
static int
bt2wbrc_ack_bt_reset(struct wbrc_pvt_data *wbrc_data)
{
	int ret = 0;

	pr_err("%s: enter \n", __func__);

	wbrc_data->bt_reset_ack = TRUE;
	smp_wmb();
	wake_up(&wbrc_data->bt_reset_waitq);

	return ret;
}

#ifdef BT_FW_DWNLD
static int
bt2wbrc_bt_fw_dwnld(struct wbrc_pvt_data *wbrc_data, void *fwblob, uint len)
{
	int ret = 0, tmo_ret = 0;
	int state = 0;

	WBRC_STATE_LOCK(wbrc_data);

	state = wbrc_data->state;

	/* check for invalid states */
	if (state == BT_ON_IN_PROGRESS ||
		state == BT_FW_DWNLD_IN_PROGRESS) {
		pr_err("%s: Unexpected state %u !\n", __func__, state);
		WBRC_STATE_UNLOCK(wbrc_data);
		ret = WBRC_ERR;
		goto exit;
	}

	/* wait if WLAN ON or WLAN recovery is in progress */
	if (state == WLAN_ON_IN_PROGRESS ||
		state == WLAN_OFF_IN_PROGRESS) {
		WBRC_STATE_UNLOCK(wbrc_data);
		pr_err("%s: state=%u, wait for WLAN ON/OFF to finish \n", __func__, state);
		tmo_ret = wbrc_wait_on_condition(&wbrc_data->state_change_waitq,
			WBRC_WAIT_TIMEOUT, &wbrc_data->state, IDLE);
		if (tmo_ret <= 0) {
			pr_err("%s: WLAN ON/OFF timed out !\n", __func__);
			ret = WBRC_ERR;
			goto exit;
		} else {
			pr_err("%s: WLAN ON/OFF finished\n", __func__);
		}
		WBRC_STATE_LOCK(wbrc_data);
	}

	wbrc_data->state = BT_FW_DWNLD_IN_PROGRESS;
	pr_err("%s: state -> BT_FW_DWNLD_IN_PROGRESS \n", __func__);

	WBRC_STATE_UNLOCK(wbrc_data);

	/* resume pcie link */
	if (wbrc2wl_wlan_pcie_link_request(wbrc_data->wl_hdl) == WBRC_OK) {
		/* download the BT FW over BAR2 */
		ret = wbrc2wl_wlan_dwnld_bt_fw(wbrc_data->wl_hdl, fwblob, len);
		if (ret) {
			pr_err("%s: BT FW dwnld fails with error %d! \n", __func__, ret);
		}
	} else {
		pr_err("%s: wbrc2wl_wlan_pcie_link_request returns error! \n", __func__);
		ret = WBRC_ERR;
	}

	/* change state back to IDLE */
	WBRC_STATE_LOCK(wbrc_data);
	wbrc_data->state = IDLE;
	pr_err("%s: state -> IDLE \n", __func__);
	WBRC_STATE_UNLOCK(wbrc_data);

	wake_up(&wbrc_data->state_change_waitq);

exit:
	return ret;
}
#endif /* BT_FW_DWNLD */

/* WL2WBRC - wlan recovery start */
int
wl2wbrc_wlan_off(void)
{
	struct wbrc_pvt_data *wbrc_data = g_wbrc_data;
	int tmo_ret = 0, state = 0;
	int ret = 0;

	pr_info("%s: enter \n", __func__);
	if (!wbrc_data) {
		pr_err("%s: not allocated mem\n", __func__);
		return WBRC_ERR;
	}

	WBRC_STATE_LOCK(wbrc_data);

	state = wbrc_data->state;

	if (state == BT_ON_IN_PROGRESS ||
		state == BT_FW_DWNLD_IN_PROGRESS) {
		WBRC_STATE_UNLOCK(wbrc_data);
		/* wait for state to change to IDLE */
		pr_err("%s: state=%u, wait for state to become IDLE \n",
			__func__, state);
		tmo_ret = wbrc_wait_on_condition(&wbrc_data->state_change_waitq,
			WBRC_WAIT_TIMEOUT, &wbrc_data->state, IDLE);
		if (tmo_ret <= 0) {
			pr_err("%s: wait for state to change to IDLE timed out !\n", __func__);
			ret = WBRC_ERR;
		} else {
			pr_err("%s: state is now IDLE \n", __func__);
		}
		WBRC_STATE_LOCK(wbrc_data);
	}

	wbrc_data->state = WLAN_OFF_IN_PROGRESS;
	pr_err("%s: state -> WLAN_OFF_IN_PROGRESS \n", __func__);
	WBRC_STATE_UNLOCK(wbrc_data);

	return ret;
}

/* WL2WBRC - wlan OFF complete */
int
wl2wbrc_wlan_off_finished(void)
{
	struct wbrc_pvt_data *wbrc_data = g_wbrc_data;
	int state = 0;
	int ret = 0;

	if (!wbrc_data) {
		pr_err("%s: not allocated mem\n", __func__);
		return WBRC_ERR;
	}

	WBRC_STATE_LOCK(wbrc_data);
	state = wbrc_data->state;
	if (state == WLAN_OFF_IN_PROGRESS) {
		wbrc_data->state = IDLE;
		wake_up(&wbrc_data->state_change_waitq);
		pr_err("%s: state -> IDLE \n", __func__);
	} else {
		pr_err("%s: unexpected state %u ! \n", __func__, state);
		ret = WBRC_ERR;
	}
	WBRC_STATE_UNLOCK(wbrc_data);

	return ret;
}

/* WL2WBRC - request reset of BT stack and wait for ack, for fatal errors */
int
wl2wbrc_req_bt_reset(void)
{
	int ret = 0, tmo_ret = 0;
	struct wbrc_pvt_data *wbrc_data;

	pr_err("%s: enter \n", __func__);
	WBRC_LOCK(wbrc_mutex);
	wbrc_data = g_wbrc_data;
	if (!wbrc_data) {
		pr_err("%s: not allocated mem\n", __func__);
		return WBRC_ERR;
	}

	if (!wbrc_data->bt_dev_opened) {
		pr_info("%s: no BT\n", __func__);
		WBRC_UNLOCK(wbrc_mutex);
		return WBRC_OK;
	}
	pr_err("%s: fatal error, reset BT... \n", __func__);
	/* bt_reset_ack will be set after BT acks for reset */
	wbrc_data->bt_reset_ack = FALSE;
	wbrc2bt_cmd_bt_reset(wbrc_data);
	WBRC_UNLOCK(wbrc_mutex);

	tmo_ret = wbrc_wait_on_condition(&wbrc_data->bt_reset_waitq,
		WBRC_WAIT_BT_RESET_ACK_TIMEOUT,
		&wbrc_data->bt_reset_ack, TRUE);
	if (tmo_ret <= 0) {
		pr_err("%s: BT reset ack timeout !\n", __func__);
		ret = WBRC_ERR;
	} else {
		pr_err("%s: got BT reset ack\n", __func__);
	}

	return ret;
}

#ifdef WBRC_HW_QUIRKS
void
wl2wbrc_wlan_before_regoff(void)
{
	struct wbrc_pvt_data *wbrc_data = g_wbrc_data;
	int tmo_ret = 0;

	if (WBRC_CHKCHIP(wbrc_data->chipid)) {
		return;
	}

	pr_err("%s: enter \n", __func__);
	WBRC_ONOFF_LOCK(wbrc_data);
	if (wbrc_data->bt_regon_inprogress) {
		WBRC_ONOFF_UNLOCK(wbrc_data);
		pr_err("%s: wait for BT regon to finish... \n", __func__);
		tmo_ret = wbrc_wait_on_condition(&wbrc_data->onoff_waitq,
			WBRC_ONOFF_WAIT_TIMEOUT, &wbrc_data->bt_regon_inprogress, FALSE);
		if (tmo_ret <= 0) {
			pr_err("%s: wait for BT regon to finish timed out !\n", __func__);
		} else {
			pr_err("%s: BT regon finished \n", __func__);
		}
		WBRC_ONOFF_LOCK(wbrc_data);
	}
	wbrc_data->wl_regoff_inprogress = TRUE;
	WBRC_ONOFF_UNLOCK(wbrc_data);
	pr_err("%s: exit \n", __func__);
}

void
wl2wbrc_wlan_after_regoff(void)
{
	struct wbrc_pvt_data *wbrc_data = g_wbrc_data;

	if (WBRC_CHKCHIP(wbrc_data->chipid)) {
		return;
	}

	wbrc_data->wl_after_regoff_ts = wbrc_sysuptime_ns();
	pr_err("%s: enter \n", __func__);
	WBRC_ONOFF_LOCK(wbrc_data);
	wbrc_data->wl_regoff_inprogress = FALSE;
	WBRC_ONOFF_UNLOCK(wbrc_data);
	pr_err("%s: exit \n", __func__);
}

void
wl2wbrc_wlan_before_regon(void)
{
	struct wbrc_pvt_data *wbrc_data = g_wbrc_data;
	int tmo_ret = 0;
	unsigned long long curts = 0;

	if (WBRC_CHKCHIP(wbrc_data->chipid)) {
		return;
	}

	curts = wbrc_sysuptime_ns();
	pr_err("%s: enter \n", __func__);

	if (wbrc_data->bt_after_regoff_ts && curts &&
		(wbrc_data->bt_after_regoff_ts < curts) &&
		((curts - wbrc_data->bt_after_regoff_ts) <
		 (MIN_REGOFF_TO_REGON_DELAY * NSEC_PER_MSEC))) {
		pr_err("%s: WL_REG_ON requested within %u ms of last BT_REG_OFF !"
			" curts=%llu ns; bt_after_regoff_ts=%llu ns\n", __func__,
			MIN_REGOFF_TO_REGON_DELAY, curts, wbrc_data->bt_after_regoff_ts);
	}
	if (wbrc_data->wl_after_regoff_ts && curts &&
		(wbrc_data->wl_after_regoff_ts < curts) &&
		((curts - wbrc_data->wl_after_regoff_ts) <
		 (MIN_REGOFF_TO_REGON_DELAY * NSEC_PER_MSEC))) {
		pr_err("%s: WL_REG_ON requested within %u ms of last WL_REG_OFF !"
			" curts=%llu ns; wl_after_regoff_ts=%llu ns\n", __func__,
			MIN_REGOFF_TO_REGON_DELAY, curts, wbrc_data->wl_after_regoff_ts);
	}

	WBRC_ONOFF_LOCK(wbrc_data);
	wbrc_data->wl_regon_inprogress = TRUE;
	if (wbrc_data->bt_regoff_inprogress) {
		WBRC_ONOFF_UNLOCK(wbrc_data);
		pr_err("%s:wait for BT regoff to finish... \n", __func__);
		tmo_ret = wbrc_wait_on_condition(&wbrc_data->onoff_waitq,
			WBRC_ONOFF_WAIT_TIMEOUT, &wbrc_data->bt_regoff_inprogress, FALSE);
		if (tmo_ret <= 0) {
			pr_err("%s: wait for BT regoff to finish timed out !\n", __func__);
		} else {
			pr_err("%s:BT regoff finished \n", __func__);
		}
		WBRC_ONOFF_LOCK(wbrc_data);
	}
	WBRC_ONOFF_UNLOCK(wbrc_data);

	pr_err("%s: delay %u ms \n", __func__, MIN_REGOFF_TO_REGON_DELAY);
	wbrc_delay(MIN_REGOFF_TO_REGON_DELAY);

	curts = wbrc_sysuptime_ns();
	if (wbrc_data->bt_after_regoff_ts && curts &&
		wbrc_data->bt_after_regoff_ts < curts) {
		pr_err("%s: delta b/w last BT_REG_OFF and this WL_REG_ON = %llu ns\n",
			__func__, curts - wbrc_data->bt_after_regoff_ts);
	}
	if (wbrc_data->wl_after_regoff_ts && curts &&
		wbrc_data->wl_after_regoff_ts < curts) {
		pr_err("%s: delta b/w last WL_REG_OFF and this WL_REG_ON = %llu ns\n",
			__func__, curts - wbrc_data->wl_after_regoff_ts);
	}
	pr_err("%s: exit \n", __func__);
}

void
wl2wbrc_wlan_after_regon(void)
{
	struct wbrc_pvt_data *wbrc_data = g_wbrc_data;

	if (WBRC_CHKCHIP(wbrc_data->chipid)) {
		return;
	}

	pr_err("%s: enter \n", __func__);
	WBRC_ONOFF_LOCK(wbrc_data);
	wbrc_data->wl_regon_inprogress = FALSE;
	WBRC_ONOFF_UNLOCK(wbrc_data);
	pr_err("%s: exit \n", __func__);
}

static void
bt2wbrc_bt_before_regoff(void)
{
	struct wbrc_pvt_data *wbrc_data = g_wbrc_data;
	int tmo_ret = 0;

	pr_err("%s: enter \n", __func__);
	WBRC_ONOFF_LOCK(wbrc_data);
	if (wbrc_data->wl_regon_inprogress) {
		WBRC_ONOFF_UNLOCK(wbrc_data);
		pr_err("%s: wait for WL regon to finish... \n", __func__);
		tmo_ret = wbrc_wait_on_condition(&wbrc_data->onoff_waitq,
			WBRC_ONOFF_WAIT_TIMEOUT, &wbrc_data->wl_regon_inprogress, FALSE);
		if (tmo_ret <= 0) {
			pr_err("%s: wait for WL regon to finish timed out !\n", __func__);
		} else {
			pr_err("%s: WL regon finished \n", __func__);
		}
		WBRC_ONOFF_LOCK(wbrc_data);
	}
	wbrc_data->bt_regoff_inprogress = TRUE;
	WBRC_ONOFF_UNLOCK(wbrc_data);
	pr_err("%s: exit \n", __func__);
}

static void
bt2wbrc_bt_after_regoff(void)
{
	struct wbrc_pvt_data *wbrc_data = g_wbrc_data;

	wbrc_data->bt_after_regoff_ts = wbrc_sysuptime_ns();
	pr_err("%s: enter \n", __func__);
	WBRC_ONOFF_LOCK(wbrc_data);
	wbrc_data->bt_regoff_inprogress = FALSE;
	WBRC_ONOFF_UNLOCK(wbrc_data);
	pr_err("%s: exit \n", __func__);

}

static void
bt2wbrc_bt_before_regon(void)
{
	struct wbrc_pvt_data *wbrc_data = g_wbrc_data;
	int tmo_ret = 0;
	unsigned long long curts = 0;

	curts = wbrc_sysuptime_ns();
	pr_err("%s: enter \n", __func__);

	if (wbrc_data->wl_after_regoff_ts && curts &&
		(wbrc_data->wl_after_regoff_ts < curts) &&
		((curts - wbrc_data->wl_after_regoff_ts) <
		 (MIN_REGOFF_TO_REGON_DELAY * NSEC_PER_MSEC))) {
		pr_err("%s: BT_REG_ON requested within %u ms of last WL_REG_OFF !"
			" curts=%llu ns; wl_after_regoff_ts=%llu ns\n", __func__,
			MIN_REGOFF_TO_REGON_DELAY, curts, wbrc_data->wl_after_regoff_ts);
	}
	if (wbrc_data->bt_after_regoff_ts && curts &&
		(wbrc_data->bt_after_regoff_ts < curts) &&
		((curts - wbrc_data->bt_after_regoff_ts) <
		 (MIN_REGOFF_TO_REGON_DELAY * NSEC_PER_MSEC))) {
		pr_err("%s: BT_REG_ON requested within %u ms of last BT_REG_OFF !"
			" curts=%llu ns; bt_after_regoff_ts=%llu ns\n", __func__,
			MIN_REGOFF_TO_REGON_DELAY, curts, wbrc_data->bt_after_regoff_ts);
	}

	WBRC_ONOFF_LOCK(wbrc_data);
	wbrc_data->bt_regon_inprogress = TRUE;
	if (wbrc_data->wl_regoff_inprogress) {
		WBRC_ONOFF_UNLOCK(wbrc_data);
		pr_err("%s:wait for WL regoff to finish... \n", __func__);
		tmo_ret = wbrc_wait_on_condition(&wbrc_data->onoff_waitq,
			WBRC_ONOFF_WAIT_TIMEOUT, &wbrc_data->wl_regoff_inprogress, FALSE);
		if (tmo_ret <= 0) {
			pr_err("%s: wait for WL regoff to finish timed out !\n", __func__);
		} else {
			pr_err("%s:WL regoff finished \n", __func__);
		}
		WBRC_ONOFF_LOCK(wbrc_data);
	}
	WBRC_ONOFF_UNLOCK(wbrc_data);

	pr_err("%s: delay %u ms \n", __func__, MIN_REGOFF_TO_REGON_DELAY);
	wbrc_delay(MIN_REGOFF_TO_REGON_DELAY);

	curts = wbrc_sysuptime_ns();
	if (wbrc_data->wl_after_regoff_ts && curts &&
		wbrc_data->wl_after_regoff_ts < curts) {
		pr_err("%s: delta b/w last WL_REG_OFF and this BT_REG_ON = %llu ns\n",
			__func__, curts - wbrc_data->wl_after_regoff_ts);
	}
	if (wbrc_data->bt_after_regoff_ts && curts &&
		wbrc_data->bt_after_regoff_ts < curts) {
		pr_err("%s: delta b/w last BT_REG_OFF and this BT_REG_ON = %llu ns\n",
			__func__, curts - wbrc_data->bt_after_regoff_ts);
	}

	pr_err("%s: exit \n", __func__);
}

static void
bt2wbrc_bt_after_regon(void)
{
	struct wbrc_pvt_data *wbrc_data = g_wbrc_data;

	pr_err("%s: enter \n", __func__);
	WBRC_ONOFF_LOCK(wbrc_data);
	wbrc_data->bt_regon_inprogress = FALSE;
	WBRC_ONOFF_UNLOCK(wbrc_data);
	pr_err("%s: exit \n", __func__);

}

static void
bt2wbrc_process_reg_onoff_cmds(int cmd)
{
	switch (cmd) {
		case WBRC_CMD_BT_BEFORE_REG_OFF:
			bt2wbrc_bt_before_regoff();
			break;
		case WBRC_CMD_BT_AFTER_REG_OFF:
			bt2wbrc_bt_after_regoff();
			break;
		case WBRC_CMD_BT_BEFORE_REG_ON:
			bt2wbrc_bt_before_regon();
			break;
		case WBRC_CMD_BT_AFTER_REG_ON:
			bt2wbrc_bt_after_regon();
			break;
		default:
			pr_err("%s: unknown cmd %d\n", __func__, cmd);
	}
}
#endif /* WBRC_HW_QUIRKS */
