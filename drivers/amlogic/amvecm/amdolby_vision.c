#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
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

static const struct dolby_vision_func_s *p_funcs;

#define DOLBY_VISION_OUTPUT_MODE_IPT			0
#define DOLBY_VISION_OUTPUT_MODE_IPT_TUNNEL		1
#define DOLBY_VISION_OUTPUT_MODE_HDR10			2
#define DOLBY_VISION_OUTPUT_MODE_SDR10			3
#define DOLBY_VISION_OUTPUT_MODE_SDR8			4
#define DOLBY_VISION_OUTPUT_MODE_BYPASS			5
static unsigned int dolby_vision_mode = DOLBY_VISION_OUTPUT_MODE_SDR10;
module_param(dolby_vision_mode, uint, 0664);
MODULE_PARM_DESC(dolby_vision_mode, "\n dolby_vision_mode\n");

/* if sink support DV, always output DV */
/* else if sink support HDR10, always output HDR10 */
/* else always output SDR */
#define DOLBY_VISION_FOLLOW_SINK		0
/* output DV ooly if source is DV and sink support DV */
/* else if sink support HDR10, always output HDR10 */
/* else always output SDR */
#define DOLBY_VISION_FOLLOW_SOURCE		1
/* always follow dolby_vision_mode */
#define DOLBY_VISION_FORCE_OUTPUT_MODE	2
static unsigned int dolby_vision_policy = DOLBY_VISION_FOLLOW_SINK;
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
#define FLAG_CLKGATE_WHEN_LOAD_LUT	0x02
#define FLAG_BYPASS_CSC				0x04
#define FLAG_BYPASS_CVM				0x08
#define FLAG_USE_SINK_MIN_MAX		0x10
#define FLAG_DISABLE_COMPOSER		0x20
#define FLAG_TOGGLE_FRAME			0x80000000
static unsigned int dolby_vision_flags =
	FLAG_RESET_EACH_FRAME | FLAG_BYPASS_CVM | FLAG_USE_SINK_MIN_MAX;
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

static unsigned int dolby_vision_target_min = 50; /* 0.0001 */
static unsigned int dolby_vision_target_max = 1000;
module_param(dolby_vision_target_min, uint, 0664);
MODULE_PARM_DESC(dolby_vision_target_min, "\n dolby_vision_target_min\n");
module_param(dolby_vision_target_max, uint, 0664);
MODULE_PARM_DESC(dolby_vision_target_max, "\n dolby_vision_target_max\n");

static unsigned int dolby_vision_graphic_min = 50; /* 0.0001 */
static unsigned int dolby_vision_graphic_max = 300;
module_param(dolby_vision_graphic_min, uint, 0664);
MODULE_PARM_DESC(dolby_vision_graphic_min, "\n dolby_vision_graphic_min\n");

static unsigned int debug_dolby;
module_param(debug_dolby, uint, 0664);
MODULE_PARM_DESC(debug_dolby, "\n debug_dolby\n");

static unsigned int debug_dolby_frame;
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

static bool dolby_vision_on;
static unsigned int frame_count;
static struct hdr10_param_s hdr10_param;
static struct dovi_setting_s dovi_setting;

int dolby_core1_set(
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

int dolby_core2_set(
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

int dolby_core3_set(
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

	VSYNC_WR_MPEG_REG(DOLBY_CORE3_SWAP_CTRL1,
		((hsize + htotal_add) << 16)
		| (vsize + vtotal_add + vsize_add));
	VSYNC_WR_MPEG_REG(DOLBY_CORE3_SWAP_CTRL3, 0x800020);
	VSYNC_WR_MPEG_REG(DOLBY_CORE3_SWAP_CTRL4, 0x40008);
	VSYNC_WR_MPEG_REG(DOLBY_CORE3_SWAP_CTRL2,
		(hsize << 16) | (vsize + vsize_add));
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
static uint32_t vpp_matrix_backup;
static uint32_t vpp_vadj_backup;
static uint32_t vpp_dummy1_backup;
void enable_dolby_vision(int enable)
{
	if (enable) {
		if (!dolby_vision_on) {
			dolby_ctrl_backup = READ_VPP_REG(VPP_DOLBY_CTRL);
			viu_misc_ctrl_backup = READ_VPP_REG(VIU_MISC_CTRL1);
			vpu_hdmi_fmt_backup = READ_VPP_REG(VPU_HDMI_FMT_CTRL);
			viu_eotf_ctrl_backup = READ_VPP_REG(VIU_EOTF_CTL);
			xvycc_lut_ctrl_backup = READ_VPP_REG(XVYCC_LUT_CTL);
			vpp_matrix_backup = READ_VPP_REG(VPP_MATRIX_CTRL);
			vpp_vadj_backup = READ_VPP_REG(VPP_VADJ_CTRL);
			vpp_dummy1_backup = READ_VPP_REG(VPP_DUMMY_DATA1);
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
			VSYNC_WR_MPEG_REG(VPP_MATRIX_CTRL, 0);
			VSYNC_WR_MPEG_REG(VPP_VADJ_CTRL, 0);
			VSYNC_WR_MPEG_REG(VPP_DUMMY_DATA1, 0x20000000);
			VSYNC_WR_MPEG_REG_BITS(VIU_OSD1_MATRIX_CTRL, 0, 0, 1);
			dolby_vision_on = 1;
			pr_dolby_dbg("Dolby Vision turn on\n");
		}
	} else {
		if (dolby_vision_on) {
			VSYNC_WR_MPEG_REG(VPP_DOLBY_CTRL,  dolby_ctrl_backup);
			VSYNC_WR_MPEG_REG(VIU_MISC_CTRL1, viu_misc_ctrl_backup);
			VSYNC_WR_MPEG_REG(VPU_HDMI_FMT_CTRL,
				vpu_hdmi_fmt_backup);
			VSYNC_WR_MPEG_REG(VIU_EOTF_CTL, viu_eotf_ctrl_backup);
			VSYNC_WR_MPEG_REG(XVYCC_LUT_CTL, xvycc_lut_ctrl_backup);
			VSYNC_WR_MPEG_REG(VPP_MATRIX_CTRL, vpp_matrix_backup);
			VSYNC_WR_MPEG_REG(VPP_VADJ_CTRL, vpp_vadj_backup);
			VSYNC_WR_MPEG_REG(VPP_DUMMY_DATA1, vpp_dummy1_backup);
			VSYNC_WR_MPEG_REG_BITS(VIU_OSD1_MATRIX_CTRL, 1, 0, 1);
			dolby_vision_on = 0;
			dolby_vision_status = BYPASS_PROCESS;
			pr_dolby_dbg("Dolby Vision turn off\n");
		}
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

static struct vframe_s *dv_vf[16][2];
static void *metadata_parser;
static char meta_buf[1024];
static char md_buf[1024];
static char comp_buf[8196];
static int dvel_receiver_event_fun(int type, void *data, void *arg)
{
	char *provider_name = (char *)data;
	int i;

	if (type == VFRAME_EVENT_PROVIDER_UNREG) {
		pr_info("%s, provider %s unregistered\n",
			__func__, provider_name);
		for (i = 0; i < 16; i++)
			dv_vf[i][0] = dv_vf[i][1] = NULL;
		if (metadata_parser && p_funcs) {
			p_funcs->metadata_parser_release();
			metadata_parser = NULL;
		}
		frame_count = 0;
		return -1;
	} else if (type == VFRAME_EVENT_PROVIDER_QUREY_STATE) {
		return RECEIVER_ACTIVE;
	} else if (type == VFRAME_EVENT_PROVIDER_REG) {
		pr_info("%s, provider %s registered\n",
			__func__, provider_name);
		for (i = 0; i < 16; i++)
			dv_vf[i][0] = dv_vf[i][1] = NULL;
		frame_count = 0;
	}
	return 0;
}

static inline void dvel_vf_put(struct vframe_s *vf)
{
	struct vframe_provider_s *vfp = vf_get_provider(DVEL_RECV_NAME);
	if (vfp)
		vf_put(vf, DVEL_RECV_NAME);
}

static inline struct vframe_s *dvel_vf_peek(void)
{
	return vf_peek(DVEL_RECV_NAME);
}

static inline struct vframe_s *dvel_vf_get(void)
{
	return vf_get(DVEL_RECV_NAME);
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

void dump_setting(struct dovi_setting_s *setting)
{
	int i;
	uint32_t *p;

	if ((debug_dolby & 0x10) && dump_enable) {
		pr_info("core1\n");
		p = (uint32_t *)&setting->dm_reg1;
		for (i = 0; i < 26; i++)
			pr_info("%x\n", p[i]);

		pr_info("composer\n");
		p = (uint32_t *)&setting->comp_reg;
		for (i = 0; i < 173; i++)
			pr_info("%x\n", p[i]);
	}

	if ((debug_dolby & 0x20) && dump_enable) {
		pr_info("core1lut\n");
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

	if ((debug_dolby & 0x10) && dump_enable) {
		pr_info("core2\n");
		p = (uint32_t *)&setting->dm_reg2;
		for (i = 0; i < 24; i++)
			pr_info("%x\n", p[i]);
	}

	if ((debug_dolby & 0x20) && dump_enable) {
		pr_info("core2lut\n");
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

	if ((debug_dolby & 0x10) && dump_enable) {
		pr_info("core3\n");
		p = (uint32_t *)&setting->dm_reg3;
		for (i = 0; i < 26; i++)
			pr_info("%x\n", p[i]);
	}

	if ((debug_dolby & 0x40) && dump_enable) {
		pr_info("core3_meta %d\n", setting->md_reg3.size);
		p = setting->md_reg3.raw_metadata;
		for (i = 0; i < setting->md_reg3.size; i++)
			pr_info("%x\n", p[i]);
	}
}

int dolby_vision_wait_metadata(struct vframe_s *vf)
{
	struct provider_aux_req_s req;
	struct vframe_s *el_vf;

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
			if (el_vf->pts_us64 == vf->pts_us64) {
				/* found el */
				return 0;
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
				/* skip old el */
				el_vf = dvel_vf_peek();
			} else {
				/* no el found */
				return 1;
			}
		}
		/* need wait el */
		if (el_vf == NULL)
			return 2;
	}
	return 0;
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
	int ret = -1;

	if ((req->aux_buf == NULL)
	|| (req->aux_size == 0))
		return 0;

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
			pr_dolby_dbg("meta(%d), pts(%lld)\n",
				size, vf->pts_us64);
			meta_buf[0] = meta_buf[1] = meta_buf[2] = 0;
			memcpy(&meta_buf[3], p+1, size-1);
			if ((debug_dolby & 4) && dump_enable) {
				for (i = 0; i < size+2; i++) {
					pr_info("%02x ",
						meta_buf[i]);
					if (i%16 == 15)
						pr_info("\n");
				}
				if (i%16 != 0)
					pr_info("\n");
			}

			/* prepare metadata parser */
			parser_ready = 0;
			if (metadata_parser == NULL) {
				metadata_parser =
					p_funcs->metadata_parser_init();
				if (metadata_parser != NULL)
					parser_ready = 1;
			} else {
				if (p_funcs->metadata_parser_reset() == 0)
					parser_ready = 1;
			}

			if (parser_ready) {
				md_size = comp_size = 0;
				ret = p_funcs->metadata_parser_process(
					meta_buf, size+2,
					comp_buf + *total_comp_size,
					&comp_size,
					md_buf + *total_md_size,
					&md_size,
					true);
				if (ret) {
					pr_dolby_error(
						"metadata parser process fail\n");
					p_funcs->metadata_parser_release();
					metadata_parser = NULL;
					return -1;
				} else {
					*total_comp_size += comp_size;
					*total_md_size += md_size;
				}
			} else {
				pr_dolby_error("metadata parser init fail\n");
				return -1;
			}
			if ((ret == 0)
			&& (debug_dolby & 8)
			&& dump_enable) {
				pr_dolby_dbg("parsed md(%d), comp(%d)\n",
					md_size, comp_size);
				for (i = 0; i < md_size; i++) {
					pr_info("%02x ",
						md_buf[*total_md_size+i]);
					if (i%16 == 15)
						pr_info("\n");
				}
				if (i%16 != 0)
					pr_info("\n");
			}
			if ((debug_dolby & 8)  && dump_enable) {
				for (i = 0; i < comp_size; i++) {
					pr_info("%02x ",
						comp_buf[*total_comp_size+i]);
					if (i%16 == 15)
						pr_info("\n");
				}
				if (i%16 != 0)
					pr_info("\n");
			}
		}
		p += size;
	}
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

	if ((!dolby_vision_enable) || (!p_funcs))
		return 1;

	if (vf) {
		pr_dolby_dbg("frame %d:\n", frame_count);

		/* check source format */
		if (signal_transfer_characteristic == 16)
			video_is_hdr10 = true;
		else
			video_is_hdr10 = false;
		if (video_is_hdr10)
			src_format = FORMAT_HDR10;

		req.vf = vf;
		req.bot_flag = 0;
		req.aux_buf = NULL;
		req.aux_size = 0;
		req.dv_enhance_exist = 0;
		vf_notify_provider_by_name("dvbldec",
			VFRAME_EVENT_RECEIVER_GET_AUX_DATA,
	    (void *)&req);
		/* parse meta in base layer */
		parse_sei_and_meta(vf, &req,
			&total_comp_size, &total_md_size, &src_format);

		if (req.dv_enhance_exist) {
			el_vf = dvel_vf_get();
			if (el_vf
			&& (el_vf->pts_us64 == vf->pts_us64)) {
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
				parse_sei_and_meta(el_vf, &el_req,
					&total_comp_size, &total_md_size,
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
	}

	/* if not DOVI, release metadata_parser */
	if ((src_format != FORMAT_DOVI) && metadata_parser) {
		p_funcs->metadata_parser_release();
		metadata_parser = NULL;
	}

	if (dolby_vision_mode == DOLBY_VISION_OUTPUT_MODE_BYPASS) {
		frame_count++;
		return 0;
	}

	/* check target luminance */
	if (dolby_vision_flags & FLAG_USE_SINK_MIN_MAX) {
		if (vinfo->dv_info != NULL) {
			if (vinfo->dv_info->ver == 0) {
				/* need lookup PQ table ... */
				/*
				dolby_vision_graphic_min =
				dolby_vision_target_min =
					vinfo->dv_info.ver0.target_min_pq;
				dolby_vision_graphic_max =
				dolby_vision_target_max =
					vinfo->dv_info.ver0.target_max_pq;
				*/
			} else if (vinfo->dv_info->ver == 1) {
				if (vinfo->dv_info->vers.ver1.target_max_lum) {
					/* Target max luminance = 100+50*CV */
					dolby_vision_graphic_max =
					dolby_vision_target_max =
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
		} else if (vinfo->hdr_info.hdr_support) {
			if (vinfo->hdr_info.lumi_max) {
				/* Luminance value = 50 * (2 ^ (CV/32)) */
				dolby_vision_graphic_max =
				dolby_vision_target_max = 50 *
					(2 ^ (vinfo->hdr_info.lumi_max >> 5));
				/* Desired Content Min Luminance =
					Desired Content Max Luminance
					* (CV/255) * (CV/255) / 100	*/
				dolby_vision_graphic_min =
				dolby_vision_target_min =
					dolby_vision_target_max * 10000
					* vinfo->hdr_info.lumi_min
					* vinfo->hdr_info.lumi_min
					/ (255 * 255 * 100);
			}
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

	flag = p_funcs->control_path(
		src_format, dst_format,
		comp_buf, total_comp_size,
		md_buf, total_md_size,
		VIDEO_PRIORITY,
		12, 0, SIG_RANGE_SMPTE, /* bit/chroma/range */
		dolby_vision_graphic_min, dolby_vision_graphic_max * 10000,
		dolby_vision_target_min, dolby_vision_target_max * 10000,
		(!el_flag) ||
		(dolby_vision_flags & FLAG_DISABLE_COMPOSER), /* disable el */
		&hdr10_param,
		&dovi_setting);
	if (flag >= 0) {
		dovi_setting.src_format = src_format;
		dovi_setting.el_flag = el_flag;
		dovi_setting.el_halfsize_flag = el_halfsize_flag;
		if (el_flag)
			pr_dolby_dbg("setting %d->%d(T:%d-%d): flag=%02x,md=%d,comp=%d\n",
				src_format, dst_format,
				dolby_vision_target_min,
				dolby_vision_target_max,
				total_md_size, total_comp_size, flag);
		else
			pr_dolby_dbg("setting %d->%d(T:%d-%d): flag=%02x,md=%d\n",
				src_format, dst_format,
				dolby_vision_target_min,
				dolby_vision_target_max,
				total_md_size, flag);
		dump_setting(&dovi_setting);
		frame_count++;
		return 0; /* setting updated */
	} else {
		pr_dolby_error("control_path() failed\n");
	}
	frame_count++;
	return 1; /* do nothing for this frame */
}

int dolby_vision_update_metadata(struct vframe_s *vf)
{
	int ret = -1;

	if (!dolby_vision_enable)
		return -1;

	if (vf && dolby_vision_vf_check(vf))
		ret = dolby_vision_parse_metadata(vf);

	return ret;
}
EXPORT_SYMBOL(dolby_vision_update_metadata);

int dolby_vision_process(struct vframe_s *vf)
{
	const struct vinfo_s *vinfo = get_current_vinfo();

	if (get_cpu_type() != MESON_CPU_MAJOR_ID_GXM)
		return -1;

	if ((!dolby_vision_enable) || (!p_funcs))
		return -1;

	if (dolby_vision_mode == DOLBY_VISION_OUTPUT_MODE_BYPASS) {
		if (dolby_vision_status != BYPASS_PROCESS)
			enable_dolby_vision(0);
		return 0;
	}

	if (vf) {
		if ((dolby_vision_flags & FLAG_TOGGLE_FRAME) == 0)
			return -1;
		dolby_core1_set(
			24,	173, 256 * 5,
			(uint32_t *)&dovi_setting.dm_reg1,
			(uint32_t *)&dovi_setting.comp_reg,
			(uint32_t *)&dovi_setting.dm_lut1,
			(vf->type & VIDTYPE_COMPRESS) ?
				vf->compWidth : vf->width,
			(vf->type & VIDTYPE_COMPRESS) ?
				vf->compHeight : vf->height,
			1, /* BL enable */
			dovi_setting.el_flag, /* EL enable */
			dovi_setting.el_halfsize_flag, /* if BL and EL is 4:1 */
			dolby_vision_mode ==
			DOLBY_VISION_OUTPUT_MODE_IPT_TUNNEL, 1);
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
			pr_dolby_dbg("Dolby Vision mode changed to DV_PROCESS\n");
			dolby_vision_status = DV_PROCESS;
		} else if ((dovi_setting.src_format == FORMAT_HDR10)
		&& (dolby_vision_status != HDR_PROCESS)) {
			pr_dolby_dbg("Dolby Vision mode changed to HDR_PROCESS\n");
			dolby_vision_status = HDR_PROCESS;
		} else if ((dovi_setting.src_format == FORMAT_SDR)
		&& (dolby_vision_status != SDR_PROCESS)) {
			pr_dolby_dbg("Dolby Vision mode changed to SDR_PROCESS\n");
			dolby_vision_status = SDR_PROCESS;
		}
		dolby_vision_flags &= ~FLAG_TOGGLE_FRAME;
		return 0;
	}

	if ((dolby_vision_flags & FLAG_TOGGLE_FRAME) == 0)
		return -1;

	dolby_vision_parse_metadata(NULL);
	dolby_core1_set(
		24,	173, 256 * 4,
		(uint32_t *)&dovi_setting.dm_reg1,
		(uint32_t *)&dovi_setting.comp_reg,
		(uint32_t *)&dovi_setting.dm_lut1,
		vinfo->width, vinfo->height,
		1, /* BL enable */
		0, /* EL enable */
		1, /* if BL and EL is 4:1 */
		dolby_vision_mode ==
		DOLBY_VISION_OUTPUT_MODE_IPT_TUNNEL, 1);
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
		pr_dolby_dbg("Dolby Vision mode changed to DV_PROCESS\n");
		dolby_vision_status = DV_PROCESS;
	} else if ((dovi_setting.src_format == FORMAT_HDR10)
	&& (dolby_vision_status != HDR_PROCESS)) {
		pr_dolby_dbg("Dolby Vision mode changed to HDR_PROCESS\n");
		dolby_vision_status = HDR_PROCESS;
	} else if ((dovi_setting.src_format == FORMAT_SDR)
	&& (dolby_vision_status != SDR_PROCESS)) {
		pr_dolby_dbg("Dolby Vision mode changed to SDR_PROCESS\n");
		dolby_vision_status = SDR_PROCESS;
	}
	dolby_vision_flags &= ~FLAG_TOGGLE_FRAME;
	return 0;
}
EXPORT_SYMBOL(dolby_vision_process);

bool is_dolby_vision_on(void)
{
	return dolby_vision_on;
}
EXPORT_SYMBOL(is_dolby_vision_on);

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
		dolby_vision_mode = mode;
		video_is_hdr10 = false;
		dolby_vision_set_toggle_flag(1);
		dolby_vision_process(NULL);
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
