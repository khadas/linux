// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#define DEBUG

#define pr_fmt(fmt) "hifi4dsp: " fmt

#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/sysfs.h>
#include <linux/string.h>
#include <linux/io.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/device.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/reset.h>
#include <linux/dma-mapping.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/of_reserved_mem.h>
#include <linux/cma.h>
#include <linux/dma-contiguous.h>
#include <linux/vmalloc.h>
#include <linux/clk.h>
#include <asm/cacheflush.h>
#include <linux/pm_runtime.h>
#include <linux/pm_domain.h>

#include "hifi4dsp_api.h"
#include "hifi4dsp_priv.h"
#include "hifi4dsp_firmware.h"
#include "hifi4dsp_dsp.h"

#include "dsp_top.h"

struct reg_iomem_t g_regbases;
static unsigned int boot_sram_addr, boot_sram_size;
static unsigned int bootlocation;
static struct mutex hifi4dsp_flock;

static struct reserved_mem hifi4_rmem;

struct hifi4dsp_priv *hifi4dsp_p[HIFI4DSP_MAX_CNT];

#define MASK_BF(x, mask, shift)			((((x) & (mask)) << (shift)))

static int hifi4dsp_driver_load_fw(struct hifi4dsp_dsp *dsp);
static int hifi4dsp_driver_load_2fw(struct hifi4dsp_dsp *dsp);
static int hifi4dsp_driver_reset(struct hifi4dsp_dsp *dsp);
static int hifi4dsp_driver_dsp_start(struct hifi4dsp_dsp *dsp);
static int hifi4dsp_driver_dsp_stop(struct hifi4dsp_dsp *dsp);
static int hifi4dsp_driver_dsp_sleep(struct hifi4dsp_dsp *dsp);
static int hifi4dsp_driver_dsp_wake(struct hifi4dsp_dsp *dsp);

static int hifi4dsp_miscdev_open(struct inode *inode, struct file *fp)
{
	int minor = iminor(inode);
	int major = imajor(inode);
	struct hifi4dsp_priv *priv;
	struct miscdevice *c;
	struct hifi4dsp_miscdev_t *pmscdev_t;

	c = fp->private_data;
	pr_debug("%s,%s,major=%d,minor=%d\n", __func__,
		 c->name,
		 major,
		 minor);

	if (strcmp(c->name, "hifi4dsp0") == 0)
		priv = hifi4dsp_p[0];
	else if (strcmp(c->name, "hifi4dsp1") == 0)
		priv = hifi4dsp_p[1];
	else
		priv = NULL;

	pmscdev_t = container_of(c, struct hifi4dsp_miscdev_t, dsp_miscdev);
	if (!pmscdev_t) {
		pr_err("hifi4dsp_miscdev_t == NULL\n");
		return -1;
	}
	pmscdev_t->priv = priv;
	if (!pmscdev_t->priv) {
		pr_err("hifi4dsp_miscdev_t pointer *pmscdev_t==NULL\n");
		return -1;
	}

	fp->private_data = priv;

	pr_debug("%s,%s,major=%d,minor=%d\n", __func__,
		 priv->dev->kobj.name,
		 major,
		 minor);
	return 0;
}

static int hifi4dsp_miscdev_release(struct inode *inode, struct file *fp)
{
	return 0;
}

static long hifi4dsp_miscdev_unlocked_ioctl(struct file *fp, unsigned int cmd,
					    unsigned long arg)
{
	int ret = 0;
	struct hifi4dsp_priv *priv;
	struct hifi4dsp_dsp *dsp;
	struct hifi4dsp_info_t *usrinfo;
	struct hifi4_shm_info_t shminfo;
	void __user *argp;

	pr_debug("%s\n", __func__);
	if (!fp->private_data) {
		pr_err("%s error:fp->private_data is null", __func__);
		return -1;
	}

	mutex_lock(&hifi4dsp_flock);
	argp = (void __user *)arg;
	priv = (struct hifi4dsp_priv *)fp->private_data;
	if (!priv) {
		pr_err("%s error: hifi4dsp_priv is null", __func__);
		ret = -1;
		goto err;
	}
	dsp = priv->dsp;
	if (!dsp) {
		pr_err("%s hifi4dsp_dsp is null:\n", __func__);
		ret = -1;
		goto err;
	}
	if (!priv->dsp->dsp_fw) {
		pr_err("%s hifi4dsp_firmware is null:\n", __func__);
		ret = -1;
		goto err;
	}
	if (!priv->dsp->fw) {
		pr_err("%s firmware is null:\n", __func__);
		ret = -1;
		goto err;
	}
	pr_debug("%s %s\n", __func__, priv->dev->kobj.name);
	usrinfo = kmalloc(sizeof(*usrinfo), GFP_KERNEL);
	if (!usrinfo) {
		ret = -1;
		goto err;
	}

	switch (cmd) {
	case HIFI4DSP_TEST:
		pr_debug("%s HIFI4DSP_TEST\n", __func__);
	break;
	case HIFI4DSP_LOAD:
		pr_debug("%s HIFI4DSP_LOAD\n", __func__);
		ret = copy_from_user(usrinfo, argp,
				     sizeof(struct hifi4dsp_info_t));
		pr_debug("\ninfo->fw1_name : %s\n", usrinfo->fw1_name);
		pr_debug("\ninfo->fw2_name : %s\n", usrinfo->fw2_name);
		priv->dsp->info = usrinfo;
		hifi4dsp_driver_load_fw(priv->dsp);
	break;
	case HIFI4DSP_2LOAD:
		pr_debug("%s HIFI4DSP_2LOAD\n", __func__);
		ret = copy_from_user(usrinfo, argp,
				     sizeof(struct hifi4dsp_info_t));
		priv->dsp->info = usrinfo;
		hifi4dsp_driver_load_2fw(priv->dsp);
	break;
	case HIFI4DSP_RESET:
		pr_debug("%s HIFI4DSP_RESET\n", __func__);
		ret = copy_from_user(usrinfo, argp,
				     sizeof(struct hifi4dsp_info_t));
		priv->dsp->info = usrinfo;
		hifi4dsp_driver_reset(priv->dsp);
	break;
	case HIFI4DSP_START:
		pr_debug("%s HIFI4DSP_START\n", __func__);
		ret = copy_from_user(usrinfo, argp,
				     sizeof(struct hifi4dsp_info_t));
		priv->dsp->info = usrinfo;
		hifi4dsp_driver_dsp_start(priv->dsp);
	break;
	case HIFI4DSP_STOP:
		pr_debug("%s HIFI4DSP_STOP\n", __func__);
		ret = copy_from_user(usrinfo, argp,
				     sizeof(struct hifi4dsp_info_t));
		priv->dsp->info = usrinfo;
		hifi4dsp_driver_dsp_stop(priv->dsp);
	break;
	case HIFI4DSP_SLEEP:
		pr_debug("%s HIFI4DSP_SLEEP\n", __func__);
		ret = copy_from_user(usrinfo, argp,
				     sizeof(struct hifi4dsp_info_t));
		priv->dsp->info = usrinfo;
		hifi4dsp_driver_dsp_sleep(priv->dsp);
	break;
	case HIFI4DSP_WAKE:
		pr_debug("%s HIFI4DSP_WAKE\n", __func__);
		ret = copy_from_user(usrinfo, argp,
				     sizeof(struct hifi4dsp_info_t));
		priv->dsp->info = usrinfo;
		hifi4dsp_driver_dsp_wake(priv->dsp);
	break;
	case HIFI4DSP_GET_INFO:
		pr_debug("%s HIFI4DSP_GET_INFO\n", __func__);
		ret = copy_from_user(usrinfo, argp,
				     sizeof(struct hifi4dsp_info_t));
		pr_debug("%s HIFI4DSP_GET_INFO %s\n", __func__,
			 usrinfo->fw_name);
		strcpy(usrinfo->fw_name, "1234abcdef");
		usrinfo->phy_addr = priv->pdata->fw_paddr;
		usrinfo->size = priv->pdata->fw_max_size;
		ret = copy_to_user(argp, usrinfo,
				   sizeof(struct hifi4dsp_info_t));
		pr_debug("%s HIFI4DSP_GET_INFO %s\n", __func__,
			 usrinfo->fw_name);
	break;
	case HIFI4DSP_SHM_CLEAN:
		ret = copy_from_user(&shminfo, argp, sizeof(shminfo));
		pr_debug("%s clean cache, addr:%lu, size:%zu\n",
			 __func__, shminfo.addr, shminfo.size);
		dma_sync_single_for_device
								(priv->dev,
								 shminfo.addr,
								 shminfo.size,
								 DMA_TO_DEVICE);
	break;
	case HIFI4DSP_SHM_INV:
		ret = copy_from_user(&shminfo, argp, sizeof(shminfo));
		pr_debug("%s invalidate cache, addr:%lu, size:%zu\n",
			 __func__, shminfo.addr, shminfo.size);
		dma_sync_single_for_device(priv->dev,
					   shminfo.addr,
					   shminfo.size,
					   DMA_FROM_DEVICE);
	break;
	default:
		pr_err("%s ioctl CMD error\n", __func__);
	break;
	}
	kfree(usrinfo);
	priv->dsp->info = NULL;
err:
	mutex_unlock(&hifi4dsp_flock);
	return ret;
}

#ifdef CONFIG_COMPAT
static long hifi4dsp_miscdev_compat_ioctl(struct file *filp,
					  unsigned int cmd, unsigned long args)
{
	long ret;

	args = (unsigned long)compat_ptr(args);
	ret = hifi4dsp_miscdev_unlocked_ioctl(filp, cmd, args);

	return ret;
}
#endif

static int hifi4dsp_miscdev_mmap(struct file *fp, struct vm_area_struct *vma)
{
	int ret = 0;
	unsigned long phys_page_addr = 0;
	unsigned long size = 0;
	struct hifi4dsp_priv *priv = NULL;

	if (!vma) {
		pr_err("input error: vma is NULL\n");
		return -1;
	}

	pr_debug("%s\n", __func__);
	if (!fp->private_data) {
		pr_err("%s error:fp->private_data is null", __func__);
		return -1;
	}

	priv = (struct hifi4dsp_priv *)fp->private_data;

	phys_page_addr = (u64)priv->pdata->fw_paddr >> PAGE_SHIFT;
	size = ((unsigned long)vma->vm_end - (unsigned long)vma->vm_start);
	pr_err("vma=0x%pK.\n", vma);
	pr_err("size=%ld, vma->vm_start=%ld, end=%ld.\n",
	       ((unsigned long)vma->vm_end - (unsigned long)vma->vm_start),
	       (unsigned long)vma->vm_start, (unsigned long)vma->vm_end);
	pr_err("phys_page_addr=%ld.\n", (unsigned long)phys_page_addr);

	vma->vm_page_prot = PAGE_SHARED;
	vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);

	if (size > priv->pdata->fw_max_size)
		size = priv->pdata->fw_max_size;

	ret = remap_pfn_range(vma,
			      vma->vm_start,
			      phys_page_addr, size, vma->vm_page_prot);
	if (ret != 0) {
		pr_err("remap_pfn_range ret=%d\n", ret);
		return -1;
	}

	return ret;
}

static int hifi4dsp_driver_load_fw(struct hifi4dsp_dsp *dsp)
{
	int err = 0;
	struct hifi4dsp_firmware *new_dsp_fw;
	struct hifi4dsp_info_t *info = dsp->info;

	if (strlen(info->fw_name) == 0)
		return -1;

	pr_debug("hifi4dsp_info_t, name=%s, paddr=0x%llx\n",
		 info->fw_name,
		 (unsigned long long)info->phy_addr);

	new_dsp_fw = hifi4dsp_fw_register(dsp, info->fw_name);
	if (!new_dsp_fw) {
		pr_err("register firmware:%s error\n", info->fw_name);
		return -1;
	}
	dsp->dsp_fw = new_dsp_fw;  /*set newest fw as def fw of dsp*/
	strcpy(new_dsp_fw->name, info->fw_name);
	if (info->phy_addr != 0) { /*to be improved*/
		//info->phy_addr may !=0, but illegal
		new_dsp_fw->paddr = info->phy_addr;
		/*TODO*/
		/*new_dsp_fw->buf = phys_to_virt(new_dsp_fw->paddr);*/
		new_dsp_fw->buf = dsp->pdata->fw_buf;
	} else {
		new_dsp_fw->paddr = dsp->pdata->fw_paddr;
		new_dsp_fw->buf = dsp->pdata->fw_buf;
	}
	pr_debug("new hifi4dsp_firmware, name=%s, paddr=0x%llx, buf=0x%p\n",
		 new_dsp_fw->name,
		 (unsigned long long)new_dsp_fw->paddr,
		 new_dsp_fw->buf);

	hifi4dsp_fw_load(new_dsp_fw);

	return err;
}

static int hifi4dsp_driver_load_2fw(struct hifi4dsp_dsp *dsp)
{
	int err = 0;
	struct hifi4dsp_firmware *new_dsp_sram_fw;
	struct hifi4dsp_firmware *new_dsp_ddr_fw;
	struct hifi4dsp_info_t *info = dsp->info;

	if (strlen(info->fw1_name) == 0)
		return -1;
	/*load dspboot_sdram.bin to ddr*/
	pr_debug("info->fw1_name : %s\n", info->fw1_name);
	new_dsp_ddr_fw = hifi4dsp_fw_register(dsp, info->fw1_name);
	if (!new_dsp_ddr_fw) {
		pr_err("register firmware:%s error\n", info->fw1_name);
		return -1;
	}
	dsp->dsp_fw = new_dsp_ddr_fw;  /*set newest fw as def fw of dsp*/
	strcpy(new_dsp_ddr_fw->name, info->fw1_name);
	new_dsp_ddr_fw->paddr = dsp->pdata->fw_paddr;
	new_dsp_ddr_fw->buf = dsp->pdata->fw_buf;
	pr_debug("new_dsp_ddr_fw, name=%s, paddr=0x%llx, virtual addr:0x%lx\n",
		 new_dsp_ddr_fw->name,
		 (unsigned long long)new_dsp_ddr_fw->paddr,
		 (unsigned long)new_dsp_ddr_fw->buf);
	hifi4dsp_fw_load(new_dsp_ddr_fw);

	/*load dspboot_sram.bin to sram*/
	if (strlen(info->fw2_name) == 0)
		return -1;
	pr_debug("info->fw2_name : %s\n", info->fw2_name);
	new_dsp_sram_fw = hifi4dsp_fw_register(dsp, info->fw2_name);
	if (!new_dsp_sram_fw) {
		pr_err("register firmware:%s error\n", info->fw2_name);
		return -1;
	}
	dsp->dsp_fw = new_dsp_sram_fw;
	strcpy(new_dsp_sram_fw->name, info->fw2_name);
	new_dsp_sram_fw->paddr = boot_sram_addr;
	new_dsp_sram_fw->buf = g_regbases.sram_base;
	hifi4dsp_fw_sram_load(new_dsp_sram_fw);

	return err;
}

static int hifi4dsp_driver_reset(struct hifi4dsp_dsp *dsp)
{
	struct	hifi4dsp_info_t *info;

	pr_debug("%s\n", __func__);
	if (!dsp->info)
		info = (struct	hifi4dsp_info_t *)dsp->info;

	dsp->info = NULL;
	return 0;
}

static int hifi4dsp_driver_dsp_boot(struct hifi4dsp_dsp *dsp)
{
	struct  hifi4dsp_info_t *info;

	info = (struct  hifi4dsp_info_t *)dsp->info;
	pr_debug("%s\n", __func__);

	dsp->info = NULL;
	return 0;
}

static int hifi4dsp_driver_dsp_start(struct hifi4dsp_dsp *dsp)
{
	struct  hifi4dsp_info_t *info;

	pr_debug("%s\n", __func__);
	info = (struct  hifi4dsp_info_t *)dsp->info;
	pr_debug("dsp_id: %d\n", dsp->id);
	pr_debug("dsp_freqence: %d Hz\n", dsp->freq);
	pr_debug("dsp_start_addr: 0x%llx\n",
		 (unsigned long long)dsp->dsp_fw->paddr);

	if (!dsp->dsp_clk) {
		pr_err("dsp_clk=NULL\n");
		return -EINVAL;
	}

	clk_set_rate(dsp->dsp_clk, dsp->freq);
	clk_prepare_enable(dsp->dsp_clk);

	if (bootlocation == 1)
		soc_dsp_bootup(dsp->id, dsp->dsp_fw->paddr, dsp->freq);
	else if (bootlocation == 2)
		soc_dsp_bootup(dsp->id, boot_sram_addr, dsp->freq);

	dsp->info = NULL;

	return 0;
}

static int hifi4dsp_driver_dsp_clk_off(struct hifi4dsp_dsp *dsp)
{
	if (!dsp->dsp_clk) {
		pr_err("dsp_clk=NULL\n");
		return  -EINVAL;
	}
	clk_disable_unprepare(dsp->dsp_clk);
	return 0;
}

static int hifi4dsp_driver_dsp_stop(struct hifi4dsp_dsp *dsp)
{
	struct  hifi4dsp_info_t *info;

	info = (struct  hifi4dsp_info_t *)dsp->info;
	pr_debug("%s\n", __func__);

	soc_dsp_poweroff(info->id);
	hifi4dsp_driver_dsp_clk_off(dsp);

	dsp->info = NULL;
	return 0;
}

static int hifi4dsp_driver_dsp_sleep(struct hifi4dsp_dsp *dsp)
{
	struct  hifi4dsp_info_t *info;

	info = (struct  hifi4dsp_info_t *)dsp->info;
	pr_debug("%s\n", __func__);

	dsp->info = NULL;
	return 0;
}

static int hifi4dsp_driver_dsp_wake(struct hifi4dsp_dsp *dsp)
{
	struct  hifi4dsp_info_t *info;

	info = (struct  hifi4dsp_info_t *)dsp->info;
	pr_debug("%s\n", __func__);

	dsp->info = NULL;
	return 0;
}

static void hifi4dsp_driver_dsp_free(struct hifi4dsp_dsp *dsp)
{
	pr_debug("%s\n", __func__);

	hifi4dsp_fw_free_all(dsp);
	//kfree(NULL) is safe and check is probably not required

	kfree(dsp->dsp_fw);
	dsp->dsp_fw = NULL;

	kfree(dsp->fw);
	dsp->fw = NULL;

	kfree(dsp->ops);
	dsp->ops = NULL;

	kfree(dsp->pdata);
	dsp->pdata = NULL;

	kfree(dsp);
}

/*transfer param from pdata to dsp*/
static int hifi4dsp_driver_resource_map(struct hifi4dsp_dsp *dsp,
					struct hifi4dsp_pdata *pdata)
{
	int ret = 0;

	if (!pdata) {
		pr_err("%s error\n", __func__);
		ret = -1;
	}
	return ret;
}

static int hifi4dsp_driver_init(struct hifi4dsp_dsp *dsp,
				struct hifi4dsp_pdata *pdata)
{
	struct device *dev;
	int ret = -ENODEV;

	dev = dsp->dev;
	pr_debug("%s\n", __func__);
	ret = hifi4dsp_driver_resource_map(dsp, pdata);
	if (ret < 0) {
		dev_err(dev, "failed to map resources\n");
		return ret;
	}
	return 0;
}

static void hifi4dsp_driver_free(struct hifi4dsp_priv *priv)
{
	pr_debug("%s\n", __func__);
	if (priv->dsp) {
		hifi4dsp_driver_dsp_free(priv->dsp);
		priv->dsp = NULL;
	}
	if (priv->dsp_fw) {
		priv->dsp_fw = NULL;
		return;
	}
}

static int hifi4dsp_load_and_parse_fw(struct hifi4dsp_firmware *dsp_fw,
				      void *pinfo)
{
	struct  hifi4dsp_info_t *info;

	info = (struct hifi4dsp_info_t *)pinfo;
	pr_debug("%s\n", __func__);
	hifi4dsp_driver_load_fw(dsp_fw->dsp);
	return 0;
}

struct hifi4dsp_ops hifi4dsp_driver_dsp_ops = {
	.boot = hifi4dsp_driver_dsp_boot,
	.reset = hifi4dsp_driver_reset,
	.sleep = hifi4dsp_driver_dsp_sleep,
	.wake = hifi4dsp_driver_dsp_wake,

	.write = hifi4dsp_smem_write,
	.read = hifi4dsp_smem_read,
	.write64 = hifi4dsp_smem_write64,
	.read64 = hifi4dsp_smem_read64,
	.ram_read = hifi4dsp_memcpy_fromio_32,
	.ram_write = hifi4dsp_memcpy_toio_32,

	.init = hifi4dsp_driver_init,
	.free = hifi4dsp_driver_dsp_free,
	.parse_fw = hifi4dsp_load_and_parse_fw,
};

static struct hifi4dsp_dsp_device hifi4dsp_dev = {
		.ops = &hifi4dsp_driver_dsp_ops,
};

static struct hifi4dsp_priv *hifi4dsp_privdata(void)
{
	return hifi4dsp_p[0];
}

static int hifi4dsp_platform_remove(struct platform_device *pdev)
{
	struct hifi4dsp_priv *priv;
	int id = 0, dsp_cnt = 0;
	int ret = 0;

	ret = of_property_read_u32(pdev->dev.of_node, "dsp-cnt", &dsp_cnt);
	if (ret < 0) {
		dev_err(&pdev->dev, "Can't retrieve dsp-cnt\n");
		ret = -EINVAL;
		goto dsp_cnt_error;
	}

	hifi4_syslog_reomve();

	priv = hifi4dsp_privdata();
	for (id = 0; id < dsp_cnt; id++) {
		if (!priv)
			continue;
		hifi4dsp_driver_free(priv);
		if (priv->dev)
			device_destroy(priv->class, priv->dev->devt);
		priv += 1;
	}
	kfree(priv);
	for (id = 0; id < dsp_cnt; id++)
		hifi4dsp_p[id] = NULL;

	return 0;
dsp_cnt_error:
	return ret;
}

static const struct file_operations hifi4dsp_miscdev_fops = {
	.owner = THIS_MODULE,
	.open = hifi4dsp_miscdev_open,
	.read = NULL,
	.write = NULL,
	.release = hifi4dsp_miscdev_release,
	.unlocked_ioctl = hifi4dsp_miscdev_unlocked_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = hifi4dsp_miscdev_compat_ioctl,
#endif
	.mmap = hifi4dsp_miscdev_mmap,
};

static struct miscdevice hifi4dsp_miscdev[] = {
	{
		.minor	= MISC_DYNAMIC_MINOR,
		.name	= "hifi4dsp0",
		.fops	= &hifi4dsp_miscdev_fops,
	},
	{
		.minor	= MISC_DYNAMIC_MINOR,
		.name	= "hifi4dsp1",
		.fops	= &hifi4dsp_miscdev_fops,
	}
};

static void *mm_vmap(phys_addr_t phys, unsigned long size)
{
	u32 offset, npages;
	struct page **pages = NULL;
	pgprot_t pgprot = PAGE_KERNEL;
	void *vaddr;
	int i;

	offset = offset_in_page(phys);
	npages = DIV_ROUND_UP(size + offset, PAGE_SIZE);

	pages = vmalloc(sizeof(struct page *) * npages);
	if (!pages)
		return NULL;
	for (i = 0; i < npages; i++) {
		pages[i] = phys_to_page(phys);
		phys += PAGE_SIZE;
	}

	vaddr = vmap(pages, npages, VM_MAP, pgprot);
	if (!vaddr) {
		pr_err("vmaped fail, size: %d\n",
		       npages << PAGE_SHIFT);
		vfree(pages);
		return NULL;
	}
	vfree(pages);
	pr_debug("[HIGH-MEM-MAP] pa(%lx) to va(%p), size: %d\n",
		 (unsigned long)phys, vaddr, npages << PAGE_SHIFT);

	return vaddr;
}

/*of read clk_gate, clk*/
static inline int of_read_dsp_irq(struct platform_device *pdev, int dsp_id)
{
	int irq = -1;

	if (dsp_id == 0)
		irq = of_irq_get_byname(pdev->dev.of_node, "irq_frm_dspa");
	else if (dsp_id == 1)
		irq = of_irq_get_byname(pdev->dev.of_node, "irq_frm_dspb");

	pr_debug("%s %s irq=%d\n", __func__,
		 (irq < 0) ? "error" : "successful", irq);

	return irq;
}

/*of read clk_gate, clk*/
static inline struct clk *of_read_dsp_clk(struct platform_device *pdev,
					  int dsp_id)
{
	struct clk *p_clk = NULL;
	char clk_name[20];

	if (dsp_id == 0) {
		strcpy(clk_name, "dspa_clk");
		p_clk = devm_clk_get(&pdev->dev, clk_name);
	} else if (dsp_id == 1) {
		strcpy(clk_name, "dspb_clk");
		p_clk = devm_clk_get(&pdev->dev, clk_name);
	}
	if (!p_clk)
		pr_err("%s %s error\n", __func__, clk_name);

	return p_clk;
}

static int hifi4dsp_runtime_resume(struct device *dev)
{
	return 0;
}

static int hifi4dsp_runtime_suspend(struct device *dev)
{
	return 0;
}

void get_dsp_baseaddr(struct platform_device *pdev)
{
	struct resource *res;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "failed to get dspa base address\n");
		return;
	}
	g_regbases.dspa_addr = devm_ioremap_resource(&pdev->dev, res);
	g_regbases.rega_size = resource_size(res);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!res) {
		dev_err(&pdev->dev, "failed to get dspb base address\n");
		return;
	}
	g_regbases.dspb_addr = devm_ioremap_resource(&pdev->dev, res);
	g_regbases.regb_size = resource_size(res);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	if (!res) {
		dev_err(&pdev->dev, "failed to get dsp hiu address\n");
		return;
	}
	//g_regbases.hiu_addr = devm_ioremap_resource(&pdev->dev, res);
	g_regbases.hiu_addr = ioremap_nocache(res->start, resource_size(res));
}

static struct hifi4dsp_pdata dsp_pdatas[] = {/*ARRAY_SIZE(dsp_pdatas)*/
	{
		.name = "hifi4dsp0",
		.clk_freq = 400 * 1000 * 1000,
	},
	{
		.name = "hifi4dsp1",
		.clk_freq = 400 * 1000 * 1000,
	},
};

static int hifi4dsp_platform_probe(struct platform_device *pdev)
{
	int ret = 0;
	int i = 0, id = 0;
	unsigned int dsp_cnt = 0;
	unsigned int dspaoffset, dspboffset;
	struct hifi4dsp_priv *priv;
	struct hifi4dsp_dsp *dsp;
	struct hifi4dsp_pdata *pdata;
	struct hifi4dsp_miscdev_t *p_dsp_miscdev;
	struct miscdevice *pmscdev;

	phys_addr_t hifi4base;
	int hifi4size;
	void *fw_addr = NULL;

	struct hifi4dsp_firmware *dsp_firmware;
	struct firmware *fw = NULL;

	struct device_node *np, *mem_node;
	struct clk *dsp_clk = NULL;

	struct hifi4dsp_info_t *hifi_info = NULL;
	struct page *cma_pages = NULL;
	struct reserved_mem *rmem = NULL;

	np = pdev->dev.of_node;

	/*dsp boot offset */
	ret = of_property_read_u32(np, "dsp-cnt", &dsp_cnt);
	if (ret < 0 || dsp_cnt <= 0) {
		dev_err(&pdev->dev, "Can't retrieve dsp-cnt\n");
		goto err1;
	}
	pr_debug("%s of read dsp-cnt=%d\n", __func__, dsp_cnt);

	ret = of_property_read_u32(np, "dspaoffset", &dspaoffset);
	if (ret < 0) {
		dev_err(&pdev->dev, "Can't retrieve dspaoffset\n");
		goto err1;
	}
	pr_debug("%s of read dspaoffset=0x%08x\n", __func__, dspaoffset);

	ret = of_property_read_u32(np, "dspboffset", &dspboffset);
	if (ret < 0) {
		dev_err(&pdev->dev, "Can't retrieve dspboffset\n");
		goto err1;
	}
	pr_debug("%s of read dspboffset=0x%08x\n", __func__, dspboffset);

	/*boot from DDR or SRAM or ...*/
	ret = of_property_read_u32(np, "bootlocation", &bootlocation);
	if (ret < 0) {
		dev_err(&pdev->dev, "Can't retrieve bootlocation\n");
		goto err1;
	}
	pr_debug("%s of read dsp bootlocation=%x\n", __func__, bootlocation);
	if (bootlocation == 1) {
		pr_info("Dsp boot from DDR !\n");
	} else if (bootlocation == 2) {
		ret = of_property_read_u32(np, "boot_sram_addr",
					   &boot_sram_addr);
		if (ret < 0) {
			dev_err(&pdev->dev, "Can't retrieve boot_sram_addr\n");
			goto err1;
		}
		ret = of_property_read_u32(np, "boot_sram_size",
					   &boot_sram_size);
		if (ret < 0) {
			dev_err(&pdev->dev, "Can't retrieve boot_sram_size\n");
			goto err1;
		}
		pr_info("Dsp boot from SRAM !boot addr : 0x%08x, allocate size: 0x%08x\n",
			boot_sram_addr, boot_sram_size);
		g_regbases.sram_base = ioremap_nocache(boot_sram_addr,
						       boot_sram_size);
	}

	/*init hifi4dsp_dsp*/
	dsp = kcalloc(dsp_cnt, sizeof(*dsp), GFP_KERNEL);
	if (!dsp)
		goto err2;

	/*init hifi4dsp_info_t*/
	hifi_info = kzalloc(sizeof(*hifi_info), GFP_KERNEL);
	if (!hifi_info)
		goto hifi_info_malloc_error;

	/*init miscdev_t, miscdevice*/
	p_dsp_miscdev = kcalloc(dsp_cnt, sizeof(struct hifi4dsp_miscdev_t),
				GFP_KERNEL);
	if (!p_dsp_miscdev) {
		HIFI4DSP_PRNT("kzalloc for p_dsp_miscdev error\n");
		goto miscdev_malloc_error;
	}
	if (!&p_dsp_miscdev->dsp_miscdev)
		pr_info("register dsp _p_dsp_miscdev->dsp_miscdev alloc error\n");
	else
		pr_info("register dsp _p_dsp_miscdev->dsp_miscdev alloc success");

	/*init hifi4dsp_priv*/
	priv = kcalloc(dsp_cnt, sizeof(struct hifi4dsp_priv),
		       GFP_KERNEL);

	if (!priv) {
		HIFI4DSP_PRNT("kzalloc for hifi4dsp_priv error\n");
		goto priv_malloc_error;
	}

	/*init hifi4dsp_pdata*/
	pdata = kcalloc(dsp_cnt, sizeof(struct hifi4dsp_pdata), GFP_KERNEL);
	if (!pdata) {
		HIFI4DSP_PRNT("kzalloc for hifi4dsp_pdata error\n");
		goto pdata_malloc_error;
	}

	/*init dsp firmware*/
	dsp_firmware = kzalloc(sizeof(*dsp_firmware), GFP_KERNEL);
	if (!dsp_firmware)
		goto dsp_firmware_malloc_error;

	/*init real dsp firmware*/
	fw = kzalloc(sizeof(*fw), GFP_KERNEL);
	if (!fw)
		goto real_fw_malloc_error;

	/*get regbase*/
	get_dsp_baseaddr(pdev);
	/*get reserve memory*/
	ret = of_reserved_mem_device_init(&pdev->dev);
	if (ret) {
		pr_err("reserved memory init fail:%d\n", ret);
		ret = -ENOMEM;
		goto err3;
	}
	mem_node = of_parse_phandle(np, "memory-region", 0);
	if (!mem_node)
		goto err3;
	rmem = of_reserved_mem_lookup(mem_node);
	of_node_put(mem_node);
	if (rmem) {
		hifi4_rmem.size = rmem->size;
		pr_debug("of read reservememsize=0x%lx\n",
			 (unsigned long)hifi4_rmem.size);
	} else {
		dev_err(&pdev->dev, "Can't retrieve reservesize\n");
		goto err3;
	}

	cma_pages =
	dma_alloc_from_contiguous(&pdev->dev,
				  PAGE_ALIGN(hifi4_rmem.size) >> PAGE_SHIFT,
				  0, false);
	if (cma_pages)
		hifi4_rmem.base = page_to_phys(cma_pages);
	else
		goto err3;
	pr_info("cma alloc hifi4 mem region success!\n");

	/*init hifi4 syslog*/
	create_hifi4_syslog();
	mutex_init(&hifi4dsp_flock);

	for (i = 0; i < dsp_cnt; i++) {
		id = i;
		p_dsp_miscdev += i;
		priv += i;
		pdata += i;
		dsp += i;
		pr_info("\nregister dsp-%d start\n", id);

		/*get boot address*/
		pr_debug("reserved_mem :base:0x%llx, size:0x%lx\n",
			 (unsigned long long)hifi4_rmem.base,
			 (unsigned long)hifi4_rmem.size);
		hifi4base = hifi4_rmem.base + (id == 0 ?
				       dspaoffset :
				       dspboffset);
		hifi4size =
			(id == 0 ?
			(dspboffset - dspaoffset) :
			((unsigned long)hifi4_rmem.size - dspboffset));
		if (!PageHighMem(cma_pages)) {
			fw_addr = phys_to_virt(hifi4base);
			pr_info("kernel addr map1 phys:0x%lx->virt:0x%lx\n",
				(unsigned long)hifi4base,
				(unsigned long)fw_addr);
		} else {
			fw_addr = mm_vmap(hifi4base, hifi4size);
			pr_info("kernel addr map2 phys:0x%lx->virt:0x%lx\n",
				(unsigned long)hifi4base,
				(unsigned long)fw_addr);
		}

		pr_debug("hifi4dsp%d, firmware :base:0x%llx, size:0x%x, virt:%lx\n",
			 id,
			 (unsigned long long)hifi4base,
			 hifi4size,
			 (unsigned long)fw_addr);

		/*init miscdevice pmscdev and register*/
		memcpy(&p_dsp_miscdev->dsp_miscdev, &hifi4dsp_miscdev[id],
		       sizeof(struct miscdevice));
		pmscdev = &p_dsp_miscdev->dsp_miscdev;

		//pmscdev = &hifi4dsp_miscdev[id];
		ret = misc_register(pmscdev);
		if (ret) {
			pr_err("register vad_miscdev error\n");
			goto err3;
		}

		/*add miscdevice to  priv*/
		priv->dev = pmscdev->this_device;

		/*init hifi4dsp_firmware and add to hifi4dsp_dsp */
		dsp_firmware->paddr = hifi4base;
		dsp_firmware->size = hifi4size;
		dsp_firmware->id = id;
		strcpy(dsp_firmware->name, "amlogic_firmware");

		/*init hifi4dsp_miscdev_t ->priv*/
		p_dsp_miscdev->priv = priv;
		if (!(p_dsp_miscdev->priv))
			pr_info("register dsp _p_dsp_miscdev->priv alloc error");
		else
			pr_info("register dsp _p_dsp_miscdev->priv alloc success");

		/*of read clk and add to priv*/
		dsp_clk = of_read_dsp_clk(pdev, id);
		priv->p_clk = dsp_clk;

		/*init hifidsp_pdata save in *hifi4dsp_data and add to priv*/
		pdata = &dsp_pdatas[i];
		pdata->fw_paddr = hifi4base;
		pdata->fw_buf = fw_addr;
		pdata->fw_max_size = hifi4size;
		pdata->reg_size = (id == 0 ?
				   g_regbases.rega_size : g_regbases.regb_size);
		pdata->reg = (id == 0 ?
			      g_regbases.dspa_addr : g_regbases.dspb_addr);
		pdata->id = id;
		priv->pdata = pdata;

		/*add hifi4dsp_dsp_device to priv*/
		priv->dsp_dev = &hifi4dsp_dev;

		/*initial hifi4dsp_dsp and add to priv*/
		mutex_init(&dsp->mutex);
		spin_lock_init(&dsp->spinlock);
		spin_lock_init(&dsp->fw_spinlock);
		INIT_LIST_HEAD(&dsp->fw_list);
		dsp->dsp_clk = dsp_clk;
		dsp->fw = fw;
		dsp->dsp_fw = dsp_firmware;
		dsp->id = id;
		dsp->freq = pdata->clk_freq;
		dsp->irq = pdata->irq;
		dsp->major_id = MAJOR(priv->dev->devt);
		dsp->dev = priv->dev;
		dsp->pdata = pdata;
		dsp->info = hifi_info;
		dsp->dsp_dev = priv->dsp_dev;
		dsp->ops = priv->dsp_dev->ops;
		priv->dsp = dsp;

		hifi4dsp_p[i] = priv;
		//p_dsp_miscdev->priv = priv;

		dev_set_drvdata(priv->dev, priv);
		pm_runtime_enable(priv->dev);
		pm_runtime_put_sync_suspend(priv->dev);
		pr_info("register dsp-%d done\n", id);
	}
	pr_info("%s done\n", __func__);
	return 0;

err3:
	kfree(fw);
real_fw_malloc_error:
	kfree(dsp_firmware);
dsp_firmware_malloc_error:
	kfree(pdata);
pdata_malloc_error:
	kfree(priv);
priv_malloc_error:
	kfree(p_dsp_miscdev);
miscdev_malloc_error:
	kfree(hifi_info);
hifi_info_malloc_error:
	kfree(dsp);
err2:
	return -ENOMEM;
err1:
	return -EINVAL;
}

static const struct of_device_id hifi4dsp_device_id[] = {
	{
		.compatible = "amlogic, hifi4dsp",
	},
	{},
};

static const struct dev_pm_ops hifi4dsp_pm_ops = {
	SET_RUNTIME_PM_OPS(hifi4dsp_runtime_suspend,
			   hifi4dsp_runtime_resume, NULL)
};

MODULE_DEVICE_TABLE(of, hifi4dsp_device_id);

static struct platform_driver hifi4dsp_platform_driver = {
	.driver = {
		.name  = "hifi4dsp",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(hifi4dsp_device_id),
		.pm = &hifi4dsp_pm_ops,
	},
	.probe  = hifi4dsp_platform_probe,
	.remove = hifi4dsp_platform_remove,
};
module_platform_driver(hifi4dsp_platform_driver);

MODULE_AUTHOR("Amlogic");
MODULE_DESCRIPTION("HiFi DSP Module Driver");
MODULE_LICENSE("GPL v2");
