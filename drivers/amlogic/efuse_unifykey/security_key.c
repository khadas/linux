// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/arm-smccc.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/kallsyms.h>
#include "security_key.h"

#undef pr_fmt
#define pr_fmt(fmt) "unifykey: " fmt

static DEFINE_SPINLOCK(storage_lock);
static int storage_init_status;
static struct bl31_storage_share_mem *share_mem;

static unsigned long storage_smc_ops(u64 func)
{
	struct arm_smccc_res res;

	arm_smccc_smc((unsigned long)func, 0, 0, 0, 0, 0, 0, 0, &res);
	return res.a0;
}

void *secure_storage_getbuf(u32 *size)
{
	if (storage_init_status == 1) {
		*size = share_mem->size;
		return (void *)(share_mem->block);
	}

	*size = 0;
	return NULL;
}

unsigned long secure_storage_write(u8 *keyname, u8 *keybuf,
				   u32 keylen, u32 keyattr)
{
	u32	      *input;
	u32	      namelen;
	u8	      *keydata;
	u8	      *name;
	unsigned long ret;
	unsigned long lockflags;

	if (storage_init_status != 1)
		return RET_EUND;

	namelen = strlen(keyname);
	if (namelen + keylen + 12 > share_mem->size)
		return RET_EMEM;

	spin_lock_irqsave(&storage_lock, lockflags);
	input = (u32 *)(share_mem->in);
	*input++ = namelen;
	*input++ = keylen;
	*input++ = keyattr;
	name = (u8 *)input;
	memcpy(name, keyname, namelen);
	keydata = name + namelen;
	memcpy(keydata, keybuf, keylen);
	ret = storage_smc_ops(BL31_STORAGE_WRITE);
	spin_unlock_irqrestore(&storage_lock, lockflags);

	return ret;
}

unsigned long secure_storage_read(u8 *keyname, u8 *keybuf,
				  u32 keylen, u32 *readlen)
{
	u32	      *input;
	u32	      *output;
	u32	      namelen;
	u8	      *name;
	u8	      *buf;
	unsigned long ret;
	unsigned long lockflags;

	if (storage_init_status != 1)
		return RET_EUND;

	namelen = strlen(keyname);
	if (namelen + 8 > share_mem->size)
		return RET_EMEM;

	spin_lock_irqsave(&storage_lock, lockflags);
	input = (u32 *)(share_mem->in);
	output = (u32 *)(share_mem->out);
	*input++ = namelen;
	*input++ = keylen;
	name = (u8 *)input;
	memcpy(name, keyname, namelen);
	ret = storage_smc_ops(BL31_STORAGE_READ);
	if (ret == RET_OK) {
		*readlen = *output;
		if (*readlen > keylen)
			*readlen = keylen;
		buf = (u8 *)(output + 1);
		memcpy(keybuf, buf, *readlen);
	}
	spin_unlock_irqrestore(&storage_lock, lockflags);

	return ret;
}

unsigned long secure_storage_verify(u8 *keyname, u8 *hashbuf)
{
	u32	      *input;
	u32	      *output;
	u32	      namelen;
	u8	      *name;
	unsigned long ret;
	unsigned long lockflags;

	if (storage_init_status != 1)
		return RET_EUND;

	namelen = strlen(keyname);
	if (namelen + 4 > share_mem->size)
		return RET_EMEM;

	spin_lock_irqsave(&storage_lock, lockflags);
	input = (u32 *)(share_mem->in);
	output = (u32 *)(share_mem->out);
	*input++ = namelen;
	name = (u8 *)input;
	memcpy(name, keyname, namelen);
	ret = storage_smc_ops(BL31_STORAGE_VERIFY);
	if (ret == RET_OK)
		memcpy(hashbuf, (u8 *)output, 32);
	spin_unlock_irqrestore(&storage_lock, lockflags);

	return ret;
}

unsigned long secure_storage_query(u8 *key_name, u32 *retval)
{
	u32	      *input;
	u32	      *output;
	u32	      namelen;
	u8	      *name;
	unsigned long ret;
	unsigned long lockflags;

	if (storage_init_status != 1)
		return RET_EUND;

	namelen = strlen(key_name);
	if (namelen + 4 > share_mem->size)
		return RET_EMEM;

	spin_lock_irqsave(&storage_lock, lockflags);
	input = (u32 *)(share_mem->in);
	output = (u32 *)(share_mem->out);
	*input++ = namelen;
	name = (u8 *)input;
	memcpy(name, key_name, namelen);
	ret = storage_smc_ops(BL31_STORAGE_QUERY);
	if (ret == RET_OK)
		*retval = *output;
	spin_unlock_irqrestore(&storage_lock, lockflags);

	return ret;
}

unsigned long secure_storage_tell(u8 *keyname, u32 *retval)
{
	u32	      *input;
	u32	      *output;
	u32	      namelen;
	u8	      *name;
	unsigned long ret;
	unsigned long lockflags;

	if (storage_init_status != 1)
		return RET_EUND;

	namelen = strlen(keyname);
	if (namelen + 4 > share_mem->size)
		return RET_EMEM;

	spin_lock_irqsave(&storage_lock, lockflags);
	input = (u32 *)(share_mem->in);
	output = (u32 *)(share_mem->out);
	*input++ = namelen;
	name = (u8 *)input;
	memcpy(name, keyname, namelen);
	ret = storage_smc_ops(BL31_STORAGE_TELL);
	if (ret == RET_OK)
		*retval = *output;
	spin_unlock_irqrestore(&storage_lock, lockflags);

	return ret;
}

unsigned long secure_storage_status(u8 *keyname, u32 *retval)
{
	u32	      *input;
	u32	      *output;
	u32	      namelen;
	u8	      *name;
	u64	      ret;
	unsigned long lockflags;

	if (storage_init_status != 1)
		return RET_EUND;

	namelen = strlen(keyname);
	if (namelen + 4 > share_mem->size)
		return RET_EMEM;

	spin_lock_irqsave(&storage_lock, lockflags);
	input = (u32 *)(share_mem->in);
	output = (u32 *)(share_mem->out);
	*input++ = namelen;
	name = (u8 *)input;
	memcpy(name, keyname, namelen);
	ret = storage_smc_ops(BL31_STORAGE_STATUS);
	if (ret == RET_OK)
		*retval = *output;
	spin_unlock_irqrestore(&storage_lock, lockflags);

	return ret;
}

int __init security_key_init(struct platform_device *pdev)
{
	unsigned long phy_in;
	unsigned long phy_out;
	unsigned long phy_block;

	storage_init_status = -1;

	share_mem = devm_kzalloc(&pdev->dev, sizeof(*share_mem), GFP_KERNEL);
	if (IS_ERR_OR_NULL(share_mem)) {
		pr_err("fail to alloc mem for struct share_mem\n");
		return -ENOMEM;
	}

	phy_in = storage_smc_ops(BL31_STORAGE_IN);
	phy_out = storage_smc_ops(BL31_STORAGE_OUT);
	phy_block = storage_smc_ops(BL31_STORAGE_BLOCK);
	share_mem->size = storage_smc_ops(BL31_STORAGE_SIZE);

	if (!phy_in || (int)phy_in == SMC_UNK ||
	    !phy_out || (int)phy_out == SMC_UNK ||
	    !phy_block || (int)phy_block == SMC_UNK ||
	    !(share_mem->size) || ((int)share_mem->size) == SMC_UNK) {
		pr_err("fail to obtain phy addr of shared mem\n");
		return -EOPNOTSUPP;
	}

	/*
	 * The BL31 shared mem for key storage is
	 * part of secure monitor, and all of the
	 * mem locates at lowmem region, so its
	 * okay to call phys_to_virt directly
	 */
	if (pfn_valid(__phys_to_pfn(phy_in))) {
		share_mem->in = (void __iomem *)phys_to_virt(phy_in);
		share_mem->out = (void __iomem *)phys_to_virt(phy_out);
		share_mem->block = (void __iomem *)phys_to_virt(phy_block);
	}	else {
		share_mem->in = ioremap_cache(phy_in, share_mem->size);
		share_mem->out = ioremap_cache(phy_out, share_mem->size);
		share_mem->block = ioremap_cache(phy_block, share_mem->size);
	}

	pr_info("storage in: 0x%lx, out: 0x%lx, block: 0x%lx\n",
		(long)(share_mem->in), (long)(share_mem->out),
		(long)(share_mem->block));

	if (!(share_mem->in) || !(share_mem->out) || !(share_mem->block)) {
		pr_err("fail to obtain virtual addr of shared mem\n");
		return -EINVAL;
	}

	storage_init_status = 1;
	pr_info("security_key init done!\n");
	return 0;
}
