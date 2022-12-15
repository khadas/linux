/*******************************************************************************
 * Copyright (C) 2022 Amlogic, Inc. All rights reserved.
 ******************************************************************************/

/*****************************************************************************/
/**
 *
 * @file adlak_os.c
 * @brief
 *
 * <pre>
 * MODIFICATION HISTORY:
 *
 * Ver   	Who				Date				Changes
 * ----------------------------------------------------------------------------
 * 1.00a shiwei.sun@amlogic.com	2022/04/08	Initial release
 * </pre>
 *
 ******************************************************************************/

/***************************** Include Files *********************************/

#include "adlak_os.h"

#include "adlak_common.h"
/************************** Constant Definitions *****************************/
#define PRINT_FUNC_NAME
// #define PRINT_FUNC_NAME AML_LOG_DEBUG("%s() ", __FUNCTION__)

/**************************** Type Definitions *******************************/

/***************** Macros (Inline Functions) Definitions *********************/

/************************** Variable Definitions *****************************/
#if ADLAK_DEBUG
static int32_t dbg_mem_alloc_count_kmd = 0;
#endif
/************************** Function Prototypes ******************************/
void *adlak_os_malloc(size_t size, uint32_t flag) {
#if ADLAK_DEBUG
    void *addr = NULL;
    dbg_mem_alloc_count_kmd += 1;
    addr = kmalloc(size, flag | ADLAK_GFP_KERNEL);
    AML_LOG_DEBUG("alloc: dbg_mem_alloc_count_kmd = %d, addr = %p\n", dbg_mem_alloc_count_kmd,
                  addr);
    return addr;
#else
    return kmalloc(size, flag | ADLAK_GFP_KERNEL);
#endif
}
void *adlak_os_zalloc(size_t size, uint32_t flag) {
#if ADLAK_DEBUG
    void *addr = NULL;
    dbg_mem_alloc_count_kmd += 1;
    addr = kzalloc(size, flag | ADLAK_GFP_KERNEL);
    AML_LOG_DEBUG("alloc: dbg_mem_alloc_count_kmd = %d, addr = %p\n", dbg_mem_alloc_count_kmd,
                  addr);
    return addr;
#else
    return kzalloc(size, flag | ADLAK_GFP_KERNEL);
#endif
}

void *adlak_os_calloc(size_t num, size_t size, uint32_t flag) {
    return adlak_os_zalloc(size * num, flag);
}

void adlak_os_free(void *ptr) {
#if ADLAK_DEBUG
    dbg_mem_alloc_count_kmd -= 1;
    AML_LOG_DEBUG("free: dbg_mem_alloc_count_kmd = %d, addr = %p\n", dbg_mem_alloc_count_kmd, ptr);
#endif
    kfree(ptr);
}

void *adlak_os_vmalloc(size_t size, uint32_t flag) {
#if ADLAK_DEBUG
    void *addr = NULL;
    dbg_mem_alloc_count_kmd += 1;
    addr = vmalloc(size);
    AML_LOG_DEBUG("alloc: dbg_mem_alloc_count_kmd = %d, addr = %p\n", dbg_mem_alloc_count_kmd,
                  addr);
    return addr;
#else
    return vmalloc(size);
#endif
}

void *adlak_os_vzalloc(size_t size, uint32_t flag) {
#if ADLAK_DEBUG
    void *addr = NULL;
    dbg_mem_alloc_count_kmd += 1;
    addr = vzalloc(size);
    AML_LOG_DEBUG("alloc: dbg_mem_alloc_count_kmd = %d, addr = %p\n", dbg_mem_alloc_count_kmd,
                  addr);
    return addr;
#else
    return vzalloc(size);
#endif
}

void adlak_os_vfree(void *ptr) {
#if ADLAK_DEBUG
    dbg_mem_alloc_count_kmd -= 1;
    AML_LOG_DEBUG("free: dbg_mem_alloc_count_kmd = %d, addr = %p\n", dbg_mem_alloc_count_kmd, ptr);
#endif
    vfree(ptr);
}

void *adlak_os_memset(void *dest, int ch, size_t count) { return memset(dest, ch, count); }

void *adlak_os_memcpy(void *dest, const void *src, size_t count) {
    return memcpy(dest, src, count);
}

int adlak_os_memcmp(const void *lhs, const void *rhs, size_t count) {
    return memcmp(lhs, rhs, count);
}

int adlak_os_printf(const char *format, ...) {
    va_list args;
    int     r;
    va_start(args, format);
    r = vprintk(format, args);
    va_end(args);
    return r;
}
int adlak_os_snprintf(char *buffer, ssize_t size, const char *format, ...) {
    int     cnt;
    va_list arg;
    if (size <= 0) {
        return 0;
    }
    va_start(arg, format);
    cnt = vsnprintf(buffer, size, format, arg);
    va_end(arg);
    return cnt;
}

/**
 * adlak_os_vasprintf - Allocate resource managed space and format a string
 *		     into that.
 * @gfp: the GFP mask used in the devm_kmalloc() call when
 *       allocating memory
 * @fmt: The printf()-style format string
 * @ap: Arguments for the format string
 * RETURNS:
 * Pointer to allocated string on success, NULL on failure.
 */
static char *adlak_os_vasprintf(gfp_t gfp, const char *fmt, va_list ap) {
    unsigned int len;
    char *       p;
    va_list      aq;

    va_copy(aq, ap);
    len = vsnprintf(NULL, 0, fmt, aq);
    va_end(aq);

    p = kmalloc(len + 1, gfp);
    if (!p) return NULL;

    vsnprintf(p, len + 1, fmt, ap);

    return p;
}

/**
 * @brief Allocate resource managed space and format a string
 *		    into that.
 *
 * @param gfp the GFP mask used in the devm_kmalloc() call when
 *       allocating memory
 * @param fmt: The printf()-style format string
 * @param ... Arguments for the format string
 * @return char* Pointer to allocated string on success, NULL on failure.
 */
char *adlak_os_asprintf(gfp_t gfp, const char *fmt, ...) {
    va_list ap;
    char *  p;

    va_start(ap, fmt);
    p = adlak_os_vasprintf(gfp, fmt, ap);
    va_end(ap);

    return p;
}

void adlak_os_msleep(unsigned int ms) { msleep(ms); }

typedef struct adlak_os_mutex_inner {
    struct mutex mutex_hd;
} adlak_os_mutex_inner_t;

int adlak_os_mutex_init(adlak_os_mutex_t *mutex) {
    adlak_os_mutex_inner_t *pmutex_inner = NULL;
    PRINT_FUNC_NAME;
    pmutex_inner =
        (adlak_os_mutex_inner_t *)adlak_os_malloc(sizeof(adlak_os_mutex_inner_t), ADLAK_GFP_KERNEL);
    if (ADLAK_IS_ERR_OR_NULL(pmutex_inner)) {
        *mutex = NULL;
        return ERR(ENOMEM);
    }
    mutex_init(&pmutex_inner->mutex_hd);
    *mutex = pmutex_inner;
    AML_LOG_DEBUG("mutex_init success!\n");
    return ERR(NONE);
}

int adlak_os_mutex_destroy(adlak_os_mutex_t *mutex) {
    adlak_os_mutex_inner_t *pmutex_inner = (adlak_os_mutex_inner_t *)*mutex;
    PRINT_FUNC_NAME;
    if (pmutex_inner != NULL) {
        mutex_destroy(&pmutex_inner->mutex_hd);
        adlak_os_free((void *)pmutex_inner);
        *mutex = NULL;
        AML_LOG_DEBUG("mutex_destroy success!\n");
        return ERR(NONE);
    }
    return ERR(EINVAL);
}

int adlak_os_mutex_lock(adlak_os_mutex_t *mutex) {
    adlak_os_mutex_inner_t *pmutex_inner = (adlak_os_mutex_inner_t *)*mutex;
    PRINT_FUNC_NAME;
    if (pmutex_inner != NULL) {
        // if (mutex_lock_interruptible(&pmutex_inner->mutex_hd)) {
        //     return ERR(EFAULT);
        // }
        mutex_lock(&pmutex_inner->mutex_hd);
        return ERR(NONE);
    }
    return ERR(EINVAL);
}

int adlak_os_mutex_unlock(adlak_os_mutex_t *mutex) {
    adlak_os_mutex_inner_t *pmutex_inner = (adlak_os_mutex_inner_t *)*mutex;
    PRINT_FUNC_NAME;
    if (pmutex_inner != NULL) {
        mutex_unlock(&pmutex_inner->mutex_hd);
        return ERR(NONE);
    }
    return ERR(EINVAL);
}

typedef struct adlak_os_spinlock_inner {
    spinlock_t    spinlock_hd;
    unsigned long flags;
} adlak_os_spinlock_inner_t;

int adlak_os_spinlock_init(adlak_os_spinlock_t *spinlock) {
    adlak_os_spinlock_inner_t *pspinlock_inner = NULL;
    PRINT_FUNC_NAME;
    pspinlock_inner = (adlak_os_spinlock_inner_t *)adlak_os_malloc(
        sizeof(adlak_os_spinlock_inner_t), ADLAK_GFP_KERNEL);
    if (ADLAK_IS_ERR_OR_NULL(pspinlock_inner)) {
        *spinlock = NULL;
        return ERR(ENOMEM);
    }
    spin_lock_init(&pspinlock_inner->spinlock_hd);
    *spinlock = pspinlock_inner;
    AML_LOG_DEBUG("spinlock_init success!\n");
    return ERR(NONE);
}

int adlak_os_spinlock_destroy(adlak_os_spinlock_t *spinlock) {
    adlak_os_spinlock_inner_t *pspinlock_inner = (adlak_os_spinlock_inner_t *)*spinlock;
    PRINT_FUNC_NAME;
    if (pspinlock_inner != NULL) {
        adlak_os_free((void *)pspinlock_inner);
        *spinlock = NULL;
        AML_LOG_DEBUG("spinlock_destroy success!\n");
        return ERR(NONE);
    }
    return ERR(EINVAL);
}

int adlak_os_spinlock_lock(adlak_os_spinlock_t *spinlock) {
    adlak_os_spinlock_inner_t *pspinlock_inner = (adlak_os_spinlock_inner_t *)*spinlock;
    PRINT_FUNC_NAME;
    if (pspinlock_inner != NULL) {
        spin_lock_irqsave(&pspinlock_inner->spinlock_hd, pspinlock_inner->flags);
        return ERR(NONE);
    }
    return ERR(EINVAL);
}

int adlak_os_spinlock_unlock(adlak_os_spinlock_t *spinlock) {
    adlak_os_spinlock_inner_t *pspinlock_inner = (adlak_os_spinlock_inner_t *)*spinlock;
    PRINT_FUNC_NAME;
    if (pspinlock_inner != NULL) {
        spin_unlock_irqrestore(&pspinlock_inner->spinlock_hd, pspinlock_inner->flags);
        return ERR(NONE);
    }
    return ERR(EINVAL);
}

// Semaphore
typedef struct adlak_os_sema_inner {
    struct semaphore sem;
} adlak_os_sema_inner_t;

int adlak_os_sema_init(adlak_os_sema_t *sem, unsigned int max_count, unsigned int init_count) {
    adlak_os_sema_inner_t *psema_inner = NULL;
    PRINT_FUNC_NAME;
    psema_inner =
        (adlak_os_sema_inner_t *)adlak_os_malloc(sizeof(adlak_os_sema_inner_t), ADLAK_GFP_KERNEL);
    if (ADLAK_IS_ERR_OR_NULL(psema_inner)) {
        *sem = NULL;
        return ERR(ENOMEM);
    }
    sema_init(&psema_inner->sem, init_count);
    *sem = psema_inner;
    AML_LOG_DEBUG("sema_init success!\n");

    return ERR(NONE);
}

int adlak_os_sema_destroy(adlak_os_sema_t *sem) {
    adlak_os_sema_inner_t *psema_inner = (adlak_os_sema_inner_t *)*sem;
    PRINT_FUNC_NAME;
    if (psema_inner != NULL) {
        adlak_os_free((void *)psema_inner);
        *sem = NULL;
        AML_LOG_DEBUG("sema_destroy success!\n");
        return ERR(NONE);
    }
    return ERR(EINVAL);
}

int adlak_os_sema_take(adlak_os_sema_t sem) {
    adlak_os_sema_inner_t *psema_inner = (adlak_os_sema_inner_t *)sem;
    PRINT_FUNC_NAME;
    if (psema_inner != NULL) {
        if (down_interruptible(&psema_inner->sem)) {
            return ERR(EINTR);
        } else {
            return ERR(NONE);
        }
    }
    return ERR(EINVAL);
}

int adlak_os_sema_take_timeout(adlak_os_sema_t sem, unsigned int ms) {
    adlak_os_sema_inner_t *psema_inner = (adlak_os_sema_inner_t *)sem;
    long                   jiffies;

    int ret = 0;
    PRINT_FUNC_NAME;
    if (psema_inner != NULL) {
        jiffies = msecs_to_jiffies(ms);

        ret = down_timeout(&psema_inner->sem, jiffies);
        if (ret) {
            AML_LOG_DEBUG("Failed to acquire semaphore[%p|%d]", &psema_inner->sem, ms);
            return ERR(EINTR);
        }
        return ERR(NONE);
    }
    return ERR(EINVAL);
}

int adlak_os_sema_give(adlak_os_sema_t sem) {
    adlak_os_sema_inner_t *psema_inner = (adlak_os_sema_inner_t *)sem;
    PRINT_FUNC_NAME;
    if (psema_inner != NULL) {
        up(&psema_inner->sem);
        return ERR(NONE);
    }
    return ERR(EINVAL);
}

int adlak_os_sema_give_from_isr(adlak_os_sema_t sem) {
    adlak_os_sema_inner_t *psema_inner = (adlak_os_sema_inner_t *)sem;
    PRINT_FUNC_NAME;
    if (psema_inner != NULL) {
        up(&psema_inner->sem);
        return ERR(NONE);
    }
    return ERR(EINVAL);
}

//

typedef struct adlak_os_thread_inner {
    struct task_struct *kthread;
} adlak_os_thread_inner_t;

static int adlak_sched_priority = MAX_RT_PRIO / 4;
module_param_named(sched_priority, adlak_sched_priority, int, 0644);
MODULE_PARM_DESC(sched_priority, "the adlak_sched_priority of kmd thread");

static void signaler_set_rtpriority(adlak_os_thread_t *pthrd) {
    adlak_os_thread_inner_t *pthread_inner = (adlak_os_thread_inner_t *)pthrd->handle;
    struct sched_param       param         = {.sched_priority = adlak_sched_priority};

    sched_setscheduler_nocheck(pthread_inner->kthread, SCHED_FIFO, &param);
}

int adlak_os_thread_create(adlak_os_thread_t *pthrd, int(*func)(void *), void *arg) {
    static uint32_t          thread_num    = 0;
    adlak_os_thread_inner_t *pthread_inner = NULL;

    PRINT_FUNC_NAME;
    pthread_inner = (adlak_os_thread_inner_t *)adlak_os_malloc(sizeof(adlak_os_thread_inner_t),
                                                               ADLAK_GFP_KERNEL);
    if (ADLAK_IS_ERR_OR_NULL(pthread_inner)) {
        AML_LOG_ERR("malloc for thread_internal_t fail!\n");
        return ERR(ENOMEM);
    }
    pthrd->thrd_should_stop = 0;
    pthread_inner->kthread =
        kthread_create((int (*)(void *))func, (void *)arg, "adlak_kthread_%d", thread_num);
    if (ADLAK_IS_ERR_OR_NULL(pthread_inner->kthread)) {
        adlak_os_free(pthread_inner);
        pthrd->handle = (void *)pthread_inner;
        AML_LOG_DEBUG("thread create fail!\n");
        return ERR(ENXIO);
    } else {
        pthrd->handle = (void *)pthread_inner;
        AML_LOG_DEBUG("thread create success!\n");
        wake_up_process(pthread_inner->kthread);
        thread_num++;
        return ERR(NONE);
    }
    signaler_set_rtpriority(pthrd);
    return ERR(NONE);
}

int adlak_os_thread_detach(adlak_os_thread_t *pthrd) {
    int                      ret;
    adlak_os_thread_inner_t *pthread_inner = (adlak_os_thread_inner_t *)pthrd->handle;
    PRINT_FUNC_NAME;
    if (pthread_inner) {
        ret = ERR(NONE);
        if (pthread_inner->kthread) {
            pthrd->thrd_should_stop = 1;
            ret                     = kthread_stop(pthread_inner->kthread);
            if (ret) {
                AML_LOG_ERR("pthread_detach fail!\n");
            } else {
                AML_LOG_DEBUG("pthread_detach success!\n");
            }
            while (pthrd->thrd_should_stop) {
                adlak_os_msleep(10);
            }
        }
        adlak_os_free(pthread_inner);
        pthrd->handle = NULL;
        return ret;
    }
    return ERR(EPERM);
}
void adlak_os_thread_yield(void) {
    PRINT_FUNC_NAME;
    yield(); /* start all threads before we kthread_stop() */
}

typedef struct adlak_os_timer_inner {
    struct timer_list timer;
    unsigned long     flags;
} adlak_os_timer_inner_t;

int adlak_os_timer_init(adlak_os_timer_t *ptim, void (*func)(struct timer_list *), void *param) {
    adlak_os_timer_inner_t *ptimer_inner = NULL;
    PRINT_FUNC_NAME;
    ptimer_inner =
        (adlak_os_timer_inner_t *)adlak_os_malloc(sizeof(adlak_os_timer_inner_t), ADLAK_GFP_KERNEL);
    if (ADLAK_IS_ERR_OR_NULL(ptimer_inner)) {
        return ERR(ENOMEM);
    }
    timer_setup(&ptimer_inner->timer, (void (*)(struct timer_list *))func, 0);
    *ptim = ptimer_inner;
    AML_LOG_DEBUG("timer_init success!\n");
    return ERR(NONE);
}

int adlak_os_timer_destroy(adlak_os_timer_t *ptim) {
    adlak_os_timer_inner_t *ptimer_inner = (adlak_os_timer_inner_t *)*ptim;
    PRINT_FUNC_NAME;
    if (ptimer_inner != NULL) {
        del_timer_sync(&ptimer_inner->timer);
        adlak_os_free((void *)ptimer_inner);
        *ptim = NULL;
        AML_LOG_DEBUG("timer_destroy success!\n");
        return ERR(NONE);
    }
    return ERR(EINVAL);
}

int adlak_os_timer_del(adlak_os_timer_t *ptim) {
    adlak_os_timer_inner_t *ptimer_inner = (adlak_os_timer_inner_t *)*ptim;
    PRINT_FUNC_NAME;
    if (ptimer_inner != NULL) {
        del_timer_sync(&ptimer_inner->timer);
        AML_LOG_DEBUG("timer_del success!\n");
        return ERR(NONE);
    }
    return ERR(EINVAL);
}

int adlak_os_timer_add(adlak_os_timer_t *ptim, unsigned int timeout_ms) {
    adlak_os_timer_inner_t *ptimer_inner = (adlak_os_timer_inner_t *)*ptim;
    unsigned long           expire;
    PRINT_FUNC_NAME;

    if (ptimer_inner != NULL) {
        expire                      = jiffies + msecs_to_jiffies(timeout_ms);
        ptimer_inner->timer.expires = round_jiffies_relative(expire);
        add_timer(&ptimer_inner->timer);
        return ERR(NONE);
    }
    return ERR(EINVAL);
}

/**
 * @brief task wait handle init
 *
 * @param hd
 * @return int
 */
int adlak_to_umd_sinal_init(uintptr_t *hd) {
    wait_queue_head_t *wait_inner = NULL;
    PRINT_FUNC_NAME;
    wait_inner = (wait_queue_head_t *)adlak_os_malloc(sizeof(wait_queue_head_t), ADLAK_GFP_KERNEL);
    if (ADLAK_IS_ERR_OR_NULL(wait_inner)) {
        return ERR(ENOMEM);
    }

    init_waitqueue_head(wait_inner);
    *hd = (uintptr_t)wait_inner;
    AML_LOG_DEBUG("adlak_to_umd_sinal_init success!\n");
    return ERR(NONE);
}

/**
 * @brief task wait handle destroy
 *
 * @param hd
 * @return int
 */
int adlak_to_umd_sinal_deinit(uintptr_t *hd) {
    wait_queue_head_t *wait_inner = (wait_queue_head_t *)*hd;
    PRINT_FUNC_NAME;
    if (wait_inner != NULL) {
        adlak_os_free((void *)wait_inner);
        *hd = (uintptr_t)NULL;
        AML_LOG_DEBUG("timer_destroy success!\n");
        return ERR(NONE);
    }
    return ERR(EINVAL);
}

int adlak_to_umd_sinal_wait(uintptr_t hd, unsigned int timeout_ms) { return ERR(EINVAL); }

int adlak_to_umd_sinal_give(uintptr_t hd) {
    wait_queue_head_t *wait_inner = (wait_queue_head_t *)hd;
    PRINT_FUNC_NAME;
    if (wait_inner != NULL) {
        wake_up_interruptible(wait_inner);
        return ERR(NONE);
    }
    return ERR(EINVAL);
}

adlak_os_ktime_t adlak_os_ktime_get(void) { return (adlak_os_ktime_t)ktime_get(); }

uintptr_t adlak_os_ktime_us_delta(const adlak_os_ktime_t later, const adlak_os_ktime_t earlier) {
    return (uintptr_t)ktime_us_delta((const ktime_t)later, (const ktime_t)earlier);
}