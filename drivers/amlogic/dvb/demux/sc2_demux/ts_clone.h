/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef _TS_CLONE_H_
#define _TS_CLONE_H_

#include <linux/types.h>
#include "ts_input.h"

/**
 * ts_clone_init
 * \retval 0:success.
 * \retval -1:fail.
 */
int ts_clone_init(void);

/**
 * ts_clone_destroy
 * \retval 0:success.
 * \retval -1:fail.
 */
int ts_clone_destroy(void);

/**
 * ts_clone_write
 * \param source:dvr or frontend num source
 * \param pdata:data pointer
 * \param buf_phys: data phy addr
 * \param count:data len
 * \param mode:1:security; 0:normal
 * \param packet_len:188 or 192
 * \retval 0:success.
 * \retval -1:fail.
 */
int ts_clone_write(int source, char *pdata, char *buf_phys, int count, int mode, int packet_len);

/**
 * ts_clone_connect
 * \param dmx_id: dmx_id
 * \param source: dvr or frontend num source
 * \param input: dma pipeline
 * \retval 0:success.
 * \retval -1:fail.
 */
int ts_clone_connect(int dmx_id, int source, struct in_elem *input);

/**
 * ts_clone_disconnect
 * \param dmx_id: dmx_id
 * \param source:dvr or frontend num source
 * \param input:dma pipeline
 * \retval 0:success.
 * \retval -1:fail.
 */
int ts_clone_disconnect(int dmx_id, int source, struct in_elem *input);

/**
 * ts_clone_dump_info
 * \param buf:
 * \retval :len
 */
int ts_clone_dump_info(char *buf);
#endif
