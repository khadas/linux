/*******************************************************************************
 * Copyright (C) 2021 Amlogic, Inc. All rights reserved.
 ******************************************************************************/

/*****************************************************************************/
/**
 *
 * @file adlak_log.h
 * @brief
 *
 * <pre>
 * MODIFICATION HISTORY:
 *
 * Ver   	Who				Date				Changes
 * ----------------------------------------------------------------------------
 * 1.00a shiwei.sun@amlogic.com	2021/06/05	Initial release
 * </pre>
 *
 ******************************************************************************/

#ifndef __ADLAK_LOG_H__
#define __ADLAK_LOG_H__

/***************************** Include Files *********************************/
#include "adlak_config.h"
#include "adlak_error.h"
#include "adlak_typedef.h"
#ifdef __cplusplus
extern "C" {
#endif

/************************** Constant Definitions *****************************/
#if ADLAK_DEBUG
#define LOG_COLOR_GREEN "\033[32m"
#define LOG_COLOR_YELLOW "\033[33m"
#define LOG_COLOR_RED "\033[31m"
#define LOG_COLOR_NULL "\033[0m"
#else
#define LOG_COLOR_GREEN
#define LOG_COLOR_YELLOW
#define LOG_COLOR_RED
#define LOG_COLOR_NULL

#endif
/**************************Global Variable************************************/
extern int g_adlak_log_level;
#if ADLAK_DEBUG
extern int g_adlak_log_level_pre;
#endif

/**************************Type Definition and Structure**********************/
#if ADLAK_DEBUG
#define ASSERT(x)                                                                      \
    do {                                                                               \
        if (!(x)) {                                                                    \
            printk(KERN_EMERG "ASSERTION failed %s:%d: %s\n", __FILE__, __LINE__, #x); \
            BUG();                                                                     \
        }                                                                              \
    } while (0)
#else
#define ASSERT(x) \
    do {          \
    } while (0)
#endif

enum LogLevel {
    LOG_CLOSE = 0,
    LOG_ERR,
    LOG_WARN,
    LOG_INFO,
    LOG_DEBUG,
    LOG_DEFAULT,
};

#if ADLAK_DEBUG
#define LOG1(LogLevel, FMT, ARGS...)                                                      \
    do {                                                                                  \
        if (LogLevel <= g_adlak_log_level) {                                              \
            if (LogLevel == LOG_ERR) {                                                    \
                pr_err(LOG_COLOR_RED "[ADLAK ERROR] pid[%d]  " FMT LOG_COLOR_NULL "\n",   \
                       task_pid_nr(current), ##ARGS);                                     \
                g_adlak_log_level = LOG_DEFAULT;                                          \
            } else if (LogLevel == LOG_WARN) {                                            \
                pr_err(LOG_COLOR_YELLOW "[ADLAK WARN] pid[%d]  " FMT LOG_COLOR_NULL "\n", \
                       task_pid_nr(current), ##ARGS);                                     \
            } else if (LogLevel == LOG_INFO) {                                            \
                pr_err(LOG_COLOR_GREEN "[ADLAK INFO] pid[%d] " FMT LOG_COLOR_NULL "\n",   \
                       task_pid_nr(current), ##ARGS);                                     \
            } else if (LogLevel == LOG_DEBUG) {                                           \
                pr_err("[ADLAK DEBUG] pid[%d] " FMT "\n", task_pid_nr(current), ##ARGS);  \
            } else if (LogLevel == LOG_DEFAULT) {                                         \
                pr_err("pid[%d] " FMT "", task_pid_nr(current), ##ARGS);                  \
            }                                                                             \
        }                                                                                 \
    } while (0)
#define LOG(LogLevel, FMT, ARGS...)                                                       \
    do {                                                                                  \
        if (LogLevel <= g_adlak_log_level) {                                              \
            if (LogLevel == LOG_ERR) {                                                    \
                pr_err(LOG_COLOR_RED "[ADLAK ERROR] " FMT LOG_COLOR_NULL "\n", ##ARGS);   \
                g_adlak_log_level = LOG_DEFAULT;                                          \
            } else if (LogLevel == LOG_WARN) {                                            \
                pr_err(LOG_COLOR_YELLOW "[ADLAK WARN] " FMT LOG_COLOR_NULL "\n", ##ARGS); \
            } else if (LogLevel == LOG_INFO) {                                            \
                pr_err(LOG_COLOR_GREEN "[ADLAK INFO] " FMT LOG_COLOR_NULL "\n", ##ARGS);  \
            } else if (LogLevel == LOG_DEBUG) {                                           \
                pr_err("[ADLAK DEBUG] " FMT "\n", ##ARGS);                                \
            } else if (LogLevel == LOG_DEFAULT) {                                         \
                pr_err("" FMT "", ##ARGS);                                                \
            }                                                                             \
        }                                                                                 \
    } while (0)
#define AML_LOG_ERR(FMT, ARGS...) LOG(LOG_ERR, FMT, ##ARGS)
#define AML_LOG_WARN(FMT, ARGS...) LOG(LOG_WARN, FMT, ##ARGS)
#define AML_LOG_INFO(FMT, ARGS...) LOG(LOG_INFO, FMT, ##ARGS)
#define AML_LOG_DEBUG(FMT, ARGS...) LOG(LOG_DEBUG, FMT, ##ARGS)
#define AML_LOG_DEFAULT(FMT, ARGS...) LOG(LOG_DEFAULT, FMT, ##ARGS)

#else

#define LOG(LogLevel, FMT, ARGS...)                                                       \
    do {                                                                                  \
        if (LogLevel <= g_adlak_log_level) {                                              \
            if (LogLevel == LOG_ERR) {                                                    \
                pr_err(LOG_COLOR_RED "[ADLAK ERROR] %s:%d %s() " FMT LOG_COLOR_NULL "\n", \
                       __FILE__, __LINE__, __FUNCTION__, ##ARGS);                         \
                                                                                          \
            } else if (LogLevel == LOG_WARN) {                                            \
                pr_warn(LOG_COLOR_YELLOW "[ADLAK WARN] %s() " FMT LOG_COLOR_NULL "\n",    \
                        __FUNCTION__, ##ARGS);                                            \
            }                                                                             \
        }                                                                                 \
    } while (0)
#define AML_LOG_ERR(FMT, ARGS...) LOG(LOG_ERR, FMT, ##ARGS)
#define AML_LOG_WARN(FMT, ARGS...) LOG(LOG_WARN, FMT, ##ARGS)
#define AML_LOG_INFO(FMT, ARGS...)
#define AML_LOG_DEBUG(FMT, ARGS...)
#define AML_LOG_DEFAULT(FMT, ARGS...)
#endif

/************************** Function Prototypes ******************************/

#ifdef __cplusplus
}
#endif

#endif /* __ADLAK_LOG_H__ end define*/
