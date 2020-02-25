/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _DI_DBG_H
#define _DI_DBG_H
#include "deinterlace.h"

extern const unsigned int reg_AFBC[AFBC_DEC_NUB][AFBC_REG_INDEX_NUB];

void parse_cmd_params(char *buf_orig, char **parm);
void dump_di_pre_stru(struct di_pre_stru_s *di_pre_stru_p);
void dump_di_post_stru(struct di_post_stru_s *di_post_stru_p);
void dump_di_buf(struct di_buf_s *di_buf);
void dump_pool(struct queue_s *q);
void dump_vframe(vframe_t *vf);
void dump_di_reg_g12(void);
void print_di_buf(struct di_buf_s *di_buf, int format);
void dump_pre_mif_state(void);
void dump_post_mif_reg(void);
void dump_afbcd_reg(void);
void dump_buf_addr(struct di_buf_s *di_buf, unsigned int num);
void dump_mif_size_state(struct di_pre_stru_s *pre,
struct di_post_stru_s *post);
void debug_device_files_add(struct device *dev);
void debug_device_files_del(struct device *dev);
extern void di_debugfs_init(void);
extern void di_debugfs_exit(void);
#endif
