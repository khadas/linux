// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/pm.h>
#include <linux/suspend.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/psci.h>
#include <linux/errno.h>
#include <linux/suspend.h>
#include <asm/suspend.h>
#include <linux/of_address.h>
#include <linux/input.h>
#include <linux/cpuidle.h>
#include <linux/pm_wakeirq.h>
#include <linux/pm_wakeup.h>
#include <linux/interrupt.h>
#include <linux/amlogic/scpi_protocol.h>
#include <asm/cpuidle.h>
#include <uapi/linux/psci.h>
#include <linux/arm-smccc.h>
#include <linux/amlogic/pm.h>
#include <linux/kobject.h>
#include <../kernel/power/power.h>
#include <linux/amlogic/power_domain.h>
#include "vad_power.h"
#include <linux/syscore_ops.h>
#include <linux/time64.h>

#undef pr_fmt
#define pr_fmt(fmt) "gxbb_pm: " fmt

#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND

static DEFINE_MUTEX(early_suspend_lock);
static DEFINE_MUTEX(sysfs_trigger_lock);
static LIST_HEAD(early_suspend_handlers);

/* In order to handle legacy early_suspend driver,
 * here we export sysfs interface
 * for user space to write /sys/power/early_suspend_trigger to trigger
 * early_suspend/late resume call back. If user space do not trigger
 * early_suspend/late_resume, this op will be done
 * by PM_SUSPEND_PREPARE notify.
 */
unsigned int sysfs_trigger;
unsigned int early_suspend_state;
bool is_clr_exit_reg;
/*
 * Avoid run early_suspend/late_resume repeatly.
 */
unsigned int already_early_suspend;

/*
 * If vrtc device is supported
 */
static bool vrtc_validation;

static pm_private_send_data_fn_t pm_send_data_fn;

int pm_set_private_send_data_callback(pm_private_send_data_fn_t fn)
{
	if (!fn) {
		pr_err("%s:%d, Pass invalid parameter.\n", __func__, __LINE__);
		return -EINVAL;
	}

	pm_send_data_fn = fn;

	return 0;
}
EXPORT_SYMBOL(pm_set_private_send_data_callback);

void register_early_suspend(struct early_suspend *handler)
{
	struct list_head *pos;

	mutex_lock(&early_suspend_lock);
	list_for_each(pos, &early_suspend_handlers) {
		struct early_suspend *e;

		e = list_entry(pos, struct early_suspend, link);
		if (e->level > handler->level)
			break;
	}
	list_add_tail(&handler->link, pos);
	mutex_unlock(&early_suspend_lock);
}
EXPORT_SYMBOL(register_early_suspend);

void unregister_early_suspend(struct early_suspend *handler)
{
	mutex_lock(&early_suspend_lock);
	list_del(&handler->link);
	mutex_unlock(&early_suspend_lock);
}
EXPORT_SYMBOL(unregister_early_suspend);

static inline void early_suspend(void)
{
	struct early_suspend *pos;

	mutex_lock(&early_suspend_lock);

	if (!already_early_suspend)
		already_early_suspend = 1;
	else
		goto end_early_suspend;

	pr_info("%s: call handlers\n", __func__);
	list_for_each_entry(pos, &early_suspend_handlers, link)
		if (pos->suspend) {
			pr_info("%s: %ps\n", __func__, pos->suspend);
			pos->suspend(pos);
		}

	pr_info("%s: done\n", __func__);

end_early_suspend:
	mutex_unlock(&early_suspend_lock);
}

static inline void late_resume(void)
{
	struct early_suspend *pos;

	mutex_lock(&early_suspend_lock);

	if (already_early_suspend)
		already_early_suspend = 0;
	else
		goto end_late_resume;

	pr_info("%s: call handlers\n", __func__);
	list_for_each_entry_reverse(pos, &early_suspend_handlers, link)
		if (pos->resume) {
			pr_info("%s: %ps\n", __func__, pos->resume);
			pos->resume(pos);
		}

	pr_info("%s: done\n", __func__);

end_late_resume:
	mutex_unlock(&early_suspend_lock);
}

static ssize_t early_suspend_trigger_show(struct device *dev,
			    struct device_attribute *attr,
			    char *buf)
{
	unsigned int len;

	len = sprintf(buf, "%d\n", early_suspend_state);

	return len;
}

static ssize_t early_suspend_trigger_store(struct device *dev,
			     struct device_attribute *attr,
			     const char *buf, size_t count)
{
	int ret;

	ret = kstrtouint(buf, 0, &early_suspend_state);
	pr_info("early_suspend_state=%d\n", early_suspend_state);

	if (ret)
		return -EINVAL;

	mutex_lock(&sysfs_trigger_lock);
	sysfs_trigger = 1;

	if (early_suspend_state == 0)
		late_resume();
	else if (early_suspend_state == 1)
		early_suspend();
	mutex_unlock(&sysfs_trigger_lock);

	return count;
}

static DEVICE_ATTR_RW(early_suspend_trigger);

void lgcy_early_suspend(void)
{
	mutex_lock(&sysfs_trigger_lock);

	if (!sysfs_trigger)
		early_suspend();

	mutex_unlock(&sysfs_trigger_lock);
}

void lgcy_late_resume(void)
{
	mutex_lock(&sysfs_trigger_lock);

	if (!sysfs_trigger)
		late_resume();

	mutex_unlock(&sysfs_trigger_lock);
}

static int lgcy_early_suspend_notify(struct notifier_block *nb,
				     unsigned long event, void *dummy)
{
	if (event == PM_SUSPEND_PREPARE)
		lgcy_early_suspend();

	if (event == PM_POST_SUSPEND)
		lgcy_late_resume();

	return NOTIFY_OK;
}

static struct notifier_block lgcy_early_suspend_notifier = {
	.notifier_call = lgcy_early_suspend_notify,
};

unsigned int lgcy_early_suspend_exit(struct platform_device *pdev)
{
	int ret;

	device_remove_file(&pdev->dev, &dev_attr_early_suspend_trigger);

	ret = unregister_pm_notifier(&lgcy_early_suspend_notifier);
	return ret;
}

#endif /*CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND*/

typedef unsigned long (psci_fn)(unsigned long, unsigned long,
				unsigned long, unsigned long);

static unsigned long __invoke_psci_fn_smc(unsigned long function_id,
					  unsigned long arg0,
					  unsigned long arg1,
					  unsigned long arg2)
{
	struct arm_smccc_res res;

	arm_smccc_smc(function_id, arg0, arg1, arg2, 0, 0, 0, 0, &res);
	return res.a0;
}

static void __iomem *debug_reg;
static void __iomem *exit_reg;
static suspend_state_t pm_state;
static unsigned int suspend_reason;
static unsigned int resume_reason;
static unsigned int suspend_time_out;
static unsigned int shutdown_time_out;
static unsigned int shutdown_set_time;

/*
 * check_vrtc_device check whether there is vrtc exist.
 */
static bool check_vrtc_device(struct device *dev)
{
	return of_property_read_bool(dev->of_node, "vrtc_invalid");
}

/*
 * get_resume_reason always return last resume reason.
 */
unsigned int get_resume_reason(void)
{
	unsigned int val = 0;

	/*For tm2 SoC, we need use scpi to get this reason*/
	if (exit_reg)
		val = readl_relaxed(exit_reg) & 0xf;
	return val;
}
EXPORT_SYMBOL_GPL(get_resume_reason);

/*
 * get_resume_method return last resume reason.
 * It can be cleared by clr_resume_method().
 */
unsigned int get_resume_method(void)
{
	return resume_reason;
}
EXPORT_SYMBOL_GPL(get_resume_method);

static void set_resume_method(unsigned int val)
{
	resume_reason = val;
}

static int clr_suspend_notify(struct notifier_block *nb,
				     unsigned long event, void *dummy)
{
	if (event == PM_SUSPEND_PREPARE)
		set_resume_method(UDEFINED_WAKEUP);

	return NOTIFY_OK;
}

static struct notifier_block clr_suspend_notifier = {
	.notifier_call = clr_suspend_notify,
};

static int frz_begin(void)
{
	pm_state = PM_SUSPEND_TO_IDLE;
	return 0;
}

static void frz_end(void)
{
	pm_state = PM_SUSPEND_ON;
}

#define FREEZE_ENTRY 0x01
#define FREEZE_EXIT	0x02
static int frz_prepare_late(void)
{
	u32 data;

	data = FREEZE_ENTRY;
	if (pm_send_data_fn)
		pm_send_data_fn((void *)&data, sizeof(data), SCPI_AOCPU,
			SCPI_CMD_PM_FREEZE, NULL, 0);
	return 0;
}

static void frz_restore_early(void)
{
	u32 data;

	data = FREEZE_EXIT;
	if (pm_send_data_fn)
		pm_send_data_fn((void *)&data, sizeof(data), SCPI_AOCPU,
			SCPI_CMD_PM_FREEZE, NULL, 0);
}

static const struct platform_s2idle_ops meson_gx_frz_ops = {
	.begin = frz_begin,
	.end = frz_end,
	.prepare_late = frz_prepare_late,
	.restore_early = frz_restore_early,
};

unsigned int is_pm_s2idle_mode(void)
{
	if (pm_state == PM_SUSPEND_TO_IDLE)
		return 1;
	else
		return 0;
}
EXPORT_SYMBOL_GPL(is_pm_s2idle_mode);

/*Call it as suspend_reason because of historical reasons. */
/*Actually, we should call it wakeup_reason.               */
ssize_t suspend_reason_show(struct device *dev,
			    struct device_attribute *attr,
			    char *buf)
{
	unsigned int len;

	suspend_reason = get_resume_reason();
	len = sprintf(buf, "%d\n", suspend_reason);

	return len;
}

ssize_t suspend_reason_store(struct device *dev,
			     struct device_attribute *attr,
			     const char *buf, size_t count)
{
	int ret;

	ret = kstrtouint(buf, 0, &suspend_reason);

	switch (ret) {
	case 1:
		__invoke_psci_fn_smc(0x82000042, suspend_reason, 0, 0);
		break;
	default:
		return -EINVAL;
	}
	return count;
}

static DEVICE_ATTR_RW(suspend_reason);

ssize_t time_out_show(struct device *dev, struct device_attribute *attr,
		      char *buf)
{
	unsigned int val = 0, len;

	if (!vrtc_validation) {
		pr_err("%s: cannot operate this node!\n", __func__);
		return -EINVAL;
	}

	val = suspend_time_out;
	len = sprintf(buf, "%d\n", val);

	return len;
}

ssize_t time_out_store(struct device *dev, struct device_attribute *attr,
		       const char *buf, size_t count)
{
	unsigned int time_out;
	int ret;

	if (!vrtc_validation) {
		pr_err("%s: cannot operate this node!\n", __func__);
		return -EINVAL;
	}

	ret = kstrtouint(buf, 10, &time_out);
	switch (ret) {
	case 0:
		suspend_time_out = time_out;
		break;
	default:
		return -EINVAL;
	}

	return count;
}

static DEVICE_ATTR_RW(time_out);

ssize_t shutdown_alarm_show(struct device *dev, struct device_attribute *attr,
		   char *buf)
{
	unsigned int len = 0;
	int val;
	struct timespec64 boot_time;

	if (!vrtc_validation) {
		pr_err("%s: cannot operate this node!\n", __func__);
		return -EINVAL;
	}

	if (shutdown_set_time == 0) {
		len = sprintf(buf, "%d\n", shutdown_set_time);
		return len;
	}

	ktime_get_boottime_ts64(&boot_time);
	val = shutdown_time_out - boot_time.tv_sec;
	if (val > 0)
		len = sprintf(buf, "%d\n", shutdown_set_time);
	else
		pr_info("%s: alarm is expired\n", __func__);

	return len;
}

ssize_t shutdown_alarm_store(struct device *dev, struct device_attribute *attr,
		    const char *buf, size_t count)
{
	unsigned int time_out;
	int ret;
	struct timespec64 boot_time;

	if (!vrtc_validation) {
		pr_err("%s: cannot operate this node!\n", __func__);
		return -EINVAL;
	}

	ret = kstrtouint(buf, 10, &time_out);
	switch (ret) {
	case 0:
		shutdown_set_time = time_out;
		ktime_get_boottime_ts64(&boot_time);
		shutdown_time_out = time_out + boot_time.tv_sec;
		break;
	default:
		return -EINVAL;
	}

	return count;
}

static DEVICE_ATTR_RW(shutdown_alarm);

static struct attribute *meson_pm_attrs[] = {
	&dev_attr_suspend_reason.attr,
	&dev_attr_time_out.attr,
	&dev_attr_shutdown_alarm.attr,
#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
	&dev_attr_early_suspend_trigger.attr,
#endif
	NULL,
};

ATTRIBUTE_GROUPS(meson_pm);

static struct class meson_pm_class = {
	.name		= "meson_pm",
	.owner		= THIS_MODULE,
	.class_groups = meson_pm_groups,
};

int gx_pm_syscore_suspend(void)
{
	if (suspend_time_out)
		writel_relaxed(suspend_time_out, debug_reg);
	return 0;
}

void gx_pm_syscore_resume(void)
{
	suspend_time_out = 0;
	set_resume_method(get_resume_reason());
}

void gx_pm_syscore_shutdown(void)
{
	int val;
	struct timespec64 boot_time;

	ktime_get_boottime_ts64(&boot_time);
	val = shutdown_time_out - boot_time.tv_sec;
	if (val > 0)
		writel_relaxed(val, debug_reg);
}

static struct syscore_ops gx_pm_syscore_ops = {
	.suspend = gx_pm_syscore_suspend,
	.resume	= gx_pm_syscore_resume,
	.shutdown = gx_pm_syscore_shutdown,
};

static int __init gx_pm_init_ops(void)
{
	register_syscore_ops(&gx_pm_syscore_ops);
	return 0;
}

static void __iomem  *interrupt_clr;

static irqreturn_t pm_wakeup_isr(int irq, void *data __maybe_unused)
{
	pr_info("[PM]Wakeup form bl30\n");

	if (interrupt_clr)
		writel(0x00, interrupt_clr);
	return IRQ_HANDLED;
}

static int meson_pm_probe(struct platform_device *pdev)
{
	unsigned int irq_pwrctrl;
	struct pm_data *p_data;
	int err;
	u32 ret;

	pr_info("enter %s!\n", __func__);

	if (!of_property_read_u32(pdev->dev.of_node,
				  "irq_pwrctrl", &irq_pwrctrl)) {
		pwr_ctrl_irq_set(irq_pwrctrl, 1, 0);
	}

	debug_reg = of_iomap(pdev->dev.of_node, 0);
	if (!debug_reg)
		return -ENOMEM;
	p_data = devm_kzalloc(&pdev->dev, sizeof(struct pm_data), GFP_KERNEL);
	if (!p_data)
		return -ENOMEM;
	p_data->dev = &pdev->dev;
	platform_set_drvdata(pdev, p_data);
	vad_wakeup_power_init(pdev, p_data);
	exit_reg = of_iomap(pdev->dev.of_node, 1);
	if (!exit_reg)
		return -ENOMEM;

	err = class_register(&meson_pm_class);
	if (unlikely(err))
		return err;

	gx_pm_init_ops();

#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
	err = register_pm_notifier(&lgcy_early_suspend_notifier);
	if (unlikely(err))
		return err;
#endif
	if (of_property_read_bool(pdev->dev.of_node, "clr_reboot_mode"))
		is_clr_exit_reg = true;
	else
		is_clr_exit_reg = false;

	s2idle_set_ops(&meson_gx_frz_ops);
	err = register_pm_notifier(&clr_suspend_notifier);
	if (unlikely(err))
		return err;

	vrtc_validation = !check_vrtc_device(&pdev->dev);

	interrupt_clr = of_iomap(pdev->dev.of_node, 2);
	if (!interrupt_clr) {
		dev_warn(&pdev->dev,
				"Read dmc_asr addr fail\n");
	}

	p_data->pm_wakeup_irq = platform_get_irq_byname(pdev, "pm_wakeup");
	if (p_data->pm_wakeup_irq < 0) {
		dev_warn(&pdev->dev, "Failed to get pm_wakeup interrupt source:%d\n",
			p_data->pm_wakeup_irq);
	} else {
		ret = request_irq(p_data->pm_wakeup_irq,
				pm_wakeup_isr, IRQF_SHARED, "pm_wakeup",
				p_data);
		if (ret) {
			dev_err(&pdev->dev, "failed to claim irq_wakeup\n");
			return -ENXIO;
		}

		device_init_wakeup(&pdev->dev, 1);
		dev_pm_set_wake_irq(&pdev->dev, p_data->pm_wakeup_irq);
	}

	pr_info("%s done\n", __func__);
	return 0;
}

int pm_suspend_noirq(struct device *dev)
{
	vad_wakeup_power_suspend(dev);
	return 0;
}

int pm_resume_noirq(struct device *dev)
{
	vad_wakeup_power_resume(dev);
	return 0;
}

static const struct dev_pm_ops meson_pm_noirq_ops = {
	.suspend_noirq = pm_suspend_noirq,
	.resume_noirq = pm_resume_noirq,
};

static int __exit meson_pm_remove(struct platform_device *pdev)
{
	if (debug_reg)
		iounmap(debug_reg);
	if (exit_reg)
		iounmap(exit_reg);
	if (interrupt_clr)
		iounmap(interrupt_clr);
	device_remove_file(&pdev->dev, &dev_attr_suspend_reason);
	device_remove_file(&pdev->dev, &dev_attr_time_out);
	device_remove_file(&pdev->dev, &dev_attr_shutdown_alarm);
#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
	lgcy_early_suspend_exit(pdev);
#endif
	return 0;
}

static const struct of_device_id amlogic_pm_dt_match[] = {
	{.compatible = "amlogic, pm",
	},
	{}
};

static void meson_pm_shutdown(struct platform_device *pdev)
{
	if ((exit_reg) && (is_clr_exit_reg))
		writel_relaxed(0, exit_reg);
}

static struct platform_driver meson_pm_driver = {
	.probe = meson_pm_probe,
	.driver = {
		   .name = "pm-meson",
		   .owner = THIS_MODULE,
		   .of_match_table = amlogic_pm_dt_match,
		   .pm = &meson_pm_noirq_ops,
		   },
	.remove = __exit_p(meson_pm_remove),
	.shutdown = meson_pm_shutdown,
};

module_platform_driver(meson_pm_driver);
MODULE_AUTHOR("Amlogic");
MODULE_DESCRIPTION("Amlogic suspend driver");
MODULE_LICENSE("GPL");
