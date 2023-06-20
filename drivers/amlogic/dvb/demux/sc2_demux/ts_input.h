/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _TS_INPUT_H_
#define _TS_INPUT_H_

#include <linux/types.h>

struct in_elem;

struct in_elem {
	__u8 used;
	__u8 id;
	struct chan_id *pchan;
};

/**
 * ts_input_init
 * \retval 0:success.
 * \retval -1:fail.
 */
int ts_input_init(void);

/**
 * ts_input_destroy
 * \retval 0:success.
 * \retval -1:fail.
 */
int ts_input_destroy(void);

/**
 * ts_input_open
 * \param id:
 * \retval in_elem:success.
 * \retval NULL:fail.
 */
struct in_elem *ts_input_open(int id);

/**
 * ts_input_close
 * \param elem:
 * \retval 0:success.
 * \retval -1:fail.
 */
int ts_input_close(struct in_elem *elem);

/**
 * ts_input_write
 * \param elem:
 * \param buf:
 * \param buf_phys:
 * \param count:
 * \param mode: 1:security; 0:normal
 * \param packet_len:188 or 192
 * \retval size:written count
 * \retval -1:fail.
 */
int ts_input_write(struct in_elem *elem, const char *buf, char *buf_phys, int count,
		int mode, int packet_len);

int ts_input_write_empty(struct in_elem *elem, int pid);

/**
 * ts_input_non_block_write
 * \param elem:
 * \param buf:
 * \param buf_phys:
 * \param count:
 * \param mode: 1:security; 0:normal
 * \param packet_len:188 or 192
 * \retval size:written count
 * \retval -1:fail.
 */
int ts_input_non_block_write(struct in_elem *elem, const char *buf, char *buf_phys, int count,
		int mode, int packet_len);

/**
 * ts_input_non_block_write_status
 * \param elem:
 * \retval 1:done, 0:not done
 */
int ts_input_non_block_write_status(struct in_elem *elem);

/**
 * ts_input_non_block_write_free
 * \param elem:
 * \retval 0:success
 */
int ts_input_non_block_write_free(struct in_elem *elem);

#endif
