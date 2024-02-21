/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __AML_LCD_TCON_SWPDF_H__
#define __AML_LCD_TCON_SWPDF_H__

#define AML_SWPDF_DBG_TIME 1
#define MATCH_INVALID_VAL 0x55555555
#define SWPDF_PATTERN_MAX 32
struct swpdf_pix_s {
	struct list_head list;
	u16 x;
	u16 y;
	u16 r;
	u16 g;
	u16 b;
	u16 ref_cnt;
};

#define SWPDF_BLOCK_W_MAX 4
#define SWPDF_BLOCK_H_MAX 4

struct swpdf_block_s {
	struct list_head list;
	struct swpdf_pix_s **pixs;
	u16 x;
	u16 y;
	u16 w;
	u16 h;
	u32 mat[SWPDF_BLOCK_H_MAX];
	u32 det[SWPDF_BLOCK_H_MAX];
};

#define SWPDF_ACT_RD_DFT 0
#define SWPDF_ACT_WR_DFT 1
#define SWPDF_ACT_WR_VAL 2

struct swpdf_act_s {
	struct list_head list;
	u32 reg;
	u32 mask;
	u32 val;
	u32 dft;
	s32 bus;
};

struct swpdf_pat_s {
	struct list_head list;
	s32 enable;
	s32 th_w;//r/g/b
	s32 th_b;//r/g/b
	s32 block_num;
	s32 act_once;
	s32 rd_dft;
	struct list_head block_list;
	struct list_head act_list;
};

struct swpdf_s {
	u8 enable;
	u8 reset;
	u8 matched;
	u32 pix_num;
	void *priv;
	struct list_head pat_list;
	struct list_head pix_list;
	struct swpdf_pix_s *cur_pix;
	struct swpdf_pat_s *last_pat;
	spinlock_t lock; /**/
#ifdef AML_SWPDF_DBG_TIME
	unsigned int dbg_tim_cnt;
	unsigned int dbg_tim_pr;
	unsigned int dbg_buf_len;
	unsigned long long tim1;
	unsigned long long tim2;
	unsigned int tim;
	char *dbg_buf;
#endif
};

#define SWPDF_DFT_BLOCK_MAX 16
#define SWPDF_DFT_ACT_MAX 16

struct swpdf_s *get_swpdf(void);
int lcd_swpdf_vs_handle(void);
void lcd_swpdf_init(struct aml_lcd_drv_s *pdrv);
struct swpdf_pat_s *swpdf_pat_create_add(struct swpdf_s *pdf, u16 th_w, u16 th_b);
struct swpdf_block_s *swpdf_block_create_add(struct swpdf_pat_s *pat,
	u16 x, u16 y, u16 w, u16 h, u32 *mat);
struct swpdf_act_s *swpdf_act_create_add(struct swpdf_pat_s *pat, u32 reg,
	u32 mask, u32 val, u32 bus);
void swpdf_act_pat(struct swpdf_pat_s *pat, s32 mode);

ssize_t lcd_swpdf_dbg_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count);
ssize_t lcd_swpdf_dbg_show(struct device *dev, struct device_attribute *attr, char *buf);

#endif
