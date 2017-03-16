#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/errno.h>
#include <linux/amlogic/amports/vframe.h>
#include <linux/amlogic/amports/video.h>
#include <linux/amlogic/amvecm/amvecm.h>
#include <linux/amlogic/vout/vout_notify.h>
#include <linux/amlogic/amports/vframe_provider.h>
#include <linux/amlogic/amports/vframe_receiver.h>
#include <linux/amlogic/amports/amstream.h>
#include "arch/vpp_regs.h"
#include "arch/vpp_hdr_regs.h"
#include "arch/vpp_dolbyvision_regs.h"
#include "dolby_vision/dolby_vision.h"

DEFINE_SPINLOCK(dovi_lock);
#include <linux/cma.h>
#include <linux/amlogic/codec_mm/codec_mm.h>
#include <linux/dma-contiguous.h>
#include <linux/amlogic/iomap.h>

static const struct dolby_vision_func_s *p_funcs;

#define DOLBY_VISION_OUTPUT_MODE_IPT			0
#define DOLBY_VISION_OUTPUT_MODE_IPT_TUNNEL		1
#define DOLBY_VISION_OUTPUT_MODE_HDR10			2
#define DOLBY_VISION_OUTPUT_MODE_SDR10			3
#define DOLBY_VISION_OUTPUT_MODE_SDR8			4
#define DOLBY_VISION_OUTPUT_MODE_BYPASS			5
static unsigned int dolby_vision_mode = DOLBY_VISION_OUTPUT_MODE_BYPASS;
module_param(dolby_vision_mode, uint, 0664);
MODULE_PARM_DESC(dolby_vision_mode, "\n dolby_vision_mode\n");

/* if sink support DV, always output DV */
/* else always output SDR/HDR */
#define DOLBY_VISION_FOLLOW_SINK		0
/* output DV only if source is DV and sink support DV */
/* else always output SDR/HDR */
#define DOLBY_VISION_FOLLOW_SOURCE		1
/* always follow dolby_vision_mode */
#define DOLBY_VISION_FORCE_OUTPUT_MODE	2
static unsigned int dolby_vision_policy;
module_param(dolby_vision_policy, uint, 0664);
MODULE_PARM_DESC(dolby_vision_policy, "\n dolby_vision_policy\n");

static bool dolby_vision_enable;
module_param(dolby_vision_enable, bool, 0664);
MODULE_PARM_DESC(dolby_vision_enable, "\n dolby_vision_enable\n");

#define BYPASS_PROCESS 0
#define SDR_PROCESS 1
#define HDR_PROCESS 2
#define DV_PROCESS 3
static uint dolby_vision_status;
module_param(dolby_vision_status, uint, 0664);
MODULE_PARM_DESC(dolby_vision_status, "\n dolby_vision_status\n");

#define FLAG_RESET_EACH_FRAME		0x01
#define FLAG_META_FROM_BL			0x02
#define FLAG_BYPASS_CVM				0x04
#define FLAG_USE_SINK_MIN_MAX		0x08
#define FLAG_CLKGATE_WHEN_LOAD_LUT	0x10
#define FLAG_SINGLE_STEP			0x20
#define FLAG_CERTIFICAION			0x40
#define FLAG_CHANGE_SEQ_HEAD		0x80
#define FLAG_DISABLE_COMPOSER		0x100
#define FLAG_BYPASS_CSC				0x200
#define FLAG_CHECK_ES_PTS			0x400
#define FLAG_TOGGLE_FRAME			0x80000000
static unsigned int dolby_vision_flags =
	FLAG_META_FROM_BL | FLAG_BYPASS_CVM;
module_param(dolby_vision_flags, uint, 0664);
MODULE_PARM_DESC(dolby_vision_flags, "\n dolby_vision_flags\n");

static unsigned int htotal_add = 0x140;
static unsigned int vtotal_add = 0x40;
static unsigned int vsize_add;
static unsigned int vwidth = 0x8;
static unsigned int hwidth = 0x8;
static unsigned int vpotch = 0x10;
static unsigned int hpotch = 0x8;
static unsigned int g_htotal_add = 0x40;
static unsigned int g_vtotal_add = 0x80;
static unsigned int g_vsize_add;
static unsigned int g_vwidth = 0x18;
static unsigned int g_hwidth = 0x10;
static unsigned int g_vpotch = 0x8;
static unsigned int g_hpotch = 0x10;

module_param(vtotal_add, uint, 0664);
MODULE_PARM_DESC(vtotal_add, "\n vtotal_add\n");
module_param(vpotch, uint, 0664);
MODULE_PARM_DESC(vpotch, "\n vpotch\n");

static unsigned int dolby_vision_target_min = 50; /* 0.0001 */
static unsigned int dolby_vision_target_max[3][3] = {
	{ 4000, 4000, 100 }, /* DOVI => DOVI/HDR/SDR */
	{ 1000, 1000, 100 }, /* HDR =>  DOVI/HDR/SDR */
	{ 600, 1000, 100 },  /* SDR =>  DOVI/HDR/SDR */
};

static unsigned int dolby_vision_default_max[3][3] = {
	{ 4000, 4000, 100 }, /* DOVI => DOVI/HDR/SDR */
	{ 1000, 1000, 100 }, /* HDR =>  DOVI/HDR/SDR */
	{ 600, 1000, 100 },  /* SDR =>  DOVI/HDR/SDR */
};

static unsigned int dolby_vision_graphic_min = 50; /* 0.0001 */
static unsigned int dolby_vision_graphic_max = 100; /* 1 */
module_param(dolby_vision_graphic_min, uint, 0664);
MODULE_PARM_DESC(dolby_vision_graphic_min, "\n dolby_vision_graphic_min\n");

static unsigned int debug_dolby;
module_param(debug_dolby, uint, 0664);
MODULE_PARM_DESC(debug_dolby, "\n debug_dolby\n");

static unsigned int debug_dolby_frame = 0xffff;
module_param(debug_dolby_frame, uint, 0664);
MODULE_PARM_DESC(debug_dolby_frame, "\n debug_dolby_frame\n");

#define pr_dolby_dbg(fmt, args...)\
	do {\
		if (debug_dolby)\
			pr_info("DOLBY: " fmt, ## args);\
	} while (0)
#define pr_dolby_error(fmt, args...)\
	pr_info("DOLBY ERROR: " fmt, ## args)
#define dump_enable \
	((debug_dolby_frame >= 0xffff) || (debug_dolby_frame == frame_count))
#define is_graphics_output_off() \
	(!(READ_VPP_REG(VPP_MISC) & (1<<12)))

static bool dolby_vision_on;
static bool dolby_vision_wait_on;
static unsigned int frame_count;
static struct hdr10_param_s hdr10_param;
static struct master_display_info_s hdr10_data;
static struct dovi_setting_s dovi_setting;
static struct dovi_setting_s new_dovi_setting;

static int dolby_core1_set(
	uint32_t dm_count,
	uint32_t comp_count,
	uint32_t lut_count,
	uint32_t *p_core1_dm_regs,
	uint32_t *p_core1_comp_regs,
	uint32_t *p_core1_lut,
	int hsize,
	int vsize,
	int bl_enable,
	int el_enable,
	int el_41_mode,
	int scramble_en,
	bool dovi_src,
	int lut_endian)
{
	uint32_t count;
	uint32_t bypass_flag = 0;
	int composer_enable = el_enable;
	int i;

	if (dolby_vision_flags & FLAG_DISABLE_COMPOSER)
		composer_enable = 0;

	if ((!dolby_vision_on)
	|| (dolby_vision_flags & FLAG_RESET_EACH_FRAME)) {
		VSYNC_WR_MPEG_REG(VIU_SW_RESET, (1 << 10)|(1 << 9));
		VSYNC_WR_MPEG_REG(VIU_SW_RESET, 0);
	}

	VSYNC_WR_MPEG_REG(DOLBY_CORE1_SWAP_CTRL0, 0);
	VSYNC_WR_MPEG_REG(DOLBY_CORE1_SWAP_CTRL1,
		((hsize + 0x40) << 16) | (vsize + 0x20));
	VSYNC_WR_MPEG_REG(DOLBY_CORE1_SWAP_CTRL3, (hwidth << 16) | vwidth);
	VSYNC_WR_MPEG_REG(DOLBY_CORE1_SWAP_CTRL4, (hpotch << 16) | vpotch);
	VSYNC_WR_MPEG_REG(DOLBY_CORE1_SWAP_CTRL2, (hsize << 16) | vsize);
	VSYNC_WR_MPEG_REG(DOLBY_CORE1_SWAP_CTRL5, 0xa);
	VSYNC_WR_MPEG_REG(DOLBY_CORE1_DMA_CTRL, 0x0);
	VSYNC_WR_MPEG_REG(DOLBY_CORE1_REG_START + 4, 4);
	VSYNC_WR_MPEG_REG(DOLBY_CORE1_REG_START + 2, 1);

	/* bypass composer to get 12bit when SDR and HDR source */
	if (!dovi_src)
		bypass_flag |= 1 << 0;
	if (scramble_en) {
		if (dolby_vision_flags & FLAG_BYPASS_CSC)
			bypass_flag |= 1 << 1;
		if (dolby_vision_flags & FLAG_BYPASS_CVM)
			bypass_flag |= 1 << 2;
	}
	VSYNC_WR_MPEG_REG(DOLBY_CORE1_REG_START + 1,
		0x78 | bypass_flag); /* bypass CVM and/or CSC */
	VSYNC_WR_MPEG_REG(DOLBY_CORE1_REG_START + 1,
		0x78 | bypass_flag); /* for delay */
	if ((dolby_vision_flags & FLAG_RESET_EACH_FRAME) && (dm_count == 0))
		count = 24;
	else
		count = dm_count;
	for (i = 0; i < count; i++)
		VSYNC_WR_MPEG_REG(DOLBY_CORE1_REG_START + 6 + i,
			p_core1_dm_regs[i]);

	if ((dolby_vision_flags & FLAG_RESET_EACH_FRAME) && (comp_count == 0))
		count = 173;
	else
		count = comp_count;
	for (i = 0; i < count; i++)
		VSYNC_WR_MPEG_REG(DOLBY_CORE1_REG_START + 6 + 44 + i,
			p_core1_comp_regs[i]);

	/* metadata program done */
	VSYNC_WR_MPEG_REG(DOLBY_CORE1_REG_START + 3, 1);

	if ((dolby_vision_flags & FLAG_RESET_EACH_FRAME) && (lut_count == 0))
		count = 256 * 5;
	else
		count = lut_count;
	if (count) {
		if (dolby_vision_flags & FLAG_CLKGATE_WHEN_LOAD_LUT)
			VSYNC_WR_MPEG_REG_BITS(DOLBY_CORE1_CLKGATE_CTRL,
				2, 2, 2);
		VSYNC_WR_MPEG_REG(DOLBY_CORE1_DMA_CTRL, 0x1401);
		if (lut_endian)
			for (i = 0; i < count; i += 4) {
				VSYNC_WR_MPEG_REG(DOLBY_CORE1_DMA_PORT,
					p_core1_lut[i+3]);
				VSYNC_WR_MPEG_REG(DOLBY_CORE1_DMA_PORT,
					p_core1_lut[i+2]);
				VSYNC_WR_MPEG_REG(DOLBY_CORE1_DMA_PORT,
					p_core1_lut[i+1]);
				VSYNC_WR_MPEG_REG(DOLBY_CORE1_DMA_PORT,
					p_core1_lut[i]);
			}
		else
			for (i = 0; i < count; i++)
				VSYNC_WR_MPEG_REG(DOLBY_CORE1_DMA_PORT,
					p_core1_lut[i]);
		if (dolby_vision_flags & FLAG_CLKGATE_WHEN_LOAD_LUT)
			VSYNC_WR_MPEG_REG_BITS(DOLBY_CORE1_CLKGATE_CTRL,
				0, 2, 2);
	}

	/* enable core1 */
	VSYNC_WR_MPEG_REG(DOLBY_CORE1_SWAP_CTRL0,
		bl_enable << 0 |
		composer_enable << 1 |
		el_41_mode << 2);
	return 0;
}

static int dolby_core2_set(
	uint32_t dm_count,
	uint32_t lut_count,
	uint32_t *p_core2_dm_regs,
	uint32_t *p_core2_lut,
	int hsize,
	int vsize,
	int dolby_enable,
	int lut_endian)
{
	uint32_t count;
	int i;

	VSYNC_WR_MPEG_REG(DOLBY_CORE2A_SWAP_CTRL0, 0);
	VSYNC_WR_MPEG_REG(DOLBY_CORE2A_SWAP_CTRL1,
		((hsize + g_htotal_add) << 16)
		| (vsize + g_vtotal_add + g_vsize_add));
	VSYNC_WR_MPEG_REG(DOLBY_CORE2A_SWAP_CTRL3,
		(g_hwidth << 16) | g_vwidth);
	VSYNC_WR_MPEG_REG(DOLBY_CORE2A_SWAP_CTRL4,
		(g_hpotch << 16) | g_vpotch);
	VSYNC_WR_MPEG_REG(DOLBY_CORE2A_SWAP_CTRL2,
		(hsize << 16) | (vsize + g_vsize_add));
	VSYNC_WR_MPEG_REG(DOLBY_CORE2A_SWAP_CTRL5, 0x0);
	VSYNC_WR_MPEG_REG(DOLBY_CORE2A_DMA_CTRL, 0x0);
	VSYNC_WR_MPEG_REG(DOLBY_CORE2A_REG_START + 2, 1);
	VSYNC_WR_MPEG_REG(DOLBY_CORE2A_REG_START + 1, 2);
	VSYNC_WR_MPEG_REG(DOLBY_CORE2A_REG_START + 1, 2);

	VSYNC_WR_MPEG_REG(DOLBY_CORE2A_CTRL, 0);

	if ((dolby_vision_flags & FLAG_RESET_EACH_FRAME)
	&& (dm_count == 0))
		count = 24;
	else
		count = dm_count;
	for (i = 0; i < count; i++)
		VSYNC_WR_MPEG_REG(DOLBY_CORE2A_REG_START + 6 + i,
			p_core2_dm_regs[i]);
	/* core2 metadata program done */
	VSYNC_WR_MPEG_REG(DOLBY_CORE2A_REG_START + 3, 1);

	if (dolby_vision_flags & FLAG_RESET_EACH_FRAME)
		count = 256 * 5;
	else
		count = lut_count;
	if (count) {
		if (dolby_vision_flags & FLAG_CLKGATE_WHEN_LOAD_LUT)
			VSYNC_WR_MPEG_REG_BITS(DOLBY_CORE2A_CLKGATE_CTRL,
				2, 2, 2);
		VSYNC_WR_MPEG_REG(DOLBY_CORE2A_DMA_CTRL, 0x1401);
		if (lut_endian)
			for (i = 0; i < count; i += 4) {
				VSYNC_WR_MPEG_REG(DOLBY_CORE2A_DMA_PORT,
					p_core2_lut[i+3]);
				VSYNC_WR_MPEG_REG(DOLBY_CORE2A_DMA_PORT,
					p_core2_lut[i+2]);
				VSYNC_WR_MPEG_REG(DOLBY_CORE2A_DMA_PORT,
					p_core2_lut[i+1]);
				VSYNC_WR_MPEG_REG(DOLBY_CORE2A_DMA_PORT,
					p_core2_lut[i]);
			}
		else
			for (i = 0; i < count; i++)
				VSYNC_WR_MPEG_REG(DOLBY_CORE2A_DMA_PORT,
					p_core2_lut[i]);
		/* core1 lookup table program done */
		if (dolby_vision_flags & FLAG_CLKGATE_WHEN_LOAD_LUT)
			VSYNC_WR_MPEG_REG_BITS(
				DOLBY_CORE2A_CLKGATE_CTRL, 0, 2, 2);
	}

	/* enable core2 */
	VSYNC_WR_MPEG_REG(DOLBY_CORE2A_SWAP_CTRL0, dolby_enable << 0);
	return 0;
}

static int dolby_core3_set(
	uint32_t dm_count,
	uint32_t md_count,
	uint32_t *p_core3_dm_regs,
	uint32_t *p_core3_md_regs,
	int hsize,
	int vsize,
	int dolby_enable,
	int scramble_en)
{
	uint32_t count;
	int i;
	int vsize_hold = 0x10;

	/* turn off free run clock */
	VSYNC_WR_MPEG_REG(DOLBY_CORE3_CLKGATE_CTRL,	0);
	VSYNC_WR_MPEG_REG(DOLBY_CORE3_SWAP_CTRL1,
		((hsize + htotal_add) << 16)
		| (vsize + vtotal_add + vsize_add + vsize_hold * 2));
	VSYNC_WR_MPEG_REG(DOLBY_CORE3_SWAP_CTRL2,
		(hsize << 16) | (vsize + vsize_add));
	VSYNC_WR_MPEG_REG(DOLBY_CORE3_SWAP_CTRL3,
		(0x80 << 16) | vsize_hold);
	VSYNC_WR_MPEG_REG(DOLBY_CORE3_SWAP_CTRL4,
		(0x04 << 16) | vsize_hold);
	VSYNC_WR_MPEG_REG(DOLBY_CORE3_SWAP_CTRL5, 0x0000);
	if (dolby_vision_mode !=
		DOLBY_VISION_OUTPUT_MODE_IPT_TUNNEL)
		VSYNC_WR_MPEG_REG(DOLBY_CORE3_SWAP_CTRL6, 0);
	else
		VSYNC_WR_MPEG_REG(DOLBY_CORE3_SWAP_CTRL6,
			0x10000000);  /* swap UV */
	VSYNC_WR_MPEG_REG(DOLBY_CORE3_REG_START + 5, 7);
	VSYNC_WR_MPEG_REG(DOLBY_CORE3_REG_START + 4, 4);
	VSYNC_WR_MPEG_REG(DOLBY_CORE3_REG_START + 4, 2);
	VSYNC_WR_MPEG_REG(DOLBY_CORE3_REG_START + 2, 1);
	/* Control Register, address 0x04 2:0 RW */
	/* Output_operating mode
	   00- IPT 12 bit 444 bypass Dolby Vision output
	   01- IPT 12 bit tunnelled over RGB 8 bit 444, dolby vision output
	   02- HDR10 output, RGB 10 bit 444 PQ
	   03- Deep color SDR, RGB 10 bit 444 Gamma
	   04- SDR, RGB 8 bit 444 Gamma
	*/
	VSYNC_WR_MPEG_REG(DOLBY_CORE3_REG_START + 1, dolby_vision_mode);
	VSYNC_WR_MPEG_REG(DOLBY_CORE3_REG_START + 1, dolby_vision_mode);
	/* for delay */

	if ((dolby_vision_flags & FLAG_RESET_EACH_FRAME)
	&& (dm_count == 0))
		count = 26;
	else
		count = dm_count;
	for (i = 0; i < count; i++)
		VSYNC_WR_MPEG_REG(DOLBY_CORE3_REG_START + 0x6 + i,
			p_core3_dm_regs[i]);
	/* from addr 0x18 */

	count = md_count;
	for (i = 0; i < count; i++)
		VSYNC_WR_MPEG_REG(DOLBY_CORE3_REG_START + 0x24 + i,
			p_core3_md_regs[i]);
	for (; i < 30; i++)
		VSYNC_WR_MPEG_REG(DOLBY_CORE3_REG_START + 0x24 + i, 0);

	/* from addr 0x90 */
	/* core3 metadata program done */
	VSYNC_WR_MPEG_REG(DOLBY_CORE3_REG_START + 3, 1);

	/* enable core3 */
	VSYNC_WR_MPEG_REG(DOLBY_CORE3_SWAP_CTRL0, (dolby_enable << 0));
	return 0;
}

static uint32_t dolby_ctrl_backup;
static uint32_t viu_misc_ctrl_backup;
static uint32_t vpu_hdmi_fmt_backup;
static uint32_t viu_eotf_ctrl_backup;
static uint32_t xvycc_lut_ctrl_backup;
static uint32_t inv_lut_ctrl_backup;
static uint32_t vpp_matrix_backup;
static uint32_t vpp_vadj_backup;
static uint32_t vpp_dummy1_backup;
static uint32_t vpp_gainoff_backup;
void enable_dolby_vision(int enable)
{
	if (enable) {
		if (!dolby_vision_on) {
			dolby_ctrl_backup =
				VSYNC_RD_MPEG_REG(VPP_DOLBY_CTRL);
			viu_misc_ctrl_backup =
				VSYNC_RD_MPEG_REG(VIU_MISC_CTRL1);
			vpu_hdmi_fmt_backup =
				VSYNC_RD_MPEG_REG(VPU_HDMI_FMT_CTRL);
			viu_eotf_ctrl_backup =
				VSYNC_RD_MPEG_REG(VIU_EOTF_CTL);
			xvycc_lut_ctrl_backup =
				VSYNC_RD_MPEG_REG(XVYCC_LUT_CTL);
			inv_lut_ctrl_backup =
				VSYNC_RD_MPEG_REG(XVYCC_INV_LUT_CTL);
			vpp_matrix_backup =
				VSYNC_RD_MPEG_REG(VPP_MATRIX_CTRL);
			vpp_vadj_backup =
				VSYNC_RD_MPEG_REG(VPP_VADJ_CTRL);
			vpp_dummy1_backup =
				VSYNC_RD_MPEG_REG(VPP_DUMMY_DATA1);
			vpp_gainoff_backup =
				VSYNC_RD_MPEG_REG(VPP_GAINOFF_CTRL0);
			VSYNC_WR_MPEG_REG(VPP_DOLBY_CTRL,
					(0x0<<21)  /* cm_datx4_mode */
				       |(0x0<<20)  /* reg_front_cti_bit_mode */
				       |(0x0<<17)  /* vpp_clip_ext_mode 19:17 */
				       |(0x1<<16)  /* vpp_dolby2_en */
				       |(0x0<<15)  /* mat_xvy_dat_mode */
				       |(0x1<<14)  /* vpp_ve_din_mode */
				       |(0x1<<12)  /* mat_vd2_dat_mode 13:12 */
				       |(0x3<<8)   /* vpp_dpath_sel 10:8 */
				       |0x1f);   /* vpp_uns2s_mode 7:0 */
			VSYNC_WR_MPEG_REG_BITS(VIU_MISC_CTRL1,
				  (0 << 4)	/* 23-20 ext mode */
				| (0 << 3)	/* 19 osd bypass */
				| (0 << 2)	/* 18 core2 bypass */
				| (0 << 1)	/* 17 core1 el bypass */
				| (0 << 0), /* 16 core1 bl bypass */
				16, 8);
			VSYNC_WR_MPEG_REG(VPU_HDMI_FMT_CTRL, 0);
			VSYNC_WR_MPEG_REG(VIU_EOTF_CTL, 0);
			VSYNC_WR_MPEG_REG(XVYCC_LUT_CTL, 0);
			VSYNC_WR_MPEG_REG(XVYCC_INV_LUT_CTL, 0);
			VSYNC_WR_MPEG_REG(VPP_MATRIX_CTRL, 0);
			VSYNC_WR_MPEG_REG(VPP_VADJ_CTRL, 0);
			VSYNC_WR_MPEG_REG(VPP_DUMMY_DATA1, 0x20000000);
			VSYNC_WR_MPEG_REG(VPP_GAINOFF_CTRL0, 0);
			VSYNC_WR_MPEG_REG_BITS(VIU_OSD1_MATRIX_CTRL, 0, 0, 1);
			VSYNC_WR_MPEG_REG_BITS(VPP_MISC, 0, 28, 1);
			enable_osd_path(0);
			pr_dolby_dbg("Dolby Vision turn on\n");
		}
		dolby_vision_on = true;
		dolby_vision_wait_on = false;
	} else {
		if (dolby_vision_on) {
			VSYNC_WR_MPEG_REG(VPP_DOLBY_CTRL,
				dolby_ctrl_backup);
			VSYNC_WR_MPEG_REG(VIU_MISC_CTRL1,
				viu_misc_ctrl_backup);
			VSYNC_WR_MPEG_REG(VPU_HDMI_FMT_CTRL,
				vpu_hdmi_fmt_backup);
			VSYNC_WR_MPEG_REG(VIU_EOTF_CTL,
				viu_eotf_ctrl_backup);
			VSYNC_WR_MPEG_REG(XVYCC_LUT_CTL,
				xvycc_lut_ctrl_backup);
			VSYNC_WR_MPEG_REG(XVYCC_INV_LUT_CTL,
				inv_lut_ctrl_backup);
			VSYNC_WR_MPEG_REG(VPP_MATRIX_CTRL,
				vpp_matrix_backup);
			VSYNC_WR_MPEG_REG(VPP_VADJ_CTRL,
				vpp_vadj_backup);
			VSYNC_WR_MPEG_REG(VPP_DUMMY_DATA1,
				vpp_dummy1_backup);
			VSYNC_WR_MPEG_REG(VPP_GAINOFF_CTRL0,
				vpp_gainoff_backup);
			VSYNC_WR_MPEG_REG_BITS(VIU_OSD1_MATRIX_CTRL, 1, 0, 1);
			enable_osd_path(1);
			pr_dolby_dbg("Dolby Vision turn off\n");
		}
		dolby_vision_on = false;
		dolby_vision_wait_on = false;
		dolby_vision_status = BYPASS_PROCESS;
	}
}
EXPORT_SYMBOL(enable_dolby_vision);

static bool video_is_hdr10;
module_param(video_is_hdr10, bool, 0664);
MODULE_PARM_DESC(video_is_hdr10, "\n video_is_hdr10\n");

/*
    dolby vision enhanced layer receiver
*/
#define DVEL_RECV_NAME "dvel"

static inline void dvel_vf_put(struct vframe_s *vf)
{
	struct vframe_provider_s *vfp = vf_get_provider(DVEL_RECV_NAME);
	int event = 0;
	if (vfp) {
		vf_put(vf, DVEL_RECV_NAME);
		event |= VFRAME_EVENT_RECEIVER_PUT;
		vf_notify_provider(DVEL_RECV_NAME, event, NULL);
	}
}

static inline struct vframe_s *dvel_vf_peek(void)
{
	return vf_peek(DVEL_RECV_NAME);
}

static inline struct vframe_s *dvel_vf_get(void)
{
	int event = 0;
	struct vframe_s *vf = vf_get(DVEL_RECV_NAME);
	if (vf) {
		event |= VFRAME_EVENT_RECEIVER_GET;
		vf_notify_provider(DVEL_RECV_NAME, event, NULL);
	}
	return vf;
}

static struct vframe_s *dv_vf[16][2];
static void *metadata_parser;
static bool metadata_parser_reset_flag;
static char meta_buf[1024];
static char md_buf[1024];
static char comp_buf[8196];
static int dvel_receiver_event_fun(int type, void *data, void *arg)
{
	char *provider_name = (char *)data;
	int i;
	unsigned long flags;

	if (type == VFRAME_EVENT_PROVIDER_UNREG) {
		pr_info("%s, provider %s unregistered\n",
			__func__, provider_name);
		spin_lock_irqsave(&dovi_lock, flags);
		for (i = 0; i < 16; i++) {
			if (dv_vf[i][0]) {
				if (dv_vf[i][1])
					dvel_vf_put(dv_vf[i][1]);
				dv_vf[i][1] = NULL;
			}
			dv_vf[i][0] = NULL;
		}
		if (metadata_parser && p_funcs) {
			p_funcs->metadata_parser_release();
			metadata_parser = NULL;
		};
		new_dovi_setting.video_width =
		new_dovi_setting.video_height = 0;
		spin_unlock_irqrestore(&dovi_lock, flags);
		memset(&hdr10_data, 0, sizeof(hdr10_data));
		memset(&hdr10_param, 0, sizeof(hdr10_param));
		frame_count = 0;
		return -1;
	} else if (type == VFRAME_EVENT_PROVIDER_QUREY_STATE) {
		return RECEIVER_ACTIVE;
	} else if (type == VFRAME_EVENT_PROVIDER_REG) {
		pr_info("%s, provider %s registered\n",
			__func__, provider_name);
		spin_lock_irqsave(&dovi_lock, flags);
		for (i = 0; i < 16; i++)
			dv_vf[i][0] = dv_vf[i][1] = NULL;
		new_dovi_setting.video_width =
		new_dovi_setting.video_height = 0;
		spin_unlock_irqrestore(&dovi_lock, flags);
		memset(&hdr10_data, 0, sizeof(hdr10_data));
		memset(&hdr10_param, 0, sizeof(hdr10_param));
		frame_count = 0;
	}
	return 0;
}

static const struct vframe_receiver_op_s dvel_vf_receiver = {
	.event_cb = dvel_receiver_event_fun
};

static struct vframe_receiver_s dvel_vf_recv;

void dolby_vision_init_receiver(void)
{
	pr_info("%s(%s)\n", __func__, DVEL_RECV_NAME);
	vf_receiver_init(&dvel_vf_recv, DVEL_RECV_NAME,
			&dvel_vf_receiver, &dvel_vf_recv);
	vf_reg_receiver(&dvel_vf_recv);
	pr_info("%s: %s\n", __func__, dvel_vf_recv.name);
}
EXPORT_SYMBOL(dolby_vision_init_receiver);

#define MAX_FILENAME_LENGTH 64
static const char comp_file[] = "%s_comp.%04d.reg";
static const char dm_reg_core1_file[] = "%s_dm_core1.%04d.reg";
static const char dm_reg_core2_file[] = "%s_dm_core2.%04d.reg";
static const char dm_reg_core3_file[] = "%s_dm_core3.%04d.reg";
static const char dm_lut_core1_file[] = "%s_dm_core1.%04d.lut";
static const char dm_lut_core2_file[] = "%s_dm_core2.%04d.lut";

static void dump_struct(void *structure, int struct_length,
	const char file_string[], int frame_nr)
{
	char fn[MAX_FILENAME_LENGTH];
	struct file *fp;
	loff_t pos = 0;
	mm_segment_t old_fs = get_fs();
	if (frame_nr == 0)
		return;
	set_fs(KERNEL_DS);
	snprintf(fn, MAX_FILENAME_LENGTH, file_string,
		"/data/tmp/tmp", frame_nr-1);
	fp = filp_open(fn, O_RDWR|O_CREAT, 0666);
	if (fp == NULL)
		pr_info("Error open file for writing %s\n", fn);
	vfs_write(fp, structure, struct_length, &pos);
	vfs_fsync(fp, 0);
	filp_close(fp, NULL);
	set_fs(old_fs);
}

void dolby_vision_dump_struct(void)
{
	dump_struct(&dovi_setting.dm_reg1,
		sizeof(dovi_setting.dm_reg1),
		dm_reg_core1_file, frame_count);
	if (dovi_setting.el_flag)
		dump_struct(&dovi_setting.comp_reg,
			sizeof(dovi_setting.comp_reg),
		comp_file, frame_count);

	if (!is_graphics_output_off())
		dump_struct(&dovi_setting.dm_reg2,
			sizeof(dovi_setting.dm_reg2),
		dm_reg_core2_file, frame_count);

	dump_struct(&dovi_setting.dm_reg3,
		sizeof(dovi_setting.dm_reg3),
		dm_reg_core3_file, frame_count);

	dump_struct(&dovi_setting.dm_lut1,
		sizeof(dovi_setting.dm_lut1),
		dm_lut_core1_file, frame_count);
	if (!is_graphics_output_off())
		dump_struct(&dovi_setting.dm_lut2,
			sizeof(dovi_setting.dm_lut2),
			dm_lut_core2_file, frame_count);
	pr_dolby_dbg("setting for frame %d dumped\n", frame_count);
}
EXPORT_SYMBOL(dolby_vision_dump_struct);

static void dump_setting(
	struct dovi_setting_s *setting,
	int frame_cnt, int debug_flag)
{
	int i;
	uint32_t *p;

	if ((debug_flag & 0x10) && dump_enable) {
		pr_info("core1\n");
		p = (uint32_t *)&setting->dm_reg1;
		for (i = 0; i < 26; i++)
			pr_info("%x\n", p[i]);
		if (setting->el_flag) {
			pr_info("\ncomposer\n");
			p = (uint32_t *)&setting->comp_reg;
			for (i = 0; i < 173; i++)
				pr_info("%x\n", p[i]);
		}
	}

	if ((debug_flag & 0x20) && dump_enable) {
		pr_info("\ncore1lut\n");
		p = (uint32_t *)&setting->dm_lut1.TmLutI;
		for (i = 0; i < 64; i++)
			pr_info("%x, %x, %x, %x\n",
				p[i*4+3], p[i*4+2], p[i*4+1], p[i*4]);
		pr_info("\n");
		p = (uint32_t *)&setting->dm_lut1.TmLutS;
		for (i = 0; i < 64; i++)
			pr_info("%x, %x, %x, %x\n",
				p[i*4+3], p[i*4+2], p[i*4+1], p[i*4]);
		pr_info("\n");
		p = (uint32_t *)&setting->dm_lut1.SmLutI;
		for (i = 0; i < 64; i++)
			pr_info("%x, %x, %x, %x\n",
				p[i*4+3], p[i*4+2], p[i*4+1], p[i*4]);
		pr_info("\n");
		p = (uint32_t *)&setting->dm_lut1.SmLutS;
		for (i = 0; i < 64; i++)
			pr_info("%x, %x, %x, %x\n",
				p[i*4+3], p[i*4+2], p[i*4+1], p[i*4]);
		pr_info("\n");
		p = (uint32_t *)&setting->dm_lut1.G2L;
		for (i = 0; i < 64; i++)
			pr_info("%x, %x, %x, %x\n",
				p[i*4+3], p[i*4+2], p[i*4+1], p[i*4]);
		pr_info("\n");
	}

	if ((debug_flag & 0x10) && dump_enable && !is_graphics_output_off()) {
		pr_info("core2\n");
		p = (uint32_t *)&setting->dm_reg2;
		for (i = 0; i < 24; i++)
			pr_info("%x\n", p[i]);
	}

	if ((debug_flag & 0x20) && dump_enable && !is_graphics_output_off()) {
		pr_info("\ncore2lut\n");
		p = (uint32_t *)&setting->dm_lut2.TmLutI;
		for (i = 0; i < 64; i++)
			pr_info("%x, %x, %x, %x\n",
				p[i*4+3], p[i*4+2], p[i*4+1], p[i*4]);
		pr_info("\n");
		p = (uint32_t *)&setting->dm_lut2.TmLutS;
		for (i = 0; i < 64; i++)
			pr_info("%x, %x, %x, %x\n",
				p[i*4+3], p[i*4+2], p[i*4+1], p[i*4]);
		pr_info("\n");
		p = (uint32_t *)&setting->dm_lut2.SmLutI;
		for (i = 0; i < 64; i++)
			pr_info("%x, %x, %x, %x\n",
				p[i*4+3], p[i*4+2], p[i*4+1], p[i*4]);
		pr_info("\n");
		p = (uint32_t *)&setting->dm_lut2.SmLutS;
		for (i = 0; i < 64; i++)
			pr_info("%x, %x, %x, %x\n",
				p[i*4+3], p[i*4+2], p[i*4+1], p[i*4]);
		pr_info("\n");
		p = (uint32_t *)&setting->dm_lut2.G2L;
		for (i = 0; i < 64; i++)
			pr_info("%x, %x, %x, %x\n",
				p[i*4+3], p[i*4+2], p[i*4+1], p[i*4]);
		pr_info("\n");
	}

	if ((debug_flag & 0x10) && dump_enable) {
		pr_info("core3\n");
		p = (uint32_t *)&setting->dm_reg3;
		for (i = 0; i < 26; i++)
			pr_info("%x\n", p[i]);
	}

	if ((debug_flag & 0x40) && dump_enable
	&& (dolby_vision_mode <= DOLBY_VISION_OUTPUT_MODE_IPT_TUNNEL)) {
		pr_info("\ncore3_meta %d\n", setting->md_reg3.size);
		p = setting->md_reg3.raw_metadata;
		for (i = 0; i < setting->md_reg3.size; i++)
			pr_info("%x\n", p[i]);
		pr_info("\n");
	}
}

void dolby_vision_dump_setting(int debug_flag)
{
	pr_dolby_dbg("\n====== setting for frame %d ======\n", frame_count);
	dump_setting(&new_dovi_setting, frame_count, debug_flag);
	pr_dolby_dbg("=== setting for frame %d dumped ===\n\n", frame_count);
}
EXPORT_SYMBOL(dolby_vision_dump_setting);

static int sink_support_dolby_vision(const struct vinfo_s *vinfo)
{
	if (!vinfo || !vinfo->dv_info)
		return 0;
	if (vinfo->dv_info->ieeeoui != 0x00d046)
		return 0;
	/* 2160p60 if TV support */
	if ((vinfo->mode >= VMODE_4K2K_SMPTE_50HZ)
	&& (vinfo->mode >= VMODE_4K2K_SMPTE_50HZ)
	&& (vinfo->dv_info->sup_2160p60hz == 1))
		return 1;
	/* 1080p~2160p30 if TV support */
	else if ((vinfo->mode >= VMODE_1080P)
	&& (vinfo->mode <= VMODE_4K2K_SMPTE_30HZ))
		return 1;
	return 0;
}

static int dolby_vision_policy_process(
	int *mode, enum signal_format_e src_format)
{
	const struct vinfo_s *vinfo;
	int mode_change = 0;

	if ((!dolby_vision_enable) || (!p_funcs)
	|| ((get_cpu_type() != MESON_CPU_MAJOR_ID_GXM)))
		return mode_change;

	vinfo = get_current_vinfo();
	if (dolby_vision_policy == DOLBY_VISION_FOLLOW_SINK) {
		if (vinfo && sink_support_dolby_vision(vinfo)) {
			/* TV support DOVI, All -> DOVI */
			if (dolby_vision_mode !=
			DOLBY_VISION_OUTPUT_MODE_IPT_TUNNEL) {
				pr_dolby_dbg("dovi output -> DOLBY_VISION_OUTPUT_MODE_IPT_TUNNEL\n");
				*mode = DOLBY_VISION_OUTPUT_MODE_IPT_TUNNEL;
				mode_change = 1;
			}
		} else {
			/* TV not support DOVI */
			if (src_format == FORMAT_DOVI) {
				/* DOVI to SDR */
				if (dolby_vision_mode !=
				DOLBY_VISION_OUTPUT_MODE_SDR8) {
					pr_dolby_dbg("dovi output -> DOLBY_VISION_OUTPUT_MODE_SDR8\n");
					*mode = DOLBY_VISION_OUTPUT_MODE_SDR8;
					mode_change = 1;
				}
			} else if (dolby_vision_mode !=
			DOLBY_VISION_OUTPUT_MODE_BYPASS) {
				/* HDR/SDR bypass */
				pr_dolby_dbg("dovi output -> DOLBY_VISION_OUTPUT_MODE_BYPASS\n");
				*mode = DOLBY_VISION_OUTPUT_MODE_BYPASS;
				mode_change = 1;
			}
		}
	} else if (dolby_vision_policy == DOLBY_VISION_FOLLOW_SOURCE) {
		if (src_format == FORMAT_DOVI) {
			/* DOVI source */
			if (vinfo && sink_support_dolby_vision(vinfo)) {
				/* TV support DOVI, DOVI -> DOVI */
				if (dolby_vision_mode !=
				DOLBY_VISION_OUTPUT_MODE_IPT_TUNNEL) {
					pr_dolby_dbg("dovi output -> DOLBY_VISION_OUTPUT_MODE_IPT_TUNNEL\n");
					*mode =
					DOLBY_VISION_OUTPUT_MODE_IPT_TUNNEL;
					mode_change = 1;
				}
			} else {
				/* TV not support DOVI, DOVI -> SDR */
				if (dolby_vision_mode !=
				DOLBY_VISION_OUTPUT_MODE_SDR8) {
					pr_dolby_dbg("dovi output -> DOLBY_VISION_OUTPUT_MODE_SDR8\n");
					*mode = DOLBY_VISION_OUTPUT_MODE_SDR8;
					mode_change = 1;
				}
			}
		} else if (dolby_vision_mode !=
		DOLBY_VISION_OUTPUT_MODE_BYPASS) {
			/* HDR/SDR bypass */
			pr_dolby_dbg("dovi output -> DOLBY_VISION_OUTPUT_MODE_BYPASS");
			*mode = DOLBY_VISION_OUTPUT_MODE_BYPASS;
			mode_change = 1;
		}
	} else if (dolby_vision_policy == DOLBY_VISION_FORCE_OUTPUT_MODE) {
		if (dolby_vision_mode != *mode) {
			pr_dolby_dbg("dovi output mode change %d -> %d\n",
				dolby_vision_mode, *mode);
			mode_change = 1;
		}
	}
	return mode_change;
}

static bool is_dovi_frame(struct vframe_s *vf)
{
	struct provider_aux_req_s req;
	char *p;
	unsigned size = 0;
	unsigned type = 0;

	req.vf = vf;
	req.bot_flag = 0;
	req.aux_buf = NULL;
	req.aux_size = 0;
	req.dv_enhance_exist = 0;
	vf_notify_provider_by_name("dvbldec",
		VFRAME_EVENT_RECEIVER_GET_AUX_DATA,
		(void *)&req);
	if (req.dv_enhance_exist)
		return true;
	if (!req.aux_buf || !req.aux_size)
		return 0;
	p = req.aux_buf;
	while (p < req.aux_buf + req.aux_size - 8) {
		size = *p++;
		size = (size << 8) | *p++;
		size = (size << 8) | *p++;
		size = (size << 8) | *p++;
		type = *p++;
		type = (type << 8) | *p++;
		type = (type << 8) | *p++;
		type = (type << 8) | *p++;
		if (type == 0x01000000)
			return true;
		p += size;
	}
	return false;
}

/* 0: no el; >0: with el */
/* 1: need wait el vf    */
/* 2: no match el found  */
/* 3: found match el     */
int dolby_vision_wait_metadata(struct vframe_s *vf)
{
	struct provider_aux_req_s req;
	struct vframe_s *el_vf;
	int ret = 0;
	unsigned int mode;

	if (debug_dolby & 0x80) {
		if (dolby_vision_flags & FLAG_SINGLE_STEP)
			/* wait fake el for "step" */
			return 1;
		else
			dolby_vision_flags |= FLAG_SINGLE_STEP;
	}
	req.vf = vf;
	req.bot_flag = 0;
	req.aux_buf = NULL;
	req.aux_size = 0;
	req.dv_enhance_exist = 0;
	vf_notify_provider_by_name("dvbldec",
		VFRAME_EVENT_RECEIVER_GET_AUX_DATA,
		(void *)&req);
	if (req.dv_enhance_exist) {
		el_vf = dvel_vf_peek();
		while (el_vf) {
			if (debug_dolby & 2)
				pr_dolby_dbg("=== peek bl(%p-%lld) with el(%p-%lld) ===\n",
					vf, vf->pts_us64,
					el_vf, el_vf->pts_us64);
			if ((el_vf->pts_us64 == vf->pts_us64)
			|| !(dolby_vision_flags & FLAG_CHECK_ES_PTS)) {
				/* found el */
				ret = 3;
				break;
			} else if (el_vf->pts_us64 < vf->pts_us64) {
				if (debug_dolby & 2)
					pr_dolby_dbg("bl(%p-%lld) => skip el pts(%p-%lld)\n",
						vf, vf->pts_us64,
						el_vf, el_vf->pts_us64);
				el_vf = dvel_vf_get();
				dvel_vf_put(el_vf);
				vf_notify_provider(DVEL_RECV_NAME,
					VFRAME_EVENT_RECEIVER_PUT, NULL);
				if (debug_dolby & 2) {
					pr_dolby_dbg("=== get & put el(%p-%lld) ===\n",
						el_vf, el_vf->pts_us64);
				}
				/* skip old el and peek new */
				el_vf = dvel_vf_peek();
			} else {
				/* no el found */
				ret = 2;
				break;
			}
		}
		/* need wait el */
		if (el_vf == NULL) {
			pr_dolby_dbg("=== bl wait el(%p-%lld) ===\n",
				vf, vf->pts_us64);
			ret = 1;
		}
	}
	if (is_dovi_frame(vf) && !dolby_vision_on) {
		/* dovi source, but dovi not enabled */
		mode = dolby_vision_mode;
		if (dolby_vision_policy_process(
			&mode, FORMAT_DOVI)) {
			dolby_vision_set_toggle_flag(1);
			if ((mode != DOLBY_VISION_OUTPUT_MODE_BYPASS)
			&& (dolby_vision_mode ==
				DOLBY_VISION_OUTPUT_MODE_BYPASS))
				dolby_vision_wait_on = true;
			dolby_vision_mode = mode;
			ret = 1;
		}
	}
	return ret;
}

void dolby_vision_vf_put(struct vframe_s *vf)
{
	int i;
	if (vf)
		for (i = 0; i < 16; i++) {
			if (dv_vf[i][0] == vf) {
				if (dv_vf[i][1]) {
					if (debug_dolby & 2)
						pr_dolby_dbg("--- put bl(%p-%lld) with el(%p-%lld) ---\n",
							vf, vf->pts_us64,
							dv_vf[i][1],
							dv_vf[i][1]->pts_us64);
					dvel_vf_put(dv_vf[i][1]);
				} else if (debug_dolby & 2) {
					pr_dolby_dbg("--- put bl(%p-%lld) ---\n",
						vf, vf->pts_us64);
				}
				dv_vf[i][0] = NULL;
				dv_vf[i][1] = NULL;
			}
		}
}
EXPORT_SYMBOL(dolby_vision_vf_put);

struct vframe_s *dolby_vision_vf_peek_el(struct vframe_s *vf)
{
	int i;

	if (dolby_vision_flags) {
		for (i = 0; i < 16; i++) {
			if (dv_vf[i][0] == vf) {
				if (dv_vf[i][1]
				&& (dolby_vision_status == BYPASS_PROCESS))
					dv_vf[i][1]->type |= VIDTYPE_VD2;
				return dv_vf[i][1];
			}
		}
	}
	return NULL;
}
EXPORT_SYMBOL(dolby_vision_vf_peek_el);

static void dolby_vision_vf_add(struct vframe_s *vf, struct vframe_s *el_vf)
{
	int i;
	for (i = 0; i < 16; i++) {
		if (dv_vf[i][0] == NULL) {
			dv_vf[i][0] = vf;
			dv_vf[i][1] = el_vf;
			break;
		}
	}
}

static int dolby_vision_vf_check(struct vframe_s *vf)
{
	int i;
	for (i = 0; i < 16; i++) {
		if (dv_vf[i][0] == vf) {
			if (debug_dolby & 2) {
				if (dv_vf[i][1])
					pr_dolby_dbg("=== bl(%p-%lld) with el(%p-%lld) toggled ===\n",
						vf,
						vf->pts_us64,
						dv_vf[i][1],
						dv_vf[i][1]->pts_us64);
				else
					pr_dolby_dbg("=== bl(%p-%lld) toggled ===\n",
						vf,
						vf->pts_us64);
			}
			return 0;
		}
	}
	return 1;
}

static int parse_sei_and_meta(
	struct vframe_s *vf,
	struct provider_aux_req_s *req,
	int *total_comp_size,
	int *total_md_size,
	enum signal_format_e *src_format)
{
	int i;
	char *p;
	unsigned size = 0;
	unsigned type = 0;
	int md_size = 0;
	int comp_size = 0;
	int parser_ready = 0;
	int ret = 2;
	unsigned long flags;

	if ((req->aux_buf == NULL)
	|| (req->aux_size == 0))
		return 1;

	p = req->aux_buf;
	while (p < req->aux_buf + req->aux_size - 8) {
		size = *p++;
		size = (size << 8) | *p++;
		size = (size << 8) | *p++;
		size = (size << 8) | *p++;
		type = *p++;
		type = (type << 8) | *p++;
		type = (type << 8) | *p++;
		type = (type << 8) | *p++;

		if (type == 0x01000000) {
			/* source is VS10 */
			*src_format = FORMAT_DOVI;
			meta_buf[0] = meta_buf[1] = meta_buf[2] = 0;
			memcpy(&meta_buf[3], p+1, size-1);
			if ((debug_dolby & 4) && dump_enable) {
				pr_dolby_dbg("metadata(%d):\n", size);
				for (i = 0; i < size+2; i += 8)
					pr_info("\t%02x %02x %02x %02x %02x %02x %02x %02x\n",
						meta_buf[i],
						meta_buf[i+1],
						meta_buf[i+2],
						meta_buf[i+3],
						meta_buf[i+4],
						meta_buf[i+5],
						meta_buf[i+6],
						meta_buf[i+7]);
			}

			/* prepare metadata parser */
			parser_ready = 0;
			if (metadata_parser == NULL) {
				metadata_parser =
					p_funcs->metadata_parser_init(
						dolby_vision_flags
						& FLAG_CHANGE_SEQ_HEAD
						? 1 : 0);
				p_funcs->metadata_parser_reset(1);
				if (metadata_parser != NULL) {
					parser_ready = 1;
					pr_dolby_dbg("metadata parser init OK\n");
				}
			} else {
				if (p_funcs->metadata_parser_reset(
					metadata_parser_reset_flag) == 0)
					metadata_parser_reset_flag = 0;
					parser_ready = 1;
			}
			if (!parser_ready) {
				pr_dolby_error(
					"meta(%d), pts(%lld) -> metadata parser init fail\n",
					size, vf->pts_us64);
				return 2;
			}

			md_size = comp_size = 0;
			spin_lock_irqsave(&dovi_lock, flags);
			if (p_funcs->metadata_parser_process(
				meta_buf, size + 2,
				comp_buf + *total_comp_size,
				&comp_size,
				md_buf + *total_md_size,
				&md_size,
				true)) {
				pr_dolby_error(
					"meta(%d), pts(%lld) -> metadata parser process fail\n",
					size, vf->pts_us64);
				ret = 2;
			} else {
				*total_comp_size += comp_size;
				*total_md_size += md_size;
				ret = 0;
			}
			spin_unlock_irqrestore(&dovi_lock, flags);
		}
		p += size;
	}
	if (*total_md_size) {
		pr_dolby_dbg(
			"meta(%d), pts(%lld) -> md(%d), comp(%d)\n",
			size, vf->pts_us64,
			*total_md_size, *total_comp_size);
		if ((debug_dolby & 8) && dump_enable)  {
			pr_dolby_dbg("parsed md(%d):\n", *total_md_size);
			for (i = 0; i < *total_md_size + 7; i += 8) {
				pr_info("\t%02x %02x %02x %02x %02x %02x %02x %02x\n",
					md_buf[i],
					md_buf[i+1],
					md_buf[i+2],
					md_buf[i+3],
					md_buf[i+4],
					md_buf[i+5],
					md_buf[i+6],
					md_buf[i+7]);
			}
		}
	}
	return ret;
}

#define INORM	50000
static u32 bt2020_primaries[3][2] = {
	{0.17 * INORM + 0.5, 0.797 * INORM + 0.5},	/* G */
	{0.131 * INORM + 0.5, 0.046 * INORM + 0.5},	/* B */
	{0.708 * INORM + 0.5, 0.292 * INORM + 0.5},	/* R */
};

static u32 bt2020_white_point[2] = {
	0.3127 * INORM + 0.5, 0.3290 * INORM + 0.5
};

void prepare_hdr10_param(
	struct vframe_master_display_colour_s *p_mdc,
	struct hdr10_param_s *p_hdr10_param)
{
	struct vframe_content_light_level_s *p_cll =
		&p_mdc->content_light_level;
	bool flag = false;
	uint32_t max_lum = 1000 * 10000;
	uint32_t min_lum = 50;

	if ((p_mdc->present_flag)
	&& !(dolby_vision_flags & FLAG_CERTIFICAION)) {
		if (p_mdc->luminance[0])
			max_lum = p_mdc->luminance[0];
		if (p_mdc->luminance[1])
			min_lum = p_mdc->luminance[1];
		if ((p_hdr10_param->
		max_display_mastering_luminance
			!= max_lum)
		|| (p_hdr10_param->
		min_display_mastering_luminance
			!= min_lum)
		|| (p_hdr10_param->Rx
			!= p_mdc->primaries[2][0])
		|| (p_hdr10_param->Ry
			!= p_mdc->primaries[2][1])
		|| (p_hdr10_param->Gx
			!= p_mdc->primaries[0][0])
		|| (p_hdr10_param->Gy
			!= p_mdc->primaries[0][1])
		|| (p_hdr10_param->Bx
			!= p_mdc->primaries[1][0])
		|| (p_hdr10_param->By
			!= p_mdc->primaries[1][1])
		|| (p_hdr10_param->Wx
			!= p_mdc->white_point[0])
		|| (p_hdr10_param->Wy
			!= p_mdc->white_point[1]))
			flag = true;
		if (true) {
			p_hdr10_param->
			max_display_mastering_luminance
				= max_lum;
			p_hdr10_param->
			min_display_mastering_luminance
				= min_lum;
			p_hdr10_param->Rx
				= p_mdc->primaries[2][0];
			p_hdr10_param->Ry
				= p_mdc->primaries[2][1];
			p_hdr10_param->Gx
				= p_mdc->primaries[0][0];
			p_hdr10_param->Gy
				= p_mdc->primaries[0][1];
			p_hdr10_param->Bx
				= p_mdc->primaries[1][0];
			p_hdr10_param->By
				= p_mdc->primaries[1][1];
			p_hdr10_param->Wx
				= p_mdc->white_point[0];
			p_hdr10_param->Wy
				= p_mdc->white_point[1];
		}
	} else {
		if ((p_hdr10_param->
		min_display_mastering_luminance
			!= max_lum)
		|| (p_hdr10_param->
		max_display_mastering_luminance
			!= min_lum)
		|| (p_hdr10_param->Rx
			!= bt2020_primaries[2][0])
		|| (p_hdr10_param->Ry
			!= bt2020_primaries[2][1])
		|| (p_hdr10_param->Gx
			!= bt2020_primaries[0][0])
		|| (p_hdr10_param->Gy
			!= bt2020_primaries[0][1])
		|| (p_hdr10_param->Bx
			!= bt2020_primaries[1][0])
		|| (p_hdr10_param->By
			!= bt2020_primaries[1][1])
		|| (p_hdr10_param->Wx
			!= bt2020_white_point[0])
		|| (p_hdr10_param->Wy
			!= bt2020_white_point[1]))
			flag = true;
		if (true) {
			p_hdr10_param->
			min_display_mastering_luminance
				= min_lum;
			p_hdr10_param->
			max_display_mastering_luminance
				= max_lum;
			p_hdr10_param->Rx
				= bt2020_primaries[2][0];
			p_hdr10_param->Ry
				= bt2020_primaries[2][1];
			p_hdr10_param->Gx
				= bt2020_primaries[0][0];
			p_hdr10_param->Gy
				= bt2020_primaries[0][1];
			p_hdr10_param->Bx
				= bt2020_primaries[1][0];
			p_hdr10_param->By
				= bt2020_primaries[1][1];
			p_hdr10_param->Wx
				= bt2020_white_point[0];
			p_hdr10_param->Wy
				= bt2020_white_point[1];
		}
	}

	if (p_cll->present_flag) {
		if ((p_hdr10_param->max_content_light_level
			!= p_cll->max_content)
		|| (p_hdr10_param->max_pic_average_light_level
			!= p_cll->max_pic_average))
			flag = true;
		if (flag) {
			p_hdr10_param->max_content_light_level
				= p_cll->max_content;
			p_hdr10_param->max_pic_average_light_level
				= p_cll->max_pic_average;
		}
	} else {
		if ((p_hdr10_param->max_content_light_level	!= 0)
		|| (p_hdr10_param->max_pic_average_light_level != 0))
			flag = true;
		if (true) {
			p_hdr10_param->max_content_light_level = 0;
			p_hdr10_param->max_pic_average_light_level = 0;
		}
	}

	if (flag) {
		pr_dolby_dbg("HDR10:\n");
		pr_dolby_dbg("\tR = %04x, %04x\n",
			p_hdr10_param->Rx,
			p_hdr10_param->Ry);
		pr_dolby_dbg("\tG = %04x, %04x\n",
			p_hdr10_param->Gx,
			p_hdr10_param->Gy);
		pr_dolby_dbg("\tB = %04x, %04x\n",
			p_hdr10_param->Bx,
			p_hdr10_param->By);
		pr_dolby_dbg("\tW = %04x, %04x\n",
			p_hdr10_param->Wx,
			p_hdr10_param->Wy);
		pr_dolby_dbg("\tMax = %d\n",
			p_hdr10_param->
			max_display_mastering_luminance);
		pr_dolby_dbg("\tMin = %d\n",
			p_hdr10_param->
			min_display_mastering_luminance);
		pr_dolby_dbg("\tMCLL = %d\n",
			p_hdr10_param->
			max_content_light_level);
		pr_dolby_dbg("\tMPALL = %d\n\n",
			p_hdr10_param->
			max_pic_average_light_level);
	}
}

static bool send_hdmi_pkt(
	enum signal_format_e dst_format,
	const struct vinfo_s *vinfo)
{
	struct hdr_10_infoframe_s *p_hdr;
	int i;
	bool flag = false;

	if (dst_format == FORMAT_HDR10) {
		p_hdr = &dovi_setting.hdr_info;
		hdr10_data.features =
			  (1 << 29)	/* video available */
			| (5 << 26)	/* unspecified */
			| (0 << 25)	/* limit */
			| (1 << 24)	/* color available */
			| (9 << 16)	/* bt2020 */
			| (14 << 8)	/* bt2020-10 */
			| (10 << 0);/* bt2020c */
		if (hdr10_data.primaries[0][0] !=
			((p_hdr->display_primaries_x_0_MSB << 8)
			| p_hdr->display_primaries_x_0_LSB))
			flag = true;
		hdr10_data.primaries[0][0] =
			(p_hdr->display_primaries_x_0_MSB << 8)
			| p_hdr->display_primaries_x_0_LSB;

		if (hdr10_data.primaries[0][1] !=
			((p_hdr->display_primaries_y_0_MSB << 8)
			| p_hdr->display_primaries_y_0_LSB))
			flag = true;
		hdr10_data.primaries[0][1] =
			(p_hdr->display_primaries_y_0_MSB << 8)
			| p_hdr->display_primaries_y_0_LSB;

		if (hdr10_data.primaries[1][0] !=
			((p_hdr->display_primaries_x_1_MSB << 8)
			| p_hdr->display_primaries_x_1_LSB))
			flag = true;
		hdr10_data.primaries[1][0] =
			(p_hdr->display_primaries_x_1_MSB << 8)
			| p_hdr->display_primaries_x_1_LSB;

		if (hdr10_data.primaries[1][1] !=
			((p_hdr->display_primaries_y_1_MSB << 8)
			| p_hdr->display_primaries_y_1_LSB))
			flag = true;
		hdr10_data.primaries[1][1] =
			(p_hdr->display_primaries_y_1_MSB << 8)
			| p_hdr->display_primaries_y_1_LSB;

		if (hdr10_data.primaries[2][0] !=
			((p_hdr->display_primaries_x_2_MSB << 8)
			| p_hdr->display_primaries_x_2_LSB))
			flag = true;
		hdr10_data.primaries[2][0] =
			(p_hdr->display_primaries_x_2_MSB << 8)
			| p_hdr->display_primaries_x_2_LSB;

		if (hdr10_data.primaries[2][1] !=
			((p_hdr->display_primaries_y_2_MSB << 8)
			| p_hdr->display_primaries_y_2_LSB))
			flag = true;
		hdr10_data.primaries[2][1] =
			(p_hdr->display_primaries_y_2_MSB << 8)
			| p_hdr->display_primaries_y_2_LSB;

		if (hdr10_data.white_point[0] !=
			((p_hdr->white_point_x_MSB << 8)
			| p_hdr->white_point_x_LSB))
			flag = true;
		hdr10_data.white_point[0] =
			(p_hdr->white_point_x_MSB << 8)
			| p_hdr->white_point_x_LSB;

		if (hdr10_data.white_point[1] !=
			((p_hdr->white_point_y_MSB << 8)
			| p_hdr->white_point_y_LSB))
			flag = true;
		hdr10_data.white_point[1] =
			(p_hdr->white_point_y_MSB << 8)
			| p_hdr->white_point_y_LSB;

		if (hdr10_data.luminance[0] !=
			((p_hdr->max_display_mastering_luminance_MSB << 8)
			| p_hdr->max_display_mastering_luminance_LSB))
			flag = true;
		hdr10_data.luminance[0] =
			(p_hdr->max_display_mastering_luminance_MSB << 8)
			| p_hdr->max_display_mastering_luminance_LSB;

		if (hdr10_data.luminance[1] !=
			((p_hdr->min_display_mastering_luminance_MSB << 8)
			| p_hdr->min_display_mastering_luminance_LSB))
			flag = true;
		hdr10_data.luminance[1] =
			(p_hdr->min_display_mastering_luminance_MSB << 8)
			| p_hdr->min_display_mastering_luminance_LSB;

		if (hdr10_data.max_content !=
			((p_hdr->max_content_light_level_MSB << 8)
			| p_hdr->max_content_light_level_LSB))
			flag = true;
		hdr10_data.max_content =
			(p_hdr->max_content_light_level_MSB << 8)
			| p_hdr->max_content_light_level_LSB;

		if (hdr10_data.max_frame_average !=
			((p_hdr->max_frame_average_light_level_MSB << 8)
			| p_hdr->max_frame_average_light_level_LSB))
			flag = true;
		hdr10_data.max_frame_average =
			(p_hdr->max_frame_average_light_level_MSB << 8)
			| p_hdr->max_frame_average_light_level_LSB;
		if (vinfo->fresh_tx_hdr_pkt)
			vinfo->fresh_tx_hdr_pkt(&hdr10_data);
		if (vinfo->fresh_tx_vsif_pkt)
			vinfo->fresh_tx_vsif_pkt(0, 0);

		if (flag) {
			pr_dolby_dbg("Info frame for hdr10 changed:\n");
			for (i = 0; i < 3; i++)
				pr_dolby_dbg(
						"\tprimaries[%1d] = %04x, %04x\n",
						i,
						hdr10_data.primaries[i][0],
						hdr10_data.primaries[i][1]);
			pr_dolby_dbg("\twhite_point = %04x, %04x\n",
				hdr10_data.white_point[0],
				hdr10_data.white_point[1]);
			pr_dolby_dbg("\tMax = %d\n",
				hdr10_data.luminance[0]);
			pr_dolby_dbg("\tMin = %d\n",
				hdr10_data.luminance[1]);
			pr_dolby_dbg("\tMCLL = %d\n",
				hdr10_data.max_content);
			pr_dolby_dbg("\tMPALL = %d\n\n",
				hdr10_data.max_frame_average);
		}
	} else if (dst_format == FORMAT_DOVI) {
		hdr10_data.features =
			  (1 << 29)	/* video available */
			| (5 << 26)	/* unspecified */
			| (0 << 25)	/* limit */
			| (1 << 24)	/* color available */
			| (1 << 16)	/* bt709 */
			| (1 << 8)	/* bt709 */
			| (1 << 0);	/* bt709 */
		for (i = 0; i < 3; i++) {
			hdr10_data.primaries[i][0] = 0;
			hdr10_data.primaries[i][1] = 0;
		}
		hdr10_data.white_point[0] = 0;
		hdr10_data.white_point[1] = 0;
		hdr10_data.luminance[0] = 0;
		hdr10_data.luminance[1] = 0;
		hdr10_data.max_content = 0;
		hdr10_data.max_frame_average = 0;
		if (vinfo->fresh_tx_hdr_pkt)
			vinfo->fresh_tx_hdr_pkt(&hdr10_data);
		if (vinfo->fresh_tx_vsif_pkt)
			vinfo->fresh_tx_vsif_pkt(
				1, dolby_vision_mode ==
				DOLBY_VISION_OUTPUT_MODE_IPT_TUNNEL
				? 1 : 0);
	} else {
		hdr10_data.features =
			  (1 << 29)	/* video available */
			| (5 << 26)	/* unspecified */
			| (0 << 25)	/* limit */
			| (1 << 24)	/* color available */
			| (1 << 16)	/* bt709 */
			| (1 << 8)	/* bt709 */
			| (1 << 0);	/* bt709 */
		for (i = 0; i < 3; i++) {
			hdr10_data.primaries[i][0] = 0;
			hdr10_data.primaries[i][1] = 0;
		}
		hdr10_data.white_point[0] = 0;
		hdr10_data.white_point[1] = 0;
		hdr10_data.luminance[0] = 0;
		hdr10_data.luminance[1] = 0;
		hdr10_data.max_content = 0;
		hdr10_data.max_frame_average = 0;
		if (vinfo->fresh_tx_hdr_pkt)
			vinfo->fresh_tx_hdr_pkt(&hdr10_data);
		if (vinfo->fresh_tx_vsif_pkt)
			vinfo->fresh_tx_vsif_pkt(0, 0);
	}
	return flag;
}

static uint32_t null_vf_cnt;
static bool video_off_handled;
static int is_video_output_off(struct vframe_s *vf)
{
	if ((READ_VPP_REG(VPP_MISC) & (1<<10)) == 0) {
		if (vf == NULL)
			null_vf_cnt++;
		else
			null_vf_cnt = 0;
		if (null_vf_cnt > 5) {
			null_vf_cnt = 0;
			return 1;
		}
	} else
		video_off_handled = 0;
	return 0;
}

#define signal_color_primaries ((vf->signal_type >> 16) & 0xff)
#define signal_transfer_characteristic ((vf->signal_type >> 8) & 0xff)
static int dolby_vision_parse_metadata(struct vframe_s *vf)
{
	const struct vinfo_s *vinfo = get_current_vinfo();
	struct vframe_s *el_vf;
	struct provider_aux_req_s req;
	struct provider_aux_req_s el_req;
	int flag;
	enum signal_format_e src_format = FORMAT_SDR;
	enum signal_format_e dst_format;
	int total_md_size = 0;
	int total_comp_size = 0;
	bool el_flag = 0;
	bool el_halfsize_flag = 1;
	uint32_t w = vinfo->width;
	uint32_t h = vinfo->height;
	int meta_flag_bl = 1;
	int meta_flag_el = 1;
	struct vframe_master_display_colour_s *p_mdc =
		&vf->prop.master_display_colour;
	unsigned int current_mode = dolby_vision_mode;
	uint32_t target_lumin_max = 0;

	if ((!dolby_vision_enable) || (!p_funcs))
		return -1;

	if (vf) {
		pr_dolby_dbg("frame %d pts %lld:\n", frame_count, vf->pts_us64);

		/* check source format */
		if (signal_transfer_characteristic == 16)
			video_is_hdr10 = true;
		else
			video_is_hdr10 = false;

		if (video_is_hdr10 || p_mdc->present_flag)
			src_format = FORMAT_HDR10;

		/* prepare parameter from SEI for hdr10 */
		if (src_format == FORMAT_HDR10)
			prepare_hdr10_param(p_mdc, &hdr10_param);

		req.vf = vf;
		req.bot_flag = 0;
		req.aux_buf = NULL;
		req.aux_size = 0;
		req.dv_enhance_exist = 0;
		vf_notify_provider_by_name("dvbldec",
			VFRAME_EVENT_RECEIVER_GET_AUX_DATA,
	    (void *)&req);
		/* parse meta in base layer */
		if (dolby_vision_flags & FLAG_META_FROM_BL)
			meta_flag_bl = parse_sei_and_meta(
				vf, &req,
				&total_comp_size, &total_md_size,
				&src_format);
		if (req.dv_enhance_exist) {
			el_vf = dvel_vf_get();
			if (el_vf &&
			((el_vf->pts_us64 == vf->pts_us64)
			|| !(dolby_vision_flags & FLAG_CHECK_ES_PTS))) {
				if (debug_dolby & 2)
					pr_dolby_dbg("+++ get bl(%p-%lld) with el(%p-%lld) +++\n",
						vf, vf->pts_us64,
						el_vf, el_vf->pts_us64);
				el_req.vf = el_vf;
				el_req.bot_flag = 0;
				el_req.aux_buf = NULL;
				el_req.aux_size = 0;
				vf_notify_provider_by_name("dveldec",
					VFRAME_EVENT_RECEIVER_GET_AUX_DATA,
					(void *)&el_req);
				/* parse meta in enhanced layer */
				if (!(dolby_vision_flags & FLAG_META_FROM_BL))
					meta_flag_el = parse_sei_and_meta(
						el_vf, &el_req,
						&total_comp_size,
						&total_md_size,
						&src_format);
				dolby_vision_vf_add(vf, el_vf);
				if (dolby_vision_flags) {
					el_flag = 1;
					if (vf->width == el_vf->width)
						el_halfsize_flag = 0;
				}
			} else {
				if (!el_vf)
					pr_dolby_error(
						"bl(%p-%lld) not found el\n",
						vf, vf->pts_us64);
				else
					pr_dolby_error(
						"bl(%p-%lld) not found el(%p-%lld)\n",
						vf, vf->pts_us64,
						el_vf, el_vf->pts_us64);
			}
		} else {
			if (debug_dolby & 2)
				pr_dolby_dbg(
					"+++ get bl(%p-%lld) +++\n",
					vf, vf->pts_us64);
			dolby_vision_vf_add(vf, NULL);
		}
		if ((src_format == FORMAT_DOVI)
		&& meta_flag_bl && meta_flag_el) {
			/* dovi frame no meta or meta error */
			/* use old setting for this frame   */
			return -1;
		}
		w = (vf->type & VIDTYPE_COMPRESS) ?
			vf->compWidth : vf->width;
		h = (vf->type & VIDTYPE_COMPRESS) ?
			vf->compHeight : vf->height;
	}

	/* if not DOVI, release metadata_parser */
	if ((src_format != FORMAT_DOVI) && metadata_parser) {
		p_funcs->metadata_parser_release();
		metadata_parser = NULL;
	}

	current_mode = dolby_vision_mode;
	if (dolby_vision_policy_process(
		&current_mode, src_format)) {
		dolby_vision_set_toggle_flag(1);
		if ((current_mode != DOLBY_VISION_OUTPUT_MODE_BYPASS)
		&& (dolby_vision_mode == DOLBY_VISION_OUTPUT_MODE_BYPASS))
			dolby_vision_wait_on = true;
		dolby_vision_mode = current_mode;
	}

	if (dolby_vision_mode == DOLBY_VISION_OUTPUT_MODE_BYPASS) {
		new_dovi_setting.video_width = 0;
		new_dovi_setting.video_height = 0;
		return -1;
	}

	/* check target luminance */
	if (is_graphics_output_off()) {
		dolby_vision_graphic_min = 0;
		dolby_vision_graphic_max = 0;
	} else {
		dolby_vision_graphic_min = 50;
		dolby_vision_graphic_max = 100;
	}
	if (dolby_vision_flags & FLAG_USE_SINK_MIN_MAX) {
		if (vinfo->dv_info->ieeeoui == 0x00d046) {
			if (vinfo->dv_info->ver == 0) {
				/* need lookup PQ table ... */
				/*
				dolby_vision_graphic_min =
				dolby_vision_target_min =
					vinfo->dv_info.ver0.target_min_pq;
				dolby_vision_graphic_max =
				target_lumin_max =
					vinfo->dv_info.ver0.target_max_pq;
				*/
			} else if (vinfo->dv_info->ver == 1) {
				if (vinfo->dv_info->vers.ver1.target_max_lum) {
					/* Target max luminance = 100+50*CV */
					dolby_vision_graphic_max =
					target_lumin_max =
						(vinfo->dv_info->
						vers.ver1.target_max_lum
						* 50 + 100);
					/* Target min luminance = (CV/127)^2 */
					dolby_vision_graphic_min =
					dolby_vision_target_min =
						(vinfo->dv_info->
						vers.ver1.target_min_lum ^ 2)
						* 10000 / (127 * 127);
				}
			}
		} else if (vinfo->hdr_info.hdr_support & 4) {
			if (vinfo->hdr_info.lumi_max) {
				/* Luminance value = 50 * (2 ^ (CV/32)) */
				dolby_vision_graphic_max =
				target_lumin_max = 50 *
					(2 ^ (vinfo->hdr_info.lumi_max >> 5));
				/* Desired Content Min Luminance =
					Desired Content Max Luminance
					* (CV/255) * (CV/255) / 100	*/
				dolby_vision_graphic_min =
				dolby_vision_target_min =
					target_lumin_max * 10000
					* vinfo->hdr_info.lumi_min
					* vinfo->hdr_info.lumi_min
					/ (255 * 255 * 100);
			}
		}
		if (target_lumin_max) {
			dolby_vision_target_max[0][0] =
			dolby_vision_target_max[0][1] =
			dolby_vision_target_max[1][0] =
			dolby_vision_target_max[1][1] =
			dolby_vision_target_max[2][0] =
			dolby_vision_target_max[2][1] =
				target_lumin_max;
		} else {
			memcpy(
				dolby_vision_target_max,
				dolby_vision_default_max,
				sizeof(dolby_vision_target_max));
		}
	}

	/* check dst format */
	if ((dolby_vision_mode == DOLBY_VISION_OUTPUT_MODE_IPT_TUNNEL)
			|| (dolby_vision_mode == DOLBY_VISION_OUTPUT_MODE_IPT))
		dst_format = FORMAT_DOVI;
	else if (dolby_vision_mode == DOLBY_VISION_OUTPUT_MODE_HDR10)
		dst_format = FORMAT_HDR10;
	else
		dst_format = FORMAT_SDR;

	new_dovi_setting.video_width = w << 16;
	new_dovi_setting.video_height = h << 16;
	flag = p_funcs->control_path(
		src_format, dst_format,
		comp_buf, total_comp_size,
		md_buf, total_md_size,
		VIDEO_PRIORITY,
		12, 0, SIG_RANGE_SMPTE, /* bit/chroma/range */
		dolby_vision_graphic_min,
		dolby_vision_graphic_max * 10000,
		dolby_vision_target_min,
		dolby_vision_target_max[src_format][dst_format] * 10000,
		(!el_flag) ||
		(dolby_vision_flags & FLAG_DISABLE_COMPOSER),
		&hdr10_param,
		&new_dovi_setting);
	if (flag >= 0) {
		new_dovi_setting.src_format = src_format;
		new_dovi_setting.dst_format = dst_format;
		new_dovi_setting.el_flag = el_flag;
		new_dovi_setting.el_halfsize_flag = el_halfsize_flag;
		new_dovi_setting.video_width = w;
		new_dovi_setting.video_height = h;
		if (el_flag)
			pr_dolby_dbg("setting %d->%d(T:%d-%d): flag=%02x,md_%s=%d,comp=%d\n",
				src_format, dst_format,
				dolby_vision_target_min,
				dolby_vision_target_max[src_format][dst_format],
				flag, meta_flag_bl ? "el" : "bl",
				total_md_size, total_comp_size);
		else
			pr_dolby_dbg("setting %d->%d(T:%d-%d): flag=%02x,md_%s=%d\n",
				src_format, dst_format,
				dolby_vision_target_min,
				dolby_vision_target_max[src_format][dst_format],
				flag, meta_flag_bl ? "el" : "bl",
				total_md_size);
		dump_setting(&new_dovi_setting, frame_count, debug_dolby);
		return 0; /* setting updated */
	} else {
		new_dovi_setting.video_width = 0;
		new_dovi_setting.video_height = 0;
		pr_dolby_error("control_path() failed\n");
	}
	return -1; /* do nothing for this frame */
}

int dolby_vision_update_metadata(struct vframe_s *vf)
{
	int ret = -1;

	if (!dolby_vision_enable)
		return -1;

	if (vf && dolby_vision_vf_check(vf)) {
		ret = dolby_vision_parse_metadata(vf);
		frame_count++;
	}

	return ret;
}
EXPORT_SYMBOL(dolby_vision_update_metadata);

static unsigned int last_dolby_vision_policy;
int dolby_vision_process(struct vframe_s *vf)
{
	const struct vinfo_s *vinfo = get_current_vinfo();

	if (get_cpu_type() != MESON_CPU_MAJOR_ID_GXM)
		return -1;

	if ((!dolby_vision_enable) || (!p_funcs))
		return -1;

	if (is_dolby_vision_on()
	&& is_video_output_off(vf)
	&& !video_off_handled) {
		dolby_vision_set_toggle_flag(1);
		pr_dolby_dbg("video off\n");
		if (debug_dolby)
			video_off_handled = 1;
	}

	if (last_dolby_vision_policy != dolby_vision_policy) {
		/* handle policy change */
		dolby_vision_set_toggle_flag(1);
		last_dolby_vision_policy = dolby_vision_policy;
	} else if (dolby_vision_mode == DOLBY_VISION_OUTPUT_MODE_BYPASS) {
		if (dolby_vision_status != BYPASS_PROCESS) {
			enable_dolby_vision(0);
			send_hdmi_pkt(FORMAT_SDR, vinfo);
			if (dolby_vision_flags & FLAG_TOGGLE_FRAME)
				dolby_vision_flags &= ~FLAG_TOGGLE_FRAME;
		}
		return 0;
	}

	if (!vf) {
		if ((dolby_vision_flags & FLAG_TOGGLE_FRAME))
			dolby_vision_parse_metadata(NULL);
	}
	if (dolby_vision_flags & FLAG_RESET_EACH_FRAME)
		dolby_vision_set_toggle_flag(1);

	if (dolby_vision_flags & FLAG_TOGGLE_FRAME) {
		if ((new_dovi_setting.video_width & 0xffff)
		&& (new_dovi_setting.video_height & 0xffff)) {
			memcpy(&dovi_setting, &new_dovi_setting,
				sizeof(dovi_setting));
			new_dovi_setting.video_width =
			new_dovi_setting.video_height = 0;
		}
		dolby_core1_set(
			24,	173, 256 * 5,
			(uint32_t *)&dovi_setting.dm_reg1,
			(uint32_t *)&dovi_setting.comp_reg,
			(uint32_t *)&dovi_setting.dm_lut1,
			dovi_setting.video_width,
			dovi_setting.video_height,
			1, /* BL enable */
			dovi_setting.el_flag, /* EL enable */
			dovi_setting.el_halfsize_flag, /* if BL and EL is 4:1 */
			dolby_vision_mode ==
			DOLBY_VISION_OUTPUT_MODE_IPT_TUNNEL,
			dovi_setting.src_format == FORMAT_DOVI,
			1);
		dolby_core2_set(
			24, 256 * 5,
			(uint32_t *)&dovi_setting.dm_reg2,
			(uint32_t *)&dovi_setting.dm_lut2,
			1920, 1080, 1, 1);
		dolby_core3_set(
			26, dovi_setting.md_reg3.size,
			(uint32_t *)&dovi_setting.dm_reg3,
			dovi_setting.md_reg3.raw_metadata,
			vinfo->width, vinfo->height, 1,
			dolby_vision_mode ==
			DOLBY_VISION_OUTPUT_MODE_IPT_TUNNEL);
		enable_dolby_vision(1);
		/* update dolby_vision_status */
		if ((dovi_setting.src_format == FORMAT_DOVI)
		&& (dolby_vision_status != DV_PROCESS)) {
			pr_dolby_dbg(
				"Dolby Vision mode changed to DV_PROCESS %d\n",
				dovi_setting.src_format);
			dolby_vision_status = DV_PROCESS;
		} else if ((dovi_setting.src_format == FORMAT_HDR10)
		&& (dolby_vision_status != HDR_PROCESS)) {
			pr_dolby_dbg(
				"Dolby Vision mode changed to HDR_PROCESS %d\n",
				dovi_setting.src_format);
			dolby_vision_status = HDR_PROCESS;
		} else if ((dovi_setting.src_format == FORMAT_SDR)
		&& (dolby_vision_status != SDR_PROCESS)) {
			pr_dolby_dbg(
				"Dolby Vision mode changed to SDR_PROCESS %d\n",
				dovi_setting.src_format);
			dolby_vision_status = SDR_PROCESS;
		}
		/* send HDMI packet according to dst_format */
		send_hdmi_pkt(dovi_setting.dst_format, vinfo);
		dolby_vision_flags &= ~FLAG_TOGGLE_FRAME;
	}
	return 0;
}
EXPORT_SYMBOL(dolby_vision_process);

bool is_dolby_vision_on(void)
{
	return dolby_vision_on
	|| dolby_vision_wait_on;
}
EXPORT_SYMBOL(is_dolby_vision_on);

bool for_dolby_vision_certification(void)
{
	return is_dolby_vision_on() &&
		dolby_vision_flags & FLAG_CERTIFICAION;
}
EXPORT_SYMBOL(for_dolby_vision_certification);

void dolby_vision_set_toggle_flag(int flag)
{
	if (flag)
		dolby_vision_flags |= FLAG_TOGGLE_FRAME;
	else
		dolby_vision_flags &= ~FLAG_TOGGLE_FRAME;
}
EXPORT_SYMBOL(dolby_vision_set_toggle_flag);

void set_dolby_vision_mode(int mode)
{
	if ((get_cpu_type() == MESON_CPU_MAJOR_ID_GXM)
	&& dolby_vision_enable) {
		video_is_hdr10 = false;
		if (dolby_vision_policy_process(
			&mode, FORMAT_SDR)) {
			dolby_vision_set_toggle_flag(1);
			if ((mode != DOLBY_VISION_OUTPUT_MODE_BYPASS)
			&& (dolby_vision_mode ==
				DOLBY_VISION_OUTPUT_MODE_BYPASS))
				dolby_vision_wait_on = true;
			pr_info("DOVI output change from %d to %d\n",
				dolby_vision_mode, mode);
			dolby_vision_mode = mode;
		}
	}
}
EXPORT_SYMBOL(set_dolby_vision_mode);

int get_dolby_vision_mode(void)
{
	return dolby_vision_mode;
}
EXPORT_SYMBOL(get_dolby_vision_mode);

bool is_dolby_vision_enable(void)
{
	return dolby_vision_enable;
}
EXPORT_SYMBOL(is_dolby_vision_enable);

int register_dv_functions(const struct dolby_vision_func_s *func)
{
	int ret = -1;
	if (!p_funcs && func) {
		pr_info("*** register_dv_functions\n ***");
		p_funcs = func;
		ret = 0;
	}
	return ret;
}
EXPORT_SYMBOL(register_dv_functions);

int unregister_dv_functions(void)
{
	int ret = -1;
	if (p_funcs) {
		pr_info("*** unregister_dv_functions\n ***");
		p_funcs = NULL;
		ret = 0;
	}
	return ret;
}
EXPORT_SYMBOL(unregister_dv_functions);
static uint64_t dma_data[3754] = {
	0x0000000000000456, 0x0000000100000042,
	0x0000000200000001, 0x0000000600000fff,
	0x0000000600000fff, 0x0000000700010010,
	0x0000000800000000, 0x0000000900000000,
	0x0000000a00000000, 0x0000000b031f2000,
	0x0000000c20000691, 0x0000000d0443fc5b,
	0x0000000e010b2000, 0x0000000f000dea57,
	0x00000010004d8000, 0x000000110004f000,
	0x00000012ff5b1000, 0x0000001300000001,
	0x0000001400000000, 0x0000001500000000,
	0x0000001600000000, 0x00000017ffff7fff,
	0x00000018ffffffff, 0x00000019ffff7fff,
	0x0000001affffffff, 0x0000001b000f7fff,
	0x0000001c06660667, 0x0000001d474c0333,
	0x0000001e0657b25e, 0x0000001f05b70ce4,
	0x00000020000ced65, 0x0000002100000000,
	0x0000002200000000, 0x000000230000000c,
	0x0000002410003ffc, 0x0000002500001fff,
	0x0000002600000fff, 0x00000027c00001f0,
	0x00000028ffffc000, 0x000000294000569e,
	0x0000002a0000c17b, 0x0000002b00008000,
	0x0000002c00008000, 0x0000002d00000000,
	0x0000002e00000000, 0x0000002f0043777f,
	0x0000003000000000, 0x0000003100000000,
	0x0000003200000000, 0x0000003300000000,
	0x0000003400000000, 0x0000003500000000,
	0x0000003600000000, 0x0000003700000000,
	0x0000003800000000, 0x0000003900000000,
	0x0000003a00000000, 0x0000003b00000000,
	0x0000003c00000000, 0x0000003d00000000,
	0x0000003e00000000, 0x0000003f00000000,
	0x0000004000000000, 0x0000004100000000,
	0x0000004200000000, 0x0000004300000000,
	0x0000004400000000, 0x0000004500000000,
	0x0000004600000000, 0x0000004700000000,
	0x0000004800000000, 0x0000004900000000,
	0x0000004a00000000, 0x0000004b00000000,
	0x0000004c00000000, 0x0000004d00000000,
	0x0000004e00000000, 0x0000004f00000000,
	0x0000005000000000, 0x0000005100000000,
	0x0000005200000000, 0x0000005300000000,
	0x0000005400000000, 0x0000005500000000,
	0x0000005600000000, 0x0000005700000000,
	0x0000005800000000, 0x0000005900000000,
	0x0000005a00000000, 0x0000005b00000000,
	0x0000005c00000000, 0x0000005d00000000,
	0x0000005e00000000, 0x0000005f00000000,
	0x0000006000000000, 0x0000006100000000,
	0x0000006200000000, 0x0000006300000000,
	0x0000006400000000, 0x0000006500000000,
	0x0000006600000000, 0x0000006700000000,
	0x0000006800000000, 0x0000006900000000,
	0x0000006a00000000, 0x0000006b00000000,
	0x0000006c00000000, 0x0000006d00000000,
	0x0000006e00000000, 0x0000006f00000000,
	0x0000007000000000, 0x0000007100000000,
	0x0000007200000000, 0x0000007300000000,
	0x0000007400000000, 0x0000007500000000,
	0x0000007600000000, 0x0000007700000000,
	0x0000007800000000, 0x0000007900000000,
	0x0000007a00000000, 0x0000007b00000000,
	0x0000007c00000000, 0x0000007d00000000,
	0x0000007e00000000, 0x0000007f00000000,
	0x0000008000000000, 0x0000008100000000,
	0x0000008200000000, 0x0000008300000000,
	0x0000008400000000, 0x0000008500000000,
	0x0000008600000000, 0x0000008700000000,
	0x0000008800000000, 0x0000008900000000,
	0x0000008a00000000, 0x0000008b00000000,
	0x0000008c00000000, 0x0000008d00000000,
	0x0000008e00000000, 0x0000008f00000000,
	0x0000009000000000, 0x0000009100000000,
	0x0000009200000000, 0x0000009300000000,
	0x0000009400000000, 0x0000009500000000,
	0x0000009600000000, 0x0000009700000000,
	0x0000009800000000, 0x0000009900000000,
	0x0000009a00000000, 0x0000009b00000000,
	0x0000009c00000000, 0x0000009d00000000,
	0x0000009e00000000, 0x0000009f00000000,
	0x000000a000000000, 0x000000a100000000,
	0x000000a200000000, 0x000000a300000000,
	0x000000a400000000, 0x000000a500000000,
	0x000000a600000000, 0x000000a700000000,
	0x000000a800000000, 0x000000a900000000,
	0x000000aa00000000, 0x000000ab00000000,
	0x000000ac00000000, 0x000000ad00000000,
	0x000000ae00000000, 0x000000af00000000,
	0x000000b000000000, 0x000000b100000000,
	0x000000b200000000, 0x000000b300000000,
	0x000000b400000000, 0x000000b500000000,
	0x000000b600000000, 0x000000b700000000,
	0x000000b800000000, 0x000000b900000000,
	0x000000ba00000000, 0x000000bb00000000,
	0x000000bc00000000, 0x000000bd00000000,
	0x000000be00000000, 0x000000bf00000000,
	0x000000c000000000, 0x000000c100000000,
	0x000000c200000000, 0x000000c300000000,
	0x000000c400000000, 0x000000c500000000,
	0x000000c600000000, 0x000000c700000000,
	0x000000c800000000, 0x000000c900000000,
	0x000000ca00000000, 0x000000cb00000000,
	0x000000cc00000000, 0x000000cd00000000,
	0x000000ce00000000, 0x000000cf00000000,
	0x000000d000000000, 0x000000d100000000,
	0x000000d200000000, 0x000000d300000000,
	0x000000d400000000, 0x000000d500000000,
	0x000000d600000000, 0x000000d700000000,
	0x000000d800000000, 0x000000d900000000,
	0x000000da00000000, 0x000000db00000000,
	0x000000dc00000000, 0x000000dd00000000,
	0x000000de00000000, 0x0000000300000001,
	0x0003e03e0003e03e, 0x0003e03e0003e03e,
	0x0004a0460004203f, 0x0006005a0005404f,
	0x000780720006c066, 0x0009208c0008507f,
	0x000ae0a7000a0099, 0x000ca0c3000bc0b5,
	0x000e70df000d80d1, 0x001040fc000f50ee,
	0x0012111a0011310b, 0x0013f13800130129,
	0x0015d1560014e147, 0x0017c1740016d165,
	0x0019b1930018b184, 0x001ba1b2001aa1a2,
	0x001d91d1001c91c1, 0x001f81f0001e91e1,
	0x0021821000208200, 0x0023823000228220,
	0x0025724f00247240, 0x0027726f0026725f,
	0x002982900028827f, 0x002b82b0002a82a0,
	0x002d82d0002c82c0, 0x002f92f1002e82e0,
	0x0031931100309301, 0x0033a33200329321,
	0x0035a3520034a342, 0x0037b3730036b363,
	0x0039c3940038b383, 0x003bd3b4003ac3a4,
	0x003dd3d5003cd3c5, 0x003fe3f6003ee3e6,
	0x0041f4170040f406, 0x004404370042f427,
	0x0046045800450448, 0x0048147900471469,
	0x004a249900491489, 0x004c24ba004b24aa,
	0x004e34db004d24ca, 0x005034fb004f34eb,
	0x0052351b0051350b, 0x0054353b0053352b,
	0x0056355b0055354b, 0x0058357b0057356b,
	0x005a359b0059358b, 0x005c35bb005b35ab,
	0x005e25da005d25cb, 0x006015f9005f25ea,
	0x0062061900611609, 0x0063f63700630628,
	0x0065e6560064e647, 0x0067c6750066d665,
	0x0069a6930068b684, 0x006b86b1006a96a2,
	0x006d66ce006c76c0, 0x006f36ec006e56dd,
	0x00710709007026fb, 0x0072d7260071f718,
	0x0074a7430073c734, 0x0076675f00758751,
	0x0078277b0077476d, 0x0079e79700790789,
	0x007b97b2007ab7a4, 0x007d47cd007c67c0,
	0x007ee7e8007e17db, 0x00809802007fc7f5,
	0x0082381c0081680f, 0x0083c83600830829,
	0x0085684f00849843, 0x0086e8680086285c,
	0x008878810087b875, 0x0089f8990089388d,
	0x008b78b1008ab8a5, 0x008ce8c8008c38bd,
	0x008e58e0008da8d4, 0x008fc8f6008f18eb,
	0x0091290d00907902, 0x009289230091d918,
	0x0093e9380093392e, 0x0095394e00948943,
	0x009689630095d958, 0x0097c9770097296d,
	0x0099098b00986981, 0x009a499f0099a995,
	0x009b79b2009ae9a9, 0x009ca9c5009c19bc,
	0x009dd9d8009d39cf, 0x009ef9ea009e69e1,
	0x00a019fc009f89f3, 0x00a12a0e00a09a05,
	0x00a23a1f00a1ba16, 0x00a34a3000a2ca27,
	0x00a44a4000a3ca38, 0x00a55a5100a4da48,
	0x00a64a6000a5ca58, 0x00a74a7000a6ca68,
	0x00a83a7f00a7ba77, 0x00a92a8e00a8aa86,
	0x00aa0a9c00a99a95, 0x00aaeaab00aa7aa4,
	0x00abcab800ab5ab2, 0x00ac9ac600ac3abf,
	0x00ad3ad300ad0acd, 0x00ad3ad300ad3ad3,
	0x00ad3ad300ad3ad3, 0x00ad3ad300ad3ad3,
	0x00ad3ad300ad3ad3, 0x00ad3ad300ad3ad3,
	0x00ad3ad300ad3ad3, 0x00ad3ad300ad3ad3,
	0x00ad3ad300ad3ad3, 0x00ad3ad300ad3ad3,
	0x00ad3ad300ad3ad3, 0x00ad3ad300ad3ad3,
	0x00ad3ad300ad3ad3, 0x00ad3ad300ad3ad3,
	0x00ad3ad300ad3ad3, 0x00ad3ad300ad3ad3,
	0x00ad3ad300ad3ad3, 0x00ad3ad300ad3ad3,
	0x00ad3ad300ad3ad3, 0x00ad3ad300ad3ad3,
	0x00ad3ad300ad3ad3, 0x00ad3ad300ad3ad3,
	0x00ad3ad300ad3ad3, 0x00ad3ad300ad3ad3,
	0x00001f1300000519, 0x0000afd500005624,
	0x0001da7000013084, 0x0003bd900002b479,
	0x000672ed0004fd4b, 0x000a066000081cfd,
	0x000e995f000c3184, 0x00140d3d00112aec,
	0x001aafe400174373, 0x00226a8c001e689a,
	0x002b65450026c6cd, 0x00357ffd00303ce1,
	0x0040ed2d003b11e7, 0x004dbc73004733f2,
	0x005b9e2800547ca4, 0x006b02e2006320a4,
	0x007bdee00073432e, 0x008e19d00084d276,
	0x00a18d6f00980703, 0x00b6d7d700ac0fc6,
	0x00cd8b7b00c24c4b, 0x00e6083900d9ef23,
	0x0100465300f2ccb9, 0x011b9777010d48ce,
	0x01391d350129fd5e, 0x0158293c014845c4,
	0x017895c60168020d, 0x019b22950189ec28,
	0x01bed6c401ad19ba, 0x01e5a60a01d25ce8,
	0x020d740401f8970b, 0x02373e6d0221f30d,
	0x0262fa9e024d5edc, 0x02909bff027962b7,
	0x02c0102b02a728e6, 0x02f13eec02d83d80,
	0x03240d2303095bca, 0x035a49ba033fa9d0,
	0x0390102c0373c624, 0x03cb654303ad4399,
	0x0405eff203e83da7, 0x04418ea2042485d2,
	0x048338e604646fb2, 0x04c3571204a2d919,
	0x0507058404e4b999, 0x054e75a1052a419d,
	0x0596a2970573a669, 0x05df42cb05ba7f72,
	0x062b933b06086a81, 0x06780a68064f846c,
	0x06c82df106a1884f, 0x071c38e906f3b2ee,
	0x077005e60745a751, 0x07c7b294079fbfcd,
	0x082365ae07f5086b, 0x087e6e560852d21b,
	0x08dd75ab08aff157, 0x093b471a090be26d,
	0x09a289c509711643, 0x0a08870b09d504b3,
	0x0a6cb3760a3d188e, 0x0adb0b100aa353ce,
	0x0b4772d10b0d8dbd, 0x0bb80f7b0b82860d,
	0x0c26199b0bee9895, 0x0c9822680c65beb8,
	0x0d15e1830cda1c15, 0x0d90b8da0d4b15b8,
	0x0e07f63c0dcfd283, 0x0e8351480e493db7,
	0x0f02e8fd0ec6d616, 0x0f86dc580f48ba9b,
	0x100f59d50fcf1205, 0x109c8832105a0acf,
	0x1124a8b610e00cff, 0x11bb5f8e116a5b57,
	0x12422b2e11f914d4, 0x12d7cb1c128c6032,
	0x137233271324642e, 0x140609fa13c14786,
	0x149e3c73145772e9, 0x1547331214f21130,
	0x15e8f5c515914918, 0x168284e21635399e,
	0x172da19416d0df50, 0x17ddcc1e177e48d3,
	0x1885189f1830df2a, 0x1930ef4218da70bd,
	0x19e17e8419889bf0, 0x1a96e5621a3b96ff,
	0x1b513b1a1af3716b, 0x1c10b5e71bb051ee,
	0x1cc4cfa01c620798, 0x1d7d6bbb1d28f44c,
	0x1e4c1f8f1de41373, 0x1f0e89e01ea3e37a,
	0x1fd5db4b1f7aa427, 0x20a22b10203219da,
	0x2173906c2100b670, 0x2236863121d47fda,
	0x23120efc22ad9517, 0x23de6e3f238c24a2,
	0x24c49e2d245b4cac, 0x259ae2e4252f2595,
	0x268c35ed261dc715, 0x276cdc0d26e57579,
	0x285297c527df1356, 0x293d8fd028c76958,
	0x2a2de32d299d0b5c, 0x2b0adfd92aa81f91,
	0x2c05b39d2b87978a, 0x2d0637ec2c853bd4,
	0x2df218652d6e7a43, 0x2ee2d0792e7739e7,
	0x2ff4096f2f6aa384, 0x30ef41eb307efa7a,
	0x31ef97bd317d037b, 0x32f531a3328048d1,
	0x340026d93388d9f9, 0x3510965f3496d5f2,
	0x36268fb335aa53f8, 0x3742415036c3730b,
	0x3863bab637c22e3e, 0x396a06cd38e649ad,
	0x3a96d1c939eef9d3, 0x3bc9c1863b1e7658,
	0x3cdfd9d73c542edb, 0x3dfb00003d6cca38,
	0x68f214c83f8ce142, 0x0000084791f15584,
	0x299115983d95415a, 0x000007d39e014382,
	0x6a711587cda2514d, 0x000007bfac31637c,
	0xdb7e17e7b6b1d170, 0x0000074cb9d2147a,
	0x6bf932b6ebbc32a8, 0x0000063ec2d3b469,
	0xec694f35dcc5844a, 0x000008478e314a56,
	0x693c16185190b156, 0x0000085297616a85,
	0xea041738459b8170, 0x000007fba5d16c82,
	0x6b201707bfac0163, 0x000007acb8a17f7b,
	0x6c791a17a2bfd190, 0x0000078acfe1b479,
	0x2d5034a725d3f24f, 0x000005fcd7541968,
	0xd8f615056fd5e4f1, 0x0000085a92215e84,
	0x599717886395816c, 0x000008619de18486,
	0xca8919a85aa2e18f, 0x00000838aef1a384,
	0x3bf318e812b631a7, 0x00000797c771a17a,
	0xbda21cb789d071b5, 0x0000076ce461e277,
	0xde5e32075deef1fa, 0x000005beda047969,
	0x3937166854908157, 0x0000086f97117686,
	0xaa011958769b5186, 0x0000087ca581a787,
	0xeb241cd87eab81b9, 0x0000087cb9c1e287,
	0x3cb5211879c221f9, 0x00000868d5722a87,
	0x2ec8255856e08242, 0x000007f2edc23b83,
	0x1e6e30375beff1fc, 0x0000085991815c6b,
	0xa98817f86a94a16d, 0x000008849d019187,
	0x9a7c1ba88fa201a5, 0x000008a5ae31d289,
	0x0bd620b8b1b561ed, 0x000008cfc6422d8c,
	0x2dae2798e0d01251, 0x00000904e6a2a48f,
	0xfe902c6922e972b9, 0x0000094fe8c2cc93,
	0xe925161941e902c6, 0x0000087095a17385,
	0x19e619b88299b187, 0x000008a1a3b1b289,
	0x5b071e88b2a9b1cc, 0x000008dcb802088c,
	0x1c9e2558f5c0822d, 0x00000932d4528391,
	0x1e812e1955dfd2b4, 0x000009bce732fa98,
	0xce5a3289f8e66312, 0x00000a50e52337a2,
	0x5966177862930164, 0x000008899aa18d87,
	0x0a501bd89b9f71a3, 0x000008c7ab31d98b,
	0xfba221f8e1b231fa, 0x00000921c2e2488f,
	0x3d7a2aa948ccc277, 0x000009a3e3a2e497,
	0x6e5832c9e7e6a30c, 0x00000a86e4534ea3,
	0x6e24389ad4e3336e, 0x00000865937167b1,
	0xe9b419187996f17a, 0x000008a3a041a988,
	0x7ac31e48bca5e1c5, 0x000008f6b362078d,
	0x5c4825d91bbb7230, 0x00000973ce929094,
	0x3e623089a8d9c2c9, 0x00000a39e5732d9e,
	0x7e2b37ca96e42354, 0x00000b59e153a5af,
	0x693c169bb1e013ca, 0x0000087a97517c86,
	0x9a0a1ad8929ba193, 0x000008c4a651ca8a,
	0x8b3f2118e4acb1eb, 0x00000931bc123b90,
	0x6cf42a2960c5226c, 0x000009d2da92de99,
	0x7e49347a17e5f31f, 0x00000adfe31372a7,
	0xddfe3cfb4de183a0, 0x00000c26de63fbbb,
	0xb97617d86793e169, 0x000008929bb19487,
	0xaa641cc8aca0b1ae, 0x000008ecaca1ef8c,
	0x1bbd242914b3c216, 0x00000975c4d27494,
	0x0da02eb9afcee2ac, 0x00000a3be5632e9f,
	0x1e25387aa1e3f359, 0x00000b88e0a3b9b1,
	0x8dd341dc02dee3ec, 0x0000086693b168c7,
	0x29b719387a97317c, 0x000008ada051ae89,
	0x1abf1ef8cda5c1cc, 0x0000091cb2f2178f,
	0x2c3827694bbac244, 0x000009bfcd42af98,
	0x1e40334a04d812ee, 0x00000ab9e39363a5,
	0x9e033c6b2de1f393, 0x00000c29de53fcba,
	0x4936167ca7dc8430, 0x0000087796c17986,
	0xc9f91ab88f9ae190, 0x000008cca4d1c98a,
	0xeb172138f2aac1ec, 0x00000950b8f24091,
	0x6ca92aa987c14272, 0x00000a0dd4d2e99c,
	0xfe38365a5be0132d, 0x00000b33e1d396ab,
	0x3de3400bb1e013ca, 0x00000cb4dc5436c3,
	0x496117586192d164, 0x0000088b9a018c87,
	0x9a371c48a89e71a6, 0x000008f0a911e68c,
	0xeb6523891caf520c, 0x00000987be126894,
	0xcd042db9c5c6c29e, 0x00000a5adac31da0,
	0x6e20390ab4e3b361, 0x00000ba1e043c3b2,
	0x2dc942ec22de73f9, 0x0000085d92115fca,
	0x598d18586e952170, 0x000008a29d019f88,
	0xaa6d1dd8c4a191bc, 0x00000916ac92028e,
	0xfba325a948b3022b, 0x000009bdc2128d97,
	0xed43304a02cab2c6, 0x00000aa2de6349a4,
	0xce0d3b4b07e28383, 0x00000bf8df03e7b7,
	0x891215ac75dd441b, 0x0000086894016a85,
	0xa9b319687e97617e, 0x000008bc9f71b289,
	0xda961f48e2a421d1, 0x0000093caf321b90,
	0xebca276972b59246, 0x000009efc452ab9a,
	0x7d5a323a38cca2e4, 0x00000adedf0366a8,
	0x9dff3cdb46e1939d, 0x00000c2ede43febb,
	0x092a162851901154, 0x0000087595c17586,
	0x29cf1a689199218c, 0x000008d6a121c38b,
	0xdaae2078ffa5c1e3, 0x00000960b0722f92,
	0x6bd428a998b6a25b, 0x00000a19c482be9d,
	0x3d42333a63cc22f7, 0x00000b0adc4372ab,
	0x4df93d8b69e113ac, 0x0000084a8ed14dbd,
	0xc93e16b85791215a, 0x0000088796e18086,
	0x99dd1b38a69a3198, 0x000008f0a1d1d18c,
	0x9ab021591aa641f1, 0x0000097db0423c94,
	0x3bbf2949b6b5e266, 0x00000a36c252c59f,
	0xccf9331a7ec8f2fa, 0x00000b1ed5e369ac,
	0x78b813bb73db139f, 0x0000083a8de14383,
	0xb94a14183190e148, 0x000007db98d13880,
	0xfa1414b7d59cd141, 0x000007c7a631567c,
	0x1a98256766a6d1ec, 0x000006e0acf2bb72,
	0x2b5239569bb0f324, 0x00000601b9741165,
	0x1c105385a6bd849c, 0x000004cfc325e954,
	0x48f314b83d8ca141, 0x0000084292615484,
	0x19b214882e964156, 0x000007d0a011487f,
	0x1aac1607c9a52153, 0x000007b7b1016e7c,
	0xbb8325f7adb7d17e, 0x000006c4bc82e471,
	0x1c6d3f866ec18369, 0x000005aacbf49761,
	0x5cc060f533d0654e, 0x000008448da1474b,
	0x093e15f84e907153, 0x0000084697f16685,
	0x0a261648319cb16a, 0x000007c4a8d15c80,
	0x0b6317a7baaf216a, 0x000007a4be018c7b,
	0xad011b4798c6a19f, 0x000006d3d182cd78,
	0xadb944d656d6c38e, 0x0000055fd5050a5d,
	0x98e914c4cccd35ed, 0x0000085691815a84,
	0xb99917485d954169, 0x000008549e818085,
	0x0aa9192847a4218a, 0x00000807b2019683,
	0xcc3c1987a9bb0185, 0x0000078ecd61ae79,
	0xde3b1e177ed811c6, 0x0000075beff1fc76,
	0xddad461686e4a345, 0x00000511d0e5835c,
	0xd92816184d8f6150, 0x0000086896717185,
	0xfa0219186c9af180, 0x00000870a5f1a286,
	0x0b411cc871ac91b6, 0x0000086fbc61e387,
	0x8d0421a86cc5d1fe, 0x00000862dbd23a86,
	0x0ec7262857e8a25a, 0x0000083aecc25985,
	0x3d8a4a27dcee2232, 0x000008529021545a,
	0x1977178863936166, 0x0000087b9c218a87,
	0x1a791b6885a1819f, 0x0000089eae71d089,
	0x0bed2108adb621ee, 0x000008d5c892368c,
	0x8df72908edd37261, 0x0000092be952bd90,
	0x5e7c2e9960e892d3, 0x000009c4e722fd99,
	0x590b1589e6e6a30b, 0x0000086894016a85,
	0x79d119387898317e, 0x00000898a291ab88,
	0x2afe1e58aba8d1c6, 0x000008dcb7d2088c,
	0xfcae25e8fbc0d231, 0x00000948d6129291,
	0x6e752f7976e292cc, 0x00000a06e633189b,
	0xde3c35ea59e5033b, 0x00000afae2b37eaa,
	0xb94916d85791215a, 0x0000087d98d18286,
	0x7a351b48919db199, 0x000008c1a9b1d28a,
	0x2b8f21d8dfb0d1f5, 0x0000092bc2124a90,
	0xfd7d2b8959cc527d, 0x000009cbe4a2f998,
	0x3e4634ca21e5c323, 0x00000aece2e378a8,
	0x3dfc3d1b59e153a5, 0x0000085991615bbc,
	0x299218586c94d16f, 0x000008989e119d88,
	0x2aa11db8b2a3b1ba, 0x000008f6b142018d,
	0x0c2925d920b9622c, 0x00000987cce29495,
	0xde553199c6d872d3, 0x00000a72e4a345a0,
	0xce143a6ae2e30374, 0x00000bdbdf73dbb5,
	0x991715cc5cdda411, 0x0000086d94f16f85,
	0xd9e21a0884993186, 0x000008bba3a1be89,
	0x7b112088dea9f1e0, 0x00000936b9223590,
	0xacc62a396dc23269, 0x000009f0d7c2e49a,
	0xde3c35ea3fe4832d, 0x00000b27e20390aa,
	0x9de1403bace023c8, 0x00000cc9dc043ec3,
	0xd94d16e85991615b, 0x0000088499018586,
	0x1a331bf8a09dd1a0, 0x000008e7a961e38c,
	0x6b8223a913b0420b, 0x00000981c0f26e94,
	0xed5e2ec9c3cad2aa, 0x00000a62e23337a0,
	0x3e163a3ad2e3436d, 0x00000bdfdf63ddb5,
	0xedb145bc75dd441b, 0x0000085791215ad0,
	0x398918486a94716d, 0x000008a19d219f88,
	0xca841e28c4a261be, 0x0000091baee20b8e,
	0xebed26e951b66239, 0x000009d2c842a998,
	0x7de7335a20d2c2eb, 0x00000ae4e30375a7,
	0x6df13e6b67e113ab, 0x00000c8edce426bf,
	0x590b158d2cdaa467, 0x0000086793f16a85,
	0xf9c319b88097d180, 0x000008c4a121bb89,
	0xeacf2068eea6b1de, 0x00000955b4023491,
	0x8c4c2a1993bbf268, 0x00000a27ce82e29d,
	0x4e30374a7fd9632a, 0x00000b65e123aaae,
	0xadcf424bf3df23e5, 0x00000d27dab465c8,
	0x3933165852902154, 0x0000087c96d17b86,
	0x19f81b589c9af196, 0x000008eba4b1d78c,
	0x3b1022b91caa81fe, 0x00000991b8425c95,
	0x4c942d19d6c06293, 0x00000a7ad32315a2,
	0xfe173a1adaddd361, 0x00000bd7df83dab4,
	0x0db4455c69dd7416, 0x0000084e8f6150d0,
	0x795a17685e924160, 0x0000089799618f87,
	0x6a251ce8bc9da1ad, 0x00000916a7a1f38e,
	0x8b4024c94cad821d, 0x000009ccbb428098,
	0x9cc02f9a16c342b9, 0x00000ac5d5733fa6,
	0x6e033c5b2adf838a, 0x00000c2fde43feba,
	0x88e814bcbddc3439, 0x0000085891115a84,
	0x097918787094316e, 0x000008b59b61a389,
	0xda451e58de9fa1c2, 0x00000941a9920c90,
	0xab5b26797aaf5238, 0x00000a01bca29c9b,
	0x4cc6315a4fc442d6, 0x00000b03d5035aaa,
	0x1df63deb68dde3a2, 0x00000c63dd8414be,
	0x18fd1538438d7146, 0x0000086992916685,
	0xc98f19888895917e, 0x000008d49ca1b58a,
	0x1a551f9900a0c1d5, 0x00000966aa422093,
	0x3b5a27a9a2afc24b, 0x00000a2bbc12ae9e,
	0xeca2322a7ac2f2e6, 0x00000b2bd18362ac,
	0x5ded3e5b8ed8a3a4, 0x0000082c89f131bf,
	0xb8fd12c8268c6136, 0x000007e393412b7f,
	0x498118d7de96e133, 0x0000076699a1eb7a,
	0x69ed29872f9c0241, 0x000006b9a212f66f,
	0xda983cd676a5a35d, 0x000005dcada44a62,
	0xeb6256e582b1f4d4, 0x000004afb9e61a51,
	0x28ad136432bc96da, 0x000008328d613e83,
	0x095312f81d90d13e, 0x000007da9931387e,
	0x39fb1a67d49dc143, 0x00000749a1d21879,
	0x5a902e4708a5127d, 0x0000067cad63536c,
	0x4b7745662cb233ce, 0x00000571bce4ee5d,
	0x4c7265c502c2459a, 0x00000400c2672748,
	0xc8e61458388ba13b, 0x0000083291f14b83,
	0x79b613d801968142, 0x000007d0a061497d,
	0xbab41817c8a60155, 0x00000737acb2337a,
	0x9b613286e9b0f2ad, 0x00000643bbe3ac69,
	0x9c8e4e35e4c2343e, 0x000004ffcf859e57,
	0x4c1c739485c9765a, 0x0000083d8c61403f,
	0x29301568458f514c, 0x0000082d97715984,
	0xca2e14e7f59d0150, 0x000007c3a9015c7c,
	0xeb7a17d7b9afe16c, 0x0000074fbc020e7a,
	0xdc6e3536dfc082bb, 0x00000615ce33f267,
	0xed275555a1d604a4, 0x000004aacb662152,
	0x08d1143408c2d71b, 0x0000084c90215284,
	0x798816884f93f15e, 0x000008389dc17184,
	0xcab416e81da3d176, 0x000007b4b321737d,
	0xac5019b7a8bb8186, 0x0000078bcfa1b379,
	0xbdce33d738d7f233, 0x0000060ade140268,
	0x8cf85a958cd764c5, 0x00000442c5e6c14f,
	0x290d1578438da146, 0x0000085994c16685,
	0xc9eb18485b997174, 0x0000085ca4c19685,
	0xbb371c085caba1aa, 0x0000085cbc31da85,
	0x3d1221b85ec611f8, 0x00000869dd924286,
	0xdeb927c871eb426e, 0x0000079ddcd42788,
	0x2c636b96eed1f563, 0x000008468e214962,
	0x295716c85791615a, 0x0000086c9a217e86,
	0x4a5a1ab8769f8192, 0x00000895ac91c688,
	0x1bd520c8a9b471e7, 0x000008dfc752378c,
	0xadf229f902d29268, 0x0000095ee892d392,
	0xbe653149a9e782f2, 0x00000a52e513389f,
	0x88e714b8b1c90667, 0x0000085a91c15d84,
	0x99ab18586995e170, 0x0000088ca0119e87,
	0xead31db8a3a631ba, 0x000008dfb522028b,
	0x3c84260906be222e, 0x00000966d3a29993,
	0x1e673109a2e072da, 0x00000a56e5033a9f,
	0xce1b399ac5e37368, 0x00000bb9dff3cdb3,
	0xc92115f8498eb14c, 0x0000086e96317385,
	0xda051a68839af18b, 0x000008bba671c689,
	0x9b562158dfad71eb, 0x00000939be524690,
	0x2d3e2bc972c8827d, 0x000009fce0b3049b,
	0x5e3336ea5ee4e33d, 0x00000b58e153a5ad,
	0xedd241fbe7df43e0, 0x0000084a8ed14dc7,
	0x296417585d922160, 0x0000088b9af18f87,
	0xea651ce8aaa041ac, 0x000008f8ad31f68c,
	0x1bde257929b50223, 0x000009a2c7e29296,
	0x0dfc3209ecd322d4, 0x00000aaee3c35ea4,
	0x4dfc3d2b32e1e395, 0x00000c63dd8414bc,
	0xa8ec14dd0ddb145a, 0x0000085d92115f84,
	0x19aa190874961176, 0x000008b39fe1af89,
	0xbac71fc8dca5c1d3, 0x00000941b4122b90,
	0x6c6529e97fbca261, 0x00000a17d142e39c,
	0x5e2f375a72dd7330, 0x00000b71e0f3afae,
	0x7dc5437c0ddec3f0, 0x00000d6bd9b482cb,
	0xb91d15e8498ea14c, 0x0000087595b17585,
	0xa9f21b08959a2190, 0x000008e6a4c1d58b,
	0x2b2822e918ab31fe, 0x00000994bab26495,
	0x3ce52e79dec3f2a2, 0x00000a93d9f335a3,
	0x5e073beb05e28382, 0x00000c35de2401b9,
	0xed90497ce4dba449, 0x000008478e414ad9,
	0x595217385991715c, 0x0000089699518f87,
	0xca361d38be9e01af, 0x00000920a981fd8e,
	0x0b8026295cb0522d, 0x000009ecc0b29f9a,
	0x5d5332fa43ca62e3, 0x00000b12e12384aa,
	0x0de0405ba0e043c3, 0x00000ceedb844dc4,
	0x58dd148da7d8e49a, 0x0000085690d15984,
	0x698418c873945170, 0x000008bf9ca1ab89,
	0x3a751f88eea1a1cf, 0x00000960ada22792,
	0x1bcc2959a4b4c25b, 0x00000a47c5a2d79f,
	0x4da4371aa7cf7320, 0x00000b94e073beb1,
	0x7dbd444c2fde43fe, 0x00000d8ad9448ecd,
	0x29011558428d4145, 0x0000087093516c85,
	0xd9b01a689496f187, 0x000008ed9f91c98b,
	0xdaa821c922a4b1f0, 0x000009a1b0f24e95,
	0x0c022c39ecb82285, 0x00000a9dc8f308a4,
	0x9dcd3a7b05d28354, 0x00000c05ded3edb7,
	0xbda3474ca3dc942f, 0x0000083e8c9141d4,
	0xc92116684f8f2150, 0x0000089095618186,
	0x89d31c08ba99119e, 0x0000091ca1d1e58e,
	0x6acb23d956a6f20e, 0x000009deb3127099,
	0x6c1c2e8a2eba12a9, 0x00000ae8ca132ea8,
	0xbdc53cbb54d30379, 0x00000c57ddb40fbc,
	0x98bc13ccf1db744f, 0x0000084a8e214b83,
	0xb93b17986690c160, 0x000008b396f19588,
	0x29ea1d78e09aa1b4, 0x00000949a331fd91,
	0x9ada257985a82228, 0x00000a13b3a28a9c,
	0xfc14301a65ba32c3, 0x00000b21c8c344ab,
	0x1d853d8b8dd0938c, 0x00000c7fdd141fc0,
	0x78b011c81d883125, 0x000007eb8de11f7f,
	0x88fb1b77bd8ec166, 0x0000075891420178,
	0xe9582a4725933250, 0x000006b29803016e,
	0x59e03da66f9ae368, 0x000005d3a1745862,
	0x3a8e57f578a514e4, 0x000004a3acc62c51,
	0xeb337be427b066eb, 0x0000082488e12a39,
	0x98f21228138ba12b, 0x000007e49291297e,
	0x39481f079b92d19b, 0x0000072c96d24576,
	0x09cb3036f19992a0, 0x00000669a033716b,
	0x0a86475619a413ec, 0x0000055ead050d5c,
	0x2b6d6774eeb1d5b8, 0x000003e6bb874e47,
	0xa89712f353b93831, 0x000008238c513482,
	0x294112d7e7906124, 0x000007d497e1427e,
	0xd9ad22b77b9841cc, 0x000006fe9df28c73,
	0xfa5e3696baa1a2f5, 0x0000061baa93e966,
	0x4b5851c5bdafd47a, 0x000004dcbb95d455,
	0x8bf777d456c1c6a3, 0x0000032db7386b3c,
	0xe8cf13b82e8a1133, 0x0000080d90e13782,
	0x99a013a7e0957130, 0x000007bf9e31637d,
	0x9a2d26275f9f61f6, 0x000006d1a712d171,
	0xcb183cf683abf349, 0x000005cbb7c46562,
	0x0c5d5ce55dbe850d, 0x0000045ac7269c4e,
	0x3b6a87b3cbbf9778, 0x000008328a913632,
	0x89171458378d9141, 0x000007e796913782,
	0xfa1114a7d79b813e, 0x000007c6a751587c,
	0xdad128d74fa8c20f, 0x000006abb2930c6f,
	0xec0042e651b8e395, 0x0000057ec7f4db5e,
	0x1c936614fecfe59f, 0x000003ecc1574648,
	0x58b1138338b7c85b, 0x0000083e8e114683,
	0xa96a15583992014f, 0x0000080c9c015882,
	0x3a9515d7cda2a14e, 0x000007b8b0e16e7c,
	0x6ba6298766b5c1ec, 0x00000695c1132d6f,
	0xcd1d47b62fc8f3cb, 0x00000545d3a5335b,
	0xcc4c6e24c5ccd5f7, 0x0000036fbab80642,
	0x48e91498378b613a, 0x0000084692715684,
	0x89c2173847970164, 0x0000084ba2118684,
	0x9b071b7850a8d19c, 0x00000867b901d685,
	0xcc9e29687ac2c1fb, 0x000007e7cf439384,
	0x2d3353f7a3d7746f, 0x0000073ecb262a77,
	0x9b548a36f7c16744, 0x000008398bc13c69,
	0x192d15c8488ee14c, 0x0000085c97516e85,
	0xea2419e86b9c7184, 0x00000897a8e1bd87,
	0x9b9020a8b5b071e1, 0x00000904c2b23a8d,
	0x1da12b1936cdb271, 0x000009b5e752f797,
	0x4d0259894cdae460, 0x000008e2c4c6e291,
	0xa8bf13d896b70871, 0x0000084b8f114e83,
	0xd977176859930160, 0x000008859c819086,
	0x6a8d1d28a2a241af, 0x000008f1b051fc8c,
	0xcc28262922b8d22b, 0x0000099fcd62a095,
	0x6e543339ecd9c2e8, 0x00000ac3e37367a4,
	0x3d03597b50e173a2, 0x00000a52bda7b0ad,
	0xc8f314f83a8c013e, 0x0000086093016484,
	0x99c519987997617c, 0x000008bea201bb89,
	0xeafd20f8eaa871e1, 0x00000959b8324291,
	0xccc72c199ec1b27e, 0x00000a45d8930d9e,
	0xde1b399aafe3c35f, 0x00000bdbdf73dbb3,
	0x1cf25b4c8ddce425, 0x0000083a8c013ec8,
	0x592e16584d8f314f, 0x0000088497218086,
	0x3a171c28a89bf19f, 0x00000905a7c1ec8d,
	0x2b6f25193faee21b, 0x000009cec0328f98,
	0x7d65325a25caa2d6, 0x00000af9e2b37da8,
	0xcde1404b91e083bc, 0x00000cfadb5453c3,
	0xa8bf13ddcad864a9, 0x0000084c8f014e83,
	0xb96a182869929166, 0x000008b49b51a288,
	0x9a6a1f28e3a091c7, 0x00000958ad722291,
	0x0be029899fb5325a, 0x00000a4dc7e2e09f,
	0xddf738bab5d30330, 0x00000bc6dfc3d3b2,
	0xbda646ec77dd341c, 0x00000e12d754c7d3,
	0xb8eb14d8398bc13c, 0x0000086b92116684,
	0xc9a61a389095f182, 0x000008ee9f61c98b,
	0x8ab8224927a511f3, 0x000009b3b2d25c96,
	0x6c452e1a06bb129a, 0x00000ad1ceb331a6,
	0xedf63ddb4bda438a, 0x00000c90dce426bd,
	0x9d704d0d54da1478, 0x000008378b613ae2,
	0xb91716484a8e314b, 0x0000089395118186,
	0x49de1c78c19931a2, 0x0000092fa331f18f,
	0xcaff258971a93222, 0x00000a11b792959b,
	0xcc99326a70c012d9, 0x00000b54d4137cad,
	0x9dcf423bdcdf73dc, 0x00000d47da4473c8,
	0x58b0138e15d744c8, 0x000008498da14983,
	0x494017e86b90a162, 0x000008c397d19f89,
	0x1a0f1ec8f79c11c3, 0x00000973a6721a93,
	0x1b372899bdac924e, 0x00000a6ebb22caa1,
	0xbcd1364ad6c3b313, 0x00000bced753bdb4,
	0x9dae45fc65dd7415, 0x00000ddad824b0d1,
	0x68ce1468328a8135, 0x0000086a8fa15e84,
	0x196319989492b17a, 0x000008f59a11bc8c,
	0xfa3620f92f9e71e4, 0x000009b6a8e24096,
	0xfb5c2b3a06af0277, 0x00000ac3bd42f7a5,
	0xbce6393b31c58341, 0x00000c32d7c3edba,
	0xfd9748accedbf440, 0x0000082e89e132d7,
	0x78e815a8448c1142, 0x0000089191417586,
	0xf97d1b38be946192, 0x000009279bc1d88e,
	0x8a4e22e964a01201, 0x000009f3aa32609a,
	0x2b682d4a46b01297, 0x00000b08bd8317aa,
	0x3ccf3b0b78c51361, 0x00000c78d50405bf,
	0x1869114d08db2459, 0x000007f288f11480,
	0xe8971af7ba88b16b, 0x000007618a91f478,
	0xb8d82917308bf23f, 0x000006bf8f42ec6f,
	0x49383c267e914351, 0x000005e395f44063,
	0x39b956758898a4cb, 0x000004b39eb61352,
	0xea507a6437a1e6d2, 0x00000318a7b88d3a,
	0x189c11680f86f11b, 0x000007c68a71587f,
	0x58ca1ee7958b41a3, 0x000007318e423d76,
	0xa9252f56f9902294, 0x0000067494d3616b,
	0xd9ab46162597a3d9, 0x0000056b9e24f85c,
	0x1a5d6614fca1d5a2, 0x000003f7aa073548,
	0x3b0c92735dae0822, 0x000008188761202b,
	0x68d31267f08a7117, 0x000007a18d41917e,
	0x890b23376e8ec1e0, 0x000006fd93028d73,
	0x298a3636bc95a2f2, 0x000006209c03e267,
	0xaa405125c39fc471, 0x000004e4a8a5c755,
	0x9b2d77b45fada695, 0x00000323b6a87c3c,
	0xe87c124279ada980, 0x000008038ad12181,
	0x88fb16e7ea8e6120, 0x0000077d9121c87b,
	0x895f27c74593521e, 0x000006c59912e470,
	0x4a0a3db67a9ca358, 0x000005c4a5246f62,
	0xdafd5d3558aa3516, 0x00000451b5d6aa4d,
	0x0b5a8983b2bc379f, 0x00000259abf9b131,
	0x88b212b823882128, 0x000007e98f212281,
	0x893e19f7e393112b, 0x0000075a9621fe79,
	0x79ca2c771c99225e, 0x0000068ba0b33d6d,
	0x4aab457635a563c1, 0x00000565b0a5015d,
	0x8be569f4e7b725c3, 0x000003bdbee78d45,
	0x7abe9b4319b6288b, 0x0000082688812a25,
	0x28fb1278248b7132, 0x000007e293f12d7f,
	0xe9981c77db98a137, 0x0000073a9c922f77,
	0x8a503106f5a0729a, 0x00000653aa53936a,
	0x4b714d15f2b05428, 0x00000506bea59458,
	0x4c0f751476c6d672, 0x0000033cb808543e,
	0x888d12c275ad7986, 0x0000082d8bc13782,
	0x794113e8208f913c, 0x000007da99813980,
	0x0a0d1dd7d29f1146, 0x00000723a4b25377,
	0x1af634d6d6a992ca, 0x00000622b603df68,
	0xbc605425b6bd9484, 0x0000055ac9665c53,
	0x9b5d893538c06761, 0x000004d5a8ea0a50,
	0x38c013a82989112d, 0x000008358fb14683,
	0x498a16783a93e155, 0x000008549e017e84,
	0x1a8421086ba4219b, 0x0000081bab72c185,
	0x1b6b40d7fbb08361, 0x000007c6bdc4cf7e,
	0x1c7269c7a9c5a5ac, 0x00000783be479f79,
	0x8a66a5476cb388d6, 0x0000082a89312e74,
	0x48fb14d8378c213d, 0x0000085693d16184,
	0xe9db19886f98617a, 0x000008b3a3b1ba88,
	0x3b262118dfaa91e3, 0x00000914b822a391,
	0xdc2e4668e7bc2393, 0x000008d9cab54c8d,
	0x7c157458e3ca763e, 0x00000903b658848f,
	0xb89512f8fda8ca0e, 0x0000083b8c413f82,
	0xb93a16a8508fb152, 0x0000088c98218786,
	0x3a331cf8b49d51a8, 0x0000091aa9e1fc8e,
	0x3ba426a95ab1922f, 0x000009f7c442ae9a,
	0x8d4b430a58cf82fb, 0x00000a33d2255ea3,
	0xfbca7cea51c8267f, 0x00000a79ae696ba6,
	0xe8c314082b89512f, 0x000008598f915683,
	0x397b19087b936171, 0x000008d29cb1b48a,
	0x6a8f20d908a261dd, 0x0000098eb0624494,
	0xec272c99e0b8d282, 0x00000aabcd531ba3,
	0xcdfe3ceb25d99377, 0x00000c76dd341cbb,
	0x3b78863bf7c86679, 0x0000082b89512fbe,
	0x08f41598408c1140, 0x0000088792f17586,
	0x79be1bb8b4972196, 0x00000923a161e78e,
	0x4aeb250966a7a218, 0x00000a0cb6c2909b,
	0x4ca432ca71bff2d9, 0x00000b67d5e38aae,
	0x2dc243bc01dee3eb, 0x00000d9bd90496cc,
	0xa89312ed76ca2645, 0x000008418bd14082,
	0x09261778658ee15a, 0x000008c096519989,
	0x6a001eb8f79ae1c0, 0x0000097da5f21d93,
	0x9b432969ceaca256, 0x00000a92bcd2dfa2,
	0xed1538fb08c67332, 0x00000c26dd53f7b8,
	0x1d884a5ce7dba44b, 0x00000e5bd644e5dc,
	0x28b814082989012d, 0x0000086a8e615a84,
	0x895619a89691a178, 0x000009019991c18c,
	0xaa3f21d9419e71ec, 0x000009dcaa225598,
	0x2b912dba39b12294, 0x00000b19c1f32caa,
	0x5d6c3ebb9ecbd386, 0x00000ce8db944bc3,
	0xbd644e5dbbd894a3, 0x0000082788b12be5,
	0xc8db1598428b113f, 0x0000089a90b17786,
	0x79811bf8cd943199, 0x000009479c91e990,
	0x1a7424f98fa1a219, 0x00000a3cada28c9e,
	0x7bcd31caa3b4d2cf, 0x00000b99c5b371b1,
	0xcda043ac2acf73d1, 0x00000d91d93491cc,
	0x688512ae5bd644e5, 0x000008438a813d82,
	0xb8fb17586d8cf158, 0x000008ce92e19689,
	0x69a81e39079671ba, 0x0000098d9f021094,
	0x4a9c27c9dca41243, 0x00000a96b032bca3,
	0xfbf0352b04b74303, 0x00000c08c783aab7,
	0x7da4472ca0d0b40a, 0x00000e0dd764c5d4,
	0x389d13b82587e128, 0x0000086d8c115584,
	0xc91619089b8e9171, 0x000009039491b38c,
	0x39c32049409831d9, 0x000009cea0b23398,
	0xdab42a2a21a5b268, 0x00000ae3b162e3a7,
	0x2bf5379b55b8132a, 0x00000c5dc723cfbd,
	0x3d7248ccf3cf342c, 0x000007f3846113d9,
	0x084b1927c8845154, 0x000007768551d37a,
	0x486f26974786121b, 0x000006dc8802c171,
	0x58a839069c893323, 0x000006068c040a65,
	0xb8f852a5ad8db492, 0x000004dd9195d354,
	0xb96076046293b690, 0x000003469868453d,
	0x885210b2a59aa93e, 0x000007cf85514a7f,
	0x88681d17a485c18d, 0x0000074787821b77,
	0x589f2cb71188a26e, 0x000006928b73326d,
	0x18f242a6468d33a7, 0x000005919144bd5f,
	0xd96561c52693a563, 0x000004279936ec4a,
	0x99f68d53909c47d3, 0x00000231a259f02e,
	0xa86813a7f785810c, 0x000007aa86f1837d,
	0xb89121677c87e1c9, 0x000007138a726b74,
	0xe8e03386d58c12cb, 0x0000063f9013b268,
	0x09544d85e592843c, 0x0000050e98458858,
	0xb9f472f48c9b964f, 0x00000357a3282b3f,
	0xca55a722a0a73945, 0x0000080785a1131d,
	0x48831747f0884117, 0x000007858941bd7b,
	0x98c62627528ab20b, 0x000006da8e62c471,
	0x09333b069190a333, 0x000005e396143f64,
	0x39d259857a9964e1, 0x0000047ba1466950,
	0x4aaa8613e2a5c755, 0x00000273ad598a33,
	0x185e1191aca2cabc, 0x000007f288e11481,
	0x18ad1aa7c389a15d, 0x0000075d8c71f979,
	0x490b2b47248e6251, 0x0000069c9363236e,
	0xb99e4336499673a2, 0x000005809dd4d75e,
	0xca73668507a24593, 0x000003ddaca75c47,
	0xcacf995328b28873, 0x0000019aa1dad826,
	0x289411481686111c, 0x000007e08bc1307f,
	0xe8e41e07a38c818e, 0x0000073590823776,
	0xe9633076f6932299, 0x0000065c99b3866a,
	0x3a244ba5fe9dc415, 0x0000051aa7857559,
	0xeb3a74248ead464c, 0x0000033ab7e8583e,
	0x9a29ac2280ae0975, 0x0000081986411e1a,
	0xd8ce11d80689511e, 0x000007c08ea1627e,
	0xd92c2137869051bb, 0x0000070e95b27374,
	0x89d135b6c89922e0, 0x0000061da1a3e767,
	0xdacb5405b4a6d487, 0x000004b3b3461353,
	0xaba780d415ba9706, 0x000002b1b0a92a36,
	0xb86711f3439feb10, 0x0000081989512681,
	0xa90b13580f8cc12c, 0x000007de92519380,
	0xf96d26d7ba9431fe, 0x0000078699f2e879,
	0x1a1c41476d9d9373, 0x00000732a684cb75,
	0xbb1e690711abe59d, 0x000006c0b877a96e,
	0x7a7ca2b6a1b398d5, 0x0000066d99abc568,
	0x489612c81b86a120, 0x000008318c813b82,
	0x094316784590214f, 0x0000085a96c1c286,
	0x69b82ce84098824c, 0x0000082f9f335d83,
	0x8a864b582ba383fd, 0x00000824ae058782,
	0xcbb1793821b4467a, 0x00000829b478bc81,
	0x29a7bae838a8aa12, 0x0000081c86b12084,
	0x58c614382c895130, 0x000008648fd15c84,
	0x598519a88993d179, 0x000008e99d81c18b,
	0x5a263078d89f6264, 0x000008e0a693ac8d,
	0x6b145318f1ab8462, 0x0000091fb7b61e90,
	0x9b8185293abeb731, 0x0000099cac79a396,
	0xc86c1219c69e5b3d, 0x0000083389513381,
	0xa8f81658548c314a, 0x000008a693518587,
	0x59cc1d38da97b1a9, 0x00000959a2820391,
	0xaae92b59a7a9123a, 0x000009cbb0f3b39d,
	0x4bc55779e9b6148b, 0x00000a45c33684a1,
	0x9b2d8eaa8cbe37a1, 0x00000acca60a5fad,
	0x989313581c86c121, 0x0000085f8bf14e83,
	0xc92d18d88a8f216b, 0x000008f59701b38b,
	0xfa152119359bd1df, 0x000009d4a7a24a97,
	0x2b702d6a34aed28b, 0x00000b20c0432aaa,
	0xecb5540bb0cad38b, 0x00000bb7c80684b7,
	0x6b0b929c06bc47d8, 0x0000081c86b120b9,
	0x88b915183d890136, 0x000008978ea17086,
	0x89611b98cc922192, 0x0000094c9ab1e690,
	0x2a5d25299a9fe219, 0x00000a56ac92949f,
	0xabd0334ac8b442df, 0x00000bdec6c394b4,
	0x8da3473c85d1c401, 0x00000db5d0059bd4,
	0xd869120c97be579c, 0x0000084188c13781,
	0x08e017286f8b3153, 0x000008d79151958a,
	0xb9951e99159511bc, 0x000009aa9e321b95,
	0xaaa0295a04a3b255, 0x00000adcb112dfa6,
	0x2c22390b5fb92332, 0x00000c98cc23f9bf,
	0x5d6d4d6d52d7346f, 0x00000e5bd644e5e3,
	0x488613781d866120, 0x000008738ab15484,
	0xe9061958a68d5173, 0x0000091d93d1bc8d,
	0x19c421996397d1e8, 0x00000a0aa152519b,
	0xfad82d6a6ea7128f, 0x00000b5eb4c325ad,
	0xdc5d3e2bedbce37e, 0x00000d41cfb451c8,
	0xbd644e5e0cd764c4, 0x0000081f862120e5,
	0x68a115384687f137, 0x000008a98c817287,
	0xe9281b98e18f5194, 0x000009639621e391,
	0x69eb2479b09a3212, 0x00000a66a3e282a0,
	0xbb0130fad2a9a2c5, 0x00000bd3b73362b4,
	0x1c7c424c6abf13be, 0x00000dcad0f493d1,
	0x085d120e5bd644e5, 0x0000084887813782,
	0xa8ba170877897152, 0x000008e08e31908a,
	0xd9441db91b9101b3, 0x000009a697e20795,
	0x3a0726f9f89bf238, 0x00000ab8a582aca5,
	0x6b1533cb29ab22f0, 0x00000c30b8238fba,
	0xec7444dcc8bf83ea, 0x00000e20cf44b6d6,
	0x981216c7de812133, 0x000007918141a97b,
	0x681d2367668181ec, 0x0000070082328973,
	0x083434d6c482b2e5, 0x0000063583e3c168,
	0x38574d35e184a442, 0x0000051a86557558,
	0x28886f34a587562a, 0x0000039189c7d142,
	0x78cd9cd2f38b38c5, 0x000007e281b12c24,
	0x38201a67bd81d167, 0x000007668261ec79,
	0xb83729073382e23a, 0x000006bc8422f26f,
	0x485d3dc67484f360, 0x000005c986e46862,
	0x08955b5563880504, 0x000004708ac67b4f,
	0xe8e38513df8c6759, 0x0000028c90296433,
	0xa8261211c9924a90, 0x000007c18281607e,
	0x88361e879782d1a1, 0x0000073484023876,
	0x885c2f76fa84d291, 0x0000066e86d36a6b,
	0xa89647f6198803ec, 0x0000054e8b05265b,
	0x98eb6b74d38cc5e2, 0x000003ad90f7a744,
	0xa9609e22fe9358b5, 0x0000016198cb3023,
	0x78341567f4833111, 0x0000079c83c1987c,
	0x985523176d8471e1, 0x000006fe86528c73,
	0xe88e36a6ba8782f4, 0x000006178a73f066,
	0x38e45355b48c3488, 0x000004c39095fb54,
	0xd9607d84329326da, 0x000002d39938f638,
	0xd9b3b992019c9a3a, 0x000007fa83d10811,
	0x484b18c7d0841149, 0x000007758591d47a,
	0x687f28074186a225, 0x000006c28962e870,
	0xc8d03e76758b135f, 0x000005b88f448161,
	0x294a5fd54591c532, 0x0000042c97e6e34c,
	0x09f89143829b87e9, 0x000001e3a3ca672c,
	0xa84110910099abc6, 0x000007db8501387f,
	0x086c1c37ae85b17c, 0x0000074d88121278,
	0xf8b62d471289926d, 0x000006838d634a6c,
	0x692746b62b8fc3d1, 0x0000055395851c5c,
	0x99cf6cf4d09905e7, 0x0000038ca157d943,
	0xca62a5a2c7a63909, 0x0000010499ebbf1e,
	0xd86411c80784110f, 0x000007bc86d1677e,
	0xd8991fa78f8811ad, 0x000007248b625275,
	0x88fd32a6e28d62b7, 0x000006429293ad69,
	0xe9954f35e095c445, 0x000004ed9d85b956,
	0xda757a7458a2169f, 0x000002e7ad08d73a,
	0x6972c0d214a85a1c, 0x0000080c84211227,
	0x08841487f586f10f, 0x000007a28981907d,
	0xe8d22317718b31db, 0x0000074c8e72b073,
	0x392b3cd73b907335, 0x0000070795647972,
	0xf9be6256e5987540, 0x000006959fc72d6b,
	0x0a8a9c0664a4185e, 0x000006109e6b3c63,
	0xe8441135fa8f2cf5, 0x0000081386b11c80,
	0x28a218682589712c, 0x000008088b41e281,
	0xc8ea2b98018cd247, 0x000007f790d33b7f,
	0xe96247d7f29353d1, 0x000007e89965447e,
	0xba1372f7e29d1628, 0x000007d3a5b8617d,
	0x89e6b3b7caaa99c4, 0x000007e68f3cf57d,
	0x586912480f845113, 0x0000084489213a82,
	0xa8d41ae8688c1153, 0x000008688e721e86,
	0x992731486f904293, 0x000008879503a787,
	0xb9b751189898144f, 0x000008c19f65ef8a,
	0x4a8881a8d9a3b6f0, 0x00000910ada9748f,
	0x392dc8b945a17ae2, 0x0000080f84611496,
	0x888e142832867129, 0x000008838bb15e85,
	0xd92b1a38b48ef17e, 0x000008e89352388e,
	0x297b3558f89522c3, 0x000009339ac3f891,
	0x7a2658795a9e54b2, 0x000009baa6f67c98,
	0xdb148e39f2abf798, 0x00000a81a6ea45a2,
	0x1846115a039b5b94, 0x0000083b86512d81,
	0x78b516686788a148, 0x000008cd8e718889,
	0x19621db90b9211af, 0x000009a29ae20e95,
	0x29f73579d69e3285, 0x00000a10a2a4149e,
	0x2ab25cba4ca694e2, 0x00000adfb026d7a9,
	0x7af0958b32b58810, 0x00000abda53a75b7,
	0x2863130814846116, 0x0000087288614d84,
	0x08de18f8a68af16c, 0x000009219151b78e,
	0xf99b21896b9531e4, 0x00000a1f9ed2529b,
	0x8ab42e2a8ca4a295, 0x00000b96b2e339b0,
	0x1b6e5a9ba1b37483, 0x00000c37bb56e1be,
	0x2b0792fc46ba1818, 0x00000818844118b9,
	0xb88115084985f132, 0x000008b18a716f87,
	0xf9071bb8ed8d4193, 0x0000097b9421e892,
	0x29d12569d198521c, 0x00000aa0a28299a3,
	0xbafa339b1da8b2e4, 0x00000c4cb7839aba,
	0xcca5481d01c06407, 0x00000d90cb861edc,
	0xd842119c95be37a0, 0x0000084d85c13481,
	0x889f17188187b151, 0x000008f48c81948b,
	0x492e1e99388f81bc, 0x000009d996c21b98,
	0x5a02294a399b2254, 0x00000b20a5c2dcaa,
	0x5b3338abaaac132e, 0x00000cf4bb43f2c4,
	0x6ce74d3db8c41465, 0x00000e5bd644e5e6,
	0x185713582183f11a, 0x0000088587415285,
	0x88bc1948bc895171, 0x0000093a8e81ba8f,
	0x79522159849191e5, 0x00000a3499124b9d,
	0x2a2a2cda9d9d9289, 0x00000b95a8431ab1,
	0xdb5b3cfc28aea370, 0x00000d83bd843acc,
	0x1d1e4dbe4bc604ae, 0x0000082583b11ce6,
	0x786c151854852135, 0x000008bc88b16f88,
	0x78d51b58f68ae190, 0x0000097d9021de93,
	0x496e23e9cc93520b, 0x00000a879ad277a2,
	0xfa442fcaf59f52b6, 0x00000bf6a9c34bb6,
	0x2b6a401c8caff3a1, 0x00000de6bdf469d3,
	0x67e6140e72c674c1, 0x000007b07e31797d,
	0xb7de1fd7877e11b8, 0x000007297dc24a75,
	0x37da3006f17db2a0, 0x0000066d7d836c6b,
	0x67d646c61e7d73e4, 0x000005647d55035c,
	0xb7d56694f67d55ac, 0x000003f37d773b47,
	0x67e192335b7da825, 0x000002037eda372b,
	0x37eb1767d97ee13b, 0x000007887ea1b77b,
	0x57e924f75a7e91ff, 0x000006eb7e92a972,
	0xe7eb3816a97ea30f, 0x0000060a7ec40365,
	0x17f15395ac7ee495, 0x000004c97f45f254,
	0xb7fe7aa4427f86c2, 0x000003028068ad3a,
	0xc823b062488119cc, 0x000007dd7f613417,
	0xb7f41b37b67f5171, 0x0000075b7f51fd78,
	0x97fa2ac7267f724f, 0x000006a47fd3166e,
	0xe80641665680138e, 0x0000059a80c4b05f,
	0x981b62452981355f, 0x000004178257034a,
	0xc83f9193748307ff, 0x000001ef851a552b,
	0x37ff12c11b876b9c, 0x000007bb7fe1697e,
	0xf8031f678f8001ac, 0x0000072980724a75,
	0x58143146eb80d2a9, 0x0000065581b3906a,
	0x282f4bc5fa82541c, 0x0000051c83b57159,
	0xd85a72c496849640, 0x0000035086e8353f,
	0x089fab628d884963, 0x0000010d907bb11b,
	0x18081607e9808122, 0x0000079680c1a27c,
	0x081923f7668121ec, 0x000006f282229e73,
	0xb8393876ac82c30b, 0x000005fe84741665,
	0xc86a5725958574b8, 0x0000049287e64751,
	0x18b484d3f4897739, 0x000002758d598634,
	0x198cbc418e8fbaeb, 0x000007f081111710,
	0xe8191967c8813155, 0x0000076f8221dd79,
	0xd83928d73a82d22f, 0x000006b78482f96f,
	0xb86c402667858375, 0x000005a18824a560,
	0xd8ba63552889c55f, 0x000003fe8db72a49,
	0x892e982348902842, 0x0000018b960af027,
	0xa81b10910099abc6, 0x000007d181e1487f,
	0xb8341cb7a9828185, 0x0000074884121a77,
	0x98652de70d852275, 0x0000067a87b3576c,
	0x88b14826208943e2, 0x000005418d25395b,
	0xa9246fe4b88f960b, 0x0000036695681441,
	0x49b8ae929698e954, 0x00000310921ca120,
	0xc82c1367fc821104, 0x000007b68371717d,
	0xa8581ff78b8461b3, 0x0000074786426675,
	0x587a37274586c2e5, 0x0000071e88b41173,
	0x18b559c70289f4c9, 0x000006bb8ce68f6e,
	0x190b8ec6908eb7a7, 0x0000062e92ca6466,
	0x5877dd55fa94fc18, 0x000007ff8221065f,
	0xb83e16b802839123, 0x000007f88471ba7f,
	0x385f27b7f5852214, 0x000007f086d2f27f,
	0xa89141b7ed87e37c, 0x000007e68a74d27e,
	0xd8dd6977e28c05a4, 0x000007d78fc7b17d,
	0xb946a6d7d19208f5, 0x000007c3967c227c,
	0x78221097d7865df7, 0x0000082a83e11e80,
	0x18571a383f85314c, 0x000008498621fd84,
	0xf8802d8853870263, 0x0000086d89335f85,
	0xf8c24ac87d8a93fa, 0x000008a58de57a88,
	0x89237798bc8ff667, 0x000008f694b8b68d,
	0x8994bd0916976a26, 0x000008db8bad5c93,
	0xb83c12680e82210c, 0x0000086685b13f83,
	0xc8831c489588015b, 0x000008ab88e23089,
	0xe8b33268c289e2a4, 0x000008ff8cb3bb8d,
	0xf90752a9258e8465, 0x0000098092c60c94,
	0x398283e9b6955711, 0x00000a359b299d9f,
	0xc942c64a4b9edb25, 0x0000081482110f97,
	0x785814684783a12b, 0x000008ab87b16487,
	0x88d31ae8e58a4187, 0x0000093b8d923392,
	0x18fd3549578e62c1, 0x000009b491b3f798,
	0x59665849f093d4b1, 0x00000a82993678a3,
	0x39f88dbad79c4792, 0x00000af7a50a1db3,
	0xb821112a349dfb49, 0x0000085083812f81,
	0xb87616b88385414b, 0x000008f989d18e8b,
	0xe8fe1e593f8ca1b7, 0x000009e993921998,
	0x098231ea5197d255, 0x00000aa59943e9a7,
	0xb9e659faf19b94b8, 0x00000bafa186a6b4,
	0xdaaf8ecc1ca4d7d9, 0x00000b02a8da0cbb,
	0x783513282281f115, 0x0000088d85114f85,
	0x68961938c787116f, 0x0000094e8c01bc90,
	0xc92821e99f8f01ea, 0x00000a6496725a9f,
	0x1a022eaadb9b029d, 0x00000bfaa60341b6,
	0x6ab9510ca7acb3a4, 0x00000d17acd663cc,
	0x9b528a8caeb1c77a, 0x0000082881e118be,
	0x484d15285d833134, 0x000008ce86b17289,
	0x68b41be90e88d195, 0x000009a78e21eb95,
	0xa950258a0391521e, 0x00000ade99229aa6,
	0x7a34337b629de2e4, 0x00000c9ea95396bf,
	0xcb7e477d5ab03400, 0x00000e77c2d4b9e2,
	0xe81c11ad1fbf16cc, 0x0000086183013582,
	0x2864172898848152, 0x000009118841958d,
	0x78d21e89588a91bc, 0x00000a009012199a,
	0x397328fa63937250, 0x00000b519b72d5ad,
	0xca5b37cbdea04323, 0x00000d2cabd3dec7,
	0x2bb34a8defb2a44c, 0x00000e71c734c3e8,
	0x482d13683281a11c, 0x0000089984315286,
	0xf87a1928d285d171, 0x0000095389c1b890,
	0x38ec21099f8c11e1, 0x00000a5291c2439f,
	0x098e2beaba95227d, 0x00000bb29d3306b3,
	0x4a743b0c44a1f357, 0x00000d96ad3413ce,
	0xfbd54ade56b3d47e, 0x000007cf7d614be7,
	0x27a31c17aa7b7182, 0x000007537a220978,
	0x77932af72079b257, 0x000006a778a3126e,
	0xf7743fc65f77f380, 0x000005b576748760,
	0xe74a5d154f759523, 0x0000046173b6924d,
	0x971e8593d572c76a, 0x0000029071595e33,
	0x27d91461da715a76, 0x000007ad7b917f7d,
	0x27ab20a7827ae1c1, 0x0000071c7a425d75,
	0xc7943226e079d2ba, 0x0000065078b39869,
	0x87754b35f978041d, 0x0000052b76955b59,
	0x574d6ef4af75a61a, 0x0000038a73f7dd42,
	0xe72aa0d2dd7338e7, 0x0000014e72cb4e21,
	0x07bc17a7d67dc13f, 0x000007847b91be7b,
	0xc7b125e7537b6209, 0x000006de7aa2be71,
	0x779c3a66977a332a, 0x000005ec79343264,
	0x177f5835857894d0, 0x0000048d77464f51,
	0x075e8363f8769734, 0x0000029375595835,
	0xe79db7e1c1750a9c, 0x000007db7e113712,
	0x87c61b77b47c4173, 0x000007577c320378,
	0x07bb2bb71f7bf259, 0x000006977b632a6e,
	0x77ab43a6457b13a9, 0x0000057c7a44de5e,
	0x879766e50379e599, 0x000003db79076047,
	0x07869a7329789872, 0x0000017e786b0326,
	0x17e612f123818b8f, 0x000007ba7d216b7e,
	0xd7d11f978e7d21ae, 0x000007257cf25075,
	0xd7cc3216e67cd2b2, 0x0000064a7ca3a269,
	0xe7c64dc5ea7c7434, 0x000005027c459b57,
	0x17c176f4737c2676, 0x000003187c188c3d,
	0x97cdb3c2467c49cf, 0x00000119889b9f15,
	0x07de1617e77eb125, 0x000007967e01a27c,
	0xf7e12407667e01ec, 0x000006f07e32a172,
	0x57e638f6a87e4310, 0x000005f77e942165,
	0xd7f05895897ec4c9, 0x0000047d7f566650,
	0xe8048833da7fb762, 0x000002488109cd31,
	0x08edbad154821b45, 0x000007ed7f011c11,
	0x07ee1937c87eb154, 0x000007727f11da7a,
	0xf7fa28a73d7f522b, 0x000006b98002f66f,
	0xb80e402668807372, 0x000005a08174a760,
	0x882f63d525822564, 0x000003f583d73849,
	0x381da2533a84f857, 0x0000037c80bbf43a,
	0x57f611040988ecf8, 0x000007d27f81457f,
	0x08041c37ac7fd180, 0x0000077a80122378,
	0x97fb3137767fd294, 0x000007577f93a476,
	0x77f45097417f6449, 0x0000070a7f25e572,
	0x37ec8086e97ef6e2, 0x000006997e99596c,
	0xf7e0ca466c7e5ae0, 0x00000682838e0a63,
	0xf7ff13e800800100, 0x000007ff7ff1847f,
	0xf7ff2327ff7ff1d4, 0x000007ff7ff29d7f,
	0xf7ff3a97ff7ff31a, 0x000007ff7ff44e7f,
	0xf7ff5e97ff7ff50d, 0x000007ff7ff6e77f,
	0xf7ff95e7ff7ff80c, 0x000007ff7ffae67f,
	0xf7ffeae7ff7ffcab, 0x000008137fe1087f,
	0xa80d171834810126, 0x0000084480d1c283,
	0xc81028784f80f21e, 0x0000086c81230185,
	0x181842f87d81538e, 0x000008a781b4e989,
	0xe8236b78c181f5bf, 0x000008fe8287d58d,
	0xa833a9d92382d91f, 0x00000973838c5994,
	0xd7fd10d88286ede5, 0x0000084b81312681,
	0x782919587982c13f, 0x000008998291f488,
	0xc82f2d28b082b25c, 0x000008ec8343598c,
	0xa8424a591083b3f3, 0x0000096a84a57293,
	0xd85e76e9a085465d, 0x00000a2086a8a89d,
	0x38a7b7ba69877a15, 0x000009258f9ceaa1,
	0x881112b8257fd110, 0x0000088b82914785,
	0x18651888c2845165, 0x0000091285c20490,
	0xa86130593085b281, 0x0000098b86939895,
	0x68825009c5875440, 0x00000a518905dca0,
	0x18b4800aa58a26da, 0x00000b658c7955b0,
	0x396dbc6abf916a74, 0x0000082d7fc1149e,
	0x882714d863810130, 0x000008d384116c89,
	0x08841b9915860190, 0x000009b58ad1e796,
	0xe8bc2f5a158dc21c, 0x00000a5a8c13a1a1,
	0xd8e0527aa58cf459, 0x00000b618f6611af,
	0x492d84dbd090d720, 0x00000b81984948c2,
	0x47fd117ab79dba7f, 0x0000086a80f13383,
	0xf83e1718a2825151, 0x0000092285b1958d,
	0x58a31ed96f87d1be, 0x00000a278d02229c,
	0x493c2a3a9690225e, 0x00000ba497e2f0b1,
	0x09804d1c089963a9, 0x00000ca898c5e2c4,
	0x29f77eecfc9a7701, 0x00000ba1a43917c6,
	0x080e13683a7fc119, 0x000008a982215387,
	0xa8561988e683a174, 0x000009768751c192,
	0xd8c22229cc8991ee, 0x00000a9a8f125da2,
	0x29642eab1692729f, 0x00000c3f9aa33fba,
	0x8a5340bcf19f939f, 0x00000e95abb483db,
	0x4ab9770da8aae5f8, 0x0000083f7fc11ccb,
	0xd82015587480d137, 0x000008e98361748a,
	0x686d1bf92c850198, 0x000009ca88e1ec97,
	0x18e0255a288b421d, 0x00000b08910295a9,
	0x298732cb8d9482dc, 0x00000cc99ce385c2,
	0x0a7c458d82a1f3e9, 0x00000e92b0a490e5,
	0x37fb11ee81bbe4aa, 0x0000087680c13884,
	0x88321738ad81e155, 0x0000092984a1958e,
	0x08831e59708651bb, 0x00000a198a62149c,
	0xb8f9283a7c8cd248, 0x00000b6792b2c5ae,
	0xa9a2360bf196330e, 0x00000d339ea3bbc8,
	0xca9d481deca3b41e, 0x00000e8eb3a497e9,
	0x37b118e7c77d0156, 0x0000077b78f1cc7a,
	0xe74025b74f76a20f, 0x000006e37382b571,
	0x972138b6a273131a, 0x0000060770e40865,
	0x66df5315ac6f8494, 0x000004d46c45e154,
	0x868a77d4556a76a4, 0x0000032d66d86c3c,
	0xd64ba8a283656972, 0x000007cb7d31511c,
	0xc7901cb7a57b318b, 0x0000074d76821277,
	0xb7452c171a73d261, 0x0000069673932c6d,
	0x171542a6487293a3, 0x0000058f6fe4c15f,
	0x46c862b5206e456c, 0x000004186aa7024a,
	0xe66c8ff37b68a7f3, 0x0000020e653a262c,
	0xf7d614b14d652b4f, 0x000007a87b51867c,
	0xf76a20f77e7921c7, 0x0000071875626374,
	0x07473356d87532c7, 0x0000063f7363b269,
	0xb70c4e05e372243f, 0x000005056f259557,
	0x96b874a4806d6663, 0x0000034069984e3e,
	0xf661ab828267b973, 0x000001436bdb5e1a,
	0xd7b917f7d37da143, 0x000007837961c07a,
	0xa76c26175376d208, 0x000006d97652c471,
	0xc7483b6690758335, 0x000005dd73644963,
	0x67095ac5717214ef, 0x0000046a6f06834f,
	0x86b788c3cc6d3778, 0x0000024e69b9c431,
	0xa723b6d16b683b21, 0x000007d97df13b13,
	0x979b1b57b37bf175, 0x0000075a77b1fe78,
	0xf7792bb720781258, 0x0000069576d32d6d,
	0x075044464075f3b0, 0x0000057273e4ed5e,
	0x471368d4f47295b0, 0x000003c16fc78946,
	0x26cb9ee3066e38a8, 0x000001446b8b5d23,
	0xf7e4131131783b7a, 0x000007bb7c51697d,
	0x27981f27927a31a8, 0x0000072a79624976,
	0x078631c6ea78f2ab, 0x0000064c77c39d6a,
	0xe7624db5ec76f432, 0x000004ff75259e57,
	0x872f77d46e74167d, 0x0000030b71c8a03c,
	0xe6ffb6623370b9ed, 0x000001297d8b8613,
	0x37cd15c7e67ea126, 0x0000079d7ac1977c,
	0x77ad23476d7ae1e1, 0x000006f97a829473,
	0xf79c3816b17a2302, 0x0000060079441265,
	0x97685a759478b4b9, 0x0000056d7436b458,
	0x26fa9325347207de, 0x000004a76d1aba4f,
	0x773bd2f45e6a6c7e, 0x000007ef7f211950,
	0xd7bf1867cf7d614b, 0x000007b47b81e27a,
	0x37a12b47ad7ae243, 0x000007967933347a,
	0x676e4737887823c9, 0x0000076275953877,
	0x272371e74b74061a, 0x0000071470384a73,
	0x36bab336f56e09a3, 0x000006af692cfe6d,
	0xe7f910c72e6f3e1d, 0x000008107d614b80,
	0x47be1e58127c4192, 0x000008177b524581,
	0xd79d3348197aa2b4, 0x0000082178d3c881,
	0xc76853682677c472, 0x0000083375261882,
	0x271c84783a73971b, 0x0000084b6fc9a184,
	0xf6b6cfc8556dab30, 0x0000081a6e0e8685,
	0x17e41328237f710e, 0x000008497d717f84,
	0x37c72308557d01d2, 0x000008747bd29d86,
	0xc7a63aa8877b331a, 0x000008b579845089,
	0x07765eb8d178850f, 0x000009157616e88f,
	0x973095e93d74a80c, 0x0000099a714ae396,
	0xb736e0996c70cc7d, 0x0000082f7f511386,
	0x07fe14185f7eb128, 0x000008977ec1a789,
	0x77da2728ac7e3207, 0x000008e67d22eb8c,
	0x37be41490a7c9375, 0x000009627b34ca93,
	0x579868d9987a659c, 0x00000a197877a39d,
	0x5761a58a647758e4, 0x00000a00788b99ab,
	0x87f31179107bcd0a, 0x0000086c7ee12e83,
	0xc8131688a17fe149, 0x0000091f82b18b8d,
	0x880029c92380a222, 0x000009767f832394,
	0xa7eb4659ac7f13ba, 0x00000a327e45299e,
	0xe7d370ea837dc60a, 0x00000b427c9839ad,
	0xd805a90b647cf976, 0x000009cf83fbe4aa,
	0x67ef13383f7f111a, 0x000008ae7fe14f87,
	0x28281938ec81216f, 0x000009818431bc93,
	0x785427a9db8611eb, 0x00000a35840324a0,
	0xb835486a798393cd, 0x00000b2b832558ac,
	0xc82d75ab95830647, 0x00000c13845868c0,
	0x48c0a9db6e881965, 0x000008467ef11daa,
	0x87fe15487e7f0137, 0x000008f88101748b,
	0xf83f1c293e826199, 0x000009ea85c1f198,
	0x68a4263a5187d226, 0x00000b4b8d02a8ac,
	0x08c7426be19032f6, 0x00000c608b951fc0,
	0x48ca727cd68b3629, 0x00000c4b904811ce,
	0xc7ee11fb8c93c937, 0x000008847f013984,
	0xe80f1778be7fe156, 0x0000094582319b8f,
	0xf8561f299583b1c4, 0x00000a548762269e,
	0x98c22a4ac7899261, 0x00000bdb8f12f0b4,
	0x99663a5c80927345, 0x00000e089ad411d3,
	0xf9b3650ea9a1146d, 0x00000c949c07a1d6,
	0x77f013b8507ed121, 0x000008c17fd15888,
	0x682119b90080e178, 0x000009948371c394,
	0xd86d2219eb8501ef, 0x00000abb88e25aa4,
	0x18df2e1b368b4299, 0x00000c5d910331bc,
	0xb98a3efd0a94938a, 0x00000ea09d545edc,
	0x2b08490ea1a62478, 0x000008547ed123e9,
	0x17fd1588897f113c, 0x000008fe80d1778c,
	0xc8331be94181e198, 0x000009de84a1e898,
	0x288324ca3b865218, 0x00000b158a5287aa,
	0x58f9313b968cc2c9, 0x00000cc492b365c2,
	0x29a8424d739653c0, 0x00000ea9a0a46ce3,
	0x17cb160e9ca9b480, 0x0000079d7ac1987c,
	0x876421a77578a1d5, 0x0000071773a26674,
	0x26d731a6df70b2bb, 0x0000065c69c3856a,
	0x066d48f60f65a3fc, 0x0000054a65652c5b,
	0xa61169c4d86365da, 0x000003cf5ea77245,
	0xe5a39603365c385e, 0x000001dc592a7228,
	0xf7ad1957c47cd15b, 0x0000077578a1d579,
	0x373626c74776321c, 0x000006d97052c571,
	0xd68f39c6976ce32a, 0x000005f568142464,
	0x26555685916734be, 0x000004a763062752,
	0x15da7ea41c6076fc, 0x000002d65af8f238,
	0x0583b32219589a15, 0x000007c77d015616,
	0x778c1d17a27b0190, 0x0000074876421a77,
	0x67032c971373626c, 0x000006926c93326d,
	0x569843d6446873aa, 0x0000057c6814de5e,
	0x263765f50665f593, 0x000003ed60b74548,
	0x95ae9683455dc847, 0x000001ba584aa828,
	0xc7d414e1585d8b3e, 0x000007a67b41897c,
	0xc76721377c7901ca, 0x0000071673a26774,
	0x36ca3316d97052c5, 0x0000063d6c13b469,
	0x16964ef5de6b2448, 0x000004f76735ab57,
	0xe61f77446b64b682, 0x0000031d5ef8853c,
	0x4595b122545c09b9, 0x0000015062cb4a17,
	0xd7b917f7d27d9145, 0x000007837961c07a,
	0xd74025c75376d208, 0x000006e070b2ba71,
	0x06e53b06986df32a, 0x000005df6d144564,
	0x46935b05716b44ee, 0x0000046566c68c4f,
	0xa6138a23c2641786, 0x000002395e59e430,
	0x967cb5515a5c6b3b, 0x000007d97df13b14,
	0xc79d1b27b57c0173, 0x0000075d7761f978,
	0xb7152a972874924b, 0x000006a071631c6e,
	0xb6f543364b70939f, 0x0000057d6da4dd5e,
	0xf69867d4ff6bb59f, 0x000003ca67177a46,
	0x961f9e330f64889b, 0x00000154605b4423,
	0x17e612f1426c6b5f, 0x000007be7c81647e,
	0xa7811e57977a71a0, 0x0000073775623476,
	0x173e3036fa73d293, 0x000006c37193ac6b,
	0xb6ce5216ad6f545c, 0x000006606a160268,
	0x563682b62d66f703, 0x000005b65f897d5f,
	0x056dcc45735b3b03, 0x000005e85f4d5a53,
	0x47cd15b7eb7ee11f, 0x000007e17a51a37e,
	0x97442537dd7781f5, 0x000007d073e2c77d,
	0xd70f3e17c772a34b, 0x000007b36ee48e7b,
	0x869c63a7a66c8555, 0x0000078866974179,
	0x25f39ca77663186f, 0x0000074e5afb5976,
	0xc5b8e2c736569d24, 0x0000081d7f41147c,
	0x77a819e8217d0156, 0x0000082e77c1ef82,
	0xb7542c283575f250, 0x0000084373e34483,
	0x770348784c7223db, 0x000008636df54c85,
	0x16867368716b6630, 0x0000089365186388,
	0xc5d6b4d8a76169be, 0x000008d0594d188b,
	0x37f41158345c5e5e, 0x000008577e313483,
	0xf7891d785f7b6185, 0x0000087f78023986,
	0x975b32889376f2a9, 0x000008c37423ba8a,
	0x37055228e1726461, 0x000009296e060090,
	0x76878249566b66ff, 0x000009bd65297698,
	0x6609c6d9f7619afb, 0x00000879617df397,
	0x17e612f83f7f111a, 0x000008a67d914587,
	0x87a520e8b27b41a9, 0x000008e679327a8c,
	0x076a3829087802f6, 0x0000095e75242293,
	0xf7185ad9937374da, 0x00000a126f56a09c,
	0x26a18f9a5f6ce7b7, 0x00000ad367fa54ab,
	0xf6a7cf4a0c68fb87, 0x000008497ef11e91,
	0x87d514c87e7e2134, 0x000008f67e216b8b,
	0x37ca22693c7f318e, 0x0000096b7b42a494,
	0x678c3c599c7a032c, 0x00000a1a7754729d,
	0xe74261aa6875e537, 0x00000b2072371eab,
	0xd6fd968b8c70184a, 0x00000ab8719a7fb6,
	0x17ed1219dd739bcf, 0x000008897e413985,
	0x57e61738c47dc153, 0x0000094e7f519790,
	0xe81c1ef9a08071c0, 0x00000a3280627a9f,
	0x87cd3d5a607e332a, 0x00000aff7b8490aa,
	0x478d652b647a3563, 0x00000c50774766bd,
	0xa7a5954c1c78185a, 0x00000ab17caa89b7,
	0x17e613d8577ec124, 0x000008cd7e015889,
	0xa7f619c90f7e9178, 0x000009ad8071c595,
	0x683322aa0b81b1f4, 0x00000af084e266a7,
	0x58922f9b7986d2ab, 0x00000c3784442cc1,
	0x5807633c9b822528, 0x00000ced815718d1,
	0x7859926c56838801, 0x0000085d7ec126b9,
	0x37e215b8967e6140, 0x000009157ea17b8d,
	0x28071c895f7f719f, 0x00000a0e81a1f69b,
	0xe849265a7883022a, 0x00000b738672a7ae,
	0x38b0347c0a8892f2, 0x00000d718df3a6cb,
	0x7971456e45915411, 0x00000d748f6649eb,
	0x07eb128c9b8f0796, 0x000008997e714186,
	0x67ec17b8d57e415c, 0x0000095e7f819e91,
	0x88181f29ae8071c6, 0x00000a6d82d224a0,
	0xd86029cade84525c, 0x00000beb8802e2b5,
	0xb8cf38bc8b8a4332, 0x00000e009003eed3,
	0x09c1462ebc942450, 0x00000ea2a56477eb,
	0xa7e71428637eb129, 0x000008d47e615c89,
	0x87f919c9127ed17a, 0x000009a48061c195,
	0x882a21a9f98171eb, 0x00000ac284024fa5,
	0xb8762cbb38859289, 0x00000c4d898313bb,
	0x08eb3becee8be364, 0x00000e61920420da,
	0xa9ff46aeb697b458, 0x000007bb7c5169ea,
	0x07861dc7977a71a0, 0x0000074476022077,
	0xc7082c171373626c, 0x0000069f6d431f6d,
	0xe6593fd65a69a388, 0x000005b961148160,
	0x05665b655a5c0513, 0x0000047b50366a4f,
	0x750582c3ed515744, 0x000002b44ed92635,
	0xe7c81652094e4a2e, 0x000007997a819d7b,
	0x275f2237707851dd, 0x0000070e73327374,
	0x36cb3306d57022cc, 0x0000064a68c3a169,
	0xc5f84ae5f864741f, 0x000005345a054d59,
	0xd4e06c94c053e5ff, 0x0000039f5027bc43,
	0x04c09d92f74e28be, 0x0000017c4afb0624,
	0xc7ab1987c27cb15f, 0x000007727871d979,
	0xe732273743760221, 0x000006d27002d070,
	0x16843af68e6c6338, 0x000005ea63b43564,
	0xa58a5755885e84cb, 0x0000049e52163551,
	0x250281b40751f71c, 0x000002ab4d593436,
	0xe4e9b1c1e24aba6a, 0x000007c67cf15816,
	0x778b1d27a17af191, 0x0000074776321b77,
	0x47012cd71173526f, 0x0000068e6c63386d,
	0x563743c63f6833b2, 0x000005805e04d85e,
	0x455365c50c57f58a, 0x000003e654474f48,
	0x64e298633851685a, 0x0000019d4b0ad427,
	0xc7d414e16852eb26, 0x000007a87b51877c,
	0xf76921077e7921c7, 0x0000071973c26374,
	0x56cc32e6db7072c2, 0x000006456883a969,
	0x25e34d55ea63b435, 0x0000050c57f58a58,
	0x957276347959c66d, 0x0000032553d8783d,
	0x64d0b1025a5059b1, 0x00000161572b2f17,
	0x07bc17a7d37da143, 0x0000078779a1b87b,
	0x4745252759772200, 0x000006e77112af72,
	0x26933956a16d631a, 0x000005f764642065,
	0xf5fe5865905ee4c0, 0x000004815dc66150,
	0x95778733df5ac759, 0x0000025953e9b232,
	0x7522c412cf4b9bad, 0x000007dd7e21353d,
	0x37a41a67ba7c516a, 0x0000076777e1eb79,
	0x17082c1767746250, 0x000007546c233f76,
	0xb6184737416733cf, 0x0000070960d53d72,
	0xd5a471e6e45e161e, 0x0000069055b8446b,
	0xd4b2b1366050a993, 0x000005fa457cca62,
	0xe7e612d6a24bad75, 0x000007fd7c416c7f,
	0xb76e2077fc79c1b4, 0x000007fb7392677f,
	0xa6b73547fa6fd2d5, 0x000007fb6673e47f,
	0xe62c55d7f4651495, 0x000007e95f76417e,
	0xd56c8727e35b5747, 0x000007d65199c97d,
	0x5464d137ce4c0b52, 0x0000084f48be317c,
	0x37cb15f82e7f111a, 0x0000083b7a21a983,
	0x174025b8457741fc, 0x0000085f7042c885,
	0xd69e3dc86f6be346, 0x0000088d68348a87,
	0x461f63389f656550, 0x000008ce5e17378b,
	0x65499b68e9599860, 0x000009274f3b3e90,
	0xd4b5e389214a2cf0, 0x000008427f011b84,
	0x57b318a8707e612e, 0x000008867841e087,
	0x67152a989c75023f, 0x000008d26e83278b,
	0x16b54648ef6d83bb, 0x0000093968a52391,
	0xa61c6fc9666575ff, 0x000009d35da81d99,
	0x5542ae7a12592969, 0x0000099b529c34a5,
	0x07ed1218a7521dad, 0x000008827e213685,
	0x07a41a58b97d514d, 0x000008e876c20b8d,
	0xe72b2f490b731275, 0x0000095a71038192,
	0x66c74d898c6ee422, 0x00000a0869a5a99c,
	0x562d7aca51666699, 0x00000aff5ed8e8aa,
	0xf5bab50af15c0a26, 0x0000094a5bccb1a2,
	0xf7df13b8597eb125, 0x000008cb7d115488,
	0x97b018f90d7c2170, 0x0000096d78e21795,
	0x675332699377129c, 0x00000a047333bf9c,
	0xd6eb530a4c71146b, 0x00000afa6c060fa9,
	0x265a836b6168f710, 0x00000b8a64a93bbd,
	0x765ab8fada650a49, 0x000008617eb129a0,
	0x77d115a89a7e2141, 0x0000091d7be1768d,
	0xd7ce1c29687c1199, 0x00000a1e7db1f19b,
	0x479b315a8b7eb226, 0x00000adc7763c5a9,
	0xd72e552b35752480, 0x00000c1170663fb9,
	0x96dc82dc916da74e, 0x00000b9b6ea921c3,
	0x87eb12bad76f9a4f, 0x000008a17e314486,
	0x57c917c8e07d715f, 0x000009727cc19f92,
	0x87e11f79c77d51c8, 0x00000a967ef22ca2,
	0xe8142abb12800267, 0x00000c3c82c2f8b9,
	0xc7af503c837ed3f4, 0x00000d54780611cd,
	0x578d7d0d0c7806e9, 0x00000bba7988f1c7,
	0x67e414786c7eb12e, 0x000008e47db1618a,
	0x57d31a29297d117f, 0x000009ca7da1ca97,
	0x47f222ba297e51f8, 0x00000b0c802265a9,
	0xc82c2f1b948162a7, 0x00000cd7846344c2,
	0xb88c409d968663a1, 0x00000ec58dd441e6,
	0x983f769d9685e614, 0x000008707ea12fcb,
	0x67dc1628a97e5148, 0x000009297d51808e,
	0x47de1c99727d71a2, 0x00000a207e81f49c,
	0x980525da867f5226, 0x00000b7981629baf,
	0x884432ec0882c2e0, 0x00000d5a862385ca,
	0x88bb43de1e8853e5, 0x00000ebe92a44cec,
	0x27ea130eb29ae45f, 0x000008a87e614887,
	0x47d817e8e37de162, 0x0000096a7db19f92,
	0xe7ec1ed9b87e21c3, 0x00000a6e7f721ba0,
	0xf817288ad880624e, 0x00000bd382b2c8b4,
	0x685e35ec6584230f, 0x00000db687e3b4d0,
	0x38f6445e768a6413, 0x00000eb896b456ec,
	0x37a41a67b67c1170, 0x0000076d7831e179,
	0x273626d74275f223, 0x000006dc7082c071,
	0xe69d3826a16d631c, 0x0000061465e3f465,
	0x65cb5005c1618474, 0x0000050057459d56,
	0x44ac70849051564a, 0x0000038e43a7d641,
	0x54129b92fe3c08b4, 0x000007b87c316d25,
	0xc7831e27957a51a4, 0x0000074075d22776,
	0x47022cc70d732275, 0x000006956cc32e6d,
	0xd64b41764d68f39c, 0x000005a45ff4a15f,
	0x054b5e75405aa53b, 0x000004544e26a64d,
	0x43ed8623ca46d77a, 0x0000029036295e33,
	0xc7c61681cd3c7a8a, 0x000007987a719f7b,
	0x075d22676e7841df, 0x0000070c73127774,
	0xf6c73376d16ff2d1, 0x000006446873ab68,
	0x05ee4bf5ef63f42d, 0x0000052559456359,
	0x74bc6ec4ad52e61d, 0x0000039143d7d242,
	0x73179e62ec3b18d0, 0x000001893c6af323,
	0xd7ac1987c17cb160, 0x000007737881d779,
	0xf734271744761220, 0x000006d37012ce70,
	0x16853af68f6c7337, 0x000005ea63b43564,
	0x75885795875e74cd, 0x0000049951d63b51,
	0xc41d80b40b4a4716, 0x000002bb38791b36,
	0x3404afc1ee34ca57, 0x000007c77d015618,
	0xa78f1cc7a37b118d, 0x0000074c76721477,
	0xa7072c371773a266, 0x000006956cc32d6d,
	0xe63e42f6476893a6, 0x000005895e94ca5e,
	0x551964251658757b, 0x0000040249d72449,
	0x437493f35d410822, 0x000001c13a8a9c2a,
	0xf7d614a17d449b06, 0x000007ac7b917f7c,
	0x77702047847971bd, 0x0000072274425475,
	0x26d73196e77112b0, 0x000006546953926a,
	0x65f44b65fc64a41a, 0x000005a157659a59,
	0x645a79d5874ee690, 0x0000051f3b28ce55,
	0x0359bdb4d837ba3a, 0x000005173bac9249,
	0x77c216f7d97de13b, 0x000007bb7981bb7b,
	0xd73526f7b6769210, 0x000007a36f82dd7a,
	0xa6653e97976b335a, 0x0000077a60a48c78,
	0x652e61b7695a4547, 0x000007404a970e75,
	0x73bf96b728410823, 0x000006de397ae770,
	0x2381d7e6b8347c91, 0x000008107e013973,
	0x47951c18117bd178, 0x0000081676721481,
	0xd6f52e381a732274, 0x000008226b036181,
	0xe6044978286603f2, 0x0000083559c55482,
	0x849d72383e52562d, 0x0000084b45d84a84,
	0xf3cfb1a84c42699d, 0x0000085236fccb84,
	0x27f011b86a376e0b, 0x000008457c716784,
	0xc76f20584f79e1b1, 0x0000086b73b26585,
	0x06b934f87d6ff2d2, 0x000008a766a3de89,
	0xf5a953d8c1610482, 0x000009005336138d,
	0xf4d083a91d50c715, 0x0000096547d98793,
	0x13cfc8d98e41eb01, 0x0000086e3b3e0496,
	0x17e21358527ed122, 0x0000088b7b218c88,
	0x674d24389e7821e4, 0x000008d27122af8b,
	0x96823b48f36ce329, 0x0000094462a45391,
	0x55b15e59755c6508, 0x000009dc57a6dd9a,
	0x14de938a1c5327f7, 0x00000a9f485aa4a6,
	0x044ed3c9d8464bd7, 0x0000085f7ea1278f,
	0xb7d11548927de13c, 0x000008f37ad1968c,
	0x773227590576d208, 0x000009526f02ec92,
	0xe65240a9856a6372, 0x000009f96484c29b,
	0xc5df67aa3d61a590, 0x00000ae359b785a8,
	0x15199ddb4354e8b6, 0x00000a69502af8b2,
	0x97e912c98f4f4c48, 0x000008a07dc14286,
	0x07bd1778dc7cd15b, 0x0000096c7ac19792,
	0xd72928599f7791f4, 0x000009f46de30d9b,
	0xb6af453a316d63a9, 0x00000acf681513a7,
	0x860f6e6b2f64c5ed, 0x00000c0c5cc803b9,
	0x05a59f7bb85b28f4, 0x00000a4659eb2eb1,
	0x97e01478717e9130, 0x000008e87ce1618a,
	0xd7a819e92e7ba17d, 0x000009d57941c397,
	0x779c224a397841ee, 0x00000acf7612e4aa,
	0x76fc45bb0f72c39f, 0x00000bcf6cb529b6,
	0x365d718c44697610, 0x00000c656517eacc,
	0x06499f7bcd64d8d4, 0x000008777ea132b1,
	0xf7d51668b17e214b, 0x000009367c01828e,
	0xe7a01c99867a61a2, 0x00000a3f7a71f79d,
	0xa7b8265aad7af22b, 0x00000bb67c32a7b2,
	0x67e0346c547d02f2, 0x00000d1975c4b9d0,
	0x87046a5d867175cf, 0x00000ca46fe788d3,
	0xc7e9134bec6f48a4, 0x000008b57e314d87,
	0x97c91858f37d8168, 0x000009877b81a693,
	0xc7b61f89dd7b31cc, 0x00000aa87bb22aa3,
	0x87cd2a3b207c3263, 0x00000c3f7da2eaba,
	0x57fd395ce87e933b, 0x00000e758153f8da,
	0xb7e55c2ed285542e, 0x00000ce47aa725dc,
	0x67e414e87f7e9136, 0x000008f47db1688b,
	0x27c21a59377cf185, 0x000009d57be1ca98,
	0x77c5224a307c01f5, 0x00000b097cd25aa9,
	0x67e22d9b887d6296, 0x00000cb47f1324c1,
	0x281c3d3d63804378, 0x00000ed384142ce2,
	0x0913449ecb8a0439, 0x0000087f7e9136ec,
	0x07dd1678b57e414e, 0x000009307d21838f,
	0x57c61c59777c91a2, 0x00000a1c7c81ec9c,
	0x57d424ba7b7cd219, 0x00000b5a7de282ae,
	0xb7f8304bdc7ea2c0, 0x00000d0980a350c6,
	0xd83e3fcdb48213a2, 0x00000ece87b434e6,
	0x27be176ec58e1442, 0x000007917a21aa7b,
	0x275f22376c7821e3, 0x0000071473826a74,
	0x76db3116e170c2b9, 0x000006686a53736a,
	0x36274596216693e0, 0x0000057c5dd4de5d,
	0x353361451c58c572, 0x000004404d16c54b,
	0xe3f68523c3467785, 0x000002b438192633,
	0x27a21a87b47c0173, 0x0000076b7811e479,
	0xf73327274075d227, 0x000006d87052c670,
	0x669738e69b6d1324, 0x0000060a65640465,
	0x65bd5195b560e486, 0x000004ec5635bb55,
	0x749373647750066f, 0x0000036a41c80e3f,
	0x43159eb2d339b8f7, 0x000007b87c316e23,
	0xd7831e17957a51a4, 0x0000074075d22676,
	0x57022cc70d732274, 0x000006946cb32f6d,
	0xb64941a64c68e39d, 0x000005a15fc4a65f,
	0x95465f153b5a6542, 0x0000044b4da6b44c,
	0x43e087a3be46378c, 0x0000027c35197d32,
	0xd7c71661ca2bba8e, 0x000007997a919d7b,
	0x47602207717871db, 0x0000071073427074,
	0x46cb32f6d67032c9, 0x0000064a68c3a269,
	0x75f44b45f6645423, 0x0000052d59a55859,
	0xf4c26df4b5534610, 0x000003994447c542,
	0xe31d9db2f43b78c4, 0x000001a429aac923,
	0x17af1927c37cc15c, 0x0000077978d1cf7a,
	0x873b26474b766215, 0x000006dd7092bf71,
	0xf69039a69a6d0325, 0x000005f964841d64,
	0xb59955a5995f64b2, 0x000004b053061852,
	0xa4367dd4264bb6ed, 0x000002dd3a38e838,
	0x429aac921d301a0f, 0x000007cb7d31501a,
	0x37961c07aa7b7184, 0x0000075677020478,
	0x97132ac723745252, 0x000006f86c93346e,
	0x362445e6eb67b3c1, 0x000006b45c350e6d,
	0x74d86b86905555d6, 0x0000063a44b7b866,
	0xa2f7a2060c3ab8d9, 0x000005ad22db905d,
	0x77d81475f019ec92, 0x000007e57b51867e,
	0xe7612207e278e1ce, 0x000007d972c27f7d,
	0xc6ad3667d36f12eb, 0x000007c565f3f27c,
	0x45a35487bd607492, 0x000007ab5316177b,
	0x441c80e7a04af703, 0x0000078837593b79,
	0xf1e6c1077b2b9a91, 0x000007a21c0d5f76,
	0x47b818281f7db141, 0x000008297901ca82,
	0x672e27c82f76221d, 0x0000083e6f12e983,
	0x165f3f38476ad365, 0x0000085d60549585,
	0xa52d61f86b5a054d, 0x0000088c4a970d87,
	0x636a9508a041481d, 0x000008cc2aaaab8b,
	0x822cddc8e31d2c33, 0x000008507ed12188,
	0x379b1b68567c416b, 0x0000087276d20a86,
	0x86fd2d5884738269, 0x000008af6b935089,
	0x861447c8c966c3dc, 0x000009095af5328e,
	0xc4bb6ed93153d601, 0x0000098c4277fa95,
	0x82c0a859c037f92a, 0x000009be2c4bff9f,
	0x17e91288d72d9d63, 0x000008907de13c86,
	0x57821e48a27b318a, 0x000008ce74d2438b,
	0xf6d03268ec7122ae, 0x000009386853ae90,
	0xa5ce4fa96662f44a, 0x000009d55605c299,
	0x14527aba174e26a7, 0x00000ab13b08d2a6,
	0xb399b57ae93a0a33, 0x0000095138dca7a2,
	0x17da14386d7e612d, 0x000008db7cd15b8a,
	0x17731fe91c7be176, 0x0000094373626e92,
	0x26ac36896f6f42e4, 0x000009dd65a3fc9a,
	0xd594563a215fd4a4, 0x00000ac351d63ba6,
	0xb49c860b2249b732, 0x00000b6146e97ab7,
	0x7445bbfab4456a85, 0x000008787e71329e,
	0xb7c91628ae7da148, 0x000009307ba17e8e,
	0x37941c297c7a819e, 0x000009e173626d9d,
	0x669938aa146e92f9, 0x00000aa464142aa5,
	0xb5ba5b2afe5de4dd, 0x00000bc358a6a2b5,
	0x252389bc365477b2, 0x00000b5450f98ebf,
	0x07e8136a954ffab4, 0x000008b77dd14d88,
	0xd7b71838f67cc167, 0x0000098b7a51a493,
	0x777a1f29e37911c9, 0x00000ab8760221a4,
	0x36b235db37743256, 0x00000b9269f421b4,
	0x66325ccbf566d4ec, 0x00000ce25f16c9c6,
	0x95c6877c9a5d6798, 0x00000b565b598bc0,
	0xe7e01518867e8139, 0x000008fd7d316b8b,
	0x37a31a79437bf188, 0x000009eb78f1cc99,
	0xe75f223a4e7781f5, 0x00000b3876325cab,
	0xb7772e5bc176f29d, 0x00000d0877f336c5,
	0x46c5573d52725447, 0x00000d6b69a656da,
	0x866b848cda684735, 0x000008897e813ac2,
	0x07d616d8c17e1153, 0x000009457c718a90,
	0xa7911cc9927b21aa, 0x00000a497841f59e,
	0x978825cab3785226, 0x00000bad78b299b2,
	0x579732ac417902de, 0x00000d9b7a037fce,
	0xd7d941de637ae3dd, 0x00000e07781566ed,
	0xc7e813bd157296da, 0x000008c27e215388,
	0x27cd18a8fe7d916d, 0x0000098b7bd1a894,
	0x97a01f39de7aa1cb, 0x00000a9e79c220a3,
	0xa79f28db0d79c253, 0x00000c137a32ccb8,
	0x37b4362cac7aa313, 0x00000e0a7c23b8d5,
	0x6822427ecf7d6416, 0x00000ecd885435ed,
	0x07e315288b7e813b, 0x000008f97dc16b8c,
	0xf7c41a49397d1187, 0x000009cc7b61c597,
	0xf7ab215a217ae1ea, 0x00000ae57ac244a7,
	0x57b32b3b577af278, 0x00000c5f7bb2f3bd,
	0x87d2387cf57c533a, 0x00000e477e63dad9,
	0x0863430ed880b424, 0x000007b07bc17aed,
	0xd7831e17917a11aa, 0x0000074576121e76,
	0x97132ac71973c262, 0x000006b36e53006e,
	0x567a3c26776b235c, 0x000005ec63d43163,
	0x35ad53559c5f84ae, 0x000004e255b5cb54,
	0x84a271b47950266c, 0x0000039043c7d340,
	0x27bd1773153d4890, 0x000007917a11aa7b,
	0x275f22376c7821e3, 0x0000071373726b74,
	0x66da3146e070b2ba, 0x000006656a33776a,
	0xe62346061e6673e5, 0x000005765d84e85c,
	0x852a62451458557e, 0x000004324c66da4a,
	0x93e58723b245979f, 0x0000029b36c94c32,
	0x37a41a67b57c0173, 0x0000076d7831e179,
	0x273626d74275f223, 0x000006dc7082c171,
	0xb69a38769f6d431e, 0x0000060e65a3fd65,
	0xb5c15115ba61247f, 0x000004f15685b355,
	0xb49772f47c504668, 0x0000036e41f8083f,
	0x63169e82d639e8f2, 0x000007ba7c416b23,
	0x17871db7987a819f, 0x0000074676221e77,
	0xd7092bf71473826a, 0x0000069e6d431f6d,
	0x865540665769838c, 0x000005b060948e60,
	0xc5565d454c5b5528, 0x000004604ed6934d,
	0xf3f78503d7478766, 0x0000029a36b94e33,
	0x07ca1611eb2d6a5c, 0x0000079f7ae1947c,
	0xd76821277878d1d0, 0x0000071b73e25f74,
	0x46d93166e370e2b5, 0x0000065d69c3846a,
	0x160a48d60c657401, 0x0000054a5b352a5b,
	0x74e56a14d85525db, 0x000004634477c045,
	0x12e3a4546439b8f7, 0x0000044f21abb245,
	0x87b61857c97d1153, 0x000007837961c07a,
	0x0733272785767215, 0x000007756fa2d978,
	0x46713d37666ba34f, 0x0000074061e46975,
	0x05585d07295c1511, 0x000006f34e26a771,
	0x53c68a96d545c799, 0x0000069431e9da6b,
	0xf195ca3675262b2e, 0x000007ff7d115465,
	0xd7881d87fe7af191, 0x000007fd75c2297f,
	0xa6ef2ef7fc728286, 0x000007f96ad3667f,
	0x660d4887f86623ee, 0x000007f55ad5367f,
	0x24c46dc7f45405fc, 0x000007f14397d87f,
	0xf2eea327f039d8f4, 0x000007ed22ab957e,
	0xe7d81487ed153d1b, 0x000008347b418882,
	0x576022183d78d1cf, 0x0000084f72c27e84,
	0x86b036185b6f22e8, 0x000008776643ea86,
	0xa5ad53588760e485, 0x000008af53f5fd89,
	0x14367df8c74c36df, 0x000008fe3978ff8e,
	0x921cbae91b2e4a44, 0x00000905152d1c93,
	0x87c416c85e7ea127, 0x0000087579a1b886,
	0xa73926888676c20b, 0x000008b16ff2d189,
	0x86733cf8ca6be347, 0x0000090961e4698e,
	0x85515dc92e5be517, 0x000009874d66bc95,
	0x33ad8d69bb44b7b8, 0x00000a2d2fca189f,
	0x019fc90a22244b64, 0x0000086e7e612e96,
	0x97b818189e7db141, 0x000008c97841e08b,
	0x37172a68e475023e, 0x000009286d631b90,
	0x163b43495268d39f, 0x000009b75de4dc98,
	0x64fe6739f457559b, 0x00000a81477767a3,
	0x63329b6ad23df87d, 0x00000a932a4ab7b2,
	0xb7e31339d721ebd8, 0x000008ae7d714887,
	0x97bb17b8e87ca160, 0x0000093c77d1ed92,
	0x66fd2d595b73d25f, 0x000009b96b635598,
	0x860f4849f46673e4, 0x00000a845ab53aa3,
	0x64ba6eead853a606, 0x00000b9b4297f5b3,
	0xc3189e5bb23988fd, 0x00000a5338ab1ab1,
	0xa7d814e8857e5138, 0x000008f77c71668b,
	0x77a61a293b7b7183, 0x000009dc7921c698,
	0xe6f92dba0574c244, 0x00000a6c6aa36ba2,
	0xe5f64b1ab8655406, 0x00000b7058c571b0,
	0xe48d73fbdb51564a, 0x00000c42412820c4,
	0x2469a24ba5467911, 0x0000088c7e613baf,
	0x27ca16c8c37dc152, 0x000009477b418890,
	0xb78f1cc9947a31a8, 0x00000a4c7791f49e,
	0x5743255aba75f222, 0x00000b666d7319b3,
	0xc60149dbab66b3de, 0x00000c7a58f56cc0,
	0x755273aceb564660, 0x00000c4d54480ecd,
	0x27e713eba5534911, 0x000008c97de15689,
	0xc7bd18c9077d1170, 0x0000099a7a11aa94,
	0x07781f69f078e1ce, 0x00000abc75f223a5,
	0xd72428db36743255, 0x00000c547012ccbb,
	0xd71636ecfa6f4316, 0x00000da8676508da,
	0x36146deda0634605, 0x00000c685f37e5d1,
	0xc7e01578957e713f, 0x000009097d51718c,
	0x87b11ac94c7c618d, 0x000009ed78e1cc99,
	0x276121fa4a7791f3, 0x00000b2674724fab,
	0x27402c8ba7729285, 0x00000ccd746311c3,
	0x47533b7d7974c360, 0x00000ee876540ce3,
	0x56b6690e3f725510, 0x000008967e7140d4,
	0x67d81718cb7e1157, 0x000009477cc18c90,
	0xf7a61cb98f7bc1aa, 0x00000a387891ee9d,
	0x676d247a9b774217, 0x00000b7c76b27db0,
	0xf76c2fbbff76b2b9, 0x00000d2c76f343c8,
	0xd7823e8dd7776393, 0x00000ee17b0417e8,
	0x57e8140ed9805423, 0x000008c87e215689,
	0xd7d01889007db16e, 0x000009817c31a593,
	0xd7a01e69cb7b21c4, 0x00000a7779220da1,
	0x6787269ada78b239, 0x00000bbd78629fb4,
	0xb78b31cc3e7872db, 0x00000d62792363cc,
	0x87b43fee0379f3ae, 0x00000edb7f2420ea,
	0x17a21a97af7bb17b, 0x000007707861dc79,
	0x274425474b766215, 0x000006f571d29a72,
	0xc6c433c6c36f32e7, 0x0000065069139968,
	0x461a46f60d6583ff, 0x000005745d64eb5c,
	0xf53d60151d58d570, 0x0000045b4e869b4b,
	0x74347e23f148f73d, 0x000007b07bc17938,
	0xe7841df7917a21a9, 0x0000074776321b76,
	0xb7152a971c73e25e, 0x000006b66e82fb6e,
	0x867d3bd67a6b5357, 0x000005ef63f42d63,
	0x65af53259e5fa4a9, 0x000004e555d5c754,
	0x94a271947b50366a, 0x0000039043c7d340,
	0x37bf1753143d3892, 0x000007937a41a67b,
	0x776321c76f7851de, 0x0000071973c26274,
	0xe6e13076e77112af, 0x0000066f6ac3686a,
	0xa62d44d6296703d4, 0x000005845e44d25d,
	0xa539609524593566, 0x000004464d66bc4b,
	0x13f984d3c846b77e, 0x000002b538292534,
	0x87a819f7b87c316e, 0x000007737881d779,
	0xb73e25f74a765217, 0x000006e77122af71,
	0xb6a836e6ad6e0309, 0x0000062266a3df66,
	0x55d74ea5d062545d, 0x0000050f58158657,
	0x24b86f249e521633, 0x0000039a4457c342,
	0xe3459933083c88a5, 0x000007bf7c916326,
	0xa78f1cc79f7ae194, 0x0000075176b20c77,
	0xe7172a5722744254, 0x000006b26e53016e,
	0xd65840066f6ac368, 0x0000066f60149e67,
	0x05386096535a154b, 0x000006084c36de63,
	0x53b08d05df4417ca, 0x000005923119f25b,
	0x77d015657e263b2d, 0x000007c87ae1937c,
	0x075e2257c67881d8, 0x000007b972e27c7c,
	0x66ba34e7b06f72df, 0x0000079b6753cc7a,
	0x05cf4f878e62645a, 0x0000077056c5ab78,
	0xd48275575f4fe674, 0x0000073a3f785074,
	0x42b4a9a72635e967, 0x000007071fdbe671,
	0x17ab1988107cc15d, 0x000008137851dd81,
	0x872928581675a22b, 0x0000081b6f12ea81,
	0x266b3dd81f6b235c, 0x0000082661b46f82,
	0x155a5cc82b5c0513, 0x000008374e869c83,
	0x53d788a83e467785, 0x0000084c3379ad84,
	0x91c6c4a853286aee, 0x0000083c7d514c85,
	0xd78c1d28447b318b, 0x0000085976022184,
	0x46f72e086572e27b, 0x000008846b735387,
	0xa61e46889666f3d5, 0x000008c05c350e8a,
	0x54e869a8d955c5c8, 0x000009144667878f,
	0x73319b89353d488f, 0x0000097827dafe95,
	0xa7e712c9811bbc5d, 0x000008797c416b86,
	0x876e20888579a1b7, 0x000008ad73c26289,
	0x06c63388c57052c6, 0x000008ff67f3b88e,
	0x95d54ed92262f449, 0x000009755705a594,
	0xa47c75e9a54fd675, 0x00000a123ec8659d,
	0x3296ad0a4d34998b, 0x000009d5203bdca8,
	0x97d914687a7e3132, 0x000008d47c216e8a,
	0x57552358dc7881d7, 0x0000091571e2998f,
	0x469b38793a6e0309, 0x0000099364d41396,
	0x55945649c95f64b1, 0x00000a4752562da0,
	0xe41c80da904a870f, 0x00000b2e37e92bad,
	0x5261b30aec2efa2e, 0x000008867e1138a4,
	0x17c81648b97d514c, 0x000009317b917f8f,
	0xd74824c95778c1d0, 0x000009967092bf96,
	0x167a3c29c86c533a, 0x00000a43627458a0,
	0xd5605c1a8c5c9502, 0x00000b364ea698ad,
	0x33cf899b9546478a, 0x00000b6b35b96abf,
	0x07e413cacb2d4a60, 0x000008c47d615289,
	0x27b61869007c516a, 0x0000098c7a51a494,
	0xb77c1ed9df7921c7, 0x00000a3f70e2b5a3,
	0xd66e3d8a776c0344, 0x00000b0f61547aab,
	0xe5425f7b6a5b152e, 0x00000c394c56dabc,
	0xc3d788bc7b4427c8, 0x00000b65356974bf,
	0xc7da1568977e5140, 0x000009097c916f8c,
	0x77a21a994c7b318a, 0x000009ea78f1cc99,
	0x176221ea4877a1f2, 0x00000b2674724fab,
	0xc68f39cba372428e, 0x00000c00627458ba,
	0x05435f6c635bb51d, 0x00000d164c66d9cd,
	0xc4a588cc9f461790, 0x0000089c7e6142bf,
	0xd7d01728d17dd159, 0x000009507bd18d90,
	0xd78e1cd99b7a11ab, 0x00000a497791f39e,
	0x274724dab076221e, 0x00000ba272a282b2,
	0xb6e62fec2f70a2bd, 0x00000d776bf345cc,
	0x756d5abd8d607492, 0x00000d4b595687dd,
	0xe7e6143caa582780, 0x000008d37df15a89,
	0xf7c618e90d7d4173, 0x000009977b11ac94,
	0x177b1ef9e878f1cb, 0x00000aa3765218a4,
	0x9730278b1074c246, 0x00000c0f7112afb8,
	0x36cb330ca26f02ed, 0x00000dec6e8382d4,
	0x06bb4ddea06ff3d9, 0x00000d6f64b650e6,
	0x17e015a89f7e6144, 0x0000090a7d71728d,
	0xc7bc1a99487cb18c, 0x000009d87a61c898,
	0xb76a20ea2d7841e8, 0x00000af0753239a8,
	0xc71d29ab60739267, 0x00000c607232d8bd,
	0xa731363cef72b31b, 0x00000e2e73a3b0d8,
	0x3791413ed774c400, 0x0000089c7e7143ee,
	0x17da16f8cc7e1158, 0x0000093c7d018890,
	0x27b31c097c7c31a3, 0x00000a1079d1df9c,
	0x276c228a66783202, 0x00000b27761254ac,
	0xb75a2bbb9475c285, 0x00000c8b75a2f6c0,
	0x3765378d1475e335, 0x00000e367763beda,
	0x07bc17aec2794404, 0x000007947a41a57b,
	0x376d20877578a1d4, 0x0000072e74e24275,
	0x77042c770572b282, 0x000006a66da3146d,
	0x467a3c366f6ac368, 0x000005f364342663,
	0x15c65085ad607493, 0x0000050f58158656,
	0xe4eb6964b953860b, 0x0000040449e72145,
	0x47a41a57b07bc179, 0x000007737881d879,
	0x774824d74f769210, 0x000006fa72229172,
	0x36cb3306ca6f82dd, 0x0000065869838c69,
	0xf62445f6166603f1, 0x000005805e14d85c,
	0xe54a5e952b59955b, 0x0000046b4f66824c,
	0xa4447c540349e722, 0x000007b37bf17539,
	0x47891d77957a51a3, 0x0000074e76921177,
	0x571e299724745251, 0x000006c26f22e96f,
	0x868b3a46886c1342, 0x0000060264f41064,
	0xf5c450b5b460d488, 0x0000050157559b55,
	0xd4c16e149c51f638, 0x000003b945f79442,
	0x87c216e3413f984d, 0x0000079a7a919c7b,
	0x176b20d77778c1d1, 0x0000072674724f75,
	0x06f02ec6f671e299, 0x000006846bd3486c,
	0x76464206416853af, 0x000005a560049f5f,
	0x755f5c454a5b352a, 0x0000047950266d4e,
	0xd3f485640249d724, 0x000004b735c96a48,
	0x17af1927be7c8164, 0x0000077e7921c77a,
	0xb745252757771202, 0x0000074d7102b274,
	0x369938a7426d7319, 0x0000071f65440873,
	0x15b152e70a607493, 0x000006d65525db6f,
	0xb47376f6b84e869b, 0x0000067e3f385869,
	0x62d1a66664367956, 0x000007eb7c816565,
	0x77851de7e97a819d, 0x000007e375c2287e,
	0xb6fb2d77df72f27a, 0x000007d66c23407d,
	0xa6384397d06813b5, 0x000007c35e74cc7c,
	0x45286277bc58d570, 0x000007ab4b86f37b,
	0x93b28cd7a243b7d5, 0x0000079131d9dc79,
	0xe7c916278b27daff, 0x000008227a919c81,
	0xb75c2298277851de, 0x0000083172d27d82,
	0xe6bd3488366f82dd, 0x0000084567b3c083,
	0x75dd4e084e630448, 0x0000086157f58985,
	0x94a271986c517646, 0x0000088642180487,
	0xf2f6a23893392907, 0x000008a824eb5389,
	0x27b218c84a7d414e, 0x0000085c78c1d085,
	0x773427186976221c, 0x000008886ff2d187,
	0xe6803b689a6c333d, 0x000008c463543e8a,
	0x85835828dd5e14d7, 0x000009175196428f,
	0xa4208069374a3717, 0x0000097d38f90d95,
	0x8242b6899e2efa2f, 0x000008747e51309b,
	0x379d1b38887c7165, 0x000008a577220189,
	0x470e2b68bb742257, 0x000008f06d33218d,
	0x264741f910691398, 0x000009595f44b693,
	0x552e61c98559755f, 0x000009e84b96f09b,
	0x63a78e2a1f4387db, 0x00000a8c307a03a5,
	0x47e1136a55270b16, 0x000008b27d714a88,
	0xa78f1cb8e67cb15f, 0x0000090275d2278e,
	0x46ed2f1920727288, 0x0000096d6ad36694,
	0xf61447b99c6653e8, 0x00000a095ba51e9c,
	0xc4e46a2a485555d6, 0x00000ad4466787a8,
	0xd342998b1e3d9887, 0x00000aba2c5a7bb4,
	0x07d315088f7df13b, 0x000008f87c71678c,
	0xa7a919d9357b9180, 0x0000097975623397,
	0xc6d831899e7182a4, 0x00000a026913979c,
	0x45ee4c0a40644423, 0x00000acf58e56ea8,
	0x84ab70bb21523631, 0x00000bcf4247ffb7,
	0x332c9c1bc03a48e7, 0x000008987e2140b3,
	0x47c416c8cb7d4155, 0x000009447b518690,
	0x97931c498a7a51a4, 0x00000a3177f1e99d,
	0x46d9316a47728287, 0x00000ab468b3a4a7,
	0x05dc4e1afe63743b, 0x00000baa577598b5,
	0xa48674cc09505666, 0x00000c4541481bc6,
	0xf7e4143bbd3a18ec, 0x000008d27d915989,
	0xc7b318a90c7c8170, 0x000009937a31a794,
	0xa77d1ed9e27911c8, 0x00000a9c766215a3,
	0x1732275b0974d243, 0x00000b996b5356b8,
	0x15e94c8bdb64f410, 0x00000c8f57b590c3,
	0x8491739cef50166d, 0x00000c54421803cd,
	0x67dc15b8a37e4145, 0x0000090f7cf1738d,
	0x67a21a894f7bd18d, 0x000009e37911c899,
	0x9767214a3977d1ec, 0x00000b0274f240a9,
	0x87172a6b77734270, 0x00000c866f62e1bf,
	0xc63643ed206d3321, 0x00000d715a3548d3,
	0x54aa70cd7f51f638, 0x000008a47e5146cf,
	0xe7d41738d67de15c, 0x0000094b7c518d90,
	0xc7921c598f7b21a8, 0x00000a2e7801e89d,
	0xd754237a8976b20d, 0x00000b5c73a265ae,
	0xa7002cfbd571f297, 0x00000cea6df30bc5,
	0x969638fd856bc34b, 0x00000e415f44b5e2,
	0x37e6146d955d7616, 0x000008d37df15a8a,
	0x37cc18a9097d6171, 0x000009837bc1a594,
	0x97851df9ca7a81c1, 0x00000a6e771202a1,
	0x1744253acb75c229, 0x00000ba072b282b3,
	0xc6f12eac1a70f2b4, 0x00000d286d1325c9,
	0x67043b1db76e0369, 0x00000e796e147ee4,
	0xd7e11588a07e6144, 0x000008ff7da16d8c,
	0x17c419e9357d0185, 0x000009b27b41b997,
	0x87801f49f97a01d5, 0x00000a9d766216a4,
	0xd73a265af975123c, 0x00000bc9721292b5,
	0x17272fec3b7152c4, 0x00000d2d732339cb,
	0x07603b0daa742375, 0x00000000000000e2
};
/*dma size:1877x2x64 bit = 30032 byte*/
static unsigned int dma_size = 30032;
static unsigned int dma_addr;

void tv_dolby_vision_config(void)
{
	unsigned int delay_tmp, el_41_mode, bl_enable;
	unsigned int el_enable, scramble, mode;
	unsigned int hsize, vsize, i;
	uint64_t *dma_addr_p;
	int flags = CODEC_MM_FLAGS_CMA_FIRST|CODEC_MM_FLAGS_CMA_CLEAR|
		CODEC_MM_FLAGS_CPU;
	delay_tmp = 10;
	bl_enable = 1;
	el_enable = 0;
	el_41_mode = 0;
	scramble = 1;
	mode = 1;
	hsize = 1920;
	vsize = 1080;
	dma_addr = codec_mm_alloc_for_dma("dolby_vision",
		(dma_size/PAGE_SIZE + 1), 0, flags);
	dma_addr_p = phys_to_virt(dma_addr);
	pr_info("dm_addr:0x%x\n", dma_addr);
	for (i = 0; i < 3754; i++)
		*(dma_addr_p + i) = dma_data[i];
	WRITE_VPP_REG(DOLBY_TV_SWAP_CTRL0, delay_tmp << 4 |
		bl_enable << 0 | el_enable << 1 | el_41_mode << 2);
	WRITE_VPP_REG(DOLBY_TV_SWAP_CTRL1, ((hsize + 0x100) << 16 |
		(vsize + 0x40)));
	WRITE_VPP_REG(DOLBY_TV_SWAP_CTRL3, 0x600004);
	WRITE_VPP_REG(DOLBY_TV_SWAP_CTRL4, 0x40004);
	WRITE_VPP_REG(DOLBY_TV_SWAP_CTRL2, (hsize<<16 | vsize));
	WRITE_VPP_REG(DOLBY_TV_AXI2DMA_CTRL1, 0x6f666080);
	WRITE_VPP_REG(DOLBY_TV_AXI2DMA_CTRL2, dma_addr);
	WRITE_VPP_REG(DOLBY_TV_AXI2DMA_CTRL3, 0x80000002);
	WRITE_VPP_REG(DOLBY_TV_AXI2DMA_CTRL0, 0x01000042);
	WRITE_VPP_REG(DOLBY_TV_AXI2DMA_CTRL0, 0x80400042);

	/*0x2c2d0:5-4-1-3-2-0*/
	WRITE_VPP_REG_BITS(DOLBY_TV_SWAP_CTRL5, 0x2c2d0, 14, 18);
	WRITE_VPP_REG_BITS(DOLBY_TV_SWAP_CTRL5, 0xa, 0, 4);

	if (scramble)
		WRITE_VPP_REG_BITS(DOLBY_TV_SWAP_CTRL5, 1, 4, 1);
	else
		WRITE_VPP_REG_BITS(DOLBY_TV_SWAP_CTRL5, 0, 4, 1);
	if (mode > 3)
		WRITE_VPP_REG(DOLBY_TV_REG_START + (231 * 4), 0xb);
	WRITE_VPP_REG_BITS(DOLBY_TV_SWAP_CTRL6, 1, 20, 1);
	WRITE_VPP_REG_BITS(VIU_MISC_CTRL1, 0, 16, 1);
}

