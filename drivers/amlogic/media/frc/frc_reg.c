// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/errno.h>
#include <linux/uaccess.h>
#include <linux/ctype.h>
#include <linux/vmalloc.h>
#include <linux/io.h>
#include <linux/amlogic/media/registers/register_map.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>

#include "frc_drv.h"
#include "frc_rdma.h"

void __iomem *frc_base;

/*****************************************************************************/
u32 regdata_inpholdctl_0002;                     // FRC_INP_HOLD_CTRL    0x0002
EXPORT_SYMBOL(regdata_inpholdctl_0002);
u32 regdata_outholdctl_0003;                     // FRC_OUT_HOLD_CTRL    0x0003
EXPORT_SYMBOL(regdata_outholdctl_0003);

u32 regdata_top_ctl_0007;                         // FRC_REG_TOP_CTRL7  0x0007
EXPORT_SYMBOL(regdata_top_ctl_0007);
u32 regdata_top_ctl_0009;                         // FRC_REG_TOP_CTRL9
EXPORT_SYMBOL(regdata_top_ctl_0009);
u32 regdata_top_ctl_0011;                        // FRC_REG_TOP_CTRL17
EXPORT_SYMBOL(regdata_top_ctl_0011);
u32 regdata_pat_pointer_0102;              // FRC_REG_PAT_POINTER  0x0102
EXPORT_SYMBOL(regdata_pat_pointer_0102);
u32 regdata_loadorgframe[16];                  // FRC_REG_LOAD_ORG_FRAME_0 0x103
EXPORT_SYMBOL(regdata_loadorgframe);

u32 regdata_phs_tab_0116;               // FRC_REG_PHS_TABLE    0x0116
EXPORT_SYMBOL(regdata_phs_tab_0116);

u32 regdata_blksizexy_012b;                     // FRC_REG_BLK_SIZE_XY_0x012b
EXPORT_SYMBOL(regdata_blksizexy_012b);
u32 regdata_blkscale_012c;                   // FRC_REG_BLK_SCALE  0x012c
EXPORT_SYMBOL(regdata_blkscale_012c);
u32 regdata_hme_scale_012d;                      // FRC_REG_ME_HME_SCALE_0x012d
EXPORT_SYMBOL(regdata_hme_scale_012d);

u32 regdata_logodbg_0142;                  // FRC_LOGO_DEBUG    0x0142
EXPORT_SYMBOL(regdata_logodbg_0142);

u32 regdata_inpmoden_04f9;               // FRC_REG_INP_MODULE_EN  0x04f9
EXPORT_SYMBOL(regdata_inpmoden_04f9);

u32 regdata_iplogo_en_0503;        // FRC_IPLOGO_EN    0x0503
EXPORT_SYMBOL(regdata_iplogo_en_0503);

u32 regdata_bbd_t2b_0604;        // FRC_BBD_DETECT_REGION_TOP2BOT  0x0604
EXPORT_SYMBOL(regdata_bbd_t2b_0604);
u32 regdata_bbd_l2r_0605;        // FRC_BBD_DETECT_REGION_LFT2RIT  0x0605
EXPORT_SYMBOL(regdata_bbd_l2r_0605);

u32 regdata_me_en_1100;           // FRC_ME_EN   0x1100
EXPORT_SYMBOL(regdata_me_en_1100);
u32 regdata_me_bbpixed_1108;           // FRC_ME_BB_PIX_ED  0x1108
EXPORT_SYMBOL(regdata_me_bbpixed_1108);
u32 regdata_me_bbblked_110a;           // FRC_ME_BB_BLK_ED  0x110a
EXPORT_SYMBOL(regdata_me_bbblked_110a);
u32 regdata_me_stat12rhst_110b;        // FRC_ME_STAT_12R_HST  0x110b
EXPORT_SYMBOL(regdata_me_stat12rhst_110b);
u32 regdata_me_stat12rh_110c;           // FRC_ME_STAT_12R_H01  0x110c
EXPORT_SYMBOL(regdata_me_stat12rh_110c);
u32 regdata_me_stat12rh_110d;           // FRC_ME_STAT_12R_H23  0x110d
EXPORT_SYMBOL(regdata_me_stat12rh_110d);
u32 regdata_me_stat12rv_110e;          // FRC_ME_STAT_12R_V0   0x110e
EXPORT_SYMBOL(regdata_me_stat12rv_110e);
u32 regdata_me_stat12rv_110f;           // FRC_ME_STAT_12R_V1   0x110f
EXPORT_SYMBOL(regdata_me_stat12rv_110f);

u32 regdata_vpbb1_1e03;            // FRC_VP_BB_1      0x1e03
EXPORT_SYMBOL(regdata_vpbb1_1e03);
u32 regdata_vpbb2_1e04;           // FRC_VP_BB_2      0x1e04
EXPORT_SYMBOL(regdata_vpbb2_1e04);
u32 regdata_vpmebb1_1e05;           // FRC_VP_ME_BB_1   0x1e05
EXPORT_SYMBOL(regdata_vpmebb1_1e05);
u32 regdata_vpmebb2_1e06;           // FRC_VP_ME_BB_2   0x1e06
EXPORT_SYMBOL(regdata_vpmebb2_1e06);

u32 regdata_vp_win1_1e58;          // FRC_VP_REGION_WINDOW_1 0x1e58
EXPORT_SYMBOL(regdata_vp_win1_1e58);
u32 regdata_vp_win2_1e59;          // FRC_VP_REGION_WINDOW_2 0x1e59
EXPORT_SYMBOL(regdata_vp_win2_1e59);
u32 regdata_vp_win3_1e5a;          // FRC_VP_REGION_WINDOW_3 0x1e5a
EXPORT_SYMBOL(regdata_vp_win3_1e5a);
u32 regdata_vp_win4_1e5b;          // FRC_VP_REGION_WINDOW_4 0x1e5b
EXPORT_SYMBOL(regdata_vp_win4_1e5b);

u32 regdata_mcset1_3000;          // FRC_MC_SETTING1   0x3000
EXPORT_SYMBOL(regdata_mcset1_3000);
u32 regdata_mcset2_3001;          // FRC_MC_SETTING2   0x3001
EXPORT_SYMBOL(regdata_mcset2_3001);

u32 regdata_mcdemo_win_3200;          // FRC_MC_DEMO_WINDOW  0x3200
EXPORT_SYMBOL(regdata_mcdemo_win_3200);

u32 regdata_topctl_3f01;               // FRC_TOP_CTRL    0x3f01
EXPORT_SYMBOL(regdata_topctl_3f01);

///////////////////////////////////////////////////////////////////////////////

u32 regdata_film_phs1_0117;    // FRC_REG_FILM_PHS_1     0x0117
EXPORT_SYMBOL(regdata_film_phs1_0117);

u32 regdata_fwd_phs_0146;                  // FRC_REG_FWD_PHS     0x0146
EXPORT_SYMBOL(regdata_fwd_phs_0146);
u32 regdata_fwd_fid_0147;                  // FRC_REG_FWD_FID     0x0147
EXPORT_SYMBOL(regdata_fwd_fid_0147);
u32 regdata_fwd_fid_posi_0148;             // FRC_REG_FWD_FID_POSI   0x0148
EXPORT_SYMBOL(regdata_fwd_fid_posi_0148);
u32 regdata_load_frame_flag0_0149;         // FRC_REG_LOAD_FRAME_FLAG_0  0x0149
EXPORT_SYMBOL(regdata_load_frame_flag0_0149);
u32 regdata_load_frame_flag1_014a;         //  FRC_REG_LOAD_FRAME_FLAG_1  0x014a
EXPORT_SYMBOL(regdata_load_frame_flag1_014a);
u32 regdata_fwd_table_cnt_phaofs_016c;     //  FRC_REG_FWD_TABLE_CNT_PHAOFS 0x016c
EXPORT_SYMBOL(regdata_fwd_table_cnt_phaofs_016c);
u32 regdata_fwd_sign_ro_016e;              //  FRC_REG_FWD_SIGN_RO  x016e
EXPORT_SYMBOL(regdata_fwd_sign_ro_016e);
u32 regdata_fwd_phs_ro_016f;               //  FRC_REG_FWD_PHS_RO   0x016f
EXPORT_SYMBOL(regdata_fwd_phs_ro_016f);
u32 regdata_fwd_phs_adj_016b;              // FRC_REG_FWD_PHS_ADJ       0x016b
EXPORT_SYMBOL(regdata_fwd_phs_adj_016b);

u32 regdata_fd_enable_0700;     // FRC_FD_ENABLE     0x0700
EXPORT_SYMBOL(regdata_fd_enable_0700);

u32 regdata_me_stat_glb_apl_156c;       // FRC_ME_STAT_GLB_APL    0x156c
EXPORT_SYMBOL(regdata_me_stat_glb_apl_156c);

/*****************************************************************************/
//#define FRC_DISABLE_REG_RD_WR
//#if 0
//struct frc_fw_regs_s {
//	u32 addr;
//	u32 value;
//};

//struct frc_fw_regs_s fw_wr_reg[500] = {
//	{0x0000, 0x00000000},
//};
//int fw_idx;
//#endif

void frc_config_reg_value(u32 need_val, u32 mask, u32 *reg_val)
{
	need_val &= mask;
	*reg_val &= ~mask;
	*reg_val |= need_val;
}
EXPORT_SYMBOL(frc_config_reg_value);

void WRITE_FRC_REG_BY_CPU(unsigned int reg, unsigned int val)
{
#ifndef FRC_DISABLE_REG_RD_WR
	if (get_frc_devp()->power_on_flag == 0)
		return;
	writel(val, (frc_base + (reg << 2)));
#endif
}
EXPORT_SYMBOL(WRITE_FRC_REG_BY_CPU);

void WRITE_FRC_REG(unsigned int reg, unsigned int val)
{
#ifndef FRC_DISABLE_REG_RD_WR
	if (get_frc_devp()->power_on_flag == 0)
		return;
	// if (fw_idx >= 500) {
	// memset(&fw_wr_reg[0], 0x00, sizeof(struct frc_fw_regs_s)*200);
	//     fw_idx = 0;
	// }
	// fw_wr_reg[fw_idx].addr = reg;
	// fw_wr_reg[fw_idx].value = val;
	// fw_idx++;
	// writel(val, (frc_base + (reg << 2)));
	FRC_RDMA_VSYNC_WR_REG(reg, val);
#endif
}
EXPORT_SYMBOL(WRITE_FRC_REG);

void WRITE_FRC_BITS(unsigned int reg, unsigned int value,
					   unsigned int start, unsigned int len)
{
#ifndef FRC_DISABLE_REG_RD_WR
	unsigned int tmp, orig;
	unsigned int mask = (((1L << len) - 1) << start);
	int r = (reg << 2);
	if (get_frc_devp()->power_on_flag == 0)
		return;
	orig =  readl((frc_base + r));
	tmp = orig  & ~mask;
	tmp |= (value << start) & mask;
	writel(tmp, (frc_base + r));
	// FRC_RDMA_VSYNC_WR_BITS(reg, value, start, len);
#endif
}
EXPORT_SYMBOL(WRITE_FRC_BITS);

void UPDATE_FRC_REG_BITS(unsigned int reg,
	unsigned int value,
	unsigned int mask)
{
#ifndef FRC_DISABLE_REG_RD_WR
	unsigned int val;

	if (get_frc_devp()->power_on_flag == 0)
		return;
	value &= mask;
	val = readl(frc_base + (reg << 2));
	val &= ~mask;
	val |= value;
	writel(val, (frc_base + (reg << 2)));
#endif
#ifndef FRC_DISABLE_REG_RD_WR
	// FRC_RDMA_VSYNC_REG_UPDATE(reg, value, mask);
#endif
}
EXPORT_SYMBOL(UPDATE_FRC_REG_BITS);

void UPDATE_FRC_REG_BITS_1(unsigned int reg,
	unsigned int value,
	unsigned int mask)
{
#ifndef FRC_DISABLE_REG_RD_WR
	unsigned int val;

	if (get_frc_devp()->power_on_flag == 0)
		return;
	value &= mask;
	val = readl(frc_base + (reg << 2));
	val &= ~mask;
	val |= value;
	writel(val, (frc_base + (reg << 2)));
#endif
}
EXPORT_SYMBOL(UPDATE_FRC_REG_BITS_1);


int READ_FRC_REG(unsigned int reg)
{
#ifndef FRC_DISABLE_REG_RD_WR
	if (get_frc_devp()->power_on_flag == 0)
		return 0;
	return readl(frc_base + (reg << 2));
#else
	return 0;
#endif
}
EXPORT_SYMBOL(READ_FRC_REG);

u32 READ_FRC_BITS(u32 reg, const u32 start, const u32 len)
{
	u32 val = 0;

#ifndef FRC_DISABLE_REG_RD_WR
	if (get_frc_devp()->power_on_flag == 0)
		return 0;
	val = ((READ_FRC_REG(reg) >> (start)) & ((1L << (len)) - 1));
#endif
	return val;
}
EXPORT_SYMBOL(READ_FRC_BITS);

u32 floor_rs(u32 ix, u32 rs)
{
	u32 rst = 0;

	rst = (ix) >> rs;

	return rst;
}
EXPORT_SYMBOL(floor_rs);

u32 ceil_rx(u32 ix, u32 rs)
{
	u32 rst = 0;
	u32 tmp = 0;

	tmp = 1 << rs;
	rst = (ix + tmp - 1) >> rs;

	return rst;
}
EXPORT_SYMBOL(ceil_rx);

s32 negative_convert(s32 data, u32 fbits)
{
	s32 rst = 0;
	s64 sign_base = (s64)1 << (fbits - 1);

	if ((data & sign_base) == sign_base)
		rst = -((sign_base << 1) - data);
	else
		rst = data;

	return rst;
}
EXPORT_SYMBOL(negative_convert);

//void check_fw_table(u8 flag)
//{
//#if 0
//	int tmp_cnt = 0;

//	if (flag == 9) {
//		memset(&fw_wr_reg[0], 0x00, sizeof(struct frc_fw_regs_s) * 500);
//		fw_idx = 0;
//	} else if (flag == 1) {
//		pr_frc(9, "record write reg list[%d]\n", fw_idx);
//		for (tmp_cnt = 0; tmp_cnt < 500;  tmp_cnt++)
//			pr_frc(9, "No%3d:0x%4x = 0x%8x\n",
//				tmp_cnt, fw_wr_reg[tmp_cnt].addr, fw_wr_reg[tmp_cnt].value);
//	} else if (flag == 2) {
//		for (tmp_cnt = 0; tmp_cnt < 16;  tmp_cnt++)
//			pr_frc(9, "No_%2d:lut1= 0x%8x, lut2=0x%8x\n",
//				tmp_cnt, regdata_fwd_fid_lut1[tmp_cnt],
//				regdata_fwd_fid_lut2[tmp_cnt]);
//	}

//#endif
//}
