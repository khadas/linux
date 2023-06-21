// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * File    : amlogic-usb-cam.c
 * Purpose : USB CAM driver
 *
 * Based on USB driver
 *
 *
 *
 *
 */
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kref.h>
#include <linux/usb.h>
#include <linux/mutex.h>
#include <asm/uaccess.h>
#include <linux/mm.h>
#include <linux/moduleparam.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/kthread.h>
#include <linux/gfp.h>
#include <linux/compat.h>
#include <asm/current.h>
#include <linux/poll.h>
#include <linux/fs.h>
#include <linux/vmalloc.h>

#include "amlogic-usb-cam.h"
#include "aml_usbcam.h"

#define usbcam_dbg(fmt...) pr_info("usbcam: " fmt)
#define usbcam_err(fmt...) pr_err("usbcam: " fmt)
#define usbcam_crit(fmt...) pr_crit("usbcam: " fmt)

static unsigned long aml_get_usbcam_version(struct aml_usbcam *usbcam_dev, unsigned long arg)
{
	int ret;
	unsigned int driver_version = USBCAM_MODULE_DRIVER_VERSION;

	usbcam_dbg("get usbcam version\n");

	if (!usbcam_dev || !arg) {
		usbcam_err("invalid parameter\n");
		return -EINVAL;
	}

	ret = copy_to_user((unsigned int *)arg, &driver_version, sizeof(unsigned int));
	if (ret) {
		usbcam_err("copy data to user error\n");
		return ret;
	}

	return ret;
}

static unsigned long aml_get_usbcam_info(struct aml_usbcam *usbcam_dev, unsigned long arg)
{
	int ret;

	usbcam_dbg("get usbcam info\n");

	if (!usbcam_dev || !arg) {
		usbcam_err("invalid parameter\n");
		return -EINVAL;
	}

	ret = copy_to_user((struct aml_usbcam_info *)arg, &usbcam_dev->usbcam_module_info,
							sizeof(struct aml_usbcam_info));

	if (ret) {
		usbcam_err("copy data to user error\n");
		return ret;
	}

	return ret;
}

static unsigned long aml_get_usbcam_capabilities(struct aml_usbcam *usbcam_dev, unsigned long arg)
{
	int ret;

	usbcam_dbg("get usbcam capabilities\n");

	if (!usbcam_dev || !arg) {
		usbcam_err("invalid parameter\n");
		return -EINVAL;
	}

	ret = copy_to_user((struct aml_usbcam_module_capabilities *)arg,
				&usbcam_dev->usbcam_module_capabilities,
				sizeof(struct aml_usbcam_module_capabilities));

	if (ret) {
		usbcam_err("copy data to user error\n");
		return ret;
	}

	return ret;
}

static unsigned long aml_cancel_transfer(struct file *filp, struct aml_usbcam *usbcam_dev)
{
	if (!usbcam_dev) {
		usbcam_err("invalid parameter\n");
		return -EINVAL;
	}
	if ((filp->f_flags & 0x03) == O_RDONLY || (filp->f_flags & 0x03) == O_RDWR) {
		if (atomic_read(&usbcam_dev->read_urb->use_count)) {
			usbcam_dbg("unlink the read urb\n");
			usb_unlink_urb(usbcam_dev->read_urb);
		}
	}
	if ((filp->f_flags & 0x03) == O_RDWR || (filp->f_flags & 0x03) == O_WRONLY) {
		if (atomic_read(&usbcam_dev->write_urb->use_count)) {
			usbcam_dbg("unlink the write urb\n");
			usb_unlink_urb(usbcam_dev->write_urb);
		}
	}
	return 0;
}

static int aml_intf_open(struct inode *node, struct file *filp)
{
	struct aml_usbcam *usbcam_dev = NULL;
	struct usb_interface *intf = NULL;
	int minor;
	int ret = 0;

	usbcam_dbg("intf open\n");

	if (!node || !filp) {
		usbcam_err("invalid parameter\n");
		return -EINVAL;
	}

	minor = iminor(node);

	intf = usb_find_interface(&aml_usbcam_driver, minor);
	if (!intf) {
		usbcam_err("can not find device for minor %d\n", minor);
		return -ENODEV;
	}

	usbcam_dev = usb_get_intfdata(intf);
	if (!usbcam_dev) {
		usbcam_err("can not get intfdata\n");
		return -ENODEV;
	}

	usbcam_dev->device_status = DEVICE_STATUS_RUNNING;

	mutex_lock(&usbcam_dev->open_mux);

	if ((filp->f_flags & 0x03) == O_RDONLY || (filp->f_flags & 0x03) == O_RDWR) {
		usbcam_dev->read_open_status = READ_OPEN;
		if (usbcam_dev->read_open_ref >= 1) {
			usbcam_err("open failed:  interface read fd is opened before\n");
			mutex_unlock(&usbcam_dev->open_mux);
			ret = -EPERM;
		} else {
			usbcam_dev->read_open_ref++;
			usbcam_dbg(" interface read fd open successfully\n");
		}
	}

	if (ret < 0)
		return ret;

	if ((filp->f_flags & 0x03) == O_RDWR || (filp->f_flags & 0x03) == O_WRONLY) {
		usbcam_dev->write_open_status = WRITE_OPEN;
		if (usbcam_dev->write_open_ref >= 1) {
			usbcam_err("open failed:  interface write fd is opened before\n");
			if ((filp->f_flags & 0x03) == O_RDWR)
				usbcam_dev->read_open_ref--;
			mutex_unlock(&usbcam_dev->open_mux);
			ret = -EPERM;
		} else {
			usbcam_dev->write_open_ref++;
			usbcam_dbg(" interface write fd open successfully\n");
		}
	}

	if (ret < 0)
		return ret;

	mutex_unlock(&usbcam_dev->open_mux);

	kref_get(&usbcam_dev->ref_count);

	//save our object in the file's private structure
	filp->private_data = usbcam_dev;

	return 0;
}

static void aml_intf_read_complete(struct urb *urb)
{
	int status;
	struct aml_usbcam *usbcam_dev = NULL;
	ulong flag;

	if (!urb) {
		usbcam_err("invalid parameter\n");
		return;
	}

	status = urb->status;
	usbcam_dev = urb->context;

	spin_lock_irqsave(&usbcam_dev->read_lock, flag);
	usbcam_dev->in_transfor = 0;
	if (status) {
		usbcam_dev->read_len = 0;
		usbcam_dev->read_err = status;
		usbcam_dev->read_status = READ_STATUS_ERROR;
		usbcam_err("urb %p read urb error, status %d\n", urb, status);
	} else {
		usbcam_dev->read_len = urb->actual_length;
		usbcam_dev->read_err = 0;
		usbcam_dev->read_status = READ_STATUS_READY;
	}
	spin_unlock_irqrestore(&usbcam_dev->read_lock, flag);

	wake_up_interruptible(&usbcam_dev->read_wq);
}

static int aml_read_usb_submit(struct aml_usbcam *usbcam_dev)
{
	int pipe;
	int ret = 0;

	if (usb_endpoint_is_int_in(usbcam_dev->in)) {
		pipe = usb_rcvintpipe(usbcam_dev->usbdev, usbcam_dev->in->bEndpointAddress);
		usb_fill_int_urb(usbcam_dev->read_urb, usbcam_dev->usbdev, pipe,
						usbcam_dev->read_buf, usbcam_dev->buf_size,
						aml_intf_read_complete, usbcam_dev,
						usbcam_dev->in->bInterval);
	} else if (usb_endpoint_is_bulk_in(usbcam_dev->in)) {
		pipe = usb_rcvbulkpipe(usbcam_dev->usbdev, usbcam_dev->in->bEndpointAddress);
		usb_fill_bulk_urb(usbcam_dev->read_urb, usbcam_dev->usbdev, pipe,
						usbcam_dev->read_buf, usbcam_dev->buf_size,
						aml_intf_read_complete, usbcam_dev);
	}

	ret = usb_submit_urb(usbcam_dev->read_urb, GFP_KERNEL);
	if (ret) {
		usbcam_err("submit urb %p failed, ret = %d", usbcam_dev->read_urb, ret);
		return ret;
	}

	return 0;
}

static ssize_t aml_intf_read(struct file *filp, char *buff, size_t size, loff_t *ppos)
{
	int ret = 0;
	struct aml_usbcam *usbcam_dev = NULL;
	size_t available, transfor, chunk;
	ulong flag;

	if (!filp || !buff || !ppos) {
		usbcam_err("invalid parameter\n");
		return -EINVAL;
	}

	usbcam_dev = filp->private_data;
	if (!usbcam_dev) {
		usbcam_err("private data is null");
		return -ENODEV;
	}

	if (!usbcam_dev->intf) {
		usbcam_err("read failed, interface is deregistered\n");
		return -ENODEV;
	}

	spin_lock_irqsave(&usbcam_dev->read_lock, flag);
	if (usbcam_dev->read_status == READ_STATUS_READY) {
		available = usbcam_dev->read_len - usbcam_dev->in_transfor;
		if (!available)
			usbcam_dev->read_status = READ_STATUS_EMPTY;
	}

	if (usbcam_dev->read_status == READ_STATUS_EMPTY ||
		usbcam_dev->read_status == READ_STATUS_WAIT_SUBMIT) {
		if (usbcam_dev->read_status == READ_STATUS_EMPTY) {
			usbcam_dev->read_status = READ_STATUS_WAIT_SUBMIT;
			spin_unlock_irqrestore(&usbcam_dev->read_lock, flag);

			ret = aml_read_usb_submit(usbcam_dev);
			if (ret) {
				usbcam_err("submit %p failed, ret = %d", usbcam_dev->read_urb, ret);
				return ret;
			}
		}
		//else {
		//spin_unlock_irqrestore(&usbcam_dev->read_lock, flag);
		//usbcam_err("urb already submitted,unable to repeat submit\n");
		//return -EAGAIN;
		//}

		if (filp->f_flags & O_NONBLOCK) {
			usbcam_dbg("return EAGAIN\n");
			return -EAGAIN;
		}
		ret = wait_event_interruptible(usbcam_dev->read_wq,
				(usbcam_dev->read_status == READ_STATUS_READY) ||
				(usbcam_dev->read_status == READ_STATUS_ERROR) ||
				(usbcam_dev->read_open_status == READ_CLOSE));
		if (ret < 0) {
			usbcam_err("wait_event_interruptible failed, ret = %d\n", ret);
			return ret;
		}
	} else {
		spin_unlock_irqrestore(&usbcam_dev->read_lock, flag);
	}

	if (usbcam_dev->read_open_status == READ_CLOSE) {
		usbcam_err(" read_open_status %d\n", usbcam_dev->read_open_status);
		return 0;
	}

	if (usbcam_dev->read_status == READ_STATUS_ERROR) {
		usbcam_err(" read error, return = %d\n", usbcam_dev->read_err);
		usbcam_dev->read_status = READ_STATUS_EMPTY;
		return usbcam_dev->read_err;
	}

	spin_lock_irqsave(&usbcam_dev->read_lock, flag);
	available = usbcam_dev->read_len - usbcam_dev->in_transfor;
	transfor = usbcam_dev->in_transfor;
	chunk = min(size, available);
	usbcam_dev->in_transfor += chunk;
	spin_unlock_irqrestore(&usbcam_dev->read_lock, flag);

	ret = copy_to_user(buff, usbcam_dev->read_buf + transfor, chunk);
	if (ret) {
		usbcam_err("data to user error\n");
		return ret;
	}

	spin_lock_irqsave(&usbcam_dev->read_lock, flag);
	if (!(usbcam_dev->read_len - usbcam_dev->in_transfor))
		usbcam_dev->read_status = READ_STATUS_EMPTY;

	spin_unlock_irqrestore(&usbcam_dev->read_lock, flag);

	return chunk;
}

static void aml_intf_write_empty_complete(struct urb *urb)
{
	int status;
	struct aml_usbcam *usbcam_dev = NULL;
	ulong flag;

	if (!urb) {
		usbcam_err("invalid parameter\n");
		return;
	}

	status = urb->status;
	usbcam_dev = urb->context;

	spin_lock_irqsave(&usbcam_dev->write_lock, flag);
	if (status) {
		usbcam_dev->write_len = 0;
		usbcam_dev->write_err = status;
		usbcam_dev->write_status = WRITE_STATUS_ERROR;
		usbcam_err("urb %p write urb error, status %d\n", urb, status);
	} else {
		usbcam_dev->write_len = urb->actual_length;
		usbcam_dev->write_err = 0;
		usbcam_dev->write_status = WRITE_STATUS_READY;
	}
	spin_unlock_irqrestore(&usbcam_dev->write_lock, flag);
	wake_up_interruptible(&usbcam_dev->write_wq);
}

static int aml_write_usb_submit_empty(struct aml_usbcam *usbcam_dev, size_t size)
{
	int pipe;
	int retval = 0;
	ulong flag;

	if (!usbcam_dev->intf) {
		usbcam_err("packet write failed,interface is deregistered\n");
		return -ENODEV;
	}

	spin_lock_irqsave(&usbcam_dev->write_lock, flag);
	usbcam_dev->write_status = WRITE_STATUS_WAIT_EMPTY_DONE;
	spin_unlock_irqrestore(&usbcam_dev->write_lock, flag);

	if (usb_endpoint_is_int_out(usbcam_dev->out)) {
		pipe = usb_sndintpipe(usbcam_dev->usbdev,
				usbcam_dev->out->bEndpointAddress);

		usb_fill_int_urb(usbcam_dev->write_urb, usbcam_dev->usbdev,
			pipe, NULL, 0, aml_intf_write_empty_complete, usbcam_dev,
			usbcam_dev->out->bInterval);
	} else {
		pipe = usb_sndbulkpipe(usbcam_dev->usbdev,
				usbcam_dev->out->bEndpointAddress);

		usb_fill_bulk_urb(usbcam_dev->write_urb, usbcam_dev->usbdev,
			pipe, NULL, 0, aml_intf_write_empty_complete, usbcam_dev);
	}

	retval = usb_submit_urb(usbcam_dev->write_urb, GFP_KERNEL);
	if (retval) {
		usbcam_err("submit zero-length packet urb %p failed, return = %d\n",
						usbcam_dev->write_urb, retval);
		return retval;
	}

	return 0;
}

static int aml_write_submit_empty_thread(void *data)
{
	struct aml_usbcam *usbcam_dev = data;
	size_t ret = 0;

	while (1) {
		ret = wait_event_interruptible(usbcam_dev->write_wq,
			(usbcam_dev->device_status == DEVICE_STATUS_DISCONNECT) ||
			(usbcam_dev->write_status == WRITE_STATUS_EMPTY));
		if (usbcam_dev->device_status == DEVICE_STATUS_DISCONNECT) {
			usbcam_err("device status is disconnect\n");
			break;
		}
		if (usbcam_dev->write_status != WRITE_STATUS_WAIT_TS_DONE &&
			usbcam_dev->write_status != WRITE_STATUS_WAIT_EMPTY_DONE) {
			aml_write_usb_submit_empty(usbcam_dev, usbcam_dev->write_size);
		} else {
			usbcam_err("urb already submitted,unable to repeat submit\n");
			return -EAGAIN;
		}
	}
	return 0;
}

static void aml_intf_write_complete(struct urb *urb)
{
	int status;
	struct aml_usbcam *usbcam_dev = NULL;
	ulong flag;

	if (!urb) {
		usbcam_err("invalid parameter\n");
		return;
	}

	status = urb->status;
	usbcam_dev = urb->context;

	spin_lock_irqsave(&usbcam_dev->write_lock, flag);
	if (status) {
		usbcam_dev->write_len = 0;
		usbcam_dev->write_err = status;
		usbcam_dev->write_status = WRITE_STATUS_ERROR;
		usbcam_err("urb %p write urb error, status %d\n", urb, status);
	} else {
		usbcam_dev->write_len = urb->actual_length;
		usbcam_dev->write_err = 0;

		if ((usbcam_dev->write_size % le16_to_cpu(usbcam_dev->out->wMaxPacketSize) == 0))
			usbcam_dev->write_status = WRITE_STATUS_EMPTY;
		else
			usbcam_dev->write_status = WRITE_STATUS_READY;
	}
	spin_unlock_irqrestore(&usbcam_dev->write_lock, flag);
	wake_up_interruptible(&usbcam_dev->write_wq);
}

static int aml_write_usb_submit(struct aml_usbcam *usbcam_dev, size_t size)
{
	int pipe;
	int retval = 0;
	ulong flag;

	spin_lock_irqsave(&usbcam_dev->write_lock, flag);
	usbcam_dev->write_status = WRITE_STATUS_WAIT_TS_DONE;
	usbcam_dev->write_size = size;
	spin_unlock_irqrestore(&usbcam_dev->write_lock, flag);
	if (usb_endpoint_is_int_out(usbcam_dev->out)) {
		pipe = usb_sndintpipe(usbcam_dev->usbdev,
				usbcam_dev->out->bEndpointAddress);

		usb_fill_int_urb(usbcam_dev->write_urb, usbcam_dev->usbdev,
							pipe, usbcam_dev->write_buf, size,
							aml_intf_write_complete, usbcam_dev,
							usbcam_dev->out->bInterval);
	} else {
		pipe = usb_sndbulkpipe(usbcam_dev->usbdev,
				usbcam_dev->out->bEndpointAddress);

		usb_fill_bulk_urb(usbcam_dev->write_urb, usbcam_dev->usbdev,
							pipe, usbcam_dev->write_buf, size,
							aml_intf_write_complete, usbcam_dev);
	}

	retval = usb_submit_urb(usbcam_dev->write_urb, GFP_KERNEL);
	if (retval) {
		usbcam_err("submit urb %p failed, return = %d\n",
						usbcam_dev->write_urb, retval);
		return retval;
	}

	return 0;
}

static ssize_t aml_intf_write(struct file *filp, const char *buff, size_t size, loff_t *ppos)
{
	int ret = 0;
	struct aml_usbcam *usbcam_dev = NULL;
	size_t writesize;
	int retval = -EINVAL;
	ulong flag;

	if (!filp || !buff || !ppos) {
		usbcam_err("invalid parameter\n");
		return -EINVAL;
	}

	usbcam_dev = filp->private_data;
	if (!usbcam_dev) {
		usbcam_err("private data is null\n");
		return -ENODEV;
	}

	if (!usbcam_dev->intf) {
		usbcam_err("write failed, interface is deregistered\n");
		return -ENODEV;
	}

	if (usbcam_dev->write_status != WRITE_STATUS_READY) {
		if (filp->f_flags & O_NONBLOCK)
			return -EAGAIN;

		ret = wait_event_interruptible(usbcam_dev->write_wq,
				(usbcam_dev->write_status == WRITE_STATUS_READY) ||
				(usbcam_dev->write_status == WRITE_STATUS_ERROR) ||
				(usbcam_dev->write_open_status == WRITE_CLOSE));
		if (ret < 0) {
			usbcam_err("wait_event_interruptible failed, ret = %d\n", ret);
			return ret;
		}
	}

	if (usbcam_dev->write_open_status == WRITE_CLOSE) {
		usbcam_err("write failed, interface is close\n");
		return 0;
	}

	spin_lock_irqsave(&usbcam_dev->write_lock, flag);
	if (usbcam_dev->write_status == WRITE_STATUS_ERROR) {
		usbcam_dev->write_status = WRITE_STATUS_READY;
		spin_unlock_irqrestore(&usbcam_dev->write_lock, flag);
		return usbcam_dev->write_err;
	}
	spin_unlock_irqrestore(&usbcam_dev->write_lock, flag);

	size = min(size, usbcam_dev->buf_size);
	if (copy_from_user(usbcam_dev->write_buf, buff, size)) {
		usbcam_err("copy_from_user error\n");
		retval = -EFAULT;
	}

	writesize = size;

	if (usbcam_dev->write_status != WRITE_STATUS_WAIT_TS_DONE &&
		usbcam_dev->write_status != WRITE_STATUS_WAIT_EMPTY_DONE) {
		ret = aml_write_usb_submit(usbcam_dev, size);//bulk transfer
		if (ret) {
			usbcam_err("submit urb %p failed, return = %d", usbcam_dev->read_urb, ret);
			return ret;
		}
	} else {
		usbcam_err("urb already submitted, unable to repeat submit\n");
		return -EAGAIN;
	}

	return writesize;
}

static void aml_usbcam_delete(struct kref *ref)
{
	struct aml_usbcam *usbcam_dev = NULL;

	if (!ref) {
		usbcam_err("invalid parameter");
		return;
	}

	usbcam_dev = container_of(ref, struct aml_usbcam, ref_count);

	usb_put_dev(usbcam_dev->usbdev);

	usb_kill_urb(usbcam_dev->read_urb);
	usb_kill_urb(usbcam_dev->write_urb);

	kfree(usbcam_dev->read_buf);
	kfree(usbcam_dev->write_buf);

	usb_free_urb(usbcam_dev->read_urb);
	usb_free_urb(usbcam_dev->write_urb);

	usbcam_dev->device_status = DEVICE_STATUS_DISCONNECT;
	wake_up_interruptible(&usbcam_dev->write_wq);
	wake_up_interruptible(&usbcam_dev->read_wq);

	kthread_stop(usbcam_dev->thread);
	kfree(usbcam_dev);
	usbcam_dev = NULL;
}

static int aml_intf_release(struct inode *inode, struct file *filp)
{
	struct aml_usbcam *usbcam_dev = NULL;

	if (!inode || !filp) {
		usbcam_err("invalid parameter\n");
		return -EINVAL;
	}

	usbcam_dev = filp->private_data;
	if (!usbcam_dev) {
		usbcam_err("private data is null\n");
		return -ENODEV;
	}

	mutex_lock(&usbcam_dev->open_mux);

	if ((filp->f_flags & 0x03) == O_RDONLY || (filp->f_flags & 0x03) == O_RDWR) {
		usbcam_dev->read_open_status = READ_CLOSE;
		if (atomic_read(&usbcam_dev->read_urb->use_count))
			usb_unlink_urb(usbcam_dev->read_urb);

		if (usbcam_dev->read_open_ref >= 1) {
			usbcam_dev->read_open_ref--;
			usbcam_dbg("interface read fd release successfully\n");
		} else {
			usbcam_err("interface read fd is released before\n");
		}
	}

	if ((filp->f_flags & 0x03) == O_RDWR || (filp->f_flags & 0x03) == O_WRONLY) {
		usbcam_dev->write_open_status = WRITE_CLOSE;
		if (atomic_read(&usbcam_dev->write_urb->use_count))
			usb_unlink_urb(usbcam_dev->write_urb);

		if (usbcam_dev->write_open_ref >= 1) {
			usbcam_dev->write_open_ref--;
			usbcam_dbg("interface write fd release successfully\n");
		} else {
			usbcam_err("interface write fd is released before\n");
		}
	}

	mutex_unlock(&usbcam_dev->open_mux);

	wake_up_interruptible(&usbcam_dev->read_wq);
	wake_up_interruptible(&usbcam_dev->write_wq);

	usbcam_dev->write_err = 0;
	usbcam_dev->read_err = 0;
	kref_put(&usbcam_dev->ref_count, aml_usbcam_delete);

	return 0;
}

static long aml_intf_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	struct aml_usbcam *usbcam_dev = NULL;

	usbcam_dev = filp->private_data;
	if (!usbcam_dev) {
		usbcam_err("no device connected\r\n");
		ret = -ENODEV;
		return ret;
	}

	switch (cmd) {
	case AML_USBCAM_IOC_GET_DRIVER_VERSION:
		usbcam_dbg("ioctl op: get usb cam driver version\n");
		ret = aml_get_usbcam_version(usbcam_dev, arg);
		break;
	case AML_USBCAM_IOC_GET_INFO:
		usbcam_dbg("ioctl op: get usb cam info\n");
		ret = aml_get_usbcam_info(usbcam_dev, arg);
		break;
	case AML_USBCAM_IOC_RESET:
		usbcam_dbg("ioctl op: reset usb cam\n");
		ret = usb_lock_device_for_reset(usbcam_dev->usbdev, NULL);
		usb_reset_device(usbcam_dev->usbdev);
		usb_unlock_device(usbcam_dev->usbdev);
		break;
	case AML_USBCAM_IOC_CANCEL_TRANSFER:
		usbcam_dbg("ioctl op: cancel transfer\n");
		ret = aml_cancel_transfer(filp, usbcam_dev);
		break;
	case AML_USBCAM_IOC_MODULE_CAPABILITIES:
		usbcam_dbg("ioctl op: get usbcam module capabilities\n");
		ret = aml_get_usbcam_capabilities(usbcam_dev, arg);
		break;
	default:
		usbcam_err("unknown op type %08x", cmd);
		ret = -1;
		break;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
static long aml_intf_compat_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	unsigned long translated_arg;

	translated_arg = (unsigned long)compat_ptr(arg);
	ret = aml_intf_ioctl(filp, cmd, translated_arg);

	return ret;
}
#endif

static __poll_t aml_intf_poll(struct file *file, struct poll_table_struct *wait)
{
	struct aml_usbcam *usbcam_dev;
	__poll_t mask = 0;
	size_t ret = 0;
	ulong flag;

	usbcam_dev = file->private_data;

	spin_lock_irqsave(&usbcam_dev->read_lock, flag);
	if (usbcam_dev->read_status == READ_STATUS_READY) {
		if (!(usbcam_dev->read_len - usbcam_dev->in_transfor))
			usbcam_dev->read_status = READ_STATUS_EMPTY;
	}

	if (usbcam_dev->read_status == READ_STATUS_EMPTY) {
		usbcam_dev->read_status = READ_STATUS_WAIT_SUBMIT;
		spin_unlock_irqrestore(&usbcam_dev->read_lock, flag);
		ret = aml_read_usb_submit(usbcam_dev);
	} else {
		spin_unlock_irqrestore(&usbcam_dev->read_lock, flag);
	}

	poll_wait(file, &usbcam_dev->read_wq, wait);
	poll_wait(file, &usbcam_dev->write_wq, wait);

	if (usbcam_dev->write_status == WRITE_STATUS_READY)
		mask |= EPOLLOUT | EPOLLWRNORM;

	if (usbcam_dev->read_status == READ_STATUS_READY)
		mask |= EPOLLIN | EPOLLRDNORM;

	if (usbcam_dev->write_status == WRITE_STATUS_ERROR ||
			usbcam_dev->read_status == READ_STATUS_ERROR)
		mask |= EPOLLERR;

	usbcam_dbg("poll mask %x\n", mask);

	return mask;
}

static const struct file_operations aml_intf_fops = {
	.owner =		THIS_MODULE,
	.open =			aml_intf_open,
	.release =		aml_intf_release,
	.read =			aml_intf_read,
	.write =		aml_intf_write,
	.poll =			aml_intf_poll,
	//.mmap =		aml_intf_mmap,
	.unlocked_ioctl =	aml_intf_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl =		aml_intf_compat_ioctl,
#endif
};

static struct usb_class_driver aml_usbcam_command_class = {
	.name = "cimodule_command%d",
	.fops = &aml_intf_fops,
	.minor_base = USBCAM_COMMAND_MINOR_BASE,
};

static struct usb_class_driver aml_usbcam_media_class = {
	.name = "cimodule_media%d",
	.fops = &aml_intf_fops,
	.minor_base = USBCAM_MEDIA_MINOR_BASE,
};

static ssize_t usb_show(struct class *class,
	struct class_attribute *attr, char *buf)
{
	int ret;

	ret = sprintf(buf, "usb%d\n", 1);
	return ret;
}
static CLASS_ATTR_RO(usb);

static struct attribute *aml_usbcam_attrs[] = {
	&class_attr_usb.attr,
	NULL
};

ATTRIBUTE_GROUPS(aml_usbcam);

struct class clp;

static int aml_usbcam_register_class(void)
{
	int ret;

	clp.name = kzalloc(CLASS_NAME_LEN, GFP_KERNEL);
	if (!clp.name)
		return -ENOMEM;

	snprintf((char *)clp.name, CLASS_NAME_LEN, "amlusbcam-%d", 0);
	clp.owner = THIS_MODULE;
	clp.class_groups = aml_usbcam_groups;
	ret = class_register(&clp);
	if (ret)
		kfree(clp.name);

	return 0;
}

static int aml_usbcam_unregister_class(void)
{
	class_unregister(&clp);
	kzfree(clp.name);
	return 0;
}

static int aml_usbcam_probe(struct usb_interface *intf, const struct usb_device_id *id)
{
	int ret = -ENOMEM;

	struct usb_device *pdev = NULL;
	struct usb_interface_assoc_descriptor *assoc_desc = NULL;

	static struct aml_usbcam *usbcam_dev;
	unsigned char intf_num;

	unsigned char *extratemp = NULL;
	int extrasize = 0;

	unsigned char descriptorlen = 0;
	unsigned char descriptortype = 0;

	int len, temp;

	if (!intf || !id) {
		usbcam_err("invalid parameter");
		ret = -EINVAL;
		goto error;
	}

	pdev = interface_to_usbdev(intf);
	intf_num = intf->cur_altsetting->desc.bInterfaceNumber;

	assoc_desc = pdev->actconfig->intf_assoc[0];
	usbcam_dbg("assoc_desc = %p\n", assoc_desc);
	if (!assoc_desc) {
		usbcam_crit("interface association descriptor is null");
		ret = -ENODEV;
		goto error;
	}

	//usb association descriptor attribute determination
	if (!(assoc_desc->bFunctionClass == IAD_FUNCTION_CLASS &&
		assoc_desc->bFunctionSubClass == IAD_FUNCTION_SUBCLASS &&
		assoc_desc->bFunctionProtocol == IAD_FUNCTION_PROTOCOL) &&
		!(assoc_desc->bFunctionClass == CI20_IAD_FUNCTION_CLASS &&
		assoc_desc->bFunctionSubClass == CI20_IAD_FUNCTION_SUBCLASS &&
		assoc_desc->bFunctionProtocol == CI20_IAD_FUNCTION_PROTOCOL)) {
		usbcam_crit("mismatched function info in IAD");
		ret = -ENODEV;
		goto error;
	}

	if (intf_num < assoc_desc->bFirstInterface ||
		intf_num > assoc_desc->bFirstInterface + assoc_desc->bInterfaceCount) {
		usbcam_err("mismatched interface number in IAD");
		ret = -ENODEV;
		goto error;
	}

	usbcam_dev = kzalloc(sizeof(*usbcam_dev), GFP_KERNEL);
	if (!usbcam_dev) {
		ret = -ENOMEM;
		goto error;
	}

	if ((intf->cur_altsetting->desc.bInterfaceClass == COMMAND_INTF_CLASS &&
		intf->cur_altsetting->desc.bInterfaceSubClass == COMMAND_INTF_SUBCLASS &&
		intf->cur_altsetting->desc.bInterfaceProtocol == COMMAND_INTF_PROTOCOL) ||
		(intf->cur_altsetting->desc.bInterfaceClass == CI20_COMMAND_INTF_CLASS &&
		intf->cur_altsetting->desc.bInterfaceSubClass == CI20_COMMAND_INTF_SUBCLASS &&
		intf->cur_altsetting->desc.bInterfaceProtocol == CI20_COMMAND_INTF_PROTOCOL)) {
		usbcam_dev->device_type = DEVICE_COMMAND;
		usbcam_dbg(" CI20_COMMAND_INTF !\n");
		ret = usb_register_dev(intf, &aml_usbcam_command_class);
		if (ret) {
			usbcam_crit("get minor failed for command interface\n");
			usb_set_intfdata(intf, NULL);
			ret = -ENODEV;
			goto error;
		}
		usbcam_dev->buf_size = USBCAM_CMD_BUFFER_SIZE;
		usbcam_dev->read_buf = kmalloc(usbcam_dev->buf_size, GFP_KERNEL);
		usbcam_dev->write_buf = kmalloc(usbcam_dev->buf_size, GFP_KERNEL);

		if (intf->cur_altsetting->desc.bInterfaceClass == CI20_COMMAND_INTF_CLASS &&
		intf->cur_altsetting->desc.bInterfaceSubClass == CI20_COMMAND_INTF_SUBCLASS &&
		intf->cur_altsetting->desc.bInterfaceProtocol == CI20_COMMAND_INTF_PROTOCOL) {
			usbcam_dev->usbcam_module_info.is_ci20_detected = 1;
			usbcam_dbg("usbcam_dev->usbcam_module_info.is_ci20_detected = %d\n",
						usbcam_dev->usbcam_module_info.is_ci20_detected);
		}

	} else if ((intf->cur_altsetting->desc.bInterfaceClass == MEDIA_INTF_CLASS &&
		intf->cur_altsetting->desc.bInterfaceSubClass == MEDIA_INTF_SUBCLASS &&
		intf->cur_altsetting->desc.bInterfaceProtocol == MEDIA_INTF_PROTOCOL) ||
		(intf->cur_altsetting->desc.bInterfaceClass == CI20_MEDIA_INTF_CLASS &&
		intf->cur_altsetting->desc.bInterfaceSubClass == CI20_MEDIA_INTF_SUBCLASS &&
		intf->cur_altsetting->desc.bInterfaceProtocol == CI20_MEDIA_INTF_PROTOCOL)) {
		usbcam_dev->device_type = DEVICE_MEDIA;
		usbcam_dbg(" CI20_MEDIA_INTF !\n");
		ret = usb_register_dev(intf, &aml_usbcam_media_class);
		if (ret) {
			usbcam_crit("get minor failed for command interface\n");
			usb_set_intfdata(intf, NULL);
			ret = -ENODEV;
			goto error;
		}
		usbcam_dev->buf_size = USBCAM_MEDIA_BUFFER_SIZE;
		usbcam_dev->read_buf = kmalloc(usbcam_dev->buf_size, GFP_KERNEL);
		usbcam_dev->write_buf = kmalloc(usbcam_dev->buf_size, GFP_KERNEL);

		if (intf->cur_altsetting->desc.bInterfaceClass == CI20_MEDIA_INTF_CLASS &&
		intf->cur_altsetting->desc.bInterfaceSubClass == CI20_MEDIA_INTF_SUBCLASS &&
		intf->cur_altsetting->desc.bInterfaceProtocol == CI20_MEDIA_INTF_PROTOCOL) {
			usbcam_dev->usbcam_module_info.is_ci20_detected = 1;
			usbcam_dbg("usbcam_dev->usbcam_module_info.is_ci20_detected = %d\n",
						usbcam_dev->usbcam_module_info.is_ci20_detected);
		}
	}

	extratemp = pdev->actconfig->extra;
	extrasize = pdev->actconfig->extralen;
	while (extrasize > 0) {
		descriptorlen = *extratemp;
		descriptortype = *(extratemp + 1);
		if (descriptorlen > extrasize || descriptorlen < 2) {
			usbcam_crit("parse the compatibility descriptor failed");
			goto error;
		}
		if (descriptortype != 0x41 || descriptorlen != 6) {
			usbcam_dbg("extra data contains an %s descriptor of type 0x%02X",
			descriptortype == 0x0B ? "interface association" : "unexpected",
									descriptortype);
			extratemp += descriptorlen;
			extrasize -= descriptorlen;
		} else {
			//0x41: compatibility descriptor type, 6:compatibility descriptor length
			usbcam_dev->usbcam_module_info.ci_compatibility = *(extratemp + 2) << 24 |
				*(extratemp + 3) << 16 | *(extratemp + 4) << 8 | *(extratemp + 5);

			if ((usbcam_dev->usbcam_module_info.ci_compatibility & 0x07) != 2) {
				// 2:architecture version 2
				usbcam_crit("this driver no support ARCH value:%d",
				usbcam_dev->usbcam_module_info.ci_compatibility & 0x07);
				goto error;
			}
			break;
		}
	}

	usbcam_dev->usbdev = usb_get_dev(interface_to_usbdev(intf));
	usbcam_dev->intf = intf;

	if (usb_endpoint_dir_out(&intf->cur_altsetting->endpoint[0].desc)) {
		usbcam_dbg(" out endpoint is  endpoint[0]!\n");
		usbcam_dev->out = &intf->cur_altsetting->endpoint[0].desc;
	} else if (usb_endpoint_dir_out(&intf->cur_altsetting->endpoint[1].desc)) {
		usbcam_dbg(" out endpoint is  endpoint[1]!\n");
		usbcam_dev->out = &intf->cur_altsetting->endpoint[1].desc;
	} else {
		usbcam_err("out endpoint not found\n");
	}

	if (usb_endpoint_dir_in(&intf->cur_altsetting->endpoint[0].desc)) {
		usbcam_dbg(" in endpoint is  endpoint[0]!\n");
		usbcam_dev->in = &intf->cur_altsetting->endpoint[0].desc;
	} else if (usb_endpoint_dir_in(&intf->cur_altsetting->endpoint[1].desc)) {
		usbcam_dbg(" in endpoint is  endpoint[1]!\n");
		usbcam_dev->in = &intf->cur_altsetting->endpoint[1].desc;
	} else {
		usbcam_err("in endpoint not found\n");
	}

	//usbcam_dbg("endpoint_type(usbcam_dev->in) = %d\n",usb_endpoint_type(usbcam_dev->in));
	//usbcam_dbg("endpoint_type(usbcam_dev->out) = %d\n",usb_endpoint_type(usbcam_dev->out));

	//usbcam_dbg("bulk_out(usbcam_dev->out) = %d\n",usb_endpoint_is_bulk_out(usbcam_dev->out));
	//usbcam_dbg("int_out(usbcam_dev->out) = %d\n",usb_endpoint_is_int_out(usbcam_dev->out));

	//usbcam_dbg("is_bulk_in(usbcam_dev->in) = %d\n",usb_endpoint_is_bulk_in(usbcam_dev->in));
	//usbcam_dbg("is_int_in(usbcam_dev->in) = %d\n",usb_endpoint_is_int_in(usbcam_dev->in));

	if ((!usb_endpoint_is_bulk_out(usbcam_dev->out) &&
		!usb_endpoint_is_int_out(usbcam_dev->out)) ||
		(!usb_endpoint_is_bulk_in(usbcam_dev->in) &&
		!usb_endpoint_is_int_in(usbcam_dev->in))) {
		usbcam_crit("mismatched endpoint type in interface\n");
		goto error;
	}

	usbcam_dev->read_open_ref = 0;
	usbcam_dev->write_open_ref = 0;
	mutex_init(&usbcam_dev->open_mux);

	usbcam_dev->usbcam_module_info.vendor_id = le16_to_cpu(pdev->descriptor.idVendor);
	usbcam_dev->usbcam_module_info.product_id = le16_to_cpu(pdev->descriptor.idProduct);

	if (pdev->manufacturer) {
		usbcam_dbg("pdev->manufacturer = %s\n", pdev->manufacturer);
		temp = strlen(pdev->manufacturer);
		len = temp < USBCAM_MAX_NAME_LEN ? temp : USBCAM_MAX_NAME_LEN;
		strncpy(usbcam_dev->usbcam_module_capabilities.ci_manufacturer_name,
							pdev->manufacturer, len);
		usbcam_dev->usbcam_module_capabilities.ci_manufacturer_name[len - 1] = '\0';
	} else {
		usbcam_crit("pdev->manufacturer inexistence");
		ret = -ENOMEM;
		goto error;
	}

	if (pdev->product) {
		usbcam_dbg("pdev->product = %s\n", pdev->product);
		temp = strlen(pdev->product);
		len = temp < USBCAM_MAX_NAME_LEN ? temp : USBCAM_MAX_NAME_LEN;
		strncpy(usbcam_dev->usbcam_module_capabilities.ci_product_name,
								pdev->product, len);
		usbcam_dev->usbcam_module_capabilities.ci_product_name[len - 1] = '\0';
	} else {
		usbcam_crit("pdev->product inexistence");
		ret = -ENOMEM;
		goto error;
	}

	usbcam_dev->usbcam_module_capabilities.ci_plus_supported = 1;
	usbcam_dev->usbcam_module_capabilities.op_profile_supported = 1;

	kref_init(&usbcam_dev->ref_count);
	mutex_init(&usbcam_dev->read_mux);
	mutex_init(&usbcam_dev->write_mux);

	init_waitqueue_head(&usbcam_dev->read_wq);
	init_waitqueue_head(&usbcam_dev->write_wq);

	spin_lock_init(&usbcam_dev->read_lock);
	spin_lock_init(&usbcam_dev->write_lock);

	usbcam_dev->thread = kthread_run(aml_write_submit_empty_thread, usbcam_dev, "%s",
							"aml_write_submit_empty_thread");

	usbcam_dev->read_urb = usb_alloc_urb(0, GFP_KERNEL);
	if (!usbcam_dev->read_urb) {
		usbcam_crit("read urb alloc failed\n");
		ret = -ENOMEM;
		goto error;
	}

	usbcam_dev->write_urb = usb_alloc_urb(0, GFP_KERNEL);
	if (!usbcam_dev->write_urb) {
		usbcam_crit("write urb alloc failed\n");
		ret = -ENOMEM;
		goto error;
	}

	usbcam_dev->read_status = READ_STATUS_EMPTY;
	usbcam_dev->device_status = DEVICE_STATUS_MATCH;

	usb_set_intfdata(intf, usbcam_dev);

	usbcam_dev->intf_reg = TRUE;

	return 0;

error:
	if (usbcam_dev && usbcam_dev->intf_reg) {
		usb_set_intfdata(usbcam_dev->intf, NULL);
		kthread_stop(usbcam_dev->thread);
		usb_deregister_dev(usbcam_dev->intf, &aml_usbcam_command_class);
		usb_deregister_dev(usbcam_dev->intf, &aml_usbcam_media_class);
		usbcam_dev->intf = NULL;
	}

	if (usbcam_dev) {
		kref_put(&usbcam_dev->ref_count, aml_usbcam_delete);
		usbcam_dev = NULL;
	}

	return ret;
}

static void aml_usbcam_disconnect(struct usb_interface *intf)
{
	struct aml_usbcam *usbcam_dev = NULL;

	usbcam_dev = usb_get_intfdata(intf);

	usb_deregister_dev(usbcam_dev->intf, &aml_usbcam_command_class);
	usb_deregister_dev(usbcam_dev->intf, &aml_usbcam_media_class);

	usb_set_intfdata(usbcam_dev->intf, NULL);
	usbcam_dev->intf = NULL;

	kref_put(&usbcam_dev->ref_count, aml_usbcam_delete);
}

static int aml_usbcam_pre_reset(struct usb_interface *intf)
{
	return 0;
}

static int aml_usbcam_post_reset(struct usb_interface *intf)
{
	return 0;
}

static struct usb_driver aml_usbcam_driver = {
	.name = "aml_usbcam",
	.id_table = aml_usbcam_table,
	.probe = aml_usbcam_probe,
	.disconnect = aml_usbcam_disconnect,
	.pre_reset = aml_usbcam_pre_reset,
	.post_reset = aml_usbcam_post_reset,
};

static int __init aml_usbcam_init(void)
{
	usbcam_dbg("usbcam init!\n");

	aml_usbcam_register_class();
	usbcam_dbg("usbcam register class!\n");

	return usb_register(&aml_usbcam_driver);
}

static void __exit aml_usbcam_exit(void)
{
	usbcam_dbg("usbcam exit!\n");

	aml_usbcam_unregister_class();

	usb_deregister(&aml_usbcam_driver);
}

module_init(aml_usbcam_init);
module_exit(aml_usbcam_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("USB CAM MODULE DRIVER");

