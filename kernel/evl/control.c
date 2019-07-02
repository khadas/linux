/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2018 Philippe Gerum  <rpm@xenomai.org>
 */

#include <linux/types.h>
#include <linux/mm.h>
#include <linux/sched/isolation.h>
#include <evl/memory.h>
#include <evl/thread.h>
#include <evl/factory.h>
#include <evl/tick.h>
#include <evl/sched.h>
#include <evl/control.h>
#include <asm/evl/syscall.h>
#include <asm/evl/fptest.h>
#include <uapi/evl/control.h>

static BLOCKING_NOTIFIER_HEAD(state_notifier_list);

atomic_t evl_runstate = ATOMIC_INIT(EVL_STATE_WARMUP);
EXPORT_SYMBOL_GPL(evl_runstate);

void evl_add_state_chain(struct notifier_block *nb)
{
	blocking_notifier_chain_register(&state_notifier_list, nb);
}
EXPORT_SYMBOL_GPL(evl_add_state_chain);

void evl_remove_state_chain(struct notifier_block *nb)
{
	blocking_notifier_chain_unregister(&state_notifier_list, nb);
}
EXPORT_SYMBOL_GPL(evl_remove_state_chain);

static inline void call_state_chain(enum evl_run_states newstate)
{
	blocking_notifier_call_chain(&state_notifier_list, newstate, NULL);
}

static int start_services(void)
{
	enum evl_run_states state;
	int ret = 0;

	state = atomic_cmpxchg(&evl_runstate,
			EVL_STATE_STOPPED,
			EVL_STATE_WARMUP);
	switch (state) {
	case EVL_STATE_RUNNING:
		break;
	case EVL_STATE_STOPPED:
		ret = evl_enable_tick();
		if (ret) {
			atomic_set(&evl_runstate, EVL_STATE_STOPPED);
			return ret;
		}
		call_state_chain(EVL_STATE_WARMUP);
		set_evl_state(EVL_STATE_RUNNING);
		printk(EVL_INFO "core started\n");
		break;
	default:
		ret = -EINPROGRESS;
	}

	return ret;
}

static int stop_services(void)
{
	enum evl_run_states state;
	int ret = 0;

	state = atomic_cmpxchg(&evl_runstate,
			EVL_STATE_RUNNING,
			EVL_STATE_TEARDOWN);
	switch (state) {
	case EVL_STATE_STOPPED:
		break;
	case EVL_STATE_RUNNING:
		ret = evl_killall(T_USER);
		if (ret) {
			set_evl_state(state);
			return ret;
		}
		call_state_chain(EVL_STATE_TEARDOWN);
		ret = evl_killall(0);
		evl_disable_tick();
		set_evl_state(EVL_STATE_STOPPED);
		printk(EVL_INFO "core stopped\n");
		break;
	default:
		ret = -EINPROGRESS;
	}

	return ret;
}

#ifdef CONFIG_EVL_SCHED_QUOTA

static int do_quota_control(struct evl_sched_ctlreq *ctl)
{
	union evl_sched_ctlparam param, __user *u_ctlp;
	union evl_sched_ctlinfo info, __user *u_infp;
	int ret;

	u_ctlp = (typeof(u_ctlp))ctl->param;
	ret = raw_copy_from_user(&param.quota, &u_ctlp->quota,
				sizeof(param.quota));
	if (ret)
		return -EFAULT;

	ret = evl_sched_quota.sched_control(ctl->cpu, &param, &info);
	if (ret || ctl->info == NULL)
		return ret;

	u_infp = (typeof(u_infp))ctl->info;
	ret = raw_copy_to_user(&u_infp->quota, &info.quota,
			sizeof(info.quota));
	if (ret)
		return -EFAULT;

	return 0;
}

#else

static int do_quota_control(struct evl_sched_ctlreq *ctl)
{
	return -EINVAL;
}

#endif

#ifdef CONFIG_EVL_SCHED_TP

static int do_tp_control(struct evl_sched_ctlreq *ctl)
{
	union evl_sched_ctlinfo *info = NULL, __user *u_infp;
	union evl_sched_ctlparam param, __user *u_ctlp;
	size_t len;
	int ret;

	u_ctlp = (typeof(u_ctlp))ctl->param;
	ret = raw_copy_from_user(&param.tp, &u_ctlp->tp, sizeof(param.tp));
	if (ret)
		return -EFAULT;

	if (ctl->info) {
		/* Quick check to prevent creepy memalloc. */
		if (param.tp.nr_windows > CONFIG_EVL_SCHED_TP_NR_PART)
			return -EINVAL;

		len = evl_tp_paramlen(&param.tp);
		info = evl_alloc(len);
		if (info == NULL)
			return -ENOMEM;
	}

	ret = evl_sched_tp.sched_control(ctl->cpu, &param, info);
	if (ret || info == NULL)
		goto out;

	u_infp = (typeof(u_infp))ctl->info;
	len = evl_tp_paramlen(&info->tp);
	ret = raw_copy_to_user(&u_infp->tp, &info->tp, len);
	if (ret)
		ret = -EFAULT;
out:
	if (info)
		evl_free(info);

	return ret;
}

#else

static int do_tp_control(struct evl_sched_ctlreq *ctl)
{
	return -EINVAL;
}

#endif

static int do_sched_control(struct evl_sched_ctlreq *ctl)
{
	int ret;

	switch (ctl->policy) {
	case SCHED_QUOTA:
		ret = do_quota_control(ctl);
		break;
	case SCHED_TP:
		ret = do_tp_control(ctl);
		break;
	default:
		return -EINVAL;
	}

	return ret;
}

static int do_cpu_state(struct evl_cpu_state *cpst)
{
	int cpu = cpst->cpu;
	__u32 state = 0;

	if (cpst->cpu >= num_possible_cpus() || !cpu_present(cpu))
		return -EINVAL;

	if (!cpu_online(cpu))
		state |= EVL_CPU_OFFLINE;

	if (is_evl_cpu(cpu))
		state |= EVL_CPU_OOB;

	if (!housekeeping_cpu(cpu, HK_FLAG_DOMAIN))
		state |= EVL_CPU_ISOL;

	return raw_copy_to_user(cpst->state, &state, sizeof(state)) ?
		-EFAULT : 0;
}

static long control_common_ioctl(struct file *filp, unsigned int cmd,
			unsigned long arg)
{
	struct evl_sched_ctlreq ctl, __user *u_ctl;
	struct evl_cpu_state cpst, __user *u_cpst;
	long ret;

	switch (cmd) {
	case EVL_CTLIOC_SCHEDCTL:
		u_ctl = (typeof(u_ctl))arg;
		ret = raw_copy_from_user(&ctl, u_ctl, sizeof(ctl));
		if (ret)
			return -EFAULT;
		ret = do_sched_control(&ctl);
		break;
	case EVL_CTLIOC_GET_CPUSTATE:
		u_cpst = (typeof(u_cpst))arg;
		ret = raw_copy_from_user(&cpst, u_cpst, sizeof(cpst));
		if (ret)
			return -EFAULT;
		ret = do_cpu_state(&cpst);
		break;
	default:
		ret = -ENOTTY;
	}

	return ret;
}

static long control_oob_ioctl(struct file *filp, unsigned int cmd,
			unsigned long arg)
{
	return control_common_ioctl(filp, cmd, arg);
}

static long control_ioctl(struct file *filp, unsigned int cmd,
			unsigned long arg)
{
	struct evl_core_info info;
	long ret;

	switch (cmd) {
	case EVL_CTLIOC_GET_COREINFO:
		info.abi_level = EVL_ABI_LEVEL;
		info.fpu_features = evl_detect_fpu();
		info.shm_size = evl_shm_size;
		ret = raw_copy_to_user((struct evl_core_info __user *)arg,
				&info, sizeof(info)) ? -EFAULT : 0;
		break;
	default:
		ret = control_common_ioctl(filp, cmd, arg);
	}

	return ret;
}

static int control_mmap(struct file *filp, struct vm_area_struct *vma)
{
	void *p = evl_get_heap_base(&evl_shared_heap);
	unsigned long pfn = __pa(p) >> PAGE_SHIFT;
	size_t len = vma->vm_end - vma->vm_start;

	if (len != evl_shm_size)
		return -EINVAL;

	return remap_pfn_range(vma, vma->vm_start, pfn, len, PAGE_SHARED);
}

static const struct file_operations control_fops = {
	.oob_ioctl	=	control_oob_ioctl,
	.unlocked_ioctl	=	control_ioctl,
	.mmap		=	control_mmap,
};

static const char *state_labels[] = {
	[EVL_STATE_DISABLED] = "disabled",
	[EVL_STATE_RUNNING] = "running",
	[EVL_STATE_STOPPED] = "stopped",
	[EVL_STATE_TEARDOWN] = "teardown",
	[EVL_STATE_WARMUP] = "warmup",
};

static ssize_t state_show(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	int st = atomic_read(&evl_runstate);

	return snprintf(buf, PAGE_SIZE, "%s\n", state_labels[st]);
}

static ssize_t state_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	size_t len = count;

	if (len && buf[len - 1] == '\n')
		len--;

	if (!strncmp(buf, "start", len))
		return start_services() ?: count;

	if (!strncmp(buf, "stop", len))
		return stop_services() ?: count;

	return -EINVAL;
}
static DEVICE_ATTR_RW(state);

static ssize_t abi_show(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", EVL_ABI_LEVEL);
}
static DEVICE_ATTR_RO(abi);

static ssize_t cpus_show(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%*pbl\n",
			cpumask_pr_args(&evl_oob_cpus));
}
static DEVICE_ATTR_RO(cpus);

#ifdef CONFIG_EVL_SCHED_QUOTA

static ssize_t quota_show(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%Lu\n",
			ktime_to_ns(evl_get_quota_period()));
}

static ssize_t quota_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	unsigned long long period;
	int ret;

	ret = kstrtoull(buf, 10, &period);
	if (ret < 0)
		return -EINVAL;

	/*
	 * If the quota period is shorter than the monotonic clock
	 * gravity for user-targeted timers, assume PEBKAC.
	 */
	if (period < evl_get_clock_gravity(&evl_mono_clock, user))
		return -EINVAL;

	evl_set_quota_period(ns_to_ktime(period));

	return count;
}
static DEVICE_ATTR_RW(quota);

#endif

#ifdef CONFIG_EVL_SCHED_TP

static ssize_t tp_show(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", CONFIG_EVL_SCHED_TP_NR_PART);
}
static DEVICE_ATTR_RO(tp);

#endif

static struct attribute *control_attrs[] = {
	&dev_attr_state.attr,
	&dev_attr_abi.attr,
	&dev_attr_cpus.attr,
#ifdef CONFIG_EVL_SCHED_QUOTA
	&dev_attr_quota.attr,
#endif
#ifdef CONFIG_EVL_SCHED_TP
	&dev_attr_tp.attr,
#endif
	NULL,
};
ATTRIBUTE_GROUPS(control);

struct evl_factory evl_control_factory = {
	.name	=	"control",
	.fops	=	&control_fops,
	.attrs	=	control_groups,
	.flags	=	EVL_FACTORY_SINGLE,
};
