/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef DI_H
#define DI_H

void dim_post_keep_cmd_release2(struct vframe_s *vframe);

/************************************************
 * dim_polic_cfg
 ************************************************/
void dim_polic_cfg(unsigned int cmd, bool on);
#define K_DIM_BYPASS_CLEAR_ALL	(0)
#define K_DIM_I_FIRST		(1)
#define K_DIM_BYPASS_ALL_P	(2)

/************************************************
 * di_api_get_instance_id
 *	only for deinterlace
 *	get current instance_id
 ************************************************/
u32 di_api_get_instance_id(void);

/************************************************
 * di_api_post_disable
 *	only for deinterlace
 ************************************************/

void di_api_post_disable(void);

#endif /* DI_H */
