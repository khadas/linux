// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * drivers/amlogic/media/di_multi/di_task.c
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

#include <linux/kthread.h>	/*ary add*/
#include <linux/freezer.h>
#include <linux/semaphore.h>
#include <linux/kfifo.h>
#include <linux/spinlock.h>
#include <uapi/linux/sched/types.h>
#include "deinterlace.h"
#include "di_data_l.h"

#include "di_prc.h"
#include "di_sys.h"
#include "di_task.h"
#include "di_vframe.h"
#include "di_dbg.h"

static void task_wakeup(struct di_task *tsk);

unsigned int di_dbg_task_flg;	/*debug only*/
module_param_named(di_dbg_task_flg, di_dbg_task_flg, uint, 0664);

bool task_send_cmd(unsigned int cmd)
{
	struct di_task *tsk = get_task();
	unsigned int val;

	dbg_reg("%s:cmd[%d]:\n", __func__, cmd);
	if (kfifo_is_full(&tsk->fifo_cmd)) {
		if (kfifo_out(&tsk->fifo_cmd, &val, sizeof(unsigned int))
		    != sizeof(unsigned int)) {
			PR_ERR("%s:can't out\n", __func__);
			return false;
		}

		PR_ERR("%s:lost cmd[%d]\n", __func__, val);
		tsk->err_cmd_cnt++;
		/*return false;*/
	}
	kfifo_in_spinlocked(&tsk->fifo_cmd, &cmd, sizeof(unsigned int),
			    &tsk->lock_cmd);

	task_wakeup(tsk);
	return true;
}

void task_send_ready(unsigned int id)
{
	struct di_task *tsk = get_task();

	task_wakeup(tsk);
	dbg_tsk("trig:r:%u\n", id);
}

static void task_self_trig(void)
{
	int ch;
	struct di_ch_s *pch;
	bool ret = false;
	struct di_hpre_s  *pre = get_hw_pre();

	if (pre->self_trig_mask) {
		//dim_tr_ops.self_trig(DI_BIT28 | pre->self_trig_mask);
		return;
	}
	for (ch = 0; ch < DI_CHANNEL_NUB; ch++) {
		pch = get_chdata(ch);
		if (pch->self_trig_mask) {
			//dim_tr_ops.self_trig(DI_BIT30 | pch->self_trig_mask);
			continue;
		}
		if (pch->self_trig_need) {
			ret = true;
			break;
		}
		//dim_tr_ops.self_trig(DI_BIT29 | pch->self_trig_need);
	}
	if (ret) {
		dbg_tsk("trig:s[0x%x]\n", pch->self_trig_need);
		task_send_ready(8);
	}
}

static bool task_get_cmd(unsigned int *cmd)
{
	struct di_task *tsk = get_task();
	unsigned int val;

	if (kfifo_is_empty(&tsk->fifo_cmd))
		return false;

	if (kfifo_out(&tsk->fifo_cmd, &val, sizeof(unsigned int))
		!= sizeof(unsigned int))
		return false;

	*cmd = val;
	return true;
}

void task_polling_cmd(void)
{
	int i;
	union DI_L_CMD_BITS cmdbyte;

	for (i = 0; i < MAX_KFIFO_L_CMD_NUB; i++) {
		if (!task_get_cmd(&cmdbyte.cmd32))
			break;
		switch (cmdbyte.b.id) {
		case ECMD_PV_LINK_REG:
			if (get_datal()->dvs_prevpp.ops)
				get_datal()->dvs_prevpp.ops->reg(NULL);
			break;
		case ECMD_PV_LINK_UNREG:
			if (get_datal()->dvs_prevpp.ops)
				get_datal()->dvs_prevpp.ops->unreg(NULL);
			break;
		default:
			break;
		}
		//dip_chst_process_reg(cmdbyte.b.ch);
	}
}

bool task_send_cmd2(unsigned int ch, unsigned int cmd)
{
	struct di_task *tsk = get_task();
	unsigned int val;

	dbg_poll("%s:cmd[%d]:\n", __func__, cmd);
	if (kfifo_is_full(&tsk->fifo_cmd2[ch])) {
		if (kfifo_out(&tsk->fifo_cmd2[ch], &val, sizeof(unsigned int))
		    != sizeof(unsigned int)) {
			PR_ERR("%s:can't out\n", __func__);
			return false;
		}

		PR_ERR("%s:lost cmd[%d]\n", __func__, val);
		tsk->err_cmd_cnt++;
		/*return false;*/
	}
	kfifo_in_spinlocked(&tsk->fifo_cmd2[ch], &cmd, sizeof(unsigned int),
			    &tsk->lock_cmd2[ch]);
	task_wakeup(tsk);
	return true;
}

bool task_get_cmd2(unsigned int ch, unsigned int *cmd)
{
	struct di_task *tsk = get_task();
	unsigned int val;

	if (kfifo_is_empty(&tsk->fifo_cmd2[ch]))
		return false;

	if (kfifo_out(&tsk->fifo_cmd2[ch], &val, sizeof(unsigned int))
		!= sizeof(unsigned int))
		return false;

	*cmd = val;
	return true;
}

void task_polling_cmd_keep(unsigned int ch, unsigned int top_sts)
{
	int i;
	union DI_L_CMD_BITS cmdbyte;
//	struct di_mng_s *pbm = get_bufmng();
	struct di_task *tsk = get_task();
//ary 2020-12-09	ulong flags = 0;
	struct di_ch_s *pch;
	struct di_mng_s *pbm;// = get_bufmng();

//	if (pbm->cma_flg_run)
//		return;
	if (top_sts == EDI_TOP_STATE_READY) {
		pch = get_chdata(ch);
		mem_cfg_realloc(pch);
		mem_cfg_pst(pch);//2020-12-17
		//mem_cfg_realloc_wait(pch);
		pbm = get_bufmng();
		if (!atomic_read(&pbm->trig_unreg[ch]))
			sct_polling(pch, 2);
	}
	if (top_sts != EDI_TOP_STATE_IDLE	&&
	    top_sts != EDI_TOP_STATE_READY	&&
	    top_sts != EDI_TOP_STATE_BYPASS)
		return;

//ary 2020-12-09	spin_lock_irqsave(&plist_lock, flags);
	dim_post_re_alloc(ch);
	//dim_post_release(ch);
//ary 2020-12-09	spin_unlock_irqrestore(&plist_lock, flags);

	if (kfifo_is_empty(&tsk->fifo_cmd2[ch]))
		return;

//	cma_st = dip_cma_get_st(ch);
	for (i = 0; i < MAX_KFIFO_L_CMD_NUB; i++) {
		if (!task_get_cmd2(ch, &cmdbyte.cmd32))
			break;
		if (cmdbyte.b.id == ECMD_RL_KEEP)
			dim_post_keep_cmd_proc(cmdbyte.b.ch, cmdbyte.b.p2);
		else
			PR_ERR("%s\n", __func__);
	}
}

static int task_is_exiting(struct di_task *tsk)
{
	if (tsk->exit)
		return 1;

/*	if (afepriv->dvbdev->writers == 1)
 *		if (time_after_eq(jiffies, fepriv->release_jiffies +
 *				  dvb_shutdown_timeout * HZ))
 *			return 1;
 */
	return 0;
}

static int task_should_wakeup(struct di_task *tsk)
{
	if (tsk->wakeup) {
		tsk->wakeup = 0;
		/*dbg only dbg_tsk("wkg[%d]\n", di_dbg_task_flg);*/
		return 1;
	}
	return task_is_exiting(tsk);
}

static void task_wakeup(struct di_task *tsk)
{
	tsk->wakeup = 1;
	wake_up_interruptible(&tsk->wait_queue);
	/*dbg_tsk("wks[%d]\n", di_dbg_task_flg);*/
}

void task_delay(unsigned int val)
{
	struct di_task *tsk = get_task();

	tsk->delay = HZ / val;
}

static int di_test_thread(void *data)
{
	struct di_task *tsk = data;
	bool semheld = false;

	tsk->delay = HZ;
	tsk->status = 0;
	tsk->wakeup = 0;

	set_freezable();
	while (1) {
		up(&tsk->sem);/* is locked when we enter the thread... */
restart:
		wait_event_interruptible_timeout(tsk->wait_queue,
						 task_should_wakeup(tsk) ||
						 kthread_should_stop()	 ||
						 freezing(current),
						 tsk->delay);
		di_dbg_task_flg = 1;

		if (kthread_should_stop() || task_is_exiting(tsk)) {
			/* got signal or quitting */
			if (!down_interruptible(&tsk->sem))
				semheld = true;
			tsk->exit = 1;
			break;
		}

		if (try_to_freeze())
			goto restart;

		if (down_interruptible(&tsk->sem))
			break;
#ifdef MARK_HIS
		if (tsk->reinitialise) {
			/*dvb_frontend_init(fe);*/

			tsk->reinitialise = 0;
		}
#endif
		di_dbg_task_flg = 2;
		task_polling_cmd();
		di_dbg_task_flg = 3;
		dip_chst_process_ch();
#ifdef DIM_PLINK_ENABLE_CREATE
		if (dpvpp_ops()		&&
		    dpvpp_is_allowed()	&&
		    (!dpvpp_is_insert() || dpvpp_is_en_polling()))
			dpvpp_ops()->parser(NULL);
#endif /* DIM_PLINK_ENABLE_CREATE */
		di_dbg_task_flg = 4;
		if (get_reg_flag_all())
			dip_hw_process();

		di_dbg_task_flg = 0;
#ifdef DBG_TEST_CREATE
		dbg_crt_do_polling();
#endif
		dip_out_ch();
		dip_sum_post_ch();
		task_self_trig();
	}

	tsk->thread = NULL;
	if (kthread_should_stop())
		tsk->exit = 1;
	else
		tsk->exit = 0;
	/*mb();*/

	if (semheld)
		up(&tsk->sem);

	task_wakeup(tsk);/*?*/
	return 0;
}

void task_stop(void/*struct di_task *tsk*/)
{
	struct di_task *tsk = get_task();
	int i;

	/*not use cmd*/
	pr_info(".");
	/*--------------------*/
	/*cmd buf*/
	if (tsk->flg_cmd) {
		kfifo_free(&tsk->fifo_cmd);
		tsk->flg_cmd = 0;
	}
	/*tsk->lock_cmd = SPIN_LOCK_UNLOCKED;*/
	spin_lock_init(&tsk->lock_cmd);

	/*cmd2 buf*/
	for (i = 0; i < DI_CHANNEL_NUB; i++) {
		if (tsk->flg_cmd2[i]) {
			kfifo_free(&tsk->fifo_cmd2[i]);
			tsk->flg_cmd2[i] = 0;
		}

		spin_lock_init(&tsk->lock_cmd2[i]);
	}
	tsk->err_cmd_cnt = 0;
	/*--------------------*/

	tsk->exit = 1;
	/*mb();*/

	if (!tsk->thread)
		return;

	kthread_stop(tsk->thread);

	sema_init(&tsk->sem, 1);
	tsk->status = 0;

	/* paranoia check in case a signal arrived */
	if (tsk->thread)
		PR_ERR("warning: thread %p won't exit\n", tsk->thread);
}

int task_start(void)
{
	int ret;
	int flg_err;
	struct di_task *tsk = get_task();
	int i;
	struct task_struct *fe_thread;
	struct sched_param param = { .sched_priority = MAX_RT_PRIO - 1 };

	pr_info(".");
	flg_err = 0;
	/*not use cmd*/
	/*--------------------*/
	/*cmd buf*/
	/*tsk->lock_cmd = SPIN_LOCK_UNLOCKED;*/
	spin_lock_init(&tsk->lock_cmd);
	tsk->err_cmd_cnt = 0;
	ret = kfifo_alloc(&tsk->fifo_cmd,
			  sizeof(unsigned int) * MAX_KFIFO_L_CMD_NUB,
			  GFP_KERNEL);
	if (ret < 0) {
		tsk->flg_cmd = false;
		PR_ERR("%s:can't get kfifo\n", __func__);
		return -1;
	}
	tsk->flg_cmd = true;

	for (i = 0; i < DI_CHANNEL_NUB; i++) {
		spin_lock_init(&tsk->lock_cmd2[i]);
		ret = kfifo_alloc(&tsk->fifo_cmd2[i],
				  sizeof(unsigned int) * MAX_KFIFO_L_CMD_NUB,
				  GFP_KERNEL);
		if (ret < 0) {
			tsk->flg_cmd2[i] = false;
			PR_ERR("%s:can't get kfifo2,ch[%d]\n",
			       __func__, i);
			flg_err++;
			break;
		}

		tsk->flg_cmd2[i] = true;
	}

	if (flg_err) {
		if (tsk->flg_cmd) {
			kfifo_free(&tsk->fifo_cmd);
			tsk->flg_cmd = false;
		}
		for (i = 0; i < DI_CHANNEL_NUB; i++) {
			if (tsk->flg_cmd2[i]) {
				kfifo_free(&tsk->fifo_cmd2[i]);
				tsk->flg_cmd2[i] = false;
			}
		}
		return -1;
	}
	/*--------------------*/
	sema_init(&tsk->sem, 1);
	init_waitqueue_head(&tsk->wait_queue);

	if (tsk->thread) {
		if (!tsk->exit)
			return 0;

		task_stop();
	}

	if (signal_pending(current)) {
		if (tsk->flg_cmd) {
			kfifo_free(&tsk->fifo_cmd);
			tsk->flg_cmd = 0;
		}
		for (i = 0; i < DI_CHANNEL_NUB; i++) {
			if (tsk->flg_cmd2[i]) {
				kfifo_free(&tsk->fifo_cmd2[i]);
				tsk->flg_cmd2[i] = 0;
			}
		}
		return -EINTR;
	}
	if (down_interruptible(&tsk->sem)) {
		if (tsk->flg_cmd) {
			kfifo_free(&tsk->fifo_cmd);
			tsk->flg_cmd = 0;
		}
		for (i = 0; i < DI_CHANNEL_NUB; i++) {
			if (tsk->flg_cmd2[i]) {
				kfifo_free(&tsk->fifo_cmd2[i]);
				tsk->flg_cmd2[i] = 0;
			}
		}
		return -EINTR;
	}

	tsk->status = 0;
	tsk->exit = 0;
	tsk->thread = NULL;
	/*mb();*/

	fe_thread = kthread_run(di_test_thread, tsk, "aml-di-test-0");
	if (IS_ERR(fe_thread)) {
		ret = PTR_ERR(fe_thread);
		PR_ERR(" failed to start kthread (%d)\n", ret);
		up(&tsk->sem);
		tsk->flg_init = 0;
		return ret;
	}

	sched_setscheduler_nocheck(fe_thread, SCHED_FIFO, &param);
	tsk->flg_init = 1;
	tsk->thread = fe_thread;
	return 0;
}

void dbg_task(void)
{
	struct di_task *tsk = get_task();

	tsk->status = 1;
	task_wakeup(tsk);
}

/* add mem serverd task */

static int mtask_is_exiting(struct di_mtask *tsk)
{
	if (tsk->exit)
		return 1;

/*	if (afepriv->dvbdev->writers == 1)
 *		if (time_after_eq(jiffies, fepriv->release_jiffies +
 *				  dvb_shutdown_timeout * HZ))
 *			return 1;
 */
	return 0;
}

static int mtask_should_wakeup(struct di_mtask *tsk)
{
	if (tsk->wakeup) {
		tsk->wakeup = 0;
		/*dbg only dbg_tsk("wkg[%d]\n", di_dbg_task_flg);*/
		return 1;
	}
	return mtask_is_exiting(tsk);
}

static void mtask_wakeup(struct di_mtask *tsk)
{
	tsk->wakeup = 1;
	wake_up_interruptible(&tsk->wait_queue);
	/*dbg_tsk("wks[%d]\n", di_dbg_task_flg);*/
}

bool mtask_send_cmd(unsigned int ch, struct mtsk_cmd_s *cmd)
{
	struct di_mtask *tsk = get_mtask();
	struct dim_fcmd_s *fcmd;
	struct mtsk_cmd_s val;

	fcmd = &tsk->fcmd[ch];
	if (!fcmd->flg) {
		//PR_ERR("%s:no fifo\n", __func__);
		return false;
	}

	//dbg_mem2("%s:cmd[%d]:\n", __func__, cmd);
	cmd->block_mode = 0;
	if (kfifo_is_full(&fcmd->fifo)) {
		if (kfifo_out(&fcmd->fifo, &val, sizeof(unsigned int))
		    != sizeof(struct mtsk_cmd_s)) {
			//PR_ERR("%s:can't out\n", __func__);
			return false;
		}

		PR_ERR("%s:lost cmd[%d]\n", __func__, val.cmd);
		tsk->err_cmd_cnt++;
		/*return false;*/
	}
	if (fcmd->flg_lock & DIM_QUE_LOCK_WR)
		kfifo_in_spinlocked(&fcmd->fifo, cmd, sizeof(struct mtsk_cmd_s),
				    &fcmd->lock_w);
	else
		kfifo_in(&fcmd->fifo, cmd, sizeof(struct mtsk_cmd_s));

	atomic_inc(&fcmd->doing);//fcmd->doing++;
	mtask_wakeup(tsk);
	return true;
}

bool mtask_send_cmd_block(unsigned int ch, struct mtsk_cmd_s *cmd)
{
	struct di_mtask *tsk = get_mtask();
	struct dim_fcmd_s *fcmd;
	struct mtsk_cmd_s val;

	fcmd = &tsk->fcmd[ch];
	if (!fcmd->flg) {
		//PR_ERR("%s:no fifo\n", __func__);
		return false;
	}

	//dbg_mem2("%s:cmd[%d]:\n", __func__, cmd);
	cmd->block_mode = 1;
	if (kfifo_is_full(&fcmd->fifo)) {
		if (kfifo_out(&fcmd->fifo, &val, sizeof(unsigned int))
		    != sizeof(struct mtsk_cmd_s)) {
			//PR_ERR("%s:can't out\n", __func__);
			return false;
		}

		PR_ERR("%s:lost cmd[%d]\n", __func__, val.cmd);
		tsk->err_cmd_cnt++;
		/*return false;*/
	}
	if (fcmd->flg_lock & DIM_QUE_LOCK_WR)
		kfifo_in_spinlocked(&fcmd->fifo, cmd, sizeof(struct mtsk_cmd_s),
				    &fcmd->lock_w);
	else
		kfifo_in(&fcmd->fifo, cmd, sizeof(struct mtsk_cmd_s));

	atomic_inc(&fcmd->doing);//fcmd->doing++;
	mtask_wakeup(tsk);
	return true;
}

bool mtsk_release(unsigned int ch, unsigned int cmd)
{
	struct mtsk_cmd_s blk_cmd;

	blk_cmd.cmd = cmd;
	mtask_send_cmd(ch, &blk_cmd);

	return true;
}

bool mtsk_alloc_block(unsigned int ch, struct mtsk_cmd_s *cmd)
{
	struct dim_fcmd_s *fcmd;
	struct di_mtask *tsk = get_mtask();
	unsigned int cnt;

	mtask_send_cmd(ch, cmd);
	fcmd = &tsk->fcmd[ch];
	cnt = 0;
	while ((atomic_read(&fcmd->doing) > 0) && (cnt < 200)) {
		/*wait 2s for finish*/
		usleep_range(10000, 10001);
		cnt++;
	}
	if (atomic_read(&fcmd->doing) > 0) {
		PR_ERR("%s:can't finish[%d]\n", __func__,
		       atomic_read(&fcmd->doing));
		return false;
	}
	return true;
}

bool mtsk_alloc_block2(unsigned int ch, struct mtsk_cmd_s *cmd)
{
	struct dim_fcmd_s *fcmd;
	struct di_mtask *tsk = get_mtask();
//	unsigned int cnt;
	int timeout = 0;

	mtask_send_cmd_block(ch, cmd);
	fcmd = &tsk->fcmd[ch];

	timeout = wait_for_completion_timeout(&fcmd->alloc_done,
		msecs_to_jiffies(30));
	if (!timeout) {
		PR_WARN("%s:ch[%d]timeout\n", __func__, ch);
		return false;
	}
	return true;
}

bool mtsk_release_block(unsigned int ch, unsigned int cmd)
{
	struct dim_fcmd_s *fcmd;
	struct di_mtask *tsk = get_mtask();
	unsigned int cnt;
	struct mtsk_cmd_s blk_cmd;

	blk_cmd.cmd = cmd;
	mtask_send_cmd(ch, &blk_cmd);
	fcmd = &tsk->fcmd[ch];
	cnt = 0;
	while ((atomic_read(&fcmd->doing) > 0) && (cnt < 200)) {
		/*wait 2s for finish*/
		usleep_range(10000, 10001);
		cnt++;
	}
	if (atomic_read(&fcmd->doing) > 0) {
		PR_ERR("%s:can't finish[%d] fix\n", __func__,
		       atomic_read(&fcmd->doing));
		/*fix*/
		atomic_set(&fcmd->doing, 0);
		return false;
	}
	return true;
}

static bool mtask_get_cmd(unsigned int ch, struct mtsk_cmd_s *cmd)
{
	struct di_mtask *tsk = get_mtask();
	struct dim_fcmd_s *fcmd;
	struct mtsk_cmd_s val;
	unsigned int ret;

	fcmd = &tsk->fcmd[ch];
	if (!fcmd->flg) {
#ifdef PRINT_BASIC
		PR_ERR("%s:no fifo\n", __func__);
#endif
		return false;
	}

	if (kfifo_is_empty(&fcmd->fifo))
		return false;

	if (fcmd->flg_lock & DIM_QUE_LOCK_RD)
		ret = kfifo_out_spinlocked(&fcmd->fifo,
					   &val,
					   sizeof(struct mtsk_cmd_s),
					   &fcmd->lock_r);
	else
		ret = kfifo_out(&fcmd->fifo, &val, sizeof(struct mtsk_cmd_s));
	if (ret	!= sizeof(struct mtsk_cmd_s))
		return false;

	*cmd = val;
	return true;
}

static void mtask_polling_cmd(unsigned int ch)
{
	int i;
	struct di_mtask *tsk = get_mtask();
	struct dim_fcmd_s *fcmd;
//	union DI_L_CMD_BLK_BITS cmdbyte;
	struct mtsk_cmd_s blk_cmd;

	fcmd = &tsk->fcmd[ch];

	if (!fcmd->flg || kfifo_is_empty(&fcmd->fifo))
		return;

	for (i = 0; i < MAX_KFIFO_L_CMD_NUB; i++) {
		if (!mtask_get_cmd(ch, &blk_cmd))
			break;

		blk_polling(ch, &blk_cmd);
		if (blk_cmd.block_mode)
			complete(&fcmd->alloc_done);
	}
}

void mtask_wake_m(void)
{
	struct di_mtask *tsk = get_mtask();

	mtask_wakeup(tsk);
}

//static
void mtask_polling_sct(struct di_ch_s *pch)
{
	//sct_alloc_in_poling(ch);
	if (pch->itf.op_ready_out)
		pch->itf.op_ready_out(pch);
}

static int di_mem_thread(void *data)
{
	struct di_mtask *tsk = data;
	bool semheld = false;
	int i;
	struct di_ch_s *pch;

	tsk->delay = HZ;
	tsk->status = 0;
	tsk->wakeup = 0;

	set_freezable();
	while (1) {
		up(&tsk->sem);/* is locked when we enter the thread... */
mrestart:
		wait_event_interruptible_timeout(tsk->wait_queue,
						 mtask_should_wakeup(tsk) ||
						 kthread_should_stop()	 ||
						 freezing(current),
						 tsk->delay);
		di_dbg_task_flg = 1;

		if (kthread_should_stop() || mtask_is_exiting(tsk)) {
			/* got signal or quitting */
			if (!down_interruptible(&tsk->sem))
				semheld = true;
			tsk->exit = 1;
			break;
		}

		if (try_to_freeze())
			goto mrestart;

		if (down_interruptible(&tsk->sem))
			break;

		/**/
		for (i = 0; i < DI_CHANNEL_NUB; i++) {
			pch = get_chdata(i);
			mtask_polling_cmd(i);
			//blk_polling(i);
			//mtask_polling_sct(pch);
		}
		dim_mng_hf_s_act();
	}

	tsk->thread = NULL;
	if (kthread_should_stop())
		tsk->exit = 1;
	else
		tsk->exit = 0;
	/*mb();*/

	if (semheld)
		up(&tsk->sem);

	mtask_wakeup(tsk);/*?*/
	return 0;
}

static void mtask_alloc(struct di_mtask *tsk)
{
	int i;
	struct dim_fcmd_s *fcmd;
	int ret;

	for (i = 0; i < DI_CHANNEL_NUB; i++) {
		/*ini*/
		fcmd = &tsk->fcmd[i];
		fcmd->flg_lock = 0;

		if (fcmd->flg_lock & DIM_QUE_LOCK_RD)
			spin_lock_init(&fcmd->lock_r);
		if (fcmd->flg_lock & DIM_QUE_LOCK_WR)
			spin_lock_init(&fcmd->lock_w);

		ret = kfifo_alloc(&fcmd->fifo,
				  sizeof(struct mtsk_cmd_s) *
				  MAX_KFIFO_L_CMD_NUB,
				  GFP_KERNEL);
		if (ret < 0) {
			fcmd->flg = false;
			tsk->err_res++;
			PR_ERR("%s:can't get kfifo2,ch[%d]\n",
			       __func__, i);
			break;
		}
		init_completion(&fcmd->alloc_done);
		fcmd->flg = true;
	}
}

static void mtask_release(struct di_mtask *tsk)
{
	int i;
	struct dim_fcmd_s *fcmd;

	for (i = 0; i < DI_CHANNEL_NUB; i++) {
		fcmd = &tsk->fcmd[i];

		if (fcmd->flg) {
			kfifo_free(&fcmd->fifo);
			fcmd->flg = false;
		}
		if (fcmd->flg_lock & DIM_QUE_LOCK_RD)
			spin_lock_init(&fcmd->lock_r);
		if (fcmd->flg_lock & DIM_QUE_LOCK_WR)
			spin_lock_init(&fcmd->lock_w);
	}
	tsk->err_res = 0;
}

void mtask_stop(void/*struct di_task *tsk*/)
{
	struct di_mtask *tsk = get_mtask();

	/*not use cmd*/
	pr_info(".");
	/*--------------------*/
	mtask_release(tsk);
	/*--------------------*/

	tsk->exit = 1;
	/*mb();*/

	if (!tsk->thread)
		return;

	kthread_stop(tsk->thread);

	sema_init(&tsk->sem, 1);
	tsk->status = 0;

	/* paranoia check in case a signal arrived */
	if (tsk->thread)
		PR_ERR("warning: thread %p won't exit\n", tsk->thread);
	PR_INF("%s:finish\n", __func__);
}

int mtask_start(void)
{
	int ret;
	int flg_err;
	struct di_mtask *tsk = get_mtask();
	struct task_struct *fe_thread;
	//struct sched_param param = { .sched_priority = MAX_RT_PRIO - 1 };

	pr_info(".");
	flg_err = 0;
	/*not use cmd*/
	/*--------------------*/
	mtask_alloc(tsk);

	/*--------------------*/
	sema_init(&tsk->sem, 1);
	init_waitqueue_head(&tsk->wait_queue);

	if (tsk->thread) {
		if (!tsk->exit)
			return 0;

		mtask_stop();
	}

	if (signal_pending(current)) {
		mtask_release(tsk);

		return -EINTR;
	}
	if (down_interruptible(&tsk->sem)) {
		mtask_release(tsk);

		return -EINTR;
	}

	tsk->status = 0;
	tsk->exit = 0;
	tsk->thread = NULL;
	/*mb();*/

	fe_thread = kthread_run(di_mem_thread, tsk, "aml-dimem-1");
	if (IS_ERR(fe_thread)) {
		ret = PTR_ERR(fe_thread);
		PR_ERR(" failed to start kthread (%d)\n", ret);
		up(&tsk->sem);
		tsk->flg_init = 0;
		return ret;
	}

	//sched_setscheduler_nocheck(fe_thread, SCHED_FIFO, &param);
	tsk->flg_init = 1;
	tsk->thread = fe_thread;
	return 0;
}

void dbg_mtask(void)
{
	struct di_mtask *tsk = get_mtask();

	tsk->status = 1;
	mtask_wakeup(tsk);
}

void dbg_send_cmd_all(void)
{
	//mtask_send_cmd(0, LCMD_BLK(ECMD_BLK_ALLOC, 2, 100));
}
