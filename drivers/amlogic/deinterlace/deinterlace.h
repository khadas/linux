#ifndef _DI_H
#define _DI_H
#include <linux/cdev.h>
#include <linux/amlogic/amports/vframe.h>
#include <linux/amlogic/amports/video.h>
/* di hardware version m8m2*/
#define NEW_DI_V1 0x00000002 /* from m6tvc */
#define NEW_DI_V2 0x00000004 /* from m6tvd */
#define NEW_DI_V3 0x00000008 /* from gx */
#define NEW_DI_V4 0x00000010 /* dnr added */
/*vframe define*/
#define vframe_t struct vframe_s

/* canvas defination */
#define DI_USE_FIXED_CANVAS_IDX
#ifdef DI_USE_FIXED_CANVAS_IDX
#define DI_PRE_MEM_NR_CANVAS_IDX		0x45
#define DI_PRE_CHAN2_NR_CANVAS_IDX		0x46
#define DI_PRE_WR_NR_CANVAS_IDX			0x47
#define DI_PRE_WR_MTN_CANVAS_IDX		0x48
/* NEW DI */
#define DI_CONTPRD_CANVAS_IDX			0x49
#define DI_CONTP2RD_CANVAS_IDX			 0x4a
#define DI_CONTWR_CANVAS_IDX			0x4b
#define DI_MCINFORD_CANVAS_IDX			0x4c
#define DI_MCINFOWR_CANVAS_IDX			0x4d
#define DI_MCVECWR_CANVAS_IDX			0x4e
/* DI POST, share with DISPLAY */
#define DI_POST_BUF0_CANVAS_IDX			0x4f
#define DI_POST_BUF1_CANVAS_IDX			0x50
#define DI_POST_MTNCRD_CANVAS_IDX		0x51
#define DI_POST_MTNPRD_CANVAS_IDX		0x52
/* MCDI POST */
#define DI_POST_MCVECRD_CANVAS_IDX		0x53
#define DI_POST_MCVECRD_CANVAS_IDX2		0x54

#ifdef CONFIG_VSYNC_RDMA
#define DI_POST_BUF0_CANVAS_IDX2		 0x55
#define DI_POST_BUF1_CANVAS_IDX2		 0x56
#define DI_POST_MTNCRD_CANVAS_IDX2		 0x57
#define DI_POST_MTNPRD_CANVAS_IDX2		 0x58
#endif
#endif
#undef USE_LIST
/* #if MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON6 */
#define NEW_KEEP_LAST_FRAME
/* #endif */
#undef DET3D
#undef SUPPORT_MPEG_TO_VDIN /* for all ic after m6c@20140731 */

#ifndef CONFIG_VSYNC_RDMA
#ifndef VSYNC_WR_MPEG_REG
#define VSYNC_WR_MPEG_REG(adr, val) aml_write_vcbus(adr, val)
#define VSYNC_WR_MPEG_REG_BITS(adr, val, start, len)  \
		aml_vcbus_update_bits(adr, ((1<<len)-1)<<start, val<<start)
#define VSYNC_RD_MPEG_REG(adr) aml_read_vcbus(adr)
#endif
#endif

/************************************
*	 di hardware level interface
*************************************/
#define WIN_SIZE_FACTOR		100

#define PD32_PAR_NUM		6
#define PD22_PAR_NUM		6
#define MAX_WIN_NUM			5

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
	uint win_start_x_r;
	uint win_end_x_r;
	uint win_start_y_r;
	uint win_end_y_r;
	uint win_32lvl;
	uint win_22lvl;
	uint pixels_num;
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
#define pulldown_mode_t enum pulldown_mode_e
struct di_buf_s {
#ifdef D2D3_SUPPORT
	unsigned long dp_buf_adr;
	unsigned int dp_buf_size;
	unsigned int reverse_flag;
#endif
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
	unsigned int mcinfo_adr;
	int mcinfo_canvas_idx;
	unsigned long mcvec_adr;
	int mcvec_canvas_idx;
	struct mcinfo_pre_s {
	unsigned int highvertfrqflg;
	unsigned int motionparadoxflg;
	unsigned int regs[26];/* reg 0x2fb0~0x2fc9 */
	} curr_field_mcinfo;
#endif
	unsigned int canvas_config_flag;
	/* 0,configed; 1,config type 1 (prog);
	2, config type 2 (interlace) */
	unsigned int canvas_config_size;
	/*bit [31~16] width; bit [15~0] height*/
	pulldown_detect_info_t field_pd_info;
	pulldown_detect_info_t win_pd_info[MAX_WIN_NUM];
#ifndef NEW_DI_V1
	unsigned int mtn_info[5];
#endif
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
};
#define di_buf_t struct di_buf_s
extern uint di_mtn_1_ctrl1;
extern uint ei_ctrl0;
extern uint ei_ctrl1;
extern uint ei_ctrl2;
#ifdef NEW_DI_V1
extern uint ei_ctrl3;
#endif
#ifdef DET3D
extern bool det3d_en;
#endif

extern uint mtn_ctrl;
extern uint mtn_ctrl_char_diff_cnt;
extern uint mtn_ctrl_low_level;
extern uint mtn_ctrl_high_level;
extern uint mtn_ctrl_diff_level;
extern uint mtn_ctrl1;
extern uint mtn_ctrl1_reduce;
extern uint mtn_ctrl1_shift;
extern uint blend_ctrl;
extern uint kdeint0;
extern uint kdeint1;
extern uint kdeint2;
#ifndef NEW_DI_V1
extern uint reg_mtn_info0;
extern uint reg_mtn_info1;
extern uint reg_mtn_info2;
extern uint reg_mtn_info3;
extern uint reg_mtn_info4;
extern uint mtn_thre_1_low;
extern uint mtn_thre_1_high;
extern uint mtn_thre_2_low;
extern uint mtn_thre_2_high;
#endif

extern uint blend_ctrl1;
extern uint blend_ctrl1_char_level;
extern uint blend_ctrl1_angle_thd;
extern uint blend_ctrl1_filt_thd;
extern uint blend_ctrl1_diff_thd;
extern uint blend_ctrl2;
extern uint blend_ctrl2_black_level;
extern uint blend_ctrl2_mtn_no_mov;
extern uint post_ctrl__di_blend_en;
extern uint post_ctrl__di_post_repeat;
extern uint di_pre_ctrl__di_pre_repeat;

extern uint field_32lvl;
extern uint field_22lvl;
extern pd_detect_threshold_t field_pd_th;
extern pd_detect_threshold_t win_pd_th[MAX_WIN_NUM];
extern pd_win_prop_t pd_win_prop[MAX_WIN_NUM];

extern int	pd_enable;

extern void di_hw_init(void);

extern void di_hw_uninit(void);

extern int di_vscale_skip_count;
/*
di hardware internal
*/

#if defined(CONFIG_AM_DEINTERLACE_SD_ONLY)
#define MAX_CANVAS_WIDTH				720
#define MAX_CANVAS_HEIGHT				576
#else
#define MAX_CANVAS_WIDTH				1920
#define MAX_CANVAS_HEIGHT				1088
#endif

#define DI_BUF_NUM			6

struct DI_MIF_TYPE {
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
	/*
	unsigned		burst_size_y:2; set 3 as default
	unsigned		burst_size_cb:2;set 1 as default
	unsigned		burst_size_cr:2;set 1 as default
	*/
	unsigned		canvas0_addr0:8;
	unsigned		canvas0_addr1:8;
	unsigned		canvas0_addr2:8;
};
#define DI_MIF_t struct DI_MIF_TYPE
struct DI_SIM_MIF_TYPE {
	unsigned short	start_x;
	unsigned short	end_x;
	unsigned short	start_y;
	unsigned short	end_y;
	unsigned short	canvas_num;
};
#define DI_SIM_MIF_t struct DI_SIM_MIF_TYPE
struct DI_MC_MIF_TYPE {
	unsigned short start_x;
	unsigned short start_y;
	unsigned short size_x;
	unsigned short size_y;
	unsigned short canvas_num;
	unsigned short blend_mode;
};
#define DI_MC_MIF_t struct DI_MC_MIF_TYPE
void disable_deinterlace(void);

void disable_pre_deinterlace(void);

void disable_post_deinterlace(void);

int get_di_pre_recycle_buf(void);


void disable_post_deinterlace_2(void);

void enable_di_mode_check_2(
	int win0_start_x, int win0_end_x, int win0_start_y, int win0_end_y,
	int win1_start_x, int win1_end_x, int win1_start_y, int win1_end_y,
	int win2_start_x, int win2_end_x, int win2_start_y, int win2_end_y,
	int win3_start_x, int win3_end_x, int win3_start_y, int win3_end_y,
	int win4_start_x, int win4_end_x, int win4_start_y, int win4_end_y
	);

void enable_di_pre_aml(
	DI_MIF_t		*di_inp_mif,
	DI_MIF_t		*di_mem_mif,
	DI_MIF_t		*di_chan2_mif,
	DI_SIM_MIF_t	*di_nrwr_mif,
	DI_SIM_MIF_t	*di_mtnwr_mif,
#ifdef NEW_DI_V1
	DI_SIM_MIF_t    *di_contp2rd_mif,
	DI_SIM_MIF_t    *di_contprd_mif,
	DI_SIM_MIF_t    *di_contwr_mif,
#endif
	int nr_en, int mtn_en, int pd32_check_en, int pd22_check_en,
	int hist_check_en, int pre_field_num, int pre_vdin_link,
	int hold_line, int urgent
	);
void enable_afbc_input(struct vframe_s *vf);
#ifdef NEW_DI_V3
void enable_mc_di_pre(DI_MC_MIF_t *di_mcinford_mif,
	DI_MC_MIF_t *di_mcinfowr_mif, DI_MC_MIF_t *di_mcvecwr_mif, int urgent);
void enable_mc_di_post(DI_MC_MIF_t *di_mcvecrd_mif, int urgent, bool reverse);
#endif

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

void enable_di_post_2(
	DI_MIF_t		*di_buf0_mif,
	DI_MIF_t		*di_buf1_mif,
	DI_SIM_MIF_t	*di_diwr_mif,
	#ifndef NEW_DI_V2
	DI_SIM_MIF_t	*di_mtncrd_mif,
	#endif
	DI_SIM_MIF_t	*di_mtnprd_mif,
	int ei_en, int blend_en, int blend_mtn_en, int blend_mode,
	int di_vpp_en, int di_ddr_en,
	int post_field_num, int hold_line , int urgent
	#ifndef NEW_DI_V1
	, unsigned long *reg_mtn_info
	#endif
);

void di_post_switch_buffer(
	DI_MIF_t		*di_buf0_mif,
	DI_MIF_t		*di_buf1_mif,
	DI_SIM_MIF_t	*di_diwr_mif,
	#ifndef NEW_DI_V2
	DI_SIM_MIF_t	*di_mtncrd_mif,
	#endif
	DI_SIM_MIF_t	*di_mtnprd_mif,
	#ifdef NEW_DI_V3
	DI_MC_MIF_t		*di_mcvecrd_mif,
	#endif
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
void reset_pulldown_state(void);

void cal_pd_parameters(pulldown_detect_info_t *cur_info,
	pulldown_detect_info_t *pre_info, pulldown_detect_info_t *next_info,
	pd_detect_threshold_t *pd_th);

void pattern_check_pre_2(int idx, pulldown_detect_info_t *cur_info,
	pulldown_detect_info_t *pre_info, pulldown_detect_info_t *pre2_info,
	int *pre_pulldown_mode, int *pre2_pulldown_mode, int *type,
	pd_detect_threshold_t *pd_th);

void reset_di_para(void);
/* for video reverse */
void di_post_read_reverse(bool reverse);
void di_post_read_reverse_irq(bool reverse);

/* new pd algorithm */
void reset_pd_his(void);
void insert_pd_his(pulldown_detect_info_t *pd_info);
void reset_pd32_status(void);
int detect_pd32(void);

extern unsigned int pd32_match_num;
extern unsigned int pd32_debug_th;
extern unsigned int pd32_diff_num_0_th;
extern unsigned int pd22_th;
extern unsigned int pd22_num_th;

#undef DI_DEBUG

#define DI_LOG_MTNINFO		0x02
#define DI_LOG_PULLDOWN		0x10
#define DI_LOG_BUFFER_STATE		0x20
#define DI_LOG_TIMESTAMP		0x100
#define DI_LOG_PRECISE_TIMESTAMP		0x200
#define DI_LOG_QUEUE		0x40
#define DI_LOG_VFRAME		0x80

extern unsigned int di_log_flag;
extern unsigned int di_debug_flag;

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

unsigned char di_get_power_control(unsigned char type);

#define DI_COUNT   1

struct di_dev_s {
	dev_t			   devt;
	struct cdev		   cdev; /* The cdev structure */
	struct device	   *dev;
	struct task_struct *task;
	unsigned char	   di_event;
	unsigned int	   di_irq;
	unsigned int	   timerc_irq;
	unsigned long	   mem_start;
	unsigned int	   mem_size;
	unsigned int	   buffer_size;
	unsigned int	   buf_num_avail;
	unsigned int	   hw_version;
};
#define di_dev_t struct di_dev_s
#endif
