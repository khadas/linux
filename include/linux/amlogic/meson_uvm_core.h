/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __MESON_UVM_H
#define __MESON_UVM_H

#include <linux/device.h>
#include <linux/dma-direction.h>
#include <linux/miscdevice.h>
#include <linux/kref.h>
#include <linux/mm_types.h>
#include <linux/mutex.h>
#include <linux/rbtree.h>
#include <linux/sched.h>
#include <linux/ioctl.h>
#include <linux/types.h>

#include <linux/amlogic/media/vfm/vframe.h>

#define MAX_BUFF_NUMS	4
#define UVM_FAKE_SIZE 4096

enum uvm_alloc_flag {
	UVM_IMM_ALLOC,
	UVM_DELAY_ALLOC,
	UVM_FAKE_ALLOC,
	UVM_SECURE_ALLOC,
	UVM_SKIP_REALLOC,
	UVM_USAGE_CACHED,
	UVM_DETACH_FLAG = 30
};

enum uvm_debug_mask {
	UVM_DEBUG_LEVEL_ERROR,
	UVM_DEBUG_LEVEL_INFO,
	UVM_DEBUG_LEVEL_DBG
};

#define UVM_ERROR       BIT(UVM_DEBUG_LEVEL_ERROR)
#define UVM_INFO        BIT(UVM_DEBUG_LEVEL_INFO)
#define UVM_DBG         BIT(UVM_DEBUG_LEVEL_DBG)

/**
 * struct uvm_handle - video dmabuffer wrap vframe
 *
 * @ref:	reference count
 * @lock:	protects the uvm_handle fields
 * @uvm_alloc:	buffer alloc info
 * @dma_buf:	dmabuf structure
 * @alloc:		non-alloc or allocated
 * @size:		dmabuf size
 * @align:		dmabuf align size
 * @flags:		alloc flag and usage flag
 * @vf:			builtin vframe
 * @vfp:		pointer the origin vframe
 * @mod_attached:	list of attached module
 * @n_mod_attached:	num of attached module
 * @mod_attached_mask:	mask of attached module
 * @usage:	    buffer usage
 */
struct uvm_handle {
	struct kref ref;
	/* protects the uvm_handle fields */
	struct mutex lock;
	/* protects the whole operation of detaching hook mod */
	struct mutex detachlock;

	struct uvm_alloc *ua;
	struct dma_buf *dmabuf;
	bool alloc;
	size_t size;
	size_t align;
	unsigned long flags;

	struct vframe_s vf;
	struct vframe_s *vfp;

	struct list_head mod_attached;
	size_t n_mod_attached;
	unsigned long mod_attached_mask;
	size_t usage;
};

/**
 * struct uvm_buf_obj - uvm buffer object
 * This structure define the uvm buffer object. uvm do not allocate buffer.
 * device that allocate buffer, wrap this structure.
 *
 * @arg:	uvm dmabuf exporter device priv structure
 * @dev:	uvm dmabuf exporter device
 */
struct uvm_buf_obj {
	void *arg;
	struct device *dev;
	struct dma_buf *dmabuf;
};

/**
 * struct uvm_alloc - uvm buffer alloc struct
 *
 * @sgt:	dmabuf sg_table
 * @pages:	dmabuf page array
 * @vaddr:	dmabuf kernel address
 * @size:	dmabuf real size
 * @obj:	uvm buffer object
 * @free:	free uvm_buf_obj related
 */
struct uvm_alloc {
	struct sg_table *sgt;
	struct page **pages;
	void *vaddr;
	size_t size;
	u64 flags;
	int scalar;
	struct uvm_buf_obj *obj;
	void (*free)(struct uvm_buf_obj *obj);
	int (*delay_alloc)(struct dma_buf *dmabuf, struct uvm_buf_obj *obj);
	int (*gpu_realloc)(struct dma_buf *dmabuf, struct uvm_buf_obj *obj, int scalar);
};

struct uvm_alloc_info {
	size_t size;
	u64 flags;
	int scalar;
	struct sg_table *sgt;
	struct uvm_buf_obj *obj;
	void (*free)(struct uvm_buf_obj *obj);
	int (*delay_alloc)(struct dma_buf *dmabuf, struct uvm_buf_obj *obj);
	int (*gpu_realloc)(struct dma_buf *dmabuf, struct uvm_buf_obj *obj, int scalar);
};

/**
 * please sync the file in hwcomposer
 * if you change uvm_hook_mod_type.
 * display/include/UvmDev.h
 */
enum uvm_hook_mod_type {
	VF_SRC_DECODER,
	VF_SRC_VDIN,
	VF_PROCESS_V4LVIDEO,
	VF_PROCESS_DI,
	VF_PROCESS_VIDEOCOMPOSER,
	VF_PROCESS_DECODER,
	PROCESS_NN,
	PROCESS_GRALLOC,
	PROCESS_AIPQ,
	PROCESS_DALTON,
	PROCESS_AIFACE,
	PROCESS_AICOLOR,
	PROCESS_HWC,
	PROCESS_INVALID
};

enum uvm_data_usage {
	UVM_USAGE_VIDEO_PLAY,
	UVM_USAGE_IMAGE_PLAY,
	UVM_USAGE_INVALID
};

/**
 * struct uvm_hook_mod - uvm hook module
 *
 * @ref:	reference count
 * @type:	module type
 * @arg:	module private data
 * @free:	module free function
 * @acquire_fence:	module acquire fence
 * @head:	module link node
 */
struct uvm_hook_mod {
	struct kref ref;
	enum uvm_hook_mod_type type;
	void *arg;
	void (*free)(void *arg);
	int (*getinfo)(void *arg, char *buf);
	int (*setinfo)(void *arg, char *buf);
	struct sync_fence *acquire_fence;
	struct list_head list;
};

struct uvm_hook_mod_info {
	enum uvm_hook_mod_type type;
	void *arg;
	void (*free)(void *arg);
	int (*getinfo)(void *arg, char *buf);
	int (*setinfo)(void *arg, char *buf);
	struct sync_fence *acquire_fence;
};

struct dma_buf *uvm_alloc_dmabuf(size_t len, size_t align, ulong flags);

bool dmabuf_is_uvm(struct dma_buf *dmabuf);
bool dmabuf_uvm_realloc(struct dma_buf *dmabuf);

struct uvm_buf_obj *dmabuf_get_uvm_buf_obj(struct dma_buf *dmabuf);

int dmabuf_bind_uvm_alloc(struct dma_buf *dmabuf, struct uvm_alloc_info *info);
int dmabuf_bind_uvm_delay_alloc(struct dma_buf *dmabuf,
				struct uvm_alloc_info *info);

/**
 * uvm vframe interface
 */
int dmabuf_set_vframe(struct dma_buf *dmabuf, struct vframe_s *vf,
		      enum uvm_hook_mod_type type);

struct vframe_s *dmabuf_get_vframe(struct dma_buf *dmabuf);

int dmabuf_put_vframe(struct dma_buf *dmabuf);

/**
 * uvm hook module interface
 */
int uvm_attach_hook_mod(struct dma_buf *dmabuf,
			struct uvm_hook_mod_info *info);
int uvm_detach_hook_mod(struct dma_buf *dmabuf,
			int type);

int meson_uvm_getinfo(struct dma_buf *dmabuf,
			int mode_type, char *buf);
int meson_uvm_setinfo(struct dma_buf *dmabuf,
			int mode_type, char *buf);

int meson_uvm_get_usage(struct dma_buf *dmabuf, size_t *usage);
int meson_uvm_set_usage(struct dma_buf *dmabuf, size_t usage);

struct uvm_hook_mod *uvm_get_hook_mod(struct dma_buf *dmabuf,
				      int type);

int uvm_put_hook_mod(struct dma_buf *dmabuf, int type);

bool is_valid_mod_type(struct dma_buf *dmabuf,
		       enum uvm_hook_mod_type type);

#endif
