/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __OSD_LOGO_H__
#define __OSD_LOGO_H__

u32 get_logo_fb_width(void);
u32 get_logo_fb_height(void);
u32 get_logo_display_bpp(void);
void get_logo_osd_reverse(u32 *index, u32 *reverse_type);
unsigned long aml_free_reserved_area(void *start, void *end, int poison, const char *s);
#endif
