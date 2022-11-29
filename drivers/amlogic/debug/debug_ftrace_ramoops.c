// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * drivers/amlogic/debug/debug_ftrace_ramoops.c
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#include <linux/kernel.h>
#include <linux/compiler.h>
#include <linux/irqflags.h>
#include <linux/percpu.h>
#include <linux/smp.h>
#include <linux/atomic.h>
#include <linux/types.h>
#include <linux/ftrace.h>
#include <linux/fs.h>
#include <linux/debugfs.h>
#include <linux/err.h>
#include <linux/amlogic/debug_ftrace_ramoops.h>
#include <../../../fs/pstore/internal.h>
#include <linux/trace_clock.h>
#include <linux/percpu.h>
#include <linux/moduleparam.h>
#include <linux/pstore_ram.h>
#include <linux/io.h>
#include <linux/printk.h>
#include <linux/uaccess.h>
#include <linux/rbtree.h>
#include <linux/of.h>
#include <linux/slab.h>

static DEFINE_PER_CPU(int, en);

#define IRQ_D	1
#define MAX_DETECT_REG 10

static unsigned int check_reg[MAX_DETECT_REG];
static unsigned int check_mask[MAX_DETECT_REG];
static unsigned int *virt_addr[MAX_DETECT_REG];
unsigned long old_val_reg[MAX_DETECT_REG];
struct rb_root reg_addr_root;

struct reg_addr {
	struct rb_node node;
	unsigned long phys_addr;
	unsigned long virt_addr;
	unsigned int size;
};

static int reg_check_panic;
core_param(reg_check_panic, reg_check_panic, int, 0644);

unsigned int dump_iomap;
core_param(dump_iomap, dump_iomap, uint, 0664);

unsigned int ramoops_ftrace_en;
EXPORT_SYMBOL(ramoops_ftrace_en);

int ramoops_io_en;
EXPORT_SYMBOL(ramoops_io_en);
core_param(ramoops_io_en, ramoops_io_en, int, 0664);

int ramoops_io_dump;
EXPORT_SYMBOL(ramoops_io_dump);
core_param(ramoops_io_dump, ramoops_io_dump, int, 0664);

int ramoops_io_skip;
EXPORT_SYMBOL(ramoops_io_skip);
core_param(ramoops_io_skip, ramoops_io_skip, int, 0664);

static int dump_phys_addr;
core_param(dump_phys_addr, dump_phys_addr, int, 0644);

const char *record_name[] = {
	"NULL",
	"IO-FUNC",
	"IO-R",
	"IO-W",
	"IO-R-E",
	"IO-W-E",
	"IO-TAG",
	"IO-SCHED-SWITCH",
	"IO-SMC-IN",
	"IO-SMC-OUT",
	"IO-SMC-NORET-IN",
	"IO-CLK-EN",
	"IO-CLK-DIS",
	"PD_POWER_ON",
	"PD_POWER_OFF",
};

void reg_check_init(void)
{
	int i;
	unsigned int *virt_tmp[MAX_DETECT_REG] = {NULL};

	memcpy(virt_tmp, virt_addr, sizeof(virt_addr));

	for (i = 0; i < MAX_DETECT_REG; i++)
		rcu_assign_pointer(virt_addr[i], NULL);

	synchronize_rcu();

	for (i = 0; i < MAX_DETECT_REG; i++) {
		if (virt_tmp[i])
			iounmap(virt_tmp[i]);
		else
			break;
	}

	for (i = 0; i < MAX_DETECT_REG; i++) {
		if (check_reg[i]) {
			virt_addr[i] = (unsigned int *)ioremap(check_reg[i], sizeof(unsigned long));
			if (!virt_addr[i]) {
				pr_err("Unable to map reg 0x%x\n", check_reg[i]);
				return;
			}
			pr_info("reg 0x%x has been mapped to 0x%px\n", check_reg[i], virt_addr[i]);
		} else {
			break;
		}
	}
}

void reg_check_func(void)
{
	unsigned int val;
	unsigned long tmp;
	unsigned int i = 0;
	unsigned int *tmp_addr;

	rcu_read_lock();
	while (i < MAX_DETECT_REG && virt_addr[i]) {
		tmp_addr = rcu_dereference(virt_addr[i]);
		if (old_val_reg[i] != -1) {
			val = *tmp_addr;
			if ((val & check_mask[i]) != (old_val_reg[i] & check_mask[i])) {
				tmp = old_val_reg[i];
				old_val_reg[i] = val;
				pr_err("phys_addr:0x%x new_val=0x%x old_val=0x%lx\n",
					check_reg[i], val, tmp);
				if (!reg_check_panic)
					dump_stack();
				else
					panic("reg_check_panic");
			}
		} else {
			old_val_reg[i] = *tmp_addr;
		}
		i++;
	}
	rcu_read_unlock();
}

static int check_reg_setup(const char *ptr, const struct kernel_param *kp)
{
	char *str_entry;
	char *str = (char *)ptr;
	unsigned int tmp;
	unsigned int i = 0, ret;

	do {
		str_entry = strsep(&str, ",");
		if (str_entry) {
			ret = kstrtou32(str_entry, 16, &tmp);
			if (ret)
				return -1;
			pr_info("check_reg: 0x%x\n", tmp);
			check_reg[i] = tmp;
			old_val_reg[i++] = -1;
		}
	} while (str_entry && i < MAX_DETECT_REG);

	reg_check_init();

	return 0;
}

static int check_reg_show(char *ptr, const struct kernel_param *kp)
{
	unsigned int i = 0;

	if (!check_reg[i])
		pr_info("No check reg\n");
	while (i < MAX_DETECT_REG && check_reg[i]) {
		pr_info("check_reg[%u]:0x%x\n", i, check_reg[i]);
		i++;
	}
	return 0;
}

static const struct kernel_param_ops check_reg_ops = {
	.set = check_reg_setup,
	.get = check_reg_show
};

core_param_cb(check_reg, &check_reg_ops, NULL, 0644);

static int check_mask_setup(const char *ptr, const struct kernel_param *kp)
{
	char *str_entry;
	char *str = (char *)ptr;
	unsigned int tmp;
	unsigned int i = 0, ret;

	do {
		str_entry = strsep(&str, ",");
		if (str_entry) {
			ret = kstrtou32(str_entry, 16, &tmp);
			if (ret)
				return -1;
			pr_info("check_mask: 0x%x\n", tmp);
			check_mask[i++] = tmp;
		}
	} while (str_entry && i < MAX_DETECT_REG);

	return 0;
}

static int check_mask_show(char *ptr, const struct kernel_param *kp)
{
	unsigned int i = 0;

	if (!check_mask[i])
		pr_info("No check mask\n");
	while (i < MAX_DETECT_REG && check_mask[i]) {
		pr_info("check_mask[%u]:0x%x\n", i, check_mask[i]);
		i++;
	}
	return 0;
}

static const struct kernel_param_ops check_mask_ops = {
	.set = check_mask_setup,
	.get = check_mask_show
};

core_param_cb(check_mask, &check_mask_ops, NULL, 0644);

static int reg_addr_insert(struct rb_root *root, struct reg_addr *new)
{
	struct rb_node **link = &root->rb_node, *parent = NULL;

	/* Figure out where to put new node */
	while (*link) {
		struct reg_addr *this = container_of(*link, struct reg_addr, node);
		int result = new->virt_addr - this->virt_addr;

		parent = *link;
		if (result < 0)
			link = &((*link)->rb_left);
		else if (result > 0)
			link = &((*link)->rb_right);
		else
			return 0;
	}

	/* Add new node and rebalance tree */
	rb_link_node(&new->node, parent, link);
	rb_insert_color(&new->node, root);

	return 1;
}

void save_iomap_info(unsigned long virt_addr, unsigned long phys_addr, unsigned int size)
{
	struct reg_addr *new;

	if (!dump_phys_addr)
		return;

	new = kmalloc(sizeof(*new), GFP_KERNEL);
	if (new) {
		new->virt_addr = virt_addr;
		new->phys_addr = phys_addr;
		new->size = size;
		reg_addr_insert(&reg_addr_root, new);
	}
}
EXPORT_SYMBOL(save_iomap_info);

void notrace pstore_io_rw_dump(struct pstore_ftrace_record *rec,
			       struct seq_file *s)
{
	unsigned long sec = 0, us = 0;
	unsigned long long time = rec->time;
	unsigned int cpu = pstore_ftrace_decode_cpu(rec);

	do_div(time, 1000);
	us = (unsigned long)do_div(time, 1000000);
	sec = (unsigned long)time;
	seq_printf(s, "[%04ld.%06ld@%d %d] <%5d-%6s> <%6s %s%08lx-%8lx>  <%ps <- %pS>\n",
		   sec, us, cpu, rec->in_irq, rec->pid, rec->comm,
		   record_name[rec->flag], rec->phys_addr ? "p:" : "", rec->val1,
		   (rec->flag == PSTORE_FLAG_IO_W || rec->flag == PSTORE_FLAG_IO_TAG) ?
		   rec->val2 : 0, (void *)rec->ip, (void *)rec->parent_ip);
}

void notrace pstore_sched_switch_dump(struct pstore_ftrace_record *rec,
			       struct seq_file *s)
{
	unsigned long sec = 0, us = 0;
	unsigned long long time = rec->time;
	unsigned int cpu = pstore_ftrace_decode_cpu(rec);

	do_div(time, 1000);
	us = (unsigned long)do_div(time, 1000000);
	sec = (unsigned long)time;
	seq_printf(s, "[%04ld.%06ld@%d %d] <%5d-%6s> <%6s %lu:%s>\n",
		   sec, us, cpu, rec->in_irq, rec->pid, rec->comm,
		   record_name[rec->flag], rec->val1, (char *)&rec->val2);
}

void notrace pstore_clk_pd_dump(struct pstore_ftrace_record *rec,
			struct seq_file *s)
{
	unsigned long sec = 0, us = 0;
	unsigned long long time = rec->time;
	unsigned int cpu = pstore_ftrace_decode_cpu(rec);

	do_div(time, 1000);
	us = (unsigned long)do_div(time, 1000000);
	sec = (unsigned long)time;
	seq_printf(s, "[%04ld.%06ld@%d %d] <%5d-%6s> <%6s %s>  <%ps <- %pS>\n",
		   sec, us, cpu, rec->in_irq, rec->pid, rec->comm, record_name[rec->flag],
		   rec->name, (void *)rec->ip, (void *)rec->parent_ip);
}

void notrace pstore_ftrace_dump(struct pstore_ftrace_record *rec,
				struct seq_file *s)
{
	switch (rec->flag & PSTORE_FLAG_MASK) {
	case PSTORE_FLAG_IO_R:
	case PSTORE_FLAG_IO_W:
	case PSTORE_FLAG_IO_W_END:
	case PSTORE_FLAG_IO_R_END:
	case PSTORE_FLAG_IO_TAG:
	case PSTORE_FLAG_IO_SMC_IN:
	case PSTORE_FLAG_IO_SMC_OUT:
	case PSTORE_FLAG_IO_SMC_NORET_IN:
		pstore_io_rw_dump(rec, s);
		break;
	case PSTORE_FLAG_IO_SCHED_SWITCH:
		pstore_sched_switch_dump(rec, s);
		break;
	case PSTORE_FLAG_CLK_ENABLE:
	case PSTORE_FLAG_CLK_DISABLE:
	case PSTORE_FLAG_PD_POWER_ON:
	case PSTORE_FLAG_PD_POWER_OFF:
		pstore_clk_pd_dump(rec, s);
		break;
	default:
		seq_printf(s, "Unknown Msg:%x\n", rec->flag);
	}
}

void notrace __pstore_sched_switch_dump(struct pstore_ftrace_record *rec)
{
	unsigned long sec = 0, us = 0;
	unsigned long long time = rec->time;
	unsigned int cpu = pstore_ftrace_decode_cpu(rec);

	do_div(time, 1000);
	us = (unsigned long)do_div(time, 1000000);
	sec = (unsigned long)time;
	pr_info("[%04ld.%06ld@%d %d] <%5d-%6s> <%6s %lu:%s>\n",
		   sec, us, cpu, rec->in_irq, rec->pid, rec->comm,
		   record_name[rec->flag], rec->val1, (char *)&rec->val2);
}

void notrace __pstore_clk_pd_dump(struct pstore_ftrace_record *rec)
{
	unsigned long sec = 0, us = 0;
	unsigned long long time = rec->time;
	unsigned int cpu = pstore_ftrace_decode_cpu(rec);

	do_div(time, 1000);
	us = (unsigned long)do_div(time, 1000000);
	sec = (unsigned long)time;
	pr_info("[%04ld.%06ld@%d %d] <%5d-%6s> <%6s %s>  <%ps <- %pS>\n",
		   sec, us, cpu, rec->in_irq, rec->pid, rec->comm, record_name[rec->flag],
		   rec->name, (void *)rec->ip, (void *)rec->parent_ip);
}

static unsigned long virt_convert_phys_addr(unsigned long virt_addr)
{
	struct rb_node *node;
	struct reg_addr *this, *tmp = NULL;
	unsigned long phys_addr;
	static unsigned long last_virt_addr, last_phys_addr;

	if ((virt_addr & PAGE_MASK) == (last_virt_addr & PAGE_MASK))
		return (last_phys_addr & PAGE_MASK) + (virt_addr & ~PAGE_MASK);

	for (node = rb_first(&reg_addr_root); node; node = rb_next(node)) {
		this = container_of(node, struct reg_addr, node);

		if (virt_addr >= this->virt_addr && virt_addr < this->virt_addr + this->size) {
			tmp = this;
			phys_addr = virt_addr - tmp->virt_addr + tmp->phys_addr;
			last_virt_addr = virt_addr;
			last_phys_addr = phys_addr;
			return phys_addr;
		}
	}
	/*No corresponding physical address found*/
	return -1;
}

void notrace pstore_io_save(unsigned long reg, unsigned long val,
			    unsigned long parent, unsigned int flag,
			    unsigned long *irq_flag)
{
	int cpu;
	unsigned long ret;
	struct pstore_ftrace_record rec = {};
	struct pstore_record record = {
		.type = PSTORE_TYPE_FTRACE,
		.buf = (char *)&rec,
		.size = sizeof(rec),
		.psi = psinfo,
	};

	if (!ramoops_ftrace_en || !ramoops_io_en)
		return;

	if ((flag == PSTORE_FLAG_IO_R || flag == PSTORE_FLAG_IO_W) && IRQ_D)
		local_irq_save(*irq_flag);

	if (flag == PSTORE_FLAG_IO_W_END)
		reg_check_func();

	switch (ramoops_io_skip) {
	case 1:
		rec.ip = CALLER_ADDR1;
		rec.parent_ip = CALLER_ADDR2;
		break;
	case 2:
		rec.ip = CALLER_ADDR2;
		rec.parent_ip = CALLER_ADDR3;
		break;
	case 3:
		rec.ip = CALLER_ADDR3;
		rec.parent_ip = CALLER_ADDR4;
		break;
	default:
		rec.ip = CALLER_ADDR0;
		rec.parent_ip = parent;
		break;
	}

	rec.flag = flag;
	rec.in_irq = !!in_irq();
	rec.phys_addr = 0;

	switch (rec.flag) {
	case PSTORE_FLAG_CLK_ENABLE:
	case PSTORE_FLAG_CLK_DISABLE:
	case PSTORE_FLAG_PD_POWER_ON:
	case PSTORE_FLAG_PD_POWER_OFF:
		strlcpy(rec.name, (char *)val, sizeof(rec.name));
		break;
	default:
		rec.val1 = reg;
		rec.val2 = val;
		break;
	}

	if (dump_phys_addr) {
		switch (rec.flag) {
		case PSTORE_FLAG_IO_R:
		case PSTORE_FLAG_IO_R_END:
		case PSTORE_FLAG_IO_W:
		case PSTORE_FLAG_IO_W_END:
			ret = virt_convert_phys_addr(reg);
			if (ret != -1) {
				rec.val1 = ret;
				rec.phys_addr = 1;
			}
			break;
		default:
			break;
		}
	}

	cpu = raw_smp_processor_id();
	if (unlikely(oops_in_progress) || unlikely(per_cpu(en, cpu))) {
		if ((flag == PSTORE_FLAG_IO_R || flag == PSTORE_FLAG_IO_W) && IRQ_D)
			local_irq_restore(*irq_flag);

		return;
	}
	per_cpu(en, cpu) = 1;
	pstore_ftrace_encode_cpu(&rec, cpu);
	strlcpy(rec.comm, current->comm, sizeof(rec.comm));
	rec.pid = current->pid;
	rec.time = trace_clock_local();

	psinfo->write(&record);
	per_cpu(en, cpu) = 0;

	if ((flag == PSTORE_FLAG_IO_R_END || flag == PSTORE_FLAG_IO_W_END) &&
	    IRQ_D)
		local_irq_restore(*irq_flag);
}
EXPORT_SYMBOL(pstore_io_save);

static void notrace __pstore_io_rw_dump(struct pstore_ftrace_record *rec)
{
	unsigned long sec = 0, us = 0;
	unsigned long long time = rec->time;
	unsigned int cpu = pstore_ftrace_decode_cpu(rec);

	do_div(time, 1000);
	us = (unsigned long)do_div(time, 1000000);
	sec = (unsigned long)time;
	pr_info("[%04ld.%06ld@%d %d] <%5d-%6s> <%6s %s%08lx-%8lx>  <%pS <- %pS>\n",
		sec, us, cpu, rec->in_irq, rec->pid, rec->comm,
		record_name[rec->flag], rec->phys_addr ? "p:" : "", rec->val1,
		(rec->flag == PSTORE_FLAG_IO_W || rec->flag == PSTORE_FLAG_IO_TAG) ?
		rec->val2 : 0, (void *)rec->ip, (void *)rec->parent_ip);
}

static void notrace __pstore_ftrace_dump_old(struct pstore_ftrace_record *rec)
{
	switch (rec->flag & PSTORE_FLAG_MASK) {
	case PSTORE_FLAG_FUNC:
		break;
	case PSTORE_FLAG_IO_R:
	case PSTORE_FLAG_IO_W:
	case PSTORE_FLAG_IO_W_END:
	case PSTORE_FLAG_IO_R_END:
	case PSTORE_FLAG_IO_TAG:
	case PSTORE_FLAG_IO_SMC_IN:
	case PSTORE_FLAG_IO_SMC_OUT:
	case PSTORE_FLAG_IO_SMC_NORET_IN:
		__pstore_io_rw_dump(rec);
		break;
	case PSTORE_FLAG_IO_SCHED_SWITCH:
		__pstore_sched_switch_dump(rec);
		break;
	case PSTORE_FLAG_CLK_ENABLE:
	case PSTORE_FLAG_CLK_DISABLE:
	case PSTORE_FLAG_PD_POWER_ON:
	case PSTORE_FLAG_PD_POWER_OFF:
		__pstore_clk_pd_dump(rec);
		break;
	default:
		pr_err("Unknown Msg:%x\n", rec->flag);
	}
}

static char reboot_mode[16];
static int __init reboot_mode_setup(char *s)
{
	if (s)
		snprintf(reboot_mode, sizeof(reboot_mode), "%s", s);

	return 0;
}
__setup("reboot_mode=", reboot_mode_setup);

bool is_shutdown_reboot(void)
{
	return !strcmp(reboot_mode, "shutdown_reboot");
}

bool is_cold_boot(void)
{
	return !strcmp(reboot_mode, "cold_boot");
}

static void dump_reg_compatible(void)
{
#ifdef CONFIG_ARM64
	u64 reg[20];
#else
	u32 reg[20];
#endif
	int reg_size, i;
	const char *string;
	struct device_node *node, *tmp_node;

	for_each_node_with_property(node, "reg") {
		tmp_node = node;

		while (tmp_node) {
			if (!of_property_read_string(tmp_node, "compatible", &string))
				break;
			tmp_node = tmp_node->parent;
		}

		pr_info("%s:\n", string);
#ifdef CONFIG_ARM64
		reg_size = of_property_read_variable_u64_array(node, "reg", reg, 0, 20);
		for (i = 0; i + 1 < reg_size; i += 2)
			pr_info("0x%llx_0x%llx\n", reg[i], reg[i + 1]);
#else
		reg_size = of_property_read_variable_u32_array(node, "reg", reg, 0, 20);
		for (i = 0; i + 1 < reg_size; i += 2)
			pr_info("0x%x_0x%x\n", reg[i], reg[i + 1]);
#endif
	}
}

void notrace pstore_ftrace_dump_old(struct persistent_ram_zone *prz)
{
	struct pstore_ftrace_record *rec;
	void *rec_end;

	rec = (struct pstore_ftrace_record *)prz->old_log;
	rec_end = (void *)rec + prz->old_log_size;

	pr_info("ramoops_io_dump=%d, buffer=%px ftrace_old_log=%px, size=%u, reboot_mode=%s\n",
		ramoops_io_dump, prz->buffer,
		rec, (unsigned int)prz->old_log_size, reboot_mode);

	if (!strcmp(reboot_mode, "cold_boot"))
		return;

	if (!strcmp(reboot_mode, "normal"))
		return;

	if (!strcmp(reboot_mode, "shutdown_reboot"))
		return;

	if (!ramoops_io_dump)
		return;

	if (!persistent_ram_old_size(prz))
		return;

	if (dump_phys_addr)
		dump_reg_compatible();

	rec = (void *)rec + prz->old_log_size % sizeof(*rec);

	while ((void *)rec < rec_end) {
		__pstore_ftrace_dump_old(rec);
		rec++;
	}
}

void __arm_smccc_smc_glue(unsigned long a0, unsigned long a1,
			unsigned long a2, unsigned long a3, unsigned long a4,
			unsigned long a5, unsigned long a6, unsigned long a7,
			struct arm_smccc_res *res, struct arm_smccc_quirk *quirk)
{
	smc_trace_start(a0, a1, is_noret_smcid(a0));
	__arm_smccc_smc(a0, a1, a2, a3, a4, a5, a6, a7, res, quirk);
	smc_trace_stop(a0, a1);
}
EXPORT_SYMBOL(__arm_smccc_smc_glue);
