#ifndef VFM_GRABBER_H
#define VFM_GRABBER_H

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>

#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/vfm/vframe_receiver.h>

#include <linux/dma-buf.h>
#include <linux/scatterlist.h>

#include "linux/time.h"

typedef struct
{
	struct class *device_class;
	struct device *file_device;
	struct vframe_receiver_s vfm_vf_receiver;
	int ready_count;
	//struct timeval starttime;
	int device_major;
	//vfm_grabber_buffer buffer[MAX_DMABUF_FD];
	//vfm_grabber_info info;

} vfm_grabber_dev;



// -------
// New API
//--------

// User space version of vframe_s in linux/amlogic/amports/vframe.h
typedef struct
{
	u32 index;
	ulong addr;
	u32 width;
	u32 height;
} vfm_grabber_canvasinfo;

typedef struct
{
	u32 index;
	u32 type;
	u32 duration;
	u32 duration_pulldown;
	u32 pts;
	u64 pts_us64;
	u32 flag;

	u32 canvas0Addr;
	u32 canvas1Addr;

	u32 bufWidth;
	u32 width;
	u32 height;
	u32 ratio_control;
	u32 bitdepth;

	vfm_grabber_canvasinfo canvas0plane0;
	vfm_grabber_canvasinfo canvas0plane1;
	vfm_grabber_canvasinfo canvas0plane2;

} vfm_grabber_frameinfo;

typedef struct
{
	struct device* dev;
	struct vframe_s* vf;
} vfm_grabber_dev_private;


// IOCTL defines
#define VFM_GRABBER_GRAB_FRAME			_IOR('V', 0x10, vfm_grabber_frameinfo)
#define VFM_GRABBER_HINT_INVALIDATE		_IO('V', 0x11)

#endif // VFM_GRABBER_H
