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
