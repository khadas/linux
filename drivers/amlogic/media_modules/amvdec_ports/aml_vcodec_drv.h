/*
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Description:
 */
#ifndef _AML_VCODEC_DRV_H_
#define _AML_VCODEC_DRV_H_

#include <linux/kref.h>
#include <linux/platform_device.h>
#include <linux/videodev2.h>
#include <linux/kfifo.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-core.h>
#include <media/videobuf2-v4l2.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/video_sink/v4lvideo_ext.h>
#include "aml_vcodec_util.h"
#include "aml_vcodec_dec.h"

#define AML_VCODEC_DRV_NAME	"aml_vcodec_drv"
#define AML_VCODEC_DEC_NAME	"aml-vcodec-dec"
#define AML_VCODEC_ENC_NAME	"aml-vcodec-enc"
#define AML_PLATFORM_STR	"platform:amlogic"

#define AML_VCODEC_MAX_PLANES	3
#define AML_V4L2_BENCHMARK	0
#define WAIT_INTR_TIMEOUT_MS	1000

/* codec types of get/set parms. */
#define V4L2_CONFIG_PARM_ENCODE		(0)
#define V4L2_CONFIG_PARM_DECODE		(1)

/* types of decode parms. */
#define V4L2_CONFIG_PARM_DECODE_CFGINFO	(1 << 0)
#define V4L2_CONFIG_PARM_DECODE_PSINFO	(1 << 1)
#define V4L2_CONFIG_PARM_DECODE_HDRINFO	(1 << 2)
#define V4L2_CONFIG_PARM_DECODE_CNTINFO	(1 << 3)

/* amlogic event define. */
/* #define V4L2_EVENT_SRC_CH_RESOLUTION	(1 << 0) */
#define V4L2_EVENT_SRC_CH_HDRINFO	(1 << 1)
#define V4L2_EVENT_SRC_CH_PSINFO	(1 << 2)
#define V4L2_EVENT_SRC_CH_CNTINFO	(1 << 3)

/* exception handing */
#define V4L2_EVENT_REQUEST_RESET	(1 << 8)
#define V4L2_EVENT_REQUEST_EXIT		(1 << 9)
#define V4L2_EVENT_SEND_ERROR		(1 << 10)

/* eos event */
#define V4L2_EVENT_SEND_EOS		(1 << 16)

/* v4l buffer pool */
#define V4L_CAP_BUFF_MAX		(32)
#define V4L_CAP_BUFF_INVALID		(0)
#define V4L_CAP_BUFF_IN_M2M		(1)
#define V4L_CAP_BUFF_IN_DEC		(2)
#define V4L_CAP_BUFF_IN_VPP		(3)
#define V4L_CAP_BUFF_IN_GE2D		(4)

/* v4l reset mode */
#define V4L_RESET_MODE_NORMAL		(1 << 0) /* reset vdec_input and decoder. */
#define V4L_RESET_MODE_LIGHT		(1 << 1) /* just only reset decoder. */

/* m2m job queue's status */
/* Instance is already queued on the job_queue */
#define TRANS_QUEUED		(1 << 0)
/* Instance is currently running in hardware */
#define TRANS_RUNNING		(1 << 1)
/* Instance is currently aborting */
#define TRANS_ABORT		(1 << 2)

#define CTX_BUF_TOTAL(ctx) (ctx->dpb_size + ctx->vpp_size + ctx->ge2d_size)
/**
 * enum aml_hw_reg_idx - AML hw register base index
 */
enum aml_hw_reg_idx {
	VDEC_SYS,
	VDEC_MISC,
	VDEC_LD,
	VDEC_TOP,
	VDEC_CM,
	VDEC_AD,
	VDEC_AV,
	VDEC_PP,
	VDEC_HWD,
	VDEC_HWQ,
	VDEC_HWB,
	VDEC_HWG,
	NUM_MAX_VDEC_REG_BASE,
	/* h264 encoder */
	VENC_SYS = NUM_MAX_VDEC_REG_BASE,
	/* vp8 encoder */
	VENC_LT_SYS,
	NUM_MAX_VCODEC_REG_BASE
};

/**
 * enum aml_instance_type - The type of an AML Vcodec instance.
 */
enum aml_instance_type {
	AML_INST_DECODER		= 0,
	AML_INST_ENCODER		= 1,
};

/**
 * enum aml_instance_state - The state of an AML Vcodec instance.
 * @AML_STATE_IDLE	- default state when instance is created
 * @AML_STATE_INIT	- vcodec instance is initialized
 * @AML_STATE_PROBE	- vdec/venc had sps/pps header parsed/encoded
 * @AML_STATE_ACTIVE	- vdec is ready for work.
 * @AML_STATE_FLUSHING	- vdec is flushing. Only used by decoder
 * @AML_STATE_FLUSHED	- decoder has transacted the last frame.
 * @AML_STATE_ABORT	- vcodec should be aborted
 */
enum aml_instance_state {
	AML_STATE_IDLE,
	AML_STATE_INIT,
	AML_STATE_PROBE,
	AML_STATE_READY,
	AML_STATE_ACTIVE,
	AML_STATE_FLUSHING,
	AML_STATE_FLUSHED,
	AML_STATE_ABORT,
};

/**
 * struct aml_encode_param - General encoding parameters type
 */
enum aml_encode_param {
	AML_ENCODE_PARAM_NONE = 0,
	AML_ENCODE_PARAM_BITRATE = (1 << 0),
	AML_ENCODE_PARAM_FRAMERATE = (1 << 1),
	AML_ENCODE_PARAM_INTRA_PERIOD = (1 << 2),
	AML_ENCODE_PARAM_FORCE_INTRA = (1 << 3),
	AML_ENCODE_PARAM_GOP_SIZE = (1 << 4),
};

enum aml_fmt_type {
	AML_FMT_DEC = 0,
	AML_FMT_ENC = 1,
	AML_FMT_FRAME = 2,
};

/**
 * struct aml_video_fmt - Structure used to store information about pixelformats
 */
struct aml_video_fmt {
	u32			fourcc;
	enum aml_fmt_type	type;
	u32			num_planes;
	const u8		*name;
};

/**
 * struct aml_codec_framesizes - Structure used to store information about
 *							framesizes
 */
struct aml_codec_framesizes {
	u32	fourcc;
	struct	v4l2_frmsize_stepwise	stepwise;
};

/**
 * struct aml_q_type - Type of queue
 */
enum aml_q_type {
	AML_Q_DATA_SRC = 0,
	AML_Q_DATA_DST = 1,
};


/**
 * struct aml_q_data - Structure used to store information about queue
 */
struct aml_q_data {
	u32	visible_width;
	u32	visible_height;
	u32	coded_width;
	u32	coded_height;
	enum v4l2_field	field;
	u32	bytesperline[AML_VCODEC_MAX_PLANES];
	u32	sizeimage[AML_VCODEC_MAX_PLANES];
	struct aml_video_fmt	*fmt;
	bool resolution_changed;
};

/**
 * struct aml_enc_params - General encoding parameters
 * @bitrate: target bitrate in bits per second
 * @num_b_frame: number of b frames between p-frame
 * @rc_frame: frame based rate control
 * @rc_mb: macroblock based rate control
 * @seq_hdr_mode: H.264 sequence header is encoded separately or joined
 *		  with the first frame
 * @intra_period: I frame period
 * @gop_size: group of picture size, it's used as the intra frame period
 * @framerate_num: frame rate numerator. ex: framerate_num=30 and
 *		   framerate_denom=1 menas FPS is 30
 * @framerate_denom: frame rate denominator. ex: framerate_num=30 and
 *		     framerate_denom=1 menas FPS is 30
 * @h264_max_qp: Max value for H.264 quantization parameter
 * @h264_profile: V4L2 defined H.264 profile
 * @h264_level: V4L2 defined H.264 level
 * @force_intra: force/insert intra frame
 */
struct aml_enc_params {
	u32	bitrate;
	u32	num_b_frame;
	u32	rc_frame;
	u32	rc_mb;
	u32	seq_hdr_mode;
	u32	intra_period;
	u32	gop_size;
	u32	framerate_num;
	u32	framerate_denom;
	u32	h264_max_qp;
	u32	h264_profile;
	u32	h264_level;
	u32	force_intra;
};

/**
 * struct vdec_pic_info  - picture size information
 * @visible_width: picture width
 * @visible_height: picture height
 * @coded_width: picture buffer width (64 aligned up from pic_w)
 * @coded_height: picture buffer heiht (64 aligned up from pic_h)
 * @y_bs_sz: Y bitstream size
 * @c_bs_sz: CbCr bitstream size
 * @y_len_sz: additional size required to store decompress information for y
 *		plane
 * @c_len_sz: additional size required to store decompress information for cbcr
 *		plane
 * E.g. suppose picture size is 176x144,
 *      buffer size will be aligned to 176x160.
 * @profile_idc: source profile level
 * @field: frame/field information.
 * @dpb_frames: used for DPB size of calculation.
 * @dpb_margin: extra buffers for decoder.
 * @vpp_margin: extra buffers for vpp.
 */
struct vdec_pic_info {
	u32 visible_width;
	u32 visible_height;
	u32 coded_width;
	u32 coded_height;
	u32 y_bs_sz;
	u32 c_bs_sz;
	u32 y_len_sz;
	u32 c_len_sz;
	int profile_idc;
	enum v4l2_field field;
	u32 dpb_frames;
	u32 dpb_margin;
	u32 vpp_margin;
};

/**
 * struct vdec_comp_buf_info - compressed buffer info
 * @max_size: max size needed for MMU Box in MB
 * @header_size: contineous size for the compressed header
 * @frame_buffer_size: SG page number to store the frame
 */
struct vdec_comp_buf_info {
	u32 max_size;
	u32 header_size;
	u32 frame_buffer_size;
};

struct aml_vdec_cfg_infos {
	u32 double_write_mode;
	u32 init_width;
	u32 init_height;
	u32 ref_buf_margin;
	u32 canvas_mem_mode;
	u32 canvas_mem_endian;
	u32 low_latency_mode;
	u32 uvm_hook_type;
	/*
	 * bit 16	: force progressive output flag.
	 * bit 15	: enable nr.
	 * bit 14	: enable di local buff.
	 * bit 13	: report downscale yuv buffer size flag.
	 * bit 12	: for second field pts mode.
	 * bit 11	: disable error policy.
	 * bit 9	: disable ge2d wrapper.
	 * bit 1	: Non-standard dv flag.
	 * bit 0	: dv two layer flag.
	 */
	u32 metadata_config_flag; // for metadata config flag
	u32 duration;
	u32 data[4];
};

struct aml_vdec_hdr_infos {
	/*
	 * bit 29   : present_flag
	 * bit 28-26: video_format "component", "PAL", "NTSC", "SECAM", "MAC", "unspecified"
	 * bit 25   : range "limited", "full_range"
	 * bit 24   : color_description_present_flag
	 * bit 23-16: color_primaries "unknown", "bt709", "undef", "bt601",
	 *            "bt470m", "bt470bg", "smpte170m", "smpte240m", "film", "bt2020"
	 * bit 15-8 : transfer_characteristic unknown", "bt709", "undef", "bt601",
	 *            "bt470m", "bt470bg", "smpte170m", "smpte240m",
	 *            "linear", "log100", "log316", "iec61966-2-4",
	 *            "bt1361e", "iec61966-2-1", "bt2020-10", "bt2020-12",
	 *            "smpte-st-2084", "smpte-st-428"
	 * bit 7-0  : matrix_coefficient "GBR", "bt709", "undef", "bt601",
	 *            "fcc", "bt470bg", "smpte170m", "smpte240m",
	 *            "YCgCo", "bt2020nc", "bt2020c"
	 */
	u32 signal_type;
	struct vframe_master_display_colour_s color_parms;
};

struct aml_vdec_ps_infos {
	u32 visible_width;
	u32 visible_height;
	u32 coded_width;
	u32 coded_height;
	u32 profile;
	u32 mb_width;
	u32 mb_height;
	u32 dpb_size;
	u32 ref_frames;
	u32 dpb_frames;
	u32 dpb_margin;
	u32 field;
	u32 data[3];
};

struct aml_vdec_cnt_infos {
	u32 bit_rate;
	u32 frame_count;
	u32 error_frame_count;
	u32 drop_frame_count;
	u32 total_data;
};

struct aml_dec_params {
	u32 parms_status;
	struct aml_vdec_cfg_infos	cfg;
	struct aml_vdec_ps_infos	ps;
	struct aml_vdec_hdr_infos	hdr;
	struct aml_vdec_cnt_infos	cnt;
};

struct v4l2_config_parm {
	u32 type;
	u32 length;
	union {
		struct aml_dec_params dec;
		struct aml_enc_params enc;
		u8 data[200];
	} parm;
	u8 buf[4096];
};

struct v4l_buff_pool {
	/*
	 * bit 31-16: buffer state
	 * bit 15- 0: buffer index
	 */
	u32 seq[V4L_CAP_BUFF_MAX];
	u32 in, out;
	u32 dec, vpp, ge2d;
};

enum aml_thread_type {
	AML_THREAD_OUTPUT,
	AML_THREAD_CAPTURE,
};

typedef void (*aml_thread_func)(struct aml_vcodec_ctx *ctx);

struct aml_vdec_thread {
	struct list_head	node;
	spinlock_t		lock;
	struct semaphore	sem;
	struct task_struct	*task;
	enum aml_thread_type	type;
	void			*priv;
	int			stop;

	aml_thread_func		func;
};

/* struct internal_comp_buf - compressed buffer
 * @index: index of this buf within (B)MMU BOX
 * @ref: [0-7]:reference number of this buf
 *       [8-15]: use for reuse.
 * @mmu_box: mmu_box of context
 * @bmmu_box: bmmu_box of context
 * @box_ref: box_ref of context
 * @header_addr: header for compressed buffer
 * @frame_buffer_size: SG buffer page number from
 * @priv_data use for video composer
 *  struct vdec_comp_buf_info
 * @used: bit[0]: 0, idle; 1, decoder alloc from mmu_box
 *        bit[1]: 0, idle; 1, decoder alloc from mmu_box_dw.
 */
struct internal_comp_buf {
	u32		index;
	u32		ref;
	void		*mmu_box;
	void		*bmmu_box;
	struct kref	*box_ref;

	ulong		header_addr;
	u32		header_size;
	u32		frame_buffer_size;
	struct file_private_data priv_data;
	ulong	header_dw_addr;
	void	*mmu_box_dw;
	void	*bmmu_box_dw;
	u32     used;
};

/*
 * struct aml_uvm_buff_ref - uvm buff is used reseve ctx ref count.
 * @index	: index of video buffer.
 * @addr	: physic address of video buffer.
 * @ref		: reference of v4ldec context.
 * @dma		: dma buf of associated with vb.
 */
struct aml_uvm_buff_ref {
	int		index;
	ulong		addr;
	struct kref	*ref;
	struct dma_buf	*dbuf;
};

/*
 * enum aml_fb_requester - indicate which module request fb buffers.
 */
enum aml_fb_requester {
	AML_FB_REQ_DEC,
	AML_FB_REQ_VPP,
	AML_FB_REQ_GE2D,
	AML_FB_REQ_MAX
};

/*
 * @query: try to achieved fb token.
 * @alloc: used for allocte fb buffer.
 */
struct aml_fb_ops {
	bool		(*query)(struct aml_fb_ops *, ulong *);
	int		(*alloc)(struct aml_fb_ops *, ulong, struct vdec_v4l2_buffer **, u32);
};

/*
 * struct aml_fb_map_table - record some buffer map infos
 * @addr	: yuv linear buffer address.
 * @header_addr	: used for compress buffer.
 * @vframe	: which is from decoder or vpp vf pool.
 * @task	: context of task chain.
 * @icomp	: compress buffer index.
 */
struct aml_fb_map_table {
	ulong		addr;
	ulong		header_addr;
	struct vframe_s	*vframe;
	struct task_chain_s *task;
	u32		icomp;
};

/*
 * struct aux_data - record sei data and dv data
 * @sei_size:	sei data size.
 * @sei_buf:	sei data addr.
 * @sei_state:	sei buffer state. (0 free, 1 not used, 2 used)
 * @comp_buf:	stores comp data parsed from sei data.
 * @md_buf:	stores md data parsed from sei data.
 */
struct aux_data {
	int		sei_size;
	char*		sei_buf;
	int		sei_state;
	char*		comp_buf;
	char*		md_buf;
};

/*
 * struct aux_info - record aux data infos
 * @sei_index:		sei data index.
 * @dv_index:		dv data index.
 * @sei_need_free:	sei buffer need to free.
 * @bufs:		stores aux data.
 * @alloc_buffer:	alloc aux buffer functions.
 * @free_buffer:	free aux buffer functions.
 * @free_one_sei_buffer:free sei buffer with index functions.
 * @bind_sei_buffer:	bind sei buffer functions.
 * @bind_dv_buffer:	bind dv buffer functions.
 */
struct aux_info {
	int	sei_index;
	int 	dv_index;
	bool    sei_need_free;
	struct aux_data bufs[V4L_CAP_BUFF_MAX];
	void 	(*alloc_buffer)(struct aml_vcodec_ctx *ctx, int flag);
	void 	(*free_buffer)(struct aml_vcodec_ctx *ctx, int flag);
	void 	(*free_one_sei_buffer)(struct aml_vcodec_ctx *ctx, char **addr, int *size, int idx);
	void 	(*bind_sei_buffer)(struct aml_vcodec_ctx *ctx, char **addr, int *size, int *idx);
	void	(*bind_dv_buffer)(struct aml_vcodec_ctx *ctx, char **comp_buf, char **md_buf);
};

/*
 * struct meta_data - record meta data.
 * @buf[META_DATA_SIZE]: meta data information.
 */
struct meta_data {
	char buf[META_DATA_SIZE];
};

/*
 * struct meta_info - record some meta data infos
 * @index: meta data index.
 * @meta_bufs: record meta data.
 */
struct meta_info {
	int		index;
	struct meta_data *meta_bufs;
};

/*
 * struct aml_vpp_cfg_infos - config vpp init param
 * @mode	: vpp work mode
 * @fmt		: picture format used to switch nv21 or nv12.
 * @buf_size: config buffer size for vpp
 * @is_drm	: is drm mode
 * @is_prog	: is a progressive source.
 * @is_bypass_p	: to set progressive bypass in vpp
 * @enable_nr	: enable nosie reduce.
 * @enable_local_buf: DI used buff alloc by itself.
 * @res_chg	: indicate resolution changed.
 * @is_vpp_reset: vpp reset just used to res chg.
 */
struct aml_vpp_cfg_infos {
	u32	mode;
	u32	fmt;
	u32	buf_size;
	bool	is_drm;
	bool	is_prog;
	bool	is_bypass_p;
	bool	enable_nr;
	bool	enable_local_buf;
	bool	res_chg;
	bool	is_vpp_reset;
};

struct aml_ge2d_cfg_infos {
	u32	mode;
	u32	buf_size;
	bool	is_drm;
	bool	bypass;
};

struct canvas_res {
	int			cid;
	u8			name[32];
};

struct canvas_cache {
	int			ref;
	struct canvas_res	res[8];
	struct mutex		lock;
};

struct aml_decoder_status_info {
	u32 error_type;
	u32 frame_width;
	u32 frame_height;
	u32 decoder_count;
	u32 decoder_error_count;
};

/*
 * struct aml_vcodec_ctx - Context (instance) private data.
 * @id: index of the context that this structure describes.
 * @ctx_ref: for deferred free of this context.
 * @type: type of the instance - decoder or encoder.
 * @dev: pointer to the aml_vcodec_dev of the device.
 * @m2m_ctx: pointer to the v4l2_m2m_ctx of the context.
 * @ada_ctx: pointer to the aml_vdec_adapt of the context.
 * @vpp: pointer to video post processor
 * @dec_if: hooked decoder driver interface.
 * @drv_handle: driver handle for specific decode instance
 * @fh: struct v4l2_fh.
 * @ctrl_hdl: handler for v4l2 framework.
 * @slock: protect v4l2 codec context.
 * @tsplock: protect the vdec thread context.
 * @empty_flush_buf: a fake size-0 capture buffer that indicates flush.
 * @list: link to ctx_list of aml_vcodec_dev.
 * @q_data: store information of input and output queue of the context.
 * @queue: waitqueue that can be used to wait for this context to finish.
 * @state_lock: protect the codec status.
 * @state: state of the context.
 * @decode_work: decoder work be used to output buffer.
 * @output_thread_ready: indicate the output thread ready.
 * @cap_pool: capture buffers are remark in the pool.
 * @vdec_thread_list: vdec thread be used to capture.
 * @dpb_size: store dpb count after header parsing
 * @vpp_size: store vpp buffer count after header parsing
 * @param_change: indicate encode parameter type
 * @param_sets_from_ucode: if true indicate ps from ucode.
 * @v4l_codec_dpb_ready: queue buffer number greater than dpb.
 # @v4l_resolution_change: indicate resolution change happend.
 * @comp: comp be used for sync picture information with decoder.
 * @config: used to set or get parms for application.
 * @picinfo: store picture info after header parsing.
 * @last_decoded_picinfo: pic information get from latest decode.
 * @colorspace: enum v4l2_colorspace; supplemental to pixelformat.
 * @ycbcr_enc: enum v4l2_ycbcr_encoding, Y'CbCr encoding.
 * @quantization: enum v4l2_quantization, colorspace quantization.
 * @xfer_func: enum v4l2_xfer_func, colorspace transfer function.
 * @cap_pix_fmt: the picture format used to switch nv21 or nv12.
 * @has_receive_eos: if receive last frame of capture that be set.
 * @is_drm_mode: decoding work on drm mode if that set.
 * @is_stream_mode: vdec input used to stream mode, default frame mode.
 * @is_stream_off: the value used to handle reset active.
 * @is_out_stream_off: streamoff called for output port.
 * @receive_cmd_stop: if receive the cmd flush decoder.
 * @reset_flag: reset mode includes lightly and normal mode.
 * @decoded_frame_cnt: the capture buffer deque number to be count.
 * @buf_used_count: means that decode allocate how many buffs from v4l.
 * @wq: wait recycle dma buffer finish.
 * @cap_wq: the wq used for wait capture buffer.
 * @dmabuff_recycle_lock: protect the lock dmabuff free.
 * @dmabuff_recycle_work: used for recycle dmabuff.
 * @dmabuff_recycle: kfifo used for store vb buff.
 * @capture_buffer: kfifo used for store capture vb buff.
 * @mmu_box: mmu_box of context.
 * @bmmu_box: bmmu_box of context.
 * @box_ref: box_ref of context.
 * @comp_info: compress buffer information.
 * @comp_bufs: compress buffer describe.
 * @comp_lock: used for lock ibuf free cb.
 * @fb_ops: frame buffer ops interface.
 * @dv_infos: dv data information.
 * @vpp_cfg: vpp init parms of configuration.
 * @vdec_pic_info_update: update pic info cb.
 * @vpp_is_need: the instance is need vpp.
 * @task_chain_pool: used to store task chain inst.
 * @index_disp: the number of frames output.
 */
struct aml_vcodec_ctx {
	int				id;
	struct kref			ctx_ref;
	enum aml_instance_type		type;
	struct aml_vcodec_dev		*dev;
	struct v4l2_m2m_ctx		*m2m_ctx;
	struct aml_vdec_adapt		*ada_ctx;
	struct aml_v4l2_vpp		*vpp;
	const struct vdec_common_if	*dec_if;
	ulong				drv_handle;
	struct v4l2_fh			fh;
	struct v4l2_ctrl_handler	ctrl_hdl;
	spinlock_t			slock;
	spinlock_t			tsplock;
	struct aml_video_dec_buf	*empty_flush_buf;
	struct list_head		list;

	struct aml_q_data		q_data[2];
	wait_queue_head_t		queue;
	struct mutex			state_lock;
	enum aml_instance_state		state;
	struct work_struct		decode_work;
	bool				output_thread_ready;
	struct v4l_buff_pool		cap_pool;
	struct list_head		vdec_thread_list;

	int				dpb_size;
	int				vpp_size;
	int				ge2d_size;
	bool				param_sets_from_ucode;
	bool				v4l_codec_dpb_ready;
	bool				v4l_resolution_change;
	struct completion		comp;
	struct v4l2_config_parm		config;
	struct vdec_pic_info		picinfo;
	struct vdec_pic_info		last_decoded_picinfo;
	enum v4l2_colorspace		colorspace;
	enum v4l2_ycbcr_encoding	ycbcr_enc;
	enum v4l2_quantization		quantization;
	enum v4l2_xfer_func		xfer_func;
	u32				cap_pix_fmt;
	u32				output_pix_fmt;

	bool				has_receive_eos;
	bool				is_drm_mode;
	bool				output_dma_mode;
	bool				is_stream_off;
	bool				is_out_stream_off;
	bool				receive_cmd_stop;
	int				reset_flag;
	int				decoded_frame_cnt;
	int				buf_used_count;
	wait_queue_head_t		wq, cap_wq, post_done_wq;
	struct mutex			capture_buffer_lock;
	spinlock_t			dmabuff_recycle_lock;
	struct mutex			buff_done_lock;
	struct work_struct		dmabuff_recycle_work;
	DECLARE_KFIFO(dmabuff_recycle, struct vb2_v4l2_buffer *, 32);
	DECLARE_KFIFO(capture_buffer, struct vb2_v4l2_buffer *, 32);

	/* compressed buffer support */
	void				*bmmu_box;
	void				*mmu_box;
	struct kref			box_ref;
	struct vdec_comp_buf_info	comp_info;
	struct internal_comp_buf	*comp_bufs;
	struct uvm_hook_mod_info	*uvm_proxy;
	struct mutex			comp_lock;

	struct aml_fb_ops		fb_ops;
	ulong				token_table[32];

	struct aml_fb_map_table		fb_map[32];
	struct aml_vpp_cfg_infos 	vpp_cfg;
	void (*vdec_pic_info_update)(struct aml_vcodec_ctx *ctx);
	bool				vpp_is_need;
	struct list_head		task_chain_pool;
	struct meta_info		meta_infos;
	struct vdec_sync		*sync;
	u32             		internal_dw_scale;

	/* ge2d field. */
	struct aml_v4l2_ge2d		*ge2d;
	struct aml_ge2d_cfg_infos 	ge2d_cfg;
	bool				ge2d_is_need;

	bool 				second_field_pts_mode;
	struct aux_info			aux_infos;
	u32				capture_memory_mode;
	u32				height_aspect_ratio;
	u32				width_aspect_ratio;
	u32				index_disp;
	bool				post_to_upper_done;
	bool			film_grain_present;
	void			*bmmu_box_dw;
	void			*mmu_box_dw;
	struct aml_decoder_status_info	decoder_status_info;
};

/**
 * struct aml_vcodec_dev - driver data.
 * @v4l2_dev		: V4L2 device to register video devices for.
 * @vfd_dec		: Video device for decoder.
 * @plat_dev		: platform device.
 * @m2m_dev_dec		: m2m device for decoder.
 * @curr_ctx		: The context that is waiting for codec hardware.
 * @id_counter		: used to identify current opened instance.
 * @dec_capability	: used to identify decode capability, ex: 4k
 * @decode_workqueue	: the worker used to output buffer schedule.
 * @ctx_list		: list of struct aml_vcodec_ctx.
 * @irqlock		: protect data access by irq handler and work thread.
 * @dev_mutex		: video_device lock.
 * @dec_mutex		: decoder hardware lock.
 * @queue		: waitqueue for waiting for completion of device commands.
 * @vpp_count		: count the number of open vpp.
 * @v4ldec_class	: creat class sysfs uesd to show some information.
 * @canche		: canvas pool specific used for v4ldec context.
 */
struct aml_vcodec_dev {
	struct v4l2_device		v4l2_dev;
	struct video_device		*vfd_dec;
	struct platform_device		*plat_dev;
	struct v4l2_m2m_dev		*m2m_dev_dec;
	struct aml_vcodec_ctx		*curr_ctx;
	ulong				id_counter;
	u32				dec_capability;
	struct workqueue_struct		*decode_workqueue;
	struct list_head		ctx_list;
	struct file			*filp;
	spinlock_t			irqlock;
	struct mutex			dev_mutex;
	struct mutex			dec_mutex;
	wait_queue_head_t		queue;
	atomic_t			vpp_count;
	struct class			v4ldec_class;
	struct canvas_cache		canche;
};

static inline struct aml_vcodec_ctx *fh_to_ctx(struct v4l2_fh *fh)
{
	return container_of(fh, struct aml_vcodec_ctx, fh);
}

static inline struct aml_vcodec_ctx *ctrl_to_ctx(struct v4l2_ctrl *ctrl)
{
	return container_of(ctrl->handler, struct aml_vcodec_ctx, ctrl_hdl);
}

void aml_thread_capture_worker(struct aml_vcodec_ctx *ctx);
void aml_thread_post_task(struct aml_vcodec_ctx *ctx, enum aml_thread_type type);
int aml_thread_start(struct aml_vcodec_ctx *ctx, aml_thread_func func,
	enum aml_thread_type type, const char *thread_name);
void aml_thread_stop(struct aml_vcodec_ctx *ctx);

#endif /* _AML_VCODEC_DRV_H_ */
