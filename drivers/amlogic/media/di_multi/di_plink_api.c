// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <linux/semaphore.h>
#include <linux/kfifo.h>
#include <linux/spinlock.h>
#include <linux/amlogic/tee.h>

#include "deinterlace.h"
#include "di_data_l.h"
#include "di_data.h"
#include "di_pre.h"
#include "di_prc.h"
#include "di_dbg.h"
#include "di_que.h"

#include "di_vframe.h"
#include "di_task.h"
#include "di_sys.h"
#include "di_api.h"
#include "di_reg_v3.h"
#include "di_hw_v3.h"
#include "di_reg_v2.h"
#include "di_afbc_v3.h"

#include "register.h"
#include "register_nr4.h"

#include <linux/dma-contiguous.h>
#include <linux/amlogic/media/codec_mm/codec_mm.h>

//#ifdef TEST_PRE_VPP_LINK
//#define DIM_PRE_VPP_NAME	"deinterlace"//"dimulti.1" //"di_pre_vpp"
//#else
#define DIM_PRE_VPP_NAME	"di_pre_vpp" //"di_pre_vpp"
//#endif

/* MAX_FIFO_SIZE */
#define MAX_FIFO_SIZE_PVPP		(32)
//#define AUTO_NR_DISABLE		(1)
//#define DBG_TIMER		(1)
//#define DBG_UNREG_LOG		(1)
//#define DBG_CHECK_VF		(1)
//#define DBG_QUE_LOCAL		(1)
//#define DBG_FLOW_SETTING		(1)
//#define DBG_QUE_IN_OUT		(1)
//#define DBG_QUE_INTERFACE	(1)
//#define DBG_OUT_SIZE		(1)
static DEFINE_SPINLOCK(lock_pvpp);
//spin_lock_irqsave(&lock_pvpp, irq_flag)
//spin_unlock_irqrestore(&lock_pvpp, irq_flag)

/*VFRAME_EVENT_PROVIDER_*/
#define VP_EVENT(cc)	(VFRAME_EVENT_PROVIDER_##cc)

static void dpvpph_reg_setting(const struct reg_acc *op_in);
static void dpvpph_size_change(struct dim_prevpp_ds_s *ds,
			       struct dimn_dvfm_s *dvfm,
			       const struct reg_acc *op_in);
static void dpvpph_display_update_all(struct dim_prevpp_ds_s *ds,
			       struct dimn_dvfm_s *dvfm,
			       const struct reg_acc *op_in);
//static
void dpvpph_display_update_part(struct dim_prevpp_ds_s *ds,
			       struct dimn_dvfm_s *ndvfm,
			       const struct reg_acc *op_in,
			       unsigned int diff);
static void dpvpph_pre_data_mif_ctrl(bool enable, const struct reg_acc *op_in);
static void dpvpph_reverse_mif_ctrl(bool reverse, unsigned int hv_mirror,
					const struct reg_acc *op_in);

static void dpvpph_gl_sw(bool on, bool pvpp_link, const struct reg_acc *op);
static void dpvpph_prelink_sw(const struct reg_acc *op, bool p_link);

static void dpvpp_ins_fill_out(struct dimn_itf_s *itf);

static void mif_cfg_v2(struct DI_MIF_S *di_mif,
		    struct dvfm_s *pvfm,
		    struct dim_mifpara_s *ppara);
static bool dpvpp_parser_nr(struct dimn_itf_s *itf);
static bool vtype_fill_d(struct dimn_itf_s *itf,
		struct vframe_s *vfmt,
		struct vframe_s *vfmf,
		struct dvfm_s *by_dvfm);
//disable irq sw static void unreg_irq(void);
static struct vframe_s *dpvpp_get_vf_base(struct dimn_itf_s *itf);
//static
//void dpvpph_unreg_setting(void);
static int dpvpp_reg_link_sw(bool vpp_disable_async);
static int dpvpp_link_sw_by_di(bool sw);
static void dpvpph_prelink_polling_check(const struct reg_acc *op, bool p_link);
static int dpvpp_link_sw_api(bool sw);
static int dpvpp_reg_idle_ch(void);
static void dpvpph_pre_frame_reset_g12(bool pvpp_link,
				       const struct reg_acc *op_in);
static void dpvpp_patch_first_buffer(struct dimn_itf_s *itf);
static unsigned int dpvpp_create_instance_id(unsigned int ch);
static unsigned int dpvpp_dbg_force_dech;
module_param_named(dpvpp_dbg_force_dech, dpvpp_dbg_force_dech, uint, 0664);
static unsigned int dpvpp_dbg_force_decv;
module_param_named(dpvpp_dbg_force_decv, dpvpp_dbg_force_decv, uint, 0664);

/* refor to enum EPVPP_BUF_CFG_T */
const char *const name_buf_cfg[] = {
	"hd_f",
	"hd_f_tvp",
	"uhd_f_afbce",
	"uhd_f_afbce_tvp",
	"uhd_f_afbce_hd",
	"uhd_f_afbce_hd_tvp",
	"hd_nv21",
	"hd_nv21_tvp",
	"uhd_nv21",
	"uhd_nv21_tvp",
	"uhd_f",
	"uhd_f_tvp",
};

//static
bool timer_cnt(unsigned long *ptimer, unsigned int hs_nub)
{
	unsigned long delay;
	bool ret = false;

	if (!hs_nub) {
		*ptimer = jiffies;
		return false;
	}

	/* debug print */
	delay = *ptimer + hs_nub * HZ;
	if (time_after(jiffies, delay)) {
		ret = true;
		*ptimer = jiffies;
	}
	return ret;
}

//move to di_plink_api.c
/**********************************************
 * bit [0:1] for debug level
 * bit 2 for vpp
 * bit 3 for di
 * bit 4 creat vpp link
 * bit 5 vpp-link is post write;
 * bit 6 nr disable all setting;
 * bit 7 nr disable irq setting;
 * bit 8 bypass in parser
 * bit 9 not active by di
 * bit [10:11] debug for only set one time;
 * bit [12]: trig update all once;
 * bit [13]: force di do nothing;
 * bit [14]: check vpp virtue active
 * bit [15]: force vpp active
 * bit [16]: force mem bypass
 * bit [17]: force update all
 * bit [18]: force not write nr
 * bit [19]: force bypass top	//in parser
 * bit [20]: force pause
 * bit [21]: insert vfm?
 * bit [22]: force not crop win;
 * bit [23]: force bypass dct post;
 * bit [24]: dbg irq en
 * bit [25]: force not bypass mem
 * bit [28]: bypass in display
 * bit [29]: force pre-vpp link on;
 **********************************************/
static unsigned int tst_pre_vpp;
module_param_named(tst_pre_vpp, tst_pre_vpp, uint, 0664);

#define dbg_plink(level, fmt, args ...)		\
	do {						\
		if ((tst_pre_vpp & 0x3) < (level))	\
			break;				\
		pr_info("dim:" fmt, ##args);		\
	} while (0)

#define dbg_plink1(fmt, args ...)		dbg_plink(1, fmt, ##args)
#define dbg_plink2(fmt, args ...)		dbg_plink(2, fmt, ##args)
#define dbg_plink3(fmt, args ...)		dbg_plink(3, fmt, ##args)

//bit 0 for vpp
bool dim_is_pre_link(void)
{
	if (tst_pre_vpp & DI_BIT2)
		return true;
	return false;
}

bool dim_is_pre_link_l(void)
{
	if (tst_pre_vpp & DI_BIT3)
		return true;

	return false;
}

bool dim_is_creat_p_vpp_link(void)
{
	if (tst_pre_vpp & DI_BIT4)
		return true;
	return false;
}

static bool dim_is_pst_wr(void)
{
	if (tst_pre_vpp & DI_BIT5)
		return true;
	return false;
}

static unsigned char dim_dbg_is_disable_nr(void)
{
	unsigned char ret = 0;

	if (tst_pre_vpp & DI_BIT6)
		ret |= DI_BIT7;

	if (tst_pre_vpp & DI_BIT7)
		ret |= DI_BIT0;

	return ret;
}

//move to di_plink_api.c

static bool dpvpp_is_bypass(void)
{
	if (tst_pre_vpp & DI_BIT8)
		return true;
	return false;
}

static bool dpvpp_is_not_active_di(void)
{
	if (tst_pre_vpp & DI_BIT9)
		return true;
	return false;
}

// return : true: update; false: not update
static bool dpvpp_set_one_time(void)
{
	if (!(tst_pre_vpp & DI_BIT10))
		return true;
	if (!(tst_pre_vpp & DI_BIT11)) {
		tst_pre_vpp |= DI_BIT11;
		return true; //need set
	}
	return false;
}

static bool dpvpp_dbg_trig_change_all_one(void)
{
	if (tst_pre_vpp & DI_BIT12) {
		tst_pre_vpp &= (~DI_BIT12);
		return true;
	}
	return false;
}

static bool dpvpp_dbg_force_same(void)
{
	if (tst_pre_vpp & DI_BIT13)
		return true;

	return false;
}

static bool dpvpp_dbg_vpp_active(void)
{
	if (tst_pre_vpp & DI_BIT14)
		return true;

	return false;
}

static bool dpvpp_dbg_force_vpp_active(void)
{
	if (tst_pre_vpp & DI_BIT15)
		return true;

	return false;
}

static bool dpvpp_dbg_force_mem_bypass(void)
{
	if (tst_pre_vpp & DI_BIT16)
		return true;

	return false;
}

static bool dpvpp_dbg_force_update_all(void)
{
	if (tst_pre_vpp & DI_BIT17)
		return true;

	return false;
}

static bool dpvpp_dbg_force_disable_ddr_wr(void)
{
	if (tst_pre_vpp & DI_BIT18) {
		tst_pre_vpp |= DI_BIT16;//need bypass mem
		return true;
	}
	return false;
}

static bool dpvpp_dbg_force_bypass_2(void)
{
	if (tst_pre_vpp & DI_BIT19)
		return true;

	if (dimp_get(edi_mp_di_debug_flag) & 0x100000)
		return true;

	return false;
}

static bool dpvpp_dbg_force_no_crop(void)
{
	if (tst_pre_vpp & DI_BIT22)
		return true;
	return false;
}

static bool dpvpp_dbg_force_bypass_dct_post(void)
{
	if (tst_pre_vpp & DI_BIT23)
		return true;
	return false;
}

bool dpvpp_dbg_en_irq(void)
{
	if (tst_pre_vpp & DI_BIT24)
		return true;
	return false;
}

static bool dpvpp_dbg_force_mem_en(void)
{
	if (tst_pre_vpp & DI_BIT25)
		return true;

	return false;
}

static bool dpvpp_bypass_display(void)
{
	if (tst_pre_vpp & DI_BIT28)
		return true;
	return false;
}

static void ext_vpp_disable_prelink_notify(bool async)
{
#ifndef VPP_LINK_USED_FUNC
	u64 ustime, udiff;

	dbg_plink1("ext:%s:begin\n", "di_disable_prelink_notify");
	ustime = cur_to_usecs();
	di_disable_prelink_notify(async);
	udiff = cur_to_usecs() - ustime;
	dbg_plink1("ext:%s:%dus:%d\n", "di_disable_prelink_notify",
	       (unsigned int)udiff, async);
#else
	PR_WARN("dbg:%s:no this function\n", __func__);
#endif
}

static void ext_vpp_prelink_state_changed_notify(void)
{
#ifndef VPP_LINK_USED_FUNC
	dbg_plink1("ext:prelink notify:begin\n");
	di_prelink_state_changed_notify();
	dbg_plink1("ext:prelink notify\n");
#else
	PR_WARN("dbg:%s:no this function\n", __func__);
#endif
}

/* allow vpp sw other*/
static void ext_vpp_prelink_real_sw(bool sw, bool wait)
{
#ifndef VPP_LINK_USED_FUNC
	di_prelink_force_dmc_priority(sw, wait);
#else
	PR_WARN("dbg:%s:no this function\n", __func__);
#endif
}

#ifdef DBG_FLOW_SETTING

void dim_print_win(struct di_win_s *win, char *name)
{
	if (!win) {
		PR_INF("%s:%s:no\n", __func__, name);
		return;
	}
	PR_INF("%s:%s:\n", __func__, name);

	PR_INF("%s:[%d:%d:%d:%d:%d:%d]:\n", "win",
		(unsigned int)win->x_size,
		(unsigned int)win->y_size,
		(unsigned int)win->x_end,
		(unsigned int)win->y_end,
		(unsigned int)win->x_st,
		(unsigned int)win->x_st);
}

int print_dim_mifpara(struct dim_mifpara_s *p, char *name)
{
	if (!p) {
		PR_INF("%s:%s:no\n", __func__, name);
		return 0;
	}
	PR_INF("%s:%s:\n", __func__, name);
	PR_INF("%s:0x%x:\n", "cur_inp_type", p->cur_inp_type);
	PR_INF("%s:0x%x:\n", "prog_proc_type", p->prog_proc_type);
//	PR_INF("%s:%d:\n", "linear", p->linear);
	PR_INF("%s:[0x%x:0x%x:0x%x:]:\n", "cvs_id",
		(unsigned int)p->cvs_id[0],
		(unsigned int)p->cvs_id[1],
		(unsigned int)p->cvs_id[2]);
	dim_print_win(&p->win, name);

	return 0;
}

int print_pvpp_dis_para_in(struct pvpp_dis_para_in_s *p, char *name)
{
	if (!p)
		return 0;

	PR_INF("pvpp_dis_para_in:%s", name);
	PR_INF("\t%s:0x%x", "dmode", p->dmode);
	dim_print_win(&p->win, name);
	return 0;
}
#endif /* DBG_FLOW_SETTING */

static const struct reg_acc dpvpp_regset;

/* get by other module not mean get by vpp, so need vpp mark */
static struct dimn_dvfm_s *dpvpp_check_dvfm_act(struct dimn_itf_s *itf,
						struct vframe_s *vf)
{
	struct dimn_dvfm_s *dvfm = NULL;

	if (!itf || !itf->ds ||
	    !vf || !vf->private_data || vf->di_flag & DI_FLAG_DI_BYPASS)
		return NULL;

	dvfm = (struct dimn_dvfm_s *)vf->private_data;
	if (!dvfm)
		return NULL;

	if (itf->sum_reg_cnt != vf->di_instance_id) {
		PR_ERR("not active:no get:0x%px\n", vf);
		return NULL;
	}
	return dvfm;
}

static bool instance_id_check(unsigned int instance_id)
{
	struct dimn_itf_s *itf;
	int i;
	struct dim_dvs_prevpp_s *dv_prevpp;
	bool ret = false;

	dv_prevpp = &get_datal()->dvs_prevpp;

	for (i = 0; i < DI_PLINK_CN_NUB; i++) {
		itf = &dv_prevpp->itf[i];
		if (atomic_read(&itf->reg) &&
		    itf->sum_reg_cnt == instance_id) {
			ret = true;
			break;
		}
	}

	return ret;
}

static struct dimn_itf_s *get_itf_vfm(struct vframe_s *vfm)
{
	//struct dimn_itf_s *itf;
	struct dimn_dvfm_s *dvfm = NULL;
	struct dim_pvpp_hw_s *hw;

	if (!vfm)
		return NULL;

	if (!(vfm->di_flag & DI_FLAG_DI_PVPPLINK))
		return NULL;
	if (!vfm->private_data)
		return NULL;
	if (!instance_id_check(vfm->di_instance_id)) {
		hw = &get_datal()->dvs_prevpp.hw;
		PR_WARN("%s:vfm illegal:0x%px:h[%d],%d\n",
			__func__,
			vfm, hw->reg_nub,
			vfm->di_instance_id);
		return NULL;
	}

	dvfm = (struct dimn_dvfm_s *)vfm->private_data;
	if (!dvfm)
		return NULL;
	if (!dvfm->itf)
		return NULL;
	if (dvfm->itf->sum_reg_cnt != vfm->di_instance_id) {
		PR_ERR("%s:id not map:0x%px:%d:%d\n",
			__func__, vfm,
			vfm->di_instance_id,
			dvfm->itf->sum_reg_cnt);
		return NULL;
	}
	return dvfm->itf;
}

/* */
static void dbg_display_status(struct vframe_s *vfm,
			 struct pvpp_dis_para_in_s *in_para,
			 void *out_para,
			 int ret)
{
	struct dimn_itf_s *itf;
	unsigned int dis_status;
	static unsigned int lst_sts;

	if (!dpvpp_dbg_en_irq())
		return;
	if (!vfm)
		return;
	itf = get_itf_vfm(vfm);
	if (!itf) {
		PR_INF("dbg:sts:no itf\n");
		return;
	}
	dis_status = atomic_read(&itf->c.dbg_display_sts);
	if (lst_sts != dis_status) {
		PR_INF("dbg:sts:0x%08x -> 0x%08x\n", lst_sts, dis_status);
		lst_sts = dis_status;
	}
}

void dbg_pre_set(unsigned int pos)
{
	if (!dpvpp_dbg_en_irq())
		return;

	PR_INF("dbg:pre_s:0x%08x\n", pos);
}

void dbg_irq_status(unsigned int pos)
{
	if (!dpvpp_dbg_en_irq())
		return;

	PR_INF("dbg:irq:0x%08x\n", pos);
}

static void dbg_check_ud(struct dim_prevpp_ds_s *ds, unsigned int dbgid)
{
#ifdef DBG_CHECK_VF
	int i;
	struct vframe_s *vf;
	void *p;
	ud tmp, tmp2;
	struct dimn_itf_s *itf;

	if (!ds || !ds->en_dbg_check_ud)
		return;
	itf = ds->itf;
	for (i = 0; i < DIM_LKIN_NUB; i++) {
		vf = dpvpp_get_vf_base(itf) + i;
		tmp = (ud)vf->private_data;
		p = &ds->lk_in_bf[i];
		tmp2 = (ud)p;
		if (tmp != tmp2) {
			PR_ERR("%s:%d:ud[%d]:0x%lx -> 0x%lx\n",
				__func__,
				dbgid,
				i,
				(unsigned long)tmp2,
				(unsigned long)tmp);
		}
	}
#endif
}

static void dbg_check_vf(struct dim_prevpp_ds_s *ds,
			 struct vframe_s *vf, unsigned int dbgid)
{
#ifdef DBG_CHECK_VF
	int i;
	ud tmp;
	bool ret = false;
	struct vframe_s *vf_l;
	struct dimn_itf_s *itf;

	if (!ds || !ds->en_dbg_check_vf)
		return;
	itf = ds->itf;
	for (i = 0; i < DIM_LKIN_NUB; i++) {
		tmp = (ud)vf;
		vf_l = dpvpp_get_vf_base(itf) + i;
		if (tmp == (ud)vf_l) {
			ret = true;
			break;
		}
	}
	if (!ret)
		PR_ERR("%s:%d:0x%px\n", __func__, dbgid, vf);
#endif
}

void dbg_cvs_addr_show(struct seq_file *s)
{
	//const struct reg_acc *ops;
	unsigned int tmp, cvs0, cvs1;
	unsigned long addr;
	char *spltb = "---------------------------";

	seq_printf(s, "cvs->addr:%s\n", spltb);
	/*input */
	tmp = VSYNC_RD_MPEG_REG(DI_INP_CANVAS0);
	cvs0 = tmp & 0xff;
	cvs1 = (tmp & 0xff00) >> 8;
	addr = canvas_get_addr(cvs0);
	seq_printf(s, "%s:reg[0x%x]\n", "in", DI_INP_CANVAS0);
	seq_printf(s, "\tcvs0[%d], 0x%lx\n", cvs0, addr);
	addr = canvas_get_addr(cvs1);
	seq_printf(s, "\tcvs1[%d], 0x%lx\n", cvs1, addr);

	/* mem */
	tmp = VSYNC_RD_MPEG_REG(DI_MEM_CANVAS0);
	cvs0 = tmp & 0xff;
	cvs1 = (tmp & 0xff00) >> 8;
	addr = canvas_get_addr(cvs0);
	seq_printf(s, "%s:reg[0x%x]\n", "mem", DI_INP_CANVAS0);
	seq_printf(s, "\tcvs0[%d], 0x%lx\n", cvs0, addr);
	addr = canvas_get_addr(cvs1);
	seq_printf(s, "\tcvs1[%d], 0x%lx\n", cvs1, addr);

	/*nr */
	tmp = VSYNC_RD_MPEG_REG(DI_NRWR_CTRL);
	cvs0 = tmp & 0xff;
	cvs1 = (tmp & 0xff00) >> 8;
	addr = canvas_get_addr(cvs0);
	seq_printf(s, "%s:reg[0x%x]\n", "nr", DI_NRWR_CTRL);
	seq_printf(s, "\tcvs0[%d], 0x%lx\n", cvs0, addr);
	addr = canvas_get_addr(cvs1);
	seq_printf(s, "\tcvs1[%d], 0x%lx\n", cvs1, addr);
}

int dpvpp_show_ds(struct seq_file *s, struct dim_prevpp_ds_s *ds)
{
	char *spltb = "---------------------------";

	if (!ds) {
		seq_printf(s, "%s,no ds\n", __func__);
		return 0;
	}
	seq_printf(s, "%s\n", spltb); /*----*/
	seq_printf(s, "%s\n", "ds-sum");
	seq_printf(s, "\tque:%d:%d:%d:%d:%d\n",
		   ds->sum_que_idle,
		   ds->sum_que_in,
		   ds->sum_que_ready,
		   ds->sum_que_recycle,
		   ds->sum_que_kback);
	seq_printf(s, "\tdct:%d:%d:\n",
		   ds->dct_sum_in,
		   ds->dct_sum_o);
	seq_printf(s, "%s\n", spltb); /*----*/
	return 0;
}

/************************************************
 * dump
 ************************************************/
int dpvpp_show_itf(struct seq_file *s, struct dimn_itf_s *itf)
{
	char *spltb = "---------------------------";
	int i;

	if (!itf) {
		seq_printf(s, "%s,no itf\n", __func__);
		return 0;
	}

	seq_printf(s, "%s\n", spltb); /*----*/
	seq_printf(s, "%s\n", "itf");
	seq_printf(s, "0x%x:id[%d]:ch[%d]\n", itf->ch, itf->id, itf->bind_ch);
	seq_printf(s, "%s:%d:%d\n", "reg", atomic_read(&itf->reg), itf->c.reg_di);
#ifdef DIM_PLINK_ENABLE_CREATE
	seq_printf(s, "%s:%d\n", "regging_api", atomic_read(&itf->regging_api));
	seq_printf(s, "%s:%d\n", "unregging_api", atomic_read(&itf->unregging_api));
#endif
	seq_printf(s, "%s:%d\n", "etype:1:vfm", itf->etype);
	seq_printf(s, "%s:%s\n", "name", itf->dvfm.name);
	seq_printf(s, "%s:0x%x:0x%x:0x%x\n", "srcx",
		   itf->src_need,
		   itf->c.src_state,
		   itf->c.src_last);
	seq_printf(s, "%s:%d\n", "bypass_c", itf->bypass_complete);
	seq_printf(s, "%s:%d\n", "bypass_reason", itf->c.bypass_reason);
	seq_printf(s, "%s:%d:%d:%d:%d\n", "pause",
		   itf->c.pause_pst_get,
		   itf->c.pause_pre_get,
		   itf->c.pause_parser,
		   itf->c.flg_block);
	seq_printf(s, "%s:\n", "sum");
	seq_printf(s, "\t%s:%d\n", "pre_get", itf->c.sum_pre_get);
	seq_printf(s, "\t%s:%d\n", "pre_put", itf->c.sum_pre_put);
	seq_printf(s, "\t%s:%d\n", "pst_get", itf->c.sum_pst_get);
	seq_printf(s, "\t%s:%d\n", "pst_put", itf->c.sum_pst_put);
	seq_printf(s, "\t%s:%d\n", "reg_cnt", itf->sum_reg_cnt);

	/* show vf_bf */
	seq_printf(s, "%s\n", spltb); /*----*/
	seq_printf(s, "%s:\n", "bf_vf_addr");
	for (i = 0; i < DIM_LKIN_NUB; i++)
		seq_printf(s, "\t%d:0x%px\n", i, &itf->vf_bf[i]);

	/* show buf_bf */
	seq_printf(s, "%s\n", spltb); /*----*/
	seq_printf(s, "%s:\n", "bf_buf_addr");
	for (i = 0; i < DIM_LKIN_NUB; i++)
		seq_printf(s, "\t%d:0x%px\n", i, &itf->buf_bf[i]);
	seq_printf(s, "%s\n", spltb); /*----*/
	if (itf->ext_vfm)
		seq_printf(s, "%s:\n", "ext_vfm");
	else
		seq_printf(s, "%s:\n", "ext_vfm null");
	if (itf->ext_ins)
		seq_printf(s, "%s:\n", "ext_ins");
	else
		seq_printf(s, "%s:\n", "ext_ins null");

	if (itf->ops_vfm)
		seq_printf(s, "%s:\n", "ops_vfm");
	else
		seq_printf(s, "%s:\n", "ops_vfm null");

	return 0;
}

int dpvpp_show_que(struct seq_file *s, struct dimn_qs_cls_s *que, unsigned char level)
{
	char *spltb = "---------------------------";
	ud rtab[MAX_FIFO_SIZE_PVPP];
	unsigned int rsize;
	int i;

	/*******************
	 * level:
	 *	0: only number
	 *	1: base infor
	 *	2: list
	 *******************/
	seq_printf(s, "%s\n", spltb);
	if (!que) {
		seq_printf(s, "%s,no que\n", __func__);
		return 0;
	}
	seq_printf(s, "%s:%d\n", que->cfg.name, level);
	seq_printf(s, "%s:%d\n", "cnt", que->ops.count(que));

	if (level < 1)
		return 0;
	/* level 1 */
	seq_printf(s, "%s:%d\n", "flg", que->flg);
	seq_printf(s, "%s:0x%x\n", "flg_lock", que->cfg.flg_lock);
	seq_printf(s, "%s:0x%x\n", "ele_nub", que->cfg.ele_nub);
	seq_printf(s, "%s:0x%x\n", "ele_size", que->cfg.ele_size);
	seq_printf(s, "%s:0x%x\n", "ele_mod", que->cfg.ele_mod);
	seq_printf(s, "%s:0x%x\n", "qsize", que->size);

	if (level < 2)
		return 0;
	rsize = que->ops.list(que, &rtab[0]);
	if (!rsize) {
		seq_printf(s, "%s:\n", "no list");
		return 0;
	}
	seq_printf(s, "%s:%d\n", "list:", rsize);
	for (i  = 0; i < rsize; i++)
		seq_printf(s, "\t%d:0x%px\n", i, (void *)rtab[i]);

	return 0;
}

int dpvpp_show_ndvfm(struct seq_file *s, struct dimn_dvfm_s *ndvfm)
{
	char *spltb = "---------------------------";
	char *splta = "--------------";

	seq_printf(s, "%s\n", spltb);
	if (!ndvfm) {
		seq_printf(s, "%s,no ndvfm\n", __func__);
		return 0;
	}

	dbg_hd(s, &ndvfm->header);
	seq_printf(s, "%s:0x%px\n", "addr", ndvfm);
	if (ndvfm->itf)
		seq_printf(s, "%s:[%d]\n", "itf id", ndvfm->itf->id);

	seq_printf(s, "%s\n", splta);
	seq_printf(s, "%s:%d\n", "etype:1:vfm", ndvfm->etype);
	seq_printf(s, "%s:0x%px\n", "pvf", ndvfm->pvf);
	seq_printf(s, "%s:0x%px\n", "ori_in", ndvfm->c.ori_in);
	seq_printf(s, "%s:%d\n", "cnt_in", ndvfm->c.cnt_in);
	seq_printf(s, "%s:%d,%d\n", "src", ndvfm->c.src_w, ndvfm->c.src_h);
	seq_printf(s, "%s:%d:%d:%d:%d:%d\n", "wr_done",
		   atomic_read(&ndvfm->c.wr_done),
		   atomic_read(&ndvfm->c.flg_vpp),
		   atomic_read(&ndvfm->c.flg_get),
		   atomic_read(&ndvfm->c.flg_ref),
		   atomic_read(&ndvfm->c.flg_show));
	seq_printf(s, "%s:%d:0x%px\n", "mem_cnt",
		   ndvfm->c.mem_cnt,
		   ndvfm->c.mem_link);
	seq_printf(s, "in_dvfm:%s\n", splta);
	seq_file_dvfm(s, NULL, &ndvfm->c.in_dvfm);
	seq_printf(s, "out_dvfm:%s\n", splta);
	seq_file_dvfm(s, NULL, &ndvfm->c.out_dvfm);
	seq_printf(s, "%s\n", spltb);
	return 0;
}

void kfifo_test(struct seq_file *s)
{
	struct kfifo	tmp_kfifo;
	ud rtab[MAX_FIFO_SIZE_PVPP], tmp;
	int i, ret;
	unsigned long val_out;

	ret = kfifo_alloc(&tmp_kfifo,
			  tst_quep_ele * MAX_FIFO_SIZE_PVPP,
			  GFP_KERNEL);
	if (ret < 0) {
		seq_printf(s, "err:%s:alloc kfifo err:%s\n", __func__, "tmp");
		return;
	}
	tmp = (ud)&i;
	kfifo_put(&tmp_kfifo, tmp);
	seq_printf(s, "put i address test:0x%px,esize[%d],recsize[0x%zx],size[%d],len[%d]\n",
		   &i,
		   kfifo_esize(&tmp_kfifo),
		   kfifo_recsize(&tmp_kfifo),
		   kfifo_size(&tmp_kfifo),
		   kfifo_len(&tmp_kfifo));
	seq_printf(s, "%s:%d\n", "kfifo_peek_len", kfifo_peek_len(&tmp_kfifo));
	tmp = 0;
	val_out = kfifo_peek(&tmp_kfifo, &tmp);
	seq_printf(s, "peek test:%lx:0x%lx\n", val_out, (unsigned long)tmp);
	tmp = 0;
	val_out = kfifo_get(&tmp_kfifo, &tmp);
	seq_printf(s, "get 0 test:0x%lx:0x%lx,esize[%d],recsize[0x%zx],size[%d],len[%d]\n",
		   val_out,
		   (unsigned long)tmp,
		   kfifo_esize(&tmp_kfifo),
		   kfifo_recsize(&tmp_kfifo),
		   kfifo_size(&tmp_kfifo),
		   kfifo_len(&tmp_kfifo));
	seq_printf(s, "%s:%d\n", "kfifo_peek_len", kfifo_peek_len(&tmp_kfifo));
	tmp = (ud)&ret;
	ret = kfifo_in(&tmp_kfifo, &tmp, tst_quep_ele);
	seq_printf(s, "in 1 address test:[nub]%d:[content]0x%px,esize[%d],recsize[0x%zx],size[%d],len[%d]\n",
			   ret,
			   &ret,
			   kfifo_esize(&tmp_kfifo),
			   kfifo_recsize(&tmp_kfifo),
			   kfifo_size(&tmp_kfifo),
			   kfifo_len(&tmp_kfifo));
	seq_printf(s, "%s:%d\n", "kfifo_peek_len", kfifo_peek_len(&tmp_kfifo));
	tmp = (ud)&i;
	ret = kfifo_in(&tmp_kfifo, &tmp, tst_quep_ele);
	seq_printf(s, "in 2 address test:[nub]%d:[content]0x%px,esize[%d],recsize[0x%zx],size[%d],len[%d]\n",
			   ret,
			   &i,
			   kfifo_esize(&tmp_kfifo),
			   kfifo_recsize(&tmp_kfifo),
			   kfifo_size(&tmp_kfifo),
			   kfifo_len(&tmp_kfifo));
	seq_printf(s, "%s:%d\n", "kfifo_peek_len", kfifo_peek_len(&tmp_kfifo));
	for (i = 0; i < MAX_FIFO_SIZE_PVPP; i++)
		rtab[i] = 0;
	ret = kfifo_out_peek(&tmp_kfifo, &rtab[0], tst_quep_ele * 2);
	seq_printf(s, "out peek test:%d\n", ret);
	for (i = 0; i < 4; i++)
		seq_printf(s, "out peek test:%d:0x%lx\n",
			   i, rtab[i]);

	kfifo_free(&tmp_kfifo);
}

int dpvpp_itf_show(struct seq_file *s, void *what)
{
	struct dimn_itf_s *itf;
	int *chp;
	unsigned int ch;
	struct di_ch_s *pch;

	chp = (int *)s->private;
	ch = *chp;
	pch = get_chdata(ch);
	if (pch->itf.pre_vpp_link && pch->itf.p_itf) {
		itf = pch->itf.p_itf;
	} else {
		seq_printf(s, "%s:ch[%d] not pre-vpp link\n", __func__, ch);
		return 0;
	}

	seq_printf(s, "%s:ch[%d]\n", __func__, *chp);

	dpvpp_show_itf(s, itf);
	dpvpp_show_ds(s, itf->ds);
	return 0;
}

static void VS_REG_WR(unsigned int addr, unsigned int val)
{
	VSYNC_WR_MPEG_REG(addr, val);
}

/* dim_VSYNC_WR_MPEG_REG_BITS */

static unsigned int VS_REG_WRB(unsigned int addr, unsigned int val,
			       unsigned int start, unsigned int len)
{
	VSYNC_WR_MPEG_REG_BITS(addr, val, start, len);

	return 0;
}

static unsigned int VS_REG_RD(unsigned int addr)
{
	return VSYNC_RD_MPEG_REG(addr);
}

static unsigned int get_reg_bits(unsigned int val, unsigned int bstart,
				 unsigned int bw)
{
	return((val &
	       (((1L << bw) - 1) << bstart)) >> (bstart));
}

static unsigned int VS_REG_RDB(unsigned int adr, unsigned int start,
			    unsigned int len)
{
	unsigned int val;

	val = VS_REG_RD(adr);

	return get_reg_bits(val, start, len);
}

static const struct reg_acc dpvpp_regset = {
	.wr = VS_REG_WR,
	.rd = VS_REG_RD,
	.bwr = VS_REG_WRB,
	.brd = VS_REG_RDB,
};

static struct vframe_s *dpvpp_get_vf_base(struct dimn_itf_s *itf)
{
	return &itf->vf_bf[0];
}

#ifdef DIM_PLINK_ENABLE_CREATE
static int event_qurey_state(struct dimn_itf_s *itf)
{
	/*int in_buf_num = 0;*/
	struct vframe_states states;

	/*fix for ucode reset method be break by di.20151230*/
	if (!atomic_read(&itf->reg))
		return RECEIVER_INACTIVE;
	if (itf->ops_vfm && itf->ops_vfm->et_states) {
		itf->ops_vfm->et_states(itf, &states);

		if (states.buf_avail_num > 0)
			return RECEIVER_ACTIVE;
	}

	if (vf_notify_receiver(itf->dvfm.name,
				  VP_EVENT(QUREY_STATE),
				  NULL) == RECEIVER_ACTIVE)
		return RECEIVER_ACTIVE;

	return RECEIVER_INACTIVE;
}
#endif /* DIM_PLINK_ENABLE_CREATE */

static struct vframe_s *in_patch_first_buffer(struct dimn_itf_s *itf)
{
	unsigned int cnt;
	struct buf_que_s *pbufq;
	union q_buf_u q_buf;
	struct dim_nins_s *ins;
	bool ret;
	unsigned int qt_in;
	unsigned int bindex;
	struct di_ch_s *pch;
	struct vframe_s *vf = NULL;

	if (!itf)
		return NULL;

	if (itf->bind_ch >= DI_CHANNEL_MAX) {
		PR_ERR("%s:ch[%d] overflow\n", __func__, itf->bind_ch);
		return NULL;
	}
	pch = get_chdata(itf->bind_ch);

	if (!pch) {
		PR_ERR("%s:no pch:id[%d]\n", __func__, itf->bind_ch);
		return NULL;
	}
	pbufq = &pch->nin_qb;

	qt_in = QBF_NINS_Q_CHECK;

	cnt = nins_cnt(pch, qt_in);

	if (!cnt)
		return NULL;

	ret = qbuf_out(pbufq, qt_in, &bindex);
	if (!ret) {
		PR_ERR("%s:1:%d:can't get out\n", __func__, bindex);
		return NULL;
	}
	if (bindex >= DIM_NINS_NUB) {
		PR_ERR("%s:2:%d\n", __func__, bindex);
		return NULL;
	}
	q_buf = pbufq->pbuf[bindex];
	ins = (struct dim_nins_s *)q_buf.qbc;
	vf = (struct vframe_s *)ins->c.ori;
	if (!vf) {
		ins->c.vfm_cp.decontour_pre = NULL;
		memset(&ins->c, 0, sizeof(ins->c));
		qbuf_in(pbufq, QBF_NINS_Q_IDLE, bindex);
		PR_ERR("%s:3:%d\n", __func__, bindex);
		return NULL;
	}
	vf->di_flag = 0;
	vf->di_flag |= DI_FLAG_DI_GET;
	if (ins->c.vfm_cp.decontour_pre)
		vf->decontour_pre = ins->c.vfm_cp.decontour_pre;
	else
		vf->decontour_pre = NULL;
	ins->c.vfm_cp.decontour_pre = NULL;
	memset(&ins->c, 0, sizeof(ins->c));
	qbuf_in(pbufq, QBF_NINS_Q_IDLE, bindex);
	itf->c.sum_pre_get++;
	dim_print("%s:%px cnt:%d\n", __func__, vf, itf->c.sum_pre_get);
	return vf;
}

struct vframe_s *in_vf_get(struct dimn_itf_s *itf)
{
#ifdef CONFIG_AMLOGIC_MEDIA_VFM
	struct vframe_s *vf;

	itf->c.sum_pre_get++;
	vf = vf_get(itf->dvfm.name);
	if (vf) {
		vf->di_flag = 0;
		vf->di_flag |= DI_FLAG_DI_GET;
	}

	return vf;
#else
	return NULL;
#endif
}

struct vframe_s *in_vf_peek(struct dimn_itf_s *itf)
{
#ifdef CONFIG_AMLOGIC_MEDIA_VFM
	return vf_peek(itf->dvfm.name);
#else
	return NULL;
#endif
}

static void in_vf_put(struct dimn_itf_s *itf, struct vframe_s *vf)
{
#ifdef CONFIG_AMLOGIC_MEDIA_VFM
	itf->c.sum_pre_put++;
	if (vf)
		vf->di_flag &= ~DI_FLAG_DI_GET;
	vf_put(vf, itf->dvfm.name);
#endif
}

static void in_buffer_put(struct dimn_itf_s *itf, struct di_buffer *buffer)
{
	if (!itf || itf->etype != EDIM_NIN_TYPE_INS ||
		!buffer) {
		PR_ERR("%s:not ins\n", __func__);
		return;
	}

	if (itf->nitf_parm.ops.empty_input_done) {
		itf->c.sum_pre_put++;
		itf->nitf_parm.ops.empty_input_done(buffer);
//#if defined(DBG_QUE_IN_OUT) || defined(DBG_QUE_INTERFACE)
		dbg_plink2("ins:empty_done:0x%px,0x%px\n", buffer, buffer->vf);
//#endif
	}
}

//vf_notify_receiver
//vf_light_unreg_provider(prov);

/*--------------------------*/
#ifdef DIM_PLINK_ENABLE_CREATE
static const char * const vfm_event_cmd[] = {
	"",
	"_UNREG",
	"_LIGHT_UNREG",
	"_START",
	NULL,	/* "_VFRAME_READY", */
	NULL,	/* "_QUREY_STATE", */
	"_RESET",
	NULL,	/* "_FORCE_BLACKOUT", */
	"_REG",
	"_LIGHT_UNREG_RETURN_VFRAME",
	NULL,	/* "_DPBUF_CONFIG", */
	NULL,	/* "_QUREY_VDIN2NR", */
	NULL,	/* "_SET_3D_VFRAME_INTERLEAVE", */
	NULL,	/* "_FR_HINT", */
	NULL,	/* "_FR_END_HINT", */
	NULL,	/* "_QUREY_DISPLAY_INFO", */
	NULL,	/* "_PROPERTY_CHANGED", */
};

#define VFRAME_EVENT_PROVIDER_CMD_MAX	16

static int dimn_receiver_event_fun(int type, void *data, void *arg)
{
	struct dev_vfram_t *pvfm;
	int ret = 0;
	char *provider_name = (char *)data;
	struct dimn_itf_s *itf;

	if (!arg) {
		PR_ERR("%s:no arg\n", __func__);
		return ret;
	}
	itf = (struct dimn_itf_s *)arg;

	pvfm = &itf->dvfm;

	if (type <= VFRAME_EVENT_PROVIDER_CMD_MAX	&&
	    vfm_event_cmd[type]) {
		dbg_ev("%s,%s:%d:%s\n", __func__,
		       pvfm->name,
		       type,
		       vfm_event_cmd[type]);
	}

	switch (type) {
	case VFRAME_EVENT_PROVIDER_UNREG: /* from back to front */
		mutex_lock(&itf->lock_reg);
		if (!atomic_read(&itf->reg)) {
			mutex_unlock(&itf->lock_reg);
			PR_WARN("%s:duplicate ureg\n", __func__);
			break;
		}
		atomic_set(&itf->unregging_back, 1);
		if (itf->ops_vfm && itf->ops_vfm->unreg_trig)
			itf->ops_vfm->unreg_trig(itf);
		mutex_unlock(&itf->lock_reg);
		vf_unreg_provider(&itf->dvfm.di_vf_prov);

		mutex_lock(&itf->lock_reg);
		atomic_set(&itf->unregging_back, 0);
		atomic_set(&itf->reg, 0);
		if (itf->ops_vfm && itf->ops_vfm->unreg_api)
			itf->ops_vfm->unreg_api(itf);
		mutex_unlock(&itf->lock_reg);
		break;
	case VFRAME_EVENT_PROVIDER_REG: /* from front to back */
		mutex_lock(&itf->lock_reg);
		if (atomic_read(&itf->reg)) {
			PR_WARN("%s:duplicate reg\n", __func__);
			mutex_unlock(&itf->lock_reg);
			break;
		}
		if (itf->ops_vfm &&
		    itf->ops_vfm->reg_pre_check &&
		    !itf->ops_vfm->reg_pre_check(itf)) {
			mutex_unlock(&itf->lock_reg);
			break;
		}

		itf->sum_reg_cnt = dpvpp_create_instance_id(itf->bind_ch);
		itf->etype = EDIM_NIN_TYPE_VFM;

		dbg_ev("reg:%s[%d]\n", provider_name, itf->sum_reg_cnt);

		if (itf->ops_vfm && itf->ops_vfm->reg_api)
			itf->ops_vfm->reg_api(itf);

		atomic_set(&itf->reg, 1);/* */
		vf_reg_provider(&pvfm->di_vf_prov);
		vf_notify_receiver(pvfm->name,
				   VP_EVENT(START), NULL);

		mutex_unlock(&itf->lock_reg);

		break;
	case VFRAME_EVENT_PROVIDER_START:
		if (itf->ops_vfm && itf->ops_vfm->reg_start)
			itf->ops_vfm->reg_start(itf);
		break;

	case VFRAME_EVENT_PROVIDER_LIGHT_UNREG:
		//no used ret = di_ori_event_light_unreg(ch);
		break;
	case VFRAME_EVENT_PROVIDER_VFRAME_READY:
		if (itf->bypass_complete)
			vf_notify_receiver(pvfm->name,
				      VP_EVENT(VFRAME_READY),
				      NULL);
		if (itf->ops_vfm && itf->ops_vfm->et_ready)
			ret = itf->ops_vfm->et_ready(itf);
		break;
	case VFRAME_EVENT_PROVIDER_QUREY_STATE:
		ret = event_qurey_state(itf);
		break;
	case VFRAME_EVENT_PROVIDER_RESET:
		if (itf->ops_vfm && itf->ops_vfm->et_reset)
			ret = itf->ops_vfm->et_reset(itf);
		break;
	case VFRAME_EVENT_PROVIDER_LIGHT_UNREG_RETURN_VFRAME:
		//no used ret = di_ori_event_light_unreg_revframe(ch);
		break;
	case VFRAME_EVENT_PROVIDER_QUREY_VDIN2NR:
		//no used ret = di_ori_event_qurey_vdin2nr(ch);
		break;
	case VFRAME_EVENT_PROVIDER_SET_3D_VFRAME_INTERLEAVE:
		//no used di_ori_event_set_3D(type, data, ch);
		break;
	case VFRAME_EVENT_PROVIDER_FR_HINT:
	case VFRAME_EVENT_PROVIDER_FR_END_HINT:
		vf_notify_receiver(pvfm->name, type, data);
		break;

	default:
		break;
	}

	return ret;
}

static const struct vframe_receiver_op_s dimn_vf_receiver = {
	.event_cb	= dimn_receiver_event_fun
};
#endif /* DIM_PLINK_ENABLE_CREATE */

static struct vframe_s *dimn_vf_peek(void *arg)
{
	struct dimn_itf_s *itf;
	struct vframe_s *vfm = NULL;

	if (!arg) {
		PR_ERR("%s:no arg\n", __func__);
		return NULL;
	} else {
		itf = (struct dimn_itf_s *)arg;
	}
	if (!atomic_read(&itf->reg)) {
		PR_ERR("%s:unreg?\n", __func__);
		return NULL;
	}

	if (itf->c.flg_block || itf->c.pause_pst_get)
		return NULL;

	if (itf->bypass_complete) {
		vfm = in_vf_peek(itf);
		if (itf->ops_vfm &&
		    itf->ops_vfm->vf_bypass_check_chg &&
		    itf->ops_vfm->vf_bypass_check_chg(itf, vfm))
			return NULL;
		return vfm;
	}
	if (itf->ops_vfm && itf->ops_vfm->vf_peek)
		vfm = itf->ops_vfm->vf_peek(itf);
	return vfm;
}

/* dpvpp_vf_ops()->get */
static struct vframe_s *dimn_vf_get(void *arg)
{
	struct dimn_itf_s *itf;
	struct vframe_s *vfm = NULL;

	if (!arg) {
		PR_ERR("%s:no arg\n", __func__);
		return NULL;
	} else {
		itf = (struct dimn_itf_s *)arg;
	}
	if (!atomic_read(&itf->reg)) {
		PR_ERR("%s:unreg?\n", __func__);
		return NULL;
	}
	if (itf->c.flg_block || itf->c.pause_pst_get)
		return NULL;
	itf->c.sum_pst_get++;

	if (itf->bypass_complete) {
		vfm = in_vf_peek(itf);
		if (itf->ops_vfm &&
		    itf->ops_vfm->vf_bypass_check_chg &&
		    itf->ops_vfm->vf_bypass_check_chg(itf, vfm))
			return NULL;
		vfm = in_vf_get(itf);
		vfm->di_flag |= DI_FLAG_DI_BYPASS;
		return vfm;
	}
	if (itf->ops_vfm && itf->ops_vfm->vf_get)
		vfm = itf->ops_vfm->vf_get(itf);
	return vfm;
}

/* dpvpp_vf_ops()->put */
static void dimn_vf_put(struct vframe_s *vf, void *arg)
{
	struct dimn_itf_s	*itf;

	if (!arg) {
		PR_ERR("%s:no arg\n", __func__);
		return;
	} else {
		itf = (struct dimn_itf_s *)arg;
	}
	if (!atomic_read(&itf->reg)) {
		PR_ERR("%s:unreg?\n", __func__);
		return;
	}

	itf->c.sum_pst_put++;

	if (vf->di_flag & DI_FLAG_DI_BYPASS) {
		vf->di_flag &= (~DI_FLAG_DI_BYPASS);
		in_vf_put(itf, vf);
		vf_notify_provider(itf->dvfm.name,
				   VFRAME_EVENT_RECEIVER_PUT, NULL);
		return;
	}
	if (itf->ops_vfm && itf->ops_vfm->vf_put)
		itf->ops_vfm->vf_put(vf, itf);
}

static int dimn_event_cb(int type, void *data, void *private_data)
{
	if (type == VFRAME_EVENT_RECEIVER_FORCE_UNREG) {
		PR_INF("%s: _FORCE_UNREG return\n",
			__func__);
		return 0;
	}
	return 0;
}

/* dpvpp_vf_ops()->vf_states */
static int dimn_vf_states(struct vframe_states *states, void *arg)
{
	struct dimn_itf_s	*itf;

	if (!states)
		return -1;
	if (!arg) {
		PR_ERR("%s:no arg\n", __func__);
		return -1;
	} else {
		itf = (struct dimn_itf_s *)arg;
	}
	if (itf->ops_vfm && itf->ops_vfm->et_states)
		itf->ops_vfm->et_states(itf, states);

	return 0;
}

//dpvpp_vf_ops()
static const struct vframe_operations_s dimn_vf_provider = {
	.peek		= dimn_vf_peek,
	.get		= dimn_vf_get,
	.put		= dimn_vf_put,
	.event_cb	= dimn_event_cb,
	.vf_states	= dimn_vf_states,
};

void dvframe_exit(struct dimn_itf_s *itf)
{
	struct dev_vfram_t *pvfm;

	pvfm = &itf->dvfm;
	vf_unreg_provider(&pvfm->di_vf_prov);
	vf_unreg_receiver(&pvfm->di_vf_recv);

	PR_INF("%s finish:%s\n", __func__, pvfm->name);
}

#ifdef DIM_PLINK_ENABLE_CREATE
void dvframe_prob(struct dimn_itf_s *itf)
{
	struct dev_vfram_t *pvfm;

	/*set_bypass_complete(pvfm, true);*/ /*test only*/
	if (!itf) {
		PR_ERR("%s:no input\n", __func__);
		return;
	}
	pvfm = &itf->dvfm;

	/*receiver:*/
	vf_receiver_init(&pvfm->di_vf_recv,
			 pvfm->name,
			 &dimn_vf_receiver, itf);
	vf_reg_receiver(&pvfm->di_vf_recv);

	/*provider:*/
	vf_provider_init(&pvfm->di_vf_prov,
			 pvfm->name,
			 &dimn_vf_provider, itf);

	PR_INF("%s finish:%s\n", __func__, pvfm->name);
}
#endif /* DIM_PLINK_ENABLE_CREATE */

/************************************************
 * que for point
 ************************************************/
#ifdef DBG_QUE_LOCAL
static void qqs_err_add(struct qs_err_log_s *plog, struct qs_err_msg_s *msg)
{
	if (!plog || !msg ||
	    plog->pos >= QS_ERR_LOG_SIZE)
		return;
	plog->msg[plog->pos] = *msg;
	plog->pos++;
}
#endif
static bool ffp_reset(struct dimn_qs_cls_s *p)
{
	kfifo_reset(&p->fifo);
	return true;
}

static bool ffp_put(struct dimn_qs_cls_s *p, void *ubuf)
{
#ifdef DBG_QUE_LOCAL
	struct qs_err_msg_s msg;
#endif
	ud buf_add;
	unsigned int ret;
#ifdef DBG_QUE_IN_OUT
	struct vframe_s *vfm;
	struct dimn_dvfm_s *ndvfm;
	bool is_act = false;
#endif

	buf_add = (ud)ubuf;
	if (p->cfg.flg_lock & DIM_QUE_LOCK_WR) {
		ret = kfifo_in_spinlocked(&p->fifo,
					  &buf_add,
					  p->cfg.ele_size,
					  &p->lock_wr);
	} else {
		ret = kfifo_in(&p->fifo, &buf_add, p->cfg.ele_size);
	}

	if (ret	!= p->cfg.ele_size) {
#ifdef DBG_QUE_LOCAL
		msg.func_id	= QS_FUNC_F_IN;
		msg.err_id	= QS_ERR_FIFO_IN;
		msg.qname	= p->cfg.name;
		msg.index1	= buf_add;
		msg.index2	= 0;
		qqs_err_add(p->plog, &msg);
#endif
		PR_ERR("%s:%s:0x%px\n", __func__, p->cfg.name, ubuf);
		return false;
	}
#ifdef DBG_QUE_IN_OUT
	vfm = (struct vframe_s *)ubuf;
	if (vfm->private_data) {
		ndvfm = (struct dimn_dvfm_s *)vfm->private_data;
		if (ndvfm->header.code == CODE_INS_LBF)
			is_act = true;
	}
	if (is_act)
		dbg_plink3("%s:put:%d ndvfm:0x%px\n",
		       p->cfg.name, ndvfm->header.index, ubuf);
	else
		dbg_plink3("%s:put:0x%px?\n", p->cfg.name, ubuf);
#endif
	return true;
}

static void *ffp_get(struct dimn_qs_cls_s *p)
{
#ifdef DBG_QUE_LOCAL
	struct qs_err_msg_s msg;
#endif
#ifdef DBG_QUE_IN_OUT
		struct vframe_s *vfm;
		struct dimn_dvfm_s *ndvfm;
		bool is_act = false;
#endif
	ud buf_add;

	unsigned int ret;

	if (p->cfg.flg_lock & DIM_QUE_LOCK_RD) {
		ret = kfifo_out_spinlocked(&p->fifo,
					   &buf_add,
					   p->cfg.ele_size,
					   &p->lock_rd);
	} else {
		ret = kfifo_out(&p->fifo, &buf_add, p->cfg.ele_size);
	}

	if (ret != p->cfg.ele_size) {
#ifdef DBG_QUE_LOCAL
		msg.func_id	= QS_FUNC_F_O;
		msg.err_id	= QS_ERR_FIFO_O;
		msg.qname	= p->cfg.name;
		msg.index1	= 0;
		msg.index2	= 0;
		qqs_err_add(p->plog, &msg);
#endif
		PR_ERR("%s:%s\n",
			 __func__, p->cfg.name);
		return NULL;
	}
#ifdef DBG_QUE_IN_OUT
	vfm = (struct vframe_s *)buf_add;
	if (vfm->private_data) {
		ndvfm = (struct dimn_dvfm_s *)vfm->private_data;
		if (ndvfm->header.code == CODE_INS_LBF)
			is_act = true;
	}
	if (is_act)
		dbg_plink3("%s:get:%d ndvfm:0x%px\n",
		       p->cfg.name, ndvfm->header.index, buf_add);
	else
		dbg_plink3("%s:get:?:0x%px\n", p->cfg.name, buf_add);
#endif

	return (void *)buf_add;
}

static unsigned int ffp_in(struct dimn_qs_cls_s *p,
			   void *ubuf, unsigned int cnt)
{
#ifdef DBG_QUE_LOCAL
	struct qs_err_msg_s msg;
	ud buf_add;
#endif
#ifdef DBG_QUE_IN_OUT
	ud *addr;
	ud buf_add;
#endif
	unsigned int ret, rcnt;

	//buf_add = (ud)ubuf;
	if (p->cfg.flg_lock & DIM_QUE_LOCK_WR) {
		ret = kfifo_in_spinlocked(&p->fifo,
					  ubuf,
					  cnt << p->cfg.ele_mod,
					  &p->lock_wr);
	} else {
		ret = kfifo_in(&p->fifo, ubuf, cnt << p->cfg.ele_mod);
	}
	rcnt = ret >> p->cfg.ele_mod;

	if (rcnt != cnt) {
#ifdef DBG_QUE_LOCAL
		msg.func_id	= QS_FUNC_F_IN;
		msg.err_id	= QS_ERR_FIFO_IN;
		msg.qname	= p->cfg.name;
		msg.index1	= buf_add;
		msg.index2	= 0;
		qqs_err_add(p->plog, &msg);
#endif
		PR_ERR("%s:%s:0x%px\n", __func__, p->cfg.name, ubuf);
		return rcnt;
	}
#ifdef DBG_QUE_IN_OUT
	addr = (ud *)ubuf;

	buf_add = *(addr + 0);

	dbg_plink3("%s:in:%d:0x%px,cnt[%d]\n", p->cfg.name, rcnt, buf_add);
#endif
	return rcnt;
}

static unsigned int ffp_out(struct dimn_qs_cls_s *p, void *buf, unsigned int cnt)
{
#ifdef DBG_QUE_LOCAL
	struct qs_err_msg_s msg;
#endif
#ifdef DBG_QUE_IN_OUT
		ud buf_add, *pud;
#endif
	unsigned int ret, rcnt;

	if (p->cfg.flg_lock & DIM_QUE_LOCK_RD) {
		ret = kfifo_out_spinlocked(&p->fifo,
					   buf,
					   cnt << p->cfg.ele_mod,
					   &p->lock_rd);
	} else {
		ret = kfifo_out(&p->fifo, buf, cnt << p->cfg.ele_mod);
	}
	rcnt = ret >> p->cfg.ele_mod;

	if (rcnt != cnt) {
#ifdef DBG_QUE_LOCAL
		msg.func_id	= QS_FUNC_F_O;
		msg.err_id	= QS_ERR_FIFO_O;
		msg.qname	= p->cfg.name;
		msg.index1	= 0;
		msg.index2	= 0;
		qqs_err_add(p->plog, &msg);
#endif
		PR_ERR("%s:%s\n",
			 __func__, p->cfg.name);
		return rcnt;
	}
#ifdef DBG_QUE_IN_OUT
	pud = (ud *)buf;

	buf_add = *(pud + 0);

	dbg_plink3("%s:out:%d:0x%px:cnt[%d]\n", p->cfg.name, rcnt, buf_add, rcnt);
#endif

	return rcnt;
}

static void *ffp_peek(struct dimn_qs_cls_s *p)
{
	ud index;
#ifdef DBG_QUE_LOCAL
	struct qs_err_msg_s msg;
#endif
	if (kfifo_is_empty(&p->fifo))
		return NULL;

	if (kfifo_out_peek(&p->fifo, &index, p->cfg.ele_size) !=
	    p->cfg.ele_size) {
#ifdef DBG_QUE_LOCAL
		msg.func_id	= QS_FUNC_F_PEEK;
		msg.err_id	= QS_ERR_FIFO_PEEK;
		msg.qname	= p->cfg.name;
		msg.index1	= 0;
		msg.index2	= 0;
		qqs_err_add(p->plog, &msg);
#endif
		PR_ERR("%s:\n", __func__);
		return NULL;
	}

	return (void *)index;
}

static bool ffp_is_full(struct dimn_qs_cls_s *p)
{
	return kfifo_is_full(&p->fifo);
}

static unsigned int ffp_count(struct dimn_qs_cls_s *p)
{
	unsigned int length;

	length = kfifo_len(&p->fifo);
	length = length >> p->cfg.ele_mod;
	return length;
}

static bool ffp_empty(struct dimn_qs_cls_s *p)
{
	//return kfifo_is_empty(&p->fifo);
	if (!ffp_count(p))
		return true;
	return false;
}

static unsigned int ffp_avail(struct dimn_qs_cls_s *p)
{
	unsigned int length;

	length = kfifo_avail(&p->fifo);
	length = length >> p->cfg.ele_mod;
	return length;
}

static unsigned int ffp_list(struct dimn_qs_cls_s *p, ud *rtab)
{
	unsigned int cnt = 0;

	cnt = kfifo_out_peek(&p->fifo, rtab, p->size);
	cnt = cnt >> p->cfg.ele_mod;
	return cnt;
}

static const struct dimn_qsp_ops_s dimn_pque_ops = {
	.put	= ffp_put,
	.get	= ffp_get,
	.in	= ffp_in,
	.out	= ffp_out,
	.peek	= ffp_peek,
	.is_empty = ffp_empty,
	.is_full	= ffp_is_full,
	.count	= ffp_count,
	.avail	= ffp_avail,
	.reset	= ffp_reset,
	.list	= ffp_list,
};

/* copy from qfp_int */
void dimn_que_int(struct dimn_qs_cls_s	*pq,
		  struct dimn_qs_cfg_s *cfg) /*  MAX_FIFO_SIZE_PVPP */
{
	int ret;

	if (!pq)
		return;

	/*creat que*/
	memset(pq, 0, sizeof(*pq));
	memcpy(&pq->cfg, cfg, sizeof(pq->cfg));

	pq->flg	= true;

	ret = kfifo_alloc(&pq->fifo,
			  cfg->ele_nub << cfg->ele_mod,
			  GFP_KERNEL);
	if (ret < 0) {
		PR_ERR("%s:alloc kfifo err:%s\n", __func__, cfg->name);
		pq->flg = false;
		return;//no resource
	}

	pq->size = kfifo_size(&pq->fifo);

	memcpy(&pq->ops, &dimn_pque_ops, sizeof(pq->ops));

	/*reset*/
	pq->ops.reset(pq);
	/*lock ?*/
	if (cfg->flg_lock & DIM_QUE_LOCK_RD)
		spin_lock_init(&pq->lock_rd);

	if (cfg->flg_lock & DIM_QUE_LOCK_WR)
		spin_lock_init(&pq->lock_wr);

	dbg_reg("%s:%s:end\n", __func__, cfg->name);
}

bool dimn_que_release(struct dimn_qs_cls_s	*pq)
{
	if (!pq)
		return true;
	if (pq->flg)
		kfifo_free(&pq->fifo);

	memset(pq, 0, sizeof(*pq));

	return true;
}

/************************************************
 * hw timer for wake up
 ************************************************/
#define DIM_HW_TIMER_MS		(12)
static void task_send_ready_now(unsigned int reason)
{
	struct dim_dvs_prevpp_s *dv_prevpp;

	dv_prevpp = &get_datal()->dvs_prevpp;

	atomic_set(&dv_prevpp->hr_timer_have, 0);
	atomic_set(&dv_prevpp->wk_need, 0);
	dv_prevpp->ktimer_lst_wk = ktime_get();
	atomic_inc(&dv_prevpp->sum_wk_real_cnt);
	task_send_ready(reason);
}

static enum hrtimer_restart dpvpp_wk_hrtimer_func(struct hrtimer *timer)
{
	task_send_ready_now(35);

	return HRTIMER_NORESTART;
}

static void task_send_wk_timer(unsigned int reason)
{
	struct dim_dvs_prevpp_s *dv_prevpp;
	ktime_t ktimer_now;
	s64 kdiff;
	unsigned int mstimer;

	dv_prevpp = &get_datal()->dvs_prevpp;
	atomic_inc(&dv_prevpp->sum_wk_rq);

	if (reason & DIM_WKUP_TAG_CRITICAL) {
		hrtimer_cancel(&dv_prevpp->hrtimer_wk);
		task_send_ready_now(reason);
		return;
	}

	atomic_inc(&dv_prevpp->wk_need);
	if (atomic_read(&dv_prevpp->hr_timer_have))
		return;

	ktimer_now = ktime_get();
	kdiff = ktime_ms_delta(ktimer_now, dv_prevpp->ktimer_lst_wk);
	if (kdiff > (DIM_HW_TIMER_MS - 3)) {
		hrtimer_cancel(&dv_prevpp->hrtimer_wk);
		task_send_ready_now(reason);
		return;
	}

	/* no timer */
	if (kdiff <= 0)
		mstimer = DIM_HW_TIMER_MS;
	else
		mstimer = DIM_HW_TIMER_MS - (unsigned int)kdiff;
	atomic_set(&dv_prevpp->hr_timer_have, 1);
	hrtimer_start(&dv_prevpp->hrtimer_wk,
		      ms_to_ktime(mstimer), HRTIMER_MODE_REL);
}

/* must from 0 and < 31*/
enum EDVPP_SRC_NEED {
	EDVPP_SRC_NEED_VAL	= DI_BIT0,
	EDVPP_SRC_NEED_KFIFO	= DI_BIT1,
	EDVPP_SRC_NEED_MSK_CRT	= 0xff, /* critical */

	EDVPP_SRC_NEED_REG2	= DI_BIT8,
	EDVPP_SRC_NEED_MEM	= DI_BIT9,
	EDVPP_SRC_NEED_SCT_TAB	= DI_BIT10,
	EDVPP_SRC_NEED_SCT_TOP	= DI_BIT11,

	EDVPP_SRC_NEED_TEST	= DI_BIT15,

	EDVPP_SRC_NEED_MSK_BYPASS = 0xff00,
	EDVPP_SRC_NEED_LINK_ON	= DI_BIT16,/* no use ?*/
};

/************************************************
 * pre-vpp link
 ************************************************/
#ifdef DIM_PLINK_ENABLE_CREATE
static bool dpvpp_t_reg_api(struct dimn_itf_s *itf)
{
	int timeout = 0;
#ifdef DBG_TIMER
	u64 ustime, udiff;

	ustime = cur_to_usecs();
	//ustime	= cur_to_msecs();
#endif
	atomic_set(&itf->regging_api, 1);

	task_send_cmd(LCMD1(ECMD_PV_LINK_REG, DIM_PRE_VPP_NUB));
	timeout = wait_for_completion_timeout(&itf->reg_done,
		msecs_to_jiffies(1000));
	if (!timeout) {
		atomic_set(&itf->regging_api, 0);
		init_completion(&itf->reg_done);
		PR_WARN("%s:ch[%d]timeout\n", __func__, itf->ch);
		return false;
	}

	atomic_set(&itf->regging_api, 0);

	//dbg only:
#ifdef DBG_TIMER
	udiff	= cur_to_usecs();
	udiff -= ustime;
	dbg_mem2("%s:us %u us\n", __func__, (unsigned int)udiff);
#endif
	return true;
}

static bool dpvpp_t_unreg_api(struct dimn_itf_s *itf)
{
	int timeout = 0;
#ifdef DBG_TIMER
	u64 ustime, udiff;

	ustime = cur_to_usecs();
	//ustime	= cur_to_msecs();
#endif
	//dump_stack();
	atomic_set(&itf->unregging_api, 1);

	task_send_cmd(LCMD1(ECMD_PV_LINK_UNREG, DIM_PRE_VPP_NUB));
	timeout = wait_for_completion_timeout(&itf->reg_done,
		msecs_to_jiffies(2000));
	if (!timeout) {
		atomic_set(&itf->regging_api, 0);
		init_completion(&itf->reg_done);
		PR_WARN("%s:ch[%d]timeout\n", __func__, itf->ch);
		return false;
	}
	atomic_set(&itf->unregging_api, 0);
//dbg only:
#ifdef DBG_TIMER
	udiff	= cur_to_usecs();
	udiff -= ustime;
	dbg_mem2("%s:us %u us\n", __func__, (unsigned int)udiff);
#endif
	return true;
}
#endif /* DIM_PLINK_ENABLE_CREATE */

/************************************************
 * in put
 ************************************************/

static void dpvpp_dbg_unreg_log_print(void)
{
#ifdef DBG_UNREG_LOG
	struct dimn_itf_s *itf;
	struct dim_dvs_prevpp_s *dv_prevpp;
	char *spltb = "---------------------------";

	dv_prevpp = &get_datal()->dvs_prevpp;
	itf = &dv_prevpp->itf;
	/* no input */
	if (!itf->c.sum_pre_get)
		return;

	PR_INF("%s\n", spltb); /*-----*/
	PR_INF("%s:\n", "timer");

	PR_INF("%-15s:%d\n", "sum_wk_rq", atomic_read(&dv_prevpp->sum_wk_rq));
	PR_INF("%-15s:%d\n", "sum_wk_real",
	       atomic_read(&dv_prevpp->sum_wk_real_cnt));

	PR_INF("%s\n", spltb); /*-----*/
	PR_INF("%s:%d\n", "etype:1:vfm", itf->etype);
	PR_INF("%s:0x%x:0x%x:0x%x\n", "srcx",
		   itf->src_need,
		   itf->c.src_state,
		   itf->c.src_last);
	PR_INF("%s:%d\n", "bypass_c", itf->bypass_complete);
	PR_INF("%s:%d:%d:%d:%d\n", "pause",
		   itf->c.pause_pst_get,
		   itf->c.pause_pre_get,
		   itf->c.pause_parser,
		   itf->c.flg_block);
	PR_INF("%s:\n", "sum");
	PR_INF("\t%s:%d\n", "pre_get", itf->c.sum_pre_get);
	PR_INF("\t%s:%d\n", "pre_put", itf->c.sum_pre_put);
	PR_INF("\t%s:%d\n", "pst_get", itf->c.sum_pst_get);
	PR_INF("\t%s:%d\n", "pst_put", itf->c.sum_pst_put);
	PR_INF("\t%s:%d\n", "reg_cnt", itf->sum_reg_cnt);
	PR_INF("\t%s:0x%x\n", "bypass flg", get_datal()->pre_vpp_active);
	PR_INF("%s\n", spltb); /*-----*/
#endif
}

//static void dpvpp_mem_release(struct dim_prevpp_ds_s *ds);
//static void dpvpp_sec_release(struct di_dat_s *dat);
//static bool dpvpp_dbg_print;
static bool dpvpp_unreg_val(struct dimn_itf_s *itf)
{
	struct dim_prevpp_ds_s *ds = NULL;
	struct dim_pvpp_hw_s *hw;
	bool flg_disable = false;
	ulong irq_flag = 0;
	int i;
	struct di_ch_s *pch;

	if (!itf) {
		PR_ERR("%s:no itf\n", __func__);
		return false;
	}
#ifdef DIM_PLINK_ENABLE_CREATE
	if (!atomic_read(&itf->unregging_api))
		return false;
#endif
	spin_lock_irqsave(&lock_pvpp, irq_flag);
	hw = &get_datal()->dvs_prevpp.hw;
	itf->flg_freeze = true;
	if (hw && hw->id_c == itf->id &&
	    hw->reg_cnt == itf->sum_reg_cnt)
		flg_disable = true;
	spin_unlock_irqrestore(&lock_pvpp, irq_flag);
	if (hw && !flg_disable && hw->reg_nub <= 1 &&
	    (hw->src_blk[0] || hw->src_blk_uhd_afbce[0])) {
		flg_disable = true;
		PR_INF("%s:lst ch?\n", __func__);
	}
	if (flg_disable && atomic_read(&hw->link_on_bydi)) {
		dpvpp_link_sw_by_di(false);
		dpvpp_reg_link_sw(false); //block;
	}
	dpvpp_dbg_unreg_log_print();

	pch = get_chdata(itf->bind_ch);
	dim_polic_unreg(pch);

	//not need set this flg 2022-01-13 set_hw_reg_flg(true);
	//use flg replace dpvpph_unreg_setting();
	itf->en_first_buf = false;
	/* mem */
	if (!itf->ds) {
		PR_WARN("%s:no ds\n", __func__);
		return false;
	}
	ds = itf->ds;
	//dpvpp_mem_release(ds);
	if (itf->c.flg_reg_mem) {
		hw->reg_nub--;
		itf->c.flg_reg_mem = false;
	}
	//dpvpp_sec_release(&hw->dat_p_afbct);
	/**/
	/* release que */
	dimn_que_release(&ds->lk_que_idle);
	dimn_que_release(&ds->lk_que_in);
	dimn_que_release(&ds->lk_que_ready);
	dimn_que_release(&ds->lk_que_kback);
	dimn_que_release(&ds->lk_que_recycle);
	vfree(ds);
	itf->ds = NULL;

	spin_lock_irqsave(&lock_pvpp, irq_flag);
	itf->sum_reg_cnt = 1; //
	itf->flg_freeze = false;
	itf->c.m_mode_n = EPVPP_MEM_T_NONE;
	memset(&itf->vf_bf[0], 0,
		sizeof(itf->vf_bf));
	memset(&itf->buf_bf[0], 0,
		sizeof(itf->buf_bf));
	spin_unlock_irqrestore(&lock_pvpp, irq_flag);
	for (i = 0; i < DIM_LKIN_NUB; i++) {
		if (itf->buf_bf[i].vf) {
			PR_INF("check:buf:%d:0x%px,0x%px\n", i,
				&itf->buf_bf[i],
				&itf->buf_bf[i].vf);
		}
	}
	for (i = 0; i < DIM_LKIN_NUB; i++) {
		if (itf->vf_bf[i].private_data) {
			PR_INF("check:vf:%d:0x%px,0x%px\n", i,
				&itf->vf_bf[i],
				itf->vf_bf[i].private_data);
		}
	}

	get_datal()->dvs_prevpp.en_polling = false;
	get_datal()->pre_vpp_exist = false;
#ifdef DIM_PLINK_ENABLE_CREATE
	complete(&itf->reg_done);
#endif
	dpvpp_mem_mng_get(2);
	dbg_plink2("%s:check:end\n", __func__);
	return true;
}

//static
unsigned char power_of_2(unsigned int a)
{
	unsigned int position = 0, i = 0;

	/* check power error */
	if (!a || (a & (a - 1)) != 0) {
		PR_ERR("not power:0x%x\n", a);
		return 0;
	}

	for (i = a; i != 0; i >>= 1)
		position++;
	position--;

	return position;
}

/* dvpp_m_ops -> reg | dpvpp_ops()->reg */
static bool dpvpp_reg(struct dimn_itf_s *itf)
{
	struct dim_prevpp_ds_s *ds = NULL;
	int i;
	struct dimn_qs_cls_s *qidle;
	struct dimn_dvfm_s *ndvfm;
	struct vframe_s *vf;
	struct dim_dvs_prevpp_s *dv_prevpp;
	struct dimn_qs_cfg_s qcfg;
	struct di_buffer *di_buffer;
	unsigned int id;
	struct dim_pvpp_hw_s *hw;
	struct div2_mm_s *mm;

	dv_prevpp = &get_datal()->dvs_prevpp;

	if (!itf)
		return false;
#ifdef DIM_PLINK_ENABLE_CREATE
	if (!atomic_read(&itf->regging_api))
		return false;
#endif /* DIM_PLINK_ENABLE_CREATE */
	//kfifo_test_print();
	/* clear htimer */
	dbg_plink2("%s:id[%d]b\n", __func__, itf->id);
	dv_prevpp->ktimer_lst_wk = ktime_get();
	atomic_set(&dv_prevpp->sum_wk_real_cnt, 0);
	atomic_set(&dv_prevpp->sum_wk_rq, 0);
	dbg_mem("dim_dvpp_set_s:size:0x%zx, etype[%d]\n",
		sizeof(struct dim_dvpp_set_s),
		itf->etype);
	id = itf->id;
	/* clear vf/di_buff*/
	memset(&itf->vf_bf[0], 0,
		sizeof(itf->vf_bf));
	memset(&itf->buf_bf[0], 0,
		sizeof(itf->buf_bf));
	/* clear c */
	memset(&itf->c, 0, sizeof(itf->c));
	itf->c.reg_di = true;
	itf->c.src_state = itf->src_need;
	itf->c.last_ds_ratio = DI_FLAG_DCT_DS_RATIO_MAX;
	/* dbg */
	di_g_plink_dbg()->display_sts = -10;
	di_g_plink_dbg()->flg_check_di_act = -10;
	di_g_plink_dbg()->flg_check_vf = -10;

	//PR_INF("%s:src_state set[0x%x]\n", __func__, itf->c.src_state);
	/* alloc */
	if (itf->ds) {
		PR_ERR("%s:have ds?\n", __func__);
		/* @ary 11-08 need check release only or unreg? */
		dpvpp_unreg_val(itf);
	}
	if (!itf->ds) {
		/* alloc*/
		ds = vmalloc(sizeof(*ds));
		if (!ds) {
			PR_ERR("%s:can't alloc\n", __func__);
			return false;
		}
		itf->c.src_state &= ~EDVPP_SRC_NEED_VAL;
		//PR_INF("%s:src_state[0x%x]\n", __func__, itf->c.src_state);
		itf->ds = ds;
	}
	ds = itf->ds;
	if (!ds) {
		PR_WARN("%s:no ds\n", __func__);
		return false;
	}
	memset(ds, 0, sizeof(*ds));
	ds->id = id;	/* @ary 11-08 add */
	ds->itf = itf;	/* @ary 11-08 add */
	/* que int cfg */
	qcfg.ele_size	= tst_quep_ele;
	qcfg.ele_nub	= MAX_FIFO_SIZE_PVPP;
	qcfg.ele_mod	= power_of_2(qcfg.ele_size);
	qcfg.flg_lock	= 0;
	qcfg.name	= "lk_idle";
	dimn_que_int(&ds->lk_que_idle, &qcfg);
	qcfg.name	= "lk_recycle";
	dimn_que_int(&ds->lk_que_recycle, &qcfg);
	qcfg.name = "lk_in";
	dimn_que_int(&ds->lk_que_in,  &qcfg);
	qcfg.name = "lk_ready";
	qcfg.flg_lock = DIM_QUE_LOCK_RD;
	dimn_que_int(&ds->lk_que_ready, &qcfg);
	qcfg.name = "lk_back";
	qcfg.flg_lock = DIM_QUE_LOCK_WR;
	dimn_que_int(&ds->lk_que_kback, &qcfg);
	/* check define */
#ifdef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
	dbg_plink1("define check:%s:yes\n", "VSYNC_WR_MPEG_REG");
#else
	PR_ERR("define check:%s:no\n", "VSYNC_WR_MPEG_REG");
#endif
	/* check que if ready */
	if (!ds->lk_que_idle.flg	||
	    !ds->lk_que_in.flg	||
	    !ds->lk_que_ready.flg	||
	    !ds->lk_que_kback.flg)
		goto ERR_DPVPP_REG;

	itf->c.src_state &= ~EDVPP_SRC_NEED_KFIFO;
	//PR_INF("%s:src_state[0x%x]\n", __func__, itf->c.src_state);

	/* data */
	for (i = 0; i < DIM_LKIN_NUB; i++) {
		ndvfm = &ds->lk_in_bf[i];
		ndvfm->header.ch = DIM_PRE_VPP_NUB;
		ndvfm->header.code = CODE_INS_LBF;
		ndvfm->etype = itf->etype;
		ndvfm->header.index = i;
		ndvfm->itf = itf;
		vf = dpvpp_get_vf_base(itf) + i;
		vf->private_data = ndvfm;
		vf->index = i;
		ndvfm->pvf = vf;
	}
	/*itf*/
	if (itf->etype == EDIM_NIN_TYPE_INS) {
		dbg_plink1("%s:0x%px\n", "caller",
			   itf->nitf_parm.caller_data);
		for (i = 0; i < DIM_LKIN_NUB; i++) {
			vf = dpvpp_get_vf_base(itf) + i;
			di_buffer = &itf->buf_bf[i];
			di_buffer->mng.ch = DIM_PRE_VPP_NUB;
			di_buffer->mng.index = i;
			di_buffer->mng.code = CODE_INS_LBF;
			di_buffer->caller_data =
				itf->nitf_parm.caller_data;
			di_buffer->vf = vf;
		}
	}

	for (i = 0; i < DIM_LKIN_NUB; i++) {
		qidle = &ds->lk_que_idle;
		qidle->ops.put(qidle, dpvpp_get_vf_base(itf) + i);
	}
	di_attach_opr_nr(&get_datal()->ops_nr_op);
	itf->bypass_complete = false;
	itf->en_first_buf = true;

	/* cfg top copy */
	di_cfg_cp_ch(&ds->cfg_cp[0]);
	/* cfg temp */
	ds->cma_mode = 4;
	if (cfgg(4K))
		ds->en_4k = true;
	else
		ds->en_4k = false;
	ds->out_mode		= EDPST_MODE_422_10BIT_PACK;
/* */
	if (cfgg(POUT_FMT) == 0) {
		ds->o_hd.b.mode = EDPST_MODE_422_10BIT_PACK;
		ds->o_hd.b.out_buf = EPVPP_BUF_CFG_T_HD_F;
		ds->o_hd.b.afbce_fmt = 0;
		ds->o_hd.b.is_sct_mem = 0;
		ds->o_hd.b.is_afbce = 0;
		if (ds->en_4k) {
			ds->o_uhd.d32 = ds->o_hd.d32;
			ds->o_uhd.b.out_buf = EPVPP_BUF_CFG_T_UHD_F;
		}
		ds->en_out_afbce = false;
		ds->m_mode_n_hd = EPVPP_MEM_T_HD_FULL;
		ds->m_mode_n_uhd = EPVPP_MEM_T_UHD_AFBCE;
		dbg_plink1("format 0\n");
#ifdef NOT_SUPPORT_SETTING
	} else if (cfgg(POUT_FMT) == 1 || cfgg(POUT_FMT) == 2) {
		ds->o_hd.b.mode = EDPST_MODE_NV21_8BIT;
		ds->o_hd.b.out_buf = EPVPP_BUF_CFG_T_HD_NV21;
		ds->o_hd.b.afbce_fmt = 0;
		ds->o_hd.b.is_sct_mem = 0;
		ds->o_hd.b.is_afbce = 0;
		if (ds->en_4k) {
			ds->o_uhd.d32 = ds->o_hd.d32;
			ds->o_uhd.b.out_buf = EPVPP_BUF_CFG_T_UHD_NV21;
		}
		ds->en_out_afbce = false;
		ds->m_mode_n_hd = EPVPP_MEM_T_HD_FULL;
		ds->m_mode_n_uhd = EPVPP_MEM_T_UHD_AFBCE;
		PR_WARN("not support\n");
#endif
	} else if (cfgg(POUT_FMT) == 3) {
		ds->o_hd.b.mode = EDPST_MODE_422_10BIT_PACK;
		ds->o_hd.b.out_buf = EPVPP_BUF_CFG_T_UHD_F_AFBCE;
		ds->o_hd.b.afbce_fmt = EAFBC_T_HD_FULL;
		ds->o_hd.b.is_afbce = 1;
		ds->o_hd.b.is_sct_mem = 0;
		if (ds->en_4k) {
			ds->o_uhd.d32 = ds->o_hd.d32;
			ds->o_uhd.b.out_buf = EPVPP_BUF_CFG_T_UHD_F_AFBCE;
		}

		ds->en_out_afbce	= true;
		ds->m_mode_n_hd = EPVPP_MEM_T_UHD_AFBCE;
		ds->m_mode_n_uhd = EPVPP_MEM_T_UHD_AFBCE;
		dbg_plink1("format 3:afbce\n");
	} else {
		//fix: hd used mif, uhd use afbce
		ds->o_hd.b.mode = EDPST_MODE_422_10BIT_PACK;
		ds->o_hd.b.out_buf = EPVPP_BUF_CFG_T_HD_F;
		ds->o_hd.b.afbce_fmt = 0;
		ds->o_hd.b.is_sct_mem = 0;
		ds->o_hd.b.is_afbce = 0;
		if (ds->en_4k) {
			ds->o_uhd.d32 = ds->o_hd.d32;
			ds->o_uhd.b.out_buf = EPVPP_BUF_CFG_T_UHD_F_AFBCE;
			ds->o_uhd.b.is_afbce = 1;
			ds->o_uhd.b.afbce_fmt = EAFBC_T_4K_FULL;
			ds->en_out_afbce = true;
		}
		ds->m_mode_n_hd = EPVPP_MEM_T_HD_FULL;
		ds->m_mode_n_uhd = EPVPP_MEM_T_UHD_AFBCE;
		dbg_plink1("fix uhd is afbce\n");
	}

	ds->out_afbce_nv2110	= false;
	ds->en_out_sctmem	= false;
	ds->en_hf_buf		= false;
	ds->en_dw		= false;
	ds->en_dbg_off_nr = dim_dbg_is_disable_nr();

	hw = &get_datal()->dvs_prevpp.hw;

	ds->op			= hw->op;
	//PR_INF("%s:ds->op:0x%px,0x%px\n", __func__, ds->op, ds->op->bwr);
	ds->mif_in.name	= "in";
	ds->mif_mem.name	= "mem";
	ds->mif_wr.name		= "wr";

	mm	= &ds->mm;
	if (ds->en_4k) {
		mm->cfg.di_h		= 2160;
		mm->cfg.di_w		= 3840;
	} else {
		mm->cfg.di_h		= 1088;
		mm->cfg.di_w		= 1920;
	}
	mm->cfg.num_local	= 0;
	mm->cfg.num_post	= DPVPP_BUF_NUB;

#ifdef DIM_PLINK_ENABLE_CREATE
	complete(&itf->reg_done);
#endif
	dct_pre_plink_reg(NULL);
	dcntr_reg(1);

	dbg_plink1("%s:id[%d]\n", __func__, itf->id);
	return true;

ERR_DPVPP_REG:
	itf->c.src_last = itf->c.src_state;
	if (ds) {
		dimn_que_release(&ds->lk_que_idle);
		dimn_que_release(&ds->lk_que_in);
		dimn_que_release(&ds->lk_que_ready);
		dimn_que_release(&ds->lk_que_kback);
		dimn_que_release(&ds->lk_que_recycle);
		vfree(itf->ds);
		itf->ds = NULL;
		itf->bypass_complete = true;
		itf->c.src_state = itf->src_need;
		PR_ERR("%s:src_state[0x%x]\n", __func__, itf->c.src_state);
	}

	return false;
}

/* from dip_itf_is_ins_exbuf */
//static //not for pre-vpp link, no ext buffer mode
bool dpvpp_itf_is_ins_exbuf(struct dimn_itf_s *itf)
{
	bool ret = false;

	if (itf->etype == EDIM_NIN_TYPE_INS &&
	    itf->tmode == EDIM_TMODE_2_PW_OUT)
		ret = true;
	return ret;
}

/* copy from di_cnt_pst_afbct */
//static
void dpvpp_cnt_pst_afbct(struct dimn_itf_s *itf)
{
	/* cnt the largest size to avoid rebuild */
	struct dim_prevpp_ds_s *ds = NULL;
//	struct dimn_itf_s *itf;

	unsigned int height, width;
	bool is_4k = false;
//	const struct di_mm_cfg_s *pcfg;
	unsigned int afbc_buffer_size = 0, afbc_tab_size = 0;
	unsigned int afbc_info_size = 0, blk_total = 0, tsize;
	bool flg_afbc = false;
	struct div2_mm_s *mm;

	if (!itf || !itf->ds)
		return;
	ds = itf->ds;
	mm = &ds->mm;

	if (dim_afds()			&&
	    dim_afds()->cnt_info_size	&&
	    ds->en_out_afbce)
		flg_afbc = true;

	if (!flg_afbc) {
		mm->cfg.size_pafbct_all	= 0;
		mm->cfg.size_pafbct_one	= 0;
		mm->cfg.nub_pafbct	= 0;
		mm->cfg.dat_pafbct_flg.d32 = 0;
		return;
	}

	/* if ic is not support*/
	is_4k = ds->en_4k;

	width	= mm->cfg.di_w;
	height	= mm->cfg.di_h;

	afbc_info_size = dim_afds()->cnt_info_size(width,
						   height,
						   &blk_total);
	afbc_buffer_size =
		dim_afds()->cnt_buf_size(0x21, blk_total);
	afbc_buffer_size = roundup(afbc_buffer_size, PAGE_SIZE);
	tsize = afbc_buffer_size + afbc_info_size;
	afbc_tab_size =
		dim_afds()->cnt_tab_size(tsize);

	mm->cfg.nub_pafbct	= mm->cfg.num_post;
	mm->cfg.size_pafbct_all = afbc_tab_size * mm->cfg.nub_pafbct;
	mm->cfg.size_pafbct_one = afbc_tab_size;

	mm->cfg.dat_pafbct_flg.d32	= 0;
	//mm->cfg.dat_pafbct_flg.b.afbc	= 1;
	mm->cfg.dat_pafbct_flg.b.typ	= EDIM_BLK_TYP_PAFBCT;
	mm->cfg.dat_pafbct_flg.b.page	= mm->cfg.size_pafbct_all >> PAGE_SHIFT;
	mm->cfg.dat_pafbci_flg.b.tvp	= 0;
	PR_INF("%s:size_pafbct_all:0x%x, 0x%x, nub[%d]:<0x%x,0x%x>\n",
		 __func__,
		 mm->cfg.size_pafbct_all,
		 mm->cfg.size_pafbct_one,
		 mm->cfg.nub_pafbct,
		 afbc_info_size,
		 afbc_buffer_size);
}

/* use dpvpp_set_pst_afbct to replace dpvpp_cnt_pst_afbct*/
static void dpvpp_set_pst_afbct(struct dimn_itf_s *itf)
{
	struct dim_prevpp_ds_s *ds = NULL;

	//unsigned int height;
	bool is_4k = false;
	struct c_cfg_afbc_s *inf_afbc;
	//unsigned int afbc_info_size = 0;
	bool flg_afbc = false;
	struct div2_mm_s *mm;

	if (!itf || !itf->ds)
		return;
	ds = itf->ds;
	mm = &ds->mm;

	if (dim_afds()			&&
	    dim_afds()->cnt_info_size	&&
	    ds->en_out_afbce)
		flg_afbc = true;

	if (!flg_afbc) {
		mm->cfg.size_pafbct_all	= 0;
		mm->cfg.size_pafbct_one	= 0;
		mm->cfg.nub_pafbct	= 0;
		mm->cfg.dat_pafbct_flg.d32 = 0;
		return;
	}

	/* if ic is not support*/
	is_4k = ds->en_4k;

	if (is_4k)
		inf_afbc = m_rd_afbc(EAFBC_T_4K_FULL);
	else
		inf_afbc = m_rd_afbc(EAFBC_T_HD_FULL);
	mm->cfg.nub_pafbct	= mm->cfg.num_post;
	mm->cfg.size_pafbct_all = inf_afbc->size_table * mm->cfg.nub_pafbct;
	mm->cfg.size_pafbct_one = inf_afbc->size_table;

	mm->cfg.dat_pafbct_flg.d32	= 0;
	//mm->cfg.dat_pafbct_flg.b.afbc	= 1;
	mm->cfg.dat_pafbct_flg.b.typ	= EDIM_BLK_TYP_PAFBCT;
	mm->cfg.dat_pafbct_flg.b.page	= mm->cfg.size_pafbct_all >> PAGE_SHIFT;
	mm->cfg.dat_pafbci_flg.b.tvp	= 0;
	PR_INF("%s:size_pafbct_all:0x%x, 0x%x, nub[%d]\n",
		 __func__,
		 mm->cfg.size_pafbct_all,
		 mm->cfg.size_pafbct_one,
		 mm->cfg.nub_pafbct);
}

/* copy from di_cnt_post_buf */
//static
int dpvpp_cnt_post_buf(struct dimn_itf_s *itf)
{
	struct dim_prevpp_ds_s *ds = NULL;
	//struct dimn_itf_s *itf;
	//struct dim_dvs_prevpp_s *dv_prevpp;
	struct div2_mm_s *mm;
	unsigned int nr_width, nr_canvas_width, canvas_align_width = 32;
	unsigned int height, width;
	unsigned int tmpa, tmpb;
	unsigned int canvas_height;
	unsigned int afbc_info_size = 0, afbc_tab_size = 0;
	unsigned int afbc_buffer_size = 0, blk_total = 0;
	enum EDPST_MODE mode;
	bool is_4k = false;
	bool is_yuv420_10 = false;
	unsigned int canvas_align_width_hf = 64;

	//dv_prevpp = &get_datal()->dvs_prevpp;
	if (!itf || !itf->ds) {
		PR_INF("%s:no ds ?\n", __func__);
		return false;
	}
	ds = itf->ds;
	mm	= &ds->mm;

	height	= mm->cfg.di_h;
	canvas_height = roundup(height, 32);
	width	= mm->cfg.di_w;
	is_4k	= ds->en_4k;
	mm->cfg.pbuf_hsize = width;
	nr_width = width;
	dbg_mem2("%s w[%d]h[%d]\n", __func__, width, height);
	nr_canvas_width = width;
	/**********************************************/
	/* count buf info */
	/**********************************************/
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A))
		canvas_align_width = 64;

	/***********************/
	mode = ds->out_mode;

	dbg_mem2("%s:mode:%d\n", __func__, mode);

	/***********************/
	if (mode == EDPST_MODE_422_10BIT_PACK)
		nr_width = (width * 5) / 4;
	else if (mode == EDPST_MODE_422_10BIT)
		nr_width = (width * 3) / 2;
	else
		nr_width = width;

	/* p */
	//tmp nr_width = roundup(nr_width, canvas_align_width);
	mm->cfg.pst_mode = mode;
	if (mode == EDPST_MODE_NV21_8BIT) {
		nr_width = roundup(nr_width, canvas_align_width);
		tmpa = (nr_width * canvas_height) >> 1;/*uv*/
		tmpb = tmpa;
		tmpa = nr_width * canvas_height;

		mm->cfg.pst_buf_uv_size = roundup(tmpb, PAGE_SIZE);
		mm->cfg.pst_buf_y_size	= roundup(tmpa, PAGE_SIZE);
		mm->cfg.pst_buf_size	= mm->cfg.pst_buf_y_size +
				mm->cfg.pst_buf_uv_size;//tmpa + tmpb;
		mm->cfg.size_post	= mm->cfg.pst_buf_size;
		mm->cfg.pst_cvs_w	= nr_width;
		mm->cfg.pst_cvs_h	= canvas_height;
		mm->cfg.pst_afbci_size	= 0;
		mm->cfg.pst_afbct_size	= 0;
	} else {
		/* 422 */
		tmpa = roundup(nr_width * canvas_height * 2, PAGE_SIZE);
		mm->cfg.pst_buf_size	= tmpa;
		mm->cfg.pst_buf_uv_size	= tmpa >> 1;
		mm->cfg.pst_buf_y_size	= mm->cfg.pst_buf_uv_size;

		if (dim_afds() && dim_afds()->cnt_info_size &&
		    ds->en_out_afbce) {
			afbc_info_size =
				dim_afds()->cnt_info_size(width,
							  height,
							  &blk_total);
			if (is_4k && ds->out_afbce_nv2110)
				is_yuv420_10 = true;

			if (is_yuv420_10)
				afbc_buffer_size =
				dim_afds()->cnt_buf_size(0x20, blk_total);
			else
				afbc_buffer_size =
					dim_afds()->cnt_buf_size(0x21, blk_total);
			afbc_buffer_size = roundup(afbc_buffer_size, PAGE_SIZE);
			if (afbc_buffer_size > mm->cfg.pst_buf_size) {
				PR_INF("%s:0x%x, 0x%x\n", "buf large:",
				       mm->cfg.pst_buf_size,
				       afbc_buffer_size);
				mm->cfg.pst_buf_size = afbc_buffer_size;
			}

			if (is_yuv420_10)
				mm->cfg.pst_buf_size = afbc_buffer_size;

			afbc_tab_size =
				dim_afds()->cnt_tab_size(mm->cfg.pst_buf_size);
		}
		mm->cfg.pst_afbci_size	= afbc_info_size;
		mm->cfg.pst_afbct_size	= afbc_tab_size;

		if (is_4k && ds->en_out_sctmem) {
			mm->cfg.size_post	= mm->cfg.pst_afbci_size;
		} else {
			mm->cfg.size_post	= mm->cfg.pst_buf_size +
					mm->cfg.pst_afbci_size;
		}

		mm->cfg.pst_cvs_w	= nr_width << 1;
		mm->cfg.pst_cvs_w	= roundup(mm->cfg.pst_cvs_w,
						  canvas_align_width);
		mm->cfg.pst_cvs_h	= canvas_height;
	}
	if (ds->en_hf_buf) {
		width = roundup(1920, canvas_align_width_hf);
		mm->cfg.size_buf_hf = width * 1080;
		mm->cfg.size_buf_hf	= PAGE_ALIGN(mm->cfg.size_buf_hf);
		mm->cfg.hf_hsize = width;
		mm->cfg.hf_vsize = 1080;

	} else {
		mm->cfg.size_buf_hf = 0;
	}

	if (ds->en_dw) {
		mm->cfg.dw_size = dim_getdw()->size_info.p.size_total;
		mm->cfg.size_post += mm->cfg.dw_size;
		//done use rsc flg pch->en_dw_mem = true;
	} else {
		mm->cfg.dw_size = 0;
	}

	//mm->cfg.size_post += mm->cfg.size_buf_hf;

	mm->cfg.size_post	= roundup(mm->cfg.size_post, PAGE_SIZE);
	mm->cfg.size_post_page	= mm->cfg.size_post >> PAGE_SHIFT;

	/* p */
	mm->cfg.pbuf_flg.b.page = mm->cfg.size_post_page;
	mm->cfg.pbuf_flg.b.is_i = 0;
	if (mm->cfg.pst_afbct_size)
		mm->cfg.pbuf_flg.b.afbc = 1;
	else
		mm->cfg.pbuf_flg.b.afbc = 0;
	if (mm->cfg.dw_size)
		mm->cfg.pbuf_flg.b.dw = 1;
	else
		mm->cfg.pbuf_flg.b.dw = 0;

	if (ds->en_out_sctmem)
		mm->cfg.pbuf_flg.b.typ = EDIM_BLK_TYP_PSCT;
	else
		mm->cfg.pbuf_flg.b.typ = EDIM_BLK_TYP_OLDP;

	if (0/*dpvpp_itf_is_ins_exbuf(itf)*/) {
		mm->cfg.pbuf_flg.b.typ |= EDIM_BLK_TYP_POUT;
		mm->cfg.size_buf_hf = 0;
	}

	dbg_mem2("%s:3 pst_cvs_w[%d]\n", __func__, mm->cfg.pst_cvs_w);

	PR_INF("%s:size:\n", __func__);
	PR_INF("\t%-15s:0x%x\n", "total_size", mm->cfg.size_post);
	PR_INF("\t%-15s:0x%x\n", "total_page", mm->cfg.size_post_page);
	PR_INF("\t%-15s:0x%x\n", "buf_size", mm->cfg.pst_buf_size);
	PR_INF("\t%-15s:0x%x\n", "y_size", mm->cfg.pst_buf_y_size);
	PR_INF("\t%-15s:0x%x\n", "uv_size", mm->cfg.pst_buf_uv_size);
	PR_INF("\t%-15s:0x%x\n", "afbci_size", mm->cfg.pst_afbci_size);
	PR_INF("\t%-15s:0x%x\n", "afbct_size", mm->cfg.pst_afbct_size);
	PR_INF("\t%-15s:0x%x\n", "flg", mm->cfg.pbuf_flg.d32);
	PR_INF("\t%-15s:0x%x\n", "dw_size", mm->cfg.dw_size);
	PR_INF("\t%-15s:0x%x\n", "size_hf", mm->cfg.size_buf_hf);

	return 0;
}

/* use dpvpp_set_post_buf replace dpvpp_cnt_post_buf*/
int dpvpp_set_post_buf(struct dimn_itf_s *itf)
{
	struct dim_prevpp_ds_s *ds = NULL;
	struct c_mm_p_cnt_out_s *inf_buf;
	struct c_cfg_afbc_s *inf_afbc_a;
	struct c_cfg_afbc_s *inf_afbc_b;
	struct div2_mm_s *mm;
	unsigned int nr_width, nr_canvas_width, canvas_align_width = 32;
	unsigned int height, width;
//	unsigned int tmpa, tmpb;
	unsigned int canvas_height;
	unsigned int afbc_info_size = 0, afbc_tab_size = 0;
	unsigned int afbc_buffer_size = 0;
	enum EDPST_MODE mode;
	bool is_4k = false;
	bool is_yuv420_10 = false;
	unsigned int canvas_align_width_hf = 64;

	if (!itf || !itf->ds) {
		PR_INF("%s:no ds ?\n", __func__);
		return false;
	}
	ds = itf->ds;
	mm	= &ds->mm;

	height	= mm->cfg.di_h;
	canvas_height = roundup(height, 32);
	width	= mm->cfg.di_w;
	is_4k	= ds->en_4k;
	mm->cfg.pbuf_hsize = width;
	nr_width = width;
	dbg_mem2("%s w[%d]h[%d]\n", __func__, width, height);
	nr_canvas_width = width;
	/**********************************************/
	/* count buf info */
	/**********************************************/
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A))
		canvas_align_width = 64;

	/***********************/
	mode = ds->out_mode;

	dbg_mem2("%s:mode:%d\n", __func__, mode);

	/***********************/

	/* p */
	//tmp nr_width = roundup(nr_width, canvas_align_width);
	mm->cfg.pst_mode = mode;
	if (mode == EDPST_MODE_NV21_8BIT) {
		inf_buf = m_rd_mm_cnt(EBLK_T_HD_NV21);
		mm->cfg.pst_buf_uv_size = inf_buf->size_total - inf_buf->off_uv;
		mm->cfg.pst_buf_y_size	= inf_buf->off_uv;
		mm->cfg.pst_buf_size	= inf_buf->size_total;
		mm->cfg.size_post	= mm->cfg.pst_buf_size;
		mm->cfg.pst_cvs_w	= inf_buf->cvs_w;
		mm->cfg.pst_cvs_h	= inf_buf->cvs_h;
		mm->cfg.pst_afbci_size	= 0;
		mm->cfg.pst_afbct_size	= 0;
	} else {
		/* 422 */
		inf_buf = m_rd_mm_cnt(EBLK_T_HD_FULL);
		inf_afbc_a = m_rd_afbc(EAFBC_T_4K_FULL);
		inf_afbc_b = m_rd_afbc(EAFBC_T_4K_YUV420_10);
		mm->cfg.pst_buf_size	= inf_buf->size_total;
		mm->cfg.pst_buf_uv_size	= inf_buf->size_total >> 1;
		mm->cfg.pst_buf_y_size	= mm->cfg.pst_buf_uv_size;

		if (dim_afds() && dim_afds()->cnt_info_size &&
		    ds->en_out_afbce) {
			afbc_info_size = inf_afbc_a->size_i;
			if (is_4k && ds->out_afbce_nv2110)
				is_yuv420_10 = true;

			if (is_yuv420_10)
				afbc_buffer_size = inf_afbc_b->size_buffer;
			else
				afbc_buffer_size = inf_afbc_a->size_buffer;

			if (afbc_buffer_size > mm->cfg.pst_buf_size) {
				PR_INF("%s:0x%x, 0x%x\n", "buf large:",
				       mm->cfg.pst_buf_size,
				       afbc_buffer_size);
				mm->cfg.pst_buf_size = afbc_buffer_size;
			}

			if (is_yuv420_10)
				mm->cfg.pst_buf_size = afbc_buffer_size;

			afbc_tab_size =	inf_afbc_a->size_table;
		}
		mm->cfg.pst_afbci_size	= afbc_info_size;
		mm->cfg.pst_afbct_size	= afbc_tab_size;

		if (is_4k && ds->en_out_sctmem) {
			mm->cfg.size_post	= mm->cfg.pst_afbci_size;
		} else {
			mm->cfg.size_post	= mm->cfg.pst_buf_size +
					mm->cfg.pst_afbci_size;
		}

		mm->cfg.pst_cvs_w	= inf_buf->cvs_w;
		mm->cfg.pst_cvs_h	= inf_buf->cvs_h;
	}
	if (ds->en_hf_buf) {
		width = roundup(1920, canvas_align_width_hf);
		mm->cfg.size_buf_hf = width * 1080;
		mm->cfg.size_buf_hf	= PAGE_ALIGN(mm->cfg.size_buf_hf);
		mm->cfg.hf_hsize = width;
		mm->cfg.hf_vsize = 1080;

	} else {
		mm->cfg.size_buf_hf = 0;
	}

	if (ds->en_dw) {
		mm->cfg.dw_size = dim_getdw()->size_info.p.size_total;
		mm->cfg.size_post += mm->cfg.dw_size;
		//done use rsc flg pch->en_dw_mem = true;
	} else {
		mm->cfg.dw_size = 0;
	}

	mm->cfg.size_post	= roundup(mm->cfg.size_post, PAGE_SIZE);
	mm->cfg.size_post_page	= mm->cfg.size_post >> PAGE_SHIFT;

	/* p */
	mm->cfg.pbuf_flg.b.page = mm->cfg.size_post_page;
	mm->cfg.pbuf_flg.b.is_i = 0;
	if (mm->cfg.pst_afbct_size)
		mm->cfg.pbuf_flg.b.afbc = 1;
	else
		mm->cfg.pbuf_flg.b.afbc = 0;
	if (mm->cfg.dw_size)
		mm->cfg.pbuf_flg.b.dw = 1;
	else
		mm->cfg.pbuf_flg.b.dw = 0;

	if (ds->en_out_sctmem)
		mm->cfg.pbuf_flg.b.typ = EDIM_BLK_TYP_PSCT;
	else
		mm->cfg.pbuf_flg.b.typ = EDIM_BLK_TYP_OLDP;

	if (0/*dpvpp_itf_is_ins_exbuf(itf)*/) {
		mm->cfg.pbuf_flg.b.typ |= EDIM_BLK_TYP_POUT;
		mm->cfg.size_buf_hf = 0;
	}

	dbg_mem2("%s:3 pst_cvs_w[%d]\n", __func__, mm->cfg.pst_cvs_w);

#ifdef HIS_CODE
	PR_INF("%s:size:\n", __func__);
	PR_INF("\t%-15s:0x%x\n", "total_size", mm->cfg.size_post);
	PR_INF("\t%-15s:0x%x\n", "total_page", mm->cfg.size_post_page);
	PR_INF("\t%-15s:0x%x\n", "buf_size", mm->cfg.pst_buf_size);
	PR_INF("\t%-15s:0x%x\n", "y_size", mm->cfg.pst_buf_y_size);
	PR_INF("\t%-15s:0x%x\n", "uv_size", mm->cfg.pst_buf_uv_size);
	PR_INF("\t%-15s:0x%x\n", "afbci_size", mm->cfg.pst_afbci_size);
	PR_INF("\t%-15s:0x%x\n", "afbct_size", mm->cfg.pst_afbct_size);
	PR_INF("\t%-15s:0x%x\n", "flg", mm->cfg.pbuf_flg.d32);
	PR_INF("\t%-15s:0x%x\n", "dw_size", mm->cfg.dw_size);
	PR_INF("\t%-15s:0x%x\n", "size_hf", mm->cfg.size_buf_hf);
#endif
	return 0;
}

#ifdef HIS_CODE
//pst_sec_alloc(pch, mm->cfg.dat_pafbct_flg.d32);
/* copy from pst_sec_alloc*/
static void dpvpp_pst_sec_alloc(struct dimn_itf_s *itf, unsigned int flg)
{
	//struct dimn_itf_s *itf;
	struct di_dat_s *pdat;
	struct blk_flg_s flgs;
	unsigned int dat_size;
	//for cma:
	struct dim_mm_s omm;
	bool ret;
	struct dim_pvpp_hw_s *hw;

	//itf = &get_datal()->dvs_prevpp.itf;

	if (!itf || !itf->ds || !(itf->c.src_state & EDVPP_SRC_NEED_SCT_TAB))
		return;
	hw = &get_datal()->dvs_prevpp.hw;

	pdat = &hw->dat_p_afbct;

	flgs.d32 = flg;

	dat_size = flgs.b.page;

	if (pdat->flg_alloc || !dat_size) {
		dbg_reg("%s:not alloc:%d,0x%x\n",
		       __func__,
		       pdat->flg_alloc, flg);
		return;
	}
	ret = mm_cma_alloc(NULL, dat_size, &omm);

	if (!ret) {
		PR_ERR("%s:cma\n", __func__);
		return;
	}
	pdat->addr_st	= omm.addr;
	//pdat->ppage	= omm.ppage;
	pdat->virt	= omm.ppage;
	pdat->cnt	= dat_size;
	pdat->addr_end = pdat->addr_st + (dat_size << PAGE_SHIFT);

	pdat->flg.d32 = flg;
	pdat->flg_alloc = 1;
	PR_INF("%s:cma:size:%d,0x%px,0x%lx\n",
	       __func__,
	       dat_size, pdat->virt, pdat->addr_st);

	// tmp pat_set_addr(pch);

	itf->c.src_state &= (~EDVPP_SRC_NEED_SCT_TAB);
}

/* copy from dim_sec_release */
static void dpvpp_sec_release(struct di_dat_s *dat)
{
	if (dat->flg_alloc) {
		dma_release_from_contiguous(NULL,
					    (struct page *)dat->virt,
					    dat->cnt);
		dat->virt = NULL;

		memset(dat, 0, sizeof(*dat));
	}
}
#endif

#ifdef HIS_CODE
static bool dpvpp_reg_alloc_mem(struct dimn_itf_s *itf)
{
	struct dim_prevpp_ds_s *ds;
	bool is_tvp = false;
	unsigned int total_page;
	struct div2_mm_s *mm;
	struct dim_mm_blk_s *blk_buf;
	struct dim_pvpp_hw_s *hw;

#ifdef DBG_TIMER
	u64 ustime, udiff;

	//ustime = cur_to_usecs();
	ustime	= cur_to_msecs();
#endif

	if (!itf || !itf->ds || !(itf->c.src_state & EDVPP_SRC_NEED_MEM))
		return false;
	/**/
	hw = &get_datal()->dvs_prevpp.hw;
	hw->reg_nub++;

	ds = itf->ds;
	blk_buf = &hw->blk_buf;
	if (blk_buf->flg_alloc) //double check
		return false;
	mm = &ds->mm;

	if (itf->c.is_tvp/* == 2*/)
		is_tvp = true;

	total_page = mm->cfg.size_post_page * mm->cfg.num_post;
	cma_alloc_blk_block(blk_buf, ds->cma_mode, is_tvp, total_page);

#ifdef DBG_TIMER
	udiff	= cur_to_msecs();
	udiff	-= ustime;
	dbg_mem2("%s:use %u ms\n", __func__, (unsigned int)udiff);
#endif
	if (!blk_buf->flg_alloc) {
		PR_ERR("%s: alloc fail\n", __func__);
		return false;
	}

	dbg_mem2("%s:\n", __func__);
	itf->c.src_state &= (~EDVPP_SRC_NEED_MEM);
	return true;
}

static void dpvpp_mem_release(struct dim_prevpp_ds_s *ds)
{
#ifdef DBG_TIMER
	u64 ustime, udiff;
#endif
	struct dim_pvpp_hw_s *hw;

	hw = &get_datal()->dvs_prevpp.hw;

	if (!ds || !hw->blk_buf.flg_alloc)
		return;

	hw->reg_nub--;
#ifdef DBG_TIMER
	ustime = cur_to_usecs();
	//ustime	= cur_to_msecs();
#endif
	cma_release_blk_block(&hw->blk_buf, ds->cma_mode);

#ifdef DBG_TIMER
	udiff	= cur_to_usecs();
	udiff	-= ustime;
	PR_INF("%s:use %u us\n", __func__, (unsigned int)udiff);
#else
	PR_INF("%s:\n", __func__);
#endif
}
#endif
irqreturn_t dpvpp_irq(int irq, void *dev_instance)
{
	struct dim_prevpp_ds_s *ds;
	//struct di_hpre_s  *pre = get_hw_pre();
	struct dim_pvpp_hw_s *hw;
	unsigned int id;

	unsigned int data32 = RD(DI_INTR_CTRL);
	unsigned int mask32 = (data32 >> 16) & 0x3ff;
	unsigned int flag = 0;
	//unsigned long irq_flg;
	static unsigned int dbg_last_fcnt;

	//dim_print("%s:\n", __func__);
	hw = &get_datal()->dvs_prevpp.hw;
	id = hw->id_c;

	ds = get_datal()->dvs_prevpp.itf[id].ds;
	if (!ds) {
		PR_ERR("%s:no ds\n", __func__);
		return IRQ_HANDLED;
	}

	//dim_ddbg_mod_save(EDI_DBG_MOD_PRE_IRQB, 0, 0);
#ifdef HIS_CODE
	if (!sc2_dbg_is_en_pre_irq()) {
		sc2_dbg_pre_info(data32);
		dim_ddbg_mod_save(EDI_DBG_MOD_PRE_IRQE, 0, 0);
		return;
	}
#endif
	if (dim_is_pre_link_l() || !hw->en_pst_wr_test) {
#ifdef HIS_CODE
		data32 &= 0x3fffffff;
		DIM_DI_WR(DI_INTR_CTRL, data32);
		DIM_DI_WR(DI_INTR_CTRL,
			  (data32 & 0xfffffffb) | (hw->intr_mode << 30));
#endif
		if (ds &&
		    dbg_last_fcnt == ds->pre_done_nub) {
			//dim_print("irq:pre link %d\n", ds->pre_done_nub);
			dbg_irq_status(0x02); /*dbg*/
			return IRQ_HANDLED;
		}
	}
	dbg_last_fcnt = ds->pre_done_nub;
	data32 &= 0x3fffffff;
	if ((data32 & 2) && !(mask32 & 2)) {
		dim_print("irq pre MTNWR ==\n");
		flag = 1;
	} else if ((data32 & 1) && !(mask32 & 1)) {
		dim_print("irq pre NRWR ==\n");
		flag = 1;
	} else {
		dim_print("irq pre DI IRQ 0x%x ==\n", data32);
		flag = 0;
	}

	if (flag && !hw->en_pst_wr_test) {
		//2023-03-30 dpvpph_gl_sw(false, true, &di_pre_regset);
		//dim_arb_sw(false);	//test-04 0622
		//dim_arb_sw(true);	//test-04 0622
		dbg_irq_status(0x03); /*dbg*/
	}

	DIM_DI_WR(DI_INTR_CTRL,
	  (data32 & 0xfffffffb) | (hw->intr_mode << 30));

	if (hw->en_pst_wr_test)
		complete(&hw->pw_done);
	return IRQ_HANDLED;
}

#ifdef HIS_CODE	//disable unreg irq
static void unreg_irq(void)
{
	struct platform_device *pdev;

	if (!get_dim_de_devp()		||
	    !get_dim_de_devp()->pdev	||
	    !get_dim_de_devp()->pre_irq_req)
		return;
	pdev = get_dim_de_devp()->pdev;

	devm_free_irq(&pdev->dev,
			       get_dim_de_devp()->pre_irq,
			       (void *)"pre_di");
	get_dim_de_devp()->pre_irq_req = false;

	PR_INF("%s:ok\n", __func__);
}
#endif

//static
bool dpvpp_mem_reg_ch(struct dimn_itf_s *itf)
{
	struct dim_pvpp_hw_s *hw;
	struct dim_prevpp_ds_s *ds;
	struct div2_mm_s *mm;
	//unsigned int out_fmt;
	//struct dvfm_s *dvfm_dm;

	if (!itf)
		return false;
	if (!itf->ds)
		return false;
	if (itf->c.src_state & EDVPP_SRC_NEED_MSK_CRT)
		return false;

	if (!itf->c.sum_pre_get)
		return false;

	if (!itf->c.m_mode_n)
		return false;
	if (!(itf->c.src_state & EDVPP_SRC_NEED_MEM))
		return false;

	hw = &get_datal()->dvs_prevpp.hw;
	ds = itf->ds;
	mm	= &ds->mm;
	if ((itf->c.m_mode_n == EPVPP_MEM_T_HD_FULL && hw->blk_rd_hd) ||
	    ((itf->c.m_mode_n == EPVPP_MEM_T_UHD_AFBCE ||
	      itf->c.m_mode_n == EPVPP_MEM_T_UHD_AFBCE_HD) &&
	     hw->blk_rd_uhd)) {
		PR_INF("%s:ch[%d]:%d:%d:%d\n", __func__, itf->bind_ch,
			itf->c.m_mode_n, hw->blk_rd_hd, hw->blk_rd_uhd);
		ds = itf->ds;

		if (!ds->en_out_afbce)
			itf->c.src_state &= ~(EDVPP_SRC_NEED_SCT_TAB);

		/* cnt post mem size */
		if (dim_afds())
			dim_afds()->pvpp_reg_val(itf);
		dpvpp_set_pst_afbct(itf);
		dpvpp_set_post_buf(itf);
		//dpvpp_pst_sec_alloc(itf, mm->cfg.dat_pafbct_flg.d32);

		if (dpvpp_dbg_force_vpp_active())
			dpvpp_link_sw_api(true);

		hw->reg_nub++;
		itf->c.flg_reg_mem = true;
		itf->c.src_state &= (~EDVPP_SRC_NEED_MEM);

		itf->c.src_state &= (~EDVPP_SRC_NEED_REG2);
		get_datal()->pre_vpp_exist = true; /* ?? */

		dbg_plink1("%s:ok:src_state[0x%x],pst_wr[%d], afbce[%d]\n",
			__func__,
			itf->c.src_state,
			hw->en_pst_wr_test,
			ds->en_out_afbce);
	}

	return true;
}

static struct pvpp_buf_cfg_s *get_buf_cfg(unsigned char index, unsigned int pos)
{
	struct pvpp_buf_cfg_s *buf_cfg;
	struct dim_pvpp_hw_s *hw;

	hw = &get_datal()->dvs_prevpp.hw;
	if (!hw || index >= K_BUF_CFG_T_NUB) {
		PR_WARN("%s:out_fmt[%d]:%d\n", __func__, index, pos);
		return NULL;
	}
	buf_cfg = &hw->buf_cfg[index];
	if (!buf_cfg->support) {
		PR_WARN("%s:not support [%d]:%d\n", __func__, index, pos);
		return NULL;
	}
	return buf_cfg;
}

static void buf_cfg_prob(void)
{
	struct dim_pvpp_hw_s *hw;
	struct pvpp_buf_cfg_s *cfg;
//	unsigned char index = 0;
	struct c_mm_p_cnt_out_s *inf_buf;
	int i, j;

	hw = &get_datal()->dvs_prevpp.hw;
	memset(&hw->buf_cfg[0], 0, sizeof(hw->buf_cfg));

	for (i = 0; i < K_BUF_CFG_T_NUB; i++) {
		cfg = &hw->buf_cfg[i];
		cfg->type = i;
		cfg->dbuf_plan_nub = 1;
		if (i == EPVPP_BUF_CFG_T_HD_F	||
		    i == EPVPP_BUF_CFG_T_HD_F_TVP) {
			inf_buf = m_rd_mm_cnt(EBLK_T_HD_FULL);
			cfg->dbuf_hsize = inf_buf->dbuf_hsize;
			for (j = 0; j < DPVPP_BUF_NUB; j++) {
				cfg->dbuf_wr_cfg[j][0].width = inf_buf->cvs_w;
				cfg->dbuf_wr_cfg[j][0].height = inf_buf->cvs_h;
			}
		} else if (i == EPVPP_BUF_CFG_T_UHD_F_AFBCE	||
			   i == EPVPP_BUF_CFG_T_UHD_F_AFBCE_TVP) {
			inf_buf = m_rd_mm_cnt(EBLK_T_UHD_FULL);
			cfg->dbuf_hsize = inf_buf->dbuf_hsize;
			for (j = 0; j < DPVPP_BUF_NUB; j++) {
				cfg->dbuf_wr_cfg[j][0].width = inf_buf->cvs_w;
				cfg->dbuf_wr_cfg[j][0].height = inf_buf->cvs_h;
			}
		} else if (i == EPVPP_BUF_CFG_T_UHD_F_AFBCE_HD	||
			   i == EPVPP_BUF_CFG_T_UHD_F_AFBCE_HD_TVP) {
			inf_buf = m_rd_mm_cnt(EBLK_T_UHD_FULL);
			cfg->dbuf_hsize = inf_buf->dbuf_hsize;
			for (j = 0; j < DPVPP_BUF_NUB; j++) {
				cfg->dbuf_wr_cfg[j][0].width = inf_buf->cvs_w;
				cfg->dbuf_wr_cfg[j][0].height = inf_buf->cvs_h;
			}
		} else if (i == EPVPP_BUF_CFG_T_HD_NV21	||
			   i == EPVPP_BUF_CFG_T_HD_NV21_TVP) {
			inf_buf = m_rd_mm_cnt(EBLK_T_HD_NV21);
			cfg->dbuf_plan_nub = 2;
			cfg->dbuf_hsize = inf_buf->dbuf_hsize;
			for (j = 0; j < DPVPP_BUF_NUB; j++) {
				cfg->dbuf_wr_cfg[j][0].width = inf_buf->cvs_w;
				cfg->dbuf_wr_cfg[j][0].height = inf_buf->cvs_h;
				cfg->dbuf_wr_cfg[j][1].width = inf_buf->cvs_w;
				cfg->dbuf_wr_cfg[j][1].height = inf_buf->cvs_h;
			}
		} else if (i == EPVPP_BUF_CFG_T_UHD_NV21	||
			   i == EPVPP_BUF_CFG_T_UHD_NV21_TVP) {
			inf_buf = m_rd_mm_cnt(EBLK_T_HD_NV21);

			cfg->dbuf_plan_nub = 2;
			cfg->dbuf_hsize = inf_buf->dbuf_hsize;
			for (j = 0; j < DPVPP_BUF_NUB; j++) {
				cfg->dbuf_wr_cfg[j][0].width = inf_buf->cvs_w;
				cfg->dbuf_wr_cfg[j][0].height = inf_buf->cvs_h;
				cfg->dbuf_wr_cfg[j][1].width = inf_buf->cvs_w;
				cfg->dbuf_wr_cfg[j][1].height = inf_buf->cvs_h;
			}
		} else if (i == EPVPP_BUF_CFG_T_UHD_F	||
			   i == EPVPP_BUF_CFG_T_UHD_F_TVP) {
			inf_buf = m_rd_mm_cnt(EBLK_T_UHD_FULL);
			cfg->dbuf_hsize = inf_buf->dbuf_hsize;
			for (j = 0; j < DPVPP_BUF_NUB; j++) {
				cfg->dbuf_wr_cfg[j][0].width = inf_buf->cvs_w;
				cfg->dbuf_wr_cfg[j][0].height = inf_buf->cvs_h;
			}
		}
	}
}

/**************************************
 * mode : ff:  clear all not care idx
 * mode : 0: clear the idx
 * mode : 1: set idx
 * idx: ref to enum EPVPP_BUF_CFG_T
 * note:
 *	for set, use EPVPP_BUF_CFG_T_HD_F meas:
 *	EPVPP_BUF_CFG_T_HD_F or
 *	EPVPP_BUF_CFG_T_HD_F_TVP and
 *	EPVPP_BUF_CFG_T_HD_NV21 or
 *	EPVPP_BUF_CFG_T_HD_NV21_TVP
 **************************************/
//static
void mem_count_buf_cfg(unsigned char mode, unsigned char idx)
{
	struct dim_pvpp_hw_s *hw;
	struct pvpp_buf_cfg_s *cfg;
	unsigned char index = 0;
	struct c_blk_m_s	*blk_m;
	struct blk_s *blk;
	int i, j;
	unsigned int offs;
	struct c_mm_p_cnt_out_s *inf_nv21;
	struct c_cfg_afbc_s *inf_a;
	struct afbce_map_s afbce_map;
	struct di_dev_s *devp = get_dim_de_devp();
	unsigned int b_uhd_size, b_hd_size, t_offs, size_buf, size_table;
	unsigned int b_uhd_total, b_hd_total;
	unsigned long buf_start;

	hw = &get_datal()->dvs_prevpp.hw;
	if (mode == 0xff) {
		for (i = 0; i < K_BUF_CFG_T_NUB; i++)
			hw->buf_cfg[i].support = false;

		PR_INF("%s:clear all\n", __func__);
		return;
	}
	if (mode > 1 || idx >= EPVPP_BUF_CFG_T_NUB) {
		PR_WARN("%s:overflow:%d:%d\n", __func__, mode, idx);
		return;
	}
	if (mode == 0) {
		hw->buf_cfg[idx].support = false;
		PR_INF("%s:clear %d\n", __func__, idx);
		//dbg_str_buf_cfg(&hw->buf_cfg[idx], get_name_buf_cfg(idx));
		return;
	}

	if (mode == 1 &&
		   (idx == EPVPP_BUF_CFG_T_HD_F ||
		    idx == EPVPP_BUF_CFG_T_HD_F_TVP)) {
		/* for hd */
		inf_nv21 = m_rd_mm_cnt(EBLK_T_HD_NV21);
		if (!hw->src_blk[0] || !hw->src_blk[1] ||
		    !inf_nv21) {
			PR_WARN("%s:no blk?\n", __func__);
			return;
		}

		if (hw->flg_tvp)
			index = EPVPP_BUF_CFG_T_HD_F_TVP;
		else
			index = EPVPP_BUF_CFG_T_HD_F;
		/**/
		cfg = &hw->buf_cfg[index];
		for (i = 0; i < DPVPP_BUF_NUB; i++) {
			blk = hw->src_blk[i];
			if (!blk) {
				PR_ERR("%s:hd:no src blk\n", __func__);
				return;
			}
			blk_m = &blk->c.b.blk_m;
			cfg->dbuf_wr_cfg[i][0].phy_addr = blk_m->mem_start;
		}
		cfg->support = true;
		//dbg_str_buf_cfg(&hw->buf_cfg[index], get_name_buf_cfg(index));
		/* for nv21 */
		if (hw->flg_tvp)
			index = EPVPP_BUF_CFG_T_HD_NV21_TVP;
		else
			index = EPVPP_BUF_CFG_T_HD_NV21;
		/**/
		cfg = &hw->buf_cfg[index];

		offs = 0;
		for (i = 0; i < DPVPP_BUF_NUB; i++) {
			blk = hw->src_blk[i];
			if (!blk) {
				PR_ERR("%s:hd:no src blk\n", __func__);
				return;
			}
			blk_m = &blk->c.b.blk_m;
			cfg->dbuf_wr_cfg[i][0].phy_addr = blk_m->mem_start;

			offs = blk_m->mem_start + inf_nv21->off_uv;
			cfg->dbuf_wr_cfg[i][1].phy_addr = offs;
		}
		cfg->support = true;
		//dbg_str_buf_cfg(&hw->buf_cfg[index], get_name_buf_cfg(index));
	} else if (mode == 1 &&
		   (idx == EPVPP_BUF_CFG_T_UHD_F_AFBCE ||
		    idx == EPVPP_BUF_CFG_T_UHD_F_AFBCE_TVP)) {
		/* check blk */
		for (i = 0; i < 2; i++) {
			if (!hw->src_blk_uhd_afbce[i]) {
				PR_ERR("%s:no buffer?%d\n", __func__, i);
				return;
			}
		}
		if (hw->flg_tvp)
			index = EPVPP_BUF_CFG_T_UHD_F_AFBCE_TVP;
		else
			index = EPVPP_BUF_CFG_T_UHD_F_AFBCE;
		cfg = &hw->buf_cfg[index];

		offs = 0;
		inf_a = m_rd_afbc(EAFBC_T_4K_FULL);
		/* afbce table */
		cfg->addr_e[0] = hw->src_blk_e->c.b.blk_m.mem_start;
		cfg->addr_e[1] = cfg->addr_e[0] + inf_a->size_table;

		/* afbce infor */
		cfg->addr_i[0] = hw->src_blk_uhd_afbce[0]->c.b.blk_m.mem_start;
		cfg->addr_i[1] = hw->src_blk_uhd_afbce[1]->c.b.blk_m.mem_start;

		inf_nv21 = m_rd_mm_cnt(EBLK_T_UHD_FULL_AFBCE);

		for (j = 0; j < DPVPP_BUF_NUB; j++) {
			buf_start = cfg->addr_i[j] + inf_a->size_i;
			cfg->afbc_crc[j] = 0;
			afbce_map.bodyadd	= buf_start;
			afbce_map.tabadd	= cfg->addr_e[j];
			afbce_map.size_buf	= inf_a->size_buffer;
			afbce_map.size_tab	= inf_a->size_table;
			cfg->afbc_crc[j] =
				dim_afds()->int_tab(&devp->pdev->dev,
						    &afbce_map);
			/* mif */
			cfg->dbuf_wr_cfg[j][0].phy_addr = buf_start;
		}
		cfg->support = true;
		//dbg_str_buf_cfg(&hw->buf_cfg[index], get_name_buf_cfg(index));
		/* uhd buffer config hd buffer */
		if (hw->flg_tvp)
			index = EPVPP_BUF_CFG_T_HD_F_TVP;
		else
			index = EPVPP_BUF_CFG_T_HD_F;
		/**/
		cfg = &hw->buf_cfg[index];

		offs = 0;
		inf_nv21 = m_rd_mm_cnt(EBLK_T_HD_FULL);
		buf_start = hw->src_blk_uhd_afbce[0]->c.b.blk_m.mem_start;
		for (i = 0; i < DPVPP_BUF_NUB; i++) {
			cfg->dbuf_wr_cfg[i][0].phy_addr = buf_start + offs;
			offs = inf_nv21->size_total;
			PR_INF("addr:[%d]:0x%lx:\n", i,
				cfg->dbuf_wr_cfg[i][0].phy_addr);
		}
		cfg->support = true;
		//dbg_str_buf_cfg(&hw->buf_cfg[index], get_name_buf_cfg(index));
		/**********************************************/
		/* nv 21 to-do */
	} else if (mode == 1 &&
		   (idx == EPVPP_BUF_CFG_T_UHD_F_AFBCE_HD ||
		    idx == EPVPP_BUF_CFG_T_UHD_F_AFBCE_HD_TVP)) {
		/* check blk */
		for (i = 0; i < 10; i++) {
			if (!hw->src_blk[i]) {
				PR_ERR("%s:no buffer?%d\n", __func__, i);
				return;
			}
		}
		if (hw->flg_tvp)
			index = EPVPP_BUF_CFG_T_UHD_F_AFBCE_HD_TVP;
		else
			index = EPVPP_BUF_CFG_T_UHD_F_AFBCE_HD;
		cfg = &hw->buf_cfg[index];
		offs = 0;
		inf_a = m_rd_afbc(EAFBC_T_4K_FULL);
		/* afbce table */
		cfg->addr_e[0] = hw->src_blk_e->c.b.blk_m.mem_start;
		cfg->addr_e[1] = cfg->addr_e[0] + inf_a->size_table;
		PR_INF("%s:afbc table:0x%lx, 0x%lx\n", __func__,
			cfg->addr_e[0],
			cfg->addr_e[1]);
		/* afbce infor */
		inf_nv21 = m_rd_mm_cnt(EBLK_T_HD_FULL);
		cfg->addr_i[0] = hw->src_blk[4]->c.b.blk_m.mem_start +
			inf_nv21->size_total - inf_a->size_i;
		cfg->addr_i[1] = hw->src_blk[9]->c.b.blk_m.mem_start +
			inf_nv21->size_total - inf_a->size_i;
		PR_INF("%s:afbc infor:0x%lx, 0x%lx\n", __func__,
			cfg->addr_i[0],
			cfg->addr_i[1]);
		//-----------------
		//buffer_4k	buffer_hd	table
		// b_uhd_total	b_hd_total	t_total
		// b_uhd_size	b_hd_size
		//				t_offs
		//
		b_hd_size = (inf_nv21->size_total >> 12) * sizeof(unsigned int);
		b_hd_total = (inf_nv21->size_total >> 12) << 12;
		for (j = 0; j < DPVPP_BUF_NUB; j++) {
			cfg->afbc_crc[j] = 0;
			t_offs = 0;
			b_uhd_size = ((inf_a->size_buffer + 0xfff) >> 12) * sizeof(unsigned int);
			b_uhd_total = ((inf_a->size_buffer + 0xfff) >> 12) << 12;

			for (i = 0; i < 5; i++) {
				blk = hw->src_blk[i + j * 5];
				blk_m = &blk->c.b.blk_m;

				if (b_uhd_size > b_hd_size) {
					size_table = b_hd_size;
					size_buf = b_hd_total;
				} else {
					size_table = inf_a->size_table - t_offs;
					size_buf = b_uhd_total;
				}
				afbce_map.bodyadd	= blk_m->mem_start;
				afbce_map.tabadd	= cfg->addr_e[j] + t_offs;
				afbce_map.size_buf	= size_buf;
				afbce_map.size_tab	= size_table;

				cfg->afbc_crc[j] +=
					dim_afds()->int_tab(&devp->pdev->dev,
							    &afbce_map);
				t_offs += size_table;
				b_uhd_size -= size_table;
				b_uhd_total -= b_hd_total;
			}
		}
		cfg->support = true;
		//dbg_str_buf_cfg(&hw->buf_cfg[index], get_name_buf_cfg(index));
	}
}

/************************************************
 * idx: enum EPVPP_BUF_CFG_T
 * by: enum EPVPP_MEM_T
 ************************************************/
static void mem_count_buf_cfg2(unsigned char mode, unsigned char idx, unsigned char by)
{
	struct dim_pvpp_hw_s *hw;
	struct pvpp_buf_cfg_s *cfg;
	unsigned char index = 0;
	struct c_blk_m_s	*blk_m;
	struct blk_s *blk;
	int i, j;
	unsigned int offs;
	struct c_mm_p_cnt_out_s *inf_nv21, *inf_buf;
	struct c_cfg_afbc_s *inf_a;
	struct afbce_map_s afbce_map;
	struct di_dev_s *devp = get_dim_de_devp();
	unsigned int b_uhd_size, b_hd_size, t_offs, size_buf, size_table;
	unsigned int b_uhd_total, b_hd_total;
	unsigned long buf_start;

	hw = &get_datal()->dvs_prevpp.hw;
	if (mode == 0xff) {
		for (i = 0; i < K_BUF_CFG_T_NUB; i++)
			hw->buf_cfg[i].support = false;

		PR_INF("%s:clear all\n", __func__);
		return;
	}
	if (mode > 1 || idx >= EPVPP_BUF_CFG_T_NUB) {
		PR_WARN("%s:overflow:%d:%d\n", __func__, mode, idx);
		return;
	}
	if (mode == 0) {
		hw->buf_cfg[idx].support = false;
		PR_INF("%s:clear %d\n", __func__, idx);
		//dbg_str_buf_cfg(&hw->buf_cfg[idx], get_name_buf_cfg(idx));
		return;
	}

	if ((idx == EPVPP_BUF_CFG_T_HD_F ||
	     idx == EPVPP_BUF_CFG_T_HD_F_TVP) &&
	    (by == EPVPP_MEM_T_HD_FULL ||
	     by == EPVPP_MEM_T_UHD_AFBCE_HD)) {
		if (!hw->src_blk[0] || !hw->src_blk[1]) {
			PR_WARN("%s:no blk?\n", __func__);
			return;
		}

		if (hw->flg_tvp)
			index = EPVPP_BUF_CFG_T_HD_F_TVP;
		else
			index = EPVPP_BUF_CFG_T_HD_F;
		/*check active or not to-do */
		/**/
		cfg = &hw->buf_cfg[index];
		for (i = 0; i < DPVPP_BUF_NUB; i++) {
			blk = hw->src_blk[i];
			if (!blk) {
				PR_ERR("%s:1:no src blk\n", __func__);
				return;
			}
			blk_m = &blk->c.b.blk_m;
			cfg->dbuf_wr_cfg[i][0].phy_addr = blk_m->mem_start;
		}
		cfg->support = true;
	} else if ((idx == EPVPP_BUF_CFG_T_UHD_F ||
		    idx == EPVPP_BUF_CFG_T_UHD_F_TVP) &&
		   (by == EPVPP_MEM_T_UHD_FULL ||
		   by == EPVPP_MEM_T_UHD_AFBCE)) {
		if (!hw->src_blk_uhd_afbce[0] || !hw->src_blk_uhd_afbce[1]) {
			PR_ERR("%s:2:no blk?\n", __func__);
			return;
		}

		if (hw->flg_tvp)
			index = EPVPP_BUF_CFG_T_UHD_F_TVP;
		else
			index = EPVPP_BUF_CFG_T_UHD_F;
		/*check active or not to-do */
		/**/
		cfg = &hw->buf_cfg[index];
		for (i = 0; i < DPVPP_BUF_NUB; i++) {
			blk = hw->src_blk_uhd_afbce[i];
			if (!blk) {
				PR_ERR("%s:hd:no src blk\n", __func__);
				return;
			}
			blk_m = &blk->c.b.blk_m;
			cfg->dbuf_wr_cfg[i][0].phy_addr = blk_m->mem_start;
		}
		cfg->support = true;
	} else if ((idx == EPVPP_BUF_CFG_T_HD_F ||
		    idx == EPVPP_BUF_CFG_T_HD_F_TVP) &&
		    (by == EPVPP_MEM_T_UHD_FULL ||
		     by == EPVPP_MEM_T_UHD_AFBCE)) {
		//dbg_str_buf_cfg(&hw->buf_cfg[index], get_name_buf_cfg(index));
		/* uhd buffer config hd buffer */
		if (hw->flg_tvp)
			index = EPVPP_BUF_CFG_T_HD_F_TVP;
		else
			index = EPVPP_BUF_CFG_T_HD_F;
		/**/
		cfg = &hw->buf_cfg[index];

		offs = 0;
		inf_buf = m_rd_mm_cnt(EBLK_T_HD_FULL);
		buf_start = hw->src_blk_uhd_afbce[0]->c.b.blk_m.mem_start;
		for (i = 0; i < DPVPP_BUF_NUB; i++) {
			cfg->dbuf_wr_cfg[i][0].phy_addr = buf_start + offs;
			offs = inf_buf->size_total;
			PR_INF("addr:[%d]:0x%lx:\n", i,
				cfg->dbuf_wr_cfg[i][0].phy_addr);
		}
		cfg->support = true;
		//dbg_str_buf_cfg(&hw->buf_cfg[index], get_name_buf_cfg(index));
	} else if ((idx == EPVPP_BUF_CFG_T_HD_NV21 ||
		   idx == EPVPP_BUF_CFG_T_HD_NV21_TVP) &&
		   (by == EPVPP_MEM_T_HD_FULL ||
		    by == EPVPP_MEM_T_UHD_AFBCE_HD)) {
		inf_nv21 = m_rd_mm_cnt(EBLK_T_HD_NV21);
		if (!inf_nv21) {
			PR_ERR("no inf:2\n");
			return;
		}
		if (hw->flg_tvp)
			index = EPVPP_BUF_CFG_T_HD_NV21_TVP;
		else
			index = EPVPP_BUF_CFG_T_HD_NV21;
		/*check active or not to-do */
		cfg = &hw->buf_cfg[index];

		offs = 0;
		for (i = 0; i < DPVPP_BUF_NUB; i++) {
			blk = hw->src_blk[i];
			if (!blk) {
				PR_ERR("%s:hd:no src blk\n", __func__);
				return;
			}
			blk_m = &blk->c.b.blk_m;
			cfg->dbuf_wr_cfg[i][0].phy_addr = blk_m->mem_start;

			offs = blk_m->mem_start + inf_nv21->off_uv;
			cfg->dbuf_wr_cfg[i][1].phy_addr = offs;
		}
		cfg->support = true;
	} else if ((idx == EPVPP_BUF_CFG_T_UHD_F_AFBCE ||
		    idx == EPVPP_BUF_CFG_T_UHD_F_AFBCE_TVP) &&
		   by == EPVPP_MEM_T_UHD_AFBCE) {
		/* check blk */
		for (i = 0; i < 2; i++) {
			if (!hw->src_blk_uhd_afbce[i]) {
				PR_ERR("%s:no buffer?%d\n", __func__, i);
				return;
			}
		}
		if (hw->flg_tvp)
			index = EPVPP_BUF_CFG_T_UHD_F_AFBCE_TVP;
		else
			index = EPVPP_BUF_CFG_T_UHD_F_AFBCE;
		cfg = &hw->buf_cfg[index];

		offs = 0;
		inf_a = m_rd_afbc(EAFBC_T_4K_FULL);
		/* afbce table */
		cfg->addr_e[0] = hw->src_blk_e->c.b.blk_m.mem_start;
		cfg->addr_e[1] = cfg->addr_e[0] + inf_a->size_table;

		/* afbce infor */
		cfg->addr_i[0] = hw->src_blk_uhd_afbce[0]->c.b.blk_m.mem_start;
		cfg->addr_i[1] = hw->src_blk_uhd_afbce[1]->c.b.blk_m.mem_start;

		for (j = 0; j < DPVPP_BUF_NUB; j++) {
			buf_start = cfg->addr_i[j] + inf_a->size_i;
			cfg->afbc_crc[j] = 0;
			afbce_map.bodyadd	= buf_start;
			afbce_map.tabadd	= cfg->addr_e[j];
			afbce_map.size_buf	= inf_a->size_buffer;
			afbce_map.size_tab	= inf_a->size_table;
			cfg->afbc_crc[j] =
				dim_afds()->int_tab(&devp->pdev->dev,
						    &afbce_map);
			/* mif */
			cfg->dbuf_wr_cfg[j][0].phy_addr = buf_start;
		}
		cfg->support = true;
	} else if ((idx == EPVPP_BUF_CFG_T_UHD_F_AFBCE_HD ||
		    idx == EPVPP_BUF_CFG_T_UHD_F_AFBCE_HD_TVP) &&
		   by == EPVPP_MEM_T_UHD_AFBCE_HD) {
		/* check blk */
		for (i = 0; i < 10; i++) {
			if (!hw->src_blk[i]) {
				PR_ERR("%s:no buffer?%d\n", __func__, i);
				return;
			}
		}
		if (hw->flg_tvp)
			index = EPVPP_BUF_CFG_T_UHD_F_AFBCE_HD_TVP;
		else
			index = EPVPP_BUF_CFG_T_UHD_F_AFBCE_HD;
		cfg = &hw->buf_cfg[index];
		offs = 0;
		inf_a = m_rd_afbc(EAFBC_T_4K_FULL);
		/* afbce table */
		cfg->addr_e[0] = hw->src_blk_e->c.b.blk_m.mem_start;
		cfg->addr_e[1] = cfg->addr_e[0] + inf_a->size_table;
		PR_INF("%s:afbc table:0x%lx, 0x%lx\n", __func__,
			cfg->addr_e[0],
			cfg->addr_e[1]);
		/* afbce infor */
		inf_buf = m_rd_mm_cnt(EBLK_T_HD_FULL);
		cfg->addr_i[0] = hw->src_blk[4]->c.b.blk_m.mem_start +
			inf_buf->size_total - inf_a->size_i;
		cfg->addr_i[1] = hw->src_blk[9]->c.b.blk_m.mem_start +
			inf_buf->size_total - inf_a->size_i;
		PR_INF("%s:afbc infor:0x%lx, 0x%lx\n", __func__,
			cfg->addr_i[0],
			cfg->addr_i[1]);
		//-----------------
		//buffer_4k	buffer_hd	table
		// b_uhd_total	b_hd_total	t_total
		// b_uhd_size	b_hd_size
		//				t_offs
		//
		b_hd_size = (inf_buf->size_total >> 12) * sizeof(unsigned int);
		b_hd_total = (inf_buf->size_total >> 12) << 12;
		for (j = 0; j < DPVPP_BUF_NUB; j++) {
			cfg->afbc_crc[j] = 0;
			t_offs = 0;
			b_uhd_size = ((inf_a->size_buffer + 0xfff) >> 12) * sizeof(unsigned int);
			b_uhd_total = ((inf_a->size_buffer + 0xfff) >> 12) << 12;

			for (i = 0; i < 5; i++) {
				blk = hw->src_blk[i + j * 5];
				blk_m = &blk->c.b.blk_m;

				if (b_uhd_size > b_hd_size) {
					size_table = b_hd_size;
					size_buf = b_hd_total;
				} else {
					size_table = inf_a->size_table - t_offs;
					size_buf = b_uhd_total;
				}
				afbce_map.bodyadd	= blk_m->mem_start;
				afbce_map.tabadd	= cfg->addr_e[j] + t_offs;
				afbce_map.size_buf	= size_buf;
				afbce_map.size_tab	= size_table;

				cfg->afbc_crc[j] +=
					dim_afds()->int_tab(&devp->pdev->dev,
							    &afbce_map);
				t_offs += size_table;
				b_uhd_size -= size_table;
				b_uhd_total -= b_hd_total;
			}
		}
		cfg->support = true;

	} else {
		PR_ERR("%s:not support:%d:%d\n", __func__, idx, by);
	}
}

/* call when buffer is rd */
static void mem_hw_reg(void)
{
	struct dim_pvpp_hw_s *hw;

	hw = &get_datal()->dvs_prevpp.hw;
	dbg_plink1("%s\n", __func__);
	init_completion(&hw->pw_done);
	if (dim_is_pst_wr())
		hw->en_pst_wr_test	= true;
	else
		hw->en_pst_wr_test	= false; //true;//
	hw->intr_mode = 3;
}

static unsigned long dpvpp_dct_mem_alloc(unsigned int ch)
{
	struct dim_pvpp_hw_s *hw;
	unsigned long addr = 0;

	hw = &get_datal()->dvs_prevpp.hw;
	if (hw->src_blk_dct) {
		if (hw->dct_ch == 0xff) {
			addr = hw->src_blk_dct->c.b.blk_m.mem_start;
			hw->dct_ch = ch;
		} else {
			PR_WARN("%s:%d: dis_ch:%d; dct_ch:%d; dct_flg:%d; src_blk_dct:%px\n",
				__func__, ch,
				hw->dct_ch, hw->dis_ch,
				hw->dct_flg, hw->src_blk_dct);
		}
	}
	return addr;
}

static void dpvpp_dct_mem_release(unsigned int ch)
{
	struct dim_pvpp_hw_s *hw;

	hw = &get_datal()->dvs_prevpp.hw;
	if (hw->src_blk_dct && hw->dct_ch == ch)
		hw->dct_ch = 0xff;
	else
		PR_WARN("%s:%d: dis_ch:%d; dct_ch:%d; dct_flg:%d; src_blk_dct:%px\n",
			__func__, ch,
			hw->dct_ch, hw->dis_ch,
			hw->dct_flg, hw->src_blk_dct);
}

bool dpvpp_dct_mem_reg(struct di_ch_s *pch)
{
	ulong irq_flag = 0;
	unsigned long addr_dct;

	if (!pch) {
		PR_ERR("%s: no pch %px\n",
			__func__, pch);
		return false;
	}

	spin_lock_irqsave(&lock_pvpp, irq_flag);
	addr_dct = dpvpp_dct_mem_alloc(pch->ch_id);
	if (!addr_dct) {
		PR_ERR("%s: allocate dct mem fail\n", __func__);
		spin_unlock_irqrestore(&lock_pvpp, irq_flag);
		return false;
	}
	if (!dct_pre_plink_reg_mem(pch, addr_dct)) {
		dpvpp_dct_mem_release(pch->ch_id);
		PR_ERR("%s: ch[%d] plink_reg_mem fail\n",
			__func__, pch->ch_id);
		spin_unlock_irqrestore(&lock_pvpp, irq_flag);
		return false;
	}
	spin_unlock_irqrestore(&lock_pvpp, irq_flag);
	return true;
}

bool dpvpp_dct_mem_unreg(struct di_ch_s *pch)
{
	ulong irq_flag = 0;

	if (!pch) {
		PR_ERR("%s: no pch %px\n",
			__func__, pch);
		return false;
	}

	spin_lock_irqsave(&lock_pvpp, irq_flag);
	dpvpp_dct_mem_release(pch->ch_id);
	dct_pre_plink_unreg_mem(pch);
	spin_unlock_irqrestore(&lock_pvpp, irq_flag);
	return true;
}

/************************************************
 * dpvpp_dct_get_flg
 * return:
 *	true: have flag;
 * ch: back channel information;
 * data:
 *	1:reg decontour;
 *	2: unreg decontour;
 *	other: nothing;
 ************************************************/
bool dpvpp_dct_get_flg(unsigned char *ch, unsigned char *data)
{
	struct dim_pvpp_hw_s *hw;
	unsigned char ch_l, data_l;

	/* TODO: check if to add spin_lock_irqsave(&lock_pvpp, irq_flag) */
	hw = &get_datal()->dvs_prevpp.hw;
	if (hw->dct_flg) {
		if (hw->dct_flg == 2)
			ch_l = (unsigned char)hw->dct_ch;
		else
			ch_l = (unsigned char)hw->dis_ch;
		data_l = (unsigned char)hw->dct_flg;
		if (data_l == 1 && ch_l > DI_CHANNEL_NUB) {
			PR_ERR("%s:fix:%d,%d\n", __func__, data_l, ch_l);
			hw->dct_flg = 0;
			return false;
		}
		*ch = ch_l;
		*data = data_l;
		return true;
	}
	return false;
}

void dpvpp_dct_clear_flg(void)
{
	struct dim_pvpp_hw_s *hw;
	ulong irq_flag = 0;

	spin_lock_irqsave(&lock_pvpp, irq_flag);
	hw = &get_datal()->dvs_prevpp.hw;
	dbg_plink1("%s flg:%d dis_ch:%d dct_ch:%d\n",
		__func__, hw->dct_flg,
		hw->dis_ch, hw->dct_ch);
	hw->dct_flg = 0;
	spin_unlock_irqrestore(&lock_pvpp, irq_flag);
}

void dpvpp_dct_unreg(unsigned char ch)
{
	struct dim_pvpp_hw_s *hw;
	ulong irq_flag = 0;
	struct di_ch_s *pch;

	//for unreg
	hw = &get_datal()->dvs_prevpp.hw;
	if (hw->dct_ch != ch)
		return;
	spin_lock_irqsave(&lock_pvpp, irq_flag);
	pch = get_chdata(ch);
	dpvpp_dct_mem_release(ch);
	dct_pre_plink_unreg_mem(pch);
	if (hw->dct_flg) {
		PR_WARN("disable flg ch:%d flg:%d dis_ch:%d dct_ch:%d\n",
			ch, hw->dct_flg, hw->dis_ch, hw->dct_ch);
	} else {
		dbg_plink3("trig dct unreg 3 ch:%d dis_ch:%d dct_ch:%d\n",
			ch, hw->dis_ch, hw->dct_ch);
		hw->dis_ch = 0xff;
	}
	spin_unlock_irqrestore(&lock_pvpp, irq_flag);

	get_datal()->dct_op->unreg(pch);
}

static void dct_pst(const struct reg_acc *op, struct dimn_dvfm_s *ndvfm)
{
	struct vframe_s *vf;
	struct dim_pvpp_hw_s *hw;
	bool ret;
	struct di_ch_s *pch;
	struct dcntr_mem_s	*pdcn;
	unsigned char dct_bypass_sts = 0;

	hw = &get_datal()->dvs_prevpp.hw;
	if (hw->dis_ch == 0xff)
		return;
	if (!op || !ndvfm || !ndvfm->c.dct_pre)
		return;
	if (!ndvfm->itf) {
		PR_ERR("%s:no itf ndvfm:%px\n", __func__, ndvfm);
		return;
	}

	pch = get_chdata(ndvfm->itf->bind_ch);
	if (!pch) {
		PR_ERR("%s:no pch ndvfm->itf:%px bind_ch:%d\n",
			__func__, ndvfm->itf,
			ndvfm->itf->bind_ch);
		return;
	}
	if (!get_datal()->dct_op->mem_check(pch, ndvfm->c.dct_pre)) {
		PR_WARN("%s:ch[%d]:dct_pre not match: %px %px\n",
			__func__, ndvfm->itf->bind_ch,
			ndvfm->c.dct_pre, pch->dct_pre);
		return;
	}
	vf = &ndvfm->c.vf_in_cp;
	vf->decontour_pre = ndvfm->c.dct_pre;
	pdcn = (struct dcntr_mem_s *)vf->decontour_pre;
	/* set pre-link flag to notify the dct pst process*/
	if (pdcn)
		pdcn->plink = true;
	dcntr_check(vf);
	ret = dcntr_set(op);
	if (ret)
		hw->dct_sum_d++;
	if (pdcn && !dpvpp_dbg_force_no_crop()) {
		if (pdcn->x_start != hw->dis_c_para.win.x_st ||
		    pdcn->x_size != hw->dis_c_para.win.x_size ||
		    pdcn->y_start != hw->dis_c_para.win.y_st ||
		    pdcn->y_size != hw->dis_c_para.win.y_size) {
			dcntr_hw_bypass(op);
			dct_bypass_sts = 1;
			dim_print("%s: dct_bypass: (%d %d %d %d) -- (%d %d %d %d)\n",
				__func__,
				pdcn->x_start, pdcn->y_start,
				pdcn->x_size, pdcn->y_size,
				hw->dis_c_para.win.x_st,
				hw->dis_c_para.win.y_st,
				hw->dis_c_para.win.x_size,
				hw->dis_c_para.win.y_size);
		}
	}
	if (dpvpp_dbg_force_bypass_dct_post()) {
		dcntr_hw_bypass(op);
		dct_bypass_sts = 1;
	}
	if (dct_bypass_sts != hw->last_dct_bypass)
		dim_print("%s: dct_bypass:%d->%d, force:%d\n",
			__func__, hw->last_dct_bypass, dct_bypass_sts,
			dpvpp_dbg_force_bypass_dct_post());
	hw->last_dct_bypass = dct_bypass_sts;
	dcntr_pq_tune(&hw->pq_rpt, op);
	vf->decontour_pre = NULL;
}

static void dpvpp_mem_dct(void)
{
	struct dim_pvpp_hw_s *hw;
	unsigned int dct_idx;
	struct blk_s *blk;
	struct dd_s *d_dd;

	if (!get_datal())
		return;
	hw = &get_datal()->dvs_prevpp.hw;
	d_dd = get_dd();
	if (hw->m_mode_tgt != hw->m_mode ||
	    hw->flg_tvp_tgt != hw->flg_tvp ||
	    hw->m_mode == EPVPP_MEM_T_NONE)
		return;

	if (hw->en_dct && !hw->src_blk_dct) {
		if (hw->flg_tvp)
			dct_idx = EBLK_T_LINK_DCT_TVP;
		else
			dct_idx = EBLK_T_LINK_DCT;
		blk = blk_o_get(d_dd, dct_idx);
		if (blk) {
			hw->src_blk_dct = blk;
			dbg_plink1("dct mem get\n");
		}
	}
}

void dpvpp_secure_pre_en(bool is_tvp)
{
	if (is_tvp) {
		if (DIM_IS_IC_EF(SC2)) {
			DIM_DI_WR(DI_PRE_SEC_IN, 0x3F);//secure
		} else {
		#ifdef CONFIG_AMLOGIC_TEE
			tee_config_device_state(16, 1);
		#endif
		}
		if (DIM_IS_IC(S5)) {
		#ifdef CONFIG_AMLOGIC_TEE
			tee_write_reg_bits
				(((DI_VIUB_SECURE_REG << 2) + 0xff800000),
				 1, 8, 1);// HF secure Polarity
		#endif
		}
		//dbg_mem2("%s:tvp3 pre SECURE\n", __func__);
	} else {
		if (DIM_IS_IC_EF(SC2)) {
			DIM_DI_WR(DI_PRE_SEC_IN, 0x0);
		} else {
		#ifdef CONFIG_AMLOGIC_TEE
			tee_config_device_state(16, 0);
		#endif
		}
		if (DIM_IS_IC(S5)) {
		#ifdef CONFIG_AMLOGIC_TEE
			tee_write_reg_bits
				(((DI_VIUB_SECURE_REG << 2) + 0xff800000),
				 0, 8, 1);// HF secure Polarity
		#endif
		}
		//dbg_mem2("%s:tvp3 pre NOSECURE:\n", __func__);
	}
}

//static
void dpvpp_mng_set(void)
{
	struct dim_pvpp_hw_s *hw;
	int i, cnt;
	struct dd_s *d_dd;
	struct blk_s *blk;
	bool ret = false;
	bool is_tvp;
	unsigned int index;

	if (!get_datal())
		return;
	hw = &get_datal()->dvs_prevpp.hw;
	d_dd = get_dd();

	dim_print("%s:<%d,%d>, <%d,%d>\n", __func__,
		hw->m_mode_tgt, hw->flg_tvp_tgt,
		hw->m_mode, hw->flg_tvp);

	if (hw->m_mode_tgt == hw->m_mode &&
	    hw->flg_tvp_tgt == hw->flg_tvp) {
		dim_print("%s:<%d,%d>, <%d,%d>\n", __func__,
			hw->m_mode_tgt, hw->flg_tvp_tgt,
			hw->m_mode, hw->flg_tvp);
		return;
	}
	dpvpp_secure_pre_en(hw->flg_tvp_tgt);
	switch (hw->m_mode_tgt) {
	case EPVPP_MEM_T_HD_FULL:
		if (hw->flg_tvp_tgt) {
			is_tvp = true;
			index = EBLK_T_HD_FULL_TVP;
		} else {
			is_tvp = false;
			index = EBLK_T_HD_FULL;
		}
		hw->flg_tvp = hw->flg_tvp_tgt;
		cnt = 0;
		for (i = 0; i < 2; i++) {
			if (hw->src_blk[i]) {
				cnt++;
				continue;
			}
			blk = blk_o_get(d_dd, index);
			if (blk) {
				hw->src_blk[i] = blk;
				cnt++;
				dbg_plink2("get blk %d\n", cnt); //tmp
			}
		}
		if (cnt >= 2) {
			ret = true;
			/* buffer is ready*/
			mem_hw_reg();
			//mem_count_buf_cfg(1, EPVPP_BUF_CFG_T_HD_F);
			mem_count_buf_cfg2(1, EPVPP_BUF_CFG_T_HD_F,
					   EPVPP_MEM_T_HD_FULL);
			mem_count_buf_cfg2(1, EPVPP_BUF_CFG_T_HD_NV21,
					   EPVPP_MEM_T_HD_FULL);
			hw->blk_rd_hd = true;
		}
		break;
	case EPVPP_MEM_T_UHD_AFBCE:
		if (hw->flg_tvp_tgt) {
			is_tvp = true;
			index = EBLK_T_UHD_FULL_AFBCE_TVP;
		} else {
			is_tvp = false;
			index = EBLK_T_UHD_FULL_AFBCE;
		}
		hw->flg_tvp = hw->flg_tvp_tgt;
		cnt = 0;
		for (i = 0; i < 2; i++) {
			if (hw->src_blk_uhd_afbce[i]) {
				cnt++;
				continue;
			}
			blk = blk_o_get(d_dd, index);
			if (blk) {
				hw->src_blk_uhd_afbce[i] = blk;
				cnt++;
				dbg_plink2("uhd get blk %d\n", cnt);
			}
		}
		if (!hw->src_blk_e) {
			blk = blk_o_get(d_dd, EBLK_T_S_LINK_AFBC_T_FULL);
			if (blk) {
				cnt++;
				hw->src_blk_e = blk;
				dbg_plink2("uhd get table %d\n", cnt);
			}
		} else {
			cnt++;
		}
		if (cnt >= 3) {
			ret = true;

			mem_hw_reg();
			//mem_count_buf_cfg(1, EPVPP_BUF_CFG_T_HD_F);
			//mem_count_buf_cfg(1, EPVPP_BUF_CFG_T_UHD_F_AFBCE);
			mem_count_buf_cfg2(1, EPVPP_BUF_CFG_T_HD_F,
					   EPVPP_MEM_T_UHD_AFBCE);
			mem_count_buf_cfg2(1, EPVPP_BUF_CFG_T_UHD_F,
					   EPVPP_MEM_T_UHD_AFBCE);
			mem_count_buf_cfg2(1, EPVPP_BUF_CFG_T_UHD_F_AFBCE,
					   EPVPP_MEM_T_UHD_AFBCE);
			hw->blk_rd_uhd = true;
			hw->blk_rd_hd = true;
		}
		break;
	case EPVPP_MEM_T_UHD_AFBCE_HD:
		if (hw->flg_tvp_tgt) {
			is_tvp = true;
			index = EBLK_T_HD_FULL_TVP;
		} else {
			is_tvp = false;
			index = EBLK_T_HD_FULL;
		}
		hw->flg_tvp = hw->flg_tvp_tgt;
		cnt = 0;
		for (i = 0; i < 10; i++) {
			if (hw->src_blk[i]) {
				cnt++;
				continue;
			}
			blk = blk_o_get(d_dd, index);
			if (blk) {
				hw->src_blk[i] = blk;
				cnt++;
			}
		}
		if (!hw->src_blk_e) {
			blk = blk_o_get(d_dd, EBLK_T_S_LINK_AFBC_T_FULL);
			if (blk) {
				cnt++;
				hw->src_blk_e = blk;
			}
		} else {
			cnt++;
		}
		if (cnt >= 11) {
			ret = true;
			mem_hw_reg();
			//mem_count_buf_cfg(1, EPVPP_BUF_CFG_T_HD_F);
			mem_count_buf_cfg(1, EPVPP_BUF_CFG_T_UHD_F_AFBCE_HD);
			hw->blk_rd_uhd = true;
		}
		break;
	case EPVPP_MEM_T_NONE:
	default:
		hw->m_mode = hw->m_mode_tgt;
		break;
	}
	if (ret) {
		dbg_plink1("%s:set:%d\n", __func__, hw->m_mode_tgt);
		hw->m_mode = hw->m_mode_tgt;
		hw->flg_tvp	= hw->flg_tvp_tgt;
	}
}

//static
void dpvpp_mem_mng_get(unsigned int id)
{
	unsigned int ch;
	struct dim_pvpp_hw_s *hw;
	struct dimn_itf_s *itf;
	int i;
	struct dd_s *d_dd;
	unsigned int blk_idx, dct_idx;
	unsigned int cnt[K_MEM_T_NUB], cnt_tvp[K_MEM_T_NUB];
	unsigned char tgt_mode;
	bool	tgt_tvp, back_tvp;
	ulong irq_flag = 0;
	bool flg_release_err = false;
	unsigned int back_tgt;

	if (!get_datal())
		return;
	hw = &get_datal()->dvs_prevpp.hw;
	d_dd = get_dd();

	if (hw->blk_used_hd	||
	    hw->blk_used_uhd) { /*in using, don't change */
		dim_print("%s:used<%d,%d>\n", __func__,
			hw->blk_used_hd, hw->blk_rd_uhd);
		return;
	}
	back_tgt = hw->m_mode_tgt;
	back_tvp = hw->flg_tvp_tgt;
	memset(&cnt[0], 0, sizeof(cnt));
	memset(&cnt_tvp[0], 0, sizeof(cnt_tvp));
	dim_print("%s:<%d,%d>, <%d,%d>\n", __func__,
		hw->m_mode_tgt, hw->flg_tvp_tgt,
		hw->m_mode, hw->flg_tvp);

	for (ch = 0; ch < DI_PLINK_CN_NUB; ch++) {
		//count mode:
		itf = &get_datal()->dvs_prevpp.itf[ch];
		if (!itf) {
			PR_ERR("%s:%d:overflow\n", __func__, ch);
			break;
		}
		if (!atomic_read(&itf->reg))
			continue;
		if (itf->c.m_mode_n >= K_MEM_T_NUB) {
			PR_ERR("%s:%d:overflow:%d\n", __func__,
				ch, itf->c.m_mode_n);
			continue;
		}
		cnt[itf->c.m_mode_n]++;
		if (itf->c.is_tvp)
			cnt_tvp[itf->c.m_mode_n]++;
	}
	//PR_INF("%s:cnt:%d:%d:%d\n", __func__, cnt[1], cnt[2], cnt[3]);
	/*tmp use large buffer config */
	tgt_mode = EPVPP_MEM_T_NONE;
	tgt_tvp = false;
	for (i = K_MEM_T_NUB - 1; i > 0; i--) {
		if (cnt[i] > 0) {
			tgt_mode = i;
			if (cnt_tvp[i])
				tgt_tvp = true;
			else
				tgt_tvp = false;
			//PR_INF("%s:tgt_mode=%d:%d\n", __func__, tgt_mode, tgt_tvp);
			break;
		}
	}
	/* same */
	if (hw->m_mode_tgt == tgt_mode &&
	    hw->flg_tvp_tgt == tgt_tvp) {
		return;
	}

	/* release first */
	if (hw->m_mode_tgt) {
		spin_lock_irqsave(&lock_pvpp, irq_flag);
		if (hw->blk_used_hd || hw->blk_used_uhd) {
			flg_release_err = true;
		} else {
			flg_release_err = false;
			hw->blk_rd_uhd = false;
			hw->blk_rd_hd = false;
		}
		spin_unlock_irqrestore(&lock_pvpp, irq_flag);
		if (flg_release_err) {
			PR_ERR("buffer used\n");
			return;
		}
		/* check dct */
		if (tgt_tvp != hw->flg_tvp_tgt &&
		    hw->src_blk_dct) {
			if (hw->flg_tvp_tgt)
				dct_idx = EBLK_T_LINK_DCT_TVP;
			else
				dct_idx = EBLK_T_LINK_DCT;
			blk_o_put(d_dd, dct_idx, hw->src_blk_dct);
			hw->src_blk_dct = NULL;
		}
		/* src_blk_uhd_afbce */
		if (hw->flg_tvp_tgt)
			blk_idx = EBLK_T_UHD_FULL_AFBCE_TVP;
		else
			blk_idx = EBLK_T_UHD_FULL_AFBCE;
		for (i = 0; i < DPVPP_BLK_NUB_MAX; i++) {
			if (hw->src_blk_uhd_afbce[i]) {
				blk_o_put(d_dd, blk_idx, hw->src_blk_uhd_afbce[i]);
				hw->src_blk_uhd_afbce[i] = NULL;
			}
		}
		/* src_blk */
		if (hw->flg_tvp_tgt)
			blk_idx = EBLK_T_HD_FULL_TVP;
		else
			blk_idx = EBLK_T_HD_FULL;
		for (i = 0; i < DPVPP_BLK_NUB_MAX; i++) {
			if (hw->src_blk[i]) {
				blk_o_put(d_dd, blk_idx, hw->src_blk[i]);
				hw->src_blk[i] = NULL;
			}
		}
	}
	/* alloc */
	if (tgt_tvp)
		dct_idx = EBLK_T_LINK_DCT_TVP;
	else
		dct_idx = EBLK_T_LINK_DCT;
	if (tgt_mode == EPVPP_MEM_T_UHD_AFBCE_HD) {
		if (tgt_tvp)
			blk_idx = EBLK_T_HD_FULL_TVP;

		else
			blk_idx = EBLK_T_HD_FULL;

		memset(&hw->blkt_n[0], 0, sizeof(hw->blkt_n));

		hw->blkt_n[blk_idx] = 10;
		hw->blkt_n[EBLK_T_S_LINK_AFBC_T_FULL] = 1;
		if (hw->en_dct)
			hw->blkt_n[dct_idx] = 1;
		m_a_blkt_cp(0, &hw->blkt_n[0]);
		hw->m_mode_tgt = EPVPP_MEM_T_UHD_AFBCE_HD;
		hw->flg_tvp_tgt = tgt_tvp;
	} else if (tgt_mode == EPVPP_MEM_T_UHD_AFBCE) {
		if (tgt_tvp)
			blk_idx = EBLK_T_UHD_FULL_AFBCE_TVP;
		else
			blk_idx = EBLK_T_UHD_FULL_AFBCE;

		memset(&hw->blkt_n[0], 0, sizeof(hw->blkt_n));

		hw->blkt_n[blk_idx] = 2;
		hw->blkt_n[EBLK_T_S_LINK_AFBC_T_FULL] = 1;
		if (hw->en_dct)
			hw->blkt_n[dct_idx] = 1;
		m_a_blkt_cp(0, &hw->blkt_n[0]);
		hw->m_mode_tgt = tgt_mode;
		hw->flg_tvp_tgt = tgt_tvp;
	} else if (tgt_mode == EPVPP_MEM_T_UHD_FULL) {
		if (tgt_tvp)
			blk_idx = EBLK_T_UHD_FULL_TVP;
		else
			blk_idx = EBLK_T_UHD_FULL;

		/* check afbce table */
		if (hw->src_blk_e) {
			blk_o_put(d_dd, EBLK_T_S_LINK_AFBC_T_FULL, hw->src_blk_e);
			hw->src_blk_e = NULL;
		}
		memset(&hw->blkt_n[0], 0, sizeof(hw->blkt_n));

		hw->blkt_n[blk_idx] = 2;
		if (hw->en_dct)
			hw->blkt_n[dct_idx] = 1;
		m_a_blkt_cp(0, &hw->blkt_n[0]);
		hw->m_mode_tgt = tgt_mode;
		hw->flg_tvp_tgt = tgt_tvp;
	} else if (tgt_mode == EPVPP_MEM_T_HD_FULL) {
		if (tgt_tvp)
			blk_idx = EBLK_T_HD_FULL_TVP;
		else
			blk_idx = EBLK_T_HD_FULL;

		/* check afbce table */
		if (hw->src_blk_e) {
			blk_o_put(d_dd, EBLK_T_S_LINK_AFBC_T_FULL, hw->src_blk_e);
			hw->src_blk_e = NULL;
		}
		memset(&hw->blkt_n[0], 0, sizeof(hw->blkt_n));
		hw->blkt_n[blk_idx] = 2;
		if (hw->en_dct)
			hw->blkt_n[dct_idx] = 1;
		m_a_blkt_cp(0, &hw->blkt_n[0]);
		hw->m_mode_tgt = EPVPP_MEM_T_HD_FULL;
		hw->flg_tvp_tgt = tgt_tvp;
	} else if (tgt_mode == EPVPP_MEM_T_NONE) {
		/* check afbce table */
		if (hw->src_blk_e) {
			blk_o_put(d_dd, EBLK_T_S_LINK_AFBC_T_FULL, hw->src_blk_e);
			hw->src_blk_e = NULL;
		}
		/**/
		if (hw->src_blk_dct) {
			if (hw->flg_tvp_tgt)
				dct_idx = EBLK_T_LINK_DCT_TVP;
			else
				dct_idx = EBLK_T_LINK_DCT;
			blk_o_put(d_dd, dct_idx, hw->src_blk_dct);
			hw->src_blk_dct = NULL;
		}
		mem_count_buf_cfg(0xff, 0);
		memset(&hw->blkt_n[0], 0, sizeof(hw->blkt_n));
		m_a_blkt_cp(0, &hw->blkt_n[0]);
		hw->m_mode_tgt = EPVPP_MEM_T_NONE;
		hw->m_mode	= hw->m_mode_tgt;
		hw->flg_tvp	= false;
		hw->flg_tvp_tgt	= false;
	} else {
		PR_WARN("%s:tgt_mode[%d]\n", __func__, tgt_mode);
	}

	if (back_tgt != hw->m_mode_tgt || back_tvp != hw->flg_tvp_tgt)
		dbg_plink1("%s:<%d %d>-><%d %d>:%d\n", __func__,
			back_tgt, back_tvp,
			hw->m_mode_tgt, hw->flg_tvp_tgt,
			id);
}

#ifdef HIS_CODE
//static
bool dpvpp_reg_2(struct dimn_itf_s *itf)
{
	struct dim_dvs_prevpp_s *dv_prevpp;
	struct div2_mm_s *mm;
	int i;
	unsigned int offs, out_fmt;
	struct dvfm_s *dvfm_dm;
	struct dim_prevpp_ds_s *ds;

	struct dim_pvpp_hw_s *hw;
	struct afbce_map_s afbce_map;
	struct di_dev_s *devp = get_dim_de_devp();

	hw = &get_datal()->dvs_prevpp.hw;

	if (!itf)
		return false;
	if (!itf->ds)
		return false;
	ds = itf->ds;
	dv_prevpp = &get_datal()->dvs_prevpp;
	//itf	= &dv_prevpp->itf;
	/* double check */
	if (itf->c.src_state & EDVPP_SRC_NEED_MSK_CRT)
		return false;
	if (!(itf->c.src_state & EDVPP_SRC_NEED_REG2) ||
	    !itf->c.sum_pre_get)
		return false;

	init_completion(&hw->pw_done);

	/* afbc temp */
	get_datal()->di_afd.top_cfg_pre =
		&ds->pre_top_cfg;

	//ds	= dv_prevpp->ds;
	mm	= &ds->mm;

	/* cfg top copy */
	di_cfg_cp_ch(&ds->cfg_cp[0]);
	/* cfg temp */
	ds->cma_mode = 4;
	if (cfgg(4K))
		ds->en_4k = true;
	else
		ds->en_4k = false;
	ds->out_mode		= EDPST_MODE_422_10BIT_PACK;
	if (cfgg(POUT_FMT) == 3) //tmp
		ds->en_out_afbce	= true;
	else
		ds->en_out_afbce	= false;
	ds->out_afbce_nv2110	= false;
	ds->en_out_sctmem	= false;
	ds->en_hf_buf		= false;
	ds->en_dw		= false;
	//move to probe hw->en_linear		= false;
	if (dim_is_pst_wr())
		hw->en_pst_wr_test	= true;
	else
		hw->en_pst_wr_test	= false; //true;//
	ds->en_dbg_off_nr = dim_dbg_is_disable_nr();

	ds->op			= &dpvpp_regset; //&di_pre_regset; //
	//PR_INF("%s:ds->op:0x%px,0x%px\n", __func__, ds->op, ds->op->bwr);
	ds->mif_in.name	= "in";
	ds->mif_mem.name	= "mem";
	ds->mif_wr.name		= "wr";
#ifdef HIS_CODE	//this is pre-vpp link fix is 3
	if (dim_is_lprepst()) //need add to reg:temp
		hw->intr_mode = 1;
	else
#endif
		hw->intr_mode = 3;

	/* cfg */
	//if (cfgg(LINEAR))
	//move	hw->en_linear = true;
	if (ds->en_4k) {
		mm->cfg.di_h		= 2160;
		mm->cfg.di_w		= 3840;
	} else {
		mm->cfg.di_h		= 1088;
		mm->cfg.di_w		= 1920;
	}
	mm->cfg.num_local	= 0;
	mm->cfg.num_post	= DPVPP_BUF_NUB;

	/* cfg temp end */
	/* clear src from cfg */
	if (!ds->en_out_afbce) {
		itf->c.src_state &= ~(EDVPP_SRC_NEED_SCT_TAB);
	}
	/* cnt post mem size */
	if (dim_afds())
		dim_afds()->pvpp_reg_val(itf);

	dpvpp_cnt_pst_afbct(itf);
	dpvpp_cnt_post_buf(itf);
	dpvpp_pst_sec_alloc(itf, mm->cfg.dat_pafbct_flg.d32);
	dpvpp_reg_alloc_mem(itf);
	offs = 0;
	if (hw->blk_buf.flg_alloc && !ds->en_out_afbce) {//---------------------
		hw->dbuf_hsize = mm->cfg.di_w;
		hw->dbuf_plan_nub = 1;
		if (ds->out_mode == EDPST_MODE_NV21_8BIT)
			hw->dbuf_plan_nub = 2;
		for (i = 0; i < DPVPP_BUF_NUB; i++) {
			hw->dbuf_wr[i][0].phy_addr =
				hw->blk_buf.mem_start + offs;
			hw->dbuf_wr[i][0].width		= mm->cfg.pst_cvs_w;
			hw->dbuf_wr[i][0].height	= mm->cfg.pst_cvs_h;
			if (hw->dbuf_plan_nub >= 2) {
				hw->dbuf_wr[i][1].phy_addr =
					hw->dbuf_wr[i][0].phy_addr +
					mm->cfg.pst_buf_y_size;
				hw->dbuf_wr[i][1].width	= mm->cfg.pst_cvs_w;
				hw->dbuf_wr[i][1].height = mm->cfg.pst_cvs_h;
			}
			offs += mm->cfg.size_post;
			dbg_mem2("addr:[%d]:0x%lx:\n", i,
				hw->dbuf_wr[i][0].phy_addr);
		}
	} else if (hw->blk_buf.flg_alloc && ds->en_out_afbce) {
		hw->dbuf_hsize = mm->cfg.di_w;
		hw->dbuf_plan_nub = 1;
		/* general*/
		for (i = 0; i < DPVPP_BUF_NUB; i++) {
			hw->addr_i[i] = hw->blk_buf.mem_start + offs;
			offs += mm->cfg.pst_afbci_size;
			hw->dbuf_wr[i][0].phy_addr =
				hw->blk_buf.mem_start + offs;
			hw->dbuf_wr[i][0].width		= mm->cfg.pst_cvs_w;
			hw->dbuf_wr[i][0].height	= mm->cfg.pst_cvs_h;

			offs += mm->cfg.size_post;
			PR_INF("addr:[%d]:0x%lx:0x%lx:\n", i,
				hw->addr_i[i],
				hw->dbuf_wr[i][0].phy_addr);
		}
		/**/
		for (i = 0; i < DPVPP_BUF_NUB; i++) {
			hw->addr_e[i] = hw->dat_p_afbct.addr_st +
				i * mm->cfg.size_pafbct_one;
			afbce_map.bodyadd	= hw->dbuf_wr[i][0].phy_addr;
			afbce_map.tabadd	= hw->addr_e[i];
			afbce_map.size_buf	= mm->cfg.pst_buf_size;
			afbce_map.size_tab	= mm->cfg.size_pafbct_one;

			hw->afbc_crc[i] =
				dim_afds()->int_tab(&devp->pdev->dev,
						    &afbce_map);
			PR_INF("%d:addr:0x%lx,afbct:0x%lx\n", i,
				hw->dbuf_wr[i][0].phy_addr,
				hw->addr_e[i]);
		}
	}

	/* dvfm_demo: type and bitdepth output not change */
	dvfm_dm	= &ds->out_dvfm_demo;
	//memset(out_dvfm, 0, sizeof(*out_dvfm));
	if (ds->out_mode == EDPST_MODE_NV21_8BIT) {
		if (ds->en_out_nv12)
			out_fmt = 2;
		else
			out_fmt = 1;
	} else/* if (ds->out_mode == EDPST_MODE_422_10BIT_PACK)*/ {
		out_fmt = 0;
	}
	dbg_plink1("%s:id[%d]:out_fmt[%d]\n", __func__, itf->id, out_fmt);
	dim_dvf_type_p_change(dvfm_dm, out_fmt);
	if (ds->en_out_afbce) {
		dvfm_dm->flg_afbce_set = true;
		dvfm_dm->vfs.type |= VIDTYPE_COMPRESS; //tmp
		dvfm_dm->vfs.type |= VIDTYPE_SCATTER;
	} else {
		dvfm_dm->flg_afbce_set = false;
	}
	dvfm_dm->vfs.plane_num = hw->dbuf_plan_nub;
	dvfm_dm->buf_hsize = hw->dbuf_hsize;
	memcpy(&dvfm_dm->vfs.canvas0_config[0],
	       &hw->dbuf_wr[0][0], sizeof(dvfm_dm->vfs.canvas0_config[0]) << 1);
	if (itf->etype == EDIM_NIN_TYPE_VFM)
		dvfm_dm->is_itf_vfm = 1;
	if (hw->en_pst_wr_test)
		dvfm_dm->is_p_pw	= 1;
	else
		dvfm_dm->is_prvpp_link	= 1;
	/* dvfm_demo end ********************************/

//no use	dpvpp_dbg_print = false;
	if (dpvpp_dbg_force_vpp_active())
		dpvpp_link_sw_api(true);

	itf->c.src_state &= (~EDVPP_SRC_NEED_REG2);
	get_datal()->pre_vpp_exist = true;
	dbg_plink1("%s:ok:src_state[0x%x],pst_wr[%d], afbce[%d]\n",
		__func__,
		itf->c.src_state,
		hw->en_pst_wr_test,
		ds->en_out_afbce);

	return true;
}
#endif
static int dpvpp_reg_link_sw(bool vpp_disable_async)
{
	unsigned int ton, ton_vpp, ton_di;
	struct platform_device *pdev;
	struct dim_pvpp_hw_s *hw;

//	if (itf && itf->c.src_state & EDVPP_SRC_NEED_REG2)
//		return 0;

	hw = &get_datal()->dvs_prevpp.hw;
	//check:
	ton_vpp = atomic_read(&hw->link_on_byvpp);
	ton_di = atomic_read(&hw->link_on_bydi);
	if (ton_vpp == ton_di)
		ton = ton_vpp;
	else
		ton = 0;

	if (ton == atomic_read(&hw->link_sts)) {
		if (atomic_read(&hw->link_in_busy))
			PR_ERR("%s: error state: check:<ton_vpp %d, ton_di %d> hw->link_sts:%d\n",
				__func__, ton_vpp, ton_di, atomic_read(&hw->link_sts));
		return 0;
	}

	if (ton) {
		//bypass other ch:
		get_datal()->pre_vpp_active = true;/* interface */
		hw->has_notify_vpp = false;
		if (dim_check_exit_process()) {
		//can sw:
			di_unreg_setting(true);
			if (dpvpp_set_one_time()) {
				pdev = get_dim_de_devp()->pdev;
				if (!pdev)
					return false;
				/* hw set */
				dpvpph_reg_setting(hw->op_n);
				/* reset to avoid flicker on T5D VB afbcd */
				dpvpph_pre_frame_reset_g12(true, hw->op_n);
			}

			atomic_set(&hw->link_sts, ton);//on
			get_datal()->pre_vpp_set = true; /* interface */
			atomic_set(&hw->link_in_busy, 0);
			ext_vpp_prelink_state_changed_notify();
			PR_INF("%s:set active:<%d, %d> op:%px\n",
			       __func__, ton_vpp, ton_di, hw->op_n);
		}
	} else {
		bool sw_done = false;

		//off:
		/*check if bypass*/
		if (!ton_di && !hw->has_notify_vpp) {
			/* non-block */
			ext_vpp_disable_prelink_notify(vpp_disable_async);
			ext_vpp_prelink_real_sw(false, false);
			hw->has_notify_vpp = true;
			sw_done = true;
		}
		if (hw->dis_last_para.dmode != EPVPP_DISPLAY_MODE_BYPASS) {
			PR_INF("wait for bypass\n");
			return 0;
		}
		if (!sw_done)
			ext_vpp_prelink_real_sw(false, true);
		atomic_set(&hw->link_sts, 0);//on
		get_datal()->pre_vpp_active = false;/* interface */
		atomic_set(&hw->link_in_busy, 0);
		ext_vpp_prelink_state_changed_notify();
		PR_INF("%s:set inactive<%d, %d>\n", __func__, ton_vpp, ton_di);
	}
	return 0;
}

static int dpvpp_link_sw_api(bool sw)
{
	struct dim_pvpp_hw_s *hw;
	int cur_sts = 0;

	hw = &get_datal()->dvs_prevpp.hw;

	cur_sts = atomic_read(&hw->link_on_byvpp);
	if (sw)
		atomic_set(&hw->link_on_byvpp, 1);
	else
		atomic_set(&hw->link_on_byvpp, 0);

	if (!sw && cur_sts && atomic_read(&hw->link_sts))
		atomic_set(&hw->link_in_busy, 1);

	dim_print("%s: link_on_byvpp:%d link_on_bydi:%d link_in_busy:%d\n",
		__func__, sw,
		atomic_read(&hw->link_on_bydi),
		atomic_read(&hw->link_in_busy));
	return true;
}

static int dpvpp_link_sw_by_di(bool sw)
{
	struct dim_pvpp_hw_s *hw;

	hw = &get_datal()->dvs_prevpp.hw;
	if (sw) {
		if (!atomic_read(&hw->link_on_bydi)) {
			atomic_set(&hw->link_on_bydi, 1);
			ext_vpp_prelink_state_changed_notify();
			dim_print("%s:%d\n", __func__, sw);
		}
	} else {
		if (atomic_read(&hw->link_on_bydi)) {
			atomic_set(&hw->link_on_bydi, 0);
			dim_print("%s:%d\n", __func__, sw);
		}
	}

	return true;
}

static bool dpvpp_que_sum(struct dimn_itf_s *itf)
{
	struct dimn_qs_cls_s *q;
	struct dim_prevpp_ds_s *ds;

	if (!itf->ds)
		return false;
	ds = itf->ds;
	q = &ds->lk_que_idle;
	ds->sum_que_idle = q->ops.count(q);
	q = &ds->lk_que_in;
	ds->sum_que_in = q->ops.count(q);
	q = &ds->lk_que_ready;
	ds->sum_que_ready = q->ops.count(q);
	q = &ds->lk_que_recycle;
	ds->sum_que_recycle = q->ops.count(q);
	q = &ds->lk_que_kback;
	ds->sum_que_kback = q->ops.count(q);
	return true;
}

/*count 4k input */
static void count_4k(struct dimn_itf_s *itf, struct dvfm_s *vf)
{
	unsigned char mode_back; //debug only
	struct dim_prevpp_ds_s *ds;

	if (!itf || !vf || VFMT_IS_EOS(vf->vfs.type) || !itf->ds)
		return;

	if (vf->is_4k)
		itf->c.sum_4k++;

	/*to-do : for more format */
	mode_back = itf->c.m_mode_n;//debug only
	if (itf->c.m_mode_n != EPVPP_MEM_T_UHD_AFBCE) {
		ds = itf->ds;
		if (itf->c.sum_4k && itf->ds->en_4k)
			itf->c.m_mode_n = ds->m_mode_n_uhd;
		else
			itf->c.m_mode_n = ds->m_mode_n_hd;
	}

	if (mode_back != itf->c.m_mode_n) {
		dbg_plink1("ch[%d]:mode:%d->%d\n",
			itf->bind_ch, mode_back, itf->c.m_mode_n);
		//dpvpp_mem_mng_get(1);//
	}
}

/* cnt: for */
static bool dpvpp_parser_bypass(struct dimn_itf_s *itf,
				unsigned int cnt)
{
	struct dimn_qs_cls_s *qin, *qready, *qidle;
	struct dimn_dvfm_s *dvfm = NULL;
	struct vframe_s *vf, *vf_ori;
	struct di_buffer *buffer_ori;
	unsigned int cnt_in;
	int i;
	struct dim_prevpp_ds_s *ds;

	if (!itf || !itf->ds)
		return false;
	ds = itf->ds;

	qin	= &ds->lk_que_in;
	qready	= &ds->lk_que_ready;
	qidle	= &ds->lk_que_idle;

	cnt_in = qin->ops.count(qin);
	if (!cnt_in || qready->ops.is_full(qready))
		return false;
	if (cnt > 0)
		cnt_in = cnt;
	for (i = 0; i < cnt_in; i++) {
		vf = qin->ops.get(qin);
		if (!vf || !vf->private_data) {
			PR_ERR("%s:qin get null?\n", __func__);
			break;
		}

		dvfm = (struct dimn_dvfm_s *)vf->private_data;
		if (itf->etype == EDIM_NIN_TYPE_VFM) {
			vf_ori = (struct vframe_s *)dvfm->c.ori_in;
			vf_ori->di_flag |= DI_FLAG_DI_BYPASS;
			vf_ori->private_data = NULL;

			qidle->ops.put(qidle, vf);
			qready->ops.put(qready, vf_ori);
			if (qready->ops.is_full(qready))
				break;
		} else {
			/*EDIM_NIN_TYPE_INS*/
			buffer_ori = (struct di_buffer *)dvfm->c.ori_in;
			buffer_ori->flag |= DI_FLAG_BUF_BY_PASS;

			qidle->ops.put(qidle, vf);
			qready->ops.put(qready, buffer_ori);
			if (qready->ops.is_full(qready))
				break;
		}
	}
	return true;
}

static bool vf_m_in(struct dimn_itf_s *itf);

static bool recycle_in(struct dimn_itf_s *itf, ud buf)
{
	struct vframe_s *vf;
	struct dimn_dvfm_s *ndvfm;
	unsigned int err = 0;
	struct dim_prevpp_ds_s *ds;
	struct dim_pvpp_hw_s *hw;
	struct di_ch_s *pch;

	if (!itf || !itf->ds) {
		PR_ERR("%s:1\n", __func__);
		return false;
	}
	ds = itf->ds;

	vf = (struct vframe_s *)buf;
	dbg_check_vf(ds, vf, 3);
	ndvfm = (struct dimn_dvfm_s *)vf->private_data;
	if (!ndvfm) {
		err = 1;
		goto RECYCLE_IN_F;
	}
	hw = &get_datal()->dvs_prevpp.hw;
	if (hw->dct_ch != 0xff && ndvfm->c.dct_pre) {
		pch = get_chdata(itf->bind_ch);
		if (hw->dct_ch == itf->bind_ch &&
		    pch && pch->dct_pre) {
			if (get_datal()->dct_op->mem_check(pch, ndvfm->c.dct_pre))
				get_datal()->dct_op->mem_put_free(ndvfm->c.dct_pre);
			else
				PR_WARN("%s:dct_pre not match %px %px\n",
					__func__, ndvfm->c.dct_pre, pch->dct_pre);
		} else {
			PR_WARN("%s:ch or NULL:%d %d:(%d %d) %px %px %px %px(%px) %s\n",
				__func__,
				itf->id, itf->bind_ch,
				hw->dct_ch, hw->dis_ch,
				ndvfm->c.dct_pre, ndvfm, vf,
				pch, pch ? pch->dct_pre : NULL,
				current->comm);
		}
		ndvfm->c.dct_pre = NULL;
		ds->dct_sum_o++;
	}
	if (ndvfm->c.ori_in) {
		if (itf->etype == EDIM_NIN_TYPE_VFM)
			in_vf_put(itf, ndvfm->c.ori_in);
		else
			in_buffer_put(itf, ndvfm->c.ori_in);
		dim_print("%s:vf put\n", __func__);
		ndvfm->c.ori_in = NULL;
		return true;
	}
RECYCLE_IN_F:

	PR_ERR("%s:err[%d]\n", __func__, err);

	return false;
}

static void recycle_back(struct dimn_itf_s *itf)
{
	struct dimn_qs_cls_s *qback, *qidle;
	unsigned int cnt, cnt_out, cnt_in;
	ud buf[DIM_LKIN_NUB];
	int i;
	struct dim_prevpp_ds_s *ds;

	if (!itf || !itf->ds) {
		PR_ERR("%s:NULL handle:%px\n", __func__, itf);
		return;
	}
	ds = itf->ds;
	qback = &ds->lk_que_kback;
	if (qback->ops.is_empty(qback)) {
		dbg_plink3("%s:lk_que_kback empty\n", __func__);
		return;
	}
	qidle	= &ds->lk_que_idle;
	cnt = qback->ops.count(qback);
	cnt_out = qback->ops.out(qback, &buf[0], cnt);
	dbg_check_ud(ds, 3); /* dbg */
	for (i = 0; i < cnt_out; i++)
		recycle_in(itf, buf[i]);

	cnt_in = qidle->ops.in(qidle, &buf[0], cnt_out);

	if (cnt != cnt_out || cnt != cnt_in)
		PR_ERR("%s:err:cnt[%d:%d:%d]\n", __func__, cnt, cnt_out, cnt_in);
	dbg_plink3("%s: END cnt[%d:%d:%d]\n", __func__, cnt, cnt_out, cnt_in);
}

static bool dpvpp_parser(struct dimn_itf_s *itf)
{
	bool is_bypass = false;

	/* reg 2 */
	if (!itf ||
	    !itf->c.reg_di || !itf->ds || itf->c.pause_parser)
		return false;

	if (itf->c.src_state & EDVPP_SRC_NEED_MSK_CRT)
		return false;

	/* input polling */
	if (itf->etype == EDIM_NIN_TYPE_VFM)
		vf_m_in(itf);
	else
		dpvpp_patch_first_buffer(itf);

	if (!pre_dbg_is_run()) //p pause
		return true;

	if ((itf->c.src_state & EDVPP_SRC_NEED_MSK_BYPASS) ||
	   dpvpp_dbg_force_bypass_2()) {
		is_bypass = true;
		if (itf->c.src_state & EDVPP_SRC_NEED_MSK_BYPASS)
			itf->c.bypass_reason = EPVPP_NO_MEM;
		dpvpp_parser_bypass(itf, 0);
		dim_print("%s:SRC_NEED_MSK_BYPASS:0x%x\n", __func__, itf->c.src_state);
		return true;
	}

	recycle_back(itf);

	/* */
	dpvpp_parser_nr(itf);

	return true;
}

static bool dpvpp_process(void *para)
{
	struct dimn_itf_s *itf;
	int ch;
	unsigned int cnt_active = 0;
	unsigned int freeze = 0;

	for (ch = 0; ch < DI_PLINK_CN_NUB; ch++) {
		itf = &get_datal()->dvs_prevpp.itf[ch];
		dpvpp_mem_reg_ch(itf);//move dpvpp_reg_2(itf);

		dpvpp_parser(itf);

		if (!itf->ds || !atomic_read(&itf->reg))
			continue;
		cnt_active++;
		if (itf->flg_freeze)
			freeze++;
		dpvpp_ins_fill_out(itf);

		if (!(itf->c.src_state &
		      (EDVPP_SRC_NEED_MSK_CRT | EDVPP_SRC_NEED_MSK_BYPASS))) {
			if (dpvpp_que_sum(itf)) {
				if (itf->ds && itf->ds->sum_que_in)
					task_send_wk_timer(EDIM_WKUP_REASON_IN_L);
			}
		}
	}

	if (dpvpp_is_not_active_di() || dim_polic_is_prvpp_bypass())
		dpvpp_link_sw_by_di(false);
	else if (!freeze && cnt_active)
		dpvpp_link_sw_by_di(true);
	else if (!cnt_active)
		dpvpp_link_sw_by_di(false);
	dpvpp_reg_link_sw(true);
	dpvpp_mem_mng_get(2);
	dpvpp_mng_set();

	dpvpp_mem_dct();
	return true;
}

static const struct dimn_pvpp_ops_s dvpp_m_ops = {
	.reg = dpvpp_reg,
	.unreg = dpvpp_unreg_val,
	.parser = dpvpp_process,
	//.display = dpvpp_display,
};

static int dpvpp_check_dvfm_addr(struct dim_prevpp_ds_s *ds, struct vframe_s *vf)
{
	ud ud_dvfm, ud_check;

	/* check addr */
	if (!ds || !vf || !vf->private_data)
		return -1;
	if (vf->index >= DIM_LKIN_NUB) {
		PR_ERR("vf:no act:overflow:0x%px\n", vf);
		return -2;
	}
	ud_dvfm = (ud)vf->private_data;
	ud_check = (ud)(&ds->lk_in_bf[vf->index]);
	if (ud_dvfm != ud_check)
		return -3;
	return 1;
}

enum EDIM_DVPP_DIFF {
	EDIM_DVPP_DIFF_NONE = 0,
	EDIM_DVPP_DIFF_SAME_FRAME = DI_BIT1,  /* 0 ~ 3 for not change */
	EDIM_DVPP_DIFF_MEM	= DI_BIT2,

	EDIM_DVPP_DIFF_ALL = DI_BIT4,
	EDIM_DVPP_DIFF_SIZE_IN	= DI_BIT5,
	EDIM_DVPP_DIFF_ONLY_ADDR = DI_BIT6,
	EDIM_DVPP_DIFF_ONLY_NO_WR = DI_BIT7,
};

/* enum EDIM_DVPP_DIFF */
static unsigned int check_diff(struct dimn_itf_s *itf,
		struct dimn_dvfm_s *dvfm_c,
		struct pvpp_dis_para_in_s *in_para)
{
	struct pvpp_dis_para_in_s *pa_l, *pa_c;
	struct di_win_s *win_l, *win_c;
	struct dsub_vf_s *dvfml, *dvfmc;
	unsigned int state = 0; //EDIM_DVPP_DIFF
	struct dim_pvpp_hw_s *hw;
	struct dim_prevpp_ds_s *ds;

	if (!itf || !itf->ds) {
		PR_ERR("%s:1\n", __func__);
		return EDIM_DVPP_DIFF_NONE;
	}
	hw = &get_datal()->dvs_prevpp.hw;
	ds = itf->ds;

	if (!ds || !dvfm_c || !in_para || !itf)
		return EDIM_DVPP_DIFF_NONE;

	dvfmc = &dvfm_c->c.in_dvfm.vfs;
	/* copy current para */
	pa_c = &hw->dis_c_para;
	hw->id_c = itf->id;

	memcpy(pa_c, in_para, sizeof(*pa_c));
	win_c = &pa_c->win;
	win_c->x_end = win_c->x_st + win_c->x_size - 1;
	win_c->y_end = win_c->y_st + win_c->y_size - 1;
	win_c->orig_w = dvfmc->width;
	win_c->orig_h = dvfmc->height;
	win_c->x_check_sum = ((win_c->x_size & 0xffff) << 16) |
		((win_c->x_st + win_c->x_end) & 0xffff);
	win_c->y_check_sum = ((win_c->y_size & 0xffff) << 16) |
		((win_c->y_st + win_c->y_end) & 0xffff);

	if (dpvpp_dbg_force_same())
		return EDIM_DVPP_DIFF_SAME_FRAME;
	if (!hw->dis_last_dvf) {
		atomic_add(DI_BIT10, &itf->c.dbg_display_sts);	/* dbg only */
		return EDIM_DVPP_DIFF_ALL;
	}
	if (dpvpp_dbg_trig_change_all_one()) {
		atomic_add(DI_BIT11, &itf->c.dbg_display_sts);	/* dbg only */
		return EDIM_DVPP_DIFF_ALL;
	}

	if (hw->id_c != hw->id_l) {
		PR_INF("diff:change id:%d:%d\n", hw->id_l, hw->id_c);
		return EDIM_DVPP_DIFF_ALL;
	}
	pa_l = &hw->dis_last_para;
	win_l = &pa_l->win;
	if (pa_c->dmode != pa_l->dmode		||
	    win_c->x_size != win_l->x_size	||
	    win_c->y_size != win_l->y_size	||
	    win_c->x_st != win_l->x_st	||
	    win_c->y_st != win_l->y_st) {
		atomic_add(DI_BIT12, &itf->c.dbg_display_sts);	/* dbg only */
		PR_INF("diff:size:%d:<%d,%d,%d,%d,%d>,<%d,%d,%d,%d,%d>\n",
			dvfm_c->c.cnt_in,
			pa_l->dmode,
			win_l->x_size,
			win_l->y_size,
			win_l->x_st,
			win_l->y_st,
			pa_c->dmode,
			win_c->x_size,
			win_c->y_size,
			win_c->x_st,
			win_c->y_st);
		return EDIM_DVPP_DIFF_ALL;
	}
//tmp a
	if ((ud)hw->dis_last_dvf == (ud)dvfm_c && !dpvpp_dbg_force_update_all())
		return EDIM_DVPP_DIFF_SAME_FRAME;

	dvfml = &hw->dis_last_dvf->c.in_dvfm.vfs;
	if (dvfml->bitdepth != dvfmc->bitdepth	||
	    dvfml->type != dvfmc->type		||
	    dvfml->width != dvfmc->width	||
	    dvfml->height != dvfmc->height) {
		atomic_add(DI_BIT13, &itf->c.dbg_display_sts);	/* dbg only */
		PR_INF("diff:size2:%d<0x%x,0x%x,%d,%d,%d>,<0x%x,0x%x,%d,%d,%d>\n",
			hw->dis_last_dvf->c.cnt_in,
			dvfml->bitdepth,
			dvfml->type,
			dvfml->width,
			dvfml->height,
			hw->dis_last_dvf->c.cnt_in,
			dvfmc->bitdepth,
			dvfmc->type,
			dvfmc->width,
			dvfmc->height,
			dvfm_c->c.cnt_in);
		return EDIM_DVPP_DIFF_ALL;
	}

	if (dpvpp_dbg_force_update_all()) { /* debug only */
		atomic_add(DI_BIT14, &itf->c.dbg_display_sts);	/* dbg only */
		return EDIM_DVPP_DIFF_ALL;
	}
	if (hw->dis_last_dvf->c.set_cfg.d32 != dvfm_c->c.set_cfg.d32) {
		atomic_add(DI_BIT15, &itf->c.dbg_display_sts);	/* dbg only */
		state = EDIM_DVPP_DIFF_MEM;
	}
	return state | EDIM_DVPP_DIFF_ONLY_ADDR;
}

static int dpvpp_get_plink_input_win(struct di_ch_s *pch,
		unsigned int src_w, unsigned int src_h, struct di_win_s *out)
{
	struct di_win_s *win_c;
	struct dim_pvpp_hw_s *hw;
	struct di_win_s tmp;
	u32 val;
	//ulong irq_flag = 0;
	int ret = 0;

	if (!pch) {
		PR_ERR("%s:1\n", __func__);
		return -1;
	}

	//spin_lock_irqsave(&lock_pvpp, irq_flag);
	hw = &get_datal()->dvs_prevpp.hw;
	if (!out ||
	    hw->dis_ch != pch->ch_id ||
	    !atomic_read(&hw->link_sts)) {
		PR_WARN("%s:2 %d %d %px, sts:%d\n",
			__func__, hw->dis_ch, pch->ch_id,
			out, atomic_read(&hw->link_sts) ? 1 : 0);
		//spin_unlock_irqrestore(&lock_pvpp, irq_flag);
		return -1;
	}

	if (dpvpp_dbg_force_no_crop())
		return -2;

	/* copy current win para */
	win_c = &hw->dis_c_para.win;
	memcpy(&tmp, win_c, sizeof(*win_c));
	/* x size check */
	val = (tmp.x_check_sum >> 16) & 0xffff;
	if (val != tmp.x_size)
		ret++;
	val = tmp.x_check_sum & 0xffff;
	if (val != ((tmp.x_st + tmp.x_end) & 0xffff))
		ret++;

	/* y size check */
	val = (tmp.y_check_sum >> 16) & 0xffff;
	if (val != tmp.y_size)
		ret++;
	val = tmp.y_check_sum & 0xffff;
	if (val != ((tmp.y_st + tmp.y_end) & 0xffff))
		ret++;

	if (src_w != tmp.orig_w)
		ret++;
	if (src_h != tmp.orig_h)
		ret++;

	if (ret)
		PR_WARN("%s: win info incompleted (%d %d; %d %d) x:%x %x %x %x; y:%x %x %x %x\n",
			__func__, src_w, src_h, tmp.orig_w, tmp.orig_h,
			tmp.x_st, tmp.x_end, tmp.x_size, tmp.x_check_sum,
			tmp.y_st, tmp.y_end, tmp.y_size, tmp.y_check_sum);
	if (!ret)
		memcpy(out, &tmp, sizeof(*out));
	//spin_unlock_irqrestore(&lock_pvpp, irq_flag);
	return ret;
}

static void dpvpp_set_default_para(struct dim_prevpp_ds_s *ds,
				   struct dimn_dvfm_s *ndvfm)
{
	struct pvpp_dis_para_in_s  *dis_para_demo;
	struct dvfm_s *in_dvfm;

	dis_para_demo = &ds->dis_para_demo;
	in_dvfm		= &ndvfm->c.in_dvfm;
	memset(dis_para_demo, 0, sizeof(*dis_para_demo));
	dis_para_demo->dmode = EPVPP_DISPLAY_MODE_NR;
	dis_para_demo->win.x_size = in_dvfm->vfs.width;
	dis_para_demo->win.y_size = in_dvfm->vfs.height;
	dis_para_demo->win.x_end = dis_para_demo->win.x_size - 1;
	dis_para_demo->win.y_end = dis_para_demo->win.y_size - 1;
#ifdef DBG_FLOW_SETTING
	print_pvpp_dis_para_in(dis_para_demo, "dis_para_demo");
#endif
}

static void dpvpp_cnt_para(struct pvpp_dis_para_in_s  *dis_para_demo,
			struct vframe_s *vfm,
			unsigned int x_off,
			unsigned int y_off)
{
	unsigned int x, y;

	if (!dis_para_demo || !vfm) {
		PR_ERR("%s:null\n", __func__);
		return;
	}
	dim_vf_x_y(vfm, &x, &y);
	memset(dis_para_demo, 0, sizeof(*dis_para_demo));
	dis_para_demo->dmode = EPVPP_DISPLAY_MODE_NR;
	dis_para_demo->win.x_st	= x_off;
	dis_para_demo->win.y_st	= y_off;
	dis_para_demo->win.x_size = x - (x_off << 1);
	dis_para_demo->win.y_size = y - (y_off << 1);
	dis_para_demo->win.x_end = dis_para_demo->win.x_st +
				dis_para_demo->win.x_size - 1;
	dis_para_demo->win.y_end = dis_para_demo->win.y_st +
				dis_para_demo->win.y_size - 1;
#ifdef DBG_FLOW_SETTING
	print_pvpp_dis_para_in(dis_para_demo, "dis_para_demo");
#endif
}

static int dpvpp_check_vf_api_l(struct vframe_s *vfm)
{
	struct dim_prevpp_ds_s *ds;
	struct dimn_itf_s *itf;
	int check_add;

	itf = get_itf_vfm(vfm);

	if (!itf || !itf->ds)
		return EPVPP_ERROR_DI_NOT_REG;
	ds = itf->ds;

	check_add = dpvpp_check_dvfm_addr(ds, vfm);

	if (check_add < 0) {
		dbg_mem("%s:add:[%d]\n", __func__, check_add);
		return EPVPP_ERROR_VFM_NOT_ACT;
	}

	if (!dpvpp_check_dvfm_act(itf, vfm))
		return EPVPP_ERROR_VFM_NOT_ACT;
	return EPVPP_DISPLAY_MODE_NR;
}

static int dpvpp_check_vf_api(struct vframe_s *vfm)
{
	ulong irq_flag = 0;
	int ret;

	spin_lock_irqsave(&lock_pvpp, irq_flag);
	ret = dpvpp_check_vf_api_l(vfm);
	spin_unlock_irqrestore(&lock_pvpp, irq_flag);
	return ret;
}

static int dpvpp_check_pre_vpp_link_by_di_api(void)
{
	struct dim_pvpp_hw_s *hw;

	hw = &get_datal()->dvs_prevpp.hw;

	if (dpvpp_dbg_vpp_active())
		return EPVPP_DISPLAY_MODE_NR;
	if (!atomic_read(&hw->link_on_bydi))
		return EPVPP_ERROR_DI_OFF;
	if (atomic_read(&hw->link_in_busy))
		return EPVPP_ERROR_DI_IN_BUSY;

	return EPVPP_DISPLAY_MODE_NR;
}

static int dpvpp_display_unreg_bypass(void)
{
	ulong irq_flag = 0;
	struct dim_pvpp_hw_s *hw;

	spin_lock_irqsave(&lock_pvpp, irq_flag);

	hw = &get_datal()->dvs_prevpp.hw;

	if (hw->dis_last_para.dmode != EPVPP_DISPLAY_MODE_BYPASS) {
		dbg_plink1("%s:trig dis_ch:%d->255\n", __func__, hw->dis_ch);
		hw->dis_ch = 0xff;
		/* from pre-vpp link to bypass */
		//be sure vpp can display
		dim_afds()->pvpp_sw_setting_op(false, hw->op);
		//dpvpph_pre_data_mif_ctrl(false, hw->op);
		dpvpph_reverse_mif_ctrl(0, 0, hw->op);
		opl1()->pre_mif_sw(false, hw->op, true);
		dpvpph_prelink_sw(hw->op, false);
	}
	if (hw->dis_ch != hw->dct_ch && !hw->dct_flg) {
		hw->dct_flg = 2;
		dbg_plink1("dis:trig2:dct unreg dis_ch:%d dct_ch:%d\n",
			hw->dis_ch, hw->dct_ch);
	}
	hw->dis_last_para.dmode = EPVPP_DISPLAY_MODE_BYPASS;
	hw->dis_last_dvf	= NULL;
	hw->blk_used_hd = false;
	hw->blk_used_uhd = false;
	atomic_set(&hw->link_instance_id, DIM_PLINK_INSTANCE_ID_INVALID);
	spin_unlock_irqrestore(&lock_pvpp, irq_flag);
	dbg_plink1("%s:end\n", "unreg_bypass");
	return EPVPP_DISPLAY_MODE_BYPASS;
}

/*copy from dim_pulldown_info_clear_g12a*/
void dpvpph_pulldown_info_clear_g12a(const struct reg_acc *op_in)
{
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A))
		op_in->bwr(DI_PRE_CTRL, 1, 30, 1);
	dim_print("%s:\n", __func__);
}

static int dpvpp_display(struct vframe_s *vfm,
			 struct pvpp_dis_para_in_s *in_para,
			 void *out_para)
{
	struct dim_prevpp_ds_s *ds;
	struct dimn_itf_s *itf;
	struct dimn_dvfm_s *ndvfm;
	unsigned int	diff;/*enum EDIM_DVPP_DIFF*/
	struct vframe_s *vf_in/*, *vf_out*/;
	const struct reg_acc *op;
	struct pvpp_dis_para_in_s *para;
	bool dbg_tm = false;
	unsigned long delay;
	struct dim_pvpp_hw_s *hw;
	struct pvpp_buf_cfg_s *buf_cfg;

	hw = &get_datal()->dvs_prevpp.hw;

	itf = get_itf_vfm(vfm);
	if (!itf || !itf->ds) {
		PR_ERR("%s:no data:0x%px:%d\n",
			__func__, vfm, vfm->di_instance_id);
		return EPVPP_ERROR_DI_NOT_REG;
	}
	ds = itf->ds;

	/* debug print */
	delay = itf->c.jiff_display + 1 * HZ;
	if (time_after(jiffies, delay)) {
		dbg_tm = true;
		itf->c.jiff_display = jiffies;
	}
	atomic_set(&itf->c.dbg_display_sts, 0);
	/*******/
	if (!ds || !vfm) {
		PR_ERR("%s:di not reg\n", __func__);
		if (!ds)
			PR_ERR("no ds\n");
		if (!vfm)
			PR_ERR("no vfm");
		return EPVPP_ERROR_DI_NOT_REG;
		//goto DISPLAY_BYPASS;
	}
	atomic_add(DI_BIT0, &itf->c.dbg_display_sts);	/* dbg only */
	if (!atomic_read(&hw->link_sts))
		goto DISPLAY_BYPASS;
	if (itf->flg_freeze)	/* @ary 11-08 add */
		goto DISPLAY_BYPASS;
	op = hw->op;
	if (itf->c.src_state &
	    (EDVPP_SRC_NEED_MSK_CRT | EDVPP_SRC_NEED_MSK_BYPASS))
		goto DISPLAY_BYPASS;
	atomic_add(DI_BIT1, &itf->c.dbg_display_sts);	/* dbg only */
	/* check if bypass */
	ndvfm = dpvpp_check_dvfm_act(itf, vfm);
	if (!ndvfm)
		goto DISPLAY_BYPASS;
	atomic_add(DI_BIT2, &itf->c.dbg_display_sts);	/* dbg only */
	dbg_check_ud(ds, 2); /* dbg */

	para = in_para;
	if (!in_para) {
		dpvpp_set_default_para(ds, ndvfm);
		para = &ds->dis_para_demo;
	}
	if (para->dmode == EPVPP_DISPLAY_MODE_BYPASS)
		goto DISPLAY_BYPASS;
	if (dpvpp_bypass_display()) //dbg only
		goto DISPLAY_BYPASS;
	/*check buffer config */
	buf_cfg = get_buf_cfg(ndvfm->c.out_fmt, 1);
	if (!buf_cfg)
		goto DISPLAY_BYPASS;

	if (ndvfm->c.is_out_4k)
		hw->blk_used_uhd = true;
	else
		hw->blk_used_hd = true;
	diff = check_diff(itf, ndvfm, para);

	vf_in = &ndvfm->c.vf_in_cp;

	ndvfm->c.cnt_display = ds->cnt_display;
#ifdef HIS_CODE
	if (ndvfm->c.cnt_display < 5)
		PR_INF("%d:diff:0x%x\n", ndvfm->c.cnt_display, diff);
#endif
	if ((diff & 0xfff0) &&
	    (diff & 0xfff0)  != (ds->dbg_last_diff & 0xfff0)) {
		dbg_mem("diff:0x%x->0x%x\n", ds->dbg_last_diff, diff);
		atomic_add(DI_BIT3, &itf->c.dbg_display_sts);	/* dbg only */
		ds->dbg_last_diff = diff;
	}
	dbg_plink3("diff:0x%x\n", diff);
	ndvfm->c.sts_diff = diff;//dbg only;
	if (diff & EDIM_DVPP_DIFF_ALL) {
		dim_print("%s:vfm:0x%px, in_para:0x%px, o:0x%px\n",
		__func__, vfm, in_para, out_para);
		hw->reg_cnt = itf->sum_reg_cnt;
		ds->pre_done_nub++;

		dpvpph_size_change(ds, ndvfm, op);
		if (nr_op() &&
		    nr_op()->cue_int &&
		    !(ds->en_dbg_off_nr & DI_BIT7))
			nr_op()->cue_int(vf_in, op);//temp need check reg write
#ifdef HIS_CODE
		if (hw->dis_c_para.win.x_size != ds->out_dvfm_demo.width ||
		    hw->dis_c_para.win.y_size != ds->out_dvfm_demo.height) {
			ds->out_dvfm_demo.width = hw->dis_c_para.win.x_size;
			ds->out_dvfm_demo.height = hw->dis_c_para.win.y_size;
		}
#endif
		//note: out_win is not same as dis_c_para.win.
		ds->out_win.x_size = hw->dis_c_para.win.x_size;
		ds->out_win.y_size = hw->dis_c_para.win.y_size;
		ds->out_win.x_end	= ds->out_win.x_size - 1;
		ds->out_win.y_end	= ds->out_win.y_size - 1;
		ds->out_win.x_st	= 0;
		ds->out_win.y_st	= 0;
		ds->cnt_display = 0;
		//mif in:
		ds->mifpara_in.cur_inp_type = 0; //p is 0, i is 1;
		ds->mifpara_in.prog_proc_type = 0x10;// p is 0x10, i is 0; tmp;
		//ds->mifpara_in.linear = 0; //tmp;
		memcpy(&ds->mifpara_in.win,
		       &hw->dis_c_para.win, sizeof(ds->mifpara_in.win));
		//mif out:
		memcpy(&ds->mifpara_out,
		       &ds->mifpara_in, sizeof(ds->mifpara_out));
		memcpy(&ds->mifpara_out.win,
		       &ds->out_win, sizeof(ds->mifpara_out.win));
#ifdef DBG_FLOW_SETTING
		print_dim_mifpara(&ds->mifpara_out, "display:out_set");
#endif
		//mif mem
		memcpy(&ds->mifpara_mem,
		       &ds->mifpara_out, sizeof(ds->mifpara_mem));

		ds->mif_in.mif_index = DI_MIF0_ID_INP;
		ds->mif_mem.mif_index  = DI_MIF0_ID_MEM;

		/**/
		if (DIM_IS_IC_EF(SC2)) {
			dim_sc2_contr_pre(&ds->pre_top_cfg, ds->op);
			if (ndvfm->c.is_out_4k)
				ds->pre_top_cfg.b.mode_4k = 1;
			else
				ds->pre_top_cfg.b.mode_4k = 0;
			dim_sc2_4k_set(ds->pre_top_cfg.b.mode_4k, ds->op);
		}
		ndvfm->c.cnt_display = ds->cnt_display;
		//cvs issue dpvpph_display_update_sub_last_cvs(itf, ds, ndvfm, op);

		//dbg_di_prelink_reg_check_v3_op(op);
		dpvpph_display_update_all(ds, ndvfm, ds->op);

		if (nr_op() && !ds->en_dbg_off_nr)
			nr_op()->nr_process_in_irq(op);

		//set last:?
		memcpy(&hw->dis_last_para,
		       &hw->dis_c_para, sizeof(hw->dis_last_para));
		hw->id_l = hw->id_c;
		dim_print("display:set end:%d\n", ds->cnt_display);
		hw->dis_last_dvf	=  ndvfm;
		/* dct */
		if (hw->en_dct &&
		    hw->dis_ch != itf->bind_ch &&
		    !hw->dct_flg) {
			dbg_plink3("dis:ch<%d:%d>\n", hw->dis_ch, itf->bind_ch);
			hw->dis_ch = itf->bind_ch;
		}
		ds->cnt_display++;
		atomic_add(DI_BIT4, &itf->c.dbg_display_sts);	/* dbg only */
	} else {
		ndvfm->c.cnt_display = ds->cnt_display;
		//cvs issue dpvpph_display_update_sub_last_cvs(itf,
		//					       ds, ndvfm, op);

		if (diff & EDIM_DVPP_DIFF_SAME_FRAME) {
			/* nothing ? */
			//PR_INF("?same?\n");
			dpvpph_prelink_polling_check(ds->op, true);
			/* dbg only */
			atomic_add(DI_BIT5, &itf->c.dbg_display_sts);
		} else if (diff & EDIM_DVPP_DIFF_ONLY_ADDR) {
			ds->pre_done_nub++;

			//dbg_di_prelink_reg_check_v3_op(op);
			dpvpph_display_update_part(ds, ndvfm, ds->op, diff);

			if (nr_op() && !ds->en_dbg_off_nr)
				nr_op()->nr_process_in_irq(op);
#ifndef AUTO_NR_DISABLE
			dpvpph_pulldown_info_clear_g12a(op);
#endif
			dim_print("display:up addr end:%d\n", ds->cnt_display);
			//set last:
			ds->cnt_display++;
			hw->dis_last_dvf	=  ndvfm;
			/* dbg only */
			atomic_add(DI_BIT6, &itf->c.dbg_display_sts);
		} else {
			PR_INF("?other?\n");
			/* dbg only */
			atomic_add(DI_BIT7, &itf->c.dbg_display_sts);
		}
	}
	ds->field_count_for_cont++;
	if (cfgg(DCT) &&
	    hw->dis_ch != 0xff		&&
	    hw->dis_ch != hw->dct_ch	&&
	    !hw->dct_flg) {
		if (hw->dct_ch != 0xff)
			hw->dct_flg = 2;
		else
			hw->dct_flg = 1;
		dbg_plink1("dis:trig:dct reg:%d->%d\n", hw->dct_ch, hw->dis_ch);
	}
	atomic_set(&hw->link_instance_id, itf->sum_reg_cnt);
	return EPVPP_DISPLAY_MODE_NR;

DISPLAY_BYPASS:
	if (hw->dis_last_para.dmode != EPVPP_DISPLAY_MODE_BYPASS) {
		hw->dis_ch = 0xff;

		/* from pre-vpp link to bypass */
		//be sure vpp can display
		dim_afds()->pvpp_sw_setting_op(false, hw->op); //disable afbcd
		//dpvpph_pre_data_mif_ctrl(false, hw->op);
		opl1()->pre_mif_sw(false, hw->op, true);
		dpvpph_reverse_mif_ctrl(0, 0, hw->op);
		dpvpph_prelink_sw(hw->op, false);
		atomic_add(DI_BIT8, &itf->c.dbg_display_sts);	/* dbg only */
		hw->blk_used_hd = false;
		hw->blk_used_uhd = false;
	}
	if (hw->dis_ch != hw->dct_ch && !hw->dct_flg) {
		hw->dct_flg = 2;
		dbg_plink1("dis:trig1:dct unreg:%d->%d\n", hw->dct_ch, hw->dis_ch);
	}
	hw->dis_last_para.dmode = EPVPP_DISPLAY_MODE_BYPASS;
	hw->dis_last_dvf	= NULL;
	if (ds) {
		ds->cnt_display = 0;
	}
	atomic_set(&hw->link_instance_id, DIM_PLINK_INSTANCE_ID_INVALID);
	atomic_add(DI_BIT9, &itf->c.dbg_display_sts);	/* dbg only */
	if (dbg_tm)
		PR_INF("%s:bypass\n", __func__);
	return EPVPP_DISPLAY_MODE_BYPASS;
}

static int dpvpp_display_api(struct vframe_s *vfm,
			 struct pvpp_dis_para_in_s *in_para,
			 void *out_para)
{
	int ret;
	ulong irq_flag = 0;
	struct vframe_s *in_vfm = vfm;

	spin_lock_irqsave(&lock_pvpp, irq_flag);
	ret = dpvpp_display(in_vfm, in_para, out_para);
	dbg_display_status(in_vfm, in_para, out_para, ret);
	spin_unlock_irqrestore(&lock_pvpp, irq_flag);
	return ret;
}

static int dpvpp_vpp_get(struct vframe_s *vfm)
{
	struct dimn_dvfm_s *dvfm = NULL;
	struct dimn_itf_s *itf;

	itf = get_itf_vfm(vfm);

	dvfm = dpvpp_check_dvfm_act(itf, vfm);
	if (!dvfm)
		return ERR_NOT_DI_VPP;
	atomic_inc(&dvfm->c.flg_vpp);
	return EPVPP_OK;
}

static int dpvpp_vpp_put(struct vframe_s *vfm)
{
	struct dimn_dvfm_s *dvfm = NULL;
	struct dimn_itf_s *itf;

	itf = get_itf_vfm(vfm);

	dvfm = dpvpp_check_dvfm_act(itf, vfm);
	if (!dvfm)
		return ERR_NOT_DI_VPP;
	if (!atomic_dec_and_test(&dvfm->c.flg_vpp))
		PR_ERR("%s:%px,flg_vpp %d\n", __func__, dvfm,
			atomic_read(&dvfm->c.flg_vpp));
	return EPVPP_OK;
}

static const struct dimn_pvpp_ops_api_s dvpp_api_ops = {
	.display	= dpvpp_display_api,
	.display_unreg_bypass = dpvpp_display_unreg_bypass,
	.vpp_sw		=  dpvpp_link_sw_api,
	.check_vf	= dpvpp_check_vf_api,
	.check_di_act	= dpvpp_check_pre_vpp_link_by_di_api,
	.vpp_get	= dpvpp_vpp_get,
	.vpp_put	= dpvpp_vpp_put,
	.get_di_in_win = dpvpp_get_plink_input_win,
};

//static
int event_reset(struct dimn_itf_s *itf)
{
	//to-do
	return 0;
}

static void dpvpp_vf_put(struct vframe_s *vf, struct dimn_itf_s *itf)
{
	struct dimn_qs_cls_s *qback;
	struct dimn_dvfm_s *dvfm = NULL;
	struct dim_prevpp_ds_s *ds = NULL;

	if (!itf || !itf->ds || !vf) {
		PR_ERR("%s:\n", __func__);
		return;
	}
	ds = itf->ds;
	dvfm = (struct dimn_dvfm_s *)vf->private_data;
	if (!dvfm) {
		PR_ERR("%s private_data null:\n", __func__);
		return;
	}
	if (dvfm->header.code != CODE_INS_LBF) {
		PR_ERR("%s:%px,code err 0x%x\n",
		       __func__, vf, dvfm->header.code);
		return;
	}

	if (!atomic_dec_and_test(&dvfm->c.flg_get)) {
		PR_ERR("%s:%px,flg_vpp %d\n", __func__, dvfm,
			atomic_read(&dvfm->c.flg_get));
		return;
	}
	qback = &ds->lk_que_kback;
	qback->ops.put(qback, vf);
	task_send_wk_timer(EDIM_WKUP_REASON_BACK_BUF);
}

static struct vframe_s *dpvpp_vf_get(struct dimn_itf_s *itf)
{
	struct dimn_qs_cls_s *qready;
	struct dimn_dvfm_s *dvfm = NULL;
	struct dim_prevpp_ds_s *ds = NULL;
	struct vframe_s *vf = NULL;
#ifdef DBG_OUT_SIZE
	static unsigned int o_x, o_y;
#endif
	if (!itf || !itf->ds) {
		PR_ERR("%s:\n", __func__);
		return NULL;
	}
	ds = itf->ds;
	qready = &ds->lk_que_ready;
	if (!qready->ops.count(qready) || itf->c.pause_pst_get)
		return NULL;
	vf = qready->ops.get(qready);
	if (!vf) {
		PR_ERR("%s:get err?\n", __func__);
		return NULL;
	}
	if (vf->di_flag & DI_FLAG_DI_BYPASS)
		return vf;

	dvfm = (struct dimn_dvfm_s *)vf->private_data;
	if (!dvfm) {
		PR_ERR("%s:no dvfm?\n", __func__);
		return NULL;
	}
	atomic_inc(&dvfm->c.flg_get);
#ifdef DBG_OUT_SIZE
	if (vf->width != o_x ||
	    vf->height != o_y) {
		PR_INF("osize:<%d,%d> <%d,%d>\n",
			o_x, o_y, vf->width, vf->height);
		o_x = vf->width;
		o_y = vf->height;
	}
#endif /* DBG_OUT_SIZE */
	return vf;
}

static struct vframe_s *dpvpp_vf_peek(struct dimn_itf_s *itf)
{
	struct dimn_qs_cls_s *qready;
	struct dim_prevpp_ds_s *ds = NULL;
	struct vframe_s *vf = NULL;

	if (!itf || !itf->ds) {
		PR_ERR("%s:\n", __func__);
		return NULL;
	}
	ds = itf->ds;
	qready = &ds->lk_que_ready;
	if (itf->c.pause_pst_get)
		return NULL;
	vf = (struct vframe_s *)qready->ops.peek(qready);

	return vf;
}

static bool vf_m_in(struct dimn_itf_s *itf)
{
	struct dimn_qs_cls_s *qidle, *qin, *qr;
	struct dimn_dvfm_s *ndvfm;
	struct dim_prevpp_ds_s *ds = NULL;
	unsigned int in_nub, free_nub;
	int i;
	struct vframe_s *vf, *mvf;
	bool flg_q;
	unsigned int err_cnt = 0;
#ifdef DBG_TIMER
	u64 ustime;

	//ustime = cur_to_usecs();
	ustime	= cur_to_msecs();
#endif

	if (!itf || !itf->ds) {
		PR_ERR("%s:\n", __func__);
		return false;
	}
	ds = itf->ds;
	if (itf->c.pause_pre_get) /* stop input get */
		return false;
	qidle	= &ds->lk_que_idle;
	qin	= &ds->lk_que_in;
	qr	= &ds->lk_que_recycle;
	in_nub		= qin->ops.count(qin);
	free_nub	= qidle->ops.count(qidle);

	if (in_nub >= DIM_K_VFM_IN_LIMIT	||
	    (free_nub < (DIM_K_VFM_IN_LIMIT - in_nub))) {
		return false;
	}

	for (i = 0; i < (DIM_K_VFM_IN_LIMIT - in_nub); i++) {
#ifdef pre_dbg_is_run
		if (!pre_dbg_is_run(itf->bind_ch)) //p_pause
			break;
#endif
		vf = in_patch_first_buffer(itf);
		if (!vf) {
			vf = in_vf_peek(itf);
			if (!vf)
				break;

			vf = in_vf_get(itf);
		}
		if (!vf)
			break;
		mvf = (struct vframe_s *)qidle->ops.get(qidle);

		if (!mvf || !mvf->private_data) {
			PR_ERR("%s:qout:%d:0x%px\n",
				__func__,
				qidle->ops.count(qidle),
				mvf);
			err_cnt++;
			in_vf_put(itf, vf);
			break;
		}

		/* dbg only */
		dbg_check_vf(ds, mvf, 1);
		if (dimp_get(edi_mp_force_width))
			vf->width = dimp_get(edi_mp_force_width);

		if (dimp_get(edi_mp_force_height))
			vf->height = dimp_get(edi_mp_force_height);

		/* dbg end **************************/
		ndvfm = (struct dimn_dvfm_s *)mvf->private_data;
		dim_print("%s:nvfm:0x%px\n", __func__, ndvfm);
		memset(&ndvfm->c, 0, sizeof(ndvfm->c));
		ndvfm->c.ori_in = vf;
		ndvfm->c.ori_vf = vf;
		ndvfm->c.cnt_in = itf->c.sum_pre_get;
		if (vf->decontour_pre) {
			ndvfm->c.dct_pre = vf->decontour_pre;
			vf->decontour_pre = NULL;
			ds->dct_sum_in++;
		}
		memcpy(&ndvfm->c.vf_in_cp, vf, sizeof(ndvfm->c.vf_in_cp));
		dim_dvf_cp(&ndvfm->c.in_dvfm, &ndvfm->c.vf_in_cp, 0);

		if (IS_COMP_MODE(ndvfm->c.in_dvfm.vfs.type)) {
			ndvfm->c.in_dvfm.vfs.width = vf->compWidth;
			ndvfm->c.in_dvfm.vfs.height = vf->compHeight;
		}
		ndvfm->c.src_w = ndvfm->c.in_dvfm.vfs.width;
		ndvfm->c.src_h = ndvfm->c.in_dvfm.vfs.height;
		count_4k(itf, &ndvfm->c.in_dvfm);
		flg_q = qin->ops.put(qin, mvf);

		if (!flg_q) {
			PR_ERR("%s:qin\n", __func__);
			err_cnt++;
			in_vf_put(itf, vf);
			qidle->ops.put(qidle, mvf);
			break;
		}
#ifdef DBG_TIMER
		if (itf->c.sum_pre_get < 4)
			dbg_mem2("get[%d]:%lu\n",
				 itf->c.sum_pre_get, (unsigned long)ustime);
#endif
	}

	if (err_cnt)
		return false;
	return true;
}

static int dpvpp_et_ready(struct dimn_itf_s *itf)
{
	task_send_wk_timer(EDIM_WKUP_REASON_IN_HAVE);

	return 0;
}

//itf->ops_vfm->reg_api
static const struct dimn_vfml_ops_s dvpp_ops_vfm = {
#ifdef DIM_PLINK_ENABLE_CREATE
	.reg_api	= dpvpp_t_reg_api,//dpvpp_reg,
	.unreg_api	= dpvpp_t_unreg_api,
#endif
	.vf_put		= dpvpp_vf_put,
	.vf_get		= dpvpp_vf_get,
	.vf_peek	= dpvpp_vf_peek,
	.et_ready	= dpvpp_et_ready,
	.vf_m_in	= vf_m_in,

};

void dpvpp_prob(void)
{
	struct dim_dvs_prevpp_s *dv_prevpp;
	struct dimn_itf_s *itf;
	struct dim_pvpp_hw_s *hw;
	int i;
	int j; //debug only

	hw = &get_datal()->dvs_prevpp.hw;

	dv_prevpp = &get_datal()->dvs_prevpp;

	memset(dv_prevpp, 0, sizeof(*dv_prevpp));

	if (!IS_IC_SUPPORT(PRE_VPP_LINK) || !cfgg(EN_PRE_LINK))
		return;
	dd_probe();

	dv_prevpp->allowed	= true;
	dv_prevpp->insert	= true;//false;
	dv_prevpp->ops = &dvpp_m_ops;
	dv_prevpp->vf_ops	= &dimn_vf_provider;
	for (i = 0; i < DI_PLINK_CN_NUB; i++) {
		itf = &dv_prevpp->itf[i];
		itf->dvfm.name	= DIM_PRE_VPP_NAME;
		itf->ch		= DIM_PRE_VPP_NUB;
		itf->id		= i;
		itf->bypass_complete = true; //tmp
		itf->ops_vfm = &dvpp_ops_vfm;
		/* src need */
		itf->src_need = EDVPP_SRC_NEED_VAL	|
				EDVPP_SRC_NEED_KFIFO	|
				EDVPP_SRC_NEED_REG2	|
				//EDVPP_SRC_NEED_SCT_TAB	|
				//EDVPP_SRC_NEED_SCT_TOP	|
				//EDVPP_SRC_NEED_TEST	| /* dbg only */
				EDVPP_SRC_NEED_MEM	|
				EDVPP_SRC_NEED_LINK_ON;
		spin_lock_init(&itf->lock_first_buf);
#ifdef DIM_PLINK_ENABLE_CREATE
		init_completion(&itf->reg_done);
		mutex_init(&itf->lock_reg);
		if (!dv_prevpp->insert)
			dvframe_prob(itf);
#endif /* DIM_PLINK_ENABLE_CREATE */
	}
	//debug only:
	for (i = 0; i < DI_PLINK_CN_NUB; i++) {
		itf = &dv_prevpp->itf[i];
		dbg_plink2("id[%d]:vfm:\n", i);
		for (j = 0; j < DIM_LKIN_NUB; j++)
			dbg_plink2("\t:%d:0x%px\n", j, &itf->vf_bf[j]);
		dbg_plink2("id[%d]:buffer:\n", i);
		for (j = 0; j < DIM_LKIN_NUB; j++)
			dbg_plink2("\t:%d:0x%px\n", j, &itf->buf_bf[j]);
	}
	/* timer */
	hrtimer_init(&dv_prevpp->hrtimer_wk, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	dv_prevpp->hrtimer_wk.function = dpvpp_wk_hrtimer_func;

	dv_prevpp->ops_api = &dvpp_api_ops;//NULL;//

#ifdef TST_2022_0817
	q_int();
#endif /* TST_2022_0817 */
	hw->op = &dpvpp_regset; //&di_pre_regset; //
	hw->op_n = &di_pre_regset;
	if (cfgg(LINEAR))
		hw->en_linear = true;
	else
		hw->en_linear = false;
	/*cvs*/
	if (!hw->en_linear) {
		hw->cvs[0][0]	= cvs_nub_get(EDI_CVSI_PVPP_A_IN1, "in1_y");
		hw->cvs[0][1]	= cvs_nub_get(EDI_CVSI_PVPP_A_IN1_C, "in1_c");

		hw->cvs[0][2]	= cvs_nub_get(EDI_CVSI_PVPP_A_NR, "nr_y");
		hw->cvs[0][3]	= cvs_nub_get(EDI_CVSI_PVPP_A_NR_C, "nr_c");

		hw->cvs[0][4]	= cvs_nub_get(EDI_CVSI_PVPP_A_MEM, "mem_y");
		hw->cvs[0][5]	= cvs_nub_get(EDI_CVSI_PVPP_A_MEM_C, "mem_c");

		hw->cvs[1][0]	= cvs_nub_get(EDI_CVSI_PVPP_B_IN1, "in1_y");
		hw->cvs[1][1]	= cvs_nub_get(EDI_CVSI_PVPP_B_IN1_C, "in1_c");

		hw->cvs[1][2]	= cvs_nub_get(EDI_CVSI_PVPP_B_NR, "nr_y");
		hw->cvs[1][3]	= cvs_nub_get(EDI_CVSI_PVPP_B_NR_C, "nr_c");

		hw->cvs[1][4]	= cvs_nub_get(EDI_CVSI_PVPP_B_MEM, "mem_y");
		hw->cvs[1][5]	= cvs_nub_get(EDI_CVSI_PVPP_B_MEM_C, "mem_c");
	}
	buf_cfg_prob();
	if (IS_IC_SUPPORT(DECONTOUR) && cfgg(DCT))
		hw->en_dct = true;
	else
		hw->en_dct = false;
	hw->dct_ch = 0xff;
	hw->dis_ch = 0xff;
	atomic_set(&hw->link_instance_id, DIM_PLINK_INSTANCE_ID_INVALID);
	dbg_tst("%s:size[0x%zx]k\n",
		__func__, (sizeof(*dv_prevpp) >> 10));
}

const struct dimn_pvpp_ops_s *dpvpp_ops(void)
{
	if (!IS_IC_SUPPORT(PRE_VPP_LINK))
		return NULL;

	return get_datal()->dvs_prevpp.ops;
}

//dimn_vf_provider
const struct vframe_operations_s *dpvpp_vf_ops(void)
{
	return get_datal()->dvs_prevpp.vf_ops;
}

const struct dimn_pvpp_ops_api_s *dpvpp_ops_api(void)
{
	struct dim_pvpp_hw_s *hw;

	hw = &get_datal()->dvs_prevpp.hw;
	if (!IS_IC_SUPPORT(PRE_VPP_LINK))
		return NULL;
#ifdef HIS_CODE
	if (hw->en_pst_wr_test)
		return NULL;
#endif
	if (!get_dim_de_devp())
		return NULL;

	return get_datal()->dvs_prevpp.ops_api;
}

/* copy from di_unreg_setting*/
//static
void dpvpph_unreg_setting(void)
{
	/*unsigned int mirror_disable = get_blackout_policy();*/
	//unsigned int mirror_disable = 0;
	struct di_dev_s *de_devp = get_dim_de_devp();
	const struct reg_acc *op = &di_pre_regset;//&dpvpp_regset; //

	dbg_plink1("%s:\n", __func__);
	//tmp sc2_dbg_set(0);
	/*set flg*/
#ifdef HIS_CODE //tmp
	set_hw_reg_flg(false);
	if (get_datal()->dct_op)
		get_datal()->dct_op->unreg_all();
	if (dim_hdr_ops())
		dim_hdr_ops()->unreg_setting();
	dim_dw_unreg_setting();
#endif
	dimh_enable_di_pre_mif(false, dimp_get(edi_mp_mcpre_en));
	post_close_new();	/*2018-11-29*/
	//dimh_afbc_reg_sw(false);
	if (dim_afds())
		dim_afds()->reg_sw_op(false, op);
	dimh_hw_uninit();
	if (is_meson_txlx_cpu()	||
	    is_meson_txhd_cpu()	||
	    is_meson_g12a_cpu() ||
	    is_meson_g12b_cpu()	||
	    is_meson_tl1_cpu()	||
	    is_meson_tm2_cpu()	||
	    DIM_IS_IC(T5)	||
	    DIM_IS_IC(T5DB)	||
	    DIM_IS_IC(T5D)	||
	    is_meson_sm1_cpu()) {
		dim_pre_gate_control(false, dimp_get(edi_mp_mcpre_en));
		if (nr_op() && nr_op()->nr_gate_control)
			nr_op()->nr_gate_control(false, op);
	} else if (DIM_IS_IC_EF(SC2)) {
		dim_pre_gate_control_sc2(false, dimp_get(edi_mp_mcpre_en));
		if (nr_op() && nr_op()->nr_gate_control)
			nr_op()->nr_gate_control(false, op);
	} else if (cpu_after_eq(MESON_CPU_MAJOR_ID_GXTVBB)) {
		DIM_DI_WR(DI_CLKG_CTRL, 0x80f60000);
		DIM_DI_WR(DI_PRE_CTRL, 0);
	} else {
		DIM_DI_WR(DI_CLKG_CTRL, 0xf60000);
	}
	/*ary add for switch to post wr, can't display*/
	dbg_pl("dimh_disable_post_deinterlace_2\n");
	dimh_disable_post_deinterlace_2();
	/* nr/blend0/ei0/mtn0 clock gate */

	dim_hw_disable(dimp_get(edi_mp_mcpre_en));

	if (is_meson_txlx_cpu() ||
	    is_meson_txhd_cpu()	||
	    is_meson_g12a_cpu() ||
	    is_meson_g12b_cpu()	||
	    is_meson_tl1_cpu()	||
	    is_meson_tm2_cpu()	||
	    DIM_IS_IC(T5)	||
	    DIM_IS_IC(T5DB)	||
	    DIM_IS_IC(T5D)	||
	    is_meson_sm1_cpu()) {
		dimh_enable_di_post_mif(GATE_OFF);
		dim_post_gate_control(false);
		dim_top_gate_control(false, false);
	} else if (DIM_IS_IC_EF(SC2)) {
		dim_post_gate_control_sc2(false);
		dim_top_gate_control_sc2(false, false);
	} else {
		DIM_DI_WR(DI_CLKG_CTRL, 0x80000000);
	}
	if (!is_meson_gxl_cpu()	&&
	    !is_meson_gxm_cpu()	&&
	    !is_meson_gxbb_cpu() &&
	    !is_meson_txlx_cpu())
		diext_clk_b_sw(false);
	dbg_pl("%s disable di mirror image.\n", __func__);

	if (dimp_get(edi_mp_post_wr_en)	&&
	     dimp_get(edi_mp_post_wr_support)) {
		/*diwr_set_power_control(0);*/
		hpst_mem_pd_sw(0);
	}
#ifdef HIS_CODE
	if (mirror_disable)
		hpst_vd1_sw(0);

	if (cfgg(HF))
		di_hf_hw_release(0xff);

	get_hw_pre()->pre_top_cfg.d32 = 0;
	get_hw_pst()->last_pst_size = 0;
	disp_frame_count = 0;/* debug only*/
#endif

	/*set clkb to low ratio*/
	if (DIM_IS_IC(T5)	||
	    DIM_IS_IC(T5DB)	||
	    DIM_IS_IC(T5D)	||
	    DIM_IS_IC(T3)) {
		#ifdef CLK_TREE_SUPPORT
		if (dimp_get(edi_mp_clock_low_ratio))
			clk_set_rate(de_devp->vpu_clkb,
				     dimp_get(edi_mp_clock_low_ratio));
		#endif
	}
	dbg_plink1("%s:end\n", __func__);
}

/* use this for coverity */
#define FIX_MC_PRE_EN	(0) //for mc_pre_en
#define FIX_MADI_EN	(0)	//madi_en
/*copy from di_reg_setting */
static void dpvpph_reg_setting(const struct reg_acc *op_in)
{
	struct di_dev_s *de_devp = get_dim_de_devp();

	dbg_plink1("%s:\n", __func__);

	diext_clk_b_sw(true);

	if (1/*pst_wr_en*/)
		dim_set_power_control(1);

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXLX)) {
		/*if (!use_2_interlace_buff) {*/
		if (1) {
			if (DIM_IS_IC_EF(SC2))
				dim_top_gate_control_sc2(true, true);
			else
				dim_top_gate_control(true, true);
			/*dim_post_gate_control(true);*/
			/* freerun for reg configuration */
			/*dimh_enable_di_post_mif(GATE_AUTO);*/
			hpst_power_ctr(true);
		} else {
			dim_top_gate_control(true, false);
		}
		de_devp->flags |= DI_VPU_CLKB_SET;
		/*set clkb to max */
		if (is_meson_g12a_cpu()	||
		    is_meson_g12b_cpu()	||
		    is_meson_tl1_cpu()	||
		    is_meson_tm2_cpu()	||
		    DIM_IS_IC(T5)	||
		    DIM_IS_IC(T5DB)	||
		    DIM_IS_IC(T5D)	||
		    is_meson_sm1_cpu()	||
		    DIM_IS_IC_EF(SC2)) {
			#ifdef CLK_TREE_SUPPORT
			clk_set_rate(de_devp->vpu_clkb,
				     de_devp->clkb_max_rate);
			#endif
		}

		dimh_enable_di_pre_mif(false, FIX_MC_PRE_EN);
		if (DIM_IS_IC_EF(SC2))
			dim_pre_gate_control_sc2(true,
						 FIX_MC_PRE_EN);
		else
			dim_pre_gate_control(true, FIX_MC_PRE_EN);
		dim_pre_gate_control(true, FIX_MC_PRE_EN);

		/*2019-01-22 by VLSI feng.wang*/
		//
		if (DIM_IS_IC_EF(SC2)) {
			if (opl1()->wr_rst_protect)
				opl1()->wr_rst_protect(true);
		} else {
			dim_rst_protect(true);
		}

		dim_pre_nr_wr_done_sel(true);
		if (nr_op() && nr_op()->nr_gate_control)
			nr_op()->nr_gate_control(true, op_in);
	} else {
		/* if mcdi enable DI_CLKG_CTRL should be 0xfef60000 */
		DIM_DI_WR(DI_CLKG_CTRL, 0xfef60001);
		/* nr/blend0/ei0/mtn0 clock gate */
	}
	/*--------------------------*/
	dim_init_setting_once();
	/*--------------------------*/
	/*di_post_reset();*/ /*add by feijun 2018-11-19 */
	if (DIM_IS_IC_EF(SC2)) {
		opl1()->pst_mif_sw(false, DI_MIF0_SEL_PST_ALL);
		opl1()->pst_dbg_contr();
	} else {
		post_mif_sw(false);
		post_dbg_contr();
	}

	/*--------------------------*/
	/*--------------------------*/
	dimh_calc_lmv_init();
	/*--------------------------*/
	/*test*/
	dimh_int_ctr(0, 0, 0, 0, 0, 0);
	//dim_ddbg_mod_save(EDI_DBG_MOD_REGE, channel, 0);
	sc2_dbg_set(DI_BIT0 | DI_BIT1);
	/*
	 * enable&disable contwr txt
	 */
	if (DIM_IS_IC_EF(SC2)) {
		op_in->bwr(DI_MTN_1_CTRL1, FIX_MADI_EN ? 5 : 0, 29, 3);
	} else if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12B)) {
		op_in->bwr(DI_MTN_CTRL, FIX_MADI_EN, 29, 1);
		op_in->bwr(DI_MTN_CTRL, FIX_MADI_EN, 31, 1);
	} else {
		op_in->bwr(DI_MTN_1_CTRL1, FIX_MADI_EN ? 5 : 0, 29, 3);
	}
	if (DIM_IS_IC(T5DB))
		dim_afds()->reg_sw_op(true, op_in);
}

/* copy from di_pre_size_change */
static void dpvpph_size_change(struct dim_prevpp_ds_s *ds,
			       struct dimn_dvfm_s *dvfm,
			       const struct reg_acc *op_in)
{
//	unsigned int blkhsize = 0;
	union hw_sc2_ctr_pre_s *sc2_pre_cfg;
//	bool pulldown_en = false; //dimp_get(edi_mp_pulldown_enable)
//	bool mc_en = false;//dimp_get(edi_mp_mcpre_en)
	const struct reg_acc *op;
	unsigned int width, height, vf_type; //ori input para
	struct nr_cfg_s cfg_data;
	struct nr_cfg_s *cfg = &cfg_data;
	struct dim_pvpp_hw_s *hw;

	hw = &get_datal()->dvs_prevpp.hw;

	width	= hw->dis_c_para.win.x_size;
	height	= hw->dis_c_para.win.y_size;
	vf_type = 0; //for p
	op = op_in;
	cfg->width = width;
	cfg->height = height;
	cfg->linkflag = 1;

	if (!op)
		op = &di_pre_regset;

	/* need check if write register. */
	if (nr_op() && !(ds->en_dbg_off_nr & DI_BIT7))
		nr_op()->nr_all_config(vf_type, op, cfg);
#ifdef HIS_CODE
	if (pulldown_en) { //dimp_get(edi_mp_pulldown_enable)
		/*pulldown_init(width, height);*/
		get_ops_pd()->init(width, height);
		dimh_init_field_mode(height);

		if (is_meson_txl_cpu()	||
		    is_meson_txlx_cpu() ||
		    is_meson_gxlx_cpu() ||
		    is_meson_txhd_cpu() ||
		    is_meson_g12a_cpu() ||
		    is_meson_g12b_cpu() ||
		    is_meson_tl1_cpu()	||
		    is_meson_tm2_cpu()	||
		    DIM_IS_IC(T5)	||
		    DIM_IS_IC(T5DB)	||
		    DIM_IS_IC(T5D)	||
		    is_meson_sm1_cpu()	||
		    DIM_IS_IC_EF(SC2))
			dim_film_mode_win_config(width, height);
	}

	if (0) { //?? ary for p no need??
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXL))
			dimh_combing_pd22_window_config(width, height);
	}
#endif /* pulldown_en */
	op->wr(DI_PRE_SIZE, (width - 1) | ((height - 1) << 16));
	dim_print("%s:DI_PRE_SIZE:%d,%d:\n", __func__, width, height);
#ifdef HIS_CODE
	if (mc_en) { //dimp_get(edi_mp_mcpre_en)
		blkhsize = (width + 4) / 5;
		op->wr(MCDI_HV_SIZEIN, height
			| (width << 16));
		op->wr(MCDI_HV_BLKSIZEIN, (dim_get_overturn() ? 3 : 0) << 30
			| blkhsize << 16 | height);
		op->wr(MCDI_BLKTOTAL, blkhsize * height);
		if (is_meson_gxlx_cpu()) {
			op->wr(MCDI_PD_22_CHK_FLG_CNT, 0);
			op->wr(MCDI_FIELD_MV, 0);
		}
	}
#endif
	// below is no use, tmp
#ifdef HIS_CODE
	if (de_devp->nrds_enable)
		dim_nr_ds_init(width, height);
	if (de_devp->pps_enable	&& dimp_get(edi_mp_pps_position)) {
		pps_w = ppre->cur_width;
		if (vf_type & VIDTYPE_TYPEMASK) {
			pps_h = ppre->cur_height >> 1;
			dim_pps_config(1, pps_w, pps_h,
				       dimp_get(edi_mp_pps_dstw),
				       (dimp_get(edi_mp_pps_dsth) >> 1));
		} else {
			pps_h = ppre->cur_height;
			dim_pps_config(1, pps_w, pps_h,
				       dimp_get(edi_mp_pps_dstw),
				       (dimp_get(edi_mp_pps_dsth)));
		}
	}
#endif
	/************************************/
	#ifdef HIS_CODE // hsc temp disable
	if (is_meson_sm1_cpu() || is_meson_tm2_cpu()	||
	    DIM_IS_IC(T5)	||
	    DIM_IS_IC(T5DB)	||
	    DIM_IS_IC(T5D)	||
	    DIM_IS_IC_EF(SC2)) {
		if (de_devp->h_sc_down_en) {
			pps_w = ppre->cur_width;
			tmp = di_mp_uit_get(edi_mp_pre_hsc_down_width);
			dim_inp_hsc_setting(pps_w, tmp);
		} else {
			dim_inp_hsc_setting(ppre->cur_width,
					    ppre->cur_width);
		}
	}
	#endif

	/*dimh_int_ctr(0, 0, 0, 0, 0, 0);*/
	if (1/*hw->en_pst_wr_test*/)
		dimh_int_ctr(1, /*ppre->madi_enable*/0,
		     dimp_get(edi_mp_det3d_en) ? 1 : 0,
		     /*de_devp->nrds_enable*/0,
		     /*dimp_get(edi_mp_post_wr_en)*/1,
		     /*ppre->mcdi_enable*/0);

	if (DIM_IS_IC_EF(SC2)) {
		//for crash test if (!ppre->cur_prog_flag)
		dim_pulldown_info_clear_g12a(hw->op);
		dim_sc2_afbce_rst(0, op);
		dbg_mem2("pre reset-----\n");

		sc2_pre_cfg = &ds->pre_top_cfg;//&get_hw_pre()->pre_top_cfg;
		dim_print("%s:cfg[%px]:inp[%d]\n", __func__,
			  sc2_pre_cfg, sc2_pre_cfg->b.afbc_inp);

		sc2_pre_cfg->b.is_4k	= 0; /*temp*/
		sc2_pre_cfg->b.nr_ch0_en	= 1;
		if (!hw->en_pst_wr_test || dim_is_pre_link_l())
			sc2_pre_cfg->b.pre_frm_sel = 2;
		else
			sc2_pre_cfg->b.pre_frm_sel = 0;
	}
}

#define FIX_CHAN2_DISABLE	(1) //chan2_disable
/* copy from dimh_enable_di_pre_aml */
static void dpvpph_enable_di_pre_aml(const struct reg_acc *op_in,
			    bool pw_en, /* fix 1 for p vpp link ?*/
			    unsigned char pre_vdin_link,
			    /* fix 0 ? and bypass mem ?*/
			    void *pre)
{
	bool mem_bypass = false;
//	bool chan2_disable = false; use FIX_CHAN2_DISABLE
	unsigned int sc2_tfbf = 0; /* DI_PRE_CTRL bit [12:11] */
	unsigned int pd_check = 0; /* DI_PRE_CTRL bit [3:2] */
	unsigned int pre_link_en = 0;
//	unsigned int madi_en = 0;	//use FIX_MADI_EN
	unsigned int pre_field_num = 1;
//	unsigned int tmpk;
	u32 val;

	if (!pw_en || dim_is_pre_link_l())
		pre_link_en = 1;

	mem_bypass = (pre_vdin_link & 0x30) ? true : false;

	pre_vdin_link &= 0xf;

	/*bit 12:11*/
	if (DIM_IS_IC_EF(SC2)) {
		sc2_tfbf = 2;
	} else if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A)) {
		sc2_tfbf = 1;
		if (pre_link_en)
			sc2_tfbf |= DI_BIT1;
	} else {
		sc2_tfbf = 0;
	}

	pd_check = 1;
#ifdef AUTO_NR_DISABLE
	pd_check = FIX_MADI_EN;
#endif
	/*
	 * the bit define is not same with before ,
	 * from sc2 DI_PRE_CTRL 0x1700,
	 * bit5/6/8/9/10/11/12
	 * bit21/22 chan2 t/b reverse,check with vlsi feijun
	 */
#ifdef HIS_CODE
	tmpk = 1			| /* nr wr en */
		    (FIX_MADI_EN << 1)	| /* mtn en */
		    (pd_check << 2)	| /* check3:2pulldown*/
		    (pd_check << 3)	| /* check2:2pulldown*/
		    (1 << 4)		|
		    (FIX_MADI_EN << 5)	| /*hist check enable*/
		/* hist check  use chan2. */
		    (FIX_MADI_EN << 6)	|
		/*hist check use data before noise reduction.*/
		    ((FIX_CHAN2_DISABLE ? 0 : 1) << 8) |
		/* chan 2 enable for 2:2 pull down check.*/
		/* line buffer 2 enable */
		    ((FIX_CHAN2_DISABLE ? 0 : 1) << 9) |
		    (0 << 10)		| /* pre drop first. */
		    //(1 << 11)		| /* nrds mif enable */
		    (sc2_tfbf << 11)	| /* nrds mif enable */
	   /* (pre_link_en << 12)		| *//* pre viu link */
		    (pre_vdin_link << 13)	   |
		/* pre go line link */
		    (pre_vdin_link << 14)	   |
		    (1 << 21)		| /*invertNRfield num*/
		    (1 << 22)		| /* MTN after NR. */
		    (0 << 25)		| /* contrd en */
		    ((mem_bypass ? 1 : 0) << 28)   |
		    pre_field_num << 29;
	dim_print("%s:tmpk=0x%x:0x%x, 0x%x,0x%x,0x%x,0x%x\n",
		__func__, tmpk,
		FIX_MADI_EN,
		FIX_CHAN2_DISABLE,
		sc2_tfbf,
		pre_vdin_link,
		pre_field_num);
#endif
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A)) {
		//chan2_disable = true;
		//opl1()->pre_gl_thd();
		if (DIM_IS_IC_EF(SC2))
			op_in->bwr(DI_SC2_PRE_GL_THD,
				dimp_get(edi_mp_prelink_hold_line), 16, 6);
		else	/*from feijun's for pre-vpp link */
			op_in->bwr(DI_PRE_GL_THD, 8, 16, 6);
		if (dimp_get(edi_mp_pre_ctrl)) {
			op_in->bwr(DI_PRE_CTRL,
				   dimp_get(edi_mp_pre_ctrl), 0, 29);
			dim_print("%s:a\n", __func__);
		} else {
			if (DIM_IS_IC_EF(SC2)) {
				op_in->wr(DI_PRE_CTRL,
					    1 | /* nr wr en */
					    (FIX_MADI_EN << 1) | /* mtn en */
					    (pd_check << 2) |
					    /*check3:2pulldown*/
					    (pd_check << 3) |
					    /*check2:2pulldown*/
					    (1 << 4)	 |
					    (FIX_MADI_EN << 5) |
					    /*hist check enable*/
					    (1 << 6)	| /* MTN after NR. */
					    (1 << 8) | /* mem en. */
						/* chan 2 enable */
					    ((FIX_CHAN2_DISABLE ? 0 : 1) << 9) |
					    (sc2_tfbf << 11)	|
					    /* nrds mif enable */
					    (pre_vdin_link << 13)	   |
					/* pre go line link */
					    (pre_vdin_link << 14)	   |
					    (1 << 21)	| /*chan2 t/b reverse*/
					    (0 << 25)   |
					    /* contrd en */
					    ((mem_bypass ? 1 : 0) << 28)   |
					    pre_field_num << 29);
				val = op_in->rd(DI_TOP_PRE_CTRL);
				val &= ~(0xff << 12);
				if (mem_bypass)
					val |= (0x81 << 12);
				op_in->wr(DI_TOP_PRE_CTRL, val);
				dim_print("%s:b:%d\n", __func__, pre_field_num);
			} else {
				op_in->wr(DI_PRE_CTRL,
					  1		 | /* nr wr en */
					  (FIX_MADI_EN << 1) | /* mtn en */
					  (pd_check << 2)	| /* check3:2pulldown*/
					  (pd_check << 3)	| /* check2:2pulldown*/
					  (1 << 4)	 |
					  (FIX_MADI_EN << 5) | /*hist check enable*/
				/* hist check  use chan2. */
					  (FIX_MADI_EN << 6) |
				/*hist check use data before noise reduction.*/
					  ((FIX_CHAN2_DISABLE ? 0 : 1) << 8) |
				/* chan 2 enable for 2:2 pull down check.*/
				/* line buffer 2 enable */
					  ((FIX_CHAN2_DISABLE ? 0 : 1) << 9) |
					  (0 << 10)	| /* pre drop first. */
					  //(1 << 11)	| /* nrds mif enable */
					  (sc2_tfbf << 11) | /* nrds mif enable */
				   /* (pre_link_en << 12)  | *//* pre viu link */
					  (pre_vdin_link << 13) |
					/* pre go line link */
					  (pre_vdin_link << 14)	|
					  (1 << 21)	| /*invertNRfield num*/
					  (1 << 22)	| /* MTN after NR. */
					  (0 << 25)	| /* contrd en */
					  ((mem_bypass ? 1 : 0) << 28)   |
					  pre_field_num << 29);
				dim_print("%s:c:%d\n", __func__, pre_field_num);
			}
		}
	} else {
		op_in->wr(DI_PRE_CTRL,
			    1			/* nr enable */
			    | (FIX_MADI_EN << 1)	/* mtn_en */
			    | (FIX_MADI_EN << 2)	/* check 3:2 pulldown */
			    | (FIX_MADI_EN << 3)	/* check 2:2 pulldown */
			    | (1 << 4)
			    | (FIX_MADI_EN << 5)	/* hist check enable */
			    | (1 << 6)		/* hist check  use chan2. */
			    | (0 << 7)
			    /* hist check use data before noise reduction. */
			    | (FIX_MADI_EN << 8)
			    /* chan 2 enable for 2:2 pull down check.*/
			    | (FIX_MADI_EN << 9)	/* line buffer 2 enable */
			    | (0 << 10)		/* pre drop first. */
			    | (sc2_tfbf << 11)		/* di pre repeat */
			    | (0 << 12)		/* pre viu link */
			    | (pre_vdin_link << 13)
			    | (pre_vdin_link << 14)	/* pre go line link */
			    | (dimp_get(edi_mp_prelink_hold_line) << 16)
			    /* pre hold line number */
			    | (1 << 22)		/* MTN after NR. */
			    | (FIX_MADI_EN << 25)	/* contrd en */
			    | (pre_field_num << 29)	/* pre field number.*/
			    );
	}
}

/* ary move to di_hw_v2.c*/
/*
 * enable/disable inp&chan2&mem&nrwr mif
 * copy from di_pre_data_mif_ctrl
 */
static void dpvpph_pre_data_mif_ctrl(bool enable, const struct reg_acc *op_in)
{
	if (enable) {
		/*****************************************/
		if (dim_afds() && dim_afds()->is_used())
			dim_afds()->inp_sw_op(true, op_in);
		if (dim_afds() && !dim_afds()->is_used_chan2())
			op_in->bwr(DI_CHAN2_GEN_REG, 1, 0, 1);

		if (dim_afds() && !dim_afds()->is_used_mem())
			op_in->bwr(DI_MEM_GEN_REG, 1, 0, 1);

		if (dim_afds() && !dim_afds()->is_used_inp())
			op_in->bwr(DI_INP_GEN_REG, 1, 0, 1);

		/* nrwr no clk gate en=0 */
		/*DIM_RDMA_WR_BITS(DI_NRWR_CTRL, 0, 24, 1);*/
	} else {
		/* nrwr no clk gate en=1 */
		/*DIM_RDMA_WR_BITS(DI_NRWR_CTRL, 1, 24, 1);*/
		/* nr wr req en =0 */
		op_in->bwr(DI_PRE_CTRL, 0, 0, 1);
		/* disable input mif*/
		op_in->bwr(DI_CHAN2_GEN_REG, 0, 0, 1);
		op_in->bwr(DI_MEM_GEN_REG, 0, 0, 1);
		op_in->bwr(DI_INP_GEN_REG, 0, 0, 1);

		/* disable AFBC input */
		//if (afbc_is_used()) {
		#ifdef HIS_CODE //ary temp
		if (dim_afds() && dim_afds()->is_used()) {
			//afbc_input_sw(false);
			if (dim_afds())
				dim_afds()->inp_sw_op(false, op_in);
		}
		#endif
	}
}

/*
 * reverse_mif control mirror mode
 * copy from dim_post_read_reverse_irq
 */

static void dpvpph_reverse_mif_ctrl(bool reverse, unsigned int hv_mirror,
		const struct reg_acc *op_in)
{
	const unsigned int *reg;
	unsigned int reg_addr;
	unsigned int mif_reg_addr;
	const unsigned int *mif_reg;

	reg = afbc_get_inp_base();
	mif_reg = mif_reg_get_v3();
	if (!mif_reg || !reg)
		return;

	reg_addr = reg[EAFBC_MODE];
	mif_reg_addr = mif_reg[MIF_GEN_REG2];
	if (DIM_IS_IC(T5DB) || DIM_IS_IC(T5))
		mif_reg_addr = DI_INP_GEN_REG2;

	if (reverse) {
		op_in->bwr(mif_reg_addr, 3, 2, 2);
		op_in->bwr(reg_addr, 3, 26, 2);//AFBC_MODE
	} else if (hv_mirror == 1) {
		op_in->bwr(mif_reg_addr,  1, 2, 1);
		op_in->bwr(mif_reg_addr,  0, 3, 1);
		op_in->bwr(reg_addr, 1, 26, 1);//AFBC_MODE
		op_in->bwr(reg_addr, 0, 27, 1);//AFBC_MODE
	} else if (hv_mirror == 2) {
		op_in->bwr(mif_reg_addr,  0, 2, 1);
		op_in->bwr(mif_reg_addr,  1, 3, 1);
		op_in->bwr(reg_addr, 0, 26, 1);//AFBC_MODE
		op_in->bwr(reg_addr, 1, 27, 1);//AFBC_MODE
	} else {
		op_in->bwr(mif_reg_addr,  0, 2, 2);
		op_in->bwr(reg_addr, 0, 26, 2);//AFBC_MODE
	}
}

/* copy from hpre_gl_sw and hpre_gl_sw_v3 */
static void dpvpph_gl_sw(bool on, bool pvpp_link, const struct reg_acc *op)
{
	if (!cpu_after_eq(MESON_CPU_MAJOR_ID_G12A))
		return;
	if (DIM_IS_IC_BF(SC2)) {
		if (pvpp_link) {
			if (op->rd(DI_PRE_GL_CTRL) & 0x80000000)
				op->bwr(DI_PRE_GL_CTRL, 0, 31, 1);
			return;
		}
		if (on)
			op->wr(DI_PRE_GL_CTRL,
			       0x80200000 | dimp_get(edi_mp_line_num_pre_frst));
		else
			op->wr(DI_PRE_GL_CTRL, 0xc0000000);
	} else {
		if (pvpp_link) {
			if (op->rd(DI_SC2_PRE_GL_CTRL) & 0x80000000)
				op->bwr(DI_SC2_PRE_GL_CTRL, 0, 31, 1);
			return;
		}
		if (on)
			op->wr(DI_SC2_PRE_GL_CTRL,
			       0x80200000 | dimp_get(edi_mp_line_num_pre_frst));
		else
			op->wr(DI_SC2_PRE_GL_CTRL, 0xc0000000);
	}
}

static void dpvpph_gl_disable(const struct reg_acc *op) //test-05
{
	if (!cpu_after_eq(MESON_CPU_MAJOR_ID_G12A))
		return;
	if (DIM_IS_IC_BF(SC2)) {
		op->wr(DI_PRE_GL_CTRL, 0xc0000000);
		op->wr(DI_POST_GL_CTRL, 0xc0000000);	//test-06
	} else {
		op->wr(DI_SC2_PRE_GL_CTRL, 0xc0000000);
		op->wr(DI_SC2_POST_GL_CTRL, 0xc0000000);
		return;
	}
}

/* copy from dim_pre_frame_reset_g12 */
static void dpvpph_pre_frame_reset_g12(bool pvpp_link,
				       const struct reg_acc *op_in)
{
	if (!cpu_after_eq(MESON_CPU_MAJOR_ID_G12A))
		return;
	if (DIM_IS_IC_BF(SC2)) {
		if (pvpp_link) {
			if (op_in->rd(DI_PRE_GL_CTRL) & 0x80000000)
				op_in->bwr(DI_PRE_GL_CTRL, 0, 31, 1);
			return;
		}
	} else {
		if (pvpp_link) {
			if (op_in->rd(DI_SC2_PRE_GL_CTRL) & 0x80000000)
				op_in->bwr(DI_SC2_PRE_GL_CTRL, 0, 31, 1);
			return;
		}
	}
}

/*copy from dim_pre_frame_reset */
void dpvpph_pre_frame_reset(const struct reg_acc *op_in)
{
	op_in->bwr(DI_PRE_CTRL, 3, 30, 2);
}

static void dpvpph_pre_to_link(void)
{
	const struct reg_acc *op;

	op = get_datal()->dvs_prevpp.hw.op_n;
	if (DIM_IS_IC_BF(SC2) && DIM_IS_IC_EF(G12A)) {
		op->bwr(DI_PRE_CTRL, 1, 12, 1);
		dbg_plink1("set:DI_PRE_CTRL bit 12\n");
	} else if (DIM_IS_IC_EF(SC2)) {
		op->bwr(DI_TOP_PRE_CTRL, 2, 30, 2);// pre viu link, use viu vsync as frame reset
		op->bwr(DI_TOP_CTRL, 1, 0, 1);// 1:pre link vpp  0:post link vpp
		dbg_plink1("set:DI_TOP_CTRL bit 0\n");
	}
}

#ifdef HIS_CODE
static void dpvpph_pre_to_link(void)
{
	const struct reg_acc *op;

	if (DIM_IS_IC_BF(SC2) && DIM_IS_IC_EF(G12A)) {
		op = get_datal()->dvs_prevpp.hw.op_n;
		op->bwr(DI_PRE_CTRL, 1, 12, 1);
		dbg_plink1("set:bit 12\n");
	}
}
#endif

//for t5d vb ?
static void dpvpph_nr_ddr_switch(bool sw, const struct reg_acc *op)
{
	op->bwr(DI_AFBCE_CTRL, sw, 4, 1);
}

/************************************************
 * from config_di_mif_v3
 *	para[0:2]: cvs id;
 *	para[3]:
 *2021-09-26: copy from mif_cfg in prepost_link.c
 *	change:delete di_buf;
 *	add win
 ************************************************/
static void mif_cfg_v2(struct DI_MIF_S *di_mif,
		    struct dvfm_s *pvfm,
		    struct dim_mifpara_s *ppara)
{
	enum DI_MIF0_ID mif_index = 0;//di_mif->mif_index;
	struct di_win_s *win;
	//struct dvfm_s *pvfm;

	if (!di_mif || !pvfm || !ppara)
		return;
	dim_print("%s:%s:\n",
		__func__,
		di_mif->name);
	mif_index = di_mif->mif_index;
	win = &ppara->win;
	if (pvfm->is_dec)
		di_mif->in_dec = 1;
	else
		di_mif->in_dec = 0;

	di_mif->canvas0_addr0 =
		ppara->cvs_id[0] & 0xff;
	di_mif->canvas0_addr1 =	ppara->cvs_id[1];
	di_mif->canvas0_addr2 = ppara->cvs_id[2];
	if (pvfm->is_linear) {
		di_mif->addr0 = pvfm->vfs.canvas0_config[0].phy_addr;
		di_mif->cvs0_w = pvfm->vfs.canvas0_config[0].width;
		di_mif->cvs1_w = 0;
		di_mif->cvs2_w = 0;
		if (pvfm->vfs.plane_num >= 2) {
			di_mif->addr1 = pvfm->vfs.canvas0_config[1].phy_addr;
			di_mif->cvs1_w = pvfm->vfs.canvas0_config[1].width;
		}
		if (pvfm->vfs.plane_num >= 3) {
			di_mif->addr2 = pvfm->vfs.canvas0_config[2].phy_addr;
			di_mif->cvs2_w = pvfm->vfs.canvas0_config[2].width;
		}
		di_mif->linear = 1;
		di_mif->buf_crop_en = 1;
		di_mif->linear_fromcvs = 1;//new for pre-post link
		if (pvfm->vfs.plane_num == 2) {
			di_mif->buf_hsize = pvfm->vfs.canvas0_config[0].width;
			dbg_ic("\t:buf_h[%d]\n", di_mif->buf_hsize);
		} else {
			//di_mif->buf_crop_en = 0;
			dbg_ic("\t:not nv21?\n");
			di_mif->buf_hsize = pvfm->buf_hsize;
		}

		if (pvfm->vfs.canvas0_config[0].block_mode)
			di_mif->block_mode = 1;
		else
			di_mif->block_mode = 0;
	} else {
		di_mif->linear = 0;
	}

	di_mif->burst_size_y	= 3;
	di_mif->burst_size_cb	= 1;
	di_mif->burst_size_cr	= 1;
	di_mif->hold_line = dimp_get(edi_mp_prelink_hold_line);
	di_mif->urgent	= dimp_get(edi_mp_pre_urgent);
	di_mif->canvas_w = pvfm->vfs.canvas0_config[0].width;

	if (pvfm->vfs.type & VIDTYPE_VIU_NV12)
		di_mif->cbcr_swap = 1;
	else
		di_mif->cbcr_swap = 0;
	//if (ppara->linear) {
	if (pvfm->is_in_linear) {
		di_mif->reg_swap = 0;
		di_mif->l_endian = 1;

	} else {
		di_mif->reg_swap = 1;
		di_mif->l_endian = 0;
		//di_mif->cbcr_swap = 0;
	}
	dim_print("%s:%s:endian[%d]\n",
		  __func__, di_mif->name, di_mif->l_endian);
	if (pvfm->vfs.bitdepth & BITDEPTH_Y10) {
		if (pvfm->vfs.type & VIDTYPE_VIU_444)
			di_mif->bit_mode =
			(pvfm->vfs.bitdepth & FULL_PACK_422_MODE) ?
			3 : 2;
		else if (pvfm->vfs.type & VIDTYPE_VIU_422)
			di_mif->bit_mode =
			(pvfm->vfs.bitdepth & FULL_PACK_422_MODE) ?
			3 : 1;
	} else {
		di_mif->bit_mode = 0;
	}
	if (pvfm->vfs.type & VIDTYPE_VIU_422) {
		/* from vdin or local vframe */
		/* 1. from vdin input mif */
		/* 2. from local mif */
		//if ((!IS_PROG(pvfm->type))	||
		//    ppara->prog_proc_type) {
		di_mif->video_mode = 1;
		di_mif->set_separate_en = 0;
		di_mif->src_field_mode = 0;
		di_mif->output_field_num = 0;
		di_mif->luma_x_start0	= win->x_st;
		di_mif->luma_x_end0	= win->x_end;
		di_mif->luma_y_start0	= win->y_st;
		if (IS_PROG(pvfm->vfs.type)) //
			di_mif->luma_y_end0 = win->y_end;
		else
			di_mif->luma_y_end0 = win->y_size / 2 - 1;

		di_mif->chroma_x_start0 = 0;
		di_mif->chroma_x_end0 = 0;
		di_mif->chroma_y_start0 = 0;
		di_mif->chroma_y_end0 = 0;

		//}
		di_mif->reg_swap = 1;
		di_mif->l_endian = 0;
		di_mif->cbcr_swap = 0;
	} else {
		if (pvfm->vfs.type & VIDTYPE_VIU_444)
			di_mif->video_mode = 2;
		else
			di_mif->video_mode = 0;
		if (pvfm->vfs.type &
		    (VIDTYPE_VIU_NV21 | VIDTYPE_VIU_NV12))
			di_mif->set_separate_en = 2;
		else
			di_mif->set_separate_en = 1;
		dim_print("%s:%d:vtype[0x%x]\n",
			  __func__, mif_index, pvfm->vfs.type);
		if (IS_PROG(pvfm->vfs.type) && ppara->prog_proc_type) {
			di_mif->src_field_mode = 0;
			di_mif->output_field_num = 0; /* top */
			di_mif->luma_x_start0	= win->x_st;
			di_mif->luma_x_end0 = win->x_end;
			di_mif->luma_y_start0 = win->y_st;
			di_mif->luma_y_end0 = win->y_end;
			di_mif->chroma_x_start0 = win->x_st / 2;
			di_mif->chroma_x_end0 = di_mif->chroma_x_start0 +
					win->x_size / 2 - 1;
			di_mif->chroma_y_start0 = win->y_st / 2;
			di_mif->chroma_y_end0 = di_mif->chroma_y_start0 +
				(win->y_size + 1) / 2 - 1;
		} else if ((ppara->cur_inp_type & VIDTYPE_INTERLACE) &&
				(ppara->cur_inp_type & VIDTYPE_VIU_FIELD)) {
			di_mif->src_prog = 0;
			di_mif->src_field_mode = 0;
			di_mif->output_field_num = 0; /* top */
			di_mif->luma_x_start0 = win->x_st;
			di_mif->luma_x_end0 = win->x_end;
			di_mif->luma_y_start0 = win->y_st;
			di_mif->luma_y_end0 = win->y_size / 2 - 1;
			di_mif->chroma_x_start0 = win->x_st / 2;
			di_mif->chroma_x_end0 = di_mif->chroma_x_start0 +
					win->x_size / 2 - 1;
			di_mif->chroma_y_start0 = win->y_st / 4;
			di_mif->chroma_y_end0 = di_mif->chroma_y_start0 +
				win->y_size / 4 - 1;
		} else {
			/*move to mp	di_mif->src_prog = force_prog?1:0;*/
			if (ppara->cur_inp_type  & VIDTYPE_INTERLACE)
				di_mif->src_prog = 0;
			else
				di_mif->src_prog =
				dimp_get(edi_mp_force_prog) ? 1 : 0;
			di_mif->src_field_mode = 1;
			if ((pvfm->vfs.type & VIDTYPE_TYPEMASK) ==
			    VIDTYPE_INTERLACE_TOP) {
				di_mif->output_field_num = 0; /* top */
				di_mif->luma_x_start0 = win->x_st;
				di_mif->luma_x_end0 = win->x_end;

				di_mif->luma_y_start0 = win->y_st;
				di_mif->luma_y_end0 = win->y_end; //??
				di_mif->chroma_x_start0 = win->x_st / 2;
				di_mif->chroma_x_end0 = di_mif->chroma_x_start0 +
					win->x_size / 2 - 1;
				di_mif->chroma_y_start0 = win->y_st / 2;
				di_mif->chroma_y_end0 = di_mif->chroma_y_start0 +
					(win->y_size + 1) / 2 - 1;
			} else {
				di_mif->output_field_num = 1;
				/* bottom */
				di_mif->luma_x_start0 = win->x_st;
				di_mif->luma_x_end0 = win->x_end;
				di_mif->luma_y_start0 = win->y_st + 1;
				di_mif->luma_y_end0 = win->y_end;
				di_mif->chroma_x_start0 = win->x_st;
				di_mif->chroma_x_end0 = di_mif->chroma_x_start0 +
					win->x_size / 2 - 1;
				di_mif->chroma_y_start0 =
					(di_mif->src_prog ? win->y_st : win->y_st + 1);
				di_mif->chroma_y_end0 = di_mif->chroma_y_start0 +
					(win->y_size + 1) / 2 - 1;
			}
		}
	}
	dim_print("\t%s:end\n", __func__);
}

/************************************************
 * from config_di_mif_v3
 *	para[0:2]: cvs id;
 *	para[3]:
 *2021-09-26: copy from mif_cfg in prepost_link.c
 *	change:delete di_buf;
 *	add win
 ************************************************/
static void mif_cfg_v2_update_addr(struct DI_MIF_S *di_mif,
		    struct dvfm_s *pvfm,
		    struct dim_mifpara_s *ppara)
{

	if (!di_mif || !pvfm || !ppara)
		return;

	di_mif->canvas0_addr0 =
		ppara->cvs_id[0] & 0xff;
	di_mif->canvas0_addr1 =	ppara->cvs_id[1];
	di_mif->canvas0_addr2 = ppara->cvs_id[2];
	if (pvfm->is_linear) {
		di_mif->addr0 = pvfm->vfs.canvas0_config[0].phy_addr;
		di_mif->cvs0_w = pvfm->vfs.canvas0_config[0].width;

		if (pvfm->vfs.plane_num >= 2) {
			di_mif->addr1 = pvfm->vfs.canvas0_config[1].phy_addr;
			di_mif->cvs1_w = pvfm->vfs.canvas0_config[1].width;
		}
		if (pvfm->vfs.plane_num >= 3) {
			di_mif->addr2 = pvfm->vfs.canvas0_config[2].phy_addr;
			di_mif->cvs2_w = pvfm->vfs.canvas0_config[2].width;
		}
	}
}

static unsigned int dpvpp_is_bypass_dvfm(struct dvfm_s *dvfm, bool en_4k)
{
	struct dim_pvpp_hw_s *hw;

	hw = &get_datal()->dvs_prevpp.hw;
	if (VFMT_IS_I(dvfm->vfs.type))
		return EPVPP_BYPASS_REASON_I;
	if (VFMT_IS_EOS(dvfm->vfs.type))
		return EPVPP_BYPASS_REASON_EOS;
	if (dvfm->vfs.type & DIM_BYPASS_VF_TYPE)
		return EPVPP_BYPASS_REASON_TYPE;
	if (en_4k) {
		if (dvfm->vfs.width > 3840 ||
		    dvfm->vfs.height > 2160)
			return EPVPP_BYPASS_REASON_SIZE_L;
		if (dvfm->is_4k && !hw->blk_rd_uhd)
			return EPVPP_BYPASS_REASON_BUF_UHD;
		else if (!dvfm->is_4k && !hw->blk_rd_hd)
			return EPVPP_BYPASS_REASON_BUF_HD;
	} else {
		if (dvfm->vfs.width > 1920 ||
		    dvfm->vfs.height > 1088)
			return EPVPP_BYPASS_REASON_SIZE_L;
		if (!hw->blk_rd_hd)
			return EPVPP_BYPASS_REASON_BUF_HD;
	}
	if (dvfm->vfs.flag & VFRAME_FLAG_HIGH_BANDWIDTH)
		return EPVPP_BYPASS_REASON_HIGH_BANDWIDTH;
	if (IS_COMP_MODE(dvfm->vfs.type)) {
		if (dim_afds() && !dim_afds()->is_supported_plink())
			return EPVPP_BYPASS_REASON_NO_AFBC;
	}
	if (dvfm->vfs.width < 128 || dvfm->vfs.height < 16)
		return EPVPP_BYPASS_REASON_SIZE_S;
	if (dpvpp_is_bypass())
		return EPVPP_BYPASS_REASON_DBG;
	if (dvfm->vfs.sei_magic_code == SEI_MAGIC_CODE &&
		(dvfm->vfs.fmt == VFRAME_SIGNAL_FMT_DOVI ||
		dvfm->vfs.fmt == VFRAME_SIGNAL_FMT_DOVI_LL))
		return EPVPP_BYPASS_REASON_DV_PATH;
	if (dvfm->vfs.flag & VFRAME_FLAG_GAME_MODE)
		return EPVPP_BYPASS_REASON_GAME_MODE;

	return 0;
}

static void dpvpp_set_type_smp(struct dim_type_smp_s *smp,
					 struct dvfm_s *dvfm)
{
	if (!smp || !dvfm)
		return;
	smp->vf_type = dvfm->vfs.type & VFMT_MASK_ALL;
	smp->x_size	= dvfm->vfs.width;
	smp->y_size	= dvfm->vfs.height;
	//smp->other?
}

static unsigned int dpvpp_is_change_dvfm(struct dim_type_smp_s *in_last,
					 struct dvfm_s *dvfm)
{
	if (in_last->vf_type != (dvfm->vfs.type & VFMT_MASK_ALL))
		return 1;
	if (in_last->x_size != dvfm->vfs.width ||
	    in_last->y_size != dvfm->vfs.height)
		return 2;
	return 0;
}

static void dpvpp_put_ready_vf(struct dimn_itf_s *itf,
				struct dim_prevpp_ds_s *ds,
				struct vframe_s *vfm)
{
	struct dimn_qs_cls_s *qready;
	struct di_buffer *buffer;

	qready	= &ds->lk_que_ready;
	if (itf->etype == EDIM_NIN_TYPE_VFM) {
		qready->ops.put(qready, vfm);
	} else {
		buffer = &itf->buf_bf[vfm->index];
		buffer->flag = 0;
		dbg_plink3("%s:[0x%px:0x%x]:vf[0x%px],%px index:%d\n",
			__func__, buffer, buffer->flag, buffer->vf,
			vfm, vfm->index);
		qready->ops.put(qready, buffer);
	}
}

static bool dpvpp_parser_nr(struct dimn_itf_s *itf)
{
	struct dimn_qs_cls_s *qin;
	struct vframe_s *vfm;
	struct dimn_dvfm_s *ndvfm = NULL;
	struct dvfm_s *in_dvfm, *out_dvfm;
	unsigned int bypass;
	struct dim_cvspara_s *cvsp;
	unsigned int chg;
	int timeout = 0; //for post write only
	struct dim_prevpp_ds_s *ds;
	struct dim_pvpp_hw_s *hw;
	struct dvfm_s *dvfm_dm;
	unsigned int out_fmt;
	struct pvpp_buf_cfg_s *buf_cfg;

	hw = &get_datal()->dvs_prevpp.hw;

	if (!itf || !itf->ds)
		return false;
	ds = itf->ds;
	qin	= &ds->lk_que_in;
	vfm	= (struct vframe_s *)qin->ops.peek(qin);
	if (!vfm)
		return false;
	ndvfm	= (struct dimn_dvfm_s *)vfm->private_data;
	if (!ndvfm) //no input
		return false;
	in_dvfm		= &ndvfm->c.in_dvfm;
	//out_dvfm	= &ndvfm->c.out_dvfm;

	bypass = dpvpp_is_bypass_dvfm(in_dvfm, ds->en_4k);
	ndvfm->c.bypass_reason = bypass;
	if (bypass) {
		if (!itf->c.bypass_reason)
			dbg_plink1("to bypass:%d:%d\n", bypass, ndvfm->c.cnt_in);
		itf->c.bypass_reason = bypass;
		dpvpp_parser_bypass(itf, 1);
		return true;
	}
	//dbg only:
	dpvpp_cnt_para(&ds->dis_para_demo2, &ndvfm->c.vf_in_cp,
			dpvpp_dbg_force_dech,
			dpvpp_dbg_force_decv);
	if (itf->c.bypass_reason)
		dbg_plink1("leave bypass:%d\n", ndvfm->c.cnt_in);
	itf->c.bypass_reason = 0;
	dim_print("%s:nub:%d\n", __func__, ndvfm->c.cnt_in);
	chg = dpvpp_is_change_dvfm(&ds->in_last, in_dvfm);
	dim_print("%s:chg:[%d]\n", __func__, chg);
	dbg_check_ud(ds, 1); /* dbg */
	if (chg) {
		/* debug only */
		if (cfgg(PAUSE_SRC_CHG)) {
			pre_run_flag = DI_RUN_FLAG_PAUSE;
			PR_INF("dbg change:ppause\n");
		}
		/* change */
		PR_INF("source change:ch[%d]:id[%d]:0x%x/%d/%d/%d=>0x%x/%d/%d/%d:%d\n",
				itf->bind_ch,
			       itf->id,
			       ds->in_last.vf_type,
			       ds->in_last.x_size,
			       ds->in_last.y_size,
			       ds->in_last.cur_source_type,
			       in_dvfm->vfs.type,
			       in_dvfm->vfs.width,
			       in_dvfm->vfs.height,
			       in_dvfm->vfs.source_type,
			       ndvfm->c.cnt_in);
		/* reset last_ds_ratio when source size change */
		itf->c.last_ds_ratio = DI_FLAG_DCT_DS_RATIO_MAX;
		ds->set_cfg_cur.b.en_linear_cp = hw->en_linear;
		ds->is_inp_4k = false;
		ds->is_out_4k = false;
		if (!VFMT_IS_EOS(in_dvfm->vfs.type) && in_dvfm->is_4k) {
			ds->is_inp_4k = true;
			ds->is_out_4k = true;
		}
		if (ds->is_inp_4k) {
			ds->o_c.d32 = ds->o_uhd.d32;
			dbg_plink1("is:uhd");
		} else {
			ds->o_c.d32 = ds->o_hd.d32;
		}

		dvfm_dm	= &ds->out_dvfm_demo;
		//memset(out_dvfm, 0, sizeof(*out_dvfm));
		if (ds->o_c.b.mode == EDPST_MODE_NV21_8BIT) { //tmp
			if (ds->en_out_nv12)
				out_fmt = 2;
			else
				out_fmt = 1;
		} else/* if (ds->out_mode == EDPST_MODE_422_10BIT_PACK)*/ {
			out_fmt = 0;
		}
		dbg_plink1("%s:id[%d]:out_fmt[%d]\n", __func__, itf->id, out_fmt);
		dim_dvf_type_p_change(dvfm_dm, out_fmt);

		buf_cfg = get_buf_cfg(ds->o_c.b.out_buf, 5);
		if (!buf_cfg) {
			PR_ERR("no buf_cfg?\n");
			return false;
		}

		if (ds->o_c.b.is_afbce) {
			dvfm_dm->flg_afbce_set = true;
			dvfm_dm->vfs.type |= VIDTYPE_COMPRESS; //tmp
			dvfm_dm->vfs.type |= VIDTYPE_SCATTER;
		} else {
			dvfm_dm->flg_afbce_set = false;
		}

		dvfm_dm->vfs.plane_num = buf_cfg->dbuf_plan_nub;
		dvfm_dm->buf_hsize = buf_cfg->dbuf_hsize;
		memcpy(&dvfm_dm->vfs.canvas0_config[0],
		       &buf_cfg->dbuf_wr_cfg[0][0],
		       sizeof(dvfm_dm->vfs.canvas0_config[0]) << 1);
		if (itf->etype == EDIM_NIN_TYPE_VFM)
			dvfm_dm->is_itf_vfm = 1;
		if (hw->en_pst_wr_test)
			dvfm_dm->is_p_pw	= 1;
		else
			dvfm_dm->is_prvpp_link	= 1;
		/* dvfm_demo end ********************************/
		/* do not care no-support */
		/* cfg seting en */

		if (IS_COMP_MODE(in_dvfm->vfs.type)) {
			ds->set_cfg_cur.b.en_in_afbcd	= true;
			ds->set_cfg_cur.b.en_in_mif	= false;
			//ds->set_cfg_cur.b.en_in_cvs	= false;
			ds->afbc_sgn_cfg.b.inp		= true;
//			ds->afbc_en_cfg.b.inp		= true;
		} else {
			ds->set_cfg_cur.b.en_in_afbcd	= false;
			ds->set_cfg_cur.b.en_in_mif	= true;
			ds->set_cfg_cur.b.en_in_cvs	= true;
			ds->afbc_sgn_cfg.b.inp		= false;
//			ds->afbc_en_cfg.b.inp		= false;
		}
		if (ds->set_cfg_cur.b.en_in_mif && !hw->en_linear)
			ds->set_cfg_cur.b.en_in_cvs	= true;
		if (ds->o_c.b.is_afbce) {//tmp
			ds->set_cfg_cur.b.en_wr_afbce	= true;
			ds->afbc_sgn_cfg.b.mem		= true;
			ds->afbc_sgn_cfg.b.enc_nr	= true;
			ds->set_cfg_cur.b.en_wr_mif	= false; /*tmp*/
		} else {
			ds->set_cfg_cur.b.en_wr_afbce	= false;
			ds->set_cfg_cur.b.en_wr_mif	= true; /*tmp*/
			ds->afbc_sgn_cfg.b.mem		= false;
			ds->afbc_sgn_cfg.b.enc_nr	= false;
		}
		ds->set_cfg_cur.b.en_mem_mif = ds->set_cfg_cur.b.en_wr_mif;
		ds->set_cfg_cur.b.en_mem_afbcd = ds->set_cfg_cur.b.en_wr_afbce;
		if (ds->set_cfg_cur.b.en_wr_mif && !hw->en_linear) {
			ds->set_cfg_cur.b.en_wr_cvs	= true;
			ds->set_cfg_cur.b.en_mem_cvs	= true;
		} else {
			ds->set_cfg_cur.b.en_wr_cvs	= false;
			ds->set_cfg_cur.b.en_mem_cvs	= false;
		}

		/* cfg out dvfm demo */
		ds->out_dvfm_demo.vfs.height	= in_dvfm->vfs.height;
		ds->out_dvfm_demo.vfs.width	= in_dvfm->vfs.width;
		if (itf->etype == EDIM_NIN_TYPE_INS)
			ds->out_dvfm_demo.is_itf_ins_local = true;
		else
			ds->out_dvfm_demo.is_itf_ins_local = false;
		/* dis_para_demo */
		if (1/*hw->en_pst_wr_test*/)
			dpvpp_set_default_para(ds, ndvfm);

		/* afbc */
		ndvfm->c.afbc_sgn_cfg.d8 = ds->afbc_sgn_cfg.d8;

		ds->pre_top_cfg.b.is_inp_4k = ds->is_inp_4k;
		ds->pre_top_cfg.b.is_mem_4k = ds->is_out_4k;

		if (itf->nitf_parm.output_format & DI_OUTPUT_LINEAR)
			ds->out_dvfm_demo.is_in_linear = 1;
		else
			ds->out_dvfm_demo.is_in_linear = 0;

		dim_print("%s:set_cfg:cur:0x%x\n",
			__func__, ds->set_cfg_cur.d32);
		if (ds->out_dvfm_demo.is_in_linear)
			ds->out_dvfm_demo.vfs.flag |= VFRAME_FLAG_VIDEO_LINEAR;
		else
			ds->out_dvfm_demo.vfs.flag &= (~VFRAME_FLAG_VIDEO_LINEAR);

		ds->out_dvfm_demo.vfs.di_flag |= DI_FLAG_DI_PVPPLINK;
		if (cfgg(LINEAR))
			ds->out_dvfm_demo.is_linear = true;
		else
			ds->out_dvfm_demo.is_linear = false;
		//set last information:
		dpvpp_set_type_smp(&ds->in_last, in_dvfm);
		ds->cnt_parser = 0;
	}

	ndvfm->c.set_cfg.d32 = ds->set_cfg_cur.d32;
	ndvfm->c.is_inp_4k = ds->is_inp_4k;
	ndvfm->c.is_out_4k = ds->is_out_4k;
	ndvfm->c.out_fmt = ds->o_c.b.out_buf;

	ndvfm->c.afbc_sgn_cfg.d8 = ds->afbc_sgn_cfg.d8;
	dim_print("%s:set_cfg:0x%x;afbc[0x%x]\n", __func__,
		ndvfm->c.set_cfg.d32,
		ndvfm->c.afbc_sgn_cfg.d8);
	memcpy(&ndvfm->c.out_dvfm,
	       &ds->out_dvfm_demo, sizeof(ndvfm->c.out_dvfm));
	#ifdef DBG_FLOW_SETTING
	print_dvfm(&ndvfm->c.out_dvfm, "parser_nr:out_dvfm");
	#endif

	/* cvs set */
	memcpy(&ndvfm->c.in_dvfm_crop, in_dvfm, sizeof(ndvfm->c.in_dvfm_crop));
	in_dvfm = &ndvfm->c.in_dvfm_crop;
	out_dvfm = &ndvfm->c.out_dvfm;
	in_dvfm->src_h = ndvfm->c.src_h;
	in_dvfm->src_w = ndvfm->c.src_w;
	out_dvfm->src_h = ndvfm->c.src_h;
	out_dvfm->src_w = ndvfm->c.src_w;
	out_dvfm->vf_ext = ndvfm->c.ori_vf;
	out_dvfm->sum_reg_cnt = itf->sum_reg_cnt;
	if (ndvfm->c.set_cfg.b.en_in_cvs) {
		/* config cvs for input */
		cvsp = &ndvfm->c.cvspara_in;
		cvsp->plane_nub	= in_dvfm->vfs.plane_num;
		cvsp->cvs_cfg	= &in_dvfm->vfs.canvas0_config[0];
		//cvs id need cfg when set
		//cvs_link(&cvspara, "in_cvs");
	}
	if (ndvfm->c.set_cfg.b.en_wr_cvs) {
		/* config cvs for input */
		cvsp = &ndvfm->c.cvspara_wr;
		cvsp->plane_nub	= out_dvfm->vfs.plane_num;
		cvsp->cvs_cfg	= &out_dvfm->vfs.canvas0_config[0];
		//cvs id need cfg when set
		//cvs_link(&cvspara, "in_cvs");
	}

	if (ndvfm->c.set_cfg.b.en_mem_cvs) {
		/* config cvs for input */
		cvsp = &ndvfm->c.cvspara_mem;
		cvsp->plane_nub	= out_dvfm->vfs.plane_num;
		cvsp->cvs_cfg	= &out_dvfm->vfs.canvas0_config[0];
		//cvs id need cfg when set
		//cvs_link(&cvspara, "in_cvs");
	}
	di_load_pq_table();//tmp
	ndvfm->c.sum_reg_cnt = itf->sum_reg_cnt;
	ndvfm->c.cnt_parser = ds->cnt_parser;
	ds->cnt_parser++;
	//ds->en_in_mif = true;

	/* get */
	vfm = (struct vframe_s *)qin->ops.get(qin);

	if (!hw->en_pst_wr_test) {
		vtype_fill_d(itf, vfm, &ndvfm->c.vf_in_cp, &ndvfm->c.out_dvfm);
		dim_print("%s:link\n", __func__);
		didbg_vframe_out_save(itf->bind_ch, vfm, 6);
		dpvpp_put_ready_vf(itf, ds, vfm);

		return true;
	}
	if (hw->en_pst_wr_test) {
		dim_print("%s:pst write\n", __func__);
		init_completion(&hw->pw_done);
		dpvpp_display(vfm, &ds->dis_para_demo, NULL);
		//wait:
		timeout = wait_for_completion_timeout(&hw->pw_done,
			msecs_to_jiffies(60));
		if (!timeout) {
			PR_WARN("%s:timeout\n", "dpvpp pw test");
			dpvpph_gl_sw(false, false, ds->op);
		}

		/**************************/
		vtype_fill_d(itf, vfm, &ndvfm->c.vf_in_cp, &ndvfm->c.out_dvfm);
		dbg_check_vf(ds, vfm, 2);

		dpvpp_put_ready_vf(itf, ds, vfm);

		return true;
	}
	return true;
}

static void dpvpph_prelink_sw(const struct reg_acc *op, bool p_link)
{
	unsigned int val;

	if (DIM_IS_IC_BF(TM2B)) {
		dim_print("%s:check return;\n", __func__);
		return;
	}

	if (p_link) {
		/* set on*/
		ext_vpp_prelink_real_sw(true, false);
		val = op->rd(VD1_AFBCD0_MISC_CTRL);
		if (DIM_IS_IC_EF(SC2)) {
			/* ? */
			op->bwr(DI_TOP_CTRL, 1, 0, 1);// 1:pre link vpp  0:post link vpp
			op->bwr(VD1_AFBCD0_MISC_CTRL, 1, 8, 6);            // data path
			op->bwr(VD1_AFBCD0_MISC_CTRL, 0, 16, 1);    // line buffer
		} else {
			op->bwr(DI_AFBCE_CTRL, 1, 3, 1);
			//op->wr(VD1_AFBCD0_MISC_CTRL, 0x100100);
			op->bwr(VD1_AFBCD0_MISC_CTRL, 1, 8, 1);
			op->bwr(VD1_AFBCD0_MISC_CTRL, 1, 20, 1);
		}
		dbg_plink1("c_sw:ak:from[0x%x]\n",
			val);
	} else {
		/* set off */
		dpvpph_gl_disable(op);	//test-05
		if (DIM_IS_IC_EF(SC2)) {
			dbg_plink1("c_sw:sc2\n");
			op->bwr(VD1_AFBCD0_MISC_CTRL, 0, 8, 6);
		} else {
			op->bwr(DI_AFBCE_CTRL, 0, 3, 1);
			op->bwr(VD1_AFBCD0_MISC_CTRL, 0, 8, 1);
			op->bwr(VD1_AFBCD0_MISC_CTRL, 0, 20, 1);
		}
		//prelink_status = false;
		dbg_plink1("c_sw:bk\n");
	}
}

static void dpvpph_prelink_polling_check(const struct reg_acc *op, bool p_link)
{
	unsigned int val;
	bool flg_set = false;

	if (DIM_IS_IC_BF(TM2B)) {
		dim_print("%s:check return;\n", __func__);
		return;
	}

	if (!p_link)
		return;

	val = op->rd(VD1_AFBCD0_MISC_CTRL);
	if (DIM_IS_IC_EF(SC2)) {
		/* check bit 8 / 16*/
		if ((val & 0x10100) != 0x00100)
			flg_set = true;

	} else {
		/* check bit 8 / 20 */
		if ((val & 0x100100) != 0x100100)
			flg_set = true;
	}
	if (flg_set) {
		PR_ERR("%s:check:ccccck 0x%x\n", __func__, val);
		dpvpph_prelink_sw(op, p_link);
		dim_print("check:ccccck\n");
	}
}

static void dpvpph_display_update_all(struct dim_prevpp_ds_s *ds,
			       struct dimn_dvfm_s *ndvfm,
			       const struct reg_acc *op_in)
{
	struct dim_cvspara_s *cvsp;
	struct dvfm_s *in_dvfm, *out_dvfm, *nr_dvfm;
	unsigned char *cvs, pos;
	struct di_win_s *winc;
	bool ref_en = true;
	struct dim_mifpara_s *mifp;
	struct dimn_dvfm_s *ndvfm_last;
	int i;
	unsigned int bypass_mem = 0;
	struct dim_pvpp_hw_s *hw;
	struct pvpp_buf_cfg_s *buf_cfg;
	unsigned short t5d_cnt;

	hw = &get_datal()->dvs_prevpp.hw;
	/* ndvfm_last */
	ndvfm_last = hw->dis_last_dvf;
	buf_cfg = get_buf_cfg(ndvfm->c.out_fmt, 2);
	if (!buf_cfg) {
		PR_WARN("buf_cfg\n");
		return;
	}
	dim_print("out_fmt:%d:\n", ndvfm->c.out_fmt);
	if (/*hw->en_pst_wr_test*/0) {
		pos = 0;
		hw->cvs_last = 0;
	} else {
		/* pos: 0 ~ 1 */
		pos = (1 - hw->cvs_last) & 1;
		hw->cvs_last = pos;
	}
	cvs = &hw->cvs[pos][0];
	in_dvfm = &ndvfm->c.in_dvfm_crop;
	winc	= &hw->dis_c_para.win;
	if (winc->x_size != in_dvfm->vfs.width ||
	    winc->y_size != in_dvfm->vfs.height) {
		in_dvfm->vfs.width = winc->x_size;
		in_dvfm->vfs.height = winc->y_size;
	}

	ds->mif_wr.tst_not_setcontr = 0;
	if (dim_is_pre_link_l() || !hw->en_pst_wr_test) { //tmp
		if (ds->cnt_display > 1)
			ds->mif_wr.tst_not_setcontr = 1;
		else
			ds->mif_wr.tst_not_setcontr = 0;
	}

	dim_print("%s:set_cfg:0x%x:tst_not_setcontr[%d]\n",
		__func__, ndvfm->c.set_cfg.d32,
		ds->mif_wr.tst_not_setcontr);
	/* cfg cvs */
	if (ndvfm->c.set_cfg.b.en_in_cvs) {
		/* config cvs for input */
		if (in_dvfm->vfs.canvas0Addr == (u32)-1) {
			cvsp = &ndvfm->c.cvspara_in;
			cvsp->plane_nub	= in_dvfm->vfs.plane_num;
			cvsp->cvs_cfg	= &in_dvfm->vfs.canvas0_config[0];
			cvsp->cvs_id	= cvs + 0;
			//cvs issue if (hw->en_pst_wr_test)
				cvs_link(cvsp, "in_cvs");
			mifp = &ds->mifpara_in;
			mifp->cvs_id[0] = (unsigned int)(*cvsp->cvs_id);
			mifp->cvs_id[1] = (unsigned int)(*(cvsp->cvs_id + 1));
		} else {
			cvsp = &ndvfm->c.cvspara_in;
			if (IS_NV21_12(in_dvfm->vfs.type))
				cvsp->plane_nub = 2;
			else if (in_dvfm->vfs.type & VIDTYPE_VIU_422)
				cvsp->plane_nub = 1;
			mifp = &ds->mifpara_in;
			mifp->cvs_id[0] = in_dvfm->vfs.canvas0Addr & 0xff;
			if (cvsp->plane_nub == 2)
				mifp->cvs_id[1] =
				(in_dvfm->vfs.canvas0Addr >> 16) & 0xff;
		}
	}

	if (ndvfm->c.set_cfg.b.en_wr_cvs || ndvfm->c.set_cfg.b.en_wr_mif) {
		//write address:
		for (i = 0; i < ndvfm->c.out_dvfm.vfs.plane_num; i++)
			ndvfm->c.out_dvfm.vfs.canvas0_config[0].phy_addr =
				buf_cfg->dbuf_wr_cfg[pos][i].phy_addr;
	} else if (ndvfm->c.set_cfg.b.en_wr_afbce) {
		ndvfm->c.out_dvfm.vfs.canvas0_config[0].phy_addr =
				buf_cfg->dbuf_wr_cfg[pos][0].phy_addr;
		ndvfm->c.out_dvfm.afbct_adr = buf_cfg->addr_e[pos];
		ndvfm->c.out_dvfm.vfs.compHeadAddr = buf_cfg->addr_i[pos];
	}

	if (ndvfm->c.set_cfg.b.en_wr_cvs) {
		/* cvs for out */
		cvsp = &ndvfm->c.cvspara_wr;
		out_dvfm = &ndvfm->c.out_dvfm;
		cvsp->plane_nub = out_dvfm->vfs.plane_num;
		cvsp->cvs_cfg		= &out_dvfm->vfs.canvas0_config[0];
		cvsp->cvs_id		= cvs + 2;
		//cvs issue if (hw->en_pst_wr_test)
			cvs_link(cvsp, "wr");

		mifp = &ds->mifpara_out;
		mifp->cvs_id[0] = (unsigned int)(*cvsp->cvs_id);
		mifp->cvs_id[1] = (unsigned int)(*(cvsp->cvs_id + 1));
	}

	/* copy out_dvfm to nr_wr_dvfm, then adjust size by crop */
	memcpy(&ndvfm->c.nr_wr_dvfm,
		&ndvfm->c.out_dvfm,
		sizeof(ndvfm->c.out_dvfm));
	nr_dvfm = &ndvfm->c.nr_wr_dvfm;
	if (winc->x_size != nr_dvfm->vfs.width ||
	    winc->y_size != nr_dvfm->vfs.height) {
		nr_dvfm->vfs.width = winc->x_size;
		nr_dvfm->vfs.height = winc->y_size;
		nr_dvfm->src_w = nr_dvfm->vfs.width;
		nr_dvfm->src_h = nr_dvfm->vfs.height;
	}

	/* check mem */
	if (!ndvfm->c.cnt_display || !ndvfm_last) {//tmp
		ref_en = false;
		//ndvfm->c.set_cfg.b.en_mem_cvs = false;
		ndvfm->c.set_cfg.b.en_mem_mif = false;
		ndvfm->c.set_cfg.b.en_mem_afbcd = false;
		dbg_plink1("display:change mem:%d\n", ndvfm->c.cnt_in);
	} else {
		/* have mem */
		memcpy(&ndvfm->c.mem_dvfm,
			&ndvfm_last->c.nr_wr_dvfm,
			sizeof(ndvfm->c.mem_dvfm));
	}
	dim_print("display:set_cfg:0x%x\n", ndvfm->c.set_cfg.d32);
	if (ndvfm->c.set_cfg.b.en_mem_cvs) {
		/* cvs for mem */
		cvsp = &ndvfm->c.cvspara_mem;
		if (ndvfm_last) {
			memcpy(&ndvfm->c.cvspara_mem,
			       &ndvfm_last->c.cvspara_wr,
			       sizeof(ndvfm->c.cvspara_mem)); //???
		} else {
			memcpy(&ndvfm->c.cvspara_mem,
				       &ndvfm->c.cvspara_wr,
				       sizeof(ndvfm->c.cvspara_mem)); //???
		}
		cvsp->cvs_id		= cvs + 4;
		//cvs issue if (hw->en_pst_wr_test)
			cvs_link(cvsp, "mem");

		mifp = &ds->mifpara_mem;
		mifp->cvs_id[0] = (unsigned int)(*cvsp->cvs_id);
		mifp->cvs_id[1] = (unsigned int)(*(cvsp->cvs_id + 1));
	}
	if (hw->en_pst_wr_test) {
		dpvpph_prelink_sw(op_in, false);
	} else {
		if (hw->dis_last_para.dmode != EPVPP_DISPLAY_MODE_NR) {
			dpvpph_pre_to_link();
			afbcd_enable_only_t5dvb(op_in, true);
			dpvpph_prelink_sw(op_in, true);
		}
	}
	/* mif in*/
	if (ndvfm->c.set_cfg.b.en_in_mif) {
		mif_cfg_v2(&ds->mif_in, &ndvfm->c.in_dvfm_crop, &ds->mifpara_in);
		if (!opl1()) {
			PR_ERR("not opl1?0x%px:\n", opl1());
			return;
		}
		if (!opl1()->pre_mif_set) {
			PR_ERR("not opl1z_pre_mif\n");
			return;
		}
		if (!op_in) {
			PR_ERR("no op_in?0x%px\n", op_in);
			return;
		}
		opl1()->pre_mif_set(&ds->mif_in, DI_MIF0_ID_INP, op_in);
	}
	/* mif wr */
	if (ndvfm->c.set_cfg.b.en_wr_mif) {
		mif_cfg_v2(&ds->mif_wr, &ndvfm->c.nr_wr_dvfm, &ds->mifpara_out);
#ifdef DBG_FLOW_SETTING
		print_dvfm(&ndvfm->c.nr_wr_dvfm, "set:nr_wr_dvfm");
		dim_dump_mif_state(&ds->mif_wr, "set:mif_wr");
		print_dim_mifpara(&ds->mifpara_out, "display:all:out_use");
#endif

		opl1()->set_wrmif_pp(&ds->mif_wr, op_in, EDI_MIFSM_NR);
	}
	if (ndvfm->c.set_cfg.b.en_mem_mif) {
		mif_cfg_v2(&ds->mif_mem,
			   &ndvfm_last->c.nr_wr_dvfm, &ds->mifpara_mem);
#ifdef DBG_FLOW_SETTING
		print_dvfm(&ndvfm_last->c.nr_wr_dvfm, "set:mem_dvfm");
		dim_dump_mif_state(&ds->mif_mem, "set:mif_mem");
		print_dim_mifpara(&ds->mifpara_mem, "display:all:mem_use");
#endif
		opl1()->pre_mif_set(&ds->mif_mem, DI_MIF0_ID_MEM, op_in);
	} else {
		//use current output
		mif_cfg_v2(&ds->mif_mem, &ndvfm->c.nr_wr_dvfm, &ds->mifpara_mem);
		opl1()->pre_mif_set(&ds->mif_mem, DI_MIF0_ID_MEM, op_in);
	}
	/* afbc */
	if (dim_afds() && dim_afds()->rest_val)
		dim_afds()->rest_val();
	if (dim_afds() && dim_afds()->pvpp_pre_check_dvfm) /* same as pre_check */
		dim_afds()->pvpp_pre_check_dvfm(ds, ndvfm);
	/* use ndvfm's sgn_set??*/

	if (ndvfm_last &&
	    ndvfm_last->c.set_cfg.b.en_in_afbcd &&
	    !ndvfm->c.set_cfg.b.en_in_afbcd) {
		dbg_mem("close afbcd:%d\n", ndvfm->c.cnt_in);
		dim_afds()->pvpp_sw_setting_op(false, op_in);
	}

	if (ndvfm->c.set_cfg.b.en_mem_afbcd ||
	    ndvfm->c.set_cfg.b.en_in_afbcd ||
	    ndvfm->c.set_cfg.b.en_wr_afbce) {
		dim_print("%s:afbc set:0x%x:0x%x\n", __func__,
			ndvfm->c.set_cfg.d32,
			ndvfm->c.afbc_sgn_cfg.b.inp);
		dim_afds()->pvpp_en_pre_set(ds, ndvfm, op_in);
	}

	/* nr write */
	if (!dpvpp_dbg_force_disable_ddr_wr())
		ds->pre_top_cfg.b.nr_ch0_en = 1;

	if (DIM_IS_IC_EF(SC2))
		dim_sc2_contr_pre(&ds->pre_top_cfg, op_in);
	else
		dpvpph_nr_ddr_switch(ds->pre_top_cfg.b.nr_ch0_en, op_in);

	atomic_set(&ndvfm->c.wr_set, 1);
	/*********************************/
	if (DIM_IS_IC(T5DB)) {
		t5d_cnt = cfgg(T5DB_P_NOTNR_THD);
		if (t5d_cnt < 5)
			t5d_cnt = 5;
		if (ds->field_count_for_cont < t5d_cnt)
			bypass_mem = 1;
		else
			bypass_mem = 0;
	}
	if (!ndvfm->c.set_cfg.b.en_mem_mif && !ndvfm->c.set_cfg.b.en_mem_afbcd)
		bypass_mem = 1;
	if (dpvpp_dbg_force_mem_bypass())
		bypass_mem = 1;
	dpvpph_enable_di_pre_aml(hw->op,
				 hw->en_pst_wr_test, bypass_mem << 4, NULL);
	dct_pst(hw->op, ndvfm);
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A)) {
		/* enable mc pre mif*/
		//dimh_enable_di_pre_mif(true, dimp_get(edi_mp_mcpre_en));
		//is dimh_enable_di_pre_mif
#ifdef HIS_CODE
		if (DIM_IS_IC_EF(SC2))
			opl1()->pre_mif_sw(true, op_in);
		else
			dpvpph_pre_data_mif_ctrl(true, op_in);
#else
		opl1()->pre_mif_sw(true, op_in, true);
#endif
		dpvpph_reverse_mif_ctrl(hw->dis_c_para.plink_reverse,
							hw->dis_c_para.plink_hv_mirror, op_in);
		dpvpph_pre_frame_reset_g12(!hw->en_pst_wr_test, op_in);
	} else { /* not test */
		dpvpph_pre_frame_reset(op_in);
		/* enable mc pre mif*/
		//dimh_enable_di_pre_mif(true, dimp_get(edi_mp_mcpre_en));
		dpvpph_pre_data_mif_ctrl(true, op_in);
	}
	dbg_mem("update all\n");
}

/* @ary 11-08 not used */
//static
void dpvpph_display_update_sub_last_cvs(struct dimn_itf_s *itf,
				struct dim_prevpp_ds_s *ds,
			       struct dimn_dvfm_s *ndvfm,
			       const struct reg_acc *op_in)
{
	struct dim_cvspara_s *cvsp;
	struct dimn_dvfm_s *ndvfm_last;
	static unsigned int last_nub;
	struct dim_pvpp_hw_s *hw;

	hw = &get_datal()->dvs_prevpp.hw;

	/* ndvfm_last */ /* same*/
	ndvfm_last = hw->dis_last_dvf;
	//dbg_di_prelink_reg_check_v3_op(op_in, !hw->en_pst_wr_test);

	if (!hw->en_pst_wr_test	&&
	    ndvfm_last &&
	    !ndvfm_last->c.set_cfg.b.en_linear_cp &&
	    atomic_read(&ndvfm_last->c.wr_set)	&&
	    !atomic_read(&ndvfm_last->c.wr_cvs_set)) {
		dim_print("%s:%d\n", __func__, ndvfm->c.cnt_display);
		cvsp = &ndvfm_last->c.cvspara_in;
		cvs_link(cvsp, "in_cvs");
		atomic_add(0x100, &itf->c.dbg_display_sts);	/* dbg only */
		if (ndvfm_last->c.set_cfg.b.en_wr_cvs) {
			cvsp = &ndvfm_last->c.cvspara_wr;
			cvs_link(cvsp, "cvs_wr");
			atomic_add(0x200, &itf->c.dbg_display_sts);/* dbg only */
		}
		if (ndvfm_last->c.set_cfg.b.en_mem_cvs) {
			cvsp = &ndvfm_last->c.cvspara_mem;
			cvs_link(cvsp, "cvs_mem");
			atomic_add(0x400, &itf->c.dbg_display_sts);/* dbg only */
		}
		/* check done, note, this is last two */
		last_nub = ds->pre_done_nub;
		/* cvs flg */
		atomic_inc(&ndvfm_last->c.wr_cvs_set);
	} else {
		if (last_nub == ds->pre_done_nub)
			return;
		last_nub = ds->pre_done_nub;
		if (!ndvfm_last) {
			dim_print("cvs_update: no last\n");
		} else {
			dim_print("linear:%d;wr_set[%d],cvs_set[%d]\n",
				ndvfm_last->c.set_cfg.b.en_linear_cp,
				atomic_read(&ndvfm_last->c.wr_set),
				atomic_read(&ndvfm_last->c.wr_cvs_set));
		}
	}
}

//static
void dpvpph_display_update_part(struct dim_prevpp_ds_s *ds,
			       struct dimn_dvfm_s *ndvfm,
			       const struct reg_acc *op_in,
			       unsigned int diff)
{
	struct dim_cvspara_s *cvsp;
	struct dvfm_s *in_dvfm, *out_dvfm, *nr_dvfm;
	unsigned char *cvs, pos;
	struct di_win_s *winc;
	bool ref_en = true;
	struct dim_mifpara_s *mifp;
	struct pvpp_buf_cfg_s *buf_cfg;
	struct dimn_dvfm_s *ndvfm_last;
	int i;
	unsigned int bypass_mem = 0;
	struct dim_pvpp_hw_s *hw;
	unsigned short t5d_cnt;

	hw = &get_datal()->dvs_prevpp.hw;

	/* ndvfm_last */ /* same*/
	ndvfm_last = hw->dis_last_dvf;
	buf_cfg = get_buf_cfg(ndvfm->c.out_fmt, 3);
	if (!buf_cfg) {
		PR_WARN("buf_cfg\n");
		return;
	}

	/* pos: 0 ~ 1 */
	pos = (1 - hw->cvs_last) & 1;
	hw->cvs_last = pos;
	/* address update */

	cvs = &hw->cvs[pos][0];
	in_dvfm = &ndvfm->c.in_dvfm_crop;
	winc	= &hw->dis_c_para.win;
	if (winc->x_size != in_dvfm->vfs.width ||
	    winc->y_size != in_dvfm->vfs.height) {
		in_dvfm->vfs.width = winc->x_size;
		in_dvfm->vfs.height = winc->y_size;
	}

	ds->mif_wr.tst_not_setcontr = 0;
	if (dim_is_pre_link_l() || !hw->en_pst_wr_test) { //tmp
		if (ds->cnt_display > 1)
			ds->mif_wr.tst_not_setcontr = 1;
		else
			ds->mif_wr.tst_not_setcontr = 0;
	}

	dim_print("%s:set_cfg:0x%x:tst_not_setcontr[%d]\n",
		__func__, ndvfm->c.set_cfg.d32,
		ds->mif_wr.tst_not_setcontr);
	/* cfg cvs */
	if (ndvfm->c.set_cfg.b.en_in_cvs) {
		if (in_dvfm->vfs.canvas0Addr == (u32)-1) {
			/* config cvs for input */
			cvsp = &ndvfm->c.cvspara_in;
			cvsp->plane_nub	= in_dvfm->vfs.plane_num;
			cvsp->cvs_cfg	= &in_dvfm->vfs.canvas0_config[0];
			cvsp->cvs_id	= cvs + 0;
			//cvs issue if (hw->en_pst_wr_test)
				cvs_link(cvsp, "in_cvs");
			mifp = &ds->mifpara_in;
			mifp->cvs_id[0] = (unsigned int)(*cvsp->cvs_id);
			mifp->cvs_id[1] = (unsigned int)(*(cvsp->cvs_id + 1));
		} else {
			cvsp = &ndvfm->c.cvspara_in;
			if (IS_NV21_12(in_dvfm->vfs.type))
				cvsp->plane_nub = 2;
			else if (in_dvfm->vfs.type & VIDTYPE_VIU_422)
				cvsp->plane_nub = 1;
			mifp = &ds->mifpara_in;
			mifp->cvs_id[0] = in_dvfm->vfs.canvas0Addr & 0xff;
			if (cvsp->plane_nub == 2)
				mifp->cvs_id[1] =
				(in_dvfm->vfs.canvas0Addr >> 16) & 0xff;
		}
	}

	if (ndvfm->c.set_cfg.b.en_wr_cvs || ndvfm->c.set_cfg.b.en_wr_mif) {
		//write address:
		for (i = 0; i < ndvfm->c.out_dvfm.vfs.plane_num; i++)
			ndvfm->c.out_dvfm.vfs.canvas0_config[0].phy_addr =
			buf_cfg->dbuf_wr_cfg[pos][i].phy_addr;
	} else if (ndvfm->c.set_cfg.b.en_wr_afbce) {
		ndvfm->c.out_dvfm.vfs.canvas0_config[0].phy_addr =
				buf_cfg->dbuf_wr_cfg[pos][0].phy_addr;
		ndvfm->c.out_dvfm.afbct_adr = buf_cfg->addr_e[pos];
		ndvfm->c.out_dvfm.vfs.compHeadAddr = buf_cfg->addr_i[pos];
	}

	if (ndvfm->c.set_cfg.b.en_wr_cvs) {
		/* cvs for out */
		cvsp = &ndvfm->c.cvspara_wr;
		out_dvfm = &ndvfm->c.out_dvfm;
		cvsp->plane_nub = out_dvfm->vfs.plane_num;
		cvsp->cvs_cfg		= &out_dvfm->vfs.canvas0_config[0];
		cvsp->cvs_id		= cvs + 2;
		//cvs issue if (hw->en_pst_wr_test)
			cvs_link(cvsp, "wr");
		mifp = &ds->mifpara_out;
		mifp->cvs_id[0] = (unsigned int)(*cvsp->cvs_id);
		mifp->cvs_id[1] = (unsigned int)(*(cvsp->cvs_id + 1));
	}

	/* copy out_dvfm to nr_wr_dvfm, then adjust size by crop */
	memcpy(&ndvfm->c.nr_wr_dvfm,
		&ndvfm->c.out_dvfm,
		sizeof(ndvfm->c.out_dvfm));
	nr_dvfm = &ndvfm->c.nr_wr_dvfm;
	if (winc->x_size != nr_dvfm->vfs.width ||
	    winc->y_size != nr_dvfm->vfs.height) {
		nr_dvfm->vfs.width = winc->x_size;
		nr_dvfm->vfs.height = winc->y_size;
		nr_dvfm->src_w = nr_dvfm->vfs.width;
		nr_dvfm->src_h = nr_dvfm->vfs.height;
	}

	/* check mem */
	if (!ndvfm->c.cnt_display || !ndvfm_last) {//tmp
		ref_en = false;
		ndvfm->c.set_cfg.b.en_mem_cvs = false;
		ndvfm->c.set_cfg.b.en_mem_mif = false;
		ndvfm->c.set_cfg.b.en_mem_afbcd = false;
		dbg_plink1("display:%d:up mem:0\n", ndvfm->c.cnt_in);
	} else {
#ifdef HIS_CODE
		if (ndvfm->c.mem_dvfm.vfs.type != ndvfm_last->c.nr_wr_dvfm.vfs.type) {
			dbg_plink1("mem:%d:type:0x%x->0x%x\n",
				ndvfm->c.cnt_in,
				ndvfm->c.mem_dvfm.vfs.type,
				ndvfm_last->c.nr_wr_dvfm.vfs.type);
		}
#endif
		/* have mem */
		memcpy(&ndvfm->c.mem_dvfm,
			&ndvfm_last->c.nr_wr_dvfm,
			sizeof(ndvfm->c.mem_dvfm));
	}

	dim_print("display:set_cfg:0x%x", ndvfm->c.set_cfg.d32);
	if (ndvfm->c.set_cfg.b.en_mem_cvs) {
		/* cvs for out */
		cvsp = &ndvfm->c.cvspara_mem;
		//update no need
		if (ndvfm_last)
			memcpy(&ndvfm->c.cvspara_mem,
			       &ndvfm_last->c.cvspara_wr,
			       sizeof(ndvfm->c.cvspara_mem));

		cvsp->cvs_id		= cvs + 4;
		//cvs issue if (hw->en_pst_wr_test)
			cvs_link(cvsp, "mem");

		mifp = &ds->mifpara_mem;
		mifp->cvs_id[0] = (unsigned int)(*cvsp->cvs_id);
		mifp->cvs_id[1] = (unsigned int)(*(cvsp->cvs_id + 1));
	}
	if (!hw->en_pst_wr_test)
		dpvpph_prelink_polling_check(op_in, true);
	/* mif in*/
	if (ndvfm->c.set_cfg.b.en_in_mif) {
		//mif_cfg_v2(&ds->mif_in,
		//	     &ndvfm->c.in_dvfm_crop,
		//	     &ds->mifpara_in);
		mif_cfg_v2_update_addr(&ds->mif_in,
				       &ndvfm->c.in_dvfm_crop, &ds->mifpara_in);
		opl1()->mif_rd_update_addr(&ds->mif_in, DI_MIF0_ID_INP, op_in);
	}
	/* mif wr */
	if (ndvfm->c.set_cfg.b.en_wr_mif) {
		//mif_cfg_v2(&ds->mif_wr, &ndvfm->c.nr_wr_dvfm, &ds->mifpara_out);
		mif_cfg_v2_update_addr(&ds->mif_wr,
				       &ndvfm->c.nr_wr_dvfm, &ds->mifpara_out);

		opl1()->wrmif_update_addr(&ds->mif_wr, op_in, EDI_MIFSM_NR);
#ifdef DBG_FLOW_SETTING
		print_dvfm(&ndvfm->c.nr_wr_dvfm, "set:nr_wr_dvfm");
		dim_dump_mif_state(&ds->mif_wr, "set:mif_wr");
		print_dim_mifpara(&ds->mifpara_out, "display:all:out_use");
#endif
	}
	if (ndvfm->c.set_cfg.b.en_mem_mif) {
		if (diff & EDIM_DVPP_DIFF_MEM) {
			mif_cfg_v2(&ds->mif_mem,
				   &ndvfm_last->c.nr_wr_dvfm, &ds->mifpara_mem);
			opl1()->pre_mif_set(&ds->mif_mem, DI_MIF0_ID_MEM, op_in);
		} else {
			mif_cfg_v2_update_addr(&ds->mif_mem,
					       &ndvfm_last->c.nr_wr_dvfm,
					       &ds->mifpara_mem);
			opl1()->mif_rd_update_addr(&ds->mif_mem,
						   DI_MIF0_ID_MEM, op_in);

#ifdef DBG_FLOW_SETTING
			print_dvfm(&ndvfm_last->c.nr_wr_dvfm, "set:mem_dvfm");
			dim_dump_mif_state(&ds->mif_mem, "set:mif_mem");
			print_dim_mifpara(&ds->mifpara_mem, "display:all:mem_use");
#endif
		}
	}
	if (dim_afds() && dim_afds()->pvpp_pre_check_dvfm) /* same as pre_check */
		dim_afds()->pvpp_pre_check_dvfm(ds, ndvfm);

	if (ndvfm->c.set_cfg.b.en_mem_afbcd ||
	    ndvfm->c.set_cfg.b.en_in_afbcd ||
	    ndvfm->c.set_cfg.b.en_wr_afbce) {
		dim_print("%s:afbc set:0x%x:0x%x\n", __func__,
			ndvfm->c.set_cfg.d32,
			ndvfm->c.afbc_sgn_cfg.b.inp);
		dim_afds()->pvpp_en_pre_set(ds, ndvfm, op_in);
	}
	/* nr write switch */
	if (atomic_read(&ndvfm->c.wr_set)) {
		ds->pre_top_cfg.b.nr_ch0_en = 0;
	} else {
		ds->pre_top_cfg.b.nr_ch0_en = 1;
		atomic_set(&ndvfm->c.wr_set, 1);
	}
	if (dpvpp_dbg_force_disable_ddr_wr())
		ds->pre_top_cfg.b.nr_ch0_en = 0;

	if (DIM_IS_IC_EF(SC2)) {
		dim_sc2_contr_pre(&ds->pre_top_cfg, op_in);
	} else { //?
		dpvpph_nr_ddr_switch(ds->pre_top_cfg.b.nr_ch0_en, op_in);
	}
	/************************************/
	if (DIM_IS_IC(T5DB)) {
		t5d_cnt = cfgg(T5DB_P_NOTNR_THD);
		if (t5d_cnt < 5)
			t5d_cnt = 5;
		if (ds->field_count_for_cont < t5d_cnt)
			bypass_mem = 1;
		else
			bypass_mem = 0;
	}
	if (!ndvfm->c.set_cfg.b.en_mem_mif && !ndvfm->c.set_cfg.b.en_mem_afbcd)
		bypass_mem = 1;
	if (dpvpp_dbg_force_mem_bypass())
		bypass_mem = 1;
	if (dpvpp_dbg_force_mem_en())
		bypass_mem = 0;
	if (bypass_mem)
		dbg_plink1("mem bypass:%d\n", ndvfm->c.cnt_in);

	dpvpph_enable_di_pre_aml(hw->op,
				 hw->en_pst_wr_test, bypass_mem << 4, NULL);
	dct_pst(hw->op, ndvfm);
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A)) {
		/* enable mc pre mif*/
		//dimh_enable_di_pre_mif(true, dimp_get(edi_mp_mcpre_en));
		//is dimh_enable_di_pre_mif
#ifdef HIS_CODE
		if (DIM_IS_IC_EF(SC2))
			opl1()->pre_mif_sw(true, op_in);
		else
			dpvpph_pre_data_mif_ctrl(true, op_in);
#else
		opl1()->pre_mif_sw(true, op_in, true);
#endif
		dpvpph_reverse_mif_ctrl(hw->dis_c_para.plink_reverse,
							hw->dis_c_para.plink_hv_mirror, op_in);
		dpvpph_pre_frame_reset_g12(!hw->en_pst_wr_test, op_in);
	} else {
		dpvpph_pre_frame_reset(op_in);
		/* enable mc pre mif*/
		//dimh_enable_di_pre_mif(true, dimp_get(edi_mp_mcpre_en));
		dpvpph_pre_data_mif_ctrl(true, op_in);
	}
}

static bool vtype_fill_d(struct dimn_itf_s *itf,
		struct vframe_s *vfmt,
		struct vframe_s *vfmf,
		struct dvfm_s *by_dvfm)
{
	int i;
	void *priv;
	unsigned int index;
	struct dcntr_mem_s *dcntr_mem = NULL;

	if (!vfmt || !vfmf || !by_dvfm || !itf)
		return false;
	/* keep private and index */
	priv = vfmt->private_data;
	index = vfmt->index;
	memcpy(vfmt, vfmf, sizeof(*vfmt));
	vfmt->private_data = priv;
	vfmt->index	= index;
	//type clear
	vfmt->type &= (~VFMT_DIPVPP_CHG_MASK);
	vfmt->type |= (by_dvfm->vfs.type & VFMT_DIPVPP_CHG_MASK);
	vfmt->bitdepth = by_dvfm->vfs.bitdepth;
	//flg clear
	vfmt->flag &= (~VFMT_FLG_CHG_MASK);
	vfmt->flag |= (by_dvfm->vfs.flag & VFMT_FLG_CHG_MASK);
	vfmt->di_flag = by_dvfm->vfs.di_flag;

	vfmt->canvas0Addr = (u32)-1;
	vfmt->canvas1Addr = (u32)-1;
	vfmt->di_instance_id = by_dvfm->sum_reg_cnt;
	if (by_dvfm->is_prvpp_link || dim_is_pre_link_l()) {
		dim_print("%s:by type:0x%x\n", __func__, by_dvfm->vfs.type);
		if (IS_COMP_MODE(by_dvfm->vfs.type)) { //
			/* afbce */
			vfmt->compBodyAddr	= by_dvfm->vfs.compBodyAddr;
			vfmt->compHeadAddr	= by_dvfm->vfs.compHeadAddr;
			vfmt->compWidth		= by_dvfm->vfs.width;
			vfmt->compHeight	= by_dvfm->vfs.height;
		} else  {
			vfmt->compBodyAddr = 0;
			vfmt->compHeadAddr = 0;
			vfmt->plane_num	= by_dvfm->vfs.plane_num;
			memset(&vfmt->canvas0_config[0],
			       0,
			       sizeof(vfmt->canvas0_config));
		}
		vfmt->vf_ext = by_dvfm->vf_ext;
		if (vfmt->vf_ext)
			vfmt->flag |= VFRAME_FLAG_DOUBLE_FRAM; /* for vf_ext */

		vfmt->early_process_fun = NULL;
		vfmt->type |= VIDTYPE_PRE_INTERLACE;
		dim_print("%s:pre-vpp link:0x%x:0x%px:%d\n", __func__,
			vfmt->type,
			vfmt,
			vfmt->index);

		vfmt->process_fun = NULL;

		if (IS_COMP_MODE(vfmf->type)) { //??
			vfmt->width = vfmt->compWidth;
			vfmt->height = vfmt->compHeight;
		}
		if (dpvpp_dbg_force_dech ||
		    dpvpp_dbg_force_decv) {
			vfmt->width		= by_dvfm->vfs.width; //tmp
			vfmt->height		= by_dvfm->vfs.height;
			dbg_plink1("dbg:force deh\n");
		}
		if (vfmt->di_flag & DI_FLAG_DI_PVPPLINK) {
			struct dimn_dvfm_s *dvfm;
			unsigned int ds_ratio = 0;

			dvfm = (struct dimn_dvfm_s *)vfmt->private_data;
			if (dvfm)
				dcntr_mem = (struct dcntr_mem_s *)dvfm->c.dct_pre;
			if (dcntr_mem) {
				ds_ratio = dcntr_mem->ds_ratio;
				if (itf->c.last_ds_ratio != ds_ratio)
					dbg_plink1("%s: ds_ratio:%d->%d\n",
						__func__, itf->c.last_ds_ratio, ds_ratio);
				itf->c.last_ds_ratio = ds_ratio;
			} else {
				/* keep the last_ds_ratio if meet dct pre timeout */
				ds_ratio = itf->c.last_ds_ratio;
			}
			ds_ratio = ds_ratio << DI_FLAG_DCT_DS_RATIO_BIT;
			ds_ratio &= DI_FLAG_DCT_DS_RATIO_MASK;
			vfmt->di_flag &= ~DI_FLAG_DCT_DS_RATIO_MASK;
			vfmt->di_flag |= ds_ratio;
		} else {
			vfmt->di_flag &= ~DI_FLAG_DCT_DS_RATIO_MASK;
		}
	} else {
		if (IS_COMP_MODE(by_dvfm->vfs.type)) {
			/* afbce */
			vfmt->compBodyAddr	= by_dvfm->vfs.compBodyAddr;
			vfmt->compHeadAddr	= by_dvfm->vfs.compHeadAddr;
		} else {
			vfmt->plane_num	= by_dvfm->vfs.plane_num;
			for (i = 0; i < vfmt->plane_num; i++)
				memcpy(&vfmt->canvas0_config[i],
				       &by_dvfm->vfs.canvas0_config[i],
				       sizeof(vfmt->canvas0_config[i]));
			vfmt->width		= by_dvfm->vfs.width; //tmp
			vfmt->height		= by_dvfm->vfs.height;
		}
		vfmt->type |= VIDTYPE_DI_PW;

		if (by_dvfm->is_itf_vfm)
			vfmt->flag |= VFRAME_FLAG_DI_PW_VFM;
		else if (by_dvfm->is_itf_ins_local)
			vfmt->flag |= VFRAME_FLAG_DI_PW_N_LOCAL;
		else
			vfmt->flag |= VFRAME_FLAG_DI_PW_N_EXT;
	}

	return true;
}

#ifdef DIM_PLINK_ENABLE_CREATE
/* @ary 11-08 not used */
//static
int dpvpp_create_instance(struct di_init_parm *parm)
{
	//struct dim_prevpp_ds_s *ds = NULL;
	struct dimn_itf_s *itf;
	int ret;

	/* try */
	ret = dpvpp_reg_idle_ch();
	if (ret < 0 || ret >= DI_PLINK_CN_NUB)
		return DI_ERR_REG_NO_IDLE_CH;
	itf = &get_datal()->dvs_prevpp.itf[ret];

	//ds	= get_datal()->dvs_prevpp.ds;
	mutex_lock(&itf->lock_reg);
	if (atomic_read(&itf->reg)) {
		PR_WARN("%s:duplicate reg\n", __func__);
		mutex_unlock(&itf->lock_reg);
		return DI_ERR_REG_NO_IDLE_CH;
	}
	if (itf->ops_vfm &&
	    itf->ops_vfm->reg_pre_check &&
	    !itf->ops_vfm->reg_pre_check(itf)) {
		mutex_unlock(&itf->lock_reg);
		return DI_ERR_REG_NO_IDLE_CH;
	}

	itf->sum_reg_cnt = dpvpp_create_instance_id(ret);

	dbg_ev("reg:[%d]\n", itf->sum_reg_cnt);
	itf->etype = EDIM_NIN_TYPE_INS;

	/*parm*/
	memcpy(&itf->nitf_parm, parm, sizeof(itf->nitf_parm));

	if (itf->ops_vfm && itf->ops_vfm->reg_api)
		itf->ops_vfm->reg_api(itf);

	atomic_set(&itf->reg, 1);/* */

	mutex_unlock(&itf->lock_reg);

	return DIM_PRE_VPP_NUB;
}

int dpvpp_destroy_instance(int index)
{
	struct dim_prevpp_ds_s *ds = NULL;
	struct dimn_itf_s *itf;
	//int ret;

	if (index >= DI_PLINK_CN_NUB) {
		PR_ERR("%s:index overflow\n", __func__);
		return DI_ERR_INDEX_OVERFLOW;
	}
	itf = &get_datal()->dvs_prevpp.itf[index];
	ds	= itf->ds;

	PR_INF("%s:\n", __func__);

	mutex_lock(&itf->lock_reg);
	if (!atomic_read(&itf->reg)) {
		mutex_unlock(&itf->lock_reg);
		PR_WARN("%s:duplicate ureg\n", __func__);
		return DI_ERR_INDEX_NOT_ACTIVE;
	}
	atomic_set(&itf->unregging_back, 1);
	if (itf->ops_vfm && itf->ops_vfm->unreg_trig)
		itf->ops_vfm->unreg_trig(itf);

	atomic_set(&itf->unregging_back, 0);
	atomic_set(&itf->reg, 0);
	if (itf->ops_vfm && itf->ops_vfm->unreg_api)
		itf->ops_vfm->unreg_api(itf);
	mutex_unlock(&itf->lock_reg);

	PR_INF("%s:end\n", __func__);
	return 0;
}
#endif /* DIM_PLINK_ENABLE_CREATE */

enum DI_ERRORTYPE dpvpp_empty_input_buffer(struct dimn_itf_s *itf,
					   struct di_buffer *buffer)
{
	struct dim_prevpp_ds_s *ds = NULL;
//	struct dimn_itf_s *itf;
	struct dimn_qs_cls_s *qidle, *qin;
	unsigned int free_nub;
	struct vframe_s *vf, *mvf;
	struct dimn_dvfm_s *ndvfm;
	struct di_buffer *buffer_l;
	unsigned int err_cnt = 0;
	bool flg_q;

	if (!itf || !itf->ds) {
		PR_ERR("%s:no itf:0x%px\n", __func__, itf);
		return DI_ERR_INDEX_NOT_ACTIVE;
	}

	ds	= itf->ds;

	if (!atomic_read(&itf->reg) || !ds) {
		PR_WARN("%s:not reg\n", __func__);
		return DI_ERR_INDEX_NOT_ACTIVE;
	}

	qidle	= &ds->lk_que_idle;
	qin	= &ds->lk_que_in;

	free_nub	= qidle->ops.count(qidle);

	if (!free_nub || qin->ops.is_full(qin)) {
		PR_ERR("%s:queue full. free_nub:%d\n", __func__, free_nub);
		return DI_ERR_IN_NO_SPACE;
	}

	mvf = (struct vframe_s *)qidle->ops.get(qidle);

	if (!mvf || !mvf->private_data) {
		PR_ERR("%s:qout:%d:0x%px\n",
			__func__,
			qidle->ops.count(qidle),
			mvf);
		err_cnt++;
		/* to do bypass ?*/
		return DI_ERR_IN_NO_SPACE; //?
	}

	dbg_check_vf(ds, mvf, 1);
	ndvfm = (struct dimn_dvfm_s *)mvf->private_data;
	dim_print("%s:nvfm:0x%px\n", __func__, ndvfm);
	memset(&ndvfm->c, 0, sizeof(ndvfm->c));
	ndvfm->c.ori_in = buffer;
	ndvfm->c.cnt_in = itf->c.sum_pre_get;
	if (buffer->vf && buffer->vf->decontour_pre) {
		ndvfm->c.dct_pre = buffer->vf->decontour_pre;
		buffer->vf->decontour_pre = NULL;
		ds->dct_sum_in++;
	}
	/* clear */
	buffer_l = &itf->buf_bf[mvf->index];
	buffer_l->flag		= 0;
	buffer_l->crcout	= 0;
	buffer_l->nrcrcout	= 0;
//#if defined(DBG_QUE_IN_OUT) || defined(DBG_QUE_INTERFACE)
	dbg_plink2("ins:in:0x%px:%d:0x%x\n",
		   buffer, ndvfm->c.cnt_in, buffer->flag);
	dbg_plink2("ins:buffer[0x%px:0x%x]:vf[0x%px:0x%x]\n",
				buffer, buffer->flag,
				buffer->vf, buffer->vf->type);

//#endif

	itf->c.sum_pre_get++;
	if (buffer->flag & DI_FLAG_EOS) {
		/* to do */
		buffer->flag |= DI_FLAG_BUF_BY_PASS;
		ndvfm->c.in_dvfm.vfs.type |= VIDTYPE_V4L_EOS;//23-02-15
	} else if (!buffer->vf) {
		PR_ERR("%s:no vf:0x%px\n", __func__, buffer);
		qidle->ops.put(qidle, mvf);
		itf->nitf_parm.ops.empty_input_done(buffer);
		return DI_ERR_UNSUPPORT;
	} else {
		vf = buffer->vf;

		ndvfm->c.ori_vf = vf;
		memcpy(&ndvfm->c.vf_in_cp, vf, sizeof(ndvfm->c.vf_in_cp));
		dim_dvf_cp(&ndvfm->c.in_dvfm, vf, 0);
		count_4k(itf, &ndvfm->c.in_dvfm);
		if (IS_COMP_MODE(ndvfm->c.in_dvfm.vfs.type)) {
			ndvfm->c.in_dvfm.vfs.width = vf->compWidth;
			ndvfm->c.in_dvfm.vfs.height = vf->compHeight;
		}
		ndvfm->c.src_w = ndvfm->c.in_dvfm.vfs.width;
		ndvfm->c.src_h = ndvfm->c.in_dvfm.vfs.height;
		//buffer_l->phy_addr	= vf->canvas0_config[0].phy_addr;//tmp
		dim_print("%s:0x%lx\n", __func__,
			  (unsigned long)vf->canvas0_config[0].phy_addr);
	}
	flg_q = qin->ops.put(qin, mvf);

	if (!flg_q) {
		PR_ERR("%s:qin\n", __func__);
		err_cnt++;
		//in_vf_put(itf, vf);
		/* how to return di_buffer ?*/
		qidle->ops.put(qidle, mvf);
		return DI_ERR_IN_NO_SPACE; //?
	}

	task_send_wk_timer(EDIM_WKUP_REASON_IN_HAVE);

	return DI_ERR_NONE;
}

static void dpvpp_patch_first_buffer(struct dimn_itf_s *itf)
{
	int i;
	unsigned int cnt1, cnt2;
	struct dim_prevpp_ds_s *ds = NULL;
	struct dimn_qs_cls_s *qidle;
	struct buf_que_s *pbufq;
	union q_buf_u q_buf;
	struct dim_nins_s *ins;
	bool ret;
	unsigned int qt_in;
	unsigned int bindex;
	struct di_buffer *buffer;
	struct di_ch_s *pch;
//	struct dcntr_mem_s	*pdcn; //tmp

	if (!itf || !itf->ds) {
		PR_ERR("%s:no itf:0x%px\n", __func__, itf);
		return;
	}

	ds	= itf->ds;
	if (!atomic_read(&itf->reg) || !ds) {
		PR_WARN("%s:not reg\n", __func__);
		return;
	}

	if (itf->bind_ch >= DI_CHANNEL_MAX) {
		PR_ERR("%s:ch[%d] overflow\n", __func__, itf->bind_ch);
		return;
	}

	pch = get_chdata(itf->bind_ch);
	if (!pch) {
		PR_ERR("%s:no pch:id[%d]\n", __func__, itf->bind_ch);
		return;
	}

	pbufq = &pch->nin_qb;
	qt_in = QBF_NINS_Q_CHECK;
	cnt1 = nins_cnt(pch, qt_in);
	if (!cnt1) {
		dbg_plink3("%s:no input buffer\n", __func__);
		return;
	}

	qidle = &ds->lk_que_idle;
	cnt2 = qidle->ops.count(qidle);
	if (!cnt2) {
		dbg_plink3("%s:no free nub. input buffer:%d\n", __func__, cnt1);
		return;
	}

	if (cnt1 > cnt2)
		cnt1 = cnt2;
	for (i = 0; i < cnt1; i++) {
		ret = qbuf_out(pbufq, qt_in, &bindex);
		if (!ret) {
			PR_ERR("%s:1:%d:can't get out\n", __func__, i);
			continue;
		}
		if (bindex >= DIM_NINS_NUB) {
			PR_ERR("%s:2:%d:%d\n", __func__, i, bindex);
			continue;
		}
		q_buf = pbufq->pbuf[bindex];
		ins = (struct dim_nins_s *)q_buf.qbc;

		buffer = (struct di_buffer *)ins->c.ori;
		if (!buffer) {
			PR_ERR("%s:3:%d,%d\n", __func__, i, bindex);
			continue;
		}
		if (buffer->vf)
			buffer->vf->decontour_pre = ins->c.vfm_cp.decontour_pre;
		else
			PR_WARN("%s:no vf:0x%px,0x%x\n",
				__func__, buffer,
				buffer->flag);
		ins->c.vfm_cp.decontour_pre = NULL;
		memset(&ins->c, 0, sizeof(ins->c));
		qbuf_in(pbufq, QBF_NINS_Q_IDLE, bindex);

		dpvpp_empty_input_buffer(itf, buffer);
	}
	//PR_INF("%s:%d\n", __func__, cnt);
}

/* @ary_note: buffer alloc by di			*/
/* @ary_note: use this api to put back display buffer	*/
/* @ary_note: same as vfm put */
//static
enum DI_ERRORTYPE dpvpp_fill_output_buffer(struct dimn_itf_s *itf,
					   struct di_buffer *buffer)
{
	struct dimn_qs_cls_s *qback;
	struct dimn_dvfm_s *dvfm = NULL;
	struct dim_prevpp_ds_s *ds = NULL;
	struct vframe_s *vf;

	if (!itf || !itf->ds) {
		PR_ERR("%s:index overflow\n", __func__);
		return DI_ERR_INDEX_OVERFLOW;
	}

	if (!buffer || !buffer->vf) {
		PR_ERR("%s:\n", __func__);
		return DI_ERR_INDEX_OVERFLOW;
	}

	ds = itf->ds;

	itf->c.sum_pst_put++;
	vf = buffer->vf;
	dvfm = (struct dimn_dvfm_s *)vf->private_data;
	if (dvfm->header.code != CODE_INS_LBF) {
		PR_ERR("%s:%px,code err 0x%x\n",
		       __func__, vf, dvfm->header.code);
		return DI_ERR_UNSUPPORT;
	}

	if (!atomic_dec_and_test(&dvfm->c.flg_get)) {
		PR_ERR("%s:%px,flg_vpp %d\n", __func__, dvfm,
			atomic_read(&dvfm->c.flg_get));
		return DI_ERR_UNSUPPORT;
	}
	qback = &ds->lk_que_kback;
	dbg_plink2("fill_output_buffer:in:0x%px:0x%px,0x%x\n",
		   buffer, vf, buffer->flag);
	qback->ops.put(qback, vf);
	task_send_wk_timer(EDIM_WKUP_REASON_BACK_BUF);

	return DI_ERR_NONE;
}

// use buffer to get itf
static enum DI_ERRORTYPE dpvpp_fill_output_buffer2_l(struct di_buffer *buffer)
{
	struct dimn_itf_s *itf;
	struct dim_pvpp_hw_s *hw;

	hw = &get_datal()->dvs_prevpp.hw;

	if (!buffer || !buffer->vf) {
		PR_ERR("%s:\n", __func__);
		return DI_ERR_INDEX_OVERFLOW;
	}
	if (!hw->reg_nub) {
		PR_WARN("%s:1:0x%px\n", __func__, buffer);
		if (buffer->vf)
			PR_WARN("di:1-1:0x%px\n", buffer->vf);
	}
	itf = get_itf_vfm(buffer->vf);
	if (!hw->reg_nub)
		PR_WARN("%s:2:0x%px\n", __func__, itf);
	if (!itf)
		return DI_ERR_INDEX_NOT_ACTIVE;
	return dpvpp_fill_output_buffer(itf, buffer);
}

enum DI_ERRORTYPE dpvpp_fill_output_buffer2(struct di_buffer *buffer)
{
	enum DI_ERRORTYPE ret;
	ulong irq_flag = 0;

	spin_lock_irqsave(&lock_pvpp, irq_flag);
	ret = dpvpp_fill_output_buffer2_l(buffer);
	spin_unlock_irqrestore(&lock_pvpp, irq_flag);

	return ret;
}

static
void dpvpp_ins_fill_out(struct dimn_itf_s *itf)
{
	struct dimn_qs_cls_s *qready;
//	struct dimn_dvfm_s *dvfm = NULL;
//	struct vframe_s *vf;
	struct di_buffer *buffer;
	unsigned int cnt_rd, cnt_out;
	ud buf[DIM_LKIN_NUB];
	int i;
	struct dimn_dvfm_s *ndvfm;
	struct dim_prevpp_ds_s *ds;

	if (!itf || !itf->ds || itf->etype != EDIM_NIN_TYPE_INS)
		return;
	ds = itf->ds;
	qready = &ds->lk_que_ready;
	cnt_rd = qready->ops.count(qready);
	if (!cnt_rd)
		return;
	cnt_out = qready->ops.out(qready, &buf[0], cnt_rd);
	dim_print("%s:%d:%d\n", __func__, cnt_out, cnt_rd);
	for (i = 0; i < cnt_out; i++) {
		buffer = (struct di_buffer *)buf[i];
		dim_print("%s:di_buffer:0x%px\n", __func__, buffer);
		if (!buffer->vf) { //ary 0615 need check vf
			PR_WARN("%s:\t buffer-vf null\n", __func__);
			return;
		}
		dim_print("%s:\t vf:0x%px\n", __func__, buffer->vf);
		dim_print("%s:\t caller:0x%px\n", __func__, buffer->caller_data);
		if (buffer->flag & DI_FLAG_BUF_BY_PASS) {
			itf->nitf_parm.ops.fill_output_done(buffer);
			itf->c.sum_pst_get++;
			itf->nitf_parm.ops.empty_input_done(buffer);
			itf->c.sum_pre_put++;
//#if defined(DBG_QUE_IN_OUT) || defined(DBG_QUE_INTERFACE)
			dbg_plink2("ins:out_done:bypass:0x%px:0x%x\n",
				   buffer, buffer->flag);
			dbg_plink2("ins:empty_done:bypass:0x%px:0x%x\n",
				   buffer->vf, buffer->vf->type);
//#endif
		} else {
			/*tmp*/
			ndvfm = (struct dimn_dvfm_s *)buffer->vf->private_data;
			atomic_inc(&ndvfm->c.flg_get);
			buffer->flag |= DI_FLAG_P;//tmp

			itf->nitf_parm.ops.fill_output_done(buffer);
			itf->c.sum_pst_get++;
//#if defined(DBG_QUE_IN_OUT) || defined(DBG_QUE_INTERFACE)
			dbg_plink2("ins:out_done:0x%px,ch[0x%x]:%d:%d:0x%x\n",
				   buffer, buffer->mng.ch, buffer->mng.index,
				   ndvfm->c.cnt_in, buffer->flag);
			dbg_plink2("\t:vf:0x%x,0x%x,0x%x\n",
				   buffer->vf->type, buffer->vf->flag,
				   buffer->vf->di_flag);
//#endif
		}
	}
}

//---------------------------------------------------------
static unsigned int dpvpp_create_instance_id(unsigned int ch)
{
	unsigned int tmp_cnt;

	tmp_cnt = atomic_inc_return(&get_datal()->cnt_reg_pre_link);
	if (tmp_cnt < DIM_PLINK_INSTANCE_ID_MIN ||
	    tmp_cnt > DIM_PLINK_INSTANCE_ID_MAX) {
		atomic_set(&get_datal()->cnt_reg_pre_link,
			DIM_PLINK_INSTANCE_ID_MIN);
		tmp_cnt = atomic_read(&get_datal()->cnt_reg_pre_link);
	}
	tmp_cnt = tmp_cnt * DI_PLINK_CN_NUB + ch;
	return tmp_cnt;
}

static int dpvpp_reg_idle_ch(void)
{
	struct dimn_itf_s *itf;
	unsigned int id;
	int ret = -1;

	for (id = 0; id < DI_PLINK_CN_NUB; id++) {
		itf = &get_datal()->dvs_prevpp.itf[id];
		if (atomic_read(&itf->reg))
			continue;

		if (atomic_read(&itf->reg)) {
			PR_WARN("%s:check\n", __func__);
			continue;
		}

		atomic_set(&itf->reg, 1);
		ret = id;

		break;
	}
	dbg_plink1("%s:id[%d]\n", __func__, ret);

	return ret;
}

int dpvpp_create_internal(struct di_ch_s *pch)
{
	//struct dim_prevpp_ds_s *ds = NULL;
	struct dimn_itf_s *itf;
	int ret;
	struct di_init_parm *parm;

	/* try */
	ret = dpvpp_reg_idle_ch();
	if (ret < 0 || ret >= DI_PLINK_CN_NUB)
		return DI_ERR_REG_NO_IDLE_CH;
	itf = &get_datal()->dvs_prevpp.itf[ret];

	if (itf->ops_vfm &&
	    itf->ops_vfm->reg_pre_check &&
	    !itf->ops_vfm->reg_pre_check(itf)) {
		//to-do: resetting
		return DI_ERR_REG_NO_IDLE_CH;
	}

	/* reg count */
	itf->sum_reg_cnt = dpvpp_create_instance_id(pch->ch_id);

	dbg_mem("reg_cnt:[%d]\n", itf->sum_reg_cnt);

	itf->etype = pch->itf.etype; /* EDIM_NIN_TYPE_INS */
	itf->bind_ch = pch->ch_id;
	if (itf->etype == EDIM_NIN_TYPE_INS) {
		/*parm*/
		parm = &pch->itf.u.dinst.parm;
		memcpy(&itf->nitf_parm, parm, sizeof(itf->nitf_parm));
	} else {
		/* vframe path */
		itf->dvfm.name = pch->itf.dvfm.name;
	}

	if (!dpvpp_reg(itf))
		PR_ERR("%s:can't reg?\n", __func__);

	pch->itf.p_itf = itf;
	/* for tvp */
	check_tvp_state(pch);
	if (pch->is_tvp == 2)
		itf->c.is_tvp = 1;
	else
		itf->c.is_tvp = 0;
	if (itf->c.is_tvp) {
		itf->ds->o_hd.b.out_buf++;
		itf->ds->o_uhd.b.out_buf++;
	}
	dbg_plink1("%s:ch[%d]:id[%d] end tvp[%d]:%d:<%d,%d>\n",
		__func__, pch->ch_id, itf->id,
		itf->c.is_tvp, itf->sum_reg_cnt,
		itf->ds->o_hd.b.out_buf,
		itf->ds->o_uhd.b.out_buf);

	return DIM_PRE_VPP_NUB;
}

int dpvpp_destroy_internal(struct dimn_itf_s *itf)
{

	if (!itf) {
		PR_ERR("%s:index overflow\n", __func__);
		return DI_ERR_INDEX_OVERFLOW;
	}

	dbg_plink1("%s:\n", "dpvpp:ins:des");

	if (!atomic_read(&itf->reg)) {
		PR_WARN("%s:duplicate ureg\n", __func__);
		return DI_ERR_INDEX_NOT_ACTIVE;
	}
	dpvpp_dct_unreg(itf->bind_ch);
	if (itf->ops_vfm && itf->ops_vfm->unreg_trig)
		itf->ops_vfm->unreg_trig(itf);

	atomic_set(&itf->reg, 0);
	dpvpp_unreg_val(itf);

	dbg_plink2("%s:id[%d]\n", __func__, itf->id);
	return 0;
}

bool dpvpp_is_allowed(void)
{
	return get_datal()->dvs_prevpp.allowed;
}

bool dpvpp_is_insert(void)
{
	return get_datal()->dvs_prevpp.insert;
}

bool dpvpp_is_en_polling(void)
{
	return get_datal()->dvs_prevpp.en_polling;
}

unsigned int dpvpp_bypass_check(struct vframe_s *vfm, bool first)
{
	unsigned int reason = 0;
	bool is_4k = false;
	unsigned int x, y;

	dim_vf_x_y(vfm, &x, &y);
	if (x > 1920 || y > 1088)
		is_4k = true;
	dbg_dbg("vfm_type:0x%x", vfm->type);
	if (vfm->type & DIM_BYPASS_VF_TYPE)
		reason = EPVPP_BYPASS_REASON_TYPE;
	if (VFMT_IS_P(vfm->type) &&
		   (vfm->width > 1920	||
		    vfm->height > (1080 + 8)))
		reason = EPVPP_BYPASS_REASON_SIZE_L;
	if (vfm->flag & VFRAME_FLAG_GAME_MODE)
		reason = EPVPP_BYPASS_REASON_GAME_MODE;
	if (vfm->flag & VFRAME_FLAG_HIGH_BANDWIDTH)
		reason = EPVPP_BYPASS_REASON_HIGH_BANDWIDTH;
	if (VFMT_IS_I(vfm->type))
		reason = EPVPP_BYPASS_REASON_I;
	if (cfgg(4K)) {
		if (vfm->width > 3840 ||
		    vfm->height > 2160)
			reason = EPVPP_BYPASS_REASON_SIZE_L;
	}
	if (IS_COMP_MODE(vfm->type)) {
		if (dim_afds() && !dim_afds()->is_supported_plink())
			reason = EPVPP_BYPASS_REASON_NO_AFBC;
	}
	if (dpvpp_is_bypass())
		reason = EPVPP_BYPASS_REASON_DBG;
	if (vfm->src_fmt.sei_magic_code == SEI_MAGIC_CODE &&
		(vfm->src_fmt.fmt == VFRAME_SIGNAL_FMT_DOVI ||
		vfm->src_fmt.fmt == VFRAME_SIGNAL_FMT_DOVI_LL))
		reason = EPVPP_BYPASS_REASON_DV_PATH;

	/* First EOS, vframe information maybe invalid. Just ignore it to create pre-link*/
	if (VFMT_IS_EOS(vfm->type) && first && !reason)
		return 0;

	/* If vframe is EOS, information below maybe invalid */
	if (vfm->duration < 1600)
		reason = EPVPP_BYPASS_REASON_120HZ;
	if (vfm->width < 128 || vfm->height < 16)
		reason = EPVPP_BYPASS_REASON_SIZE_S;
	if (VFMT_IS_EOS(vfm->type))
		reason = EPVPP_BYPASS_REASON_EOS;
	return reason;
}

bool dpvpp_try_reg(struct di_ch_s *pch, struct vframe_s *vfm)
{
	bool ponly_enable = false;
	unsigned int reason = 0;

	if (!dpvpp_ops()	||
	    !dpvpp_is_allowed()	||
	    !dpvpp_is_insert())
		return false;
	reason = dpvpp_bypass_check(vfm, true);
	dim_bypass_set(pch, 0, reason);
	if (reason != 0)
		return false;
	//check  ponly:
	if ((vfm->flag & VFRAME_FLAG_DI_P_ONLY)/* || bget(&dim_cfg, 1)*/)
		ponly_enable = true;

	if (!ponly_enable &&
	    cfgg(PONLY_MODE) == 1 &&
	    (vfm->type & VIDTYPE_TYPEMASK) == VIDTYPE_PROGRESSIVE &&
	    !(vfm->type & VIDTYPE_FORCE_SIGN_IP_JOINT)) {
		ponly_enable = true;
	}

	if (get_datal()->fg_bypass_en && vfm->fgs_valid)
		ponly_enable = false;

	if (!ponly_enable) {
		dbg_plink1("%s:not ponly:%d:0x%x, fgs_valid:[0x%x]\n",
			__func__,
			cfgg(PONLY_MODE), vfm->type, vfm->fgs_valid);
		return false;
	}
	if (dpvpp_create_internal(pch) < 0) {
		dbg_plink1("%s:failed\n", __func__);
		return false;
	}
	return true;
}

/*di_display_pre_vpp_link*/
int dim_pre_vpp_link_display(struct vframe_s *vfm,
			  struct pvpp_dis_para_in_s *in_para, void *out_para)
{
	int ret = 0;
	struct pvpp_dis_para_in_s *in;
	static int last_x, last_v;

	di_g_plink_dbg()->display_cnt++;
	if (in_para && in_para->unreg_bypass) {
		if (dpvpp_ops_api() && dpvpp_ops_api()->display_unreg_bypass)
			ret = dpvpp_ops_api()->display_unreg_bypass();
		else
			PR_ERR("%s:no unreg_bypass?\n", __func__);
		return ret;
	}
	if (!vfm)
		return 0;
	dbg_plink3("%s:0x%px, 0x%px:\n", __func__, vfm, in_para);
	/* dbg only */
	if (in_para) {
		dimp_set(edi_mp_prelink_hold_line, in_para->follow_hold_line);
		if (last_x != in_para->win.x_end ||
		    last_v != in_para->win.y_end) {
			dbg_plink1("itf:disp:w<%d,%d><%d,%d>m%d\n",
				last_x,
				last_v,
				in_para->win.x_end,
				in_para->win.y_end,
				in_para->dmode);
			last_x = in_para->win.x_end;
			last_v = in_para->win.y_end;
		}
	}
	if (dpvpp_ops_api() && dpvpp_ops_api()->display) {
		ret = dpvpp_ops_api()->display(vfm, in_para, out_para);
		in = in_para;
		dbg_plink3("%s:ret:0x%x:\n", "api:display", ret);
		if (in)
			dbg_plink3("\tm[%d]:\n", in->dmode);

		if (di_g_plink_dbg()->display_sts != ret) {
			dbg_plink1("%s:0x%x->0x%x\n", "api:display",
			       di_g_plink_dbg()->display_sts, ret);
			di_g_plink_dbg()->display_sts = ret;
		}
	}

	return ret;
}

//EXPORT_SYMBOL(di_display_pre_vpp_link);

int dpvpp_check_vf(struct vframe_s *vfm)
{
	int ret = 0;
#if defined(DBG_QUE_IN_OUT) || defined(DBG_QUE_INTERFACE)
	static ud vfm_last; //dbg only
	ud vfm_cur;
#endif
	if (!vfm) {
		PR_WARN("%s:no vf\n", __func__);
		return -1;
	}
	if (dpvpp_ops_api() && dpvpp_ops_api()->check_vf)
		ret = dpvpp_ops_api()->check_vf(vfm);

	if (di_g_plink_dbg()->flg_check_vf != ret) { /*dbg*/
		dbg_plink1("%s:0x%x->0x%x\n", "api:check vf",
		       di_g_plink_dbg()->flg_check_vf,
		       ret);
		di_g_plink_dbg()->flg_check_vf = ret;
	}

#if defined(DBG_QUE_IN_OUT) || defined(DBG_QUE_INTERFACE)
	vfm_cur = (ud)vfm;
	if (vfm_cur != vfm_last) {
		dbg_plink3("%s:0x%px:0x%x\n", "api:check",
			   vfm,
			   ret);
		dbg_plink3("\tfunc:0x%px\n", vfm->process_fun);
		vfm_last = vfm_cur;
	}
#endif
	return ret;
}

int dpvpp_check_di_act(void)
{
	int ret = EPVPP_ERROR_DI_NOT_REG;

	if (dpvpp_ops_api() && dpvpp_ops_api()->check_di_act)
		ret = dpvpp_ops_api()->check_di_act();
	if (di_g_plink_dbg()->flg_check_di_act != ret) {
		dbg_plink1("%s:%d->%d\n", "api:check act",
		       di_g_plink_dbg()->flg_check_di_act,
		       ret);
		di_g_plink_dbg()->flg_check_di_act = ret;
	}

	return ret;
}

int dpvpp_sw(bool on)
{
	int ret = 0;

	if (dpvpp_ops_api() && dpvpp_ops_api()->vpp_sw)
		ret = dpvpp_ops_api()->vpp_sw(on);
	if (di_g_plink_dbg()->flg_sw != on) {
		dbg_plink1("%s:%d->%d:%d\n", "api:sw",
		       di_g_plink_dbg()->flg_sw,
		       on, ret);
		di_g_plink_dbg()->flg_sw = on;
	}

	return ret;
}

unsigned int dpvpp_get_ins_id(void)
{
	unsigned int ret = 0;
	struct dim_pvpp_hw_s *hw;

	if (!get_dim_de_devp() || !get_dim_de_devp()->data_l)
		return 0;

	hw = &get_datal()->dvs_prevpp.hw;
	ret = (unsigned int)atomic_read(&hw->link_instance_id);
	return ret;
}

struct canvas_config_s *dpvpp_get_mem_cvs(unsigned int index)
{
	struct dim_pvpp_hw_s *hw;
	struct canvas_config_s *pcvs;

	hw = &get_datal()->dvs_prevpp.hw;

	pcvs = &hw->buf_cfg[EPVPP_BUF_CFG_T_HD_F].dbuf_wr_cfg[index][0];

	return pcvs;
}

/************************************/
/* also see enum EBLK_TYPE */
const char * const name_blk_t[] = {
	"hd_full",	/* 1080p yuv 422 10 bit */
	"hd_full_tvp",
	"uhd_f_afbce",
	"uhd_f_afbce_tvp",
	"uhd_full",
	"uhd_full_tvp",
	"hd_nv21",
	"hd_nv21_tvp",
	"l_dct",
	"l_dct_tvp",
	"s_afbc_table_4k"
};

static const char *get_name_blk_t(unsigned int idx)
{
	const char *p = "overflow";

	if (idx < ARRAY_SIZE(name_blk_t))
		p = name_blk_t[idx];
	return p;
}

struct c_mm_p_cnt_out_s *m_rd_mm_cnt(unsigned int id)
{
	struct dd_s *dd;

	dd = get_dd();
	if (!dd || id >= K_BLK_T_NUB)
		return NULL;

	return &dd->blk_cfg[id];
}

struct c_cfg_afbc_s *m_rd_afbc(unsigned int id)
{
	struct dd_s *dd;

	dd = get_dd();
	if (!dd || id >= K_AFBC_T_NUB)
		return NULL;

	return &dd->afbc_i[id];
}

struct c_cfg_dct_s *m_rd_dct(void)
{
	struct dd_s *dd;

	dd = get_dd();

	return &dd->dct_i;
}

bool dim_mm_alloc_api2(struct mem_a_s *in_para, struct dim_mm_s *o)
{
	bool ret = false;
//	int cma_mode;
	struct di_dev_s *de_devp = get_dim_de_devp();
	u64 timer_st, timer_end, diff; //debug only
	struct c_cfg_blki_s *blk_i;

#ifdef CONFIG_CMA
	if (!in_para || !in_para->inf || !o) {
		PR_ERR("%s:no input\n", __func__);
		return false;
	}
	blk_i = in_para->inf;
	timer_st = cur_to_msecs();//dbg

	if (blk_i->mem_from == MEM_FROM_CODEC) {//
		ret = mm_codec_alloc(DEVICE_NAME,
				     blk_i->page_size,
				     4,
				     o,
				     blk_i->tvp);
	} else if (blk_i->mem_from == MEM_FROM_CMA_DI) { //
		ret = mm_cma_alloc(&de_devp->pdev->dev, blk_i->page_size, o);
	} else if (blk_i->mem_from == MEM_FROM_CMA_C) { //
		ret = mm_cma_alloc(NULL, blk_i->page_size, o);
	} else {
		PR_ERR("%s:not finish function:%d:\n",
			__func__, blk_i->mem_from);
	}

	timer_end = cur_to_msecs();//dbg

	diff = timer_end - timer_st;
	if (!ret) {
		PR_ERR("a:<%d,%d,0x%x>:%s,%s\n",
			blk_i->mem_from, blk_i->tvp, blk_i->page_size,
			in_para->owner, in_para->note);
	}
	dbg_plink1("a:%ums:<%d,%d>:<0x%lx,0x%x>:%s,%s\n", (unsigned int)diff,
		 blk_i->mem_from, blk_i->tvp,
		 o->addr, blk_i->mem_size,
		in_para->owner, in_para->note);

#endif
	return ret;
}

bool dim_mm_release_api2(struct mem_r_s *in_para)
{
	bool ret = false;
	struct di_dev_s *de_devp = get_dim_de_devp();
	struct c_blk_m_s *blk;

#ifdef CONFIG_CMA
	if (!in_para || !in_para->blk) {
		PR_ERR("%s:no input\n", __func__);
		return false;
	}
	blk = in_para->blk;
	if (!blk->inf) {
		PR_ERR("%s: blk no inf\n", __func__);
		return false;
	}
	if (blk->inf->mem_from == MEM_FROM_CODEC) {
		codec_mm_free_for_dma(DEVICE_NAME, blk->mem_start);
		ret = true;
	} else if (blk->inf->mem_from == MEM_FROM_CMA_DI) {
		ret = dma_release_from_contiguous(&de_devp->pdev->dev,
						  blk->pages,
						  blk->inf->page_size);
	} else if (blk->inf->mem_from == MEM_FROM_CMA_C) {
		ret = dma_release_from_contiguous(NULL,
						  blk->pages,
						  blk->inf->page_size);
	}
	dbg_plink1("r:<%d,%d,0x%x>:%s\n",
		 blk->inf->mem_from, blk->inf->tvp, blk->inf->page_size,
		in_para->note);
#endif
	return ret;
}

int m_a_blkt_cp(unsigned int ch, unsigned char *blkt_n)
{
	struct c_cfg_blkt_s *pblkt;
	struct dd_s *d_dd;

	if (!get_datal() || !get_datal()->dd)
		return -1;
	d_dd = (struct dd_s *)get_datal()->dd;
	if (ch >= __CH_MAX) {
		PR_ERR("%s:overflow:%d\n", __func__, ch);
		return -2;
	}
	pblkt = &d_dd->blkt_d[ch];

	if (!pblkt) {
		PR_ERR("%s: no input?\n", __func__);
		return -1;
	}

	mutex_lock(&pblkt->lock_cfg);
	memcpy(&pblkt->blkt_n[0], blkt_n, sizeof(pblkt->blkt_n));
	pblkt->flg_update = true;
	mutex_unlock(&pblkt->lock_cfg);
	mtask_wake_m();
	return 0;
}

static int count_mm_afbc(struct c_mm_p_cnt_in_s *i_cfg,
			struct c_cfg_afbc_s *o_cfg)
{
	unsigned int afbc_info_size = 0, afbc_tab_size = 0;
	unsigned int afbc_buffer_size = 0, blk_total = 0;
	unsigned int width, height;
	bool is_yuv420_10 = false;

	if (!dim_afds() || !dim_afds()->cnt_info_size ||
	    !i_cfg || !o_cfg) {
		PR_WARN("%s:no api\n", __func__);
		return 0;
	}

	width	= i_cfg->w;
	height	= i_cfg->h;

	afbc_info_size =
		dim_afds()->cnt_info_size(width,
					  height,
					  &blk_total);

	if (i_cfg->mode == EDPST_MODE_420_10BIT)
		is_yuv420_10 = true;

	if (is_yuv420_10)
		afbc_buffer_size =
		dim_afds()->cnt_buf_size(0x20, blk_total);
	else
		afbc_buffer_size =
			dim_afds()->cnt_buf_size(0x21, blk_total);
	afbc_buffer_size = roundup(afbc_buffer_size, PAGE_SIZE);

	afbc_tab_size =
		dim_afds()->cnt_tab_size(afbc_buffer_size);

	o_cfg->size_i	= afbc_info_size;
	o_cfg->size_table = afbc_tab_size;
	o_cfg->size_buffer = afbc_buffer_size;
	return 0;
}

/*copy from cnt_mm_info_simple_p */
static int count_mm_p(struct c_mm_p_cnt_in_s *i_cfg,
		struct c_mm_p_cnt_out_s *o_cfg)
{
	unsigned int tmpa, tmpb;
	unsigned int height;
	unsigned int width;
	unsigned int canvas_height;

	unsigned int nr_width;
	unsigned int canvas_align_width = 32;

	if (!i_cfg || !o_cfg) {
		PR_ERR("%s:no input\n", __func__);
		return -1;
	}
	height	= i_cfg->h;
	canvas_height = roundup(height, 32);
	width	= i_cfg->w;
	nr_width = width;
	o_cfg->mode = i_cfg->mode;
	/**********************************************/
	/* count buf info */
	/**********************************************/
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A))
		canvas_align_width = 64;

	if (i_cfg->mode == EDPST_MODE_422_10BIT_PACK)
		nr_width = (width * 5) / 4;
	else if (i_cfg->mode == EDPST_MODE_422_10BIT)
		nr_width = (width * 3) / 2;
	else
		nr_width = width;

	/* p */
	//nr_width = roundup(nr_width, canvas_align_width);
	o_cfg->dbuf_hsize = roundup(width, canvas_align_width);

	if (i_cfg->mode == EDPST_MODE_NV21_8BIT) {
		nr_width = roundup(nr_width, canvas_align_width);
		tmpa = (nr_width * canvas_height) >> 1;/*uv*/
		tmpb = roundup(tmpa, PAGE_SIZE);
		tmpa = roundup(nr_width * canvas_height, PAGE_SIZE);

		o_cfg->off_uv		= tmpa;
		o_cfg->size_total	= tmpa + tmpb;
		o_cfg->size_page = o_cfg->size_total >> PAGE_SHIFT;
		o_cfg->cvs_w	= nr_width;
		o_cfg->cvs_h	= canvas_height;
	} else {
		/* 422 */
		tmpa = roundup(nr_width * canvas_height * 2, PAGE_SIZE);
		o_cfg->size_total = tmpa;
		o_cfg->size_page = o_cfg->size_total >> PAGE_SHIFT;
		o_cfg->cvs_w	= nr_width << 1;
		o_cfg->cvs_h	= canvas_height;
		o_cfg->off_uv	= o_cfg->size_total;
	}

	return 0;
}

void m_polling(void)
{
	struct dd_s *d_dd;
	struct di_data_l_s *pdata;
	int i, j;
	struct c_cfg_blkt_s *pblkt;
	unsigned int flash_cnt = 0, buf_cnt;
	int alloc_cnt = 0;
	struct blk_s *blk;
	struct c_cfg_blki_s *blk_i;
	struct dim_mm_s oret;
	bool aret, flg_a = false;
	struct mem_r_s r_para;
	struct mem_a_s a_para;

	if (!IS_IC_SUPPORT(PRE_VPP_LINK) || !cfgg(EN_PRE_LINK))
		return;
	pdata = get_datal();
	if (!pdata)
		return;
	d_dd = (struct dd_s *)pdata->dd;

	/* flash from d */
	for (i = 0; i < __CH_MAX; i++) {
		pblkt = &d_dd->blkt_d[i];
		if (pblkt && pblkt->flg_update) {
			mutex_lock(&pblkt->lock_cfg);
			memcpy(&d_dd->blkt_m[i][0], &pblkt->blkt_n[0],
				sizeof(d_dd->blkt_m[i]));
			pblkt->flg_update = 0;
			mutex_unlock(&pblkt->lock_cfg);
			flash_cnt++;
		} else {
			continue;
		}
	}

	/* count */
	if (flash_cnt) {
		for (j = 0; j < K_BLK_T_NUB; j++) {
			buf_cnt = 0;
			for (i = 0; i < __CH_MAX; i++)
				buf_cnt += d_dd->blkt_m[i][j];
			d_dd->blkt_tgt[j] = buf_cnt;
		}
		dbg_mem2("%s:flash_cnt:%d\n", __func__, flash_cnt);
	}

	/* polling release */
	for (j = 0; j < K_BLK_T_NUB; j++) {
		if (d_dd->blkt_tgt[j] < d_dd->blkt_crr[j]) {
			buf_cnt = d_dd->blkt_crr[j] - d_dd->blkt_tgt[j];
			for (i = 0; i < buf_cnt; i++) {
				//blk = qblk_get(&d_dd->f_blk_t[j]);
				blk = blk_o_get(d_dd, j);
				if (!blk) {
					PR_WARN("%s:release:[%d]:%d:%d:break\n",
						__func__, j, i, buf_cnt);
					break;
				}
				memset(&r_para, 0, sizeof(r_para));
				r_para.blk = &blk->c.b.blk_m;//note: tmp;
				r_para.note = get_name_blk_t(j);
				dim_mm_release_api2(&r_para);
				memset(&blk->c, 0, sizeof(blk->c));
				blk_idle_put(d_dd, blk);
				d_dd->blkt_crr[j]--;
			}
		} else if (d_dd->blkt_tgt[j] > d_dd->blkt_crr[j]) {
			alloc_cnt += (d_dd->blkt_tgt[j] - d_dd->blkt_crr[j]);
		}
	}
	if (alloc_cnt <= 0)
		return;
	/* polling alloc */
	for (j = 0; j < K_BLK_T_NUB; j++) {
		if (d_dd->blkt_tgt[j] > d_dd->blkt_crr[j]) {
			blk = blk_idle_get(d_dd);
			if (!blk) {
				PR_ERR("%s:alloc:no idle\n", __func__);
				break;
			}
			//PR_INF("idle:0x%x\n", blk);
			blk_i = &d_dd->blki[j];
			memset(&oret, 0, sizeof(oret));
			memset(&a_para, 0, sizeof(a_para));
			a_para.inf = blk_i;
			a_para.owner = get_name_blk_t(j);
			a_para.note	= "tmp";
			aret = dim_mm_alloc_api2(&a_para, &oret);
			if (aret) {
				blk->c.blk_typ = j;
				blk->c.b.blk_m.inf = blk_i;
				blk->c.b.blk_m.flg_alloc = true;
				blk->c.b.blk_m.mem_start = oret.addr;
				blk->c.b.blk_m.pages	= oret.ppage;
				blk->c.st_id = EBLK_ST_ALLOC;
				d_dd->blkt_crr[j]++;
				alloc_cnt--;
				//PR_INF("alloc:%s:\n", a_para.owner);

				blk_o_put(d_dd, j, blk);
				flg_a = true;
			} else {
				PR_WARN("alloc failed\n");
				blk_idle_put(d_dd, blk);
			}
			break;
		}
	}

	if (alloc_cnt > 0) {
		//PR_INF("%s:trig:%d:\n", __func__, alloc_cnt);
		mtask_wake_m();
	}
	if (flg_a)
		task_send_ready(31);
}

static unsigned int cnt_dct_buf_size(struct c_cfg_dct_s *dct_inf)
{
	int grd_size = SZ_512K;		/*(81*8*16 byte)*45 = 455K*/
	int yds_size = SZ_512K + SZ_32K;    /*960 * 576 = 540K*/
	int cds_size = yds_size / 2;	    /*960 * 576 / 2= 270K*/
	int total_size = grd_size + yds_size + cds_size;

	if (!dct_inf) {
		PR_ERR("%s:\n", __func__);
		return 0;
	}
	dct_inf->grd_size	= grd_size;
	dct_inf->yds_size	= yds_size;
	dct_inf->cds_size	= cds_size;
	dct_inf->size_one	= total_size;

	return total_size;
}

static void dim_reg_buf_i(void)
{
	struct di_data_l_s *pdata;
	struct dd_s *d_dd;
//	struct c_cfg_blkt_s *pblkt;
	struct c_cfg_blki_s *blk_i;
	struct c_mm_p_cnt_in_s *i_cfg, i_cfg_d;
	struct c_mm_p_cnt_out_s *o_cfg, o_cfg_d;
	struct c_cfg_afbc_s *afbc_cfg, afbc_cfg_d;
	int i;
	unsigned int size_tt;
	bool flg_special;

	pdata = get_datal();
	if (!pdata || !pdata->dd)
		return;
	d_dd = (struct dd_s *)pdata->dd;
	i_cfg = &i_cfg_d;
	o_cfg = &o_cfg_d;
	afbc_cfg = &afbc_cfg_d;

	/* count afbc infor */
	memset(i_cfg, 0, sizeof(*i_cfg));

	for (i = 0; i < K_AFBC_T_NUB; i++) {
		memset(afbc_cfg, 0, sizeof(*afbc_cfg));

		if (i == EAFBC_T_HD_FULL) {
			i_cfg->w = 1920;
			i_cfg->h = 1088;
			i_cfg->mode = EDPST_MODE_422_10BIT_PACK;
		} else if (i == EAFBC_T_4K_FULL) {
			i_cfg->w = 3840;
			i_cfg->h = 2160;
			i_cfg->mode = EDPST_MODE_422_10BIT_PACK;
		} else if (i ==  EAFBC_T_4K_YUV420_10) {
			i_cfg->w = 3840;
			i_cfg->h = 2160;
			i_cfg->mode = EDPST_MODE_420_10BIT;
		} else { /* to-do other format*/
			PR_WARN("%s:type %d is replay by %d\n",
				__func__, i, EAFBC_T_4K_FULL);
			i_cfg->mode = EDPST_MODE_422_10BIT_PACK;
		}
		count_mm_afbc(i_cfg, afbc_cfg);
		memcpy(&d_dd->afbc_i[i], afbc_cfg, sizeof(d_dd->afbc_i[i]));
	}
	/* count dct */
	d_dd->dct_i.nub = cfgg(POST_NUB) + 1;//DIM_P_LINK_DCT_NUB;
	d_dd->dct_i.size_one = cnt_dct_buf_size(&d_dd->dct_i);
	d_dd->dct_i.size_total = d_dd->dct_i.size_one * d_dd->dct_i.nub;
	/* count buffer base */
	for (i = 0; i < K_BLK_T_NUB; i++) {
		blk_i = &d_dd->blki[i];
		memset(i_cfg, 0, sizeof(*i_cfg));
		memset(o_cfg, 0, sizeof(*o_cfg));
		flg_special = false;
		if (i == EBLK_T_HD_FULL) {
			i_cfg->w = 1920;
			i_cfg->h = 1088;
			i_cfg->mode = EDPST_MODE_422_10BIT_PACK;

			count_mm_p(i_cfg, o_cfg);
			memcpy(&d_dd->blk_cfg[i], o_cfg, sizeof(d_dd->blk_cfg[i]));
			blk_i->blk_t = i;
			blk_i->mem_size = o_cfg->size_total;
			blk_i->page_size = o_cfg->size_page;
			blk_i->tvp	= false;
			blk_i->mem_from	= MEM_FROM_CODEC;
			blk_i->cnt_cfg = &d_dd->blk_cfg[i];
		} else if (i == EBLK_T_UHD_FULL) {
			i_cfg->w = 3840;
			i_cfg->h = 2160;
			i_cfg->mode = EDPST_MODE_422_10BIT_PACK;

			count_mm_p(i_cfg, o_cfg);
			memcpy(&d_dd->blk_cfg[i], o_cfg, sizeof(d_dd->blk_cfg[i]));
			blk_i->blk_t = i;
			blk_i->mem_size = o_cfg->size_total;
			blk_i->page_size = o_cfg->size_page;
			blk_i->tvp	= false;
			blk_i->mem_from	= MEM_FROM_CODEC;
			blk_i->cnt_cfg = &d_dd->blk_cfg[i];
		} else if (i == EBLK_T_HD_NV21) {
			i_cfg->w = 1920;
			i_cfg->h = 1088;
			i_cfg->mode = EDPST_MODE_NV21_8BIT;

			count_mm_p(i_cfg, o_cfg);
			memcpy(&d_dd->blk_cfg[i], o_cfg, sizeof(d_dd->blk_cfg[i]));
			blk_i->blk_t = i;
			blk_i->mem_size = o_cfg->size_total;
			blk_i->page_size = o_cfg->size_page;
			blk_i->tvp	= false;
			blk_i->mem_from	= MEM_FROM_CODEC;
			blk_i->cnt_cfg = &d_dd->blk_cfg[i];
		} else if (i == EBLK_T_LINK_DCT) {
			d_dd->blk_cfg[i].size_total = d_dd->dct_i.size_total;
			d_dd->blk_cfg[i].size_page =
				d_dd->blk_cfg[i].size_total >> PAGE_SHIFT;

			blk_i->blk_t = i;
			blk_i->mem_size = d_dd->blk_cfg[i].size_total;
			blk_i->page_size = d_dd->blk_cfg[i].size_page;
			blk_i->tvp	= false;
			blk_i->mem_from	= MEM_FROM_CODEC;
			blk_i->cnt_cfg = &d_dd->blk_cfg[i];
		} else if (i == EBLK_T_S_LINK_AFBC_T_FULL) {
			/**/
			size_tt = d_dd->afbc_i[EAFBC_T_4K_FULL].size_table;
			size_tt = size_tt * DPVPP_BUF_NUB;
			size_tt = roundup(size_tt, PAGE_SIZE);
			blk_i->blk_t = i;
			blk_i->mem_from = MEM_FROM_CODEC;
			blk_i->tvp = false;
			blk_i->mem_size = size_tt;
			blk_i->page_size = size_tt >> PAGE_SHIFT;
			memset(&d_dd->blk_cfg[i], 0, sizeof(d_dd->blk_cfg[i]));
			d_dd->blk_cfg[i].mode = EDPST_MODE_420_10BIT;
			d_dd->blk_cfg[i].size_total = blk_i->mem_size;
			d_dd->blk_cfg[i].size_page = blk_i->page_size;
			//note: here: buffer is larger than uhd_full,
			//not set ->cnt_cfg
		} else {
			flg_special = true;
		}
		if (!flg_special)  //dbg only
			dbg_plink2("%s:%d:cnt\n", __func__, i);
	}
	/* count buffer special */
	for (i = 0; i < K_BLK_T_NUB; i++) {
		blk_i = &d_dd->blki[i];
		if (i == EBLK_T_HD_FULL_TVP) {
			memcpy(blk_i, &d_dd->blki[EBLK_T_HD_FULL], sizeof(*blk_i));
			blk_i->blk_t = EBLK_T_HD_FULL_TVP;
			blk_i->tvp = true;
			memcpy(&d_dd->blk_cfg[i], &d_dd->blk_cfg[EBLK_T_HD_FULL],
				sizeof(d_dd->blk_cfg[i]));
		} else if (i == EBLK_T_UHD_FULL_TVP) {
			memcpy(blk_i, &d_dd->blki[EBLK_T_UHD_FULL], sizeof(*blk_i));
			blk_i->blk_t = EBLK_T_UHD_FULL_TVP;
			blk_i->tvp = true;
			memcpy(&d_dd->blk_cfg[i], &d_dd->blk_cfg[EBLK_T_UHD_FULL],
				sizeof(d_dd->blk_cfg[i]));
		} else if (i == EBLK_T_HD_NV21_TVP) {
			memcpy(blk_i, &d_dd->blki[EBLK_T_HD_NV21], sizeof(*blk_i));
			blk_i->blk_t = EBLK_T_HD_NV21_TVP;
			blk_i->tvp = true;
			memcpy(&d_dd->blk_cfg[i], &d_dd->blk_cfg[EBLK_T_HD_NV21],
				sizeof(d_dd->blk_cfg[i]));
		} else if (i == EBLK_T_UHD_FULL_AFBCE) {
			/* blk_cfg */
			memcpy(&d_dd->blk_cfg[i], &d_dd->blk_cfg[EBLK_T_UHD_FULL],
				sizeof(d_dd->blk_cfg[i]));

			size_tt = d_dd->afbc_i[EAFBC_T_4K_FULL].size_i +
				d_dd->afbc_i[EAFBC_T_4K_FULL].size_buffer;
			blk_i->blk_t = i;
			blk_i->mem_size = size_tt;
			blk_i->page_size = blk_i->mem_size >> PAGE_SHIFT;

			d_dd->blk_cfg[i].size_total = blk_i->mem_size;
			d_dd->blk_cfg[i].size_page = blk_i->page_size;

			blk_i->tvp	= false;
			blk_i->mem_from	= MEM_FROM_CODEC;
			blk_i->cnt_cfg = &d_dd->blk_cfg[i];
			//dbg_str_mm_cnt(o_cfg, "a");
		} else if (i == EBLK_T_UHD_FULL_AFBCE_TVP) {
			memcpy(blk_i, &d_dd->blki[EBLK_T_UHD_FULL_AFBCE], sizeof(*blk_i));
			blk_i->blk_t = i;
			blk_i->tvp = true;
			memcpy(&d_dd->blk_cfg[i], &d_dd->blk_cfg[EBLK_T_UHD_FULL_AFBCE],
				sizeof(d_dd->blk_cfg[i]));
		} else if (i == EBLK_T_LINK_DCT_TVP) {
			memcpy(blk_i, &d_dd->blki[EBLK_T_LINK_DCT], sizeof(*blk_i));
			blk_i->blk_t = i;
			blk_i->tvp = true;
			memcpy(&d_dd->blk_cfg[i], &d_dd->blk_cfg[EBLK_T_LINK_DCT],
				sizeof(d_dd->blk_cfg[i]));
		}
	}
}

bool dd_exit(void)
{
	struct dd_s *d_dd;
	struct di_data_l_s *pdata;
	int i;

	pdata = get_datal();
	if (!pdata)
		return false;
	d_dd = (struct dd_s *)pdata->dd;

	UCF_RELEASE(&d_dd->f_dis_free);
	UCF_RELEASE(&d_dd->f_blk_idle);
	for (i = 0; i < K_BLK_T_NUB; i++)
		UCF_RELEASE(&d_dd->f_blk_t[i]);

	kfree(d_dd);
	pdata->dd = NULL;
	return true;
}

bool dd_probe(void)
{
	struct dd_s *d_dd;
	struct di_data_l_s *pdata;
	unsigned int err_cnt = 0;
	int i;
	struct c_hd_s *hd;
	struct blk_s *blk;

	pdata = get_datal();
	if (!pdata)
		return false;

	d_dd = kzalloc(sizeof(*d_dd), GFP_KERNEL);
	if (!d_dd) {
		PR_ERR("%s fail to allocate memory.\n", __func__);
		goto fail_kmalloc_dd;
	}
	pdata->dd = d_dd;
	/* mutex */
	for (i = 0; i < __CH_MAX; i++)
		mutex_init(&d_dd->blkt_d[i].lock_cfg);

	/* que */
	err_cnt = 0;
	i = 0;
	err_cnt += UCF_INIT(&d_dd->f_dis_free,	__SIZE_DIS_FREE, i++, "dis_free");
	err_cnt += UCF_INIT(&d_dd->f_blk_idle,	__SIZE_BLK_IDLE, i++, "blk_idle");
	err_cnt += UCF_INIT(&d_dd->f_blk_t[EBLK_T_HD_FULL],
		__SIZE_BLK_PLINK_HD, i++, get_name_blk_t(EBLK_T_HD_FULL));
	err_cnt += UCF_INIT(&d_dd->f_blk_t[EBLK_T_HD_FULL_TVP],
		__SIZE_BLK_PLINK_HD, i++, get_name_blk_t(EBLK_T_HD_FULL_TVP));
	err_cnt += UCF_INIT(&d_dd->f_blk_t[EBLK_T_UHD_FULL_AFBCE],
		__SIZE_BLK_PLINK_UHD, i++, get_name_blk_t(EBLK_T_UHD_FULL_AFBCE));
	err_cnt += UCF_INIT(&d_dd->f_blk_t[EBLK_T_UHD_FULL_AFBCE_TVP],
		__SIZE_BLK_PLINK_UHD, i++,
		get_name_blk_t(EBLK_T_UHD_FULL_AFBCE_TVP));
	err_cnt += UCF_INIT(&d_dd->f_blk_t[EBLK_T_UHD_FULL],
		__SIZE_BLK_PLINK_UHD, i++, get_name_blk_t(EBLK_T_UHD_FULL));
	err_cnt += UCF_INIT(&d_dd->f_blk_t[EBLK_T_UHD_FULL_TVP],
		__SIZE_BLK_PLINK_UHD, i++, get_name_blk_t(EBLK_T_UHD_FULL_TVP));
	err_cnt += UCF_INIT(&d_dd->f_blk_t[EBLK_T_HD_NV21],
		__SIZE_BLK_PLINK_UHD, i++, get_name_blk_t(EBLK_T_HD_NV21));
	err_cnt += UCF_INIT(&d_dd->f_blk_t[EBLK_T_HD_NV21_TVP],
		__SIZE_BLK_PLINK_UHD, i++, get_name_blk_t(EBLK_T_HD_NV21_TVP));
	err_cnt += UCF_INIT(&d_dd->f_blk_t[EBLK_T_LINK_DCT],
		__SIZE_BLK_PLINK_UHD, i++, get_name_blk_t(EBLK_T_LINK_DCT));
	err_cnt += UCF_INIT(&d_dd->f_blk_t[EBLK_T_LINK_DCT_TVP],
		__SIZE_BLK_PLINK_UHD, i++, get_name_blk_t(EBLK_T_LINK_DCT_TVP));
	err_cnt += UCF_INIT(&d_dd->f_blk_t[EBLK_T_S_LINK_AFBC_T_FULL],
				__SIZE_BLK_PLINK_UHD, i++,
				get_name_blk_t(EBLK_T_S_LINK_AFBC_T_FULL));
	if (err_cnt)
		goto fail_dd_fifo;

	/* blk init */
	for (i = 0; i < __NUB_BLK_MAX; i++) {
		hd	= &d_dd->hd_blk[i];
		blk	= &d_dd->d_blk[i];

		hd->code = CODE_DD_BLK;
		hd->hd_tp = HD_T_BLK;
		hd->idx	= i;
		hd->p	= blk;
		blk->hd	= hd;
		atomic_set(&blk->get, 0);
		blk_idle_put(d_dd, blk);
	}

	dim_reg_buf_i();
	dbg_plink1("%s:ok\n", __func__);
	return true;
fail_dd_fifo:
	UCF_RELEASE(&d_dd->f_dis_free);
	UCF_RELEASE(&d_dd->f_blk_idle);
	for (i = 0; i < K_BLK_T_NUB; i++)
		UCF_RELEASE(&d_dd->f_blk_t[i]);

	kfree(d_dd);
	pdata->dd = NULL;
fail_kmalloc_dd:
	return false;
}
