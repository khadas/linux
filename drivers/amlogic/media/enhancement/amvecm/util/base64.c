// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * drivers/amlogic/media/enhancement/amvecm/util/base64.c
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

#include "base64.h"

/* BASE 64 encode table */
static char base64en[] = {
	'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
	'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
	'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
	'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
	'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
	'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
	'w', 'x', 'y', 'z', '0', '1', '2', '3',
	'4', '5', '6', '7', '8', '9', '+', '/'
};

/* ASCII order for BASE 64 decode, 255 in unused character */
static unsigned char base64de[] = {
	/* nul, soh, stx, etx, eot, enq, ack, bel, */
	   255, 255, 255, 255, 255, 255, 255, 255,

	/*  bs,  ht,  nl,  vt,  np,  cr,  so,  si, */
	   255, 255, 255, 255, 255, 255, 255, 255,

	/* dle, dc1, dc2, dc3, dc4, nak, syn, etb, */
	   255, 255, 255, 255, 255, 255, 255, 255,

	/* can,  em, sub, esc,  fs,  gs,  rs,  us, */
	   255, 255, 255, 255, 255, 255, 255, 255,

	/*  sp, '!', '"', '#', '$', '%', '&', ''', */
	   255, 255, 255, 255, 255, 255, 255, 255,

	/* '(', ')', '*', '+', ',', '-', '.', '/', */
	   255, 255, 255,  62, 255, 255, 255,  63,

	/* '0', '1', '2', '3', '4', '5', '6', '7', */
		52,  53,  54,  55,  56,  57,  58,  59,

	/* '8', '9', ':', ';', '<', '=', '>', '?', */
		60,  61, 255, 255, 255, 255, 255, 255,

	/* '@', 'A', 'B', 'C', 'D', 'E', 'F', 'G', */
	   255,   0,   1,  2,   3,   4,   5,    6,

	/* 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', */
		 7,   8,   9,  10,  11,  12,  13,  14,

	/* 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', */
		15,  16,  17,  18,  19,  20,  21,  22,

	/* 'X', 'Y', 'Z', '[', '\', ']', '^', '_', */
		23,  24,  25, 255, 255, 255, 255, 255,

	/* '`', 'a', 'b', 'c', 'd', 'e', 'f', 'g', */
	   255,  26,  27,  28,  29,  30,  31,  32,

	/* 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', */
		33,  34,  35,  36,  37,  38,  39,  40,

	/* 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', */
		41,  42,  43,  44,  45,  46,  47,  48,

	/* 'x', 'y', 'z', '{', '|', '}', '~', del, */
		49,  50,  51, 255, 255, 255, 255, 255
};

unsigned long base64_encode(unsigned char *in, unsigned int inlen,
			    char *out)
{
	unsigned char *inptr = in;
	int counter = 0, i;
	int modval;

	for (i = 0; i < inlen; i += 3) {
		unsigned int combine3 = 0;

		combine3 |= inptr[i];
		combine3 |= (inptr[i + 1] << 8);
		combine3 |= (inptr[i + 2] << 16);
		out[counter] = base64en[combine3 & 0x3f];
		++counter;
		out[counter] = base64en[(combine3 >> 6) & 0x3f];
		++counter;
		out[counter] = base64en[(combine3 >> 12) & 0x3f];
		++counter;
		out[counter] = base64en[(combine3 >> 18) & 0x3f];
		++counter;
	}
	modval = inlen % 3;
	if (modval == 1) {
		unsigned int combine3 = 0;

		combine3 |= inptr[inlen - 1];
		out[counter] = base64en[combine3 & 0x3f];
		++counter;
		out[counter] = base64en[(combine3 >> 6) & 0x3f];
		++counter;
	}
	if (modval == 2) {
		unsigned int combine3 = 0;

		combine3 |= inptr[inlen - 2];
		combine3 |= (inptr[inlen - 1] << 8);
		out[counter] = base64en[combine3 & 0x3f];
		++counter;
		out[counter] = base64en[(combine3 >> 6) & 0x3f];
		++counter;
		out[counter] = base64en[(combine3 >> 12) & 0x3f];
		++counter;
	}

	return counter;
}

unsigned long base64_decode(char *in, unsigned int inlen,
			    unsigned char *out)
{
	char *inptr = in;
	int counter = 0, i;
	int modval;

	for (i = 0; i < inlen; i += 4) {
		unsigned int combine4 = 0;

		combine4 |=
		(unsigned int)base64de[(int)inptr[i]];
		combine4 |=
		((unsigned int)(base64de[(int)inptr[i + 1]]) << 6);
		combine4 |=
		((unsigned int)(base64de[(int)inptr[i + 2]]) << 12);
		combine4 |=
		((unsigned int)(base64de[(int)inptr[i + 3]]) << 18);

		out[counter] = (unsigned char)(combine4 & 0xff);
		++counter;
		out[counter] = (unsigned char)((combine4 >> 8) & 0xff);
		++counter;
		out[counter] = (unsigned char)((combine4 >> 16) & 0xff);
		++counter;
	}
	modval = inlen % 4;
	if (modval == 1) {
		unsigned int combine4 = 0;

		combine4 |= (unsigned int)base64de[(int)inptr[inlen - 1]];
		out[counter] = combine4 & 0xff;
		++counter;
	}
	if (modval == 2) {
		unsigned int combine4 = 0;

		combine4 |=
		(unsigned int)base64de[(int)inptr[inlen - 2]];
		combine4 |=
		((unsigned int)(base64de[(int)inptr[inlen - 1]]) << 6);
		out[counter] =
		(unsigned char)(combine4 & 0xff);
		++counter;
		out[counter] =
		(unsigned char)((combine4 >> 8) & 0xff);
		++counter;
	}
	if (modval == 3) {
		unsigned int combine4 = 0;

		combine4 |=
		(unsigned int)base64de[(int)inptr[inlen - 3]];
		combine4 |=
		((unsigned int)(base64de[(int)inptr[inlen - 2]]) << 6);
		combine4 |=
		((unsigned int)(base64de[(int)inptr[inlen - 1]]) << 12);

		out[counter] = (unsigned char)(combine4 & 0xff);
		++counter;
		out[counter] = (unsigned char)((combine4 >> 8) & 0xff);
		++counter;
		out[counter] = (unsigned char)((combine4 >> 16) & 0xff);
		++counter;
	}
	return counter;
}
