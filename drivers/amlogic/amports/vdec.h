/*
 * drivers/amlogic/amports/vdec.h
 *
 * Copyright (C) 2015 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
*/

#ifndef VDEC_H
#define VDEC_H
#include "amports_config.h"
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/list.h>
#include <linux/completion.h>
#include <linux/irqreturn.h>

#include <linux/amlogic/amports/amstream.h>
#include <linux/amlogic/amports/vframe.h>
#include <linux/amlogic/amports/vframe_provider.h>
#include <linux/amlogic/amports/vframe_receiver.h>

#include "vdec_input.h"

s32 vdec_dev_register(void);
s32 vdec_dev_unregister(void);



int vdec_source_changed(int format, int width, int height, int fps);
int vdec2_source_changed(int format, int width, int height, int fps);
int hevc_source_changed(int format, int width, int height, int fps);


#define DEC_FLAG_HEVC_WORKAROUND 0x01

enum vdec_type_e {
	VDEC_1 = 0,
	VDEC_HCODEC,
	VDEC_2,
	VDEC_HEVC,
	VDEC_MAX
};

extern void vdec2_power_mode(int level);
extern void vdec_poweron(enum vdec_type_e core);
extern void vdec_poweroff(enum vdec_type_e core);
extern bool vdec_on(enum vdec_type_e core);

/*irq num as same as .dts*/
/*
	interrupts = <0 3 1
		0 23 1
		0 32 1
		0 43 1
		0 44 1
		0 45 1>;
	interrupt-names = "vsync",
		"demux",
		"parser",
		"mailbox_0",
		"mailbox_1",
		"mailbox_2";
*/
enum vdec_irq_num {
	VSYNC_IRQ = 0,
	DEMUX_IRQ,
	PARSER_IRQ,
	VDEC_IRQ_0,
	VDEC_IRQ_1,
	VDEC_IRQ_2,
	VDEC_IRQ_MAX,
};
extern s32 vdec_request_threaded_irq(enum vdec_irq_num num,
			irq_handler_t handler,
			irq_handler_t thread_fn,
			unsigned long irqflags,
			const char *devname, void *dev);
extern s32 vdec_request_irq(enum vdec_irq_num num, irq_handler_t handler,
	const char *devname, void *dev);
extern void vdec_free_irq(enum vdec_irq_num num, void *dev);

extern void dma_contiguous_early_fixup(phys_addr_t base, unsigned long size);
unsigned int get_vdec_clk_config_settings(void);
void update_vdec_clk_config_settings(unsigned int config);
unsigned int  get_mmu_mode(void);


struct vdec_s;
enum vformat_t;

/* stream based with single instance decoder driver */
#define VDEC_TYPE_SINGLE           0
/* stream based with multi-instance decoder with HW resouce sharing */
#define VDEC_TYPE_STREAM_PARSER    1
/* frame based with multi-instance decoder, input block list based */
#define VDEC_TYPE_FRAME_BLOCK      2
/* frame based with multi-instance decoder, single circular input block */
#define VDEC_TYPE_FRAME_CIRCULAR   3

/* decoder status: uninitialized */
#define VDEC_STATUS_UNINITIALIZED  0
/* decoder status: before the decoder can start consuming data */
#define VDEC_STATUS_DISCONNECTED   1
/* decoder status: decoder should become disconnected once it's not active */
#define VDEC_STATUS_CONNECTED      2
/* decoder status: decoder owns HW resource and is running */
#define VDEC_STATUS_ACTIVE         3

#define VDEC_PROVIDER_NAME_SIZE 16
#define VDEC_RECEIVER_NAME_SIZE 16
#define VDEC_MAP_NAME_SIZE      40

#define VDEC_FLAG_OTHER_INPUT_CONTEXT 0x0
#define VDEC_FLAG_SELF_INPUT_CONTEXT 0x01

struct vdec_s {
	u32 magic;
	struct list_head list;
	int id;

	struct vdec_s *master;
	struct vdec_s *slave;
	struct stream_port_s *port;
	int status;
	int next_status;
	int type;
	int port_flag;
	int format;
	u32 pts;
	u64 pts64;
	bool pts_valid;
	int flag;
	int sched;

	struct completion inactive_done;

	/* config (temp) */
	unsigned long mem_start;
	unsigned long mem_end;
	unsigned int alloc_mem_size;

	struct device *cma_dev;
	struct platform_device *dev;
	struct dec_sysinfo sys_info_store;
	struct dec_sysinfo *sys_info;

	/* input */
	struct vdec_input_s input;

	/* mc cache */
	u32 mc[4096 * 4];
	bool mc_loaded;

	/* frame provider/receiver interface */
	char vf_provider_name[VDEC_PROVIDER_NAME_SIZE];
	struct vframe_provider_s vframe_provider;
	char *vf_receiver_name;
	char vfm_map_id[VDEC_MAP_NAME_SIZE];
	char vfm_map_chain[VDEC_MAP_NAME_SIZE];
	int vf_receiver_inst;
	enum FRAME_BASE_VIDEO_PATH frame_base_video_path;
	bool use_vfm_path;
	char config[PAGE_SIZE];
	int config_len;

	/* canvas */
	int (*get_canvas)(unsigned int index, unsigned int base);

	int (*dec_status)(struct vdec_s *vdec, struct vdec_info *vstatus);
	int (*set_trickmode)(struct vdec_s *vdec, unsigned long trickmode);

	bool (*run_ready)(struct vdec_s *vdec);
	void (*run)(struct vdec_s *vdec,
			void (*callback)(struct vdec_s *, void *), void *);
	void (*reset)(struct vdec_s *vdec);
	irqreturn_t (*irq_handler)(struct vdec_s *);
	irqreturn_t (*threaded_irq_handler)(struct vdec_s *);

	/* private */
	void *private;       /* decoder per instance specific data */
};

/* common decoder vframe provider name to use default vfm path */
#define VFM_DEC_PROVIDER_NAME "decoder"
#define VFM_DEC_DVBL_PROVIDER_NAME "dvbldec"
#define VFM_DEC_DVEL_PROVIDER_NAME "dveldec"

#define hw_to_vdec(hw) ((struct vdec_s *) \
	(platform_get_drvdata(hw->platform_dev)))

#define canvas_y(canvas) ((canvas) & 0xff)
#define canvas_u(canvas) (((canvas) >> 8) & 0xff)
#define canvas_v(canvas) (((canvas) >> 16) & 0xff)
#define canvas_y2(canvas) (((canvas) >> 16) & 0xff)
#define canvas_u2(canvas) (((canvas) >> 24) & 0xff)

#define vdec_frame_based(vdec) \
	(((vdec)->type == VDEC_TYPE_FRAME_BLOCK) || \
	 ((vdec)->type == VDEC_TYPE_FRAME_CIRCULAR))
#define vdec_stream_based(vdec) \
	(((vdec)->type == VDEC_TYPE_STREAM_PARSER) || \
	 ((vdec)->type == VDEC_TYPE_SINGLE))
#define vdec_single(vdec) \
	((vdec)->type == VDEC_TYPE_SINGLE)
#define vdec_dual(vdec) \
	(((vdec)->port->type & PORT_TYPE_DUALDEC) ||\
	 (amports_get_debug_flags() & 0x100))
#define vdec_secure(vdec) \
	(((vdec)->port_flag & PORT_FLAG_DRM))

/* construct vdec strcture */
extern struct vdec_s *vdec_create(struct stream_port_s *port,
				struct vdec_s *master);

/* set video format */
extern int vdec_set_format(struct vdec_s *vdec, int format);

/* set PTS */
extern int vdec_set_pts(struct vdec_s *vdec, u32 pts);

extern int vdec_set_pts64(struct vdec_s *vdec, u64 pts64);

/* set vfm map when use frame base decoder */
extern int vdec_set_video_path(struct vdec_s *vdec, int video_path);

/* add frame data to input chain */
extern int vdec_write_vframe(struct vdec_s *vdec, const char *buf,
				size_t count);

/* mark the vframe_chunk as consumed */
extern void vdec_vframe_dirty(struct vdec_s *vdec,
				struct vframe_chunk_s *chunk);

/* prepare decoder input */
extern int vdec_prepare_input(struct vdec_s *vdec, struct vframe_chunk_s **p);

/* clean decoder input */
extern void vdec_clean_input(struct vdec_s *vdec);

/* enable decoder input */
extern void vdec_enable_input(struct vdec_s *vdec);

/* set decoder input prepare level */
extern void vdec_set_prepare_level(struct vdec_s *vdec, int level);

/* set vdec input */
extern int vdec_set_input_buffer(struct vdec_s *vdec, u32 start, u32 size);

/* check if decoder can get more input */
extern bool vdec_has_more_input(struct vdec_s *vdec);

/* allocate input chain
 * register vdec_device
 * create output, vfm or create ionvideo output
 * insert vdec to vdec_manager for scheduling
 */
extern int vdec_connect(struct vdec_s *vdec);

/* remove vdec from vdec_manager scheduling
 * release input chain
 * disconnect video output from ionvideo
 */
extern int vdec_disconnect(struct vdec_s *vdec);

/* release vdec structure */
extern int vdec_destroy(struct vdec_s *vdec);

/* reset vdec */
extern int vdec_reset(struct vdec_s *vdec);

extern void vdec_set_status(struct vdec_s *vdec, int status);

extern void vdec_set_next_status(struct vdec_s *vdec, int status);

extern int vdec_set_decinfo(struct vdec_s *vdec, struct dec_sysinfo *p);

extern int vdec_init(struct vdec_s *vdec, int is_4k);

extern void vdec_release(struct vdec_s *vdec);

extern int vdec_status(struct vdec_s *vdec, struct vdec_info *vstatus);

extern int vdec_set_trickmode(struct vdec_s *vdec, unsigned long trickmode);

extern void vdec_set_flag(struct vdec_s *vdec, u32 flag);

extern void vdec_set_eos(struct vdec_s *vdec, bool eos);

extern void vdec_set_next_sched(struct vdec_s *vdec, struct vdec_s *next_vdec);

extern const char *vdec_status_str(struct vdec_s *vdec);

extern const char *vdec_type_str(struct vdec_s *vdec);

extern const char *vdec_device_name_str(struct vdec_s *vdec);

extern void  vdec_count_info(struct vdec_info *vs, unsigned int err,
	unsigned int offset);

#endif				/* VDEC_H */
