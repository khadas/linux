/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __DSC_DEBUG_H__
#define __DSC_DEBUG_H__

/* ver:20220808: initial version */
#define DSC_DRV_VERSION  "20210808"

#define DSC_PR(fmt, args...)      pr_info("dsc: " fmt "", ## args)
#define DSC_ERR(fmt, args...)     pr_err("dsc error: " fmt "", ## args)

int dsc_debug_file_create(struct aml_dsc_drv_s *dsc_drv);
int dsc_debug_file_remove(struct aml_dsc_drv_s *dsc_drv);

#endif

