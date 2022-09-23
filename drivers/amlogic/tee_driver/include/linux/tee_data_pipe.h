/* SPDX-License-Identifier: (GPL-2.0 OR BSD-2-Clause) */
/*
 * Copyright (c) 2015-2022, Amlogic, Inc.
 */

#ifndef __DATA_PIPE_H__
#define __DATA_PIPE_H__

#include "tee_drv.h"
#include "tee.h"

void init_data_pipe_set(void);

void destroy_data_pipe_set(void);

/* pipe client ioctl */
uint32_t tee_ioctl_open_data_pipe(struct tee_context *tee_ctx,
		struct tee_iocl_data_pipe_context __user *user_pipe_ctx);

uint32_t tee_ioctl_close_data_pipe(struct tee_context *tee_ctx,
		struct tee_iocl_data_pipe_context __user *user_pipe_ctx);

uint32_t tee_ioctl_write_pipe_data(struct tee_context *tee_ctx,
		struct tee_iocl_data_pipe_context __user *user_pipe_ctx);

uint32_t tee_ioctl_read_pipe_data(struct tee_context *tee_ctx,
		struct tee_iocl_data_pipe_context __user *user_pipe_ctx);

uint32_t tee_ioctl_listen_data_pipe(struct tee_context *tee_ctx,
		uint32_t __user *user_backlog);

uint32_t tee_ioctl_accept_data_pipe(struct tee_context *tee_ctx,
		uint32_t __user *user_pipe_id);

#endif /*__DATA_PIPE_H__*/
