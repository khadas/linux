/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c) 2023 Rockchip Electronics Co., Ltd. */

#ifndef _RKVPSS_PROCFS_H
#define _RKVPSS_PROCFS_H

#ifdef CONFIG_PROC_FS
int rkvpss_proc_init(struct rkvpss_device *dev);
void rkvpss_proc_cleanup(struct rkvpss_device *dev);
#else
static inline int rkvpss_proc_init(struct rkvpss_device *dev) { return 0; }
static inline void rkvpss_proc_cleanup(struct rkvpss_device *dev) {}
#endif

#endif
