/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * drivers/amlogic/media/enhancement/amvecm/util/enc_dec.h
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

#ifndef ENC_DEC_H
#define ENC_DEC_H

#define SAFE_SIZE(s) ((unsigned long)((s) * 2))
#define DECHUFF_MAXLEN (4913 * 3 * 4)

unsigned long huff64_encode(unsigned int *in, unsigned int inlen,
			    char *out);

/*
 * return values is out length
 */
unsigned long huff64_decode(char *in, unsigned int inlen,
			    unsigned int *out, unsigned int outlen);

#endif /* ENC_DEC_H */
