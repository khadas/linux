// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include "vpq_module_gamma.h"
#include "../vpq_printk.h"

int vpq_module_gamma_blend(struct vpq_tcon_gamma_table_s *wb_gamma,
				 struct vpq_gamma_tabel_s *index_gamma,
				 struct vpq_tcon_gamma_table_s *target_gamma)
{
	unsigned int i, final_value;
	unsigned int blend_alp, blend_bet;

	for (i = 1; i < 255; i++) {
		blend_alp = index_gamma->data[i] / 1000;
		blend_bet = index_gamma->data[i] % 1000;
		if (blend_alp > 255) {
			VPQ_PR_INFO(PR_MODULE, "%s index_gamma->data[%d]:%d",
				__func__, i, index_gamma->data[i]);
			VPQ_PR_INFO(PR_MODULE, "%s blend_alp:%d blend_bet:%d",
				__func__, blend_alp, blend_bet);
			continue;
		}
		if (blend_alp > 255)
			blend_alp = 255;

		final_value = wb_gamma->data[blend_alp] +
			(wb_gamma->data[blend_alp + 1] - wb_gamma->data[blend_alp]) *
			(blend_bet / 1000);

		target_gamma->data[i] = (unsigned short)final_value;
	}

	//min point & max point
	target_gamma->data[0] = wb_gamma->data[0];
	target_gamma->data[255] = wb_gamma->data[255];

	return 0;
}
