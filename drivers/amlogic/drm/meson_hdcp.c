// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <drm/drm_modeset_helper.h>
#include <drm/drmP.h>
#include <drm/drm_edid.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_hdcp.h>

#include <linux/component.h>
#include <linux/irq.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/kthread.h>
#include <linux/workqueue.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/media/vout/hdmi_tx/hdmi_tx_module.h>
#include <linux/arm-smccc.h>
#include <linux/workqueue.h>
#include "meson_drv.h"
#include "meson_hdmi.h"
#include "meson_hdcp.h"

/* ioctl numbers */
enum {
	TEE_HDCP_START,
	TEE_HDCP_END,
	HDCP_DAEMON_LOAD_END,
	HDCP_DAEMON_REPORT,
	HDCP_EXE_VER_SET,
	HDCP_TX_VER_REPORT,
	HDCP_DOWNSTR_VER_REPORT,
	HDCP_EXE_VER_REPORT
};

#define TEE_HDCP_IOC_START    _IOW('P', TEE_HDCP_START, int)
#define TEE_HDCP_IOC_END    _IOW('P', TEE_HDCP_END, int)

#define HDCP_DAEMON_IOC_LOAD_END    _IOW('P', HDCP_DAEMON_LOAD_END, int)
#define HDCP_DAEMON_IOC_REPORT    _IOR('P', HDCP_DAEMON_REPORT, int)

#define HDCP_EXE_VER_IOC_SET    _IOW('P', HDCP_EXE_VER_SET, int)
#define HDCP_TX_VER_IOC_REPORT    _IOR('P', HDCP_TX_VER_REPORT, int)
#define HDCP_DOWNSTR_VER_IOC_REPORT    _IOR('P', HDCP_DOWNSTR_VER_REPORT, int)
#define HDCP_EXE_VER_IOC_REPORT    _IOR('P', HDCP_EXE_VER_REPORT, int)

static unsigned int get_hdcp_downstream_ver(struct am_hdmi_tx *am_hdmi)
{
	unsigned int hdcp_downstream_type = drm_get_rx_hdcp_cap();

	DRM_INFO("downstream support hdcp14: %d\n",
		 hdcp_downstream_type & 0x1);
	DRM_INFO("downstream support hdcp22: %d\n",
		 (hdcp_downstream_type & 0x2) >> 1);

	return hdcp_downstream_type;
}

static unsigned int get_hdcp_hdmitx_version(struct am_hdmi_tx *am_hdmi)
{
	unsigned int hdcp_tx_type = drm_hdmitx_get_hdcp_cap();

	DRM_INFO("hdmitx support hdcp14: %d\n",
		 hdcp_tx_type & 0x1);
	DRM_INFO("hdmitx support hdcp22: %d\n",
		 (hdcp_tx_type & 0x2) >> 1);
	return hdcp_tx_type;
}

static int am_get_hdcp_exe_type(struct am_hdmi_tx *am_hdmi)
{
	int type;

	am_hdmi->hdcp_tx_type = get_hdcp_hdmitx_version(am_hdmi);
	DRM_INFO("%s usr_type: %d, tx_type: %d\n",
		 __func__, am_hdmi->hdcp_user_type, am_hdmi->hdcp_tx_type);
	if (am_hdmi->hdcp_user_type == 0)
		return HDCP_NULL;
	if (/* !am_hdmi->hdcp_downstream_type && */
		am_hdmi->hdcp_tx_type)
		am_hdmi->hdcp_downstream_type =
			get_hdcp_downstream_ver(am_hdmi);
	switch (am_hdmi->hdcp_tx_type & 0x3) {
	case 0x3:
		if ((am_hdmi->hdcp_downstream_type & 0x2) &&
			(am_hdmi->hdcp_user_type & 0x2))
			type = HDCP_MODE22;
		else if ((am_hdmi->hdcp_downstream_type & 0x1) &&
			 (am_hdmi->hdcp_user_type & 0x1))
			type = HDCP_MODE14;
		else
			type = HDCP_NULL;
		break;
	case 0x2:
		if ((am_hdmi->hdcp_downstream_type & 0x2) &&
			(am_hdmi->hdcp_user_type & 0x2))
			type = HDCP_MODE22;
		else
			type = HDCP_NULL;
		break;
	case 0x1:
		if ((am_hdmi->hdcp_downstream_type & 0x1) &&
			(am_hdmi->hdcp_user_type & 0x1))
			type = HDCP_MODE14;
		else
			type = HDCP_NULL;
		break;
	default:
		type = HDCP_NULL;
		DRM_INFO("[%s]: TX no hdcp key\n", __func__);
		break;
	}
	return type;
}

void am_hdcp_disable(struct am_hdmi_tx *am_hdmi)
{
	DRM_INFO("[%s]: %d\n", __func__, am_hdmi->hdcp_execute_type);
	/* TODO: whether to keep exe type */
	/* if (am_hdmi->hdcp_execute_type == HDCP_MODE22) { */
		am_hdmi->hdcp_report = HDCP_TX22_DISCONNECT;
		/* wait for tx22 to enter unconnected state */
		msleep(200);
		am_hdmi->hdcp_report = HDCP_TX22_STOP;
		/* wakeup hdcp_tx22 to stop hdcp22 */
		wake_up(&am_hdmi->hdcp_comm_queue);
		/* wait for hdcp_tx22 stop hdcp22 done */
		msleep(200);
		drm_hdmitx_hdcp_disable(HDCP_MODE22);
	/* } else if (am_hdmi->hdcp_execute_type  == HDCP_MODE14) { */
		drm_hdmitx_hdcp_disable(HDCP_MODE14);
	/* } */
	am_hdmi->hdcp_execute_type = HDCP_NULL;
	am_hdmi->hdcp_result = 0;
	am_hdmi->hdcp_en = 0;
}

void am_hdcp_disconnect(struct am_hdmi_tx *am_hdmi)
{
	/* if (am_hdmi->hdcp_execute_type == HDCP_MODE22) */
		/* drm_hdmitx_hdcp_disable(HDCP_MODE22); */
	/* else if (am_hdmi->hdcp_execute_type  == HDCP_MODE14) */
		/* drm_hdmitx_hdcp_disable(HDCP_MODE14); */
	am_hdmi->hdcp_report = HDCP_TX22_DISCONNECT;
	am_hdmi->hdcp_downstream_type = 0;
	/* TODO: for suspend/resume, need to stop/start hdcp22
	 * need to keep exe type
	 */
	am_hdmi->hdcp_execute_type = HDCP_NULL;
	am_hdmi->hdcp_result = 0;
	am_hdmi->hdcp_en = 0;
	DRM_INFO("[%s]: HDCP_TX22_DISCONNECT\n", __func__);
	wake_up(&am_hdmi->hdcp_comm_queue);
}

void am_hdcp_enable(struct am_hdmi_tx *am_hdmi)
{
	/* hdcp enabled, but may not start auth as key not ready */
	am_hdmi->hdcp_en = 1;
	am_hdmi->hdcp_execute_type = am_get_hdcp_exe_type(am_hdmi);

	if (am_hdmi->hdcp_execute_type == HDCP_MODE22) {
		drm_hdmitx_hdcp_enable(2);
		am_hdmi->hdcp_report = HDCP_TX22_START;
		msleep(50);
		wake_up(&am_hdmi->hdcp_comm_queue);
	} else if (am_hdmi->hdcp_execute_type == HDCP_MODE14) {
		drm_hdmitx_hdcp_enable(1);
	}
	DRM_INFO("[%s]: report=%d, use_type=%u, execute=%u\n",
		 __func__, am_hdmi->hdcp_report,
		 am_hdmi->hdcp_user_type, am_hdmi->hdcp_execute_type);
}

static long hdcp_comm_ioctl(struct file *file,
			    unsigned int cmd,
			    unsigned long arg)
{
	int rtn_val;
	unsigned int out_size;
	struct miscdevice *phdcp_comm_device;
	struct am_hdmi_tx *am_hdmi;

	phdcp_comm_device = file->private_data;
	am_hdmi = container_of(phdcp_comm_device,
			       struct am_hdmi_tx,
			       hdcp_comm_device);

	switch (cmd) {
	case TEE_HDCP_IOC_START:
		/* notify by TEE, hdcp key ready, echo 2 > hdcp_mode */
		rtn_val = 0;
		am_hdmi->hdcp_tx_type = get_hdcp_hdmitx_version(am_hdmi);
		if (am_hdmi->hdcp_tx_type & 0x2)
			drm_hdmitx_hdcp22_init();
		DRM_INFO("hdcp key load ready\n");
		break;
	case TEE_HDCP_IOC_END:
		rtn_val = 0;
		break;
	case HDCP_DAEMON_IOC_LOAD_END:
		/* hdcp_tx22 load ready (after TEE key ready) */
		DRM_INFO("IOC_LOAD_END %d, %d\n",
			 am_hdmi->hdcp_report, am_hdmi->hdcp_poll_report);
		if (am_hdmi->hdcp_en && am_hdmi->hdcp_execute_type == 0) {
			am_hdcp_enable(am_hdmi);
			DRM_INFO("hdcp start before key ready, restart: %d\n",
				 am_hdmi->hdcp_execute_type);
		}
		am_hdmi->hdcp_poll_report = am_hdmi->hdcp_report;
		rtn_val = 0;
		break;
	case HDCP_DAEMON_IOC_REPORT:
		rtn_val = copy_to_user((void __user *)arg,
				       (void *)&am_hdmi->hdcp_report,
				       sizeof(am_hdmi->hdcp_report));
		if (rtn_val != 0) {
			out_size = sizeof(am_hdmi->hdcp_report);
			DRM_INFO("out_size: %u, leftsize: %u\n",
				 out_size, rtn_val);
			rtn_val = -1;
		}
		break;
	case HDCP_EXE_VER_IOC_SET:
		if (arg > 2) {
			rtn_val = -1;
		} else {
			am_hdmi->hdcp_user_type = arg;
			rtn_val = 0;
			if (hdmitx_hpd_hw_op(HPD_READ_HPD_GPIO)) {
				am_hdcp_disable(am_hdmi);
				am_hdcp_enable(am_hdmi);
			}
		}
		break;
	case HDCP_TX_VER_IOC_REPORT:
		rtn_val = copy_to_user((void __user *)arg,
				       (void *)&am_hdmi->hdcp_tx_type,
				       sizeof(am_hdmi->hdcp_tx_type));
		if (rtn_val != 0) {
			out_size = sizeof(am_hdmi->hdcp_tx_type);
			DRM_INFO("out_size: %u, leftsize: %u\n",
				 out_size, rtn_val);
			rtn_val = -1;
		}
		break;
	case HDCP_DOWNSTR_VER_IOC_REPORT:
		rtn_val = copy_to_user((void __user *)arg,
				       (void *)&am_hdmi->hdcp_downstream_type,
				       sizeof(am_hdmi->hdcp_downstream_type));
		if (rtn_val != 0) {
			out_size = sizeof(am_hdmi->hdcp_downstream_type);
			DRM_INFO("out_size: %u, leftsize: %u\n",
				 out_size, rtn_val);
			rtn_val = -1;
		}
		break;
	case HDCP_EXE_VER_IOC_REPORT:
		rtn_val = copy_to_user((void __user *)arg,
				       (void *)&am_hdmi->hdcp_execute_type,
				       sizeof(am_hdmi->hdcp_execute_type));
		if (rtn_val != 0) {
			out_size = sizeof(am_hdmi->hdcp_execute_type);
			DRM_INFO("out_size: %u, leftsize: %u\n",
				 out_size, rtn_val);
			rtn_val = -1;
		}
		break;
	default:
		rtn_val = -EPERM;
	}
	return rtn_val;
}

static unsigned int hdcp_comm_poll(struct file *file, poll_table *wait)
{
	struct miscdevice *phdcp_comm_device;
	struct am_hdmi_tx *am_hdmi;
	unsigned int mask = 0;

	phdcp_comm_device = file->private_data;
	am_hdmi = container_of(phdcp_comm_device,
			       struct am_hdmi_tx,
			       hdcp_comm_device);
	DRM_INFO("hdcp_poll %d, %d\n", am_hdmi->hdcp_report, am_hdmi->hdcp_poll_report);
	poll_wait(file, &am_hdmi->hdcp_comm_queue, wait);
	if (am_hdmi->hdcp_report != am_hdmi->hdcp_poll_report) {
		mask = POLLIN | POLLRDNORM;
		am_hdmi->hdcp_poll_report = am_hdmi->hdcp_report;
	}
	return mask;
}

static const struct file_operations hdcp_comm_file_operations = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = hdcp_comm_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = hdcp_comm_ioctl,
#endif
	.poll = hdcp_comm_poll,
};

void hdcp_comm_init(struct am_hdmi_tx *am_hdmi)
{
	int ret = 0;

	am_hdmi->hdcp_user_type = 0x3;
	am_hdmi->hdcp_report = HDCP_TX22_DISCONNECT;
	init_waitqueue_head(&am_hdmi->hdcp_comm_queue);
	am_hdmi->hdcp_comm_device.minor = MISC_DYNAMIC_MINOR;
	am_hdmi->hdcp_comm_device.name = "tee_comm_hdcp";
	am_hdmi->hdcp_comm_device.fops = &hdcp_comm_file_operations;
	ret = misc_register(&am_hdmi->hdcp_comm_device);
	if (ret < 0)
		DRM_ERROR("%s [ERROR] misc_register fail\n", __func__);
}

void hdcp_comm_exit(struct am_hdmi_tx *am_hdmi)
{
	misc_deregister(&am_hdmi->hdcp_comm_device);
}

