/*
 * Hibernation support specific for ARM
 *
 * Derived from work on ARM hibernation support by:
 *
 * Ubuntu project, hibernation support for mach-dove
 * Copyright (C) 2010 Nokia Corporation (Hiroshi Doyu)
 * Copyright (C) 2010 Texas Instruments, Inc. (Teerth Reddy et al.)
 *  https://lkml.org/lkml/2010/6/18/4
 *  https://lists.linux-foundation.org/pipermail/linux-pm/2010-June/027422.html
 *  https://patchwork.kernel.org/patch/96442/
 *
 * Copyright (C) 2006 Rafael J. Wysocki <rjw@sisk.pl>
 *
 * License terms: GNU General Public License (GPL) version 2
 */

#include <linux/mm.h>
#include <linux/suspend.h>
#include <asm/system_misc.h>
#include <asm/pgtable.h>
#include <asm/suspend.h>
#include <asm/memory.h>
#include <asm/tlbflush.h>
#include <linux/interrupt.h>
#include <asm/cacheflush.h>
#include "arch_hib.h"

int pfn_is_nosave(unsigned long pfn)
{
	unsigned long nosave_begin_pfn = __pa(&__nosave_begin) >> PAGE_SHIFT;
	unsigned long nosave_end_pfn =
		PAGE_ALIGN(__pa(&__nosave_end)) >> PAGE_SHIFT;
	return (pfn >= nosave_begin_pfn) && (pfn < nosave_end_pfn);
}

void notrace save_processor_state(void)
{
	WARN_ON(num_online_cpus() != 1);
	local_fiq_disable();
}

void notrace restore_processor_state(void)
{
	local_fiq_enable();
}

typedef void (*phys_reset_t)(unsigned long);
typedef void (*phys_cache_func_t)(void);

static u64 hib_stack[PAGE_SIZE/sizeof(u64)] __nosavedata;

static void __hib_restart(unsigned long addr)
{
	phys_reset_t phys_reset;
	phys_cache_func_t phys_cache_func;
	setup_mm_for_reboot();

	/* Clean and invalidate caches */
	phys_cache_func =
		(phys_cache_func_t)(unsigned long)virt_to_phys(flush_cache_all);
	phys_cache_func();

	/* Turn D-cache off */
	phys_cache_func =
		(phys_cache_func_t)(unsigned long)virt_to_phys(cpu_cache_off);
	phys_cache_func();

	/* Push out any further dirty data, and ensure cache is empty */
	phys_cache_func =
		(phys_cache_func_t)(unsigned long)virt_to_phys(flush_cache_all);
	phys_cache_func();

	phys_reset = (phys_reset_t)(unsigned long)virt_to_phys(cpu_reset);
	phys_reset(addr);

	/* Should never get here. */
	BUG();
}

void hib_restart(unsigned long addr)
{
	u64 *stack = hib_stack + ARRAY_SIZE(hib_stack);
	/* Disable interrupts first */
	local_irq_disable();
	local_fiq_disable();

	/* Change to the new stack and continue with the reset. */
	call_with_stack(__hib_restart, (void *)addr, (void *)stack);

	/* Should never get here. */
	BUG();
}


/*
 * Snapshot kernel memory and reset the system.
 *
 * swsusp_save() is executed in the suspend finisher so that the CPU
 * context pointer and memory are part of the saved image, which is
 * required by the resume kernel image to restart execution from
 * swsusp_arch_suspend().
 *
 * soft_restart is not technically needed, but is used to get success
 * returned from cpu_suspend.
 *
 * When soft reboot completes, the hibernation snapshot is written out.
 */

static int notrace arch_save_image(unsigned long unused)
{
	int ret;

	ret = swsusp_save();
	if (ret == 0)
		hib_restart(virt_to_phys(cpu_resume));
	return ret;
}

/*
 * Save the current CPU state before suspend / poweroff.
 */
int notrace swsusp_arch_suspend(void)
{
	/* cpu_switch_mm(idmap_pg_dir, &init_mm); */
	return __cpu_suspend(0, arch_save_image);
}

/*
 * Restore page contents for physical pages that were in use during loading
 * hibernation image.  Switch to idmap_pgd so the physical page tables
 * are overwritten with the same contents.
 */
static void notrace arch_restore_image(unsigned long unused)
{
	struct pbe *pbe;

	cpu_switch_mm(idmap_pg_dir, &init_mm);
	flush_tlb_all();

	for (pbe = restore_pblist; pbe; pbe = pbe->next)
		copy_page(pbe->orig_address, pbe->address);

	hib_restart(virt_to_phys(cpu_resume));
}

/*
 * Resume from the hibernation image.
 * Due to the kernel heap / data restore, stack contents change underneath
 * and that would make function calls impossible; switch to a temporary
 * stack within the nosave region to avoid that problem.
 */
int swsusp_arch_resume(void)
{
	call_with_stack(arch_restore_image, 0,
		hib_stack + ARRAY_SIZE(hib_stack));
	return 0;
}
