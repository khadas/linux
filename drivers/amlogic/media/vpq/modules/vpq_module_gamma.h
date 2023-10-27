/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __VPQ_MODULE_GAMMA_H__
#define __VPQ_MODULE_GAMMA_H__

#include "../vpq_common.h"

int vpq_module_gamma_blend(struct vpq_tcon_gamma_table_s *wb_gamma,
				 struct vpq_gamma_tabel_s *index_gamma,
				 struct vpq_tcon_gamma_table_s *target_gamma);

#endif //__VPQ_MODULE_GAMMA_H__

