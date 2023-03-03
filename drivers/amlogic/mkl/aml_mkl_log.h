/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef AML_MKL_LOG
#define AML_MKL_LOG

#include <linux/printk.h>

#define LOG_DEBUG (1)
#define LOG_INFO  (2)
#define LOG_WARN  (3)
#define LOG_ERR	  (4)

#ifndef LOG_PRIO
#define LOG_PRIO LOG_INFO
#endif

#if LOG_PRIO == LOG_DEBUG
#define DEBUG
#endif

#ifndef TAG
#define TAG "MKL"
#endif

#define NORMAL_COLOR "\033[0m"
#define RED_COLOR    "\033[1;31m"
#define GREEN_COLOR  "\033[1;32m"
#define YELLOW_COLOR "\033[1;33m"
#define BLUE_COLOR   "\033[1;34m"

#define TRACE                                                               \
	do {                                                                \
		if (LOG_PRIO <= LOG_DEBUG)                                  \
			pr_info("[%s][%s][%d]\n", TAG, __func__, __LINE__); \
	} while (0)

#define LOGD(fmt, ...)                                                   \
	do {                                                             \
		if (LOG_PRIO <= LOG_DEBUG)                               \
			pr_info(GREEN_COLOR "[%s][%s][%d]" NORMAL_COLOR  \
					    " " fmt,                     \
				TAG, __func__, __LINE__, ##__VA_ARGS__); \
	} while (0)

#define LOGI(fmt, ...)                                                   \
	do {                                                             \
		if (LOG_PRIO <= LOG_INFO)                                \
			pr_info(YELLOW_COLOR "[%s][%s][%d]" NORMAL_COLOR \
					     " " fmt,                    \
				TAG, __func__, __LINE__, ##__VA_ARGS__); \
	} while (0)

#define LOGW(fmt, ...)                                                   \
	do {                                                             \
		if (LOG_PRIO <= LOG_WARN)                                \
			pr_warn(BLUE_COLOR "[%s][%s][%d]" NORMAL_COLOR   \
					   " " fmt,                      \
				TAG, __func__, __LINE__, ##__VA_ARGS__); \
	} while (0)

#define LOGE(fmt, ...)                                                        \
	do {                                                                  \
		if (LOG_PRIO <= LOG_ERR)                                      \
			pr_err(RED_COLOR "[%s][%s][%d]" NORMAL_COLOR " " fmt, \
			       TAG, __func__, __LINE__, ##__VA_ARGS__);       \
	} while (0)

#endif /* AML_MKL_LOG */
