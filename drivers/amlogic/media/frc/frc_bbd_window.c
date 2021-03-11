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
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_fdt.h>
#include <linux/of_reserved_mem.h>
#include <linux/of_irq.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/errno.h>
#include <linux/uaccess.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/ctype.h>
#include <linux/vmalloc.h>
#include <linux/interrupt.h>

#include "frc_reg.h"
#include "frc_common.h"
#include "frc_drv.h"

void set_bb_me(void)
{
	u32 bb_frc[4], bb_me[4], blk_bb_me[4], dsx, dsy, blkx, blky;

	bb_frc[0] = READ_FRC_REG(FRC_REG_BLACKBAR_XYXY_ME_ST) >> 16 & 0xFFFF;
	bb_frc[1] = READ_FRC_REG(FRC_REG_BLACKBAR_XYXY_ME_ST)     & 0xFFFF;
	bb_frc[2] = READ_FRC_REG(FRC_REG_BLACKBAR_XYXY_ME_ED) >> 16  & 0xFFFF;
	bb_frc[3] = READ_FRC_REG(FRC_REG_BLACKBAR_XYXY_ME_ED)     & 0xFFFF;

	dsx = READ_FRC_REG(FRC_REG_ME_HME_SCALE) >> 6 & 0x3;
	dsy = READ_FRC_REG(FRC_REG_ME_HME_SCALE) >> 4 & 0x3;
	blkx = READ_FRC_REG(FRC_REG_BLK_SIZE_XY) >> 19 & 0x7;
	blky = READ_FRC_REG(FRC_REG_BLK_SIZE_XY) >> 16 & 0x7;

	bb_me[0] = ceil_rx(bb_frc[0], dsx);
	bb_me[1] = ceil_rx(bb_frc[1], dsy);
	bb_me[2] = floor_rs(bb_frc[2], dsx);
	bb_me[3] = floor_rs(bb_frc[3], dsy);
	blk_bb_me[0] = floor_rs(bb_me[0], blkx);
	blk_bb_me[1] = floor_rs(bb_me[1], blky);
	blk_bb_me[2] = floor_rs(bb_me[2], blkx);
	blk_bb_me[3] = floor_rs(bb_me[3], blky);

	UPDATE_FRC_REG_BITS(FRC_ME_BB_PIX_ST, bb_me[0] << 16, 0xFFF0000);
	UPDATE_FRC_REG_BITS(FRC_ME_BB_PIX_ST, bb_me[1], 0xFFF);
	UPDATE_FRC_REG_BITS(FRC_ME_BB_PIX_ED, bb_me[2] << 16, 0xFFF0000);
	UPDATE_FRC_REG_BITS(FRC_ME_BB_PIX_ED, bb_me[3], 0xFFF);
	UPDATE_FRC_REG_BITS(FRC_ME_BB_BLK_ST, blk_bb_me[0] << 16, 0x3FF0000);
	UPDATE_FRC_REG_BITS(FRC_ME_BB_BLK_ST, blk_bb_me[1], 0x3FF);
	UPDATE_FRC_REG_BITS(FRC_ME_BB_BLK_ED, blk_bb_me[2] << 16, 0x3FF0000);
	UPDATE_FRC_REG_BITS(FRC_ME_BB_BLK_ED, blk_bb_me[3], 0x3FF);
}

void set_bb_hme(void)
{
	u32 bb_me[4], bb_hme[4], blk_bb_hme[4], dsx, dsy, blkx, blky;

	bb_me[0] = READ_FRC_REG(FRC_ME_BB_PIX_ST) >> 16 & 0xFFF;
	bb_me[1] = READ_FRC_REG(FRC_ME_BB_PIX_ST)     & 0xFFF;
	bb_me[2] = READ_FRC_REG(FRC_ME_BB_PIX_ED) >> 16 & 0xFFF;
	bb_me[3] = READ_FRC_REG(FRC_ME_BB_PIX_ED)     & 0xFFF;

	dsx = READ_FRC_REG(FRC_REG_ME_HME_SCALE) >> 2 & 0x3;
	dsy = READ_FRC_REG(FRC_REG_ME_HME_SCALE)    & 0x3;
	blkx = READ_FRC_REG(FRC_REG_BLK_SIZE_XY) >> 7  & 0x7;
	blky = READ_FRC_REG(FRC_REG_BLK_SIZE_XY) >> 4  & 0x7;

	bb_hme[0] = ceil_rx(bb_me[0], dsx);
	bb_hme[1] = ceil_rx(bb_me[1], dsy);
	bb_hme[2] = floor_rs(bb_me[2], dsx);
	bb_hme[3] = floor_rs(bb_me[3], dsy);
	blk_bb_hme[0] = floor_rs(bb_hme[0], blkx);
	blk_bb_hme[1] = floor_rs(bb_hme[1], blky);
	blk_bb_hme[2] = floor_rs(bb_hme[2], blkx);
	blk_bb_hme[3] = floor_rs(bb_hme[3], blky);

	UPDATE_FRC_REG_BITS(FRC_ME_BB_PIX_ST + HME_TOP_OFFSET, bb_hme[0] << 16, 0xFFF0000);
	UPDATE_FRC_REG_BITS(FRC_ME_BB_PIX_ST + HME_TOP_OFFSET, bb_hme[1], 0xFFF);
	UPDATE_FRC_REG_BITS(FRC_ME_BB_PIX_ED + HME_TOP_OFFSET, bb_hme[2] << 16, 0xFFF0000);
	UPDATE_FRC_REG_BITS(FRC_ME_BB_PIX_ED + HME_TOP_OFFSET, bb_hme[3], 0xFFF);
	UPDATE_FRC_REG_BITS(FRC_ME_BB_BLK_ST + HME_TOP_OFFSET, blk_bb_hme[0] << 16, 0x3FF0000);
	UPDATE_FRC_REG_BITS(FRC_ME_BB_BLK_ST + HME_TOP_OFFSET, blk_bb_hme[1], 0x3FF);
	UPDATE_FRC_REG_BITS(FRC_ME_BB_BLK_ED + HME_TOP_OFFSET, blk_bb_hme[2] << 16, 0x3FF0000);
	UPDATE_FRC_REG_BITS(FRC_ME_BB_BLK_ED + HME_TOP_OFFSET, blk_bb_hme[3], 0x3FF);
}

void set_bb_mc(void)
{
	u32 bb_frc[4], bb_me[4], blk_bb_me[4], dsx, dsy, blkx, blky;

	bb_frc[0] = READ_FRC_REG(FRC_REG_BLACKBAR_XYXY_ST) >> 16 & 0xFFFF;
	bb_frc[1] = READ_FRC_REG(FRC_REG_BLACKBAR_XYXY_ST)     & 0xFFFF;
	bb_frc[2] = READ_FRC_REG(FRC_REG_BLACKBAR_XYXY_ED) >> 16 & 0xFFFF;
	bb_frc[3] = READ_FRC_REG(FRC_REG_BLACKBAR_XYXY_ED)     & 0xFFFF;

	dsx = READ_FRC_REG(FRC_REG_ME_HME_SCALE) >> 6 & 0x3;
	dsy = READ_FRC_REG(FRC_REG_ME_HME_SCALE) >> 4 & 0x3;
	blkx = READ_FRC_REG(FRC_REG_BLK_SIZE_XY) >> 19 & 0x7;
	blky = READ_FRC_REG(FRC_REG_BLK_SIZE_XY) >> 16 & 0x7;

	bb_me[0] = ceil_rx(bb_frc[0], dsx);
	bb_me[1] = ceil_rx(bb_frc[1], dsy);
	bb_me[2] = floor_rs(bb_frc[2], dsx);
	bb_me[3] = floor_rs(bb_frc[3], dsy);
	blk_bb_me[0] = floor_rs(bb_me[0], blkx);
	blk_bb_me[1] = floor_rs(bb_me[1], blky);
	blk_bb_me[2] = floor_rs(bb_me[2], blkx);
	blk_bb_me[3] = floor_rs(bb_me[3], blky);

	UPDATE_FRC_REG_BITS(FRC_MC_BB_HANDLE_ORG_ME_BB_XYXY_LEFT_TOP, bb_me[0] << 16, 0xFFFF0000);
	UPDATE_FRC_REG_BITS(FRC_MC_BB_HANDLE_ORG_ME_BB_XYXY_LEFT_TOP, bb_me[1], 0xFFFF);
	UPDATE_FRC_REG_BITS(FRC_MC_BB_HANDLE_ORG_ME_BB_XYXY_RIGHT_BOT, bb_me[2] << 16, 0xFFFF0000);
	UPDATE_FRC_REG_BITS(FRC_MC_BB_HANDLE_ORG_ME_BB_XYXY_RIGHT_BOT, bb_me[3], 0xFFFF);
	UPDATE_FRC_REG_BITS(FRC_MC_BB_HANDLE_ORG_ME_BLK_BB_XYXY_LFT_AND_TOP, blk_bb_me[0] << 16,
			    0xFFFF0000);
	UPDATE_FRC_REG_BITS(FRC_MC_BB_HANDLE_ORG_ME_BLK_BB_XYXY_LFT_AND_TOP, blk_bb_me[1], 0xFFFF);
	UPDATE_FRC_REG_BITS(FRC_MC_BB_HANDLE_ORG_ME_BLK_BB_XYXY_RIT_AND_BOT, blk_bb_me[2] << 16,
			    0xFFFF0000);
	UPDATE_FRC_REG_BITS(FRC_MC_BB_HANDLE_ORG_ME_BLK_BB_XYXY_RIT_AND_BOT, blk_bb_me[3], 0xFFF);
}

void set_bb_vp(void)
{
	u32 bb_frc[4], bb_vp[4], blk_bb_vp[4], dsx, dsy, blkx, blky;

	bb_frc[0] = READ_FRC_REG(FRC_REG_BLACKBAR_XYXY_ME_ST) >> 16 & 0xFFFF;
	bb_frc[1] = READ_FRC_REG(FRC_REG_BLACKBAR_XYXY_ME_ST)     & 0xFFFF;
	bb_frc[2] = READ_FRC_REG(FRC_REG_BLACKBAR_XYXY_ME_ED) >> 16 & 0xFFFF;
	bb_frc[3] = READ_FRC_REG(FRC_REG_BLACKBAR_XYXY_ME_ED)     & 0xFFFF;

	dsx = READ_FRC_REG(FRC_REG_ME_HME_SCALE) >> 6 & 0x3;
	dsy = READ_FRC_REG(FRC_REG_ME_HME_SCALE) >> 4 & 0x3;
	blkx = READ_FRC_REG(FRC_REG_BLK_SIZE_XY) >> 19 & 0x7;
	blky = READ_FRC_REG(FRC_REG_BLK_SIZE_XY) >> 16 & 0x7;

	bb_vp[0]  =   floor_rs(bb_frc[0], dsx);
	bb_vp[1]  =   floor_rs(bb_frc[1], dsy);
	bb_vp[2]  =   floor_rs(bb_frc[2], dsx);
	bb_vp[3]  =   floor_rs(bb_frc[3], dsy);

	UPDATE_FRC_REG_BITS(FRC_VP_BB_1, ((bb_vp[0] >> blkx) + 1), 0xFFFF);
	UPDATE_FRC_REG_BITS(FRC_VP_BB_1, ((bb_vp[1] >> blky) + 1) << 16, 0xFFFF0000);
	UPDATE_FRC_REG_BITS(FRC_VP_BB_2, ((bb_vp[2] >> blkx) - 1), 0xFFFF);
	UPDATE_FRC_REG_BITS(FRC_VP_BB_2, ((bb_vp[3] >> blky) - 1) << 16, 0xFFFF0000);

	bb_vp[0] = ceil_rx(bb_frc[0], dsx);
	bb_vp[1] = ceil_rx(bb_frc[1], dsy);

	blk_bb_vp[0] = floor_rs(bb_vp[0], blkx);
	blk_bb_vp[1] = floor_rs(bb_vp[1], blky);
	blk_bb_vp[2] = floor_rs(bb_vp[2], blkx);
	blk_bb_vp[3] = floor_rs(bb_vp[3], blky);

	UPDATE_FRC_REG_BITS(FRC_VP_ME_BB_1, blk_bb_vp[0], 0xFFFF);
	UPDATE_FRC_REG_BITS(FRC_VP_ME_BB_1, blk_bb_vp[1] << 16, 0xFFFF0000);
	UPDATE_FRC_REG_BITS(FRC_VP_ME_BB_2, blk_bb_vp[2], 0xFFFF);
	UPDATE_FRC_REG_BITS(FRC_VP_ME_BB_2, blk_bb_vp[3] << 16, 0xFFFF0000);
}

void set_me_region_boundary(void)
{
	u16 blk_bb_me[4], xstep, ystep;

	blk_bb_me[0] = READ_FRC_REG(FRC_ME_BB_BLK_ST) >> 16 & 0x3FF;
	blk_bb_me[1] = READ_FRC_REG(FRC_ME_BB_BLK_ST)     & 0x3FF;
	blk_bb_me[2] = READ_FRC_REG(FRC_ME_BB_BLK_ED) >> 16 & 0x3FF;
	blk_bb_me[3] = READ_FRC_REG(FRC_ME_BB_BLK_ED)     & 0x3FF;

	xstep = (blk_bb_me[2] - blk_bb_me[0]) / 4;
	ystep = (blk_bb_me[3] - blk_bb_me[1]) / 3;

	UPDATE_FRC_REG_BITS(FRC_ME_STAT_12R_HST, blk_bb_me[0], 0x3FF);
	UPDATE_FRC_REG_BITS(FRC_ME_STAT_12R_H01, (blk_bb_me[0] + xstep) << 16, 0x3FF0000);
	UPDATE_FRC_REG_BITS(FRC_ME_STAT_12R_H01, (blk_bb_me[0] + 2 * xstep), 0x3FF);
	UPDATE_FRC_REG_BITS(FRC_ME_STAT_12R_H23, (blk_bb_me[0] + 3 * xstep) << 16, 0x3FF0000);
	UPDATE_FRC_REG_BITS(FRC_ME_STAT_12R_H23, blk_bb_me[2], 0x3FF);
	UPDATE_FRC_REG_BITS(FRC_ME_STAT_12R_V0, blk_bb_me[1] << 16, 0x3FF0000);
	UPDATE_FRC_REG_BITS(FRC_ME_STAT_12R_V0, (blk_bb_me[1] + ystep), 0x3FF);
	UPDATE_FRC_REG_BITS(FRC_ME_STAT_12R_V1, (blk_bb_me[1] + 2 * ystep) << 16, 0x3FF0000);
	UPDATE_FRC_REG_BITS(FRC_ME_STAT_12R_V1, blk_bb_me[3], 0x3FF);
}

void set_hme_region_boundary(void)
{
	u16 blk_bb_me[4], xstep, ystep;

	blk_bb_me[0] = READ_FRC_REG(FRC_ME_BB_BLK_ST + HME_TOP_OFFSET) >> 16 & 0x3FF;
	blk_bb_me[1] = READ_FRC_REG(FRC_ME_BB_BLK_ST + HME_TOP_OFFSET)     & 0x3FF;
	blk_bb_me[2] = READ_FRC_REG(FRC_ME_BB_BLK_ED + HME_TOP_OFFSET) >> 16 & 0x3FF;
	blk_bb_me[3] = READ_FRC_REG(FRC_ME_BB_BLK_ED + HME_TOP_OFFSET)     & 0x3FF;

	xstep = (blk_bb_me[2] - blk_bb_me[0]) / 4;
	ystep = (blk_bb_me[3] - blk_bb_me[1]) / 3;

	UPDATE_FRC_REG_BITS(FRC_ME_STAT_12R_HST + HME_TOP_OFFSET, blk_bb_me[0], 0x3FF);
	UPDATE_FRC_REG_BITS(FRC_ME_STAT_12R_H01 + HME_TOP_OFFSET, (blk_bb_me[0] + xstep) << 16,
			    0x3FF0000);
	UPDATE_FRC_REG_BITS(FRC_ME_STAT_12R_H01 + HME_TOP_OFFSET, (blk_bb_me[0] + 2 * xstep),
			    0x3FF);
	UPDATE_FRC_REG_BITS(FRC_ME_STAT_12R_H23 + HME_TOP_OFFSET, (blk_bb_me[0] + 3 * xstep) << 16,
			    0x3FF0000);
	UPDATE_FRC_REG_BITS(FRC_ME_STAT_12R_H23 + HME_TOP_OFFSET, blk_bb_me[2], 0x3FF);
	UPDATE_FRC_REG_BITS(FRC_ME_STAT_12R_V0 + HME_TOP_OFFSET, blk_bb_me[1] << 16, 0x3FF0000);
	UPDATE_FRC_REG_BITS(FRC_ME_STAT_12R_V0 + HME_TOP_OFFSET, (blk_bb_me[1] + ystep), 0x3FF);
	UPDATE_FRC_REG_BITS(FRC_ME_STAT_12R_V1 + HME_TOP_OFFSET, (blk_bb_me[1] + 2 * ystep) << 16,
			    0x3FF0000);
	UPDATE_FRC_REG_BITS(FRC_ME_STAT_12R_V1 + HME_TOP_OFFSET, blk_bb_me[3], 0x3FF);
}

void set_vp_region_boundary(void)
{
	u16 blk_bb_vp[4], xstep, ystep;

	blk_bb_vp[0] = READ_FRC_REG(FRC_VP_BB_1) >> 16 & 0xFFFF;
	blk_bb_vp[1] = READ_FRC_REG(FRC_VP_BB_1)     & 0xFFFF;
	blk_bb_vp[2] = READ_FRC_REG(FRC_VP_BB_2) >> 16 & 0xFFFF;
	blk_bb_vp[3] = READ_FRC_REG(FRC_VP_BB_2)     & 0xFFFF;

	xstep = (blk_bb_vp[2] - blk_bb_vp[0]) / 4;
	ystep = (blk_bb_vp[3] - blk_bb_vp[1]) / 3;

	UPDATE_FRC_REG_BITS(FRC_VP_REGION_WINDOW_1, blk_bb_vp[0], 0xFF);
	UPDATE_FRC_REG_BITS(FRC_VP_REGION_WINDOW_1, (blk_bb_vp[0] + xstep) << 8, 0xFFF00);
	UPDATE_FRC_REG_BITS(FRC_VP_REGION_WINDOW_1, (blk_bb_vp[0] + 2 * xstep) << 20,
			    0xFFF00000);
	UPDATE_FRC_REG_BITS(FRC_VP_REGION_WINDOW_2, (blk_bb_vp[0] + 3 * xstep), 0xFFF);
	UPDATE_FRC_REG_BITS(FRC_VP_REGION_WINDOW_2, blk_bb_vp[2] << 12, 0xFFF000);
	UPDATE_FRC_REG_BITS(FRC_VP_REGION_WINDOW_2, blk_bb_vp[1] << 24, 0xFF000000);
	UPDATE_FRC_REG_BITS(FRC_VP_REGION_WINDOW_3, (blk_bb_vp[1] + ystep), 0xFF);
	UPDATE_FRC_REG_BITS(FRC_VP_REGION_WINDOW_3, (blk_bb_vp[1] + 2 * ystep) << 8, 0xFFF00);
	UPDATE_FRC_REG_BITS(FRC_VP_REGION_WINDOW_3, blk_bb_vp[3] << 20, 0xFFF00000);
}

void set_bb_iplogo(void)
{
	u8 dsx, dsy;
	u8 me_logo_dsx, me_logo_dsy;
	u16 logo_bb_xyxy[4];            // reg_logo_bb_xyxy[0/1/2/3] : lft/top/rit/bot
	u16 iplogo_bb_xyxy[4];          // reg_iplogo_bb_xyxy[0/1/2/3]

	dsx             = READ_FRC_REG(FRC_REG_ME_HME_SCALE) >> 6  & 0x3;    //reg_me_dsx_scale
	dsy             = READ_FRC_REG(FRC_REG_ME_HME_SCALE) >> 4  & 0x3;    //reg_me_dsy_scale
	me_logo_dsx     = READ_FRC_REG(FRC_REG_BLK_SIZE_XY) >> 25 & 0x1;    //reg_me_logo_dsx_ratio
	me_logo_dsy     = READ_FRC_REG(FRC_REG_BLK_SIZE_XY) >> 24 & 0x1;    //reg_me_logo_dsy_ratio
	logo_bb_xyxy[0] = READ_FRC_REG(FRC_LOGO_BB_LFT_TOP) >> 16 & 0xFFFF; //reg_logo_bb_xyxy_0
	logo_bb_xyxy[1] = READ_FRC_REG(FRC_LOGO_BB_LFT_TOP) >> 0  & 0xFFFF; //reg_logo_bb_xyxy_1
	logo_bb_xyxy[2] = READ_FRC_REG(FRC_LOGO_BB_RIT_BOT) >> 16 & 0xFFFF; //reg_logo_bb_xyxy_2
	logo_bb_xyxy[3] = READ_FRC_REG(FRC_LOGO_BB_RIT_BOT) >> 0  & 0xFFFF; //reg_logo_bb_xyxy_3

	iplogo_bb_xyxy[1] = (logo_bb_xyxy[1] + (1 << dsy) - 1) >> dsy;
	iplogo_bb_xyxy[3] = (logo_bb_xyxy[3] + (1 << dsy) - 1) >> dsy;
	iplogo_bb_xyxy[0] = (logo_bb_xyxy[0] + (1 << dsx) - 1) >> dsx;
	iplogo_bb_xyxy[2] = (logo_bb_xyxy[2] + (1 << dsx) - 1) >> dsx;

	iplogo_bb_xyxy[1] = (iplogo_bb_xyxy[1] + (1 << me_logo_dsy) - 1) >> me_logo_dsy;
	iplogo_bb_xyxy[3] = (iplogo_bb_xyxy[3] + (1 << me_logo_dsy) - 1) >> me_logo_dsy;
	iplogo_bb_xyxy[0] = (iplogo_bb_xyxy[0] + (1 << me_logo_dsx) - 1) >> me_logo_dsx;
	iplogo_bb_xyxy[2] = (iplogo_bb_xyxy[2] + (1 << me_logo_dsx) - 1) >> me_logo_dsx;

	UPDATE_FRC_REG_BITS(FRC_IPLOGO_BB_PIX_ST, iplogo_bb_xyxy[1] << 0,  0xFFF);
	UPDATE_FRC_REG_BITS(FRC_IPLOGO_BB_PIX_ED, iplogo_bb_xyxy[3] << 0,  0xFFF);
	UPDATE_FRC_REG_BITS(FRC_IPLOGO_BB_PIX_ST, iplogo_bb_xyxy[0] << 16, 0xFFF0000);
	UPDATE_FRC_REG_BITS(FRC_IPLOGO_BB_PIX_ED, iplogo_bb_xyxy[2] << 16, 0xFFF0000);
}

void set_bb_melogo(void)
{
	u8 dsx, dsy;
	u8 me_blksize_x, me_blksize_y;      // reg_me_blksize_x/y
	u8 melogo_blackbar_top_coring, melogo_blackbar_bot_coring;
	u8 melogo_blackbar_lft_coring, melogo_blackbar_rit_coring;
	u16 logo_bb_xyxy[4];
	u16 iplogo_bb_xyxy[4];              // reg_iplogo_bb_xyxy[0/1/2/3]
	u16 melogo_bb_xyxy[4];              // reg_melogo_bb_blk_xyxy[0/1/2/3]

	dsx             = READ_FRC_REG(FRC_REG_ME_HME_SCALE) >> 6  & 0x3;
	dsy             = READ_FRC_REG(FRC_REG_ME_HME_SCALE) >> 4  & 0x3;
	me_blksize_x    = READ_FRC_REG(FRC_ME_EN) >> 12 & 0x7;
	me_blksize_y    = READ_FRC_REG(FRC_ME_EN) >> 8  & 0x7;
	logo_bb_xyxy[0] = READ_FRC_REG(FRC_LOGO_BB_LFT_TOP) >> 16 & 0xFFFF;
	logo_bb_xyxy[1] = READ_FRC_REG(FRC_LOGO_BB_LFT_TOP) >> 0  & 0xFFFF;
	logo_bb_xyxy[2] = READ_FRC_REG(FRC_LOGO_BB_RIT_BOT) >> 16 & 0xFFFF;
	logo_bb_xyxy[3] = READ_FRC_REG(FRC_LOGO_BB_RIT_BOT) >> 0  & 0xFFFF;
	melogo_blackbar_top_coring = READ_FRC_REG(FRC_MELOGO_BB_CORING) >> 24 & 0xFF;
	melogo_blackbar_bot_coring = READ_FRC_REG(FRC_MELOGO_BB_CORING) >> 16 & 0xFF;
	melogo_blackbar_lft_coring = READ_FRC_REG(FRC_MELOGO_BB_CORING) >> 8  & 0xFF;
	melogo_blackbar_rit_coring = READ_FRC_REG(FRC_MELOGO_BB_CORING) >> 0  & 0xFF;

	iplogo_bb_xyxy[1] = (logo_bb_xyxy[1] + (1 << dsy) - 1) >> dsy;
	iplogo_bb_xyxy[3] = (logo_bb_xyxy[3] + (1 << dsy) - 1) >> dsy;
	iplogo_bb_xyxy[0] = (logo_bb_xyxy[0] + (1 << dsx) - 1) >> dsx;
	iplogo_bb_xyxy[2] = (logo_bb_xyxy[2] + (1 << dsx) - 1) >> dsx;

	melogo_bb_xyxy[1] = (iplogo_bb_xyxy[1] + (1 << me_blksize_y) - 1) >> me_blksize_y;
	melogo_bb_xyxy[3] = (iplogo_bb_xyxy[3] + (1 << me_blksize_y) - 1) >> me_blksize_y;
	melogo_bb_xyxy[0] = (iplogo_bb_xyxy[0] + (1 << me_blksize_x) - 1) >> me_blksize_x;
	melogo_bb_xyxy[2] = (iplogo_bb_xyxy[2] + (1 << me_blksize_x) - 1) >> me_blksize_x;

	melogo_bb_xyxy[1] = melogo_bb_xyxy[1] + melogo_blackbar_top_coring;
	melogo_bb_xyxy[3] = melogo_bb_xyxy[3] - melogo_blackbar_bot_coring;
	melogo_bb_xyxy[0] = melogo_bb_xyxy[0] + melogo_blackbar_lft_coring;
	melogo_bb_xyxy[2] = melogo_bb_xyxy[2] - melogo_blackbar_rit_coring;

	UPDATE_FRC_REG_BITS(FRC_MELOGO_BB_BLK_ST, melogo_bb_xyxy[1] << 0,  0x3FF);
	UPDATE_FRC_REG_BITS(FRC_MELOGO_BB_BLK_ED, melogo_bb_xyxy[3] << 0,  0x3FF);
	UPDATE_FRC_REG_BITS(FRC_MELOGO_BB_BLK_ST, melogo_bb_xyxy[0] << 16, 0x3FF0000);
	UPDATE_FRC_REG_BITS(FRC_MELOGO_BB_BLK_ED, melogo_bb_xyxy[2] << 16, 0x3FF0000);
}

void frc_bbd_window_ctrl(void)
{
	set_bb_hme();
	set_bb_me();
	set_bb_mc();
	set_bb_vp();
	set_hme_region_boundary();
	set_me_region_boundary();
	set_vp_region_boundary();
	set_bb_iplogo();
	set_bb_melogo();
}

