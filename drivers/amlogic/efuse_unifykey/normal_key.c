// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/slab.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/arm-smccc.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <crypto/hash.h>
#include <crypto/sha.h>
#include "normal_key.h"

#undef pr_fmt
#define pr_fmt(fmt) "unifykey: " fmt
#define DBG 0

/* Storage BLOCK RAW HEAD: fixed 512B*/
#define ENC_TYPE_DEFAULT 0
#define ENC_TYPE_EFUSE	1
#define ENC_TYPE_FIXED	2

#define STORAGE_BLOCK_RAW_HEAD_SIZE 512

#define BLOCK_VERSION_0		0

#define ERR_HEADER	0x1
#define ERR_KEYMEMFAIL	0x2
#define ERR_KEYRDFAIL	0x4
#define ERR_KEYCHKFAIL	0x8
#define ERR_ENCHDFAIL	0x10
#define ERR_DATASZ	0x20
struct storage_block_raw_head {
	u8 mark[16]; /* AMLNORMAL*/
	u32 version;
	u32 enctype; /*from EFUSE, from default, from fixed*/
	u32 keycnt;
	u32 initcnt;
	u32 wrtcnt;
	u32 errcnt;
	u32 flags;
	u8  headhash[32];
	u8  hash[32];
};

/* Storage BLOCK ENC HEAD: fixed 512B*/
#define STORAGE_BLOCK_ENC_HEAD_SIZE 512
struct storage_block_enc_head {
	u32 blocksize;
	u32 flashsize;
};

/* Storage Format: TLV*/
enum emTLVTag {
	EMTLVNONE,

	EMTLVHEAD,
	EMTLVHEADSIZE,

	EMTLVOBJECT,
	EMTLVOBJNAMESIZE,
	EMTLVOBJNAME,
	EMTLVOBJDATASIZE,
	EMTLVOBJDATABUF,
	EMTLVOBJTYPE,
	EMTLVOBJATTR,
	EMTLVOBJHASHBUF,

	EMTLVHEADFLASHSIZE,
};

struct storage_node {
	struct list_head node;
	struct storage_object object;
};

static LIST_HEAD(keys);
static int blockinited;
static struct storage_block_raw_head rawhead;
static struct storage_block_enc_head enchead;
static char *blockmark = "AMLNORMAL";

#if DBG
static void dump_raw_head(struct storage_block_raw_head *prawhead)
{
	pr_info("rawhead:\n");
	pr_info("mark: %s\n", prawhead->mark);
	pr_info("keycnt: %u\n", prawhead->keycnt);
	pr_info("initcnt: %u\n", prawhead->initcnt);
	pr_info("wrtcnt: %u\n", prawhead->wrtcnt);
	pr_info("errcnt: %u\n", prawhead->errcnt);
	pr_info("flags: 0x%x\n", prawhead->flags);
}

static void dump_enc_head(struct storage_block_enc_head *penchead)
{
	pr_info("enchead:\n");
	pr_info("blocksize: %u\n", penchead->blocksize);
	pr_info("flashsize: %u\n", penchead->flashsize);
}

static void dump_mem(const u8 *p, int len)
{
	int idx = 0, j, tmp;
	char buf[64];
	int total;

	while (idx < len) {
		total = 0;
		tmp = min(((int)len - idx), 16);
		for (j = 0; j < tmp; j++)
			total += snprintf(buf + total, 64 - total, "%02x ", p[idx + j]);
		buf[total] = 0;
		pr_info("%s\n", buf);
		idx += tmp;
	}
}

static void dump_object(struct storage_object *obj)
{
	pr_info("key: [%u, %.*s, %x, %x, %u]\n",
		obj->namesize, obj->namesize, obj->name,
		obj->type, obj->attribute, obj->datasize);
	if (obj->dataptr) {
		pr_info("data:\n");
		dump_mem(obj->dataptr, obj->datasize);
	}
}
#endif

static u32 Tlv_WriteUint32(u8 *output, s32 len,
			   u32 tag, u32 value)
{
	u32 *out = (u32 *)output;

	if (len < 12)
		return 0;

	out[0] = tag;
	out[1] = 4;
	out[2] = value;
	return 12;
}

static u32 Tlv_WriteBuf(u8 *output, s32 len,
			u32 tag,
			u32 length, u8 *input)
{
	u8 *out = output;
	u32 tmplen = (((length + 3) / 4) * 4);

	if (len < (s32)(8 + tmplen))
		return 0;

	*((u32 *)out) = tag;
	*((u32 *)(out + 4)) = tmplen;
	memset(out + 8, 0, tmplen);
	memcpy(out + 8, input, length);

	return tmplen + 8;
}

static u32 Tlv_ReadTl(u8 *input, int32_t len,
		      u32 *tag, u32 *length,
		      u32 *idx)
{
	if (len < 8)
		return 0;

	*tag = *((u32 *)input);
	*length =  *((u32 *)(input + 4));

	if ((8 + *length) > len)
		return 0;
	*idx += 8;

	return 8;
}

static u32 Tlv_ReadHead(u8 *input, int32_t len,
			struct storage_block_enc_head *pblockhead)
{
	u32 tag;
	u32 sum;
	u32 length;
	u32 idx = 0;
	u32 ret;

	ret = Tlv_ReadTl(input, len,
			 &tag, &sum, &idx);
	if (!ret)
		return 0;

	if (tag != EMTLVHEAD)
		return 0;

	sum += ret;
	while (idx < sum) {
		ret = Tlv_ReadTl(input + idx, len - idx,
				 &tag, &length, &idx);
		if (!ret)
			return 0;

		switch (tag) {
		case EMTLVHEADSIZE:
			pblockhead->blocksize = *((u32 *)(input + idx));
			break;
		case EMTLVHEADFLASHSIZE:
			pblockhead->flashsize = *((u32 *)(input + idx));
			break;
		default:
			break;
		}
		idx += length;
	}
	return sum;
}

static u32 Tlv_ReadObject(u8 *input, int32_t len,
			  struct storage_object *pcontent)
{
	u32 tag;
	u32 length;
	u32 sum;
	u32 idx = 0;
	u32 ret;

	memset(pcontent, 0, sizeof(*pcontent));
	ret = Tlv_ReadTl(input, len,
			 &tag, &sum, &idx);
	if (!ret)
		return 0;

	if (tag != EMTLVOBJECT)
		return 0;

	sum += ret;
	while (idx < sum) {
		ret = Tlv_ReadTl(input + idx, len - idx,
				 &tag, &length, &idx);
		if (!ret)
			goto tlv_readkeycontent_err;

		switch (tag) {
		case EMTLVOBJNAMESIZE:
			pcontent->namesize = *((u32 *)(input + idx));
			break;
		case EMTLVOBJNAME:
			memset(pcontent->name, 0, MAX_OBJ_NAME_LEN);
			memcpy(pcontent->name, input + idx, pcontent->namesize);
			break;
		case EMTLVOBJTYPE:
			pcontent->type = *((u32 *)(input + idx));
			break;
		case EMTLVOBJATTR:
			pcontent->attribute = *((u32 *)(input + idx));
			break;
		case EMTLVOBJDATASIZE:
			pcontent->datasize = *((u32 *)(input + idx));
			break;
		case EMTLVOBJHASHBUF:
			if (length != 32)
				goto tlv_readkeycontent_err;
			memcpy(pcontent->hashptr, input + idx, length);
			break;
		case EMTLVOBJDATABUF:
			pcontent->dataptr = kmalloc(pcontent->datasize,
						    GFP_KERNEL);
			if (!pcontent->dataptr)
				goto tlv_readkeycontent_err;
			memcpy(pcontent->dataptr,
			       input + idx, pcontent->datasize);
			break;
		default:
			break;
		}
		idx += length;
	}
	return sum;

tlv_readkeycontent_err:
	kfree(pcontent->dataptr);
	return 0;
}

#define WRT_UINT32(tag, field) \
	({ \
		u32 __tmp; \
		__tmp = Tlv_WriteUint32(output + idx, len - idx, \
					tag, field); \
		if (__tmp) \
			idx += __tmp; \
		__tmp; \
	})

#define WRT_BUF(tag, buflen, buf) \
	({ \
		u32 __tmp; \
		__tmp = Tlv_WriteBuf(output + idx, len - idx, \
				     tag, buflen, buf); \
		if (__tmp) \
			idx += __tmp; \
		__tmp; \
	})

u32 Tlv_WriteHead(struct storage_block_enc_head *enchead,
		  u8 *output, int32_t len)
{
	u32 *sum;
	u32 idx = 0;

	if (len < 8)
		return 0;

	*(u32 *)output = EMTLVHEAD;
	sum = (u32 *)(output + 4);
	idx += 8;

	if (!WRT_UINT32(EMTLVHEADSIZE, enchead->blocksize))
		return 0;
	if (!WRT_UINT32(EMTLVHEADFLASHSIZE, enchead->flashsize))
		return 0;

	*sum = idx - 8;
	return idx;
}

u32 Tlv_WriteObject(struct storage_object *object,
		    u8 *output, int32_t len)
{
	u32 *sum;
	u32 idx = 0;

	if (len < 8)
		return 0;

	*(u32 *)output = EMTLVOBJECT;
	sum = (u32 *)(output + 4);
	idx += 8;

	if (object->namesize != 0) {
		if (!WRT_UINT32(EMTLVOBJNAMESIZE, object->namesize))
			return 0;
		if (!WRT_BUF(EMTLVOBJNAME, object->namesize,
			     (u8 *)object->name))
			return 0;
	}

	if (object->dataptr && object->datasize != 0) {
		if (!WRT_UINT32(EMTLVOBJDATASIZE, object->datasize))
			return 0;
		if (!WRT_BUF(EMTLVOBJDATABUF, object->datasize,
			     object->dataptr))
			return 0;
	}

	if (!WRT_BUF(EMTLVOBJHASHBUF, 32, object->hashptr))
		return 0;
	if (!WRT_UINT32(EMTLVOBJTYPE, object->type))
		return 0;
	if (!WRT_UINT32(EMTLVOBJATTR, object->attribute))
		return 0;

	*sum = idx - 8;
	return idx;
}

static int normalkey_hash(u8 *data, u32 len, u8 *hash)
{
	struct crypto_shash *tfm;
	struct shash_desc *desc;
	int ret = -1;

	memset(hash, 0, SHA256_DIGEST_SIZE);
	tfm = crypto_alloc_shash("sha256", 0, 0);
	if (IS_ERR(tfm)) {
		pr_err("Failed to allocate hash\n");
		ret = -EINVAL;
		goto finish;
	}

	desc = kmalloc(sizeof(*desc) + crypto_shash_descsize(tfm), GFP_KERNEL);
	if (!desc) {
		ret = -ENOMEM;
		goto out_free_shash;
	}
	desc->tfm = tfm;

	ret = crypto_shash_digest(desc, data, len, hash);
	if (ret) {
		pr_err("Failed to digest, err = %d\n", ret);
		goto out_free_desc;
	}

out_free_desc:
	kfree(desc);
out_free_shash:
	crypto_free_shash(tfm);
finish:
	return ret;
}

int normalkey_init(void)
{
	if (blockinited)
		return -1;

	blockinited = 1;
	return 0;
}

void normalkey_deinit(void)
{
	struct storage_node *pos, *n;

	if (!blockinited)
		return;

	blockinited = 0;

	list_for_each_entry_safe(pos, n, &keys, node) {
		list_del(&pos->node);
		kfree(pos->object.dataptr);
		kfree(pos);
	}
}

struct storage_object *normalkey_get(const u8 *name)
{
	struct storage_node *pos;
	struct storage_object *obj;
	u32 len;

	if (!name)
		return NULL;

	len = strlen(name);
	list_for_each_entry(pos, &keys, node) {
		obj = &pos->object;
		if (len == obj->namesize &&
		    !memcmp(name, obj->name, len))
			return obj;
	}

	return NULL;
}

int normalkey_add(const u8 *name, u8 *buffer, u32 len, u32 attr)
{
	struct storage_object *obj;
	struct storage_node *node;
	u32 namelen;
	u8 *data;

	if (blockinited != 2)
		return -1;

	if (!name || !buffer || !len || (attr & OBJ_ATTR_SECURE))
		return -1;

	namelen = strlen(name);
	if (namelen > MAX_OBJ_NAME_LEN)
		return -1;

	obj = normalkey_get(name);
	if (obj) {
		if (attr != obj->attribute)
			return -1;
		if (len > obj->datasize) {
			data = kmalloc(len, GFP_KERNEL);
			if (!data)
				return -1;
			kfree(obj->dataptr);
			obj->dataptr = data;
		}
	} else {
		node = kmalloc(sizeof(*node), GFP_KERNEL);
		if (!node)
			return -1;
		data = kmalloc(len, GFP_KERNEL);
		if (!data) {
			kfree(node);
			return -1;
		}
		obj = &node->object;
		memcpy(obj->name, name, namelen);
		obj->namesize = namelen;
		obj->attribute = attr;
		obj->type = OBJ_TYPE_GENERIC;
		obj->dataptr = data;
		list_add(&node->node, &keys);
	}
	obj->datasize = len;
	memcpy(obj->dataptr, buffer, len);
	normalkey_hash(buffer, len, obj->hashptr);
	return 0;
}

int normalkey_del(const u8 *name)
{
	struct storage_object *obj;
	struct storage_node *node;

	if (blockinited != 2)
		return -1;

	obj = normalkey_get(name);
	if (!obj)
		return -1;

	node = container_of(obj, struct storage_node, object);
	list_del(&node->node);
	kfree(obj->dataptr);
	kfree(node);

	return 0;
}

int normalkey_readfromblock(void *block, unsigned long size)
{
	struct storage_block_raw_head *prawhead;
	u8 *penchead, *pdata;
	struct storage_node *node = NULL;
	u8 hash[32];
	u32 idx;
	u32 ret;

	if (blockinited != 1)
		return -1;

	prawhead = (struct storage_block_raw_head *)block;
	penchead = (u8 *)block + STORAGE_BLOCK_RAW_HEAD_SIZE;
	pdata = penchead + STORAGE_BLOCK_ENC_HEAD_SIZE;

	if (!block || size <=
	    (STORAGE_BLOCK_ENC_HEAD_SIZE + STORAGE_BLOCK_RAW_HEAD_SIZE))
		return -1;

	blockinited = 2;

	memset(&rawhead, 0, sizeof(rawhead));
	strncpy(rawhead.mark, blockmark, 15);
	rawhead.version = BLOCK_VERSION_0;

	enchead.flashsize = size;
	if (strcmp((const char *)prawhead->mark, blockmark) != 0) {
		pr_info("mark is not found\n");
		return 0;
	}

	normalkey_hash((u8 *)prawhead, sizeof(*prawhead) - 64,
		       rawhead.headhash);
	if (memcmp(rawhead.headhash, prawhead->headhash, 32)) {
		pr_info("rawhead hash check fail\n");
		rawhead.flags |= ERR_HEADER;
	} else {
		pr_info("rawhead hash check successful\n");
		rawhead.keycnt = prawhead->keycnt;
		rawhead.initcnt = prawhead->initcnt;
		rawhead.wrtcnt = prawhead->wrtcnt;
		rawhead.errcnt = prawhead->errcnt;
		rawhead.flags = prawhead->flags;
	}

	rawhead.initcnt++;

#if DBG
	dump_raw_head(&rawhead);
#endif

	normalkey_hash(penchead, size - STORAGE_BLOCK_RAW_HEAD_SIZE,
		       rawhead.hash);
	if (memcmp(rawhead.hash, prawhead->hash, 32)) {
		pr_info("data hash check fail\n");
		rawhead.errcnt++;
		return 0;
	}

	ret = Tlv_ReadHead(penchead, STORAGE_BLOCK_ENC_HEAD_SIZE,
			   &enchead);
	if (!ret) {
		pr_info("read head fail\n");
		rawhead.flags |= ERR_ENCHDFAIL;
		return 0;
	}

#if DBG
	dump_enc_head(&enchead);
#endif

	if (size < (enchead.blocksize + STORAGE_BLOCK_ENC_HEAD_SIZE +
	    STORAGE_BLOCK_RAW_HEAD_SIZE)) {
		rawhead.flags |= ERR_DATASZ;
		return 0;
	}

	idx = 0;
	while (idx < enchead.blocksize) {
		struct storage_object *obj = NULL;

		if (!node) {
			node = kmalloc(sizeof(*node), GFP_KERNEL);
			if (!node) {
				rawhead.flags |= ERR_KEYMEMFAIL;
				break;
			}
		}
		obj = &node->object;
		ret = Tlv_ReadObject(pdata + idx,
				     enchead.blocksize - idx, obj);
		if (!ret) {
			rawhead.flags |= ERR_KEYRDFAIL;
			break;
		}
		idx += ret;

		normalkey_hash(obj->dataptr, obj->datasize, hash);
		if (memcmp(hash, obj->hashptr, 32)) {
			kfree(obj->dataptr);
			rawhead.flags |= ERR_KEYCHKFAIL;
			continue;
		}
#if DBG
		dump_object(obj);
#endif
		list_add(&node->node, &keys);
		node = NULL;
	}

	kfree(node);
	return 0;
}

int normalkey_writetoblock(void *block, unsigned long size)
{
	u8 *prawhead;
	u8 *penchead, *pdata;
	struct storage_object *obj = NULL;
	struct storage_node *node = NULL;
	u32 idx;
	u32 ret;

	if (blockinited != 2)
		return -1;

	prawhead = (u8 *)block;
	penchead = prawhead + STORAGE_BLOCK_RAW_HEAD_SIZE;
	pdata = penchead + STORAGE_BLOCK_ENC_HEAD_SIZE;

	if (!block || size <=
	    (STORAGE_BLOCK_ENC_HEAD_SIZE + STORAGE_BLOCK_RAW_HEAD_SIZE))
		return -1;

	enchead.flashsize = size;
	size -= (STORAGE_BLOCK_ENC_HEAD_SIZE + STORAGE_BLOCK_RAW_HEAD_SIZE);
	idx = 0;
	rawhead.keycnt = 0;
	list_for_each_entry(node, &keys, node) {
		obj = &node->object;
		ret = Tlv_WriteObject(obj, pdata + idx, size - idx);
		if (!ret)
			return -1;
		idx += ret;
		rawhead.keycnt++;
	}
	enchead.blocksize = idx;

	ret = Tlv_WriteHead(&enchead, penchead, STORAGE_BLOCK_ENC_HEAD_SIZE);
	if (!ret)
		return -1;

	rawhead.wrtcnt++;
	normalkey_hash((u8 *)&rawhead, sizeof(rawhead) - 64,
		       rawhead.headhash);
	normalkey_hash(penchead, enchead.flashsize - STORAGE_BLOCK_RAW_HEAD_SIZE,
		       rawhead.hash);
	memcpy(prawhead, &rawhead, sizeof(rawhead));

	return 0;
}
