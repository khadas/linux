/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _TS_INPUT_H_
#define _TS_INPUT_H_

#include <linux/types.h>

struct in_elem;

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
 * \param sec_level:
 * \retval in_elem:success.
 * \retval NULL:fail.
 */
struct in_elem *ts_input_open(int id, int sec_level);

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
 * \param count:
 * \retval size:written count
 * \retval -1:fail.
 */
int ts_input_write(struct in_elem *elem, const char *buf, int count);

int ts_input_write_empty(struct in_elem *elem, int pid);
#endif
