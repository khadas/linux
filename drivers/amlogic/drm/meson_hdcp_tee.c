// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/miscdevice.h>
#include <drm/drmP.h>
#include <linux/amlogic/media/vout/hdmi_tx/hdmi_common.h>
#include <linux/amlogic/media/vout/hdmi_tx/hdmi_tx_module.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>
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
		rtn_val = 0;
		get_hdcp_hdmitx_version(am_hdmi);
		if (am_hdmi->hdcp_tx_type & 0x2)
			am_hdcp22_init(am_hdmi);
		break;
	case TEE_HDCP_IOC_END:
		rtn_val = 0;
		break;
	case HDCP_DAEMON_IOC_LOAD_END:
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
				cancel_delayed_work(&am_hdmi->hdcp_prd_proc);
				flush_workqueue(am_hdmi->hdcp_wq);
				am_hdcp_disable(am_hdmi);
				queue_delayed_work(am_hdmi->hdcp_wq,
					&am_hdmi->hdcp_prd_proc, 0);
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
	poll_wait(file, &am_hdmi->hdcp_comm_queue, wait);
	if (am_hdmi->hdcp_report != am_hdmi->hdcp_poll_report) {
		mask = POLLIN | POLLRDNORM;
		am_hdmi->hdcp_poll_report = am_hdmi->hdcp_report;
	}
	return mask;
}

static const struct file_operations hdcp_comm_file_operations = {
	.unlocked_ioctl = hdcp_comm_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = hdcp_comm_ioctl,
#endif
	.poll = hdcp_comm_poll,
	.owner = THIS_MODULE,
};

int hdcp_comm_init(struct am_hdmi_tx *am_hdmi)
{
	am_hdmi->hdcp_user_type = 3;
	init_waitqueue_head(&am_hdmi->hdcp_comm_queue);
	am_hdmi->hdcp_wq = alloc_workqueue(DEVICE_NAME,
		WQ_HIGHPRI | WQ_CPU_INTENSIVE, 0);
	INIT_DELAYED_WORK(&am_hdmi->hdcp_prd_proc,
			  am_hdcp_enable);
	am_hdmi->hdcp_comm_device.minor = MISC_DYNAMIC_MINOR;
	am_hdmi->hdcp_comm_device.name = "tee_comm_hdcp";
	am_hdmi->hdcp_comm_device.fops = &hdcp_comm_file_operations;
	return misc_register(&am_hdmi->hdcp_comm_device);
}

void hdcp_comm_exit(struct am_hdmi_tx *am_hdmi)
{
	misc_deregister(&am_hdmi->hdcp_comm_device);
}

