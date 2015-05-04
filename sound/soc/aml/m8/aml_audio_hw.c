/*
 * sound/soc/aml/m8/aml_audio_hw.c
 *
 * Copyright (C) 2015 Amlogic, Inc. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/clk.h>
#include <linux/module.h>

/* Amlogic headers */
#include <linux/amlogic/iomap.h>
#include <linux/amlogic/sound/aiu_regs.h>
#include <linux/amlogic/sound/audin_regs.h>
#include "aml_audio_hw.h"

unsigned ENABLE_IEC958 = 1;
unsigned IEC958_MODE = AIU_958_MODE_PCM16;
unsigned I2S_MODE = AIU_I2S_MODE_PCM16;

int audio_in_buf_ready = 0;
int audio_out_buf_ready = 0;

unsigned int IEC958_bpf = 0x7dd;
EXPORT_SYMBOL(IEC958_bpf);
unsigned int IEC958_brst = 0xc;
EXPORT_SYMBOL(IEC958_brst);
unsigned int IEC958_length = 0x7dd * 8;
EXPORT_SYMBOL(IEC958_length);
unsigned int IEC958_padsize = 0x8000;
EXPORT_SYMBOL(IEC958_padsize);
unsigned int IEC958_mode = 1;
EXPORT_SYMBOL(IEC958_mode);
unsigned int IEC958_syncword1 = 0x7ffe;
EXPORT_SYMBOL(IEC958_syncword1);
unsigned int IEC958_syncword2 = 0x8001;
EXPORT_SYMBOL(IEC958_syncword2);
unsigned int IEC958_syncword3 = 0;
EXPORT_SYMBOL(IEC958_syncword3);
unsigned int IEC958_syncword1_mask = 0;
EXPORT_SYMBOL(IEC958_syncword1_mask);
unsigned int IEC958_syncword2_mask = 0;
EXPORT_SYMBOL(IEC958_syncword2_mask);
unsigned int IEC958_syncword3_mask = 0xffff;
EXPORT_SYMBOL(IEC958_syncword3_mask);
unsigned int IEC958_chstat0_l = 0x1902;
EXPORT_SYMBOL(IEC958_chstat0_l);
unsigned int IEC958_chstat0_r = 0x1902;
EXPORT_SYMBOL(IEC958_chstat0_r);
unsigned int IEC958_chstat1_l = 0x200;
EXPORT_SYMBOL(IEC958_chstat1_l);
unsigned int IEC958_chstat1_r = 0x200;
EXPORT_SYMBOL(IEC958_chstat1_r);
unsigned int IEC958_mode_raw = 0;
EXPORT_SYMBOL(IEC958_mode_raw);

/*
bit 0:soc in slave mode for adc;
bit 1:audio in data source from spdif in;
bit 2:adc & spdif in work at the same time;
*/
unsigned audioin_mode = I2SIN_MASTER_MODE;

/* Bit 3:  mute constant */
/* 0 => 'h0000000 */
/* 1 => 'h800000 */
unsigned int dac_mute_const = 0x800000;

/*
				fIn * (M)
	    Fout   =  -----------------------------
			    (N) * (OD+1) * (XD)
*/
int audio_clock_config_table[][13][2] = {
	{
	 /* 256 */
#if OVERCLOCK == 0
	 {0x0005cc08, (60 - 1)},	/* 32 */
	 {0x0005e965, (40 - 1)},	/* 44.1 */
	 {0x0004c9a0, (50 - 1)},	/* 48K */
	 {0x0005cc08, (20 - 1)},	/* 96k ,24.576M */
	 {0x0005cc08, (10 - 1)},	/* 192k, 49.152M */
	 {0x0007f400, (125 - 1)},	/* 8k */
	 {0x0006c6f6, (116 - 1)},	/* 11.025 */
	 {0x0007e47f, (86 - 1)},	/* 12 */
	 {0x0004f880, (100 - 1)},	/* 16 */
	 {0x0004c4a4, (87 - 1)},	/* 22.05 */
	 {0x0007e47f, (43 - 1)},	/* 24 */
	 {0x0007f3f0, (127 - 1)},	/* 7875 */
	 {0x0005c88b, (22 - 1)},	/* 88.2k ,22.579M */
#else
	 /* 512FS */
	 {0x0004f880, (25 - 1)},	/* 32 */
	 {0x0004cdf3, (21 - 1)},	/* 44.1 */
	 {0x0006d0a4, (13 - 1)},	/* 48 */
	 {0x0004e15a, (9 - 1)},
	 {0x0006f207, (3 - 1)},
	 {0x0004f880, (100 - 1)},	/* 8k */
	 {0x0004c4a4, (87 - 1)},	/* 11.025 */
	 {0x0007e47f, (43 - 1)},	/* 12 */
	 {0x0004f880, (50 - 1)},	/* 16 */
	 {0x0004cdf3, (42 - 1)},	/* 22.05 */
	 {0x0007c4e6, (23 - 1)},	/* 24 */
	 {0x0006e1b6, (76 - 1)},	/* 7875 */
#endif
	 },
	{
	 /* 384 */
	 {0x0007c4e6, (23 - 1)},	/* 32 */
	 {0x0004c4a4, (29 - 1)},	/* 44.1 */
	 {0x0004cb18, (26 - 1)},	/* 48 */
	 {0x0004cb18, (13 - 1)},	/* 96 */
	 {0x0004e15a, (6 - 1)},
	 {0x0007e47f, (86 - 1)},	/* 8k */
	 {0x0007efa5, (61 - 1)},	/* 11.025 */
	 {0x0006de98, (67 - 1)},	/* 12 */
	 {0x0007e47f, (43 - 1)},	/* 16 */
	 {0x0004c4a4, (58 - 1)},	/* 22.05 */
	 {0x0004c60e, (53 - 1)},	/* 24 */
	 {0x0007fdfa, (83 - 1)},	/* 7875 */
	 }
};

void audio_set_aiubuf(u32 addr, u32 size, unsigned int channel)
{
	aml_write_cbus(AIU_MEM_I2S_START_PTR, addr & 0xffffffc0);
	aml_write_cbus(AIU_MEM_I2S_RD_PTR, addr & 0xffffffc0);
	if (channel == 8) {
		aml_cbus_update_bits(AIU_CLK_CTRL_MORE, 1 << 6, 1 << 6);
		aml_write_cbus(AIU_MEM_I2S_END_PTR,
			       (addr & 0xffffffc0) + (size & 0xffffffc0) - 256);
	} else {
		aml_cbus_update_bits(AIU_CLK_CTRL_MORE, 1 << 6, 0 << 6);
		aml_write_cbus(AIU_MEM_I2S_END_PTR,
				(addr & 0xffffffc0) + (size & 0xffffffc0) - 64);
	}
	/* Hold I2S */
	aml_write_cbus(AIU_I2S_MISC, 0x0004);
	/* No mute, no swap */
	aml_write_cbus(AIU_I2S_MUTE_SWAP, 0x0000);
	/* Release hold and force audio data to left or right */
	aml_write_cbus(AIU_I2S_MISC, 0x0010);

	if (channel == 8) {
		pr_info("%s channel == 8\n", __func__);
		/* [31:16] IRQ block. */
		aml_write_cbus(AIU_MEM_I2S_MASKS, (24 << 16) |
		/*
		*  [15: 8] chan_mem_mask.
		*  Each bit indicates which channels exist in memory
		*/
					(0xff << 8) |
		/*
		*  [ 7: 0] chan_rd_mask.
		*  Each bit indicates which channels are READ from memory
		*/
					(0xff << 0));
	} else
		/* [31:16] IRQ block. */
		aml_write_cbus(AIU_MEM_I2S_MASKS, (24 << 16) |
		/* [15: 8] chan_mem_mask.
		* Each bit indicates which channels exist in memory
		*/
					(0x3 << 8) |
		/*
		*  [ 7: 0] chan_rd_mask.
		*  Each bit indicates which channels are READ from memory
		*/
					(0x3 << 0));

	/* 16 bit PCM mode */
	/* aml_cbus_update_bits(AIU_MEM_I2S_CONTROL, 1, 6, 1); */
	/* Set init high then low to initilize the I2S memory logic */
	aml_cbus_update_bits(AIU_MEM_I2S_CONTROL, 1, 1);
	aml_cbus_update_bits(AIU_MEM_I2S_CONTROL, 1, 0);

	aml_write_cbus(AIU_MEM_I2S_BUF_CNTL, 1 | (0 << 1));
	aml_write_cbus(AIU_MEM_I2S_BUF_CNTL, 0 | (0 << 1));

	audio_out_buf_ready = 1;
}

void audio_set_958outbuf(u32 addr, u32 size, int flag)
{
	if (ENABLE_IEC958) {
		aml_write_cbus(AIU_MEM_IEC958_START_PTR, addr & 0xffffffc0);
		if (aml_read_cbus(AIU_MEM_IEC958_START_PTR) ==
		    aml_read_cbus(AIU_MEM_I2S_START_PTR)) {
			aml_write_cbus(AIU_MEM_IEC958_RD_PTR,
				       aml_read_cbus(AIU_MEM_I2S_RD_PTR));
		} else
			aml_write_cbus(AIU_MEM_IEC958_RD_PTR,
				       addr & 0xffffffc0);
		if (flag == 0) {
			/* this is for 16bit 2 channel */
			aml_write_cbus(AIU_MEM_IEC958_END_PTR,
				(addr & 0xffffffc0) + (size & 0xffffffc0) - 64);
		} else {
			/* this is for RAW mode */
			aml_write_cbus(AIU_MEM_IEC958_END_PTR,
				(addr & 0xffffffc0) + (size & 0xffffffc0) - 1);
		}
		aml_cbus_update_bits(AIU_MEM_IEC958_MASKS, 0xffff, 0x303);

		aml_cbus_update_bits(AIU_MEM_IEC958_CONTROL, 1, 1);
		aml_cbus_update_bits(AIU_MEM_IEC958_CONTROL, 1, 0);

		aml_write_cbus(AIU_MEM_IEC958_BUF_CNTL, 1 | (0 << 1));
		aml_write_cbus(AIU_MEM_IEC958_BUF_CNTL, 0 | (0 << 1));
	}
}

/*
i2s mode 0: master 1: slave
*/
static void i2sin_fifo0_set_buf(u32 addr, u32 size,
				u32 i2s_mode, u32 i2s_sync)
{
	unsigned char mode = 0;
	unsigned int sync_mode = 0;
	if (i2s_sync)
		sync_mode = i2s_sync;
	if (i2s_mode & I2SIN_SLAVE_MODE)
		mode = 1;
	aml_write_cbus(AUDIN_FIFO0_START, addr & 0xffffffc0);
	aml_write_cbus(AUDIN_FIFO0_PTR, (addr & 0xffffffc0));
	aml_write_cbus(AUDIN_FIFO0_END,
		       (addr & 0xffffffc0) + (size & 0xffffffc0) - 8);

	aml_write_cbus(AUDIN_FIFO0_CTRL, (1 << AUDIN_FIFO0_EN)	/* FIFO0_EN */
		       |(1 << AUDIN_FIFO0_LOAD)	/* load start address */
		       |(1 << AUDIN_FIFO0_DIN_SEL)

		       /* DIN from i2sin */
		       /* |(1<<6)    // 32 bits data in. */
		       /* |(0<<7)    // put the 24bits data to  low 24 bits */
		       |(4 << AUDIN_FIFO0_ENDIAN)	/* AUDIN_FIFO0_ENDIAN */
		       |(2 << AUDIN_FIFO0_CHAN)	/* two channel */
		       |(0 << 16)	/* to DDR */
		       |(1 << AUDIN_FIFO0_UG)	/* Urgent request. */
		       |(0 << 17)	/* Overflow Interrupt mask */
		       |(0 << 18)
		       /* Audio in INT */
		       /* |(1<<19)    // hold 0 enable */
		       |(0 << AUDIN_FIFO0_UG)	/* hold0 to aififo */
	    );

	aml_write_cbus(AUDIN_FIFO0_CTRL1, 0 << 4	/* fifo0_dest_sel */
		       | 2 << 2	/* fifo0_din_byte_num */
		       | 0 << 0);	/* fifo0_din_pos */

	aml_write_cbus(AUDIN_I2SIN_CTRL, (3 << I2SIN_SIZE)
		       | (1 << I2SIN_CHAN_EN)	/* 2 channel */
		       |(sync_mode << I2SIN_POS_SYNC)
		       | (1 << I2SIN_LRCLK_SKEW)
		       | (1 << I2SIN_LRCLK_INVT)
		       | (!mode << I2SIN_CLK_SEL)
		       | (!mode << I2SIN_LRCLK_SEL)
		       | (!mode << I2SIN_DIR)
	    );

}

static void spdifin_reg_set(void)
{
	/* get clk81 clk_rate */
	struct clk *clk_src = clk_get_sys("clk81", NULL);
	u32 clk_rate = clk_get_rate(clk_src);
	u32 spdif_clk_time = 54;	/* 54us */
	u32 spdif_mode_14bit = ((clk_rate / 500000 + 1) >> 1) * spdif_clk_time;
	/* sysclk/32(bit)/2(ch)/2(bmc) */
	u32 period_data = (clk_rate / 64000 + 1) >> 1;
	u32 period_32k = (period_data + (1 << 4)) >> 5;	/* 32k min period */
	u32 period_44k = (period_data / 22 + 1) >> 1;	/* 44k min period */
	u32 period_48k = (period_data / 24 + 1) >> 1;	/* 48k min period */
	u32 period_96k = (period_data / 48 + 1) >> 1;	/* 96k min period */
	u32 period_192k = (period_data / 96 + 1) >> 1;	/* 192k min period */

	pr_info("spdifin_reg_set: clk_rate=%d\n", clk_rate);

	aml_write_cbus(AUDIN_SPDIF_MODE,
		       (aml_read_cbus(AUDIN_SPDIF_MODE) & 0x7fffc000) |
		       (spdif_mode_14bit << 0));
	aml_write_cbus(AUDIN_SPDIF_FS_CLK_RLTN,
		       (period_32k << 0) |
		       (period_44k << 6) |
		       (period_48k << 12) |
				/* Spdif_fs_clk_rltn */
		       (period_96k << 18) | (period_192k << 24));

}

static void spdifin_fifo1_set_buf(u32 addr, u32 size)
{
	aml_write_cbus(AUDIN_SPDIF_MODE,
		       aml_read_cbus(AUDIN_SPDIF_MODE) & 0x7fffffff);
	aml_write_cbus(AUDIN_FIFO1_START, addr & 0xffffffc0);
	aml_write_cbus(AUDIN_FIFO1_PTR, (addr & 0xffffffc0));
	aml_write_cbus(AUDIN_FIFO1_END,
		       (addr & 0xffffffc0) + (size & 0xffffffc0) - 8);
	aml_write_cbus(AUDIN_FIFO1_CTRL, (1 << AUDIN_FIFO1_EN)	/* FIFO0_EN */
		       |(1 << AUDIN_FIFO1_LOAD)	/* load start address. */
		       |(0 << AUDIN_FIFO1_DIN_SEL)

		       /* DIN from i2sin. */
		       /* |(1<<6)   // 32 bits data in. */
		       /* |(0<<7)   // put the 24bits data to  low 24 bits */
		       |(4 << AUDIN_FIFO1_ENDIAN)	/* AUDIN_FIFO0_ENDIAN */
		       |(2 << AUDIN_FIFO1_CHAN)	/* 2 channel */
		       |(0 << 16)	/* to DDR */
		       |(1 << AUDIN_FIFO1_UG)	/* Urgent request. */
		       |(0 << 17)	/* Overflow Interrupt mask */
		       |(0 << 18)
		       /* Audio in INT */
		       /* |(1<<19)   //hold 0 enable */
		       |(0 << AUDIN_FIFO1_UG)	/* hold0 to aififo */
	    );

	/*
	*  according clk81 to set reg spdif_mode(0x2800)
	*  the last 14 bit and reg Spdif_fs_clk_rltn(0x2801)
	*/
	spdifin_reg_set();

	aml_write_cbus(AUDIN_FIFO1_CTRL1, 0xc);
}

void audio_in_i2s_set_buf(u32 addr, u32 size, u32 i2s_mode, u32 i2s_sync)
{
	pr_info("i2sin_fifo0_set_buf\n");
	i2sin_fifo0_set_buf(addr, size, i2s_mode, i2s_sync);
	audio_in_buf_ready = 1;
}

void audio_in_spdif_set_buf(u32 addr, u32 size)
{
	pr_info("spdifin_fifo1_set_buf\n");
	spdifin_fifo1_set_buf(addr, size);

}

/* extern void audio_in_enabled(int flag); */

void audio_in_i2s_enable(int flag)
{
	int rd = 0, start = 0;
	if (flag) {
		/* reset only when start i2s input */
 reset_again:
		/* reset FIFO 0 */
		aml_cbus_update_bits(AUDIN_FIFO0_CTRL, 0x2, 0x2);
		aml_write_cbus(AUDIN_FIFO0_PTR, 0);
		rd = aml_read_cbus(AUDIN_FIFO0_PTR);
		start = aml_read_cbus(AUDIN_FIFO0_START);
		if (rd != start) {
			pr_err("error %08x, %08x !!!!!!!!!!!!!!!!!!!!!!!!\n",
			       rd, start);
			goto reset_again;
		}
		aml_cbus_update_bits(AUDIN_I2SIN_CTRL, 1 << I2SIN_EN,
				     1 << I2SIN_EN);
	} else {
		aml_cbus_update_bits(AUDIN_I2SIN_CTRL, 1 << I2SIN_EN,
				     0 << I2SIN_EN);
	}
}

void audio_in_spdif_enable(int flag)
{
	int rd = 0, start = 0;

	if (flag) {
 reset_again:
		/* reset FIFO 0 */
		aml_cbus_update_bits(AUDIN_FIFO1_CTRL, 0x2, 0x2);
		aml_write_cbus(AUDIN_FIFO1_PTR, 0);
		rd = aml_read_cbus(AUDIN_FIFO1_PTR);
		start = aml_read_cbus(AUDIN_FIFO1_START);
		if (rd != start) {
			pr_err("error %08x, %08x !!!!!!!!!!!!!!!!!!!!!!!!\n",
			       rd, start);
			goto reset_again;
		}
		aml_write_cbus(AUDIN_SPDIF_MODE,
			       aml_read_cbus(AUDIN_SPDIF_MODE) | (1 << 31));
	} else {
		aml_write_cbus(AUDIN_SPDIF_MODE,
			       aml_read_cbus(AUDIN_SPDIF_MODE) & ~(1 << 31));
	}
}

int if_audio_in_i2s_enable(void)
{
	return aml_read_cbus(AUDIN_I2SIN_CTRL) & (1 << 15);
}

int if_audio_in_spdif_enable(void)
{
	return aml_read_cbus(AUDIN_SPDIF_MODE) & (1 << 31);
}

unsigned int audio_in_i2s_rd_ptr(void)
{
	unsigned int val;
	val = aml_read_cbus(AUDIN_FIFO0_RDPTR);
	pr_info("audio in i2s rd ptr: %x\n", val);
	return val;
}

unsigned int audio_in_spdif_rd_ptr(void)
{
	unsigned int val;
	val = aml_read_cbus(AUDIN_FIFO1_RDPTR);
	pr_info("audio in spdif rd ptr: %x\n", val);
	return val;
}

unsigned int audio_in_i2s_wr_ptr(void)
{
	unsigned int val;
	aml_write_cbus(AUDIN_FIFO0_PTR, 1);
	val = aml_read_cbus(AUDIN_FIFO0_PTR);
	return (val) & (~0x3F);
	/* return val&(~0x7); */
}

unsigned int audio_in_spdif_wr_ptr(void)
{
	unsigned int val;
	aml_write_cbus(AUDIN_FIFO1_PTR, 1);
	val = aml_read_cbus(AUDIN_FIFO1_PTR);
	return (val) & (~0x3F);
}

void audio_in_i2s_set_wrptr(unsigned int val)
{
	aml_write_cbus(AUDIN_FIFO0_RDPTR, val);
}

void audio_in_spdif_set_wrptr(unsigned int val)
{
	aml_write_cbus(AUDIN_FIFO1_RDPTR, val);
}

void audio_set_i2s_mode(u32 mode)
{
	const unsigned short mask[4] = {
		0x303,		/* 2x16 */
		0x303,		/* 2x24 */
		0x303,		/* 8x24 */
		0x303,		/* 2x32 */
	};

	if (mode < sizeof(mask) / sizeof(unsigned short)) {
		/* four two channels stream */
		aml_write_cbus(AIU_I2S_SOURCE_DESC, 1);

		if (mode == AIU_I2S_MODE_PCM16) {
			aml_cbus_update_bits(AIU_MEM_I2S_CONTROL, 1 << 6,
					     1 << 6);
			aml_cbus_update_bits(AIU_I2S_SOURCE_DESC, 1 << 5, 0);
		} else if (mode == AIU_I2S_MODE_PCM32) {
			aml_cbus_update_bits(AIU_MEM_I2S_CONTROL, 1 << 6, 0);
			aml_cbus_update_bits(AIU_I2S_SOURCE_DESC, 1 << 5,
					     1 << 5);
		} else if (mode == AIU_I2S_MODE_PCM24) {
			aml_cbus_update_bits(AIU_MEM_I2S_CONTROL, 1 << 6, 0);
			aml_cbus_update_bits(AIU_I2S_SOURCE_DESC, 1 << 5,
					     1 << 5);
		}

		aml_cbus_update_bits(AIU_MEM_I2S_MASKS, 0xffff, mask[mode]);
	}
}

/*
 *  if normal clock, i2s clock is twice of 958 clock,
 *  so the divisor for i2s is 8, but 4 for 958
 *  if over clock, the devisor for i2s is 8, but for 958 should be 1,
 *  because 958 should be 4 times speed according to i2s.
 *  This is dolby digital plus's spec
 */

void audio_util_set_dac_format(unsigned format)
{
	/* 958 divisor more, if true, divided by 2, 4, 6, 8. */
	aml_write_cbus(AIU_CLK_CTRL, (0 << 12) |
	/* alrclk skew: 1=alrclk transitions on the cycle before msb is sent */
			(1 << 8) |
			(1 << 6) |	/* invert aoclk */
			(1 << 7) |	/* invert lrclk */
#if OVERCLOCK == 1
	/* 958 divisor: 0=no div; 1=div by 2; 2=div by 3; 3=div by 4. */
	       (3 << 4) |
	/* i2s divisor: 0=no div; 1=div by 2; 2=div by 4; 3=div by 8. */
	       (3 << 2) |
#else
	/* 958 divisor: 0=no div; 1=div by 2; 2=div by 3; 3=div by 4. */
			(1 << 4) |
	/* i2s divisor: 0=no div; 1=div by 2; 2=div by 4; 3=div by 8. */
			(2 << 2) |
#endif
			(1 << 1) |	/* enable 958 clock */
			(1 << 0));	/* enable I2S clock */
	if (format == AUDIO_ALGOUT_DAC_FORMAT_DSP)
		aml_cbus_update_bits(AIU_CLK_CTRL, 0x3 << 8, 1 << 8);
	else if (format == AUDIO_ALGOUT_DAC_FORMAT_LEFT_JUSTIFY)
		aml_cbus_update_bits(AIU_CLK_CTRL, 0x3 << 8, 0);

	if (dac_mute_const == 0x800000)
		aml_write_cbus(AIU_I2S_DAC_CFG, 0x000f);
	else
		/* Payload 24-bit, Msb first, alrclk = aoclk/64 */
		aml_write_cbus(AIU_I2S_DAC_CFG, 0x0007);
	aml_write_cbus(AIU_I2S_SOURCE_DESC, 0x0001);	/* four 2-channel */
}

/* iec958 and i2s clock are separated after M6TV. */
void audio_util_set_dac_958_format(unsigned format)
{
	/* 958 divisor more, if true, divided by 2, 4, 6, 8 */
	aml_cbus_update_bits(AIU_CLK_CTRL, 1 << 12, 0);
#if IEC958_OVERCLOCK == 1
	/* 958 divisor: 0=no div; 1=div by 2; 2=div by 3; 3=div by 4. */
	aml_cbus_update_bits(AIU_CLK_CTRL, 3 << 4, 3 << 4);
#else
	/* 958 divisor: 0=no div; 1=div by 2; 2=div by 3; 3=div by 4. */
	aml_cbus_update_bits(AIU_CLK_CTRL, 3 << 4, 1 << 4);
#endif
	/* enable 958 divider */
	aml_cbus_update_bits(AIU_CLK_CTRL, 1 << 1, 1 << 1);
}

void audio_util_set_dac_i2s_format(unsigned format)
{
	/* invert aoclk */
	aml_cbus_update_bits(AIU_CLK_CTRL, 1 << 6, 1 << 6);
	/* invert lrclk */
	aml_cbus_update_bits(AIU_CLK_CTRL, 1 << 7, 1 << 7);
	/* alrclk skew: 1=alrclk transitions on the cycle before msb is sent */
	aml_cbus_update_bits(AIU_CLK_CTRL, 0x3 << 8, 1 << 8);
#if OVERCLOCK == 1
	/* i2s divisor: 0=no div; 1=div by 2; 2=div by 4; 3=div by 8. */
	aml_cbus_update_bits(AIU_CLK_CTRL, 0x3 << 2, 0x3 << 2);
#else
	/* i2s divisor: 0=no div; 1=div by 2; 2=div by 4; 3=div by 8. */
	aml_cbus_update_bits(AIU_CLK_CTRL, 0x3 << 2, 0x2 << 2);
#endif
	/* enable I2S clock */
	aml_cbus_update_bits(AIU_CLK_CTRL, 1, 1);

	if (format == AUDIO_ALGOUT_DAC_FORMAT_LEFT_JUSTIFY)
		aml_cbus_update_bits(AIU_CLK_CTRL, 0x3 << 8, 0);

	if (dac_mute_const == 0x800000)
		aml_write_cbus(AIU_I2S_DAC_CFG, 0x000f);
	else
	/* Payload 24-bit, Msb first, alrclk = aoclk/64 */
		aml_write_cbus(AIU_I2S_DAC_CFG, 0x0007);

	/* four 2-channel */
	aml_write_cbus(AIU_I2S_SOURCE_DESC, 0x0001);
}

#if 1
enum clk_enum {
	CLK_NONE = 0,
	CLK_MPLL0,
	CLK_MPLL1,
	CLK_MPLL2
};

/* iec958 and i2s clock are separated after M6TV. */
void audio_set_i2s_clk(unsigned freq, unsigned fs_config, unsigned mpll)
{
	int i, index = 0, xtal = 0;
	int mpll_reg, clk_src;
	int (*audio_clock_config)[2];
	switch (mpll) {
	case 0:
		mpll_reg = HHI_MPLL_CNTL7;
		clk_src = CLK_MPLL0;
		break;
	case 1:
		mpll_reg = HHI_MPLL_CNTL8;
		clk_src = CLK_MPLL1;
		break;
	case 2:
		mpll_reg = HHI_MPLL_CNTL9;
		clk_src = CLK_MPLL2;
		break;
	default:
		BUG();
	}

	switch (freq) {
	case AUDIO_CLK_FREQ_192:
		index = 4;
		break;
	case AUDIO_CLK_FREQ_96:
		index = 3;
		break;
	case AUDIO_CLK_FREQ_48:
		index = 2;
		break;
	case AUDIO_CLK_FREQ_441:
		index = 1;
		break;
	case AUDIO_CLK_FREQ_32:
		index = 0;
		break;
	case AUDIO_CLK_FREQ_8:
		index = 5;
		break;
	case AUDIO_CLK_FREQ_11:
		index = 6;
		break;
	case AUDIO_CLK_FREQ_12:
		index = 7;
		break;
	case AUDIO_CLK_FREQ_16:
		index = 8;
		break;
	case AUDIO_CLK_FREQ_22:
		index = 9;
		break;
	case AUDIO_CLK_FREQ_24:
		index = 10;
		break;
	case AUDIO_CLK_FREQ_882:
		index = 12;
		break;
	default:
		index = 0;
		break;
	};

	if (fs_config == AUDIO_CLK_256FS) {
		/* divide 256 */
		xtal = 0;
	} else if (fs_config == AUDIO_CLK_384FS) {
		/* divide 384 */
		xtal = 1;
	}
	audio_clock_config = audio_clock_config_table[xtal];

	/* gate the clock off */
	aml_write_cbus(HHI_AUD_CLK_CNTL,
		       aml_read_cbus(HHI_AUD_CLK_CNTL) & ~(1 << 8));
	aml_write_cbus(AIU_CLK_CTRL_MORE, 0);

	/* Set filter register */
	/* aml_write_cbus(HHI_MPLL_CNTL3, 0x26e1250); */

    /*--- DAC clock  configuration--- */
	/* Disable mclk */
	aml_cbus_update_bits(HHI_AUD_CLK_CNTL, 1 << 8, 0);

	/* Select clk source, 0=none; 1=Multi-Phase PLL0;
	*2=Multi-Phase PLL1; 3=Multi-Phase PLL2.
	*/
	aml_cbus_update_bits(HHI_AUD_CLK_CNTL, 0x3 << 9, clk_src << 9);

	/* Configure Multi-Phase PLLX */
	aml_write_cbus(mpll_reg, audio_clock_config[index][0]);
	/* set codec dac ratio---lrclk--64fs */
	aml_cbus_update_bits(AIU_CODEC_DAC_LRCLK_CTRL, 0xfff, (64 - 1));

	/* delay 5uS */
	/* udelay(5); */
	for (i = 0; i < 500000; i++)
		;

	/* gate the clock on */
	aml_write_cbus(HHI_AUD_CLK_CNTL,
		       aml_read_cbus(HHI_AUD_CLK_CNTL) | (1 << 8));

	/* Audio DAC Clock enable */
	aml_write_cbus(HHI_AUD_CLK_CNTL,
		       aml_read_cbus(HHI_AUD_CLK_CNTL) | (1 << 23));

	/* ---ADC clock  configuration--- */
	/* Disable mclk */
	aml_cbus_update_bits(HHI_AUD_CLK_CNTL, 1 << 8, 0);
	/*
	*  Set pll over mclk ratio
	*  we want 256fs ADC MLCK,so for over clock mode,
	*  divide more 2 than I2S  DAC CLOCK
	*/
#if OVERCLOCK == 0
	aml_cbus_update_bits(HHI_AUD_CLK_CNTL, 0xff,
			     audio_clock_config[index][1]);
#else
	aml_cbus_update_bits(HHI_AUD_CLK_CNTL, 0xff,
			     (audio_clock_config[index][1] + 1) * 2 - 1);
#endif

	/* Set mclk over sclk ratio */
	aml_cbus_update_bits(AIU_CLK_CTRL_MORE, 0x3f << 8, (4 - 1) << 8);

	/* set codec adc ratio---lrclk--64fs */
	aml_cbus_update_bits(AIU_CODEC_ADC_LRCLK_CTRL, 0xfff, 64 - 1);

	/* Enable sclk */
	aml_cbus_update_bits(AIU_CLK_CTRL_MORE, 1 << 14, 1 << 14);
	/* Enable mclk */
	aml_cbus_update_bits(HHI_AUD_CLK_CNTL, 1 << 8, 1 << 8);

	/* delay 2uS */
	/* udelay(2); */
	for (i = 0; i < 200000; i++)
		;
}

/* iec958 and i2s clock are separated after M6TV. Use PLL1 for iec958 clock */
void audio_set_958_clk(unsigned freq, unsigned fs_config)
{
	int i;
	int xtal = 0;

	int (*audio_clock_config)[2];

	int index = 0;
	pr_info("audio_set_958_clk, freq=%d,\n", freq);
	switch (freq) {
	case AUDIO_CLK_FREQ_192:
		index = 4;
		break;
	case AUDIO_CLK_FREQ_96:
		index = 3;
		break;
	case AUDIO_CLK_FREQ_48:
		index = 2;
		break;
	case AUDIO_CLK_FREQ_441:
		index = 1;
		break;
	case AUDIO_CLK_FREQ_32:
		index = 0;
		break;
	case AUDIO_CLK_FREQ_8:
		index = 5;
		break;
	case AUDIO_CLK_FREQ_11:
		index = 6;
		break;
	case AUDIO_CLK_FREQ_12:
		index = 7;
		break;
	case AUDIO_CLK_FREQ_16:
		index = 8;
		break;
	case AUDIO_CLK_FREQ_22:
		index = 9;
		break;
	case AUDIO_CLK_FREQ_24:
		index = 10;
		break;
	case AUDIO_CLK_FREQ_882:
		index = 12;
		break;
	default:
		index = 0;
		break;
	};

	if (fs_config == AUDIO_CLK_256FS)
		/* divide 256 */
		xtal = 0;
	else if (fs_config == AUDIO_CLK_384FS)
		/* divide 384 */
		xtal = 1;

	audio_clock_config = audio_clock_config_table[xtal];

	/* gate the clock off */
	aml_cbus_update_bits(HHI_AUD_CLK_CNTL, 1 << 8, 0 << 8);

    /*--- IEC958 clock  configuration, use MPLL1--- */
	/* Disable mclk */
	aml_cbus_update_bits(HHI_AUD_CLK_CNTL2, 1 << 24, 0 << 24);
	/* IEC958_USE_CNTL */
	aml_cbus_update_bits(HHI_AUD_CLK_CNTL2, 1 << 27, 1 << 27);
	/* Select clk source, 0=none; 1=Multi-Phase PLL0;
	*  2=Multi-Phase PLL1; 3=Multi-Phase PLL2.
	*/
	aml_cbus_update_bits(HHI_AUD_CLK_CNTL2, 0x3 << 25, CLK_MPLL1 << 25);

	/* Configure Multi-Phase PLL1 */
	aml_write_cbus(MPLL_958_CNTL, audio_clock_config[index][0]);
	/* Set the XD value */
#if IEC958_OVERCLOCK == 1
	aml_cbus_update_bits(HHI_AUD_CLK_CNTL2, 0xff << 16,
			     ((audio_clock_config[index][1] + 1) / 2 -
			      1) << 16);
#else
	aml_cbus_update_bits(HHI_AUD_CLK_CNTL2, 0xff << 16,
			     audio_clock_config[index][1] << 16);
#endif

	/* delay 5uS */
	/* udelay(5); */
	for (i = 0; i < 500000; i++)
		;

	/* gate the clock on */
	aml_cbus_update_bits(HHI_AUD_CLK_CNTL, 1 << 8, 1 << 8);
	/* Enable mclk */
	aml_cbus_update_bits(HHI_AUD_CLK_CNTL2, 1 << 24, 1 << 24);
}
#endif
void audio_enable_ouput(int flag)
{
	if (flag) {
		aml_write_cbus(AIU_RST_SOFT, 0x05);
		aml_read_cbus(AIU_I2S_SYNC);
		aml_cbus_update_bits(AIU_MEM_I2S_CONTROL, 3 << 1, 3 << 1);

		/* Maybe cause POP noise */
		/* audio_i2s_unmute(); */
	} else {
		aml_cbus_update_bits(AIU_MEM_I2S_CONTROL, 3 << 1, 0);

		/* Maybe cause POP noise */
		/* audio_i2s_mute(); */
	}
	/* audio_out_enabled(flag); */
}

int if_audio_out_enable(void)
{
	return aml_read_cbus(AIU_MEM_I2S_CONTROL) & (0x3<<1);
}

int if_958_audio_out_enable(void)
{
	return aml_read_cbus(AIU_MEM_IEC958_CONTROL) & (0x3<<1);
}

unsigned int read_i2s_rd_ptr(void)
{
	unsigned int val;
	val = aml_read_cbus(AIU_MEM_I2S_RD_PTR);
	return val;
}

unsigned int read_iec958_rd_ptr(void)
{
	unsigned int val;
	val = aml_read_cbus(AIU_MEM_IEC958_RD_PTR);
	return val;
}

void aml_audio_i2s_unmute(void)
{
	aml_cbus_update_bits(AIU_I2S_MUTE_SWAP, 0xff << 8, 0);
}

void aml_audio_i2s_mute(void)
{
	aml_cbus_update_bits(AIU_I2S_MUTE_SWAP, 0xff << 8, 0xff << 8);
}

void audio_i2s_unmute(void)
{
	aml_cbus_update_bits(AIU_I2S_MUTE_SWAP, 0xff << 8, 0);
	aml_cbus_update_bits(AIU_958_CTRL, 0x3 << 3, 0 << 3);
}

void audio_i2s_mute(void)
{
	aml_cbus_update_bits(AIU_I2S_MUTE_SWAP, 0xff << 8, 0xff << 8);
	aml_cbus_update_bits(AIU_958_CTRL, 0x3 << 3, 0x3 << 3);
}

void audio_mute_left_right(unsigned flag)
{
	if (flag == 0) {	/* right */
		aml_cbus_update_bits(AIU_958_CTRL, 0x3 << 3, 0x1 << 3);
	} else if (flag == 1) {	/* left */
		aml_cbus_update_bits(AIU_958_CTRL, 0x3 << 3, 0x2 << 3);
	}
}

void audio_hw_958_reset(unsigned slow_domain, unsigned fast_domain)
{
	aml_write_cbus(AIU_958_DCU_FF_CTRL, 0);
	aml_write_cbus(AIU_RST_SOFT, (slow_domain << 3) | (fast_domain << 2));
}

void audio_hw_958_raw(void)
{
	if (ENABLE_IEC958) {
		aml_write_cbus(AIU_958_MISC, 1);
		/* raw */
		aml_cbus_update_bits(AIU_MEM_IEC958_CONTROL, 1 << 8, 1 << 8);
		/* 8bit */
		aml_cbus_update_bits(AIU_MEM_IEC958_CONTROL, 1 << 7, 0);
		/* endian */
		aml_cbus_update_bits(AIU_MEM_IEC958_CONTROL, 0x7 << 3, 1 << 3);
	}

	aml_write_cbus(AIU_958_BPF, IEC958_bpf);
	aml_write_cbus(AIU_958_BRST, IEC958_brst);
	aml_write_cbus(AIU_958_LENGTH, IEC958_length);
	aml_write_cbus(AIU_958_PADDSIZE, IEC958_padsize);
	/* disable int */
	aml_cbus_update_bits(AIU_958_DCU_FF_CTRL, 0x3 << 2, 0);

	if (IEC958_mode == 1) {	/* search in byte */
		aml_cbus_update_bits(AIU_958_DCU_FF_CTRL, 0x7 << 4, 0x7 << 4);
	} else if (IEC958_mode == 2) {	/* search in word */
		aml_cbus_update_bits(AIU_958_DCU_FF_CTRL, 0x7 << 4, 0x5 << 4);
	} else {
		aml_cbus_update_bits(AIU_958_DCU_FF_CTRL, 0x7 << 4, 0);
	}
	aml_write_cbus(AIU_958_CHSTAT_L0, IEC958_chstat0_l);
	aml_write_cbus(AIU_958_CHSTAT_L1, IEC958_chstat1_l);
	aml_write_cbus(AIU_958_CHSTAT_R0, IEC958_chstat0_r);
	aml_write_cbus(AIU_958_CHSTAT_R1, IEC958_chstat1_r);

	aml_write_cbus(AIU_958_SYNWORD1, IEC958_syncword1);
	aml_write_cbus(AIU_958_SYNWORD2, IEC958_syncword2);
	aml_write_cbus(AIU_958_SYNWORD3, IEC958_syncword3);
	aml_write_cbus(AIU_958_SYNWORD1_MASK, IEC958_syncword1_mask);
	aml_write_cbus(AIU_958_SYNWORD2_MASK, IEC958_syncword2_mask);
	aml_write_cbus(AIU_958_SYNWORD3_MASK, IEC958_syncword3_mask);

	pr_info("%s: %d\n", __func__, __LINE__);
	pr_info("\tBPF: %x\n", IEC958_bpf);
	pr_info("\tBRST: %x\n", IEC958_brst);
	pr_info("\tLENGTH: %x\n", IEC958_length);
	pr_info("\tPADDSIZE: %x\n", IEC958_length);
	pr_info("\tsyncword: %x, %x, %x\n\n", IEC958_syncword1,
				IEC958_syncword2, IEC958_syncword3);

}

void set_958_channel_status(struct _aiu_958_channel_status_t *set)
{
	if (set) {
		aml_write_cbus(AIU_958_CHSTAT_L0, set->chstat0_l);
		aml_write_cbus(AIU_958_CHSTAT_L1, set->chstat1_l);
		aml_write_cbus(AIU_958_CHSTAT_R0, set->chstat0_r);
		aml_write_cbus(AIU_958_CHSTAT_R1, set->chstat1_r);
	}
}

static void audio_hw_set_958_pcm24(struct _aiu_958_raw_setting_t *set)
{
	/* in pcm mode, set bpf to 128 */
	aml_write_cbus(AIU_958_BPF, 0x80);
	set_958_channel_status(set->chan_stat);
}

void audio_set_958_mode(unsigned mode, struct _aiu_958_raw_setting_t *set)
{
	if (mode == AIU_958_MODE_PCM_RAW) {
		mode = AIU_958_MODE_PCM16;	/* use 958 raw pcm mode */
		aml_write_cbus(AIU_958_VALID_CTRL, 3);
	} else
		aml_write_cbus(AIU_958_VALID_CTRL, 0);

	if (mode == AIU_958_MODE_RAW) {

		audio_hw_958_raw();
		if (ENABLE_IEC958) {
			aml_write_cbus(AIU_958_MISC, 1);
			/* raw */
			aml_cbus_update_bits(AIU_MEM_IEC958_CONTROL,
						1 << 8, 1 << 8);
			/* 8bit */
			aml_cbus_update_bits(AIU_MEM_IEC958_CONTROL,
						1 << 7, 0);
			/* endian */
			aml_cbus_update_bits(AIU_MEM_IEC958_CONTROL,
						0x7 << 3, 0x1 << 3);
		}

		pr_info("IEC958 RAW\n");
	} else if (mode == AIU_958_MODE_PCM32) {
		audio_hw_set_958_pcm24(set);
		if (ENABLE_IEC958) {
			aml_write_cbus(AIU_958_MISC, 0x2020 | (1 << 7));
			/* pcm */
			aml_cbus_update_bits(AIU_MEM_IEC958_CONTROL,
						1 << 8, 0);
			/* 16bit */
			aml_cbus_update_bits(AIU_MEM_IEC958_CONTROL,
						1 << 7, 0);
			/* endian */
			aml_cbus_update_bits(AIU_MEM_IEC958_CONTROL,
						0x7 << 3, 0);
		}
		pr_info("IEC958 PCM32\n");
	} else if (mode == AIU_958_MODE_PCM24) {
		audio_hw_set_958_pcm24(set);
		if (ENABLE_IEC958) {
			aml_write_cbus(AIU_958_MISC, 0x2020 | (1 << 7));
			/* pcm */
			aml_cbus_update_bits(AIU_MEM_IEC958_CONTROL,
						1 << 8, 0);
			/* 16bit */
			aml_cbus_update_bits(AIU_MEM_IEC958_CONTROL,
						1 << 7, 0);
			/* endian */
			aml_cbus_update_bits(AIU_MEM_IEC958_CONTROL,
						0x7 << 3, 0);

		}
		pr_info("IEC958 24bit\n");
	} else if (mode == AIU_958_MODE_PCM16) {
		audio_hw_set_958_pcm24(set);
		if (ENABLE_IEC958) {
			aml_write_cbus(AIU_958_MISC, 0x2042);
			/* pcm */
			aml_cbus_update_bits(AIU_MEM_IEC958_CONTROL,
						1 << 8, 0);
			/* 16bit */
			aml_cbus_update_bits(AIU_MEM_IEC958_CONTROL,
						1 << 7, 1 << 7);
			/* endian */
			aml_cbus_update_bits(AIU_MEM_IEC958_CONTROL,
						0x7 << 3, 0);
		}
		pr_info("IEC958 16bit\n");
	}

	audio_hw_958_reset(0, 1);

	aml_write_cbus(AIU_958_FORCE_LEFT, 1);
}

void audio_out_i2s_enable(unsigned flag)
{
	if (flag) {
		aml_write_cbus(AIU_RST_SOFT, 0x01);
		aml_read_cbus(AIU_I2S_SYNC);
		aml_cbus_update_bits(AIU_MEM_I2S_CONTROL,
				0x3 << 1, 0x3 << 1);
		/* Maybe cause POP noise */
		/* audio_i2s_unmute(); */
	} else {
		aml_cbus_update_bits(AIU_MEM_I2S_CONTROL, 0x3 << 1, 0);

		/* Maybe cause POP noise */
		/* audio_i2s_mute(); */
	}
	/* audio_out_enabled(flag); */
}

void audio_hw_958_enable(unsigned flag)
{
	if (ENABLE_IEC958) {
		if (flag) {
			aml_write_cbus(AIU_RST_SOFT, 0x04);
			aml_write_cbus(AIU_958_FORCE_LEFT, 0);
			aml_cbus_update_bits(AIU_958_DCU_FF_CTRL, 1, 1);
			aml_cbus_update_bits(AIU_MEM_IEC958_CONTROL, 0x3 << 1,
					     0x3 << 1);
		} else {
			aml_write_cbus(AIU_958_DCU_FF_CTRL, 0);
			aml_cbus_update_bits(AIU_MEM_IEC958_CONTROL, 0x3 << 1,
					     0);
		}
	}
}

unsigned int read_i2s_mute_swap_reg(void)
{
	unsigned int val;
	val = aml_read_cbus(AIU_I2S_MUTE_SWAP);
	return val;
}

void audio_i2s_swap_left_right(unsigned int flag)
{
	if (ENABLE_IEC958)
		aml_cbus_update_bits(AIU_958_CTRL, 0x3 << 1, flag << 1);

	aml_cbus_update_bits(AIU_I2S_MUTE_SWAP, 0x3, flag);
}

#if 0
unsigned int audio_hdmi_init_ready(void)
{
	return READ_MPEG_REG_BITS(AIU_HDMI_CLK_DATA_CTRL, 0, 2);
}

/* power gate control for iec958 audio out */
unsigned audio_spdifout_pg_enable(unsigned char enable)
{
	if (enable) {
		aml_cbus_update_bits(MPLL_958_CNTL, 1, 14, 1);
		AUDIO_CLK_GATE_ON(AIU_IEC958);
		AUDIO_CLK_GATE_ON(AIU_ICE958_AMCLK);
	} else {
		AUDIO_CLK_GATE_OFF(AIU_IEC958);
		AUDIO_CLK_GATE_OFF(AIU_ICE958_AMCLK);
		aml_cbus_update_bits(MPLL_958_CNTL, 0, 14, 1);
	}
	return 0;
}

/*
    power gate control for normal aiu  domain including i2s in/out
    TODO: move i2s out /adc related gate to i2s cpu dai driver
*/
unsigned audio_aiu_pg_enable(unsigned char enable)
{
	if (enable)
		switch_mod_gate_by_name("audio", 1);
	else
		switch_mod_gate_by_name("audio", 0);

	return 0;
}
#endif
