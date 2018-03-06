/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __AES_HWKEY_GCM_H__
#define __AES_HWKEY_GCM_H__

#define HWKEY_IV_LEN  12
#define HWKEY_TAG_LEN 16

struct aes_gcm_ctx {
	int init;
	unsigned int idx;
	int do_encrypt;
};

/*
 * The API below assumes 12 byte IV and 16 byte tag (the lengths aren't passed)
 * This API should not do any padding (that is up to the caller) and it is fine
 * if the process function fails if the argument length is not a multiple of
 * the block size.
 */

/**
 * final argument specifies whether this is encrypt or decrypt
 * encrypt passes null for tag
 */
int aes_hwkey_gcm_init(struct aes_gcm_ctx *ctx,
		const u8 *iv,
		const u8 *aad, size_t aad_len,
		const u8 *tag,
		int do_encrypt);

/**
 * encrypts or decrypts (depending on init)
 * len must be block size multiple
 * we don't really want/need this to be multi-call interface
 * fails if this is decrypt and tag from init does not match
 */
int aes_hwkey_gcm_process(struct aes_gcm_ctx *ctx,
		const u8 *in, u8 *out, size_t len);

/**
 * encrypt calls this one at the end to receive the generated tag
 */
int aes_hwkey_gcm_get_tag(struct aes_gcm_ctx *ctx, u8 *tag);

#endif //__AES_HWKEY_GCM_H__
