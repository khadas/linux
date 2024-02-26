// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 Rockchip Electronics Co. Ltd.
 *
 * Author: Zorro Liu <zorro.liu@rock-chips.com>
 */

	.text
	.file	"rkf_waveform.c"
	.file	1 "/home/lyx/rk-android14/kernel-6.1" "drivers/gpu/drm/rockchip/ebc-dev/epdlut/epd_lut.h"
	.file	2 "/home/lyx/rk-android14/kernel-6.1" "drivers/gpu/drm/rockchip/ebc-dev/epdlut/rkf_waveform.c"
	.file	3 "/home/lyx/rk-android14/kernel-6.1" "./include/linux/stddef.h"
	.file	4 "/home/lyx/rk-android14/kernel-6.1" "./include/linux/slab.h"
	.file	5 "/home/lyx/rk-android14/kernel-6.1" "./include/uapi/asm-generic/int-ll64.h"
	.file	6 "/home/lyx/rk-android14/kernel-6.1" "./include/asm-generic/int-ll64.h"
	.file	7 "/home/lyx/rk-android14/kernel-6.1" "./include/linux/types.h"
	.globl	rkf_wf_input
	.p2align	2
	.type	rkf_wf_input,@function
rkf_wf_input:
.Lfunc_begin0:
	.loc	2 128 0
	.cfi_sections .debug_frame
	.cfi_startproc
	str	x30, [x18], #8
	.cfi_escape 0x16, 0x12, 0x02, 0x82, 0x78
	stp	x29, x30, [sp, #-64]!
	stp	x24, x23, [sp, #16]
	mov	x29, sp
	stp	x22, x21, [sp, #32]
	stp	x20, x19, [sp, #48]
	.cfi_def_cfa w29, 64
	.cfi_offset w19, -8
	.cfi_offset w20, -16
	.cfi_offset w21, -24
	.cfi_offset w22, -32
	.cfi_offset w23, -40
	.cfi_offset w24, -48
	.cfi_offset w30, -56
	.cfi_offset w29, -64
.Ltmp0:
	.loc	2 132 6 prologue_end
	adrp	x22, global_waveform
	ldr	x8, [x22, :lo12:global_waveform]
.Ltmp1:
	.loc	2 132 6 is_stmt 0
	cbz	x8, .LBB0_3
.Ltmp2:
	.loc	2 0 6
	mov	w0, #-16
.Ltmp3:
.LBB0_2:
	.loc	2 152 1 is_stmt 1
	ldp	x20, x19, [sp, #48]
	ldp	x22, x21, [sp, #32]
	ldp	x24, x23, [sp, #16]
	ldp	x29, x30, [sp], #64
	ldr	x30, [x18, #-8]!
	ret
.Ltmp4:
.LBB0_3:
	.loc	2 93 6
	adrp	x1, .L.str.5
	mov	x19, x0
.Ltmp5:
	.loc	2 135 28
	add	x0, x0, #4
.Ltmp6:
	.loc	2 93 6
	add	x1, x1, :lo12:.L.str.5
	mov	w2, #12
	bl	strncmp
.Ltmp7:
	.loc	2 93 6 is_stmt 0
	cbnz	w0, .LBB0_12
.Ltmp8:
	.loc	2 106 16 is_stmt 1
	ldr	w23, [x19]
.Ltmp9:
	.loc	2 106 6 is_stmt 0
	cmp	w23, #64, lsl #12
	b.gt	.LBB0_14
.Ltmp10:
	.loc	2 0 6
	sxtw	x8, w23
.Ltmp11:
	.loc	2 83 2 is_stmt 1
	adrp	x21, crc32_table
	add	x21, x21, :lo12:crc32_table
	mov	w1, wzr
	mov	x0, x21
	mov	w2, #1024
.Ltmp12:
	.loc	2 109 12
	ldr	w20, [x19, x8]
.Ltmp13:
	.loc	2 0 12 is_stmt 0
	mov	w24, #3511
	movk	w24, #1217, lsl #16
.Ltmp14:
	.loc	2 83 2 is_stmt 1
	bl	memset
.Ltmp15:
	.loc	2 0 2 is_stmt 0
	mov	w13, #28088
	mov	w14, #14044
	mov	w15, #7022
	mov	w8, wzr
	mov	w9, wzr
	mov	w10, wzr
	mov	w11, wzr
	mov	x12, xzr
	movk	w13, #9736, lsl #16
	movk	w14, #4868, lsl #16
	movk	w15, #2434, lsl #16
	mov	w16, #-2147483648
	mov	w17, #1073741824
	mov	w0, #536870912
	mov	w1, #268435456
.Ltmp16:
.LBB0_6:
	.loc	2 68 8 is_stmt 1
	lsl	w2, w12, #24
	tst	w12, #0x40
	and	w2, w13, w2, asr #31
.Ltmp17:
	eor	w3, w2, w14
	csel	w2, w2, w3, eq
	tst	w12, #0x20
	eor	w3, w2, w15
	csel	w2, w2, w3, eq
	tst	w12, #0x10
	eor	w3, w2, w24
	csel	w2, w2, w3, eq
.Ltmp18:
	.loc	2 68 16 is_stmt 0
	eor	w3, w2, w8
.Ltmp19:
	.loc	2 68 8
	lsl	w2, w2, #1
.Ltmp20:
	eor	w4, w2, w24
	cmp	w3, #0
	csel	w2, w2, w4, ge
.Ltmp21:
	.loc	2 64 16 is_stmt 1
	add	w8, w8, w1
.Ltmp22:
	.loc	2 68 16
	eor	w3, w2, w9
.Ltmp23:
	.loc	2 68 8 is_stmt 0
	lsl	w2, w2, #1
.Ltmp24:
	eor	w4, w2, w24
	cmp	w3, #0
	csel	w2, w2, w4, ge
.Ltmp25:
	.loc	2 64 16 is_stmt 1
	add	w9, w9, w0
.Ltmp26:
	.loc	2 68 16
	eor	w3, w2, w10
.Ltmp27:
	.loc	2 68 8 is_stmt 0
	lsl	w2, w2, #1
.Ltmp28:
	eor	w4, w2, w24
	cmp	w3, #0
	csel	w2, w2, w4, ge
.Ltmp29:
	.loc	2 64 16 is_stmt 1
	add	w10, w10, w17
.Ltmp30:
	.loc	2 68 16
	eor	w3, w2, w11
.Ltmp31:
	.loc	2 68 8 is_stmt 0
	lsl	w2, w2, #1
.Ltmp32:
	eor	w4, w2, w24
	cmp	w3, #0
	csel	w2, w2, w4, ge
.Ltmp33:
	.loc	2 64 16 is_stmt 1
	add	w11, w11, w16
.Ltmp34:
	.loc	2 74 16
	str	w2, [x21, x12, lsl #2]
.Ltmp35:
	.loc	2 64 24
	add	x12, x12, #1
.Ltmp36:
	.loc	2 64 2 is_stmt 0
	cmp	x12, #256
	b.ne	.LBB0_6
.Ltmp37:
	.loc	2 0 2
	mov	w2, wzr
.Ltmp38:
	.loc	2 85 2 is_stmt 1
	cbz	w23, .LBB0_10
.Ltmp39:
	.loc	2 0 2 is_stmt 0
	mov	x8, x19
.Ltmp40:
.LBB0_9:
	.loc	2 86 60 is_stmt 1
	ldrb	w9, [x8], #1
.Ltmp41:
	.loc	2 85 16
	subs	w23, w23, #1
	.loc	2 86 58
	eor	w9, w9, w2, lsr #24
	.loc	2 86 30 is_stmt 0
	ldr	w9, [x21, w9, uxtw #2]
	.loc	2 86 28
	eor	w2, w9, w2, lsl #8
.Ltmp42:
	.loc	2 85 2 is_stmt 1
	b.ne	.LBB0_9
.Ltmp43:
.LBB0_10:
	.loc	2 112 6
	cmp	w20, w2
	b.ne	.LBB0_13
.Ltmp44:
	.loc	2 147 2
	adrp	x0, .L.str.2
	add	x1, x19, #20
	add	x0, x0, :lo12:.L.str.2
	bl	_printk
.Ltmp45:
	.loc	2 0 2 is_stmt 0
	mov	w0, wzr
.Ltmp46:
	.loc	2 149 18 is_stmt 1
	str	x19, [x22, :lo12:global_waveform]
	b	.LBB0_2
.Ltmp47:
.LBB0_12:
	.loc	2 96 3
	adrp	x0, .L.str.6
	add	x0, x0, :lo12:.L.str.6
	bl	_printk
.Ltmp48:
	.loc	2 137 3
	adrp	x0, .L.str
	add	x0, x0, :lo12:.L.str
	b	.LBB0_15
.Ltmp49:
.LBB0_13:
	.loc	2 113 3
	adrp	x0, .L.str.7
	mov	w1, w20
	add	x0, x0, :lo12:.L.str.7
	bl	_printk
.Ltmp50:
.LBB0_14:
	.loc	2 143 3
	adrp	x0, .L.str.1
	add	x0, x0, :lo12:.L.str.1
.Ltmp51:
.LBB0_15:
	.loc	2 0 0 is_stmt 0
	bl	_printk
.Ltmp52:
	mov	w0, #-1
	b	.LBB0_2
.Ltmp53:
.Lfunc_end0:
	.size	rkf_wf_input, .Lfunc_end0-rkf_wf_input
	.cfi_endproc

	.globl	rkf_wf_get_version
	.p2align	2
	.type	rkf_wf_get_version,@function
rkf_wf_get_version:
.Lfunc_begin1:
	.loc	2 161 0 is_stmt 1
	.cfi_startproc
	.loc	2 162 7 prologue_end
	adrp	x8, global_waveform
	ldr	x8, [x8, :lo12:global_waveform]
.Ltmp54:
	.loc	2 162 6 is_stmt 0
	add	x9, x8, #84
	cmp	x8, #0
	csel	x0, xzr, x9, eq
	.loc	2 166 1 is_stmt 1
	ret
.Ltmp55:
.Lfunc_end1:
	.size	rkf_wf_get_version, .Lfunc_end1-rkf_wf_get_version
	.cfi_endproc

	.globl	rkf_wf_get_lut
	.p2align	2
	.type	rkf_wf_get_lut,@function
rkf_wf_get_lut:
.Lfunc_begin2:
	.loc	2 543 0
	.cfi_startproc
	str	x30, [x18], #8
	.cfi_escape 0x16, 0x12, 0x02, 0x82, 0x78
	stp	x29, x30, [sp, #-80]!
	stp	x26, x25, [sp, #16]
	mov	x29, sp
	stp	x24, x23, [sp, #32]
	stp	x22, x21, [sp, #48]
	stp	x20, x19, [sp, #64]
	.cfi_def_cfa w29, 80
	.cfi_offset w19, -8
	.cfi_offset w20, -16
	.cfi_offset w21, -24
	.cfi_offset w22, -32
	.cfi_offset w23, -40
	.cfi_offset w24, -48
	.cfi_offset w25, -56
	.cfi_offset w26, -64
	.cfi_offset w30, -72
	.cfi_offset w29, -80
.Ltmp56:
	.loc	2 549 7 prologue_end
	adrp	x23, global_waveform
	ldr	x8, [x23, :lo12:global_waveform]
.Ltmp57:
	.loc	2 549 6 is_stmt 0
	cbz	x8, .LBB2_19
.Ltmp58:
	.loc	2 0 6
	mov	x20, x0
.Ltmp59:
	.loc	2 552 6 is_stmt 1
	cbz	x0, .LBB2_20
.Ltmp60:
	.loc	2 555 6
	cmp	w3, #32
	b.eq	.LBB2_25
.Ltmp61:
	.loc	2 566 28
	adrp	x24, rkf_wf_get_lut.sftemp
	.loc	2 566 19 is_stmt 0
	mov	w8, #26215
	movk	w8, #26214, lsl #16
.Ltmp62:
	.loc	2 560 6 is_stmt 1
	cmp	w1, #8
	mov	w11, #5
	mov	w19, w2
.Ltmp63:
	.loc	2 566 28
	ldrsw	x9, [x24, :lo12:rkf_wf_get_lut.sftemp]
	.loc	2 566 19 is_stmt 0
	smull	x10, w2, w8
.Ltmp64:
	.loc	2 560 6 is_stmt 1
	csel	w21, w11, w1, eq
.Ltmp65:
	.loc	2 0 6 is_stmt 0
	adrp	x26, rkf_wf_get_lut.stype
.Ltmp66:
	.loc	2 566 35 is_stmt 1
	mul	x8, x9, x8
	.loc	2 566 19 is_stmt 0
	lsr	x9, x10, #63
	asr	x10, x10, #33
	add	w25, w10, w9
	.loc	2 566 35
	lsr	x9, x8, #63
	asr	x8, x8, #33
	add	w8, w8, w9
	.loc	2 566 41
	cmp	w25, w8
	b.ne	.LBB2_5
.Ltmp67:
	.loc	2 0 41
	ldr	w9, [x26, :lo12:rkf_wf_get_lut.stype]
	.loc	2 566 41
	cmp	w9, w21
	b.eq	.LBB2_18
.Ltmp68:
.LBB2_5:
	.loc	2 569 6 is_stmt 1
	cmp	w21, #10
	b.eq	.LBB2_17
.Ltmp69:
	.loc	2 571 15
	ldr	x0, [x20, #8]
.Ltmp70:
	.loc	2 571 7 is_stmt 0
	cbz	x0, .LBB2_8
.Ltmp71:
	.loc	2 572 4 is_stmt 1
	bl	kfree
.Ltmp72:
	.loc	2 573 17
	str	xzr, [x20, #8]
.Ltmp73:
.LBB2_8:
	.loc	2 576 15
	mov	x0, x20
	mov	w1, w21
	mov	w2, w19
	bl	parse_wf_data
.Ltmp74:
	.loc	2 577 7
	tbnz	w0, #31, .LBB2_27
.Ltmp75:
	.loc	2 0 0 is_stmt 0
	mov	w22, w0
.Ltmp76:
	.loc	2 581 33 is_stmt 1
	ldr	x1, [x20, #16]
	.loc	2 581 3 is_stmt 0
	mov	x0, x20
	mov	w2, w22
	bl	rkf_lut_init_wf_table
.Ltmp77:
	.loc	2 582 21 is_stmt 1
	ldrb	w8, [x20, #1]
.Ltmp78:
	.loc	2 587 15
	ldr	x0, [x20, #8]
.Ltmp79:
	.loc	2 583 21
	orr	w8, w22, w8, lsl #8
	str	w8, [x20]
	.loc	2 584 9
	str	w21, [x26, :lo12:rkf_wf_get_lut.stype]
	.loc	2 587 7
	cbz	x0, .LBB2_11
.Ltmp80:
	.loc	2 588 4
	bl	kfree
.Ltmp81:
	.loc	2 589 17
	str	xzr, [x20, #8]
.Ltmp82:
.LBB2_11:
	.loc	2 188 34
	ldr	x8, [x23, :lo12:global_waveform]
.Ltmp83:
	.loc	2 173 9
	bic	w10, w19, w19, asr #31
.Ltmp84:
	.loc	2 174 9
	cmp	w10, #50
	mov	w11, #50
	mov	x9, xzr
.Ltmp85:
	csel	w10, w10, w11, lo
.Ltmp86:
	.loc	2 176 2
	add	x11, x8, #532
.Ltmp87:
.LBB2_12:
	.loc	2 177 14
	ldrb	w12, [x11, x9]
.Ltmp88:
	.loc	2 177 7 is_stmt 0
	cmp	w12, w10, uxtb
	b.hi	.LBB2_15
.Ltmp89:
	.loc	2 176 23 is_stmt 1
	add	x9, x9, #1
.Ltmp90:
	.loc	2 176 2 is_stmt 0
	cmp	x9, #50
	b.ne	.LBB2_12
.Ltmp91:
	.loc	2 0 2
	mov	x9, xzr
.Ltmp92:
.LBB2_15:
	.loc	2 229 14 is_stmt 1
	add	x9, x8, x9, lsl #2
.Ltmp93:
	.loc	4 558 9
	mov	w1, #3520
.Ltmp94:
	.loc	2 229 14
	ldr	w9, [x9, #2132]
.Ltmp95:
	.loc	2 230 19
	add	x26, x8, x9
.Ltmp96:
	.loc	2 237 9
	ldrb	w21, [x26]
.Ltmp97:
	.loc	2 439 26
	lsl	x22, x21, #6
.Ltmp98:
	.loc	4 558 9
	mov	x0, x22
	bl	__kmalloc
.Ltmp99:
	.loc	2 246 6
	cbz	x0, .LBB2_26
.Ltmp100:
	.loc	2 249 36
	add	x1, x26, #4
	.loc	2 249 2 is_stmt 0
	mov	x2, x22
	.loc	2 0 0
	mov	x23, x0
.Ltmp101:
	.loc	2 249 2
	bl	memcpy
.Ltmp102:
	.loc	2 597 33 is_stmt 1
	ldr	x1, [x20, #32]
	.loc	2 597 3 is_stmt 0
	mov	x0, x20
	mov	w2, w21
.Ltmp103:
	.loc	2 440 15 is_stmt 1
	str	x23, [x20, #8]
.Ltmp104:
	.loc	2 597 3
	bl	rkf_lut_init_wf_table
.Ltmp105:
	.loc	2 601 27
	ldrsw	x8, [x24, :lo12:rkf_wf_get_lut.sftemp]
	mov	w9, #26215
	movk	w9, #26214, lsl #16
.Ltmp106:
	.loc	2 598 28
	str	w21, [x20, #4]
.Ltmp107:
	.loc	2 601 34
	mul	x8, x8, x9
	lsr	x9, x8, #63
	asr	x8, x8, #33
	add	w8, w8, w9
.Ltmp108:
.LBB2_17:
	.loc	2 601 6 is_stmt 0
	cmp	w25, w8
	b.ne	.LBB2_21
.Ltmp109:
.LBB2_18:
	.loc	2 0 6
	mov	w0, wzr
	b	.LBB2_28
.Ltmp110:
.LBB2_19:
	mov	w0, #-19
.Ltmp111:
	b	.LBB2_28
.Ltmp112:
.LBB2_20:
	mov	w0, #-22
	b	.LBB2_28
.Ltmp113:
.LBB2_21:
	.loc	2 603 15 is_stmt 1
	ldr	x0, [x20, #8]
.Ltmp114:
	.loc	2 603 7 is_stmt 0
	cbz	x0, .LBB2_23
.Ltmp115:
	.loc	2 604 4 is_stmt 1
	bl	kfree
.Ltmp116:
	.loc	2 605 17
	str	xzr, [x20, #8]
.Ltmp117:
.LBB2_23:
	.loc	2 607 17
	bl	epd_overlay_lut
.Ltmp118:
	mov	w1, w0
.Ltmp119:
	.loc	2 608 15
	mov	x0, x20
	mov	w2, w19
	bl	parse_wf_data
.Ltmp120:
	.loc	2 609 7
	tbnz	w0, #31, .LBB2_27
.Ltmp121:
	.loc	2 0 0 is_stmt 0
	mov	w21, w0
.Ltmp122:
	.loc	2 613 33 is_stmt 1
	ldr	x1, [x20, #24]
	.loc	2 613 3 is_stmt 0
	mov	x0, x20
	mov	w2, w21
	bl	rkf_lut_init_wf_table
.Ltmp123:
	.loc	2 615 21 is_stmt 1
	ldrb	w8, [x20]
	mov	w0, wzr
	.loc	2 616 21
	bfi	w8, w21, #8, #24
	str	w8, [x20]
	.loc	2 617 10
	str	w19, [x24, :lo12:rkf_wf_get_lut.sftemp]
	b	.LBB2_28
.Ltmp124:
.LBB2_25:
	.loc	2 556 3
	adrp	x0, .L.str.3
	add	x0, x0, :lo12:.L.str.3
	bl	_printk
.Ltmp125:
	.loc	2 0 3 is_stmt 0
	mov	w0, #-22
	b	.LBB2_28
.Ltmp126:
.LBB2_26:
	.loc	2 440 15 is_stmt 1
	str	xzr, [x20, #8]
.Ltmp127:
.LBB2_27:
	.loc	2 0 0 is_stmt 0
	adrp	x0, .L.str.4
	add	x0, x0, :lo12:.L.str.4
	bl	_printk
.Ltmp128:
	mov	w0, #-5
.Ltmp129:
.LBB2_28:
	.loc	2 621 1 is_stmt 1
	ldp	x20, x19, [sp, #64]
	ldp	x22, x21, [sp, #48]
	ldp	x24, x23, [sp, #32]
	ldp	x26, x25, [sp, #16]
	ldp	x29, x30, [sp], #80
	ldr	x30, [x18, #-8]!
	ret
.Ltmp130:
.Lfunc_end2:
	.size	rkf_wf_get_lut, .Lfunc_end2-rkf_wf_get_lut
	.cfi_endproc
	.file	8 "/home/lyx/rk-android14/kernel-6.1" "./include/uapi/asm-generic/posix_types.h"

	.p2align	2
	.type	parse_wf_data,@function
parse_wf_data:
.Lfunc_begin3:
	.loc	2 466 0
	.cfi_startproc
	str	x30, [x18], #8
	.cfi_escape 0x16, 0x12, 0x02, 0x82, 0x78
	stp	x29, x30, [sp, #-64]!
	stp	x24, x23, [sp, #16]
	mov	x29, sp
	stp	x22, x21, [sp, #32]
	stp	x20, x19, [sp, #48]
	.cfi_def_cfa w29, 64
	.cfi_offset w19, -8
	.cfi_offset w20, -16
	.cfi_offset w21, -24
	.cfi_offset w22, -32
	.cfi_offset w23, -40
	.cfi_offset w24, -48
	.cfi_offset w30, -56
	.cfi_offset w29, -64
.Ltmp131:
	.loc	2 470 2 prologue_end
	cmp	w1, #7
	b.hi	.LBB3_14
.Ltmp132:
	.loc	2 0 2 is_stmt 0
	adrp	x9, .LJTI3_0
	mov	w8, w1
	add	x9, x9, :lo12:.LJTI3_0
	mov	x19, x0
.Ltmp133:
	adr	x10, .LBB3_2
	ldrb	w11, [x9, x8]
	add	x10, x10, x11, lsl #2
	br	x10
.Ltmp134:
.LBB3_2:
	.loc	2 188 34 is_stmt 1
	adrp	x8, global_waveform
.Ltmp135:
	.loc	2 173 9
	bic	w10, w2, w2, asr #31
.Ltmp136:
	.loc	2 174 9
	cmp	w10, #50
	mov	w11, #50
	mov	x9, xzr
.Ltmp137:
	csel	w10, w10, w11, lo
.Ltmp138:
	.loc	2 188 34
	ldr	x8, [x8, :lo12:global_waveform]
.Ltmp139:
	.loc	2 176 2
	add	x11, x8, #148
.Ltmp140:
.LBB3_3:
	.loc	2 177 14
	ldrb	w12, [x11, x9]
.Ltmp141:
	.loc	2 177 7 is_stmt 0
	cmp	w12, w10, uxtb
	b.hi	.LBB3_6
.Ltmp142:
	.loc	2 176 23 is_stmt 1
	add	x9, x9, #1
.Ltmp143:
	.loc	2 176 2 is_stmt 0
	cmp	x9, #50
	b.ne	.LBB3_3
.Ltmp144:
	.loc	2 0 2
	mov	x9, xzr
.Ltmp145:
.LBB3_6:
	.loc	2 229 14 is_stmt 1
	add	x9, x8, x9, lsl #2
.Ltmp146:
	.loc	4 558 9
	mov	w1, #3520
.Ltmp147:
	.loc	2 229 14
	ldr	w9, [x9, #596]
.Ltmp148:
	.loc	2 230 19
	add	x24, x8, x9
.Ltmp149:
	.loc	2 237 9
	ldrb	w20, [x24]
.Ltmp150:
	.loc	2 333 11
	add	x23, x20, #15
	.loc	2 333 39 is_stmt 0
	lsr	x8, x23, #2
.Ltmp151:
	and	x22, x8, #0x7c
.Ltmp152:
	.loc	4 558 9 is_stmt 1
	mov	x0, x22
	bl	__kmalloc
.Ltmp153:
	.loc	2 246 6
	cbz	x0, .LBB3_14
.Ltmp154:
	.loc	2 249 36
	add	x1, x24, #4
	.loc	2 249 2 is_stmt 0
	mov	x2, x22
	.loc	2 0 0
	mov	x21, x0
.Ltmp155:
	.loc	2 249 2
	bl	memcpy
.Ltmp156:
	.loc	2 265 27 is_stmt 1
	lsl	w8, w23, #6
.Ltmp157:
	.loc	4 558 9
	mov	w1, #3520
.Ltmp158:
	.loc	2 265 27
	and	w0, w8, #0x7c00
.Ltmp159:
	.loc	4 558 9
	bl	__kmalloc
.Ltmp160:
	.loc	2 266 6
	cbz	x0, .LBB3_52
.Ltmp161:
	.loc	2 269 2
	cbz	w20, .LBB3_13
.Ltmp162:
	.loc	2 0 0 is_stmt 0
	lsr	w11, w23, #4
.Ltmp163:
	mov	w10, #16384
.Ltmp164:
	.loc	2 269 2
	cmp	w11, #1
	mov	w12, #1365
	mov	w8, wzr
	mov	w9, wzr
	movk	w10, #21, lsl #16
	csinc	w11, w11, wzr, gt
.Ltmp165:
	.loc	2 0 2
	movk	w12, #5440, lsl #16
	mov	x13, x21
.Ltmp166:
.LBB3_10:
	mov	w15, wzr
	mov	w14, wzr
.Ltmp167:
	.loc	2 270 15 is_stmt 1
	ldr	w16, [x13]
.Ltmp168:
.LBB3_11:
	.loc	2 272 26
	asr	w17, w16, w15
	.loc	2 272 31 is_stmt 0
	add	w3, w8, w14
	.loc	2 272 36
	and	w1, w17, #0x3
.Ltmp169:
	.loc	2 276 26 is_stmt 1
	lsl	w17, w17, #30
.Ltmp170:
	.loc	2 279 13
	add	w4, w3, #3
.Ltmp171:
	.loc	2 271 17
	add	w14, w14, #16
.Ltmp172:
	.loc	2 0 17 is_stmt 0
	mul	w2, w1, w10
	.loc	2 271 17
	add	w15, w15, #2
.Ltmp173:
	.loc	2 276 19 is_stmt 1
	bfi	w17, w1, #12, #2
.Ltmp174:
	.loc	2 271 3
	cmp	w14, #256
.Ltmp175:
	.loc	2 276 19
	orr	w17, w17, w2
.Ltmp176:
	.loc	2 279 13
	add	w2, w3, #1
.Ltmp177:
	.loc	2 276 19
	madd	w17, w1, w12, w17
.Ltmp178:
	.loc	2 279 13
	add	w1, w3, #2
	.loc	2 279 20 is_stmt 0
	str	w17, [x0, w2, sxtw #2]
.Ltmp179:
	.loc	2 279 13
	add	w2, w3, #4
	.loc	2 279 20
	str	w17, [x0, w1, sxtw #2]
.Ltmp180:
	.loc	2 279 13
	add	w1, w3, #5
	.loc	2 279 20
	str	w17, [x0, w4, sxtw #2]
.Ltmp181:
	.loc	2 279 13
	add	w4, w3, #6
	.loc	2 279 20
	str	w17, [x0, w2, sxtw #2]
.Ltmp182:
	.loc	2 279 13
	add	w2, w3, #7
	.loc	2 279 20
	str	w17, [x0, w1, sxtw #2]
.Ltmp183:
	.loc	2 279 13
	add	w1, w3, #8
	.loc	2 279 20
	str	w17, [x0, w4, sxtw #2]
.Ltmp184:
	.loc	2 279 13
	add	w4, w3, #9
	.loc	2 279 20
	str	w17, [x0, w2, sxtw #2]
.Ltmp185:
	.loc	2 279 13
	add	w2, w3, #10
	.loc	2 279 20
	str	w17, [x0, w1, sxtw #2]
.Ltmp186:
	.loc	2 279 13
	add	w1, w3, #11
	.loc	2 279 20
	str	w17, [x0, w4, sxtw #2]
.Ltmp187:
	str	w17, [x0, w2, sxtw #2]
.Ltmp188:
	.loc	2 279 13
	add	w2, w3, #12
	.loc	2 279 20
	str	w17, [x0, w1, sxtw #2]
.Ltmp189:
	.loc	2 279 13
	add	w1, w3, #13
	.loc	2 279 20
	str	w17, [x0, w3, sxtw #2]
.Ltmp190:
	str	w17, [x0, w2, sxtw #2]
.Ltmp191:
	.loc	2 279 13
	add	w2, w3, #14
	.loc	2 279 20
	str	w17, [x0, w1, sxtw #2]
.Ltmp192:
	.loc	2 279 13
	add	w1, w3, #15
	.loc	2 279 20
	str	w17, [x0, w2, sxtw #2]
.Ltmp193:
	str	w17, [x0, w1, sxtw #2]
.Ltmp194:
	.loc	2 271 3 is_stmt 1
	b.ne	.LBB3_11
.Ltmp195:
	.loc	2 270 21
	add	x13, x13, #4
.Ltmp196:
	.loc	2 269 24
	add	w9, w9, #1
.Ltmp197:
	.loc	2 269 2 is_stmt 0
	add	w8, w8, w14
	cmp	w9, w11
	b.ne	.LBB3_10
.Ltmp198:
.LBB3_13:
	.loc	2 341 15 is_stmt 1
	str	x0, [x19, #8]
	.loc	2 0 0 is_stmt 0
	mov	x0, x21
	bl	kfree
.Ltmp199:
	b	.LBB3_51
.Ltmp200:
.LBB3_14:
	mov	w20, #-1
	b	.LBB3_51
.Ltmp201:
.LBB3_15:
	.loc	2 188 34 is_stmt 1
	adrp	x8, global_waveform
.Ltmp202:
	.loc	2 173 9
	bic	w10, w2, w2, asr #31
.Ltmp203:
	.loc	2 174 9
	cmp	w10, #50
	mov	w11, #50
	mov	x9, xzr
.Ltmp204:
	csel	w10, w10, w11, lo
.Ltmp205:
	.loc	2 188 34
	ldr	x8, [x8, :lo12:global_waveform]
.Ltmp206:
	.loc	2 176 2
	add	x11, x8, #468
.Ltmp207:
.LBB3_16:
	.loc	2 177 14
	ldrb	w12, [x11, x9]
.Ltmp208:
	.loc	2 177 7 is_stmt 0
	cmp	w12, w10, uxtb
	b.hi	.LBB3_19
.Ltmp209:
	.loc	2 176 23 is_stmt 1
	add	x9, x9, #1
.Ltmp210:
	.loc	2 176 2 is_stmt 0
	cmp	x9, #50
	b.ne	.LBB3_16
.Ltmp211:
	.loc	2 0 2
	mov	x9, xzr
.Ltmp212:
.LBB3_19:
	.loc	2 229 14 is_stmt 1
	add	x9, x8, x9, lsl #2
	ldr	w9, [x9, #1876]
.Ltmp213:
	.loc	2 0 14 is_stmt 0
	b	.LBB3_48
.Ltmp214:
.LBB3_20:
	.loc	2 188 34 is_stmt 1
	adrp	x8, global_waveform
.Ltmp215:
	.loc	2 173 9
	bic	w10, w2, w2, asr #31
.Ltmp216:
	.loc	2 174 9
	cmp	w10, #50
	mov	w11, #50
	mov	x9, xzr
.Ltmp217:
	csel	w10, w10, w11, lo
.Ltmp218:
	.loc	2 188 34
	ldr	x8, [x8, :lo12:global_waveform]
.Ltmp219:
	.loc	2 176 2
	add	x11, x8, #212
.Ltmp220:
.LBB3_21:
	.loc	2 177 14
	ldrb	w12, [x11, x9]
.Ltmp221:
	.loc	2 177 7 is_stmt 0
	cmp	w12, w10, uxtb
	b.hi	.LBB3_27
.Ltmp222:
	.loc	2 176 23 is_stmt 1
	add	x9, x9, #1
.Ltmp223:
	.loc	2 176 2 is_stmt 0
	cmp	x9, #50
	b.ne	.LBB3_21
	b	.LBB3_26
.Ltmp224:
.LBB3_23:
	.loc	2 188 34 is_stmt 1
	adrp	x8, global_waveform
.Ltmp225:
	.loc	2 173 9
	bic	w10, w2, w2, asr #31
.Ltmp226:
	.loc	2 174 9
	cmp	w10, #50
	mov	w11, #50
	mov	x9, xzr
.Ltmp227:
	csel	w10, w10, w11, lo
.Ltmp228:
	.loc	2 188 34
	ldr	x8, [x8, :lo12:global_waveform]
.Ltmp229:
	.loc	2 176 2
	add	x11, x8, #212
.Ltmp230:
.LBB3_24:
	.loc	2 177 14
	ldrb	w12, [x11, x9]
.Ltmp231:
	.loc	2 177 7 is_stmt 0
	cmp	w12, w10, uxtb
	b.hi	.LBB3_27
.Ltmp232:
	.loc	2 176 23 is_stmt 1
	add	x9, x9, #1
.Ltmp233:
	.loc	2 176 2 is_stmt 0
	cmp	x9, #50
	b.ne	.LBB3_24
.Ltmp234:
.LBB3_26:
	.loc	2 0 2
	mov	x9, xzr
.Ltmp235:
.LBB3_27:
	add	x9, x8, x9, lsl #2
	ldr	w9, [x9, #852]
	b	.LBB3_48
.Ltmp236:
.LBB3_28:
	.loc	2 188 34 is_stmt 1
	adrp	x8, global_waveform
.Ltmp237:
	.loc	2 173 9
	bic	w10, w2, w2, asr #31
.Ltmp238:
	.loc	2 174 9
	cmp	w10, #50
	mov	w11, #50
	mov	x9, xzr
.Ltmp239:
	csel	w10, w10, w11, lo
.Ltmp240:
	.loc	2 188 34
	ldr	x8, [x8, :lo12:global_waveform]
.Ltmp241:
	.loc	2 176 2
	add	x11, x8, #276
.Ltmp242:
.LBB3_29:
	.loc	2 177 14
	ldrb	w12, [x11, x9]
.Ltmp243:
	.loc	2 177 7 is_stmt 0
	cmp	w12, w10, uxtb
	b.hi	.LBB3_32
.Ltmp244:
	.loc	2 176 23 is_stmt 1
	add	x9, x9, #1
.Ltmp245:
	.loc	2 176 2 is_stmt 0
	cmp	x9, #50
	b.ne	.LBB3_29
.Ltmp246:
	.loc	2 0 2
	mov	x9, xzr
.Ltmp247:
.LBB3_32:
	.loc	2 229 14 is_stmt 1
	add	x9, x8, x9, lsl #2
	ldr	w9, [x9, #1108]
.Ltmp248:
	.loc	2 0 14 is_stmt 0
	b	.LBB3_48
.Ltmp249:
.LBB3_33:
	.loc	2 188 34 is_stmt 1
	adrp	x8, global_waveform
.Ltmp250:
	.loc	2 173 9
	bic	w10, w2, w2, asr #31
.Ltmp251:
	.loc	2 174 9
	cmp	w10, #50
	mov	w11, #50
	mov	x9, xzr
.Ltmp252:
	csel	w10, w10, w11, lo
.Ltmp253:
	.loc	2 188 34
	ldr	x8, [x8, :lo12:global_waveform]
.Ltmp254:
	.loc	2 176 2
	add	x11, x8, #340
.Ltmp255:
.LBB3_34:
	.loc	2 177 14
	ldrb	w12, [x11, x9]
.Ltmp256:
	.loc	2 177 7 is_stmt 0
	cmp	w12, w10, uxtb
	b.hi	.LBB3_37
.Ltmp257:
	.loc	2 176 23 is_stmt 1
	add	x9, x9, #1
.Ltmp258:
	.loc	2 176 2 is_stmt 0
	cmp	x9, #50
	b.ne	.LBB3_34
.Ltmp259:
	.loc	2 0 2
	mov	x9, xzr
.Ltmp260:
.LBB3_37:
	.loc	2 229 14 is_stmt 1
	add	x9, x8, x9, lsl #2
	ldr	w9, [x9, #1364]
.Ltmp261:
	.loc	2 0 14 is_stmt 0
	b	.LBB3_48
.Ltmp262:
.LBB3_38:
	.loc	2 188 34 is_stmt 1
	adrp	x8, global_waveform
.Ltmp263:
	.loc	2 173 9
	bic	w10, w2, w2, asr #31
.Ltmp264:
	.loc	2 174 9
	cmp	w10, #50
	mov	w11, #50
	mov	x9, xzr
.Ltmp265:
	csel	w10, w10, w11, lo
.Ltmp266:
	.loc	2 188 34
	ldr	x8, [x8, :lo12:global_waveform]
.Ltmp267:
	.loc	2 176 2
	add	x11, x8, #404
.Ltmp268:
.LBB3_39:
	.loc	2 177 14
	ldrb	w12, [x11, x9]
.Ltmp269:
	.loc	2 177 7 is_stmt 0
	cmp	w12, w10, uxtb
	b.hi	.LBB3_42
.Ltmp270:
	.loc	2 176 23 is_stmt 1
	add	x9, x9, #1
.Ltmp271:
	.loc	2 176 2 is_stmt 0
	cmp	x9, #50
	b.ne	.LBB3_39
.Ltmp272:
	.loc	2 0 2
	mov	x9, xzr
.Ltmp273:
.LBB3_42:
	.loc	2 229 14 is_stmt 1
	add	x9, x8, x9, lsl #2
	ldr	w9, [x9, #1620]
.Ltmp274:
	.loc	2 0 14 is_stmt 0
	b	.LBB3_48
.Ltmp275:
.LBB3_43:
	.loc	2 188 34 is_stmt 1
	adrp	x8, global_waveform
.Ltmp276:
	.loc	2 173 9
	bic	w10, w2, w2, asr #31
.Ltmp277:
	.loc	2 174 9
	cmp	w10, #50
	mov	w11, #50
	mov	x9, xzr
.Ltmp278:
	csel	w10, w10, w11, lo
.Ltmp279:
	.loc	2 188 34
	ldr	x8, [x8, :lo12:global_waveform]
.Ltmp280:
	.loc	2 176 2
	add	x11, x8, #532
.Ltmp281:
.LBB3_44:
	.loc	2 177 14
	ldrb	w12, [x11, x9]
.Ltmp282:
	.loc	2 177 7 is_stmt 0
	cmp	w12, w10, uxtb
	b.hi	.LBB3_47
.Ltmp283:
	.loc	2 176 23 is_stmt 1
	add	x9, x9, #1
.Ltmp284:
	.loc	2 176 2 is_stmt 0
	cmp	x9, #50
	b.ne	.LBB3_44
.Ltmp285:
	.loc	2 0 2
	mov	x9, xzr
.Ltmp286:
.LBB3_47:
	.loc	2 229 14 is_stmt 1
	add	x9, x8, x9, lsl #2
	ldr	w9, [x9, #2132]
.Ltmp287:
.LBB3_48:
	.loc	2 0 0 is_stmt 0
	add	x23, x8, x9
.Ltmp288:
	mov	w1, #3520
.Ltmp289:
	ldrb	w22, [x23]
	lsl	x20, x22, #6
.Ltmp290:
	mov	x0, x20
	bl	__kmalloc
.Ltmp291:
	cbz	x0, .LBB3_50
.Ltmp292:
	add	x1, x23, #4
	mov	x2, x20
	mov	x21, x0
.Ltmp293:
	bl	memcpy
.Ltmp294:
	mov	w20, w22
.Ltmp295:
	str	x21, [x19, #8]
.Ltmp296:
	b	.LBB3_51
.Ltmp297:
.LBB3_50:
	mov	w20, #-1
.Ltmp298:
	str	xzr, [x19, #8]
.Ltmp299:
.LBB3_51:
	.loc	2 530 1 is_stmt 1
	mov	w0, w20
	ldp	x20, x19, [sp, #48]
	ldp	x22, x21, [sp, #32]
	ldp	x24, x23, [sp, #16]
	ldp	x29, x30, [sp], #64
	ldr	x30, [x18, #-8]!
	ret
.LBB3_52:
.Ltmp300:
	.loc	2 0 0 is_stmt 0
	mov	x0, x21
	.loc	2 341 15 is_stmt 1
	str	xzr, [x19, #8]
	.loc	2 0 0 is_stmt 0
	bl	kfree
.Ltmp301:
	mov	w20, #-1
.Ltmp302:
	b	.LBB3_51
.Ltmp303:
.Lfunc_end3:
	.size	parse_wf_data, .Lfunc_end3-parse_wf_data
	.cfi_endproc
	.section	.rodata,"a",@progbits
.LJTI3_0:
	.byte	(.LBB3_2-.LBB3_2)>>2
	.byte	(.LBB3_15-.LBB3_2)>>2
	.byte	(.LBB3_20-.LBB3_2)>>2
	.byte	(.LBB3_23-.LBB3_2)>>2
	.byte	(.LBB3_28-.LBB3_2)>>2
	.byte	(.LBB3_33-.LBB3_2)>>2
	.byte	(.LBB3_38-.LBB3_2)>>2
	.byte	(.LBB3_43-.LBB3_2)>>2

	.text
	.p2align	2
	.type	rkf_lut_init_wf_table,@function
rkf_lut_init_wf_table:
.Lfunc_begin4:
	.loc	2 448 0 is_stmt 1
	.cfi_startproc
	.loc	2 452 2 prologue_end
	cmp	w2, #1
	b.lt	.LBB4_5
.Ltmp304:
	.loc	2 0 2 is_stmt 0
	mov	w8, wzr
	mov	w9, wzr
	mov	x10, xzr
.Ltmp305:
	.loc	2 452 14
	mov	w11, w2
.Ltmp306:
	.loc	2 452 2
	add	x12, x1, #7
.Ltmp307:
.LBB4_2:
	.loc	2 453 34 is_stmt 1
	ldr	x15, [x0, #8]
.Ltmp308:
	and	x14, x8, #0x7ffffff
	mov	x13, xzr
	add	x14, x12, x14, lsl #5
.Ltmp309:
	.loc	2 454 3
	add	x15, x15, w9, sxtw #2
.Ltmp310:
.LBB4_3:
	.loc	2 0 3 is_stmt 0
	add	x16, x15, x13
.Ltmp311:
	.loc	2 454 31
	add	x13, x13, #4
.Ltmp312:
	.loc	2 454 3
	cmp	x13, #64
.Ltmp313:
	.loc	2 456 16 is_stmt 1
	ldrb	w17, [x16]
	.loc	2 456 15 is_stmt 0
	and	w17, w17, #0x3
.Ltmp314:
	.loc	2 457 54 is_stmt 1
	sturb	w17, [x14, #-7]
.Ltmp315:
	.loc	2 456 16
	ldrb	w17, [x16]
	.loc	2 456 15 is_stmt 0
	ubfx	w17, w17, #2, #2
.Ltmp316:
	.loc	2 457 54 is_stmt 1
	sturb	w17, [x14, #-6]
.Ltmp317:
	.loc	2 456 16
	ldrb	w17, [x16]
	.loc	2 456 15 is_stmt 0
	ubfx	w17, w17, #4, #2
.Ltmp318:
	.loc	2 457 54 is_stmt 1
	sturb	w17, [x14, #-5]
.Ltmp319:
	.loc	2 456 16
	ldrb	w17, [x16]
	.loc	2 456 15 is_stmt 0
	lsr	w17, w17, #6
.Ltmp320:
	.loc	2 457 54 is_stmt 1
	sturb	w17, [x14, #-4]
.Ltmp321:
	.loc	2 456 16
	ldrb	w17, [x16, #1]
	.loc	2 456 15 is_stmt 0
	and	w17, w17, #0x3
.Ltmp322:
	.loc	2 457 54 is_stmt 1
	sturb	w17, [x14, #-3]
.Ltmp323:
	.loc	2 456 16
	ldr	w17, [x16]
	.loc	2 456 15 is_stmt 0
	ubfx	w17, w17, #10, #2
.Ltmp324:
	.loc	2 457 54 is_stmt 1
	sturb	w17, [x14, #-2]
.Ltmp325:
	.loc	2 456 16
	ldr	w17, [x16]
	.loc	2 456 15 is_stmt 0
	ubfx	w17, w17, #12, #2
.Ltmp326:
	.loc	2 457 54 is_stmt 1
	sturb	w17, [x14, #-1]
.Ltmp327:
	.loc	2 456 16
	ldr	w17, [x16]
	.loc	2 456 15 is_stmt 0
	ubfx	w17, w17, #14, #2
.Ltmp328:
	.loc	2 457 54 is_stmt 1
	strb	w17, [x14]
.Ltmp329:
	.loc	2 456 16
	ldrb	w17, [x16, #2]
	.loc	2 456 15 is_stmt 0
	and	w17, w17, #0x3
.Ltmp330:
	.loc	2 457 54 is_stmt 1
	strb	w17, [x14, #1]
.Ltmp331:
	.loc	2 456 16
	ldr	w17, [x16]
	.loc	2 456 15 is_stmt 0
	ubfx	w17, w17, #18, #2
.Ltmp332:
	.loc	2 457 54 is_stmt 1
	strb	w17, [x14, #2]
.Ltmp333:
	.loc	2 456 16
	ldr	w17, [x16]
	.loc	2 456 15 is_stmt 0
	ubfx	w17, w17, #20, #2
.Ltmp334:
	.loc	2 457 54 is_stmt 1
	strb	w17, [x14, #3]
.Ltmp335:
	.loc	2 456 16
	ldr	w17, [x16]
	.loc	2 456 15 is_stmt 0
	ubfx	w17, w17, #22, #2
.Ltmp336:
	.loc	2 457 54 is_stmt 1
	strb	w17, [x14, #4]
.Ltmp337:
	.loc	2 456 16
	ldrb	w17, [x16, #3]
	.loc	2 456 15 is_stmt 0
	and	w17, w17, #0x3
.Ltmp338:
	.loc	2 457 54 is_stmt 1
	strb	w17, [x14, #5]
.Ltmp339:
	.loc	2 456 16
	ldr	w17, [x16]
	.loc	2 456 15 is_stmt 0
	ubfx	w17, w17, #26, #2
.Ltmp340:
	.loc	2 457 54 is_stmt 1
	strb	w17, [x14, #6]
.Ltmp341:
	.loc	2 456 16
	ldr	w17, [x16]
	.loc	2 456 15 is_stmt 0
	ubfx	w17, w17, #28, #2
.Ltmp342:
	.loc	2 457 54 is_stmt 1
	strb	w17, [x14, #7]
.Ltmp343:
	.loc	2 456 16
	ldr	w16, [x16]
	.loc	2 456 39 is_stmt 0
	lsr	w16, w16, #30
.Ltmp344:
	.loc	2 457 54 is_stmt 1
	strb	w16, [x14, #8]
.Ltmp345:
	.loc	2 454 31
	add	x14, x14, #32
.Ltmp346:
	.loc	2 454 3 is_stmt 0
	b.ne	.LBB4_3
.Ltmp347:
	.loc	2 452 28 is_stmt 1
	add	x10, x10, #1
.Ltmp348:
	.loc	2 452 2 is_stmt 0
	add	w9, w9, #16
	add	w8, w8, #32
	cmp	x10, x11
	b.ne	.LBB4_2
.Ltmp349:
.LBB4_5:
	.loc	2 462 2 is_stmt 1
	ret
.Ltmp350:
.Lfunc_end4:
	.size	rkf_lut_init_wf_table, .Lfunc_end4-rkf_lut_init_wf_table
	.cfi_endproc

	.type	global_waveform,@object
	.local	global_waveform
	.comm	global_waveform,8,8
	.type	.L.str,@object
	.section	.rodata.str1.1,"aMS",@progbits,1
.L.str:
	.asciz	"rkf: failed to check RKF file format\n"
	.size	.L.str, 38

	.type	.L.str.1,@object
.L.str.1:
	.asciz	"rkf: failed to check crc RKF waveform\n"
	.size	.L.str.1, 39

	.type	.L.str.2,@object
.L.str.2:
	.asciz	"rkf file version: %s\n"
	.size	.L.str.2, 22

	.type	rkf_wf_get_lut.stype,@object
	.data
	.p2align	2
rkf_wf_get_lut.stype:
	.word	11
	.size	rkf_wf_get_lut.stype, 4

	.type	rkf_wf_get_lut.sftemp,@object
	.p2align	2
rkf_wf_get_lut.sftemp:
	.word	4294967295
	.size	rkf_wf_get_lut.sftemp, 4

	.type	.L.str.3,@object
	.section	.rodata.str1.1,"aMS",@progbits,1
.L.str.3:
	.asciz	"rkf: not support 5bit waveform\n"
	.size	.L.str.3, 32

	.type	.L.str.4,@object
.L.str.4:
	.asciz	"rkf waveform get frame number failed\n"
	.size	.L.str.4, 38

	.type	.L.str.5,@object
.L.str.5:
	.asciz	"rkf waveform"
	.size	.L.str.5, 13

	.type	.L.str.6,@object
.L.str.6:
	.asciz	"rkf: check format failed\n"
	.size	.L.str.6, 26

	.type	.L.str.7,@object
.L.str.7:
	.asciz	"[EINK]: waveform crc err readcrc = %x crccheck = %x\n"
	.size	.L.str.7, 53

	.type	crc32_table,@object
	.local	crc32_table
	.comm	crc32_table,1024,4
	.section	.debug_loc,"",@progbits
.Ldebug_loc0:
	.xword	.Lfunc_begin0-.Lfunc_begin0
	.xword	.Ltmp3-.Lfunc_begin0
	.hword	1
	.byte	80
	.xword	.Ltmp4-.Lfunc_begin0
	.xword	.Ltmp5-.Lfunc_begin0
	.hword	1
	.byte	80
	.xword	.Ltmp5-.Lfunc_begin0
	.xword	.Lfunc_end0-.Lfunc_begin0
	.hword	1
	.byte	99
	.xword	0
	.xword	0
.Ldebug_loc1:
	.xword	.Ltmp0-.Lfunc_begin0
	.xword	.Ltmp1-.Lfunc_begin0
	.hword	1
	.byte	80
	.xword	.Ltmp1-.Lfunc_begin0
	.xword	.Lfunc_end0-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc2:
	.xword	.Ltmp1-.Lfunc_begin0
	.xword	.Ltmp3-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	0
	.byte	159
	.xword	.Ltmp4-.Lfunc_begin0
	.xword	.Ltmp8-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	0
	.byte	159
	.xword	.Ltmp8-.Lfunc_begin0
	.xword	.Ltmp44-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	127
	.byte	159
	.xword	.Ltmp44-.Lfunc_begin0
	.xword	.Ltmp48-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	0
	.byte	159
	.xword	.Ltmp48-.Lfunc_begin0
	.xword	.Ltmp50-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	127
	.byte	159
	.xword	.Ltmp50-.Lfunc_begin0
	.xword	.Ltmp51-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	0
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc3:
	.xword	.Ltmp6-.Lfunc_begin0
	.xword	.Ltmp7-.Lfunc_begin0
	.hword	1
	.byte	80
	.xword	0
	.xword	0
.Ldebug_loc4:
	.xword	.Ltmp8-.Lfunc_begin0
	.xword	.Ltmp44-.Lfunc_begin0
	.hword	1
	.byte	99
	.xword	.Ltmp49-.Lfunc_begin0
	.xword	.Ltmp50-.Lfunc_begin0
	.hword	1
	.byte	99
	.xword	0
	.xword	0
.Ldebug_loc5:
	.xword	.Ltmp8-.Lfunc_begin0
	.xword	.Ltmp43-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	.Ltmp43-.Lfunc_begin0
	.xword	.Ltmp44-.Lfunc_begin0
	.hword	1
	.byte	82
	.xword	.Ltmp49-.Lfunc_begin0
	.xword	.Ltmp50-.Lfunc_begin0
	.hword	1
	.byte	82
	.xword	0
	.xword	0
.Ldebug_loc6:
	.xword	.Ltmp8-.Lfunc_begin0
	.xword	.Ltmp13-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	.Ltmp13-.Lfunc_begin0
	.xword	.Ltmp44-.Lfunc_begin0
	.hword	1
	.byte	100
	.xword	.Ltmp49-.Lfunc_begin0
	.xword	.Ltmp50-.Lfunc_begin0
	.hword	1
	.byte	100
	.xword	0
	.xword	0
.Ldebug_loc7:
	.xword	.Ltmp14-.Lfunc_begin0
	.xword	.Ltmp40-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	.Ltmp40-.Lfunc_begin0
	.xword	.Ltmp43-.Lfunc_begin0
	.hword	1
	.byte	82
	.xword	0
	.xword	0
.Ldebug_loc8:
	.xword	.Ltmp14-.Lfunc_begin0
	.xword	.Ltmp40-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc9:
	.xword	.Ltmp14-.Lfunc_begin0
	.xword	.Ltmp40-.Lfunc_begin0
	.hword	1
	.byte	103
	.xword	0
	.xword	0
.Ldebug_loc10:
	.xword	.Ltmp14-.Lfunc_begin0
	.xword	.Ltmp40-.Lfunc_begin0
	.hword	1
	.byte	99
	.xword	.Ltmp40-.Lfunc_begin0
	.xword	.Ltmp43-.Lfunc_begin0
	.hword	1
	.byte	88
	.xword	0
	.xword	0
.Ldebug_loc11:
	.xword	.Ltmp16-.Lfunc_begin0
	.xword	.Ltmp17-.Lfunc_begin0
	.hword	5
	.byte	124
	.byte	0
	.byte	72
	.byte	36
	.byte	159
	.xword	.Ltmp17-.Lfunc_begin0
	.xword	.Ltmp18-.Lfunc_begin0
	.hword	5
	.byte	124
	.byte	0
	.byte	75
	.byte	36
	.byte	159
	.xword	.Ltmp18-.Lfunc_begin0
	.xword	.Ltmp21-.Lfunc_begin0
	.hword	5
	.byte	124
	.byte	0
	.byte	76
	.byte	36
	.byte	159
	.xword	.Ltmp21-.Lfunc_begin0
	.xword	.Ltmp25-.Lfunc_begin0
	.hword	5
	.byte	124
	.byte	0
	.byte	77
	.byte	36
	.byte	159
	.xword	.Ltmp25-.Lfunc_begin0
	.xword	.Ltmp29-.Lfunc_begin0
	.hword	5
	.byte	124
	.byte	0
	.byte	78
	.byte	36
	.byte	159
	.xword	.Ltmp29-.Lfunc_begin0
	.xword	.Ltmp33-.Lfunc_begin0
	.hword	5
	.byte	124
	.byte	0
	.byte	79
	.byte	36
	.byte	159
	.xword	.Ltmp33-.Lfunc_begin0
	.xword	.Ltmp36-.Lfunc_begin0
	.hword	7
	.byte	124
	.byte	0
	.byte	79
	.byte	36
	.byte	49
	.byte	36
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc12:
	.xword	.Ltmp16-.Lfunc_begin0
	.xword	.Ltmp17-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	.Ltmp18-.Lfunc_begin0
	.xword	.Ltmp20-.Lfunc_begin0
	.hword	1
	.byte	82
	.xword	.Ltmp21-.Lfunc_begin0
	.xword	.Ltmp24-.Lfunc_begin0
	.hword	1
	.byte	82
	.xword	.Ltmp25-.Lfunc_begin0
	.xword	.Ltmp28-.Lfunc_begin0
	.hword	1
	.byte	82
	.xword	.Ltmp29-.Lfunc_begin0
	.xword	.Ltmp32-.Lfunc_begin0
	.hword	1
	.byte	82
	.xword	.Ltmp33-.Lfunc_begin0
	.xword	.Ltmp37-.Lfunc_begin0
	.hword	1
	.byte	82
	.xword	0
	.xword	0
.Ldebug_loc13:
	.xword	.Ltmp16-.Lfunc_begin0
	.xword	.Ltmp17-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	.Ltmp17-.Lfunc_begin0
	.xword	.Ltmp18-.Lfunc_begin0
	.hword	2
	.byte	51
	.byte	159
	.xword	.Ltmp18-.Lfunc_begin0
	.xword	.Ltmp21-.Lfunc_begin0
	.hword	2
	.byte	52
	.byte	159
	.xword	.Ltmp21-.Lfunc_begin0
	.xword	.Ltmp25-.Lfunc_begin0
	.hword	2
	.byte	53
	.byte	159
	.xword	.Ltmp25-.Lfunc_begin0
	.xword	.Ltmp29-.Lfunc_begin0
	.hword	2
	.byte	54
	.byte	159
	.xword	.Ltmp29-.Lfunc_begin0
	.xword	.Ltmp33-.Lfunc_begin0
	.hword	2
	.byte	55
	.byte	159
	.xword	.Ltmp33-.Lfunc_begin0
	.xword	.Ltmp37-.Lfunc_begin0
	.hword	2
	.byte	56
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc14:
	.xword	.Lfunc_begin2-.Lfunc_begin0
	.xword	.Ltmp59-.Lfunc_begin0
	.hword	1
	.byte	80
	.xword	.Ltmp59-.Lfunc_begin0
	.xword	.Ltmp110-.Lfunc_begin0
	.hword	1
	.byte	100
	.xword	.Ltmp110-.Lfunc_begin0
	.xword	.Ltmp111-.Lfunc_begin0
	.hword	1
	.byte	80
	.xword	.Ltmp112-.Lfunc_begin0
	.xword	.Ltmp129-.Lfunc_begin0
	.hword	1
	.byte	100
	.xword	0
	.xword	0
.Ldebug_loc15:
	.xword	.Lfunc_begin2-.Lfunc_begin0
	.xword	.Ltmp65-.Lfunc_begin0
	.hword	1
	.byte	81
	.xword	.Ltmp65-.Lfunc_begin0
	.xword	.Ltmp97-.Lfunc_begin0
	.hword	1
	.byte	101
	.xword	.Ltmp110-.Lfunc_begin0
	.xword	.Ltmp113-.Lfunc_begin0
	.hword	1
	.byte	81
	.xword	.Ltmp124-.Lfunc_begin0
	.xword	.Ltmp125-.Lfunc_begin0
	.hword	1
	.byte	81
	.xword	.Ltmp125-.Lfunc_begin0
	.xword	.Ltmp126-.Lfunc_begin0
	.hword	4
	.byte	243
	.byte	1
	.byte	81
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc16:
	.xword	.Lfunc_begin2-.Lfunc_begin0
	.xword	.Ltmp63-.Lfunc_begin0
	.hword	1
	.byte	82
	.xword	.Ltmp63-.Lfunc_begin0
	.xword	.Ltmp110-.Lfunc_begin0
	.hword	1
	.byte	99
	.xword	.Ltmp110-.Lfunc_begin0
	.xword	.Ltmp113-.Lfunc_begin0
	.hword	1
	.byte	82
	.xword	.Ltmp113-.Lfunc_begin0
	.xword	.Ltmp124-.Lfunc_begin0
	.hword	1
	.byte	99
	.xword	.Ltmp124-.Lfunc_begin0
	.xword	.Ltmp125-.Lfunc_begin0
	.hword	1
	.byte	82
	.xword	.Ltmp126-.Lfunc_begin0
	.xword	.Ltmp129-.Lfunc_begin0
	.hword	1
	.byte	99
	.xword	0
	.xword	0
.Ldebug_loc17:
	.xword	.Lfunc_begin2-.Lfunc_begin0
	.xword	.Ltmp72-.Lfunc_begin0
	.hword	1
	.byte	83
	.xword	.Ltmp72-.Lfunc_begin0
	.xword	.Ltmp73-.Lfunc_begin0
	.hword	4
	.byte	243
	.byte	1
	.byte	83
	.byte	159
	.xword	.Ltmp110-.Lfunc_begin0
	.xword	.Ltmp113-.Lfunc_begin0
	.hword	1
	.byte	83
	.xword	.Ltmp124-.Lfunc_begin0
	.xword	.Ltmp125-.Lfunc_begin0
	.hword	1
	.byte	83
	.xword	.Ltmp125-.Lfunc_begin0
	.xword	.Ltmp126-.Lfunc_begin0
	.hword	4
	.byte	243
	.byte	1
	.byte	83
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc18:
	.xword	.Ltmp57-.Lfunc_begin0
	.xword	.Ltmp74-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	0
	.byte	159
	.xword	.Ltmp74-.Lfunc_begin0
	.xword	.Ltmp76-.Lfunc_begin0
	.hword	1
	.byte	80
	.xword	.Ltmp76-.Lfunc_begin0
	.xword	.Ltmp98-.Lfunc_begin0
	.hword	1
	.byte	102
	.xword	.Ltmp104-.Lfunc_begin0
	.xword	.Ltmp108-.Lfunc_begin0
	.hword	1
	.byte	101
	.xword	.Ltmp110-.Lfunc_begin0
	.xword	.Ltmp113-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	0
	.byte	159
	.xword	.Ltmp120-.Lfunc_begin0
	.xword	.Ltmp122-.Lfunc_begin0
	.hword	1
	.byte	80
	.xword	.Ltmp122-.Lfunc_begin0
	.xword	.Ltmp124-.Lfunc_begin0
	.hword	1
	.byte	101
	.xword	.Ltmp124-.Lfunc_begin0
	.xword	.Ltmp126-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	0
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc19:
	.xword	.Ltmp57-.Lfunc_begin0
	.xword	.Ltmp119-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	0
	.byte	159
	.xword	.Ltmp119-.Lfunc_begin0
	.xword	.Ltmp120-.Lfunc_begin0
	.hword	1
	.byte	81
	.xword	.Ltmp124-.Lfunc_begin0
	.xword	.Ltmp127-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	0
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc20:
	.xword	.Ltmp83-.Lfunc_begin0
	.xword	.Ltmp87-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	0
	.byte	159
	.xword	.Ltmp87-.Lfunc_begin0
	.xword	.Ltmp91-.Lfunc_begin0
	.hword	1
	.byte	89
	.xword	0
	.xword	0
.Ldebug_loc21:
	.xword	.Ltmp83-.Lfunc_begin0
	.xword	.Ltmp84-.Lfunc_begin0
	.hword	1
	.byte	99
	.xword	.Ltmp84-.Lfunc_begin0
	.xword	.Ltmp91-.Lfunc_begin0
	.hword	1
	.byte	90
	.xword	0
	.xword	0
.Ldebug_loc22:
	.xword	.Ltmp83-.Lfunc_begin0
	.xword	.Ltmp96-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	.Ltmp96-.Lfunc_begin0
	.xword	.Ltmp100-.Lfunc_begin0
	.hword	1
	.byte	106
	.xword	0
	.xword	0
.Ldebug_loc23:
	.xword	.Ltmp83-.Lfunc_begin0
	.xword	.Ltmp92-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc24:
	.xword	.Ltmp83-.Lfunc_begin0
	.xword	.Ltmp95-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc25:
	.xword	.Ltmp83-.Lfunc_begin0
	.xword	.Ltmp99-.Lfunc_begin0
	.hword	4
	.byte	120
	.byte	212
	.byte	16
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc26:
	.xword	.Ltmp83-.Lfunc_begin0
	.xword	.Ltmp99-.Lfunc_begin0
	.hword	4
	.byte	120
	.byte	148
	.byte	4
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc27:
	.xword	.Ltmp83-.Lfunc_begin0
	.xword	.Ltmp85-.Lfunc_begin0
	.hword	1
	.byte	88
	.xword	.Ltmp85-.Lfunc_begin0
	.xword	.Ltmp100-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc28:
	.xword	.Ltmp85-.Lfunc_begin0
	.xword	.Ltmp100-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	6
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc29:
	.xword	.Ltmp85-.Lfunc_begin0
	.xword	.Ltmp100-.Lfunc_begin0
	.hword	1
	.byte	99
	.xword	0
	.xword	0
.Ldebug_loc30:
	.xword	.Ltmp85-.Lfunc_begin0
	.xword	.Ltmp98-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	.Ltmp98-.Lfunc_begin0
	.xword	.Ltmp108-.Lfunc_begin0
	.hword	1
	.byte	102
	.xword	.Ltmp126-.Lfunc_begin0
	.xword	.Ltmp127-.Lfunc_begin0
	.hword	1
	.byte	102
	.xword	0
	.xword	0
.Ldebug_loc31:
	.xword	.Ltmp85-.Lfunc_begin0
	.xword	.Ltmp97-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	.Ltmp97-.Lfunc_begin0
	.xword	.Ltmp108-.Lfunc_begin0
	.hword	1
	.byte	101
	.xword	.Ltmp126-.Lfunc_begin0
	.xword	.Ltmp127-.Lfunc_begin0
	.hword	1
	.byte	101
	.xword	0
	.xword	0
.Ldebug_loc32:
	.xword	.Ltmp85-.Lfunc_begin0
	.xword	.Ltmp96-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	.Ltmp96-.Lfunc_begin0
	.xword	.Ltmp108-.Lfunc_begin0
	.hword	1
	.byte	106
	.xword	.Ltmp126-.Lfunc_begin0
	.xword	.Ltmp127-.Lfunc_begin0
	.hword	1
	.byte	106
	.xword	0
	.xword	0
.Ldebug_loc33:
	.xword	.Ltmp85-.Lfunc_begin0
	.xword	.Ltmp108-.Lfunc_begin0
	.hword	1
	.byte	99
	.xword	.Ltmp126-.Lfunc_begin0
	.xword	.Ltmp127-.Lfunc_begin0
	.hword	1
	.byte	99
	.xword	0
	.xword	0
.Ldebug_loc34:
	.xword	.Ltmp85-.Lfunc_begin0
	.xword	.Ltmp108-.Lfunc_begin0
	.hword	1
	.byte	100
	.xword	.Ltmp126-.Lfunc_begin0
	.xword	.Ltmp127-.Lfunc_begin0
	.hword	1
	.byte	100
	.xword	0
	.xword	0
.Ldebug_loc35:
	.xword	.Ltmp85-.Lfunc_begin0
	.xword	.Ltmp104-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	127
	.byte	159
	.xword	.Ltmp126-.Lfunc_begin0
	.xword	.Ltmp127-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	127
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc36:
	.xword	.Ltmp85-.Lfunc_begin0
	.xword	.Ltmp108-.Lfunc_begin0
	.hword	1
	.byte	99
	.xword	.Ltmp126-.Lfunc_begin0
	.xword	.Ltmp127-.Lfunc_begin0
	.hword	1
	.byte	99
	.xword	0
	.xword	0
.Ldebug_loc37:
	.xword	.Ltmp85-.Lfunc_begin0
	.xword	.Ltmp108-.Lfunc_begin0
	.hword	2
	.byte	55
	.byte	159
	.xword	.Ltmp126-.Lfunc_begin0
	.xword	.Ltmp127-.Lfunc_begin0
	.hword	2
	.byte	55
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc38:
	.xword	.Ltmp85-.Lfunc_begin0
	.xword	.Ltmp108-.Lfunc_begin0
	.hword	1
	.byte	100
	.xword	.Ltmp126-.Lfunc_begin0
	.xword	.Ltmp127-.Lfunc_begin0
	.hword	1
	.byte	100
	.xword	0
	.xword	0
.Ldebug_loc39:
	.xword	.Ltmp98-.Lfunc_begin0
	.xword	.Ltmp100-.Lfunc_begin0
	.hword	4
	.byte	16
	.byte	192
	.byte	27
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc40:
	.xword	.Ltmp98-.Lfunc_begin0
	.xword	.Ltmp100-.Lfunc_begin0
	.hword	4
	.byte	16
	.byte	192
	.byte	25
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc41:
	.xword	.Ltmp98-.Lfunc_begin0
	.xword	.Ltmp99-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	.Ltmp99-.Lfunc_begin0
	.xword	.Ltmp101-.Lfunc_begin0
	.hword	1
	.byte	80
	.xword	.Ltmp101-.Lfunc_begin0
	.xword	.Ltmp108-.Lfunc_begin0
	.hword	1
	.byte	103
	.xword	0
	.xword	0
.Ldebug_loc42:
	.xword	.Ltmp98-.Lfunc_begin0
	.xword	.Ltmp108-.Lfunc_begin0
	.hword	1
	.byte	106
	.xword	0
	.xword	0
.Ldebug_loc43:
	.xword	.Ltmp98-.Lfunc_begin0
	.xword	.Ltmp100-.Lfunc_begin0
	.hword	1
	.byte	102
	.xword	0
	.xword	0
.Ldebug_loc44:
	.xword	.Ltmp98-.Lfunc_begin0
	.xword	.Ltmp100-.Lfunc_begin0
	.hword	1
	.byte	102
	.xword	0
	.xword	0
.Ldebug_loc45:
	.xword	.Ltmp98-.Lfunc_begin0
	.xword	.Ltmp108-.Lfunc_begin0
	.hword	1
	.byte	102
	.xword	0
	.xword	0
.Ldebug_loc46:
	.xword	.Lfunc_begin3-.Lfunc_begin0
	.xword	.Ltmp133-.Lfunc_begin0
	.hword	1
	.byte	80
	.xword	.Ltmp133-.Lfunc_begin0
	.xword	.Ltmp200-.Lfunc_begin0
	.hword	1
	.byte	99
	.xword	.Ltmp201-.Lfunc_begin0
	.xword	.Ltmp299-.Lfunc_begin0
	.hword	1
	.byte	99
	.xword	.Ltmp300-.Lfunc_begin0
	.xword	.Lfunc_end3-.Lfunc_begin0
	.hword	1
	.byte	99
	.xword	0
	.xword	0
.Ldebug_loc47:
	.xword	.Lfunc_begin3-.Lfunc_begin0
	.xword	.Ltmp147-.Lfunc_begin0
	.hword	1
	.byte	81
	.xword	.Ltmp147-.Lfunc_begin0
	.xword	.Ltmp200-.Lfunc_begin0
	.hword	4
	.byte	243
	.byte	1
	.byte	81
	.byte	159
	.xword	.Ltmp201-.Lfunc_begin0
	.xword	.Ltmp289-.Lfunc_begin0
	.hword	1
	.byte	81
	.xword	.Ltmp289-.Lfunc_begin0
	.xword	.Ltmp299-.Lfunc_begin0
	.hword	4
	.byte	243
	.byte	1
	.byte	81
	.byte	159
	.xword	.Ltmp300-.Lfunc_begin0
	.xword	.Lfunc_end3-.Lfunc_begin0
	.hword	4
	.byte	243
	.byte	1
	.byte	81
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc48:
	.xword	.Lfunc_begin3-.Lfunc_begin0
	.xword	.Ltmp153-.Lfunc_begin0
	.hword	1
	.byte	82
	.xword	.Ltmp153-.Lfunc_begin0
	.xword	.Ltmp200-.Lfunc_begin0
	.hword	4
	.byte	243
	.byte	1
	.byte	82
	.byte	159
	.xword	.Ltmp201-.Lfunc_begin0
	.xword	.Ltmp291-.Lfunc_begin0
	.hword	1
	.byte	82
	.xword	.Ltmp291-.Lfunc_begin0
	.xword	.Ltmp299-.Lfunc_begin0
	.hword	4
	.byte	243
	.byte	1
	.byte	82
	.byte	159
	.xword	.Ltmp300-.Lfunc_begin0
	.xword	.Lfunc_end3-.Lfunc_begin0
	.hword	4
	.byte	243
	.byte	1
	.byte	82
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc49:
	.xword	.Ltmp131-.Lfunc_begin0
	.xword	.Ltmp199-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	127
	.byte	159
	.xword	.Ltmp199-.Lfunc_begin0
	.xword	.Ltmp200-.Lfunc_begin0
	.hword	1
	.byte	100
	.xword	.Ltmp200-.Lfunc_begin0
	.xword	.Ltmp296-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	127
	.byte	159
	.xword	.Ltmp297-.Lfunc_begin0
	.xword	.Ltmp298-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	127
	.byte	159
	.xword	.Ltmp300-.Lfunc_begin0
	.xword	.Ltmp302-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	127
	.byte	159
	.xword	.Ltmp302-.Lfunc_begin0
	.xword	.Lfunc_end3-.Lfunc_begin0
	.hword	1
	.byte	100
	.xword	0
	.xword	0
.Ldebug_loc50:
	.xword	.Ltmp136-.Lfunc_begin0
	.xword	.Ltmp138-.Lfunc_begin0
	.hword	1
	.byte	90
	.xword	.Ltmp139-.Lfunc_begin0
	.xword	.Ltmp144-.Lfunc_begin0
	.hword	1
	.byte	82
	.xword	0
	.xword	0
.Ldebug_loc51:
	.xword	.Ltmp137-.Lfunc_begin0
	.xword	.Ltmp139-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	.Ltmp139-.Lfunc_begin0
	.xword	.Ltmp151-.Lfunc_begin0
	.hword	1
	.byte	88
	.xword	0
	.xword	0
.Ldebug_loc52:
	.xword	.Ltmp137-.Lfunc_begin0
	.xword	.Ltmp154-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	0
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc53:
	.xword	.Ltmp137-.Lfunc_begin0
	.xword	.Ltmp153-.Lfunc_begin0
	.hword	1
	.byte	82
	.xword	0
	.xword	0
.Ldebug_loc54:
	.xword	.Ltmp137-.Lfunc_begin0
	.xword	.Ltmp152-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	.Ltmp152-.Lfunc_begin0
	.xword	.Ltmp200-.Lfunc_begin0
	.hword	1
	.byte	102
	.xword	.Ltmp300-.Lfunc_begin0
	.xword	.Lfunc_end3-.Lfunc_begin0
	.hword	1
	.byte	102
	.xword	0
	.xword	0
.Ldebug_loc55:
	.xword	.Ltmp137-.Lfunc_begin0
	.xword	.Ltmp150-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	.Ltmp150-.Lfunc_begin0
	.xword	.Ltmp200-.Lfunc_begin0
	.hword	1
	.byte	100
	.xword	.Ltmp300-.Lfunc_begin0
	.xword	.Ltmp302-.Lfunc_begin0
	.hword	1
	.byte	100
	.xword	0
	.xword	0
.Ldebug_loc56:
	.xword	.Ltmp137-.Lfunc_begin0
	.xword	.Ltmp149-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	.Ltmp149-.Lfunc_begin0
	.xword	.Ltmp200-.Lfunc_begin0
	.hword	1
	.byte	104
	.xword	.Ltmp300-.Lfunc_begin0
	.xword	.Lfunc_end3-.Lfunc_begin0
	.hword	1
	.byte	104
	.xword	0
	.xword	0
.Ldebug_loc57:
	.xword	.Ltmp137-.Lfunc_begin0
	.xword	.Ltmp156-.Lfunc_begin0
	.hword	12
	.byte	48
	.byte	159
	.byte	147
	.byte	4
	.byte	48
	.byte	159
	.byte	147
	.byte	4
	.byte	48
	.byte	159
	.byte	147
	.byte	8
	.xword	.Ltmp156-.Lfunc_begin0
	.xword	.Ltmp200-.Lfunc_begin0
	.hword	10
	.byte	100
	.byte	147
	.byte	4
	.byte	48
	.byte	159
	.byte	147
	.byte	4
	.byte	101
	.byte	147
	.byte	8
	.xword	.Ltmp300-.Lfunc_begin0
	.xword	.Ltmp302-.Lfunc_begin0
	.hword	10
	.byte	100
	.byte	147
	.byte	4
	.byte	48
	.byte	159
	.byte	147
	.byte	4
	.byte	101
	.byte	147
	.byte	8
	.xword	.Ltmp302-.Lfunc_begin0
	.xword	.Lfunc_end3-.Lfunc_begin0
	.hword	9
	.byte	147
	.byte	4
	.byte	48
	.byte	159
	.byte	147
	.byte	4
	.byte	101
	.byte	147
	.byte	8
	.xword	0
	.xword	0
.Ldebug_loc58:
	.xword	.Ltmp137-.Lfunc_begin0
	.xword	.Ltmp153-.Lfunc_begin0
	.hword	1
	.byte	82
	.xword	0
	.xword	0
.Ldebug_loc59:
	.xword	.Ltmp137-.Lfunc_begin0
	.xword	.Ltmp200-.Lfunc_begin0
	.hword	1
	.byte	99
	.xword	.Ltmp300-.Lfunc_begin0
	.xword	.Lfunc_end3-.Lfunc_begin0
	.hword	1
	.byte	99
	.xword	0
	.xword	0
.Ldebug_loc60:
	.xword	.Ltmp139-.Lfunc_begin0
	.xword	.Ltmp140-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	0
	.byte	159
	.xword	.Ltmp140-.Lfunc_begin0
	.xword	.Ltmp144-.Lfunc_begin0
	.hword	1
	.byte	89
	.xword	0
	.xword	0
.Ldebug_loc61:
	.xword	.Ltmp139-.Lfunc_begin0
	.xword	.Ltmp144-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	0
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc62:
	.xword	.Ltmp139-.Lfunc_begin0
	.xword	.Ltmp149-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	.Ltmp149-.Lfunc_begin0
	.xword	.Ltmp154-.Lfunc_begin0
	.hword	1
	.byte	104
	.xword	0
	.xword	0
.Ldebug_loc63:
	.xword	.Ltmp139-.Lfunc_begin0
	.xword	.Ltmp145-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc64:
	.xword	.Ltmp139-.Lfunc_begin0
	.xword	.Ltmp148-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc65:
	.xword	.Ltmp139-.Lfunc_begin0
	.xword	.Ltmp144-.Lfunc_begin0
	.hword	4
	.byte	120
	.byte	148
	.byte	1
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc66:
	.xword	.Ltmp139-.Lfunc_begin0
	.xword	.Ltmp154-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc67:
	.xword	.Ltmp139-.Lfunc_begin0
	.xword	.Ltmp154-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc68:
	.xword	.Ltmp152-.Lfunc_begin0
	.xword	.Ltmp154-.Lfunc_begin0
	.hword	4
	.byte	16
	.byte	192
	.byte	27
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc69:
	.xword	.Ltmp152-.Lfunc_begin0
	.xword	.Ltmp154-.Lfunc_begin0
	.hword	4
	.byte	16
	.byte	192
	.byte	25
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc70:
	.xword	.Ltmp152-.Lfunc_begin0
	.xword	.Ltmp153-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	.Ltmp153-.Lfunc_begin0
	.xword	.Ltmp155-.Lfunc_begin0
	.hword	1
	.byte	80
	.xword	.Ltmp155-.Lfunc_begin0
	.xword	.Ltmp161-.Lfunc_begin0
	.hword	1
	.byte	101
	.xword	0
	.xword	0
.Ldebug_loc71:
	.xword	.Ltmp152-.Lfunc_begin0
	.xword	.Ltmp161-.Lfunc_begin0
	.hword	1
	.byte	104
	.xword	0
	.xword	0
.Ldebug_loc72:
	.xword	.Ltmp152-.Lfunc_begin0
	.xword	.Ltmp154-.Lfunc_begin0
	.hword	1
	.byte	102
	.xword	0
	.xword	0
.Ldebug_loc73:
	.xword	.Ltmp152-.Lfunc_begin0
	.xword	.Ltmp154-.Lfunc_begin0
	.hword	1
	.byte	102
	.xword	0
	.xword	0
.Ldebug_loc74:
	.xword	.Ltmp152-.Lfunc_begin0
	.xword	.Ltmp161-.Lfunc_begin0
	.hword	1
	.byte	102
	.xword	0
	.xword	0
.Ldebug_loc75:
	.xword	.Ltmp157-.Lfunc_begin0
	.xword	.Ltmp159-.Lfunc_begin0
	.hword	15
	.byte	120
	.byte	0
	.byte	16
	.byte	128
	.byte	248
	.byte	1
	.byte	26
	.byte	16
	.byte	255
	.byte	255
	.byte	255
	.byte	255
	.byte	15
	.byte	26
	.byte	159
	.xword	.Ltmp159-.Lfunc_begin0
	.xword	.Ltmp160-.Lfunc_begin0
	.hword	1
	.byte	80
	.xword	0
	.xword	0
.Ldebug_loc76:
	.xword	.Ltmp157-.Lfunc_begin0
	.xword	.Ltmp159-.Lfunc_begin0
	.hword	15
	.byte	120
	.byte	0
	.byte	16
	.byte	128
	.byte	248
	.byte	1
	.byte	26
	.byte	16
	.byte	255
	.byte	255
	.byte	255
	.byte	255
	.byte	15
	.byte	26
	.byte	159
	.xword	.Ltmp159-.Lfunc_begin0
	.xword	.Ltmp160-.Lfunc_begin0
	.hword	1
	.byte	80
	.xword	0
	.xword	0
.Ldebug_loc77:
	.xword	.Ltmp163-.Lfunc_begin0
	.xword	.Ltmp165-.Lfunc_begin0
	.hword	1
	.byte	91
	.xword	0
	.xword	0
.Ldebug_loc78:
	.xword	.Ltmp156-.Lfunc_begin0
	.xword	.Ltmp166-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	0
	.byte	159
	.xword	.Ltmp166-.Lfunc_begin0
	.xword	.Ltmp168-.Lfunc_begin0
	.hword	1
	.byte	88
	.xword	.Ltmp168-.Lfunc_begin0
	.xword	.Ltmp172-.Lfunc_begin0
	.hword	12
	.byte	126
	.byte	0
	.byte	17
	.byte	16
	.byte	27
	.byte	17
	.byte	16
	.byte	30
	.byte	120
	.byte	0
	.byte	34
	.byte	159
	.xword	.Ltmp179-.Lfunc_begin0
	.xword	.Ltmp180-.Lfunc_begin0
	.hword	15
	.byte	126
	.byte	0
	.byte	17
	.byte	16
	.byte	27
	.byte	17
	.byte	16
	.byte	30
	.byte	17
	.byte	2
	.byte	120
	.byte	0
	.byte	34
	.byte	34
	.byte	159
	.xword	.Ltmp180-.Lfunc_begin0
	.xword	.Ltmp181-.Lfunc_begin0
	.hword	15
	.byte	126
	.byte	0
	.byte	17
	.byte	16
	.byte	27
	.byte	17
	.byte	16
	.byte	30
	.byte	17
	.byte	3
	.byte	120
	.byte	0
	.byte	34
	.byte	34
	.byte	159
	.xword	.Ltmp181-.Lfunc_begin0
	.xword	.Ltmp182-.Lfunc_begin0
	.hword	15
	.byte	126
	.byte	0
	.byte	17
	.byte	16
	.byte	27
	.byte	17
	.byte	16
	.byte	30
	.byte	17
	.byte	4
	.byte	120
	.byte	0
	.byte	34
	.byte	34
	.byte	159
	.xword	.Ltmp182-.Lfunc_begin0
	.xword	.Ltmp183-.Lfunc_begin0
	.hword	15
	.byte	126
	.byte	0
	.byte	17
	.byte	16
	.byte	27
	.byte	17
	.byte	16
	.byte	30
	.byte	17
	.byte	5
	.byte	120
	.byte	0
	.byte	34
	.byte	34
	.byte	159
	.xword	.Ltmp183-.Lfunc_begin0
	.xword	.Ltmp184-.Lfunc_begin0
	.hword	15
	.byte	126
	.byte	0
	.byte	17
	.byte	16
	.byte	27
	.byte	17
	.byte	16
	.byte	30
	.byte	17
	.byte	6
	.byte	120
	.byte	0
	.byte	34
	.byte	34
	.byte	159
	.xword	.Ltmp184-.Lfunc_begin0
	.xword	.Ltmp185-.Lfunc_begin0
	.hword	15
	.byte	126
	.byte	0
	.byte	17
	.byte	16
	.byte	27
	.byte	17
	.byte	16
	.byte	30
	.byte	17
	.byte	7
	.byte	120
	.byte	0
	.byte	34
	.byte	34
	.byte	159
	.xword	.Ltmp185-.Lfunc_begin0
	.xword	.Ltmp186-.Lfunc_begin0
	.hword	15
	.byte	126
	.byte	0
	.byte	17
	.byte	16
	.byte	27
	.byte	17
	.byte	16
	.byte	30
	.byte	17
	.byte	8
	.byte	120
	.byte	0
	.byte	34
	.byte	34
	.byte	159
	.xword	.Ltmp186-.Lfunc_begin0
	.xword	.Ltmp187-.Lfunc_begin0
	.hword	15
	.byte	126
	.byte	0
	.byte	17
	.byte	16
	.byte	27
	.byte	17
	.byte	16
	.byte	30
	.byte	17
	.byte	9
	.byte	120
	.byte	0
	.byte	34
	.byte	34
	.byte	159
	.xword	.Ltmp187-.Lfunc_begin0
	.xword	.Ltmp188-.Lfunc_begin0
	.hword	15
	.byte	126
	.byte	0
	.byte	17
	.byte	16
	.byte	27
	.byte	17
	.byte	16
	.byte	30
	.byte	17
	.byte	10
	.byte	120
	.byte	0
	.byte	34
	.byte	34
	.byte	159
	.xword	.Ltmp188-.Lfunc_begin0
	.xword	.Ltmp189-.Lfunc_begin0
	.hword	15
	.byte	126
	.byte	0
	.byte	17
	.byte	16
	.byte	27
	.byte	17
	.byte	16
	.byte	30
	.byte	17
	.byte	11
	.byte	120
	.byte	0
	.byte	34
	.byte	34
	.byte	159
	.xword	.Ltmp189-.Lfunc_begin0
	.xword	.Ltmp190-.Lfunc_begin0
	.hword	15
	.byte	126
	.byte	0
	.byte	17
	.byte	16
	.byte	27
	.byte	17
	.byte	16
	.byte	30
	.byte	17
	.byte	12
	.byte	120
	.byte	0
	.byte	34
	.byte	34
	.byte	159
	.xword	.Ltmp190-.Lfunc_begin0
	.xword	.Ltmp191-.Lfunc_begin0
	.hword	15
	.byte	126
	.byte	0
	.byte	17
	.byte	16
	.byte	27
	.byte	17
	.byte	16
	.byte	30
	.byte	17
	.byte	1
	.byte	120
	.byte	0
	.byte	34
	.byte	34
	.byte	159
	.xword	.Ltmp191-.Lfunc_begin0
	.xword	.Ltmp192-.Lfunc_begin0
	.hword	15
	.byte	126
	.byte	0
	.byte	17
	.byte	16
	.byte	27
	.byte	17
	.byte	16
	.byte	30
	.byte	17
	.byte	13
	.byte	120
	.byte	0
	.byte	34
	.byte	34
	.byte	159
	.xword	.Ltmp192-.Lfunc_begin0
	.xword	.Ltmp193-.Lfunc_begin0
	.hword	15
	.byte	126
	.byte	0
	.byte	17
	.byte	16
	.byte	27
	.byte	17
	.byte	16
	.byte	30
	.byte	17
	.byte	14
	.byte	120
	.byte	0
	.byte	34
	.byte	34
	.byte	159
	.xword	.Ltmp193-.Lfunc_begin0
	.xword	.Ltmp194-.Lfunc_begin0
	.hword	15
	.byte	126
	.byte	0
	.byte	17
	.byte	16
	.byte	27
	.byte	17
	.byte	16
	.byte	30
	.byte	17
	.byte	15
	.byte	120
	.byte	0
	.byte	34
	.byte	34
	.byte	159
	.xword	.Ltmp194-.Lfunc_begin0
	.xword	.Ltmp197-.Lfunc_begin0
	.hword	15
	.byte	126
	.byte	0
	.byte	17
	.byte	16
	.byte	27
	.byte	17
	.byte	16
	.byte	30
	.byte	17
	.byte	16
	.byte	120
	.byte	0
	.byte	34
	.byte	34
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc79:
	.xword	.Ltmp156-.Lfunc_begin0
	.xword	.Ltmp166-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	0
	.byte	159
	.xword	.Ltmp168-.Lfunc_begin0
	.xword	.Ltmp198-.Lfunc_begin0
	.hword	1
	.byte	96
	.xword	0
	.xword	0
.Ldebug_loc80:
	.xword	.Ltmp156-.Lfunc_begin0
	.xword	.Ltmp166-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	0
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc81:
	.xword	.Ltmp156-.Lfunc_begin0
	.xword	.Ltmp166-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	0
	.byte	159
	.xword	.Ltmp169-.Lfunc_begin0
	.xword	.Ltmp178-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	15
	.byte	159
	.xword	.Ltmp178-.Lfunc_begin0
	.xword	.Ltmp198-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	16
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc82:
	.xword	.Ltmp156-.Lfunc_begin0
	.xword	.Ltmp166-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	0
	.byte	159
	.xword	.Ltmp168-.Lfunc_begin0
	.xword	.Ltmp172-.Lfunc_begin0
	.hword	6
	.byte	126
	.byte	0
	.byte	17
	.byte	16
	.byte	27
	.byte	159
	.xword	.Ltmp194-.Lfunc_begin0
	.xword	.Ltmp198-.Lfunc_begin0
	.hword	9
	.byte	126
	.byte	0
	.byte	17
	.byte	16
	.byte	27
	.byte	17
	.byte	1
	.byte	34
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc83:
	.xword	.Ltmp156-.Lfunc_begin0
	.xword	.Ltmp166-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	0
	.byte	159
	.xword	.Ltmp166-.Lfunc_begin0
	.xword	.Ltmp198-.Lfunc_begin0
	.hword	1
	.byte	89
	.xword	0
	.xword	0
.Ldebug_loc84:
	.xword	.Ltmp156-.Lfunc_begin0
	.xword	.Ltmp160-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	.Ltmp160-.Lfunc_begin0
	.xword	.Ltmp198-.Lfunc_begin0
	.hword	1
	.byte	80
	.xword	0
	.xword	0
.Ldebug_loc85:
	.xword	.Ltmp156-.Lfunc_begin0
	.xword	.Ltmp166-.Lfunc_begin0
	.hword	1
	.byte	101
	.xword	0
	.xword	0
.Ldebug_loc86:
	.xword	.Ltmp159-.Lfunc_begin0
	.xword	.Ltmp161-.Lfunc_begin0
	.hword	4
	.byte	16
	.byte	192
	.byte	27
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc87:
	.xword	.Ltmp159-.Lfunc_begin0
	.xword	.Ltmp161-.Lfunc_begin0
	.hword	4
	.byte	16
	.byte	192
	.byte	25
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc88:
	.xword	.Ltmp168-.Lfunc_begin0
	.xword	.Ltmp169-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	0
	.byte	159
	.xword	.Ltmp169-.Lfunc_begin0
	.xword	.Ltmp170-.Lfunc_begin0
	.hword	5
	.byte	129
	.byte	0
	.byte	51
	.byte	26
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc89:
	.xword	.Ltmp178-.Lfunc_begin0
	.xword	.Ltmp195-.Lfunc_begin0
	.hword	1
	.byte	97
	.xword	0
	.xword	0
.Ldebug_loc90:
	.xword	.Ltmp203-.Lfunc_begin0
	.xword	.Ltmp205-.Lfunc_begin0
	.hword	1
	.byte	90
	.xword	.Ltmp206-.Lfunc_begin0
	.xword	.Ltmp211-.Lfunc_begin0
	.hword	1
	.byte	82
	.xword	0
	.xword	0
.Ldebug_loc91:
	.xword	.Ltmp204-.Lfunc_begin0
	.xword	.Ltmp206-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	.Ltmp206-.Lfunc_begin0
	.xword	.Ltmp214-.Lfunc_begin0
	.hword	1
	.byte	88
	.xword	0
	.xword	0
.Ldebug_loc92:
	.xword	.Ltmp204-.Lfunc_begin0
	.xword	.Ltmp214-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	1
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc93:
	.xword	.Ltmp204-.Lfunc_begin0
	.xword	.Ltmp214-.Lfunc_begin0
	.hword	1
	.byte	82
	.xword	0
	.xword	0
.Ldebug_loc94:
	.xword	.Ltmp204-.Lfunc_begin0
	.xword	.Ltmp214-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc95:
	.xword	.Ltmp204-.Lfunc_begin0
	.xword	.Ltmp214-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc96:
	.xword	.Ltmp204-.Lfunc_begin0
	.xword	.Ltmp214-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc97:
	.xword	.Ltmp204-.Lfunc_begin0
	.xword	.Ltmp214-.Lfunc_begin0
	.hword	1
	.byte	82
	.xword	0
	.xword	0
.Ldebug_loc98:
	.xword	.Ltmp204-.Lfunc_begin0
	.xword	.Ltmp214-.Lfunc_begin0
	.hword	1
	.byte	99
	.xword	0
	.xword	0
.Ldebug_loc99:
	.xword	.Ltmp206-.Lfunc_begin0
	.xword	.Ltmp207-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	0
	.byte	159
	.xword	.Ltmp207-.Lfunc_begin0
	.xword	.Ltmp211-.Lfunc_begin0
	.hword	1
	.byte	89
	.xword	0
	.xword	0
.Ldebug_loc100:
	.xword	.Ltmp206-.Lfunc_begin0
	.xword	.Ltmp211-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	0
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc101:
	.xword	.Ltmp206-.Lfunc_begin0
	.xword	.Ltmp214-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc102:
	.xword	.Ltmp206-.Lfunc_begin0
	.xword	.Ltmp212-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc103:
	.xword	.Ltmp206-.Lfunc_begin0
	.xword	.Ltmp213-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc104:
	.xword	.Ltmp206-.Lfunc_begin0
	.xword	.Ltmp211-.Lfunc_begin0
	.hword	4
	.byte	120
	.byte	212
	.byte	3
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc105:
	.xword	.Ltmp206-.Lfunc_begin0
	.xword	.Ltmp214-.Lfunc_begin0
	.hword	4
	.byte	120
	.byte	212
	.byte	14
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc106:
	.xword	.Ltmp206-.Lfunc_begin0
	.xword	.Ltmp214-.Lfunc_begin0
	.hword	4
	.byte	120
	.byte	212
	.byte	3
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc107:
	.xword	.Ltmp216-.Lfunc_begin0
	.xword	.Ltmp218-.Lfunc_begin0
	.hword	1
	.byte	90
	.xword	.Ltmp219-.Lfunc_begin0
	.xword	.Ltmp224-.Lfunc_begin0
	.hword	1
	.byte	82
	.xword	0
	.xword	0
.Ldebug_loc108:
	.xword	.Ltmp217-.Lfunc_begin0
	.xword	.Ltmp219-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	.Ltmp219-.Lfunc_begin0
	.xword	.Ltmp224-.Lfunc_begin0
	.hword	1
	.byte	88
	.xword	0
	.xword	0
.Ldebug_loc109:
	.xword	.Ltmp217-.Lfunc_begin0
	.xword	.Ltmp224-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	2
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc110:
	.xword	.Ltmp217-.Lfunc_begin0
	.xword	.Ltmp224-.Lfunc_begin0
	.hword	1
	.byte	82
	.xword	0
	.xword	0
.Ldebug_loc111:
	.xword	.Ltmp217-.Lfunc_begin0
	.xword	.Ltmp224-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc112:
	.xword	.Ltmp217-.Lfunc_begin0
	.xword	.Ltmp224-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc113:
	.xword	.Ltmp217-.Lfunc_begin0
	.xword	.Ltmp224-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc114:
	.xword	.Ltmp217-.Lfunc_begin0
	.xword	.Ltmp224-.Lfunc_begin0
	.hword	2
	.byte	50
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc115:
	.xword	.Ltmp217-.Lfunc_begin0
	.xword	.Ltmp224-.Lfunc_begin0
	.hword	1
	.byte	82
	.xword	0
	.xword	0
.Ldebug_loc116:
	.xword	.Ltmp217-.Lfunc_begin0
	.xword	.Ltmp224-.Lfunc_begin0
	.hword	1
	.byte	99
	.xword	0
	.xword	0
.Ldebug_loc117:
	.xword	.Ltmp219-.Lfunc_begin0
	.xword	.Ltmp220-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	0
	.byte	159
	.xword	.Ltmp220-.Lfunc_begin0
	.xword	.Ltmp224-.Lfunc_begin0
	.hword	1
	.byte	89
	.xword	0
	.xword	0
.Ldebug_loc118:
	.xword	.Ltmp219-.Lfunc_begin0
	.xword	.Ltmp224-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	0
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc119:
	.xword	.Ltmp219-.Lfunc_begin0
	.xword	.Ltmp224-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc120:
	.xword	.Ltmp219-.Lfunc_begin0
	.xword	.Ltmp224-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc121:
	.xword	.Ltmp219-.Lfunc_begin0
	.xword	.Ltmp224-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc122:
	.xword	.Ltmp219-.Lfunc_begin0
	.xword	.Ltmp224-.Lfunc_begin0
	.hword	4
	.byte	120
	.byte	212
	.byte	1
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc123:
	.xword	.Ltmp219-.Lfunc_begin0
	.xword	.Ltmp224-.Lfunc_begin0
	.hword	4
	.byte	120
	.byte	212
	.byte	6
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc124:
	.xword	.Ltmp219-.Lfunc_begin0
	.xword	.Ltmp224-.Lfunc_begin0
	.hword	4
	.byte	120
	.byte	212
	.byte	1
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc125:
	.xword	.Ltmp226-.Lfunc_begin0
	.xword	.Ltmp228-.Lfunc_begin0
	.hword	1
	.byte	90
	.xword	.Ltmp229-.Lfunc_begin0
	.xword	.Ltmp234-.Lfunc_begin0
	.hword	1
	.byte	82
	.xword	0
	.xword	0
.Ldebug_loc126:
	.xword	.Ltmp227-.Lfunc_begin0
	.xword	.Ltmp229-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	.Ltmp229-.Lfunc_begin0
	.xword	.Ltmp234-.Lfunc_begin0
	.hword	1
	.byte	88
	.xword	0
	.xword	0
.Ldebug_loc127:
	.xword	.Ltmp227-.Lfunc_begin0
	.xword	.Ltmp234-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	2
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc128:
	.xword	.Ltmp227-.Lfunc_begin0
	.xword	.Ltmp234-.Lfunc_begin0
	.hword	1
	.byte	82
	.xword	0
	.xword	0
.Ldebug_loc129:
	.xword	.Ltmp227-.Lfunc_begin0
	.xword	.Ltmp234-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc130:
	.xword	.Ltmp227-.Lfunc_begin0
	.xword	.Ltmp234-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc131:
	.xword	.Ltmp227-.Lfunc_begin0
	.xword	.Ltmp234-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc132:
	.xword	.Ltmp227-.Lfunc_begin0
	.xword	.Ltmp234-.Lfunc_begin0
	.hword	2
	.byte	50
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc133:
	.xword	.Ltmp227-.Lfunc_begin0
	.xword	.Ltmp234-.Lfunc_begin0
	.hword	1
	.byte	82
	.xword	0
	.xword	0
.Ldebug_loc134:
	.xword	.Ltmp227-.Lfunc_begin0
	.xword	.Ltmp234-.Lfunc_begin0
	.hword	1
	.byte	99
	.xword	0
	.xword	0
.Ldebug_loc135:
	.xword	.Ltmp229-.Lfunc_begin0
	.xword	.Ltmp230-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	0
	.byte	159
	.xword	.Ltmp230-.Lfunc_begin0
	.xword	.Ltmp234-.Lfunc_begin0
	.hword	1
	.byte	89
	.xword	0
	.xword	0
.Ldebug_loc136:
	.xword	.Ltmp229-.Lfunc_begin0
	.xword	.Ltmp234-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	0
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc137:
	.xword	.Ltmp229-.Lfunc_begin0
	.xword	.Ltmp234-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc138:
	.xword	.Ltmp229-.Lfunc_begin0
	.xword	.Ltmp234-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc139:
	.xword	.Ltmp229-.Lfunc_begin0
	.xword	.Ltmp234-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc140:
	.xword	.Ltmp229-.Lfunc_begin0
	.xword	.Ltmp234-.Lfunc_begin0
	.hword	4
	.byte	120
	.byte	212
	.byte	1
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc141:
	.xword	.Ltmp229-.Lfunc_begin0
	.xword	.Ltmp234-.Lfunc_begin0
	.hword	4
	.byte	120
	.byte	212
	.byte	6
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc142:
	.xword	.Ltmp229-.Lfunc_begin0
	.xword	.Ltmp234-.Lfunc_begin0
	.hword	4
	.byte	120
	.byte	212
	.byte	1
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc143:
	.xword	.Ltmp238-.Lfunc_begin0
	.xword	.Ltmp240-.Lfunc_begin0
	.hword	1
	.byte	90
	.xword	.Ltmp241-.Lfunc_begin0
	.xword	.Ltmp246-.Lfunc_begin0
	.hword	1
	.byte	82
	.xword	0
	.xword	0
.Ldebug_loc144:
	.xword	.Ltmp239-.Lfunc_begin0
	.xword	.Ltmp241-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	.Ltmp241-.Lfunc_begin0
	.xword	.Ltmp249-.Lfunc_begin0
	.hword	1
	.byte	88
	.xword	0
	.xword	0
.Ldebug_loc145:
	.xword	.Ltmp239-.Lfunc_begin0
	.xword	.Ltmp249-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	3
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc146:
	.xword	.Ltmp239-.Lfunc_begin0
	.xword	.Ltmp249-.Lfunc_begin0
	.hword	1
	.byte	82
	.xword	0
	.xword	0
.Ldebug_loc147:
	.xword	.Ltmp239-.Lfunc_begin0
	.xword	.Ltmp249-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc148:
	.xword	.Ltmp239-.Lfunc_begin0
	.xword	.Ltmp249-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc149:
	.xword	.Ltmp239-.Lfunc_begin0
	.xword	.Ltmp249-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc150:
	.xword	.Ltmp239-.Lfunc_begin0
	.xword	.Ltmp249-.Lfunc_begin0
	.hword	2
	.byte	51
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc151:
	.xword	.Ltmp239-.Lfunc_begin0
	.xword	.Ltmp249-.Lfunc_begin0
	.hword	1
	.byte	82
	.xword	0
	.xword	0
.Ldebug_loc152:
	.xword	.Ltmp239-.Lfunc_begin0
	.xword	.Ltmp249-.Lfunc_begin0
	.hword	1
	.byte	99
	.xword	0
	.xword	0
.Ldebug_loc153:
	.xword	.Ltmp241-.Lfunc_begin0
	.xword	.Ltmp242-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	0
	.byte	159
	.xword	.Ltmp242-.Lfunc_begin0
	.xword	.Ltmp246-.Lfunc_begin0
	.hword	1
	.byte	89
	.xword	0
	.xword	0
.Ldebug_loc154:
	.xword	.Ltmp241-.Lfunc_begin0
	.xword	.Ltmp246-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	0
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc155:
	.xword	.Ltmp241-.Lfunc_begin0
	.xword	.Ltmp249-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc156:
	.xword	.Ltmp241-.Lfunc_begin0
	.xword	.Ltmp247-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc157:
	.xword	.Ltmp241-.Lfunc_begin0
	.xword	.Ltmp248-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc158:
	.xword	.Ltmp241-.Lfunc_begin0
	.xword	.Ltmp246-.Lfunc_begin0
	.hword	4
	.byte	120
	.byte	148
	.byte	2
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc159:
	.xword	.Ltmp241-.Lfunc_begin0
	.xword	.Ltmp249-.Lfunc_begin0
	.hword	4
	.byte	120
	.byte	212
	.byte	8
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc160:
	.xword	.Ltmp241-.Lfunc_begin0
	.xword	.Ltmp249-.Lfunc_begin0
	.hword	4
	.byte	120
	.byte	148
	.byte	2
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc161:
	.xword	.Ltmp251-.Lfunc_begin0
	.xword	.Ltmp253-.Lfunc_begin0
	.hword	1
	.byte	90
	.xword	.Ltmp254-.Lfunc_begin0
	.xword	.Ltmp259-.Lfunc_begin0
	.hword	1
	.byte	82
	.xword	0
	.xword	0
.Ldebug_loc162:
	.xword	.Ltmp252-.Lfunc_begin0
	.xword	.Ltmp254-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	.Ltmp254-.Lfunc_begin0
	.xword	.Ltmp262-.Lfunc_begin0
	.hword	1
	.byte	88
	.xword	0
	.xword	0
.Ldebug_loc163:
	.xword	.Ltmp252-.Lfunc_begin0
	.xword	.Ltmp262-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	4
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc164:
	.xword	.Ltmp252-.Lfunc_begin0
	.xword	.Ltmp262-.Lfunc_begin0
	.hword	1
	.byte	82
	.xword	0
	.xword	0
.Ldebug_loc165:
	.xword	.Ltmp252-.Lfunc_begin0
	.xword	.Ltmp262-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc166:
	.xword	.Ltmp252-.Lfunc_begin0
	.xword	.Ltmp262-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc167:
	.xword	.Ltmp252-.Lfunc_begin0
	.xword	.Ltmp262-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc168:
	.xword	.Ltmp252-.Lfunc_begin0
	.xword	.Ltmp262-.Lfunc_begin0
	.hword	2
	.byte	52
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc169:
	.xword	.Ltmp252-.Lfunc_begin0
	.xword	.Ltmp262-.Lfunc_begin0
	.hword	1
	.byte	82
	.xword	0
	.xword	0
.Ldebug_loc170:
	.xword	.Ltmp252-.Lfunc_begin0
	.xword	.Ltmp262-.Lfunc_begin0
	.hword	1
	.byte	99
	.xword	0
	.xword	0
.Ldebug_loc171:
	.xword	.Ltmp254-.Lfunc_begin0
	.xword	.Ltmp255-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	0
	.byte	159
	.xword	.Ltmp255-.Lfunc_begin0
	.xword	.Ltmp259-.Lfunc_begin0
	.hword	1
	.byte	89
	.xword	0
	.xword	0
.Ldebug_loc172:
	.xword	.Ltmp254-.Lfunc_begin0
	.xword	.Ltmp259-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	0
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc173:
	.xword	.Ltmp254-.Lfunc_begin0
	.xword	.Ltmp262-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc174:
	.xword	.Ltmp254-.Lfunc_begin0
	.xword	.Ltmp260-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc175:
	.xword	.Ltmp254-.Lfunc_begin0
	.xword	.Ltmp261-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc176:
	.xword	.Ltmp254-.Lfunc_begin0
	.xword	.Ltmp259-.Lfunc_begin0
	.hword	4
	.byte	120
	.byte	212
	.byte	2
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc177:
	.xword	.Ltmp254-.Lfunc_begin0
	.xword	.Ltmp262-.Lfunc_begin0
	.hword	4
	.byte	120
	.byte	212
	.byte	10
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc178:
	.xword	.Ltmp254-.Lfunc_begin0
	.xword	.Ltmp262-.Lfunc_begin0
	.hword	4
	.byte	120
	.byte	212
	.byte	2
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc179:
	.xword	.Ltmp264-.Lfunc_begin0
	.xword	.Ltmp266-.Lfunc_begin0
	.hword	1
	.byte	90
	.xword	.Ltmp267-.Lfunc_begin0
	.xword	.Ltmp272-.Lfunc_begin0
	.hword	1
	.byte	82
	.xword	0
	.xword	0
.Ldebug_loc180:
	.xword	.Ltmp265-.Lfunc_begin0
	.xword	.Ltmp267-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	.Ltmp267-.Lfunc_begin0
	.xword	.Ltmp275-.Lfunc_begin0
	.hword	1
	.byte	88
	.xword	0
	.xword	0
.Ldebug_loc181:
	.xword	.Ltmp265-.Lfunc_begin0
	.xword	.Ltmp275-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	5
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc182:
	.xword	.Ltmp265-.Lfunc_begin0
	.xword	.Ltmp275-.Lfunc_begin0
	.hword	1
	.byte	82
	.xword	0
	.xword	0
.Ldebug_loc183:
	.xword	.Ltmp265-.Lfunc_begin0
	.xword	.Ltmp275-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc184:
	.xword	.Ltmp265-.Lfunc_begin0
	.xword	.Ltmp275-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc185:
	.xword	.Ltmp265-.Lfunc_begin0
	.xword	.Ltmp275-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc186:
	.xword	.Ltmp265-.Lfunc_begin0
	.xword	.Ltmp275-.Lfunc_begin0
	.hword	2
	.byte	53
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc187:
	.xword	.Ltmp265-.Lfunc_begin0
	.xword	.Ltmp275-.Lfunc_begin0
	.hword	1
	.byte	82
	.xword	0
	.xword	0
.Ldebug_loc188:
	.xword	.Ltmp265-.Lfunc_begin0
	.xword	.Ltmp275-.Lfunc_begin0
	.hword	1
	.byte	99
	.xword	0
	.xword	0
.Ldebug_loc189:
	.xword	.Ltmp267-.Lfunc_begin0
	.xword	.Ltmp268-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	0
	.byte	159
	.xword	.Ltmp268-.Lfunc_begin0
	.xword	.Ltmp272-.Lfunc_begin0
	.hword	1
	.byte	89
	.xword	0
	.xword	0
.Ldebug_loc190:
	.xword	.Ltmp267-.Lfunc_begin0
	.xword	.Ltmp272-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	0
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc191:
	.xword	.Ltmp267-.Lfunc_begin0
	.xword	.Ltmp275-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc192:
	.xword	.Ltmp267-.Lfunc_begin0
	.xword	.Ltmp273-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc193:
	.xword	.Ltmp267-.Lfunc_begin0
	.xword	.Ltmp274-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc194:
	.xword	.Ltmp267-.Lfunc_begin0
	.xword	.Ltmp272-.Lfunc_begin0
	.hword	4
	.byte	120
	.byte	148
	.byte	3
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc195:
	.xword	.Ltmp267-.Lfunc_begin0
	.xword	.Ltmp275-.Lfunc_begin0
	.hword	4
	.byte	120
	.byte	212
	.byte	12
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc196:
	.xword	.Ltmp267-.Lfunc_begin0
	.xword	.Ltmp275-.Lfunc_begin0
	.hword	4
	.byte	120
	.byte	148
	.byte	3
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc197:
	.xword	.Ltmp277-.Lfunc_begin0
	.xword	.Ltmp279-.Lfunc_begin0
	.hword	1
	.byte	90
	.xword	.Ltmp280-.Lfunc_begin0
	.xword	.Ltmp285-.Lfunc_begin0
	.hword	1
	.byte	82
	.xword	0
	.xword	0
.Ldebug_loc198:
	.xword	.Ltmp278-.Lfunc_begin0
	.xword	.Ltmp280-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	.Ltmp280-.Lfunc_begin0
	.xword	.Ltmp287-.Lfunc_begin0
	.hword	1
	.byte	88
	.xword	0
	.xword	0
.Ldebug_loc199:
	.xword	.Ltmp278-.Lfunc_begin0
	.xword	.Ltmp287-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	6
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc200:
	.xword	.Ltmp278-.Lfunc_begin0
	.xword	.Ltmp287-.Lfunc_begin0
	.hword	1
	.byte	82
	.xword	0
	.xword	0
.Ldebug_loc201:
	.xword	.Ltmp278-.Lfunc_begin0
	.xword	.Ltmp287-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc202:
	.xword	.Ltmp278-.Lfunc_begin0
	.xword	.Ltmp287-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc203:
	.xword	.Ltmp278-.Lfunc_begin0
	.xword	.Ltmp287-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc204:
	.xword	.Ltmp278-.Lfunc_begin0
	.xword	.Ltmp287-.Lfunc_begin0
	.hword	1
	.byte	82
	.xword	0
	.xword	0
.Ldebug_loc205:
	.xword	.Ltmp278-.Lfunc_begin0
	.xword	.Ltmp287-.Lfunc_begin0
	.hword	1
	.byte	99
	.xword	0
	.xword	0
.Ldebug_loc206:
	.xword	.Ltmp280-.Lfunc_begin0
	.xword	.Ltmp281-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	0
	.byte	159
	.xword	.Ltmp281-.Lfunc_begin0
	.xword	.Ltmp285-.Lfunc_begin0
	.hword	1
	.byte	89
	.xword	0
	.xword	0
.Ldebug_loc207:
	.xword	.Ltmp280-.Lfunc_begin0
	.xword	.Ltmp285-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	0
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc208:
	.xword	.Ltmp280-.Lfunc_begin0
	.xword	.Ltmp287-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc209:
	.xword	.Ltmp280-.Lfunc_begin0
	.xword	.Ltmp286-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc210:
	.xword	.Ltmp280-.Lfunc_begin0
	.xword	.Ltmp287-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc211:
	.xword	.Ltmp280-.Lfunc_begin0
	.xword	.Ltmp285-.Lfunc_begin0
	.hword	4
	.byte	120
	.byte	148
	.byte	4
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc212:
	.xword	.Ltmp280-.Lfunc_begin0
	.xword	.Ltmp287-.Lfunc_begin0
	.hword	4
	.byte	120
	.byte	212
	.byte	16
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc213:
	.xword	.Ltmp280-.Lfunc_begin0
	.xword	.Ltmp287-.Lfunc_begin0
	.hword	4
	.byte	120
	.byte	148
	.byte	4
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc214:
	.xword	.Lfunc_begin4-.Lfunc_begin0
	.xword	.Ltmp307-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	0
	.byte	159
	.xword	.Ltmp307-.Lfunc_begin0
	.xword	.Ltmp349-.Lfunc_begin0
	.hword	1
	.byte	90
	.xword	0
	.xword	0
.Ldebug_loc215:
	.xword	.Lfunc_begin4-.Lfunc_begin0
	.xword	.Ltmp307-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	.Ltmp308-.Lfunc_begin0
	.xword	.Ltmp310-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	.Ltmp310-.Lfunc_begin0
	.xword	.Ltmp312-.Lfunc_begin0
	.hword	6
	.byte	125
	.byte	0
	.byte	17
	.byte	4
	.byte	27
	.byte	159
	.xword	.Ltmp345-.Lfunc_begin0
	.xword	.Ltmp349-.Lfunc_begin0
	.hword	9
	.byte	125
	.byte	0
	.byte	17
	.byte	4
	.byte	27
	.byte	17
	.byte	1
	.byte	34
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc216:
	.xword	.Lfunc_begin4-.Lfunc_begin0
	.xword	.Ltmp307-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	.Ltmp311-.Lfunc_begin0
	.xword	.Ltmp315-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	.Ltmp315-.Lfunc_begin0
	.xword	.Ltmp317-.Lfunc_begin0
	.hword	2
	.byte	49
	.byte	159
	.xword	.Ltmp317-.Lfunc_begin0
	.xword	.Ltmp319-.Lfunc_begin0
	.hword	2
	.byte	50
	.byte	159
	.xword	.Ltmp319-.Lfunc_begin0
	.xword	.Ltmp321-.Lfunc_begin0
	.hword	2
	.byte	51
	.byte	159
	.xword	.Ltmp321-.Lfunc_begin0
	.xword	.Ltmp323-.Lfunc_begin0
	.hword	2
	.byte	52
	.byte	159
	.xword	.Ltmp323-.Lfunc_begin0
	.xword	.Ltmp325-.Lfunc_begin0
	.hword	2
	.byte	53
	.byte	159
	.xword	.Ltmp325-.Lfunc_begin0
	.xword	.Ltmp327-.Lfunc_begin0
	.hword	2
	.byte	54
	.byte	159
	.xword	.Ltmp327-.Lfunc_begin0
	.xword	.Ltmp329-.Lfunc_begin0
	.hword	2
	.byte	55
	.byte	159
	.xword	.Ltmp329-.Lfunc_begin0
	.xword	.Ltmp331-.Lfunc_begin0
	.hword	2
	.byte	56
	.byte	159
	.xword	.Ltmp331-.Lfunc_begin0
	.xword	.Ltmp333-.Lfunc_begin0
	.hword	2
	.byte	57
	.byte	159
	.xword	.Ltmp333-.Lfunc_begin0
	.xword	.Ltmp335-.Lfunc_begin0
	.hword	2
	.byte	58
	.byte	159
	.xword	.Ltmp335-.Lfunc_begin0
	.xword	.Ltmp337-.Lfunc_begin0
	.hword	2
	.byte	59
	.byte	159
	.xword	.Ltmp337-.Lfunc_begin0
	.xword	.Ltmp339-.Lfunc_begin0
	.hword	2
	.byte	60
	.byte	159
	.xword	.Ltmp339-.Lfunc_begin0
	.xword	.Ltmp341-.Lfunc_begin0
	.hword	2
	.byte	61
	.byte	159
	.xword	.Ltmp341-.Lfunc_begin0
	.xword	.Ltmp343-.Lfunc_begin0
	.hword	2
	.byte	62
	.byte	159
	.xword	.Ltmp343-.Lfunc_begin0
	.xword	.Ltmp345-.Lfunc_begin0
	.hword	2
	.byte	63
	.byte	159
	.xword	.Ltmp345-.Lfunc_begin0
	.xword	.Ltmp349-.Lfunc_begin0
	.hword	2
	.byte	64
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc217:
	.xword	.Ltmp307-.Lfunc_begin0
	.xword	.Ltmp308-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	.Ltmp308-.Lfunc_begin0
	.xword	.Ltmp310-.Lfunc_begin0
	.hword	21
	.byte	127
	.byte	0
	.byte	122
	.byte	0
	.byte	52
	.byte	36
	.byte	18
	.byte	16
	.byte	31
	.byte	37
	.byte	48
	.byte	32
	.byte	30
	.byte	16
	.byte	32
	.byte	36
	.byte	33
	.byte	52
	.byte	30
	.byte	34
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc218:
	.xword	.Ltmp311-.Lfunc_begin0
	.xword	.Ltmp314-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	.Ltmp314-.Lfunc_begin0
	.xword	.Ltmp315-.Lfunc_begin0
	.hword	1
	.byte	97
	.xword	.Ltmp315-.Lfunc_begin0
	.xword	.Ltmp316-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	.Ltmp316-.Lfunc_begin0
	.xword	.Ltmp317-.Lfunc_begin0
	.hword	1
	.byte	97
	.xword	.Ltmp317-.Lfunc_begin0
	.xword	.Ltmp318-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	.Ltmp318-.Lfunc_begin0
	.xword	.Ltmp319-.Lfunc_begin0
	.hword	1
	.byte	97
	.xword	.Ltmp319-.Lfunc_begin0
	.xword	.Ltmp320-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	.Ltmp321-.Lfunc_begin0
	.xword	.Ltmp322-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	.Ltmp322-.Lfunc_begin0
	.xword	.Ltmp323-.Lfunc_begin0
	.hword	1
	.byte	97
	.xword	.Ltmp323-.Lfunc_begin0
	.xword	.Ltmp324-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	.Ltmp324-.Lfunc_begin0
	.xword	.Ltmp325-.Lfunc_begin0
	.hword	1
	.byte	97
	.xword	.Ltmp325-.Lfunc_begin0
	.xword	.Ltmp326-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	.Ltmp326-.Lfunc_begin0
	.xword	.Ltmp327-.Lfunc_begin0
	.hword	1
	.byte	97
	.xword	.Ltmp327-.Lfunc_begin0
	.xword	.Ltmp328-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	.Ltmp328-.Lfunc_begin0
	.xword	.Ltmp329-.Lfunc_begin0
	.hword	1
	.byte	97
	.xword	.Ltmp329-.Lfunc_begin0
	.xword	.Ltmp330-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	.Ltmp330-.Lfunc_begin0
	.xword	.Ltmp331-.Lfunc_begin0
	.hword	1
	.byte	97
	.xword	.Ltmp331-.Lfunc_begin0
	.xword	.Ltmp332-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	.Ltmp332-.Lfunc_begin0
	.xword	.Ltmp333-.Lfunc_begin0
	.hword	1
	.byte	97
	.xword	.Ltmp333-.Lfunc_begin0
	.xword	.Ltmp334-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	.Ltmp334-.Lfunc_begin0
	.xword	.Ltmp335-.Lfunc_begin0
	.hword	1
	.byte	97
	.xword	.Ltmp335-.Lfunc_begin0
	.xword	.Ltmp336-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	.Ltmp336-.Lfunc_begin0
	.xword	.Ltmp337-.Lfunc_begin0
	.hword	1
	.byte	97
	.xword	.Ltmp337-.Lfunc_begin0
	.xword	.Ltmp338-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	.Ltmp338-.Lfunc_begin0
	.xword	.Ltmp339-.Lfunc_begin0
	.hword	1
	.byte	97
	.xword	.Ltmp339-.Lfunc_begin0
	.xword	.Ltmp340-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	.Ltmp340-.Lfunc_begin0
	.xword	.Ltmp341-.Lfunc_begin0
	.hword	1
	.byte	97
	.xword	.Ltmp341-.Lfunc_begin0
	.xword	.Ltmp342-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	.Ltmp342-.Lfunc_begin0
	.xword	.Ltmp343-.Lfunc_begin0
	.hword	1
	.byte	97
	.xword	.Ltmp343-.Lfunc_begin0
	.xword	.Ltmp344-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
	.section	.debug_abbrev,"",@progbits
	.byte	1
	.byte	17
	.byte	1
	.byte	37
	.byte	14
	.byte	19
	.byte	5
	.byte	3
	.byte	14
	.byte	16
	.byte	23
	.byte	27
	.byte	14
	.byte	17
	.byte	1
	.byte	18
	.byte	6
	.byte	0
	.byte	0
	.byte	2
	.byte	46
	.byte	1
	.byte	17
	.byte	1
	.byte	18
	.byte	6
	.byte	64
	.byte	24
	.ascii	"\227B"
	.byte	25
	.byte	3
	.byte	14
	.byte	58
	.byte	11
	.byte	59
	.byte	5
	.byte	39
	.byte	25
	.byte	73
	.byte	19
	.byte	63
	.byte	25
	.byte	0
	.byte	0
	.byte	3
	.byte	52
	.byte	0
	.byte	3
	.byte	14
	.byte	73
	.byte	19
	.byte	58
	.byte	11
	.byte	59
	.byte	5
	.byte	2
	.byte	24
	.byte	0
	.byte	0
	.byte	4
	.byte	5
	.byte	0
	.byte	2
	.byte	23
	.byte	3
	.byte	14
	.byte	58
	.byte	11
	.byte	59
	.byte	5
	.byte	73
	.byte	19
	.byte	0
	.byte	0
	.byte	5
	.byte	52
	.byte	0
	.byte	2
	.byte	23
	.byte	3
	.byte	14
	.byte	58
	.byte	11
	.byte	59
	.byte	5
	.byte	73
	.byte	19
	.byte	0
	.byte	0
	.byte	6
	.byte	29
	.byte	1
	.byte	49
	.byte	19
	.byte	85
	.byte	23
	.byte	88
	.byte	11
	.byte	89
	.byte	5
	.byte	87
	.byte	11
	.byte	0
	.byte	0
	.byte	7
	.byte	5
	.byte	0
	.byte	2
	.byte	23
	.byte	49
	.byte	19
	.byte	0
	.byte	0
	.byte	8
	.byte	52
	.byte	0
	.byte	2
	.byte	23
	.byte	49
	.byte	19
	.byte	0
	.byte	0
	.byte	9
	.byte	29
	.byte	1
	.byte	49
	.byte	19
	.byte	17
	.byte	1
	.byte	18
	.byte	6
	.byte	88
	.byte	11
	.byte	89
	.byte	11
	.byte	87
	.byte	11
	.byte	0
	.byte	0
	.byte	10
	.byte	5
	.byte	0
	.byte	2
	.byte	24
	.byte	49
	.byte	19
	.byte	0
	.byte	0
	.byte	11
	.byte	52
	.byte	0
	.byte	28
	.byte	13
	.byte	49
	.byte	19
	.byte	0
	.byte	0
	.byte	12
	.byte	11
	.byte	1
	.byte	17
	.byte	1
	.byte	18
	.byte	6
	.byte	0
	.byte	0
	.byte	13
	.byte	52
	.byte	0
	.byte	2
	.byte	24
	.byte	49
	.byte	19
	.byte	0
	.byte	0
	.byte	14
	.byte	29
	.byte	1
	.byte	49
	.byte	19
	.byte	85
	.byte	23
	.byte	88
	.byte	11
	.byte	89
	.byte	11
	.byte	87
	.byte	11
	.byte	0
	.byte	0
	.byte	15
	.byte	29
	.byte	1
	.byte	49
	.byte	19
	.byte	17
	.byte	1
	.byte	18
	.byte	6
	.byte	88
	.byte	11
	.byte	89
	.byte	5
	.byte	87
	.byte	11
	.byte	0
	.byte	0
	.byte	16
	.ascii	"\211\202\001"
	.byte	0
	.byte	49
	.byte	19
	.byte	17
	.byte	1
	.byte	0
	.byte	0
	.byte	17
	.ascii	"\211\202\001"
	.byte	1
	.byte	49
	.byte	19
	.byte	17
	.byte	1
	.byte	0
	.byte	0
	.byte	18
	.ascii	"\212\202\001"
	.byte	0
	.byte	2
	.byte	24
	.ascii	"\221B"
	.byte	24
	.byte	0
	.byte	0
	.byte	19
	.byte	4
	.byte	1
	.byte	73
	.byte	19
	.byte	3
	.byte	14
	.byte	11
	.byte	11
	.byte	58
	.byte	11
	.byte	59
	.byte	11
	.byte	0
	.byte	0
	.byte	20
	.byte	40
	.byte	0
	.byte	3
	.byte	14
	.byte	28
	.byte	15
	.byte	0
	.byte	0
	.byte	21
	.byte	36
	.byte	0
	.byte	3
	.byte	14
	.byte	62
	.byte	11
	.byte	11
	.byte	11
	.byte	0
	.byte	0
	.byte	22
	.byte	52
	.byte	0
	.byte	3
	.byte	14
	.byte	73
	.byte	19
	.byte	58
	.byte	11
	.byte	59
	.byte	11
	.byte	2
	.byte	24
	.byte	0
	.byte	0
	.byte	23
	.byte	15
	.byte	0
	.byte	73
	.byte	19
	.byte	0
	.byte	0
	.byte	24
	.byte	19
	.byte	1
	.byte	3
	.byte	14
	.byte	11
	.byte	5
	.byte	58
	.byte	11
	.byte	59
	.byte	11
	.byte	0
	.byte	0
	.byte	25
	.byte	13
	.byte	0
	.byte	3
	.byte	14
	.byte	73
	.byte	19
	.byte	58
	.byte	11
	.byte	59
	.byte	11
	.byte	56
	.byte	11
	.byte	0
	.byte	0
	.byte	26
	.byte	13
	.byte	0
	.byte	3
	.byte	14
	.byte	73
	.byte	19
	.byte	58
	.byte	11
	.byte	59
	.byte	11
	.byte	56
	.byte	5
	.byte	0
	.byte	0
	.byte	27
	.byte	1
	.byte	1
	.byte	73
	.byte	19
	.byte	0
	.byte	0
	.byte	28
	.byte	33
	.byte	0
	.byte	73
	.byte	19
	.byte	55
	.byte	11
	.byte	0
	.byte	0
	.byte	29
	.byte	36
	.byte	0
	.byte	3
	.byte	14
	.byte	11
	.byte	11
	.byte	62
	.byte	11
	.byte	0
	.byte	0
	.byte	30
	.byte	33
	.byte	0
	.byte	73
	.byte	19
	.byte	55
	.byte	5
	.byte	0
	.byte	0
	.byte	31
	.byte	4
	.byte	1
	.byte	73
	.byte	19
	.byte	11
	.byte	11
	.byte	58
	.byte	11
	.byte	59
	.byte	11
	.byte	0
	.byte	0
	.byte	32
	.byte	4
	.byte	1
	.byte	73
	.byte	19
	.byte	3
	.byte	14
	.byte	11
	.byte	11
	.byte	58
	.byte	11
	.byte	59
	.byte	5
	.byte	0
	.byte	0
	.byte	33
	.byte	38
	.byte	0
	.byte	73
	.byte	19
	.byte	0
	.byte	0
	.byte	34
	.byte	22
	.byte	0
	.byte	73
	.byte	19
	.byte	3
	.byte	14
	.byte	58
	.byte	11
	.byte	59
	.byte	11
	.byte	0
	.byte	0
	.byte	35
	.byte	15
	.byte	0
	.byte	0
	.byte	0
	.byte	36
	.byte	46
	.byte	1
	.byte	3
	.byte	14
	.byte	58
	.byte	11
	.byte	59
	.byte	11
	.byte	39
	.byte	25
	.byte	73
	.byte	19
	.byte	32
	.byte	11
	.byte	0
	.byte	0
	.byte	37
	.byte	5
	.byte	0
	.byte	3
	.byte	14
	.byte	58
	.byte	11
	.byte	59
	.byte	11
	.byte	73
	.byte	19
	.byte	0
	.byte	0
	.byte	38
	.byte	52
	.byte	0
	.byte	3
	.byte	14
	.byte	58
	.byte	11
	.byte	59
	.byte	11
	.byte	73
	.byte	19
	.byte	0
	.byte	0
	.byte	39
	.byte	46
	.byte	1
	.byte	3
	.byte	14
	.byte	58
	.byte	11
	.byte	59
	.byte	11
	.byte	39
	.byte	25
	.byte	32
	.byte	11
	.byte	0
	.byte	0
	.byte	40
	.byte	46
	.byte	1
	.byte	17
	.byte	1
	.byte	18
	.byte	6
	.byte	64
	.byte	24
	.ascii	"\227B"
	.byte	25
	.byte	3
	.byte	14
	.byte	58
	.byte	11
	.byte	59
	.byte	11
	.byte	39
	.byte	25
	.byte	73
	.byte	19
	.byte	63
	.byte	25
	.byte	0
	.byte	0
	.byte	41
	.byte	5
	.byte	0
	.byte	2
	.byte	23
	.byte	3
	.byte	14
	.byte	58
	.byte	11
	.byte	59
	.byte	11
	.byte	73
	.byte	19
	.byte	0
	.byte	0
	.byte	42
	.byte	52
	.byte	0
	.byte	2
	.byte	23
	.byte	3
	.byte	14
	.byte	58
	.byte	11
	.byte	59
	.byte	11
	.byte	73
	.byte	19
	.byte	0
	.byte	0
	.byte	43
	.byte	5
	.byte	0
	.byte	28
	.byte	15
	.byte	49
	.byte	19
	.byte	0
	.byte	0
	.byte	44
	.byte	46
	.byte	0
	.byte	17
	.byte	1
	.byte	18
	.byte	6
	.byte	64
	.byte	24
	.ascii	"\227B"
	.byte	25
	.byte	3
	.byte	14
	.byte	58
	.byte	11
	.byte	59
	.byte	11
	.byte	39
	.byte	25
	.byte	73
	.byte	19
	.byte	63
	.byte	25
	.byte	0
	.byte	0
	.byte	45
	.byte	46
	.byte	1
	.byte	3
	.byte	14
	.byte	58
	.byte	11
	.byte	59
	.byte	5
	.byte	39
	.byte	25
	.byte	73
	.byte	19
	.byte	32
	.byte	11
	.byte	0
	.byte	0
	.byte	46
	.byte	5
	.byte	0
	.byte	3
	.byte	14
	.byte	58
	.byte	11
	.byte	59
	.byte	5
	.byte	73
	.byte	19
	.byte	0
	.byte	0
	.byte	47
	.byte	52
	.byte	0
	.byte	3
	.byte	14
	.byte	58
	.byte	11
	.byte	59
	.byte	5
	.byte	73
	.byte	19
	.byte	0
	.byte	0
	.byte	48
	.byte	19
	.byte	1
	.byte	3
	.byte	14
	.byte	11
	.byte	11
	.byte	58
	.byte	11
	.byte	59
	.byte	11
	.byte	0
	.byte	0
	.byte	49
	.byte	11
	.byte	1
	.byte	0
	.byte	0
	.byte	50
	.byte	46
	.byte	1
	.byte	3
	.byte	14
	.byte	58
	.byte	11
	.byte	59
	.byte	11
	.byte	39
	.byte	25
	.byte	60
	.byte	25
	.byte	63
	.byte	25
	.byte	0
	.byte	0
	.byte	51
	.byte	5
	.byte	0
	.byte	73
	.byte	19
	.byte	0
	.byte	0
	.byte	52
	.byte	38
	.byte	0
	.byte	0
	.byte	0
	.byte	53
	.byte	46
	.byte	1
	.byte	17
	.byte	1
	.byte	18
	.byte	6
	.byte	64
	.byte	24
	.ascii	"\227B"
	.byte	25
	.byte	49
	.byte	19
	.byte	0
	.byte	0
	.byte	54
	.byte	11
	.byte	1
	.byte	85
	.byte	23
	.byte	0
	.byte	0
	.byte	55
	.byte	46
	.byte	1
	.byte	17
	.byte	1
	.byte	18
	.byte	6
	.byte	64
	.byte	24
	.ascii	"\227B"
	.byte	25
	.byte	3
	.byte	14
	.byte	58
	.byte	11
	.byte	59
	.byte	5
	.byte	39
	.byte	25
	.byte	73
	.byte	19
	.byte	0
	.byte	0
	.byte	56
	.byte	5
	.byte	0
	.byte	2
	.byte	24
	.byte	3
	.byte	14
	.byte	58
	.byte	11
	.byte	59
	.byte	5
	.byte	73
	.byte	19
	.byte	0
	.byte	0
	.byte	57
	.byte	46
	.byte	0
	.byte	3
	.byte	14
	.byte	58
	.byte	11
	.byte	59
	.byte	11
	.byte	39
	.byte	25
	.byte	73
	.byte	19
	.byte	60
	.byte	25
	.byte	63
	.byte	25
	.byte	0
	.byte	0
	.byte	0
	.section	.debug_info,"",@progbits
.Lcu_begin0:
	.word	.Ldebug_info_end0-.Ldebug_info_start0
.Ldebug_info_start0:
	.hword	4
	.word	.debug_abbrev
	.byte	8
	.byte	1
	.word	.Linfo_string0
	.hword	12
	.word	.Linfo_string1
	.word	.Lline_table_start0
	.word	.Linfo_string2
	.xword	.Lfunc_begin0
	.word	.Lfunc_end4-.Lfunc_begin0
	.byte	2
	.xword	.Lfunc_begin2
	.word	.Lfunc_end2-.Lfunc_begin2
	.byte	1
	.byte	109

	.word	.Linfo_string138
	.byte	2
	.hword	542

	.word	932

	.byte	3
	.word	.Linfo_string3
	.word	834
	.byte	2
	.hword	544
	.byte	9
	.byte	3
	.xword	rkf_wf_get_lut.stype
	.byte	3
	.word	.Linfo_string19
	.word	932
	.byte	2
	.hword	545
	.byte	9
	.byte	3
	.xword	rkf_wf_get_lut.sftemp
	.byte	4
	.word	.Ldebug_loc14
	.word	.Linfo_string94
	.byte	2
	.hword	542
	.word	2199
	.byte	4
	.word	.Ldebug_loc15
	.word	.Linfo_string101
	.byte	2
	.hword	542
	.word	834
	.byte	4
	.word	.Ldebug_loc16
	.word	.Linfo_string102
	.byte	2
	.hword	542
	.word	932
	.byte	4
	.word	.Ldebug_loc17
	.word	.Linfo_string143
	.byte	2
	.hword	542
	.word	932
	.byte	5
	.word	.Ldebug_loc18
	.word	.Linfo_string95
	.byte	2
	.hword	547
	.word	932
	.byte	5
	.word	.Ldebug_loc19
	.word	.Linfo_string144
	.byte	2
	.hword	546
	.word	932
	.byte	6
	.word	2273
	.word	.Ldebug_ranges3
	.byte	2
	.hword	592
	.byte	15
	.byte	7
	.word	.Ldebug_loc38
	.word	2286
	.byte	7
	.word	.Ldebug_loc37
	.word	2298
	.byte	7
	.word	.Ldebug_loc36
	.word	2310
	.byte	8
	.word	.Ldebug_loc35
	.word	2322
	.byte	6
	.word	2125
	.word	.Ldebug_ranges4
	.byte	2
	.hword	520
	.byte	15
	.byte	7
	.word	.Ldebug_loc34
	.word	2138
	.byte	7
	.word	.Ldebug_loc33
	.word	2150
	.byte	8
	.word	.Ldebug_loc30
	.word	2162
	.byte	8
	.word	.Ldebug_loc31
	.word	2174
	.byte	8
	.word	.Ldebug_loc32
	.word	2186
	.byte	6
	.word	2024
	.word	.Ldebug_ranges5
	.byte	2
	.hword	435
	.byte	9
	.byte	7
	.word	.Ldebug_loc29
	.word	2036
	.byte	7
	.word	.Ldebug_loc28
	.word	2047
	.byte	8
	.word	.Ldebug_loc22
	.word	2058
	.byte	8
	.word	.Ldebug_loc23
	.word	2069
	.byte	8
	.word	.Ldebug_loc24
	.word	2080
	.byte	8
	.word	.Ldebug_loc25
	.word	2091
	.byte	8
	.word	.Ldebug_loc26
	.word	2102
	.byte	8
	.word	.Ldebug_loc27
	.word	2113
	.byte	9
	.word	2335
	.xword	.Ltmp83
	.word	.Ltmp91-.Ltmp83
	.byte	2
	.byte	228
	.byte	7
	.byte	10
	.byte	4
	.byte	120
	.ascii	"\224\004"
	.byte	159
	.word	2347
	.byte	7
	.word	.Ldebug_loc21
	.word	2358
	.byte	8
	.word	.Ldebug_loc20
	.word	2369
	.byte	11
	.byte	0
	.word	2380
	.byte	12
	.xword	.Ltmp83
	.word	.Ltmp84-.Ltmp83
	.byte	11
	.byte	0
	.word	2392
	.byte	13
	.byte	1
	.byte	99
	.word	2403
	.byte	0
	.byte	12
	.xword	.Ltmp84
	.word	.Ltmp86-.Ltmp84
	.byte	11
	.byte	50
	.word	2416
	.byte	13
	.byte	1
	.byte	90
	.word	2427
	.byte	0
	.byte	0
	.byte	0
	.byte	6
	.word	2570
	.word	.Ldebug_ranges6
	.byte	2
	.hword	440
	.byte	17
	.byte	7
	.word	.Ldebug_loc42
	.word	2582
	.byte	7
	.word	.Ldebug_loc45
	.word	2593
	.byte	8
	.word	.Ldebug_loc41
	.word	2604
	.byte	14
	.word	2532
	.word	.Ldebug_ranges7
	.byte	2
	.byte	245
	.byte	8
	.byte	7
	.word	.Ldebug_loc44
	.word	2545
	.byte	7
	.word	.Ldebug_loc40
	.word	2557
	.byte	6
	.word	2440
	.word	.Ldebug_ranges8
	.byte	4
	.hword	689
	.byte	9
	.byte	7
	.word	.Ldebug_loc43
	.word	2453
	.byte	7
	.word	.Ldebug_loc39
	.word	2465
	.byte	0
	.byte	0
	.byte	0
	.byte	15
	.word	2616
	.xword	.Ltmp96
	.word	.Ltmp97-.Ltmp96
	.byte	2
	.hword	436
	.byte	14
	.byte	10
	.byte	1
	.byte	106
	.word	2628
	.byte	0
	.byte	0
	.byte	0
	.byte	16
	.word	2640
	.xword	.Ltmp72
	.byte	17
	.word	2659
	.xword	.Ltmp74
	.byte	18
	.byte	1
	.byte	82
	.byte	2
	.byte	131
	.byte	0
	.byte	18
	.byte	1
	.byte	81
	.byte	2
	.byte	133
	.byte	0
	.byte	18
	.byte	1
	.byte	80
	.byte	2
	.byte	132
	.byte	0
	.byte	0
	.byte	17
	.word	5004
	.xword	.Ltmp77
	.byte	18
	.byte	1
	.byte	82
	.byte	2
	.byte	134
	.byte	0
	.byte	18
	.byte	1
	.byte	80
	.byte	2
	.byte	132
	.byte	0
	.byte	0
	.byte	16
	.word	2640
	.xword	.Ltmp81
	.byte	17
	.word	5004
	.xword	.Ltmp105
	.byte	18
	.byte	1
	.byte	82
	.byte	2
	.byte	133
	.byte	0
	.byte	18
	.byte	1
	.byte	80
	.byte	2
	.byte	132
	.byte	0
	.byte	0
	.byte	16
	.word	2640
	.xword	.Ltmp116
	.byte	16
	.word	5181
	.xword	.Ltmp118
	.byte	17
	.word	2659
	.xword	.Ltmp120
	.byte	18
	.byte	1
	.byte	82
	.byte	2
	.byte	131
	.byte	0
	.byte	18
	.byte	1
	.byte	80
	.byte	2
	.byte	132
	.byte	0
	.byte	0
	.byte	17
	.word	5004
	.xword	.Ltmp123
	.byte	18
	.byte	1
	.byte	82
	.byte	2
	.byte	133
	.byte	0
	.byte	18
	.byte	1
	.byte	80
	.byte	2
	.byte	132
	.byte	0
	.byte	0
	.byte	0
	.byte	19
	.word	925
	.word	.Linfo_string18
	.byte	4
	.byte	1
	.byte	15
	.byte	20
	.word	.Linfo_string5
	.byte	0
	.byte	20
	.word	.Linfo_string6
	.byte	1
	.byte	20
	.word	.Linfo_string7
	.byte	2
	.byte	20
	.word	.Linfo_string8
	.byte	3
	.byte	20
	.word	.Linfo_string9
	.byte	4
	.byte	20
	.word	.Linfo_string10
	.byte	5
	.byte	20
	.word	.Linfo_string11
	.byte	6
	.byte	20
	.word	.Linfo_string12
	.byte	7
	.byte	20
	.word	.Linfo_string13
	.byte	8
	.byte	20
	.word	.Linfo_string14
	.byte	9
	.byte	20
	.word	.Linfo_string15
	.byte	10
	.byte	20
	.word	.Linfo_string16
	.byte	11
	.byte	20
	.word	.Linfo_string17
	.byte	12
	.byte	0
	.byte	21
	.word	.Linfo_string4
	.byte	7
	.byte	4
	.byte	21
	.word	.Linfo_string20
	.byte	5
	.byte	4
	.byte	22
	.word	.Linfo_string21
	.word	960
	.byte	2
	.byte	55
	.byte	9
	.byte	3
	.xword	global_waveform
	.byte	23
	.word	965
	.byte	24
	.word	.Linfo_string45
	.hword	2388
	.byte	2
	.byte	30
	.byte	25
	.word	.Linfo_string22
	.word	932
	.byte	2
	.byte	31
	.byte	0
	.byte	25
	.word	.Linfo_string23
	.word	1239
	.byte	2
	.byte	32
	.byte	4
	.byte	25
	.word	.Linfo_string26
	.word	1239
	.byte	2
	.byte	33
	.byte	20
	.byte	25
	.word	.Linfo_string27
	.word	1239
	.byte	2
	.byte	34
	.byte	36
	.byte	25
	.word	.Linfo_string28
	.word	1239
	.byte	2
	.byte	35
	.byte	52
	.byte	25
	.word	.Linfo_string29
	.word	1239
	.byte	2
	.byte	36
	.byte	68
	.byte	25
	.word	.Linfo_string30
	.word	1265
	.byte	2
	.byte	37
	.byte	84
	.byte	25
	.word	.Linfo_string31
	.word	1265
	.byte	2
	.byte	38
	.byte	148
	.byte	25
	.word	.Linfo_string32
	.word	1265
	.byte	2
	.byte	39
	.byte	212
	.byte	26
	.word	.Linfo_string33
	.word	1265
	.byte	2
	.byte	40
	.hword	276
	.byte	26
	.word	.Linfo_string34
	.word	1265
	.byte	2
	.byte	41
	.hword	340
	.byte	26
	.word	.Linfo_string35
	.word	1265
	.byte	2
	.byte	42
	.hword	404
	.byte	26
	.word	.Linfo_string36
	.word	1265
	.byte	2
	.byte	43
	.hword	468
	.byte	26
	.word	.Linfo_string37
	.word	1265
	.byte	2
	.byte	44
	.hword	532
	.byte	26
	.word	.Linfo_string38
	.word	1277
	.byte	2
	.byte	45
	.hword	596
	.byte	26
	.word	.Linfo_string39
	.word	1277
	.byte	2
	.byte	46
	.hword	852
	.byte	26
	.word	.Linfo_string40
	.word	1277
	.byte	2
	.byte	47
	.hword	1108
	.byte	26
	.word	.Linfo_string41
	.word	1277
	.byte	2
	.byte	48
	.hword	1364
	.byte	26
	.word	.Linfo_string42
	.word	1277
	.byte	2
	.byte	49
	.hword	1620
	.byte	26
	.word	.Linfo_string43
	.word	1277
	.byte	2
	.byte	50
	.hword	1876
	.byte	26
	.word	.Linfo_string44
	.word	1277
	.byte	2
	.byte	51
	.hword	2132
	.byte	0
	.byte	27
	.word	1251
	.byte	28
	.word	1258
	.byte	16
	.byte	0
	.byte	21
	.word	.Linfo_string24
	.byte	8
	.byte	1
	.byte	29
	.word	.Linfo_string25
	.byte	8
	.byte	7
	.byte	27
	.word	1251
	.byte	28
	.word	1258
	.byte	64
	.byte	0
	.byte	27
	.word	925
	.byte	28
	.word	1258
	.byte	64
	.byte	0
	.byte	22
	.word	.Linfo_string46
	.word	1310
	.byte	2
	.byte	56
	.byte	9
	.byte	3
	.xword	crc32_table
	.byte	27
	.word	925
	.byte	30
	.word	1258
	.hword	256
	.byte	0
	.byte	19
	.word	925
	.word	.Linfo_string54
	.byte	4
	.byte	2
	.byte	20
	.byte	20
	.word	.Linfo_string47
	.byte	0
	.byte	20
	.word	.Linfo_string48
	.byte	1
	.byte	20
	.word	.Linfo_string49
	.byte	2
	.byte	20
	.word	.Linfo_string50
	.byte	3
	.byte	20
	.word	.Linfo_string51
	.byte	4
	.byte	20
	.word	.Linfo_string52
	.byte	5
	.byte	20
	.word	.Linfo_string53
	.byte	6
	.byte	0
	.byte	31
	.word	925
	.byte	4
	.byte	3
	.byte	10
	.byte	20
	.word	.Linfo_string55
	.byte	0
	.byte	20
	.word	.Linfo_string56
	.byte	1
	.byte	0
	.byte	32
	.word	925
	.word	.Linfo_string62
	.byte	4
	.byte	4
	.hword	332
	.byte	20
	.word	.Linfo_string57
	.byte	0
	.byte	20
	.word	.Linfo_string58
	.byte	0
	.byte	20
	.word	.Linfo_string59
	.byte	1
	.byte	20
	.word	.Linfo_string60
	.byte	2
	.byte	20
	.word	.Linfo_string61
	.byte	3
	.byte	0
	.byte	23
	.word	1448
	.byte	33
	.word	1251
	.byte	23
	.word	1458
	.byte	34
	.word	1469
	.word	.Linfo_string64
	.byte	6
	.byte	21
	.byte	34
	.word	925
	.word	.Linfo_string63
	.byte	5
	.byte	27
	.byte	23
	.word	1251
	.byte	23
	.word	1490
	.byte	21
	.word	.Linfo_string65
	.byte	8
	.byte	1
	.byte	23
	.word	1502
	.byte	33
	.word	1490
	.byte	34
	.word	925
	.word	.Linfo_string66
	.byte	7
	.byte	148
	.byte	23
	.word	925
	.byte	35
	.byte	36
	.word	.Linfo_string67
	.byte	2
	.byte	91

	.word	932
	.byte	1
	.byte	37
	.word	.Linfo_string68
	.byte	2
	.byte	91
	.word	1548
	.byte	0
	.byte	23
	.word	1553
	.byte	34
	.word	1564
	.word	.Linfo_string70
	.byte	6
	.byte	17
	.byte	34
	.word	1490
	.word	.Linfo_string69
	.byte	5
	.byte	21
	.byte	36
	.word	.Linfo_string71
	.byte	2
	.byte	101

	.word	932
	.byte	1
	.byte	37
	.word	.Linfo_string72
	.byte	2
	.byte	101
	.word	960
	.byte	38
	.word	.Linfo_string73
	.byte	2
	.byte	103
	.word	1458
	.byte	38
	.word	.Linfo_string74
	.byte	2
	.byte	104
	.word	1458
	.byte	0
	.byte	36
	.word	.Linfo_string75
	.byte	2
	.byte	78

	.word	925
	.byte	1
	.byte	37
	.word	.Linfo_string76
	.byte	2
	.byte	78
	.word	1485
	.byte	37
	.word	.Linfo_string77
	.byte	2
	.byte	78
	.word	925
	.byte	38
	.word	.Linfo_string78
	.byte	2
	.byte	81
	.word	925
	.byte	38
	.word	.Linfo_string79
	.byte	2
	.byte	80
	.word	925
	.byte	0
	.byte	39
	.word	.Linfo_string80
	.byte	2
	.byte	58

	.byte	1
	.byte	37
	.word	.Linfo_string81
	.byte	2
	.byte	58
	.word	925
	.byte	37
	.word	.Linfo_string82
	.byte	2
	.byte	58
	.word	1518
	.byte	38
	.word	.Linfo_string79
	.byte	2
	.byte	60
	.word	925
	.byte	38
	.word	.Linfo_string83
	.byte	2
	.byte	61
	.word	925
	.byte	38
	.word	.Linfo_string78
	.byte	2
	.byte	62
	.word	925
	.byte	38
	.word	.Linfo_string84
	.byte	2
	.byte	60
	.word	925
	.byte	0
	.byte	40
	.xword	.Lfunc_begin0
	.word	.Lfunc_end0-.Lfunc_begin0
	.byte	1
	.byte	109

	.word	.Linfo_string136
	.byte	2
	.byte	127

	.word	932

	.byte	41
	.word	.Ldebug_loc0
	.word	.Linfo_string140
	.byte	2
	.byte	127
	.word	1523
	.byte	42
	.word	.Ldebug_loc1
	.word	.Linfo_string141
	.byte	2
	.byte	130
	.word	960
	.byte	42
	.word	.Ldebug_loc2
	.word	.Linfo_string142
	.byte	2
	.byte	129
	.word	932
	.byte	14
	.word	1524
	.word	.Ldebug_ranges0
	.byte	2
	.byte	135
	.byte	8
	.byte	7
	.word	.Ldebug_loc3
	.word	1536
	.byte	0
	.byte	14
	.word	1575
	.word	.Ldebug_ranges1
	.byte	2
	.byte	141
	.byte	8
	.byte	7
	.word	.Ldebug_loc4
	.word	1587
	.byte	8
	.word	.Ldebug_loc5
	.word	1598
	.byte	8
	.word	.Ldebug_loc6
	.word	1609
	.byte	14
	.word	1621
	.word	.Ldebug_ranges2
	.byte	2
	.byte	110
	.byte	13
	.byte	7
	.word	.Ldebug_loc10
	.word	1633
	.byte	7
	.word	.Ldebug_loc9
	.word	1644
	.byte	8
	.word	.Ldebug_loc7
	.word	1655
	.byte	8
	.word	.Ldebug_loc8
	.word	1666
	.byte	9
	.word	1678
	.xword	.Ltmp16
	.word	.Ltmp37-.Ltmp16
	.byte	2
	.byte	84
	.byte	2
	.byte	43
	.ascii	"\267\233\204&"
	.word	1686
	.byte	13
	.byte	1
	.byte	92
	.word	1708
	.byte	8
	.word	.Ldebug_loc11
	.word	1719
	.byte	8
	.word	.Ldebug_loc12
	.word	1730
	.byte	8
	.word	.Ldebug_loc13
	.word	1741
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	44
	.xword	.Lfunc_begin1
	.word	.Lfunc_end1-.Lfunc_begin1
	.byte	1
	.byte	111

	.word	.Linfo_string137
	.byte	2
	.byte	160

	.word	1443

	.byte	36
	.word	.Linfo_string85
	.byte	2
	.byte	186

	.word	1497
	.byte	1
	.byte	37
	.word	.Linfo_string86
	.byte	2
	.byte	186
	.word	932
	.byte	37
	.word	.Linfo_string87
	.byte	2
	.byte	186
	.word	932
	.byte	38
	.word	.Linfo_string88
	.byte	2
	.byte	193
	.word	1523
	.byte	38
	.word	.Linfo_string89
	.byte	2
	.byte	192
	.word	925
	.byte	38
	.word	.Linfo_string90
	.byte	2
	.byte	191
	.word	925
	.byte	38
	.word	.Linfo_string91
	.byte	2
	.byte	189
	.word	1518
	.byte	38
	.word	.Linfo_string92
	.byte	2
	.byte	190
	.word	1480
	.byte	38
	.word	.Linfo_string72
	.byte	2
	.byte	188
	.word	960
	.byte	0
	.byte	45
	.word	.Linfo_string93
	.byte	2
	.hword	429

	.word	932
	.byte	1
	.byte	46
	.word	.Linfo_string94
	.byte	2
	.hword	429
	.word	2199
	.byte	46
	.word	.Linfo_string86
	.byte	2
	.hword	429
	.word	932
	.byte	47
	.word	.Linfo_string22
	.byte	2
	.hword	433
	.word	925
	.byte	47
	.word	.Linfo_string95
	.byte	2
	.hword	432
	.word	925
	.byte	47
	.word	.Linfo_string88
	.byte	2
	.hword	431
	.word	1497
	.byte	0
	.byte	23
	.word	2204
	.byte	48
	.word	.Linfo_string99
	.byte	40
	.byte	1
	.byte	32
	.byte	25
	.word	.Linfo_string95
	.word	925
	.byte	1
	.byte	33
	.byte	0
	.byte	25
	.word	.Linfo_string96
	.word	925
	.byte	1
	.byte	34
	.byte	4
	.byte	25
	.word	.Linfo_string97
	.word	1518
	.byte	1
	.byte	35
	.byte	8
	.byte	25
	.word	.Linfo_string98
	.word	2261
	.byte	1
	.byte	36
	.byte	16
	.byte	0
	.byte	27
	.word	1548
	.byte	28
	.word	1258
	.byte	3
	.byte	0
	.byte	45
	.word	.Linfo_string100
	.byte	2
	.hword	465

	.word	932
	.byte	1
	.byte	46
	.word	.Linfo_string94
	.byte	2
	.hword	465
	.word	2199
	.byte	46
	.word	.Linfo_string101
	.byte	2
	.hword	465
	.word	834
	.byte	46
	.word	.Linfo_string102
	.byte	2
	.hword	465
	.word	932
	.byte	47
	.word	.Linfo_string95
	.byte	2
	.hword	467
	.word	932
	.byte	0
	.byte	36
	.word	.Linfo_string103
	.byte	2
	.byte	168

	.word	932
	.byte	1
	.byte	37
	.word	.Linfo_string91
	.byte	2
	.byte	168
	.word	1480
	.byte	37
	.word	.Linfo_string86
	.byte	2
	.byte	168
	.word	932
	.byte	38
	.word	.Linfo_string79
	.byte	2
	.byte	171
	.word	932
	.byte	38
	.word	.Linfo_string104
	.byte	2
	.byte	170
	.word	932
	.byte	49
	.byte	38
	.word	.Linfo_string105
	.byte	2
	.byte	173
	.word	932
	.byte	38
	.word	.Linfo_string106
	.byte	2
	.byte	173
	.word	932
	.byte	0
	.byte	49
	.byte	38
	.word	.Linfo_string107
	.byte	2
	.byte	174
	.word	932
	.byte	38
	.word	.Linfo_string108
	.byte	2
	.byte	174
	.word	932
	.byte	0
	.byte	0
	.byte	45
	.word	.Linfo_string109
	.byte	4
	.hword	539

	.word	1523
	.byte	1
	.byte	46
	.word	.Linfo_string110
	.byte	4
	.hword	539
	.word	2492
	.byte	46
	.word	.Linfo_string115
	.byte	4
	.hword	539
	.word	1507
	.byte	49
	.byte	47
	.word	.Linfo_string116
	.byte	4
	.hword	543
	.word	925
	.byte	0
	.byte	0
	.byte	34
	.word	2503
	.word	.Linfo_string114
	.byte	7
	.byte	55
	.byte	34
	.word	2514
	.word	.Linfo_string113
	.byte	8
	.byte	72
	.byte	34
	.word	2525
	.word	.Linfo_string112
	.byte	8
	.byte	16
	.byte	21
	.word	.Linfo_string111
	.byte	7
	.byte	8
	.byte	45
	.word	.Linfo_string117
	.byte	4
	.hword	687

	.word	1523
	.byte	1
	.byte	46
	.word	.Linfo_string110
	.byte	4
	.hword	687
	.word	2492
	.byte	46
	.word	.Linfo_string115
	.byte	4
	.hword	687
	.word	1507
	.byte	0
	.byte	36
	.word	.Linfo_string118
	.byte	2
	.byte	240

	.word	1518
	.byte	1
	.byte	37
	.word	.Linfo_string88
	.byte	2
	.byte	240
	.word	1497
	.byte	37
	.word	.Linfo_string22
	.byte	2
	.byte	241
	.word	925
	.byte	38
	.word	.Linfo_string119
	.byte	2
	.byte	243
	.word	1485
	.byte	0
	.byte	36
	.word	.Linfo_string120
	.byte	2
	.byte	235

	.word	1490
	.byte	1
	.byte	37
	.word	.Linfo_string88
	.byte	2
	.byte	235
	.word	1497
	.byte	0
	.byte	50
	.word	.Linfo_string121
	.byte	4
	.byte	196



	.byte	51
	.word	2653
	.byte	0
	.byte	23
	.word	2658
	.byte	52
	.byte	53
	.xword	.Lfunc_begin3
	.word	.Lfunc_end3-.Lfunc_begin3
	.byte	1
	.byte	109

	.word	2273
	.byte	7
	.word	.Ldebug_loc46
	.word	2286
	.byte	7
	.word	.Ldebug_loc47
	.word	2298
	.byte	7
	.word	.Ldebug_loc48
	.word	2310
	.byte	8
	.word	.Ldebug_loc49
	.word	2322
	.byte	6
	.word	5192
	.word	.Ldebug_ranges9
	.byte	2
	.hword	472
	.byte	15
	.byte	7
	.word	.Ldebug_loc59
	.word	5205
	.byte	7
	.word	.Ldebug_loc58
	.word	5217
	.byte	8
	.word	.Ldebug_loc54
	.word	5229
	.byte	8
	.word	.Ldebug_loc55
	.word	5241
	.byte	8
	.word	.Ldebug_loc56
	.word	5253
	.byte	8
	.word	.Ldebug_loc57
	.word	5265
	.byte	6
	.word	2024
	.word	.Ldebug_ranges10
	.byte	2
	.hword	329
	.byte	9
	.byte	7
	.word	.Ldebug_loc53
	.word	2036
	.byte	7
	.word	.Ldebug_loc52
	.word	2047
	.byte	8
	.word	.Ldebug_loc51
	.word	2113
	.byte	8
	.word	.Ldebug_loc62
	.word	2058
	.byte	8
	.word	.Ldebug_loc63
	.word	2069
	.byte	8
	.word	.Ldebug_loc64
	.word	2080
	.byte	8
	.word	.Ldebug_loc66
	.word	2091
	.byte	8
	.word	.Ldebug_loc67
	.word	2102
	.byte	14
	.word	2335
	.word	.Ldebug_ranges11
	.byte	2
	.byte	228
	.byte	7
	.byte	7
	.word	.Ldebug_loc65
	.word	2347
	.byte	7
	.word	.Ldebug_loc50
	.word	2358
	.byte	8
	.word	.Ldebug_loc60
	.word	2369
	.byte	8
	.word	.Ldebug_loc61
	.word	2380
	.byte	12
	.xword	.Ltmp136
	.word	.Ltmp138-.Ltmp136
	.byte	11
	.byte	50
	.word	2416
	.byte	11
	.byte	0
	.word	2427
	.byte	0
	.byte	0
	.byte	0
	.byte	6
	.word	2570
	.word	.Ldebug_ranges12
	.byte	2
	.hword	334
	.byte	15
	.byte	7
	.word	.Ldebug_loc71
	.word	2582
	.byte	7
	.word	.Ldebug_loc74
	.word	2593
	.byte	8
	.word	.Ldebug_loc70
	.word	2604
	.byte	14
	.word	2532
	.word	.Ldebug_ranges13
	.byte	2
	.byte	245
	.byte	8
	.byte	7
	.word	.Ldebug_loc73
	.word	2545
	.byte	7
	.word	.Ldebug_loc69
	.word	2557
	.byte	6
	.word	2440
	.word	.Ldebug_ranges14
	.byte	4
	.hword	689
	.byte	9
	.byte	7
	.word	.Ldebug_loc72
	.word	2453
	.byte	7
	.word	.Ldebug_loc68
	.word	2465
	.byte	0
	.byte	0
	.byte	0
	.byte	15
	.word	2616
	.xword	.Ltmp149
	.word	.Ltmp150-.Ltmp149
	.byte	2
	.hword	330
	.byte	14
	.byte	10
	.byte	1
	.byte	104
	.word	2628
	.byte	0
	.byte	6
	.word	5278
	.word	.Ldebug_ranges15
	.byte	2
	.hword	341
	.byte	17
	.byte	8
	.word	.Ldebug_loc77
	.word	5301
	.byte	8
	.word	.Ldebug_loc78
	.word	5313
	.byte	8
	.word	.Ldebug_loc79
	.word	5325
	.byte	8
	.word	.Ldebug_loc80
	.word	5337
	.byte	8
	.word	.Ldebug_loc81
	.word	5349
	.byte	8
	.word	.Ldebug_loc82
	.word	5361
	.byte	8
	.word	.Ldebug_loc83
	.word	5373
	.byte	8
	.word	.Ldebug_loc84
	.word	5385
	.byte	8
	.word	.Ldebug_loc85
	.word	5397
	.byte	6
	.word	2532
	.word	.Ldebug_ranges16
	.byte	2
	.hword	265
	.byte	10
	.byte	7
	.word	.Ldebug_loc76
	.word	2545
	.byte	7
	.word	.Ldebug_loc87
	.word	2557
	.byte	6
	.word	2440
	.word	.Ldebug_ranges17
	.byte	4
	.hword	689
	.byte	9
	.byte	7
	.word	.Ldebug_loc75
	.word	2453
	.byte	7
	.word	.Ldebug_loc86
	.word	2465
	.byte	0
	.byte	0
	.byte	54
	.word	.Ldebug_ranges18
	.byte	8
	.word	.Ldebug_loc88
	.word	5410
	.byte	8
	.word	.Ldebug_loc89
	.word	5422
	.byte	0
	.byte	0
	.byte	0
	.byte	15
	.word	5436
	.xword	.Ltmp201
	.word	.Ltmp214-.Ltmp201
	.byte	2
	.hword	508
	.byte	15
	.byte	7
	.word	.Ldebug_loc98
	.word	5449
	.byte	7
	.word	.Ldebug_loc97
	.word	5461
	.byte	8
	.word	.Ldebug_loc94
	.word	5473
	.byte	8
	.word	.Ldebug_loc95
	.word	5485
	.byte	8
	.word	.Ldebug_loc96
	.word	5497
	.byte	15
	.word	2024
	.xword	.Ltmp201
	.word	.Ltmp214-.Ltmp201
	.byte	2
	.hword	377
	.byte	9
	.byte	7
	.word	.Ldebug_loc93
	.word	2036
	.byte	7
	.word	.Ldebug_loc92
	.word	2047
	.byte	8
	.word	.Ldebug_loc91
	.word	2113
	.byte	8
	.word	.Ldebug_loc101
	.word	2058
	.byte	8
	.word	.Ldebug_loc102
	.word	2069
	.byte	8
	.word	.Ldebug_loc103
	.word	2080
	.byte	8
	.word	.Ldebug_loc105
	.word	2091
	.byte	8
	.word	.Ldebug_loc106
	.word	2102
	.byte	14
	.word	2335
	.word	.Ldebug_ranges19
	.byte	2
	.byte	228
	.byte	7
	.byte	7
	.word	.Ldebug_loc104
	.word	2347
	.byte	7
	.word	.Ldebug_loc90
	.word	2358
	.byte	8
	.word	.Ldebug_loc99
	.word	2369
	.byte	8
	.word	.Ldebug_loc100
	.word	2380
	.byte	12
	.xword	.Ltmp203
	.word	.Ltmp205-.Ltmp203
	.byte	11
	.byte	50
	.word	2416
	.byte	13
	.byte	1
	.byte	90
	.word	2427
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	15
	.word	5510
	.xword	.Ltmp214
	.word	.Ltmp224-.Ltmp214
	.byte	2
	.hword	502
	.byte	15
	.byte	7
	.word	.Ldebug_loc116
	.word	5523
	.byte	7
	.word	.Ldebug_loc115
	.word	5535
	.byte	7
	.word	.Ldebug_loc114
	.word	5547
	.byte	8
	.word	.Ldebug_loc111
	.word	5559
	.byte	8
	.word	.Ldebug_loc112
	.word	5571
	.byte	8
	.word	.Ldebug_loc113
	.word	5583
	.byte	15
	.word	2024
	.xword	.Ltmp214
	.word	.Ltmp224-.Ltmp214
	.byte	2
	.hword	358
	.byte	9
	.byte	7
	.word	.Ldebug_loc110
	.word	2036
	.byte	7
	.word	.Ldebug_loc109
	.word	2047
	.byte	8
	.word	.Ldebug_loc108
	.word	2113
	.byte	8
	.word	.Ldebug_loc119
	.word	2058
	.byte	8
	.word	.Ldebug_loc120
	.word	2069
	.byte	8
	.word	.Ldebug_loc121
	.word	2080
	.byte	8
	.word	.Ldebug_loc123
	.word	2091
	.byte	8
	.word	.Ldebug_loc124
	.word	2102
	.byte	14
	.word	2335
	.word	.Ldebug_ranges20
	.byte	2
	.byte	228
	.byte	7
	.byte	7
	.word	.Ldebug_loc122
	.word	2347
	.byte	7
	.word	.Ldebug_loc107
	.word	2358
	.byte	8
	.word	.Ldebug_loc117
	.word	2369
	.byte	8
	.word	.Ldebug_loc118
	.word	2380
	.byte	12
	.xword	.Ltmp216
	.word	.Ltmp218-.Ltmp216
	.byte	11
	.byte	50
	.word	2416
	.byte	13
	.byte	1
	.byte	90
	.word	2427
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	15
	.word	5510
	.xword	.Ltmp224
	.word	.Ltmp234-.Ltmp224
	.byte	2
	.hword	478
	.byte	15
	.byte	7
	.word	.Ldebug_loc134
	.word	5523
	.byte	7
	.word	.Ldebug_loc133
	.word	5535
	.byte	7
	.word	.Ldebug_loc132
	.word	5547
	.byte	8
	.word	.Ldebug_loc129
	.word	5559
	.byte	8
	.word	.Ldebug_loc130
	.word	5571
	.byte	8
	.word	.Ldebug_loc131
	.word	5583
	.byte	15
	.word	2024
	.xword	.Ltmp224
	.word	.Ltmp234-.Ltmp224
	.byte	2
	.hword	358
	.byte	9
	.byte	7
	.word	.Ldebug_loc128
	.word	2036
	.byte	7
	.word	.Ldebug_loc127
	.word	2047
	.byte	8
	.word	.Ldebug_loc126
	.word	2113
	.byte	8
	.word	.Ldebug_loc137
	.word	2058
	.byte	8
	.word	.Ldebug_loc138
	.word	2069
	.byte	8
	.word	.Ldebug_loc139
	.word	2080
	.byte	8
	.word	.Ldebug_loc141
	.word	2091
	.byte	8
	.word	.Ldebug_loc142
	.word	2102
	.byte	14
	.word	2335
	.word	.Ldebug_ranges21
	.byte	2
	.byte	228
	.byte	7
	.byte	7
	.word	.Ldebug_loc140
	.word	2347
	.byte	7
	.word	.Ldebug_loc125
	.word	2358
	.byte	8
	.word	.Ldebug_loc135
	.word	2369
	.byte	8
	.word	.Ldebug_loc136
	.word	2380
	.byte	12
	.xword	.Ltmp226
	.word	.Ltmp228-.Ltmp226
	.byte	11
	.byte	50
	.word	2416
	.byte	13
	.byte	1
	.byte	90
	.word	2427
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	15
	.word	5510
	.xword	.Ltmp236
	.word	.Ltmp249-.Ltmp236
	.byte	2
	.hword	484
	.byte	15
	.byte	7
	.word	.Ldebug_loc152
	.word	5523
	.byte	7
	.word	.Ldebug_loc151
	.word	5535
	.byte	7
	.word	.Ldebug_loc150
	.word	5547
	.byte	8
	.word	.Ldebug_loc147
	.word	5559
	.byte	8
	.word	.Ldebug_loc148
	.word	5571
	.byte	8
	.word	.Ldebug_loc149
	.word	5583
	.byte	15
	.word	2024
	.xword	.Ltmp236
	.word	.Ltmp249-.Ltmp236
	.byte	2
	.hword	358
	.byte	9
	.byte	7
	.word	.Ldebug_loc146
	.word	2036
	.byte	7
	.word	.Ldebug_loc145
	.word	2047
	.byte	8
	.word	.Ldebug_loc144
	.word	2113
	.byte	8
	.word	.Ldebug_loc155
	.word	2058
	.byte	8
	.word	.Ldebug_loc156
	.word	2069
	.byte	8
	.word	.Ldebug_loc157
	.word	2080
	.byte	8
	.word	.Ldebug_loc159
	.word	2091
	.byte	8
	.word	.Ldebug_loc160
	.word	2102
	.byte	14
	.word	2335
	.word	.Ldebug_ranges22
	.byte	2
	.byte	228
	.byte	7
	.byte	7
	.word	.Ldebug_loc158
	.word	2347
	.byte	7
	.word	.Ldebug_loc143
	.word	2358
	.byte	8
	.word	.Ldebug_loc153
	.word	2369
	.byte	8
	.word	.Ldebug_loc154
	.word	2380
	.byte	12
	.xword	.Ltmp238
	.word	.Ltmp240-.Ltmp238
	.byte	11
	.byte	50
	.word	2416
	.byte	13
	.byte	1
	.byte	90
	.word	2427
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	15
	.word	5510
	.xword	.Ltmp249
	.word	.Ltmp262-.Ltmp249
	.byte	2
	.hword	490
	.byte	15
	.byte	7
	.word	.Ldebug_loc170
	.word	5523
	.byte	7
	.word	.Ldebug_loc169
	.word	5535
	.byte	7
	.word	.Ldebug_loc168
	.word	5547
	.byte	8
	.word	.Ldebug_loc165
	.word	5559
	.byte	8
	.word	.Ldebug_loc166
	.word	5571
	.byte	8
	.word	.Ldebug_loc167
	.word	5583
	.byte	15
	.word	2024
	.xword	.Ltmp249
	.word	.Ltmp262-.Ltmp249
	.byte	2
	.hword	358
	.byte	9
	.byte	7
	.word	.Ldebug_loc164
	.word	2036
	.byte	7
	.word	.Ldebug_loc163
	.word	2047
	.byte	8
	.word	.Ldebug_loc162
	.word	2113
	.byte	8
	.word	.Ldebug_loc173
	.word	2058
	.byte	8
	.word	.Ldebug_loc174
	.word	2069
	.byte	8
	.word	.Ldebug_loc175
	.word	2080
	.byte	8
	.word	.Ldebug_loc177
	.word	2091
	.byte	8
	.word	.Ldebug_loc178
	.word	2102
	.byte	14
	.word	2335
	.word	.Ldebug_ranges23
	.byte	2
	.byte	228
	.byte	7
	.byte	7
	.word	.Ldebug_loc176
	.word	2347
	.byte	7
	.word	.Ldebug_loc161
	.word	2358
	.byte	8
	.word	.Ldebug_loc171
	.word	2369
	.byte	8
	.word	.Ldebug_loc172
	.word	2380
	.byte	12
	.xword	.Ltmp251
	.word	.Ltmp253-.Ltmp251
	.byte	11
	.byte	50
	.word	2416
	.byte	13
	.byte	1
	.byte	90
	.word	2427
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	15
	.word	5510
	.xword	.Ltmp262
	.word	.Ltmp275-.Ltmp262
	.byte	2
	.hword	496
	.byte	15
	.byte	7
	.word	.Ldebug_loc188
	.word	5523
	.byte	7
	.word	.Ldebug_loc187
	.word	5535
	.byte	7
	.word	.Ldebug_loc186
	.word	5547
	.byte	8
	.word	.Ldebug_loc183
	.word	5559
	.byte	8
	.word	.Ldebug_loc184
	.word	5571
	.byte	8
	.word	.Ldebug_loc185
	.word	5583
	.byte	15
	.word	2024
	.xword	.Ltmp262
	.word	.Ltmp275-.Ltmp262
	.byte	2
	.hword	358
	.byte	9
	.byte	7
	.word	.Ldebug_loc182
	.word	2036
	.byte	7
	.word	.Ldebug_loc181
	.word	2047
	.byte	8
	.word	.Ldebug_loc180
	.word	2113
	.byte	8
	.word	.Ldebug_loc191
	.word	2058
	.byte	8
	.word	.Ldebug_loc192
	.word	2069
	.byte	8
	.word	.Ldebug_loc193
	.word	2080
	.byte	8
	.word	.Ldebug_loc195
	.word	2091
	.byte	8
	.word	.Ldebug_loc196
	.word	2102
	.byte	14
	.word	2335
	.word	.Ldebug_ranges24
	.byte	2
	.byte	228
	.byte	7
	.byte	7
	.word	.Ldebug_loc194
	.word	2347
	.byte	7
	.word	.Ldebug_loc179
	.word	2358
	.byte	8
	.word	.Ldebug_loc189
	.word	2369
	.byte	8
	.word	.Ldebug_loc190
	.word	2380
	.byte	12
	.xword	.Ltmp264
	.word	.Ltmp266-.Ltmp264
	.byte	11
	.byte	50
	.word	2416
	.byte	13
	.byte	1
	.byte	90
	.word	2427
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	15
	.word	2125
	.xword	.Ltmp275
	.word	.Ltmp287-.Ltmp275
	.byte	2
	.hword	520
	.byte	15
	.byte	7
	.word	.Ldebug_loc205
	.word	2138
	.byte	7
	.word	.Ldebug_loc204
	.word	2150
	.byte	8
	.word	.Ldebug_loc201
	.word	2162
	.byte	8
	.word	.Ldebug_loc202
	.word	2174
	.byte	8
	.word	.Ldebug_loc203
	.word	2186
	.byte	15
	.word	2024
	.xword	.Ltmp275
	.word	.Ltmp287-.Ltmp275
	.byte	2
	.hword	435
	.byte	9
	.byte	7
	.word	.Ldebug_loc200
	.word	2036
	.byte	7
	.word	.Ldebug_loc199
	.word	2047
	.byte	8
	.word	.Ldebug_loc198
	.word	2113
	.byte	8
	.word	.Ldebug_loc208
	.word	2058
	.byte	8
	.word	.Ldebug_loc209
	.word	2069
	.byte	8
	.word	.Ldebug_loc210
	.word	2080
	.byte	8
	.word	.Ldebug_loc212
	.word	2091
	.byte	8
	.word	.Ldebug_loc213
	.word	2102
	.byte	14
	.word	2335
	.word	.Ldebug_ranges25
	.byte	2
	.byte	228
	.byte	7
	.byte	7
	.word	.Ldebug_loc211
	.word	2347
	.byte	7
	.word	.Ldebug_loc197
	.word	2358
	.byte	8
	.word	.Ldebug_loc206
	.word	2369
	.byte	8
	.word	.Ldebug_loc207
	.word	2380
	.byte	12
	.xword	.Ltmp277
	.word	.Ltmp279-.Ltmp277
	.byte	11
	.byte	50
	.word	2416
	.byte	13
	.byte	1
	.byte	90
	.word	2427
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	17
	.word	2640
	.xword	.Ltmp199
	.byte	18
	.byte	1
	.byte	80
	.byte	2
	.byte	133
	.byte	0
	.byte	0
	.byte	17
	.word	2640
	.xword	.Ltmp301
	.byte	18
	.byte	1
	.byte	80
	.byte	2
	.byte	133
	.byte	0
	.byte	0
	.byte	0
	.byte	55
	.xword	.Lfunc_begin4
	.word	.Lfunc_end4-.Lfunc_begin4
	.byte	1
	.byte	111

	.word	.Linfo_string139
	.byte	2
	.hword	447

	.word	932
	.byte	56
	.byte	1
	.byte	80
	.word	.Linfo_string97
	.byte	2
	.hword	447
	.word	2199
	.byte	56
	.byte	1
	.byte	81
	.word	.Linfo_string98
	.byte	2
	.hword	447
	.word	1548
	.byte	56
	.byte	1
	.byte	82
	.word	.Linfo_string95
	.byte	2
	.hword	447
	.word	932
	.byte	5
	.word	.Ldebug_loc214
	.word	.Linfo_string79
	.byte	2
	.hword	449
	.word	932
	.byte	5
	.word	.Ldebug_loc215
	.word	.Linfo_string145
	.byte	2
	.hword	450
	.word	1458
	.byte	5
	.word	.Ldebug_loc216
	.word	.Linfo_string146
	.byte	2
	.hword	450
	.word	1458
	.byte	12
	.xword	.Ltmp307
	.word	.Ltmp347-.Ltmp307
	.byte	5
	.word	.Ldebug_loc217
	.word	.Linfo_string147
	.byte	2
	.hword	453
	.word	1518
	.byte	12
	.xword	.Ltmp313
	.word	.Ltmp345-.Ltmp313
	.byte	5
	.word	.Ldebug_loc218
	.word	.Linfo_string148
	.byte	2
	.hword	456
	.word	1553
	.byte	0
	.byte	0
	.byte	0
	.byte	57
	.word	.Linfo_string122
	.byte	1
	.byte	48

	.word	932


	.byte	45
	.word	.Linfo_string123
	.byte	2
	.hword	321

	.word	932
	.byte	1
	.byte	46
	.word	.Linfo_string94
	.byte	2
	.hword	321
	.word	2199
	.byte	46
	.word	.Linfo_string86
	.byte	2
	.hword	321
	.word	932
	.byte	47
	.word	.Linfo_string22
	.byte	2
	.hword	326
	.word	925
	.byte	47
	.word	.Linfo_string95
	.byte	2
	.hword	325
	.word	925
	.byte	47
	.word	.Linfo_string88
	.byte	2
	.hword	324
	.word	1497
	.byte	47
	.word	.Linfo_string124
	.byte	2
	.hword	323
	.word	2204
	.byte	0
	.byte	36
	.word	.Linfo_string125
	.byte	2
	.byte	254

	.word	1518
	.byte	1
	.byte	37
	.word	.Linfo_string124
	.byte	2
	.byte	254
	.word	2199
	.byte	47
	.word	.Linfo_string126
	.byte	2
	.hword	260
	.word	932
	.byte	47
	.word	.Linfo_string127
	.byte	2
	.hword	259
	.word	932
	.byte	47
	.word	.Linfo_string128
	.byte	2
	.hword	258
	.word	932
	.byte	47
	.word	.Linfo_string129
	.byte	2
	.hword	257
	.word	932
	.byte	47
	.word	.Linfo_string130
	.byte	2
	.hword	257
	.word	932
	.byte	47
	.word	.Linfo_string84
	.byte	2
	.hword	257
	.word	932
	.byte	47
	.word	.Linfo_string79
	.byte	2
	.hword	257
	.word	932
	.byte	47
	.word	.Linfo_string131
	.byte	2
	.hword	256
	.word	1518
	.byte	47
	.word	.Linfo_string132
	.byte	2
	.hword	256
	.word	1518
	.byte	49
	.byte	47
	.word	.Linfo_string86
	.byte	2
	.hword	272
	.word	932
	.byte	47
	.word	.Linfo_string133
	.byte	2
	.hword	273
	.word	932
	.byte	0
	.byte	0
	.byte	45
	.word	.Linfo_string134
	.byte	2
	.hword	370

	.word	932
	.byte	1
	.byte	46
	.word	.Linfo_string94
	.byte	2
	.hword	370
	.word	2199
	.byte	46
	.word	.Linfo_string86
	.byte	2
	.hword	370
	.word	932
	.byte	47
	.word	.Linfo_string22
	.byte	2
	.hword	374
	.word	925
	.byte	47
	.word	.Linfo_string95
	.byte	2
	.hword	373
	.word	925
	.byte	47
	.word	.Linfo_string88
	.byte	2
	.hword	372
	.word	1497
	.byte	0
	.byte	45
	.word	.Linfo_string135
	.byte	2
	.hword	351

	.word	932
	.byte	1
	.byte	46
	.word	.Linfo_string94
	.byte	2
	.hword	351
	.word	2199
	.byte	46
	.word	.Linfo_string86
	.byte	2
	.hword	351
	.word	932
	.byte	46
	.word	.Linfo_string101
	.byte	2
	.hword	351
	.word	1323
	.byte	47
	.word	.Linfo_string22
	.byte	2
	.hword	355
	.word	925
	.byte	47
	.word	.Linfo_string95
	.byte	2
	.hword	354
	.word	925
	.byte	47
	.word	.Linfo_string88
	.byte	2
	.hword	353
	.word	1497
	.byte	0
	.byte	0
.Ldebug_info_end0:
	.section	.debug_ranges,"",@progbits
.Ldebug_ranges0:
	.xword	.Ltmp4-.Lfunc_begin0
	.xword	.Ltmp5-.Lfunc_begin0
	.xword	.Ltmp6-.Lfunc_begin0
	.xword	.Ltmp8-.Lfunc_begin0
	.xword	.Ltmp47-.Lfunc_begin0
	.xword	.Ltmp48-.Lfunc_begin0
	.xword	0
	.xword	0
.Ldebug_ranges1:
	.xword	.Ltmp8-.Lfunc_begin0
	.xword	.Ltmp44-.Lfunc_begin0
	.xword	.Ltmp49-.Lfunc_begin0
	.xword	.Ltmp50-.Lfunc_begin0
	.xword	0
	.xword	0
.Ldebug_ranges2:
	.xword	.Ltmp11-.Lfunc_begin0
	.xword	.Ltmp12-.Lfunc_begin0
	.xword	.Ltmp14-.Lfunc_begin0
	.xword	.Ltmp43-.Lfunc_begin0
	.xword	0
	.xword	0
.Ldebug_ranges3:
	.xword	.Ltmp82-.Lfunc_begin0
	.xword	.Ltmp102-.Lfunc_begin0
	.xword	.Ltmp103-.Lfunc_begin0
	.xword	.Ltmp104-.Lfunc_begin0
	.xword	.Ltmp126-.Lfunc_begin0
	.xword	.Ltmp127-.Lfunc_begin0
	.xword	0
	.xword	0
.Ldebug_ranges4:
	.xword	.Ltmp82-.Lfunc_begin0
	.xword	.Ltmp102-.Lfunc_begin0
	.xword	.Ltmp103-.Lfunc_begin0
	.xword	.Ltmp104-.Lfunc_begin0
	.xword	.Ltmp126-.Lfunc_begin0
	.xword	.Ltmp127-.Lfunc_begin0
	.xword	0
	.xword	0
.Ldebug_ranges5:
	.xword	.Ltmp82-.Lfunc_begin0
	.xword	.Ltmp93-.Lfunc_begin0
	.xword	.Ltmp94-.Lfunc_begin0
	.xword	.Ltmp96-.Lfunc_begin0
	.xword	0
	.xword	0
.Ldebug_ranges6:
	.xword	.Ltmp93-.Lfunc_begin0
	.xword	.Ltmp94-.Lfunc_begin0
	.xword	.Ltmp98-.Lfunc_begin0
	.xword	.Ltmp102-.Lfunc_begin0
	.xword	0
	.xword	0
.Ldebug_ranges7:
	.xword	.Ltmp93-.Lfunc_begin0
	.xword	.Ltmp94-.Lfunc_begin0
	.xword	.Ltmp98-.Lfunc_begin0
	.xword	.Ltmp99-.Lfunc_begin0
	.xword	0
	.xword	0
.Ldebug_ranges8:
	.xword	.Ltmp93-.Lfunc_begin0
	.xword	.Ltmp94-.Lfunc_begin0
	.xword	.Ltmp98-.Lfunc_begin0
	.xword	.Ltmp99-.Lfunc_begin0
	.xword	0
	.xword	0
.Ldebug_ranges9:
	.xword	.Ltmp134-.Lfunc_begin0
	.xword	.Ltmp200-.Lfunc_begin0
	.xword	.Ltmp300-.Lfunc_begin0
	.xword	.Ltmp303-.Lfunc_begin0
	.xword	0
	.xword	0
.Ldebug_ranges10:
	.xword	.Ltmp134-.Lfunc_begin0
	.xword	.Ltmp146-.Lfunc_begin0
	.xword	.Ltmp147-.Lfunc_begin0
	.xword	.Ltmp149-.Lfunc_begin0
	.xword	0
	.xword	0
.Ldebug_ranges11:
	.xword	.Ltmp135-.Lfunc_begin0
	.xword	.Ltmp138-.Lfunc_begin0
	.xword	.Ltmp139-.Lfunc_begin0
	.xword	.Ltmp144-.Lfunc_begin0
	.xword	0
	.xword	0
.Ldebug_ranges12:
	.xword	.Ltmp146-.Lfunc_begin0
	.xword	.Ltmp147-.Lfunc_begin0
	.xword	.Ltmp152-.Lfunc_begin0
	.xword	.Ltmp156-.Lfunc_begin0
	.xword	0
	.xword	0
.Ldebug_ranges13:
	.xword	.Ltmp146-.Lfunc_begin0
	.xword	.Ltmp147-.Lfunc_begin0
	.xword	.Ltmp152-.Lfunc_begin0
	.xword	.Ltmp153-.Lfunc_begin0
	.xword	0
	.xword	0
.Ldebug_ranges14:
	.xword	.Ltmp146-.Lfunc_begin0
	.xword	.Ltmp147-.Lfunc_begin0
	.xword	.Ltmp152-.Lfunc_begin0
	.xword	.Ltmp153-.Lfunc_begin0
	.xword	0
	.xword	0
.Ldebug_ranges15:
	.xword	.Ltmp156-.Lfunc_begin0
	.xword	.Ltmp162-.Lfunc_begin0
	.xword	.Ltmp164-.Lfunc_begin0
	.xword	.Ltmp198-.Lfunc_begin0
	.xword	0
	.xword	0
.Ldebug_ranges16:
	.xword	.Ltmp157-.Lfunc_begin0
	.xword	.Ltmp158-.Lfunc_begin0
	.xword	.Ltmp159-.Lfunc_begin0
	.xword	.Ltmp160-.Lfunc_begin0
	.xword	0
	.xword	0
.Ldebug_ranges17:
	.xword	.Ltmp157-.Lfunc_begin0
	.xword	.Ltmp158-.Lfunc_begin0
	.xword	.Ltmp159-.Lfunc_begin0
	.xword	.Ltmp160-.Lfunc_begin0
	.xword	0
	.xword	0
.Ldebug_ranges18:
	.xword	.Ltmp168-.Lfunc_begin0
	.xword	.Ltmp171-.Lfunc_begin0
	.xword	.Ltmp173-.Lfunc_begin0
	.xword	.Ltmp174-.Lfunc_begin0
	.xword	.Ltmp175-.Lfunc_begin0
	.xword	.Ltmp194-.Lfunc_begin0
	.xword	0
	.xword	0
.Ldebug_ranges19:
	.xword	.Ltmp202-.Lfunc_begin0
	.xword	.Ltmp205-.Lfunc_begin0
	.xword	.Ltmp206-.Lfunc_begin0
	.xword	.Ltmp211-.Lfunc_begin0
	.xword	0
	.xword	0
.Ldebug_ranges20:
	.xword	.Ltmp215-.Lfunc_begin0
	.xword	.Ltmp218-.Lfunc_begin0
	.xword	.Ltmp219-.Lfunc_begin0
	.xword	.Ltmp224-.Lfunc_begin0
	.xword	0
	.xword	0
.Ldebug_ranges21:
	.xword	.Ltmp225-.Lfunc_begin0
	.xword	.Ltmp228-.Lfunc_begin0
	.xword	.Ltmp229-.Lfunc_begin0
	.xword	.Ltmp234-.Lfunc_begin0
	.xword	0
	.xword	0
.Ldebug_ranges22:
	.xword	.Ltmp237-.Lfunc_begin0
	.xword	.Ltmp240-.Lfunc_begin0
	.xword	.Ltmp241-.Lfunc_begin0
	.xword	.Ltmp246-.Lfunc_begin0
	.xword	0
	.xword	0
.Ldebug_ranges23:
	.xword	.Ltmp250-.Lfunc_begin0
	.xword	.Ltmp253-.Lfunc_begin0
	.xword	.Ltmp254-.Lfunc_begin0
	.xword	.Ltmp259-.Lfunc_begin0
	.xword	0
	.xword	0
.Ldebug_ranges24:
	.xword	.Ltmp263-.Lfunc_begin0
	.xword	.Ltmp266-.Lfunc_begin0
	.xword	.Ltmp267-.Lfunc_begin0
	.xword	.Ltmp272-.Lfunc_begin0
	.xword	0
	.xword	0
.Ldebug_ranges25:
	.xword	.Ltmp276-.Lfunc_begin0
	.xword	.Ltmp279-.Lfunc_begin0
	.xword	.Ltmp280-.Lfunc_begin0
	.xword	.Ltmp285-.Lfunc_begin0
	.xword	0
	.xword	0
	.section	.debug_str,"MS",@progbits,1
.Linfo_string0:
	.asciz	"Android (8508608, based on r450784e) clang version 14.0.7 (https://android.googlesource.com/toolchain/llvm-project 4c603efb0cca074e9238af8b4106c30add4418f6)"
.Linfo_string1:
	.asciz	"drivers/gpu/drm/rockchip/ebc-dev/epdlut/rkf_waveform.c"
.Linfo_string2:
	.asciz	"/home/lyx/rk-android14/kernel-6.1"
.Linfo_string3:
	.asciz	"stype"
.Linfo_string4:
	.asciz	"unsigned int"
.Linfo_string5:
	.asciz	"WF_TYPE_RESET"
.Linfo_string6:
	.asciz	"WF_TYPE_GRAY2"
.Linfo_string7:
	.asciz	"WF_TYPE_GRAY4"
.Linfo_string8:
	.asciz	"WF_TYPE_GC16"
.Linfo_string9:
	.asciz	"WF_TYPE_GL16"
.Linfo_string10:
	.asciz	"WF_TYPE_GLR16"
.Linfo_string11:
	.asciz	"WF_TYPE_GLD16"
.Linfo_string12:
	.asciz	"WF_TYPE_A2"
.Linfo_string13:
	.asciz	"WF_TYPE_GCC16"
.Linfo_string14:
	.asciz	"PVI_WF_MAX"
.Linfo_string15:
	.asciz	"WF_TYPE_AUTO"
.Linfo_string16:
	.asciz	"WF_TYPE_MAX"
.Linfo_string17:
	.asciz	"WF_TYPE_GRAY16"
.Linfo_string18:
	.asciz	"epd_lut_type"
.Linfo_string19:
	.asciz	"sftemp"
.Linfo_string20:
	.asciz	"int"
.Linfo_string21:
	.asciz	"global_waveform"
.Linfo_string22:
	.asciz	"length"
.Linfo_string23:
	.asciz	"format"
.Linfo_string24:
	.asciz	"char"
.Linfo_string25:
	.asciz	"__ARRAY_SIZE_TYPE__"
.Linfo_string26:
	.asciz	"version"
.Linfo_string27:
	.asciz	"timeandday"
.Linfo_string28:
	.asciz	"panel_name"
.Linfo_string29:
	.asciz	"panel_info"
.Linfo_string30:
	.asciz	"full_version"
.Linfo_string31:
	.asciz	"reset_temp_list"
.Linfo_string32:
	.asciz	"gc16_temp_list"
.Linfo_string33:
	.asciz	"gl16_temp_list"
.Linfo_string34:
	.asciz	"glr16_temp_list"
.Linfo_string35:
	.asciz	"gld16_temp_list"
.Linfo_string36:
	.asciz	"du_temp_list"
.Linfo_string37:
	.asciz	"a2_temp_list"
.Linfo_string38:
	.asciz	"reset_list"
.Linfo_string39:
	.asciz	"gc16_list"
.Linfo_string40:
	.asciz	"gl16_list"
.Linfo_string41:
	.asciz	"glr16_list"
.Linfo_string42:
	.asciz	"gld16_list"
.Linfo_string43:
	.asciz	"du_list"
.Linfo_string44:
	.asciz	"a2_list"
.Linfo_string45:
	.asciz	"rkf_waveform"
.Linfo_string46:
	.asciz	"crc32_table"
.Linfo_string47:
	.asciz	"RKF_WF_RESET"
.Linfo_string48:
	.asciz	"RKF_WF_DU"
.Linfo_string49:
	.asciz	"RKF_WF_GC16"
.Linfo_string50:
	.asciz	"RKF_WF_GL16"
.Linfo_string51:
	.asciz	"RKF_WF_GLR16"
.Linfo_string52:
	.asciz	"RKF_WF_GLD16"
.Linfo_string53:
	.asciz	"RKF_WF_A2"
.Linfo_string54:
	.asciz	"rkf_waveform_type"
.Linfo_string55:
	.asciz	"false"
.Linfo_string56:
	.asciz	"true"
.Linfo_string57:
	.asciz	"KMALLOC_NORMAL"
.Linfo_string58:
	.asciz	"KMALLOC_DMA"
.Linfo_string59:
	.asciz	"KMALLOC_CGROUP"
.Linfo_string60:
	.asciz	"KMALLOC_RECLAIM"
.Linfo_string61:
	.asciz	"NR_KMALLOC_TYPES"
.Linfo_string62:
	.asciz	"kmalloc_cache_type"
.Linfo_string63:
	.asciz	"__u32"
.Linfo_string64:
	.asciz	"u32"
.Linfo_string65:
	.asciz	"unsigned char"
.Linfo_string66:
	.asciz	"gfp_t"
.Linfo_string67:
	.asciz	"check_wf_format"
.Linfo_string68:
	.asciz	"buf"
.Linfo_string69:
	.asciz	"__u8"
.Linfo_string70:
	.asciz	"u8"
.Linfo_string71:
	.asciz	"check_wf_crc"
.Linfo_string72:
	.asciz	"waveform"
.Linfo_string73:
	.asciz	"crccheck"
.Linfo_string74:
	.asciz	"readcrc"
.Linfo_string75:
	.asciz	"crc_32"
.Linfo_string76:
	.asciz	"a_data"
.Linfo_string77:
	.asciz	"a_size"
.Linfo_string78:
	.asciz	"n_accum"
.Linfo_string79:
	.asciz	"i"
.Linfo_string80:
	.asciz	"crc_build_table32"
.Linfo_string81:
	.asciz	"a_poly"
.Linfo_string82:
	.asciz	"crc_table"
.Linfo_string83:
	.asciz	"n_data"
.Linfo_string84:
	.asciz	"j"
.Linfo_string85:
	.asciz	"get_wf_buf"
.Linfo_string86:
	.asciz	"temp"
.Linfo_string87:
	.asciz	"mode"
.Linfo_string88:
	.asciz	"pbuf"
.Linfo_string89:
	.asciz	"pt"
.Linfo_string90:
	.asciz	"wf_offset"
.Linfo_string91:
	.asciz	"list"
.Linfo_string92:
	.asciz	"templist"
.Linfo_string93:
	.asciz	"parse_wf_a2"
.Linfo_string94:
	.asciz	"output"
.Linfo_string95:
	.asciz	"frame_num"
.Linfo_string96:
	.asciz	"repair_frame_num"
.Linfo_string97:
	.asciz	"data"
.Linfo_string98:
	.asciz	"wf_table"
.Linfo_string99:
	.asciz	"epd_lut_data"
.Linfo_string100:
	.asciz	"parse_wf_data"
.Linfo_string101:
	.asciz	"lut_type"
.Linfo_string102:
	.asciz	"temperture"
.Linfo_string103:
	.asciz	"get_wf_level"
.Linfo_string104:
	.asciz	"level"
.Linfo_string105:
	.asciz	"__UNIQUE_ID___y347"
.Linfo_string106:
	.asciz	"__UNIQUE_ID___x346"
.Linfo_string107:
	.asciz	"__UNIQUE_ID___y349"
.Linfo_string108:
	.asciz	"__UNIQUE_ID___x348"
.Linfo_string109:
	.asciz	"kmalloc"
.Linfo_string110:
	.asciz	"size"
.Linfo_string111:
	.asciz	"unsigned long"
.Linfo_string112:
	.asciz	"__kernel_ulong_t"
.Linfo_string113:
	.asciz	"__kernel_size_t"
.Linfo_string114:
	.asciz	"size_t"
.Linfo_string115:
	.asciz	"flags"
.Linfo_string116:
	.asciz	"index"
.Linfo_string117:
	.asciz	"kzalloc"
.Linfo_string118:
	.asciz	"decode_wf_data"
.Linfo_string119:
	.asciz	"out"
.Linfo_string120:
	.asciz	"get_wf_frm_num"
.Linfo_string121:
	.asciz	"kfree"
.Linfo_string122:
	.asciz	"epd_overlay_lut"
.Linfo_string123:
	.asciz	"parse_wf_reset"
.Linfo_string124:
	.asciz	"input"
.Linfo_string125:
	.asciz	"decode_wf_reset"
.Linfo_string126:
	.asciz	"len"
.Linfo_string127:
	.asciz	"num"
.Linfo_string128:
	.asciz	"temp_data"
.Linfo_string129:
	.asciz	"h"
.Linfo_string130:
	.asciz	"k"
.Linfo_string131:
	.asciz	"odata"
.Linfo_string132:
	.asciz	"idata"
.Linfo_string133:
	.asciz	"temp1"
.Linfo_string134:
	.asciz	"parse_wf_gray2"
.Linfo_string135:
	.asciz	"parse_wf_gray16"
.Linfo_string136:
	.asciz	"rkf_wf_input"
.Linfo_string137:
	.asciz	"rkf_wf_get_version"
.Linfo_string138:
	.asciz	"rkf_wf_get_lut"
.Linfo_string139:
	.asciz	"rkf_lut_init_wf_table"
.Linfo_string140:
	.asciz	"waveform_file"
.Linfo_string141:
	.asciz	"wf"
.Linfo_string142:
	.asciz	"ret"
.Linfo_string143:
	.asciz	"pic"
.Linfo_string144:
	.asciz	"overlay_lut"
.Linfo_string145:
	.asciz	"old_gray"
.Linfo_string146:
	.asciz	"new_gray"
.Linfo_string147:
	.asciz	"lut_data"
.Linfo_string148:
	.asciz	"pix0"
	.ident	"Android (8508608, based on r450784e) clang version 14.0.7 (https://android.googlesource.com/toolchain/llvm-project 4c603efb0cca074e9238af8b4106c30add4418f6)"
	.section	".note.GNU-stack","",@progbits
	.addrsig
	.section	.debug_line,"",@progbits
.Lline_table_start0:
