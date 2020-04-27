// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

//#define DEBUG
#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/io.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/firmware.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include "hifi4dsp_priv.h"
#include "hifi4dsp_firmware.h"
#include "hifi4dsp_dsp.h"

static inline void hifi4dsp_fw_memcpy(void __iomem *dest,
				      void *src, u32 bytes)
{
	memcpy_toio(dest, src, bytes);
}

/* general a new hifi4dsp_firmware object, called by hi-level functions */
struct hifi4dsp_firmware *hifi4dsp_fw_new(struct hifi4dsp_dsp *dsp,
					  const struct firmware *fw,
					  void *private)
{
	struct hifi4dsp_firmware *dsp_fw;

	dsp_fw = kzalloc(sizeof(*dsp_fw), GFP_KERNEL);

	if (!dsp_fw)
		goto dsp_fw_malloc_err;

	 dsp_fw->dsp = dsp;
	 //dsp_fw->priv = dsp->priv;

	if (private)
		dsp_fw->private = private;
	if (fw)
		dsp_fw->size = fw->size;
	pr_debug("%s done\n", __func__);

dsp_fw_malloc_err:
	return dsp_fw;
}

/*dsp_fw must be initied before this operation,
 *special the *dsp pointer must not be null
 */
int hifi4dsp_fw_add(struct hifi4dsp_firmware *dsp_fw)
{
	unsigned long flags;
	struct hifi4dsp_dsp *dsp;

	if (!dsp_fw || !dsp_fw->dsp)
		return -1;
	dsp = dsp_fw->dsp;
	spin_lock_irqsave(&dsp->fw_spinlock, flags);
	list_add_tail(&dsp_fw->list, &dsp->fw_list);
	dsp->fw_cnt++;
	dsp_fw->id = dsp->fw_cnt;
	spin_unlock_irqrestore(&dsp->fw_spinlock, flags);
	pr_debug("%s done\n", __func__);
	return 0;
}

static struct hifi4dsp_firmware	*hifi4dsp_fw_search_by_name
(struct hifi4dsp_dsp *dsp, char *name)
{
	struct hifi4dsp_firmware *dsp_fw = NULL;
	struct hifi4dsp_firmware *pfw = NULL;
	unsigned long flags;

	spin_lock_irqsave(&dsp->fw_spinlock, flags);
	if (list_empty(&dsp->fw_list)) {
		pfw = NULL;
		pr_err("Firmware list is empty\n");
	}

	list_for_each_entry(dsp_fw, &dsp->fw_list, list) {
		if (memcmp(dsp_fw->name, name, strlen(name)) == 0) {
			pfw = dsp_fw;
			break;
		}
	}
	spin_unlock_irqrestore(&dsp->fw_spinlock, flags);
	return pfw;
}

int hifi4dsp_dump_memory(const void *buf, unsigned int bytes, int col)
{
	int i = 0, n = 0, size = 0;
	const u8 *pdata;
	char str[256];
	char a_str[24];

	pdata = (u8 *)buf;
	size = bytes;
	memset(a_str, '\0', sizeof(a_str));
	memset(str, '\0', sizeof(str));
	while (n < size) {
		sprintf(a_str, "%p: ", pdata);
		strcat(str, a_str);
		col = ((size - n) > col) ? col : (size - n);
		for (i = 0; i < col; i++) {
			sprintf(a_str, "%02x ", *(pdata + i));
			strcat(str, a_str);
		}
		pr_info("%s\n", str);
		memset(str, '\0', sizeof(str));/*re-init buf*/
		pdata += col;
		n += col;
	}

	return 0;
}

//resource res;
int hifi4dsp_fw_copy_to_ddr(const struct firmware *fw,
			    struct hifi4dsp_firmware *dsp_fw)
{
	int fw_bytes = 0;
	const u8 *fw_src;
	void *fw_dst;

	fw_src = fw->data;
	fw_bytes = fw->size;
	fw_dst = dsp_fw->buf;
	hifi4dsp_dump_memory(fw_src, 32, 16);
	hifi4dsp_dump_memory(fw_src + fw_bytes - 32, 32, 16);
	pr_debug("%s fw_src:0x%p, pdata_dst=0x%p, szie=%d bytes\n",
		 __func__, fw_src, fw_dst, fw_bytes);
	//memcpy(fw_dst, fw_src, fw_bytes);
	memcpy_toio(fw_dst, fw_src, fw_bytes);
	//do memory barrier
	//mb();

	/*cache clean*/
	dma_sync_single_for_device(dsp_fw->dsp->dev,
				   dsp_fw->paddr,
				   dsp_fw->size,
				   DMA_TO_DEVICE);
	pr_info("\n after copy to ddr and clean cache:\n");
	hifi4dsp_dump_memory(dsp_fw->buf, 32, 16);
	hifi4dsp_dump_memory(dsp_fw->buf + dsp_fw->size - 32, 32, 16);

	return 0;
}

//resource res;
int hifi4dsp_fw_copy_to_sram(const struct firmware *fw,
			     struct hifi4dsp_firmware *dsp_fw)
{
	int fw_bytes = 0;
	const u8 *fw_src;
	void *fw_dst;

	fw_src = fw->data;
	fw_bytes = fw->size;
	fw_dst = g_regbases.sram_base;
	hifi4dsp_dump_memory(fw_src, 32, 16);
	hifi4dsp_dump_memory(fw_src + fw_bytes - 32, 32, 16);
	pr_debug("%s fw_src:0x%p, pdata_dst=0x%p, szie=%d bytes\n",
		 __func__, fw_src, fw_dst, fw_bytes);

	/*copy firmware to sram*/
	pr_info("\ncopy firmware from ddr to sram\n");
	memcpy_toio(g_regbases.sram_base, fw_src, fw_bytes);

	hifi4dsp_dump_memory(g_regbases.sram_base, 32, 16);
	hifi4dsp_dump_memory(g_regbases.sram_base
			     + fw_bytes - 32, 32, 16);
	return 0;
}

int hifi4dsp_fw_load(struct hifi4dsp_firmware *dsp_fw)
{
	const struct firmware *fw;
	struct hifi4dsp_dsp *dsp;
	int err = 0;

	pr_info("%s loading firmware %s\n", __func__, dsp_fw->name);

	if (!dsp_fw || !dsp_fw->dsp)
		return -1;

	dsp = dsp_fw->dsp;
	err = request_firmware(&fw, dsp_fw->name, dsp_fw->dsp->dev);
	if (err < 0) {
		HIFI4DSP_PRNT("can't load the %s,err=%d\n", dsp_fw->name, err);
		goto done;
	}
	if (!fw) {
		HIFI4DSP_PRNT("firmware pointer==NULL\n");
		goto done;
	}
	if (!dsp_fw) {
		HIFI4DSP_PRNT("hifi4dsp_firmware pointer==NULL\n");
		err = ENOMEM;
		goto release;
	}
	dsp_fw->size = fw->size;
	hifi4dsp_fw_copy_to_ddr(fw, dsp_fw);
release:
	release_firmware(fw);
done:
	return err;
}

int hifi4dsp_fw_sram_load(struct hifi4dsp_firmware *dsp_fw)
{
	const struct firmware *fw;
	struct hifi4dsp_dsp *dsp;
	int err = 0;

	pr_info("%s loading firmware %s\n", __func__, dsp_fw->name);

	if (!dsp_fw || !dsp_fw->dsp)
		return -1;

	dsp = dsp_fw->dsp;
	err = request_firmware(&fw, dsp_fw->name, dsp_fw->dsp->dev);
	if (err < 0) {
		HIFI4DSP_PRNT("can't load the %s,err=%d\n", dsp_fw->name, err);
		goto done;
	}
	if (!fw) {
		HIFI4DSP_PRNT("firmware pointer==NULL\n");
		goto done;
	}
	if (!dsp_fw) {
		HIFI4DSP_PRNT("hifi4dsp_firmware pointer==NULL\n");
		err = ENOMEM;
		goto release;
	}
	dsp_fw->size = fw->size;
	hifi4dsp_fw_copy_to_sram(fw, dsp_fw);
release:
	release_firmware(fw);
done:
	return err;
}

int hifi4dsp_fw_reload(struct hifi4dsp_dsp *dsp,
		       struct hifi4dsp_firmware *dsp_fw)
{
	int err = 0;

	return err;
}

int hifi4dsp_fw_unload(struct hifi4dsp_firmware *dsp_fw)
{
	int err = 0;

	return err;
}

/* create a hifi4dsp_firmware object and
 * add it to the fw_list of hifi4dsp_dsp
 */
struct hifi4dsp_firmware *hifi4dsp_fw_register(struct hifi4dsp_dsp *dsp,
					       char *name)
{
	struct hifi4dsp_firmware *dsp_fw;
	int str_len = 0;

	dsp_fw = hifi4dsp_fw_search_by_name(dsp, name);
	if (dsp_fw) {
		pr_info("%s firmware( %s ) has been registered\n",
			__func__, name);
		return dsp_fw;
	}
	dsp_fw = hifi4dsp_fw_new(dsp, NULL, NULL);
	if (dsp_fw) {
		str_len = sizeof(dsp_fw->name) - 1;
		strncpy(dsp_fw->name, name, str_len);
		hifi4dsp_fw_add(dsp_fw);
		pr_info("%s %s done\n", __func__, dsp_fw->name);
	}
	return dsp_fw;
}

/* free single firmware object */
void hifi4dsp_fw_free(struct hifi4dsp_firmware *dsp_fw)
{
	struct hifi4dsp_dsp *dsp = dsp_fw->dsp;

	mutex_lock(&dsp->mutex);
	list_del(&dsp_fw->list);
	kfree(dsp_fw);
	mutex_unlock(&dsp->mutex);
}

/* free all firmware objects */
void hifi4dsp_fw_free_all(struct hifi4dsp_dsp *dsp)
{
	struct hifi4dsp_firmware *dsp_fw, *t;

	mutex_lock(&dsp->mutex);
	list_for_each_entry_safe(dsp_fw, t, &dsp->fw_list, list) {
		list_del(&dsp_fw->list);
		kfree(dsp_fw);
	}
	mutex_unlock(&dsp->mutex);
}
