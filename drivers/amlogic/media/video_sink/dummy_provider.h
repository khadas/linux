/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef VIDEO_PROVIDER_HEADER_HH
#define VIDEO_PROVIDER_HEADER_HH

extern unsigned int dummy_video_log_level;

#define VP_VFM_POOL_SIZE 3
#define VP_LEVEL_DEBUG   1
#define VP_LEVEL_DEBUG2  2

#define VIDEO_PROVIDER_NAME "dummy_video"
#define VP_VFPATH_ID    "dummy_video"

#define VP_VFPATH_CHAIN "dummy_video amvideo"

#undef pr_fmt
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt
#define vp_info(fmt, ...) \
	pr_info(fmt, ##__VA_ARGS__)

#define vp_err(fmt, ...) \
	pr_err(fmt, ##__VA_ARGS__)

#define vp_dbg(fmt, ...) \
	do { \
		if (dummy_video_log_level >= VP_LEVEL_DEBUG) { \
			pr_info(fmt, ##__VA_ARGS__); \
		} \
	} while (0)

#define vp_dbg2(fmt, ...) \
	do { \
		if (dummy_video_log_level >= VP_LEVEL_DEBUG2) { \
			pr_info(fmt, ##__VA_ARGS__); \
		} \
	} while (0)

enum {
	VP_FMT_NV21,
	VP_FMT_NV12,
	VP_FMT_RGB888,
	VP_FMT_YUV444_PACKED
};

enum {
	VP_MEM_ION,
	VP_MEM_DMABUF,
};

enum {
	VP_BIG_ENDIAN,
	VP_LITTLE_ENDIAN
};

struct video_provider_device_s {
	char name[20];
	int major;
	struct class *cla;
	struct device *dev;
	const struct vinfo_s *vinfo;
};

struct vp_frame_s {
	int shared_fd;
	int mem_type;
	int width;  /* frame width */
	int height; /* frame height */
	int format;
	int endian;
};

struct vp_pool_s {
	struct vframe_s vfm;
	int used;
};

#define VIDEO_PROVIDER_IOC_MAGIC  'V'
#define VIDEO_PROVIDER_IOCTL_RENDER   _IOW(VIDEO_PROVIDER_IOC_MAGIC, 0x00, \
						struct vp_frame_s)
#define VIDEO_PROVIDER_IOCTL_POST     _IO(VIDEO_PROVIDER_IOC_MAGIC, 0x01)

#endif
