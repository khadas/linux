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

static DEFINE_PER_CPU(int, en);

#define IRQ_D	1

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

const char *record_name[] = {
	"NULL",
	"FUNC",
	"IO-R",
	"IO-W",
	"IO-R-E",
	"IO-W-E",
	"IO-TAG",
};

void notrace pstore_io_rw_dump(struct pstore_ftrace_record *rec,
			       struct seq_file *s)
{
	unsigned long sec = 0, us = 0;
	unsigned long long time = rec->time;
	unsigned int cpu = pstore_ftrace_decode_cpu(rec);

	do_div(time, 1000);
	us = (unsigned long)do_div(time, 1000000);
	sec = (unsigned long)time;
	seq_printf(s, "[%04ld.%06ld@%d %d] <%5d-%6s> <%6s %08lx-%8lx>  <%ps <- %pS>\n",
		   sec, us, cpu, rec->in_irq, rec->pid, rec->comm,
		   record_name[rec->flag],
		   rec->val1, (rec->flag == PSTORE_FLAG_IO_W) ? rec->val2 : 0,
		   (void *)rec->ip, (void *)rec->parent_ip);
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
		pstore_io_rw_dump(rec, s);
		break;
	default:
		seq_printf(s, "Unknown Msg:%x\n", rec->flag);
	}
}

void notrace pstore_io_save(unsigned long reg, unsigned long val,
			    unsigned long parent, unsigned int flag,
			    unsigned long *irq_flag)
{
	int cpu;
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
	rec.val1 = reg;
	rec.val2 = val;

	cpu = raw_smp_processor_id();

	if (unlikely(oops_in_progress) || unlikely(per_cpu(en, cpu)))
		return;
	per_cpu(en, cpu) = 1;
	pstore_ftrace_encode_cpu(&rec, cpu);
	strlcpy(rec.comm, current->comm, sizeof(rec.comm) - 1);
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
	pr_info("[%04ld.%06ld@%d %d] <%5d-%6s> <%6s %08lx-%8lx>  <%pS <- %pS>\n",
		sec, us, cpu, rec->in_irq, rec->pid, rec->comm,
		record_name[rec->flag], rec->val1,
		(rec->flag == PSTORE_FLAG_IO_W) ? rec->val2 : 0,
		(void *)rec->ip, (void *)rec->parent_ip);
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
		__pstore_io_rw_dump(rec);
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

	if (!ramoops_io_dump)
		return;

	if (!persistent_ram_old_size(prz))
		return;

	rec = (void *)rec + prz->old_log_size % sizeof(*rec);

	while ((void *)rec < rec_end) {
		__pstore_ftrace_dump_old(rec);
		rec++;
	}
}
