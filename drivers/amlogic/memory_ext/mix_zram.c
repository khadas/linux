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
#include <linux/swap.h>
#include <crypto/internal/scompress.h>

#define MIX_RATIO_CNT		10

#define LZ4		1
#define ZSTD		2

/*
 * range: 0 ~ 10, lower more zstd, higher more lz4
 * default 50% zstd and 50% lz4
 */
static int mixed_ratio = 5;
static const char mix_type[MIX_RATIO_CNT + 1][MIX_RATIO_CNT] = {
	{ZSTD, ZSTD, ZSTD, ZSTD, ZSTD,
	 ZSTD, ZSTD, ZSTD, ZSTD, ZSTD}, /* 0 full of ZSTD */
	{ZSTD, ZSTD, ZSTD, ZSTD, LZ4,
	 ZSTD, ZSTD, ZSTD, ZSTD, ZSTD}, /* 1, 10% lz4, 90% ZSTD */
	{ZSTD, ZSTD, ZSTD, LZ4,  ZSTD,
	 ZSTD, LZ4,  ZSTD, ZSTD, ZSTD}, /* 2, 20% lz4, 80% ZSTD */
	{ZSTD, ZSTD, LZ4,  ZSTD, ZSTD,
	 LZ4,  ZSTD, ZSTD, LZ4,  ZSTD}, /* 3, 30% lz4, 70% ZSTD */
	{ZSTD, LZ4,  ZSTD, LZ4,  ZSTD,
	 LZ4,  ZSTD, ZSTD, LZ4,  ZSTD}, /* 4, 40% lz4, 60% ZSTD */
	{ZSTD, LZ4,  ZSTD, LZ4,  ZSTD,
	 LZ4,  ZSTD, LZ4,  ZSTD, LZ4 }, /* 5, 50% lz4, 50% ZSTD */
	{LZ4,  ZSTD, LZ4,  ZSTD, LZ4,
	 ZSTD, LZ4,  LZ4,  ZSTD, LZ4 }, /* 6, 60% lz4, 40% ZSTD */
	{LZ4,  LZ4,  ZSTD, LZ4,  LZ4,
	 ZSTD, LZ4,  LZ4,  ZSTD, LZ4 }, /* 7, 70% lz4, 30% ZSTD */
	{LZ4,  LZ4,  LZ4,  ZSTD, LZ4,
	 LZ4,  LZ4,  ZSTD, LZ4,  LZ4 }, /* 8, 80% lz4, 20% ZSTD */
	{LZ4,  LZ4,  LZ4,  LZ4,  ZSTD,
	 LZ4,  LZ4,  LZ4,  LZ4,  LZ4 }, /* 9, 90% lz4, 10% ZSTD */
	{LZ4,  LZ4,  LZ4,  LZ4,  LZ4,
	 LZ4,  LZ4,  LZ4,  LZ4,  LZ4 }, /* 10 full of LZ4 */
};

static int type_flag;
static DEFINE_SPINLOCK(zclock);
static DEFINE_SPINLOCK(zdlock);
struct mix_ctx {
	struct crypto_comp *lz4;
};

#define ZSTD_DEF_LEVEL	1

struct zstd_ctx {
	ZSTD_CCtx *cctx;
	ZSTD_DCtx *dctx;
	void *cwksp;
	void *dwksp;
};

static struct zstd_ctx direct_zstd, kswap_zstd;

static ZSTD_parameters zstd_params(void)
{
	return ZSTD_getParams(ZSTD_DEF_LEVEL, 0, 0);
}

static int zstd_comp_init(struct zstd_ctx *ctx)
{
	int ret = 0;
	const ZSTD_parameters params = zstd_params();
	const size_t wksp_size = zstd_cctx_workspace_bound(&params.cParams);

	ctx->cwksp = vzalloc(wksp_size);
	pr_emerg("%s, size:%d\n", __func__, (int)wksp_size);
	if (!ctx->cwksp) {
		ret = -ENOMEM;
		goto out;
	}

	ctx->cctx = zstd_init_cctx(ctx->cwksp, wksp_size);
	if (!ctx->cctx) {
		ret = -EINVAL;
		goto out_free;
	}
out:
	return ret;
out_free:
	vfree(ctx->cwksp);
	goto out;
}

static int zstd_decomp_init(struct zstd_ctx *ctx)
{
	int ret = 0;
	const size_t wksp_size = zstd_dctx_workspace_bound();

	ctx->dwksp = vzalloc(wksp_size);
	pr_emerg("%s, size:%d\n", __func__, (int)wksp_size);
	if (!ctx->dwksp) {
		ret = -ENOMEM;
		goto out;
	}

	ctx->dctx = zstd_init_dctx(ctx->dwksp, wksp_size);
	if (!ctx->dctx) {
		ret = -EINVAL;
		goto out_free;
	}
out:
	return ret;
out_free:
	vfree(ctx->dwksp);
	goto out;
}

static void zstd_comp_exit(struct zstd_ctx *ctx)
{
	vfree(ctx->cwksp);
	ctx->cwksp = NULL;
	ctx->cctx = NULL;
}

static void zstd_decomp_exit(struct zstd_ctx *ctx)
{
	vfree(ctx->dwksp);
	ctx->dwksp = NULL;
	ctx->dctx = NULL;
}

static int __zstd_init(void *ctx)
{
	int ret;

	ret = zstd_comp_init(ctx);
	if (ret)
		return ret;
	ret = zstd_decomp_init(ctx);
	if (ret)
		zstd_comp_exit(ctx);
	return ret;
}

static void __zstd_exit(void *ctx)
{
	zstd_comp_exit(ctx);
	zstd_decomp_exit(ctx);
}

static int __zstd_compress(const u8 *src, unsigned int slen,
			   u8 *dst, unsigned int *dlen, void *ctx)
{
	size_t out_len;
	struct zstd_ctx *zctx = ctx;
	const ZSTD_parameters params = zstd_params();

	out_len = zstd_compress_cctx(zctx->cctx, dst, *dlen, src, slen, &params);
	if (ZSTD_isError(out_len))
		return -EINVAL;
	*dlen = out_len;
	return 0;
}

static int __zstd_decompress(const u8 *src, unsigned int slen,
			     u8 *dst, unsigned int *dlen, void *ctx)
{
	size_t out_len;
	struct zstd_ctx *zctx = ctx;

	out_len = zstd_decompress_dctx(zctx->dctx, dst, *dlen, src, slen);
	if (ZSTD_isError(out_len))
		return -EINVAL;
	*dlen = out_len;
	return 0;
}


static int mix_comp_init(struct crypto_tfm *tfm)
{
	struct mix_ctx *ctx = crypto_tfm_ctx(tfm);
	int ret = 0;

	if (!ctx)
		return -EINVAL;

	ctx->lz4 = crypto_alloc_comp("lz4", 0, 0);
	if (IS_ERR_OR_NULL(ctx->lz4)) {
		pr_err("create lz4 or zstd crypto failed:%px\n",
			 ctx->lz4);
		return -ENOMEM;
	}
	if (!direct_zstd.cctx) {
		ret = __zstd_init(&direct_zstd);
		if (ret)
			goto nomem;
	}
	if (!kswap_zstd.cctx) {
		ret = __zstd_init(&kswap_zstd);
		if (ret)
			goto nomem1;
	}

	return 0;
nomem1:
	__zstd_exit(&direct_zstd);
nomem:
	crypto_free_comp(ctx->lz4);
	return -ENOMEM;
}

static void mix_comp_exit(struct crypto_tfm *tfm)
{
	struct mix_ctx *ctx = crypto_tfm_ctx(tfm);

	if (!ctx)
		return;
	if (ctx->lz4)
		crypto_free_comp(ctx->lz4);
}

static int mix_compress(struct crypto_tfm *t, const u8 *src, unsigned int slen,
			u8 *dst, unsigned int *dlen)
{
	int out_len;
	unsigned int type, dst_capcity;
	struct mix_ctx *ctx = crypto_tfm_ctx(t);

	// keep in range
	if (unlikely(mixed_ratio < 0 || mixed_ratio > MIX_RATIO_CNT))
		mixed_ratio = 5;

	type = mix_type[mixed_ratio][type_flag];
	type_flag++;
	if (type_flag >= MIX_RATIO_CNT)
		type_flag = 0;

	dst_capcity = *dlen;
	if (type == LZ4) {
		out_len = crypto_comp_compress(ctx->lz4, src, slen, dst, dlen);
	} else {
		type = ZSTD;
		if (current_is_kswapd()) {
			out_len = __zstd_compress(src, slen, dst, dlen, &kswap_zstd);
		} else {
			spin_lock(&zclock);
			out_len = __zstd_compress(src, slen, dst, dlen, &direct_zstd);
			spin_unlock(&zclock);
		}
	}
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

	/* get type */
	type = src[slen - 1];
	pr_debug("%s, src:%px dst:%px, slen:%d, dlen:%d, type:%d",
		__func__, src, dst, slen, *dlen, type);
	if (type == LZ4) {
		out_len = crypto_comp_decompress(ctx->lz4, src,
						 slen - 1, dst, dlen);
	} else if (type == ZSTD) {
		if (current_is_kswapd()) {
			out_len = __zstd_decompress(src, slen - 1, dst, dlen, &kswap_zstd);
		} else {
			spin_lock(&zdlock);
			out_len = __zstd_decompress(src, slen - 1, dst, dlen, &direct_zstd);
			spin_unlock(&zdlock);
		}
	} else {
		pr_err("%s, unsupported type:%d, src:%px, slen:%d\n",
			__func__, type, src, slen);
		return -EINVAL;
	}

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
	if (direct_zstd.cctx) {
		__zstd_exit(&direct_zstd);
		direct_zstd.cctx = NULL;
	}
	if (kswap_zstd.cctx) {
		__zstd_exit(&kswap_zstd);
		kswap_zstd.cctx = NULL;
	}
}

module_init(mix_mod_init);
module_exit(mix_mod_exit);
module_param_named(mixed_ratio, mixed_ratio, int, 0644);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Mixed Compression Algorithm");
