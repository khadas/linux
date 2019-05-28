#include <linux/platform_device.h>
#include <linux/version.h>
#include <linux/pm.h>
#include <linux/of.h>
#include <linux/module.h>            /* kernel module definitions */
#include <linux/ioport.h>            /* request_mem_region */
#include <linux/slab.h>
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 14, 29))
#include <mach/register.h>
#include <mach/irqs.h>
#include <mach/io.h>
#endif
#include "meson_main.h"
#include <common/mali_kernel_common.h>
#include <common/mali_osk_profiling.h>
#include <common/mali_pmu.h>
#include <linux/mali/mali_utgard.h>

static int mali_os_suspend(struct device *device)
{
	int ret = 0;

	MALI_DEBUG_PRINT(4, ("mali_os_suspend() called\n"));
	ret = mali_deep_suspend(device);

	return ret;
}

static int mali_os_resume(struct device *device)
{
	int ret = 0;

	MALI_DEBUG_PRINT(4, ("mali_os_resume() called\n"));

	ret = mali_deep_resume(device);

	return ret;
}

static int mali_os_freeze(struct device *device)
{
	int ret = 0;

	MALI_DEBUG_PRINT(4, ("mali_os_freeze() called\n"));

	mali_dev_freeze();

	if (NULL != device->driver &&
	    NULL != device->driver->pm &&
	    NULL != device->driver->pm->freeze)
	{
		/* Need to notify Mali driver about this event */
		ret = device->driver->pm->freeze(device);
	}

	return ret;
}
//copy from r4p1 linux/mali_pmu_power_up_down.c
#if MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON8
static int mali_pmu_powerup(void)
{
	struct mali_pmu_core *pmu = mali_pmu_get_global_pmu_core();

	MALI_DEBUG_PRINT(5, ("Mali PMU: Power up\n"));

	MALI_DEBUG_ASSERT_POINTER(pmu);
	if (NULL == pmu) {
		return -ENXIO;
	}

	mali_pmu_power_up_all(pmu);

	return 0;
}
#endif

static int mali_os_thaw(struct device *device)
{
	int ret = 0;

	MALI_DEBUG_PRINT(4, ("mali_os_thaw() called\n"));
#if MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON8
	enable_clock();
	mali_pmu_powerup();
#endif

	if (NULL != device->driver &&
	    NULL != device->driver->pm &&
	    NULL != device->driver->pm->thaw)
	{
		/* Need to notify Mali driver about this event */
		ret = device->driver->pm->thaw(device);
	}

	return ret;
}

static int mali_os_restore(struct device *device)
{
	MALI_DEBUG_PRINT(4, ("mali_os_thaw() called\n"));
#if MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON8
	mali_dev_restore();
#endif
	return mali_os_resume(device);
}

#ifdef CONFIG_PM_RUNTIME
#if 0
static int mali_runtime_suspend(struct device *device)
{
	int ret = 0;

	MALI_DEBUG_PRINT(4, ("mali_runtime_suspend() called\n"));
	ret = mali_light_suspend(device);

	return ret;
}

static int mali_runtime_resume(struct device *device)
{
	int ret = 0;

	MALI_DEBUG_PRINT(4, ("mali_run	time_resume() called\n"));
	ret = mali_light_resume(device);

	return ret;
}

static int mali_runtime_idle(struct device *device)
{
	MALI_DEBUG_PRINT(4, ("mali_runtime_idle() called\n"));

	if (NULL != device->driver &&
	    NULL != device->driver->pm &&
	    NULL != device->driver->pm->runtime_idle)
	{
		/* Need to notify Mali driver about this event */
		int ret = device->driver->pm->runtime_idle(device);
		if (0 != ret)
		{
			return ret;
		}
	}

	pm_runtime_suspend(device);

	return 0;
}
#endif
#endif

static struct dev_pm_ops mali_gpu_device_type_pm_ops =
{
	.suspend = mali_os_suspend,
	.resume = mali_os_resume,
	.freeze = mali_os_freeze,
	.thaw = mali_os_thaw,
	.restore = mali_os_restore,
#if 0//def CONFIG_PM_RUNTIME
	.runtime_suspend = mali_runtime_suspend,
	.runtime_resume = mali_runtime_resume,
	.runtime_idle = mali_runtime_idle,
#endif
};

struct device_type mali_pm_device =
{
	.pm = &mali_gpu_device_type_pm_ops,
};
