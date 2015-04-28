/*
 * drivers/amlogic/amports/vdec.c
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

#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/amlogic/amports/vformat.h>
#include <linux/amlogic/iomap.h>
#include "vdec_reg.h"
#include "vdec.h"
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/libfdt_env.h>
#include <linux/of_reserved_mem.h>
#include <linux/dma-contiguous.h>
#include <linux/cma.h>
#include <linux/module.h>
#include <linux/slab.h>
#include "amports_priv.h"

#include "amports_config.h"
#include "amvdec.h"


#include "arch/clk.h"
#include <linux/reset.h>

static DEFINE_SPINLOCK(lock);

#define MC_SIZE (4096 * 4)

#define SUPPORT_VCODEC_NUM  1
static int inited_vcodec_num;
static int poweron_clock_level;
static unsigned int debug_trace_num = 16 * 20;
static int vdec_irq[VDEC_IRQ_MAX];
static struct platform_device *vdec_device;
static struct platform_device *vdec_core_device;
struct am_reg {
	char *name;
	int offset;
};

static struct cma *vdec_cma;
static struct vdec_dev_reg_s vdec_dev_reg;

static const char * const vdec_device_name[] = {
	"amvdec_mpeg12",
	"amvdec_mpeg4",
	"amvdec_h264",
	"amvdec_mjpeg",
	"amvdec_real",
	"amjpegdec",
	"amvdec_vc1",
	"amvdec_avs",
	"amvdec_yuv",
	"amvdec_h264mvc",
	"amvdec_h264_4k2k",
	"amvdec_h265"
};

void vdec_set_decinfo(struct dec_sysinfo *p)
{
	vdec_dev_reg.sys_info = p;
}

int vdec_set_resource(unsigned long start, unsigned long end, struct device *p)
{
	if (inited_vcodec_num != 0) {
		pr_info
		("ERROR:We can't support the change resource at running\n");
		return -1;
	}

	vdec_dev_reg.mem_start = start;
	vdec_dev_reg.mem_end = end;
	vdec_dev_reg.cma_dev = p;

	return 0;
}

s32 vdec_init(enum vformat_e vf)
{
	s32 r;

	if (inited_vcodec_num >= SUPPORT_VCODEC_NUM) {
		pr_info("We only support the one video code at each time\n");
		return -EIO;
	}

	inited_vcodec_num++;

	vdec_device =
		platform_device_register_data(&vdec_core_device->dev,
				vdec_device_name[vf], -1,
				&vdec_dev_reg, sizeof(vdec_dev_reg));

	if (IS_ERR(vdec_device)) {
		r = PTR_ERR(vdec_device);
		pr_info("vdec: Decoder device register failed (%d)\n", r);
		inited_vcodec_num--;
		goto error;
	}

	return 0;

error:
	vdec_device = NULL;

	inited_vcodec_num--;

	return r;
}

s32 vdec_release(enum vformat_e vf)
{
	if (vdec_device)
		platform_device_unregister(vdec_device);

	inited_vcodec_num--;

	vdec_device = NULL;

	return 0;
}

#if 1				/* MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON8 */
void vdec_poweron(enum vdec_type_e core)
{
	ulong flags;

	spin_lock_irqsave(&lock, flags);

	if (core == VDEC_1) {
		/* vdec1 power on */
		WRITE_AOREG(AO_RTI_GEN_PWR_SLEEP0,
				READ_AOREG(AO_RTI_GEN_PWR_SLEEP0) & ~0xc);
		/* wait 10uS */
		udelay(10);
		/* vdec1 soft reset */
		WRITE_VREG(DOS_SW_RESET0, 0xfffffffc);
		WRITE_VREG(DOS_SW_RESET0, 0);
		/* enable vdec1 clock */
		/*add power on vdec clock level setting,only for m8 chip,
		   m8baby and m8m2 can dynamic adjust vdec clock,
		   power on with default clock level */
		if (poweron_clock_level == 1 && is_meson_m8_cpu())
			vdec_clock_hi_enable();
		else
			vdec_clock_enable();
		/* power up vdec memories */
		WRITE_VREG(DOS_MEM_PD_VDEC, 0);
		/* remove vdec1 isolation */
		WRITE_AOREG(AO_RTI_GEN_PWR_ISO0,
				READ_AOREG(AO_RTI_GEN_PWR_ISO0) & ~0xC0);
		/* reset DOS top registers */
		WRITE_VREG(DOS_VDEC_MCRCC_STALL_CTRL, 0);
	} else if (core == VDEC_2) {
		if (has_vdec2()) {
			/* vdec2 power on */
			WRITE_AOREG(AO_RTI_GEN_PWR_SLEEP0,
					READ_AOREG(AO_RTI_GEN_PWR_SLEEP0) &
					~0x30);
			/* wait 10uS */
			udelay(10);
			/* vdec2 soft reset */
			WRITE_VREG(DOS_SW_RESET2, 0xffffffff);
			WRITE_VREG(DOS_SW_RESET2, 0);
			/* enable vdec1 clock */
			vdec2_clock_enable();
			/* power up vdec memories */
			WRITE_VREG(DOS_MEM_PD_VDEC2, 0);
			/* remove vdec2 isolation */
			WRITE_AOREG(AO_RTI_GEN_PWR_ISO0,
					READ_AOREG(AO_RTI_GEN_PWR_ISO0) &
					~0x300);
			/* reset DOS top registers */
			WRITE_VREG(DOS_VDEC2_MCRCC_STALL_CTRL, 0);
		}
	} else if (core == VDEC_HCODEC) {
		if (has_hdec()) {
			/* hcodec power on */
			WRITE_AOREG(AO_RTI_GEN_PWR_SLEEP0,
					READ_AOREG(AO_RTI_GEN_PWR_SLEEP0) &
					~0x3);
			/* wait 10uS */
			udelay(10);
			/* hcodec soft reset */
			WRITE_VREG(DOS_SW_RESET1, 0xffffffff);
			WRITE_VREG(DOS_SW_RESET1, 0);
			/* enable hcodec clock */
			hcodec_clock_enable();
			/* power up hcodec memories */
			WRITE_VREG(DOS_MEM_PD_HCODEC, 0);
			/* remove hcodec isolation */
			WRITE_AOREG(AO_RTI_GEN_PWR_ISO0,
					READ_AOREG(AO_RTI_GEN_PWR_ISO0) &
					~0x30);
		}
	} else if (core == VDEC_HEVC) {
		if (has_hevc_vdec()) {
			/* hevc power on */
			WRITE_AOREG(AO_RTI_GEN_PWR_SLEEP0,
					READ_AOREG(AO_RTI_GEN_PWR_SLEEP0) &
					~0xc0);
			/* wait 10uS */
			udelay(10);
			/* hevc soft reset */
			WRITE_VREG(DOS_SW_RESET3, 0xffffffff);
			WRITE_VREG(DOS_SW_RESET3, 0);
			/* enable hevc clock */
			hevc_clock_enable();
			/* power up hevc memories */
			WRITE_VREG(DOS_MEM_PD_HEVC, 0);
			/* remove hevc isolation */
			WRITE_AOREG(AO_RTI_GEN_PWR_ISO0,
					READ_AOREG(AO_RTI_GEN_PWR_ISO0) &
					~0xc00);
		}
	}

	spin_unlock_irqrestore(&lock, flags);

}

void vdec_poweroff(enum vdec_type_e core)
{
	ulong flags;

	spin_lock_irqsave(&lock, flags);

	if (core == VDEC_1) {
		/* enable vdec1 isolation */
		WRITE_AOREG(AO_RTI_GEN_PWR_ISO0,
				READ_AOREG(AO_RTI_GEN_PWR_ISO0) | 0xc0);
		/* power off vdec1 memories */
		WRITE_VREG(DOS_MEM_PD_VDEC, 0xffffffffUL);
		/* disable vdec1 clock */
		vdec_clock_off();
		/* vdec1 power off */
		WRITE_AOREG(AO_RTI_GEN_PWR_SLEEP0,
				READ_AOREG(AO_RTI_GEN_PWR_SLEEP0) | 0xc);
	} else if (core == VDEC_2) {
		if (has_vdec2()) {
			/* enable vdec2 isolation */
			WRITE_AOREG(AO_RTI_GEN_PWR_ISO0,
					READ_AOREG(AO_RTI_GEN_PWR_ISO0) |
					0x300);
			/* power off vdec2 memories */
			WRITE_VREG(DOS_MEM_PD_VDEC2, 0xffffffffUL);
			/* disable vdec2 clock */
			vdec2_clock_off();
			/* vdec2 power off */
			WRITE_AOREG(AO_RTI_GEN_PWR_SLEEP0,
					READ_AOREG(AO_RTI_GEN_PWR_SLEEP0) |
					0x30);
		}
	} else if (core == VDEC_HCODEC) {
		if (has_hdec()) {
			/* enable hcodec isolation */
			WRITE_AOREG(AO_RTI_GEN_PWR_ISO0,
					READ_AOREG(AO_RTI_GEN_PWR_ISO0) |
					0x30);
			/* power off hcodec memories */
			WRITE_VREG(DOS_MEM_PD_HCODEC, 0xffffffffUL);
			/* disable hcodec clock */
			hcodec_clock_off();
			/* hcodec power off */
			WRITE_AOREG(AO_RTI_GEN_PWR_SLEEP0,
					READ_AOREG(AO_RTI_GEN_PWR_SLEEP0) | 3);
		}
	} else if (core == VDEC_HEVC) {
		if (has_hevc_vdec()) {
			/* enable hevc isolation */
			WRITE_AOREG(AO_RTI_GEN_PWR_ISO0,
					READ_AOREG(AO_RTI_GEN_PWR_ISO0) |
					0xc00);
			/* power off hevc memories */
			WRITE_VREG(DOS_MEM_PD_HEVC, 0xffffffffUL);
			/* disable hevc clock */
			hevc_clock_off();
			/* hevc power off */
			WRITE_AOREG(AO_RTI_GEN_PWR_SLEEP0,
					READ_AOREG(AO_RTI_GEN_PWR_SLEEP0) |
					0xc0);
		}
	}

	spin_unlock_irqrestore(&lock, flags);

}

bool vdec_on(enum vdec_type_e core)
{
	bool ret = false;

	if (core == VDEC_1) {
		if (((READ_AOREG(AO_RTI_GEN_PWR_SLEEP0) & 0xc) == 0) &&
			(READ_MPEG_REG(HHI_VDEC_CLK_CNTL) & 0x100))
			ret = true;
	} else if (core == VDEC_2) {
		if (has_vdec2()) {
			if (((READ_AOREG(AO_RTI_GEN_PWR_SLEEP0) & 0x30) == 0) &&
				(READ_MPEG_REG(HHI_VDEC2_CLK_CNTL) & 0x100))
				ret = true;
		}
	} else if (core == VDEC_HCODEC) {
		if (has_hdec()) {
			if (((READ_AOREG(AO_RTI_GEN_PWR_SLEEP0) & 0x3) == 0) &&
				(READ_MPEG_REG(HHI_VDEC_CLK_CNTL) & 0x1000000))
				ret = true;
		}
	} else if (core == VDEC_HEVC) {
		if (has_hevc_vdec()) {
			if (((READ_AOREG(AO_RTI_GEN_PWR_SLEEP0) & 0xc0) == 0) &&
				(READ_MPEG_REG(HHI_VDEC2_CLK_CNTL) & 0x1000000))
				ret = true;
		}
	}

	return ret;
}

#elif 0				/* MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON6TVD */
void vdec_poweron(enum vdec_type_e core)
{
	ulong flags;

	spin_lock_irqsave(&lock, flags);

	if (core == VDEC_1) {
		/* vdec1 soft reset */
		WRITE_VREG(DOS_SW_RESET0, 0xfffffffc);
		WRITE_VREG(DOS_SW_RESET0, 0);
		/* enable vdec1 clock */
		vdec_clock_enable();
		/* reset DOS top registers */
		WRITE_VREG(DOS_VDEC_MCRCC_STALL_CTRL, 0);
	} else if (core == VDEC_2) {
		/* vdec2 soft reset */
		WRITE_VREG(DOS_SW_RESET2, 0xffffffff);
		WRITE_VREG(DOS_SW_RESET2, 0);
		/* enable vdec2 clock */
		vdec2_clock_enable();
		/* reset DOS top registers */
		WRITE_VREG(DOS_VDEC2_MCRCC_STALL_CTRL, 0);
	} else if (core == VDEC_HCODEC) {
		/* hcodec soft reset */
		WRITE_VREG(DOS_SW_RESET1, 0xffffffff);
		WRITE_VREG(DOS_SW_RESET1, 0);
		/* enable hcodec clock */
		hcodec_clock_enable();
	}

	spin_unlock_irqrestore(&lock, flags);
}

void vdec_poweroff(enum vdec_type_e core)
{
	ulong flags;

	spin_lock_irqsave(&lock, flags);

	if (core == VDEC_1) {
		/* disable vdec1 clock */
		vdec_clock_off();
	} else if (core == VDEC_2) {
		/* disable vdec2 clock */
		vdec2_clock_off();
	} else if (core == VDEC_HCODEC) {
		/* disable hcodec clock */
		hcodec_clock_off();
	}

	spin_unlock_irqrestore(&lock, flags);
}

bool vdec_on(enum vdec_type_e core)
{
	bool ret = false;

	if (core == VDEC_1) {
		if (READ_MPEG_REG(HHI_VDEC_CLK_CNTL) & 0x100)
			ret = true;
	} else if (core == VDEC_2) {
		if (READ_MPEG_REG(HHI_VDEC2_CLK_CNTL) & 0x100)
			ret = true;
	} else if (core == VDEC_HCODEC) {
		if (READ_MPEG_REG(HHI_VDEC_CLK_CNTL) & 0x1000000)
			ret = true;
	}

	return ret;
}
#endif

void vdec_power_mode(int level)
{
	/* todo: add level routines for clock adjustment per chips */
	ulong flags;
	ulong fiq_flag;

	if (vdec_clock_level(VDEC_1) == level)
		return;

	spin_lock_irqsave(&lock, flags);
	raw_local_save_flags(fiq_flag);
	local_fiq_disable();

	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_M8B)
		vdec_clock_prepare_switch();

	if (level == 0)
		vdec_clock_enable();
	else
		vdec_clock_hi_enable();

	raw_local_irq_restore(fiq_flag);
	spin_unlock_irqrestore(&lock, flags);
}

void vdec2_power_mode(int level)
{
	if (has_vdec2()) {
		/* todo: add level routines for clock adjustment per chips */
		ulong flags;
		ulong fiq_flag;

		if (vdec_clock_level(VDEC_2) == level)
			return;

		spin_lock_irqsave(&lock, flags);
		raw_local_save_flags(fiq_flag);
		local_fiq_disable();

		if (get_cpu_type() >= MESON_CPU_MAJOR_ID_M8B)
			vdec_clock_prepare_switch();

		if (level == 0)
			vdec2_clock_enable();
		else
			vdec2_clock_hi_enable();

		raw_local_irq_restore(fiq_flag);
		spin_unlock_irqrestore(&lock, flags);
	}
}

static enum vdec2_usage_e vdec2_usage = USAGE_NONE;
void set_vdec2_usage(enum vdec2_usage_e usage)
{
	if (has_vdec2()) {
		ulong flags;
		spin_lock_irqsave(&lock, flags);
		vdec2_usage = usage;
		spin_unlock_irqrestore(&lock, flags);
	}
}

enum vdec2_usage_e get_vdec2_usage(void)
{
	if (has_vdec2())
		return vdec2_usage;
	else
		return 0;
}

static struct am_reg am_risc[] = {
	{"MSP", 0x300},
	{"MPSR", 0x301},
	{"MCPU_INT_BASE", 0x302},
	{"MCPU_INTR_GRP", 0x303},
	{"MCPU_INTR_MSK", 0x304},
	{"MCPU_INTR_REQ", 0x305},
	{"MPC-P", 0x306},
	{"MPC-D", 0x307},
	{"MPC_E", 0x308},
	{"MPC_W", 0x309}
};

static ssize_t amrisc_regs_show(struct class *class,
		struct class_attribute *attr, char *buf)
{
	char *pbuf = buf;
	struct am_reg *regs = am_risc;
	int rsize = sizeof(am_risc) / sizeof(struct am_reg);
	int i;
	unsigned val;
	unsigned long flags = 0;
	ssize_t ret;

	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_M8) {
		spin_lock_irqsave(&lock, flags);
		if (!vdec_on(VDEC_1)) {
			spin_unlock_irqrestore(&lock, flags);
			pbuf += sprintf(pbuf, "amrisc is power off\n");
			ret = pbuf - buf;
			return ret;
		}
	} else if (get_cpu_type() >= MESON_CPU_MAJOR_ID_M6) {
		/*TODO:M6 define */
		/*
		   switch_mod_gate_by_type(MOD_VDEC, 1);
		 */
		amports_switch_gate("vdec", 1);
	}
	pbuf += sprintf(pbuf, "amrisc registers show:\n");
	for (i = 0; i < rsize; i++) {
		val = READ_VREG(regs[i].offset);
		pbuf += sprintf(pbuf, "%s(%#x)\t:%#x(%d)\n",
				regs[i].name, regs[i].offset, val, val);
	}
	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_M8)
		spin_unlock_irqrestore(&lock, flags);
	else if (get_cpu_type() >= MESON_CPU_MAJOR_ID_M6) {
		/*TODO:M6 define */
		/*
		   switch_mod_gate_by_type(MOD_VDEC, 0);
		 */
		amports_switch_gate("vdec", 0);
	}
	ret = pbuf - buf;
	return ret;
}

static ssize_t dump_trace_show(struct class *class,
		struct class_attribute *attr, char *buf)
{
	int i;
	char *pbuf = buf;
	unsigned long flags = 0;
	ssize_t ret;
	u16 *trace_buf = kmalloc(debug_trace_num * 2, GFP_KERNEL);
	if (!trace_buf) {
		pbuf += sprintf(pbuf, "No Memory bug\n");
		ret = pbuf - buf;
		return ret;
	}
	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_M8) {
		spin_lock_irqsave(&lock, flags);
		if (!vdec_on(VDEC_1)) {
			spin_unlock_irqrestore(&lock, flags);
			kfree(trace_buf);
			pbuf += sprintf(pbuf, "amrisc is power off\n");
			ret = pbuf - buf;
			return ret;
		}
	} else if (get_cpu_type() >= MESON_CPU_MAJOR_ID_M6) {
		/*TODO:M6 define */
		/*
		   switch_mod_gate_by_type(MOD_VDEC, 1);
		 */
		amports_switch_gate("vdec", 1);
	}
	pr_info("dump trace steps:%d start\n", debug_trace_num);
	i = 0;
	while (i <= debug_trace_num - 16) {
		trace_buf[i] = READ_VREG(MPC_E);
		trace_buf[i + 1] = READ_VREG(MPC_E);
		trace_buf[i + 2] = READ_VREG(MPC_E);
		trace_buf[i + 3] = READ_VREG(MPC_E);
		trace_buf[i + 4] = READ_VREG(MPC_E);
		trace_buf[i + 5] = READ_VREG(MPC_E);
		trace_buf[i + 6] = READ_VREG(MPC_E);
		trace_buf[i + 7] = READ_VREG(MPC_E);
		trace_buf[i + 8] = READ_VREG(MPC_E);
		trace_buf[i + 9] = READ_VREG(MPC_E);
		trace_buf[i + 10] = READ_VREG(MPC_E);
		trace_buf[i + 11] = READ_VREG(MPC_E);
		trace_buf[i + 12] = READ_VREG(MPC_E);
		trace_buf[i + 13] = READ_VREG(MPC_E);
		trace_buf[i + 14] = READ_VREG(MPC_E);
		trace_buf[i + 15] = READ_VREG(MPC_E);
		i += 16;
	};
	pr_info("dump trace steps:%d finished\n", debug_trace_num);
	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_M8)
		spin_unlock_irqrestore(&lock, flags);
	else if (get_cpu_type() >= MESON_CPU_MAJOR_ID_M6) {
		/*TODO:M6 define */
		/*
		   switch_mod_gate_by_type(MOD_VDEC, 0);
		 */
		amports_switch_gate("vdec", 0);
	}
	for (i = 0; i < debug_trace_num; i++) {
		if (i % 4 == 0) {
			if (i % 16 == 0)
				pbuf += sprintf(pbuf, "\n");
			else if (i % 8 == 0)
				pbuf += sprintf(pbuf, "  ");
			else	/* 4 */
				pbuf += sprintf(pbuf, " ");
		}
		pbuf += sprintf(pbuf, "%04x:", trace_buf[i]);
	}
	while (i < debug_trace_num)
		;
	kfree(trace_buf);
	pbuf += sprintf(pbuf, "\n");
	ret = pbuf - buf;
	return ret;
}

static ssize_t clock_level_show(struct class *class,
		struct class_attribute *attr, char *buf)
{
	char *pbuf = buf;
	size_t ret;
	pbuf += sprintf(pbuf, "%d\n", vdec_clock_level(VDEC_1));

	if (has_vdec2())
		pbuf += sprintf(pbuf, "%d\n", vdec_clock_level(VDEC_2));

	if (has_hevc_vdec())
		pbuf += sprintf(pbuf, "%d\n", vdec_clock_level(VDEC_HEVC));

	ret = pbuf - buf;
	return ret;
}

static ssize_t store_poweron_clock_level(struct class *class,
		struct class_attribute *attr,
		const char *buf, size_t size)
{
	unsigned val;
	ssize_t ret;

	ret = sscanf(buf, "%d", &val);
	if (ret != 1)
		return -EINVAL;
	poweron_clock_level = val;
	return size;
}

static ssize_t show_poweron_clock_level(struct class *class,
		struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", poweron_clock_level);
}

/*irq num as same as .dts*/
/*
	interrupts = <0 3 1
		0 23 1
		0 32 1
		0 43 1
		0 44 1
		0 45 1>;
	interrupt-names = "vsync",
		"demux",
		"parser",
		"mailbox_0",
		"mailbox_1",
		"mailbox_2";
*/
s32 vdec_request_irq(enum vdec_irq_num num, irq_handler_t handler,
				const char *devname, void *dev)
{
	s32 res_irq;
	s32 ret = 0;
	if (num >= VDEC_IRQ_MAX) {
		pr_err("[%s] request irq error, irq num too big!", __func__);
		return -EINVAL;
	}
	res_irq = platform_get_irq(vdec_core_device, num);
	if (res_irq < 0) {
		pr_err("[%s] get irq error!", __func__);
		return -EINVAL;
	}
	vdec_irq[num] = res_irq;
	ret = request_irq(vdec_irq[num], handler,
	IRQF_SHARED, devname, dev);
	return ret;
}

void vdec_free_irq(enum vdec_irq_num num, void *dev)
{
	if (num >= VDEC_IRQ_MAX) {
		pr_err("[%s] request irq error, irq num too big!", __func__);
		return;
	}
	free_irq(vdec_irq[num], dev);
}

static struct class_attribute vdec_class_attrs[] = {
	__ATTR_RO(amrisc_regs),
	__ATTR_RO(dump_trace),
	__ATTR_RO(clock_level),
	__ATTR(poweron_clock_level, S_IRUGO | S_IWUSR | S_IWGRP,
	show_poweron_clock_level, store_poweron_clock_level),
	__ATTR_NULL
};

static struct class vdec_class = {
		.name = "vdec",
		.class_attrs = vdec_class_attrs,
	};

static int vdec_probe(struct platform_device *pdev)
{
	s32 r;

	r = class_register(&vdec_class);
	if (r) {
		pr_info("vdec class create fail.\n");
		return r;
	}

	vdec_core_device = pdev;

	r = of_reserved_mem_device_init(&pdev->dev);
	if (r == 0)
		pr_info("vdec_probe done\n");
	if (get_cpu_type() < MESON_CPU_MAJOR_ID_M8) {
		/* default to 250MHz */
		vdec_clock_hi_enable();
	}
	return 0;

	return r;
}

static int vdec_remove(struct platform_device *pdev)
{
	class_unregister(&vdec_class);

	return 0;
}

#ifdef CONFIG_USE_OF
static const struct of_device_id amlogic_vdec_dt_match[] = {
	{
		.compatible = "amlogic, vdec",
	},
	{},
};
#else
#define amlogic_vdec_dt_match NULL
#define amlogic_vdec_cma_dt_match NULL
#endif

static struct platform_driver vdec_driver = {
	.probe = vdec_probe,
	.remove = vdec_remove,
	.driver = {
		.name = "vdec",
		.of_match_table = amlogic_vdec_dt_match,
	}
};

static int __init vdec_module_init(void)
{
	if (platform_driver_register(&vdec_driver)) {
		pr_info("failed to register vdec module\n");
		return -ENODEV;
	}

	return 0;
}

static void __exit vdec_module_exit(void)
{
	platform_driver_unregister(&vdec_driver);
	return;
}

static int vdec_mem_device_init(struct reserved_mem *rmem, struct device *dev)
{
	unsigned long start, end;
	start = rmem->base;
	end = rmem->base + rmem->size - 1;
	pr_info("init vdec memsource %lx->%lx\n", start, end);

	dev_set_cma_area(dev, vdec_cma);
	vdec_set_resource(start, end, dev);

	return 0;
}

static const struct reserved_mem_ops rmem_vdec_ops = {
	.device_init = vdec_mem_device_init,
};

static int __init vdec_mem_setup(struct reserved_mem *rmem)
{
	rmem->ops = &rmem_vdec_ops;
	pr_info("vdec: reserved mem setup\n");

	return 0;
}

#ifdef CONFIG_CMA
static int __init vdec_cma_setup(struct reserved_mem *rmem)
{
	int ret;
	phys_addr_t base, size, align;

	align = (phys_addr_t)PAGE_SIZE << max(MAX_ORDER - 1, pageblock_order);
	base = ALIGN(rmem->base, align);
	size = round_down(rmem->size - (base - rmem->base), align);

	ret = cma_init_reserved_mem(base, size, 0, &vdec_cma);
	if (ret) {
		pr_info("vdec: cma init reserve area failed.\n");
		return ret;
	}

#ifndef CONFIG_ARM64
	dma_contiguous_early_fixup(base, size);
#endif

	pr_info("vdec: cma setup\n");

	return 0;
}

RESERVEDMEM_OF_DECLARE(vdec_cma, "amlogic, vdec-cma-memory", vdec_cma_setup);
#endif

RESERVEDMEM_OF_DECLARE(vdec, "amlogic, vdec-memory", vdec_mem_setup);

module_param(debug_trace_num, uint, 0664);

module_init(vdec_module_init);
module_exit(vdec_module_exit);

MODULE_DESCRIPTION("AMLOGIC vdec driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tim Yao <timyao@amlogic.com>");
