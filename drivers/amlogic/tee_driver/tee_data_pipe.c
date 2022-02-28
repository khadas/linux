/*
 * Copyright (C) 2014-2017 Amlogic, Inc. All rights reserved.
 *
 * All information contained herein is Amlogic confidential.
 *
 * This software is provided to you pursuant to Software License Agreement
 * (SLA) with Amlogic Inc ("Amlogic"). This software may be used
 * only in accordance with the terms of this agreement.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification is strictly prohibited without prior written permission from
 * Amlogic.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <tee_data_pipe.h>

#define DATA_PIPE_DEBUG                   (0)
#define PIPE_INFO_DEBUG                   (0)

#define SIZE_1K                           (1024)
#define SIZE_1M                           (SIZE_1K * SIZE_1K)

#define MAX_NUM_DATA_PIPE                 (64)

#define MAX_SIZE_ALL_CACHE                (8 * SIZE_1M)
#define MAX_SIZE_ALL_PERSISTENCE_CACHE    (2 * SIZE_1M)

#define MAX_SIZE_PERSISTENCE_PIPE_CACHE   (128 * SIZE_1K)
#define MIN_SIZE_PERSISTENCE_PIPE_CACHE   (32 * SIZE_1K)

#define MODE_BLOCKING                     (0)
#define MODE_NOT_BLOCKING                 (1)

#define STATUS_CLOSED                     (0)
#define STATUS_OPENING                    (1)
#define STATUS_OPENED                     (2)

#define PIPE_TYPE_MASTER                  (0)
#define PIPE_TYPE_SLAVE                   (1)

#define TEEC_SUCCESS                      (0x00000000)
#define TEE_ERROR_ACCESS_DENIED           (0xFFFF0001)
#define TEEC_ERROR_BAD_PARAMETERS         (0xFFFF0006)
#define TEEC_ERROR_OUT_OF_MEMORY          (0xFFFF000C)
#define TEEC_ERROR_COMMUNICATION          (0xFFFF000E)
#define TEEC_ERROR_SECURITY               (0xFFFF000F)

#define LOG_TAG                           "[Data Pipe] "

#define ERROR(arg, ...) \
	pr_err(LOG_TAG "[Error] [Function: %s, Line: %d] " arg, __func__, __LINE__, ##__VA_ARGS__)

#define INFO(arg, ...)                    pr_info(LOG_TAG arg)

#if DATA_PIPE_DEBUG
#define DEBUG(arg, ...) \
	pr_info(LOG_TAG "[Debug] [Function: %s, Line: %d] " arg, __func__, __LINE__, ##__VA_ARGS__)
#else
#define DEBUG(arg, ...)
#endif

#define MIN(a, b)                         ((a) < (b) ? (a) : (b))

/*
 * when pipe.is_persistent = true, the pipe cache will not be free and will be reused.
 * pipe.status may be STATUS_CLOSED or STATUS_OPENING or STATUS_OPENED.
 * pipe,mode may be MODE_BLOCKING or MODE_NOT_BLOCKING.
 */
typedef struct {
	uint32_t cache_size;
	uint32_t data_size;
	char *data_start;
	char *data_end;
	char *cache;
} cyclic_cache_t;

typedef struct {
	bool is_persistent;
	uint32_t status;
	uint32_t id;
	uint32_t mode;
	cyclic_cache_t cache_to_svr;
	cyclic_cache_t cache_from_svr;
	struct mutex pipe_lock;
} data_pipe_t;

static data_pipe_t g_data_pipe_set[MAX_NUM_DATA_PIPE];
static struct mutex g_pipe_set_lock;

static uint32_t g_cur_all_cache_size = 0;
static uint32_t g_cur_persistence_cache_size = 0;
static uint32_t g_cur_max_pipe_id = 0;
static uint32_t g_backlog = 0; // set the max pipe count from server
static uint32_t g_cur_pipe_cnt = 0; // current pipe count
static struct mutex g_pipe_cnt_lock;

static wait_queue_head_t g_wait_queue_for_accept;
static wait_queue_head_t g_wait_queue_for_open;
static wait_queue_head_t g_wait_queue_for_data;

static bool g_stop_accept = false;

static void* cache_malloc(uint32_t size)
{
	if (size <= MAX_SIZE_PERSISTENCE_PIPE_CACHE)
		return kmalloc(size, GFP_KERNEL);
	else
		return vmalloc(size);
}

static void cache_free(void *cache, uint32_t size)
{
	if (size <= MAX_SIZE_PERSISTENCE_PIPE_CACHE)
		kfree(cache);
	else
		vfree(cache);
}

static bool is_cache_full(uint32_t status, const cyclic_cache_t *cache)
{
	return status == STATUS_OPENED && cache->data_size == cache->cache_size;
}

static bool is_cache_empty(uint32_t status, const cyclic_cache_t *cache)
{
	return status == STATUS_OPENED && cache->data_size == 0;
}

static void dump_pipe_info(const data_pipe_t *pipe)
{
	(void)pipe;

#if PIPE_INFO_DEBUG
	DEBUG("pipe->id = %d\n", pipe->id);
	DEBUG("pipe->status = %d\n", pipe->status);
	DEBUG("pipe->mode = %d\n", pipe->mode);
	DEBUG("pipe->is_persistent = %d\n", (int)pipe->is_persistent);

	DEBUG("pipe->cache_to_svr.cache_size = 0x%08X\n", pipe->cache_to_svr.cache_size);
	DEBUG("pipe->cache_to_svr.data_size = %d\n", pipe->cache_to_svr.data_size);
	DEBUG("pipe->cache_to_svr.data_start = 0x%016lx\n", (long)pipe->cache_to_svr.data_start);
	DEBUG("pipe->cache_to_svr.data_end = 0x%016lx\n", (long)pipe->cache_to_svr.data_end);
	DEBUG("pipe->cache_to_svr.cache = 0x%016lx\n", (long)pipe->cache_to_svr.cache);

	DEBUG("pipe->cache_from_svr.cache_size = 0x%08X\n", pipe->cache_from_svr.cache_size);
	DEBUG("pipe->cache_from_svr.data_size = %d\n", pipe->cache_from_svr.data_size);
	DEBUG("pipe->cache_from_svr.data_start = 0x%016lx\n", (long)pipe->cache_from_svr.data_start);
	DEBUG("pipe->cache_from_svr.data_end = 0x%016lx\n", (long)pipe->cache_from_svr.data_end);
	DEBUG("pipe->cache_from_svr.cache = 0x%016lx\n", (long)pipe->cache_from_svr.cache);
#endif
}

static data_pipe_t* get_opening_pipe(void)
{
	uint32_t i = 0;

	for (i = 0; i < g_backlog; i++) {
		if (g_data_pipe_set[i].status == STATUS_OPENING) {
			return &g_data_pipe_set[i];
		}
	}

	return NULL;
}

static data_pipe_t* get_pipe_by_id(uint32_t id)
{
	data_pipe_t *pipe = NULL;
	uint32_t i = 0;

	for (i = 0; i < g_backlog; i++) {
		if (id == g_data_pipe_set[i].id) {
			pipe = &g_data_pipe_set[i];
			break;
		}
	}

	return pipe;
}

static data_pipe_t* get_adapted_pipe(uint32_t exp_cache_size, uint32_t mode)
{
	data_pipe_t *adapted_pipe = NULL;
	data_pipe_t *pipe_ptr = NULL;
	uint32_t i = 0;

	mutex_lock(&g_pipe_set_lock);

	/* find adapted persistent data pipe */
	for (i = 0; i < g_backlog; i++) {
		pipe_ptr = &g_data_pipe_set[i];
		if (pipe_ptr->status != STATUS_CLOSED
				|| !pipe_ptr->is_persistent
				|| pipe_ptr->cache_to_svr.cache_size < exp_cache_size
				|| pipe_ptr->cache_from_svr.cache_size < exp_cache_size)
			continue;

		if (!adapted_pipe) {
			adapted_pipe = &g_data_pipe_set[i]; // find the first adapted data pipe
			continue;
		}

		if ((pipe_ptr->cache_to_svr.cache_size < adapted_pipe->cache_to_svr.cache_size)
			&& (pipe_ptr->cache_from_svr.cache_size < adapted_pipe->cache_from_svr.cache_size))
			adapted_pipe = pipe_ptr; // the cur pipe is more adapted then pre one
	}

	if (!adapted_pipe) {
		/* find adapted not persistent data pipe */
		for (i = 0; i < g_backlog; i++) {
			pipe_ptr = &g_data_pipe_set[i];
			if (pipe_ptr->status == STATUS_CLOSED) {
				adapted_pipe = pipe_ptr;
				break;
			}
		}
	}

	if (!adapted_pipe) {
		ERROR("there are too many opened data pipes\n");
		goto exit;
	}

	/* initialize data pipe */
	if (!adapted_pipe->is_persistent) {
		if (g_cur_all_cache_size + exp_cache_size * 2 > MAX_SIZE_ALL_CACHE) {
			adapted_pipe = NULL;
			ERROR("the allocated pipe cache exceeds the limit\n");
			goto exit;
		}

		adapted_pipe->cache_to_svr.cache = cache_malloc(exp_cache_size);
		if (!adapted_pipe->cache_to_svr.cache) {
			adapted_pipe = NULL;
			ERROR("cache_malloc failed\n");
			goto exit;
		}

		adapted_pipe->cache_from_svr.cache = cache_malloc(exp_cache_size);
		if (!adapted_pipe->cache_from_svr.cache) {
			adapted_pipe = NULL;
			ERROR("cache_malloc failed\n");
			goto exit;
		}

		adapted_pipe->cache_to_svr.cache_size = exp_cache_size;
		adapted_pipe->cache_from_svr.cache_size = exp_cache_size;
		g_cur_all_cache_size += exp_cache_size * 2;
	}
	adapted_pipe->id = g_cur_max_pipe_id + 1;
	g_cur_max_pipe_id++;
	adapted_pipe->mode = mode;
	adapted_pipe->status = STATUS_OPENING;

exit:
	mutex_unlock(&g_pipe_set_lock);

	return adapted_pipe;
}

#define ONCE_COPY_FROM_USER(to, from, size) \
	if (size && copy_from_user(to, from, size)) { \
		mutex_unlock(&pipe->pipe_lock); \
		ERROR("copy_from_user failed\n"); \
		return TEEC_ERROR_SECURITY; \
	}

static uint32_t write_cache(data_pipe_t *pipe, cyclic_cache_t *cache,
		void __user *to_write_data, uint32_t *to_write_size)
{
	uint32_t once_write_size = 0;
	uint32_t finished_size = 0;
	uint32_t empty_size = 0;
	char *cache_end = NULL;
	uint32_t tmp_id = pipe->id;
	int wait_status = 0;

	while (pipe->status == STATUS_OPENED && *to_write_size > 0 && pipe->id == tmp_id && wait_status == 0) {
		mutex_lock(&pipe->pipe_lock);
		if (pipe->status == STATUS_OPENED && !is_cache_full(pipe->status, cache) && pipe->id == tmp_id) {
			if (is_cache_empty(pipe->status, cache)) {
				once_write_size = MIN(*to_write_size, cache->cache_size);
				ONCE_COPY_FROM_USER(cache->cache, to_write_data, once_write_size)
				cache->data_start = cache->cache;
				cache->data_end = cache->cache + once_write_size - 1;
				DEBUG("cache->cache = 0x%016lx, cache->data_start = 0x%016lx, cache->data_end = 0x%016lx, once_write_size = %d\n",
					(long)cache->cache, (long)cache->data_start, (long)cache->data_end, once_write_size);
			} else if (cache->data_start <= cache->data_end) {
				cache_end = cache->cache + cache->cache_size - 1;
				if (cache->data_end != cache_end) {
					empty_size = cache_end - cache->data_end;
					once_write_size = MIN(*to_write_size, empty_size);
					ONCE_COPY_FROM_USER(cache->data_end + 1, to_write_data, once_write_size)
					cache->data_end += once_write_size;
					DEBUG("cache->cache = 0x%016lx, cache->data_start = 0x%016lx, cache->data_end = 0x%016lx, once_write_size = %d, empty_size = %d\n",
						(long)cache->cache, (long)cache->data_start, (long)cache->data_end, once_write_size, empty_size);
				} else {
					empty_size = cache->data_start - cache->cache;
					once_write_size = MIN(*to_write_size, empty_size);
					ONCE_COPY_FROM_USER(cache->cache, to_write_data, once_write_size)
					cache->data_end = cache->cache + once_write_size - 1;
					DEBUG("cache->cache = 0x%016lx, cache->data_start = 0x%016lx, cache->data_end = 0x%016lx, once_write_size = %d, empty_size = %d\n",
						(long)cache->cache, (long)cache->data_start, (long)cache->data_end, once_write_size, empty_size);
				}
			} else if (cache->data_start > cache->data_end) {
				once_write_size = MIN(*to_write_size, cache->data_size);
				ONCE_COPY_FROM_USER(cache->data_end + 1, to_write_data, once_write_size)
				cache->data_end += once_write_size;
				DEBUG("cache->cache = 0x%016lx, cache->data_start = 0x%016lx, cache->data_end = 0x%016lx, once_write_size = %d\n",
					(long)cache->cache, (long)cache->data_start, (long)cache->data_end, once_write_size);
			}

			cache->data_size += once_write_size;
			DEBUG("cache->data_size = %d\n", cache->data_size);
			finished_size += once_write_size;
			if (once_write_size < *to_write_size)
				*to_write_size -= once_write_size;
			else
				*to_write_size = 0;
			to_write_data += once_write_size;
		}
		mutex_unlock(&pipe->pipe_lock);
		wake_up_interruptible(&g_wait_queue_for_data);

		if (*to_write_size == 0)
			break;

		if (pipe->mode == MODE_BLOCKING) {
			DEBUG("start wait_event_interruptible in function write_cache\n");
			wait_status = wait_event_interruptible(g_wait_queue_for_data, !is_cache_full(pipe->status, cache));
			DEBUG("end wait_event_interruptible in function write_cache, wait_status = %d\n", wait_status);
		} else {
			if (is_cache_full(pipe->status, cache))
				break;
		}
	}

	*to_write_size = finished_size;

	return TEEC_SUCCESS;
}

#define ONCE_COPY_TO_USER(to, from, size) \
	if (size && copy_to_user(to, from, size)) { \
		mutex_unlock(&pipe->pipe_lock); \
		ERROR("copy_to_user failed\n"); \
		return TEEC_ERROR_SECURITY; \
	}

static uint32_t read_cache(data_pipe_t *pipe, cyclic_cache_t *cache,
		void __user *data_buf, uint32_t *buf_size)
{
	uint32_t once_read_size = 0;
	uint32_t finished_size = 0;
	uint32_t readable_size = 0;
	char *cache_end = NULL;
	uint32_t tmp_id = pipe->id;
	int wait_status = 0;

	while (pipe->status == STATUS_OPENED && *buf_size > 0 && pipe->id == tmp_id && wait_status == 0) {
		mutex_lock(&pipe->pipe_lock);
		if (pipe->status == STATUS_OPENED && !is_cache_empty(pipe->status, cache) && pipe->id == tmp_id) {
			if (cache->data_start > cache->data_end) {
				cache_end = cache->cache + cache->cache_size - 1;
				readable_size = cache_end - cache->data_start + 1;
				once_read_size = MIN(*buf_size, readable_size);
				ONCE_COPY_TO_USER(data_buf, cache->data_start, once_read_size)
				if (readable_size == once_read_size)
					cache->data_start = cache->cache;
				else
					cache->data_start += once_read_size;
				DEBUG("cache->cache = 0x%016lx, cache->data_start = 0x%016lx, cache->data_end = 0x%016lx, once_read_size = %d, readable_size = %d\n",
					(long)cache->cache, (long)cache->data_start, (long)cache->data_end, once_read_size, readable_size);
			} else {
				once_read_size = MIN(*buf_size, cache->data_size);
				ONCE_COPY_TO_USER(data_buf, cache->data_start, once_read_size)
				cache->data_start += once_read_size;
				DEBUG("cache->cache = 0x%016lx, cache->data_start = 0x%016lx, cache->data_end = 0x%016lx, once_read_size = %d\n",
					(long)cache->cache, (long)cache->data_start, (long)cache->data_end, once_read_size);
			}

			cache->data_size -= once_read_size;
			DEBUG("cache->data_size = %d\n", cache->data_size);
			finished_size += once_read_size;
			if (once_read_size < *buf_size)
				*buf_size -= once_read_size;
			else
				*buf_size = 0;
			data_buf += once_read_size;
			if (is_cache_empty(pipe->status, cache)) {
				cache->data_start = NULL;
				cache->data_end = NULL;
			}
		}
		mutex_unlock(&pipe->pipe_lock);
		wake_up_interruptible(&g_wait_queue_for_data);

		if (*buf_size == 0)
			break;

		if (pipe->mode == MODE_BLOCKING) {
			DEBUG("start wait_event_interruptible in function read_cache\n");
			wait_status = wait_event_interruptible(g_wait_queue_for_data, !is_cache_empty(pipe->status, cache));
			DEBUG("end wait_event_interruptible in function read_cache, wait_status = %d\n", wait_status);
		} else {
			if (is_cache_empty(pipe->status, cache))
				break;
		}
	}

	*buf_size = finished_size;

	return TEEC_SUCCESS;
}

static uint32_t close_pipe_by_id(uint32_t pipe_id)
{
	data_pipe_t *pipe_ptr = NULL;
	cyclic_cache_t *cache_to_svr = NULL;
	cyclic_cache_t *cache_from_svr = NULL;

	pipe_ptr = get_pipe_by_id(pipe_id);

	if (!pipe_ptr) {
		INFO("data pipe had been closed\n");
		return TEEC_SUCCESS;
	}

	mutex_lock(&pipe_ptr->pipe_lock);

	if (pipe_ptr->status == STATUS_CLOSED) {
		INFO("data pipe had been closed\n");
		goto exit;
	}

	if (pipe_ptr->status == STATUS_OPENED) {
		mutex_lock(&g_pipe_cnt_lock);
		g_cur_pipe_cnt--;
		mutex_unlock(&g_pipe_cnt_lock);
	}

	pipe_ptr->status = STATUS_CLOSED;
	pipe_ptr->mode = 0;

	cache_to_svr = &pipe_ptr->cache_to_svr;
	cache_to_svr->data_size = 0;
	cache_to_svr->data_start = NULL;
	cache_to_svr->data_end = NULL;

	cache_from_svr = &pipe_ptr->cache_from_svr;
	cache_from_svr->data_size = 0;
	cache_from_svr->data_start = NULL;
	cache_from_svr->data_end = NULL;

	pipe_ptr->id = 0;

	if (pipe_ptr->is_persistent)
		goto exit;

	if (cache_to_svr->cache_size >= MIN_SIZE_PERSISTENCE_PIPE_CACHE
			&& cache_to_svr->cache_size <= MAX_SIZE_PERSISTENCE_PIPE_CACHE
			&& cache_from_svr->cache_size >= MIN_SIZE_PERSISTENCE_PIPE_CACHE
			&& cache_from_svr->cache_size <= MAX_SIZE_PERSISTENCE_PIPE_CACHE
			&& cache_to_svr->cache_size + cache_from_svr->cache_size
			+ g_cur_persistence_cache_size <= MAX_SIZE_ALL_PERSISTENCE_CACHE) {
		pipe_ptr->is_persistent = true;
		g_cur_persistence_cache_size += (cache_to_svr->cache_size + cache_from_svr->cache_size);
	} else {
		g_cur_all_cache_size -= (cache_to_svr->cache_size + cache_from_svr->cache_size);
		cache_free(cache_to_svr->cache, cache_to_svr->cache_size);
		cache_to_svr->cache_size = 0;
		cache_to_svr->cache = NULL;
		cache_free(cache_from_svr->cache, cache_from_svr->cache_size);
		cache_from_svr->cache_size = 0;
		cache_from_svr->cache = NULL;
	}

exit:
	mutex_unlock(&pipe_ptr->pipe_lock);

	// wake up the blocking read or write action
	wake_up_interruptible(&g_wait_queue_for_data);

	return TEEC_SUCCESS;
}

void init_data_pipe_set(void)
{
	uint32_t i = 0;

	DEBUG("enter function %s\n", __func__);

	memset(g_data_pipe_set, 0, sizeof(g_data_pipe_set));
	for (i = 0; i < MAX_NUM_DATA_PIPE; i++) {
		g_data_pipe_set[i].status = STATUS_CLOSED;
		g_data_pipe_set[i].is_persistent = false;
		mutex_init(&g_data_pipe_set[i].pipe_lock);
	}

	mutex_init(&g_pipe_set_lock);
	mutex_init(&g_pipe_cnt_lock);
	init_waitqueue_head(&g_wait_queue_for_accept);
	init_waitqueue_head(&g_wait_queue_for_open);
	init_waitqueue_head(&g_wait_queue_for_data);

	DEBUG("leave function %s\n", __func__);
}

void destroy_data_pipe_set(void)
{
	uint32_t i = 0;
	cyclic_cache_t *cache = NULL;

	DEBUG("enter function %s\n", __func__);

	g_stop_accept = true;
	wake_up_interruptible(&g_wait_queue_for_accept);

	for (i = 0; i < g_backlog; i++) {
		close_pipe_by_id(g_data_pipe_set[i].id);

		cache = &g_data_pipe_set[i].cache_to_svr;
		if (cache->cache_size > 0) {
			cache_free(cache->cache, cache->cache_size);
			cache->cache_size = 0;
			cache->cache = NULL;
		}

		cache = &g_data_pipe_set[i].cache_from_svr;
		if (cache->cache_size > 0) {
			cache_free(cache->cache, cache->cache_size);
			cache->cache_size = 0;
			cache->cache = NULL;
		}
	}

	DEBUG("leave function %s\n", __func__);
}

uint32_t tee_ioctl_open_data_pipe(struct tee_context *tee_ctx,
		struct tee_iocl_data_pipe_context __user *user_pipe_ctx)
{
	struct tee_iocl_data_pipe_context pipe_ctx;
	data_pipe_t *pipe_ptr = NULL;

	DEBUG("enter function %s\n", __func__);

	if (g_backlog == 0) {
		ERROR("data pipe server isn't running\n");
		return TEE_ERROR_ACCESS_DENIED;
	}

	if (g_cur_pipe_cnt >= g_backlog) {
		ERROR("there are too many opened data pipes, max pipe count: %d, current pipe count: %d\n",
				g_backlog, g_cur_pipe_cnt);
		return TEE_ERROR_ACCESS_DENIED;
	}

	if (copy_from_user(&pipe_ctx, user_pipe_ctx, sizeof(pipe_ctx))) {
		ERROR("copy_from_user failed\n");
		return TEEC_ERROR_SECURITY;
	}

	pipe_ptr = get_adapted_pipe(pipe_ctx.cache_size, pipe_ctx.mode);
	if (pipe_ptr) {
		dump_pipe_info(pipe_ptr);

		// notify accept thread that there is a new opening pipe
		wake_up_interruptible(&g_wait_queue_for_accept);

		// wait to set the status for STATUS_OPENED or STATUS_CLOSED
		DEBUG("start block in function open_data_pipe, status = %d\n", pipe_ptr->status);
		wait_event_interruptible_timeout(g_wait_queue_for_open, pipe_ptr->status != STATUS_OPENING, jiffies + HZ);
		DEBUG("end block in function open_data_pipe, status = %d\n", pipe_ptr->status);

		if (pipe_ptr->status == STATUS_CLOSED) {
			ERROR("connect the data pipe server failed\n");
			return TEE_ERROR_ACCESS_DENIED;
		}

		if (pipe_ptr->status == STATUS_OPENING) {
			close_pipe_by_id(pipe_ptr->id);
			ERROR("the data pipe server is dead\n");
			return TEEC_ERROR_COMMUNICATION;
		}

		pipe_ctx.id = pipe_ptr->id;
		if (copy_to_user(user_pipe_ctx, &pipe_ctx, sizeof(pipe_ctx))) {
			ERROR("copy_to_user failed\n");
			return TEEC_ERROR_SECURITY;
		}

		mutex_lock(&g_pipe_cnt_lock);
		g_cur_pipe_cnt++;
		mutex_unlock(&g_pipe_cnt_lock);

		DEBUG("leave function %s\n", __func__);

		return TEEC_SUCCESS;
	} else
		return TEEC_ERROR_OUT_OF_MEMORY;
}

uint32_t tee_ioctl_close_data_pipe(struct tee_context *tee_ctx,
		struct tee_iocl_data_pipe_context __user *user_pipe_ctx)
{
	uint32_t res = TEEC_SUCCESS;
	struct tee_iocl_data_pipe_context pipe_ctx;

	DEBUG("enter function %s\n", __func__);

	if (copy_from_user(&pipe_ctx, user_pipe_ctx, sizeof(pipe_ctx))) {
		ERROR("copy_from_user failed\n");
		return TEEC_ERROR_SECURITY;
	}

	res = close_pipe_by_id(pipe_ctx.id);

	DEBUG("leave function %s\n", __func__);

	return res;
}

uint32_t tee_ioctl_write_pipe_data(struct tee_context *tee_ctx,
		struct tee_iocl_data_pipe_context __user *user_pipe_ctx)
{
	uint32_t res = TEEC_SUCCESS;
	struct tee_iocl_data_pipe_context pipe_ctx;
	data_pipe_t *pipe_ptr = NULL;
	cyclic_cache_t *cache = NULL;
	uint32_t to_write_size = 0;

	DEBUG("enter function %s\n", __func__);

	if (copy_from_user(&pipe_ctx, user_pipe_ctx, sizeof(pipe_ctx))) {
		ERROR("copy_from_user failed\n");
		return TEEC_ERROR_SECURITY;
	}

	if (!pipe_ctx.data_ptr || !pipe_ctx.data_size) {
		ERROR("data can not be NULL\n");
		return TEEC_ERROR_BAD_PARAMETERS;
	}

	pipe_ptr = get_pipe_by_id(pipe_ctx.id);
	if (!pipe_ptr) {
		ERROR("data pipe id is error\n");
		return TEEC_ERROR_BAD_PARAMETERS;
	}

	dump_pipe_info(pipe_ptr);

	if (pipe_ctx.type == PIPE_TYPE_MASTER)
		cache = &pipe_ptr->cache_to_svr;
	else if (pipe_ctx.type == PIPE_TYPE_SLAVE)
		cache = &pipe_ptr->cache_from_svr;
	else {
		ERROR("data pipe type is error\n");
		return TEEC_ERROR_BAD_PARAMETERS;
	}

	to_write_size = pipe_ctx.data_size;
	res = write_cache(pipe_ptr, cache, (void *)(unsigned long)pipe_ctx.data_ptr, &pipe_ctx.data_size);
	if (res != TEEC_SUCCESS || ((pipe_ptr->mode == MODE_BLOCKING) && (to_write_size != pipe_ctx.data_size)))
		close_pipe_by_id(pipe_ptr->id);
	if (res != TEEC_SUCCESS) {
		ERROR("write_cache failed\n");
		return res;
	}

	if (copy_to_user(user_pipe_ctx, &pipe_ctx, sizeof(pipe_ctx))) {
		ERROR("copy_to_user failed\n");
		return TEEC_ERROR_SECURITY;
	}

	DEBUG("leave function %s\n", __func__);

	return TEEC_SUCCESS;
}

uint32_t tee_ioctl_read_pipe_data(struct tee_context *tee_ctx,
		struct tee_iocl_data_pipe_context __user *user_pipe_ctx)
{
	uint32_t res = TEEC_SUCCESS;
	struct tee_iocl_data_pipe_context pipe_ctx;
	data_pipe_t *pipe_ptr = NULL;
	cyclic_cache_t *cache = NULL;
	uint32_t to_read_size = 0;

	DEBUG("enter function %s\n", __func__);

	if (copy_from_user(&pipe_ctx, user_pipe_ctx, sizeof(pipe_ctx))) {
		ERROR("copy_from_user failed\n");
		return TEEC_ERROR_SECURITY;
	}

	if (!pipe_ctx.data_ptr || !pipe_ctx.data_size) {
		ERROR("data can not be NULL\n");
		return TEEC_ERROR_BAD_PARAMETERS;
	}

	pipe_ptr = get_pipe_by_id(pipe_ctx.id);
	if (!pipe_ptr) {
		ERROR("data pipe id is error\n");
		return TEEC_ERROR_BAD_PARAMETERS;
	}

	dump_pipe_info(pipe_ptr);

	if (pipe_ctx.type == PIPE_TYPE_MASTER)
		cache = &pipe_ptr->cache_from_svr;
	else if (pipe_ctx.type == PIPE_TYPE_SLAVE)
		cache = &pipe_ptr->cache_to_svr;
	else {
		ERROR("data pipe type is error\n");
		return TEEC_ERROR_BAD_PARAMETERS;
	}

	to_read_size = pipe_ctx.data_size;
	res = read_cache(pipe_ptr, cache, (void *)(unsigned long)pipe_ctx.data_ptr, &pipe_ctx.data_size);
	if (res != TEEC_SUCCESS || ((pipe_ptr->mode == MODE_BLOCKING) && (to_read_size != pipe_ctx.data_size)))
		close_pipe_by_id(pipe_ptr->id);
	if (res != TEEC_SUCCESS) {
		ERROR("read_cache failed\n");
		return res;
	}

	if (copy_to_user(user_pipe_ctx, &pipe_ctx, sizeof(pipe_ctx))) {
		ERROR("copy_to_user failed\n");
		return TEEC_ERROR_SECURITY;
	}

	DEBUG("leave function %s\n", __func__);

	return TEEC_SUCCESS;
}

uint32_t tee_ioctl_listen_data_pipe(struct tee_context *tee_ctx,
		uint32_t __user *user_backlog)
{
	DEBUG("enter function %s\n", __func__);

	if (copy_from_user(&g_backlog, user_backlog, sizeof(uint32_t))) {
		ERROR("copy_from_user failed\n");
		return TEEC_ERROR_SECURITY;
	}

	if (g_backlog > MAX_NUM_DATA_PIPE || g_backlog == 0)
		g_backlog = MAX_NUM_DATA_PIPE;

	DEBUG("leave function %s\n", __func__);

	return TEEC_SUCCESS;
}

uint32_t tee_ioctl_accept_data_pipe(struct tee_context *tee_ctx,
		uint32_t __user *user_pipe_id)
{
	uint32_t res = TEEC_SUCCESS;
	data_pipe_t *pipe = NULL;
	int wait_status = 0;

	DEBUG("enter function %s\n", __func__);

	wake_up_interruptible(&g_wait_queue_for_open);

	DEBUG("start block in function accept_data_pipe\n");
	wait_status = wait_event_interruptible(g_wait_queue_for_accept, (pipe = get_opening_pipe()) || g_stop_accept);
	if (wait_status) {
		ERROR("wait_event_interruptible for accepting connection failed, return %d\n", wait_status);
		goto exit;
	}
	DEBUG("end block in function accept_data_pipe, pipe->id = %d\n", pipe->id);

	if (pipe) {
		if (copy_to_user(user_pipe_id, &pipe->id, sizeof(uint32_t))) {
			ERROR("copy_to_user failed\n");
			res = TEEC_ERROR_SECURITY;
		} else {
			mutex_lock(&pipe->pipe_lock);
			pipe->status = STATUS_OPENED;
			mutex_unlock(&pipe->pipe_lock);
		}
	}

	if (g_stop_accept)
		res = TEE_ERROR_ACCESS_DENIED;

exit:
	wake_up_interruptible(&g_wait_queue_for_open);

	DEBUG("leave function %s\n", __func__);

	return res;
}
