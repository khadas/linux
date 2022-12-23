// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
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
#include "tb_task.h"
#include "di_vframe.h"
#include "di_dbg.h"
#include "nr_downscale.h"

#ifdef DIM_TB_DETECT
void tb_polling(unsigned int ch, struct tbtsk_cmd_s *cmd)
{
	struct di_tbtask *tsk = get_tbtask();
	struct dim_fcmd_s *fcmd;
	struct di_ch_s *pch;

	pch = get_chdata(cmd->ch);
	fcmd = &tsk->fcmd[cmd->ch];
	dbg_tb("%s:cmd=0x%x,ch=0x%x,cmdch=0x%x\n", __func__,
			cmd->cmd, ch, cmd->ch);
	if (ch != cmd->ch)
		return;

	switch (cmd->cmd) {
	case ECMD_TB_REG:
	#ifdef DIM_TB_DETECT
		dim_tb_alloc(pch);
		dim_tb_reg_init(&cmd->in_buf_vf, 1, cmd->ch);
		if (dim_tb_buffer_init(cmd->ch) < 0) {
			PR_INF(" alloc tb mem ng.\n");
			dim_tb_t_release(pch);
			return;
		}
	#endif
		break;
	case ECMD_TB_PROC:
	#ifdef DIM_TB_DETECT
		dim_tb_function(&cmd->in_buf_vf, cmd->field_count, cmd->ch);
	#endif
		break;
	case ECMD_TB_RELEASE:
	#ifdef DIM_TB_DETECT
		dim_tb_buffer_uninit(cmd->ch);
		dim_tb_t_release(pch);
	#endif
		break;
	case ECMD_TB_ALGORITHM:
	#ifdef DIM_TB_DETECT
		if (dim_tb_task_process(&cmd->in_buf_vf,
					cmd->field_count,
					cmd->ch))
			dbg_tb("not support tb_task\n");
	#endif
		break;

	default:
		dbg_tb("no command\n");
		break;
	}

	atomic_dec(&fcmd->doing);//fcmd->doing--;

	tbtask_send_ready(0);
}

static bool tbtask_get_cmd(unsigned int ch, struct tbtsk_cmd_s *cmd)
{
	struct di_tbtask *tsk = get_tbtask();
	struct dim_fcmd_s *fcmd;
	struct tbtsk_cmd_s val;
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
					   sizeof(struct tbtsk_cmd_s),
					   &fcmd->lock_r);
	else
		ret = kfifo_out(&fcmd->fifo, &val, sizeof(struct tbtsk_cmd_s));
	if (ret	!= sizeof(struct tbtsk_cmd_s))
		return false;

	*cmd = val;
	return true;
}

static void tbtask_polling_cmd(unsigned int ch)
{
	int i;
	struct di_tbtask *tsk = get_tbtask();
	struct dim_fcmd_s *fcmd;
//	union DI_L_CMD_BLK_BITS cmdbyte;
	struct tbtsk_cmd_s blk_cmd;
	struct di_ch_s *pch;

	pch = get_chdata(ch);
	if ((!IS_IC_SUPPORT(TB)) || !pch->en_tb)
		return;

	fcmd = &tsk->fcmd[ch];

	if (!fcmd->flg || kfifo_is_empty(&fcmd->fifo))
		return;

	for (i = 0; i < MAX_KFIFO_L_CMD_NUB; i++) {
		if (!tbtask_get_cmd(ch, &blk_cmd))
			break;

		tb_polling(ch, &blk_cmd);
		if (blk_cmd.block_mode)
			complete(&fcmd->alloc_done);
	}
}

static int tbtask_is_exiting(struct di_tbtask *tsk)
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

static int tbtask_should_wakeup(struct di_tbtask *tsk)
{
	if (tsk->wakeup) {
		tsk->wakeup = 0;
		/*dbg only dbg_tsk("wkg[%d]\n", di_dbg_task_flg);*/
		dbg_once("%s:\n", __func__);
		return 1;
	}
	return tbtask_is_exiting(tsk);
}

static void tbtask_wakeup(struct di_tbtask *tsk)
{
	tsk->wakeup = 1;
	wake_up_interruptible(&tsk->wait_queue);
}

void tbtask_send_ready(unsigned int id)
{
	struct di_tbtask *tsk = get_tbtask();

	tbtask_wakeup(tsk);
	//dbg_once("%s:id=0x%x\n", __func__, id);
}

bool tbtask_send_cmd(unsigned int ch, struct tbtsk_cmd_s *cmd)
{
	struct di_tbtask *tsk = get_tbtask();
	struct dim_fcmd_s *fcmd;
	struct tbtsk_cmd_s val;

	fcmd = &tsk->fcmd[ch];
	if (!fcmd->flg) {
		//PR_ERR("%s:no fifo\n", __func__);
		return false;
	}

	//PR_INF("%s:cmd[%d]:\n", __func__, cmd->cmd);
	cmd->block_mode = 0;
	if (kfifo_is_full(&fcmd->fifo)) {
		if (kfifo_out(&fcmd->fifo, &val, sizeof(unsigned int))
		    != sizeof(struct tbtsk_cmd_s)) {
			//PR_ERR("%s:can't out\n", __func__);
			return false;
		}

		PR_ERR("%s:lost cmd[%d]\n", __func__, val.cmd);
		tsk->err_cmd_cnt++;
		/*return false;*/
	}
	if (fcmd->flg_lock & DIM_QUE_LOCK_WR)
		kfifo_in_spinlocked(&fcmd->fifo, cmd, sizeof(struct tbtsk_cmd_s),
				    &fcmd->lock_w);
	else
		kfifo_in(&fcmd->fifo, cmd, sizeof(struct tbtsk_cmd_s));

	atomic_inc(&fcmd->doing);//fcmd->doing++;
	tbtask_wakeup(tsk);
	return true;
}

bool tbtask_send_cmd_block(unsigned int ch, struct tbtsk_cmd_s *cmd)
{
	struct di_tbtask *tsk = get_tbtask();
	struct dim_fcmd_s *fcmd;
	struct tbtsk_cmd_s val;

	fcmd = &tsk->fcmd[ch];
	if (!fcmd->flg) {
		//PR_ERR("%s:no fifo\n", __func__);
		return false;
	}

	cmd->block_mode = 1;
	if (kfifo_is_full(&fcmd->fifo)) {
		if (kfifo_out(&fcmd->fifo, &val, sizeof(unsigned int))
		    != sizeof(struct tbtsk_cmd_s)) {
			//PR_ERR("%s:can't out\n", __func__);
			return false;
		}

		PR_ERR("%s:lost cmd[%d]\n", __func__, val.cmd);
		tsk->err_cmd_cnt++;
		/*return false;*/
	}
	if (fcmd->flg_lock & DIM_QUE_LOCK_WR)
		kfifo_in_spinlocked(&fcmd->fifo, cmd, sizeof(struct tbtsk_cmd_s),
				    &fcmd->lock_w);
	else
		kfifo_in(&fcmd->fifo, cmd, sizeof(struct tbtsk_cmd_s));

	atomic_inc(&fcmd->doing);//fcmd->doing++;
	tbtask_wakeup(tsk);
	return true;
}

bool tbtsk_alloc_block(unsigned int ch, struct tbtsk_cmd_s *cmd)
{
	struct dim_fcmd_s *fcmd;
	struct di_tbtask *tsk = get_tbtask();
//	unsigned int cnt;
	int timeout = 0;
	struct di_ch_s *pch;

	pch = get_chdata(ch);

	if ((!IS_IC_SUPPORT(TB)) || !pch->en_tb)
		return false;
	dbg_tb("%s :s =%d\n", __func__, ch);

	tbtask_send_cmd_block(ch, cmd);
	fcmd = &tsk->fcmd[ch];

	timeout = wait_for_completion_timeout(&fcmd->alloc_done,
		msecs_to_jiffies(30));
	if (!timeout) {
		PR_WARN("%s:ch[%d]timeout\n", __func__, ch);
		return false;
	}
	return true;
}

bool tbtsk_release_block(unsigned int ch, unsigned int cmd)
{
	struct dim_fcmd_s *fcmd;
	struct di_tbtask *tsk = get_tbtask();
	unsigned int cnt;
	struct tbtsk_cmd_s blk_cmd;

	blk_cmd.cmd = cmd;
	tbtask_send_cmd(ch, &blk_cmd);
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

static int tb_thread(void *data)
{
	struct di_tbtask *tsk = data;
	bool semheld = false;
	int i;
	//struct di_ch_s *pch;

	tsk->delay = HZ;
	tsk->status = 0;
	tsk->wakeup = 0;

	set_freezable();

	while (1) {
		up(&tsk->sem);/* is locked when we enter the thread... */
mrestart:
		wait_event_interruptible_timeout(tsk->wait_queue,
						 tbtask_should_wakeup(tsk) ||
						 kthread_should_stop()	 ||
						 freezing(current),
						 tsk->delay);

		if (kthread_should_stop() || tbtask_is_exiting(tsk)) {
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
			//pch = get_chdata(i);
			tbtask_polling_cmd(i);
			//blk_polling(i);
			//mtask_polling_sct(pch);
		}
	}

	tsk->thread = NULL;
	if (kthread_should_stop())
		tsk->exit = 1;
	else
		tsk->exit = 0;
	/*mb();*/

	if (semheld)
		up(&tsk->sem);

	tbtask_wakeup(tsk);/*?*/
	return 0;
}

static void tbtask_alloc(struct di_tbtask *tsk)
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
				  sizeof(struct tbtsk_cmd_s) *
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

static void tbtask_release(struct di_tbtask *tsk)
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
#endif

void tbtask_stop(void/*struct di_task *tsk*/)
{
#ifdef DIM_TB_DETECT
	struct di_tbtask *tsk = get_tbtask();

	if (!IS_IC_SUPPORT(TB))
		return;

	/*not use cmd*/
	pr_info(".");
	/*--------------------*/
	tbtask_release(tsk);
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
#endif
}

int tbtask_start(void)
{
#ifdef DIM_TB_DETECT
	int ret;
	int flg_err;
	struct di_tbtask *tsk = get_tbtask();
	struct task_struct *fe_thread;
	//struct sched_param param = { .sched_priority = MAX_RT_PRIO - 1 };

	dbg_tb("%s :s\n", __func__);

	if (!IS_IC_SUPPORT(TB))
		return 0;
	pr_info(".");
	flg_err = 0;
	/*not use cmd*/
	/*--------------------*/
	tbtask_alloc(tsk);

	/*--------------------*/
	sema_init(&tsk->sem, 1);
	init_waitqueue_head(&tsk->wait_queue);

	if (tsk->thread) {
		if (!tsk->exit)
			return 0;

		tbtask_stop();
	}

	if (signal_pending(current)) {
		tbtask_release(tsk);

		return -EINTR;
	}
	if (down_interruptible(&tsk->sem)) {
		tbtask_release(tsk);

		return -EINTR;
	}

	tsk->status = 0;
	tsk->exit = 0;
	tsk->thread = NULL;
	/*mb();*/

	fe_thread = kthread_run(tb_thread, tsk, "aml-tb-1");
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
#endif
	return 0;
}

