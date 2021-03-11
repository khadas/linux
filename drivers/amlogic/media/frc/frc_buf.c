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

#include "frc_reg.h"
#include "frc_common.h"
#include "frc_drv.h"
#include "frc_buf.h"


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

	/*info buffer*/
	pr_frc(0, "lossy_mc_y_info_buf_paddr:0x%x\n", devp->buf.lossy_mc_y_info_buf_paddr);
	pr_frc(0, "lossy_mc_c_info_buf_paddr:0x%x\n", devp->buf.lossy_mc_c_info_buf_paddr);
	pr_frc(0, "lossy_mc_v_info_buf_paddr:0x%x\n", devp->buf.lossy_mc_v_info_buf_paddr);
	pr_frc(0, "lossy_me_x_info_buf_paddr:0x%x\n", devp->buf.lossy_me_x_info_buf_paddr);

	/*lossy data buffer*/
	for (i = 0; i < FRC_TOTAL_BUF_NUM; i++)
		pr_frc(0, "lossy_mc_y_data_buf_paddr[%d]:0x%x\n", i,
		       devp->buf.lossy_mc_y_data_buf_paddr[i]);

	for (i = 0; i < FRC_TOTAL_BUF_NUM; i++)
		pr_frc(0, "lossy_mc_c_data_buf_paddr[%d]:0x%x\n", i,
		       devp->buf.lossy_mc_c_data_buf_paddr[i]);

	for (i = 0; i < FRC_TOTAL_BUF_NUM; i++)
		pr_frc(0, "lossy_mc_v_data_buf_paddr[%d]:0x%x\n", i,
		       devp->buf.lossy_mc_v_data_buf_paddr[i]);

	for (i = 0; i < FRC_TOTAL_BUF_NUM; i++)
		pr_frc(0, "lossy_me_data_buf_paddr[%d]:0x%x\n", i,
		       devp->buf.lossy_me_data_buf_paddr[i]);

	/*link buffer*/
	for (i = 0; i < FRC_TOTAL_BUF_NUM; i++)
		pr_frc(0, "lossy_mc_y_link_buf_paddr[%d]:0x%x\n", i,
		       devp->buf.lossy_mc_y_link_buf_paddr[i]);

	for (i = 0; i < FRC_TOTAL_BUF_NUM; i++)
		pr_frc(0, "lossy_mc_c_link_buf_paddr[%d]:0x%x\n", i,
		       devp->buf.lossy_mc_c_link_buf_paddr[i]);

	for (i = 0; i < FRC_TOTAL_BUF_NUM; i++)
		pr_frc(0, "lossy_mc_v_link_buf_paddr[%d]:0x%x\n", i,
		       devp->buf.lossy_mc_v_link_buf_paddr[i]);


	for (i = 0; i < FRC_TOTAL_BUF_NUM; i++)
		pr_frc(0, "lossy_me_link_buf_paddr[%d]:0x%x\n", i,
		       devp->buf.lossy_me_link_buf_paddr[i]);

	/*norm buffer*/
	for (i = 0; i < FRC_TOTAL_BUF_NUM; i++)
		pr_frc(0, "norm_hme_data_buf_paddr[%d]:0x%x\n", i,
		       devp->buf.norm_hme_data_buf_paddr[i]);

	for (i = 0; i < FRC_MEMV_BUF_NUM; i++)
		pr_frc(0, "norm_memv_buf_paddr[%d]:0x%x\n", i,
		       devp->buf.norm_memv_buf_paddr[i]);

	for (i = 0; i < FRC_MEMV_BUF_NUM; i++)
		pr_frc(0, "norm_hmemv_buf_paddr[%d]:0x%x\n", i,
		       devp->buf.norm_hmemv_buf_paddr[i]);

	for (i = 0; i < FRC_MEVP_BUF_NUM; i++)
		pr_frc(0, "norm_mevp_out_buf_paddr[%d]:0x%x\n", i,
		       devp->buf.norm_mevp_out_buf_paddr[i]);

	for (i = 0; i < FRC_TOTAL_BUF_NUM; i++)
		pr_frc(0, "norm_iplogo_buf_paddr[%d]:0x%x\n", i,
		       devp->buf.norm_iplogo_buf_paddr[i]);

	pr_frc(0, "norm_logo_irr_buf_paddr:0x%x\n", devp->buf.norm_logo_irr_buf_paddr);
	pr_frc(0, "norm_logo_scc_buf_paddr:0x%x\n", devp->buf.norm_logo_scc_buf_paddr);

	for (i = 0; i < FRC_TOTAL_BUF_NUM; i++)
		pr_frc(0, "norm_melogo_buf_paddr[%d]:0x%x\n", i,
		       devp->buf.norm_melogo_buf_paddr[i]);

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
	pr_frc(log, "lossy_mc_data_buf_size=%d, all:%d\n",
	       devp->buf.lossy_mc_c_data_buf_size[0],
	       devp->buf.lossy_mc_c_data_buf_size[0] * FRC_TOTAL_BUF_NUM * 2);

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
	       devp->buf.norm_hmemv_buf_size[0] * FRC_MEMV_BUF_NUM);

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
	for (i = FRC_REG_HME_NC_UNI_MV_ADDRX_0; i <= FRC_REG_HME_PC_PHS_MV_ADDR; i++)
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
 * alloc resove memory
 */
int frc_buf_alloc(struct frc_dev_s *devp)
{
	if (devp->buf.cma_mem_size == 0) {
		pr_frc(0, "cma_mem_size err\n");
		return -1;
	}

	devp->buf.cma_mem_paddr_pages =
	dma_alloc_from_contiguous(&devp->pdev->dev, devp->buf.cma_mem_size >> PAGE_SHIFT, 0, 0);

	if (!devp->buf.cma_mem_paddr_pages) {
		devp->buf.cma_mem_size = 0;
		pr_frc(0, "cma_alloc fail\n");
		return -0;
	}
	/*physical pages address to real address*/
	devp->buf.cma_mem_paddr_start = page_to_phys(devp->buf.cma_mem_paddr_pages);
	devp->buf.cma_mem_alloced = 1;
	pr_frc(0, "cma paddr_start=0x%lx size:0x%x\n", (ulong)devp->buf.cma_mem_paddr_start,
	       devp->buf.cma_mem_size);

	return 0;
}

int frc_buf_release(struct frc_dev_s *devp)
{
	if (devp->buf.cma_mem_size && devp->buf.cma_mem_paddr_pages) {
		dma_release_from_contiguous(&devp->pdev->dev, devp->buf.cma_mem_paddr_pages,
					    devp->buf.cma_mem_size >> PAGE_SHIFT);
		devp->buf.cma_mem_paddr_pages = NULL;
		devp->buf.cma_mem_paddr_start = 0;
		devp->buf.cma_mem_alloced = 0;
	} else {
		pr_frc(0, "%s no buffer exist\n", __func__);
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
	int log = 1;

	if (!devp)
		return -1;

	/*size initial, alloc max support size accordint to vout*/
	//for debug devp->buf.in_hsize = frc_in_hsize;
	//for debug devp->buf.in_vsize = frc_in_vsize;
	devp->buf.in_hsize = devp->out_sts.vout_width;
	devp->buf.in_vsize = devp->out_sts.vout_height;

	/*align size*/
	devp->buf.in_align_hsize = roundup(devp->buf.in_hsize, FRC_HVSIZE_ALIGN_SIZE);
	devp->buf.in_align_vsize = roundup(devp->buf.in_vsize, FRC_HVSIZE_ALIGN_SIZE);
	align_hsize = devp->buf.in_align_hsize;
	align_vsize = devp->buf.in_align_vsize;
	devp->buf.me_hsize =
		roundup(devp->buf.in_align_hsize, FRC_ME_SD_RATE) / FRC_ME_SD_RATE;
	devp->buf.me_vsize =
		roundup(devp->buf.in_align_vsize, FRC_ME_SD_RATE) / FRC_ME_SD_RATE;

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

	/*mc y/c/v info buffer, address 64 bytes align*/
	devp->buf.lossy_mc_y_info_buf_size = LOSSY_MC_INFO_LINE_SIZE * FRC_SLICER_NUM;
	devp->buf.lossy_mc_c_info_buf_size = LOSSY_MC_INFO_LINE_SIZE * FRC_SLICER_NUM;
	devp->buf.lossy_mc_v_info_buf_size = 0/*LOSSY_MC_INFO_LINE_SIZE * FRC_SLICER_NUM*/;

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
		devp->buf.lossy_mc_y_data_buf_size[i] =
			(temp * align_vsize * FRC_COMPRESS_RATE) / 100;
		devp->buf.lossy_mc_c_data_buf_size[i] =
			(temp * align_vsize * FRC_COMPRESS_RATE) / 100;
		devp->buf.lossy_mc_v_data_buf_size[i] =
			0/*(temp * align_vsize * FRC_COMPRESS_RATE) / 100*/;
		devp->buf.total_size += devp->buf.lossy_mc_y_data_buf_size[i] * 2;
		devp->buf.total_size += devp->buf.lossy_mc_v_data_buf_size[i];
	}
	pr_frc(log, "lossy_mc_data_buf_size=%d, line buf:%d all:%d\n",
	       devp->buf.lossy_mc_c_data_buf_size[0], temp,
	       devp->buf.lossy_mc_c_data_buf_size[0] * FRC_TOTAL_BUF_NUM * 2);

	temp = (devp->buf.me_hsize * FRC_ME_BITS_NUM + 511) / 8;
	/*lossy me data buffer*/
	for (i = 0; i < FRC_TOTAL_BUF_NUM; i++) {
		devp->buf.lossy_me_data_buf_size[i] =
			(temp * devp->buf.me_vsize * FRC_COMPRESS_RATE) / 100;
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
		//devp->buf.lossy_mc_v_link_buf_size[i] = temp * 1;
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
	for (i = 0; i < FRC_MEMV_BUF_NUM; i++) {
		devp->buf.norm_memv_buf_size[i] = temp * devp->buf.me_blk_vsize;
		devp->buf.total_size += devp->buf.norm_memv_buf_size[i];
	}
	pr_frc(log, "norm_memv_buf_size=%d ,line buf:%d all:%d\n",
	       devp->buf.norm_memv_buf_size[0], temp,
	       devp->buf.norm_memv_buf_size[0] * FRC_MEMV_BUF_NUM);

	/*norm hmemv buffer*/
	temp = (devp->buf.hme_blk_hsize * 64 + 511) / 8;
	for (i = 0; i < FRC_MEMV_BUF_NUM; i++) {
		devp->buf.norm_hmemv_buf_size[i] = temp * devp->buf.hme_blk_vsize;
		devp->buf.total_size += devp->buf.norm_hmemv_buf_size[i];
	}
	pr_frc(log, "norm_hmemv_buf_size=%d ,line buf:%d all:%d\n",
	       devp->buf.norm_hmemv_buf_size[0], temp,
	       devp->buf.norm_hmemv_buf_size[0] * FRC_MEMV_BUF_NUM);

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
	u32 paddr = 0, base;
	int log = 1;

	/*----------------- buffer alloc------------------*/
	base = devp->buf.cma_mem_paddr_start;
	/*mc y/c/v me info buffer, address 64 bytes align*/
	devp->buf.lossy_mc_y_info_buf_paddr = paddr;
	pr_frc(log, "lossy_mc_y_info_buf_paddr:0x%x", paddr);
	paddr += roundup(devp->buf.lossy_mc_y_info_buf_size, ALIGN_64);
	devp->buf.lossy_mc_c_info_buf_paddr = paddr;
	pr_frc(log, "lossy_mc_c_info_buf_paddr:0x%x", paddr);
	paddr += roundup(devp->buf.lossy_mc_c_info_buf_size, ALIGN_64);
	devp->buf.lossy_mc_v_info_buf_paddr = paddr;
	pr_frc(log, "lossy_mc_v_info_buf_paddr:0x%x", paddr);
	paddr += roundup(devp->buf.lossy_mc_v_info_buf_size, ALIGN_64);
	devp->buf.lossy_me_x_info_buf_paddr = paddr;
	pr_frc(log, "lossy_me_x_info_buf_paddr:0x%x", paddr);
	paddr += roundup(devp->buf.lossy_me_x_info_buf_size, ALIGN_64);

	/*lossy data buffer*/
	paddr = roundup(paddr, ALIGN_4K);
	real_onebuf_size = roundup(devp->buf.lossy_mc_y_data_buf_size[0], ALIGN_4K);
	for (i = 0; i < FRC_TOTAL_BUF_NUM; i++) {
		devp->buf.lossy_mc_y_data_buf_paddr[i] = paddr;
		pr_frc(log, "lossy_mc_y_data_buf_paddr[%d]:0x%x\n", i, base + paddr);
		paddr += real_onebuf_size;
	}

	real_onebuf_size = roundup(devp->buf.lossy_mc_c_data_buf_size[0], ALIGN_4K);
	for (i = 0; i < FRC_TOTAL_BUF_NUM; i++) {
		devp->buf.lossy_mc_c_data_buf_paddr[i] = paddr;
		pr_frc(log, "lossy_mc_c_data_buf_paddr[%d]:0x%x\n", i, base + paddr);
		paddr += real_onebuf_size;
	}

	real_onebuf_size = roundup(devp->buf.lossy_mc_v_data_buf_size[0], ALIGN_4K);
	for (i = 0; i < FRC_TOTAL_BUF_NUM; i++) {
		devp->buf.lossy_mc_v_data_buf_paddr[i] = paddr;
		pr_frc(log, "lossy_mc_v_data_buf_paddr[%d]:0x%x\n", i, base + paddr);
		paddr += real_onebuf_size;
	}

	real_onebuf_size = roundup(devp->buf.lossy_me_data_buf_size[0], ALIGN_4K);
	for (i = 0; i < FRC_TOTAL_BUF_NUM; i++) {
		devp->buf.lossy_me_data_buf_paddr[i] = paddr;
		pr_frc(log, "lossy_me_data_buf_paddr[%d]:0x%x\n", i, base + paddr);
		paddr += real_onebuf_size;
	}

	paddr = roundup(paddr, ALIGN_4K);
	/*link buffer*/
	real_onebuf_size = roundup(devp->buf.lossy_mc_y_link_buf_size[0], ALIGN_64);
	for (i = 0; i < FRC_TOTAL_BUF_NUM; i++) {
		devp->buf.lossy_mc_y_link_buf_paddr[i] = paddr;
		pr_frc(log, "lossy_mc_y_link_buf_paddr[%d]:0x%x\n", i, base + paddr);
		paddr += real_onebuf_size;
	}

	real_onebuf_size = roundup(devp->buf.lossy_mc_c_link_buf_size[0], ALIGN_64);
	for (i = 0; i < FRC_TOTAL_BUF_NUM; i++) {
		devp->buf.lossy_mc_c_link_buf_paddr[i] = paddr;
		pr_frc(log, "lossy_mc_c_link_buf_paddr[%d]:0x%x\n", i, base + paddr);
		paddr += real_onebuf_size;
	}

	real_onebuf_size = roundup(devp->buf.lossy_mc_v_link_buf_size[0], ALIGN_64);
	for (i = 0; i < FRC_TOTAL_BUF_NUM; i++) {
		devp->buf.lossy_mc_v_link_buf_paddr[i] = paddr;
		pr_frc(log, "lossy_mc_v_link_buf_paddr[%d]:0x%x\n", i, base + paddr);
		paddr += real_onebuf_size;
	}

	real_onebuf_size = roundup(devp->buf.lossy_me_link_buf_size[0], ALIGN_64);
	for (i = 0; i < FRC_TOTAL_BUF_NUM; i++) {
		devp->buf.lossy_me_link_buf_paddr[i] = paddr;
		pr_frc(log, "lossy_me_link_buf_paddr[%d]:0x%x\n", i, base + paddr);
		paddr += real_onebuf_size;
	}

	/*norm buffer*/
	paddr = roundup(paddr, ALIGN_4K);
	real_onebuf_size = roundup(devp->buf.norm_hme_data_buf_size[0], ALIGN_4K);
	for (i = 0; i < FRC_TOTAL_BUF_NUM; i++) {
		devp->buf.norm_hme_data_buf_paddr[i] = paddr;
		pr_frc(log, "norm_hme_data_buf_paddr[%d]:0x%x\n", i, base + paddr);
		paddr += real_onebuf_size;
	}

	real_onebuf_size = roundup(devp->buf.norm_memv_buf_size[0], ALIGN_64);
	for (i = 0; i < FRC_MEMV_BUF_NUM; i++) {
		devp->buf.norm_memv_buf_paddr[i] = paddr;
		pr_frc(log, "norm_memv_buf_paddr[%d]:0x%x\n", i, base + paddr);
		paddr += real_onebuf_size;
	}

	real_onebuf_size = roundup(devp->buf.norm_hmemv_buf_size[0], ALIGN_64);
	for (i = 0; i < FRC_MEMV_BUF_NUM; i++) {
		devp->buf.norm_hmemv_buf_paddr[i] = paddr;
		pr_frc(log, "norm_hmemv_buf_paddr[%d]:0x%x\n", i, base + paddr);
		paddr += real_onebuf_size;
	}

	real_onebuf_size = roundup(devp->buf.norm_mevp_out_buf_size[0], ALIGN_64);
	for (i = 0; i < FRC_MEVP_BUF_NUM; i++) {
		devp->buf.norm_mevp_out_buf_paddr[i] = paddr;
		pr_frc(log, "norm_mevp_out_buf_paddr[%d]:0x%x\n", i, base + paddr);
		paddr += real_onebuf_size;
	}

	/*ip logo*/
	paddr = roundup(paddr, ALIGN_4K);
	real_onebuf_size = roundup(devp->buf.norm_iplogo_buf_size[0], ALIGN_4K);
	for (i = 0; i < FRC_TOTAL_BUF_NUM; i++) {
		devp->buf.norm_iplogo_buf_paddr[i] = paddr;
		pr_frc(log, "norm_iplogo_buf_paddr[%d]:0x%x\n", i, base + paddr);
		paddr += real_onebuf_size;
	}
	/*logo irr*/
	paddr = roundup(paddr, ALIGN_4K);
	real_onebuf_size = roundup(devp->buf.norm_logo_irr_buf_size, ALIGN_64);
	devp->buf.norm_logo_irr_buf_paddr = paddr;
	pr_frc(log, "norm_logo_irr_buf_paddr:0x%x\n", base + paddr);
	paddr += real_onebuf_size;

	/*logo ssc*/
	paddr = roundup(paddr, ALIGN_4K);
	real_onebuf_size = roundup(devp->buf.norm_logo_scc_buf_size, ALIGN_64);
	devp->buf.norm_logo_scc_buf_paddr = paddr;
	pr_frc(log, "norm_logo_scc_buf_paddr:0x%x\n", base + paddr);
	paddr += real_onebuf_size;

	/*melogo*/
	paddr = roundup(paddr, ALIGN_4K);
	real_onebuf_size = roundup(devp->buf.norm_melogo_buf_size[0], ALIGN_64);
	for (i = 0; i < FRC_TOTAL_BUF_NUM; i++) {
		devp->buf.norm_melogo_buf_paddr[i] = paddr;
		pr_frc(log, "norm_melogo_buf_paddr[%d]:0x%x\n", i, base + paddr);
		paddr += real_onebuf_size;
	}
	devp->buf.real_total_size = paddr;
	pr_frc(0, "%s base:0x%x real_total_size:0x%x(%d)\n", __func__, base, paddr, paddr);

	return 0;
}

/*
 * config data link buffer
 */
int frc_buf_mapping_tab_init(struct frc_dev_s *devp)
{
	u32 i, j, k = 0;
	phys_addr_t cma_paddr = 0;
	dma_addr_t paddr;
	u8 *cma_vaddr = 0;
	u32 vmap_offset_start = 0, vmap_offset_end;
	u32 *linktab_vaddr = NULL;
	u8 *p = NULL;
	u32 data_buf_addr, data_buf_size;
	u32 link_tab_all_size;
	u32 log = 1;

	cma_paddr = devp->buf.cma_mem_paddr_start;
	link_tab_all_size =
		devp->buf.norm_hme_data_buf_paddr[0] - devp->buf.lossy_mc_y_link_buf_paddr[0];
	pr_frc(log, "paddr start:0x%lx, link start=0x%08x - 0x%08x, size:0x%x\n",
	       (ulong)devp->buf.cma_mem_paddr_start,
	       devp->buf.lossy_mc_y_link_buf_paddr[0], devp->buf.norm_hme_data_buf_paddr[0],
	       link_tab_all_size);

	if (link_tab_all_size == 0) {
		pr_frc(0, "link buf err\n");
		return -1;
	}

	vmap_offset_start = devp->buf.lossy_mc_y_link_buf_paddr[0];
	vmap_offset_end = devp->buf.norm_hme_data_buf_paddr[0];
	cma_vaddr = frc_buf_vmap(cma_paddr, vmap_offset_end);
	pr_frc(0, "map: paddr=0x%lx, vaddr=0x%lx, link tab size=0x%x (%d)\n", (ulong)cma_paddr,
	       (ulong)cma_vaddr, link_tab_all_size, link_tab_all_size);

	memset(cma_vaddr + vmap_offset_start, 0, link_tab_all_size);

	/*split data buffer and fill to link mc buffer: mc y*/
	data_buf_size = roundup(devp->buf.lossy_mc_y_data_buf_size[0], ALIGN_4K);
	if (data_buf_size > 0) {
		pr_frc(log, "lossy_mc_y_data_buf_size:0x%x (%d)\n", data_buf_size, data_buf_size);
		for (i = 0; i < FRC_TOTAL_BUF_NUM; i++) {
			p = cma_vaddr + devp->buf.lossy_mc_y_link_buf_paddr[i];
			paddr = cma_paddr + devp->buf.lossy_mc_y_link_buf_paddr[i];
			linktab_vaddr = (u32 *)p;
			data_buf_addr =
				(cma_paddr + devp->buf.lossy_mc_y_data_buf_paddr[i]) & 0xffffffff;
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
			p = cma_vaddr + devp->buf.lossy_mc_c_link_buf_paddr[i];
			paddr = cma_paddr + devp->buf.lossy_mc_y_link_buf_paddr[i];
			linktab_vaddr = (u32 *)p;
			data_buf_addr =
				(cma_paddr + devp->buf.lossy_mc_c_data_buf_paddr[i]) & 0xffffffff;
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
			p = cma_vaddr + devp->buf.lossy_mc_v_link_buf_paddr[i];
			paddr = cma_paddr + devp->buf.lossy_mc_y_link_buf_paddr[i];
			linktab_vaddr = (u32 *)p;
			data_buf_addr =
				(cma_paddr + devp->buf.lossy_mc_v_data_buf_paddr[i]) & 0xffffffff;
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
			p = cma_vaddr + devp->buf.lossy_me_link_buf_paddr[i];
			paddr = cma_paddr + devp->buf.lossy_mc_y_link_buf_paddr[i];
			linktab_vaddr = (u32 *)p;
			data_buf_addr =
				(cma_paddr + devp->buf.lossy_me_data_buf_paddr[i]) & 0xffffffff;
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

	return 0;
}

/*
 * config buffer address to every hw buffer register
 *
 */
int frc_buf_config(struct frc_dev_s *devp)
{
	u32 i = 0;
	u32 base;
	u32 log = 1;

	if (!devp || !devp->buf.cma_mem_paddr_start || !devp->buf.cma_mem_alloced) {
		pr_frc(0, "%s fail <cma alloced:%d>\n", __func__, devp->buf.cma_mem_alloced);
		return -1;
	}

	base = devp->buf.cma_mem_paddr_start;
	pr_frc(log, "%s cma base:0x%x\n", __func__, base);
	/*mc info buffer*/
	WRITE_FRC_REG(FRC_REG_MC_YINFO_BADDR, base + devp->buf.lossy_mc_y_info_buf_paddr);
	WRITE_FRC_REG(FRC_REG_MC_CINFO_BADDR, base + devp->buf.lossy_mc_c_info_buf_paddr);
	WRITE_FRC_REG(FRC_REG_MC_VINFO_BADDR, base + devp->buf.lossy_mc_v_info_buf_paddr);
	WRITE_FRC_REG(FRC_REG_ME_XINFO_BADDR, base + devp->buf.lossy_me_x_info_buf_paddr);

	/*lossy mc y,c,v data buffer, data buffer needn't config*/
	//for (i = FRC_REG_MC_YBUF_ADDRX_0; i <= FRC_REG_MC_YBUF_ADDRX_15; i++)
	//	WRITE_FRC_REG(i, base + devp->buf.lossy_mc_y_data_buf_paddr[i]);
	//for (i = FRC_REG_MC_CBUF_ADDRX_0; i <= FRC_REG_MC_CBUF_ADDRX_15; i++)
	//	WRITE_FRC_REG(i, base + devp->buf.lossy_mc_c_data_buf_paddr[i]);
	//for (i = FRC_REG_MC_VBUF_ADDRX_0; i <= FRC_REG_MC_VBUF_ADDRX_15; i++)
	//	WRITE_FRC_REG(i, base + devp->buf.lossy_mc_v_data_buf_paddr[i]);
	/*lossy me data buffer, data buffer needn't config*/
	//for (i = FRC_REG_MC_VBUF_ADDRX_0; i <= FRC_REG_MC_VBUF_ADDRX_15; i++)
	//	WRITE_FRC_REG(i, base + devp->buf.lossy_me_data_buf_paddr[i]);

	/*lossy mc link buffer*/
	for (i = FRC_REG_MC_YBUF_ADDRX_0; i <= FRC_REG_MC_YBUF_ADDRX_15; i++) {
		WRITE_FRC_REG(i,
			base + devp->buf.lossy_mc_y_link_buf_paddr[i - FRC_REG_MC_YBUF_ADDRX_0]);
	}
	for (i = FRC_REG_MC_CBUF_ADDRX_0; i <= FRC_REG_MC_CBUF_ADDRX_15; i++)
		WRITE_FRC_REG(i,
			base + devp->buf.lossy_mc_c_link_buf_paddr[i - FRC_REG_MC_CBUF_ADDRX_0]);

	for (i = FRC_REG_MC_VBUF_ADDRX_0; i <= FRC_REG_MC_VBUF_ADDRX_15; i++)
		WRITE_FRC_REG(i,
			base + devp->buf.lossy_mc_v_link_buf_paddr[i - FRC_REG_MC_VBUF_ADDRX_0]);

	/*lossy me link buffer*/
	for (i = FRC_REG_ME_BUF_ADDRX_0; i <= FRC_REG_ME_BUF_ADDRX_15; i++)
		WRITE_FRC_REG(i,
			base + devp->buf.lossy_me_link_buf_paddr[i - FRC_REG_ME_BUF_ADDRX_0]);

	/*norm hme data buffer*/
	for (i = FRC_REG_HME_BUF_ADDRX_0; i <= FRC_REG_HME_BUF_ADDRX_15; i++)
		WRITE_FRC_REG(i,
			base + devp->buf.norm_hme_data_buf_paddr[i - FRC_REG_HME_BUF_ADDRX_0]);

	/*norm memv buffer*/
	for (i = FRC_REG_ME_NC_UNI_MV_ADDRX_0; i <= FRC_REG_ME_PC_PHS_MV_ADDR; i++)
		WRITE_FRC_REG(i,
			base + devp->buf.norm_memv_buf_paddr[i - FRC_REG_ME_NC_UNI_MV_ADDRX_0]);

	/*norm hmemv buffer*/
	for (i = FRC_REG_HME_NC_UNI_MV_ADDRX_0; i <= FRC_REG_HME_PC_PHS_MV_ADDR; i++)
		WRITE_FRC_REG(i,
			base + devp->buf.norm_hmemv_buf_paddr[i - FRC_REG_HME_NC_UNI_MV_ADDRX_0]);

	/*norm mevp buffer*/
	for (i = FRC_REG_VP_MC_MV_ADDRX_0; i <= FRC_REG_VP_MC_MV_ADDRX_1; i++)
		WRITE_FRC_REG(i,
			base + devp->buf.norm_mevp_out_buf_paddr[i - FRC_REG_VP_MC_MV_ADDRX_0]);

	/*norm iplogo buffer*/
	for (i = FRC_REG_IP_LOGO_ADDRX_0; i <= FRC_REG_IP_LOGO_ADDRX_15; i++)
		WRITE_FRC_REG(i,
			base + devp->buf.norm_iplogo_buf_paddr[i - FRC_REG_IP_LOGO_ADDRX_0]);

	/*norm logo irr buffer*/
	WRITE_FRC_REG(FRC_REG_LOGO_IIR_BUF_ADDR, base + devp->buf.norm_logo_irr_buf_paddr);

	/*norm logo scc buffer*/
	WRITE_FRC_REG(FRC_REG_LOGO_SCC_BUF_ADDR, base + devp->buf.norm_logo_scc_buf_paddr);

	/*norm iplogo buffer*/
	for (i = FRC_REG_ME_LOGO_ADDRX_0; i <= FRC_REG_ME_LOGO_ADDRX_15; i++)
		WRITE_FRC_REG(i,
			base + devp->buf.norm_melogo_buf_paddr[i - FRC_REG_ME_LOGO_ADDRX_0]);

	frc_buf_mapping_tab_init(devp);

	pr_frc(0, "%s success!\n", __func__);
	return 0;
}

