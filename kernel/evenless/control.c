/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2018 Philippe Gerum  <rpm@xenomai.org>
 */

#include <linux/types.h>
#include <linux/mm.h>
#include <evenless/memory.h>
#include <evenless/thread.h>
#include <evenless/factory.h>
#include <evenless/tick.h>
#include <evenless/control.h>
#include <asm/evenless/syscall.h>
#include <asm/evenless/fptest.h>
#include <uapi/evenless/control.h>

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

static long control_ioctl(struct file *filp, unsigned int cmd,
			unsigned long arg)
{
	struct evl_core_info info;
	long ret;

	/*
	 * NOTE: OOB <-> in-band switching services can only apply to
	 * the current thread, which should not need a file descriptor
	 * on its own element for issuing them. The control device is
	 * the right channel for requesting such services.
	 */

	switch (cmd) {
	case EVL_CTLIOC_GET_COREINFO:
		info.abi_level = EVL_ABI_LEVEL;
		info.fpu_features = evl_detect_fpu();
		info.shm_size = evl_shm_size;
		ret = raw_copy_to_user((struct evl_core_info __user *)arg,
				&info, sizeof(info)) ? -EFAULT : 0;
		break;
	case EVL_CTLIOC_SWITCH_OOB:
		ret = evl_switch_oob();
		break;
	case EVL_CTLIOC_SWITCH_INBAND:
		/*
		 * We already switched an OOB caller to inband mode as
		 * a result of handling this ioctl() call. Yummie.
		 */
		ret = 0;
		break;
	case EVL_CTLIOC_DETACH_SELF:
		ret = evl_detach_self();
		break;
	default:
		ret = -ENOTTY;
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

static struct attribute *control_attrs[] = {
	&dev_attr_state.attr,
	&dev_attr_abi.attr,
	NULL,
};
ATTRIBUTE_GROUPS(control);

struct evl_factory evl_control_factory = {
	.name	=	"control",
	.fops	=	&control_fops,
	.attrs	=	control_groups,
	.flags	=	EVL_FACTORY_SINGLE,
};
