// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
 */

#include <linux/time64.h>
#include <linux/ktime.h>
#include <linux/timekeeping.h>
#include <linux/workqueue.h>

#include "debug.h"

#ifdef DEBUG_IRQ

#define DUMP_SIZE 128
#define DEBUG_ITEMS 3

struct ddr_debug {
	struct work_struct work;
	unsigned int *ddr_table;
	unsigned int *dump_buffer;
	unsigned int ddr_counter;
	unsigned int ddr_id;
	unsigned int buffer_size;
	bool ddr_type;
};

struct ddr_debug ddr[2][DDRMAX];
static bool ddr_debug_enable;

static inline unsigned int get_tick_count(void)
{
	struct timespec ts;

	ktime_get_ts(&ts);
	return (int)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

static void dump_data(struct work_struct *p_work)
{
	struct ddr_debug *pddr;
	int i, diff, average, valid_space = 0, hwptr, swptr, buffer_size;

	pddr = container_of(p_work, struct ddr_debug, work);
	if (!pddr)
		return;
	buffer_size = pddr->buffer_size;

	for (i = 0; i < (DUMP_SIZE - 1); i++) {
		diff = *(pddr->dump_buffer + (i + 1) * DEBUG_ITEMS) -
					*(pddr->dump_buffer + i * DEBUG_ITEMS);
		hwptr = *(pddr->dump_buffer + i * DEBUG_ITEMS + 1);
		swptr = *(pddr->dump_buffer + i * DEBUG_ITEMS + 2);

		if (pddr->ddr_type == FRDDR)
			valid_space = (swptr - hwptr + buffer_size) % buffer_size;
		else
			valid_space = (hwptr - swptr + buffer_size) % buffer_size;

		pr_info("%s - %d: [%d] %d, diff %d, hwptr %d, swptr %d, valid %d\n",
			pddr->ddr_type ? "FRDDR" : "TODDR",
			pddr->ddr_id, i, *(pddr->dump_buffer + i * DEBUG_ITEMS),
			diff, hwptr, swptr, valid_space);
	}
	average = *(pddr->dump_buffer + (DUMP_SIZE - 1) * DEBUG_ITEMS) - *pddr->dump_buffer;
	pr_info("%s - %d: total %d, time average %d\n",
			pddr->ddr_type ? "FRDDR" : "TODDR",
			pddr->ddr_id, average, (average / (DUMP_SIZE - 1)));
}

void get_time_stamp(bool ddr_type, unsigned int ddr_id, unsigned int hwptr,
		unsigned int swptr, unsigned int buffer_size)
{
	struct ddr_debug *pddr = &ddr[ddr_type][ddr_id];
	unsigned int *ptable;

	if (ddr_debug_enable) {
		/* if enable debugging, malloc debug buffer*/
		if (!pddr->ddr_table) {
			pddr->ddr_table = kmalloc_array(DUMP_SIZE * DEBUG_ITEMS,
								sizeof(int), GFP_KERNEL);
			pddr->dump_buffer = kmalloc_array(DUMP_SIZE * DEBUG_ITEMS,
								sizeof(int), GFP_KERNEL);
			pddr->ddr_counter = 0;
			pddr->ddr_type = ddr_type;
			pddr->ddr_id = ddr_id;
			if (!pddr->ddr_table || !pddr->dump_buffer)
				return;
			INIT_WORK(&pddr->work, dump_data);
			pr_info("start debug ddr type [%d], id [%d]\n", ddr_type, ddr_id);
			pddr->buffer_size = buffer_size;
		}

		ptable = pddr->ddr_table + (pddr->ddr_counter * DEBUG_ITEMS);
		*ptable++ = get_tick_count(); /* system time */
		*ptable++ = hwptr; /* hw pointer */
		*ptable = swptr;   /* app pointer */
		pddr->ddr_counter++;
		if (pddr->ddr_counter == DUMP_SIZE) {
			pddr->ddr_counter = 0;
			memcpy(pddr->dump_buffer, pddr->ddr_table,
					(DUMP_SIZE * sizeof(int) * DEBUG_ITEMS));
			schedule_work(&pddr->work);
		}
	} else if (pddr->ddr_table) {
		/* if disable debugging, free debug buffer*/
		cancel_work_sync(&pddr->work);
		kfree(pddr->ddr_table);
		pddr->ddr_table = NULL;
		kfree(pddr->dump_buffer);
		pddr->dump_buffer = NULL;
		pddr->ddr_counter = 0;
		pddr->ddr_type = 0;
		pddr->ddr_id = 0;
		pr_info("stop debug ddr type [%d], id [%d]\n", ddr_type, ddr_id);
	}
}

void set_ddr_debug(bool debug_enable)
{
	pr_info("debug ddr %s\n", debug_enable ? "enable" : "disable");
	ddr_debug_enable = debug_enable;
}

bool get_ddr_debug(void)
{
	return ddr_debug_enable;
}

#endif

