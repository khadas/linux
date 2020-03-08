// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/seq_file.h>
#include <linux/debugfs.h>

#include "di_data.h"
#include "di_dbg.h"

#include "di_reg_tab.h"
#include "deinterlace.h"
#include "deinterlace_dbg.h"
#include "deinterlace_hw.h"
#include "di_data_l.h"
#include "di_que.h"
#include "di_task.h"
#include "di_prc.h"
#include "di_pre.h"
#include "di_post.h"

/********************************
 *trace:
 *******************************/
#define CREATE_TRACE_POINTS
#include "dim_trace.h"

#undef TRACE_INCLUDE_PATH
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_PATH ../../drivers/amlogic/media/di_multi
#define TRACE_INCLUDE_FILE dim_trace
#include <trace/define_trace.h>

extern unsigned long long notrace sched_clock(void);

/**********************/
/* data get */
static struct dentry **dich_get_dbgroot(unsigned int ch)
{
	return &get_datal()->ch_data[ch].dbg_rootx;
}

static struct dentry **dich_get_dbgroot_top(void)
{
	return &get_datal()->dbg_root_top;
}

static int *di_get_plane(void)
{
	return &get_datal()->plane[0];
}

static struct vframe_s *di_get_dbg_vframe_in(unsigned int ch)
{
	return &get_datal()->ch_data[ch].dbg_data.vfm_input;
}

static struct vframe_s **di_get_dbg_vframe_out(unsigned int ch)
{
	return &get_datal()->ch_data[ch].dbg_data.pfm_out;
}

/********************************
 *timer:
 *******************************/

u64 cur_to_msecs(void)
{
	u64 cur = sched_clock();

	do_div(cur, NSEC_PER_MSEC);
	return cur;
}

u64 cur_to_usecs(void)/*2019*/
{
	u64 cur = sched_clock();

	do_div(cur, NSEC_PER_USEC);
	return cur;
}

/********************************
 *trace:
 *******************************/

static void trace_pre(unsigned int index, unsigned long ctime)
{
	trace_dim_pre("PRE-IRQ-0", index, ctime);
}

static void trace_post(unsigned int index, unsigned long ctime)
{
	trace_dim_post("POST-IRQ-1", index, ctime);
}

#define DI_TRACE_LIMIT		8
static void trace_pre_get(unsigned int index)
{
	u64 ustime;

	if (di_get_disp_cnt() > DI_TRACE_LIMIT)
		return;

	ustime = cur_to_usecs();
	trace_dim_pre_getxx("PRE-GET-01", index, ustime);
}

static void trace_pre_set(unsigned int index)
{
	u64 ustime;

	if (di_get_disp_cnt() > DI_TRACE_LIMIT)
		return;

	ustime = cur_to_usecs();
	trace_dim_pre_setxx("PRE-SET-01", index, ustime);
}

static void trace_pre_ready(unsigned int index)
{
	u64 ustime;

	if (di_get_disp_cnt() > DI_TRACE_LIMIT)
		return;

	ustime = cur_to_usecs();
	trace_dim_pre_ready("PRE-READY2", index, ustime);
}

static void trace_post_ready(unsigned int index)
{
	u64 ustime;

	if (di_get_disp_cnt() > DI_TRACE_LIMIT)
		return;

	ustime = cur_to_usecs();
	trace_dim_pst_ready("PST-READY3", index, ustime);
}

static void trace_post_get(unsigned int index)
{
	u64 ustime;

	if (di_get_disp_cnt() > DI_TRACE_LIMIT)
		return;

	ustime = cur_to_usecs();
	trace_dim_pst_getxx("PST-GET-04", index, ustime);
}

static void trace_post_get2(unsigned int index)
{
	u64 ustime;

	if (di_get_disp_cnt() > DI_TRACE_LIMIT)
		return;

	ustime = cur_to_usecs();
	trace_dim_pst_get2x("PST-GET-0a", index, ustime);
}

static void trace_post_set(unsigned int index)
{
	u64 ustime;

	if (di_get_disp_cnt() > DI_TRACE_LIMIT)
		return;

	ustime = cur_to_usecs();
	trace_dim_pst_setxx("PST-SET-05", index, ustime);
}

static void trace_post_irq(unsigned int index)
{
	u64 ustime;

	if (di_get_disp_cnt() > DI_TRACE_LIMIT)
		return;

	ustime = cur_to_usecs();
	trace_dim_pst_irxxx("PST-IRQ-06", index, ustime);
}

static void trace_post_doing(unsigned int index)
{
	u64 ustime;

	if (di_get_disp_cnt() > DI_TRACE_LIMIT)
		return;

	ustime = cur_to_usecs();
	trace_dim_pst_doing("PST-DOI-07", index, ustime);
}

static void trace_post_peek(unsigned int index)
{
	u64 ustime;

	if (di_get_disp_cnt() > DI_TRACE_LIMIT)
		return;

	ustime = cur_to_usecs();
	trace_dim_pst_peekx("PST-PEEK-8", index, ustime);
}

const struct dim_tr_ops_s dim_tr_ops = {
	.pre = trace_pre,
	.post = trace_post,

	.pre_get = trace_pre_get,
	.pre_set = trace_pre_set,
	.pre_ready = trace_pre_ready,
	.post_ready = trace_post_ready,
	.post_get = trace_post_get,
	.post_get2 = trace_post_get2,

	.post_set = trace_post_set,
	.post_ir = trace_post_irq,
	.post_do = trace_post_doing,
	.post_peek = trace_post_peek,
};

static unsigned int seq_get_channel(struct seq_file *s)
{
	int *pch;

	pch = (int *)s->private;
	return *pch;
}

/********************************
 *debug register:
 *******************************/
/* also see enum EDI_DBG_MOD */
const char * const dbg_mode_name[] = {
	"REGB",
	"REGE",
	"UNREGB",
	"UNREGE",
	"PRE_SETB",
	"PRE_SETE",
	"PRE_DONEB",
	"PRE_DONEE",
	"PST_SETB",
	"PST_SETE",
	"PST_IRQB",
	"PST_IRQE",
	"PST_DB",
	"PST_DE",
	"PST_CH_CHG",
	"PST_TOUT",
	"RVB",
	"RVE",
	"PST_RESIZE",
};

const char *ddbg_get_mod_name(unsigned int mod)
{
	if (mod >= EDI_DBG_MOD_END)
		return "nothing!";

	return dbg_mode_name[mod];
}

void ddbg_reg_save(unsigned int addr, unsigned int val,
		   unsigned int st, unsigned int bw)
{
	struct di_dbg_reg dbg_reg;
	struct di_dbg_reg_log *plog = get_dbg_reg_log();
	unsigned int pos;

	if (!plog->en_reg)
		return;
	if (plog->en_notoverwrite && plog->overflow)
		return;

	pos = plog->pos;

	dbg_reg.addr = addr;
	dbg_reg.val = val;
	dbg_reg.st_bit = st;
	dbg_reg.b_w = bw;
	dbg_reg.res = 0;

	plog->log[pos].reg = dbg_reg;
	pos++;
	if (pos >= (K_DI_SIZE_REG_LOG - 1)) {
		if (plog->en_notoverwrite) {
			plog->overflow = 1;

		} else {
			pos = 0;
			plog->overflow = 1;
		}
	}
	plog->wsize++;
	plog->pos = pos;
}

void dim_ddbg_mod_save(unsigned int mod, unsigned int ch, unsigned int cnt)
{
	struct di_dbg_mod dbg_mod;
	struct di_dbg_reg_log *plog = get_dbg_reg_log();
	unsigned int pos;

/*--------------------------*/
	if (ch)
		h_dbg_reg_set(mod | 0x80000000);
	else
		h_dbg_reg_set(mod);
/*--------------------------*/

	if (!plog->en_mod)
		return;
	if (plog->en_notoverwrite && plog->overflow)
		return;
	pos = plog->pos;

	dbg_mod.lable = K_DI_LAB_MOD;
	dbg_mod.ch = ch;
	dbg_mod.mod = mod;
	dbg_mod.cnt = cnt;
	dbg_mod.res = 0;

	plog->log[pos].mod = dbg_mod;
	pos++;
	if (pos >= (K_DI_SIZE_REG_LOG - 1)) {
		if (plog->en_notoverwrite) {
			plog->overflow = 1;

		} else {
			pos = 0;
			plog->overflow = 1;
		}
	}
	plog->wsize++;
	plog->pos = pos;
}

#ifdef MARK_HIS
void ddbg_sw(bool on)
{
	struct di_dbg_reg_log *plog = get_dbg_reg_log();

	plog->en = on;
}
#else

void ddbg_sw(enum EDI_LOG_TYPE mode, bool on)
{
	struct di_dbg_reg_log *plog = get_dbg_reg_log();

	switch (mode) {
	case EDI_LOG_TYPE_ALL:
		plog->en_all = on;
		break;
	case EDI_LOG_TYPE_REG:
		plog->en_reg = on;
		break;
	case EDI_LOG_TYPE_MOD:
		plog->en_mod = on;
		break;
	default:
		PR_WARN("%s:mode overlow:%d\n", __func__, mode);
		break;
	}
}
#endif
void ddbg_reg_clear(void)
{
	struct di_dbg_reg_log *plog = get_dbg_reg_log();

	memset(plog, 0, sizeof(struct di_dbg_reg_log));
	plog->en_notoverwrite = 1;
}

static int ddbg_log_reg_show(struct seq_file *seq, void *v)
{
	struct di_dbg_reg_log *plog = get_dbg_reg_log();
	unsigned int pos;
	int i;

	if (plog->overflow)
		pos = K_DI_SIZE_REG_LOG;
	else
		pos = plog->pos;

	seq_printf(seq, "%s:pos=%d,overflow=%d, size=%d\n",
		   __func__, plog->pos, plog->overflow, plog->wsize);

	for (i = 0; i < pos; i++) {
		if (plog->log[i].mod.lable == K_DI_LAB_MOD) {
			seq_printf(seq, "%d,ch[%d]:cnt[%d]:%s\n",
				   i,
				   plog->log[i].mod.ch,
				   plog->log[i].mod.cnt,
				   ddbg_get_mod_name(plog->log[i].mod.mod));
			continue;
		}

		seq_printf(seq, "\t0x%x,0x%x,%d,%d\n",
			   plog->log[i].reg.addr,
			   plog->log[i].reg.val,
			   plog->log[i].reg.st_bit,
			   plog->log[i].reg.b_w);
	}

	return 0;
}

static ssize_t ddbg_log_reg_store(struct file *file, const char __user *userbuf,
				  size_t count, loff_t *ppos)
{
	unsigned int item, val;
	char buf[80];
	int ret;

	count = min_t(size_t, count, (sizeof(buf) - 1));
	if (copy_from_user(buf, userbuf, count))
		return -EFAULT;

	buf[count] = 0;

	ret = sscanf(buf, "%i %i", &item, &val);

	switch (ret) {
	case 2:
		if (item == 0 && val == 0) {
			/*help info:*/
			pr_info("help:\n");
			/*all:*/
			pr_info("\t1 0 : log all disable\n");
			pr_info("\t1 1 : log all enable\n");
			/*reg:*/
			pr_info("\t2 0 : log reg disable\n");
			pr_info("\t2 1 : log reg enable\n");
			/*mod:*/
			pr_info("\t3 0 : log mod disable\n");
			pr_info("\t3 1 : log mod enable\n");
			/*clean:*/
			pr_info("\t4 0 : clear\n");
			break;
		}
		switch (item) {
		case 1:
		case 2:
		case 3:
			pr_info("ddbg_sw: %d\n", val);
			ddbg_sw(item, val);
			break;
		case 4:
			pr_info("ddbg_reg_clear\n");
			ddbg_reg_clear();
			break;
		};

		break;
	default:
		pr_info("err:please enter: 0 x for help\n");
		return -EINVAL;
	}

	return count;
}

/**********************/
static int seq_file_vframe(struct seq_file *seq, void *v, struct vframe_s *pvfm)
{
	if (!pvfm) {
		seq_puts(seq, "war: dump vframe NULL\n");
		return 0;
	}
	seq_printf(seq, "%-15s:0x%p\n", "addr", pvfm);
	seq_printf(seq, "%-15s:%d\n", "index", pvfm->index);
	seq_printf(seq, "%-15s:%d\n", "index_disp", pvfm->index_disp);
	seq_printf(seq, "%-15s:%d\n", "omx_index", pvfm->omx_index);
	seq_printf(seq, "%-15s:0x%x\n", "type", pvfm->type);
	seq_printf(seq, "%-15s:0x%x\n", "type_backup", pvfm->type_backup);
	seq_printf(seq, "%-15s:0x%x\n", "type_original", pvfm->type_original);
	seq_printf(seq, "%-15s:%d\n", "blend_mode", pvfm->blend_mode);
	seq_printf(seq, "%-15s:%d\n", "duration",  pvfm->duration);
	seq_printf(seq, "%-15s:%d\n", "duration_pull", pvfm->duration_pulldown);
	seq_printf(seq, "%-15s:%d\n", "pts", pvfm->pts);

	seq_printf(seq, "%-15s:%lld\n", "pts_us64", pvfm->pts_us64);
	seq_printf(seq, "%-15s:%d\n", "next_vf_pts_valid",
		   pvfm->next_vf_pts_valid);
	seq_printf(seq, "%-15s:%d\n", "next_vf_pts", pvfm->next_vf_pts);
	seq_printf(seq, "%-15s:%d\n", "disp_pts", pvfm->disp_pts);
	seq_printf(seq, "%-15s:%lld\n", "disp_pts_us64", pvfm->disp_pts_us64);
	seq_printf(seq, "%-15s:%lld\n", "timestamp", pvfm->timestamp);
	seq_printf(seq, "%-15s:%d\n", "flag", pvfm->flag);
	seq_printf(seq, "%-15s:0x%x\n", "canvas0Addr", pvfm->canvas0Addr);
	seq_printf(seq, "%-15s:0x%x\n", "canvas1Addr", pvfm->canvas1Addr);
	seq_printf(seq, "%-15s:0x%x\n", "compHeadAddr", pvfm->compHeadAddr);
	seq_printf(seq, "%-15s:0x%x\n", "compBodyAddr", pvfm->compBodyAddr);
	seq_printf(seq, "%-15s:%d\n", "plane_num", pvfm->plane_num);

	seq_printf(seq, "%-15s:%d\n", "bufWidth", pvfm->bufWidth);
	seq_printf(seq, "%-15s:%d\n", "width", pvfm->width);
	seq_printf(seq, "%-15s:%d\n", "height", pvfm->height);
	seq_printf(seq, "%-15s:%d\n", "compWidth", pvfm->compWidth);
	seq_printf(seq, "%-15s:%d\n", "compHeight", pvfm->compHeight);
	seq_printf(seq, "%-15s:%d\n", "ratio_control", pvfm->ratio_control);
	seq_printf(seq, "%-15s:%d\n", "bitdepth", pvfm->bitdepth);
	seq_printf(seq, "%-15s:%d\n", "signal_type", pvfm->signal_type);

	/*
	 *	   bit 29: present_flag
	 *	   bit 28-26: video_format
	 *	   "component", "PAL", "NTSC", "SECAM",
	 *	   "MAC", "unspecified"
	 *	   bit 25: range "limited", "full_range"
	 *	   bit 24: color_description_present_flag
	 *	   bit 23-16: color_primaries
	 *	   "unknown", "bt709", "undef", "bt601",
	 *	   "bt470m", "bt470bg", "smpte170m", "smpte240m",
	 *	   "film", "bt2020"
	 *	   bit 15-8: transfer_characteristic
	 *	   "unknown", "bt709", "undef", "bt601",
	 *	   "bt470m", "bt470bg", "smpte170m", "smpte240m",
	 *	   "linear", "log100", "log316", "iec61966-2-4",
	 *	   "bt1361e", "iec61966-2-1", "bt2020-10", "bt2020-12",
	 *	   "smpte-st-2084", "smpte-st-428"
	 *	   bit 7-0: matrix_coefficient
	 *	   "GBR", "bt709", "undef", "bt601",
	 *	   "fcc", "bt470bg", "smpte170m", "smpte240m",
	 *	   "YCgCo", "bt2020nc", "bt2020c"
	 */
	seq_printf(seq, "%-15s:0x%x\n", "orientation", pvfm->orientation);
	seq_printf(seq, "%-15s:0x%x\n", "video_angle", pvfm->video_angle);
	seq_printf(seq, "%-15s:0x%x\n", "source_type", pvfm->source_type);

	seq_printf(seq, "%-15s:0x%x\n", "phase", pvfm->phase);
	seq_printf(seq, "%-15s:0x%x\n", "source_mode", pvfm->source_mode);
	seq_printf(seq, "%-15s:0x%x\n", "sig_fmt", pvfm->sig_fmt);
	seq_printf(seq, "%-15s:0x%x\n", "trans_fmt", pvfm->trans_fmt);

	seq_printf(seq, "%-15s:0x%x\n", "mode_3d_enable",
		   pvfm->mode_3d_enable);

	seq_printf(seq, "%-15s:0x%p\n", "early_process_fun",
		   pvfm->early_process_fun);
	seq_printf(seq, "%-15s:0x%p\n", "process_fun",
		   pvfm->early_process_fun);
	seq_printf(seq, "%-15s:0x%p\n", "private_data",
		   pvfm->early_process_fun);

	/* vframe properties */

	/* pixel aspect ratio */
	seq_printf(seq, "%-15s:%d\n", "pixel_ratio", pvfm->pixel_ratio);

	/* ready from decode on  jiffies_64 */
	seq_printf(seq, "%-15s:%d\n", "use_cnt", atomic_read(&pvfm->use_cnt));
	seq_printf(seq, "%-15s:%d\n", "frame_dirty", pvfm->frame_dirty);
	/*
	 *prog_proc_config:
	 *1: process p from decoder as filed
	 *0: process p from decoder as frame
	 */
	seq_printf(seq, "%-15s:0x%x\n", "prog_proc_config",
		   pvfm->prog_proc_config);
		/* used for indicate current video is motion or static */
	seq_printf(seq, "%-15s:%d\n", "combing_cur_lev",
		   pvfm->combing_cur_lev);
	return 0;
}

/**********************/
/* debug input vframe */
/**********************/
void didbg_vframe_in_copy(unsigned int ch, struct vframe_s *pvfm)
{
	struct vframe_s *pvfm_t;

	if (!di_cfgx_get(ch, EDI_DBG_CFGX_IDX_VFM_IN))
		return;

	pvfm_t = di_get_dbg_vframe_in(ch);

	memcpy(pvfm_t, pvfm, sizeof(struct vframe_s));
}

static int seq_file_vframe_in_show(struct seq_file *seq, void *v)
{
	unsigned int ch;

	ch = seq_get_channel(seq);
	seq_printf(seq, "%s:ch[%d]\n", __func__, ch);

	if (!di_cfgx_get(ch, EDI_DBG_CFGX_IDX_VFM_IN)) {
		seq_puts(seq, "war: cfg[EDI_DBG_CFGX_IDX_VFM_IN] disable\n");
		return 0;
	}

	seq_file_vframe(seq, v, di_get_dbg_vframe_in(ch));

	return 0;
}

/***********************/
/* debug output vframe */
/***********************/
void didbg_vframe_out_save(struct vframe_s *pvfm)
{
	unsigned int ch;
	struct vframe_s **pvfm_t;

	ch = DI_SUB_ID_S0;
	if (!di_cfgx_get(ch, EDI_DBG_CFGX_IDX_VFM_OT))
		return;

	pvfm_t = di_get_dbg_vframe_out(ch);
	*pvfm_t = pvfm;
}

static int seq_file_vframe_out_show(struct seq_file *seq, void *v)
{
	unsigned int ch;

	ch = seq_get_channel(seq);
	seq_printf(seq, "%s:ch[%d]\n", __func__, ch);

	if (!di_cfgx_get(ch, EDI_DBG_CFGX_IDX_VFM_OT)) {
		seq_puts(seq, "war: cfg[EDI_DBG_CFGX_IDX_VFM_OT] disable\n");
		return 0;
	}

	seq_file_vframe(seq, v, *di_get_dbg_vframe_out(ch));

	return 0;
}

/**********************/
/* debug vframe type  */
/**********************/
const struct di_vframe_type_info di_vtype_info[] = {
	{"interlace", VIDTYPE_INTERLACE, NULL},
	{"bottom", VIDTYPE_INTERLACE_BOTTOM, NULL},

	{"interllace first", VIDTYPE_INTERLACE_FIRST, NULL},
	{"mvc", VIDTYPE_MVC, NULL},
	{"no video en", VIDTYPE_NO_VIDEO_ENABLE, NULL},
	{"v422", VIDTYPE_VIU_422, NULL},
	{"field", VIDTYPE_VIU_FIELD, NULL},

	{"single plane", VIDTYPE_VIU_SINGLE_PLANE, NULL},
	{"v444", VIDTYPE_VIU_444, NULL},
	{"nv21", VIDTYPE_VIU_NV21, NULL},
	{"vscale disable", VIDTYPE_VSCALE_DISABLE, NULL},
	{"cvs toggle", VIDTYPE_CANVAS_TOGGLE, NULL},
	{"pre interlace", VIDTYPE_PRE_INTERLACE, NULL},
	{"high run", VIDTYPE_HIGHRUN, NULL},
	{"compress", VIDTYPE_COMPRESS, NULL},
	{"pic", VIDTYPE_PIC, NULL},
	{"scatter", VIDTYPE_SCATTER, NULL},
	{"vd2", VIDTYPE_VD2, NULL},
	{"compress loss", VIDTYPE_COMPRESS_LOSS, NULL},
	{"comb", VIDTYPE_COMB_MODE, NULL},
	{"tb detect", TB_DETECT_MASK, NULL},

	/*finish*/
	{NULL, TABLE_FLG_END, NULL},
};

static void didbg_vtype_set(unsigned int type)
{
	get_datal()->dbg_data.vframe_type = type;
}

static unsigned int didbg_vtype_get(void)
{
	return get_datal()->dbg_data.vframe_type;
}

static int seq_file_vtype_show(struct seq_file *seq, void *v)
{
	unsigned int vtype;
	int i;
	unsigned int mask;

	i = 0;
	vtype = didbg_vtype_get();

	seq_printf(seq, "%s:vtype[0x%x]\n", __func__, vtype);

	while (di_vtype_info[i].name) {
		mask = di_vtype_info[i].mask;

		if ((vtype & mask) == mask) {
			seq_printf(seq, "\t%-15s:y\n", di_vtype_info[i].name);
		} else {
			if (di_vtype_info[i].other) {
				seq_printf(seq, "\t%-15s:yes\n",
					   di_vtype_info[i].other);
			} else {
				seq_printf(seq, "\t%-15s:no\n",
					   di_vtype_info[i].name);
			}
		}
		i++;
	}

	return 0;
}

ssize_t seq_file_vtype_store(struct file *file, const char __user *userbuf,
			     size_t count, loff_t *ppos)
{
	char buf[20];
	int ret;

	unsigned int vtype;

	count = min_t(size_t, count, (sizeof(buf) - 1));
	if (copy_from_user(buf, userbuf, count))
		return -EFAULT;

	buf[count] = 0;
	/*reg, bit, width, val*/
	#ifdef MARK_HIS
	ret = sscanfxxx(buf, "%x", &vtype);

	/*--------------------------*/

	switch (ret) {
	case 1:
		pr_info("save type:0x%x", vtype);
		didbg_vtype_set(vtype);
		break;
	default:
		pr_info("war:please enter vtype\n");
		break;
	}

	#else
	ret = kstrtouint(buf, 0, &vtype);
	if (ret) {
		pr_info("war:please enter vtype\n");
		return 0;
	}
	pr_info("save type:0x%x", vtype);
	didbg_vtype_set(vtype);
	#endif
	return count;
}

/**************************************
 *
 * show vframe current
 *
 **************************************/
static int seq_file_curr_vframe_show(struct seq_file *seq, void *v)
{
	unsigned int ch;
	struct di_buf_s *p = NULL;
	struct vframe_s *pvfm;
	char *splt = "---------------------------";
	char *splt2 = "-------------";
	int itmp;
	unsigned int tmpa[MAX_FIFO_SIZE];
	unsigned int psize;

	ch = seq_get_channel(seq);
	seq_printf(seq, "%s:ch[%d]\n", __func__, ch);

	seq_printf(seq, "%s\n", splt);

	/********************************/
	/* post_doing_list		*/
	/********************************/
	seq_puts(seq, "vfm for: post_doing_list:\n");
	//queue_for_each_entry(p, ch, QUEUE_POST_DOING, list) {
	di_que_list(ch, QUE_POST_DOING, &tmpa[0], &psize);
	for (itmp = 0; itmp < psize; itmp++) {
		p = pw_qindex_2_buf(ch, tmpa[itmp]);
		pvfm = p->vframe;
		seq_file_vframe(seq, v, pvfm);
		seq_printf(seq, "%s\n", splt2);
	}
	seq_printf(seq, "%s\n", splt);

	/********************************/
	/* pre_ready_list		*/
	/********************************/
	seq_puts(seq, "pre_ready_list:\n");
	di_que_list(ch, QUE_PRE_READY, &tmpa[0], &psize);
	for (itmp = 0; itmp < psize; itmp++) {
		p = pw_qindex_2_buf(ch, tmpa[itmp]);

		pvfm = p->vframe;
		seq_file_vframe(seq, v, pvfm);
		seq_printf(seq, "%s\n", splt2);
	}
	seq_printf(seq, "%s\n", splt);

	/********************************/
	/* post_ready_list		*/
	/********************************/
	di_que_list(ch, QUE_POST_READY, &tmpa[0], &psize);
	seq_printf(seq, "post_ready_list: curr(%d)\n", psize);

	for (itmp = 0; itmp < psize; itmp++) {
		p = pw_qindex_2_buf(ch, tmpa[itmp]);
		pvfm = p->vframe;
		seq_file_vframe(seq, v, pvfm);
		seq_printf(seq, "%s\n", splt2);
	}
	seq_printf(seq, "%s\n", splt);

	/********************************/
	/* display_list			*/
	/********************************/
	seq_puts(seq, "display_list:\n");
	queue_for_each_entry(p, ch, QUEUE_DISPLAY, list) {
		pvfm = p->vframe;
		seq_file_vframe(seq, v, pvfm);
		seq_printf(seq, "%s\n", splt2);
	}
	seq_printf(seq, "%s\n", splt);

	return 0;
}

/**************************************
 *
 * summmary variable
 *
 **************************************/
static int seq_file_sum_show(struct seq_file *seq, void *v)
{
	unsigned int ch;
	char *sname;
	unsigned int val;
	unsigned int tsize;
	int i;

	ch = seq_get_channel(seq);

	tsize = di_sum_get_tab_size();/*ARRAY_SIZE(di_sum_tab);*/

	seq_printf(seq, "%s:ch[%d]\n", __func__, ch);
	for (i = 0; i < tsize; i++) {
		if (!di_sum_check(ch, i))
			continue;
		di_sum_get_info(ch, i, &sname, &val);
		seq_printf(seq, "\t%-2d:%-15s:%d\n", i, sname, val);
	}

	seq_printf(seq, "%s:finish\n", __func__);

	return 0;
}

/********************************/
/* state */
/********************************/

static int seq_file_state_show(struct seq_file *seq, void *v)
{
	unsigned int ch;

	ch = seq_get_channel(seq);
	seq_printf(seq, "%s:ch[%d]\n", __func__, ch);

	dim_state_show(seq, v, ch);

	return 0;
}

static int seq_file_mif_show(struct seq_file *seq, void *v)
{
	unsigned int ch;

	ch = seq_get_channel(seq);
	seq_printf(seq, "%s:ch[%d]\n", __func__, ch);

	dim_dump_mif_size_state_show(seq, v, ch);

	return 0;
}

/********************************/
#define DEFINE_SEQ_SHOW_ONLY(__name) \
static int __name ## _open(struct inode *inode, struct file *file)	\
{ \
	return single_open(file, __name ## _show, inode->i_private);	\
} \
									\
static const struct file_operations __name ## _fops = {			\
	.owner = THIS_MODULE,		\
	.open = __name ## _open,	\
	.read = seq_read,		\
	.llseek = seq_lseek,		\
	.release = single_release,	\
}

/*--------------------------*/
/*note: this define can't used for x*/
#define DEFINE_SEQ_SHOW_STORE(__name) \
static int __name ## _open(struct inode *inode, struct file *file)	\
{ \
	return single_open(file, __name ## _show, inode->i_private);	\
} \
									\
static const struct file_operations __name ## _fops = {			\
	.owner = THIS_MODULE,		\
	.open = __name ## _open,	\
	.read = seq_read,		\
	.write = __name ## _store,	\
	.llseek = seq_lseek,		\
	.release = single_release,	\
}

/*--------------------------*/

#define DEFINE_SHOW_STORE(__name) \
static const struct file_operations __name ## _fops = {			\
	.owner = THIS_MODULE,		\
	.open = simple_open,	\
	.read = __name ## _show,		\
	.write = __name ## _store,	\
}

/*--------------------------*/
#define DEFINE_STORE_ONLY(__name) \
static const struct file_operations __name ## _fops = {			\
	.owner = THIS_MODULE,		\
	.open = simple_open,	\
	.read = NULL,		\
	.write = __name ## _store,	\
}

/**********************/

static int rcfgx_show(struct seq_file *s, void *what)
{
	int i;
	int *pch;

	pch = (int *)s->private;

	seq_printf(s, "%s:ch[%d]\n", __func__, *pch);

	for (i = EDI_CFGX_BEGIN; i < EDI_DBG_CFGX_END; i++) {
		seq_printf(s, "\tidx[%2d]:%-15s:%d\n", i,
			   di_cfgx_get_name(i),
			   di_cfgx_get(*pch, i));
	}

	return 0;
}

/*************************************************************/
static ssize_t wcfgx_store(struct file *file, const char __user *userbuf,
			   size_t count, loff_t *ppos)
{
	unsigned int item, val;
	char buf[80];
	int ret;
	int *pch;

	count = min_t(size_t, count, (sizeof(buf) - 1));
	if (copy_from_user(buf, userbuf, count))
		return -EFAULT;

	buf[count] = 0;

	ret = sscanf(buf, "%i %i", &item, &val);

	pch = (int *)file->private_data;
	pr_info("%s:ch[%d]\n", __func__, *pch);

	switch (ret) {
	case 2:
		if (item <= EDI_CFGX_BEGIN	||
		    item >= EDI_DBG_CFGX_END) {
			pr_info("war:cfg_item is overflow[%d,%d]:%d\n",
				EDI_CFGX_BEGIN,
				EDI_DBG_CFGX_END,
				item);
			break;
		}
		if (val > 1)
			pr_info("war:cfg value[%d] is not bool\n", val);

		pr_info("change cfg:%s\n", di_cfgx_get_name(item));
		pr_info("\t%d -> %d\n", di_cfgx_get(*pch, item), val);
		di_cfgx_set(*pch, item, val);
		break;
	default:
		pr_info("err:please enter: cfg_item, value(bool)\n");
		return -EINVAL;
	}

	return count;
}

/***********************************************
 *
 ***********************************************/
ssize_t keep_buf_clear_store(struct file *file, const char __user *userbuf,
			     size_t count, loff_t *ppos)
{
	char buf[20];
	int ret;

	unsigned int ch;

	count = min_t(size_t, count, (sizeof(buf) - 1));
	if (copy_from_user(buf, userbuf, count))
		return -EFAULT;

	buf[count] = 0;
	/*reg, bit, width, val*/

	ret = kstrtouint(buf, 0, &ch);
	if (ret) {
		pr_info("war:please enter ch\n");
		return 0;
	}
	pr_info("ch:%d", ch);

	if (ch >= DI_CHANNEL_NUB) {
		PR_ERR("%s:ch is overflow %d\n", __func__, ch);
		return 0;
	}

	dim_dbg_release_keep_all(ch);

	return count;
}

static int cfgt_help_show(struct seq_file *s, void *what)
{
	seq_puts(s, "cat list\n");
	seq_printf(s, "\t%-10s:%s\n", "cfg_ai", "all cfg infor");
	seq_printf(s, "\t%-10s:%s\n", "cfg_av", "all cfg val");
	seq_printf(s, "\t%-10s:%s\n", "cfg_one", "sel val or infor");
	seq_printf(s, "\t%-10s:%s\n", "cfg_sel", "sel infor");
	seq_puts(s, "echo list\n");
	seq_printf(s, "\t%-10s:%s\n", "val > cfgw_one",
		   "change cfg that have sel");
	seq_printf(s, "\t%-10s:%s\n", "index val > cfgw_index",
		   "change cfg by index");
	seq_printf(s, "\t%-10s:%s\n", "mode sel(0/1) index > cfgw_sel",
		   "change sel");
	return 0;
}

static int cfgt_itme_all_show(struct seq_file *s, void *what)
{
	di_cfgt_show_item_all(s);
	return 0;
}

static int cfgt_val_all_show(struct seq_file *s, void *what)
{
	di_cfgt_show_val_all(s);
	return 0;
}

static int cfgt_one_show(struct seq_file *s, void *what)
{
	if (get_datal()->cfg_dbg_mode)
		di_cfgt_show_item_sel(s);
	else
		di_cfgt_show_val_sel(s);
	return 0;
}

static int cfgt_sel_show(struct seq_file *s, void *what)
{
	unsigned int i;

	i = get_datal()->cfg_sel;
	seq_printf(s, "mode[%d]:index[%d]\n",
		   get_datal()->cfg_dbg_mode, i);
	if (!di_cfg_top_check(i))
		return 0;
	seq_printf(s, "%s\n", di_cfg_top_get_name(i));
	return 0;
}

static ssize_t cfgt_sel_store(struct file *file, const char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	unsigned int sel, index;
	char buf[80];
	int ret;

	count = min_t(size_t, count, (sizeof(buf) - 1));
	if (copy_from_user(buf, userbuf, count))
		return -EFAULT;
	buf[count] = 0;
	ret = sscanf(buf, "%i %i", &sel, &index);
	switch (ret) {
	case 2:
		di_cfgt_set_sel(sel, index);
		break;
	default:
		pr_info("err:please enter: cfg_item, index\n");
		return -EINVAL;
	}
	return count;
}

static ssize_t cfgtw_id_store(struct file *file, const char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	unsigned int index, val;
	char buf[80];
	int ret;

	count = min_t(size_t, count, (sizeof(buf) - 1));
	if (copy_from_user(buf, userbuf, count))
		return -EFAULT;
	buf[count] = 0;
	ret = sscanf(buf, "%i %i", &index, &val);
	switch (ret) {
	case 2:
		if (!di_cfg_top_check(index))
			break;
		pr_info("%s:%d->%d\n",
			di_cfg_top_get_name(index),
			di_cfg_top_get(index),
			val);
		di_cfg_top_set(index, val);
		break;
	default:
		pr_info("err:please enter: cfg_item, index\n");
		return -EINVAL;
	}
	return count;
}

static ssize_t cfgt_one_store(struct file *file, const char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	unsigned int index, val;
	char buf[80];
	int ret;

	count = min_t(size_t, count, (sizeof(buf) - 1));
	if (copy_from_user(buf, userbuf, count))
		return -EFAULT;
	buf[count] = 0;
	ret = kstrtouint(buf, 0, &val);
	if (ret) {
		pr_info("war:please enter val\n");
		return 0;
	}
	index = get_datal()->cfg_sel;
	if (!di_cfg_top_check(index))
		return count;
	pr_info("%s:%d->%d\n",
		di_cfg_top_get_name(index),
		di_cfg_top_get(index),
		val);
	di_cfg_top_set(index, val);
	return count;
}

/***************************************************************
 * parameter show and store for top : DI
 **************************************************************/
static int mpr_di_show(struct seq_file *s, void *what)
{
	int i;

	seq_printf(s, "%s:\n", __func__);

	for (i = EDI_MP_SUB_DI_B; i < EDI_MP_SUB_DI_E; i++) {
		seq_printf(s, "\tidx[%2d]:%-15s:%d\n",
			   i - EDI_MP_SUB_DI_B,
			   di_mp_uit_get_name(i),
			   di_mp_uit_get(i));
	}

	return 0;
}

static ssize_t mpw_di_store(struct file *file, const char __user *userbuf,
			    size_t count, loff_t *ppos)
{
	unsigned int item, rid;
	char buf[80];
	int ret, val;

	count = min_t(size_t, count, (sizeof(buf) - 1));
	if (copy_from_user(buf, userbuf, count))
		return -EFAULT;

	buf[count] = 0;

	ret = sscanf(buf, "%i %i", &item, &val);

	pr_info("%s:\n", __func__);

	switch (ret) {
	case 2:
		/*check []*/
		if (item >= (EDI_MP_SUB_DI_E - EDI_MP_SUB_DI_B)) {
			PR_WARN("index is overflow[%d,%d]:%d\n",
				0,
				EDI_MP_SUB_DI_E - EDI_MP_SUB_DI_B,
				item);
			break;
		}
		rid = item + EDI_MP_SUB_DI_B;
		pr_info("change mp :%s\n",
			di_mp_uit_get_name(rid));
		pr_info("\t%d -> %d\n", di_mp_uit_get(rid), val);
		di_mp_uit_set(rid, val);
		break;
	default:
		PR_ERR("please enter: id, value(int)\n");
		return -EINVAL;
	}

	return count;
}

/***************************************************************
 * parameter show and store for top : nr
 **************************************************************/
static int mpr_nr_show(struct seq_file *s, void *what)
{
	int i;

	seq_printf(s, "%s:\n", __func__);

	for (i = EDI_MP_SUB_NR_B; i < EDI_MP_SUB_NR_E; i++) {
		seq_printf(s, "\tidx[%2d]:%-15s:%d\n",
			   i - EDI_MP_SUB_NR_B,
			   di_mp_uit_get_name(i),
			   di_mp_uit_get(i));
	}

	return 0;
}

static ssize_t mpw_nr_store(struct file *file, const char __user *userbuf,
			    size_t count, loff_t *ppos)
{
	unsigned int item, rid;
	char buf[80];
	int ret, val;

	count = min_t(size_t, count, (sizeof(buf) - 1));
	if (copy_from_user(buf, userbuf, count))
		return -EFAULT;

	buf[count] = 0;

	ret = sscanf(buf, "%i %i", &item, &val);

	pr_info("%s:\n", __func__);

	switch (ret) {
	case 2:
		/*check []*/
		if (item >= (EDI_MP_SUB_NR_E - EDI_MP_SUB_NR_B)) {
			PR_WARN("index is overflow[%d,%d]:%d\n",
				0,
				EDI_MP_SUB_NR_E - EDI_MP_SUB_NR_B,
				item);
			break;
		}
		rid = item + EDI_MP_SUB_NR_B;
		pr_info("change mp:%s\n",
			di_mp_uit_get_name(rid));
		pr_info("\t%d -> %d\n", di_mp_uit_get(rid), val);
		di_mp_uit_set(rid, val);
		break;
	default:
		PR_ERR("please enter: id, value(int)\n");
		return -EINVAL;
	}

	return count;
}

/***************************************************************
 * parameter show and store for top : pulldown
 **************************************************************/
static int mpr_pd_show(struct seq_file *s, void *what)
{
	int i;

	seq_printf(s, "%s:\n", __func__);

	for (i = EDI_MP_SUB_PD_B; i < EDI_MP_SUB_PD_E; i++) {
		seq_printf(s, "\tidx[%2d]:%-15s:%d\n",
			   i - EDI_MP_SUB_PD_B,
			   di_mp_uit_get_name(i),
			   di_mp_uit_get(i));
	}

	return 0;
}

static ssize_t mpw_pd_store(struct file *file, const char __user *userbuf,
			    size_t count, loff_t *ppos)
{
	unsigned int item, rid;
	char buf[80];
	int ret, val;

	count = min_t(size_t, count, (sizeof(buf) - 1));
	if (copy_from_user(buf, userbuf, count))
		return -EFAULT;

	buf[count] = 0;

	ret = sscanf(buf, "%i %i", &item, &val);

	pr_info("%s:\n", __func__);

	switch (ret) {
	case 2:
		/*check []*/
		if (item >= (EDI_MP_SUB_PD_E - EDI_MP_SUB_PD_B)) {
			PR_WARN("index is overflow[%d,%d]:%d\n",
				0,
				EDI_MP_SUB_PD_E - EDI_MP_SUB_PD_B,
				item);
			break;
		}
		rid = item + EDI_MP_SUB_PD_B;
		pr_info("change mp:%s\n",
			di_mp_uit_get_name(rid));
		pr_info("\t%d -> %d\n", di_mp_uit_get(rid), val);
		di_mp_uit_set(rid, val);
		break;
	default:
		PR_ERR("please enter: id, value(int)\n");
		return -EINVAL;
	}

	return count;
}

/***************************************************************
 * parameter show and store for top : mtn
 **************************************************************/
static int mpr_mtn_show(struct seq_file *s, void *what)
{
	int i;

	seq_printf(s, "%s:\n", __func__);

	for (i = EDI_MP_SUB_MTN_B; i < EDI_MP_SUB_MTN_E; i++) {
		seq_printf(s, "\tidx[%2d]:%-15s:%d\n",
			   i - EDI_MP_SUB_MTN_B,
			   di_mp_uit_get_name(i),
			   di_mp_uit_get(i));
	}

	return 0;
}

static ssize_t mpw_mtn_store(struct file *file, const char __user *userbuf,
			     size_t count, loff_t *ppos)
{
	unsigned int item, rid;
	char buf[80];
	int ret, val;

	count = min_t(size_t, count, (sizeof(buf) - 1));
	if (copy_from_user(buf, userbuf, count))
		return -EFAULT;

	buf[count] = 0;

	ret = sscanf(buf, "%i %i", &item, &val);

	pr_info("%s:\n", __func__);

	switch (ret) {
	case 2:
		/*check []*/
		if (item >= (EDI_MP_SUB_MTN_E - EDI_MP_SUB_MTN_B)) {
			PR_WARN("index is overflow[%d,%d]:%d\n",
				0,
				EDI_MP_SUB_MTN_E - EDI_MP_SUB_MTN_B,
				item);
			break;
		}
		rid = item + EDI_MP_SUB_MTN_B;
		pr_info("change mp:%s\n",
			di_mp_uit_get_name(rid));
		pr_info("\t%d -> %d\n", di_mp_uit_get(rid), val);
		di_mp_uit_set(rid, val);
		break;
	default:
		PR_ERR("please enter: id, value(int)\n");
		return -EINVAL;
	}

	return count;
}

/**********************/
static int mpxr_show(struct seq_file *s, void *what)
{
	int i;
	int *pch;

	pch = (int *)s->private;

	seq_printf(s, "%s:ch[%d]\n", __func__, *pch);

	for (i = EDI_MP_UIX_BEGIN; i < EDI_MP_UIX_END; i++) {
		seq_printf(s, "\tidx[%2d]:%-15s:%d\n", i,
			   di_mp_uix_get_name(i),
			   di_mp_uix_get(*pch, i));
	}

	return 0;
}

/*************************************************************/
static ssize_t mpxw_store(struct file *file, const char __user *userbuf,
			  size_t count, loff_t *ppos)
{
	unsigned int item, val;
	char buf[80];
	int ret;
	int *pch;

	count = min_t(size_t, count, (sizeof(buf) - 1));
	if (copy_from_user(buf, userbuf, count))
		return -EFAULT;

	buf[count] = 0;

	ret = sscanf(buf, "%i %i", &item, &val);

	pch = (int *)file->private_data;
	pr_info("%s:ch[%d]\n", __func__, *pch);

	switch (ret) {
	case 2:
		if (item <= EDI_MP_UIX_BEGIN	||
		    item >= EDI_MP_UIX_END) {
			PR_WARN("mpxw is overflow[%d,%d]:%d\n",
				EDI_MP_UIX_BEGIN,
				EDI_MP_UIX_END,
				item);
			break;
		}

		pr_info("change mp ch[%d]:%s\n", *pch,
			di_mp_uix_get_name(item));
		pr_info("\t%d -> %d\n", di_mp_uix_get(*pch, item), val);
		di_mp_uix_set(*pch, item, val);
		break;
	default:
		PR_ERR("please enter: mpxw, value(unsigned int)\n");
		return -EINVAL;
	}

	return count;
}

static ssize_t buf_cnt_store(struct file *file, const char __user *userbuf,
			     size_t count, loff_t *ppos)
{
	int w, h, pflg, mc;
	char buf[80];
	int ret, cnt_flg;

	count = min_t(size_t, count, (sizeof(buf) - 1));
	if (copy_from_user(buf, userbuf, count))
		return -EFAULT;

	buf[count] = 0;

	ret = sscanf(buf, "%i %i %i %i", &w, &h, &pflg, &mc);

	pr_info("%s:\n", __func__);
	cnt_flg = 0;
	switch (ret) {
	case 2:
		cnt_flg = 1;
		pflg = 0;
		mc = 1;
		break;
	case 3:
		cnt_flg = 1;
		mc = 1;
		break;
	case 4:
		cnt_flg = 1;
		break;
	default:
		PR_ERR("please enter: w, h, pflg, mc\n");
		return -EINVAL;
	}

	if (cnt_flg)
		di_cnt_buf(w, h, pflg, mc, 1, 1);
	return count;
}

/**********************/
void dbg_f_post_disable(unsigned int para)
{
	dimh_disable_post_deinterlace_2();
}

void dbg_f_trig_task(unsigned int para)
{
	task_send_ready();
}

const struct di_dbg_func_s di_func_tab[] = {
	{EDI_DBG_F_00, dbg_f_post_disable,
		"dimh_disable_post_deinterlace_2", "no para"},
	{EDI_DBG_F_01, dbg_f_trig_task,
		"trig task", "no para"},
	{EDI_DBG_F_02, dpre_dbg_f_trig,
		"trig pre flow debug", "bit[4]:ddebug on/off;bi[3:0]:cnt"},
	{EDI_DBG_F_03, dpst_dbg_f_trig,
		"trig post flow debug", "bit[4]:ddebug on/off;bi[3:0]:cnt"},
	{EDI_DBG_F_04, hpst_dbg_power_ctr_trig,
		"trig post power", "1: on; 0: off"},

	{EDI_DBG_F_05, hpst_dbg_mem_pd_trig,
		"trig post mem pd", "no para"},
	{EDI_DBG_F_06, hpst_dbg_trig_gate,
		"trig post gate off/on", "no para"},
	{EDI_DBG_F_07, hpst_dbg_trig_mif,
		"trig post mif off/free", "no para"},
};

static ssize_t wfunc_store(struct file *file, const char __user *userbuf,
			   size_t count, loff_t *ppos)
{
	unsigned int findex, para;
	char buf[20];
	int ret;

	int i;

	count = min_t(size_t, count, (sizeof(buf) - 1));
	if (copy_from_user(buf, userbuf, count))
		return -EFAULT;

	buf[count] = 0;

	ret = sscanf(buf, "%i %x", &findex, &para);

	switch (ret) {
	case 2:
		pr_info("func:%d,para=0x%x\n", findex, para);
		for (i = 0; i < ARRAY_SIZE(di_func_tab); i++) {
			if (i == findex && di_func_tab[i].index == findex) {
				if (di_func_tab[i].func)
					di_func_tab[i].func(para);
				pr_info("func:%s finish\n",
					di_func_tab[i].name);
				break;
			}
		}
		break;
	default:
		pr_info("warn: please enter function index and para\n");
		return -EINVAL;
	}

	return count;
}

static int rfunc_show(struct seq_file *seq, void *v)
{
	int i;

	seq_puts(seq, "debug function list:\n");

	for (i = 0; i < ARRAY_SIZE(di_func_tab); i++) {
		if (di_func_tab[i].index != i)
			seq_printf(seq, "warn: index(%d->%d) is not map\n",
				   i, di_func_tab[i].index);
		seq_printf(seq, "index[%d]:%s:%s\n",
			   di_func_tab[i].index,
			   di_func_tab[i].name,
			   di_func_tab[i].info);
	}
	return 0;
}

/**********************/
static ssize_t reg_show(struct file *file, char __user *userbuf,
			size_t count, loff_t *ppos)
{
	char buf[80];

	ssize_t len;
	int *pint;

	pint = (int *)file->private_data;
	pr_info("pint=0x%p,val=%d\n", pint, *pint);

	len = snprintf(buf, sizeof(buf), "%s\n",
		       __func__);

	return simple_read_from_buffer(userbuf, count, ppos, buf, len);
}

static ssize_t reg_store(struct file *file, const char __user *userbuf,
			 size_t count, loff_t *ppos)
{
	unsigned int reg, val;
	char buf[80];
	int ret;
	int *pint;

	count = min_t(size_t, count, (sizeof(buf) - 1));
	if (copy_from_user(buf, userbuf, count))
		return -EFAULT;

	buf[count] = 0;

	ret = sscanf(buf, "%x %i", &reg, &val);

	switch (ret) {
	case 1:
		pr_info("reg:0x%x\n", reg);

		pint = (int *)file->private_data;
		pr_info("pint=0x%p,val=%d\n", pint, *pint);
		break;
	case 2:
		pr_info("reg:0x%x,val=%d\n", reg, val);
		break;
	default:
		return -EINVAL;
	}

	return count;
}

/**********************/
DEFINE_SEQ_SHOW_ONLY(dim_reg_cue_int);
/**********************/
DEFINE_SEQ_SHOW_ONLY(rcfgx);
DEFINE_SEQ_SHOW_ONLY(seq_file_vframe_in);
DEFINE_SEQ_SHOW_ONLY(seq_file_vframe_out);
DEFINE_SEQ_SHOW_ONLY(seq_file_state);
DEFINE_SEQ_SHOW_ONLY(seq_file_mif);
DEFINE_SEQ_SHOW_ONLY(seq_file_sum);

DEFINE_SEQ_SHOW_ONLY(reg_con);
DEFINE_SEQ_SHOW_ONLY(rfunc);
DEFINE_SEQ_SHOW_ONLY(mpxr);
DEFINE_SEQ_SHOW_ONLY(mpr_di);
DEFINE_SEQ_SHOW_ONLY(mpr_nr);
DEFINE_SEQ_SHOW_ONLY(mpr_pd);
DEFINE_SEQ_SHOW_ONLY(mpr_mtn);
DEFINE_SEQ_SHOW_ONLY(cfgt_help);
DEFINE_SEQ_SHOW_ONLY(cfgt_itme_all);
DEFINE_SEQ_SHOW_ONLY(cfgt_val_all);
DEFINE_STORE_ONLY(cfgtw_id);
DEFINE_SEQ_SHOW_STORE(cfgt_one);
DEFINE_SEQ_SHOW_STORE(cfgt_sel);

DEFINE_SEQ_SHOW_ONLY(seq_file_curr_vframe);

DEFINE_STORE_ONLY(wcfgx);
DEFINE_STORE_ONLY(wfunc);
DEFINE_STORE_ONLY(mpxw);
DEFINE_STORE_ONLY(mpw_di);
DEFINE_STORE_ONLY(mpw_nr);
DEFINE_STORE_ONLY(mpw_pd);
DEFINE_STORE_ONLY(mpw_mtn);
DEFINE_STORE_ONLY(buf_cnt);
DEFINE_STORE_ONLY(keep_buf_clear);

DEFINE_SHOW_STORE(reg);

DEFINE_SEQ_SHOW_STORE(seq_file_vtype);
DEFINE_SEQ_SHOW_STORE(ddbg_log_reg);

/**********************/

struct di_dbgfs_files_t {
	const char *name;
	const umode_t mode;
	const struct file_operations *fops;
};

static const struct di_dbgfs_files_t di_debugfs_files_top[] = {
	{"vtype", S_IFREG | 0644, &seq_file_vtype_fops},
	{"reg_log", S_IFREG | 0644, &ddbg_log_reg_fops},
	{"regctr", S_IFREG | 0644, &reg_con_fops},
	{"rfunc", S_IFREG | 0644, &rfunc_fops},
	{"wfunc", S_IFREG | 0644, &wfunc_fops},
	{"reg_cue", S_IFREG | 0644, &dim_reg_cue_int_fops},
	/*module parameter*/
	{"mr_di", S_IFREG | 0644, &mpr_di_fops},
	{"mw_di", S_IFREG | 0644, &mpw_di_fops},
	{"mr_nr", S_IFREG | 0644, &mpr_nr_fops},
	{"mw_nr", S_IFREG | 0644, &mpw_nr_fops},
	{"mr_pd", S_IFREG | 0644, &mpr_pd_fops},
	{"mw_pd", S_IFREG | 0644, &mpw_pd_fops},
	{"mr_mtn", S_IFREG | 0644, &mpr_mtn_fops},
	{"mw_mtn", S_IFREG | 0644, &mpw_mtn_fops},
	{"buf_cnt", S_IFREG | 0644, &buf_cnt_fops},
	{"keep_clear", S_IFREG | 0644, &keep_buf_clear_fops},
	{"cfghelp", S_IFREG | 0644, &cfgt_help_fops},
	{"cfgr_ai", S_IFREG | 0644, &cfgt_itme_all_fops},
	{"cfgr_av", S_IFREG | 0644, &cfgt_val_all_fops},
	{"cfgw_id", S_IFREG | 0644, &cfgtw_id_fops},
	{"cfg_one", S_IFREG | 0644, &cfgt_one_fops},
	{"cfg_sel", S_IFREG | 0644, &cfgt_sel_fops},
};

static const struct di_dbgfs_files_t di_debugfs_files[] = {
	{"rcfgx", S_IFREG | 0644, &rcfgx_fops},
	{"wcfgx", S_IFREG | 0644, &wcfgx_fops},
	{"rvfm_in", S_IFREG | 0644, &seq_file_vframe_in_fops},
	{"rvfm_out", S_IFREG | 0644, &seq_file_vframe_out_fops},
	{"state", S_IFREG | 0644, &seq_file_state_fops},
	{"dumpmif", S_IFREG | 0644, &seq_file_mif_fops},
	{"test_reg", S_IFREG | 0644, &reg_fops},
	{"sum", S_IFREG | 0644, &seq_file_sum_fops},
	{"mpxr", S_IFREG | 0644, &mpxr_fops},
	{"mpxw", S_IFREG | 0644, &mpxw_fops},
	{"vfmc", S_IFREG | 0644, &seq_file_curr_vframe_fops},
};

void didbg_fs_init(void)
{
	int i, j;
	char name[5];
	/*char buf[3];*/

	struct dentry **root_ent;

	struct dentry *ent;
	int *pplane = di_get_plane();

	for (i = 0; i < DI_CHANNEL_NUB; i++) {
#ifdef MARK_HIS
		strcpy(name, "di");
		sprintf(buf, "%01d", i);
		strncat(name, buf, sizeof(buf) - 1);
#endif
		snprintf(name, sizeof(name), "di%01d", i);
		root_ent = dich_get_dbgroot(i);
		if (IS_ERR_OR_NULL(root_ent))
			continue;
		*root_ent = debugfs_create_dir(name, NULL);
		if (IS_ERR_OR_NULL(*root_ent))
			continue;
		*(pplane + i) = i;
		/*printk("plane 0x%p\n", &plane_ch[i]);*/
		for (j = 0; j < ARRAY_SIZE(di_debugfs_files); j++) {
			ent = debugfs_create_file(di_debugfs_files[j].name,
						  di_debugfs_files[j].mode,
						  *root_ent, (pplane + i),
						  di_debugfs_files[j].fops);
			if (!ent)
				PR_ERR("debugfs create failed\n");
		}
	}
	/*top*/
	root_ent = dich_get_dbgroot_top();
	if (IS_ERR_OR_NULL(root_ent))
		return;
	*root_ent = debugfs_create_dir("di_top", NULL);
	if (IS_ERR_OR_NULL(*root_ent))
		return;
	for (i = 0; i < ARRAY_SIZE(di_debugfs_files_top); i++) {
		ent = debugfs_create_file(di_debugfs_files_top[i].name,
					  di_debugfs_files_top[i].mode,
					  *root_ent, NULL,
					  di_debugfs_files_top[i].fops);
		if (!ent)
			PR_ERR("debugfs top [%d]create failed\n", i);
	}
}

void didbg_fs_exit(void)
{
	struct dentry **root_ent;
	int i;

	for (i = 0; i < DI_CHANNEL_NUB; i++) {
		root_ent = dich_get_dbgroot(i);
		debugfs_remove_recursive(*root_ent);
	}

	/*top*/
	root_ent = dich_get_dbgroot_top();
	debugfs_remove_recursive(*root_ent);

	pr_info("%s:finish\n", __func__);
}

/*-----------------------*/

