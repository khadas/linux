#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/errno.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
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

#include <linux/cma.h>
#include <linux/amlogic/codec_mm/codec_mm.h>
#include <linux/dma-contiguous.h>
#include <linux/amlogic/iomap.h>

DEFINE_SPINLOCK(dovi_lock);

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

/* STB: if sink support DV, always output DV
		else always output SDR/HDR */
/* TV:  when source is DV, convert to SDR */
#define DOLBY_VISION_FOLLOW_SINK		0

/* STB: output DV only if source is DV
		and sink support DV
		else always output SDR/HDR */
/* TV:  when source is DV or HDR, convert to SDR */
#define DOLBY_VISION_FOLLOW_SOURCE		1

/* STB: always follow dolby_vision_mode */
/* TV:  if set dolby_vision_mode to SDR8,
		convert all format to SDR by TV core,
		else bypass Dolby Vision */
#define DOLBY_VISION_FORCE_OUTPUT_MODE	2

static unsigned int dolby_vision_policy;
module_param(dolby_vision_policy, uint, 0664);
MODULE_PARM_DESC(dolby_vision_policy, "\n dolby_vision_policy\n");

static bool dolby_vision_enable;
module_param(dolby_vision_enable, bool, 0664);
MODULE_PARM_DESC(dolby_vision_enable, "\n dolby_vision_enable\n");

static bool force_stb_mode;
module_param(force_stb_mode, bool, 0664);
MODULE_PARM_DESC(force_stb_mode, "\n force_stb_mode\n");

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

#define FLAG_FRAME_DELAY_MASK	0xf
#define FLAG_FRAME_DELAY_SHIFT	16

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
/*dma size:1877x2x64 bit = 30032 byte*/
#define TV_DMA_TBL_SIZE 3754
static unsigned int dma_size = 30032;
static dma_addr_t dma_paddr;
static void *dma_vaddr;
static unsigned int dma_start_line = 0x400;

#define CRC_BUFF_SIZE (256 * 1024)
#define CRC_DELAY_FRAME 5
static char *crc_output_buf;
static u32 crc_outpuf_buff_off;
static u32 crc_count;
static s32 crc_read_delay;

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

static char md_buf[1024];
static char comp_buf[8196];

static struct dovi_setting_s dovi_setting;
static struct dovi_setting_s new_dovi_setting;

static struct pq_config_s pq_config;
struct ui_menu_params_s menu_param;
static struct tv_dovi_setting_s tv_dovi_setting;
static bool tv_dovi_setting_update_flag;
static struct platform_device *dovi_pdev;

/* (256*4+256*2)*4/8 64bit */
#define STB_DMA_TBL_SIZE \
	((sizeof(dovi_setting.dm_lut1) + \
	sizeof(dovi_setting.dm_lut1.G2L)) / 8)
static uint64_t stb_core1_lut[STB_DMA_TBL_SIZE];

static void dump_tv_setting(
	struct tv_dovi_setting_s *setting,
	int frame_cnt, int debug_flag)
{
	int i;
	uint64_t *p;

	if ((debug_flag & 0x10) && dump_enable) {
		pr_info("\nreg\n");
		p = (uint64_t *)&setting->core1_reg_lut[0];
		for (i = 0; i < 222; i += 2)
			pr_info("%016llx, %016llx,\n", p[i], p[i+1]);
	}
	if ((debug_flag & 0x20) && dump_enable) {
		pr_info("\ng2l_lut\n");
		p = (uint64_t *)&setting->core1_reg_lut[0];
		for (i = 222; i < 222 + 256; i += 2)
			pr_info("%016llx %016llx\n", p[i], p[i+1]);
	}
	if ((debug_flag & 0x40) && dump_enable) {
		pr_info("\n3d_lut\n");
		p = (uint64_t *)&setting->core1_reg_lut[0];
		for (i = 222 + 256; i < 222 + 256 + 3276; i += 2)
			pr_info("%016llx %016llx\n", p[i], p[i+1]);
		pr_info("\n");
	}
}

void dolby_vision_update_pq_config(char *pq_config_buf)
{
	memcpy(&pq_config, pq_config_buf, sizeof(struct pq_config_s));
	pr_dolby_dbg("update_pq_config[%ld] %x %x %x %x\n",
		sizeof(struct pq_config_s),
		pq_config_buf[1],
		pq_config_buf[2],
		pq_config_buf[3],
		pq_config_buf[4]);
}
EXPORT_SYMBOL(dolby_vision_update_pq_config);

static void prepare_stb_dolby_core1_lut(uint32_t *p_core1_lut)
{
	uint32_t *p_lut;
	int i;

	p_lut = &p_core1_lut[256*4]; /* g2l */
	for (i = 0; i < 128; i++) {
		stb_core1_lut[i] =
		stb_core1_lut[i+128] =
			((uint64_t)p_lut[1] << 32) |
			((uint64_t)p_lut[0] & 0xffffffff);
		p_lut += 2;
	}
	p_lut = &p_core1_lut[0]; /* 4 lut */
	for (i = 256; i < 768; i++) {
		stb_core1_lut[i] =
			((uint64_t)p_lut[1] << 32) |
			((uint64_t)p_lut[0] & 0xffffffff);
		p_lut += 2;
	}
}

static int stb_dolby_core1_set(
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
	VSYNC_WR_MPEG_REG(DOLBY_TV_SWAP_CTRL0, 0);
	VSYNC_WR_MPEG_REG(DOLBY_TV_SWAP_CTRL2,
		(hsize << 16) | vsize);
	VSYNC_WR_MPEG_REG(DOLBY_TV_SWAP_CTRL1,
		((hsize + 0x40) << 16) | (vsize + 0x20));
	VSYNC_WR_MPEG_REG(DOLBY_TV_SWAP_CTRL3,
		(hwidth << 16) | vwidth);
	VSYNC_WR_MPEG_REG(DOLBY_TV_SWAP_CTRL4,
		(hpotch << 16) | vpotch);
	VSYNC_WR_MPEG_REG(DOLBY_TV_SWAP_CTRL5, 0x6);
	VSYNC_WR_MPEG_REG(DOLBY_TV_SWAP_CTRL6, 0xba000000);

	/* axi dma for lut table */
	prepare_stb_dolby_core1_lut(p_core1_lut);
	/* dma1:11-0 tv_oo+g2l size, dma2:23-12 3d lut size */
	WRITE_VPP_REG(DOLBY_TV_AXI2DMA_CTRL1, 0x00000080);
	WRITE_VPP_REG(DOLBY_TV_AXI2DMA_CTRL2, (u32)dma_paddr);
	/* dma3:23-12 cvm size */
	WRITE_VPP_REG(DOLBY_TV_AXI2DMA_CTRL3,
		0x80100000 | dma_start_line);
	WRITE_VPP_REG(DOLBY_TV_AXI2DMA_CTRL0, 0x01000062);
	WRITE_VPP_REG(DOLBY_TV_AXI2DMA_CTRL0, 0x80400042);

	VSYNC_WR_MPEG_REG(DOLBY_TV_REG_START + 4, 4);
	VSYNC_WR_MPEG_REG(DOLBY_TV_REG_START + 2, 1);

	/* bypass composer to get 12bit when SDR and HDR source */
	if (!dovi_src)
		bypass_flag |= 1 << 0;
	if (scramble_en) {
		if (dolby_vision_flags & FLAG_BYPASS_CVM)
			bypass_flag |= 1 << 13;
	}
	VSYNC_WR_MPEG_REG(DOLBY_TV_REG_START + 1,
		(0x7 << 6) | (el_41_mode << 3) | bypass_flag);
	/* for delay */
	VSYNC_WR_MPEG_REG(DOLBY_TV_REG_START + 1,
		(0x7 << 6) | (el_41_mode << 3) | bypass_flag);
	if ((dolby_vision_flags & FLAG_RESET_EACH_FRAME) && (dm_count == 0))
		count = 27;
	else
		count = dm_count;
	for (i = 0; i < 14; i++)
		VSYNC_WR_MPEG_REG(DOLBY_TV_REG_START + 6 + i,
			p_core1_dm_regs[i]);
	for (i = 17; i < count; i++)
		VSYNC_WR_MPEG_REG(DOLBY_TV_REG_START + 6 + i,
			p_core1_dm_regs[i-3]);
	VSYNC_WR_MPEG_REG(DOLBY_TV_REG_START + 6 + 27, 0);

	if ((dolby_vision_flags & FLAG_RESET_EACH_FRAME) && (comp_count == 0))
		count = 173;
	else
		count = comp_count;
	for (i = 0; i < count; i++)
		VSYNC_WR_MPEG_REG(DOLBY_TV_REG_START + 6 + 44 + i,
			p_core1_comp_regs[i]);

	/* metadata program done */
	VSYNC_WR_MPEG_REG(DOLBY_TV_REG_START + 3, 1);

	/* bypass dither */
	WRITE_VPP_REG_BITS(DOLBY_TV_SWAP_CTRL6, 1, 25, 1);

	VSYNC_WR_MPEG_REG(DOLBY_TV_SWAP_CTRL0,
		(el_41_mode ? 3 : 0x10) << 4 |
		bl_enable << 0 | composer_enable << 1 | el_41_mode << 2);

	tv_dovi_setting_update_flag = true;
	return 0;
}

static int tv_dolby_core1_set(
	uint64_t *dma_data,
	int hsize,
	int vsize,
	int bl_enable,
	int el_enable,
	int el_41_mode,
	bool scramble)
{
	WRITE_VPP_REG(DOLBY_TV_SWAP_CTRL0,
		(el_41_mode ? 3 : 0x10) << 4 |
		bl_enable << 0 | el_enable << 1 | el_41_mode << 2);
	WRITE_VPP_REG(DOLBY_TV_SWAP_CTRL1,
		((hsize + 0x100) << 16 | (vsize + 0x40)));
	WRITE_VPP_REG(DOLBY_TV_SWAP_CTRL3, (hwidth << 16) | vwidth);
	WRITE_VPP_REG(DOLBY_TV_SWAP_CTRL4, (hpotch << 16) | vpotch);
	WRITE_VPP_REG(DOLBY_TV_SWAP_CTRL2, (hsize << 16) | vsize);

	if (!dolby_vision_on) {
		WRITE_VPP_REG(DOLBY_TV_AXI2DMA_CTRL1, 0x6f666080);
		WRITE_VPP_REG(DOLBY_TV_AXI2DMA_CTRL2, (u32)dma_paddr);
		WRITE_VPP_REG(DOLBY_TV_AXI2DMA_CTRL3,
			0x80000000 | dma_start_line);
		WRITE_VPP_REG(DOLBY_TV_AXI2DMA_CTRL0, 0x01000042);
		WRITE_VPP_REG(DOLBY_TV_AXI2DMA_CTRL0, 0x80400042);
	}
	/*0x2c2d0:5-4-1-3-2-0*/
	WRITE_VPP_REG_BITS(DOLBY_TV_SWAP_CTRL5, 0x2c2d0, 14, 18);
	WRITE_VPP_REG_BITS(DOLBY_TV_SWAP_CTRL5, 0xa, 0, 4);

	if (scramble)
		WRITE_VPP_REG_BITS(DOLBY_TV_SWAP_CTRL5, 1, 4, 1);
	else
		WRITE_VPP_REG_BITS(DOLBY_TV_SWAP_CTRL5, 0, 4, 1);

	WRITE_VPP_REG_BITS(DOLBY_TV_SWAP_CTRL6, 1, 20, 1);
	/* bypass dither */
	WRITE_VPP_REG_BITS(DOLBY_TV_SWAP_CTRL6, 1, 25, 1);

	if (dolby_vision_flags & FLAG_CERTIFICAION)
		crc_count++;
	return 0;
}

int dolby_vision_update_setting(void)
{
	uint64_t *dma_data;
	uint32_t size = 0;
	int i;
	uint64_t *p;

	if (!p_funcs)
		return -1;
	if (!tv_dovi_setting_update_flag)
		return 0;
	if (is_meson_txlx_package_962X() && !force_stb_mode) {
		dma_data = tv_dovi_setting.core1_reg_lut;
		size = sizeof(uint64_t) * TV_DMA_TBL_SIZE;
		memcpy(dma_vaddr, dma_data, size);
	} else if (is_meson_txlx_package_962E() || force_stb_mode) {
		dma_data = stb_core1_lut;
		size = sizeof(uint64_t) * STB_DMA_TBL_SIZE;
		memcpy(dma_vaddr, dma_data, size);
	}
	if (size && (debug_dolby & 0x8)) {
		p = (uint64_t *)dma_vaddr;
		for (i = 0; i < size / 8; i += 2)
			pr_info("%016llx, %016llx\n", p[i], p[i+1]);
	}
	tv_dovi_setting_update_flag = false;
	return -1;
}
EXPORT_SYMBOL(dolby_vision_update_setting);

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

	VSYNC_WR_MPEG_REG(DOLBY_CORE2A_CLKGATE_CTRL, 0);
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
	if (is_meson_txlx_package_962E() || force_stb_mode)
		VSYNC_WR_MPEG_REG(DOLBY_CORE2A_SWAP_CTRL5, 0xf8000000);
	else
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
	for (i = 0; i < count; i++) {
#ifdef FORCE_HDMI_META
		if ((i == 20) && (p_core3_md_regs[i] == 0x5140a3e))
			VSYNC_WR_MPEG_REG(DOLBY_CORE3_REG_START + 0x24 + i,
				(p_core3_md_regs[i] & 0xffffff00) | 0x80);
		else
#endif
		VSYNC_WR_MPEG_REG(DOLBY_CORE3_REG_START + 0x24 + i,
			p_core3_md_regs[i]);
	}
	for (; i < 30; i++)
		VSYNC_WR_MPEG_REG(DOLBY_CORE3_REG_START + 0x24 + i, 0);

	/* from addr 0x90 */
	/* core3 metadata program done */
	VSYNC_WR_MPEG_REG(DOLBY_CORE3_REG_START + 3, 1);

	/* enable core3 */
	VSYNC_WR_MPEG_REG(DOLBY_CORE3_SWAP_CTRL0, (dolby_enable << 0));
	return 0;
}

static void osd_bypass(int bypass)
{
	static uint32_t osd_backup_ctrl;
	static uint32_t osd_backup_eotf;
	static uint32_t osd_backup_mtx;
	if (bypass) {
		osd_backup_ctrl = VSYNC_RD_MPEG_REG(VIU_OSD1_CTRL_STAT);
		osd_backup_eotf = VSYNC_RD_MPEG_REG(VIU_OSD1_EOTF_CTL);
		osd_backup_mtx = VSYNC_RD_MPEG_REG(VPP_MATRIX_CTRL);
		VSYNC_WR_MPEG_REG_BITS(VIU_OSD1_EOTF_CTL, 0, 31, 1);
		VSYNC_WR_MPEG_REG_BITS(VIU_OSD1_CTRL_STAT, 0, 3, 1);
		VSYNC_WR_MPEG_REG_BITS(VPP_MATRIX_CTRL, 0, 7, 1);
	} else {
		VSYNC_WR_MPEG_REG(VPP_MATRIX_CTRL, osd_backup_mtx);
		VSYNC_WR_MPEG_REG(VIU_OSD1_CTRL_STAT, osd_backup_ctrl);
		VSYNC_WR_MPEG_REG(VIU_OSD1_EOTF_CTL, osd_backup_eotf);
	}
}

static uint32_t viu_eotf_ctrl_backup;
static uint32_t xvycc_lut_ctrl_backup;
static uint32_t inv_lut_ctrl_backup;
static uint32_t vpp_vadj_backup;
static uint32_t vpp_gainoff_backup;
static uint32_t vpp_ve_enable_ctrl_backup;
static uint32_t xvycc_vd1_rgb_ctrst_backup;
static uint32_t vpp_misc_backup;
static bool is_video_effect_bypass;
static void video_effect_bypass(int bypass)
{
	if (bypass) {
		if (!is_video_effect_bypass) {
			viu_eotf_ctrl_backup =
				VSYNC_RD_MPEG_REG(VIU_EOTF_CTL);
			xvycc_lut_ctrl_backup =
				VSYNC_RD_MPEG_REG(XVYCC_LUT_CTL);
			inv_lut_ctrl_backup =
				VSYNC_RD_MPEG_REG(XVYCC_INV_LUT_CTL);
			vpp_vadj_backup =
				VSYNC_RD_MPEG_REG(VPP_VADJ_CTRL);
			vpp_gainoff_backup =
				VSYNC_RD_MPEG_REG(VPP_GAINOFF_CTRL0);
			vpp_ve_enable_ctrl_backup =
				VSYNC_RD_MPEG_REG(VPP_VE_ENABLE_CTRL);
			xvycc_vd1_rgb_ctrst_backup =
				VSYNC_RD_MPEG_REG(XVYCC_VD1_RGB_CTRST);
			vpp_misc_backup =
				VSYNC_RD_MPEG_REG(VPP_MISC);
			is_video_effect_bypass = true;
		}
		VSYNC_WR_MPEG_REG(VIU_EOTF_CTL, 0);
		VSYNC_WR_MPEG_REG(XVYCC_LUT_CTL, 0);
		VSYNC_WR_MPEG_REG(XVYCC_INV_LUT_CTL, 0);
		VSYNC_WR_MPEG_REG(VPP_VADJ_CTRL, 0);
		VSYNC_WR_MPEG_REG(VPP_GAINOFF_CTRL0, 0);
		VSYNC_WR_MPEG_REG(VPP_VE_ENABLE_CTRL, 0);
		VSYNC_WR_MPEG_REG(XVYCC_VD1_RGB_CTRST, 0);
		if (vpp_misc_backup & (1<<28))
			VSYNC_WR_MPEG_REG_BITS(VPP_MISC,
				0, 28, 1);
	} else if (is_video_effect_bypass) {
		VSYNC_WR_MPEG_REG(VIU_EOTF_CTL,
			viu_eotf_ctrl_backup);
		VSYNC_WR_MPEG_REG(XVYCC_LUT_CTL,
			xvycc_lut_ctrl_backup);
		VSYNC_WR_MPEG_REG(XVYCC_INV_LUT_CTL,
			inv_lut_ctrl_backup);
		VSYNC_WR_MPEG_REG(VPP_VADJ_CTRL,
			vpp_vadj_backup);
		VSYNC_WR_MPEG_REG(VPP_GAINOFF_CTRL0,
			vpp_gainoff_backup);
		VSYNC_WR_MPEG_REG(VPP_VE_ENABLE_CTRL,
			vpp_ve_enable_ctrl_backup);
		VSYNC_WR_MPEG_REG(XVYCC_VD1_RGB_CTRST,
			xvycc_vd1_rgb_ctrst_backup);
		if (vpp_misc_backup & (1<<28))
			VSYNC_WR_MPEG_REG_BITS(VPP_MISC,
				1, 28, 1);
	}
}

static uint32_t dolby_ctrl_backup;
static uint32_t viu_misc_ctrl_backup;
static uint32_t vpu_hdmi_fmt_backup;
static uint32_t vpp_matrix_backup;
static uint32_t vpp_dummy1_backup;
static uint32_t vpp_data_conv_para0_backup;
static uint32_t vpp_data_conv_para1_backup;
static uint32_t dolby_tv_gate_ctrl_backup;
static uint32_t dolby_core2_gate_ctrl_backup;
static uint32_t dolby_core3_gate_ctrl_backup;
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
			vpp_matrix_backup =
				VSYNC_RD_MPEG_REG(VPP_MATRIX_CTRL);
			vpp_dummy1_backup =
				VSYNC_RD_MPEG_REG(VPP_DUMMY_DATA1);
			if (is_meson_txlx_cpu()) {
				vpp_data_conv_para0_backup =
					VSYNC_RD_MPEG_REG(VPP_DAT_CONV_PARA0);
				vpp_data_conv_para1_backup =
					VSYNC_RD_MPEG_REG(VPP_DAT_CONV_PARA1);
				dolby_tv_gate_ctrl_backup =
					VSYNC_RD_MPEG_REG(
					DOLBY_TV_CLKGATE_CTRL);
				dolby_core2_gate_ctrl_backup =
					VSYNC_RD_MPEG_REG(
					DOLBY_CORE2A_CLKGATE_CTRL);
				dolby_core3_gate_ctrl_backup =
					VSYNC_RD_MPEG_REG(
					DOLBY_CORE3_CLKGATE_CTRL);
				WRITE_VPP_REG(
					DOLBY_TV_CLKGATE_CTRL, 0x2800);
				WRITE_VPP_REG(
					DOLBY_CORE2A_CLKGATE_CTRL, 0);
				WRITE_VPP_REG(
					DOLBY_CORE3_CLKGATE_CTRL, 0);
			}
			if (is_meson_txlx_package_962X() && !force_stb_mode) {
				VSYNC_WR_MPEG_REG_BITS(
					VIU_MISC_CTRL1,
					/* vd2 to core1 */
					(0 << 1) |
					/* ! core1 bl bypass */
					(0 << 0),
					16, 2);
				if (dolby_vision_flags & FLAG_CERTIFICAION) {
					/* bypass dither/PPS/SR/CM, EO/OE */
					VSYNC_WR_MPEG_REG_BITS(
						VPP_DOLBY_CTRL, 3, 0, 2);
					/* bypass all video effect */
					video_effect_bypass(1);
					/* 12 bit unsigned to sign
					   before vadj1 */
					/* 12 bit sign to unsign
					   before post blend */
					VSYNC_WR_MPEG_REG(
						VPP_DAT_CONV_PARA0, 0x08000800);
					/* 12->10 before vadj2
					   10->12 after gainoff */
					VSYNC_WR_MPEG_REG(
						VPP_DAT_CONV_PARA1, 0x20002000);
					WRITE_VPP_REG(0x33e7, 0xb);
				} else {
					/* bypass all video effect */
					video_effect_bypass(1);
					/* 12->10 before vadj1
					   10->12 before post blend */
					VSYNC_WR_MPEG_REG(
						VPP_DAT_CONV_PARA0, 0x20002000);
					/* 12->10 before vadj2
					   10->12 after gainoff */
					VSYNC_WR_MPEG_REG(
						VPP_DAT_CONV_PARA1, 0x20002000);
				}
				crc_read_delay =
					(dolby_vision_flags >>
					FLAG_FRAME_DELAY_SHIFT)
					& FLAG_FRAME_DELAY_MASK;
				/* osd rgb to yuv, vpp out yuv to rgb */
				VSYNC_WR_MPEG_REG(VPP_MATRIX_CTRL, 0x81);
				pr_dolby_dbg("Dolby Vision TV core turn on\n");
			} else if (is_meson_txlx_package_962E()
				|| force_stb_mode) {
				osd_bypass(1);
				VSYNC_WR_MPEG_REG_BITS(VPP_DOLBY_CTRL,
					1, 3, 1); /* core3 enable */
				VSYNC_WR_MPEG_REG_BITS(VIU_MISC_CTRL1,
					(0 << 2) | /* ! core2 bypass */
					(0 << 1) | /* vd2 to core1 */
					(0 << 0),  /* ! core1 bypass */
					16, 3);
				if (dolby_vision_flags & FLAG_CERTIFICAION) {
					/* bypass dither/PPS/SR/CM
					   bypass EO/OE
					   bypass vadj2/mtx/gainoff */
					VSYNC_WR_MPEG_REG_BITS(
						VPP_DOLBY_CTRL, 7, 0, 3);
					/* bypass all video effect */
					video_effect_bypass(1);
					/* 12 bit unsigned to sign
					   before vadj1 */
					/* 12 bit sign to unsign
					   before post blend */
					VSYNC_WR_MPEG_REG(
						VPP_DAT_CONV_PARA0, 0x08000800);
					/* 12->10 before vadj2
					   10->12 after gainoff */
					VSYNC_WR_MPEG_REG(
						VPP_DAT_CONV_PARA1, 0x20002000);
				} else {
					/* bypass all video effect */
					video_effect_bypass(1);
					/* 12->10 before vadj1
					   10->12 before post blend */
					VSYNC_WR_MPEG_REG(
						VPP_DAT_CONV_PARA0, 0x20002000);
					/* 12->10 before vadj2
					   10->12 after gainoff */
					VSYNC_WR_MPEG_REG(
						VPP_DAT_CONV_PARA1, 0x20002000);
				}
				if (is_meson_txlx_package_962X())
					VSYNC_WR_MPEG_REG(VPP_MATRIX_CTRL, 1);
				else
					VSYNC_WR_MPEG_REG(VPP_MATRIX_CTRL, 0);
				VSYNC_WR_MPEG_REG(VPU_HDMI_FMT_CTRL, 0);
				pr_dolby_dbg("Dolby Vision STB cores turn on\n");
			} else {
				VSYNC_WR_MPEG_REG(VPP_DOLBY_CTRL,
					/* cm_datx4_mode */
					(0x0<<21) |
					/* reg_front_cti_bit_mode */
					(0x0<<20) |
					/* vpp_clip_ext_mode 19:17 */
					(0x0<<17) |
					/* vpp_dolby2_en */
					(0x1<<16) |
					/* mat_xvy_dat_mode */
					(0x0<<15) |
					/* vpp_ve_din_mode */
					(0x1<<14) |
					/* mat_vd2_dat_mode 13:12 */
					(0x1<<12) |
					/* vpp_dpath_sel 10:8 */
					(0x3<<8) |
					/* vpp_uns2s_mode 7:0 */
					0x1f);
				VSYNC_WR_MPEG_REG_BITS(VIU_MISC_CTRL1,
					/* 23-20 ext mode */
					(0 << 4) |
					/* 19 osd bypass */
					(0 << 3) |
					/* 18 core2 bypass */
					(0 << 2) |
					/* 17 core1 el bypass */
					(0 << 1) |
					/* 16 core1 bl bypass */
					(0 << 0),
					16, 8);
				video_effect_bypass(1);
				VSYNC_WR_MPEG_REG(VPU_HDMI_FMT_CTRL, 0);
				VSYNC_WR_MPEG_REG(VPP_MATRIX_CTRL, 0);
				VSYNC_WR_MPEG_REG(VPP_DUMMY_DATA1, 0x20000000);
				enable_osd_path(0);
				pr_dolby_dbg("Dolby Vision turn on\n");
			}
		}
		dolby_vision_on = true;
		dolby_vision_wait_on = false;
	} else {
		if (dolby_vision_on) {
			if (is_meson_txlx_package_962X() && !force_stb_mode) {
				VSYNC_WR_MPEG_REG_BITS(
					VIU_MISC_CTRL1,
					/* vd2 connect to vpp */
					(1 << 1) |
					/* 16 core1 bl bypass */
					(1 << 0),
					16, 2);
				pr_dolby_dbg("Dolby Vision TV core turn off\n");
			} else if (is_meson_txlx_package_962E()
			|| force_stb_mode) {
				VSYNC_WR_MPEG_REG_BITS(
					VIU_MISC_CTRL1,
					(1 << 2) |	/* core2 bypass */
					(1 << 1) |	/* vd2 connect to vpp */
					(1 << 0),	/* core1 bl bypass */
					16, 3);
				VSYNC_WR_MPEG_REG_BITS(VPP_DOLBY_CTRL,
					0, 3, 1);   /* core3 disable */
				osd_bypass(0);
				pr_dolby_dbg("Dolby Vision STB cores turn off\n");
			} else {
				enable_osd_path(1);
				pr_dolby_dbg("Dolby Vision turn off\n");
			}
			if (is_meson_txlx_cpu()) {
				VSYNC_WR_MPEG_REG(VPP_DAT_CONV_PARA0,
					vpp_data_conv_para0_backup);
				VSYNC_WR_MPEG_REG(VPP_DAT_CONV_PARA1,
					vpp_data_conv_para1_backup);
				VSYNC_WR_MPEG_REG(
					DOLBY_TV_CLKGATE_CTRL,
					dolby_tv_gate_ctrl_backup);
				VSYNC_WR_MPEG_REG(
					DOLBY_CORE2A_CLKGATE_CTRL,
					dolby_core2_gate_ctrl_backup);
				VSYNC_WR_MPEG_REG(
					DOLBY_CORE3_CLKGATE_CTRL,
					dolby_core3_gate_ctrl_backup);
			}
			video_effect_bypass(0);
			VSYNC_WR_MPEG_REG(VPP_DOLBY_CTRL,
				dolby_ctrl_backup);
			VSYNC_WR_MPEG_REG(VIU_MISC_CTRL1,
				viu_misc_ctrl_backup);
			VSYNC_WR_MPEG_REG(VPU_HDMI_FMT_CTRL,
				vpu_hdmi_fmt_backup);
			VSYNC_WR_MPEG_REG(VPP_MATRIX_CTRL,
				vpp_matrix_backup);
			VSYNC_WR_MPEG_REG(VPP_DUMMY_DATA1,
				vpp_dummy1_backup);
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

void dolby_vision_init_receiver(void *pdev)
{
	ulong alloc_size;

	pr_info("%s(%s)\n", __func__, DVEL_RECV_NAME);
	vf_receiver_init(&dvel_vf_recv, DVEL_RECV_NAME,
			&dvel_vf_receiver, &dvel_vf_recv);
	vf_reg_receiver(&dvel_vf_recv);
	pr_info("%s: %s\n", __func__, dvel_vf_recv.name);
	dovi_pdev = (struct platform_device *)pdev;
	alloc_size = dma_size;
	alloc_size = (alloc_size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
	dma_vaddr = dma_alloc_coherent(&dovi_pdev->dev,
		alloc_size, &dma_paddr, GFP_KERNEL);
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
	if (is_meson_txlx_package_962X() && !force_stb_mode)
		dump_tv_setting(&tv_dovi_setting, frame_count, debug_dolby);
	else
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

	if ((!dolby_vision_enable) || (!p_funcs))
		return mode_change;

	if (is_meson_txlx_package_962X() && !force_stb_mode) {
		if (dolby_vision_policy == DOLBY_VISION_FORCE_OUTPUT_MODE) {
			if (*mode == DOLBY_VISION_OUTPUT_MODE_BYPASS) {
				if (dolby_vision_mode !=
				DOLBY_VISION_OUTPUT_MODE_BYPASS) {
					pr_dolby_dbg("dovi tv output mode change %d -> %d\n",
						dolby_vision_mode, *mode);
					mode_change = 1;
				}
			} else if (*mode == DOLBY_VISION_OUTPUT_MODE_SDR8) {
				if (dolby_vision_mode !=
				DOLBY_VISION_OUTPUT_MODE_SDR8) {
					pr_dolby_dbg("dovi tv output mode change %d -> %d\n",
						dolby_vision_mode, *mode);
					mode_change = 1;
				}
			} else {
				pr_dolby_error(
					"not support dovi output mode %d\n",
					*mode);
				return mode_change;
			}
		} else if (DOLBY_VISION_FOLLOW_SINK) {
			if (src_format == FORMAT_DOVI) {
				if (dolby_vision_mode !=
				DOLBY_VISION_OUTPUT_MODE_SDR8) {
					pr_dolby_dbg("dovi tv output -> DOLBY_VISION_OUTPUT_MODE_SDR8\n");
					*mode = DOLBY_VISION_OUTPUT_MODE_SDR8;
					mode_change = 1;
				}
			} else {
				if (dolby_vision_mode !=
				DOLBY_VISION_OUTPUT_MODE_BYPASS) {
					pr_dolby_dbg("dovi tv output -> DOLBY_VISION_OUTPUT_MODE_BYPASS\n");
					*mode = DOLBY_VISION_OUTPUT_MODE_BYPASS;
					mode_change = 1;
				}
			}
		} else if (DOLBY_VISION_FOLLOW_SOURCE) {
			if ((src_format == FORMAT_DOVI)
			|| (src_format == FORMAT_HDR10)) {
				if (dolby_vision_mode !=
				DOLBY_VISION_OUTPUT_MODE_SDR8) {
					pr_dolby_dbg("dovi tv output -> DOLBY_VISION_OUTPUT_MODE_SDR8\n");
					*mode = DOLBY_VISION_OUTPUT_MODE_SDR8;
					mode_change = 1;
				}
			} else {
				if (dolby_vision_mode !=
				DOLBY_VISION_OUTPUT_MODE_BYPASS) {
					pr_dolby_dbg("dovi tv output -> DOLBY_VISION_OUTPUT_MODE_BYPASS\n");
					*mode = DOLBY_VISION_OUTPUT_MODE_BYPASS;
					mode_change = 1;
				}
			}
		}
		return mode_change;
	}

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

	if ((vf->source_type == VFRAME_SOURCE_TYPE_HDMI)
	&& is_meson_txlx_package_962X() && !force_stb_mode) {
		vf_notify_provider_by_name("dv_vdin",
			VFRAME_EVENT_RECEIVER_GET_AUX_DATA,
			(void *)&req);
		if (req.aux_buf && req.aux_size)
			return 1;
		else
			return 0;
	} else if (vf->source_type == VFRAME_SOURCE_TYPE_OTHERS) {
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
	if (crc_read_delay > 0) {
		crc_read_delay--;
		return 1;
	}
	crc_read_delay =
		(dolby_vision_flags >>
		FLAG_FRAME_DELAY_SHIFT)
		& FLAG_FRAME_DELAY_MASK;

	if (dolby_vision_flags & FLAG_CERTIFICAION
		&& crc_count > 0) {
		u32 crc = READ_VPP_REG(0x33ef);
		tv_dolby_vision_insert_crc(crc);
	}
	req.vf = vf;
	req.bot_flag = 0;
	req.aux_buf = NULL;
	req.aux_size = 0;
	req.dv_enhance_exist = 0;

	if (vf->source_type == VFRAME_SOURCE_TYPE_OTHERS)
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
			if ((mode != DOLBY_VISION_OUTPUT_MODE_BYPASS)
			&& (dolby_vision_mode ==
				DOLBY_VISION_OUTPUT_MODE_BYPASS))
				dolby_vision_wait_on = true;
#if 0
			dolby_vision_set_toggle_flag(1);
			pr_dolby_dbg("[dolby_vision_wait_metadata] output change from %d to %d\n",
				dolby_vision_mode, mode);
			dolby_vision_mode = mode;
			ret = 1;
#endif
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
				&& (dolby_vision_status == BYPASS_PROCESS)
				&& !is_dolby_vision_on())
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

			if (!p_funcs)
				return -1;
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
		if ((debug_dolby & 4) && dump_enable)  {
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

	struct vframe_master_display_colour_s *p_mdc;
	unsigned int current_mode = dolby_vision_mode;
	uint32_t target_lumin_max = 0;
	enum input_mode_e input_mode = INPUT_MODE_OTT;

	if (!dolby_vision_enable)
		return -1;

	if (vf) {
		w = (vf->type & VIDTYPE_COMPRESS) ?
			vf->compWidth : vf->width;
		h = (vf->type & VIDTYPE_COMPRESS) ?
			vf->compHeight : vf->height;
	}

	if (is_meson_txlx_package_962X()  && !force_stb_mode && vf
	&& (vf->source_type == VFRAME_SOURCE_TYPE_HDMI)) {
		req.vf = vf;
		req.bot_flag = 0;
		req.aux_buf = NULL;
		req.aux_size = 0;
		req.dv_enhance_exist = 0;
		vf_notify_provider_by_name("dv_vdin",
			VFRAME_EVENT_RECEIVER_GET_AUX_DATA,
			(void *)&req);
		input_mode = INPUT_MODE_HDMI;
		pr_dolby_dbg("vdin0 get aux data %p %x\n",
			req.aux_buf, req.aux_size);
		if (req.aux_buf && req.aux_size) {
			memcpy(md_buf, req.aux_buf, req.aux_size);
			src_format = FORMAT_DOVI;
		}
		total_md_size = req.aux_size;
		total_comp_size = 0;
		meta_flag_bl = 0;
		if (debug_dolby & 2)
			pr_dolby_dbg(
				"+++ get bl(%p-%lld) +++\n",
				vf, vf->pts_us64);
		dolby_vision_vf_add(vf, NULL);
	} else if (vf && (vf->source_type == VFRAME_SOURCE_TYPE_OTHERS)) {
		pr_dolby_dbg("frame %d pts %lld:\n", frame_count, vf->pts_us64);

		/* check source format */
		if (signal_transfer_characteristic == 16)
			video_is_hdr10 = true;
		else
			video_is_hdr10 = false;

		p_mdc =	&vf->prop.master_display_colour;
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
		input_mode = INPUT_MODE_OTT;
		pr_dolby_dbg("dvbldec get aux data %p %x\n",
			req.aux_buf, req.aux_size);
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
				el_flag = 1;
				if (vf->width == el_vf->width)
					el_halfsize_flag = 0;
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


	if ((src_format == FORMAT_DOVI)
	&& meta_flag_bl && meta_flag_el) {
		/* dovi frame no meta or meta error */
		/* use old setting for this frame   */
		return -1;
	}

	/* if not DOVI, release metadata_parser */
	if ((src_format != FORMAT_DOVI) && metadata_parser) {
		if (p_funcs)
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
		pr_dolby_dbg("[dolby_vision_parse_metadata] output change from %d to %d\n",
				dolby_vision_mode, current_mode);
		dolby_vision_mode = current_mode;
	}

	if (dolby_vision_mode == DOLBY_VISION_OUTPUT_MODE_BYPASS) {
		new_dovi_setting.video_width = 0;
		new_dovi_setting.video_height = 0;
		return -1;
	}

	if (!p_funcs)
		return -1;

	/* TV core */
	if (is_meson_txlx_package_962X() && !force_stb_mode) {
		tv_dovi_setting.video_width = w << 16;
		tv_dovi_setting.video_height = h << 16;
		flag = p_funcs->tv_control_path(
			src_format, input_mode,
			comp_buf, total_comp_size,
			md_buf, total_md_size,
			12, 0, SIG_RANGE_SMPTE, /* bit/chroma/range */
			&pq_config, &menu_param,
			(!el_flag) ||
			(dolby_vision_flags & FLAG_DISABLE_COMPOSER),
			&hdr10_param,
			&tv_dovi_setting);
		if (flag >= 0) {
			if (input_mode == INPUT_MODE_HDMI) {
				if (h > 1080)
					tv_dovi_setting.core1_reg_lut[1] =
						0x0000000100000043;
				else
					tv_dovi_setting.core1_reg_lut[1] =
						0x0000000100000042;
			} else {
				if (el_halfsize_flag)
					tv_dovi_setting.core1_reg_lut[1] =
						0x000000010000004c;
				else
					tv_dovi_setting.core1_reg_lut[1] =
						0x0000000100000044;
			}
			/* enable CRC */
			if (dolby_vision_flags & FLAG_CERTIFICAION)
				tv_dovi_setting.core1_reg_lut[3] =
					0x000000ea00000001;
			tv_dovi_setting.src_format = src_format;
			tv_dovi_setting.el_flag = el_flag;
			tv_dovi_setting.el_halfsize_flag = el_halfsize_flag;
			tv_dovi_setting.video_width = w;
			tv_dovi_setting.video_height = h;
			tv_dovi_setting.input_mode = input_mode;
			if (el_flag)
				pr_dolby_dbg("tv setting %s-%d: flag=%02x,md_%s=%d,comp=%d\n",
					input_mode == INPUT_MODE_HDMI ?
					"hdmi" : "ott",
					src_format,
					flag, meta_flag_bl ? "el" : "bl",
					total_md_size, total_comp_size);
			else
				pr_dolby_dbg("tv setting %s-%d: flag=%02x,md_%s=%d\n",
					input_mode == INPUT_MODE_HDMI ?
					"hdmi" : "ott",
					src_format,
					flag, meta_flag_bl ? "el" : "bl",
					total_md_size);
			tv_dovi_setting_update_flag = true;
			dump_tv_setting(&tv_dovi_setting,
				frame_count, debug_dolby);
			return 0; /* setting updated */
		} else {
			tv_dovi_setting.video_width = 0;
			tv_dovi_setting.video_height = 0;
			pr_dolby_error("tv_control_path() failed\n");
		}
		return -1;
	}

	/* STB core */
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

	if (!is_meson_gxm_cpu() && !is_meson_txlx_cpu())
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
	}

	if (!vf) {
		if (dolby_vision_flags & FLAG_TOGGLE_FRAME)
			dolby_vision_parse_metadata(NULL);
	}

	if (dolby_vision_mode == DOLBY_VISION_OUTPUT_MODE_BYPASS) {
		if (dolby_vision_status != BYPASS_PROCESS) {
			enable_dolby_vision(0);
			if (!is_meson_txlx_package_962X() && !force_stb_mode)
				send_hdmi_pkt(FORMAT_SDR, vinfo);
			if (dolby_vision_flags & FLAG_TOGGLE_FRAME)
				dolby_vision_flags &= ~FLAG_TOGGLE_FRAME;
		}
		return 0;
	}

	if (is_dolby_vision_on())
		video_effect_bypass(1);

	if (dolby_vision_flags & FLAG_RESET_EACH_FRAME)
		dolby_vision_set_toggle_flag(1);

	if (dolby_vision_flags & FLAG_TOGGLE_FRAME) {
		if (is_meson_txlx_package_962X() && !force_stb_mode) {
			if ((tv_dovi_setting.video_width & 0xffff)
			&& (tv_dovi_setting.video_height & 0xffff)) {
				tv_dolby_core1_set(
					tv_dovi_setting.core1_reg_lut,
					tv_dovi_setting.video_width,
					tv_dovi_setting.video_height,
					1, /* BL enable */
					tv_dovi_setting.el_flag, /* EL enable */
					tv_dovi_setting.el_halfsize_flag,
					tv_dovi_setting.input_mode ==
					INPUT_MODE_HDMI
				);
				enable_dolby_vision(1);
				tv_dovi_setting.video_width = 0;
				tv_dovi_setting.video_height = 0;
			}
		} else {
			if ((new_dovi_setting.video_width & 0xffff)
			&& (new_dovi_setting.video_height & 0xffff)) {
				memcpy(&dovi_setting, &new_dovi_setting,
					sizeof(dovi_setting));
				new_dovi_setting.video_width =
				new_dovi_setting.video_height = 0;
			}
			if (is_meson_txlx_package_962E() || force_stb_mode)
				stb_dolby_core1_set(
					27, 173, 256 * 5,
					(uint32_t *)&dovi_setting.dm_reg1,
					(uint32_t *)&dovi_setting.comp_reg,
					(uint32_t *)&dovi_setting.dm_lut1,
					dovi_setting.video_width,
					dovi_setting.video_height,
					1, /* BL enable */
					dovi_setting.el_flag, /* EL enable */
					dovi_setting.el_halfsize_flag,
					dolby_vision_mode ==
					DOLBY_VISION_OUTPUT_MODE_IPT_TUNNEL,
					dovi_setting.src_format == FORMAT_DOVI,
					1);
			else
				dolby_core1_set(
					24, 173, 256 * 5,
					(uint32_t *)&dovi_setting.dm_reg1,
					(uint32_t *)&dovi_setting.comp_reg,
					(uint32_t *)&dovi_setting.dm_lut1,
					dovi_setting.video_width,
					dovi_setting.video_height,
					1, /* BL enable */
					dovi_setting.el_flag, /* EL enable */
					dovi_setting.el_halfsize_flag,
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
			/* send HDMI packet according to dst_format */
			send_hdmi_pkt(dovi_setting.dst_format, vinfo);
		}
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
	if ((is_meson_gxm_cpu() || is_meson_txlx_cpu())
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

bool is_dolby_vision_stb_mode(void)
{
	return force_stb_mode ||
	is_meson_txlx_package_962E() ||
	is_meson_gxm_cpu();
}
EXPORT_SYMBOL(is_dolby_vision_stb_mode);

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

static uint64_t dma_data_tv[3754] = {
	0x0000000000000456, 0x0000000100000040,
	0x0000000200000001, 0x0000000600003fff,
	0x0000000600003fff, 0x0000000700010004,
	0x0000000800000000, 0x0000000900000000,
	0x0000000a00000000, 0x0000000b00002567,
	0x0000000c25673996, 0x0000000deee1f926,
	0x0000000e43dc2567, 0x0000000f000d0000,
	0x0000001007c85c00, 0x00000011fd967c00,
	0x0000001209111c00, 0x0000001300000000,
	0x0000001400000000, 0x0000001500000000,
	0x0000001600000000, 0x000000174ac92c3d,
	0x00000018142c08f9, 0x000000190e4a5d8b,
	0x0000001a08850150, 0x0000001b000f762b,
	0x0000001c06660666, 0x0000001d47480333,
	0x0000001e0656b262, 0x0000001f05b70ce4,
	0x00000020000ced65, 0x0000002100000000,
	0x0000002200000000, 0x000000230000000c,
	0x0000002410003ffc, 0x0000002500001fff,
	0x0000002600000fff, 0x00000027c00001f0,
	0x00000028ffffc000, 0x000000294000569e,
	0x0000002a0000c17b, 0x0000002b00008000,
	0x0000002c00008000, 0x0000002d00000000,
	0x0000002e00000000, 0x0000002f0043777f,
	0x0000003000000000, 0x0000003100000000,
	0x0000003200000000, 0x0000003300870780,
	0x0000003400000eaa, 0x0000003500000017,
	0x0000003600000009, 0x000000370001a800,
	0x000000380004f8d4, 0x00000039000849a8,
	0x0000003a000b9a7c, 0x0000003b000003ff,
	0x0000003c00006aaa, 0x0000003d0005f13c,
	0x0000003e0025bffd, 0x0000003f3fe76fdc,
	0x0000004000063f31, 0x000000410020aae1,
	0x000000423ffc1dbd, 0x000000430006f200,
	0x00000044001a277c, 0x00000045000b47d7,
	0x000000460008f602, 0x00000047000e13a5,
	0x00000048001d455d, 0x00000049000f7bf2,
	0x0000004a3ff09d7f, 0x0000004b003e6025,
	0x0000004c00219aae, 0x0000004d3faef56b,
	0x0000004e007996a1, 0x0000004f0099a20a,
	0x000000503e3c9b00, 0x000000510196bac2,
	0x0000005200281aeb, 0x0000005300000000,
	0x0000005400000000, 0x000000550000006d,
	0x0000005600000f60, 0x000000570036e755,
	0x0000005800000000, 0x0000005900000001,
	0x0000005a00000002, 0x0000005b000ffc00,
	0x0000005c00000000, 0x0000005d00000000,
	0x0000005e00000000, 0x0000005f00000000,
	0x0000006000000000, 0x0000006100000000,
	0x0000006200000000, 0x0000006300000000,
	0x0000006400000000, 0x0000006500000000,
	0x0000006600000000, 0x0000006700000000,
	0x0000006800000000, 0x0000006900000000,
	0x0000006a00000000, 0x0000006bf7b95b00,
	0x0000006c000000ff, 0x0000006d02273190,
	0x0000006e00000000, 0x0000006f18b122e0,
	0x0000007000000000, 0x00000071139c0d40,
	0x0000007200000000, 0x00000073fe1399f6,
	0x00000074000000ff, 0x00000075fb54e0b8,
	0x00000076000000ff, 0x00000077dfbc1380,
	0x00000078000000ff, 0x00000079049ec5c8,
	0x0000007a00000000, 0x0000007bff0aed97,
	0x0000007c000000ff, 0x0000007de95b82c0,
	0x0000007e000000ff, 0x0000007ff27426b0,
	0x00000080000000ff, 0x00000081fe7c771c,
	0x00000082000000ff, 0x0000008306291520,
	0x0000008400000000, 0x000000851d136ac0,
	0x0000008600000000, 0x00000087fc7d81cc,
	0x00000088000000ff, 0x0000008900167229,
	0x0000008a00000000, 0x0000008b09436580,
	0x0000008c00000000, 0x0000008d04189e78,
	0x0000008e00000000, 0x0000008f042e1140,
	0x0000009000000000, 0x00000091fb26af18,
	0x00000092000000ff, 0x0000009300000000,
	0x0000009400000000, 0x0000009500000000,
	0x0000009600000000, 0x0000009700000003,
	0x00000098000001fb, 0x000000990000062c,
	0x0000009a0005d046, 0x0000009b00000000,
	0x0000009c00000001, 0x0000009d00000002,
	0x0000009e000ffc00, 0x0000009f00000000,
	0x000000a000000000, 0x000000a100000000,
	0x000000a200000000, 0x000000a300000000,
	0x000000a400000000, 0x000000a500000000,
	0x000000a600000000, 0x000000a700000000,
	0x000000a800000000, 0x000000a900000000,
	0x000000aa00000000, 0x000000ab00000000,
	0x000000ac00000000, 0x000000ad00000000,
	0x000000ae04365c20, 0x000000af00000000,
	0x000000b0f9a59500, 0x000000b1000000ff,
	0x000000b2f57ee790, 0x000000b3000000ff,
	0x000000b4f81a23e8, 0x000000b5000000ff,
	0x000000b604885c88, 0x000000b700000000,
	0x000000b808876fd0, 0x000000b900000000,
	0x000000ba11f3a420, 0x000000bb00000000,
	0x000000bcff3b3b1d, 0x000000bd000000ff,
	0x000000be0719de78, 0x000000bf00000000,
	0x000000c004f0b720, 0x000000c100000000,
	0x000000c201699936, 0x000000c300000000,
	0x000000c4f629a930, 0x000000c5000000ff,
	0x000000c6ecb89b60, 0x000000c7000000ff,
	0x000000c8edd8ec40, 0x000000c9000000ff,
	0x000000ca02a2e668, 0x000000cb00000000,
	0x000000ccfce59974, 0x000000cd000000ff,
	0x000000ce00d3bad7, 0x000000cf00000000,
	0x000000d00192c59a, 0x000000d100000000,
	0x000000d2074de818, 0x000000d300000000,
	0x000000d411bee5c0, 0x000000d500000000,
	0x000000d600000000, 0x000000d700000000,
	0x000000d800000000, 0x000000d900000000,
	0x000000da00000003, 0x000000db0000022e,
	0x000000dc00000627, 0x000000dd000ae467,
	0x000000de00000000, 0x0000000300000001,

	0x0003e03e0003e03e, 0x0003e03e0003e03e,
	0x000470430003e03e, 0x0005d0570005204c,
	0x0007506f00069063, 0x0008f0890008207c,
	0x000ab0a40009d096, 0x000c60bf000b80b1,
	0x000e30db000d40cd, 0x000ff0f8000f10ea,
	0x0011c1150010e106, 0x0013a1320012b124,
	0x0015715000149141, 0x0017516e0016615f,
	0x0019418c0018517d, 0x001b21aa001a319b,
	0x001d11c9001c11ba, 0x001f01e8001e01d8,
	0x0020f207001ff1f7, 0x0022e2260021e216,
	0x0024d2450023e236, 0x0026d2650025d255,
	0x0028c2840027d275, 0x002ac2a40029c294,
	0x002cc2c4002bc2b4, 0x002ec2e4002dc2d4,
	0x0030c304002fc2f4, 0x0032c3240031c314,
	0x0034c3440033c334, 0x0036c3640035c354,
	0x0038c3840037c374, 0x003ad3a50039d395,
	0x003cd3c5003bd3b5, 0x003ed3e5003dd3d5,
	0x0040e405003fd3f5, 0x0042e4260041e416,
	0x0044e4460043e436, 0x0046e4660045e456,
	0x0048e4860047e476, 0x004af4a70049f496,
	0x004cf4c7004bf4b7, 0x004ef4e7004df4d7,
	0x0050e506004ff4f7, 0x0052e5260051e516,
	0x0054e5460053e536, 0x0056d5660055e556,
	0x0058d5850057d575, 0x005ac5a40059c595,
	0x005cb5c3005bc5b4, 0x005ea5e2005db5d3,
	0x00609601005fa5f2, 0x0062862000618611,
	0x0064663e0063762f, 0x0066465d0065564d,
	0x0068267b0067366c, 0x006a06980069168a,
	0x006bd6b6006af6a7, 0x006db6d3006cc6c5,
	0x006f86f0006e96e2, 0x0071470d007066ff,
	0x0073172a0072371b, 0x0074d7460073f738,
	0x007697620075b754, 0x0078577e00777770,
	0x007a07990079278b, 0x007bb7b4007ad7a7,
	0x007d67cf007c87c2, 0x007f07e9007e37dc,
	0x0080a804007fd7f7, 0x0082481d00817810,
	0x0083d8370083082a, 0x008568500084a843,
	0x0086f8690086385c, 0x008878810087b875,
	0x0089f8990089388d, 0x008b78b1008ab8a5,
	0x008ce8c8008c38bd, 0x008e58df008da8d4,
	0x008fc8f6008f08eb, 0x0091290c00907901,
	0x009289220091d917, 0x0093d9380093292d,
	0x0095294d00948942, 0x009679620095d958,
	0x0097b9760097196c, 0x0098f98b00986980,
	0x009a399e00999994, 0x009b69b2009ad9a8,
	0x009c99c5009c09bb, 0x009dc9d7009d39ce,
	0x009ee9ea009e59e1, 0x00a009fc009f79f3,
	0x00a12a0d00a09a05, 0x00a23a1f00a1aa16,
	0x00a34a3000a2ba27, 0x00a44a4000a3ca38,
	0x00a55a5000a4ca48, 0x00a64a6000a5ca59,
	0x00a74a7000a6ca68, 0x00a83a7f00a7ca78,
	0x00a92a8e00a8ba87, 0x00aa1a9d00a99a96,
	0x00aafaab00aa8aa4, 0x00abdab900ab6ab2,
	0x00acbac700ac4ac0, 0x00ad3ad300ad1ace,
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
	0x00001f4b0000051f, 0x0000b12c000056cb,
	0x0001dfb3000132e0, 0x0003c7c10002bafc,
	0x00068094000508c4, 0x000a1e0600083195,
	0x000eb1a5000c480a, 0x00144b4600115cc6,
	0x001af96c00177eeb, 0x0022c98b001ebc6e,
	0x002bc8350027224d, 0x003601400030bcb8,
	0x00417fdd003b972f, 0x004e4eb40047bc9c,
	0x005c77ed00553766, 0x006c05460064117e,
	0x007d00180074546e, 0x008f716500860963,
	0x00a361de00993934, 0x00b8d9ea00adec6e,
	0x00cfe1ae00c42b55, 0x00e8811000dbfdef,
	0x0102bfbf00f56c06, 0x011ea53301107d2b,
	0x013c38b2012d38bf, 0x015b8156014ba5f0,
	0x017c860d016bcbc2, 0x019f4d9b018db10e,
	0x01c3de9f01b15c86, 0x01ea3f9501d6d4b6,
	0x021276d501fe2008, 0x023c8a98022744c4,
	0x026880f802524914, 0x02965ff1027f3302,
	0x02c62d6502ae087f, 0x02f7ef1b02decf5d,
	0x032baac103118d56, 0x036165ec0346480d,
	0x0399261a037d050c, 0x03d2f0b503b5c9c4,
	0x040ecb0f03f09b95, 0x044cba67042d7fc8,
	0x048cc3ea046c7b91, 0x04ceecae04ad9413,
	0x051339bc04f0ce5d, 0x0559b00805362f6c,
	0x05a25477057dbc2e, 0x05ed2bdc05c7797e,
	0x063a3afb06136c28, 0x0689868b066198ea,
	0x06db133006b20471, 0x072ee5840704b35b,
	0x078502110759aa3b, 0x07dd6d5407b0ed94,
	0x08382bbd080a81dd, 0x089541af08666b7f,
	0x08f4b38308c4aed8, 0x0956858309255039,
	0x09babbf0098853e9, 0x0a215afe09edbe20,
	0x0a8a66d70a55930f, 0x0af5e39b0abfd6da,
	0x0b63d55e0b2c8d9b, 0x0bd4402b0b9bbb63,
	0x0c4728030c0d6436, 0x0cbc90de0c818c11,
	0x0d347ea90cf836e7, 0x0daef54a0d7168a1,
	0x0e2bf89d0ded2520, 0x0eab8c750e6b703b,
	0x0f2db49d0eec4dc3, 0x0fb274d80f6fc17c,
	0x1039d0e00ff5cf27, 0x10c3cc67107e7a79,
	0x11506b1a1109c721, 0x11dfb09a1197b8c6,
	0x1271a08412285309, 0x13063e6c12bb9980,
	0x139d8de013518fbc, 0x1437926613ea3949,
	0x14d44f7e148599a8, 0x1573c8a01523b457,
	0x1616014015c48cc9, 0x16bafcc816682670,
	0x1762be9f170e84b3, 0x180d4a2417b7aaf6,
	0x18baa2b018639c93, 0x196acb9619125ce3,
	0x1a1dc82519c3ef34, 0x1ad39ba41a7856d2,
	0x1b8c49551b2f9702, 0x1c47d4741be9b303,
	0x1d0640391ca6ae0e, 0x1dc78fd51d668b59,
	0x1e8bc6761e294e13, 0x1f52e7421eeef964,
	0x201cf55b1fb79073, 0x20e9f3e02083165f,
	0x21b9e5e721518e42, 0x228cce852222fb32,
	0x2362b0c922f76041, 0x243b8fbb23cec07c,
	0x25176e6324a91ee8, 0x25f64fc025867e8b,
	0x26d836d02666e262, 0x27bd268a274a4d68,
	0x28a521e12830c293, 0x29902bc6291a44d3,
	0x2a7e47232a06d717, 0x2b6f76e02af67c47,
	0x2c63bddf2be93749, 0x2d5b1f002cdf0afe,
	0x2e559d1e2dd7fa42, 0x2f533b0f2ed407ee,
	0x3053fba62fd336d9, 0x3157e1b530d589d2,
	0x325ef00531db03a8, 0x3369295f32e3a724,
	0x3476908833ef770e, 0x3587284034fe7626,
	0x369af3443610a72d, 0x37b1f44e37260cdd,
	0x38cc2e15383ea9ef, 0x39e9a34b395a8117,
	0x3b0a56a03a799507, 0x3c2e4ac03b9be86c,
	0x3d5582533cc17df0, 0x3e8000003dea583c,

	0x68f214c83f8cd142, 0x0000084691e15484,
	0x199015883c95315a, 0x000007d49df14382,
	0x6a6e1577cda2314d, 0x000007bfac01637c,
	0xeb7a17d7b7b1916f, 0x0000073db8b22a7a,
	0xfbeb3366e2bb42b6, 0x00000638c203bd68,
	0xac5c4f95d7c4a452, 0x000008468e214956,
	0x593c16185190b156, 0x0000085297516a85,
	0xca021728449b7170, 0x000007f5a5c16982,
	0x6b1d1707bfabd163, 0x000007adb8517f7b,
	0x7c721a07a2bf718f, 0x0000078bcf61b379,
	0x3d38361702d18285, 0x000005f1d6142967,
	0xd8f5150567d574fe, 0x0000085a92215e84,
	0x599617886395716c, 0x000008619dd18386,
	0xba87198858a2d18e, 0x00000833aed1a084,
	0x3bed18d80ab601a3, 0x00000797c711a07a,
	0xcd981ca78ad001b4, 0x0000076de3c1e177,
	0x9e3f35875eee31f8, 0x000005aed9349067,
	0x3937166854906156, 0x0000086f97117686,
	0x9a001958759b4185, 0x0000087ba551a687,
	0xbb211cb87cab61b7, 0x00000879b981e087,
	0xccb020d874c1d1f6, 0x0000085fd5122586,
	0xfec024a849e0023a, 0x000007c5ee722981,
	0x6e3c35d75beff1fc, 0x0000085991615c67,
	0x998717f86a94916d, 0x000008849ce19187,
	0x8a7a1b988ea1f1a4, 0x000008a3ae01d189,
	0xdbd12098afb521ec, 0x000008cbc5e22a8b,
	0xcda62758dccfa24e, 0x000008fde6029e8e,
	0x4e932c1918e992b5, 0x00000942e8f2c793,
	0xe923160934e932c1, 0x0000087195917385,
	0x09e519b88299a186, 0x000008a0a391b189,
	0x4b041e78b1a991ca, 0x000008d9b7c2078c,
	0xec982538f2c0222b, 0x0000092ed3d27f90,
	0x9e832de94fdf32b0, 0x000009b4e752f697,
	0x2e5c3249eee6830e, 0x00000a46e54333a2,
	0x596617786292e164, 0x000008899a918c87,
	0xfa4e1bc89b9f61a3, 0x000008c5ab01d88a,
	0xcb9d21d8dfb201f8, 0x0000091ec282468f,
	0x0d712a7944cc4274, 0x0000099fe302e097,
	0xee593299e0e6b309, 0x00000a7de4734aa2,
	0xce26385acbe3536a, 0x00000864936166b0,
	0xe9b319187896f17a, 0x000008a3a021a988,
	0x6ac11e38bba5c1c4, 0x000008f5b332068d,
	0x2c4225b919bb322e, 0x00000971ce128d94,
	0xfe563049a5d932c6, 0x00000a32e5832b9d,
	0xfe2d379a8ee43351, 0x00000b4fe173a1ae,
	0x693a168ba8e033c6, 0x0000087a97417c86,
	0x8a091ac8919b9193, 0x000008c4a631c98a,
	0x6b3b20f8e3ac81ea, 0x00000930bbc23a90,
	0x3cec29f95ec4c26a, 0x000009cfd9f2db99,
	0x0e4a344a12e6031d, 0x00000ad8e3236fa7,
	0x4e003cbb45e1939d, 0x00000c1ede83f7bb,
	0xb97517c86693c168, 0x000008929ba19487,
	0x9a621cc8aca091ae, 0x000008ebac71ee8c,
	0x0bb8241913b39215, 0x00000973c4727294,
	0xdd972e89acce62a9, 0x00000a36e5732c9e,
	0xbe27385a9ce40356, 0x00000b80e0c3b6b0,
	0x1dd5419bfadf03e8, 0x0000086693a168c7,
	0x29b619387a97317b, 0x000008ada031ad89,
	0x0abd1ee8cca5a1cc, 0x0000091ab2b2168f,
	0x0c3227494aba7242, 0x000009bdccc2ac98,
	0xde35331a01d782eb, 0x00000ab4e3b361a4,
	0x2e043c3b27e20390, 0x00000c22de73f9ba,
	0x4934166ca0dca42d, 0x0000087796b17986,
	0xb9f81aa88f9ad190, 0x000008cca4b1c98a,
	0xdb132128f1aa91eb, 0x0000094eb8a23f91,
	0x4ca12a8986c0e270, 0x00000a0ad432e69c,
	0xae39363a58df632a, 0x00000b2ee1f393ab,
	0xcde43fdbaae023c7, 0x00000caedc7433c2,
	0x396117586192b163, 0x0000088b99f18b87,
	0x9a351c38a79e61a5, 0x000008efa8e1e58c,
	0xdb6023791baf120b, 0x00000985bdc26794,
	0xacfc2d89c3c6529c, 0x00000a58da231aa0,
	0x1e2238eab0e3c35f, 0x00000b9ce063c1b2,
	0xddcb42cc1cde83f6, 0x0000085d92015fc9,
	0x598d18586e952170, 0x000008a29cf19f88,
	0xaa6a1dc8c3a181bb, 0x00000915ac62018e,
	0xeb9d258947b2c22a, 0x000009bcc1a28b97,
	0xcd39302a00ca42c4, 0x00000a9fdda346a4,
	0x8e0e3b2b03e28382, 0x00000bf3df13e5b7,
	0x791115ac70dd5419, 0x0000086894016a85,
	0xa9b219687d97617e, 0x000008bb9f51b189,
	0xca931f38e1a401d0, 0x0000093caef21a90,
	0xcbc4274971b54245, 0x000009eec3e2a99a,
	0x6d4f320a36cc32e2, 0x00000adcde5363a8,
	0x5e003ccb42e1a39c, 0x00000c2bde53fcbb,
	0x092a1628518ff154, 0x0000087595b17586,
	0x19ce1a589199118c, 0x000008d6a101c28b,
	0xcaab2068ffa591e3, 0x0000095fb0422e92,
	0x4bcf289997b64259, 0x00000a18c412bc9d,
	0x2d39330a62cba2f4, 0x00000b08db936fab,
	0x1df93d7b66e123ab, 0x0000084a8ec14dbd,
	0xb93d16b85791215a, 0x0000088796d18086,
	0x89dc1b38a59a2198, 0x000008efa1b1d08c,
	0x9aad21491aa611f0, 0x0000097cb0023b94,
	0x2bb92929b5b59265, 0x00000a35c1e2c49f,
	0xbcf132fa7dc872f8, 0x00000b1dd55367ac,
	0x78b713ab72da739d, 0x000008398de14383,
	0xa94914183190e148, 0x000007db98c13780,
	0xfa1314a7d59cc140, 0x000007c8a611557c,
	0xda9125c761a661f3, 0x000006dcac92c071,
	0xeb4939a698b06329, 0x000005feb8c41664,
	0xfc0553c5a4bcd4a1, 0x000004ccc265ed53,
	0x48f314b83d8c9141, 0x0000084192615484,
	0xd9b214682d964156, 0x000007d19ff1487e,
	0x1aaa1607c9a50153, 0x000007b8b0c16e7c,
	0x3b7626c786b571bb, 0x000006bebbc2ee71,
	0xbc5e401668c0b373, 0x000005a4cb04a160,
	0x0cbc61752ecf7555, 0x000008438d91464b,
	0x093d15f84e906153, 0x0000084697e16685,
	0xca251628309ca16a, 0x000007c4a8a15b7f,
	0x0b5f1797bbaef16a, 0x000007a5bdb18b7b,
	0xbcfa1b3798c6419e, 0x000006c2cff2e978,
	0xedae45f64ad573a0, 0x00000555d4751a5c,
	0x88e814b4c4ccc5f9, 0x0000085691815a84,
	0xb99717485d953168, 0x000008539e617f85,
	0xdaa7191845a40189, 0x000007ffb1e19282,
	0xdc361987a9bac184, 0x0000078ecd01ad79,
	0xee311df77fd781c5, 0x0000075cef81fb76,
	0xad9d47e665e2e377, 0x00000503d025985b,
	0xd92816184d8f5150, 0x0000086796617185,
	0xea0019086c9ae180, 0x0000086fa5d1a186,
	0xdb3d1ca86fac71b5, 0x0000086bbc21e186,
	0x1cfe216867c571fa, 0x00000859db623486,
	0xfecb25b84ae81253, 0x00000820ed224e83,
	0x9d734ca71deae290, 0x0000085190015458,
	0x0976178863936166, 0x0000087a9c118a87,
	0x0a771b5884a1619e, 0x0000089cae41cf89,
	0xdbe820e8abb5f1ec, 0x000008d1c832338b,
	0x3dee28c8e9d2f25d, 0x00000923e962ba90,
	0xbe7f2e5957e8b2d0, 0x000009bae742f998,
	0x49091579dce6c307, 0x0000086794016a85,
	0x69d019287898217d, 0x00000897a271aa88,
	0x0afb1e48aaa8a1c5, 0x000008dab792078c,
	0xcca725c8f9c0722f, 0x00000944d5928f91,
	0x0e762f4972e1f2c8, 0x000009ffe643159b,
	0x4e3e35aa51e51337, 0x00000af1e2d37aaa,
	0xb94816d857910159, 0x0000087d98c18286,
	0x6a331b38909da199, 0x000008c0a981d18a,
	0x0b8a21b8ddb0a1f4, 0x00000929c1c24890,
	0xbd752b5957cbf27b, 0x000009c7e3f2f598,
	0xce47349a1be5e321, 0x00000ae5e2f375a7,
	0xcdfe3ceb51e173a2, 0x0000085891415bbb,
	0x199118486d94d16e, 0x000008989e019d88,
	0x1a9e1da8b2a391b9, 0x000008f4b101ff8d,
	0xec2325b91eb9222a, 0x00000985cc729294,
	0xae4a3159c3d7e2d0, 0x00000a6ce4b343a0,
	0x5e163a3adce31371, 0x00000bd4df93d8b5,
	0x991615bc54ddb40e, 0x0000086d94f16f85,
	0xd9e019f883992186, 0x000008baa381bd89,
	0x6b0d2078dea9c1e0, 0x00000935b8d23490,
	0x8cbf2a096bc1d267, 0x000009eed742e19a,
	0x7e3e35ba3ce3d32a, 0x00000b21e2238eaa,
	0x2de3400ba6e033c5, 0x00000cc2dc243bc3,
	0xc94d16e85891415b, 0x0000088498f18586,
	0x0a311bf89f9db1a0, 0x000008e6a931e28c,
	0x5b7d238912b0120a, 0x0000097fc0926d94,
	0xbd562e99c1ca62a7, 0x00000a5fe18333a0,
	0xee173a0acee3536b, 0x00000bd9df73dab4,
	0x7db2458c6edd5418, 0x0000085791015ad0,
	0x398818386a94716c, 0x000008a09d119e88,
	0xca811e18c3a241bd, 0x0000091aaeb20a8e,
	0xcbe726c94fb62238, 0x000009d0c7d2a798,
	0x5ddc332a1ed242e9, 0x00000ae0e31373a7,
	0x0df23e4b63e133a9, 0x00000c89dcf423bf,
	0x590a157d26dab465, 0x0000086793e16985,
	0xf9c219b88097c180, 0x000008c3a101ba89,
	0xdacc2058eda691dd, 0x00000953b3c23391,
	0x7c4529f991bb9266, 0x00000a25ce02e09d,
	0x0e31373a7dd8c327, 0x00000b61e133a9ae,
	0x5dd0422beedf33e3, 0x00000d22dac463c8,
	0x3932165851901154, 0x0000087c96d17b86,
	0x19f71b489c9ae196, 0x000008eba491d78c,
	0x2b0d22a91baa51fe, 0x00000990b7f25b95,
	0x2c8e2cf9d5c00292, 0x00000a78d29313a2,
	0xbe1839fad7dd335e, 0x00000bd3df93d8b4,
	0xcdb5453c64dd8414, 0x0000084d8f5150cf,
	0x795917585e923160, 0x0000089799518f87,
	0x6a231cd8bc9d81ac, 0x00000916a771f38e,
	0x8b3c24b94cad521c, 0x000009cbbaf27e98,
	0x8cb82f7a15c2e2b8, 0x00000ac3d4e33ca6,
	0x3e043c4b28dee388, 0x00000c2cde43fdba,
	0x88e614bcbadc4438, 0x0000085891115a84,
	0x097918787094216e, 0x000008b59b51a389,
	0xca431e58de9f81c2, 0x00000940a9620c90,
	0x9b56266979af2237, 0x00000a00bc529b9b,
	0x3cbe313a4ec3d2d4, 0x00000b01d47357aa,
	0xedf63dcb67dd43a0, 0x00000c60dd9413bd,
	0x18fd1538428d6145, 0x0000086992916685,
	0xc98e19788895917d, 0x000008d49c91b48a,
	0x0a521f8900a0a1d4, 0x00000966aa121f93,
	0x3b562799a2af724a, 0x00000a2abbc2ac9e,
	0xdc9a320a78c282e4, 0x00000b2ad0f360ac,
	0x4de33e3b8cd803a2, 0x0000082b89e130bf,
	0xa8fd12b8268c6135, 0x000007e393412b7f,
	0x297e1907de96d133, 0x000007659971ed7a,
	0x59e929a72e9bd242, 0x000006b8a1c2f86f,
	0xca923d0675a5535f, 0x000005dbad444c62,
	0xdb59571581b174d6, 0x000004adb9561d51,
	0x28ac136430bbf6dc, 0x000008328d613e83,
	0x095312f81d90d13e, 0x000007db9921387e,
	0xf9f61ac7d49da142, 0x00000747a1821c78,
	0x3a8a2e7706a4d27f, 0x0000067aad03566c,
	0x2b6e45a62bb1c3d1, 0x0000056fbc34f35d,
	0x1c666604ffc1859e, 0x000003fdc2372c48,
	0xc8e61458378b913b, 0x0000083191f14b83,
	0x79b513d7ff967141, 0x000007d0a051487d,
	0xaaa319b7c8a5e155, 0x00000733ac323a79,
	0x5b5832e6e5b072b3, 0x0000063fbb43b269,
	0x5c804e95e0c17444, 0x000004fbce95a557,
	0xfc18741480c92662, 0x0000083c8c513f3e,
	0x192f1558458f414c, 0x0000082c97715984,
	0xca2c14e7f19cf14e, 0x000007c3a8e15c7c,
	0xeb7617d7b9afb16b, 0x00000741baf2257a,
	0x5c6035e6d7bfb2c8, 0x0000060dcd23fe67,
	0x6d2056259ad4f4af, 0x000004a2caf62d52,
	0x08cf143401c27725, 0x0000084c90115284,
	0x798716884e93f15e, 0x000008379db17084,
	0xeab516881ba3c175, 0x000007b5b2f1737c,
	0xbc4b19b7a8bb4185, 0x0000078bcf31b279,
	0x8db335970dd53274, 0x000005fbdd541a67,
	0xccee5bc57fd6b4da, 0x00000437c556d14e,
	0x290c1578438d9146, 0x0000085994c16585,
	0xc9ea18385b996174, 0x0000085ba4a19585,
	0x9b331be85aab81a8, 0x00000859bbf1d885,
	0xdd0c21785ac5c1f6, 0x00000862dd123d85,
	0x3ebb277868eab268, 0x00000770db345688,
	0x8c576cd6cdd0f57f, 0x000008468e014960,
	0x295616b85791615a, 0x0000086b9a117d86,
	0x3a581aa8769f7192, 0x00000894ac61c588,
	0xfbd120a8a7b431e5, 0x000008dcc6f2358b,
	0x6dea29c8fed22265, 0x00000959e8a2d092,
	0x5e663119a4e792ef, 0x0000099edbf4419f,
	0x88e614b88ec79690, 0x0000085a91c15d84,
	0x99a918586995e170, 0x0000088b9ff19d87,
	0xdad11da8a2a611ba, 0x000008deb4f2008b,
	0x0c7e25e904bdd22c, 0x00000964d3329793,
	0xde6830e99fdfe2d7, 0x00000a51e513389e,
	0x6e1d397abfe38365, 0x00000bb4e003cbb3,
	0xc92015f8498ea14c, 0x0000086e96217385,
	0xca031a68839ad18a, 0x000008baa651c589,
	0x7b522148dead41ea, 0x00000938be024490,
	0x0d372ba970c8127b, 0x000009f9e023019b,
	0x1e3436ca5ae4f33b, 0x00000b53e163a3ad,
	0x9dd341dbe2df53de, 0x0000084a8eb14dc7,
	0x296317585d922160, 0x0000088b9ae18e87,
	0xda631ce8a9a021ab, 0x000008f7ad01f58c,
	0x0bd9255927b4c222, 0x000009a0c7729096,
	0xedf231d9ead2a2d2, 0x00000aaae3d35ca3,
	0xfdfd3d0b2de1f393, 0x00000c5edd9412bb,
	0x98eb14cd07db2458, 0x0000085d92115f84,
	0x19a9190874961176, 0x000008b39fc1af89,
	0xaac41fb8dba591d2, 0x00000940b3d22a90,
	0x4c5f29c97ebc525f, 0x00000a15d0c2e09c,
	0x1e30373a70dce32d, 0x00000b6de103aeae,
	0x1dc6434c08ded3ee, 0x00000d66d9c480cb,
	0xc91d15e8498e814c, 0x0000087595a17585,
	0xa9f01b08949a0190, 0x000008e5a4a1d48b,
	0x1b2422d917ab01fe, 0x00000992ba626395,
	0x1cde2e59ddc392a0, 0x00000a91d96332a3,
	0x2e083bdb02e29381, 0x00000c31de33ffb9,
	0x9d91495cdfdbb447, 0x000008478e314ad9,
	0x495117385991615c, 0x0000089699418e87,
	0xba341d38be9df1ae, 0x0000091fa951fc8e,
	0xfb7c26195bb0222c, 0x000009ebc0529d99,
	0x3d4a32da42ca02e1, 0x00000b10e07381aa,
	0xcde1404b9de053c1, 0x00000ce9db944cc3,
	0x48dc147da2d8f499, 0x0000085690d15884,
	0x698218b873944170, 0x000008bf9c91ab89,
	0x3a721f88eea181cf, 0x0000095fad722692,
	0xfbc62949a3b4825a, 0x00000a46c542d59e,
	0x2d9b36eaa6cf031e, 0x00000b91e083bcb1,
	0x3dbe443c2bde53fd, 0x00000d86d9548dcd,
	0x39011558418d3144, 0x0000087093416b85,
	0xe9ae1a589496e187, 0x000008ec9f71c88b,
	0xdaa521c921a491ef, 0x000009a0b0c24d95,
	0xfbfc2c29ebb7d284, 0x00000a9cc87306a3,
	0x7dc43a4b04d1f351, 0x00000c02dee3ecb7,
	0x8da3473ca0dca42d, 0x0000083d8c8141d4,
	0xc92116684e8f2150, 0x0000089095618086,
	0x89d21bf8b999019e, 0x0000091ca1b1e48e,
	0x6ac823c955a6d20e, 0x000009ddb2d26f99,
	0x5c162e7a2db9c2a8, 0x00000ae7c9a32ca8,
	0x9dbb3c8b53d27377, 0x00000c55ddb40ebc,
	0x98bb13cceedb844d, 0x0000084a8e114b83,
	0xb93a17986790c160, 0x000008b396e19588,
	0x19e91d68e09a81b4, 0x00000948a311fd91,
	0x8ad7256985a80227, 0x00000a12b362899c,
	0xec0e2ffa64b9e2c2, 0x00000b20c85342ab,
	0x0d7c3d5b8cd0138a, 0x00000c7ddd241fc0,
	0x78b011c81c882125, 0x000007eb8de11f7f,
	0x98fa1b77bc8eb167, 0x0000075891420178,
	0xe9552a5725932250, 0x000006b197e3026e,
	0x59dd3da66f9ab368, 0x000005d3a1245862,
	0x3a89580578a4c4e4, 0x000004a3ac662c51,
	0xeb2c7be427aff6ec, 0x0000082488d12a39,
	0x98f21228138b912b, 0x000007e49291297e,
	0x39471f179a92b19c, 0x0000072b96b24676,
	0x09c73046f09962a1, 0x000006689ff3726b,
	0x0a81475619a3d3ec, 0x0000055dac950e5c,
	0x1b646794eeb165b9, 0x000003e6baf75047,
	0x989712e351b92834, 0x000008238c413482,
	0x294012d7e7905124, 0x000007cf97914a7e,
	0xc9aa22c77a9821ce, 0x000006fd9dc28d73,
	0xea5936a6b9a162f6, 0x0000061aaa43eb66,
	0x2b5051e5bcaf647c, 0x000004dbbaf5d655,
	0x6bf5780454c116a5, 0x0000032bb7186f3c,
	0xe8cf13b82e8a0132, 0x0000080d90d13782,
	0x999f13a7e0956130, 0x000007b89db16d7d,
	0x8a2926575d9f21f9, 0x000006cfa6c2d471,
	0xab113d2681ab934c, 0x000005c9b7346862,
	0xec525d255bbde511, 0x00000456c6f6a24d,
	0xfb678813c8bf677d, 0x000008328a813631,
	0x79171458378d9141, 0x000007e396913582,
	0xfa0f14a7d79b713e, 0x000007c6a731587c,
	0xaacb29274ba86215, 0x000006a8b213116f,
	0xbbf643364eb8539a, 0x0000057ac724e15e,
	0xbc8e6694f9cf95a7, 0x000003e6c1074f47,
	0x48b0138333b78863, 0x0000083e8e114683,
	0x996915583991f14e, 0x0000080b9c015782,
	0x3a9215c7cda2914e, 0x000007b8b0b16d7c,
	0x0b9b2a275ab4e1ff, 0x0000068fc053366f,
	0x6d0d485629c823d4, 0x0000053ed3453e5b,
	0x4c456ef4becc7603, 0x00000368ba581142,
	0x48e81498378b613a, 0x0000084692715684,
	0x89c117384796f163, 0x0000084aa2018584,
	0x8b041b584ea8b19b, 0x00000865b8d1d485,
	0xcc802c1878c271fa, 0x000007d6ce03a782,
	0x3d29551793d63480, 0x0000072dca863b76,
	0xdb4c8b36eac0d754, 0x000008388ba13c68,
	0x192c15c8488ee14c, 0x0000085c97416e85,
	0xea2219d86a9c5184, 0x00000896a8c1bc87,
	0x8b8c2098b4b041df, 0x00000903c262398d,
	0xfd9a2ae935cd5270, 0x000009b2e752f596,
	0xbcf05b9928d92493, 0x000008ccc3d6fe8f,
	0x98be13d885b64887, 0x0000084b8f114e83,
	0xc97617685992f160, 0x000008849c719086,
	0x6a8b1d18a2a221ae, 0x000008f0b021fb8c,
	0xbc23260921b8922a, 0x0000099ecd029f95,
	0x4e543329ead952e6, 0x00000ac0e38366a4,
	0x1cdc5dcb4de183a0, 0x00000a37bc77d4aa,
	0xc8f314f83a8c013e, 0x0000086093016384,
	0x89c419887997517c, 0x000008bea1e1ba89,
	0xdafa20e8eaa851e1, 0x00000958b7f24191,
	0xacc02bf99cc1627c, 0x00000a44d8130b9e,
	0xae1c398aade3c35e, 0x00000bd8df83dab3,
	0xccae630c89dcf424, 0x0000083a8c013ec2,
	0x592d16584d8f214f, 0x0000088397117f86,
	0x3a151c28a89be19e, 0x00000905a791eb8d,
	0x1b6c25093faeb21a, 0x000009cdbfe28e98,
	0x6d5d323a23ca32d4, 0x00000af7e2b37ca8,
	0x9de2402b8ee093bb, 0x00000cf6db6451c3,
	0x98be13ddc6d864a7, 0x0000084c8ef14e83,
	0xb969182869928166, 0x000008b39b31a288,
	0x9a671f18e2a071c7, 0x00000957ad422191,
	0xfbdb29799eb4f259, 0x00000a4cc782de9e,
	0xbdee389ab4d2832f, 0x00000bc3dfc3d1b2,
	0x8da746cc74dd441b, 0x00000e0dd764c5d3,
	0xb8ea14d8388bb13c, 0x0000086b92116684,
	0xb9a51a389095f182, 0x000008ee9f41c88b,
	0x8ab5223927a4f1f3, 0x000009b2b2925a96,
	0x5c3f2dfa05bac299, 0x00000ad0ce432fa6,
	0xcdf73dcb49d9b388, 0x00000c8ddce425bd,
	0x5d714cfd50da1476, 0x000008378b613ae2,
	0xc91616484a8e314b, 0x0000089395018186,
	0x49dc1c78c19921a2, 0x0000092fa301f18f,
	0xcafb257971a90221, 0x00000a10b742939b,
	0xbc92325a6fbfc2d8, 0x00000b53d3937aad,
	0x6dd0422bdadf03da, 0x00000d43da4471c8,
	0x48af138e12d754c7, 0x000008488da14983,
	0x493f17e86b909162, 0x000008c397c19e89,
	0x1a0d1ec8f79c01c3, 0x00000973a6421a93,
	0x0b332879bdac624d, 0x00000a6dbad2c8a1,
	0xacca362ad5c35311, 0x00000bccd6d3bbb4,
	0x6daf45ec63dd8414, 0x00000dd7d824afd1,
	0x68ce1468328a7135, 0x0000086a8fa15e84,
	0x296219989392a17a, 0x000008f59a01bc8c,
	0xea3420f92e9e51e3, 0x000009b6a8b23f96,
	0xfb582b2a06aec276, 0x00000ac2bcf2f5a5,
	0xacdf392b30c51340, 0x00000c31d743ebba,
	0xcd97489cccdc043f, 0x0000082e89d132d7,
	0x78e815a8448c1142, 0x0000089191417586,
	0xf97c1b38be945192, 0x000009279ba1d88e,
	0x7a4b22d9649ff200, 0x000009f2aa025f9a,
	0x1b642d3a45afe296, 0x00000b07bd3316aa,
	0x2cc93aeb77c4b35f, 0x00000c77d49403bf,
	0x1869113d06db2458, 0x000007f288f11480,
	0xe8971af7ba88b16b, 0x000007618a91f478,
	0xb8d72907308be23e, 0x000006bf8f32ec6f,
	0x59363c267e913351, 0x000005e395d43f63,
	0x39b55665899884cb, 0x000004b39e761352,
	0xea4b7a5437a1a6d2, 0x00000318a7688b3a,
	0x189b11580f86e11b, 0x000007c68a71587f,
	0x58c91ee7958b31a4, 0x000007318e323d76,
	0xa9242f56f9901294, 0x0000067494b3616b,
	0xd9a84616259773da, 0x0000056b9de4f95c,
	0x1a586614fca195a2, 0x000003f7a9a73548,
	0x3b0c92735dada822, 0x000008188751202b,
	0x68d21277f08a7117, 0x000007a18d31917e,
	0x890a23376e8eb1e0, 0x000006fd92e28e73,
	0x29873636bb9582f2, 0x000006209bd3e267,
	0xaa3c5125c39f9471, 0x000004e4a855c755,
	0x9b2677b45fad4694, 0x00000322b6a87c3c,
	0xe87c124279ada981, 0x000008038ad12181,
	0x78f916f7ea8e5120, 0x0000077d9111c87b,
	0x895d27d744933220, 0x000006c498e2e470,
	0x4a063db6799c6358, 0x000005c4a4e46f62,
	0xcaf65d4557a9e516, 0x00000450b566ab4d,
	0xfb5989a3b1bba7a0, 0x00000258abe9b330,
	0x88b212b823881127, 0x000007e98f212281,
	0x793c1a07e493012b, 0x0000075a9601fe79,
	0x79c82c871b99025f, 0x0000068ba0833d6d,
	0x3aa5458635a523c2, 0x00000564b035025d,
	0x7bdc6a14e6b6b5c4, 0x000003bcbec79045,
	0x5abc9b7317b6088e, 0x0000082688712a25,
	0x18fa1278248b7132, 0x000007e293e12c7f,
	0xd9951c97db989137, 0x000007399c623077,
	0x7a4c3126f4a0329c, 0x00000651a9f3956a,
	0x2b6a4d45f1aff42a, 0x00000504be059658,
	0x1c0c756473c63675, 0x00000339b7d8593e,
	0x888c12c272ad498c, 0x0000082c8bc13782,
	0x694013d81f8f813c, 0x000007da99713980,
	0xea091e07d29f0146, 0x00000721a4625676,
	0xfaef3506d3a942cd, 0x0000061fb593e267,
	0x8c4e5545b4bd1488, 0x0000055ec9166555,
	0xbb5789e53ac0176a, 0x000004d8a89a1550,
	0x38c013a82989012d, 0x000008358fa14683,
	0x498916783a93d154, 0x000008549df17e84,
	0x7a7a21d86aa4019a, 0x00000816ab02c884,
	0xcb624137f7b01367, 0x000007c1bd14d57d,
	0xac6d6a57a3c4e5b4, 0x0000077cbde7aa78,
	0x2a60a5f766b328e1, 0x0000082a89212e74,
	0x48fb14d8378c213d, 0x0000085693c16184,
	0xd9d919786f98517a, 0x000008b2a391ba88,
	0x3b232118dfaa71e2, 0x000008ffb6d2c291,
	0x3c204728ddbb53a0, 0x000008cfc9b5588d,
	0xcc0d7548d9c9f64b, 0x000008f8b5e8928e,
	0xb89412f8f2a84a1c, 0x0000083b8c313f82,
	0xb93916a8508fb152, 0x0000088c98118786,
	0x3a311ce8b49d31a8, 0x0000091aa9c1fb8e,
	0x3ba126a959b1622f, 0x000009f7c3f2ad9a,
	0x8d2945ca57cf22fa, 0x00000a1ed1257ba1,
	0xebbd7e5a3fc75698, 0x00000a6badc97ea5,
	0xe8c314082b89512f, 0x000008598f815683,
	0x297b18f87a935170, 0x000008d19ca1b38a,
	0x6a8c20c907a241dd, 0x0000098db0224394,
	0xec222c89e0b89281, 0x00000aa9ccf319a3,
	0xbdfe3ceb24d91375, 0x00000c74dd441bbb,
	0xcb68880bd5c6c6a8, 0x0000082b89412fbc,
	0x08f41588408c1140, 0x0000088692e17586,
	0x79bd1bb8b3971195, 0x00000922a141e68e,
	0x3ae824f966a77217, 0x00000a0cb6828f9b,
	0x3c9f32ba70bfa2d8, 0x00000b66d57388ae,
	0xfdc343abffdef3ea, 0x00000d98d91494cb,
	0xa89212ed3ac6f6a2, 0x000008418bd14082,
	0xf9251778658ed15a, 0x000008c096419988,
	0x59ff1eb8f79ad1bf, 0x0000097ca5c21c93,
	0x9b3f2959cdac7255, 0x00000a91bc82dea2,
	0xdd0d38db07c62330, 0x00000c25dcd3f5b8,
	0xed884a4ce5dba44a, 0x00000e5bd644e5db,
	0x28b814082988f12d, 0x0000086a8e515a84,
	0x895519a896919178, 0x000009019981c08c,
	0xaa3d21d9419e51ec, 0x000009dca9f25498,
	0x1b8d2daa38b0f293, 0x00000b17c1a32aaa,
	0x3d653e9b9dcb7384, 0x00000ce5dba44ac3,
	0xbd644e5db9d8a4a2, 0x0000082788b12be5,
	0xc8db1598428b013f, 0x0000089a90b17786,
	0x69811be8cd942199, 0x000009479c81e990,
	0x0a7224e98fa17218, 0x00000a3bad728a9e,
	0x6bc831baa2b492ce, 0x00000b98c55370b1,
	0xbd98438c29cf03cf, 0x00000d8fd93490cc,
	0x6884129e5bd644e5, 0x000008438a813d82,
	0xb8fb17586d8cf158, 0x000008cf92d19589,
	0x69a61e29079661ba, 0x0000098d9ee21094,
	0x3a9a27c9dba3f243, 0x00000a96aff2bba3,
	0xfbeb351b03b70302, 0x00000c07c733a8b7,
	0x5d9e470c9ed05408, 0x00000e0ad774c3d4,
	0x389d13b82487e127, 0x0000086d8c115584,
	0xc91619089b8e8171, 0x000009039481b38c,
	0x39c22039409821d9, 0x000009cea0923398,
	0xcab12a1a20a59267, 0x00000ae3b122e2a7,
	0x2bf1378b54b7d329, 0x00000c5bc6c3cdbd,
	0x2d6c48acf1ced42a, 0x000007f3845113d9,
	0x084b1927c8845154, 0x000007768551d37a,
	0x586f26974886121a, 0x000006dc87f2c171,
	0x58a739069c892322, 0x000006068bf40965,
	0xb8f75295ae8da491, 0x000004de9175d254,
	0xc95e75e46393a68e, 0x000003479848433d,
	0x885110b2a69a793c, 0x000007cf85514a7f,
	0x88681d17a485c18c, 0x0000074787821b77,
	0x589f2ca71288a26e, 0x000006928b73326d,
	0x18f042a6468d23a7, 0x000005929134bd5f,
	0xe96361a526939562, 0x000004289906ea4a,
	0xa9f28d33919c17d2, 0x00000232a229ed2e,
	0xa86813a7f785810c, 0x000007aa86f1837d,
	0xb89121677c87d1c9, 0x000007138a726b74,
	0xe8de3386d58c12cb, 0x0000063f9003b268,
	0x19524d75e592643c, 0x0000050e98158758,
	0xc9f072e48d9b664e, 0x00000358a2e8293f,
	0xda56a712a1a6e943, 0x0000080785a1131d,
	0x48831747f0883117, 0x000007858941bd7b,
	0x98c52627528aa20b, 0x000006da8e52c471,
	0x09313b0691908333, 0x000005e396043f64,
	0x39cf59857b9944e0, 0x0000047ca1066850,
	0x5aa585f3e3a58754, 0x00000273ad598a33,
	0x085d1181ada2dabb, 0x000007f288d11481,
	0x18ac1aa7c389a15d, 0x0000075d8c61f979,
	0x490a2b37248e6251, 0x0000069b9343236e,
	0xb99b4336499643a2, 0x000005819da4d75e,
	0xca6e668506a20593, 0x000003ddac575c47,
	0xbacf996329b21872, 0x0000019aa1dad826,
	0x289411481686111c, 0x000007df8bb1317f,
	0xe8e31e07a28c718f, 0x0000073590723776,
	0xe9613086f5931299, 0x0000065b9993866a,
	0x3a214ba5fe9d9416, 0x00000519a7357659,
	0xeb3474348eace64c, 0x00000339b7d8593e,
	0x7a28ac327fadf977, 0x0000081986411e1a,
	0xd8ce11d80689511e, 0x000007bf8ea1627e,
	0xc92a2137859041bb, 0x0000070e95927374,
	0x79cf35b6c79902e0, 0x0000061ca163e867,
	0xcac65415b4a69488, 0x000004b3b2e61453,
	0x9ba680f414ba2708, 0x000002b0b0992d36,
	0xa86711f35b9f6b1e, 0x0000081989512681,
	0x990a13580f8cc12c, 0x000007dd92319480,
	0xf96b26e7ba9421ff, 0x0000078699c2e979,
	0x0a1841576d9d6375, 0x00000733a634cc75,
	0xab16693710ab85a0, 0x000006c0b7e7ac6e,
	0x7a79a316a0b368db, 0x0000066e997bcc68,
	0x489512c81b869120, 0x000008318c813b82,
	0x094316784590114e, 0x000008589681c686,
	0x49b52d083e98524e, 0x0000082e9ef35f83,
	0x6a814b782aa33400, 0x00000822ad958b82,
	0xaba879781eb3b67e, 0x00000825b438c281,
	0xd9a2bb6834a86a19, 0x0000081c86b12083,
	0x58c614382c895130, 0x000008648fd15c84,
	0x598419a88993c178, 0x000008e99d71c18b,
	0x1a2130c8d49f126a, 0x000008dca633b08d,
	0x2b0c5368edab1467, 0x0000091ab7162490,
	0x2b7c85a934be0737, 0x00000994ac29ac96,
	0xd86c1219be9e0b46, 0x0000083389413281,
	0xa8f81658548c314a, 0x000008a693518487,
	0x59ca1d38d997a1a9, 0x00000959a2620291,
	0x3ad32d79a7a8f239, 0x000009c1b033c09c,
	0xbbb85829e1b56497, 0x00000a3dc2668fa0,
	0xdb268f7a81bdc7ad, 0x00000ac3a58a6cac,
	0x989313581d86c121, 0x0000085f8be14e83,
	0xc92c18d88a8f116b, 0x000008f496f1b38b,
	0xfa132109359bc1df, 0x000009d3a7724997,
	0x2b6d2d5a34aea28a, 0x00000b1fc00329aa,
	0x8c9d55fb79c7a3db, 0x00000ba4c7369cb6,
	0x9b0093cbf5bb97ed, 0x0000081c86b120b8,
	0x88b915183d88f136, 0x000008978e917086,
	0x89611b98cc921192, 0x0000094c9a91e690,
	0x1a5b2529999fd218, 0x00000a56ac72949f,
	0x9bcb333ac7b412de, 0x00000bddc67393b4,
	0x5da4472c84d163ff, 0x00000d70cc95ffd4,
	0xc869120c81bd37be, 0x0000084188b13781,
	0x08e017286f8b2153, 0x000008d79141958a,
	0xb9941e99159501bc, 0x000009aa9e221b95,
	0x8a9d295a04a39254, 0x00000adcb0f2dea6,
	0x0c1d38eb5db8e331, 0x00000c96cbd3f7bf,
	0x2d6e4d4d50d6c46d, 0x00000e5bd644e5e3,
	0x488613781e866120, 0x000008738aa15484,
	0xe9051958a68d5173, 0x0000091c93d1bc8d,
	0x19c321996297c1e8, 0x00000a0aa142509b,
	0xead62d5a6ea6f28e, 0x00000b5db49324ad,
	0xcc593e0bebbca37d, 0x00000d3fcf644fc8,
	0xbd644e5e09d774c3, 0x0000081f862120e5,
	0x68a115384687f137, 0x000008a98c817287,
	0xe9271b98e18f5193, 0x000009639601e391,
	0x69ea2479af9a1212, 0x00000a66a3c282a0,
	0xaafe30ead1a982c4, 0x00000bd1b70360b4,
	0xfc77422c68bee3bc, 0x00000dc7d0a491d0,
	0x085d120e5bd644e5, 0x0000084887813782,
	0xa8ba16f877896152, 0x000008df8e218f8a,
	0xd9431db91b9101b3, 0x000009a697d20795,
	0x2a0626f9f79be238, 0x00000ab7a562aba5,
	0x4b1233ab27ab02ef, 0x00000c2eb7f38eba,
	0xcc7044bcc6bf43e9, 0x00000e1dcf04b4d6,
	0x981216c7de812133, 0x000007918141a97b,
	0x681d2367668181ec, 0x0000070082428873,
	0x183434c6c482b2e5, 0x0000063683d3c068,
	0x48564d25e2849441, 0x0000051b86557458,
	0x38876f14a6876628, 0x0000039389c7cf42,
	0x98cc9cb2f58b38c3, 0x000007e381b12c24,
	0x38201a67bd81d167, 0x000007668261ec79,
	0xb83729073482e239, 0x000006bc8422f16f,
	0x485d3db67584e35f, 0x000005ca86d46662,
	0x18945b3564880503, 0x000004718ac6794f,
	0x08e384f3e18c5757, 0x0000028e90296134,
	0xa8261211cb923a8d, 0x000007c18281607e,
	0x88361e879782d1a1, 0x0000073484023876,
	0x985c2f66fa84d291, 0x0000066e86d36a6b,
	0xa89547e61a8803eb, 0x0000054e8af5255b,
	0xa8eb6b54d48cb5e1, 0x000003ae90e7a544,
	0xc95e9df2ff9358b2, 0x0000016398bb2c23,
	0x78341567f4833111, 0x0000079c83c1987c,
	0x985423176e8471e0, 0x000006fe86528c73,
	0xe88d36a6bb8772f4, 0x000006178a73ef66,
	0x48e35345b48c2487, 0x000004c49075f954,
	0xf95e7d64339306d8, 0x000002d49918f438,
	0xe9b4b962029c7a38, 0x000007fa83d10811,
	0x484b18c7d0841149, 0x000007758591d47a,
	0x687e28074186a224, 0x000006c28962e870,
	0xd8cf3e66758b035f, 0x000005b88f348161,
	0x39495fb54691b531, 0x0000042d97c6e14c,
	0x19f59123839b67e7, 0x000001e5a39a652c,
	0xa84010910199bbc3, 0x000007db8501387f,
	0x086b1c37ae85b17c, 0x0000074d88121278,
	0xf8b52d471289926d, 0x000006838d534a6c,
	0x792546b62b8fb3d1, 0x0000055495651b5c,
	0xa9cc6ce4d198e5e6, 0x0000038da127d843,
	0xda63a592c7a5f908, 0x0000010499ebbe1e,
	0xd86411c80784010f, 0x000007bc86d1677e,
	0xc8981fa78f8801ad, 0x000007238b525275,
	0x88fc3296e28d62b7, 0x000006429283ad69,
	0xf9934f35df95a445, 0x000004ed9d55ba56,
	0xda707a7458a1e69f, 0x000002e8acc8d63a,
	0xf969c1e214a85a1c, 0x0000080c84211129,
	0x08831487f586f10f, 0x000007a28971907d,
	0x38d02327718b21db, 0x0000074c8e62b174,
	0x59293ce73c905336, 0x0000070895347a72,
	0x29bb6266e7984541, 0x000006979f872e6c,
	0x4a839c3668a3b860, 0x000006149e4b3f63,
	0xe8441135ff8f0cf9, 0x0000081386b11c80,
	0x28a118682589612c, 0x000008078b31e381,
	0xc8e92ba8018cc248, 0x000007f790b33c7f,
	0xd96047e7f29333d2, 0x000007e89935447e,
	0xba0e7317e29cd629, 0x000007d2a558637d,
	0x79e4b3e7caaa29c6, 0x000007e68f1cf87d,
	0x586912480f845113, 0x0000084489113a82,
	0x98d31af8688c0153, 0x000008678e521f86,
	0x992531586e902294, 0x0000088694e3a887,
	0x99b451289797e451, 0x000008bf9f15f18a,
	0x1a8281d8d7a366f3, 0x0000090dad39778f,
	0xf92ac91941a15ae6, 0x0000080f84611495,
	0x888e142832867129, 0x000008838bb15e85,
	0xd92a1a38b48ef17e, 0x000008e693223a8e,
	0x09783588f69502c5, 0x000009309a83fb91,
	0x3a2158a9579e14b5, 0x000009b6a6968098,
	0x9b0d8e89edab879c, 0x00000a7aa6aa4ba2,
	0x18461149ff9b1b9b, 0x0000083b86512d81,
	0x78b516686788a148, 0x000008cd8e718889,
	0x19611db90b9201af, 0x000009a19ad20e95,
	0xc9f035e9ca9d8295, 0x00000a0ba2441a9d,
	0xaaaa5d2a46a624e9, 0x00000ad8afa6dfa8,
	0xfaea963b2ab4e818, 0x00000ab6a4da80b6,
	0x3862130815845116, 0x0000087288614d84,
	0xf8de18f8a68af16c, 0x000009209141b68d,
	0xe99a21796a9531e4, 0x00000a1e9eb2529b,
	0x7ab22e1a8ba48295, 0x00000b94b2b338b0,
	0x2b5f5bbb8cb2549e, 0x00000c29ba86f2bd,
	0x7aff93fc3bb97829, 0x00000818844117b8,
	0xb88015084985f132, 0x000008b18a716f87,
	0xf9071bb8ec8d4193, 0x0000097b9411e892,
	0x19d02569d198421b, 0x00000a9fa26298a3,
	0x9af8338b1ca892e3, 0x00000c4ab76398ba,
	0xaca247fcfec03405, 0x00000d6bc98656dc,
	0xd842119c83bd47bb, 0x0000084d85c13481,
	0x889e17188187b151, 0x000008f48c81948b,
	0x392e1e89378f81bc, 0x000009d896c21b98,
	0x5a01293a389b2253, 0x00000b1fa5a2dcaa,
	0x4b31389ba9abf32d, 0x00000cf2bb13f0c4,
	0x6ce34d3db5c3e464, 0x00000e5bd644e5e6,
	0x185713582183e11a, 0x0000088587415285,
	0x78bc1938bc895171, 0x0000093a8e71ba8f,
	0x69512159839191e5, 0x00000a3399124b9d,
	0x1a282cca9c9d8288, 0x00000b94a83319b1,
	0xab593cec27ae836e, 0x00000d80bd5438cc,
	0x1d1a4dbe48c5d4ac, 0x0000082583b11ce6,
	0x786c151854852135, 0x000008bc88b16f88,
	0x68d51b58f68ae190, 0x0000097d9021dd93,
	0x496d23d9cc93520b, 0x00000a869ad276a2,
	0xda432fbaf39f42b5, 0x00000bf4a9b349b6,
	0x0b683ffc8aafd3a0, 0x00000de4bdd467d3,
	0x67e6140e72c644c1, 0x000007b17e41797d,
	0xb7de1fd7877e01b8, 0x000007297dd24a75,
	0x37da2ff6f27db29f, 0x0000066d7d936b6b,
	0x77d746b61f7d83e4, 0x000005657d65025c,
	0xd7d76674f77d65aa, 0x000003f47d873947,
	0x87e392035d7db821, 0x000002057efa332b,
	0x37eb1767d97ee13a, 0x000007887ea1b77b,
	0x67e924f75a7e91ff, 0x000006eb7e92a972,
	0xf7eb3816a97ea30f, 0x0000060b7ed40265,
	0x27f15385ac7ef494, 0x000004ca7f55ef54,
	0xd7ff7a74447f96bf, 0x000003048078aa3a,
	0xf825b0224a8139c9, 0x000007de7f613417,
	0xb7f41b37b67f5171, 0x0000075b7f61fc78,
	0x97fa2ac7267f724f, 0x000006a57fe3156e,
	0xf80641565780138d, 0x0000059b80d4ae5f,
	0xa81c62252a81355d, 0x000004198267014a,
	0xe8409163768317fc, 0x000001f2853a512b,
	0x37ff12c11b875b9c, 0x000007bb7fe1697e,
	0xf8031f67908001ac, 0x0000072980724975,
	0x68143146ec80d2a8, 0x0000065581b3906a,
	0x382f4ba5fa82541b, 0x0000051d83b57059,
	0xf85a72949784a63f, 0x0000035286e8333f,
	0x38a1ab228e885960, 0x0000010d906bb11b,
	0x18081607e9807122, 0x0000079680c1a27c,
	0x081a23e7668121ec, 0x000006f282229e73,
	0xb8393876ac82c30b, 0x000005ff84741565,
	0xc8695715958574b7, 0x0000049387f64551,
	0x38b484b3f6898736, 0x000002778d598434,
	0x198abc31908fbae7, 0x000007f081111710,
	0xe81a1957c8813155, 0x000007708221dd79,
	0xd83928d73a82d22f, 0x000006b88482f86f,
	0xb86c402667859374, 0x000005a28824a460,
	0xe8ba63352989c55e, 0x000003ff8db72849,
	0xa92d98034a902840, 0x0000018c960aed27,
	0xa81b10910099abc6, 0x000007d181e1487f,
	0xc8331ca7a9828185, 0x0000074884121a77,
	0x98652de70d852275, 0x0000067a87a3566c,
	0x98b14816208933e2, 0x000005418d25385b,
	0xb9236fd4b98f860b, 0x0000036795581341,
	0x299eb1529798d953, 0x0000032b91bcac27,
	0xc82c1367fc821104, 0x000007b68371717d,
	0xa8581ff78b8461b3, 0x0000074986326675,
	0x787a37274786c2e6, 0x0000072088a41273,
	0x38b359c70489d4ca, 0x000006bd8cc6906e,
	0x49088ed6938e97a8, 0x00000632929a6566,
	0xa876dd75ff94cc19, 0x000007ff8221065f,
	0xb83e16b802839123, 0x000007f88471ba7f,
	0x385e27b7f5852214, 0x000007f086d2f27f,
	0xa89141b7ed87e37c, 0x000007e68a64d27e,
	0xd8db6987e28bf5a5, 0x000007d88fa7b17d,
	0xb943a6e7d291e8f6, 0x000007c3966c237c,
	0x78221097d8864df8, 0x0000082a83e11e80,
	0x18571a383f85314c, 0x000008498621fd84,
	0xe8802d9852870264, 0x0000086c89235f85,
	0xe8c04ad87c8a83fa, 0x000008a48dc57b88,
	0x692177a8bb8fd668, 0x000008f39488b78d,
	0x5993bd2913972a27, 0x000008d98b8d5f93,
	0xb83c12680e82210c, 0x0000086685b13f83,
	0xb8821c589588015b, 0x000008ab88d23089,
	0xd8b23278c289e2a4, 0x000008fd8ca3bc8d,
	0xd90652b9238e6467, 0x0000097d92960e94,
	0xf97f8419b4952713, 0x00000a309ae9a09e,
	0x893fc6aa499e8b29, 0x0000081482110f97,
	0x785814684783a12b, 0x000008ab87b16487,
	0x88d31ae8e58a4187, 0x000009388d723692,
	0xf8fb3569558e42c4, 0x000009b29183fa97,
	0x09625889ed93b4b3, 0x00000a7d98f67ca3,
	0xc9f48e0ad19c0796, 0x00000af3a49a24b2,
	0xb820112a2f9dab50, 0x0000085083812f81,
	0xa87616b88385414b, 0x000008f889d18e8b,
	0xe8fd1e493e8ca1b7, 0x000009e993921998,
	0x697a32aa5097d254, 0x00000a9e98f3f1a6,
	0x39e05a7aea9b44c0, 0x00000ba7a116aeb4,
	0x6aa78f6c12a457e1, 0x00000afaa87a18bb,
	0x783513282281f115, 0x0000088d85114f85,
	0x68951938c687016f, 0x0000094e8bf1bc90,
	0xb92821e99f8f01ea, 0x00000a639672599f,
	0x0a022e9ada9af29d, 0x00000bf8a60340b6,
	0xeaa6530ca4aca3a3, 0x00000d04abf679ca,
	0xeb488baca1b0f78c, 0x0000082881e118bd,
	0x484d15285d833134, 0x000008ce86b17289,
	0x68b51bd90e88d195, 0x000009a78e21eb95,
	0x994f258a0291521e, 0x00000add992299a6,
	0x5a33336b609de2e3, 0x00000c9ca94395bf,
	0x8b7d475d57b023fe, 0x00000e77c2b4b9e2,
	0xd81c11ad06bda6f2, 0x0000086183013582,
	0x2864172898848152, 0x000009118841958d,
	0x78d21e89578a91bc, 0x000009ff9012199a,
	0x297328ea62936250, 0x00000b4f9b72d4ad,
	0xaa5a37abdca04323, 0x00000d29abc3ddc7,
	0x3bb24a8decb2944a, 0x00000e71c714c3e8,
	0x482d13683281a11c, 0x0000089984415286,
	0xf87a1928d285d171, 0x0000095389c1b790,
	0x38ec20f99e8c21e1, 0x00000a5091c2439f,
	0xe98e2bdab995227d, 0x00000bb09d3305b2,
	0x2a743afc41a1e356, 0x00000d92ad3411ce,
	0xfbd44ade52b3c47c, 0x000007cf7d614be7,
	0x27a31c17aa7b7182, 0x000007547a220878,
	0x77932af72079c257, 0x000006a878b3116e,
	0xf7753fb660780380, 0x000005b576848660,
	0x074c5ce55175b521, 0x0000046273e6904e,
	0xc7228553d672f767, 0x0000029271995a33,
	0x27d91451dc71aa72, 0x000007ad7b917f7d,
	0x27ab20a7827af1c0, 0x0000071d7a525d75,
	0xd7953226e079e2b9, 0x0000065078c39769,
	0x97764b15fa78141c, 0x0000052c76a55959,
	0x774f6ec4b175d617, 0x0000038b7427da42,
	0x072ea092df7368e4, 0x00000151730b4922,
	0x07bc17a7d67dc13f, 0x000007847ba1bd7b,
	0xc7b125e7537b6209, 0x000006de7ab2bd71,
	0x879c3a56987a432a, 0x000005ed79443064,
	0x278158158678a4cf, 0x0000048e77664c51,
	0x27618333fa76c731, 0x0000029675995535,
	0xe79eb7e1c4754a98, 0x000007db7e113712,
	0x87c61b77b47c4173, 0x000007577c320378,
	0x07bb2bb71f7bf259, 0x000006987b632a6e,
	0x87ab4386457b13a8, 0x0000057d7a54dc5e,
	0xa79966c5047a0597, 0x000003dd79275d47,
	0x37899a332c78d86e, 0x0000018178aafe26,
	0x17e612f123819b8f, 0x000007ba7d216b7e,
	0xd7d11f978e7d21ae, 0x000007267cf24f75,
	0xd7cc3216e67ce2b1, 0x0000064a7ca3a169,
	0xe7c64da5eb7c8433, 0x000005027c559957,
	0x37c376d4757c4674, 0x0000031a7c48893d,
	0xc7d0b382497c89cb, 0x0000011988ab9f15,
	0x07de1617e77eb125, 0x000007967e01a27c,
	0xf7e22407667e01ec, 0x000006f07e32a172,
	0x67e738f6a97e530f, 0x000005f77ea42165,
	0xe7f158858a7ed4c8, 0x0000047e7f666450,
	0x08068803db7fd760, 0x000002498129ca32,
	0x08eebad156824b41, 0x000007ed7f011c11,
	0x07ee1937c87eb154, 0x000007727f11da7a,
	0x07fb28973d7f522b, 0x000006b98002f570,
	0xb80f401668807372, 0x000005a08184a660,
	0x882f63d525822563, 0x000003f683f73649,
	0x1818a2e3b9834889, 0x00000392808bfa3c,
	0x57f611041a88acff, 0x000007d27f81457f,
	0x08041c37ac7fd180, 0x0000077b80122478,
	0xa7fb3137777fd294, 0x000007587f93a376,
	0x97f45097427f744a, 0x0000070c7f25e572,
	0x57ec8076eb7ef6e2, 0x0000069d7e99596c,
	0x47e1ca46717e5adf, 0x00000686837e0c64,
	0xf7ff13e800800100, 0x000007ff7ff1847f,
	0xf7ff2327ff7ff1d4, 0x000007ff7ff29d7f,
	0xf7ff3a97ff7ff31a, 0x000007ff7ff44e7f,
	0xf7ff5e97ff7ff50d, 0x000007ff7ff6e77f,
	0xf7ff95e7ff80080c, 0x000007ff800ae57f,
	0xf7ffeae7ff7ffcab, 0x000008137fe1087f,
	0xa80d171834810126, 0x0000084480d1c283,
	0xc81028784f80f21e, 0x0000086b81330185,
	0x081842f87c81538d, 0x000008a681b4e989,
	0xc8236b88bf81f5bf, 0x000008fd8287d58d,
	0x7833a9e92082d91f, 0x00000970838c5994,
	0xd7fd10d88186dde7, 0x0000084b81312681,
	0x782919587982c13f, 0x000008998291f488,
	0xb82f2d28b082b25c, 0x000008eb8343598c,
	0x98424a690f83a3f4, 0x0000096884a57393,
	0x985d76f99d85365f, 0x00000a1b8698aa9d,
	0x18a6b7ea64876a17, 0x000009228f6cefa1,
	0x881112b8257fd110, 0x0000088b82914785,
	0x18651888c2845165, 0x0000091085b20690,
	0x886030692f85b282, 0x0000098986939a95,
	0x38805029c2874442, 0x00000a4d88f5dfa0,
	0xb8b2803aa08a06dd, 0x00000b5d8c4959af,
	0xf96abccabb913a7a, 0x0000082d7fd1149d,
	0x882714d863810130, 0x000008d384116c89,
	0xf8841b9915860190, 0x000009b48ad1e795,
	0xa8b92fba158dc21c, 0x00000a558bf3a6a1,
	0x78de52caa08cc45d, 0x00000b598f2617af,
	0xf927856bc7909726, 0x00000b7b97f952c1,
	0x47fd117ab19d6a89, 0x0000086a80f13383,
	0xe83e1718a2825151, 0x0000092285b1958d,
	0x58a31ed96e87c1be, 0x00000a268d02229c,
	0x393c2a2a9590225e, 0x00000ba297f2f0b1,
	0x19764e4bec9813cf, 0x00000c9a9845f1c3,
	0x89ee7fdcf299e711, 0x00000b98a3b925c5,
	0x080e13683a7fc119, 0x000008a882215387,
	0x98561988e683a174, 0x000009768751c092,
	0xc8c22229cb8991ee, 0x00000a998f125ca2,
	0x09642eab1492729f, 0x00000c3d9aa33eba,
	0x4a54409cee9f939e, 0x00000e91abc482db,
	0x4aab789d8ba96624, 0x0000083f7fc11cca,
	0xd82015587480d137, 0x000008e98361748a,
	0x686d1bf92c850198, 0x000009c988f1eb97,
	0x08e0255a278b521d, 0x00000b06911294a9,
	0xf98732ab8a9482db, 0x00000cc69ce384c1,
	0xca7d456d7fa203e7, 0x00000e92b0a490e4,
	0x37fb11de81bbe4a9, 0x0000087680c13884,
	0x88321738ad81e155, 0x0000092884a1958e,
	0xf8841e59708651bb, 0x00000a188a62149b,
	0x98f9283a7b8cd248, 0x00000b6592b2c4ae,
	0x79a335fbee96330d, 0x00000d309ea3b9c8,
	0xca9d481de9a3b41d, 0x00000e8eb3a497e9,
	0x37b118e7c87d0155, 0x0000077b78f1cb7a,
	0xe74025a74f76a20f, 0x000006e473a2b471,
	0x972338a6a273231a, 0x0000060871040765,
	0x76e25305ad6fa493, 0x000004d56c75df54,
	0xa68e77a4576ab6a1, 0x0000032f6728693c,
	0xf652a8628565c96e, 0x000007cb7d31501c,
	0xc7901cb7a57b318b, 0x0000074e76921177,
	0xc7462c171b73d260, 0x0000069673b32b6d,
	0x271742964972a3a2, 0x000005907004c05f,
	0x56cb6295216e756a, 0x0000041a6ae7004a,
	0x06728fb37e68f7f0, 0x00000211658a212d,
	0xf7d614a14c656b50, 0x000007a87b51867c,
	0xf76a20f77e7921c6, 0x0000071875726474,
	0x17483346d87552c6, 0x0000063f7373b169,
	0xc70e4de5e472443e, 0x000005066f559357,
	0xb6bc7474816d9660, 0x0000034269e84b3e,
	0x2667ab328568096f, 0x000001436c1b5f1b,
	0xd7ba17e7d47da143, 0x000007837961c07a,
	0xa76c26175476e208, 0x000006da7662c471,
	0xd74a3b5690759335, 0x000005de73844763,
	0x770c5aa5727234ee, 0x0000046c6f36814f,
	0xa6bb8893ce6d8774, 0x000002506a09c031,
	0x9727b6d16d689b1d, 0x000007d97df13b13,
	0x979b1b57b37bf175, 0x0000075a77c1fe78,
	0x07792bb720781257, 0x0000069676f32d6e,
	0x17524436417613af, 0x000005737404ec5e,
	0x671668b4f572c5ad, 0x000003c26ff78646,
	0x56d09ea3086e78a5, 0x000001476bdb5823,
	0xf7e4131130786b7b, 0x000007bb7c51697d,
	0x27981f27927a31a8, 0x0000072a79724876,
	0x178731c6ea7902ab, 0x0000064d77d39d6a,
	0xe7644da5ed771431, 0x0000050075459d57,
	0xa73277a47074467b, 0x0000030d72089d3c,
	0x0704b632357109ea, 0x000001297dbb8714,
	0x37cd15c7e67ea126, 0x0000079d7ac1977c,
	0x77ad23476d7af1e1, 0x000006f97a929473,
	0xf79c3806b17a3302, 0x0000060179641165,
	0xa7675ad59578d4b8, 0x000005787436b759,
	0xd6fb9345417217e0, 0x000004b46d3abc4f,
	0x173cd3346c6a8c80, 0x000007ef7f211951,
	0xe7c01867cf7d614b, 0x000007b47b81e27a,
	0x37a22b37ad7ae243, 0x000007977933357a,
	0x77704737887833c9, 0x0000076375a53877,
	0x372571e74d741619, 0x0000071770684973,
	0x66bdb316f76e39a3, 0x000006b3696cfd6d,
	0xe7f910c7306f5e1f, 0x000008107d614b80,
	0x47be1e58127c4192, 0x000008177b524581,
	0xd79d3348197aa2b4, 0x0000082178e3c781,
	0xc76a53582677d471, 0x0000083375361782,
	0x171e84783973a71b, 0x0000084b6fe9a084,
	0xe6b9cfb8546dcb2f, 0x000008196e3e8685,
	0x17e41328237f710e, 0x000008497d717f84,
	0x37c72308557d01d2, 0x000008747be29d86,
	0xb7a73aa8867b331a, 0x000008b379945089,
	0xf7775ea8cf78950f, 0x000009127626e88e,
	0x673295d93a74b80c, 0x00000996716ae396,
	0xa738e0a96b70ec7e, 0x0000082f7f511386,
	0xf7ff14185f7eb127, 0x000008977ec1a788,
	0x67db2728ab7e3208, 0x000008e57d22eb8c,
	0x27bf4149097c9375, 0x000009607b34cb93,
	0x279868e9957a759c, 0x00000a157887a49d,
	0xf762a59a5f7768e6, 0x000009fe789b9caa,
	0x87f311790f7bcd0d, 0x0000086c7ee12e83,
	0xc8131688a17fe149, 0x0000091e82b18c8d,
	0x67ff29e922809223, 0x000009747f832494,
	0x87ea4669a97f13bb, 0x00000a2e7e352b9e,
	0x87d2710a7f7dc60c, 0x00000b3c7c983cad,
	0x9804a95b607ce97b, 0x000009cb83fbeaaa,
	0x67ef13383f7f111a, 0x000008ae7fe14f87,
	0x28281938ec81216f, 0x000009808431bc93,
	0x18502819da8621eb, 0x00000a3083e329a0,
	0x683348aa748373d1, 0x00000b2483155cac,
	0x282c760b8e82f64b, 0x00000c0d842871c0,
	0xd8bcaa7b6987e96e, 0x000008467ef11da9,
	0x87fe15487e7f0137, 0x000008f78101748b,
	0xe83f1c293e826198, 0x000009e985c1f198,
	0x58a5262a5087e226, 0x00000b498d12a7ac,
	0x18be437bdf9042f5, 0x00000c528b252ebf,
	0xa8c4735cc98ae635, 0x00000c428fe820cd,
	0xb7ee11fb83936945, 0x000008847f013984,
	0xe80f1778be7fe156, 0x0000094582319b8f,
	0xe8571f299483b1c4, 0x00000a538762269e,
	0x78c32a3ac689a261, 0x00000bd98f22efb4,
	0x59673a4c7d929344, 0x00000e049b0410d3,
	0xa9a3671e4a9c24ff, 0x00000c869b67b7d5,
	0x77f113b8507ed121, 0x000008c17fd15888,
	0x682119b90080e178, 0x000009938371c294,
	0xc86e2219ea8511ef, 0x00000aba88f259a4,
	0xf8e02e0b358b5299, 0x00000c5a912330bb,
	0x798b3edd0794a389, 0x00000e9b9d745cdc,
	0xfade4dfea1a63479, 0x000008537ed122e5,
	0x17fd1588897f113c, 0x000008fe80d1768c,
	0xb8331be94181e198, 0x000009de84a1e898,
	0x188424ca3a865217, 0x00000b148a6287aa,
	0x28fa312b948cd2c9, 0x00000cc092c364c2,
	0xd9aa422d6f9673be, 0x00000ea9a0b46ce2,
	0x17cb160e9ca9c481, 0x0000079d7ac1977c,
	0x876421a77578a1d5, 0x0000071773a26574,
	0x26d73196e070c2ba, 0x0000065d69c3846a,
	0x067348e60f6653fc, 0x0000054a65b52a5b,
	0xc6176994da63c5d8, 0x000003d15f076f45,
	0x15ab95c3385cb85b, 0x000001df59ba6e29,
	0xf7ad1957c47cd15b, 0x0000077578a1d579,
	0x373726c74776321c, 0x000006d97062c571,
	0xe69039b6976ce32a, 0x000005f568742464,
	0x465a5665926784bc, 0x000004a863562452,
	0x45e27e741e60d6f9, 0x000002d85b78ee38,
	0xf589b3321d592a0f, 0x000007c87d015515,
	0x878c1d17a27b0190, 0x0000074876421a77,
	0x77042c971373726c, 0x000006926ca3326d,
	0x569d43c6456893a9, 0x0000057d6854dd5e,
	0x463d65d508664591, 0x000003ef61274248,
	0xc5b69643475e3843, 0x000001bd58daa328,
	0xd7d414e1575deb3f, 0x000007a67b41897c,
	0xc76721377c7901ca, 0x0000071673a26774,
	0x36ca3316d97062c5, 0x0000063e6c53b469,
	0x269a4ed5de6b6447, 0x000004f86785a957,
	0x062477146d65067f, 0x0000031f5f68823d,
	0x859db0d2575c79b5, 0x00000150632b4b17,
	0xd7b917f7d27d9145, 0x000007837961bf7a,
	0xd74025c75376d208, 0x000006e070c2ba71,
	0x16e93af6986e6329, 0x000005e06d444464,
	0x56975ad5726b84ed, 0x0000046667068a4f,
	0xc61a89f3c4647783, 0x0000023b5ed9e030,
	0x8682b561595ccb3c, 0x000007d97df13b14,
	0xc79d1b27b57c0173, 0x0000075e7761f878,
	0xc7152a872974924a, 0x000006a071831c6e,
	0xc6f743264c70c39e, 0x0000057d6dd4dc5e,
	0x069c67b5006bf59d, 0x000003cc67777747,
	0xb6269e031064e898, 0x0000015360ab4523,
	0x17e612f1426ccb60, 0x000007bf7c91637e,
	0xa7811e57977a71a0, 0x0000073775623476,
	0x17403026fa740292, 0x000006c671a3ad6b,
	0xe6d05216b16f745c, 0x000006646a460268,
	0xa63a82a632671703, 0x000005bc5fc97d5f,
	0x9573cc357b5b9b03, 0x000005ee5f8d5d53,
	0x47cd15b7eb7ee11f, 0x000007e17a51a37e,
	0x97442537dd7781f5, 0x000007d07402c67d,
	0xe7103e17c772c34a, 0x000007b26f048e7b,
	0x869e6397a66ca554, 0x0000078866c74079,
	0x45f79c977763586e, 0x0000074e5b4b5876,
	0xb5bde2d738570d22, 0x0000081d7f41147c,
	0x77a819e8217d0156, 0x0000082e77c1ef82,
	0xb7552c2835760250, 0x0000084273f34483,
	0x670548684c7243da, 0x000008626e154c85,
	0x06897358706b862f, 0x0000089265586388,
	0xa5dcb4c8a561b9bd, 0x000008ce59ad178b,
	0x37f41158335cae5f, 0x000008577e313483,
	0xf7891d785f7b6185, 0x0000087f78023986,
	0x975c3288927702a9, 0x000008c37433ba8a,
	0x17065228e0727462, 0x000009286e260090,
	0x368a8239536b86ff, 0x000009b965697598,
	0x660dc6e9f261eafa, 0x0000087861bdf497,
	0x07e612e83f7f111a, 0x000008a67d914587,
	0x87a520e8b27b41a9, 0x000008e579427b8c,
	0xf76b3829077802f6, 0x0000095c75342392,
	0xc7195ae9917384db, 0x00000a0f6f76a19c,
	0xc6a48faa596cf7b8, 0x00000ad1680a57aa,
	0xc6aacf8a09691b8b, 0x000008497ef11e91,
	0x87d514c87e7e2134, 0x000008f67e216b8b,
	0x27ca22793c7f418e, 0x000009697b42a594,
	0x478c3c699a7a132d, 0x00000a177764739d,
	0x974261ca6375e539, 0x00000b19724720ab,
	0x96fd96db8470384c, 0x00000ab4719a84b6,
	0x07ed1219d973abd5, 0x000008887e413985,
	0x57e61738c47dc153, 0x0000094e7f519790,
	0xd81d1ee9a08081c0, 0x00000a2b8022829f,
	0x37cb3d9a5b7e232f, 0x00000af97b7495aa,
	0xc78c658b5c7a2568, 0x00000c4677376cbc,
	0x37a395ec1677f863, 0x00000aaa7c8a93b7,
	0x17e613d8577ec124, 0x000008cd7e015889,
	0x97f719c90f7e9178, 0x000009ac8081c595,
	0x5833229a0a81c1f4, 0x00000aee84f265a7,
	0x28942f8b7786f2aa, 0x00000c2783b43ec1,
	0x6803640c8c81c536, 0x00000ce380f728d0,
	0xe854935c4c833810, 0x0000085c7ec126b8,
	0x37e215b8967e6140, 0x000009157eb17b8d,
	0x18071c795e7f819f, 0x00000a0e81b1f59b,
	0xc84b264a76831229, 0x00000b718682a6ae,
	0x08b2346c0788b2f1, 0x00000d6d8e13a5cb,
	0x190f50ce4091840f, 0x00000d5d8e566be4,
	0x07ec128c8c8e67ae, 0x000008997e714186,
	0x67ec17b8d57e415c, 0x0000095e7f819e91,
	0x78191f29ae8071c6, 0x00000a6c82e224a0,
	0xb86229badc84625c, 0x00000be98812e2b5,
	0x88d1389c878a6331, 0x00000dfc9033ecd3,
	0x09c3462ebc943450, 0x00000e189e354deb,
	0xa7e71428637eb129, 0x000008d47e615c89,
	0x77f919c9127ed17a, 0x000009a38071c195,
	0x782b21a9f88181eb, 0x00000ac084124ea5,
	0x98782cab3685b288, 0x00000c4a899313bb,
	0xc8ed3bcceb8c0363, 0x00000e5c92341ed9,
	0xaa0146beb697e458, 0x000007bb7c5169ea,
	0x07861dc7977a71a0, 0x0000074476022077,
	0xc7082c071373726c, 0x0000069f6d531e6d,
	0xf65a3fc65b69b387, 0x000005ba61247f60,
	0x15685b355b5c1511, 0x0000047d5056674f,
	0x85128293ef527742, 0x000002b74f992235,
	0xe7c816420b4f0a2a, 0x000007997a819d7b,
	0x275f2237707861dc, 0x0000070f73327274,
	0x46cb32f6d57022cb, 0x0000064b68d3a069,
	0xd5f94ac5f964741e, 0x000005355a154b59,
	0xb5146cc4c253f5fd, 0x000003a15117b943,
	0x34ce9d42fa4f08bb, 0x0000017f4bbb0124,
	0xc7ab1987c27cb15f, 0x000007727881d979,
	0xe733273743760221, 0x000006d27002cf70,
	0x26853ae68e6c6338, 0x000005eb63c43364,
	0xc58b57358a5e94c9, 0x000004a052263251,
	0x451081840953271a, 0x000002ae4e393036,
	0xd4f3b1d1e54b8a65, 0x000007c67cf15816,
	0x778c1d17a17af191, 0x0000074876321b77,
	0x47022cc71173526e, 0x0000068f6c73376d,
	0x763843a6406843b0, 0x000005815e14d75e,
	0x456965c50e580587, 0x000003e855274c48,
	0x84ee98233a522857, 0x000001a04bdacf27,
	0xc7d414e167537b27, 0x000007a87b51877c,
	0xf76921077e7921c7, 0x0000071973c26374,
	0x56cc32d6db7082c1, 0x000006466883a869,
	0x35e44d35eb63b434, 0x000005095ad58e58,
	0xa57d76147a5a866b, 0x000003275488763d,
	0x84dbb0c25c5109ae, 0x0000016157bb3117,
	0x07bc17a7d47da143, 0x0000078779a1b87b,
	0x4745251759772200, 0x000006e77122af72,
	0x36943946a26d731a, 0x000005f864741f65,
	0xf6075855915ef4be, 0x000004825e565f50,
	0xa5808703e15b6757, 0x0000025b5499af32,
	0xa526c4b2f34bcbb8, 0x000007dd7e21353e,
	0x37a41a67ba7c516a, 0x0000076777e1eb79,
	0x37082c1768746250, 0x000007556c233f76,
	0xc6184737426733cf, 0x0000070a61453d72,
	0xf5aa71e6e75e761d, 0x000006945638436b,
	0x24bab12664512992, 0x000005ff461cc863,
	0xe7e612d6a54c1d78, 0x000007fd7c416c7f,
	0xb76e2077fc79c1b4, 0x000007fb7392677f,
	0xa6b73537fa6fd2d5, 0x000007fa6683e37f,
	0xe63155d7f3657495, 0x000007e95fb6417e,
	0xd5728717e35bb746, 0x000007d65209c87d,
	0x546cd117ce4c8b51, 0x0000084c494e317c,
	0x37cb15f82e7f111a, 0x0000083b7a21a983,
	0x074025b8457741fc, 0x0000085f7042c885,
	0xc6a33dd86e6bf346, 0x0000088b68648a87,
	0x362463389e65a550, 0x000008cb5e57368b,
	0x45509b58e659e860, 0x000009234fbb3c90,
	0xc4bde399224aacf0, 0x000008427f011b84,
	0x47b318b8707e612e, 0x000008867841e087,
	0x57152a989c75023f, 0x000008d06ed3278b,
	0x06b74638ee6db3bb, 0x0000093768c52391,
	0x66206fc9646595ff, 0x000009ce5df81d99,
	0xf548ae7a0d597969, 0x0000099a52fc36a4,
	0xf7ed1218a6528dae, 0x000008827e213684,
	0xf7a41a58b97d514d, 0x000008e776c20b8c,
	0xd72d2f590a731276, 0x0000095871238192,
	0x36c94d998a6f0422, 0x00000a0469c5aa9c,
	0xf6307ada4d66969a, 0x00000af85f18e9a9,
	0xd5bdb54aef5c4a2a, 0x000009485bfcb5a2,
	0xf7df13b8597eb125, 0x000008cb7d115488,
	0x87b018f90d7c2170, 0x0000096b78f21895,
	0x475432799077129d, 0x00000a017343c09c,
	0x96ec531a4771246e, 0x00000af46c1611a9,
	0x965c839b5a691712, 0x00000b8664c940bc,
	0x265cb96ad6652a4f, 0x000008617eb129a0,
	0x77d115a8997e2141, 0x0000091c7be1768d,
	0xc7cf1c29687c2199, 0x00000a1d7dd1f19b,
	0xe79931ba747da248, 0x00000ad57743caa8,
	0x572d557b2e751486, 0x00000c07705645b9,
	0x46db836c856da754, 0x00000b946e992bc3,
	0x87eb12bad06f8a58, 0x000008a17e314486,
	0x57ca17c8df7d715f, 0x000009717cd19f92,
	0x77e11f69c67d51c8, 0x00000a947f022ba2,
	0xb8162abb0f802267, 0x00000c3982e2f7b9,
	0xa7a8514c6d7e0410, 0x00000d4377b61fcc,
	0xb7887e0d0177a6f9, 0x00000bb0794900c6,
	0x67e414786c7eb12e, 0x000008e47db1618a,
	0x47d31a29297d117f, 0x000009c97da1ca97,
	0x27f322ba277e61f7, 0x00000b0a804265a9,
	0x982e2f0b918172a6, 0x00000cd4849343c2,
	0x5890407d918693a0, 0x00000eaa8c946be6,
	0x9836780d7f84e638, 0x0000086f7eb12fca,
	0x57dc1628a97e5148, 0x000009287d51808e,
	0x37df1c89727d81a2, 0x00000a1f7e91f49c,
	0x780625ca857f6225, 0x00000b7781829aaf,
	0x584732dc0682d2df, 0x00000d55865383ca,
	0x88be43de198893e3, 0x00000ebe92d44dec,
	0x17eb130e31944525, 0x000008a87e614887,
	0x37d817e8e37de162, 0x000009697db19e92,
	0xd7ec1ec9b77e21c3, 0x00000a6c7f921aa0,
	0xd818287ad680724e, 0x00000bd082d2c7b4,
	0x286035cc6284430e, 0x00000db28813b3d0,
	0x28f9445e708aa411, 0x00000eb896e456ec,
	0x47a41a57b67c1170, 0x0000076d7831e179,
	0x273626d74275f223, 0x000006dd7092bf71,
	0xf69e3816a16d631a, 0x0000061565f3f365,
	0x75cc4fe5c3619471, 0x0000050257659a56,
	0x74ae704492517646, 0x0000039143d7d241,
	0x74299b63013c38af, 0x000007b97c316c25,
	0xc7831e27957a51a4, 0x0000074075d22776,
	0x57022cc70d732274, 0x000006956cc32d6d,
	0xe64c41564e69039b, 0x000005a560049f5f,
	0x254d5e45415ac539, 0x000004564e46a34d,
	0x73f085d3cd46f776, 0x0000029336595833,
	0xc7c61671cf3e0a87, 0x000007987a719f7b,
	0x175e22576f7851de, 0x0000070c73127674,
	0xf6c73376d16ff2d0, 0x000006446873aa68,
	0x25f04bd5f064042b, 0x0000052759556159,
	0xa4be6e74af52f61a, 0x0000039443f7cd42,
	0xb31a9e12ef3b38cb, 0x000001873daaf623,
	0xd7ac1987c17cb160, 0x000007737881d779,
	0xf73427174576121f, 0x000006d37012cd70,
	0x26853ad68f6c7337, 0x000005eb63c43364,
	0x95895775885e84cc, 0x0000049b51f63851,
	0xf42080640e4a6712, 0x000002bf38a91636,
	0x1417afe1ed382a58, 0x000007c77d015618,
	0xa78f1cc7a37b118d, 0x0000074c76721477,
	0xb7072c271773a265, 0x000006966cd32c6d,
	0xf63f42e64868a3a5, 0x0000058a5ea4c85e,
	0x751b63f518588578, 0x0000040549f72049,
	0x737693a36041381e, 0x000001c33c5a992a,
	0xf7d614a17b459b08, 0x000007ac7b917f7c,
	0x77702047847971bd, 0x0000072374425475,
	0x36d73196e77112b0, 0x000006556953916a,
	0x85f54b45fc64a418, 0x000005af57459e59,
	0x245979e5944ed692, 0x0000052a3b28ce56,
	0xe36cbdc4e139da3e, 0x0000051f3c7c9849,
	0x77c216f7d97de13b, 0x000007bb7981bb7b,
	0xe73526f7b676a20f, 0x000007a46f92dc7a,
	0xb6653e87986b4359, 0x0000077c60b48b78,
	0x752f61a76a5a4546, 0x000007424a970d75,
	0x63e396d72a411821, 0x000006df3aaae770,
	0x238ed806b9358c8f, 0x0000080f7e013973,
	0x47951c18117bd178, 0x0000081676721481,
	0xd6f52e381a732274, 0x000008226b036181,
	0xe6054978276603f1, 0x0000083559d55382,
	0x849e72183e52662b, 0x0000084747584b84,
	0xd3ddb1884a43599c, 0x0000085037ecc984,
	0x17f011b869382e0c, 0x000008447c716784,
	0xc77020484f79e1b1, 0x0000086b73b26485,
	0xf6ba34f87c6ff2d1, 0x000008a666b3de88,
	0xd5a953d8c0610482, 0x000008fe5346128d,
	0xc4dc83a91a51a716, 0x0000096148798693,
	0x13d9c8e98a429b01, 0x0000086e3bee0496,
	0x17e21358527ed122, 0x0000088b7b218d88,
	0x574d24489e7821e4, 0x000008d17122af8b,
	0x76823b48f26cf329, 0x0000094262a45391,
	0x15ba5e59735c6508, 0x000009d95826dd9a,
	0xb4e7938a1653a7f6, 0x00000a9e48caa6a5,
	0xf457d3e9d746cbd9, 0x0000085e7ea1278e,
	0xb7d11548927de13c, 0x000008f17ac1978c,
	0x673127690476d209, 0x000009516f02ed92,
	0xc65240b9836a6373, 0x000009f564d4c49b,
	0x75e467ba3961f591, 0x00000adc5a0786a8,
	0xe51e9e1b3b5548b8, 0x00000a65507afdb1,
	0x97e912c98b4f9c4d, 0x000008a07dc14286,
	0xf7be1778dc7cd15b, 0x0000096c7ac19791,
	0xb72728799b7761f8, 0x000009f16e330f9b,
	0x66b2456a2d6d83ab, 0x00000aca683515a7,
	0x06126e9b2864e5f0, 0x00000c025cf806b9,
	0xb5a89febb35b48fb, 0x00000a415a1b36b0,
	0x97e01478717e912f, 0x000008e87cf1618a,
	0xc7a819e92e7ba17d, 0x000009d57941c297,
	0x579f224a387891ee, 0x00000ac675e2edaa,
	0x06fc461b0772b3a5, 0x00000bc66cb52fb6,
	0x765d71fc39697617, 0x00000c5f6517f3cb,
	0x964aa02bc664d8df, 0x000008777ea132b0,
	0xf7d51668b07e214b, 0x000009367c11828e,
	0xd7a11c99857a61a2, 0x00000a3e7a81f79d,
	0x87ba265aac7b122a, 0x00000bb37c52a7b2,
	0x47bd38bc517d32f1, 0x00000d037524d1cd,
	0xc6fe6b8d727115e1, 0x00000c986f979ad2,
	0xc7e9134be16f18b4, 0x000008b57e314d87,
	0x87c91858f37d8168, 0x000009867b91a593,
	0xb7b71f89dc7b41cc, 0x00000aa67bd22aa3,
	0x57cf2a2b1e7c5262, 0x00000c3c7dc2e9ba,
	0x0800393ce47ec339, 0x00000e6f81a3f6da,
	0xf7d25eded185842f, 0x00000cd37a0740da,
	0x67e414e87e7e9135, 0x000008f37db1688b,
	0x17c21a59377cf185, 0x000009d47bf1ca98,
	0x57c6224a2f7c11f4, 0x00000b077ce259a9,
	0x37e42d8b867d8295, 0x00000cb07f4323c1,
	0xd81f3d2d5e807376, 0x00000ed384442ce1,
	0x28d54c2eca8a3439, 0x0000087f7e9136e7,
	0x07dd1678b57e414e, 0x000009307d21838f,
	0x47c61c49777c91a1, 0x00000a1b7c81ec9c,
	0x37d624aa7a7ce219, 0x00000b587df281ae,
	0x87fb303bd97ec2bf, 0x00000d0480d34ec6,
	0x78423fadaf8243a1, 0x00000ece87e434e6,
	0x37be176ec48e4443, 0x000007917a21a97b,
	0x275f22376c7821e2, 0x0000071473826a74,
	0x86dc3106e170d2b8, 0x000006696a63726a,
	0x462845762266a3df, 0x0000057d5de4dc5d,
	0x553461051e58e56f, 0x000004424d36c14b,
	0x13f984d3c6469781, 0x000002b838492034,
	0x27a31a87b57c0173, 0x0000076b7821e479,
	0xf73327274075d227, 0x000006d97052c570,
	0x769838c69b6d1324, 0x0000060b65740265,
	0x85be5165b660f485, 0x000004ee5655b855,
	0x949573147a50266c, 0x0000036d41e8093f,
	0x83189e52d639e8f2, 0x000007b87c316e23,
	0xd7831e17957a51a3, 0x0000074075d22676,
	0x57022cb70e732273, 0x000006956cc32d6d,
	0xc64a41864d68f39c, 0x000005a25fd4a45f,
	0xb5475ee53d5a7540, 0x0000044d4dc6b04c,
	0x73e38753c1465788, 0x0000028035597732,
	0xd7c71661ce2bea88, 0x0000079a7a919c7b,
	0x47602207727871da, 0x0000071173526f74,
	0x56cc32e6d77042c9, 0x0000064a68c3a069,
	0x85f54b35f7646421, 0x0000052e59b55659,
	0x14c46dc4b753660d, 0x0000039c4467c143,
	0x23209d62f73ba8c0, 0x000001a429aac924,
	0x17af1927c37cd15c, 0x0000077978d1cf7a,
	0x873b26474b767215, 0x000006dd7092be71,
	0x069139969b6d1325, 0x000005fa64941b65,
	0xd59a55859a5f74b0, 0x000004b253261552,
	0xd4397d94284bc6ea, 0x000002df3a68e438,
	0x7287aeb221304a09, 0x000007cb7d31501f,
	0x37961c07aa7b7184, 0x0000075677020478,
	0xa7142ab723745252, 0x000006fb6c83346e,
	0x562445e6ed67b3c0, 0x000006b85c350d6d,
	0xc4d96b76935555d5, 0x0000064044c7b666,
	0x12f9a1d6113ac8d7, 0x000005b522eb8d5e,
	0x87d81465f519dc94, 0x000007e67b61857e,
	0xe7612207e378e1ce, 0x000007d972d27e7d,
	0xd6ad3657d36f12ea, 0x000007c66603f17c,
	0x55a45477bd608491, 0x000007ab5326167b,
	0x641e80a7a14b0701, 0x0000078937793979,
	0x11e8c0c77d2bba8d, 0x0000079c1f9d6677,
	0x37b818181f7db141, 0x000008297901ca82,
	0x672e27c82f76221d, 0x0000083e6f22e983,
	0x165f3f28476ad365, 0x0000085d60649485,
	0xa52e61d86a5a154c, 0x0000088b4aa70b87,
	0x436c94d89f41581a, 0x000008ca2acaa88b,
	0x6245ddf8e11d5c2f, 0x000008507ed12188,
	0x379b1b68567c416b, 0x0000087276d20986,
	0x76fd2d4883739268, 0x000008ae6b934f89,
	0x661447b8c966c3db, 0x000009085b05318e,
	0x94bc6eb92e53e600, 0x000009884287f895,
	0x42c2a829bd380928, 0x000009bc2dfc029f,
	0x07e91288d62ebd64, 0x000008907de13c86,
	0x47821e48a27b318a, 0x000008cd74d2448b,
	0xe6d03268eb7132ad, 0x000009366853ae90,
	0x85cf4f9964630449, 0x000009d25605c199,
	0xc4537a9a134e26a6, 0x00000aac3b08d1a5,
	0x93a7b5bae63b5a38, 0x0000094f399caaa2,
	0x17da14386d7e612d, 0x000008db7cd15b8a,
	0x07731ff91c7be176, 0x0000094273626e92,
	0x06ac36896d6f42e5, 0x000009da65a3fd9a,
	0x9594563a1d5fd4a4, 0x00000abe51d63ca6,
	0x24a9862b154d173a, 0x00000b5e47797fb7,
	0x444ebc4ab045fa8a, 0x000008777e71329e,
	0xb7ca1618ae7da148, 0x0000092f7ba17e8e,
	0x27951c197c7a819e, 0x000009dd7342709d,
	0x269838ca106e82fb, 0x00000a9f64042ca5,
	0x45c45b5af85dd4df, 0x00000bba5916a5b5,
	0xd5288a2c2b54c7b6, 0x00000b4f513996be,
	0xf7e8136a90503abc, 0x000008b77de14d87,
	0xc7b71838f67cd167, 0x0000098b7a51a493,
	0x677a1f29e37911c8, 0x00000ab7760221a4,
	0x96bc368b14727289, 0x00000b896a1429b3,
	0xb6345d4beb66e4f3, 0x00000cd55f36d1c5,
	0x25c7882c935d77a2, 0x00000b4f5b7996c0,
	0xe7e01518857e8138, 0x000008fc7d316b8b,
	0x27a31a79437bf188, 0x000009ea78f1cb99,
	0xc75f223a4d7781f4, 0x00000b3676725cab,
	0x877b2e4bbe77329d, 0x00000d03784334c5,
	0xe6bd589d3271446d, 0x00000d5e69466ad8,
	0xd669858cce67f748, 0x000008897e913ac1,
	0x07d716d8c17e1153, 0x000009447c818a90,
	0x87931cb9917b21a9, 0x00000a487861f59e,
	0x778a25cab1787225, 0x00000baa78e298b2,
	0x179b329c3e7932dd, 0x00000d957a537dce,
	0xd7dd41de5c7b33db, 0x00000de676a599ed,
	0xb7e813bd0471f6f5, 0x000008c27e215388,
	0x17cd1898fe7d916d, 0x0000098b7bd1a894,
	0x87a01f29dd7aa1cb, 0x00000a9c79e220a3,
	0x77a128cb0b79e253, 0x00000c107a62cbb8,
	0xe7b7360ca87ad312, 0x00000e047c63b6d4,
	0x6826428ec97db414, 0x00000ecd889436ed,
	0x07e315288b7e913b, 0x000008f97dc16b8c,
	0xe7c41a49397d1186, 0x000009cb7b61c597,
	0xd7ac214a207ae1ea, 0x00000ae37ad243a7,
	0x27b62b2b557b0278, 0x00000c5b7bd2f2bd,
	0x37d6385cf17c8338, 0x00000e417eb3d9d9,
	0x0867431ed880f424, 0x000007b07bc179ed,
	0xd7831e17917a11aa, 0x0000074676221e76,
	0x97132ac71a73d261, 0x000006b36e62ff6e,
	0x667b3c06786b335b, 0x000005ed63d43063,
	0x55af53359d5f94ab, 0x000004e555d5c754,
	0xb4a471747b504669, 0x0000039343e7cf40,
	0x27be1773193d688c, 0x000007917a21aa7b,
	0x275f22376c7821e3, 0x0000071473726a74,
	0x76db3136e070c2ba, 0x000006666a43766a,
	0xf62445e61f6673e4, 0x000005775d94e55c,
	0xa52b62151658657c, 0x000004354c86d64a,
	0xd3e886c3b545b79a, 0x0000029f36f94732,
	0x37a41a67b57c0172, 0x0000076d7831e179,
	0x273626d74275f223, 0x000006dc7082c071,
	0xb69b38669f6d531e, 0x0000061065b3fb65,
	0xc5c250f5bb61347d, 0x000004f35695b155,
	0xe49972b47f506664, 0x000003714228033f,
	0xa3199e32d93a18ed, 0x000007ba7c416b23,
	0x27871da7987a819f, 0x0000074676221d77,
	0xd7092be715738269, 0x0000069f6d431f6d,
	0x965540565869838b, 0x000005b160a48d60,
	0xe5585d154e5b6526, 0x000004634ef68f4d,
	0x23fa84b3d947a763, 0x0000029d36e94934,
	0x17ca1601ee2d9a56, 0x0000079f7ae1947c,
	0xd76821277978d1cf, 0x0000071c73e25e74,
	0x56d93166e470f2b4, 0x0000065d69c3846a,
	0x260b48b60c658400, 0x0000054c5b55285b,
	0x94e669e4d95535d9, 0x000004824427c845,
	0x42e2a4647a3998fb, 0x0000046021abb146,
	0x87b61857c97d1153, 0x000007837961c07a,
	0x1733272786767214, 0x000007766fa2da78,
	0x66713d27676ba34e, 0x0000074161f46775,
	0x15595ce72b5c2510, 0x000006f64e36a571,
	0x73c78a76d845e796, 0x000006973209d76b,
	0x4197c9f679265b2a, 0x000007ff7d115466,
	0xd7881d87fe7af191, 0x000007fd75c2297f,
	0xa6ef2ee7fc729285, 0x000007f96ad3667f,
	0x760e4877f86623ed, 0x000007f55ad5357f,
	0x24c66d97f45415fa, 0x000007f143b7d57f,
	0xf2efa2f7f039f8f1, 0x000007ee22cb917e,
	0xe7d81487ed155d16, 0x000008347b418882,
	0x576022183c78d1cf, 0x0000084f72c27e84,
	0x76b036085b6f22e8, 0x000008766643e986,
	0x95ae53488660f484, 0x000008ae5405fc89,
	0xf4377dc8c64c46dd, 0x000008fb3988fc8d,
	0x621eba99192e6a40, 0x00000906153d1a93,
	0x87c416c85e7ea127, 0x0000087579a1b886,
	0x973926888676c20b, 0x000008b06ff2d089,
	0x66733cf8ca6be347, 0x0000090761e4688e,
	0x65525db92c5bf515, 0x000009844d76ba95,
	0xe3ae8d49b744c7b6, 0x00000a292fda169e,
	0x01a0c8fa22244b64, 0x0000086e7e612d96,
	0x97b818189e7db141, 0x000008c97841e08b,
	0x27162a68e275023e, 0x000009266d631b90,
	0xf63b43395068d39f, 0x000009b45df4dc97,
	0x34fe6729f057559b, 0x00000a7c478766a3,
	0xf3339b4acb3df87c, 0x00000a922a4ab8b1,
	0xa7e31339cc285be9, 0x000008ae7d714887,
	0x97bb17b8e87ca160, 0x0000093b77c1ee92,
	0x46fc2d595973d260, 0x000009b66b635698,
	0x460f4859f16673e4, 0x00000a7f5ab53aa3,
	0xf4ba6efad353a607, 0x00000b924297f6b2,
	0x93169e9bb0396901, 0x00000a4f39cb20b1,
	0xa7d814e8847e5137, 0x000008f77c71668b,
	0x67a61a293b7b7182, 0x000009dc7921c598,
	0x96f82dea0074924a, 0x00000a686a936ea2,
	0x85f54b4ab2653408, 0x00000b6858b574b0,
	0x448c742bd151364d, 0x00000c3844e82fc4,
	0xd472a2db9f47391a, 0x0000088c7e613bae,
	0x17cb16c8c37dc153, 0x000009467b518790,
	0xa78f1cb9937a31a7, 0x00000a4b7791f49e,
	0x3744254ab8760221, 0x00000b596cf327b3,
	0x15fd4a5ba16663e6, 0x00000c6e58b574c0,
	0xf556747cdd570669, 0x00000c4654881acc,
	0x17e713eb9e53791d, 0x000008c97de15689,
	0xb7be18b9067d116f, 0x000009997a21aa94,
	0xf7781f59ef78e1ce, 0x00000abb75f222a4,
	0xa72528cb33744254, 0x00000c507022cbbb,
	0x16e33d8cf4705316, 0x00000d8e66f522d6,
	0x66116f2d9162f61b, 0x00000c5d5f27f5d0,
	0xb7e01578957e713f, 0x000009087d51718c,
	0x77b11ac94c7c718d, 0x000009ec78f1cc99,
	0x176221ea497791f3, 0x00000b2474724eab,
	0xe7452c7ba4731285, 0x00000cc974b30fc2,
	0xd7593b5d7375135e, 0x00000ee776a40de2,
	0x46ae6ace1670954e, 0x000008957e7140d3,
	0x67d81708cb7e1157, 0x000009477cc18c90,
	0xe7a61ca98e7bc1aa, 0x00000a3778a1ee9d,
	0x476f247a99776217, 0x00000b7a76e27cb0,
	0xb76f2fabfc76d2b8, 0x00000d27773342c8,
	0x67883e6dd177b391, 0x00000ee07b5418e8,
	0x57e813fed880a424, 0x000008c87e215689,
	0xc7d11889007db16e, 0x000009807c31a493,
	0xc7a01e59ca7b31c3, 0x00000a7679320ca1,
	0x4789269ad878c238, 0x00000bba78829eb4,
	0x678e31ac3b78a2da, 0x00000d5d796361cc,
	0x27b93fcdfd7a33ac, 0x00000eda7f6421ea,
	0x17a21a97b07bc17a, 0x000007707861dc79,
	0x374425474b766215, 0x000006f571e29972,
	0xd6c533a6c46f32e6, 0x0000065069239768,
	0x561c46d60e6593fd, 0x000005755d84e85c,
	0x153f5fe51f58f56d, 0x0000045d4ea6984c,
	0x94367de3f4491739, 0x000007b17bd17938,
	0xe7841df7917a21a9, 0x0000074776321b76,
	0xc7152a871c73e25e, 0x000006b66e82fb6e,
	0x967e3bb67b6b6356, 0x000005f064042b63,
	0x85b152f5a05fc4a7, 0x000004e755f5c454,
	0xc4a571547d505666, 0x0000039343e7cf40,
	0x47bf1743173d588d, 0x000007937a41a67b,
	0x776321c76f7851de, 0x0000071a73d26174,
	0xf6e13066e77122af, 0x000006706ac3676a,
	0xb62e44b6296703d3, 0x000005855e54d15d,
	0xc53a606525594563, 0x000004484d86b84b,
	0x43fb8483cb46d779, 0x000002b838492034,
	0x87a819f7b87c316d, 0x000007737881d779,
	0xc73e25e74a766216, 0x000006e87122ae71,
	0xc6a936c6ad6e0308, 0x0000062366b3de66,
	0x65d84e75d162645b, 0x0000051158258357,
	0x54ba6ef4a0523630, 0x0000039d4477bf42,
	0x134898e30b3cb8a0, 0x000007bf7c916327,
	0xa78f1cc79f7ae194, 0x0000075176b20c77,
	0xe7172a4723744254, 0x000006b26e53006e,
	0x46574016706ad366, 0x0000067560049e68,
	0x55396086585a254a, 0x0000060e4c46dd63,
	0xd3b28ce5e54427c8, 0x0000059a3129f05b,
	0x87d0155586265b2a, 0x000007c87ae1937c,
	0x075e2257c67881d8, 0x000007b972e27c7c,
	0x76ba34e7b06f72df, 0x0000079b6753cb7a,
	0x15d04f778e627459, 0x0000077156d5aa78,
	0xe4837527604ff672, 0x0000073b3f984d74,
	0x62b7a95728360963, 0x0000070a1ffbe271,
	0x17ab1988107cd15c, 0x000008147861dd81,
	0x872928481675a22b, 0x0000081b6f22e981,
	0x266c3dc81f6b335b, 0x0000082661c46d82,
	0x055b5ca82b5c1512, 0x000008364e969983,
	0x43d988783d469782, 0x0000084b3399a984,
	0x81c8c46852288ae9, 0x0000083c7d514c85,
	0xe78c1d18447b318b, 0x0000085976022184,
	0x36f72e086572f27a, 0x000008836b835287,
	0x961f4678956703d4, 0x000008bf5c450c8a,
	0x44ea6988d855d5c7, 0x000009124677848f,
	0x43339b49323d688d, 0x0000097427fafb95,
	0xa7e712c9821bdc5b, 0x000008797c416b86,
	0x776e20888579a1b7, 0x000008ac73c26189,
	0x06c63378c47052c6, 0x000008fe6803b88e,
	0x75d64ec921630449, 0x000009725705a494,
	0x647d75d9a24fe674, 0x00000a0e3ed8629d,
	0x0297acfa4834b988, 0x000009d5203bdba8,
	0x97d91468797e4132, 0x000008d37c216f8a,
	0x47552358db7881d8, 0x0000091371d2998f,
	0x269b3869386e0309, 0x0000099164d41396,
	0x15945639c65f64b1, 0x00000a4352662ca0,
	0x841d80ca8a4a970e, 0x00000b2737f92aad,
	0x3260b32aeb2efa2f, 0x000008867e1137a4,
	0x17c81648b97d514c, 0x000009317ba17e8f,
	0xc74824d95578b1d3, 0x000009947092bf96,
	0xd67a3c29c56c533a, 0x00000a3e62645a9f,
	0x75605c2a875c9503, 0x00000b2f4e9699ad,
	0xd3ce89bb8d46478a, 0x00000b6935996ebe,
	0xf7e413cac92d2a64, 0x000008c47d615288,
	0x27b61858ff7c516a, 0x0000098b7a51a494,
	0x777a1f29de7921c6, 0x00000a3a70c2b8a3,
	0x866c3dba726be346, 0x00000b0961347dab,
	0x55405fbb625b0531, 0x00000c2e4c36ddbc,
	0x83d3892c7743e7cf, 0x00000b6135397abf,
	0xc7da1568967e5140, 0x000009087ca16f8c,
	0x67a21a994c7b318a, 0x000009ea78f1cb99,
	0xf76221da4777a1f2, 0x00000b2374724eaa,
	0x16893a7b777022cb, 0x00000bf4622461ba,
	0x253f5fec575b6526, 0x00000d0f4c06e4cc,
	0x44af898c9845b79b, 0x0000089b7e6142bf,
	0xd7d01728d17dd159, 0x0000094f7bd18d90,
	0xd78e1cc99a7a11aa, 0x00000a4877a1f29e,
	0x074824caae76221d, 0x00000b9f72b281b2,
	0x76e72fcc2b70b2bb, 0x00000d726c0343cc,
	0xe58e5c3d725f74b0, 0x00000d3f59869bdb,
	0xe7e6143c9f583790, 0x000008d37df15a89,
	0xe7c618e90d7d4173, 0x000009967b21ab94,
	0xf77c1ef9e77901ca, 0x00000aa1765218a3,
	0x6731277b0e74c245, 0x00000c0b7122aeb8,
	0xe6cc32ec9e6f02eb, 0x00000de56f3381d3,
	0x86a051ae987083d7, 0x00000d5f644669e3,
	0x17e015a89e7e6143, 0x000009097d71728d,
	0xc7bc1a99477cc18c, 0x000009d77a71c798,
	0x976b20da2c7851e8, 0x00000aee753238a8,
	0x971e299b5e73a266, 0x00000c5c72a2d7bd,
	0x4737361cea73131a, 0x00000e277403aed8,
	0x1751492ecf7533fe, 0x0000089c7e7142e9,
	0x17da16f8cc7e1158, 0x0000093c7d018790,
	0x17b31bf97b7c31a2, 0x00000a0f79e1df9c,
	0x176e228a64783201, 0x00000b24763253ac,
	0x875d2bab9275f284, 0x00000c8775e2f4c0,
	0xe76a376d0f762334, 0x00000e3077b3bcd9,
	0x07bc17aebb79a402, 0x000007947a41a57b,
	0x376d20877578a1d4, 0x0000072e74e24275,
	0x87052c770572b281, 0x000006a76db3136d,
	0x567a3c26706ad366, 0x000005f564442563,
	0x35c85055ae608491, 0x0000051158358356,
	0x04ed6934bb539608, 0x000004064a071e46,
	0x47a41a57b17bd179, 0x000007737881d779,
	0x774824d74f76a20f, 0x000006fb72229172,
	0x46cb32f6ca6f92dc, 0x0000065969938a69,
	0x062545d6176613ef, 0x000005825e24d55d,
	0x054c5e652d59a558, 0x0000046e4f867e4d,
	0xc4467c14064a071e, 0x000007b47bf17439,
	0x47891d77957a51a3, 0x0000074e76921077,
	0x671e299725746251, 0x000006c26f22e86f,
	0x968b3a26896c1340, 0x0000060365040e64,
	0x05c65095b660e485, 0x0000050357759856,
	0x04c36dd49e521635, 0x000003bb46179143,
	0x87c316e3443fb849, 0x0000079a7a919c7b,
	0x176b20c77878c1d1, 0x0000072674724e75,
	0x06f12eb6f671e298, 0x000006846be3476c,
	0x864741e6426853ad, 0x000005a660149d5f,
	0x85605c254c5b5528, 0x0000047b50366a4e,
	0x33f185a440491739, 0x000004c635b96b4a,
	0x17af1927bf7c9163, 0x0000077e7921c67a,
	0xc745252757771202, 0x0000074e7102b274,
	0x46993897446d7319, 0x0000072165440773,
	0x35b252e70c607492, 0x000006d85535da6f,
	0xe47576c6bc4e9699, 0x000006823f485569,
	0xa2d2a63669368953, 0x000007ec7c816465,
	0x67851de7e97a819d, 0x000007e375c2277e,
	0xb6fc2d77df72f27a, 0x000007d66c233f7d,
	0xa6394387d06823b4, 0x000007c35e84cb7c,
	0x45296257bc58e56f, 0x000007ac4b96f07b,
	0xa3b48ca7a343d7d2, 0x0000079231f9d979,
	0xe7ca16178c27fafb, 0x000008227a919c81,
	0xb75c2298277851de, 0x0000083072d27d82,
	0xe6be3478366f92dc, 0x0000084567b3c083,
	0x65de4de84d631446, 0x0000086158058785,
	0x84a471786c518644, 0x0000088542380187,
	0xe2f8a1f892394904, 0x000008a7250b4f89,
	0x27b218c8497d414e, 0x0000085c78c1d085,
	0x773427186976321b, 0x000008876ff2d187,
	0xc6813b68996c433c, 0x000008c363643d8a,
	0x75845818db5e24d6, 0x0000091451a6408f,
	0x64228039344a5715, 0x0000097939190a95,
	0x4245b6499a2f1a2b, 0x000008747e51309b,
	0x379d1b38887c7166, 0x000008a577220189,
	0x370e2b58ba743256, 0x000008ef6d33208d,
	0x164741e90e691398, 0x000009575f44b593,
	0x152f61a98259755e, 0x000009e44ba6ee9b,
	0x13a88dfa1a4397d9, 0x00000a85309a01a5,
	0x37e1136a55270b16, 0x000008b27d714a88,
	0x978f1cc8e67cb15f, 0x0000090175c2278e,
	0x36ed2f191f728287, 0x0000096b6ad36694,
	0xc61447a9996653e9, 0x00000a055ba51e9c,
	0x64e46a2a435555d5, 0x00000ace466786a8,
	0xc34199ab173da885, 0x00000ab92c4a7db4,
	0x07d315088f7df13b, 0x000008f77c71668c,
	0x97a919c9347b9180, 0x0000097775523597,
	0xa6d731999c7172a4, 0x000009ff6913989c,
	0x05ed4c1a3c644425, 0x00000aca58d56fa8,
	0x04aa70cb1a522632, 0x00000bc6423800b7,
	0x132a9c4bbe3a28eb, 0x000008987e2140b3,
	0x47c416c8cb7d4155, 0x000009437b518690,
	0x87931c49897a51a3, 0x00000a2f77f1e89d,
	0xf6d631aa4172528c, 0x00000aae6893a7a6,
	0x85da4e4af763643e, 0x00000ba157559bb4,
	0xf484750bff50366a, 0x00000c41411821c5,
	0xe7e4143bb939e8f2, 0x000008d27d915989,
	0xc7b318a90b7c9170, 0x000009927a31a794,
	0x977d1ec9e17911c8, 0x00000a9a767215a3,
	0xe732273b0674e242, 0x00000b8c6ae364b7,
	0x55e44d2bd064a419, 0x00000c82577598c2,
	0x148b743ce14fd675, 0x00000c4e41c80dcd,
	0x57dc15b8a27e4145, 0x0000090f7cf1738d,
	0x57a31a894e7bd18c, 0x000009e27911c899,
	0x7767213a3777d1ec, 0x00000b0074f23fa9,
	0x47172a5b7473526f, 0x00000c816f72dfbf,
	0x262645bd1b6d431f, 0x00000d5d59955ad2,
	0x74d5721d74515649, 0x000008a47e5146ce,
	0xe7d41738d67de15c, 0x0000094b7c618d90,
	0xa7931c598e7b21a8, 0x00000a2c7801e79d,
	0xb754236a8776b20d, 0x00000b5973b264ae,
	0x67012cdbd271f296, 0x00000ce56e0309c5,
	0x369738dd7f6bd349, 0x00000e166004e9e2,
	0x37e6146d865d662d, 0x000008d37e015a8a,
	0x27cc18a9087d7171, 0x000009827bd1a494,
	0x87861df9c97a81c1, 0x00000a6c771202a1,
	0xf745252ac975c228, 0x00000b9d72b280b2,
	0x86f22e9c167102b3, 0x00000d236d2322c9,
	0x070d3afdb06ed368, 0x00000e3d6bd4cce4,
	0xd7e115889f7e6144, 0x000008ff7da16d8c,
	0x07c419d9357d0184, 0x000009b17b51b897,
	0x67811f39f87a01d5, 0x00000a9b766215a4,
	0xa73b264af775223b, 0x00000bc5722291b5,
	0xc72d2fdc357202c4, 0x00000d28738337ca,
	0xa7663aeda4748373, 0x00000000000000e1,
};

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

void tv_dolby_vision_config(int setting)
{
	unsigned int delay_tmp, el_41_mode, bl_enable;
	unsigned int el_enable, scramble, mode;
	unsigned int hsize, vsize;
	delay_tmp = 10;
	bl_enable = 1;
	el_enable = 0;
	el_41_mode = 0;
	scramble = 1;
	mode = 1;
	hsize = 1920;
	vsize = 1080;

	pr_info("dm_addr:%p\n", dma_vaddr);
	if (setting == 1) {
		el_enable = 1;
		el_41_mode = 1;
		scramble = 0;
		memcpy(dma_vaddr, dma_data_tv,
			sizeof(uint64_t) * TV_DMA_TBL_SIZE);
	} else {
		memcpy(dma_vaddr, dma_data,
			sizeof(uint64_t) * TV_DMA_TBL_SIZE);
	}
	WRITE_VPP_REG(DOLBY_TV_SWAP_CTRL0, delay_tmp << 4 |
		bl_enable << 0 | el_enable << 1 | el_41_mode << 2);
	WRITE_VPP_REG(DOLBY_TV_SWAP_CTRL1, ((hsize + 0x100) << 16 |
		(vsize + 0x40)));
	WRITE_VPP_REG(DOLBY_TV_SWAP_CTRL3, 0x600004);
	WRITE_VPP_REG(DOLBY_TV_SWAP_CTRL4, 0x40004);
	WRITE_VPP_REG(DOLBY_TV_SWAP_CTRL2, (hsize<<16 | vsize));
	WRITE_VPP_REG(DOLBY_TV_AXI2DMA_CTRL1, 0x6f666080);
	WRITE_VPP_REG(DOLBY_TV_AXI2DMA_CTRL2, (u32)dma_paddr);
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
	/* bypass dither */
	WRITE_VPP_REG_BITS(DOLBY_TV_SWAP_CTRL6, 1, 25, 1);
	WRITE_VPP_REG_BITS(VIU_MISC_CTRL1, 0, 16, 2);
}

void tv_dolby_vision_crc_clear(int flag)
{
	crc_outpuf_buff_off = 0;
	crc_count = 0;
	if (!crc_output_buf)
		crc_output_buf = vmalloc(CRC_BUFF_SIZE);
	pr_info(
		"tv_dolby_vision_crc_clear, crc_output_buf %p\n",
		crc_output_buf);
	if (crc_output_buf)
		memset(crc_output_buf, 0, CRC_BUFF_SIZE);
	return;
}

char *tv_dolby_vision_get_crc(u32 *len)
{
	if ((!crc_output_buf) ||
		(!len) ||
		(crc_outpuf_buff_off == 0))
		return NULL;
	*len = crc_outpuf_buff_off;
	return crc_output_buf;
}

void tv_dolby_vision_insert_crc(u32 crc)
{
	char str[64];
	int len;
	if (!crc_output_buf)
		return;
	memset(str, 0, sizeof(str));
	snprintf(str, 64, "crc(%d) = 0x%08x", crc_count - 1, crc);
	len = strlen(str);
	str[len] = 0xa;
	len++;
	memcpy(
		&crc_output_buf[crc_outpuf_buff_off],
		&str[0], len);
	crc_outpuf_buff_off += len;
	return;
}

