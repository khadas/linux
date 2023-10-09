// SPDX-License-Identifier: GPL-2.0-only
/*
 *  linux/arch/arm/kernel/process.c
 *
 *  Copyright (C) 1996-2000 Russell King - Converted to ARM.
 *  Original Copyright (C) 1995  Linus Torvalds
 */
#include <stdarg.h>

#include <linux/export.h>
#include <linux/sched.h>
#include <linux/sched/debug.h>
#include <linux/sched/task.h>
#include <linux/sched/task_stack.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/stddef.h>
#include <linux/unistd.h>
#include <linux/user.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/elfcore.h>
#include <linux/pm.h>
#include <linux/tick.h>
#include <linux/utsname.h>
#include <linux/uaccess.h>
#include <linux/random.h>
#include <linux/hw_breakpoint.h>
#include <linux/leds.h>

#include <asm/processor.h>
#include <asm/thread_notify.h>
#include <asm/stacktrace.h>
#include <asm/system_misc.h>
#include <asm/mach/time.h>
#include <asm/tls.h>
#include <asm/vdso.h>

#ifdef CONFIG_AMLOGIC_DEBUG_LOCKUP
#include <linux/amlogic/debug_lockup.h>
#endif

#if defined(CONFIG_STACKPROTECTOR) && !defined(CONFIG_STACKPROTECTOR_PER_TASK)
#include <linux/stackprotector.h>
unsigned long __stack_chk_guard __read_mostly;
EXPORT_SYMBOL(__stack_chk_guard);
#endif

#ifdef CONFIG_AMLOGIC_MODIFY
#include <linux/amlogic/secmon.h>
#endif

static const char *processor_modes[] __maybe_unused = {
  "USER_26", "FIQ_26" , "IRQ_26" , "SVC_26" , "UK4_26" , "UK5_26" , "UK6_26" , "UK7_26" ,
  "UK8_26" , "UK9_26" , "UK10_26", "UK11_26", "UK12_26", "UK13_26", "UK14_26", "UK15_26",
  "USER_32", "FIQ_32" , "IRQ_32" , "SVC_32" , "UK4_32" , "UK5_32" , "MON_32" , "ABT_32" ,
  "UK8_32" , "UK9_32" , "HYP_32", "UND_32" , "UK12_32", "UK13_32", "UK14_32", "SYS_32"
};

static const char *isa_modes[] __maybe_unused = {
  "ARM" , "Thumb" , "Jazelle", "ThumbEE"
};

/*
 * This is our default idle handler.
 */

void (*arm_pm_idle)(void);

/*
 * Called from the core idle loop.
 */

void arch_cpu_idle(void)
{
	if (arm_pm_idle)
		arm_pm_idle();
	else
		cpu_do_idle();
	local_irq_enable();
}

void arch_cpu_idle_prepare(void)
{
	local_fiq_enable();
}

void arch_cpu_idle_enter(void)
{
#ifdef CONFIG_AMLOGIC_DEBUG_LOCKUP
	__arch_cpu_idle_enter();
#endif
	ledtrig_cpu(CPU_LED_IDLE_START);
#ifdef CONFIG_PL310_ERRATA_769419
	wmb();
#endif
}

void arch_cpu_idle_exit(void)
{
	ledtrig_cpu(CPU_LED_IDLE_END);
#ifdef CONFIG_AMLOGIC_DEBUG_LOCKUP
	__arch_cpu_idle_exit();
#endif
}

/*
 * dump a block of kernel memory from around the given address
 */
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

#ifdef CONFIG_AMLOGIC_MODIFY
	/*
	 * Treating data in general purpose register as an address
	 * and dereferencing it is quite a dangerous behaviour,
	 * especially when it is an address belonging to secure
	 * region or ioremap region, which can lead to external
	 * abort on non-linefetch and can not be protected by
	 * probe_kernel_address.
	 * We need more strict filtering rules
	 */

#ifdef CONFIG_AMLOGIC_SEC
	/*
	 * filter out secure monitor region
	 */
	if (addr <= (unsigned long)high_memory)
		if (within_secmon_region(addr))
			return;
#endif

	/*
	 * filter out ioremap region
	 */
	if ((addr >= VMALLOC_START) && (addr <= VMALLOC_END))
		if (!pfn_valid(vmalloc_to_pfn((void *)addr)))
			return;
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
		for (j = 0; j < 8; j++) {
			u32	data;

			if (probe_kernel_address(p, data))
				pr_cont(" ********");
			else
				pr_cont(" %08x", data);
			++p;
		}
		pr_cont("\n");
	}
}

#ifdef CONFIG_AMLOGIC_USER_FAULT
/*
 * dump a block of user memory from around the given address
 */
static void show_user_data(unsigned long addr, int nbytes, const char *name)
{
	int	i, j;
	int	nlines;
	u32	*p;
	char	buf[128] = {0};
	int	len = 0;

	if (!access_ok((void *)addr, nbytes))
		return;

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
		len = 0;
		len += snprintf(buf + len, sizeof(buf) - len,
						"%04lx ", (unsigned long)p & 0xffff);
		for (j = 0; j < 8; j++) {
			int bad;
			unsigned char data[4];

			bad = __copy_from_user_inatomic(data, (const void *)p, sizeof(u32));
			if (bad)
				len += snprintf(buf + len, sizeof(buf) - len, " ********");
			else
				len += snprintf(buf + len, sizeof(buf) - len,
								" %08x", *(u32 *)data);
			++p;
		}
		pr_info("%s\n", buf);
	}
}
#endif /* CONFIG_AMLOGIC_USER_FAULT */

static void show_extra_register_data(struct pt_regs *regs, int nbytes)
{
	mm_segment_t fs;

	fs = get_fs();
	set_fs(KERNEL_DS);
	show_data(regs->ARM_pc - nbytes, nbytes * 2, "PC");
	show_data(regs->ARM_lr - nbytes, nbytes * 2, "LR");
	show_data(regs->ARM_sp - nbytes, nbytes * 2, "SP");
	show_data(regs->ARM_ip - nbytes, nbytes * 2, "IP");
	show_data(regs->ARM_fp - nbytes, nbytes * 2, "FP");
	show_data(regs->ARM_r0 - nbytes, nbytes * 2, "R0");
	show_data(regs->ARM_r1 - nbytes, nbytes * 2, "R1");
	show_data(regs->ARM_r2 - nbytes, nbytes * 2, "R2");
	show_data(regs->ARM_r3 - nbytes, nbytes * 2, "R3");
	show_data(regs->ARM_r4 - nbytes, nbytes * 2, "R4");
	show_data(regs->ARM_r5 - nbytes, nbytes * 2, "R5");
	show_data(regs->ARM_r6 - nbytes, nbytes * 2, "R6");
	show_data(regs->ARM_r7 - nbytes, nbytes * 2, "R7");
	show_data(regs->ARM_r8 - nbytes, nbytes * 2, "R8");
	show_data(regs->ARM_r9 - nbytes, nbytes * 2, "R9");
	show_data(regs->ARM_r10 - nbytes, nbytes * 2, "R10");
	set_fs(fs);
}

#ifdef CONFIG_AMLOGIC_USER_FAULT
static void show_user_extra_register_data(struct pt_regs *regs, int nbytes)
{
	show_user_data(regs->ARM_pc - nbytes, nbytes * 2, "PC");
	show_user_data(regs->ARM_lr - nbytes, nbytes * 2, "LR");
	show_user_data(regs->ARM_sp - nbytes, nbytes * 2, "SP");
	show_user_data(regs->ARM_ip - nbytes, nbytes * 2, "IP");
	show_user_data(regs->ARM_fp - nbytes, nbytes * 2, "FP");
	show_user_data(regs->ARM_r0 - nbytes, nbytes * 2, "R0");
	show_user_data(regs->ARM_r1 - nbytes, nbytes * 2, "R1");
	show_user_data(regs->ARM_r2 - nbytes, nbytes * 2, "R2");
	show_user_data(regs->ARM_r3 - nbytes, nbytes * 2, "R3");
	show_user_data(regs->ARM_r4 - nbytes, nbytes * 2, "R4");
	show_user_data(regs->ARM_r5 - nbytes, nbytes * 2, "R5");
	show_user_data(regs->ARM_r6 - nbytes, nbytes * 2, "R6");
	show_user_data(regs->ARM_r7 - nbytes, nbytes * 2, "R7");
	show_user_data(regs->ARM_r8 - nbytes, nbytes * 2, "R8");
	show_user_data(regs->ARM_r9 - nbytes, nbytes * 2, "R9");
	show_user_data(regs->ARM_r10 - nbytes, nbytes * 2, "R10");
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
			pr_cont("%s", p);
		} else {
			pr_cont(" get file path failed\n");
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
				pr_cont("[stack:%d]", tid);
			}
			goto done;
		}

		if (vma_get_anon_name(vma))
			pr_cont("[anon]");
	}

done:
	if (name)
		pr_cont("%s", name);
	pr_cont("\n");
}

static void show_vmalloc_pfn(struct pt_regs *regs)
{
	int i;
	struct page *page;

	for (i = 0; i < 16; i++) {
		if (is_vmalloc_or_module_addr((void *)regs->uregs[i])) {
			page = vmalloc_to_page((void *)regs->uregs[i]);
			if (!page)
				continue;
			pr_info("R%-2d : %08lx, PFN:%5lx\n",
				i, regs->uregs[i], page_to_pfn(page));
		}
	}
}
#endif /* CONFIG_AMLOGIC_USER_FAULT */

void __show_regs(struct pt_regs *regs)
{
	unsigned long flags;
	char buf[64];
#ifndef CONFIG_CPU_V7M
	unsigned int domain, fs;
#ifdef CONFIG_CPU_SW_DOMAIN_PAN
	/*
	 * Get the domain register for the parent context. In user
	 * mode, we don't save the DACR, so lets use what it should
	 * be. For other modes, we place it after the pt_regs struct.
	 */
	if (user_mode(regs)) {
		domain = DACR_UACCESS_ENABLE;
		fs = get_fs();
	} else {
		domain = to_svc_pt_regs(regs)->dacr;
		fs = to_svc_pt_regs(regs)->addr_limit;
	}
#else
	domain = get_domain();
	fs = get_fs();
#endif
#endif

	show_regs_print_info(KERN_DEFAULT);

	printk("PC is at %pS\n", (void *)instruction_pointer(regs));
	printk("LR is at %pS\n", (void *)regs->ARM_lr);
#ifdef CONFIG_AMLOGIC_USER_FAULT
	if (user_mode(regs)) {
		show_vma(current->mm, instruction_pointer(regs));
		show_vma(current->mm, regs->ARM_lr);
	}
#endif
	printk("pc : [<%08lx>]    lr : [<%08lx>]    psr: %08lx\n",
	       regs->ARM_pc, regs->ARM_lr, regs->ARM_cpsr);
	printk("sp : %08lx  ip : %08lx  fp : %08lx\n",
	       regs->ARM_sp, regs->ARM_ip, regs->ARM_fp);
	printk("r10: %08lx  r9 : %08lx  r8 : %08lx\n",
		regs->ARM_r10, regs->ARM_r9,
		regs->ARM_r8);
	printk("r7 : %08lx  r6 : %08lx  r5 : %08lx  r4 : %08lx\n",
		regs->ARM_r7, regs->ARM_r6,
		regs->ARM_r5, regs->ARM_r4);
	printk("r3 : %08lx  r2 : %08lx  r1 : %08lx  r0 : %08lx\n",
		regs->ARM_r3, regs->ARM_r2,
		regs->ARM_r1, regs->ARM_r0);

	flags = regs->ARM_cpsr;
	buf[0] = flags & PSR_N_BIT ? 'N' : 'n';
	buf[1] = flags & PSR_Z_BIT ? 'Z' : 'z';
	buf[2] = flags & PSR_C_BIT ? 'C' : 'c';
	buf[3] = flags & PSR_V_BIT ? 'V' : 'v';
	buf[4] = '\0';

#ifdef CONFIG_AMLOGIC_USER_FAULT
	show_vmalloc_pfn(regs);
#endif

#ifndef CONFIG_CPU_V7M
	{
		const char *segment;

		if ((domain & domain_mask(DOMAIN_USER)) ==
		    domain_val(DOMAIN_USER, DOMAIN_NOACCESS))
			segment = "none";
		else if (fs == KERNEL_DS)
			segment = "kernel";
		else
			segment = "user";

		printk("Flags: %s  IRQs o%s  FIQs o%s  Mode %s  ISA %s  Segment %s\n",
			buf, interrupts_enabled(regs) ? "n" : "ff",
			fast_interrupts_enabled(regs) ? "n" : "ff",
			processor_modes[processor_mode(regs)],
			isa_modes[isa_mode(regs)], segment);
	}
#else
	printk("xPSR: %08lx\n", regs->ARM_cpsr);
#endif

#ifdef CONFIG_CPU_CP15
	{
		unsigned int ctrl;

		buf[0] = '\0';
#ifdef CONFIG_CPU_CP15_MMU
		{
			unsigned int transbase;
			asm("mrc p15, 0, %0, c2, c0\n\t"
			    : "=r" (transbase));
			snprintf(buf, sizeof(buf), "  Table: %08x  DAC: %08x",
				transbase, domain);
		}
#endif
		asm("mrc p15, 0, %0, c1, c0\n" : "=r" (ctrl));

		printk("Control: %08x%s\n", ctrl, buf);
	}
#endif

#ifdef CONFIG_AMLOGIC_USER_FAULT
	if (!user_mode(regs))
#endif
		show_extra_register_data(regs, 128);
#ifdef CONFIG_AMLOGIC_USER_FAULT
	else
		show_user_extra_register_data(regs, 128);
#endif
}

void show_regs(struct pt_regs * regs)
{
	__show_regs(regs);
	dump_stack();
}

ATOMIC_NOTIFIER_HEAD(thread_notify_head);

EXPORT_SYMBOL_GPL(thread_notify_head);

/*
 * Free current thread data structures etc..
 */
void exit_thread(struct task_struct *tsk)
{
	thread_notify(THREAD_NOTIFY_EXIT, task_thread_info(tsk));
}

void flush_thread(void)
{
	struct thread_info *thread = current_thread_info();
	struct task_struct *tsk = current;

	flush_ptrace_hw_breakpoint(tsk);

	memset(thread->used_cp, 0, sizeof(thread->used_cp));
	memset(&tsk->thread.debug, 0, sizeof(struct debug_info));
	memset(&thread->fpstate, 0, sizeof(union fp_state));

	flush_tls();

	thread_notify(THREAD_NOTIFY_FLUSH, thread);
}

void release_thread(struct task_struct *dead_task)
{
}

asmlinkage void ret_from_fork(void) __asm__("ret_from_fork");

int
copy_thread_tls(unsigned long clone_flags, unsigned long stack_start,
	    unsigned long stk_sz, struct task_struct *p, unsigned long tls)
{
	struct thread_info *thread = task_thread_info(p);
	struct pt_regs *childregs = task_pt_regs(p);

	memset(&thread->cpu_context, 0, sizeof(struct cpu_context_save));

#ifdef CONFIG_CPU_USE_DOMAINS
	/*
	 * Copy the initial value of the domain access control register
	 * from the current thread: thread->addr_limit will have been
	 * copied from the current thread via setup_thread_stack() in
	 * kernel/fork.c
	 */
	thread->cpu_domain = get_domain();
#endif

	if (likely(!(p->flags & PF_KTHREAD))) {
		*childregs = *current_pt_regs();
		childregs->ARM_r0 = 0;
		if (stack_start)
			childregs->ARM_sp = stack_start;
	} else {
		memset(childregs, 0, sizeof(struct pt_regs));
		thread->cpu_context.r4 = stk_sz;
		thread->cpu_context.r5 = stack_start;
		childregs->ARM_cpsr = SVC_MODE;
	}
	thread->cpu_context.pc = (unsigned long)ret_from_fork;
	thread->cpu_context.sp = (unsigned long)childregs;

	clear_ptrace_hw_breakpoint(p);

	if (clone_flags & CLONE_SETTLS)
		thread->tp_value[0] = tls;
	thread->tp_value[1] = get_tpuser();

	thread_notify(THREAD_NOTIFY_COPY, thread);

#ifdef CONFIG_STACKPROTECTOR_PER_TASK
	thread->stack_canary = p->stack_canary;
#endif

	return 0;
}

/*
 * Fill in the task's elfregs structure for a core dump.
 */
int dump_task_regs(struct task_struct *t, elf_gregset_t *elfregs)
{
	elf_core_copy_regs(elfregs, task_pt_regs(t));
	return 1;
}

/*
 * fill in the fpe structure for a core dump...
 */
int dump_fpu (struct pt_regs *regs, struct user_fp *fp)
{
	struct thread_info *thread = current_thread_info();
	int used_math = thread->used_cp[1] | thread->used_cp[2];

	if (used_math)
		memcpy(fp, &thread->fpstate.soft, sizeof (*fp));

	return used_math != 0;
}
EXPORT_SYMBOL(dump_fpu);

unsigned long get_wchan(struct task_struct *p)
{
	struct stackframe frame;
	unsigned long stack_page;
	int count = 0;
	if (!p || p == current || p->state == TASK_RUNNING)
		return 0;

	frame.fp = thread_saved_fp(p);
	frame.sp = thread_saved_sp(p);
	frame.lr = 0;			/* recovered from the stack */
	frame.pc = thread_saved_pc(p);
	stack_page = (unsigned long)task_stack_page(p);
	do {
		if (frame.sp < stack_page ||
		    frame.sp >= stack_page + THREAD_SIZE ||
		    unwind_frame(&frame) < 0)
			return 0;
		if (!in_sched_functions(frame.pc))
			return frame.pc;
	} while (count ++ < 16);
	return 0;
}

#ifdef CONFIG_MMU
#ifdef CONFIG_KUSER_HELPERS
/*
 * The vectors page is always readable from user space for the
 * atomic helpers. Insert it into the gate_vma so that it is visible
 * through ptrace and /proc/<pid>/mem.
 */
static struct vm_area_struct gate_vma;

static int __init gate_vma_init(void)
{
	vma_init(&gate_vma, NULL);
	gate_vma.vm_page_prot = PAGE_READONLY_EXEC;
	gate_vma.vm_start = 0xffff0000;
	gate_vma.vm_end	= 0xffff0000 + PAGE_SIZE;
	gate_vma.vm_flags = VM_READ | VM_EXEC | VM_MAYREAD | VM_MAYEXEC;
	return 0;
}
arch_initcall(gate_vma_init);

struct vm_area_struct *get_gate_vma(struct mm_struct *mm)
{
	return &gate_vma;
}

int in_gate_area(struct mm_struct *mm, unsigned long addr)
{
	return (addr >= gate_vma.vm_start) && (addr < gate_vma.vm_end);
}

int in_gate_area_no_mm(unsigned long addr)
{
	return in_gate_area(NULL, addr);
}
#define is_gate_vma(vma)	((vma) == &gate_vma)
#else
#define is_gate_vma(vma)	0
#endif

const char *arch_vma_name(struct vm_area_struct *vma)
{
	return is_gate_vma(vma) ? "[vectors]" : NULL;
}

/* If possible, provide a placement hint at a random offset from the
 * stack for the sigpage and vdso pages.
 */
static unsigned long sigpage_addr(const struct mm_struct *mm,
				  unsigned int npages)
{
	unsigned long offset;
	unsigned long first;
	unsigned long last;
	unsigned long addr;
	unsigned int slots;

	first = PAGE_ALIGN(mm->start_stack);

	last = TASK_SIZE - (npages << PAGE_SHIFT);

	/* No room after stack? */
	if (first > last)
		return 0;

	/* Just enough room? */
	if (first == last)
		return first;

	slots = ((last - first) >> PAGE_SHIFT) + 1;

	offset = get_random_int() % slots;

	addr = first + (offset << PAGE_SHIFT);

	return addr;
}

static struct page *signal_page;
extern struct page *get_signal_page(void);

static int sigpage_mremap(const struct vm_special_mapping *sm,
		struct vm_area_struct *new_vma)
{
	current->mm->context.sigpage = new_vma->vm_start;
	return 0;
}

static const struct vm_special_mapping sigpage_mapping = {
	.name = "[sigpage]",
	.pages = &signal_page,
	.mremap = sigpage_mremap,
};

int arch_setup_additional_pages(struct linux_binprm *bprm, int uses_interp)
{
	struct mm_struct *mm = current->mm;
	struct vm_area_struct *vma;
	unsigned long npages;
	unsigned long addr;
	unsigned long hint;
	int ret = 0;

	if (!signal_page)
		signal_page = get_signal_page();
	if (!signal_page)
		return -ENOMEM;

	npages = 1; /* for sigpage */
	npages += vdso_total_pages;

	if (down_write_killable(&mm->mmap_sem))
		return -EINTR;
	hint = sigpage_addr(mm, npages);
	addr = get_unmapped_area(NULL, hint, npages << PAGE_SHIFT, 0, 0);
	if (IS_ERR_VALUE(addr)) {
		ret = addr;
		goto up_fail;
	}

	vma = _install_special_mapping(mm, addr, PAGE_SIZE,
		VM_READ | VM_EXEC | VM_MAYREAD | VM_MAYWRITE | VM_MAYEXEC,
		&sigpage_mapping);

	if (IS_ERR(vma)) {
		ret = PTR_ERR(vma);
		goto up_fail;
	}

	mm->context.sigpage = addr;

	/* Unlike the sigpage, failure to install the vdso is unlikely
	 * to be fatal to the process, so no error check needed
	 * here.
	 */
	arm_install_vdso(mm, addr + PAGE_SIZE);

 up_fail:
	up_write(&mm->mmap_sem);
	return ret;
}
#endif
