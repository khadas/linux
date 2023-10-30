// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/version.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/reboot.h>
#include <linux/memblock.h>
#include <linux/vmalloc.h>
#include <linux/arm-smccc.h>
#include <linux/memcontrol.h>
#include <linux/kthread.h>
#include <linux/amlogic/vmap_stack.h>
#include <linux/highmem.h>
#include <linux/delay.h>
#include <linux/sched/task_stack.h>
#ifdef CONFIG_KASAN
#include <linux/kasan.h>
#endif
#include <asm/tlbflush.h>
#include <asm/stacktrace.h>

#define DEBUG							0

#define D(format, args...)					\
	{ if (DEBUG)						\
		pr_info("VMAP:%s " format, __func__, ##args);	\
	}

#define E(format, args...)	pr_err("VMAP:%s " format, __func__, ##args)

static unsigned long stack_shrink_jiffies;
static unsigned char vmap_shrink_enable;
static atomic_t vmap_stack_size;
static atomic_t vmap_fault_count;
static atomic_t vmap_pre_handle_count;
static struct aml_vmap *avmap;
static atomic_t vmap_cache_flag;

#ifdef CONFIG_ARM64
DEFINE_PER_CPU(unsigned long [THREAD_SIZE / sizeof(long)], vmap_stack)
	__aligned(PAGE_SIZE);
#else
static unsigned long irq_stack1[(THREAD_SIZE / sizeof(long))]
				__aligned(THREAD_SIZE);
void *irq_stack[NR_CPUS] = {
	irq_stack1,	/* only assign 1st irq stack ,other need alloc */
};

static unsigned long vmap_stack1[(THREAD_SIZE / sizeof(long))]
				__aligned(THREAD_SIZE);

static void *vmap_stack[NR_CPUS] = {
	vmap_stack1,	/* only assign 1st vmap stack ,other need alloc */
};
#endif

void update_vmap_stack(int diff)
{
	atomic_add(diff, &vmap_stack_size);
}
EXPORT_SYMBOL(update_vmap_stack);

int get_vmap_stack_size(void)
{
	return atomic_read(&vmap_stack_size);
}
EXPORT_SYMBOL(get_vmap_stack_size);

#ifdef CONFIG_ARM
void notrace __setup_vmap_stack(unsigned long cpu)
{
	void *stack;
#define VMAP_MASK		(GFP_ATOMIC | __GFP_ZERO)
#ifdef CONFIG_THUMB2_KERNEL
#define TAG	"r"
#else
#define TAG	"I"
#endif
	stack = vmap_stack[cpu];
	if (!stack) {
		stack = (void *)__get_free_pages(VMAP_MASK, THREAD_SIZE_ORDER);
		WARN_ON(!stack);
		vmap_stack[cpu] = stack;
		irq_stack[cpu] = (void *)__get_free_pages(VMAP_MASK,
							  THREAD_SIZE_ORDER);
		WARN_ON(!irq_stack[cpu]);
	}

	pr_debug("cpu %ld, vmap stack:[%lx-%lx]\n",
		cpu, (unsigned long)stack,
		(unsigned long)stack + THREAD_START_SP);
	pr_debug("cpu %ld, irq  stack:[%lx-%lx]\n",
		cpu, (unsigned long)irq_stack[cpu],
		(unsigned long)irq_stack[cpu] + THREAD_START_SP);
	stack += THREAD_SIZE;
	stack -= sizeof(struct thread_info);
	/*
	 * reserve 24 byte for r0, lr, spsr, sp_svc and 8 bytes gap
	 */
	stack -= (24);
	asm volatile (
		"msr	cpsr_c, %1	\n"
		"mov	sp, %0		\n"
		"msr	cpsr_c, %2	\n"
		:
		: "r" (stack),
		  TAG(PSR_F_BIT | PSR_I_BIT | ABT_MODE),
		  TAG(PSR_F_BIT | PSR_I_BIT | SVC_MODE)
		: "memory", "cc"
	);
}

int on_vmap_irq_stack(unsigned long sp, int cpu)
{
	unsigned long sp_irq;

	sp_irq = (unsigned long)irq_stack[cpu];
	if ((sp & ~(THREAD_SIZE - 1)) == (sp_irq & ~(THREAD_SIZE - 1)))
		return 1;
	return 0;
}

unsigned long notrace irq_stack_entry(unsigned long sp)
{
	int cpu = raw_smp_processor_id();

	if (!on_vmap_irq_stack(sp, cpu)) {
		unsigned long sp_irq = (unsigned long)irq_stack[cpu];
		void *src, *dst;

		/*
		 * copy some data to irq stack
		 */
		src = current_thread_info();
		dst = (void *)(sp_irq + THREAD_INFO_OFFSET);
		memcpy(dst, src, offsetof(struct thread_info, cpu_context));
		sp_irq = (unsigned long)dst - 8;
		/*
		 * save start addr of the interrupted task's context
		 * used to back trace stack call from irq
		 * Note: sp_irq must be aligned to 16 bytes
		 */
		*((unsigned long *)sp_irq) = sp;
		return sp_irq;
	}
	return sp;
}

unsigned long notrace pmd_check(unsigned long addr, unsigned long far)
{
	unsigned int index;
	pgd_t *pgd, *pgd_k;
	pud_t *pud, *pud_k;
	pmd_t *pmd, *pmd_k;

	if (far < TASK_SIZE)
		return addr;

	index = pgd_index(far);

	pgd   = cpu_get_pgd() + index;
	pgd_k = init_mm.pgd + index;

	if (pgd_none(*pgd_k))
		goto bad_area;
	if (!pgd_present(*pgd))
		set_pgd(pgd, *pgd_k);

	pud   = pud_offset(pgd, far);
	pud_k = pud_offset(pgd_k, far);

	if (pud_none(*pud_k))
		goto bad_area;
	if (!pud_present(*pud))
		set_pud(pud, *pud_k);

	pmd   = pmd_offset(pud, far);
	pmd_k = pmd_offset(pud_k, far);

#ifdef CONFIG_ARM_LPAE
	/*
	 * Only one hardware entry per PMD with LPAE.
	 */
	index = 0;
#else
	/*
	 * On ARM one Linux PGD entry contains two hardware entries (see page
	 * tables layout in pgtable.h). We normally guarantee that we always
	 * fill both L1 entries. But create_mapping() doesn't follow the rule.
	 * It can create individual L1 entries, so here we have to call
	 * pmd_none() check for the entry really corresponded to address, not
	 * for the first of pair.
	 */
	index = (far >> SECTION_SHIFT) & 1;
#endif
	if (pmd_none(pmd_k[index]))
		goto bad_area;

	copy_pmd(pmd, pmd_k);
bad_area:
	return addr;
}
#endif

int is_vmap_addr(unsigned long addr)
{
	unsigned long start, end;

	start = (unsigned long)avmap->root_vm->addr;
	end   = (unsigned long)avmap->root_vm->addr + avmap->root_vm->size;
	if (addr >= start && addr < end)
		return 1;
	else
		return 0;
}

static struct page *get_vmap_cached_page(int *remain)
{
	unsigned long flags;
	struct page *page;

	spin_lock_irqsave(&avmap->page_lock, flags);
	if (unlikely(!avmap->cached_pages)) {
		spin_unlock_irqrestore(&avmap->page_lock, flags);
		pr_info("%s, no cached page.\n", __func__);
		return NULL;
	}
	page = list_first_entry(&avmap->list, struct page, lru);
	list_del(&page->lru);
	avmap->cached_pages--;
	*remain = avmap->cached_pages;
	spin_unlock_irqrestore(&avmap->page_lock, flags);

	return page;
}

struct page *check_pte_exist(unsigned long addr)
{
	struct mm_struct *mm;
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;

	mm = &init_mm;

	pgd = pgd_offset(mm, addr);

	if (pgd_none(*pgd))
		return NULL;

	if (pgd_bad(*pgd))
		return NULL;

	pud = pud_offset(pgd, addr);
	if (pud_none(*pud))
		return NULL;

	if (pud_bad(*pud))
		return NULL;

	pmd = pmd_offset(pud, addr);
	if (pmd_none(*pmd))
		return NULL;

	if (pmd_bad(*pmd))
		return NULL;

	pte = pte_offset_kernel(pmd, addr);
	if (pte_none(*pte))
		return NULL;
#ifdef CONFIG_ARM64
	return pte_page(*pte);
#elif defined(CONFIG_ARM)
	return pte_page(pte_val(*pte));
#else
	return NULL;	/* not supported */
#endif
}

static int vmap_mmu_set(struct page *page, unsigned long addr, int set)
{
	pgd_t *pgd = NULL;
	pud_t *pud = NULL;
	pmd_t *pmd = NULL;
	pte_t *pte = NULL;

	pgd = pgd_offset_k(addr);
	pud = pud_alloc(&init_mm, pgd, addr);
	if (!pud)
		goto nomem;

	if (pud_none(*pud)) {
		pmd = pmd_alloc(&init_mm, pud, addr);
		if (!pmd)
			goto nomem;
	}

	pmd = pmd_offset(pud, addr);
	if (pmd_none(*pmd)) {
		pte = pte_alloc_kernel(pmd, addr);
		if (!pte)
			goto nomem;
	}

	pte = pte_offset_kernel(pmd, addr);
	if (set)
		set_pte_at(&init_mm, addr, pte, mk_pte(page, PAGE_KERNEL));
	else
		pte_clear(&init_mm, addr, pte);
	flush_tlb_kernel_range(addr, addr + PAGE_SIZE);
#ifdef CONFIG_ARM64
	D("add:%lx, pgd:%px %llx, pmd:%px %llx, pte:%px %llx\n",
		addr, pgd, pgd_val(*pgd), pmd, pmd_val(*pmd),
		pte, pte_val(*pte));
#elif defined(CONFIG_ARM)
	D("add:%lx, pgd:%px %x, pmd:%px %x, pte:%px %x\n",
		addr, pgd, (unsigned int)pgd_val(*pgd),
		pmd, (unsigned int)pmd_val(*pmd),
		pte, pte_val(*pte));
#endif
	return 0;
nomem:
	E("allocation page table failed, G:%px, U:%px, M:%px, T:%px",
		pgd, pud, pmd, pte);
	return -ENOMEM;
}

static int stack_floor_page(unsigned long addr)
{
	unsigned long pos;

	pos = addr & (THREAD_SIZE - 1);
	/*
	 * stack address must align to THREAD_SIZE
	 */
	if (THREAD_SIZE_ORDER > 1)
		return pos < PAGE_SIZE;
	else
		return pos < (PAGE_SIZE / 4);
}

static int check_addr_up_flow(unsigned long addr)
{
	/*
	 * It's the first page of 4 contiguous virtual address
	 * rage(aligned to THREAD_SIZE) but next page of this
	 * addr is not mapped
	 */
	if (stack_floor_page(addr) && !check_pte_exist(addr + PAGE_SIZE))
		return 1;
	return 0;
}

static void dump_backtrace_entry(unsigned long ip, unsigned long fp,
				 unsigned long sp)
{
	unsigned long fp_size = 0;

#ifdef CONFIG_ARM64
	if (fp >= VMALLOC_START) {
		fp_size = *((unsigned long *)fp) - fp;
		/* fp cross IRQ or vmap stack */
		if (fp_size >= THREAD_SIZE)
			fp_size = 0;
	}
	pr_info("[%016lx+%4ld][<%px>] %pS\n",
		fp, fp_size, (void *)ip, (void *)ip);
#elif defined(CONFIG_ARM)
	if (fp >= TASK_SIZE) {
		fp_size = fp - sp + 4;
		/* fp cross IRQ or vmap stack */
		if (fp_size >= THREAD_SIZE)
			fp_size = 0;
	}
	pr_info("[%08lx+%4ld][<%px>] %pS\n",
		fp, fp_size, (void *)ip, (void *)ip);
#endif
}

static noinline void show_fault_stack(unsigned long addr, struct pt_regs *regs)
{
	struct stackframe frame = {};
	unsigned long sp;

#ifdef CONFIG_ARM64
	frame.fp = regs->regs[29];
	frame.prev_fp = addr;
	frame.pc = (unsigned long)regs->regs[30];
	sp = addr;
	frame.prev_type = STACK_TYPE_UNKNOWN;
#elif defined(CONFIG_ARM)
	frame.fp = regs->ARM_fp;
	frame.sp = regs->ARM_sp;
	frame.lr = regs->ARM_lr;
	frame.pc = (unsigned long)regs->uregs[15];
	sp = regs->ARM_sp;
#endif

	pr_info("Addr:%lx, Call trace:\n", addr);
#ifdef CONFIG_ARM64
	pr_info("[%016lx+%4ld][<%px>] %pS\n",
		addr, frame.fp - addr, (void *)regs->pc, (void *)regs->pc);
#elif defined(CONFIG_ARM)
	pr_info("[%08lx+%4ld][<%px>] %pS\n",
		addr, frame.fp - addr, (void *)regs->uregs[15],
		(void *)regs->uregs[15]);
#endif
	while (1) {
		int ret;

	#ifdef CONFIG_ARM64
		dump_backtrace_entry(frame.pc, frame.fp, sp);
		ret = unwind_frame(current, &frame);
	#elif defined(CONFIG_ARM)
		dump_backtrace_entry(frame.pc, frame.fp, frame.sp);
		ret = unwind_frame(&frame);
	#endif
		if (ret < 0)
			break;
	}
}

static void check_sp_fault_again(struct pt_regs *regs)
{
	unsigned long sp = 0, addr;
	struct page *page;
	int cache = 0;

#ifdef CONFIG_ARM
	sp = regs->ARM_sp;
#elif defined(CONFIG_ARM64)
	sp = regs->sp;
#endif
	addr = sp - sizeof(*regs);

	/*
	 * When we handle vmap stack fault, we are in pre-allcated
	 * per-cpu vmap stack. But if sp is near bottom of a page and we
	 * return to normal handler, sp may down grow to another page
	 * to cause a vmap fault again. So we need map next page for
	 * stack before page-fault happen.
	 *
	 * But we need check sp is really in vmap stack range.
	 */
	if (!is_vmap_addr(addr)) /* addr may in linear mapping */
		return;

	if (sp && ((addr & PAGE_MASK) != (sp & PAGE_MASK))) {
		/*
		 * will fault when we copy back context, so handle
		 * it first
		 */
		D("fault again, sp:%lx, addr:%lx\n", sp, addr);
		page = get_vmap_cached_page(&cache);
		WARN_ON(!page);
		vmap_mmu_set(page, addr, 1);
		update_vmap_stack(1);
	#ifndef CONFIG_KASAN
		if (THREAD_SIZE_ORDER > 1 && stack_floor_page(addr)) {
			E("task:%d %s, stack near overflow, addr:%lx\n",
				current->pid, current->comm, addr);
			show_fault_stack(addr, regs);
		}
	#endif

		/* cache is not enough */
		if (cache <= (VMAP_CACHE_PAGE / 2))
			atomic_inc(&vmap_cache_flag);

		D("map page:%5lx for addr:%lx\n", page_to_pfn(page), addr);
		atomic_inc(&vmap_pre_handle_count);
	#if DEBUG
		show_fault_stack(addr, regs);
	#endif
	}
}

/*
 * IRQ should *NEVER* been opened in this handler
 */
int handle_vmap_fault(unsigned long addr, unsigned int esr,
		      struct pt_regs *regs)
{
	struct page *page;
	int cache = 0;

	if (!is_vmap_addr(addr)) {
		check_sp_fault_again(regs);
		return -EINVAL;
	}

	D("addr:%lx, esr:%x, task:%5d %s\n",
		addr, esr, current->pid, current->comm);
#ifdef CONFIG_ARM64
	D("pc:%ps, %llx, lr:%ps, %llx, sp:%llx, %lx\n",
		(void *)regs->pc, regs->pc,
		(void *)regs->regs[30], regs->regs[30], regs->sp,
		current_stack_pointer);
#elif defined(CONFIG_ARM)
	D("pc:%ps, %lx, lr:%ps, %lx, sp:%lx, %lx\n",
		(void *)regs->uregs[15], regs->uregs[15],
		(void *)regs->uregs[14], regs->uregs[14], regs->uregs[13],
		current_stack_pointer);
#endif

	if (check_addr_up_flow(addr)) {
		E("address %lx out of range\n", addr);
	#ifdef CONFIG_ARM64
		E("PC is:%llx, %ps, LR is:%llx %ps\n",
			regs->pc, (void *)regs->pc,
			regs->regs[30], (void *)regs->regs[30]);
	#elif defined(CONFIG_ARM)
		E("PC is:%lx, %ps, LR is:%lx %ps\n",
			regs->uregs[15], (void *)regs->uregs[15],
			regs->uregs[14], (void *)regs->uregs[14]);
	#endif
		E("task:%d %s, stack:%px, %lx\n",
			current->pid, current->comm, current->stack,
			current_stack_pointer);
		show_fault_stack(addr, regs);
		check_sp_fault_again(regs);
		return -ERANGE;
	}

#ifdef CONFIG_ARM
	page = check_pte_exist(addr);
	if (page) {
		D("task:%d %s, page:%lx mapped for addr:%lx\n",
		  current->pid, current->comm, page_to_pfn(page), addr);
		check_sp_fault_again(regs);
		return -EINVAL;
	}
#endif

	/*
	 * allocate a new page for vmap
	 */
	page = get_vmap_cached_page(&cache);
	WARN_ON(!page);
	vmap_mmu_set(page, addr, 1);
	update_vmap_stack(1);
	if (THREAD_SIZE_ORDER > 1 && stack_floor_page(addr)) {
		E("task:%d %s, stack near overflow, addr:%lx\n",
			current->pid, current->comm, addr);
		show_fault_stack(addr, regs);
	}

	/* cache is not enough */
	if (cache <= (VMAP_CACHE_PAGE / 2))
		atomic_inc(&vmap_cache_flag);

	atomic_inc(&vmap_fault_count);
	D("map page:%5lx for addr:%lx\n", page_to_pfn(page), addr);
#if DEBUG
	show_fault_stack(addr, regs);
#endif
	return 0;
}
EXPORT_SYMBOL(handle_vmap_fault);

static unsigned long vmap_shrink_count(struct shrinker *s,
				  struct shrink_control *sc)
{
	return global_zone_page_state(NR_KERNEL_STACK_KB);
}

static int shrink_vm_stack(unsigned long low, unsigned long high)
{
	int pages = 0;
	struct page *page;

	for (; low < (high & PAGE_MASK); low += PAGE_SIZE) {
		page = vmalloc_to_page((const void *)low);
		vmap_mmu_set(page, low, 0);
		update_vmap_stack(-1);
		__free_page(page);
		pages++;
	}
	return pages;
}

void vmap_report_meminfo(struct seq_file *m)
{
	unsigned long kb = 1 << (PAGE_SHIFT - 10);
	unsigned long tmp1, tmp2, tmp3;

	tmp1 = kb * atomic_read(&vmap_stack_size);
	tmp2 = kb * atomic_read(&vmap_fault_count);
	tmp3 = kb * atomic_read(&vmap_pre_handle_count);

	seq_printf(m, "VmapStack:      %8ld kB\n", tmp1);
	seq_printf(m, "VmapFault:      %8ld kB\n", tmp2);
	seq_printf(m, "VmapPfault:     %8ld kB\n", tmp3);
}

static unsigned long get_task_stack_floor(unsigned long sp)
{
	unsigned long end;

	end = sp & (THREAD_SIZE - 1);
	while (sp > end) {
		if (!vmalloc_to_page((const void *)sp))
			break;
		sp -= PAGE_SIZE;
	}
	return PAGE_ALIGN(sp);
}

static unsigned long vmap_shrink_scan(struct shrinker *s,
				      struct shrink_control *sc)
{
	struct task_struct *tsk;
	unsigned long thread_sp;
	unsigned long stack_floor;
	unsigned long rem = 0;

	if (!vmap_shrink_enable)
		return 0;

	/*
	 * sleep for a while if shrink too ofen
	 */
	if (jiffies - stack_shrink_jiffies <= STACK_SHRINK_SLEEP)
		return 0;

	rcu_read_lock();
	for_each_process(tsk) {
		thread_sp = thread_saved_sp(tsk);
		stack_floor = get_task_stack_floor(thread_sp);
		/*
		 * Make sure selected task is sleeping
		 */
		D("r:%3ld, sp:[%lx-%lx], s:%5ld, tsk:%lx %d %s\n",
			rem, thread_sp, stack_floor,
			thread_sp - stack_floor,
			tsk->state, tsk->pid, tsk->comm);
		task_lock(tsk);
		if (tsk->state == TASK_RUNNING) {
			task_unlock(tsk);
			continue;
		}
		if (thread_sp - stack_floor >= STACK_SHRINK_THRESHOLD)
			rem += shrink_vm_stack(stack_floor, thread_sp);
		task_unlock(tsk);
	}
	rcu_read_unlock();
	stack_shrink_jiffies = jiffies;

	return rem;
}

static struct shrinker vmap_shrinker = {
	.scan_objects = vmap_shrink_scan,
	.count_objects = vmap_shrink_count,
	.seeks = DEFAULT_SEEKS * 16
};

/* FOR debug */
static unsigned long vmap_debug_jiff;

void aml_account_task_stack(struct task_struct *tsk, int account)
{
	unsigned long stack = (unsigned long)task_stack_page(tsk);
	struct page *first_page;

	if (unlikely(!is_vmap_addr(stack))) {
		/* stack get from kmalloc */
		first_page = virt_to_page((void *)stack);
		mod_zone_page_state(page_zone(first_page), NR_KERNEL_STACK_KB,
				    THREAD_SIZE / 1024 * account);

		update_vmap_stack(account * (THREAD_SIZE / PAGE_SIZE));
		mod_memcg_obj_state((void *)stack, MEMCG_KERNEL_STACK_KB,
				    account * (THREAD_SIZE / 1024));
		return;
	}
	stack += STACK_TOP_PAGE_OFF;
	first_page = vmalloc_to_page((void *)stack);
	mod_zone_page_state(page_zone(first_page), NR_KERNEL_STACK_KB,
			    THREAD_SIZE / 1024 * account);
	if (time_after(jiffies, vmap_debug_jiff + HZ * 5)) {
		int ratio, rem;

		vmap_debug_jiff = jiffies;
		ratio = ((get_vmap_stack_size() << (PAGE_SHIFT - 10)) * 10000) /
			global_zone_page_state(NR_KERNEL_STACK_KB);
		rem   = ratio % 100;
		D("STACK:%ld KB, vmap:%d KB, cached:%d KB, rate:%2d.%02d%%\n",
			global_zone_page_state(NR_KERNEL_STACK_KB),
			get_vmap_stack_size() << (PAGE_SHIFT - 10),
			avmap->cached_pages << (PAGE_SHIFT - 10),
			ratio / 100, rem);
	}
}

#ifdef CONFIG_KASAN
DEFINE_MUTEX(stack_shadow_lock);	/* For kasan */
static void check_and_map_stack_shadow(unsigned long addr)
{
	unsigned long shadow;
	struct page *page, *pages[2] = {};
	int ret;

	shadow = (unsigned long)kasan_mem_to_shadow((void *)addr);
	page   = check_pte_exist(shadow);
	if (page) {
		WARN(page_address(page) == (void *)kasan_early_shadow_page,
		     "bad pte, page:%px, %lx, addr:%lx\n",
		     page_address(page), page_to_pfn(page), addr);
		return;
	}
	shadow = shadow & PAGE_MASK;
	page   = alloc_page(GFP_KERNEL | __GFP_HIGHMEM |
			    __GFP_ZERO | __GFP_HIGH);
	if (!page) {
		WARN(!page,
		     "alloc page for addr:%lx, shadow:%lx fail\n",
		     addr, shadow);
		return;
	}
	pages[0] = page;
	ret = map_kernel_range_noflush(shadow, PAGE_SIZE, PAGE_KERNEL, pages);
	if (ret < 0) {
		pr_err("%s, map shadow:%lx failed:%d\n", __func__, shadow, ret);
		__free_page(page);
	}
}
#endif

void *aml_stack_alloc(int node, struct task_struct *tsk)
{
	unsigned long bitmap_no, raw_start;
	struct page *page;
	unsigned long addr, map_addr, flags;

	spin_lock_irqsave(&avmap->vmap_lock, flags);
	raw_start = avmap->start_bit;
	bitmap_no = find_next_zero_bit(avmap->bitmap, MAX_TASKS,
				       avmap->start_bit);
	avmap->start_bit = bitmap_no + 1; /* next idle address space */
	if (bitmap_no >= MAX_TASKS) {
		spin_unlock_irqrestore(&avmap->vmap_lock, flags);
		/*
		 * if vmap address space is full, we still need to try
		 * to get stack from kmalloc
		 */
		addr = __get_free_pages(GFP_KERNEL, THREAD_SIZE_ORDER);
		E("BITMAP FULL, kmalloc task stack:%lx\n", addr);
		return (void *)addr;
	}
	bitmap_set(avmap->bitmap, bitmap_no, 1);
	spin_unlock_irqrestore(&avmap->vmap_lock, flags);

	page = alloc_page(THREADINFO_GFP | __GFP_ZERO | __GFP_HIGHMEM);
	if (!page) {
		spin_lock_irqsave(&avmap->vmap_lock, flags);
		bitmap_clear(avmap->bitmap, bitmap_no, 1);
		spin_unlock_irqrestore(&avmap->vmap_lock, flags);
		E("allocation page failed\n");
		return NULL;
	}
	/*
	 * map first page only
	 */
	addr = (unsigned long)avmap->root_vm->addr + THREAD_SIZE * bitmap_no;
	map_addr = addr + STACK_TOP_PAGE_OFF;
	vmap_mmu_set(page, map_addr, 1);
	update_vmap_stack(1);
#ifdef CONFIG_KASAN
	/* 2 thread stack can be a single shadow page, we need use lock */
	mutex_lock(&stack_shadow_lock);
	check_and_map_stack_shadow(addr);
	mutex_unlock(&stack_shadow_lock);
#endif

	D("bit idx:%5ld, start:%5ld, addr:%lx, page:%lx\n",
		bitmap_no, raw_start, addr, page_to_pfn(page));

	return (void *)addr;
}

void aml_stack_free(struct task_struct *tsk)
{
	unsigned long stack = (unsigned long)tsk->stack;
	unsigned long addr, bitmap_no;
	struct page *page;
	unsigned long flags;

	if (unlikely(!is_vmap_addr(stack))) {
		/* stack get from kmalloc */
		free_pages(stack, THREAD_SIZE_ORDER);
		return;
	}

	addr = stack + STACK_TOP_PAGE_OFF;
	for (; addr >= stack; addr -= PAGE_SIZE) {
		page = vmalloc_to_page((const void *)addr);
		if (!page)
			break;
	#ifdef CONFIG_KASAN
		kasan_unpoison_shadow((void *)addr, PAGE_SIZE);
	#endif
		vmap_mmu_set(page, addr, 0);
		/* supplement for stack page cache first */
		spin_lock_irqsave(&avmap->page_lock, flags);
		if (avmap->cached_pages < VMAP_CACHE_PAGE) {
			list_add_tail(&page->lru, &avmap->list);
			avmap->cached_pages++;
			spin_unlock_irqrestore(&avmap->page_lock, flags);
			clear_highpage(page);	/* clear for next use */
		} else {
			spin_unlock_irqrestore(&avmap->page_lock, flags);
			__free_page(page);
		}
		update_vmap_stack(-1);
	}
	bitmap_no = (stack - (unsigned long)avmap->root_vm->addr) / THREAD_SIZE;
	spin_lock_irqsave(&avmap->vmap_lock, flags);
	bitmap_clear(avmap->bitmap, bitmap_no, 1);
	if (bitmap_no < avmap->start_bit)
		avmap->start_bit = bitmap_no;
	spin_unlock_irqrestore(&avmap->vmap_lock, flags);
}

/*
 * page cache maintain task for vmap
 */
static int vmap_task(void *data)
{
	struct page *page;
	struct list_head head;
	int i, cnt;
	unsigned long flags;
	struct aml_vmap *v = (struct aml_vmap *)data;

	set_user_nice(current, -19);
	while (1) {
		if (kthread_should_stop())
			break;

		if (!atomic_read(&vmap_cache_flag)) {
			msleep_interruptible(20);
			continue;
		}
		spin_lock_irqsave(&v->page_lock, flags);
		cnt = v->cached_pages;
		spin_unlock_irqrestore(&v->page_lock, flags);
		if (cnt >= VMAP_CACHE_PAGE) {
			D("cache full cnt:%d\n", cnt);
			continue;
		}

		INIT_LIST_HEAD(&head);
		for (i = 0; i < VMAP_CACHE_PAGE - cnt; i++) {
			page = alloc_page(GFP_KERNEL | __GFP_HIGHMEM |
					  __GFP_ZERO | __GFP_HIGH);
			if (!page) {
				E("get page failed, allocated:%d, cnt:%d\n",
				  i, cnt);
				break;
			}
			list_add(&page->lru, &head);
		}
		spin_lock_irqsave(&v->page_lock, flags);
		list_splice(&head, &v->list);
		v->cached_pages += i;
		spin_unlock_irqrestore(&v->page_lock, flags);
		atomic_set(&vmap_cache_flag, 0);
		E("add %d pages, cnt:%d\n", i, cnt);
	}
	return 0;
}

int __init start_thread_work(void)
{
	kthread_run(vmap_task, avmap, "vmap_thread");
	return 0;
}
arch_initcall(start_thread_work);

void __init thread_stack_cache_init(void)
{
	int i;
	struct page *page;
#ifdef CONFIG_KASAN
	unsigned long align, size;
#endif

	page = alloc_pages(GFP_KERNEL | __GFP_HIGHMEM, VMAP_CACHE_PAGE_ORDER);
	if (!page)
		return;

	avmap = kzalloc(sizeof(*avmap), GFP_KERNEL);
	if (!avmap) {
		__free_pages(page, VMAP_CACHE_PAGE_ORDER);
		return;
	}

	avmap->bitmap = kzalloc(MAX_TASKS / 8, GFP_KERNEL);
	if (!avmap->bitmap) {
		__free_pages(page, VMAP_CACHE_PAGE_ORDER);
		kfree(avmap);
		return;
	}
	pr_info("%s, vmap:%px, bitmap:%px, cache page:%lx\n",
		__func__, avmap, avmap->bitmap, page_to_pfn(page));
#ifdef CONFIG_KASAN
	align = PGDIR_SIZE << KASAN_SHADOW_SCALE_SHIFT;
	size  = VM_STACK_AREA_SIZE;
	size  = ALIGN(size, align);
	avmap->root_vm = __get_vm_area_node(size, align, VM_NO_GUARD,
					    VMALLOC_START, VMALLOC_END,
					    NUMA_NO_NODE, GFP_KERNEL,
					    __builtin_return_address(0));
	WARN(!avmap->root_vm, "alloc vmap area %lx failed\n", size);
	if (avmap->root_vm) {
		unsigned long s, e;

		s = (unsigned long)kasan_mem_to_shadow(avmap->root_vm->addr);
		e = (unsigned long)avmap->root_vm->addr + size;
		e = (unsigned long)kasan_mem_to_shadow((void *)e);
		pr_info("%s, s:%lx, e:%lx, size:%lx\n", __func__, s, e, size);
		clear_pgds(s, e);
	}
#else
	avmap->root_vm = __get_vm_area_node(VM_STACK_AREA_SIZE,
					    VMAP_ALIGN,
					    0, VMAP_ADDR_START, VMAP_ADDR_END,
					    NUMA_NO_NODE, GFP_KERNEL,
					    __builtin_return_address(0));
#endif
	if (!avmap->root_vm) {
		__free_pages(page, VMAP_CACHE_PAGE_ORDER);
		kfree(avmap->bitmap);
		kfree(avmap);
		return;
	}
	pr_info("%s, allocation vm area:%px, addr:%px, size:%lx\n", __func__,
		avmap->root_vm, avmap->root_vm->addr,
		avmap->root_vm->size);

	INIT_LIST_HEAD(&avmap->list);
	spin_lock_init(&avmap->page_lock);
	spin_lock_init(&avmap->vmap_lock);

	for (i = 0; i < VMAP_CACHE_PAGE; i++) {
		list_add(&page->lru, &avmap->list);
		page++;
	}
	avmap->cached_pages = VMAP_CACHE_PAGE;

#ifdef CONFIG_ARM64
	/*
	 * the for_each_possible_cpu() traversed index of cpu does not out of bounds.
	 */
	// coverity[Out-of-bounds read]
	for_each_possible_cpu(i) {
		unsigned long addr;

		addr = (unsigned long)per_cpu_ptr(vmap_stack, i);
		pr_debug("cpu %d, vmap_stack:[%lx-%lx]\n",
			i, addr, addr + THREAD_START_SP);
		addr = (unsigned long)per_cpu_ptr(irq_stack, i);
		pr_debug("cpu %d, irq_stack: [%lx-%lx]\n",
			i, addr, addr + THREAD_START_SP);
	}
#endif
	register_shrinker(&vmap_shrinker);
}
