/*
 * Copyright (c) 2015-2016, Linaro Limited
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include "tee_drv.h"
#include "optee_private.h"
#include "optee_smc.h"

struct wq_entry {
	struct list_head link;
	struct completion c;
	u32 key;
};

void optee_wait_queue_init(struct optee_wait_queue *priv)
{
	mutex_init(&priv->mu);
	INIT_LIST_HEAD(&priv->db);
}

void optee_wait_queue_exit(struct optee_wait_queue *priv)
{
	mutex_destroy(&priv->mu);
}

static void handle_rpc_func_cmd_get_time(struct optee_msg_arg *arg)
{
	struct timespec64 ts;

	if (arg->num_params != 1)
		goto bad;
	if ((arg->params[0].attr & OPTEE_MSG_ATTR_TYPE_MASK) !=
			OPTEE_MSG_ATTR_TYPE_VALUE_OUTPUT)
		goto bad;

	ktime_get_real_ts64(&ts);
	arg->params[0].u.value.a = ts.tv_sec;
	arg->params[0].u.value.b = ts.tv_nsec;

	arg->ret = TEEC_SUCCESS;
	return;
bad:
	arg->ret = TEEC_ERROR_BAD_PARAMETERS;
}

static struct wq_entry *wq_entry_get(struct optee_wait_queue *wq, u32 key)
{
	struct wq_entry *w;

	mutex_lock(&wq->mu);

	list_for_each_entry(w, &wq->db, link)
		if (w->key == key)
			goto out;

	w = kmalloc(sizeof(*w), GFP_KERNEL);
	if (w) {
		init_completion(&w->c);
		w->key = key;
		list_add_tail(&w->link, &wq->db);
	}
out:
	mutex_unlock(&wq->mu);
	return w;
}

static void wq_sleep(struct optee_wait_queue *wq, u32 key)
{
	struct wq_entry *w = wq_entry_get(wq, key);

	if (w) {
		wait_for_completion(&w->c);
		mutex_lock(&wq->mu);
		list_del(&w->link);
		mutex_unlock(&wq->mu);
		kfree(w);
	}
}

static void wq_wakeup(struct optee_wait_queue *wq, u32 key)
{
	struct wq_entry *w = wq_entry_get(wq, key);

	if (w)
		complete(&w->c);
}

static void handle_rpc_func_cmd_wq(struct optee *optee,
				   struct optee_msg_arg *arg)
{
	if (arg->num_params != 1)
		goto bad;

	if ((arg->params[0].attr & OPTEE_MSG_ATTR_TYPE_MASK) !=
			OPTEE_MSG_ATTR_TYPE_VALUE_INPUT)
		goto bad;

	switch (arg->params[0].u.value.a) {
	case OPTEE_MSG_RPC_WAIT_QUEUE_SLEEP:
		wq_sleep(&optee->wait_queue, arg->params[0].u.value.b);
		break;
	case OPTEE_MSG_RPC_WAIT_QUEUE_WAKEUP:
		wq_wakeup(&optee->wait_queue, arg->params[0].u.value.b);
		break;
	default:
		goto bad;
	}

	arg->ret = TEEC_SUCCESS;
	return;
bad:
	arg->ret = TEEC_ERROR_BAD_PARAMETERS;
}

static void handle_rpc_func_cmd_wait(struct optee_msg_arg *arg)
{
	u32 msec_to_wait;

	if (arg->num_params != 1)
		goto bad;

	if ((arg->params[0].attr & OPTEE_MSG_ATTR_TYPE_MASK) !=
			OPTEE_MSG_ATTR_TYPE_VALUE_INPUT)
		goto bad;

	msec_to_wait = arg->params[0].u.value.a;

	/* set task's state to interruptible sleep */
	set_current_state(TASK_INTERRUPTIBLE);

	/* take a nap */
	msleep(msec_to_wait);

	arg->ret = TEEC_SUCCESS;
	return;
bad:
	arg->ret = TEEC_ERROR_BAD_PARAMETERS;
}

static void handle_rpc_supp_cmd(struct tee_context *ctx,
				struct optee_msg_arg *arg)
{
	struct tee_param *params;

	arg->ret_origin = TEEC_ORIGIN_COMMS;

	params = kmalloc_array(arg->num_params, sizeof(struct tee_param),
			       GFP_KERNEL);
	if (!params) {
		arg->ret = TEEC_ERROR_OUT_OF_MEMORY;
		return;
	}

	if (optee_from_msg_param(params, arg->num_params, arg->params)) {
		arg->ret = TEEC_ERROR_BAD_PARAMETERS;
		goto out;
	}

	arg->ret = optee_supp_thrd_req(ctx, arg->cmd, arg->num_params, params);

	if (optee_to_msg_param(arg->params, arg->num_params, params))
		arg->ret = TEEC_ERROR_BAD_PARAMETERS;
out:
	kfree(params);
}

static struct tee_shm *cmd_alloc_suppl(struct tee_context *ctx, size_t sz)
{
	u32 ret;
	struct tee_param param;
	struct optee *optee = tee_get_drvdata(ctx->teedev);
	struct tee_shm *shm;

	param.attr = TEE_IOCTL_PARAM_ATTR_TYPE_VALUE_INOUT;
	param.u.value.a = OPTEE_MSG_RPC_SHM_TYPE_APPL;
	param.u.value.b = sz;
	param.u.value.c = 0;

	ret = optee_supp_thrd_req(ctx, OPTEE_MSG_RPC_CMD_SHM_ALLOC, 1, &param);
	if (ret)
		return ERR_PTR(-ENOMEM);

	mutex_lock(&optee->supp.mutex);
	/* Increases count as secure world doesn't have a reference */
	shm = tee_shm_get_from_id(optee->supp.ctx, param.u.value.c);
	mutex_unlock(&optee->supp.mutex);
	return shm;
}

static void handle_rpc_func_cmd_shm_alloc(struct tee_context *ctx,
					  struct optee_msg_arg *arg)
{
	phys_addr_t pa;
	struct tee_shm *shm;
	size_t sz;
	size_t n;

	arg->ret_origin = TEEC_ORIGIN_COMMS;

	if (!arg->num_params ||
	    arg->params[0].attr != OPTEE_MSG_ATTR_TYPE_VALUE_INPUT) {
		arg->ret = TEEC_ERROR_BAD_PARAMETERS;
		return;
	}

	for (n = 1; n < arg->num_params; n++) {
		if (arg->params[n].attr != OPTEE_MSG_ATTR_TYPE_NONE) {
			arg->ret = TEEC_ERROR_BAD_PARAMETERS;
			return;
		}
	}

	sz = arg->params[0].u.value.b;
	switch (arg->params[0].u.value.a) {
	case OPTEE_MSG_RPC_SHM_TYPE_APPL:
		shm = cmd_alloc_suppl(ctx, sz);
		break;
	case OPTEE_MSG_RPC_SHM_TYPE_KERNEL:
		shm = tee_shm_alloc(ctx, sz, TEE_SHM_MAPPED);
		break;
	default:
		arg->ret = TEEC_ERROR_BAD_PARAMETERS;
		return;
	}

	if (IS_ERR(shm)) {
		arg->ret = TEEC_ERROR_OUT_OF_MEMORY;
		return;
	}

	if (tee_shm_get_pa(shm, 0, &pa)) {
		arg->ret = TEEC_ERROR_BAD_PARAMETERS;
		goto bad;
	}

	arg->params[0].attr = OPTEE_MSG_ATTR_TYPE_TMEM_OUTPUT;
	arg->params[0].u.tmem.buf_ptr = pa;
	arg->params[0].u.tmem.size = sz;
	arg->params[0].u.tmem.shm_ref = (unsigned long)shm;
	arg->ret = TEEC_SUCCESS;
	return;
bad:
	tee_shm_free(shm);
}

static void cmd_free_suppl(struct tee_context *ctx, struct tee_shm *shm)
{
	struct tee_param param;

	param.attr = TEE_IOCTL_PARAM_ATTR_TYPE_VALUE_INOUT;
	param.u.value.a = OPTEE_MSG_RPC_SHM_TYPE_APPL;
	param.u.value.b = tee_shm_get_id(shm);
	param.u.value.c = 0;

	/*
	 * Match the tee_shm_get_from_id() in cmd_alloc_suppl() as secure
	 * world has released its reference.
	 *
	 * It's better to do this before sending the request to supplicant
	 * as we'd like to let the process doing the initial allocation to
	 * do release the last reference too in order to avoid stacking
	 * many pending fput() on the client process. This could otherwise
	 * happen if secure world does many allocate and free in a single
	 * invoke.
	 */
	tee_shm_put(shm);

	optee_supp_thrd_req(ctx, OPTEE_MSG_RPC_CMD_SHM_FREE, 1, &param);
}

static void handle_rpc_func_cmd_shm_free(struct tee_context *ctx,
					 struct optee_msg_arg *arg)
{
	struct tee_shm *shm;

	arg->ret_origin = TEEC_ORIGIN_COMMS;

	if (arg->num_params != 1 ||
	    arg->params[0].attr != OPTEE_MSG_ATTR_TYPE_VALUE_INPUT) {
		arg->ret = TEEC_ERROR_BAD_PARAMETERS;
		return;
	}

	shm = (struct tee_shm *)(unsigned long)arg->params[0].u.value.b;
	switch (arg->params[0].u.value.a) {
	case OPTEE_MSG_RPC_SHM_TYPE_APPL:
		cmd_free_suppl(ctx, shm);
		break;
	case OPTEE_MSG_RPC_SHM_TYPE_KERNEL:
		tee_shm_free(shm);
		break;
	default:
		arg->ret = TEEC_ERROR_BAD_PARAMETERS;
	}
	arg->ret = TEEC_SUCCESS;
}

#define TEE_SECURE_TIMER_FLAG_ONESHOT   0
#define TEE_SECURE_TIMER_FLAG_PERIOD    1
struct optee_timer_data {
	struct tee_context *ctx;
	uint32_t sess;
	uint32_t handle;
	uint32_t flags;
	uint32_t timeout;
	struct delayed_work work;
	struct list_head list_node;
	uint32_t delay_cancel;
	uint32_t working;
};

static void timer_work_task(struct work_struct *work)
{
	struct tee_ioctl_invoke_arg arg;
	struct tee_param params[4];
	struct optee_timer_data *timer_data = container_of((struct delayed_work *)work,
			struct optee_timer_data, work);
	struct tee_context *ctx = timer_data->ctx;
	struct tee_device *teedev = ctx->teedev;
	struct optee *optee = tee_get_drvdata(teedev);
	struct optee_timer *timer = &optee->timer;
	struct workqueue_struct *wq = timer->wq;
	int ret = 0;

	params[0].attr = TEE_IOCTL_PARAM_ATTR_TYPE_VALUE_INPUT;
	params[1].attr = TEE_IOCTL_PARAM_ATTR_TYPE_NONE;
	params[2].attr = TEE_IOCTL_PARAM_ATTR_TYPE_NONE;
	params[3].attr = TEE_IOCTL_PARAM_ATTR_TYPE_NONE;

	params[0].u.value.a = timer_data->handle;

	arg.session = timer_data->sess;
	arg.num_params = 4;
	arg.func = 0xFFFFFFFE;
	mutex_lock(&timer->mutex);
	timer_data->working = 1;
	mutex_unlock(&timer->mutex);
	ret = optee_invoke_func(ctx, &arg, params);
	if (ret != 0)
		pr_err(KERN_EMERG "%s: invoke cmd failed ret = 0x%x\n", __func__, ret);

	mutex_lock(&timer->mutex);
	if (timer_data->delay_cancel ||
			(!(timer_data->flags & TEE_SECURE_TIMER_FLAG_PERIOD))) {
		list_del(&timer_data->list_node);
		kfree(timer_data);
		mutex_unlock(&timer->mutex);
	} else {
		timer_data->working = 0;
		mutex_unlock(&timer->mutex);
		queue_delayed_work(wq, &timer_data->work,
				msecs_to_jiffies(timer_data->timeout));
	}
}

void optee_timer_init(struct optee_timer *timer)
{
	struct workqueue_struct *wq = NULL;

	mutex_init(&timer->mutex);
	INIT_LIST_HEAD(&timer->timer_list);

	wq = create_workqueue("tee_timer");
	if (!wq)
		return;
	timer->wq = wq;
}

void optee_timer_destroy(struct optee_timer *timer)
{
	struct optee_timer_data *timer_data = NULL;
	struct optee_timer_data *temp = NULL;

	mutex_lock(&timer->mutex);
	list_for_each_entry_safe(timer_data, temp, &timer->timer_list, list_node) {
		if (timer_data != NULL) {
			cancel_delayed_work_sync(&timer_data->work);
			list_del(&timer_data->list_node);
			kfree(timer_data);
		}
	}
	mutex_unlock(&timer->mutex);

	mutex_destroy(&timer->mutex);
	destroy_workqueue(timer->wq);
}

void optee_timer_missed_destroy(struct tee_context *ctx, u32 session)
{
	struct optee_timer_data *timer_data = NULL;
	struct optee_timer_data *temp = NULL;
	struct tee_device *teedev = ctx->teedev;
	struct optee *optee = tee_get_drvdata(teedev);
	struct optee_timer *timer = &optee->timer;

	mutex_lock(&timer->mutex);
	list_for_each_entry_safe(timer_data, temp, &timer->timer_list, list_node) {
		if (timer_data != NULL && timer_data->ctx == ctx
				&& timer_data->sess == session) {
			if (timer_data->working) {
				timer_data->delay_cancel = 1;
				continue;
			}
			cancel_delayed_work_sync(&timer_data->work);
			list_del(&timer_data->list_node);
			kfree(timer_data);
		}
	}
	mutex_unlock(&timer->mutex);
}

static void handle_rpc_func_cmd_timer_create(struct tee_context *ctx,
					 struct optee_msg_arg *arg)
{
	struct optee_timer_data *timer_data;
	struct tee_device *teedev = ctx->teedev;
	struct optee *optee = tee_get_drvdata(teedev);
	struct optee_timer *timer = &optee->timer;
	struct workqueue_struct *wq = timer->wq;

	if (arg->num_params != 2 ||
	    arg->params[0].attr != OPTEE_MSG_ATTR_TYPE_VALUE_INPUT ||
	    arg->params[1].attr != OPTEE_MSG_ATTR_TYPE_VALUE_INPUT) {
		arg->ret = TEEC_ERROR_BAD_PARAMETERS;
		return;
	}

	timer_data = kmalloc(sizeof(struct optee_timer_data), GFP_KERNEL);
	if (!timer_data) {
		arg->ret = TEEC_ERROR_OUT_OF_MEMORY;
		return;
	}

	timer_data->ctx = ctx;
	timer_data->sess = arg->params[0].u.value.a;
	timer_data->handle = arg->params[0].u.value.b;
	timer_data->timeout = arg->params[1].u.value.a;
	timer_data->flags = arg->params[1].u.value.b;
	timer_data->delay_cancel = 0;
	timer_data->working= 0;
	INIT_DELAYED_WORK(&timer_data->work, timer_work_task);

	mutex_lock(&timer->mutex);
	list_add_tail(&timer_data->list_node, &timer->timer_list);
	mutex_unlock(&timer->mutex);

	queue_delayed_work(wq, &timer_data->work, msecs_to_jiffies(timer_data->timeout));

	arg->ret = TEEC_SUCCESS;
}

static void handle_rpc_func_cmd_timer_destroy(struct tee_context *ctx,
					 struct optee_msg_arg *arg)
{
	uint32_t handle;
	struct tee_device *teedev = ctx->teedev;
	struct optee *optee = tee_get_drvdata(teedev);
	struct optee_timer *timer = &optee->timer;
	struct optee_timer_data *timer_data;

	if (arg->num_params != 1 ||
	    arg->params[0].attr != OPTEE_MSG_ATTR_TYPE_VALUE_INPUT) {
		arg->ret = TEEC_ERROR_BAD_PARAMETERS;
		return;
	}

	handle = arg->params[0].u.value.b;

	mutex_lock(&timer->mutex);
	list_for_each_entry(timer_data, &timer->timer_list, list_node) {
		if (timer_data->handle == handle) {
			if (timer_data->working) {
				timer_data->delay_cancel = 1;
				arg->ret = TEEC_SUCCESS;
				goto out;
			}
			cancel_delayed_work_sync(&timer_data->work);
			list_del(&timer_data->list_node);
			kfree(timer_data);

			arg->ret = TEEC_SUCCESS;
			goto out;
		}
	}

	arg->ret = TEEC_ERROR_BAD_PARAMETERS;
out:
	mutex_unlock(&timer->mutex);
}

static void handle_rpc_func_cmd(struct tee_context *ctx, struct optee *optee,
				struct tee_shm *shm)
{
	struct optee_msg_arg *arg;

	arg = tee_shm_get_va(shm, 0);
	if (IS_ERR(arg)) {
		pr_err("%s: tee_shm_get_va %p failed\n", __func__, shm);
		return;
	}

	switch (arg->cmd) {
	case OPTEE_MSG_RPC_CMD_GET_TIME:
		handle_rpc_func_cmd_get_time(arg);
		break;
	case OPTEE_MSG_RPC_CMD_WAIT_QUEUE:
		handle_rpc_func_cmd_wq(optee, arg);
		break;
	case OPTEE_MSG_RPC_CMD_SUSPEND:
		handle_rpc_func_cmd_wait(arg);
		break;
	case OPTEE_MSG_RPC_CMD_SHM_ALLOC:
		handle_rpc_func_cmd_shm_alloc(ctx, arg);
		break;
	case OPTEE_MSG_RPC_CMD_SHM_FREE:
		handle_rpc_func_cmd_shm_free(ctx, arg);
		break;
	case OPTEE_MSG_RPC_CMD_TIMER_CREATE:
		handle_rpc_func_cmd_timer_create(ctx, arg);
		break;
	case OPTEE_MSG_RPC_CMD_TIMER_DESTROY:
		handle_rpc_func_cmd_timer_destroy(ctx, arg);
		break;
	default:
		handle_rpc_supp_cmd(ctx, arg);
	}
}

/**
 * optee_handle_rpc() - handle RPC from secure world
 * @ctx:	context doing the RPC
 * @param:	value of registers for the RPC
 *
 * Result of RPC is written back into @param.
 */
void optee_handle_rpc(struct tee_context *ctx, struct optee_rpc_param *param)
{
	struct tee_device *teedev = ctx->teedev;
	struct optee *optee = tee_get_drvdata(teedev);
	struct tee_shm *shm;
	phys_addr_t pa;

	switch (OPTEE_SMC_RETURN_GET_RPC_FUNC(param->a0)) {
	case OPTEE_SMC_RPC_FUNC_ALLOC:
		shm = tee_shm_alloc(ctx, param->a1, TEE_SHM_MAPPED);
		if (!IS_ERR(shm) && !tee_shm_get_pa(shm, 0, &pa)) {
			reg_pair_from_64(&param->a1, &param->a2, pa);
			reg_pair_from_64(&param->a4, &param->a5,
					 (unsigned long)shm);
		} else {
			param->a1 = 0;
			param->a2 = 0;
			param->a4 = 0;
			param->a5 = 0;
		}
		break;
	case OPTEE_SMC_RPC_FUNC_FREE:
		shm = reg_pair_to_ptr(param->a1, param->a2);
		tee_shm_free(shm);
		break;
	case OPTEE_SMC_RPC_FUNC_IRQ:
		/*
		 * An IRQ was raised while secure world was executing,
		 * since all IRQs are handled in Linux a dummy RPC is
		 * performed to let Linux take the IRQ through the normal
		 * vector.
		 */
		break;
	case OPTEE_SMC_RPC_FUNC_CMD:
		shm = reg_pair_to_ptr(param->a1, param->a2);
		handle_rpc_func_cmd(ctx, optee, shm);
		break;
	default:
		pr_warn("Unknown RPC func 0x%x\n",
			(u32)OPTEE_SMC_RETURN_GET_RPC_FUNC(param->a0));
		break;
	}

	param->a0 = OPTEE_SMC_CALL_RETURN_FROM_RPC;
}
