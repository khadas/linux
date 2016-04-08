#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/platform_device.h>
#include <linux/amlogic/iomap.h>
#include <linux/of.h>
#include <linux/of_fdt.h>

#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/compat.h>
#include <linux/random.h>
#include <linux/ptrace.h>
#include <linux/hw_breakpoint.h>
#include <linux/amlogic/stacktrace_arm64.h>
#include <linux/amlogic/aml_watch_point.h>
#include <linux/cpu.h>
#include <linux/smp.h>
#include <asm/cputype.h>
#include <asm/system_misc.h>
#include <asm/debug-monitors.h>
#include <uapi/linux/hw_breakpoint.h>
#include <linux/kallsyms.h>

#ifdef CONFIG_ARM64
/*
 * macro for test
 */
#define WATCH_TEST		0

/*
 * watch point struct
 * @used:       whether this watch point is used
 * @watch_addr: virtual address you want to watch
 * @ctrl:       control value of this address
 * @handler:    event handler if watch point triggered
 */
struct aml_watch_point {
	unsigned int used;
	unsigned int idx;
	unsigned long watch_addr;
	struct arch_hw_breakpoint_ctrl ctrl;
	wp_handle handler;
};

DEFINE_SPINLOCK(watch_lock);
static int num_watch_points;
static struct aml_watch_point *wp;
static DEFINE_PER_CPU(struct aml_watch_point *, cpu_wp);

/*
 * MDSCR access routines.
 */
static void mdscr_write(u32 mdscr)
{
	unsigned long flags;
	local_dbg_save(flags);
	asm volatile("msr mdscr_el1, %0" : : "r" (mdscr));
	local_dbg_restore(flags);
}

static u32 mdscr_read(void)
{
	u32 mdscr;
	asm volatile("mrs %0, mdscr_el1" : "=r" (mdscr));
	return mdscr;
}

static void debug_enable(int en)
{
	unsigned int val;

	val = mdscr_read();

	if (en)
		val |=  (DBG_MDSCR_MDE | DBG_MDSCR_KDE);
	else
		val &= ~(DBG_MDSCR_MDE | DBG_MDSCR_KDE);

	mdscr_write(val);
	local_dbg_enable();
}

static void single_step_enable(int en)
{
	unsigned int val;

	val = mdscr_read();

	if (en)
		val |=  (DBG_MDSCR_SS);
	else
		val &= ~(DBG_MDSCR_SS);

	mdscr_write(val);
}

static int get_num_wrps(void)
{
	return ((read_cpuid(ID_AA64DFR0_EL1) >> 20) & 0xf) + 1;
}

static u64 read_wb_reg(int reg, int n)
{
	u64 v = 0;

	switch (reg + n) {
	case (AARCH64_DBG_REG_WVR + 0):
		AARCH64_DBG_READ(0, AARCH64_DBG_REG_NAME_WVR, v);
		break;
	case (AARCH64_DBG_REG_WVR + 1):
		AARCH64_DBG_READ(1, AARCH64_DBG_REG_NAME_WVR, v);
		break;
	case (AARCH64_DBG_REG_WVR + 2):
		AARCH64_DBG_READ(2, AARCH64_DBG_REG_NAME_WVR, v);
		break;
	case (AARCH64_DBG_REG_WVR + 3):
		AARCH64_DBG_READ(3, AARCH64_DBG_REG_NAME_WVR, v);
		break;
	case (AARCH64_DBG_REG_WVR + 4):
		AARCH64_DBG_READ(4, AARCH64_DBG_REG_NAME_WVR, v);
		break;
	case (AARCH64_DBG_REG_WVR + 5):
		AARCH64_DBG_READ(5, AARCH64_DBG_REG_NAME_WVR, v);
		break;
	case (AARCH64_DBG_REG_WVR + 6):
		AARCH64_DBG_READ(6, AARCH64_DBG_REG_NAME_WVR, v);
		break;
	case (AARCH64_DBG_REG_WVR + 7):
		AARCH64_DBG_READ(7, AARCH64_DBG_REG_NAME_WVR, v);
		break;
	case (AARCH64_DBG_REG_WVR + 8):
		AARCH64_DBG_READ(8, AARCH64_DBG_REG_NAME_WVR, v);
		break;
	case (AARCH64_DBG_REG_WVR + 9):
		AARCH64_DBG_READ(9, AARCH64_DBG_REG_NAME_WVR, v);
		break;
	case (AARCH64_DBG_REG_WVR + 10):
		AARCH64_DBG_READ(10, AARCH64_DBG_REG_NAME_WVR, v);
		break;
	case (AARCH64_DBG_REG_WVR + 11):
		AARCH64_DBG_READ(11, AARCH64_DBG_REG_NAME_WVR, v);
		break;
	case (AARCH64_DBG_REG_WVR + 12):
		AARCH64_DBG_READ(12, AARCH64_DBG_REG_NAME_WVR, v);
		break;
	case (AARCH64_DBG_REG_WVR + 13):
		AARCH64_DBG_READ(13, AARCH64_DBG_REG_NAME_WVR, v);
		break;
	case (AARCH64_DBG_REG_WVR + 14):
		AARCH64_DBG_READ(14, AARCH64_DBG_REG_NAME_WVR, v);
		break;
	case (AARCH64_DBG_REG_WVR + 15):
		AARCH64_DBG_READ(15, AARCH64_DBG_REG_NAME_WVR, v);
		break;

	case (AARCH64_DBG_REG_WCR + 0):
		AARCH64_DBG_READ(0, AARCH64_DBG_REG_NAME_WCR, v);
		break;
	case (AARCH64_DBG_REG_WCR + 1):
		AARCH64_DBG_READ(1, AARCH64_DBG_REG_NAME_WCR, v);
		break;
	case (AARCH64_DBG_REG_WCR + 2):
		AARCH64_DBG_READ(2, AARCH64_DBG_REG_NAME_WCR, v);
		break;
	case (AARCH64_DBG_REG_WCR + 3):
		AARCH64_DBG_READ(3, AARCH64_DBG_REG_NAME_WCR, v);
		break;
	case (AARCH64_DBG_REG_WCR + 4):
		AARCH64_DBG_READ(4, AARCH64_DBG_REG_NAME_WCR, v);
		break;
	case (AARCH64_DBG_REG_WCR + 5):
		AARCH64_DBG_READ(5, AARCH64_DBG_REG_NAME_WCR, v);
		break;
	case (AARCH64_DBG_REG_WCR + 6):
		AARCH64_DBG_READ(6, AARCH64_DBG_REG_NAME_WCR, v);
		break;
	case (AARCH64_DBG_REG_WCR + 7):
		AARCH64_DBG_READ(7, AARCH64_DBG_REG_NAME_WCR, v);
		break;
	case (AARCH64_DBG_REG_WCR + 8):
		AARCH64_DBG_READ(8, AARCH64_DBG_REG_NAME_WCR, v);
		break;
	case (AARCH64_DBG_REG_WCR + 9):
		AARCH64_DBG_READ(9, AARCH64_DBG_REG_NAME_WCR, v);
		break;
	case (AARCH64_DBG_REG_WCR + 10):
		AARCH64_DBG_READ(10, AARCH64_DBG_REG_NAME_WCR, v);
		break;
	case (AARCH64_DBG_REG_WCR + 11):
		AARCH64_DBG_READ(11, AARCH64_DBG_REG_NAME_WCR, v);
		break;
	case (AARCH64_DBG_REG_WCR + 12):
		AARCH64_DBG_READ(12, AARCH64_DBG_REG_NAME_WCR, v);
		break;
	case (AARCH64_DBG_REG_WCR + 13):
		AARCH64_DBG_READ(13, AARCH64_DBG_REG_NAME_WCR, v);
		break;
	case (AARCH64_DBG_REG_WCR + 14):
		AARCH64_DBG_READ(14, AARCH64_DBG_REG_NAME_WCR, v);
		break;
	case (AARCH64_DBG_REG_WCR + 15):
		AARCH64_DBG_READ(15, AARCH64_DBG_REG_NAME_WCR, v);
		break;
	default:
		pr_warn("attempt to read from unknown register %d\n", n);
	}

	return v;
}

static int dump_watch_point_reg(char *buf)
{
	int i;
	unsigned long value;
	int size = 0;

	for (i = 0; i < num_watch_points; i++) {
		value = read_wb_reg(AARCH64_DBG_REG_WVR, i);
		size += sprintf(buf + size, "DBG_REG_WVR%d:%lx\n", i, value);
		value = read_wb_reg(AARCH64_DBG_REG_WCR, i);
		size += sprintf(buf + size, "DBG_REG_WCR%d:%lx\n", i, value);
	}
	return size;
}

static void write_wb_reg(int reg, int n, u64 v)
{
	switch (reg + n) {
	case (AARCH64_DBG_REG_WVR + 0):
		AARCH64_DBG_WRITE(0, AARCH64_DBG_REG_NAME_WVR, v);
		break;
	case (AARCH64_DBG_REG_WVR + 1):
		AARCH64_DBG_WRITE(1, AARCH64_DBG_REG_NAME_WVR, v);
		break;
	case (AARCH64_DBG_REG_WVR + 2):
		AARCH64_DBG_WRITE(2, AARCH64_DBG_REG_NAME_WVR, v);
		break;
	case (AARCH64_DBG_REG_WVR + 3):
		AARCH64_DBG_WRITE(3, AARCH64_DBG_REG_NAME_WVR, v);
		break;
	case (AARCH64_DBG_REG_WVR + 4):
		AARCH64_DBG_WRITE(4, AARCH64_DBG_REG_NAME_WVR, v);
		break;
	case (AARCH64_DBG_REG_WVR + 5):
		AARCH64_DBG_WRITE(5, AARCH64_DBG_REG_NAME_WVR, v);
		break;
	case (AARCH64_DBG_REG_WVR + 6):
		AARCH64_DBG_WRITE(6, AARCH64_DBG_REG_NAME_WVR, v);
		break;
	case (AARCH64_DBG_REG_WVR + 7):
		AARCH64_DBG_WRITE(7, AARCH64_DBG_REG_NAME_WVR, v);
		break;
	case (AARCH64_DBG_REG_WVR + 8):
		AARCH64_DBG_WRITE(8, AARCH64_DBG_REG_NAME_WVR, v);
		break;
	case (AARCH64_DBG_REG_WVR + 9):
		AARCH64_DBG_WRITE(9, AARCH64_DBG_REG_NAME_WVR, v);
		break;
	case (AARCH64_DBG_REG_WVR + 10):
		AARCH64_DBG_WRITE(10, AARCH64_DBG_REG_NAME_WVR, v);
		break;
	case (AARCH64_DBG_REG_WVR + 11):
		AARCH64_DBG_WRITE(11, AARCH64_DBG_REG_NAME_WVR, v);
		break;
	case (AARCH64_DBG_REG_WVR + 12):
		AARCH64_DBG_WRITE(12, AARCH64_DBG_REG_NAME_WVR, v);
		break;
	case (AARCH64_DBG_REG_WVR + 13):
		AARCH64_DBG_WRITE(13, AARCH64_DBG_REG_NAME_WVR, v);
		break;
	case (AARCH64_DBG_REG_WVR + 14):
		AARCH64_DBG_WRITE(14, AARCH64_DBG_REG_NAME_WVR, v);
		break;
	case (AARCH64_DBG_REG_WVR + 15):
		AARCH64_DBG_WRITE(15, AARCH64_DBG_REG_NAME_WVR, v);
		break;

	case (AARCH64_DBG_REG_WCR + 0):
		AARCH64_DBG_WRITE(0, AARCH64_DBG_REG_NAME_WCR, v);
		break;
	case (AARCH64_DBG_REG_WCR + 1):
		AARCH64_DBG_WRITE(1, AARCH64_DBG_REG_NAME_WCR, v);
		break;
	case (AARCH64_DBG_REG_WCR + 2):
		AARCH64_DBG_WRITE(2, AARCH64_DBG_REG_NAME_WCR, v);
		break;
	case (AARCH64_DBG_REG_WCR + 3):
		AARCH64_DBG_WRITE(3, AARCH64_DBG_REG_NAME_WCR, v);
		break;
	case (AARCH64_DBG_REG_WCR + 4):
		AARCH64_DBG_WRITE(4, AARCH64_DBG_REG_NAME_WCR, v);
		break;
	case (AARCH64_DBG_REG_WCR + 5):
		AARCH64_DBG_WRITE(5, AARCH64_DBG_REG_NAME_WCR, v);
		break;
	case (AARCH64_DBG_REG_WCR + 6):
		AARCH64_DBG_WRITE(6, AARCH64_DBG_REG_NAME_WCR, v);
		break;
	case (AARCH64_DBG_REG_WCR + 7):
		AARCH64_DBG_WRITE(7, AARCH64_DBG_REG_NAME_WCR, v);
		break;
	case (AARCH64_DBG_REG_WCR + 8):
		AARCH64_DBG_WRITE(8, AARCH64_DBG_REG_NAME_WCR, v);
		break;
	case (AARCH64_DBG_REG_WCR + 9):
		AARCH64_DBG_WRITE(9, AARCH64_DBG_REG_NAME_WCR, v);
		break;
	case (AARCH64_DBG_REG_WCR + 10):
		AARCH64_DBG_WRITE(10, AARCH64_DBG_REG_NAME_WCR, v);
		break;
	case (AARCH64_DBG_REG_WCR + 11):
		AARCH64_DBG_WRITE(11, AARCH64_DBG_REG_NAME_WCR, v);
		break;
	case (AARCH64_DBG_REG_WCR + 12):
		AARCH64_DBG_WRITE(12, AARCH64_DBG_REG_NAME_WCR, v);
		break;
	case (AARCH64_DBG_REG_WCR + 13):
		AARCH64_DBG_WRITE(13, AARCH64_DBG_REG_NAME_WCR, v);
		break;
	case (AARCH64_DBG_REG_WCR + 14):
		AARCH64_DBG_WRITE(14, AARCH64_DBG_REG_NAME_WCR, v);
		break;
	case (AARCH64_DBG_REG_WCR + 15):
		AARCH64_DBG_WRITE(15, AARCH64_DBG_REG_NAME_WCR, v);
		break;
	default:
		pr_warn("attempt to write to unknown register %d\n", n);
	}
	isb();
}

static int addr_valid(unsigned long addr, struct aml_watch_point *wp)
{
	unsigned int len;

	if (wp == NULL || wp->used == 0)
		return 0;

	/* right now we don't support large range mask */
	switch (wp->ctrl.len) {
	case ARM_BREAKPOINT_LEN_1:
		len = HW_BREAKPOINT_LEN_1;
		break;

	case ARM_BREAKPOINT_LEN_2:
		len = HW_BREAKPOINT_LEN_2;
		break;

	case ARM_BREAKPOINT_LEN_4:
		len = HW_BREAKPOINT_LEN_4;
		break;

	case ARM_BREAKPOINT_LEN_8:
		len = HW_BREAKPOINT_LEN_8;
		break;

	default:
		return 0;
	}

	/* far is in watch address and len range */
	if ((wp->watch_addr < addr + len) &&
	    (wp->watch_addr > addr - len)) {
		return 1;
	}
	return 0;
}

static void watch_point_enable(int id, int en)
{
	unsigned int ctrl = read_wb_reg(AARCH64_DBG_REG_WCR, id);

	if (en)
		ctrl |= 1;
	else
		ctrl &= ~1;

	write_wb_reg(AARCH64_DBG_REG_WCR, id, ctrl);
}

void dump_backtrace(struct pt_regs *regs, struct task_struct *tsk)
{
	struct stackframe frame;
	const register unsigned long current_sp asm ("sp");

	if (!tsk)
		tsk = current;

	if (regs) {
		frame.fp = regs->regs[29];
		frame.sp = regs->sp;
		frame.pc = regs->pc;
	} else if (tsk == current) {
		frame.fp = (unsigned long)__builtin_frame_address(0);
		frame.sp = current_sp;
		frame.pc = (unsigned long)dump_backtrace;
	} else {
		/*
		 * task blocked in __switch_to
		 */
		frame.fp = thread_saved_fp(tsk);
		frame.sp = thread_saved_sp(tsk);
		frame.pc = thread_saved_pc(tsk);
	}

	pr_info("Call trace:\n");
	while (1) {
		unsigned long where = frame.pc;
		int ret;

		ret = unwind_frame(&frame);
		if (ret < 0)
			break;
		print_ip_sym(where);
	}
}

static void spsr_ss_set(struct pt_regs *regs, int en)
{

	unsigned long spsr;

	spsr = regs->pstate;
	if (en)
		spsr |= DBG_SPSR_SS;
	else
		spsr &= ~DBG_SPSR_SS;
	regs->pstate = spsr;
}

static int aml_watchpoint_handler(unsigned long addr, unsigned int esr,
				  struct pt_regs *regs)
{
	int i, ret, handled = 0, valid = 0;
	unsigned long pc = regs->pc;
	struct aml_watch_point *awp = NULL;

	spin_lock(&watch_lock);
	for (i = 0; i < num_watch_points; i++) {
		if (addr_valid(addr, &wp[i])) {
			awp = &wp[i];
			watch_point_enable(i, 0);
			this_cpu_write(cpu_wp, awp);
			/* handle this event by customer */
			if (awp->handler) {
				ret = awp->handler(addr, esr, regs);
				handled = 1;
			}
			valid = 1;
			break;
		}
	}

	if (handled) {
		/* re-enable watchpoint and debug control */
		spin_unlock(&watch_lock);
		spsr_ss_set(regs, 1);
		single_step_enable(1);
		return ret;
	}

	/* default handle for this event */
	if (valid) {
		spin_unlock(&watch_lock);
		pr_info("---- watch point %d triggered, watch addr:%lx ----\n",
			awp->idx, awp->watch_addr);
		pr_info("[%d]%s, fault addr:%016lx, esr:%08x, mdscr:%x\n",
			current->pid, current->comm, addr, esr, mdscr_read());
		dump_backtrace(regs, current);
		/*
		 * NOTE:
		 * watch point exception will re-execuate instrution which
		 * triggled exception after return. This behavior will cause
		 * watch point exception trigger again and again. Although add
		 * pc to next instruction will avoid this problem, but
		 * instruction at trigger point will not be excuated. We need
		 * use single step exception to fix this problem.
		 */
		/* regs->pc += 4;*/
		spsr_ss_set(regs, 1);
		single_step_enable(1);
		return 0;
	}

	/* this should never happen */
	spin_unlock(&watch_lock);
	pr_err("%s, unabled to handle watch point address:%lx, pc:%lx\n",
		__func__, addr, pc);
	regs->pc += 4;
	return 1;
}

static int aml_single_step_handler(unsigned long addr, unsigned int esr,
				   struct pt_regs *regs)
{
	struct aml_watch_point *awp = NULL;

	awp = this_cpu_read(cpu_wp);
	pr_debug("%s, addr:%lx, esr:%x, awp:%p\n", __func__, addr, esr, awp);
	single_step_enable(0);
	if (awp != NULL)
		watch_point_enable(awp->idx, 1);
	return 0;
}

static void smp_wp_set(void *info)
{
	struct aml_watch_point *awp;

	awp = (struct aml_watch_point *)info;

	if (awp == NULL)
		return;

	write_wb_reg(AARCH64_DBG_REG_WVR, awp->idx, awp->watch_addr);
	write_wb_reg(AARCH64_DBG_REG_WCR, awp->idx, encode_ctrl_reg(awp->ctrl));
	debug_enable(1);
	mb();	/* ensure sequence */
}

static void smp_wp_clr(void *info)
{
	struct aml_watch_point *awp;

	awp = (struct aml_watch_point *)info;

	if (awp == NULL)
		return;

	write_wb_reg(AARCH64_DBG_REG_WVR, awp->idx, 0);
	write_wb_reg(AARCH64_DBG_REG_WCR, awp->idx, 0);
	mb();	/* ensure sequence */
}

static int aml_watch_point_cpu_notify(struct notifier_block *self,
				      unsigned long action, void *data)
{
	int cpu = (unsigned long)data;
	int i, used = 0;
	struct aml_watch_point *awp;

	if (action == CPU_ONLINE) {
		/* init all registed watch point for online cpu */
		spin_lock(&watch_lock);
		for (i = 0; i < num_watch_points; i++) {
			awp = &wp[i];
			if (!awp->used)
				continue;
			used++;
			smp_call_function_single(cpu, smp_wp_set, awp, 1);
		}
		spin_unlock(&watch_lock);
		if (used) {
			pr_info("cpu%d enabled %d watch points when up\n",
				cpu, used);
		}
	}
	return NOTIFY_OK;
}

static struct notifier_block aml_wp_nb = {
	.notifier_call = aml_watch_point_cpu_notify,
};

/* register a watch pointer */
int aml_watch_point_register(unsigned long addr,
			     unsigned int len,
			     unsigned int type,
			     unsigned int privilege,
			     wp_handle handle)
{
	int i, mask;
	struct aml_watch_point *awp = NULL;

	/* parameter check */
	if (addr < PAGE_OFFSET)
		pr_warn("input addr:%ld is not kernel address\n", addr);

	if ((len != HW_BREAKPOINT_LEN_8) &&
	    (len != HW_BREAKPOINT_LEN_4) &&
	    (len != HW_BREAKPOINT_LEN_2) &&
	    (len != HW_BREAKPOINT_LEN_1)) {
		pr_err("bad input len:%d\n", len);
		return -EINVAL;
	}
	if (type & ~(ARM_BREAKPOINT_LOAD | ARM_BREAKPOINT_STORE)) {
		pr_err("bad input type:%d\n", type);
		return -EINVAL;
	}
	if (privilege & ~(AARCH64_BREAKPOINT_EL1 | AARCH64_BREAKPOINT_EL0)) {
		pr_err("bad input privilege:%d\n", privilege);
		return -EINVAL;
	}

	/* check if all watch points are used */
	spin_lock(&watch_lock);
	for (i = 0; i < num_watch_points; i++) {
		if (wp[i].used == 0) {
			awp = &wp[i];
			break;
		}
	}
	if (i == num_watch_points) {
		spin_unlock(&watch_lock);
		pr_err("%s, watch point is all used\n", __func__);
		return -ENODEV;
	}

	/* initialize parameters and update register */
	mask = (1 << len) - 1;
	awp->used           = 1;
	awp->idx            = i;
	awp->watch_addr     = addr;
	awp->ctrl.len       = mask;
	awp->ctrl.type      = type;
	awp->ctrl.privilege = privilege;
	awp->ctrl.enabled   = 1;
	awp->handler        = handle;

	/* set watch point on this cpu */
	write_wb_reg(AARCH64_DBG_REG_WVR, awp->idx, awp->watch_addr);
	write_wb_reg(AARCH64_DBG_REG_WCR, awp->idx, encode_ctrl_reg(awp->ctrl));
	debug_enable(1);

	/* set watch point on other cpu */
	smp_call_function(smp_wp_set, awp, 1);
	spin_unlock(&watch_lock);
	pr_info("watch point[%d], addr:%lx, len:%d, type:%x, privilege:%x\n",
		i, addr, len, type, privilege);
	return 0;
}
EXPORT_SYMBOL(aml_watch_point_register);

/* remove watch point according given address */
void aml_watch_point_remove(unsigned long addr)
{
	int i;

	spin_lock(&watch_lock);
	for (i = 0; i < num_watch_points; i++) {
		if (wp[i].used == 1 && wp[i].watch_addr == addr) {
			write_wb_reg(AARCH64_DBG_REG_WVR, i, 0);
			write_wb_reg(AARCH64_DBG_REG_WCR, i, 0);
			smp_call_function(smp_wp_clr, &wp[i], 1);
			memset(&wp[i], 0, sizeof(*wp));
			break;
		}
	}
	spin_unlock(&watch_lock);
}
EXPORT_SYMBOL(aml_watch_point_remove);

/*
 * force clear a watch point
 */
static void aml_watch_point_clear(int idx)
{
	spin_lock(&watch_lock);
	if (wp[idx].used == 1) {
		write_wb_reg(AARCH64_DBG_REG_WVR, idx, 0);
		write_wb_reg(AARCH64_DBG_REG_WCR, idx, 0);
		smp_call_function(smp_wp_clr, &wp[idx], 1);
		memset(&wp[idx], 0, sizeof(*wp));
	}
	spin_unlock(&watch_lock);
}

static ssize_t num_watch_points_show(struct class *cla,
	struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", num_watch_points);
}

static ssize_t watch_addr_show(struct class *cla,
	struct class_attribute *attr, char *buf)
{
	int i, s = 0;

	spin_lock(&watch_lock);
	for (i = 0; i < num_watch_points; i++) {
		s += sprintf(buf + s, "id:%d, used:%d, addr:0x%016lx, ",
			    i, wp[i].used, wp[i].watch_addr);
		s += sprintf(buf + s, "len:%08x, type:%x, privilege:%x, ",
			    wp[i].ctrl.len, wp[i].ctrl.type,
			    wp[i].ctrl.privilege);
		s += sprintf(buf + s, "handler:%pf\n",
			    wp[i].handler);
	}
	spin_unlock(&watch_lock);
	return s;
}

static ssize_t watch_addr_store(struct class *cla,
	struct class_attribute *attr, const char *buf, size_t count)
{
	unsigned long addr;
	u32 len = HW_BREAKPOINT_LEN_8;
	u32 type = ARM_BREAKPOINT_LOAD | ARM_BREAKPOINT_STORE;
	u32 privilege = AARCH64_BREAKPOINT_EL1;
	int ret;

	ret = sscanf(buf, "%lx %d %x %d", &addr, &len, &type, &privilege);
	if (ret < 1)
		return -EINVAL;

	ret = aml_watch_point_register(addr, len, type, privilege, NULL);

	return count;
}

static ssize_t clear_store(struct class *cla,
	struct class_attribute *attr, const char *buf, size_t count)
{
	int i;
	int idx = -1;

	i = sscanf(buf, "%d", &idx);
	if (idx >= num_watch_points) {
		pr_err("input index %d out of range:[0 - %d]\n",
			idx, num_watch_points);
		return -EINVAL;
	}

	/* negative value means clear all watch point */
	if (idx < 0) {
		for (i = 0; i < num_watch_points; i++)
			aml_watch_point_clear(i);
	} else {
		aml_watch_point_clear(idx);
	}

	return count;
}

#if WATCH_TEST
unsigned long test_value;
EXPORT_SYMBOL(test_value);
static struct work_struct test_work;
static long test_demo;
static void test_job(struct work_struct *work)
{
	unsigned long flags;

	flags = arch_local_save_flags();
	test_demo++;
	pr_info("%s, test_demo:%ld, flags:%lx\n", __func__, test_demo, flags);
	msleep(1000);
	schedule_work(&test_work);
}

/*
 * This is a handler demo for watch point
 * In this demo, we think variable 'test_demo' in range [0 ~ 5] is valid,
 * other value are not correct, if we found test_demo is out of range,
 * then we report a bug in kernel
 */
static int aml_wp_test_handle(unsigned long addr,
			      unsigned int esr,
			      struct pt_regs *reg)
{
	/* show who changed test_demo */
	pr_info("%s, test_demo:%ld, pc:%llx, addr:%lx\n",
		__func__, test_demo, reg->pc, addr);
	dump_backtrace(reg, current);

	/* BUG if test_demo out of range */
	if (test_demo > 5 || test_demo < 0)
		BUG();

	return 0;
}

static ssize_t test_handler_show(struct class *cla,
	struct class_attribute *attr, char *buf)
{
	schedule_work(&test_work);
	return 0;
}

static ssize_t test_show(struct class *cla,
	struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%ld\n", test_value);
}

static ssize_t test_store(struct class *cla,
	struct class_attribute *attr, const char *buf, size_t count)
{
	if (sscanf(buf, "%lx", &test_value) != 1)
		return -EINVAL;

	return count;
}

static ssize_t rand_test_show(struct class *cla,
	struct class_attribute *attr, char *buf)
{
	unsigned char *p;
	unsigned long *r;
	int rand;
	int offset;

	p = (unsigned char *)&test_value;
	rand = get_random_int();
	offset = rand % (sizeof(unsigned long));
	p += offset * (rand & 1 ? (1) : (-1));
	r = (unsigned long *)(p);
	return sprintf(buf, "point:%p, valueL:%ld\n", r, *r);
}

static ssize_t rand_test_store(struct class *cla,
	struct class_attribute *attr, const char *buf, size_t count)
{
	unsigned char *p;
	unsigned long *r;
	int rand;

	p = (unsigned char *)&test_value;
	rand = get_random_int();
	p += rand % (sizeof(unsigned long)) * (rand & 1 ? (1) : (-1));
	r = (unsigned long *)(p);
	pr_info("pointer:%p\n", r);
	if (sscanf(buf, "%lx", r) != 1)
		return -EINVAL;

	return count;
}
#endif	/* WATCH_TEST */

static ssize_t dump_show(struct class *cla,
	struct class_attribute *attr, char *buf)
{
	return dump_watch_point_reg(buf);
}

static struct class_attribute watch_point_attr[] = {
	__ATTR(watch_addr, 0664, watch_addr_show, watch_addr_store),
#if WATCH_TEST
	__ATTR(test, 0664, test_show, test_store),
	__ATTR(rand_test, 0664, rand_test_show, rand_test_store),
	__ATTR_RO(test_handler),
#endif
	__ATTR_RO(dump),
	__ATTR_RO(num_watch_points),
	__ATTR_WO(clear),
	__ATTR_NULL
};

static struct class watch_point_class = {
	.name = "watch_point",
	.class_attrs = watch_point_attr,
};

static int aml_watch_point_probe(struct platform_device *pdev)
{
	int r;

	pr_info("%s, in\n", __func__);
	num_watch_points = get_num_wrps();
	wp = kzalloc(sizeof(*wp) * num_watch_points, GFP_KERNEL);
	if (wp == NULL) {
		pr_err("alloc memory failed\n");
		return -EINVAL;
	}
	r = class_register(&watch_point_class);
	if (r) {
		pr_err("regist watch_point_class failed\n");
		return -EINVAL;
	}

	/* hook watch point handler */
	hook_debug_fault_code(DBG_ESR_EVT_HWWP, aml_watchpoint_handler, SIGBUS,
			      0, "aml-watchpoint handler");
	/* hook single step handler */
	hook_debug_fault_code(DBG_ESR_EVT_HWSS, aml_single_step_handler, SIGBUS,
			      0, "aml-singlestep handler");
	/* Register hotplug handler. */
	register_cpu_notifier(&aml_wp_nb);

#if WATCH_TEST
	/* TEST DEMO for handler */
	INIT_WORK(&test_work, test_job);
	test_demo = 0;
	aml_watch_point_register((unsigned long)&test_demo,
				 HW_BREAKPOINT_LEN_8,
				 ARM_BREAKPOINT_STORE,
				 AARCH64_BREAKPOINT_EL1,
				 aml_wp_test_handle);
#endif
	return 0;
}

static int aml_watch_point_drv_remove(struct platform_device *pdev)
{
	unregister_cpu_notifier(&aml_wp_nb);
	class_unregister(&watch_point_class);
	return 0;
}

static struct platform_driver aml_watch_point_driver = {
	.driver = {
		.name  = "aml_watch_point",
		.owner = THIS_MODULE,
	},
	.probe = aml_watch_point_probe,
	.remove = aml_watch_point_drv_remove,
};

static int __init aml_watch_pint_init(void)
{
	struct platform_device *pdev;
	int ret;

	pdev = platform_device_alloc("aml_watch_point", 0);
	if (!pdev) {
		pr_err("alloc pdev aml_watch_point failed\n");
		return -EINVAL;
	}
	ret = platform_device_add(pdev);
	if (ret) {
		pr_err("regist pdev failed, ret:%d\n", ret);
		return ret;
	}
	ret = platform_driver_register(&aml_watch_point_driver);
	return ret;
}

static void __exit aml_watch_point_uninit(void)
{
	platform_driver_unregister(&aml_watch_point_driver);
}

arch_initcall(aml_watch_pint_init);
module_exit(aml_watch_point_uninit);
MODULE_DESCRIPTION("amlogic watch point driver");
MODULE_LICENSE("GPL");
#endif
