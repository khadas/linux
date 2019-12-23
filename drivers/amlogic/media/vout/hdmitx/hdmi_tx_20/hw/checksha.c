// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/types.h>
#include <linux/printk.h>
#include <linux/string.h>
#include "checksha.h"

static void shamsg_reset(struct shamsg_t *msg)
{
	int i = 0;

	msg->shamsg_idx = 0;
	msg->shamsg_cmp = FALSE;
	msg->shamsg_crp = FALSE;
	for (i = 0; i < sizeof(msg->shamsg_len); i++)
		msg->shamsg_len[i] = 0;

	msg->shamsg_dgt[0] = SHA_CONST_VAL0;
	msg->shamsg_dgt[1] = SHA_CONST_VAL1;
	msg->shamsg_dgt[2] = SHA_CONST_VAL2;
	msg->shamsg_dgt[3] = SHA_CONST_VAL3;
	msg->shamsg_dgt[4] = SHA_CONST_VAL4;
}

static unsigned int SHAMSG_SHIFT(unsigned int bit, unsigned int val)
{
	return ((val << bit) & 0xFFFFFFFF) | (val >> (32 - bit));
}

static void shamsg_calcblock(struct shamsg_t *msg)
{
	const unsigned int K[] = {
		SHA_CONST_K0,
		SHA_CONST_K1,
		SHA_CONST_K2,
		SHA_CONST_K3
	};
	unsigned int W[80];
	unsigned int A, B, C, D, E;
	unsigned int temp = 0;
	int t = 0;

#define SHIFT_DAT(idx, off) \
	(((unsigned int)msg->shamsg_blk[t * 4 + (idx)]) << (off))

	for (t = 0; t < 80; t++) {
		if (t < 16) {
			W[t] = SHIFT_DAT(0, 24);
			W[t] |= SHIFT_DAT(1, 16);
			W[t] |= SHIFT_DAT(2, 8);
			W[t] |= SHIFT_DAT(3, 0);
		} else {
			temp = W[t - 3] ^ W[t - 8] ^ W[t - 14] ^ W[t - 16];
			W[t] = SHAMSG_SHIFT(1, temp);
		}
	}

	A = msg->shamsg_dgt[0];
	B = msg->shamsg_dgt[1];
	C = msg->shamsg_dgt[2];
	D = msg->shamsg_dgt[3];
	E = msg->shamsg_dgt[4];

	for (t = 0; t < 80; t++) {
		temp = SHAMSG_SHIFT(5, A);
		if (t < 20)
			temp += ((B & C) | ((~B) & D)) + E + W[t] + K[0];
		else if (t < 40)
			temp += (B ^ C ^ D) + E + W[t] + K[1];
		else if (t < 60)
			temp += ((B & C) | (B & D) | (C & D)) + E + W[t] + K[2];
		else
			temp += (B ^ C ^ D) + E + W[t] + K[3];
		E = D;
		D = C;
		C = SHAMSG_SHIFT(30, B);
		B = A;
		A = (temp & MAX_VAL_MASK);
	}

	msg->shamsg_dgt[0] = (msg->shamsg_dgt[0] + A) & MAX_VAL_MASK;
	msg->shamsg_dgt[1] = (msg->shamsg_dgt[1] + B) & MAX_VAL_MASK;
	msg->shamsg_dgt[2] = (msg->shamsg_dgt[2] + C) & MAX_VAL_MASK;
	msg->shamsg_dgt[3] = (msg->shamsg_dgt[3] + D) & MAX_VAL_MASK;
	msg->shamsg_dgt[4] = (msg->shamsg_dgt[4] + E) & MAX_VAL_MASK;

	msg->shamsg_idx = 0;
}

static void shamsg_init(struct shamsg_t *msg,
			const unsigned char *data,
			int size)
{
	int i = 0;
	unsigned int j = 0;
	int rc = TRUE;

	if (!data || size == 0) {
		pr_info("invalid parameters\n");
		return;
	}
	if (msg->shamsg_cmp == 1 || msg->shamsg_crp == 1) {
		msg->shamsg_crp = TRUE;
		return;
	}
	while (size-- && msg->shamsg_crp == 0) {
		msg->shamsg_blk[msg->shamsg_idx++] = *data;

		for (i = 0; i < 8; i++) {
			rc = TRUE;
			for (j = 0; j < sizeof(msg->shamsg_len); j++) {
				msg->shamsg_len[j]++;
				if (msg->shamsg_len[j] != 0) {
					rc = FALSE;
					break;
				}
			}
			msg->shamsg_crp = (msg->shamsg_crp == 1 ||
				rc == 1) ? TRUE : FALSE;
		}
		if (msg->shamsg_idx == 64)
			shamsg_calcblock(msg);
		data++;
	}
}

static void shamsg_padmsg(struct shamsg_t *msg)
{
	if (msg->shamsg_idx > 55) {
		msg->shamsg_blk[msg->shamsg_idx++] = 0x80;
		while (msg->shamsg_idx < 64)
			msg->shamsg_blk[msg->shamsg_idx++] = 0;
		shamsg_calcblock(msg);
		while (msg->shamsg_idx < 56)
			msg->shamsg_blk[msg->shamsg_idx++] = 0;
	} else {
		msg->shamsg_blk[msg->shamsg_idx++] = 0x80;
		while (msg->shamsg_idx < 56)
			msg->shamsg_blk[msg->shamsg_idx++] = 0;
	}

	msg->shamsg_blk[56] = msg->shamsg_len[7];
	msg->shamsg_blk[57] = msg->shamsg_len[6];
	msg->shamsg_blk[58] = msg->shamsg_len[5];
	msg->shamsg_blk[59] = msg->shamsg_len[4];
	msg->shamsg_blk[60] = msg->shamsg_len[3];
	msg->shamsg_blk[61] = msg->shamsg_len[2];
	msg->shamsg_blk[62] = msg->shamsg_len[1];
	msg->shamsg_blk[63] = msg->shamsg_len[0];

	shamsg_calcblock(msg);
}

static int get_shamsg_result(struct shamsg_t *msg)
{
	if (msg->shamsg_crp == 1)
		return FALSE;
	if (msg->shamsg_crp == 0) {
		shamsg_padmsg(msg);
		msg->shamsg_crp = TRUE;
	}
	return TRUE;
}

int calc_hdcp_ksv_valid(const unsigned char *data, int size)
{
	int i = 0;
	struct shamsg_t shamsg;

	memset(&shamsg, 0, sizeof(struct shamsg_t));

	if (!data || size < (HDCP_HEAD + SHA_MAX_SIZE)) {
		pr_info("invalid parameters\n");
		return FALSE;
	}

	shamsg_reset(&shamsg);
	shamsg_init(&shamsg, data, size - SHA_MAX_SIZE);
	if (get_shamsg_result(&shamsg) == FALSE) {
		pr_info("hdcp invalid ksv/sha message\n");
		return FALSE;
	}
	for (i = 0; i < SHA_MAX_SIZE; i++) {
		if (data[size - SHA_MAX_SIZE + i] !=
			(uint8_t)(shamsg.shamsg_dgt[i / 4]
			>> ((i % 4) * 8))) {
			pr_info("hdcp ksv/sha not match\n");
			return FALSE;
		}
	}
	return TRUE;
}
