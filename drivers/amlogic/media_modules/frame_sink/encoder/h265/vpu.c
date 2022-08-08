/*
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Description:
 * linux device driver for VPU.
 */
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/module.h>
#include <linux/dma-mapping.h>
#include <linux/wait.h>
#include <linux/list.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/reset.h>
#include <linux/clk.h>
#include <linux/compat.h>
#include <linux/of_reserved_mem.h>
#include <linux/of_address.h>
#include <linux/compat.h>
#include <linux/amlogic/media/codec_mm/codec_mm.h>

#include <linux/amlogic/media/registers/cpu_version.h>
#include <linux/version.h>
#include "../../../frame_provider/decoder/utils/vdec_power_ctrl.h"
#include <linux/amlogic/media/utils/vdec_reg.h>
#include <linux/amlogic/power_ctrl.h>
#include <dt-bindings/power/sc2-pd.h>
#include <linux/amlogic/power_domain.h>
#include <linux/amlogic/power_ctrl.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,11,1)
#include <linux/sched/signal.h>
#endif

#include <linux/amlogic/media/utils/vdec_reg.h>
#include "../../../common/media_clock/switch/amports_gate.h"

#include "vpu.h"
#include "vmm.h"

/* definitions to be changed as customer  configuration */
/* if you want to have clock gating scheme frame by frame */
/* #define VPU_SUPPORT_CLOCK_CONTROL */

//#define VPU_SUPPORT_CLOCK_CONTROL


#define VPU_PLATFORM_DEVICE_NAME "HevcEnc"
#define VPU_DEV_NAME "HevcEnc"
#define VPU_CLASS_NAME "HevcEnc"

#ifndef VM_RESERVED	/*for kernel up to 3.7.0 version*/
#define VM_RESERVED (VM_DONTEXPAND | VM_DONTDUMP)
#endif

#define MHz (1000000)

#define VPU_INIT_VIDEO_MEMORY_SIZE_IN_BYTE (64 * SZ_1M)

#define LOG_ALL 0
#define LOG_INFO 1
#define LOG_DEBUG 2
#define LOG_ERROR 3

#define enc_pr(level, x...) \
	do { \
		if (level >= print_level) \
			printk(x); \
	} while (0)

static s32 print_level = LOG_DEBUG;
static s32 clock_level = 4;

static s32 wave_clocka;
static s32 wave_clockb;
static s32 wave_clockc;

static struct video_mm_t s_vmem;
static struct vpudrv_buffer_t s_video_memory = {0};
static bool use_reserve;
static ulong cma_pool_size;

/* end customer definition */
static struct vpudrv_buffer_t s_instance_pool = {0};
static struct vpudrv_buffer_t s_common_memory = {0};
static struct vpu_drv_context_t s_vpu_drv_context;
static s32 s_vpu_major;
static struct device *hevcenc_dev;

static s32 s_vpu_open_ref_count;
static s32 s_vpu_irq;
static bool s_vpu_irq_requested;

static struct vpudrv_buffer_t s_vpu_register = {0};

static s32 s_interrupt_flag;
static wait_queue_head_t s_interrupt_wait_q;

static spinlock_t s_vpu_lock = __SPIN_LOCK_UNLOCKED(s_vpu_lock);
static DEFINE_SEMAPHORE(s_vpu_sem);
static struct list_head s_vbp_head = LIST_HEAD_INIT(s_vbp_head);
static struct list_head s_inst_list_head = LIST_HEAD_INIT(s_inst_list_head);
static struct tasklet_struct hevc_tasklet;
static struct platform_device *hevc_pdev;

static struct vpu_bit_firmware_info_t s_bit_firmware_info[MAX_NUM_VPU_CORE];

static struct vpu_dma_cfg dma_cfg[3];

struct vpu_clks {
	struct clk *wave_aclk;
	struct clk *wave_bclk;
	struct clk *wave_cclk;
};

static struct vpu_clks s_vpu_clks;

#define CHECK_RET(_ret) if (ret) {enc_pr(LOG_ERROR, \
		"%s:%d:function call failed with result: %d\n",\
		__FUNCTION__, __LINE__, _ret);}

static u32 vpu_src_addr_config(struct vpu_dma_buf_info_t);
static void vpu_dma_buffer_unmap(struct vpu_dma_cfg *cfg);

static void dma_flush(u32 buf_start, u32 buf_size)
{
	if (hevc_pdev)
		dma_sync_single_for_device(
			&hevc_pdev->dev, buf_start,
			buf_size, DMA_TO_DEVICE);
}

static void cache_flush(u32 buf_start, u32 buf_size)
{
	if (hevc_pdev)
		dma_sync_single_for_cpu(
			&hevc_pdev->dev, buf_start,
			buf_size, DMA_FROM_DEVICE);
}

s32 vpu_hw_reset(void)
{
	enc_pr(LOG_DEBUG, "request vpu reset from application.\n");
	return 0;
}

s32 vpu_clk_prepare(struct device *dev, struct vpu_clks *clks)
{
	int ret;

	s32 new_clocka = 667;
	s32 new_clockb = 400;
	s32 new_clockc = 400;

	if (wave_clocka > 0)
		new_clocka = wave_clocka;
	if (wave_clockb > 0)
		new_clockb = wave_clockb;
	if (wave_clockc > 0)
		new_clockc = wave_clockc;

	clks->wave_aclk = devm_clk_get(dev, "cts_wave420_aclk");
	if (IS_ERR_OR_NULL(clks->wave_aclk)) {
		enc_pr(LOG_ERROR, "failed to get wave aclk\n");
		return -1;
	}

	clks->wave_bclk = devm_clk_get(dev, "cts_wave420_bclk");
	if (IS_ERR_OR_NULL(clks->wave_aclk)) {
		enc_pr(LOG_ERROR, "failed to get wave aclk\n");
		return -1;
	}

	clks->wave_cclk = devm_clk_get(dev, "cts_wave420_cclk");
	if (IS_ERR_OR_NULL(clks->wave_aclk)) {
		enc_pr(LOG_ERROR, "failed to get wave aclk\n");
		return -1;
	}

	ret = clk_set_rate(clks->wave_aclk, new_clocka * MHz);
	CHECK_RET(ret);
	ret = clk_set_rate(clks->wave_bclk, new_clockb * MHz);
	CHECK_RET(ret);
	ret = clk_set_rate(clks->wave_cclk, new_clockc * MHz);

	CHECK_RET(ret);
	ret = clk_prepare(clks->wave_aclk);
	CHECK_RET(ret);
	ret = clk_prepare(clks->wave_bclk);
	CHECK_RET(ret);
	ret = clk_prepare(clks->wave_cclk);
	CHECK_RET(ret);

	enc_pr(LOG_ERROR, "wave_clk_a: %lu MHz\n", clk_get_rate(clks->wave_aclk) / 1000000);
	enc_pr(LOG_ERROR, "wave_clk_b: %lu MHz\n", clk_get_rate(clks->wave_bclk) / 1000000);
	enc_pr(LOG_ERROR, "wave_clk_c: %lu MHz\n", clk_get_rate(clks->wave_cclk) / 1000000);

	return 0;
}

void vpu_clk_unprepare(struct device *dev, struct vpu_clks *clks)
{
	clk_unprepare(clks->wave_cclk);
	devm_clk_put(dev, clks->wave_cclk);

	clk_unprepare(clks->wave_bclk);
	devm_clk_put(dev, clks->wave_bclk);

	clk_unprepare(clks->wave_aclk);
	devm_clk_put(dev, clks->wave_aclk);
}

s32 vpu_clk_config(u32 enable)
{
	if (enable) {
		if (get_cpu_type() >= MESON_CPU_MAJOR_ID_SC2) {
			clk_enable(s_vpu_clks.wave_aclk);
			clk_enable(s_vpu_clks.wave_bclk);
			clk_enable(s_vpu_clks.wave_cclk);
		} else {
			if (get_cpu_type() >= MESON_CPU_MAJOR_ID_G12A)
				HevcEnc_MoreClock_enable();
			HevcEnc_clock_enable(clock_level);
		}
	} else {
		if (get_cpu_type() >= MESON_CPU_MAJOR_ID_SC2) {
			clk_disable(s_vpu_clks.wave_cclk);
			clk_disable(s_vpu_clks.wave_bclk);
			clk_disable(s_vpu_clks.wave_aclk);
		} else {
			HevcEnc_clock_disable();
			if (get_cpu_type() >= MESON_CPU_MAJOR_ID_G12A)
				HevcEnc_MoreClock_disable();
		}
	}

	return 0;
}

static s32 vpu_alloc_dma_buffer(struct vpudrv_buffer_t *vb)
{
	if (!vb)
		return -1;

	vb->phys_addr = (ulong)vmem_alloc(&s_vmem, vb->size, 0);
	if ((ulong)vb->phys_addr == (ulong)-1) {
		enc_pr(LOG_ERROR,
			"Physical memory allocation error size=%d\n", vb->size);
		return -1;
	}

	enc_pr(LOG_INFO, "vpu_alloc_dma_buffer: vb->phys_addr 0x%lx \n",vb->phys_addr);
	return 0;
}

static void vpu_free_dma_buffer(struct vpudrv_buffer_t *vb)
{
	if (!vb)
		return;
	enc_pr(LOG_INFO, "vpu_free_dma_buffer 0x%lx\n",vb->phys_addr);

	if (vb->phys_addr)
		vmem_free(&s_vmem, vb->phys_addr, 0);
}

static s32 vpu_free_instances(struct file *filp)
{
	struct vpudrv_instanace_list_t *vil, *n;
	struct vpudrv_instance_pool_t *vip;
	void *vip_base;

	enc_pr(LOG_DEBUG, "vpu_free_instances\n");

	list_for_each_entry_safe(vil, n, &s_inst_list_head, list) {
		if (vil->filp == filp) {
			vip_base = (void *)s_instance_pool.base;
			enc_pr(LOG_INFO,
				"free_instances instIdx=%d, coreIdx=%d, vip_base=%p\n",
				(s32)vil->inst_idx,
				(s32)vil->core_idx,
				vip_base);
			vip = (struct vpudrv_instance_pool_t *)vip_base;
			if (vip) {
				/* only first 4 byte is key point
				 *	(inUse of CodecInst in vpuapi)
				 *    to free the corresponding instance.
				 */
				memset(&vip->codecInstPool[vil->inst_idx],
					0x00, 4);
			}
			s_vpu_open_ref_count--;
			list_del(&vil->list);
			kfree(vil);
		}
	}
	return 1;
}

static s32 vpu_free_buffers(struct file *filp)
{
	struct vpudrv_buffer_pool_t *pool, *n;
	struct vpudrv_buffer_t vb;

	enc_pr(LOG_DEBUG, "vpu_free_buffers\n");

	list_for_each_entry_safe(pool, n, &s_vbp_head, list) {
		if (pool->filp == filp) {
			vb = pool->vb;
			if (vb.phys_addr) {
				vpu_free_dma_buffer(&vb);
				list_del(&pool->list);
				kfree(pool);
			}
		}
	}
	return 0;
}

static u32 vpu_is_buffer_cached(struct file *filp, ulong vm_pgoff)
{
	struct vpudrv_buffer_pool_t *pool, *n;
	struct vpudrv_buffer_t vb;
	bool find = false;
	u32 cached = 0;

	enc_pr(LOG_ALL, "[+]vpu_is_buffer_cached\n");
	spin_lock(&s_vpu_lock);
	list_for_each_entry_safe(pool, n, &s_vbp_head, list) {
		if (pool->filp == filp) {
			vb = pool->vb;
			if (((vb.phys_addr  >> PAGE_SHIFT) == vm_pgoff)
				&& find == false){
				cached = vb.cached;
				find = true;
			}
		}
	}
	spin_unlock(&s_vpu_lock);
	enc_pr(LOG_ALL, "[-]vpu_is_buffer_cached, ret:%d\n", cached);
	return cached;
}

static void hevcenc_isr_tasklet(ulong data)
{
	struct vpu_drv_context_t *dev = (struct vpu_drv_context_t *)data;

	enc_pr(LOG_INFO, "hevcenc_isr_tasklet  interruput:0x%08lx\n",
		dev->interrupt_reason);
	if (dev->interrupt_reason) {
		/* notify the interrupt to user space */
		if (dev->async_queue) {
			enc_pr(LOG_ALL, "kill_fasync e %s\n", __func__);
			kill_fasync(&dev->async_queue, SIGIO, POLL_IN);
		}
		s_interrupt_flag = 1;
		wake_up_interruptible(&s_interrupt_wait_q);
	}
	enc_pr(LOG_ALL, "[-]%s\n", __func__);
}

static irqreturn_t vpu_irq_handler(s32 irq, void *dev_id)
{
	struct vpu_drv_context_t *dev = (struct vpu_drv_context_t *)dev_id;
	/* this can be removed.
	 *	it also work in VPU_WaitInterrupt of API function
	 */
	u32 core;
	ulong interrupt_reason = 0;

	enc_pr(LOG_ALL, "[+]%s\n", __func__);

	for (core = 0; core < MAX_NUM_VPU_CORE; core++) {
		if (s_bit_firmware_info[core].size == 0) {
			/* it means that we didn't get an information
			 *	the current core from API layer.
			 *	No core activated.
			 */
			enc_pr(LOG_ERROR,
				"s_bit_firmware_info[core].size is zero\n");
			continue;
		}
		if (ReadVpuRegister(W4_VPU_VPU_INT_STS)) {
			interrupt_reason = ReadVpuRegister(W4_VPU_INT_REASON);
			WriteVpuRegister(W4_VPU_INT_REASON_CLEAR,
				interrupt_reason);
			WriteVpuRegister(W4_VPU_VINT_CLEAR, 0x1);
			dev->interrupt_reason |= interrupt_reason;
		}
		enc_pr(LOG_INFO,
			"intr_reason: 0x%08lx\n", dev->interrupt_reason);
	}
	if (dev->interrupt_reason)
		tasklet_schedule(&hevc_tasklet);
	enc_pr(LOG_ALL, "[-]%s\n", __func__);
	return IRQ_HANDLED;
}

static s32 vpu_open(struct inode *inode, struct file *filp)
{
	bool alloc_buffer = false;
	s32 r = 0;

	enc_pr(LOG_DEBUG, "[+] %s, open_count=%d\n", __func__,
			s_vpu_drv_context.open_count);
	enc_pr(LOG_DEBUG, "vpu_open, calling process: %d:%s\n", current->pid, current->comm);
	spin_lock(&s_vpu_lock);
	s_vpu_drv_context.open_count++;
	if (s_vpu_drv_context.open_count == 1) {
		alloc_buffer = true;
	} else {
		r = -EBUSY;
		enc_pr(LOG_ERROR, "vpu_open, device is busy, s_vpu_drv_context.open_count=%d\n",
				s_vpu_drv_context.open_count);
		s_vpu_drv_context.open_count--;
		spin_unlock(&s_vpu_lock);
		return r;
	}
	filp->private_data = (void *)(&s_vpu_drv_context);
	spin_unlock(&s_vpu_lock);
	if (alloc_buffer && !use_reserve) {
#ifdef CONFIG_CMA
		s_video_memory.size = VPU_INIT_VIDEO_MEMORY_SIZE_IN_BYTE;
		s_video_memory.phys_addr =
			(ulong)codec_mm_alloc_for_dma(VPU_DEV_NAME,
			VPU_INIT_VIDEO_MEMORY_SIZE_IN_BYTE >> PAGE_SHIFT, 0, 0);
		if (s_video_memory.phys_addr) {
			enc_pr(LOG_DEBUG,
				"allocating phys 0x%lx, virt addr 0x%lx, size %dk\n",
				s_video_memory.phys_addr,
				s_video_memory.base,
				s_video_memory.size >> 10);
			if (vmem_init(&s_vmem,
				s_video_memory.phys_addr,
				s_video_memory.size) < 0) {
				enc_pr(LOG_ERROR, "fail to init vmem system\n");
				r = -ENOMEM;
				codec_mm_free_for_dma(
					VPU_DEV_NAME,
					(u32)s_video_memory.phys_addr);
				vmem_exit(&s_vmem);
				memset(&s_video_memory, 0,
					sizeof(struct vpudrv_buffer_t));
				memset(&s_vmem, 0,
					sizeof(struct video_mm_t));
			}
		} else {
			enc_pr(LOG_ERROR,
				"CMA failed to allocate dma buffer for %s, phys: 0x%lx\n",
				VPU_DEV_NAME, s_video_memory.phys_addr);
			if (s_video_memory.phys_addr)
				codec_mm_free_for_dma(
					VPU_DEV_NAME,
					(u32)s_video_memory.phys_addr);
			s_video_memory.phys_addr = 0;
			r = -ENOMEM;
		}
#else
		enc_pr(LOG_ERROR,
			"No CMA and reserved memory for HevcEnc!!!\n");
		r = -ENOMEM;
#endif
	} else if (!s_video_memory.phys_addr) {
		enc_pr(LOG_ERROR,
			"HevcEnc memory is not malloced!!!\n");
		r = -ENOMEM;
	}
	if (alloc_buffer) {
		ulong flags;
		u32 data32;

		if ((s_vpu_irq >= 0) && (s_vpu_irq_requested == false)) {
			s32 err;

			err = request_irq(s_vpu_irq, vpu_irq_handler, 0,
				"HevcEnc-irq", (void *)(&s_vpu_drv_context));
			if (err) {
				enc_pr(LOG_ERROR,
					"fail to register interrupt handler\n");
				spin_lock(&s_vpu_lock);
				s_vpu_drv_context.open_count--;
				spin_unlock(&s_vpu_lock);
				return -EFAULT;
			}
			s_vpu_irq_requested = true;
		}
		if (get_cpu_type() >= MESON_CPU_MAJOR_ID_SC2) {
		} else
		    amports_switch_gate("vdec", 1);

		spin_lock_irqsave(&s_vpu_lock, flags);

		if (get_cpu_type() >= MESON_CPU_MAJOR_ID_SC2) {
			//vpu_clk_config(1);
			pwr_ctrl_psci_smc(PDID_SC2_DOS_WAVE, PWR_ON);
		} else {
			WRITE_AOREG(AO_RTI_GEN_PWR_SLEEP0,
				READ_AOREG(AO_RTI_GEN_PWR_SLEEP0) &
				(get_cpu_type() == MESON_CPU_MAJOR_ID_SM1
				? ~0x8 : ~(0x3<<24)));
		}
		udelay(10);

		if (get_cpu_type() <= MESON_CPU_MAJOR_ID_TXLX) {
			data32 = 0x700;
			data32 |= READ_VREG(DOS_SW_RESET4);
			WRITE_VREG(DOS_SW_RESET4, data32);
			data32 &= ~0x700;
			WRITE_VREG(DOS_SW_RESET4, data32);
		} else {
			data32 = 0xf00;
			data32 |= READ_VREG(DOS_SW_RESET4);
			WRITE_VREG(DOS_SW_RESET4, data32);
			data32 &= ~0xf00;
			WRITE_VREG(DOS_SW_RESET4, data32);
		}

		if (get_cpu_type() >= MESON_CPU_MAJOR_ID_SC2) {
			pr_err("consider using reset control\n");
		} else {
			WRITE_MPEG_REG(RESET0_REGISTER, data32 & ~(1<<21));
			WRITE_MPEG_REG(RESET0_REGISTER, data32 | (1<<21));
			READ_MPEG_REG(RESET0_REGISTER);
			READ_MPEG_REG(RESET0_REGISTER);
			READ_MPEG_REG(RESET0_REGISTER);
			READ_MPEG_REG(RESET0_REGISTER);
		}

#ifndef VPU_SUPPORT_CLOCK_CONTROL
		vpu_clk_config(1);
#endif
		/* Enable wave420l_vpu_idle_rise_irq,
		 *	Disable wave420l_vpu_idle_fall_irq
		 */
		WRITE_VREG(DOS_WAVE420L_CNTL_STAT, 0x1);
		WRITE_VREG(DOS_MEM_PD_WAVE420L, 0x0);

		if (get_cpu_type() >= MESON_CPU_MAJOR_ID_SC2) {

		} else {
			WRITE_AOREG(AO_RTI_GEN_PWR_ISO0,
				READ_AOREG(AO_RTI_GEN_PWR_ISO0) &
				(get_cpu_type() == MESON_CPU_MAJOR_ID_SM1
				? ~0x8 : ~(0x3<<12)));
		}
		spin_unlock_irqrestore(&s_vpu_lock, flags);
	}
	memset(dma_cfg, 0, sizeof(dma_cfg));
	dma_cfg[0].fd = -1;
	dma_cfg[1].fd = -1;
	dma_cfg[2].fd = -1;

	if (r != 0) {
		spin_lock(&s_vpu_lock);
		enc_pr(LOG_DEBUG, "vpu_open, error handling, r=%d, s_vpu_drv_context.open_count=%d\n",
				r, s_vpu_drv_context.open_count);
		s_vpu_drv_context.open_count--;
		spin_unlock(&s_vpu_lock);
	}
	enc_pr(LOG_DEBUG, "[-] %s, ret: %d\n", __func__, r);
	return r;
}

static long vpu_ioctl(struct file *filp, u32 cmd, ulong arg)
{
	s32 ret = 0;
	struct vpu_drv_context_t *dev =
		(struct vpu_drv_context_t *)filp->private_data;

	switch (cmd) {
	case VDI_IOCTL_ALLOCATE_PHYSICAL_MEMORY:
		{
			struct vpudrv_buffer_pool_t *vbp;

			enc_pr(LOG_ALL,
				"[+]VDI_IOCTL_ALLOCATE_PHYSICAL_MEMORY\n");
			ret = down_interruptible(&s_vpu_sem);
			if (ret == 0) {
				vbp = kzalloc(sizeof(*vbp), GFP_KERNEL);
				if (!vbp) {
					up(&s_vpu_sem);
					return -ENOMEM;
				}

				ret = copy_from_user(&(vbp->vb),
					(struct vpudrv_buffer_t *)arg,
					sizeof(struct vpudrv_buffer_t));
				if (ret) {
					kfree(vbp);
					up(&s_vpu_sem);
					return -EFAULT;
				}

				ret = vpu_alloc_dma_buffer(&(vbp->vb));
				if (ret == -1) {
					ret = -ENOMEM;
					kfree(vbp);
					up(&s_vpu_sem);
					break;
				}
				ret = copy_to_user((void __user *)arg,
					&(vbp->vb),
					sizeof(struct vpudrv_buffer_t));
				if (ret) {
					kfree(vbp);
					ret = -EFAULT;
					up(&s_vpu_sem);
					break;
				}

				vbp->filp = filp;
				spin_lock(&s_vpu_lock);
				list_add(&vbp->list, &s_vbp_head);
				spin_unlock(&s_vpu_lock);

				up(&s_vpu_sem);
			}
			enc_pr(LOG_ALL,
				"[-]VDI_IOCTL_ALLOCATE_PHYSICAL_MEMORY\n");
		}
		break;
#ifdef CONFIG_COMPAT
	case VDI_IOCTL_ALLOCATE_PHYSICAL_MEMORY32:
		{
			struct vpudrv_buffer_pool_t *vbp;
			struct compat_vpudrv_buffer_t buf32;

			enc_pr(LOG_ALL,
				"[+]VDI_IOCTL_ALLOCATE_PHYSICAL_MEMORY32\n");
			memset(&buf32, 0, sizeof(struct compat_vpudrv_buffer_t));
			ret = down_interruptible(&s_vpu_sem);
			if (ret == 0) {
				vbp = kzalloc(sizeof(*vbp), GFP_KERNEL);
				if (!vbp) {
					up(&s_vpu_sem);
					return -ENOMEM;
				}

				ret = copy_from_user(&buf32,
					(struct compat_vpudrv_buffer_t *)arg,
					sizeof(struct compat_vpudrv_buffer_t));
				if (ret) {
					kfree(vbp);
					up(&s_vpu_sem);
					return -EFAULT;
				}

				vbp->vb.size = buf32.size;
				vbp->vb.cached = buf32.cached;
				vbp->vb.phys_addr =
					(ulong)buf32.phys_addr;
				vbp->vb.virt_addr =
					(ulong)buf32.virt_addr;
				ret = vpu_alloc_dma_buffer(&(vbp->vb));
				if (ret == -1) {
					ret = -ENOMEM;
					kfree(vbp);
					up(&s_vpu_sem);
					break;
				}

				buf32.size = vbp->vb.size;
				buf32.phys_addr =
					(compat_ulong_t)vbp->vb.phys_addr;
				buf32.virt_addr =
					(compat_ulong_t)vbp->vb.virt_addr;

				ret = copy_to_user((void __user *)arg,
					&buf32,
					sizeof(struct compat_vpudrv_buffer_t));
				if (ret) {
					kfree(vbp);
					ret = -EFAULT;
					up(&s_vpu_sem);
					break;
				}

				vbp->filp = filp;
				spin_lock(&s_vpu_lock);
				list_add(&vbp->list, &s_vbp_head);
				spin_unlock(&s_vpu_lock);

				up(&s_vpu_sem);
			}
			enc_pr(LOG_ALL,
				"[-]VDI_IOCTL_ALLOCATE_PHYSICAL_MEMORY32\n");
		}
		break;
#endif
	case VDI_IOCTL_FREE_PHYSICALMEMORY:
		{
			struct vpudrv_buffer_pool_t *vbp, *n;
			struct vpudrv_buffer_t vb;

			enc_pr(LOG_ALL,
				"[+]VDI_IOCTL_FREE_PHYSICALMEMORY\n");
			ret = down_interruptible(&s_vpu_sem);
			if (ret == 0) {
				ret = copy_from_user(&vb,
					(struct vpudrv_buffer_t *)arg,
					sizeof(struct vpudrv_buffer_t));
				if (ret) {
					up(&s_vpu_sem);
					return -EACCES;
				}

				if (vb.phys_addr)
					vpu_free_dma_buffer(&vb);

				spin_lock(&s_vpu_lock);
				list_for_each_entry_safe(vbp, n,
					&s_vbp_head, list) {
					if (vbp->vb.phys_addr == vb.phys_addr) {
						list_del(&vbp->list);
						kfree(vbp);
						break;
					}
				}
				spin_unlock(&s_vpu_lock);

				up(&s_vpu_sem);
			}
			enc_pr(LOG_ALL,
				"[-]VDI_IOCTL_FREE_PHYSICALMEMORY\n");
		}
		break;
#ifdef CONFIG_COMPAT
	case VDI_IOCTL_FREE_PHYSICALMEMORY32:
		{
			struct vpudrv_buffer_pool_t *vbp, *n;
			struct compat_vpudrv_buffer_t buf32;
			struct vpudrv_buffer_t vb;

			enc_pr(LOG_ALL,
				"[+]VDI_IOCTL_FREE_PHYSICALMEMORY32\n");
			ret = down_interruptible(&s_vpu_sem);
			if (ret == 0) {
				ret = copy_from_user(&buf32,
					(struct compat_vpudrv_buffer_t *)arg,
					sizeof(struct compat_vpudrv_buffer_t));
				if (ret) {
					up(&s_vpu_sem);
					return -EACCES;
				}

				vb.size = buf32.size;
				vb.phys_addr =
					(ulong)buf32.phys_addr;
				vb.virt_addr =
					(ulong)buf32.virt_addr;

				if (vb.phys_addr)
					vpu_free_dma_buffer(&vb);

				spin_lock(&s_vpu_lock);
				list_for_each_entry_safe(vbp, n,
					&s_vbp_head, list) {
					if ((compat_ulong_t)vbp->vb.base
						== buf32.base) {
						list_del(&vbp->list);
						kfree(vbp);
						break;
					}
				}
				spin_unlock(&s_vpu_lock);
				up(&s_vpu_sem);
			}
			enc_pr(LOG_ALL,
				"[-]VDI_IOCTL_FREE_PHYSICALMEMORY32\n");
		}
		break;
#endif
	case VDI_IOCTL_GET_RESERVED_VIDEO_MEMORY_INFO:
		{
			enc_pr(LOG_ALL,
				"[+]VDI_IOCTL_GET_RESERVED_VIDEO_MEMORY_INFO\n");
			if (s_video_memory.phys_addr != 0) {
				ret = copy_to_user((void __user *)arg,
					&s_video_memory,
					sizeof(struct vpudrv_buffer_t));
				if (ret != 0)
					ret = -EFAULT;
			} else {
				ret = -EFAULT;
			}
			enc_pr(LOG_ALL,
				"[-]VDI_IOCTL_GET_RESERVED_VIDEO_MEMORY_INFO\n");
		}
		break;
#ifdef CONFIG_COMPAT
	case VDI_IOCTL_GET_RESERVED_VIDEO_MEMORY_INFO32:
		{
			struct compat_vpudrv_buffer_t buf32;

			memset(&buf32, 0, sizeof(struct compat_vpudrv_buffer_t));
			enc_pr(LOG_ALL,
				"[+]VDI_IOCTL_GET_RESERVED_VIDEO_MEMORY_INFO32\n");

			buf32.size = s_video_memory.size;
			buf32.phys_addr =
				(compat_ulong_t)s_video_memory.phys_addr;
			buf32.virt_addr =
				(compat_ulong_t)s_video_memory.virt_addr;
			if (s_video_memory.phys_addr != 0) {
				ret = copy_to_user((void __user *)arg,
					&buf32,
					sizeof(struct compat_vpudrv_buffer_t));
				if (ret != 0)
					ret = -EFAULT;
			} else {
				ret = -EFAULT;
			}
			enc_pr(LOG_ALL,
				"[-]VDI_IOCTL_GET_RESERVED_VIDEO_MEMORY_INFO32\n");
		}
		break;
#endif
	case VDI_IOCTL_WAIT_INTERRUPT:
		{
			struct vpudrv_intr_info_t info;

			enc_pr(LOG_ALL,
				"[+]VDI_IOCTL_WAIT_INTERRUPT\n");
			ret = copy_from_user(&info,
				(struct vpudrv_intr_info_t *)arg,
				sizeof(struct vpudrv_intr_info_t));
			if (ret != 0)
				return -EFAULT;

			ret = wait_event_interruptible_timeout(
				s_interrupt_wait_q,
				s_interrupt_flag != 0,
				msecs_to_jiffies(info.timeout));
			if (!ret) {
				ret = -ETIME;
				break;
			}
			enc_pr(LOG_INFO,
			       "s_interrupt_flag(%d), reason(0x%08lx)\n",
			       s_interrupt_flag, dev->interrupt_reason);
			if (dev->interrupt_reason & (1 << W4_INT_ENC_PIC)) {
				u32 start, end, size, core = 0;

				start = ReadVpuRegister(W4_BS_RD_PTR);
				end = ReadVpuRegister(W4_BS_WR_PTR);
				size = ReadVpuRegister(W4_RET_ENC_PIC_BYTE);
				enc_pr(LOG_INFO, "flush output buffer, ");
				enc_pr(LOG_INFO,
					"start:0x%x, end:0x%x, size:0x%x\n",
					start, end, size);
				if (end - start > size && end > start)
					size = end - start;
				if (size > 0)
					cache_flush(start, size);
			}

			if (signal_pending(current)) {
				ret = -ERESTARTSYS;
				break;
			}

			enc_pr(LOG_INFO,
				"s_interrupt_flag(%d), reason(0x%08lx)\n",
				s_interrupt_flag, dev->interrupt_reason);

			info.intr_reason = dev->interrupt_reason;
			s_interrupt_flag = 0;
			dev->interrupt_reason = 0;
			ret = copy_to_user((void __user *)arg,
				&info, sizeof(struct vpudrv_intr_info_t));
			enc_pr(LOG_ALL,
				"[-]VDI_IOCTL_WAIT_INTERRUPT\n");
			if (ret != 0)
				return -EFAULT;
		}
		break;
	case VDI_IOCTL_SET_CLOCK_GATE:
		{
			u32 clkgate;

			enc_pr(LOG_ALL,
				"[+]VDI_IOCTL_SET_CLOCK_GATE\n");
			if (get_user(clkgate, (u32 __user *) arg))
				return -EFAULT;
#ifdef VPU_SUPPORT_CLOCK_CONTROL
			vpu_clk_config(clkgate);
#endif
			enc_pr(LOG_ALL,
				"[-]VDI_IOCTL_SET_CLOCK_GATE\n");
		}
		break;
	case VDI_IOCTL_GET_INSTANCE_POOL:
		{
			enc_pr(LOG_ALL,
				"[+]VDI_IOCTL_GET_INSTANCE_POOL\n");
			ret = down_interruptible(&s_vpu_sem);
			if (ret != 0)
				break;

			if (s_instance_pool.base != 0) {
				ret = copy_to_user((void __user *)arg,
					&s_instance_pool,
					sizeof(struct vpudrv_buffer_t));
				ret = (ret != 0) ? -EFAULT : 0;
			} else {
				ret = copy_from_user(&s_instance_pool,
					(struct vpudrv_buffer_t *)arg,
					sizeof(struct vpudrv_buffer_t));
				if (ret == 0) {
					s_instance_pool.size =
						PAGE_ALIGN(
						s_instance_pool.size);
					s_instance_pool.base =
						(ulong)vmalloc(
						s_instance_pool.size);
					s_instance_pool.phys_addr =
						s_instance_pool.base;
					if (s_instance_pool.base == 0) {
						ret = -EFAULT;
						up(&s_vpu_sem);
						break;
					}
					/*clearing memory*/
					memset((void *)s_instance_pool.base,
						0, s_instance_pool.size);
					ret = copy_to_user((void __user *)arg,
						&s_instance_pool,
						sizeof(struct vpudrv_buffer_t));
					if (ret != 0)
						ret = -EFAULT;
				} else
					ret = -EFAULT;
			}
			up(&s_vpu_sem);
			enc_pr(LOG_ALL,
				"[-]VDI_IOCTL_GET_INSTANCE_POOL\n");
		}
		break;
#ifdef CONFIG_COMPAT
	case VDI_IOCTL_GET_INSTANCE_POOL32:
		{
			struct compat_vpudrv_buffer_t buf32;

			memset(&buf32, 0, sizeof(struct compat_vpudrv_buffer_t));
			enc_pr(LOG_ALL,
				"[+]VDI_IOCTL_GET_INSTANCE_POOL32\n");
			ret = down_interruptible(&s_vpu_sem);
			if (ret != 0)
				break;
			if (s_instance_pool.base != 0) {
				buf32.size = s_instance_pool.size;
				buf32.phys_addr =
					(compat_ulong_t)
					s_instance_pool.phys_addr;
				buf32.virt_addr =
					(compat_ulong_t)
					s_instance_pool.virt_addr;
				ret = copy_to_user((void __user *)arg,
					&buf32,
					sizeof(struct compat_vpudrv_buffer_t));
				ret = (ret != 0) ? -EFAULT : 0;
			} else {
				ret = copy_from_user(&buf32,
					(struct compat_vpudrv_buffer_t *)arg,
					sizeof(struct compat_vpudrv_buffer_t));
				if (ret == 0) {
					s_instance_pool.size = buf32.size;
					s_instance_pool.size =
						PAGE_ALIGN(
						s_instance_pool.size);
					s_instance_pool.base =
						(ulong)vmalloc(
						s_instance_pool.size);
					s_instance_pool.phys_addr =
						s_instance_pool.base;
					buf32.size =
						s_instance_pool.size;
					buf32.phys_addr =
						(compat_ulong_t)
						s_instance_pool.phys_addr;
					buf32.base =
						(compat_ulong_t)
						s_instance_pool.base;
					buf32.virt_addr =
						(compat_ulong_t)
						s_instance_pool.virt_addr;
					if (s_instance_pool.base == 0) {
						ret = -EFAULT;
						up(&s_vpu_sem);
						break;
					}
					/*clearing memory*/
					memset((void *)s_instance_pool.base,
						0x0, s_instance_pool.size);
					ret = copy_to_user((void __user *)arg,
						&buf32,
						sizeof(
						struct compat_vpudrv_buffer_t));
					if (ret != 0)
						ret = -EFAULT;
				} else
					ret = -EFAULT;
			}
			up(&s_vpu_sem);
			enc_pr(LOG_ALL,
				"[-]VDI_IOCTL_GET_INSTANCE_POOL32\n");
		}
		break;
#endif
	case VDI_IOCTL_GET_COMMON_MEMORY:
		{
			enc_pr(LOG_ALL,
				"[+]VDI_IOCTL_GET_COMMON_MEMORY\n");
			if (s_common_memory.phys_addr != 0) {
				ret = copy_to_user((void __user *)arg,
					&s_common_memory,
					sizeof(struct vpudrv_buffer_t));
				if (ret != 0)
					ret = -EFAULT;
			} else {
				ret = copy_from_user(&s_common_memory,
					(struct vpudrv_buffer_t *)arg,
					sizeof(struct vpudrv_buffer_t));
				if (ret != 0) {
					ret = -EFAULT;
					break;
				}
				if (vpu_alloc_dma_buffer(
					&s_common_memory) != -1) {
					ret = copy_to_user((void __user *)arg,
						&s_common_memory,
						sizeof(struct vpudrv_buffer_t));
					if (ret != 0)
						ret = -EFAULT;
				} else
					ret = -EFAULT;
			}
			enc_pr(LOG_ALL,
				"[-]VDI_IOCTL_GET_COMMON_MEMORY\n");
		}
		break;
#ifdef CONFIG_COMPAT
	case VDI_IOCTL_GET_COMMON_MEMORY32:
		{
			struct compat_vpudrv_buffer_t buf32;

			memset(&buf32, 0, sizeof(struct compat_vpudrv_buffer_t));
			enc_pr(LOG_ALL,
				"[+]VDI_IOCTL_GET_COMMON_MEMORY32\n");

			buf32.size = s_common_memory.size;
			buf32.phys_addr =
				(compat_ulong_t)
				s_common_memory.phys_addr;
			buf32.virt_addr =
				(compat_ulong_t)
				s_common_memory.virt_addr;
			if (s_common_memory.phys_addr != 0) {
				ret = copy_to_user((void __user *)arg,
					&buf32,
					sizeof(struct compat_vpudrv_buffer_t));
				if (ret != 0)
					ret = -EFAULT;
			} else {
				ret = copy_from_user(&buf32,
					(struct compat_vpudrv_buffer_t *)arg,
					sizeof(struct compat_vpudrv_buffer_t));
				if (ret != 0) {
					ret = -EFAULT;
					break;
				}
				s_common_memory.size = buf32.size;
				if (vpu_alloc_dma_buffer(
					&s_common_memory) != -1) {
					buf32.size =
						s_common_memory.size;
					buf32.phys_addr =
						(compat_ulong_t)
						s_common_memory.phys_addr;
					buf32.virt_addr =
						(compat_ulong_t)
						s_common_memory.virt_addr;
					ret = copy_to_user((void __user *)arg,
						&buf32,
						sizeof(
						struct compat_vpudrv_buffer_t));
					if (ret != 0)
						ret = -EFAULT;
				} else
					ret = -EFAULT;
			}
			enc_pr(LOG_ALL,
				"[-]VDI_IOCTL_GET_COMMON_MEMORY32\n");
		}
		break;
#endif
	case VDI_IOCTL_OPEN_INSTANCE:
		{
			struct vpudrv_inst_info_t inst_info;
			struct vpudrv_instanace_list_t *vil, *n;

			vil = kzalloc(sizeof(*vil), GFP_KERNEL);
			if (!vil)
				return -ENOMEM;

			if (copy_from_user(&inst_info,
				(struct vpudrv_inst_info_t *)arg,
				sizeof(struct vpudrv_inst_info_t)))
			{
				kfree(vil);
				return -EFAULT;
			}

			vil->inst_idx = inst_info.inst_idx;
			vil->core_idx = inst_info.core_idx;
			vil->filp = filp;

			spin_lock(&s_vpu_lock);
			list_add(&vil->list, &s_inst_list_head);

			/* counting the current open instance number */
			inst_info.inst_open_count = 0;
			list_for_each_entry_safe(vil, n,
				&s_inst_list_head, list) {
				if (vil->core_idx == inst_info.core_idx)
					inst_info.inst_open_count++;
			}

			 /* flag just for that vpu is in opened or closed */
			s_vpu_open_ref_count++;
			spin_unlock(&s_vpu_lock);

			if (copy_to_user((void __user *)arg,
				&inst_info,
				sizeof(struct vpudrv_inst_info_t))) {
				kfree(vil);
				return -EFAULT;
			}

			enc_pr(LOG_DEBUG,
				"VDI_IOCTL_OPEN_INSTANCE ");
			enc_pr(LOG_DEBUG,
				"core_idx=%d, inst_idx=%d, ",
				(u32)inst_info.core_idx,
				(u32)inst_info.inst_idx);
			enc_pr(LOG_DEBUG,
				"s_vpu_open_ref_count=%d, inst_open_count=%d\n",
				s_vpu_open_ref_count,
				inst_info.inst_open_count);
		}
		break;
	case VDI_IOCTL_CLOSE_INSTANCE:
		{
			struct vpudrv_inst_info_t inst_info;
			struct vpudrv_instanace_list_t *vil, *n;

			enc_pr(LOG_ALL,
				"[+]VDI_IOCTL_CLOSE_INSTANCE\n");
			if (copy_from_user(&inst_info,
				(struct vpudrv_inst_info_t *)arg,
				sizeof(struct vpudrv_inst_info_t)))
				return -EFAULT;

			spin_lock(&s_vpu_lock);
			list_for_each_entry_safe(vil, n,
				&s_inst_list_head, list) {
				if (vil->inst_idx == inst_info.inst_idx &&
					vil->core_idx == inst_info.core_idx) {
					list_del(&vil->list);
					kfree(vil);
					break;
				}
			}

			/* counting the current open instance number */
			inst_info.inst_open_count = 0;
			list_for_each_entry_safe(vil, n,
				&s_inst_list_head, list) {
				if (vil->core_idx == inst_info.core_idx)
					inst_info.inst_open_count++;
			}

			/* flag just for that vpu is in opened or closed */
			s_vpu_open_ref_count--;
			spin_unlock(&s_vpu_lock);

			if (copy_to_user((void __user *)arg,
				&inst_info,
				sizeof(struct vpudrv_inst_info_t)))
				return -EFAULT;

			enc_pr(LOG_DEBUG,
				"VDI_IOCTL_CLOSE_INSTANCE ");
			enc_pr(LOG_DEBUG,
				"core_idx=%d, inst_idx=%d, ",
				(u32)inst_info.core_idx,
				(u32)inst_info.inst_idx);
			enc_pr(LOG_DEBUG,
				"s_vpu_open_ref_count=%d, inst_open_count=%d\n",
				s_vpu_open_ref_count,
				inst_info.inst_open_count);
		}
		break;
	case VDI_IOCTL_GET_INSTANCE_NUM:
		{
			struct vpudrv_inst_info_t inst_info;
			struct vpudrv_instanace_list_t *vil, *n;

			enc_pr(LOG_ALL,
				"[+]VDI_IOCTL_GET_INSTANCE_NUM\n");

			ret = copy_from_user(&inst_info,
				(struct vpudrv_inst_info_t *)arg,
				sizeof(struct vpudrv_inst_info_t));
			if (ret != 0)
				break;

			inst_info.inst_open_count = 0;

			spin_lock(&s_vpu_lock);
			list_for_each_entry_safe(vil, n,
				&s_inst_list_head, list) {
				if (vil->core_idx == inst_info.core_idx)
					inst_info.inst_open_count++;
			}
			spin_unlock(&s_vpu_lock);

			ret = copy_to_user((void __user *)arg,
				&inst_info,
				sizeof(struct vpudrv_inst_info_t));

			enc_pr(LOG_DEBUG,
				"VDI_IOCTL_GET_INSTANCE_NUM ");
			enc_pr(LOG_DEBUG,
				"core_idx=%d, inst_idx=%d, open_count=%d\n",
				(u32)inst_info.core_idx,
				(u32)inst_info.inst_idx,
				inst_info.inst_open_count);
		}
		break;
	case VDI_IOCTL_RESET:
		{
			vpu_hw_reset();
		}
		break;
	case VDI_IOCTL_GET_REGISTER_INFO:
		{
			enc_pr(LOG_ALL,
				"[+]VDI_IOCTL_GET_REGISTER_INFO\n");
			ret = copy_to_user((void __user *)arg,
				&s_vpu_register,
				sizeof(struct vpudrv_buffer_t));
			if (ret != 0)
				ret = -EFAULT;
			enc_pr(LOG_ALL,
				"[-]VDI_IOCTL_GET_REGISTER_INFO ");
			enc_pr(LOG_ALL,
				"s_vpu_register.phys_addr=0x%lx, ",
				s_vpu_register.phys_addr);
			enc_pr(LOG_ALL,
				"s_vpu_register.virt_addr=0x%lx, ",
				s_vpu_register.virt_addr);
			enc_pr(LOG_ALL,
				"s_vpu_register.size=0x%x\n",
				s_vpu_register.size);
		}
		break;
#ifdef CONFIG_COMPAT
	case VDI_IOCTL_GET_REGISTER_INFO32:
		{
			struct compat_vpudrv_buffer_t buf32;

			memset(&buf32, 0, sizeof(struct compat_vpudrv_buffer_t));
			enc_pr(LOG_ALL,
				"[+]VDI_IOCTL_GET_REGISTER_INFO32\n");

			buf32.size = s_vpu_register.size;
			buf32.phys_addr =
				(compat_ulong_t)
				s_vpu_register.phys_addr;
			buf32.virt_addr =
				(compat_ulong_t)
				s_vpu_register.virt_addr;
			ret = copy_to_user((void __user *)arg,
				&buf32,
				sizeof(
				struct compat_vpudrv_buffer_t));
			if (ret != 0)
				ret = -EFAULT;
			enc_pr(LOG_ALL,
				"[-]VDI_IOCTL_GET_REGISTER_INFO32 ");
			enc_pr(LOG_ALL,
				"s_vpu_register.phys_addr=0x%lx, ",
				s_vpu_register.phys_addr);
			enc_pr(LOG_ALL,
				"s_vpu_register.virt_addr=0x%lx, ",
				s_vpu_register.virt_addr);
			enc_pr(LOG_ALL,
				"s_vpu_register.size=0x%x\n",
				s_vpu_register.size);
		}
		break;
	case VDI_IOCTL_FLUSH_BUFFER32:
		{
			struct vpudrv_buffer_pool_t *pool, *n;
			struct compat_vpudrv_buffer_t buf32;
			struct vpudrv_buffer_t vb;
			bool find = false;
			u32 cached = 0;

			enc_pr(LOG_ALL,
				"[+]VDI_IOCTL_FLUSH_BUFFER32\n");

			ret = copy_from_user(&buf32,
				(struct compat_vpudrv_buffer_t *)arg,
				sizeof(struct compat_vpudrv_buffer_t));
			if (ret)
				return -EFAULT;
			spin_lock(&s_vpu_lock);
			list_for_each_entry_safe(pool, n,
				&s_vbp_head, list) {
				if (pool->filp == filp) {
					vb = pool->vb;
					if (((compat_ulong_t)vb.phys_addr
						== buf32.phys_addr)
						&& find == false){
						cached = vb.cached;
						find = true;
					}
				}
			}
			spin_unlock(&s_vpu_lock);
			if (find && cached)
				dma_flush(
					(u32)buf32.phys_addr,
					(u32)buf32.size);
			enc_pr(LOG_ALL,
				"[-]VDI_IOCTL_FLUSH_BUFFER32\n");
		}
		break;
#endif
	case VDI_IOCTL_FLUSH_BUFFER:
		{
			struct vpudrv_buffer_pool_t *pool, *n;
			struct vpudrv_buffer_t vb, buf;
			bool find = false;
			u32 cached = 0;

			enc_pr(LOG_ALL,
				"[+]VDI_IOCTL_FLUSH_BUFFER\n");

			ret = copy_from_user(&buf,
				(struct vpudrv_buffer_t *)arg,
				sizeof(struct vpudrv_buffer_t));
			if (ret)
				return -EFAULT;
			spin_lock(&s_vpu_lock);
			list_for_each_entry_safe(pool, n,
				&s_vbp_head, list) {
				if (pool->filp == filp) {
					vb = pool->vb;
					if ((vb.phys_addr
						== buf.phys_addr)
						&& find == false){
						cached = vb.cached;
						find = true;
					}
				}
			}
			spin_unlock(&s_vpu_lock);
			if (find && cached)
				dma_flush(
					(u32)buf.phys_addr,
					(u32)buf.size);
			enc_pr(LOG_ALL,
				"[-]VDI_IOCTL_FLUSH_BUFFER\n");
		}
		break;
	case VDI_IOCTL_CONFIG_DMA:
		{
			struct vpu_dma_buf_info_t dma_info;

			enc_pr(LOG_ALL,
				"[+]VDI_IOCTL_CONFIG_DMA\n");
			if (copy_from_user(&dma_info,
				(struct vpu_dma_buf_info_t *)arg,
				sizeof(struct vpu_dma_buf_info_t)))
				return -EFAULT;

			if (vpu_src_addr_config(dma_info)) {
				enc_pr(LOG_ERROR,
					"src addr config error\n");
				return -EFAULT;
			}

			enc_pr(LOG_ALL,
				"[-]VDI_IOCTL_CONFIG_DMA %d, %d, %d\n",
				dma_info.fd[0],
				dma_info.fd[1],
				dma_info.fd[2]);
		}
		break;
	case VDI_IOCTL_UNMAP_DMA:
		{
			enc_pr(LOG_ALL,
				"[+]VDI_IOCTL_UNMAP_DMA\n");

			vpu_dma_buffer_unmap(&dma_cfg[0]);
			if (dma_cfg[1].paddr != 0) {
				vpu_dma_buffer_unmap(&dma_cfg[1]);
			}
			if (dma_cfg[2].paddr != 0) {
				vpu_dma_buffer_unmap(&dma_cfg[2]);
			}
			enc_pr(LOG_ALL,
				"[-]VDI_IOCTL_UNMAP_DMA\n");
		}
		break;
	default:
		{
			enc_pr(LOG_ERROR,
				"No such IOCTL, cmd is 0x%x\n", cmd);
			ret = -EFAULT;
		}
		break;
	}
	return ret;
}

#ifdef CONFIG_COMPAT
static long vpu_compat_ioctl(struct file *filp, u32 cmd, ulong arg)
{
	long ret;

	arg = (ulong)compat_ptr(arg);
	ret = vpu_ioctl(filp, cmd, arg);
	return ret;
}
#endif

static ssize_t vpu_write(struct file *filp,
	const char *buf,
	size_t len,
	loff_t *ppos)
{
	enc_pr(LOG_INFO, "vpu_write len=%d\n", (int)len);

	if (!buf) {
		enc_pr(LOG_ERROR, "vpu_write buf = NULL error\n");
		return -EFAULT;
	}

	if (len == sizeof(struct vpu_bit_firmware_info_t))	{
		struct vpu_bit_firmware_info_t *bit_firmware_info;

		bit_firmware_info =
			kmalloc(sizeof(struct vpu_bit_firmware_info_t),
			GFP_KERNEL);
		if (!bit_firmware_info) {
			enc_pr(LOG_ERROR,
				"vpu_write bit_firmware_info allocation error\n");
			return -EFAULT;
		}

		if (copy_from_user(bit_firmware_info, buf, len)) {
			enc_pr(LOG_ERROR,
				"vpu_write copy_from_user error for bit_firmware_info\n");
			kfree(bit_firmware_info);
			return -EFAULT;
		}

		if (bit_firmware_info->size ==
			sizeof(struct vpu_bit_firmware_info_t)) {
			enc_pr(LOG_INFO,
				"vpu_write set bit_firmware_info coreIdx=0x%x, ",
				bit_firmware_info->core_idx);
			enc_pr(LOG_INFO,
				"reg_base_offset=0x%x size=0x%x, bit_code[0]=0x%x\n",
				bit_firmware_info->reg_base_offset,
				bit_firmware_info->size,
				bit_firmware_info->bit_code[0]);

			if (bit_firmware_info->core_idx
				> MAX_NUM_VPU_CORE) {
				enc_pr(LOG_ERROR,
					"vpu_write coreIdx[%d] is ",
					bit_firmware_info->core_idx);
				enc_pr(LOG_ERROR,
					"exceeded than MAX_NUM_VPU_CORE[%d]\n",
					MAX_NUM_VPU_CORE);
				kfree(bit_firmware_info);
				return -ENODEV;
			}
			if (bit_firmware_info->core_idx >= MAX_NUM_VPU_CORE)
			{
				enc_pr(LOG_ERROR,"bit_firmware_info->core_idx %d is invalid!\n", bit_firmware_info->core_idx);
				kfree(bit_firmware_info);
				return -1;
			}
			memcpy((void *)&s_bit_firmware_info
				[bit_firmware_info->core_idx],
				bit_firmware_info,
				sizeof(struct vpu_bit_firmware_info_t));
			kfree(bit_firmware_info);
			return len;
		}
		kfree(bit_firmware_info);
	}
	return -1;
}

static s32 vpu_release(struct inode *inode, struct file *filp)
{
	s32 ret = 0;
	ulong flags;

	enc_pr(LOG_DEBUG, "vpu_release, calling process: %d:%s\n", current->pid, current->comm);
	ret = down_interruptible(&s_vpu_sem);
	enc_pr(LOG_DEBUG, "vpu_release, ret = %d\n", ret);

	spin_lock(&s_vpu_lock);
	if (s_vpu_drv_context.open_count <= 0) {
		enc_pr(LOG_DEBUG, "vpu_release, open_count=%d, already released or even not inited\n",
			s_vpu_drv_context.open_count);
		s_vpu_drv_context.open_count = 0;
		spin_unlock(&s_vpu_lock);
		goto exit_release;
	}
	spin_unlock(&s_vpu_lock);

	if (ret == 0) {
		vpu_free_buffers(filp);
		vpu_free_instances(filp);

		spin_lock(&s_vpu_lock);
		enc_pr(LOG_DEBUG, "vpu_release, decrease open_count from %d\n",
				s_vpu_drv_context.open_count);

		s_vpu_drv_context.open_count--;
		spin_unlock(&s_vpu_lock);
		if (s_vpu_drv_context.open_count == 0) {
			enc_pr(LOG_DEBUG,
			       "vpu_release: s_interrupt_flag(%d), reason(0x%08lx)\n",
			       s_interrupt_flag, s_vpu_drv_context.interrupt_reason);
			s_vpu_drv_context.interrupt_reason = 0;
			s_interrupt_flag = 0;
			if (s_instance_pool.base) {
				enc_pr(LOG_DEBUG, "free instance pool\n");
				vfree((const void *)s_instance_pool.base);
				s_instance_pool.base = 0;
			}
			if (s_common_memory.phys_addr) {
				enc_pr(LOG_INFO, "vpu_release, s_common_memory 0x%lx\n",s_common_memory.phys_addr);
				vpu_free_dma_buffer(&s_common_memory);
				s_common_memory.phys_addr = 0;
			}

			if (s_video_memory.phys_addr && !use_reserve) {
				enc_pr(LOG_DEBUG, "vpu_release, s_video_memory 0x%lx\n",s_video_memory.phys_addr);
				codec_mm_free_for_dma(
					VPU_DEV_NAME,
					(u32)s_video_memory.phys_addr);
				vmem_exit(&s_vmem);
				memset(&s_video_memory,
					0, sizeof(struct vpudrv_buffer_t));
				memset(&s_vmem,
					0, sizeof(struct video_mm_t));
			}

			if ((s_vpu_irq >= 0) && (s_vpu_irq_requested == true)) {
				free_irq(s_vpu_irq, &s_vpu_drv_context);
				s_vpu_irq_requested = false;
			}
			spin_lock_irqsave(&s_vpu_lock, flags);

			if (get_cpu_type() >= MESON_CPU_MAJOR_ID_SC2) {
				//vpu_clk_config(0);
				pwr_ctrl_psci_smc(PDID_SC2_DOS_WAVE, PWR_OFF);
			} else {
				WRITE_AOREG(AO_RTI_GEN_PWR_ISO0,
					READ_AOREG(AO_RTI_GEN_PWR_ISO0) |
					(get_cpu_type() == MESON_CPU_MAJOR_ID_SM1
					? 0x8 : (0x3<<12)));
			}

			udelay(10);

			WRITE_VREG(DOS_MEM_PD_WAVE420L, 0xffffffff);
#ifndef VPU_SUPPORT_CLOCK_CONTROL
			vpu_clk_config(0);
#endif

			if (get_cpu_type() >= MESON_CPU_MAJOR_ID_SC2) {

			} else {
				WRITE_AOREG(AO_RTI_GEN_PWR_SLEEP0,
					READ_AOREG(AO_RTI_GEN_PWR_SLEEP0) |
					(get_cpu_type() == MESON_CPU_MAJOR_ID_SM1
					? 0x8 : (0x3<<24)));
			}

			udelay(10);
			spin_unlock_irqrestore(&s_vpu_lock, flags);
			if (get_cpu_type() >= MESON_CPU_MAJOR_ID_SC2) {
			} else
			    amports_switch_gate("vdec", 0);
		}
	}
exit_release:
	up(&s_vpu_sem);
	return 0;
}

static s32 vpu_fasync(s32 fd, struct file *filp, s32 mode)
{
	struct vpu_drv_context_t *dev =
		(struct vpu_drv_context_t *)filp->private_data;
	return fasync_helper(fd, filp, mode, &dev->async_queue);
}

static s32 vpu_map_to_register(struct file *fp, struct vm_area_struct *vm)
{
	ulong pfn;

	vm->vm_flags |= VM_IO | VM_RESERVED;
	vm->vm_page_prot =
		pgprot_noncached(vm->vm_page_prot);
	pfn = s_vpu_register.phys_addr >> PAGE_SHIFT;
	return remap_pfn_range(vm, vm->vm_start, pfn,
		vm->vm_end - vm->vm_start,
		vm->vm_page_prot) ? -EAGAIN : 0;
}

static s32 vpu_map_to_physical_memory(
	struct file *fp, struct vm_area_struct *vm)
{
	vm->vm_flags |= VM_IO | VM_RESERVED;
	if (vm->vm_pgoff ==
		(s_common_memory.phys_addr >> PAGE_SHIFT)) {
		vm->vm_page_prot =
			pgprot_noncached(vm->vm_page_prot);
	} else {
		if (vpu_is_buffer_cached(fp, vm->vm_pgoff) == 0)
			vm->vm_page_prot =
				pgprot_noncached(vm->vm_page_prot);
	}
	/* vm->vm_page_prot = pgprot_writecombine(vm->vm_page_prot); */
	if (!pfn_valid(vm->vm_pgoff)) {
		enc_pr(LOG_ERROR, "%s invalid pfn\n", __FUNCTION__);
		return -EAGAIN;
	}
	return remap_pfn_range(vm, vm->vm_start, vm->vm_pgoff,
		vm->vm_end - vm->vm_start, vm->vm_page_prot) ? -EAGAIN : 0;
}

static s32 vpu_map_to_instance_pool_memory(
	struct file *fp, struct vm_area_struct *vm)
{
	s32 ret;
	long length = vm->vm_end - vm->vm_start;
	ulong start = vm->vm_start;
	s8 *vmalloc_area_ptr = (s8 *)s_instance_pool.base;
	ulong pfn;

	vm->vm_flags |= VM_RESERVED;

	/* loop over all pages, map it page individually */
	while (length > 0) {
		pfn = vmalloc_to_pfn(vmalloc_area_ptr);
		ret = remap_pfn_range(vm, start, pfn,
			PAGE_SIZE, PAGE_SHARED);
		if (ret < 0)
			return ret;
		start += PAGE_SIZE;
		vmalloc_area_ptr += PAGE_SIZE;
		length -= PAGE_SIZE;
	}
	return 0;
}

/*
 * @brief memory map interface for vpu file operation
 * @return 0 on success or negative error code on error
 */
static s32 vpu_mmap(struct file *fp, struct vm_area_struct *vm)
{
	/* if (vm->vm_pgoff == (s_vpu_register.phys_addr >> PAGE_SHIFT)) */
	if ((vm->vm_end - vm->vm_start == s_vpu_register.size + 1) &&
						(vm->vm_pgoff == 0)) {
		vm->vm_pgoff = (s_vpu_register.phys_addr >> PAGE_SHIFT);
		return vpu_map_to_register(fp, vm);
	}

	if (vm->vm_pgoff == 0)
		return vpu_map_to_instance_pool_memory(fp, vm);

	return vpu_map_to_physical_memory(fp, vm);
}
static int vpu_dma_buffer_map(struct vpu_dma_cfg *cfg)
{
	int ret = -1;
	int fd = -1;
	struct dma_buf *dbuf = NULL;
	struct dma_buf_attachment *d_att = NULL;
	struct sg_table *sg = NULL;
	void *vaddr = NULL;
	struct device *dev = NULL;
	enum dma_data_direction dir;

	if (cfg == NULL || (cfg->fd < 0) || cfg->dev == NULL) {
		enc_pr(LOG_ERROR, "error dma param\n");
		return -EINVAL;
	}
	fd = cfg->fd;
	dev = cfg->dev;
	dir = cfg->dir;

	dbuf = dma_buf_get(fd);
	if (dbuf == NULL) {
		enc_pr(LOG_ERROR, "failed to get dma buffer,fd %d\n",fd);
		return -EINVAL;
	}

	d_att = dma_buf_attach(dbuf, dev);
	if (d_att == NULL) {
		enc_pr(LOG_ERROR, "failed to set dma attach\n");
		goto attach_err;
	}

	sg = dma_buf_map_attachment(d_att, dir);
	if (sg == NULL) {
		enc_pr(LOG_ERROR, "failed to get dma sg\n");
		goto map_attach_err;
	}
	cfg->dbuf = dbuf;
	cfg->attach = d_att;
	cfg->vaddr = vaddr;
	cfg->sg = sg;

	return 0;

map_attach_err:
	dma_buf_detach(dbuf, d_att);
attach_err:
	dma_buf_put(dbuf);

	return ret;
}

static void vpu_dma_buffer_unmap(struct vpu_dma_cfg *cfg)
{
	int fd = -1;
	struct dma_buf *dbuf = NULL;
	struct dma_buf_attachment *d_att = NULL;
	struct sg_table *sg = NULL;
	/*void *vaddr = NULL;*/
	struct device *dev = NULL;
	enum dma_data_direction dir;

	if (cfg == NULL || (cfg->fd < 0) || cfg->dev == NULL
			|| cfg->dbuf == NULL /*|| cfg->vaddr == NULL*/
			|| cfg->attach == NULL || cfg->sg == NULL) {
		enc_pr(LOG_ERROR, "unmap: Error dma param\n");
		return;
	}

	fd = cfg->fd;
	dev = cfg->dev;
	dir = cfg->dir;
	dbuf = cfg->dbuf;
	d_att = cfg->attach;
	sg = cfg->sg;

	dma_buf_unmap_attachment(d_att, sg, dir);
	dma_buf_detach(dbuf, d_att);
	dma_buf_put(dbuf);

	enc_pr(LOG_INFO, "vpu_dma_buffer_unmap fd %d\n",fd);
}

static int vpu_dma_buffer_get_phys(struct vpu_dma_cfg *cfg, unsigned long *addr)
{
	struct sg_table *sg_table;
	struct page *page;
	int ret;

	ret = vpu_dma_buffer_map(cfg);
	if (ret < 0) {
		printk("vpu_dma_buffer_map failed\n");
		return ret;
	}
	if (cfg->sg) {
		sg_table = cfg->sg;
		page = sg_page(sg_table->sgl);
		*addr = PFN_PHYS(page_to_pfn(page));
		ret = 0;
	}
	enc_pr(LOG_INFO,"vpu_dma_buffer_get_phys\n");

	return ret;
}

static u32 vpu_src_addr_config(struct vpu_dma_buf_info_t info) {
	unsigned long phy_addr_y = 0;
	unsigned long phy_addr_u = 0;
	unsigned long phy_addr_v = 0;
	unsigned long Ysize = info.width * info.height;
	unsigned long Usize = Ysize >> 2;
	s32 ret = 0;
	u32 core = 0;

	//y
	dma_cfg[0].dir = DMA_TO_DEVICE;
	dma_cfg[0].fd = info.fd[0];
	dma_cfg[0].dev = &(hevc_pdev->dev);
	ret = vpu_dma_buffer_get_phys(&dma_cfg[0], &phy_addr_y);
	if (ret < 0) {
		enc_pr(LOG_ERROR, "import fd %d failed\n", info.fd[0]);
		return -1;
	}

	//u
	if (info.num_planes >=2) {
		dma_cfg[1].dir = DMA_TO_DEVICE;
		dma_cfg[1].fd = info.fd[1];
		dma_cfg[1].dev = &(hevc_pdev->dev);
		ret = vpu_dma_buffer_get_phys(&dma_cfg[1], &phy_addr_u);
		if (ret < 0) {
			enc_pr(LOG_ERROR, "import fd %d failed\n", info.fd[1]);
			return -1;
		}
	}

	//v
	if (info.num_planes >=3) {
		dma_cfg[2].dir = DMA_TO_DEVICE;
		dma_cfg[2].fd = info.fd[2];
		dma_cfg[2].dev = &(hevc_pdev->dev);
		ret = vpu_dma_buffer_get_phys(&dma_cfg[2], &phy_addr_v);
		if (ret < 0) {
			enc_pr(LOG_ERROR, "import fd %d failed\n", info.fd[2]);
			return -1;
		}
	}

	enc_pr(LOG_INFO, "vpu_src_addr_config phy_addr 0x%lx, 0x%lx, 0x%lx\n",
		phy_addr_y, phy_addr_u, phy_addr_v);

	dma_cfg[0].paddr = (void *)phy_addr_y;
	dma_cfg[1].paddr = (void *)phy_addr_u;
	dma_cfg[2].paddr = (void *)phy_addr_v;

	enc_pr(LOG_INFO, "info.num_planes %d, info.fmt %d\n",
		info.num_planes, info.fmt);

	WriteVpuRegister(W4_SRC_ADDR_Y, phy_addr_y);
	if (info.num_planes == 1) {
		if (info.fmt == AMVENC_YUV420) {
			WriteVpuRegister(W4_SRC_ADDR_U, phy_addr_y + Ysize);
			WriteVpuRegister(W4_SRC_ADDR_V, phy_addr_y + Ysize + Usize);
		} else if (info.fmt == AMVENC_NV12 || info.fmt == AMVENC_NV21 ) {
			WriteVpuRegister(W4_SRC_ADDR_U, phy_addr_y + Ysize);
			WriteVpuRegister(W4_SRC_ADDR_V, phy_addr_y + Ysize);
		} else {
			enc_pr(LOG_ERROR, "not support fmt %d\n", info.fmt);
		}

	} else if (info.num_planes == 2) {
		if (info.fmt == AMVENC_NV12 || info.fmt == AMVENC_NV21 ) {
			WriteVpuRegister(W4_SRC_ADDR_U, phy_addr_u);
			WriteVpuRegister(W4_SRC_ADDR_V, phy_addr_u);
		} else {
			enc_pr(LOG_ERROR, "not support fmt %d\n", info.fmt);
		}

	} else if (info.num_planes == 3) {
		if (info.fmt == AMVENC_YUV420) {
			WriteVpuRegister(W4_SRC_ADDR_U, phy_addr_u);
			WriteVpuRegister(W4_SRC_ADDR_V, phy_addr_v);
		} else {
			enc_pr(LOG_ERROR, "not support fmt %d\n", info.fmt);
		}
	}
	return 0;

}

static const struct file_operations vpu_fops = {
	.owner = THIS_MODULE,
	.open = vpu_open,
	.release = vpu_release,
	.write = vpu_write,
	.unlocked_ioctl = vpu_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = vpu_compat_ioctl,
#endif
	.fasync = vpu_fasync,
	.mmap = vpu_mmap,
};

static ssize_t hevcenc_status_show(struct class *cla,
				  struct class_attribute *attr, char *buf)
{
	return snprintf(buf, 40, "hevcenc_status_show\n");
}

#if LINUX_VERSION_CODE <= KERNEL_VERSION(4,13,1)
static struct class_attribute hevcenc_class_attrs[] = {
	__ATTR(encode_status,
	S_IRUGO | S_IWUSR,
	hevcenc_status_show,
	NULL),
	__ATTR_NULL
};

static struct class hevcenc_class = {
	.name = VPU_CLASS_NAME,
	.class_attrs = hevcenc_class_attrs,
};
#else /* LINUX_VERSION_CODE <= KERNEL_VERSION(4,13,1) */

static CLASS_ATTR_RO(hevcenc_status);

static struct attribute *hevcenc_class_attrs[] = {
	&class_attr_hevcenc_status.attr,
	NULL
};

ATTRIBUTE_GROUPS(hevcenc_class);

static struct class hevcenc_class = {
	.name = VPU_CLASS_NAME,
	.class_groups = hevcenc_class_groups,
};
#endif /* LINUX_VERSION_CODE <= KERNEL_VERSION(4,13,1) */


s32 init_HevcEnc_device(void)
{
	s32  r = 0;

	r = register_chrdev(0, VPU_DEV_NAME, &vpu_fops);
	if (r <= 0) {
		enc_pr(LOG_ERROR, "register hevcenc device error.\n");
		return  r;
	}
	s_vpu_major = r;

	r = class_register(&hevcenc_class);
	if (r < 0) {
		enc_pr(LOG_ERROR, "error create hevcenc class.\n");
		return r;
	}

	hevcenc_dev = device_create(&hevcenc_class, NULL,
				       MKDEV(s_vpu_major, 0), NULL,
				       VPU_DEV_NAME);

	if (IS_ERR(hevcenc_dev)) {
		enc_pr(LOG_ERROR, "create hevcenc device error.\n");
		class_unregister(&hevcenc_class);
		return -1;
	}
	return r;
}

s32 uninit_HevcEnc_device(void)
{
	if (hevcenc_dev)
		device_destroy(&hevcenc_class, MKDEV(s_vpu_major, 0));

	class_destroy(&hevcenc_class);

	unregister_chrdev(s_vpu_major, VPU_DEV_NAME);
	return 0;
}

static s32 hevc_mem_device_init(
	struct reserved_mem *rmem, struct device *dev)
{
	s32 r;

	if (!rmem) {
		enc_pr(LOG_ERROR,
			"Can not obtain I/O memory, will allocate hevc buffer!\n");
		r = -EFAULT;
		return r;
	}

	if ((!rmem->base) ||
		(rmem->size < VPU_INIT_VIDEO_MEMORY_SIZE_IN_BYTE)) {
		enc_pr(LOG_ERROR,
			"memory range error, 0x%lx - 0x%lx\n",
			 (ulong)rmem->base, (ulong)rmem->size);
		r = -EFAULT;
		return r;
	}
	r = 0;
	s_video_memory.size = rmem->size;
	s_video_memory.phys_addr = (ulong)rmem->base;
	enc_pr(LOG_DEBUG, "hevc_mem_device_init %d, 0x%lx\n ",s_video_memory.size,s_video_memory.phys_addr);

	return r;
}

static s32 vpu_probe(struct platform_device *pdev)
{
	s32 err = 0, irq, reg_count, idx;
	struct resource res;
	struct device_node *np, *child;

	enc_pr(LOG_DEBUG, "vpu_probe, clock_a: %d, clock_b: %d, clock_c: %d\n",
		wave_clocka, wave_clockb, wave_clockc);

	s_vpu_major = 0;
	use_reserve = false;
	s_vpu_irq = -1;
	cma_pool_size = 0;
	s_vpu_irq_requested = false;
	s_vpu_open_ref_count = 0;
	hevcenc_dev = NULL;
	hevc_pdev = NULL;
	memset(&s_video_memory, 0, sizeof(struct vpudrv_buffer_t));
	memset(&s_vpu_register, 0, sizeof(struct vpudrv_buffer_t));
	memset(&s_vmem, 0, sizeof(struct video_mm_t));
	memset(&s_bit_firmware_info[0], 0, sizeof(s_bit_firmware_info));
	memset(&res, 0, sizeof(struct resource));

	idx = of_reserved_mem_device_init(&pdev->dev);

	if (idx != 0) {
		enc_pr(LOG_DEBUG,
			"HevcEnc reserved memory config fail.\n");
	} else if (s_video_memory.phys_addr) {
		use_reserve = true;
	}

	if (use_reserve == false) {
#ifndef CONFIG_CMA
		enc_pr(LOG_ERROR,
			"HevcEnc reserved memory is invaild, probe fail!\n");
		err = -EFAULT;
		goto ERROR_PROVE_DEVICE;
#else
		cma_pool_size =
			(codec_mm_get_total_size() >
			(VPU_INIT_VIDEO_MEMORY_SIZE_IN_BYTE)) ?
			(VPU_INIT_VIDEO_MEMORY_SIZE_IN_BYTE) :
			codec_mm_get_total_size();
		enc_pr(LOG_DEBUG,
			"HevcEnc - cma memory pool size: %d MB\n",
			(u32)cma_pool_size / SZ_1M);
#endif
	}

	/* get interrupt resource */
	irq = platform_get_irq_byname(pdev, "wave420l_irq");
	if (irq < 0) {
		enc_pr(LOG_ERROR, "get HevcEnc irq resource error\n");
		err = -ENXIO;
		goto ERROR_PROVE_DEVICE;
	}
	s_vpu_irq = irq;
	enc_pr(LOG_DEBUG, "HevcEnc - wave420l_irq: %d\n", s_vpu_irq);
#if 0
	rstc = devm_reset_control_get(&pdev->dev, "HevcEnc");
	if (IS_ERR(rstc)) {
		enc_pr(LOG_ERROR,
			"get HevcEnc rstc error: %lx\n", PTR_ERR(rstc));
		rstc = NULL;
		err = -ENOENT;
		goto ERROR_PROVE_DEVICE;
	}
	reset_control_assert(rstc);
	s_vpu_rstc = rstc;

	clk = clk_get(&pdev->dev, "clk_HevcEnc");
	if (IS_ERR(clk)) {
		enc_pr(LOG_ERROR, "cannot get clock\n");
		clk = NULL;
		err = -ENOENT;
		goto ERROR_PROVE_DEVICE;
	}
	s_vpu_clk = clk;
#endif

	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_SC2) {
		if (vpu_clk_prepare(&pdev->dev, &s_vpu_clks)) {
			err = -ENOENT;
			//goto ERROR_PROVE_DEVICE;
			return err;
		}
	}

	np = pdev->dev.of_node;
	reg_count = 0;
	for_each_child_of_node(np, child) {
		if (of_address_to_resource(child, 0, &res)
			|| (reg_count > 1)) {
			enc_pr(LOG_ERROR,
				"no reg ranges or more reg ranges %d\n",
				reg_count);
			err = -ENXIO;
			goto ERROR_PROVE_DEVICE;
		}
		/* if platform driver is implemented */
		if (res.start != 0) {
			s_vpu_register.phys_addr = res.start;
			s_vpu_register.virt_addr =
				(ulong)ioremap_nocache(
				res.start, resource_size(&res));
			s_vpu_register.size = res.end - res.start;
			enc_pr(LOG_DEBUG,
				"vpu base address get from platform driver ");
			enc_pr(LOG_DEBUG,
				"physical base addr=0x%lx, virtual base=0x%lx\n",
				s_vpu_register.phys_addr,
				s_vpu_register.virt_addr);
		} else {
			s_vpu_register.phys_addr = VPU_REG_BASE_ADDR;
			s_vpu_register.virt_addr =
				(ulong)ioremap_nocache(
				s_vpu_register.phys_addr, VPU_REG_SIZE);
			s_vpu_register.size = VPU_REG_SIZE;
			enc_pr(LOG_DEBUG,
				"vpu base address get from defined value ");
			enc_pr(LOG_DEBUG,
				"physical base addr=0x%lx, virtual base=0x%lx\n",
				s_vpu_register.phys_addr,
				s_vpu_register.virt_addr);
		}
		reg_count++;
	}

	/* get the major number of the character device */
	if (init_HevcEnc_device()) {
		err = -EBUSY;
		enc_pr(LOG_ERROR, "could not allocate major number\n");
		goto ERROR_PROVE_DEVICE;
	}
	enc_pr(LOG_DEBUG, "SUCCESS alloc_chrdev_region\n");

	init_waitqueue_head(&s_interrupt_wait_q);
	tasklet_init(&hevc_tasklet,
		hevcenc_isr_tasklet,
		(ulong)&s_vpu_drv_context);
	s_common_memory.base = 0;
	s_instance_pool.base = 0;

	if (use_reserve == true) {
		if (vmem_init(&s_vmem, s_video_memory.phys_addr,
			s_video_memory.size) < 0) {
			enc_pr(LOG_ERROR, "fail to init vmem system\n");
			goto ERROR_PROVE_DEVICE;
		}
		enc_pr(LOG_DEBUG,
			"success to probe vpu device with video memory ");
		enc_pr(LOG_DEBUG,
			"phys_addr=0x%lx, base = 0x%lx\n",
			(ulong)s_video_memory.phys_addr,
			(ulong)s_video_memory.base);
	} else
		enc_pr(LOG_DEBUG,
			"success to probe vpu device with video memory from cma\n");
	hevc_pdev = pdev;
	return 0;

ERROR_PROVE_DEVICE:
	if (s_vpu_register.virt_addr) {
		iounmap((void *)s_vpu_register.virt_addr);
		memset(&s_vpu_register, 0, sizeof(struct vpudrv_buffer_t));
	}

	if (s_video_memory.phys_addr) {
		vmem_exit(&s_vmem);
		memset(&s_video_memory, 0, sizeof(struct vpudrv_buffer_t));
		memset(&s_vmem, 0, sizeof(struct video_mm_t));
	}

	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_SC2)
		vpu_clk_unprepare(&pdev->dev, &s_vpu_clks);

	if (s_vpu_irq_requested == true) {
		if (s_vpu_irq >= 0) {
			free_irq(s_vpu_irq, &s_vpu_drv_context);
			s_vpu_irq = -1;
		}
		s_vpu_irq_requested = false;
	}
	uninit_HevcEnc_device();
	return err;
}

static s32 vpu_remove(struct platform_device *pdev)
{
	enc_pr(LOG_DEBUG, "vpu_remove\n");

	if (s_instance_pool.base) {
		vfree((const void *)s_instance_pool.base);
		s_instance_pool.base = 0;
	}

	if (s_common_memory.phys_addr) {
		vpu_free_dma_buffer(&s_common_memory);
		s_common_memory.phys_addr = 0;
	}

	if (s_video_memory.phys_addr) {
		if (!use_reserve) {
			codec_mm_free_for_dma(
			VPU_DEV_NAME,
			(u32)s_video_memory.phys_addr);
		}
		vmem_exit(&s_vmem);
		memset(&s_video_memory,
			0, sizeof(struct vpudrv_buffer_t));
		memset(&s_vmem,
			0, sizeof(struct video_mm_t));
	}

	if (s_vpu_irq_requested == true) {
		if (s_vpu_irq >= 0) {
			free_irq(s_vpu_irq, &s_vpu_drv_context);
			s_vpu_irq = -1;
		}
		s_vpu_irq_requested = false;
	}

	if (s_vpu_register.virt_addr) {
		iounmap((void *)s_vpu_register.virt_addr);
		memset(&s_vpu_register,
			0, sizeof(struct vpudrv_buffer_t));
	}
	hevc_pdev = NULL;

	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_SC2)
		vpu_clk_unprepare(&pdev->dev, &s_vpu_clks);
	uninit_HevcEnc_device();
	return 0;
}

#ifdef CONFIG_PM
static void Wave4BitIssueCommand(u32 core, u32 cmd)
{
	WriteVpuRegister(W4_VPU_BUSY_STATUS, 1);
	WriteVpuRegister(W4_CORE_INDEX, 0);
	/* coreIdx = ReadVpuRegister(W4_VPU_BUSY_STATUS); */
	/* coreIdx = 0; */
	/* WriteVpuRegister(W4_INST_INDEX,
	 *	(instanceIndex & 0xffff) | (codecMode << 16));
	 */
	WriteVpuRegister(W4_COMMAND, cmd);
	WriteVpuRegister(W4_VPU_HOST_INT_REQ, 1);
}

static s32 vpu_suspend(struct platform_device *pdev, pm_message_t state)
{
	u32 core;
	ulong timeout = jiffies + HZ; /* vpu wait timeout to 1sec */

	enc_pr(LOG_DEBUG, "vpu_suspend\n");

	vpu_clk_config(1);

	if (s_vpu_open_ref_count > 0) {
		for (core = 0; core < MAX_NUM_VPU_CORE; core++) {
			if (s_bit_firmware_info[core].size == 0)
				continue;
			while (ReadVpuRegister(W4_VPU_BUSY_STATUS)) {
				if (time_after(jiffies, timeout)) {
					enc_pr(LOG_ERROR,
						"SLEEP_VPU BUSY timeout");
					goto DONE_SUSPEND;
				}
			}
			Wave4BitIssueCommand(core, W4_CMD_SLEEP_VPU);

			while (ReadVpuRegister(W4_VPU_BUSY_STATUS)) {
				if (time_after(jiffies, timeout)) {
					enc_pr(LOG_ERROR,
						"SLEEP_VPU BUSY timeout");
					goto DONE_SUSPEND;
				}
			}
			if (ReadVpuRegister(W4_RET_SUCCESS) == 0) {
				enc_pr(LOG_ERROR,
					"SLEEP_VPU failed [0x%x]",
					ReadVpuRegister(W4_RET_FAIL_REASON));
				goto DONE_SUSPEND;
			}
		}
	}

	vpu_clk_config(0);
	return 0;

DONE_SUSPEND:
	vpu_clk_config(0);
	return -EAGAIN;
}
static s32 vpu_resume(struct platform_device *pdev)
{
	u32 i;
	u32 core;
	u32 val;
	ulong timeout = jiffies + HZ; /* vpu wait timeout to 1sec */
	ulong code_base;
	u32 code_size;
	u32 remap_size;
	u32 regVal;
	u32 hwOption = 0;

	enc_pr(LOG_DEBUG, "vpu_resume\n");

	vpu_clk_config(1);
	if (s_vpu_open_ref_count > 0) {
		for (core = 0; core < MAX_NUM_VPU_CORE; core++) {
			if (s_bit_firmware_info[core].size == 0)
				continue;
			code_base = s_common_memory.phys_addr;
			/* ALIGN TO 4KB */
			code_size = (s_common_memory.size & ~0xfff);
			if (code_size < s_bit_firmware_info[core].size * 2)
				goto DONE_WAKEUP;

			/*---- LOAD BOOT CODE */
			for (i = 0; i < 512; i += 2) {
				val = s_bit_firmware_info[core].bit_code[i];
				val |= (s_bit_firmware_info[core].bit_code[i+1] << 16);
				WriteVpu(code_base+(i*2), val);
			}

			regVal = 0;
			WriteVpuRegister(W4_PO_CONF, regVal);

			/* Reset All blocks */
			regVal = 0x7ffffff;
			WriteVpuRegister(W4_VPU_RESET_REQ, regVal);

			/* Waiting reset done */
			while (ReadVpuRegister(W4_VPU_RESET_STATUS)) {
				if (time_after(jiffies, timeout))
					goto DONE_WAKEUP;
			}

			WriteVpuRegister(W4_VPU_RESET_REQ, 0);

			/* remap page size */
			remap_size = (code_size >> 12) & 0x1ff;
			regVal = 0x80000000 | (W4_REMAP_CODE_INDEX<<12)
				| (0 << 16) | (1<<11) | remap_size;
			WriteVpuRegister(W4_VPU_REMAP_CTRL, regVal);
			/* DO NOT CHANGE! */
			WriteVpuRegister(W4_VPU_REMAP_VADDR, 0x00000000);
			WriteVpuRegister(W4_VPU_REMAP_PADDR, code_base);
			WriteVpuRegister(W4_ADDR_CODE_BASE, code_base);
			WriteVpuRegister(W4_CODE_SIZE, code_size);
			WriteVpuRegister(W4_CODE_PARAM, 0);
			WriteVpuRegister(W4_INIT_VPU_TIME_OUT_CNT, timeout);
			WriteVpuRegister(W4_HW_OPTION, hwOption);

			/* Interrupt */
#if 0
			regVal = (1 << W4_INT_DEC_PIC_HDR);
			regVal |= (1 << W4_INT_DEC_PIC);
			regVal |= (1 << W4_INT_QUERY_DEC);
			regVal |= (1 << W4_INT_SLEEP_VPU);
			regVal |= (1 << W4_INT_BSBUF_EMPTY);
#endif
			regVal = 0xfffffefe;
			WriteVpuRegister(W4_VPU_VINT_ENABLE, regVal);
			Wave4BitIssueCommand(core, W4_CMD_INIT_VPU);
			WriteVpuRegister(W4_VPU_REMAP_CORE_START, 1);
			while (ReadVpuRegister(W4_VPU_BUSY_STATUS)) {
				if (time_after(jiffies, timeout))
					goto DONE_WAKEUP;
			}

			if (ReadVpuRegister(W4_RET_SUCCESS) == 0) {
				enc_pr(LOG_ERROR,
					"WAKEUP_VPU failed [0x%x]",
					ReadVpuRegister(W4_RET_FAIL_REASON));
				goto DONE_WAKEUP;
			}
		}
	}

	if (s_vpu_open_ref_count == 0)
		vpu_clk_config(0);
DONE_WAKEUP:
	if (s_vpu_open_ref_count > 0)
		vpu_clk_config(1);
	return 0;
}
#else
#define vpu_suspend NULL
#define vpu_resume NULL
#endif /* !CONFIG_PM */

static const struct of_device_id cnm_hevcenc_dt_match[] = {
	{
		.compatible = "cnm, HevcEnc",
	},
	{},
};

static struct platform_driver vpu_driver = {
	.driver = {
		.name = VPU_PLATFORM_DEVICE_NAME,
		.of_match_table = cnm_hevcenc_dt_match,
	},
	.probe = vpu_probe,
	.remove = vpu_remove,
	.suspend = vpu_suspend,
	.resume = vpu_resume,
};

static s32 __init vpu_init(void)
{
	s32 res;

	enc_pr(LOG_DEBUG, "vpu_init\n");

	if ((get_cpu_type() != MESON_CPU_MAJOR_ID_GXM)
		&& (get_cpu_type() != MESON_CPU_MAJOR_ID_G12A)
			&& (get_cpu_type() != MESON_CPU_MAJOR_ID_GXLX)
				&& (get_cpu_type() != MESON_CPU_MAJOR_ID_G12B)
				&& (get_cpu_type() != MESON_CPU_MAJOR_ID_SM1)
				&& (get_cpu_type() != MESON_CPU_MAJOR_ID_SC2)) {
		enc_pr(LOG_DEBUG,
			"The chip is not support hevc encoder\n");
		return -1;
	}
	if (get_cpu_type() == MESON_CPU_MAJOR_ID_G12A) {
		if ((READ_EFUSE_REG(EFUSE_LIC2)  >> 12) & 1) {
			enc_pr(LOG_DEBUG,
				"Chip efuse disabled H265\n");
			return -1;
		}
	}

	res = platform_driver_register(&vpu_driver);
	enc_pr(LOG_INFO,
		"end vpu_init result=0x%x\n", res);
	return res;
}

static void __exit vpu_exit(void)
{
	enc_pr(LOG_DEBUG, "vpu_exit\n");
	if ((get_cpu_type() != MESON_CPU_MAJOR_ID_GXM) &&
		(get_cpu_type() != MESON_CPU_MAJOR_ID_G12A) &&
		(get_cpu_type() != MESON_CPU_MAJOR_ID_GXLX) &&
		(get_cpu_type() != MESON_CPU_MAJOR_ID_G12B) &&
		(get_cpu_type() != MESON_CPU_MAJOR_ID_SC2) &&
		(get_cpu_type() != MESON_CPU_MAJOR_ID_SM1)) {
		enc_pr(LOG_INFO,
			"The chip is not support hevc encoder\n");
		return;
	}
	platform_driver_unregister(&vpu_driver);
}

static const struct reserved_mem_ops rmem_hevc_ops = {
	.device_init = hevc_mem_device_init,
};

static s32 __init hevc_mem_setup(struct reserved_mem *rmem)
{
	rmem->ops = &rmem_hevc_ops;
	enc_pr(LOG_DEBUG, "HevcEnc reserved mem setup.\n");
	return 0;
}

module_param(print_level, uint, 0664);
MODULE_PARM_DESC(print_level, "\n print_level\n");

module_param(clock_level, uint, 0664);
MODULE_PARM_DESC(clock_level, "\n clock_level\n");

module_param(wave_clocka, uint, 0664);
MODULE_PARM_DESC(wave_clocka, "\n wave_clocka\n");

module_param(wave_clockb, uint, 0664);
MODULE_PARM_DESC(wave_clockb, "\n wave_clockb\n");

module_param(wave_clockc, uint, 0664);
MODULE_PARM_DESC(wave_clockc, "\n wave_clockc\n");

MODULE_AUTHOR("Amlogic using C&M VPU, Inc.");
MODULE_DESCRIPTION("VPU linux driver");
MODULE_LICENSE("GPL");

module_init(vpu_init);
module_exit(vpu_exit);
RESERVEDMEM_OF_DECLARE(amlogic, "amlogic, HevcEnc-memory", hevc_mem_setup);
