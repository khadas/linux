/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef AML_KT_LOG
#define AML_KT_LOG

#include <linux/printk.h>

#define LOG_DEBUG (1)
#define LOG_INFO  (2)
#define LOG_ERR   (3)

#define NORMAL_COLOR "\033[0m"
#define RED_COLOR    "\033[1;31m"
#define GREEN_COLOR  "\033[1;32m"
#define YELLOW_COLOR "\033[1;33m"

extern u32 kt_log_level;
extern u32 old_kt_log_level;

#define LOGD(fmt, ...)                                                   \
	do {                                                             \
		if (kt_log_level <= LOG_DEBUG)                               \
			pr_info(GREEN_COLOR "[%s][%d]" NORMAL_COLOR  \
					    " " fmt,                     \
				__func__, __LINE__, ##__VA_ARGS__); \
	} while (0)

#define LOGI(fmt, ...)                                                   \
	do {                                                             \
		if (kt_log_level <= LOG_INFO)                                \
			pr_info(YELLOW_COLOR "[%s][%d]" NORMAL_COLOR \
					     " " fmt,                    \
				__func__, __LINE__, ##__VA_ARGS__); \
	} while (0)

#define LOGE(fmt, ...)                                                        \
	do {                                                                  \
		if (kt_log_level <= LOG_ERR)                                      \
			pr_err(RED_COLOR "[%s][%d]" NORMAL_COLOR " " fmt, \
			    __func__, __LINE__, ##__VA_ARGS__);       \
	} while (0)

#define KT_LOGD(fmt, ...)                                                   \
	do {                                                             \
		if (old_kt_log_level <= LOG_DEBUG)                               \
			pr_info(GREEN_COLOR "[%s][%d]" NORMAL_COLOR  \
					    " " fmt,                     \
				__func__, __LINE__, ##__VA_ARGS__); \
	} while (0)

#define KT_LOGI(fmt, ...)                                                   \
	do {                                                             \
		if (old_kt_log_level <= LOG_INFO)                                \
			pr_info(YELLOW_COLOR "[%s][%d]" NORMAL_COLOR \
					     " " fmt,                    \
				__func__, __LINE__, ##__VA_ARGS__); \
	} while (0)

#define KT_LOGE(fmt, ...)                                                        \
	do {                                                                  \
		if (old_kt_log_level <= LOG_ERR)                                      \
			pr_err(RED_COLOR "[%s][%d]" NORMAL_COLOR " " fmt, \
			    __func__, __LINE__, ##__VA_ARGS__);       \
	} while (0)

#endif /* AML_KT_LOG */
