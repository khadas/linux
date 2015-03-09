#ifndef _OSD_DEBUG_H_
#define _OSD_DEBUG_H_

#include <stdarg.h>
#include <linux/printk.h>

#define OSD_LOG_TAG "[OSD]"
#define OSD_LOG_LEVEL_INFO 0
#define OSD_LOG_LEVEL_DEBUG 1
#define OSD_LOG_LEVEL_ERROR 2

extern unsigned int osd_log_level;
#define osd_log_info(fmt, ...) \
	pr_info(fmt, ##__VA_ARGS__)

#define osd_log_err(fmt, ...) \
	pr_err(fmt, ##__VA_ARGS__)

#define osd_log_dbg(fmt, ...) \
	do { \
		if (osd_log_level == OSD_LOG_LEVEL_ERROR) { \
			pr_err(fmt, ##__VA_ARGS__); \
		} else if (osd_log_level == OSD_LOG_LEVEL_DEBUG) { \
			pr_warn(fmt, ##__VA_ARGS__); \
		} else if (osd_log_level == OSD_LOG_LEVEL_INFO) { \
			pr_info(fmt, ##__VA_ARGS__); \
		} \
	} while (0)

#endif
