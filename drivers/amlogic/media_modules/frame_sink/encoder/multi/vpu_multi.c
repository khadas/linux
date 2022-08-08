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
 */
#define LOG_LINE() pr_err("[%s:%d]\n", __FUNCTION__, __LINE__);
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/module.h>
#include <linux/dma-mapping.h>
#include <linux/wait.h>
#include <linux/list.h>
#include <linux/compat.h>
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
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include <linux/kfifo.h>
#include <linux/kthread.h>
#include <linux/sched/signal.h>
#include <linux/amlogic/cpu_version.h>
//#include <linux/amlogic/pwr_ctrl.h>

#include <linux/amlogic/media/canvas/canvas.h>
#include <linux/amlogic/media/canvas/canvas_mgr.h>

#include <linux/amlogic/power_ctrl.h>
#include <dt-bindings/power/t7-pd.h>
#include <linux/amlogic/power_domain.h>
#include "../../../common/chips/decoder_cpu_ver_info.h"
#include "../../../frame_provider/decoder/utils/vdec.h"
#include "../../../frame_provider/decoder/utils/vdec_power_ctrl.h"
#include "vpu_multi.h"
#include "vmm_multi.h"

#define MAX_INTERRUPT_QUEUE (16*MAX_NUM_INSTANCE)

/* definitions to be changed as customer  configuration */
/* if you want to have clock gating scheme frame by frame */
#define VPU_SUPPORT_CLOCK_CONTROL

#define VPU_PLATFORM_DEVICE_NAME "amvenc_multi"
#define VPU_DEV_NAME "amvenc_multi"
#define VPU_CLASS_NAME "amvenc_multi"

#ifndef VM_RESERVED	/*for kernel up to 3.7.0 version*/
#define VM_RESERVED (VM_DONTEXPAND | VM_DONTDUMP)
#endif

#define MHz (1000000)

#define VPU_INIT_VIDEO_MEMORY_SIZE_IN_BYTE (256 * SZ_1M)

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
static s32 clock_gate_count = 0;
static u32 set_clock_freq = 0;

static struct video_mm_t s_vmem;
static struct vpudrv_buffer_t s_video_memory = {0};
static bool use_reserve;
static ulong cma_pool_size;
static u32 cma_cfg_size;

static u32 clock_a, clock_b, clock_c;
static int dump_input;
static int dump_es;
static int vpu_hw_reset(void);
static void hw_reset(bool reset);

struct vpu_clks {
	struct clk *dos_clk;
	struct clk *dos_apb_clk;
	struct clk *a_clk;
	struct clk *b_clk;
	struct clk *c_clk;
};

static struct vpu_clks s_vpu_clks;

#ifdef CONFIG_COMPAT
static struct file *file_open(const char *path, int flags, int rights)
{
    struct file *filp = NULL;
    mm_segment_t oldfs;
    long err1 = 0;
    void *err2 = NULL;

    oldfs = get_fs();
    //set_fs(get_ds());
    set_fs(KERNEL_DS);
    filp = filp_open(path, flags, rights);
    set_fs(oldfs);

    if (IS_ERR(filp)) {
        err1 = PTR_ERR(filp);
        err2 = ERR_PTR(err1);
        pr_err("filp_open return %p, %ld, %p\n", filp, err1, err2);
        return NULL;
    }

    return filp;
}

static void file_close(struct file *file)
{
    filp_close(file, NULL);
}
/*
static int file_read(struct file *file, unsigned long long offset, unsigned char *data, unsigned int size)
{
    mm_segment_t oldfs;
    int ret;

    oldfs = get_fs();
    set_fs(KERNEL_DS);

    ret = vfs_read(file, data, size, &offset);

    set_fs(oldfs);
    return ret;
}*/
static int file_write(struct file *file, unsigned long long offset, unsigned char *data, unsigned int size)
{
    mm_segment_t oldfs;
    int ret;
    //loff_t pos;

    oldfs = get_fs();
    set_fs(KERNEL_DS);
    //pos = file->f_pos;
    ret = vfs_write(file, data, size, &offset);
    //file->f_pos = pos;

    set_fs(oldfs);
    return ret;
}

static int file_sync(struct file *file)
{
    vfs_fsync(file, 0);
    return 0;
}
static s32 dump_raw_input(struct canvas_s *cs0) {
	u8 *data;

	u32 addr, canvas_w, picsize_y;
	//u32 input = request->src;
	//u8 iformat = MAX_FRAME_FMT;
	struct file *filp;
	int ret;

	addr  = cs0->addr;

	canvas_w  = ((cs0->width + 31) >> 5) << 5;
	picsize_y = cs0->height;

	filp = file_open("/data/multienc.yuv", O_CREAT | O_RDWR | O_APPEND, 0777);

	if (filp) {
		data = (u8*)phys_to_virt(addr);
		ret = file_write(filp, 0, data, canvas_w * picsize_y);

		file_sync(filp);
		file_close(filp);
	} else
		pr_err("open encoder.yuv failed\n");

	return 0;
}

static s32 dump_data(u32 phy_addr, u32 size) {
	u8 *data ;

	struct file *filp;
	int ret;

	filp = file_open("/data/multienc.es", O_CREAT | O_RDWR | O_APPEND, 0777);

	if (filp) {
		data = (u8*)phys_to_virt(phy_addr);
		ret = file_write(filp, 0, data, size);

		file_sync(filp);
		file_close(filp);
	} else
		pr_err("open encoder.es failed\n");

	return 0;
}
#endif

/*static s32 dump_encoded_data(u8 *data, u32 size) {
	struct file *filp;

	filp = file_open("/data/multienc_output.es", O_APPEND | O_RDWR, 0644);

	if (filp) {
		file_write(filp, 0, data, size);
		file_sync(filp);
		file_close(filp);
	} else
		pr_err("/data/multienc_output.es failed\n");

	return 0;
}*/

static void vpu_clk_put(struct device *dev, struct vpu_clks *clks)
{
	if (!(clks->c_clk == NULL || IS_ERR(clks->c_clk)))
		devm_clk_put(dev, clks->c_clk);
	if (!(clks->b_clk == NULL || IS_ERR(clks->b_clk)))
		devm_clk_put(dev, clks->b_clk);
	if (!(clks->a_clk == NULL || IS_ERR(clks->a_clk)))
		devm_clk_put(dev, clks->a_clk);
	if (!(clks->dos_apb_clk == NULL || IS_ERR(clks->dos_apb_clk)))
		devm_clk_put(dev, clks->dos_apb_clk);
	if (!(clks->dos_clk == NULL || IS_ERR(clks->dos_clk)))
		devm_clk_put(dev, clks->dos_clk);
}

static int vpu_clk_get(struct device *dev, struct vpu_clks *clks)
{
	int ret = 0;

	clks->dos_clk = devm_clk_get(dev, "clk_dos");

	if (IS_ERR(clks->dos_clk)) {
		enc_pr(LOG_ERROR, "cannot get clk_dos clock\n");
		clks->dos_clk = NULL;
		ret = -ENOENT;
		goto err;
	}

	clks->dos_apb_clk = devm_clk_get(dev, "clk_apb_dos");

	if (IS_ERR(clks->dos_apb_clk)) {
		enc_pr(LOG_ERROR, "cannot get clk_apb_dos clock\n");
		clks->dos_apb_clk = NULL;
		ret = -ENOENT;
		goto err;
	}

	clks->a_clk = devm_clk_get(dev, "clk_MultiEnc_A");

	if (IS_ERR(clks->a_clk)) {
		enc_pr(LOG_ERROR, "cannot get clock\n");
		clks->a_clk = NULL;
		ret = -ENOENT;
		goto err;
	}

	clks->b_clk = devm_clk_get(dev, "clk_MultiEnc_B");

	if (IS_ERR(clks->b_clk)) {
		enc_pr(LOG_ERROR, "cannot get clk_MultiEnc_B clock\n");
		clks->b_clk = NULL;
		ret = -ENOENT;
		goto err;
	}

	clks->c_clk = devm_clk_get(dev, "clk_MultiEnc_C");

	if (IS_ERR(clks->c_clk)) {
		enc_pr(LOG_ERROR, "cannot get clk_MultiEnc_C clock\n");
		clks->c_clk = NULL;
		ret = -ENOENT;
		goto err;
	}

	return 0;
err:
	vpu_clk_put(dev, clks);

	return ret;
}

static void vpu_clk_enable(struct vpu_clks *clks)
{
	u32 freq = 400;

	if (set_clock_freq && set_clock_freq <= 400)
		freq = set_clock_freq;

	clk_set_rate(clks->dos_clk, freq * MHz);
	clk_set_rate(clks->dos_apb_clk, freq * MHz);

	if (clock_a > 0) {
		pr_info("vpu_multi: desired clock_a freq %u\n", clock_a);
		clk_set_rate(clks->a_clk, clock_a);
	} else
		clk_set_rate(clks->a_clk, 666666666);

	if (clock_b > 0) {
		pr_info("vpu_multi: desired clock_b freq %u\n", clock_b);
		clk_set_rate(clks->b_clk, clock_b);
	} else
		clk_set_rate(clks->b_clk, 500 * MHz);

	if (clock_c > 0) {
		pr_info("vpu_multi: desired clock_c freq %u\n", clock_c);
		clk_set_rate(clks->c_clk, clock_c);
	} else
		clk_set_rate(clks->c_clk, 500 * MHz);

	clk_prepare_enable(clks->dos_clk);
	clk_prepare_enable(clks->dos_apb_clk);
	clk_prepare_enable(clks->a_clk);
	clk_prepare_enable(clks->b_clk);
	clk_prepare_enable(clks->c_clk);

	enc_pr(LOG_DEBUG, "dos: %ld, dos_apb: %ld, a: %ld, b: %ld, c: %ld\n",
	       clk_get_rate(clks->dos_clk), clk_get_rate(clks->dos_apb_clk),
	       clk_get_rate(clks->a_clk), clk_get_rate(clks->b_clk),
	       clk_get_rate(clks->c_clk));

	/* the power on */
	if (get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_T7) {
		pr_err("powering on wave521 for t7\n");
		vdec_poweron(VDEC_WAVE);
		mdelay(5);
		pr_err("wave power stauts after poweron: %d\n", vdec_on(VDEC_WAVE));
	} else {
		pwr_ctrl_psci_smc(PDID_T7_DOS_WAVE, true);
		mdelay(5);
		pr_err("wave power stauts after poweron: %lu\n", pwr_ctrl_status_psci_smc(PDID_T7_DOS_WAVE));
	}

	/* reset */
	hw_reset(true);
	mdelay(5);
	hw_reset(false);
	/* gate the clocks */
#ifdef VPU_SUPPORT_CLOCK_CONTROL
	pr_err("vpu_clk_enable, now gate off the clock\n");
	clk_disable(clks->c_clk);
	clk_disable(clks->b_clk);
	clk_disable(clks->a_clk);
#endif
}

static void vpu_clk_disable(struct vpu_clks *clks)
{
#ifdef VPU_SUPPORT_CLOCK_CONTROL
	if (clock_gate_count > 0)
#endif
	{
		enc_pr(LOG_INFO, "vpu unclosed clock %d\n", clock_gate_count);
		clk_disable(clks->c_clk);
		clk_disable(clks->b_clk);
		clk_disable(clks->a_clk);
		clock_gate_count = 0;
	}
	clk_unprepare(clks->c_clk);
	clk_unprepare(clks->b_clk);
	clk_unprepare(clks->a_clk);
	clk_disable_unprepare(clks->dos_apb_clk);
	clk_disable_unprepare(clks->dos_clk);
	/* the power off */
	/* the power on */
	if (get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_T7) {
		pr_err("powering off wave521 for t7\n");
		vdec_poweron(VDEC_WAVE);
		mdelay(5);
		pr_err("wave power stauts after poweroff: %d\n", vdec_on(VDEC_WAVE));
	} else {
		pwr_ctrl_psci_smc(PDID_T7_DOS_WAVE, false);
		mdelay(5);
		pr_err("wave power stauts after poweroff: %lu\n", pwr_ctrl_status_psci_smc(PDID_T7_DOS_WAVE));
	}
}

/* end customer definition */
static struct vpudrv_buffer_t s_instance_pool = {0};
static struct vpudrv_buffer_t s_common_memory = {0};
static struct vpu_drv_context_t s_vpu_drv_context;
static s32 s_vpu_major;
static s32 s_register_flag;
static struct device *multienc_dev;

static s32 s_vpu_open_ref_count;
static s32 s_vpu_irq;
static bool s_vpu_irq_requested;

static struct vpudrv_buffer_t s_vpu_register = {0};

static s32 s_interrupt_flag[MAX_NUM_INSTANCE];
static wait_queue_head_t s_interrupt_wait_q[MAX_NUM_INSTANCE];
static struct kfifo s_interrupt_pending_q[MAX_NUM_INSTANCE];
static s32 s_fifo_alloc_flag[MAX_NUM_INSTANCE];
static spinlock_t s_kfifo_lock = __SPIN_LOCK_UNLOCKED(s_kfifo_lock);

static spinlock_t s_vpu_lock = __SPIN_LOCK_UNLOCKED(s_vpu_lock);
static DEFINE_SEMAPHORE(s_vpu_sem);
static struct list_head s_vbp_head = LIST_HEAD_INIT(s_vbp_head);
static struct list_head s_inst_list_head = LIST_HEAD_INIT(s_inst_list_head);
static struct tasklet_struct multienc_tasklet;
static struct platform_device *multienc_pdev;

static struct vpu_bit_firmware_info_t s_bit_firmware_info[MAX_NUM_VPU_CORE];

static spinlock_t s_dma_buf_lock = __SPIN_LOCK_UNLOCKED(s_dma_buf_lock);
static struct list_head s_dma_bufp_head = LIST_HEAD_INIT(s_dma_bufp_head);

static s32 vpu_src_addr_config(struct vpudrv_dma_buf_info_t *dma_info,
		struct file *filp);
static s32 vpu_src_addr_unmap(struct vpudrv_dma_buf_info_t *dma_info,
		struct file *filp);
static void vpu_dma_buffer_unmap(struct vpu_dma_cfg *cfg);

static void dma_flush(u32 buf_start, u32 buf_size)
{
	if (multienc_pdev)
		dma_sync_single_for_device(
			&multienc_pdev->dev, buf_start,
			buf_size, DMA_TO_DEVICE);
}

static void cache_flush(u32 buf_start, u32 buf_size)
{
	if (multienc_pdev)
		dma_sync_single_for_cpu(
			&multienc_pdev->dev, buf_start,
			buf_size, DMA_FROM_DEVICE);
}

s32 vpu_hw_reset(void)
{
	enc_pr(LOG_DEBUG, "request vpu reset from application\n");
	return 0;
}

static void vpu_clk_config(int enable)
{
	struct vpu_clks *clks = &s_vpu_clks;

	enc_pr(LOG_INFO, " vpu clock config %d\n", enable);

	if (enable == 0) {
		clock_gate_count --;
		if (clock_gate_count == 0) {
			clk_disable(clks->c_clk);
			clk_disable(clks->b_clk);
			clk_disable(clks->a_clk);
		} else if (clock_gate_count < 0)
			enc_pr(LOG_ERROR, "vpu clock alredy closed %d\n",
				clock_gate_count);
	} else {
		clock_gate_count ++;
		if (clock_gate_count == 1) {
			clk_enable(clks->a_clk);
			clk_enable(clks->b_clk);
			clk_enable(clks->c_clk);
		}
	}
}

static s32 vpu_alloc_dma_buffer(struct vpudrv_buffer_t *vb)
{
	if (!vb)
		return -1;

	vb->phys_addr = (ulong)vmem_alloc(&s_vmem, vb->size, 0);
	if ((ulong)vb->phys_addr == (ulong)-1) {
		enc_pr(LOG_ERROR,
			"Physical memory allocation error size=%d\n",
			vb->size);
		return -1;
	}

	enc_pr(LOG_INFO,
		"%s: vb->phys_addr 0x%lx\n", __func__,
		vb->phys_addr);
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
	s32 instance_pool_size_per_core;
	void *vdi_mutexes_base;
	const s32 PTHREAD_MUTEX_T_DESTROY_VALUE = 0xdead10cc;

	enc_pr(LOG_DEBUG, "[VPUDRV] vpu_free_instances\n");

	/* s_instance_pool.size was assigned to the size of all core once
		call VDI_IOCTL_GET_INSTANCE_POOL by user. */
	instance_pool_size_per_core = (s_instance_pool.size/MAX_NUM_VPU_CORE);

	list_for_each_entry_safe(vil, n, &s_inst_list_head, list) {
		if (vil->filp == filp) {
			vip_base = (void *)(s_instance_pool.base +
				(instance_pool_size_per_core*vil->core_idx));
			enc_pr(LOG_INFO,
				"free_ins Idx=%d, core=%d, base=%p, sz=%d\n",
				(s32)vil->inst_idx, (s32)vil->core_idx,
				vip_base,
				(s32)instance_pool_size_per_core);
			vip = (struct vpudrv_instance_pool_t *)vip_base;
			if (vip) {
				/*	only first 4 byte is key point
				 *	(inUse of CodecInst in vpuapi)
				 *	to free the corresponding instance.
				 */
				memset(&vip->codecInstPool[vil->inst_idx],
					0x00, 4);

#define PTHREAD_MUTEX_T_HANDLE_SIZE 4
				vdi_mutexes_base = (vip_base +
					(instance_pool_size_per_core -
					PTHREAD_MUTEX_T_HANDLE_SIZE*4));
				enc_pr(LOG_INFO,
					"Force destroy in user space ");
				enc_pr(LOG_INFO," vdi_mutex_base=%p \n",
					vdi_mutexes_base);
				if (vdi_mutexes_base) {
					s32 i;
					for (i = 0; i < 4; i++) {
						memcpy(vdi_mutexes_base,
						&PTHREAD_MUTEX_T_DESTROY_VALUE,
						PTHREAD_MUTEX_T_HANDLE_SIZE);
						vdi_mutexes_base +=
						PTHREAD_MUTEX_T_HANDLE_SIZE;
					}
				}
			}
			spin_lock(&s_vpu_lock);
			s_vpu_open_ref_count--;
			list_del(&vil->list);
			spin_unlock(&s_vpu_lock);
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
				spin_lock(&s_vpu_lock);
				list_del(&pool->list);
				spin_unlock(&s_vpu_lock);
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
			if (((vb.phys_addr >> PAGE_SHIFT) == vm_pgoff)
				&& find == false){
				cached = vb.cached;
				find = true;
				break;
			}
		}
	}
	spin_unlock(&s_vpu_lock);
	enc_pr(LOG_ALL, "[-]vpu_is_buffer_cached, ret:%d\n", cached);
	return cached;
}

static s32 vpu_dma_buf_release(struct file *filp)
{
	struct vpudrv_dma_buf_pool_t *pool, *n;
	struct vpu_dma_cfg vb;

	enc_pr(LOG_DEBUG, "vpu_release_dma_buffers\n");
	list_for_each_entry_safe(pool, n, &s_dma_bufp_head, list) {
		if (pool->filp == filp) {
			vb = pool->dma_cfg;
			if (vb.attach) {
				vpu_dma_buffer_unmap(&vb);
				spin_lock(&s_dma_buf_lock);
				list_del(&pool->list);
				spin_unlock(&s_dma_buf_lock);
				kfree(pool);
			}
		}
	}
	return 0;
}

static inline u32 get_inst_idx(u32 reg_val)
{
	u32 inst_idx;
	int i;
	for (i=0; i < MAX_NUM_INSTANCE; i++)
	{
		if (((reg_val >> i)&0x01) == 1)
			break;
	}
	inst_idx = i;
	return inst_idx;
}

static s32 get_vpu_inst_idx(struct vpu_drv_context_t *dev, u32 *reason,
			    u32 empty_inst, u32 done_inst, u32 seq_inst)
{
	s32 inst_idx;
	u32 reg_val;
	u32 int_reason;

	int_reason = *reason;
	enc_pr(LOG_INFO,
		"[+]%s, reason=0x%x, empty_inst=0x%x, done_inst=0x%x\n",
		__func__, int_reason, empty_inst, done_inst);

	if (int_reason & (1 << INT_BSBUF_EMPTY))
	{
		reg_val = (empty_inst & 0xffff);
		inst_idx = get_inst_idx(reg_val);
		*reason = (1 << INT_BSBUF_EMPTY);
		enc_pr(LOG_DEBUG,
			"%s, RET_BS_EMPTY_INST reg_val=0x%x, inst_idx=%d\n",
			__func__, reg_val, inst_idx);
		goto GET_VPU_INST_IDX_HANDLED;
	}

	if (int_reason & (1 << INT_INIT_SEQ))
	{
		reg_val = (seq_inst & 0xffff);
		inst_idx = get_inst_idx(reg_val);
		*reason = (1 << INT_INIT_SEQ);
		enc_pr(LOG_DEBUG,
			"%s, RET_QUEUE_CMD_DONE INIT_SEQ val=0x%x, idx=%d\n",
			__func__, reg_val, inst_idx);
		goto GET_VPU_INST_IDX_HANDLED;
	}

	if (int_reason & (1 << INT_DEC_PIC))
	{
		reg_val = (done_inst & 0xffff);
		inst_idx = get_inst_idx(reg_val);
		*reason = (1 << INT_DEC_PIC);
		enc_pr(LOG_INFO,
			"%s, RET_QUEUE_CMD_DONE DEC_PIC val=0x%x, idx=%d\n",
			__func__, reg_val, inst_idx);

		if (int_reason & (1 << INT_ENC_LOW_LATENCY))
		{
			u32 ll_inst_idx;
			reg_val = (done_inst >> 16);
			ll_inst_idx = get_inst_idx(reg_val);
			if (ll_inst_idx == inst_idx)
				*reason = ((1 << INT_DEC_PIC)
					| (1 << INT_ENC_LOW_LATENCY));
			enc_pr(LOG_DEBUG, "%s, LOW_LATENCY ", __func__);
			enc_pr(LOG_DEBUG, "val=0x%x, idx=%d, ll_idx=%d\n",
				reg_val, inst_idx, ll_inst_idx);
		}
		goto GET_VPU_INST_IDX_HANDLED;
	}

	if (int_reason & (1 << INT_ENC_SET_PARAM))
	{
		reg_val = (seq_inst & 0xffff);
		inst_idx = get_inst_idx(reg_val);
		*reason = (1 << INT_ENC_SET_PARAM);
		enc_pr(LOG_DEBUG,
			"%s, RET_QUEUE_CMD_DONE SET_PARAM val=0x%x, idx=%d\n",
			__func__, reg_val, inst_idx);
		goto GET_VPU_INST_IDX_HANDLED;
	}

#ifdef SUPPORT_SOURCE_RELEASE_INTERRUPT
	if (int_reason & (1 << INT_ENC_SRC_RELEASE))
	{
		reg_val = (done_inst & 0xffff);
		inst_idx = get_inst_idx(reg_val);
		*reason = (1 << INT_ENC_SRC_RELEASE);
		enc_pr(LOG_DEBUG,
			"%s, RET_QUEUE_CMD_DONE SRC_RELEASE ",
			__func__);
		enc_pr(LOG_DEBUG,
			"val=0x%x, idx=%d\n", reg_val, inst_idx);
		goto GET_VPU_INST_IDX_HANDLED;
	}
#endif

	if (int_reason & (1 << INT_ENC_LOW_LATENCY))
	{
		reg_val = (done_inst >> 16);
		inst_idx = get_inst_idx(reg_val);
		*reason = (1 << INT_ENC_LOW_LATENCY);
		enc_pr(LOG_DEBUG,
			"%s, RET_QUEUE_CMD_DONE LOW_LATENCY ",
			__func__);
		enc_pr(LOG_DEBUG,
			"val=0x%x, idx=%d\n", reg_val, inst_idx);
		goto GET_VPU_INST_IDX_HANDLED;
	}

	inst_idx = -1;
	*reason = 0;
	enc_pr(LOG_DEBUG,
		"%s, UNKNOWN INTERRUPT REASON: 0x%08x\n",
		__func__, int_reason);

GET_VPU_INST_IDX_HANDLED:

	enc_pr(LOG_INFO, "[-]%s, inst_idx=%d. *reason=0x%x\n", __func__,
		inst_idx, *reason);

	return inst_idx;
}

static void multienc_isr_tasklet(ulong data)
{
	u32 intr_reason;
	u32 inst_index;
	struct vpu_drv_context_t *dev = (struct vpu_drv_context_t *)data;

	/* notify the interrupt to user space */
	if (dev->async_queue) {
		enc_pr(LOG_ALL, "kill_fasync e %s\n", __func__);
		kill_fasync(&dev->async_queue, SIGIO, POLL_IN);
	}

	for (inst_index=0; inst_index < MAX_NUM_INSTANCE; inst_index++) {
		intr_reason = dev->interrupt_flag[inst_index];
		if (intr_reason) {
			dev->interrupt_flag[inst_index] = 0;
			enc_pr(LOG_INFO,
				"isr_tasklet intr:0x%08x ins_index %d\n",
				intr_reason, inst_index);
			s_interrupt_flag[inst_index] = 1;
			wake_up_interruptible(&s_interrupt_wait_q[inst_index]);
		}
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
	u32 intr_reason;
	u32 intr_inst_index;

	enc_pr(LOG_ALL, "[+]%s\n", __func__);

#ifdef VPU_IRQ_CONTROL
	disable_irq_nosync(s_vpu_irq);
#endif

	intr_inst_index = 0;
	intr_reason = 0;

	for (core = 0; core < MAX_NUM_VPU_CORE; core++) {
		u32 empty_inst;
		u32 done_inst;
		u32 seq_inst;
		u32 i, reason, reason_clr;
		if (s_bit_firmware_info[core].size == 0) {
			/* it means that we didn't get an information
			 *	the current core from API layer.
			 *	No core activated.
			 */
			enc_pr(LOG_ERROR,
				"s_bit_firmware_info[core].size is zero\n");
			continue;
		}
		if (ReadVpuRegister(VP5_VPU_VPU_INT_STS) == 0)
			continue;
		reason = ReadVpuRegister(VP5_VPU_INT_REASON);
		empty_inst = ReadVpuRegister(VP5_RET_BS_EMPTY_INST);
		done_inst = ReadVpuRegister(VP5_RET_QUEUE_CMD_DONE_INST);
		seq_inst = ReadVpuRegister(VP5_RET_SEQ_DONE_INSTANCE_INFO);
		reason_clr  = reason;
		enc_pr(LOG_INFO, "irq reason=0x%x, inst: empty=0x%x,",
		       reason, empty_inst);
		enc_pr(LOG_INFO, " done=0x%x, seq_inst=0x%x\n",
		       done_inst, seq_inst);
		for (i = 0; i < MAX_NUM_INSTANCE; i++) {
			if (empty_inst == 0 && done_inst == 0 && seq_inst == 0)
				break;

			intr_reason = reason;
			intr_inst_index = get_vpu_inst_idx(dev,
							   &intr_reason,
							   empty_inst,
							   done_inst,
							   seq_inst);

			enc_pr(LOG_INFO, "irq instance: %d", intr_inst_index);
			enc_pr(LOG_INFO, " reason: %08x", intr_reason);
			enc_pr(LOG_INFO, " empty_inst: %08x", empty_inst);
			enc_pr(LOG_INFO, " done_inst: %08x", done_inst);
			enc_pr(LOG_INFO, " seq_inst: %08x\n", seq_inst);

			if (intr_inst_index < MAX_NUM_INSTANCE) {
				if (intr_reason == (1 << INT_BSBUF_EMPTY)) {
					empty_inst = empty_inst &
						~(1 << intr_inst_index);
					WriteVpuRegister(VP5_RET_BS_EMPTY_INST,
						empty_inst);
					if (empty_inst == 0)
						reason &=
							~(1 << INT_BSBUF_EMPTY);
					enc_pr(LOG_INFO,
						"%s, RET_BS_EMPTY_INST Clear ",
						__func__);
					enc_pr(LOG_INFO,
						"inst=0x%x, index=%d\n",
						empty_inst, intr_inst_index);
				}
				if (intr_reason == (1 << INT_DEC_PIC)) {
					done_inst = done_inst &
						~(1 << intr_inst_index);
					WriteVpuRegister(
						VP5_RET_QUEUE_CMD_DONE_INST,
						done_inst);
					if (done_inst == 0)
						reason &= ~(1 << INT_DEC_PIC);
					enc_pr(LOG_INFO,
						"%s, RET_QUEUE_CMD_DONE ",
						__func__);
					enc_pr(LOG_INFO,
						"inst=0x%x, index=%d\n",
						done_inst, intr_inst_index);
				}
				if ((intr_reason == (1 << INT_INIT_SEQ)) ||
				    (intr_reason ==
				    (1 << INT_ENC_SET_PARAM))) {
					seq_inst = seq_inst &
						~(1 << intr_inst_index);
					WriteVpuRegister
						(VP5_RET_SEQ_DONE_INSTANCE_INFO,
						 seq_inst);
					if (seq_inst == 0)
						reason &= ~(1 << INT_INIT_SEQ |
							1 << INT_ENC_SET_PARAM);

					enc_pr(LOG_INFO, "%s, RET_", __func__);
					enc_pr(LOG_INFO,
					       "SEQ_DONE_INSTANCE_INFO inst");
					enc_pr(LOG_INFO,
					       "=0x%x, intr_inst_index=%d\n",
						done_inst, intr_inst_index);
				}
				if (intr_reason == (1 << INT_ENC_LOW_LATENCY)) {
					done_inst = (done_inst >> 16);
					done_inst = done_inst
						& ~(1 << intr_inst_index);
					done_inst = (done_inst << 16);
					WriteVpuRegister(
						VP5_RET_QUEUE_CMD_DONE_INST,
						done_inst);
					if (done_inst == 0)
						reason &=
						~(1 << INT_ENC_LOW_LATENCY);

					enc_pr(LOG_INFO,
						"%s, LOW_LATENCY Clear ",
						__func__);
					enc_pr(LOG_INFO,
						"inst=0x%x, index=%d\n",
						done_inst, intr_inst_index);
				}
				if (!kfifo_is_full(
					&s_interrupt_pending_q[intr_inst_index]
					)) {
					if (intr_reason ==
						((1 << INT_ENC_PIC) |
						(1 <<
						INT_ENC_LOW_LATENCY))) {
						u32 ll_intr_reason =
							(1 <<
							INT_ENC_PIC);
						kfifo_in_spinlocked(
							&s_interrupt_pending_q[
							intr_inst_index],
							&ll_intr_reason,
							sizeof(u32),
							&s_kfifo_lock);
					} else
						kfifo_in_spinlocked(
							&s_interrupt_pending_q[
							intr_inst_index],
							&intr_reason,
							sizeof(u32),
							&s_kfifo_lock);
				}
				else {
					enc_pr(LOG_ERROR, "kfifo_is_full ");
					enc_pr(LOG_ERROR,
						"kfifo_count %d index %d\n",
						kfifo_len(
						&s_interrupt_pending_q[
						intr_inst_index]),
						intr_inst_index);
				}
				dev->interrupt_flag[intr_inst_index] =
					intr_reason;
			}
			else {
				enc_pr(LOG_ERROR,
					"intr_inst_index (%d) is wrong \n",
					intr_inst_index);
			}
			enc_pr(LOG_INFO,
			       "intr_reason: 0x%08x\n", intr_reason);
		}
		if (reason != 0) {
			enc_pr(LOG_ERROR, "INTERRUPT REASON REMAINED: %08x\n",
			       reason);
		}
		WriteVpuRegister(VP5_VPU_INT_REASON_CLEAR, reason_clr);
		WriteVpuRegister(VP5_VPU_VINT_CLEAR, 0x1);
	}

	tasklet_schedule(&multienc_tasklet);
	enc_pr(LOG_ALL, "[-]%s\n", __func__);
	return IRQ_HANDLED;
}

#define RESETCTRL_RESET1_LEVEL (0xfe000044)

static void hw_reset(bool reset)
{
	void __iomem *reset_addr;
	uint32_t val;

	reset_addr = ioremap_nocache(RESETCTRL_RESET1_LEVEL, 8);
	if (reset_addr == NULL) {
		enc_pr(LOG_ERROR, "%s: Failed to ioremap\n", __func__);
		return;
	}

	val = __raw_readl(reset_addr);
	if (reset)
		val &= ~(1 << 28);
	else
		val |= (1 << 28);
	__raw_writel(val, reset_addr);

	mdelay(5);

	iounmap(reset_addr);
	if (reset)
		enc_pr(LOG_INFO, "%s:reset\n", __func__);
	else
		enc_pr(LOG_INFO, "%s:release reset\n", __func__);

}

static s32 vpu_open(struct inode *inode, struct file *filp)
{
	bool first_open = false;
	s32 r = 0;

	//enc_pr(LOG_DEBUG, "[+] %s, filp=%lu, %lu, f_count=%lld\n", __func__,
			//(unsigned long)filp, ( ((unsigned long)filp)%8), filp->f_count.counter);
	spin_lock(&s_vpu_lock);
	s_vpu_drv_context.open_count++;
	if (s_vpu_drv_context.open_count == 1) {
		first_open = true;
	} /*else {
		r = -EBUSY;
		s_vpu_drv_context.open_count--;
		spin_unlock(&s_vpu_lock);
		return r;
	}*/
	filp->private_data = (void *)(&s_vpu_drv_context);
	spin_unlock(&s_vpu_lock);
	if (first_open && !use_reserve) {
#ifdef CONFIG_CMA
		s_video_memory.size = cma_cfg_size;
		s_video_memory.phys_addr = (ulong)codec_mm_alloc_for_dma(VPU_DEV_NAME, cma_cfg_size >> PAGE_SHIFT, 0, 0);

		if (s_video_memory.phys_addr) {
			enc_pr(LOG_DEBUG, "allocating phys 0x%lx, ", s_video_memory.phys_addr);
			enc_pr(LOG_DEBUG, "virt addr 0x%lx, size %dk\n", s_video_memory.base, s_video_memory.size >> 10);

			if (vmem_init(&s_vmem, s_video_memory.phys_addr, s_video_memory.size) < 0) {
				enc_pr(LOG_ERROR, "fail to init vmem system\n");
				r = -ENOMEM;

				codec_mm_free_for_dma(VPU_DEV_NAME, (u32)s_video_memory.phys_addr);
				vmem_exit(&s_vmem);
				memset(&s_video_memory, 0, sizeof(struct vpudrv_buffer_t));
				memset(&s_vmem, 0, sizeof(struct video_mm_t));
			}
		} else {
			enc_pr(LOG_ERROR, "Failed to alloc dma buffer %s, phys:0x%lx\n", VPU_DEV_NAME, s_video_memory.phys_addr);

			if (s_video_memory.phys_addr)
				codec_mm_free_for_dma(VPU_DEV_NAME, (u32)s_video_memory.phys_addr);

			s_video_memory.phys_addr = 0;
			r = -ENOMEM;
		}
#else
		enc_pr(LOG_ERROR, "No CMA and reserved memory for MultiEnc!!!\n");
		r = -ENOMEM;
#endif
	} else if (!s_video_memory.phys_addr) {
		enc_pr(LOG_ERROR, "MultiEnc memory is not malloced yet wait & retry!\n");
		r = -EBUSY;
	}

	if (first_open) {
		if ((s_vpu_irq >= 0) && (s_vpu_irq_requested == false)) {
			s32 err;

			err = request_irq(s_vpu_irq, vpu_irq_handler, 0, "MultiEnc-irq", (void *)(&s_vpu_drv_context));

			if (err) {
				enc_pr(LOG_ERROR, "Failed to register irq handler\n");
				spin_lock(&s_vpu_lock);
				s_vpu_drv_context.open_count--;
				spin_unlock(&s_vpu_lock);
				return -EFAULT;
			}

			s_vpu_irq_requested = true;
		}

		/* enable vpu clks and power*/
		vpu_clk_enable(&s_vpu_clks);
	}

	if (r != 0) {
		spin_lock(&s_vpu_lock);
		s_vpu_drv_context.open_count--;
		spin_unlock(&s_vpu_lock);
	}

	enc_pr(LOG_DEBUG, "[-] %s, ret: %d\n", __func__, r);
	return r;
}
ulong phys_addrY;
ulong phys_addrU;
ulong phys_addrV;

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

				list_for_each_entry_safe(vbp, n,
					&s_vbp_head, list) {
					if (vbp->vb.phys_addr
						== vb.phys_addr) {
						spin_lock(&s_vpu_lock);
						list_del(&vbp->list);
						spin_unlock(&s_vpu_lock);
						kfree(vbp);
						break;
					}
				}
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
				/*TODO check equal condition*/
				list_for_each_entry_safe(vbp, n,
					&s_vbp_head, list) {
					if ((compat_ulong_t)vbp->vb.phys_addr
						== buf32.phys_addr) {
						spin_lock(&s_vpu_lock);
						list_del(&vbp->list);
						spin_unlock(&s_vpu_lock);
						kfree(vbp);
						break;
					}
				}
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
				"[+]VDI_IOCTL_GET_RESERVED_VIDEO_MEMORY\n");
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
				"[-]VDI_IOCTL_GET_RESERVED_VIDEO_MEMORY\n");
		}
		break;
#ifdef CONFIG_COMPAT
	case VDI_IOCTL_GET_RESERVED_VIDEO_MEMORY_INFO32:
		{
			struct compat_vpudrv_buffer_t buf32;

			enc_pr(LOG_ALL,
				"[+]VDI_IOCTL_GET_RESERVED_VIDEO_MEMORY32\n");
			memset(&buf32, 0, sizeof(struct compat_vpudrv_buffer_t));
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
				"[-]VDI_IOCTL_GET_RESERVED_VIDEO_MEMORY32\n");
		}
		break;
#endif
	case VDI_IOCTL_WAIT_INTERRUPT:
		{
			struct vpudrv_intr_info_t info;
			u32 intr_inst_index;
			u32 intr_reason_in_q;
			u32 interrupt_flag_in_q;

			enc_pr(LOG_ALL,
				"[+]VDI_IOCTL_WAIT_INTERRUPT\n");
			ret = copy_from_user(&info,
				(struct vpudrv_intr_info_t *)arg,
				sizeof(struct vpudrv_intr_info_t));
			if (ret != 0)
				return -EFAULT;
			intr_inst_index = info.intr_inst_index;

			intr_reason_in_q = 0;
			if (intr_inst_index >= MAX_NUM_INSTANCE)
			{
				enc_pr(LOG_ALL,
					"error, intr_inst_index is invalid !\n");
				return -EFAULT;
			}
			interrupt_flag_in_q = kfifo_out_spinlocked(
				&s_interrupt_pending_q[intr_inst_index],
				&intr_reason_in_q, sizeof(u32),
				&s_kfifo_lock);
			if (interrupt_flag_in_q > 0)
			{
				dev->interrupt_reason[intr_inst_index] =
					intr_reason_in_q;
				enc_pr(LOG_ALL,
					"Intr Remain in Q: inst_index= %d, ",
					intr_inst_index);
				enc_pr(LOG_ALL, "reason= 0x%x, flag= %d\n",
					intr_reason_in_q,
					interrupt_flag_in_q);
				goto INTERRUPT_REMAIN_IN_QUEUE;
			}

			ret = wait_event_interruptible_timeout(
				s_interrupt_wait_q[intr_inst_index],
				s_interrupt_flag[intr_inst_index] != 0,
				msecs_to_jiffies(info.timeout));
			if (!ret) {
				ret = -ETIME;
				break;
			}

			if (signal_pending(current)) {
				ret = -ERESTARTSYS;
				break;
			}

			intr_reason_in_q = 0;
			interrupt_flag_in_q = kfifo_out_spinlocked(
				&s_interrupt_pending_q[intr_inst_index],
				&intr_reason_in_q, sizeof(u32), &s_kfifo_lock);
			if (interrupt_flag_in_q > 0) {
				dev->interrupt_reason[intr_inst_index] =
					intr_reason_in_q;
			}
			else {
				dev->interrupt_reason[intr_inst_index] = 0;
			}
			enc_pr(LOG_INFO,
				"inst_index(%d),s_interrupt_flag(%d), ",
				intr_inst_index,
				s_interrupt_flag[intr_inst_index]);
			enc_pr(LOG_INFO,
				"reason(0x%08lx)\n",
				dev->interrupt_reason[intr_inst_index]);

INTERRUPT_REMAIN_IN_QUEUE:
			info.intr_reason =
				dev->interrupt_reason[intr_inst_index];
			s_interrupt_flag[intr_inst_index] = 0;
			dev->interrupt_reason[intr_inst_index] = 0;

#ifdef VPU_IRQ_CONTROL
			enable_irq(s_vpu_irq);
#endif
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
			ret = down_interruptible(&s_vpu_sem);
			if (ret != 0)
				return -EFAULT;
			vpu_clk_config(clkgate);
			up(&s_vpu_sem);
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
						sizeof(struct
						vpudrv_buffer_t));
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

			enc_pr(LOG_ALL,
				"[+]VDI_IOCTL_GET_INSTANCE_POOL32\n");
			memset(&buf32, 0, sizeof(struct compat_vpudrv_buffer_t));
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
					sizeof(struct
					compat_vpudrv_buffer_t));
				ret = (ret != 0) ? -EFAULT : 0;
			} else {
				ret = copy_from_user(&buf32,
					(struct compat_vpudrv_buffer_t *)arg,
					sizeof(struct
					compat_vpudrv_buffer_t));
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
						sizeof(struct
						compat_vpudrv_buffer_t));
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
				ret = down_interruptible(&s_vpu_sem);
				if (ret != 0)
					break;
				if (vpu_alloc_dma_buffer(
					&s_common_memory) != -1) {
					ret = copy_to_user((void __user *)arg,
						&s_common_memory,
						sizeof(struct
						vpudrv_buffer_t));
					if (ret != 0)
						ret = -EFAULT;
				} else
					ret = -EFAULT;
				up(&s_vpu_sem);
			}
			enc_pr(LOG_ALL,
				"[-]VDI_IOCTL_GET_COMMON_MEMORY\n");
		}
		break;
#ifdef CONFIG_COMPAT
	case VDI_IOCTL_GET_COMMON_MEMORY32:
		{
			struct compat_vpudrv_buffer_t buf32;

			enc_pr(LOG_ALL,
				"[+]VDI_IOCTL_GET_COMMON_MEMORY32\n");

			memset(&buf32, 0, sizeof(struct compat_vpudrv_buffer_t));
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
				ret = down_interruptible(&s_vpu_sem);
				if (ret != 0)
					break;
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
						&buf32, sizeof(struct
						compat_vpudrv_buffer_t));
					if (ret != 0)
						ret = -EFAULT;
				} else
					ret = -EFAULT;
				up(&s_vpu_sem);
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

			enc_pr(LOG_DEBUG,
				"[+]VDI_IOCTL_OPEN_INSTANCE\n");

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
			ret = down_interruptible(&s_vpu_sem);
			if (ret != 0)
			{
				kfree(vil);
				break;
			}
			vil->inst_idx = inst_info.inst_idx;
			vil->core_idx = inst_info.core_idx;
			vil->filp = filp;
			/* counting the current open instance number */
			inst_info.inst_open_count = 0;
			spin_lock(&s_vpu_lock);
			list_add(&vil->list, &s_inst_list_head);
			list_for_each_entry_safe(vil, n,
				&s_inst_list_head, list) {
				if (vil->core_idx == inst_info.core_idx)
					inst_info.inst_open_count++;
			}
			if (inst_info.inst_idx >= MAX_NUM_INSTANCE)
			{
				enc_pr(LOG_ALL,
					"error, inst_info.inst_idx is invalid !\n");
				kfree(vil);
				spin_unlock(&s_vpu_lock);
				return -EFAULT;
			}
			kfifo_reset(
				&s_interrupt_pending_q[inst_info.inst_idx]);

			 /* flag just for that vpu is in opened or closed */
			s_vpu_open_ref_count++;
			spin_unlock(&s_vpu_lock);
			up(&s_vpu_sem);
			if (copy_to_user((void __user *)arg,
				&inst_info,
				sizeof(struct vpudrv_inst_info_t))) {
				kfree(vil);
				return -EFAULT;
			}

			enc_pr(LOG_DEBUG,
				"[-]VDI_IOCTL_OPEN_INSTANCE ");
			enc_pr(LOG_DEBUG,
				"core_idx = %d, inst_idx = %d, ",
				(u32)inst_info.core_idx,
				(u32)inst_info.inst_idx);
			enc_pr(LOG_DEBUG,
				"s_vpu_open_ref_count = %d, ",
				s_vpu_open_ref_count);
			enc_pr(LOG_DEBUG,
				"inst_open_count = %d\n",
				inst_info.inst_open_count);
		}
		break;
	case VDI_IOCTL_CLOSE_INSTANCE:
		{
			struct vpudrv_inst_info_t inst_info;
			struct vpudrv_instanace_list_t *vil, *n;
			u32 found = 0;

			enc_pr(LOG_ALL,
				"[+]VDI_IOCTL_CLOSE_INSTANCE\n");
			if (copy_from_user(&inst_info,
				(struct vpudrv_inst_info_t *)arg,
				sizeof(struct vpudrv_inst_info_t)))
				return -EFAULT;
			ret = down_interruptible(&s_vpu_sem);
			if (ret != 0)
				break;

			list_for_each_entry_safe(vil, n,
				&s_inst_list_head, list) {
				if (vil->inst_idx == inst_info.inst_idx &&
					vil->core_idx == inst_info.core_idx) {
					spin_lock(&s_vpu_lock);
					list_del(&vil->list);
					spin_unlock(&s_vpu_lock);
					kfree(vil);
					found = 1;
					break;
				}
			}

			if (found == 0) {
				up(&s_vpu_sem);
				return -EINVAL;
			}

			/* counting the current open instance number */
			inst_info.inst_open_count = 0;
			list_for_each_entry_safe(vil, n,
				&s_inst_list_head, list) {
				if (vil->core_idx == inst_info.core_idx)
					inst_info.inst_open_count++;
			}

			kfifo_reset(
				&s_interrupt_pending_q[inst_info.inst_idx]);
			/* flag just for that vpu is in opened or closed */
			spin_lock(&s_vpu_lock);
			s_vpu_open_ref_count--;
			spin_unlock(&s_vpu_lock);
			up(&s_vpu_sem);
			if (copy_to_user((void __user *)arg,
				&inst_info,
				sizeof(struct vpudrv_inst_info_t)))
				return -EFAULT;

			enc_pr(LOG_DEBUG,
				"[-]VDI_IOCTL_CLOSE_INSTANCE ");
			enc_pr(LOG_DEBUG,
				"core_idx= %d, inst_idx= %d, ",
				(u32)inst_info.core_idx,
				(u32)inst_info.inst_idx);
			enc_pr(LOG_DEBUG,
				"s_vpu_open_ref_count= %d, ",
				s_vpu_open_ref_count);
			enc_pr(LOG_DEBUG,
				"inst_open_count= %d\n",
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
				"[-]VDI_IOCTL_GET_INSTANCE_NUM ");
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

			enc_pr(LOG_ALL,
				"[+]VDI_IOCTL_GET_REGISTER_INFO32\n");

			memset(&buf32, 0, sizeof(struct compat_vpudrv_buffer_t));
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
					if (((compat_ulong_t)vb.phys_addr <=
						buf32.phys_addr) &&
						(((compat_ulong_t)vb.phys_addr
							+ vb.size)
						> buf32.phys_addr)
						&& find == false){
						cached = vb.cached;
						find = true;
						break;
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
					if ((vb.phys_addr <= buf.phys_addr)
						&& ((vb.phys_addr + vb.size)
							> buf.phys_addr)
						&& find == false){
						cached = vb.cached;
						find = true;
						break;
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
	case VDI_IOCTL_CACHE_INV_BUFFER:
		{
			struct vpudrv_buffer_t buf;
			struct vpudrv_buffer_pool_t *pool, *n;
			struct vpudrv_buffer_t vb;
			bool find = false;
			u32 cached = 0;
			enc_pr(LOG_ALL,
				"[+]VDI_IOCTL_CACHE_INV_BUFFER\n");

			ret = copy_from_user(&buf,
				(struct vpudrv_buffer_t *)arg,
				sizeof(struct vpudrv_buffer_t));
			if (ret)
				return -EFAULT;

			spin_lock(&s_vpu_lock);
			list_for_each_entry_safe(pool, n, &s_vbp_head, list) {
				if (pool->filp == filp) {
					vb = pool->vb;
					if ((vb.phys_addr <= buf.phys_addr) && ((vb.phys_addr + vb.size) > buf.phys_addr) && find == false) {
						cached = vb.cached;
						find = true;
						break;
					}
				}
			}
			spin_unlock(&s_vpu_lock);
			if (find && cached) {
				//pr_err("[%d]doing cache flush for %p~%p\n", __LINE__, (long)(buf.phys_addr), (long)(buf.phys_addr+buf.size));
				cache_flush((u32)buf.phys_addr,(u32)buf.size);
			}

			enc_pr(LOG_ALL,"[-]VDI_IOCTL_CACHE_INV_BUFFER\n");
		}
		break;
#ifdef CONFIG_COMPAT
	case VDI_IOCTL_CACHE_INV_BUFFER32:
		{
			struct compat_vpudrv_buffer_t buf32;
			struct vpudrv_buffer_pool_t *pool, *n;
			struct vpudrv_buffer_t vb;
			bool find = false;
			u32 cached = 0;
			enc_pr(LOG_ALL, "[+]VDI_IOCTL_CACHE_INV_BUFFER32\n");

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
						<= buf32.phys_addr)
					&& (((compat_ulong_t)vb.phys_addr +
						vb.size) > buf32.phys_addr)
					&& find == false){
						cached = vb.cached;
						find = true;
						break;
					}
				}
			}
			spin_unlock(&s_vpu_lock);

			if (find && cached) {
				cache_flush((u32)buf32.phys_addr, (u32)buf32.size);

				if (dump_es) {
					pr_err("dump es frame, size=%u\n", (u32)buf32.size);
					dump_data((u32)buf32.phys_addr, (u32)buf32.size);
				}
			}
			enc_pr(LOG_INFO, "[-]VVDI_IOCTL_CACHE_INV_BUFFER32\n");
		}
		break;
#endif
	case VDI_IOCTL_CONFIG_DMA:
		{
			struct vpudrv_dma_buf_info_t dma_info;
			enc_pr(LOG_DEBUG,
				"[+]VDI_IOCTL_CONFIG_DMA_BUF\n");

			if (copy_from_user(&dma_info,
				(struct vpudrv_dma_buf_info_t *)arg,
				sizeof(struct vpudrv_dma_buf_info_t)))
				return -EFAULT;
			ret = down_interruptible(&s_vpu_sem);
			if (ret != 0)
				break;
			if (vpu_src_addr_config(&dma_info, filp)) {
				up(&s_vpu_sem);
				enc_pr(LOG_ERROR,
						"src addr config error\n");
				ret = -EFAULT;
				break;
			}
			up(&s_vpu_sem);
			ret = copy_to_user((void __user *)arg,
				&dma_info,
				sizeof(struct vpudrv_dma_buf_info_t));
			if (ret) {
				ret = -EFAULT;
				break;
			}
			enc_pr(LOG_DEBUG,
				"[-]VDI_IOCTL_CONFIG_DMA_BUF %d, %d, %d\n",
				dma_info.fd[0],
				dma_info.fd[1],
				dma_info.fd[2]);
		}
		break;

		//hoan add for canvas
		case VDI_IOCTL_READ_CANVAS:
		{
			struct vpudrv_dma_buf_canvas_info_t dma_info;

			struct canvas_s dst ;
		    u32 canvas = 0;

			if (copy_from_user(&dma_info,
					(struct vpudrv_dma_buf_canvas_info_t *)arg,
					sizeof(struct vpudrv_dma_buf_canvas_info_t)))
			{
				return -EFAULT;
			}

			ret = down_interruptible(&s_vpu_sem);
			if (ret != 0)
			{
				up(&s_vpu_sem);
				break;
			}

			canvas = dma_info.canvas_index;
			enc_pr(LOG_DEBUG,"[+]VDI_IOCTL_READ_CANVAS,canvas = 0x%x\n",canvas);
			if (canvas & 0xff)
			{
				canvas_read(canvas & 0xff, &dst);
				dma_info.phys_addr[0] = dst.addr;

				if ((canvas & 0xff00) >> 8)
				{
					canvas_read((canvas & 0xff00) >> 8, &dst);
					dma_info.phys_addr[1] = dst.addr;

				}

				if ((canvas & 0xff0000) >> 16)
				{
					canvas_read((canvas & 0xff0000) >> 16, &dst);
					dma_info.phys_addr[2] = dst.addr;
				}

				enc_pr(LOG_DEBUG,"[+]VDI_IOCTL_READ_CANVAS,phys_addr[0] = 0x%lx,phys_addr[1] = 0x%lx,phys_addr[2] = 0x%lx\n",dma_info.phys_addr[0],dma_info.phys_addr[1],dma_info.phys_addr[2]);

			}
			else
			{
				dma_info.phys_addr[0] = 0;
				dma_info.phys_addr[1] = 0;
				dma_info.phys_addr[2] = 0;
			}
			up(&s_vpu_sem);
			#if 0
			dma_info.phys_addr[0] = phys_addrY;
			dma_info.phys_addr[1] = phys_addrU;
			dma_info.phys_addr[2] = phys_addrV;
			#endif

			ret = copy_to_user((void __user *)arg,
				&dma_info,
				sizeof(struct vpudrv_dma_buf_canvas_info_t));

			enc_pr(LOG_DEBUG,"[-]VDI_IOCTL_READ_CANVAS,copy_to_user End\n");
			if (ret)
			{
				ret = -EFAULT;
				break;
			}

		}
		break;
		//end

#ifdef CONFIG_COMPAT
	case VDI_IOCTL_CONFIG_DMA32:
		{
			struct vpudrv_dma_buf_info_t dma_info;
			struct compat_vpudrv_dma_buf_info_t dma_info32;
			enc_pr(LOG_ALL,
				"[+]VDI_IOCTL_CONFIG_DMA_BUF32\n");

			if (copy_from_user(&dma_info32,
				(struct compat_vpudrv_dma_buf_info_t *)arg,
				sizeof(struct compat_vpudrv_dma_buf_info_t)))
				return -EFAULT;
			dma_info.num_planes = dma_info32.num_planes;
			dma_info.fd[0] = (int) dma_info32.fd[0];
			dma_info.fd[1] = (int) dma_info32.fd[1];
			dma_info.fd[2] = (int) dma_info32.fd[2];
			ret = down_interruptible(&s_vpu_sem);
			if (ret != 0)
				break;
			if (vpu_src_addr_config(&dma_info, filp)) {
				up(&s_vpu_sem);
				enc_pr(LOG_ERROR,
						"src addr config error\n");
				ret = -EFAULT;
				break;
			}
			up(&s_vpu_sem);
			dma_info32.phys_addr[0] =
				(compat_ulong_t) dma_info.phys_addr[0];
			dma_info32.phys_addr[1] =
				(compat_ulong_t) dma_info.phys_addr[1];
			dma_info32.phys_addr[2] =
				(compat_ulong_t) dma_info.phys_addr[2];
			ret = copy_to_user((void __user *)arg,
				&dma_info32,
				sizeof(struct compat_vpudrv_dma_buf_info_t));
			if (ret) {
				ret = -EFAULT;
				break;
			}
			enc_pr(LOG_ALL,
				"[-]VDI_IOCTL_CONFIG_DMA_BUF32 %d, %d, %d\n",
				dma_info.fd[0],
				dma_info.fd[1],
				dma_info.fd[2]);
		}
		break;

#if 1

	//hoan add for canvas
	case VDI_IOCTL_READ_CANVAS32:
	{
		struct vpudrv_dma_buf_canvas_info_t dma_info;
		struct compat_vpudrv_dma_buf_canvas_info_t dma_info32;
		struct canvas_s dst;
		u32 canvas = 0;

		if (copy_from_user(&dma_info32,
				(struct compat_vpudrv_dma_buf_canvas_info_t *)arg,
				sizeof(struct compat_vpudrv_dma_buf_canvas_info_t)))
		{
			return -EFAULT;
		}

		ret = down_interruptible(&s_vpu_sem);
		if (ret != 0)
		{
			up(&s_vpu_sem);
			break;
		}

		canvas = dma_info32.canvas_index;
		enc_pr(LOG_INFO,"[+]VDI_IOCTL_READ_CANVAS32,canvas = 0x%x\n",dma_info32.canvas_index);
		if (canvas & 0xff)
		{
			canvas_read(canvas & 0xff, &dst);
			dma_info.phys_addr[0] = dst.addr;

			if (dump_input) {
				dump_raw_input(&dst);
			}

			if ((canvas & 0xff00) >> 8)
			{
				canvas_read((canvas & 0xff00) >> 8, &dst);
				dma_info.phys_addr[1] = dst.addr;
				if (dump_input) {
					dump_raw_input(&dst);
				}
			}

			if ((canvas & 0xff0000) >> 16)
			{
				canvas_read((canvas & 0xff0000) >> 16, &dst);
				dma_info.phys_addr[2] = dst.addr;
				if (dump_input) {
					dump_raw_input(&dst);
				}
			}

			enc_pr(LOG_INFO,"VDI_IOCTL_READ_CANVAS32_1,phys_addr[0] = 0x%lx,phys_addr[1] = 0x%lx,phys_addr[2] = 0x%lx\n",
				dma_info.phys_addr[0],dma_info.phys_addr[1],dma_info.phys_addr[2]);
		}
		else
		{
			dma_info.phys_addr[0] = 0;
			dma_info.phys_addr[1] = 0;
			dma_info.phys_addr[2] = 0;
		}

		up(&s_vpu_sem);

		dma_info32.phys_addr[0] =  (compat_ulong_t)dma_info.phys_addr[0];
		dma_info32.phys_addr[1] =  (compat_ulong_t)dma_info.phys_addr[1];
		dma_info32.phys_addr[2] =  (compat_ulong_t)dma_info.phys_addr[2];

		enc_pr(LOG_INFO,"VDI_IOCTL_READ_CANVAS32_2,phys_addr[0] = 0x%lx,phys_addr[1] = 0x%lx,phys_addr[2] = 0x%lx\n",dma_info.phys_addr[0],  dma_info.phys_addr[1],  dma_info.phys_addr[2]);
		enc_pr(LOG_INFO,"VDI_IOCTL_READ_CANVAS32_3,phys_addr[0] = 0x%x,phys_addr[1] = 0x%x,phys_addr[2] = 0x%x\n",   dma_info32.phys_addr[0],dma_info32.phys_addr[1],dma_info32.phys_addr[2]);

		ret = copy_to_user((void __user *)arg,
			&dma_info32,
			sizeof(struct compat_vpudrv_dma_buf_canvas_info_t));

		enc_pr(LOG_INFO,"[-]VDI_IOCTL_READ_CANVAS,copy_to_user End\n");
		if (ret)
		{
			ret = -EFAULT;
			break;
		}

	}
	break;
	//end
#endif

	case VDI_IOCTL_UNMAP_DMA32:
		{
			struct vpudrv_dma_buf_info_t dma_info;
			struct compat_vpudrv_dma_buf_info_t dma_info32;

			enc_pr(LOG_ALL,
				"[+]VDI_IOCTL_UNMAP_DMA32\n");

			if (copy_from_user(&dma_info32,
				(struct compat_vpudrv_dma_buf_info_t *)arg,
				sizeof(struct compat_vpudrv_dma_buf_info_t)))
				return -EFAULT;
			dma_info.num_planes = dma_info32.num_planes;
			dma_info.fd[0] = (int) dma_info32.fd[0];
			dma_info.fd[1] = (int) dma_info32.fd[1];
			dma_info.fd[2] = (int) dma_info32.fd[2];
			dma_info.phys_addr[0] =
				(ulong) dma_info32.phys_addr[0];
			dma_info.phys_addr[1] =
				(ulong) dma_info32.phys_addr[1];
			dma_info.phys_addr[2] =
				(ulong) dma_info32.phys_addr[2];

			ret = down_interruptible(&s_vpu_sem);
			if (ret != 0)
				break;
			if (vpu_src_addr_unmap(&dma_info, filp)) {
				up(&s_vpu_sem);
				enc_pr(LOG_ERROR,
					"dma addr unmap config error\n");
				ret = -EFAULT;
				break;
			}
			up(&s_vpu_sem);
			enc_pr(LOG_ALL,
				"[-]VDI_IOCTL_UNMAP_DMA32\n");
		}
		break;
#endif
	case VDI_IOCTL_UNMAP_DMA:
		{
			struct vpudrv_dma_buf_info_t dma_info;

			enc_pr(LOG_ALL,
				"[+]VDI_IOCTL_UNMAP_DMA\n");

			if (copy_from_user(&dma_info,
				(struct vpudrv_dma_buf_info_t *)arg,
				sizeof(struct vpudrv_dma_buf_info_t)))
				return -EFAULT;
			ret = down_interruptible(&s_vpu_sem);
			if (ret != 0)
				break;
			if (vpu_src_addr_unmap(&dma_info, filp)) {
				up(&s_vpu_sem);
				enc_pr(LOG_ERROR,
					"dma addr unmap config error\n");
				ret = -EFAULT;
				break;
			}
			up(&s_vpu_sem);
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
				"bit_firmware_info allocation error\n");
			return -EFAULT;
		}

		if (copy_from_user(bit_firmware_info, buf, len)) {
			enc_pr(LOG_ERROR,
				"copy_from_user error for firmware_info\n");
			kfree(bit_firmware_info);
			return -EFAULT;
		}

		if (bit_firmware_info->size ==
			sizeof(struct vpu_bit_firmware_info_t)) {
			enc_pr(LOG_INFO,
				"set bit_firmware_info coreIdx= 0x%x, ",
				bit_firmware_info->core_idx);

			enc_pr(LOG_INFO,
				"base_offset = 0x%x, size = 0x%x, ",
				bit_firmware_info->reg_base_offset,
				bit_firmware_info->size);

			enc_pr(LOG_INFO,"bit_code[0] = 0x%x\n",
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
				enc_pr(LOG_ERROR,
					"bit_firmware_info->core_idx invalid\n");
				kfree(bit_firmware_info);
				return -ENODEV;

			}
			memcpy((void *)&s_bit_firmware_info[bit_firmware_info->core_idx], bit_firmware_info, sizeof(struct vpu_bit_firmware_info_t));
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
	u32 open_count;
	s32 i;

	//enc_pr(LOG_DEBUG, "vpu_release filp=%lu, f_counter=%lld\n",
			//(unsigned long)filp, filp->f_count.counter);
	ret = down_interruptible(&s_vpu_sem);

	if (ret == 0) {
		/* found and free the not handled
		buffer by user applications */
		vpu_free_buffers(filp);
		vpu_dma_buf_release(filp);
		/* found and free the not closed
		instance by user applications */
		vpu_free_instances(filp);

		spin_lock(&s_vpu_lock);
		s_vpu_drv_context.open_count--;
		open_count = s_vpu_drv_context.open_count;
		spin_unlock(&s_vpu_lock);

		pr_err("open_count=%u\n", open_count);

		if (open_count == 0) {
			for (i=0; i<MAX_NUM_INSTANCE; i++) {
				kfifo_reset(&s_interrupt_pending_q[i]);
				s_interrupt_flag[i] = 0;
				s_vpu_drv_context.interrupt_reason[i] = 0;
				s_vpu_drv_context.interrupt_flag[i] = 0;
			}
			if (s_instance_pool.base) {
				enc_pr(LOG_DEBUG, "free instance pool\n");
				vfree((const void *)s_instance_pool.base);
				s_instance_pool.base = 0;
			}
			if (s_common_memory.phys_addr) {
				enc_pr(LOG_INFO,
				"vpu_release, s_common_memory 0x%lx\n",
				s_common_memory.phys_addr);
				vpu_free_dma_buffer(&s_common_memory);
				s_common_memory.phys_addr = 0;
			}

			if (s_video_memory.phys_addr && !use_reserve) {
				enc_pr(LOG_DEBUG,
					"vpu_release, s_video_memory 0x%lx\n",
					s_video_memory.phys_addr);
				codec_mm_free_for_dma(
					VPU_DEV_NAME,
					(u32)s_video_memory.phys_addr);
				vmem_exit(&s_vmem);
				memset(&s_video_memory,
					0, sizeof(struct vpudrv_buffer_t));
				memset(&s_vmem,
					0, sizeof(struct video_mm_t));
			}
			if ((s_vpu_irq >= 0)
				&& (s_vpu_irq_requested == true)) {
				free_irq(s_vpu_irq, &s_vpu_drv_context);
				s_vpu_irq_requested = false;
			}

			/* disable vpu clks.*/
			vpu_clk_disable(&s_vpu_clks);
		}
	}
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
	if ((vm->vm_end - vm->vm_start) == (s_vpu_register.size + 1) &&
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
	struct page *page = NULL;
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
	if (IS_ERR_OR_NULL(dbuf)) {
		enc_pr(LOG_ERROR, "failed to get dma buffer,fd %d\n",fd);
		return -EINVAL;
	}

	d_att = dma_buf_attach(dbuf, dev);
	if (IS_ERR(d_att)) {
		enc_pr(LOG_ERROR, "failed to set dma attach\n");
		goto attach_err;
	}

	sg = dma_buf_map_attachment(d_att, dir);
	if (IS_ERR(sg)) {
		enc_pr(LOG_ERROR, "failed to get dma sg\n");
		goto map_attach_err;
	}

	page = sg_page(sg->sgl);
	cfg->paddr = PFN_PHYS(page_to_pfn(page));
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

static s32 vpu_dma_buffer_get_phys(struct vpu_dma_cfg *cfg,
		unsigned long *addr)
{
	int ret = 0;
	if (cfg->paddr == 0)
	{ /* only mapp once */
		ret = vpu_dma_buffer_map(cfg);
		if (ret < 0) {
			enc_pr(LOG_ERROR, "vpu_dma_buffer_map failed\n");
			return ret;
		}
	}
	if (cfg->paddr) *addr = cfg->paddr;
	enc_pr(LOG_INFO,"vpu_dma_buffer_get_phys 0x%lx\n", cfg->paddr);
	return ret;
}

static s32 vpu_src_addr_config(struct vpudrv_dma_buf_info_t *pinfo,
		struct file *filp)
{
	struct vpudrv_dma_buf_pool_t *vbp;
	unsigned long phy_addr;
	struct vpu_dma_cfg *cfg;
	s32 idx, ret = 0;
	if (pinfo->num_planes == 0 || pinfo->num_planes > 3)
		return -EFAULT;

	for (idx = 0; idx < pinfo->num_planes; idx++)
		pinfo->phys_addr[idx] = 0;
	for (idx = 0; idx < pinfo->num_planes; idx++) {
		vbp = kzalloc(sizeof(*vbp), GFP_KERNEL);
		if (!vbp) {
			ret = -ENOMEM;
			break;
		}
		memset(vbp, 0, sizeof(struct vpudrv_dma_buf_pool_t));
		cfg = &vbp->dma_cfg;
		cfg->dir = DMA_TO_DEVICE;
		cfg->fd = pinfo->fd[idx];
		cfg->dev = &(multienc_pdev->dev);
		phy_addr = 0;
		ret = vpu_dma_buffer_get_phys(cfg, &phy_addr);
		if (ret < 0) {
			enc_pr(LOG_ERROR, "import fd %d failed\n", cfg->fd);
			kfree(vbp);
			ret = -1;
			break;
		}
		pinfo->phys_addr[idx] = (ulong) phy_addr;
		vbp->filp = filp;
		spin_lock(&s_dma_buf_lock);
		list_add(&vbp->list, &s_dma_bufp_head);
		spin_unlock(&s_dma_buf_lock);
	}
	enc_pr(LOG_INFO, "vpu_src_addr_config phy_addr 0x%lx, 0x%lx, 0x%lx\n",
		pinfo->phys_addr[0], pinfo->phys_addr[1], pinfo->phys_addr[2]);
	//hoan add for canvas test
	phys_addrY = pinfo->phys_addr[0];
	phys_addrU = pinfo->phys_addr[1];
	phys_addrV = pinfo->phys_addr[2];

	//end
	return ret;
}

static s32 vpu_src_addr_unmap(struct vpudrv_dma_buf_info_t *pinfo,
		struct file *filp)
{
	struct vpudrv_dma_buf_pool_t *pool, *n;
	struct vpu_dma_cfg vb;
	ulong phys_addr;
	s32 plane_idx = 0;
	s32 ret = 0;
	s32 found;

	if (pinfo->num_planes == 0 || pinfo->num_planes > 3)
		return -EFAULT;

	enc_pr(LOG_INFO,
		"dma_unmap planes %d fd: %d-%d-%d, phy_add: 0x%lx-%lx-%lx\n",
		pinfo->num_planes, pinfo->fd[0],pinfo->fd[1], pinfo->fd[2],
		pinfo->phys_addr[0], pinfo->phys_addr[1], pinfo->phys_addr[2]);

	list_for_each_entry_safe(pool, n, &s_dma_bufp_head, list) {
		found = 0;
		if (pool->filp == filp) {
			vb = pool->dma_cfg;
			phys_addr = vb.paddr;
			if (vb.fd == pinfo->fd[0])
			{
				if (phys_addr != pinfo->phys_addr[0]) {
					enc_pr(LOG_ERROR, "dma_unmap plane 0");
					enc_pr(LOG_ERROR, " no match ");
					enc_pr(LOG_ERROR, "0x%lx %lx\n",
						phys_addr, pinfo->phys_addr[0]);
				}
				found = 1;
				plane_idx++;
			}
			else if (vb.fd == pinfo->fd[1]
				&& pinfo->num_planes > 1) {
				if (phys_addr != pinfo->phys_addr[1]) {
					enc_pr(LOG_ERROR, "dma_unmap plane 1");
					enc_pr(LOG_ERROR, " no match ");
					enc_pr(LOG_ERROR, "0x%lx %lx\n",
						phys_addr, pinfo->phys_addr[1]);
				}
				plane_idx++;
				found = 1;
			}
			else if (vb.fd == pinfo->fd[2]
				&& pinfo->num_planes > 2) {
				if (phys_addr != pinfo->phys_addr[2]) {
					enc_pr(LOG_ERROR, "dma_unmap plane 2");
					enc_pr(LOG_ERROR, " no match ");
					enc_pr(LOG_ERROR, "0x%lx %lx\n",
						phys_addr, pinfo->phys_addr[2]);
				}
				plane_idx++;
				found = 1;
			}
			if (found && vb.attach) {
				vpu_dma_buffer_unmap(&vb);
				spin_lock(&s_dma_buf_lock);
				list_del(&pool->list);
				spin_unlock(&s_dma_buf_lock);
				kfree(pool);
			}
		}
	}

	if (plane_idx != pinfo->num_planes) {
		enc_pr(LOG_DEBUG, "dma_unmap fd planes not match\n");
		enc_pr(LOG_DEBUG, " found %d need %d\n",
		       plane_idx, pinfo->num_planes);
	}
	return ret;
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

static ssize_t encode_status_show(struct class *cla,
				struct class_attribute *attr, char *buf)
{
	struct vmem_info_t info;
	char *pbuf = buf;
	s32 open_count;

	spin_lock(&s_vpu_lock);
	open_count = s_vpu_drv_context.open_count;
	spin_unlock(&s_vpu_lock);

	vmem_get_info(&s_vmem, &info);
	pbuf += snprintf(buf, 40, "\nmultienc memory usage info:\n");
	pbuf += snprintf(buf, 120, "Total %ld used %ld free %ld page sz %ld, open_count=%u, in_interrupt=%lu, flag=%u\n",
		info.total_pages*info.page_size,
		info.alloc_pages*info.page_size,
		info.free_pages*info.page_size,
		info.page_size,
		open_count,
		in_interrupt(),
		current->flags & PF_KTHREAD);
	return pbuf - buf;
}

static CLASS_ATTR_RO(encode_status);
static struct attribute *multienc_class_attrs[] = {
	&class_attr_encode_status.attr,
	NULL
};

ATTRIBUTE_GROUPS(multienc_class);

static struct class multienc_class = {
	.name = VPU_CLASS_NAME,
	.class_groups = multienc_class_groups,
};

s32 init_MultiEnc_device(void)
{
	s32  r = 0;

	r = register_chrdev(0, VPU_DEV_NAME, &vpu_fops);
	if (r <= 0) {
		enc_pr(LOG_ERROR, "register multienc device error.\n");
		return  r;
	}
	s_vpu_major = r;

	r = class_register(&multienc_class);
	if (r < 0) {
		enc_pr(LOG_ERROR, "error create multienc class.\n");
		return r;
	}
	s_register_flag = 1;
	multienc_dev = device_create(&multienc_class, NULL,
					MKDEV(s_vpu_major, 0), NULL,
					VPU_DEV_NAME);

	if (IS_ERR(multienc_dev)) {
		enc_pr(LOG_ERROR, "create multienc device error.\n");
		class_unregister(&multienc_class);
		return -1;
	}
	return r;
}

s32 uninit_MultiEnc_device(void)
{
	if (multienc_dev)
		device_destroy(&multienc_class, MKDEV(s_vpu_major, 0));

	if (s_register_flag)
		class_destroy(&multienc_class);
	s_register_flag = 0;

	if (s_vpu_major)
		unregister_chrdev(s_vpu_major, VPU_DEV_NAME);
	s_vpu_major = 0;
	return 0;
}

static s32 multienc_mem_device_init(
	struct reserved_mem *rmem, struct device *dev)
{
	s32 r;

	if (!rmem) {
		enc_pr(LOG_ERROR, "Can not obtain I/O memory, ");
		enc_pr(LOG_ERROR, "will allocate multienc buffer!\n");

		r = -EFAULT;
		return r;
	}

	if ((!rmem->base) ||
		(rmem->size < cma_cfg_size)) {
		enc_pr(LOG_ERROR,
			"memory range error, 0x%lx - 0x%lx\n",
			 (ulong)rmem->base, (ulong)rmem->size);
		r = -EFAULT;
		return r;
	}
	r = 0;
	s_video_memory.size = rmem->size;
	s_video_memory.phys_addr = (ulong)rmem->base;
	enc_pr(LOG_DEBUG, "multienc_mem_device_init %d, 0x%lx\n",
		s_video_memory.size,s_video_memory.phys_addr);

	return r;
}

static s32 vpu_probe(struct platform_device *pdev)
{
	s32 err = 0, irq, reg_count, idx;
	struct resource res;
	struct device_node *np, *child;

	enc_pr(LOG_DEBUG, "vpu_probe\n");

	s_vpu_major = 0;
	use_reserve = false;
	s_vpu_irq = -1;
	cma_pool_size = 0;
	s_vpu_irq_requested = false;
	spin_lock(&s_vpu_lock);
	s_vpu_open_ref_count = 0;
	spin_unlock(&s_vpu_lock);
	multienc_dev = NULL;
	multienc_pdev = NULL;
	s_register_flag = 0;
	memset(&s_video_memory, 0, sizeof(struct vpudrv_buffer_t));
	memset(&s_vpu_register, 0, sizeof(struct vpudrv_buffer_t));
	memset(&s_vmem, 0, sizeof(struct video_mm_t));
	memset(&s_bit_firmware_info[0], 0, sizeof(s_bit_firmware_info));
	memset(&res, 0, sizeof(struct resource));
	memset(&s_fifo_alloc_flag, 0, sizeof(s_fifo_alloc_flag));
	np = pdev->dev.of_node;
	err = of_property_read_u32(np, "config_mm_sz_mb", &cma_cfg_size);

	cma_cfg_size = 100;
	enc_pr(LOG_ERROR, "reset cma_cfg_size to 200");

	if (err) {
		enc_pr(LOG_ERROR, "failed to get config_mm_sz_mb node, use default\n");
		cma_cfg_size = VPU_INIT_VIDEO_MEMORY_SIZE_IN_BYTE;
		err = 0;
	} else
		cma_cfg_size = cma_cfg_size*SZ_1M;

	enc_pr(LOG_INFO, "cma  cfg size  %d\n", cma_cfg_size);
	idx = of_reserved_mem_device_init(&pdev->dev);

	if (idx != 0)
		enc_pr(LOG_DEBUG, "MultiEnc reserved memory config fail.\n");
	else if (s_video_memory.phys_addr)
		use_reserve = true;

	if (use_reserve == false) {
#ifndef CONFIG_CMA
		enc_pr(LOG_ERROR, "MultiEnc reserved memory is invaild, probe fail!\n");
		err = -EFAULT;
		goto ERROR_PROVE_DEVICE;
#else
		cma_pool_size =
			(codec_mm_get_total_size() > (cma_cfg_size)) ?
			(cma_cfg_size) :
			codec_mm_get_total_size();

		enc_pr(LOG_DEBUG, "MultiEnc - cma memory pool size: %d MB\n", (u32)cma_pool_size / SZ_1M);
#endif
	}

	/* get interrupt resource */
	irq = platform_get_irq_byname(pdev, "multienc_irq");

	if (irq < 0) {
		enc_pr(LOG_ERROR, "get MultiEnc irq resource error\n");
		err = -EFAULT;

		goto ERROR_PROVE_DEVICE;
	}

	s_vpu_irq = irq;
	enc_pr(LOG_DEBUG, "MultiEnc -irq: %d\n", s_vpu_irq);

	/* get vpu clks */
	if (vpu_clk_get(&pdev->dev, &s_vpu_clks)) {
		enc_pr(LOG_DEBUG, "get vpu clks fail.\n");
		goto ERROR_PROVE_DEVICE;
	} else
		enc_pr(LOG_DEBUG, "MultiEnc. clock get success\n");

	reg_count = 0;
	np = pdev->dev.of_node;

	for_each_child_of_node(np, child) {
		if (of_address_to_resource(child, 0, &res) || (reg_count > 1)) {
			enc_pr(LOG_ERROR, "no reg ranges or more reg ranges %d\n", reg_count);
			err = -ENXIO;
			goto ERROR_PROVE_DEVICE;
		}

		/* if platform driver is implemented */
		if (res.start != 0) {
			s_vpu_register.phys_addr = res.start;
			s_vpu_register.virt_addr = (ulong)ioremap_nocache(res.start, resource_size(&res));
			s_vpu_register.size = res.end - res.start;

			enc_pr(LOG_DEBUG, "vpu base address get from platform driver ");
			enc_pr(LOG_DEBUG, "physical addr=0x%lx, virtual addr=0x%lx\n",
					s_vpu_register.phys_addr, s_vpu_register.virt_addr);
		} else {
			s_vpu_register.phys_addr = VPU_REG_BASE_ADDR;
			s_vpu_register.virt_addr = (ulong)ioremap_nocache(s_vpu_register.phys_addr, VPU_REG_SIZE);
			s_vpu_register.size = VPU_REG_SIZE;

			enc_pr(LOG_DEBUG, "vpu base address get from defined value ");
			enc_pr(LOG_DEBUG, "physical addr=0x%lx, virtual addr=0x%lx\n",
				s_vpu_register.phys_addr, s_vpu_register.virt_addr);
		}

		reg_count++;
	}

	/* get the major number of the character device */
	if (init_MultiEnc_device()) {
		err = -EBUSY;
		enc_pr(LOG_ERROR, "could not allocate major number\n");

		goto ERROR_PROVE_DEVICE;
	}

	enc_pr(LOG_INFO, "SUCCESS alloc_chrdev_region\n");

	for (idx = 0; idx < MAX_NUM_INSTANCE; idx ++)
		init_waitqueue_head(&s_interrupt_wait_q[idx]);

	for (idx = 0; idx < MAX_NUM_INSTANCE; idx ++) {
		err = kfifo_alloc(&s_interrupt_pending_q[idx], MAX_INTERRUPT_QUEUE*sizeof(u32), GFP_KERNEL);

		if (err) {
			enc_pr(LOG_ERROR,"kfifo_alloc failed 0x%x\n", err);
			goto ERROR_PROVE_DEVICE;
		}

		s_fifo_alloc_flag[idx] = 1;
	}

	tasklet_init(&multienc_tasklet, multienc_isr_tasklet, (ulong)&s_vpu_drv_context);

	s_common_memory.base = 0;
	s_instance_pool.base = 0;

	if (use_reserve == true) {
		if (vmem_init(&s_vmem, s_video_memory.phys_addr, s_video_memory.size) < 0) {
			enc_pr(LOG_ERROR, "fail to init vmem system\n");
			goto ERROR_PROVE_DEVICE;
		}

		enc_pr(LOG_DEBUG, "success to probe vpu device with video memory");
		enc_pr(LOG_DEBUG, "phys_addr=0x%lx, base = 0x%lx\n",
			(ulong)s_video_memory.phys_addr, (ulong)s_video_memory.base);
	} else {
		enc_pr(LOG_DEBUG, "success to probe vpu device with video memory \n");
	}

	enc_pr(LOG_DEBUG, "to be allocate from CMA pool_size 0x%lx\n", cma_pool_size);
	multienc_pdev = pdev;

	return 0;

ERROR_PROVE_DEVICE:
	for (idx = 0; idx < MAX_NUM_INSTANCE; idx++) {
		if (s_fifo_alloc_flag[idx])
			kfifo_free(&s_interrupt_pending_q[idx]);
		s_fifo_alloc_flag[idx] = 0;
	}

	if (s_vpu_register.virt_addr) {
		iounmap((void *)s_vpu_register.virt_addr);
		memset(&s_vpu_register, 0, sizeof(struct vpudrv_buffer_t));
	}

	if (s_video_memory.phys_addr) {
		vmem_exit(&s_vmem);
		memset(&s_video_memory, 0, sizeof(struct vpudrv_buffer_t));
		memset(&s_vmem, 0, sizeof(struct video_mm_t));
	}

	if (s_vpu_irq_requested == true) {
		if (s_vpu_irq >= 0) {
			free_irq(s_vpu_irq, &s_vpu_drv_context);
			s_vpu_irq = -1;
		}
		s_vpu_irq_requested = false;
	}
	uninit_MultiEnc_device();
	return err;
}

static s32 vpu_remove(struct platform_device *pdev)
{
	s32 idx;
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
	for (idx = 0; idx < MAX_NUM_INSTANCE; idx++)
		kfifo_free(&s_interrupt_pending_q[idx]);

	if (s_vpu_register.virt_addr) {
		iounmap((void *)s_vpu_register.virt_addr);
		memset(&s_vpu_register,
			0, sizeof(struct vpudrv_buffer_t));
	}
	if (clock_gate_count > 0)
	{
		vpu_clk_disable(&s_vpu_clks);
	}
	vpu_clk_put(&multienc_pdev->dev, &s_vpu_clks);
	multienc_pdev = NULL;
	uninit_MultiEnc_device();
	return 0;
}

#ifdef CONFIG_PM
#define VP5_CMD_INIT_VPU         (0x0001)
#define VP5_CMD_SLEEP_VPU        (0x0004)
#define VP5_CMD_WAKEUP_VPU       (0x0002)

static void Vp5BitIssueCommand(int core, u32 cmd)
{
	WriteVpuRegister(VP5_VPU_BUSY_STATUS, 1);
	WriteVpuRegister(VP5_COMMAND, cmd);
	WriteVpuRegister(VP5_VPU_HOST_INT_REQ, 1);

	return;
}

static s32 vpu_suspend(struct platform_device *pdev, pm_message_t state)
{
	u32 core;
	ulong timeout = jiffies + HZ; /* vpu wait timeout to 1sec */

	enc_pr(LOG_DEBUG, "vpu_suspend\n");

	if (s_vpu_open_ref_count > 0) {
#ifdef VPU_SUPPORT_CLOCK_CONTROL
		vpu_clk_config(1);
#endif
		for (core = 0; core < MAX_NUM_VPU_CORE; core++) {
			if (s_bit_firmware_info[core].size == 0)
				continue;
			while (ReadVpuRegister(VP5_VPU_BUSY_STATUS)) {
				if (time_after(jiffies, timeout)) {
					enc_pr(LOG_ERROR,
						"SLEEP_VPU BUSY timeout");
					goto DONE_SUSPEND;
				}
			}
			Vp5BitIssueCommand(core, VP5_CMD_SLEEP_VPU);
			while (ReadVpuRegister(VP5_VPU_BUSY_STATUS)) {
				if (time_after(jiffies, timeout)) {
					enc_pr(LOG_ERROR,
						"SLEEP_VPU BUSY timeout");
					goto DONE_SUSPEND;
				}
			}
			if (ReadVpuRegister(VP5_RET_SUCCESS) == 0) {
				enc_pr(LOG_ERROR,
					"SLEEP_VPU failed [0x%x]",
					ReadVpuRegister(VP5_RET_FAIL_REASON));
				goto DONE_SUSPEND;
			}
		}
#ifdef VPU_SUPPORT_CLOCK_CONTROL
		vpu_clk_config(0);
		if (clock_gate_count > 0)
#endif
		{
			clk_disable(s_vpu_clks.c_clk);
			clk_disable(s_vpu_clks.b_clk);
			clk_disable(s_vpu_clks.a_clk);
		}
		/* the power off */
		pwr_ctrl_psci_smc(PDID_T7_DOS_WAVE, false);
	}
	return 0;

DONE_SUSPEND:
#ifdef VPU_SUPPORT_CLOCK_CONTROL
	vpu_clk_config(0);
#endif
	return -EAGAIN;
}
static s32 vpu_resume(struct platform_device *pdev)
{
	u32 core;
	ulong timeout = jiffies + HZ; /* vpu wait timeout to 1sec */
	ulong code_base;
	u32 code_size;
	u32 remap_size;
	u32 regVal;
	u32 hwOption = 0;

	enc_pr(LOG_DEBUG, "vpu_resume\n");

	if (s_vpu_open_ref_count > 0) {
#ifdef VPU_SUPPORT_CLOCK_CONTROL
		if (clock_gate_count > 0)
#endif
		{
			clk_enable(s_vpu_clks.a_clk);
			clk_enable(s_vpu_clks.b_clk);
			clk_enable(s_vpu_clks.c_clk);
		}
		vpu_clk_config(1);
		/* the power on */
		pwr_ctrl_psci_smc(PDID_T7_DOS_WAVE, true);
		for (core = 0; core < MAX_NUM_VPU_CORE; core++) {
			if (s_bit_firmware_info[core].size == 0)
				continue;
			code_base = s_common_memory.phys_addr;
			/* ALIGN TO 4KB */
			code_size = (s_common_memory.size & ~0xfff);
			if (code_size < s_bit_firmware_info[core].size * 2)
				goto DONE_WAKEUP;
			regVal = 0;
			WriteVpuRegister(VP5_PO_CONF, regVal);

			/* Reset All blocks */
			regVal = 0x7ffffff;
			WriteVpuRegister(VP5_VPU_RESET_REQ, regVal);
			/* Waiting reset done */
			while (ReadVpuRegister(VP5_VPU_RESET_STATUS)) {
				if (time_after(jiffies, timeout))
					goto DONE_WAKEUP;
			}
			WriteVpuRegister(VP5_VPU_RESET_REQ, 0);

			/* remap page size */
			remap_size = (code_size >> 12) & 0x1ff;
			regVal = 0x80000000 | (VP5_REMAP_CODE_INDEX<<12)
				| (0 << 16) | (1<<11) | remap_size;
			WriteVpuRegister(VP5_VPU_REMAP_CTRL, regVal);
			/* DO NOT CHANGE! */
			WriteVpuRegister(VP5_VPU_REMAP_VADDR, 0x00000000);
			WriteVpuRegister(VP5_VPU_REMAP_PADDR, code_base);
			WriteVpuRegister(VP5_ADDR_CODE_BASE, code_base);
			WriteVpuRegister(VP5_CODE_SIZE, code_size);
			WriteVpuRegister(VP5_CODE_PARAM, 0);
			WriteVpuRegister(VP5_INIT_VPU_TIME_OUT_CNT, timeout);
			WriteVpuRegister(VP5_HW_OPTION, hwOption);

			/* Interrupt */
			regVal = (1 << INT_ENC_SET_PARAM);
			regVal |= (1 << INT_ENC_PIC);
			regVal |= (1 << INT_INIT_SEQ);
			regVal |= (1 << INT_DEC_PIC);
			regVal |= (1 << INT_BSBUF_EMPTY);
			WriteVpuRegister(VP5_VPU_VINT_ENABLE, regVal);
			Vp5BitIssueCommand(core, VP5_CMD_INIT_VPU);
			WriteVpuRegister(VP5_VPU_REMAP_CORE_START, 1);
			while (ReadVpuRegister(VP5_VPU_BUSY_STATUS)) {
				if (time_after(jiffies, timeout))
					goto DONE_WAKEUP;
			}

			if (ReadVpuRegister(VP5_RET_SUCCESS) == 0) {
				enc_pr(LOG_ERROR,
					"WAKEUP_VPU failed [0x%x]",
					ReadVpuRegister(VP5_RET_FAIL_REASON));
				goto DONE_WAKEUP;
			}
		}
	}
DONE_WAKEUP:
	if (s_vpu_open_ref_count > 0)
		vpu_clk_config(0);
	return 0;
}
#else
#define vpu_suspend NULL
#define vpu_resume NULL
#endif /* !CONFIG_PM */

static const struct of_device_id cnm_multienc_dt_match[] = {
	{
		.compatible = "cnm, MultiEnc",
	},
	{},
};

static struct platform_driver vpu_driver = {
	.driver = {
		.name = VPU_PLATFORM_DEVICE_NAME,
		.of_match_table = cnm_multienc_dt_match,
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

	if (get_cpu_major_id() != AM_MESON_CPU_MAJOR_ID_T7) {
		pr_err("The chip is not support multi encoder!!\n");
		return -1;
	}

	res = platform_driver_register(&vpu_driver);
	enc_pr(LOG_INFO,
		"end vpu_init result=0x%x\n", res);
	return res;
}

static void __exit vpu_exit(void)
{
	enc_pr(LOG_DEBUG, "vpu_exit\n");
	platform_driver_unregister(&vpu_driver);
}

static const struct reserved_mem_ops rmem_multienc_ops = {
	.device_init = multienc_mem_device_init,
};

static s32 __init multienc_mem_setup(struct reserved_mem *rmem)
{
	rmem->ops = &rmem_multienc_ops;
	enc_pr(LOG_DEBUG, "MultiEnc reserved mem setup.\n");
	return 0;
}

module_param(print_level, uint, 0664);
MODULE_PARM_DESC(print_level, "\n print_level\n");

module_param(clock_level, uint, 0664);
MODULE_PARM_DESC(clock_level, "\n clock_level\n");

module_param(clock_gate_count, uint, 0664);
MODULE_PARM_DESC(clock_gate_count, "\n clock_gate_count\n");

module_param(set_clock_freq, uint, 0664);
MODULE_PARM_DESC(set_clock_freq, "\n set clk freq\n");

module_param(clock_a, uint, 0664);
MODULE_PARM_DESC(clock_a, "\n clock_a\n");

module_param(clock_b, uint, 0664);
MODULE_PARM_DESC(clock_b, "\n clock_b\n");

module_param(clock_c, uint, 0664);
MODULE_PARM_DESC(clock_c, "\n clock_c\n");

module_param(dump_input, uint, 0664);
MODULE_PARM_DESC(dump_input, "\n dump_input\n");

module_param(dump_es, uint, 0664);
MODULE_PARM_DESC(dump_es, "\n dump_es\n");

MODULE_AUTHOR("Amlogic Inc.");
MODULE_DESCRIPTION("VPU linux driver");
MODULE_LICENSE("GPL");

module_init(vpu_init);
module_exit(vpu_exit);
RESERVEDMEM_OF_DECLARE(cnm_multienc, "cnm, MultiEnc-mem", multienc_mem_setup);
