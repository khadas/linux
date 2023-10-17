// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

//#define DEBUG

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
#include <linux/kdebug.h>
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
#include <linux/amlogic/scpi_protocol.h>
#include <linux/compat.h>
#include "hifi4dsp_api.h"
#include "hifi4dsp_priv.h"
#include "hifi4dsp_firmware.h"
#include "hifi4dsp_dsp.h"

#include "dsp_top.h"

struct reg_iomem_t g_regbases;
unsigned int bootlocation;
static unsigned int boot_sram_addr, boot_sram_size;
unsigned int bootlocation_b;
static unsigned int dspb_sram_addr, dspb_sram_size;
static struct mutex hifi4dsp_flock;

static struct reserved_mem hifi4_rmem; /*dsp firmware memory*/
static struct reserved_mem hifi_shmem; /*dsp share memory*/

struct hifi4dsp_priv *hifi4dsp_p[HIFI4DSP_MAX_CNT];
struct delayed_work dsp_status_work;
struct delayed_work dsp_logbuff_work;
struct workqueue_struct *dsp_status_wq;
struct workqueue_struct *dsp_logbuff_wq;
static unsigned long dsp_online;
static unsigned int dsp_monitor_period_ms = 2000;
static unsigned int dsp_logbuff_polling_period_ms;
#define		SUSPEND_CLK_FREQ	24000000

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

struct hifi4dsp_addr *hifi4dsp_get_share_memory(void)
{
	if (!hifi4dsp_p[0] || !hifi4dsp_p[0]->dsp) {
		pr_err("%s failed.\n", __func__);
		return NULL;
	}
	return &hifi4dsp_p[0]->dsp->addr;
}
EXPORT_SYMBOL(hifi4dsp_get_share_memory);

struct hifi4dsp_firmware *hifi4dsp_get_firmware(int dspid)
{
	if (!hifi4dsp_p[dspid])
		return NULL;

	return hifi4dsp_p[dspid]->dsp->dsp_fw;
}

void hifi4dsp_shm_clean(int dspid, unsigned int paddr, unsigned int size)
{
	if (!hifi4dsp_p[dspid])
		return;
	dma_sync_single_for_device(hifi4dsp_p[dspid]->dev, paddr, size, DMA_TO_DEVICE);
}

void hifi4dsp_shm_invalidate(int dspid, unsigned int paddr, unsigned int size)
{
	if (!hifi4dsp_p[dspid])
		return;
	dma_sync_single_for_device(hifi4dsp_p[dspid]->dev, paddr, size, DMA_FROM_DEVICE);
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
		if (ret) {
			kfree(usrinfo);
			pr_err("%s error: HIFI4DSP_LOAD is error", __func__);
			goto err;
		}
		pr_debug("\ninfo->fw1_name : %s\n", usrinfo->fw1_name);
		pr_debug("\ninfo->fw2_name : %s\n", usrinfo->fw2_name);
		priv->dsp->info = usrinfo;
		hifi4dsp_driver_load_fw(priv->dsp);
	break;
	case HIFI4DSP_2LOAD:
		pr_debug("%s HIFI4DSP_2LOAD\n", __func__);
		ret = copy_from_user(usrinfo, argp,
				     sizeof(struct hifi4dsp_info_t));
		if (ret) {
			kfree(usrinfo);
			pr_err("%s error: HIFI4DSP_2LOAD is error", __func__);
			goto err;
		}
		priv->dsp->info = usrinfo;
		hifi4dsp_driver_load_2fw(priv->dsp);
	break;
	case HIFI4DSP_RESET:
		pr_debug("%s HIFI4DSP_RESET\n", __func__);
		ret = copy_from_user(usrinfo, argp,
				     sizeof(struct hifi4dsp_info_t));
		if (ret) {
			kfree(usrinfo);
			pr_err("%s error: HIFI4DSP_RESET is error", __func__);
			goto err;
		}
		priv->dsp->info = usrinfo;
		hifi4dsp_driver_reset(priv->dsp);
	break;
	case HIFI4DSP_START:
		pr_debug("%s HIFI4DSP_START\n", __func__);
		ret = copy_from_user(usrinfo, argp,
				     sizeof(struct hifi4dsp_info_t));
		if (ret) {
			kfree(usrinfo);
			pr_err("%s error: HIFI4DSP_START is error", __func__);
			goto err;
		}
		priv->dsp->info = usrinfo;
		hifi4dsp_driver_dsp_start(priv->dsp);
	break;
	case HIFI4DSP_STOP:
		pr_debug("%s HIFI4DSP_STOP\n", __func__);
		ret = copy_from_user(usrinfo, argp,
				     sizeof(struct hifi4dsp_info_t));
		if (ret) {
			kfree(usrinfo);
			pr_err("%s error: HIFI4DSP_STOP is error", __func__);
			goto err;
		}
		priv->dsp->info = usrinfo;
		hifi4dsp_driver_dsp_stop(priv->dsp);
	break;
	case HIFI4DSP_SLEEP:
		pr_debug("%s HIFI4DSP_SLEEP\n", __func__);
		ret = copy_from_user(usrinfo, argp,
				     sizeof(struct hifi4dsp_info_t));
		if (ret) {
			kfree(usrinfo);
			pr_err("%s error: HIFI4DSP_SLEEP is error", __func__);
			goto err;
		}
		priv->dsp->info = usrinfo;
		hifi4dsp_driver_dsp_sleep(priv->dsp);
	break;
	case HIFI4DSP_WAKE:
		pr_debug("%s HIFI4DSP_WAKE\n", __func__);
		ret = copy_from_user(usrinfo, argp,
				     sizeof(struct hifi4dsp_info_t));
		if (ret) {
			kfree(usrinfo);
			pr_err("%s error: HIFI4DSP_WAKE is error", __func__);
			goto err;
		}
		priv->dsp->info = usrinfo;
		hifi4dsp_driver_dsp_wake(priv->dsp);
	break;
	case HIFI4DSP_GET_INFO:
		pr_debug("%s HIFI4DSP_GET_INFO\n", __func__);
		ret = copy_from_user(usrinfo, argp,
				     sizeof(struct hifi4dsp_info_t));
		if (ret) {
			kfree(usrinfo);
			pr_err("%s error: HIFI4DSP_GET_INFO is error", __func__);
			goto err;
		}
		pr_debug("%s HIFI4DSP_GET_INFO %s\n", __func__,
			 usrinfo->fw_name);
		strncpy(usrinfo->fw_name, "1234abcdef", sizeof(usrinfo->fw_name));
		usrinfo->phy_addr = priv->pdata->fw_paddr;
		usrinfo->size = priv->pdata->fw_max_size;
		ret = copy_to_user(argp, usrinfo,
				   sizeof(struct hifi4dsp_info_t));
		if (ret) {
			kfree(usrinfo);
			pr_err("%s error: HIFI4DSP_GET_INFO copy_to_user is error", __func__);
			goto err;
		}
		pr_debug("%s HIFI4DSP_GET_INFO %s\n", __func__,
			 usrinfo->fw_name);
	break;
	case HIFI4DSP_SHM_CLEAN:
		if (!strcmp(get_hifi_fw_mem_type(), "sram"))
			break;
		pr_debug("%s HIFI4DSP_SHM_CLEAN\n", __func__);
		ret = copy_from_user(&shminfo, argp, sizeof(shminfo));
		if (ret || shminfo.addr > (dsp->pdata->fw_paddr +
					dsp->pdata->fw_max_size) || shminfo.addr <
					dsp->pdata->fw_paddr ||
					shminfo.size > (dsp->pdata->fw_paddr +
					dsp->pdata->fw_max_size - shminfo.addr)) {
			kfree(usrinfo);
			pr_err("%s error: HIFI4DSP_SHM_CLEAN is error", __func__);
			goto err;
		}
		pr_debug("%s clean cache, addr:%u, size:%u\n",
			 __func__, shminfo.addr, shminfo.size);
		dma_sync_single_for_device
								(priv->dev,
								 shminfo.addr,
								 shminfo.size,
								 DMA_TO_DEVICE);
	break;
	case HIFI4DSP_SHM_INV:
		if (!strcmp(get_hifi_fw_mem_type(), "sram"))
			break;
		pr_debug("%s HIFI4DSP_SHM_INV\n", __func__);
		ret = copy_from_user(&shminfo, argp, sizeof(shminfo));
		if (ret || shminfo.addr > (dsp->pdata->fw_paddr +
					dsp->pdata->fw_max_size) || shminfo.addr <
					dsp->pdata->fw_paddr ||
					shminfo.size > (dsp->pdata->fw_paddr +
					dsp->pdata->fw_max_size - shminfo.addr)) {
			kfree(usrinfo);
			pr_err("%s error: HIFI4DSP_SHM_INV is error", __func__);
			goto err;
		}
		pr_debug("%s invalidate cache, addr:%u, size:%u\n",
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
	new_dsp_sram_fw->paddr = dsp->id ? dspb_sram_addr : boot_sram_addr;
	new_dsp_sram_fw->buf = dsp->id ? g_regbases.sram_base_b : g_regbases.sram_base;
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

enum dsp_online_status {
	DSP_NONE,
	DSPA_ONLINE = 1,
	DSPB_ONLINE,
	DSPAB_ONLINE
};

enum dsp_health {
	DSP_GOOD,
	DSPA_HANG = 1,
	DSPB_HANG,
	DSPAB_HANG
};

static enum dsp_health get_dsp_health_status(unsigned long online)
{
	enum dsp_health ret = DSP_GOOD;
	static u32 last_cnt[HIFI4DSP_MAX_CNT] = {0};
	u32 this_cnt[HIFI4DSP_MAX_CNT];

	switch (online & 0x03) {
	case DSP_NONE:
		break;
	case DSPA_ONLINE:
		this_cnt[DSPA] = readl(hifi4dsp_p[DSPA]->dsp->status_reg);
		pr_debug("[%s]dspa[%u %u]\n", __func__, last_cnt[DSPA], this_cnt[DSPA]);
		if (this_cnt[DSPA] == last_cnt[DSPA]) {
			hifi4dsp_p[DSPA]->dsp->dsphang = 1;
			ret = DSPA_HANG;
		}
		last_cnt[DSPA] = this_cnt[DSPA];
		break;
	case DSPB_ONLINE:
		this_cnt[DSPB] = readl(hifi4dsp_p[DSPB]->dsp->status_reg);
		pr_debug("[%s]dspb[%u %u]\n", __func__, last_cnt[DSPB], this_cnt[DSPB]);
		if (this_cnt[DSPB] == last_cnt[DSPB]) {
			hifi4dsp_p[DSPB]->dsp->dsphang = 1;
			ret = DSPB_HANG;
		}
		last_cnt[DSPB] = this_cnt[DSPB];
		break;
	case DSPAB_ONLINE:
		this_cnt[DSPA] = readl(hifi4dsp_p[DSPA]->dsp->status_reg);
		this_cnt[DSPB] = readl(hifi4dsp_p[DSPB]->dsp->status_reg);
		pr_debug("[%s]dspa[%u %u]dspb[%u %u]\n", __func__,
			last_cnt[DSPA], this_cnt[DSPA], last_cnt[DSPB], this_cnt[DSPB]);
		if (this_cnt[DSPA] == last_cnt[DSPA] && this_cnt[DSPB] == last_cnt[DSPB]) {
			hifi4dsp_p[DSPA]->dsp->dsphang = 1;
			hifi4dsp_p[DSPB]->dsp->dsphang = 1;
			ret = DSPAB_HANG;
		} else if (this_cnt[DSPA] == last_cnt[DSPA]) {
			hifi4dsp_p[DSPA]->dsp->dsphang = 1;
			hifi4dsp_p[DSPB]->dsp->dsphang = 0;
			ret = DSPA_HANG;
		} else if (this_cnt[DSPB] == last_cnt[DSPB]) {
			hifi4dsp_p[DSPA]->dsp->dsphang = 0;
			hifi4dsp_p[DSPB]->dsp->dsphang = 1;
			ret = DSPB_HANG;
		} else {
			hifi4dsp_p[DSPA]->dsp->dsphang = 0;
			hifi4dsp_p[DSPB]->dsp->dsphang = 0;
		}
		last_cnt[DSPA] = this_cnt[DSPA];
		last_cnt[DSPB] = this_cnt[DSPB];
		break;
	}
	return ret;
}

static void dsp_health_monitor(struct work_struct *work)
{
	char data[20], *envp[] = { data, NULL };

	if (dsp_online == 0)
		return;

	switch (get_dsp_health_status(dsp_online)) {
	case DSP_GOOD:
		break;
	case DSPA_HANG:
		snprintf(data, sizeof(data), "ACTION=DSP_WTD_A");
		kobject_uevent_env(&hifi4dsp_p[DSPA]->dsp->dev->kobj, KOBJ_CHANGE, envp);
		pr_debug("[%s][DSPA_HANG]\n", __func__);
		break;
	case DSPB_HANG:
		snprintf(data, sizeof(data), "ACTION=DSP_WTD_B");
		kobject_uevent_env(&hifi4dsp_p[DSPB]->dsp->dev->kobj, KOBJ_CHANGE, envp);
		pr_debug("[%s][DSPB_HANG]\n", __func__);
		break;
	case DSPAB_HANG:
		snprintf(data, sizeof(data), "ACTION=DSP_WTD_WHOLE");
		kobject_uevent_env(&hifi4dsp_p[DSPA]->dsp->dev->kobj, KOBJ_CHANGE, envp);
		pr_debug("[%s][DSPAB_HANG]\n", __func__);
		break;
	}

	queue_delayed_work(dsp_status_wq, &dsp_status_work,
		msecs_to_jiffies(dsp_monitor_period_ms));
}

static void dsp_logbuff_polling(struct work_struct *work)
{
	struct dsp_ring_buffer *logbuffa =
		(dsp_online & 0x1) ? hifi4dsp_p[DSPA]->dsp->logbuff : NULL;
	struct dsp_ring_buffer *logbuffb =
		(dsp_online & 0x2) ? hifi4dsp_p[DSPB]->dsp->logbuff : NULL;
	unsigned int len_a, len_b;

	if (logbuffa)
		pr_debug("[%s %d][0x%x 0x%x 0x%x %u %u]\n", __func__, __LINE__,
			logbuffa->magic, logbuffa->basepaddr, logbuffa->size,
			logbuffa->headr, logbuffa->tail);

	len_a = get_logbuff_loglen(logbuffa);
	len_b = get_logbuff_loglen(logbuffb);
	if (len_a)
		show_logbuff_log(logbuffa, DSPA, len_a);
	if (len_b)
		show_logbuff_log(logbuffb, DSPB, len_b);

	queue_delayed_work(dsp_logbuff_wq, &dsp_logbuff_work,
		msecs_to_jiffies(dsp_logbuff_polling_period_ms));
}

static void init_and_start_dsp_log_polling(void)
{
	if (!dsp_logbuff_wq) {
		dsp_logbuff_wq = create_freezable_workqueue("dsplogbuff_wq");
		if (!dsp_logbuff_wq)
			pr_err("create dsplogbuff_wq failed.\n");

		INIT_DEFERRABLE_WORK(&dsp_logbuff_work, dsp_logbuff_polling);
		queue_delayed_work(dsp_logbuff_wq, &dsp_logbuff_work,
			msecs_to_jiffies(dsp_logbuff_polling_period_ms));
	}
}

static int hifi4dsp_driver_dsp_start(struct hifi4dsp_dsp *dsp)
{
	struct  hifi4dsp_info_t *info;
	struct dsp_ring_buffer *rb;
	int ret;
	char requestinfo[] = "AP request dsp logbuffer info";

	pr_debug("%s\n", __func__);
	info = (struct  hifi4dsp_info_t *)dsp->info;
	pr_debug("dsp_id: %d\n", dsp->id);
	pr_debug("dsp_frequence: %d Hz\n", dsp->freq);
	pr_debug("dsp_start_addr: 0x%llx\n",
		 (unsigned long long)dsp->dsp_fw->paddr);

	if (dsp->dspstarted == 1) {
		pr_err("duplicate start dsp\n");
		return -EPERM;
	}

	if (!dsp->dsp_clk) {
		pr_err("dsp_clk=NULL\n");
		return -EINVAL;
	}

	ret = clk_set_rate(dsp->dsp_clk, dsp->freq);
	if (ret) {
		pr_debug("Can not set clock rate\n");
		return ret;
	}
	clk_prepare_enable(dsp->dsp_clk);

	switch (dsp->id) {
	case DSPA:
		if (bootlocation == 1)
			soc_dsp_bootup(dsp->id, dsp->dsp_fw->paddr, dsp->freq);
		else if (bootlocation == 2)
			soc_dsp_bootup(dsp->id, boot_sram_addr, dsp->freq);
		break;
	case DSPB:
		if (bootlocation_b == 3)
			soc_dsp_bootup(dsp->id, dspb_sram_addr, dsp->freq);
		else
			soc_dsp_bootup(dsp->id, dsp->dsp_fw->paddr, dsp->freq);
		break;
	default:
		break;
	}

	dsp->info = NULL;
	dsp->dspstarted = 1;
	set_bit(dsp->id, &dsp_online);
	pr_warn("[%s]dsp_online=0x%lx\n", __func__, dsp_online);
	if (hifi4dsp_p[dsp->id]->dsp->status_reg && !dsp_status_wq) {
		dsp_status_wq = create_freezable_workqueue("dspstatus_wq");
		if (!dsp_status_wq) {
			pr_err("create dspstatus_wq failed.\n");
			return -EINVAL;
		}
		INIT_DEFERRABLE_WORK(&dsp_status_work, dsp_health_monitor);
		queue_delayed_work(dsp_status_wq, &dsp_status_work,
			msecs_to_jiffies(dsp_monitor_period_ms));
	}

	if (!dsp_logbuff_polling_period_ms)
		goto out;

	msleep(100);
	rb = kzalloc(sizeof(*rb), GFP_KERNEL);
	if (!rb)
		goto out;
	scpi_send_data(requestinfo, sizeof(requestinfo), dsp->id ? SCPI_DSPB : SCPI_DSPA,
		SCPI_CMD_HIFI5_SYSLOG_START, rb, sizeof(*rb));
	if (rb->magic == DSP_LOGBUFF_MAGIC) {
		if (pfn_valid(__phys_to_pfn(rb->basepaddr)))
			dsp->logbuff =
			(struct dsp_ring_buffer *)__phys_to_virt(rb->basepaddr);
		else
			dsp->logbuff =
			(struct dsp_ring_buffer *)ioremap_cache(rb->basepaddr,
			rb->size + sizeof(*rb) - 4);
		pr_debug("[%s %d][0x%x 0x%x %u %u %u %u]\n", __func__, __LINE__,
		dsp->logbuff->magic, dsp->logbuff->basepaddr, dsp->logbuff->head,
		dsp->logbuff->headr, dsp->logbuff->tail, dsp->logbuff->size);
		init_and_start_dsp_log_polling();
	} else {
		dsp->logbuff = NULL;
	}
	kfree(rb);
out:
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
	char message[30];

	info = (struct  hifi4dsp_info_t *)dsp->info;
	pr_debug("%s\n", __func__);
	strncpy(message, "SCPI_CMD_HIFI4STOP", sizeof(message));

	if (!dsp->dspstarted)
		goto out;

	if (hifi4dsp_p[dsp->id]->dsp->status_reg && !dsp->dsphang) {
		scpi_send_data(message, sizeof(message), dsp->id ? SCPI_DSPB : SCPI_DSPA,
			SCPI_CMD_HIFI4STOP, NULL, 0);
		msleep(50);
	}
	clear_bit(dsp->id, &dsp_online);
	soc_dsp_poweroff(info->id);
	hifi4dsp_driver_dsp_clk_off(dsp);
	dsp->dspstarted = 0;
	dsp->info = NULL;
	pr_warn("[%s]dsp_online=0x%lx\n", __func__, dsp_online);
	if (!dsp_online && dsp_status_wq) {
		cancel_delayed_work_sync(&dsp_status_work);
		flush_workqueue(dsp_status_wq);
		destroy_workqueue(dsp_status_wq);
		memset(&dsp_status_work, 0, sizeof(dsp_status_work));
		dsp_status_wq = NULL;
	}
	if (dsp_logbuff_wq &&
	(hifi4dsp_p[dsp->id ? DSPB : DSPA]->dsp->dspstarted == 0 ||
	!(hifi4dsp_p[dsp->id ? DSPB : DSPA]->dsp->logbuff))) {
		cancel_delayed_work_sync(&dsp_logbuff_work);
		flush_workqueue(dsp_logbuff_wq);
		destroy_workqueue(dsp_logbuff_wq);
		memset(&dsp_logbuff_work, 0, sizeof(dsp_logbuff_work));
		dsp_logbuff_wq = NULL;
	}
	dsp->logbuff = NULL;
out:
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
	int ret;

	dev = dsp->dev;
	pr_debug("%s\n", __func__);
	ret = hifi4dsp_driver_resource_map(dsp, pdata);
	if (ret < 0) {
		dev_err(dev, "failed to map resources\n");
		return ret;
	}
	return 0;
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
	int id = 0, ret = 0;
	u32 dsp_cnt = 0;

	ret = of_property_read_u32(pdev->dev.of_node, "dsp-cnt", &dsp_cnt);
	if (ret < 0) {
		dev_err(&pdev->dev, "Can't retrieve dsp-cnt\n");
		ret = -EINVAL;
		goto dsp_cnt_error;
	}

	hifi_syslog_remove();

	priv = hifi4dsp_privdata();
	for (id = 0; id < dsp_cnt; id++) {
		if (!priv)
			continue;
		if (priv->dev)
			device_destroy(priv->class, priv->dev->devt);
		priv += 1;
	}

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

static void *mm_vmap(phys_addr_t phys, unsigned long size, pgprot_t pgprotattr)
{
	u32 offset, npages;
	struct page **pages = NULL;
	pgprot_t pgprot = pgprotattr;
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
		strncpy(clk_name, "dspa_clk", sizeof(clk_name));
		p_clk = devm_clk_get(&pdev->dev, clk_name);
	} else if (dsp_id == 1) {
		strncpy(clk_name, "dspb_clk", sizeof(clk_name));
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

	if (pdev->num_resources < 2)
		return;
	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!res) {
		dev_err(&pdev->dev, "failed to get dspb base address\n");
		return;
	}
	g_regbases.dspb_addr = devm_ioremap_resource(&pdev->dev, res);
	g_regbases.regb_size = resource_size(res);
}

void get_dsp_statusreg(struct platform_device *pdev, int dsp_cnt,
	struct hifi4dsp_priv **hifi4dsp_p)
{
	struct resource *res;
	int i;

	if (pdev->num_resources < 3)
		return;
	for (i = 0; i < dsp_cnt; i++) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, 2 + i);
		if (!res) {
			dev_err(&pdev->dev, "failed to get dsp%d status register.\n", i);
			return;
		}
		hifi4dsp_p[i]->dsp->status_reg = devm_ioremap_resource(&pdev->dev, res);
		if (IS_ERR(hifi4dsp_p[i]->dsp->status_reg))
			hifi4dsp_p[i]->dsp->status_reg = NULL;
	}
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

static int hifi4dsp_attach_pd(struct device *dev, int dsp_cnt)
{
	struct hifi4dsp_dsp *dsp = dev_get_drvdata(dev);
	struct device_link *link;
	char *pd_name[2] = {"dspa", "dspb"};
	int i;

	if (dev->pm_domain)
		return -1;
	for (i = 0; i < dsp_cnt; i++) {
		dsp += i;
		dsp->pd_dsp = dev_pm_domain_attach_by_name(dev, pd_name[i]);
		if (IS_ERR(dsp->pd_dsp))
			return PTR_ERR(dsp->pd_dsp);
		if (!dsp->pd_dsp)
			return -1;
		link = device_link_add(dev, dsp->pd_dsp,
				       DL_FLAG_STATELESS |
				       DL_FLAG_PM_RUNTIME |
				       DL_FLAG_RPM_ACTIVE);
		if (!link) {
			dev_err(dev, "Failed to add device_link to %s pd.\n",
				pd_name[i]);
			return -EINVAL;
		}
	}
	return 0;
}

static int get_hifi_firmware_mem(struct reserved_mem *fwmem, struct platform_device *pdev)
{
	int ret;
	struct device_node *mem_node;
	struct reserved_mem *tmp = NULL;
	struct page *cma_pages = NULL;
	u32 dspsrambase = 0, dspsramsize = 0;

	/*parse sram fwmem preferentially*/
	ret = of_property_read_u32(pdev->dev.of_node, "dspsrambase", &dspsrambase);
	if (ret < 0)
		goto parse_cma;

	pr_debug("of read dspsrambase=0x%08x\n", dspsrambase);

	ret = of_property_read_u32(pdev->dev.of_node, "dspsramsize", &dspsramsize);
	if (ret < 0) {
		dev_err(&pdev->dev, "Can't retrieve dspsramsize\n");
		goto parse_cma;
	}
	pr_debug("of read dspsramsize=0x%08x\n", dspsramsize);

	fwmem->size = dspsramsize;
	fwmem->base = dspsrambase;
	fwmem->priv = "sram";
	pr_info("of get sram memory region success[0x%lx 0x%lx]\n",
		(unsigned long)fwmem->base, (unsigned long)fwmem->size);
	return 0;

	/*parse ddr fwmem*/
parse_cma:
	ret = of_reserved_mem_device_init(&pdev->dev);
	if (ret) {
		pr_debug("reserved memory init fail:%d\n", ret);
		goto out;
	}
	mem_node = of_parse_phandle(pdev->dev.of_node, "memory-region", 0);
	if (!mem_node) {
		ret = -1;
		goto out;
	}
	tmp = of_reserved_mem_lookup(mem_node);
	of_node_put(mem_node);
	if (tmp) {
		fwmem->size = tmp->size;
		cma_pages = cma_alloc(dev_get_cma_area(&pdev->dev),
		      PAGE_ALIGN(fwmem->size) >> PAGE_SHIFT, 0, false);
		if (cma_pages) {
			fwmem->base = page_to_phys(cma_pages);
			fwmem->priv = "ddr";
			pr_info("of read fwmem phys = [0x%lx 0x%lx]\n",
				(unsigned long)fwmem->base, (unsigned long)fwmem->size);
		}
	} else {
		ret = -1;
		dev_err(&pdev->dev, "Can't retrieve reserve memory region\n");
	}
out:
	return ret;
}

static int get_hifi_share_mem(struct reserved_mem *shmem, struct platform_device *pdev)
{
	struct resource *dsp_shm_res;

	if (pdev->num_resources < 5)
		return -1;
	/*parse shmem*/
	dsp_shm_res = platform_get_resource(pdev, IORESOURCE_MEM, 4);
	if (!dsp_shm_res) {
		dev_err(&pdev->dev, "failed to get dsp share memory resource.\n");
		return -1;
	}
	shmem->base = dsp_shm_res->start;
	shmem->size = resource_size(dsp_shm_res);
	pr_info("of read shmem phys = [0x%lx 0x%lx]\n",
		(unsigned long)shmem->base, (unsigned long)shmem->size);

	return 0;
}

static void hifi_fw_mem_update(int dsp_id, phys_addr_t *base, int *size)
{
	if (!strcmp(hifi4_rmem.priv, "sram")) {
		*base = hifi4_rmem.base + (dsp_id == 0 ? 0 : hifi4_rmem.size >> 1);
		*size = hifi4_rmem.size >> 1;
	}
}

static void *hifi_fw_mem_map(phys_addr_t base, int size)
{
	if (!strcmp(hifi4_rmem.priv, "sram"))
		return ioremap_nocache(base, size);
	else
		return mm_vmap(base, size, pgprot_dmacoherent(PAGE_KERNEL));
}

void *get_hifi_fw_mem_type(void)
{
	return hifi4_rmem.priv;
}

static int hifi4dsp_clk_to_24M(struct hifi4dsp_dsp *dsp)
{
	int ret;

	if (!dsp->dsp_clk) {
		pr_err("dsp_clk=NULL\n");
		return  -EINVAL;
	}

	ret = clk_set_rate(dsp->dsp_clk, SUSPEND_CLK_FREQ);
	if (ret) {
		pr_err("%s: error in setting dsp clk rate!\n",
		       __func__);
		return ret;
	}

	return 0;
}

static int hifi4dsp_clk_to_normal(struct hifi4dsp_dsp *dsp)
{
	int ret;

	if (!dsp->dsp_clk) {
		pr_err("dsp_clk=NULL\n");
		return  -EINVAL;
	}

	ret = clk_set_rate(dsp->dsp_clk, dsp->freq);
	if (ret) {
		pr_err("%s: error in setting dsp clk rate!\n",
		       __func__);
		return ret;
	}

	return 0;
}

static int hifi4dsp_driver_dsp_suspend(struct hifi4dsp_dsp *dsp)
{
	char message[30];

	strncpy(message, "SCPI_CMD_HIFI4SUSPEND", sizeof(message));

	if (!dsp->id)
		scpi_send_data(message, sizeof(message),
				  SCPI_DSPA, SCPI_CMD_HIFI4SUSPEND,
				  message, sizeof(message));
	else
		scpi_send_data(message, sizeof(message),
				  SCPI_DSPB, SCPI_CMD_HIFI4SUSPEND,
				  message, sizeof(message));
	hifi4dsp_clk_to_24M(dsp);

	return 0;
}

static int hifi4dsp_driver_dsp_resume(struct hifi4dsp_dsp *dsp)
{
	char message[30];

	/*switch dsp clk to normal*/
	hifi4dsp_clk_to_normal(dsp);

	strncpy(message, "SCPI_CMD_HIFI4RESUME", sizeof(message));

	if (!dsp->id)
		scpi_send_data(message, sizeof(message),
				  SCPI_DSPA, SCPI_CMD_HIFI4RESUME,
				  message, sizeof(message));
	else
		scpi_send_data(message, sizeof(message),
				  SCPI_DSPB, SCPI_CMD_HIFI4RESUME,
				  message, sizeof(message));
	return 0;
}

int of_read_dsp_cnt(struct platform_device *pdev)
{
	int ret;
	u32 dsp_cnt = 0;

	ret = of_property_read_u32(pdev->dev.of_node, "dsp-cnt", &dsp_cnt);
	if (ret < 0) {
		dev_err(&pdev->dev, "Can't retrieve dsp-cnt\n");
		return -EINVAL;
	}
	pr_debug("%s of read dsp-cnt=%d\n", __func__, dsp_cnt);

	return dsp_cnt;
}

static int dsp_suspend(struct device *dev)
{
	int dsp_cnt, i;
	struct hifi4dsp_dsp *dsp = NULL;
	struct platform_device *pdev = to_platform_device(dev);

	dsp_cnt = of_read_dsp_cnt(pdev);

	if (dsp_cnt > 0) {
		for (i = 0; i < dsp_cnt; i++) {
			dsp = hifi4dsp_p[i]->dsp;
			if (dsp->dspstarted == 1 && dsp->suspend_resume_support) {
				pr_debug("AP send suspend cmd to dsp...\n");
				hifi4dsp_driver_dsp_suspend(dsp);
			}
		}
	}

	return 0;
}

static int dsp_resume(struct device *dev)
{
	int dsp_cnt, i;
	struct hifi4dsp_dsp *dsp = NULL;
	struct platform_device *pdev = to_platform_device(dev);

	dsp_cnt = of_read_dsp_cnt(pdev);

	if (dsp_cnt > 0) {
		for (i = 0; i < dsp_cnt; i++) {
			dsp = hifi4dsp_p[i]->dsp;
			if (dsp->dspstarted == 1 && dsp->suspend_resume_support) {
				pr_debug("AP send resume cmd to dsp...\n");
				hifi4dsp_driver_dsp_resume(dsp);
			}
		}
	}

	return 0;
}

static ssize_t suspend_store(struct device *dev,
			     struct device_attribute *attr,
			     const char *buf, size_t size)
{
	char message[30];
	struct hifi4dsp_dsp *dsp = NULL;
	int dspid;

	strncpy(message, "SCPI_CMD_HIFI4SUSPEND", sizeof(message));

	if (!strncmp(buf, "hifi4a", 6)) {
		dspid = 0;
	} else if (!strncmp(buf, "hifi4b", 6)) {
		dspid = 1;
	} else {
		pr_err("please input the right args : hifi4a/hifi4b\n");
		return 0;
	}
	dsp = hifi4dsp_p[dspid]->dsp;

	if (!dsp || !dsp->suspend_resume_support)
		return 0;

	scpi_send_data(message, sizeof(message),
				  dspid ? SCPI_DSPB : SCPI_DSPA, SCPI_CMD_HIFI4SUSPEND,
				  message, sizeof(message));
	hifi4dsp_clk_to_24M(dsp);

	return size;
}
static DEVICE_ATTR_WO(suspend);

static ssize_t resume_store(struct device *dev,
			    struct device_attribute *attr,
			    const char *buf, size_t size)
{
	char message[30];
	struct hifi4dsp_dsp *dsp = NULL;
	int dspid;

	strncpy(message, "SCPI_CMD_HIFI4RESUME", sizeof(message));

	if (!strncmp(buf, "hifi4a", 6)) {
		dspid = 0;
	} else if (!strncmp(buf, "hifi4b", 6)) {
		dspid = 1;
	} else {
		pr_err("please input the right args : hifi4a/hifi4b\n");
		return 0;
	}
	dsp = hifi4dsp_p[dspid]->dsp;

	if (!dsp || !dsp->suspend_resume_support)
		return 0;

	hifi4dsp_clk_to_normal(dsp);
	scpi_send_data(message, sizeof(message),
				  dspid ? SCPI_DSPB : SCPI_DSPA, SCPI_CMD_HIFI4RESUME,
				  message, sizeof(message));

	return size;
}
static DEVICE_ATTR_WO(resume);

static int die_notify(struct notifier_block *self,
			unsigned long cmd, void *ptr)
{
	dsp_logbuff_show(DSPA);
	dsp_logbuff_show(DSPB);
	return NOTIFY_DONE;
}

static struct notifier_block die_notifier = {
	.notifier_call	= die_notify,
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
	enum dsp_start_mode startmode;
	u32 optimize_longcall[2];
	u32 sram_remap_addr[4];
	u32 pm_support[2];

	phys_addr_t hifi4base;
	int hifi4size;
	void *fw_addr = NULL;

	struct hifi4dsp_firmware *dsp_firmware;
	struct firmware *fw = NULL;

	struct device_node *np;
	struct clk *dsp_clk = NULL;

	struct hifi4dsp_info_t *hifi_info = NULL;
	unsigned int dsp_freqs[2];

	np = pdev->dev.of_node;

	/*dsp boot offset */
	ret = of_property_read_u32(np, "dsp-cnt", &dsp_cnt);
	if (ret < 0 || dsp_cnt <= 0) {
		dev_err(&pdev->dev, "Can't retrieve dsp-cnt\n");
		return -EINVAL;
	}
	pr_debug("%s of read dsp-cnt=%d\n", __func__, dsp_cnt);

	ret = of_property_read_u32(np, "dsp-start-mode", &startmode);
	if (ret < 0) {
		dev_err(&pdev->dev, "can't retrieve dsp-start-mode\n");
		return -EINVAL;
	}
	pr_debug("%s of read dsp_start_mode = %d\n", __func__, startmode);

	ret = of_property_read_u32(np, "dspaoffset", &dspaoffset);
	if (ret < 0) {
		dev_err(&pdev->dev, "Can't retrieve dspaoffset\n");
		return -EINVAL;
	}
	pr_debug("%s of read dspaoffset=0x%08x\n", __func__, dspaoffset);

	ret = of_property_read_u32(np, "dspboffset", &dspboffset);
	if (ret < 0) {
		dev_err(&pdev->dev, "Can't retrieve dspboffset\n");
		return -EINVAL;
	}
	pr_debug("%s of read dspboffset=0x%08x\n", __func__, dspboffset);

	ret = of_property_read_u32(np, "dsp-monitor-period-ms", &dsp_monitor_period_ms);
	if (ret && ret != -EINVAL)
		dev_err(&pdev->dev, "of get dsp-monitor-period-ms property failed\n");

	/*boot from DDR or SRAM or ...*/
	ret = of_property_read_u32(np, "bootlocation", &bootlocation);
	if (ret < 0) {
		dev_err(&pdev->dev, "Can't retrieve bootlocation\n");
		return -EINVAL;
	}
	pr_debug("%s of read dsp bootlocation=%x\n", __func__, bootlocation);
	if (bootlocation == 1) {
		pr_info("Dsp boot from DDR !\n");
	} else if (bootlocation == 2) {
		ret = of_property_read_u32(np, "boot_sram_addr",
					   &boot_sram_addr);
		if (ret < 0) {
			dev_err(&pdev->dev, "Can't retrieve boot_sram_addr\n");
			return -EINVAL;
		}
		ret = of_property_read_u32(np, "boot_sram_size",
					   &boot_sram_size);
		if (ret < 0) {
			dev_err(&pdev->dev, "Can't retrieve boot_sram_size\n");
			return -EINVAL;
		}
		pr_info("DspA boot from SRAM !boot addr : 0x%08x, allocate size: 0x%08x\n",
			boot_sram_addr, boot_sram_size);
		g_regbases.sram_base = ioremap_nocache(boot_sram_addr,
						       boot_sram_size);
	}

	ret = of_property_read_u32(np, "bootlocation_b", &bootlocation_b);
	if (ret && ret != -EINVAL)
		dev_err(&pdev->dev, "of get bootlocation_b property failed\n");
	if (bootlocation_b == DDR_SRAM) {
		ret = of_property_read_u32(np, "dspb_sram_addr",
					   &dspb_sram_addr);
		if (ret < 0) {
			dev_err(&pdev->dev, "Can't retrieve dspb_sram_addr\n");
			return -EINVAL;
		}
		ret = of_property_read_u32(np, "dspb_sram_size",
					   &dspb_sram_size);
		if (ret < 0) {
			dev_err(&pdev->dev, "Can't retrieve dspb_sram_size\n");
			return -EINVAL;
		}
		pr_info("DspB boot from SRAM !boot addr : 0x%08x, allocate size: 0x%08x\n",
			dspb_sram_addr, dspb_sram_size);
		g_regbases.sram_base_b = ioremap_nocache(dspb_sram_addr, dspb_sram_size);
	}

	ret = of_property_read_u32_array(np, "optimize_longcall", &optimize_longcall[0], 2);
	if (ret && ret != -EINVAL)
		dev_err(&pdev->dev, "of get optimize_longcall property failed\n");
	ret = of_property_read_u32_array(np, "sram_remap_addr", &sram_remap_addr[0], 4);
	if (ret && ret != -EINVAL)
		dev_err(&pdev->dev, "of get sram_remap_addr property failed\n");
	ret = of_property_read_u32_array(np, "suspend_resume_support", &pm_support[0], 2);
	if (ret && ret != -EINVAL)
		dev_err(&pdev->dev, "of get suspend_resume_support property failed\n");

	ret = of_property_read_u32(np, "logbuff-polling-period-ms",
			&dsp_logbuff_polling_period_ms);
	if (ret && ret != -EINVAL)
		dev_err(&pdev->dev, "of get logbuff-polling-period-ms property failed\n");

	/*init hifi4dsp_dsp*/
	dsp = devm_kzalloc(&pdev->dev, sizeof(*dsp) * dsp_cnt, GFP_KERNEL);
	if (!dsp)
		return -ENOMEM;
	/*
	 * The driver always uses memory and doesn't need to be freed.
	 */
	/* coverity[leaked_storage:SUPPRESS] */

	/*init hifi4dsp_info_t*/
	hifi_info = devm_kzalloc(&pdev->dev, sizeof(*hifi_info) * dsp_cnt, GFP_KERNEL);
	if (!hifi_info)
		return -ENOMEM;

	/*init miscdev_t, miscdevice*/
	p_dsp_miscdev = devm_kzalloc(&pdev->dev, sizeof(struct hifi4dsp_miscdev_t) * dsp_cnt,
				GFP_KERNEL);
	if (!p_dsp_miscdev) {
		HIFI4DSP_PRNT("devm_kzalloc for p_dsp_miscdev error\n");
		return -ENOMEM;
	}

	/*init hifi4dsp_priv*/
	priv = devm_kzalloc(&pdev->dev, sizeof(struct hifi4dsp_priv) * dsp_cnt,
		       GFP_KERNEL);
	if (!priv) {
		HIFI4DSP_PRNT("devm_kzalloc for hifi4dsp_priv error\n");
		return -ENOMEM;
	}

	/*init hifi4dsp_pdata*/
	pdata = devm_kzalloc(&pdev->dev, sizeof(struct hifi4dsp_pdata) * dsp_cnt, GFP_KERNEL);
	if (!pdata) {
		HIFI4DSP_PRNT("devm_kzalloc for hifi4dsp_pdata error\n");
		return -ENOMEM;
	}

	/*init dsp firmware*/
	dsp_firmware = devm_kzalloc(&pdev->dev, sizeof(*dsp_firmware) * dsp_cnt, GFP_KERNEL);
	if (!dsp_firmware) {
		HIFI4DSP_PRNT("devm_kzalloc for hifi4dsp_firmware error\n");
		return -ENOMEM;
	}

	/*init real dsp firmware*/
	fw = devm_kzalloc(&pdev->dev, sizeof(*fw) * dsp_cnt, GFP_KERNEL);
	if (!fw) {
		HIFI4DSP_PRNT("devm_kzalloc for firmware error\n");
		return -ENOMEM;
	}

	/*get regbase*/
	get_dsp_baseaddr(pdev);

	if (get_hifi_firmware_mem(&hifi4_rmem, pdev))
		return -EINVAL;
	if (!get_hifi_share_mem(&hifi_shmem, pdev)) {
		dsp->addr.smem_paddr = hifi_shmem.base;
		dsp->addr.smem_size = hifi_shmem.size;
		if (dsp->addr.smem_paddr && dsp->addr.smem_size) {
			dsp->addr.smem = mm_vmap(dsp->addr.smem_paddr, dsp->addr.smem_size,
				pgprot_dmacoherent(PAGE_KERNEL));
			pr_info("sharemem map phys:0x%lx-->virt:0x%lx\n",
				(unsigned long)dsp->addr.smem_paddr, (unsigned long)dsp->addr.smem);
		}
	}

	if (dsp_cnt > 1) {
		platform_set_drvdata(pdev, dsp);
		ret = hifi4dsp_attach_pd(&pdev->dev, dsp_cnt);
		if (ret < 0)
			return -EINVAL;
	}

	/*get dsp_clk_freqs */
	ret = of_property_read_u32_array(np, "dsp_clk_freqs", &dsp_freqs[0], dsp_cnt);
	if (!ret)
		for (i = 0; i < dsp_cnt; i++)
			dsp_pdatas[i].clk_freq = dsp_freqs[i];
	else
		pr_debug("\ndsp_clk_freqs not configed in dtsi.");

	mutex_init(&hifi4dsp_flock);

	for (i = 0; i < dsp_cnt; i++) {
		id = i;
		p_dsp_miscdev += i;
		priv += i;
		pdata += i;
		dsp += i;
		dsp_firmware += i;
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
			(dspboffset - dspaoffset - hifi_shmem.size) :
			((unsigned long)hifi4_rmem.size - dspboffset));
		hifi_fw_mem_update(id, &hifi4base, &hifi4size);
		fw_addr = hifi_fw_mem_map(hifi4base, hifi4size);
		pr_info("hifi4dsp%d, firmware :base:0x%llx, size:0x%x, virt:%lx\n",
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
			goto err;
		}

		/*add miscdevice to  priv*/
		priv->dev = pmscdev->this_device;

		/*init hifi4dsp_firmware and add to hifi4dsp_dsp */
		dsp_firmware->paddr = hifi4base;
		dsp_firmware->size = hifi4size;
		dsp_firmware->id = id;
		dsp_firmware->buf = fw_addr;
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
		dsp->start_mode = startmode;
		dsp->dsphang = 0;
		dsp->optimize_longcall = optimize_longcall[id];
		dsp->sram_remap_addr[0] = sram_remap_addr[2 * id];
		dsp->sram_remap_addr[1] = sram_remap_addr[2 * id + 1];
		dsp->suspend_resume_support = pm_support[id];
		priv->dsp = dsp;

		hifi4dsp_p[i] = priv;

		if (dsp_cnt > 1) {
			pm_runtime_put_sync_suspend(dsp->pd_dsp);
		} else {
			pm_runtime_enable(&pdev->dev);
			priv->dev_pd = &pdev->dev;
		}
		create_hifi_debugfs_files(dsp);
		pr_info("register dsp-%d done\n", id);
	}
	get_dsp_statusreg(pdev, dsp_cnt, hifi4dsp_p);
	device_create_file(&pdev->dev, &dev_attr_suspend);
	device_create_file(&pdev->dev, &dev_attr_resume);
	if (register_die_notifier(&die_notifier))
		pr_err("%s,register die notifier failed!\n", __func__);

	pr_info("%s done\n", __func__);
	return 0;
err:
	for (i -= 1; i >= 0; i--) {
		pmscdev -= 1;
		misc_deregister(pmscdev);
	}

	return -ENODEV;
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
	.suspend = dsp_suspend,
	.resume = dsp_resume,
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
