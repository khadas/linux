// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#define pr_fmt(fmt) "vshrinker: " fmt

#include <linux/stddef.h>
#include <linux/mm.h>
#include <linux/highmem.h>
#include <linux/swap.h>
#include <linux/interrupt.h>
#include <linux/pagemap.h>
#include <linux/jiffies.h>
#include <linux/memblock.h>
#include <linux/compiler.h>
#include <linux/kernel.h>
#include <linux/kasan.h>
#include <linux/module.h>
#include <linux/vmalloc.h>
#include <linux/suspend.h>
#include <linux/pagevec.h>
#include <linux/blkdev.h>
#include <linux/slab.h>
#include <linux/shrinker.h>
#include <linux/delay.h>
#include <linux/vmstat.h>
#include <linux/crypto.h>
#include <linux/mmzone.h>
#include <linux/zsmalloc.h>
#include <linux/amlogic/vmalloc_shrinker.h>
#ifdef CONFIG_AMLOGIC_PAGE_TRACE
#include <linux/amlogic/page_trace.h>
#endif
#include <linux/proc_fs.h>
#include <asm/system_misc.h>

#define PTE_LOCK_BIT		(63)
#define MAX_CACHED_PAGES	(32)
#define CACHED_PAGE_ORDER	(5)
#define RESCAN			1800

#define D(format, args...)					\
	{ if (vdebug > 1)					\
		pr_info("%s " format, __func__, ##args);	\
	}

#define I(format, args...)					\
	{ if (vdebug)						\
		pr_info("%s " format, __func__, ##args);	\
	}

#define E(format, args...)	pr_err("%s " format, __func__, ##args)

static int vdebug;
struct kpage_map {
	unsigned long addr;
	unsigned long handle;
	unsigned int  size;
#ifdef CONFIG_AMLOGIC_PAGE_TRACE
	struct page_trace trace;
#endif
	pte_t old_pte;
	union {
		struct rb_node node;
		struct list_head list;
	};
};

struct vmalloc_shinker {
	struct zs_pool *mem_pool;
	void   *buffer;
	struct crypto_comp *tfm;
	struct proc_dir_entry *d_vmalloc_shrinker;
	struct rb_root root;
	unsigned long last_vaddr;
	unsigned long scanned_pages;
	pte_t cur_scan_pte;
	struct list_head page_list;
	struct list_head km_free_list;
	struct delayed_work page_work;
	atomic64_t cur_scan_addr;
	spinlock_t tree_lock;	/* protect for tree */
	spinlock_t tfm_lock;	/* protect for zstrem */
	spinlock_t page_lock;	/* protect for page_list */
	spinlock_t free_lock;	/* protect for free_list */
	atomic_t scan_with_fault;
	atomic_t reclaimed_pages;
	atomic_t zram_used_size;
	atomic_t restored_cnt;
	int cached_pages;
	int enable;
};

struct vmalloc_shinker *vs;

static pte_t *vmalloc_page_pte(unsigned long addr, struct page **page)
{
	pgd_t *pgd = pgd_offset_k(addr);
	p4d_t *p4d;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *ptep, pte;

	if (pgd_none(*pgd))
		return NULL;
	p4d = p4d_offset(pgd, addr);
	if (p4d_none(*p4d))
		return NULL;
	pud = pud_offset(p4d, addr);

	/*
	 * Don't dereference bad PUD or PMD (below) entries. This will also
	 * identify huge mappings, which we may encounter on architectures
	 * that define CONFIG_HAVE_ARCH_HUGE_VMAP=y. Such regions will be
	 * identified as vmalloc addresses by is_vmalloc_addr(), but are
	 * not [unambiguously] associated with a struct page, so there is
	 * no correct value to return for them.
	 */
	if (pud_none(*pud) || pud_bad(*pud))
		return NULL;
	pmd = pmd_offset(pud, addr);
	if (pmd_none(*pmd) || pmd_bad(*pmd))
		return NULL;

	ptep = pte_offset_map(pmd, addr);
	pte = *ptep;
	if (pte_present(pte) && page)
		*page = pte_page(pte);
	return ptep;
}

struct kpage_map *find_km(unsigned long addr)
{
	struct rb_node *rb;
	struct kpage_map *km;

	rb = vs->root.rb_node;
	while (rb) {
		km = rb_entry(rb, struct kpage_map, node);
		if (km->addr == addr) {
			/* mark we are handling this addr*/
			return km;
		}
		if (addr < km->addr)
			rb = km->node.rb_left;
		else
			rb = km->node.rb_right;
	}
	return NULL;
}

static void scan_pte_op(unsigned long addr, unsigned long pfn,
			 pte_t *ptep, pte_t pte, int set)
{
	u64 tmp = addr;

	if (set == 0) {
		atomic64_set(&vs->cur_scan_addr, tmp);
		vs->cur_scan_pte = pte; /* back up */
		set_pte(ptep, __pte(0));
		flush_tlb_kernel_range(addr, addr + PAGE_SIZE);
	} else {
		set_pte(ptep, pte);
		flush_tlb_kernel_range(addr, addr + PAGE_SIZE);
		atomic64_set(&vs->cur_scan_addr, 0ULL);
		vs->cur_scan_pte = __pte(0);
	}
}

int handle_vmalloc_fault(struct pt_regs *regs, unsigned long faddr)
{
	struct kpage_map *km;
	unsigned long flags, addr, iflag, scan_addr;
	unsigned int dst_len = PAGE_SIZE;
	struct page *page;
	void *src, *dst;
	pte_t *ptep, pte;
	pgprot_t raw_prot;
	int ret;

	addr = faddr & PAGE_MASK;
	if (!is_vmalloc_or_module_addr((void *)addr))
		return -EINVAL;

	local_irq_save(iflag);
	D("handle:%lx, cached:%d\n", addr, vs->cached_pages);
	ptep = vmalloc_page_pte(addr, NULL);
	if (pte_present(*ptep)) {
		local_irq_restore(iflag);
		D("pte already exist:%lx:%lx\n", addr,
			(unsigned long)pte_val(*ptep));
		return 0;
	}

	/* check if this page is under scan */
	scan_addr = atomic64_read(&vs->cur_scan_addr);
	if ((scan_addr & ~0x01UL) == addr) {
		if (!(scan_addr & 0x01)) {	/* scan not finished */
			atomic_set(&vs->scan_with_fault, 1);
			set_pte(ptep, vs->cur_scan_pte);
			flush_tlb_kernel_range(addr, addr + PAGE_SIZE);
			local_irq_restore(iflag);
			I("restore addr %lx while scan\n", addr);
			return 0;
		}
	}

	bit_spin_lock(PTE_LOCK_BIT, (unsigned long *)ptep);
	/* find kpage mapping */
	spin_lock_irqsave(&vs->tree_lock, flags);
	km = find_km(addr);
	if (km)
		rb_erase(&km->node, &vs->root);
	spin_unlock_irqrestore(&vs->tree_lock, flags);
	if (!km) {
		bit_spin_unlock(PTE_LOCK_BIT, (unsigned long *)ptep);
		local_irq_restore(iflag);
		if (pte_present(*ptep)) {
			D("pte1 already exist:%lx:%lx\n",
				addr, (unsigned long)pte_val(*ptep));
		} else {
			E("can't find zs mem for addr:%lx %lx\n", addr, faddr);
		}
		return 0;	/* will refault again? */
	}
	/* alloc page and decompress ? */
	spin_lock_irqsave(&vs->page_lock, flags);
	if (vs->cached_pages > 0) {
		page = list_first_entry(&vs->page_list, struct page, lru);
		list_del(&page->lru);
		vs->cached_pages--;
		spin_unlock_irqrestore(&vs->page_lock, flags);
	} else {
		spin_unlock_irqrestore(&vs->page_lock, flags);
		page = alloc_page(GFP_ATOMIC | __GFP_HIGHMEM);
		if (!page) {
			E("can't get page, free:%ld, cma:%ld\n",
				global_zone_page_state(NR_FREE_PAGES),
				global_zone_page_state(NR_FREE_CMA_PAGES));
			page = alloc_page(GFP_ATOMIC | __GFP_HIGHMEM |
					  __GFP_NO_CMA);
			WARN_ON(!page);
		}
	}

	page->private = jiffies;
	set_bit(PG_owner_priv_1, &page->flags); /* do not fault it again ? */
	src = zs_map_object(vs->mem_pool, km->handle,
			    ZS_MM_RO | ZS_MM_SHRINKER);
	dst = kmap_atomic(page);
	spin_lock_irqsave(&vs->tfm_lock, flags);
	ret = crypto_comp_decompress(vs->tfm, src, km->size, dst, &dst_len);
	spin_unlock_irqrestore(&vs->tfm_lock, flags);
	kunmap_atomic(dst);
	zs_unmap_object(vs->mem_pool, km->handle);
	WARN_ON(ret);

	raw_prot = __pgprot(km->old_pte.pte & ~PTE_ADDR_MASK);
	pte  = mk_pte(page, raw_prot);
	set_pte(ptep, pte);
	flush_tlb_kernel_range(addr, addr + PAGE_SIZE);
	bit_spin_unlock(PTE_LOCK_BIT, (unsigned long *)ptep);
	pte_unmap(ptep);
	local_irq_restore(iflag);
	/* at this time, pte is set and can notify other cpu now */
#ifdef CONFIG_AMLOGIC_PAGE_TRACE
	set_page_trace(page, 0, GFP_KERNEL, (void *)unpack_ip(&km->trace));
#endif

	atomic_dec(&vs->reclaimed_pages);
	atomic_sub(km->size, &vs->zram_used_size);
	atomic_inc(&vs->restored_cnt);
	spin_lock_irqsave(&vs->free_lock, flags);
	INIT_LIST_HEAD(&km->list);
	list_add_tail(&km->list, &vs->km_free_list);	/* free later */
	spin_unlock_irqrestore(&vs->free_lock, flags);

	D("restore page:%5lx for addr:%lx, ret:%d, dst_len:%d\n",
		page_to_pfn(page), addr, ret, dst_len);

	return 0;
}

static void insert_kpage(struct kpage_map *km, unsigned long addr)
{
	struct rb_node **link, *parent = NULL;
	struct kpage_map *tmp;

	link  = &vs->root.rb_node;

	while (*link) {
		parent = *link;
		tmp = rb_entry(parent, struct kpage_map, node);
		if (addr < tmp->addr)
			link = &tmp->node.rb_left;
		else if (addr > tmp->addr)
			link = &tmp->node.rb_right;
		else
			WARN_ON(1);		/* meet again ? */
	}
	rb_link_node(&km->node, parent, link);
	rb_insert_color(&km->node, &vs->root);
}

static int vmalloc_shrinker_scan(void *data)
{
	unsigned long addr = 0, handle = 0, flags, pfn;
	struct page *page;
	struct kpage_map *km;
	struct vmap_area *va;
	pte_t *ptep, old_pte;
	void *dst, *src;
	int i, total_pages, comp_len, ret, scanned = 0;
	struct module *module = NULL;

	while (1) {
		msleep(1000);
		if (kthread_should_stop())
			break;
		if (!vs->enable)
			continue;

		va = get_vm(vs->last_vaddr);
		if (!va || (va && !va->vm)) {
			I("can't find va for %lx or null vm, va:%px\n",
				vs->last_vaddr, va);
			vs->last_vaddr = MODULES_VADDR;
			continue;
		}

		/* ignore can't scan area */
		if (!(va->vm->flags & VM_SCAN)) {
			vs->last_vaddr = va->va_end;
			if (va->va_start >= VMALLOC_END)
				vs->last_vaddr = MODULES_VADDR;
			continue;
		}

		if (!is_vmalloc_or_module_addr((void *)va->va_start)) {
			/* reset to start of module*/
			I("reset for %lx\n", vs->last_vaddr);
			if (va->va_start >= VMALLOC_END)
				vs->last_vaddr = MODULES_VADDR;
			continue;
		}

		if (vs->last_vaddr < va->va_end &&
		    vs->last_vaddr > va->va_start)
			addr = vs->last_vaddr;
		else
			addr = va->va_start;

		module      = __module_address(addr);
		scanned     = 0;
		total_pages = (va->va_end - addr) / PAGE_SIZE;
		D("scan addr [%lx - %lx], vf:%lx, module:%s, caller:%ps\n",
			addr, va->va_end, va->vm->flags,
			module ? module->name : "NULL", va->vm->caller);
		for (i = 0; i < total_pages; i++) {
			page = NULL;
			ptep = vmalloc_page_pte(addr, &page);
			if (!page)
				goto next;

			pfn = page_to_pfn(page);
			if (!pfn_valid(pfn))
				goto next;

			/* ignore page which hard to compress */
			if (test_bit(PG_owner_priv_1, &page->flags)) {
				if (!page->private ||
				    (page->private &&
				     jiffies - page->private < HZ * RESCAN))
					goto next;
			}

			/*
			 * set flag and clear pte first to avoid write
			 * during compress, if fault during process, then
			 * ignore this case
			 */
			old_pte = *ptep;
			atomic_set(&vs->scan_with_fault, 0);
			scan_pte_op(addr, pfn, ptep, old_pte, 0);
			D("addr:%lx, page:%lx, fun:%ps\n", addr, pfn,
				(void *)get_page_trace(page));
			comp_len = PAGE_SIZE * 2;
			spin_lock_irqsave(&vs->tfm_lock, flags);
			src = kmap_atomic(page);
			ret = crypto_comp_compress(vs->tfm, src, PAGE_SIZE,
						   vs->buffer, &comp_len);
			kunmap_atomic(src);
			spin_unlock_irqrestore(&vs->tfm_lock, flags);

			if (unlikely(ret)) {
				scan_pte_op(addr, pfn, ptep, old_pte, 1);
				E("compress vaddr:%lx failed, ret:%d",
					addr, ret);
				goto next;
			}
			if (comp_len > (PAGE_SIZE * 7 / 8)) {
				/* mark this page is hard to compress */
				scan_pte_op(addr, pfn, ptep, old_pte, 1);
				set_bit(PG_owner_priv_1, &page->flags);
				goto next;
			}

			km = kzalloc(sizeof(*km), GFP_KERNEL | __GFP_NOWARN);
			if (!km) {
				scan_pte_op(addr, pfn, ptep, old_pte, 1);
				continue;
			}

			handle = zs_malloc(vs->mem_pool, comp_len,
					   __GFP_KSWAPD_RECLAIM |
					   __GFP_NOWARN | __GFP_HIGHMEM |
					   __GFP_MOVABLE);
			if (!handle) {
				scan_pte_op(addr, pfn, ptep, old_pte, 1);
				kfree(km);
				continue;
			}

			km->addr    = addr;
			km->handle  = handle;
			km->size    = comp_len;
			km->old_pte = old_pte;
		#ifdef CONFIG_AMLOGIC_PAGE_TRACE
			km->trace   = *(find_page_base(page));
		#endif

			dst = zs_map_object(vs->mem_pool, handle, ZS_MM_WO);
			memcpy(dst, vs->buffer, comp_len);
			zs_unmap_object(vs->mem_pool, handle);
			spin_lock_irqsave(&vs->tree_lock, flags);
			insert_kpage(km, addr);
			atomic64_inc(&vs->cur_scan_addr);
			spin_unlock_irqrestore(&vs->tree_lock, flags);

			if (atomic_read(&vs->scan_with_fault) & 0x01) {
				/* fault when scan */
				I("fault with scan %lx\n", addr);
				release_vshrinker_page(addr, PAGE_SIZE);
				goto next;
			} else {
				__free_pages(page, 0);
			}
			scanned++;
			atomic_inc(&vs->reclaimed_pages);
			atomic_add(comp_len, &vs->zram_used_size);

			D("reclaim vmalloc %lx page:%5lx, comp size:%4d, scanned:%2d\n",
				addr, pfn, comp_len, scanned);
next:
			addr += PAGE_SIZE;
			pte_unmap(ptep);
			if (scanned >= SWAP_CLUSTER_MAX)
				break;
		}
		if (scanned)
			vs->last_vaddr = addr;
		else /* no suitable page for compress */
			vs->last_vaddr = va->va_end;
		vs->scanned_pages += scanned;
		D("scan done, reclaimed pages:%5d, zspage:%5ld, bytes:%d, restore:%d, scanned:%ld, cached:%2d\n",
			atomic_read(&vs->reclaimed_pages),
			zs_get_total_pages(vs->mem_pool),
			atomic_read(&vs->zram_used_size),
			atomic_read(&vs->restored_cnt),
			vs->scanned_pages, vs->cached_pages);
	}
	return 0;
}

void release_vshrinker_page(unsigned long addr, unsigned long size)
{
	struct kpage_map *km;
	unsigned long flags;
	unsigned long end;

	if (!vs)
		return;

	end = PAGE_ALIGN(addr + size);
	while (addr < end) {
		/* find kpage mapping */
		spin_lock_irqsave(&vs->tree_lock, flags);
		km = find_km(addr);
		if (km)
			rb_erase(&km->node, &vs->root);
		spin_unlock_irqrestore(&vs->tree_lock, flags);
		if (!km) {
			addr += PAGE_SIZE;
			continue;
		}

		zs_free(vs->mem_pool, km->handle);
		atomic_dec(&vs->reclaimed_pages);
		atomic_sub(km->size, &vs->zram_used_size);
		atomic_inc(&vs->restored_cnt);
		kfree(km);

		I("free zram for addr:%lx\n", addr);
		addr += PAGE_SIZE;
	}
}

static void page_works(struct work_struct *work)
{
	int pages, i;
	unsigned long flags;
	struct page *page;
	struct list_head head;
	struct kpage_map *km, *next;

	spin_lock_irqsave(&vs->free_lock, flags);
	if (!list_empty(&vs->km_free_list)) {
		list_for_each_entry_safe(km, next, &vs->km_free_list, list) {
			list_del(&km->list);
			zs_free(vs->mem_pool, km->handle);
			kfree(km);
		}
	}
	spin_unlock_irqrestore(&vs->free_lock, flags);

	spin_lock_irqsave(&vs->page_lock, flags);
	pages = vs->cached_pages;
	spin_unlock_irqrestore(&vs->page_lock, flags);

	if (pages < MAX_CACHED_PAGES) {
		INIT_LIST_HEAD(&head);
		i = 0;
		while (i < MAX_CACHED_PAGES - pages) {
			page = alloc_page(GFP_KERNEL | __GFP_HIGHMEM);
			if (page) {
				list_add(&page->lru, &head);
				i++;
			}
		}
		spin_lock_irqsave(&vs->page_lock, flags);
		vs->cached_pages += i;
		list_splice(&head, &vs->page_list);
		spin_unlock_irqrestore(&vs->page_lock, flags);
	}

	schedule_delayed_work(&vs->page_work, msecs_to_jiffies(200));
}

int __init start_vshrinker(void)
{
	kthread_run(vmalloc_shrinker_scan, vs, "vshrinker");
	return 0;
}
late_initcall(start_vshrinker);

static int vmalloc_shrinker_show(struct seq_file *m, void *arg)
{
	unsigned long bytes;

	bytes = zs_get_total_pages(vs->mem_pool) * PAGE_SIZE;
	seq_printf(m, "reclaimed pages:%9d\n",
		   atomic_read(&vs->reclaimed_pages));
	seq_printf(m, "zram used pages:%9ld\n",
		   zs_get_total_pages(vs->mem_pool));
	seq_printf(m, "zram used bytes:%9d\n",
		   atomic_read(&vs->zram_used_size));
	seq_printf(m, "zram usage ratio:%8ld%%\n",
		   atomic_read(&vs->zram_used_size) * 100UL / bytes);
	seq_printf(m, "fault count:    %9d\n", atomic_read(&vs->restored_cnt));
	seq_printf(m, "total scanned:  %9ld\n", vs->scanned_pages);
	return 0;
}

static int vmalloc_shrinker_open(struct inode *inode, struct file *file)
{
	return single_open(file, vmalloc_shrinker_show, NULL);
}

static ssize_t vmalloc_shrinker_write(struct file *file,
				     const char __user *buffer,
				     size_t count, loff_t *ppos)
{
	char *buf;
	unsigned long arg = 0;

	buf = kmalloc(count, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	if (copy_from_user(buf, buffer, count)) {
		kfree(buf);
		return -EINVAL;
	}

	if (!strncmp(buf, "enable=", 7)) {	/* option for 'merge=' */
		if (sscanf(buf, "enable=%ld", &arg) < 0) {
			kfree(buf);
			return -EINVAL;
		}
		vs->enable = arg ? 1 : 0;
		I("vmalloc shinker:%d\n", vs->enable);
	}

	if (!strncmp(buf, "debug=", 6)) {	/* option for 'merge=' */
		if (sscanf(buf, "debug=%ld", &arg) < 0) {
			kfree(buf);
			return -EINVAL;
		}
		vdebug = arg;
		I("vmalloc shinker:%d\n", vdebug);
	}

	kfree(buf);

	return count;
}

static const struct file_operations vmalloc_shrinker_file_ops = {
	.open		= vmalloc_shrinker_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.write		= vmalloc_shrinker_write,
	.release	= single_release,
};

static int __init vmalloc_shrinker_init(void)
{
	struct page *page;
	struct vmalloc_shinker *tmp_vs = NULL;
	int i;

	page = alloc_pages(GFP_KERNEL | __GFP_HIGHMEM, CACHED_PAGE_ORDER);
	if (!page)
		return -ENOMEM;

	tmp_vs = kzalloc(sizeof(*tmp_vs), GFP_KERNEL);
	if (!tmp_vs)
		return -ENOMEM;

	spin_lock_init(&tmp_vs->tree_lock);
	spin_lock_init(&tmp_vs->tfm_lock);
	spin_lock_init(&tmp_vs->page_lock);
	spin_lock_init(&tmp_vs->free_lock);
	INIT_LIST_HEAD(&tmp_vs->page_list);
	INIT_LIST_HEAD(&tmp_vs->km_free_list);

	tmp_vs->last_vaddr    = MODULES_VADDR;
	tmp_vs->enable        = 1;
	tmp_vs->buffer        = kmalloc(PAGE_SIZE * 2, GFP_KERNEL);
	tmp_vs->tfm           = crypto_alloc_comp("lz4", 0, 0);
	if (!tmp_vs->buffer || !tmp_vs->tfm)
		goto exit;

	tmp_vs->mem_pool = zs_create_pool("vmalloc_shrinker");
	if (!tmp_vs->mem_pool)
		goto exit;

	tmp_vs->d_vmalloc_shrinker = proc_create("vmalloc_shrinker", 0644,
					    NULL, &vmalloc_shrinker_file_ops);
	if (IS_ERR_OR_NULL(tmp_vs->d_vmalloc_shrinker)) {
		E("create vmalloc shrinker sysfs failed\n");
		return -1;
	}
	for (i = 0; i < MAX_CACHED_PAGES; i++) {
		list_add(&page->lru, &tmp_vs->page_list);
		page++;
	}
	INIT_DELAYED_WORK(&tmp_vs->page_work, page_works);
	tmp_vs->cached_pages = MAX_CACHED_PAGES;
	schedule_delayed_work(&tmp_vs->page_work, msecs_to_jiffies(200));

	I("create shrinker\n");
	vs = tmp_vs;
	return 0;

exit:
	if (tmp_vs->mem_pool)
		zs_destroy_pool(tmp_vs->mem_pool);
	if (tmp_vs->tfm)
		crypto_free_comp(tmp_vs->tfm);
	kfree(tmp_vs->buffer);
	kfree(tmp_vs);
	tmp_vs = NULL;
	__free_pages(page, CACHED_PAGE_ORDER);
	return -EINVAL;
}

static void __exit vmalloc_shrinker_exit(void)
{
	if (vs) {
		zs_destroy_pool(vs->mem_pool);
		crypto_free_comp(vs->tfm);
		cancel_delayed_work_sync(&vs->page_work);
		proc_remove(vs->d_vmalloc_shrinker);
		kfree(vs->buffer);
		kfree(vs);
		vs = NULL;
	}
}

module_init(vmalloc_shrinker_init);
module_exit(vmalloc_shrinker_exit);
