/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _VIDEOBUF_RES_H
#define _VIDEOBUF_RES_H

#include <media/videobuf-core.h>

struct videobuf_res_privdata {
	/* const* char dev_name; */
	u32 magic;
	resource_size_t start;
	resource_size_t end;
	void *priv;
};

void videobuf_queue_res_init(struct videobuf_queue *q,
			     const struct videobuf_queue_ops *ops,
			     struct device *dev,
			     spinlock_t *irqlock,
			     enum v4l2_buf_type type,
			     enum v4l2_field field,
			     unsigned int msize,
			     void *priv,
			     /* mutex lock */
			     struct mutex *ext_lock);

resource_size_t videobuf_to_res(struct videobuf_buffer *buf);
void videobuf_res_free(struct videobuf_queue *q,
		       struct videobuf_buffer *buf);

#endif /* _VIDEOBUF_RES_H */
