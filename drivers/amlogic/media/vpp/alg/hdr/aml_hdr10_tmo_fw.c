// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/amlogic/media/amvecm/hdr10_tmo_alg.h>
#include "aml_hdr10_tmo_fw.h"

static int pr_tmo_en;
module_param(pr_tmo_en, int, 0664);
MODULE_PARM_DESC(pr_tmo_en, "\n pr_tmo_en\n");

#define pr_tmo_dbg(fmt, args...)\
	do {\
		if (pr_tmo_en)\
			pr_info("HDR10_TMO: " fmt, ##args);\
	} while (0)

static unsigned int eo_lut_28[143] = {
	0, 1, 4, 7, 12, 17, 24, 31, 39, 49, 59, 71, 83, 97,
	111, 127, 144, 320, 578, 931, 1392, 1977, 2701, 3582,
	4638, 5891, 7362, 9076, 11058, 13337, 15942, 18907,
	22266, 26058, 30324, 35106, 40454, 46418, 53052, 60415,
	68572, 77590, 87542, 98508, 110570, 123821, 138356, 154282,
	171708, 190757, 211556, 234243, 258967, 285886, 315170, 347002,
	381578, 419105, 459810, 503932, 551729, 603476, 659470, 720026,
	785484, 856207, 932584, 1015032, 1103997, 1199956, 1303424,
	1414947, 1535113, 1664552, 1803938, 1953994, 2115494, 2289267,
	2476202, 2677251, 2893435, 3125848, 3375662, 3644136, 3932616,
	4242547, 4575481, 4933078, 5317122, 5729526, 6172341, 6647771,
	7158178, 7706100, 8294261, 8925586, 9603216, 10330526, 11111139,
	11948953, 12848153, 13813241, 14849055, 15960799, 17154072, 18434897,
	19809755, 21285624, 22870015, 24571018, 26397348, 28358395, 30464279,
	32725910, 35155053, 37764399, 40567642, 43579559, 46816107, 50294518,
	54033406, 58052888, 62374710, 67022388, 72021358, 77399146, 83185550,
	89412833, 96115952, 103332784, 111104394, 119475320, 128493881, 138212527,
	148688209, 159982795, 172163524, 185303500, 199482248, 214786307,
	231309901, 249155666, 268435456
};

static struct aml_tmo_reg_sw tmo_reg = {
	.tmo_en = 1,
	.reg_highlight = 185,
	.reg_light_th = 110,
	.reg_hist_th = 120,
	.reg_highlight_th1 = 70,
	.reg_highlight_th2 = 12,
	.reg_display_e = 700,
	.reg_middle_a = 102,
	.reg_middle_a_adj = 315,
	.reg_middle_b = 27,
	.reg_middle_s = 64,
	.reg_max_th1 = 50,
	.reg_middle_th = 500,
	.reg_thold1 = 300,
	.reg_thold2 = 30,
	.reg_thold3 = 100,
	.reg_thold4 = 250,
	.reg_max_th2 = 18,
	.reg_pnum_th = 5000,
	.reg_hl0 = 60,
	.reg_hl1 = 38,
	.reg_hl2 = 102,
	.reg_hl3 = 80,
	.reg_display_adj = 115,
	.reg_low_adj = 38,
	.reg_high_en = 2,
	.reg_high_adj1 = 13,
	.reg_high_adj2 = 77,
	.reg_high_maxdiff = 64,
	.reg_high_mindiff = 8,
	.reg_avg_th = 0,
	.reg_avg_adj = 150,
	.alpha = 250,
	.eo_lut = eo_lut_28,

	/*param for alg func*/
	.w = 3840,
	.h = 2160,
	.oo_gain = NULL,
	.pre_hdr10_tmo_alg = NULL,
};

static void change_param(int val)
{
	if (val == 0 || val == 1) {
		tmo_reg.reg_highlight = 185;
		tmo_reg.reg_middle_a = 102;
		tmo_reg.reg_low_adj = 38;
		tmo_reg.reg_hl0 = 60;
		tmo_reg.reg_hl1 = 38;
		tmo_reg.reg_hl2 = 102;
		tmo_reg.reg_hl3 = 80;
	} else if (val == 2) {
		tmo_reg.reg_highlight = 832;
		tmo_reg.reg_middle_a = 80;
		tmo_reg.reg_low_adj = 30;
		tmo_reg.reg_hl0 = 40;
		tmo_reg.reg_hl1 = 60;
		tmo_reg.reg_hl2 = 80;
		tmo_reg.reg_hl3 = 90;
	}

	pr_info("tmo_reg changed from %d to %d\n", tmo_reg.tmo_en, val);
}

void hdr10_tmo_set_reg(struct hdr_tmo_sw *pre_tmo_reg)
{
	if (!pre_tmo_reg) {
		pr_tmo_dbg("pre_tmo_reg is NULL\n");
		pr_tmo_en = 0;
		return;
	}

	pr_tmo_dbg("tmo_en = %d\n", pre_tmo_reg->tmo_en);
	pr_tmo_dbg("reg_highlight = %d\n", pre_tmo_reg->reg_highlight);
	pr_tmo_dbg("reg_light_th = %d\n", pre_tmo_reg->reg_light_th);
	pr_tmo_dbg("reg_hist_th = %d\n", pre_tmo_reg->reg_hist_th);
	pr_tmo_dbg("reg_highlight_th1 = %d\n", pre_tmo_reg->reg_highlight_th1);
	pr_tmo_dbg("reg_highlight_th2 = %d\n", pre_tmo_reg->reg_highlight_th2);
	pr_tmo_dbg("reg_display_e = %d\n", pre_tmo_reg->reg_display_e);
	pr_tmo_dbg("reg_middle_a = %d\n", pre_tmo_reg->reg_middle_a);
	pr_tmo_dbg("reg_middle_a_adj = %d\n", pre_tmo_reg->reg_middle_a_adj);
	pr_tmo_dbg("reg_middle_b = %d\n", pre_tmo_reg->reg_middle_b);
	pr_tmo_dbg("reg_middle_s = %d\n", pre_tmo_reg->reg_middle_s);
	pr_tmo_dbg("reg_max_th1 = %d\n", pre_tmo_reg->reg_max_th1);
	pr_tmo_dbg("reg_middle_th = %d\n", pre_tmo_reg->reg_middle_th);
	pr_tmo_dbg("reg_thold2 = %d\n", pre_tmo_reg->reg_thold2);
	pr_tmo_dbg("reg_thold3 = %d\n", pre_tmo_reg->reg_thold3);
	pr_tmo_dbg("reg_thold4 = %d\n", pre_tmo_reg->reg_thold4);
	pr_tmo_dbg("reg_thold1 = %d\n", pre_tmo_reg->reg_thold1);
	pr_tmo_dbg("reg_max_th2 = %d\n", pre_tmo_reg->reg_max_th2);
	pr_tmo_dbg("reg_pnum_th = %d\n", pre_tmo_reg->reg_pnum_th);
	pr_tmo_dbg("reg_hl0 = %d\n", pre_tmo_reg->reg_hl0);
	pr_tmo_dbg("reg_hl1 = %d\n", pre_tmo_reg->reg_hl1);
	pr_tmo_dbg("reg_hl2 = %d\n", pre_tmo_reg->reg_hl2);
	pr_tmo_dbg("reg_hl3 = %d\n", pre_tmo_reg->reg_hl3);
	pr_tmo_dbg("reg_display_adj = %d\n", pre_tmo_reg->reg_display_adj);
	pr_tmo_dbg("reg_low_adj = %d\n", pre_tmo_reg->reg_low_adj);
	pr_tmo_dbg("reg_high_en = %d\n", pre_tmo_reg->reg_high_en);
	pr_tmo_dbg("reg_high_adj1 = %d\n", pre_tmo_reg->reg_high_adj1);
	pr_tmo_dbg("reg_high_adj2 = %d\n", pre_tmo_reg->reg_high_adj2);
	pr_tmo_dbg("reg_high_maxdiff = %d\n", pre_tmo_reg->reg_high_maxdiff);
	pr_tmo_dbg("reg_high_mindiff = %d\n", pre_tmo_reg->reg_high_mindiff);
	pr_tmo_dbg("reg_avg_th = %d\n", pre_tmo_reg->reg_avg_th);
	pr_tmo_dbg("reg_avg_adj = %d\n", pre_tmo_reg->reg_avg_adj);
	pr_tmo_dbg("alpha = %d\n", pre_tmo_reg->alpha);

	if (tmo_reg.tmo_en != pre_tmo_reg->tmo_en)
		change_param((int)pre_tmo_reg->tmo_en);

	tmo_reg.tmo_en = pre_tmo_reg->tmo_en;
	tmo_reg.reg_highlight = pre_tmo_reg->reg_highlight;
	tmo_reg.reg_hist_th = pre_tmo_reg->reg_hist_th;
	tmo_reg.reg_light_th = pre_tmo_reg->reg_light_th;
	tmo_reg.reg_highlight_th1 = pre_tmo_reg->reg_highlight_th1;
	tmo_reg.reg_highlight_th2 = pre_tmo_reg->reg_highlight_th2;
	tmo_reg.reg_display_e = pre_tmo_reg->reg_display_e;
	tmo_reg.reg_middle_a = pre_tmo_reg->reg_middle_a;
	tmo_reg.reg_middle_a_adj = pre_tmo_reg->reg_middle_a_adj;
	tmo_reg.reg_middle_b = pre_tmo_reg->reg_middle_b;
	tmo_reg.reg_middle_s = pre_tmo_reg->reg_middle_s;
	tmo_reg.reg_max_th1 = pre_tmo_reg->reg_max_th1;
	tmo_reg.reg_middle_th = pre_tmo_reg->reg_middle_th;
	tmo_reg.reg_thold1 = pre_tmo_reg->reg_thold1;
	tmo_reg.reg_thold2 = pre_tmo_reg->reg_thold2;
	tmo_reg.reg_thold3 = pre_tmo_reg->reg_thold3;
	tmo_reg.reg_thold4 = pre_tmo_reg->reg_thold4;
	tmo_reg.reg_max_th2 = pre_tmo_reg->reg_max_th2;
	tmo_reg.reg_pnum_th = pre_tmo_reg->reg_pnum_th;
	tmo_reg.reg_hl0 = pre_tmo_reg->reg_hl0;
	tmo_reg.reg_hl1 = pre_tmo_reg->reg_hl1;
	tmo_reg.reg_hl2 = pre_tmo_reg->reg_hl2;
	tmo_reg.reg_hl3 = pre_tmo_reg->reg_hl3;
	tmo_reg.reg_display_adj = pre_tmo_reg->reg_display_adj;
	tmo_reg.reg_avg_th = pre_tmo_reg->reg_avg_th;
	tmo_reg.reg_avg_adj = pre_tmo_reg->reg_avg_adj;
	tmo_reg.reg_low_adj = pre_tmo_reg->reg_low_adj;
	tmo_reg.reg_high_en = pre_tmo_reg->reg_high_en;
	tmo_reg.reg_high_adj1 = pre_tmo_reg->reg_high_adj1;
	tmo_reg.reg_high_adj2 = pre_tmo_reg->reg_high_adj2;
	tmo_reg.reg_high_maxdiff = pre_tmo_reg->reg_high_maxdiff;
	tmo_reg.reg_high_mindiff = pre_tmo_reg->reg_high_mindiff;
	tmo_reg.alpha = pre_tmo_reg->alpha;
}

/*struct hdr_tmo_sw pre_tmo_reg_s;*/
void hdr10_tmo_get_reg(struct hdr_tmo_sw *pre_tmo_reg_s)
{
	pre_tmo_reg_s->tmo_en = tmo_reg.tmo_en;
	pre_tmo_reg_s->reg_highlight = tmo_reg.reg_highlight;
	pre_tmo_reg_s->reg_hist_th = tmo_reg.reg_hist_th;
	pre_tmo_reg_s->reg_light_th = tmo_reg.reg_light_th;
	pre_tmo_reg_s->reg_highlight_th1 = tmo_reg.reg_highlight_th1;
	pre_tmo_reg_s->reg_highlight_th2 = tmo_reg.reg_highlight_th2;
	pre_tmo_reg_s->reg_display_e = tmo_reg.reg_display_e;
	pre_tmo_reg_s->reg_middle_a = tmo_reg.reg_middle_a;
	pre_tmo_reg_s->reg_middle_a_adj = tmo_reg.reg_middle_a_adj;
	pre_tmo_reg_s->reg_middle_b = tmo_reg.reg_middle_b;
	pre_tmo_reg_s->reg_middle_s = tmo_reg.reg_middle_s;
	pre_tmo_reg_s->reg_max_th1 = tmo_reg.reg_max_th1;
	pre_tmo_reg_s->reg_middle_th = tmo_reg.reg_middle_th;
	pre_tmo_reg_s->reg_thold1 = tmo_reg.reg_thold1;
	pre_tmo_reg_s->reg_thold2 = tmo_reg.reg_thold2;
	pre_tmo_reg_s->reg_thold3 = tmo_reg.reg_thold3;
	pre_tmo_reg_s->reg_thold4 = tmo_reg.reg_thold4;
	pre_tmo_reg_s->reg_max_th2 = tmo_reg.reg_max_th2;
	pre_tmo_reg_s->reg_pnum_th = tmo_reg.reg_pnum_th;
	pre_tmo_reg_s->reg_hl0 = tmo_reg.reg_hl0;
	pre_tmo_reg_s->reg_hl1 = tmo_reg.reg_hl1;
	pre_tmo_reg_s->reg_hl2 = tmo_reg.reg_hl2;
	pre_tmo_reg_s->reg_hl3 = tmo_reg.reg_hl3;
	pre_tmo_reg_s->reg_display_adj = tmo_reg.reg_display_adj;
	pre_tmo_reg_s->reg_avg_th = tmo_reg.reg_avg_th;
	pre_tmo_reg_s->reg_avg_adj = tmo_reg.reg_avg_adj;
	pre_tmo_reg_s->reg_low_adj = tmo_reg.reg_low_adj;
	pre_tmo_reg_s->reg_high_en = tmo_reg.reg_high_en;
	pre_tmo_reg_s->reg_high_adj1 = tmo_reg.reg_high_adj1;
	pre_tmo_reg_s->reg_high_adj2 = tmo_reg.reg_high_adj2;
	pre_tmo_reg_s->reg_high_maxdiff = tmo_reg.reg_high_maxdiff;
	pre_tmo_reg_s->reg_high_mindiff = tmo_reg.reg_high_mindiff;
	pre_tmo_reg_s->alpha = tmo_reg.alpha;
}

static char hdr_tmo_debug_usage_str[33][25] = {
	"tmo_en = ",
	"reg_highlight = ",
	"reg_light_th = ",
	"reg_hist_th = ",
	"reg_highlight_th1 = ",
	"reg_highlight_th2 = ",
	"reg_display_e = ",
	"reg_middle_a = ",
	"reg_middle_a_adj = ",
	"reg_middle_b = ",
	"reg_middle_s = ",
	"reg_max_th1 = ",
	"reg_middle_th = ",
	"reg_thold2 = ",
	"reg_thold3 = ",
	"reg_thold4 = ",
	"reg_thold1 = ",
	"reg_max_th2 = ",
	"reg_pnum_th = ",
	"reg_hl0 = ",
	"reg_hl1 = ",
	"reg_hl2 = ",
	"reg_hl3 = ",
	"reg_display_adj = ",
	"reg_low_adj = ",
	"reg_high_en = ",
	"reg_high_adj1 = ",
	"reg_high_adj2 = ",
	"reg_high_maxdiff = ",
	"reg_high_mindiff = ",
	"reg_avg_th = ",
	"reg_avg_adj = ",
	"alpha = "
};

static void tmo_reg_get_arr(int *arry)
{
	arry[0] = tmo_reg.tmo_en;
	arry[1] = tmo_reg.reg_highlight;
	arry[2] = tmo_reg.reg_light_th;
	arry[3] = tmo_reg.reg_hist_th;
	arry[4] = tmo_reg.reg_highlight_th1;
	arry[5] = tmo_reg.reg_highlight_th2;
	arry[6] = tmo_reg.reg_display_e;
	arry[7] = tmo_reg.reg_middle_a;
	arry[8] = tmo_reg.reg_middle_a_adj;
	arry[9] = tmo_reg.reg_middle_b;
	arry[10] = tmo_reg.reg_middle_s;
	arry[11] = tmo_reg.reg_max_th1;
	arry[12] = tmo_reg.reg_middle_th;
	arry[13] = tmo_reg.reg_thold2;
	arry[14] = tmo_reg.reg_thold3;
	arry[15] = tmo_reg.reg_thold4;
	arry[16] = tmo_reg.reg_thold1;
	arry[17] = tmo_reg.reg_max_th2;
	arry[18] = tmo_reg.reg_pnum_th;
	arry[19] = tmo_reg.reg_hl0;
	arry[20] = tmo_reg.reg_hl1;
	arry[21] = tmo_reg.reg_hl2;
	arry[22] = tmo_reg.reg_hl3;
	arry[23] = tmo_reg.reg_display_adj;
	arry[24] = tmo_reg.reg_low_adj;
	arry[25] = tmo_reg.reg_high_en;
	arry[26] = tmo_reg.reg_high_adj1;
	arry[27] = tmo_reg.reg_high_adj2;
	arry[28] = tmo_reg.reg_high_maxdiff;
	arry[29] = tmo_reg.reg_high_mindiff;
	arry[30] = tmo_reg.reg_avg_th;
	arry[31] = tmo_reg.reg_avg_adj;
	arry[32] = tmo_reg.alpha;
}

static int arry_int2str(int val, char *str_tmp)
{
	int temp;
	char str[10];
	int len = 0, count = 0;

	if (val == 0) {
		str_tmp[count++] = '0';
		return count;
	}

	while (val) {
		temp = val % 10;
		str[len++] = temp + '0';
		val /= 10;
	}

	str[len] = '\0';
	while (len) {
		str_tmp[count++] = str[len - 1];
		len--;
	}

	str_tmp[count] = '\0';
	return count;
}

void hdr10_tmo_adb_show(char *str)
{
	int tmo_val[33];
	int i = 0, j = 0, k = 0;
	int len1 = 0, len2 = 0;
	char str_tmp[25];

	tmo_reg_get_arr(tmo_val);

	for (i = 0; i < 33; i++) {
		len1 = strlen(hdr_tmo_debug_usage_str[i]);
		/*copy str from hdr_tmo_debug_usage_str*/
		for (k = 0; k < len1; k++)
			str[j++] = hdr_tmo_debug_usage_str[i][k];

		len2 = arry_int2str(tmo_val[i], str_tmp);

		/*copy tmo_val*/
		for (k = 0; k < len2; k++)
			str[j++] = str_tmp[k];

		str[j++] = '\n';
	}

	str[j] = '\0';
}

struct aml_tmo_reg_sw *tmo_fw_param_get(void)
{
	return &tmo_reg;
};
EXPORT_SYMBOL(tmo_fw_param_get);

void hdr10_tmo_gen_data(u32 *oo_gain, int *hdr_hist)
{
	struct aml_tmo_reg_sw *pre_tmo_reg;

	pre_tmo_reg = tmo_fw_param_get();
	if (!pre_tmo_reg->pre_hdr10_tmo_alg)
		pr_tmo_dbg("%s: hdr10_tmo alg func is NULL\n", __func__);
	else
		tmo_reg.pre_hdr10_tmo_alg(pre_tmo_reg, oo_gain, hdr_hist);
}

void hdr10_tmo_params_show(void)
{
	pr_info("tmo_en = %d\n", tmo_reg.tmo_en);
	pr_info("reg_highlight = %d\n", tmo_reg.reg_highlight);
	pr_info("reg_light_th = %d\n", tmo_reg.reg_light_th);
	pr_info("reg_hist_th = %d\n", tmo_reg.reg_hist_th);
	pr_info("reg_highlight_th1 = %d\n", tmo_reg.reg_highlight_th1);
	pr_info("reg_highlight_th2 = %d\n", tmo_reg.reg_highlight_th2);
	pr_info("reg_display_e = %d\n", tmo_reg.reg_display_e);
	pr_info("reg_middle_a = %d\n", tmo_reg.reg_middle_a);
	pr_info("reg_middle_a_adj = %d\n", tmo_reg.reg_middle_a_adj);
	pr_info("reg_middle_b = %d\n", tmo_reg.reg_middle_b);
	pr_info("reg_middle_s = %d\n", tmo_reg.reg_middle_s);
	pr_info("reg_max_th1 = %d\n", tmo_reg.reg_max_th1);
	pr_info("reg_middle_th = %d\n", tmo_reg.reg_middle_th);
	pr_info("reg_thold2 = %d\n", tmo_reg.reg_thold2);
	pr_info("reg_thold3 = %d\n", tmo_reg.reg_thold3);
	pr_info("reg_thold4 = %d\n", tmo_reg.reg_thold4);
	pr_info("reg_thold1 = %d\n", tmo_reg.reg_thold1);
	pr_info("reg_max_th2 = %d\n", tmo_reg.reg_max_th2);
	pr_info("reg_pnum_th = %d\n", tmo_reg.reg_pnum_th);
	pr_info("reg_hl0 = %d\n", tmo_reg.reg_hl0);
	pr_info("reg_hl1 = %d\n", tmo_reg.reg_hl1);
	pr_info("reg_hl2 = %d\n", tmo_reg.reg_hl2);
	pr_info("reg_hl3 = %d\n", tmo_reg.reg_hl3);
	pr_info("reg_display_adj = %d\n", tmo_reg.reg_display_adj);
	pr_info("reg_low_adj = %d\n", tmo_reg.reg_low_adj);
	pr_info("reg_high_en = %d\n", tmo_reg.reg_high_en);
	pr_info("reg_high_adj1 = %d\n", tmo_reg.reg_high_adj1);
	pr_info("reg_high_adj2 = %d\n", tmo_reg.reg_high_adj2);
	pr_info("reg_high_maxdiff = %d\n", tmo_reg.reg_high_maxdiff);
	pr_info("reg_high_mindiff = %d\n", tmo_reg.reg_high_mindiff);
	pr_info("reg_avg_th = %d\n", tmo_reg.reg_avg_th);
	pr_info("reg_avg_adj = %d\n", tmo_reg.reg_avg_adj);
	pr_info("alpha = %d\n", tmo_reg.alpha);
}

int hdr10_tmo_debug(char **parm)
{
	long val;

	if (!strcmp(parm[0], "tmo_en")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;

		if (tmo_reg.tmo_en != (int)val)
			change_param((int)val);

		tmo_reg.tmo_en = (int)val;
		pr_tmo_dbg("tmo_en = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "highlight")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;

		tmo_reg.reg_highlight = (int)val;
		pr_tmo_dbg("reg_highlight = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "light_th")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;

		tmo_reg.reg_light_th = (int)val;
		pr_tmo_dbg("reg_light_th = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "hist_th")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;

		tmo_reg.reg_hist_th = (int)val;
		pr_tmo_dbg("reg_hist_th = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "highlight_th1")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;

		tmo_reg.reg_highlight_th1 = (int)val;
		pr_tmo_dbg("reg_highlight_th1 = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "highlight_th2")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;

		tmo_reg.reg_highlight_th2 = (int)val;
		pr_tmo_dbg("reg_highlight_th2 = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "display_e")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;

		tmo_reg.reg_display_e = (int)val;
		pr_tmo_dbg("reg_display_e = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "middle_a")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;

		tmo_reg.reg_middle_a = (int)val;
		pr_tmo_dbg("reg_middle_a = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "middle_a_adj")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;

		tmo_reg.reg_middle_a_adj = (int)val;
		pr_tmo_dbg("reg_middle_a_adj = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "middle_b")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;

		tmo_reg.reg_middle_b = (int)val;
		pr_tmo_dbg("reg_middle_b = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "middle_s")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;

		tmo_reg.reg_middle_s = (int)val;
		pr_tmo_dbg("reg_middle_s = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "max_th1")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;

		tmo_reg.reg_max_th1 = (int)val;
		pr_tmo_dbg("reg_max_th1 = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "max_th2")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;

		tmo_reg.reg_max_th2 = (int)val;
		pr_tmo_dbg("reg_max_th2 = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "middle_th")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;

		tmo_reg.reg_middle_th = (int)val;
		pr_tmo_dbg("reg_middle_th = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "thold1")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;

		tmo_reg.reg_thold1 = (int)val;
		pr_tmo_dbg("reg_thold1 = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "thold2")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;

		tmo_reg.reg_thold2 = (int)val;
		pr_tmo_dbg("reg_thold2 = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "thold3")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;

		tmo_reg.reg_thold3 = (int)val;
		pr_tmo_dbg("reg_thold3 = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "thold4")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;

		tmo_reg.reg_thold4 = (int)val;
		pr_tmo_dbg("reg_thold4 = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "pnum_th")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;

		tmo_reg.reg_pnum_th = (int)val;
		pr_tmo_dbg("reg_pnum_th = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "hl0")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;

		tmo_reg.reg_hl0 = (int)val;
		pr_tmo_dbg("reg_hl0 = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "hl1")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;

		tmo_reg.reg_hl1 = (int)val;
		pr_tmo_dbg("reg_hl1 = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "hl2")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;

		tmo_reg.reg_hl2 = (int)val;
		pr_tmo_dbg("reg_hl2 = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "hl3")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;

		tmo_reg.reg_hl3 = (int)val;
		pr_tmo_dbg("reg_hl3 = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "display_adj")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;

		tmo_reg.reg_display_adj = (int)val;
		pr_tmo_dbg("reg_display_adj = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "low_adj")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;

		tmo_reg.reg_low_adj = (int)val;
		pr_tmo_dbg("reg_low_adj = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "high_en")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;

		tmo_reg.reg_high_en = (int)val;
		pr_tmo_dbg("reg_high_en = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "high_adj1")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;

		tmo_reg.reg_high_adj1 = (int)val;
		pr_tmo_dbg("reg_high_adj1 = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "high_adj2")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;

		tmo_reg.reg_high_adj2 = (int)val;
		pr_tmo_dbg("reg_high_adj2 = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "high_maxdiff")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;

		tmo_reg.reg_high_maxdiff = (int)val;
		pr_tmo_dbg("reg_high_maxdiff = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "high_mindiff")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;

		tmo_reg.reg_high_mindiff = (int)val;
		pr_tmo_dbg("reg_high_mindiff = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "avg_th")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;

		tmo_reg.reg_avg_th = (int)val;
		pr_tmo_dbg("reg_avg_th = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "avg_adj")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;

		tmo_reg.reg_avg_adj = (int)val;
		pr_tmo_dbg("reg_avg_adj = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "alpha")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;

		tmo_reg.alpha = (int)val;
		pr_tmo_dbg("alpha = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "read_param")) {
		hdr10_tmo_params_show();
	}

	return 0;
error:
	return -1;
}

