// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_fdt.h>
#include <linux/of_reserved_mem.h>
#include <linux/of_irq.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/errno.h>
#include <linux/uaccess.h>
#include <linux/ioport.h>
#include <linux/ctype.h>
#include <linux/vmalloc.h>
#include <linux/interrupt.h>
#include <linux/highmem.h>
#include <linux/dma-mapping.h>
#include <linux/dma-contiguous.h>
#include <linux/page-flags.h>
#include <linux/amlogic/media/frc/frc_reg.h>
#include <linux/amlogic/media/frc/frc_common.h>
#include <linux/sched/clock.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/module.h>

#include "frc_drv.h"
#include "frc_buf.h"
#include "frc_rdma.h"

int frc_buf_test = 1;
module_param(frc_buf_test, int, 0664);
MODULE_PARM_DESC(frc_buf_test, "frc dynamic buf debug");

static u8 cur_state = 1;

void frc_dump_mm_data(void *addr, u32 size)
{
	u32 *c = addr;
	ulong start;
	ulong end;

	start = (ulong)addr;
	end = start + size;

	//pr_info("addr:0x%lx size:0x%x\n", (ulong)addr);
	for (start = (ulong)addr; start < end; start += 64) {
		pr_info("\tvaddr(0x%lx): %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x\n",
		(ulong)c, c[0], c[1], c[2], c[3], c[4], c[5], c[6], c[7],
		c[8], c[9], c[10], c[11], c[12], c[13], c[14], c[15]);
		c += 16;
	}
}

/*
 * frc map p to v memory address
 */
u8 *frc_buf_vmap(ulong addr, u32 size)
{
	u8 *vaddr = NULL;
	struct page **pages = NULL;
	u32 i, npages, offset = 0;
	ulong phys, page_start;
	pgprot_t pgprot = PAGE_KERNEL;

	if (!PageHighMem(phys_to_page(addr))) {
		vaddr = phys_to_virt(addr);
		//pr_frc(1, "low mem map to 0x%lx\n", (ulong)vaddr);
		return vaddr;
	}

	offset = offset_in_page(addr);
	page_start = addr - offset;
	npages = DIV_ROUND_UP(size + offset, PAGE_SIZE);

	pages = kmalloc_array(npages, sizeof(struct page *), GFP_KERNEL);
	if (!pages)
		return NULL;

	for (i = 0; i < npages; i++) {
		phys = page_start + i * PAGE_SIZE;
		pages[i] = pfn_to_page(phys >> PAGE_SHIFT);
	}

	vaddr = vmap(pages, npages, VM_MAP, pgprot);
	if (!vaddr) {
		pr_frc(0, "the phy(%lx) vmaped fail, size: %d\n",
		       (ulong)page_start, npages << PAGE_SHIFT);
		kfree(pages);
		return NULL;
	}

	kfree(pages);

	pr_frc(1, "[MEM-MAP] %s, pa(%lx) to va(%lx), size: %d\n",
	       __func__, (ulong)page_start, (ulong)vaddr, npages << PAGE_SHIFT);

	return vaddr + offset;
}

void frc_buf_unmap(u32 *vaddr)
{
	void *addr = (void *)(PAGE_MASK & (ulong)vaddr);

	if (is_vmalloc_or_module_addr(vaddr)) {
		pr_frc(0, "unmap v: %px\n", addr);
		vunmap(addr);
	}
}

void frc_buf_dma_flush(struct frc_dev_s *devp, void *vaddr, ulong phy_addr, int size,
					enum dma_data_direction dir)
{
	//ulong phy_addr;

	//if (is_vmalloc_or_module_addr(vaddr)) {
	//	phy_addr = page_to_phys(vmalloc_to_page(vaddr)) + offset_in_page(vaddr);
	//	if (phy_addr && PageHighMem(phys_to_page(phy_addr))) {
	//		pr_frc(0, "flush v: %lx, p: %lx\n", vaddr, phy_addr);
	//		dma_sync_single_for_device(&devp->pdev->dev, phy_addr, size, dir);
	//	}
	//	return;
	//}

	dma_sync_single_for_device(&devp->pdev->dev, phy_addr, size, DMA_FROM_DEVICE);
}

/*
 * frc dump all memory address
 */
void frc_buf_dump_memory_addr_info(struct frc_dev_s *devp)
{
	u32 i;
	u32 base;

	base = devp->buf.cma_mem_paddr_start;
	/*info buffer*/
	pr_frc(0, "lossy_mc_y_info_buf_paddr:0x%x size:0x%08x\n",
		base + devp->buf.lossy_mc_y_info_buf_paddr,
		devp->buf.lossy_mc_y_info_buf_size);
	pr_frc(0, "lossy_mc_c_info_buf_paddr:0x%x size:0x%08x\n",
		base + devp->buf.lossy_mc_c_info_buf_paddr,
		devp->buf.lossy_mc_c_info_buf_size);
	pr_frc(0, "lossy_mc_v_info_buf_paddr:0x%x size:0x%08x\n",
		base + devp->buf.lossy_mc_v_info_buf_paddr,
		devp->buf.lossy_mc_v_info_buf_size);
	pr_frc(0, "lossy_me_x_info_buf_paddr:0x%x size:0x%08x\n",
		base + devp->buf.lossy_me_x_info_buf_paddr,
		devp->buf.lossy_me_x_info_buf_size);

	/*lossy data buffer*/
	for (i = 0; i < FRC_TOTAL_BUF_NUM; i++)
		pr_frc(0, "lossy_mc_y_data_buf_paddr[%d]:0x%08x size:0x%08x\n", i,
		       base + devp->buf.lossy_mc_y_data_buf_paddr[i],
		       devp->buf.lossy_mc_y_data_buf_size[i]);

	for (i = 0; i < FRC_TOTAL_BUF_NUM; i++)
		pr_frc(0, "lossy_mc_c_data_buf_paddr[%d]:0x%x size:0x%08x\n", i,
		       base + devp->buf.lossy_mc_c_data_buf_paddr[i],
		       devp->buf.lossy_mc_c_data_buf_size[i]);

	for (i = 0; i < FRC_TOTAL_BUF_NUM; i++)
		pr_frc(0, "lossy_mc_v_data_buf_paddr[%d]:0x%x size:0x%08x\n", i,
		       base + devp->buf.lossy_mc_v_data_buf_paddr[i],
		       devp->buf.lossy_mc_v_data_buf_size[i]);

	for (i = 0; i < FRC_TOTAL_BUF_NUM; i++)
		pr_frc(0, "lossy_me_data_buf_paddr[%d]:0x%x size:0x%08x\n", i,
		       base + devp->buf.lossy_me_data_buf_paddr[i],
		       devp->buf.lossy_me_data_buf_size[i]);

	/*link buffer*/
	for (i = 0; i < FRC_TOTAL_BUF_NUM; i++)
		pr_frc(0, "lossy_mc_y_link_buf_paddr[%d]:0x%x size:0x%08x\n", i,
		       base + devp->buf.lossy_mc_y_link_buf_paddr[i],
		       devp->buf.lossy_mc_y_link_buf_size[i]);

	for (i = 0; i < FRC_TOTAL_BUF_NUM; i++)
		pr_frc(0, "lossy_mc_c_link_buf_paddr[%d]:0x%x size:0x%08x\n", i,
		       base + devp->buf.lossy_mc_c_link_buf_paddr[i],
		       devp->buf.lossy_mc_c_link_buf_size[i]);

	for (i = 0; i < FRC_TOTAL_BUF_NUM; i++)
		pr_frc(0, "lossy_mc_v_link_buf_paddr[%d]:0x%x size:0x%08x\n", i,
		       base + devp->buf.lossy_mc_v_link_buf_paddr[i],
		       devp->buf.lossy_mc_v_link_buf_size[i]);


	for (i = 0; i < FRC_TOTAL_BUF_NUM; i++)
		pr_frc(0, "lossy_me_link_buf_paddr[%d]:0x%x size:0x%08x\n", i,
		       base + devp->buf.lossy_me_link_buf_paddr[i],
		       devp->buf.lossy_me_link_buf_size[i]);

	/*norm buffer*/
	for (i = 0; i < FRC_TOTAL_BUF_NUM; i++)
		pr_frc(0, "norm_hme_data_buf_paddr[%d]:0x%x size:0x%08x\n", i,
		       base + devp->buf.norm_hme_data_buf_paddr[i],
		       devp->buf.norm_hme_data_buf_size[i]);

	for (i = 0; i < FRC_MEMV_BUF_NUM; i++)
		pr_frc(0, "norm_memv_buf_paddr[%d]:0x%x size:0x%08x\n", i,
		       base + devp->buf.norm_memv_buf_paddr[i],
		       devp->buf.norm_memv_buf_size[i]);

	for (i = 0; i < FRC_MEMV2_BUF_NUM; i++)
		pr_frc(0, "norm_hmemv_buf_paddr[%d]:0x%x size:0x%08x\n", i,
		       base + devp->buf.norm_hmemv_buf_paddr[i],
		       devp->buf.norm_hmemv_buf_size[i]);

	for (i = 0; i < FRC_MEVP_BUF_NUM; i++)
		pr_frc(0, "norm_mevp_out_buf_paddr[%d]:0x%x size:0x%08x\n", i,
		       base + devp->buf.norm_mevp_out_buf_paddr[i],
		       devp->buf.norm_mevp_out_buf_size[i]);

	for (i = 0; i < FRC_TOTAL_BUF_NUM; i++)
		pr_frc(0, "norm_iplogo_buf_paddr[%d]:0x%x size:0x%08x\n", i,
		       base + devp->buf.norm_iplogo_buf_paddr[i],
		       devp->buf.norm_iplogo_buf_size[i]);

	pr_frc(0, "norm_logo_irr_buf_paddr:0x%x size:0x%x\n",
		base + devp->buf.norm_logo_irr_buf_paddr, devp->buf.norm_logo_irr_buf_size);
	pr_frc(0, "norm_logo_scc_buf_paddr:0x%x size:0x%x\n",
		base + devp->buf.norm_logo_scc_buf_paddr, devp->buf.norm_logo_scc_buf_size);

	for (i = 0; i < FRC_TOTAL_BUF_NUM; i++)
		pr_frc(0, "norm_melogo_buf_paddr[%d]:0x%x size:0x%08x\n", i,
		       base + devp->buf.norm_melogo_buf_paddr[i],
		       devp->buf.norm_melogo_buf_size[i]);

	pr_frc(0, "total_size:0x%x\n", devp->buf.total_size);
	pr_frc(0, "real_total_size:0x%x\n", devp->buf.real_total_size);
	pr_frc(0, "cma_mem_alloced:0x%x\n", devp->buf.cma_mem_alloced);
	pr_frc(0, "cma_mem_paddr_start:0x%lx\n", (ulong)devp->buf.cma_mem_paddr_start);
}

void frc_buf_dump_memory_size_info(struct frc_dev_s *devp)
{
	u32 log = 0;

	pr_frc(log, "in hv (%d, %d) align (%d, %d)",
	       devp->buf.in_hsize, devp->buf.in_vsize,
	       devp->buf.in_align_hsize, devp->buf.in_align_vsize);
	pr_frc(log, "me hv (%d, %d)", devp->buf.me_hsize, devp->buf.me_vsize);
	pr_frc(log, "logo hv (%d, %d)", devp->buf.logo_hsize, devp->buf.logo_vsize);
	pr_frc(log, "hme hv (%d, %d)", devp->buf.hme_hsize, devp->buf.hme_vsize);
	pr_frc(log, "me blk hv (%d, %d)", devp->buf.me_blk_hsize,
	       devp->buf.me_blk_vsize);
	pr_frc(log, "hme blk (%d, %d)", devp->buf.hme_blk_hsize,
	       devp->buf.hme_blk_vsize);

	/*mc info buffer*/
	pr_frc(log, "lossy_mc_info_buf_size=%d, line buf:%d all:%d\n",
	       devp->buf.lossy_mc_y_info_buf_size, LOSSY_MC_INFO_LINE_SIZE,
	       devp->buf.lossy_mc_y_info_buf_size * 2);

	/*me info buffer*/
	pr_frc(log, "lossy_me_info_buf_size=%d, line buf:%d all:%d\n",
	       devp->buf.lossy_me_x_info_buf_size, LOSSY_MC_INFO_LINE_SIZE,
	       devp->buf.lossy_me_x_info_buf_size * 1);

	/*lossy mc data buffer*/
	pr_frc(log, "lossy_mc_y_data_buf_size=%d, all:%d\n",
	       devp->buf.lossy_mc_y_data_buf_size[0],
	       devp->buf.lossy_mc_y_data_buf_size[0] * FRC_TOTAL_BUF_NUM);
	pr_frc(log, "lossy_mc_c_data_buf_size=%d, all:%d\n",
	       devp->buf.lossy_mc_c_data_buf_size[0],
	       devp->buf.lossy_mc_c_data_buf_size[0] * FRC_TOTAL_BUF_NUM);

	/*lossy me data buffer*/
	pr_frc(log, "lossy_me_data_buf_size=%d, all:%d\n",
	       devp->buf.lossy_me_data_buf_size[0],
	       devp->buf.lossy_me_data_buf_size[0] * FRC_TOTAL_BUF_NUM);

	/*mc link buffer*/
	pr_frc(log, "lossy_mc_link_buf_size=%d, all:%d\n",
	       devp->buf.lossy_mc_y_link_buf_size[0],
	       devp->buf.lossy_mc_y_link_buf_size[0] * FRC_TOTAL_BUF_NUM * 2);

	/*me link buffer*/
	pr_frc(log, "lossy_me_link_buf_size=%d , all:%d\n",
	       devp->buf.lossy_me_link_buf_size[0],
	       devp->buf.lossy_me_link_buf_size[0] * FRC_TOTAL_BUF_NUM);

	/*norm hme data buffer*/
	pr_frc(log, "norm_hme_data_buffer=%d , all:%d\n",
	       devp->buf.norm_hme_data_buf_size[0],
	       devp->buf.norm_hme_data_buf_size[0] * FRC_TOTAL_BUF_NUM);

	/*norm memv buffer*/
	pr_frc(log, "norm_memv_buf_size=%d , all:%d\n",
	       devp->buf.norm_memv_buf_size[0],
	       devp->buf.norm_memv_buf_size[0] * FRC_MEMV_BUF_NUM);

	/*norm hmemv buffer*/
	pr_frc(log, "norm_hmemv_buf_size=%d , all:%d\n",
	       devp->buf.norm_hmemv_buf_size[0],
	       devp->buf.norm_hmemv_buf_size[0] * FRC_MEMV2_BUF_NUM);

	/*norm mevp buffer*/
	pr_frc(log, "norm_mevp_out_buf_size=%d , all:%d\n",
	       devp->buf.norm_mevp_out_buf_size[0],
	       devp->buf.norm_mevp_out_buf_size[0] * FRC_MEVP_BUF_NUM);

	/*norm iplogo buffer*/
	pr_frc(log, "norm_iplogo_buf_size=%d , all:%d\n",
	       devp->buf.norm_iplogo_buf_size[0],
	       devp->buf.norm_iplogo_buf_size[0] * FRC_TOTAL_BUF_NUM);

	/*norm logo irr buffer*/
	pr_frc(log, "norm_logo_irr_buf_size=%d\n",
	       devp->buf.norm_logo_irr_buf_size);

	/*norm logo scc buffer*/
	pr_frc(log, "norm_logo_scc_buf_size=%d\n",
	       devp->buf.norm_logo_scc_buf_size);

	/*norm iplogo buffer*/
	pr_frc(log, "norm_melogo_buf_size=%d , all:%d\n",
	       devp->buf.norm_melogo_buf_size[0],
	       devp->buf.norm_melogo_buf_size[0] * FRC_TOTAL_BUF_NUM);

	pr_frc(0, "total_size=%d\n", devp->buf.total_size);
}

void frc_buf_dump_link_tab(struct frc_dev_s *devp, u32 mode)
{
	u32 *vaddr;
	u32 *vaddr_start;
	phys_addr_t cma_addr = 0;
	u32 map_size = 0;
	u32 i;

	pr_frc(0, "%s md:%d\n", __func__, mode);
	if (mode == 0) {
		for (i = 0; i < FRC_TOTAL_BUF_NUM; i++) {
			cma_addr =
			devp->buf.cma_mem_paddr_start + devp->buf.lossy_mc_y_link_buf_paddr[i];
			map_size = roundup(devp->buf.lossy_mc_y_link_buf_size[i], ALIGN_64);
			if (map_size == 0)
				break;
			pr_frc(0, "dump buf %d:0x%lx size:0x%x\n", i, (ulong)cma_addr, map_size);
			vaddr = (u32 *)frc_buf_vmap(cma_addr, map_size);
			vaddr_start = vaddr;
			frc_dump_mm_data(vaddr, map_size);
			frc_buf_unmap(vaddr_start);
		}
	} else if (mode == 1) {
		for (i = 0; i < FRC_TOTAL_BUF_NUM; i++) {
			cma_addr =
			devp->buf.cma_mem_paddr_start + devp->buf.lossy_mc_c_link_buf_paddr[i];
			map_size = roundup(devp->buf.lossy_mc_c_link_buf_size[i], ALIGN_64);
			if (map_size == 0)
				break;
			pr_frc(0, "dump :0x%lx size:0x%x\n", (ulong)cma_addr, map_size);
			vaddr = (u32 *)frc_buf_vmap(cma_addr, map_size);
			vaddr_start = vaddr;
			frc_dump_mm_data(vaddr, map_size);
			frc_buf_unmap(vaddr_start);
		}
	} else if (mode == 2) {
		for (i = 0; i < FRC_TOTAL_BUF_NUM; i++) {
			cma_addr =
			devp->buf.cma_mem_paddr_start + devp->buf.lossy_mc_v_link_buf_paddr[i];
			map_size = roundup(devp->buf.lossy_mc_v_link_buf_size[i], ALIGN_64);
			if (map_size == 0)
				break;
			pr_frc(0, "dump :0x%lx size:0x%x\n", (ulong)cma_addr, map_size);
			vaddr = (u32 *)frc_buf_vmap(cma_addr, map_size);
			vaddr_start = vaddr;
			frc_dump_mm_data(vaddr, map_size);
			frc_buf_unmap(vaddr_start);
		}
	} else if (mode == 3) {
		for (i = 0; i < FRC_TOTAL_BUF_NUM; i++) {
			cma_addr =
			devp->buf.cma_mem_paddr_start + devp->buf.lossy_me_link_buf_paddr[i];
			map_size = roundup(devp->buf.lossy_me_link_buf_size[i], ALIGN_64);
			if (map_size == 0)
				break;
			pr_frc(0, "dump :0x%lx size:0x%x\n", (ulong)cma_addr, map_size);
			vaddr = (u32 *)frc_buf_vmap(cma_addr, map_size);
			vaddr_start = vaddr;
			frc_dump_mm_data(vaddr, map_size);
			frc_buf_unmap(vaddr_start);
		}
	}
}

void frc_dump_buf_data(struct frc_dev_s *devp, u32 cma_addr, u32 size)
{
	struct file *filp = NULL;
	loff_t pos = 0;
	void *buf = NULL;
	unsigned int len = size;
	mm_segment_t old_fs = get_fs();
	char *path = "/data/frc.bin";

	pr_info("%s paddr:0x%x, size:0x%x\n", __func__, cma_addr, size);
	if ((size > 1024 * 1024 * 20) || size == 0)
		return;

	set_fs(KERNEL_DS);
	filp = filp_open(path, O_RDWR | O_CREAT, 0666);

	if (IS_ERR_OR_NULL(filp)) {
		pr_info("create %s error or filp is NULL\n", path);
		set_fs(old_fs);
		return;
	}

	if (!devp->buf.cma_mem_alloced) {
		pr_frc(0, "%s:no cma alloc mem\n", __func__);
		set_fs(old_fs);
		return;
	}

	buf = (u32 *)frc_buf_vmap(cma_addr, len);
	if (!buf) {
		pr_info("vdin_vmap error\n");
		goto exit;
	}
	frc_buf_dma_flush(devp, buf, cma_addr, len, DMA_FROM_DEVICE);
	//write
	vfs_write(filp, buf, len, &pos);

	frc_buf_unmap(buf);
exit:
	vfs_fsync(filp, 0);
	filp_close(filp, NULL);
	set_fs(old_fs);
}

void frc_dump_buf_reg(void)
{
	u32 i = 0;

	pr_frc(0, "%s (0x%x) val:0x%x\n", "FRC_REG_MC_YINFO_BADDR", FRC_REG_MC_YINFO_BADDR,
	       READ_FRC_REG(FRC_REG_MC_YINFO_BADDR));
	pr_frc(0, "%s (0x%x) val:0x%x\n", "FRC_REG_MC_CINFO_BADDR", FRC_REG_MC_CINFO_BADDR,
	       READ_FRC_REG(FRC_REG_MC_CINFO_BADDR));
	pr_frc(0, "%s (0x%x) val:0x%x\n", "FRC_REG_MC_VINFO_BADDR", FRC_REG_MC_VINFO_BADDR,
	       READ_FRC_REG(FRC_REG_MC_VINFO_BADDR));
	pr_frc(0, "%s (0x%x) val:0x%x\n", "FRC_REG_ME_XINFO_BADDR", FRC_REG_ME_XINFO_BADDR,
	       READ_FRC_REG(FRC_REG_ME_XINFO_BADDR));

	for (i = FRC_REG_MC_YBUF_ADDRX_0; i <= FRC_REG_MC_YBUF_ADDRX_15; i++)
		pr_frc(0, "%s%d (0x%x) val:0x%x\n",
		       "FRC_REG_MC_YBUF_ADDRX_", i - FRC_REG_MC_YBUF_ADDRX_0, i,
		       READ_FRC_REG(i));

	for (i = FRC_REG_MC_CBUF_ADDRX_0; i <= FRC_REG_MC_CBUF_ADDRX_15; i++)
		pr_frc(0, "%s%d (0x%x) val:0x%x\n",
		       "FRC_REG_MC_CBUF_ADDRX_", i - FRC_REG_MC_CBUF_ADDRX_0, i,
		       READ_FRC_REG(i));

	for (i = FRC_REG_MC_VBUF_ADDRX_0; i <= FRC_REG_MC_VBUF_ADDRX_15; i++)
		pr_frc(0, "%s%d (0x%x) val:0x%x\n",
		       "FRC_REG_MC_VBUF_ADDRX_", i - FRC_REG_MC_VBUF_ADDRX_0, i,
		       READ_FRC_REG(i));

	/*lossy me link buffer*/
	for (i = FRC_REG_ME_BUF_ADDRX_0; i <= FRC_REG_ME_BUF_ADDRX_15; i++)
		pr_frc(0, "%s%d (0x%x) val:0x%x\n",
		       "FRC_REG_ME_BUF_ADDRX_", i - FRC_REG_ME_BUF_ADDRX_0, i,
		       READ_FRC_REG(i));

	/*norm hme data buffer*/
	for (i = FRC_REG_HME_BUF_ADDRX_0; i <= FRC_REG_HME_BUF_ADDRX_15; i++)
		pr_frc(0, "%s%d (0x%x) val:0x%x\n",
		       "FRC_REG_HME_BUF_ADDRX_", i - FRC_REG_HME_BUF_ADDRX_0, i,
		       READ_FRC_REG(i));

	/*norm memv buffer*/
	for (i = FRC_REG_ME_NC_UNI_MV_ADDRX_0; i <= FRC_REG_ME_PC_PHS_MV_ADDR; i++)
		pr_frc(0, "%s%d (0x%x) val:0x%x\n",
		       "FRC_REG_ME_NC_UNI_MV_ADDRX_", i - FRC_REG_ME_NC_UNI_MV_ADDRX_0, i,
		       READ_FRC_REG(i));

	/*norm hmemv buffer*/
	for (i = FRC_REG_HME_NC_UNI_MV_ADDRX_0; i <= FRC_REG_VP_PF_UNI_MV_ADDR; i++)
		pr_frc(0, "%s%d (0x%x) val:0x%x\n",
		       "FRC_REG_HME_NC_UNI_MV_ADDRX_", i - FRC_REG_HME_NC_UNI_MV_ADDRX_0, i,
		       READ_FRC_REG(i));

	/*norm mevp buffer*/
	for (i = FRC_REG_VP_MC_MV_ADDRX_0; i <= FRC_REG_VP_MC_MV_ADDRX_1; i++)
		pr_frc(0, "%s%d (0x%x) val:0x%x\n",
		       "FRC_REG_VP_MC_MV_ADDRX_", i - FRC_REG_VP_MC_MV_ADDRX_0, i,
		       READ_FRC_REG(i));

	/*norm iplogo buffer*/
	for (i = FRC_REG_IP_LOGO_ADDRX_0; i <= FRC_REG_IP_LOGO_ADDRX_15; i++)
		pr_frc(0, "%s%d (0x%x) val:0x%x\n",
		       "FRC_REG_IP_LOGO_ADDRX_", i - FRC_REG_IP_LOGO_ADDRX_0, i,
		       READ_FRC_REG(i));

	pr_frc(0, "%s (0x%x) val:0x%x\n",
	       "FRC_REG_LOGO_IIR_BUF_ADDR", FRC_REG_LOGO_IIR_BUF_ADDR,
	       READ_FRC_REG(FRC_REG_LOGO_IIR_BUF_ADDR));
	pr_frc(0, "%s (0x%x) val:0x%x\n",
	       "FRC_REG_LOGO_SCC_BUF_ADDR", FRC_REG_LOGO_SCC_BUF_ADDR,
	       READ_FRC_REG(FRC_REG_LOGO_SCC_BUF_ADDR));

	/*norm iplogo buffer*/
	for (i = FRC_REG_ME_LOGO_ADDRX_0; i <= FRC_REG_ME_LOGO_ADDRX_15; i++)
		pr_frc(0, "%s%d (0x%x) val:0x%x\n",
		       "FRC_REG_ME_LOGO_ADDRX_", i - FRC_REG_ME_LOGO_ADDRX_0, i,
		       READ_FRC_REG(i));
}

/*
 * alloc resolve memory
 */
int frc_buf_alloc(struct frc_dev_s *devp)
{
	if (devp->buf.cma_mem_size == 0) {
		pr_frc(0, "cma_mem_size err\n");
		return -1;
	}

	/* The total memory size is 0xa000000 160M
	 * buffer 8+8 release memory size: 0x4ee0000 78M
	 * buffer 12+4 release memory size: 0x2770000 39M
	 * buffer 14+2 release memory size: 0x13b8000 19M
	 */
	devp->buf.cma_mem_size2 = 0x2770000;
	devp->buf.cma_mem_size = 0xa000000 -
		devp->buf.cma_mem_size2 - FRC_RDMA_SIZE;

	if (!devp->buf.cma_buf_alloc) {
		devp->buf.cma_mem_paddr_pages =
			dma_alloc_from_contiguous(&devp->pdev->dev,
				devp->buf.cma_mem_size >> PAGE_SHIFT, 0, 0);
		if (!devp->buf.cma_mem_paddr_pages) {
			devp->buf.cma_mem_size = 0;
			pr_frc(0, "cma_alloc buffer1 fail\n");
			return -1;
		}
		devp->buf.cma_buf_alloc = 1;
		/*physical pages address to real address*/
		devp->buf.cma_mem_paddr_start =
			page_to_phys(devp->buf.cma_mem_paddr_pages);
		pr_frc(0, "cma paddr_start=0x%lx size:0x%x\n",
			(ulong)devp->buf.cma_mem_paddr_start, devp->buf.cma_mem_size);
		// rdma buf alloc 1M
		frc_rdma_alloc_buf();
	}
	if (!devp->buf.cma_buf_alloc2) {
		devp->buf.cma_mem_paddr_pages2 =
			dma_alloc_from_contiguous(&devp->pdev->dev,
				devp->buf.cma_mem_size2 >> PAGE_SHIFT, 0, 0);
		if (!devp->buf.cma_mem_paddr_pages2) {
			devp->buf.cma_mem_size2 = 0;
			pr_frc(0, "cma_alloc buffer2 fail\n");
			return -1;
		}
		devp->buf.cma_buf_alloc2 = 1;
		/*physical pages address to real address*/
		devp->buf.cma_mem_paddr_start2 =
			page_to_phys(devp->buf.cma_mem_paddr_pages2);
		// pr_frc(0, "cma paddr_start2=0x%lx size:0x%x\n",
		// (ulong)devp->buf.cma_mem_paddr_start2, devp->buf.cma_mem_size2);
	}

	return 0;
}

int frc_buf_release(struct frc_dev_s *devp)
{
	if (devp->buf.cma_mem_size &&
		devp->buf.cma_mem_paddr_pages && devp->buf.cma_buf_alloc) {
		dma_release_from_contiguous(&devp->pdev->dev,
			devp->buf.cma_mem_paddr_pages, devp->buf.cma_mem_size >> PAGE_SHIFT);
		devp->buf.cma_mem_paddr_pages = NULL;
		devp->buf.cma_mem_paddr_start = 0;
		devp->buf.cma_mem_alloced = 0;
		devp->buf.cma_buf_alloc = 0;
		pr_frc(2, "%s buffer1 released\n", __func__);
		//rdma buf release
		frc_rdma_release_buf();
	} else {
		pr_frc(0, "%s no buffer exist\n", __func__);
	}

	if (devp->buf.cma_mem_size2 &&
		devp->buf.cma_mem_paddr_pages2 && devp->buf.cma_buf_alloc2) {
		dma_release_from_contiguous(&devp->pdev->dev,
			devp->buf.cma_mem_paddr_pages2, devp->buf.cma_mem_size2 >> PAGE_SHIFT);
		devp->buf.cma_mem_paddr_pages2 = NULL;
		devp->buf.cma_mem_paddr_start2 = 0;
		devp->buf.cma_mem_alloced = 0;
		devp->buf.cma_buf_alloc2 = 0;
		pr_frc(2, "%s buffer2 released\n", __func__);
	} else {
		pr_frc(0, "%s no buffer2 exist\n", __func__);
	}

	return 0;
}

/*
 * calculate buffer depend on input source
 */
int frc_buf_calculate(struct frc_dev_s *devp)
{
	u32 i;
	u32 align_hsize;
	u32 align_vsize;
	u32 temp;
	int log = 2;
	u32 ratio;

	if (!devp)
		return -1;

	if (devp->buf.memc_comprate == 0)
		devp->buf.memc_comprate = FRC_COMPRESS_RATE;
	if (devp->buf.me_comprate == 0)
		devp->buf.me_comprate = FRC_COMPRESS_RATE_ME;
	if (devp->buf.mc_c_comprate == 0)
		devp->buf.mc_c_comprate = FRC_COMPRESS_RATE_MC_C;
	if (devp->buf.mc_y_comprate == 0)
		devp->buf.mc_y_comprate = FRC_COMPRESS_RATE_MC_Y;

	/*size initial, alloc max support size accordint to vout*/
	devp->buf.in_hsize = devp->out_sts.vout_width;
	devp->buf.in_vsize = devp->out_sts.vout_height;

	/*align size*/
	devp->buf.in_align_hsize = roundup(devp->buf.in_hsize, FRC_HVSIZE_ALIGN_SIZE);
	devp->buf.in_align_vsize = roundup(devp->buf.in_vsize, FRC_HVSIZE_ALIGN_SIZE);
	align_hsize = devp->buf.in_align_hsize;
	align_vsize = devp->buf.in_align_vsize;

	if (devp->out_sts.vout_width > 1920 && devp->out_sts.vout_height > 1080) {
		devp->buf.me_hsize =
			roundup(devp->buf.in_align_hsize, FRC_ME_SD_RATE_4K) / FRC_ME_SD_RATE_4K;
		devp->buf.me_vsize =
			roundup(devp->buf.in_align_vsize, FRC_ME_SD_RATE_4K) / FRC_ME_SD_RATE_4K;
		ratio = 32;
	} else {
		devp->buf.me_hsize =
			roundup(devp->buf.in_align_hsize, FRC_ME_SD_RATE_HD) / FRC_ME_SD_RATE_HD;
		devp->buf.me_vsize =
			roundup(devp->buf.in_align_vsize, FRC_ME_SD_RATE_HD) / FRC_ME_SD_RATE_HD;
		ratio = 16;
	}

	devp->buf.logo_hsize =
		roundup(devp->buf.me_hsize, FRC_LOGO_SD_RATE) / FRC_LOGO_SD_RATE;
	devp->buf.logo_vsize =
		roundup(devp->buf.me_vsize, FRC_LOGO_SD_RATE) / FRC_LOGO_SD_RATE;

	devp->buf.hme_hsize =
		roundup(devp->buf.me_hsize, FRC_HME_SD_RATE) / FRC_HME_SD_RATE;
	devp->buf.hme_vsize =
		roundup(devp->buf.me_vsize, FRC_HME_SD_RATE) / FRC_HME_SD_RATE;

	devp->buf.me_blk_hsize =
		roundup(devp->buf.me_hsize, 4) / 4;
	devp->buf.me_blk_vsize =
		roundup(devp->buf.me_vsize, 4) / 4;

	devp->buf.hme_blk_hsize =
		roundup(devp->buf.hme_hsize, 4) / 4;
	devp->buf.hme_blk_vsize =
		roundup(devp->buf.hme_vsize, 4) / 4;

	pr_frc(log, "in hv (%d, %d) align (%d, %d)",
	       devp->buf.in_hsize, devp->buf.in_vsize,
	       devp->buf.in_align_hsize, devp->buf.in_align_vsize);
	pr_frc(log, "me hv (%d, %d)", devp->buf.me_hsize, devp->buf.me_vsize);
	pr_frc(log, "logo hv (%d, %d)", devp->buf.logo_hsize, devp->buf.logo_vsize);
	pr_frc(log, "hme hv (%d, %d)", devp->buf.hme_hsize, devp->buf.hme_vsize);
	pr_frc(log, "me blk hv (%d, %d)", devp->buf.me_blk_hsize,
	       devp->buf.me_blk_vsize);
	pr_frc(log, "hme blk (%d, %d)", devp->buf.hme_blk_hsize,
	       devp->buf.hme_blk_vsize);

	/* ------------ cal buffer start -----------------*/
	pr_frc(0, "dc_rate:(me:%d,mc_y:%d,mc_c:%d)\n",
		devp->buf.me_comprate, devp->buf.mc_y_comprate,
		devp->buf.mc_c_comprate);

	/*mc y/c/v info buffer, address 64 bytes align*/
	devp->buf.lossy_mc_y_info_buf_size = LOSSY_MC_INFO_LINE_SIZE * FRC_SLICER_NUM;
	devp->buf.lossy_mc_c_info_buf_size = LOSSY_MC_INFO_LINE_SIZE * FRC_SLICER_NUM;
	devp->buf.lossy_mc_v_info_buf_size = LOSSY_MC_INFO_LINE_SIZE * FRC_SLICER_NUM;

	devp->buf.total_size += devp->buf.lossy_mc_y_info_buf_size;
	devp->buf.total_size += devp->buf.lossy_mc_c_info_buf_size;
	devp->buf.total_size += devp->buf.lossy_mc_v_info_buf_size;
	pr_frc(log, "lossy_mc_info_buf_size=%d, line buf:%d all:%d\n",
	       devp->buf.lossy_mc_y_info_buf_size, LOSSY_MC_INFO_LINE_SIZE,
	       devp->buf.lossy_mc_y_info_buf_size * 2);

	/*me info buffer*/
	devp->buf.lossy_me_x_info_buf_size = LOSSY_MC_INFO_LINE_SIZE * FRC_SLICER_NUM;
	devp->buf.total_size += devp->buf.lossy_me_x_info_buf_size;

	pr_frc(log, "lossy_me_info_buf_size=%d, line buf:%d all:%d\n",
	       devp->buf.lossy_me_x_info_buf_size, LOSSY_MC_INFO_LINE_SIZE,
	       devp->buf.lossy_me_x_info_buf_size * 1);

	temp = (align_hsize * FRC_MC_BITS_NUM + 511) / 8;
	/*lossy mc data buffer*/
	for (i = 0; i < FRC_TOTAL_BUF_NUM; i++) {
		devp->buf.lossy_mc_y_data_buf_size[i] = ALIGN_4K * ratio +
			(temp * align_vsize * devp->buf.mc_y_comprate) / 100;
		devp->buf.lossy_mc_c_data_buf_size[i] = ALIGN_4K * ratio +
			(temp * align_vsize * devp->buf.mc_c_comprate) / 100;
		devp->buf.lossy_mc_v_data_buf_size[i] = 0;//ALIGN_4K * 4 +
			//(temp * align_vsize * FRC_COMPRESS_RATE) / 100;
		devp->buf.total_size += devp->buf.lossy_mc_y_data_buf_size[i];
		devp->buf.total_size += devp->buf.lossy_mc_c_data_buf_size[i];
		devp->buf.total_size += devp->buf.lossy_mc_v_data_buf_size[i];
	}
	pr_frc(log, "lossy_mc_data_buf_size=%d, line buf:%d all:%d\n",
	       devp->buf.lossy_mc_c_data_buf_size[0], temp,
	       devp->buf.lossy_mc_c_data_buf_size[0] * FRC_TOTAL_BUF_NUM * 3);

	temp = (devp->buf.me_hsize * FRC_ME_BITS_NUM + 511) / 8;
	/*lossy me data buffer*/
	for (i = 0; i < FRC_TOTAL_BUF_NUM; i++) {
		devp->buf.lossy_me_data_buf_size[i] = ALIGN_4K * ratio +
			(temp * devp->buf.me_vsize * devp->buf.me_comprate) / 100;
		devp->buf.total_size += devp->buf.lossy_me_data_buf_size[i];
	}
	pr_frc(log, "lossy_me_data_buf_size=%d, line buf:%d all:%d\n",
	       devp->buf.lossy_me_data_buf_size[0], temp,
	       devp->buf.lossy_me_data_buf_size[0] * FRC_TOTAL_BUF_NUM);

	/*mc link buffer*/
	temp = (roundup(devp->buf.lossy_mc_y_data_buf_size[0], ALIGN_4K) / ALIGN_4K) * 4;
	for (i = 0; i < FRC_TOTAL_BUF_NUM; i++) {
		devp->buf.lossy_mc_y_link_buf_size[i] = temp * 1;
		devp->buf.lossy_mc_c_link_buf_size[i] = temp * 1;
		devp->buf.lossy_mc_v_link_buf_size[i] = 0;//temp * 1;
		devp->buf.total_size += devp->buf.lossy_mc_y_link_buf_size[i] * 2;
		devp->buf.total_size += devp->buf.lossy_mc_v_link_buf_size[i];
	}
	pr_frc(log, "lossy_mc_link_buf_size=%d ,line buf:%d all:%d\n",
	       devp->buf.lossy_mc_y_link_buf_size[0], temp,
	       devp->buf.lossy_mc_y_link_buf_size[0] * FRC_TOTAL_BUF_NUM * 2);

	/*me link buffer*/
	temp = (roundup(devp->buf.lossy_me_data_buf_size[0], ALIGN_4K) / ALIGN_4K) * 4;
	for (i = 0; i < FRC_TOTAL_BUF_NUM; i++) {
		devp->buf.lossy_me_link_buf_size[i] = temp;
		devp->buf.total_size += devp->buf.lossy_me_link_buf_size[i];
	}
	pr_frc(log, "lossy_me_link_buf_size=%d ,line buf:%d all:%d\n",
	       devp->buf.lossy_me_link_buf_size[0], temp,
	       devp->buf.lossy_me_link_buf_size[0] * FRC_TOTAL_BUF_NUM);

	/*norm hme data buffer*/
	temp = (devp->buf.hme_hsize * FRC_ME_BITS_NUM + 511) / 8;
	for (i = 0; i < FRC_TOTAL_BUF_NUM; i++) {
		devp->buf.norm_hme_data_buf_size[i] = temp * devp->buf.hme_vsize;
		devp->buf.total_size += devp->buf.norm_hme_data_buf_size[i];
	}
	pr_frc(log, "norm_hme_data_buffer=%d ,line buf:%d all:%d\n",
	       devp->buf.norm_hme_data_buf_size[0], temp,
	       devp->buf.norm_hme_data_buf_size[0] * FRC_TOTAL_BUF_NUM);

	/*norm memv buffer*/
	temp = (devp->buf.me_blk_hsize * 64 + 511) / 8;
	for (i = 0; i < FRC_MEMV_BUF_NUM; i++) {//-----------------may be 5 buffer
		devp->buf.norm_memv_buf_size[i] = temp * devp->buf.me_blk_vsize;
		devp->buf.total_size += devp->buf.norm_memv_buf_size[i];
	}
	pr_frc(log, "norm_memv_buf_size=%d ,line buf:%d all:%d\n",
	       devp->buf.norm_memv_buf_size[0], temp,
	       devp->buf.norm_memv_buf_size[0] * FRC_MEMV_BUF_NUM);

	/*norm hmemv buffer*/
	temp = (devp->buf.hme_blk_hsize * 64 + 511) / 8;
	for (i = 0; i < FRC_MEMV2_BUF_NUM; i++) {
		devp->buf.norm_hmemv_buf_size[i] = temp * devp->buf.hme_blk_vsize;
		devp->buf.total_size += devp->buf.norm_hmemv_buf_size[i];
	}
	pr_frc(log, "norm_hmemv_buf_size=%d ,line buf:%d all:%d\n",
	       devp->buf.norm_hmemv_buf_size[0], temp,
	       devp->buf.norm_hmemv_buf_size[0] * FRC_MEMV2_BUF_NUM);

	/*norm mevp buffer*/
	temp = (devp->buf.me_blk_hsize * 64 + 511) / 8;
	for (i = 0; i < FRC_MEVP_BUF_NUM; i++) {
		devp->buf.norm_mevp_out_buf_size[i] = temp * devp->buf.me_blk_vsize;
		devp->buf.total_size += devp->buf.norm_mevp_out_buf_size[i];
	}
	pr_frc(log, "norm_mevp_out_buf_size=%d ,line buf:%d all:%d\n",
	       devp->buf.norm_mevp_out_buf_size[0], temp,
	       devp->buf.norm_mevp_out_buf_size[0] * FRC_MEVP_BUF_NUM);

	/*norm iplogo buffer*/
	temp = (devp->buf.logo_hsize * 1 + 511) / 8;
	for (i = 0; i < FRC_TOTAL_BUF_NUM; i++) {
		devp->buf.norm_iplogo_buf_size[i] = temp * devp->buf.logo_vsize;
		devp->buf.total_size += devp->buf.norm_iplogo_buf_size[i];
	}
	pr_frc(log, "norm_iplogo_buf_size=%d ,line buf:%d all:%d\n",
	       devp->buf.norm_iplogo_buf_size[0], temp,
	       devp->buf.norm_iplogo_buf_size[0] * FRC_TOTAL_BUF_NUM);

	/*norm logo irr buffer*/
	temp = (devp->buf.logo_hsize * 6 + 511) / 8;
	devp->buf.norm_logo_irr_buf_size = temp * devp->buf.logo_vsize;
	devp->buf.total_size += devp->buf.norm_logo_irr_buf_size;
	pr_frc(log, "norm_logo_irr_buf_size=%d ,line buf:%d\n",
	       devp->buf.norm_logo_irr_buf_size, temp);

	/*norm logo scc buffer*/
	temp = (devp->buf.logo_hsize * 5 + 511) / 8;
	devp->buf.norm_logo_scc_buf_size = temp * devp->buf.logo_vsize;
	devp->buf.total_size += devp->buf.norm_logo_scc_buf_size;
	pr_frc(log, "norm_logo_scc_buf_size=%d ,line buf:%d\n",
	       devp->buf.norm_logo_scc_buf_size, temp);

	/*norm iplogo buffer*/
	temp = (devp->buf.me_blk_hsize + 1 + 511) / 8;
	for (i = 0; i < FRC_TOTAL_BUF_NUM; i++) {
		devp->buf.norm_melogo_buf_size[i] = temp * devp->buf.me_blk_vsize;
		devp->buf.total_size += devp->buf.norm_melogo_buf_size[i];
	}
	pr_frc(log, "norm_melogo_buf_size=%d ,line buf:%d all:%d\n",
	       devp->buf.norm_melogo_buf_size[0], temp,
	       devp->buf.norm_melogo_buf_size[0] * FRC_TOTAL_BUF_NUM);

	pr_frc(0, "total_size=%d\n", devp->buf.total_size);

	return 0;
}

int frc_buf_distribute(struct frc_dev_s *devp)
{
	u32 i;
	u32 real_onebuf_size;
	u32 paddr = 0, base, base2;
	u32 paddr2 = 0;
	int log = 2;

	/*----------------- buffer alloc------------------*/
	base = devp->buf.cma_mem_paddr_start;
	base2 = devp->buf.cma_mem_paddr_start2;
	/*mc y/c/v me info buffer, address 64 bytes align*/
	devp->buf.lossy_mc_y_info_buf_paddr = paddr;
	pr_frc(log, "lossy_mc_y_info_buf_paddr:0x%x", paddr);
	paddr += roundup(devp->buf.lossy_mc_y_info_buf_size, ALIGN_4K);
	devp->buf.lossy_mc_c_info_buf_paddr = paddr;
	pr_frc(log, "lossy_mc_c_info_buf_paddr:0x%x", paddr);
	paddr += roundup(devp->buf.lossy_mc_c_info_buf_size, ALIGN_4K);
	devp->buf.lossy_mc_v_info_buf_paddr = paddr;
	pr_frc(log, "lossy_mc_v_info_buf_paddr:0x%x", paddr);
	paddr += roundup(devp->buf.lossy_mc_v_info_buf_size, ALIGN_4K);
	devp->buf.lossy_me_x_info_buf_paddr = paddr;
	pr_frc(log, "lossy_me_x_info_buf_paddr:0x%x", paddr);
	paddr += roundup(devp->buf.lossy_me_x_info_buf_size, ALIGN_4K);

	/*norm buffer*/
	paddr = roundup(paddr, ALIGN_4K * 16);/*secure size need 64K align*/
	real_onebuf_size = roundup(devp->buf.norm_hme_data_buf_size[0], ALIGN_4K);
	for (i = 0; i < FRC_TOTAL_BUF_NUM; i++) {
		devp->buf.norm_hme_data_buf_paddr[i] = paddr;
		pr_frc(log, "norm_hme_data_buf_paddr[%d]:0x%x\n", i, paddr);
		paddr += real_onebuf_size;
	}

	real_onebuf_size = roundup(devp->buf.norm_memv_buf_size[0], ALIGN_4K);
	for (i = 0; i < FRC_MEMV_BUF_NUM; i++) {
		devp->buf.norm_memv_buf_paddr[i] = paddr;
		pr_frc(log, "norm_memv_buf_paddr[%d]:0x%x\n", i, paddr);
		paddr += real_onebuf_size;
	}

	real_onebuf_size = roundup(devp->buf.norm_hmemv_buf_size[0], ALIGN_4K);
	for (i = 0; i < FRC_MEMV2_BUF_NUM; i++) {
		devp->buf.norm_hmemv_buf_paddr[i] = paddr;
		pr_frc(log, "norm_hmemv_buf_paddr[%d]:0x%x\n", i, paddr);
		paddr += real_onebuf_size;
	}

	real_onebuf_size = roundup(devp->buf.norm_mevp_out_buf_size[0], ALIGN_4K);
	for (i = 0; i < FRC_MEVP_BUF_NUM; i++) {
		devp->buf.norm_mevp_out_buf_paddr[i] = paddr;
		pr_frc(log, "norm_mevp_out_buf_paddr[%d]:0x%x\n", i, paddr);
		paddr += real_onebuf_size;
	}

	/*ip logo*/
	paddr = roundup(paddr, ALIGN_4K);
	real_onebuf_size = roundup(devp->buf.norm_iplogo_buf_size[0], ALIGN_4K);
	for (i = 0; i < FRC_TOTAL_BUF_NUM; i++) {
		devp->buf.norm_iplogo_buf_paddr[i] = paddr;
		pr_frc(log, "norm_iplogo_buf_paddr[%d]:0x%x\n", i, paddr);
		paddr += real_onebuf_size;
	}
	/*logo irr*/
	paddr = roundup(paddr, ALIGN_4K);
	real_onebuf_size = roundup(devp->buf.norm_logo_irr_buf_size, ALIGN_4K);
	devp->buf.norm_logo_irr_buf_paddr = paddr;
	pr_frc(log, "norm_logo_irr_buf_paddr:0x%x\n", paddr);
	paddr += real_onebuf_size;

	/*logo ssc*/
	paddr = roundup(paddr, ALIGN_4K);
	real_onebuf_size = roundup(devp->buf.norm_logo_scc_buf_size, ALIGN_4K);
	devp->buf.norm_logo_scc_buf_paddr = paddr;
	pr_frc(log, "norm_logo_scc_buf_paddr:0x%x\n", paddr);
	paddr += real_onebuf_size;

	/*melogo*/
	paddr = roundup(paddr, ALIGN_4K);
	real_onebuf_size = roundup(devp->buf.norm_melogo_buf_size[0], ALIGN_4K);
	for (i = 0; i < FRC_TOTAL_BUF_NUM; i++) {
		devp->buf.norm_melogo_buf_paddr[i] = paddr;
		pr_frc(log, "norm_melogo_buf_paddr[%d]:0x%x\n", i, paddr);
		paddr += real_onebuf_size;
	}

	paddr = roundup(paddr, ALIGN_4K * 16);/*secure size need 64K align*/
	/*link buffer*/
	real_onebuf_size = roundup(devp->buf.lossy_mc_y_link_buf_size[0], ALIGN_4K);
	for (i = 0; i < FRC_TOTAL_BUF_NUM; i++) {
		devp->buf.lossy_mc_y_link_buf_paddr[i] = paddr;
		pr_frc(log, "lossy_mc_y_link_buf_paddr[%d]:0x%x\n", i, paddr);
		paddr += real_onebuf_size;
	}

	real_onebuf_size = roundup(devp->buf.lossy_mc_c_link_buf_size[0], ALIGN_4K);
	for (i = 0; i < FRC_TOTAL_BUF_NUM; i++) {
		devp->buf.lossy_mc_c_link_buf_paddr[i] = paddr;
		pr_frc(log, "lossy_mc_c_link_buf_paddr[%d]:0x%x\n", i, paddr);
		paddr += real_onebuf_size;
	}

	real_onebuf_size = roundup(devp->buf.lossy_mc_v_link_buf_size[0], ALIGN_4K);
	for (i = 0; i < FRC_TOTAL_BUF_NUM; i++) {
		devp->buf.lossy_mc_v_link_buf_paddr[i] = paddr;
		pr_frc(log, "lossy_mc_v_link_buf_paddr[%d]:0x%x\n", i, paddr);
		paddr += real_onebuf_size;
	}

	real_onebuf_size = roundup(devp->buf.lossy_me_link_buf_size[0], ALIGN_4K);
	for (i = 0; i < FRC_TOTAL_BUF_NUM; i++) {
		devp->buf.lossy_me_link_buf_paddr[i] = paddr;
		pr_frc(log, "lossy_me_link_buf_paddr[%d]:0x%x\n", i, paddr);
		paddr += real_onebuf_size;
	}

	/*lossy lossy_mc_y data buffer*/
	/*0-7 data buffer*/
	paddr = roundup(paddr, ALIGN_4K * 16);/*secure size need 64K align*/
	real_onebuf_size = roundup(devp->buf.lossy_mc_y_data_buf_size[0], ALIGN_4K);
	for (i = 0; i < FRC_RE_BUF_NUM; i++) {
		devp->buf.lossy_mc_y_data_buf_paddr[i] = paddr;
		pr_frc(log, "lossy_mc_y_data_buf_paddr[%d]:0x%x\n", i, paddr);
		paddr += real_onebuf_size;
	}

	/*lossy lossy_mc_c data buffer*/
	real_onebuf_size = roundup(devp->buf.lossy_mc_c_data_buf_size[0], ALIGN_4K);
	for (i = 0; i < FRC_RE_BUF_NUM; i++) {
		devp->buf.lossy_mc_c_data_buf_paddr[i] = paddr;
		pr_frc(log, "lossy_mc_c_data_buf_paddr[%d]:0x%x\n", i, paddr);
		paddr += real_onebuf_size;
	}
	/*lossy lossy_mc_v data buffer*/
	real_onebuf_size = roundup(devp->buf.lossy_mc_v_data_buf_size[0], ALIGN_4K);
	for (i = 0; i < FRC_RE_BUF_NUM; i++) {
		devp->buf.lossy_mc_v_data_buf_paddr[i] = paddr;
		pr_frc(log, "lossy_mc_v_data_buf_paddr[%d]:0x%x\n", i, paddr);
		paddr += real_onebuf_size;
	}
	/*lossy lossy_me data buffer*/
	real_onebuf_size = roundup(devp->buf.lossy_me_data_buf_size[0], ALIGN_4K);
	for (i = 0; i < FRC_RE_BUF_NUM; i++) {
		devp->buf.lossy_me_data_buf_paddr[i] = paddr;
		pr_frc(log, "lossy_me_data_buf_paddr[%d]:0x%x\n", i, paddr);
		paddr += real_onebuf_size;
	}

	paddr2 = roundup(paddr2, ALIGN_4K * 16);
	// 8-15 data buffer
	real_onebuf_size = roundup(devp->buf.lossy_mc_y_data_buf_size[0], ALIGN_4K);
	for (i = FRC_RE_BUF_NUM; i < FRC_TOTAL_BUF_NUM; i++) {
		devp->buf.lossy_mc_y_data_buf_paddr[i] = paddr2;
		pr_frc(log, "lossy_mc_y_data_buf_paddr[%d]:0x%x\n", i, paddr2);
		paddr2 += real_onebuf_size;
	}
	/*lossy lossy_mc_c data buffer*/
	real_onebuf_size = roundup(devp->buf.lossy_mc_c_data_buf_size[0], ALIGN_4K);
	for (i = FRC_RE_BUF_NUM; i < FRC_TOTAL_BUF_NUM; i++) {
		devp->buf.lossy_mc_c_data_buf_paddr[i] = paddr2;
		pr_frc(log, "lossy_mc_c_data_buf_paddr[%d]:0x%x\n", i, paddr2);
		paddr2 += real_onebuf_size;
	}
	/*lossy lossy_mc_v data buffer*/
	real_onebuf_size = roundup(devp->buf.lossy_mc_v_data_buf_size[0], ALIGN_4K);
	for (i = FRC_RE_BUF_NUM; i < FRC_TOTAL_BUF_NUM; i++) {
		devp->buf.lossy_mc_v_data_buf_paddr[i] = paddr2;
		pr_frc(log, "lossy_mc_v_data_buf_paddr[%d]:0x%x\n", i, paddr2);
		paddr2 += real_onebuf_size;
	}
	/*lossy lossy_me data buffer*/
	real_onebuf_size = roundup(devp->buf.lossy_me_data_buf_size[0], ALIGN_4K);
	for (i = FRC_RE_BUF_NUM; i < FRC_TOTAL_BUF_NUM; i++) {
		devp->buf.lossy_me_data_buf_paddr[i] = paddr2;
		pr_frc(log, "lossy_me_data_buf_paddr[%d]:0x%x\n", i, paddr2);
		paddr2 += real_onebuf_size;
	}

	paddr = roundup(paddr, ALIGN_4K * 16);
	paddr2 = roundup(paddr2, ALIGN_4K * 16);/*secure size need 64K align*/

	devp->buf.real_total_size = paddr + paddr2;
	if (devp->buf.real_total_size > devp->buf.cma_mem_size + devp->buf.cma_mem_size2)
		pr_frc(0, "buf err: need %d, cur size:%d\n", paddr + paddr2,
			devp->buf.cma_mem_size + devp->buf.cma_mem_size2);
	else
		pr_frc(0, "%s base:0x%x base2:0x%x real_total_size:0x%x(%d)\n",
			__func__, base, base2, paddr + paddr2, paddr + paddr2);

	return 0;
}

/*
 * config data link buffer
 */
int frc_buf_mapping_tab_init(struct frc_dev_s *devp)
{
	u32 i, j, k = 0;
	phys_addr_t cma_paddr = 0, cma_paddr2 = 0;
	dma_addr_t paddr;
	u8 *cma_vaddr = 0, *cma_vaddr2 = 0;
	u32 vmap_offset_start = 0, vmap_offset_end;
	u32 *linktab_vaddr = NULL;
	u8 *p = NULL;
	u32 data_buf_addr, data_buf_size;
	u32 link_tab_all_size;
	u32 log = 2;
	//u32 *init_start_addr;

	cma_paddr = devp->buf.cma_mem_paddr_start;
	cma_paddr2 = devp->buf.cma_mem_paddr_start2;
	link_tab_all_size =
		devp->buf.lossy_mc_y_data_buf_paddr[0] - devp->buf.lossy_mc_y_link_buf_paddr[0];
	pr_frc(log, "paddr start:0x%lx, link start=0x%08x - 0x%08x, size:0x%x\n",
	       (ulong)devp->buf.cma_mem_paddr_start,
	       devp->buf.lossy_mc_y_link_buf_paddr[0], devp->buf.lossy_mc_y_data_buf_paddr[0],
	       link_tab_all_size);

	if (link_tab_all_size <= 0) {
		pr_frc(0, "link buf err\n");
		return -1;
	}

	vmap_offset_start = devp->buf.lossy_mc_y_link_buf_paddr[0];
	vmap_offset_end = devp->buf.lossy_mc_y_data_buf_paddr[0];
	cma_vaddr = frc_buf_vmap(cma_paddr, vmap_offset_end);
	cma_vaddr2 = frc_buf_vmap(cma_paddr2, vmap_offset_end);
	pr_frc(0, "map: paddr=0x%lx, vaddr=0x%lx, vaddr2=0x%lx, link tab size=0x%x (%d)\n",
		(ulong)cma_paddr, (ulong)cma_vaddr, (ulong)cma_vaddr2,
		link_tab_all_size, link_tab_all_size);

	//init_start_addr = (u32 *)(cma_vaddr + vmap_offset_start);
	//for (i = 0; i < link_tab_all_size; i++) {
	//	*init_start_addr = cma_paddr + devp->buf.lossy_mc_y_data_buf_paddr[0];
	//	init_start_addr++;
	//}
	memset(cma_vaddr + vmap_offset_start, 0, link_tab_all_size);
	memset(cma_vaddr2 + vmap_offset_start, 0, link_tab_all_size);

	/*split data buffer and fill to link mc buffer: mc y*/
	data_buf_size = roundup(devp->buf.lossy_mc_y_data_buf_size[0], ALIGN_4K);
	if (data_buf_size > 0) {
		pr_frc(log, "lossy_mc_y_data_buf_size:0x%x (%d)\n", data_buf_size, data_buf_size);
		for (i = 0; i < FRC_TOTAL_BUF_NUM; i++) {
			if (i < FRC_RE_BUF_NUM)
				data_buf_addr =
				(cma_paddr + devp->buf.lossy_mc_y_data_buf_paddr[i]) & 0xffffffff;
			else
				data_buf_addr =
				(cma_paddr2 + devp->buf.lossy_mc_y_data_buf_paddr[i]) & 0xffffffff;
			p = cma_vaddr + devp->buf.lossy_mc_y_link_buf_paddr[i];
			paddr = cma_paddr + devp->buf.lossy_mc_y_link_buf_paddr[i];
			linktab_vaddr = (u32 *)p;
			k = 0;
			for (j = 0; j < data_buf_size; j += ALIGN_4K) {
				*linktab_vaddr = data_buf_addr + j;
				linktab_vaddr++;
				k += 4;
			}
			frc_buf_dma_flush(devp, p, paddr, data_buf_size, DMA_TO_DEVICE);
			//pr_frc(log, "link buf size:%d, data div4k size= %d\n",
			//       devp->buf.lossy_mc_y_link_buf_size[i], k);
		}
	}

	data_buf_size = roundup(devp->buf.lossy_mc_c_data_buf_size[0], ALIGN_4K);
	if (data_buf_size > 0) {
		pr_frc(log, "lossy_mc_c_data_buf_size:0x%x (%d)\n", data_buf_size, data_buf_size);
		for (i = 0; i < FRC_TOTAL_BUF_NUM; i++) {
			if (i < FRC_RE_BUF_NUM)
				data_buf_addr =
				(cma_paddr + devp->buf.lossy_mc_c_data_buf_paddr[i]) & 0xffffffff;
			else
				data_buf_addr =
				(cma_paddr2 + devp->buf.lossy_mc_c_data_buf_paddr[i]) & 0xffffffff;
			p = cma_vaddr + devp->buf.lossy_mc_c_link_buf_paddr[i];
			paddr = cma_paddr + devp->buf.lossy_mc_y_link_buf_paddr[i];
			linktab_vaddr = (u32 *)p;
			k = 0;
			for (j = 0; j < data_buf_size; j += ALIGN_4K) {
				*linktab_vaddr = data_buf_addr + j;
				linktab_vaddr++;
				k += 4;
			}
			//pr_frc(log, "link buf size:%d, data div4k size= %d\n",
			//       devp->buf.lossy_mc_y_link_buf_size[i], k);
			frc_buf_dma_flush(devp, p, paddr, data_buf_size, DMA_TO_DEVICE);
		}
	}

	data_buf_size = roundup(devp->buf.lossy_mc_v_data_buf_size[0], ALIGN_4K);
	if (data_buf_size > 0) {
		pr_frc(log, "lossy_mc_v_data_buf_size:0x%x (%d)\n", data_buf_size, data_buf_size);
		for (i = 0; i < FRC_TOTAL_BUF_NUM; i++) {
			if (i < FRC_RE_BUF_NUM)
				data_buf_addr =
				(cma_paddr + devp->buf.lossy_mc_v_data_buf_paddr[i]) & 0xffffffff;
			else
				data_buf_addr =
				(cma_paddr2 + devp->buf.lossy_mc_v_data_buf_paddr[i]) & 0xffffffff;
			p = cma_vaddr + devp->buf.lossy_mc_v_link_buf_paddr[i];
			paddr = cma_paddr + devp->buf.lossy_mc_y_link_buf_paddr[i];
			linktab_vaddr = (u32 *)p;
			k = 0;
			for (j = 0; j < data_buf_size; j += ALIGN_4K) {
				*linktab_vaddr = data_buf_addr + j;
				linktab_vaddr++;
				k += 4;
			}
			//pr_frc(log, "link buf size:%d, data div4k size= %d\n",
			//       devp->buf.lossy_mc_y_link_buf_size[i], k);
			frc_buf_dma_flush(devp, p, paddr, data_buf_size, DMA_TO_DEVICE);
		}
	}
	data_buf_size = roundup(devp->buf.lossy_me_data_buf_size[0], ALIGN_4K);
	if (data_buf_size > 0) {
		pr_frc(log, "lossy_me_data_buf_size:0x%x (%d)\n", data_buf_size, data_buf_size);
		for (i = 0; i < FRC_TOTAL_BUF_NUM; i++) {
			if (i < FRC_RE_BUF_NUM)
				data_buf_addr =
				(cma_paddr + devp->buf.lossy_me_data_buf_paddr[i]) & 0xffffffff;
			else
				data_buf_addr =
				(cma_paddr2 + devp->buf.lossy_me_data_buf_paddr[i]) & 0xffffffff;
			p = cma_vaddr + devp->buf.lossy_me_link_buf_paddr[i];
			paddr = cma_paddr + devp->buf.lossy_mc_y_link_buf_paddr[i];
			linktab_vaddr = (u32 *)p;
			k = 0;
			for (j = 0; j < data_buf_size; j += ALIGN_4K) {
				*linktab_vaddr = data_buf_addr + j;
				linktab_vaddr++;
				k += 4;
			}
			frc_buf_dma_flush(devp, p, paddr, data_buf_size, DMA_TO_DEVICE);
			//pr_frc(log, "link buf size:%d, data div4k size= %d\n",
			//       devp->buf.lossy_mc_y_link_buf_size[i], k);
		}
	}
	frc_buf_unmap((u32 *)cma_vaddr);
	frc_buf_unmap((u32 *)cma_vaddr2);

	return 0;
}

/*
 * config buffer address to every hw buffer register
 *
 */
int frc_buf_config(struct frc_dev_s *devp)
{
	u32 i = 0;
	u32 base, base2;
	u32 log = 0;

	if (!devp) {
		pr_frc(0, "%s fail<devp is null>\n", __func__);
		return -1;
	} else if (!devp->buf.cma_mem_alloced) {
		pr_frc(0, "%s fail <cma alloced:%d>\n", __func__, devp->buf.cma_mem_alloced);
		return -1;
	} else if (!devp->buf.cma_mem_paddr_start) {
		pr_frc(0, "%s fail <cma_paddr is null>\n", __func__);
		return -1;
	}
	base = devp->buf.cma_mem_paddr_start;
	base2 = devp->buf.cma_mem_paddr_start2;
	pr_frc(log, "%s cma base:0x%x, base2:0x%x\n", __func__, base, base2);
	/*mc info buffer*/
	WRITE_FRC_REG_BY_CPU(FRC_REG_MC_YINFO_BADDR, base + devp->buf.lossy_mc_y_info_buf_paddr);
	WRITE_FRC_REG_BY_CPU(FRC_REG_MC_CINFO_BADDR, base + devp->buf.lossy_mc_c_info_buf_paddr);
	WRITE_FRC_REG_BY_CPU(FRC_REG_MC_VINFO_BADDR, base + devp->buf.lossy_mc_v_info_buf_paddr);
	WRITE_FRC_REG_BY_CPU(FRC_REG_ME_XINFO_BADDR, base + devp->buf.lossy_me_x_info_buf_paddr);

	/*lossy mc y,c,v data buffer, data buffer needn't config*/
	/*lossy me data buffer, data buffer needn't config*/

	/*lossy mc link buffer*/
	for (i = FRC_REG_MC_YBUF_ADDRX_0; i <= FRC_REG_MC_YBUF_ADDRX_15; i++) {
		WRITE_FRC_REG_BY_CPU(i,
			base + devp->buf.lossy_mc_y_link_buf_paddr[i - FRC_REG_MC_YBUF_ADDRX_0]);
	}
	for (i = FRC_REG_MC_CBUF_ADDRX_0; i <= FRC_REG_MC_CBUF_ADDRX_15; i++)
		WRITE_FRC_REG_BY_CPU(i,
			base + devp->buf.lossy_mc_c_link_buf_paddr[i - FRC_REG_MC_CBUF_ADDRX_0]);

	for (i = FRC_REG_MC_VBUF_ADDRX_0; i <= FRC_REG_MC_VBUF_ADDRX_15; i++)
		WRITE_FRC_REG_BY_CPU(i,
			base + devp->buf.lossy_mc_v_link_buf_paddr[i - FRC_REG_MC_VBUF_ADDRX_0]);

	/*lossy me link buffer*/
	for (i = FRC_REG_ME_BUF_ADDRX_0; i <= FRC_REG_ME_BUF_ADDRX_15; i++)
		WRITE_FRC_REG_BY_CPU(i,
			base + devp->buf.lossy_me_link_buf_paddr[i - FRC_REG_ME_BUF_ADDRX_0]);

	/*norm hme data buffer*/
	for (i = FRC_REG_HME_BUF_ADDRX_0; i <= FRC_REG_HME_BUF_ADDRX_15; i++)
		WRITE_FRC_REG_BY_CPU(i,
			base + devp->buf.norm_hme_data_buf_paddr[i - FRC_REG_HME_BUF_ADDRX_0]);

	/*norm memv buffer*/
	for (i = FRC_REG_ME_NC_UNI_MV_ADDRX_0; i <= FRC_REG_ME_PC_PHS_MV_ADDR; i++)
		WRITE_FRC_REG_BY_CPU(i,
			base + devp->buf.norm_memv_buf_paddr[i - FRC_REG_ME_NC_UNI_MV_ADDRX_0]);

	/*norm hmemv buffer*/
	for (i = FRC_REG_HME_NC_UNI_MV_ADDRX_0; i <= FRC_REG_VP_PF_UNI_MV_ADDR; i++)
		WRITE_FRC_REG_BY_CPU(i,
			base + devp->buf.norm_hmemv_buf_paddr[i - FRC_REG_HME_NC_UNI_MV_ADDRX_0]);

	/*norm mevp buffer*/
	for (i = FRC_REG_VP_MC_MV_ADDRX_0; i <= FRC_REG_VP_MC_MV_ADDRX_1; i++)
		WRITE_FRC_REG_BY_CPU(i,
			base + devp->buf.norm_mevp_out_buf_paddr[i - FRC_REG_VP_MC_MV_ADDRX_0]);

	/*norm iplogo buffer*/
	for (i = FRC_REG_IP_LOGO_ADDRX_0; i <= FRC_REG_IP_LOGO_ADDRX_15; i++)
		WRITE_FRC_REG_BY_CPU(i,
			base + devp->buf.norm_iplogo_buf_paddr[i - FRC_REG_IP_LOGO_ADDRX_0]);

	/*norm logo irr buffer*/
	WRITE_FRC_REG_BY_CPU(FRC_REG_LOGO_IIR_BUF_ADDR, base + devp->buf.norm_logo_irr_buf_paddr);

	/*norm logo scc buffer*/
	WRITE_FRC_REG_BY_CPU(FRC_REG_LOGO_SCC_BUF_ADDR, base + devp->buf.norm_logo_scc_buf_paddr);

	/*norm iplogo buffer*/
	for (i = FRC_REG_ME_LOGO_ADDRX_0; i <= FRC_REG_ME_LOGO_ADDRX_15; i++)
		WRITE_FRC_REG_BY_CPU(i,
			base + devp->buf.norm_melogo_buf_paddr[i - FRC_REG_ME_LOGO_ADDRX_0]);

	frc_buf_mapping_tab_init(devp);

	//pr_frc(0, "%s success!\n", __func__);
	return 0;
}

void frc_mem_dynamic_proc(struct work_struct *work)
{
	u8 buf_ctrl;
	u64 timestamp, timestamp2;

	struct frc_dev_s *devp = get_frc_devp();

	pr_frc(0, "%s buf_ctrl = %d\n", __func__, devp->buf.buf_ctrl);
	buf_ctrl = devp->buf.buf_ctrl; // 0 release buf, 1 alloc buf
	/* HDMI/cvbd/tuner or debug_test*/
	if (devp->in_sts.frc_is_tvin || !frc_buf_test) {
		if (!devp->buf.cma_buf_alloc2) // buffer2 released
			buf_ctrl = 1;
		else
			return;
	}
	if (cur_state == buf_ctrl)
		return;
	cur_state = buf_ctrl;
	if (buf_ctrl) {
		timestamp = sched_clock();
		frc_buf_alloc(devp);
		//frc_buf_mapping_tab_init(devp);
		timestamp2 = sched_clock();
		if (devp->buf.cma_buf_alloc && devp->buf.cma_buf_alloc2)
			devp->buf.cma_mem_alloced = 1;
		pr_frc(0, "%s cma paddr_start2=0x%lx size:0x%x used time:%lld\n",
			__func__, (ulong)devp->buf.cma_mem_paddr_start2,
			devp->buf.cma_mem_size2, timestamp2 - timestamp);
	} else {
		timestamp = sched_clock();
		if (devp->buf.cma_buf_alloc == 1) {
			devp->buf.cma_buf_alloc = 0;   /*keep cma paddr_start*/
			frc_buf_release(devp);
			devp->buf.cma_buf_alloc = 1;
		} else {
			pr_frc(0, "%s release buffer error\n", __func__);
		}
		timestamp2 = sched_clock();
		pr_frc(0, "%s frc buffer2 released, used time:%lld\n",
		__func__, timestamp2 - timestamp);
	}
}
