// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/string.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/spinlock.h>
#include <linux/sched.h>
#include <linux/fcntl.h>
#include <asm/irq.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#include <linux/pinctrl/pinmux.h>
#include <linux/gpio/consumer.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/sched/clock.h>
#include <linux/compat.h>
#include <linux/of.h>
#include <linux/kthread.h>
#include <uapi/linux/sched/types.h>

#ifndef MESON_CPU_TYPE
#define MESON_CPU_TYPE 0x50
#endif
#ifndef MESON_CPU_TYPE_MESON6
#define MESON_CPU_TYPE_MESON6		0x60
#endif
#ifdef CONFIG_ARCH_ARC700
#include <asm/arch/am_regs.h>
#else
#endif
#include <linux/poll.h>
#include <linux/delay.h>
/*#include < mach/gpio.h > */
#include <linux/platform_device.h>
#include <linux/slab.h>
//#include <linux/amlogic/aml_gpio_consumer.h>
#define OWNER_NAME "smc"
#if MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON6
#include <mach/mod_gate.h>
#include <mach/power_gate.h>
#endif

#include <linux/version.h>
//#include <linux/amlogic/aml_gpio_consumer.h>
#include <linux/amlogic/amsmc.h>
#include <linux/clk.h>
#include <linux/reset.h>
#include <linux/amlogic/media/utils/vdec_reg.h>
#include <linux/mod_devicetable.h>
#include <linux/ktime.h>

#include "c_stb_regs_define.h"
#include "smc_reg.h"

#define DRIVER_NAME "amsmc_sc2"
#define MODULE_NAME "amsmc_sc2"
#define DEVICE_NAME "amsmc_sc2"
#define CLASS_NAME "amsmc-class"

#define INPUT 0
#define OUTPUT 1
#define OUTLEVEL_LOW 0
#define OUTLEVEL_HIGH 1
#define PULLLOW 1
#define PULLHIGH 0

/*#define FILE_DEBUG*/
#define MEM_DEBUG

#ifdef FILE_DEBUG
#define DBUF_SIZE (512 * 2)
#define pr_dbg(fmt, args...) \
	do { if (smc_debug > 0) { \
			dcnt = sprintf(dbuf, fmt, ## args); \
			debug_write(dbuf, dcnt); \
		} \
	} while (0)
#define pr_dbg printk
#define Fpr(a...) \
	do { \
		if (smc_debug > 1) { \
			dcnt = sprintf(dbuf, a); \
			debug_write(dbuf, dcnt); \
		} \
	} while (0)
#define Ipr		 Fpr

#elif defined(MEM_DEBUG)
#define DBUF_SIZE (512 * 1024)
#define pr_dbg(fmt, args...) \
	do {\
		if (smc_debug > 0) { \
			if (dwrite > (DBUF_SIZE - 512)) { \
				dcnt = sprintf(dbuf + dwrite, fmt, ## args); \
				dwrite += dcnt; \
				dwrite = 0; \
				dread = 0; \
			} else { \
				dcnt = sprintf(dbuf + dwrite, fmt, ## args); \
				dwrite += dcnt; \
			} \
		} \
	} while (0)
#define Fpr(_a...) \
do { \
	if (smc_debug > 1) { \
		if (dwrite > (DBUF_SIZE - 512)) { \
			dcnt = print_time(local_clock(), dbuf + dwrite); \
			dwrite += dcnt; \
			dcnt = sprintf(dbuf + dwrite, _a); \
			dwrite += dcnt; \
			dwrite = 0; \
			dread = 0; \
		} else { \
			dcnt = print_time(local_clock(), dbuf + dwrite); \
			dwrite += dcnt; \
			dcnt = sprintf(dbuf + dwrite, _a); \
			dwrite += dcnt; \
		} \
	} \
} while (0)
#define Ipr		 Fpr

#else
//#if 1
#define pr_dbg(fmt, args...) \
do {\
	if (smc_debug > 0) \
		pr_err("Smartcard: " fmt, ## args); \
} \
while (0)
#define Fpr(a...)	do { if (smc_debug > 1) printk(a); } while (0)
#define Ipr		 Fpr
//#else
//#define pr_dbg(fmt, args...)
//#define Fpr(a...)
//#endif
#endif

#define pr_error(fmt, args...) pr_err("Smartcard: " fmt, ## args)
#define pr_inf(fmt, args...)  pr_err(fmt, ## args)
#define DEBUG_FILE_NAME	 "/storage/external_storage/debug.smc"
static struct file *debug_filp;
static loff_t debug_file_pos;
static int smc_debug;
static int smc_debug_save;
#ifdef MEM_DEBUG
static char *dbuf;
static int dread, dwrite;
static int dcnt;
#endif
/*no used reset ctl,need use clk in 4.9 kernel*/
static struct clk *aml_smartcard_clk;
static int t5w_smartcard;
static int smartcard_mpll0;

#define REG_READ 0
#define REG_WRITE 1

static void debug_write(const char __user *buf, size_t count)
{
	mm_segment_t old_fs;

	if (!debug_filp)
		return;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	if (count != vfs_write(debug_filp, buf, count, &debug_file_pos))
		pr_info("Failed to write debug file\n");

	set_fs(old_fs);
}

#ifdef MEM_DEBUG
static void open_debug(void)
{
	debug_filp = filp_open(DEBUG_FILE_NAME, O_WRONLY, 0);
	if (IS_ERR(debug_filp)) {
		pr_inf("smartcard: open debug file failed\n");
		debug_filp = NULL;
	} else {
		pr_inf("smc: debug file[%s] open.\n", DEBUG_FILE_NAME);
	}
}

static void close_debug(void)
{
	if (debug_filp) {
		filp_close(debug_filp, current->files);
		debug_filp = NULL;
		debug_file_pos = 0;
	}
}

static size_t print_time(u64 ts, char *buf)
{
	unsigned long rem_nsec;

	rem_nsec = do_div(ts, 1000000000);

	if (!buf)
		return snprintf(NULL, 0, "[%5lu.000000] ", (unsigned long)ts);

	return sprintf(buf, "[%5lu.%06lu] ",
		       (unsigned long)ts, rem_nsec / 1000);
}
#endif

#ifdef CONFIG_OF
static const struct of_device_id smc_dt_match[] = {
	{.compatible = "amlogic,smartcard-sc2",
	 },
	{}
};
#else
#define smc_dt_match NULL
#endif

MODULE_PARM_DESC(smc0_irq, "\n\t\t Irq number of smartcard0");
static int smc0_irq = -1;
module_param(smc0_irq, int, 0644);

MODULE_PARM_DESC(smc0_reset, "\n\t\t Reset GPIO pin of smartcard0");
static int smc0_reset = -1;
module_param(smc0_reset, int, 0644);

MODULE_PARM_DESC(atr_delay, "\n\t\t atr delay");
static int atr_delay;
module_param(atr_delay, int, 0644);

MODULE_PARM_DESC(atr_holdoff, "\n\t\t atr_holdoff");
static int atr_holdoff = 1;
module_param(atr_holdoff, int, 0644);

MODULE_PARM_DESC(cwt_det_en, "\n\t\t cwt_det_en");
static int cwt_det_en;
module_param(cwt_det_en, int, 0644);
MODULE_PARM_DESC(btw_det_en, "\n\t\t btw_det_en");
static int btw_det_en;
module_param(btw_det_en, int, 0644);
MODULE_PARM_DESC(etu_msr_en, "\n\t\t etu_msr_en");
static int etu_msr_en;
module_param(etu_msr_en, int, 0644);
MODULE_PARM_DESC(clock_source, "\n\t\t clock_source");
static int clock_source;
module_param(clock_source, int, 0644);

MODULE_PARM_DESC(atr_timeout, "\n\t\t atr_timeout");
static int atr_timeout = 200000;
module_param(atr_timeout, int, 0644);

MODULE_PARM_DESC(atr1_timeout, "\n\t\t atr1_timeout");
static int atr1_timeout = 1000000;
module_param(atr1_timeout, int, 0644);

MODULE_PARM_DESC(atr_reset, "\n\t\t atr_reset");
static int atr_reset = 1;
module_param(atr_reset, int, 0644);

MODULE_PARM_DESC(bwt_time, "\n\t\t bwt_time");
static int bwt_time = BWT_BASE_DEFAULT;
module_param(bwt_time, int, 0644);

MODULE_PARM_DESC(atr_clk_mux, "\n\t\t atr_clk_mux");
static int atr_clk_mux = ATR_CLK_MUX_DEFAULT;
module_param(atr_clk_mux, int, 0644);

MODULE_PARM_DESC(atr_final_tcnt, "\n\t\t atr_final_tcnt");
static int atr_final_tcnt = ATR_FINAL_TCNT_DEFAULT;
module_param(atr_final_tcnt, int, 0644);

MODULE_PARM_DESC(send_use_task, "\n\t\t send_use_task");
static int send_use_task;
module_param(send_use_task, int, 0644);

#define NO_HOT_RESET
/*#define DISABLE_RECV_INT*/
/*#define ATR_FROM_INT*/
#define SW_INVERT
/*#define SMC_FIQ*/
/*#define ATR_OUT_READ*/
#define KEEP_PARAM_AFTER_CLOSE

#define CONFIG_AML_SMARTCARD_GPIO_FOR_DET
#define CONFIG_AM_SMARTCARD_GPIO_FOR_RST

#ifdef CONFIG_AML_SMARTCARD_GPIO_FOR_DET
#define DET_FROM_PIO
#endif
#ifdef CONFIG_AM_SMARTCARD_GPIO_FOR_RST
#define RST_FROM_PIO
#endif

#ifndef CONFIG_MESON_ARM_GIC_FIQ
#undef SMC_FIQ
#endif

#ifdef SMC_FIQ
#include < plat/fiq_bridge.h >
#ifndef ATR_FROM_INT
#define ATR_FROM_INT
#endif
#endif

#define RECV_BUF_SIZE	 1024
#define SEND_BUF_SIZE	 1024

#define RESET_ENABLE	  (smc->reset_level)	/*reset */
#define RESET_DISABLE	 (!smc->reset_level)	/*dis-reset */

enum sc_type {
	SC_DIRECT,
	SC_INVERSE,
};

struct send_smc_task {
	int running;
	int wake_up;
	wait_queue_head_t wait_queue;
	struct task_struct *out_task;
};

struct smc_dev {
	int id;
	struct device *dev;
	struct platform_device *pdev;
	int init;
	int used;
	int cardin;
	int active;
	/*protect dev*/
	struct mutex lock;
	/*protect rw*/
	spinlock_t slock;
	wait_queue_head_t rd_wq;
	wait_queue_head_t wr_wq;
	int recv_start;
	int recv_count;
	int send_start;
	int send_count;
	char recv_buf[RECV_BUF_SIZE];
	char send_buf[SEND_BUF_SIZE];
	char r_cache[RECV_BUF_SIZE];
	char w_cache[SEND_BUF_SIZE];
	struct am_smc_param param;
	struct am_smc_atr atr;

	struct gpio_desc *enable_pin;
#define SMC_ENABLE_PIN_NAME "smc:ENABLE"
	int enable_level;

	struct gpio_desc *enable_5v3v_pin;
#define SMC_ENABLE_5V3V_PIN_NAME "smc:5V3V"
	int enable_5v3v_level;
	int (*reset)(void *smc, int value);
	int irq_num;
	int reset_level;

	u32 pin_clk_pinmux_reg;
	u32 pin_clk_pinmux_bit;
	struct gpio_desc *pin_clk_pin;
#define SMC_CLK_PIN_NAME "smc:PINCLK"
	u32 pin_clk_oen_reg;
	u32 pin_clk_out_reg;
	u32 pin_clk_oebit;
	u32 pin_clk_oubit;
	u32 use_enable_pin;

#ifdef SW_INVERT
	int atr_mode;
	enum sc_type sc_type;
#endif

	int recv_end;
	int send_end;
#ifdef SMC_FIQ
	bridge_item_t smc_fiq_bridge;
#endif

#ifdef DET_FROM_PIO
	struct gpio_desc *detect_pin;
#define SMC_DETECT_PIN_NAME "smc:DETECT"
#endif
#ifdef RST_FROM_PIO
	struct gpio_desc *reset_pin;
#define SMC_RESET_PIN_NAME "smc:RESET"
#endif

	int detect_invert;

	struct pinctrl *pinctrl;

	struct tasklet_struct tasklet;
	struct send_smc_task send_task;
};

#define SMC_DEV_NAME	 "smc_sc2"
#define SMC_CLASS_NAME  "smc_sc2-class"
#define SMC_DEV_COUNT	1

static struct mutex smc_lock;
static int smc_major;
static struct smc_dev smc_dev[SMC_DEV_COUNT];
static int ENA_GPIO_PULL = 1;
static int DIV_SMC = 4;

#ifdef SW_INVERT
static const unsigned char inv_table[256] = {
	0xFF, 0x7F, 0xBF, 0x3F, 0xDF, 0x5F, 0x9F, 0x1F,
	0xEF, 0x6F, 0xAF, 0x2F, 0xCF, 0x4F, 0x8F, 0x0F,
	0xF7, 0x77, 0xB7, 0x37, 0xD7, 0x57, 0x97, 0x17,
	0xE7, 0x67, 0xA7, 0x27, 0xC7, 0x47, 0x87, 0x07,
	0xFB, 0x7B, 0xBB, 0x3B, 0xDB, 0x5B, 0x9B, 0x1B,
	0xEB, 0x6B, 0xAB, 0x2B, 0xCB, 0x4B, 0x8B, 0x0B,
	0xF3, 0x73, 0xB3, 0x33, 0xD3, 0x53, 0x93, 0x13,
	0xE3, 0x63, 0xA3, 0x23, 0xC3, 0x43, 0x83, 0x03,
	0xFD, 0x7D, 0xBD, 0x3D, 0xDD, 0x5D, 0x9D, 0x1D,
	0xED, 0x6D, 0xAD, 0x2D, 0xCD, 0x4D, 0x8D, 0x0D,
	0xF5, 0x75, 0xB5, 0x35, 0xD5, 0x55, 0x95, 0x15,
	0xE5, 0x65, 0xA5, 0x25, 0xC5, 0x45, 0x85, 0x05,
	0xF9, 0x79, 0xB9, 0x39, 0xD9, 0x59, 0x99, 0x19,
	0xE9, 0x69, 0xA9, 0x29, 0xC9, 0x49, 0x89, 0x09,
	0xF1, 0x71, 0xB1, 0x31, 0xD1, 0x51, 0x91, 0x11,
	0xE1, 0x61, 0xA1, 0x21, 0xC1, 0x41, 0x81, 0x01,
	0xFE, 0x7E, 0xBE, 0x3E, 0xDE, 0x5E, 0x9E, 0x1E,
	0xEE, 0x6E, 0xAE, 0x2E, 0xCE, 0x4E, 0x8E, 0x0E,
	0xF6, 0x76, 0xB6, 0x36, 0xD6, 0x56, 0x96, 0x16,
	0xE6, 0x66, 0xA6, 0x26, 0xC6, 0x46, 0x86, 0x06,
	0xFA, 0x7A, 0xBA, 0x3A, 0xDA, 0x5A, 0x9A, 0x1A,
	0xEA, 0x6A, 0xAA, 0x2A, 0xCA, 0x4A, 0x8A, 0x0A,
	0xF2, 0x72, 0xB2, 0x32, 0xD2, 0x52, 0x92, 0x12,
	0xE2, 0x62, 0xA2, 0x22, 0xC2, 0x42, 0x82, 0x02,
	0xFC, 0x7C, 0xBC, 0x3C, 0xDC, 0x5C, 0x9C, 0x1C,
	0xEC, 0x6C, 0xAC, 0x2C, 0xCC, 0x4C, 0x8C, 0x0C,
	0xF4, 0x74, 0xB4, 0x34, 0xD4, 0x54, 0x94, 0x14,
	0xE4, 0x64, 0xA4, 0x24, 0xC4, 0x44, 0x84, 0x04,
	0xF8, 0x78, 0xB8, 0x38, 0xD8, 0x58, 0x98, 0x18,
	0xE8, 0x68, 0xA8, 0x28, 0xC8, 0x48, 0x88, 0x08,
	0xF0, 0x70, 0xB0, 0x30, 0xD0, 0x50, 0x90, 0x10,
	0xE0, 0x60, 0xA0, 0x20, 0xC0, 0x40, 0x80, 0x00
};
#endif /*SW_INVERT */

#define dump(b, l) do { \
	int i; \
	pr_dbg("dump: "); \
	for (i = 0; i < (l); i++) \
		pr_dbg("%02x ", *(((unsigned char *)(b)) + i)); \
		pr_dbg("\n"); \
} while (0)

static int _gpio_out(struct gpio_desc *gpio, int val, const char *owner);
static int smc_default_init(struct smc_dev *smc);
static int smc_hw_set_param(struct smc_dev *smc);
static int smc_hw_reset(struct smc_dev *smc);
static int smc_hw_hot_reset(struct smc_dev *smc);
static int smc_hw_active(struct smc_dev *smc);
static int smc_hw_deactive(struct smc_dev *smc);
static int smc_hw_get_status(struct smc_dev *smc, int *sret);
static void smc_clk_enable(int enable);

static ssize_t gpio_pull_show(struct class *class,
			      struct class_attribute *attr, char *buf)
{
	if (ENA_GPIO_PULL > 0)
		return sprintf(buf, "%s\n", "enable GPIO pull low");
	else
		return sprintf(buf, "%s\n", "disable GPIO pull low");
}

static void *p_smc_hw_base;

static void sc2_write_smc(unsigned int reg, unsigned int val)
{
	void *ptr = (void *)(p_smc_hw_base + reg);

	writel(val, ptr);
//	pr_dbg("write addr:%lx, org v:0x%0x, ret v:0x%0x\n",
//	       (unsigned long)ptr, val, readl(ptr));
}

static int sc2_read_smc(unsigned int reg)
{
	void *addr = p_smc_hw_base + reg;
	int ret = 0;

	ret = readl(addr);
//	pr_dbg("read addr:%lx, value:0x%0x\n", (unsigned long)addr, ret);
	return ret;
}

static void sc2_write_sys(unsigned int reg, unsigned int val)
{
	void *ptr = (void *)(p_smc_hw_base - 0x38000 + reg);

	if (t5w_smartcard)
		return;

	writel(val, ptr);
//	pr_dbg("write addr:%lx, org v:0x%0x, ret v:0x%0x\n",
//	       (unsigned long)ptr, val, readl(ptr));
}

static int sc2_read_sys(unsigned int reg)
{
	void *addr = p_smc_hw_base - 0x38000 + reg;
	int ret = 0;

	if (t5w_smartcard)
		return -1;

	ret = readl(addr);
//	pr_dbg("read addr:%lx, value:0x%0x\n", (unsigned long)addr, ret);
	return ret;
}

static int sc2_smc_addr(struct platform_device *pdev)
{
	struct resource *res;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		pr_error("%s fail\n", __func__);
		return -1;
	}

	p_smc_hw_base = devm_ioremap_nocache(&pdev->dev, res->start,
					     resource_size(res));
	if (!p_smc_hw_base) {
		pr_error("%s base addr error\n", __func__);
		return -1;
	}

	pr_error("%s addr:0x%x\n", __func__, (unsigned int)res->start);

	if (res->start == 0xfe000000) {
		p_smc_hw_base += 0x38000;
		pr_error("sc2 smartcard\n");
	} else {
		pr_error("t5w smartcard\n");
		t5w_smartcard = 1;
	}
	return 0;
}

#define WRITE_CBUS_REG(_r, _v)   sc2_write_smc((_r), _v)
#define READ_CBUS_REG(_r)        sc2_read_smc((_r))

#define SMC_READ_REG(a)          READ_CBUS_REG(SMARTCARD_##a)
#define SMC_WRITE_REG(a, b)      WRITE_CBUS_REG(SMARTCARD_##a, b)

static ssize_t gpio_pull_store(struct class *class,
			     struct class_attribute *attr,
			     const char *buf, size_t count)
{
	int dbg;

	if (kstrtoint(buf, 0, &dbg))
		return -EINVAL;

	ENA_GPIO_PULL = dbg;
	return count;
}

static ssize_t ctrl_5v3v_show(struct class *class,
			      struct class_attribute *attr, char *buf)
{
	struct smc_dev *smc = NULL;
	int enable_5v3v = 0;

	mutex_lock(&smc_lock);
	smc = &smc_dev[0];
	enable_5v3v = smc->enable_5v3v_level;
	mutex_unlock(&smc_lock);

	return sprintf(buf, "5v3v_pin level = %d\n", enable_5v3v);
}

static ssize_t ctrl_5v3v_store(struct class *class,
			       struct class_attribute *attr,
			       const char *buf, size_t count)
{
	unsigned int enable_5v3v = 0;
	struct smc_dev *smc = NULL;

	if (kstrtouint(buf, 0, &enable_5v3v))
		return -EINVAL;

	mutex_lock(&smc_lock);
	smc = &smc_dev[0];
	smc->enable_5v3v_level = enable_5v3v;

	if (smc->enable_5v3v_pin) {
		_gpio_out(smc->enable_5v3v_pin,
			  smc->enable_5v3v_level, SMC_ENABLE_5V3V_PIN_NAME);
		pr_error("enable_pin: -->(%d)\n",
			 (smc->enable_5v3v_level) ? 1 : 0);
	}
	mutex_unlock(&smc_lock);

	return count;
}

static ssize_t debug_reset_store(struct class *class,
				 struct class_attribute *attr,
				 const char *buf, size_t count)
{
	unsigned int enable_5v3v = 0;
	struct smc_dev *smc = NULL;
	int sret;

	smc = &smc_dev[0];

	if (kstrtouint(buf, 0, &enable_5v3v))
		return -EINVAL;

	smc_hw_get_status(smc, &sret);
	smc_hw_reset(smc);

	return count;
}

static ssize_t enable_reset_store(struct class *class,
				  struct class_attribute *attr,
				  const char *buf, size_t count)
{
	unsigned int enable_reset = 0;
	struct smc_dev *smc = NULL;

	smc = &smc_dev[0];

	if (kstrtouint(buf, 0, &enable_reset))
		return -EINVAL;

	if (enable_reset)
		_gpio_out(smc->reset_pin, RESET_ENABLE, SMC_RESET_PIN_NAME);
	else
		_gpio_out(smc->reset_pin, RESET_DISABLE, SMC_RESET_PIN_NAME);

	return count;
}

static ssize_t enable_cmd_store(struct class *class,
				struct class_attribute *attr,
				const char *buf, size_t count)
{
	unsigned int enable_cmd = 0;
	struct smc_dev *smc = NULL;

	smc = &smc_dev[0];

	if (kstrtouint(buf, 0, &enable_cmd))
		return -EINVAL;

	if (enable_cmd)
		_gpio_out(smc->enable_pin,
			  smc->enable_level, SMC_ENABLE_PIN_NAME);
	else
		_gpio_out(smc->enable_pin,
			  !smc->enable_level, SMC_ENABLE_PIN_NAME);

	return count;
}

static ssize_t enable_clk_store(struct class *class,
				struct class_attribute *attr,
				const char *buf, size_t count)
{
	unsigned int enable_clk = 0;
	struct smc_dev *smc = NULL;

	smc = &smc_dev[0];

	if (kstrtouint(buf, 0, &enable_clk))
		return -EINVAL;

	if (enable_clk)
		smc_clk_enable(1);
	else
		smc_clk_enable(0);

	return count;
}

static ssize_t freq_show(struct class *class,
			 struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%dKHz\n", smc_dev[0].param.freq);
}

static ssize_t freq_store(struct class *class,
			  struct class_attribute *attr,
			  const char *buf, size_t count)
{
	int freq = 0;

	if (kstrtoint(buf, 0, &freq))
		return -EINVAL;

	if (freq)
		smc_dev[0].param.freq = freq;

	pr_dbg("freq -> %dKHz\n", smc_dev[0].param.freq);
	return count;
}

static ssize_t div_smc_show(struct class *class,
			    struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "div -> %d\n", DIV_SMC);
}

static ssize_t div_smc_store(struct class *class,
			     struct class_attribute *attr,
			     const char *buf, size_t count)
{
	int div = 0;

	if (kstrtoint(buf, 0, &div))
		return -EINVAL;

	if (div)
		DIV_SMC = div;

	pr_dbg("div -> %d\n", DIV_SMC);
	return count;
}

#ifdef MEM_DEBUG
static ssize_t debug_show(struct class *class,
			  struct class_attribute *attr, char *buf)
{
	pr_inf("Usage:\n");
	pr_inf("\techo [ 1 | 2 | 0 | dump | reset ] >");
	pr_inf("debug : enable(1/2)|disable|dump|reset\n");
	pr_inf("\t dump file: " DEBUG_FILE_NAME "\n");
	return 0;
}

static ssize_t debug_store(struct class *class,
			   struct class_attribute *attr,
			   const char *buf, size_t count)
{
	int smc_debug_level = 0;

	switch (buf[0]) {
	case '2':
	case '1':{
			if (dbuf) {
				if (buf[0] == '2')
					smc_debug = 2;
				else if (buf[0] == '1')
					smc_debug = 1;
				memset(dbuf, 0, DBUF_SIZE);
				dwrite = 0;
				dread = 0;
				pr_inf("dbuf cleanup\n");
				break;
			}
			{
			void *p = kmalloc(DBUF_SIZE, GFP_KERNEL);

			smc_debug_level++;
			if (buf[0] == '2')
				smc_debug_level++;

			if (p) {
				dbuf = (char *)p;
				smc_debug = smc_debug_level;
				memset(dbuf, 0, DBUF_SIZE);
				dwrite = 0;
				dread = 0;
				pr_inf("dbuf cleanup\n");
			} else {
				pr_error("krealloc(dbuf:%d) failed\n",
					 DBUF_SIZE);
			}
			break;
			}
		}
	case '0':
		smc_debug = 0;
		kfree(dbuf);
		dbuf = NULL;
		break;
	case 'r':
	case 'R':
		if (smc_debug) {
			memset(dbuf, 0, DBUF_SIZE);
			dwrite = 0;
			dread = 0;
			pr_inf("dbuf cleanup\n");
		}
		break;
	case 'd':
	case 'D':
		if (smc_debug || smc_debug_save) {
			open_debug();
			debug_write(dbuf, DBUF_SIZE);
			close_debug();
			pr_inf("dbuf dump ok\n");
		}
		break;
	default:
		break;
	}
	return count;
}
#endif

/*0: I/O
 *1: GPIO as H
 *2: GPIO as L
 */
static int c7_pins_mode;
static int c4_pins_mode = -1;
static int c8_pins_mode = -1;

static ssize_t pins_mode_show(struct class *class, struct class_attribute *attr,
			      char *buf)
{
	int r, total = 0;

	if (c4_pins_mode == -1 || c8_pins_mode == -1) {
		pr_inf("please set C4/C8 first\n");
		return 0;
	}

	r = sprintf(buf, "C4:%c ",
		    (c4_pins_mode) ? ((c4_pins_mode == 1) ? 'H' : 'L') : '0');
	buf += r;
	total += r;

	r = sprintf(buf, "C7:%c ",
		    (c7_pins_mode) ? ((c7_pins_mode == 1) ? 'H' : 'L') : '0');
	buf += r;
	total += r;

	r = sprintf(buf, "C8:%c\n",
		    (c8_pins_mode) ? ((c8_pins_mode == 1) ? 'H' : 'L') : '0');
	buf += r;
	total += r;

	return total;
}

static int pins_set(struct pinctrl *pinctrl, char *name)
{
	struct pinctrl_state *s;
	int ret = 0;

	s = pinctrl_lookup_state(pinctrl, name);
	if (IS_ERR(s)) {
		pr_info("can't get %s\n", name);
		return -1;
	}
	ret = pinctrl_select_state(pinctrl, s);
	if (ret < 0) {
		pr_info("can't select %s\n", name);
		return -1;
	}
	pr_inf("pin mode set %s\n", name);
	return 0;
}

static int is_same_pin_mode(const char *buf)
{
	pr_inf("input:%s\n", buf);

	if (buf[1] == '7') {
		if (buf[3] == '0' && c7_pins_mode == 0)
			return 1;
		else if (buf[3] == 'H' && c7_pins_mode == 1)
			return 1;
		else if (buf[3] == 'L' && c7_pins_mode == 2)
			return 1;
	}

	if (buf[1] == '4') {
		if (buf[3] == '0' && c4_pins_mode == 0)
			return 1;
		else if (buf[3] == 'H' && c4_pins_mode == 1)
			return 1;
		else if (buf[3] == 'L' && c4_pins_mode == 2)
			return 1;
	}

	if (buf[1] == '8') {
		if (buf[3] == '0' && c8_pins_mode == 0)
			return 1;
		else if (buf[3] == 'H' && c8_pins_mode == 1)
			return 1;
		else if (buf[3] == 'L' && c8_pins_mode == 2)
			return 1;
	}
	return 0;
}

static int save_pin_mode(const char *buf)
{
	if (buf[1] == '7') {
		if (buf[3] == '0')
			c7_pins_mode = 0;
		else if (buf[3] == 'H')
			c7_pins_mode = 1;
		else if (buf[3] == 'L')
			c7_pins_mode = 2;
	}

	if (buf[1] == '4') {
		if (buf[3] == '0')
			c4_pins_mode = 0;
		else if (buf[3] == 'H')
			c4_pins_mode = 1;
		else if (buf[3] == 'L')
			c4_pins_mode = 2;
	}

	if (buf[1] == '8') {
		if (buf[3] == '0')
			c8_pins_mode = 0;
		else if (buf[3] == 'H')
			c8_pins_mode = 1;
		else if (buf[3] == 'L')
			c8_pins_mode = 2;
	}
	pr_inf("c4:%d, c7:%d, c8:%d\n", c4_pins_mode, c7_pins_mode,
	       c8_pins_mode);

	return 0;
}

static ssize_t pins_mode_store(struct class *class,
			       struct class_attribute *attr, const char *buf,
			       size_t count)
{
	char name[32];
	int valid = 0;
	struct smc_dev *smc = NULL;

	if (count < 4 || buf[0] != 'C') {
		pr_inf("error param %s ", buf);
		pr_info("it should CX:0/H/L, X:4/7/8\n");
		return count;
	}

	if (is_same_pin_mode(buf)) {
		pr_info("same pin mode pin mode %s\n", buf);
		return count;
	}

	if (buf[1] == '4' || buf[1] == '7' || buf[1] == '8') {
		memset(name, 0, sizeof(name));
		if (buf[3] == '0') {
			if (buf[1] == '7')
				sprintf(name, "pins-mode0");
			else if (buf[1] == '4')
				sprintf(name, "pins-mode1");
			else if (buf[1] == '8')
				sprintf(name, "pins-mode2");
			valid = 1;
		} else if (buf[3] == 'H') {
			if (buf[1] == '7')
				sprintf(name, "data-m0-h");
			else if (buf[1] == '4')
				sprintf(name, "data-m1-h");
			else if (buf[1] == '8')
				sprintf(name, "data-m2-h");
			valid = 1;
		} else if (buf[3] == 'L') {
			if (buf[1] == '7')
				sprintf(name, "data-m0-l");
			else if (buf[1] == '4')
				sprintf(name, "data-m1-l");
			else if (buf[1] == '8')
				sprintf(name, "data-m2-l");
			valid = 1;
		}
	}
	if (valid) {
		mutex_lock(&smc_lock);
		smc = &smc_dev[0];
		if (smc->pinctrl && pins_set(smc->pinctrl, name) == 0)
			save_pin_mode(buf);
		mutex_unlock(&smc_lock);
	}
	return count;
}

static CLASS_ATTR_RW(gpio_pull);
static CLASS_ATTR_RW(ctrl_5v3v);
static CLASS_ATTR_RW(freq);
static CLASS_ATTR_RW(div_smc);
static CLASS_ATTR_WO(debug_reset);
static CLASS_ATTR_WO(enable_reset);
static CLASS_ATTR_WO(enable_cmd);
static CLASS_ATTR_WO(enable_clk);
static CLASS_ATTR_RW(pins_mode);
#ifdef MEM_DEBUG
static CLASS_ATTR_RW(debug);
#endif

static struct attribute *smc_class_attrs[] = {
	&class_attr_gpio_pull.attr,
	&class_attr_ctrl_5v3v.attr,
	&class_attr_freq.attr,
	&class_attr_div_smc.attr,
	&class_attr_debug_reset.attr,
	&class_attr_enable_reset.attr,
	&class_attr_enable_cmd.attr,
	&class_attr_enable_clk.attr,
	&class_attr_pins_mode.attr,

#ifdef MEM_DEBUG
	&class_attr_debug.attr,
#endif
	NULL,
};

ATTRIBUTE_GROUPS(smc_class);

static struct class smc_class = {
	.name = SMC_CLASS_NAME,
	.class_groups = smc_class_groups,
};

#ifndef CONFIG_OF
static int _gpio_request(unsigned int gpio, const char *owner)
{
	gpio_request(gpio, owner);
	return 0;
}
#endif

static int _gpio_out(struct gpio_desc *gpio, int val, const char *owner)
{
	int ret = 0;

	if (val < 0)
		return -1;

	if (val != 0)
		val = 1;
	ret = gpiod_direction_output(gpio, val);
	return ret;
}

#ifdef DET_FROM_PIO
static int _gpio_in(struct gpio_desc *gpio, const char *owner)
{
	int ret = 0;

	ret = gpiod_get_value(gpio);
	return ret;
}
#endif
static int _gpio_free(struct gpio_desc *gpio, const char *owner)
{
	gpiod_put(gpio);
	return 0;
}

static inline int smc_write_end(struct smc_dev *smc)
{
	int ret;
	unsigned long flags;

	spin_lock_irqsave(&smc->slock, flags);
	ret = (!smc->cardin || !smc->send_count);
	spin_unlock_irqrestore(&smc->slock, flags);

	return ret;
}

static inline int smc_can_read(struct smc_dev *smc)
{
	int ret;
	unsigned long flags;

	spin_lock_irqsave(&smc->slock, flags);
	ret = (!smc->cardin || smc->recv_count);
	spin_unlock_irqrestore(&smc->slock, flags);

	return ret;
}

static inline int smc_can_write(struct smc_dev *smc)
{
	int ret;
	unsigned long flags;

	spin_lock_irqsave(&smc->slock, flags);
	ret = (!smc->cardin || (smc->send_count != SEND_BUF_SIZE));
	spin_unlock_irqrestore(&smc->slock, flags);
	return ret;
}

static void smc_mp0_clk_set(int clk)
{
	unsigned int data;

#define SMARTCARD_CLK_CTRL (0x26 * 4)

	data = sc2_read_sys(SMARTCARD_CLK_CTRL);

	data &= 0xFFFFF900;
	if (clk == 4500)
		data |= 0x224;
	else if (clk == 6750)
		data |= 0x049;
	else if (clk == 13500)
		data |= 0x024;
	else
		data |= 500000 / clk - 1;

	sc2_write_sys(SMARTCARD_CLK_CTRL, data);
	pr_error("%s data:0x%0x\n", __func__, data);
}

static void smc_clk_enable(int enable)
{
	unsigned int data;

#define SMARTCARD_CLK_CTRL (0x26 * 4)

	if (t5w_smartcard)
		return;

	data = sc2_read_sys(SMARTCARD_CLK_CTRL);
	if (enable)
		data |= (1 << 8);
	else
		data &= ~(1 << 8);
	sc2_write_sys(SMARTCARD_CLK_CTRL, data);
}

static int smc_hw_set_param(struct smc_dev *smc)
{
	unsigned long v = 0;
	struct smccard_hw_reg0 *reg0;
	struct smccard_hw_reg6 *reg6;
	struct smccard_hw_reg2 *reg2;
	struct smccard_hw_reg5 *reg5;
	unsigned long freq_cpu = 0;

	pr_error("hw set param\n");
	if (smc->param.freq == 0 || smc->param.d == 0) {
		pr_error("hw set param, freq or d = 0 invalid\n");
		return 0;
	}

	if (smartcard_mpll0) {
		smc_mp0_clk_set(smc->param.freq);
	} else if (t5w_smartcard) {
//		freq_cpu = clk_get_rate(aml_smartcard_clk) / 1000 * DIV_SMC;
		switch (clock_source) {
		case 0:
			freq_cpu = 500000;
			break;
		case 1:
			freq_cpu = 666000;
			break;
		case 2:
			freq_cpu = 400000;
			break;
		case 3:
			freq_cpu = 24000;
			break;
		default:
			freq_cpu = 500000;
			break;
		}
	} else {
		clk_set_rate(aml_smartcard_clk, smc->param.freq * 1000);
	}

	v = SMC_READ_REG(REG0);
	reg0 = (struct smccard_hw_reg0 *)&v;
#if (ETU_CLK_SEL == 0)
	reg0->etu_divider = ETU_DIVIDER_CLOCK_HZ * smc->param.f /
	    (smc->param.d * smc->param.freq) - 1;
#else
	reg0->etu_divider = smc->param.f / smc->param.d - 1;
#endif
	SMC_WRITE_REG(REG0, v);
	pr_error("REG0: 0x%08lx\n", v);
	pr_error("f	  :%d\n", smc->param.f);
	pr_error("d	  :%d\n", smc->param.d);
	pr_error("freq	:%d\n", smc->param.freq);

	v = SMC_READ_REG(REG1);
	pr_error("REG1: 0x%08lx\n", v);

	v = SMC_READ_REG(REG2);
	reg2 = (struct smccard_hw_reg2 *)&v;
	reg2->recv_invert = smc->param.recv_invert;
	reg2->recv_parity = smc->param.recv_parity;
	reg2->recv_lsb_msb = smc->param.recv_lsb_msb;
	reg2->xmit_invert = smc->param.xmit_invert;
	reg2->xmit_parity = smc->param.xmit_parity;
	reg2->xmit_lsb_msb = smc->param.xmit_lsb_msb;
	reg2->xmit_retries = smc->param.xmit_retries;
	reg2->xmit_repeat_dis = smc->param.xmit_repeat_dis;
	reg2->recv_no_parity = smc->param.recv_no_parity;
	if (t5w_smartcard)
		reg2->clk_tcnt = freq_cpu / smc->param.freq - 1;
	reg2->det_filter_sel = DET_FILTER_SEL_DEFAULT;
	reg2->io_filter_sel = IO_FILTER_SEL_DEFAULT;
	if (t5w_smartcard)
		reg2->clk_sel = clock_source;
	/*reg2->pulse_irq = 0; */
	SMC_WRITE_REG(REG2, v);
	pr_error("REG2: 0x%08lx\n", v);
	pr_error("recv_inv:%d\n", smc->param.recv_invert);
	pr_error("recv_lsb:%d\n", smc->param.recv_lsb_msb);
	pr_error("recv_par:%d\n", smc->param.recv_parity);
	pr_error("recv_npa:%d\n", smc->param.recv_no_parity);
	pr_error("xmit_inv:%d\n", smc->param.xmit_invert);
	pr_error("xmit_lsb:%d\n", smc->param.xmit_lsb_msb);
	pr_error("xmit_par:%d\n", smc->param.xmit_parity);
	pr_error("xmit_rep:%d\n", smc->param.xmit_repeat_dis);
	pr_error("xmit_try:%d\n", smc->param.xmit_retries);
	if (t5w_smartcard)
		pr_error("clk_tcnt:%d freq_cpu:%ld\n", reg2->clk_tcnt, freq_cpu);

	v = SMC_READ_REG(REG5);
	reg5 = (struct smccard_hw_reg5 *)&v;
	reg5->cwt_detect_en = cwt_det_en;
	reg5->bwt_base_time_gnt = bwt_time;
	SMC_WRITE_REG(REG5, v);
	pr_error("REG5: 0x%08lx\n", v);

	v = SMC_READ_REG(REG6);
	reg6 = (struct smccard_hw_reg6 *)&v;
	reg6->cwi_value = smc->param.cwi;
	reg6->bwi = smc->param.bwi;
	reg6->bgt = smc->param.bgt - 2;
	reg6->N_parameter = smc->param.n;

	SMC_WRITE_REG(REG6, v);
	pr_error("REG6: 0x%08lx\n", v);
	pr_error("N	  :%d\n", smc->param.n);
	pr_error("cwi	 :%d\n", smc->param.cwi);
	pr_error("bgt	 :%d\n", smc->param.bgt);
	pr_error("bwi	 :%d\n", smc->param.bwi);

	return 0;
}

static int smc_default_init(struct smc_dev *smc)
{
	smc->param.f = F_DEFAULT;
	smc->param.d = D_DEFAULT;
	smc->param.freq = FREQ_DEFAULT;
	smc->param.recv_invert = 0;
	smc->param.recv_parity = 0;
	smc->param.recv_lsb_msb = 0;
	smc->param.recv_no_parity = 1;
	smc->param.xmit_invert = 0;
	smc->param.xmit_lsb_msb = 0;
	smc->param.xmit_retries = 1;
	smc->param.xmit_repeat_dis = 1;
	smc->param.xmit_parity = 0;

	/*set reg6 param value */
	smc->param.n = N_DEFAULT;
	smc->param.bwi = BWI_DEFAULT;
	smc->param.cwi = CWI_DEFAULT;
	smc->param.bgt = BGT_DEFAULT;
	return 0;
}

static int smc_hw_setup(struct smc_dev *smc, int clk_out)
{
	unsigned long v = 0;
	struct smccard_hw_reg0 *reg0;
	struct smc_answer_t0_rst *reg1;
	struct smccard_hw_reg2 *reg2;
	struct smc_interrupt_reg *reg_int;
	struct smccard_hw_reg5 *reg5;
	struct smccard_hw_reg6 *reg6;
	struct smccard_hw_reg8 *reg8;
	unsigned long freq_cpu = 0;

	if (t5w_smartcard) {
	//	freq_cpu = clk_get_rate(aml_smartcard_clk) / 1000 * DIV_SMC;
		switch (clock_source) {
		case 0:
			freq_cpu = 500000;
			break;
		case 1:
			freq_cpu = 666000;
			break;
		case 2:
			freq_cpu = 400000;
			break;
		case 3:
			freq_cpu = 24000;
			break;
		default:
			freq_cpu = 500000;
			break;
		}
		pr_error("SMC CLK SOURCE - %luKHz\n", freq_cpu);
	} else {
		if (!smartcard_mpll0)
			clk_set_rate(aml_smartcard_clk, smc->param.freq * 1000);
	}

#ifdef RST_FROM_PIO
	_gpio_out(smc->reset_pin, RESET_ENABLE, SMC_RESET_PIN_NAME);
#endif

	v = SMC_READ_REG(REG0);
	reg0 = (struct smccard_hw_reg0 *)&v;
	reg0->enable = 1;
	if (clk_out)
		reg0->clk_en = 1;
	else
		reg0->clk_en = 0;
	reg0->clk_oen = 0;
//      reg0->card_detect = 0;
	reg0->start_atr = 0;
//      reg0->start_atr_en = 0;
	reg0->rst_level = RESET_ENABLE;
//      reg0->io_level = 0;
//	reg0->recv_fifo_threshold = FIFO_THRESHOLD_DEFAULT;
#if (ETU_CLK_SEL == 0)
	reg0->etu_divider = ETU_DIVIDER_CLOCK_HZ * smc->param.f
	    / (smc->param.d * smc->param.freq) - 1;
#else
	reg0->etu_divider = smc->param.f / smc->param.d - 1;
#endif
	reg0->first_etu_offset = 5;
	SMC_WRITE_REG(REG0, v);
	smc_clk_enable(reg0->clk_en);

	if (smartcard_mpll0)
		smc_mp0_clk_set(smc->param.freq);

	pr_error("REG0: 0x%08lx\n", v);
	pr_error("f	  :%d\n", smc->param.f);
	pr_error("d	  :%d\n", smc->param.d);
	pr_error("freq	:%d\n", smc->param.freq);

	v = SMC_READ_REG(REG1);
	reg1 = (struct smc_answer_t0_rst *)&v;
	reg1->atr_final_tcnt = atr_final_tcnt;
	reg1->atr_holdoff_tcnt = ATR_HOLDOFF_TCNT_DEFAULT;
	reg1->atr_clk_mux = atr_clk_mux;
	reg1->atr_holdoff_en = atr_holdoff;	/*ATR_HOLDOFF_EN; */
	reg1->etu_clk_sel = ETU_CLK_SEL;
	reg1->atr_reset = atr_reset;
	SMC_WRITE_REG(REG1, v);
	pr_error("REG1: 0x%08lx\n", v);

	v = SMC_READ_REG(REG2);
	reg2 = (struct smccard_hw_reg2 *)&v;
	reg2->recv_invert = smc->param.recv_invert;
	reg2->recv_parity = smc->param.recv_parity;
	reg2->recv_lsb_msb = smc->param.recv_lsb_msb;
	reg2->xmit_invert = smc->param.xmit_invert;
	reg2->xmit_parity = smc->param.xmit_parity;
	reg2->xmit_lsb_msb = smc->param.xmit_lsb_msb;
	reg2->xmit_retries = smc->param.xmit_retries;
	reg2->xmit_repeat_dis = smc->param.xmit_repeat_dis;
	reg2->recv_no_parity = smc->param.recv_no_parity;
	if (t5w_smartcard)
		reg2->clk_tcnt = freq_cpu / smc->param.freq - 1;
	reg2->det_filter_sel = DET_FILTER_SEL_DEFAULT;
	reg2->io_filter_sel = IO_FILTER_SEL_DEFAULT;
	if (t5w_smartcard)
		reg2->clk_sel = clock_source;
	/*reg2->pulse_irq = 0; */
	SMC_WRITE_REG(REG2, v);
	pr_error("REG2: 0x%08lx\n", v);
	pr_error("recv_inv:%d\n", smc->param.recv_invert);
	pr_error("recv_lsb:%d\n", smc->param.recv_lsb_msb);
	pr_error("recv_par:%d\n", smc->param.recv_parity);
	pr_error("recv_npa:%d\n", smc->param.recv_no_parity);
	pr_error("xmit_inv:%d\n", smc->param.xmit_invert);
	pr_error("xmit_lsb:%d\n", smc->param.xmit_lsb_msb);
	pr_error("xmit_par:%d\n", smc->param.xmit_parity);
	pr_error("xmit_rep:%d\n", smc->param.xmit_repeat_dis);
	pr_error("xmit_try:%d\n", smc->param.xmit_retries);
	if (t5w_smartcard)
		pr_error("clk_tcnt:%d freq_cpu:%ld\n", reg2->clk_tcnt, freq_cpu);

	v = SMC_READ_REG(INTR);
	reg_int = (struct smc_interrupt_reg *)&v;
	reg_int->recv_fifo_bytes_threshold_int_mask = 0;
	reg_int->send_fifo_last_byte_int_mask = 1;
	reg_int->cwt_expired_int_mask = 1;
	reg_int->bwt_expired_int_mask = 1;
	reg_int->write_full_fifo_int_mask = 1;
	reg_int->send_and_recv_conflict_int_mask = 1;
	reg_int->recv_error_int_mask = 1;
	reg_int->send_error_int_mask = 1;
	reg_int->rst_expired_int_mask = 1;
	reg_int->card_detect_int_mask = 0;
	SMC_WRITE_REG(INTR, v | 0x03FF);
	pr_error("INTR: 0x%08lx\n", v);

	v = SMC_READ_REG(REG5);
	reg5 = (struct smccard_hw_reg5 *)&v;
	reg5->cwt_detect_en = cwt_det_en;
	reg5->btw_detect_en = btw_det_en;
	reg5->etu_msr_en = etu_msr_en;
	reg5->bwt_base_time_gnt = bwt_time;
	SMC_WRITE_REG(REG5, v);
	pr_error("REG5: 0x%08lx\n", v);

	v = SMC_READ_REG(REG6);
	reg6 = (struct smccard_hw_reg6 *)&v;
	reg6->N_parameter = smc->param.n;
	reg6->cwi_value = smc->param.cwi;
	reg6->bgt = smc->param.bgt - 2;
	reg6->bwi = smc->param.bwi;

	SMC_WRITE_REG(REG6, v);
	pr_error("REG6: 0x%08lx\n", v);
	pr_error("N	  :%d\n", smc->param.n);
	pr_error("cwi	 :%d\n", smc->param.cwi);
	pr_error("bgt	 :%d\n", smc->param.bgt);
	pr_error("bwi	 :%d\n", smc->param.bwi);

	/*use new recv_fifo_count(8bits) to void overflow(4bits) issue*/
	v = SMC_READ_REG(REG8);
	reg8 = (struct smccard_hw_reg8 *)&v;
	reg8->lrg_fifo_recv_thr = FIFO_THRESHOLD_DEFAULT;
	reg8->use_lrg_fifo_recv = 1;
	SMC_WRITE_REG(REG8, v);

	return 0;
}

static void enable_smc_clk(struct smc_dev *smc)
{
	unsigned int _value;

	if ((smc->pin_clk_pinmux_reg == -1) ||
		(smc->pin_clk_pinmux_bit == -1))
		return;
	_value = READ_CBUS_REG(smc->pin_clk_pinmux_reg);
	_value |= smc->pin_clk_pinmux_bit;
	WRITE_CBUS_REG(smc->pin_clk_pinmux_reg, _value);
}

static void disable_smc_clk(struct smc_dev *smc)
{
	unsigned int _value;

	if ((smc->pin_clk_pinmux_reg == -1) ||
		(smc->pin_clk_pinmux_bit == -1))
		return;

	_value = READ_CBUS_REG(smc->pin_clk_pinmux_reg);
	_value &= ~smc->pin_clk_pinmux_bit;
	WRITE_CBUS_REG(smc->pin_clk_pinmux_reg, _value);
	_value = READ_CBUS_REG(smc->pin_clk_pinmux_reg);

	/*pr_dbg("disable smc_clk: mux[%x]\n", _value); */

	if ((smc->pin_clk_oen_reg != -1) &&
		(smc->pin_clk_out_reg != -1) &&
		(smc->pin_clk_oebit != -1)   &&
		(smc->pin_clk_oubit != -1)) {
		/*force the clk pin to low. */
		_value = READ_CBUS_REG(smc->pin_clk_oen_reg);
		_value &= ~smc->pin_clk_oebit;
		WRITE_CBUS_REG(smc->pin_clk_oen_reg, _value);
		_value = READ_CBUS_REG(smc->pin_clk_out_reg);
		_value &= ~smc->pin_clk_oubit;
		WRITE_CBUS_REG(smc->pin_clk_out_reg, _value);
		pr_dbg("disable smc_clk: pin[%x](reg)\n", _value);
	} else if (smc->pin_clk_pin) {
		//usleep_range(20, 100);
		/*      _gpio_out(smc->pin_clk_pin,
		 *      0,
		 *      SMC_CLK_PIN_NAME);
		 */
		//usleep_range(1000, 1200);
	} else {
		pr_error("no reg/bit or pin");
	}
}

static int smc_hw_active(struct smc_dev *smc)
{
	if (ENA_GPIO_PULL > 0) {
		enable_smc_clk(smc);
		usleep_range(200, 400);
	}
	if (!smc->active) {
		if (smc->reset) {
			smc->reset(NULL, 0);
			pr_dbg("call reset(0) in bsp.\n");
		} else {
			if (smc->use_enable_pin) {
				_gpio_out(smc->enable_pin,
					  smc->enable_level,
					  SMC_ENABLE_PIN_NAME);
			}
		}

		usleep_range(200, 300);
		//add clk output for synamedia's requirement
		smc_hw_setup(smc, 1);
		smc->active = 1;
	}

	return 0;
}

static int smc_hw_deactive(struct smc_dev *smc)
{
	//for loop print, it can stop by strigger
//	if (smc_debug) {
//		open_debug();
//		debug_write(dbuf, DBUF_SIZE);
//		close_debug();
//		smc_debug = 0;
//		smc_debug_save = 1;
//		pr_inf("dbuf dump ok\n");
//	}

	if (smc->active) {
		unsigned long sc_reg0 = SMC_READ_REG(REG0);
		struct smccard_hw_reg0 *sc_reg0_reg = (void *)&sc_reg0;

#ifdef RST_FROM_PIO
		_gpio_out(smc->reset_pin, RESET_ENABLE, SMC_RESET_PIN_NAME);
		//_gpio_out(smc->reset_pin, RESET_DISABLE, SMC_RESET_PIN_NAME);
#endif
		usleep_range(100, 150);

		sc_reg0_reg->rst_level = RESET_ENABLE;
		sc_reg0_reg->enable = 1;
		sc_reg0_reg->start_atr = 0;
//		sc_reg0_reg->start_atr_en = 0;
		sc_reg0_reg->clk_en = 0;
		SMC_WRITE_REG(REG0, sc_reg0);
		smc_clk_enable(sc_reg0_reg->clk_en);

		if (smc->reset) {
			smc->reset(NULL, 1);
			pr_dbg("call reset(1) in bsp.\n");
		} else {
			if (smc->use_enable_pin)
				_gpio_out(smc->enable_pin, !smc->enable_level,
					  SMC_ENABLE_PIN_NAME);
		}
		if (ENA_GPIO_PULL > 0) {
			disable_smc_clk(smc);
			/*smc_pull_down_data(); */
		}

		smc->active = 0;
	}

	return 0;
}

//#define INV(a) ((smc->sc_type == SC_INVERSE) ? inv_table[(int)(a)] : (a))

#ifndef ATR_FROM_INT
static int smc_hw_get(struct smc_dev *smc, int cnt, int timeout)
{
	unsigned int sc_status;
	unsigned int rev_status;
	s64 char_nsec = (s64)timeout * 1000;
	s64 prev_time_nsec;
	int times = timeout / 10;
	struct smc_status_reg *sc_status_reg =
	    (struct smc_status_reg *)&sc_status;
	struct smccard_hw_reg8 *rev_status_reg =
	    (struct smccard_hw_reg8 *)&rev_status;

	prev_time_nsec = ktime_to_ns(ktime_get());

	while ((times > 0) && (cnt > 0)) {
		sc_status = SMC_READ_REG(STATUS);
		rev_status = SMC_READ_REG(REG8);

//		if (times % 1000 == 0)
//			pr_dbg("read atr status %08x\n", sc_status);

		if (sc_status_reg->rst_expired_status) {
			pr_error("atr timeout\n");
			return -1;
		}

		if (sc_status_reg->cwt_expired_status) {
			pr_error("cwt timeout when get atr,");
			pr_error("but maybe it is natural!\n");
		}

		if (sc_status_reg->recv_fifo_empty) {
			usleep_range(10, 20);
			times--;
		} else {
			while (rev_status_reg->recv_fifo_count > 0) {
				u8 byte = (SMC_READ_REG(FIFO)) & 0xff;

#ifdef SW_INVERT
				if (smc->sc_type == SC_INVERSE)
					byte = inv_table[byte];
#endif

				smc->atr.atr[smc->atr.atr_len++] = byte;
				rev_status_reg->recv_fifo_count--;
				cnt--;
				if (cnt == 0) {
					pr_dbg("read atr bytes ok\n");
					return 0;
				}
			}
		}
		if ((ktime_to_ns(ktime_get()) - prev_time_nsec) > char_nsec)
			break;
	}

	pr_error("read atr failed cnt:%d\n", cnt);
	return -1;
}

#else

static int smc_fiq_get(struct smc_dev *smc, int size, int timeout)
{
	int ret = 0;
	int times = timeout / 10;
	int start, end;

	if (!times)
		times = 1;

	while ((times > 0) && (size > 0)) {
		start = smc->recv_start;
		end = smc->recv_end;	/*momentary value */

		if (!smc->cardin) {
			ret = -ENODEV;
		} else if (start == end) {
			ret = -EAGAIN;
		} else {
			int i;
			/*ATR only, no loop */
			ret = end - start;
			if (ret > size)
				ret = size;
			memcpy(&smc->atr.atr[smc->atr.atr_len],
			       &smc->recv_buf[start], ret);
			for (i = smc->atr.atr_len;
			     i < smc->atr.atr_len + ret; i++) {
				if (smc->sc_type == SC_INVERSE)
					smc->atr.atr[i] =
					inv_table[smc->atr.atr[i]];
				else
					smc->atr.atr[i] = smc->atr.atr[i];
			}
			smc->atr.atr_len += ret;

			smc->recv_start += ret;
			size - = ret;
		}

		if (ret < 0) {
			msleep(20);
			times--;
		}
	}

	if (size > 0)
		ret = -ETIMEDOUT;

	return ret;
}
#endif /*ifndef ATR_FROM_INT */

static int smc_hw_read_atr(struct smc_dev *smc)
{
	char *ptr = smc->atr.atr;
	int his_len, t, tnext = 0, only_t0 = 1, loop_cnt = 0;
	int i;

	pr_dbg("read atr\n");
#ifdef ATR_FROM_INT
#define smc_hw_get smc_fiq_get
#endif

	smc->atr.atr_len = 0;
	//from 2000 to 100 for reducing to get
	//ATR time
	if (atr_timeout == 200000) {
		if (smc_hw_get(smc, 2, atr_timeout) < 0)
			goto end;
	} else {
		if (smc_hw_get(smc, 1, atr_timeout) < 0)
			goto end;

		if (smc_hw_get(smc, 1, atr_timeout) < 0)
			goto end;
	}

#ifdef SW_INVERT
	if (ptr[0] == 0x03) {
		smc->sc_type = SC_INVERSE;
		ptr[0] = inv_table[(int)ptr[0]];
		if (smc->atr.atr_len > 1)
			ptr[1] = inv_table[(int)ptr[1]];
	} else if (ptr[0] == 0x3F) {
		smc->sc_type = SC_INVERSE;
		if (smc->atr.atr_len > 1)
			ptr[1] = inv_table[(int)ptr[1]];
	} else if (ptr[0] == 0x3B) {
		smc->sc_type = SC_DIRECT;
	} else if (ptr[0] == 0x23) {
		smc->sc_type = SC_DIRECT;
		ptr[0] = inv_table[(int)ptr[0]];
		if (smc->atr.atr_len > 1)
			ptr[1] = inv_table[(int)ptr[1]];
	}
#endif /*SW_INVERT */

	ptr++;
	his_len = ptr[0] & 0x0F;

	do {
		tnext = 0;
		loop_cnt++;
		if (ptr[0] & 0x10) {
			if (smc_hw_get(smc, 1, atr1_timeout) < 0)
				goto end;
		}
		if (ptr[0] & 0x20) {
			if (smc_hw_get(smc, 1, atr1_timeout) < 0)
				goto end;
		}
		if (ptr[0] & 0x40) {
			if (smc_hw_get(smc, 1, atr1_timeout) < 0)
				goto end;
		}
		if (ptr[0] & 0x80) {
			if (smc_hw_get(smc, 1, atr1_timeout) < 0)
				goto end;

			ptr = &smc->atr.atr[smc->atr.atr_len - 1];
			t = ptr[0] & 0x0F;
			if (t)
				only_t0 = 0;
			if (ptr[0] & 0xF0)
				tnext = 1;
		}
	} while (tnext && loop_cnt < 4);

	if (!only_t0)
		his_len++;
	for (i = 0; i < his_len; i++)
		if (smc_hw_get(smc, 1, atr1_timeout) < 0)
			goto end;

	pr_dbg("get atr len:%d data: ", smc->atr.atr_len);
	for (i = 0; i < smc->atr.atr_len; i++)
		pr_dbg("%02x ", smc->atr.atr[i]);
	pr_dbg("\n");

#ifdef ATR_OUT_READ
	if (smc->atr.atr_len) {
		pr_dbg("reset recv_start %d->0\n", smc->recv_start);
		smc->recv_start = 0;
		if (smc->sc_type == SC_INVERSE) {
			int i;

			for (i = 0; i < smc->atr.atr_len; i++)
				smc->recv_buf[smc->recv_start + i] =
				    smc->atr.atr[i];
		}
	}
#endif
	return 0;

end:
	pr_error("read atr failed\n");
	return -EIO;
#ifdef ATR_FROM_INT
#undef smc_hw_get
#endif
}

static void smc_reset_prepare(struct smc_dev *smc)
{
	/*reset recv&send buf */
	smc->send_start = 0;
	smc->send_count = 0;
	smc->recv_start = 0;
	smc->recv_count = 0;

	/*Read ATR */
	smc->atr.atr_len = 0;
	smc->recv_count = 0;
	smc->send_count = 0;

	smc->recv_end = 0;
	smc->send_end = 0;

#ifdef SW_INVERT
	smc->sc_type = SC_DIRECT;
	smc->atr_mode = 1;
#endif
}

static int smc_hw_reset(struct smc_dev *smc)
{
	unsigned long flags;
	int ret;
	unsigned long sc_reg0 = SMC_READ_REG(REG0);
	struct smccard_hw_reg0 *sc_reg0_reg = (void *)&sc_reg0;
	unsigned long sc_int;
	struct smc_interrupt_reg *sc_int_reg = (void *)&sc_int;

	pr_dbg("smc read reg0 0x%lx\n", sc_reg0);

	spin_lock_irqsave(&smc->slock, flags);
	if (smc->cardin)
		ret = 0;
	else
		ret = -ENODEV;
	spin_unlock_irqrestore(&smc->slock, flags);

	if (ret >= 0) {
		/*Reset */
#ifdef NO_HOT_RESET
		smc->active = 0;
#endif
		if (smc->active) {
			smc_reset_prepare(smc);

			sc_reg0_reg->rst_level = RESET_ENABLE;
			sc_reg0_reg->clk_en = 1;
#if (ETU_CLK_SEL == 0)
			sc_reg0_reg->etu_divider = ETU_DIVIDER_CLOCK_HZ *
			    smc->param.f / (smc->param.d * smc->param.freq) - 1;
#else
			sc_reg0_reg->etu_divider = smc->param.f / smc->param.d - 1;
#endif
			SMC_WRITE_REG(REG0, sc_reg0);
			smc_clk_enable(sc_reg0_reg->clk_en);
			if (smartcard_mpll0)
				smc_mp0_clk_set(smc->param.freq);
#ifdef RST_FROM_PIO
			_gpio_out(smc->reset_pin,
				  RESET_ENABLE, SMC_RESET_PIN_NAME);
#endif

			usleep_range(800 / smc->param.freq, 800);

			/*disable receive interrupt */
			sc_int = SMC_READ_REG(INTR);
			sc_int_reg->recv_fifo_bytes_threshold_int_mask = 0;
			sc_int_reg->rst_expired_int_mask = 0;
			SMC_WRITE_REG(INTR, sc_int | 0x3FF);

			sc_reg0_reg->rst_level = RESET_DISABLE;
			sc_reg0_reg->start_atr = 1;
			SMC_WRITE_REG(REG0, sc_reg0);

			/*write again for atr count*/
			SMC_WRITE_REG(REG0, sc_reg0);
#ifdef RST_FROM_PIO
			_gpio_out(smc->reset_pin,
				  RESET_DISABLE, SMC_RESET_PIN_NAME);
#endif
		} else {
			smc_hw_deactive(smc);

			usleep_range(200, 400);

			smc_hw_active(smc);

			smc_reset_prepare(smc);

			sc_reg0_reg->clk_en = 1;
			sc_reg0_reg->enable = 0;
			sc_reg0_reg->rst_level = RESET_ENABLE;
			SMC_WRITE_REG(REG0, sc_reg0);
			smc_clk_enable(sc_reg0_reg->clk_en);

			if (smartcard_mpll0)
				smc_mp0_clk_set(smc->param.freq);

#ifdef RST_FROM_PIO
			if (smc->use_enable_pin) {
				_gpio_out(smc->enable_pin,
					  smc->enable_level,
					  SMC_ENABLE_PIN_NAME);
				usleep_range(100, 200);
			}
			_gpio_out(smc->reset_pin,
				  RESET_ENABLE, SMC_RESET_PIN_NAME);
#endif
			usleep_range(2000, 3000);	/*>= 400/f ; */

			/*disable receive interrupt */
			sc_int = SMC_READ_REG(INTR);
			sc_int_reg->recv_fifo_bytes_threshold_int_mask = 0;
			sc_int_reg->rst_expired_int_mask = 0;
			SMC_WRITE_REG(INTR, sc_int | 0x3FF);

			sc_reg0_reg->rst_level = RESET_DISABLE;
//                      sc_reg0_reg->start_atr_en = 1;
			sc_reg0_reg->start_atr = 1;
			sc_reg0_reg->enable = 1;
#if (ETU_CLK_SEL == 0)
			sc_reg0_reg->etu_divider = ETU_DIVIDER_CLOCK_HZ *
			    smc->param.f / (smc->param.d * smc->param.freq) - 1;
#else
			sc_reg0_reg->etu_divider = smc->param.f / smc->param.d - 1;
#endif
			SMC_WRITE_REG(REG0, sc_reg0);

			/*write again for atr count*/
			SMC_WRITE_REG(REG0, sc_reg0);
#ifdef RST_FROM_PIO

			_gpio_out(smc->reset_pin,
				  RESET_DISABLE, SMC_RESET_PIN_NAME);
#endif
		}

#if defined(ATR_FROM_INT)
		/*enable receive interrupt */
		sc_int = SMC_READ_REG(INTR);
		sc_int_reg->recv_fifo_bytes_threshold_int_mask = 1;
		SMC_WRITE_REG(INTR, sc_int | 0x3FF);
#endif

		/*msleep(atr_delay); */
		ret = smc_hw_read_atr(smc);

#ifdef SW_INVERT
		smc->atr_mode = 0;
#endif

#if defined(ATR_FROM_INT)
		/*disable receive interrupt */
		sc_int = SMC_READ_REG(INTR);
		sc_int_reg->recv_fifo_bytes_threshold_int_mask = 0;
		SMC_WRITE_REG(INTR, sc_int | 0x3FF);
#endif

		/*Disable ATR */
		sc_reg0 = SMC_READ_REG(REG0);
//              sc_reg0_reg->start_atr_en = 0;
		sc_reg0_reg->start_atr = 0;
		SMC_WRITE_REG(REG0, sc_reg0);

#ifndef DISABLE_RECV_INT
		sc_int_reg->recv_fifo_bytes_threshold_int_mask = 1;
#endif
		SMC_WRITE_REG(INTR, sc_int | 0x3FF);
	}

	return ret;
}

static int smc_hw_hot_reset(struct smc_dev *smc)
{
	unsigned long flags;
	int ret;
	unsigned long sc_reg0 = SMC_READ_REG(REG0);
	struct smccard_hw_reg0 *sc_reg0_reg = (void *)&sc_reg0;
	unsigned long sc_int;
	struct smc_interrupt_reg *sc_int_reg = (void *)&sc_int;

	pr_dbg("smc read reg0 0x%lx\n", sc_reg0);

	spin_lock_irqsave(&smc->slock, flags);
	if (smc->cardin)
		ret = 0;
	else
		ret = -ENODEV;
	spin_unlock_irqrestore(&smc->slock, flags);

	if (ret >= 0) {
		/*Reset */
		if (smc->active) {
			smc_reset_prepare(smc);

			sc_reg0_reg->rst_level = RESET_ENABLE;
			sc_reg0_reg->enable = 0;
			sc_reg0_reg->clk_en = 1;
			SMC_WRITE_REG(REG0, sc_reg0);
			smc_clk_enable(sc_reg0_reg->clk_en);
#ifdef RST_FROM_PIO
			_gpio_out(smc->reset_pin, RESET_ENABLE,
				  SMC_RESET_PIN_NAME);
#endif
			/*te >= 40000/f to meet synamedia's requirement*/
			usleep_range(40000 * 1000 / smc->param.freq,
				     40000 * 1000 / smc->param.freq + 20);

			/*disable receive interrupt */
			sc_int = SMC_READ_REG(INTR);
			sc_int_reg->recv_fifo_bytes_threshold_int_mask = 0;
			sc_int_reg->rst_expired_int_mask = 0;
			SMC_WRITE_REG(INTR, sc_int | 0x3FF);

			sc_reg0_reg->rst_level = RESET_DISABLE;
			sc_reg0_reg->start_atr = 1;
			sc_reg0_reg->enable = 1;
			SMC_WRITE_REG(REG0, sc_reg0);

			/*write again for atr count*/
			SMC_WRITE_REG(REG0, sc_reg0);
#ifdef RST_FROM_PIO
			if (smc->use_enable_pin) {
				_gpio_out(smc->enable_pin,
					  smc->enable_level,
					  SMC_ENABLE_PIN_NAME);
				usleep_range(100, 200);
			}

			_gpio_out(smc->reset_pin, RESET_DISABLE,
				  SMC_RESET_PIN_NAME);
#endif
			/*tf >= 400/f && <= 40000/f*/
//remove this for reducing get ATR time,
//there are delay in smc_hw_read_atr.
//			usleep_range(400 * 1000 / smc->param.freq + 20,
//				     40000 * 1000 / smc->param.freq);
		}
#if defined(ATR_FROM_INT)
		/*enable receive interrupt */
		sc_int = SMC_READ_REG(INTR);
		sc_int_reg->recv_fifo_bytes_threshold_int_mask = 1;
		SMC_WRITE_REG(INTR, sc_int | 0x3FF);
#endif

		/*msleep(atr_delay); */
		ret = smc_hw_read_atr(smc);

#ifdef SW_INVERT
		smc->atr_mode = 0;
#endif

#if defined(ATR_FROM_INT)
		/*disable receive interrupt */
		sc_int = SMC_READ_REG(INTR);
		sc_int_reg->recv_fifo_bytes_threshold_int_mask = 0;
		SMC_WRITE_REG(INTR, sc_int | 0x3FF);
#endif

		/*Disable ATR */
		sc_reg0 = SMC_READ_REG(REG0);
		//              sc_reg0_reg->start_atr_en = 0;
		sc_reg0_reg->start_atr = 0;
		SMC_WRITE_REG(REG0, sc_reg0);

#ifndef DISABLE_RECV_INT
		sc_int_reg->recv_fifo_bytes_threshold_int_mask = 1;
#endif
		SMC_WRITE_REG(INTR, sc_int | 0x3FF);
	}

	return ret;
}

static int smc_hw_get_status(struct smc_dev *smc, int *sret)
{
	unsigned long flags;
	int cardin;
#ifndef DET_FROM_PIO
	unsigned int reg_val;
	struct smccard_hw_reg0 *reg = (struct smccard_hw_reg0 *)&reg_val;
#endif

#ifdef DET_FROM_PIO
	cardin = _gpio_in(smc->detect_pin, OWNER_NAME);
	//	pr_dbg("get_status: card detect: %d\n", smc->cardin);
	if (!cardin) {
		if (smc->use_enable_pin)
			_gpio_out(smc->enable_pin,
				  !smc->enable_level, SMC_ENABLE_PIN_NAME);
		cardin = _gpio_in(smc->detect_pin, OWNER_NAME);
	}
#else
	reg_val = SMC_READ_REG(REG0);
	cardin = reg->card_detect;
	/* pr_error("get_status: smc reg0 %08x,
	 * card detect: %d\n", reg_val, reg->card_detect);
	 */
#endif
	/* pr_dbg("det:%d, det_invert:%d\n",
	 * smc->cardin, smc->detect_invert);
	 */
	spin_lock_irqsave(&smc->slock, flags);
	smc->cardin = cardin;
	if (smc->detect_invert)
		smc->cardin = !smc->cardin;

	*sret = smc->cardin;

	spin_unlock_irqrestore(&smc->slock, flags);

	return 0;
}

static inline void _atomic_wrap_inc(int *p, int wrap)
{
	int i = *p;

	i++;
	if (i >= wrap)
		i = 0;
	*p = i;
}

static inline void _atomic_wrap_add(int *p, int add, int wrap)
{
	int i = *p;

	i += add;
	if (i >= wrap)
		i %= wrap;
	*p = i;
}

static inline int smc_can_recv_max(struct smc_dev *smc)
{
	int start = smc->recv_start;
	int end = smc->recv_end;

	if (end >= start)
		return RECV_BUF_SIZE - end + start;
	else
		return start - end;
}

static int smc_hw_start_send(struct smc_dev *smc)
{
	unsigned int sc_status;
	struct smc_status_reg *sc_status_reg =
	    (struct smc_status_reg *)&sc_status;
	u8 byte;

	{
		unsigned long v = 0;
		struct smccard_hw_reg6 *reg6;

		v = SMC_READ_REG(REG6);
		reg6 = (struct smccard_hw_reg6 *)&v;

		/*Just write one byte, set N = 0*/
		if ((smc->send_end + 1) % SEND_BUF_SIZE == smc->send_start) {
			if (reg6->N_parameter != 0) {
				reg6->N_parameter = 0;
				SMC_WRITE_REG(REG6, v);
			}
		} else {
			/*recovery register N_parameter*/
			if (reg6->N_parameter != smc->param.n) {
				reg6->N_parameter = smc->param.n;
				SMC_WRITE_REG(REG6, v);
			}
		}
	}

	/*trigger only */
	sc_status = SMC_READ_REG(STATUS);
	if (smc->send_end != smc->send_start &&
		!sc_status_reg->xmit_fifo_full) {
		pr_dbg("s i f [%d:%d]\n", smc->send_start, smc->send_end);
		byte = smc->send_buf[smc->send_end];
		_atomic_wrap_inc(&smc->send_end, SEND_BUF_SIZE);
#ifdef SW_INVERT
		if (smc->sc_type == SC_INVERSE)
			byte = inv_table[byte];
#endif
		SMC_WRITE_REG(FIFO, byte);
		pr_dbg("send 1st byte to hw\n");
	}

	return 0;
}

static int _task_send_func(void *data)
{
	struct smc_dev *smc = (struct smc_dev *)data;
	unsigned int sc_status;
	struct smc_status_reg *sc_status_reg =
	    (struct smc_status_reg *)&sc_status;
	u8 byte;
	unsigned long v = 0;
	struct smccard_hw_reg6 *reg6;
	int ret = 0;
	unsigned long sc_int;
	struct smc_interrupt_reg *sc_int_reg = (void *)&sc_int;
	struct sched_param param = {.sched_priority = MAX_RT_PRIO - 1};

	sched_setscheduler(current, SCHED_FIFO, &param);

	while (smc->send_task.running) {
		ret = wait_event_interruptible(smc->send_task.wait_queue, smc->send_task.wake_up);
		smc->send_task.wake_up = 0;
		if (!smc->send_task.running)
			return 0;
		v = SMC_READ_REG(REG6);
		reg6 = (struct smccard_hw_reg6 *)&v;

#ifdef FIX_NO_ACK
		/*recovery register N_parameter*/
		if (reg6->N_parameter != smc->param.n) {
			reg6->N_parameter = smc->param.n;
			SMC_WRITE_REG(REG6, v);
		}
#endif
		pr_dbg("s i f [%d:%d]\n", smc->send_start, smc->send_end);

		while (1) {
			sc_status = SMC_READ_REG(STATUS);

			/*write done, exit write action*/
			if (smc->send_end == smc->send_start)
				break;
			if (sc_status_reg->xmit_fifo_full)
				usleep_range(50, 100);

			v = SMC_READ_REG(REG6);
			reg6 = (struct smccard_hw_reg6 *)&v;

			/*Just write one byte, set N = 0*/
			if ((smc->send_end + 1) % SEND_BUF_SIZE == smc->send_start) {
#ifdef FIX_NO_ACK
				if (reg6->N_parameter != 0) {
					reg6->N_parameter = 0;
					SMC_WRITE_REG(REG6, v);
				}
#endif
				/*trigger write done interrupt*/
				sc_int = SMC_READ_REG(INTR);
				sc_int_reg->send_fifo_last_byte_int_mask = 1;
				SMC_WRITE_REG(INTR, sc_int | 0x3FF);
			}

			sc_status = SMC_READ_REG(STATUS);
			if (smc->send_end != smc->send_start &&
				!sc_status_reg->xmit_fifo_full) {
				byte = smc->send_buf[smc->send_end];
				_atomic_wrap_inc(&smc->send_end, SEND_BUF_SIZE);
#ifdef SW_INVERT
				if (smc->sc_type == SC_INVERSE)
					byte = inv_table[byte];
#endif
				SMC_WRITE_REG(FIFO, byte);
				pr_dbg("u s > %02x\n", byte);
			}
		}
	}
	return 0;
}

static int smc_hw_start_send_use_task(struct smc_dev *smc)
{
	if (smc->send_task.running == 0) {
		smc->send_task.running = 1;
		smc->send_task.wake_up = 0;
		init_waitqueue_head(&smc->send_task.wait_queue);
		smc->send_task.out_task =  kthread_run(_task_send_func,
				(void *)smc, "smc_send_task");
		if (!smc->send_task.out_task) {
			pr_error("create smc send task fail\n");
			smc->send_task.running = 0;
			return -1;
		}
	}

	if (smc->send_task.running) {
		smc->send_task.wake_up = 1;
		wake_up_interruptible(&smc->send_task.wait_queue);
	}
	return 0;
}

#ifdef SMC_FIQ
static irqreturn_t smc_bridge_isr(int irq, void *dev_id)
{
	struct smc_dev *smc = (struct smc_dev *)dev_id;

#ifdef DET_FROM_PIO
	smc->cardin = _gpio_in(smc->detect_pin, SMC_DETECT_PIN_NAME);
	if (smc->detect_invert)
		smc->cardin = !smc->cardin;
#endif

	if (smc->recv_start != smc->recv_end)
		wake_up_interruptible(&smc->rd_wq);
	if (smc->send_start == smc->send_end)
		wake_up_interruptible(&smc->wr_wq);

	return IRQ_HANDLED;
}

static void smc_irq_handler(void)
{
	struct smc_dev *smc = &smc_dev[0];
	unsigned int sc_status;
	unsigned int sc_reg0;
	unsigned int sc_int;
	unsigned int rev_status;
	struct smc_status_reg *sc_status_reg =
	    (struct smc_status_reg *)&sc_status;
	struct smc_interrupt_reg *sc_int_reg =
	    (struct smc_interrupt_reg *)&sc_int;
	struct smccard_hw_reg0 *sc_reg0_reg = (void *)&sc_reg0;
	struct smccard_hw_reg8 *rev_status_reg =
		(struct smccard_hw_reg8 *)&rev_status;

	sc_int = SMC_READ_REG(INTR);
	/*Fpr("smc intr:0x%x\n", sc_int); */

	if (sc_int_reg->recv_fifo_bytes_threshold_int) {
		int num = 0;

		sc_status = SMC_READ_REG(STATUS);
		rev_status = SMC_READ_REG(REG8);
		num = rev_status_reg->recv_fifo_count;

		if (num > smc_can_recv_max(smc)) {
			pr_error("receive buffer overflow\n");
		} else {
			u8 byte;

			while (rev_status_reg->recv_fifo_count) {
				byte = SMC_READ_REG(FIFO);
#ifdef SW_INVERT
				if (!smc->atr_mode &&
				    smc->sc_type == SC_INVERSE)
					byte = inv_table[byte];
#endif
				smc->recv_buf[smc->recv_end] = byte;
				_atomic_wrap_inc(&smc->recv_end, RECV_BUF_SIZE);
				num++;
				sc_status = SMC_READ_REG(STATUS);
				Fpr("F%02x ", byte);
			}
			Fpr("Fr > %d bytes\n", num);

			fiq_bridge_pulse_trigger(&smc->smc_fiq_bridge);
		}

		sc_int_reg->recv_fifo_bytes_threshold_int = 0;
	}

	if (sc_int_reg->send_fifo_last_byte_int) {
		int start = smc->send_start;
		int cnt = 0;
		u8 byte;

		while (1) {
			sc_status = SMC_READ_REG(STATUS);
			if (smc->send_end == start ||
			    sc_status_reg->xmit_fifo_full)
				break;

			byte = smc->send_buf[smc->send_end];
			Fpr("Fs > %02x ", byte);
			_atomic_wrap_inc(&smc->send_end, SEND_BUF_SIZE);
#ifdef SW_INVERT
			if (smc->sc_type == SC_INVERSE)
				byte = inv_table[byte];
#endif
			SMC_WRITE_REG(FIFO, byte);
			cnt++;
		}

		Fpr("Fs > %d bytes\n", cnt);

		if (smc->send_end == start) {
			sc_int_reg->send_fifo_last_byte_int_mask = 0;
			sc_int_reg->recv_fifo_bytes_threshold_int_mask = 1;
			fiq_bridge_pulse_trigger(&smc->smc_fiq_bridge);
		}

		sc_int_reg->send_fifo_last_byte_int = 0;
	}

	SMC_WRITE_REG(INTR, sc_int | 0x3FF);

#ifndef DET_FROM_PIO
	sc_reg0 = SMC_READ_REG(REG0);
	smc->cardin = sc_reg0_reg->card_detect;
	if (smc->detect_invert)
		smc->cardin = !smc->cardin;
#endif
}

#else

static int transmit_chars(struct smc_dev *smc)
{
	unsigned int status;
	struct smc_status_reg *status_r = (struct smc_status_reg *)&status;
	int cnt = 0;
	u8 byte;
	int start = smc->send_start;

	while (1) {
		status = SMC_READ_REG(STATUS);
		if (smc->send_end == start || status_r->xmit_fifo_full)
			break;
		/*N !=0, last byte will set N=0, then avoid lost ACK*/
		if (smc->param.n != 0) {
			if ((smc->send_end + 1) % SEND_BUF_SIZE == start) {
				if (cnt == 0) {
					unsigned long v = 0;
					struct smccard_hw_reg6 *reg6;

					v = SMC_READ_REG(REG6);
					reg6 = (struct smccard_hw_reg6 *)&v;
					reg6->N_parameter = 0;
					SMC_WRITE_REG(REG6, v);
				} else {
					break;
				}
			}
		}

		byte = smc->send_buf[smc->send_end];
		Ipr("s > %02x\n", byte);
		_atomic_wrap_inc(&smc->send_end, SEND_BUF_SIZE);
#ifdef SW_INVERT
		if (smc->sc_type == SC_INVERSE)
			byte = inv_table[byte];
#endif
		SMC_WRITE_REG(FIFO, byte);
		cnt++;
	}

	Ipr("s > %d bytes\n", cnt);
	return cnt;
}

static int receive_chars(struct smc_dev *smc)
{
	unsigned int status;
	unsigned int rev_status;
	struct smc_status_reg *status_r = (struct smc_status_reg *)&status;
	struct smccard_hw_reg8 *rev_status_reg =
		(struct smccard_hw_reg8 *)&rev_status;
	int cnt = 0;
	u8 byte;

	rev_status = SMC_READ_REG(REG8);
	if (rev_status_reg->recv_fifo_count > smc_can_recv_max(smc)) {
		Ipr("receive buffer overflow\n");
		return -1;
	}

	status = SMC_READ_REG(STATUS);
	while (!status_r->recv_fifo_empty) {
		byte = SMC_READ_REG(FIFO);
#ifdef SW_INVERT
		if (!smc->atr_mode && smc->sc_type == SC_INVERSE)
			byte = inv_table[byte];
#endif
		smc->recv_buf[smc->recv_end] = byte;
		_atomic_wrap_inc(&smc->recv_end, RECV_BUF_SIZE);
		cnt++;
		status = SMC_READ_REG(STATUS);
		Ipr("%02x ", byte);
	}
	Ipr("r > %d bytes\n", cnt);

	return cnt;
}

static void smc_irq_bh_handler(unsigned long arg)
{
	struct smc_dev *smc = (struct smc_dev *)arg;
#ifndef DET_FROM_PIO
	unsigned int sc_reg0;
	struct smccard_hw_reg0 *sc_reg0_reg = (void *)&sc_reg0;
#endif

	/*Read card status */
#ifndef DET_FROM_PIO
	sc_reg0 = SMC_READ_REG(REG0);
	smc->cardin = sc_reg0_reg->card_detect;
#else
	smc->cardin = _gpio_in(smc->detect_pin, SMC_DETECT_PIN_NAME);
#endif
	if (smc->detect_invert)
		smc->cardin = !smc->cardin;

	if (smc->recv_start != smc->recv_end) {
		wake_up_interruptible(&smc->rd_wq);
		Fpr("wakeup recv\n");
	}
	if (smc->send_start == smc->send_end) {
		wake_up_interruptible(&smc->wr_wq);
		Fpr("wakeup send\n");
	}
}

static irqreturn_t smc_irq_handler(int irq, void *data)
{
	struct smc_dev *smc = (struct smc_dev *)data;
	unsigned int sc_status;
	unsigned int sc_int;
	struct smc_status_reg *sc_status_reg =
	    (struct smc_status_reg *)&sc_status;

	sc_int = SMC_READ_REG(INTR);
	Ipr("Int:0x%x\n", sc_int);

	sc_status = SMC_READ_REG(STATUS);
	Ipr("Sta:0x%x\n", sc_status);

	/*Receive */
	sc_status = SMC_READ_REG(STATUS);
	if (!sc_status_reg->recv_fifo_empty)
		receive_chars(smc);

	SMC_WRITE_REG(INTR, sc_int);

	/* Send */
	sc_status = SMC_READ_REG(STATUS);
	if (!sc_status_reg->xmit_fifo_full)
		transmit_chars(smc);

	tasklet_schedule(&smc->tasklet);

	return IRQ_HANDLED;
}
#endif /*ifdef SMC_FIQ */

static void smc_dev_deinit(struct smc_dev *smc)
{
	if (smc->irq_num != -1) {
		free_irq(smc->irq_num, &smc);
		smc->irq_num = -1;
		tasklet_kill(&smc->tasklet);
	}
	if (smc->send_task.running)	{
		smc->send_task.running = 0;
		smc->send_task.wake_up = 1;
		wake_up_interruptible(&smc->send_task.wait_queue);
	}
	if (smc->use_enable_pin)
		_gpio_free(smc->enable_pin, SMC_ENABLE_PIN_NAME);
	clk_disable_unprepare(aml_smartcard_clk);

#ifdef DET_FROM_PIO
	if (smc->detect_pin)
		_gpio_free(smc->detect_pin, SMC_DETECT_PIN_NAME);
#endif
#ifdef RST_FROM_PIO
	if (smc->reset_pin)
		_gpio_free(smc->reset_pin, SMC_RESET_PIN_NAME);
#endif
#ifdef CONFIG_OF
	if (smc->pinctrl)
		devm_pinctrl_put(smc->pinctrl);
#endif
	if (smc->dev)
		device_destroy(&smc_class, MKDEV(smc_major, smc->id));

	mutex_destroy(&smc->lock);

	smc->init = 0;

#if defined(MESON_CPU_TYPE_MESON8) && (MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON8)
	CLK_GATE_OFF(SMART_CARD_MPEG_DOMAIN);
#endif
}

static int _set_gpio(struct smc_dev *smc, struct gpio_desc **gpiod,
		     char *str, int input_output, int output_level)
{
	int ret = 0;
	/*pr_dbg("smc _set_gpio %s %p\n", str, *gpiod); */
	if (IS_ERR(*gpiod)) {
		pr_dbg("smc %s request failed\n", str);
		return -1;
	}
	if (input_output == OUTPUT) {
		*gpiod = gpiod_get(&smc->pdev->dev, str,
				   output_level ? GPIOD_OUT_HIGH :
				   GPIOD_OUT_LOW);
		ret = gpiod_direction_output(*gpiod, output_level);
	} else if (input_output == INPUT) {
		*gpiod = gpiod_get(&smc->pdev->dev, str, GPIOD_IN);
		ret = gpiod_direction_input(*gpiod);
		ret |= gpiod_set_pull(*gpiod, GPIOD_PULL_UP);
	} else {
		pr_dbg("SMC Request gpio direction invalid\n");
	}
	return ret;
}

static int smc_dev_init(struct smc_dev *smc, int id)
{
#ifndef CONFIG_OF
	struct resource *res;
#else
	int ret;
	u32 value;
	char buf[32];
	const char *dts_str;
	struct resource *res;
#endif

#if defined(MESON_CPU_TYPE_MESON8) && (MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON8)
	CLK_GATE_ON(SMART_CARD_MPEG_DOMAIN);
#endif
	/*of_match_node(smc_dt_match, smc->pdev->dev.of_node); */
	smc->id = id;

	smc->pinctrl = devm_pinctrl_get_select_default(&smc->pdev->dev);
	if (IS_ERR(smc->pinctrl))
		return -1;

	ret = of_property_read_string(smc->pdev->dev.of_node,
				      "smc_need_enable_pin", &dts_str);
	if (ret < 0) {
		pr_error("failed to get smartcard node.\n");
		return -EINVAL;
	}
	if (strcmp(dts_str, "yes") == 0)
		smc->use_enable_pin = 1;
	else
		smc->use_enable_pin = 0;
	if (smc->use_enable_pin == 1) {
		smc->enable_pin = NULL;
		if (!smc->enable_pin) {
			snprintf(buf, sizeof(buf), "smc%d_enable_pin", id);
			_set_gpio(smc, &smc->enable_pin, "enable_pin",
				  OUTPUT, OUTLEVEL_HIGH);
		}

		if (smc->use_enable_pin) {
			snprintf(buf, sizeof(buf), "smc%d_enable_level", id);
			ret = of_property_read_u32(smc->pdev->dev.of_node,
				buf, &value);
			if (!ret) {
				smc->enable_level = value;
				if (smc->enable_pin) {
					_gpio_out(smc->enable_pin,
						smc->enable_level,
						SMC_ENABLE_PIN_NAME);
					pr_error("enable_pin: -->(%d)\n",
						 (!smc->enable_level) ? 1 : 0);
				}
			} else {
				pr_error("cannot find resource \"%s\"\n", buf);
			}
		}
	}

	smc->reset_pin = NULL;
	ret = _set_gpio(smc, &smc->reset_pin, "reset_pin",
		OUTPUT, OUTLEVEL_HIGH);
	if (ret) {
		pr_dbg("smc reset pin request failed, we can not work now\n");
		return -1;
	}
	ret = of_property_read_u32(smc->pdev->dev.of_node,
		"reset_level", &value);
	smc->reset_level = value;
	pr_dbg("smc reset_level %d\n", value);

	smc->irq_num = smc0_irq;
	if (smc->irq_num == -1) {
		snprintf(buf, sizeof(buf), "smc%d_irq", id);

		res = platform_get_resource_byname(smc->pdev,
			IORESOURCE_IRQ, buf);
		if (!res) {
			pr_error("cannot get resource \"%s\"\n", buf);
			return -1;
		}
		smc->irq_num = res->start;
	}
	smc->pin_clk_pinmux_reg = -1;
	snprintf(buf, sizeof(buf), "smc%d_clk_pinmux_reg", id);
	ret = of_property_read_u32(smc->pdev->dev.of_node, buf, &value);
	if (!ret) {
		smc->pin_clk_pinmux_reg = value;
		pr_error("%s: 0x%x\n", buf, smc->pin_clk_pinmux_reg);
	} else {
		pr_error("cannot find resource \"%s\"\n", buf);
	}
	smc->pin_clk_pinmux_bit = -1;
	snprintf(buf, sizeof(buf), "smc%d_clk_pinmux_bit", id);
	ret = of_property_read_u32(smc->pdev->dev.of_node, buf, &value);
	if (!ret) {
		smc->pin_clk_pinmux_bit = value;
		pr_error("%s: 0x%x\n", buf, smc->pin_clk_pinmux_bit);
	} else {
		pr_error("cannot find resource \"%s\"\n", buf);
	}
	smc->pin_clk_oen_reg = -1;
	snprintf(buf, sizeof(buf), "smc%d_clk_oen_reg", id);
	ret = of_property_read_u32(smc->pdev->dev.of_node, buf, &value);
	if (!ret) {
		smc->pin_clk_oen_reg = value;
		pr_error("%s: 0x%x\n", buf, smc->pin_clk_oen_reg);
	} else {
		pr_error("cannot find resource \"%s\"\n", buf);
	}

	smc->pin_clk_out_reg = -1;
	snprintf(buf, sizeof(buf), "smc%d_clk_out_reg", id);
	ret = of_property_read_u32(smc->pdev->dev.of_node, buf, &value);
	if (!ret) {
		smc->pin_clk_out_reg = value;
		pr_error("%s: 0x%x\n", buf, smc->pin_clk_out_reg);
	} else {
		pr_error("cannot find resource \"%s\"\n", buf);
	}

	smc->pin_clk_oebit = -1;
	snprintf(buf, sizeof(buf), "smc%d_clk_oebit", id);
	ret = of_property_read_u32(smc->pdev->dev.of_node, buf, &value);
	if (!ret) {
		smc->pin_clk_oebit = value;
		pr_error("%s: 0x%x\n", buf, smc->pin_clk_oebit);
	} else {
		pr_error("cannot find resource \"%s\"\n", buf);
	}
	smc->pin_clk_oubit = -1;
	snprintf(buf, sizeof(buf), "smc%d_clk_oubit", id);
	ret = of_property_read_u32(smc->pdev->dev.of_node, buf, &value);
	if (!ret) {
		smc->pin_clk_oubit = value;
		pr_error("%s: 0x%x\n", buf, smc->pin_clk_oubit);
	} else {
		pr_error("cannot find resource \"%s\"\n", buf);
	}
#ifdef DET_FROM_PIO
	smc->detect_pin = NULL;
	if (!smc->detect_pin) {
		ret = _set_gpio(smc, &smc->detect_pin, "detect_pin", INPUT, 0);
		if (ret) {
			pr_dbg("smc detect_pin failed, we can not work\n");
			return -1;
		}
	}
#else /*CONFIG_OF */
	ret = platform_get_resource_byname(smc->pdev, IORESOURCE_MEM, buf);
	if (!ret) {
		pr_error("cannot get resource \"%s\"\n", buf);
	} else {
		smc->detect_pin = res->start;
		_gpio_request(smc->detect_pin, SMC_DETECT_PIN_NAME);
	}
#endif /*CONFIG_OF */

	snprintf(buf, sizeof(buf), "smc%d_clock_source", id);
	ret = of_property_read_u32(smc->pdev->dev.of_node, buf, &value);
	if (!ret) {
		clock_source = value;
		pr_error("%s: %d\n", buf, clock_source);
	} else {
		pr_error("cannot find resource \"%s\"\n", buf);
	}

	smc->detect_invert = 0;
	snprintf(buf, sizeof(buf), "smc%d_det_invert", id);
	ret = of_property_read_u32(smc->pdev->dev.of_node, buf, &value);
	if (!ret) {
		smc->detect_invert = value;
		pr_error("%s: %d\n", buf, smc->detect_invert);
	} else {
		pr_error("cannot find resource \"%s\"\n", buf);
	}
	smc->enable_5v3v_pin = NULL;
	if (!smc->enable_5v3v_pin) {
		snprintf(buf, sizeof(buf), "smc%d_5v3v_pin", id);
		ret = _set_gpio(smc, &smc->enable_5v3v_pin,
				"enable_5v3v_pin", OUTPUT, OUTLEVEL_HIGH);
		if (ret == -1)
			pr_error("5v3v_pin not work, it maybe face problems\n");
	}

	smc->enable_5v3v_level = 0;
	snprintf(buf, sizeof(buf), "smc%d_5v3v_level", id);
	ret = of_property_read_u32(smc->pdev->dev.of_node, buf, &value);
	if (!ret) {
		smc->enable_5v3v_level = value;
		if (smc->enable_5v3v_pin) {
			_gpio_out(smc->enable_5v3v_pin,
				  smc->enable_5v3v_level,
				  SMC_ENABLE_5V3V_PIN_NAME);
			pr_error("5v3v_pin: -->(%d)\n",
				 (smc->enable_5v3v_level) ? 1 : 0);
		}
	} else {
		pr_error("cannot find resource \"%s\"\n", buf);
	}

	init_waitqueue_head(&smc->rd_wq);
	init_waitqueue_head(&smc->wr_wq);

	spin_lock_init(&smc->slock);
	mutex_init(&smc->lock);

#ifdef SMC_FIQ
	{
		int r = -1;

		smc->smc_fiq_bridge.handle = smc_bridge_isr;
		smc->smc_fiq_bridge.key = (u32)smc;
		smc->smc_fiq_bridge.name = "smc_bridge_isr";
		r = register_fiq_bridge_handle(&smc->smc_fiq_bridge);
		if (r) {
			pr_error("smc fiq bridge register error.\n");
			return -1;
		}
	}

	request_fiq(smc->irq_num, &smc_irq_handler);
#else
	smc->irq_num = request_irq(smc->irq_num,
				   (irq_handler_t)smc_irq_handler,
				   IRQF_SHARED | IRQF_TRIGGER_HIGH,
				   "smc", smc);
	if (smc->irq_num < 0) {
		pr_error("request irq error %d!\n", smc->irq_num);
		smc_dev_deinit(smc);
		return -1;
	}

	tasklet_init(&smc->tasklet, smc_irq_bh_handler, (unsigned long)smc);
#endif
	snprintf(buf, sizeof(buf), "smc%d", smc->id);
	smc->dev = device_create(&smc_class, NULL,
		MKDEV(smc_major, smc->id), smc, buf);
	if (!smc->dev) {
		pr_error("create device error!\n");
		smc_dev_deinit(smc);
		return -1;
	}

	smc_default_init(smc);

	smc->init = 1;

	smc_hw_setup(smc, 0);

	return 0;
}

static int smc_open(struct inode *inode, struct file *filp)
{
	int id = iminor(inode);
	struct smc_dev *smc = NULL;

	id = 0;
	smc = &smc_dev[id];
	mutex_init(&smc->lock);
	mutex_lock(&smc->lock);

#ifdef FILE_DEBUG
	open_debug();
#endif

	if (smc->used) {
		mutex_unlock(&smc->lock);
		pr_error("smartcard %d already openned!", id);
		return -EBUSY;
	}

	smc->used = 1;

#if MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON6
	switch_mod_gate_by_name("smart_card", 1);
#endif

	mutex_unlock(&smc->lock);

	filp->private_data = smc;

	return 0;
}

static int smc_close(struct inode *inode, struct file *filp)
{
	struct smc_dev *smc = (struct smc_dev *)filp->private_data;

	mutex_lock(&smc->lock);

	smc_hw_deactive(smc);

#ifndef KEEP_PARAM_AFTER_CLOSE
	smc_default_init(smc);
#endif

	smc->used = 0;

#if MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON6
	switch_mod_gate_by_name("smart_card", 0);
#endif

#ifdef FILE_DEBUG
	close_debug();
#endif

	mutex_unlock(&smc->lock);

	return 0;
}

static ssize_t smc_read(struct file *filp,
			char __user *buff, size_t size, loff_t *ppos)
{
	struct smc_dev *smc = (struct smc_dev *)filp->private_data;
	unsigned long flags;
	int ret;
	int start = 0, end;
	int cnt = 0;
	long cr;

	memset(&smc->r_cache, 0, RECV_BUF_SIZE);
	ret = mutex_lock_interruptible(&smc->lock);
	if (ret)
		return ret;

	spin_lock_irqsave(&smc->slock, flags);
	if (ret == 0) {
		start = smc->recv_start;
		end = smc->recv_end;

		if (!smc->cardin) {
			ret = -ENODEV;
		} else if (start == end) {
			ret = -EAGAIN;
		} else {
			ret = (end > start) ? (end - start) :
			    (RECV_BUF_SIZE - start + end);
			if (ret > size)
				ret = size;
		}
	}

	if (ret > 0) {
		pr_dbg("read %d bytes\n", ret);
		cnt = RECV_BUF_SIZE - start;
		spin_unlock_irqrestore(&smc->slock, flags);
		if (cnt >= ret) {
			memcpy(smc->r_cache, smc->recv_buf + start, ret);
		} else {
			int cnt1 = ret - cnt;

			memcpy(smc->r_cache, smc->recv_buf + start, cnt);
			memcpy(smc->r_cache + cnt, smc->recv_buf, cnt1);
		}
		spin_lock_irqsave(&smc->slock, flags);
		_atomic_wrap_add(&smc->recv_start, ret, RECV_BUF_SIZE);
	}
	spin_unlock_irqrestore(&smc->slock, flags);

	mutex_unlock(&smc->lock);
	if (ret > 0) {
		cr = copy_to_user(buff, smc->r_cache, ret);
		if (cr != 0) {
			pr_error("copy_to_user fail ret:%ld\n", cr);
			return -1;
		}
	}

	return ret;
}

static ssize_t smc_write(struct file *filp,
			 const char __user *buff, size_t size, loff_t *offp)
{
	struct smc_dev *smc = (struct smc_dev *)filp->private_data;
	unsigned long flags;
	int ret;
	unsigned long sc_int;
	struct smc_interrupt_reg *sc_int_reg = (void *)&sc_int;
	int start = 0, end;
	long cp_ret;

	if (size > SEND_BUF_SIZE)
		return -1;

	memset(&smc->w_cache, 0, SEND_BUF_SIZE);
	cp_ret = copy_from_user(&smc->w_cache, buff, size);
	if (cp_ret != 0) {
		pr_error("smc:copy_from_user fail\n");
		return -1;
	}

	ret = mutex_lock_interruptible(&smc->lock);
	if (ret)
		return ret;

	spin_lock_irqsave(&smc->slock, flags);

	if (ret == 0) {
		start = smc->send_start;
		end = smc->send_end;

		if (!smc->cardin) {
			ret = -ENODEV;
		} else if (start != end) {
			ret = -EAGAIN;
		} else {
			ret = size;
			if (ret >= SEND_BUF_SIZE)
				ret = SEND_BUF_SIZE - 1;
		}
	}

	if (ret > 0) {
		int cnt = SEND_BUF_SIZE - start;

		spin_unlock_irqrestore(&smc->slock, flags);
		if (cnt >= ret) {
			memcpy(smc->send_buf + start, smc->w_cache, ret);
		} else {
			int cnt1 = ret - cnt;

			memcpy(smc->send_buf + start, smc->w_cache, cnt);
			memcpy(smc->send_buf, smc->w_cache + cnt, cnt1);
		}
		spin_lock_irqsave(&smc->slock, flags);
		_atomic_wrap_add(&smc->send_start, ret, SEND_BUF_SIZE);
	}

	spin_unlock_irqrestore(&smc->slock, flags);

	if (ret > 0) {
		sc_int = SMC_READ_REG(INTR);
#ifdef DISABLE_RECV_INT
		sc_int_reg->recv_fifo_bytes_threshold_int_mask = 0;
#endif
		sc_int_reg->recv_fifo_bytes_threshold_int_mask = 1;
		if (send_use_task)
			sc_int_reg->send_fifo_last_byte_int_mask = 0;
		else
			sc_int_reg->send_fifo_last_byte_int_mask = 1;
		SMC_WRITE_REG(INTR, sc_int | 0x3FF);

		pr_dbg("write %d bytes\n", ret);
		if (send_use_task)
			smc_hw_start_send_use_task(smc);
		else
			smc_hw_start_send(smc);
	}

	mutex_unlock(&smc->lock);

	return ret;
}

static unsigned int smc_poll(struct file *filp, struct poll_table_struct *wait)
{
	struct smc_dev *smc = (struct smc_dev *)filp->private_data;
	unsigned int ret = 0;
	unsigned long flags;

	poll_wait(filp, &smc->rd_wq, wait);
	poll_wait(filp, &smc->wr_wq, wait);

	spin_lock_irqsave(&smc->slock, flags);

	if (smc->recv_start != smc->recv_end) {
		ret |= POLLIN | POLLRDNORM;
		Fpr("rd\n");
	}
	if (smc->send_start == smc->send_end) {
		ret |= POLLOUT | POLLWRNORM;
		Fpr("wd\n");
	}
	if (!smc->cardin)
		ret |= POLLERR;

	spin_unlock_irqrestore(&smc->slock, flags);

	return ret;
}

static long smc_ioctl(struct file *file, unsigned int cmd, ulong arg)
{
	struct smc_dev *smc = (struct smc_dev *)file->private_data;
	int ret = 0;
	long cr;

	switch (cmd) {
	case AMSMC_IOC_RESET:
		{
			ret = mutex_lock_interruptible(&smc->lock);
			if (ret)
				return ret;
			ret = smc_hw_reset(smc);
			if (ret >= 0)
				cr = copy_to_user((void *)arg, &smc->atr,
						  sizeof(struct am_smc_atr));
			mutex_unlock(&smc->lock);
	} break;
	case AMSMC_IOC_HOT_RESET: {
		ret = mutex_lock_interruptible(&smc->lock);
		if (ret)
			return ret;
		ret = smc_hw_hot_reset(smc);
		if (ret >= 0)
			cr = copy_to_user((void *)arg, &smc->atr,
					  sizeof(struct am_smc_atr));
		mutex_unlock(&smc->lock);
	} break;
	case AMSMC_IOC_GET_STATUS:
		{
			int status;

			smc_hw_get_status(smc, &status);
			cr = copy_to_user((void *)arg, &status, sizeof(int));
		}
		break;
	case AMSMC_IOC_ACTIVE:
		{
			ret = mutex_lock_interruptible(&smc->lock);
			if (ret)
				return ret;

			ret = smc_hw_active(smc);

			mutex_unlock(&smc->lock);
		}
		break;
	case AMSMC_IOC_DEACTIVE:
		{
			ret = mutex_lock_interruptible(&smc->lock);
			if (ret)
				return ret;

			ret = smc_hw_deactive(smc);

			mutex_unlock(&smc->lock);
		}
		break;
	case AMSMC_IOC_GET_PARAM:
		{
			ret = mutex_lock_interruptible(&smc->lock);
			if (ret)
				return ret;
			cr = copy_to_user((void *)arg, &smc->param,
					  sizeof(struct am_smc_param));

			mutex_unlock(&smc->lock);
		}
		break;
	case AMSMC_IOC_SET_PARAM:
		{
			ret = mutex_lock_interruptible(&smc->lock);
			if (ret)
				return ret;

			cr = copy_from_user(&smc->param, (void *)arg,
					    sizeof(struct am_smc_param));
			ret = smc_hw_set_param(smc);

			mutex_unlock(&smc->lock);
		}
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
static long smc_ioctl_compat(struct file *filp,
			     unsigned int cmd, unsigned long args)
{
	unsigned long ret;

	args = (unsigned long)compat_ptr(args);
	ret = smc_ioctl(filp, cmd, args);
	return ret;
}
#endif

static const struct file_operations smc_fops = {
	.owner = THIS_MODULE,
	.open = smc_open,
	.write = smc_write,
	.read = smc_read,
	.release = smc_close,
	.unlocked_ioctl = smc_ioctl,
	.poll = smc_poll,
#ifdef CONFIG_COMPAT
	.compat_ioctl = smc_ioctl_compat,
#endif
};

static int smc_probe(struct platform_device *pdev)
{
	struct smc_dev *smc = NULL;
	int i, ret;

	mutex_lock(&smc_lock);

	if (sc2_smc_addr(pdev) != 0) {
		dev_err(&pdev->dev, "get smartcard reg map fail\n");
		mutex_unlock(&smc_lock);
		return -1;
	}

	for (i = 0; i < SMC_DEV_COUNT; i++) {
		if (!smc_dev[i].init) {
			smc = &smc_dev[i];
			break;
		}
	}
	aml_smartcard_clk = devm_clk_get(&pdev->dev, "smartcard");
	if (IS_ERR_OR_NULL(aml_smartcard_clk)) {
		dev_err(&pdev->dev, "get smartcard clk fail\n");
		aml_smartcard_clk = devm_clk_get(&pdev->dev, "smartcard_mpll0");
		if (IS_ERR_OR_NULL(aml_smartcard_clk)) {
			dev_err(&pdev->dev, "get smartcard mpll0 clk fail\n");
			mutex_unlock(&smc_lock);
			return -1;
		}
		smartcard_mpll0 = 1;
	}

	ret = clk_prepare_enable(aml_smartcard_clk);
	if (ret) {
		pr_error("enable smc_clk fail:%d\n", ret);
		clk_put(aml_smartcard_clk);
	} else {
		if (smartcard_mpll0 == 1)
			clk_set_rate(aml_smartcard_clk, 166500000);
		else if (!t5w_smartcard)
			clk_set_rate(aml_smartcard_clk, 4000000);
	}
	if (smc) {
		smc->init = 1;
		smc->pdev = pdev;
		dev_set_drvdata(&pdev->dev, smc);
		ret = smc_dev_init(smc, i);
		if (ret < 0)
			smc = NULL;
	}

	mutex_unlock(&smc_lock);

	return smc ? 0 : -1;
}

static int smc_remove(struct platform_device *pdev)
{
	struct smc_dev *smc = (struct smc_dev *)dev_get_drvdata(&pdev->dev);

	mutex_lock(&smc_lock);

	smc_dev_deinit(smc);

	mutex_unlock(&smc_lock);

	return 0;
}

static struct platform_driver smc_driver = {
	.probe = smc_probe,
	.remove = smc_remove,
	.driver = {
		   .name = "amlogic-smc_sc2",
		   .owner = THIS_MODULE,
		   .of_match_table = smc_dt_match,
	},
};

static int __init smc_sc2_mod_init(void)
{
	int ret = -1;

	mutex_init(&smc_lock);
	smc_major = register_chrdev(0, SMC_DEV_NAME, &smc_fops);
	if (smc_major <= 0) {
		mutex_destroy(&smc_lock);
		pr_error("register chrdev error\n");
		goto error_register_chrdev;
	}

	if (class_register(&smc_class) < 0) {
		pr_error("register class error\n");
		goto error_class_register;
	}

	if (platform_driver_register(&smc_driver) < 0) {
		pr_error("register platform driver error\n");
		goto error_platform_drv_register;
	}
	return 0;
error_platform_drv_register:
	class_unregister(&smc_class);
error_class_register:
	unregister_chrdev(smc_major, SMC_DEV_NAME);
error_register_chrdev:
	mutex_destroy(&smc_lock);
	return ret;
}

static void __exit smc_sc2_mod_exit(void)
{
	platform_driver_unregister(&smc_driver);
	class_unregister(&smc_class);
	unregister_chrdev(smc_major, SMC_DEV_NAME);
	mutex_destroy(&smc_lock);
}

module_init(smc_sc2_mod_init);

module_exit(smc_sc2_mod_exit);

MODULE_AUTHOR("AMLOGIC");

MODULE_DESCRIPTION("AMLOGIC smart card driver");

MODULE_LICENSE("GPL");
