/*
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Description:
 * @file    cimax+usb-driver.c
 *
 * @brief   CIMaX+ USB Driver for linux based operating systems.
 *
 * Bruno Tonelli   <bruno.tonelli@smardtv.com>
 * & Franck Descours <franck.descours@smardtv.com>
 * for SmarDTV France, La Ciotat
 */
#define FRBIT
/*#define DEBUG*/
/*#define DEBUG_BITRATE*/
/*#define DEBUG_ISOC_IN*/
/*#define DEBUG_ISOC_OUT*/
/*#define DEBUG_CONTINUITY*/

/******************************************************************************
 * Include
 ******************************************************************************/
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/usb.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/list.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/dvb/ca.h>
#include <linux/compat.h>

#include "cimax+usb-driver.h"
#include "cimax+usb_fw.h"
#include "cimax+usb_config.h"
#ifdef TIMESTAMP
#include "cimax+usb_time.h"
#endif
#include "../../aml_cimax_usb_priv.h"

/******************************************************************************
 * Defines
 *****************************************************************************/
#define DRIVER_VERSION "v1.1.2"
#define DRIVER_AUTHOR "Bruno Tonelli, tonelli@smardtv.com"
#define DRIVER_DESC "CIMaX+ USB Driver for Linux (c)2009-2011"

#define DRIVER_MAX_NUMBER   1

MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_LICENSE("GPL");

/******************************************************************************
 * Structures
 ******************************************************************************/
/******************************************************************************
 * Globals
 ******************************************************************************/
#ifdef FRBIT
int CimaxCfg = 1;
module_param_named(CimaxCfg, CimaxCfg, int, 0644);
MODULE_PARM_DESC(CimaxCfg, "Turn on/off configuration of CIMaX+ (default: on)");
int CimaxDwnl = 1;
module_param_named(CimaxDwnl, CimaxDwnl, int, 0644);
MODULE_PARM_DESC(CimaxDwnl, "Enable upload of FW in CIMaX+ chip (default: on)");
#endif

static struct device_s *gdevice;
static unsigned int gdeviceNumber;

static struct usb_driver device_driver;
static struct timespec gStart;

static __u8 nullHeader[] = {
	0x47, 0x1F, 0xFF, 0x1F, 0xFA, 0xDE, 0xBA, 0xBE
};

static struct bulk_timer_s gbulk_timer[DEVICE_NUM_CAM];
int (*cimax_usb_dev_add)(struct device_s *device, int id);
int (*cimax_usb_dev_remove)(struct device_s *device, int id);

#ifdef TIMESTAMP
static int bSetTimestamps;
#endif

/******************************************************************************
 * Functions
 ******************************************************************************/
#ifdef DEBUG_CONTINUITY
#define TS_MAXPIDS 8192                  /* max value of a PID */
unsigned char    tab_cc[TS_MAXPIDS];

__u16 get_ts_pid(unsigned char *pid)
{
	__u16 pp = 0;

	pp = (pid[0] & 0X1f)<<8;
	pp |= pid[1];

	return pp;
}

static void init_tab_cc(void)
{
	memset(tab_cc, 0xff, TS_MAXPIDS);
}

static int dbg_cc(unsigned char *buf)
{
	int pid;
	unsigned char cc;

	if (buf[0] != DEVICE_MPEG2_SYNC_BYTE) {
		err("Out Of Sync: ");
		return -1;
	}

	pid = get_ts_pid(buf + 1);

	if (!(buf[3] & 0x10))   /* no payload?*/
		return 0;

	if (buf[1] & 0x80)
		err("Error in TS for PID: %d\n", pid);

	/* Check continuity count*/
	cc = tab_cc[pid];
	if (cc == 255)
		cc = (buf[3] & 15);
	else {
		cc = ((cc) + 1) & 15;
		if (cc != (buf[3] & 15)) {
			/* Otherwise, this is a real corruption */
			err("pid %d cc %d expected cc %d actual\n",
					pid, cc, buf[3] & 15);
			cc = (buf[3] & 15);
		}
	}
	return 0;
}
#endif

/*-------------------------------------------------------------------*/
#ifdef DEBUG
static void dbg_dump(char *hdr, unsigned char *data, int size)
{
	int i;
	char line[40];
	char str[9];
	line[0] = 0;
	for (i = 0; i < size; i++) {
		sprintf(line, "%s%.2x ", line, data[i]);
		if ((data[i] >= 32) && (data[i] <= 126))
			str[i%8] = data[i];
		else
			str[i%8] = '.';
		if (!((i+1)%8)) {
			str[i%8 + 1] = 0;
			dbg_s("%s %s %s", hdr, line, str);
			line[0] = 0;
		} /* if */
	} /* for */
	if (i%8) {
		int j;
		str[i%8 + 1] = 0;
		for (j = (i%8); j < 8; j++)
			sprintf(line, "%s   ", line);
		dbg_s("%s %s %s", hdr, line, str);
		line[0] = 0;
	} /* if */
} /* dbg_dump */
#else
#define dbg_dump(format, arg...) do {} while (0)
#endif /* DEBUG */

static unsigned long copyDataFrom(int us,
		void *to, const void *from, unsigned long n)
{
	if (us)
		return copy_from_user(to, from, n);
	memcpy(to, from, n);
	return 0;
}

static unsigned long copyDataTo(int us,
		void *to, const void *from, unsigned long n)
{
	if (us)
		return copy_to_user(to, from, n);
	memcpy(to, from, n);
	return 0;
}

static void vb_init(struct video_buf_s *buf)
{
	buf->readOffset = 0;
	buf->writeOffset = 0;
	buf->isEmpty = 1;
} /* vb_init */

static int vb_get_write_size(struct video_buf_s *buf)
{
	int writeSize = 0;

	if (buf->writeOffset == buf->readOffset) {
		if (buf->isEmpty)
			writeSize = DEVICE_VB_LENGTH;
	} else if (buf->writeOffset > buf->readOffset)
		writeSize =
			DEVICE_VB_LENGTH - (buf->writeOffset - buf->readOffset);
	else
		writeSize = buf->readOffset - buf->writeOffset;
	return writeSize;
} /* vb_get_write_size */

static int vb_write(struct video_buf_s *buf, __u8 *data, int size)
{
	int writeSize = vb_get_write_size(buf);
	int firstPart = DEVICE_VB_LENGTH - buf->writeOffset;
	if (size > writeSize)
		size = writeSize;

	if (size < firstPart) {
		memcpy(&buf->data[buf->writeOffset], data, size);
		buf->writeOffset += size;
	} /* if */ else {
		memcpy(&buf->data[buf->writeOffset], data, firstPart);
		memcpy(buf->data, &data[firstPart], size - firstPart);
		buf->writeOffset = size - firstPart;
	} /* else */

	if (size > 0)
		buf->isEmpty = 0;
	return size;
} /* vb_write */

static int vb_read_next(struct video_buf_s *buf, __u8 *data)
{
	int readSize;
	int firstPart;
	int nextOffset;
	int isStuffing;
	int ret;

	readSize = DEVICE_VB_LENGTH - vb_get_write_size(buf);
	nextOffset = buf->readOffset + DEVICE_MPEG2_PACKET_SIZE;
	if (nextOffset >= DEVICE_VB_LENGTH)
		nextOffset -= DEVICE_VB_LENGTH;
	while (readSize > DEVICE_MPEG2_PACKET_SIZE) {
		if ((buf->data[buf->readOffset] == DEVICE_MPEG2_SYNC_BYTE) &&
			(buf->data[nextOffset] == DEVICE_MPEG2_SYNC_BYTE)) {
			/* packet in sync */
			break;
		} /* if */
		buf->readOffset++;
		if (buf->readOffset == DEVICE_VB_LENGTH)
			buf->readOffset = 0;
		nextOffset++;
		if (nextOffset == DEVICE_VB_LENGTH)
			nextOffset = 0;
		readSize--;
	} /* while */
	if (readSize <= DEVICE_MPEG2_PACKET_SIZE) {
		buf->isEmpty = 1;
		return 0;
	} /* if */

	/* packet is in sync, check if it is a stuffing packet */
	isStuffing = 0;
	firstPart = DEVICE_VB_LENGTH - buf->readOffset;
	if (firstPart < DEVICE_NULL_HEADER_SIZE) {
		if ((memcmp(nullHeader, &buf->data[buf->readOffset], firstPart)
				== 0) &&
				(memcmp(&nullHeader[firstPart], buf->data,
					    DEVICE_NULL_HEADER_SIZE - firstPart)
					== 0)) {
			isStuffing = 1;
		} /* if */
	} /* if */
	else {
		if (memcmp(nullHeader, &buf->data[buf->readOffset],
					DEVICE_NULL_HEADER_SIZE) == 0) {
			isStuffing = 1;
		} /* if */
	} /* else */
	readSize -= DEVICE_MPEG2_PACKET_SIZE;
	if (readSize <= DEVICE_MPEG2_PACKET_SIZE)
		buf->isEmpty = 1;

	/* skip stuffing packet */
	if (isStuffing) {
		buf->readOffset = nextOffset;
		return 0;
	} /* if */

	/* copy packet to user space */
	if (firstPart >= DEVICE_MPEG2_PACKET_SIZE) {
		ret = copy_to_user(data,
			&buf->data[buf->readOffset], DEVICE_MPEG2_PACKET_SIZE);
	} /* if */ else {
		ret = copy_to_user(data,
			&buf->data[buf->readOffset], firstPart);
		ret = copy_to_user(&data[firstPart],
			buf->data, DEVICE_MPEG2_PACKET_SIZE - firstPart);
	} /* else */
	buf->readOffset = nextOffset;
	return DEVICE_MPEG2_PACKET_SIZE;
} /* vb_read_next */

/*-------------------------------------------------------------------*/
#ifdef DEBUG_BITRATE
static void print_bitrate(struct ts_channel_s *channel, __u8 channel_number)
{
	int     readSize;
	ktime_t  currentTime;
	int  diffTime_us;
	int bitrate;

	currentTime = ktime_get_real();
	if (!(channel->bitrateTime.tv64)) {
		channel->bitrateTime = currentTime;
	} else {
		readSize = DEVICE_VB_LENGTH - vb_get_write_size(&channel->inVb);
		dbg("%d bytes received\n", readSize);
		diffTime_us = (int)(ktime_us_delta(currentTime,
					channel->bitrateTime));
		if (diffTime_us) {
			bitrate = (int)((readSize * 8 * USEC_PER_SEC)
					/ diffTime_us);
		}
		channel->bitrateTime = currentTime;
		dbg("received bitrate for channel[%d] = %dbps\n",
			channel_number, bitrate);
	}
}
#endif /* DEBUG_BITRATE */
/*-------------------------------------------------------------------*/

static void device_cibulk_complete(struct urb *urb)
{
	dbg("start");
	kfree(urb->transfer_buffer);
	usb_free_urb(urb);
	dbg("end");
} /* device_cibulk_complete */

static int device_cibulk_send(struct device_s *device,
		struct ioctl_data_s *data,
		int user_space)
{
	int         res;
	struct urb *urb;
	int         size;
	int         index = -1;
	__u8        *ptr;
	__u32       todo = data->txSize;
	__u8        *userData = data->txData;

	dbg("start");

	do {
		/* get a free bulk message */
		urb = usb_alloc_urb(0, GFP_KERNEL);
		if (!urb) {
			err("alloc urb");
			return -ENOMEM;
		} /* if */
		urb->dev = device->usbdev;

		/* allocate bulk data */
		size = device->ciBulk.outMaxPacketSize;
		if (todo < size)
			size = todo;
		urb->transfer_buffer = kmalloc(size, GFP_KERNEL);
		if (!urb->transfer_buffer) {
			err("alloc transfer buffer");
			usb_free_urb(urb);
			return -ENOMEM;
		} /* if */

		/* copy data */
		ptr = urb->transfer_buffer;
		res = copyDataFrom(user_space, ptr, userData, size);

#ifdef TIMESTAMP
		if (bSetTimestamps) {
			if (index == -1) {
				SetTimestamp("urb %x, toSend %d, send %d",
					    urb, todo, size);
				SetTimestamp("cmd 0x%02x", ptr[0]);
			} else {
				SetTimestamp("urb %x, toSend %d, send %d",
					    urb, todo, size);
			}
		}
#endif

		/* first packet, get index */
		if (index == -1) {
			if ((ptr[DEVICE_COMMAND_OFFSET] == DEVICE_CMD_INIT) ||
				(ptr[DEVICE_COMMAND_OFFSET]
					== DEVICE_CMD_WRITE_REG) ||
				(ptr[DEVICE_COMMAND_OFFSET]
					== DEVICE_CMD_READ_REG)) {
				index = 0; /* register command, no module */
			} else if (ptr[DEVICE_COMMAND_OFFSET]
					& DEVICE_SEL_MASK) {
				index = 1; /* module B */
			} else {
				index = 0; /* module A */
			} /* else */
			device->ciBulk.ciData[index].syncDataSize = 0;
			device->ciBulk.ciData[index].syncSignal = 0;
		} /* if */

		/* submit bulk */
		urb->pipe = usb_sndbulkpipe(device->usbdev,
				DEVICE_BULK_OUT_PIPE);
		urb->transfer_buffer_length = size;
		urb->complete = device_cibulk_complete;
		urb->context  = NULL;
		dbg_dump("txBuf", urb->transfer_buffer,
				urb->transfer_buffer_length);
		res = usb_submit_urb(urb, GFP_KERNEL);
		if (res < 0) {
			err("submit urb res = %d", res);
			kfree(urb->transfer_buffer);
			usb_free_urb(urb);
			return -ENOMEM;
		} /* if */
		todo -= size;
		userData += size;
	} while (todo);

	device->ciBulk.ciData[index].bPendingSend = 1;
	dbg("end");
	return index;
} /* device_cibulk_send */

static void device_int_complete(struct urb *urb)
{
	unsigned long   flags;
	struct device_s *device = urb->context;
	__u8            *dataToCopy;
	int             sizeToCopy, SizeReceived;
	__u8            isFirstPacket = 0;
	__u8            isLastPacket = 0;
	__u8            index, i;
	__u8            status;
	struct message_node_s *message;

	dbg("start");

	if (urb->status) {
		dbg("urb status %d, not submitted again", urb->status);
		kfree(urb->transfer_buffer);
		usb_free_urb(urb);
		for (i = 0; i < DEVICE_NUM_INT_IN_URBS; i++) {
			if (device->ciBulk.intUrb[i] == urb)
				device->ciBulk.intUrb[i] = NULL;
		}
		return;
	} /* if */

	spin_lock_irqsave(&device->ciBulk.intUrbLock, flags);
	dbg("urb status %d, transfer_buffer_length %d actual_length %d",
			urb->status,
			urb->transfer_buffer_length,
			urb->actual_length);
	dataToCopy = urb->transfer_buffer;
	SizeReceived =  urb->actual_length;
	dbg_dump("total rxBuf", dataToCopy, SizeReceived);


	do {
		if (device->ciBulk.intSizeToReceive == 0) {
			if (!dataToCopy[DEVICE_STATUS_OFFSET] &&
					!dataToCopy[DEVICE_LENGTH_MSB_OFFSET] &&
					!dataToCopy[DEVICE_LENGTH_LSB_OFFSET] &&
					!dataToCopy[DEVICE_COUNTER_OFFSET]){
				dbg("no data receive");
				memset(urb->transfer_buffer,
						0, urb->transfer_buffer_length);
				usb_submit_urb(urb, GFP_ATOMIC);
				return;
			}
			/* first packet, read header */
			isFirstPacket = 1;
			device->ciBulk.intCurrStatus =
				dataToCopy[DEVICE_STATUS_OFFSET] &
				DEVICE_CMD_MASK;
			if (dataToCopy[DEVICE_STATUS_OFFSET]
				& DEVICE_SEL_MASK) {
				device->ciBulk.intCurrIndex = 1; /* module B */
			} else {
				device->ciBulk.intCurrIndex = 0; /* module A */
			}
			if ((device->ciBulk.intCurrStatus == DEVICE_READ_REGOK)
				|| (device->ciBulk.intCurrStatus
					== DEVICE_WRITE_REGOK)) {
				device->ciBulk.intSizeToReceive =
					dataToCopy[DEVICE_LENGTH_LSB_OFFSET] +
					DEVICE_DATA_OFFSET;
			} else {
				device->ciBulk.intSizeToReceive =
				dataToCopy[DEVICE_LENGTH_MSB_OFFSET] * 256
					+ dataToCopy[DEVICE_LENGTH_LSB_OFFSET]
					+ DEVICE_DATA_OFFSET;
			}
		} /* if */

		/* get last packet state */
		status     = device->ciBulk.intCurrStatus;
		index      = device->ciBulk.intCurrIndex;
		sizeToCopy = device->ciBulk.intSizeToReceive;
		if (sizeToCopy > urb->actual_length) {
			/* limit size to received buffer size */
			sizeToCopy = urb->actual_length;
		} /* if */ else
			isLastPacket = 1;
		dbg_dump("rxBuf", dataToCopy, sizeToCopy);

#ifndef FRBIT
		if (status == DEVICE_DATAREADY) {
			if (device->ciBulk.ciData[index].bPendingSend)
				status = DEVICE_DATAREADY_SYNC;
		}
#endif

#ifdef TIMESTAMP
		if (device->ciBulk.intSizeToReceive > 2000)
			bSetTimestamps = 1;
		if (bSetTimestamps) {
			SetTimestamp("urb %x,toReceive %d,received %d,toCopy%d",
				urb,
				device->ciBulk.intSizeToReceive,
				SizeReceived,
				sizeToCopy);
			SetTimestamp("status 0x%02x, camIndex %d, isLast %d",
				status, index, isLastPacket);
		}
#endif

		switch (status) {
		case DEVICE_INITOK:
		case DEVICE_READ_REGOK:
		case DEVICE_WRITE_REGOK:
			index = 0;
		case DEVICE_CAMRESETOK:
			/*only for debug*/
			if (!dataToCopy[DEVICE_STATUS_OFFSET] &&
				!dataToCopy[DEVICE_LENGTH_MSB_OFFSET] &&
				!dataToCopy[DEVICE_LENGTH_LSB_OFFSET] &&
				!dataToCopy[DEVICE_COUNTER_OFFSET]){
				break;
			}
		case DEVICE_CISOK:
		case DEVICE_WRITECOROK:
		case DEVICE_NEGOTIATEOK:
		case DEVICE_WRITELPDUOK:
		case DEVICE_WRITELPDUBUSY:
		case DEVICE_READLPDUOK:
		case DEVICE_WRITEEXTOK:
		case DEVICE_READEXTOK:
		case DEVICE_NO_CAM:
		case DEVICE_NOK:
		case DEVICE_MCARD_WRITEOK:
		case DEVICE_CAMPARSE_ERROR:
		case DEVICE_CMDPENDING:
		case DEVICE_REGSTATUSOK:
		case DEVICE_DATAREADY_SYNC:
			/* copy partial message */
			spin_lock_irqsave(&device->ciBulk.intLock,
				flags);
			memcpy(&device->ciBulk.ciData[index].
				syncData[device->ciBulk.ciData[index].
				syncDataSize],
				dataToCopy, sizeToCopy);
			device->ciBulk.intSizeToReceive -= sizeToCopy;
			device->ciBulk.ciData[index].syncDataSize +=
				sizeToCopy;
			spin_unlock_irqrestore(&device->ciBulk.intLock,
				flags);
			dbg("copied %d bytes at offset %d", sizeToCopy,
				device->ciBulk.ciData[index].
				syncDataSize - sizeToCopy);

			if (isLastPacket) {
				/* last packet received, sync message */
				device->ciBulk.ciData[index].syncSignal = 1;
				wake_up_interruptible(&device->ciBulk.
					ciData[index].syncWait);
				device->ciBulk.ciData[index].bPendingSend = 0;
				dbg("sync signal return %d %d ",
					device->ciBulk.ciData[index].
						syncDataSize, index);
			} /* if */
			break;
		case DEVICE_CAMDET:
		case DEVICE_DATAREADY:
		case DEVICE_MCARD_READ:
		case DEVICE_FRBit:
			if (isFirstPacket) {
				/* create new async message */
				message = kmalloc(sizeof(struct message_node_s),
					GFP_ATOMIC);
				if (!message) {
					err("cannot allocate async message");
					break;
				}
				memset(message,
					0, sizeof(struct message_node_s));
				list_add_tail(&message->node,
					&device->ciBulk.ciData[index].
						asyncDataList);
			} /* if */
			else {
				/* get tail message */
				message = list_entry((device->ciBulk.
					ciData[index].asyncDataList.prev),
					struct message_node_s, node);
			} /* else */

			/* copy partial message */
			spin_lock_irqsave(&device->ciBulk.intLock, flags);
			memcpy(&message->data[message->size],
					dataToCopy, sizeToCopy);
			device->ciBulk.intSizeToReceive -= sizeToCopy;
			message->size += sizeToCopy;
			spin_unlock_irqrestore(&device->ciBulk.intLock, flags);
			dbg("async copied %d bytes at offset %d", sizeToCopy,
				message->size - sizeToCopy);

			if (isLastPacket) {
				/* last packet received, signal async message */
				wake_up_interruptible(&device->ciBulk.
					ciData[index].asyncWait);
				dbg("async signal %d", index);
			} /* if */
			break;
		case DEVICE_GPIOCHANGE:
			info("GPIO change %x %x %x",
				status, dataToCopy[4], dataToCopy[5]);
			device->ciBulk.intSizeToReceive -= sizeToCopy;
			break;
		default:
			err("unknown status 0x%2x", status);
			break;
		} /* switch */
		dataToCopy += sizeToCopy;
		SizeReceived -= sizeToCopy;

	} while (SizeReceived > 0);

	memset(urb->transfer_buffer, 0, urb->transfer_buffer_length);
	usb_submit_urb(urb, GFP_ATOMIC);

#ifdef TIMESTAMP
	/*if (bSetTimestamps) {
		SetTimestamp("urb %x submitted", urb);
	}*/
#endif

	spin_unlock_irqrestore(&device->ciBulk.intUrbLock, flags);

	dbg("end");
} /* device_int_complete */

static int device_wait_sync_data(struct device_s *device,
		__u8          index,
		struct ioctl_data_s *data,
		int user_space)
{
	unsigned long flags;
	int ret;

	dbg("start %d", index);

	spin_lock_irqsave(&device->ciBulk.intLock, flags);
	while (device->ciBulk.ciData[index].syncSignal == 0) {
		/* nothing to copy */
		spin_unlock_irqrestore(&device->ciBulk.intLock, flags);
		if (wait_event_interruptible(device->ciBulk.
				ciData[index].syncWait,
				device->ciBulk.ciData[index].syncSignal)) {
			device->ciBulk.ciData[index].bPendingSend = 0;
			err("interrupt");
			return -ERESTARTSYS;
			/* signal: tell the fs layer to handle it */
		} /* if */
		/* otherwise loop, but first reacquire the lock */
		spin_lock_irqsave(&device->ciBulk.intLock, flags);
	} /* while */

	/* copy packet to user space buffer */
	if (device->ciBulk.ciData[index].syncDataSize < data->rxSize)
		/* truncate returned message against user buffer size */
		data->rxSize = device->ciBulk.ciData[index].syncDataSize;
	spin_unlock_irqrestore(&device->ciBulk.intLock, flags);
	/* release the lock */
	ret = copyDataTo(user_space,
			data->rxData,
			device->ciBulk.ciData[index].syncData, data->rxSize);
	dbg_dump("userMsg",
		device->ciBulk.ciData[index].syncData, data->rxSize);
	dbg("userRet %d", data->rxSize);
	device->ciBulk.ciData[index].syncDataSize = 0;
	device->ciBulk.ciData[index].syncSignal = 0;

	dbg("end");
	return 0;
} /* device_wait_sync_data */

static int device_wait_async_data(struct device_s *device,
		__u8       index,
		struct rw_data_s *data,
		int user_space)
{
	struct list_head *item;
	struct message_node_s *message;
	unsigned long     flags;
	int ret;

	dbg("start %d", index);

	if ((device->askToRelease) || (device->askToSuspend)) {
		err("ask to release or ask to suspend");
		return -EINTR; /* device close interrupt */
	} /* if */

	if (index >= DEVICE_NUM_CAM) {
		err("bad index(%d)", index);
		return -EINVAL;
	}

	spin_lock_irqsave(&device->ciBulk.intLock, flags);
	while (list_empty(&device->ciBulk.ciData[index].asyncDataList)) {
		/* nothing to copy */
		spin_unlock_irqrestore(&device->ciBulk.intLock, flags);
		/* release the lock */
		if (wait_event_interruptible(device->ciBulk.
				ciData[index].asyncWait,
			device->askToRelease ||
			device->askToSuspend ||
			(!list_empty(&device->ciBulk.
				ciData[index].asyncDataList)))) {
			err("interrupt");
			return -ERESTARTSYS;
			/* signal: tell the fs layer to handle it */
		} /* if */
		if ((device->askToRelease) || (device->askToSuspend)) {
			err("ask to release or ask to suspend");
			return -EINTR; /* device close interrupt */
		} /* if */
		/* otherwise loop, but first reacquire the lock */
		spin_lock_irqsave(&device->ciBulk.intLock, flags);
	} /* while */

	/* ok, data is there, return first item */
	item = device->ciBulk.ciData[index].asyncDataList.next;
	message = list_entry(item, struct message_node_s, node);
	if (message->size < data->size) {
		/* truncate returned message against user buffer size */
		data->size = message->size;
	} /* if */
	spin_unlock_irqrestore(&device->ciBulk.intLock, flags);
	/* release the lock */
	ret = copyDataTo(user_space, data->data, message->data, data->size);
	dbg_dump("userMsg", message->data, data->size);
	dbg("userRet %d", data->size);
	list_del(item);
	kfree(message);

	dbg("end");
	return 0;
} /* device_wait_async_data */

static int device_start_intr(struct device_s *device)
{
	__u8        i, j;
	struct urb *urb;

	dbg("start");

	for (i = 0; i < DEVICE_NUM_INT_IN_URBS; i++) {
		urb = usb_alloc_urb(0, GFP_KERNEL);
		if (!urb) {
			err("alloc urb");
			return -ENOMEM;
		} /* if */
		urb->transfer_buffer =
			kmalloc(device->ciBulk.inMaxPacketSize, GFP_KERNEL);
		if (!urb->transfer_buffer) {
			err("alloc transfer buffer");
			usb_free_urb(urb);
			urb = NULL;
			return -ENOMEM;
		} /* if */

		urb->dev  = device->usbdev;
		urb->pipe = usb_rcvintpipe(device->usbdev, DEVICE_INT_IN_PIPE);
		urb->transfer_buffer_length = device->ciBulk.inMaxPacketSize;
		urb->complete = device_int_complete;
		urb->context  = device;
		urb->interval = 1;
		device->ciBulk.intUrb[i] = urb;
		for (j = 0; j < DEVICE_NUM_CAM; j++) {
			init_waitqueue_head(&device->ciBulk.ciData[j].syncWait);
			init_waitqueue_head(&device->ciBulk.
				ciData[j].asyncWait);
		} /* for */
		usb_submit_urb(device->ciBulk.intUrb[i], GFP_KERNEL);
	}

	dbg("end");
	return 0;
} /* device_start_intr */

static void device_stop_intr(struct device_s *device)
{
	struct list_head *item;
	struct list_head *tmp;
	struct message_node_s *message;
	int               i, j;

	dbg("start");

	for (i = 0; i < DEVICE_NUM_INT_IN_URBS; i++) {
		if (!device->ciBulk.intUrb[i])
			break;
		usb_unlink_urb(device->ciBulk.intUrb[i]);
		device->ciBulk.intUrb[i] = NULL;
		for (j = 0; j < DEVICE_NUM_CAM; j++) {
			for (item = device->ciBulk.ciData[j].asyncDataList.next;
				item != &device->ciBulk.ciData[j].asyncDataList;
				) {
				message = list_entry(item,
						struct message_node_s, node);
				tmp = item->next;
				list_del(item);
				kfree(item);
				item = tmp;
			} /* for */
		} /* for */
		dbg("unlink urb");
	}

	dbg("end");
} /* device_stop_intr */

static void device_iso_in_complete(struct urb *urb)
{
	unsigned long flags;
	struct ts_channel_s *channel = urb->context;
	__u8          i;
	__u8          *data;

	/*dbg("start");*/

	if (urb->status) {
		dbg("urb status %d, not submitted again", urb->status);
		kfree(urb->transfer_buffer);
		usb_free_urb(urb);
		return;
	} /* if */

	spin_lock_irqsave(&channel->inLock, flags);
	for (i = 0; i < urb->number_of_packets; i++) {
		data = urb->transfer_buffer + urb->iso_frame_desc[i].offset;
		if (!urb->iso_frame_desc[i].status &&
				(urb->iso_frame_desc[i].actual_length > 0)) {
			if (vb_get_write_size(&channel->inVb)
				>= urb->iso_frame_desc[i].actual_length) {
				vb_write(&channel->inVb, data,
					urb->iso_frame_desc[i].actual_length);
			} /* if */
			else {
				err("video buffer is full, packet loss %d",
					urb->iso_frame_desc[i].actual_length);
			} /* else */
		} /* if */
		else {
			err("frame rejected, status %x, actual_length %d bytes",
				urb->iso_frame_desc[i].status,
				urb->iso_frame_desc[i].actual_length);
		}
	} /* for */
	spin_unlock_irqrestore(&channel->inLock, flags);

	if (!channel->inVb.isEmpty)
		wake_up_interruptible(&channel->inWait);

	memset(urb->transfer_buffer,
		0, DEVICE_ISOC_LENGTH(channel->maxPacketSize));
	urb->transfer_buffer_length =
		DEVICE_ISOC_LENGTH(channel->maxPacketSize);
	urb->number_of_packets = DEVICE_NUM_FRAMES_PER_URB;
	urb->complete = device_iso_in_complete;
	urb->context = channel;
	urb->transfer_flags = URB_ISO_ASAP;
	urb->interval = 1;
	for (i = 0; i < DEVICE_NUM_FRAMES_PER_URB; i++) {
		urb->iso_frame_desc[i].offset = i * channel->maxPacketSize;
		urb->iso_frame_desc[i].length = channel->maxPacketSize;
	} /* for */
	usb_submit_urb(urb, GFP_ATOMIC);

	/*dbg("end");*/
} /* device_iso_in_complete */

static void device_tsbulk_in_complete(struct urb *urb)
{
	unsigned long flags;
	struct ts_channel_s *channel = urb->context;
	__u8          *data;
#ifdef DEBUG_CONTINUITY
	unsigned int i;
#endif

	/*dbg("start");*/

	if (urb->status) {
		err("urb status %d(%x), not submitted again",
			urb->status, urb->status);
		kfree(urb->transfer_buffer);
		usb_free_urb(urb);
		return;
	} /* if */

	spin_lock_irqsave(&channel->inLock, flags);
	data = urb->transfer_buffer;

#ifdef DEBUG_CONTINUITY
	i = 0;
	/* check synchro byte*/
	while (i < urb->actual_length) {
		if (!((data[i] == DEVICE_MPEG2_SYNC_BYTE) &&
			(data[i+DEVICE_MPEG2_PACKET_SIZE]
				== DEVICE_MPEG2_SYNC_BYTE))) {
			i++;
		} else {
			/* Synchro find*/
			break;
		}
	}

	/* Synchro Ok, check discontinuity*/
	while (i < urb->actual_length) {
		if (dbg_cc(&data[i]) < 0) {
			dbg("(actual_length= %d i=%d pkt=%d)",
				urb->actual_length,
				i,
				i/DEVICE_MPEG2_PACKET_SIZE);
			dbg("%02x %02x %02x %02x %02x %02x %02x %02x\n",
				data[i-4], data[i-3], data[i-2], data[i-1],
				data[i], data[i+1], data[i+2], data[i+3]);
		}
		i += DEVICE_MPEG2_PACKET_SIZE;
	}
#endif

	if (urb->actual_length) {
		channel->nbByteRead += urb->actual_length;
		if (vb_get_write_size(&channel->inVb) >= urb->actual_length)
			vb_write(&channel->inVb, data, urb->actual_length);
		else
			err("video buffer is full, packet loss %d",
				urb->actual_length);
	} else {
		/*warn("receive size of 0\n");*/
	}

	spin_unlock_irqrestore(&channel->inLock, flags);
	/*  dbg("urb->actual_length=%d",urb->actual_length);*/
	/*  info("urb->actual_length=%d\n",urb->actual_length);*/

	if (!channel->inVb.isEmpty)
		wake_up_interruptible(&channel->inWait);
	kfree(urb->transfer_buffer);
	usb_free_urb(urb);
	/*dbg("end");*/
} /* device_tsbulk_in_complete */

static int device_fill_ts(struct device_s *device,
		__u8       index,
		struct rw_data_s *data)
{
	unsigned long flags;
	__u32         copiedSize;
	struct ts_channel_s *channel = &device->channel[index];

	/*dbg("start");*/

	spin_lock_irqsave(&channel->inLock, flags);
	do {
		while (channel->inVb.isEmpty) {
			/* nothing to copy */
			spin_unlock_irqrestore(&channel->inLock, flags);
			/* release the lock */
			if (wait_event_interruptible(channel->inWait,
					    device->askToRelease ||
					    device->askToSuspend ||
					    (!channel->inVb.isEmpty))) {
				err("interrupt");
				return -ERESTARTSYS;
				/* signal: tell the fs layer to handle it */
			} /* if */
			if ((device->askToRelease) || (device->askToSuspend)) {
				err("ask to release or ask to suspend");
				return -EINTR; /* device close interrupt */
			} /* if */
			/* otherwise loop, but first reacquire the lock */
			spin_lock_irqsave(&channel->inLock, flags);
		} /* while */

		spin_unlock_irqrestore(&channel->inLock, flags);

		copiedSize = vb_read_next(&channel->inVb,
			&data->data[data->copiedSize]);
		if (copiedSize) {
			/*dbg("copied %d bytes in buffer 0x%p, offset %d",
			  copiedSize, data->data, data->copiedSize);*/
			data->copiedSize += copiedSize;
		} /* if */
		spin_lock_irqsave(&channel->inLock, flags);
	} while ((data->copiedSize+DEVICE_MPEG2_PACKET_SIZE) <= data->size);
	/* buffer not full */

	spin_unlock_irqrestore(&channel->inLock, flags);

#ifdef DEBUG_BITRATE
	print_bitrate(channel, index);
#endif

	/*dbg("end, buffer 0x%p", data->data);*/
	return 0;
} /* device_fill_ts */

static int device_start_iso_in(struct device_s *device, __u8 index)
{
	int         i, j;
	int         ret = 0;
	struct urb *urb;

	/*dbg("start");*/

#ifdef DEBUG_BITRATE
	device->channel[index].bitrateTime = ktime_set(0, 0);
#endif
	for (i = 0; i < DEVICE_NUM_ISOC_IN_URBS; i++) {
		urb = usb_alloc_urb(DEVICE_NUM_FRAMES_PER_URB, GFP_KERNEL);
		device->channel[index].isocInUrb[i] = urb;
		if (urb) {
			/*urb->transfer_buffer =
				kmalloc(DEVICE_ISOC_LENGTH, GFP_KERNEL);*/
			urb->transfer_buffer =
				kmalloc(DEVICE_ISOC_LENGTH(
					device->channel[index].maxPacketSize),
					GFP_KERNEL);
			if (!urb->transfer_buffer) {
				ret = -ENOMEM;
				err("transfer_buffer allocation failed %d", i);
				break;
			} /* if */
		} /* if */ else {
			ret = -ENOMEM;
			err("usb_alloc_urb failed %d", i);
			break;
		} /* if */
	} /* for */

	if (ret) {
		/* Allocation error, must free already allocated data */
		for (i = 0; i < DEVICE_NUM_ISOC_IN_URBS; i++) {
			urb = device->channel[index].isocInUrb[i];
			if (urb) {
				kfree(urb->transfer_buffer);
				if (urb->transfer_buffer)
					urb->transfer_buffer = NULL;
				usb_free_urb(urb);
				device->channel[index].isocInUrb[i] = NULL;
			} /* if */
		} /* for */
		return ret;
	} /* if */

	for (i = 0; i < DEVICE_NUM_ISOC_IN_URBS; i++) {
		urb = device->channel[index].isocInUrb[i];
		memset(urb->transfer_buffer,
			0,
			DEVICE_ISOC_LENGTH(
				device->channel[index].maxPacketSize));
		urb->transfer_buffer_length =
			DEVICE_ISOC_LENGTH(
				device->channel[index].maxPacketSize);
		urb->number_of_packets = DEVICE_NUM_FRAMES_PER_URB;
		urb->complete = device_iso_in_complete;
		urb->context = &device->channel[index];
		urb->dev = device->usbdev;
		urb->pipe = usb_rcvisocpipe(
			device->usbdev, DEVICE_TS_IN_PIPE + index);
		urb->transfer_flags = URB_ISO_ASAP;
		urb->interval = 1;
		for (j = 0; j < DEVICE_NUM_FRAMES_PER_URB; j++) {
			urb->iso_frame_desc[j].offset =
				j * device->channel[index].maxPacketSize;
			urb->iso_frame_desc[j].length =
				device->channel[index].maxPacketSize;
		} /* for */
	} /* for */
	for (i = 0; i < DEVICE_NUM_ISOC_IN_URBS; i++)
		usb_submit_urb(device->channel[index].isocInUrb[i], GFP_KERNEL);

	/*dbg("end");*/
	return 0;
} /* device_start_iso_in */

static void device_stop_iso_in(struct device_s *device, __u8 index)
{
	int i;

	/*dbg("start");*/

	for (i = 0; i < DEVICE_NUM_ISOC_IN_URBS; i++) {
		if (device->channel[index].isocInUrb[i]) {
			usb_unlink_urb(device->channel[index].isocInUrb[i]);
			device->channel[index].isocInUrb[i] = NULL;
			dbg("unlink urb %i", i);
		} /* if */
	} /* for */

	/*dbg("end");*/
} /* device_stop_iso_in */

static int device_start_tsbulk_in(struct device_s *device, __u8 index)
{
	struct urb *urb;

	dbg("start");

	urb = usb_alloc_urb(0, GFP_KERNEL);
	if (urb) {
		urb->transfer_buffer  = kmalloc(3072, GFP_KERNEL);
		if (!urb->transfer_buffer) {
			err("transfer_buffer allocation failed");
			usb_free_urb(urb);
			return -ENOMEM;
		} /* if */
	} /* if */ else {
		err("usb_alloc_urb failed");
		return -ENOMEM;
	} /* if */
	device->channel[index].bulkInUrb = urb;
	memset(urb->transfer_buffer, 0, 3072);
	urb->transfer_buffer_length = 3072;

	urb->complete = device_tsbulk_in_complete;
	urb->context = &device->channel[index];
	urb->dev = device->usbdev;
	urb->pipe = usb_rcvbulkpipe(device->usbdev, DEVICE_TS_IN_PIPE + index);
	usb_submit_urb(device->channel[index].bulkInUrb, GFP_KERNEL);

	dbg("end");
	return 0;
} /* device_start_tsbulk_in */

static void device_stop_tsbulk_in(struct device_s *device, __u8 index)
{
	dbg("start");

	if (device->channel[index].bulkInUrb) {
		usb_unlink_urb(device->channel[index].bulkInUrb);
		device->channel[index].bulkInUrb = NULL;
		dbg("unlink urb");
	} /* if */

	dbg("end");
} /* device_stop_tsbulk_in */

static void device_iso_out_complete(struct urb *urb)
{
	struct ts_channel_s *channel = urb->context;
	struct urb    *tmpUrb;
	int           i;
	int ret = 0;

	/*dbg("start");*/

/*dbg_dump("txBuf", urb->transfer_buffer, urb->transfer_buffer_length);*/

	if (urb->status || channel->outStop) {
		/* error, free all coming urbs */
		err("free urb");
		channel->outStop = 1;
		atomic_dec(&channel->numOutUrbs);
		kfree(urb->transfer_buffer);
		usb_free_urb(urb);
		return;
	} /* if */

	for (i = 0; i < DEVICE_NUM_ISOC_OUT_URBS; i++) {
		if (urb == channel->isocOutUrb[i])
			break;
	} /* for */
	if (i == DEVICE_NUM_ISOC_OUT_URBS) {
		/* urb must be deleted */
		kfree(urb->transfer_buffer);
		usb_free_urb(urb);
	} /* if */

	if (atomic_dec_and_test(&channel->numOutUrbs)) {
		/* get next free urb */
		tmpUrb = channel->isocOutUrb[channel->nextFreeOutUrbIndex++];
		if (channel->nextFreeOutUrbIndex == DEVICE_NUM_ISOC_OUT_URBS)
			channel->nextFreeOutUrbIndex = 0;

		/* reinitialize urb with null packets */
		memset(tmpUrb->transfer_buffer,
			0xCD, DEVICE_ISOC_LENGTH(channel->maxPacketSize));
		for (i = 0;
			i < DEVICE_ISOC_LENGTH(channel->maxPacketSize);
			i += DEVICE_MPEG2_PACKET_SIZE) {
			memcpy(tmpUrb->transfer_buffer+i,
				nullHeader, sizeof(nullHeader));
		} /* for */
		tmpUrb->transfer_buffer_length =
			DEVICE_ISOC_LENGTH(channel->maxPacketSize);
		tmpUrb->number_of_packets = DEVICE_NUM_FRAMES_PER_URB;
		tmpUrb->complete = device_iso_out_complete;
		tmpUrb->context = channel;
		tmpUrb->transfer_flags = URB_ISO_ASAP;
		tmpUrb->interval = 1;
		for (i = 0; i < DEVICE_NUM_FRAMES_PER_URB; i++) {
			tmpUrb->iso_frame_desc[i].offset =
				i * channel->maxPacketSize;
			tmpUrb->iso_frame_desc[i].length =
				channel->maxPacketSize;
		} /* for */

		/* submit urb */
		ret = usb_submit_urb(tmpUrb, GFP_ATOMIC);
		if (ret)
			err("usb_submit_urb failed %d", ret);

		atomic_inc(&channel->numOutUrbs);
	} /* if */

	/*dbg("end");*/
} /* device_iso_out_complete */

static int device_tsiso_send(struct device_s *device,
	__u8 index, __u8 *data, int size)
{
	int          i, j;
	struct urb   **urb;
	__u32        numUrbs;
	int          ret = 0;

	/*  dbg("start");*/

	numUrbs =
		size / DEVICE_ISOC_LENGTH(device->channel[index].maxPacketSize);
	urb = kmalloc(numUrbs * sizeof(struct urb *), GFP_KERNEL);
	if (!urb) {
		err("urb array allocation failed %d", numUrbs);
		return -ENOMEM;

	} /* if */
	memset(urb, 0, numUrbs * sizeof(struct urb *));

	for (i = 0; i < numUrbs; i++) {
		urb[i] = usb_alloc_urb(DEVICE_NUM_FRAMES_PER_URB, GFP_KERNEL);
		if (urb[i]) {
			urb[i]->transfer_buffer =
				kmalloc(DEVICE_ISOC_LENGTH(
					device->channel[index].maxPacketSize),
						GFP_KERNEL);
			if (!urb[i]->transfer_buffer) {
				ret = -ENOMEM;
				err("transfer_buffer allocation failed %d", i);
				break;
			} /* if */
		} /* if */
		else {
			ret = -ENOMEM;
			err("usb_alloc_urb failed %d", i);
			break;
		} /* if */
	} /* for */
	if (ret) {
		/* Allocation error, must free already allocated data */
		for (i = 0; i < numUrbs; i++) {
			if (urb[i]) {
				kfree(urb[i]->transfer_buffer);
				urb[i]->transfer_buffer = NULL;
				usb_free_urb(urb[i]);
				urb[i] = NULL;
			} /* if */
		} /* for */
		kfree(urb);
		return ret;
	} /* if */

	for (i = 0; i < numUrbs; i++) {
		ret = copy_from_user(urb[i]->transfer_buffer,
			&data[i*DEVICE_ISOC_LENGTH(
					device->channel[index].maxPacketSize)],
				DEVICE_ISOC_LENGTH(
					device->channel[index].maxPacketSize));
		urb[i]->transfer_buffer_length = DEVICE_ISOC_LENGTH(
			device->channel[index].maxPacketSize);
		urb[i]->number_of_packets = DEVICE_NUM_FRAMES_PER_URB;
		urb[i]->complete = device_iso_out_complete;
		urb[i]->context = &device->channel[index];
		urb[i]->dev = device->usbdev;
		urb[i]->pipe = usb_sndisocpipe(device->usbdev,
			DEVICE_TS_OUT_PIPE + index);
		urb[i]->transfer_flags = URB_ISO_ASAP;
		urb[i]->interval = 1;
		for (j = 0; j < DEVICE_NUM_FRAMES_PER_URB; j++) {
			urb[i]->iso_frame_desc[j].offset =
				j * device->channel[index].maxPacketSize;
			urb[i]->iso_frame_desc[j].length =
				device->channel[index].maxPacketSize;
		} /* for */
	} /* for */

	atomic_add(numUrbs, &device->channel[index].numOutUrbs);
	for (i = 0; i < numUrbs; i++) {
		ret = usb_submit_urb(urb[i], GFP_ATOMIC);
		if (ret)
			err("usb_submit_urb failed %d", ret);
	} /* for */

	kfree(urb);

	/*  dbg("end");*/
	return size;
} /* device_tsiso_send */

static int device_start_iso_out(struct device_s *device, __u8 index)
{
	int         i, j;
	int         ret = 0;
	struct urb  *urb;

	/*dbg("start");*/

	device->channel[index].outStop = 0;
	for (i = 0; i < DEVICE_NUM_ISOC_OUT_URBS; i++) {
		urb = usb_alloc_urb(DEVICE_NUM_FRAMES_PER_URB, GFP_KERNEL);
		device->channel[index].isocOutUrb[i] = urb;
		if (urb) {
			urb->transfer_buffer = kmalloc(DEVICE_ISOC_LENGTH(
				device->channel[index].maxPacketSize),
				GFP_KERNEL);
			if (!urb->transfer_buffer) {
				ret = -ENOMEM;
				err("transfer_buffer allocation failed %d", i);
				break;
			} /* if */
		} /* if */ else {
			ret = -ENOMEM;
			err("usb_alloc_urb failed %d", i);
			break;
		} /* if */
	} /* for */

	if (ret) {
		/* Allocation error, must free already allocated data */
		for (i = 0; i < DEVICE_NUM_ISOC_OUT_URBS; i++) {
			urb = device->channel[index].isocOutUrb[i];
			if (urb) {
				kfree(urb->transfer_buffer);
				urb->transfer_buffer = NULL;
				usb_free_urb(urb);
				device->channel[index].isocOutUrb[i] = NULL;
			} /* if */
		} /* for */
		return ret;
	} /* if */

	for (i = 0; i < DEVICE_NUM_ISOC_OUT_URBS; i++) {
		urb = device->channel[index].isocOutUrb[i];
		memset(urb->transfer_buffer, 0, DEVICE_ISOC_LENGTH(
			device->channel[index].maxPacketSize));
		for (j = 0;
			j < DEVICE_ISOC_LENGTH(
				device->channel[index].maxPacketSize);
			j += DEVICE_MPEG2_PACKET_SIZE) {
			memcpy(urb->transfer_buffer+j,
				nullHeader, sizeof(nullHeader));
		} /* for */
		urb->transfer_buffer_length = DEVICE_ISOC_LENGTH(
			device->channel[index].maxPacketSize);
		urb->number_of_packets = DEVICE_NUM_FRAMES_PER_URB;
		urb->complete = device_iso_out_complete;
		urb->context = &device->channel[index];
		urb->dev = device->usbdev;
		urb->pipe = usb_sndisocpipe(device->usbdev,
			DEVICE_TS_OUT_PIPE + index);
		urb->transfer_flags = URB_ISO_ASAP;
		urb->interval = 1;
		for (j = 0; j < DEVICE_NUM_FRAMES_PER_URB; j++) {
			urb->iso_frame_desc[j].offset =
				j * device->channel[index].maxPacketSize;
			urb->iso_frame_desc[j].length =
				device->channel[index].maxPacketSize;
		} /* for */
	} /* for */
	device->channel[index].nextFreeOutUrbIndex = DEVICE_NUM_ISOC_OUT_URBS-1;
	atomic_set(&device->channel[index].numOutUrbs, 1);
	for (i = 0; i < DEVICE_NUM_ISOC_OUT_URBS-1; i++) {
		ret = usb_submit_urb(
			device->channel[index].isocOutUrb[i], GFP_KERNEL);
		if (ret)
			err("usb_submit_urb failed %d", ret);
	} /* for */

	/*dbg("end");*/
	return 0;
} /* device_start_iso_out */

static void device_stop_iso_out(struct device_s *device, __u8 index)
{
	dbg("start");

	device->channel[index].outStop = 1;

	dbg("end");
} /* device_stop_iso_out */

static void device_tsbulk_complete(struct urb *urb)
{
	struct device_s *device = urb->context;
	__u8 index = 0;

	/*dbg("start");*/
	if (!urb->status) {
		if (usb_endpoint_num(&(urb->ep->desc)) != DEVICE_TS_OUT_PIPE)
			index = 1;
		device->channel[index].nbByteSend += urb->actual_length;
	}
	kfree(urb->transfer_buffer);
	usb_free_urb(urb);
	/*dbg("end");*/
} /* device_tsbulk_complete */

static int device_tsbulk_send(struct device_s *device,
	__u8 index, __u8 *data, int size)
{
	struct urb *urb;
	/*  int         todo = size;*/
	int ret = 0;

	dbg("start");

	/* get a free bulk message */
	urb = usb_alloc_urb(0, GFP_KERNEL);
	if (!urb) {
		err("alloc urb");
		return -ENOMEM;
	} /* if */
	urb->dev = device->usbdev;

	/* allocate bulk data */
	urb->transfer_buffer = kmalloc(size, GFP_KERNEL);
	if (!urb->transfer_buffer) {
		err("alloc transfer buffer");
		usb_free_urb(urb);
		return -ENOMEM;
	} /* if */

	/* copy data */
	ret = copy_from_user(urb->transfer_buffer, data, size);

	/* submit bulk */
	urb->pipe = usb_sndbulkpipe(device->usbdev, DEVICE_TS_OUT_PIPE + index);
	urb->transfer_buffer_length = size;
	urb->complete = device_tsbulk_complete;
	urb->context  = device;
	/*dbg("Transmit %d bytes\n",urb->transfer_buffer_length);*/
	/*dbg_dump("txBuf",
		urb->transfer_buffer, urb->transfer_buffer_length);*/
	mod_timer(&(gbulk_timer[index].StartBulkReadTimer),
		usecs_to_jiffies(50));

	if (usb_submit_urb(urb, GFP_KERNEL) < 0) {
		err("submit urb");
		kfree(urb->transfer_buffer);
		usb_free_urb(urb);
		return -ENOMEM;
	} /* if */

	dbg("end");
	return 0;
} /* device_tsbulk_send */

/* ---------------------------------------------------------- */
static void StartBulkRead_func(struct timer_list * timer)
{
	struct bulk_timer_s *bulk_time = from_timer(bulk_time,timer,StartBulkReadTimer) ;

	device_start_tsbulk_in(bulk_time->device, bulk_time->index);
}

static int device_drv_open(struct device_s *device)
{
	int index;
	mutex_lock(&device->lock);

	if (!device->usbdev) {
		err("no dev, can not start dev");
		mutex_unlock(&device->lock);
		return -ENODEV;
	}

	if (device->opened) {
		mutex_unlock(&device->lock);
		device->opened++;
		info("udev=%p opened=%d", (device->usbdev), device->opened);
		return 0;
	} /* while */

	info("set interface 0");
	if (usb_set_interface(device->usbdev, 0, 0) < 0) {
		mutex_unlock(&device->lock);
		err("set_interface fail");
		return -EINVAL;
	} /* if */

	device->opened++;
	device->askToRelease = 0;
	mutex_unlock(&device->lock);

	for (index = 0; index < DEVICE_NUM_CAM; index++) {
		device->channel[index].nbByteSend = -376;
		device->channel[index].nbByteRead = 0;
		device->channel[index].FirstTransfer = true;
		gbulk_timer[index].device = device;
		gbulk_timer[index].index = index;
		timer_setup(&gbulk_timer[index].StartBulkReadTimer,
			StartBulkRead_func,0);
	}
#ifdef DEBUG_CONTINUITY
	init_tab_cc();
#endif
	info("udev=%p opened=%d", (device->usbdev), device->opened);
	return 0;
}

static int device_ci_unlock_read(struct device_s *device)
{
	if (device->opened) {
		/* release blocking functions */
		device->askToRelease = 1;
		wake_up_interruptible(&device->ciBulk.ciData[0].asyncWait);
		wake_up_interruptible(&device->ciBulk.ciData[1].asyncWait);
		wake_up_interruptible(&device->channel[0].inWait);
		wake_up_interruptible(&device->channel[1].inWait);
	}
	return 0;
}

static int device_drv_close(struct device_s *device)
{
	int i;
	mutex_lock(&device->lock);
	if (device->opened && ((--device->opened) == 0)) {
		device->askToRelease = 1;
		device_stop_intr(device);
		for (i = 0; i < DEVICE_NUM_CAM; i++) {
			if (device->useIsoc) {
				device_stop_iso_out(device, i);
				device_stop_iso_in(device, i);
			} else
				device_stop_tsbulk_in(device, i);
		} /* for */
		device->opened = 0;
	} /* if */
	mutex_unlock(&device->lock);
	return 0;
}

static int device_open(struct inode *inode, struct file *file)
{
	int       devnum = iminor(inode);
#ifdef DEBUG
	int        type = (MINOR(inode->i_rdev) >> 4);
	int        num = (MINOR(inode->i_rdev) & 0xf);
#endif
	int   ret = 0;

	struct device_s *device;

	dbg("start, devnum = %d type = %d num = %d", devnum, type, num);

	if (gdeviceNumber >= DRIVER_MAX_NUMBER) {
		dbg("only support one device open");
		return -EINVAL;
	}
	device = &gdevice[gdeviceNumber];
	/*gdeviceNumber++;*/

	ktime_get_ts(&gStart);

	ret = device_drv_open(device);
	if (ret < 0)
		return ret;

	file->f_pos = 0;
	file->private_data = device;

	dbg("end");
	return nonseekable_open(inode, file);
} /* device_open */

static int device_release(struct inode *inode, struct file *file)
{
	struct device_s *device = (struct device_s *)file->private_data;

	dbg("start");
	device_drv_close(device);
	dbg("end");
	return 0;
} /* device_release */

int cimax_usb_select_interface(struct device_s *device, unsigned long intf)
{
	int          max    =  0;
	int          mult    =  0;
	int          ret    =  0;

	info("set interface %ld", intf);
	if (usb_set_interface(device->usbdev, 0, intf) < 0) {
		err("set_interface failed interface 0, altSetting %ld", intf);
		return -EINVAL;
	} /* if */

	/* check endpoints */
	/* CI bulk out */
	if (!usb_endpoint_is_bulk_out(
			&device->usbdev->ep_out[DEVICE_BULK_OUT_PIPE]->desc)) {
		err("unexpected endpoint %d", DEVICE_BULK_OUT_PIPE);
		return -EINVAL;
	} /* if */
	device->ciBulk.outMaxPacketSize = DEVICE_BULK_OUT_MAXPACKET;
	dbg("CI bulk out (endpoint %d), packet size %d", DEVICE_BULK_OUT_PIPE,
			device->ciBulk.outMaxPacketSize);
	/* CI int in */
	if (!usb_endpoint_is_int_in(
			&device->usbdev->ep_in[DEVICE_INT_IN_PIPE]->desc)) {
		err("unexpected endpoint %d", DEVICE_INT_IN_PIPE);
		return -EINVAL;
	} /* if */
	device->ciBulk.inMaxPacketSize =
		device->usbdev->ep_in[DEVICE_INT_IN_PIPE]->desc.wMaxPacketSize;
	dbg("CI int in (endpoint %d), packet size %d", DEVICE_INT_IN_PIPE,
			device->ciBulk.inMaxPacketSize);
	/* TS out */
	if (device->usbdev->ep_out[DEVICE_TS_OUT_PIPE] == NULL)
		dbg("no TS endpoint");
	else {
		if (usb_endpoint_is_bulk_out(
			&device->usbdev->ep_out[DEVICE_TS_OUT_PIPE]->desc)) {
			device->useIsoc = 0;
			dbg("TS is configured as bulk");
		} else if (usb_endpoint_is_isoc_out(
			&device->usbdev->ep_out[DEVICE_TS_OUT_PIPE]->desc)) {
			device->useIsoc = 1;
			dbg("TS is configured as isochronous");
		} else {
			err("unexpected endpoint %d", DEVICE_TS_OUT_PIPE);
			return -EINVAL;
		} /* if */
		max = device->usbdev->
			ep_out[DEVICE_TS_OUT_PIPE]->desc.wMaxPacketSize;
		mult = 1 + ((max >> 11) & 0x03);
		max &= 0x7ff;
		device->channel[0].maxPacketSize = max * mult;
		dbg("TS out (endpoint %d), packet size %d", DEVICE_TS_OUT_PIPE,
				device->channel[0].maxPacketSize);

		max = device->usbdev->
			ep_out[DEVICE_TS_OUT_PIPE+1]->desc.wMaxPacketSize;
		mult = 1 + ((max >> 11) & 0x03);
		max &= 0x7ff;
		device->channel[1].maxPacketSize = max * mult;
		dbg("TS out (endpoint %d), packet size %d",
				DEVICE_TS_OUT_PIPE + 1,
				device->channel[1].maxPacketSize);
	}

	/* start intr urb */
	if (device->ciBulk.intUrb[0] == NULL) {
		ret = device_start_intr(device);
		if (ret < 0) {
			err("cannot start int urb");
			return ret;
		} /* if */
	} /* if */

	return ret;
}
EXPORT_SYMBOL(cimax_usb_select_interface);

static int device_ci_write(struct device_s *device,
		struct ioctl_data_s *data, int isIoctl)
{
	int ret = 0;
	if (!device)
		return -ENODEV;
	ret = device_cibulk_send(device, data, isIoctl);
	if (ret < 0)
		return ret;
	return device_wait_sync_data(device, ret, data, isIoctl);
}

static int device_ci_write_ioctl(struct device_s *device,
		struct ioctl_data_s *data)
{
	return device_ci_write(device, data, 1);
}

int cimax_usb_ci_write(struct device_s *device,
		u8 *txData, int txSize, u8 *rxData, int rxSize)
{
	struct ioctl_data_s data;
	if (!device)
		return -ENODEV;
	memset(&data, 0, sizeof(data));
	data.txData = txData;
	data.txSize = txSize;
	data.rxData = rxData;
	data.rxSize = rxSize;
	return device_ci_write(device, &data, 0);
}
EXPORT_SYMBOL(cimax_usb_ci_write);

int cimax_usb_ci_read_evt(struct device_s *device,
		int moduleId, u8 *buf, int size)
{
	int ret = 0;
	struct rw_data_s data;
	if (!device || !device->opened)
		return -ENODEV;
	memset(&data, 0, sizeof(data));
	data.moduleId = moduleId;
	data.data = buf;
	data.size = size;
	ret = device_wait_async_data(device, data.moduleId, &data, 0);
	if (ret < 0) {
		err("wait ci read failed");
		return ret;
	} /* if */
	dbg("return CI, moduleId %d, data 0x%p, size %d",
			data.moduleId, data.data, data.size);
	return ret;
}
EXPORT_SYMBOL(cimax_usb_ci_read_evt);

int cimax_usb_device_open(struct device_s *device)
{
	return device_drv_open(device);
}
EXPORT_SYMBOL(cimax_usb_device_open);

int cimax_usb_device_unlock_read(struct device_s *device)
{
	int ret = 0;
	if (!device)
		return 0;
	mutex_lock(&device->lock);
	ret = device_ci_unlock_read(device);
	mutex_unlock(&device->lock);
	return ret;
}
EXPORT_SYMBOL(cimax_usb_device_unlock_read);

int cimax_usb_device_close(struct device_s *device)
{
	if (!device)
		return 0;
	cimax_usb_device_unlock_read(device);
	if (cimax_usb_dev_remove)
		cimax_usb_dev_remove(device, gdeviceNumber);
	return device_drv_close(device);
}
EXPORT_SYMBOL(cimax_usb_device_close);

static long device_ioctl(struct file *file,
		unsigned int cmd, unsigned long arg)
{
	struct device_s *device = (struct device_s *)file->private_data;
	int          err    = 0;
	int          ret    = 0;
	struct ioctl_data_s data;
	void         *transfer_buffer = NULL;

	dbg("start");

	/* Don't decode wrong cmds: return ENOTTY (inappropriate ioctl) */
	if (_IOC_TYPE(cmd) != DEVICE_IOC_MAGIC)
		return -ENOTTY;
	if (_IOC_NR(cmd) > DEVICE_IOC_MAXNR)
		return -ENOTTY;

	/* Verify direction (read/write) */
	if (_IOC_DIR(cmd) & _IOC_READ)
//		err = !access_ok(VERIFY_WRITE, (void *)arg, _IOC_SIZE(cmd));
		err = !access_ok((void *)arg, _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
//		err = !access_ok(VERIFY_READ, (void *)arg, _IOC_SIZE(cmd));
		err = !access_ok((void *)arg, _IOC_SIZE(cmd));
	if (err)
		return -EFAULT;

	if (mutex_lock_interruptible(&device->lock))
		return -ERESTARTSYS;

	switch (cmd) {
	case DEVICE_IOC_SELECT_INTF:
		dbg("DEVICE_IOC_SELECT_INTF start");
		ret = cimax_usb_select_interface(device, arg);
		dbg("DEVICE_IOC_SELECT_INTF end");
		break;

	case DEVICE_IOC_CI_WRITE:
		dbg("DEVICE_IOC_CI_WRITE start");

		/* send CI message */
		ret = copy_from_user(&data,
			(void __user *)arg, sizeof(struct ioctl_data_s));
		dbg("inMsg, rx 0x%p, rxSize %d, tx 0x%p, txSize %d",
			data.rxData,
			data.rxSize,
			data.txData,
			data.txSize);
		ret = device_ci_write_ioctl(device, &data);
		if (ret < 0)
			break;
		ret = copy_to_user((void __user *)arg,
			&data, sizeof(struct ioctl_data_s));

		dbg("DEVICE_IOC_CI_WRITE end");
		break;

	case DEVICE_IOC_UNLOCK_READ:
		dbg("DEVICE_IOC_UNLOCK_READ start");

		ret = device_ci_unlock_read(device);

		dbg("DEVICE_IOC_UNLOCK_READ end");
		break;

	case DEVICE_IOC_SET_CONFIG:
		dbg("DEVICE_IOC_SET_CONFIG start");

		/* send CI message */
		ret = copy_from_user(&data,
			(void __user *)arg, sizeof(struct ioctl_data_s));
		dbg("inMsg, rx 0x%p, rxSize %d, tx 0x%p, txSize %d",
			data.rxData, data.rxSize,
			data.txData, data.txSize);
		transfer_buffer = kmalloc(data.txSize, GFP_KERNEL);
		memcpy(transfer_buffer, data.txData, data.txSize);
		dbg_dump("New config", transfer_buffer, data.txSize);
		err = usb_control_msg(device->usbdev,
			usb_sndctrlpipe(device->usbdev, 0),
			USB_REQ_SET_DESCRIPTOR,
			USB_TYPE_STANDARD,
			(USB_DT_CONFIG << 8),
			0,
			transfer_buffer,
			data.txSize,
			5000);
		if (err < 0) {
			err("set_config failed %d", err);
			ret = -EINVAL;
		}
		kfree(transfer_buffer);
		dbg("DEVICE_IOC_SET_CONFIG end");
		break;

	default:
		ret = -ENOIOCTLCMD;
		break;
	} /* switch */
	mutex_unlock(&device->lock);

	dbg("end, ret %d", ret);
	return ret;
} /* device_ioctl */

static ssize_t device_write(struct file *file, const char __user *buf,
		size_t count, loff_t *ppos)
{
	int       ret;
	struct rw_data_s data;
	struct device_s *device = (struct device_s *)file->private_data;

	dbg("start");

	/* get transmission buffer */
	ret = copy_from_user(&data, buf, sizeof(struct rw_data_s));
	dbg("txBuffer, moduleId %u, data 0x%p, size %d",
			data.moduleId, data.data, data.size);
	if (data.moduleId >= DEVICE_NUM_CAM) {
		err("bad moduleId");
		return 0;
	}

	if (device->useIsoc) {
		if (!data.size || (data.size % DEVICE_ISOC_LENGTH(
			device->channel[data.moduleId].maxPacketSize))) {
			err("transmission buffer size must be a multiple of %d",
				DEVICE_ISOC_LENGTH(
				device->channel[data.moduleId].maxPacketSize));
			return -EINVAL;
		} /* if */
	}

	if (device->useIsoc) {
		if (device->channel[data.moduleId].isocInUrb[0] == NULL) {
			ret = device_start_iso_in(device, data.moduleId);
			if (ret < 0)
				return ret;
		} /* if */

		if (device->channel[data.moduleId].isocOutUrb[0] == NULL) {
			ret = device_start_iso_out(device, data.moduleId);
			if (ret < 0)
				return ret;
		} /* if */

		dbg("call device_tsiso_send moduleId %d, data 0x%p, size %d",
			data.moduleId, data.data, data.size);
		ret = device_tsiso_send(device,
			data.moduleId, data.data, data.size);
	} /* if */
	else {
		ret = device_tsbulk_send(device,
			data.moduleId, data.data, data.size);
	} /* else */

	dbg("end, moduleId %d return %d", data.moduleId, ret);
	return ret;
} /* device_write */

static ssize_t device_read(struct file *file, char __user *buf,
		size_t count, loff_t *ppos)
{
	int       res;
	struct rw_data_s data;
	struct device_s *device = (struct device_s *)file->private_data;

	dbg("start");

	if (count != sizeof(struct rw_data_s)) {
		err("try to read uncorrect size %zd", count);
		return -EFAULT;
	} /* if */
	res = copy_from_user(&data, buf, sizeof(struct rw_data_s));
	data.copiedSize = 0;
	if (data.type == DEVICE_TYPE_TS_READ) {
		res = device_fill_ts(device, data.moduleId, &data);
		if (res < 0) {
			err("fill ts buffer failed");
			return res;
		} /* if */
		dbg("return TS, moduleId %d, data 0x%p, size %d, copiedSize %d",
			data.moduleId, data.data, data.size, data.copiedSize);
		/*res = count;*/
		res = data.copiedSize;
	} /* if */
	else if (data.type == DEVICE_TYPE_CI_READ) {
		res = device_wait_async_data(device, data.moduleId, &data, 1);
		if (res < 0) {
			err("wait ci read failed");
			return res;
		} /* if */
		dbg("return CI, moduleId %d, data 0x%p, size %d",
				data.moduleId, data.data, data.size);
		res = data.size;
	} /* else if */
	else {
		err("unknown data type %d", data.type);
		res = -EFAULT;
	} /* else */

	dbg("end, return %d", res);
	return res;
} /* device_read */

/******************************************************************************
 * @brief
 *   write data on Control endpoint.
 *

 * @param   dev
 *   Pointer to usb device.
 *
 * @param   addr

 *   register address to write.
 *
 * @param   data
 *   data to write.
 *

 * @param   size
 *   size to write.
 *
 * @return

 *   data writen or ENODEV error
 ******************************************************************************/
int write_ctrl_message(struct usb_device *dev, int addr, void *data, int size)
{
	int ret;
	void *ptr = NULL;
#ifdef DEBUG
	/*   int i;*/
	/*   unsigned char dump[500];*/
#endif

	/*   info("%s: . addr = %04x size=%d",DRIVER_NAME,addr,size);*/

	if (size <= 0)
		return 0;

	ptr = kmemdup(data, size, GFP_KERNEL);
	if (!ptr)
		return -ENOMEM;

	ret = usb_control_msg
			(dev, usb_sndctrlpipe(dev, 0), 0xA0, 0x40, addr, 0x0001,
			ptr, size, 300);
	if (ret != size) {
		err("Failed to write CIMaX+ register 0x%04x", addr);
		ret = -ENODEV;
	}

#ifdef DEBUG
	/*   dump[0] =0;
		 for(i=0;i<size;i++) {
		 if((i !=0) && ((i%16) == 0)) {
		 dbg("cimax+usb: %s",dump);
		 dump[0] =0;
		 }
		 sprintf(dump,"%s%02x ",dump,((unsigned char *)ptr)[i]);
		 }
		 dbg("cimax+usb: %s",dump);
	 */
#endif
	kfree(ptr);

	return ret;
}

/******************************************************************************
 * @brief
 *   read data from Control endpoint.
 *

 * @param   dev
 *   Pointer to usb device.
 *
 * @param   addr
 *   firmware address to read.

 *
 * @param   data
 *   pointer to buffer to fill with register data.
 *

 * @param   size
 *   size to read.
 *
 * @return
 *   data writen or ENODEV error

 ******************************************************************************/
int read_ctrl_message(struct usb_device *dev, int addr, void *data, int size)
{
	int ret;
	ret = usb_control_msg
			(dev, usb_rcvctrlpipe(dev, 0), 0xA0, 0xC0, addr, 0x0001,
			(void *)data, size, 300);
	if (ret != size) {
		err("Failed to read CIMaX+ register 0x%04x return %d",
			addr, ret);
		ret = -ENODEV;
	}
	return ret;
}

/******************************************************************************
 * @brief
 *   Start new Firmware.
 *
 * @param   dev
 *   Pointer to usb device.
 *
 * @return
 *   None.
 ******************************************************************************/
int init_fw(struct usb_device *dev)
{
	int len;
	char *bootStatus = NULL;
	bootStatus = kmalloc(sizeof(char), GFP_KERNEL);
	if (!bootStatus) {
		pr_err("%s: init_fw kmalloc failed\n",
				DRIVER_NAME);
		return 0;
	}

	info("%s: .", DRIVER_NAME);
	len = usb_control_msg(dev, usb_rcvctrlpipe(dev, 0),
		0xA0, 0xC0, 0x0000, 0x0000, bootStatus, 1, 100);
	if (len == 1) {
		info("--> Init Status = %02X", *bootStatus);
		if (bootStatus)
			kfree(bootStatus);
		return 0;
	}
	if (bootStatus)
		kfree(bootStatus);

	return len;
}

/******************************************************************************
 * @brief
 *   Start new Firmware.
 *
 * @param   dev
 *   Pointer to usb device.

 *
 * @return
 *   None.
 ******************************************************************************/
int write_ep6_message(struct usb_device *dev, void *data, int size)
{
	int ret;
	void * ptr = NULL;
	ptr = kmemdup(data, size, GFP_KERNEL);
	if (!ptr)
		return -ENOMEM;

	if (usb_bulk_msg(dev, usb_sndbulkpipe(dev, 6),
			ptr, size, &ret, 200) < 0) {
		err("Failed to write cmd 0x%02x", ((unsigned char *)data)[0]);
		ret = -ENODEV;
	}
	kfree(ptr);
	return ret;
}

/******************************************************************************
 * @brief
 *   Start new Firmware.
 *
 * @param   dev
 *   Pointer to usb device.
 *
 * @return
 *   None.
 ******************************************************************************/
int read_ep5_message(struct usb_device *dev, void *data, int size)
{
	int ret;

	if (usb_interrupt_msg(dev, usb_rcvintpipe(dev, 5),
			data, size, &ret, 200) < 0) {
		err("Failed read interrupt endpoint");
		ret = -ENODEV;
	}
	return ret;
}

#ifdef CONFIG_COMPAT
static long device_compat_ioctl(struct file *filp,
		unsigned int cmd, unsigned long args)
{
	unsigned long ret;

	args = (unsigned long)compat_ptr(args);
	ret = device_ioctl(filp, cmd, args);
	return ret;
}
#endif

static const struct file_operations device_fops = {
	.owner   = THIS_MODULE,
	.open    = device_open,
	.release = device_release,
	.unlocked_ioctl = device_ioctl,
	.write   = device_write,
	.read    = device_read,
	/*
	.poll    = device_poll,
	*/
#ifdef CONFIG_COMPAT
	.compat_ioctl = device_compat_ioctl,
#endif
};

struct cimaxusb_priv_ops_t cimaxusb_priv_ops = {
	.write_ctrl_message  = write_ctrl_message,
	.read_ctrl_message   = read_ctrl_message,
	.init_fw             = init_fw,
	.write_ep6_message   = write_ep6_message,
	.read_ep5_message    = read_ep5_message
};

static struct usb_class_driver device_class = {
	.name = "cimaxusb%d",
	.fops = &device_fops,
	.minor_base = DEVICE_MINOR,
};

/* ---------------------------------------------------------- */


void cimax_usb_set_cb(void *cb1, void *cb2)
{
	cimax_usb_dev_add = cb1;
	cimax_usb_dev_remove = cb2;
}
EXPORT_SYMBOL(cimax_usb_set_cb);

static int device_probe(struct usb_interface *intf,
		const struct usb_device_id *id)
{
	struct usb_device *usbdev = interface_to_usbdev(intf);
	struct device_s *device;
	char cmd[] = { 0x0C, 0x01, 0x00, 0x00 };
	char *rsp;

	dbg("start vendor id 0x%x, product id 0x%x, Device id 0x%x minor 0x%x",
			le16_to_cpu(usbdev->descriptor.idVendor),
			le16_to_cpu(usbdev->descriptor.idProduct),
			le16_to_cpu(usbdev->descriptor.bcdDevice),
			intf->minor);

	/*  device = &gdevice[intf->minor];*/
	device = &gdevice[gdeviceNumber];

	mutex_lock(&device->lock);
	/*  device->usbdev = usbdev;*/
	device->usbdev = usb_get_dev(usbdev);
	dbg("device->usbdev 0x%p", (device->usbdev));

	/* set private callback functions */
	device->ops = &cimaxusb_priv_ops;

	device->askToSuspend = 0;

	usb_set_intfdata(intf, device);
	mutex_unlock(&device->lock);

	if (usb_register_dev(intf, &device_class)) {
		err("usb_register_dev");
		usb_set_intfdata(intf, NULL);
		return -ENOMEM;
	} /* if */

	/* test if firmware loafing is needed */
#ifdef FRBIT
	if ((le16_to_cpu(usbdev->descriptor.bcdDevice) != 0) &&
			(CimaxDwnl == 1)) {
#else
	if (le16_to_cpu(usbdev->descriptor.bcdDevice) != 0) {
#endif
		info("start firmware download");
		/* load firmware*/
		cimaxusb_fw_upload(device);
		info("end firmware download");
	} else {
		info("set alternate setting 1");
		if (usb_set_interface(device->usbdev, 0, 1) < 0) {
			err("set_interface failed intf 0, alt 1");
		} else {
			info("check FW version");
			/* Get BOOT version */
			if (write_ep6_message(device->usbdev,
				cmd, sizeof(cmd)) == sizeof(cmd)) {
				rsp = kcalloc(256,
					sizeof(unsigned char),
					GFP_KERNEL);
				if (!rsp) {
					err("out of memory");
					return -ENOMEM;
				}
				if (read_ep5_message(device->usbdev,
						rsp, 256) >= 0) {
					info("=> ---- F.W. Version -------");
					info("=>= %02X.%02X.%02X.%02X.%02X%c",
						rsp[4], rsp[5], rsp[6],
						rsp[7], rsp[8], rsp[9]);
					info("=> Boot Version = %d.%d",
						rsp[10], rsp[11]);
					info("=> --------------------");
				}
				kfree(rsp);
			}
		}
		info("start cfg download");
		if (cimaxusb_configuration_setting(device) < 0)
			err(" Error : set CIMaX+ configuration");
		info("end cfg download");

		if (cimax_usb_dev_add)
			cimax_usb_dev_add(device, gdeviceNumber);
	}

	dbg("end");
	return 0;
} /* device_probe */

static void device_disconnect(struct usb_interface *intf)
{
	struct device_s *device = usb_get_intfdata(intf);
	int       i;

	dbg("start");

	if (!device)
		return;

	mutex_lock(&device->lock);
	if (device->opened) {
		/* release blocking functions */
		device->askToRelease = 1;
		wake_up_interruptible(&device->ciBulk.ciData[0].asyncWait);
		wake_up_interruptible(&device->ciBulk.ciData[1].asyncWait);
		wake_up_interruptible(&device->channel[0].inWait);
		wake_up_interruptible(&device->channel[1].inWait);
		device_stop_intr(device);
		for (i = 0; i < DEVICE_NUM_CAM; i++) {
			if (device->useIsoc) {
				device_stop_iso_out(device, i);
				device_stop_iso_in(device, i);
			} /* if */
			else
				device_stop_tsbulk_in(device, i);
		} /* for */
		device->opened = 0;

		if (cimax_usb_dev_remove)
			cimax_usb_dev_remove(device, gdeviceNumber);
	} /* if */
	mutex_unlock(&device->lock);
	usb_set_intfdata(intf, NULL);
	if (device) {
		usb_deregister_dev(intf, &device_class);
		device->usbdev = NULL;
	} /* if */
	dbg("end");
} /* device_disconnect */

static int cimaxusb_suspend(struct usb_interface *intf, pm_message_t message)
{
	struct device_s *device = usb_get_intfdata(intf);
	int       i;

	dbg("start");

	if (!device)
		return 0;

	mutex_lock(&device->lock);
	if (device->opened) {
		/* release blocking functions */
		device->askToSuspend = 1;
		wake_up_interruptible(&device->ciBulk.ciData[0].asyncWait);
		wake_up_interruptible(&device->ciBulk.ciData[1].asyncWait);
		wake_up_interruptible(&device->channel[0].inWait);
		wake_up_interruptible(&device->channel[1].inWait);
		device_stop_intr(device);
		for (i = 0; i < DEVICE_NUM_CAM; i++) {
			if (device->useIsoc) {
				device_stop_iso_out(device, i);
				device_stop_iso_in(device, i);
			} /* if */
			else
				device_stop_tsbulk_in(device, i);
		} /* for */
		device->opened = 0;
	} /* if */
	mutex_unlock(&device->lock);
	dbg("end");

	return 0;
}

static int cimaxusb_resume(struct usb_interface *intf)
{
	struct device_s *device = usb_get_intfdata(intf);

	dbg("start");

	if (!device)
		return 0;

	device->askToSuspend = 0;
	dbg("end");
	return 0;
}

static struct usb_device_id device_ids[] = {
	{ USB_DEVICE(0x1b0d, 0x2f00) },
	{ USB_DEVICE(0x1b0d, 0x2f01) },
	{ USB_DEVICE(0x1b0d, 0x2f02) },
	{ USB_DEVICE(0x1b0d, 0x2f03) },
	{ USB_DEVICE(0x1b0d, 0x2f04) },
	{ }           /* Terminating entry */
};

MODULE_DEVICE_TABLE(usb, device_ids);


static struct usb_driver device_driver = {
	.name = "cimaxusb",
	.probe = device_probe,
	.disconnect = device_disconnect,
	.suspend =  cimaxusb_suspend,
	.resume = cimaxusb_resume,
	.id_table = device_ids,
};

/* ---------------------------------------------------------- */

static int device_init_module(void)
{
	int ret = 0;
	int i, j;
	struct device_s *device;

	info("start");

	if (!gdevice)
		gdevice = kcalloc(DRIVER_MAX_NUMBER,
			sizeof(struct device_s), GFP_KERNEL);
	if (!gdevice) {
		err("not enough memory");
		return -ENOMEM;
	}

	for (i = 0; i < DRIVER_MAX_NUMBER; i++) {
		device = &gdevice[i];
		/* initialize struct */
		memset(device, 0, sizeof(struct device_s));
		mutex_init(&device->lock);

		/* initialize ci bulk struct */
		device->ciBulk.counter = 1;
		spin_lock_init(&device->ciBulk.intLock);
		spin_lock_init(&device->ciBulk.intUrbLock);
		for (j = 0; j < DEVICE_NUM_CAM; j++) {
			init_waitqueue_head(&device->ciBulk.ciData[j].syncWait);
			init_waitqueue_head(
				&device->ciBulk.ciData[j].asyncWait);
			INIT_LIST_HEAD(&device->ciBulk.ciData[j].asyncDataList);
		} /* for */

		/* initialize channels */
		for (j = 0; j < DEVICE_NUM_CAM; j++) {
			spin_lock_init(&device->channel[j].inLock);
			init_waitqueue_head(&device->channel[j].inWait);
			vb_init(&device->channel[j].inVb);
			device->channel[j].syncOffset = -1;
			spin_lock_init(&device->channel[j].outLock);
		} /* for */
	} /* for */

	/* register misc device */
	ret = usb_register(&device_driver);

#ifdef TIMESTAMP
	InitTimestamp();
#endif

	if (ret)
		info("end driver register failed");
	else
		info("end driver registered");

	info(DRIVER_VERSION ":" DRIVER_DESC);

	return ret;
} /* device_init_module */

static void device_exit_module(void)
{
	int i;
	struct device_s *device;

	info("start");

#ifdef TIMESTAMP
	ShowTimestamp();
#endif

	for (i = 0; i < DRIVER_MAX_NUMBER; i++) {
		device = &gdevice[i];
		device->askToRelease = 1;
		/* destroy struct */
		mutex_destroy(&device->lock);
	} /* for */
	usb_deregister(&device_driver);
	gdeviceNumber = 0;

	kfree(gdevice);
	gdevice = NULL;
	info("end");
} /* device_exit_module */

module_init(device_init_module);
module_exit(device_exit_module);
