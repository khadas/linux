/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * drivers/amlogic/media/enhancement/amvecm/util/huffman.h
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

#ifndef ____HUFFMAN_H
#define ____HUFFMAN_H

/**
 * Required size of huffheap parameter to compress and decompress
 *
 * Note: if you change any of the data types in the _huffman_node
 * or _huffman_encode_table structs in huffman.c, this also must be
 * changed.
 */
#define HUFFHEAP_SIZE \
((sizeof(unsigned long) * 257) + \
(((sizeof(void *) * 4) + \
sizeof(unsigned long) + \
sizeof(unsigned long)) * (257 * 3)) + \
((sizeof(unsigned long) + \
sizeof(unsigned long)) * 257))

/**
 * Huffman encode a block of data
 *
 * @param in Input data
 * @param inlen Input data length
 * @param out Output buffer
 * @param outlen Output buffer length
 * @param huffheap Heap memory to use for compression
 * @return Size of encoded result or 0 on out buffer overrun
 */
unsigned long huffman_compress(const unsigned char *in, unsigned long inlen,
			       unsigned char *out,
			       unsigned long outlen, void *huffheap);

/**
 * Huffman decode a block of data
 *
 * @param in Input data
 * @param inlen Length of input data
 * @param out Output buffer
 * @param outlen Length of output buffer
 * @param huffheap Heap memory to use for decompression
 * @return Size of decoded result or 0
 */
unsigned long huffman_decompress(const unsigned char *in, unsigned long inlen,
				 unsigned char *out,
				 unsigned long outlen, void *huffheap);

#endif
