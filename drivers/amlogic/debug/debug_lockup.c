// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */
#include <linux/stacktrace.h>
#include <linux/export.h>
#include <linux/types.h>
#include <linux/smp.h>
#include <linux/irqflags.h>
#include <linux/sched.h>
#include <linux/moduleparam.h>
#include <linux/debugfs.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/sched/clock.h>
#include <linux/sched/debug.h>
#include <linux/slab.h>
#include <linux/amlogic/debug_ftrace_ramoops.h>
#include <linux/arm-smccc.h>
#include <linux/delay.h>
#include <linux/io.h>
#ifdef CONFIG_AMLOGIC_RAMDUMP
#include <linux/amlogic/ramdump.h>
#endif

/*isr trace*/
#define ns2ms			(1000 * 1000)
#define LONG_ISR		(500 * ns2ms)
#define LONG_SIRQ		(500 * ns2ms)
#define CHK_WINDOW		(1000 * ns2ms)
#define IRQ_CNT			256
#define CCCNT_WARN		15000
/*irq disable trace*/
#define LONG_IRQDIS		(1000 * 1000000)	/*1000 ms*/
#define OUT_WIN			(500 * 1000000)		/*500 ms*/
#define LONG_IDLE		(5000000000)		/*5 sec*/
#define ENTRY			10

/*isr trace*/
static unsigned long long t_base[IRQ_CNT];
static unsigned long long t_isr[IRQ_CNT];
static unsigned long long t_total[IRQ_CNT];
static unsigned int cnt_total[IRQ_CNT];
static void *cpu_action[NR_CPUS];
static int cpu_irq[NR_CPUS] = {0};
static void *cpu_sirq[NR_CPUS] = {NULL};
/*irq disable trace*/
static unsigned long long	t_i_d[NR_CPUS];
static int			irq_flg;
static struct stack_trace	irq_trace[NR_CPUS];
static unsigned long		t_entries[NR_CPUS][ENTRY];
static struct stack_trace smc_trace[NR_CPUS + 1];
static  unsigned long smc_t_entries[NR_CPUS + 1][ENTRY];
static unsigned long smc_comd[NR_CPUS + 1];
static unsigned long long t_s_d[NR_CPUS + 1];
static unsigned long long	t_idle[NR_CPUS] = { 0 };
static unsigned long long	t_d_out;

static unsigned long isr_thr = LONG_ISR;
core_param(isr_thr, isr_thr, ulong, 0644);
static unsigned long irq_dis_thr = LONG_IRQDIS;
core_param(irq_dis_thr, irq_dis_thr, ulong, 0644);
static unsigned long sirq_thr = LONG_SIRQ;
core_param(sirq_thr, sirq_thr, ulong, 0644);
static int irq_check_en;
core_param(irq_check_en, irq_check_en, int, 0644);
static int isr_check_en = 1;
core_param(isr_check_en, isr_check_en, int, 0644);
static unsigned long out_thr = OUT_WIN;
core_param(out_thr, out_thr, ulong, 0644);

static int isr_long_panic;
core_param(isr_long_panic, isr_long_panic, int, 0644);
static int sirq_long_panic;
core_param(sirq_long_panic, sirq_long_panic, int, 0644);
static int dis_irq_long_panic;
core_param(dis_irq_long_panic, dis_irq_long_panic, int, 0644);
static int irq_ratio_panic;
core_param(irq_ratio_panic, irq_ratio_panic, int, 0644);

#if (defined CONFIG_ARM64) || (defined CONFIG_AMLOGIC_ARMV8_AARCH32)
static int fiq_check_en = 1;
core_param(fiq_check_en, fiq_check_en, int, 0644);
static int fiq_check_show_regs_en;
core_param(fiq_check_show_regs_en, fiq_check_show_regs_en, int, 0644);

struct atf_regs {
	u64 regs[31];
	u64 sp;
	u64 pc;
	u64 pstate;
};

struct atf_fiq_smc_calls_result {
	unsigned long phy_addr;
	unsigned long buf_size;
	unsigned long percpu_size;
	unsigned long reserved1;
};

static void *atf_fiq_debug_virt_addr;
static unsigned long atf_fiq_debug_buf_size;
static unsigned long atf_fiq_debug_percpu_size;
#endif

noinline void isr_long_trace(void)
{
	pr_err("%s()\n", __func__);
}
EXPORT_SYMBOL(isr_long_trace);

noinline void irq_ratio_trace(void)
{
	pr_err("%s()\n", __func__);
}
EXPORT_SYMBOL(irq_ratio_trace);

noinline void sirq_long_trace(void)
{
	pr_err("%s()\n", __func__);
}
EXPORT_SYMBOL(sirq_long_trace);

noinline void dis_irq_long_trace(void)
{
	pr_err("%s()\n", __func__);
}
EXPORT_SYMBOL(dis_irq_long_trace);

extern struct task_struct *get_current_cpu_task(int cpu);

void notrace isr_in_hook(unsigned int cpu, unsigned long long *tin,
			 unsigned int irq, void *act)
{
	if (irq >= IRQ_CNT || !isr_check_en || oops_in_progress)
		return;

	pstore_ftrace_io_tag((unsigned long)irq, (unsigned long)act);

	cpu_irq[cpu] = irq;
	cpu_action[cpu] = act;
	*tin = sched_clock();
	if (*tin >= CHK_WINDOW + t_base[irq]) {
		t_base[irq]	= *tin;
		t_isr[irq]	= 0;
		t_total[irq]	= 0;
		cnt_total[irq]	= 0;
	}
}

void notrace isr_out_hook(unsigned int cpu, unsigned long long tin,
			  unsigned int irq)
{
	unsigned long long tout;
	unsigned long long ratio = 0;
	unsigned long long t_diff;
	unsigned long long t_isr_tmp;
	unsigned long long t_total_tmp;

	if (!isr_check_en || oops_in_progress)
		return;
	if (irq >= IRQ_CNT || cpu_irq[cpu] <= 0)
		return;
	cpu_irq[cpu] = 0;
	tout = sched_clock();
	t_isr[irq] += (tout > tin) ? (tout - tin) : 0;
	t_total[irq] = (tout - t_base[irq]);
	cnt_total[irq]++;

	pstore_ftrace_io_tag(0xFFF1, (unsigned long)irq);

	if (tout > isr_thr + tin) {
		t_diff = tout - tin;
		do_div(t_diff, ns2ms);
		pr_err("ISR_Long___ERR. irq:%d  tout-tin:%llu ms\n",
		       irq, t_diff);
		isr_long_trace();

		if (isr_long_panic)
			panic("isr_long_panic");
	}

	if (t_total[irq] < CHK_WINDOW)
		return;

	if (t_isr[irq] * 100 >= 50 * t_total[irq] || cnt_total[irq] > CCCNT_WARN) {
		t_isr_tmp = t_isr[irq];
		do_div(t_isr_tmp, ns2ms);
		t_total_tmp = t_total[irq];
		do_div(t_total_tmp, ns2ms);
		ratio = t_isr_tmp * 100;
		do_div(ratio, t_total_tmp);
		pr_err("IRQRatio___ERR.irq:%d ratio:%llu\n", irq, ratio);
		pr_err("t_isr:%llu  t_total:%llu, cnt:%d, %ps t:%lld\n",
			t_isr_tmp, t_total_tmp, cnt_total[irq], cpu_action[cpu], tout - tin);

		irq_ratio_trace();

		if (irq_ratio_panic)
			panic("irq_ratio_panic");
	}
	t_base[irq] = sched_clock();
	t_isr[irq] = 0;
	t_total[irq] = 0;
	cnt_total[irq] = 0;
	cpu_action[cpu] = NULL;
}

void notrace sirq_in_hook(unsigned int cpu, unsigned long long *tin, void *p)
{
	cpu_sirq[cpu] = p;
	*tin = sched_clock();
	pstore_ftrace_io_tag(0xFFE0, (unsigned long)p);
}

void notrace sirq_out_hook(unsigned int cpu, unsigned long long tin, void *p)
{
	unsigned long long tout = sched_clock();
	unsigned long long t_diff;

	pstore_ftrace_io_tag(0xFFE1, (unsigned long)p);

	if (cpu_sirq[cpu] && tin && (tout > tin + sirq_thr) &&
	    !oops_in_progress) {
		t_diff = tout - tin;
		do_div(t_diff, ns2ms);
		pr_err("SIRQLong___ERR. sirq:%p  tout-tin:%llu ms\n",
		       p, t_diff);
		sirq_long_trace();

		if (sirq_long_panic)
			panic("sirq_long_panic");
	}
	cpu_sirq[cpu] = NULL;
}

void notrace irq_trace_start(unsigned long flags)
{
	unsigned int cpu;
	int softirq = 0;

	if (!irq_flg  || !irq_check_en || oops_in_progress)
		return;

	if (arch_irqs_disabled_flags(flags))
		return;

	cpu = get_cpu();
	put_cpu();
	softirq =  task_thread_info(current)->preempt_count & SOFTIRQ_MASK;
	if ((!current->pid && !softirq) || t_i_d[cpu] || cpu_is_offline(cpu) ||
	    (softirq_count() && !cpu_sirq[cpu]))
		return;

	memset(&irq_trace[cpu], 0, sizeof(irq_trace[cpu]));
	memset(&t_entries[cpu][0], 0, sizeof(t_entries[cpu][0]) * ENTRY);
	irq_trace[cpu].entries = &t_entries[cpu][0];
	irq_trace[cpu].max_entries = ENTRY;
	irq_trace[cpu].skip = 2;
	irq_trace[cpu].nr_entries = 0;
	t_i_d[cpu] = sched_clock();

	save_stack_trace(&irq_trace[cpu]);
}
EXPORT_SYMBOL(irq_trace_start);

void notrace irq_trace_stop(unsigned long flag)
{
	unsigned long long t_i_e, t;
	unsigned int cpu;
	int softirq = 0;
	static int out_cnt;

	if (!irq_check_en ||  !irq_flg || oops_in_progress)
		return;

	if (arch_irqs_disabled_flags(flag))
		return;

	cpu = get_cpu();
	put_cpu();
	if (!t_i_d[cpu] ||
	    !arch_irqs_disabled_flags(arch_local_save_flags())) {
		t_i_d[cpu] = 0;
		return;
	}

	t_i_e = sched_clock();
	if (t_i_e <  t_i_d[cpu]) {
		t_i_d[cpu] = 0;
		return;
	}

	t = (t_i_e - t_i_d[cpu]);
	softirq =  task_thread_info(current)->preempt_count & SOFTIRQ_MASK;

	if (!(!current->pid && !softirq) && t > irq_dis_thr && t_i_d[cpu] &&
	    !(softirq_count() && !cpu_sirq[cpu])) {
		out_cnt++;
		if (t_i_e >= t_d_out + out_thr) {
			t_d_out = t_i_e;
			do_div(t, ns2ms);
			do_div(t_i_e, ns2ms);
			do_div(t_i_d[cpu], ns2ms);
			pr_err("\n\nDisIRQ___ERR:%llu ms <%llu %llu> %d:\n",
			       t, t_i_e, t_i_d[cpu], out_cnt);
			stack_trace_print(irq_trace[cpu].entries,
					  irq_trace[cpu].nr_entries, 0);
			dump_stack();

			dis_irq_long_trace();

			if (dis_irq_long_panic)
				panic("dis_irq_long_panic");
		}
	}
	t_i_d[cpu] = 0;
}
EXPORT_SYMBOL(irq_trace_stop);

int  notrace in_irq_trace(void)
{
	int cpu = get_cpu();

	put_cpu();
	return (irq_check_en  && irq_flg && t_i_d[cpu]);
}

void __attribute__((weak)) lockup_hook(int cpu)
{
}

void  notrace __arch_cpu_idle_enter(void)
{
	int cpu;

	if ((!irq_check_en ||  !irq_flg) && !isr_check_en)
		return;
	cpu = get_cpu();
	put_cpu();
	t_idle[cpu] = local_clock();
}

void  notrace __arch_cpu_idle_exit(void)
{
	int cpu;

	if ((!irq_check_en ||  !irq_flg) && !isr_check_en)
		return;
	cpu = get_cpu();
	put_cpu();
	t_idle[cpu] = 0;
}

static unsigned long smcid_skip_list[] = {
	0x84000001, /* suspend A32*/
	0xC4000001, /* suspend A64*/
	0x84000002, /* cpu off */
	0x84000008, /* system off */
	0x84000009, /* system reset */
	0x8400000E, /* system suspend A32 */
	0xC400000E, /* system suspend A64 */
};

bool notrace is_noret_smcid(unsigned long smcid)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(smcid_skip_list); i++) {
		if (smcid == smcid_skip_list[i])
			return true;
	}

	return false;
}

void notrace smc_trace_start(unsigned long smcid, unsigned long val, bool noret)
{
	unsigned int cpu;
	unsigned long flags;

	if (noret) {
		pstore_ftrace_io_smc_noret_in(smcid, val);
		return;
	}

	pstore_ftrace_io_smc_in(smcid, val);

	if (!irq_flg || !irq_check_en || oops_in_progress)
		return;

	local_irq_save(flags);

	cpu = get_cpu();
	put_cpu();

	memset(&smc_trace[cpu], 0, sizeof(smc_trace[cpu]));
	memset(&smc_t_entries[cpu][0], 0, sizeof(smc_t_entries[cpu][0]) * ENTRY);
	smc_trace[cpu].entries = &smc_t_entries[cpu][0];
	smc_trace[cpu].max_entries = ENTRY;
	smc_trace[cpu].skip = 2;
	smc_trace[cpu].nr_entries = 0;
	t_s_d[cpu] = sched_clock();
	smc_comd[cpu] = smcid;

	save_stack_trace(&smc_trace[cpu]);
	local_irq_restore(flags);
}
EXPORT_SYMBOL(smc_trace_start);

void notrace smc_trace_stop(unsigned long smcid, unsigned long val)
{
	unsigned int cpu;
	unsigned long flags;

	pstore_ftrace_io_smc_out(smcid, val);

	if (!irq_flg || !irq_check_en || oops_in_progress)
		return;

	local_irq_save(flags);
	cpu = get_cpu();
	put_cpu();
	t_s_d[cpu] = 0;
	smc_comd[cpu] = 0;
	local_irq_restore(flags);
}
EXPORT_SYMBOL(smc_trace_stop);

#if (defined CONFIG_ARM64) || (defined CONFIG_AMLOGIC_ARMV8_AARCH32)
void atf_fiq_debug_entry(void)
{
	int cpu;
	struct atf_regs *pregs  = NULL;
	struct atf_regs atf_regs;
	struct pt_regs regs;

	cpu = get_cpu();
	put_cpu();

	pregs = (struct atf_regs *)(atf_fiq_debug_virt_addr +
						cpu * atf_fiq_debug_percpu_size);
	memcpy(&atf_regs, pregs, sizeof(struct atf_regs));

#ifdef CONFIG_ARM64
	/* sp_el0 */
	memcpy(&regs.regs[0], &atf_regs.regs[0], 31 * sizeof(uint64_t));
	regs.pc = atf_regs.pc;
	regs.pstate = atf_regs.pstate;
#elif CONFIG_AMLOGIC_ARMV8_AARCH32
	regs.ARM_cpsr = (unsigned long)atf_regs.pstate;
	regs.ARM_pc = (unsigned long)atf_regs.pc;
	regs.ARM_sp = (unsigned long)atf_regs.regs[19];
	regs.ARM_lr = (unsigned long)atf_regs.regs[18];
	regs.ARM_ip = (unsigned long)atf_regs.regs[12];
	regs.ARM_fp = (unsigned long)atf_regs.regs[11];
	regs.ARM_r10 = (unsigned long)atf_regs.regs[10];
	regs.ARM_r9 = (unsigned long)atf_regs.regs[9];
	regs.ARM_r8 = (unsigned long)atf_regs.regs[8];
	regs.ARM_r7 = (unsigned long)atf_regs.regs[7];
	regs.ARM_r6 = (unsigned long)atf_regs.regs[6];
	regs.ARM_r5 = (unsigned long)atf_regs.regs[5];
	regs.ARM_r4 = (unsigned long)atf_regs.regs[4];
	regs.ARM_r3 = (unsigned long)atf_regs.regs[3];
	regs.ARM_r2 = (unsigned long)atf_regs.regs[2];
	regs.ARM_r1 = (unsigned long)atf_regs.regs[1];
	regs.ARM_r0 = (unsigned long)atf_regs.regs[0];
#endif
#ifdef CONFIG_AMLOGIC_RAMDUMP
	pr_err("\n");
	do_flush_cpu_cache();
#endif

	pr_err("\n\n--------fiq_dump CPU%d--------\n\n", cpu);
	if (fiq_check_show_regs_en)
		show_regs(&regs);
	dump_stack();
	pr_err("\n");
	while (1)
		;
}
#endif

void pr_lockup_info(int c)
{
	int cpu;
	int virq = irq_check_en;
	int visr = isr_check_en;
	unsigned long long t_idle_diff;
	unsigned long long t_idle_tmp;
	unsigned long long t_i_diff;
	unsigned long long t_i_tmp;

	irq_flg = 0;
	irq_check_en = 0;
	isr_check_en = 0;
	console_loglevel = 7;
	pr_err("\n\n\nHARDLOCKUP____ERR.CPU[%d] <irqen:%d isren%d>START\n",
	       c, virq, visr);
	for_each_online_cpu(cpu) {
		/*
		 * The error is caused by using for_each_cpu type macros.
		 */
		/* coverity[overrun-local:SUPPRESS] */
		unsigned long long t_cur = sched_clock();
		struct task_struct *p = get_current_cpu_task(cpu);
		int preempt = task_thread_info(p)->preempt_count;

		pr_err("\ndump_cpu[%d] irq:%3d preempt:%x  %s\n",
		       cpu, cpu_irq[cpu], preempt,	 p->comm);

		pr_err("IRQ %ps, %x\n", cpu_action[cpu],
		       (unsigned int)(preempt & HARDIRQ_MASK));

		pr_err("SotIRQ %ps, %x\n", cpu_sirq[cpu],
		       (unsigned int)(preempt & SOFTIRQ_MASK));

		if (t_i_d[cpu]) {
			t_i_diff = t_cur - t_i_d[cpu];
			do_div(t_i_diff, ns2ms);
			t_i_tmp = t_i_d[cpu];
			do_div(t_i_tmp, ns2ms);
			pr_err("IRQ____ERR[%d]. <%llu %llu>.\n",
			       cpu, t_i_tmp, t_i_diff);
			stack_trace_print(irq_trace[cpu].entries,
					  irq_trace[cpu].nr_entries, 0);
		}

		if (t_s_d[cpu] || smc_comd[cpu]) {
			t_i_diff = t_cur - t_s_d[cpu];
			do_div(t_i_diff, ns2ms);
			t_i_tmp = t_s_d[cpu];
			do_div(t_i_tmp, ns2ms);
			pr_err("SMC____ERR[%d]. <0x%lx %llu %llu>.\n",
			       cpu, smc_comd[cpu], t_i_tmp, t_i_diff);
			stack_trace_print(smc_trace[cpu].entries,
					  smc_trace[cpu].nr_entries, 0);
		}
		if (t_idle[cpu] && (t_idle[cpu] > LONG_IDLE + t_cur)) {
			t_idle_diff = t_cur - t_idle[cpu];
			do_div(t_idle_diff, ns2ms);
			do_div(t_cur, ns2ms);
			t_idle_tmp = t_idle[cpu];
			do_div(t_idle_tmp, ns2ms);
			pr_err("CPU[%d] IdleLong____ERR:%llu ms <%llu %llu>\n",
			       cpu, t_idle_diff, t_cur, t_idle_tmp);
		}
		dump_cpu_task(cpu);
		lockup_hook(cpu);
	}
	pr_err("\nHARDLOCKUP____ERR.END\n\n");

#if (defined CONFIG_ARM64) || (defined CONFIG_AMLOGIC_ARMV8_AARCH32)
	if (atf_fiq_debug_virt_addr && fiq_check_en) {
		struct arm_smccc_res res;

		arm_smccc_smc(0x820000f1, 0x2, c, 0, 0, 0, 0, 0, &res);
		mdelay(1000);//wait for the fiq dump to complete
	}
#endif
}

static struct dentry *debug_lockup;
#define debug_fs(x)							\
static ssize_t x##_write(struct file *file, const char __user *userbuf,	\
				   size_t count, loff_t *ppos)		\
{									\
	char buf[20];							\
	unsigned long val;						\
	int ret;							\
	count = min_t(size_t, count, (sizeof(buf) - 1));		\
	if (copy_from_user(buf, userbuf, count))			\
		return -EFAULT;						\
	buf[count] = 0;							\
	ret = kstrtoul(buf, 0, &val);					\
	if (ret)							\
		return -1;						\
	x = (typeof(x))val;						\
	pr_info("%s:%ld\n", __func__, (unsigned long)x);		\
	return count;							\
}									\
static ssize_t x##_read(struct file *file, char __user *userbuf,	\
				 size_t count, loff_t *ppos)		\
{									\
	char buf[20];							\
	unsigned long val;						\
	ssize_t len;							\
	val = (unsigned long)x;						\
	len = snprintf(buf, sizeof(buf), "%ld\n", val);			\
	pr_info("%s:%ld\n", __func__, val);				\
	return simple_read_from_buffer(userbuf, count, ppos, buf, len);	\
}									\
static const struct file_operations x##_debug_ops = {			\
	.open		= simple_open,					\
	.read		= x##_read,					\
	.write		= x##_write,					\
}

debug_fs(isr_thr);
debug_fs(irq_dis_thr);
debug_fs(sirq_thr);
debug_fs(irq_check_en);
debug_fs(isr_check_en);
debug_fs(out_thr);
#define FIVE(m)         {m; m; m; m; m; }
#define TEN(m)          {FIVE(m) FIVE(m)}
#define FIFTY(m)        {TEN(m) TEN(m) TEN(m) TEN(m) TEN(m)}
#define HUNDRED(m)      {FIFTY(m) FIFTY(m)}
#define FIVE_HUNDRED(m) {HUNDRED(m) HUNDRED(m) HUNDRED(m) HUNDRED(m) HUNDRED(m)}
#define THOUSAND(m)     {FIVE_HUNDRED(m) FIVE_HUNDRED(m)}
#if defined CONFIG_ARM
#define ONE_LOOP asm("add r0, r0, #1\n":::"r0", "memory")
#elif defined CONFIG_ARM64
#define ONE_LOOP asm("add x0, x0, #1\n":::"x0", "memory")
#endif

#define TIMENS_1M_LOOPS_OF_1G_CPU 1005000  //about 1ms
static int cpu_speed_test(void)
{
	int i, mhz;
	unsigned long long start, end;
	unsigned int delta;
	unsigned long flags;

	local_irq_save(flags);

	/* do 1M loops */
	start = sched_clock();
	for (i = 0; i < 1000; i++)
		THOUSAND(ONE_LOOP);

	end = sched_clock();

	local_irq_restore(flags);

	if (end - start > UINT_MAX) {
		WARN(1, "%s() consume %llu ns\n", __func__, end - start);
		return 0;
	}

	delta = (unsigned int)(end - start);
	mhz = TIMENS_1M_LOOPS_OF_1G_CPU * 1000 / delta;

	pr_debug("%s() delta_us=%u mhz=%d\n", __func__, delta / 1000, mhz);

	return mhz;
}

static ssize_t cpu_mhz_read(struct file *file, char __user *userbuf,
			    size_t count, loff_t *ppos)
{
	char buf[20];
	int mhz;
	ssize_t len;

	mhz = cpu_speed_test();
	len = snprintf(buf, sizeof(buf), "%d\n", mhz);

	pr_info("cpu_speed_test() mhz=%d\n", mhz);

	return simple_read_from_buffer(userbuf, count, ppos, buf, len);
}

static const struct file_operations cpu_mhz_debug_ops = {
	.open = simple_open,
	.read = cpu_mhz_read,
};

static int test_mhz_period_ms;
core_param(test_mhz_period_ms, test_mhz_period_ms, int, 0644);

static int test_mhz_console;
core_param(test_mhz_console, test_mhz_console, int, 0644);

static void remote_function(void *data)
{
	int mhz;

	mhz = cpu_speed_test();

	//trace_printk("cpu[%d] cpu_mhz=%d\n", smp_processor_id(), mhz);
	if (test_mhz_console)
		pr_info("cpu[%d] cpu_mhz=%d\n", smp_processor_id(), mhz);
}

static enum hrtimer_restart test_mhz_hrtimer_func(struct hrtimer *timer)
{
	int mhz, cpu, cur_cpu, cur_cluster;
	ktime_t now, interval;
	static call_single_data_t csd;

	if (!test_mhz_period_ms) {
		pr_info("stop test_mhz hrtimer\n");
		return HRTIMER_NORESTART;
	}

	/* don't too frequently */
	if (test_mhz_period_ms < 1000)
		test_mhz_period_ms = 1000;

	mhz = cpu_speed_test();

	cur_cpu = smp_processor_id();
	cur_cluster = topology_physical_package_id(cur_cpu);

	//trace_printk("cpu[%d] cpu_mhz=%d\n", cur_cpu, mhz);
	if (test_mhz_console)
		pr_info("cpu[%d] cpu_mhz=%d\n", cur_cpu, mhz);

	for_each_online_cpu(cpu) {
		if (cpu == cur_cpu)
			continue;

		if (topology_physical_package_id(cpu) != cur_cluster) {
			csd.func = remote_function;
			csd.info = NULL;
			csd.flags = 0;
			smp_call_function_single_async(cpu, &csd);
			break;
		}
	}

	now = ktime_get();
	interval = ktime_set(test_mhz_period_ms / 1000,
			     test_mhz_period_ms % 1000 * 1000000);
	hrtimer_forward(timer, now, interval);
	return HRTIMER_RESTART;
}

static void test_mhz_hrtimer_init(void)
{
	struct hrtimer *hrtimer;

	if (!test_mhz_period_ms)
		return;

	hrtimer = kmalloc(sizeof(*hrtimer), GFP_KERNEL);

	hrtimer_init(hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	hrtimer->function = test_mhz_hrtimer_func;

	hrtimer_start(hrtimer, ktime_set(1, 0), HRTIMER_MODE_REL);
}

#if (defined CONFIG_ARM64) || (defined CONFIG_AMLOGIC_ARMV8_AARCH32)
static void fiq_debug_addr_init(void)
{
	union {
		struct arm_smccc_res smccc;
		struct atf_fiq_smc_calls_result result;
	} res;
	phys_addr_t atf_fiq_debug_phy_addr = 0;

	memset(&res, 0, sizeof(res));
	arm_smccc_smc(0x820000f1, 0x1,
		(unsigned long)atf_fiq_debug_entry, 0, 0, 0, 0, 0, &res.smccc);
	atf_fiq_debug_phy_addr = (phys_addr_t)res.result.phy_addr;
	atf_fiq_debug_buf_size = res.result.buf_size;
	atf_fiq_debug_percpu_size = res.result.percpu_size;

	/* Current ATF version does not support FIQ DEBUG */
	if (atf_fiq_debug_phy_addr == 0 || atf_fiq_debug_phy_addr == 0xFFFFFFFF) {
		pr_err("invalid atf_buf_phy_addr\n");
		return;
	}

	if (pfn_valid(__phys_to_pfn(atf_fiq_debug_phy_addr)))
		atf_fiq_debug_virt_addr = (void __iomem *)
					__phys_to_virt(atf_fiq_debug_phy_addr);
	else
		atf_fiq_debug_virt_addr = ioremap_cache(atf_fiq_debug_phy_addr,
						atf_fiq_debug_buf_size);

	pr_info("atf phy addr:%lx, size: %lx, atf virt addr:%lx\n",
		(unsigned long)atf_fiq_debug_phy_addr, atf_fiq_debug_buf_size,
		(unsigned long)atf_fiq_debug_virt_addr);

	if (atf_fiq_debug_virt_addr)
		memset(atf_fiq_debug_virt_addr, 0, atf_fiq_debug_buf_size);
}
#endif

static int __init debug_lockup_init(void)
{
	debug_lockup = debugfs_create_dir("lockup", NULL);
	if (IS_ERR_OR_NULL(debug_lockup)) {
		pr_warn("failed to create debug_lockup\n");
		debug_lockup = NULL;
		return -1;
	}
	debugfs_create_file("isr_thr", S_IFREG | 0664,
			    debug_lockup, NULL, &isr_thr_debug_ops);
	debugfs_create_file("irq_dis_thr", S_IFREG | 0664,
			    debug_lockup, NULL, &irq_dis_thr_debug_ops);
	debugfs_create_file("sirq_thr", S_IFREG | 0664,
			    debug_lockup, NULL, &sirq_thr_debug_ops);
	debugfs_create_file("out_thr", S_IFREG | 0664,
			    debug_lockup, NULL, &out_thr_debug_ops);
	debugfs_create_file("isr_check_en", S_IFREG | 0664,
			    debug_lockup, NULL, &isr_check_en_debug_ops);
	debugfs_create_file("irq_check_en", S_IFREG | 0664,
			    debug_lockup, NULL, &irq_check_en_debug_ops);

	debugfs_create_file("cpu_mhz", S_IFREG | 0664,
			    debug_lockup, NULL, &cpu_mhz_debug_ops);
	irq_flg = 1;

	test_mhz_hrtimer_init();
#if (defined CONFIG_ARM64) || (defined CONFIG_AMLOGIC_ARMV8_AARCH32)
	fiq_debug_addr_init();
#endif

	return 0;
}
late_initcall(debug_lockup_init);

MODULE_DESCRIPTION("Amlogic debug lockup module");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jianxiong Pan <jianxiong.pan@amlogic.com>");
