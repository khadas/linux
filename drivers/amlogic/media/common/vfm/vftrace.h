/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef VFTRACE_HANDLE_
#define VFTRACE_HANDLE_
#include <linux/amlogic/media/vfm/vframe.h>
void *vftrace_alloc_trace(const char *name, int get, int max);
void vftrace_free_trace(void *handle);
void vftrace_info_in(void *vhandle, struct vframe_s *vf);
void vftrace_dump_trace_infos(void *vhandle);

#endif
