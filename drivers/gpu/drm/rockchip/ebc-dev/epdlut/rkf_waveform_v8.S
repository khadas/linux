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
	.loc	2 564 0
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
	.loc	2 570 7 prologue_end
	adrp	x23, global_waveform
	ldr	x8, [x23, :lo12:global_waveform]
.Ltmp57:
	.loc	2 570 6 is_stmt 0
	cbz	x8, .LBB2_27
.Ltmp58:
	.loc	2 0 6
	mov	x20, x0
.Ltmp59:
	.loc	2 573 6 is_stmt 1
	cbz	x0, .LBB2_28
.Ltmp60:
	.loc	2 576 6
	cmp	w3, #32
	b.eq	.LBB2_33
.Ltmp61:
	.loc	2 587 28
	adrp	x24, rkf_wf_get_lut.sftemp
	.loc	2 587 19 is_stmt 0
	mov	w8, #26215
	movk	w8, #26214, lsl #16
.Ltmp62:
	.loc	2 581 6 is_stmt 1
	cmp	w1, #8
	mov	w11, #5
	mov	w19, w2
.Ltmp63:
	.loc	2 587 28
	ldrsw	x9, [x24, :lo12:rkf_wf_get_lut.sftemp]
	.loc	2 587 19 is_stmt 0
	smull	x10, w2, w8
.Ltmp64:
	.loc	2 581 6 is_stmt 1
	csel	w21, w11, w1, eq
.Ltmp65:
	.loc	2 0 6 is_stmt 0
	adrp	x26, rkf_wf_get_lut.stype
.Ltmp66:
	.loc	2 587 35 is_stmt 1
	mul	x8, x9, x8
	.loc	2 587 19 is_stmt 0
	lsr	x9, x10, #63
	asr	x10, x10, #33
	add	w25, w10, w9
	.loc	2 587 35
	lsr	x9, x8, #63
	asr	x8, x8, #33
	add	w8, w8, w9
	.loc	2 587 41
	cmp	w25, w8
	b.ne	.LBB2_5
.Ltmp67:
	.loc	2 0 41
	ldr	w9, [x26, :lo12:rkf_wf_get_lut.stype]
	.loc	2 587 41
	cmp	w9, w21
	b.eq	.LBB2_26
.Ltmp68:
.LBB2_5:
	.loc	2 590 6 is_stmt 1
	cmp	w21, #10
	b.eq	.LBB2_25
.Ltmp69:
	.loc	2 592 15
	ldr	x0, [x20, #8]
.Ltmp70:
	.loc	2 592 7 is_stmt 0
	cbz	x0, .LBB2_8
.Ltmp71:
	.loc	2 593 4 is_stmt 1
	bl	kfree
.Ltmp72:
	.loc	2 594 17
	str	xzr, [x20, #8]
.Ltmp73:
.LBB2_8:
	.loc	2 597 15
	mov	x0, x20
	mov	w1, w21
	mov	w2, w19
	bl	parse_wf_data
.Ltmp74:
	.loc	2 598 7
	tbnz	w0, #31, .LBB2_35
.Ltmp75:
	.loc	2 0 0 is_stmt 0
	mov	w22, w0
.Ltmp76:
	.loc	2 602 33 is_stmt 1
	ldr	x1, [x20, #16]
	.loc	2 602 3 is_stmt 0
	mov	x0, x20
	mov	w2, w22
	bl	rkf_lut_init_wf_table
.Ltmp77:
	.loc	2 604 30 is_stmt 1
	cmp	w21, #7
	b.eq	.LBB2_11
.Ltmp78:
	cmp	w21, #1
	b.ne	.LBB2_17
.Ltmp79:
.LBB2_11:
	.loc	2 537 2
	cbz	w22, .LBB2_17
.Ltmp80:
	.loc	2 537 16 is_stmt 0
	mov	w10, w22
	mov	x9, xzr
.Ltmp81:
	.loc	2 0 0
	ldr	x8, [x20, #16]
.Ltmp82:
	.loc	2 537 2
	lsl	x10, x10, #10
.Ltmp83:
.LBB2_13:
	.loc	2 538 20 is_stmt 1
	add	x11, x8, w9, sxtw
.Ltmp84:
	.loc	2 537 16
	add	x9, x9, #1024
.Ltmp85:
	.loc	2 537 2 is_stmt 0
	cmp	x10, x9
.Ltmp86:
	.loc	2 539 38 is_stmt 1
	ldrb	w12, [x11, #960]
	.loc	2 540 38
	ldrb	w13, [x11, #30]
	.loc	2 539 36
	strb	w12, [x11, #832]
	.loc	2 540 36
	strb	w13, [x11, #862]
	.loc	2 541 36
	strb	w12, [x11, #896]
	.loc	2 542 36
	strb	w13, [x11, #94]
.Ltmp87:
	.loc	2 537 2
	b.ne	.LBB2_13
.Ltmp88:
	.loc	2 544 2
	cmp	w22, #2
	b.lt	.LBB2_17
.Ltmp89:
	.loc	2 0 2 is_stmt 0
	lsr	w10, w22, #1
	mov	x9, xzr
	.loc	2 544 2
	lsl	x10, x10, #10
.Ltmp90:
.LBB2_16:
	.loc	2 545 20 is_stmt 1
	add	x11, x8, w9, sxtw
.Ltmp91:
	.loc	2 544 16
	add	x9, x9, #1024
.Ltmp92:
	.loc	2 544 2 is_stmt 0
	cmp	x10, x9
.Ltmp93:
	.loc	2 546 38 is_stmt 1
	ldrb	w12, [x11, #30]
	.loc	2 547 38
	ldrb	w13, [x11, #960]
	.loc	2 546 36
	strb	w12, [x11, #926]
	.loc	2 547 36
	strb	w13, [x11, #64]
.Ltmp94:
	.loc	2 544 2
	b.ne	.LBB2_16
.Ltmp95:
.LBB2_17:
	.loc	2 608 21
	ldrb	w8, [x20, #1]
.Ltmp96:
	.loc	2 613 15
	ldr	x0, [x20, #8]
.Ltmp97:
	.loc	2 609 21
	orr	w8, w22, w8, lsl #8
	str	w8, [x20]
	.loc	2 610 9
	str	w21, [x26, :lo12:rkf_wf_get_lut.stype]
	.loc	2 613 7
	cbz	x0, .LBB2_19
.Ltmp98:
	.loc	2 614 4
	bl	kfree
.Ltmp99:
	.loc	2 615 17
	str	xzr, [x20, #8]
.Ltmp100:
.LBB2_19:
	.loc	2 188 34
	ldr	x8, [x23, :lo12:global_waveform]
.Ltmp101:
	.loc	2 173 9
	bic	w10, w19, w19, asr #31
.Ltmp102:
	.loc	2 174 9
	cmp	w10, #50
	mov	w11, #50
	mov	x9, xzr
.Ltmp103:
	csel	w10, w10, w11, lo
.Ltmp104:
	.loc	2 176 2
	add	x11, x8, #532
.Ltmp105:
.LBB2_20:
	.loc	2 177 14
	ldrb	w12, [x11, x9]
.Ltmp106:
	.loc	2 177 7 is_stmt 0
	cmp	w12, w10, uxtb
	b.hi	.LBB2_23
.Ltmp107:
	.loc	2 176 23 is_stmt 1
	add	x9, x9, #1
.Ltmp108:
	.loc	2 176 2 is_stmt 0
	cmp	x9, #50
	b.ne	.LBB2_20
.Ltmp109:
	.loc	2 0 2
	mov	x9, xzr
.Ltmp110:
.LBB2_23:
	.loc	2 229 14 is_stmt 1
	add	x9, x8, x9, lsl #2
.Ltmp111:
	.loc	4 558 9
	mov	w1, #3520
.Ltmp112:
	.loc	2 229 14
	ldr	w9, [x9, #2132]
.Ltmp113:
	.loc	2 230 19
	add	x26, x8, x9
.Ltmp114:
	.loc	2 237 9
	ldrb	w21, [x26]
.Ltmp115:
	.loc	2 439 26
	lsl	x22, x21, #6
.Ltmp116:
	.loc	4 558 9
	mov	x0, x22
	bl	__kmalloc
.Ltmp117:
	.loc	2 246 6
	cbz	x0, .LBB2_34
.Ltmp118:
	.loc	2 249 36
	add	x1, x26, #4
	.loc	2 249 2 is_stmt 0
	mov	x2, x22
	.loc	2 0 0
	mov	x23, x0
.Ltmp119:
	.loc	2 249 2
	bl	memcpy
.Ltmp120:
	.loc	2 623 33 is_stmt 1
	ldr	x1, [x20, #32]
	.loc	2 623 3 is_stmt 0
	mov	x0, x20
	mov	w2, w21
.Ltmp121:
	.loc	2 440 15 is_stmt 1
	str	x23, [x20, #8]
.Ltmp122:
	.loc	2 623 3
	bl	rkf_lut_init_wf_table
.Ltmp123:
	.loc	2 627 27
	ldrsw	x8, [x24, :lo12:rkf_wf_get_lut.sftemp]
	mov	w9, #26215
	movk	w9, #26214, lsl #16
.Ltmp124:
	.loc	2 624 28
	str	w21, [x20, #4]
.Ltmp125:
	.loc	2 627 34
	mul	x8, x8, x9
	lsr	x9, x8, #63
	asr	x8, x8, #33
	add	w8, w8, w9
.Ltmp126:
.LBB2_25:
	.loc	2 627 6 is_stmt 0
	cmp	w25, w8
	b.ne	.LBB2_29
.Ltmp127:
.LBB2_26:
	.loc	2 0 6
	mov	w0, wzr
	b	.LBB2_36
.Ltmp128:
.LBB2_27:
	mov	w0, #-19
.Ltmp129:
	b	.LBB2_36
.Ltmp130:
.LBB2_28:
	mov	w0, #-22
	b	.LBB2_36
.Ltmp131:
.LBB2_29:
	.loc	2 629 15 is_stmt 1
	ldr	x0, [x20, #8]
.Ltmp132:
	.loc	2 629 7 is_stmt 0
	cbz	x0, .LBB2_31
.Ltmp133:
	.loc	2 630 4 is_stmt 1
	bl	kfree
.Ltmp134:
	.loc	2 631 17
	str	xzr, [x20, #8]
.Ltmp135:
.LBB2_31:
	.loc	2 633 17
	bl	epd_overlay_lut
.Ltmp136:
	mov	w1, w0
.Ltmp137:
	.loc	2 634 15
	mov	x0, x20
	mov	w2, w19
	bl	parse_wf_data
.Ltmp138:
	.loc	2 635 7
	tbnz	w0, #31, .LBB2_35
.Ltmp139:
	.loc	2 0 0 is_stmt 0
	mov	w21, w0
.Ltmp140:
	.loc	2 639 33 is_stmt 1
	ldr	x1, [x20, #24]
	.loc	2 639 3 is_stmt 0
	mov	x0, x20
	mov	w2, w21
	bl	rkf_lut_init_wf_table
.Ltmp141:
	.loc	2 641 21 is_stmt 1
	ldrb	w8, [x20]
	mov	w0, wzr
	.loc	2 642 21
	bfi	w8, w21, #8, #24
	str	w8, [x20]
	.loc	2 643 10
	str	w19, [x24, :lo12:rkf_wf_get_lut.sftemp]
	b	.LBB2_36
.Ltmp142:
.LBB2_33:
	.loc	2 577 3
	adrp	x0, .L.str.3
	add	x0, x0, :lo12:.L.str.3
	bl	_printk
.Ltmp143:
	.loc	2 0 3 is_stmt 0
	mov	w0, #-22
	b	.LBB2_36
.Ltmp144:
.LBB2_34:
	.loc	2 440 15 is_stmt 1
	str	xzr, [x20, #8]
.Ltmp145:
.LBB2_35:
	.loc	2 0 0 is_stmt 0
	adrp	x0, .L.str.4
	add	x0, x0, :lo12:.L.str.4
	bl	_printk
.Ltmp146:
	mov	w0, #-5
.Ltmp147:
.LBB2_36:
	.loc	2 647 1 is_stmt 1
	ldp	x20, x19, [sp, #64]
	ldp	x22, x21, [sp, #48]
	ldp	x24, x23, [sp, #32]
	ldp	x26, x25, [sp, #16]
	ldp	x29, x30, [sp], #80
	ldr	x30, [x18, #-8]!
	ret
.Ltmp148:
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
.Ltmp149:
	.loc	2 470 2 prologue_end
	cmp	w1, #7
	b.hi	.LBB3_14
.Ltmp150:
	.loc	2 0 2 is_stmt 0
	adrp	x9, .LJTI3_0
	mov	w8, w1
	add	x9, x9, :lo12:.LJTI3_0
	mov	x19, x0
.Ltmp151:
	adr	x10, .LBB3_2
	ldrb	w11, [x9, x8]
	add	x10, x10, x11, lsl #2
	br	x10
.Ltmp152:
.LBB3_2:
	.loc	2 188 34 is_stmt 1
	adrp	x8, global_waveform
.Ltmp153:
	.loc	2 173 9
	bic	w10, w2, w2, asr #31
.Ltmp154:
	.loc	2 174 9
	cmp	w10, #50
	mov	w11, #50
	mov	x9, xzr
.Ltmp155:
	csel	w10, w10, w11, lo
.Ltmp156:
	.loc	2 188 34
	ldr	x8, [x8, :lo12:global_waveform]
.Ltmp157:
	.loc	2 176 2
	add	x11, x8, #148
.Ltmp158:
.LBB3_3:
	.loc	2 177 14
	ldrb	w12, [x11, x9]
.Ltmp159:
	.loc	2 177 7 is_stmt 0
	cmp	w12, w10, uxtb
	b.hi	.LBB3_6
.Ltmp160:
	.loc	2 176 23 is_stmt 1
	add	x9, x9, #1
.Ltmp161:
	.loc	2 176 2 is_stmt 0
	cmp	x9, #50
	b.ne	.LBB3_3
.Ltmp162:
	.loc	2 0 2
	mov	x9, xzr
.Ltmp163:
.LBB3_6:
	.loc	2 229 14 is_stmt 1
	add	x9, x8, x9, lsl #2
.Ltmp164:
	.loc	4 558 9
	mov	w1, #3520
.Ltmp165:
	.loc	2 229 14
	ldr	w9, [x9, #596]
.Ltmp166:
	.loc	2 230 19
	add	x24, x8, x9
.Ltmp167:
	.loc	2 237 9
	ldrb	w20, [x24]
.Ltmp168:
	.loc	2 333 11
	add	x23, x20, #15
	.loc	2 333 39 is_stmt 0
	lsr	x8, x23, #2
.Ltmp169:
	and	x22, x8, #0x7c
.Ltmp170:
	.loc	4 558 9 is_stmt 1
	mov	x0, x22
	bl	__kmalloc
.Ltmp171:
	.loc	2 246 6
	cbz	x0, .LBB3_14
.Ltmp172:
	.loc	2 249 36
	add	x1, x24, #4
	.loc	2 249 2 is_stmt 0
	mov	x2, x22
	.loc	2 0 0
	mov	x21, x0
.Ltmp173:
	.loc	2 249 2
	bl	memcpy
.Ltmp174:
	.loc	2 265 27 is_stmt 1
	lsl	w8, w23, #6
.Ltmp175:
	.loc	4 558 9
	mov	w1, #3520
.Ltmp176:
	.loc	2 265 27
	and	w0, w8, #0x7c00
.Ltmp177:
	.loc	4 558 9
	bl	__kmalloc
.Ltmp178:
	.loc	2 266 6
	cbz	x0, .LBB3_52
.Ltmp179:
	.loc	2 269 2
	cbz	w20, .LBB3_13
.Ltmp180:
	.loc	2 0 0 is_stmt 0
	lsr	w11, w23, #4
.Ltmp181:
	mov	w10, #16384
.Ltmp182:
	.loc	2 269 2
	cmp	w11, #1
	mov	w12, #1365
	mov	w8, wzr
	mov	w9, wzr
	movk	w10, #21, lsl #16
	csinc	w11, w11, wzr, gt
.Ltmp183:
	.loc	2 0 2
	movk	w12, #5440, lsl #16
	mov	x13, x21
.Ltmp184:
.LBB3_10:
	mov	w15, wzr
	mov	w14, wzr
.Ltmp185:
	.loc	2 270 15 is_stmt 1
	ldr	w16, [x13]
.Ltmp186:
.LBB3_11:
	.loc	2 272 26
	asr	w17, w16, w15
	.loc	2 272 31 is_stmt 0
	add	w3, w8, w14
	.loc	2 272 36
	and	w1, w17, #0x3
.Ltmp187:
	.loc	2 276 26 is_stmt 1
	lsl	w17, w17, #30
.Ltmp188:
	.loc	2 279 13
	add	w4, w3, #3
.Ltmp189:
	.loc	2 271 17
	add	w14, w14, #16
.Ltmp190:
	.loc	2 0 17 is_stmt 0
	mul	w2, w1, w10
	.loc	2 271 17
	add	w15, w15, #2
.Ltmp191:
	.loc	2 276 19 is_stmt 1
	bfi	w17, w1, #12, #2
.Ltmp192:
	.loc	2 271 3
	cmp	w14, #256
.Ltmp193:
	.loc	2 276 19
	orr	w17, w17, w2
.Ltmp194:
	.loc	2 279 13
	add	w2, w3, #1
.Ltmp195:
	.loc	2 276 19
	madd	w17, w1, w12, w17
.Ltmp196:
	.loc	2 279 13
	add	w1, w3, #2
	.loc	2 279 20 is_stmt 0
	str	w17, [x0, w2, sxtw #2]
.Ltmp197:
	.loc	2 279 13
	add	w2, w3, #4
	.loc	2 279 20
	str	w17, [x0, w1, sxtw #2]
.Ltmp198:
	.loc	2 279 13
	add	w1, w3, #5
	.loc	2 279 20
	str	w17, [x0, w4, sxtw #2]
.Ltmp199:
	.loc	2 279 13
	add	w4, w3, #6
	.loc	2 279 20
	str	w17, [x0, w2, sxtw #2]
.Ltmp200:
	.loc	2 279 13
	add	w2, w3, #7
	.loc	2 279 20
	str	w17, [x0, w1, sxtw #2]
.Ltmp201:
	.loc	2 279 13
	add	w1, w3, #8
	.loc	2 279 20
	str	w17, [x0, w4, sxtw #2]
.Ltmp202:
	.loc	2 279 13
	add	w4, w3, #9
	.loc	2 279 20
	str	w17, [x0, w2, sxtw #2]
.Ltmp203:
	.loc	2 279 13
	add	w2, w3, #10
	.loc	2 279 20
	str	w17, [x0, w1, sxtw #2]
.Ltmp204:
	.loc	2 279 13
	add	w1, w3, #11
	.loc	2 279 20
	str	w17, [x0, w4, sxtw #2]
.Ltmp205:
	str	w17, [x0, w2, sxtw #2]
.Ltmp206:
	.loc	2 279 13
	add	w2, w3, #12
	.loc	2 279 20
	str	w17, [x0, w1, sxtw #2]
.Ltmp207:
	.loc	2 279 13
	add	w1, w3, #13
	.loc	2 279 20
	str	w17, [x0, w3, sxtw #2]
.Ltmp208:
	str	w17, [x0, w2, sxtw #2]
.Ltmp209:
	.loc	2 279 13
	add	w2, w3, #14
	.loc	2 279 20
	str	w17, [x0, w1, sxtw #2]
.Ltmp210:
	.loc	2 279 13
	add	w1, w3, #15
	.loc	2 279 20
	str	w17, [x0, w2, sxtw #2]
.Ltmp211:
	str	w17, [x0, w1, sxtw #2]
.Ltmp212:
	.loc	2 271 3 is_stmt 1
	b.ne	.LBB3_11
.Ltmp213:
	.loc	2 270 21
	add	x13, x13, #4
.Ltmp214:
	.loc	2 269 24
	add	w9, w9, #1
.Ltmp215:
	.loc	2 269 2 is_stmt 0
	add	w8, w8, w14
	cmp	w9, w11
	b.ne	.LBB3_10
.Ltmp216:
.LBB3_13:
	.loc	2 341 15 is_stmt 1
	str	x0, [x19, #8]
	.loc	2 0 0 is_stmt 0
	mov	x0, x21
	bl	kfree
.Ltmp217:
	b	.LBB3_51
.Ltmp218:
.LBB3_14:
	mov	w20, #-1
	b	.LBB3_51
.Ltmp219:
.LBB3_15:
	.loc	2 188 34 is_stmt 1
	adrp	x8, global_waveform
.Ltmp220:
	.loc	2 173 9
	bic	w10, w2, w2, asr #31
.Ltmp221:
	.loc	2 174 9
	cmp	w10, #50
	mov	w11, #50
	mov	x9, xzr
.Ltmp222:
	csel	w10, w10, w11, lo
.Ltmp223:
	.loc	2 188 34
	ldr	x8, [x8, :lo12:global_waveform]
.Ltmp224:
	.loc	2 176 2
	add	x11, x8, #468
.Ltmp225:
.LBB3_16:
	.loc	2 177 14
	ldrb	w12, [x11, x9]
.Ltmp226:
	.loc	2 177 7 is_stmt 0
	cmp	w12, w10, uxtb
	b.hi	.LBB3_19
.Ltmp227:
	.loc	2 176 23 is_stmt 1
	add	x9, x9, #1
.Ltmp228:
	.loc	2 176 2 is_stmt 0
	cmp	x9, #50
	b.ne	.LBB3_16
.Ltmp229:
	.loc	2 0 2
	mov	x9, xzr
.Ltmp230:
.LBB3_19:
	.loc	2 229 14 is_stmt 1
	add	x9, x8, x9, lsl #2
	ldr	w9, [x9, #1876]
.Ltmp231:
	.loc	2 0 14 is_stmt 0
	b	.LBB3_48
.Ltmp232:
.LBB3_20:
	.loc	2 188 34 is_stmt 1
	adrp	x8, global_waveform
.Ltmp233:
	.loc	2 173 9
	bic	w10, w2, w2, asr #31
.Ltmp234:
	.loc	2 174 9
	cmp	w10, #50
	mov	w11, #50
	mov	x9, xzr
.Ltmp235:
	csel	w10, w10, w11, lo
.Ltmp236:
	.loc	2 188 34
	ldr	x8, [x8, :lo12:global_waveform]
.Ltmp237:
	.loc	2 176 2
	add	x11, x8, #212
.Ltmp238:
.LBB3_21:
	.loc	2 177 14
	ldrb	w12, [x11, x9]
.Ltmp239:
	.loc	2 177 7 is_stmt 0
	cmp	w12, w10, uxtb
	b.hi	.LBB3_27
.Ltmp240:
	.loc	2 176 23 is_stmt 1
	add	x9, x9, #1
.Ltmp241:
	.loc	2 176 2 is_stmt 0
	cmp	x9, #50
	b.ne	.LBB3_21
	b	.LBB3_26
.Ltmp242:
.LBB3_23:
	.loc	2 188 34 is_stmt 1
	adrp	x8, global_waveform
.Ltmp243:
	.loc	2 173 9
	bic	w10, w2, w2, asr #31
.Ltmp244:
	.loc	2 174 9
	cmp	w10, #50
	mov	w11, #50
	mov	x9, xzr
.Ltmp245:
	csel	w10, w10, w11, lo
.Ltmp246:
	.loc	2 188 34
	ldr	x8, [x8, :lo12:global_waveform]
.Ltmp247:
	.loc	2 176 2
	add	x11, x8, #212
.Ltmp248:
.LBB3_24:
	.loc	2 177 14
	ldrb	w12, [x11, x9]
.Ltmp249:
	.loc	2 177 7 is_stmt 0
	cmp	w12, w10, uxtb
	b.hi	.LBB3_27
.Ltmp250:
	.loc	2 176 23 is_stmt 1
	add	x9, x9, #1
.Ltmp251:
	.loc	2 176 2 is_stmt 0
	cmp	x9, #50
	b.ne	.LBB3_24
.Ltmp252:
.LBB3_26:
	.loc	2 0 2
	mov	x9, xzr
.Ltmp253:
.LBB3_27:
	add	x9, x8, x9, lsl #2
	ldr	w9, [x9, #852]
	b	.LBB3_48
.Ltmp254:
.LBB3_28:
	.loc	2 188 34 is_stmt 1
	adrp	x8, global_waveform
.Ltmp255:
	.loc	2 173 9
	bic	w10, w2, w2, asr #31
.Ltmp256:
	.loc	2 174 9
	cmp	w10, #50
	mov	w11, #50
	mov	x9, xzr
.Ltmp257:
	csel	w10, w10, w11, lo
.Ltmp258:
	.loc	2 188 34
	ldr	x8, [x8, :lo12:global_waveform]
.Ltmp259:
	.loc	2 176 2
	add	x11, x8, #276
.Ltmp260:
.LBB3_29:
	.loc	2 177 14
	ldrb	w12, [x11, x9]
.Ltmp261:
	.loc	2 177 7 is_stmt 0
	cmp	w12, w10, uxtb
	b.hi	.LBB3_32
.Ltmp262:
	.loc	2 176 23 is_stmt 1
	add	x9, x9, #1
.Ltmp263:
	.loc	2 176 2 is_stmt 0
	cmp	x9, #50
	b.ne	.LBB3_29
.Ltmp264:
	.loc	2 0 2
	mov	x9, xzr
.Ltmp265:
.LBB3_32:
	.loc	2 229 14 is_stmt 1
	add	x9, x8, x9, lsl #2
	ldr	w9, [x9, #1108]
.Ltmp266:
	.loc	2 0 14 is_stmt 0
	b	.LBB3_48
.Ltmp267:
.LBB3_33:
	.loc	2 188 34 is_stmt 1
	adrp	x8, global_waveform
.Ltmp268:
	.loc	2 173 9
	bic	w10, w2, w2, asr #31
.Ltmp269:
	.loc	2 174 9
	cmp	w10, #50
	mov	w11, #50
	mov	x9, xzr
.Ltmp270:
	csel	w10, w10, w11, lo
.Ltmp271:
	.loc	2 188 34
	ldr	x8, [x8, :lo12:global_waveform]
.Ltmp272:
	.loc	2 176 2
	add	x11, x8, #340
.Ltmp273:
.LBB3_34:
	.loc	2 177 14
	ldrb	w12, [x11, x9]
.Ltmp274:
	.loc	2 177 7 is_stmt 0
	cmp	w12, w10, uxtb
	b.hi	.LBB3_37
.Ltmp275:
	.loc	2 176 23 is_stmt 1
	add	x9, x9, #1
.Ltmp276:
	.loc	2 176 2 is_stmt 0
	cmp	x9, #50
	b.ne	.LBB3_34
.Ltmp277:
	.loc	2 0 2
	mov	x9, xzr
.Ltmp278:
.LBB3_37:
	.loc	2 229 14 is_stmt 1
	add	x9, x8, x9, lsl #2
	ldr	w9, [x9, #1364]
.Ltmp279:
	.loc	2 0 14 is_stmt 0
	b	.LBB3_48
.Ltmp280:
.LBB3_38:
	.loc	2 188 34 is_stmt 1
	adrp	x8, global_waveform
.Ltmp281:
	.loc	2 173 9
	bic	w10, w2, w2, asr #31
.Ltmp282:
	.loc	2 174 9
	cmp	w10, #50
	mov	w11, #50
	mov	x9, xzr
.Ltmp283:
	csel	w10, w10, w11, lo
.Ltmp284:
	.loc	2 188 34
	ldr	x8, [x8, :lo12:global_waveform]
.Ltmp285:
	.loc	2 176 2
	add	x11, x8, #404
.Ltmp286:
.LBB3_39:
	.loc	2 177 14
	ldrb	w12, [x11, x9]
.Ltmp287:
	.loc	2 177 7 is_stmt 0
	cmp	w12, w10, uxtb
	b.hi	.LBB3_42
.Ltmp288:
	.loc	2 176 23 is_stmt 1
	add	x9, x9, #1
.Ltmp289:
	.loc	2 176 2 is_stmt 0
	cmp	x9, #50
	b.ne	.LBB3_39
.Ltmp290:
	.loc	2 0 2
	mov	x9, xzr
.Ltmp291:
.LBB3_42:
	.loc	2 229 14 is_stmt 1
	add	x9, x8, x9, lsl #2
	ldr	w9, [x9, #1620]
.Ltmp292:
	.loc	2 0 14 is_stmt 0
	b	.LBB3_48
.Ltmp293:
.LBB3_43:
	.loc	2 188 34 is_stmt 1
	adrp	x8, global_waveform
.Ltmp294:
	.loc	2 173 9
	bic	w10, w2, w2, asr #31
.Ltmp295:
	.loc	2 174 9
	cmp	w10, #50
	mov	w11, #50
	mov	x9, xzr
.Ltmp296:
	csel	w10, w10, w11, lo
.Ltmp297:
	.loc	2 188 34
	ldr	x8, [x8, :lo12:global_waveform]
.Ltmp298:
	.loc	2 176 2
	add	x11, x8, #532
.Ltmp299:
.LBB3_44:
	.loc	2 177 14
	ldrb	w12, [x11, x9]
.Ltmp300:
	.loc	2 177 7 is_stmt 0
	cmp	w12, w10, uxtb
	b.hi	.LBB3_47
.Ltmp301:
	.loc	2 176 23 is_stmt 1
	add	x9, x9, #1
.Ltmp302:
	.loc	2 176 2 is_stmt 0
	cmp	x9, #50
	b.ne	.LBB3_44
.Ltmp303:
	.loc	2 0 2
	mov	x9, xzr
.Ltmp304:
.LBB3_47:
	.loc	2 229 14 is_stmt 1
	add	x9, x8, x9, lsl #2
	ldr	w9, [x9, #2132]
.Ltmp305:
.LBB3_48:
	.loc	2 0 0 is_stmt 0
	add	x23, x8, x9
.Ltmp306:
	mov	w1, #3520
.Ltmp307:
	ldrb	w22, [x23]
	lsl	x20, x22, #6
.Ltmp308:
	mov	x0, x20
	bl	__kmalloc
.Ltmp309:
	cbz	x0, .LBB3_50
.Ltmp310:
	add	x1, x23, #4
	mov	x2, x20
	mov	x21, x0
.Ltmp311:
	bl	memcpy
.Ltmp312:
	mov	w20, w22
.Ltmp313:
	str	x21, [x19, #8]
.Ltmp314:
	b	.LBB3_51
.Ltmp315:
.LBB3_50:
	mov	w20, #-1
.Ltmp316:
	str	xzr, [x19, #8]
.Ltmp317:
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
.Ltmp318:
	.loc	2 0 0 is_stmt 0
	mov	x0, x21
	.loc	2 341 15 is_stmt 1
	str	xzr, [x19, #8]
	.loc	2 0 0 is_stmt 0
	bl	kfree
.Ltmp319:
	mov	w20, #-1
.Ltmp320:
	b	.LBB3_51
.Ltmp321:
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
.Ltmp322:
	.loc	2 0 2 is_stmt 0
	mov	w8, wzr
	mov	w9, wzr
	mov	x10, xzr
.Ltmp323:
	.loc	2 452 14
	mov	w11, w2
.Ltmp324:
	.loc	2 452 2
	add	x12, x1, #7
.Ltmp325:
.LBB4_2:
	.loc	2 453 34 is_stmt 1
	ldr	x15, [x0, #8]
.Ltmp326:
	and	x14, x8, #0x7ffffff
	mov	x13, xzr
	add	x14, x12, x14, lsl #5
.Ltmp327:
	.loc	2 454 3
	add	x15, x15, w9, sxtw #2
.Ltmp328:
.LBB4_3:
	.loc	2 0 3 is_stmt 0
	add	x16, x15, x13
.Ltmp329:
	.loc	2 454 31
	add	x13, x13, #4
.Ltmp330:
	.loc	2 454 3
	cmp	x13, #64
.Ltmp331:
	.loc	2 456 16 is_stmt 1
	ldrb	w17, [x16]
	.loc	2 456 15 is_stmt 0
	and	w17, w17, #0x3
.Ltmp332:
	.loc	2 457 54 is_stmt 1
	sturb	w17, [x14, #-7]
.Ltmp333:
	.loc	2 456 16
	ldrb	w17, [x16]
	.loc	2 456 15 is_stmt 0
	ubfx	w17, w17, #2, #2
.Ltmp334:
	.loc	2 457 54 is_stmt 1
	sturb	w17, [x14, #-6]
.Ltmp335:
	.loc	2 456 16
	ldrb	w17, [x16]
	.loc	2 456 15 is_stmt 0
	ubfx	w17, w17, #4, #2
.Ltmp336:
	.loc	2 457 54 is_stmt 1
	sturb	w17, [x14, #-5]
.Ltmp337:
	.loc	2 456 16
	ldrb	w17, [x16]
	.loc	2 456 15 is_stmt 0
	lsr	w17, w17, #6
.Ltmp338:
	.loc	2 457 54 is_stmt 1
	sturb	w17, [x14, #-4]
.Ltmp339:
	.loc	2 456 16
	ldrb	w17, [x16, #1]
	.loc	2 456 15 is_stmt 0
	and	w17, w17, #0x3
.Ltmp340:
	.loc	2 457 54 is_stmt 1
	sturb	w17, [x14, #-3]
.Ltmp341:
	.loc	2 456 16
	ldr	w17, [x16]
	.loc	2 456 15 is_stmt 0
	ubfx	w17, w17, #10, #2
.Ltmp342:
	.loc	2 457 54 is_stmt 1
	sturb	w17, [x14, #-2]
.Ltmp343:
	.loc	2 456 16
	ldr	w17, [x16]
	.loc	2 456 15 is_stmt 0
	ubfx	w17, w17, #12, #2
.Ltmp344:
	.loc	2 457 54 is_stmt 1
	sturb	w17, [x14, #-1]
.Ltmp345:
	.loc	2 456 16
	ldr	w17, [x16]
	.loc	2 456 15 is_stmt 0
	ubfx	w17, w17, #14, #2
.Ltmp346:
	.loc	2 457 54 is_stmt 1
	strb	w17, [x14]
.Ltmp347:
	.loc	2 456 16
	ldrb	w17, [x16, #2]
	.loc	2 456 15 is_stmt 0
	and	w17, w17, #0x3
.Ltmp348:
	.loc	2 457 54 is_stmt 1
	strb	w17, [x14, #1]
.Ltmp349:
	.loc	2 456 16
	ldr	w17, [x16]
	.loc	2 456 15 is_stmt 0
	ubfx	w17, w17, #18, #2
.Ltmp350:
	.loc	2 457 54 is_stmt 1
	strb	w17, [x14, #2]
.Ltmp351:
	.loc	2 456 16
	ldr	w17, [x16]
	.loc	2 456 15 is_stmt 0
	ubfx	w17, w17, #20, #2
.Ltmp352:
	.loc	2 457 54 is_stmt 1
	strb	w17, [x14, #3]
.Ltmp353:
	.loc	2 456 16
	ldr	w17, [x16]
	.loc	2 456 15 is_stmt 0
	ubfx	w17, w17, #22, #2
.Ltmp354:
	.loc	2 457 54 is_stmt 1
	strb	w17, [x14, #4]
.Ltmp355:
	.loc	2 456 16
	ldrb	w17, [x16, #3]
	.loc	2 456 15 is_stmt 0
	and	w17, w17, #0x3
.Ltmp356:
	.loc	2 457 54 is_stmt 1
	strb	w17, [x14, #5]
.Ltmp357:
	.loc	2 456 16
	ldr	w17, [x16]
	.loc	2 456 15 is_stmt 0
	ubfx	w17, w17, #26, #2
.Ltmp358:
	.loc	2 457 54 is_stmt 1
	strb	w17, [x14, #6]
.Ltmp359:
	.loc	2 456 16
	ldr	w17, [x16]
	.loc	2 456 15 is_stmt 0
	ubfx	w17, w17, #28, #2
.Ltmp360:
	.loc	2 457 54 is_stmt 1
	strb	w17, [x14, #7]
.Ltmp361:
	.loc	2 456 16
	ldr	w16, [x16]
	.loc	2 456 39 is_stmt 0
	lsr	w16, w16, #30
.Ltmp362:
	.loc	2 457 54 is_stmt 1
	strb	w16, [x14, #8]
.Ltmp363:
	.loc	2 454 31
	add	x14, x14, #32
.Ltmp364:
	.loc	2 454 3 is_stmt 0
	b.ne	.LBB4_3
.Ltmp365:
	.loc	2 452 28 is_stmt 1
	add	x10, x10, #1
.Ltmp366:
	.loc	2 452 2 is_stmt 0
	add	w9, w9, #16
	add	w8, w8, #32
	cmp	x10, x11
	b.ne	.LBB4_2
.Ltmp367:
.LBB4_5:
	.loc	2 462 2 is_stmt 1
	ret
.Ltmp368:
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
	.xword	.Ltmp128-.Lfunc_begin0
	.hword	1
	.byte	100
	.xword	.Ltmp128-.Lfunc_begin0
	.xword	.Ltmp129-.Lfunc_begin0
	.hword	1
	.byte	80
	.xword	.Ltmp130-.Lfunc_begin0
	.xword	.Ltmp147-.Lfunc_begin0
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
	.xword	.Ltmp115-.Lfunc_begin0
	.hword	1
	.byte	101
	.xword	.Ltmp128-.Lfunc_begin0
	.xword	.Ltmp131-.Lfunc_begin0
	.hword	1
	.byte	81
	.xword	.Ltmp142-.Lfunc_begin0
	.xword	.Ltmp143-.Lfunc_begin0
	.hword	1
	.byte	81
	.xword	.Ltmp143-.Lfunc_begin0
	.xword	.Ltmp144-.Lfunc_begin0
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
	.xword	.Ltmp128-.Lfunc_begin0
	.hword	1
	.byte	99
	.xword	.Ltmp128-.Lfunc_begin0
	.xword	.Ltmp131-.Lfunc_begin0
	.hword	1
	.byte	82
	.xword	.Ltmp131-.Lfunc_begin0
	.xword	.Ltmp142-.Lfunc_begin0
	.hword	1
	.byte	99
	.xword	.Ltmp142-.Lfunc_begin0
	.xword	.Ltmp143-.Lfunc_begin0
	.hword	1
	.byte	82
	.xword	.Ltmp144-.Lfunc_begin0
	.xword	.Ltmp147-.Lfunc_begin0
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
	.xword	.Ltmp128-.Lfunc_begin0
	.xword	.Ltmp131-.Lfunc_begin0
	.hword	1
	.byte	83
	.xword	.Ltmp142-.Lfunc_begin0
	.xword	.Ltmp143-.Lfunc_begin0
	.hword	1
	.byte	83
	.xword	.Ltmp143-.Lfunc_begin0
	.xword	.Ltmp144-.Lfunc_begin0
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
	.xword	.Ltmp116-.Lfunc_begin0
	.hword	1
	.byte	102
	.xword	.Ltmp122-.Lfunc_begin0
	.xword	.Ltmp126-.Lfunc_begin0
	.hword	1
	.byte	101
	.xword	.Ltmp128-.Lfunc_begin0
	.xword	.Ltmp131-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	0
	.byte	159
	.xword	.Ltmp138-.Lfunc_begin0
	.xword	.Ltmp140-.Lfunc_begin0
	.hword	1
	.byte	80
	.xword	.Ltmp140-.Lfunc_begin0
	.xword	.Ltmp142-.Lfunc_begin0
	.hword	1
	.byte	101
	.xword	.Ltmp142-.Lfunc_begin0
	.xword	.Ltmp144-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	0
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc19:
	.xword	.Ltmp57-.Lfunc_begin0
	.xword	.Ltmp137-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	0
	.byte	159
	.xword	.Ltmp137-.Lfunc_begin0
	.xword	.Ltmp138-.Lfunc_begin0
	.hword	1
	.byte	81
	.xword	.Ltmp142-.Lfunc_begin0
	.xword	.Ltmp145-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	0
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc20:
	.xword	.Ltmp82-.Lfunc_begin0
	.xword	.Ltmp95-.Lfunc_begin0
	.hword	1
	.byte	88
	.xword	0
	.xword	0
.Ldebug_loc21:
	.xword	.Ltmp79-.Lfunc_begin0
	.xword	.Ltmp83-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	0
	.byte	159
	.xword	.Ltmp83-.Lfunc_begin0
	.xword	.Ltmp85-.Lfunc_begin0
	.hword	7
	.byte	121
	.byte	0
	.byte	17
	.byte	128
	.byte	8
	.byte	27
	.byte	159
	.xword	.Ltmp87-.Lfunc_begin0
	.xword	.Ltmp88-.Lfunc_begin0
	.hword	10
	.byte	121
	.byte	0
	.byte	17
	.byte	128
	.byte	8
	.byte	27
	.byte	17
	.byte	1
	.byte	34
	.byte	159
	.xword	.Ltmp88-.Lfunc_begin0
	.xword	.Ltmp90-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	0
	.byte	159
	.xword	.Ltmp90-.Lfunc_begin0
	.xword	.Ltmp92-.Lfunc_begin0
	.hword	7
	.byte	121
	.byte	0
	.byte	17
	.byte	128
	.byte	8
	.byte	27
	.byte	159
	.xword	.Ltmp94-.Lfunc_begin0
	.xword	.Ltmp95-.Lfunc_begin0
	.hword	10
	.byte	121
	.byte	0
	.byte	17
	.byte	128
	.byte	8
	.byte	27
	.byte	17
	.byte	1
	.byte	34
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc22:
	.xword	.Ltmp79-.Lfunc_begin0
	.xword	.Ltmp83-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	.Ltmp84-.Lfunc_begin0
	.xword	.Ltmp95-.Lfunc_begin0
	.hword	1
	.byte	91
	.xword	0
	.xword	0
.Ldebug_loc23:
	.xword	.Ltmp101-.Lfunc_begin0
	.xword	.Ltmp105-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	0
	.byte	159
	.xword	.Ltmp105-.Lfunc_begin0
	.xword	.Ltmp109-.Lfunc_begin0
	.hword	1
	.byte	89
	.xword	0
	.xword	0
.Ldebug_loc24:
	.xword	.Ltmp101-.Lfunc_begin0
	.xword	.Ltmp102-.Lfunc_begin0
	.hword	1
	.byte	99
	.xword	.Ltmp102-.Lfunc_begin0
	.xword	.Ltmp109-.Lfunc_begin0
	.hword	1
	.byte	90
	.xword	0
	.xword	0
.Ldebug_loc25:
	.xword	.Ltmp101-.Lfunc_begin0
	.xword	.Ltmp114-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	.Ltmp114-.Lfunc_begin0
	.xword	.Ltmp118-.Lfunc_begin0
	.hword	1
	.byte	106
	.xword	0
	.xword	0
.Ldebug_loc26:
	.xword	.Ltmp101-.Lfunc_begin0
	.xword	.Ltmp110-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc27:
	.xword	.Ltmp101-.Lfunc_begin0
	.xword	.Ltmp113-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc28:
	.xword	.Ltmp101-.Lfunc_begin0
	.xword	.Ltmp117-.Lfunc_begin0
	.hword	4
	.byte	120
	.byte	212
	.byte	16
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc29:
	.xword	.Ltmp101-.Lfunc_begin0
	.xword	.Ltmp117-.Lfunc_begin0
	.hword	4
	.byte	120
	.byte	148
	.byte	4
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc30:
	.xword	.Ltmp101-.Lfunc_begin0
	.xword	.Ltmp103-.Lfunc_begin0
	.hword	1
	.byte	88
	.xword	.Ltmp103-.Lfunc_begin0
	.xword	.Ltmp118-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc31:
	.xword	.Ltmp103-.Lfunc_begin0
	.xword	.Ltmp118-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	6
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc32:
	.xword	.Ltmp103-.Lfunc_begin0
	.xword	.Ltmp118-.Lfunc_begin0
	.hword	1
	.byte	99
	.xword	0
	.xword	0
.Ldebug_loc33:
	.xword	.Ltmp103-.Lfunc_begin0
	.xword	.Ltmp116-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	.Ltmp116-.Lfunc_begin0
	.xword	.Ltmp126-.Lfunc_begin0
	.hword	1
	.byte	102
	.xword	.Ltmp144-.Lfunc_begin0
	.xword	.Ltmp145-.Lfunc_begin0
	.hword	1
	.byte	102
	.xword	0
	.xword	0
.Ldebug_loc34:
	.xword	.Ltmp103-.Lfunc_begin0
	.xword	.Ltmp115-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	.Ltmp115-.Lfunc_begin0
	.xword	.Ltmp126-.Lfunc_begin0
	.hword	1
	.byte	101
	.xword	.Ltmp144-.Lfunc_begin0
	.xword	.Ltmp145-.Lfunc_begin0
	.hword	1
	.byte	101
	.xword	0
	.xword	0
.Ldebug_loc35:
	.xword	.Ltmp103-.Lfunc_begin0
	.xword	.Ltmp114-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	.Ltmp114-.Lfunc_begin0
	.xword	.Ltmp126-.Lfunc_begin0
	.hword	1
	.byte	106
	.xword	.Ltmp144-.Lfunc_begin0
	.xword	.Ltmp145-.Lfunc_begin0
	.hword	1
	.byte	106
	.xword	0
	.xword	0
.Ldebug_loc36:
	.xword	.Ltmp103-.Lfunc_begin0
	.xword	.Ltmp126-.Lfunc_begin0
	.hword	1
	.byte	99
	.xword	.Ltmp144-.Lfunc_begin0
	.xword	.Ltmp145-.Lfunc_begin0
	.hword	1
	.byte	99
	.xword	0
	.xword	0
.Ldebug_loc37:
	.xword	.Ltmp103-.Lfunc_begin0
	.xword	.Ltmp126-.Lfunc_begin0
	.hword	1
	.byte	100
	.xword	.Ltmp144-.Lfunc_begin0
	.xword	.Ltmp145-.Lfunc_begin0
	.hword	1
	.byte	100
	.xword	0
	.xword	0
.Ldebug_loc38:
	.xword	.Ltmp103-.Lfunc_begin0
	.xword	.Ltmp122-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	127
	.byte	159
	.xword	.Ltmp144-.Lfunc_begin0
	.xword	.Ltmp145-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	127
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc39:
	.xword	.Ltmp103-.Lfunc_begin0
	.xword	.Ltmp126-.Lfunc_begin0
	.hword	1
	.byte	99
	.xword	.Ltmp144-.Lfunc_begin0
	.xword	.Ltmp145-.Lfunc_begin0
	.hword	1
	.byte	99
	.xword	0
	.xword	0
.Ldebug_loc40:
	.xword	.Ltmp103-.Lfunc_begin0
	.xword	.Ltmp126-.Lfunc_begin0
	.hword	2
	.byte	55
	.byte	159
	.xword	.Ltmp144-.Lfunc_begin0
	.xword	.Ltmp145-.Lfunc_begin0
	.hword	2
	.byte	55
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc41:
	.xword	.Ltmp103-.Lfunc_begin0
	.xword	.Ltmp126-.Lfunc_begin0
	.hword	1
	.byte	100
	.xword	.Ltmp144-.Lfunc_begin0
	.xword	.Ltmp145-.Lfunc_begin0
	.hword	1
	.byte	100
	.xword	0
	.xword	0
.Ldebug_loc42:
	.xword	.Ltmp116-.Lfunc_begin0
	.xword	.Ltmp118-.Lfunc_begin0
	.hword	4
	.byte	16
	.byte	192
	.byte	27
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc43:
	.xword	.Ltmp116-.Lfunc_begin0
	.xword	.Ltmp118-.Lfunc_begin0
	.hword	4
	.byte	16
	.byte	192
	.byte	25
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc44:
	.xword	.Ltmp116-.Lfunc_begin0
	.xword	.Ltmp117-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	.Ltmp117-.Lfunc_begin0
	.xword	.Ltmp119-.Lfunc_begin0
	.hword	1
	.byte	80
	.xword	.Ltmp119-.Lfunc_begin0
	.xword	.Ltmp126-.Lfunc_begin0
	.hword	1
	.byte	103
	.xword	0
	.xword	0
.Ldebug_loc45:
	.xword	.Ltmp116-.Lfunc_begin0
	.xword	.Ltmp126-.Lfunc_begin0
	.hword	1
	.byte	106
	.xword	0
	.xword	0
.Ldebug_loc46:
	.xword	.Ltmp116-.Lfunc_begin0
	.xword	.Ltmp118-.Lfunc_begin0
	.hword	1
	.byte	102
	.xword	0
	.xword	0
.Ldebug_loc47:
	.xword	.Ltmp116-.Lfunc_begin0
	.xword	.Ltmp118-.Lfunc_begin0
	.hword	1
	.byte	102
	.xword	0
	.xword	0
.Ldebug_loc48:
	.xword	.Ltmp116-.Lfunc_begin0
	.xword	.Ltmp126-.Lfunc_begin0
	.hword	1
	.byte	102
	.xword	0
	.xword	0
.Ldebug_loc49:
	.xword	.Lfunc_begin3-.Lfunc_begin0
	.xword	.Ltmp151-.Lfunc_begin0
	.hword	1
	.byte	80
	.xword	.Ltmp151-.Lfunc_begin0
	.xword	.Ltmp218-.Lfunc_begin0
	.hword	1
	.byte	99
	.xword	.Ltmp219-.Lfunc_begin0
	.xword	.Ltmp317-.Lfunc_begin0
	.hword	1
	.byte	99
	.xword	.Ltmp318-.Lfunc_begin0
	.xword	.Lfunc_end3-.Lfunc_begin0
	.hword	1
	.byte	99
	.xword	0
	.xword	0
.Ldebug_loc50:
	.xword	.Lfunc_begin3-.Lfunc_begin0
	.xword	.Ltmp165-.Lfunc_begin0
	.hword	1
	.byte	81
	.xword	.Ltmp165-.Lfunc_begin0
	.xword	.Ltmp218-.Lfunc_begin0
	.hword	4
	.byte	243
	.byte	1
	.byte	81
	.byte	159
	.xword	.Ltmp219-.Lfunc_begin0
	.xword	.Ltmp307-.Lfunc_begin0
	.hword	1
	.byte	81
	.xword	.Ltmp307-.Lfunc_begin0
	.xword	.Ltmp317-.Lfunc_begin0
	.hword	4
	.byte	243
	.byte	1
	.byte	81
	.byte	159
	.xword	.Ltmp318-.Lfunc_begin0
	.xword	.Lfunc_end3-.Lfunc_begin0
	.hword	4
	.byte	243
	.byte	1
	.byte	81
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc51:
	.xword	.Lfunc_begin3-.Lfunc_begin0
	.xword	.Ltmp171-.Lfunc_begin0
	.hword	1
	.byte	82
	.xword	.Ltmp171-.Lfunc_begin0
	.xword	.Ltmp218-.Lfunc_begin0
	.hword	4
	.byte	243
	.byte	1
	.byte	82
	.byte	159
	.xword	.Ltmp219-.Lfunc_begin0
	.xword	.Ltmp309-.Lfunc_begin0
	.hword	1
	.byte	82
	.xword	.Ltmp309-.Lfunc_begin0
	.xword	.Ltmp317-.Lfunc_begin0
	.hword	4
	.byte	243
	.byte	1
	.byte	82
	.byte	159
	.xword	.Ltmp318-.Lfunc_begin0
	.xword	.Lfunc_end3-.Lfunc_begin0
	.hword	4
	.byte	243
	.byte	1
	.byte	82
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc52:
	.xword	.Ltmp149-.Lfunc_begin0
	.xword	.Ltmp217-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	127
	.byte	159
	.xword	.Ltmp217-.Lfunc_begin0
	.xword	.Ltmp218-.Lfunc_begin0
	.hword	1
	.byte	100
	.xword	.Ltmp218-.Lfunc_begin0
	.xword	.Ltmp314-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	127
	.byte	159
	.xword	.Ltmp315-.Lfunc_begin0
	.xword	.Ltmp316-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	127
	.byte	159
	.xword	.Ltmp318-.Lfunc_begin0
	.xword	.Ltmp320-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	127
	.byte	159
	.xword	.Ltmp320-.Lfunc_begin0
	.xword	.Lfunc_end3-.Lfunc_begin0
	.hword	1
	.byte	100
	.xword	0
	.xword	0
.Ldebug_loc53:
	.xword	.Ltmp154-.Lfunc_begin0
	.xword	.Ltmp156-.Lfunc_begin0
	.hword	1
	.byte	90
	.xword	.Ltmp157-.Lfunc_begin0
	.xword	.Ltmp162-.Lfunc_begin0
	.hword	1
	.byte	82
	.xword	0
	.xword	0
.Ldebug_loc54:
	.xword	.Ltmp155-.Lfunc_begin0
	.xword	.Ltmp157-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	.Ltmp157-.Lfunc_begin0
	.xword	.Ltmp169-.Lfunc_begin0
	.hword	1
	.byte	88
	.xword	0
	.xword	0
.Ldebug_loc55:
	.xword	.Ltmp155-.Lfunc_begin0
	.xword	.Ltmp172-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	0
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc56:
	.xword	.Ltmp155-.Lfunc_begin0
	.xword	.Ltmp171-.Lfunc_begin0
	.hword	1
	.byte	82
	.xword	0
	.xword	0
.Ldebug_loc57:
	.xword	.Ltmp155-.Lfunc_begin0
	.xword	.Ltmp170-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	.Ltmp170-.Lfunc_begin0
	.xword	.Ltmp218-.Lfunc_begin0
	.hword	1
	.byte	102
	.xword	.Ltmp318-.Lfunc_begin0
	.xword	.Lfunc_end3-.Lfunc_begin0
	.hword	1
	.byte	102
	.xword	0
	.xword	0
.Ldebug_loc58:
	.xword	.Ltmp155-.Lfunc_begin0
	.xword	.Ltmp168-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	.Ltmp168-.Lfunc_begin0
	.xword	.Ltmp218-.Lfunc_begin0
	.hword	1
	.byte	100
	.xword	.Ltmp318-.Lfunc_begin0
	.xword	.Ltmp320-.Lfunc_begin0
	.hword	1
	.byte	100
	.xword	0
	.xword	0
.Ldebug_loc59:
	.xword	.Ltmp155-.Lfunc_begin0
	.xword	.Ltmp167-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	.Ltmp167-.Lfunc_begin0
	.xword	.Ltmp218-.Lfunc_begin0
	.hword	1
	.byte	104
	.xword	.Ltmp318-.Lfunc_begin0
	.xword	.Lfunc_end3-.Lfunc_begin0
	.hword	1
	.byte	104
	.xword	0
	.xword	0
.Ldebug_loc60:
	.xword	.Ltmp155-.Lfunc_begin0
	.xword	.Ltmp174-.Lfunc_begin0
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
	.xword	.Ltmp174-.Lfunc_begin0
	.xword	.Ltmp218-.Lfunc_begin0
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
	.xword	.Ltmp318-.Lfunc_begin0
	.xword	.Ltmp320-.Lfunc_begin0
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
	.xword	.Ltmp320-.Lfunc_begin0
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
.Ldebug_loc61:
	.xword	.Ltmp155-.Lfunc_begin0
	.xword	.Ltmp171-.Lfunc_begin0
	.hword	1
	.byte	82
	.xword	0
	.xword	0
.Ldebug_loc62:
	.xword	.Ltmp155-.Lfunc_begin0
	.xword	.Ltmp218-.Lfunc_begin0
	.hword	1
	.byte	99
	.xword	.Ltmp318-.Lfunc_begin0
	.xword	.Lfunc_end3-.Lfunc_begin0
	.hword	1
	.byte	99
	.xword	0
	.xword	0
.Ldebug_loc63:
	.xword	.Ltmp157-.Lfunc_begin0
	.xword	.Ltmp158-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	0
	.byte	159
	.xword	.Ltmp158-.Lfunc_begin0
	.xword	.Ltmp162-.Lfunc_begin0
	.hword	1
	.byte	89
	.xword	0
	.xword	0
.Ldebug_loc64:
	.xword	.Ltmp157-.Lfunc_begin0
	.xword	.Ltmp162-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	0
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc65:
	.xword	.Ltmp157-.Lfunc_begin0
	.xword	.Ltmp167-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	.Ltmp167-.Lfunc_begin0
	.xword	.Ltmp172-.Lfunc_begin0
	.hword	1
	.byte	104
	.xword	0
	.xword	0
.Ldebug_loc66:
	.xword	.Ltmp157-.Lfunc_begin0
	.xword	.Ltmp163-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc67:
	.xword	.Ltmp157-.Lfunc_begin0
	.xword	.Ltmp166-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc68:
	.xword	.Ltmp157-.Lfunc_begin0
	.xword	.Ltmp162-.Lfunc_begin0
	.hword	4
	.byte	120
	.byte	148
	.byte	1
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc69:
	.xword	.Ltmp157-.Lfunc_begin0
	.xword	.Ltmp172-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc70:
	.xword	.Ltmp157-.Lfunc_begin0
	.xword	.Ltmp172-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc71:
	.xword	.Ltmp170-.Lfunc_begin0
	.xword	.Ltmp172-.Lfunc_begin0
	.hword	4
	.byte	16
	.byte	192
	.byte	27
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc72:
	.xword	.Ltmp170-.Lfunc_begin0
	.xword	.Ltmp172-.Lfunc_begin0
	.hword	4
	.byte	16
	.byte	192
	.byte	25
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc73:
	.xword	.Ltmp170-.Lfunc_begin0
	.xword	.Ltmp171-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	.Ltmp171-.Lfunc_begin0
	.xword	.Ltmp173-.Lfunc_begin0
	.hword	1
	.byte	80
	.xword	.Ltmp173-.Lfunc_begin0
	.xword	.Ltmp179-.Lfunc_begin0
	.hword	1
	.byte	101
	.xword	0
	.xword	0
.Ldebug_loc74:
	.xword	.Ltmp170-.Lfunc_begin0
	.xword	.Ltmp179-.Lfunc_begin0
	.hword	1
	.byte	104
	.xword	0
	.xword	0
.Ldebug_loc75:
	.xword	.Ltmp170-.Lfunc_begin0
	.xword	.Ltmp172-.Lfunc_begin0
	.hword	1
	.byte	102
	.xword	0
	.xword	0
.Ldebug_loc76:
	.xword	.Ltmp170-.Lfunc_begin0
	.xword	.Ltmp172-.Lfunc_begin0
	.hword	1
	.byte	102
	.xword	0
	.xword	0
.Ldebug_loc77:
	.xword	.Ltmp170-.Lfunc_begin0
	.xword	.Ltmp179-.Lfunc_begin0
	.hword	1
	.byte	102
	.xword	0
	.xword	0
.Ldebug_loc78:
	.xword	.Ltmp175-.Lfunc_begin0
	.xword	.Ltmp177-.Lfunc_begin0
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
	.xword	.Ltmp177-.Lfunc_begin0
	.xword	.Ltmp178-.Lfunc_begin0
	.hword	1
	.byte	80
	.xword	0
	.xword	0
.Ldebug_loc79:
	.xword	.Ltmp175-.Lfunc_begin0
	.xword	.Ltmp177-.Lfunc_begin0
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
	.xword	.Ltmp177-.Lfunc_begin0
	.xword	.Ltmp178-.Lfunc_begin0
	.hword	1
	.byte	80
	.xword	0
	.xword	0
.Ldebug_loc80:
	.xword	.Ltmp181-.Lfunc_begin0
	.xword	.Ltmp183-.Lfunc_begin0
	.hword	1
	.byte	91
	.xword	0
	.xword	0
.Ldebug_loc81:
	.xword	.Ltmp174-.Lfunc_begin0
	.xword	.Ltmp184-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	0
	.byte	159
	.xword	.Ltmp184-.Lfunc_begin0
	.xword	.Ltmp186-.Lfunc_begin0
	.hword	1
	.byte	88
	.xword	.Ltmp186-.Lfunc_begin0
	.xword	.Ltmp190-.Lfunc_begin0
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
	.xword	.Ltmp197-.Lfunc_begin0
	.xword	.Ltmp198-.Lfunc_begin0
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
	.xword	.Ltmp198-.Lfunc_begin0
	.xword	.Ltmp199-.Lfunc_begin0
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
	.xword	.Ltmp199-.Lfunc_begin0
	.xword	.Ltmp200-.Lfunc_begin0
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
	.xword	.Ltmp200-.Lfunc_begin0
	.xword	.Ltmp201-.Lfunc_begin0
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
	.xword	.Ltmp201-.Lfunc_begin0
	.xword	.Ltmp202-.Lfunc_begin0
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
	.xword	.Ltmp202-.Lfunc_begin0
	.xword	.Ltmp203-.Lfunc_begin0
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
	.xword	.Ltmp203-.Lfunc_begin0
	.xword	.Ltmp204-.Lfunc_begin0
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
	.xword	.Ltmp204-.Lfunc_begin0
	.xword	.Ltmp205-.Lfunc_begin0
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
	.xword	.Ltmp205-.Lfunc_begin0
	.xword	.Ltmp206-.Lfunc_begin0
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
	.xword	.Ltmp206-.Lfunc_begin0
	.xword	.Ltmp207-.Lfunc_begin0
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
	.xword	.Ltmp207-.Lfunc_begin0
	.xword	.Ltmp208-.Lfunc_begin0
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
	.xword	.Ltmp208-.Lfunc_begin0
	.xword	.Ltmp209-.Lfunc_begin0
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
	.xword	.Ltmp209-.Lfunc_begin0
	.xword	.Ltmp210-.Lfunc_begin0
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
	.xword	.Ltmp210-.Lfunc_begin0
	.xword	.Ltmp211-.Lfunc_begin0
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
	.xword	.Ltmp211-.Lfunc_begin0
	.xword	.Ltmp212-.Lfunc_begin0
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
	.xword	.Ltmp212-.Lfunc_begin0
	.xword	.Ltmp215-.Lfunc_begin0
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
.Ldebug_loc82:
	.xword	.Ltmp174-.Lfunc_begin0
	.xword	.Ltmp184-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	0
	.byte	159
	.xword	.Ltmp186-.Lfunc_begin0
	.xword	.Ltmp216-.Lfunc_begin0
	.hword	1
	.byte	96
	.xword	0
	.xword	0
.Ldebug_loc83:
	.xword	.Ltmp174-.Lfunc_begin0
	.xword	.Ltmp184-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	0
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc84:
	.xword	.Ltmp174-.Lfunc_begin0
	.xword	.Ltmp184-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	0
	.byte	159
	.xword	.Ltmp187-.Lfunc_begin0
	.xword	.Ltmp196-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	15
	.byte	159
	.xword	.Ltmp196-.Lfunc_begin0
	.xword	.Ltmp216-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	16
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc85:
	.xword	.Ltmp174-.Lfunc_begin0
	.xword	.Ltmp184-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	0
	.byte	159
	.xword	.Ltmp186-.Lfunc_begin0
	.xword	.Ltmp190-.Lfunc_begin0
	.hword	6
	.byte	126
	.byte	0
	.byte	17
	.byte	16
	.byte	27
	.byte	159
	.xword	.Ltmp212-.Lfunc_begin0
	.xword	.Ltmp216-.Lfunc_begin0
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
.Ldebug_loc86:
	.xword	.Ltmp174-.Lfunc_begin0
	.xword	.Ltmp184-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	0
	.byte	159
	.xword	.Ltmp184-.Lfunc_begin0
	.xword	.Ltmp216-.Lfunc_begin0
	.hword	1
	.byte	89
	.xword	0
	.xword	0
.Ldebug_loc87:
	.xword	.Ltmp174-.Lfunc_begin0
	.xword	.Ltmp178-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	.Ltmp178-.Lfunc_begin0
	.xword	.Ltmp216-.Lfunc_begin0
	.hword	1
	.byte	80
	.xword	0
	.xword	0
.Ldebug_loc88:
	.xword	.Ltmp174-.Lfunc_begin0
	.xword	.Ltmp184-.Lfunc_begin0
	.hword	1
	.byte	101
	.xword	0
	.xword	0
.Ldebug_loc89:
	.xword	.Ltmp177-.Lfunc_begin0
	.xword	.Ltmp179-.Lfunc_begin0
	.hword	4
	.byte	16
	.byte	192
	.byte	27
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc90:
	.xword	.Ltmp177-.Lfunc_begin0
	.xword	.Ltmp179-.Lfunc_begin0
	.hword	4
	.byte	16
	.byte	192
	.byte	25
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc91:
	.xword	.Ltmp186-.Lfunc_begin0
	.xword	.Ltmp187-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	0
	.byte	159
	.xword	.Ltmp187-.Lfunc_begin0
	.xword	.Ltmp188-.Lfunc_begin0
	.hword	5
	.byte	129
	.byte	0
	.byte	51
	.byte	26
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc92:
	.xword	.Ltmp196-.Lfunc_begin0
	.xword	.Ltmp213-.Lfunc_begin0
	.hword	1
	.byte	97
	.xword	0
	.xword	0
.Ldebug_loc93:
	.xword	.Ltmp221-.Lfunc_begin0
	.xword	.Ltmp223-.Lfunc_begin0
	.hword	1
	.byte	90
	.xword	.Ltmp224-.Lfunc_begin0
	.xword	.Ltmp229-.Lfunc_begin0
	.hword	1
	.byte	82
	.xword	0
	.xword	0
.Ldebug_loc94:
	.xword	.Ltmp222-.Lfunc_begin0
	.xword	.Ltmp224-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	.Ltmp224-.Lfunc_begin0
	.xword	.Ltmp232-.Lfunc_begin0
	.hword	1
	.byte	88
	.xword	0
	.xword	0
.Ldebug_loc95:
	.xword	.Ltmp222-.Lfunc_begin0
	.xword	.Ltmp232-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	1
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc96:
	.xword	.Ltmp222-.Lfunc_begin0
	.xword	.Ltmp232-.Lfunc_begin0
	.hword	1
	.byte	82
	.xword	0
	.xword	0
.Ldebug_loc97:
	.xword	.Ltmp222-.Lfunc_begin0
	.xword	.Ltmp232-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc98:
	.xword	.Ltmp222-.Lfunc_begin0
	.xword	.Ltmp232-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc99:
	.xword	.Ltmp222-.Lfunc_begin0
	.xword	.Ltmp232-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc100:
	.xword	.Ltmp222-.Lfunc_begin0
	.xword	.Ltmp232-.Lfunc_begin0
	.hword	1
	.byte	82
	.xword	0
	.xword	0
.Ldebug_loc101:
	.xword	.Ltmp222-.Lfunc_begin0
	.xword	.Ltmp232-.Lfunc_begin0
	.hword	1
	.byte	99
	.xword	0
	.xword	0
.Ldebug_loc102:
	.xword	.Ltmp224-.Lfunc_begin0
	.xword	.Ltmp225-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	0
	.byte	159
	.xword	.Ltmp225-.Lfunc_begin0
	.xword	.Ltmp229-.Lfunc_begin0
	.hword	1
	.byte	89
	.xword	0
	.xword	0
.Ldebug_loc103:
	.xword	.Ltmp224-.Lfunc_begin0
	.xword	.Ltmp229-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	0
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc104:
	.xword	.Ltmp224-.Lfunc_begin0
	.xword	.Ltmp232-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc105:
	.xword	.Ltmp224-.Lfunc_begin0
	.xword	.Ltmp230-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc106:
	.xword	.Ltmp224-.Lfunc_begin0
	.xword	.Ltmp231-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc107:
	.xword	.Ltmp224-.Lfunc_begin0
	.xword	.Ltmp229-.Lfunc_begin0
	.hword	4
	.byte	120
	.byte	212
	.byte	3
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc108:
	.xword	.Ltmp224-.Lfunc_begin0
	.xword	.Ltmp232-.Lfunc_begin0
	.hword	4
	.byte	120
	.byte	212
	.byte	14
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc109:
	.xword	.Ltmp224-.Lfunc_begin0
	.xword	.Ltmp232-.Lfunc_begin0
	.hword	4
	.byte	120
	.byte	212
	.byte	3
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc110:
	.xword	.Ltmp234-.Lfunc_begin0
	.xword	.Ltmp236-.Lfunc_begin0
	.hword	1
	.byte	90
	.xword	.Ltmp237-.Lfunc_begin0
	.xword	.Ltmp242-.Lfunc_begin0
	.hword	1
	.byte	82
	.xword	0
	.xword	0
.Ldebug_loc111:
	.xword	.Ltmp235-.Lfunc_begin0
	.xword	.Ltmp237-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	.Ltmp237-.Lfunc_begin0
	.xword	.Ltmp242-.Lfunc_begin0
	.hword	1
	.byte	88
	.xword	0
	.xword	0
.Ldebug_loc112:
	.xword	.Ltmp235-.Lfunc_begin0
	.xword	.Ltmp242-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	2
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc113:
	.xword	.Ltmp235-.Lfunc_begin0
	.xword	.Ltmp242-.Lfunc_begin0
	.hword	1
	.byte	82
	.xword	0
	.xword	0
.Ldebug_loc114:
	.xword	.Ltmp235-.Lfunc_begin0
	.xword	.Ltmp242-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc115:
	.xword	.Ltmp235-.Lfunc_begin0
	.xword	.Ltmp242-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc116:
	.xword	.Ltmp235-.Lfunc_begin0
	.xword	.Ltmp242-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc117:
	.xword	.Ltmp235-.Lfunc_begin0
	.xword	.Ltmp242-.Lfunc_begin0
	.hword	2
	.byte	50
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc118:
	.xword	.Ltmp235-.Lfunc_begin0
	.xword	.Ltmp242-.Lfunc_begin0
	.hword	1
	.byte	82
	.xword	0
	.xword	0
.Ldebug_loc119:
	.xword	.Ltmp235-.Lfunc_begin0
	.xword	.Ltmp242-.Lfunc_begin0
	.hword	1
	.byte	99
	.xword	0
	.xword	0
.Ldebug_loc120:
	.xword	.Ltmp237-.Lfunc_begin0
	.xword	.Ltmp238-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	0
	.byte	159
	.xword	.Ltmp238-.Lfunc_begin0
	.xword	.Ltmp242-.Lfunc_begin0
	.hword	1
	.byte	89
	.xword	0
	.xword	0
.Ldebug_loc121:
	.xword	.Ltmp237-.Lfunc_begin0
	.xword	.Ltmp242-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	0
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc122:
	.xword	.Ltmp237-.Lfunc_begin0
	.xword	.Ltmp242-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc123:
	.xword	.Ltmp237-.Lfunc_begin0
	.xword	.Ltmp242-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc124:
	.xword	.Ltmp237-.Lfunc_begin0
	.xword	.Ltmp242-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc125:
	.xword	.Ltmp237-.Lfunc_begin0
	.xword	.Ltmp242-.Lfunc_begin0
	.hword	4
	.byte	120
	.byte	212
	.byte	1
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc126:
	.xword	.Ltmp237-.Lfunc_begin0
	.xword	.Ltmp242-.Lfunc_begin0
	.hword	4
	.byte	120
	.byte	212
	.byte	6
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc127:
	.xword	.Ltmp237-.Lfunc_begin0
	.xword	.Ltmp242-.Lfunc_begin0
	.hword	4
	.byte	120
	.byte	212
	.byte	1
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc128:
	.xword	.Ltmp244-.Lfunc_begin0
	.xword	.Ltmp246-.Lfunc_begin0
	.hword	1
	.byte	90
	.xword	.Ltmp247-.Lfunc_begin0
	.xword	.Ltmp252-.Lfunc_begin0
	.hword	1
	.byte	82
	.xword	0
	.xword	0
.Ldebug_loc129:
	.xword	.Ltmp245-.Lfunc_begin0
	.xword	.Ltmp247-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	.Ltmp247-.Lfunc_begin0
	.xword	.Ltmp252-.Lfunc_begin0
	.hword	1
	.byte	88
	.xword	0
	.xword	0
.Ldebug_loc130:
	.xword	.Ltmp245-.Lfunc_begin0
	.xword	.Ltmp252-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	2
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc131:
	.xword	.Ltmp245-.Lfunc_begin0
	.xword	.Ltmp252-.Lfunc_begin0
	.hword	1
	.byte	82
	.xword	0
	.xword	0
.Ldebug_loc132:
	.xword	.Ltmp245-.Lfunc_begin0
	.xword	.Ltmp252-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc133:
	.xword	.Ltmp245-.Lfunc_begin0
	.xword	.Ltmp252-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc134:
	.xword	.Ltmp245-.Lfunc_begin0
	.xword	.Ltmp252-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc135:
	.xword	.Ltmp245-.Lfunc_begin0
	.xword	.Ltmp252-.Lfunc_begin0
	.hword	2
	.byte	50
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc136:
	.xword	.Ltmp245-.Lfunc_begin0
	.xword	.Ltmp252-.Lfunc_begin0
	.hword	1
	.byte	82
	.xword	0
	.xword	0
.Ldebug_loc137:
	.xword	.Ltmp245-.Lfunc_begin0
	.xword	.Ltmp252-.Lfunc_begin0
	.hword	1
	.byte	99
	.xword	0
	.xword	0
.Ldebug_loc138:
	.xword	.Ltmp247-.Lfunc_begin0
	.xword	.Ltmp248-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	0
	.byte	159
	.xword	.Ltmp248-.Lfunc_begin0
	.xword	.Ltmp252-.Lfunc_begin0
	.hword	1
	.byte	89
	.xword	0
	.xword	0
.Ldebug_loc139:
	.xword	.Ltmp247-.Lfunc_begin0
	.xword	.Ltmp252-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	0
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc140:
	.xword	.Ltmp247-.Lfunc_begin0
	.xword	.Ltmp252-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc141:
	.xword	.Ltmp247-.Lfunc_begin0
	.xword	.Ltmp252-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc142:
	.xword	.Ltmp247-.Lfunc_begin0
	.xword	.Ltmp252-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc143:
	.xword	.Ltmp247-.Lfunc_begin0
	.xword	.Ltmp252-.Lfunc_begin0
	.hword	4
	.byte	120
	.byte	212
	.byte	1
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc144:
	.xword	.Ltmp247-.Lfunc_begin0
	.xword	.Ltmp252-.Lfunc_begin0
	.hword	4
	.byte	120
	.byte	212
	.byte	6
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc145:
	.xword	.Ltmp247-.Lfunc_begin0
	.xword	.Ltmp252-.Lfunc_begin0
	.hword	4
	.byte	120
	.byte	212
	.byte	1
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc146:
	.xword	.Ltmp256-.Lfunc_begin0
	.xword	.Ltmp258-.Lfunc_begin0
	.hword	1
	.byte	90
	.xword	.Ltmp259-.Lfunc_begin0
	.xword	.Ltmp264-.Lfunc_begin0
	.hword	1
	.byte	82
	.xword	0
	.xword	0
.Ldebug_loc147:
	.xword	.Ltmp257-.Lfunc_begin0
	.xword	.Ltmp259-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	.Ltmp259-.Lfunc_begin0
	.xword	.Ltmp267-.Lfunc_begin0
	.hword	1
	.byte	88
	.xword	0
	.xword	0
.Ldebug_loc148:
	.xword	.Ltmp257-.Lfunc_begin0
	.xword	.Ltmp267-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	3
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc149:
	.xword	.Ltmp257-.Lfunc_begin0
	.xword	.Ltmp267-.Lfunc_begin0
	.hword	1
	.byte	82
	.xword	0
	.xword	0
.Ldebug_loc150:
	.xword	.Ltmp257-.Lfunc_begin0
	.xword	.Ltmp267-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc151:
	.xword	.Ltmp257-.Lfunc_begin0
	.xword	.Ltmp267-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc152:
	.xword	.Ltmp257-.Lfunc_begin0
	.xword	.Ltmp267-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc153:
	.xword	.Ltmp257-.Lfunc_begin0
	.xword	.Ltmp267-.Lfunc_begin0
	.hword	2
	.byte	51
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc154:
	.xword	.Ltmp257-.Lfunc_begin0
	.xword	.Ltmp267-.Lfunc_begin0
	.hword	1
	.byte	82
	.xword	0
	.xword	0
.Ldebug_loc155:
	.xword	.Ltmp257-.Lfunc_begin0
	.xword	.Ltmp267-.Lfunc_begin0
	.hword	1
	.byte	99
	.xword	0
	.xword	0
.Ldebug_loc156:
	.xword	.Ltmp259-.Lfunc_begin0
	.xword	.Ltmp260-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	0
	.byte	159
	.xword	.Ltmp260-.Lfunc_begin0
	.xword	.Ltmp264-.Lfunc_begin0
	.hword	1
	.byte	89
	.xword	0
	.xword	0
.Ldebug_loc157:
	.xword	.Ltmp259-.Lfunc_begin0
	.xword	.Ltmp264-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	0
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc158:
	.xword	.Ltmp259-.Lfunc_begin0
	.xword	.Ltmp267-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc159:
	.xword	.Ltmp259-.Lfunc_begin0
	.xword	.Ltmp265-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc160:
	.xword	.Ltmp259-.Lfunc_begin0
	.xword	.Ltmp266-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc161:
	.xword	.Ltmp259-.Lfunc_begin0
	.xword	.Ltmp264-.Lfunc_begin0
	.hword	4
	.byte	120
	.byte	148
	.byte	2
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc162:
	.xword	.Ltmp259-.Lfunc_begin0
	.xword	.Ltmp267-.Lfunc_begin0
	.hword	4
	.byte	120
	.byte	212
	.byte	8
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc163:
	.xword	.Ltmp259-.Lfunc_begin0
	.xword	.Ltmp267-.Lfunc_begin0
	.hword	4
	.byte	120
	.byte	148
	.byte	2
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc164:
	.xword	.Ltmp269-.Lfunc_begin0
	.xword	.Ltmp271-.Lfunc_begin0
	.hword	1
	.byte	90
	.xword	.Ltmp272-.Lfunc_begin0
	.xword	.Ltmp277-.Lfunc_begin0
	.hword	1
	.byte	82
	.xword	0
	.xword	0
.Ldebug_loc165:
	.xword	.Ltmp270-.Lfunc_begin0
	.xword	.Ltmp272-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	.Ltmp272-.Lfunc_begin0
	.xword	.Ltmp280-.Lfunc_begin0
	.hword	1
	.byte	88
	.xword	0
	.xword	0
.Ldebug_loc166:
	.xword	.Ltmp270-.Lfunc_begin0
	.xword	.Ltmp280-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	4
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc167:
	.xword	.Ltmp270-.Lfunc_begin0
	.xword	.Ltmp280-.Lfunc_begin0
	.hword	1
	.byte	82
	.xword	0
	.xword	0
.Ldebug_loc168:
	.xword	.Ltmp270-.Lfunc_begin0
	.xword	.Ltmp280-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc169:
	.xword	.Ltmp270-.Lfunc_begin0
	.xword	.Ltmp280-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc170:
	.xword	.Ltmp270-.Lfunc_begin0
	.xword	.Ltmp280-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc171:
	.xword	.Ltmp270-.Lfunc_begin0
	.xword	.Ltmp280-.Lfunc_begin0
	.hword	2
	.byte	52
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc172:
	.xword	.Ltmp270-.Lfunc_begin0
	.xword	.Ltmp280-.Lfunc_begin0
	.hword	1
	.byte	82
	.xword	0
	.xword	0
.Ldebug_loc173:
	.xword	.Ltmp270-.Lfunc_begin0
	.xword	.Ltmp280-.Lfunc_begin0
	.hword	1
	.byte	99
	.xword	0
	.xword	0
.Ldebug_loc174:
	.xword	.Ltmp272-.Lfunc_begin0
	.xword	.Ltmp273-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	0
	.byte	159
	.xword	.Ltmp273-.Lfunc_begin0
	.xword	.Ltmp277-.Lfunc_begin0
	.hword	1
	.byte	89
	.xword	0
	.xword	0
.Ldebug_loc175:
	.xword	.Ltmp272-.Lfunc_begin0
	.xword	.Ltmp277-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	0
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc176:
	.xword	.Ltmp272-.Lfunc_begin0
	.xword	.Ltmp280-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc177:
	.xword	.Ltmp272-.Lfunc_begin0
	.xword	.Ltmp278-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc178:
	.xword	.Ltmp272-.Lfunc_begin0
	.xword	.Ltmp279-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc179:
	.xword	.Ltmp272-.Lfunc_begin0
	.xword	.Ltmp277-.Lfunc_begin0
	.hword	4
	.byte	120
	.byte	212
	.byte	2
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc180:
	.xword	.Ltmp272-.Lfunc_begin0
	.xword	.Ltmp280-.Lfunc_begin0
	.hword	4
	.byte	120
	.byte	212
	.byte	10
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc181:
	.xword	.Ltmp272-.Lfunc_begin0
	.xword	.Ltmp280-.Lfunc_begin0
	.hword	4
	.byte	120
	.byte	212
	.byte	2
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc182:
	.xword	.Ltmp282-.Lfunc_begin0
	.xword	.Ltmp284-.Lfunc_begin0
	.hword	1
	.byte	90
	.xword	.Ltmp285-.Lfunc_begin0
	.xword	.Ltmp290-.Lfunc_begin0
	.hword	1
	.byte	82
	.xword	0
	.xword	0
.Ldebug_loc183:
	.xword	.Ltmp283-.Lfunc_begin0
	.xword	.Ltmp285-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	.Ltmp285-.Lfunc_begin0
	.xword	.Ltmp293-.Lfunc_begin0
	.hword	1
	.byte	88
	.xword	0
	.xword	0
.Ldebug_loc184:
	.xword	.Ltmp283-.Lfunc_begin0
	.xword	.Ltmp293-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	5
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc185:
	.xword	.Ltmp283-.Lfunc_begin0
	.xword	.Ltmp293-.Lfunc_begin0
	.hword	1
	.byte	82
	.xword	0
	.xword	0
.Ldebug_loc186:
	.xword	.Ltmp283-.Lfunc_begin0
	.xword	.Ltmp293-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc187:
	.xword	.Ltmp283-.Lfunc_begin0
	.xword	.Ltmp293-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc188:
	.xword	.Ltmp283-.Lfunc_begin0
	.xword	.Ltmp293-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc189:
	.xword	.Ltmp283-.Lfunc_begin0
	.xword	.Ltmp293-.Lfunc_begin0
	.hword	2
	.byte	53
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc190:
	.xword	.Ltmp283-.Lfunc_begin0
	.xword	.Ltmp293-.Lfunc_begin0
	.hword	1
	.byte	82
	.xword	0
	.xword	0
.Ldebug_loc191:
	.xword	.Ltmp283-.Lfunc_begin0
	.xword	.Ltmp293-.Lfunc_begin0
	.hword	1
	.byte	99
	.xword	0
	.xword	0
.Ldebug_loc192:
	.xword	.Ltmp285-.Lfunc_begin0
	.xword	.Ltmp286-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	0
	.byte	159
	.xword	.Ltmp286-.Lfunc_begin0
	.xword	.Ltmp290-.Lfunc_begin0
	.hword	1
	.byte	89
	.xword	0
	.xword	0
.Ldebug_loc193:
	.xword	.Ltmp285-.Lfunc_begin0
	.xword	.Ltmp290-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	0
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc194:
	.xword	.Ltmp285-.Lfunc_begin0
	.xword	.Ltmp293-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc195:
	.xword	.Ltmp285-.Lfunc_begin0
	.xword	.Ltmp291-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc196:
	.xword	.Ltmp285-.Lfunc_begin0
	.xword	.Ltmp292-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc197:
	.xword	.Ltmp285-.Lfunc_begin0
	.xword	.Ltmp290-.Lfunc_begin0
	.hword	4
	.byte	120
	.byte	148
	.byte	3
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc198:
	.xword	.Ltmp285-.Lfunc_begin0
	.xword	.Ltmp293-.Lfunc_begin0
	.hword	4
	.byte	120
	.byte	212
	.byte	12
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc199:
	.xword	.Ltmp285-.Lfunc_begin0
	.xword	.Ltmp293-.Lfunc_begin0
	.hword	4
	.byte	120
	.byte	148
	.byte	3
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc200:
	.xword	.Ltmp295-.Lfunc_begin0
	.xword	.Ltmp297-.Lfunc_begin0
	.hword	1
	.byte	90
	.xword	.Ltmp298-.Lfunc_begin0
	.xword	.Ltmp303-.Lfunc_begin0
	.hword	1
	.byte	82
	.xword	0
	.xword	0
.Ldebug_loc201:
	.xword	.Ltmp296-.Lfunc_begin0
	.xword	.Ltmp298-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	.Ltmp298-.Lfunc_begin0
	.xword	.Ltmp305-.Lfunc_begin0
	.hword	1
	.byte	88
	.xword	0
	.xword	0
.Ldebug_loc202:
	.xword	.Ltmp296-.Lfunc_begin0
	.xword	.Ltmp305-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	6
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc203:
	.xword	.Ltmp296-.Lfunc_begin0
	.xword	.Ltmp305-.Lfunc_begin0
	.hword	1
	.byte	82
	.xword	0
	.xword	0
.Ldebug_loc204:
	.xword	.Ltmp296-.Lfunc_begin0
	.xword	.Ltmp305-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc205:
	.xword	.Ltmp296-.Lfunc_begin0
	.xword	.Ltmp305-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc206:
	.xword	.Ltmp296-.Lfunc_begin0
	.xword	.Ltmp305-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc207:
	.xword	.Ltmp296-.Lfunc_begin0
	.xword	.Ltmp305-.Lfunc_begin0
	.hword	1
	.byte	82
	.xword	0
	.xword	0
.Ldebug_loc208:
	.xword	.Ltmp296-.Lfunc_begin0
	.xword	.Ltmp305-.Lfunc_begin0
	.hword	1
	.byte	99
	.xword	0
	.xword	0
.Ldebug_loc209:
	.xword	.Ltmp298-.Lfunc_begin0
	.xword	.Ltmp299-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	0
	.byte	159
	.xword	.Ltmp299-.Lfunc_begin0
	.xword	.Ltmp303-.Lfunc_begin0
	.hword	1
	.byte	89
	.xword	0
	.xword	0
.Ldebug_loc210:
	.xword	.Ltmp298-.Lfunc_begin0
	.xword	.Ltmp303-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	0
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc211:
	.xword	.Ltmp298-.Lfunc_begin0
	.xword	.Ltmp305-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc212:
	.xword	.Ltmp298-.Lfunc_begin0
	.xword	.Ltmp304-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc213:
	.xword	.Ltmp298-.Lfunc_begin0
	.xword	.Ltmp305-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc214:
	.xword	.Ltmp298-.Lfunc_begin0
	.xword	.Ltmp303-.Lfunc_begin0
	.hword	4
	.byte	120
	.byte	148
	.byte	4
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc215:
	.xword	.Ltmp298-.Lfunc_begin0
	.xword	.Ltmp305-.Lfunc_begin0
	.hword	4
	.byte	120
	.byte	212
	.byte	16
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc216:
	.xword	.Ltmp298-.Lfunc_begin0
	.xword	.Ltmp305-.Lfunc_begin0
	.hword	4
	.byte	120
	.byte	148
	.byte	4
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc217:
	.xword	.Lfunc_begin4-.Lfunc_begin0
	.xword	.Ltmp325-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	0
	.byte	159
	.xword	.Ltmp325-.Lfunc_begin0
	.xword	.Ltmp367-.Lfunc_begin0
	.hword	1
	.byte	90
	.xword	0
	.xword	0
.Ldebug_loc218:
	.xword	.Lfunc_begin4-.Lfunc_begin0
	.xword	.Ltmp325-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	.Ltmp326-.Lfunc_begin0
	.xword	.Ltmp328-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	.Ltmp328-.Lfunc_begin0
	.xword	.Ltmp330-.Lfunc_begin0
	.hword	6
	.byte	125
	.byte	0
	.byte	17
	.byte	4
	.byte	27
	.byte	159
	.xword	.Ltmp363-.Lfunc_begin0
	.xword	.Ltmp367-.Lfunc_begin0
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
.Ldebug_loc219:
	.xword	.Lfunc_begin4-.Lfunc_begin0
	.xword	.Ltmp325-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	.Ltmp329-.Lfunc_begin0
	.xword	.Ltmp333-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	.Ltmp333-.Lfunc_begin0
	.xword	.Ltmp335-.Lfunc_begin0
	.hword	2
	.byte	49
	.byte	159
	.xword	.Ltmp335-.Lfunc_begin0
	.xword	.Ltmp337-.Lfunc_begin0
	.hword	2
	.byte	50
	.byte	159
	.xword	.Ltmp337-.Lfunc_begin0
	.xword	.Ltmp339-.Lfunc_begin0
	.hword	2
	.byte	51
	.byte	159
	.xword	.Ltmp339-.Lfunc_begin0
	.xword	.Ltmp341-.Lfunc_begin0
	.hword	2
	.byte	52
	.byte	159
	.xword	.Ltmp341-.Lfunc_begin0
	.xword	.Ltmp343-.Lfunc_begin0
	.hword	2
	.byte	53
	.byte	159
	.xword	.Ltmp343-.Lfunc_begin0
	.xword	.Ltmp345-.Lfunc_begin0
	.hword	2
	.byte	54
	.byte	159
	.xword	.Ltmp345-.Lfunc_begin0
	.xword	.Ltmp347-.Lfunc_begin0
	.hword	2
	.byte	55
	.byte	159
	.xword	.Ltmp347-.Lfunc_begin0
	.xword	.Ltmp349-.Lfunc_begin0
	.hword	2
	.byte	56
	.byte	159
	.xword	.Ltmp349-.Lfunc_begin0
	.xword	.Ltmp351-.Lfunc_begin0
	.hword	2
	.byte	57
	.byte	159
	.xword	.Ltmp351-.Lfunc_begin0
	.xword	.Ltmp353-.Lfunc_begin0
	.hword	2
	.byte	58
	.byte	159
	.xword	.Ltmp353-.Lfunc_begin0
	.xword	.Ltmp355-.Lfunc_begin0
	.hword	2
	.byte	59
	.byte	159
	.xword	.Ltmp355-.Lfunc_begin0
	.xword	.Ltmp357-.Lfunc_begin0
	.hword	2
	.byte	60
	.byte	159
	.xword	.Ltmp357-.Lfunc_begin0
	.xword	.Ltmp359-.Lfunc_begin0
	.hword	2
	.byte	61
	.byte	159
	.xword	.Ltmp359-.Lfunc_begin0
	.xword	.Ltmp361-.Lfunc_begin0
	.hword	2
	.byte	62
	.byte	159
	.xword	.Ltmp361-.Lfunc_begin0
	.xword	.Ltmp363-.Lfunc_begin0
	.hword	2
	.byte	63
	.byte	159
	.xword	.Ltmp363-.Lfunc_begin0
	.xword	.Ltmp367-.Lfunc_begin0
	.hword	2
	.byte	64
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc220:
	.xword	.Ltmp325-.Lfunc_begin0
	.xword	.Ltmp326-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	.Ltmp326-.Lfunc_begin0
	.xword	.Ltmp328-.Lfunc_begin0
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
.Ldebug_loc221:
	.xword	.Ltmp329-.Lfunc_begin0
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
	.xword	.Ltmp344-.Lfunc_begin0
	.xword	.Ltmp345-.Lfunc_begin0
	.hword	1
	.byte	97
	.xword	.Ltmp345-.Lfunc_begin0
	.xword	.Ltmp346-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	.Ltmp346-.Lfunc_begin0
	.xword	.Ltmp347-.Lfunc_begin0
	.hword	1
	.byte	97
	.xword	.Ltmp347-.Lfunc_begin0
	.xword	.Ltmp348-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	.Ltmp348-.Lfunc_begin0
	.xword	.Ltmp349-.Lfunc_begin0
	.hword	1
	.byte	97
	.xword	.Ltmp349-.Lfunc_begin0
	.xword	.Ltmp350-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	.Ltmp350-.Lfunc_begin0
	.xword	.Ltmp351-.Lfunc_begin0
	.hword	1
	.byte	97
	.xword	.Ltmp351-.Lfunc_begin0
	.xword	.Ltmp352-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	.Ltmp352-.Lfunc_begin0
	.xword	.Ltmp353-.Lfunc_begin0
	.hword	1
	.byte	97
	.xword	.Ltmp353-.Lfunc_begin0
	.xword	.Ltmp354-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	.Ltmp354-.Lfunc_begin0
	.xword	.Ltmp355-.Lfunc_begin0
	.hword	1
	.byte	97
	.xword	.Ltmp355-.Lfunc_begin0
	.xword	.Ltmp356-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	.Ltmp356-.Lfunc_begin0
	.xword	.Ltmp357-.Lfunc_begin0
	.hword	1
	.byte	97
	.xword	.Ltmp357-.Lfunc_begin0
	.xword	.Ltmp358-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	.Ltmp358-.Lfunc_begin0
	.xword	.Ltmp359-.Lfunc_begin0
	.hword	1
	.byte	97
	.xword	.Ltmp359-.Lfunc_begin0
	.xword	.Ltmp360-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	.Ltmp360-.Lfunc_begin0
	.xword	.Ltmp361-.Lfunc_begin0
	.hword	1
	.byte	97
	.xword	.Ltmp361-.Lfunc_begin0
	.xword	.Ltmp362-.Lfunc_begin0
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
	.byte	5
	.byte	0
	.byte	2
	.byte	24
	.byte	49
	.byte	19
	.byte	0
	.byte	0
	.byte	9
	.byte	52
	.byte	0
	.byte	2
	.byte	23
	.byte	49
	.byte	19
	.byte	0
	.byte	0
	.byte	10
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

	.word	.Linfo_string141
	.byte	2
	.hword	563

	.word	980

	.byte	3
	.word	.Linfo_string3
	.word	882
	.byte	2
	.hword	565
	.byte	9
	.byte	3
	.xword	rkf_wf_get_lut.stype
	.byte	3
	.word	.Linfo_string19
	.word	980
	.byte	2
	.hword	566
	.byte	9
	.byte	3
	.xword	rkf_wf_get_lut.sftemp
	.byte	4
	.word	.Ldebug_loc14
	.word	.Linfo_string99
	.byte	2
	.hword	563
	.word	2309
	.byte	4
	.word	.Ldebug_loc15
	.word	.Linfo_string104
	.byte	2
	.hword	563
	.word	882
	.byte	4
	.word	.Ldebug_loc16
	.word	.Linfo_string105
	.byte	2
	.hword	563
	.word	980
	.byte	4
	.word	.Ldebug_loc17
	.word	.Linfo_string146
	.byte	2
	.hword	563
	.word	980
	.byte	5
	.word	.Ldebug_loc18
	.word	.Linfo_string87
	.byte	2
	.hword	568
	.word	980
	.byte	5
	.word	.Ldebug_loc19
	.word	.Linfo_string147
	.byte	2
	.hword	567
	.word	980
	.byte	6
	.word	2072
	.word	.Ldebug_ranges3
	.byte	2
	.hword	605
	.byte	4
	.byte	7
	.word	.Ldebug_loc20
	.word	2085
	.byte	8
	.byte	1
	.byte	102
	.word	2097
	.byte	9
	.word	.Ldebug_loc21
	.word	2109
	.byte	9
	.word	.Ldebug_loc22
	.word	2121
	.byte	0
	.byte	6
	.word	2383
	.word	.Ldebug_ranges4
	.byte	2
	.hword	618
	.byte	15
	.byte	7
	.word	.Ldebug_loc41
	.word	2396
	.byte	7
	.word	.Ldebug_loc40
	.word	2408
	.byte	7
	.word	.Ldebug_loc39
	.word	2420
	.byte	9
	.word	.Ldebug_loc38
	.word	2432
	.byte	6
	.word	2235
	.word	.Ldebug_ranges5
	.byte	2
	.hword	520
	.byte	15
	.byte	7
	.word	.Ldebug_loc37
	.word	2248
	.byte	7
	.word	.Ldebug_loc36
	.word	2260
	.byte	9
	.word	.Ldebug_loc33
	.word	2272
	.byte	9
	.word	.Ldebug_loc34
	.word	2284
	.byte	9
	.word	.Ldebug_loc35
	.word	2296
	.byte	6
	.word	2134
	.word	.Ldebug_ranges6
	.byte	2
	.hword	435
	.byte	9
	.byte	7
	.word	.Ldebug_loc32
	.word	2146
	.byte	7
	.word	.Ldebug_loc31
	.word	2157
	.byte	9
	.word	.Ldebug_loc25
	.word	2168
	.byte	9
	.word	.Ldebug_loc26
	.word	2179
	.byte	9
	.word	.Ldebug_loc27
	.word	2190
	.byte	9
	.word	.Ldebug_loc28
	.word	2201
	.byte	9
	.word	.Ldebug_loc29
	.word	2212
	.byte	9
	.word	.Ldebug_loc30
	.word	2223
	.byte	10
	.word	2445
	.xword	.Ltmp101
	.word	.Ltmp109-.Ltmp101
	.byte	2
	.byte	228
	.byte	7
	.byte	8
	.byte	4
	.byte	120
	.ascii	"\224\004"
	.byte	159
	.word	2457
	.byte	7
	.word	.Ldebug_loc24
	.word	2468
	.byte	9
	.word	.Ldebug_loc23
	.word	2479
	.byte	11
	.byte	0
	.word	2490
	.byte	12
	.xword	.Ltmp101
	.word	.Ltmp102-.Ltmp101
	.byte	11
	.byte	0
	.word	2502
	.byte	13
	.byte	1
	.byte	99
	.word	2513
	.byte	0
	.byte	12
	.xword	.Ltmp102
	.word	.Ltmp104-.Ltmp102
	.byte	11
	.byte	50
	.word	2526
	.byte	13
	.byte	1
	.byte	90
	.word	2537
	.byte	0
	.byte	0
	.byte	0
	.byte	6
	.word	2680
	.word	.Ldebug_ranges7
	.byte	2
	.hword	440
	.byte	17
	.byte	7
	.word	.Ldebug_loc45
	.word	2692
	.byte	7
	.word	.Ldebug_loc48
	.word	2703
	.byte	9
	.word	.Ldebug_loc44
	.word	2714
	.byte	14
	.word	2642
	.word	.Ldebug_ranges8
	.byte	2
	.byte	245
	.byte	8
	.byte	7
	.word	.Ldebug_loc47
	.word	2655
	.byte	7
	.word	.Ldebug_loc43
	.word	2667
	.byte	6
	.word	2550
	.word	.Ldebug_ranges9
	.byte	4
	.hword	689
	.byte	9
	.byte	7
	.word	.Ldebug_loc46
	.word	2563
	.byte	7
	.word	.Ldebug_loc42
	.word	2575
	.byte	0
	.byte	0
	.byte	0
	.byte	15
	.word	2726
	.xword	.Ltmp114
	.word	.Ltmp115-.Ltmp114
	.byte	2
	.hword	436
	.byte	14
	.byte	8
	.byte	1
	.byte	106
	.word	2738
	.byte	0
	.byte	0
	.byte	0
	.byte	16
	.word	2750
	.xword	.Ltmp72
	.byte	17
	.word	2769
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
	.word	5114
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
	.word	2750
	.xword	.Ltmp99
	.byte	17
	.word	5114
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
	.byte	16
	.word	2750
	.xword	.Ltmp134
	.byte	16
	.word	5291
	.xword	.Ltmp136
	.byte	17
	.word	2769
	.xword	.Ltmp138
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
	.word	5114
	.xword	.Ltmp141
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
	.word	973
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
	.word	1008
	.byte	2
	.byte	55
	.byte	9
	.byte	3
	.xword	global_waveform
	.byte	23
	.word	1013
	.byte	24
	.word	.Linfo_string45
	.hword	2388
	.byte	2
	.byte	30
	.byte	25
	.word	.Linfo_string22
	.word	980
	.byte	2
	.byte	31
	.byte	0
	.byte	25
	.word	.Linfo_string23
	.word	1287
	.byte	2
	.byte	32
	.byte	4
	.byte	25
	.word	.Linfo_string26
	.word	1287
	.byte	2
	.byte	33
	.byte	20
	.byte	25
	.word	.Linfo_string27
	.word	1287
	.byte	2
	.byte	34
	.byte	36
	.byte	25
	.word	.Linfo_string28
	.word	1287
	.byte	2
	.byte	35
	.byte	52
	.byte	25
	.word	.Linfo_string29
	.word	1287
	.byte	2
	.byte	36
	.byte	68
	.byte	25
	.word	.Linfo_string30
	.word	1313
	.byte	2
	.byte	37
	.byte	84
	.byte	25
	.word	.Linfo_string31
	.word	1313
	.byte	2
	.byte	38
	.byte	148
	.byte	25
	.word	.Linfo_string32
	.word	1313
	.byte	2
	.byte	39
	.byte	212
	.byte	26
	.word	.Linfo_string33
	.word	1313
	.byte	2
	.byte	40
	.hword	276
	.byte	26
	.word	.Linfo_string34
	.word	1313
	.byte	2
	.byte	41
	.hword	340
	.byte	26
	.word	.Linfo_string35
	.word	1313
	.byte	2
	.byte	42
	.hword	404
	.byte	26
	.word	.Linfo_string36
	.word	1313
	.byte	2
	.byte	43
	.hword	468
	.byte	26
	.word	.Linfo_string37
	.word	1313
	.byte	2
	.byte	44
	.hword	532
	.byte	26
	.word	.Linfo_string38
	.word	1325
	.byte	2
	.byte	45
	.hword	596
	.byte	26
	.word	.Linfo_string39
	.word	1325
	.byte	2
	.byte	46
	.hword	852
	.byte	26
	.word	.Linfo_string40
	.word	1325
	.byte	2
	.byte	47
	.hword	1108
	.byte	26
	.word	.Linfo_string41
	.word	1325
	.byte	2
	.byte	48
	.hword	1364
	.byte	26
	.word	.Linfo_string42
	.word	1325
	.byte	2
	.byte	49
	.hword	1620
	.byte	26
	.word	.Linfo_string43
	.word	1325
	.byte	2
	.byte	50
	.hword	1876
	.byte	26
	.word	.Linfo_string44
	.word	1325
	.byte	2
	.byte	51
	.hword	2132
	.byte	0
	.byte	27
	.word	1299
	.byte	28
	.word	1306
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
	.word	1299
	.byte	28
	.word	1306
	.byte	64
	.byte	0
	.byte	27
	.word	973
	.byte	28
	.word	1306
	.byte	64
	.byte	0
	.byte	22
	.word	.Linfo_string46
	.word	1358
	.byte	2
	.byte	56
	.byte	9
	.byte	3
	.xword	crc32_table
	.byte	27
	.word	973
	.byte	30
	.word	1306
	.hword	256
	.byte	0
	.byte	19
	.word	973
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
	.word	973
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
	.word	973
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
	.word	1496
	.byte	33
	.word	1299
	.byte	23
	.word	1506
	.byte	34
	.word	1517
	.word	.Linfo_string64
	.byte	6
	.byte	21
	.byte	34
	.word	973
	.word	.Linfo_string63
	.byte	5
	.byte	27
	.byte	23
	.word	1299
	.byte	23
	.word	1538
	.byte	21
	.word	.Linfo_string65
	.byte	8
	.byte	1
	.byte	23
	.word	1550
	.byte	33
	.word	1538
	.byte	34
	.word	973
	.word	.Linfo_string66
	.byte	7
	.byte	148
	.byte	23
	.word	973
	.byte	35
	.byte	36
	.word	.Linfo_string67
	.byte	2
	.byte	91

	.word	980
	.byte	1
	.byte	37
	.word	.Linfo_string68
	.byte	2
	.byte	91
	.word	1596
	.byte	0
	.byte	23
	.word	1601
	.byte	34
	.word	1612
	.word	.Linfo_string70
	.byte	6
	.byte	17
	.byte	34
	.word	1538
	.word	.Linfo_string69
	.byte	5
	.byte	21
	.byte	36
	.word	.Linfo_string71
	.byte	2
	.byte	101

	.word	980
	.byte	1
	.byte	37
	.word	.Linfo_string72
	.byte	2
	.byte	101
	.word	1008
	.byte	38
	.word	.Linfo_string73
	.byte	2
	.byte	103
	.word	1506
	.byte	38
	.word	.Linfo_string74
	.byte	2
	.byte	104
	.word	1506
	.byte	0
	.byte	36
	.word	.Linfo_string75
	.byte	2
	.byte	78

	.word	973
	.byte	1
	.byte	37
	.word	.Linfo_string76
	.byte	2
	.byte	78
	.word	1533
	.byte	37
	.word	.Linfo_string77
	.byte	2
	.byte	78
	.word	973
	.byte	38
	.word	.Linfo_string78
	.byte	2
	.byte	81
	.word	973
	.byte	38
	.word	.Linfo_string79
	.byte	2
	.byte	80
	.word	973
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
	.word	973
	.byte	37
	.word	.Linfo_string82
	.byte	2
	.byte	58
	.word	1566
	.byte	38
	.word	.Linfo_string79
	.byte	2
	.byte	60
	.word	973
	.byte	38
	.word	.Linfo_string83
	.byte	2
	.byte	61
	.word	973
	.byte	38
	.word	.Linfo_string78
	.byte	2
	.byte	62
	.word	973
	.byte	38
	.word	.Linfo_string84
	.byte	2
	.byte	60
	.word	973
	.byte	0
	.byte	40
	.xword	.Lfunc_begin0
	.word	.Lfunc_end0-.Lfunc_begin0
	.byte	1
	.byte	109

	.word	.Linfo_string139
	.byte	2
	.byte	127

	.word	980

	.byte	41
	.word	.Ldebug_loc0
	.word	.Linfo_string143
	.byte	2
	.byte	127
	.word	1571
	.byte	42
	.word	.Ldebug_loc1
	.word	.Linfo_string144
	.byte	2
	.byte	130
	.word	1008
	.byte	42
	.word	.Ldebug_loc2
	.word	.Linfo_string145
	.byte	2
	.byte	129
	.word	980
	.byte	14
	.word	1572
	.word	.Ldebug_ranges0
	.byte	2
	.byte	135
	.byte	8
	.byte	7
	.word	.Ldebug_loc3
	.word	1584
	.byte	0
	.byte	14
	.word	1623
	.word	.Ldebug_ranges1
	.byte	2
	.byte	141
	.byte	8
	.byte	7
	.word	.Ldebug_loc4
	.word	1635
	.byte	9
	.word	.Ldebug_loc5
	.word	1646
	.byte	9
	.word	.Ldebug_loc6
	.word	1657
	.byte	14
	.word	1669
	.word	.Ldebug_ranges2
	.byte	2
	.byte	110
	.byte	13
	.byte	7
	.word	.Ldebug_loc10
	.word	1681
	.byte	7
	.word	.Ldebug_loc9
	.word	1692
	.byte	9
	.word	.Ldebug_loc7
	.word	1703
	.byte	9
	.word	.Ldebug_loc8
	.word	1714
	.byte	10
	.word	1726
	.xword	.Ltmp16
	.word	.Ltmp37-.Ltmp16
	.byte	2
	.byte	84
	.byte	2
	.byte	43
	.ascii	"\267\233\204&"
	.word	1734
	.byte	13
	.byte	1
	.byte	92
	.word	1756
	.byte	9
	.word	.Ldebug_loc11
	.word	1767
	.byte	9
	.word	.Ldebug_loc12
	.word	1778
	.byte	9
	.word	.Ldebug_loc13
	.word	1789
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	44
	.xword	.Lfunc_begin1
	.word	.Lfunc_end1-.Lfunc_begin1
	.byte	1
	.byte	111

	.word	.Linfo_string140
	.byte	2
	.byte	160

	.word	1491

	.byte	45
	.word	.Linfo_string85
	.byte	2
	.hword	532

	.word	980
	.byte	1
	.byte	46
	.word	.Linfo_string86
	.byte	2
	.hword	532
	.word	1596
	.byte	46
	.word	.Linfo_string87
	.byte	2
	.hword	532
	.word	980
	.byte	47
	.word	.Linfo_string88
	.byte	2
	.hword	534
	.word	980
	.byte	47
	.word	.Linfo_string89
	.byte	2
	.hword	535
	.word	1596
	.byte	0
	.byte	36
	.word	.Linfo_string90
	.byte	2
	.byte	186

	.word	1545
	.byte	1
	.byte	37
	.word	.Linfo_string91
	.byte	2
	.byte	186
	.word	980
	.byte	37
	.word	.Linfo_string92
	.byte	2
	.byte	186
	.word	980
	.byte	38
	.word	.Linfo_string93
	.byte	2
	.byte	193
	.word	1571
	.byte	38
	.word	.Linfo_string94
	.byte	2
	.byte	192
	.word	973
	.byte	38
	.word	.Linfo_string95
	.byte	2
	.byte	191
	.word	973
	.byte	38
	.word	.Linfo_string96
	.byte	2
	.byte	189
	.word	1566
	.byte	38
	.word	.Linfo_string97
	.byte	2
	.byte	190
	.word	1528
	.byte	38
	.word	.Linfo_string72
	.byte	2
	.byte	188
	.word	1008
	.byte	0
	.byte	45
	.word	.Linfo_string98
	.byte	2
	.hword	429

	.word	980
	.byte	1
	.byte	46
	.word	.Linfo_string99
	.byte	2
	.hword	429
	.word	2309
	.byte	46
	.word	.Linfo_string91
	.byte	2
	.hword	429
	.word	980
	.byte	47
	.word	.Linfo_string22
	.byte	2
	.hword	433
	.word	973
	.byte	47
	.word	.Linfo_string87
	.byte	2
	.hword	432
	.word	973
	.byte	47
	.word	.Linfo_string93
	.byte	2
	.hword	431
	.word	1545
	.byte	0
	.byte	23
	.word	2314
	.byte	48
	.word	.Linfo_string102
	.byte	40
	.byte	1
	.byte	32
	.byte	25
	.word	.Linfo_string87
	.word	973
	.byte	1
	.byte	33
	.byte	0
	.byte	25
	.word	.Linfo_string100
	.word	973
	.byte	1
	.byte	34
	.byte	4
	.byte	25
	.word	.Linfo_string101
	.word	1566
	.byte	1
	.byte	35
	.byte	8
	.byte	25
	.word	.Linfo_string86
	.word	2371
	.byte	1
	.byte	36
	.byte	16
	.byte	0
	.byte	27
	.word	1596
	.byte	28
	.word	1306
	.byte	3
	.byte	0
	.byte	45
	.word	.Linfo_string103
	.byte	2
	.hword	465

	.word	980
	.byte	1
	.byte	46
	.word	.Linfo_string99
	.byte	2
	.hword	465
	.word	2309
	.byte	46
	.word	.Linfo_string104
	.byte	2
	.hword	465
	.word	882
	.byte	46
	.word	.Linfo_string105
	.byte	2
	.hword	465
	.word	980
	.byte	47
	.word	.Linfo_string87
	.byte	2
	.hword	467
	.word	980
	.byte	0
	.byte	36
	.word	.Linfo_string106
	.byte	2
	.byte	168

	.word	980
	.byte	1
	.byte	37
	.word	.Linfo_string96
	.byte	2
	.byte	168
	.word	1528
	.byte	37
	.word	.Linfo_string91
	.byte	2
	.byte	168
	.word	980
	.byte	38
	.word	.Linfo_string79
	.byte	2
	.byte	171
	.word	980
	.byte	38
	.word	.Linfo_string107
	.byte	2
	.byte	170
	.word	980
	.byte	49
	.byte	38
	.word	.Linfo_string108
	.byte	2
	.byte	173
	.word	980
	.byte	38
	.word	.Linfo_string109
	.byte	2
	.byte	173
	.word	980
	.byte	0
	.byte	49
	.byte	38
	.word	.Linfo_string110
	.byte	2
	.byte	174
	.word	980
	.byte	38
	.word	.Linfo_string111
	.byte	2
	.byte	174
	.word	980
	.byte	0
	.byte	0
	.byte	45
	.word	.Linfo_string112
	.byte	4
	.hword	539

	.word	1571
	.byte	1
	.byte	46
	.word	.Linfo_string113
	.byte	4
	.hword	539
	.word	2602
	.byte	46
	.word	.Linfo_string118
	.byte	4
	.hword	539
	.word	1555
	.byte	49
	.byte	47
	.word	.Linfo_string119
	.byte	4
	.hword	543
	.word	973
	.byte	0
	.byte	0
	.byte	34
	.word	2613
	.word	.Linfo_string117
	.byte	7
	.byte	55
	.byte	34
	.word	2624
	.word	.Linfo_string116
	.byte	8
	.byte	72
	.byte	34
	.word	2635
	.word	.Linfo_string115
	.byte	8
	.byte	16
	.byte	21
	.word	.Linfo_string114
	.byte	7
	.byte	8
	.byte	45
	.word	.Linfo_string120
	.byte	4
	.hword	687

	.word	1571
	.byte	1
	.byte	46
	.word	.Linfo_string113
	.byte	4
	.hword	687
	.word	2602
	.byte	46
	.word	.Linfo_string118
	.byte	4
	.hword	687
	.word	1555
	.byte	0
	.byte	36
	.word	.Linfo_string121
	.byte	2
	.byte	240

	.word	1566
	.byte	1
	.byte	37
	.word	.Linfo_string93
	.byte	2
	.byte	240
	.word	1545
	.byte	37
	.word	.Linfo_string22
	.byte	2
	.byte	241
	.word	973
	.byte	38
	.word	.Linfo_string122
	.byte	2
	.byte	243
	.word	1533
	.byte	0
	.byte	36
	.word	.Linfo_string123
	.byte	2
	.byte	235

	.word	1538
	.byte	1
	.byte	37
	.word	.Linfo_string93
	.byte	2
	.byte	235
	.word	1545
	.byte	0
	.byte	50
	.word	.Linfo_string124
	.byte	4
	.byte	196



	.byte	51
	.word	2763
	.byte	0
	.byte	23
	.word	2768
	.byte	52
	.byte	53
	.xword	.Lfunc_begin3
	.word	.Lfunc_end3-.Lfunc_begin3
	.byte	1
	.byte	109

	.word	2383
	.byte	7
	.word	.Ldebug_loc49
	.word	2396
	.byte	7
	.word	.Ldebug_loc50
	.word	2408
	.byte	7
	.word	.Ldebug_loc51
	.word	2420
	.byte	9
	.word	.Ldebug_loc52
	.word	2432
	.byte	6
	.word	5302
	.word	.Ldebug_ranges10
	.byte	2
	.hword	472
	.byte	15
	.byte	7
	.word	.Ldebug_loc62
	.word	5315
	.byte	7
	.word	.Ldebug_loc61
	.word	5327
	.byte	9
	.word	.Ldebug_loc57
	.word	5339
	.byte	9
	.word	.Ldebug_loc58
	.word	5351
	.byte	9
	.word	.Ldebug_loc59
	.word	5363
	.byte	9
	.word	.Ldebug_loc60
	.word	5375
	.byte	6
	.word	2134
	.word	.Ldebug_ranges11
	.byte	2
	.hword	329
	.byte	9
	.byte	7
	.word	.Ldebug_loc56
	.word	2146
	.byte	7
	.word	.Ldebug_loc55
	.word	2157
	.byte	9
	.word	.Ldebug_loc54
	.word	2223
	.byte	9
	.word	.Ldebug_loc65
	.word	2168
	.byte	9
	.word	.Ldebug_loc66
	.word	2179
	.byte	9
	.word	.Ldebug_loc67
	.word	2190
	.byte	9
	.word	.Ldebug_loc69
	.word	2201
	.byte	9
	.word	.Ldebug_loc70
	.word	2212
	.byte	14
	.word	2445
	.word	.Ldebug_ranges12
	.byte	2
	.byte	228
	.byte	7
	.byte	7
	.word	.Ldebug_loc68
	.word	2457
	.byte	7
	.word	.Ldebug_loc53
	.word	2468
	.byte	9
	.word	.Ldebug_loc63
	.word	2479
	.byte	9
	.word	.Ldebug_loc64
	.word	2490
	.byte	12
	.xword	.Ltmp154
	.word	.Ltmp156-.Ltmp154
	.byte	11
	.byte	50
	.word	2526
	.byte	11
	.byte	0
	.word	2537
	.byte	0
	.byte	0
	.byte	0
	.byte	6
	.word	2680
	.word	.Ldebug_ranges13
	.byte	2
	.hword	334
	.byte	15
	.byte	7
	.word	.Ldebug_loc74
	.word	2692
	.byte	7
	.word	.Ldebug_loc77
	.word	2703
	.byte	9
	.word	.Ldebug_loc73
	.word	2714
	.byte	14
	.word	2642
	.word	.Ldebug_ranges14
	.byte	2
	.byte	245
	.byte	8
	.byte	7
	.word	.Ldebug_loc76
	.word	2655
	.byte	7
	.word	.Ldebug_loc72
	.word	2667
	.byte	6
	.word	2550
	.word	.Ldebug_ranges15
	.byte	4
	.hword	689
	.byte	9
	.byte	7
	.word	.Ldebug_loc75
	.word	2563
	.byte	7
	.word	.Ldebug_loc71
	.word	2575
	.byte	0
	.byte	0
	.byte	0
	.byte	15
	.word	2726
	.xword	.Ltmp167
	.word	.Ltmp168-.Ltmp167
	.byte	2
	.hword	330
	.byte	14
	.byte	8
	.byte	1
	.byte	104
	.word	2738
	.byte	0
	.byte	6
	.word	5388
	.word	.Ldebug_ranges16
	.byte	2
	.hword	341
	.byte	17
	.byte	9
	.word	.Ldebug_loc80
	.word	5411
	.byte	9
	.word	.Ldebug_loc81
	.word	5423
	.byte	9
	.word	.Ldebug_loc82
	.word	5435
	.byte	9
	.word	.Ldebug_loc83
	.word	5447
	.byte	9
	.word	.Ldebug_loc84
	.word	5459
	.byte	9
	.word	.Ldebug_loc85
	.word	5471
	.byte	9
	.word	.Ldebug_loc86
	.word	5483
	.byte	9
	.word	.Ldebug_loc87
	.word	5495
	.byte	9
	.word	.Ldebug_loc88
	.word	5507
	.byte	6
	.word	2642
	.word	.Ldebug_ranges17
	.byte	2
	.hword	265
	.byte	10
	.byte	7
	.word	.Ldebug_loc79
	.word	2655
	.byte	7
	.word	.Ldebug_loc90
	.word	2667
	.byte	6
	.word	2550
	.word	.Ldebug_ranges18
	.byte	4
	.hword	689
	.byte	9
	.byte	7
	.word	.Ldebug_loc78
	.word	2563
	.byte	7
	.word	.Ldebug_loc89
	.word	2575
	.byte	0
	.byte	0
	.byte	54
	.word	.Ldebug_ranges19
	.byte	9
	.word	.Ldebug_loc91
	.word	5520
	.byte	9
	.word	.Ldebug_loc92
	.word	5532
	.byte	0
	.byte	0
	.byte	0
	.byte	15
	.word	5546
	.xword	.Ltmp219
	.word	.Ltmp232-.Ltmp219
	.byte	2
	.hword	508
	.byte	15
	.byte	7
	.word	.Ldebug_loc101
	.word	5559
	.byte	7
	.word	.Ldebug_loc100
	.word	5571
	.byte	9
	.word	.Ldebug_loc97
	.word	5583
	.byte	9
	.word	.Ldebug_loc98
	.word	5595
	.byte	9
	.word	.Ldebug_loc99
	.word	5607
	.byte	15
	.word	2134
	.xword	.Ltmp219
	.word	.Ltmp232-.Ltmp219
	.byte	2
	.hword	377
	.byte	9
	.byte	7
	.word	.Ldebug_loc96
	.word	2146
	.byte	7
	.word	.Ldebug_loc95
	.word	2157
	.byte	9
	.word	.Ldebug_loc94
	.word	2223
	.byte	9
	.word	.Ldebug_loc104
	.word	2168
	.byte	9
	.word	.Ldebug_loc105
	.word	2179
	.byte	9
	.word	.Ldebug_loc106
	.word	2190
	.byte	9
	.word	.Ldebug_loc108
	.word	2201
	.byte	9
	.word	.Ldebug_loc109
	.word	2212
	.byte	14
	.word	2445
	.word	.Ldebug_ranges20
	.byte	2
	.byte	228
	.byte	7
	.byte	7
	.word	.Ldebug_loc107
	.word	2457
	.byte	7
	.word	.Ldebug_loc93
	.word	2468
	.byte	9
	.word	.Ldebug_loc102
	.word	2479
	.byte	9
	.word	.Ldebug_loc103
	.word	2490
	.byte	12
	.xword	.Ltmp221
	.word	.Ltmp223-.Ltmp221
	.byte	11
	.byte	50
	.word	2526
	.byte	13
	.byte	1
	.byte	90
	.word	2537
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	15
	.word	5620
	.xword	.Ltmp232
	.word	.Ltmp242-.Ltmp232
	.byte	2
	.hword	502
	.byte	15
	.byte	7
	.word	.Ldebug_loc119
	.word	5633
	.byte	7
	.word	.Ldebug_loc118
	.word	5645
	.byte	7
	.word	.Ldebug_loc117
	.word	5657
	.byte	9
	.word	.Ldebug_loc114
	.word	5669
	.byte	9
	.word	.Ldebug_loc115
	.word	5681
	.byte	9
	.word	.Ldebug_loc116
	.word	5693
	.byte	15
	.word	2134
	.xword	.Ltmp232
	.word	.Ltmp242-.Ltmp232
	.byte	2
	.hword	358
	.byte	9
	.byte	7
	.word	.Ldebug_loc113
	.word	2146
	.byte	7
	.word	.Ldebug_loc112
	.word	2157
	.byte	9
	.word	.Ldebug_loc111
	.word	2223
	.byte	9
	.word	.Ldebug_loc122
	.word	2168
	.byte	9
	.word	.Ldebug_loc123
	.word	2179
	.byte	9
	.word	.Ldebug_loc124
	.word	2190
	.byte	9
	.word	.Ldebug_loc126
	.word	2201
	.byte	9
	.word	.Ldebug_loc127
	.word	2212
	.byte	14
	.word	2445
	.word	.Ldebug_ranges21
	.byte	2
	.byte	228
	.byte	7
	.byte	7
	.word	.Ldebug_loc125
	.word	2457
	.byte	7
	.word	.Ldebug_loc110
	.word	2468
	.byte	9
	.word	.Ldebug_loc120
	.word	2479
	.byte	9
	.word	.Ldebug_loc121
	.word	2490
	.byte	12
	.xword	.Ltmp234
	.word	.Ltmp236-.Ltmp234
	.byte	11
	.byte	50
	.word	2526
	.byte	13
	.byte	1
	.byte	90
	.word	2537
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	15
	.word	5620
	.xword	.Ltmp242
	.word	.Ltmp252-.Ltmp242
	.byte	2
	.hword	478
	.byte	15
	.byte	7
	.word	.Ldebug_loc137
	.word	5633
	.byte	7
	.word	.Ldebug_loc136
	.word	5645
	.byte	7
	.word	.Ldebug_loc135
	.word	5657
	.byte	9
	.word	.Ldebug_loc132
	.word	5669
	.byte	9
	.word	.Ldebug_loc133
	.word	5681
	.byte	9
	.word	.Ldebug_loc134
	.word	5693
	.byte	15
	.word	2134
	.xword	.Ltmp242
	.word	.Ltmp252-.Ltmp242
	.byte	2
	.hword	358
	.byte	9
	.byte	7
	.word	.Ldebug_loc131
	.word	2146
	.byte	7
	.word	.Ldebug_loc130
	.word	2157
	.byte	9
	.word	.Ldebug_loc129
	.word	2223
	.byte	9
	.word	.Ldebug_loc140
	.word	2168
	.byte	9
	.word	.Ldebug_loc141
	.word	2179
	.byte	9
	.word	.Ldebug_loc142
	.word	2190
	.byte	9
	.word	.Ldebug_loc144
	.word	2201
	.byte	9
	.word	.Ldebug_loc145
	.word	2212
	.byte	14
	.word	2445
	.word	.Ldebug_ranges22
	.byte	2
	.byte	228
	.byte	7
	.byte	7
	.word	.Ldebug_loc143
	.word	2457
	.byte	7
	.word	.Ldebug_loc128
	.word	2468
	.byte	9
	.word	.Ldebug_loc138
	.word	2479
	.byte	9
	.word	.Ldebug_loc139
	.word	2490
	.byte	12
	.xword	.Ltmp244
	.word	.Ltmp246-.Ltmp244
	.byte	11
	.byte	50
	.word	2526
	.byte	13
	.byte	1
	.byte	90
	.word	2537
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	15
	.word	5620
	.xword	.Ltmp254
	.word	.Ltmp267-.Ltmp254
	.byte	2
	.hword	484
	.byte	15
	.byte	7
	.word	.Ldebug_loc155
	.word	5633
	.byte	7
	.word	.Ldebug_loc154
	.word	5645
	.byte	7
	.word	.Ldebug_loc153
	.word	5657
	.byte	9
	.word	.Ldebug_loc150
	.word	5669
	.byte	9
	.word	.Ldebug_loc151
	.word	5681
	.byte	9
	.word	.Ldebug_loc152
	.word	5693
	.byte	15
	.word	2134
	.xword	.Ltmp254
	.word	.Ltmp267-.Ltmp254
	.byte	2
	.hword	358
	.byte	9
	.byte	7
	.word	.Ldebug_loc149
	.word	2146
	.byte	7
	.word	.Ldebug_loc148
	.word	2157
	.byte	9
	.word	.Ldebug_loc147
	.word	2223
	.byte	9
	.word	.Ldebug_loc158
	.word	2168
	.byte	9
	.word	.Ldebug_loc159
	.word	2179
	.byte	9
	.word	.Ldebug_loc160
	.word	2190
	.byte	9
	.word	.Ldebug_loc162
	.word	2201
	.byte	9
	.word	.Ldebug_loc163
	.word	2212
	.byte	14
	.word	2445
	.word	.Ldebug_ranges23
	.byte	2
	.byte	228
	.byte	7
	.byte	7
	.word	.Ldebug_loc161
	.word	2457
	.byte	7
	.word	.Ldebug_loc146
	.word	2468
	.byte	9
	.word	.Ldebug_loc156
	.word	2479
	.byte	9
	.word	.Ldebug_loc157
	.word	2490
	.byte	12
	.xword	.Ltmp256
	.word	.Ltmp258-.Ltmp256
	.byte	11
	.byte	50
	.word	2526
	.byte	13
	.byte	1
	.byte	90
	.word	2537
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	15
	.word	5620
	.xword	.Ltmp267
	.word	.Ltmp280-.Ltmp267
	.byte	2
	.hword	490
	.byte	15
	.byte	7
	.word	.Ldebug_loc173
	.word	5633
	.byte	7
	.word	.Ldebug_loc172
	.word	5645
	.byte	7
	.word	.Ldebug_loc171
	.word	5657
	.byte	9
	.word	.Ldebug_loc168
	.word	5669
	.byte	9
	.word	.Ldebug_loc169
	.word	5681
	.byte	9
	.word	.Ldebug_loc170
	.word	5693
	.byte	15
	.word	2134
	.xword	.Ltmp267
	.word	.Ltmp280-.Ltmp267
	.byte	2
	.hword	358
	.byte	9
	.byte	7
	.word	.Ldebug_loc167
	.word	2146
	.byte	7
	.word	.Ldebug_loc166
	.word	2157
	.byte	9
	.word	.Ldebug_loc165
	.word	2223
	.byte	9
	.word	.Ldebug_loc176
	.word	2168
	.byte	9
	.word	.Ldebug_loc177
	.word	2179
	.byte	9
	.word	.Ldebug_loc178
	.word	2190
	.byte	9
	.word	.Ldebug_loc180
	.word	2201
	.byte	9
	.word	.Ldebug_loc181
	.word	2212
	.byte	14
	.word	2445
	.word	.Ldebug_ranges24
	.byte	2
	.byte	228
	.byte	7
	.byte	7
	.word	.Ldebug_loc179
	.word	2457
	.byte	7
	.word	.Ldebug_loc164
	.word	2468
	.byte	9
	.word	.Ldebug_loc174
	.word	2479
	.byte	9
	.word	.Ldebug_loc175
	.word	2490
	.byte	12
	.xword	.Ltmp269
	.word	.Ltmp271-.Ltmp269
	.byte	11
	.byte	50
	.word	2526
	.byte	13
	.byte	1
	.byte	90
	.word	2537
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	15
	.word	5620
	.xword	.Ltmp280
	.word	.Ltmp293-.Ltmp280
	.byte	2
	.hword	496
	.byte	15
	.byte	7
	.word	.Ldebug_loc191
	.word	5633
	.byte	7
	.word	.Ldebug_loc190
	.word	5645
	.byte	7
	.word	.Ldebug_loc189
	.word	5657
	.byte	9
	.word	.Ldebug_loc186
	.word	5669
	.byte	9
	.word	.Ldebug_loc187
	.word	5681
	.byte	9
	.word	.Ldebug_loc188
	.word	5693
	.byte	15
	.word	2134
	.xword	.Ltmp280
	.word	.Ltmp293-.Ltmp280
	.byte	2
	.hword	358
	.byte	9
	.byte	7
	.word	.Ldebug_loc185
	.word	2146
	.byte	7
	.word	.Ldebug_loc184
	.word	2157
	.byte	9
	.word	.Ldebug_loc183
	.word	2223
	.byte	9
	.word	.Ldebug_loc194
	.word	2168
	.byte	9
	.word	.Ldebug_loc195
	.word	2179
	.byte	9
	.word	.Ldebug_loc196
	.word	2190
	.byte	9
	.word	.Ldebug_loc198
	.word	2201
	.byte	9
	.word	.Ldebug_loc199
	.word	2212
	.byte	14
	.word	2445
	.word	.Ldebug_ranges25
	.byte	2
	.byte	228
	.byte	7
	.byte	7
	.word	.Ldebug_loc197
	.word	2457
	.byte	7
	.word	.Ldebug_loc182
	.word	2468
	.byte	9
	.word	.Ldebug_loc192
	.word	2479
	.byte	9
	.word	.Ldebug_loc193
	.word	2490
	.byte	12
	.xword	.Ltmp282
	.word	.Ltmp284-.Ltmp282
	.byte	11
	.byte	50
	.word	2526
	.byte	13
	.byte	1
	.byte	90
	.word	2537
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	15
	.word	2235
	.xword	.Ltmp293
	.word	.Ltmp305-.Ltmp293
	.byte	2
	.hword	520
	.byte	15
	.byte	7
	.word	.Ldebug_loc208
	.word	2248
	.byte	7
	.word	.Ldebug_loc207
	.word	2260
	.byte	9
	.word	.Ldebug_loc204
	.word	2272
	.byte	9
	.word	.Ldebug_loc205
	.word	2284
	.byte	9
	.word	.Ldebug_loc206
	.word	2296
	.byte	15
	.word	2134
	.xword	.Ltmp293
	.word	.Ltmp305-.Ltmp293
	.byte	2
	.hword	435
	.byte	9
	.byte	7
	.word	.Ldebug_loc203
	.word	2146
	.byte	7
	.word	.Ldebug_loc202
	.word	2157
	.byte	9
	.word	.Ldebug_loc201
	.word	2223
	.byte	9
	.word	.Ldebug_loc211
	.word	2168
	.byte	9
	.word	.Ldebug_loc212
	.word	2179
	.byte	9
	.word	.Ldebug_loc213
	.word	2190
	.byte	9
	.word	.Ldebug_loc215
	.word	2201
	.byte	9
	.word	.Ldebug_loc216
	.word	2212
	.byte	14
	.word	2445
	.word	.Ldebug_ranges26
	.byte	2
	.byte	228
	.byte	7
	.byte	7
	.word	.Ldebug_loc214
	.word	2457
	.byte	7
	.word	.Ldebug_loc200
	.word	2468
	.byte	9
	.word	.Ldebug_loc209
	.word	2479
	.byte	9
	.word	.Ldebug_loc210
	.word	2490
	.byte	12
	.xword	.Ltmp295
	.word	.Ltmp297-.Ltmp295
	.byte	11
	.byte	50
	.word	2526
	.byte	13
	.byte	1
	.byte	90
	.word	2537
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	17
	.word	2750
	.xword	.Ltmp217
	.byte	18
	.byte	1
	.byte	80
	.byte	2
	.byte	133
	.byte	0
	.byte	0
	.byte	17
	.word	2750
	.xword	.Ltmp319
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

	.word	.Linfo_string142
	.byte	2
	.hword	447

	.word	980
	.byte	56
	.byte	1
	.byte	80
	.word	.Linfo_string101
	.byte	2
	.hword	447
	.word	2309
	.byte	56
	.byte	1
	.byte	81
	.word	.Linfo_string86
	.byte	2
	.hword	447
	.word	1596
	.byte	56
	.byte	1
	.byte	82
	.word	.Linfo_string87
	.byte	2
	.hword	447
	.word	980
	.byte	5
	.word	.Ldebug_loc217
	.word	.Linfo_string79
	.byte	2
	.hword	449
	.word	980
	.byte	5
	.word	.Ldebug_loc218
	.word	.Linfo_string148
	.byte	2
	.hword	450
	.word	1506
	.byte	5
	.word	.Ldebug_loc219
	.word	.Linfo_string149
	.byte	2
	.hword	450
	.word	1506
	.byte	12
	.xword	.Ltmp325
	.word	.Ltmp365-.Ltmp325
	.byte	5
	.word	.Ldebug_loc220
	.word	.Linfo_string150
	.byte	2
	.hword	453
	.word	1566
	.byte	12
	.xword	.Ltmp331
	.word	.Ltmp363-.Ltmp331
	.byte	5
	.word	.Ldebug_loc221
	.word	.Linfo_string151
	.byte	2
	.hword	456
	.word	1601
	.byte	0
	.byte	0
	.byte	0
	.byte	57
	.word	.Linfo_string125
	.byte	1
	.byte	48

	.word	980


	.byte	45
	.word	.Linfo_string126
	.byte	2
	.hword	321

	.word	980
	.byte	1
	.byte	46
	.word	.Linfo_string99
	.byte	2
	.hword	321
	.word	2309
	.byte	46
	.word	.Linfo_string91
	.byte	2
	.hword	321
	.word	980
	.byte	47
	.word	.Linfo_string22
	.byte	2
	.hword	326
	.word	973
	.byte	47
	.word	.Linfo_string87
	.byte	2
	.hword	325
	.word	973
	.byte	47
	.word	.Linfo_string93
	.byte	2
	.hword	324
	.word	1545
	.byte	47
	.word	.Linfo_string127
	.byte	2
	.hword	323
	.word	2314
	.byte	0
	.byte	36
	.word	.Linfo_string128
	.byte	2
	.byte	254

	.word	1566
	.byte	1
	.byte	37
	.word	.Linfo_string127
	.byte	2
	.byte	254
	.word	2309
	.byte	47
	.word	.Linfo_string129
	.byte	2
	.hword	260
	.word	980
	.byte	47
	.word	.Linfo_string130
	.byte	2
	.hword	259
	.word	980
	.byte	47
	.word	.Linfo_string131
	.byte	2
	.hword	258
	.word	980
	.byte	47
	.word	.Linfo_string132
	.byte	2
	.hword	257
	.word	980
	.byte	47
	.word	.Linfo_string133
	.byte	2
	.hword	257
	.word	980
	.byte	47
	.word	.Linfo_string84
	.byte	2
	.hword	257
	.word	980
	.byte	47
	.word	.Linfo_string79
	.byte	2
	.hword	257
	.word	980
	.byte	47
	.word	.Linfo_string134
	.byte	2
	.hword	256
	.word	1566
	.byte	47
	.word	.Linfo_string135
	.byte	2
	.hword	256
	.word	1566
	.byte	49
	.byte	47
	.word	.Linfo_string91
	.byte	2
	.hword	272
	.word	980
	.byte	47
	.word	.Linfo_string136
	.byte	2
	.hword	273
	.word	980
	.byte	0
	.byte	0
	.byte	45
	.word	.Linfo_string137
	.byte	2
	.hword	370

	.word	980
	.byte	1
	.byte	46
	.word	.Linfo_string99
	.byte	2
	.hword	370
	.word	2309
	.byte	46
	.word	.Linfo_string91
	.byte	2
	.hword	370
	.word	980
	.byte	47
	.word	.Linfo_string22
	.byte	2
	.hword	374
	.word	973
	.byte	47
	.word	.Linfo_string87
	.byte	2
	.hword	373
	.word	973
	.byte	47
	.word	.Linfo_string93
	.byte	2
	.hword	372
	.word	1545
	.byte	0
	.byte	45
	.word	.Linfo_string138
	.byte	2
	.hword	351

	.word	980
	.byte	1
	.byte	46
	.word	.Linfo_string99
	.byte	2
	.hword	351
	.word	2309
	.byte	46
	.word	.Linfo_string91
	.byte	2
	.hword	351
	.word	980
	.byte	46
	.word	.Linfo_string104
	.byte	2
	.hword	351
	.word	1371
	.byte	47
	.word	.Linfo_string22
	.byte	2
	.hword	355
	.word	973
	.byte	47
	.word	.Linfo_string87
	.byte	2
	.hword	354
	.word	973
	.byte	47
	.word	.Linfo_string93
	.byte	2
	.hword	353
	.word	1545
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
	.xword	.Ltmp79-.Lfunc_begin0
	.xword	.Ltmp81-.Lfunc_begin0
	.xword	.Ltmp82-.Lfunc_begin0
	.xword	.Ltmp95-.Lfunc_begin0
	.xword	0
	.xword	0
.Ldebug_ranges4:
	.xword	.Ltmp100-.Lfunc_begin0
	.xword	.Ltmp120-.Lfunc_begin0
	.xword	.Ltmp121-.Lfunc_begin0
	.xword	.Ltmp122-.Lfunc_begin0
	.xword	.Ltmp144-.Lfunc_begin0
	.xword	.Ltmp145-.Lfunc_begin0
	.xword	0
	.xword	0
.Ldebug_ranges5:
	.xword	.Ltmp100-.Lfunc_begin0
	.xword	.Ltmp120-.Lfunc_begin0
	.xword	.Ltmp121-.Lfunc_begin0
	.xword	.Ltmp122-.Lfunc_begin0
	.xword	.Ltmp144-.Lfunc_begin0
	.xword	.Ltmp145-.Lfunc_begin0
	.xword	0
	.xword	0
.Ldebug_ranges6:
	.xword	.Ltmp100-.Lfunc_begin0
	.xword	.Ltmp111-.Lfunc_begin0
	.xword	.Ltmp112-.Lfunc_begin0
	.xword	.Ltmp114-.Lfunc_begin0
	.xword	0
	.xword	0
.Ldebug_ranges7:
	.xword	.Ltmp111-.Lfunc_begin0
	.xword	.Ltmp112-.Lfunc_begin0
	.xword	.Ltmp116-.Lfunc_begin0
	.xword	.Ltmp120-.Lfunc_begin0
	.xword	0
	.xword	0
.Ldebug_ranges8:
	.xword	.Ltmp111-.Lfunc_begin0
	.xword	.Ltmp112-.Lfunc_begin0
	.xword	.Ltmp116-.Lfunc_begin0
	.xword	.Ltmp117-.Lfunc_begin0
	.xword	0
	.xword	0
.Ldebug_ranges9:
	.xword	.Ltmp111-.Lfunc_begin0
	.xword	.Ltmp112-.Lfunc_begin0
	.xword	.Ltmp116-.Lfunc_begin0
	.xword	.Ltmp117-.Lfunc_begin0
	.xword	0
	.xword	0
.Ldebug_ranges10:
	.xword	.Ltmp152-.Lfunc_begin0
	.xword	.Ltmp218-.Lfunc_begin0
	.xword	.Ltmp318-.Lfunc_begin0
	.xword	.Ltmp321-.Lfunc_begin0
	.xword	0
	.xword	0
.Ldebug_ranges11:
	.xword	.Ltmp152-.Lfunc_begin0
	.xword	.Ltmp164-.Lfunc_begin0
	.xword	.Ltmp165-.Lfunc_begin0
	.xword	.Ltmp167-.Lfunc_begin0
	.xword	0
	.xword	0
.Ldebug_ranges12:
	.xword	.Ltmp153-.Lfunc_begin0
	.xword	.Ltmp156-.Lfunc_begin0
	.xword	.Ltmp157-.Lfunc_begin0
	.xword	.Ltmp162-.Lfunc_begin0
	.xword	0
	.xword	0
.Ldebug_ranges13:
	.xword	.Ltmp164-.Lfunc_begin0
	.xword	.Ltmp165-.Lfunc_begin0
	.xword	.Ltmp170-.Lfunc_begin0
	.xword	.Ltmp174-.Lfunc_begin0
	.xword	0
	.xword	0
.Ldebug_ranges14:
	.xword	.Ltmp164-.Lfunc_begin0
	.xword	.Ltmp165-.Lfunc_begin0
	.xword	.Ltmp170-.Lfunc_begin0
	.xword	.Ltmp171-.Lfunc_begin0
	.xword	0
	.xword	0
.Ldebug_ranges15:
	.xword	.Ltmp164-.Lfunc_begin0
	.xword	.Ltmp165-.Lfunc_begin0
	.xword	.Ltmp170-.Lfunc_begin0
	.xword	.Ltmp171-.Lfunc_begin0
	.xword	0
	.xword	0
.Ldebug_ranges16:
	.xword	.Ltmp174-.Lfunc_begin0
	.xword	.Ltmp180-.Lfunc_begin0
	.xword	.Ltmp182-.Lfunc_begin0
	.xword	.Ltmp216-.Lfunc_begin0
	.xword	0
	.xword	0
.Ldebug_ranges17:
	.xword	.Ltmp175-.Lfunc_begin0
	.xword	.Ltmp176-.Lfunc_begin0
	.xword	.Ltmp177-.Lfunc_begin0
	.xword	.Ltmp178-.Lfunc_begin0
	.xword	0
	.xword	0
.Ldebug_ranges18:
	.xword	.Ltmp175-.Lfunc_begin0
	.xword	.Ltmp176-.Lfunc_begin0
	.xword	.Ltmp177-.Lfunc_begin0
	.xword	.Ltmp178-.Lfunc_begin0
	.xword	0
	.xword	0
.Ldebug_ranges19:
	.xword	.Ltmp186-.Lfunc_begin0
	.xword	.Ltmp189-.Lfunc_begin0
	.xword	.Ltmp191-.Lfunc_begin0
	.xword	.Ltmp192-.Lfunc_begin0
	.xword	.Ltmp193-.Lfunc_begin0
	.xword	.Ltmp212-.Lfunc_begin0
	.xword	0
	.xword	0
.Ldebug_ranges20:
	.xword	.Ltmp220-.Lfunc_begin0
	.xword	.Ltmp223-.Lfunc_begin0
	.xword	.Ltmp224-.Lfunc_begin0
	.xword	.Ltmp229-.Lfunc_begin0
	.xword	0
	.xword	0
.Ldebug_ranges21:
	.xword	.Ltmp233-.Lfunc_begin0
	.xword	.Ltmp236-.Lfunc_begin0
	.xword	.Ltmp237-.Lfunc_begin0
	.xword	.Ltmp242-.Lfunc_begin0
	.xword	0
	.xword	0
.Ldebug_ranges22:
	.xword	.Ltmp243-.Lfunc_begin0
	.xword	.Ltmp246-.Lfunc_begin0
	.xword	.Ltmp247-.Lfunc_begin0
	.xword	.Ltmp252-.Lfunc_begin0
	.xword	0
	.xword	0
.Ldebug_ranges23:
	.xword	.Ltmp255-.Lfunc_begin0
	.xword	.Ltmp258-.Lfunc_begin0
	.xword	.Ltmp259-.Lfunc_begin0
	.xword	.Ltmp264-.Lfunc_begin0
	.xword	0
	.xword	0
.Ldebug_ranges24:
	.xword	.Ltmp268-.Lfunc_begin0
	.xword	.Ltmp271-.Lfunc_begin0
	.xword	.Ltmp272-.Lfunc_begin0
	.xword	.Ltmp277-.Lfunc_begin0
	.xword	0
	.xword	0
.Ldebug_ranges25:
	.xword	.Ltmp281-.Lfunc_begin0
	.xword	.Ltmp284-.Lfunc_begin0
	.xword	.Ltmp285-.Lfunc_begin0
	.xword	.Ltmp290-.Lfunc_begin0
	.xword	0
	.xword	0
.Ldebug_ranges26:
	.xword	.Ltmp294-.Lfunc_begin0
	.xword	.Ltmp297-.Lfunc_begin0
	.xword	.Ltmp298-.Lfunc_begin0
	.xword	.Ltmp303-.Lfunc_begin0
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
	.asciz	"rkf_wf_a2_repair"
.Linfo_string86:
	.asciz	"wf_table"
.Linfo_string87:
	.asciz	"frame_num"
.Linfo_string88:
	.asciz	"f"
.Linfo_string89:
	.asciz	"table"
.Linfo_string90:
	.asciz	"get_wf_buf"
.Linfo_string91:
	.asciz	"temp"
.Linfo_string92:
	.asciz	"mode"
.Linfo_string93:
	.asciz	"pbuf"
.Linfo_string94:
	.asciz	"pt"
.Linfo_string95:
	.asciz	"wf_offset"
.Linfo_string96:
	.asciz	"list"
.Linfo_string97:
	.asciz	"templist"
.Linfo_string98:
	.asciz	"parse_wf_a2"
.Linfo_string99:
	.asciz	"output"
.Linfo_string100:
	.asciz	"repair_frame_num"
.Linfo_string101:
	.asciz	"data"
.Linfo_string102:
	.asciz	"epd_lut_data"
.Linfo_string103:
	.asciz	"parse_wf_data"
.Linfo_string104:
	.asciz	"lut_type"
.Linfo_string105:
	.asciz	"temperture"
.Linfo_string106:
	.asciz	"get_wf_level"
.Linfo_string107:
	.asciz	"level"
.Linfo_string108:
	.asciz	"__UNIQUE_ID___y347"
.Linfo_string109:
	.asciz	"__UNIQUE_ID___x346"
.Linfo_string110:
	.asciz	"__UNIQUE_ID___y349"
.Linfo_string111:
	.asciz	"__UNIQUE_ID___x348"
.Linfo_string112:
	.asciz	"kmalloc"
.Linfo_string113:
	.asciz	"size"
.Linfo_string114:
	.asciz	"unsigned long"
.Linfo_string115:
	.asciz	"__kernel_ulong_t"
.Linfo_string116:
	.asciz	"__kernel_size_t"
.Linfo_string117:
	.asciz	"size_t"
.Linfo_string118:
	.asciz	"flags"
.Linfo_string119:
	.asciz	"index"
.Linfo_string120:
	.asciz	"kzalloc"
.Linfo_string121:
	.asciz	"decode_wf_data"
.Linfo_string122:
	.asciz	"out"
.Linfo_string123:
	.asciz	"get_wf_frm_num"
.Linfo_string124:
	.asciz	"kfree"
.Linfo_string125:
	.asciz	"epd_overlay_lut"
.Linfo_string126:
	.asciz	"parse_wf_reset"
.Linfo_string127:
	.asciz	"input"
.Linfo_string128:
	.asciz	"decode_wf_reset"
.Linfo_string129:
	.asciz	"len"
.Linfo_string130:
	.asciz	"num"
.Linfo_string131:
	.asciz	"temp_data"
.Linfo_string132:
	.asciz	"h"
.Linfo_string133:
	.asciz	"k"
.Linfo_string134:
	.asciz	"odata"
.Linfo_string135:
	.asciz	"idata"
.Linfo_string136:
	.asciz	"temp1"
.Linfo_string137:
	.asciz	"parse_wf_gray2"
.Linfo_string138:
	.asciz	"parse_wf_gray16"
.Linfo_string139:
	.asciz	"rkf_wf_input"
.Linfo_string140:
	.asciz	"rkf_wf_get_version"
.Linfo_string141:
	.asciz	"rkf_wf_get_lut"
.Linfo_string142:
	.asciz	"rkf_lut_init_wf_table"
.Linfo_string143:
	.asciz	"waveform_file"
.Linfo_string144:
	.asciz	"wf"
.Linfo_string145:
	.asciz	"ret"
.Linfo_string146:
	.asciz	"pic"
.Linfo_string147:
	.asciz	"overlay_lut"
.Linfo_string148:
	.asciz	"old_gray"
.Linfo_string149:
	.asciz	"new_gray"
.Linfo_string150:
	.asciz	"lut_data"
.Linfo_string151:
	.asciz	"pix0"
	.ident	"Android (8508608, based on r450784e) clang version 14.0.7 (https://android.googlesource.com/toolchain/llvm-project 4c603efb0cca074e9238af8b4106c30add4418f6)"
	.section	".note.GNU-stack","",@progbits
	.addrsig
	.section	.debug_line,"",@progbits
.Lline_table_start0:
