/* SPDX-License-Identifier: ((GPL-2.0+ WITH Linux-syscall-note) OR BSD-3-Clause) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __LINUXTV_EXT_HEADER__
#define __LINUXTV_EXT_HEADER__

#define LINUXTV_EXT_VER_MAJOR 1       /* 0 <= major < 16  */
#define LINUXTV_EXT_VER_MINOR 0       /* 0 <= minor < 256 */
#define LINUXTV_EXT_VER_PATCH 1       /* 0 <= patch < 256 */
#define LINUXTV_EXT_VER_SUBMISSION 102  /* 0 <= submission < 4096 */

#define LINUXTV_EXT_VER_MAJOR_WIDTH 4
#define LINUXTV_EXT_VER_MINOR_WIDTH 8
#define LINUXTV_EXT_VER_PATCH_WIDTH 8
#define LINUXTV_EXT_VER_SUBMISSION_WIDTH 12

#define LINUXTV_EXT_VER_MAJOR_SHIFT (32 - LINUXTV_EXT_VER_MAJOR_WIDTH)
#define LINUXTV_EXT_VER_MINOR_SHIFT                                            \
	(LINUXTV_EXT_VER_MAJOR_SHIFT - LINUXTV_EXT_VER_MINOR_WIDTH)
#define LINUXTV_EXT_VER_PATCH_SHIFT                                            \
	(LINUXTV_EXT_VER_MINOR_SHIFT - LINUXTV_EXT_VER_PATCH_WIDTH)

#define LINUXTV_EXT_VER(major, minor, patch, submission)                       \
	(((major) << LINUXTV_EXT_VER_MAJOR_SHIFT) +                                \
	((minor) << LINUXTV_EXT_VER_MINOR_SHIFT) +                                \
	((patch) << LINUXTV_EXT_VER_PATCH_SHIFT) + (submission))

#define LINUXTV_EXT_VER_CUR                                                    \
	LINUXTV_EXT_VER(LINUXTV_EXT_VER_MAJOR, LINUXTV_EXT_VER_MINOR,              \
					LINUXTV_EXT_VER_PATCH, LINUXTV_EXT_VER_SUBMISSION)

#endif /* __LINUXTV_EXT_HEADER__ */
