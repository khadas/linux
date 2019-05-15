/*
 * drivers/amlogic/media/frame_provider/decoder/utils/frame_check.c
 *
 * Copyright (C) 2016 Amlogic, Inc. All rights reserved.
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
#include <linux/errno.h>
#include <linux/kfifo.h>
#include <linux/interrupt.h>
#include <linux/semaphore.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/kfifo.h>
#include <linux/kthread.h>
#include <linux/platform_device.h>
#include <linux/amlogic/media/canvas/canvas.h>
#include <linux/amlogic/media/canvas/canvas_mgr.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include <linux/dma-mapping.h>
#include <linux/dma-contiguous.h>
#include <asm-generic/checksum.h>
#include <linux/amlogic/media/codec_mm/configs.h>
#include <linux/crc32.h>
#include <linux/fs.h>
#include "vdec.h"
#include "frame_check.h"
#include "amlogic_fbc_hook.h"


#define FC_ERROR	0x0

#define FC_YUV_DEBUG	0x01
#define FC_CRC_DEBUG	0x02
#define FC_TST_DEBUG	0x80

#define YUV_MASK	0x01
#define CRC_MASK	0x02

#define YUV_DEF_SIZE (1920*1088*3/2)
#define YUV_DEF_NUM	 1

#define MAX_SIZE_AFBC_PLANES 	(4096 * 2048)

#define VMAP_STRIDE_SIZE  (1024*1024)

static unsigned int fc_debug;
static unsigned int size_yuv_buf = (YUV_DEF_SIZE * YUV_DEF_NUM);

#define dbg_print(mask, ...) do {					\
			if ((fc_debug & mask) ||				\
				(mask == FC_ERROR))					\
				printk("[FRMAE_CHECK] "__VA_ARGS__);\
		} while(0)


#define CRC_PATH  "/data/tmp/"
#define YUV_PATH  "/data/tmp/"
static char comp_crc[128] = "name";


module_param_string(comp_crc, comp_crc, 128, 0664);
MODULE_PARM_DESC(comp_crc, "\n crc_filename\n");

module_param(fc_debug, uint, 0664);
MODULE_PARM_DESC(fc_debug, "\n frame check debug\n");

module_param(size_yuv_buf, uint, 0664);
MODULE_PARM_DESC(size_yuv_buf, "\n size_yuv_buf\n");

static __inline void set_enable(struct pic_check_mgr_t *p, int mask)
{
	p->enable |= mask;
}

static __inline void set_disable(struct pic_check_mgr_t *p, int mask)
{
	p->enable &= (~mask);
}

static int get_frame_size(struct pic_check_mgr_t *pic,
	struct vframe_s *vf)
{
	if ((vf->width < 1) || (vf->height < 1))
			return -1;
	pic->size_y = vf->width* vf->height;
	pic->size_uv = pic->size_y >> 1;
	pic->size_pic = pic->size_y + pic->size_uv;

	if (!(vf->type & VIDTYPE_VIU_NV21))
		return 0;

	if ((vf->canvas0Addr == vf->canvas1Addr) &&
		(vf->canvas0Addr != 0) &&
		(vf->canvas0Addr != -1)) {
		pic->canvas_w =
			canvas_get_width(canvasY(vf->canvas0Addr));
		pic->canvas_h =
			canvas_get_height(canvasY(vf->canvas0Addr));
	} else {
		pic->canvas_w = vf->canvas0_config[0].width;
		pic->canvas_h = vf->canvas0_config[1].height;
	}

	if ((pic->canvas_h < 1) || (pic->canvas_w < 1)) {
		dbg_print(FC_ERROR, "(canvas,pic) w(%d,%d), h(%d,%d)\n",
			pic->canvas_w, vf->width, pic->canvas_h, vf->height);
		return -1;
	}
/*
	int blkmod;
	blkmod = canvas_get_blkmode(canvasY(vf->canvas0Addr));
	if (blkmod != CANVAS_BLKMODE_LINEAR) {
		dbg_print(0, "WARN: canvas blkmod %x\n", blkmod);
	}
*/
	return 0;
}

static int canvas_get_virt_addr(struct pic_check_mgr_t *pic,
	struct vframe_s *vf)
{
	int phy_y_addr, phy_uv_addr;
	void *vaddr_y, *vaddr_uv;

	if ((vf->canvas0Addr == vf->canvas1Addr) &&
		(vf->canvas0Addr != 0) &&
		(vf->canvas0Addr != -1)) {
		phy_y_addr = canvas_get_addr(canvasY(vf->canvas0Addr));
		phy_uv_addr = canvas_get_addr(canvasUV(vf->canvas0Addr));
	} else {
		phy_y_addr = vf->canvas0_config[0].phy_addr;
		phy_uv_addr = vf->canvas0_config[1].phy_addr;
	}
	vaddr_y	= codec_mm_phys_to_virt(phy_y_addr);
	vaddr_uv = codec_mm_phys_to_virt(phy_uv_addr);

	if (((!vaddr_y) || (!vaddr_uv)) && ((!phy_y_addr) || (!phy_uv_addr))) {
		dbg_print(FC_ERROR, "%s, y_addr %p(0x%x), uv_addr %p(0x%x)\n",
			__func__, vaddr_y, phy_y_addr, vaddr_uv, phy_uv_addr);
		return -1;
	}
	pic->y_vaddr = vaddr_y;
	pic->uv_vaddr = vaddr_uv;
	pic->y_phyaddr = phy_y_addr;
	pic->uv_phyaddr = phy_uv_addr;

	return 0;
}

static int str_strip(char *str)
{
	char *s = str;
	int i = 0;

	while (s[i]) {
		if (s[i] == '\n')
			s[i] = 0;
		else if (s[i] == ' ')
			s[i] = '_';
		i++;
	}

	return i;
}

static char *fget_crc_str(char *buf,
	unsigned int size, struct pic_check_t *fc)
{
	unsigned int c = 0, sz, ret, index, crc1, crc2;
	mm_segment_t old_fs;
	char *cs;

	if (!fc->compare_fp)
		return NULL;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	do {
		cs = buf;
		sz = size;
		while (--sz && (c = vfs_read(fc->compare_fp,
			cs, 1, &fc->compare_pos) != 0)) {
			if (*cs++ == '\n')
				break;
		}
		*cs = '\0';
		if ((c == 0) && (cs == buf))
			return NULL;
		ret = sscanf(buf, "%08u: %8x %8x", &index, &crc1, &crc2);
		dbg_print(FC_CRC_DEBUG, "%s, index = %d, cmp = %d\n",
			__func__, index, fc->cmp_crc_cnt);
	}while(ret != 3 || index != fc->cmp_crc_cnt);

	set_fs(old_fs);
	fc->cmp_crc_cnt++;

	return buf;
}

static struct file* file_open(int mode, const char *str, ...)
{
	char file[256] = {0};
	struct file* fp = NULL;
	va_list args;

	va_start(args, str);
	vsnprintf(file, sizeof(file), str, args);

	fp = filp_open(file, mode, 0);
	if (IS_ERR(fp)) {
		fp = NULL;
		dbg_print(FC_ERROR, "open %s failed\n", file);
		return fp;
	}
	dbg_print(FC_ERROR, "open %s success\n", file);
	va_end(args);

	return fp;
}

static int write_yuv_work(struct pic_check_mgr_t *mgr)
{
	mm_segment_t old_fs;
	unsigned int i, wr_size;
	struct pic_dump_t *dump = &mgr->pic_dump;

	if (dump->dump_cnt > 0) {
		if (!dump->yuv_fp) {
			dump->yuv_fp = file_open(O_CREAT | O_WRONLY | O_TRUNC,
				"%s%s-%d-%d.yuv", YUV_PATH, comp_crc, mgr->id, mgr->file_cnt);
			dump->yuv_pos = 0;
		}

		if ((mgr->enable & YUV_MASK) &&
			(dump->yuv_fp != NULL) &&
			(dump->dump_cnt >= dump->num)) {

			old_fs = get_fs();
			set_fs(KERNEL_DS);

			for (i = 0; dump->dump_cnt > 0; i++) {
				wr_size = vfs_write(dump->yuv_fp,
					(dump->buf_addr + i * mgr->size_pic),
					mgr->size_pic, &dump->yuv_pos);
				if (mgr->size_pic != wr_size) {
					dbg_print(FC_ERROR, "buf failed to write yuv file\n");
					break;
				}
				dump->dump_cnt--;
			}
			set_fs(old_fs);
			vfs_fsync(dump->yuv_fp, 0);

			filp_close(dump->yuv_fp, current->files);
			dump->yuv_pos = 0;
			dump->yuv_fp = NULL;
			set_disable(mgr, YUV_MASK);
			dbg_print(FC_YUV_DEBUG,
				"closed yuv file, dump yuv exit\n");
			dump->num = 0;
			dump->dump_cnt = 0;
			if (dump->buf_addr != NULL)
				vfree(dump->buf_addr);
			dump->buf_addr = NULL;
			dump->buf_size = 0;
		}
	}

	return 0;
}

static int write_crc_work(struct pic_check_mgr_t *mgr)
{
	unsigned int wr_size;
	char *crc_buf, crc_tmp[64*30];
	mm_segment_t old_fs;
	struct pic_check_t *check = &mgr->pic_check;

	if (mgr->enable & CRC_MASK) {
		wr_size = 0;
		while (kfifo_get(&check->wr_chk_q, &crc_buf) != 0) {
			wr_size += sprintf(&crc_tmp[wr_size], "%s", crc_buf);
			if (check->compare_fp != NULL) {
				if (!fget_crc_str(crc_buf, SIZE_CRC, check)) {
					dbg_print(0, "%s, can't get more compare crc\n", __func__);
					filp_close(check->compare_fp, current->files);
					check->compare_fp = NULL;
				}
			}
			kfifo_put(&check->new_chk_q, crc_buf);
		}
		if (check->check_fp && (wr_size != 0)) {
			old_fs = get_fs();
			set_fs(KERNEL_DS);
			if (wr_size != vfs_write(check->check_fp,
				crc_tmp, wr_size, &check->check_pos)) {
				dbg_print(FC_ERROR, "failed to check_dump_filp\n");
			}
			set_fs(old_fs);
		}
	}
	return 0;
}

static void do_check_work(struct work_struct *work)
{
	struct pic_check_mgr_t *mgr = container_of(work,
		struct pic_check_mgr_t, frame_check_work);

	write_yuv_work(mgr);

	write_crc_work(mgr);
}

static void check_schedule(struct pic_check_mgr_t *mgr)
{
	if (atomic_read(&mgr->work_inited))
		vdec_schedule_work(&mgr->frame_check_work);
}

static int memcpy_phy_to_virt(char *to_virt,
	ulong phy_from, unsigned int size)
{
	void *vaddr = NULL;
	unsigned int tmp_size = 0;

	while (size > 0) {
		if (size >= VMAP_STRIDE_SIZE) {
			vaddr = (void *)codec_mm_vmap(phy_from, VMAP_STRIDE_SIZE);
			tmp_size = VMAP_STRIDE_SIZE;
			phy_from += VMAP_STRIDE_SIZE;
			size -= VMAP_STRIDE_SIZE;
		} else {
			vaddr = (void *)codec_mm_vmap(phy_from, size);
			tmp_size = size;
			size = 0;
		}
		if (vaddr == NULL) {
			dbg_print(FC_YUV_DEBUG, "%s: codec_mm_vmap failed phy: 0x%x\n",
				__func__, (unsigned int)phy_from);
			return -1;
		}
		memcpy(to_virt, vaddr, tmp_size);
		to_virt += tmp_size;

		codec_mm_unmap_phyaddr(vaddr);
	}
	return 0;
}

static int do_yuv_dump(struct pic_check_mgr_t *mgr, struct vframe_s *vf)
{
	int i, ret = 0;
	void *tmp_addr, *tmp_yaddr, *tmp_uvaddr;
	ulong y_phyaddr, uv_phyaddr;
	struct pic_dump_t *dump = &mgr->pic_dump;
	/*
	if ((vf->type & VIDTYPE_VIU_NV21) == 0)
		return 0;
	*/
	if (dump->start > 0) {
		dump->start--;
		return 0;
	}

	if (dump->dump_cnt >= dump->num)
		return 0;

	if (mgr->size_pic >
		(dump->buf_size - dump->dump_cnt * mgr->size_pic)) {
		if (dump->buf_size) {
			dbg_print(FC_ERROR,
				"not enough buf, force dump less\n");
			dump->num = dump->dump_cnt;
			check_schedule(mgr);
		} else
			set_disable(mgr, YUV_MASK);
		return -1;
	}

	tmp_addr = dump->buf_addr +
		mgr->size_pic * dump->dump_cnt;
	if ((mgr->uv_vaddr == NULL) || (mgr->y_vaddr == NULL)) {
		y_phyaddr = mgr->y_phyaddr;
		uv_phyaddr = mgr->uv_phyaddr;
		if (vf->width == mgr->canvas_w) {
			ret |= memcpy_phy_to_virt(tmp_addr, y_phyaddr, mgr->size_y);
			ret |= memcpy_phy_to_virt(tmp_addr + mgr->size_y,
				uv_phyaddr, mgr->size_uv);
		} else {
			for (i = 0; i < vf->height; i++) {
				ret |= memcpy_phy_to_virt(tmp_addr, y_phyaddr, vf->width);
				y_phyaddr += mgr->canvas_w;
				tmp_addr += vf->width;
			}
			for (i = 0; i < vf->height/2; i++) {
				ret |= memcpy_phy_to_virt(tmp_addr, uv_phyaddr, vf->width);
				uv_phyaddr += mgr->canvas_w;
				tmp_addr += vf->width;
			}
		}
		if (ret < 0) {
			dbg_print(0, "dump yuv failed, may codec_mm_vmap failed\n");
			return ret;
		}
	} else {
		if (vf->width == mgr->canvas_w) {
			memcpy(tmp_addr, mgr->y_vaddr, mgr->size_y);
			memcpy(tmp_addr + mgr->size_y,
				mgr->uv_vaddr, mgr->size_uv);
		} else {
			tmp_yaddr = mgr->y_vaddr;
			tmp_uvaddr = mgr->uv_vaddr;
			for (i = 0; i < vf->height; i++) {
				memcpy(tmp_addr, tmp_yaddr, vf->width);
				tmp_yaddr += mgr->canvas_w;
				tmp_addr += vf->width;
			}
			for (i = 0; i < vf->height/2; i++) {
				memcpy(tmp_addr, tmp_uvaddr, vf->width);
				tmp_uvaddr += mgr->canvas_w;
				tmp_addr += vf->width;
			}
		}
	}
	dump->dump_cnt++;
	dbg_print(0,
		"----->>dump frame num: %d, dump %dst, size %x\n",
		mgr->frame_cnt, dump->dump_cnt, mgr->size_pic);

	if (dump->dump_cnt >= dump->num)
		check_schedule(mgr);

	return 0;
}

static int crc_store(struct pic_check_mgr_t *mgr, struct vframe_s *vf,
	int crc_y, int crc_uv)
{
	int ret = 0;
	char *crc_addr;
	int comp_frame = 0, comp_crc_y, comp_crc_uv;
	struct pic_check_t *check = &mgr->pic_check;

	if (kfifo_get(&check->new_chk_q, &crc_addr) == 0) {
		dbg_print(0, "%08d: %08x %08x\n",
			mgr->frame_cnt, crc_y, crc_uv);
		return -1;
	}
	if (check->cmp_crc_cnt > mgr->frame_cnt) {
		sscanf(crc_addr, "%08u: %8x %8x",
			&comp_frame, &comp_crc_y, &comp_crc_uv);

		dbg_print(0, "%08d: %08x %08x <--> %08d: %08x %08x\n",
			mgr->frame_cnt, crc_y, crc_uv,
			comp_frame, comp_crc_y, comp_crc_uv);
		/*
		if ((mgr->frame_cnt % 100) == 0)
			comp_crc_uv = mgr->frame_cnt;
		*/
		if (comp_frame == mgr->frame_cnt) {
			if ((comp_crc_y != crc_y) || (crc_uv != comp_crc_uv)) {
					mgr->pic_dump.start = 0;
					mgr->pic_dump.num++;
					dbg_print(0, "\n\nError: %08d, %08x %08x != %08x %08x\n\n",
						mgr->frame_cnt, crc_y, crc_uv, comp_crc_y, comp_crc_uv);
					do_yuv_dump(mgr, vf);
			}
		}
    }

	if (check->check_fp) {
		ret = snprintf(crc_addr, SIZE_CRC,
			"%08d: %08x %08x\n", mgr->frame_cnt, crc_y, crc_uv);

		kfifo_put(&check->wr_chk_q, crc_addr);
		if ((mgr->frame_cnt & 0xf) == 0)
			check_schedule(mgr);
	}
	return ret;
}

static int crc32_vmap_le(unsigned int *crc32,
	ulong phyaddr, unsigned int size)
{
	void *vaddr = NULL;
	unsigned int crc = 0;
	unsigned int tmp_size = 0;

	while (size > 0) {
		if (size >= VMAP_STRIDE_SIZE) {
			vaddr = (void *)codec_mm_vmap(phyaddr, VMAP_STRIDE_SIZE);
			tmp_size = VMAP_STRIDE_SIZE;
			phyaddr += VMAP_STRIDE_SIZE;
			size -= VMAP_STRIDE_SIZE;
		} else {
			vaddr = (void *)codec_mm_vmap(phyaddr, size);
			tmp_size = size;
			size = 0;
		}
		if (vaddr == NULL) {
			dbg_print(FC_CRC_DEBUG, "%s: codec_mm_vmap failed phy: 0x%x\n",
				__func__, (unsigned int)phyaddr);
			return -1;
		}
		crc = crc32_le(crc, vaddr, tmp_size);

		codec_mm_unmap_phyaddr(vaddr);
	}
	*crc32 = crc;

	return 0;
}

static int do_check_nv21(struct pic_check_mgr_t *mgr, struct vframe_s *vf)
{
	int i;
	unsigned int crc_y = 0, crc_uv = 0;
	void *p_yaddr, *p_uvaddr;
	ulong y_phyaddr, uv_phyaddr;
	int ret = 0;

	p_yaddr = mgr->y_vaddr;
	p_uvaddr = mgr->uv_vaddr;
	y_phyaddr = mgr->y_phyaddr;
	uv_phyaddr = mgr->uv_phyaddr;
	if ((p_yaddr == NULL) || (p_uvaddr == NULL))
	{
		if (vf->width == mgr->canvas_w) {
			ret = crc32_vmap_le(&crc_y, y_phyaddr, mgr->size_y);
			ret |= crc32_vmap_le(&crc_uv, uv_phyaddr, mgr->size_uv);
		} else {
			for (i = 0; i < vf->height; i++) {
				ret |= crc32_vmap_le(&crc_y, y_phyaddr, vf->width);
				y_phyaddr += mgr->canvas_w;
			}
			for (i = 0; i < vf->height/2; i++) {
				ret |= crc32_vmap_le(&crc_uv, uv_phyaddr, vf->width);
				uv_phyaddr += mgr->canvas_w;
			}
		}
		if (ret < 0) {
			dbg_print(0, "calc crc failed, may codec_mm_vmap failed\n");
			return ret;
		}
	} else {
		if (mgr->frame_cnt == 0) {
			unsigned int *p = mgr->y_vaddr;
			dbg_print(0, "YUV0000: %08x-%08x-%08x-%08x\n",
				p[0], p[1], p[2], p[3]);
		}
		if (vf->width == mgr->canvas_w) {
			crc_y = crc32_le(crc_y, p_yaddr, mgr->size_y);
			crc_uv = crc32_le(crc_uv, p_uvaddr, mgr->size_uv);
		} else {
			for (i = 0; i < vf->height; i++) {
				crc_y = crc32_le(crc_y, p_yaddr, vf->width);
				p_yaddr += mgr->canvas_w;
			}
			for (i = 0; i < vf->height/2; i++) {
				crc_uv = crc32_le(crc_uv, p_uvaddr, vf->width);
				p_uvaddr += mgr->canvas_w;
			}
		}
	}

	crc_store(mgr, vf, crc_y, crc_uv);

	return 0;
}

static int do_check_yuv16(struct pic_check_mgr_t *mgr,
	struct vframe_s *vf, char *ybuf, char *uvbuf,
	char *ubuf, char *vbuf)
{
	unsigned int crc1, crc2, crc3, crc4;
	int w, h;

	w = vf->width;
	h = vf->height;
	crc1 = 0;
	crc2 = 0;
	crc3 = 0;
	crc4 = 0;

	crc1 = crc32_le(0, ybuf, w * h *2);
	crc2 = crc32_le(0, ubuf, w * h/2);
	crc3 = crc32_le(0, vbuf, w * h/2);
	crc4 = crc32_le(0, uvbuf, w * h*2/2);
	/*
	printk("%08d: %08x %08x %08x %08x\n",
		mgr->frame_cnt, crc1, crc4, crc2, crc3);
	*/
	mgr->size_y = w * h * 2;
	mgr->size_uv = w * h;
	mgr->size_pic = mgr->size_y + mgr->size_uv;
	mgr->y_vaddr = ybuf;
	mgr->uv_vaddr = uvbuf;
	mgr->canvas_w = w;
	mgr->canvas_h = h;
	crc_store(mgr, vf, crc1, crc4);

	return 0;
}

static int fbc_check_prepare(struct pic_check_t *check,
	int resize, int y_size)
{
	int i = 0;

	if  (y_size > MAX_SIZE_AFBC_PLANES)
		return -1;

	if (((!check->fbc_planes[0]) ||
		(!check->fbc_planes[1]) ||
		(!check->fbc_planes[2]) ||
		(!check->fbc_planes[3])) &&
		(!resize))
		return -1;

	if (resize) {
		dbg_print(0, "size changed to 0x%x(y_size)\n", y_size);
		for (i = 0; i < ARRAY_SIZE(check->fbc_planes); i++) {
			if (check->fbc_planes[i]) {
				vfree(check->fbc_planes[i]);
				check->fbc_planes[i] = NULL;
			}
		}
	}
	for (i = 0; i < ARRAY_SIZE(check->fbc_planes); i++) {
		if (!check->fbc_planes[i])
			check->fbc_planes[i] =
				vmalloc(y_size * sizeof(short));
	}
	if ((!check->fbc_planes[0]) ||
		(!check->fbc_planes[1]) ||
		(!check->fbc_planes[2]) ||
		(!check->fbc_planes[3])) {
		dbg_print(0, "vmalloc staicplanes failed %lx %lx %lx %lx\n",
			(ulong)check->fbc_planes[0],
			(ulong)check->fbc_planes[1],
			(ulong)check->fbc_planes[2],
			(ulong)check->fbc_planes[3]);
		for (i = 0; i < ARRAY_SIZE(check->fbc_planes); i++) {
			if (check->fbc_planes[i]) {
				vfree(check->fbc_planes[i]);
				check->fbc_planes[i] = NULL;
			}
		}
		return -1;
	} else
		dbg_print(FC_CRC_DEBUG, "vmalloc staicplanes sucessed\n");

	return 0;
}

static struct vdec_s *single_mode_vdec = NULL;

int decoder_do_frame_check(struct vdec_s *vdec, struct vframe_s *vf)
{
	int ret = 0;
	int resize = 0;
	void *planes[4];
	struct pic_check_t *check = NULL;
	struct pic_check_mgr_t *mgr = NULL;

	if (vdec == NULL) {
		if (single_mode_vdec == NULL)
			return 0;
		mgr = &single_mode_vdec->vfc;
	} else
		mgr = &vdec->vfc;

	if ((mgr == NULL) ||
		(vf == NULL) ||
		(mgr->enable == 0))
		return 0;

	if (get_frame_size(mgr, vf) < 0)
		return -1;

	if (mgr->last_size_pic != mgr->size_pic) {
		resize = 1;
		dbg_print(0, "size changed, %x-->%x\n",
			mgr->last_size_pic, mgr->size_pic);
	} else
		resize = 0;
	mgr->last_size_pic = mgr->size_pic;

	if (vf->type & VIDTYPE_VIU_NV21) {
		if (canvas_get_virt_addr(mgr, vf) < 0)
			return -2;

		if (mgr->enable & CRC_MASK)
			ret = do_check_nv21(mgr, vf);
		if (mgr->enable & YUV_MASK)
			do_yuv_dump(mgr, vf);

	} else if  (vf->type & VIDTYPE_SCATTER) {
		check = &mgr->pic_check;

		if (mgr->pic_dump.buf_addr != NULL) {
			dbg_print(0, "scatter free yuv buf\n");
			vfree(mgr->pic_dump.buf_addr);
			mgr->pic_dump.buf_addr = NULL;
		}
		if (fbc_check_prepare(check,
				resize, mgr->size_y) < 0)
			return -3;
		planes[0] = check->fbc_planes[0];
		planes[1] = check->fbc_planes[1];
		planes[2] = check->fbc_planes[2];
		planes[3] = check->fbc_planes[3];
		AMLOGIC_FBC_vframe_decoder(planes, vf, 0, 0);
		do_check_yuv16(mgr, vf,
			(void *)planes[0], (void *)planes[3],//uv
			(void *)planes[1], (void *)planes[2]);
	}
	mgr->frame_cnt++;

	return ret;
}
EXPORT_SYMBOL(decoder_do_frame_check);

static int dump_buf_alloc(struct pic_dump_t *dump)
{
	if ((dump->buf_addr != NULL) &&
		(dump->buf_size != 0))
		return 0;

	dump->buf_addr =
		(char *)vmalloc(size_yuv_buf);
	if (!dump->buf_addr) {
		dump->buf_size = 0;
		dbg_print(0, "vmalloc yuv buf failed\n");
		return -ENOMEM;
	}
	dump->buf_size = size_yuv_buf;

	dbg_print(0, "%s: buf for yuv is alloced\n", __func__);

	return 0;
}

int dump_yuv_trig(struct pic_check_mgr_t *mgr,
	int id, int start, int num)
{
	struct pic_dump_t *dump = &mgr->pic_dump;

	if (!dump->num) {
		mgr->id = id;
		dump->start = start;
		dump->num = num;
		dump->end = start + num;
		dump->dump_cnt = 0;
		dump->yuv_fp = NULL;
		if (!atomic_read(&mgr->work_inited)) {
			INIT_WORK(&mgr->frame_check_work, do_check_work);
			atomic_set(&mgr->work_inited, 1);
		}
		dump_buf_alloc(dump);
		str_strip(comp_crc);
		set_enable(mgr, YUV_MASK);
	} else {
		dbg_print(FC_ERROR, "yuv dump now, trig later\n");
		return -EBUSY;
	}
	dbg_print(0, "dump yuv trigger, from %d to %d frame\n",
		dump->start, dump->end);
	return 0;
}

int frame_check_init(struct pic_check_mgr_t *mgr, int id)
{
	int i;
	struct pic_dump_t *dump = &mgr->pic_dump;
	struct pic_check_t *check = &mgr->pic_check;

	mgr->frame_cnt = 0;
	mgr->size_pic = 0;
	mgr->last_size_pic = 0;
	mgr->id = id;

	dump->num = 0;
	dump->dump_cnt = 0;
	dump->yuv_fp = NULL;
	check->check_pos = 0;
	check->compare_pos = 0;

	if (!atomic_read(&mgr->work_inited)) {
		INIT_WORK(&mgr->frame_check_work, do_check_work);
		atomic_set(&mgr->work_inited, 1);
	}
	/* for dump error yuv prepare. */
	dump_buf_alloc(dump);

	/* try to open compare crc32 file */
	str_strip(comp_crc);
	check->compare_fp = file_open(O_RDONLY,
		"%s%s", CRC_PATH, comp_crc);

	/* create crc32 log file */
	check->check_fp = file_open(O_WRONLY | O_CREAT,
		"%s%s-%d-%d.crc", CRC_PATH, comp_crc, id, mgr->file_cnt);

	INIT_KFIFO(check->new_chk_q);
	INIT_KFIFO(check->wr_chk_q);
	check->check_addr = vmalloc(SIZE_CRC * SIZE_CHECK_Q);
	if (check->check_addr == NULL) {
		dbg_print(FC_ERROR, "vmalloc qbuf fail\n");
	} else {
		void *qaddr = NULL, *rdret = NULL;
		check->cmp_crc_cnt = 0;
		for (i = 0; i < SIZE_CHECK_Q &&
			(check->compare_fp != NULL); i++) {
			qaddr = check->check_addr + i * SIZE_CRC;
			rdret = fget_crc_str(qaddr,
				SIZE_CRC, check);
			if (rdret == NULL) {
				dbg_print(0, "can't get compare crc string\n");
				filp_close(check->compare_fp, current->files);
				check->compare_fp = NULL;
			}

			kfifo_put(&check->new_chk_q, qaddr);
		}
	}
	set_enable(mgr, CRC_MASK);
	dbg_print(0, "%s end\n", __func__);

	return 0;
}

void frame_check_exit(struct pic_check_mgr_t *mgr)
{
	int i;
	struct pic_dump_t *dump = &mgr->pic_dump;
	struct pic_check_t *check = &mgr->pic_check;

	if (mgr->enable != 0) {
		if (dump->dump_cnt != 0) {
			dbg_print(0, "%s, cnt = %d, num = %d\n",
				__func__, dump->dump_cnt, dump->num);
			set_enable(mgr, YUV_MASK);
		}
		if (atomic_read(&mgr->work_inited)) {
			cancel_work_sync(&mgr->frame_check_work);
			atomic_set(&mgr->work_inited, 0);
		}
		write_yuv_work(mgr);
		write_crc_work(mgr);

		for (i = 0; i < ARRAY_SIZE(check->fbc_planes); i++) {
			if (check->fbc_planes[i]) {
				vfree(check->fbc_planes[i]);
				check->fbc_planes[i] = NULL;
			}
		}
		if (check->check_addr) {
			vfree(check->check_addr);
			check->check_addr = NULL;
		}

		if (check->check_fp) {
			filp_close(check->check_fp, current->files);
			check->check_fp = NULL;
		}
		if (check->compare_fp) {
			filp_close(check->compare_fp, current->files);
			check->compare_fp = NULL;
		}
		if (dump->yuv_fp) {
			filp_close(dump->yuv_fp, current->files);
			dump->yuv_fp = NULL;
		}
		if (dump->buf_addr) {
			vfree(dump->buf_addr);
			dump->buf_addr = NULL;
		}
		mgr->file_cnt++;
		set_disable(mgr, YUV_MASK | CRC_MASK);
		dbg_print(0, "%s end\n", __func__);
	}
}



static unsigned int yuv_enable, check_enable;
static unsigned int yuv_start[MAX_INSTANCE_MUN];
static unsigned int yuv_num[MAX_INSTANCE_MUN];


int vdec_frame_check_init(struct vdec_s *vdec)
{
	int ret = 0, id = 0;

	if ((vdec == NULL) ||
		(vdec->is_reset))
		return 0;

	if (!check_enable && !yuv_enable)
		return 0;

	if (vdec_single(vdec))
		single_mode_vdec = vdec;

	vdec->canvas_mode = CANVAS_BLKMODE_LINEAR;
	id = vdec->id;

	if (check_enable & (0x01 << id)) {
		frame_check_init(&vdec->vfc, id);
		check_enable &= ~(0x01 << id);
	}

	if (yuv_enable & (0x01 << id)) {
		ret = dump_yuv_trig(&vdec->vfc,
			id, yuv_start[id], yuv_num[id]);
		if (ret < 0)
			pr_info("dump yuv init failed\n");
		else {
			pr_info("dump yuv init ok, total %d\n",
				yuv_num[id]);
			vdec->canvas_mode = CANVAS_BLKMODE_LINEAR;
		}
		yuv_num[id] = 0;
		yuv_start[id] = 0;
		yuv_enable &= ~(0x01 << id);
	}

	return ret;
}

void vdec_frame_check_exit(struct vdec_s *vdec)
{
	if (vdec == NULL)
		return;
	frame_check_exit(&vdec->vfc);

	single_mode_vdec = NULL;
}

ssize_t dump_yuv_store(struct class *class,
		struct class_attribute *attr,
		const char *buf, size_t size)
{
	struct vdec_s *vdec = NULL;
	unsigned int id = 0, num = 0, start = 0;
	int ret = -1;

	ret = sscanf(buf, "%d %d %d", &id, &start, &num);
	if (ret < 0) {
		pr_info("%s, parse failed\n", buf);
		return size;
	}
	if ((num == 0) || (num > YUV_MAX_DUMP_NUM)) {
		pr_info("requred yuv num %d, max %d\n",
			num, YUV_MAX_DUMP_NUM);
		return size;
	}
	vdec = vdec_get_with_id(id);
	if (vdec == NULL) {
		yuv_start[id] = start;
		yuv_num[id] = num;
		yuv_enable |= (1 << id);
		pr_info("no connected vdec.%d now, set dump ok\n", id);
		return size;
	}

	ret = dump_yuv_trig(&vdec->vfc, id, start, num);
	if (ret < 0)
		pr_info("trigger dump yuv failed\n");
	else
		pr_info("trigger dump yuv init ok, total %d frames\n", num);

	return size;
}

ssize_t dump_yuv_show(struct class *class,
		struct class_attribute *attr, char *buf)
{
	int i;
	char *pbuf = buf;

	for (i = 0; i < MAX_INSTANCE_MUN; i++) {
		pbuf += pr_info("vdec.%d, start: %d, total: %d frames\n",
			i, yuv_start[i], yuv_num[i]);
	}
	pbuf += sprintf(pbuf,
		"\nUsage: echo [id] [start] [num] > dump_yuv\n\n");
	return pbuf - buf;
}


ssize_t frame_check_store(struct class *class,
		struct class_attribute *attr,
		const char *buf, size_t size)
{
	int ret = -1;
	int on_off, id;

	ret = sscanf(buf, "%d %d", &id, &on_off);
	if (ret < 0) {
		pr_info("%s, parse failed\n", buf);
		return size;
	}
	if (id >= MAX_INSTANCE_MUN) {
		pr_info("%d out of max vdec id\n", id);
		return size;
	}
	if (on_off)
		check_enable |= (1 << id);
	else
		check_enable &= ~(1 << id);

	return size;
}

ssize_t frame_check_show(struct class *class,
		struct class_attribute *attr, char *buf)
{
	int i;
	char *pbuf = buf;

	for (i = 0; i < MAX_INSTANCE_MUN; i++) {
		pbuf += sprintf(pbuf,
			"vdec.%d\tcrc: %s\n", i,
			(check_enable & (0x01 << i))?"enabled":"--");
	}
	pbuf += sprintf(pbuf,
		"\nUsage:\techo [id]  [1:on/0:off] > frame_check\n\n");

	return pbuf - buf;
}




