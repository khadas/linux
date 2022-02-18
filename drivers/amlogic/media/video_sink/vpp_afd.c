// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

/* Linux Headers */
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/sched.h>

/* Amlogic Headers */
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vout/vinfo.h>

/* Local Headers */
#include "video_priv.h"

static DEFINE_MUTEX(afd_mutex);
static struct VPP_AFD_Func_Ptr *gfunc;
static void *afd_handle[MAX_VD_LAYER];
static atomic_t afd_block_flag = ATOMIC_INIT(0);

bool is_afd_available(u8 id)
{
	if (!gfunc || id >= MAX_VD_LAYER || !afd_handle[id])
		return false;

	if (atomic_read(&afd_block_flag) > 0)
		return false;
	return true;
}

int afd_process(u8 id, struct afd_in_param *in, struct afd_out_param *out)
{
	int ret = 0;

	if (!gfunc || id >= MAX_VD_LAYER || !afd_handle[id])
		return -1;

	if (!in || !out)
		return -2;

	if (atomic_read(&afd_block_flag) > 0)
		return -3;
	ret = gfunc->info_get(afd_handle[id], in, out);
	return ret;
}

int register_afd_function(struct VPP_AFD_Func_Ptr *func, const char *ver)
{
	int ret = -1, i;
	bool err_flag = false;

	atomic_set(&afd_block_flag, 1);
	while (atomic_read(&video_inirq_flag) > 0)
		schedule();
	mutex_lock(&afd_mutex);
	pr_info("%s: gfunc %p, func: %p, ver:%s\n",
		      __func__, gfunc, func, ver);
	if (!gfunc && func) {
		gfunc = func;
		ret = 0;
	}
	if (!gfunc) {
		pr_info("%s: fail to reg gfunc ver:%s\n",
				__func__, ver);
		goto error;
	}
	for (i = 0; i < MAX_VD_LAYER; i++) {
		afd_handle[i] = gfunc->init();
		if (!afd_handle[i]) {
			err_flag = true;
			break;
		}
	}
	if (err_flag) {
		for (i = 0; i < MAX_VD_LAYER; i++) {
			if (afd_handle[i]) {
				gfunc->uninit(afd_handle[i]);
				afd_handle[i] = NULL;
			}
		}
	}
error:
	mutex_unlock(&afd_mutex);
	atomic_set(&afd_block_flag, 0);
	return ret;
}
EXPORT_SYMBOL(register_afd_function);

int unregister_afd_function(struct VPP_AFD_Func_Ptr *func)
{
	int ret = -1, i;

	atomic_set(&afd_block_flag, 1);
	while (atomic_read(&video_inirq_flag) > 0)
		schedule();

	mutex_lock(&afd_mutex);
	pr_info("%s: gfunc %p, func: %p\n",
		      __func__, gfunc, func);
	if (func && func == gfunc) {
		for (i = 0; i < MAX_VD_LAYER; i++) {
			if (afd_handle[i]) {
				gfunc->uninit(afd_handle[i]);
				afd_handle[i] = NULL;
			}
		}
		gfunc = NULL;
		ret = 0;
	}
	mutex_unlock(&afd_mutex);
	atomic_set(&afd_block_flag, 0);
	return ret;
}
EXPORT_SYMBOL(unregister_afd_function);

