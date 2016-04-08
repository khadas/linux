
#ifndef __AML_WATCH_POINT_H__
#define __AML_WATCH_POINT_H__

#include <uapi/linux/elf.h>
#include <uapi/linux/hw_breakpoint.h>

#define ARM_BREAKPOINT_LOAD	1
#define ARM_BREAKPOINT_STORE	2

/* Privilege Levels */
#define AARCH64_BREAKPOINT_EL1	1
#define AARCH64_BREAKPOINT_EL0	2

typedef int (*wp_handle)(unsigned long , unsigned int , struct pt_regs *);

#ifdef CONFIG_HAVE_HW_BREAKPOINT
extern int aml_watch_point_register(unsigned long addr,
				    unsigned int len,
				    unsigned int type,
				    unsigned int privilege,
				    wp_handle handle);

extern void aml_watch_point_remove(unsigned long addr);
#else
static inline int aml_watch_point_register(unsigned long addr,
					   unsigned int len,
					   unsigned int type,
					   unsigned int privilege,
					   wp_handle handle)
{
	return -1;
}

static inline void aml_watch_point_remove(unsigned long addr)
{

}
#endif

#endif
