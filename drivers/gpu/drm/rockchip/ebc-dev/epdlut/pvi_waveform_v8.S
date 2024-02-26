// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 Rockchip Electronics Co. Ltd.
 *
 * Author: Zorro Liu <zorro.liu@rock-chips.com>
 */

	.text
	.file	"pvi_waveform.c"
	.file	1 "/home/lyx/rk-android14/kernel-6.1" "./include/uapi/asm-generic/int-ll64.h"
	.file	2 "/home/lyx/rk-android14/kernel-6.1" "./include/asm-generic/int-ll64.h"
	.file	3 "/home/lyx/rk-android14/kernel-6.1" "drivers/gpu/drm/rockchip/ebc-dev/epdlut/pvi_waveform.c"
	.file	4 "/home/lyx/rk-android14/kernel-6.1" "drivers/gpu/drm/rockchip/ebc-dev/epdlut/epd_lut.h"
	.file	5 "/home/lyx/rk-android14/kernel-6.1" "./include/linux/stddef.h"
	.globl	pvi_wf_get_lut
	.p2align	2
	.type	pvi_wf_get_lut,@function
pvi_wf_get_lut:
.Lfunc_begin0:
	.loc	3 243 0
	.cfi_sections .debug_frame
	.cfi_startproc
	str	x30, [x18], #8
	.cfi_escape 0x16, 0x12, 0x02, 0x82, 0x78
	stp	x29, x30, [sp, #-96]!
	str	x27, [sp, #16]
	mov	x29, sp
	stp	x26, x25, [sp, #32]
	stp	x24, x23, [sp, #48]
	stp	x22, x21, [sp, #64]
	stp	x20, x19, [sp, #80]
	.cfi_def_cfa w29, 96
	.cfi_offset w19, -8
	.cfi_offset w20, -16
	.cfi_offset w21, -24
	.cfi_offset w22, -32
	.cfi_offset w23, -40
	.cfi_offset w24, -48
	.cfi_offset w25, -56
	.cfi_offset w26, -64
	.cfi_offset w27, -80
	.cfi_offset w30, -88
	.cfi_offset w29, -96
.Ltmp0:
	.loc	3 251 7 prologue_end
	adrp	x24, global_waveform
	ldr	x8, [x24, :lo12:global_waveform]
.Ltmp1:
	.loc	3 251 6 is_stmt 0
	cbz	x8, .LBB0_10
.Ltmp2:
	.loc	3 0 6
	mov	w19, w2
.Ltmp3:
	mov	w21, w1
.Ltmp4:
	mov	x20, x0
.Ltmp5:
	.loc	3 254 14 is_stmt 1
	cbnz	x0, .LBB0_3
.Ltmp6:
	.loc	3 0 14 is_stmt 0
	mov	w9, #16
	.loc	3 254 18
	ldr	x9, [x9]
.Ltmp7:
	.loc	3 254 6
	cbz	x9, .LBB0_11
.Ltmp8:
.LBB0_3:
	.loc	3 259 28 is_stmt 1
	adrp	x23, pvi_wf_get_lut.sftemp
	.loc	3 259 19 is_stmt 0
	mov	w9, #21846
	movk	w9, #21845, lsl #16
	adrp	x26, pvi_wf_get_lut.stype
	.loc	3 259 28
	ldrsw	x10, [x23, :lo12:pvi_wf_get_lut.sftemp]
	.loc	3 259 19
	smull	x11, w19, w9
	ldr	w13, [x26, :lo12:pvi_wf_get_lut.stype]
	.loc	3 259 35
	mul	x9, x10, x9
	.loc	3 259 19
	lsr	x10, x11, #63
	lsr	x11, x11, #32
	.loc	3 259 35
	lsr	x12, x9, #63
	lsr	x9, x9, #32
	.loc	3 259 19
	add	w25, w11, w10
	.loc	3 259 35
	add	w9, w9, w12
	.loc	3 259 41
	cmp	w25, w9
	ccmp	w13, w21, #0, eq
	b.eq	.LBB0_25
.Ltmp9:
	.loc	3 262 11 is_stmt 1
	adrp	x10, need_pic
	.loc	3 264 6
	cmp	w21, #10
	.loc	3 262 11
	str	w3, [x10, :lo12:need_pic]
	.loc	3 264 6
	b.eq	.LBB0_24
.Ltmp10:
	.loc	3 266 18
	ldr	x10, [x20, #16]
	.loc	3 266 16 is_stmt 0
	adrp	x27, waveformdata
.Ltmp11:
	.loc	3 201 34 is_stmt 1
	ldrb	w9, [x8, #38]
.Ltmp12:
	.loc	3 266 16
	str	x10, [x27, :lo12:waveformdata]
.Ltmp13:
	.loc	3 205 2
	cbz	w9, .LBB0_12
.Ltmp14:
	.loc	3 0 2 is_stmt 0
	mov	x10, xzr
	.loc	3 205 2
	add	x8, x8, #48
.Ltmp15:
.LBB0_7:
	.loc	3 206 21 is_stmt 1
	ldrb	w11, [x8, x10]
.Ltmp16:
	.loc	3 206 7 is_stmt 0
	cmp	w11, w19
	b.ge	.LBB0_13
.Ltmp17:
	.loc	3 205 29 is_stmt 1
	add	x10, x10, #1
.Ltmp18:
	.loc	3 205 2 is_stmt 0
	cmp	x9, x10
	b.ne	.LBB0_7
.Ltmp19:
	.loc	3 0 2
	mov	w8, #-1
	mov	w10, w9
	b	.LBB0_14
.Ltmp20:
.LBB0_10:
	mov	w0, #-19
.Ltmp21:
	b	.LBB0_35
.Ltmp22:
.LBB0_11:
	mov	w0, #-22
	b	.LBB0_35
.Ltmp23:
.LBB0_12:
	mov	w10, wzr
	mov	w8, #-1
	b	.LBB0_14
.Ltmp24:
.LBB0_13:
	mov	w8, w10
.Ltmp25:
.LBB0_14:
	.loc	3 268 16 is_stmt 1
	mov	w0, w21
	sub	w11, w10, #1
.Ltmp26:
	.loc	3 212 6
	cmp	w10, w9
	csel	w22, w11, w8, eq
.Ltmp27:
	.loc	3 268 16
	bl	get_wf_mode_index
.Ltmp28:
	and	w0, w0, #0xff
.Ltmp29:
	.loc	3 269 15
	mov	w1, w22
	bl	get_wf_frm_num
.Ltmp30:
	.loc	3 270 7
	tbnz	w0, #31, .LBB0_36
.Ltmp31:
	.loc	3 274 21
	ldrb	w8, [x20, #1]
	.loc	3 278 18
	ldr	x9, [x20, #32]
	.loc	3 275 21
	orr	w8, w0, w8, lsl #8
	str	w8, [x20]
.Ltmp32:
	.loc	3 201 17
	ldr	x10, [x24, :lo12:global_waveform]
.Ltmp33:
	.loc	3 276 9
	str	w21, [x26, :lo12:pvi_wf_get_lut.stype]
	.loc	3 278 16
	str	x9, [x27, :lo12:waveformdata]
.Ltmp34:
	.loc	3 201 34
	ldrb	w8, [x10, #38]
.Ltmp35:
	.loc	3 205 2
	cbz	w8, .LBB0_20
.Ltmp36:
	.loc	3 0 2 is_stmt 0
	mov	x9, xzr
	.loc	3 205 2
	add	x10, x10, #48
.Ltmp37:
.LBB0_17:
	.loc	3 206 21 is_stmt 1
	ldrb	w11, [x10, x9]
.Ltmp38:
	.loc	3 206 7 is_stmt 0
	cmp	w11, w19
	b.ge	.LBB0_21
.Ltmp39:
	.loc	3 205 29 is_stmt 1
	add	x9, x9, #1
.Ltmp40:
	.loc	3 205 2 is_stmt 0
	cmp	x8, x9
	b.ne	.LBB0_17
.Ltmp41:
	.loc	3 0 2
	mov	w10, #-1
	mov	w9, w8
	b	.LBB0_22
.Ltmp42:
.LBB0_20:
	mov	w9, wzr
	mov	w10, #-1
	b	.LBB0_22
.Ltmp43:
.LBB0_21:
	mov	w10, w9
.Ltmp44:
.LBB0_22:
	.loc	3 280 16 is_stmt 1
	mov	w0, #7
.Ltmp45:
	.loc	3 0 16 is_stmt 0
	sub	w11, w9, #1
.Ltmp46:
	.loc	3 212 6 is_stmt 1
	cmp	w9, w8
	csel	w21, w11, w10, eq
.Ltmp47:
	.loc	3 280 16
	bl	get_wf_mode_index
.Ltmp48:
	and	w0, w0, #0xff
.Ltmp49:
	.loc	3 281 15
	mov	w1, w21
	bl	get_wf_frm_num
.Ltmp50:
	.loc	3 282 7
	tbnz	w0, #31, .LBB0_36
.Ltmp51:
	.loc	3 289 27
	ldrsw	x8, [x23, :lo12:pvi_wf_get_lut.sftemp]
	mov	w9, #21846
	movk	w9, #21845, lsl #16
.Ltmp52:
	.loc	3 286 28
	str	w0, [x20, #4]
.Ltmp53:
	.loc	3 289 34
	mul	x8, x8, x9
	lsr	x9, x8, #63
	lsr	x8, x8, #32
	add	w9, w8, w9
.Ltmp54:
.LBB0_24:
	.loc	3 289 6 is_stmt 0
	cmp	w25, w9
	b.ne	.LBB0_26
.Ltmp55:
.LBB0_25:
	.loc	3 0 6
	mov	w0, wzr
	b	.LBB0_35
.Ltmp56:
.LBB0_26:
	.loc	3 291 18 is_stmt 1
	ldr	x8, [x20, #24]
	.loc	3 291 16 is_stmt 0
	adrp	x9, waveformdata
	str	x8, [x9, :lo12:waveformdata]
	.loc	3 292 17 is_stmt 1
	bl	epd_overlay_lut
.Ltmp57:
	.loc	3 201 17
	ldr	x10, [x24, :lo12:global_waveform]
	.loc	3 201 34 is_stmt 0
	ldrb	w8, [x10, #38]
.Ltmp58:
	.loc	3 205 2 is_stmt 1
	cbz	w8, .LBB0_31
.Ltmp59:
	.loc	3 0 2 is_stmt 0
	mov	x9, xzr
	.loc	3 205 2
	add	x10, x10, #48
.Ltmp60:
.LBB0_28:
	.loc	3 206 21 is_stmt 1
	ldrb	w11, [x10, x9]
.Ltmp61:
	.loc	3 206 7 is_stmt 0
	cmp	w11, w19
	b.ge	.LBB0_32
.Ltmp62:
	.loc	3 205 29 is_stmt 1
	add	x9, x9, #1
.Ltmp63:
	.loc	3 205 2 is_stmt 0
	cmp	x8, x9
	b.ne	.LBB0_28
.Ltmp64:
	.loc	3 0 2
	mov	w10, #-1
	mov	w9, w8
.Ltmp65:
	b	.LBB0_33
.Ltmp66:
.LBB0_31:
	mov	w9, wzr
	mov	w10, #-1
	b	.LBB0_33
.Ltmp67:
.LBB0_32:
	mov	w10, w9
.Ltmp68:
.LBB0_33:
	sub	w11, w9, #1
	.loc	3 212 6 is_stmt 1
	cmp	w9, w8
	csel	w21, w11, w10, eq
.Ltmp69:
	.loc	3 294 16
	bl	get_wf_mode_index
.Ltmp70:
	and	w0, w0, #0xff
.Ltmp71:
	.loc	3 295 15
	mov	w1, w21
	bl	get_wf_frm_num
.Ltmp72:
	.loc	3 296 7
	tbnz	w0, #31, .LBB0_36
.Ltmp73:
	.loc	3 300 21
	ldrb	w8, [x20]
	.loc	3 301 21
	bfi	w8, w0, #8, #24
	mov	w0, wzr
.Ltmp74:
	str	w8, [x20]
	.loc	3 302 10
	str	w19, [x23, :lo12:pvi_wf_get_lut.sftemp]
.Ltmp75:
.LBB0_35:
	.loc	3 306 1
	ldp	x20, x19, [sp, #80]
	ldp	x22, x21, [sp, #64]
	ldp	x24, x23, [sp, #48]
	ldp	x26, x25, [sp, #32]
	ldr	x27, [sp, #16]
	ldp	x29, x30, [sp], #96
	ldr	x30, [x18, #-8]!
	ret
.LBB0_36:
.Ltmp76:
	.loc	3 0 0 is_stmt 0
	adrp	x0, .L.str
.Ltmp77:
	add	x0, x0, :lo12:.L.str
	bl	_printk
.Ltmp78:
	mov	w0, #-5
	b	.LBB0_35
.Ltmp79:
.Lfunc_end0:
	.size	pvi_wf_get_lut, .Lfunc_end0-pvi_wf_get_lut
	.cfi_endproc

	.p2align	2
	.type	get_wf_mode_index,@function
get_wf_mode_index:
.Lfunc_begin1:
	.loc	3 182 0 is_stmt 1
	.cfi_startproc
	str	x30, [x18], #8
	.cfi_escape 0x16, 0x12, 0x02, 0x82, 0x78
	stp	x29, x30, [sp, #-16]!
	mov	x29, sp
	.cfi_def_cfa w29, 16
	.cfi_offset w30, -8
	.cfi_offset w29, -16
.Ltmp80:
	.loc	3 186 6 prologue_end
	cmp	w0, #11
	b.hs	.LBB1_12
.Ltmp81:
	.loc	3 194 33
	adrp	x8, global_waveform
.Ltmp82:
	.loc	3 189 13
	cmp	w0, #10
.Ltmp83:
	.loc	3 194 33
	ldr	x8, [x8, :lo12:global_waveform]
.Ltmp84:
	.loc	3 167 20
	ldrb	w9, [x8, #16]
	mov	w8, #3
.Ltmp85:
	.loc	3 189 13
	csel	w8, w8, w0, eq
.Ltmp86:
	.loc	3 167 2
	sub	w9, w9, #9
	cmp	w9, #75
	b.hi	.LBB1_4
.Ltmp87:
	.loc	3 0 2 is_stmt 0
	adrp	x10, .LJTI1_0
	add	x10, x10, :lo12:.LJTI1_0
	adr	x11, .LBB1_3
	ldrb	w12, [x10, x9]
	add	x11, x11, x12, lsl #2
	adrp	x9, pvi_mode_table
	add	x9, x9, :lo12:pvi_mode_table
	br	x11
.Ltmp88:
.LBB1_3:
	adrp	x9, pvi_mode_table+30
	add	x9, x9, :lo12:pvi_mode_table+30
	b	.LBB1_10
.Ltmp89:
.LBB1_4:
	adrp	x9, pvi_mode_table+70
	add	x9, x9, :lo12:pvi_mode_table+70
	b	.LBB1_10
.Ltmp90:
.LBB1_5:
	adrp	x9, pvi_mode_table+10
	add	x9, x9, :lo12:pvi_mode_table+10
	b	.LBB1_10
.Ltmp91:
.LBB1_6:
	adrp	x9, pvi_mode_table+20
	add	x9, x9, :lo12:pvi_mode_table+20
	b	.LBB1_10
.Ltmp92:
.LBB1_7:
	adrp	x9, pvi_mode_table+40
	add	x9, x9, :lo12:pvi_mode_table+40
	b	.LBB1_10
.Ltmp93:
.LBB1_8:
	adrp	x9, pvi_mode_table+50
	add	x9, x9, :lo12:pvi_mode_table+50
	b	.LBB1_10
.Ltmp94:
.LBB1_9:
	adrp	x9, pvi_mode_table+60
	add	x9, x9, :lo12:pvi_mode_table+60
.Ltmp95:
.LBB1_10:
	.loc	3 196 9 is_stmt 1
	ldrb	w0, [x9, x8]
.Ltmp96:
.LBB1_11:
	.loc	3 197 1
	ldp	x29, x30, [sp], #16
	ldr	x30, [x18, #-8]!
	ret
.Ltmp97:
.LBB1_12:
	.loc	3 187 3
	adrp	x0, .L.str.3
.Ltmp98:
	add	x0, x0, :lo12:.L.str.3
	bl	_printk
.Ltmp99:
	.loc	3 0 3 is_stmt 0
	mov	w0, #255
	b	.LBB1_11
.Ltmp100:
.Lfunc_end1:
	.size	get_wf_mode_index, .Lfunc_end1-get_wf_mode_index
	.cfi_endproc
	.section	.rodata,"a",@progbits
.LJTI1_0:
	.byte	(.LBB1_10-.LBB1_3)>>2
	.byte	(.LBB1_4-.LBB1_3)>>2
	.byte	(.LBB1_4-.LBB1_3)>>2
	.byte	(.LBB1_4-.LBB1_3)>>2
	.byte	(.LBB1_4-.LBB1_3)>>2
	.byte	(.LBB1_4-.LBB1_3)>>2
	.byte	(.LBB1_4-.LBB1_3)>>2
	.byte	(.LBB1_4-.LBB1_3)>>2
	.byte	(.LBB1_4-.LBB1_3)>>2
	.byte	(.LBB1_5-.LBB1_3)>>2
	.byte	(.LBB1_4-.LBB1_3)>>2
	.byte	(.LBB1_4-.LBB1_3)>>2
	.byte	(.LBB1_4-.LBB1_3)>>2
	.byte	(.LBB1_6-.LBB1_3)>>2
	.byte	(.LBB1_4-.LBB1_3)>>2
	.byte	(.LBB1_4-.LBB1_3)>>2
	.byte	(.LBB1_3-.LBB1_3)>>2
	.byte	(.LBB1_4-.LBB1_3)>>2
	.byte	(.LBB1_4-.LBB1_3)>>2
	.byte	(.LBB1_4-.LBB1_3)>>2
	.byte	(.LBB1_4-.LBB1_3)>>2
	.byte	(.LBB1_4-.LBB1_3)>>2
	.byte	(.LBB1_4-.LBB1_3)>>2
	.byte	(.LBB1_4-.LBB1_3)>>2
	.byte	(.LBB1_4-.LBB1_3)>>2
	.byte	(.LBB1_4-.LBB1_3)>>2
	.byte	(.LBB1_7-.LBB1_3)>>2
	.byte	(.LBB1_4-.LBB1_3)>>2
	.byte	(.LBB1_4-.LBB1_3)>>2
	.byte	(.LBB1_4-.LBB1_3)>>2
	.byte	(.LBB1_4-.LBB1_3)>>2
	.byte	(.LBB1_4-.LBB1_3)>>2
	.byte	(.LBB1_4-.LBB1_3)>>2
	.byte	(.LBB1_4-.LBB1_3)>>2
	.byte	(.LBB1_4-.LBB1_3)>>2
	.byte	(.LBB1_4-.LBB1_3)>>2
	.byte	(.LBB1_4-.LBB1_3)>>2
	.byte	(.LBB1_4-.LBB1_3)>>2
	.byte	(.LBB1_4-.LBB1_3)>>2
	.byte	(.LBB1_4-.LBB1_3)>>2
	.byte	(.LBB1_4-.LBB1_3)>>2
	.byte	(.LBB1_4-.LBB1_3)>>2
	.byte	(.LBB1_4-.LBB1_3)>>2
	.byte	(.LBB1_4-.LBB1_3)>>2
	.byte	(.LBB1_4-.LBB1_3)>>2
	.byte	(.LBB1_4-.LBB1_3)>>2
	.byte	(.LBB1_4-.LBB1_3)>>2
	.byte	(.LBB1_4-.LBB1_3)>>2
	.byte	(.LBB1_4-.LBB1_3)>>2
	.byte	(.LBB1_4-.LBB1_3)>>2
	.byte	(.LBB1_4-.LBB1_3)>>2
	.byte	(.LBB1_4-.LBB1_3)>>2
	.byte	(.LBB1_4-.LBB1_3)>>2
	.byte	(.LBB1_4-.LBB1_3)>>2
	.byte	(.LBB1_4-.LBB1_3)>>2
	.byte	(.LBB1_4-.LBB1_3)>>2
	.byte	(.LBB1_4-.LBB1_3)>>2
	.byte	(.LBB1_4-.LBB1_3)>>2
	.byte	(.LBB1_3-.LBB1_3)>>2
	.byte	(.LBB1_4-.LBB1_3)>>2
	.byte	(.LBB1_4-.LBB1_3)>>2
	.byte	(.LBB1_4-.LBB1_3)>>2
	.byte	(.LBB1_4-.LBB1_3)>>2
	.byte	(.LBB1_8-.LBB1_3)>>2
	.byte	(.LBB1_4-.LBB1_3)>>2
	.byte	(.LBB1_4-.LBB1_3)>>2
	.byte	(.LBB1_4-.LBB1_3)>>2
	.byte	(.LBB1_4-.LBB1_3)>>2
	.byte	(.LBB1_4-.LBB1_3)>>2
	.byte	(.LBB1_4-.LBB1_3)>>2
	.byte	(.LBB1_4-.LBB1_3)>>2
	.byte	(.LBB1_4-.LBB1_3)>>2
	.byte	(.LBB1_4-.LBB1_3)>>2
	.byte	(.LBB1_4-.LBB1_3)>>2
	.byte	(.LBB1_4-.LBB1_3)>>2
	.byte	(.LBB1_9-.LBB1_3)>>2

	.text
	.p2align	2
	.type	get_wf_frm_num,@function
get_wf_frm_num:
.Lfunc_begin2:
	.loc	3 219 0 is_stmt 1
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
.Ltmp101:
	.loc	3 220 45 prologue_end
	adrp	x8, global_waveform
	.loc	3 224 37
	lsl	w9, w0, #2
	.loc	3 220 45
	ldr	x14, [x8, :lo12:global_waveform]
.Ltmp102:
	.loc	3 222 32
	ldrb	w8, [x14, #32]
.Ltmp103:
	.loc	3 224 21
	add	x8, x14, x8
.Ltmp104:
	.loc	3 224 30 is_stmt 0
	add	x8, x8, x9
.Ltmp105:
	.loc	3 225 22 is_stmt 1
	ldrb	w9, [x8]
	.loc	3 225 35 is_stmt 0
	ldrb	w10, [x8, #1]
	.loc	3 225 48
	ldrb	w11, [x8, #2]
	.loc	3 225 6
	ldrb	w12, [x8, #3]
	.loc	3 225 33
	add	w9, w10, w9
	.loc	3 225 46
	add	w9, w9, w11
.Ltmp106:
	.loc	3 225 6
	cmp	w12, w9, uxtb
	b.ne	.LBB2_36
.Ltmp107:
	.loc	3 0 6
	ldr	w8, [x8]
.Ltmp108:
	.loc	3 231 43 is_stmt 1
	lsl	w9, w1, #2
	.loc	3 229 28
	and	x8, x8, #0xffffff
.Ltmp109:
	.loc	3 231 21
	add	x8, x14, x8
.Ltmp110:
	.loc	3 231 30 is_stmt 0
	add	x8, x8, w9, sxtw
.Ltmp111:
	.loc	3 232 22 is_stmt 1
	ldrb	w9, [x8]
	.loc	3 232 35 is_stmt 0
	ldrb	w10, [x8, #1]
	.loc	3 232 48
	ldrb	w11, [x8, #2]
	.loc	3 232 6
	ldrb	w12, [x8, #3]
	.loc	3 232 33
	add	w9, w10, w9
	.loc	3 232 46
	add	w9, w9, w11
.Ltmp112:
	.loc	3 232 6
	cmp	w12, w9, uxtb
	b.ne	.LBB2_37
.Ltmp113:
	.loc	3 55 7 is_stmt 1
	adrp	x20, waveformdata
	ldr	x10, [x20, :lo12:waveformdata]
.Ltmp114:
	.loc	3 55 6 is_stmt 0
	cbz	x10, .LBB2_39
.Ltmp115:
	.loc	3 0 6
	adrp	x21, need_pic
.Ltmp116:
	.loc	3 60 6 is_stmt 1
	adrp	x22, maxpic
	ldr	w15, [x8]
	mov	w16, #5
	mov	w0, #32
.Ltmp117:
	.loc	3 0 6 is_stmt 0
	mov	w13, wzr
	ldr	w9, [x21, :lo12:need_pic]
	mov	w11, wzr
	.loc	3 60 6
	ldrb	w8, [x22, :lo12:maxpic]
.Ltmp118:
	.loc	3 0 0
	and	x15, x15, #0xffffff
.Ltmp119:
	add	x14, x14, x15
.Ltmp120:
	mov	w19, wzr
	cmp	w9, #32
	mov	w12, wzr
	cset	w17, eq
	bics	w15, w17, w8
.Ltmp121:
	mov	w17, #16
	cinc	w16, w16, ne
	cmp	w8, #0
	csel	w17, w0, w17, ne
	mov	w0, #1
.Ltmp122:
	b	.LBB2_6
.Ltmp123:
.LBB2_4:
	mov	w0, #1
.Ltmp124:
.LBB2_5:
	add	w11, w11, #1
.Ltmp125:
	.loc	3 64 7 is_stmt 1
	cmp	w19, #254
	b.hi	.LBB2_18
.Ltmp126:
.LBB2_6:
	.loc	3 69 7
	ldrb	w3, [x14, w11, uxtw]
.Ltmp127:
	.loc	3 69 7 is_stmt 0
	cmp	w3, #252
	b.eq	.LBB2_11
.Ltmp128:
	cmp	w3, #255
	b.eq	.LBB2_19
.Ltmp129:
	.loc	3 79 7 is_stmt 1
	tbnz	w0, #0, .LBB2_12
.Ltmp130:
.LBB2_9:
	.loc	3 114 44
	add	w0, w13, #1
.Ltmp131:
	.loc	3 114 75 is_stmt 0
	lsl	w1, w12, w15
.Ltmp132:
	.loc	3 114 47
	lsl	w2, w13, w16
	.loc	3 114 35
	add	w1, w1, w19, lsl #10
	.loc	3 115 47 is_stmt 1
	lsl	w0, w0, w16
.Ltmp133:
	.loc	3 114 65
	add	w2, w1, w2
	.loc	3 115 65
	add	w0, w1, w0
	.loc	3 114 99
	and	w4, w3, #0x3
	.loc	3 115 106
	ubfx	w5, w3, #2, #2
	.loc	3 114 88
	strb	w4, [x10, w2, uxtw]
	.loc	3 115 44
	add	w2, w13, #2
.Ltmp134:
	.loc	3 115 88 is_stmt 0
	strb	w5, [x10, w0, uxtw]
	.loc	3 116 44 is_stmt 1
	add	w0, w13, #3
.Ltmp135:
	.loc	3 116 47 is_stmt 0
	lsl	w2, w2, w16
	.loc	3 117 47 is_stmt 1
	lsl	w0, w0, w16
.Ltmp136:
	.loc	3 116 65
	add	w2, w1, w2
	.loc	3 117 65
	add	w0, w1, w0
	.loc	3 116 106
	ubfx	w4, w3, #4, #2
	.loc	3 117 100
	lsr	w1, w3, #6
	.loc	3 117 44 is_stmt 0
	add	w13, w13, #4
.Ltmp137:
	.loc	3 116 88 is_stmt 1
	strb	w4, [x10, w2, uxtw]
	.loc	3 120 8
	cmp	w13, w17
	.loc	3 117 88
	strb	w1, [x10, w0, uxtw]
.Ltmp138:
	.loc	3 0 88 is_stmt 0
	mov	w0, wzr
	.loc	3 120 8 is_stmt 1
	b.lo	.LBB2_5
.Ltmp139:
	.loc	3 122 9
	add	w1, w12, #1
.Ltmp140:
	.loc	3 0 9 is_stmt 0
	mov	w13, wzr
	.loc	3 122 18
	cmp	w1, w17
.Ltmp141:
	.loc	3 122 9
	csinc	w12, wzr, w12, hs
	cinc	w19, w19, hs
.Ltmp142:
	.loc	3 0 9
	b	.LBB2_5
.Ltmp143:
.LBB2_11:
	.loc	3 76 10 is_stmt 1
	add	w11, w11, #1
.Ltmp144:
	.loc	3 0 0 is_stmt 0
	ldrb	w3, [x14, w11, uxtw]
.Ltmp145:
	.loc	3 79 7 is_stmt 1
	tbnz	w0, #0, .LBB2_9
.Ltmp146:
.LBB2_12:
	.loc	3 81 44
	add	w0, w13, #1
.Ltmp147:
	.loc	3 81 75 is_stmt 0
	lsl	w1, w12, w15
.Ltmp148:
	.loc	3 81 47
	lsl	w2, w13, w16
	.loc	3 81 35
	add	w4, w1, w19, lsl #10
	.loc	3 82 47 is_stmt 1
	lsl	w1, w0, w16
	.loc	3 81 65
	add	w2, w4, w2
	.loc	3 82 65
	add	w5, w4, w1
	.loc	3 81 99
	and	w0, w3, #0x3
.Ltmp149:
	.loc	3 82 106
	ubfx	w1, w3, #2, #2
	.loc	3 81 88
	strb	w0, [x10, w2, uxtw]
	.loc	3 82 44
	add	w2, w13, #2
.Ltmp150:
	.loc	3 82 88 is_stmt 0
	strb	w1, [x10, w5, uxtw]
	.loc	3 83 44 is_stmt 1
	add	w5, w13, #3
.Ltmp151:
	.loc	3 83 47 is_stmt 0
	lsl	w2, w2, w16
	.loc	3 84 47 is_stmt 1
	lsl	w5, w5, w16
.Ltmp152:
	.loc	3 83 65
	add	w6, w4, w2
	.loc	3 83 106 is_stmt 0
	ubfx	w2, w3, #4, #2
	.loc	3 84 65 is_stmt 1
	add	w4, w4, w5
	.loc	3 84 100 is_stmt 0
	lsr	w3, w3, #6
.Ltmp153:
	.loc	3 84 44
	add	w13, w13, #4
.Ltmp154:
	.loc	3 87 8 is_stmt 1
	cmp	w13, w17
	.loc	3 83 88
	strb	w2, [x10, w6, uxtw]
	.loc	3 84 88
	strb	w3, [x10, w4, uxtw]
.Ltmp155:
	.loc	3 87 8
	b.lo	.LBB2_14
.Ltmp156:
	.loc	3 89 9
	add	w4, w12, #1
.Ltmp157:
	.loc	3 0 9 is_stmt 0
	mov	w13, wzr
	.loc	3 89 18
	cmp	w4, w17
.Ltmp158:
	.loc	3 89 9
	csinc	w12, wzr, w12, hs
	cinc	w19, w19, hs
.Ltmp159:
.LBB2_14:
	.loc	3 0 0
	add	w11, w11, #1
.Ltmp160:
	.loc	3 95 8 is_stmt 1
	ldrb	w4, [x14, w11, uxtw]
.Ltmp161:
	.loc	3 96 4
	cbnz	w4, .LBB2_16
	b	.LBB2_4
.Ltmp162:
.LBB2_15:
	.loc	3 96 19 is_stmt 0
	sub	w4, w4, #1
.Ltmp163:
	tst	w4, #0xff
.Ltmp164:
	.loc	3 96 4
	b.eq	.LBB2_4
.Ltmp165:
.LBB2_16:
	.loc	3 97 45 is_stmt 1
	add	w5, w13, #1
.Ltmp166:
	.loc	3 97 76 is_stmt 0
	lsl	w6, w12, w15
	.loc	3 98 45 is_stmt 1
	add	w23, w13, #2
.Ltmp167:
	.loc	3 99 45
	add	w24, w13, #3
.Ltmp168:
	.loc	3 97 48
	lsl	w7, w13, w16
	.loc	3 97 36 is_stmt 0
	add	w6, w6, w19, lsl #10
	.loc	3 98 48 is_stmt 1
	lsl	w5, w5, w16
	.loc	3 99 48
	lsl	w23, w23, w16
	.loc	3 100 48
	lsl	w24, w24, w16
.Ltmp169:
	.loc	3 97 66
	add	w7, w6, w7
	.loc	3 98 66
	add	w5, w6, w5
	.loc	3 99 66
	add	w23, w6, w23
	.loc	3 100 66
	add	w6, w6, w24
	.loc	3 100 45 is_stmt 0
	add	w13, w13, #4
.Ltmp170:
	.loc	3 101 9 is_stmt 1
	cmp	w13, w17
	.loc	3 97 89
	strb	w0, [x10, w7, uxtw]
	.loc	3 98 89
	strb	w1, [x10, w5, uxtw]
	.loc	3 99 89
	strb	w2, [x10, w23, uxtw]
	.loc	3 100 89
	strb	w3, [x10, w6, uxtw]
	.loc	3 101 9
	b.lo	.LBB2_15
.Ltmp171:
	.loc	3 103 10
	add	w5, w12, #1
.Ltmp172:
	.loc	3 0 10 is_stmt 0
	mov	w13, wzr
	.loc	3 103 19
	cmp	w5, w17
.Ltmp173:
	.loc	3 103 10
	csinc	w12, wzr, w12, hs
	cinc	w19, w19, hs
.Ltmp174:
	.loc	3 0 10
	b	.LBB2_15
.Ltmp175:
.LBB2_18:
	.loc	3 65 4 is_stmt 1
	adrp	x0, .L.str.6
	add	x0, x0, :lo12:.L.str.6
	bl	_printk
.Ltmp176:
	.loc	3 130 6
	ldrb	w8, [x22, :lo12:maxpic]
	ldr	w9, [x21, :lo12:need_pic]
.Ltmp177:
.LBB2_19:
	.loc	3 130 24 is_stmt 0
	cbz	w8, .LBB2_26
.Ltmp178:
	cmp	w9, #16
	b.ne	.LBB2_26
.Ltmp179:
	.loc	3 132 3 is_stmt 1
	cbz	w19, .LBB2_41
.Ltmp180:
	.loc	3 0 3 is_stmt 0
	ldr	x13, [x20, :lo12:waveformdata]
	mov	w10, wzr
	mov	w11, wzr
	.loc	3 132 3
	add	x12, x13, #16
	add	x13, x13, #7
.Ltmp181:
.LBB2_23:
	.loc	3 0 3
	mov	w15, w10
	mov	x16, #-2
.Ltmp182:
	add	x14, x12, x15
	and	x15, x15, #0xffffffe0
	add	x15, x13, x15
.Ltmp183:
.LBB2_24:
	.loc	3 135 17 is_stmt 1
	ldurb	w17, [x14, #-16]
.Ltmp184:
	.loc	3 133 28
	add	x16, x16, #2
.Ltmp185:
	.loc	3 133 4 is_stmt 0
	cmp	x16, #30
.Ltmp186:
	.loc	3 136 61 is_stmt 1
	sturb	w17, [x15, #-7]
.Ltmp187:
	.loc	3 135 17
	ldurb	w17, [x14, #-14]
.Ltmp188:
	.loc	3 136 61
	sturb	w17, [x15, #-6]
.Ltmp189:
	.loc	3 135 17
	ldurb	w17, [x14, #-12]
.Ltmp190:
	.loc	3 136 61
	sturb	w17, [x15, #-5]
.Ltmp191:
	.loc	3 135 17
	ldurb	w17, [x14, #-10]
.Ltmp192:
	.loc	3 136 61
	sturb	w17, [x15, #-4]
.Ltmp193:
	.loc	3 135 17
	ldurb	w17, [x14, #-8]
.Ltmp194:
	.loc	3 136 61
	sturb	w17, [x15, #-3]
.Ltmp195:
	.loc	3 135 17
	ldurb	w17, [x14, #-6]
.Ltmp196:
	.loc	3 136 61
	sturb	w17, [x15, #-2]
.Ltmp197:
	.loc	3 135 17
	ldurb	w17, [x14, #-4]
.Ltmp198:
	.loc	3 136 61
	sturb	w17, [x15, #-1]
.Ltmp199:
	.loc	3 135 17
	ldurb	w17, [x14, #-2]
.Ltmp200:
	.loc	3 136 61
	strb	w17, [x15]
.Ltmp201:
	.loc	3 135 17
	ldrb	w17, [x14]
.Ltmp202:
	.loc	3 136 61
	strb	w17, [x15, #1]
.Ltmp203:
	.loc	3 135 17
	ldrb	w17, [x14, #2]
.Ltmp204:
	.loc	3 136 61
	strb	w17, [x15, #2]
.Ltmp205:
	.loc	3 135 17
	ldrb	w17, [x14, #4]
.Ltmp206:
	.loc	3 136 61
	strb	w17, [x15, #3]
.Ltmp207:
	.loc	3 135 17
	ldrb	w17, [x14, #6]
.Ltmp208:
	.loc	3 136 61
	strb	w17, [x15, #4]
.Ltmp209:
	.loc	3 135 17
	ldrb	w17, [x14, #8]
.Ltmp210:
	.loc	3 136 61
	strb	w17, [x15, #5]
.Ltmp211:
	.loc	3 135 17
	ldrb	w17, [x14, #10]
.Ltmp212:
	.loc	3 136 61
	strb	w17, [x15, #6]
.Ltmp213:
	.loc	3 135 17
	ldrb	w17, [x14, #12]
.Ltmp214:
	.loc	3 136 61
	strb	w17, [x15, #7]
.Ltmp215:
	.loc	3 135 17
	ldrb	w17, [x14, #14]
.Ltmp216:
	.loc	3 133 28
	add	x14, x14, #64
.Ltmp217:
	.loc	3 136 61
	strb	w17, [x15, #8]
.Ltmp218:
	.loc	3 133 28
	add	x15, x15, #32
.Ltmp219:
	.loc	3 133 4 is_stmt 0
	b.lo	.LBB2_24
.Ltmp220:
	.loc	3 132 31 is_stmt 1
	add	w11, w11, #1
.Ltmp221:
	.loc	3 132 3 is_stmt 0
	add	w10, w10, #1024
.Ltmp222:
	cmp	w19, w11, uxtb
	b.hi	.LBB2_23
.Ltmp223:
.LBB2_26:
	.loc	3 0 3
	cmp	w9, #32
	cset	w9, eq
.Ltmp224:
	.loc	3 142 24 is_stmt 1
	and	w8, w8, w9
	cmp	w8, #1
	b.ne	.LBB2_41
.Ltmp225:
	.loc	3 144 3
	cbz	w19, .LBB2_41
.Ltmp226:
	.loc	3 0 3 is_stmt 0
	ldr	x8, [x20, :lo12:waveformdata]
	mov	x9, xzr
	.loc	3 144 3
	subs	w10, w19, #1
	b.eq	.LBB2_34
.Ltmp227:
	neg	w11, w19
	tst	w11, #0xff
	b.eq	.LBB2_34
.Ltmp228:
	.loc	3 0 3
	cmp	w10, #255
	b.hi	.LBB2_34
.Ltmp229:
	add	x10, x10, #1
	mov	x12, x8
	.loc	3 144 3
	and	x9, x10, #0x1fffffffe
	mov	x11, x9
.Ltmp230:
.LBB2_32:
	.loc	3 144 15
	strb	wzr, [x12, #989]
	subs	x11, x11, #2
	strb	wzr, [x12, #2013]
	add	x12, x12, #2048
	b.ne	.LBB2_32
.Ltmp231:
	.loc	3 144 3
	cmp	x10, x9
	b.eq	.LBB2_41
.Ltmp232:
.LBB2_34:
	add	x8, x8, x9, lsl #10
	add	w9, w9, #1
	add	x8, x8, #989
.Ltmp233:
.LBB2_35:
	add	w10, w9, #1
	cmp	w19, w9, uxtb
.Ltmp234:
	.loc	3 145 45 is_stmt 1
	strb	wzr, [x8]
.Ltmp235:
	.loc	3 144 3
	add	x8, x8, #1024
	mov	w9, w10
	b.hi	.LBB2_35
	b	.LBB2_41
.Ltmp236:
.LBB2_36:
	.loc	3 226 3
	adrp	x0, .L.str.4
.Ltmp237:
	adrp	x1, .L__func__.get_wf_frm_num
.Ltmp238:
	add	x0, x0, :lo12:.L.str.4
	add	x1, x1, :lo12:.L__func__.get_wf_frm_num
	mov	w2, #226
	b	.LBB2_38
.Ltmp239:
.LBB2_37:
	.loc	3 233 3
	adrp	x0, .L.str.4
.Ltmp240:
	adrp	x1, .L__func__.get_wf_frm_num
.Ltmp241:
	add	x0, x0, :lo12:.L.str.4
	add	x1, x1, :lo12:.L__func__.get_wf_frm_num
	mov	w2, #233
.Ltmp242:
.LBB2_38:
	.loc	3 0 0 is_stmt 0
	bl	_printk
.Ltmp243:
	b	.LBB2_40
.Ltmp244:
.LBB2_39:
	.loc	3 56 3 is_stmt 1
	adrp	x0, .L.str.5
.Ltmp245:
	add	x0, x0, :lo12:.L.str.5
	bl	_printk
.Ltmp246:
.LBB2_40:
	.loc	3 0 3 is_stmt 0
	mov	w19, #-22
.Ltmp247:
.LBB2_41:
	.loc	3 240 1 is_stmt 1
	mov	w0, w19
	ldp	x20, x19, [sp, #48]
	ldp	x22, x21, [sp, #32]
	ldp	x24, x23, [sp, #16]
	ldp	x29, x30, [sp], #64
	ldr	x30, [x18, #-8]!
	ret
.Ltmp248:
.Lfunc_end2:
	.size	get_wf_frm_num, .Lfunc_end2-get_wf_frm_num
	.cfi_endproc
	.file	6 "/home/lyx/rk-android14/kernel-6.1" "./include/linux/types.h"

	.globl	pvi_wf_input
	.p2align	2
	.type	pvi_wf_input,@function
pvi_wf_input:
.Lfunc_begin3:
	.loc	3 315 0
	.cfi_startproc
	str	x30, [x18], #8
	.cfi_escape 0x16, 0x12, 0x02, 0x82, 0x78
	stp	x29, x30, [sp, #-32]!
	stp	x20, x19, [sp, #16]
	mov	x29, sp
	.cfi_def_cfa w29, 32
	.cfi_offset w19, -8
	.cfi_offset w20, -16
	.cfi_offset w30, -24
	.cfi_offset w29, -32
.Ltmp249:
	.loc	3 318 6 prologue_end
	adrp	x20, global_waveform
	ldr	x8, [x20, :lo12:global_waveform]
.Ltmp250:
	.loc	3 318 6 is_stmt 0
	cbz	x8, .LBB3_3
.Ltmp251:
	.loc	3 0 6
	mov	w0, #-16
.Ltmp252:
.LBB3_2:
	.loc	3 347 1 is_stmt 1
	ldp	x20, x19, [sp, #16]
	ldp	x29, x30, [sp], #32
	ldr	x30, [x18, #-8]!
	ret
.Ltmp253:
.LBB3_3:
	.loc	3 0 1 is_stmt 0
	mov	x19, x0
.Ltmp254:
	.loc	3 321 2 is_stmt 1
	ldrb	w1, [x0, #16]
	adrp	x0, .L.str.1
	add	x0, x0, :lo12:.L.str.1
	bl	_printk
.Ltmp255:
	.loc	3 324 18
	ldrb	w1, [x19, #16]
	.loc	3 324 2 is_stmt 0
	sub	w8, w1, #9
	cmp	w8, #75
	b.hi	.LBB3_7
.Ltmp256:
	.loc	3 0 2
	adrp	x9, .LJTI3_0
	add	x9, x9, :lo12:.LJTI3_0
	adr	x10, .LBB3_5
	ldrb	w11, [x9, x8]
	add	x10, x10, x11, lsl #2
	br	x10
.Ltmp257:
.LBB3_5:
	.loc	3 331 10 is_stmt 1
	adrp	x8, maxpic
	mov	w9, #1
	strb	w9, [x8, :lo12:maxpic]
.Ltmp258:
.LBB3_6:
	.loc	3 0 10 is_stmt 0
	mov	w0, wzr
	.loc	3 344 18 is_stmt 1
	str	x19, [x20, :lo12:global_waveform]
	b	.LBB3_2
.Ltmp259:
.LBB3_7:
	.loc	3 339 3
	adrp	x0, .L.str.2
	ldrb	w2, [x19, #22]
	add	x0, x0, :lo12:.L.str.2
	bl	_printk
.Ltmp260:
	.loc	3 0 3 is_stmt 0
	mov	w0, #-8
	b	.LBB3_2
.Ltmp261:
.Lfunc_end3:
	.size	pvi_wf_input, .Lfunc_end3-pvi_wf_input
	.cfi_endproc
	.section	.rodata,"a",@progbits
.LJTI3_0:
	.byte	(.LBB3_6-.LBB3_5)>>2
	.byte	(.LBB3_7-.LBB3_5)>>2
	.byte	(.LBB3_7-.LBB3_5)>>2
	.byte	(.LBB3_7-.LBB3_5)>>2
	.byte	(.LBB3_7-.LBB3_5)>>2
	.byte	(.LBB3_7-.LBB3_5)>>2
	.byte	(.LBB3_7-.LBB3_5)>>2
	.byte	(.LBB3_7-.LBB3_5)>>2
	.byte	(.LBB3_7-.LBB3_5)>>2
	.byte	(.LBB3_6-.LBB3_5)>>2
	.byte	(.LBB3_7-.LBB3_5)>>2
	.byte	(.LBB3_7-.LBB3_5)>>2
	.byte	(.LBB3_7-.LBB3_5)>>2
	.byte	(.LBB3_5-.LBB3_5)>>2
	.byte	(.LBB3_7-.LBB3_5)>>2
	.byte	(.LBB3_5-.LBB3_5)>>2
	.byte	(.LBB3_5-.LBB3_5)>>2
	.byte	(.LBB3_7-.LBB3_5)>>2
	.byte	(.LBB3_7-.LBB3_5)>>2
	.byte	(.LBB3_7-.LBB3_5)>>2
	.byte	(.LBB3_7-.LBB3_5)>>2
	.byte	(.LBB3_7-.LBB3_5)>>2
	.byte	(.LBB3_7-.LBB3_5)>>2
	.byte	(.LBB3_5-.LBB3_5)>>2
	.byte	(.LBB3_7-.LBB3_5)>>2
	.byte	(.LBB3_7-.LBB3_5)>>2
	.byte	(.LBB3_6-.LBB3_5)>>2
	.byte	(.LBB3_7-.LBB3_5)>>2
	.byte	(.LBB3_7-.LBB3_5)>>2
	.byte	(.LBB3_7-.LBB3_5)>>2
	.byte	(.LBB3_7-.LBB3_5)>>2
	.byte	(.LBB3_7-.LBB3_5)>>2
	.byte	(.LBB3_7-.LBB3_5)>>2
	.byte	(.LBB3_7-.LBB3_5)>>2
	.byte	(.LBB3_7-.LBB3_5)>>2
	.byte	(.LBB3_7-.LBB3_5)>>2
	.byte	(.LBB3_7-.LBB3_5)>>2
	.byte	(.LBB3_7-.LBB3_5)>>2
	.byte	(.LBB3_7-.LBB3_5)>>2
	.byte	(.LBB3_7-.LBB3_5)>>2
	.byte	(.LBB3_7-.LBB3_5)>>2
	.byte	(.LBB3_7-.LBB3_5)>>2
	.byte	(.LBB3_7-.LBB3_5)>>2
	.byte	(.LBB3_7-.LBB3_5)>>2
	.byte	(.LBB3_7-.LBB3_5)>>2
	.byte	(.LBB3_7-.LBB3_5)>>2
	.byte	(.LBB3_7-.LBB3_5)>>2
	.byte	(.LBB3_7-.LBB3_5)>>2
	.byte	(.LBB3_7-.LBB3_5)>>2
	.byte	(.LBB3_7-.LBB3_5)>>2
	.byte	(.LBB3_7-.LBB3_5)>>2
	.byte	(.LBB3_7-.LBB3_5)>>2
	.byte	(.LBB3_7-.LBB3_5)>>2
	.byte	(.LBB3_7-.LBB3_5)>>2
	.byte	(.LBB3_7-.LBB3_5)>>2
	.byte	(.LBB3_7-.LBB3_5)>>2
	.byte	(.LBB3_7-.LBB3_5)>>2
	.byte	(.LBB3_7-.LBB3_5)>>2
	.byte	(.LBB3_5-.LBB3_5)>>2
	.byte	(.LBB3_7-.LBB3_5)>>2
	.byte	(.LBB3_7-.LBB3_5)>>2
	.byte	(.LBB3_7-.LBB3_5)>>2
	.byte	(.LBB3_7-.LBB3_5)>>2
	.byte	(.LBB3_5-.LBB3_5)>>2
	.byte	(.LBB3_7-.LBB3_5)>>2
	.byte	(.LBB3_7-.LBB3_5)>>2
	.byte	(.LBB3_7-.LBB3_5)>>2
	.byte	(.LBB3_7-.LBB3_5)>>2
	.byte	(.LBB3_7-.LBB3_5)>>2
	.byte	(.LBB3_7-.LBB3_5)>>2
	.byte	(.LBB3_7-.LBB3_5)>>2
	.byte	(.LBB3_7-.LBB3_5)>>2
	.byte	(.LBB3_7-.LBB3_5)>>2
	.byte	(.LBB3_7-.LBB3_5)>>2
	.byte	(.LBB3_7-.LBB3_5)>>2
	.byte	(.LBB3_6-.LBB3_5)>>2

	.text
	.globl	pvi_wf_get_version
	.p2align	2
	.type	pvi_wf_get_version,@function
pvi_wf_get_version:
.Lfunc_begin4:
	.loc	3 356 0 is_stmt 1
	.cfi_startproc
	.loc	3 357 39 prologue_end
	adrp	x8, global_waveform
	ldr	x8, [x8, :lo12:global_waveform]
.Ltmp262:
	.loc	3 360 6
	cbz	x8, .LBB4_2
.Ltmp263:
	.loc	3 363 2
	adrp	x0, pvi_wf_get_version.spi_id_buffer
	ldur	x9, [x8, #73]
	ldr	x10, [x8, #88]
	add	x0, x0, :lo12:pvi_wf_get_version.spi_id_buffer
	ldur	x11, [x8, #81]
	ldur	x8, [x8, #65]
.Ltmp264:
	stur	x10, [x0, #23]
	stp	x9, x11, [x0, #8]
	str	x8, [x0]
	.loc	3 364 20
	strb	wzr, [x0, #31]
	.loc	3 367 1
	ret
.LBB4_2:
.Ltmp265:
	.loc	3 0 1 is_stmt 0
	mov	x0, xzr
	.loc	3 367 1
	ret
.Ltmp266:
.Lfunc_end4:
	.size	pvi_wf_get_version, .Lfunc_end4-pvi_wf_get_version
	.cfi_endproc

	.type	pvi_mode_table,@object
	.section	.rodata,"a",@progbits
	.globl	pvi_mode_table
pvi_mode_table:
	.asciz	"\000\001\001\002\003\003\003\004\003"
	.asciz	"\000\001\007\003\003\005\006\004\005"
	.asciz	"\000\001\001\002\003\004\004\006\005"
	.asciz	"\000\001\007\002\003\004\005\006\004"
	.asciz	"\000\001\005\002\003\003\003\004\003"
	.asciz	"\000\001\001\002\003\004\004\006\004"
	.asciz	"\000\001\001\002\003\004\004\005\004"
	.asciz	"\000\001\001\002\003\004\005\006\004"
	.size	pvi_mode_table, 80

	.type	pvi_wf_get_lut.stype,@object
	.data
	.p2align	2
pvi_wf_get_lut.stype:
	.word	11
	.size	pvi_wf_get_lut.stype, 4

	.type	pvi_wf_get_lut.sftemp,@object
	.p2align	2
pvi_wf_get_lut.sftemp:
	.word	4294967295
	.size	pvi_wf_get_lut.sftemp, 4

	.type	global_waveform,@object
	.local	global_waveform
	.comm	global_waveform,8,8
	.type	need_pic,@object
	.p2align	2
need_pic:
	.word	16
	.size	need_pic, 4

	.type	waveformdata,@object
	.local	waveformdata
	.comm	waveformdata,8,8
	.type	.L.str,@object
	.section	.rodata.str1.1,"aMS",@progbits,1
.L.str:
	.asciz	"pvi waveform get frame number failed\n"
	.size	.L.str, 38

	.type	.L.str.1,@object
.L.str.1:
	.asciz	"pvi : input waveform version 0x%x\n"
	.size	.L.str.1, 35

	.type	maxpic,@object
	.local	maxpic
	.comm	maxpic,1,4
	.type	.L.str.2,@object
.L.str.2:
	.asciz	"pvi : Unknow waveform version 0x%x, 0x%x, may be wrong waveform file\n"
	.size	.L.str.2, 70

	.type	pvi_wf_get_version.spi_id_buffer,@object
	.local	pvi_wf_get_version.spi_id_buffer
	.comm	pvi_wf_get_version.spi_id_buffer,32,1
	.type	.L.str.3,@object
.L.str.3:
	.asciz	"pvi: unspport PVI waveform type"
	.size	.L.str.3, 32

	.type	.L.str.4,@object
.L.str.4:
	.asciz	"pvi: %s %d check error\n"
	.size	.L.str.4, 24

	.type	.L__func__.get_wf_frm_num,@object
.L__func__.get_wf_frm_num:
	.asciz	"get_wf_frm_num"
	.size	.L__func__.get_wf_frm_num, 15

	.type	.L.str.5,@object
.L.str.5:
	.asciz	"waveformdata is NULL\n"
	.size	.L.str.5, 22

	.type	.L.str.6,@object
.L.str.6:
	.asciz	"pvi: decodec waveform error, framenum err\n"
	.size	.L.str.6, 43

	.section	.debug_loc,"",@progbits
.Ldebug_loc0:
	.xword	.Lfunc_begin0-.Lfunc_begin0
	.xword	.Ltmp5-.Lfunc_begin0
	.hword	1
	.byte	80
	.xword	.Ltmp5-.Lfunc_begin0
	.xword	.Ltmp20-.Lfunc_begin0
	.hword	1
	.byte	100
	.xword	.Ltmp20-.Lfunc_begin0
	.xword	.Ltmp21-.Lfunc_begin0
	.hword	1
	.byte	80
	.xword	.Ltmp22-.Lfunc_begin0
	.xword	.Ltmp75-.Lfunc_begin0
	.hword	1
	.byte	100
	.xword	.Ltmp76-.Lfunc_begin0
	.xword	.Lfunc_end0-.Lfunc_begin0
	.hword	1
	.byte	100
	.xword	0
	.xword	0
.Ldebug_loc1:
	.xword	.Lfunc_begin0-.Lfunc_begin0
	.xword	.Ltmp4-.Lfunc_begin0
	.hword	1
	.byte	81
	.xword	.Ltmp4-.Lfunc_begin0
	.xword	.Ltmp20-.Lfunc_begin0
	.hword	1
	.byte	101
	.xword	.Ltmp20-.Lfunc_begin0
	.xword	.Ltmp22-.Lfunc_begin0
	.hword	1
	.byte	81
	.xword	.Ltmp22-.Lfunc_begin0
	.xword	.Ltmp47-.Lfunc_begin0
	.hword	1
	.byte	101
	.xword	.Ltmp47-.Lfunc_begin0
	.xword	.Ltmp54-.Lfunc_begin0
	.hword	4
	.byte	243
	.byte	1
	.byte	81
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc2:
	.xword	.Lfunc_begin0-.Lfunc_begin0
	.xword	.Ltmp3-.Lfunc_begin0
	.hword	1
	.byte	82
	.xword	.Ltmp3-.Lfunc_begin0
	.xword	.Ltmp20-.Lfunc_begin0
	.hword	1
	.byte	99
	.xword	.Ltmp20-.Lfunc_begin0
	.xword	.Ltmp22-.Lfunc_begin0
	.hword	1
	.byte	82
	.xword	.Ltmp22-.Lfunc_begin0
	.xword	.Ltmp75-.Lfunc_begin0
	.hword	1
	.byte	99
	.xword	.Ltmp76-.Lfunc_begin0
	.xword	.Lfunc_end0-.Lfunc_begin0
	.hword	1
	.byte	99
	.xword	0
	.xword	0
.Ldebug_loc3:
	.xword	.Lfunc_begin0-.Lfunc_begin0
	.xword	.Ltmp28-.Lfunc_begin0
	.hword	1
	.byte	83
	.xword	.Ltmp28-.Lfunc_begin0
	.xword	.Ltmp54-.Lfunc_begin0
	.hword	4
	.byte	243
	.byte	1
	.byte	83
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc4:
	.xword	.Ltmp1-.Lfunc_begin0
	.xword	.Ltmp58-.Lfunc_begin0
	.hword	3
	.byte	48
	.byte	32
	.byte	159
	.xword	.Ltmp58-.Lfunc_begin0
	.xword	.Ltmp70-.Lfunc_begin0
	.hword	1
	.byte	80
	.xword	0
	.xword	0
.Ldebug_loc5:
	.xword	.Ltmp1-.Lfunc_begin0
	.xword	.Ltmp30-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	0
	.byte	159
	.xword	.Ltmp30-.Lfunc_begin0
	.xword	.Ltmp45-.Lfunc_begin0
	.hword	1
	.byte	80
	.xword	.Ltmp50-.Lfunc_begin0
	.xword	.Ltmp54-.Lfunc_begin0
	.hword	1
	.byte	80
	.xword	.Ltmp72-.Lfunc_begin0
	.xword	.Ltmp74-.Lfunc_begin0
	.hword	1
	.byte	80
	.xword	.Ltmp76-.Lfunc_begin0
	.xword	.Ltmp77-.Lfunc_begin0
	.hword	1
	.byte	80
	.xword	0
	.xword	0
.Ldebug_loc6:
	.xword	.Ltmp1-.Lfunc_begin0
	.xword	.Ltmp29-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	0
	.byte	159
	.xword	.Ltmp29-.Lfunc_begin0
	.xword	.Ltmp30-.Lfunc_begin0
	.hword	1
	.byte	80
	.xword	.Ltmp49-.Lfunc_begin0
	.xword	.Ltmp50-.Lfunc_begin0
	.hword	1
	.byte	80
	.xword	.Ltmp71-.Lfunc_begin0
	.xword	.Ltmp72-.Lfunc_begin0
	.hword	1
	.byte	80
	.xword	0
	.xword	0
.Ldebug_loc7:
	.xword	.Ltmp1-.Lfunc_begin0
	.xword	.Ltmp27-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	0
	.byte	159
	.xword	.Ltmp27-.Lfunc_begin0
	.xword	.Ltmp47-.Lfunc_begin0
	.hword	1
	.byte	102
	.xword	.Ltmp47-.Lfunc_begin0
	.xword	.Ltmp54-.Lfunc_begin0
	.hword	1
	.byte	101
	.xword	.Ltmp69-.Lfunc_begin0
	.xword	.Ltmp75-.Lfunc_begin0
	.hword	1
	.byte	101
	.xword	0
	.xword	0
.Ldebug_loc8:
	.xword	.Ltmp13-.Lfunc_begin0
	.xword	.Ltmp19-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	0
	.byte	159
	.xword	.Ltmp25-.Lfunc_begin0
	.xword	.Ltmp31-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	0
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc9:
	.xword	.Ltmp12-.Lfunc_begin0
	.xword	.Ltmp15-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	0
	.byte	159
	.xword	.Ltmp15-.Lfunc_begin0
	.xword	.Ltmp19-.Lfunc_begin0
	.hword	1
	.byte	90
	.xword	0
	.xword	0
.Ldebug_loc10:
	.xword	.Ltmp12-.Lfunc_begin0
	.xword	.Ltmp19-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	127
	.byte	159
	.xword	.Ltmp25-.Lfunc_begin0
	.xword	.Ltmp27-.Lfunc_begin0
	.hword	1
	.byte	88
	.xword	.Ltmp27-.Lfunc_begin0
	.xword	.Ltmp31-.Lfunc_begin0
	.hword	1
	.byte	102
	.xword	0
	.xword	0
.Ldebug_loc11:
	.xword	.Ltmp13-.Lfunc_begin0
	.xword	.Ltmp19-.Lfunc_begin0
	.hword	1
	.byte	99
	.xword	.Ltmp25-.Lfunc_begin0
	.xword	.Ltmp31-.Lfunc_begin0
	.hword	1
	.byte	99
	.xword	0
	.xword	0
.Ldebug_loc12:
	.xword	.Ltmp34-.Lfunc_begin0
	.xword	.Ltmp41-.Lfunc_begin0
	.hword	1
	.byte	99
	.xword	.Ltmp44-.Lfunc_begin0
	.xword	.Ltmp51-.Lfunc_begin0
	.hword	1
	.byte	99
	.xword	0
	.xword	0
.Ldebug_loc13:
	.xword	.Ltmp34-.Lfunc_begin0
	.xword	.Ltmp35-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	0
	.byte	159
	.xword	.Ltmp35-.Lfunc_begin0
	.xword	.Ltmp41-.Lfunc_begin0
	.hword	1
	.byte	88
	.xword	.Ltmp44-.Lfunc_begin0
	.xword	.Ltmp48-.Lfunc_begin0
	.hword	1
	.byte	88
	.xword	0
	.xword	0
.Ldebug_loc14:
	.xword	.Ltmp34-.Lfunc_begin0
	.xword	.Ltmp41-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	127
	.byte	159
	.xword	.Ltmp44-.Lfunc_begin0
	.xword	.Ltmp47-.Lfunc_begin0
	.hword	1
	.byte	90
	.xword	.Ltmp47-.Lfunc_begin0
	.xword	.Ltmp51-.Lfunc_begin0
	.hword	1
	.byte	101
	.xword	0
	.xword	0
.Ldebug_loc15:
	.xword	.Ltmp34-.Lfunc_begin0
	.xword	.Ltmp37-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	0
	.byte	159
	.xword	.Ltmp37-.Lfunc_begin0
	.xword	.Ltmp41-.Lfunc_begin0
	.hword	1
	.byte	89
	.xword	0
	.xword	0
.Ldebug_loc16:
	.xword	.Ltmp58-.Lfunc_begin0
	.xword	.Ltmp73-.Lfunc_begin0
	.hword	1
	.byte	99
	.xword	0
	.xword	0
.Ldebug_loc17:
	.xword	.Ltmp58-.Lfunc_begin0
	.xword	.Ltmp70-.Lfunc_begin0
	.hword	1
	.byte	88
	.xword	0
	.xword	0
.Ldebug_loc18:
	.xword	.Ltmp58-.Lfunc_begin0
	.xword	.Ltmp60-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	0
	.byte	159
	.xword	.Ltmp60-.Lfunc_begin0
	.xword	.Ltmp65-.Lfunc_begin0
	.hword	1
	.byte	89
	.xword	.Ltmp66-.Lfunc_begin0
	.xword	.Ltmp67-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	0
	.byte	159
	.xword	.Ltmp67-.Lfunc_begin0
	.xword	.Ltmp68-.Lfunc_begin0
	.hword	1
	.byte	89
	.xword	0
	.xword	0
.Ldebug_loc19:
	.xword	.Ltmp58-.Lfunc_begin0
	.xword	.Ltmp68-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	127
	.byte	159
	.xword	.Ltmp68-.Lfunc_begin0
	.xword	.Ltmp69-.Lfunc_begin0
	.hword	1
	.byte	90
	.xword	.Ltmp69-.Lfunc_begin0
	.xword	.Ltmp73-.Lfunc_begin0
	.hword	1
	.byte	101
	.xword	0
	.xword	0
.Ldebug_loc20:
	.xword	.Lfunc_begin1-.Lfunc_begin0
	.xword	.Ltmp96-.Lfunc_begin0
	.hword	1
	.byte	80
	.xword	.Ltmp96-.Lfunc_begin0
	.xword	.Ltmp97-.Lfunc_begin0
	.hword	4
	.byte	243
	.byte	1
	.byte	80
	.byte	159
	.xword	.Ltmp97-.Lfunc_begin0
	.xword	.Ltmp98-.Lfunc_begin0
	.hword	1
	.byte	80
	.xword	.Ltmp98-.Lfunc_begin0
	.xword	.Lfunc_end1-.Lfunc_begin0
	.hword	4
	.byte	243
	.byte	1
	.byte	80
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc21:
	.xword	.Ltmp80-.Lfunc_begin0
	.xword	.Ltmp95-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	.Ltmp95-.Lfunc_begin0
	.xword	.Ltmp96-.Lfunc_begin0
	.hword	1
	.byte	89
	.xword	.Ltmp97-.Lfunc_begin0
	.xword	.Lfunc_end1-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc22:
	.xword	.Ltmp80-.Lfunc_begin0
	.xword	.Ltmp86-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	0
	.byte	159
	.xword	.Ltmp97-.Lfunc_begin0
	.xword	.Lfunc_end1-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	0
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc23:
	.xword	.Ltmp84-.Lfunc_begin0
	.xword	.Ltmp85-.Lfunc_begin0
	.hword	1
	.byte	88
	.xword	0
	.xword	0
.Ldebug_loc24:
	.xword	.Lfunc_begin2-.Lfunc_begin0
	.xword	.Ltmp117-.Lfunc_begin0
	.hword	1
	.byte	80
	.xword	.Ltmp117-.Lfunc_begin0
	.xword	.Ltmp236-.Lfunc_begin0
	.hword	4
	.byte	243
	.byte	1
	.byte	80
	.byte	159
	.xword	.Ltmp236-.Lfunc_begin0
	.xword	.Ltmp237-.Lfunc_begin0
	.hword	1
	.byte	80
	.xword	.Ltmp237-.Lfunc_begin0
	.xword	.Ltmp239-.Lfunc_begin0
	.hword	4
	.byte	243
	.byte	1
	.byte	80
	.byte	159
	.xword	.Ltmp239-.Lfunc_begin0
	.xword	.Ltmp240-.Lfunc_begin0
	.hword	1
	.byte	80
	.xword	.Ltmp240-.Lfunc_begin0
	.xword	.Ltmp244-.Lfunc_begin0
	.hword	4
	.byte	243
	.byte	1
	.byte	80
	.byte	159
	.xword	.Ltmp244-.Lfunc_begin0
	.xword	.Ltmp245-.Lfunc_begin0
	.hword	1
	.byte	80
	.xword	.Ltmp245-.Lfunc_begin0
	.xword	.Lfunc_end2-.Lfunc_begin0
	.hword	4
	.byte	243
	.byte	1
	.byte	80
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc25:
	.xword	.Lfunc_begin2-.Lfunc_begin0
	.xword	.Ltmp123-.Lfunc_begin0
	.hword	1
	.byte	81
	.xword	.Ltmp132-.Lfunc_begin0
	.xword	.Ltmp139-.Lfunc_begin0
	.hword	4
	.byte	243
	.byte	1
	.byte	81
	.byte	159
	.xword	.Ltmp148-.Lfunc_begin0
	.xword	.Ltmp156-.Lfunc_begin0
	.hword	4
	.byte	243
	.byte	1
	.byte	81
	.byte	159
	.xword	.Ltmp236-.Lfunc_begin0
	.xword	.Ltmp238-.Lfunc_begin0
	.hword	1
	.byte	81
	.xword	.Ltmp238-.Lfunc_begin0
	.xword	.Ltmp239-.Lfunc_begin0
	.hword	4
	.byte	243
	.byte	1
	.byte	81
	.byte	159
	.xword	.Ltmp239-.Lfunc_begin0
	.xword	.Ltmp241-.Lfunc_begin0
	.hword	1
	.byte	81
	.xword	.Ltmp241-.Lfunc_begin0
	.xword	.Ltmp244-.Lfunc_begin0
	.hword	4
	.byte	243
	.byte	1
	.byte	81
	.byte	159
	.xword	.Ltmp244-.Lfunc_begin0
	.xword	.Ltmp246-.Lfunc_begin0
	.hword	1
	.byte	81
	.xword	.Ltmp246-.Lfunc_begin0
	.xword	.Ltmp247-.Lfunc_begin0
	.hword	4
	.byte	243
	.byte	1
	.byte	81
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc26:
	.xword	.Ltmp101-.Lfunc_begin0
	.xword	.Ltmp102-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	.Ltmp102-.Lfunc_begin0
	.xword	.Ltmp120-.Lfunc_begin0
	.hword	1
	.byte	94
	.xword	.Ltmp236-.Lfunc_begin0
	.xword	.Ltmp243-.Lfunc_begin0
	.hword	1
	.byte	94
	.xword	.Ltmp244-.Lfunc_begin0
	.xword	.Ltmp246-.Lfunc_begin0
	.hword	1
	.byte	94
	.xword	0
	.xword	0
.Ldebug_loc27:
	.xword	.Ltmp102-.Lfunc_begin0
	.xword	.Ltmp103-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	0
	.byte	159
	.xword	.Ltmp103-.Lfunc_begin0
	.xword	.Ltmp104-.Lfunc_begin0
	.hword	1
	.byte	88
	.xword	.Ltmp109-.Lfunc_begin0
	.xword	.Ltmp110-.Lfunc_begin0
	.hword	1
	.byte	88
	.xword	.Ltmp119-.Lfunc_begin0
	.xword	.Ltmp121-.Lfunc_begin0
	.hword	1
	.byte	95
	.xword	0
	.xword	0
.Ldebug_loc28:
	.xword	.Ltmp102-.Lfunc_begin0
	.xword	.Ltmp105-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	.Ltmp105-.Lfunc_begin0
	.xword	.Ltmp108-.Lfunc_begin0
	.hword	1
	.byte	88
	.xword	.Ltmp111-.Lfunc_begin0
	.xword	.Ltmp113-.Lfunc_begin0
	.hword	1
	.byte	88
	.xword	.Ltmp120-.Lfunc_begin0
	.xword	.Ltmp176-.Lfunc_begin0
	.hword	1
	.byte	94
	.xword	.Ltmp236-.Lfunc_begin0
	.xword	.Ltmp243-.Lfunc_begin0
	.hword	1
	.byte	88
	.xword	0
	.xword	0
.Ldebug_loc29:
	.xword	.Ltmp124-.Lfunc_begin0
	.xword	.Ltmp176-.Lfunc_begin0
	.hword	1
	.byte	94
	.xword	0
	.xword	0
.Ldebug_loc30:
	.xword	.Ltmp113-.Lfunc_begin0
	.xword	.Ltmp123-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	.Ltmp125-.Lfunc_begin0
	.xword	.Ltmp131-.Lfunc_begin0
	.hword	1
	.byte	93
	.xword	.Ltmp131-.Lfunc_begin0
	.xword	.Ltmp133-.Lfunc_begin0
	.hword	1
	.byte	80
	.xword	.Ltmp134-.Lfunc_begin0
	.xword	.Ltmp135-.Lfunc_begin0
	.hword	1
	.byte	82
	.xword	.Ltmp135-.Lfunc_begin0
	.xword	.Ltmp136-.Lfunc_begin0
	.hword	1
	.byte	80
	.xword	.Ltmp137-.Lfunc_begin0
	.xword	.Ltmp138-.Lfunc_begin0
	.hword	1
	.byte	93
	.xword	.Ltmp138-.Lfunc_begin0
	.xword	.Ltmp143-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	.Ltmp143-.Lfunc_begin0
	.xword	.Ltmp147-.Lfunc_begin0
	.hword	1
	.byte	93
	.xword	.Ltmp147-.Lfunc_begin0
	.xword	.Ltmp149-.Lfunc_begin0
	.hword	1
	.byte	80
	.xword	.Ltmp150-.Lfunc_begin0
	.xword	.Ltmp151-.Lfunc_begin0
	.hword	1
	.byte	82
	.xword	.Ltmp151-.Lfunc_begin0
	.xword	.Ltmp152-.Lfunc_begin0
	.hword	1
	.byte	85
	.xword	.Ltmp154-.Lfunc_begin0
	.xword	.Ltmp156-.Lfunc_begin0
	.hword	1
	.byte	93
	.xword	.Ltmp156-.Lfunc_begin0
	.xword	.Ltmp159-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	.Ltmp160-.Lfunc_begin0
	.xword	.Ltmp166-.Lfunc_begin0
	.hword	1
	.byte	93
	.xword	.Ltmp166-.Lfunc_begin0
	.xword	.Ltmp167-.Lfunc_begin0
	.hword	1
	.byte	85
	.xword	.Ltmp167-.Lfunc_begin0
	.xword	.Ltmp168-.Lfunc_begin0
	.hword	1
	.byte	103
	.xword	.Ltmp168-.Lfunc_begin0
	.xword	.Ltmp169-.Lfunc_begin0
	.hword	1
	.byte	104
	.xword	.Ltmp170-.Lfunc_begin0
	.xword	.Ltmp171-.Lfunc_begin0
	.hword	1
	.byte	93
	.xword	.Ltmp171-.Lfunc_begin0
	.xword	.Ltmp175-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	.Ltmp175-.Lfunc_begin0
	.xword	.Ltmp176-.Lfunc_begin0
	.hword	1
	.byte	93
	.xword	.Ltmp182-.Lfunc_begin0
	.xword	.Ltmp183-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	.Ltmp183-.Lfunc_begin0
	.xword	.Ltmp218-.Lfunc_begin0
	.hword	10
	.byte	122
	.byte	0
	.byte	17
	.byte	128
	.byte	8
	.byte	27
	.byte	17
	.byte	2
	.byte	30
	.byte	159
	.xword	.Ltmp218-.Lfunc_begin0
	.xword	.Ltmp222-.Lfunc_begin0
	.hword	13
	.byte	122
	.byte	0
	.byte	17
	.byte	128
	.byte	8
	.byte	27
	.byte	17
	.byte	2
	.byte	30
	.byte	17
	.byte	2
	.byte	34
	.byte	159
	.xword	.Ltmp244-.Lfunc_begin0
	.xword	.Ltmp246-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc31:
	.xword	.Ltmp113-.Lfunc_begin0
	.xword	.Ltmp123-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	.Ltmp125-.Lfunc_begin0
	.xword	.Ltmp140-.Lfunc_begin0
	.hword	1
	.byte	92
	.xword	.Ltmp140-.Lfunc_begin0
	.xword	.Ltmp143-.Lfunc_begin0
	.hword	1
	.byte	81
	.xword	.Ltmp143-.Lfunc_begin0
	.xword	.Ltmp157-.Lfunc_begin0
	.hword	1
	.byte	92
	.xword	.Ltmp157-.Lfunc_begin0
	.xword	.Ltmp159-.Lfunc_begin0
	.hword	1
	.byte	84
	.xword	.Ltmp160-.Lfunc_begin0
	.xword	.Ltmp172-.Lfunc_begin0
	.hword	1
	.byte	92
	.xword	.Ltmp172-.Lfunc_begin0
	.xword	.Ltmp175-.Lfunc_begin0
	.hword	1
	.byte	85
	.xword	.Ltmp175-.Lfunc_begin0
	.xword	.Ltmp176-.Lfunc_begin0
	.hword	1
	.byte	92
	.xword	.Ltmp183-.Lfunc_begin0
	.xword	.Ltmp187-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	.Ltmp187-.Lfunc_begin0
	.xword	.Ltmp189-.Lfunc_begin0
	.hword	2
	.byte	50
	.byte	159
	.xword	.Ltmp189-.Lfunc_begin0
	.xword	.Ltmp191-.Lfunc_begin0
	.hword	2
	.byte	52
	.byte	159
	.xword	.Ltmp191-.Lfunc_begin0
	.xword	.Ltmp193-.Lfunc_begin0
	.hword	2
	.byte	54
	.byte	159
	.xword	.Ltmp193-.Lfunc_begin0
	.xword	.Ltmp195-.Lfunc_begin0
	.hword	2
	.byte	56
	.byte	159
	.xword	.Ltmp195-.Lfunc_begin0
	.xword	.Ltmp197-.Lfunc_begin0
	.hword	2
	.byte	58
	.byte	159
	.xword	.Ltmp197-.Lfunc_begin0
	.xword	.Ltmp199-.Lfunc_begin0
	.hword	2
	.byte	60
	.byte	159
	.xword	.Ltmp199-.Lfunc_begin0
	.xword	.Ltmp201-.Lfunc_begin0
	.hword	2
	.byte	62
	.byte	159
	.xword	.Ltmp201-.Lfunc_begin0
	.xword	.Ltmp203-.Lfunc_begin0
	.hword	2
	.byte	64
	.byte	159
	.xword	.Ltmp203-.Lfunc_begin0
	.xword	.Ltmp205-.Lfunc_begin0
	.hword	2
	.byte	66
	.byte	159
	.xword	.Ltmp205-.Lfunc_begin0
	.xword	.Ltmp207-.Lfunc_begin0
	.hword	2
	.byte	68
	.byte	159
	.xword	.Ltmp207-.Lfunc_begin0
	.xword	.Ltmp209-.Lfunc_begin0
	.hword	2
	.byte	70
	.byte	159
	.xword	.Ltmp209-.Lfunc_begin0
	.xword	.Ltmp211-.Lfunc_begin0
	.hword	2
	.byte	72
	.byte	159
	.xword	.Ltmp211-.Lfunc_begin0
	.xword	.Ltmp213-.Lfunc_begin0
	.hword	2
	.byte	74
	.byte	159
	.xword	.Ltmp213-.Lfunc_begin0
	.xword	.Ltmp215-.Lfunc_begin0
	.hword	2
	.byte	76
	.byte	159
	.xword	.Ltmp215-.Lfunc_begin0
	.xword	.Ltmp218-.Lfunc_begin0
	.hword	2
	.byte	78
	.byte	159
	.xword	.Ltmp218-.Lfunc_begin0
	.xword	.Ltmp223-.Lfunc_begin0
	.hword	3
	.byte	16
	.byte	32
	.byte	159
	.xword	.Ltmp244-.Lfunc_begin0
	.xword	.Ltmp246-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc32:
	.xword	.Ltmp113-.Lfunc_begin0
	.xword	.Ltmp123-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	.Ltmp125-.Lfunc_begin0
	.xword	.Ltmp142-.Lfunc_begin0
	.hword	1
	.byte	99
	.xword	.Ltmp143-.Lfunc_begin0
	.xword	.Ltmp159-.Lfunc_begin0
	.hword	1
	.byte	99
	.xword	.Ltmp160-.Lfunc_begin0
	.xword	.Ltmp174-.Lfunc_begin0
	.hword	1
	.byte	99
	.xword	.Ltmp175-.Lfunc_begin0
	.xword	.Ltmp236-.Lfunc_begin0
	.hword	1
	.byte	99
	.xword	.Ltmp244-.Lfunc_begin0
	.xword	.Ltmp246-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc33:
	.xword	.Ltmp113-.Lfunc_begin0
	.xword	.Ltmp123-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	.Ltmp125-.Lfunc_begin0
	.xword	.Ltmp138-.Lfunc_begin0
	.hword	1
	.byte	91
	.xword	.Ltmp143-.Lfunc_begin0
	.xword	.Ltmp155-.Lfunc_begin0
	.hword	1
	.byte	91
	.xword	.Ltmp160-.Lfunc_begin0
	.xword	.Ltmp176-.Lfunc_begin0
	.hword	1
	.byte	91
	.xword	.Ltmp244-.Lfunc_begin0
	.xword	.Ltmp246-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc34:
	.xword	.Ltmp113-.Lfunc_begin0
	.xword	.Ltmp123-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	.Ltmp130-.Lfunc_begin0
	.xword	.Ltmp143-.Lfunc_begin0
	.hword	1
	.byte	83
	.xword	.Ltmp146-.Lfunc_begin0
	.xword	.Ltmp153-.Lfunc_begin0
	.hword	1
	.byte	83
	.xword	.Ltmp184-.Lfunc_begin0
	.xword	.Ltmp223-.Lfunc_begin0
	.hword	1
	.byte	97
	.xword	.Ltmp244-.Lfunc_begin0
	.xword	.Ltmp246-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc35:
	.xword	.Ltmp113-.Lfunc_begin0
	.xword	.Ltmp123-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	.Ltmp162-.Lfunc_begin0
	.xword	.Ltmp175-.Lfunc_begin0
	.hword	1
	.byte	84
	.xword	.Ltmp179-.Lfunc_begin0
	.xword	.Ltmp181-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	.Ltmp181-.Lfunc_begin0
	.xword	.Ltmp223-.Lfunc_begin0
	.hword	1
	.byte	91
	.xword	.Ltmp225-.Lfunc_begin0
	.xword	.Ltmp233-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	.Ltmp244-.Lfunc_begin0
	.xword	.Ltmp246-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc36:
	.xword	.Ltmp113-.Lfunc_begin0
	.xword	.Ltmp122-.Lfunc_begin0
	.hword	2
	.byte	49
	.byte	159
	.xword	.Ltmp143-.Lfunc_begin0
	.xword	.Ltmp144-.Lfunc_begin0
	.hword	9
	.byte	112
	.byte	0
	.byte	48
	.byte	32
	.byte	39
	.byte	16
	.byte	1
	.byte	26
	.byte	159
	.xword	.Ltmp244-.Lfunc_begin0
	.xword	.Ltmp246-.Lfunc_begin0
	.hword	2
	.byte	49
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc37:
	.xword	.Ltmp113-.Lfunc_begin0
	.xword	.Ltmp122-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	0
	.byte	159
	.xword	.Ltmp122-.Lfunc_begin0
	.xword	.Ltmp123-.Lfunc_begin0
	.hword	1
	.byte	95
	.xword	.Ltmp124-.Lfunc_begin0
	.xword	.Ltmp176-.Lfunc_begin0
	.hword	1
	.byte	95
	.xword	.Ltmp244-.Lfunc_begin0
	.xword	.Ltmp246-.Lfunc_begin0
	.hword	3
	.byte	17
	.byte	0
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc38:
	.xword	.Lfunc_begin3-.Lfunc_begin0
	.xword	.Ltmp252-.Lfunc_begin0
	.hword	1
	.byte	80
	.xword	.Ltmp253-.Lfunc_begin0
	.xword	.Ltmp254-.Lfunc_begin0
	.hword	1
	.byte	80
	.xword	.Ltmp254-.Lfunc_begin0
	.xword	.Lfunc_end3-.Lfunc_begin0
	.hword	1
	.byte	99
	.xword	0
	.xword	0
.Ldebug_loc39:
	.xword	.Ltmp249-.Lfunc_begin0
	.xword	.Ltmp250-.Lfunc_begin0
	.hword	1
	.byte	80
	.xword	.Ltmp250-.Lfunc_begin0
	.xword	.Lfunc_end3-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	0
	.xword	0
.Ldebug_loc40:
	.xword	.Lfunc_begin4-.Lfunc_begin0
	.xword	.Ltmp262-.Lfunc_begin0
	.hword	2
	.byte	48
	.byte	159
	.xword	.Ltmp262-.Lfunc_begin0
	.xword	.Ltmp264-.Lfunc_begin0
	.hword	1
	.byte	88
	.xword	.Ltmp265-.Lfunc_begin0
	.xword	.Lfunc_end4-.Lfunc_begin0
	.hword	1
	.byte	88
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
	.byte	52
	.byte	0
	.byte	3
	.byte	14
	.byte	73
	.byte	19
	.byte	63
	.byte	25
	.byte	58
	.byte	11
	.byte	59
	.byte	11
	.byte	2
	.byte	24
	.byte	0
	.byte	0
	.byte	3
	.byte	1
	.byte	1
	.byte	73
	.byte	19
	.byte	0
	.byte	0
	.byte	4
	.byte	33
	.byte	0
	.byte	73
	.byte	19
	.byte	55
	.byte	11
	.byte	0
	.byte	0
	.byte	5
	.byte	38
	.byte	0
	.byte	73
	.byte	19
	.byte	0
	.byte	0
	.byte	6
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
	.byte	7
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
	.byte	8
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
	.byte	9
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
	.byte	10
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
	.byte	11
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
	.byte	12
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
	.byte	13
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
	.byte	14
	.byte	5
	.byte	0
	.byte	2
	.byte	23
	.byte	49
	.byte	19
	.byte	0
	.byte	0
	.byte	15
	.byte	52
	.byte	0
	.byte	2
	.byte	23
	.byte	49
	.byte	19
	.byte	0
	.byte	0
	.byte	16
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
	.ascii	"\211\202\001"
	.byte	0
	.byte	49
	.byte	19
	.byte	17
	.byte	1
	.byte	0
	.byte	0
	.byte	20
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
	.byte	21
	.byte	40
	.byte	0
	.byte	3
	.byte	14
	.byte	28
	.byte	15
	.byte	0
	.byte	0
	.byte	22
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
	.byte	23
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
	.byte	24
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
	.byte	25
	.byte	15
	.byte	0
	.byte	73
	.byte	19
	.byte	0
	.byte	0
	.byte	26
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
	.byte	27
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
	.byte	28
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
	.byte	29
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
	.byte	30
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
	.byte	31
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
	.byte	32
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
	.byte	0
	.byte	0
	.byte	33
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
	.byte	34
	.byte	52
	.byte	0
	.byte	28
	.byte	15
	.byte	49
	.byte	19
	.byte	0
	.byte	0
	.byte	35
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
	.byte	36
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
	.byte	37
	.byte	15
	.byte	0
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
	.word	.Linfo_string3
	.word	63

	.byte	3
	.byte	153
	.byte	9
	.byte	3
	.xword	pvi_mode_table
	.byte	3
	.word	81
	.byte	4
	.word	115
	.byte	8
	.byte	4
	.word	115
	.byte	10
	.byte	0
	.byte	5
	.word	86
	.byte	6
	.word	97
	.word	.Linfo_string6
	.byte	2
	.byte	17
	.byte	6
	.word	108
	.word	.Linfo_string5
	.byte	1
	.byte	21
	.byte	7
	.word	.Linfo_string4
	.byte	8
	.byte	1
	.byte	8
	.word	.Linfo_string7
	.byte	8
	.byte	7
	.byte	9
	.xword	.Lfunc_begin0
	.word	.Lfunc_end0-.Lfunc_begin0
	.byte	1
	.byte	109

	.word	.Linfo_string82
	.byte	3
	.byte	242

	.word	691

	.byte	10
	.word	.Linfo_string8
	.word	593
	.byte	3
	.byte	244
	.byte	9
	.byte	3
	.xword	pvi_wf_get_lut.stype
	.byte	10
	.word	.Linfo_string24
	.word	691
	.byte	3
	.byte	245
	.byte	9
	.byte	3
	.xword	pvi_wf_get_lut.sftemp
	.byte	11
	.word	.Ldebug_loc0
	.word	.Linfo_string87
	.byte	3
	.byte	242
	.word	1899
	.byte	11
	.word	.Ldebug_loc1
	.word	.Linfo_string92
	.byte	3
	.byte	242
	.word	593
	.byte	11
	.word	.Ldebug_loc2
	.word	.Linfo_string57
	.byte	3
	.byte	242
	.word	691
	.byte	11
	.word	.Ldebug_loc3
	.word	.Linfo_string93
	.byte	3
	.byte	242
	.word	691
	.byte	12
	.word	.Ldebug_loc4
	.word	.Linfo_string94
	.byte	3
	.byte	249
	.word	593
	.byte	12
	.word	.Ldebug_loc5
	.word	.Linfo_string88
	.byte	3
	.byte	248
	.word	691
	.byte	12
	.word	.Ldebug_loc6
	.word	.Linfo_string95
	.byte	3
	.byte	247
	.word	691
	.byte	12
	.word	.Ldebug_loc7
	.word	.Linfo_string96
	.byte	3
	.byte	246
	.word	691
	.byte	13
	.word	1308
	.word	.Ldebug_ranges0
	.byte	3
	.hword	267
	.byte	16
	.byte	14
	.word	.Ldebug_loc11
	.word	1320
	.byte	15
	.word	.Ldebug_loc8
	.word	1331
	.byte	15
	.word	.Ldebug_loc9
	.word	1342
	.byte	15
	.word	.Ldebug_loc10
	.word	1353
	.byte	0
	.byte	13
	.word	1308
	.word	.Ldebug_ranges1
	.byte	3
	.hword	279
	.byte	16
	.byte	14
	.word	.Ldebug_loc12
	.word	1320
	.byte	15
	.word	.Ldebug_loc13
	.word	1331
	.byte	15
	.word	.Ldebug_loc14
	.word	1353
	.byte	15
	.word	.Ldebug_loc15
	.word	1342
	.byte	0
	.byte	16
	.word	1308
	.xword	.Ltmp57
	.word	.Ltmp69-.Ltmp57
	.byte	3
	.hword	293
	.byte	16
	.byte	14
	.word	.Ldebug_loc16
	.word	1320
	.byte	15
	.word	.Ldebug_loc17
	.word	1331
	.byte	15
	.word	.Ldebug_loc18
	.word	1342
	.byte	15
	.word	.Ldebug_loc19
	.word	1353
	.byte	0
	.byte	17
	.word	1365
	.xword	.Ltmp28
	.byte	18
	.byte	1
	.byte	80
	.byte	2
	.byte	133
	.byte	0
	.byte	0
	.byte	17
	.word	1464
	.xword	.Ltmp30
	.byte	18
	.byte	1
	.byte	81
	.byte	2
	.byte	134
	.byte	0
	.byte	0
	.byte	17
	.word	1365
	.xword	.Ltmp48
	.byte	18
	.byte	1
	.byte	80
	.byte	1
	.byte	55
	.byte	0
	.byte	17
	.word	1464
	.xword	.Ltmp50
	.byte	18
	.byte	1
	.byte	81
	.byte	2
	.byte	133
	.byte	0
	.byte	0
	.byte	19
	.word	1659
	.xword	.Ltmp57
	.byte	19
	.word	1365
	.xword	.Ltmp70
	.byte	17
	.word	1464
	.xword	.Ltmp72
	.byte	18
	.byte	1
	.byte	81
	.byte	2
	.byte	133
	.byte	0
	.byte	0
	.byte	0
	.byte	20
	.word	684
	.word	.Linfo_string23
	.byte	4
	.byte	4
	.byte	15
	.byte	21
	.word	.Linfo_string10
	.byte	0
	.byte	21
	.word	.Linfo_string11
	.byte	1
	.byte	21
	.word	.Linfo_string12
	.byte	2
	.byte	21
	.word	.Linfo_string13
	.byte	3
	.byte	21
	.word	.Linfo_string14
	.byte	4
	.byte	21
	.word	.Linfo_string15
	.byte	5
	.byte	21
	.word	.Linfo_string16
	.byte	6
	.byte	21
	.word	.Linfo_string17
	.byte	7
	.byte	21
	.word	.Linfo_string18
	.byte	8
	.byte	21
	.word	.Linfo_string19
	.byte	9
	.byte	21
	.word	.Linfo_string20
	.byte	10
	.byte	21
	.word	.Linfo_string21
	.byte	11
	.byte	21
	.word	.Linfo_string22
	.byte	12
	.byte	0
	.byte	7
	.word	.Linfo_string9
	.byte	7
	.byte	4
	.byte	7
	.word	.Linfo_string25
	.byte	5
	.byte	4
	.byte	22
	.xword	.Lfunc_begin4
	.word	.Lfunc_end4-.Lfunc_begin4
	.byte	1
	.byte	111

	.word	.Linfo_string86
	.byte	3
	.hword	355

	.word	1288

	.byte	23
	.word	.Linfo_string26
	.word	763
	.byte	3
	.hword	358
	.byte	9
	.byte	3
	.xword	pvi_wf_get_version.spi_id_buffer
	.byte	24
	.word	.Ldebug_loc40
	.word	.Linfo_string69
	.byte	3
	.hword	357
	.word	1288
	.byte	0
	.byte	3
	.word	775
	.byte	4
	.word	115
	.byte	32
	.byte	0
	.byte	7
	.word	.Linfo_string27
	.byte	8
	.byte	1
	.byte	10
	.word	.Linfo_string28
	.word	803
	.byte	3
	.byte	40
	.byte	9
	.byte	3
	.xword	waveformdata
	.byte	25
	.word	86
	.byte	10
	.word	.Linfo_string29
	.word	829
	.byte	3
	.byte	42
	.byte	9
	.byte	3
	.xword	global_waveform
	.byte	25
	.word	834
	.byte	26
	.word	.Linfo_string58
	.byte	52
	.byte	3
	.byte	13
	.byte	27
	.word	.Linfo_string30
	.word	1119
	.byte	3
	.byte	14
	.byte	0
	.byte	27
	.word	.Linfo_string33
	.word	1119
	.byte	3
	.byte	15
	.byte	4
	.byte	27
	.word	.Linfo_string34
	.word	1119
	.byte	3
	.byte	16
	.byte	8
	.byte	27
	.word	.Linfo_string35
	.word	86
	.byte	3
	.byte	17
	.byte	12
	.byte	27
	.word	.Linfo_string36
	.word	86
	.byte	3
	.byte	18
	.byte	13
	.byte	27
	.word	.Linfo_string37
	.word	1141
	.byte	3
	.byte	19
	.byte	14
	.byte	27
	.word	.Linfo_string41
	.word	86
	.byte	3
	.byte	20
	.byte	16
	.byte	27
	.word	.Linfo_string42
	.word	86
	.byte	3
	.byte	21
	.byte	17
	.byte	27
	.word	.Linfo_string43
	.word	86
	.byte	3
	.byte	22
	.byte	18
	.byte	27
	.word	.Linfo_string44
	.word	86
	.byte	3
	.byte	23
	.byte	19
	.byte	27
	.word	.Linfo_string45
	.word	86
	.byte	3
	.byte	24
	.byte	20
	.byte	27
	.word	.Linfo_string46
	.word	86
	.byte	3
	.byte	25
	.byte	21
	.byte	27
	.word	.Linfo_string47
	.word	86
	.byte	3
	.byte	26
	.byte	22
	.byte	27
	.word	.Linfo_string48
	.word	86
	.byte	3
	.byte	27
	.byte	23
	.byte	27
	.word	.Linfo_string49
	.word	1119
	.byte	3
	.byte	28
	.byte	24
	.byte	27
	.word	.Linfo_string50
	.word	86
	.byte	3
	.byte	29
	.byte	28
	.byte	27
	.word	.Linfo_string51
	.word	1170
	.byte	3
	.byte	30
	.byte	29
	.byte	27
	.word	.Linfo_string52
	.word	86
	.byte	3
	.byte	31
	.byte	32
	.byte	27
	.word	.Linfo_string53
	.word	1182
	.byte	3
	.byte	32
	.byte	33
	.byte	27
	.word	.Linfo_string54
	.word	86
	.byte	3
	.byte	33
	.byte	37
	.byte	27
	.word	.Linfo_string55
	.word	86
	.byte	3
	.byte	34
	.byte	38
	.byte	27
	.word	.Linfo_string56
	.word	1194
	.byte	3
	.byte	35
	.byte	39
	.byte	27
	.word	.Linfo_string57
	.word	1206
	.byte	3
	.byte	36
	.byte	48
	.byte	0
	.byte	6
	.word	1130
	.word	.Linfo_string32
	.byte	2
	.byte	21
	.byte	6
	.word	684
	.word	.Linfo_string31
	.byte	1
	.byte	27
	.byte	6
	.word	1152
	.word	.Linfo_string40
	.byte	2
	.byte	19
	.byte	6
	.word	1163
	.word	.Linfo_string39
	.byte	1
	.byte	24
	.byte	7
	.word	.Linfo_string38
	.byte	7
	.byte	2
	.byte	3
	.word	86
	.byte	4
	.word	115
	.byte	3
	.byte	0
	.byte	3
	.word	86
	.byte	4
	.word	115
	.byte	4
	.byte	0
	.byte	3
	.word	86
	.byte	4
	.word	115
	.byte	9
	.byte	0
	.byte	3
	.word	86
	.byte	4
	.word	115
	.byte	1
	.byte	0
	.byte	10
	.word	.Linfo_string59
	.word	691
	.byte	3
	.byte	43
	.byte	9
	.byte	3
	.xword	need_pic
	.byte	10
	.word	.Linfo_string60
	.word	691
	.byte	3
	.byte	41
	.byte	16
	.byte	3
	.xword	maxpic
	.byte	148
	.byte	1
	.byte	64
	.byte	30
	.byte	64
	.byte	34
	.byte	159
	.byte	28
	.word	684
	.byte	4
	.byte	5
	.byte	10
	.byte	21
	.word	.Linfo_string61
	.byte	0
	.byte	21
	.word	.Linfo_string62
	.byte	1
	.byte	0
	.byte	25
	.word	1293
	.byte	5
	.word	775
	.byte	25
	.word	108
	.byte	25
	.word	691
	.byte	29
	.word	.Linfo_string63
	.byte	3
	.byte	199

	.word	691
	.byte	1
	.byte	30
	.word	.Linfo_string57
	.byte	3
	.byte	199
	.word	691
	.byte	31
	.word	.Linfo_string64
	.byte	3
	.byte	201
	.word	691
	.byte	31
	.word	.Linfo_string65
	.byte	3
	.byte	203
	.word	691
	.byte	31
	.word	.Linfo_string66
	.byte	3
	.byte	202
	.word	691
	.byte	0
	.byte	32
	.xword	.Lfunc_begin1
	.word	.Lfunc_end1-.Lfunc_begin1
	.byte	1
	.byte	109

	.word	.Linfo_string83
	.byte	3
	.byte	181

	.word	86
	.byte	11
	.word	.Ldebug_loc20
	.word	.Linfo_string92
	.byte	3
	.byte	181
	.word	593
	.byte	12
	.word	.Ldebug_loc21
	.word	.Linfo_string70
	.byte	3
	.byte	183
	.word	1705
	.byte	12
	.word	.Ldebug_loc22
	.word	.Linfo_string97
	.byte	3
	.byte	184
	.word	691
	.byte	33
	.word	1670
	.word	.Ldebug_ranges2
	.byte	3
	.byte	194
	.byte	14
	.byte	14
	.word	.Ldebug_loc23
	.word	1682
	.byte	34
	.byte	0
	.word	1693
	.byte	0
	.byte	0
	.byte	32
	.xword	.Lfunc_begin2
	.word	.Lfunc_end2-.Lfunc_begin2
	.byte	1
	.byte	109

	.word	.Linfo_string84
	.byte	3
	.byte	218

	.word	691
	.byte	11
	.word	.Ldebug_loc24
	.word	.Linfo_string97
	.byte	3
	.byte	218
	.word	691
	.byte	11
	.word	.Ldebug_loc25
	.word	.Linfo_string96
	.byte	3
	.byte	218
	.word	691
	.byte	12
	.word	.Ldebug_loc26
	.word	.Linfo_string69
	.byte	3
	.byte	220
	.word	1298
	.byte	12
	.word	.Ldebug_loc27
	.word	.Linfo_string76
	.byte	3
	.byte	222
	.word	691
	.byte	12
	.word	.Ldebug_loc28
	.word	.Linfo_string98
	.byte	3
	.byte	221
	.word	1298
	.byte	33
	.word	1710
	.word	.Ldebug_ranges3
	.byte	3
	.byte	239
	.byte	9
	.byte	14
	.word	.Ldebug_loc29
	.word	1722
	.byte	15
	.word	.Ldebug_loc30
	.word	1733
	.byte	15
	.word	.Ldebug_loc31
	.word	1744
	.byte	15
	.word	.Ldebug_loc32
	.word	1755
	.byte	15
	.word	.Ldebug_loc33
	.word	1766
	.byte	15
	.word	.Ldebug_loc34
	.word	1777
	.byte	15
	.word	.Ldebug_loc35
	.word	1788
	.byte	15
	.word	.Ldebug_loc36
	.word	1799
	.byte	15
	.word	.Ldebug_loc37
	.word	1810
	.byte	0
	.byte	0
	.byte	35
	.word	.Linfo_string67
	.byte	4
	.byte	48

	.word	691


	.byte	29
	.word	.Linfo_string68
	.byte	3
	.byte	164

	.word	1705
	.byte	1
	.byte	30
	.word	.Linfo_string69
	.byte	3
	.byte	164
	.word	829
	.byte	31
	.word	.Linfo_string70
	.byte	3
	.byte	166
	.word	1705
	.byte	0
	.byte	25
	.word	81
	.byte	29
	.word	.Linfo_string71
	.byte	3
	.byte	45

	.word	691
	.byte	1
	.byte	30
	.word	.Linfo_string72
	.byte	3
	.byte	45
	.word	1298
	.byte	31
	.word	.Linfo_string73
	.byte	3
	.byte	47
	.word	684
	.byte	31
	.word	.Linfo_string74
	.byte	3
	.byte	47
	.word	684
	.byte	31
	.word	.Linfo_string75
	.byte	3
	.byte	48
	.word	684
	.byte	31
	.word	.Linfo_string76
	.byte	3
	.byte	49
	.word	684
	.byte	31
	.word	.Linfo_string77
	.byte	3
	.byte	50
	.word	108
	.byte	31
	.word	.Linfo_string65
	.byte	3
	.byte	51
	.word	108
	.byte	31
	.word	.Linfo_string78
	.byte	3
	.byte	52
	.word	1822
	.byte	31
	.word	.Linfo_string81
	.byte	3
	.byte	53
	.word	691
	.byte	0
	.byte	6
	.word	1833
	.word	.Linfo_string80
	.byte	6
	.byte	30
	.byte	7
	.word	.Linfo_string79
	.byte	2
	.byte	1
	.byte	22
	.xword	.Lfunc_begin3
	.word	.Lfunc_end3-.Lfunc_begin3
	.byte	1
	.byte	109

	.word	.Linfo_string85
	.byte	3
	.hword	314

	.word	691

	.byte	36
	.word	.Ldebug_loc38
	.word	.Linfo_string99
	.byte	3
	.hword	314
	.word	1978
	.byte	24
	.word	.Ldebug_loc39
	.word	.Linfo_string100
	.byte	3
	.hword	316
	.word	829
	.byte	0
	.byte	25
	.word	1904
	.byte	26
	.word	.Linfo_string91
	.byte	40
	.byte	4
	.byte	32
	.byte	27
	.word	.Linfo_string88
	.word	684
	.byte	4
	.byte	33
	.byte	0
	.byte	27
	.word	.Linfo_string89
	.word	684
	.byte	4
	.byte	34
	.byte	4
	.byte	27
	.word	.Linfo_string72
	.word	1961
	.byte	4
	.byte	35
	.byte	8
	.byte	27
	.word	.Linfo_string90
	.word	1966
	.byte	4
	.byte	36
	.byte	16
	.byte	0
	.byte	25
	.word	684
	.byte	3
	.word	803
	.byte	4
	.word	115
	.byte	3
	.byte	0
	.byte	37
	.byte	0
.Ldebug_info_end0:
	.section	.debug_ranges,"",@progbits
.Ldebug_ranges0:
	.xword	.Ltmp11-.Lfunc_begin0
	.xword	.Ltmp12-.Lfunc_begin0
	.xword	.Ltmp13-.Lfunc_begin0
	.xword	.Ltmp19-.Lfunc_begin0
	.xword	.Ltmp26-.Lfunc_begin0
	.xword	.Ltmp27-.Lfunc_begin0
	.xword	0
	.xword	0
.Ldebug_ranges1:
	.xword	.Ltmp32-.Lfunc_begin0
	.xword	.Ltmp33-.Lfunc_begin0
	.xword	.Ltmp34-.Lfunc_begin0
	.xword	.Ltmp41-.Lfunc_begin0
	.xword	.Ltmp46-.Lfunc_begin0
	.xword	.Ltmp47-.Lfunc_begin0
	.xword	0
	.xword	0
.Ldebug_ranges2:
	.xword	.Ltmp84-.Lfunc_begin0
	.xword	.Ltmp85-.Lfunc_begin0
	.xword	.Ltmp86-.Lfunc_begin0
	.xword	.Ltmp87-.Lfunc_begin0
	.xword	0
	.xword	0
.Ldebug_ranges3:
	.xword	.Ltmp113-.Lfunc_begin0
	.xword	.Ltmp118-.Lfunc_begin0
	.xword	.Ltmp124-.Lfunc_begin0
	.xword	.Ltmp236-.Lfunc_begin0
	.xword	.Ltmp244-.Lfunc_begin0
	.xword	.Ltmp246-.Lfunc_begin0
	.xword	0
	.xword	0
	.section	.debug_str,"MS",@progbits,1
.Linfo_string0:
	.asciz	"Android (8508608, based on r450784e) clang version 14.0.7 (https://android.googlesource.com/toolchain/llvm-project 4c603efb0cca074e9238af8b4106c30add4418f6)"
.Linfo_string1:
	.asciz	"drivers/gpu/drm/rockchip/ebc-dev/epdlut/pvi_waveform.c"
.Linfo_string2:
	.asciz	"/home/lyx/rk-android14/kernel-6.1"
.Linfo_string3:
	.asciz	"pvi_mode_table"
.Linfo_string4:
	.asciz	"unsigned char"
.Linfo_string5:
	.asciz	"__u8"
.Linfo_string6:
	.asciz	"u8"
.Linfo_string7:
	.asciz	"__ARRAY_SIZE_TYPE__"
.Linfo_string8:
	.asciz	"stype"
.Linfo_string9:
	.asciz	"unsigned int"
.Linfo_string10:
	.asciz	"WF_TYPE_RESET"
.Linfo_string11:
	.asciz	"WF_TYPE_GRAY2"
.Linfo_string12:
	.asciz	"WF_TYPE_GRAY4"
.Linfo_string13:
	.asciz	"WF_TYPE_GC16"
.Linfo_string14:
	.asciz	"WF_TYPE_GL16"
.Linfo_string15:
	.asciz	"WF_TYPE_GLR16"
.Linfo_string16:
	.asciz	"WF_TYPE_GLD16"
.Linfo_string17:
	.asciz	"WF_TYPE_A2"
.Linfo_string18:
	.asciz	"WF_TYPE_GCC16"
.Linfo_string19:
	.asciz	"PVI_WF_MAX"
.Linfo_string20:
	.asciz	"WF_TYPE_AUTO"
.Linfo_string21:
	.asciz	"WF_TYPE_MAX"
.Linfo_string22:
	.asciz	"WF_TYPE_GRAY16"
.Linfo_string23:
	.asciz	"epd_lut_type"
.Linfo_string24:
	.asciz	"sftemp"
.Linfo_string25:
	.asciz	"int"
.Linfo_string26:
	.asciz	"spi_id_buffer"
.Linfo_string27:
	.asciz	"char"
.Linfo_string28:
	.asciz	"waveformdata"
.Linfo_string29:
	.asciz	"global_waveform"
.Linfo_string30:
	.asciz	"checksum"
.Linfo_string31:
	.asciz	"__u32"
.Linfo_string32:
	.asciz	"u32"
.Linfo_string33:
	.asciz	"file_length"
.Linfo_string34:
	.asciz	"serial"
.Linfo_string35:
	.asciz	"run_type"
.Linfo_string36:
	.asciz	"fpl_platform"
.Linfo_string37:
	.asciz	"fpl_lot"
.Linfo_string38:
	.asciz	"unsigned short"
.Linfo_string39:
	.asciz	"__u16"
.Linfo_string40:
	.asciz	"u16"
.Linfo_string41:
	.asciz	"mode_version"
.Linfo_string42:
	.asciz	"wf_version"
.Linfo_string43:
	.asciz	"wf_subversion"
.Linfo_string44:
	.asciz	"wf_type"
.Linfo_string45:
	.asciz	"panel_size"
.Linfo_string46:
	.asciz	"amepd_part_number"
.Linfo_string47:
	.asciz	"wfm_rev"
.Linfo_string48:
	.asciz	"frame_rate"
.Linfo_string49:
	.asciz	"reserved"
.Linfo_string50:
	.asciz	"vcom_offset"
.Linfo_string51:
	.asciz	"xwia"
.Linfo_string52:
	.asciz	"sc1"
.Linfo_string53:
	.asciz	"unknown1"
.Linfo_string54:
	.asciz	"modenumber"
.Linfo_string55:
	.asciz	"temperaturenumber"
.Linfo_string56:
	.asciz	"unknown2"
.Linfo_string57:
	.asciz	"temperture"
.Linfo_string58:
	.asciz	"pvi_waveform"
.Linfo_string59:
	.asciz	"need_pic"
.Linfo_string60:
	.asciz	"maxpic"
.Linfo_string61:
	.asciz	"false"
.Linfo_string62:
	.asciz	"true"
.Linfo_string63:
	.asciz	"get_wf_temp_index"
.Linfo_string64:
	.asciz	"temp_num"
.Linfo_string65:
	.asciz	"i"
.Linfo_string66:
	.asciz	"level"
.Linfo_string67:
	.asciz	"epd_overlay_lut"
.Linfo_string68:
	.asciz	"parse_mode_version"
.Linfo_string69:
	.asciz	"waveform"
.Linfo_string70:
	.asciz	"pvi_modes"
.Linfo_string71:
	.asciz	"decodewaveform"
.Linfo_string72:
	.asciz	"data"
.Linfo_string73:
	.asciz	"oldpic"
.Linfo_string74:
	.asciz	"newpic"
.Linfo_string75:
	.asciz	"frame_tmp"
.Linfo_string76:
	.asciz	"offset"
.Linfo_string77:
	.asciz	"tempdata"
.Linfo_string78:
	.asciz	"israr"
.Linfo_string79:
	.asciz	"_Bool"
.Linfo_string80:
	.asciz	"bool"
.Linfo_string81:
	.asciz	"shift_n"
.Linfo_string82:
	.asciz	"pvi_wf_get_lut"
.Linfo_string83:
	.asciz	"get_wf_mode_index"
.Linfo_string84:
	.asciz	"get_wf_frm_num"
.Linfo_string85:
	.asciz	"pvi_wf_input"
.Linfo_string86:
	.asciz	"pvi_wf_get_version"
.Linfo_string87:
	.asciz	"output"
.Linfo_string88:
	.asciz	"frame_num"
.Linfo_string89:
	.asciz	"repair_frame_num"
.Linfo_string90:
	.asciz	"wf_table"
.Linfo_string91:
	.asciz	"epd_lut_data"
.Linfo_string92:
	.asciz	"lut_type"
.Linfo_string93:
	.asciz	"pic"
.Linfo_string94:
	.asciz	"overlay_lut"
.Linfo_string95:
	.asciz	"mode_index"
.Linfo_string96:
	.asciz	"temp_index"
.Linfo_string97:
	.asciz	"mode"
.Linfo_string98:
	.asciz	"wf_data"
.Linfo_string99:
	.asciz	"waveform_file"
.Linfo_string100:
	.asciz	"pvi_wf"
	.ident	"Android (8508608, based on r450784e) clang version 14.0.7 (https://android.googlesource.com/toolchain/llvm-project 4c603efb0cca074e9238af8b4106c30add4418f6)"
	.section	".note.GNU-stack","",@progbits
	.addrsig
	.addrsig_sym pvi_wf_get_version.spi_id_buffer
	.section	.debug_line,"",@progbits
.Lline_table_start0:
