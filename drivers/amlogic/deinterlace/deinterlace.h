#ifndef _DI_H
#define _DI_H
#include <linux/cdev.h>
#include <linux/amlogic/amports/vframe.h>
#include <linux/amlogic/amports/video.h>
#include <linux/atomic.h>

/* di hardware version m8m2*/
#define NEW_DI_V1 0x00000002 /* from m6tvc */
#define NEW_DI_V2 0x00000004 /* from m6tvd */
#define NEW_DI_V3 0x00000008 /* from gx */
#define NEW_DI_V4 0x00000010 /* dnr added */

/*trigger_pre_di_process param*/
#define TRIGGER_PRE_BY_PUT		'p'
#define TRIGGER_PRE_BY_DE_IRQ		'i'
#define TRIGGER_PRE_BY_UNREG		'u'
/*di_timer_handle*/
#define TRIGGER_PRE_BY_TIMER		't'
#define TRIGGER_PRE_BY_FORCE_UNREG	'f'
#define TRIGGER_PRE_BY_VFRAME_READY	'r'
#define TRIGGER_PRE_BY_PROVERDER_UNREG	'n'
#define TRIGGER_PRE_BY_DEBUG_DISABLE	'd'
#define TRIGGER_PRE_BY_TIMERC		'T'
#define TRIGGER_PRE_BY_PROVERDER_REG	'R'


/*vframe define*/
#define vframe_t struct vframe_s

/* canvas defination */
#define DI_USE_FIXED_CANVAS_IDX

#undef USE_LIST
/* #if MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON6 */
#define NEW_KEEP_LAST_FRAME
/* #endif */
#define	DET3D
#undef SUPPORT_MPEG_TO_VDIN /* for all ic after m6c@20140731 */

#ifndef CONFIG_VSYNC_RDMA
#ifndef VSYNC_WR_MPEG_REG
#define VSYNC_WR_MPEG_REG(adr, val) aml_write_vcbus(adr, val)
#define VSYNC_WR_MPEG_REG_BITS(adr, val, start, len)  \
		aml_vcbus_update_bits(adr, ((1<<len)-1)<<start, val<<start)
#define VSYNC_RD_MPEG_REG(adr) aml_read_vcbus(adr)
#endif
#endif

#include "film_vof_soft.h"

/************************************
*	 di hardware level interface
*************************************/
#define MAX_WIN_NUM			5
/* if post size < 80, filter of ei can't work */
#define MIN_POST_WIDTH  80
#define MIN_BLEND_WIDTH  27
struct pulldown_detect_info_s {
	unsigned field_diff;
/* total pixels difference between current field and previous field */
	unsigned field_diff_num;
/* the number of pixels with big difference between
				current field and previous field */
	unsigned frame_diff;
/*total pixels difference between current field and previouse-previouse field*/
	unsigned frame_diff_num;
/* the number of pixels with big difference between current
				field and previouse-previous field */
	unsigned frame_diff_skew;
/* the difference between current frame_diff and previous frame_diff */
	unsigned frame_diff_num_skew;
/* the difference between current frame_diff_num and previous frame_diff_num*/
/* parameters for detection */
	unsigned field_diff_by_pre;
	unsigned field_diff_by_next;
	unsigned field_diff_num_by_pre;
	unsigned field_diff_num_by_next;
	unsigned frame_diff_by_pre;
	unsigned frame_diff_num_by_pre;
	unsigned frame_diff_skew_ratio;
	unsigned frame_diff_num_skew_ratio;
	/* matching pattern */
	unsigned field_diff_pattern;
	unsigned field_diff_num_pattern;
	unsigned frame_diff_pattern;
	unsigned frame_diff_num_pattern;
};
#define pulldown_detect_info_t struct pulldown_detect_info_s
struct pd_detect_threshold_s {
	/*
		if frame_diff < threshold, cur_field and
			pre_pre_field is top/bot or bot/top;
		if field_diff < threshold, cur_field and
			pre_field is top/bot or bot/top;
	 */
	unsigned frame_diff_chg_th;
	unsigned frame_diff_num_chg_th;
	unsigned field_diff_chg_th;
	unsigned field_diff_num_chg_th;
	/*
		if frame_diff_skew < threshold, pre_field/cur_filed is top/bot
	*/
	unsigned frame_diff_skew_th;
	unsigned frame_diff_num_skew_th;

	unsigned field_diff_num_th;
};
#define pd_detect_threshold_t struct pd_detect_threshold_s
struct pd_win_prop_s {
	unsigned int pixels_num;
};
#define pd_win_prop_t struct pd_win_prop_s
enum process_fun_index_e {
	PROCESS_FUN_NULL = 0,
	PROCESS_FUN_DI,
	PROCESS_FUN_PD,
	PROCESS_FUN_PROG,
	PROCESS_FUN_BOB
};
#define process_fun_index_t enum process_fun_index_e
enum pulldown_mode_e {
	PULL_DOWN_BLEND_0 = 0,/* buf1=dup[0] */
	PULL_DOWN_BLEND_2 = 1,/* buf1=dup[2] */
	PULL_DOWN_MTN	  = 2,/* mtn only */
	PULL_DOWN_BUF1	  = 3,/* do wave with dup[0] */
	PULL_DOWN_EI	  = 4,/* ei only */
	PULL_DOWN_NORMAL  = 5,/* normal di */
};

enum canvas_idx_e {
	NR_CANVAS,
	MTN_CANVAS,
	MV_CANVAS,
};
#define pulldown_mode_t enum pulldown_mode_e
struct di_buf_s {
#ifdef USE_LIST
	struct list_head list;
#endif
	struct vframe_s *vframe;
	int index; /* index in vframe_in_dup[] or vframe_in[],
	only for type of VFRAME_TYPE_IN */
	int post_proc_flag; /* 0,no post di; 1, normal post di;
	2, edge only; 3, dummy */
	int new_format_flag;
	int type;
	int throw_flag;
	int invert_top_bot_flag;
	int seq;
	int pre_ref_count; /* none zero, is used by mem_mif,
	chan2_mif, or wr_buf*/
	int post_ref_count; /* none zero, is used by post process */
	int queue_index;
	/*below for type of VFRAME_TYPE_LOCAL */
	unsigned long nr_adr;
	int nr_canvas_idx;
	unsigned long mtn_adr;
	int mtn_canvas_idx;
#ifdef NEW_DI_V1
	unsigned long cnt_adr;
	int cnt_canvas_idx;
#endif
#ifdef NEW_DI_V3
	unsigned long mcinfo_adr;
	int mcinfo_canvas_idx;
	unsigned long mcvec_adr;
	int mcvec_canvas_idx;
	struct mcinfo_pre_s {
	unsigned int highvertfrqflg;
	unsigned int motionparadoxflg;
	unsigned int regs[26];/* reg 0x2fb0~0x2fc9 */
	} curr_field_mcinfo;
#endif
	/* blend window */
	unsigned short reg0_s;
	unsigned short reg0_e;
	unsigned short reg0_bmode;
	unsigned short reg1_s;
	unsigned short reg1_e;
	unsigned short reg1_bmode;
	unsigned short reg2_s;
	unsigned short reg2_e;
	unsigned short reg2_bmode;
	unsigned short reg3_s;
	unsigned short reg3_e;
	unsigned short reg3_bmode;
	/* tff bff check result bit[1:0]*/
	unsigned int privated;
	unsigned int canvas_config_flag;
	/* 0,configed; 1,config type 1 (prog);
	2, config type 2 (interlace) */
	unsigned int canvas_height;
	unsigned int canvas_width[3];/* nr/mtn/mv */
	/*bit [31~16] width; bit [15~0] height*/
	pulldown_detect_info_t field_pd_info;
	pulldown_detect_info_t win_pd_info[MAX_WIN_NUM];
	pulldown_mode_t pulldown_mode;
	int win_pd_mode[5];
	process_fun_index_t process_fun_index;
	int early_process_fun_index;
	int left_right;/*1,left eye; 0,right eye in field alternative*/
	/*below for type of VFRAME_TYPE_POST*/
	struct di_buf_s *di_buf[2];
	struct di_buf_s *di_buf_dup_p[5];
	/* 0~4: n-2, n-1, n, n+1, n+2;	n is the field to display*/
	struct di_buf_s *di_wr_linked_buf;
	/* debug for di-vf-get/put
	1: after get
	0: after put*/
	atomic_t di_cnt;
};
extern uint di_mtn_1_ctrl1;
#ifdef DET3D
extern bool det3d_en;
#endif

extern uint mtn_ctrl1;

extern pd_detect_threshold_t win_pd_th[MAX_WIN_NUM];
extern pd_win_prop_t pd_win_prop[MAX_WIN_NUM];

extern int	pd_enable;

extern void di_hw_init(void);

extern void di_hw_uninit(void);

extern void enable_di_pre_mif(int enable);

extern int di_vscale_skip_count;

extern unsigned int di_force_bit_mode;

/*
di hardware internal
*/
#define RDMA_DET3D_IRQ				0x20
/* vdin0 rdma irq */
#define RDMA_DEINT_IRQ				0x2
#define RDMA_TABLE_SIZE                    ((PAGE_SIZE)<<1)

#if defined(CONFIG_AM_DEINTERLACE_SD_ONLY)
#define MAX_CANVAS_WIDTH				720
#define MAX_CANVAS_HEIGHT				576
#else
#define MAX_CANVAS_WIDTH				1920
#define MAX_CANVAS_HEIGHT				1088
#endif

struct DI_MIF_s {
	unsigned short	luma_x_start0;
	unsigned short	luma_x_end0;
	unsigned short	luma_y_start0;
	unsigned short	luma_y_end0;
	unsigned short	chroma_x_start0;
	unsigned short	chroma_x_end0;
	unsigned short	chroma_y_start0;
	unsigned short	chroma_y_end0;
	unsigned		set_separate_en:2;
	unsigned		src_field_mode:1;
	unsigned		video_mode:1;
	unsigned		output_field_num:1;
	unsigned		bit_mode:2;
	unsigned		src_prog:1;
	/*
	unsigned		burst_size_y:2; set 3 as default
	unsigned		burst_size_cb:2;set 1 as default
	unsigned		burst_size_cr:2;set 1 as default
	*/
	unsigned		canvas0_addr0:8;
	unsigned		canvas0_addr1:8;
	unsigned		canvas0_addr2:8;
};
struct DI_SIM_MIF_s {
	unsigned short	start_x;
	unsigned short	end_x;
	unsigned short	start_y;
	unsigned short	end_y;
	unsigned short	canvas_num;
	unsigned short	bit_mode;
};
struct DI_MC_MIF_s {
	unsigned short start_x;
	unsigned short start_y;
	unsigned short size_x;
	unsigned short size_y;
	unsigned short canvas_num;
	unsigned short blend_en;
	unsigned short vecrd_offset;
};
void disable_deinterlace(void);

void disable_pre_deinterlace(void);

void disable_post_deinterlace(void);

int get_di_pre_recycle_buf(void);


void disable_post_deinterlace_2(void);

void enable_film_mode_check(unsigned int width, unsigned int height,
		enum vframe_source_type_e);

void enable_di_pre_aml(
	struct DI_MIF_s		*di_inp_mif,
	struct DI_MIF_s		*di_mem_mif,
	struct DI_MIF_s		*di_chan2_mif,
	struct DI_SIM_MIF_s	*di_nrwr_mif,
	struct DI_SIM_MIF_s	*di_mtnwr_mif,
#ifdef NEW_DI_V1
	struct DI_SIM_MIF_s    *di_contp2rd_mif,
	struct DI_SIM_MIF_s    *di_contprd_mif,
	struct DI_SIM_MIF_s    *di_contwr_mif,
#endif
	int nr_en, int mtn_en, int pd32_check_en, int pd22_check_en,
	int hist_check_en, int pre_field_num, int pre_vdin_link,
	int hold_line, int urgent
	);
void enable_afbc_input(struct vframe_s *vf);
#ifdef NEW_DI_V3
void enable_mc_di_pre(struct DI_MC_MIF_s *di_mcinford_mif,
	struct DI_MC_MIF_s *di_mcinfowr_mif,
	struct DI_MC_MIF_s *di_mcvecwr_mif, int urgent);
void enable_mc_di_post(struct DI_MC_MIF_s *di_mcvecrd_mif,
	int urgent, bool reverse);
#endif

void read_new_pulldown_info(struct FlmModReg_t *pFMRegp);

void enable_region_blend(
	int reg0_en, int reg0_start_x, int reg0_end_x, int reg0_start_y,
	int reg0_end_y, int reg0_mode, int reg1_en, int reg1_start_x,
	int reg1_end_x, int reg1_start_y, int reg1_end_y, int reg1_mode,
	int reg2_en, int reg2_start_x, int reg2_end_x, int reg2_start_y,
	int reg2_end_y, int reg2_mode, int reg3_en, int reg3_start_x,
	int reg3_end_x, int reg3_start_y, int reg3_end_y, int reg3_mode
	);


void run_deinterlace(unsigned zoom_start_x_lines, unsigned zoom_end_x_lines,
	unsigned zoom_start_y_lines, unsigned zoom_end_y_lines,
	unsigned type, int mode, int hold_line);

void deinterlace_init(void);

void initial_di_pre_aml(int hsize_pre, int vsize_pre, int hold_line);

void initial_di_post_2(int hsize_post, int vsize_post, int hold_line);
void di_nr_level_config(int level);
void enable_di_post_2(
	struct DI_MIF_s		*di_buf0_mif,
	struct DI_MIF_s		*di_buf1_mif,
	struct DI_MIF_s		*di_buf2_mif,
	struct DI_SIM_MIF_s	*di_diwr_mif,
	#ifndef NEW_DI_V2
	struct DI_SIM_MIF_s	*di_mtncrd_mif,
	#endif
	struct DI_SIM_MIF_s	*di_mtnprd_mif,
	int ei_en, int blend_en, int blend_mtn_en, int blend_mode,
	int di_vpp_en, int di_ddr_en,
	int post_field_num, int hold_line , int urgent
	#ifndef NEW_DI_V1
	, unsigned long *reg_mtn_info
	#endif
);

void di_post_switch_buffer(
	struct DI_MIF_s		*di_buf0_mif,
	struct DI_MIF_s		*di_buf1_mif,
	struct DI_MIF_s		*di_buf2_mif,
	struct DI_SIM_MIF_s	*di_diwr_mif,
	#ifndef NEW_DI_V2
	struct DI_SIM_MIF_s	*di_mtncrd_mif,
	#endif
	struct DI_SIM_MIF_s	*di_mtnprd_mif,
	struct DI_MC_MIF_s		*di_mcvecrd_mif,
	int ei_en, int blend_en, int blend_mtn_en, int blend_mode,
	int di_vpp_en, int di_ddr_en,
	int post_field_num, int hold_line, int urgent
	#ifndef NEW_DI_V1
	, unsigned long *reg_mtn_info
	#endif
);

bool read_pulldown_info(pulldown_detect_info_t *field_pd_info,
		pulldown_detect_info_t *win_pd_info);
#ifndef NEW_DI_V1
void read_mtn_info(unsigned long *mtn_info, unsigned long*);
#endif

/* for video reverse */
void di_post_read_reverse(bool reverse);
void di_post_read_reverse_irq(bool reverse, unsigned char mc_pre_flag);
extern void recycle_keep_buffer(void);

/* #define DI_BUFFER_DEBUG */

#define DI_LOG_MTNINFO		0x02
#define DI_LOG_PULLDOWN		0x10
#define DI_LOG_BUFFER_STATE		0x20
#define DI_LOG_TIMESTAMP		0x100
#define DI_LOG_PRECISE_TIMESTAMP		0x200
#define DI_LOG_QUEUE		0x40
#define DI_LOG_VFRAME		0x80

extern unsigned int di_log_flag;
extern unsigned int di_debug_flag;
extern bool mcpre_en;
extern bool dnr_reg_update;
extern bool dnr_dm_en;
extern int mpeg2vdin_flag;
extern int di_vscale_skip_count_real;
extern unsigned int pulldown_enable;

extern bool post_wr_en;
extern unsigned int post_wr_surpport;

int di_print(const char *fmt, ...);


struct reg_set_s {
	unsigned int adr;
	unsigned int val;
	unsigned short start;
	unsigned short len;
};
#define reg_set_t struct reg_set_s

#define REG_SET_MAX_NUM 128
#define FMT_MAX_NUM		32
struct reg_cfg_ {
	struct reg_cfg_ *next;
	unsigned int source_types_enable;
	/* each bit corresponds to one source type */
	unsigned int pre_post_type; /* pre, 0; post, 1 */
	unsigned int dtv_defintion_type;
	/*high defintion,0; stand defintion ,1;common,2*/
	unsigned int sig_fmt_range[FMT_MAX_NUM];
	/* {bit[31:16]~bit[15:0]}, include bit[31:16] and bit[15:0]  */
	reg_set_t reg_set[REG_SET_MAX_NUM];
};
#define reg_cfg_t struct reg_cfg_
int get_current_vscale_skip_count(struct vframe_s *vf);

void di_set_power_control(unsigned char type, unsigned char enable);
void diwr_set_power_control(unsigned char enable);

unsigned char di_get_power_control(unsigned char type);
void config_di_bit_mode(vframe_t *vframe, unsigned int bypass_flag);
void combing_pd22_window_config(unsigned int width, unsigned int height);
int tff_bff_check(int height, int width);
void tbff_init(void);
void di_hw_disable(void);
#ifdef CONFIG_AM_ATVDEMOD
extern int aml_atvdemod_get_snr_ex(void);
#endif

void DI_Wr(unsigned int addr, unsigned int val);
void DI_Wr_reg_bits(unsigned int adr, unsigned int val,
		unsigned int start, unsigned int len);
void DI_VSYNC_WR_MPEG_REG(unsigned int addr, unsigned int val);
void DI_VSYNC_WR_MPEG_REG_BITS(unsigned int addr, unsigned int val,
	unsigned int start, unsigned int len);

#define DI_COUNT   1
#define DI_MAP_FLAG	0x1
struct di_dev_s {
	dev_t			   devt;
	struct cdev		   cdev; /* The cdev structure */
	struct device	   *dev;
	struct platform_device	*pdev;
	struct task_struct *task;
	unsigned char	   di_event;
	unsigned int	   di_irq;
	unsigned int	   flags;
	unsigned int	   timerc_irq;
	unsigned long	   jiffy;
	unsigned long	   mem_start;
	unsigned int	   mem_size;
	unsigned int	   buffer_size;
	unsigned int	   buf_num_avail;
	unsigned int	   hw_version;
	int		rdma_handle;
	/* is surpport nr10bit */
	unsigned int	   nr10bit_surpport;
	/* is DI surpport post wr to mem for OMX */
	unsigned int       post_wr_surpport;
	unsigned int	   flag_cma;
	unsigned int	   cma_alloc[10];
	unsigned int	   buffer_addr[10];
	struct page	*pages[10];
};
#define di_dev_t struct di_dev_s

#define di_pr_info(fmt, args ...)   pr_info("DI: " fmt, ## args)

#define pr_dbg(fmt, args ...)       pr_debug("DI: " fmt, ## args)

#define pr_error(fmt, args ...)     pr_err("DI: " fmt, ## args)

#endif
