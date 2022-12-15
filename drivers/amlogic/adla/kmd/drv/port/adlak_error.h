/*******************************************************************************
 * Copyright (C) 2022 Amlogic, Inc. All rights reserved.
 ******************************************************************************/

/*****************************************************************************/
/**
 *
 * @file adlak_error.h
 * @brief
 *
 * <pre>
 * MODIFICATION HISTORY:
 *
 * Ver   	Who				Date				Changes
 * ----------------------------------------------------------------------------
 * 1.00a shiwei.sun@amlogic.com	2022/03/28	Initial release
 * </pre>
 *
 ******************************************************************************/

#ifndef __ADLAK_ERROR_H__
#define __ADLAK_ERROR_H__

/***************************** Include Files *********************************/
#include "adlak_os_base.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ADLAK_MAX_ERRNO 4095

#define ADLAK_IS_ERR_VALUE(x) \
    unlikely((unsigned long)(void *)(x) >= (unsigned long)-ADLAK_MAX_ERRNO)

static inline void *__must_check ADLAK_ERR_PTR(long error) { return (void *)error; }

static inline long __must_check ADLAK_PTR_ERR(__force const void *ptr) { return (long)ptr; }

static inline bool __must_check ADLAK_IS_ERR(__force const void *ptr) {
    return ADLAK_IS_ERR_VALUE((unsigned long)ptr);
}

static inline bool __must_check ADLAK_IS_ERR_OR_NULL(__force const void *ptr) {
    return unlikely(!ptr) || ADLAK_IS_ERR_VALUE((unsigned long)ptr);
}

static inline int __must_check ADLAK_PTR_ERR_OR_ZERO(__force const void *ptr) {
    if (ADLAK_IS_ERR(ptr))
        return ADLAK_PTR_ERR(ptr);
    else
        return 0;
}

typedef enum {
    ADLAK_NONE     = 0,
    ADLAK_SUCCESS  = 0,
    ADLAK_ESUCCESS = ADLAK_SUCCESS,
    ADLAK_EPERM    = 1,  /* Operation not permitted */
    ADLAK_ENOENT   = 2,  /* No such file or directory */
    ADLAK_ESRCH    = 3,  /* No such process */
    ADLAK_EINTR    = 4,  /* Interrupted system call */
    ADLAK_EIO      = 5,  /* I/O error */
    ADLAK_ENXIO    = 6,  /* No such device or address */
    ADLAK_E2BIG    = 7,  /* Argument list too long */
    ADLAK_ENOEXEC  = 8,  /* Exec format error */
    ADLAK_EBADF    = 9,  /* Bad file number */
    ADLAK_ECHILD   = 10, /* No child processes */
    ADLAK_EAGAIN   = 11, /* Try again */
    ADLAK_ENOMEM   = 12, /* Out of memory */
    ADLAK_EACCES   = 13, /* Permission denied */
    ADLAK_EFAULT   = 14, /* Bad address */
    ADLAK_ENOTBLK  = 15, /* Block device required */
    ADLAK_EBUSY    = 16, /* Device or resource busy */
    ADLAK_EEXIST   = 17, /* File exists */
    ADLAK_EXDEV    = 18, /* Cross-device link */
    ADLAK_ENODEV   = 19, /* No such device */
    ADLAK_ENOTDIR  = 20, /* Not a directory */
    ADLAK_EISDIR   = 21, /* Is a directory */
    ADLAK_EINVAL   = 22, /* Invalid argument */
    ADLAK_ENFILE   = 23, /* File table overflow */
    ADLAK_EMFILE   = 24, /* Too many open files */
    ADLAK_ENOTTY   = 25, /* Not a typewriter */
    ADLAK_ETXTBSY  = 26, /* Text file busy */
    ADLAK_EFBIG    = 27, /* File too large */
    ADLAK_ENOSPC   = 28, /* No space left on device */
    ADLAK_ESPIPE   = 29, /* Illegal seek */
    ADLAK_EROFS    = 30, /* Read-only file system */
    ADLAK_EMLINK   = 31, /* Too many links */
    ADLAK_EPIPE    = 32, /* Broken pipe */
    ADLAK_EDOM     = 33, /* Math argument out of domain of func */
    ADLAK_ERANGE   = 34, /* Math result not representable */
} adlak_status_t;

#define ERR(code) -ADLAK_##code

#ifdef __cplusplus
}
#endif

#endif /* __ADLAK_ERROR_H__ end define*/
