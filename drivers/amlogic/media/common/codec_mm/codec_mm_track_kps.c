// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/timer.h>
#include <linux/list.h>
#include <linux/string.h>
#include <linux/atomic.h>
#include <linux/hash.h>
#include <linux/hashtable.h>
#include <linux/sched/signal.h>
#include <linux/sched/task.h>
#include <linux/sched/clock.h>
#include <linux/sched/mm.h>
#include <linux/fdtable.h>
#include <linux/delay.h>
#include <linux/kallsyms.h>
#include <linux/dma-buf.h>
#include <linux/ptrace.h>

#include "codec_mm_track_kps.h"

#ifdef CONFIG_HAVE_FUNCTION_ARG_ACCESS_API
#define GET_PARMS(r, i)	regs_get_kernel_argument(r, i)
#else
#define NR_REG_ARGUMENTS 8
#define GET_PARMS(r, i)		\
	({ typeof(r) _r = (r);	\
	typeof(i) _i = (i);	\
	(_i < NR_REG_ARGUMENTS) ? _r->uregs[_i] : 0; })
#endif

struct codec_mm_kps_s {
	struct kprobe	kps[DBUF_TRACE_FUNC_MAX];
	kps_cb		func;
	void		*priv;
};

static struct codec_mm_kps_s *get_kps_ctx(struct kprobe *p, int i)
{
	return container_of(p - i, struct codec_mm_kps_s, kps[0]);
}

static bool is_dma_buf_file(struct file *file)
{
	return file && file->private_data;
}

static int kp_fd_install_pre(struct kprobe *p, struct pt_regs *regs)
{
	struct codec_mm_kps_s *kctx = get_kps_ctx(p, DBUF_TRACE_FUNC_0);
	u32 fd = GET_PARMS(regs, 1);
	struct file *file = (struct file *)GET_PARMS(regs, 2);

	if (!is_dma_buf_file(file))
		goto out;

	kctx->func(kctx->priv, p->symbol_name, 0, DBUF_TRACE_FUNC_0, file, &fd, NULL);
out:
	return 0;
}

static void kp_fd_install_post(struct kprobe *p, struct pt_regs *regs,
		unsigned long flags)
{
	//
}

static int kp_do_dup2_pre(struct kprobe *p, struct pt_regs *regs)
{
	struct codec_mm_kps_s *kctx = get_kps_ctx(p, DBUF_TRACE_FUNC_1);
	struct files_struct *files = (struct files_struct *)GET_PARMS(regs, 0);
	struct file *file = (struct file *)GET_PARMS(regs, 1);
	u32 fd = GET_PARMS(regs, 2);
	struct file *tofree;
	struct fdtable *fdt;

	if (!is_dma_buf_file(file))
		goto out;

	fdt = files_fdtable(files);
	tofree = fdt->fd[fd];
	if (!tofree && fd_is_open(fd, fdt))
		goto out;

	kctx->func(kctx->priv, p->symbol_name, 0, DBUF_TRACE_FUNC_1, file, &fd, NULL);
out:
	return 0;
}

static void kp_do_dup2_post(struct kprobe *p, struct pt_regs *regs,
		unsigned long flags)
{
	//
}

static int kp_dbuf_file_release_pre(struct kprobe *p, struct pt_regs *regs)
{
	struct codec_mm_kps_s *kctx = get_kps_ctx(p, DBUF_TRACE_FUNC_2);
	//struct inode *inode = (struct inode *)GET_PARMS(regs, 0);
	struct file *file = (struct file *)GET_PARMS(regs, 1);

	if (!is_dma_buf_file(file))
		goto out;

	kctx->func(kctx->priv, p->symbol_name, 0, DBUF_TRACE_FUNC_2, file, NULL, NULL);
out:
	return 0;
}

static void kp_dbuf_file_release_post(struct kprobe *p, struct pt_regs *regs,
		unsigned long flags)
{
	//
}

static int kp_put_unused_fd_pre(struct kprobe *p, struct pt_regs *regs)
{
	struct codec_mm_kps_s *kctx = get_kps_ctx(p, DBUF_TRACE_FUNC_3);
	u32 fd = GET_PARMS(regs, 0);
	struct file *file = fget(fd);

	if (!file || !is_dma_buf_file(file))
		goto out;

	kctx->func(kctx->priv, p->symbol_name, 0, DBUF_TRACE_FUNC_3, file, &fd, NULL);
out:
	if (file)
		fput(file);

	return 0;
}

static void kp_put_unused_fd_post(struct kprobe *p, struct pt_regs *regs,
		unsigned long flags)
{
	//
}

static struct kprobe g_kps[] = {
#include "codec_mm_kps_config.h"
};

void codec_mm_unregister_kprobes(struct kprobe **kps, int num)
{
	int i;

	if (num <= 0)
		return;

	for (i = 0; i < num; i++)
		unregister_kprobe(kps[i]);
}

int codec_mm_register_kprobes(struct kprobe **kps, int num)
{
	int i, ret = 0;

	if (num <= 0)
		return -EINVAL;

	for (i = 0; i < num; i++) {
		ret = register_kprobe(kps[i]);
		if (ret < 0) {
			if (i > 0)
				codec_mm_unregister_kprobes(kps, i);
			break;
		}
	}

	return ret;
}

int codec_mm_reg_kprobes(void **kps_h, void *priv, kps_cb cb)
{
	struct codec_mm_kps_s *kctx;
	int ret;

	KPS_DEF();

	kctx = vzalloc(sizeof(*kctx));
	if (!kctx)
		return -ENOMEM;

	KPS_INIT(kctx);
	kctx->func	= cb;
	kctx->priv	= priv;
	memcpy(kctx->kps, g_kps, sizeof(g_kps));

	ret = codec_mm_register_kprobes(KPS_GET(), KPS_SIZE());
	if (ret < 0) {
		pr_err("%s fail, ret:%d\n", __func__, ret);
		return ret;
	}

	*kps_h = kctx;

	return 0;
}

void codec_mm_unreg_kprobes(void *kps_h)
{
	struct codec_mm_kps_s *kctx = kps_h;

	KPS_DEF();

	KPS_INIT(kctx);

	codec_mm_unregister_kprobes(KPS_GET(), KPS_SIZE());
}

