#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/major.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include "mach_reg.h"

#define __asmeq(x, y)  ".ifnc " x "," y " ; .err ; .endif\n\t"

void sec_reg_write(unsigned *addr, unsigned value)
{
	register long x0 asm("x0") = 0x82000019;
	register long x1 asm("x1") = (unsigned long)addr;
	register long x2 asm("x2") = value;
	asm volatile(
		__asmeq("%0", "x0")
		__asmeq("%1", "x1")
		__asmeq("%2", "x2")
		"smc #0\n"
		: : "r"(x0), "r"(x1), "r"(x2)
	);
}

unsigned sec_reg_read(unsigned *addr)
{
	register long x0 asm("x0") = 0x82000018;
	register long x1 asm("x1") = (unsigned long)addr;
	asm volatile(
		__asmeq("%0", "x0")
		__asmeq("%1", "x1")
		"smc #0\n"
		: "+r"(x0) : "r"(x1)
	);
	return (unsigned)(x0&0xffffffff);
}
