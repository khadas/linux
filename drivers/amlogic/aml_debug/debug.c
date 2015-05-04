/*
 * drivers/amlogic/aml_debug/debug.c
 *
 * Copyright (C) 2015 Amlogic, Inc. All rights reserved.
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



/* Standard Linux headers */
#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/wakelock.h>

/* Amlogic headers */
#include <linux/amlogic/iomap.h>
#include <linux/amlogic/clk.h>
#define CLASS_NAME        "amlogic"
/* path name:		"/sys/class/CLASS_NAME/" */
enum{
	WORK_MODE_READ,
	WORK_MODE_WRITE,
	WORK_MODE_CLKMEASURE,
	WORK_MODE_DUMP,
	WORK_MODE_THREAD,
	WORK_MODE_STACK,
	WORK_MODE_GPIOTEST,
};
static const char *usage_str = {"Usage:\n"
"    echo [read | write <data>] [c | v | a  | d ] addr > debug ; Access CBUS/VCBUS/AOBUS/DOS/ logic address\n"
"    echo clkmsr {<index>} > debug ; Output clk source value, no index then all\n"
"    echo thread {<pid>} > debug; Show thread infomation, no pid then all\n"
"    echo stack <pid> > debug; Show thread's stack\n"
};
static const char *syntax_error_str = {"Invalid syntax\n"};

static ssize_t dbg_do_help(struct class *class,
			struct class_attribute *attr,	char *buf)
{
	return sprintf(buf, "%s\n", usage_str);
}

static char *base_addr_type(char base)
{
	switch (base) {
	case '0':
		return "MEMORY";
	case 'c':
	case 'C':
		return "CBUS";
	case 'a':
	case 'A':
		return "AOBUS";
	case 'd':
	case 'D':
		return "DOS";
	case 'v':
	case 'V':
		return "VCBUS";
	default:
		break;
	}
	return "INVALID";
}
/* Command Works */
unsigned int raw_read_work(char base, unsigned int address)
{
	unsigned int value = 0;
	switch (base) {
	case 'c':
	case 'C':
		value = aml_read_cbus(address);
		break;
	case 'a':
	case 'A':
		value = aml_read_aobus(address);
		break;
	case 'd':
	case 'D':
		value = aml_read_dosbus(address);
		break;
	case 'v':
	case 'V':
		value = aml_read_vcbus(address);
		break;
	default:
		break;
	}
	return value;
}
static int  do_read_work(char argn , char **argv)
{
	char base;
	char *type = NULL;
	unsigned int address, value;

	if (argn < 2) {
		pr_info("%s", syntax_error_str);
		return -1;
	}

	base = argv[1][0];
	if (base == '0' || argn == 2) {
		address = simple_strtol(argv[1], NULL, 16);
		base = '0';
	} else
		address = simple_strtol(argv[2], NULL, 16);

	type = base_addr_type(base);
	value = raw_read_work(base, address);
	if (base == '0')
		pr_info("%s[0x%08x]=0x%08x\n", type, address, value);
	else
		pr_info("%s[0x%04x]=0x%08x\n", type, address, value);
	return 0;
}

static int  do_dump_work(char argn , char **argv)
{
	char base;
	char *type = NULL;
	unsigned int start, end, value;

	if (argn < 4) {
		printk("%s", syntax_error_str);
		return -1;
	}

	base = argv[1][0];
	type = base_addr_type(base);
	start = simple_strtol(argv[2], NULL, 16);
	end = simple_strtol(argv[3], NULL, 16);

	do {
			value = raw_read_work(base, start);
			printk("%s[0x%04x]=0x%08x\n", type, start, value);
		start++;
	} while (start <= end);
	return 0;
}

static int  do_dumpn_work(char argn , char **argv)
{
	char base;
	char *type = NULL;
	unsigned int start, end, value;

	if (argn < 4) {
		printk("%s", syntax_error_str);
		return -1;
	}

	base = argv[1][0];
	type = base_addr_type(base);
	start = simple_strtol(argv[2], NULL, 16);
	end = simple_strtol(argv[3], NULL, 16);

	do {
			value = raw_read_work(base, start);
	    if (value)
		printk("%s[0x%04x]=0x%08x\n", type, start, value);
		start++;
	} while (start <= end);
	return 0;
}
int do_clk_measure_work(char argn , char **argv)
{
	if (argn == 1)
		meson_clk_measure(0xff);
	 else
		meson_clk_measure(simple_strtol(argv[1], NULL, 10));
	return 0;
}

static int do_write_work(char argn , char **argv)
{
	char base;
	char *type = NULL;
	unsigned int address, value;

	if (argn < 3) {
		pr_info("%s", syntax_error_str);
		return -1;
	}

	base = argv[2][0];
	if (base == '0' || argn == 3) {
		address = simple_strtol(argv[2], NULL, 16);
		base = '0';
	} else
		address = simple_strtol(argv[3], NULL, 16);

	type = base_addr_type(base);
	value = simple_strtol(argv[1], NULL, 16);

	switch (base) {
	case 'c':
	case 'C':
		aml_write_cbus(address, value);
		break;
	case 'a':
	case 'A':
		aml_write_aobus(address, value);
		break;
	case 'd':
	case 'D':
		aml_write_dosbus(address, value);
		break;
	case 'v':
	case 'V':
		aml_write_vcbus(address, value);
		break;
	default:
		break;
	}
	if (base == '0')
		pr_info("Write %s[0x%08x]=0x%08x\n", type, address,
				raw_read_work(base, address));
	else
		pr_info("Write %s[0x%04x]=0x%08x\n", type, address,
				raw_read_work(base, address));
	return 0;
}

int do_thread_show_work(char argn , char **argv)
{
	struct task_struct *p;
	int pid;

	if (argn != 1 && argn != 2) {
		pr_info("%s", syntax_error_str);
		return 0;
	}

	pr_info("pid:	state:	task:	name:\n");
	if (argn == 1) {
		for_each_process(p) {
			pid = pid_vnr(get_task_pid(p, PIDTYPE_PID));
			pr_info("%4d: \t%ld\t%p\t%s\n",
					pid, p->state, p, p->comm);
		}
	} else{
		pid = simple_strtol(argv[1], NULL, 10);
		p = pid_task(find_vpid(pid), PIDTYPE_PID);
		pr_info("%5d: \t%ld\t%p\t%s\n", pid, p->state, p, p->comm);
	}
	return 0;
}
int do_stack_show_work(char argn , char **argv)
{
	pid_t pid;
	struct task_struct *task;

	if (argn != 2) {
		pr_info("%s", syntax_error_str);
	} else{
		pid = simple_strtol(argv[1], NULL, 10);
		rcu_read_lock();
		task = pid_task(find_vpid(pid), PIDTYPE_PID);
		rcu_read_unlock();
		if (task)
			sched_show_task(task);
		else
			pr_info("PID %d not found\n", pid);
	}
	return 0;
}

#ifdef CONFIG_GPIO_TEST
static struct wake_lock	debug_lock;

#define MAX_ARG_NUM 8

int do_gpio_test_work(char argn , char **argv)
{
	if (argn < 2 || argn > MAX_ARG_NUM) {
		pr_info("%s", syntax_error_str);
	} else{
		wake_lock(&debug_lock);
		gpiotest(argn, argv);
		wake_unlock(&debug_lock);
	}
	return 0;
}
#else

#define MAX_ARG_NUM 4

#endif /*CONFIG_GPIO_TEST*/



/* Main Command Dispatcher */
static ssize_t dbg_do_command(struct class *class,
			struct class_attribute *attr,
			const char *buf, size_t count)
{

	int argn;
	char *buf_work, *p, *para;
	char *argv[MAX_ARG_NUM];
	char cmd, work_mode;

	buf_work = kstrdup(buf, GFP_KERNEL);
	p = buf_work;

	for (argn = 0; argn < MAX_ARG_NUM; argn++) {
		para = strsep(&p, " ");
		if (para == NULL)
			break;
		argv[argn] = para;
	}

	if (argn < 1 || argn > MAX_ARG_NUM)
		goto end;

	cmd = argv[0][0];
	switch (cmd) {
	case 'r':
	case 'R':
		work_mode = WORK_MODE_READ;
		do_read_work(argn, argv);
		break;
	case 'w':
	case 'W':
		work_mode = WORK_MODE_WRITE;
		do_write_work(argn, argv);
		break;
	case 'c':
	case 'C':
		work_mode = WORK_MODE_CLKMEASURE;
		do_clk_measure_work(argn, argv);
		break;
	case 'd':
	case 'D':
		work_mode = WORK_MODE_DUMP;
	if ((argv[0][1] == 'n') || (argv[0][1] == 'N') || (argv[0][4] == 'n') || (argv[0][4] == 'N'))
	    do_dumpn_work(argn, argv);
	else
	    do_dump_work(argn, argv);
		break;
	case 't':
	case 'T':
		work_mode = WORK_MODE_THREAD;
		do_thread_show_work(argn, argv);
		break;
	case 's':
	case 'S':
		work_mode = WORK_MODE_STACK;
		do_stack_show_work(argn, argv);
		break;

#ifdef CONFIG_GPIO_TEST
	case 'g':
	case 'G':
		work_mode = WORK_MODE_GPIOTEST;
		do_gpio_test_work(argn, argv);
		break;
#endif /* CONFIG_GPIO_TEST */

	default:
		goto end;
	}

	kfree(buf_work);
	return count;
end:
	pr_info("error command!\n");
	kfree(buf_work);
	return -EINVAL;

}

static CLASS_ATTR(debug, S_IWUSR | S_IRUGO, dbg_do_help, dbg_do_command);
static CLASS_ATTR(help, S_IWUSR | S_IRUGO, dbg_do_help, NULL);

struct class *aml_sys_class;
EXPORT_SYMBOL(aml_sys_class);
static int __init aml_debug_init(void)
{
	int ret = 0;

	aml_sys_class = class_create(THIS_MODULE, CLASS_NAME);

	ret = class_create_file(aml_sys_class, &class_attr_debug);
	ret = class_create_file(aml_sys_class, &class_attr_help);

#ifdef CONFIG_GPIO_TEST
	wake_lock_init(&debug_lock, WAKE_LOCK_SUSPEND, "gpiotest");
#endif /* CONFIG_GPIO_TEST */

	return ret;
}

static void __exit aml_debug_exit(void)
{
#ifdef CONFIG_GPIO_TEST
	wake_lock_destroy(&debug_lock);
#endif /* CONFIG_GPIO_TEST */

	class_destroy(aml_sys_class);
}


module_init(aml_debug_init);
module_exit(aml_debug_exit);

MODULE_DESCRIPTION("Amlogic debug module");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Victor Wan <victor.wan@amlogic.com>");

