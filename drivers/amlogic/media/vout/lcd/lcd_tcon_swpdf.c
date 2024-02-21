// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/sched/clock.h>
#include <linux/of.h>
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>
#include <linux/spinlock.h>
#include "lcd_common.h"
#include "lcd_reg.h"
#include "lcd_tcon.h"
#include "lcd_tcon_swpdf.h"

struct swpdf_s g_swpdf;

struct swpdf_s *get_swpdf(void)
{
	return &g_swpdf;
}

/* ============================================================================================= */
static inline void lcd_tcon_set_pre_curser(struct aml_lcd_drv_s *pdrv, u16 x, u16 y)
{
	lcd_tcon_setb(pdrv, 0x205, (u32)x, 0, 14);
	lcd_tcon_setb(pdrv, 0x205, (u32)y, 14, 14);
}

static inline void lcd_tcon_get_pre_curser(struct aml_lcd_drv_s *pdrv, u16 *r, u16 *g, u16 *b)
{
	*r = (u16)lcd_tcon_getb(pdrv, 0x226, 0, 12);
	*g = (u16)lcd_tcon_getb(pdrv, 0x226, 12, 12);
	*b = (u16)lcd_tcon_getb(pdrv, 0x227, 0, 12);
}

static inline void lcd_tcon_set_post_curser(struct aml_lcd_drv_s *pdrv, u16 x, u16 y)
{
	lcd_tcon_setb(pdrv, 0x12d, (u32)x, 14, 12);
	lcd_tcon_setb(pdrv, 0x12d, (u32)y, 0, 14);
}

static inline void lcd_tcon_get_post_curser(struct aml_lcd_drv_s *pdrv, u16 *r, u16 *g, u16 *b)
{
	*r = (u16)lcd_tcon_getb(pdrv, 0x140, 0, 10);
	*g = (u16)lcd_tcon_getb(pdrv, 0x140, 10, 10);
	*b = (u16)lcd_tcon_getb(pdrv, 0x140, 20, 10);
}

static struct swpdf_pat_s *find_pat_from_list(struct list_head *head, s32 id)
{
	struct swpdf_pat_s *pat;
	s32 i = 0;

	if (!head || list_empty(head))
		return NULL;

	list_for_each_entry(pat, head, list)
		if (i++ == id)
			return pat;

	return NULL;
}

/* find act from act_list by id */
static struct swpdf_block_s *find_block_from_list(struct list_head *head, s32 id)
{
	struct swpdf_block_s *block;
	s32 i = 0;

	if (!head || list_empty(head))
		return NULL;

	list_for_each_entry(block, head, list)
		if (i++ == id)
			return block;

	return NULL;
}

/* find act from act_list by id */
static struct swpdf_act_s *find_act_from_list(struct list_head *head, s32 id)
{
	struct swpdf_act_s *act;
	s32 i = 0;

	if (!head || list_empty(head))
		return NULL;

	list_for_each_entry(act, head, list)
		if (i++ == id)
			return act;

	return NULL;
}

/* find pixel form pix_list by coordinate */
static struct swpdf_pix_s *find_pix_from_list(struct list_head *head, u16 x, u16 y)
{
	struct swpdf_pix_s *pix;

	if (!head || list_empty(head))
		return NULL;

	list_for_each_entry(pix, head, list)
		if (pix->x == x && pix->y == y)
			return pix;

	return NULL;
}

/*
 * create block bufffer and add to pat block_list
 * create a w * h block at position [x, y]
 * mat: the pointer of match table
 * this function will alloc block buffer and pixel buffer
 */
struct swpdf_block_s *swpdf_block_create_add(struct swpdf_pat_s *pat,
	u16 x, u16 y, u16 w, u16 h, u32 *mat)
{
	struct swpdf_s *pdf = get_swpdf();
	struct swpdf_block_s *block = NULL;
	struct swpdf_pix_s *pix, **pixs;
	u32 cnt, i, j, m, n, k = 0;

	if (!pat || w == 0 || h == 0 || !mat)
		return NULL;

	cnt = w * h;
	block = kzalloc(sizeof(*block), GFP_KERNEL);
	pixs = kcalloc(cnt, sizeof(pix), GFP_KERNEL);
	if (!block || !pixs)
		goto __pdf_pat_block_create_add_err;

	block->pixs = pixs;
	block->x = x;
	block->y = y;
	block->w = w;
	block->h = h;

	for (j = 0, n = y; j < h; j++, n++) {
		block->mat[j] = mat[j];
		for (i = 0, m = x; i < w; i++, m++) {
			pix = find_pix_from_list(&pdf->pix_list, m, n);
			if (!pix) {
				pix = kzalloc(sizeof(*pix), GFP_KERNEL);
				if (!pix)
					goto __pdf_pat_block_create_add_err;
				pix->x = m;
				pix->y = n;
				pix->ref_cnt = 0;
				list_add_tail(&pix->list, &pdf->pix_list);
				pdf->pix_num++;
			}
			pix->ref_cnt++;
			block->pixs[k++] = pix;
		}
	}

	list_add_tail(&block->list, &pat->block_list);
	pat->block_num++;
	return block;

__pdf_pat_block_create_add_err:
	kfree(block);
	kfree(pixs);
	return NULL;
}

/* remove block and release block buffer,
 * and release pixel buffer if this pixel is only used by this block
 */
static void swpdf_block_remove(struct swpdf_pat_s *pat, struct swpdf_block_s *block)
{
	u32 i = 0, cnt = 0;
	struct swpdf_s *pdf = get_swpdf();

	if (!block)
		return;

	cnt = block->w * block->h;
	for (i = 0; i < cnt; i++) {
		block->pixs[i]->ref_cnt--;
		if (block->pixs[i]->ref_cnt == 0) {
			list_del(&block->pixs[i]->list);
			kfree(block->pixs[i]);
			pdf->pix_num--;
		}
	}
	list_del(&block->list);
	kfree(block->pixs);
	kfree(block);
	pat->block_num--;
}

/* alloc act buffer and add to pat act_list */
struct swpdf_act_s *swpdf_act_create_add(struct swpdf_pat_s *pat, u32 reg,
	u32 mask, u32 val, u32 bus)
{
	struct swpdf_act_s *act = kzalloc(sizeof(*act), GFP_KERNEL);

	if (!act)
		return NULL;
	if (!pat) {
		kfree(act);
		return NULL;
	}

	act->reg = reg;
	act->mask = mask;
	act->val = val;
	act->bus = bus;

	list_add_tail(&act->list, &pat->act_list);
	return act;
}

static void swpdf_act_remove(struct swpdf_act_s *act)
{
	if (!act)
		return;

	list_del(&act->list);
	kfree(act);
}

/* alloc pattern buffer and add to pdf pat_list */
struct swpdf_pat_s *swpdf_pat_create_add(struct swpdf_s *pdf, u16 th_w, u16 th_b)
{
	struct swpdf_pat_s *pat;

	if (!pdf)
		return NULL;

	pat = kzalloc(sizeof(*pat), GFP_KERNEL);
	if (!pat)
		return NULL;

	pat->th_w = th_w;
	pat->th_b = th_b;
	pat->enable = 1;
	INIT_LIST_HEAD(&pat->block_list);
	INIT_LIST_HEAD(&pat->act_list);
	list_add_tail(&pat->list, &pdf->pat_list);

	return pat;
}

static void swpdf_pat_destroy(struct swpdf_s *pdf, struct swpdf_pat_s *pat)
{
	struct swpdf_block_s *block, *tmpb;
	struct swpdf_act_s *act, *tmpa;

	if (!pdf || !pat)
		return;

	if (!list_empty(&pat->block_list))
		list_for_each_entry_safe(block, tmpb, &pat->block_list, list)
			swpdf_block_remove(pat, block);

	if (!list_empty(&pat->act_list))
		list_for_each_entry_safe(act, tmpa, &pat->act_list, list)
			swpdf_act_remove(act);

	list_del(&pat->list);
	kfree(pat);
}

/* ============================================================================================= */
/*
 * return a line det_val
 * pixels: rgb|rgb|rgb|rgb
 * each sub-pixel occupy 2 bits,
 * 0b00= match <= thres_b
 * 0b11= match >= thres_w
 * 0b10= match between thres_b and thres_w
 * 0b01= not use, reserved
 * ex: a 4 pixel(12-sub-pixel) serial is
 * 0x0,0x3ff,0x0 | 0x3ff,0x200,0x3ff | 0x0,0x3ff,0x3ff | 0x3ff,0x0,0x280
 * the thres_w and thres_b is 0x300, 0x100
 * then the detect value is: 0b00,0b11,0b00 | 0b11,0b10,0b11 | 0b00,0b11,0b11 | 0b11,0b00,0b10
 * combine from left to right: 0b001100111011001111110010, aka:0x33b3f2
 * so if matching line like this, set match_val to 0x33b3f2
 * as u32 match value used, the max pixel count in a line is 4
 * every line has one match_val. Eg: a block with size 3x4 has 4 match_val
 */

static inline u32 swpdf_det_line(struct swpdf_pix_s **pixs, u32 count, u16 th_w, u16 th_b)
{
	u32 i = 0, val = 0, tmp = 0;

	if (!pixs || count < 1 || count > SWPDF_BLOCK_W_MAX)
		return 0;

	for (i = 0; i < count && pixs[i]; i++, tmp = 0) {
		tmp |= (pixs[i]->r >= th_w ? 3 : pixs[i]->r <= th_b ? 0 : 2) << 4;
		tmp |= (pixs[i]->g >= th_w ? 3 : pixs[i]->g <= th_b ? 0 : 2) << 2;
		tmp |= (pixs[i]->b >= th_w ? 3 : pixs[i]->b <= th_b ? 0 : 2) << 0;
		val |= tmp << (count - i - 1) * 8;
	}

	return val;
}

/* return 1: block matched, 0: block match fail */
static inline s32 swpdf_match_block(struct swpdf_block_s *block, u16 th_w, u16 th_b)
{
	s32 i = 0, ret = 0;
	struct swpdf_pix_s **pixs = NULL;

	if (!block || block->h < 1 || !block->pixs)
		return 0;

	for (i = 0; i < block->h; i++) {
		pixs = block->pixs + block->w * i;
		block->det[i] = swpdf_det_line(pixs, block->w, th_w, th_b);
		ret += block->det[i] == block->mat[i] ? 1 : 0;
	}

	return ret == block->h;
}

/*return 1: pat matched 0: pat match fail*/
static inline s32 swpdf_match_pat(struct swpdf_pat_s *pat)
{
	struct swpdf_block_s *block = NULL;
	s32 ret = 0;

	if (!pat || list_empty(&pat->block_list))
		return 0;

	list_for_each_entry(block, &pat->block_list, list)
		ret += swpdf_match_block(block, pat->th_w, pat->th_b);

	return ret == pat->block_num;
}

/* only support LCD_REG_DBG_TCON_BUS now, it may be expanded later */
#define LCD_REG_DBG_TCON_BUS 6
void swpdf_act_pat(struct swpdf_pat_s *pat, s32 mode)
{
	struct swpdf_s *pdf = get_swpdf();
	struct aml_lcd_drv_s *pdrv = (struct aml_lcd_drv_s *)pdf->priv;
	struct swpdf_act_s *act;
	u32 val;

	if (!pdrv || !pat || list_empty(&pat->act_list))
		return;
	list_for_each_entry(act, &pat->act_list, list) {
		act->bus = LCD_REG_DBG_TCON_BUS;
		switch (act->bus) {
		case LCD_REG_DBG_TCON_BUS:
			if (mode == SWPDF_ACT_WR_DFT) {
				lcd_tcon_write(pdrv, act->reg, act->dft);
				continue;
			}

			val = lcd_tcon_read(pdrv, act->reg);
			if (mode == SWPDF_ACT_RD_DFT) {
				act->dft = val;
				pat->rd_dft = 1;
				continue;
			}

			val &= ~act->mask;
			val |= act->val;
			lcd_tcon_write(pdrv, act->reg, val);

			break;
		default:
			break;
		}
	}
}

__maybe_unused void lcd_swpdf_parse_pat(struct aml_lcd_drv_s *pdrv)
{
	struct device_node *child;
	const struct device_node *np;
	char name[64];
	u32 pat_mask = 0, th[2], blk_arr[8 * 16], act_arr[4 * 16];
	s32 blk_cnt = 0, act_cnt = 0, ret = 0, i, k;
	struct swpdf_s *pdf = get_swpdf();
	struct swpdf_pat_s *pat = NULL;
	unsigned int x, y, w, h, reg, mask, val, bus, *mat;

	if (!pdrv->dev->of_node) {
		LCDERR("dev of_node is null\n");
		return;
	}

	child = of_get_child_by_name(pdrv->dev->of_node, "swpdf");
	if (!child) {
		LCDERR("[%d]: failed to get swpdf\n", pdrv->index);
		return;
	}
	pat_mask = pdrv->config.customer_sw_pdf;
	for (i = 0; i < 32; i++, pat_mask >>= 1) {
		if (!(pat_mask & 0x1))
			continue;

		sprintf(name, "pattern_%d", i);
		np = of_get_child_by_name(child, name);
		if (!np) {
			LCDPR("[%d]: failed to get %s\n", pdrv->index, name);
			continue;
		}
		ret = of_property_read_u32_array(np, "th", th, 2);
		if (ret) {
			LCDPR("[%d]: failed to get threshold\n", pdrv->index);
			continue;
		}

		blk_cnt = of_property_count_elems_of_size(np, "block", sizeof(u32));

		ret = of_property_read_u32_array(np, "block", blk_arr, blk_cnt);
		if (ret) {
			LCDPR("[%d]: failed to get block\n", pdrv->index);
			continue;
		}

		act_cnt = of_property_count_elems_of_size(np, "act", sizeof(u32));

		ret = of_property_read_u32_array(np, "act", act_arr, act_cnt);
		if (ret) {
			LCDPR("[%d]: failed to get act\n", pdrv->index);
			continue;
		}
		pat = swpdf_pat_create_add(pdf, th[0], th[1]);
		if (!pat)
			continue;

		for (k = 0; k < blk_cnt / 8; k++) {
			x = blk_arr[k * 8 + 0];
			y = blk_arr[k * 8 + 1];
			w = blk_arr[k * 8 + 2];
			h = blk_arr[k * 8 + 3];
			mat = blk_arr + 4;
			if (!swpdf_block_create_add(pat, x, y, w, h, mat))
				LCDERR("add blk:[%d, %d, %d, %d] mat:[%08x, %08x, %08x, %08x]\n",
					x, y, w, h, mat[0], mat[1], mat[2], mat[3]);
		}

		for (k = 0; k < act_cnt / 4; k++) {
			reg = act_arr[k * 4 + 0];
			mask = act_arr[k * 4 + 1];
			val = act_arr[k * 4 + 2];
			bus = act_arr[k * 4 + 3];
			if (!swpdf_act_create_add(pat, reg, mask, val, bus))
				LCDERR("add act: reg=0x%x, mask=0x%x, val=0x%x\n", reg, mask, val);
		}
		swpdf_act_pat(pat, SWPDF_ACT_RD_DFT);
	}
}

static struct device_attribute swpdf_attr =
	__ATTR(swpdf, 0644, lcd_swpdf_dbg_show, lcd_swpdf_dbg_store);

void lcd_swpdf_init(struct aml_lcd_drv_s *pdrv)
{
	struct swpdf_s *pdf = get_swpdf();

	pdf->reset = 0;
	pdf->enable = 1;
	pdf->priv = pdrv;
	pdf->last_pat = NULL;
	pdf->cur_pix = NULL;
	INIT_LIST_HEAD(&pdf->pat_list);
	INIT_LIST_HEAD(&pdf->pix_list);
	spin_lock_init(&pdf->lock);
	if (device_create_file(pdrv->dev, &swpdf_attr))
		LCDPR("swpdf debug attr create failed\n");
	//lcd_swpdf_parse_pat(pdrv);
}

static void swpdf_reset(void)
{
	struct swpdf_s *pdf = get_swpdf();
	struct aml_lcd_drv_s *pdrv = (struct aml_lcd_drv_s *)pdf->priv;
	struct swpdf_pix_s *pix;

	pdf->reset = 0;
	if (pdf->last_pat && pdrv) {
		pdf->last_pat->act_once = 0;
		swpdf_act_pat(pdf->last_pat, SWPDF_ACT_WR_DFT);
	}
	pdf->cur_pix = NULL;
	pdf->last_pat = NULL;
	pdf->matched = 0;

	if (!list_empty(&pdf->pix_list)) {
		list_for_each_entry(pix, &pdf->pix_list, list) {
			pix->r = 0;
			pix->g = 0;
			pix->b = 0;
		}
	}
}

s32 lcd_swpdf_vs_handle(void)
{
	struct swpdf_s *pdf = get_swpdf();
	struct aml_lcd_drv_s *pdrv = pdf->priv;
	unsigned long flags;
	struct swpdf_pix_s *pix;
	struct swpdf_pat_s *pat;
	char match = 0;

	if (!pdrv || !pdf->enable || list_empty(&pdf->pix_list) || list_empty(&pdf->pat_list))
		return -1;

	if (!spin_trylock_irqsave(&pdf->lock, flags))
		return -1;

#ifdef AML_SWPDF_DBG_TIME
	if (pdf->dbg_tim_pr)
		pdf->tim1 = sched_clock();
#endif
	if (pdf->cur_pix)
		pix = pdf->cur_pix;
	else
		pix = list_first_entry(&pdf->pix_list, struct swpdf_pix_s, list);

	lcd_tcon_get_pre_curser(pdrv, &pix->r, &pix->g, &pix->b);

	list_for_each_entry(pat, &pdf->pat_list, list) {
		if (pat->enable && swpdf_match_pat(pat)) {
			if (!pat->act_once) {
				pat->act_once = 1;
				if (!pat->rd_dft)
					swpdf_act_pat(pat, SWPDF_ACT_RD_DFT);
				swpdf_act_pat(pat, SWPDF_ACT_WR_VAL);
				if (lcd_debug_print_flag & LCD_DBG_PR_ISR)
					LCDPR("pat matched, change state\n");
			}
			match = 1;
			pdf->last_pat = pat;
			break;//break if matched a pattern
		}
	}
	if (pdf->matched && !match) {
		if (pdf->last_pat) {
			pdf->last_pat->act_once = 0;
			if (!pat->rd_dft)
				swpdf_act_pat(pat, SWPDF_ACT_RD_DFT);
			swpdf_act_pat(pdf->last_pat, SWPDF_ACT_WR_DFT);
			if (lcd_debug_print_flag & LCD_DBG_PR_ISR)
				LCDPR("no pat match, reset state\n");
		}
		pdf->last_pat = NULL;
	}
	pdf->matched = match;

	if (list_is_last(&pix->list, &pdf->pix_list))
		pix = list_first_entry(&pdf->pix_list, struct swpdf_pix_s, list);
	else
		pix = list_next_entry(pix, list);

	pdf->cur_pix = pix;

	lcd_tcon_set_pre_curser(pdrv, pix->x, pix->y);

//swpdf_vs_handle_exit:
	spin_unlock_irqrestore(&pdf->lock, flags);
#ifdef AML_SWPDF_DBG_TIME
	if (pdf->dbg_tim_pr && pdf->dbg_buf) {
		pdf->tim2 = sched_clock();
		pdf->tim = lcd_do_div(pdf->tim2 - pdf->tim1, 1000);
		pdf->dbg_buf_len += sprintf(pdf->dbg_buf + pdf->dbg_buf_len, "%05u ", pdf->tim);
		pdf->dbg_tim_cnt++;
		if (!(pdf->dbg_tim_cnt % 10))
			pdf->dbg_buf_len += sprintf(pdf->dbg_buf + pdf->dbg_buf_len, "\n");
		if (pdf->dbg_tim_cnt >= 120) {
			pdf->dbg_buf_len = 0;
			pdf->dbg_tim_cnt = 0;
			if (pdf->dbg_tim_pr)
				LCDPR("swpdf 120 frames time(us) :\n%s\n", pdf->dbg_buf);
		}
	}
#endif
	return 0;
}

/*===============================================================================================*/
const char *swpdf_help_str = {
	"echo enable 1/0 > swpdf\n"
	"echo pattern create th_w th_b > swpdf\n"
	"echo pattern 0 destroy > swpdf\n"
	"echo pattern 0 enable 1/0 > swpdf\n"
	"echo pattern 0 thres th_w th_b > swpdf\n"
	"echo pattern 0 add_b x y w h <val0 val1 val2 val3> > swpdf\n"
	"echo pattern 0 rm_b id > swpdf\n"
	"echo pattern 0 add_a reg mask val bus(must be 6) > swpdf\n"
	"echo pattern 0 rm_a id > swpdf\n"
};

#define SWPDF_MAX_PARAM 20

void swpdf_param_restore(struct aml_lcd_drv_s *pdrv, char **parm)
{
	unsigned long flags = 0;
	struct swpdf_s *pdf = get_swpdf();
	struct swpdf_pat_s *pat = NULL;
	struct swpdf_block_s *b = NULL;
	struct swpdf_act_s *a = NULL;
	s32 idp = 0, idb = 0, ida = 0;
	u32 i, x, y, w, h, mat[SWPDF_BLOCK_H_MAX] = {MATCH_INVALID_VAL};
	u32 thw, thb, temp, reg, mask, val, bus;
	s32 ret = 0;

	if (!parm || !pdrv)
		return;

	if (strcmp(parm[0], "help") == 0) {
		pr_info("%s\n", swpdf_help_str);
		return;
	}

	spin_lock_irqsave(&pdf->lock, flags);
	if (strcmp(parm[0], "dbg_time") == 0) {
#ifdef AML_SWPDF_DBG_TIME
		ret = kstrtouint(parm[1], 10, &temp);
		if (temp && !pdf->dbg_buf) {
			pdf->dbg_buf = kzalloc(4096, GFP_KERNEL);
			pdf->dbg_tim_cnt = 0;
			pdf->dbg_buf_len = 0;
		}
		if (!pdf->dbg_buf)
			pdf->dbg_tim_pr = 0;
		else
			pdf->dbg_tim_pr = temp;
		if (pdf->dbg_tim_pr == 0) {
			pdf->dbg_tim_cnt = 0;
			pdf->dbg_buf_len = 0;
		}
#endif
		goto __pdf_store_exit;
	} else if (strcmp(parm[0], "enable") == 0) {
		ret = kstrtouint(parm[1], 10, &temp);
		if (ret || pdf->enable == !!temp)
			goto  __pdf_store_exit;

		pdf->enable = !!temp;
		swpdf_reset();
		LCDPR("pdf %sable\n", temp ? "en" : "dis");
		goto __pdf_store_exit;
	} else if (strcmp(parm[0], "pattern") == 0) {
		if (!parm[1] || !parm[2])
			goto  __pdf_store_exit;

		if (strcmp(parm[1], "create") == 0) {
			ret |= kstrtouint(parm[2], 16, &thw);
			ret |= kstrtouint(parm[3], 16, &thb);
			if (ret) {
				thw = 0;
				thb = 0;
			}
			if (!swpdf_pat_create_add(pdf, thw, thb))
				LCDERR("pdf new pat create failed\n");
			goto  __pdf_store_exit;
		}

		ret = kstrtouint(parm[1], 10, &idp);
		if (ret) {
			LCDERR("error find idp\n");
			goto  __pdf_store_exit;
		}

		pat = find_pat_from_list(&pdf->pat_list, idp);
			if (!pat) {
				LCDERR("error find pat\n");
				goto  __pdf_store_exit;
			}

		if (strcmp(parm[2], "destroy") == 0) {
			swpdf_pat_destroy(pdf, pat);
			LCDPR("pdf pat %d destroyed\n", idp);
		} else if (strcmp(parm[2], "enable") == 0 && parm[3]) {
			ret = kstrtouint(parm[3], 10, &temp);
			if (ret || pat->enable == !!temp)
				goto  __pdf_store_exit;

			swpdf_reset();
			pat->enable = !!temp;
			LCDPR("pdf pat %d enable=%d\n", idp, pat->enable);
		} else if (strcmp(parm[2], "add_b") == 0) {
			if (!parm[3] || !parm[4] || !parm[5] || !parm[6])
				goto  __pdf_store_exit;
			ret |= kstrtouint(parm[3], 10, &x);
			ret |= kstrtouint(parm[4], 10, &y);
			ret |= kstrtouint(parm[5], 10, &w);
			ret |= kstrtouint(parm[6], 10, &h);

			if (ret || w > SWPDF_BLOCK_W_MAX || h > SWPDF_BLOCK_H_MAX)
				goto  __pdf_store_exit;

			for (i = 0; i < h; i++) {
				if (!parm[7 + i])
					goto  __pdf_store_exit;
				ret |= kstrtouint(parm[7 + i], 16, &mat[i]);
			}
			if (ret)
				goto  __pdf_store_exit;

			if (!swpdf_block_create_add(pat, x, y, w, h, mat)) {
				LCDERR("blk add failed\n");
				goto  __pdf_store_exit;
			}
			swpdf_reset();
			LCDPR("pat %d add blk:[%d %d %d %d] mat:0x[%08x, %08x, %08x, %08x]\n",
				idp, x, y, w, h, mat[0], mat[1], mat[2], mat[3]);
		} else if (strcmp(parm[2], "rm_b") == 0 && parm[3]) {
			ret = kstrtouint(parm[3], 10, &idb);
			if (ret)
				goto  __pdf_store_exit;

			b = find_block_from_list(&pat->block_list, idb);
			if (!b)
				goto  __pdf_store_exit;

			swpdf_reset();
			LCDPR("pat %d rm blk:[%d %d %d %d] mat:0x[%08x, %08x,%08x,%08x]\n",
				idp, b->x, b->y, b->w, b->h,
				b->mat[0], b->mat[1], b->mat[2], b->mat[3]);
			swpdf_block_remove(pat, b);
		} else if (strcmp(parm[2], "thres") == 0) {
			if (!parm[3] || !parm[4])
				goto  __pdf_store_exit;
			ret |= kstrtouint(parm[3], 16, &thw);
			ret |= kstrtouint(parm[4], 16, &thb);
			if (ret)
				goto  __pdf_store_exit;

			swpdf_reset();
			pat->th_w = thw;
			pat->th_b = thb;
			LCDPR("pat %d threshold:0x%x-0x%x\n", idp, thw, thb);
		} else if (strcmp(parm[2], "add_a") == 0) {
			if (!parm[3] || !parm[4] || !parm[5] || !parm[6])
				goto  __pdf_store_exit;

			ret |= kstrtouint(parm[3], 16, &reg);
			ret |= kstrtouint(parm[4], 16, &mask);
			ret |= kstrtouint(parm[5], 16, &val);
			ret |= kstrtouint(parm[6], 16, &bus);
			if (ret)
				goto  __pdf_store_exit;

			swpdf_reset();
			if (!swpdf_act_create_add(pat, reg, mask, val, bus))
				LCDERR("pat %d failed to add act: reg=0x%x, mask=0x%x, val=0x%x\n",
					idp, reg, mask, val);
			swpdf_act_pat(pat, SWPDF_ACT_RD_DFT);
			LCDPR("pat %d add act:reg=0x%x mask=0x%x val=0x%x\n",
				idp, reg, mask, val);
		} else if (strcmp(parm[2], "rm_a") == 0 && parm[3]) {
			ret = kstrtouint(parm[3], 10, &ida);
			if (ret)
				goto  __pdf_store_exit;

			a = find_act_from_list(&pat->act_list, ida);
			if (!a)
				goto __pdf_store_exit;

			swpdf_reset();
			LCDPR("pat %d rm act:reg=0x%x mask=0x%x val=0x%x dft=0x%x\n",
				idp, a->reg, a->mask, a->val, a->dft);
			swpdf_act_remove(a);
		}
	}

__pdf_store_exit:
	spin_unlock_irqrestore(&pdf->lock, flags);
}

ssize_t swpdf_param_show(char *buf)
{
	struct swpdf_s *pdf = get_swpdf();
	struct swpdf_pat_s *pat = NULL;
	struct swpdf_block_s *b = NULL;
	struct swpdf_act_s *a = NULL;
	//struct swpdf_pix_s *pix = NULL;
	s32 len = 0, i, j;

	len += sprintf(buf + len,
		"pdf: en=%d rst=%d matched=%d, pix_num=%d\n\n",
		pdf->enable, pdf->reset, pdf->matched, pdf->pix_num);
/*
 *	i = 0;
 *	if (!list_empty(&pdf->pix_list)) {
 *		list_for_each_entry(pix, &pdf->pix_list, list) {
 *			len += sprintf(buf + len,
 *				"pix[%d]:x=%04d, y=%04d, r=0x%03x, g=0x%03x, b=0x%03x, ref=%d\n",
 *				i++, pix->x, pix->y, pix->r, pix->g, pix->b, pix->ref_cnt);
 *		}
 *	}
 */
	i = 0;
	if (list_empty(&pdf->pat_list))
		goto __pdf_param_show_exit;

	list_for_each_entry(pat, &pdf->pat_list, list) {
		len += sprintf(buf + len,
			"\npat[%d]: en=%d, block_num=%d, thw=0x%08x, thb=0x%08x\n",
			i++, pat->enable, pat->block_num, pat->th_w, pat->th_b);

		if (!list_empty(&pat->block_list)) {
			j = 0;
			list_for_each_entry(b, &pat->block_list, list) {
				len += sprintf(buf + len, "blk[%d]: x:%04d, y:%04d, w:%d, h:%d, ",
					j++, b->x, b->y, b->w, b->h);
				len += sprintf(buf + len, "mat:[0x%08x, 0x%08x,0x%08x,0x%08x], ",
					b->mat[0], b->mat[1], b->mat[2], b->mat[3]);
				len += sprintf(buf + len, "det:[0x%08x, 0x%08x,0x%08x,0x%08x]\n",
					b->det[0], b->det[1], b->det[2], b->det[3]);
			}
			len += sprintf(buf + len, "\n");
		}

		if (!list_empty(&pat->act_list)) {
			j = 0;
			list_for_each_entry(a, &pat->act_list, list) {
				len += sprintf(buf + len,
					"act[%d]: reg:0x%x, mask:0x%x, val:0x%x, dft:0x%x\n",
					j++, a->reg, a->mask, a->val, a->dft);
			}
			len += sprintf(buf + len, "\n");
		}
	}
__pdf_param_show_exit:
	return len;
}

ssize_t lcd_swpdf_dbg_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	char *buf_orig;
	char **parm = NULL;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (!buf_orig)
		return count;

	parm = kcalloc(SWPDF_MAX_PARAM, sizeof(char *), GFP_KERNEL);
	if (!parm)
		goto __swpdf_dbg_store_exit;

	lcd_debug_parse_param(buf_orig, parm, SWPDF_MAX_PARAM);
	swpdf_param_restore(pdrv, parm);

__swpdf_dbg_store_exit:
	kfree(parm);
	kfree(buf_orig);
	return count;
}

ssize_t lcd_swpdf_dbg_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return swpdf_param_show(buf);
}

