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
 * @file    cimax+usb-driver.h
 *
 * @brief   CIMaX+ USB Driver for linux based operating systems.
 *
 * Bruno Tonelli   <bruno.tonelli@smardtv.com>
 * & Franck Descours <franck.descours@smardtv.com>
 * for SmarDTV France, La Ciotat
 */
#include <linux/version.h>

#ifndef CIMAXPLUS_USB_DRIVER_H
#define CIMAXPLUS_USB_DRIVER_H

/******************************************************************************
 * Includes
 *****************************************************************************/
/******************************************************************************
 * Defines
 *****************************************************************************/
/**
 *  @brief
 *    Driver Name
 */
#define DRIVER_NAME        "cimax+usb"
/**
 *  @brief
 *    An unassigned USB minor.
 */
#define DEVICE_MINOR                          240

/**
 *  @brief
 *    Driver version.
 */
#define DEVICE_VERSION                     0x1000

/**
 *  @brief
 *    Number of CA module supported by the driver.
 */
#define DEVICE_NUM_CAM                          2

/**
 *  @brief
 *    Buffer length.
 */
#define DEVICE_MESSAGE_LENGTH                4100

/* Offset */
#define DEVICE_COMMAND_OFFSET                   0
#define DEVICE_STATUS_OFFSET                    0
#define DEVICE_COUNTER_OFFSET                   1
#define DEVICE_LENGTH_MSB_OFFSET                2
#define DEVICE_LENGTH_LSB_OFFSET                3
#define DEVICE_DATA_OFFSET                      4

/* Mask */
#define DEVICE_SEL_MASK                      0x80
#define DEVICE_TYP_MASK                      0x40
#define DEVICE_CMD_MASK                      0x3F

/* Command tag */
#define DEVICE_CMD_INIT                      0x00
#define DEVICE_CMD_WRITE_REG                 0x7F
#define DEVICE_CMD_READ_REG                  0xFF
#define DEVICE_CMD_CAMRESET                  0x01
#define DEVICE_CMD_GETCIS                    0x02
#define DEVICE_CMD_WRITECOR                  0x03
#define DEVICE_CMD_NEGOTIATE                 0x04
#define DEVICE_CMD_WRITELPDU                 0x05
#define DEVICE_CMD_READLPDU                  0x06
#define DEVICE_CMD_WRITEEXT                  0x07
#define DEVICE_CMD_READEXT                   0x08
#define DEVICE_CMD_CC1RESET                  0x09
#define DEVICE_CMD_MCARD_WRITE               0x0a

/* Status field */
#define DEVICE_CAMRESETOK                    0x00
#define DEVICE_CISOK                         0x01
#define DEVICE_WRITECOROK                    0x02
#define DEVICE_NEGOTIATEOK                   0x03
#define DEVICE_WRITELPDUOK                   0x04
#define DEVICE_CAMDET                        0x05
#define DEVICE_READLPDUOK                    0x06
#define DEVICE_WRITEEXTOK                    0x07
#define DEVICE_READEXTOK                     0x08
#define DEVICE_NO_CAM                        0x09
#define DEVICE_NOK                           0x0a
#define DEVICE_INITOK                        0x0b
#define DEVICE_READ_REGOK                    0x0c
#define DEVICE_WRITE_REGOK                   0x0d
#define DEVICE_DATAREADY                     0x0e
#define DEVICE_MCARD_WRITEOK                 0x0f
#define DEVICE_MCARD_READ                    0x10
#define DEVICE_CAMPARSE_ERROR                0x11
#define DEVICE_WRITELPDUBUSY                 0x14
#define DEVICE_CMDPENDING                    0x16
#define DEVICE_REGSTATUSOK                   0x17
#define DEVICE_GPIOCHANGE                    0x18
#define DEVICE_FRBit                         0x1A


#define DEVICE_DATAREADY_SYNC                0x3e

/**
 *  @brief
 *    MPEG2 transport size,.isochronous size and number of frames per URB.
 */
#define DEVICE_MPEG2_PACKET_SIZE             188
#define DEVICE_MPEG2_SYNC_BYTE               0x47
#define DEVICE_NULL_HEADER_SIZE              8
#define DEVICE_NUM_FRAMES_PER_URB            8
#define DEVICE_ISOC_LENGTH(x)                (DEVICE_NUM_FRAMES_PER_URB*x)
#define DEVICE_VB_LENGTH                     902400

/**
 *  @brief
 *    Endpoint address.
 */
#define DEVICE_TS_IN_PIPE                      1 /* and 2 */
#define DEVICE_TS_OUT_PIPE                     3 /* and 4 */
#define DEVICE_INT_IN_PIPE                     5
#define DEVICE_BULK_OUT_PIPE                   6
#define DEVICE_BULK_OUT_MAXPACKET            256

/**
 *  @brief
 *    Number of isochronous/int URBs in the driver.
 */
#define DEVICE_NUM_ISOC_OUT_URBS                3
#define DEVICE_NUM_ISOC_IN_URBS                 2
#define DEVICE_NUM_INT_IN_URBS                  2

/**
 *  @brief
 *    ioctl() calls definition.
 */
#define DEVICE_IOC_MAGIC        'a'
#define DEVICE_IOC_SELECT_INTF  _IOWR(DEVICE_IOC_MAGIC,  0, signed long)
#define DEVICE_IOC_CI_WRITE     _IOWR(DEVICE_IOC_MAGIC,  1, struct ioctl_data_s)
#define DEVICE_IOC_UNLOCK_READ  _IOWR(DEVICE_IOC_MAGIC,  2, signed long)
#define DEVICE_IOC_SET_CONFIG   _IOWR(DEVICE_IOC_MAGIC,  3, struct ioctl_data_s)
#define DEVICE_IOC_MAXNR        4

/******************************************************************************
 * Types
 *****************************************************************************/
#ifdef __KERNEL__
#include <linux/list.h>
#include <linux/atomic.h>
#include <linux/printk.h>

#undef dbg
#undef dbg_isoc_in
#undef dbg_isoc_out

#undef err
#undef info
#undef warn

#define DEBUG

#ifdef DEBUG
#define dbg(format, arg...) pr_debug("cimax+usb: %s> " format "\n" , \
		__func__, ## arg)
#define dbg_s(format, arg...)\
	pr_debug("cimax+usb: " format "\n" , ## arg)
#else
#define dbg(format, arg...) do {} while (0)
#define dbg_s(format, arg...) do {} while (0)
#endif

#ifdef DEBUG_ISOC_IN
#define dbg_isoc_in(format, arg...)\
	pr_debug("cimax+usb: %s> " format "\n" , \
			__func__, ## arg)
#else
#define dbg_isoc_in(format, arg...) do {} while (0)
#endif

#ifdef DEBUG_ISOC_OUT
#define dbg_isoc_out(format, arg...)\
	pr_debug("cimax+usb: %s> " format "\n" , \
			__func__, ## arg)
#else
#define dbg_isoc_out(format, arg...) do {} while (0)
#endif

#define err(format, arg...)\
	pr_err("cimax+usb: %s> ERROR " format "\n" , \
			__func__, ## arg)
#define info(format, arg...)\
	pr_info("cimax+usb: %s> " format "\n" , \
			__func__, ## arg)
#define warn(format, arg...)\
	pr_warn("cimax+usb: %s> WARN" format "\n" , \
			__func__, ## arg)

/**
 *  @brief
 *    Video buffer structure.
 */
struct video_buf_s {
	__u8 data[DEVICE_VB_LENGTH];
	int  readOffset;
	int  writeOffset;
	int  isEmpty;
};
#endif

/**
 *  @brief
 *    Io control data structure exchanged between user and kernel space.
 */
struct ioctl_data_s {
	__u8  *txData;
	__u32 txSize;
	__u8  *rxData;
	__u32 rxSize;
};

/**
 *  @brief
 *    Read/write type exchanged between user and kernel space.
 */
enum rw_type_e {
	DEVICE_TYPE_CI_READ,
	DEVICE_TYPE_TS_WRITE,
	DEVICE_TYPE_TS_READ
};

/**
 *  @brief
 *    Read/write data structure exchanged between user and kernel space.
 */
struct rw_data_s {
	enum rw_type_e type;
	__u8      moduleId;
	__u8      *data;
	__u32     size;
	__u32     copiedSize;
};
#ifdef __KERNEL__
/**
 *  @brief
 *    Message node structure. Can be inserted in a list.
 */
struct message_node_s {
	__u8              data[DEVICE_MESSAGE_LENGTH];
	__u32             size;
	struct list_head  node;
};

/**
 *  @brief
 *    Received CI data.
 */
struct ci_rx_data_s {
	wait_queue_head_t  syncWait;
	__u8               syncSignal;
	__u8               syncData[DEVICE_MESSAGE_LENGTH];
	__u32              syncDataSize;
	wait_queue_head_t  asyncWait;
	struct list_head   asyncDataList;
	__u8               bPendingSend;
};

/**
 *  @brief
 *    CI bulk channel.
 */
struct ci_bulk_s {
	__u8               counter;
	__u16              inMaxPacketSize;
	__u16              outMaxPacketSize;
	struct urb         *intUrb[DEVICE_NUM_INT_IN_URBS];
	spinlock_t         intLock;
	spinlock_t         intUrbLock;
	__u8               intCurrStatus;
	__u8               intCurrIndex;
	__u16              intSizeToReceive;
	struct ci_rx_data_s       ciData[DEVICE_NUM_CAM];
};

/**
 *  @brief
 *    TS channel (can use isoc or bulk interface).
 * x
 */
struct ts_channel_s {
	spinlock_t        inLock;
	wait_queue_head_t inWait;
	struct video_buf_s       inVb;
	int               syncOffset;
	int               prevOffset;
	__u8              lastPacket[DEVICE_MPEG2_PACKET_SIZE];
	__u8              lastPacketSize;
	spinlock_t        outLock;
	__u8              nextFreeOutUrbIndex;
	atomic_t          numOutUrbs;
	__u8              outStop;
	__u16             maxPacketSize;
	/* isochronous urbs */
	struct urb        *isocInUrb[DEVICE_NUM_ISOC_IN_URBS];
	struct urb        *isocOutUrb[DEVICE_NUM_ISOC_OUT_URBS];
	/* bulk urbs */
	struct urb       *bulkInUrb;
	int              nbByteSend;
	int              nbByteRead;
	__u8             FirstTransfer;

#ifdef DEBUG_BITRATE
	ktime_t bitrateTime
#endif
};

struct device_s {
	struct mutex       lock;
	struct usb_device  *usbdev;
	__u8               opened;
	__u8               askToRelease;
	__u8               askToSuspend;
	struct ci_bulk_s          ciBulk;
	__u8               useIsoc;
	struct ts_channel_s       channel[DEVICE_NUM_CAM];
	/* bus adapter private ops callback */
	struct cimaxusb_priv_ops_t *ops;
	int                ref;
};

struct bulk_timer_s {
	struct device_s *device;
	__u8      index;
	struct timer_list StartBulkReadTimer;
};

int cimax_usb_select_interface(struct device_s *device, unsigned long intf);
int cimax_usb_ci_write(struct device_s *device,
		u8 *txData, int txSize, u8 *rxData, int rxSize);
int cimax_usb_ci_read_evt(struct device_s *device,
		int moduleId, u8 *buf, int size);

int cimax_usb_device_unlock_read(struct device_s *device);
int cimax_usb_device_open(struct device_s *device);
int cimax_usb_device_close(struct device_s *device);
void cimax_usb_set_cb(void *cb1, void *cb2);


#endif
#endif
