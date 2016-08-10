/*
 * drivers/amlogic/amports/jpegdec.c
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
#include <linux/module.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/major.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>

#include <linux/amlogic/amports/jpegdec.h>
#include <linux/amlogic/canvas/canvas.h>
#include <linux/uaccess.h>
#include <linux/amlogic/codec_mm/codec_mm.h>
#include <linux/dma-contiguous.h>
#include <linux/of.h>
#include <linux/of_fdt.h>


#include "amvdec.h"
#include "streambuf.h"
#include "vdec_reg.h"
#include "amports_priv.h"
/* #include "jpegdec_mc.h" */

#define DEVICE_NAME "amjpegdec"
#define DRIVER_NAME "amjpegdec"
#define MODULE_NAME "amjpegdec"

/* #define DEBUG */
#define JPEGDEC_CANVAS_INDEX   0

#define JPEGDEC_OUTPUT_CANVAS_Y (JPEGDEC_CANVAS_INDEX)
#define JPEGDEC_OUTPUT_CANVAS_U (JPEGDEC_CANVAS_INDEX+1)
#define JPEGDEC_OUTPUT_CANVAS_V (JPEGDEC_CANVAS_INDEX+2)

#define JPEGDEC_OUTPUT_CANVAS \
	(((JPEGDEC_OUTPUT_CANVAS_V) << 16) |\
	((JPEGDEC_OUTPUT_CANVAS_U) << 8) | (JPEGDEC_OUTPUT_CANVAS_Y))

#define PSCALE_CANVAS_Y (JPEGDEC_CANVAS_INDEX+3)
#define PSCALE_CANVAS_U (JPEGDEC_CANVAS_INDEX+4)
#define PSCALE_CANVAS_V (JPEGDEC_CANVAS_INDEX+5)
#define PSCALE_CANVAS \
	(((PSCALE_CANVAS_V) << 16) |\
	 ((PSCALE_CANVAS_U) << 8) | ((PSCALE_CANVAS_Y)))

#define JPEG_DECODE_START       AV_SCRATCH_0
#define JPEG_INFO               AV_SCRATCH_0
#define JPEG_PIC_WIDTH          AV_SCRATCH_1
#define JPEG_PIC_HEIGHT         AV_SCRATCH_2
#define JPEG_DECODE_PARAMETER   AV_SCRATCH_3
#define JPEG_SCREEN_OFFSET_Y    AV_SCRATCH_4
#define JPEG_SCREEN_OFFSET_X    AV_SCRATCH_5
#define JPEG_MCU_CROP_HSTART    PSCALE_PICI_W
#define JPEG_MCU_CROP_HEND      PSCALE_PICI_H
#define JPEG_MCU_CROP_VSTART    PSCALE_PICO_W
#define JPEG_MCU_CROP_VEND      PSCALE_PICO_H

#define CMA_ALLOC_SIZE 10

#ifdef DEBUG
#define pr_dbg(fmt, args...) pr_info(KERN_DEBUG "amjpegdec: " fmt, ## args)
#else
#define pr_dbg(fmt, args...)
#endif
#define pr_error(fmt, args...) pr_err(KERN_ERR "amjpegdec: " fmt, ## args)

struct jpegdec_s {
	struct jpegdec_config_s conf;
	struct jpegdec_info_s info;
	unsigned state;
};

static struct class *amjpegdec_class;
static struct device *amjpegdec_dev;
static const char jpegdec_id[] = "amjpegdec";

static DEFINE_MUTEX(jpegdec_module_mutex);
static struct jpegdec_s *dec;
static unsigned long pbufAddr;
static unsigned long pbufSize;
static struct jpegdec_mem_info_s jegdec_mem_info;

static irqreturn_t jpegdec_isr(int irq, void *dev_id)
{

	WRITE_VREG(ASSIST_MBOX1_CLR_REG, 1);

	if ((dec->state & JPEGDEC_STAT_INFO_READY) == 0) {
		dec->info.width = READ_VREG(JPEG_PIC_WIDTH);
		dec->info.height = READ_VREG(JPEG_PIC_HEIGHT);
		dec->info.comp_num = READ_VREG(JPEG_INFO) >> 1;

		pr_dbg("ucode report picture size %dx%d, comp_num %d\n",
			dec->info.width, dec->info.height,
			dec->info.comp_num);

		if ((dec->info.comp_num != 1) && (dec->info.comp_num != 3))
			dec->state |= JPEGDEC_STAT_INFO_READY |
			JPEGDEC_STAT_UNSUPPORT;
		else
			dec->state |= JPEGDEC_STAT_INFO_READY |
			JPEGDEC_STAT_WAIT_DECCONFIG;
	} else {
		unsigned r = READ_VREG(JPEG_DECODE_START);

		dec->state &= ~JPEGDEC_STAT_WAIT_DATA;

		if (r == 2) {
			pr_dbg("ucode report decoding finished\n");
			dec->state |= JPEGDEC_STAT_DONE;
		} else if ((r == 4) || (r == 6))
			dec->state |= JPEGDEC_STAT_ERROR;
		else
			pr_error("jpegdec_isr ucode unknow status 0x%x\n", r);
	}

	return IRQ_HANDLED;
}

static int _init_dec(struct jpegdec_s *d)
{
	int r;

	amvdec_enable();

	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_M6) {
		WRITE_VREG(DOS_SW_RESET0, (1 << 11));
		WRITE_VREG(DOS_SW_RESET0, 0);
	} else {
		WRITE_MPEG_REG(RESET0_REGISTER, RESET_VCPU);
	}

	WRITE_VREG(ASSIST_AMR1_INT0, 0x1);
	WRITE_VREG(ASSIST_AMR1_INT1, 0xf);
	WRITE_VREG(ASSIST_AMR1_INT2, 0x8);
	WRITE_VREG(ASSIST_AMR1_INT3, 0xa);
	WRITE_VREG(ASSIST_AMR1_INT4, 0x3);
	WRITE_VREG(ASSIST_AMR1_INT5, 0x9);
	WRITE_VREG(ASSIST_AMR1_INT6, 0x4);

	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_M6) {
		WRITE_VREG(DOS_SW_RESET0, (1 << 11) | (1 << 7) | (1 << 6));
		WRITE_VREG(DOS_SW_RESET0, 0);
	} else {
		WRITE_MPEG_REG(RESET0_REGISTER, RESET_VCPU |
			RESET_IQIDCT | RESET_MC);
	}
	if (amvdec_loadmc_ex(VFORMAT_JPEG, "jpegdec_mc", NULL) < 0) {
		amvdec_disable();

		pr_error("jpegdec ucode loading failed.\n");
		return -EBUSY;
	}
	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_M6) {
		WRITE_VREG(DOS_SW_RESET0, (1 << 11) | (1 << 10) |
			(1 << 7) | (1 << 6));
		WRITE_VREG(DOS_SW_RESET0, 0);
	} else {
		WRITE_MPEG_REG(RESET0_REGISTER, RESET_VCPU |
			RESET_IQIDCT | RESET_MC);
		WRITE_MPEG_REG(RESET2_REGISTER, RESET_PSCALE);
	}
	WRITE_VREG(PSCALE_RST, 0x7);
	WRITE_VREG(PSCALE_RST, 0x0);

	WRITE_VREG(JPEG_PIC_WIDTH, 0);
	WRITE_VREG(JPEG_PIC_HEIGHT, 0);
	WRITE_VREG(ASSIST_MBOX1_MASK, 1);
	WRITE_VREG(ASSIST_MBOX1_CLR_REG, 1);

	r = request_irq(INT_VDEC, jpegdec_isr, IRQF_SHARED, "jpegdec-irq",
					(void *)jpegdec_id);

	if (r) {
		amvdec_disable();

		pr_error("jpegdec irq register error.\n");
		return -ENOENT;
	}

	d->state = JPEGDEC_STAT_WAIT_DATA | JPEGDEC_STAT_WAIT_INFOCONFIG;

	return 0;
}

static void _init_scaler(unsigned horz_step, unsigned vert_step)
{
	int i;

	/* 2 point bilinear */
	static const unsigned filt_coef[] = {
		0x00800000, 0x007e0200, 0x007c0400, 0x007a0600,
		0x00780800, 0x00760a00, 0x00740c00, 0x00720e00,
		0x00701000, 0x006e1200, 0x006c1400, 0x006a1600,
		0x00681800, 0x00661a00, 0x00641c00, 0x00621e00,
		0x00602000, 0x005e2200, 0x005c2400, 0x005a2600,
		0x00582800, 0x00562a00, 0x00542c00, 0x00522e00,
		0x00503000, 0x004e3200, 0x004c3400, 0x004a3600,
		0x00483800, 0x00463a00, 0x00443c00, 0x00423e00,
		0x00404000
	};

	WRITE_VREG(PSCALE_CTRL, 0xc000);

	/* write filter coefs */
	WRITE_VREG(PSCALE_BMEM_ADDR, 0);
	for (i = 0; i < 33; i++) {
		WRITE_VREG(PSCALE_BMEM_DAT, 0);
		WRITE_VREG(PSCALE_BMEM_DAT, filt_coef[i]);
	}

#define BM_HORZ_Y_PHASE_STEP_OFFSET (36*2+1)
#define BM_HORZ_C_PHASE_STEP_OFFSET (40*2+1)
#define BM_VERT_Y_PHASE_STEP_OFFSET (38*2+1)
#define BM_VERT_C_PHASE_STEP_OFFSET (42*2+1)
#define BM_HORZ_Y_INI_INFO_OFFSET   (37*2)
#define BM_HORZ_C_INI_INFO_OFFSET   (41*2)
#define BM_VERT_Y_INI_INFO_OFFSET   (39*2)
#define BM_VERT_C_INI_INFO_OFFSET   (43*2)

	/* Y horizontal initial info */
	WRITE_VREG(PSCALE_BMEM_ADDR, BM_HORZ_Y_INI_INFO_OFFSET);
	WRITE_VREG(PSCALE_BMEM_DAT, 0x0008);
	WRITE_VREG(PSCALE_BMEM_DAT, 0x60000000);

	/* C horizontal initial info */
	WRITE_VREG(PSCALE_BMEM_ADDR, BM_HORZ_C_INI_INFO_OFFSET);
	WRITE_VREG(PSCALE_BMEM_DAT, 0x0008);
	WRITE_VREG(PSCALE_BMEM_DAT, 0x60000000);

	/* Y vertical initial info */
	WRITE_VREG(PSCALE_BMEM_ADDR, BM_VERT_Y_INI_INFO_OFFSET);
	WRITE_VREG(PSCALE_BMEM_DAT, 0x0008);
	WRITE_VREG(PSCALE_BMEM_DAT, 0x60000000);

	/* C vertical initial info */
	WRITE_VREG(PSCALE_BMEM_ADDR, BM_VERT_C_INI_INFO_OFFSET);
	WRITE_VREG(PSCALE_BMEM_DAT, 0x0008);
	WRITE_VREG(PSCALE_BMEM_DAT, 0x60000000);

	/* Y horizontal phase step */
	WRITE_VREG(PSCALE_BMEM_ADDR, BM_HORZ_Y_PHASE_STEP_OFFSET);
	WRITE_VREG(PSCALE_BMEM_DAT, horz_step);

	/* C horizontal phase step */
	WRITE_VREG(PSCALE_BMEM_ADDR, BM_HORZ_C_PHASE_STEP_OFFSET);
	WRITE_VREG(PSCALE_BMEM_DAT, horz_step);

	/* Y vertical phase step */
	WRITE_VREG(PSCALE_BMEM_ADDR, BM_VERT_Y_PHASE_STEP_OFFSET);
	WRITE_VREG(PSCALE_BMEM_DAT, vert_step);

	/* C vertical phase step */
	WRITE_VREG(PSCALE_BMEM_ADDR, BM_VERT_C_PHASE_STEP_OFFSET);
	WRITE_VREG(PSCALE_BMEM_DAT, vert_step);
}

static void _dec_run(void)
{
	static unsigned filt0_table[9] = { 0, 0, 1, 1, 2, 2, 2, 2, 2 };
	static unsigned addr_mode_tab[4] = { 0x0, 0xd, 0xa, 0x7 };
	unsigned filt0_horz_ratio2, filt0_vert_ratio2;
	unsigned filt0_horz_ratio, filt0_vert_ratio;
	unsigned horz_step, vert_step;
	unsigned r, cmd = 0;
	unsigned pscaleCanvasWidth = (dec->conf.angle & 1) ? dec->conf.dec_h :
				dec->conf.dec_w;
	pscaleCanvasWidth = (pscaleCanvasWidth + 63) & (~63);
	canvas_config(JPEGDEC_OUTPUT_CANVAS_Y,
		dec->conf.addr_y,
		dec->conf.canvas_width, dec->conf.dec_h, CANVAS_ADDR_NOWRAP,
		CANVAS_BLKMODE_LINEAR);

	canvas_config(JPEGDEC_OUTPUT_CANVAS_U,
		dec->conf.addr_u,
		dec->conf.canvas_width / 2,
		dec->conf.dec_h / 2, CANVAS_ADDR_NOWRAP,
		CANVAS_BLKMODE_LINEAR);

	canvas_config(JPEGDEC_OUTPUT_CANVAS_V,
		dec->conf.addr_v,
		dec->conf.canvas_width / 2,
		dec->conf.dec_h / 2, CANVAS_ADDR_NOWRAP,
		CANVAS_BLKMODE_LINEAR);

	canvas_config(PSCALE_CANVAS_Y, pbufAddr, pscaleCanvasWidth, 128,
				  CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_32X32);

	canvas_config(PSCALE_CANVAS_U,
		pbufAddr + ((pscaleCanvasWidth + 7) & ~7) * 128,
		pscaleCanvasWidth / 2, 128,
		CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_32X32);

	canvas_config(PSCALE_CANVAS_V,
		pbufAddr + ((pscaleCanvasWidth + 7) & ~7) * 128 +
		((pscaleCanvasWidth / 2 + 7) & ~7) * 128,
		pscaleCanvasWidth / 2, 128,
		CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_32X32);

	WRITE_VREG(JPEG_SCREEN_OFFSET_X, dec->conf.dec_x);
	WRITE_VREG(JPEG_SCREEN_OFFSET_Y, dec->conf.dec_y);
	WRITE_VREG(PSCALE_PICO_SHIFT_XY, 0);

	WRITE_VREG(JPEG_MCU_CROP_HSTART, 0);
	WRITE_VREG(JPEG_MCU_CROP_HEND, ((dec->info.width + 15) >> 4) - 1);
	WRITE_VREG(JPEG_MCU_CROP_VSTART, 0);
	WRITE_VREG(JPEG_MCU_CROP_VEND, ((dec->info.height + 15) >> 4) - 1);

	if ((dec->conf.angle & 1) == 0) {
		r = dec->info.width / dec->conf.dec_w;
		filt0_horz_ratio = filt0_table[r];
		filt0_horz_ratio2 = filt0_table[min((unsigned)(dec->info.width /
			dec->conf.dec_w) >> (2 + r), 8U)];
		horz_step = (dec->info.width <<
			(16 - filt0_horz_ratio - filt0_horz_ratio2)) /
			dec->conf.dec_w;

		r = dec->info.height / dec->conf.dec_h;
		filt0_vert_ratio = filt0_table[r];
		filt0_vert_ratio2 = filt0_table[min(
				(dec->info.height / dec->conf.dec_h) >> (2 + r),
				8U)];
		vert_step = (dec->info.height <<
				(16 - filt0_vert_ratio - filt0_vert_ratio2)) /
				dec->conf.dec_h;

		WRITE_VREG(JPEG_PIC_WIDTH, dec->conf.dec_w);
		WRITE_VREG(JPEG_PIC_HEIGHT, dec->conf.dec_h);

	} else {
		r = dec->info.width / dec->conf.dec_h;
		filt0_horz_ratio = filt0_table[r];
		filt0_horz_ratio2 = filt0_table[min((unsigned)(dec->info.width /
				dec->conf.dec_h) >> (2 + r), 8U)];
		horz_step = (dec->info.width <<
			(16 - filt0_horz_ratio - filt0_horz_ratio2)) /
			dec->conf.dec_h;

		r = dec->info.height / dec->conf.dec_w;
		filt0_vert_ratio = filt0_table[r];
		filt0_vert_ratio2 = filt0_table[min((dec->info.height /
				dec->conf.dec_w) >>
				(2 + r), 8U)];
		vert_step = (dec->info.height <<
			(16 - filt0_vert_ratio - filt0_vert_ratio2)) /
			dec->conf.dec_w;

		WRITE_VREG(JPEG_PIC_WIDTH, dec->conf.dec_h);
		WRITE_VREG(JPEG_PIC_HEIGHT, dec->conf.dec_w);
	}

	_init_scaler(horz_step, vert_step);

	WRITE_VREG(JPEG_DECODE_PARAMETER, filt0_horz_ratio2 << 14 |/* round1 */
			   filt0_vert_ratio2 << 12 |	/* round1 */
			   filt0_horz_ratio << 10 |	/* round0 */
			   filt0_vert_ratio << 8 |	/* round0 */
			   addr_mode_tab[dec->conf.angle & 3] << 4);

	WRITE_VREG(PSCALE_RBUF_START_BLKX, 0);
	WRITE_VREG(PSCALE_RBUF_START_BLKY, 0);

	WRITE_VREG(PSCALE_CANVAS_WR_ADDR, JPEGDEC_OUTPUT_CANVAS);
	WRITE_VREG(PSCALE_CANVAS_RD_ADDR, PSCALE_CANVAS);

	if (dec->conf.opt & JPEGDEC_OPT_FULLRANGE)
		cmd |= 1 << 5;

	if (dec->conf.opt & JPEGDEC_OPT_THUMBNAIL_ONLY)
		cmd |= 1 << 7;

	pr_dbg("Enable decoding\n");

	WRITE_VREG(JPEG_DECODE_START, cmd | 1);
}

static int amjpegdec_open(struct inode *inode, struct file *file)
{
	int r = 0;

	mutex_lock(&jpegdec_module_mutex);

	if (dec != NULL)
		r = -EBUSY;

	dec = kcalloc(1, sizeof(struct jpegdec_s), GFP_KERNEL);
	if (dec == NULL)
		r = -ENOMEM;

	mutex_unlock(&jpegdec_module_mutex);

	if (r == 0) {
		r = _init_dec(dec);
		if (r < 0) {
			kfree(dec);
			dec = NULL;
		} else
			file->private_data = dec;
	}

	return r;
}

static int amjpegdec_release(struct inode *inode, struct file *file)
{
	if (dec) {
		free_irq(INT_VDEC, (void *)jpegdec_id);

		kfree(dec);
		dec = NULL;
	}

	amvdec_disable();

	if (pbufAddr) {
		codec_mm_free_for_dma(
			"jpegdec",
			pbufAddr);
		pbufAddr = 0;
		pbufSize = 0;
		pr_info("jpegdec cma memory release succeed\n");
	}
	return 0;
}

static long amjpegdec_ioctl(struct file *file, unsigned int cmd, ulong arg)
{
	int r = 0;
	void __user *argp;
	argp = (void __user *)arg;

	switch (cmd) {
	case JPEGDEC_IOC_INFOCONFIG:
		if (dec->state & JPEGDEC_STAT_WAIT_INFOCONFIG) {

			dec->conf.opt |= arg & (JPEGDEC_OPT_THUMBNAIL_ONLY |
				JPEGDEC_OPT_THUMBNAIL_PREFERED);

			WRITE_VREG(JPEG_DECODE_START,
				(dec->conf.opt &
				JPEGDEC_OPT_THUMBNAIL_PREFERED) ? 1 : 0);

			dec->state &= ~JPEGDEC_STAT_WAIT_INFOCONFIG;
			dec->state |= JPEGDEC_STAT_WAIT_DATA;
			amvdec_start();
		} else
			r = -EPERM;
		break;
	case JPEGDEC_IOC_DECCONFIG32:
		if (dec->state & JPEGDEC_STAT_WAIT_DECCONFIG) {
			ulong paddr_y, paddr_u, paddr_v , r;
			struct compat_jpegdec_config_s __user *uf =
				(struct compat_jpegdec_config_s *)argp;
			memset(&dec->conf, 0, sizeof(struct jpegdec_config_s));
			r = get_user(paddr_y, &uf->addr_y);
			dec->conf.addr_y = paddr_y;
			r |= get_user(paddr_u, &uf->addr_u);
			dec->conf.addr_u = paddr_u;
			r |= get_user(paddr_v, &uf->addr_v);
			dec->conf.addr_v = paddr_v;
			r |= get_user(dec->conf.canvas_width,
				&uf->canvas_width);
			r |= get_user(dec->conf.opt, &uf->opt);
			r |= get_user(dec->conf.src_crop_x, &uf->src_crop_x);
			r |= get_user(dec->conf.src_crop_y, &uf->src_crop_y);
			r |= get_user(dec->conf.src_crop_w, &uf->src_crop_w);
			r |= get_user(dec->conf.src_crop_h, &uf->src_crop_h);
			r |= get_user(dec->conf.dec_x, &uf->dec_x);
			r |= get_user(dec->conf.dec_y, &uf->dec_y);
			r |= get_user(dec->conf.dec_w, &uf->dec_w);
			r |= get_user(dec->conf.dec_h, &uf->dec_h);
			r |= get_user(dec->conf.angle, &uf->angle);
			if (r) {
				pr_err("JPEGDEC_IOC_DECCONFIG32 get parameter failed .\n");
				return -EFAULT;
			}
			pr_dbg("amjpegdec_ioctl:config,target (%d-%d-%d-%d)\n",
				dec->conf.dec_x, dec->conf.dec_y,
				dec->conf.dec_w, dec->conf.dec_h);
			pr_dbg("planes (0x%lx-0x%lx-0x%lx), pbufAddr=0x%lx\n",
				dec->conf.addr_y, dec->conf.addr_u,
				dec->conf.addr_v, pbufAddr);

			if ((dec->conf.angle & 1) == 0) {
				if ((dec->conf.dec_w > dec->info.width) ||
					(dec->conf.dec_h > dec->info.height))
					return -EPERM;

			} else {
				if ((dec->conf.dec_w > dec->info.height) ||
					(dec->conf.dec_h > dec->info.width))
					return -EPERM;
			}

			dec->state &= ~JPEGDEC_STAT_WAIT_DECCONFIG;
			_dec_run();
		} else
			r = -EPERM;
		break;
	case JPEGDEC_IOC_DECCONFIG:
		if (dec->state & JPEGDEC_STAT_WAIT_DECCONFIG) {
			if (copy_from_user(&dec->conf,
				(void *)arg, sizeof(struct jpegdec_config_s)))
				return -EFAULT;

			pr_dbg("amjpegdec_ioctl:config,target (%d-%d-%d-%d)\n",
				dec->conf.dec_x, dec->conf.dec_y,
					dec->conf.dec_w, dec->conf.dec_h);
			pr_dbg("planes (0x%lx-0x%lx-0x%lx), pbufAddr=0x%lx\n",
				dec->conf.addr_y, dec->conf.addr_u,
				dec->conf.addr_v, pbufAddr);

			if ((dec->conf.angle & 1) == 0) {
				if ((dec->conf.dec_w > dec->info.width) ||
					(dec->conf.dec_h > dec->info.height))
					return -EPERM;

			} else {
				if ((dec->conf.dec_w > dec->info.height) ||
					(dec->conf.dec_h > dec->info.width))
					return -EPERM;
			}

			dec->state &= ~JPEGDEC_STAT_WAIT_DECCONFIG;

			_dec_run();

		} else
			r = -EPERM;
		break;

	case JPEGDEC_IOC_INFO:
		if (dec->state & JPEGDEC_STAT_INFO_READY) {
			pr_dbg("amjpegdec_ioctl:  JPEGDEC_IOC_INFO, %dx%d\n",
				dec->info.width,
				dec->info.height);
			if (copy_to_user((void *)arg, &dec->info,
					sizeof(struct jpegdec_info_s)))
				return -EFAULT;
		} else
			r = -EAGAIN;
		break;

	case JPEGDEC_IOC_STAT:
		return dec->state;
	case JPEGDEC_G_MEM_INFO:
		if (copy_from_user(&jegdec_mem_info, (void __user *)arg,
				   sizeof(struct jpegdec_mem_info_s)))
			r = -EFAULT;
		else {
			unsigned pscaleCanvasbwidth =
				(jegdec_mem_info.angle & 1) ?
				jegdec_mem_info.dec_h : jegdec_mem_info.dec_w;
			pscaleCanvasbwidth = (pscaleCanvasbwidth + 63) & (~63);
			pscaleCanvasbwidth = pscaleCanvasbwidth * 2 * 128;
			jegdec_mem_info.canv_addr =
				pscaleCanvasbwidth + pbufAddr;
			jegdec_mem_info.canv_len =
				pbufSize - pscaleCanvasbwidth;

			if (copy_to_user((void __user *)arg, &jegdec_mem_info,
					sizeof(struct jpegdec_mem_info_s)))
				r = -EFAULT;
		}
		break;
	default:
		return -ENOIOCTLCMD;
	}

	return r;
}

static int mmap(struct file *filp, struct vm_area_struct *vma)
{
	unsigned long off = vma->vm_pgoff << PAGE_SHIFT;
	unsigned vm_size = vma->vm_end - vma->vm_start;

	if (vm_size == 0)
		return -EAGAIN;
	/* pr_info("mmap:%x\n",vm_size); */
	off += jegdec_mem_info.canv_addr;

	vma->vm_flags |= VM_DONTEXPAND | VM_DONTDUMP | VM_IO;

	if (remap_pfn_range(vma, vma->vm_start, off >> PAGE_SHIFT,
		vma->vm_end - vma->vm_start, vma->vm_page_prot)) {
		pr_info("set_cached: failed remap_pfn_range\n");
		return -EAGAIN;
	}
	return 0;

}

#ifdef CONFIG_COMPAT
static long amjpegdec_compat_ioctl(struct file *filp,
			      unsigned int cmd, unsigned long args)
{
	unsigned long ret;

	args = (unsigned long)compat_ptr(args);
	ret = amjpegdec_ioctl(filp, cmd, args);

	return ret;
}
#endif

static const struct file_operations amjpegdec_fops = {
	.owner = THIS_MODULE,
	.open = amjpegdec_open,
	.mmap = mmap,
	.release = amjpegdec_release,
	.unlocked_ioctl = amjpegdec_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = amjpegdec_compat_ioctl,
#endif
};

int AMHWJPEGDEC_MAJOR = 0;
static int amjpegdec_probe(struct platform_device *pdev)
{
	int r;
	int flags;

	pr_dbg(" register amjpegdec .\n");
	AMHWJPEGDEC_MAJOR = 0;
	r = register_chrdev(AMHWJPEGDEC_MAJOR, "amjpegdec", &amjpegdec_fops);

	if (r < 0) {
		pr_err("Can't register major for amjpegdec device\n");
		return r;
	}
	AMHWJPEGDEC_MAJOR = r;
	amjpegdec_class = class_create(THIS_MODULE, DEVICE_NAME);

	amjpegdec_dev = device_create(amjpegdec_class, NULL,
					MKDEV(AMHWJPEGDEC_MAJOR, 0),
					NULL, DEVICE_NAME);

	flags = CODEC_MM_FLAGS_DMA_CPU|CODEC_MM_FLAGS_CMA_CLEAR;

	pbufAddr = codec_mm_alloc_for_dma(
					"jpegdec",
					(CMA_ALLOC_SIZE*SZ_1M)/PAGE_SIZE,
					0, flags);
	if (!pbufAddr) {
		pr_err("jpegdec alloc cma buffer failed\n");
		return -1;
	} else {
		pbufSize = (CMA_ALLOC_SIZE*SZ_1M);
	}
	pr_info("jpegdec cma memory is %lx , size is  %lx\n" ,
		pbufAddr , pbufSize);
	return 0;
}

static int amjpegdec_remove(struct platform_device *pdev)
{
	device_destroy(amjpegdec_class, MKDEV(AMHWJPEGDEC_MAJOR, 0));

	class_destroy(amjpegdec_class);

	unregister_chrdev(AMHWJPEGDEC_MAJOR, DEVICE_NAME);

	return 0;
}

static const struct of_device_id amlogic_amjpegdec_dt_match[] = {
	{
		.compatible = "amlogic, amjpegdec",
	},
	{},
};

static struct platform_driver amjpegdec_driver = {
	.probe = amjpegdec_probe,
	.remove = amjpegdec_remove,
	.driver = {
		.name = "amjpegdec",
		.owner = THIS_MODULE,
		.of_match_table = amlogic_amjpegdec_dt_match,
	}
};

static int __init amjpegdec_init(void)
{
	if (platform_driver_register(&amjpegdec_driver)) {
		pr_err("failed to register amjpegdec module\n");
		return -ENOENT;
	}

	return 0;
}

static void __exit amjpegdec_exit(void)
{
	platform_driver_unregister(&amjpegdec_driver);
	return;
}

module_init(amjpegdec_init);
module_exit(amjpegdec_exit);

MODULE_DESCRIPTION("AMLOGIC HW JPEG decoder driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tim Yao <timyao@amlogic.com>");
