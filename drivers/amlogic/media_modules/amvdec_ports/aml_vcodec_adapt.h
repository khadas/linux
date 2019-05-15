/*
* Copyright (C) 2017 Amlogic, Inc. All rights reserved.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful, but WITHOUT
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
* FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
* more details.
*
* You should have received a copy of the GNU General Public License along
* with this program; if not, write to the Free Software Foundation, Inc.,
* 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*
* Description:
*/
#ifndef VDEC_ADAPT_H
#define VDEC_ADAPT_H

#include <linux/amlogic/media/utils/vformat.h>
#include <linux/amlogic/media/utils/amstream.h>
#include "../stream_input/parser/streambuf.h"
#include "aml_vcodec_drv.h"

struct aml_vdec_adapt {
	enum vformat_e format;
	void *vsi;
	int32_t failure;
	uint32_t inst_addr;
	unsigned int signaled;
	struct aml_vcodec_ctx *ctx;
	struct platform_device *dev;
	wait_queue_head_t wq;
	struct file *filp;
	struct vdec_s *vdec;
	struct stream_port_s port;
	struct dec_sysinfo dec_prop;
	char *recv_name;
};

int video_decoder_init(struct aml_vdec_adapt *ada_ctx);

int video_decoder_release(struct aml_vdec_adapt *ada_ctx);

int vdec_vbuf_write(struct aml_vdec_adapt *ada_ctx,
	const char *buf, unsigned int count);

int vdec_vframe_write(struct aml_vdec_adapt *ada_ctx,
	const char *buf, unsigned int count, unsigned long int timestamp);

int is_need_to_buf(struct aml_vdec_adapt *ada_ctx);

void aml_decoder_flush(struct aml_vdec_adapt *ada_ctx);

int aml_codec_reset(struct aml_vdec_adapt *ada_ctx);

extern void dump_write(const char __user *buf, size_t count);

bool is_input_ready(struct aml_vdec_adapt *ada_ctx);

#endif /* VDEC_ADAPT_H */

