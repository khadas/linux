// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2017, Amlogic, Inc.
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
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/workqueue.h>
#include <linux/io.h>

#include "optee_smc.h"
#include "log.h"
#include "optee_private.h"

#define LOGGER_LOOPBUFFER_MAGIC       0xAA00AA00
#define LOGGER_LOOPBUFFER_OFFSET      0x00000080
#define OPTEE_LOG_READ_MAX            PAGE_SIZE
#define OPTEE_LOG_LINE_MAX            1024
#define OPTEE_LOG_TIMER_INTERVAL      1

#undef pr_fmt
#define pr_fmt(fmt) "[TEE] " fmt

struct loopbuffer_ctl_s {
	unsigned int magic;
	unsigned int inited;
	unsigned int total_size;
	unsigned int fill_size;
	unsigned int mode;
	unsigned int reader;
	unsigned int writer;

	char *buffer;
};

static void *log_buf_va;
static u8 line_buff[OPTEE_LOG_LINE_MAX];

struct delayed_work log_work;
static struct workqueue_struct *log_workqueue;

static ssize_t log_buff_get_read_buff(char **buf, int len)
{
	int writer;
	int reader;
	int read_size = 0;
	struct loopbuffer_ctl_s *ctl = (struct loopbuffer_ctl_s *)log_buf_va;

	if (!ctl || len <= 0)
		return 0;

	writer = ctl->writer;
	reader = ctl->reader;

	if (reader == writer)
		read_size = 0;
	else if (reader < writer)
		read_size = writer - reader;
	else
		read_size = ctl->total_size - reader;

	if (read_size > len)
		read_size = len;

	*buf = (char *)log_buf_va + LOGGER_LOOPBUFFER_OFFSET + reader;
	ctl->reader += read_size;
	if (ctl->reader == ctl->total_size)
		ctl->reader = 0;

	return read_size;
}

static size_t log_print_text(char *buf, size_t size)
{
	char *line = line_buff;
	const char *text = buf;
	const char *next = NULL;
	s32 remaining = size;
	size_t line_size = 0;
	size_t scan_size = 0;

	if (!buf || !size)
		return 0;

	while (text && remaining > 0) {
		/* Reserve 2 bytes for EOL and EOS */
		scan_size = remaining > (OPTEE_LOG_LINE_MAX - 2) ?
			(OPTEE_LOG_LINE_MAX - 2) : remaining;
		next = memchr(text, '\n', scan_size);
		if (next) {
			/* EOL is found */
			next++;
			line_size = next - text;
			/* Add a extra EOS */
			line[line_size] = '\0';
		} else {
			/* No EOL found.*/
			line_size = scan_size;
			/* Truncate string to scan_size and add EOL and EOS. */
			line[line_size] = '\n';
			line[line_size + 1] = '\0';
		}
		memcpy(line, text, line_size);
		remaining -= line_size;
		text += line_size;
		pr_notice("%s", line);
	}

	/* All remaining should be consumed */
	if (!text || remaining) {
		pr_err("WARNING: text(%p) is NULL or remaining(%d) is not 0.\n",
				text, remaining);
	}

	return size;
}

static void do_log_timer(struct work_struct *work)
{
	size_t len;
	char *read_buff = NULL;

	len = log_buff_get_read_buff(&read_buff, OPTEE_LOG_READ_MAX);
	if (len > 0)
		log_print_text(read_buff, len);

	if (queue_delayed_work(log_workqueue, &log_work, OPTEE_LOG_TIMER_INTERVAL * HZ) == 0)
		pr_err("%s:%d Failed to join the workqueue\n", __func__, __LINE__);
}

int optee_log_init(void)
{
	int rc = 0;
	size_t size = 0;
	phys_addr_t begin = 0;
	phys_addr_t end = 0;
	struct arm_smccc_res smccc = { 0 };
	struct loopbuffer_ctl_s *log_ctl = NULL;

	arm_smccc_smc(OPTEE_SMC_GET_LOGGER_CONFIG, 0, 0, 0, 0, 0, 0, 0, &smccc);
	if (smccc.a0 == TEEC_SUCCESS) {
		/* v3, get log buffer config from BL32 */
		begin = roundup(smccc.a1, PAGE_SIZE);
		end = rounddown(smccc.a1 + smccc.a2, PAGE_SIZE);
		size = end - begin;
	} else if (smccc.a0 == OPTEE_SMC_RETURN_UNKNOWN_FUNCTION) {
		/* v1 & v2, can not get log buffer config, use log buffer in share memory */
		memset(&smccc, 0, sizeof(smccc));
		arm_smccc_smc(OPTEE_SMC_GET_SHM_CONFIG, 0, 0, 0, 0, 0, 0, 0, &smccc);
		if (smccc.a0 != TEEC_SUCCESS) {
			pr_err("tee get share memory config failed, res = 0x%lx\n", smccc.a0);
			rc = -EACCES;
			goto err;
		}

		begin = rounddown(smccc.a1 + smccc.a2, PAGE_SIZE) - DEF_LOGGER_SHM_SIZE;
		size = DEF_LOGGER_SHM_SIZE;
	} else {
		/* Logger disabled */
		rc = -EACCES;
		goto err;
	}

	log_buf_va = memremap(begin, size, MEMREMAP_WB);
	if (!log_buf_va) {
		pr_err("tee log buffer memremap failed\n");
		rc = -ENOMEM;
		goto err;
	}

	memset(&smccc, 0, sizeof(smccc));
	arm_smccc_smc(OPTEE_SMC_ENABLE_LOGGER, 1, 0, 0, 0, 0, 0, 0, &smccc);
	if (smccc.a0 != TEEC_SUCCESS) {
		pr_err("tee log buffer enable failed, res = 0x%lx\n", smccc.a0);
		rc = -EACCES;
		goto err;
	}

	log_ctl = (struct loopbuffer_ctl_s *)log_buf_va;
	if (log_ctl->magic != LOGGER_LOOPBUFFER_MAGIC || log_ctl->inited != 1) {
		pr_err("tee log buffer init failed\n");
		rc = -EINVAL;
		goto err;
	}

	/* init workqueue */
	log_workqueue = create_singlethread_workqueue("tee-log-wq");
	INIT_DELAYED_WORK(&log_work, do_log_timer);
	if (queue_delayed_work(log_workqueue, &log_work, OPTEE_LOG_TIMER_INTERVAL * HZ) == 0) {
		pr_err("%s:%d failed to join the workqueue.\n", __func__, __LINE__);
		rc = -EBUSY;
		goto err;
	}

	return rc;

err:
	arm_smccc_smc(OPTEE_SMC_ENABLE_LOGGER, 0, 0, 0, 0, 0, 0, 0, &smccc);

	return rc;
}

void optee_log_uninit(void)
{
	struct arm_smccc_res smccc;

	if (log_workqueue) {
		cancel_delayed_work_sync(&log_work);
		destroy_workqueue(log_workqueue);
	}

	arm_smccc_smc(OPTEE_SMC_ENABLE_LOGGER, 0, 0, 0, 0, 0, 0, 0, &smccc);
}
