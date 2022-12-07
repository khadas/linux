// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * drivers/amlogic/media/enhancement/amvecm/util/enc_dec.c
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include "enc_dec.h"
#include "base64.h"
#include "huffman.h"

unsigned long huff64_encode(unsigned int *in, unsigned int inlen, char *out)
{
	unsigned int *inptr = in;
	unsigned char *testin;
	unsigned char *testout;
	unsigned char *huffbuf;
	unsigned long hufflen;
	char *enbase64array;
	unsigned long base64len;
	int counter, i;

	testin = kmalloc(inlen * 2, GFP_KERNEL);
	if (!testin)
		return 0;
	counter = 0;
	for (i = 0; i < inlen; i += 2) {
		unsigned int combine2 = inptr[i] | (inptr[i + 1] << 12);

		testin[counter] = combine2 & 0xff;
		++counter;
		testin[counter] = (combine2 >> 8) & 0xff;
		++counter;
		testin[counter] = (combine2 >> 16) & 0xff;
		++counter;
	}
	if (inlen % 2) {
		unsigned int combine2 = inptr[inlen - 1];

		testin[counter] = combine2 & 0xff;
		++counter;
		testin[counter] = (combine2 >> 8) & 0xff;
		++counter;
	}
	testout =  kmalloc(counter * 2, GFP_KERNEL);
	if (!testout) {
		kfree(testin);
		return 0;
	}
	huffbuf =  kmalloc(HUFFHEAP_SIZE, GFP_KERNEL);
	if (!huffbuf) {
		kfree(testin);
		kfree(testout);
		return 0;
	}
	hufflen = huffman_compress(testin, counter,
				   testout, counter * 2, huffbuf);
	enbase64array = kmalloc(hufflen * 2, GFP_KERNEL);
	if (!enbase64array) {
		kfree(huffbuf);
		kfree(testin);
		kfree(testout);
		return 0;
	}
	base64len = base64_encode(testout, hufflen, enbase64array);
	for (i = 0; i < base64len; i += 1)
		out[i] = enbase64array[i];
	kfree(testin);
	kfree(testout);
	kfree(huffbuf);
	kfree(enbase64array);
	return base64len;
}

unsigned long huff64_decode(char *in, unsigned int inlen,
			    unsigned int *out, unsigned int outlen)
{
	char *inptr = in;
	unsigned char *decodebase64array;
	unsigned long hufflen;
	unsigned char *testver;
	unsigned char *huffbuf;
	unsigned long dechufflen;
	unsigned int *testver12bit;
	int counter = 0, i;
	int modval;

	decodebase64array =  kmalloc(inlen * 2, GFP_KERNEL);
	if (!decodebase64array)
		return 0;

	hufflen = base64_decode(inptr, inlen, decodebase64array);
	testver = kmalloc(DECHUFF_MAXLEN, GFP_KERNEL);
	if (!testver) {
		kfree(decodebase64array);
		return 0;
	}
	huffbuf = kmalloc(HUFFHEAP_SIZE, GFP_KERNEL);
	if (!huffbuf) {
		kfree(decodebase64array);
		kfree(testver);
		return 0;
	}

	memset(testver, 0, DECHUFF_MAXLEN);
	dechufflen = huffman_decompress(decodebase64array, hufflen,
					testver, DECHUFF_MAXLEN, huffbuf);

	if (dechufflen == 0) {
		kfree(testver);
		kfree(huffbuf);
		kfree(decodebase64array);
		return 0;
	}

	testver12bit = kmalloc(dechufflen * 4, GFP_KERNEL);
	if (!testver12bit) {
		kfree(testver);
		kfree(huffbuf);
		kfree(decodebase64array);
		return 0;
	}
	counter = 0;
	for (i = 0; i < dechufflen; i += 3) {
		unsigned int combine3 = 0;

		combine3 |= testver[i];
		combine3 |= (testver[i + 1] << 8);
		combine3 |= (testver[i + 2] << 16);
		testver12bit[counter] = combine3 & 0xfff;
		++counter;
		testver12bit[counter] = (combine3 >> 12) & 0xfff;
		++counter;
	}
	modval = dechufflen % 3;
	if (modval == 1) {
		unsigned int combine3 = 0;

		combine3 |= testver[dechufflen - 1];
		testver12bit[counter] = combine3 & 0xfff;
		++counter;
	}
	if (modval == 2) {
		unsigned int combine3 = 0;

		combine3 |= testver[dechufflen - 2];
		combine3 |= (testver[dechufflen - 1] << 8);
		testver12bit[counter] = combine3 & 0xfff;
		++counter;
		testver12bit[counter] = (combine3 >> 12) & 0xfff;
		++counter;
	}
	for (i = 0; i < outlen; i += 1)
		out[i] = testver12bit[i];

	kfree(testver);
	kfree(huffbuf);
	kfree(decodebase64array);
	kfree(testver12bit);
	return counter;
}
