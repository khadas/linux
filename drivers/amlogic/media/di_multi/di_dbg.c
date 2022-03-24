// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * drivers/amlogic/media/di_multi/di_dbg.c
 *
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
 */

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/seq_file.h>
#include <linux/debugfs.h>
#include <linux/sched/clock.h>
#include "di_data.h"

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
#include "di_dbg.h"
#include "di_sys.h"

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

#ifdef MARK_HIS
static struct vframe_s **di_get_dbg_vframe_out(unsigned int ch)
{
	return &get_datal()->ch_data[ch].dbg_data.pfm_out;
}
#endif

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

static void trace_irq_aisr(unsigned int index)
{
	u64 ustime;

	ustime = cur_to_usecs();
	trace_dim_irq_aisr("PRE-AISR-1", index, ustime);
}

static void trace_irq_dct(unsigned int index)
{
	u64 ustime;

	ustime = cur_to_usecs();
	trace_dim_irq_dct("PRE-DIRQ-1", index, ustime);
}

static void trace_dct_set(unsigned int index)
{
	u64 ustime;

	ustime = cur_to_usecs();
	trace_dim_dct_set("PRE-DCTS-1", index, ustime);
}

#define DI_TRACE_LIMIT		50
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

static void trace_slef_trig(unsigned int index)
{
	u64 ustime;

	if (di_get_disp_cnt() > DI_TRACE_LIMIT)
		return;

	ustime = cur_to_usecs();
	trace_dim_self_trig("P-TRIG", index, ustime);
}

static void trace_msct(unsigned int index, u64 timer_begin)
{
	u64 ustime;

	ustime = cur_to_usecs();
	trace_dim_sct_alloc("SCT-ALLOC", index, ustime - timer_begin);
}

static void trace_msct_tail(unsigned int index, unsigned int used_cnt)
{
	trace_dim_sct_tail("SCT-TAILX", index, (u64)used_cnt);
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
	.sct_alloc = trace_msct,
	.sct_tail  = trace_msct_tail,
	.self_trig = trace_slef_trig,
	.irq_aisr = trace_irq_aisr,
	.irq_dct = trace_irq_dct,
	.dct_set = trace_dct_set,
};

/*keep same order as enum EDBG_TIMER*/
static const char * const dbg_timer_name[] = {
	"reg_b",
	"reg_e",
	"unreg_b",
	"unreg_e",
	"1_peek",
	"dct_b",
	"dct_e",
	"1_get",
	"2_get",
	"3_get",
	"alloc",
	"mem1",
	"mem2",
	"mem3",
	"mem4",
	"mem5",
	"ready",
	"1_pre_cfg",
	"1_pre_ready",
	"2_pre_cfg",
	"2_pre_ready",
	"3_pre_cfg",
	"3_pre_ready",
	"1_pst",
	"2_pst",
	"1_pst_get",
	"2_pst_get",
	"3_pst_get",
	"a_sec_pre_b",
	"a_sec_pre_e",
	"a_sec_pst_b",
	"a_sec_pst_e",
	"pre_bypass_0",
	"pre_bypass_1",
	"pre_bypass_2",
	"dctp_0",
	"dctp_1",
	"dctp_2",
};

static const unsigned int crc_value[CRC_COUNT_NUB][CRC_NUB] = {
	{0xfa34ba3b, 0x6ee997fc, 0x9b9b9757},
	{0x73cf853a, 0x15c2a743, 0x5277700a},
	{0x5fb1aa69, 0xd32974f0, 0xd4eba4b3},
	{0x56d314a8, 0x65141931, 0x7555f3f7},
	{0x8ae376a3, 0x460fefde, 0xaae9e2a8},
	{0x8f597055, 0xf5ab056a, 0xbb6ad7f},
	{0xdb6aa249, 0xe253adda, 0x29f8a81f},
	{0x7d97b111, 0x51a47d, 0x9713ee6b},
	{0x749bed58, 0x5de61417, 0x2332c62},
	{0xc00cb131, 0xd443d780, 0x1847c8ed},
	{0x88b65af8, 0xb656b462, 0xe7892e9},
	{0xb3309546, 0xf612f6e7, 0x7bbc9e8d},
	{0x73f3e269, 0x2ed08da6, 0xe924af1c},
	{0xbe0eb0a1, 0x75c9d03f, 0xf7691baa},
	{0xe05b0258, 0xc5079743, 0x140d4899},
	{0xd692e594, 0xb920b1aa, 0xe910b32e},
	{0x538943f0, 0xe9d99783, 0xad886e46},
	{0xbfb0c4d7, 0x146ae882, 0xf3c49a9},
	{0x2458a298, 0x3a673707, 0x422545ba},
	{0x65860c9, 0x42e49489, 0xe133a509},

};

void dbg_slt_crc_count(struct di_ch_s *pch, unsigned int postcrc,
		       unsigned int nrcrc, unsigned int mtncrc)
{
	if (pch->crc_cnt >= CRC_COUNT_NUB)
		return;

	if (crc_value[pch->crc_cnt][0] != postcrc)
		dbg_post_ref("err count=%x, postcrc=0x%x, demo=0x%x",
		pch->crc_cnt, postcrc, crc_value[pch->crc_cnt][0]);
	if (crc_value[pch->crc_cnt][1] != nrcrc)
		dbg_post_ref("err count=%x, nrcrc=0x%x, demo=0x%x",
		pch->crc_cnt, nrcrc, crc_value[pch->crc_cnt][1]);
	if (crc_value[pch->crc_cnt][2] != mtncrc)
		dbg_post_ref("err count=%x, mtncrc=0x%x, demo=0x%x",
		pch->crc_cnt, mtncrc, crc_value[pch->crc_cnt][2]);
	pch->crc_cnt++;
	dbg_post_ref("count test=%d", pch->crc_cnt);
}

void dbg_slt_crc(struct di_buf_s *di_buf)
{
	bool flg = 0;
	unsigned int *p;
	int i;
	unsigned int crc = 0;
	unsigned long crc_tmp = 0;
	unsigned int sh, sv;
	//unsigned int size_y, size_uv;
	unsigned long nr_size = 0, dump_adr = 0;

	sh = di_buf->vframe->canvas0_config[0].width;
	sv = di_buf->vframe->canvas0_config[0].height;
	//size_y = sh * sv;
	//size_uv = size_y / 2;
	//nr_size = size_y + size_uv;
	nr_size = sh * (sv - 8) / 4;//0x4f1a00/4

	dump_adr = di_buf->vframe->canvas0_config[0].phy_addr;//di_buf->nr_adr;

	p = (unsigned int *)dim_vmap(dump_adr, nr_size, &flg);
	if (!p) {
		pr_error("%s:vmap:0x%lx\n", __func__, dump_adr);
		return;
	}

	for (i = 0; i < nr_size; i++)
		crc_tmp += *(p + i);

	crc = (unsigned int)crc_tmp;

	dbg_post_ref("%s:crc_tmp:0x%x,nr_size=0x%lx,dump_adr=0x%lx\n", __func__,
		crc, nr_size, dump_adr);

	if (flg)
		dim_unmap_phyaddr((u8 *)p);
}

void dbg_timer(unsigned int ch, enum EDBG_TIMER item)
{
	u64 ustime, udiff;
	struct di_ch_s *pch;
	unsigned int tmp;

	pch = get_chdata(ch);
	//ustime = cur_to_usecs();
	ustime	= cur_to_msecs();

	if (item >= EDBG_TIMER_NUB)
		return;
	if (item == EDBG_TIMER_MEM_1) {
		if (pch->dbg_data.timer_mem_alloc_cnt < 5) {
			tmp = EDBG_TIMER_MEM_1 + pch->dbg_data.timer_mem_alloc_cnt;
			pch->dbg_data.ms_dbg[tmp] = ustime;
			pch->dbg_data.timer_mem_alloc_cnt++;
			//PR_INF("%s:%s:%lu\n", __func__,
			//	 dbg_timer_name[item],
			//	 (unsigned long)ustime);
		}
		return;
	}
	//PR_INF("%s:%s:%lu\n", __func__, dbg_timer_name[item], (unsigned long)ustime);
	pch->dbg_data.ms_dbg[item] = ustime;
	//PR_INF("%s:%d:ms[%llu]\n", __func__, item, ustime);
	switch (item) {
	case EDBG_TIMER_REG_E:
		udiff = pch->dbg_data.ms_dbg[EDBG_TIMER_REG_E] -
			pch->dbg_data.ms_dbg[EDBG_TIMER_REG_B];
		dbg_ev("reg:use[%llu]ms\n", udiff);
		break;
	case EDBG_TIMER_UNREG_E:

		udiff = pch->dbg_data.ms_dbg[EDBG_TIMER_UNREG_E] -
			pch->dbg_data.ms_dbg[EDBG_TIMER_UNREG_B];
		dbg_ev("unreg:use[%llu]ms\n", udiff);
		//dbg_ev("\tb[%llu]:e[%llu]\n",
		 //      pch->dbg_data.ms_dbg[EDBG_TIMER_UNREG_B],
		 //      pch->dbg_data.ms_dbg[EDBG_TIMER_UNREG_E]);
		break;
	case EDBG_TIMER_SEC_PRE_E:
		udiff = pch->dbg_data.ms_dbg[EDBG_TIMER_SEC_PRE_E] -
			pch->dbg_data.ms_dbg[EDBG_TIMER_SEC_PRE_B];
		dbg_ev("pre_sec:use[%llu]ms\n", udiff);
		break;
	case EDBG_TIMER_SEC_PST_E:
		udiff = pch->dbg_data.ms_dbg[EDBG_TIMER_SEC_PST_E] -
			pch->dbg_data.ms_dbg[EDBG_TIMER_SEC_PST_B];
		dbg_ev("pst_sec:use[%llu]ms\n", udiff);
		break;
	default:
		break;
	}
}

void dbg_timer_clear(unsigned int ch)
{
	int i;
	struct di_ch_s *pch;

	pch = get_chdata(ch);

	for (i = EDBG_TIMER_FIRST_PEEK; i < EDBG_TIMER_NUB; i++)
		pch->dbg_data.ms_dbg[i] = 0;

	pch->dbg_data.timer_mem_alloc_cnt = 0;
}
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

static int seq_hf_info(struct seq_file *seq, void *v, struct hf_info_t *hf)
{
	if (!hf) {
		//seq_printf(seq, "no hf\n");
		return 0;
	}
	seq_printf(seq, "hf_info:0x%px, 0x%x\n", hf, hf->index);
	seq_printf(seq, "\t<%d,%d>\n", hf->width, hf->height);
	seq_printf(seq, "\t<%d,%d>\n", hf->buffer_w, hf->buffer_h);
	seq_printf(seq, "\taddr:0x%lx\n", hf->phy_addr);
	return 0;
}

static int seq_file_vframe(struct seq_file *seq, void *v, struct vframe_s *pvfm)
{
	int i;
	struct canvas_config_s *pcvs;
	struct dim_rpt_s *rpt;

	if (!pvfm) {
		seq_puts(seq, "war: dump vframe NULL\n");
		return 0;
	}
	seq_printf(seq, "%-15s:0x%px\n", "addr", pvfm);
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
	seq_printf(seq, "%-15s:0x%x\n", "flag", pvfm->flag);
	seq_printf(seq, "\t%-15s:%d\n", "flag:VFRAME_FLAG_DOUBLE_FRAM",
		   pvfm->flag & VFRAME_FLAG_DOUBLE_FRAM);
	seq_printf(seq, "\t%-15s:%d\n", "flag:VFRAME_FLAG_DI_P_ONLY",
		   pvfm->flag & VFRAME_FLAG_DI_P_ONLY);
	seq_printf(seq, "\t%-15s:%d\n", "flag:VFRAME_FLAG_DI_PW_VFM",
		   pvfm->flag & VFRAME_FLAG_DI_PW_VFM);
	seq_printf(seq, "\t%-15s:%d\n", "flag:VFRAME_FLAG_DI_PW_N_LOCAL",
		   pvfm->flag & VFRAME_FLAG_DI_PW_N_LOCAL);
	seq_printf(seq, "\t%-15s:%d\n", "flag:_DI_PW_N_EXT",
		   pvfm->flag & VFRAME_FLAG_DI_PW_N_EXT);
	seq_printf(seq, "\t%-15s:%d\n", "flag:_HF",
		   pvfm->flag & VFRAME_FLAG_HF);
	seq_printf(seq, "\t%-15s:%d\n", "flag:_DI_DW",
		   pvfm->flag & VFRAME_FLAG_DI_DW);
	seq_printf(seq, "%-15s:0x%x\n", "canvas0Addr", pvfm->canvas0Addr);
	seq_printf(seq, "%-15s:0x%x\n", "canvas1Addr", pvfm->canvas1Addr);
	seq_printf(seq, "%-15s:0x%lx\n", "compHeadAddr", pvfm->compHeadAddr);
	seq_printf(seq, "%-15s:0x%lx\n", "compBodyAddr", pvfm->compBodyAddr);
	seq_printf(seq, "%-15s:%d\n", "plane_num", pvfm->plane_num);

	seq_printf(seq, "%-15s:%d\n", "bufWidth", pvfm->bufWidth);
	seq_printf(seq, "%-15s:%d\n", "width", pvfm->width);
	seq_printf(seq, "%-15s:%d\n", "height", pvfm->height);
	seq_printf(seq, "%-15s:%d\n", "compWidth", pvfm->compWidth);
	seq_printf(seq, "%-15s:%d\n", "compHeight", pvfm->compHeight);
	seq_printf(seq, "%-15s:%d\n", "ratio_control", pvfm->ratio_control);
	seq_printf(seq, "%-15s:0x%x\n", "bitdepth", pvfm->bitdepth);
	seq_printf(seq, "%-15s:0x%x\n", "signal_type", pvfm->signal_type);

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
		   pvfm->process_fun);
	seq_printf(seq, "%-15s:0x%p\n", "private_data",
		   pvfm->private_data);

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
	for (i = 0; i < pvfm->plane_num; i++) {
		pcvs = &pvfm->canvas0_config[i];
		seq_printf(seq, "%-15s:%d\n", "canvas0_cfg", i);
	#ifdef CVS_UINT
		seq_printf(seq, "\t%-15s:0x%x\n", "phy_addr",
			   pcvs->phy_addr);
	#else
		seq_printf(seq, "\t%-15s:0x%lx\n", "phy_addr",
			   pcvs->phy_addr);
	#endif
		seq_printf(seq, "\t%-15s:%d\n", "width",
			   pcvs->width);
		seq_printf(seq, "\t%-15s:%d\n", "height",
			   pcvs->height);
		seq_printf(seq, "\t%-15s:%d\n", "block_mode",
			   pcvs->block_mode);
		seq_printf(seq, "\t%-15s:0x%x\n", "endian",
			   pcvs->endian);
	}

	seq_hf_info(seq, v, pvfm->hf_info);

	if (pvfm->vf_ext)
		seq_printf(seq, "%-15s\n", "vf_ext");
	else
		seq_printf(seq, "%-15s\n", "vf_ext:none");
	rpt = dim_api_getrpt(pvfm);
	if (rpt) {
		if (rpt->spt_bits) {
			seq_printf(seq, "bits[0x%x], map0[0x%x], map1[0x%x],map2[0x%x],map3[0x%x],map15[0x%x], bld2[0x%x]\n",
				   rpt->spt_bits,
				   rpt->dct_map_0,
				   rpt->dct_map_1,
				   rpt->dct_map_2,
				   rpt->dct_map_3,
				   rpt->dct_map_15,
				   rpt->dct_bld_2);
		}
	}

	return 0;
}

//static
int seq_file_dvfm(struct seq_file *seq, void *v, struct dvfm_s *pvfm)
{
	int i;
	struct canvas_config_s *pcvs;

	if (!pvfm) {
		seq_puts(seq, "war: dump dvfm NULL\n");
		return 0;
	}
	seq_printf(seq, "%-15s:0x%p\n", "addr", pvfm);
	seq_printf(seq, "%-15s:0x%x\n", "type", pvfm->type);
	seq_printf(seq, "%-15s:0x%x\n", "canvas0Addr", pvfm->canvas0Addr);
	seq_printf(seq, "%-15s:0x%x\n", "canvas1Addr", pvfm->canvas1Addr);
	seq_printf(seq, "%-15s:0x%x\n", "compHeadAddr", pvfm->compHeadAddr);
	seq_printf(seq, "%-15s:0x%x\n", "compBodyAddr", pvfm->compBodyAddr);
	seq_printf(seq, "%-15s:%d\n", "plane_num", pvfm->plane_num);
	seq_printf(seq, "%-15s:%d\n", "width", pvfm->width);
	seq_printf(seq, "%-15s:%d\n", "height", pvfm->height);
	seq_printf(seq, "%-15s:%d\n", "bitdepth", pvfm->bitdepth);
	for (i = 0; i < pvfm->plane_num; i++) {
		pcvs = &pvfm->canvas0_config[i];
		seq_printf(seq, "%-15s:%d\n", "canvas0_cfg", i);
	#ifdef CVS_UINT
		seq_printf(seq, "\t%-15s:0x%x\n", "phy_addr",
			   pcvs->phy_addr);
	#else
		seq_printf(seq, "\t%-15s:0x%lx\n", "phy_addr",
			   pcvs->phy_addr);
	#endif
		seq_printf(seq, "\t%-15s:%d\n", "width",
			   pcvs->width);
		seq_printf(seq, "\t%-15s:%d\n", "height",
			   pcvs->height);
		seq_printf(seq, "\t%-15s:%d\n", "block_mode",
			   pcvs->block_mode);
		seq_printf(seq, "\t%-15s:0x%x\n", "endian",
			   pcvs->endian);
	}
	seq_printf(seq, "%-15s:%d\n", "nub_in_frm", pvfm->nub_in_frm);
	seq_printf(seq, "%-15s:0x%x\n", "vfm_type_in", pvfm->vfm_type_in);
	seq_printf(seq, "%-15s:0x%x\n", "sts_di", pvfm->sts_di);
	PR_INF("\t%-15s\n", "cvs");
	for (i = 0; i < 2; i++)
		seq_printf(seq, "\t%-15s:%d\n", "cvs id", pvfm->cvs_nu[i]);

	seq_printf(seq, "\t%-15s:%d\n", "buf_hsize", pvfm->buf_hsize);
	seq_printf(seq, "\t%-15s:%d\n", "is_dw", pvfm->is_dw);
	return 0;
}

void print_dvfm(struct dvfm_s *pvfm, char *name)
{
	int i;
	struct canvas_config_s *pcvs;

	if (!pvfm) {
		PR_INF("war: dump dvfm NULL\n");
		return;
	}
	PR_INF("%s:%-15s:0x%p-------------\n", name, "addr", pvfm);
	PR_INF("\t%-15s:0x%x\n", "type", pvfm->type);
	PR_INF("\t%-15s:0x%x\n", "canvas0Addr", pvfm->canvas0Addr);
	PR_INF("\t%-15s:0x%x\n", "canvas1Addr", pvfm->canvas1Addr);
	PR_INF("\t%-15s:0x%x\n", "compHeadAddr", pvfm->compHeadAddr);
	PR_INF("\t%-15s:0x%x\n", "compBodyAddr", pvfm->compBodyAddr);
	PR_INF("\t%-15s:%d\n", "plane_num", pvfm->plane_num);
	PR_INF("\t%-15s:%d\n", "width", pvfm->width);
	PR_INF("\t%-15s:%d\n", "height", pvfm->height);
	PR_INF("\t%-15s:0x%x\n", "bitdepth", pvfm->bitdepth);
	for (i = 0; i < pvfm->plane_num; i++) {
		pcvs = &pvfm->canvas0_config[i];
		PR_INF("\t%-15s:%d\n", "canvas0_cfg", i);
	#ifdef CVS_UINT
		PR_INF("\t\t%-15s:0x%x\n", "phy_addr",
			   pcvs->phy_addr);
	#else
		PR_INF("\t\t%-15s:0x%lx\n", "phy_addr",
			   pcvs->phy_addr);
	#endif
		PR_INF("\t\t%-15s:%d\n", "width",
			   pcvs->width);
		PR_INF("\t\t%-15s:%d\n", "height",
			   pcvs->height);
		PR_INF("\t\t%-15s:%d\n", "block_mode",
			   pcvs->block_mode);
		PR_INF("\t\t%-15s:0x%x\n", "endian",
			   pcvs->endian);
	}
	PR_INF("\t%-15s:%d\n", "nub_in_frm", pvfm->nub_in_frm);
	PR_INF("\t%-15s:0x%x\n", "vfm_type_in", pvfm->vfm_type_in);
	PR_INF("\t%-15s:0x%x\n", "sts_di", pvfm->sts_di);
	PR_INF("\t%-15s\n", "cvs");
	for (i = 0; i < 2; i++)
		PR_INF("\t\t%-15s:%d\n", "cvs id", pvfm->cvs_nu[i]);

	PR_INF("\t%-15s:%d\n", "buf_hsize", pvfm->buf_hsize);
	PR_INF("\t%-15s:%d\n", "is_dw", pvfm->is_dw);
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
void didbg_vframe_out_save(unsigned int ch, struct vframe_s *pvfm, unsigned int id)
{
	//unsigned int ch;
	//struct vframe_s **pvfm_t;

	//ch = DI_SUB_ID_S0;
	if (!di_cfgx_get(ch, EDI_DBG_CFGX_IDX_VFM_OT))
		return;

	//pvfm_t = di_get_dbg_vframe_out(ch);
	/* *pvfm_t = pvfm; */
	get_datal()->ch_data[ch].dbg_data.pfm_out = pvfm;
	if (dimp_get(edi_mp_bypass_post_state)) {
		PR_INF("%s:pvfm:0x%px\n", __func__,
			get_datal()->ch_data[ch].dbg_data.pfm_out);
	}
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

	//seq_file_vframe(seq, v, *di_get_dbg_vframe_out(ch));
	seq_file_vframe(seq,
			v,
			get_datal()->ch_data[ch].dbg_data.pfm_out);

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

static int dbg_q_sct_show(struct seq_file *s, void *what)
{
	int *chp;
	struct di_ch_s *pch;

	chp = (int *)s->private;

	seq_printf(s, "%s:ch[%d]\n", __func__, *chp);
	pch = get_chdata(*chp);

	dbg_q_listid(s, &pch->sct_qb);

	return 0;
}

static int dbg_sct_peek_show(struct seq_file *s, void *what)
{
	int *chp;
	struct di_ch_s *pch;
	struct dim_sct_s *qsct;

	chp = (int *)s->private;

	seq_printf(s, "%s:ch[%d]\n", __func__, *chp);
	pch = get_chdata(*chp);

	qsct = qsct_peek(pch, QBF_SCT_Q_IDLE);
	if (!qsct) {
		seq_printf(s, "%s:\n", "null");
		return 0;
	}

	seq_printf(s, "\t%d:\n", qsct->header.index);

	return 0;
}

static int dbg_sct_used_pat_show(struct seq_file *s, void *what)
{
	int *chp;
	struct di_ch_s *pch;
//	struct dim_sct_s *qsct;

	chp = (int *)s->private;

	seq_printf(s, "%s:ch[%d]\n", __func__, *chp);
	pch = get_chdata(*chp);

	dbg_sct_used(s, pch);
	return 0;
}

static int dbg_q_nins_show(struct seq_file *s, void *what)
{
	int *chp;
	struct di_ch_s *pch;

	chp = (int *)s->private;

	seq_printf(s, "%s:ch[%d]\n", __func__, *chp);
	pch = get_chdata(*chp);

	dbg_q_listid(s, &pch->nin_qb);

	dbg_q_list_qbuf(s, &pch->nin_qb);

	//qbuf_dbg_checkid(&pch->nin_qb, 10);
	return 0;
}

static int dbg_nins_peek_show(struct seq_file *s, void *what)
{
	int *chp;
	struct di_ch_s *pch;
	struct dim_nins_s *nin;

	chp = (int *)s->private;

	seq_printf(s, "%s:ch[%d]\n", __func__, *chp);
	pch = get_chdata(*chp);

	nin = nins_peek(pch);
	if (!nin) {
		seq_printf(s, "%s:\n", "null");
		return 0;
	}

	seq_printf(s, "\t%d:\n", nin->header.index);

	return 0;
}

static int dbg_q_ndis_show(struct seq_file *s, void *what)
{
	int *chp;
	struct di_ch_s *pch;

	chp = (int *)s->private;

	seq_printf(s, "%s:ch[%d]\n", __func__, *chp);
	pch = get_chdata(*chp);

	dbg_q_listid(s, &pch->ndis_qb);

	//dbg_q_list_qbuf(s, &pch->ndis_qb);
	ndis_dbg_qbuf_detail(s, pch);

	//qbuf_dbg_checkid(&pch->nin_qb, 10);
	return 0;
}

static int dbg_q_ndkb_show(struct seq_file *s, void *what)
{
	int *chp;
	struct di_ch_s *pch;

	chp = (int *)s->private;

	seq_printf(s, "%s:ch[%d]\n", __func__, *chp);
	pch = get_chdata(*chp);

	ndkb_dbg_list(s, pch);
	return 0;
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
	#ifdef MARK_HIS	//@ary_note: todo
	di_que_list(ch, QUE_POST_READY, &tmpa[0], &psize);
	seq_printf(seq, "post_ready_list: curr(%d)\n", psize);

	for (itmp = 0; itmp < psize; itmp++) {
		p = pw_qindex_2_buf(ch, tmpa[itmp]);
		pvfm = p->vframe;
		seq_file_vframe(seq, v, pvfm);
		seq_printf(seq, "%s\n", splt2);
	}
	seq_printf(seq, "%s\n", splt);
	#endif
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

static int seq_file_stateb_show(struct seq_file *s, void *v)
{
	unsigned int ch;
	struct di_ch_s *pch;
	//struct di_pre_stru_s *ppre;
	//struct di_post_stru_s *ppst;
	int i;
	u64 diff1, diff2, treg, tlast, tcrr, tcrr_old;
	//struct di_dat_s *pdat;
	//struct di_mm_s *mm;

	ch = seq_get_channel(s);
	seq_printf(s, "%s:ch[%d]\n", __func__, ch);

	//mm = dim_mm_get(ch);

	pch = get_chdata(ch);
	if (pch) {
		treg = pch->dbg_data.ms_dbg[EDBG_TIMER_REG_B];
		tlast = treg;
		seq_printf(s, "%s:%llu\n", "timer", treg);
		for (i = EDBG_TIMER_FIRST_PEEK; i < EDBG_TIMER_NUB; i++) {
			tcrr = pch->dbg_data.ms_dbg[i];
			tcrr_old = tcrr;
			if (!tcrr)
				tcrr = tlast;
			diff1 = tcrr - treg;
			diff2 = tcrr - tlast;
			seq_printf(s, "\t%-20s %-10llu %-10llu %-10llu\n",
				   dbg_timer_name[i],
				   tcrr_old,
				   diff1, diff2);
			tlast = tcrr;
		}
	}
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

static int hw_info_show(struct seq_file *s, void *v)
{
	seq_printf(s, "hw_infor:%s:\n", get_datal()->hop_l1->info.name);
	seq_printf(s, "\tupdate:%s:\n", get_datal()->hop_l1->info.update);
	return 0;
}

static ssize_t dbg_crc_store(struct file *file, const char __user *userbuf,
			     size_t count, loff_t *ppos)
{
	unsigned int val;
	char buf[80];
	int ret;
	int *chp;
	unsigned int ch;
//	struct dim_dbg_cvs_s *dbg;
	struct di_ch_s *pch;
	unsigned int mode;
	int i;

	count = min_t(size_t, count, (sizeof(buf) - 1));
	if (copy_from_user(buf, userbuf, count))
		return -EFAULT;

	buf[count] = 0;
	chp = (int *)file->private_data;
	//pr_info("%s:ch[%d]\n", __func__, *chp);
	ch = *chp;

	ret = kstrtouint(buf, 0, &val);
	if (ret) {
		//pr_info("war:please enter val\n");
		return 0;
	}
	pch = get_chdata(ch);
	if (val < 0xf)
		return count;

	mode = ((val & 0xfff) >> 4);
	pr_info("4k test:rotation:0x%x, 0x%x\n", mode, val);
	if (val & DI_BIT16) {
		//pr_info("5000\n");
		for (i = 0; i < 5000; i++) {
			dbg_cp_4k(pch, mode);
			usleep_range(5000, 5001);
		}
	} else {
		dbg_cp_4k(pch, mode);
	}
	return count;
}

static ssize_t dbg_pip_store(struct file *file, const char __user *userbuf,
			     size_t count, loff_t *ppos)
{
	unsigned int val;
	char buf[80];
	int ret;
	int *chp;
	unsigned int ch;
//	struct dim_dbg_cvs_s *dbg;
	struct di_ch_s *pch;
	unsigned int mode;
	unsigned int i;

	count = min_t(size_t, count, (sizeof(buf) - 1));
	if (copy_from_user(buf, userbuf, count))
		return -EFAULT;

	buf[count] = 0;
	chp = (int *)file->private_data;
	//pr_info("%s:ch[%d]\n", __func__, *chp);
	ch = *chp;

	ret = kstrtouint(buf, 0, &val);
	if (ret) {
		//pr_info("war:please enter val\n");
		return 0;
	}
	pch = get_chdata(ch);
	//dbg = &pch->dbg_data.dbg_cvs;

	mode = val & 0xffff;
	if (val & DI_BIT16) {
		for (i = 0; i < 200; i++) {
			dbg_pip_func(pch, mode);
			usleep_range(10000, 10001);
		}
	} else {
		dbg_pip_func(pch, mode);
	}
	return count;
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

	//dim_dbg_release_keep_all(ch);

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
	task_send_ready(10);
}

void dbg_f_pq_sel(unsigned int para)
{
	unsigned int data[2];
	static unsigned int cnt;

	if (para == 0) {
		dim_pq_db_sel(DIM_DB_SV_DCT_BL2, 0, NULL);
	} else if (para == 1) {
		data[0] = cnt << 16;
		data[1] = 0x1ff0000;
		dim_pq_db_sel(DIM_DB_SV_DCT_BL2, 1, &data[0]);
		PR_INF("%s:data[0x%x],data[0x%x],cnt[0x%x]\n",
			__func__, data[0], data[1], cnt);
		cnt++;
	}
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
	{EDI_DBG_F_08, dbg_f_pq_sel,
		"trig pq set", "no para"},
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

static int policy_show(struct seq_file *s, void *what)
{
	int ch;
	struct dim_policy_s *pp = get_dpolicy();
	struct di_ch_s *pch;
	unsigned int ptt;

	/*bypass*/
	for (ch = 0; ch < DI_CHANNEL_NUB; ch++) {
		pch = get_chdata(ch);
		seq_printf(s, "ch[%d]:\n", ch);
		seq_printf(s, "\tneed:%d,0x%x\n",
			   pch->bypass.b.need_bypass,
			   pch->bypass.b.reason_n);
		seq_printf(s, "\tis:%d,0x%x\n",
			   pch->bypass.b.is_bypass,
			   pch->bypass.b.reason_i);
	}
	/*config*/
	seq_puts(s, "cfg:\n");
	seq_printf(s, "\tbypass_all_p:\t%d\n",
		   pp->cfg_b.bypass_all_p);
	seq_printf(s, "\ti_first:\t%d\n",
		   pp->cfg_b.i_first);
	/*policy*/
	seq_puts(s, "policy:\n");
	ptt = 0;
	for (ch = 0; ch < DI_CHANNEL_NUB; ch++) {
		seq_printf(s, "\tch[%d]\t:%d\n", ch, pp->ch[ch]);
		ptt += pp->ch[ch];
	}
	seq_printf(s, "order=0x%x\n", pp->order_i);
	seq_printf(s, "std=%d, total:%d\n", pp->std, ptt);

	return 0;
}

static int hf_en_show(struct seq_file *s, void *what)
{
	int ch;

	struct di_ch_s *pch;

	/*ch*/
	seq_puts(s, "ch:\n");
	for (ch = 0; ch < DI_CHANNEL_NUB; ch++) {
		pch = get_chdata(ch);
		seq_printf(s, "ch[%d];ext:%d\n",
			   ch, dip_itf_is_ins_exbuf(pch));
		seq_printf(s, "\ten_hf_buf:%d,en_hf:%d\n",
			   pch->en_hf_buf,
			   pch->en_hf);
	}
	/*top*/
	if (get_datal() && get_datal()->mdata)
		seq_printf(s, "ic:%s;cfg:\n",
		get_datal()->mdata->name);
	seq_printf(s, "\tcfg:\t%d\n",
		   cfgg(HF));
	seq_printf(s, "\tbusy:\t%d\n",
		   get_datal()->hf_busy);
	seq_printf(s, "\tcnt:\t%d\n",
		   get_datal()->hf_src_cnt);
	seq_printf(s, "\thf_owner:\t%d\n",
		   get_datal()->hf_owner);

	return 0;
}

/**********************/
static int dbg_afd0_reg_show(struct seq_file *s, void *v)
{
	dbg_afd_reg_v3(s, EAFBC_DEC0);
	return 0;
}

static int dbg_afd1_reg_show(struct seq_file *s, void *v)
{
	dbg_afd_reg_v3(s, EAFBC_DEC1);
	return 0;
}

static int dbg_afd2_reg_show(struct seq_file *s, void *v)
{
	dbg_afd_reg_v3(s, EAFBC_DEC2_DI);
	return 0;
}

static int dbg_afd3_reg_show(struct seq_file *s, void *v)
{
	dbg_afd_reg_v3(s, EAFBC_DEC3_MEM);
	return 0;
}

static int dbg_afd4_reg_show(struct seq_file *s, void *v)
{
	dbg_afd_reg_v3(s, EAFBC_DEC_CHAN2);
	return 0;
}

static int dbg_afd5_reg_show(struct seq_file *s, void *v)
{
	dbg_afd_reg_v3(s, EAFBC_DEC_IF0);
	return 0;
}

static int dbg_afd6_reg_show(struct seq_file *s, void *v)
{
	dbg_afd_reg_v3(s, EAFBC_DEC_IF1);
	return 0;
}

static int dbg_afd7_reg_show(struct seq_file *s, void *v)
{
	dbg_afd_reg_v3(s, EAFBC_DEC_IF2);
	return 0;
}

static int dbg_afde0_reg_show(struct seq_file *s, void *v)
{
	dbg_afe_reg_v3(s, EAFBC_ENC0);
	return 0;
}

static int dbg_afde1_reg_show(struct seq_file *s, void *v)
{
	dbg_afe_reg_v3(s, EAFBC_ENC1);
	return 0;
}

static int dbg_afd_bits_afbc0_show(struct seq_file *s, void *v)
{
	//struct reg_afbs_s *pafbc = get_afbc(0);
	dbg_afbcd_bits_show(s, EAFBC_DEC0);
	return 0;
}

static int dbg_afd_bits_afbc1_show(struct seq_file *s, void *v)
{
	dbg_afbcd_bits_show(s, EAFBC_DEC1);

	return 0;
}

static int dbg_afd_bits_afbc2_show(struct seq_file *s, void *v)
{
	dbg_afbcd_bits_show(s, EAFBC_DEC2_DI);

	return 0;
}

static int dbg_afd_bits_afbc3_show(struct seq_file *s, void *v)
{
	dbg_afbcd_bits_show(s, EAFBC_DEC3_MEM);
	return 0;
}

static int dbg_afd_bits_afbc4_show(struct seq_file *s, void *v)
{
	//struct reg_afbs_s *pafbc = get_afbc(0);
	dbg_afbcd_bits_show(s, EAFBC_DEC_CHAN2);
	return 0;
}

static int dbg_afd_bits_afbc5_show(struct seq_file *s, void *v)
{
	dbg_afbcd_bits_show(s, EAFBC_DEC_IF0);

	return 0;
}

static int dbg_afd_bits_afbc6_show(struct seq_file *s, void *v)
{
	dbg_afbcd_bits_show(s, EAFBC_DEC_IF1);

	return 0;
}

static int dbg_afd_bits_afbc7_show(struct seq_file *s, void *v)
{
	dbg_afbcd_bits_show(s, EAFBC_DEC_IF2);
	return 0;
}

static int dbg_afd_bits_afbce0_show(struct seq_file *s, void *v)
{
	dbg_afbce_bits_show(s, EAFBC_ENC0);

	return 0;
}

static int dbg_afd_bits_afbce1_show(struct seq_file *s, void *v)
{
	dbg_afbce_bits_show(s, EAFBC_ENC1);
	return 0;
}

/* mif */
static int mif_inp_reg_show(struct seq_file *s, void *v)
{
	//opl1()->dbg_mif_reg(s, DI_MIF0_ID_INP);
	dbg_mif_reg(s, DI_MIF0_ID_INP);
	return 0;
}

static int mif_mem_reg_show(struct seq_file *s, void *v)
{
	//opl1()->dbg_mif_reg(s, DI_MIF0_ID_MEM);
	dbg_mif_reg(s, DI_MIF0_ID_MEM);
	return 0;
}

static int mif_chan2_reg_show(struct seq_file *s, void *v)
{
	//opl1()->dbg_mif_reg(s, DI_MIF0_ID_CHAN2);
	dbg_mif_reg(s, DI_MIF0_ID_CHAN2);
	return 0;
}

static int mif_if0_reg_show(struct seq_file *s, void *v)
{
	//opl1()->dbg_mif_reg(s, DI_MIF0_ID_IF0);
	dbg_mif_reg(s, DI_MIF0_ID_IF0);
	return 0;
}

static int mif_if1_reg_show(struct seq_file *s, void *v)
{
	//opl1()->dbg_mif_reg(s, DI_MIF0_ID_IF1);
	dbg_mif_reg(s, DI_MIF0_ID_IF1);
	return 0;
}

static int mif_if2_reg_show(struct seq_file *s, void *v)
{
	//opl1()->dbg_mif_reg(s, DI_MIF0_ID_IF2);
	dbg_mif_reg(s, DI_MIF0_ID_IF2);
	return 0;
}

static int mif_nr_reg_show(struct seq_file *s, void *v)
{
	dbg_mif_wr_bits_show(s, EDI_MIFSM_NR);
	return 0;
}

static int mif_wr_reg_show(struct seq_file *s, void *v)
{
	dbg_mif_wr_bits_show(s, EDI_MIFSM_WR);
	return 0;
}

static int dbg_mif_print_show(struct seq_file *s, void *v)
{
	if (DIM_IS_IC_EF(SC2)) {
		opl1()->dbg_reg_pre_mif_print2();
		opl1()->dbg_reg_pst_mif_print2();
	}
	return 0;
}

/*hdr*/
static int hdr_vd1_reg_show(struct seq_file *s, void *v)
{
	dim_dbg_hdr_reg1(s, v, 1);
	return 0;
}

static int hdr_di_reg_show(struct seq_file *s, void *v)
{
	dim_dbg_hdr_reg1(s, v, 8);
	return 0;
}

/**********************/
DEFINE_SEQ_SHOW_ONLY(dim_reg_cue_int);
DEFINE_SEQ_SHOW_ONLY(policy);
DEFINE_SEQ_SHOW_ONLY(hf_en);

/**********************/
DEFINE_SEQ_SHOW_ONLY(rcfgx);
DEFINE_SEQ_SHOW_ONLY(seq_file_vframe_in);
DEFINE_SEQ_SHOW_ONLY(seq_file_vframe_out);
DEFINE_SEQ_SHOW_ONLY(seq_file_state);
DEFINE_SEQ_SHOW_ONLY(seq_file_stateb);

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

/*afbc*/
DEFINE_SEQ_SHOW_ONLY(dbg_afbc_cfg_v3);
DEFINE_SEQ_SHOW_ONLY(dbg_afd0_reg);
DEFINE_SEQ_SHOW_ONLY(dbg_afd1_reg);
DEFINE_SEQ_SHOW_ONLY(dbg_afd2_reg);
DEFINE_SEQ_SHOW_ONLY(dbg_afd3_reg);
DEFINE_SEQ_SHOW_ONLY(dbg_afd4_reg);
DEFINE_SEQ_SHOW_ONLY(dbg_afd5_reg);
DEFINE_SEQ_SHOW_ONLY(dbg_afd6_reg);
DEFINE_SEQ_SHOW_ONLY(dbg_afd7_reg);
DEFINE_SEQ_SHOW_ONLY(dbg_afd_bits_afbc0);
DEFINE_SEQ_SHOW_ONLY(dbg_afd_bits_afbc1);
DEFINE_SEQ_SHOW_ONLY(dbg_afd_bits_afbc2);
DEFINE_SEQ_SHOW_ONLY(dbg_afd_bits_afbc3);
DEFINE_SEQ_SHOW_ONLY(dbg_afd_bits_afbc4);
DEFINE_SEQ_SHOW_ONLY(dbg_afd_bits_afbc5);
DEFINE_SEQ_SHOW_ONLY(dbg_afd_bits_afbc6);
DEFINE_SEQ_SHOW_ONLY(dbg_afd_bits_afbc7);
DEFINE_SEQ_SHOW_ONLY(dbg_afd_bits_afbce0);
DEFINE_SEQ_SHOW_ONLY(dbg_afd_bits_afbce1);

DEFINE_SEQ_SHOW_ONLY(dbg_afde0_reg);
DEFINE_SEQ_SHOW_ONLY(dbg_afde1_reg);

/* mif */
DEFINE_SEQ_SHOW_ONLY(mif_inp_reg);
DEFINE_SEQ_SHOW_ONLY(mif_mem_reg);
DEFINE_SEQ_SHOW_ONLY(mif_chan2_reg);
DEFINE_SEQ_SHOW_ONLY(mif_if0_reg);
DEFINE_SEQ_SHOW_ONLY(mif_if1_reg);
DEFINE_SEQ_SHOW_ONLY(mif_if2_reg);
DEFINE_SEQ_SHOW_ONLY(mif_nr_reg);
DEFINE_SEQ_SHOW_ONLY(mif_wr_reg);
DEFINE_SEQ_SHOW_ONLY(reg_contr);

/* hdr */
DEFINE_SEQ_SHOW_ONLY(hdr_vd1_reg);
DEFINE_SEQ_SHOW_ONLY(hdr_di_reg);
DEFINE_SEQ_SHOW_ONLY(dim_dbg_hdr_para);

/* hw */
DEFINE_SEQ_SHOW_ONLY(hw_info);

DEFINE_SEQ_SHOW_ONLY(dbg_mif_print);
DEFINE_STORE_ONLY(dbg_crc);
DEFINE_STORE_ONLY(dbg_pip);

/*decontour*/
DEFINE_SEQ_SHOW_ONLY(dbg_dct_mif);
DEFINE_SEQ_SHOW_ONLY(dbg_dct_contr);
DEFINE_SEQ_SHOW_ONLY(dbg_dct_core);
DEFINE_SEQ_SHOW_ONLY(dct_pre_ch);
DEFINE_SEQ_SHOW_ONLY(dct_pre_reg);
DEFINE_SEQ_SHOW_ONLY(dct_pre);
DEFINE_SEQ_SHOW_ONLY(dbg_q_sct);
DEFINE_SEQ_SHOW_ONLY(dbg_sct_peek);
DEFINE_SEQ_SHOW_ONLY(dbg_sct_used_pat);
DEFINE_SEQ_SHOW_ONLY(dim_dbg_sct_top);
DEFINE_SEQ_SHOW_ONLY(dbg_q_nins);
DEFINE_SEQ_SHOW_ONLY(dbg_nins_peek);
DEFINE_SEQ_SHOW_ONLY(dbg_q_ndis);
DEFINE_SEQ_SHOW_ONLY(dbg_q_ndkb);

//test:
#ifdef TST_NEW_INS_INTERFACE
DEFINE_SEQ_SHOW_ONLY(dim_dbg_tst_in);
#endif
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
	{"regctr2", S_IFREG | 0644, &reg_contr_fops},
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
	{"policy", S_IFREG | 0644, &policy_fops},
	{"hf_cfg", S_IFREG | 0644, &hf_en_fops},
	{"afbc_cfg", S_IFREG | 0644, &dbg_afbc_cfg_v3_fops},
	{"reg_afd0", S_IFREG | 0644, &dbg_afd0_reg_fops},
	{"reg_afd1", S_IFREG | 0644, &dbg_afd1_reg_fops},
	{"reg_afd2", S_IFREG | 0644, &dbg_afd2_reg_fops},
	{"reg_afd3", S_IFREG | 0644, &dbg_afd3_reg_fops},
	{"reg_afd4", S_IFREG | 0644, &dbg_afd4_reg_fops},
	{"reg_afd5", S_IFREG | 0644, &dbg_afd5_reg_fops},
	{"reg_afd6", S_IFREG | 0644, &dbg_afd6_reg_fops},
	{"reg_afd7", S_IFREG | 0644, &dbg_afd7_reg_fops},
	{"reg_afe0", S_IFREG | 0644, &dbg_afde0_reg_fops},
	{"reg_afe1", S_IFREG | 0644, &dbg_afde1_reg_fops},
	{"bits_afd0", S_IFREG | 0644, &dbg_afd_bits_afbc0_fops},
	{"bits_afd1", S_IFREG | 0644, &dbg_afd_bits_afbc1_fops},
	{"bits_afd2", S_IFREG | 0644, &dbg_afd_bits_afbc2_fops},
	{"bits_afd3", S_IFREG | 0644, &dbg_afd_bits_afbc3_fops},
	{"bits_afd4", S_IFREG | 0644, &dbg_afd_bits_afbc4_fops},
	{"bits_afd5", S_IFREG | 0644, &dbg_afd_bits_afbc5_fops},
	{"bits_afd6", S_IFREG | 0644, &dbg_afd_bits_afbc6_fops},
	{"bits_afd7", S_IFREG | 0644, &dbg_afd_bits_afbc7_fops},
	{"bits_afe0", S_IFREG | 0644, &dbg_afd_bits_afbce0_fops},
	{"bits_afe1", S_IFREG | 0644, &dbg_afd_bits_afbce1_fops},
	{"reg_mif_inp", S_IFREG | 0644, &mif_inp_reg_fops},
	{"reg_mif_mem", S_IFREG | 0644, &mif_mem_reg_fops},
	{"reg_mif_ch2", S_IFREG | 0644, &mif_chan2_reg_fops},
	{"reg_mif_if0", S_IFREG | 0644, &mif_if0_reg_fops},
	{"reg_mif_if1", S_IFREG | 0644, &mif_if1_reg_fops},
	{"reg_mif_if2", S_IFREG | 0644, &mif_if2_reg_fops},
	{"reg_mif_nr", S_IFREG | 0644, &mif_nr_reg_fops},
	{"reg_mif_wr", S_IFREG | 0644, &mif_wr_reg_fops},
	{"reg_hdr_vd1", S_IFREG | 0644, &hdr_vd1_reg_fops},
	{"reg_hdr_di", S_IFREG | 0644, &hdr_di_reg_fops},
	{"hdr_rpara", S_IFREG | 0644, &dim_dbg_hdr_para_fops},
	{"regmif", S_IFREG | 0644, &dbg_mif_print_fops},
	{"hw_info", S_IFREG | 0644, &hw_info_fops},
	{"dct_mif", S_IFREG | 0644, &dbg_dct_mif_fops},
	{"dct_ctr", S_IFREG | 0644, &dbg_dct_contr_fops},
	{"dct_other", S_IFREG | 0644, &dbg_dct_core_fops},
	{"dct_preh", S_IFREG | 0644, &dct_pre_fops},
	{"dct_pre_reg", S_IFREG | 0644, &dct_pre_reg_fops},
#ifdef TST_NEW_INS_INTERFACE
	{"tst_list_in", S_IFREG | 0644, &dim_dbg_tst_in_fops},
#endif

};

static const struct di_dbgfs_files_t di_debugfs_files[] = {
	{"rcfgx", S_IFREG | 0644, &rcfgx_fops},
	{"wcfgx", S_IFREG | 0644, &wcfgx_fops},
	{"rvfm_in", S_IFREG | 0644, &seq_file_vframe_in_fops},
	{"rvfm_out", S_IFREG | 0644, &seq_file_vframe_out_fops},
	{"state", S_IFREG | 0644, &seq_file_state_fops},
	{"stateb", S_IFREG | 0644, &seq_file_stateb_fops},
	{"dumpmif", S_IFREG | 0644, &seq_file_mif_fops},
	{"test_reg", S_IFREG | 0644, &reg_fops},
	{"sum", S_IFREG | 0644, &seq_file_sum_fops},
	{"mpxr", S_IFREG | 0644, &mpxr_fops},
	{"mpxw", S_IFREG | 0644, &mpxw_fops},
	{"vfmc", S_IFREG | 0644, &seq_file_curr_vframe_fops},
	{"dbg_crc", S_IFREG | 0644, &dbg_crc_fops},
	{"dbg_pip", S_IFREG | 0644, &dbg_pip_fops},
	{"dct_pre_ch", S_IFREG | 0644, &dct_pre_ch_fops},
	{"sct_top", S_IFREG | 0644, &dim_dbg_sct_top_fops},
	{"list_sct", S_IFREG | 0644, &dbg_q_sct_fops},
	{"list_sct_peek", S_IFREG | 0644, &dbg_sct_peek_fops},
	{"list_sct_used", S_IFREG | 0644, &dbg_sct_used_pat_fops},
	{"list_ndis", S_IFREG | 0644, &dbg_q_ndis_fops},
	{"list_ndkb", S_IFREG | 0644, &dbg_q_ndkb_fops},
	{"list_nin", S_IFREG | 0644, &dbg_q_nins_fops},
	{"list_nin_peek", S_IFREG | 0644, &dbg_nins_peek_fops}
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

