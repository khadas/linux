#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/major.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#include <linux/of_reserved_mem.h>

#include <linux/dma-buf.h>
#include <linux/scatterlist.h>
#include <linux/delay.h>
#include <linux/wait.h>

#include <linux/amlogic/media/canvas/canvas.h>

#include "vfm_grabber.h"


// Module version
#define VERSION_MAJOR 0
#define VERSION_MINOR 0

// Names / devices and general config

#define DRIVER_NAME     "vfm_grabber"
#define MODULE_NAME     DRIVER_NAME
#define DEVICE_NAME     DRIVER_NAME
#define RECEIVER_NAME   DRIVER_NAME
#define DEBUG_LOGFILE 	"/storage/vfm_grabber.log"

// Variables
DECLARE_WAIT_QUEUE_HEAD(waitQueue);

static vfm_grabber_dev grabber_dev;

//////////////////////////////////////////////////
// Log functions
//////////////////////////////////////////////////
static struct file* logfile = NULL;	// Current logfile pointer
static loff_t log_pos = 0;		// Current logfile position


// this function writes in a user file
void debug_log(char *prefix, char *format, ...)
{
	char logstr[300];
	char fullstr[512];
	va_list args;
	mm_segment_t old_fs;

	if (!logfile)
	{
		logfile = filp_open(DEBUG_LOGFILE, O_RDWR | O_CREAT, 0644);
		if (!logfile) return;
	}

	va_start(args, format);
	vsprintf(logstr, format, args);
	sprintf(fullstr, "%s : %s", prefix, logstr);
	va_end(args);

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	vfs_write(logfile, fullstr, strlen(fullstr), &log_pos);
	set_fs(old_fs);
}

// this function writes in the kernel log
void system_log(int logtype, char *prefix, char *format, ...)
{
	char logstr[300];
	char fullstr[512];
	va_list args;

	va_start(args, format);
	vsprintf(logstr, format, args);
	sprintf(fullstr, "%s : %s", prefix, logstr);
	va_end(args);

	if (logtype == 0)
		pr_info("%s", fullstr);
	else
		pr_err("%s", fullstr);
}

//#define DEBUG
#ifdef DEBUG
#define log_info(fmt, arg...)  	debug_log("info", fmt, ## arg)
#define log_error(fmt, arg...) 	debug_log("error", fmt, ## arg)
#else
#define log_info(fmt, arg...) 	system_log(0, DRIVER_NAME, fmt, ## arg)
#define log_error(fmt, arg...)  system_log(1, DRIVER_NAME, fmt, ## arg)
#endif

static int vfm_grabber_receiver_event_fun(int type, void *data, void *private_data)
{

	vfm_grabber_dev *dev = (vfm_grabber_dev *)private_data;

	//static struct timeval frametime;
	//int elapsedtime;

	//log_info("Got VFM event %d \n", type);

	switch (type)
	{
	case VFRAME_EVENT_PROVIDER_RESET:
	case VFRAME_EVENT_PROVIDER_UNREG:
		//for (i = 0; i < MAX_DMABUF_FD; i++)
		//	release_dmabuf(dev, i);
		//dev->info.frames_ready = 0;
		dev->ready_count = 0;
		break;

	case VFRAME_EVENT_PROVIDER_REG:
		dev->ready_count = 0;
		break;

	case VFRAME_EVENT_PROVIDER_START:
		dev->ready_count = 0;
		//dev->info.frames_decoded = 0;
		//dev->info.frames_ready = 0;
		break;

	case VFRAME_EVENT_PROVIDER_QUREY_STATE:
		return RECEIVER_ACTIVE;
		break;

	case VFRAME_EVENT_PROVIDER_VFRAME_READY:
		//dev->info.frames_ready++;
		//dev->info.frames_decoded++;

		//if (dev->framecount == 0)
		//	do_gettimeofday(&dev->starttime);

		//do_gettimeofday(&frametime);
		//elapsedtime = (frametime.tv_sec * 1000000 + frametime.tv_usec) - (dev->starttime.tv_sec * 1000000 + dev->starttime.tv_usec);

		++dev->ready_count;

		//log_info("Got VFRAME_EVENT_PROVIDER_VFRAME_READY, Framerate = %d / %d\n", dev->framecount, elapsedtime);

		wake_up(&waitQueue);

		break;

	}

	return 0;
}

static const struct vframe_receiver_op_s vfm_grabber_vf_receiver = {
	.event_cb = vfm_grabber_receiver_event_fun
};

//////////////////////////////////////////////////
// File operations functions
//////////////////////////////////////////////////
static long vfm_grabber_ioctl(struct file *file, unsigned int cmd, ulong arg)
{
	int ret = 0;
	//vfm_grabber_frame frame = { 0 };
	//struct vframe_s *vf;
	vfm_grabber_dev *dev = (vfm_grabber_dev *)(&grabber_dev);
	vfm_grabber_dev_private* priv = (vfm_grabber_dev_private*)file->private_data;
	struct canvas_s cs0;
	vfm_grabber_frameinfo info;
	int waitResult;

	ret = -1;

	switch (cmd)
	{
		case VFM_GRABBER_GRAB_FRAME:
		{
			if (dev->ready_count < 1)
			{
				// Timeout after 1 second.
				waitResult = wait_event_timeout(waitQueue, dev->ready_count > 0, 1 * HZ);

				if (waitResult == 0)
				{
					// Timeout occured
					return -ETIME;
				}
			}

			if (priv->vf)
			{
				vf_put(priv->vf, RECEIVER_NAME);
				priv->vf = NULL;
			}

			priv->vf = vf_get(RECEIVER_NAME);
			if (priv->vf)
			{
				--dev->ready_count;

				//log_info("VFM_GRABBER_GET_FRAME ioctl peeked frame of type %d\n", vf->type);

				memset(&info, 0, sizeof(info));

				// TODO: Fill in data to return
				info.index = priv->vf->index;
				info.type = priv->vf->type;
				info.duration = priv->vf->duration;
				info.duration_pulldown = priv->vf->duration_pulldown;
				info.pts = priv->vf->pts;
				info.pts_us64 = priv->vf->pts_us64;
				info.flag = priv->vf->flag;

				info.canvas0Addr = priv->vf->canvas0Addr;
				info.canvas1Addr = priv->vf->canvas1Addr;

				info.bufWidth = priv->vf->bufWidth;
				info.width = priv->vf->width;
				info.height = priv->vf->height;
				info.ratio_control = priv->vf->ratio_control;
				info.bitdepth = priv->vf->bitdepth;

				canvas_read(priv->vf->canvas0Addr & 0xff, &cs0);
				info.canvas0plane0.index = cs0.index;
				info.canvas0plane0.addr = cs0.addr;
				info.canvas0plane0.width = cs0.width;
				info.canvas0plane0.height = cs0.height;

				canvas_read(priv->vf->canvas0Addr >> 8 & 0xff, &cs0);
				info.canvas0plane1.index = cs0.index;
				info.canvas0plane1.addr = cs0.addr;
				info.canvas0plane1.width = cs0.width;
				info.canvas0plane1.height = cs0.height;

				canvas_read(priv->vf->canvas0Addr >> 16 & 0xff, &cs0);
				info.canvas0plane2.index = cs0.index;
				info.canvas0plane2.addr = cs0.addr;
				info.canvas0plane2.width = cs0.width;
				info.canvas0plane2.height = cs0.height;


				return copy_to_user((void*)arg, &info, sizeof(info));
			}
			else
			{
				// This should not happen
				log_info("VFM_GRABBER_GET_FRAME ioctl bad code path.\n");
				return -ENODATA;
			}

			//return 0;
		}
		break;

		case VFM_GRABBER_HINT_INVALIDATE:
		{
			if (priv->vf)
			{
				vf_put(priv->vf, RECEIVER_NAME);
				priv->vf = NULL;
			}

			return 0;
		}
		break;
	}

	return ret;
}

static int vfm_grabber_open(struct inode *inode, struct file *file)
{
	vfm_grabber_dev_private* priv;

	priv = kmalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv)
	{
		return -ENOMEM;
	}

	memset(priv, 0, sizeof(*priv));

	priv->dev = grabber_dev.file_device;

	file->private_data = priv;

	return 0;
}

static int vfm_grabber_release(struct inode *inode, struct file *file)
{
	vfm_grabber_dev_private* priv = file->private_data;

	if (priv->vf)
	{
		vf_put(priv->vf, RECEIVER_NAME);
	}

	kfree(priv);

	return 0;
}


static const struct file_operations vfm_grabber_fops =
{
  .owner = THIS_MODULE,
  .open = vfm_grabber_open,
  //.mmap = vfm_grabber_mmap,
  .release = vfm_grabber_release,
  .unlocked_ioctl = vfm_grabber_ioctl,
};

//////////////////////////////////////////////////
// Probe and remove functions
//////////////////////////////////////////////////
static int vfm_grabber_probe(struct platform_device *pdev)
{
	int ret;
	memset(&grabber_dev, 0, sizeof(grabber_dev));

	ret = register_chrdev(VERSION_MAJOR, DEVICE_NAME, &vfm_grabber_fops);
	if (ret < 0)
	{
		log_error("can't register major for device\n");
		return ret;
	}

	grabber_dev.device_major = ret;

	grabber_dev.device_class = class_create(THIS_MODULE, DEVICE_NAME);
	if (!grabber_dev.device_class)
	{
		log_error("failed to create class\n");
		return -EFAULT;
	}

	grabber_dev.file_device = device_create(grabber_dev.device_class, NULL,
		MKDEV(grabber_dev.device_major, VERSION_MINOR),
		NULL, DEVICE_NAME);
	if (!grabber_dev.file_device)
	{
		log_error("failed to create device %s", DEVICE_NAME);
		return -EFAULT;
	}

	// register to vfm
	vf_receiver_init(&grabber_dev.vfm_vf_receiver, RECEIVER_NAME, &vfm_grabber_vf_receiver, &grabber_dev);
	vf_reg_receiver(&grabber_dev.vfm_vf_receiver);

	log_info("driver probed successfully\n");
	return 0;
}

static int vfm_grabber_remove(struct platform_device *pdev)
{
	// unregister from vfm
	vf_unreg_receiver(&grabber_dev.vfm_vf_receiver);
	//vf_receiver_free(&grabber_dev.vfm_vf_receiver);

	device_destroy(grabber_dev.device_class, MKDEV(grabber_dev.device_major, VERSION_MINOR));

	class_destroy(grabber_dev.device_class);

	unregister_chrdev(grabber_dev.device_major, DEVICE_NAME);

	return 0;
}

//////////////////////////////////////////////////
// Module Init / Exit functions
//////////////////////////////////////////////////
static int __init vfm_grabber_init(void)
{
	//if (platform_driver_register(&vfm_grabber_driver))
	//{
	//	log_error("failed to register vfm_grabber module\n");
	//	return -ENODEV;
	//}

	vfm_grabber_probe(NULL);

	log_info("module initialized successfully\n");
	return 0;
}

static void __exit vfm_grabber_exit(void)
{
	//platform_driver_unregister(&vfm_grabber_driver);

	vfm_grabber_remove(NULL);
	log_info("module exited\n");
	return;
}

module_init(vfm_grabber_init);
module_exit(vfm_grabber_exit);

// Module declaration
//////////////////////////////////////////////////
MODULE_DESCRIPTION("VFM Grabber driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Lionel CHAZALLON <LongChair@hotmail.com>, OtherCrashOverride <OtherCrashOverride@users.noreply.github.com>. Nick Xie <nick@khadas.com>");

