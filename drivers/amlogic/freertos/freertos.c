// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#define pr_fmt(fmt)	"freertos: " fmt
//#define DEBUG

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sysfs.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/printk.h>
#include <linux/string.h>
#include <linux/arm-smccc.h>
#include <linux/psci.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>
#include <uapi/linux/psci.h>
#include <linux/debugfs.h>
#include <linux/sched/signal.h>
#include <linux/of_reserved_mem.h>
#include <linux/amlogic/freertos.h>
#include <linux/mm.h>
#include <linux/sched/clock.h>

#define AML_RTOS_NAME "freertos"

#define SIP_FREERTOS_FID		0x8200004B
#define RTOS_REQUEST_INFO		0
#define RTOS_ALLOW_COREUP		1
#define SMC_UNK				-1
#define RSVED_MAX_IRQ			1024
#define RTOS_RUN_FLAG			0xA5A5A5A5

#define LOGBUF_RDFLG	0x80000000
struct logbuf_t {
	u32 write;
	u32 read;
	char buf[0];
};

static struct reserved_mem res_mem;
static unsigned long rtosinfo_phy;
static struct xrtosinfo_t *rtosinfo;
static int freertos_finished;
static DEFINE_MUTEX(freertos_lock);
static DEFINE_SPINLOCK(freertos_chk_lock);
static int ipi_rcv, freertos_disabled;
static DECLARE_BITMAP(freertos_irqrsv_mask, RSVED_MAX_IRQ);
static int freertos_irqrsv_inited;
static struct dentry *rtos_debug_dir;
static u32 logbuf_len;
static struct logbuf_t *logbuf;
static struct dentry *rtos_logbuf_file;
static DEFINE_MUTEX(freertos_logbuf_lock);

static BLOCKING_NOTIFIER_HEAD(rtos_nofitier_chain);

static int aml_rtos_logbuf_save(void);

static unsigned long long rtos_time_limit = 5000000000;
static unsigned long rtos_log_save;
static int rtos_param(char *buf)
{
	if (!buf)
		return -EINVAL;

	if (kstrtoul(buf, 0, &rtos_log_save)) {
		pr_info("invalid input:%s\n", buf);
		return 0;
	}

	pr_info("%s, buf:%s, rtos_log_save:%ld\n", __func__, buf, rtos_log_save);

	return 0;
}
__setup("rtos_log_save=", rtos_param);

int register_freertos_notifier(struct notifier_block *nb)
{
	int err;

	err = blocking_notifier_chain_register(&rtos_nofitier_chain, nb);
	return err;
}
EXPORT_SYMBOL_GPL(register_freertos_notifier);

int unregister_freertos_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&rtos_nofitier_chain, nb);
}
EXPORT_SYMBOL(unregister_freertos_notifier);

static int call_freertos_notifiers(unsigned long val, void *v)
{
	return blocking_notifier_call_chain(&rtos_nofitier_chain, val, v);
}

static unsigned long freertos_request_info(void)
{
	struct device_node *rtos_reserved, *freertos;
	struct reserved_mem *rmem;
	unsigned int info_offset;
	unsigned long info_base;
	int r = 0;

	rtos_reserved = of_find_compatible_node(NULL, NULL, "amlogic, aml_rtos_memory");
	if (!rtos_reserved) {
		pr_err("rtos_reserved device node find error\n");
		return 0;
	}

	rmem = of_reserved_mem_lookup(rtos_reserved);
	if (!rmem) {
		pr_err("%s no such reserved mem\n", (char *)rtos_reserved->name);
		return 0;
	}

	if (!rmem->base || !rmem->size) {
		pr_err("%s unexpected reserved memory\n", (char *)rtos_reserved->name);
		return 0;
	}
	info_base = rmem->base;
	res_mem.base = rmem->base;
	res_mem.size = rmem->size;

	freertos = of_find_compatible_node(NULL, NULL, "amlogic,freertos");
	if (!freertos) {
		pr_err("freertos device node find error\n");
		return 0;
	}
	r = of_property_read_u32(freertos, "info_offset", &info_offset);
	if (r < 0) {
		pr_err("rtos info offset find error\n");
		return 0;
	}
	pr_debug("rtos base:%lx, info offset:%x\n", info_base, info_offset);

	return (info_base + info_offset);
}

unsigned int freertos_is_run(void)
{
	unsigned long phy;
	struct xrtosinfo_t *info;

	phy = freertos_request_info();
	if (!phy)
		return 0;

	info = (struct xrtosinfo_t *)memremap(phy, sizeof(struct xrtosinfo_t), MEMREMAP_WB);
	if (info) {
		if (info->rtos_run_flag != RTOS_RUN_FLAG) {
			pr_debug("rtos_run_flag:%x\n", info->rtos_run_flag);
			return 0;
		}
	} else {
		pr_err("%s,map freertos info failed\n", __func__);
		return 0;
	}
	iounmap(info);
	return 1;
}
EXPORT_SYMBOL(freertos_is_run);

/*T7 need not update info to bl31
 * static unsigned long freertos_allow_coreup(void)
 * {
 *	struct arm_smccc_res res;
 *
 *	arm_smccc_smc(SIP_FREERTOS_FID, RTOS_ALLOW_COREUP,
 *		      0, 0, 0, 0, 0, 0, &res);
 *	return res.a0;
 * }
 */

//////////temporary walk around method///////////
#include <linux/delay.h>
#include <uapi/linux/psci.h>

#ifdef CONFIG_64BIT
#define PSCI_AFFINITY_INFO PSCI_0_2_FN64_AFFINITY_INFO
#else
#define PSCI_AFFINITY_INFO PSCI_0_2_FN_AFFINITY_INFO
#endif

int meson_psci_affinity_info(unsigned int cpu)
{
	struct arm_smccc_res res;

	arm_smccc_smc(PSCI_AFFINITY_INFO, cpu, 0, 0, 0, 0, 0, 0, &res);
	return res.a0;
}

/*
 *Can't shutdown C1 now
 *int meson_psci_sys_power_off(void)
 *{
 *	struct arm_smccc_res res;
 *
 *	arm_smccc_smc(PSCI_0_2_FN_SYSTEM_OFF, 0, 0, 0, 0, 0, 0, 0, &res);
 *	return res.a0;
 *}
 */

int meson_do_core_off(unsigned int cpu)
{
	int i, err;

	for (i = 0; i < 10; i++) {
		err = meson_psci_affinity_info(cpu);
		if (err == PSCI_0_2_AFFINITY_LEVEL_OFF) {
			pr_debug("CPU %d killed.\n", cpu);
			return 0;
		}

		usleep_range(10000, 15000);
		pr_info("Retrying again to check for CPU kill\n");
	}

	pr_warn("CPU%d may not have shut down cleanly (err: %d)\n",
		cpu, err);
	return -ETIMEDOUT;
}

////////////////////////////////////////////

static int freertos_coreup_prepare(int cpu, int bootup)
{
	if (meson_do_core_off(cpu)) {
		pr_err("cpu %u power off fail\n", cpu);
		return -1;
	}
	pr_info("cpu %u power off success\n", cpu);
	/* freertos_allow_coreup(); */
	if (bootup)
		usleep_range(10000, 15000);
	return 0;
}

static void freertos_coreup(int cpu, int bootup)
{
	if (bootup) {
		pr_debug("cpu %u power on start\n", cpu);
		if (!device_online(get_cpu_device(cpu)))
			pr_info("cpu %u power on success\n", cpu);
		else
			pr_err("cpu %u power on failed\n", cpu);
	}
}

static void freertos_do_finish(int bootup)
{
	unsigned int cpu;

	mutex_lock(&freertos_lock);
	if (!freertos_finished && rtosinfo &&
	    rtosinfo->status == ertosstat_done) {
		/*if (rtosinfo->flags & RTOSINFO_FLG_SHUTDOWN) {
		 *	pr_info("begin system shutdown\n");
		 *	meson_psci_sys_power_off();
		 *	pr_info("system shutdown fail\n");
		 *	goto done;
		 *}
		 */
		for_each_present_cpu(cpu) {
			if (rtosinfo->cpumask & (1 << cpu)) {
				if (!cpu_online(cpu)) {
					pr_info("cpu %u finish\n", cpu);
					if (freertos_coreup_prepare(cpu, bootup) < 0)
						continue;
					aml_rtos_logbuf_save();
					freertos_coreup(cpu, bootup);
				} else {
					pr_info("cpu %u already take over\n", cpu);
				}

				free_reserved_area(__va(res_mem.base),
					__va(PAGE_ALIGN(res_mem.base + res_mem.size)),
						   0,
						   "free_mem");
				call_freertos_notifiers(1, NULL);
				break;
			}
		}
//done:
		freertos_finished = 1;
	}
	mutex_unlock(&freertos_lock);
}

static void freertos_do_work(struct work_struct *work)
{
	freertos_do_finish(1);
}

static DECLARE_WORK(freertos_work, freertos_do_work);

int freertos_finish(void)
{
	int chk = 0;
	unsigned long flg;
	unsigned int cpu;

	spin_lock_irqsave(&freertos_chk_lock, flg);
	cpu = smp_processor_id();
	if (cpu == 0 && !ipi_rcv && rtosinfo && !freertos_finished &&
	    rtosinfo->status == ertosstat_done) {
		pr_info("ipi received\n");
		for_each_present_cpu(cpu) {
			if (rtosinfo->cpumask & (1 << cpu)) {
				if (cpu_online(cpu))
					pr_info("cpu %u already take over\n", cpu);
				else
					chk = 1;
				break;
			}
		}
		ipi_rcv = 1;
	}
	spin_unlock_irqrestore(&freertos_chk_lock, flg);

	if (chk) {
		schedule_work(&freertos_work);
		return 0;
	} else {
		return -1;
	}
}
EXPORT_SYMBOL(freertos_finish);

int freertos_is_finished(void)
{
	return freertos_finished;
}
EXPORT_SYMBOL(freertos_is_finished);

struct xrtosinfo_t *freertos_get_info(void)
{
	return rtosinfo;
}
EXPORT_SYMBOL(freertos_get_info);

static int __init disable_freertos(char *str)
{
	freertos_disabled = 1;
	return 0;
}

early_param("disable_freertos", disable_freertos);

static inline void _logbuf_begin_get_log(void)
{
	u32 rd1, rd2;

	do {
		rd1 = READ_ONCE(logbuf->read);
		rd2 = (rd1 | LOGBUF_RDFLG);
	} while (rd1 != cmpxchg(&logbuf->read, rd1, rd2));
}

static inline void _logbuf_post_get_log(int len, int last)
{
	u32 rd1, rd2;

	do {
		rd1 = READ_ONCE(logbuf->read);
		rd2 = (rd1 & ~LOGBUF_RDFLG);
		rd2 += len;
		if (rd2 >= logbuf_len)
			rd2 -= logbuf_len;
		if (last == 0)
			rd2 |= (rd1 & LOGBUF_RDFLG);
	} while (rd1 != cmpxchg(&logbuf->read, rd1, rd2));
}

static inline int _logbuf_get_log(const char **pbuf)
{
	u32 rd1, rd2, wt;
	int len;

	rd1 = READ_ONCE(logbuf->read);
	wt = READ_ONCE(logbuf->write);
	rd2 = (rd1 & ~LOGBUF_RDFLG);
	if (rd2 <= wt)
		len = wt - rd2;
	else
		len = logbuf_len - rd2;
	if (pbuf)
		*pbuf = (const char *)logbuf->buf + rd2;
	return len;
}

static ssize_t logbuf_read(struct file *file, char __user *buf,
			   size_t size, loff_t *ppos)
{
	const char *p;
	int cnt = 0, err;

	while (1) {
		cnt = _logbuf_get_log(&p);
		if (cnt > 0 || signal_pending(current))
			break;
		usleep_range(1000, 1500);
	}
	if (cnt > size)
		cnt = size;
	if (cnt <= 0)
		return 0;
	err = copy_to_user(buf, p, cnt);
	if (err)
		return -1;
	*ppos += cnt;
	_logbuf_post_get_log(cnt, 0);
	return cnt;
}

static int logbuf_open(struct inode *inode, struct file *file)
{
	int ret;

	ret = mutex_lock_interruptible(&freertos_logbuf_lock);
	if (ret < 0)
		return ret;
	_logbuf_begin_get_log();
	return 0;
}

static int logbuf_release(struct inode *inode, struct file *file)
{
	_logbuf_post_get_log(0, 1);
	mutex_unlock(&freertos_logbuf_lock);
	return 0;
}

static const struct file_operations logbuf_fops = {
	.open		= logbuf_open,
	.read		= logbuf_read,
	.write		= NULL,
	.llseek		= NULL,
	.release	= logbuf_release,
};

static int aml_rtos_logbuf_save(void)
{
	struct logbuf_t *tmpbuf;

	if (!rtos_log_save)
		pr_info("parm rtos_log_save not set\n");

	pr_info("start to save logbuf\n");
	if (!rtos_debug_dir) {
		pr_err("rtos logbuf dir not exit\n");
		return -1;
	}

	if (rtosinfo->logbuf_len <= sizeof(struct logbuf_t)) {
		pr_err("rtos logbuf len is error\n");
		return -1;
	}

	tmpbuf = vmalloc(rtosinfo->logbuf_len);
	pr_info("tmpbuf addr: %p\n", tmpbuf);
	pr_info("logbuf addr: %p\n", logbuf);
	if (logbuf) {
		pr_info("logbuf will save to tmp buf\n");
		memcpy(tmpbuf, logbuf, rtosinfo->logbuf_len);
		logbuf = tmpbuf;
	} else {
		pr_err("logbuf addr error\n");
		pr_info("ori read %d, write %d, tmp read %d write %d\n",
			logbuf->read, logbuf->write,
			tmpbuf->read, tmpbuf->write);
	}

	return 0;
}

static int aml_rtos_logbuf_init(void)
{
	phys_addr_t phy;
	size_t size;

	if (!rtos_debug_dir)
		return -1;
	if (rtosinfo->logbuf_len <= sizeof(struct logbuf_t))
		return -1;
	phy = (phys_addr_t)rtosinfo->logbuf_phy;
	size = rtosinfo->logbuf_len;
	pr_info("logbuffer: 0x%x, 0x%x\n",
		rtosinfo->logbuf_phy, rtosinfo->logbuf_len);
	logbuf = memremap(phy, size, MEMREMAP_WB);
	if (!logbuf) {
		pr_err("map log buffer failed\n");
		return -1;
	}
	logbuf_len = (u32)(size - sizeof(struct logbuf_t));
	rtos_logbuf_file = debugfs_create_file("logbuf", S_IFREG | 0440,
					       rtos_debug_dir, NULL,
					       &logbuf_fops);
	return 0;
}

static void aml_rtos_logbuf_deinit(void)
{
	debugfs_remove(rtos_logbuf_file);
	rtos_logbuf_file = NULL;

	if (logbuf) {
		iounmap(logbuf);
		logbuf = NULL;
	}
}

static ssize_t android_status_store(struct class *cla,
			  struct class_attribute *attr,
			  const char *buf, size_t count)
{
	long val = 0;

	if (kstrtoul(buf, 0, &val)) {
		pr_err("invalid input:%s\n", buf);
		return count;
	}

	pr_info("set android_status is %ld\n", val);
	rtosinfo->android_status = val;

	return count;
}

static ssize_t android_status_show(struct class *cla,
			 struct class_attribute *attr, char *buf)
{
	int cnt = 0;

	cnt =  sprintf(buf, "%d\n", rtosinfo->android_status);

	return cnt;
}
static CLASS_ATTR_RW(android_status);

static ssize_t ipi_send_store(struct class *cla,
			  struct class_attribute *attr,
			  const char *buf, size_t count)
{
	long cpu = 0;

	if (kstrtoul(buf, 0, &cpu)) {
		pr_err("invalid input cpu number:%s\n", buf);
		return count;
	}

	pr_info("set ipi to cpu%ld\n", cpu);
	arch_send_ipi_rtos(cpu);

	return count;
}
static CLASS_ATTR_WO(ipi_send);

static ssize_t time_limit_store(struct class *cla, struct class_attribute *attr,
				const char *buf, size_t count)
{
	unsigned long val = 0;

	if (kstrtoul(buf, 0, &val)) {
		pr_err("invalid input:%s\n", buf);
		return count;
	}
	pr_info("set time limit is %ld ns\n", val);
	rtos_time_limit = val;
	return count;
}

static ssize_t time_limit_show(struct class *cla, struct class_attribute *attr,
			       char *buf)
{
	int cnt = 0;

	cnt = sprintf(buf, "time limit value :%lld ns\n", rtos_time_limit);
	return cnt;
}
static CLASS_ATTR_RW(time_limit);
static struct attribute *freertos_attrs[] = {
	&class_attr_android_status.attr,
	&class_attr_ipi_send.attr,
	&class_attr_time_limit.attr,
	NULL
};
ATTRIBUTE_GROUPS(freertos);

static struct class freertos_class = {
	.name = "freertos",
	.class_groups = freertos_groups,
};

static int aml_rtos_probe(struct platform_device *pdev)
{
	rtos_debug_dir = debugfs_create_dir("freertos", NULL);
	rtosinfo_phy = freertos_request_info();
	pr_info("rtosinfo_phy=%lx\n", rtosinfo_phy);
	if (rtosinfo_phy == 0 || (int)rtosinfo_phy == SMC_UNK)
		return 0;

	rtosinfo = (struct xrtosinfo_t *)memremap(rtosinfo_phy,
						  sizeof(struct xrtosinfo_t),
						  MEMREMAP_WB);
	if (rtosinfo) {
		if (rtosinfo->rtos_run_flag == RTOS_RUN_FLAG) {
			if (class_register(&freertos_class)) {
				pr_err("regist freertos_class failed\n");
				return -EINVAL;
			}
		}
		aml_rtos_logbuf_init();
		freertos_do_finish(1);
	} else {
		pr_err("map freertos info failed\n");
	}

	return 0;
}

static int __exit aml_rtos_remove(struct platform_device *pdev)
{
	aml_rtos_logbuf_deinit();
	if (rtosinfo)
		iounmap(rtosinfo);

	debugfs_remove(rtos_debug_dir);
	return 0;
}

static void aml_rtos_shutdown(struct platform_device *pdev)
{
	unsigned long long tick_s = 0, tick_e = 0;

	pr_emerg("rtos shutdown in\n");
	if (!freertos_finished) {
		pr_emerg("wait rtos finished\n");
		arch_send_ipi_rtos(1);
		tick_s = sched_clock();
		while (!freertos_finished) {
			tick_e = sched_clock() - tick_s;
			if (tick_e > rtos_time_limit)
				panic("rtos finished time out\n");
			usleep_range(500, 1000);
		}
		tick_e = sched_clock() - tick_s;
	}
	pr_emerg("rtos shutdown out, time %lld ns\n", tick_e);
}

static const struct of_device_id aml_freertos_dt_match[] = {
	{
		.compatible = "amlogic,freertos",
	},
	{},
};

static struct platform_driver aml_rtos_driver = {
	.driver = {
		.name = AML_RTOS_NAME,
		.owner = THIS_MODULE,
		.of_match_table = aml_freertos_dt_match,
	},
	.probe = aml_rtos_probe,
	.remove = __exit_p(aml_rtos_remove),
	.shutdown = aml_rtos_shutdown,
};

static void freertos_get_irqrsved(void)
{
	struct device_node *np;
	const __be32 *irqrsv;
	u32 len, irq;
	unsigned int tmp;
	int i;

	mutex_lock(&freertos_lock);
	if (freertos_irqrsv_inited)
		goto exit;
	freertos_irqrsv_inited = 1;

	tmp = freertos_is_run();
	if (!tmp)
		goto exit;

	np = of_find_matching_node_and_match(NULL, aml_freertos_dt_match, NULL);
	if (!np)
		goto exit;
	if (!of_device_is_available(np))
		goto ret;
	irqrsv = of_get_property(np, "irqs_rsv", &len);
	if (!irqrsv)
		goto ret;
	len /= sizeof(*irqrsv);

	for (i = 0; i < len; i++) {
		irq = be32_to_cpu(irqrsv[i]);
		if (irq >= RSVED_MAX_IRQ)
			continue;
		set_bit(irq, freertos_irqrsv_mask);
	}
ret:
	of_node_put(np);
exit:
	mutex_unlock(&freertos_lock);
}

int freertos_is_irq_rsved(unsigned int irq)
{
	if (freertos_disabled || irq >= RSVED_MAX_IRQ)
		return 0;

	if (!freertos_irqrsv_inited)
		freertos_get_irqrsved();

	return test_bit(irq, freertos_irqrsv_mask);
}
EXPORT_SYMBOL(freertos_is_irq_rsved);

u32 freertos_get_irqregval(u32 val, u32 oldval,
			   unsigned int irqbase,
			   unsigned int n)
{
	u32 mask, valmask;
	u32 width;
	unsigned int j;

	if (freertos_disabled)
		return val;
	if (n > 32 || 32 % n) {
		pr_err("invalid number of irqs: n=%u\n", n);
		return 0;
	}
	width = 32 / n;
	mask = (1UL << width) - 1;
	valmask = 0;
	for (j = 0; j < n; j++) {
		if (freertos_is_irq_rsved(irqbase + j))
			valmask |= (mask << (j * width));
	}
	if (!valmask)
		return val;
	return (oldval & valmask) | (val & (~valmask));
}
EXPORT_SYMBOL(freertos_get_irqregval);

static int __init aml_freertos_init(void)
{
	if (freertos_disabled)
		return 0;

	return platform_driver_register(&aml_rtos_driver);
}
subsys_initcall_sync(aml_freertos_init);

static void __exit aml_freertos_exit(void)
{
	if (freertos_disabled)
		return;
	platform_driver_unregister(&aml_rtos_driver);
}
module_exit(aml_freertos_exit);

MODULE_DESCRIPTION("FreeRtos Driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Amlogic, Inc.");
