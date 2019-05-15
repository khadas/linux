/*
 * drivers/amlogic/media/frame_provider/decoder/utils/vdec.h
 *
 * Copyright (C) 2016 Amlogic, Inc. All rights reserved.
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
#include <linux/amlogic/media/utils/amports_config.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/list.h>
#include <linux/completion.h>
#include <linux/irqreturn.h>

#include <linux/amlogic/media/utils/amstream.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/vfm/vframe_receiver.h>

/*#define CONFIG_AM_VDEC_DV*/

#include "vdec_input.h"
#include "frame_check.h"

s32 vdec_dev_register(void);
s32 vdec_dev_unregister(void);

int vdec_source_changed(int format, int width, int height, int fps);
int vdec2_source_changed(int format, int width, int height, int fps);
int hevc_source_changed(int format, int width, int height, int fps);
struct device *get_vdec_device(void);
int vdec_module_init(void);
void vdec_module_exit(void);

#define MAX_INSTANCE_MUN  9

#define VDEC_DEBUG_SUPPORT

#define DEC_FLAG_HEVC_WORKAROUND 0x01

#define VDEC_FIFO_ALIGN 8

enum vdec_type_e {
	VDEC_1 = 0,
	VDEC_HCODEC,
	VDEC_2,
	VDEC_HEVC,
	VDEC_HEVCB,
	VDEC_MAX
};

#define CORE_MASK_VDEC_1 (1 << VDEC_1)
#define CORE_MASK_HCODEC (1 << VDEC_HCODEC)
#define CORE_MASK_VDEC_2 (1 << VDEC_2)
#define CORE_MASK_HEVC (1 << VDEC_HEVC)
#define CORE_MASK_HEVC_FRONT (1 << VDEC_HEVC)
#define CORE_MASK_HEVC_BACK (1 << VDEC_HEVCB)
#define CORE_MASK_COMBINE (1UL << 31)

extern void vdec2_power_mode(int level);
extern void vdec_poweron(enum vdec_type_e core);
extern void vdec_poweroff(enum vdec_type_e core);
extern bool vdec_on(enum vdec_type_e core);
extern void vdec_power_reset(void);

/*irq num as same as .dts*/

/*
 *	interrupts = <0 3 1
 *		0 23 1
 *		0 32 1
 *		0 43 1
 *		0 44 1
 *		0 45 1>;
 *	interrupt-names = "vsync",
 *		"demux",
 *		"parser",
 *		"mailbox_0",
 *		"mailbox_1",
 *		"mailbox_2";
 */
enum vdec_irq_num {
	VSYNC_IRQ = 0,
	DEMUX_IRQ,
	PARSER_IRQ,
	VDEC_IRQ_0,
	VDEC_IRQ_1,
	VDEC_IRQ_2,
	VDEC_IRQ_HEVC_BACK,
	VDEC_IRQ_MAX,
};

enum vdec_fr_hint_state {
	VDEC_NO_NEED_HINT = 0,
	VDEC_NEED_HINT,
	VDEC_HINTED,
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
//unsigned int get_mmu_mode(void);//DEBUG_TMP

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
#define VDEC_MAP_NAME_SIZE      90

#define VDEC_FLAG_OTHER_INPUT_CONTEXT 0x0
#define VDEC_FLAG_SELF_INPUT_CONTEXT 0x01

#define VDEC_NEED_MORE_DATA_RUN   0x01
#define VDEC_NEED_MORE_DATA_DIRTY 0x02
#define VDEC_NEED_MORE_DATA       0x04

struct vdec_s {
	u32 magic;
	struct list_head list;
	unsigned long core_mask;
	unsigned long active_mask;
	unsigned long sched_mask;
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
	u64 timestamp;
	bool timestamp_valid;
	int flag;
	int sched;
	int need_more_data;
	u32 canvas_mode;

	struct completion inactive_done;

	/* config (temp) */
	unsigned long mem_start;
	unsigned long mem_end;

	void *mm_blk_handle;

	struct device *cma_dev;
	struct platform_device *dev;
	struct dec_sysinfo sys_info_store;
	struct dec_sysinfo *sys_info;

	/* input */
	struct vdec_input_s input;

	struct pic_check_mgr_t vfc;

	/* mc cache */
	u32 mc[4096 * 4];
	bool mc_loaded;
	u32 mc_type;
	/* frame provider/receiver interface */
	char vf_provider_name[VDEC_PROVIDER_NAME_SIZE];
	struct vframe_provider_s vframe_provider;
	char *vf_receiver_name;
	char vfm_map_id[VDEC_MAP_NAME_SIZE];
	char vfm_map_chain[VDEC_MAP_NAME_SIZE];
	int vf_receiver_inst;
	enum FRAME_BASE_VIDEO_PATH frame_base_video_path;
	enum vdec_fr_hint_state fr_hint_state;
	bool use_vfm_path;
	char config[PAGE_SIZE];
	int config_len;
	bool is_reset;
	bool dolby_meta_with_el;

	/* canvas */
	int (*get_canvas)(unsigned int index, unsigned int base);
	int (*get_canvas_ex)(int type, int id);
	void (*free_canvas_ex)(int index, int id);

	int (*dec_status)(struct vdec_s *vdec, struct vdec_info *vstatus);
	int (*set_trickmode)(struct vdec_s *vdec, unsigned long trickmode);
	int (*set_isreset)(struct vdec_s *vdec, int isreset);
	void (*vdec_fps_detec)(int id);

	unsigned long (*run_ready)(struct vdec_s *vdec, unsigned long mask);
	void (*run)(struct vdec_s *vdec, unsigned long mask,
			void (*callback)(struct vdec_s *, void *), void *);
	void (*reset)(struct vdec_s *vdec);
	void (*dump_state)(struct vdec_s *vdec);
	irqreturn_t (*irq_handler)(struct vdec_s *vdec, int irq);
	irqreturn_t (*threaded_irq_handler)(struct vdec_s *vdec, int irq);

	int (*user_data_read)(struct vdec_s *vdec,
			struct userdata_param_t *puserdata_para);
	void (*reset_userdata_fifo)(struct vdec_s *vdec, int bInit);
	void (*wakeup_userdata_poll)(struct vdec_s *vdec);
	/* private */
	void *private;       /* decoder per instance specific data */
#ifdef VDEC_DEBUG_SUPPORT
	u64 profile_start_clk[VDEC_MAX];
	u64 total_clk[VDEC_MAX];
	u32 check_count[VDEC_MAX];
	u32 not_run_ready_count[VDEC_MAX];
	u32 input_underrun_count[VDEC_MAX];
	u32 run_count[VDEC_MAX];
	u64 run_clk[VDEC_MAX];
	u64 start_run_clk[VDEC_MAX];
#endif
	atomic_t inirq_thread_flag;
	atomic_t inirq_flag;
	int parallel_dec;
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
	 (vdec_get_debug_flags() & 0x100))
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

/* set receive id when receive is ionvideo or amlvideo */
extern int vdec_set_receive_id(struct vdec_s *vdec, int receive_id);

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

/* sync decoder input */
extern int vdec_sync_input(struct vdec_s *vdec);

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

extern int vdec_set_isreset(struct vdec_s *vdec, int isreset);

extern int vdec_set_dv_metawithel(struct vdec_s *vdec, int isdvmetawithel);

extern void vdec_set_no_powerdown(int flag);

extern int vdec_is_support_4k(void);
extern void vdec_set_flag(struct vdec_s *vdec, u32 flag);

extern void vdec_set_eos(struct vdec_s *vdec, bool eos);

extern void vdec_set_next_sched(struct vdec_s *vdec, struct vdec_s *next_vdec);

extern const char *vdec_status_str(struct vdec_s *vdec);

extern const char *vdec_type_str(struct vdec_s *vdec);

extern const char *vdec_device_name_str(struct vdec_s *vdec);

extern void vdec_schedule_work(struct work_struct *work);

extern void  vdec_count_info(struct vdec_info *vs, unsigned int err,
	unsigned int offset);

extern bool vdec_need_more_data(struct vdec_s *vdec);

extern void vdec_reset_core(struct vdec_s *vdec);

extern void hevc_reset_core(struct vdec_s *vdec);

extern void vdec_set_suspend_clk(int mode, int hevc);

extern unsigned long vdec_ready_to_run(struct vdec_s *vdec, unsigned long mask);

extern void vdec_prepare_run(struct vdec_s *vdec, unsigned long mask);

extern void vdec_core_request(struct vdec_s *vdec, unsigned long mask);

extern int vdec_core_release(struct vdec_s *vdec, unsigned long mask);

extern bool vdec_core_with_input(unsigned long mask);

extern void vdec_core_finish_run(struct vdec_s *vdec, unsigned long mask);

#ifdef VDEC_DEBUG_SUPPORT
extern void vdec_set_step_mode(void);
#endif

extern void vdec_disable_DMC(struct vdec_s *vdec);
extern void vdec_enable_DMC(struct vdec_s *vdec);

int vdec_read_user_data(struct vdec_s *vdec,
				struct userdata_param_t *p_userdata_param);

int vdec_wakeup_userdata_poll(struct vdec_s *vdec);

void vdec_reset_userdata_fifo(struct vdec_s *vdec, int bInit);

struct vdec_s *vdec_get_vdec_by_id(int vdec_id);

#ifdef VDEC_DEBUG_SUPPORT
extern void vdec_set_step_mode(void);
#endif
int vdec_get_debug_flags(void);

unsigned char is_mult_inc(unsigned int);

int vdec_get_status(struct vdec_s *vdec);

void vdec_set_timestamp(struct vdec_s *vdec, u64 timestamp);

struct vdec_s *vdec_get_with_id(unsigned id);

void *vdec_get_active_vfc(int core_mask);

#endif				/* VDEC_H */
