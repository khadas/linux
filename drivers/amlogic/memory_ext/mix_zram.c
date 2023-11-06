// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <linux/crypto.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/net.h>
#include <linux/vmalloc.h>
#include <linux/zstd.h>
#include <linux/lz4.h>
#include <linux/crypto.h>
#include <crypto/internal/scompress.h>

#define TYPE_LZ4		1
#define TYPE_ZSTD		2

/*
 * range: 0 ~ 10, lower more zstd, higher more lz4
 * default 50% zstd and 50% lz4
 */
static int mixed_ratio = 5;
static int type_flag;
struct mix_ctx {
	struct crypto_comp *zstd;
	struct crypto_comp *lz4;
};

static int mix_comp_init(struct crypto_tfm *tfm)
{
	struct mix_ctx *ctx = crypto_tfm_ctx(tfm);

	if (!ctx)
		return -EINVAL;

	ctx->zstd = crypto_alloc_comp("zstd", 0, 0);
	ctx->lz4 = crypto_alloc_comp("lz4", 0, 0);
	if (IS_ERR_OR_NULL(ctx->zstd) || IS_ERR_OR_NULL(ctx->lz4)) {
		pr_err("create lz4 or zstd crypto failed:%px %px\n",
			 ctx->zstd, ctx->lz4);
		return -ENOMEM;
	}
	return 0;
}

static void mix_comp_exit(struct crypto_tfm *tfm)
{
	struct mix_ctx *ctx = crypto_tfm_ctx(tfm);

	if (!ctx)
		return;
	if (ctx->zstd)
		crypto_free_comp(ctx->zstd);
	if (ctx->lz4)
		crypto_free_comp(ctx->lz4);
}

static int mix_compress(struct crypto_tfm *t, const u8 *src, unsigned int slen,
			u8 *dst, unsigned int *dlen)
{
	int out_len;
	unsigned int type, dst_capcity;
	struct mix_ctx *ctx = crypto_tfm_ctx(t);
	struct crypto_comp *tfm = NULL;

	// keep in range
	if (unlikely(mixed_ratio < 0 || mixed_ratio > 10))
		mixed_ratio = 5;

	if (type_flag  < mixed_ratio) {
		type = TYPE_LZ4;
		tfm = ctx->lz4;
	} else {
		type = TYPE_ZSTD;
		tfm = ctx->zstd;
	}
	type_flag++;
	if (type_flag >= 10)
		type_flag = 0;

	dst_capcity = *dlen;
	out_len = crypto_comp_compress(tfm, src, slen, dst, dlen);
	if (out_len < 0 || *dlen >= dst_capcity) {
		pr_err("%s, out_len:%d, type:%d, dlen:%d:%d\n",
			__func__, out_len, type, *dlen, dst_capcity);
		return -EINVAL;
	}
	/* record type in last byte and increase length */
	pr_debug("%s, src:%px dst:%px, slen:%d, dlen:%d, cap:%d type:%d",
		__func__, src, dst, slen, *dlen, dst_capcity, type);
	dst[*dlen] = type;
	*dlen = *dlen + 1;

	return 0;
}

static int mix_decompress(struct crypto_tfm *t, const u8 *src,
			  unsigned int slen, u8 *dst, unsigned int *dlen)
{
	int out_len;
	unsigned int type;
	struct mix_ctx *ctx = crypto_tfm_ctx(t);
	struct crypto_comp *tfm = NULL;

	/* get type */
	type = src[slen - 1];
	pr_debug("%s, src:%px dst:%px, slen:%d, dlen:%d, type:%d",
		__func__, src, dst, slen, *dlen, type);
	if (type == TYPE_LZ4) {
		tfm = ctx->lz4;
	} else if (type == TYPE_ZSTD) {
		tfm = ctx->zstd;
	} else {
		pr_err("%s, unsupported type:%d, src:%px, slen:%d\n",
			__func__, type, src, slen);
		return -EINVAL;
	}

	out_len = crypto_comp_decompress(tfm, src, slen - 1, dst, dlen);
	if (out_len < 0) {
		pr_err("%s, out_len:%d, type:%d, dlen:%d\n",
			__func__, out_len, type, *dlen);
		return -EINVAL;
	}
	return 0;
}

static struct crypto_alg mix_alg = {
	.cra_name		= "mix",
	.cra_driver_name	= "mix-generic",
	.cra_flags		= CRYPTO_ALG_TYPE_COMPRESS,
	.cra_ctxsize		= sizeof(struct mix_ctx),
	.cra_module		= THIS_MODULE,
	.cra_init		= mix_comp_init,
	.cra_exit		= mix_comp_exit,
	.cra_u			= { .compress = {
	.coa_compress		= mix_compress,
	.coa_decompress		= mix_decompress} }
};

static int __init mix_mod_init(void)
{
	int ret;

	ret = crypto_register_alg(&mix_alg);
	pr_info("%s, ret:%d\n", __func__, ret);
	if (ret)
		return ret;

	return ret;
}

static void __exit mix_mod_exit(void)
{
	crypto_unregister_alg(&mix_alg);
}

module_init(mix_mod_init);
module_exit(mix_mod_exit);
module_param_named(mixed_ratio, mixed_ratio, int, 0644);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Mixed Compression Algorithm");
