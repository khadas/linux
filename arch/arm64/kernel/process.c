// SPDX-License-Identifier: GPL-2.0-only
/*
 * Based on arch/arm/kernel/process.c
 *
 * Original Copyright (C) 1995  Linus Torvalds
 * Copyright (C) 1996-2000 Russell King - Converted to ARM.
 * Copyright (C) 2012 ARM Ltd.
 */

#include <stdarg.h>

#include <linux/compat.h>
#include <linux/efi.h>
#include <linux/export.h>
#include <linux/sched.h>
#include <linux/sched/debug.h>
#include <linux/sched/task.h>
#include <linux/sched/task_stack.h>
#include <linux/kernel.h>
#include <linux/lockdep.h>
#include <linux/mm.h>
#include <linux/stddef.h>
#include <linux/sysctl.h>
#include <linux/unistd.h>
#include <linux/user.h>
#include <linux/delay.h>
#include <linux/reboot.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/cpu.h>
#include <linux/elfcore.h>
#include <linux/pm.h>
#include <linux/tick.h>
#include <linux/utsname.h>
#include <linux/uaccess.h>
#include <linux/random.h>
#include <linux/hw_breakpoint.h>
#include <linux/personality.h>
#include <linux/notifier.h>
#include <trace/events/power.h>
#include <linux/percpu.h>
#include <linux/thread_info.h>
#include <linux/prctl.h>

#include <asm/alternative.h>
#include <asm/arch_gicv3.h>
#include <asm/compat.h>
#include <asm/cpufeature.h>
#include <asm/cacheflush.h>
#include <asm/exec.h>
#include <asm/fpsimd.h>
#include <asm/mmu_context.h>
#include <asm/processor.h>
#include <asm/pointer_auth.h>
#include <asm/scs.h>
#include <asm/stacktrace.h>

#if defined(CONFIG_STACKPROTECTOR) && !defined(CONFIG_STACKPROTECTOR_PER_TASK)
#include <linux/stackprotector.h>
unsigned long __stack_chk_guard __ro_after_init;
EXPORT_SYMBOL(__stack_chk_guard);
#endif

#ifdef CONFIG_AMLOGIC_MODIFY
#include <linux/amlogic/secmon.h>
#endif

/*
 * Function pointers to optional machine specific functions
 */
void (*pm_power_off)(void);
EXPORT_SYMBOL_GPL(pm_power_off);

static void __cpu_do_idle(void)
{
	dsb(sy);
	wfi();
}

static void __cpu_do_idle_irqprio(void)
{
	unsigned long pmr;
	unsigned long daif_bits;

	daif_bits = read_sysreg(daif);
	write_sysreg(daif_bits | PSR_I_BIT, daif);

	/*
	 * Unmask PMR before going idle to make sure interrupts can
	 * be raised.
	 */
	pmr = gic_read_pmr();
	gic_write_pmr(GIC_PRIO_IRQON | GIC_PRIO_PSR_I_SET);

	__cpu_do_idle();

	gic_write_pmr(pmr);
	write_sysreg(daif_bits, daif);
}

/*
 *	cpu_do_idle()
 *
 *	Idle the processor (wait for interrupt).
 *
 *	If the CPU supports priority masking we must do additional work to
 *	ensure that interrupts are not masked at the PMR (because the core will
 *	not wake up if we block the wake up signal in the interrupt controller).
 */
void cpu_do_idle(void)
{
	if (system_uses_irq_prio_masking())
		__cpu_do_idle_irqprio();
	else
		__cpu_do_idle();
}

/*
 * This is our default idle handler.
 */
void arch_cpu_idle(void)
{
	/*
	 * This should do all the clock switching and wait for interrupt
	 * tricks
	 */
	trace_cpu_idle_rcuidle(1, smp_processor_id());
	cpu_do_idle();
	local_irq_enable();
	trace_cpu_idle_rcuidle(PWR_EVENT_EXIT, smp_processor_id());
}

#ifdef CONFIG_HOTPLUG_CPU
void arch_cpu_idle_dead(void)
{
       cpu_die();
}
#endif

/*
 * Called by kexec, immediately prior to machine_kexec().
 *
 * This must completely disable all secondary CPUs; simply causing those CPUs
 * to execute e.g. a RAM-based pin loop is not sufficient. This allows the
 * kexec'd kernel to use any and all RAM as it sees fit, without having to
 * avoid any code or data used by any SW CPU pin loop. The CPU hotplug
 * functionality embodied in disable_nonboot_cpus() to achieve this.
 */
void machine_shutdown(void)
{
	disable_nonboot_cpus();
}

/*
 * Halting simply requires that the secondary CPUs stop performing any
 * activity (executing tasks, handling interrupts). smp_send_stop()
 * achieves this.
 */
void machine_halt(void)
{
	local_irq_disable();
	smp_send_stop();
	while (1);
}

/*
 * Power-off simply requires that the secondary CPUs stop performing any
 * activity (executing tasks, handling interrupts). smp_send_stop()
 * achieves this. When the system power is turned off, it will take all CPUs
 * with it.
 */
void machine_power_off(void)
{
	local_irq_disable();
	smp_send_stop();
	if (pm_power_off)
		pm_power_off();
}

/*
 * Restart requires that the secondary CPUs stop performing any activity
 * while the primary CPU resets the system. Systems with multiple CPUs must
 * provide a HW restart implementation, to ensure that all CPUs reset at once.
 * This is required so that any code running after reset on the primary CPU
 * doesn't have to co-ordinate with other CPUs to ensure they aren't still
 * executing pre-reset code, and using RAM that the primary CPU's code wishes
 * to use. Implementing such co-ordination would be essentially impossible.
 */
void machine_restart(char *cmd)
{
	/* Disable interrupts first */
	local_irq_disable();
	smp_send_stop();

	/*
	 * UpdateCapsule() depends on the system being reset via
	 * ResetSystem().
	 */
	if (efi_enabled(EFI_RUNTIME_SERVICES))
		efi_reboot(reboot_mode, NULL);

	/* Now call the architecture specific reboot code. */
	do_kernel_restart(cmd);

	/*
	 * Whoops - the architecture was unable to reboot.
	 */
	printk("Reboot failed -- System halted\n");
	while (1);
}

static void print_pstate(struct pt_regs *regs)
{
	u64 pstate = regs->pstate;

	if (compat_user_mode(regs)) {
		printk("pstate: %08llx (%c%c%c%c %c %s %s %c%c%c)\n",
			pstate,
			pstate & PSR_AA32_N_BIT ? 'N' : 'n',
			pstate & PSR_AA32_Z_BIT ? 'Z' : 'z',
			pstate & PSR_AA32_C_BIT ? 'C' : 'c',
			pstate & PSR_AA32_V_BIT ? 'V' : 'v',
			pstate & PSR_AA32_Q_BIT ? 'Q' : 'q',
			pstate & PSR_AA32_T_BIT ? "T32" : "A32",
			pstate & PSR_AA32_E_BIT ? "BE" : "LE",
			pstate & PSR_AA32_A_BIT ? 'A' : 'a',
			pstate & PSR_AA32_I_BIT ? 'I' : 'i',
			pstate & PSR_AA32_F_BIT ? 'F' : 'f');
	} else {
		printk("pstate: %08llx (%c%c%c%c %c%c%c%c %cPAN %cUAO)\n",
			pstate,
			pstate & PSR_N_BIT ? 'N' : 'n',
			pstate & PSR_Z_BIT ? 'Z' : 'z',
			pstate & PSR_C_BIT ? 'C' : 'c',
			pstate & PSR_V_BIT ? 'V' : 'v',
			pstate & PSR_D_BIT ? 'D' : 'd',
			pstate & PSR_A_BIT ? 'A' : 'a',
			pstate & PSR_I_BIT ? 'I' : 'i',
			pstate & PSR_F_BIT ? 'F' : 'f',
			pstate & PSR_PAN_BIT ? '+' : '-',
			pstate & PSR_UAO_BIT ? '+' : '-');
	}
}

#ifdef CONFIG_AMLOGIC_USER_FAULT

static void show_data(unsigned long addr, int nbytes, const char *name)
{
	int	i, j;
	int	nlines;
	u32	*p;

	/*
	 * don't attempt to dump non-kernel addresses or
	 * values that are probably just small negative numbers
	 */
	if (addr < PAGE_OFFSET || addr > -256UL)
		return;

	/*
	 * Treating data in general purpose register as an address
	 * and dereferencing it is quite a dangerous behaviour,
	 * especially when it belongs to secure monotor region or
	 * ioremap region(for arm64 vmalloc region is already filtered
	 * out), which can lead to external abort on non-linefetch and
	 * can not be protected by probe_kernel_address.
	 * We need more strict filtering rules
	 */

#ifdef CONFIG_AMLOGIC_SEC
	/*
	 * filter out secure monitor region
	 */
	if (addr <= (unsigned long)high_memory)
		if (within_secmon_region(addr)) {
			printk("\n%s: %#lx S\n", name, addr);
			return;
		}
#endif

	printk("\n%s: %#lx:\n", name, addr);

	/*
	 * round address down to a 32 bit boundary
	 * and always dump a multiple of 32 bytes
	 */
	p = (u32 *)(addr & ~(sizeof(u32) - 1));
	nbytes += (addr & (sizeof(u32) - 1));
	nlines = (nbytes + 31) / 32;

	for (i = 0; i < nlines; i++) {
		/*
		 * just display low 16 bits of address to keep
		 * each line of the dump < 80 characters
		 */
		printk("%04lx ", (unsigned long)p & 0xffff);
		for (j = 0; j < 8; j++) {
			u32	data;

			if (probe_kernel_address(p, data)) {
				pr_cont(" ********");
			} else {
				pr_cont(" %08x", data);
			}
			++p;
		}
		pr_cont("\n");
	}
}
/*
 * dump a block of user memory from around the given address
 */
static void show_user_data(unsigned long addr, int nbytes, const char *name)
{
	int	i, j;
	int	nlines;
	u32	*p;

	if (!access_ok((void *)addr, nbytes))
		return;

#ifdef CONFIG_AMLOGIC_SEC
	/*
	 * filter out secure monitor region
	 */
	if (within_secmon_region(addr)) {
		pr_info("\n%s: %#lx S\n", name, addr);
		return;
	}
#endif

	pr_info("\n%s: %#lx:\n", name, addr);

	/*
	 * round address down to a 32 bit boundary
	 * and always dump a multiple of 32 bytes
	 */
	p = (u32 *)(addr & ~(sizeof(u32) - 1));
	nbytes += (addr & (sizeof(u32) - 1));
	nlines = (nbytes + 31) / 32;

	for (i = 0; i < nlines; i++) {
		/*
		 * just display low 16 bits of address to keep
		 * each line of the dump < 80 characters
		 */
		pr_info("%04lx ", (unsigned long)p & 0xffff);
		if (irqs_disabled())
			continue;
		for (j = 0; j < 8; j++) {
			u32	data;
			int bad;

			bad = __get_user(data, p);
			if (bad)
				pr_cont(" ********");
			else
				pr_cont(" %08x", data);
			++p;
		}
		pr_cont("\n");
	}
}

static void show_pfn(unsigned long reg, char *s)
{
	struct page *page;

	if (reg < (unsigned long)PAGE_OFFSET) {
		pr_info("%s : %016lx  U\n", s, reg);
	} else if (reg <= (unsigned long)high_memory) {
		pr_info("%s : %016lx, PFN:%5lx L\n", s, reg, virt_to_pfn(reg));
	} else {
		page = vmalloc_to_page((const void *)reg);
		if (page)
			pr_info("%s : %016lx, PFN:%5lx V\n", s, reg, page_to_pfn(page));
		else
			pr_info("%s : %016lx, PFN:***** V\n", s, reg);
	}
}

static void show_regs_pfn(struct pt_regs *regs)
{
	int i;
	struct page *page;

	for (i = 0; i < 31; i++) {
		if (regs->regs[i] < (unsigned long)PAGE_OFFSET) {
			continue;
		} else if (regs->regs[i] <= (unsigned long)high_memory) {
			pr_info("R%-2d : %016llx, PFN:%5lx L\n",
				i, regs->regs[i], virt_to_pfn((void *)regs->regs[i]));
		} else {
			page = vmalloc_to_page((void *)regs->regs[i]);
			if (page)
				pr_info("R%-2d : %016llx, PFN:%5lx V\n",
					i, regs->regs[i], page_to_pfn(page));
			else
				pr_info("R%-2d : %016llx, PFN:***** V\n", i, regs->regs[i]);
		}
	}
}

static void show_extra_register_data(struct pt_regs *regs, int nbytes)
{
	mm_segment_t fs;
	unsigned int i;

	fs = get_fs();
	set_fs(KERNEL_DS);
	show_data(regs->pc - nbytes, nbytes * 2, "PC");
	show_data(regs->regs[30] - nbytes, nbytes * 2, "LR");
	show_data(regs->sp - nbytes, nbytes * 2, "SP");
	show_data(read_sysreg(far_el1), nbytes * 2, "FAR");
	for (i = 0; i < 30; i++) {
		char name[4];

		snprintf(name, sizeof(name), "X%u", i);
		show_data(regs->regs[i] - nbytes, nbytes * 2, name);
	}
	set_fs(fs);
}

static void show_user_extra_register_data(struct pt_regs *regs, int nbytes)
{
	unsigned int i, top_reg;
	u64 sp, lr;

	if (compat_user_mode(regs)) {
		lr = regs->compat_lr;
		sp = regs->compat_sp;
		top_reg = 13;
	} else {
		lr = regs->regs[30];
		sp = regs->sp;
		top_reg = 29;
	}

	show_user_data(regs->pc - nbytes, nbytes * 2, "PC");
	show_user_data(lr - nbytes, nbytes * 2, "LR");
	show_user_data(sp - nbytes, nbytes * 2, "SP");
	show_user_data(read_sysreg(far_el1), nbytes * 2, "FAR");
	for (i = 0; i < top_reg; i++) {
		char name[4];

		snprintf(name, sizeof(name), "r%u", i);
		show_user_data(regs->regs[i] - nbytes, nbytes * 2, name);
	}
}

void show_vma(struct mm_struct *mm, unsigned long addr)
{
	struct vm_area_struct *vma;
	struct file *file;
	vm_flags_t flags;
	unsigned long ino = 0;
	unsigned long long pgoff = 0;
	unsigned long start, end;
	dev_t dev = 0;
	const char *name = NULL;

	vma = find_vma(mm, addr);
	if (!vma) {
		pr_info("can't find vma for %lx\n", addr);
		return;
	}

	file = vma->vm_file;
	flags = vma->vm_flags;
	if (file) {
		struct inode *inode = file_inode(vma->vm_file);

		dev = inode->i_sb->s_dev;
		ino = inode->i_ino;
		pgoff = ((loff_t)vma->vm_pgoff) << PAGE_SHIFT;
	}

	/* We don't show the stack guard page in /proc/maps */
	start = vma->vm_start;
	end = vma->vm_end;

	pr_info("vma for %lx:\n", addr);
	pr_info("%08lx-%08lx %c%c%c%c %08llx %02x:%02x %lu ",
		start,
		end,
		flags & VM_READ ? 'r' : '-',
		flags & VM_WRITE ? 'w' : '-',
		flags & VM_EXEC ? 'x' : '-',
		flags & VM_MAYSHARE ? 's' : 'p',
		pgoff,
		MAJOR(dev), MINOR(dev), ino);

	/*
	 * Print the dentry name for named mappings, and a
	 * special [heap] marker for the heap:
	 */
	if (file) {
		char file_name[256] = {};
		char *p = d_path(&file->f_path, file_name, 256);

		if (!IS_ERR(p)) {
			mangle_path(file_name, p, "\n");
			pr_info("%s", p);
		} else {
			pr_info(" get file path failed\n");
		}
		goto done;
	}

	name = arch_vma_name(vma);
	if (!name) {
		pid_t tid;

		if (!mm) {
			name = "[vdso]";
			goto done;
		}

		if (vma->vm_start <= mm->brk &&
		    vma->vm_end >= mm->start_brk) {
			name = "[heap]";
			goto done;
		}

		tid = vma_is_stack_for_current(vma);

		if (tid != 0) {
			/*
			 * Thread stack in /proc/PID/task/TID/maps or
			 * the main process stack.
			 */
			if ((vma->vm_start <= mm->start_stack &&
			    vma->vm_end >= mm->start_stack)) {
				name = "[stack]";
			} else {
				/* Thread stack in /proc/PID/maps */
				pr_info("[stack:%d]", tid);
			}
			goto done;
		}

		if (vma_get_anon_name(vma))
			pr_info("[anon]");
	}

done:
	if (name)
		pr_info("%s", name);
	pr_info("\n");
}
#endif /* CONFIG_AMLOGIC_USER_FAULT */

void __show_regs(struct pt_regs *regs)
{
	int i, top_reg;
	u64 lr, sp;

	if (compat_user_mode(regs)) {
		lr = regs->compat_lr;
		sp = regs->compat_sp;
		top_reg = 12;
	} else {
		lr = regs->regs[30];
		sp = regs->sp;
		top_reg = 29;
	}

#ifdef CONFIG_AMLOGIC_USER_FAULT
	if (user_mode(regs)) {
		show_vma(current->mm, instruction_pointer(regs));
		show_vma(current->mm, lr);
		show_vma(current->mm, read_sysreg(far_el1));
	}
	show_pfn(instruction_pointer(regs), "PC");
	show_pfn(sp, "SP");
	show_pfn(read_sysreg(far_el1), "FAR");
	show_regs_pfn(regs);
#endif /* CONFIG_AMLOGIC_USER_FAULT */

	show_regs_print_info(KERN_DEFAULT);
	print_pstate(regs);

	if (!user_mode(regs)) {
		printk("pc : %pS\n", (void *)regs->pc);
		printk("lr : %pS\n", (void *)lr);
	} else {
		printk("pc : %016llx\n", regs->pc);
		printk("lr : %016llx\n", lr);
	}

	printk("sp : %016llx\n", sp);

	if (system_uses_irq_prio_masking())
		printk("pmr_save: %08llx\n", regs->pmr_save);

	i = top_reg;

	while (i >= 0) {
		printk("x%-2d: %016llx ", i, regs->regs[i]);
		i--;

		if (i % 2 == 0) {
			pr_cont("x%-2d: %016llx ", i, regs->regs[i]);
			i--;
		}

		pr_cont("\n");
	}
#ifdef CONFIG_AMLOGIC_USER_FAULT
	if (!user_mode(regs))
		show_extra_register_data(regs, 128);
	else
		show_user_extra_register_data(regs, 128);
	printk("\n");
#endif /* CONFIG_AMLOGIC_USER_FAULT */
}

void show_regs(struct pt_regs * regs)
{
	__show_regs(regs);
	dump_backtrace(regs, NULL);
}

static void tls_thread_flush(void)
{
	write_sysreg(0, tpidr_el0);

	if (is_compat_task()) {
		current->thread.uw.tp_value = 0;

		/*
		 * We need to ensure ordering between the shadow state and the
		 * hardware state, so that we don't corrupt the hardware state
		 * with a stale shadow state during context switch.
		 */
		barrier();
		write_sysreg(0, tpidrro_el0);
	}
}

static void flush_tagged_addr_state(void)
{
	if (IS_ENABLED(CONFIG_ARM64_TAGGED_ADDR_ABI))
		clear_thread_flag(TIF_TAGGED_ADDR);
}

void flush_thread(void)
{
	fpsimd_flush_thread();
	tls_thread_flush();
	flush_ptrace_hw_breakpoint(current);
	flush_tagged_addr_state();
}

void release_thread(struct task_struct *dead_task)
{
}

void arch_release_task_struct(struct task_struct *tsk)
{
	fpsimd_release_task(tsk);
}

int arch_dup_task_struct(struct task_struct *dst, struct task_struct *src)
{
	if (current->mm)
		fpsimd_preserve_current_state();
	*dst = *src;

	/* We rely on the above assignment to initialize dst's thread_flags: */
	BUILD_BUG_ON(!IS_ENABLED(CONFIG_THREAD_INFO_IN_TASK));

	/*
	 * Detach src's sve_state (if any) from dst so that it does not
	 * get erroneously used or freed prematurely.  dst's sve_state
	 * will be allocated on demand later on if dst uses SVE.
	 * For consistency, also clear TIF_SVE here: this could be done
	 * later in copy_process(), but to avoid tripping up future
	 * maintainers it is best not to leave TIF_SVE and sve_state in
	 * an inconsistent state, even temporarily.
	 */
	dst->thread.sve_state = NULL;
	clear_tsk_thread_flag(dst, TIF_SVE);

	return 0;
}

asmlinkage void ret_from_fork(void) asm("ret_from_fork");

int copy_thread_tls(unsigned long clone_flags, unsigned long stack_start,
		unsigned long stk_sz, struct task_struct *p, unsigned long tls)
{
	struct pt_regs *childregs = task_pt_regs(p);

	memset(&p->thread.cpu_context, 0, sizeof(struct cpu_context));

	/*
	 * In case p was allocated the same task_struct pointer as some
	 * other recently-exited task, make sure p is disassociated from
	 * any cpu that may have run that now-exited task recently.
	 * Otherwise we could erroneously skip reloading the FPSIMD
	 * registers for p.
	 */
	fpsimd_flush_task_state(p);

	if (likely(!(p->flags & PF_KTHREAD))) {
		*childregs = *current_pt_regs();
		childregs->regs[0] = 0;

		/*
		 * Read the current TLS pointer from tpidr_el0 as it may be
		 * out-of-sync with the saved value.
		 */
		*task_user_tls(p) = read_sysreg(tpidr_el0);

		if (stack_start) {
			if (is_compat_thread(task_thread_info(p)))
				childregs->compat_sp = stack_start;
			else
				childregs->sp = stack_start;
		}

		/*
		 * If a TLS pointer was passed to clone, use it for the new
		 * thread.
		 */
		if (clone_flags & CLONE_SETTLS)
			p->thread.uw.tp_value = tls;
	} else {
		memset(childregs, 0, sizeof(struct pt_regs));
		childregs->pstate = PSR_MODE_EL1h;
		if (IS_ENABLED(CONFIG_ARM64_UAO) &&
		    cpus_have_const_cap(ARM64_HAS_UAO))
			childregs->pstate |= PSR_UAO_BIT;

		if (arm64_get_ssbd_state() == ARM64_SSBD_FORCE_DISABLE)
			set_ssbs_bit(childregs);

		if (system_uses_irq_prio_masking())
			childregs->pmr_save = GIC_PRIO_IRQON;

		p->thread.cpu_context.x19 = stack_start;
		p->thread.cpu_context.x20 = stk_sz;
	}
	p->thread.cpu_context.pc = (unsigned long)ret_from_fork;
	p->thread.cpu_context.sp = (unsigned long)childregs;

	ptrace_hw_copy_thread(p);

	return 0;
}

void tls_preserve_current_state(void)
{
	*task_user_tls(current) = read_sysreg(tpidr_el0);
}

static void tls_thread_switch(struct task_struct *next)
{
	tls_preserve_current_state();

	if (is_compat_thread(task_thread_info(next)))
		write_sysreg(next->thread.uw.tp_value, tpidrro_el0);
	else if (!arm64_kernel_unmapped_at_el0())
		write_sysreg(0, tpidrro_el0);

	write_sysreg(*task_user_tls(next), tpidr_el0);
}

/* Restore the UAO state depending on next's addr_limit */
void uao_thread_switch(struct task_struct *next)
{
	if (IS_ENABLED(CONFIG_ARM64_UAO)) {
		if (task_thread_info(next)->addr_limit == KERNEL_DS)
			asm(ALTERNATIVE("nop", SET_PSTATE_UAO(1), ARM64_HAS_UAO));
		else
			asm(ALTERNATIVE("nop", SET_PSTATE_UAO(0), ARM64_HAS_UAO));
	}
}

/*
 * Force SSBS state on context-switch, since it may be lost after migrating
 * from a CPU which treats the bit as RES0 in a heterogeneous system.
 */
static void ssbs_thread_switch(struct task_struct *next)
{
	struct pt_regs *regs = task_pt_regs(next);

	/*
	 * Nothing to do for kernel threads, but 'regs' may be junk
	 * (e.g. idle task) so check the flags and bail early.
	 */
	if (unlikely(next->flags & PF_KTHREAD))
		return;

	/*
	 * If all CPUs implement the SSBS extension, then we just need to
	 * context-switch the PSTATE field.
	 */
	if (cpu_have_feature(cpu_feature(SSBS)))
		return;

	/* If the mitigation is enabled, then we leave SSBS clear. */
	if ((arm64_get_ssbd_state() == ARM64_SSBD_FORCE_ENABLE) ||
	    test_tsk_thread_flag(next, TIF_SSBD))
		return;

	if (compat_user_mode(regs))
		set_compat_ssbs_bit(regs);
	else if (user_mode(regs))
		set_ssbs_bit(regs);
}

/*
 * We store our current task in sp_el0, which is clobbered by userspace. Keep a
 * shadow copy so that we can restore this upon entry from userspace.
 *
 * This is *only* for exception entry from EL0, and is not valid until we
 * __switch_to() a user task.
 */
DEFINE_PER_CPU(struct task_struct *, __entry_task);

static void entry_task_switch(struct task_struct *next)
{
	__this_cpu_write(__entry_task, next);
}

/*
 * ARM erratum 1418040 handling, affecting the 32bit view of CNTVCT.
 * Ensure access is disabled when switching to a 32bit task, ensure
 * access is enabled when switching to a 64bit task.
 */
static void erratum_1418040_thread_switch(struct task_struct *next)
{
	if (!IS_ENABLED(CONFIG_ARM64_ERRATUM_1418040) ||
	    !this_cpu_has_cap(ARM64_WORKAROUND_1418040))
		return;

	if (is_compat_thread(task_thread_info(next)))
		sysreg_clear_set(cntkctl_el1, ARCH_TIMER_USR_VCT_ACCESS_EN, 0);
	else
		sysreg_clear_set(cntkctl_el1, 0, ARCH_TIMER_USR_VCT_ACCESS_EN);
}

static void erratum_1418040_new_exec(void)
{
	preempt_disable();
	erratum_1418040_thread_switch(current);
	preempt_enable();
}

/*
 * Thread switching.
 */
__notrace_funcgraph struct task_struct *__switch_to(struct task_struct *prev,
				struct task_struct *next)
{
	struct task_struct *last;

	fpsimd_thread_switch(next);
	tls_thread_switch(next);
	hw_breakpoint_thread_switch(next);
	contextidr_thread_switch(next);
	entry_task_switch(next);
	uao_thread_switch(next);
	ptrauth_thread_switch(next);
	ssbs_thread_switch(next);
	erratum_1418040_thread_switch(next);
	scs_overflow_check(next);

	/*
	 * Complete any pending TLB or cache maintenance on this CPU in case
	 * the thread migrates to a different CPU.
	 * This full barrier is also required by the membarrier system
	 * call.
	 */
	dsb(ish);

	/* the actual thread switch */
	last = cpu_switch_to(prev, next);

	return last;
}

unsigned long get_wchan(struct task_struct *p)
{
	struct stackframe frame;
	unsigned long stack_page, ret = 0;
	int count = 0;
	if (!p || p == current || p->state == TASK_RUNNING)
		return 0;

	stack_page = (unsigned long)try_get_task_stack(p);
	if (!stack_page)
		return 0;

	start_backtrace(&frame, thread_saved_fp(p), thread_saved_pc(p));

	do {
		if (unwind_frame(p, &frame))
			goto out;
		if (!in_sched_functions(frame.pc)) {
			ret = frame.pc;
			goto out;
		}
	} while (count ++ < 16);

out:
	put_task_stack(p);
	return ret;
}

unsigned long arch_align_stack(unsigned long sp)
{
	if (!(current->personality & ADDR_NO_RANDOMIZE) && randomize_va_space)
		sp -= get_random_int() & ~PAGE_MASK;
	return sp & ~0xf;
}

/*
 * Called from setup_new_exec() after (COMPAT_)SET_PERSONALITY.
 */
void arch_setup_new_exec(void)
{
	current->mm->context.flags = is_compat_task() ? MMCF_AARCH32 : 0;

	ptrauth_thread_init_user(current);
	erratum_1418040_new_exec();
}

#ifdef CONFIG_ARM64_TAGGED_ADDR_ABI
/*
 * Control the relaxed ABI allowing tagged user addresses into the kernel.
 */
static unsigned int tagged_addr_disabled;

long set_tagged_addr_ctrl(unsigned long arg)
{
	if (is_compat_task())
		return -EINVAL;
	if (arg & ~PR_TAGGED_ADDR_ENABLE)
		return -EINVAL;

	/*
	 * Do not allow the enabling of the tagged address ABI if globally
	 * disabled via sysctl abi.tagged_addr_disabled.
	 */
	if (arg & PR_TAGGED_ADDR_ENABLE && tagged_addr_disabled)
		return -EINVAL;

	update_thread_flag(TIF_TAGGED_ADDR, arg & PR_TAGGED_ADDR_ENABLE);

	return 0;
}

long get_tagged_addr_ctrl(void)
{
	if (is_compat_task())
		return -EINVAL;

	if (test_thread_flag(TIF_TAGGED_ADDR))
		return PR_TAGGED_ADDR_ENABLE;

	return 0;
}

/*
 * Global sysctl to disable the tagged user addresses support. This control
 * only prevents the tagged address ABI enabling via prctl() and does not
 * disable it for tasks that already opted in to the relaxed ABI.
 */
static int zero;
static int one = 1;

static struct ctl_table tagged_addr_sysctl_table[] = {
	{
		.procname	= "tagged_addr_disabled",
		.mode		= 0644,
		.data		= &tagged_addr_disabled,
		.maxlen		= sizeof(int),
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= &zero,
		.extra2		= &one,
	},
	{ }
};

static int __init tagged_addr_init(void)
{
	if (!register_sysctl("abi", tagged_addr_sysctl_table))
		return -EINVAL;
	return 0;
}

core_initcall(tagged_addr_init);
#endif	/* CONFIG_ARM64_TAGGED_ADDR_ABI */

asmlinkage void __sched arm64_preempt_schedule_irq(void)
{
	lockdep_assert_irqs_disabled();

	/*
	 * Preempting a task from an IRQ means we leave copies of PSTATE
	 * on the stack. cpufeature's enable calls may modify PSTATE, but
	 * resuming one of these preempted tasks would undo those changes.
	 *
	 * Only allow a task to be preempted once cpufeatures have been
	 * enabled.
	 */
	if (static_branch_likely(&arm64_const_caps_ready))
		preempt_schedule_irq();
}
