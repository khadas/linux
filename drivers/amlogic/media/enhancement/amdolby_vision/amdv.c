// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/compat.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/errno.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/video_sink/video.h>
#include <linux/amlogic/media/video_sink/video_signal_notify.h>
#include <linux/amlogic/media/registers/register_map.h>
//#include <linux/amlogic/cpu_version.h>
/* media module used media/registers/cpu_version.h since kernel 5.4 */
#include <linux/amlogic/media/registers/cpu_version.h>
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_VECM
#include <linux/amlogic/media/amvecm/amvecm.h>
#endif
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/vfm/vframe_receiver.h>
#include <linux/amlogic/media/utils/amstream.h>
#include "../amvecm/arch/vpp_regs.h"
#include "../amvecm/arch/vpp_dolbyvision_regs.h"
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_VECM
#include "../amvecm/arch/vpp_hdr_regs.h"
#include "../amvecm/amcsc.h"
#include "../amvecm/reg_helper.h"
#endif
#include <linux/amlogic/media/registers/regs/viu_regs.h>
#include <linux/amlogic/media/amdolbyvision/dolby_vision.h>
#include <linux/cma.h>
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include <linux/amlogic/media/vpu/vpu.h>
#include <linux/dma-contiguous.h>
#include <linux/amlogic/iomap.h>
#include <linux/poll.h>
#include <linux/workqueue.h>
#include "amdv.h"
#include "amdv_regs_s5.h"
#include "md_config.h"
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/of.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/ctype.h>/* for parse_para_pq */
#include <linux/string.h>
#include <linux/vmalloc.h>
#include <linux/arm-smccc.h>
#include <linux/spinlock.h>
#include <linux/amlogic/media/vout/lcd/lcd_notify.h>
#include <linux/amlogic/gki_module.h>
#include <linux/amlogic/media/vout/hdmi_tx_ext.h>
#include <linux/amlogic/media/utils/am_com.h>
#include "../../common/vfm/vfm.h"
#include <linux/amlogic/media/utils/vdec_reg.h>

DEFINE_SPINLOCK(amdv_lock);

static unsigned int dolby_vision_probe_ok;
const struct dolby_vision_func_s *p_funcs_stb;
const struct dolby_vision_func_s *p_funcs_tv;

#define AMDOLBY_VISION_NAME               "amdolby_vision"
#define AMDOLBY_VISION_CLASS_NAME         "amdolby_vision"

struct amdolby_vision_dev_s {
	dev_t                       devt;
	struct cdev                 cdev;
	dev_t                       devno;
	struct device               *dev;
	struct class                *clsp;
	wait_queue_head_t	dv_queue;
};

static struct amdolby_vision_dev_s amdolby_vision_dev;
struct dv_device_data_s dv_meson_dev;
static unsigned int dolby_vision_request_mode = 0xff;

unsigned int dolby_vision_mode = AMDV_OUTPUT_MODE_BYPASS;
module_param(dolby_vision_mode, uint, 0664);
MODULE_PARM_DESC(dolby_vision_mode, "\n dolby_vision_mode\n");

unsigned int amdv_target_mode = AMDV_OUTPUT_MODE_BYPASS;
module_param(amdv_target_mode, uint, 0444);
MODULE_PARM_DESC(amdv_target_mode, "\n amdv_target_mode\n");

static unsigned int dolby_vision_profile = 0xff;
module_param(dolby_vision_profile, uint, 0664);
MODULE_PARM_DESC(dolby_vision_profile, "\n dolby_vision_profile\n");

static unsigned int dolby_vision_level = 0xff;
module_param(dolby_vision_level, uint, 0664);
MODULE_PARM_DESC(dolby_vision_level, "\n dolby_vision_level\n");

static unsigned int primary_debug;
module_param(primary_debug, uint, 0664);
MODULE_PARM_DESC(primary_debug, "\n primary_debug\n");
/* STB: if sink support DV, always output DV*/
/*		else always output SDR/HDR */
/* TV:  when source is DV/HDR10/HLG, convert to SDR */
/* #define AMDV_FOLLOW_SINK		0 */

/* STB: output DV only if source is DV*/
/*		and sink support DV*/
/*		else always output SDR/HDR */
/* TV:  when source is DV, convert to SDR */
/* #define AMDV_FOLLOW_SOURCE		1 */

/* STB: always follow dolby_vision_mode */
/* TV:  if set dolby_vision_mode to SDR8,*/
/*		convert all format to SDR by TV core,*/
/*		else bypass Dolby Vision */
/* #define AMDV_FORCE_OUTPUT_MODE	2 */

static unsigned int dolby_vision_policy;
module_param(dolby_vision_policy, uint, 0664);
MODULE_PARM_DESC(dolby_vision_policy, "\n dolby_vision_policy\n");
static unsigned int last_dolby_vision_policy;

/* === HDR10 === */
/* bit0: follow sink 0: bypass hdr10 to vpp 1: process hdr10 by dolby core */
/* bit1: follow source 0: bypass hdr10 to vpp 1: process hdr10 by dolby core */
/* === HDR10+ === */
/* bit2: 0: bypass hdr10+ to vpp, 1: process hdr10+ as hdr10 by dolby core */
/* === HLG ===== */
/* bit3: follow sink 0: bypass hlg to vpp, 1: process hlg by dolby core */
/* bit4: follow source 0: bypass hlg to vpp, 1: process hlg by dolby core */
/* === SDR === */
/* bit5: follow sink 0: bypass SDR to vpp, 1: process SDR by dolby core */
/* bit6: follow source 0: bypass SDR to vpp, 1: process SDR by dolby core */
#define HDR_BY_DV_F_SINK 0x1
#define HDR_BY_DV_F_SRC 0x2
#define HDRP_BY_DV 0x4
#define HLG_BY_DV_F_SINK 0x8
#define HLG_BY_DV_F_SRC 0x10
#define SDR_BY_DV_F_SINK 0x20
#define SDR_BY_DV_F_SRC 0x40

static unsigned int dolby_vision_hdr10_policy;
module_param(dolby_vision_hdr10_policy, uint, 0664);
MODULE_PARM_DESC(dolby_vision_hdr10_policy, "\n dolby_vision_hdr10_policy\n");
static unsigned int last_dolby_vision_hdr10_policy;

static bool enable_amvs12_for_tv;

/* enable hdmi dv std to stb core */
static uint hdmi_to_stb_policy = 1;
module_param(hdmi_to_stb_policy, uint, 0664);
MODULE_PARM_DESC(hdmi_to_stb_policy, "\n hdmi_to_stb_policy\n");

static bool hdmi_source_led_as_hdr10 = true;

static bool dolby_vision_enable;
module_param(dolby_vision_enable, bool, 0664);
MODULE_PARM_DESC(dolby_vision_enable, "\n dolby_vision_enable\n");

static bool amdv_efuse_bypass;
module_param(amdv_efuse_bypass, bool, 0664);
MODULE_PARM_DESC(amdv_efuse_bypass, "\n amdv_efuse_bypass\n");
static bool efuse_mode;

uint amdv_mask = 7;
module_param(amdv_mask, uint, 0664);
MODULE_PARM_DESC(amdv_mask, "\n amdv_mask\n");

uint dolby_vision_status;
module_param(dolby_vision_status, uint, 0664);
MODULE_PARM_DESC(dolby_vision_status, "\n dolby_vision_status\n");

/* delay before first frame toggle when core off->on */
static uint amdv_wait_delay = 2;
module_param(amdv_wait_delay, uint, 0664);
MODULE_PARM_DESC(amdv_wait_delay, "\n amdv_wait_delay\n");

/* reset 1st fake frame (bit 0)*/
/*   and other fake frames (bit 1)*/
/*   and other toggle frames (bit 2) */
static uint amdv_reset = (1 << 1) | (1 << 0);
module_param(amdv_reset, uint, 0664);
MODULE_PARM_DESC(amdv_reset, "\n amdv_reset\n");

/* force run mode */
uint amdv_run_mode = 0xff; /* not force */
module_param(amdv_run_mode, uint, 0664);
MODULE_PARM_DESC(amdv_run_mode, "\n amdv_run_mode\n");

/* number of fake frame (run mode = 1) */
#define RUN_MODE_DELAY 2
#define RUN_MODE_DELAY_GXM 3
#define RUN_MODE_DELAY_G12 1

uint amdv_run_mode_delay = RUN_MODE_DELAY;
module_param(amdv_run_mode_delay, uint, 0664);
MODULE_PARM_DESC(amdv_run_mode_delay, "\n amdv_run_mode_delay\n");

/* reset control -- end << 8 | start */
static uint amdv_reset_delay =
	(RUN_MODE_DELAY << 8) | RUN_MODE_DELAY;
module_param(amdv_reset_delay, uint, 0664);
MODULE_PARM_DESC(amdv_reset_delay, "\n amdv_reset_delay\n");

static unsigned int amdv_tuning_mode;
module_param(amdv_tuning_mode, uint, 0664);
MODULE_PARM_DESC(amdv_tuning_mode, "\n amdv_tuning_mode\n");

u32 dolby_vision_ll_policy = DOLBY_VISION_LL_DISABLE;
module_param(dolby_vision_ll_policy, uint, 0664);
MODULE_PARM_DESC(dolby_vision_ll_policy, "\n dolby_vision_ll_policy\n");

u32 last_dolby_vision_ll_policy = DOLBY_VISION_LL_DISABLE;

#ifdef V2_4_3
#define DOLBY_VISION_STD_RGB_TUNNEL	0
#define DOLBY_VISION_STD_YUV422		1
static u32 dolby_vision_std_policy = DOLBY_VISION_STD_RGB_TUNNEL;
module_param(dolby_vision_std_policy, uint, 0664);
MODULE_PARM_DESC(dolby_vision_std_policy, "\n dolby_vision_ll_policy\n");
/*static u32 last_dolby_vision_std_policy = DOLBY_VISION_STD_RGB_TUNNEL;*/
static bool force_support_emp;
#endif

static unsigned int amdv_src_format;
static enum signal_format_enum graphic_fmt = FORMAT_SDR;
static enum signal_format_enum g_dst_format;

static unsigned int force_mel;
module_param(force_mel, uint, 0664);
MODULE_PARM_DESC(force_mel, "\n force_mel\n");

static unsigned int force_best_pq;
module_param(force_best_pq, uint, 0664);
MODULE_PARM_DESC(force_best_pq, "\n force_best_pq\n");

/*bit0: 0-> efuse, 1->no efuse; */
/*bit1: 1->ko loaded*/
/*bit2: 1-> value updated*/
static int support_info;
int get_dv_support_info(void)
{
	return support_info;
}
EXPORT_SYMBOL(get_dv_support_info);

char cur_crc[32] = "invalid";
#define FLAG_FRAME_DELAY_MASK	0xf
#define FLAG_FRAME_DELAY_SHIFT	16

unsigned int dolby_vision_flags = FLAG_BYPASS_VPP | FLAG_FORCE_CVM;
module_param(dolby_vision_flags, uint, 0664);
MODULE_PARM_DESC(dolby_vision_flags, "\n dolby_vision_flags\n");

#define DV_NAME_LEN_MAX 32

#define TV_DMA_TBL_SIZE 3754
/* 256+(256*4+256*2)*4/8 64bit */
#define STB_DMA_TBL_SIZE (256 + (256 * 4 + 256 * 2) * 4 / 8)

static u64 stb_core1_lut[STB_DMA_TBL_SIZE];

/*dma size:1877x2x64 bit = 30032 byte*/
static unsigned int dma_size = 30032;
static dma_addr_t dma_paddr;
static void *dma_vaddr;

#define CRC_BUFF_SIZE (256 * 1024)
static char *crc_output_buf;
static u32 crc_output_buff_off;
static u32 crc_count;
static u32 crc_bypass_count;
static u32 setting_update_count;/*for cert crc*/
static s32 crc_read_delay;

u32 core1_disp_hsize;
u32 core1_disp_vsize;
static u32 vsync_count;

#define FLAG_VSYNC_CNT 10
#define MAX_TRANSITION_DELAY 5
#define MIN_TRANSITION_DELAY 2

static u32 vpp_data_422T0444_backup;

static bool is_osd_off;
static bool osd_onoff_changed;
static int core1_switch;
static int core3_switch;
bool force_set_lut;
static char *ko_info;
static char total_chip_name[20];
static char chip_name[5] = "null";
/*bit0: copy core1a to core1b;  bit1: copy core1a to core1c*/
int copy_core1a;
/*bit0: core2a, bit1: core2c*/
int core2_sel = 1;
static unsigned int amdv_target_min = 50; /* 0.0001 */
static unsigned int amdv_target_max[3][3] = {
	{ 4000, 1000, 100 }, /* DV => DV/HDR/SDR */
	{ 1000, 1000, 100 }, /* HDR =>  DV/HDR/SDR */
	{ 600, 1000, 100 },  /* SDR =>  DV/HDR/SDR */
};

static unsigned int amdv_default_max[3][3] = {
	{ 4000, 4000, 100 }, /* DV => DV/HDR/SDR */
	{ 1000, 1000, 100 }, /* HDR =>  DV/HDR/SDR */
	{ 600, 1000, 100 },  /* SDR =>  DV/HDR/SDR */
};

static unsigned int amdv_graphic_min = 50; /* 0.0001 */
static unsigned int amdv_graphic_max; /* 100 */
static unsigned int old_amdv_graphic_max;
module_param(amdv_graphic_min, uint, 0664);
MODULE_PARM_DESC(amdv_graphic_min, "\n amdv_graphic_min\n");
module_param(amdv_graphic_max, uint, 0664);
MODULE_PARM_DESC(amdv_graphic_max, "\n amdv_graphic_max\n");

static unsigned int dv_HDR10_graphics_max = 300;
static unsigned int dv_graphic_blend_test;
module_param(dv_graphic_blend_test, uint, 0664);
MODULE_PARM_DESC(dv_graphic_blend_test, "\n dv_graphic_blend_test\n");

static unsigned int dv_target_graphics_max[3][3] = {
	{ 300, 375, 380 }, /* DV => DV/HDR/SDR */
	{ 300, 375, 100 }, /* HDR =>  DV/HDR/SDR */
	{ 300, 375, 100 }, /* SDR =>  DV/HDR/SDR */
};

static unsigned int dv_target_graphics_LL_max[3][3] = {
	{ 300, 375, 100 }, /* DV => DV/HDR/SDR */
	{ 210, 375, 100 }, /* HDR =>  DV/HDR/SDR */
	{ 300, 375, 100 }, /* SDR =>  DV/HDR/SDR */
};

static unsigned int dv_target_graphics_max_26[5][3] = {
	{ 300, 300, 370 }, /* DV => DV/HDR/SDR */
	{ 150, 304, 300 }, /* HDR =>  DV/HDR/SDR */
	{ 300, 305, 300 }, /* SDR =>  DV/HDR/SDR */
	{ 300, 300, 300 }, /* reserved DV LL =>  DV/HDR/SDR */
	{ 300, 300, 300 }, /* HLG =>  DV/HDR/SDR */
};

static unsigned int dv_target_graphics_LL_max_26[5][3] = {
	{ 300, 300, 370 }, /* DV => DV/HDR/SDR */
	{ 210, 300, 300 }, /* HDR =>  DV/HDR/SDR */
	{ 300, 300, 300 }, /* SDR =>  DV/HDR/SDR */
	{ 300, 300, 300 }, /* reserved DV LL =>  DV/HDR/SDR */
	{ 300, 300, 300 }, /* HLG =>  DV/HDR/SDR */
};

/*these two parameters form OSD*/
static unsigned int osd_graphic_width[OSD_MAX_INDEX] = {1920};
static unsigned int osd_graphic_height[OSD_MAX_INDEX] = {1080};
static unsigned int new_osd_graphic_width[OSD_MAX_INDEX] = {1920};
static unsigned int new_osd_graphic_height[OSD_MAX_INDEX] = {1080};

static unsigned int enable_tunnel;
static u32 vpp_data_422T0444_backup;
/* 0: auto priority 1: graphic priority 2: video priority */
unsigned int dolby_vision_graphics_priority;
module_param(dolby_vision_graphics_priority, uint, 0664);
MODULE_PARM_DESC(dolby_vision_graphics_priority, "\n dolby_vision_graphics_priority\n");

/*1: graphic; 2: video*/
int force_priority;

/*1:HDR10, 2:HLG, 3: DV LL*/
static int force_hdmin_fmt;

/*1: force bypass 1d93 bit0=1; 2: force not bypass 1d93 bit0=0;*/
static int force_bypass_pps_sr_cm;

/*0: input video1; 1: input video2;*/
static int pri_input;

static int force_pri_input = -1;

int cur_valid_video_num;

static unsigned int atsc_sei = 1;
module_param(atsc_sei, uint, 0664);
MODULE_PARM_DESC(atsc_sei, "\n atsc_sei\n");

static struct dv_atsc p_atsc_md;
static bool need_update_cfg;
static unsigned int sdr_ref_mode;
module_param(sdr_ref_mode, uint, 0664);
MODULE_PARM_DESC(sdr_ref_mode, "\n sdr_ref_mode\n");

static bool ambient_update;
static struct ambient_cfg_s ambient_config_new;

static unsigned int ambient_test_mode;
module_param(ambient_test_mode, uint, 0664);
MODULE_PARM_DESC(ambient_test_mode, "\n ambient_test_mode\n");

struct ambient_cfg_s ambient_darkdetail = {16, 0, 0, 0, 0, 0, 1};

static int content_fps = 24;
static int gd_rf_adjust;
static int enable_vf_check;
static u32 last_vf_valid_crc;
unsigned int debug_dolby;
module_param(debug_dolby, uint, 0664);
MODULE_PARM_DESC(debug_dolby, "\n debug_dolby\n");

static unsigned int debug_dolby_frame = 0xffff;
module_param(debug_dolby_frame, uint, 0664);
MODULE_PARM_DESC(debug_dolby_frame, "\n debug_dolby_frame\n");

unsigned int debug_ko;

#define dump_enable \
	((debug_dolby_frame >= 0xffff) || \
	(debug_dolby_frame + 1 == frame_count))

#define single_step_enable \
	(((debug_dolby_frame >= 0xffff) || \
	((debug_dolby_frame + 1) == frame_count)) && \
	(debug_dolby & 0x80))

u32 dv_cert_graphic_width = 1920;
module_param(dv_cert_graphic_width, uint, 0664);
MODULE_PARM_DESC(dv_cert_graphic_width, "\n dv_cert_graphic_width\n");

u32 dv_cert_graphic_height = 1080;
module_param(dv_cert_graphic_height, uint, 0664);
MODULE_PARM_DESC(dv_cert_graphic_height, "\n dv_cert_graphic_height\n");

bool dolby_vision_on;
bool amdv_core1_on;
u32 amdv_core1_on_cnt;
uint amdv_on_count; /*for run mode*/
u32 amdv_core2_on_cnt;

bool dv_core1_on[NUM_IPCORE1];
u32 dv_core1_on_cnt[NUM_IPCORE1];
u32 dv_run_mode_count[NUM_IPCORE1];

struct dv_core1_inst_s dv_core1[NUM_IPCORE1];

static bool amdv_el_disable;

bool amdv_wait_on;
module_param(amdv_wait_on, bool, 0664);
MODULE_PARM_DESC(amdv_wait_on, "\n amdv_wait_on\n");

static int amdv_uboot_on;
static bool amdv_wait_init;
static int amdv_wait_count;
static unsigned int frame_count;
static bool amdv_on_in_uboot;

struct hdr10_parameter hdr10_param;
static struct hdr10_parameter last_hdr10_param;/*only for tv*/
static struct master_display_info_s hdr10_data; /*for hdmi out*/

static char *md_buf[2];
static char *comp_buf[2];
static char *drop_md_buf[2];
static char *drop_comp_buf[2];
static char *graphic_md_buf;
static int graphic_md_size;

#define VSEM_IF_BUF_SIZE 4096
#define VSEM_PKT_SIZE 31

static char *vsem_if_buf;/* buffer for vsem or vsif */
static char *vsem_md_buf;
static int current_id = 1;
struct dovi_setting_s dovi_setting;
struct dovi_setting_s new_dovi_setting;
void *pq_config_fake;
struct tv_dovi_setting_s *tv_dovi_setting;

static bool pq_config_set_flag;
static int backup_comp_size;
static int backup_md_size;

static bool best_pq_config_set_flag;
struct ui_menu_params menu_param = {50, 50, 50};
static bool tv_dovi_setting_change_flag;
static bool dovi_setting_update_flag;
bool amdv_setting_video_flag;

static struct platform_device *amdv_pdev;
static bool vsvdb_config_set_flag;
static bool vfm_path_on;
static bool dv_unique_drm;

static bool tv_mode;
static bool mel_mode;
static bool osd_update;
static bool enable_fel;
static bool enable_mel;
static u32 tv_backlight;
static bool tv_backlight_changed;
static bool tv_backlight_force_update;
static int force_disable_dv_backlight;
static bool dv_control_backlight_status;
#ifdef CONFIG_AMLOGIC_LCD
static bool use_12b_bl = true;/*12bit backlight interface*/
#endif
static bool enable_vpu_probe;
int use_target_lum_from_cfg;

static int bl_delay_cnt;
static int set_backlight_delay_vsync = 1;
module_param(set_backlight_delay_vsync, int, 0664);
MODULE_PARM_DESC(set_backlight_delay_vsync,    "\n set_backlight_delay_vsync\n");

static int debug_cp_res;
s16 brightness_off[8][2] = {
	{0, 150}, /* DV OTT DM3, DV OTT DM4  */
	{0, 150}, /* DV Sink-led DM3, DV Sink-led DM4 */
	{0, 0},   /* DV Source-led DM3, DM4, actually no DM3 for source-led*/
	{0, 300}, /* HDR10 HDMI DM4, OTT HDR10 DM4, no DM3 for HDR10 */
	{0, 0},   /* HLG HDMI DM4, OTT HLG DM4, no DM3 for HDR10 */
	{0, 0},    /* reserved1 */
	{0, 0},    /* reserved2 */
	{0, 0},    /* reserved3 */
};

struct tv_input_info_s *tv_input_info;
static bool enable_vpu_probe;
static bool module_installed;
static bool recovery_mode;
static bool force_runmode;

static unsigned int hdmi_frame_count;/*changed when frame content update*/

static u32 mali_afbcd_top_ctrl;
static u32 mali_afbcd_top_ctrl_mask;
static u32 mali_afbcd1_top_ctrl;
static u32 mali_afbcd1_top_ctrl_mask;
static bool update_mali_top_ctrl;

bool multi_dv_mode;/*idk2.6 or not*/
bool enable_multi_core1;/*enable two core1 or not*/
static bool core1a_core1b_switch;
struct m_dovi_setting_s m_dovi_setting;
struct m_dovi_setting_s new_m_dovi_setting;
struct m_dovi_setting_s invalid_m_dovi_setting;
struct dv_inst_s dv_inst[NUM_INST];
static int last_unmap_id;
static int hdmi_inst_id;
enum priority_mode_enum pri_mode = V_PRIORITY;
int hdmi_path_id;
static int vd1_inst_id = -1;
static int vd2_inst_id = -1;

static u32 inst_debug[2];
static u32 inst_res_debug[4];/*force set inst0 w/h inst1 w/h*/

static int force_two_valid;
module_param(force_two_valid, int, 0664);
MODULE_PARM_DESC(force_two_valid,    "\n force_two_valid\n");

static int dv_core1_detunnel = 1;
static bool update_control_path_flag;

static bool hdmi_in_allm;
static bool local_allm;

static int dv_dual_layer;

#define MAX_PARAM   8
bool is_aml_gxm(void)
{
	if (dv_meson_dev.cpu_id == _CPU_MAJOR_ID_GXM)
		return true;
	else
		return false;
}

bool is_aml_g12(void)
{
	if (dv_meson_dev.cpu_id == _CPU_MAJOR_ID_G12)
		return true;
	else
		return false;
}

bool is_aml_sc2(void)
{
	if (dv_meson_dev.cpu_id == _CPU_MAJOR_ID_SC2)
		return true;
	else
		return false;
}

bool is_aml_s4d(void)
{
	if (dv_meson_dev.cpu_id == _CPU_MAJOR_ID_S4D)
		return true;
	else
		return false;
}

bool is_aml_s5(void)
{
	if (dv_meson_dev.cpu_id == _CPU_MAJOR_ID_S5)
		return true;
	else
		return false;
}

bool is_aml_box(void)
{
	if (is_aml_gxm() || is_aml_g12() || is_aml_sc2() || is_aml_s4d() || is_aml_s5())
		return true;
	else
		return false;
}

bool is_aml_txlx(void)
{
	if (dv_meson_dev.cpu_id == _CPU_MAJOR_ID_TXLX)
		return true;
	else
		return false;
}

bool is_aml_txlx_tvmode(void)
{
	if (is_aml_txlx() && tv_mode == 1)
		return true;
	else
		return false;
}

bool is_aml_txlx_stbmode(void)
{
	if (is_aml_txlx() && tv_mode == 0)
		return true;
	else
		return false;
}

bool is_aml_tm2(void)
{
	if (dv_meson_dev.cpu_id == _CPU_MAJOR_ID_TM2 ||
		dv_meson_dev.cpu_id == _CPU_MAJOR_ID_TM2_REVB)
		return true;
	else
		return false;
}

bool is_aml_tm2revb(void)
{
	if (dv_meson_dev.cpu_id == _CPU_MAJOR_ID_TM2_REVB)
		return true;
	else
		return false;
}

bool is_aml_tm2_tvmode(void)
{
	if (is_aml_tm2() && tv_mode == 1)
		return true;
	else
		return false;
}

bool is_aml_tm2_stbmode(void)
{
	if (is_aml_tm2() && tv_mode == 0)
		return true;
	else
		return false;
}

bool is_aml_t7(void)
{
	if (dv_meson_dev.cpu_id == _CPU_MAJOR_ID_T7)
		return true;
	else
		return false;
}

bool is_aml_t7_tvmode(void)
{
	if (is_aml_t7() && tv_mode == 1)
		return true;
	else
		return false;
}

bool is_aml_t7_stbmode(void)
{
	if (is_aml_t7() && tv_mode == 0)
		return true;
	else
		return false;
}

bool is_aml_t3(void)
{
	if (dv_meson_dev.cpu_id == _CPU_MAJOR_ID_T3)
		return true;
	else
		return false;
}

bool is_aml_t3_tvmode(void)
{
	if (is_aml_t3() && tv_mode == 1)
		return true;
	else
		return false;
}

bool is_aml_t5w(void)
{
	if (dv_meson_dev.cpu_id == _CPU_MAJOR_ID_T5W)
		return true;
	else
		return false;
}

bool is_amdv_stb_mode(void)
{
	return is_aml_txlx_stbmode() ||
		is_aml_tm2_stbmode() ||
		is_aml_t7_stbmode() ||
		is_aml_box();
}
EXPORT_SYMBOL(is_amdv_stb_mode);

bool is_aml_tvmode(void)
{
	if (is_aml_tm2_tvmode() ||
	    is_aml_txlx_tvmode() ||
	    is_aml_t7_tvmode() ||
	    is_aml_t3_tvmode() ||
	    is_aml_t5w())
		return true;
	else
		return false;
}

bool is_aml_stb_hdmimode(void)
{
	if ((is_aml_tm2_stbmode() || is_aml_t7_stbmode()) &&
	    module_installed && tv_dovi_setting &&
	    tv_dovi_setting->input_mode == IN_MODE_HDMI &&
	    hdmi_to_stb_policy)
		return true;
	else
		return false;
}

int is_graphics_output_off(void)
{
	if (is_aml_g12() || is_aml_tm2_stbmode() ||
	    is_aml_t7_stbmode() || is_aml_sc2() ||
	    is_aml_s4d())
		return !(READ_VPP_REG(OSD1_BLEND_SRC_CTRL) & (0xf << 8)) &&
		!(READ_VPP_REG(OSD2_BLEND_SRC_CTRL) & (0xf << 8));
	else if (is_aml_s5())
		return !(READ_VPP_REG(S5_OSD1_BLEND_SRC_CTRL) & 0xf) &&
		!(READ_VPP_REG(S5_OSD2_BLEND_SRC_CTRL) & 0xf);
	else
		return (!(READ_VPP_REG(VPP_MISC) & (1 << 12)));
}

bool core1_detunnel(void)
{
	if (is_aml_t7_stbmode() &&
		dv_core1_detunnel && multi_dv_mode)
		return 1;
	else if (is_aml_tm2_stbmode() && is_aml_tm2revb() &&
		dv_core1_detunnel)
		return 1;
	else
		return 0;
}

static u32 CORE1A_BASE;
static u32 CORE1B_BASE;
static u32 CORE1C_BASE;
static u32 CORE2A_BASE;
static u32 CORE2C_BASE;
static u32 CORE3_BASE;
static u32 CORETV_BASE;
static u32 CORE3_S1_BASE;
static u32 CORE3_S2_BASE;
static u32 CORE3_S3_BASE;

static void amdv_addr(void)
{
	if (is_aml_gxm() || is_aml_g12()) {
		CORE1A_BASE = 0x3300;
		CORE2A_BASE = 0x3400;
		CORE3_BASE = 0x3600;
	} else if (is_aml_txlx()) {
		CORE2A_BASE = 0x3400;
		CORE3_BASE = 0x3600;
		CORETV_BASE = 0x3300;
	} else if (is_aml_tm2()) {
		CORE1A_BASE = 0x3300;
		CORE1B_BASE = 0x4400;
		CORE2A_BASE = 0x3400;
		CORE3_BASE = 0x3600;
		CORETV_BASE = 0x4300;
	} else if (is_aml_sc2()) {
		CORE1A_BASE = 0x3300;
		CORE2A_BASE = 0x3400;
		CORE3_BASE = 0x3600;
	} else if (is_aml_t7()) {
		CORE1A_BASE = 0x3300;
		CORE1B_BASE = 0x4400;
		CORE1C_BASE = 0x6000;
		CORE2A_BASE = 0x3400;
		CORE2C_BASE = 0x6100;
		CORE3_BASE = 0x3600;
		CORETV_BASE = 0x4300;
	} else if (is_aml_t3()) {
		CORETV_BASE = 0x4300;
	} else if (is_aml_s4d()) {
		CORE1A_BASE = 0x3300;
		CORE2A_BASE = 0x3400;
		CORE3_BASE = 0x3600;
	} else if (is_aml_t5w()) {
		CORETV_BASE = 0x4300;
	} else if (is_aml_s5()) {
		CORE1A_BASE = 0x3300;
		CORE1B_BASE = 0x0a00;
		CORE2A_BASE = 0x0b00;
		CORE2C_BASE = 0x0d00;
		CORE3_BASE = 0x0e00; /*S0*/
		CORE3_S1_BASE = 0x0f00;
		CORE3_S2_BASE = 0x1200;
		CORE3_S3_BASE = 0x1300;
	}
}

static u32 addr_map(u32 adr)
{
	if (adr & CORE1A_OFFSET)
		adr = (adr & 0xffff) + CORE1A_BASE;
	else if (adr & CORE1B_OFFSET)
		adr = (adr & 0xffff) + CORE1B_BASE;
	else if (adr & CORE1C_OFFSET)
		adr = (adr & 0xffff) + CORE1C_BASE;
	else if (adr & CORE2A_OFFSET)
		adr = (adr & 0xffff) + CORE2A_BASE;
	else if (adr & CORE2C_OFFSET)
		adr = (adr & 0xffff) + CORE2C_BASE;
	else if (adr & CORE3_OFFSET)
		adr = (adr & 0xffff) + CORE3_BASE;
	else if (adr & CORETV_OFFSET)
		adr = (adr & 0xffff) + CORETV_BASE;
	else if (adr & CORE3_S1_OFFSET)
		adr = (adr & 0xffff) + CORE3_S1_BASE;
	else if (adr & CORE3_S2_OFFSET)
		adr = (adr & 0xffff) + CORE3_S2_BASE;
	else if (adr & CORE3_S3_OFFSET)
		adr = (adr & 0xffff) + CORE3_S3_BASE;

	return adr;
}

u32 VSYNC_RD_DV_REG(u32 adr)
{
	adr = addr_map(adr);
	return VSYNC_RD_MPEG_REG(adr);
}

int VSYNC_WR_DV_REG(u32 adr, u32 val)
{
	adr = addr_map(adr);
	VSYNC_WR_MPEG_REG(adr, val);
	return 0;
}

int VSYNC_WR_DV_REG_BITS(u32 adr, u32 val, u32 start, u32 len)
{
	adr = addr_map(adr);
	VSYNC_WR_MPEG_REG_BITS(adr, val, start, len);
	return 0;
}

u32 READ_VPP_DV_REG(u32 adr)
{
	adr = addr_map(adr);
	return READ_VPP_REG(adr);
}

int WRITE_VPP_DV_REG(u32 adr, const u32 val)
{
	adr = addr_map(adr);
	WRITE_VPP_REG(adr, val);
	return 0;
}

int WRITE_VPP_DV_REG_BITS(u32 adr, const u32 val, u32 start, u32 len)
{
	adr = addr_map(adr);
	WRITE_VCBUS_REG_BITS(adr, val, start, len);
	return 0;
}

void amdolby_vision_wakeup_queue(void)
{
	struct amdolby_vision_dev_s *devp = &amdolby_vision_dev;

	wake_up(&devp->dv_queue);
}

static unsigned int amdolby_vision_poll(struct file *file, poll_table *wait)
{
	struct amdolby_vision_dev_s *devp = file->private_data;
	unsigned int mask = 0;

	poll_wait(file, &devp->dv_queue, wait);
	mask = (POLLIN | POLLRDNORM);

	return mask;
}

bool dv_inst_valid(int id)
{
	if (id < 0 || id >= NUM_INST)
		return false;
	return true;
}

bool layerid_valid(int layerid)
{
	if (layerid < 0 || layerid >= NUM_IPCORE1)
		return false;
	return true;
}

static inline bool single_step_enable_v2(int inst_id, u8 layer_id)
{
	if (!dv_inst_valid(inst_id))
		inst_id = 0;
	if (layer_id == VD1_PATH) {
		return (((debug_dolby_frame >= 0xffff) ||
			((debug_dolby_frame + 1) == dv_inst[inst_id].frame_count)) &&
			(debug_dolby & 0x80));
	} else if (layer_id == VD2_PATH) {
		return (((debug_dolby_frame >= 0xffff) ||
			((debug_dolby_frame + 1) == dv_inst[inst_id].frame_count)) &&
			(debug_dolby & 0x800));
	} else {
		pr_dv_dbg("not support vd%d\n", layer_id);
		return false;
	}
}

static inline bool dump_enable_f(int inst_id)
{
	u32 count = frame_count;

	if (multi_dv_mode && dv_inst_valid(inst_id))
		count = dv_inst[inst_id].frame_count;

	return ((debug_dolby_frame >= 0xffff) ||
			(debug_dolby_frame == count) ||
			(debug_dolby_frame + 1 == count));
}

static void dump_tv_setting
	(struct tv_dovi_setting_s *setting,
	 int frame_cnt, int debug_flag)
{
	int i;
	u64 *p;

	if ((debug_flag & 0x10) && dump_enable) {
		if (is_aml_txlx_tvmode()) {
			pr_info("txlx tv core1 reg: 0x1a07 val = 0x%x\n",
				READ_VPP_DV_REG(VIU_MISC_CTRL1));
		} else if (is_aml_t7_tvmode()) {
			pr_info("t7_tv reg: SWAP_CTRL1 0x1a70 val = 0x%x\n",
				READ_VPP_DV_REG(AMDV_PATH_SWAP_CTRL1));
			pr_info("t7_tv reg: SWAP_CTRL2 0x1a71 val = 0x%x\n",
				READ_VPP_DV_REG(AMDV_PATH_SWAP_CTRL2));
			pr_info("t7_tv reg: tv core 0x1a83(bit4) val = 0x%x\n",
				READ_VPP_DV_REG(VPP_VD1_DSC_CTRL));
			pr_info("t7_tv reg: vd2 dv 0x1a84(bit4) val = 0x%x\n",
				READ_VPP_DV_REG(VPP_VD2_DSC_CTRL));
			pr_info("t7_tv reg: vd3 dv 0x1a85(bit4) val = 0x%x\n",
				READ_VPP_DV_REG(VPP_VD3_DSC_CTRL));
		} else if (is_aml_t3_tvmode() ||
			is_aml_t5w()) {
			pr_info("t3/5w_tv reg: TV select 0x2749(bit16) val = 0x%x\n",
				READ_VPP_DV_REG(VPP_TOP_VTRL));
			pr_info("t3/5w_tv reg: SWAP_CTRL1 0x1a70 val = 0x%x\n",
				READ_VPP_DV_REG(AMDV_PATH_SWAP_CTRL1));
			pr_info("t3/5w_tv reg: SWAP_CTRL2 0x1a71 val = 0x%x\n",
				READ_VPP_DV_REG(AMDV_PATH_SWAP_CTRL2));
			pr_info("t3/5w_tv reg: tv core 0x1a73(bit16) val = 0x%x\n",
				READ_VPP_DV_REG(VIU_VD1_PATH_CTRL));
		} else if (is_aml_tm2_tvmode()) {
			pr_info("tm2_tv reg: tv core 0x1a0c(bit1/0) val = 0x%x\n",
				READ_VPP_DV_REG(AMDV_PATH_CTRL));
		}

		pr_info("\nreg\n");
		p = (u64 *)&setting->core1_reg_lut[0];
		for (i = 0; i < 222; i += 2)
			pr_info("%016llx, %016llx,\n", p[i], p[i + 1]);
	}
	if ((debug_flag & 0x20) && dump_enable) {
		pr_info("\ng2l_lut\n");
		p = (u64 *)&setting->core1_reg_lut[0];
		for (i = 222; i < 222 + 256; i += 2)
			pr_info("%016llx %016llx\n", p[i], p[i + 1]);
	}
	if ((debug_flag & 0x40) && dump_enable) {
		pr_info("\n3d_lut\n");
		p = (u64 *)&setting->core1_reg_lut[0];
		for (i = 222 + 256; i < 222 + 256 + 3276; i += 2)
			pr_info("%016llx %016llx\n", p[i], p[i + 1]);
		pr_info("\n");
	}
}

void amdv_update_pq_config(char *pq_config_buf)
{
	memcpy((struct pq_config *)pq_config_fake,
		pq_config_buf, sizeof(struct pq_config));
	pr_info("update_pq_config[%zu] %x %x %x %x\n",
		sizeof(struct pq_config),
		pq_config_buf[1],
		pq_config_buf[2],
		pq_config_buf[3],
		pq_config_buf[4]);
	pq_config_set_flag = true;
}

void amdv_update_vsvdb_config(char *vsvdb_buf, u32 tbl_size)
{
	if (multi_dv_mode) {
		if (tbl_size > sizeof(new_m_dovi_setting.vsvdb_tbl)) {
			pr_info("update_vsvdb_config tbl size overflow %d\n", tbl_size);
			return;
		}
		memset(&new_m_dovi_setting.vsvdb_tbl[0],
			0, sizeof(new_m_dovi_setting.vsvdb_tbl));
		memcpy(&new_m_dovi_setting.vsvdb_tbl[0],
			vsvdb_buf, tbl_size);
		new_m_dovi_setting.vsvdb_len = tbl_size;
		new_m_dovi_setting.vsvdb_changed = 1;
	} else {
		if (tbl_size > sizeof(new_dovi_setting.vsvdb_tbl)) {
			pr_info("update_vsvdb_config tbl size overflow %d\n", tbl_size);
			return;
		}
		memset(&new_dovi_setting.vsvdb_tbl[0],
			0, sizeof(new_dovi_setting.vsvdb_tbl));
		memcpy(&new_dovi_setting.vsvdb_tbl[0],
			vsvdb_buf, tbl_size);
		new_dovi_setting.vsvdb_len = tbl_size;
		new_dovi_setting.vsvdb_changed = 1;
	}
	amdv_set_toggle_flag(1);
	if (tbl_size >= 8)
		pr_info("update_vsvdb_config[%d] %x %x %x %x %x %x %x %x\n",
			tbl_size, vsvdb_buf[0], vsvdb_buf[1], vsvdb_buf[2], vsvdb_buf[3],
			vsvdb_buf[4], vsvdb_buf[5], vsvdb_buf[6], vsvdb_buf[7]);
	vsvdb_config_set_flag = true;
}

int prepare_stb_dvcore1_reg(u32 run_mode,
					  u32 *p_core1_dm_regs, u32 *p_core1_comp_regs)
{
	int index = 0;
	int i;

	/* 4 */
	stb_core1_lut[index++] = ((u64)4 << 32) | 4;
	stb_core1_lut[index++] = ((u64)4 << 32) | 4;
	stb_core1_lut[index++] = ((u64)3 << 32) | 1;
	stb_core1_lut[index++] = ((u64)2 << 32) | 1;

	/* 1 + 14 + 10 + 1 */
	stb_core1_lut[index++] = ((u64)1 << 32) | run_mode;
	for (i = 0; i < 14; i++)
		stb_core1_lut[index++] =
			((u64)(6 + i) << 32)
			| p_core1_dm_regs[i];
	for (i = 17; i < 27; i++)
		stb_core1_lut[index++] =
			((u64)(6 + i) << 32)
			| p_core1_dm_regs[i - 3];
	stb_core1_lut[index++] = ((u64)(6 + 27) << 32) | 0;

	/* 173 + 1 */
	for (i = 0; i < 173; i++)
		stb_core1_lut[index++] =
			((u64)(6 + 44 + i) << 32)
			| p_core1_comp_regs[i];
	stb_core1_lut[index++] = ((u64)3 << 32) | 1;

	if (index & 1) {
		pr_dv_error("stb core1 reg tbl odd size\n");
		stb_core1_lut[index++] = ((u64)3 << 32) | 1;
	}
	return index;
}

void prepare_stb_dvcore1_lut(u32 base, u32 *p_core1_lut)
{
	u32 *p_lut;
	int i;

	p_lut = &p_core1_lut[256 * 4]; /* g2l */
	for (i = 0; i < 128; i++) {
		stb_core1_lut[base + i] =
		stb_core1_lut[base + i + 128] =
			((u64)p_lut[1] << 32) |
			((u64)p_lut[0] & 0xffffffff);
		p_lut += 2;
	}
	p_lut = &p_core1_lut[0]; /* 4 lut */
	for (i = 256; i < 768; i++) {
		stb_core1_lut[base + i] =
			((u64)p_lut[1] << 32) |
			((u64)p_lut[0] & 0xffffffff);
		p_lut += 2;
	}
}

static bool skip_cvm_tbl[2][2][4][4] = {
	{ /* core1: video */
		{ /* video priority */
			{1, 1, 0, 0}, /* dv in */
			{1, 1, 0, 0}, /* hdr in */
			{0, 0, 1, 0}, /* sdr in */
			{0, 0, 0, 0}  /* only hdmi in */
		},
		{ /* graphic priority */
			{0, 0, 0, 0}, /* dv in */
			{0, 0, 0, 0}, /* hdr in */
			{0, 0, 1, 0}, /* sdr in */
			{0, 0, 0, 0}  /* only hdmi in */
		}
	},
	{ /* core2: graphic */
		{ /* video priority */
			{0, 0, 0, 0}, /* dv in */
			{0, 0, 0, 0}, /* hdr in */
			{0, 0, 1, 0}, /* sdr in */
			{0, 0, 0, 0}  /* only hdmi in */
		},
		{ /* graphic priority */
			{0, 0, 0, 0}, /* dv in */
			{0, 0, 0, 0}, /* hdr in */
			{0, 0, 1, 0}, /* sdr in */
			{0, 0, 0, 0}  /* only hdmi in */
		}
	}
};

bool need_skip_cvm(unsigned int is_graphic)
{
	bool ret = false;

	if (dolby_vision_flags & FLAG_CERTIFICATION)
		return false;
	if (dolby_vision_flags & FLAG_FORCE_CVM)
		return false;

	if (multi_dv_mode) {
		ret = skip_cvm_tbl[is_graphic]
			[pri_mode]
			[new_m_dovi_setting.input[0].src_format == FORMAT_INVALID ?
				FORMAT_SDR : new_m_dovi_setting.input[0].src_format]
			[new_m_dovi_setting.dovi_ll_enable ?
				FORMAT_DOVI_LL : new_m_dovi_setting.dst_format];
	} else {
		ret = skip_cvm_tbl[is_graphic]
			[pri_mode]
			[new_dovi_setting.src_format == FORMAT_INVALID ?
				FORMAT_SDR : new_dovi_setting.src_format]
			[new_dovi_setting.dovi_ll_enable ?
				FORMAT_DOVI_LL : new_dovi_setting.dst_format];
	}
	return ret;
}

#ifdef V2_4_3
static unsigned char hdmi_metadata[211 * 8];
static unsigned int hdmi_metadata_size;
void convert_hdmi_metadata(uint32_t *md)
{
	int i = 0;
	u32 *p = md;
	int shift = 0;
	u32 size = md[0] >> 8;

	if (size > 211 * 8)
		size = 211 * 8;

	hdmi_metadata[0] = *p++ & 0xff;
	shift = 0;
	for (i = 1; i < size; i++) {
		hdmi_metadata[i] = (*p >> shift) & 0xff;
		shift += 8;
		if (shift == 32) {
			p++;
			shift = 0;
		}
	}
	hdmi_metadata_size = size;
}

bool need_send_emp_meta(const struct vinfo_s *vinfo)
{
	if (!vinfo || !vinfo->vout_device || !vinfo->vout_device->dv_info)
		return false;
	return is_aml_tm2_stbmode() &&
	(vinfo->vout_device->dv_info->dv_emp_cap || force_support_emp) &&
	((dolby_vision_mode == AMDV_OUTPUT_MODE_IPT_TUNNEL) ||
	dolby_vision_mode == AMDV_OUTPUT_MODE_IPT) &&
	(dolby_vision_ll_policy == DOLBY_VISION_LL_DISABLE) &&
	(dolby_vision_std_policy == DOLBY_VISION_STD_YUV422);
}
#endif

void update_graphic_width_height(unsigned int width,
	unsigned int height, enum OSD_INDEX index)
{
	if (index >= OSD_MAX_INDEX) {
		pr_info("error osd index\n");
	} else {
		new_osd_graphic_width[index] = width;
		new_osd_graphic_height[index] = height;
	}

	if (debug_dolby & 2)
		pr_dv_dbg("update osd%d %d_%d\n", index + 1, width, height);
}
EXPORT_SYMBOL(update_graphic_width_height);

void update_graphic_status(void)
{
	osd_update = true;
	pr_dv_dbg("osd update, need toggle\n");
}

static int is_graphic_changed(void)
{
	int ret = 0;
	int i = 0;

	if (is_graphics_output_off()) {
		if (!is_osd_off) {
			pr_dv_dbg("osd off\n");
			is_osd_off = true;
			osd_onoff_changed = true;
			ret |= 1;
		}
	} else if (is_osd_off) {
		/* force reset core2 when osd off->on */
		set_force_reset_core2(true);
		force_set_lut = true;
		pr_dv_dbg("osd on\n");
		is_osd_off = false;
		osd_onoff_changed = true;
		ret |= 2;
	}
	for (i = 0; i < OSD_MAX_INDEX; i++) {
		if (i == 0 || is_aml_s5()) {
			if (osd_graphic_width[i] != new_osd_graphic_width[i] ||
			    osd_graphic_height[i] != new_osd_graphic_height[i]) {
				set_force_reset_core2(true);
				if (debug_dolby & 0x2)
					pr_dv_dbg("osd changed %d %d-%d %d\n",
						     osd_graphic_width[i],
						     osd_graphic_height[i],
						     new_osd_graphic_width[i],
						     new_osd_graphic_height[i]);
				/* TODO: g12/tm2/sc2/t7 osd pps is after dolby core2, but */
				/* sometimes osd do crop,should monitor osd size change*/
				if (!is_osd_off /*&&!is_aml_tm2()&&!is_aml_sc2()&&!is_aml_t7()*/) {
					osd_graphic_width[i] = new_osd_graphic_width[i];
					osd_graphic_height[i] = new_osd_graphic_height[i];
					ret |= 2;
					force_set_lut = true;
				}
			}
		}
	}
	if (old_amdv_graphic_max !=
	    amdv_graphic_max) {
		if (debug_dolby & 0x2)
			pr_dv_dbg("graphic max changed %d-%d\n",
				     old_amdv_graphic_max,
				     amdv_graphic_max);
		if (!is_osd_off) {
			old_amdv_graphic_max =
				amdv_graphic_max;
			ret |= 2;
			force_set_lut = true;
		}
	}
	return ret;
}

static bool is_hdr10_src_primary_changed(void)
{
	if (hdr10_param.r_x != last_hdr10_param.r_x ||
	    hdr10_param.r_y != last_hdr10_param.r_y ||
	    hdr10_param.g_x != last_hdr10_param.g_x ||
	    hdr10_param.g_y != last_hdr10_param.g_y ||
	    hdr10_param.b_x != last_hdr10_param.b_x ||
	    hdr10_param.b_y != last_hdr10_param.b_y) {
		last_hdr10_param.r_x = hdr10_param.r_x;
		last_hdr10_param.r_y = hdr10_param.r_y;
		last_hdr10_param.g_x = hdr10_param.g_x;
		last_hdr10_param.g_y = hdr10_param.g_y;
		last_hdr10_param.b_x = hdr10_param.b_x;
		last_hdr10_param.b_y = hdr10_param.b_y;
		return 1;
	}
	return 0;
}

int get_mute_type(void)
{
	if (dolby_vision_ll_policy == DOLBY_VISION_LL_RGB444)
		return MUTE_TYPE_RGB;
	else if ((dolby_vision_ll_policy == DOLBY_VISION_LL_YUV422) ||
		 (dolby_vision_mode == AMDV_OUTPUT_MODE_SDR8) ||
		 (dolby_vision_mode == AMDV_OUTPUT_MODE_SDR10) ||
		 (dolby_vision_mode == AMDV_OUTPUT_MODE_HDR10))
		return MUTE_TYPE_YUV;
	else if ((dolby_vision_mode == AMDV_OUTPUT_MODE_IPT) ||
		 (dolby_vision_mode == AMDV_OUTPUT_MODE_IPT_TUNNEL))
		return MUTE_TYPE_IPT;
	else
		return MUTE_TYPE_NONE;
}

u32 get_graphic_width(u32 index)
{
	if (index < OSD_MAX_INDEX)
		return osd_graphic_width[index];
	else
		return osd_graphic_width[0];
}

u32 get_graphic_height(u32 index)
{
	if (index < OSD_MAX_INDEX)
		return osd_graphic_height[index];
	else
		return osd_graphic_height[0];
}

bool get_core1a_core1b_switch(void)
{
	return core1a_core1b_switch;
}

void set_update_cfg(bool flag)
{
	need_update_cfg = flag;
}

static inline void switch_to_tv_mode(void)
{
	tv_mode = 1;
	support_info |= 1 << 3;
	amdv_run_mode_delay =
		RUN_MODE_DELAY;
}

int amdv_update_setting(struct vframe_s *vf)
{
	u64 *dma_data;
	u32 size = 0;
	int i;
	u64 *p;
	int dv_id = 0;

	if (!p_funcs_stb && !p_funcs_tv)
		return -1;
	if (!dovi_setting_update_flag)
		return 0;
	if (dolby_vision_flags &
		FLAG_DISABLE_DMA_UPDATE) {
		dovi_setting_update_flag = false;
		setting_update_count++;
		return -1;
	}
	if (!dma_vaddr)
		return -1;
	if (efuse_mode == 1) {
		dovi_setting_update_flag = false;
		setting_update_count++;
		return -1;
	}
	if (is_aml_tm2_tvmode() ||
	    is_aml_t3_tvmode() ||
	    is_aml_t5w() ||
	    (is_aml_tm2_stbmode() && is_aml_stb_hdmimode() &&
	    !core1_detunnel())) {
		dma_data = tv_dovi_setting->core1_reg_lut;
		size = 8 * TV_DMA_TBL_SIZE;
		memcpy(dma_vaddr, dma_data, size);
	} else if (is_aml_t7_tvmode() ||
		(is_aml_t7_stbmode() && is_aml_stb_hdmimode() &&
		!core1_detunnel())) {
		dma_data = tv_dovi_setting->core1_reg_lut;
		size = 8 * TV_DMA_TBL_SIZE * 16;
		p = (u64 *)dma_vaddr;
		/*Write 128bit, DMA address jump 128bit * 16, then write 128bit */
		for (i = 0; i < TV_DMA_TBL_SIZE; i += 2) {
			memcpy((void *)p, dma_data, 16);
			dma_data += 2;
			p += 2 * 16;
		}
	} else if (is_aml_txlx_stbmode()) {
		dma_data = stb_core1_lut;
		size = 8 * STB_DMA_TBL_SIZE;
		memcpy(dma_vaddr, dma_data, size);
	}
	if (size && (debug_dolby & 0x8000)) {
		p = (uint64_t *)dma_vaddr;
		pr_info("dma_vaddr %p, dma size %d\n", dma_vaddr, TV_DMA_TBL_SIZE);
		for (i = 0; i < size / 8; i += 2)
			pr_info("%016llx, %016llx\n", p[i], p[i + 1]);
	}
	dovi_setting_update_flag = false;
	if (multi_dv_mode) {
		if (vf && dv_inst_valid(vf->src_fmt.dv_id))
			dv_id = vf->src_fmt.dv_id;
		setting_update_count = dv_inst[dv_id].frame_count;
	} else {
		setting_update_count = frame_count;
	}
	return -1;
}
EXPORT_SYMBOL(amdv_update_setting);

/*update timing to 1080p if size < 1080p*/
void update_dvcore2_timing(u32 *hsize, u32 *vsize)
{
	if (!is_aml_s5()) {/*s5 timing is different*/
		if (hsize && vsize &&
		    !(dolby_vision_flags & FLAG_CERTIFICATION) &&
		    !(dolby_vision_flags & FLAG_DEBUG_CORE2_TIMING) &&
		    *hsize < 1920 && *vsize < 1080) {
			*hsize = 1920;
			*vsize = 1080;
		}
	}
}
EXPORT_SYMBOL(update_dvcore2_timing);

void set_dovi_setting_update_flag(bool flag)
{
	dovi_setting_update_flag = flag;
}

void set_amdv_wait_on(void)
{
	amdv_wait_on = true;
}

void clear_dolby_vision_wait(void)
{
	int i;

	if (debug_dolby & 2)
		pr_info("clear amdv_wait_on\n");

	amdv_wait_on = false;
	if (multi_dv_mode) {
		for (i = 0; i < NUM_INST; i++) {
			dv_inst[i].amdv_wait_init = false;
			dv_inst[i].amdv_wait_count = 0;
		}
	} else {
		amdv_wait_init = false;
		amdv_wait_count = 0;
	}
}

void set_frame_count(int val)
{
	if (!multi_dv_mode)
		frame_count = val;
}

int get_frame_count(void)
{
	return frame_count;
}

void set_vf_crc_valid(int val)
{
	last_vf_valid_crc = val;
}

void reset_dv_param(void)
{
	int i;

	if (debug_dolby & 1)
		pr_info("reset dv param\n");
	dolby_vision_on = false;
	amdv_wait_on = false;
	dolby_vision_status = BYPASS_PROCESS;
	amdv_target_mode = AMDV_OUTPUT_MODE_BYPASS;
	dolby_vision_mode = AMDV_OUTPUT_MODE_BYPASS;

	cur_csc_type[VD1_PATH] = VPP_MATRIX_NULL;
	/* clean mute flag for next time dv on */
	dolby_vision_flags &= ~FLAG_MUTE;
	hdmi_frame_count = 0;
	force_bypass_from_prebld_to_vadj1 = 0;
	if (multi_dv_mode) {
		for (i = 0; i < NUM_INST; i++) {
			dv_inst[i].amdv_src_format = 0;
			dv_inst[i].amdv_wait_init = false;
			dv_inst[i].amdv_wait_count = 0;
			dv_inst[i].frame_count = 0;
		}
		for (i = 0; i < NUM_IPCORE1; i++) {
			dv_core1[i].run_mode_count = 0;
			dv_core1[i].core1_on = false;
			dv_core1[i].core1_on_cnt = 0;
			dv_core1[i].core1_disp_hsize = 0;
			dv_core1[i].core1_disp_vsize = 0;
			dv_core1[i].amdv_setting_video_flag = 0;
		}
		amdv_core2_on_cnt = 0;
		cur_valid_video_num = 0;
	} else {
		core1_disp_hsize = 0;
		core1_disp_vsize = 0;
		amdv_src_format = 0;
		amdv_wait_init = false;
		amdv_wait_count = 0;
		frame_count = 0;
		amdv_on_count = 0;
		amdv_core1_on = false;
		amdv_core1_on_cnt = 0;
		amdv_core2_on_cnt = 0;
	}
}

void update_dma_buf(void)
{
	u32 size = 0;
	u64 *dma_data;

	if (efuse_mode == 1) {
		if (is_aml_tvmode()) {
			if (dma_vaddr && tv_dovi_setting) {
				dma_data = tv_dovi_setting->core1_reg_lut;
				size = 8 * TV_DMA_TBL_SIZE;
				if (is_aml_t7())
					size = 8 * TV_DMA_TBL_SIZE * 16;
				memset(dma_vaddr, 0x0, size);
				memcpy((uint64_t *)dma_vaddr + 1,
					dma_data + 1,
					8);
			}
		}
		if (is_aml_txlx_stbmode()) {
			size = 8 * STB_DMA_TBL_SIZE;
			if (dma_vaddr)
				memset(dma_vaddr, 0x0, size);
		}
	}
}

/*  dolby vision enhanced layer receiver*/

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

void dv_vf_light_unreg_provider(void)
{
	int i;
	unsigned long flags;

	spin_lock_irqsave(&amdv_lock, flags);
	if (vfm_path_on) {
		if (multi_dv_mode) {
			/*multi-mode todo*/
			//for (i = 0; i < 16; i++) {
			//	if (dv_inst[dv_id].dv_vf[i][0]) {
			//		if (dv_inst[dv_id].dv_vf[i][1])
			//			dvel_vf_put(dv_inst[dv_id].dv_vf[i][1]);
			//		dv_inst[dv_id].dv_vf[i][1] = NULL;
			//	}
			//	dv_inst[dv_id].dv_vf[i][0] = NULL;
			//}
			//dv_inst[dv_id].frame_count = 0;
		} else {
			for (i = 0; i < 16; i++) {
				if (dv_vf[i][0]) {
					if (dv_vf[i][1])
						dvel_vf_put(dv_vf[i][1]);
					dv_vf[i][1] = NULL;
				}
				dv_vf[i][0] = NULL;
			}
			frame_count = 0;
			/* if (metadata_parser && p_funcs) {*/
			/*	p_funcs->metadata_parser_release();*/
			/*	metadata_parser = NULL;*/
			/*} */
		}
		setting_update_count = 0;
		crc_count = 0;
		crc_bypass_count = 0;
		amdv_el_disable = 0;
	}
	vfm_path_on = false;
	spin_unlock_irqrestore(&amdv_lock, flags);
}
EXPORT_SYMBOL(dv_vf_light_unreg_provider);

void dv_vf_light_reg_provider(void)
{
	int i;
	unsigned long flags;

	spin_lock_irqsave(&amdv_lock, flags);
	if (!vfm_path_on) {
		if (multi_dv_mode) {
			/*multi-mode todo*/
			//for (i = 0; i < 16; i++) {
			//	dv_inst[dv_id].dv_vf[i][0] = NULL;
			//	dv_inst[dv_id].dv_vf[i][1] = NULL;
			//}
			//dv_inst[dv_id].frame_count = 0;
			//for (i = 0; i < NUM_INST; i++)
			//memset(&dv_inst[i].hdr10_param, 0, sizeof(dv_inst[i].hdr10_param));
		} else {
			for (i = 0; i < 16; i++) {
				dv_vf[i][0] = NULL;
				dv_vf[i][1] = NULL;
			}
			frame_count = 0;
		}
		memset(&hdr10_data, 0, sizeof(hdr10_data));
		memset(&hdr10_param, 0, sizeof(hdr10_param));
		memset(&last_hdr10_param, 0, sizeof(last_hdr10_param));

		setting_update_count = 0;
		crc_count = 0;
		crc_bypass_count = 0;
		amdv_el_disable = 0;
	}
	vfm_path_on = true;
	spin_unlock_irqrestore(&amdv_lock, flags);
}
EXPORT_SYMBOL(dv_vf_light_reg_provider);

static int dvel_receiver_event_fun(int type, void *data, void *arg)
{
	char *provider_name = (char *)data;

	if (type == VFRAME_EVENT_PROVIDER_UNREG) {
		pr_info("%s, provider %s unregistered\n",
			__func__, provider_name);
		if (!multi_dv_mode)/*dvel is not used for multi-mode*/
			dv_vf_light_unreg_provider();
		return -1;
	} else if (type == VFRAME_EVENT_PROVIDER_QUREY_STATE) {
		return RECEIVER_ACTIVE;
	} else if (type == VFRAME_EVENT_PROVIDER_REG) {
		pr_info("%s, provider %s registered\n",
			__func__, provider_name);
		if (!multi_dv_mode)/*dvel is not used for multi-mode*/
			dv_vf_light_reg_provider();
	}
	return 0;
}

static const struct vframe_receiver_op_s dvel_vf_receiver = {
	.event_cb = dvel_receiver_event_fun
};

static struct vframe_receiver_s dvel_vf_recv;

void amdv_init_receiver(void *pdev)
{
	ulong alloc_size;
	int i;

	pr_info("%s(%s)\n", __func__, DVEL_RECV_NAME);
	vf_receiver_init(&dvel_vf_recv, DVEL_RECV_NAME,
			&dvel_vf_receiver, &dvel_vf_recv);
	vf_reg_receiver(&dvel_vf_recv);
	pr_info("%s: %s\n", __func__, dvel_vf_recv.name);
	amdv_pdev = (struct platform_device *)pdev;
	alloc_size = dma_size;
	if (is_aml_t7())
		alloc_size = dma_size * 16; /*t7 need dma addr align to 128bit*/
	alloc_size = (alloc_size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
	dma_vaddr = dma_alloc_coherent(&amdv_pdev->dev,
		alloc_size, &dma_paddr, GFP_KERNEL);
	pr_info("get dma_vaddr %p\n", dma_vaddr);
	for (i = 0; i < 2; i++) {
		md_buf[i] = vmalloc(MD_BUF_SIZE);
		if (md_buf[i])
			memset(md_buf[i], 0, MD_BUF_SIZE);
		comp_buf[i] = vmalloc(COMP_BUF_SIZE);
		if (comp_buf[i])
			memset(comp_buf[i], 0, COMP_BUF_SIZE);
		drop_md_buf[i] = vmalloc(MD_BUF_SIZE);
		if (drop_md_buf[i])
			memset(drop_md_buf[i], 0, MD_BUF_SIZE);
		drop_comp_buf[i] = vmalloc(COMP_BUF_SIZE);
		if (drop_comp_buf[i])
			memset(drop_comp_buf[i], 0, COMP_BUF_SIZE);
	}
	vsem_if_buf = vmalloc(VSEM_IF_BUF_SIZE);
	if (vsem_if_buf)
		memset(vsem_if_buf, 0, VSEM_IF_BUF_SIZE);
	vsem_md_buf = vmalloc(VSEM_IF_BUF_SIZE);
	if (vsem_md_buf)
		memset(vsem_md_buf, 0, VSEM_IF_BUF_SIZE);
}

void amdv_clear_buf(u8 dv_id)
{
	int i;
	unsigned long flags;

	spin_lock_irqsave(&amdv_lock, flags);

	if (debug_dolby & 0x1000)
		pr_dv_dbg("[inst%d]%s\n", dv_id + 1, __func__);

	if (multi_dv_mode) {
		if (dv_id < NUM_INST) {
			for (i = 0; i < 16; i++) {
				if ((debug_dolby & 0x1000) && dv_inst[dv_id].dv_vf[i][0])
					pr_dv_dbg("[inst%d]clear dv_vf %p\n",
						     dv_id + 1, dv_inst[dv_id].dv_vf[i][0]);
				dv_inst[dv_id].dv_vf[i][0] = NULL;
				dv_inst[dv_id].dv_vf[i][1] = NULL;
			}
		}
	} else {
		for (i = 0; i < 16; i++) {
			dv_vf[i][0] = NULL;
			dv_vf[i][1] = NULL;
		}
	}
	spin_unlock_irqrestore(&amdv_lock, flags);
}

void amdv_create_inst(void)
{
	int i;

	for (i = 0; i < NUM_INST; i++) {
		dv_inst[i].md_buf[0] = vmalloc(MD_BUF_SIZE);
		if (dv_inst[i].md_buf[0])
			memset(dv_inst[0].md_buf[0], 0, MD_BUF_SIZE);
		dv_inst[i].comp_buf[0] = vmalloc(COMP_BUF_SIZE);
		if (dv_inst[i].comp_buf[0])
			memset(dv_inst[i].comp_buf[0], 0, COMP_BUF_SIZE);

		dv_inst[i].md_buf[1] = vmalloc(MD_BUF_SIZE);
		if (dv_inst[i].md_buf[1])
			memset(dv_inst[0].md_buf[1], 0, MD_BUF_SIZE);
		dv_inst[i].comp_buf[1] = vmalloc(COMP_BUF_SIZE);
		if (dv_inst[i].comp_buf[1])
			memset(dv_inst[i].comp_buf[1], 0, COMP_BUF_SIZE);

		dv_inst[i].current_id = 0;
		dv_inst[i].metadata_parser = NULL;
		dv_inst[i].layer_id = VD_PATH_MAX;
		dv_inst[i].mapped = false;
	}
	for (i = 0; i < NUM_IPCORE1; i++) {
		dv_core1[i].core1_disp_hsize = 0;
		dv_core1[i].core1_disp_vsize = 0;
		dv_core1[i].amdv_setting_video_flag = false;
		dv_core1[i].core1_on = false;
		dv_core1[i].core1_on_cnt = 0;
		dv_core1[i].run_mode_count = 0;
	}

	graphic_md_buf = vmalloc(MD_BUF_SIZE);
	if (graphic_md_buf)
		memset(graphic_md_buf, 0, MD_BUF_SIZE);
}

static DEFINE_MUTEX(dv_inst_lock);

int dv_inst_map(int *inst)
{
	int i;
	int ret = 0;
	int new_map_id = -1;
	/*bool keep_last_frame = false; both two video keep last frame*/

	if (!multi_dv_mode) {
		*inst = 0;
		return 0;
	}

	mutex_lock(&dv_inst_lock);

	for (i = 0; i < NUM_INST; i++) {
		if (!dv_inst[i].mapped)
			break;
	}
	if (i == NUM_INST) {
		ret = -1;
		pr_info("dv_inst all mapped!");
	}

	if (ret == -1) {
		mutex_unlock(&dv_inst_lock);
		*inst = -1;
		pr_info("%s failed\n", __func__);
		return -ENODEV;
	}
	/*for (i = 0; i < NUM_IPCORE1; i++) {*/
	/*	pr_dv_dbg("[%s]layer_id %d, mapped %d\n", __func__,*/
	/*		     dv_inst[i].layer_id, dv_inst[i].mapped);*/
	/*	if (dv_inst[i].layer_id == i && dv_inst[i].mapped == false) {*/
	/*		keep_last_frame = true;*/
	/*	} else {*/
	/*		keep_last_frame = false;*/
	/*		break;*/
	/*	}*/
	/*}*/

	/*if (keep_last_frame) {*/
	/*	new_map_id = last_unmap_id;*/
	/*	pr_dv_dbg("[%s]new map id: %d\n",*/
	/*		     __func__, new_map_id);*/
	/*} else {*/
	/*	for (i = 0; i < NUM_INST; i++) {*/
	/*		if (!dv_inst[i].mapped) {*/
	/*			new_map_id = i;*/
	/*			pr_info("[%s]map id %d\n", __func__, new_map_id);*/
	/*			break;*/
	/*		}*/
	/*	}*/
	/*}*/
	for (i = 0; i < NUM_INST; i++) {
		if (!dv_inst[i].mapped) {
			new_map_id = i;
			pr_info("[%s]map id %d\n", __func__, new_map_id);
			break;
		}
	}
	if (new_map_id >= 0) {
		dv_inst[new_map_id].mapped = true;
		*inst = new_map_id;

		if (!dv_inst[*inst].metadata_parser && p_funcs_stb)
			dv_inst[*inst].metadata_parser =
				p_funcs_stb->multi_mp_init(dolby_vision_flags
								& FLAG_CHANGE_SEQ_HEAD
								? 1 : 0);
		if (dv_inst[*inst].metadata_parser) {
			p_funcs_stb->multi_mp_reset(dv_inst[*inst].metadata_parser, 1);
			pr_dv_dbg("reset mp\n");
		}
		mutex_unlock(&dv_inst_lock);
		/*clear dv_vf and framecount*/
		amdv_clear_buf(*inst);
		dv_inst[*inst].frame_count = 0;
		dv_inst[*inst].last_mel_mode = 0;
		dv_inst[*inst].last_total_md_size = 0;
		dv_inst[*inst].last_total_comp_size = 0;
		dv_inst[*inst].layer_id = VD_PATH_MAX;
		dv_inst[*inst].video_height = 0;
		dv_inst[*inst].video_width = 0;
		dv_inst[*inst].src_format = FORMAT_SDR;
		dv_inst[*inst].valid = 0;
		dv_inst[*inst].current_id = 0;
		dv_inst[*inst].in_md = NULL;
		dv_inst[*inst].in_md_size = 0;
		dv_inst[*inst].in_comp = NULL;
		dv_inst[*inst].in_comp_size = 0;
		return 0;
	}
	mutex_unlock(&dv_inst_lock);
	*inst = -1;
	pr_info("%s failed\n", __func__);
	return -ENODEV;
}
EXPORT_SYMBOL(dv_inst_map);

void dv_inst_unmap(int inst)
{
	if (!multi_dv_mode)
		return;

	pr_info("%s %d\n", __func__, inst);

	mutex_lock(&dv_inst_lock);

	if (dv_inst_valid(inst) && dv_inst[inst].mapped) {
		dv_inst[inst].mapped = false;
		last_unmap_id = inst;
		if (dv_inst[inst].metadata_parser && p_funcs_stb) {
			p_funcs_stb->multi_mp_release
				(&dv_inst[inst].metadata_parser);
			dv_inst[inst].metadata_parser = NULL;
		}
		pr_info("%s %d OK\n", __func__, inst);
	}
	mutex_unlock(&dv_inst_lock);
}
EXPORT_SYMBOL(dv_inst_unmap);

void force_unmap_all_inst(void)
{
	int i;

	if (!multi_dv_mode)
		return;

	for (i = 0; i < NUM_INST; i++) {
		amdv_clear_buf(i);
		dv_inst[i].mapped = 0;
		dv_inst[i].frame_count = 0;
		dv_inst[i].last_mel_mode = 0;
		dv_inst[i].last_total_md_size = 0;
		dv_inst[i].last_total_comp_size = 0;
		dv_inst[i].layer_id = VD_PATH_MAX;
		dv_inst[i].video_height = 0;
		dv_inst[i].video_width = 0;
		dv_inst[i].src_format = FORMAT_INVALID;
		dv_inst[i].valid = 0;
		dv_inst[i].current_id = 0;
		dv_inst[i].in_md = NULL;
		dv_inst[i].in_md_size = 0;
		dv_inst[i].in_comp = NULL;
		dv_inst[i].in_comp_size = 0;
	}
}

#define MAX_FILENAME_LENGTH 64
static const char comp_file[] = "%s_comp.%04d.reg";
static const char dm_reg_core1_file[] = "%s_dm_core1.%04d.reg";
static const char dm_reg_core2_file[] = "%s_dm_core2.%04d.reg";
static const char dm_reg_core3_file[] = "%s_dm_core3.%04d.reg";
static const char dm_lut_core1_file[] = "%s_dm_core1.%04d.lut";
static const char dm_lut_core2_file[] = "%s_dm_core2.%04d.lut";
/*for multi video*/
static const char comp_file_1[] = "%s_comp_1.%04d.reg";
static const char dm_reg_core1_1_file[] = "%s_dm_core1_1.%04d.reg";
static const char dm_lut_core1_1_file[] = "%s_dm_core1_1.%04d.lut";

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
		"/data/tmp/tmp", frame_nr - 1);
	fp = filp_open(fn, O_RDWR | O_CREAT, 0666);
	if (!fp) {
		pr_info("Error open file for writing NULL\n");
	} else {
		vfs_write(fp, structure, struct_length, &pos);
		vfs_fsync(fp, 0);
		filp_close(fp, NULL);
	}
	set_fs(old_fs);
}

void amdv_dump_struct(void)
{
	int i;

	if (multi_dv_mode) {
		for (i = 0; i < NUM_IPCORE1; i++) {
			dump_struct(&m_dovi_setting.core1[i].dm_reg,
				    sizeof(m_dovi_setting.core1[i].dm_reg),
				    dm_reg_core1_file, dv_inst[i].frame_count);

			dump_struct(&m_dovi_setting.core1[i].comp_reg,
				    sizeof(m_dovi_setting.core1[i].comp_reg),
				    comp_file, dv_inst[i].frame_count);

			dump_struct(&m_dovi_setting.core1[i].dm_lut,
				    sizeof(m_dovi_setting.core1[i].dm_lut),
				    dm_lut_core1_file, dv_inst[i].frame_count);
		}
		if (!is_graphics_output_off()) {
			dump_struct(&dovi_setting.dm_reg2,
				    sizeof(dovi_setting.dm_reg2),
				    dm_reg_core2_file, dv_inst[0].frame_count);
			dump_struct(&dovi_setting.dm_lut2,
				    sizeof(dovi_setting.dm_lut2),
				    dm_lut_core2_file, dv_inst[0].frame_count);
		}
		dump_struct(&dovi_setting.dm_reg3,
			    sizeof(dovi_setting.dm_reg3),
			    dm_reg_core3_file, dv_inst[0].frame_count);

		pr_dv_dbg("setting for frame %d %ddumped\n", dv_inst[0].frame_count,
			     dv_inst[0].frame_count);
	} else {
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

		pr_dv_dbg("setting for frame %d dumped\n", frame_count);
	}
}
EXPORT_SYMBOL(amdv_dump_struct);

static void dump_setting(struct dovi_setting_s *setting,
			      int frame_cnt, int debug_flag)
{
	int i;
	u32 *p;

	if ((debug_flag & 0x10) && dump_enable) {
		pr_info("path control reg\n");
		if (is_aml_txlx_stbmode()) {
			pr_info("txlx dv core1/2 reg: 0x1a07 val = 0x%x\n",
				READ_VPP_DV_REG(VIU_MISC_CTRL1));
		} else if (is_aml_g12() || is_aml_sc2() || is_aml_s4d()) {
			pr_info("g12/sc2/s4d  stb reg: 0x1a0c val = 0x%x\n",
				READ_VPP_DV_REG(AMDV_PATH_CTRL));
		} else if (is_aml_tm2_stbmode()) {
			pr_info("tm2_stb reg: 0x1a0c val = 0x%x\n",
				READ_VPP_DV_REG(AMDV_PATH_CTRL));
		} else if (is_aml_t7_stbmode()) {
			pr_info("t7_stb reg: SWAP_CTRL1 0x1a70 val = 0x%x\n",
				READ_VPP_DV_REG(AMDV_PATH_SWAP_CTRL1));
			pr_info("t7_stb reg: SWAP_CTRL2 0x1a71 val = 0x%x\n",
				READ_VPP_DV_REG(AMDV_PATH_SWAP_CTRL2));
			pr_info("t7_stb reg: vd1 core1 0x1a83 (bit4)val = 0x%x\n",
				READ_VPP_DV_REG(VPP_VD1_DSC_CTRL));
			pr_info("t7_stb reg: vd2 core1 0x1a84 val = 0x%x\n",
				READ_VPP_DV_REG(VPP_VD2_DSC_CTRL));
			pr_info("t7_stb reg: vd3 core1 0x1a85 val = 0x%x\n",
				READ_VPP_DV_REG(VPP_VD3_DSC_CTRL));
			pr_info("t7_stb reg: core2a 1a0f(bit14) val = 0x%x\n",
				READ_VPP_DV_REG(MALI_AFBCD_TOP_CTRL));
			pr_info("t7_stb reg: core2c 1a55(bit19) val = 0x%x\n",
				READ_VPP_DV_REG(MALI_AFBCD1_TOP_CTRL));
		} else if (is_aml_s5()) {
			pr_info("s5 reg: vd1 core1 0x2822(bit0, 1:enable) = 0x%x\n",
				READ_VPP_DV_REG(VD1_S0_DV_BYPASS_CTRL));
			pr_info("s5 reg: vd2 core1 0x3888(bit0, 1:enable) = 0x%x\n",
				READ_VPP_DV_REG(VD2_DV_BYPASS_CTRL));
			pr_info("s5 reg: core2a core2c 0x6077(bit0 bit4, 1:bypass) = 0x%x\n",
				READ_VPP_DV_REG(OSD_DOLBY_BYPASS_EN));
			pr_info("s5 reg: core3 VPU_CTRL 0x10fd(bit11, 1:enable) = 0x%x\n",
				READ_VPP_DV_REG(VPU_DOLBY_TOP_CTRL));
			pr_info("s5 reg: core3 S0 0x2501(bit3, 1:enable) = 0x%x\n",
				READ_VPP_DV_REG(S5_VPP_DOLBY_CTRL));
			pr_info("s5 reg: core3 S1 0x2601(bit3, 1:enable) = 0x%x\n",
				READ_VPP_DV_REG(VPP_SLICE1_DOLBY_CTRL));
			pr_info("s5 reg: core3 S2 0x2c01(bit3, 1:enable) = 0x%x\n",
				READ_VPP_DV_REG(VPP_SLICE2_DOLBY_CTRL));
			pr_info("s5 reg: core3 S3 0x3e01(bit3, 1:enable) = 0x%x\n",
				READ_VPP_DV_REG(VPP_SLICE3_DOLBY_CTRL));
			pr_info("s5 swap reg: core1a 0x3300, core1b 0x0a00\n");
			pr_info("s5 swap reg: core2a 0x0b00, core2c 0x0d00\n");
			pr_info("s5 swap reg: core3 0x0e00,0x0f00,0x1200,0x1300\n");
		}

		pr_info("core1\n");
		p = (u32 *)&setting->dm_reg1;
		for (i = 0; i < 27; i++)
			pr_info("%08x\n", p[i]);
		pr_info("\ncomposer\n");
		p = (u32 *)&setting->comp_reg;
		for (i = 0; i < 173; i++)
			pr_info("%08x\n", p[i]);
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
		if (is_aml_gxm()) {
			pr_info("core1 swap\n");
			for (i = AMDV_CORE1A_CLKGATE_CTRL;
				i <= AMDV_CORE1A_DMA_PORT; i++)
				pr_info("[0x%4x] = 0x%x\n",
					i, READ_VPP_DV_REG(i));
			pr_info("core1 real reg\n");
			for (i = AMDV_CORE1A_REG_START;
				i <= AMDV_CORE1A_REG_START + 5;
				i++)
				pr_info("[0x%4x] = 0x%x\n",
					i, READ_VPP_DV_REG(i));
			pr_info("core1 composer real reg\n");
			for (i = 0; i < 173 ; i++)
				pr_info("%08x\n",
					READ_VPP_DV_REG
					(AMDV_CORE1A_REG_START + 50 + i));
		} else if (is_aml_txlx_stbmode()) {
			pr_info("core1 swap\n");
			for (i = AMDV_CORE1A_CLKGATE_CTRL;
				i <= AMDV_CORE1A_DMA_PORT; i++)
				pr_info("[0x%4x] = 0x%x\n",
					i, READ_VPP_DV_REG(i));
			pr_info("core1 real reg\n");
			for (i = AMDV_CORE1A_REG_START;
				i <= AMDV_CORE1A_REG_START + 5;
				i++)
				pr_info("[0x%4x] = 0x%x\n",
					i, READ_VPP_DV_REG(i));
		}
#endif
	}
	if ((debug_flag & 0x20) && dump_enable) {
		pr_info("\ncore1lut\n");
		p = (u32 *)&setting->dm_lut1.tm_lut_i;
		for (i = 0; i < 64; i++)
			pr_info
			("%08x, %08x, %08x, %08x\n",
			 p[i * 4 + 3], p[i * 4 + 2], p[i * 4 + 1], p[i * 4]);
		pr_info("\n");
		p = (u32 *)&setting->dm_lut1.tm_lut_s;
		for (i = 0; i < 64; i++)
			pr_info
			("%08x, %08x, %08x, %08x\n",
			 p[i * 4 + 3], p[i * 4 + 2], p[i * 4 + 1], p[i * 4]);
		pr_info("\n");
		p = (u32 *)&setting->dm_lut1.sm_lut_i;
		for (i = 0; i < 64; i++)
			pr_info
			("%08x, %08x, %08x, %08x\n",
			 p[i * 4 + 3], p[i * 4 + 2], p[i * 4 + 1], p[i * 4]);
		pr_info("\n");
		p = (u32 *)&setting->dm_lut1.sm_lut_s;
		for (i = 0; i < 64; i++)
			pr_info
			("%08x, %08x, %08x, %08x\n",
			 p[i * 4 + 3], p[i * 4 + 2], p[i * 4 + 1], p[i * 4]);
		pr_info("\n");
		p = (u32 *)&setting->dm_lut1.g_2_l;
		for (i = 0; i < 64; i++)
			pr_info
			("%08x, %08x, %08x, %08x\n",
			 p[i * 4 + 3], p[i * 4 + 2], p[i * 4 + 1], p[i * 4]);
		pr_info("\n");
	}

	if ((debug_flag & 0x10) && dump_enable && !is_graphics_output_off()) {
		pr_info("core2\n");
		p = (u32 *)&setting->dm_reg2;
		for (i = 0; i < 24; i++)
			pr_info("%08x\n", p[i]);
		pr_info("core2a swap\n");
		for (i = AMDV_CORE2A_CLKGATE_CTRL;
			i <= AMDV_CORE2A_DMA_PORT; i++)
			pr_info("[0x%4x] = 0x%x\n",
				i, READ_VPP_DV_REG(i));
		pr_info("core2a real reg\n");
		for (i = AMDV_CORE2A_REG_START;
			i <= AMDV_CORE2A_REG_START + 30; i++)
			pr_info("[0x%4x] = 0x%x\n",
				i, READ_VPP_DV_REG(i));
		if (is_aml_s5()) {
			pr_info("core2c swap\n");
			for (i = AMDV_CORE2C_CLKGATE_CTRL;
				i <= AMDV_CORE2C_DMA_PORT; i++)
				pr_info("[0x%4x] = 0x%x\n",
					i, READ_VPP_DV_REG(i));
			pr_info("core2c real reg\n");
			for (i = AMDV_CORE2C_REG_START;
				i <= AMDV_CORE2C_REG_START + 30; i++)
				pr_info("[0x%4x] = 0x%x\n",
					i, READ_VPP_DV_REG(i));
		}
	}

	if ((debug_flag & 0x20) && dump_enable && !is_graphics_output_off()) {
		pr_info("\ncore2lut\n");
		p = (u32 *)&setting->dm_lut2.tm_lut_i;
		for (i = 0; i < 64; i++)
			pr_info
			("%08x, %08x, %08x, %08x\n",
			 p[i * 4 + 3], p[i * 4 + 2], p[i * 4 + 1], p[i * 4]);
		pr_info("\n");
		p = (u32 *)&setting->dm_lut2.tm_lut_s;
		for (i = 0; i < 64; i++)
			pr_info
			("%08x, %08x, %08x, %08x\n",
			 p[i * 4 + 3], p[i * 4 + 2], p[i * 4 + 1], p[i * 4]);
		pr_info("\n");
		p = (u32 *)&setting->dm_lut2.sm_lut_i;
		for (i = 0; i < 64; i++)
			pr_info
			("%08x, %08x, %08x, %08x\n",
			 p[i * 4 + 3], p[i * 4 + 2], p[i * 4 + 1], p[i * 4]);
		pr_info("\n");
		p = (u32 *)&setting->dm_lut2.sm_lut_s;
		for (i = 0; i < 64; i++)
			pr_info
			("%08x, %08x, %08x, %08x\n",
			 p[i * 4 + 3], p[i * 4 + 2], p[i * 4 + 1], p[i * 4]);
		pr_info("\n");
		p = (u32 *)&setting->dm_lut2.g_2_l;
		for (i = 0; i < 64; i++)
			pr_info
			("%08x, %08x, %08x, %08x\n",
			 p[i * 4 + 3], p[i * 4 + 2], p[i * 4 + 1], p[i * 4]);
		pr_info("\n");
	}

	if ((debug_flag & 0x10) && dump_enable) {
		pr_info("core3\n");
		p = (u32 *)&setting->dm_reg3;
		for (i = 0; i < 26; i++)
			pr_info("%08x\n", p[i]);
		pr_info("core3 swap\n");
		for (i = AMDV_CORE3_CLKGATE_CTRL;
			i <= AMDV_CORE3_OUTPUT_CSC_CRC; i++)
			pr_info("[0x%4x] = 0x%x\n",
				i, READ_VPP_DV_REG(i));
		pr_info("core3 real reg\n");
		for (i = AMDV_CORE3_REG_START;
			i <= AMDV_CORE3_REG_START + 67; i++)
			pr_info("[0x%4x] = 0x%x\n",
				i, READ_VPP_DV_REG(i));
		if (is_aml_s5()) {
			pr_info("core3 S1 swap\n");
			for (i = AMDV_CORE3_S1_CLKGATE_CTRL;
				i <= AMDV_CORE3_S1_OUTPUT_CSC_CRC; i++)
				pr_info("[0x%4x] = 0x%x\n",
					i, READ_VPP_DV_REG(i));
			pr_info("core3 S1 real reg\n");
			for (i = AMDV_CORE3_S1_REG_START;
				i <= AMDV_CORE3_S1_REG_START + 67; i++)
				pr_info("[0x%4x] = 0x%x\n",
					i, READ_VPP_DV_REG(i));
			pr_info("core3 S2 swap\n");
			for (i = AMDV_CORE3_S2_CLKGATE_CTRL;
				i <= AMDV_CORE3_S2_OUTPUT_CSC_CRC; i++)
				pr_info("[0x%4x] = 0x%x\n",
					i, READ_VPP_DV_REG(i));
			pr_info("core3 S2 real reg\n");
			for (i = AMDV_CORE3_S2_REG_START;
				i <= AMDV_CORE3_S2_REG_START + 67; i++)
				pr_info("[0x%4x] = 0x%x\n",
					i, READ_VPP_DV_REG(i));
			pr_info("core3 S3 swap\n");
			for (i = AMDV_CORE3_S3_CLKGATE_CTRL;
				i <= AMDV_CORE3_S3_OUTPUT_CSC_CRC; i++)
				pr_info("[0x%4x] = 0x%x\n",
					i, READ_VPP_DV_REG(i));
			pr_info("core3 S3 real reg\n");
			for (i = AMDV_CORE3_S3_REG_START;
				i <= AMDV_CORE3_S3_REG_START + 67; i++)
				pr_info("[0x%4x] = 0x%x\n",
					i, READ_VPP_DV_REG(i));

			pr_info("core3 meta ctrl\n");
			for (i = SLICE0_META_CTRL0;
				i <= SLICE0_META_CTRL0 + 12; i++)
				pr_info("[0x%4x] = 0x%x\n",
					i, READ_VPP_DV_REG(i));
		}
	}

	if ((debug_flag & 0x40) && dump_enable &&
	    dolby_vision_mode <= AMDV_OUTPUT_MODE_IPT_TUNNEL) {
		pr_info("\ncore3_meta %d\n", setting->md_reg3.size);
		p = setting->md_reg3.raw_metadata;
		for (i = 0; i < setting->md_reg3.size; i++)
			pr_info("%08x\n", p[i]);
		pr_info("\n");
	}
}

static void dump_m_setting(struct m_dovi_setting_s *m_setting,
				  int frame_cnt, int debug_flag, int inst_id)
{
	int i;
	int j;
	u32 *p;

	if ((debug_flag & 0x10) && dump_enable_f(inst_id)) {
		if (is_aml_txlx_stbmode()) {
			pr_info("txlx dv core1/2 reg: 0x1a07 = 0x%x\n",
				READ_VPP_DV_REG(VIU_MISC_CTRL1));
		} else if (is_aml_g12() || is_aml_sc2() || is_aml_s4d()) {
			pr_info("g12/sc2/s4d  stb reg: 0x1a0c = 0x%x\n",
				READ_VPP_DV_REG(AMDV_PATH_CTRL));
		} else if (is_aml_tm2_stbmode()) {
			pr_info("tm2_stb reg: 0x1a0c = 0x%x\n",
				READ_VPP_DV_REG(AMDV_PATH_CTRL));
		} else if (is_aml_t7_stbmode()) {
			pr_info("t7_stb reg: vd1 core1 0x1a83(bit4) = 0x%x\n",
				READ_VPP_DV_REG(VPP_VD1_DSC_CTRL));
			pr_info("t7_stb reg: vd2 core1 0x1a84 val = 0x%x\n",
				READ_VPP_DV_REG(VPP_VD2_DSC_CTRL));
			pr_info("t7_stb reg: vd3 core1 0x1a85 val = 0x%x\n",
				READ_VPP_DV_REG(VPP_VD3_DSC_CTRL));
			pr_info("t7_stb reg: core2a 0x1a0f(bit14) = 0x%x\n",
				READ_VPP_DV_REG(MALI_AFBCD_TOP_CTRL));
			pr_info("t7_stb reg: core2c 0x1a55(bit19) = 0x%x\n",
				READ_VPP_DV_REG(MALI_AFBCD1_TOP_CTRL));
		} else if (is_aml_s5()) {
			pr_info("s5 reg: vd1 core1 0x2822(bit0, 1:enable) = 0x%x\n",
				READ_VPP_DV_REG(VD1_S0_DV_BYPASS_CTRL));
			pr_info("s5 reg: vd2 core1 0x3888(bit0, 1:enable) = 0x%x\n",
				READ_VPP_DV_REG(VD2_DV_BYPASS_CTRL));
			pr_info("s5 reg: core2a core2c 0x6077(bit0 bit4, 1:bypass) = 0x%x\n",
				READ_VPP_DV_REG(OSD_DOLBY_BYPASS_EN));
			pr_info("s5 reg: core3 VPU_CTRL 0x10fd(bit11, 1:enable) = 0x%x\n",
				READ_VPP_DV_REG(VPU_DOLBY_TOP_CTRL));
			pr_info("s5 reg: core3 S0 0x2501(bit3, 1:enable) = 0x%x\n",
				READ_VPP_DV_REG(S5_VPP_DOLBY_CTRL));
			pr_info("s5 reg: core3 S1 0x2601(bit3, 1:enable) = 0x%x\n",
				READ_VPP_DV_REG(VPP_SLICE1_DOLBY_CTRL));
			pr_info("s5 reg: core3 S2 0x2c01(bit3, 1:enable) = 0x%x\n",
				READ_VPP_DV_REG(VPP_SLICE2_DOLBY_CTRL));
			pr_info("s5 reg: core3 S3 0x3e01(bit3, 1:enable) = 0x%x\n",
				READ_VPP_DV_REG(VPP_SLICE3_DOLBY_CTRL));
			pr_info("s5 swap reg: core1a 0x3300, core1b 0x0a00\n");
			pr_info("s5 swap reg: core2a 0x0b00, core2c 0x0d00\n");
			pr_info("s5 swap reg: core3 0x0e00,0x0f00,0x1200,0x1300\n");
		}
	}

	for (j = 0; j < NUM_IPCORE1; j++) {
		if ((debug_flag & 0x10) && dump_enable_f(inst_id)) {
			pr_info("video-%d\n", j + 1);
			p = (u32 *)&m_setting->core1[j].dm_reg;
			for (i = 0; i < 27; i++)
				pr_info("%08x\n", p[i]);
			pr_info("\nvideo-%d composer\n", j + 1);
			p = (u32 *)&m_setting->core1[j].comp_reg;
			for (i = 0; i < 173; i++)
				pr_info("%08x\n", p[i]);
		}
		if ((debug_flag & 0x10) && dump_enable_f(inst_id)) {
			pr_info("\nvideo-%d lut\n", j + 1);
			p = (uint32_t *)&m_setting->core1[j].dm_lut.tm_lut_i;
			for (i = 0; i < 64; i++)
				pr_info("%08x, %08x, %08x, %08x\n",
					p[i * 4 + 3], p[i * 4 + 2], p[i * 4 + 1], p[i * 4]);
			pr_info("\n");
			p = (uint32_t *)&m_setting->core1[j].dm_lut.tm_lut_s;
			for (i = 0; i < 64; i++)
				pr_info("%08x, %08x, %08x, %08x\n",
					p[i * 4 + 3], p[i * 4 + 2], p[i * 4 + 1], p[i * 4]);
			pr_info("\n");
			p = (uint32_t *)&m_setting->core1[j].dm_lut.sm_lut_i;
			for (i = 0; i < 64; i++)
				pr_info("%08x, %08x, %08x, %08x\n",
					p[i * 4 + 3], p[i * 4 + 2], p[i * 4 + 1], p[i * 4]);
			pr_info("\n");
			p = (uint32_t *)&m_setting->core1[j].dm_lut.sm_lut_s;
			for (i = 0; i < 64; i++)
				pr_info("%08x, %08x, %08x, %08x\n",
					p[i * 4 + 3], p[i * 4 + 2], p[i * 4 + 1], p[i * 4]);
			pr_info("\n");
			p = (uint32_t *)&m_setting->core1[j].dm_lut.g_2_l;
			for (i = 0; i < 64; i++)
				pr_info("%08x, %08x, %08x, %08x\n",
					p[i * 4 + 3], p[i * 4 + 2], p[i * 4 + 1], p[i * 4]);
			pr_info("\n");
		}
	}

	if ((debug_flag & 0x20) && dump_enable_f(0) && !is_graphics_output_off()) {
		pr_info("core2\n");
		p = (uint32_t *)&m_setting->dm_reg2;
		for (i = 0; i < 24; i++)
			pr_info("%08x\n", p[i]);
		pr_info("core2a swap\n");
		for (i = AMDV_CORE2A_CLKGATE_CTRL;
			i <= AMDV_CORE2A_DMA_PORT; i++)
			pr_info("[0x%4x] = 0x%x\n",
				i, READ_VPP_DV_REG(i));
		pr_info("core2a real reg\n");
		for (i = AMDV_CORE2A_REG_START;
			i <= AMDV_CORE2A_REG_START + 30; i++)
			pr_info("[0x%4x] = 0x%x\n",
				i, READ_VPP_DV_REG(i));
		if (is_aml_s5()) {
			pr_info("core2c swap\n");
			for (i = AMDV_CORE2C_CLKGATE_CTRL;
				i <= AMDV_CORE2C_DMA_PORT; i++)
				pr_info("[0x%4x] = 0x%x\n",
					i, READ_VPP_DV_REG(i));
			pr_info("core2c real reg\n");
			for (i = AMDV_CORE2C_REG_START;
				i <= AMDV_CORE2C_REG_START + 30; i++)
				pr_info("[0x%4x] = 0x%x\n",
					i, READ_VPP_DV_REG(i));
		}
	}

	if ((debug_flag & 0x20) && dump_enable_f(0) && !is_graphics_output_off()) {
		pr_info("\ncore2lut\n");
		p = (uint32_t *)&m_setting->dm_lut2.tm_lut_i;
		for (i = 0; i < 64; i++)
			pr_info("%08x, %08x, %08x, %08x\n",
				p[i * 4 + 3], p[i * 4 + 2], p[i * 4 + 1], p[i * 4]);
		pr_info("\n");
		p = (uint32_t *)&m_setting->dm_lut2.tm_lut_s;
		for (i = 0; i < 64; i++)
			pr_info("%08x, %08x, %08x, %08x\n",
				p[i * 4 + 3], p[i * 4 + 2], p[i * 4 + 1], p[i * 4]);
		pr_info("\n");
		p = (uint32_t *)&m_setting->dm_lut2.sm_lut_i;
		for (i = 0; i < 64; i++)
			pr_info("%08x, %08x, %08x, %08x\n",
				p[i * 4 + 3], p[i * 4 + 2], p[i * 4 + 1], p[i * 4]);
		pr_info("\n");
		p = (uint32_t *)&m_setting->dm_lut2.sm_lut_s;
		for (i = 0; i < 64; i++)
			pr_info("%08x, %08x, %08x, %08x\n",
				p[i * 4 + 3], p[i * 4 + 2], p[i * 4 + 1], p[i * 4]);
		pr_info("\n");
		p = (uint32_t *)&m_setting->dm_lut2.g_2_l;
		for (i = 0; i < 64; i++)
			pr_info("%08x, %08x, %08x, %08x\n",
				p[i * 4 + 3], p[i * 4 + 2], p[i * 4 + 1], p[i * 4]);
		pr_info("\n");
	}

	if ((debug_flag & 0x40) && dump_enable_f(0)) {
		pr_info("core3\n");
		p = (uint32_t *)&m_setting->dm_reg3;
		for (i = 0; i < 26; i++)
			pr_info("%08x\n", p[i]);
		pr_info("core3 swap\n");
		for (i = AMDV_CORE3_CLKGATE_CTRL;
			i <= AMDV_CORE3_OUTPUT_CSC_CRC; i++)
			pr_info("[0x%4x] = 0x%x\n",
				i, READ_VPP_DV_REG(i));
		pr_info("core3 real reg\n");
		for (i = AMDV_CORE3_REG_START;
			i <= AMDV_CORE3_REG_START + 67; i++)
			pr_info("[0x%4x] = 0x%x\n",
				i, READ_VPP_DV_REG(i));
		if (is_aml_s5() && core3_slice_info.slice_num > 1) {
			pr_info("core3 S1 swap\n");
			for (i = AMDV_CORE3_S1_CLKGATE_CTRL;
				i <= AMDV_CORE3_S1_OUTPUT_CSC_CRC; i++)
				pr_info("[0x%4x] = 0x%x\n",
					i, READ_VPP_DV_REG(i));
			pr_info("core3 S1 real reg\n");
			for (i = AMDV_CORE3_S1_REG_START;
				i <= AMDV_CORE3_S1_REG_START + 67; i++)
				pr_info("[0x%4x] = 0x%x\n",
					i, READ_VPP_DV_REG(i));
			pr_info("core3 S2 swap\n");
			for (i = AMDV_CORE3_S2_CLKGATE_CTRL;
				i <= AMDV_CORE3_S2_OUTPUT_CSC_CRC; i++)
				pr_info("[0x%4x] = 0x%x\n",
					i, READ_VPP_DV_REG(i));
			pr_info("core3 S2 real reg\n");
			for (i = AMDV_CORE3_S2_REG_START;
				i <= AMDV_CORE3_S2_REG_START + 67; i++)
				pr_info("[0x%4x] = 0x%x\n",
					i, READ_VPP_DV_REG(i));
			pr_info("core3 S3 swap\n");
			for (i = AMDV_CORE3_S3_CLKGATE_CTRL;
				i <= AMDV_CORE3_S3_OUTPUT_CSC_CRC; i++)
				pr_info("[0x%4x] = 0x%x\n",
					i, READ_VPP_DV_REG(i));
			pr_info("core3 S3 real reg\n");
			for (i = AMDV_CORE3_S3_REG_START;
				i <= AMDV_CORE3_S3_REG_START + 67; i++)
				pr_info("[0x%4x] = 0x%x\n",
					i, READ_VPP_DV_REG(i));
			pr_info("core3 meta ctrl\n");
			for (i = SLICE0_META_CTRL0;
				i <= SLICE0_META_CTRL0 + 12; i++)
				pr_info("[0x%4x] = 0x%x\n",
					i, READ_VPP_DV_REG(i));
		}
	}

	if ((debug_flag & 0x40) && dump_enable_f(0) &&
	    dolby_vision_mode <= AMDV_OUTPUT_MODE_IPT_TUNNEL) {
		pr_info("\ncore3_meta %d\n", m_setting->md_reg3.size);
		p = m_setting->md_reg3.raw_metadata;
		for (i = 0; i < m_setting->md_reg3.size; i++)
			pr_info("%08x\n", p[i]);
		pr_info("\n");
	}
}

void amdv_dump_setting(int debug_flag)
{
	pr_dv_dbg("\n====== setting for frame %d ======\n", frame_count);
	if (is_aml_tvmode()) {
		dump_tv_setting(tv_dovi_setting,
			frame_count, debug_flag);
	} else {
		if (multi_dv_mode)
			dump_m_setting(&new_m_dovi_setting, dv_inst[0].frame_count,
						   debug_flag, vd1_inst_id);
		else
			dump_setting(&new_dovi_setting, frame_count, debug_flag);
	}
	if (multi_dv_mode)
		pr_dv_dbg("=== setting for frame %d dumped ===\n\n", dv_inst[0].frame_count);
	else
		pr_dv_dbg("=== setting for frame %d dumped ===\n\n", frame_count);
}
EXPORT_SYMBOL(amdv_dump_setting);

static int sink_support_dv(const struct vinfo_s *vinfo)
{
	if (dolby_vision_flags & FLAG_DISABLE_DV_OUT)
		return 0;
	return (sink_hdr_support(vinfo) & DV_SUPPORT) >> DV_SUPPORT_SHF;
}

static int sink_support_hdr(const struct vinfo_s *vinfo)
{
	return sink_hdr_support(vinfo) & HDR_SUPPORT;
}

static int sink_support_hdr10_plus(const struct vinfo_s *vinfo)
{
	return sink_hdr_support(vinfo) & HDRP_SUPPORT;
}

static int current_hdr_cap = -1; /* should set when probe */
static int current_sink_available;

static int is_policy_changed(void)
{
	int ret = 0;

	if (last_dolby_vision_policy != dolby_vision_policy) {
		/* handle policy change */
		pr_dv_dbg("policy changed 0x%x->0x%x\n",
			last_dolby_vision_policy,
			dolby_vision_policy);
		last_dolby_vision_policy = dolby_vision_policy;
		ret |= 1;
	}
	if (last_dolby_vision_ll_policy != dolby_vision_ll_policy) {
		/* handle ll policy change when dolby on */
		if (dolby_vision_on) {
			pr_dv_dbg("ll policy changed 0x%x->0x%x\n",
				     last_dolby_vision_ll_policy,
				     dolby_vision_ll_policy);
			last_dolby_vision_ll_policy = dolby_vision_ll_policy;
			ret |= 2;
		}
	}
	if (last_dolby_vision_hdr10_policy != dolby_vision_hdr10_policy) {
		/* handle policy change */
		pr_dv_dbg("hdr10 policy changed 0x%x->0x%x\n",
			last_dolby_vision_hdr10_policy,
			dolby_vision_hdr10_policy);
		last_dolby_vision_hdr10_policy = dolby_vision_hdr10_policy;
		ret |= 4;
	}
	return ret;
}

#define signal_cuva ((vf->signal_type >> 31) & 1)
#define signal_color_primaries ((vf->signal_type >> 16) & 0xff)
#define signal_transfer_characteristic ((vf->signal_type >> 8) & 0xff)

static bool vf_is_hlg(struct vframe_s *vf)
{
	if ((signal_transfer_characteristic == 14 ||
	     signal_transfer_characteristic == 18) &&
	    signal_color_primaries == 9 && !signal_cuva)
		return true;
	return false;
}

static bool is_hlg_frame(struct vframe_s *vf)
{
	if (!vf)
		return false;
	/* report hlg in these cases: */
	/* 1: tv*/
	/* 2. stb v2.6*/
	/* 3. stb v2.4 when hlg not processed by dv, why?*/
	if ((is_aml_tvmode() || multi_dv_mode ||
		(get_amdv_hdr_policy() & 2) == 0) &&
		(signal_transfer_characteristic == 14 ||
		 signal_transfer_characteristic == 18) &&
		signal_color_primaries == 9 && !signal_cuva)
		return true;

	return false;
}

static bool vf_is_hdr10_plus(struct vframe_s *vf)
{
	if (signal_transfer_characteristic == 0x30 &&
	    (signal_color_primaries == 9 ||
	     signal_color_primaries == 2))
		return true;
	return false;
}

static bool is_hdr10plus_frame(struct vframe_s *vf)
{
	const struct vinfo_s *vinfo = get_current_vinfo();

	if (!vf)
		return false;
	if (!(dolby_vision_hdr10_policy & HDRP_BY_DV)) {
		/* report hdr10 for the content hdr10+ and
		 * sink is hdr10+ case
		 */
		if (signal_transfer_characteristic == 0x30 &&
		    (is_aml_tvmode() || sink_support_hdr10_plus(vinfo)) &&
		    (signal_color_primaries == 9 ||
		     signal_color_primaries == 2))
			return true;
	}
	return false;
}

static bool vf_is_hdr10(struct vframe_s *vf)
{
	if (signal_transfer_characteristic == 16 &&
	    (signal_color_primaries == 9 ||
	     signal_color_primaries == 2) && !signal_cuva)
		return true;
	return false;
}

static bool is_hdr10_frame(struct vframe_s *vf)
{
	const struct vinfo_s *vinfo = get_current_vinfo();

	if (!vf)
		return false;
	if ((signal_transfer_characteristic == 16 ||
		/* report as hdr10 for the content hdr10+ and
		 * sink not support hdr10+ or use DV to handle
		 * hdr10+ as hdr10
		 */
		(signal_transfer_characteristic == 0x30 &&
		((!sink_support_hdr10_plus(vinfo) && !is_aml_tvmode()) ||
		(dolby_vision_hdr10_policy & HDRP_BY_DV)))) &&
		(signal_color_primaries == 9 ||
		 signal_color_primaries == 2) && !signal_cuva)
		return true;
	return false;
}

static bool is_mvc_frame(struct vframe_s *vf)
{
	if (!vf)
		return false;
	if (vf->type & VIDTYPE_MVC)
		return true;
	return false;
}

static bool is_primesl_frame(struct vframe_s *vf)
{
	enum vframe_signal_fmt_e fmt;

	if (!vf)
		return false;

	fmt = get_vframe_src_fmt(vf);
	if (fmt == VFRAME_SIGNAL_FMT_HDR10PRIME)
		return true;

	return false;
}

static bool is_cuva_frame(struct vframe_s *vf)
{
	if (signal_cuva)
		return true;
	return false;
}

static const char *input_str[10] = {
	"NONE",
	"HDR",
	"HDR+",
	"DV",
	"PRIME",
	"HLG",
	"SDR",
	"MVC",
	"CUVA_HDR",
	"CUVA_HLG"
};

/*update pwm control when src changed or pic mode changed*/
/*control pwm only in case : src=dovi and gdEnable=1*/
void update_pwm_control(void)
{
	int gd_en = 0;

	if (is_aml_tvmode()) {
		if (pq_config_fake &&
		    ((struct pq_config *)pq_config_fake)
		    ->tdc.gd_config.gd_enable &&
		    amdv_src_format == 3)
			gd_en = 1;
		else
			gd_en = 0;

		pr_dv_dbg("%s: %s, src %d, gd_en %d, bl %d\n",
			     __func__, get_cur_pic_mode_name(),
			     amdv_src_format, gd_en, tv_backlight);

#ifdef CONFIG_AMLOGIC_LCD
		if (!force_disable_dv_backlight) {
			aml_lcd_atomic_notifier_call_chain
			(LCD_EVENT_BACKLIGHT_GD_SEL, &gd_en);
			dv_control_backlight_status = gd_en > 0 ? true : false;
			if (gd_en)
				tv_backlight_force_update = true;
		}
#endif
	}
}

static void update_src_format_v1(enum signal_format_enum src_format, struct vframe_s *vf)
{
	enum signal_format_enum cur_format = amdv_src_format;

	if (src_format == FORMAT_DOVI ||
		src_format == FORMAT_DOVI_LL) {
		amdv_src_format = 3;
	} else {
		if (vf) {
			if (is_cuva_frame(vf)) {
				if ((signal_transfer_characteristic == 14 ||
				     signal_transfer_characteristic == 18) &&
				    signal_color_primaries == 9)
					amdv_src_format = 9;
				else if (signal_transfer_characteristic == 16)
					amdv_src_format = 8;
			} else if (is_primesl_frame(vf)) {
				/* need check prime_sl before hdr and sdr */
				amdv_src_format = 4;
			} else if (vf_is_hdr10_plus(vf)) {
				amdv_src_format = 2;
			} else if (vf_is_hdr10(vf)) {
				amdv_src_format = 1;
			} else if (vf_is_hlg(vf)) {
				amdv_src_format = 5;
			} else if (is_mvc_frame(vf)) {
				amdv_src_format = 7;
			} else {
				amdv_src_format = 6;
			}
		}
	}
	if (cur_format != amdv_src_format) {
		update_pwm_control();
		pr_dv_dbg
		("update src fmt: %s => %s, signal_type 0x%x, src fmt %d\n",
		input_str[cur_format],
		input_str[amdv_src_format],
		vf ? vf->signal_type : 0,
		src_format);
		cur_format = amdv_src_format;
	}
}

static void update_src_format_v2(enum signal_format_enum src_format, struct vframe_s *vf)
{
	enum signal_format_enum cur_format;
	int dv_id = 0;

	if (vf)
		dv_id = vf->src_fmt.dv_id;

	if (!dv_inst_valid(dv_id)) {
		pr_err("[%s]err inst %d\n", __func__, dv_id);
		dv_id = 0;
	}
	cur_format = dv_inst[dv_id].amdv_src_format;

	if (src_format == FORMAT_DOVI ||
		src_format == FORMAT_DOVI_LL) {
		dv_inst[dv_id].amdv_src_format = 3;
	} else {
		if (vf) {
			if (is_cuva_frame(vf)) {
				if ((signal_transfer_characteristic == 14 ||
				     signal_transfer_characteristic == 18) &&
				    signal_color_primaries == 9)
					dv_inst[dv_id].amdv_src_format = 9;
				else if (signal_transfer_characteristic == 16)
					dv_inst[dv_id].amdv_src_format = 8;
			} else if (is_primesl_frame(vf)) {
				/* need check prime_sl before hdr and sdr */
				dv_inst[dv_id].amdv_src_format = 4;
			} else if (vf_is_hdr10_plus(vf)) {
				dv_inst[dv_id].amdv_src_format = 2;
			} else if (vf_is_hdr10(vf)) {
				dv_inst[dv_id].amdv_src_format = 1;
			} else if (vf_is_hlg(vf)) {
				dv_inst[dv_id].amdv_src_format = 5;
			} else if (is_mvc_frame(vf)) {
				dv_inst[dv_id].amdv_src_format = 7;
			} else {
				dv_inst[dv_id].amdv_src_format = 6;
			}
		}
	}
	if (cur_format != dv_inst[dv_id].amdv_src_format) {
		update_pwm_control();
		pr_dv_dbg
		("[inst%d]update src fmt: %s=>%s, signal_type 0x%x, src fmt %d\n",
		dv_id + 1,
		input_str[cur_format],
		input_str[dv_inst[dv_id].amdv_src_format],
		vf ? vf->signal_type : 0,
		src_format);
		cur_format = dv_inst[dv_id].amdv_src_format;
	}
}

static void update_src_format(enum signal_format_enum src_format, struct vframe_s *vf)
{
	if (multi_dv_mode)
		update_src_format_v2(src_format, vf);
	else
		update_src_format_v1(src_format, vf);
}

int layer_id_to_dv_id(enum vd_path_e vd_path)
{
	int i;
	int dv_id = -1;

	if (multi_dv_mode) {
		for (i = 0; i < NUM_INST; i++) {
			if (dv_inst[i].layer_id == vd_path) {
				dv_id = i;
				break;
			}
		}
		if (debug_dolby & 0x1000)
			pr_dv_dbg("vd%d <=> inst%d\n", vd_path + 1, dv_id + 1);

		if (dv_id >= 0)
			return dv_id;

		if (debug_dolby & 0x1000)
			pr_dv_dbg("vd%d not found dv_id, please check\n", vd_path + 1);
		return dv_id;

		/*If no dv inst display on vd_path, */
		/*Return inst1 if inst0 is being used and inst1 is free*/
		//if (dv_inst[0].layer_id != VD_PATH_MAX && dv_inst[1].layer_id == VD_PATH_MAX)
		//	return 1;
	}
	return 0;
}

int get_amdv_src_format(enum vd_path_e vd_path)
{
	int dv_id = 0;

	if (multi_dv_mode) {
		dv_id = layer_id_to_dv_id(vd_path);
		if (vd_path >= VD_PATH_MAX || !dv_inst_valid(dv_id))
			return UNKNOWN_SOURCE;

		if (!enable_multi_core1 && vd_path > VD1_PATH)
			return UNKNOWN_SOURCE;
		else
			return dv_inst[dv_id].amdv_src_format;
	} else {
		return amdv_src_format;
	}
}
EXPORT_SYMBOL(get_amdv_src_format);

static enum signal_format_enum get_cur_src_format(void)
{
	int cur_format = amdv_src_format;
	enum signal_format_enum ret = FORMAT_SDR;
	int dv_id = pri_input; /*pri input as default*/

	if (multi_dv_mode)
		cur_format = dv_inst[dv_id].amdv_src_format;

	switch (cur_format) {
	case 1: /* HDR10 */
		ret = FORMAT_HDR10;
		break;
	case 2: /* HDR10+ */
		ret = FORMAT_HDR10PLUS;
		break;
	case 3: /* DOVI */
		ret = FORMAT_DOVI;
		break;
	case 4: /* PRIMESL */
		ret = FORMAT_PRIMESL;
		break;
	case 5: /* HLG */
		ret = FORMAT_HLG;
		break;
	case 6: /* SDR */
		ret = FORMAT_SDR;
		break;
	case 7: /* MVC */
		ret = FORMAT_MVC;
		break;
	case 8: /* CUVA_HDR */
	case 9: /* CUVA_HLG */
		ret = FORMAT_CUVA;
		break;
	default:
		break;
	}
	return ret;
}

static bool is_dual_layer_dv(struct vframe_s *vf)
{
	if (!dv_dual_layer)
		return false;
	if (vf) {
		pr_dv_dbg("vf ext_signal_type is=%d\n", vf->ext_signal_type);
		if ((vf->ext_signal_type & (1 << 1)) ||
		(vf->ext_signal_type & (1 << 2)))
			return true;
		else
			return false;
	}
	return false;
}

static int amdv_policy_process_v1(struct vframe_s *vf,
				int *mode, enum signal_format_enum src_format)
{
	const struct vinfo_s *vinfo;
	int mode_change = 0;
	int h = 0;
	int w = 0;

	if (is_aml_tvmode()) {
		if (dolby_vision_policy == AMDV_FORCE_OUTPUT_MODE) {
			if (*mode == AMDV_OUTPUT_MODE_BYPASS) {
				if (dolby_vision_mode !=
				AMDV_OUTPUT_MODE_BYPASS) {
					pr_dv_dbg("dovi tv output mode change %d -> %d\n",
						     dolby_vision_mode, *mode);
					mode_change = 1;
				}
			} else if (*mode == AMDV_OUTPUT_MODE_SDR8) {
				if (dolby_vision_mode !=
				AMDV_OUTPUT_MODE_SDR8) {
					pr_dv_dbg
					("amdv tv output mode change %d->%d\n",
					 dolby_vision_mode, *mode);
					mode_change = 1;
				}
			} else {
				pr_dv_error
					("not support amdv output mode %d\n",
					 *mode);
				return mode_change;
			}
		} else if (dolby_vision_policy == AMDV_FOLLOW_SINK) {
			/* bypass dv_mode with efuse */
			if (efuse_mode == 1 && !amdv_efuse_bypass) {
				if (dolby_vision_mode !=
				AMDV_OUTPUT_MODE_BYPASS) {
					*mode = AMDV_OUTPUT_MODE_BYPASS;
					mode_change = 1;
				} else {
					mode_change = 0;
				}
				return mode_change;
			}

			if (cur_csc_type[VD1_PATH] != 0xffff &&
			    (get_hdr_module_status(VD1_PATH, VPP_TOP0)
			     == HDR_MODULE_ON)) {
				if (dolby_vision_mode !=
				AMDV_OUTPUT_MODE_BYPASS) {
					pr_dv_dbg("src=%d, hdr module=ON, dovi tv output -> BYPASS\n",
						src_format);
					*mode = AMDV_OUTPUT_MODE_BYPASS;
					mode_change = 1;
				} else {
					if (debug_dolby & 1)
						pr_dv_dbg("src=%d, but hdr ON, dv keep BYPASS\n",
							     src_format);
				}
			} else if ((src_format == FORMAT_DOVI) ||
				(src_format == FORMAT_DOVI_LL) ||
				((src_format == FORMAT_HDR10) &&
				(dolby_vision_hdr10_policy &
				 HDR_BY_DV_F_SINK)) ||
				((src_format == FORMAT_HLG) &&
				(dolby_vision_hdr10_policy &
				 HLG_BY_DV_F_SINK))) {
				if (dolby_vision_mode !=
				    AMDV_OUTPUT_MODE_SDR8) {
					pr_dv_dbg("src=%d, dovi tv output -> SDR8\n",
						src_format);
					*mode = AMDV_OUTPUT_MODE_SDR8;
					mode_change = 1;
				}
			} else {
				if (dolby_vision_mode !=
				    AMDV_OUTPUT_MODE_BYPASS) {
					pr_dv_dbg("src=%d, dovi tv output -> BYPASS\n",
						src_format);
					*mode = AMDV_OUTPUT_MODE_BYPASS;
					mode_change = 1;
				}
			}
		} else if (dolby_vision_policy == AMDV_FOLLOW_SOURCE) {
			/* bypass dv_mode with efuse */
			if (efuse_mode == 1 && !amdv_efuse_bypass) {
				if (dolby_vision_mode !=
				    AMDV_OUTPUT_MODE_BYPASS) {
					*mode = AMDV_OUTPUT_MODE_BYPASS;
					mode_change = 1;
				} else {
					mode_change = 0;
				}
				return mode_change;
			}
			if (cur_csc_type[VD1_PATH] != 0xffff &&
			    (get_hdr_module_status(VD1_PATH, VPP_TOP0)
			     == HDR_MODULE_ON)) {
				if (dolby_vision_mode !=
				AMDV_OUTPUT_MODE_BYPASS) {
					pr_dv_dbg("src=%d, hdr module=ON, dovi tv output -> BYPASS\n",
						src_format);
					*mode = AMDV_OUTPUT_MODE_BYPASS;
					mode_change = 1;
				}
			} else if ((src_format == FORMAT_DOVI) ||
				(src_format == FORMAT_DOVI_LL) ||
				((src_format == FORMAT_HDR10) &&
				(dolby_vision_hdr10_policy &
				 HDR_BY_DV_F_SRC)) ||
				((src_format == FORMAT_HLG) &&
				(dolby_vision_hdr10_policy &
				 HLG_BY_DV_F_SRC))) {
				if (dolby_vision_mode !=
				AMDV_OUTPUT_MODE_SDR8) {
					pr_dv_dbg("src=%d, dovi tv output -> SDR8\n",
						src_format);
					*mode = AMDV_OUTPUT_MODE_SDR8;
					mode_change = 1;
				}
			} else {
				if (dolby_vision_mode !=
				AMDV_OUTPUT_MODE_BYPASS) {
					pr_dv_dbg("src=%d, dovi tv output -> BYPASS\n",
						src_format);
					*mode = AMDV_OUTPUT_MODE_BYPASS;
					mode_change = 1;
				}
			}
		}
		return mode_change;
	}

	if (vf) {
		h = (vf->type & VIDTYPE_COMPRESS) ?
			vf->compHeight : vf->height;
		w = (vf->type & VIDTYPE_COMPRESS) ?
			vf->compWidth : vf->width;
		if ((w > 3840 && h > 2160))  {
			if (dolby_vision_mode !=
				AMDV_OUTPUT_MODE_BYPASS) {
				if (debug_dolby & 2)
					pr_dv_dbg("src size: %dx%d, dovi output -> BYPASS\n",
						w, h);
				*mode = AMDV_OUTPUT_MODE_BYPASS;
				mode_change = 1;
			} else {
				mode_change = 0;
		}
		return mode_change;
		}
	}

	vinfo = get_current_vinfo();
	if (src_format == FORMAT_MVC) {
		if (dolby_vision_mode !=
			AMDV_OUTPUT_MODE_BYPASS) {
			if (debug_dolby & 2)
				pr_dv_dbg
					("mvc, dovi output -> BYPASS\n");
			*mode = AMDV_OUTPUT_MODE_BYPASS;
			mode_change = 1;
		} else {
			mode_change = 0;
		}
		return mode_change;
	}
	if (src_format == FORMAT_PRIMESL) {
		if (dolby_vision_mode !=
			AMDV_OUTPUT_MODE_BYPASS) {
			if (debug_dolby & 2)
				pr_dv_dbg
					("prime_sl, dovi output -> BYPASS\n");
			*mode = AMDV_OUTPUT_MODE_BYPASS;
			mode_change = 1;
		} else {
			mode_change = 0;
		}
		return mode_change;
	}
	if (src_format == FORMAT_CUVA) {
		if (dolby_vision_mode !=
			AMDV_OUTPUT_MODE_BYPASS) {
			if (debug_dolby & 2)
				pr_dv_dbg
					("cuva, dovi output -> BYPASS\n");
			*mode = AMDV_OUTPUT_MODE_BYPASS;
			mode_change = 1;
		} else {
			mode_change = 0;
		}
		return mode_change;
	}
	if (dolby_vision_policy == AMDV_FOLLOW_SINK) {
		/* bypass dv_mode with efuse */
		if (efuse_mode == 1 && !amdv_efuse_bypass)  {
			if (dolby_vision_mode !=
			    AMDV_OUTPUT_MODE_BYPASS) {
				*mode = AMDV_OUTPUT_MODE_BYPASS;
				mode_change = 1;
			} else {
				mode_change = 0;
			}
			return mode_change;
		}
		if (src_format == FORMAT_HLG ||
		    (src_format == FORMAT_HDR10PLUS &&
		    !(dolby_vision_hdr10_policy & HDRP_BY_DV))) {
			if (dolby_vision_mode !=
				AMDV_OUTPUT_MODE_BYPASS) {
				if (debug_dolby & 2)
					pr_dv_dbg("hlg/hdr+, dovi output -> BYPASS\n");
				*mode = AMDV_OUTPUT_MODE_BYPASS;
				mode_change = 1;
			}
		} else if (cur_csc_type[VD1_PATH] != 0xffff &&
			(get_hdr_module_status(VD1_PATH, VPP_TOP0) == HDR_MODULE_ON)) {
			/*if vpp is playing hlg/hdr10+*/
			/*dolby need bypass at this time*/
			if (dolby_vision_mode !=
				AMDV_OUTPUT_MODE_BYPASS) {
				if (debug_dolby & 2)
					pr_dv_dbg
				("src=%d, hdr module on, dovi output->BYPASS\n",
					 src_format);
				*mode = AMDV_OUTPUT_MODE_BYPASS;
				mode_change = 1;
			}
			return mode_change;
		} else if (is_aml_stb_hdmimode() &&
			(src_format == FORMAT_DOVI) &&
			sink_support_dv(vinfo)) {
			/* HDMI DV sink-led in and TV support dv */
			if (dolby_vision_ll_policy !=
			DOLBY_VISION_LL_DISABLE &&
			!amdv_core1_on)
				pr_dv_dbg("hdmi in sink-led but output is source-led!\n");
			if (dolby_vision_mode !=
			AMDV_OUTPUT_MODE_IPT_TUNNEL) {
				pr_dv_dbg("hdmi dovi, dovi output -> AMDV_OUTPUT_MODE_IPT_TUNNEL\n");
				*mode =	AMDV_OUTPUT_MODE_IPT_TUNNEL;
				mode_change = 1;
			}
		} else if (vinfo && sink_support_dv(vinfo)) {
			/* TV support DOVI, All -> DOVI */
			if (dolby_vision_mode !=
				AMDV_OUTPUT_MODE_IPT_TUNNEL) {
				pr_dv_dbg("src=%d, dovi output -> AMDV_OUTPUT_MODE_IPT_TUNNEL\n",
					src_format);
				*mode = AMDV_OUTPUT_MODE_IPT_TUNNEL;
				mode_change = 1;
			}
		} else if (vinfo && sink_support_hdr(vinfo)) {
			/* TV support HDR, All -> HDR */
			if (dolby_vision_mode !=
				AMDV_OUTPUT_MODE_HDR10) {
				pr_dv_dbg("src=%d, dovi output -> AMDV_OUTPUT_MODE_HDR10\n",
					src_format);
				*mode = AMDV_OUTPUT_MODE_HDR10;
				mode_change = 1;
			}
		} else {
			/* TV not support DOVI and HDR */
			if (src_format == FORMAT_DOVI ||
			    src_format == FORMAT_DOVI_LL) {
				/* DOVI to SDR */
				if (dolby_vision_mode !=
				AMDV_OUTPUT_MODE_SDR8) {
					pr_dv_dbg("dovi, dovi output -> AMDV_OUTPUT_MODE_SDR8\n");
					*mode = AMDV_OUTPUT_MODE_SDR8;
					mode_change = 1;
				}
			} else if (src_format == FORMAT_HDR10) {
				if (dolby_vision_hdr10_policy
					& HDR_BY_DV_F_SINK) {
					if (dolby_vision_mode !=
					AMDV_OUTPUT_MODE_SDR8) {
						/* HDR10 to SDR */
						pr_dv_dbg("hdr, dovi output -> AMDV_OUTPUT_MODE_SDR8\n");
						*mode =
						AMDV_OUTPUT_MODE_SDR8;
						mode_change = 1;
					}
				} else if (dolby_vision_mode !=
					AMDV_OUTPUT_MODE_BYPASS) {
					/* HDR bypass */
					pr_dv_dbg("hdr, dovi output -> AMDV_OUTPUT_MODE_BYPASS\n");
					*mode = AMDV_OUTPUT_MODE_BYPASS;
					mode_change = 1;
				}
			} else if (is_meson_g12b_cpu() || is_meson_g12a_cpu()) {
				/* dv cores keep on if in sdr mode */
				if (dolby_vision_mode !=
				AMDV_OUTPUT_MODE_SDR8) {
					/* SDR to SDR */
					pr_dv_dbg("sdr, dovi output -> AMDV_OUTPUT_MODE_SDR8\n");
					*mode = AMDV_OUTPUT_MODE_SDR8;
					mode_change = 1;
				}
			} else if (dolby_vision_mode !=
			AMDV_OUTPUT_MODE_BYPASS) {
				/* HDR/SDR bypass */
				pr_dv_dbg("sdr, dovi output -> AMDV_OUTPUT_MODE_BYPASS\n");
				*mode = AMDV_OUTPUT_MODE_BYPASS;
				mode_change = 1;
			}
		}
	} else if (dolby_vision_policy == AMDV_FOLLOW_SOURCE) {
		/* bypass dv_mode with efuse */
		if (efuse_mode == 1 && !amdv_efuse_bypass) {
			if (dolby_vision_mode !=
			AMDV_OUTPUT_MODE_BYPASS) {
				*mode = AMDV_OUTPUT_MODE_BYPASS;
				mode_change = 1;
			} else {
				mode_change = 0;
			}
			return mode_change;
		}
		if (cur_csc_type[VD1_PATH] != 0xffff &&
		    get_hdr_module_status(VD1_PATH, VPP_TOP0) == HDR_MODULE_ON &&
		    (!(src_format == FORMAT_DOVI ||
		    src_format == FORMAT_DOVI_LL))) {
			/* bypass dolby incase VPP is not in sdr mode */
			if (dolby_vision_mode !=
				AMDV_OUTPUT_MODE_BYPASS) {
				pr_dv_dbg("hdr module on, dovi output -> AMDV_OUTPUT_MODE_BYPASS\n");
				*mode = AMDV_OUTPUT_MODE_BYPASS;
				mode_change = 1;
			}
			return mode_change;
		} else if (is_aml_stb_hdmimode() &&
			(src_format == FORMAT_DOVI)) {
			/* HDMI DV sink-led in and TV support */
			if (sink_support_dv(vinfo)) {
				/* support dv sink-led or source-led*/
				if (dolby_vision_ll_policy !=
				DOLBY_VISION_LL_DISABLE &&
				!amdv_core1_on)
					pr_dv_dbg("hdmi in sink-led but output is source-led!\n");
				if (dolby_vision_mode !=
				AMDV_OUTPUT_MODE_IPT_TUNNEL) {
					pr_dv_dbg("hdmi dovi, dovi output -> AMDV_OUTPUT_MODE_IPT_TUNNEL\n");
					*mode =
					AMDV_OUTPUT_MODE_IPT_TUNNEL;
					mode_change = 1;
				}
			} else if (vinfo && sink_support_hdr(vinfo)) {
				/* TV support HDR, DOVI -> HDR */
				if (dolby_vision_mode !=
				AMDV_OUTPUT_MODE_HDR10) {
					pr_dv_dbg("hdmi dovi, dovi output -> AMDV_OUTPUT_MODE_HDR10\n");
					*mode = AMDV_OUTPUT_MODE_HDR10;
					mode_change = 1;
				}
			} else {
				/* TV not support DOVI and HDR, DOVI -> SDR */
				if (dolby_vision_mode !=
				AMDV_OUTPUT_MODE_SDR8) {
					pr_dv_dbg("hdmi dovi,, dovi output -> AMDV_OUTPUT_MODE_SDR8\n");
					*mode = AMDV_OUTPUT_MODE_SDR8;
					mode_change = 1;
				}
			}
		} else if ((src_format == FORMAT_DOVI) ||
			(src_format == FORMAT_DOVI_LL)) {
			/* DOVI source */
			if (vinfo && sink_support_dv(vinfo)) {
				/* TV support DOVI, DOVI -> DOVI */
				if (dolby_vision_mode !=
				AMDV_OUTPUT_MODE_IPT_TUNNEL) {
					pr_dv_dbg("dovi, dovi output -> AMDV_OUTPUT_MODE_IPT_TUNNEL\n");
					*mode =
					AMDV_OUTPUT_MODE_IPT_TUNNEL;
					mode_change = 1;
				}
			} else if (vinfo && sink_support_hdr(vinfo)) {
				/* TV support HDR, DOVI -> HDR */
				if (dolby_vision_mode !=
				AMDV_OUTPUT_MODE_HDR10) {
					pr_dv_dbg("dovi, dovi output -> AMDV_OUTPUT_MODE_HDR10\n");
					*mode = AMDV_OUTPUT_MODE_HDR10;
					mode_change = 1;
				}
			} else {
				/* TV not support DOVI and HDR, DOVI -> SDR */
				if (dolby_vision_mode !=
				AMDV_OUTPUT_MODE_SDR8) {
					pr_dv_dbg("dovi, dovi output -> AMDV_OUTPUT_MODE_SDR8\n");
					*mode = AMDV_OUTPUT_MODE_SDR8;
					mode_change = 1;
				}
			}
		} else if ((src_format == FORMAT_HDR10) &&
			(dolby_vision_hdr10_policy & HDR_BY_DV_F_SRC)) {
			if (vinfo && sink_support_hdr(vinfo)) {
				/* TV support HDR, HDR -> HDR */
				if (dolby_vision_mode !=
				AMDV_OUTPUT_MODE_HDR10) {
					pr_dv_dbg("hdr10, dovi output -> AMDV_OUTPUT_MODE_HDR10\n");
					*mode = AMDV_OUTPUT_MODE_HDR10;
					mode_change = 1;
				}
			} else {
				if (dolby_vision_mode !=
				AMDV_OUTPUT_MODE_SDR8) {
					/* HDR10 to SDR */
					pr_dv_dbg("hdr10, dovi output -> AMDV_OUTPUT_MODE_SDR8\n");
					*mode = AMDV_OUTPUT_MODE_SDR8;
					mode_change = 1;
				}
			}
		} else if (dolby_vision_mode !=
			AMDV_OUTPUT_MODE_BYPASS) {
			/* HDR/SDR bypass */
			pr_dv_dbg("sdr, dovi output -> AMDV_OUTPUT_MODE_BYPASS\n");
			*mode = AMDV_OUTPUT_MODE_BYPASS;
			mode_change = 1;
		}
	} else if (dolby_vision_policy == AMDV_FORCE_OUTPUT_MODE) {
		if (dolby_vision_mode != *mode) {
			pr_dv_dbg("src=%d, dovi output mode change %d -> %d\n",
				src_format, dolby_vision_mode, *mode);
			mode_change = 1;
		}
	}
	return mode_change;
}

/*multi-inst tv mode, todo*/
static int amdv_policy_process_v2_tv(int *mode,
				enum signal_format_enum src_format)
{
	return 1;
}

static int amdv_policy_process_v2_stb(struct vframe_s *vf,
				int *mode, enum signal_format_enum src_format)
{
	const struct vinfo_s *vinfo;
	int mode_change = 0;
	int h = 0;
	int w = 0;

	if (vf) {
		h = (vf->type & VIDTYPE_COMPRESS) ?
			vf->compHeight : vf->height;
		w = (vf->type & VIDTYPE_COMPRESS) ?
			vf->compWidth : vf->width;
		if ((w > 3840 && h > 2160))  {
			if (dolby_vision_mode !=
				AMDV_OUTPUT_MODE_BYPASS) {
				if (debug_dolby & 2)
					pr_dv_dbg("src size: %dx%d, dovi output -> BYPASS\n",
						w, h);
				*mode = AMDV_OUTPUT_MODE_BYPASS;
				mode_change = 1;
			} else {
				mode_change = 0;
		}
		return mode_change;
		}
	}

	vinfo = get_current_vinfo();
	if (src_format == FORMAT_MVC) {
		if (dolby_vision_mode !=
			AMDV_OUTPUT_MODE_BYPASS) {
			if (debug_dolby & 2)
				pr_dv_dbg
					("mvc, dovi output -> BYPASS\n");
			*mode = AMDV_OUTPUT_MODE_BYPASS;
			mode_change = 1;
		} else {
			mode_change = 0;
		}
		return mode_change;
	}
	if (src_format == FORMAT_PRIMESL) {
		if (dolby_vision_mode !=
			AMDV_OUTPUT_MODE_BYPASS) {
			if (debug_dolby & 2)
				pr_dv_dbg
					("prime_sl, dovi output -> BYPASS\n");
			*mode = AMDV_OUTPUT_MODE_BYPASS;
			mode_change = 1;
		} else {
			mode_change = 0;
		}
		return mode_change;
	}
	if (src_format == FORMAT_CUVA) {
		if (dolby_vision_mode !=
			AMDV_OUTPUT_MODE_BYPASS) {
			if (debug_dolby & 2)
				pr_dv_dbg
					("cuva, dovi output -> BYPASS\n");
			*mode = AMDV_OUTPUT_MODE_BYPASS;
			mode_change = 1;
		} else {
			mode_change = 0;
		}
		return mode_change;
	}
	if (dolby_vision_policy == AMDV_FOLLOW_SINK) {
		/* bypass dv_mode with efuse */
		if (efuse_mode == 1 && !amdv_efuse_bypass)  {
			if (dolby_vision_mode !=
			    AMDV_OUTPUT_MODE_BYPASS) {
				*mode = AMDV_OUTPUT_MODE_BYPASS;
				mode_change = 1;
			} else {
				mode_change = 0;
			}
			return mode_change;
		}
		if ((src_format == FORMAT_HLG &&
		    !(dolby_vision_hdr10_policy & HLG_BY_DV_F_SINK)) ||
		    (src_format == FORMAT_HDR10PLUS &&
		    !(dolby_vision_hdr10_policy & HDRP_BY_DV))) {
			if (dolby_vision_mode !=
				AMDV_OUTPUT_MODE_BYPASS) {
				if (debug_dolby & 2)
					pr_dv_dbg("hlg/hdr+, dovi output -> BYPASS\n");
				*mode = AMDV_OUTPUT_MODE_BYPASS;
				mode_change = 1;
			}
		} else if (cur_csc_type[VD1_PATH] != 0xffff &&
			   (get_hdr_module_status(VD1_PATH, VPP_TOP0)
			   == HDR_MODULE_ON)) {
			/*if vpp is playing hlg/hdr10+*/
			/*dolby need bypass at this time*/
			if (dolby_vision_mode !=
				AMDV_OUTPUT_MODE_BYPASS) {
				if (debug_dolby & 2)
					pr_dv_dbg
				("src=%d, hdr module on, dovi output->BYPASS\n",
					 src_format);
				*mode = AMDV_OUTPUT_MODE_BYPASS;
				mode_change = 1;
			} else {
				if (debug_dolby & 1)
					pr_dv_dbg("src=%d, but hdr ON, dv keep BYPASS\n",
						     src_format);
			}
			return mode_change;
		} else if (is_aml_stb_hdmimode() &&
			(src_format == FORMAT_DOVI) &&
			sink_support_dv(vinfo)) {
			/* HDMI DV sink-led in and TV support dv */
			if (dolby_vision_ll_policy !=
			    DOLBY_VISION_LL_DISABLE &&
			    !dv_core1[0].core1_on &&
			    !dv_core1[1].core1_on)
				pr_dv_dbg("hdmi in sink-led but output is source-led!\n");
			if (dolby_vision_mode !=
			AMDV_OUTPUT_MODE_IPT_TUNNEL) {
				pr_dv_dbg("hdmi dovi, dovi output -> AMDV_OUTPUT_MODE_IPT_TUNNEL\n");
				*mode =	AMDV_OUTPUT_MODE_IPT_TUNNEL;
				mode_change = 1;
			}
		} else if (vinfo && sink_support_dv(vinfo)) {
			/* TV support DOVI, All -> DOVI */
			if (dolby_vision_mode !=
				AMDV_OUTPUT_MODE_IPT_TUNNEL) {
				pr_dv_dbg("src=%d, dovi output -> AMDV_OUTPUT_MODE_IPT_TUNNEL\n",
					src_format);
				*mode = AMDV_OUTPUT_MODE_IPT_TUNNEL;
				mode_change = 1;
			}
		} else if (vinfo && sink_support_hdr(vinfo)) {
			/* TV support HDR, All -> HDR */
			if (dolby_vision_mode !=
				AMDV_OUTPUT_MODE_HDR10) {
				pr_dv_dbg("%d->AMDV_OUTPUT_MODE_HDR10, cap=%x\n",
					src_format, sink_hdr_support(vinfo));
				*mode = AMDV_OUTPUT_MODE_HDR10;
				mode_change = 1;
			}
		} else {
			/* TV not support DOVI and HDR */
			if (src_format == FORMAT_DOVI ||
			    src_format == FORMAT_DOVI_LL) {
				/* DOVI to SDR */
				if (dolby_vision_mode !=
				AMDV_OUTPUT_MODE_SDR8) {
					pr_dv_dbg("dovi->AMDV_OUTPUT_MODE_SDR8, cap=%x\n",
							  sink_hdr_support(vinfo));
					*mode = AMDV_OUTPUT_MODE_SDR8;
					mode_change = 1;
				}
			} else if (src_format == FORMAT_HDR10) {
				if (dolby_vision_hdr10_policy
					& HDR_BY_DV_F_SINK) {
					if (dolby_vision_mode !=
					AMDV_OUTPUT_MODE_SDR8) {
						/* HDR10 to SDR */
						pr_dv_dbg("hdr->AMDV_OUTPUT_MODE_SDR8, cap=%x\n",
								  sink_hdr_support(vinfo));
						*mode =
						AMDV_OUTPUT_MODE_SDR8;
						mode_change = 1;
					}
				} else if (dolby_vision_mode !=
					AMDV_OUTPUT_MODE_BYPASS) {
					/* HDR bypass */
					pr_dv_dbg("hdr->AMDV_OUTPUT_MODE_BYPASS, cap=%x\n",
							  sink_hdr_support(vinfo));
					*mode = AMDV_OUTPUT_MODE_BYPASS;
					mode_change = 1;
				}
			} else if (src_format == FORMAT_HLG) {
				if (dolby_vision_hdr10_policy
					& HLG_BY_DV_F_SINK) {
					if (dolby_vision_mode !=
					AMDV_OUTPUT_MODE_SDR8) {
						/* HLG to SDR */
						pr_dv_dbg("hlg->AMDV_OUTPUT_MODE_SDR8, cap=%x\n",
								  sink_hdr_support(vinfo));
						*mode =
						AMDV_OUTPUT_MODE_SDR8;
						mode_change = 1;
					}
				} else if (dolby_vision_mode !=
					AMDV_OUTPUT_MODE_BYPASS) {
					/* HLG bypass */
					pr_dv_dbg("hlg->AMDV_OUTPUT_MODE_BYPASS, cap=%x\n",
							  sink_hdr_support(vinfo));
					*mode = AMDV_OUTPUT_MODE_BYPASS;
					mode_change = 1;
				}
			} else if (is_meson_g12b_cpu() || is_meson_g12a_cpu()
			/* || is_aml_tm2_stbmode() */) {
				/* dv cores keep on if in sdr mode */
				if (dolby_vision_mode !=
				AMDV_OUTPUT_MODE_SDR8) {
					/* SDR to SDR */
					pr_dv_dbg("sdr->AMDV_OUTPUT_MODE_SDR8, cap=%x\n",
							  sink_hdr_support(vinfo));
					*mode = AMDV_OUTPUT_MODE_SDR8;
					mode_change = 1;
				}
			} else if (dolby_vision_mode !=
			AMDV_OUTPUT_MODE_BYPASS) {
				/* HDR/SDR bypass */
				pr_dv_dbg("sdr->AMDV_OUTPUT_MODE_BYPASS, cap=%x\n",
						  sink_hdr_support(vinfo));
				*mode = AMDV_OUTPUT_MODE_BYPASS;
				mode_change = 1;
			}
		}
	} else if (dolby_vision_policy == AMDV_FOLLOW_SOURCE) {
		/* bypass dv_mode with efuse */
		if (efuse_mode == 1 && !amdv_efuse_bypass) {
			if (dolby_vision_mode !=
			    AMDV_OUTPUT_MODE_BYPASS) {
				*mode = AMDV_OUTPUT_MODE_BYPASS;
				mode_change = 1;
			} else {
				mode_change = 0;
			}
			return mode_change;
		}
		if (cur_csc_type[VD1_PATH] != 0xffff &&
		    get_hdr_module_status(VD1_PATH, VPP_TOP0) == HDR_MODULE_ON &&
		    (!(src_format == FORMAT_DOVI ||
		    src_format == FORMAT_DOVI_LL))) {
			/* bypass dolby incase VPP is not in sdr mode */
			if (dolby_vision_mode !=
				AMDV_OUTPUT_MODE_BYPASS) {
				pr_dv_dbg("hdr module on, AMDV_OUTPUT_MODE_BYPASS\n");
				*mode = AMDV_OUTPUT_MODE_BYPASS;
				mode_change = 1;
			}
			return mode_change;
		} else if (is_aml_stb_hdmimode() &&
			   (src_format == FORMAT_DOVI)) {
			/* HDMI DV sink-led in and TV support */
			if (sink_support_dv(vinfo)) {
				/* support dv sink-led or source-led*/
				if (dolby_vision_ll_policy !=
				    DOLBY_VISION_LL_DISABLE &&
				    !dv_core1[0].core1_on &&
				    !dv_core1[1].core1_on)
					pr_dv_dbg("hdmi in sink-led but output is source-led!\n");
				if (dolby_vision_mode !=
				AMDV_OUTPUT_MODE_IPT_TUNNEL) {
					pr_dv_dbg("hdmi dovi->AMDV_OUTPUT_MODE_IPT_TUNNEL\n");
					*mode =
					AMDV_OUTPUT_MODE_IPT_TUNNEL;
					mode_change = 1;
				}
			} else if (vinfo && sink_support_hdr(vinfo)) {
				/* TV support HDR, DOVI -> HDR */
				if (dolby_vision_mode !=
				AMDV_OUTPUT_MODE_HDR10) {
					pr_dv_dbg("hdmi dovi->AMDV_OUTPUT_MODE_HDR10\n");
					*mode = AMDV_OUTPUT_MODE_HDR10;
					mode_change = 1;
				}
			} else {
				/* TV not support DOVI and HDR, DOVI -> SDR */
				if (dolby_vision_mode !=
				AMDV_OUTPUT_MODE_SDR8) {
					pr_dv_dbg("hdmi dovi->AMDV_OUTPUT_MODE_SDR8\n");
					*mode = AMDV_OUTPUT_MODE_SDR8;
					mode_change = 1;
				}
			}
		} else if ((src_format == FORMAT_DOVI) ||
			(src_format == FORMAT_DOVI_LL)) {
			/* DOVI source */
			if (vinfo && sink_support_dv(vinfo)) {
				/* TV support DOVI, DOVI -> DOVI */
				if (dolby_vision_mode !=
				AMDV_OUTPUT_MODE_IPT_TUNNEL) {
					pr_dv_dbg("dovi->AMDV_OUTPUT_MODE_IPT_TUNNEL\n");
					*mode =
					AMDV_OUTPUT_MODE_IPT_TUNNEL;
					mode_change = 1;
				}
			} else if (vinfo && sink_support_hdr(vinfo)) {
				/* TV support HDR, DOVI -> HDR */
				if (dolby_vision_mode !=
				AMDV_OUTPUT_MODE_HDR10) {
					pr_dv_dbg("dovi->AMDV_OUTPUT_MODE_HDR10, cap=%x\n",
							  sink_hdr_support(vinfo));
					*mode = AMDV_OUTPUT_MODE_HDR10;
					mode_change = 1;
				}
			} else {
				/* TV not support DOVI and HDR, DOVI -> SDR */
				if (dolby_vision_mode !=
				AMDV_OUTPUT_MODE_SDR8) {
					pr_dv_dbg("dovi->AMDV_OUTPUT_MODE_SDR8, cap=%x\n",
							  sink_hdr_support(vinfo));
					*mode = AMDV_OUTPUT_MODE_SDR8;
					mode_change = 1;
				}
			}
		} else if ((src_format == FORMAT_HDR10) &&
			(dolby_vision_hdr10_policy & HDR_BY_DV_F_SRC)) {
			if (vinfo && sink_support_hdr(vinfo)) {
				/* TV support HDR, HDR -> HDR */
				if (dolby_vision_mode !=
				AMDV_OUTPUT_MODE_HDR10) {
					pr_dv_dbg("hdr10->AMDV_OUTPUT_MODE_HDR10\n");
					*mode = AMDV_OUTPUT_MODE_HDR10;
					mode_change = 1;
				}
			} else {
				if (dolby_vision_mode !=
				AMDV_OUTPUT_MODE_SDR8) {
					/* HDR10 to SDR */
					pr_dv_dbg("hdr10->AMDV_OUTPUT_MODE_SDR8, cap=%x\n",
							  sink_hdr_support(vinfo));
					*mode = AMDV_OUTPUT_MODE_SDR8;
					mode_change = 1;
				}
			}
		} else if ((src_format == FORMAT_HLG) &&
		(dolby_vision_hdr10_policy & HLG_BY_DV_F_SRC)) {
			if (vinfo && sink_support_hdr(vinfo)) {
				/* TV support HDR, HLG -> HDR */
				if (dolby_vision_mode !=
				AMDV_OUTPUT_MODE_HDR10) {
					pr_dv_dbg("hlg->AMDV_OUTPUT_MODE_HDR10\n");
					*mode = AMDV_OUTPUT_MODE_HDR10;
					mode_change = 1;
				}
			} else {
				if (dolby_vision_mode !=
				AMDV_OUTPUT_MODE_SDR8) {
					/* HLG to SDR */
					pr_dv_dbg("hlg->AMDV_OUTPUT_MODE_SDR8, cap=%x\n",
							  sink_hdr_support(vinfo));
					*mode = AMDV_OUTPUT_MODE_SDR8;
					mode_change = 1;
				}
			}
		} else if (is_dual_layer_dv(vf)) {
			if (vinfo && sink_support_dv(vinfo)) {
				/* TV support DOVI, DOVI -> DOVI */
				if (dolby_vision_mode !=
				AMDV_OUTPUT_MODE_IPT_TUNNEL) {
					pr_dv_dbg("dovi->AMDV_OUTPUT_MODE_IPT_TUNNEL\n");
					pr_dv_dbg("double dv ipt _tunnel process:\n");
					*mode = AMDV_OUTPUT_MODE_IPT_TUNNEL;
					mode_change = 1;
				}
			} else if (vinfo && sink_support_hdr(vinfo)) {
				/* TV support HDR, DOVI -> HDR */
				if (dolby_vision_mode !=
				AMDV_OUTPUT_MODE_HDR10) {
					pr_dv_dbg("dovi->AMDV_OUTPUT_MODE_HDR10, cap=%x\n",
							  sink_hdr_support(vinfo));
					pr_dv_dbg("double dv hdr10 process:\n");
					*mode = AMDV_OUTPUT_MODE_HDR10;
					mode_change = 1;
				}
			} else {
				/* TV not support DOVI and HDR, DOVI -> SDR */
				if (dolby_vision_mode !=
				AMDV_OUTPUT_MODE_SDR8) {
					pr_dv_dbg("dovi->AMDV_OUTPUT_MODE_SDR8, cap=%x\n",
							  sink_hdr_support(vinfo));
					pr_dv_dbg("double dv sdr8 process:\n");
					*mode = AMDV_OUTPUT_MODE_SDR8;
					mode_change = 1;
				}
			}
		} else if (dolby_vision_mode !=
			AMDV_OUTPUT_MODE_BYPASS) {
			/* HDR/SDR bypass */
			pr_dv_dbg("sdr->AMDV_OUTPUT_MODE_BYPASS\n");
			*mode = AMDV_OUTPUT_MODE_BYPASS;
			mode_change = 1;
		}
	} else if (dolby_vision_policy == AMDV_FORCE_OUTPUT_MODE) {
		if (dolby_vision_mode != *mode) {
			pr_dv_dbg("src=%d, output mode change %d -> %d\n",
				src_format, dolby_vision_mode, *mode);
			mode_change = 1;
		}
	}
	return mode_change;
}

static int amdv_policy_process(struct vframe_s *vf, int *mode,
			enum signal_format_enum src_format)
{
	int mode_change = 0;

	if (!dolby_vision_enable || (!p_funcs_stb && !p_funcs_tv))
		return mode_change;

	if (multi_dv_mode) {
		if (is_aml_tvmode())
			mode_change = amdv_policy_process_v2_tv(mode, src_format);
		else
			mode_change = amdv_policy_process_v2_stb(vf, mode, src_format);
	} else {
		mode_change = amdv_policy_process_v1(vf, mode, src_format);
	}
	return mode_change;
}

/* dv provider of each video layer*/
static char dv_provider[2][32] = {"dvbldec", "dvbldec2"};

void amdv_set_provider(char *prov_name, enum vd_path_e vd_layer)
{
	if (prov_name && strlen(prov_name) < 32) {
		if (strcmp(dv_provider[vd_layer], prov_name)) {
			strcpy(dv_provider[vd_layer], prov_name);
			pr_dv_dbg("VD%d: provider changed to %s\n",
				vd_layer + 1, dv_provider[vd_layer]);
		}
	}
}
EXPORT_SYMBOL(amdv_set_provider);

/* 0: no dv, 1: dv std, 2: dv ll */
int is_amdv_frame(struct vframe_s *vf)
{
	struct provider_aux_req_s req;
	char *p;
	unsigned int size = 0;
	unsigned int type = 0;
	enum vframe_signal_fmt_e fmt;
	int dv_id;
	int layer_id = VD1_PATH;

	if (!vf)
		return 0;

	dv_id = vf->src_fmt.dv_id;
	if (!dv_inst_valid(dv_id))
		dv_id = 0;

	if (dv_inst[dv_id].layer_id >= 0 && dv_inst[dv_id].layer_id < VD3_PATH)
		layer_id = dv_inst[dv_id].layer_id;

	fmt = get_vframe_src_fmt(vf);
	if (fmt == VFRAME_SIGNAL_FMT_DOVI)
		return true;

	if (fmt != VFRAME_SIGNAL_FMT_INVALID)
		return false;

	req.vf = vf;
	req.bot_flag = 0;
	req.aux_buf = NULL;
	req.aux_size = 0;
	req.dv_enhance_exist = 0;
	req.low_latency = 0;

	if (vf->source_type == VFRAME_SOURCE_TYPE_HDMI &&
	    (is_aml_tvmode() ||
	    ((is_aml_tm2_stbmode() || is_aml_t7_stbmode()) &&
	    hdmi_to_stb_policy))) {
		vf_notify_provider_by_name("dv_vdin",
			VFRAME_EVENT_RECEIVER_GET_AUX_DATA,
			(void *)&req);
		if (debug_dolby & 2)
			pr_dv_dbg("is_dovi: vf %p, emp.size %d, aux_size %d\n",
				     vf, vf->emp.size,
				     req.aux_size);
		if ((req.aux_buf && req.aux_size) ||
			(dolby_vision_flags & FLAG_FORCE_DV_LL))
			return 1;
		if (req.low_latency)
			return 2;
		p = vf->emp.addr;
		if (p && vf->emp.size > 0 &&
		    p[0] == 0x7f &&
		    p[10] == 0x46 &&
		    p[11] == 0xd0) {
			if (p[13] == 0)
				return 1;
			else
				return 2;
		}
		return 0;
	} else if (vf->source_type == VFRAME_SOURCE_TYPE_OTHERS) {
		if (!strcmp(dv_provider[layer_id], "dvbldec") ||
			!strcmp(dv_provider[layer_id], "dvbldec2"))
			vf_notify_provider_by_name(dv_provider[layer_id],
					   VFRAME_EVENT_RECEIVER_GET_AUX_DATA,
					   (void *)&req);
		if (req.dv_enhance_exist)
			return 1;
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
			if (type == DV_SEI ||
			    ((type & 0xffff0000) == AV1_SEI &&
			    !vf_is_hdr10_plus(vf)))
				return 1;
			p += size;
		}
	}
	return 0;
}
EXPORT_SYMBOL(is_amdv_frame);

bool is_dovi_dual_layer_frame(struct vframe_s *vf)
{
	struct provider_aux_req_s req;
	enum vframe_signal_fmt_e fmt;
	int dv_id;
	int layer_id = VD1_PATH;

	if (!vf)
		return false;

	if (!enable_fel)
		return false;

	dv_id = vf->src_fmt.dv_id;
	if (!dv_inst_valid(dv_id))
		dv_id = 0;

	if (dv_inst[dv_id].layer_id >= 0 && dv_inst[dv_id].layer_id < VD3_PATH)
		layer_id = dv_inst[dv_id].layer_id;

	fmt = get_vframe_src_fmt(vf);
	/* valid src_fmt = DOVI or invalid src_fmt will check dual layer */
	/* otherwise, it certainly is a non-dv vframe */
	if (fmt != VFRAME_SIGNAL_FMT_DOVI &&
	    fmt != VFRAME_SIGNAL_FMT_INVALID)
		return false;

	req.vf = vf;
	req.bot_flag = 0;
	req.aux_buf = NULL;
	req.aux_size = 0;
	req.dv_enhance_exist = 0;

	if (vf->source_type == VFRAME_SOURCE_TYPE_OTHERS) {
		if (!strcmp(dv_provider[layer_id], "dvbldec") ||
			!strcmp(dv_provider[layer_id], "dvbldec2"))
			vf_notify_provider_by_name(dv_provider[layer_id],
			VFRAME_EVENT_RECEIVER_GET_AUX_DATA,
			(void *)&req);
		if (req.dv_enhance_exist)
			return true;
	}
	return false;
}
EXPORT_SYMBOL(is_dovi_dual_layer_frame);

int amdv_check_mvc(struct vframe_s *vf)
{
	int mode;

	if (is_mvc_frame(vf) && dolby_vision_on) {
		/* mvc source, but dovi enabled, need bypass dv */
		mode = dolby_vision_mode;
		if (amdv_policy_process(vf, &mode, FORMAT_MVC)) {
			if (mode != AMDV_OUTPUT_MODE_BYPASS &&
			    dolby_vision_mode ==
			    AMDV_OUTPUT_MODE_BYPASS)
				amdv_wait_on = true;
			amdv_target_mode = mode;
			return 1;
		}
	}
	return 0;
}
EXPORT_SYMBOL(amdv_check_mvc);

int amdv_check_hlg(struct vframe_s *vf)
{
	int mode;

	if (!is_amdv_frame(vf) && is_hlg_frame(vf) && !dolby_vision_on) {
		/* hlg source, but dovi not enabled */
		mode = dolby_vision_mode;
		if (amdv_policy_process(vf, &mode, FORMAT_HLG)) {
			if (mode != AMDV_OUTPUT_MODE_BYPASS &&
			    dolby_vision_mode ==
			    AMDV_OUTPUT_MODE_BYPASS)
				amdv_wait_on = true;
			amdv_target_mode = mode;
			return 1;
		}
	}
	return 0;
}
EXPORT_SYMBOL(amdv_check_hlg);

int amdv_check_hdr10plus(struct vframe_s *vf)
{
	int mode;

	if (!is_amdv_frame(vf) && is_hdr10plus_frame(vf) && !dolby_vision_on) {
		/* hdr10+ source, but dovi not enabled */
		mode = dolby_vision_mode;
		if (amdv_policy_process(vf, &mode, FORMAT_HDR10PLUS)) {
			if (mode != AMDV_OUTPUT_MODE_BYPASS &&
			    dolby_vision_mode ==
			    AMDV_OUTPUT_MODE_BYPASS)
				amdv_wait_on = true;
			amdv_target_mode = mode;
			return 1;
		}
	}
	return 0;
}
EXPORT_SYMBOL(amdv_check_hdr10plus);

int amdv_check_hdr10(struct vframe_s *vf)
{
	int mode;

	if (!is_amdv_frame(vf) && is_hdr10_frame(vf) && !dolby_vision_on) {
		/* hdr10 source, but dovi not enabled */
		mode = dolby_vision_mode;
		if (amdv_policy_process(vf, &mode, FORMAT_HDR10)) {
			if (mode != AMDV_OUTPUT_MODE_BYPASS &&
			    dolby_vision_mode ==
			    AMDV_OUTPUT_MODE_BYPASS)
				amdv_wait_on = true;
			amdv_target_mode = mode;
			return 1;
		}
	}
	return 0;
}
EXPORT_SYMBOL(amdv_check_hdr10);

int amdv_check_primesl(struct vframe_s *vf)
{
	int mode;

	if (is_primesl_frame(vf) && dolby_vision_on) {
		/* primesl source, but dovi enabled, need bypass dv */
		mode = dolby_vision_mode;
		if (amdv_policy_process(vf, &mode, FORMAT_PRIMESL)) {
			if (mode != AMDV_OUTPUT_MODE_BYPASS &&
			    dolby_vision_mode ==
			    AMDV_OUTPUT_MODE_BYPASS)
				amdv_wait_on = true;
			amdv_target_mode = mode;
			return 1;
		}
	}
	return 0;
}
EXPORT_SYMBOL(amdv_check_primesl);

int amdv_check_cuva(struct vframe_s *vf)
{
	int mode;

	if (is_cuva_frame(vf) && dolby_vision_on) {
		/* cuva source, but dovi enabled, need bypass dv */
		mode = dolby_vision_mode;
		if (amdv_policy_process(vf, &mode, FORMAT_CUVA)) {
			if (mode != AMDV_OUTPUT_MODE_BYPASS &&
			    dolby_vision_mode ==
			    AMDV_OUTPUT_MODE_BYPASS)
				amdv_wait_on = true;
			amdv_target_mode = mode;
			return 1;
		}
	}
	return 0;
}
EXPORT_SYMBOL(amdv_check_cuva);

void amdv_vf_put(struct vframe_s *vf)
{
	int i;
	int dv_id = 0;

	if (multi_dv_mode && vf) {
		if (vf && dv_inst_valid(vf->src_fmt.dv_id))
			dv_id = vf->src_fmt.dv_id;
		for (i = 0; i < 16; i++) {
			if (dv_inst[dv_id].dv_vf[i][0] == vf) {
				if (dv_inst[dv_id].dv_vf[i][1]) {
					dvel_vf_put(dv_inst[dv_id].dv_vf[i][1]);
				} else if (debug_dolby & 2) {
					pr_dv_dbg("--#%d: put bl(%p-%lld) --\n",
						     dv_id, vf, vf->pts_us64);
				}
				dv_inst[dv_id].dv_vf[i][0] = NULL;
				dv_inst[dv_id].dv_vf[i][1] = NULL;
			}
		}
	} else if (!multi_dv_mode && vf) {
		for (i = 0; i < 16; i++) {
			if (dv_vf[i][0] == vf) {
				if (dv_vf[i][1]) {
					if (debug_dolby & 2)
						pr_dv_dbg("--- put bl(%p-%lld) with el(%p-%lld) ---\n",
							vf, vf->pts_us64,
							dv_vf[i][1],
							dv_vf[i][1]->pts_us64);
					dvel_vf_put(dv_vf[i][1]);
				} else if (debug_dolby & 2) {
					pr_dv_dbg("--- put bl(%p-%lld) ---\n",
						vf, vf->pts_us64);
				}
				dv_vf[i][0] = NULL;
				dv_vf[i][1] = NULL;
			}
		}
	}
}
EXPORT_SYMBOL(amdv_vf_put);

struct vframe_s *amdv_vf_peek_el(struct vframe_s *vf)
{
	int i;
	int dv_id = 0;

	if (dolby_vision_flags && (p_funcs_stb || p_funcs_tv)) {
		if (multi_dv_mode) {
			if (vf && dv_inst_valid(vf->src_fmt.dv_id))
				dv_id = vf->src_fmt.dv_id;
			for (i = 0; i < 16; i++) {
				if (dv_inst[dv_id].dv_vf[i][0] == vf) {
					if (dv_inst[dv_id].dv_vf[i][1] &&
					    dolby_vision_status == BYPASS_PROCESS &&
					    !is_amdv_on())
						dv_inst[dv_id].dv_vf[i][1]->type |= VIDTYPE_VD2;
					return dv_inst[dv_id].dv_vf[i][1];
				}
			}
		} else {
			for (i = 0; i < 16; i++) {
				if (dv_vf[i][0] == vf) {
					if (dv_vf[i][1] &&
					    dolby_vision_status == BYPASS_PROCESS &&
					    !is_amdv_on())
						dv_vf[i][1]->type |= VIDTYPE_VD2;
					return dv_vf[i][1];
				}
			}
		}
	}
	return NULL;
}
EXPORT_SYMBOL(amdv_vf_peek_el);

static void amdvdolby_vision_vf_add(struct vframe_s *vf, struct vframe_s *el_vf)
{
	int i;
	int dv_id = 0;

	if (multi_dv_mode) {
		if (vf && dv_inst_valid(vf->src_fmt.dv_id))
			dv_id = vf->src_fmt.dv_id;
		for (i = 0; i < 16; i++) {
			if (!dv_inst[dv_id].dv_vf[i][0]) {
				dv_inst[dv_id].dv_vf[i][0] = vf;
				dv_inst[dv_id].dv_vf[i][1] = el_vf;
				if (debug_dolby & 2) {
					if (el_vf && vf)
						pr_dv_dbg("--#%d: add bl(%p-%lld) with el %p --\n",
							     dv_id, vf, vf->pts_us64, el_vf);
					else if (vf)
						pr_dv_dbg("--#%d: add bl(%p-%lld --\n",
							     dv_id, vf, vf->pts_us64);
				}
				break;
			}
		}
	} else {
		for (i = 0; i < 16; i++) {
			if (!dv_vf[i][0]) {
				dv_vf[i][0] = vf;
				dv_vf[i][1] = el_vf;
				break;
			}
		}
	}
}

static int amdv_vf_check(struct vframe_s *vf)
{
	int i;
	int dv_id = 0;

	if (multi_dv_mode) {
		if (vf && dv_inst_valid(vf->src_fmt.dv_id))
			dv_id = vf->src_fmt.dv_id;
		for (i = 0; i < 16; i++) {
			if (dv_inst[dv_id].dv_vf[i][0] == vf) {
				if (debug_dolby & 2) {
					if (dv_inst[dv_id].dv_vf[i][1] && vf)
						pr_dv_dbg("== #%d: bl(%p-%lld) with el(%p-%lld) toggled ==\n",
							dv_id,
							vf,
							vf->pts_us64,
							dv_inst[dv_id].dv_vf[i][1],
							dv_inst[dv_id].dv_vf[i][1]->pts_us64);
					else if (vf)
						pr_dv_dbg("== #%d:bl(%p-%lld) toggled ==\n",
							dv_id,
							vf,
							vf->pts_us64);
				}
				return 0;
			}
		}
	} else {
		for (i = 0; i < 16; i++) {
			if (dv_vf[i][0] == vf) {
				if (debug_dolby & 2) {
					if (dv_vf[i][1] && vf)
						pr_dv_dbg("=== bl(%p-%lld) with el(%p-%lld) toggled ===\n",
							vf,
							vf->pts_us64,
							dv_vf[i][1],
							dv_vf[i][1]->pts_us64);
					else if (vf)
						pr_dv_dbg("=== bl(%p-%lld) toggled ===\n",
							vf,
							vf->pts_us64);
				}
				return 0;
			}
		}
	}
	return 1;
}

static bool check_atsc_dvb(char *p)
{
	u32 country_code;
	u32 provider_code;
	u32 user_id;
	u32 user_type_code;

	if (!p)
		return false;

	country_code = *(p + 4);
	provider_code = (*(p + 5) << 8) |
			*(p + 6);
	user_id = (*(p + 7) << 24) |
		(*(p + 8) << 16) |
		(*(p + 9) << 8) |
		(*(p + 10));
	user_type_code = *(p + 11);
	if (country_code == 0xB5 &&
	    ((provider_code ==
	    ATSC_T35_PROV_CODE &&
	    user_id == ATSC_USER_ID_CODE) ||
	    (provider_code ==
	    DVB_T35_PROV_CODE &&
	    user_id == DVB_USER_ID_CODE)) &&
	    user_type_code ==
	    DM_MD_USER_TYPE_CODE)
		return true;
	else
		return false;
}

int parse_sei_and_meta_ext_v1(struct vframe_s *vf,
					 char *aux_buf,
					 int aux_size,
					 int *total_comp_size,
					 int *total_md_size,
					 void *fmt,
					 int *ret_flags,
					 char *md_buf,
					 char *comp_buf)
{
	int i;
	char *p;
	unsigned int size = 0;
	unsigned int type = 0;
	int md_size = 0;
	int comp_size = 0;
	int parser_ready = 0;
	int ret = 2;
	unsigned long flags = 0;
	bool parser_overflow = false;
	int rpu_ret = 0;
	int reset_flag = 0;
	unsigned int rpu_size = 0;
	enum signal_format_enum *src_format = (enum signal_format_enum *)fmt;
	static int parse_process_count;
	char meta_buf[1024];
	static u32 last_play_id;

	if (!aux_buf || aux_size == 0 || !fmt || !md_buf || !comp_buf ||
	    !total_comp_size || !total_md_size || !ret_flags)
		return 1;

	parse_process_count++;
	if (parse_process_count > 1) {
		pr_err("parser not support multi instance\n");
		ret = 1;
		goto parse_err;
	}

	/* release metadata_parser when new playing */
	if (vf && vf->src_fmt.play_id != last_play_id) {
		if (metadata_parser) {
			if (p_funcs_stb)
				p_funcs_stb->metadata_parser_release();
			if (p_funcs_tv)
				p_funcs_tv->metadata_parser_release();
			metadata_parser = NULL;
			pr_dv_dbg("new play, release parser\n");
			amdv_clear_buf(0);
		}
		last_play_id = vf->src_fmt.play_id;
		if (debug_dolby & 2)
			pr_dv_dbg("update play id=%d:\n", last_play_id);
	}

	p = aux_buf;
	while (p < aux_buf + aux_size - 8) {
		size = *p++;
		size = (size << 8) | *p++;
		size = (size << 8) | *p++;
		size = (size << 8) | *p++;
		type = *p++;
		type = (type << 8) | *p++;
		type = (type << 8) | *p++;
		type = (type << 8) | *p++;
		if (debug_dolby & 4)
			pr_dv_dbg("metadata type=%08x, size=%d:\n",
				     type, size);
		if (size == 0 || size > aux_size) {
			pr_dv_dbg("invalid aux size %d\n", size);
			ret = 1;
			goto parse_err;
		}
		if (type == DV_SEI || /* hevc t35 sei */
			(type & 0xffff0000) == AV1_SEI) { /* av1 t35 obu */
			*total_comp_size = 0;
			*total_md_size = 0;

			if ((type & 0xffff0000) == AV1_SEI &&
			    p[0] == 0xb5 &&
			    p[1] == 0x00 &&
			    p[2] == 0x3b &&
			    p[3] == 0x00 &&
			    p[4] == 0x00 &&
			    p[5] == 0x08 &&
			    p[6] == 0x00 &&
			    p[7] == 0x37 &&
			    p[8] == 0xcd &&
			    p[9] == 0x08) {
				/* AV1 dv meta in obu */
				*src_format = FORMAT_DOVI;
				meta_buf[0] = 0;
				meta_buf[1] = 0;
				meta_buf[2] = 0;
				meta_buf[3] = 0x01;
				meta_buf[4] = 0x19;
				if (p[11] & 0x10) {
					rpu_size = 0x100;
					rpu_size |= (p[11] & 0x0f) << 4;
					rpu_size |= (p[12] >> 4) & 0x0f;
					if (p[12] & 0x08) {
						pr_dv_error
							("rpu in obu exceed 512 bytes\n");
						break;
					}
					for (i = 0; i < rpu_size; i++) {
						meta_buf[5 + i] =
							(p[12 + i] & 0x07) << 5;
						meta_buf[5 + i] |=
							(p[13 + i] >> 3) & 0x1f;
					}
					rpu_size += 5;
				} else {
					rpu_size = (p[10] & 0x1f) << 3;
					rpu_size |= (p[11] >> 5) & 0x07;
					for (i = 0; i < rpu_size; i++) {
						meta_buf[5 + i] =
							(p[11 + i] & 0x0f) << 4;
						meta_buf[5 + i] |=
							(p[12 + i] >> 4) & 0x0f;
					}
					rpu_size += 5;
				}
			} else {
				/* HEVC dv meta in sei */
				*src_format = FORMAT_DOVI;
				if (size > (sizeof(meta_buf) - 3))
					size = (sizeof(meta_buf) - 3);
				meta_buf[0] = 0;
				meta_buf[1] = 0;
				meta_buf[2] = 0;
				memcpy(&meta_buf[3], p + 1, size - 1);
				rpu_size = size + 2;
			}
			if ((debug_dolby & 4) && dump_enable) {
				pr_dv_dbg("metadata(%d):\n", rpu_size);
				for (i = 0; i < size; i += 16)
					pr_info("%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
						meta_buf[i],
						meta_buf[i + 1],
						meta_buf[i + 2],
						meta_buf[i + 3],
						meta_buf[i + 4],
						meta_buf[i + 5],
						meta_buf[i + 6],
						meta_buf[i + 7],
						meta_buf[i + 8],
						meta_buf[i + 9],
						meta_buf[i + 10],
						meta_buf[i + 11],
						meta_buf[i + 12],
						meta_buf[i + 13],
						meta_buf[i + 14],
						meta_buf[i + 15]);
			}

			if (tv_mode) {
				if (!p_funcs_tv) {
					ret = 1;
					goto parse_err;
				}
			} else {
				if (!p_funcs_stb) {
					ret = 1;
					goto parse_err;
				}
			}
			/* prepare metadata parser */
			spin_lock_irqsave(&amdv_lock, flags);
			parser_ready = 0;
			if (!metadata_parser) {
				if (is_aml_tvmode()) {
					if (p_funcs_tv) {
						metadata_parser =
						p_funcs_tv->metadata_parser_init
							(dolby_vision_flags
							 & FLAG_CHANGE_SEQ_HEAD
							 ? 1 : 0);
						p_funcs_tv->metadata_parser_reset(1);
					} else {
						pr_dv_dbg("p_funcs_tv is null\n");
					}
				} else {
					if (p_funcs_stb) {
						metadata_parser =
						p_funcs_stb->metadata_parser_init
							(dolby_vision_flags
							 & FLAG_CHANGE_SEQ_HEAD
							 ? 1 : 0);
						p_funcs_stb->metadata_parser_reset(1);
					} else {
						pr_dv_dbg("p_funcs_stb is null\n");
					}
				}
				if (metadata_parser) {
					parser_ready = 1;
					if (debug_dolby & 1)
						pr_dv_dbg("metadata parser init OK\n");
				}
			} else {
				if (is_aml_tvmode()) {
					if (p_funcs_tv->metadata_parser_reset
					    (metadata_parser_reset_flag) == 0)
						metadata_parser_reset_flag = 0;
				} else {
					if (p_funcs_stb->metadata_parser_reset
					    (metadata_parser_reset_flag) == 0)
						metadata_parser_reset_flag = 0;
				}
				parser_ready = 1;
			}
			if (!parser_ready) {
				spin_unlock_irqrestore(&amdv_lock, flags);
				pr_dv_error
				("meta(%d), pts(%lld) -> parser init fail\n",
					rpu_size, vf->pts_us64);
				ret = 1;
				goto parse_err;
			}

			md_size = 0;
			comp_size = 0;

			if (is_aml_tvmode())
				rpu_ret =
				p_funcs_tv->metadata_parser_process
				(meta_buf, rpu_size,
				 comp_buf + *total_comp_size,
				 &comp_size,
				 md_buf + *total_md_size,
				 &md_size,
				 true);
			else
				rpu_ret =
				p_funcs_stb->metadata_parser_process
				(meta_buf, rpu_size,
				 comp_buf + *total_comp_size,
				 &comp_size,
				 md_buf + *total_md_size,
				 &md_size,
				 true);

			if (rpu_ret < 0) {
				if (vf)
					pr_dv_error
					("meta(%d), pts(%lld) -> metadata parser process fail\n",
					 rpu_size, vf->pts_us64);
				ret = 3;
			} else {
				if (*total_comp_size + comp_size
					< COMP_BUF_SIZE)
					*total_comp_size += comp_size;
				else
					parser_overflow = true;

				if (*total_md_size + md_size
					< MD_BUF_SIZE)
					*total_md_size += md_size;
				else
					parser_overflow = true;
				if (rpu_ret == 1)
					*ret_flags = 1;
				ret = 0;
			}
			spin_unlock_irqrestore(&amdv_lock, flags);
			if (parser_overflow) {
				ret = 2;
				break;
			}
			/*dv type just appears once in metadata
			 *after parsing dv type,breaking the
			 *circulation directly
			 */
			break;
		}
		p += size;
	}

	/*continue to check atsc/dvb dv */
	if (atsc_sei && *src_format != FORMAT_DOVI) {
		struct dv_vui_parameters vui_param = {0};
		static u32 len_2086_sei;
		u32 len_2094_sei = 0;
		static u8 payload_2086_sei[MAX_LEN_2086_SEI];
		u8 payload_2094_sei[MAX_LEN_2094_SEI];
		unsigned char nal_type;
		unsigned char sei_payload_type = 0;
		unsigned char sei_payload_size = 0;

		if (vf) {
			vui_param.video_fmt_i = (vf->signal_type >> 26) & 7;
			vui_param.video_fullrange_b = (vf->signal_type >> 25) & 1;
			vui_param.color_description_b = (vf->signal_type >> 24) & 1;
			vui_param.color_primaries_i = (vf->signal_type >> 16) & 0xff;
			vui_param.trans_characteristic_i =
							(vf->signal_type >> 8) & 0xff;
			vui_param.matrix_coeff_i = (vf->signal_type) & 0xff;
			if (debug_dolby & 2)
				pr_dv_dbg("vui_param %d, %d, %d, %d, %d, %d\n",
					vui_param.video_fmt_i,
					vui_param.video_fullrange_b,
					vui_param.color_description_b,
					vui_param.color_primaries_i,
					vui_param.trans_characteristic_i,
					vui_param.matrix_coeff_i);
		}

		p = aux_buf;

		if ((debug_dolby & 0x200) && dump_enable) {
			pr_dv_dbg("aux_buf(%d):\n", aux_size);
			for (i = 0; i < aux_size; i += 8)
				pr_info("\t%02x %02x %02x %02x %02x %02x %02x %02x\n",
					p[i],
					p[i + 1],
					p[i + 2],
					p[i + 3],
					p[i + 4],
					p[i + 5],
					p[i + 6],
					p[i + 7]);
		}
		while (p < aux_buf + aux_size - 8) {
			size = *p++;
			size = (size << 8) | *p++;
			size = (size << 8) | *p++;
			size = (size << 8) | *p++;
			type = *p++;
			type = (type << 8) | *p++;
			type = (type << 8) | *p++;
			type = (type << 8) | *p++;
			if (debug_dolby & 2)
				pr_dv_dbg("type: 0x%x\n", type);

			/*4 byte size + 4 byte type*/
			/*1 byte nal_type + 1 byte (layer_id+temporal_id)*/
			/*1 byte payload type + 1 byte size + payload data*/
			if (type == 0x02000000) {
				nal_type = ((*p) & 0x7E) >> 1; /*nal unit type*/
				if (debug_dolby & 2)
					pr_dv_dbg("nal_type: %d\n",
						     nal_type);

				if (nal_type == PREFIX_SEI_NUT_NAL ||
					nal_type == SUFFIX_SEI_NUT_NAL) {
					sei_payload_type = *(p + 2);
					sei_payload_size = *(p + 3);
					if (debug_dolby & 2)
						pr_dv_dbg("type %d, size %d\n",
							     sei_payload_type,
							     sei_payload_size);
					if (sei_payload_type ==
					SEI_TYPE_MASTERING_DISP_COLOUR_VOLUME) {
						len_2086_sei =
							sei_payload_size;
						memcpy(payload_2086_sei, p + 4,
						       len_2086_sei);
					} else if (sei_payload_type == SEI_ITU_T_T35 &&
						sei_payload_size >= 8 && check_atsc_dvb(p)) {
						len_2094_sei = sei_payload_size;
						memcpy(payload_2094_sei, p + 4,
						       len_2094_sei);
					}
					if (len_2086_sei > 0 &&
					    len_2094_sei > 0)
						break;
				}
			}
			p += size;
		}
		if (len_2094_sei > 0) {
			/* source is VS10 */
			*total_comp_size = 0;
			*total_md_size = 0;
			*src_format = FORMAT_DOVI;
			p_atsc_md.vui_param = vui_param;
			p_atsc_md.len_2086_sei = len_2086_sei;
			memcpy(p_atsc_md.sei_2086, payload_2086_sei,
			       len_2086_sei);
			p_atsc_md.len_2094_sei = len_2094_sei;
			memcpy(p_atsc_md.sei_2094, payload_2094_sei,
			       len_2094_sei);
			size = sizeof(struct dv_atsc);
			if (size > sizeof(meta_buf))
				size = sizeof(meta_buf);
			memcpy(meta_buf, (unsigned char *)(&p_atsc_md), size);
			if ((debug_dolby & 4) && dump_enable) {
				pr_dv_dbg("metadata(%d):\n", size);
				for (i = 0; i < size; i += 8)
					pr_info("\t%02x %02x %02x %02x %02x %02x %02x %02x\n",
						meta_buf[i],
						meta_buf[i + 1],
						meta_buf[i + 2],
						meta_buf[i + 3],
						meta_buf[i + 4],
						meta_buf[i + 5],
						meta_buf[i + 6],
						meta_buf[i + 7]);
			}
			if (tv_mode) {
				if (!p_funcs_tv) {
					ret = 1;
					goto parse_err;
				}
			} else {
				if (!p_funcs_stb) {
					ret = 1;
					goto parse_err;
				}
			}
			/* prepare metadata parser */
			spin_lock_irqsave(&amdv_lock, flags);
			parser_ready = 0;
			reset_flag = 2; /*flag: bit0 flag, bit1 0->dv, 1->atsc*/

			if (!metadata_parser) {
				if (is_aml_tvmode()) {
					metadata_parser =
					p_funcs_tv->metadata_parser_init
						(dolby_vision_flags
						& FLAG_CHANGE_SEQ_HEAD
						? 1 : 0);
					p_funcs_tv->metadata_parser_reset
						(reset_flag | 1);
				} else {
					metadata_parser =
					p_funcs_stb->metadata_parser_init
						(dolby_vision_flags
						& FLAG_CHANGE_SEQ_HEAD
						? 1 : 0);
					p_funcs_stb->metadata_parser_reset
						(reset_flag | 1);
				}
				if (metadata_parser) {
					parser_ready = 1;
					if (debug_dolby & 1)
						pr_dv_dbg("metadata parser init OK\n");
				}
			} else {
				if (is_aml_tvmode()) {
					if (p_funcs_tv->metadata_parser_reset
						(reset_flag |
						metadata_parser_reset_flag
							) == 0)
						metadata_parser_reset_flag = 0;
				} else {
					if (p_funcs_stb->metadata_parser_reset
						(reset_flag |
						metadata_parser_reset_flag
							) == 0)
						metadata_parser_reset_flag = 0;
				}
				parser_ready = 1;
			}

			if (!parser_ready) {
				spin_unlock_irqrestore(&amdv_lock, flags);
				if (vf)
					pr_dv_error
					("meta(%d), pts(%lld) -> metadata parser init fail\n",
					 size, vf->pts_us64);
				ret = 1;
				goto parse_err;
			}

			md_size = 0;
			comp_size = 0;

			if (is_aml_tvmode())
				rpu_ret =
				p_funcs_tv->metadata_parser_process
				(meta_buf, size,
				comp_buf + *total_comp_size,
				&comp_size,
				md_buf + *total_md_size,
				&md_size,
				true);
			else
				rpu_ret =
				p_funcs_stb->metadata_parser_process
				(meta_buf, size,
				comp_buf + *total_comp_size,
				&comp_size,
				md_buf + *total_md_size,
				&md_size,
				true);

			if (rpu_ret < 0) {
				if (vf)
					pr_dv_error
					("meta(%d), pts(%lld) -> metadata parser process fail\n",
					size, vf->pts_us64);
				ret = 3;
			} else {
				if (*total_comp_size + comp_size
					< COMP_BUF_SIZE)
					*total_comp_size += comp_size;
				else
					parser_overflow = true;

				if (*total_md_size + md_size
					< MD_BUF_SIZE)
					*total_md_size += md_size;
				else
					parser_overflow = true;
				if (rpu_ret == 1)
					*ret_flags = 1;
				ret = 0;
			}
			spin_unlock_irqrestore(&amdv_lock, flags);

			if (parser_overflow)
				ret = 2;
		} else {
			len_2086_sei = 2;
		}
	}

	if (*total_md_size) {
		if ((debug_dolby & 1) && vf)
			pr_dv_dbg
			("meta(%d), pts(%lld) -> md(%d), comp(%d)\n",
			 size, vf->pts_us64,
			 *total_md_size, *total_comp_size);
		if ((debug_dolby & 4) && dump_enable)  {
			pr_dv_dbg("parsed md(%d):\n", *total_md_size);
			for (i = 0; i < *total_md_size + 7; i += 8) {
				pr_info("%02x %02x %02x %02x %02x %02x %02x %02x\n",
					md_buf[i],
					md_buf[i + 1],
					md_buf[i + 2],
					md_buf[i + 3],
					md_buf[i + 4],
					md_buf[i + 5],
					md_buf[i + 6],
					md_buf[i + 7]);
			}
		}
	}
parse_err:
	parse_process_count--;
	return ret;
}

int parse_sei_and_meta_ext_v2(struct vframe_s *vf,
					char *aux_buf,
					int aux_size,
					int *total_comp_size,
					int *total_md_size,
					void *fmt,
					int *ret_flags,
					char *md_buf,
					char *comp_buf)
{
	int i;
	char *p;
	unsigned int size = 0;
	unsigned int type = 0;
	int md_size = 0;
	int comp_size = 0;
	int ret = 2;
	bool parser_overflow = false;
	int rpu_ret = 0;
	unsigned int rpu_size = 0;
	enum signal_format_enum *src_format = (enum signal_format_enum *)fmt;
	char meta_buf[1024];
	int dv_id = vf->src_fmt.dv_id;

	if (!dv_inst_valid(dv_id)) {
		pr_err("err inst %d\n", dv_id);
		return 1;
	}

	if (!aux_buf || aux_size == 0 || !fmt || !md_buf || !comp_buf ||
	    !total_comp_size || !total_md_size || !ret_flags)
		return 1;

	p = aux_buf;
	while (p < aux_buf + aux_size - 8) {
		size = *p++;
		size = (size << 8) | *p++;
		size = (size << 8) | *p++;
		size = (size << 8) | *p++;
		type = *p++;
		type = (type << 8) | *p++;
		type = (type << 8) | *p++;
		type = (type << 8) | *p++;
		if (debug_dolby & 4)
			pr_dv_dbg("[inst%d]metadata type=%08x, size=%d:\n",
				     dv_id + 1, type, size);
		if (size == 0 || size > aux_size) {
			pr_dv_dbg("invalid aux size %d\n", size);
			ret = 1;
			goto parse_err;
		}
		if (type == DV_SEI || /* hevc t35 sei */
		(type & 0xffff0000) == AV1_SEI) { /* av1 t35 obu */
			*total_comp_size = 0;
			*total_md_size = 0;

			if ((type & 0xffff0000) == AV1_SEI &&
			    p[0] == 0xb5 &&
			    p[1] == 0x00 &&
			    p[2] == 0x3b &&
			    p[3] == 0x00 &&
			    p[4] == 0x00 &&
			    p[5] == 0x08 &&
			    p[6] == 0x00 &&
			    p[7] == 0x37 &&
			    p[8] == 0xcd &&
			    p[9] == 0x08) {
				/* AV1 dv meta in obu */
				*src_format = FORMAT_DOVI;
				meta_buf[0] = 0;
				meta_buf[1] = 0;
				meta_buf[2] = 0;
				meta_buf[3] = 0x01;
				meta_buf[4] = 0x19;
				if (p[11] & 0x10) {
					rpu_size = 0x100;
					rpu_size |= (p[11] & 0x0f) << 4;
					rpu_size |= (p[12] >> 4) & 0x0f;
					if (p[12] & 0x08) {
						pr_dv_error("rpu in obu exceed 512 bytes\n");
						break;
					}
					for (i = 0; i < rpu_size; i++) {
						meta_buf[5 + i] =
							(p[12 + i] & 0x07) << 5;
						meta_buf[5 + i] |=
							(p[13 + i] >> 3) & 0x1f;
					}
					rpu_size += 5;
				} else {
					rpu_size = (p[10] & 0x1f) << 3;
					rpu_size |= (p[11] >> 5) & 0x07;
					for (i = 0; i < rpu_size; i++) {
						meta_buf[5 + i] =
							(p[11 + i] & 0x0f) << 4;
						meta_buf[5 + i] |=
							(p[12 + i] >> 4) & 0x0f;
					}
					rpu_size += 5;
				}
			} else {
				/* HEVC dv meta in sei */
				*src_format = FORMAT_DOVI;
				if (size > (sizeof(meta_buf) - 3))
					size = (sizeof(meta_buf) - 3);
				meta_buf[0] = 0;
				meta_buf[1] = 0;
				meta_buf[2] = 0;
				memcpy(&meta_buf[3], p + 1, size - 1);
				rpu_size = size + 2;
			}
			if ((debug_dolby & 4) && dump_enable_f(dv_id)) {
				pr_dv_dbg("[inst%d]metadata(%d):\n", dv_id + 1, rpu_size);
				for (i = 0; i < size; i += 16)
					pr_info("%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
						meta_buf[i],
						meta_buf[i + 1],
						meta_buf[i + 2],
						meta_buf[i + 3],
						meta_buf[i + 4],
						meta_buf[i + 5],
						meta_buf[i + 6],
						meta_buf[i + 7],
						meta_buf[i + 8],
						meta_buf[i + 9],
						meta_buf[i + 10],
						meta_buf[i + 11],
						meta_buf[i + 12],
						meta_buf[i + 13],
						meta_buf[i + 14],
						meta_buf[i + 15]);
			}

			if (tv_mode) {
				if (!p_funcs_tv) {
					ret = 1;
					goto parse_err;
				}
			} else {
				if (!p_funcs_stb) {
					ret = 1;
					goto parse_err;
				}
			}
			/* prepare metadata parser */
			if (!dv_inst[dv_id].metadata_parser) {
				pr_dv_error
				("[inst%d]meta(%d), pts(%lld) -> metadata parser init fail\n",
					dv_id + 1, rpu_size, vf->pts_us64);
				ret = 1;
				goto parse_err;
			}

			md_size = 0;
			comp_size = 0;
			if (is_aml_tvmode()) {
				rpu_ret =
				p_funcs_tv->metadata_parser_process
				(meta_buf, rpu_size,
				 comp_buf +
				 *total_comp_size,
				 &comp_size,
				 md_buf + *total_md_size,
				 &md_size,
				 true);
			} else {
				rpu_ret =
				p_funcs_stb->multi_mp_process
				(dv_inst[dv_id].metadata_parser,
				 meta_buf, rpu_size,
				 comp_buf +
				 *total_comp_size,
				 &comp_size,
				 md_buf + *total_md_size,
				 &md_size,
				 true, DV_TYPE_DOVI);
			}

			if (rpu_ret < 0) {
				pr_dv_error
				("[inst%d]meta(%d), pts(%lld) -> metadata parser process fail\n",
				dv_id + 1, rpu_size, vf->pts_us64);
				ret = 3;
			} else {
				if (*total_comp_size + comp_size
					< COMP_BUF_SIZE)
					*total_comp_size += comp_size;
				else
					parser_overflow = true;

				if (*total_md_size + md_size
					< MD_BUF_SIZE)
					*total_md_size += md_size;
				else
					parser_overflow = true;
				if (rpu_ret == 1)
					*ret_flags = 1;
				ret = 0;
			}
			if (parser_overflow) {
				ret = 2;
				break;
			}
			/*dv type just appears once in metadata
			 *after parsing dv type,breaking the
			 *circulation directly
			 */
			break;
		}
		p += size;
	}

	/*continue to check atsc/dvb dv */
	if (atsc_sei && *src_format != FORMAT_DOVI) {
		struct dv_vui_parameters vui_param;
		static u32 len_2086_sei;
		u32 len_2094_sei = 0;
		static u8 payload_2086_sei[MAX_LEN_2086_SEI];
		u8 payload_2094_sei[MAX_LEN_2094_SEI];
		unsigned char nal_type;
		unsigned char sei_payload_type = 0;
		unsigned char sei_payload_size = 0;

		vui_param.video_fmt_i = (vf->signal_type >> 26) & 7;
		vui_param.video_fullrange_b = (vf->signal_type >> 25) & 1;
		vui_param.color_description_b = (vf->signal_type >> 24) & 1;
		vui_param.color_primaries_i = (vf->signal_type >> 16) & 0xff;
		vui_param.trans_characteristic_i =
						(vf->signal_type >> 8) & 0xff;
		vui_param.matrix_coeff_i = (vf->signal_type) & 0xff;
		if (debug_dolby & 2)
			pr_dv_dbg("vui_param %d, %d, %d, %d, %d, %d\n",
				vui_param.video_fmt_i,
				vui_param.video_fullrange_b,
				vui_param.color_description_b,
				vui_param.color_primaries_i,
				vui_param.trans_characteristic_i,
				vui_param.matrix_coeff_i);

		p = aux_buf;

		if ((debug_dolby & 0x200) && dump_enable_f(dv_id)) {
			pr_dv_dbg("[inst%d]aux_buf(%d):\n", dv_id + 1, aux_size);
			for (i = 0; i < aux_size; i += 8)
				pr_info("\t%02x %02x %02x %02x %02x %02x %02x %02x\n",
					p[i],
					p[i + 1],
					p[i + 2],
					p[i + 3],
					p[i + 4],
					p[i + 5],
					p[i + 6],
					p[i + 7]);
		}
		while (p < aux_buf + aux_size - 8) {
			size = *p++;
			size = (size << 8) | *p++;
			size = (size << 8) | *p++;
			size = (size << 8) | *p++;
			type = *p++;
			type = (type << 8) | *p++;
			type = (type << 8) | *p++;
			type = (type << 8) | *p++;
			if (debug_dolby & 2)
				pr_dv_dbg("type: 0x%x\n", type);

			/*4 byte size + 4 byte type*/
			/*1 byte nal_type + 1 byte (layer_id+temporal_id)*/
			/*1 byte payload type + 1 byte size + payload data*/
			if (type == 0x02000000) {
				nal_type = ((*p) & 0x7E) >> 1; /*nal unit type*/
				if (debug_dolby & 2)
					pr_dv_dbg("nal_type: %d\n",
						     nal_type);

				if (nal_type == PREFIX_SEI_NUT_NAL ||
					nal_type == SUFFIX_SEI_NUT_NAL) {
					sei_payload_type = *(p + 2);
					sei_payload_size = *(p + 3);
					if (debug_dolby & 2)
						pr_dv_dbg("type %d, size %d\n",
							     sei_payload_type,
							     sei_payload_size);
					if (sei_payload_type ==
					SEI_TYPE_MASTERING_DISP_COLOUR_VOLUME) {
						len_2086_sei =
							sei_payload_size;
						memcpy(payload_2086_sei, p + 4,
						       len_2086_sei);
					} else if (sei_payload_type == SEI_ITU_T_T35 &&
						sei_payload_size >= 8 && check_atsc_dvb(p)) {
						len_2094_sei = sei_payload_size;
						memcpy(payload_2094_sei, p + 4,
						       len_2094_sei);
					}
					if (len_2086_sei > 0 &&
					    len_2094_sei > 0)
						break;
				}
			}
			p += size;
		}
		if (len_2094_sei > 0) {
			/* source is VS10 */
			*total_comp_size = 0;
			*total_md_size = 0;
			*src_format = FORMAT_DOVI;
			p_atsc_md.vui_param = vui_param;
			p_atsc_md.len_2086_sei = len_2086_sei;
			memcpy(p_atsc_md.sei_2086, payload_2086_sei,
			       len_2086_sei);
			p_atsc_md.len_2094_sei = len_2094_sei;
			memcpy(p_atsc_md.sei_2094, payload_2094_sei,
			       len_2094_sei);
			size = sizeof(struct dv_atsc);
			if (size > sizeof(meta_buf))
				size = sizeof(meta_buf);
			memcpy(meta_buf, (unsigned char *)(&p_atsc_md), size);
			if ((debug_dolby & 4) && dump_enable_f(dv_id)) {
				pr_dv_dbg("[inst%d]metadata(%d):\n", dv_id + 1, size);
				for (i = 0; i < size; i += 8)
					pr_info("\t%02x %02x %02x %02x %02x %02x %02x %02x\n",
						meta_buf[i],
						meta_buf[i + 1],
						meta_buf[i + 2],
						meta_buf[i + 3],
						meta_buf[i + 4],
						meta_buf[i + 5],
						meta_buf[i + 6],
						meta_buf[i + 7]);
			}
			if (tv_mode) {
				if (!p_funcs_tv) {
					ret = 1;
					goto parse_err;
				}
			} else {
				if (!p_funcs_stb) {
					ret = 1;
					goto parse_err;
				}
			}
			/* prepare metadata parser */
			if (!dv_inst[dv_id].metadata_parser) {
				pr_dv_error
				("meta(%d), pts(%lld) -> metadata parser init fail\n",
				size, vf->pts_us64);
				ret = 1;
				goto parse_err;
			}

			md_size = 0;
			comp_size = 0;

			if (is_aml_tvmode())
				rpu_ret =
				p_funcs_tv->metadata_parser_process
				(meta_buf, size,
				comp_buf + *total_comp_size,
				&comp_size,
				md_buf + *total_md_size,
				&md_size,
				true);
			else
				rpu_ret =
				p_funcs_stb->multi_mp_process
				(dv_inst[dv_id].metadata_parser,
				 meta_buf, size,
				 comp_buf + *total_comp_size,
				 &comp_size,
				 md_buf + *total_md_size,
				 &md_size,
				 true, DV_TYPE_ATSC);

			if (rpu_ret < 0) {
				pr_dv_error
					("meta(%d), pts(%lld) -> metadata parser process fail\n",
					size, vf->pts_us64);
				ret = 3;
			} else {
				if (*total_comp_size + comp_size
					< COMP_BUF_SIZE)
					*total_comp_size += comp_size;
				else
					parser_overflow = true;

				if (*total_md_size + md_size
					< MD_BUF_SIZE)
					*total_md_size += md_size;
				else
					parser_overflow = true;
				if (rpu_ret == 1)
					*ret_flags = 1;
				ret = 0;
			}

			if (parser_overflow)
				ret = 2;
		} else {
			len_2086_sei = 2;
		}
	}

	if (*total_md_size) {
		if (debug_dolby & 1)
			pr_dv_dbg
			("[inst%d]meta(%d), pts(%lld) -> md(%d), comp(%d)\n",
			dv_id + 1,
			size, vf->pts_us64,
			*total_md_size, *total_comp_size);
		if ((debug_dolby & 4) && dump_enable_f(dv_id))  {
			pr_dv_dbg("[inst%d]parsed md(%d):\n", dv_id + 1, *total_md_size);
			for (i = 0; i < *total_md_size + 7; i += 8) {
				pr_info("\t%02x %02x %02x %02x %02x %02x %02x %02x\n",
					md_buf[i],
					md_buf[i + 1],
					md_buf[i + 2],
					md_buf[i + 3],
					md_buf[i + 4],
					md_buf[i + 5],
					md_buf[i + 6],
					md_buf[i + 7]);
			}
		}
	}
parse_err:
	return ret;
}

/* ret 0: parser metadata success */
/* ret 1: do nothing */
/* ret 2: parser overflow */
/* ret 3: parser error */
int parse_sei_and_meta_ext(struct vframe_s *vf, char *aux_buf,
				    int aux_size, int *total_comp_size,
				    int *total_md_size, void *fmt,
				    int *ret_flags, char *md_buf, char *comp_buf)
{
	int ret;

	if (multi_dv_mode)
		ret = parse_sei_and_meta_ext_v2(vf, aux_buf, aux_size, total_comp_size,
						total_md_size, fmt, ret_flags, md_buf, comp_buf);
	else
		ret = parse_sei_and_meta_ext_v1(vf, aux_buf, aux_size, total_comp_size,
						total_md_size, fmt, ret_flags, md_buf, comp_buf);

	return ret;
}
EXPORT_SYMBOL(parse_sei_and_meta_ext);

static int parse_sei_and_meta(struct vframe_s *vf,
				      struct provider_aux_req_s *req,
				      int *total_comp_size,
				      int *total_md_size,
				      enum signal_format_enum *src_format,
				      int *ret_flags, bool drop_flag, u32 inst_id)
{
	int ret = 1;
	char *p_md_buf;
	char *p_comp_buf;
	int next_id = current_id ^ 1;

	if (!req->aux_buf || req->aux_size == 0)
		return 1;

	if (multi_dv_mode) {
		next_id = dv_inst[inst_id].current_id ^ 1;
		p_md_buf = dv_inst[inst_id].md_buf[next_id];
		p_comp_buf = dv_inst[inst_id].comp_buf[next_id];
	} else {
		if (drop_flag) {
			p_md_buf = drop_md_buf[next_id];
			p_comp_buf = drop_comp_buf[next_id];
		} else {
			p_md_buf = md_buf[next_id];
			p_comp_buf = comp_buf[next_id];
		}
	}
	ret = parse_sei_and_meta_ext(vf,
				     req->aux_buf,
				     req->aux_size,
				     total_comp_size,
				     total_md_size,
				     src_format,
				     ret_flags,
				     p_md_buf,
				     p_comp_buf);

	if (multi_dv_mode) {
		if (ret == 0) {
			dv_inst[inst_id].current_id = next_id;
			dv_inst[inst_id].backup_comp_size = *total_comp_size;
			dv_inst[inst_id].backup_md_size = *total_md_size;
		} else { /*parser err, use backup md and comp*/
			*total_comp_size = dv_inst[inst_id].backup_comp_size;
			*total_md_size = dv_inst[inst_id].backup_md_size;
		}
	} else {
		if (ret == 0) {
			current_id = next_id;
			backup_comp_size = *total_comp_size;
			backup_md_size = *total_md_size;
		} else { /*parser err, use backup md and comp*/
			*total_comp_size = backup_comp_size;
			*total_md_size = backup_md_size;
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

static u32 p3_primaries[3][2] = {
	{0.265 * INORM + 0.5, 0.69 * INORM + 0.5},	/* G */
	{0.15 * INORM + 0.5, 0.06 * INORM + 0.5},	/* B */
	{0.68 * INORM + 0.5, 0.32 * INORM + 0.5},	/* R */
};

static u32 bt2020_white_point[2] = {
	0.3127 * INORM + 0.5, 0.3290 * INORM + 0.5
};

static u32 p3_white_point[2] = {
	0.3127 * INORM + 0.5, 0.3290 * INORM + 0.5
};

void prepare_hdr10_param(struct vframe_master_display_colour_s *p_mdc,
			 struct hdr10_parameter *p_hdr10_param)
{
	struct vframe_content_light_level_s *p_cll =
		&p_mdc->content_light_level;
	u8 flag = 0;
	u32 max_lum = 1000 * 10000;
	u32 min_lum = 50;
	int primaries_type = 0;

	if (get_primary_policy() == PRIMARIES_NATIVE ||
	    primary_debug == 1 ||
	    (dolby_vision_flags & FLAG_CERTIFICATION) ||
	    !strcasecmp(get_cur_pic_mode_name(), "hdr10_dark")) {
		p_hdr10_param->min_display_mastering_lum =
			min_lum;
		p_hdr10_param->max_display_mastering_lum =
			max_lum;
		p_hdr10_param->r_x = bt2020_primaries[2][0];
		p_hdr10_param->r_y = bt2020_primaries[2][1];
		p_hdr10_param->g_x = bt2020_primaries[0][0];
		p_hdr10_param->g_y = bt2020_primaries[0][1];
		p_hdr10_param->b_x = bt2020_primaries[1][0];
		p_hdr10_param->b_y = bt2020_primaries[1][1];
		p_hdr10_param->w_x = bt2020_white_point[0];
		p_hdr10_param->w_y = bt2020_white_point[1];
		p_hdr10_param->max_content_light_level = 0;
		p_hdr10_param->max_frame_avg_light_level = 0;
		return;
	} else if (get_primary_policy() == PRIMARIES_AUTO ||
		primary_debug == 2) {
		p_hdr10_param->min_display_mastering_lum = min_lum;
		p_hdr10_param->max_display_mastering_lum = max_lum;
		p_hdr10_param->r_x = p3_primaries[2][0];
		p_hdr10_param->r_y = p3_primaries[2][1];
		p_hdr10_param->g_x = p3_primaries[0][0];
		p_hdr10_param->g_y = p3_primaries[0][1];
		p_hdr10_param->b_x = p3_primaries[1][0];
		p_hdr10_param->b_y = p3_primaries[1][1];
		p_hdr10_param->w_x = p3_white_point[0];
		p_hdr10_param->w_y = p3_white_point[1];
		p_hdr10_param->max_content_light_level = 0;
		p_hdr10_param->max_frame_avg_light_level = 0;
		return;
	}

	primaries_type = get_primaries_type(p_mdc);
	if (primaries_type == 2) {
		/* GBR -> RGB as DV will swap back to GBR
		 * in send_hdmi_pkt
		 */
		if (p_hdr10_param->max_display_mastering_lum !=
		    p_mdc->luminance[0] ||
		    p_hdr10_param->min_display_mastering_lum !=
		    p_mdc->luminance[1] ||
		    p_hdr10_param->r_x != p_mdc->primaries[2][0] ||
		    p_hdr10_param->r_y != p_mdc->primaries[2][1] ||
		    p_hdr10_param->g_x != p_mdc->primaries[0][0] ||
		    p_hdr10_param->g_y != p_mdc->primaries[0][1] ||
		    p_hdr10_param->b_x != p_mdc->primaries[1][0] ||
		    p_hdr10_param->b_y != p_mdc->primaries[1][1] ||
		    p_hdr10_param->w_x != p_mdc->white_point[0] ||
		    p_hdr10_param->w_y != p_mdc->white_point[1]) {
			flag |= 1;
			p_hdr10_param->max_display_mastering_lum =
				p_mdc->luminance[0];
			if (p_mdc->luminance[0] == 0) {
				if ((debug_dolby & 1))
					pr_info("invalid max, use default\n");
				p_hdr10_param->max_display_mastering_lum = max_lum;
			}
			p_hdr10_param->min_display_mastering_lum =
				p_mdc->luminance[1];
			p_hdr10_param->r_x = p_mdc->primaries[2][0];
			p_hdr10_param->r_y = p_mdc->primaries[2][1];
			p_hdr10_param->g_x = p_mdc->primaries[0][0];
			p_hdr10_param->g_y = p_mdc->primaries[0][1];
			p_hdr10_param->b_x = p_mdc->primaries[1][0];
			p_hdr10_param->b_y = p_mdc->primaries[1][1];
			p_hdr10_param->w_x = p_mdc->white_point[0];
			p_hdr10_param->w_y = p_mdc->white_point[1];
		}
	} else if (primaries_type == 1) {
		/* RGB -> RGB and dv will swap to send as GBR
		 * in send_hdmi_pkt
		 */
		if (p_hdr10_param->max_display_mastering_lum !=
		    p_mdc->luminance[0] ||
		    p_hdr10_param->min_display_mastering_lum !=
		    p_mdc->luminance[1] ||
		    p_hdr10_param->r_x != p_mdc->primaries[0][0] ||
		    p_hdr10_param->r_y != p_mdc->primaries[0][1] ||
		    p_hdr10_param->g_x != p_mdc->primaries[1][0] ||
		    p_hdr10_param->g_y != p_mdc->primaries[1][1] ||
		    p_hdr10_param->b_x != p_mdc->primaries[2][0] ||
		    p_hdr10_param->b_y != p_mdc->primaries[2][1] ||
		    p_hdr10_param->w_x != p_mdc->white_point[0] ||
		    p_hdr10_param->w_y != p_mdc->white_point[1]) {
			flag |= 1;
			p_hdr10_param->max_display_mastering_lum =
				p_mdc->luminance[0];
			if (p_mdc->luminance[0] == 0) {
				if ((debug_dolby & 1))
					pr_info("invalid max, use default\n");
				p_hdr10_param->max_display_mastering_lum = max_lum;
			}
			p_hdr10_param->min_display_mastering_lum =
				p_mdc->luminance[1];
			p_hdr10_param->r_x = p_mdc->primaries[0][0];
			p_hdr10_param->r_y = p_mdc->primaries[0][1];
			p_hdr10_param->g_x = p_mdc->primaries[1][0];
			p_hdr10_param->g_y = p_mdc->primaries[1][1];
			p_hdr10_param->b_x = p_mdc->primaries[2][0];
			p_hdr10_param->b_y = p_mdc->primaries[2][1];
			p_hdr10_param->w_x = p_mdc->white_point[0];
			p_hdr10_param->w_y = p_mdc->white_point[1];
		}
	} else if (primaries_type == 0) {
		if (is_amdv_stb_mode() &&
			p_mdc->present_flag &&
			p_mdc->primaries[0][0] == 0 &&
			p_mdc->primaries[0][1] == 0 &&
			p_mdc->primaries[1][0] == 0 &&
			p_mdc->primaries[1][1] == 0 &&
			p_mdc->primaries[2][0] == 0 &&
			p_mdc->primaries[2][1] == 0 &&
			p_mdc->white_point[0] == 0 &&
			p_mdc->white_point[1] == 0 &&
			p_mdc->luminance[0] == 0 &&
			p_mdc->luminance[1] == 0 &&
			p_cll->max_pic_average == 0 &&
			p_cll->max_content == 0) {/*stb, passthrough zero drms*/
			flag |= 1;
			p_hdr10_param->max_display_mastering_lum =
				p_mdc->luminance[0];
			p_hdr10_param->min_display_mastering_lum =
				p_mdc->luminance[1];
			p_hdr10_param->r_x = p_mdc->primaries[0][0];
			p_hdr10_param->r_y = p_mdc->primaries[0][1];
			p_hdr10_param->g_x = p_mdc->primaries[1][0];
			p_hdr10_param->g_y = p_mdc->primaries[1][1];
			p_hdr10_param->b_x = p_mdc->primaries[2][0];
			p_hdr10_param->b_y = p_mdc->primaries[2][1];
			p_hdr10_param->w_x = p_mdc->white_point[0];
			p_hdr10_param->w_y = p_mdc->white_point[1];
			p_cll->present_flag = 1;
			if (debug_dolby & 1)
				pr_info("source primary zero, passthrough\n");
		} else {
			/* GBR -> RGB as DV will swap back to GBR
			 * in send_hdmi_pkt
			 */
			if (p_hdr10_param->min_display_mastering_lum !=
			    min_lum ||
			    p_hdr10_param->max_display_mastering_lum !=
			    max_lum ||
			    p_hdr10_param->r_x != bt2020_primaries[2][0] ||
			    p_hdr10_param->r_y != bt2020_primaries[2][1] ||
			    p_hdr10_param->g_x != bt2020_primaries[0][0] ||
			    p_hdr10_param->g_y != bt2020_primaries[0][1] ||
			    p_hdr10_param->b_x != bt2020_primaries[1][0] ||
			    p_hdr10_param->b_y != bt2020_primaries[1][1] ||
			    p_hdr10_param->w_x != bt2020_white_point[0] ||
			    p_hdr10_param->w_y != bt2020_white_point[1]) {
				flag |= 2;
				p_hdr10_param->min_display_mastering_lum =
					min_lum;
				p_hdr10_param->max_display_mastering_lum =
					max_lum;
				p_hdr10_param->r_x = bt2020_primaries[2][0];
				p_hdr10_param->r_y = bt2020_primaries[2][1];
				p_hdr10_param->g_x = bt2020_primaries[0][0];
				p_hdr10_param->g_y = bt2020_primaries[0][1];
				p_hdr10_param->b_x = bt2020_primaries[1][0];
				p_hdr10_param->b_y = bt2020_primaries[1][1];
				p_hdr10_param->w_x = bt2020_white_point[0];
				p_hdr10_param->w_y = bt2020_white_point[1];
				if (debug_dolby & 1)
					pr_info("source primary invalid, use bt2020\n");
			}
		}
	}

	if (p_cll->present_flag) {
		if (p_hdr10_param->max_content_light_level
		    != p_cll->max_content ||
		    p_hdr10_param->max_frame_avg_light_level
			!= p_cll->max_pic_average)
			flag |= 4;
		if (flag & 4) {
			p_hdr10_param->max_content_light_level =
				p_cll->max_content;
			p_hdr10_param->max_frame_avg_light_level =
				p_cll->max_pic_average;
		}
	} else {
		if (p_hdr10_param->max_content_light_level != 0 ||
		    p_hdr10_param->max_frame_avg_light_level != 0) {
			p_hdr10_param->max_content_light_level = 0;
			p_hdr10_param->max_frame_avg_light_level = 0;
			flag |= 8;
		}
	}

	if ((flag && (debug_dolby & 1)) || (debug_dolby & 8)) {
		pr_dv_dbg
			("HDR10 src: primaries %d, maxcontent %d, flag %d, type %d\n",
			 p_mdc->present_flag, p_cll->present_flag,
			 flag, primaries_type);
		pr_dv_dbg
			("\tR = %04x, %04x\n",
			 p_hdr10_param->r_x, p_hdr10_param->r_y);
		pr_dv_dbg
			("\tG = %04x, %04x\n",
			 p_hdr10_param->g_x, p_hdr10_param->g_y);
		pr_dv_dbg
			("\tB = %04x, %04x\n",
			 p_hdr10_param->b_x, p_hdr10_param->b_y);
		pr_dv_dbg
			("\tW = %04x, %04x\n",
			 p_hdr10_param->w_x, p_hdr10_param->w_y);
		pr_dv_dbg
			("\tMax = %d\n",
			 p_hdr10_param->max_display_mastering_lum);
		pr_dv_dbg
			("\tMin = %d\n",
			 p_hdr10_param->min_display_mastering_lum);
		pr_dv_dbg
			("\tMCLL = %d\n",
			 p_hdr10_param->max_content_light_level);
		pr_dv_dbg
			("\tMPALL = %d\n\n",
			 p_hdr10_param->max_frame_avg_light_level);
	}
}

static int prepare_vsif_pkt(struct dv_vsif_para *vsif,
				void *p_setting,
				const struct vinfo_s *vinfo,
				enum signal_format_enum src_format)
{
	struct dovi_setting_s *setting;
	struct m_dovi_setting_s *m_setting;
	bool src_content_flag = false;

#define NEW_DOLBY_SIGNAL_TYPE 0
	if (!vsif || !vinfo || !p_setting ||
	    !vinfo->vout_device || !vinfo->vout_device->dv_info)
		return -1;

	if (multi_dv_mode) {
		m_setting = (struct m_dovi_setting_s *)p_setting;
		if (m_setting->content_info.content_type_info > 0 ||
			m_setting->content_info.l11_byte2 > 0 ||
			m_setting->content_info.l11_byte3 > 0 ||
			m_setting->content_info.white_point > 0)
			src_content_flag = true;
		if ((debug_dolby & 1))
			pr_dv_dbg("L11_md_present %d,src_content_info %d,allm %d %d,sink_dm_ver %d\n",
				      m_setting->output_vsif.l11_md_present,
				      src_content_flag, hdmi_in_allm,
				      local_allm,
				      vinfo->vout_device->dv_info->dm_version);
		/*Send L11 vsif in two cases*/
		/*case 1: cp return output_vsif with L11 that is from source meta(sink-led or ott)*/
		/*case 2: cp return content_info with L11 and sink_dm_ver >=2(hdmi in source-led)*/
		/*case 3: todo... source contains game mode and sink support dv game mode*/
		if (m_setting->dovi_ll_enable && (m_setting->output_vsif.l11_md_present ||
		    (src_content_flag && vinfo->vout_device->dv_info->dm_version >= 2))) {
			vsif->ver2_l11_flag = 1;
			vsif->vers.ver2_l11.low_latency = m_setting->dovi_ll_enable;
#if NEW_DOLBY_SIGNAL_TYPE
			if (src_format == FORMAT_DOVI || src_format == FORMAT_DOVI_LL)
				vsif->vers.ver2_l11.dobly_vision_signal = 1;/*0b0001*/
			else if (src_format == FORMAT_HDR10)
				vsif->vers.ver2_l11.dobly_vision_signal = 3;/*0b0011*/
			else if (src_format == FORMAT_HLG)
				vsif->vers.ver2_l11.dobly_vision_signal = 7;/*0b0111*/
			else/* if (src_format == FORMAT_SDR || src_format == FORMAT_SDR_2020)*/
				vsif->vers.ver2_l11.dobly_vision_signal = 5;/*0b0101*/
#else
			vsif->vers.ver2_l11.dobly_vision_signal = 1;
#endif
			vsif->vers.ver2_l11.src_dm_version = m_setting->output_vsif.src_dm_version;

			if ((debug_dolby & 1))
				pr_dv_dbg("L11 src %d, dobly_vision_signal %d %d, src_dm_ver %d\n",
					     src_format, vsif->vers.ver2_l11.dobly_vision_signal,
					     m_setting->output_vsif.dobly_vision_signal,
					     vsif->vers.ver2_l11.src_dm_version);
			if (vinfo->vout_device->dv_info &&
			    vinfo->vout_device->dv_info->sup_backlight_control &&
			    (m_setting->ext_md.avail_level_mask & EXT_MD_LEVEL_2)) {
				vsif->vers.ver2_l11.backlt_ctrl_MD_present = 1;
				vsif->vers.ver2_l11.eff_tmax_PQ_hi =
					m_setting->ext_md.level_2.target_max_pq_h & 0xf;
				vsif->vers.ver2_l11.eff_tmax_PQ_low =
					m_setting->ext_md.level_2.target_max_pq_l;
			} else {
				vsif->vers.ver2_l11.backlt_ctrl_MD_present = 0;
				vsif->vers.ver2_l11.eff_tmax_PQ_hi = 0;
				vsif->vers.ver2_l11.eff_tmax_PQ_low = 0;
			}

			if (m_setting->dovi_ll_enable &&
			    (m_setting->ext_md.avail_level_mask
				& EXT_MD_LEVEL_255)) {
				vsif->vers.ver2_l11.auxiliary_MD_present = 1;
				vsif->vers.ver2_l11.auxiliary_runmode =
					m_setting->ext_md.level_255.run_mode;
				vsif->vers.ver2_l11.auxiliary_runversion =
					m_setting->ext_md.level_255.run_version;
				vsif->vers.ver2_l11.auxiliary_debug0 =
					m_setting->ext_md.level_255.dm_debug_0;
			} else {
				vsif->vers.ver2_l11.auxiliary_MD_present = 0;
				vsif->vers.ver2_l11.auxiliary_runmode = 0;
				vsif->vers.ver2_l11.auxiliary_runversion = 0;
				vsif->vers.ver2_l11.auxiliary_debug0 = 0;
			}
			if (m_setting->output_vsif.l11_md_present) {
				vsif->vers.ver2_l11.content_type =
					m_setting->output_vsif.content_type;
				vsif->vers.ver2_l11.intended_white_point =
					m_setting->output_vsif.intended_white_point;
				vsif->vers.ver2_l11.l11_byte2 = m_setting->output_vsif.l11_byte2;
				vsif->vers.ver2_l11.l11_byte3 = m_setting->output_vsif.l11_byte3;
			} else if (src_content_flag) {
				vsif->vers.ver2_l11.content_type =
					m_setting->content_info.content_type_info;
				vsif->vers.ver2_l11.intended_white_point =
					m_setting->content_info.white_point;
				vsif->vers.ver2_l11.l11_byte2 = m_setting->content_info.l11_byte2;
				vsif->vers.ver2_l11.l11_byte3 = m_setting->content_info.l11_byte3;
			}
			if ((debug_dolby & 1))
				pr_dv_dbg("L11 send content_type %d, wp %d, 11_byte %d %d\n",
					     vsif->vers.ver2_l11.content_type,
					     vsif->vers.ver2_l11.intended_white_point,
					     vsif->vers.ver2_l11.l11_byte2,
					     vsif->vers.ver2_l11.l11_byte3);
		} else {
			vsif->ver2_l11_flag = 0;
			vsif->vers.ver2.low_latency = m_setting->dovi_ll_enable;
#if NEW_DOLBY_SIGNAL_TYPE
			if (src_format == FORMAT_DOVI || src_format == FORMAT_DOVI_LL)
				vsif->vers.ver2.dobly_vision_signal = 1;/*0b0001*/
			else if (src_format == FORMAT_HDR10)
				vsif->vers.ver2.dobly_vision_signal = 3;/*0b0011*/
			else if (src_format == FORMAT_HLG)
				vsif->vers.ver2.dobly_vision_signal = 7;/*0b0111*/
			else/* if (src_format == FORMAT_SDR || src_format == FORMAT_SDR_2020)*/
				vsif->vers.ver2.dobly_vision_signal = 5;/*0b0101*/
#else
			vsif->vers.ver2.dobly_vision_signal = 1;
#endif
			vsif->vers.ver2.src_dm_version = m_setting->output_vsif.src_dm_version;

			if ((debug_dolby & 1))
				pr_dv_dbg("src %d, signal %d %d, src_dm_ver %d\n",
					     src_format, vsif->vers.ver2.dobly_vision_signal,
					     m_setting->output_vsif.dobly_vision_signal,
					     vsif->vers.ver2.src_dm_version);
			if (vinfo->vout_device->dv_info &&
			    vinfo->vout_device->dv_info->sup_backlight_control &&
			    (m_setting->ext_md.avail_level_mask & EXT_MD_LEVEL_2)) {
				vsif->vers.ver2.backlt_ctrl_MD_present = 1;
				vsif->vers.ver2.eff_tmax_PQ_hi =
					m_setting->ext_md.level_2.target_max_pq_h & 0xf;
				vsif->vers.ver2.eff_tmax_PQ_low =
					m_setting->ext_md.level_2.target_max_pq_l;
			} else {
				vsif->vers.ver2.backlt_ctrl_MD_present = 0;
				vsif->vers.ver2.eff_tmax_PQ_hi = 0;
				vsif->vers.ver2.eff_tmax_PQ_low = 0;
			}

			if (m_setting->dovi_ll_enable &&
			    (m_setting->ext_md.avail_level_mask
				& EXT_MD_LEVEL_255)) {
				vsif->vers.ver2.auxiliary_MD_present = 1;
				vsif->vers.ver2.auxiliary_runmode =
					m_setting->ext_md.level_255.run_mode;
				vsif->vers.ver2.auxiliary_runversion =
					m_setting->ext_md.level_255.run_version;
				vsif->vers.ver2.auxiliary_debug0 =
					m_setting->ext_md.level_255.dm_debug_0;
			} else {
				vsif->vers.ver2.auxiliary_MD_present = 0;
				vsif->vers.ver2.auxiliary_runmode = 0;
				vsif->vers.ver2.auxiliary_runversion = 0;
				vsif->vers.ver2.auxiliary_debug0 = 0;
			}
		}
	} else {
		setting = (struct dovi_setting_s *)p_setting;
		vsif->vers.ver2.low_latency =
			setting->dovi_ll_enable;
#if NEW_DOLBY_SIGNAL_TYPE
		if (src_format == FORMAT_DOVI || src_format == FORMAT_DOVI_LL)
			vsif->vers.ver2.dobly_vision_signal = 1;/*0b0001*/
		else if (src_format == FORMAT_HDR10)
			vsif->vers.ver2.dobly_vision_signal = 3;/*0b0011*/
		else if (src_format == FORMAT_HLG)
			vsif->vers.ver2.dobly_vision_signal = 7;/*0b0111*/
		else/* if (src_format == FORMAT_SDR || src_format == FORMAT_SDR_2020)*/
			vsif->vers.ver2.dobly_vision_signal = 5;/*0b0101*/
#else
		vsif->vers.ver2.dobly_vision_signal = 1;
#endif
		if ((debug_dolby & 2))
			pr_dv_dbg("src %d, dobly_vision_signal %d\n",
				     src_format, vsif->vers.ver2.dobly_vision_signal);

		if (vinfo->vout_device->dv_info &&
		    vinfo->vout_device->dv_info->sup_backlight_control &&
		    (setting->ext_md.avail_level_mask & EXT_MD_LEVEL_2)) {
			vsif->vers.ver2.backlt_ctrl_MD_present = 1;
			vsif->vers.ver2.eff_tmax_PQ_hi =
				setting->ext_md.level_2.target_max_pq_h & 0xf;
			vsif->vers.ver2.eff_tmax_PQ_low =
				setting->ext_md.level_2.target_max_pq_l;
		} else {
			vsif->vers.ver2.backlt_ctrl_MD_present = 0;
			vsif->vers.ver2.eff_tmax_PQ_hi = 0;
			vsif->vers.ver2.eff_tmax_PQ_low = 0;
		}

		if (setting->dovi_ll_enable &&
		    (setting->ext_md.avail_level_mask
			& EXT_MD_LEVEL_255)) {
			vsif->vers.ver2.auxiliary_MD_present = 1;
			vsif->vers.ver2.auxiliary_runmode =
				setting->ext_md.level_255.run_mode;
			vsif->vers.ver2.auxiliary_runversion =
				setting->ext_md.level_255.run_version;
			vsif->vers.ver2.auxiliary_debug0 =
				setting->ext_md.level_255.dm_debug_0;
		} else {
			vsif->vers.ver2.auxiliary_MD_present = 0;
			vsif->vers.ver2.auxiliary_runmode = 0;
			vsif->vers.ver2.auxiliary_runversion = 0;
			vsif->vers.ver2.auxiliary_debug0 = 0;
		}
	}

	return 0;
}

unsigned char vsif_emp[32];
static int prepare_emp_vsif_pkt
	(unsigned char *vsif_PB,
	void *p_setting,
	const struct vinfo_s *vinfo)
{
	struct dovi_setting_s *setting;
	struct m_dovi_setting_s *m_setting;

	if (!vinfo || !p_setting ||
		!vinfo->vout_device || !vinfo->vout_device->dv_info)
		return -1;
	memset(vsif_PB, 0, 32);

	if (multi_dv_mode) {
		m_setting = (struct m_dovi_setting_s *)p_setting;

		/* low_latency */
		if (m_setting->dovi_ll_enable)
			vsif_PB[0] = 1 << 0;
		/* dovi_signal_type */
		vsif_PB[0] |= 1 << 1;
		/* source_dm_version */
		vsif_PB[0] |= 3 << 5;

		if (vinfo->vout_device->dv_info->sup_backlight_control &&
			(m_setting->ext_md.avail_level_mask & EXT_MD_LEVEL_2)) {
			vsif_PB[1] |= 1 << 7;
			vsif_PB[1] |=
				m_setting->ext_md.level_2.target_max_pq_h & 0xf;
			vsif_PB[2] =
				m_setting->ext_md.level_2.target_max_pq_l;
		}

		if (m_setting->dovi_ll_enable &&
		    (m_setting->ext_md.avail_level_mask & EXT_MD_LEVEL_255)) {
			vsif_PB[1] |= 1 << 6;
			vsif_PB[3] = m_setting->ext_md.level_255.run_mode;
			vsif_PB[4] = m_setting->ext_md.level_255.run_version;
			vsif_PB[5] = m_setting->ext_md.level_255.dm_debug_0;
		}
	} else {
		setting = (struct dovi_setting_s *)p_setting;

		/* low_latency */
		if (setting->dovi_ll_enable)
			vsif_PB[0] = 1 << 0;
		/* dovi_signal_type */
		vsif_PB[0] |= 1 << 1;
		/* source_dm_version */
		vsif_PB[0] |= 3 << 5;

		if (vinfo->vout_device->dv_info->sup_backlight_control &&
			(setting->ext_md.avail_level_mask & EXT_MD_LEVEL_2)) {
			vsif_PB[1] |= 1 << 7;
			vsif_PB[1] |=
				setting->ext_md.level_2.target_max_pq_h & 0xf;
			vsif_PB[2] =
				setting->ext_md.level_2.target_max_pq_l;
		}

		if (setting->dovi_ll_enable &&
		    (setting->ext_md.avail_level_mask & EXT_MD_LEVEL_255)) {
			vsif_PB[1] |= 1 << 6;
			vsif_PB[3] = setting->ext_md.level_255.run_mode;
			vsif_PB[4] = setting->ext_md.level_255.run_version;
			vsif_PB[5] = setting->ext_md.level_255.dm_debug_0;
		}
	}
	return 0;
}

static int notify_vd_signal_to_amvideo(struct vd_signal_info_s *vd_signal)
{
	static int pre_signal = -1;
#ifdef CONFIG_AMLOGIC_MEDIA_VIDEO
	if (pre_signal != vd_signal->signal_type) {
		vd_signal->vd1_signal_type =
			SIGNAL_DOVI;
		vd_signal->vd2_signal_type =
			SIGNAL_DOVI;
		amvideo_notifier_call_chain
			(AMVIDEO_UPDATE_SIGNAL_MODE,
			(void *)vd_signal);
	}
#endif
	pre_signal = vd_signal->signal_type;
	return 0;
}

/* #define HDMI_SEND_ALL_PKT */
static u32 last_dst_format = FORMAT_SDR;

static bool send_hdmi_pkt
	(enum signal_format_enum src_format,
	 enum signal_format_enum dst_format,
	 const struct vinfo_s *vinfo, struct vframe_s *vf)
{
	struct hdr10_infoframe *p_hdr;
	int i;
	bool flag = false;
	static int sdr_transition_delay;
	struct vd_signal_info_s vd_signal;
	bool dovi_ll_enable = false;
	bool diagnostic_enable = false;

	if ((debug_dolby & 2))
		pr_dv_dbg("[%s]src_format %d,dst %d,last %d:,vf %p\n",
			     __func__, src_format, dst_format,
			     last_dst_format, vf);

	if (dst_format == FORMAT_HDR10) {
		sdr_transition_delay = 0;
		if (multi_dv_mode)
			p_hdr = &m_dovi_setting.hdr_info;
		else
			p_hdr = &dovi_setting.hdr_info;
		hdr10_data.features =
			  (1 << 29)	/* video available */
			| (5 << 26)	/* unspecified */
			| (0 << 25)	/* limit */
			| (1 << 24)	/* color available */
			| (9 << 16)	/* bt2020 */
			| (0x10 << 8)	/* bt2020-10 */
			| (10 << 0);/* bt2020c */
		if (src_format != FORMAT_HDR10PLUS) {
			/* keep as r,g,b when src not HDR+ */
			if (hdr10_data.primaries[0][0] !=
				((p_hdr->primaries_x_0_msb << 8)
				| p_hdr->primaries_x_0_lsb))
				flag = true;
			hdr10_data.primaries[0][0] =
				(p_hdr->primaries_x_0_msb << 8)
				| p_hdr->primaries_x_0_lsb;

			if (hdr10_data.primaries[0][1] !=
				((p_hdr->primaries_y_0_msb << 8)
				| p_hdr->primaries_y_0_lsb))
				flag = true;
			hdr10_data.primaries[0][1] =
				(p_hdr->primaries_y_0_msb << 8)
				| p_hdr->primaries_y_0_lsb;

			if (hdr10_data.primaries[1][0] !=
				((p_hdr->primaries_x_1_msb << 8)
				| p_hdr->primaries_x_1_lsb))
				flag = true;
			hdr10_data.primaries[1][0] =
				(p_hdr->primaries_x_1_msb << 8)
				| p_hdr->primaries_x_1_lsb;

			if (hdr10_data.primaries[1][1] !=
				((p_hdr->primaries_y_1_msb << 8)
				| p_hdr->primaries_y_1_lsb))
				flag = true;
			hdr10_data.primaries[1][1] =
				(p_hdr->primaries_y_1_msb << 8)
				| p_hdr->primaries_y_1_lsb;
			if (hdr10_data.primaries[2][0] !=
				((p_hdr->primaries_x_2_msb << 8)
				| p_hdr->primaries_x_2_lsb))
				flag = true;
			hdr10_data.primaries[2][0] =
				(p_hdr->primaries_x_2_msb << 8)
				| p_hdr->primaries_x_2_lsb;

			if (hdr10_data.primaries[2][1] !=
				((p_hdr->primaries_y_2_msb << 8)
				| p_hdr->primaries_y_2_lsb))
				flag = true;
			hdr10_data.primaries[2][1] =
				(p_hdr->primaries_y_2_msb << 8)
				| p_hdr->primaries_y_2_lsb;
		} else {
			/* need invert to g,b,r */
			if (hdr10_data.primaries[0][0] !=
				((p_hdr->primaries_x_1_msb << 8)
				| p_hdr->primaries_x_1_lsb))
				flag = true;
			hdr10_data.primaries[0][0] =
				(p_hdr->primaries_x_1_msb << 8)
				| p_hdr->primaries_x_1_lsb;

			if (hdr10_data.primaries[0][1] !=
				((p_hdr->primaries_y_1_msb << 8)
				| p_hdr->primaries_y_1_lsb))
				flag = true;
			hdr10_data.primaries[0][1] =
				(p_hdr->primaries_y_1_msb << 8)
				| p_hdr->primaries_y_1_lsb;

			if (hdr10_data.primaries[1][0] !=
				((p_hdr->primaries_x_2_msb << 8)
				| p_hdr->primaries_x_2_lsb))
				flag = true;
			hdr10_data.primaries[1][0] =
				(p_hdr->primaries_x_2_msb << 8)
				| p_hdr->primaries_x_2_lsb;

			if (hdr10_data.primaries[1][1] !=
				((p_hdr->primaries_y_2_msb << 8)
				| p_hdr->primaries_y_2_lsb))
				flag = true;
			hdr10_data.primaries[1][1] =
				(p_hdr->primaries_y_2_msb << 8)
				| p_hdr->primaries_y_2_lsb;
			if (hdr10_data.primaries[2][0] !=
				((p_hdr->primaries_x_0_msb << 8)
				| p_hdr->primaries_x_0_lsb))
				flag = true;
			hdr10_data.primaries[2][0] =
				(p_hdr->primaries_x_0_msb << 8)
				| p_hdr->primaries_x_0_lsb;

			if (hdr10_data.primaries[2][1] !=
				((p_hdr->primaries_y_0_msb << 8)
				| p_hdr->primaries_y_0_lsb))
				flag = true;
			hdr10_data.primaries[2][1] =
				(p_hdr->primaries_y_0_msb << 8)
				| p_hdr->primaries_y_0_lsb;
		}

		if (hdr10_data.white_point[0] !=
			((p_hdr->white_point_x_msb << 8)
			| p_hdr->white_point_x_lsb))
			flag = true;
		hdr10_data.white_point[0] =
			(p_hdr->white_point_x_msb << 8)
			| p_hdr->white_point_x_lsb;

		if (hdr10_data.white_point[1] !=
			((p_hdr->white_point_y_msb << 8)
			| p_hdr->white_point_y_lsb))
			flag = true;
		hdr10_data.white_point[1] =
			(p_hdr->white_point_y_msb << 8)
			| p_hdr->white_point_y_lsb;

		if (hdr10_data.luminance[0] !=
			((p_hdr->max_display_mastering_lum_msb << 8)
			| p_hdr->max_display_mastering_lum_lsb))
			flag = true;
		hdr10_data.luminance[0] =
			(p_hdr->max_display_mastering_lum_msb << 8)
			| p_hdr->max_display_mastering_lum_lsb;

		if (hdr10_data.luminance[1] !=
			((p_hdr->min_display_mastering_lum_msb << 8)
			| p_hdr->min_display_mastering_lum_lsb))
			flag = true;
		hdr10_data.luminance[1] =
			(p_hdr->min_display_mastering_lum_msb << 8)
			| p_hdr->min_display_mastering_lum_lsb;

		if (hdr10_data.max_content !=
			((p_hdr->max_content_light_level_msb << 8)
			| p_hdr->max_content_light_level_lsb))
			flag = true;
		hdr10_data.max_content =
			(p_hdr->max_content_light_level_msb << 8)
			| p_hdr->max_content_light_level_lsb;

		if (hdr10_data.max_frame_average !=
			((p_hdr->max_frame_avg_light_level_msb << 8)
			| p_hdr->max_frame_avg_light_level_lsb))
			flag = true;
		hdr10_data.max_frame_average =
			(p_hdr->max_frame_avg_light_level_msb << 8)
			| p_hdr->max_frame_avg_light_level_lsb;

		if (vinfo && vinfo->vout_device &&
		    vinfo->vout_device->fresh_tx_hdr_pkt)
			vinfo->vout_device->fresh_tx_hdr_pkt(&hdr10_data);
#ifdef HDMI_SEND_ALL_PKT
		if (vinfo && vinfo->vout_device &&
			vinfo->vout_device->fresh_tx_vsif_pkt) {
#ifdef V2_4_3
			send_emp(EOTF_T_NULL,
				YUV422_BIT12,
				(struct dv_vsif_para *)NULL,
				(unsigned char *)NULL,
				0,
				true);
#else
			vinfo->vout_device->fresh_tx_vsif_pkt
				(0, 0, NULL, true);
#endif
		}
#endif
		if (last_dst_format != FORMAT_HDR10 ||
		    (dolby_vision_flags & FLAG_FORCE_HDMI_PKT))
			pr_dv_dbg("send hdmi pkt: HDR10\n");

		last_dst_format = dst_format;
		vd_signal.signal_type = SIGNAL_HDR10;
		notify_vd_signal_to_amvideo(&vd_signal);
		if ((flag && (debug_dolby & 1)) || (debug_dolby & 8)) {
			pr_dv_dbg("Output info for hdr10 changed:\n");
			for (i = 0; i < 3; i++)
				pr_dv_dbg("\tprimaries[%1d] = %04x, %04x\n",
					     i,
					     hdr10_data.primaries[i][0],
					     hdr10_data.primaries[i][1]);
			pr_dv_dbg("\twhite_point = %04x, %04x\n",
				hdr10_data.white_point[0],
				hdr10_data.white_point[1]);
			pr_dv_dbg("\tMax = %d\n",
				hdr10_data.luminance[0]);
			pr_dv_dbg("\tMin = %d\n",
				hdr10_data.luminance[1]);
			pr_dv_dbg("\tMCLL = %d\n",
				hdr10_data.max_content);
			pr_dv_dbg("\tMPALL = %d\n\n",
				hdr10_data.max_frame_average);
		}
	} else if (dst_format == FORMAT_DOVI) {
		struct dv_vsif_para vsif;

		sdr_transition_delay = 0;
		memset(&vsif, 0, sizeof(vsif));
		if (vinfo) {
			if (multi_dv_mode) {
				prepare_vsif_pkt
					(&vsif, &m_dovi_setting, vinfo, src_format);
				prepare_emp_vsif_pkt
					(vsif_emp, &m_dovi_setting, vinfo);
			} else {
				prepare_vsif_pkt
					(&vsif, &dovi_setting, vinfo, src_format);
				prepare_emp_vsif_pkt(vsif_emp, &dovi_setting, vinfo);
			}
		}
#ifdef HDMI_SEND_ALL_PKT
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
		if (vinfo && vinfo->vout_device &&
		    vinfo->vout_device->fresh_tx_hdr_pkt)
			vinfo->vout_device->fresh_tx_hdr_pkt(&hdr10_data);
#endif
		if (multi_dv_mode) {
			dovi_ll_enable = m_dovi_setting.dovi_ll_enable;
			diagnostic_enable = m_dovi_setting.diagnostic_enable;
		} else {
			dovi_ll_enable = dovi_setting.dovi_ll_enable;
			diagnostic_enable = dovi_setting.diagnostic_enable;
		}

		if (vinfo && vinfo->vout_device &&
		    vinfo->vout_device->fresh_tx_vsif_pkt) {
			if (dovi_ll_enable) {
#ifdef V2_4_3
				send_emp(EOTF_T_LL_MODE,
					diagnostic_enable
					? RGB_10_12BIT : YUV422_BIT12,
					&vsif, vsif_emp, 15, false);
#else
				vinfo->vout_device->fresh_tx_vsif_pkt
					(EOTF_T_LL_MODE,
					diagnostic_enable
					? RGB_10_12BIT : YUV422_BIT12,
					&vsif, false);
#endif
			} else {
#ifdef V2_4_3
				send_emp(EOTF_T_DOLBYVISION,
				dolby_vision_mode ==
				AMDV_OUTPUT_MODE_IPT_TUNNEL
				? RGB_8BIT : YUV422_BIT12,
				NULL,
				hdmi_metadata,
				hdmi_metadata_size,
				false);
#else
				vinfo->vout_device->fresh_tx_vsif_pkt
				(EOTF_T_DOLBYVISION,
				dolby_vision_mode ==
				AMDV_OUTPUT_MODE_IPT_TUNNEL
				? RGB_8BIT : YUV422_BIT12, &vsif,
				false);
#endif
			}
		}
		if (last_dst_format != FORMAT_DOVI ||
		    (dolby_vision_flags & FLAG_FORCE_HDMI_PKT))
			pr_dv_dbg("send hdmi pkt: %s\n",
				     dovi_ll_enable ? "LL" : "DV");
		last_dst_format = dst_format;
		vd_signal.signal_type = SIGNAL_DOVI;
		notify_vd_signal_to_amvideo(&vd_signal);
	} else if (last_dst_format != dst_format) {
		if (last_dst_format == FORMAT_HDR10) {
			sdr_transition_delay = 0;
			if (!(vf && (is_hlg_frame(vf) ||
				is_hdr10plus_frame(vf) ||
				is_primesl_frame(vf)))) {
				/* TODO: double check if need add prime sl case */
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
				if (vinfo && vinfo->vout_device &&
				    vinfo->vout_device->fresh_tx_hdr_pkt) {
					pr_dv_dbg("fresh tx_hdr_pkt zero\n");
					vinfo->vout_device->fresh_tx_hdr_pkt
					(&hdr10_data);
					last_dst_format = dst_format;
				}
			}
		} else if (last_dst_format == FORMAT_DOVI) {
			if (vinfo && vinfo->vout_device &&
				vinfo->vout_device->fresh_tx_vsif_pkt) {
				if (vf && (is_hlg_frame(vf) ||
					   is_hdr10plus_frame(vf) ||
					   is_cuva_frame(vf) ||
					   is_primesl_frame(vf))) {
					/* TODO: double check if need add prime sl case */
					/* HLG/HDR10+/CUVA/PRIMESL case: first switch to SDR
					 * immediately.
					 */
					pr_dv_dbg
				("send pkt: HDR10+/HLG/RPIMESL: signal SDR first\n");
#ifdef V2_4_3
					send_emp(EOTF_T_NULL,
					YUV422_BIT12,
					(struct dv_vsif_para *)NULL,
					(unsigned char *)NULL,
					0,
					true);
#else
					vinfo->vout_device->fresh_tx_vsif_pkt
						(0, 0, NULL, true);
#endif

					last_dst_format = dst_format;
					sdr_transition_delay = 0;
				} else if (sdr_transition_delay >=
					   MAX_TRANSITION_DELAY) {
					pr_dv_dbg
				("send pkt: VSIF disabled, signal SDR\n");
#ifdef V2_4_3
					send_emp(EOTF_T_NULL,
					YUV422_BIT12,
					(struct dv_vsif_para *)NULL,
					(unsigned char *)NULL,
					0,
					true);
#else
					vinfo->vout_device->fresh_tx_vsif_pkt
						(0, 0, NULL, true);
#endif
					last_dst_format = dst_format;
					sdr_transition_delay = 0;
				} else {
					if (sdr_transition_delay == 0) {
						pr_dv_dbg("send pkt: disable Dovi/H14b VSIF\n");

#ifdef V2_4_3
					send_emp(EOTF_T_NULL,
					YUV422_BIT12,
					(struct dv_vsif_para *)NULL,
					(unsigned char *)NULL,
					0,
					false);
#else
					vinfo->vout_device->fresh_tx_vsif_pkt
						(0, 0, NULL, false);
#endif
					}
					sdr_transition_delay++;
				}
			}
		}
		vd_signal.signal_type = SIGNAL_SDR;
		notify_vd_signal_to_amvideo(&vd_signal);
	}
	if (dolby_vision_flags & FLAG_FORCE_HDMI_PKT)
		dolby_vision_flags &= ~FLAG_FORCE_HDMI_PKT;
	return flag;
}

static void send_hdmi_pkt_ahead
	(enum signal_format_enum dst_format,
	 const struct vinfo_s *vinfo)
{
	bool dovi_ll_enable = false;
	bool diagnostic_enable = false;

	if ((dolby_vision_flags & FLAG_FORCE_DV_LL) ||
	    dolby_vision_ll_policy == DOLBY_VISION_LL_YUV422 ||
	    dolby_vision_ll_policy == DOLBY_VISION_LL_RGB444) {
		dovi_ll_enable = true;
		if (dolby_vision_ll_policy == DOLBY_VISION_LL_RGB444)
			diagnostic_enable = true;
	}

	if (dst_format == FORMAT_DOVI) {
		struct dv_vsif_para vsif;

		memset(&vsif, 0, sizeof(vsif));
		vsif.vers.ver2.low_latency = dovi_ll_enable;
		vsif.vers.ver2.dobly_vision_signal = 1;
		vsif.vers.ver2.backlt_ctrl_MD_present = 0;
		vsif.vers.ver2.eff_tmax_PQ_hi = 0;
		vsif.vers.ver2.eff_tmax_PQ_low = 0;
		vsif.vers.ver2.auxiliary_MD_present = 0;
		vsif.vers.ver2.auxiliary_runmode = 0;
		vsif.vers.ver2.auxiliary_runversion = 0;
		vsif.vers.ver2.auxiliary_debug0 = 0;

		if (vinfo && vinfo->vout_device &&
			vinfo->vout_device->fresh_tx_vsif_pkt) {
			if (dovi_ll_enable)
				vinfo->vout_device->fresh_tx_vsif_pkt
					(EOTF_T_DV_AHEAD,
					diagnostic_enable
					? RGB_10_12BIT : YUV422_BIT12,
					&vsif, false);
			else
				vinfo->vout_device->fresh_tx_vsif_pkt
					(EOTF_T_DV_AHEAD,
					(amdv_target_mode ==
					dovi_ll_enable)
					? YUV422_BIT12 : RGB_8BIT, &vsif,
					false);
		}
		last_dst_format = dst_format;
		pr_dv_dbg("send_hdmi_pkt ahead: %s\n",
			     dovi_ll_enable ? "LL" : "DV");
	}
}

static u32 null_vf_cnt;
static bool video_off_handled;
static int is_video_output_off(struct vframe_s *vf)
{
	if ((READ_VPP_DV_REG(VPP_MISC) & (1 << 10)) == 0) {
		/*Not reset frame0/1 clipping*/
		/*when core off to avoid green garbage*/
		if (is_aml_tvmode() && !vf &&
		    amdv_on_count <= amdv_run_mode_delay)
			return 0;
		if (!vf)
			null_vf_cnt++;
		else
			null_vf_cnt = 0;
		if (null_vf_cnt > amdv_wait_delay + 1) {
			null_vf_cnt = 0;
			return 1;
		}
		if (video_off_handled)
			return 1;
	} else {
		video_off_handled = 0;
	}
	return 0;
}

bool is_dv_standard_es(int dvel, int mflag, int width)
{
	if (dolby_vision_profile == 4 &&
	    dvel == 1 && mflag == 0 &&
	    width >= 3840)
		return false;
	else
		return true;
}

static int prepare_dv_meta
	(struct md_reg_ipcore3 *out,
	unsigned char *p_md, int size)
{
	int i, shift;
	u32 value;
	unsigned char *p;
	u32 *p_out;

	/* calculate md size in double word */
	out->size = 1 + (size - 1 + 3) / 4;

	/* write metadata into register structure*/
	p = p_md;
	p_out = out->raw_metadata;
	*p_out++ = (size << 8) | p[0];
	shift = 0; value = 0;
	for (i = 1; i < size; i++) {
		value = value | (p[i] << shift);
		shift += 8;
		if (shift == 32) {
			*p_out++ = value;
			shift = 0; value = 0;
		}
	}
	if (shift != 0)
		*p_out++ = value;

	return out->size;
}

#define VSEM_BUF_SIZE 0x1000
#define VSIF_PAYLOAD_LEN   24
#define VSEM_PKT_BODY_SIZE   28
#define VSEM_FIRST_PKT_EDR_DATA_SIZE   15

struct vsem_data_pkt {
	unsigned char pkt_type;
	unsigned char hb1;
	unsigned char seq_index;
	unsigned char pb[VSEM_PKT_BODY_SIZE];
};

#define HEADER_MASK_FIRST        0x80
#define HEADER_MASK_LAST         0x40

unsigned short get_data_len(struct vsem_data_pkt *pkt)
{
	return (pkt->pb[5] << 8) + pkt->pb[6];
}

unsigned char get_vsem_byte(u8 md_byte, u8 mask)
{
	u8 field_lsb_off = 0;
	u8 field_mask = mask;

	while (!(field_mask & 1)) {
		field_mask >>= 1;
		field_lsb_off++;
	}

	return ((md_byte & mask) >> field_lsb_off);
}

int vsem_check(unsigned char *control_data, unsigned char *vsem_payload)
{
	struct vsem_data_pkt *cur_pkt;
	unsigned char *p_buf = NULL;
	u16 cur_len = 0;
	int rv = 0;
	u32 crc;
	u32 data_length = 0;
	bool last = false;

	cur_pkt = (struct vsem_data_pkt *)(control_data);
	p_buf = vsem_payload;

	if (!get_vsem_byte(cur_pkt->hb1, HEADER_MASK_FIRST)) {
		rv = -1;
		return rv;
	}
	data_length = get_data_len(cur_pkt);
	memcpy(p_buf, &cur_pkt->pb[0], VSEM_PKT_BODY_SIZE);
	cur_len = VSEM_PKT_BODY_SIZE - 7;
	p_buf += VSEM_PKT_BODY_SIZE;
	while (!last) {
		cur_pkt += 1;
		if (get_vsem_byte(cur_pkt->hb1, HEADER_MASK_LAST)) {
			memcpy(p_buf, &cur_pkt->pb[0],
				data_length - cur_len);
			last = true;
		} else {
			memcpy(p_buf, &cur_pkt->pb[0],
				VSEM_PKT_BODY_SIZE);
			cur_len += VSEM_PKT_BODY_SIZE;
			p_buf += VSEM_PKT_BODY_SIZE;
		}
		if (cur_len >= data_length) {
			pr_info("didn't find last pkt! %d >= %d\n",
				cur_len, data_length);
			break;
		}
		if (cur_len >= VSEM_BUF_SIZE - VSEM_PKT_BODY_SIZE) {
			pr_info("vsem payload buffer is too small!\n");
			break;
		}
	}

	/* crc check */
	crc = get_crc32(vsem_payload + 13, data_length - 6);
	if (crc != 0) {
		pr_info("Error: crc error while reading control data\n");
		rv = -1;
		return rv;
	}

	return rv;
}

/*for hdmi cert: not run cp when vf not change*/
/*sometimes  even if frame is same but  frame crc changed, maybe ucd or rx not stable,*/
/*so need to check  few more frames*/
bool check_vf_changed(struct vframe_s *vf)
{
#define MAX_VF_CRC_CHECK_COUNT 4

	static u32 new_vf_crc;
	static u32 vf_crc_repeat_cnt;
	bool changed = false;

	if (!vf)
		return true;

	if (debug_dolby & 1)
		pr_dv_dbg("vf %p, crc %x, last valid crc %x, rpt %d\n",
				vf, vf->crc, last_vf_valid_crc,
				vf_crc_repeat_cnt);

	if (vf->crc == 0) {
		/*invalid crc, maybe vdin dropped last frame*/
		return changed;
	}
	if (last_vf_valid_crc == 0) {
		last_vf_valid_crc = vf->crc;
		changed = true;
		hdmi_frame_count = 0;
		pr_dv_dbg("first frame\n");
	} else if (vf->crc != last_vf_valid_crc) {
		if (vf->crc != new_vf_crc)
			vf_crc_repeat_cnt = 0;
		else
			++vf_crc_repeat_cnt;

		if (vf_crc_repeat_cnt >= MAX_VF_CRC_CHECK_COUNT) {
			vf_crc_repeat_cnt = 0;
			last_vf_valid_crc = vf->crc;
			changed = true;
			++hdmi_frame_count;
			pr_dv_dbg("new frame\n");
		}
	} else {
		vf_crc_repeat_cnt = 0;
	}
	new_vf_crc = vf->crc;
	return changed;
}

static u32 last_total_md_size;
static u32 last_total_comp_size;

int amdv_parse_metadata_v1(struct vframe_s *vf,
					      u8 toggle_mode,
					      bool bypass_release,
					      bool drop_flag)
{
	const struct vinfo_s *vinfo = get_current_vinfo();
	struct vframe_s *el_vf;
	struct provider_aux_req_s req;
	struct provider_aux_req_s el_req;
	int flag;
	enum signal_format_enum src_format = FORMAT_SDR;
	enum signal_format_enum tmp_fmt = FORMAT_SDR;
	enum signal_format_enum check_format;
	enum signal_format_enum dst_format;
	enum signal_format_enum cur_src_format;
	enum signal_format_enum cur_dst_format;
	int total_md_size = 0;
	int total_comp_size = 0;
	bool el_flag = 0;
	bool el_halfsize_flag = 1;
	u32 w = 0xffff;
	u32 h = 0xffff;
	int meta_flag_bl = 1;
	int meta_flag_el = 1;
	int src_chroma_format = 0;
	int src_bdp = 12;
	bool video_frame = false;
	int i;
	struct vframe_master_display_colour_s *p_mdc;
	unsigned int current_mode = dolby_vision_mode;
	u32 target_lumin_max = 0;
	enum input_mode_enum input_mode = IN_MODE_OTT;
	enum priority_mode_enum pri_mode = V_PRIORITY;
	u32 graphic_min = 50; /* 0.0001 */
	u32 graphic_max = 100; /* 1 */
	int ret_flags = 0;
	static int bypass_frame = -1;
	static int last_current_format;
	int ret = -1;
	bool mel_flag = false;
	int vsem_size = 0;
	int vsem_if_size = 0;
	bool dump_emp = false;
	bool dv_vsem = false;
	bool hdr10_src_primary_changed = false;
	unsigned long time_use = 0;
	struct timeval start;
	struct timeval end;
	char *pic_mode;
	bool run_control_path = true;
	bool vf_changed = true;
	char *dvel_provider = NULL;
	struct ambient_cfg_s *p_ambient = NULL;

	memset(&req, 0, (sizeof(struct provider_aux_req_s)));
	memset(&el_req, 0, (sizeof(struct provider_aux_req_s)));

	if (vf) {
		video_frame = true;
		w = (vf->type & VIDTYPE_COMPRESS) ?
			vf->compWidth : vf->width;
		h = (vf->type & VIDTYPE_COMPRESS) ?
			vf->compHeight : vf->height;
	}

	if (is_aml_tvmode() && vf &&
	    vf->source_type == VFRAME_SOURCE_TYPE_HDMI) {
		req.vf = vf;
		req.bot_flag = 0;
		req.aux_buf = NULL;
		req.aux_size = 0;
		req.dv_enhance_exist = 0;
		req.low_latency = 0;
		vf_notify_provider_by_name("dv_vdin",
					   VFRAME_EVENT_RECEIVER_GET_AUX_DATA,
					   (void *)&req);
		input_mode = IN_MODE_HDMI;

		if ((dolby_vision_flags & FLAG_CERTIFICATION) && enable_vf_check)
			vf_changed = check_vf_changed(vf);

		/* meta */
		if ((dolby_vision_flags & FLAG_RX_EMP_VSEM) &&
			vf->emp.size > 0) {
			vsem_size = vf->emp.size * VSEM_PKT_SIZE;
			memcpy(vsem_if_buf, vf->emp.addr, vsem_size);
			if (vsem_if_buf[0] == 0x7f &&
			    vsem_if_buf[10] == 0x46 &&
			    vsem_if_buf[11] == 0xd0) {
				dv_vsem = true;
				if (!vsem_check(vsem_if_buf, vsem_md_buf)) {
					vsem_if_size = vsem_size;
					if (!vsem_md_buf[10]) {
						req.aux_buf =
							&vsem_md_buf[13];
						req.aux_size =
							(vsem_md_buf[5] << 8)
							+ vsem_md_buf[6]
							- 6 - 4;
						/* cancel vsem, use md */
						/* vsem_if_size = 0; */
					} else {
						req.low_latency = 1;
					}
				} else {
					/* emp error, use previous md */
					pr_dv_dbg("EMP packet error %d\n",
						vf->emp.size);
					dump_emp = true;
					vsem_if_size = 0;
					req.aux_buf = NULL;
					req.aux_size =
						last_total_md_size;
				}
			} else if (debug_dolby & 4) {
				pr_dv_dbg("EMP packet not DV vsem %d\n",
					vf->emp.size);
				dump_emp = true;
			}
			if ((debug_dolby & 4) || dump_emp) {
				pr_info("vsem pkt count = %d\n", vf->emp.size);
				for (i = 0; i < vsem_size; i += 8) {
					pr_info("%02x %02x %02x %02x %02x %02x %02x %02x\n",
						vsem_if_buf[i],
						vsem_if_buf[i + 1],
						vsem_if_buf[i + 2],
						vsem_if_buf[i + 3],
						vsem_if_buf[i + 4],
						vsem_if_buf[i + 5],
						vsem_if_buf[i + 6],
						vsem_if_buf[i + 7]);
				}
			}
		}

		/* w/t vsif and no dv_vsem */
		if (vf->vsif.size && !dv_vsem) {
			memset(vsem_if_buf, 0, VSEM_IF_BUF_SIZE);
			memcpy(vsem_if_buf, vf->vsif.addr, vf->vsif.size);
			vsem_if_size = vf->vsif.size;
			if (debug_dolby & 4) {
				pr_info("vsif size = %d\n", vf->vsif.size);
				for (i = 0; i < vsem_if_size; i += 8) {
					pr_info("%02x %02x %02x %02x %02x %02x %02x %02x\n",
						vsem_if_buf[i],
						vsem_if_buf[i + 1],
						vsem_if_buf[i + 2],
						vsem_if_buf[i + 3],
						vsem_if_buf[i + 4],
						vsem_if_buf[i + 5],
						vsem_if_buf[i + 6],
						vsem_if_buf[i + 7]);
				}
			}
		}

		if (debug_dolby & 1) {
			if (dv_vsem || vsem_if_size)
				pr_dv_dbg("vdin get %s:%d, md:%p %d,ll:%d,bit %x\n",
					dv_vsem ? "vsem" : "vsif",
					dv_vsem ? vsem_size : vsem_if_size,
					req.aux_buf, req.aux_size,
					req.low_latency,
					vf->bitdepth);
			else
				pr_dv_dbg("vdin get md %p %d,ll:%d,bit %x\n",
					req.aux_buf, req.aux_size,
					req.low_latency, vf->bitdepth);
		}

		/*check vsem_if_buf */
		if (vsem_if_size &&
			vsem_if_buf[0] != 0x81) {
			/*not vsif, continue to check vsem*/
			if (!(vsem_if_buf[0] == 0x7F &&
				vsem_if_buf[1] == 0x80 &&
				vsem_if_buf[10] == 0x46 &&
				vsem_if_buf[11] == 0xd0 &&
				vsem_if_buf[12] == 0x00)) {
				vsem_if_size = 0;
				pr_dv_dbg("vsem_if_buf is invalid!\n");
				pr_dv_dbg("%x %x %x %x %x %x %x %x %x %x %x %x\n",
					vsem_if_buf[0],
					vsem_if_buf[1],
					vsem_if_buf[2],
					vsem_if_buf[3],
					vsem_if_buf[4],
					vsem_if_buf[5],
					vsem_if_buf[6],
					vsem_if_buf[7],
					vsem_if_buf[8],
					vsem_if_buf[9],
					vsem_if_buf[10],
					vsem_if_buf[11]);
			}
		}
		if ((dolby_vision_flags & FLAG_FORCE_DV_LL) ||
		    req.low_latency == 1) {
			src_format = FORMAT_DOVI_LL;
			src_chroma_format = 0;
			for (i = 0; i < 2; i++) {
				if (md_buf[i])
					memset(md_buf[i], 0, MD_BUF_SIZE);
				if (comp_buf[i])
					memset(comp_buf[i], 0, COMP_BUF_SIZE);
			}
			req.aux_size = 0;
			req.aux_buf = NULL;
		} else if (req.aux_size) {
			if (req.aux_buf) {
				current_id = current_id ^ 1;
				memcpy(md_buf[current_id],
				       req.aux_buf, req.aux_size);
			}
			src_format = FORMAT_DOVI;
		} else {
			if (toggle_mode == 2)
				src_format =  tv_dovi_setting->src_format;
			if (vf->type & VIDTYPE_VIU_422)
				src_chroma_format = 1;
			p_mdc =	&vf->prop.master_display_colour;
			if (is_hdr10_frame(vf) || force_hdmin_fmt == 1) {
				src_format = FORMAT_HDR10;
				/* prepare parameter from hdmi for hdr10 */
				p_mdc->luminance[0] *= 10000;
				prepare_hdr10_param
					(p_mdc, &hdr10_param);
				req.dv_enhance_exist = 0;
				src_bdp = 12;
			}
			if (is_aml_tm2_tvmode() || is_aml_t7_tvmode() ||
			    is_aml_t3_tvmode() ||
			    is_aml_t5w()) {
				if (src_format != FORMAT_DOVI &&
					(is_hlg_frame(vf) || force_hdmin_fmt == 2)) {
					src_format = FORMAT_HLG;
					src_bdp = 12;
				}
				if (src_format == FORMAT_SDR &&
					!req.dv_enhance_exist)
					src_bdp = 12;
			}
		}
		if ((debug_dolby & 4) && req.aux_size) {
			pr_dv_dbg("metadata(%d):\n", req.aux_size);
			for (i = 0; i < req.aux_size + 8; i += 8)
				pr_info("\t%02x %02x %02x %02x %02x %02x %02x %02x\n",
					md_buf[current_id][i],
					md_buf[current_id][i + 1],
					md_buf[current_id][i + 2],
					md_buf[current_id][i + 3],
					md_buf[current_id][i + 4],
					md_buf[current_id][i + 5],
					md_buf[current_id][i + 6],
					md_buf[current_id][i + 7]);
		}

		total_md_size = req.aux_size;
		total_comp_size = 0;
		meta_flag_bl = 0;
		if (req.aux_buf && req.aux_size) {
			last_total_md_size = total_md_size;
			last_total_comp_size = total_comp_size;
		} else if (toggle_mode == 2) {
			total_md_size = last_total_md_size;
			total_comp_size = last_total_comp_size;
		}
		if (debug_dolby & 1)
			pr_dv_dbg("frame %d, %p, pts %lld, format: %s\n",
			frame_count, vf, vf->pts_us64,
			(src_format == FORMAT_HDR10) ? "HDR10" :
			((src_format == FORMAT_DOVI) ? "DOVI" :
			((src_format == FORMAT_DOVI_LL) ? "DOVI_LL" :
			((src_format == FORMAT_HLG) ? "HLG" : "SDR"))));

		if (toggle_mode == 1) {
			if (debug_dolby & 2)
				pr_dv_dbg
					("+++ get bl(%p-%lld) +++\n",
					 vf, vf->pts_us64);
			amdvdolby_vision_vf_add(vf, NULL);
		}
	} else if (vf && (vf->source_type == VFRAME_SOURCE_TYPE_OTHERS)) {
		enum vframe_signal_fmt_e fmt;

		input_mode = IN_MODE_OTT;

		req.vf = vf;
		req.bot_flag = 0;
		req.aux_buf = NULL;
		req.aux_size = 0;
		req.dv_enhance_exist = 0;

		/* check source format */
		fmt = get_vframe_src_fmt(vf);
		if ((fmt == VFRAME_SIGNAL_FMT_DOVI ||
		    fmt == VFRAME_SIGNAL_FMT_INVALID) &&
		    !vf->discard_dv_data) {
			vf_notify_provider_by_name
				(dv_provider[0],
				 VFRAME_EVENT_RECEIVER_GET_AUX_DATA,
				  (void *)&req);
		}
		/* use callback aux date first, if invalid, use sei_ptr */
		if ((!req.aux_buf || !req.aux_size) &&
		    fmt == VFRAME_SIGNAL_FMT_DOVI) {
			u32 sei_size = 0;
			char *sei;

			if (debug_dolby & 1)
				pr_dv_dbg("no aux %p %x, el %d from %s, use sei_ptr\n",
					     req.aux_buf,
					     req.aux_size,
					     req.dv_enhance_exist,
					     dv_provider[0]);

			sei = (char *)get_sei_from_src_fmt
				(vf, &sei_size);
			if (sei && sei_size) {
				req.aux_buf = sei;
				req.aux_size = sei_size;
				req.dv_enhance_exist =
					vf->src_fmt.dual_layer;
			}
		}
		if (debug_dolby & 1)
			pr_dv_dbg("%s get vf %p(%d), fmt %d, aux %p %x, el %d\n",
				     dv_provider[0], vf, vf->discard_dv_data, fmt,
				     req.aux_buf,
				     req.aux_size,
				     req.dv_enhance_exist);
		/* parse meta in base layer */
		if (toggle_mode != 2) {
			ret = get_md_from_src_fmt(vf);
			if (ret == 1) { /*parse succeeded*/

				meta_flag_bl = 0;
				src_format = FORMAT_DOVI;
				memcpy(md_buf[current_id],
				       vf->src_fmt.md_buf,
				       vf->src_fmt.md_size);
				memcpy(comp_buf[current_id],
				       vf->src_fmt.comp_buf,
				       vf->src_fmt.comp_size);
				total_md_size =  vf->src_fmt.md_size;
				total_comp_size =  vf->src_fmt.comp_size;
				ret_flags = vf->src_fmt.parse_ret_flags;
				if ((debug_dolby & 4) && dump_enable) {
					pr_dv_dbg("get md_buf %p, size(%d):\n",
						vf->src_fmt.md_buf, vf->src_fmt.md_size);
					for (i = 0; i < total_md_size; i += 8)
						pr_info("%02x %02x %02x %02x %02x %02x %02x %02x\n",
							md_buf[current_id][i],
							md_buf[current_id][i + 1],
							md_buf[current_id][i + 2],
							md_buf[current_id][i + 3],
							md_buf[current_id][i + 4],
							md_buf[current_id][i + 5],
							md_buf[current_id][i + 6],
							md_buf[current_id][i + 7]);
				}
			} else {  /*no parse or parse failed*/
				if (get_vframe_src_fmt(vf) ==
				    VFRAME_SIGNAL_FMT_HDR10PRIME)
					src_format = FORMAT_PRIMESL;
				else
					meta_flag_bl =
					parse_sei_and_meta
						(vf, &req,
						 &total_comp_size,
						 &total_md_size,
						 &src_format,
						 &ret_flags, drop_flag, 0);
			}
			if (force_mel)
				ret_flags = 1;

			if (ret_flags && req.dv_enhance_exist) {
				if (!strcmp(dv_provider[0], "dvbldec"))
					vf_notify_provider_by_name
					(dv_provider[0],
					 VFRAME_EVENT_RECEIVER_DOLBY_BYPASS_EL,
					 (void *)&req);
				amdv_el_disable = 1;
				pr_dv_dbg("bypass mel\n");
			}
			if (ret_flags == 1)
				mel_flag = true;
			if (!is_dv_standard_es(req.dv_enhance_exist,
					       ret_flags, w)) {
				src_format = FORMAT_SDR;
				/* dovi_setting.src_format = src_format; */
				total_comp_size = 0;
				total_md_size = 0;
				src_bdp = 10;
				bypass_release = true;
			}
			if ((is_aml_tm2_stbmode()  || is_aml_sc2() ||
			    is_aml_t7_stbmode() || is_aml_s4d()) &&
			    (req.dv_enhance_exist && !mel_flag &&
			    ((dolby_vision_flags & FLAG_CERTIFICATION) == 0)) &&
			    !enable_fel) {
				src_format = FORMAT_SDR;
				/* dovi_setting.src_format = src_format; */
				total_comp_size = 0;
				total_md_size = 0;
				src_bdp = 10;
				bypass_release = true;
			}
			if (is_aml_tvmode() &&
				(req.dv_enhance_exist && !mel_flag)) {
				src_format = FORMAT_SDR;
				/* dovi_setting.src_format = src_format; */
				total_comp_size = 0;
				total_md_size = 0;
				src_bdp = 10;
				bypass_release = true;
				if (debug_dolby & 1)
					pr_dv_dbg("tv: bypass fel\n");
			}
		} else if (is_amdv_stb_mode()) {
			src_format = dovi_setting.src_format;
		} else if (is_aml_tvmode()) {
			src_format = tv_dovi_setting->src_format;
		}

		if (src_format != FORMAT_DOVI && is_primesl_frame(vf)) {
			src_format = FORMAT_PRIMESL;
			src_bdp = 10;
		}

		if (src_format != FORMAT_DOVI && is_hdr10_frame(vf)) {
			src_format = FORMAT_HDR10;
			/* prepare parameter from SEI for hdr10 */
			p_mdc =	&vf->prop.master_display_colour;
			prepare_hdr10_param(p_mdc, &hdr10_param);
			/* for 962x with v1.4 or stb with v2.3 may use 12 bit */
			src_bdp = 10;
			req.dv_enhance_exist = 0;
		}

		if (src_format != FORMAT_DOVI && is_hlg_frame(vf)) {
			src_format = FORMAT_HLG;
			if (is_aml_tvmode())
				src_bdp = 10;
		}

		if (src_format != FORMAT_DOVI && is_hdr10plus_frame(vf))
			src_format = FORMAT_HDR10PLUS;

		if (src_format != FORMAT_DOVI && is_mvc_frame(vf))
			src_format = FORMAT_MVC;

		if (src_format != FORMAT_DOVI && is_cuva_frame(vf))
			src_format = FORMAT_CUVA;

		/* TODO: need 962e ? */
		if (src_format == FORMAT_SDR &&
		    is_amdv_stb_mode() &&
		    !req.dv_enhance_exist)
			src_bdp = 10;

		if (src_format == FORMAT_SDR &&
			is_aml_tvmode() &&
			!req.dv_enhance_exist)
			src_bdp = 10;

		if (((debug_dolby & 1) || frame_count == 0) && toggle_mode == 1)
			pr_info
			("DV:[%d,%lld,%d,%s,%d,%d]\n",
			 frame_count, vf->pts_us64, src_bdp,
			 (src_format == FORMAT_HDR10) ? "HDR10" :
			 (src_format == FORMAT_DOVI ? "DOVI" :
			 (src_format == FORMAT_HLG ? "HLG" :
			 (src_format == FORMAT_HDR10PLUS ? "HDR10+" :
			 (src_format == FORMAT_CUVA ? "CUVA" :
			 (src_format == FORMAT_PRIMESL ? "PRIMESL" :
			 (req.dv_enhance_exist ? "DOVI (el meta)" : "SDR")))))),
			 req.aux_size, req.dv_enhance_exist);
		if (src_format != FORMAT_DOVI && !req.dv_enhance_exist)
			memset(&req, 0, sizeof(req));

		/* check dvel decoder is active, if active, should */
		/* get/put el data, otherwise, dvbl is stuck */
		dvel_provider = vf_get_provider_name(DVEL_RECV_NAME);
		if (req.dv_enhance_exist && toggle_mode == 1 &&
		    dvel_provider && !strcmp(dvel_provider, "dveldec")) {
			el_vf = dvel_vf_get();
			if (el_vf && (el_vf->pts_us64 == vf->pts_us64 ||
				      !(dolby_vision_flags &
				      FLAG_CHECK_ES_PTS))) {
				if (debug_dolby & 2)
					pr_dv_dbg("+++ get bl(%p-%lld) with el(%p-%lld) +++\n",
						vf, vf->pts_us64,
						el_vf, el_vf->pts_us64);
				if (meta_flag_bl) {
					int el_md_size = 0;
					int el_comp_size = 0;

					el_req.vf = el_vf;
					el_req.bot_flag = 0;
					el_req.aux_buf = NULL;
					el_req.aux_size = 0;
					if (!strcmp(dv_provider[0], "dvbldec"))
						vf_notify_provider_by_name
						("dveldec",
						 VFRAME_EVENT_RECEIVER_GET_AUX_DATA,
						 (void *)&el_req);
					if (el_req.aux_buf &&
					    el_req.aux_size) {
						meta_flag_el =
							parse_sei_and_meta
							(el_vf, &el_req,
							 &el_comp_size,
							 &el_md_size,
							 &src_format,
							 &ret_flags, drop_flag,
							0);
					}
					if (!meta_flag_el) {
						total_comp_size =
							el_comp_size;
						total_md_size =
							el_md_size;
						src_bdp = 12;
					}
					/* force set format as DOVI*/
					/*	when meta data error */
					if (meta_flag_el &&
					    el_req.aux_buf &&
					    el_req.aux_size)
						src_format = FORMAT_DOVI;
					if (debug_dolby & 2)
						pr_dv_dbg
					("el mode:src_fmt:%d,meta_flag_el:%d\n",
						 src_format,
						 meta_flag_el);
					if (meta_flag_el && frame_count == 0)
						pr_info
					("el mode:parser err,aux %p,size:%d\n",
						 el_req.aux_buf,
						 el_req.aux_size);
				}
				amdvdolby_vision_vf_add(vf, el_vf);
				el_flag = 1;
				if (vf->width == el_vf->width)
					el_halfsize_flag = 0;
			} else {
				if (!el_vf)
					pr_dv_error
					("bl(%p-%lld) not found el\n",
					 vf, vf->pts_us64);
				else
					pr_dv_error
					("bl(%p-%lld) not found el(%p-%lld)\n",
					 vf, vf->pts_us64,
					 el_vf, el_vf->pts_us64);
			}
		} else if (toggle_mode == 1) {
			if (debug_dolby & 2)
				pr_dv_dbg("+++ get bl(%p-%lld) +++\n",
					     vf, vf->pts_us64);
			amdvdolby_vision_vf_add(vf, NULL);
		}

		if (req.dv_enhance_exist)
			el_flag = 1;

		if (toggle_mode != 2) {
			if (!drop_flag) {
				last_total_md_size = total_md_size;
				last_total_comp_size = total_comp_size;
			}
		} else if (meta_flag_bl && meta_flag_el) {
			total_md_size = last_total_md_size;
			total_comp_size = last_total_comp_size;
			if (is_amdv_stb_mode())
				el_flag = dovi_setting.el_flag;
			else
				el_flag = tv_dovi_setting->el_flag;
			mel_flag = mel_mode;
			if (debug_dolby & 2)
				pr_dv_dbg("update el_flag %d, melFlag %d\n",
					     el_flag, mel_flag);
			meta_flag_bl = 0;
		}

		if (el_flag && !mel_flag &&
		    ((dolby_vision_flags & FLAG_CERTIFICATION) == 0) &&
		    !enable_fel) {
			el_flag = 0;
			amdv_el_disable = 1;
		}
		if (el_flag && !enable_mel)
			el_flag = 0;
		if (src_format != FORMAT_DOVI) {
			el_flag = 0;
			mel_flag = 0;
		}
		if (src_format == FORMAT_DOVI && meta_flag_bl && meta_flag_el) {
			/* dovi frame no meta or meta error */
			/* use old setting for this frame   */
			pr_dv_dbg("no meta or meta err!\n");
			return -1;
		}
	} else if (vf && vf->source_type == VFRAME_SOURCE_TYPE_HDMI &&
	(is_aml_tm2_stbmode() || is_aml_t7_stbmode())  && hdmi_to_stb_policy) {
		req.vf = vf;
		req.bot_flag = 0;
		req.aux_buf = NULL;
		req.aux_size = 0;
		req.dv_enhance_exist = 0;
		req.low_latency = 0;

		vf_notify_provider_by_name("dv_vdin",
			VFRAME_EVENT_RECEIVER_GET_AUX_DATA,
			(void *)&req);
		if (req.low_latency == 1) {
			src_format = FORMAT_HDR10;
			if (!vf_is_hdr10(vf)) {
				vf->signal_type &= 0xff0000ff;
				vf->signal_type |= 0x00091000;
			}
			p_mdc = &vf->prop.master_display_colour;

			prepare_hdr10_param(p_mdc, &hdr10_param);
			src_bdp = 10;
			req.aux_size = 0;
			req.aux_buf = NULL;
		} else if (req.aux_size) {
			if (req.aux_buf) {
				current_id = current_id ^ 1;
				memcpy(md_buf[current_id],
				       req.aux_buf, req.aux_size);
			}
			src_format = FORMAT_DOVI;
			input_mode = IN_MODE_HDMI;
			src_bdp = 12;
			meta_flag_bl = 0;
			el_flag = 0;
			mel_flag = 0;
			if ((debug_dolby & 4) && dump_enable) {
				pr_dv_dbg("metadata(%d):\n", req.aux_size);
				for (i = 0; i < req.aux_size; i += 8)
					pr_info("\t%02x %02x %02x %02x %02x %02x %02x %02x\n",
						md_buf[current_id][i],
						md_buf[current_id][i + 1],
						md_buf[current_id][i + 2],
						md_buf[current_id][i + 3],
						md_buf[current_id][i + 4],
						md_buf[current_id][i + 5],
						md_buf[current_id][i + 6],
						md_buf[current_id][i + 7]);
			}
		} else {
			if (toggle_mode == 2)
				src_format = dovi_setting.src_format;

			if (is_hdr10_frame(vf) || force_hdmin_fmt == 1) {
				src_format = FORMAT_HDR10;
				p_mdc =	&vf->prop.master_display_colour;
				if (p_mdc->present_flag)
					p_mdc->luminance[0] *= 10000;
				prepare_hdr10_param(p_mdc, &hdr10_param);
			}

			if (is_hlg_frame(vf) || force_hdmin_fmt == 2)
				src_format = FORMAT_HLG;

			if (is_hdr10plus_frame(vf))
				src_format = FORMAT_HDR10PLUS;

			src_bdp = 10;
		}
		total_md_size = req.aux_size;
		total_comp_size = 0;
	}

	if (src_format == FORMAT_DOVI && meta_flag_bl && meta_flag_el) {
		/* dovi frame no meta or meta error */
		/* use old setting for this frame   */
		pr_dv_dbg("no meta or meta err!\n");
		return -1;
	}

	/* if not DOVI, release metadata_parser */
	if (vf && src_format != FORMAT_DOVI &&
	    metadata_parser && !bypass_release) {
		if (p_funcs_stb)
			p_funcs_stb->metadata_parser_release();
		if (p_funcs_tv)
			p_funcs_tv->metadata_parser_release();
		metadata_parser = NULL;
		pr_dv_dbg("parser release\n");
	}

	if (drop_flag) {
		pr_dv_dbg("drop frame_count %d\n", frame_count);
		return 2;
	}

	check_format = src_format;
	if (vf) {
		update_src_format(check_format, vf);
		last_current_format = check_format;
	}

	if (dolby_vision_request_mode != 0xff) {
		dolby_vision_mode = dolby_vision_request_mode;
		dolby_vision_request_mode = 0xff;
	}
	current_mode = dolby_vision_mode;
	if (amdv_policy_process
		(vf, &current_mode, check_format)) {
		if (!amdv_wait_init)
			amdv_set_toggle_flag(1);
		pr_dv_dbg("[%s]output change from %d to %d(%d, %p, %d)\n",
			     __func__, dolby_vision_mode, current_mode,
			     toggle_mode, vf, src_format);
		amdv_target_mode = current_mode;
		dolby_vision_mode = current_mode;
		if (is_amdv_stb_mode())
			new_dovi_setting.mode_changed = 1;
	} else {
		/*not clear target mode when:*/
		/*no mode change && no vf && target is not bypass */
		if ((!vf && amdv_target_mode != dolby_vision_mode &&
		    amdv_target_mode !=
		    AMDV_OUTPUT_MODE_BYPASS)) {
			if (debug_dolby & 8)
				pr_dv_dbg("not update target mode %d\n",
					     amdv_target_mode);
		} else if (amdv_target_mode != dolby_vision_mode) {
			amdv_target_mode = dolby_vision_mode;
			if (debug_dolby & 8)
				pr_dv_dbg("update target mode %d\n",
					     amdv_target_mode);
		}
	}

	if (vf && (debug_dolby & 8))
		pr_dv_dbg("parse_metadata: vf %p(index %d), mode %d\n",
			      vf, vf->omx_index, dolby_vision_mode);

	if (get_hdr_module_status(VD1_PATH, VPP_TOP0) != HDR_MODULE_ON &&
	    check_format == FORMAT_SDR) {
		/* insert 2 SDR frames before send DOVI */
		if ((dolby_vision_mode == AMDV_OUTPUT_MODE_IPT_TUNNEL ||
		     dolby_vision_mode == AMDV_OUTPUT_MODE_IPT) &&
		    (last_current_format == FORMAT_HLG ||
		    last_current_format == FORMAT_PRIMESL ||
		    last_current_format == FORMAT_HDR10PLUS)) {
			/* TODO: if need add primesl */
			bypass_frame = 0;
			pr_dv_dbg
			("[%s] source transition from %d to %d\n",
			 __func__, last_current_format, check_format);
		}
		last_current_format = check_format;
	}

	if (bypass_frame >= 0 && bypass_frame < MIN_TRANSITION_DELAY) {
		dolby_vision_mode = AMDV_OUTPUT_MODE_BYPASS;
		bypass_frame++;
	} else {
		bypass_frame = -1;
	}

	if (dolby_vision_mode == AMDV_OUTPUT_MODE_BYPASS) {
		new_dovi_setting.video_width = 0;
		new_dovi_setting.video_height = 0;
		new_dovi_setting.mode_changed = 0;
		if (amdv_target_mode == AMDV_OUTPUT_MODE_BYPASS)
			amdv_wait_on = false;
		if (debug_dolby & 8)
			pr_dv_dbg("now bypass mode, target %d, wait %d\n",
				  amdv_target_mode,
				  amdv_wait_on);
		if (get_hdr_module_status(VD1_PATH, VPP_TOP0) == HDR_MODULE_BYPASS)
			return 1;
		return -1;
	}

	if (tv_mode) {
		if (!p_funcs_tv)
			return -1;
	} else {
		if (!p_funcs_stb)
			return -1;
	}

	/* TV core */
	if (is_aml_tvmode()) {
		if (src_format != tv_dovi_setting->src_format ||
			(dolby_vision_flags & FLAG_CERTIFICATION)) {
			pq_config_set_flag = false;
			best_pq_config_set_flag = false;
		}
		if (!pq_config_set_flag) {
			if (debug_dolby & 2)
				pr_dv_dbg("update def_tgt_display_cfg\n");
			if (!get_load_config_status()) {
				memcpy(&(((struct pq_config *)pq_config_fake)->tdc),
				       &def_tgt_display_cfg_ll,
				       sizeof(def_tgt_display_cfg_ll));
			}
			pq_config_set_flag = true;
		}
		if (force_best_pq && !best_pq_config_set_flag) {
			pr_dv_dbg("update best def_tgt_display_cfg\n");
			memcpy(&(((struct pq_config *)
				pq_config_fake)->tdc),
				&def_tgt_display_cfg_bestpq,
				sizeof(def_tgt_display_cfg_bestpq));
			best_pq_config_set_flag = true;

			if (p_funcs_tv && p_funcs_tv->tv_control_path)
				p_funcs_tv->tv_control_path
					(FORMAT_INVALID, 0,
					NULL, 0,
					NULL, 0,
					0, 0,
					SIGNAL_RANGE_SMPTE,
					NULL, NULL,
					0,
					NULL,
					NULL,
					NULL, 0,
					NULL,
					NULL);
		}
		calculate_panel_max_pq(src_format, vinfo,
				       &(((struct pq_config *)
				       pq_config_fake)->tdc));

		((struct pq_config *)
			pq_config_fake)->tdc.tuning_mode =
			amdv_tuning_mode;
		if (dolby_vision_flags & FLAG_DISABLE_COMPOSER) {
			((struct pq_config *)pq_config_fake)
				->tdc.tuning_mode |=
				TUNING_MODE_EL_FORCE_DISABLE;
			el_halfsize_flag = 0;
		} else {
			((struct pq_config *)pq_config_fake)
				->tdc.tuning_mode &=
				(~TUNING_MODE_EL_FORCE_DISABLE);
		}
		if ((dolby_vision_flags & FLAG_CERTIFICATION) && sdr_ref_mode) {
			((struct pq_config *)
			pq_config_fake)->tdc.ambient_config.ambient =
			0;
			((struct pq_config *)pq_config_fake)
				->tdc.ref_mode_dark_id = 0;
			((struct pq_config *)pq_config_fake)
				->tdc.dm31_avail = 1;
		}
		if (is_hdr10_src_primary_changed()) {
			hdr10_src_primary_changed = true;
			pr_dv_dbg("hdr10 src primary changed!\n");
		}
		if (src_format != tv_dovi_setting->src_format ||
			tv_dovi_setting->video_width != w ||
			tv_dovi_setting->video_height != h ||
			hdr10_src_primary_changed) {
			pr_dv_dbg("reset control_path fmt %d->%d, w %d->%d, h %d->%d\n",
				tv_dovi_setting->src_format, src_format,
				tv_dovi_setting->video_width, w,
				tv_dovi_setting->video_height, h);
			/*for hdmi in cert*/
			if (dolby_vision_flags & FLAG_CERTIFICATION)
				vf_changed = true;
			if (p_funcs_tv && p_funcs_tv->tv_control_path)
				p_funcs_tv->tv_control_path
					(FORMAT_INVALID, 0,
					NULL, 0,
					NULL, 0,
					0, 0,
					SIGNAL_RANGE_SMPTE,
					NULL, NULL,
					0,
					NULL,
					NULL,
					NULL, 0,
					NULL,
					NULL);
		}
		tv_dovi_setting->video_width = w << 16;
		tv_dovi_setting->video_height = h << 16;

		if (debug_cp_res > 0) {
			tv_dovi_setting->video_width = debug_cp_res & 0xffff0000;
			tv_dovi_setting->video_height = (debug_cp_res & 0xffff) << 16;
		}
		if (!p_funcs_tv || !p_funcs_tv->tv_control_path)
			return -1;
		pic_mode = get_cur_pic_mode_name();
		if (!(dolby_vision_flags & FLAG_CERTIFICATION) && pic_mode &&
		    (strstr(pic_mode, "dark") ||
		    strstr(pic_mode, "Dark") ||
		    strstr(pic_mode, "DARK"))) {
			memcpy(tv_input_info,
			       brightness_off,
			       sizeof(brightness_off));
			/*for HDR10/HLG, only has DM4, ko only use value from tv_input_info[3][1]*/
			/*and tv_input_info[4][1]. To avoid ko code changed, we reuse these*/
			/*parameter for both HDMI and OTT mode, that means need copy HDR10 to */
			/*tv_input_info[3][1] and copy HLG to tv_input_info[4][1] for HDMI mode*/
			if (input_mode == IN_MODE_HDMI) {
				tv_input_info->brightness_off[3][1] = brightness_off[3][0];
				tv_input_info->brightness_off[4][1] = brightness_off[4][0];
			}
		} else {
			memset(tv_input_info, 0, sizeof(brightness_off));
		}
		/*config source fps and gd_rf_adjust, dmcfg_id*/
		tv_input_info->content_fps = content_fps * (1 << 16);
		tv_input_info->gd_rf_adjust = gd_rf_adjust;
		tv_input_info->tid = get_pic_mode();
		if (debug_dolby & 0x400)
			do_gettimeofday(&start);
		/*for hdmi in cert, only run control_path for different frame*/
		if ((dolby_vision_flags & FLAG_CERTIFICATION) &&
		    !vf_changed && input_mode == IN_MODE_HDMI) {
			run_control_path = false;
		}
		if (run_control_path) {
			if (ambient_update) {
				/*only if cfg enables darkdetail we allow the API to set values*/
				if (((struct pq_config *)pq_config_fake)->
					tdc.ambient_config.dark_detail) {
					ambient_config_new.dark_detail =
					cfg_info[cur_pic_mode].dark_detail;
				}
				p_ambient = &ambient_config_new;
			} else {
				if (ambient_test_mode == 1 && toggle_mode == 1 &&
				    frame_count < AMBIENT_CFG_FRAMES) {
					p_ambient = &ambient_test_cfg[frame_count];
				} else if (ambient_test_mode == 2 && toggle_mode == 1 &&
					   frame_count < AMBIENT_CFG_FRAMES) {
					p_ambient = &ambient_test_cfg_2[frame_count];
				} else if (ambient_test_mode == 3 && toggle_mode == 1 &&
					   hdmi_frame_count < AMBIENT_CFG_FRAMES) {
					p_ambient = &ambient_test_cfg_3[hdmi_frame_count];
				} else if (((struct pq_config *)pq_config_fake)->
					tdc.ambient_config.dark_detail) {
					/*only if cfg enables darkdetail we allow the API to set*/
					ambient_darkdetail.dark_detail =
						cfg_info[cur_pic_mode].dark_detail;
					p_ambient = &ambient_darkdetail;
				}
			}
			if (debug_dolby & 0x200)
				pr_dv_dbg("[count %d %d]dark_detail from cfg:%d,from api:%d\n",
					     hdmi_frame_count, frame_count,
					     ((struct pq_config *)pq_config_fake)->
					     tdc.ambient_config.dark_detail,
					     cfg_info[cur_pic_mode].dark_detail);
			flag = p_funcs_tv->tv_control_path
				(src_format, input_mode,
				comp_buf[current_id],
				(src_format == FORMAT_DOVI) ? total_comp_size : 0,
				md_buf[current_id],
				(src_format == FORMAT_DOVI) ? total_md_size : 0,
				src_bdp,
				src_chroma_format,
				SIGNAL_RANGE_SMPTE, /* bit/chroma/range */
				(struct pq_config *)pq_config_fake, &menu_param,
				(!el_flag) ||
				(dolby_vision_flags & FLAG_DISABLE_COMPOSER),
				&hdr10_param,
				tv_dovi_setting,
				vsem_if_buf, vsem_if_size,
				p_ambient,
				tv_input_info);

			if (debug_dolby & 0x400) {
				do_gettimeofday(&end);
				time_use = (end.tv_sec - start.tv_sec) * 1000000 +
					(end.tv_usec - start.tv_usec);

				pr_info("controlpath time: %5ld us\n", time_use);
			}
			if (flag >= 0) {
				if (input_mode == IN_MODE_HDMI) {
					if (h > 1080)
						tv_dovi_setting->core1_reg_lut[1] =
							0x0000000100000043;
					else
						tv_dovi_setting->core1_reg_lut[1] =
							0x0000000100000042;
					if (get_video_reverse() &&
					(src_format == FORMAT_DOVI ||
					src_format == FORMAT_DOVI_LL)) {
						u32 coeff[3][3];
						u64 *p =
							&tv_dovi_setting->core1_reg_lut[9];

						coeff[0][1] = (p[0] >> 16) & 0xffff;
						coeff[0][2] = p[1] & 0xffff;
						coeff[1][1] = p[2] & 0xffff;
						coeff[1][2] = (p[2] >> 16) & 0xffff;
						coeff[2][1] = (p[3] >> 16) & 0xffff;
						coeff[2][2] = p[4] & 0xffff;

						p[0] = (p[0] & 0xffffffff0000ffff) |
							(coeff[0][2] << 16);
						p[1] = (p[1] & 0xffffffffffff0000) |
							coeff[0][1];
						p[2] = (p[2] & 0xffffffff00000000) |
							(coeff[1][1] << 16) |
							coeff[1][2];
						p[3] = (p[3] & 0xffffffff0000ffff) |
							(coeff[2][2] << 16);
						p[4] = (p[4] & 0xffffffffffff0000) |
							coeff[2][1];
					}
				} else {
					if (src_format == FORMAT_HDR10)
						tv_dovi_setting->core1_reg_lut[1] =
							0x000000010000404c;
					else if (el_halfsize_flag)
						tv_dovi_setting->core1_reg_lut[1] =
							0x000000010000004c;
					else
						tv_dovi_setting->core1_reg_lut[1] =
							0x0000000100000044;
				}
				/* enable CRC */
				if ((dolby_vision_flags & FLAG_CERTIFICATION) &&
					!(dolby_vision_flags & FLAG_DISABLE_CRC))
					tv_dovi_setting->core1_reg_lut[3] =
						0x000000ea00000001;
				tv_dovi_setting->src_format = src_format;
				tv_dovi_setting->el_flag = el_flag;
				tv_dovi_setting
					->el_halfsize_flag = el_halfsize_flag;
				tv_dovi_setting->video_width = w;
				tv_dovi_setting->video_height = h;
				tv_dovi_setting
					->input_mode = input_mode;
				tv_dovi_setting_change_flag = true;
				amdv_setting_video_flag = video_frame;

				if (debug_dolby & 1) {
					if (el_flag)
						pr_dv_dbg
					("tv setting %s-%d:flag=%x,md=%d,comp=%d\n",
						 input_mode == IN_MODE_HDMI ?
						 "hdmi" : "ott",
						 src_format,
						 flag,
						 total_md_size,
						 total_comp_size);
					else
						pr_dv_dbg
						("tv setting %s-%d:flag=%x,md=%d\n",
						 input_mode == IN_MODE_HDMI ?
						 "hdmi" : "ott",
						 src_format,
						 flag,
						 total_md_size);
				}
				dump_tv_setting(tv_dovi_setting,
					frame_count, debug_dolby);
				mel_mode = mel_flag;
				ret = 0; /* setting updated */
			} else {
				tv_dovi_setting->video_width = 0;
				tv_dovi_setting->video_height = 0;
				pr_dv_error("tv_control_path() failed\n");
			}
		} else { /*for cert: vf no change, not run cp*/
			if (h > 1080)
				tv_dovi_setting->core1_reg_lut[1] =
					0x0000000100000043;
			else
				tv_dovi_setting->core1_reg_lut[1] =
					0x0000000100000042;

			/* enable CRC */
			if ((dolby_vision_flags & FLAG_CERTIFICATION) &&
				!(dolby_vision_flags & FLAG_DISABLE_CRC))
				tv_dovi_setting->core1_reg_lut[3] =
					0x000000ea00000001;
			tv_dovi_setting->src_format = src_format;
			tv_dovi_setting->el_flag = el_flag;
			tv_dovi_setting
				->el_halfsize_flag = el_halfsize_flag;
			tv_dovi_setting->video_width = w;
			tv_dovi_setting->video_height = h;
			tv_dovi_setting
				->input_mode = input_mode;
			tv_dovi_setting_change_flag = true;
			amdv_setting_video_flag = video_frame;
			ret = 0;
		}
		return ret;
	}

	/* update input mode for HDMI in STB core */
	if (is_aml_tm2_stbmode() || is_aml_t7_stbmode()) {
		tv_dovi_setting->input_mode = input_mode;
		if (is_aml_stb_hdmimode() && !core1_detunnel()) {
			tv_dovi_setting->src_format = src_format;
			tv_dovi_setting->video_width = w;
			tv_dovi_setting->video_height = h;
			tv_dovi_setting->el_flag = false;
			tv_dovi_setting->el_halfsize_flag = false;
			amdv_run_mode_delay = RUN_MODE_DELAY;
		} else {
			amdv_run_mode_delay = 0;
		}
	}
	/* check dst format */
	if (dolby_vision_mode == AMDV_OUTPUT_MODE_IPT_TUNNEL ||
	    dolby_vision_mode == AMDV_OUTPUT_MODE_IPT)
		dst_format = FORMAT_DOVI;
	else if (dolby_vision_mode == AMDV_OUTPUT_MODE_HDR10)
		dst_format = FORMAT_HDR10;
	else
		dst_format = FORMAT_SDR;

	/* STB core */
	/* check target luminance */
	graphic_min = amdv_graphic_min;
	if (amdv_graphic_max != 0) {
		graphic_max = amdv_graphic_max;
	} else {
		if (src_format >= 0 && src_format <
		    ARRAY_SIZE(dv_target_graphics_max))
			tmp_fmt = src_format;
		if ((dolby_vision_flags & FLAG_FORCE_DV_LL) ||
		    dolby_vision_ll_policy >= DOLBY_VISION_LL_YUV422) {
			graphic_max =
				dv_target_graphics_LL_max
					[tmp_fmt][dst_format];
		} else {
			graphic_max =
				dv_target_graphics_max
					[tmp_fmt][dst_format];
		}

		if (dv_graphic_blend_test && dst_format == FORMAT_HDR10)
			graphic_max = dv_HDR10_graphics_max;
	}

	if (dolby_vision_flags & FLAG_USE_SINK_MIN_MAX) {
		if (vinfo->vout_device->dv_info->ieeeoui == 0x00d046) {
			if (vinfo->vout_device->dv_info->ver == 0) {
				/* need lookup PQ table ... */
			} else if (vinfo->vout_device->dv_info->ver == 1) {
				if (vinfo->vout_device->dv_info->tmax_lum) {
					/* Target max luminance = 100+50*CV */
					graphic_max =
					target_lumin_max =
						(vinfo->vout_device->dv_info->tmax_lum
						* 50 + 100);
					/* Target min luminance = (CV/127)^2 */
					graphic_min =
					amdv_target_min =
						(vinfo->vout_device->dv_info->tmin_lum ^ 2)
						* 10000 / (127 * 127);
				}
			}
		} else if (sink_hdr_support(vinfo) & HDR_SUPPORT) {
			if (vinfo->hdr_info.lumi_max) {
				/* Luminance value = 50 * (2 ^ (CV/32)) */
				graphic_max =
				target_lumin_max = 50 *
					(2 ^ (vinfo->hdr_info.lumi_max >> 5));
				/* Desired Content Min Luminance =*/
				/*	Desired Content Max Luminance*/
				/*	* (CV/255) * (CV/255) / 100	*/
				graphic_min =
				amdv_target_min =
					target_lumin_max * 10000
					* vinfo->hdr_info.lumi_min
					* vinfo->hdr_info.lumi_min
					/ (255 * 255 * 100);
			}
		}
		if (target_lumin_max) {
			amdv_target_max[0][0] =
			amdv_target_max[0][1] =
			amdv_target_max[1][0] =
			amdv_target_max[1][1] =
			amdv_target_max[2][0] =
			amdv_target_max[2][1] =
				target_lumin_max;
		} else {
			memcpy(amdv_target_max,
			       amdv_default_max,
			       sizeof(amdv_target_max));
		}
	}

	if (is_osd_off) {
		graphic_min = 0;
		graphic_max = 0;
	}

	if (new_dovi_setting.video_width && new_dovi_setting.video_height) {
	/* Toggle multiple frames in one vsync case: */
	/* new_dovi_setting.video_width will be available, but not be applied */
	/* So use new_dovi_setting as reference instead of dovi_setting. */
	/* To avoid unnecessary reset control_path. */
		cur_src_format = new_dovi_setting.src_format;
		cur_dst_format = new_dovi_setting.dst_format;
	} else {
		cur_src_format = dovi_setting.src_format;
		cur_dst_format = dovi_setting.dst_format;
	}
	if (src_format != cur_src_format ||
	    dst_format != cur_dst_format) {
		pr_dv_dbg
			("reset cp: src:%d->%d, dst:%d-%d, count:%d, flags:%x\n",
			 cur_src_format, src_format,
			 cur_dst_format, dst_format,
			 frame_count, dolby_vision_flags);
		p_funcs_stb->control_path
			(FORMAT_INVALID, 0,
			 comp_buf[current_id], 0,
			 md_buf[current_id], 0,
			 0, 0, 0, SIGNAL_RANGE_SMPTE,
			 0, 0, 0, 0,
			 0,
			 NULL,
			 NULL);
	}
	if (!vsvdb_config_set_flag) {
		memset(&new_dovi_setting.vsvdb_tbl[0],
		       0, sizeof(new_dovi_setting.vsvdb_tbl));
		new_dovi_setting.vsvdb_len = 0;
		new_dovi_setting.vsvdb_changed = 1;
		vsvdb_config_set_flag = true;
	}
	if ((dolby_vision_flags & FLAG_DISABLE_LOAD_VSVDB) == 0) {
		/* check if vsvdb is changed */
		if (vinfo &&  vinfo->vout_device &&
		    vinfo->vout_device->dv_info &&
		    vinfo->vout_device->dv_info->ieeeoui == 0x00d046 &&
		    vinfo->vout_device->dv_info->block_flag == CORRECT) {
			if (new_dovi_setting.vsvdb_len
				!= vinfo->vout_device->dv_info->length + 1)
				new_dovi_setting.vsvdb_changed = 1;
			else if (memcmp
				(&new_dovi_setting.vsvdb_tbl[0],
				 &vinfo->vout_device->dv_info->rawdata[0],
				 vinfo->vout_device->dv_info->length + 1))
				new_dovi_setting.vsvdb_changed = 1;
			memset(&new_dovi_setting.vsvdb_tbl[0],
			       0, sizeof(new_dovi_setting.vsvdb_tbl));
			memcpy(&new_dovi_setting.vsvdb_tbl[0],
			       &vinfo->vout_device->dv_info->rawdata[0],
			       vinfo->vout_device->dv_info->length + 1);
			new_dovi_setting.vsvdb_len =
				vinfo->vout_device->dv_info->length + 1;
			if (new_dovi_setting.vsvdb_changed &&
			    new_dovi_setting.vsvdb_len) {
				int k = 0;

				pr_dv_dbg("new vsvdb[%d]:\n",
					     new_dovi_setting.vsvdb_len);
				pr_dv_dbg
				("%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x\n",
				 new_dovi_setting.vsvdb_tbl[k + 0],
				 new_dovi_setting.vsvdb_tbl[k + 1],
				 new_dovi_setting.vsvdb_tbl[k + 2],
				 new_dovi_setting.vsvdb_tbl[k + 3],
				 new_dovi_setting.vsvdb_tbl[k + 4],
				 new_dovi_setting.vsvdb_tbl[k + 5],
				 new_dovi_setting.vsvdb_tbl[k + 6],
				 new_dovi_setting.vsvdb_tbl[k + 7]);
				k += 8;
				pr_dv_dbg
				("%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x\n",
				 new_dovi_setting.vsvdb_tbl[k + 0],
				 new_dovi_setting.vsvdb_tbl[k + 1],
				 new_dovi_setting.vsvdb_tbl[k + 2],
				 new_dovi_setting.vsvdb_tbl[k + 3],
				 new_dovi_setting.vsvdb_tbl[k + 4],
				 new_dovi_setting.vsvdb_tbl[k + 5],
				 new_dovi_setting.vsvdb_tbl[k + 6],
				 new_dovi_setting.vsvdb_tbl[k + 7]);
				k += 8;
				pr_dv_dbg
				("%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x\n",
				 new_dovi_setting.vsvdb_tbl[k + 0],
				 new_dovi_setting.vsvdb_tbl[k + 1],
				 new_dovi_setting.vsvdb_tbl[k + 2],
				 new_dovi_setting.vsvdb_tbl[k + 3],
				 new_dovi_setting.vsvdb_tbl[k + 4],
				 new_dovi_setting.vsvdb_tbl[k + 5],
				 new_dovi_setting.vsvdb_tbl[k + 6],
				 new_dovi_setting.vsvdb_tbl[k + 7]);
				k += 8;
				pr_dv_dbg
				("%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x\n",
				 new_dovi_setting.vsvdb_tbl[k + 0],
				 new_dovi_setting.vsvdb_tbl[k + 1],
				 new_dovi_setting.vsvdb_tbl[k + 2],
				 new_dovi_setting.vsvdb_tbl[k + 3],
				 new_dovi_setting.vsvdb_tbl[k + 4],
				 new_dovi_setting.vsvdb_tbl[k + 5],
				 new_dovi_setting.vsvdb_tbl[k + 6],
				 new_dovi_setting.vsvdb_tbl[k + 7]);
			}
		} else {
			if (new_dovi_setting.vsvdb_len)
				new_dovi_setting.vsvdb_changed = 1;
			memset(&new_dovi_setting.vsvdb_tbl[0],
				0, sizeof(new_dovi_setting.vsvdb_tbl));
			new_dovi_setting.vsvdb_len = 0;
		}
	}

	/* cert: some graphic test also need video pri 5223,5243,5253,5263 */
	if (dolby_vision_flags & FLAG_CERTIFICATION) {
		if ((dolby_vision_flags & FLAG_PRIORITY_GRAPHIC))
			pri_mode = G_PRIORITY;
		else
			pri_mode = V_PRIORITY;
	} else {
		/* audo mode:check video/graphics priority on the fly */
		if (get_video_enabled() && is_graphics_output_off())
			pri_mode = V_PRIORITY;
		else
			pri_mode = G_PRIORITY;
		/*user debug mode*/
		if (force_priority == 1)
			pri_mode = G_PRIORITY;
		else if (force_priority == 2)
			pri_mode = V_PRIORITY;
	}

	if (dst_format == FORMAT_DOVI) {
		if ((dolby_vision_flags & FLAG_FORCE_DV_LL) ||
		    dolby_vision_ll_policy >= DOLBY_VISION_LL_YUV422)
			new_dovi_setting.use_ll_flag = 1;
		else
			new_dovi_setting.use_ll_flag = 0;
		if (dolby_vision_ll_policy == DOLBY_VISION_LL_RGB444 ||
		    (dolby_vision_flags & FLAG_FORCE_RGB_OUTPUT))
			new_dovi_setting.ll_rgb_desired = 1;
		else
			new_dovi_setting.ll_rgb_desired = 0;
	} else {
		new_dovi_setting.use_ll_flag = 0;
		new_dovi_setting.ll_rgb_desired = 0;
	}
	if (dst_format == FORMAT_HDR10 &&
	    (dolby_vision_flags & FLAG_DOVI2HDR10_NOMAPPING))
		new_dovi_setting.dovi2hdr10_nomapping = 1;
	else
		new_dovi_setting.dovi2hdr10_nomapping = 0;

	/* always use rgb setting */
	new_dovi_setting.g_bitdepth = 8;
	new_dovi_setting.g_format = G_SDR_RGB;

	new_dovi_setting.diagnostic_enable = 0;
	new_dovi_setting.diagnostic_mux_select = 0;
	new_dovi_setting.dovi_ll_enable = 0;

	if (vinfo) {
		new_dovi_setting.vout_width = vinfo->width;
		new_dovi_setting.vout_height = vinfo->height;
	} else {
		new_dovi_setting.vout_width = 0;
		new_dovi_setting.vout_height = 0;
	}
	memset(&new_dovi_setting.ext_md, 0, sizeof(struct ext_md_s));
	new_dovi_setting.video_width = w << 16;
	new_dovi_setting.video_height = h << 16;

	if (debug_dolby & 0x400)
		do_gettimeofday(&start);
	if (is_aml_stb_hdmimode()) {
		/* generate core2/core3 setting */
		flag = p_funcs_stb->control_path
			(src_format, dst_format,
			comp_buf[current_id],
			(src_format == FORMAT_DOVI) ? total_comp_size : 0,
			md_buf[current_id],
			(src_format == FORMAT_DOVI) ? total_md_size : 0,
			V_PRIORITY,
			src_bdp, 0, SIGNAL_RANGE_SMPTE,
			graphic_min,
			graphic_max * 10000,
			amdv_target_min,
			amdv_target_max
			[src_format][dst_format] * 10000,
			false,
			&hdr10_param,
			&new_dovi_setting);
		/* overwrite core3 meta by hdmi input */
		if (dst_format == FORMAT_DOVI &&
		dolby_vision_ll_policy == DOLBY_VISION_LL_DISABLE)
			prepare_dv_meta
				(&new_dovi_setting.md_reg3,
				md_buf[current_id], total_md_size);
	} else {
		flag = p_funcs_stb->control_path
		(src_format, dst_format,
		comp_buf[current_id],
		(src_format == FORMAT_DOVI) ? total_comp_size : 0,
		md_buf[current_id],
		(src_format == FORMAT_DOVI) ? total_md_size : 0,
		pri_mode,
		src_bdp, 0, SIGNAL_RANGE_SMPTE, /* bit/chroma/range */
		graphic_min,
		graphic_max * 10000,
		amdv_target_min,
		amdv_target_max[src_format][dst_format] * 10000,
		(!el_flag && !mel_flag) ||
		(dolby_vision_flags & FLAG_DISABLE_COMPOSER),
		&hdr10_param,
		&new_dovi_setting);
	}
	if (debug_dolby & 0x400) {
		do_gettimeofday(&end);
		time_use = (end.tv_sec - start.tv_sec) * 1000000 +
				(end.tv_usec - start.tv_usec);

		pr_info("stb controlpath time: %5ld us\n", time_use);
	}
	if (flag >= 0) {
		update_stb_core_setting_flag(flag);
		if ((dolby_vision_flags & FLAG_FORCE_DV_LL) &&
		    dst_format == FORMAT_DOVI)
			new_dovi_setting.dovi_ll_enable = 1;
		if ((dolby_vision_flags & FLAG_FORCE_RGB_OUTPUT) &&
		    dst_format == FORMAT_DOVI) {
			new_dovi_setting.dovi_ll_enable = 1;
			new_dovi_setting.diagnostic_enable = 1;
			new_dovi_setting.diagnostic_mux_select = 1;
		}
		if (debug_dolby & 2)
			pr_dv_dbg("ll_enable=%d,diagnostic=%d,ll_policy=%d\n",
				     new_dovi_setting.dovi_ll_enable,
				     new_dovi_setting.diagnostic_enable,
				     dolby_vision_ll_policy);
		new_dovi_setting.src_format = src_format;
		new_dovi_setting.dst_format = dst_format;
		new_dovi_setting.el_flag = el_flag;
		new_dovi_setting.el_halfsize_flag = el_halfsize_flag;
		new_dovi_setting.video_width = w;
		new_dovi_setting.video_height = h;
		amdv_setting_video_flag = video_frame;
		if (debug_dolby & 1) {
			if (is_video_output_off(vf))
				pr_dv_dbg
				("setting %d->%d(T:%d-%d), osd:%dx%d\n",
				 src_format, dst_format,
				 amdv_target_min,
				 amdv_target_max
				 [src_format][dst_format],
				 osd_graphic_width[OSD1_INDEX],
				 osd_graphic_height[OSD1_INDEX]);
			if (el_flag) {
				pr_dv_dbg
					("v %d: %dx%d %d->%d(T:%d-%d), g %d: %dx%d %d->%d, %s\n",
					amdv_setting_video_flag,
					w == 0xffff ? 0 : w,
					h == 0xffff ? 0 : h,
					src_format, dst_format,
					amdv_target_min,
					amdv_target_max
					[src_format][dst_format],
					!is_graphics_output_off(),
					osd_graphic_width[OSD1_INDEX],
					osd_graphic_height[OSD1_INDEX],
					graphic_min,
					graphic_max * 10000,
					pri_mode == V_PRIORITY ?
					"vpr" : "gpr");
				pr_dv_dbg
					("flag=%x, md=%d, comp=%d, frame:%d\n",
					flag, total_md_size, total_comp_size,
					frame_count);
			} else {
				pr_dv_dbg("v %d: %dx%d %d->%d(T:%d-%d), g %d: %dx%d %d->%d, %s, flag=%x, md=%d, frame:%d\n",
					amdv_setting_video_flag,
					w == 0xffff ? 0 : w,
					h == 0xffff ? 0 : h,
					src_format, dst_format,
					amdv_target_min,
					amdv_target_max
					[src_format][dst_format],
					!is_graphics_output_off(),
					osd_graphic_width[OSD1_INDEX],
					osd_graphic_height[OSD1_INDEX],
					graphic_min,
					graphic_max * 10000,
					pri_mode == V_PRIORITY ?
					"vpr" : "gpr",
					flag,
					total_md_size, frame_count);
			}
		}
		dump_setting(&new_dovi_setting, frame_count, debug_dolby);
		mel_mode = mel_flag;
		return 0; /* setting updated */
	}
	if (flag < 0) {
		pr_dv_dbg("video %d:%dx%d setting %d->%d(T:%d-%d): pri_mode=%d, no_el=%d, md=%d, frame:%d\n",
			amdv_setting_video_flag,
			w == 0xffff ? 0 : w,
			h == 0xffff ? 0 : h,
			src_format, dst_format,
			amdv_target_min,
			amdv_target_max
			[src_format][dst_format],
			pri_mode,
			(!el_flag && !mel_flag),
			total_md_size, frame_count);
		new_dovi_setting.video_width = 0;
		new_dovi_setting.video_height = 0;
		pr_dv_error("control_path(%d, %d) failed %d\n",
			src_format, dst_format, flag);
		if ((debug_dolby & 0x2000) && dump_enable && total_md_size > 0) {
			pr_dv_dbg("control_path failed, md(%d):\n",
				total_md_size);
			for (i = 0; i < total_md_size + 7; i += 8)
				pr_info("%02x %02x %02x %02x %02x %02x %02x %02x\n",
					md_buf[current_id][i],
					md_buf[current_id][i + 1],
					md_buf[current_id][i + 2],
					md_buf[current_id][i + 3],
					md_buf[current_id][i + 4],
					md_buf[current_id][i + 5],
					md_buf[current_id][i + 6],
					md_buf[current_id][i + 7]);
		}
	}
	return -1; /* do nothing for this frame */
}

/*multi-inst tv mode, todo*/
int amdv_parse_metadata_v2_tv(struct vframe_s *vf,
						   u8 toggle_mode,
						   bool bypass_release,
						   bool drop_flag)
{
	return -1;
}

bool check_drm(u8 *drm_pkt)
{
	u8 *data_byte = &drm_pkt[3];
	int primaries_x[3], primaries_y[3];
	int index_r, index_g, index_b;
	int white_point_x_lsb;
	int white_point_y_lsb;
	int max_cll, max_fall;
	int max_disp_mastering_lumi;
	bool bt2020 = 0;

	/* CTA-861-G, 6.9.1:indices 0, 1, or 2 are determined by the following relationship: */
	/*The R color primary corresponds to the index of the largest primaries_x value. */
	/*The G  color primary corresponds to the index of the largest primaries_y value. */
	/*The B color primary corresponds to the index with neither the largest primaries_y */
	/*value nor the largest primaries_x value*/
	primaries_x[0] = (data_byte[4] << 8) | data_byte[3];
	primaries_y[0] = (data_byte[6] << 8) | data_byte[5];
	primaries_x[1] = (data_byte[8] << 8) | data_byte[7];
	primaries_y[1] = (data_byte[10] << 8) | data_byte[9];
	primaries_x[2] = (data_byte[12] << 8) | data_byte[11];
	primaries_y[2] = (data_byte[14] << 8) | data_byte[13];
	white_point_x_lsb = data_byte[15];
	white_point_y_lsb = data_byte[17];
	max_disp_mastering_lumi = ((data_byte[20] << 8) | data_byte[19]);
	max_cll  = (data_byte[24] << 8) | data_byte[23];
	max_fall = (data_byte[26] << 8) | data_byte[25];

	if (primaries_x[0] > primaries_x[1] &&
		primaries_x[0] > primaries_x[2])
		index_r = 0;
	else if (primaries_x[1] > primaries_x[2])
		index_r = 1;
	else
		index_r = 2;

	if (primaries_y[0] > primaries_y[1] && primaries_y[0] > primaries_y[2])
		index_g = 0;
	else if (primaries_y[1] > primaries_y[2])
		index_g = 1;
	else
		index_g = 2;

	if (index_r == 0 && index_g == 1)
		index_b = 2;
	else if (index_r == 0 && index_g == 2)
		index_b = 1;
	else if (index_r == 1 && index_g == 0)
		index_b = 2;
	else if (index_r == 1 && index_g == 2)
		index_b = 0;
	else if (index_r == 2 && index_g == 0)
		index_b = 1;
	else
		index_b = 0;

	/* check if it is DV signal from PC*/
	if (max_disp_mastering_lumi == 0x0000)
		bt2020 = 0;
	else if ((primaries_x[index_r] & 0xFFFE) != 0x8A26)
		bt2020 = 0;
	else if ((primaries_y[index_r] & 0xFFFE) != 0x3976)
		bt2020 = 0;
	else if ((primaries_x[index_g] & 0xFFFE) != 0x2108)
		bt2020 = 0;
	else if ((primaries_y[index_g] & 0xFFFE) != 0x9B7E)
		bt2020 = 0;
	else if ((primaries_x[index_b] & 0xFFFE) != 0x19FA)
		bt2020 = 0;
	else if ((primaries_y[index_b] & 0xFFFE) != 0x0846)
		bt2020 = 0;
	else if ((white_point_x_lsb & 0xFE) != 0xEE)
		bt2020 = 0;
	else if ((white_point_y_lsb & 0xFE) != 0x30)
		bt2020 = 0;
	else if ((max_cll & 0x00FE) != 0xFE)
		bt2020 = 0;
	else if ((max_fall & 0x00FE) != 0xAA)
		bt2020 = 0;
	else
		bt2020 = 1;

	if (debug_dolby & 1)
		pr_dv_dbg("bt2020 %d\n", bt2020);

	return bt2020;
}

static u8 force_drm[32] = {0};
static u8 force_drm_1[32] = {
0x87, 0x01, 0x1A, 0xF4, 0x02, 0x00, 0x26, 0x8A,
0x76, 0x39, 0x08, 0x21, 0x7E, 0x9B, 0xFA, 0x19,
0x46, 0x08, 0xEE, 0x3D, 0x30, 0x40, 0xE8, 0x03,
0x32, 0x00, 0xFE, 0x03, 0xAA, 0x03, 0x00};

static u8 force_drm_2[32] = {
0x87, 0x01, 0x1A, 0xF4, 0x02, 0x00, 0x26, 0x8A,
0x76, 0x39, 0xFA, 0x19, 0x46, 0x08, 0x08, 0x21,
0x7E, 0x9B, 0xEE, 0x3D, 0x30, 0x40, 0xE8, 0x03,
0x32, 0x00, 0xFE, 0x03, 0xAA, 0x03, 0x00};

static u8 force_drm_3[32] = {
0x87, 0x01, 0x1A, 0xF4, 0x02, 0x00, 0x08, 0x21,
0x7E, 0x9B, 0xFA, 0x19, 0x46, 0x08, 0x26, 0x8A,
0x76, 0x39, 0xEE, 0x3D, 0x30, 0x40, 0xE8, 0x03,
0x32, 0x00, 0xFE, 0x03, 0xAA, 0x03, 0x00};

static u8 force_drm_4[32] = {
0x87, 0x01, 0x1A, 0xF4, 0x02, 0x00, 0x08, 0x21,
0x7E, 0x9B, 0x26, 0x8A, 0x76, 0x39, 0xFA, 0x19,
0x46, 0x08, 0xEE, 0x3D, 0x30, 0x40, 0xE8, 0x03,
0x32, 0x00, 0xFE, 0x03, 0xAA, 0x03, 0x00};

static u8 force_drm_5[32] = {
0x87, 0x01, 0x1A, 0xF4, 0x02, 0x00, 0xFA, 0x19,
0x46, 0x08, 0x08, 0x21, 0x7E, 0x9B, 0x26, 0x8A,
0x76, 0x39, 0xEE, 0x3D, 0x30, 0x40, 0xE8, 0x03,
0x32, 0x00, 0xFE, 0x03, 0xAA, 0x03, 0x00};

static u8 force_drm_6[32] = {
0x87, 0x01, 0x1A, 0xF4, 0x02, 0x00, 0xFA, 0x19,
0x46, 0x08, 0x26, 0x8A, 0x76, 0x39, 0x08, 0x21,
0x7E, 0x9B, 0xEE, 0x3D, 0x30, 0x40, 0xE8, 0x03,
0x32, 0x00, 0xFE, 0x03, 0xAA, 0x03, 0x00};

static u8 force_drm_7[32] = {
0x87, 0x01, 0x1A, 0xEC, 0x02, 0x00, 0x27, 0x8A,
0x77, 0x39, 0x09, 0x21, 0x7F, 0x9B, 0xFB, 0x19,
0x47, 0x08, 0xEF, 0x3D, 0x31, 0x40, 0xE8, 0x03,
0x32, 0x00, 0xFE, 0x03, 0xAA, 0x03, 0x00};

bool is_dv_unique_drm(struct vframe_s *vf)
{
	int i;
	u8 *drm_if;

	if (!vf)
		return false;

	if (force_hdmin_fmt >= 3) {/*debug mode*/
		if (force_hdmin_fmt == 3) {
			for (i = 0; i < 32; i++)
				force_drm[i] = force_drm_1[i];
		} else if (force_hdmin_fmt == 4) {
			for (i = 0; i < 32; i++)
				force_drm[i] = force_drm_2[i];
		} else if (force_hdmin_fmt == 5) {
			for (i = 0; i < 32; i++)
				force_drm[i] = force_drm_3[i];
		} else if (force_hdmin_fmt == 6) {
			for (i = 0; i < 32; i++)
				force_drm[i] = force_drm_4[i];
		} else if (force_hdmin_fmt == 7) {
			for (i = 0; i < 32; i++)
				force_drm[i] = force_drm_5[i];
		} else if (force_hdmin_fmt == 8) {
			for (i = 0; i < 32; i++)
				force_drm[i] = force_drm_6[i];
		} else if (force_hdmin_fmt == 9) {
			for (i = 0; i < 32; i++)
				force_drm[i] = force_drm_7[i];
		}
		if (force_drm[0] == 0x87 && force_drm[1] == 0x01 &&
			force_drm[2] == 0x1A && force_drm[4] == 0x2 &&
			force_drm[5] == 0x0)
			return check_drm(force_drm);
	} else {
		/*vdin bind drm pkt to vf*/
		drm_if = vf->drm_if.addr;
		if (vf->drm_if.size > 0 && drm_if && drm_if[0] == 0x87 && drm_if[1] == 0x01 &&
		   drm_if[2] == 0x1A && drm_if[4] == 0x2 && drm_if[5] == 0x0)
			return check_drm(drm_if);
	}

	return false;
}

/* toggle mode: 0: not toggle; 1: toggle frame; 2: use keep frame */
/* ret 0: parser done for v2*/
/* ret 1: both dolby and hdr module bypass */
/* ret 2: fail generating setting for this frame */
/* ret -1: do nothing */

int amdv_parse_metadata_v2_stb(struct vframe_s *vf,
						    enum vd_path_e vd_path,
						    u8 toggle_mode,
						    bool bypass_release,
						    bool drop_flag)
{
	const struct vinfo_s *vinfo = get_current_vinfo();
	struct vframe_s *el_vf;
	struct provider_aux_req_s req;
	struct provider_aux_req_s el_req;
	enum signal_format_enum src_format = FORMAT_INVALID;
	enum signal_format_enum tmp_fmt = FORMAT_SDR;
	enum signal_format_enum check_format;
	enum signal_format_enum dst_format;
	int total_md_size = 0;
	int total_comp_size = 0;
	int vsem_size = 0;
	int vsem_if_size = 0;
	bool dump_emp = false;
	bool dv_vsem = false;
	bool el_flag = 0;
	bool el_halfsize_flag = 1;
	u32 w = 0xffff;
	u32 h = 0xffff;
	int meta_flag_bl = 1;
	int meta_flag_el = 1;
	int src_bdp = 12;
	bool video_frame = false;
	int i;
	struct vframe_master_display_colour_s *p_mdc;
	unsigned int current_mode = dolby_vision_mode;
	u32 target_lumin_max = 0;
	enum input_mode_enum input_mode = IN_MODE_OTT;
	u32 graphic_min = 50; /* 0.0001 */
	static u32 graphic_max = 100; /* 1 */
	int ret_flags = 0;
	static int bypass_frame = -1;
	static int last_current_format;
	int ret = -1;
	bool mel_flag = false;
	u32 cur_md_id;
	int dv_id = -1;
	char *dvel_provider = NULL;
	u32 cur_use_ll_flag;
	u32 cur_ll_rgb_desired;

	memset(&req, 0, (sizeof(struct provider_aux_req_s)));
	memset(&el_req, 0, (sizeof(struct provider_aux_req_s)));

	if (!dolby_vision_enable || !module_installed || !p_funcs_stb)
		return -1;

	if (vd_path > VD2_PATH) {
		pr_dv_dbg("not support vd%d\n", vd_path + 1);
		vd_path = VD1_PATH;
	}
	if (vf)
		src_format = FORMAT_SDR; /*SDR8 by default*/
	if (vf) {
		video_frame = true;
		w = (vf->type & VIDTYPE_COMPRESS) ?
			vf->compWidth : vf->width;
		h = (vf->type & VIDTYPE_COMPRESS) ?
			vf->compHeight : vf->height;

		dv_id = vf->src_fmt.dv_id;
		if (!dv_inst_valid(dv_id)) {
			pr_err("[%s]err inst %d\n", __func__, dv_id);
			dv_id = 0;
		}

	} //else {
		//dv_id = layer_id_to_dv_id(vd_path);
	//}
	if (debug_dolby & 0x1000)
		pr_dv_dbg("[inst%d]parse_metadata %p on vd%d, toggle %d\n",
			     dv_id + 1, vf, vd_path + 1, toggle_mode);

	if (vf && vf->source_type == VFRAME_SOURCE_TYPE_OTHERS) {
		enum vframe_signal_fmt_e fmt;

		input_mode = IN_MODE_OTT;
		req.vf = vf;
		req.bot_flag = 0;
		req.aux_buf = NULL;
		req.aux_size = 0;
		req.dv_enhance_exist = 0;

		/* check source format */
		fmt = get_vframe_src_fmt(vf);
		if ((fmt == VFRAME_SIGNAL_FMT_DOVI ||
		    fmt == VFRAME_SIGNAL_FMT_INVALID) &&
		    !vf->discard_dv_data) {
			vf_notify_provider_by_name
				(dv_provider[vd_path],
				 VFRAME_EVENT_RECEIVER_GET_AUX_DATA,
				 (void *)&req);
			/* use callback aux date first, if invalid, use sei_ptr */
			if ((!req.aux_buf || !req.aux_size) &&
			    fmt == VFRAME_SIGNAL_FMT_DOVI) {
				u32 sei_size = 0;
				char *sei;

				if (debug_dolby & 1)
					pr_dv_dbg("no aux %p %x, el %d from %s, use sei_ptr\n",
						     req.aux_buf,
						     req.aux_size,
						     req.dv_enhance_exist,
						     dv_provider[vd_path]);

				sei = (char *)get_sei_from_src_fmt
					(vf, &sei_size);
				if (sei && sei_size) {
					req.aux_buf = sei;
					req.aux_size = sei_size;
					req.dv_enhance_exist =
						vf->src_fmt.dual_layer;
				}
			}
		}
		if (debug_dolby & 1)
			pr_dv_dbg("[inst%d vd%d]%s get %p(%d,index %d),fmt %d,aux %p %x,el %d\n",
				     dv_id + 1,
				     vd_path + 1,
				     dv_provider[vd_path], vf, vf->discard_dv_data,
				     vf->omx_index, fmt,
				     req.aux_buf,
				     req.aux_size,
				     req.dv_enhance_exist);
		/* parse meta in base layer */
		if (toggle_mode != 2) {
			ret = get_md_from_src_fmt(vf);
			if (ret == 1) { /*parse succeeded*/
				meta_flag_bl = 0;
				src_format = FORMAT_DOVI;
				dv_inst[dv_id].current_id = dv_inst[dv_id].current_id ^ 1;
				cur_md_id = dv_inst[dv_id].current_id;
				memcpy(dv_inst[dv_id].md_buf[cur_md_id],
				       vf->src_fmt.md_buf,
				       vf->src_fmt.md_size);
				memcpy(dv_inst[dv_id].comp_buf[cur_md_id],
				       vf->src_fmt.comp_buf,
				       vf->src_fmt.comp_size);

				total_md_size =  vf->src_fmt.md_size;
				total_comp_size =  vf->src_fmt.comp_size;
				ret_flags = vf->src_fmt.parse_ret_flags;
				if ((debug_dolby & 4) && dump_enable_f(dv_id))  {
					pr_dv_dbg("[inst%d]get md_buf %p, size(%d):\n",
						dv_id + 1, vf->src_fmt.md_buf,
						vf->src_fmt.md_size);
					for (i = 0; i < total_md_size; i += 8)
						pr_info("%02x %02x %02x %02x %02x %02x %02x %02x\n",
						dv_inst[dv_id].md_buf[cur_md_id][i],
						dv_inst[dv_id].md_buf[cur_md_id][i + 1],
						dv_inst[dv_id].md_buf[cur_md_id][i + 2],
						dv_inst[dv_id].md_buf[cur_md_id][i + 3],
						dv_inst[dv_id].md_buf[cur_md_id][i + 4],
						dv_inst[dv_id].md_buf[cur_md_id][i + 5],
						dv_inst[dv_id].md_buf[cur_md_id][i + 6],
						dv_inst[dv_id].md_buf[cur_md_id][i + 7]);
				}
				if ((debug_dolby & 0x40000) && dump_enable_f(dv_id))  {
					pr_dv_dbg("[inst%d]get comp_buf %p, size(%d):\n",
						dv_id + 1, vf->src_fmt.comp_buf,
						vf->src_fmt.comp_size);
					for (i = 0; i < 100; i += 8)
						pr_info("%02x %02x %02x %02x %02x %02x %02x %02x\n",
						dv_inst[dv_id].comp_buf[cur_md_id][i],
						dv_inst[dv_id].comp_buf[cur_md_id][i + 1],
						dv_inst[dv_id].comp_buf[cur_md_id][i + 2],
						dv_inst[dv_id].comp_buf[cur_md_id][i + 3],
						dv_inst[dv_id].comp_buf[cur_md_id][i + 4],
						dv_inst[dv_id].comp_buf[cur_md_id][i + 5],
						dv_inst[dv_id].comp_buf[cur_md_id][i + 6],
						dv_inst[dv_id].comp_buf[cur_md_id][i + 7]);
				}
			} else {  /*no parse or parse failed*/
				if (get_vframe_src_fmt(vf) ==
				    VFRAME_SIGNAL_FMT_HDR10PRIME)
					src_format = FORMAT_PRIMESL;
				else
					meta_flag_bl =
					parse_sei_and_meta
						(vf, &req,
						 &total_comp_size,
						 &total_md_size,
						 &src_format,
						 &ret_flags, drop_flag,
						 dv_id);
			}
			if (force_mel)
				ret_flags = 1;

			if (ret_flags && req.dv_enhance_exist) {
				if (!strcmp(dv_provider[vd_path], "dvbldec") ||
					!strcmp(dv_provider[vd_path], "dvbldec2"))
					vf_notify_provider_by_name
						(dv_provider[vd_path],
						 VFRAME_EVENT_RECEIVER_DOLBY_BYPASS_EL,
						 (void *)&req);
				pr_dv_dbg("bypass MEL\n");
			}

			if (ret_flags == 1)
				mel_flag = true;
			if (!is_dv_standard_es(req.dv_enhance_exist,
				ret_flags, w)) {
				src_format = FORMAT_SDR;
				/* dovi_setting.src_format = src_format; */
				total_comp_size = 0;
				total_md_size = 0;
				src_bdp = 10;
				/* bypass_release = true; */
				req.dv_enhance_exist = 0;
				amdv_el_disable = 1;
				if (debug_dolby & 1)
					pr_dv_dbg("bypass FEL\n");
			}
			if (is_amdv_stb_mode() &&
				(req.dv_enhance_exist && !mel_flag &&
				((dolby_vision_flags & FLAG_CERTIFICATION) == 0)) &&
				!enable_fel) {
				src_format = FORMAT_SDR;
				/* dovi_setting.src_format = src_format; */
				total_comp_size = 0;
				total_md_size = 0;
				src_bdp = 10;
				/* bypass_release = true; */
				req.dv_enhance_exist = 0;
				amdv_el_disable = 1;
				if (debug_dolby & 1)
					pr_dv_dbg("stb: bypass FEL\n");
			}
		} else if (is_amdv_stb_mode()) {
			src_format = m_dovi_setting.input[dv_id].src_format;
		}

		if (src_format != FORMAT_DOVI && is_primesl_frame(vf)) {
			src_format = FORMAT_PRIMESL;
			src_bdp = 10;
		}

		if (src_format != FORMAT_DOVI && is_hdr10_frame(vf)) {
			src_format = FORMAT_HDR10;
			/* prepare parameter from SEI for hdr10 */
			p_mdc = &vf->prop.master_display_colour;
			prepare_hdr10_param(p_mdc, &dv_inst[dv_id].hdr10_param);

			/* for 962x with v1.4 or stb with v2.3 may use 12 bit */
			src_bdp = 10;
			req.dv_enhance_exist = 0;
		}

		if (src_format != FORMAT_DOVI && is_hlg_frame(vf)) {
			src_format = FORMAT_HLG;
			if (is_aml_tm2_tvmode())
				src_bdp = 10;
		}

		if (src_format != FORMAT_DOVI && is_hdr10plus_frame(vf))
			src_format = FORMAT_HDR10PLUS;

		if (src_format != FORMAT_DOVI && is_mvc_frame(vf))
			src_format = FORMAT_MVC;

		if (src_format != FORMAT_DOVI && is_cuva_frame(vf))
			src_format = FORMAT_CUVA;

		/* TODO: need 962e ? */
		if (src_format == FORMAT_SDR &&
		    is_amdv_stb_mode() &&
		    !req.dv_enhance_exist)
			src_bdp = 10;
		if (src_format == FORMAT_SDR &&
		     is_aml_tm2_tvmode() &&
		     !req.dv_enhance_exist)
			src_bdp = 10;

		if (((debug_dolby & 1) || dv_inst[dv_id].frame_count == 0) &&
		    toggle_mode == 1)
			pr_dv_dbg("[inst%d]parse_metadata %d,%lld,%d,%s,%d,%d\n",
				dv_id + 1,
				dv_inst[dv_id].frame_count, vf->pts_us64, src_bdp,
				(src_format == FORMAT_HDR10) ? "HDR10" :
				(src_format == FORMAT_DOVI ? "DOVI" :
				(src_format == FORMAT_HLG ? "HLG" :
				(src_format == FORMAT_HDR10PLUS ? "HDR10+" :
				(src_format == FORMAT_CUVA ? "CUVA" :
				(src_format == FORMAT_PRIMESL ? "PRIMESL" :
				(req.dv_enhance_exist ? "DOVI (el)" : "SDR")))))),
				req.aux_size, req.dv_enhance_exist);
		if (src_format != FORMAT_DOVI && !req.dv_enhance_exist)
			memset(&req, 0, sizeof(req));

		/* check dvel decoder is active, if active, should */
		/* get/put el data, otherwise, dvbl is stuck */
		dvel_provider = vf_get_provider_name(DVEL_RECV_NAME);
		if (req.dv_enhance_exist && toggle_mode == 1 &&
		    dvel_provider && !strcmp(dvel_provider, "dveldec")) {
			el_vf = dvel_vf_get();
			if (el_vf &&  (el_vf->pts_us64 == vf->pts_us64 ||
			    !(dolby_vision_flags & FLAG_CHECK_ES_PTS))) {
				if (debug_dolby & 2)
					pr_dv_dbg("+++ get bl(%p-%lld) with el(%p-%lld) +++\n",
						vf, vf->pts_us64,
						el_vf, el_vf->pts_us64);
				if (meta_flag_bl) {
					int el_md_size = 0;
					int el_comp_size = 0;

					el_req.vf = el_vf;
					el_req.bot_flag = 0;
					el_req.aux_buf = NULL;
					el_req.aux_size = 0;
					if (!strcmp(dv_provider[vd_path], "dvbldec") ||
						!strcmp(dv_provider[vd_path], "dvbldec2"))
						vf_notify_provider_by_name
						(dv_provider[vd_path],
						 VFRAME_EVENT_RECEIVER_GET_AUX_DATA,
						 (void *)&el_req);
					if (el_req.aux_buf && el_req.aux_size) {
						meta_flag_el =
							parse_sei_and_meta
							(el_vf, &el_req,
							 &el_comp_size,
							 &el_md_size,
							 &src_format,
							 &ret_flags, drop_flag,
							 dv_id);
					}
					if (!meta_flag_el) {
						total_comp_size =
							el_comp_size;
						total_md_size =
							el_md_size;
						src_bdp = 12;
					}
					/* force set format as DOVI*/
					/*	when meta data error */
					if (meta_flag_el && el_req.aux_buf && el_req.aux_size)
						src_format = FORMAT_DOVI;
					if (debug_dolby & 2)
						pr_dv_dbg
						("el mode:src_fmt:%d,meta_flag_el: %d\n",
						src_format,
						meta_flag_el);
					if (meta_flag_el && dv_inst[dv_id].frame_count == 0)
						pr_info
						("el mode:parser err,aux %p,size:%d\n",
						 el_req.aux_buf,
						 el_req.aux_size);
				}
				amdvdolby_vision_vf_add(vf, el_vf);
				el_flag = 1;
				if (vf->width == el_vf->width)
					el_halfsize_flag = 0;
			} else {
				if (!el_vf)
					pr_dv_error
					("bl(%p-%lld) not found el\n",
					 vf, vf->pts_us64);
				else
					pr_dv_error
					("bl(%p-%lld) not found el(%p-%lld)\n",
					 vf, vf->pts_us64,
					 el_vf, el_vf->pts_us64);
			}
		} else if (toggle_mode == 1) {
			if (debug_dolby & 2)
				pr_dv_dbg("+++ get bl(%p-%lld) +++\n",
					     vf, vf->pts_us64);
			amdvdolby_vision_vf_add(vf, NULL);
		}

		if (req.dv_enhance_exist)
			el_flag = 1;

		if (toggle_mode != 2) {
			if (!drop_flag) {
				dv_inst[dv_id].last_total_md_size = total_md_size;
				dv_inst[dv_id].last_total_comp_size = total_comp_size;
				dv_inst[dv_id].last_mel_mode = mel_flag;
			}
		} else if (meta_flag_bl && meta_flag_el) {
			total_md_size = m_dovi_setting.input[dv_id].in_md_size;
			total_comp_size = m_dovi_setting.input[dv_id].in_comp_size;
			el_flag = m_dovi_setting.input[dv_id].el_flag;
			mel_flag = dv_inst[dv_id].last_mel_mode;
			if (debug_dolby & 2)
				pr_dv_dbg("update el_flag %d, melFlag %d\n",
					     el_flag, mel_flag);
			meta_flag_bl = 0;
			if (total_md_size == 0)
				src_format = FORMAT_SDR;
		}
		if (el_flag && !enable_mel)
			el_flag = 0;
		if ((el_flag && !mel_flag &&
			((dolby_vision_flags & FLAG_CERTIFICATION) == 0)) &&
			!enable_fel) {
			el_flag = 0;
			amdv_el_disable = 1;
		}
		if (src_format != FORMAT_DOVI) {
			el_flag = 0;
			mel_flag = 0;
		}
	} else if (vf && (vf->source_type == VFRAME_SOURCE_TYPE_HDMI) &&
		(is_aml_tm2_stbmode() || is_aml_t7_stbmode()) && hdmi_to_stb_policy) {
		if (vf->flag & VFRAME_FLAG_ALLM_MODE)
			hdmi_in_allm = true;
		else
			hdmi_in_allm = false;

		req.vf = vf;
		req.bot_flag = 0;
		req.aux_buf = NULL;
		req.aux_size = 0;
		req.dv_enhance_exist = 0;
		req.low_latency = 0;

		vf_notify_provider_by_name("dv_vdin",
			VFRAME_EVENT_RECEIVER_GET_AUX_DATA,
			(void *)&req);

		/* meta */
		if ((dolby_vision_flags & FLAG_RX_EMP_VSEM) &&
			vf->emp.size > 0) {
			vsem_size = vf->emp.size * VSEM_PKT_SIZE;
			memcpy(vsem_if_buf, vf->emp.addr, vsem_size);
			if (vsem_if_buf[0] == 0x7f &&
			    vsem_if_buf[10] == 0x46 &&
			    vsem_if_buf[11] == 0xd0) {
				dv_vsem = true;
				if (!vsem_check(vsem_if_buf, vsem_md_buf)) {
					vsem_if_size = vsem_size;
					if (!vsem_md_buf[10]) {
						req.aux_buf =
							&vsem_md_buf[13];
						req.aux_size =
							(vsem_md_buf[5] << 8)
							+ vsem_md_buf[6]
							- 6 - 4;
						/* cancel vsem, use md */
						/* vsem_if_size = 0; */
					} else {
						req.low_latency = 1;
					}
				} else {
					/* emp error, use previous md */
					pr_dv_dbg("EMP packet error %d\n",
						vf->emp.size);
					dump_emp = true;
					vsem_if_size = 0;
					req.aux_buf = NULL;
					req.aux_size = dv_inst[dv_id].last_total_md_size;
				}
			} else if (debug_dolby & 4) {
				dv_vsem = false;
				pr_dv_dbg("EMP packet not DV vsem %d\n",
					vf->emp.size);
				dump_emp = true;
			}
			if ((debug_dolby & 4) || dump_emp) {
				pr_info("vsem pkt count = %d\n", vf->emp.size);
				for (i = 0; i < vsem_size; i += 8) {
					pr_info("%02x %02x %02x %02x %02x %02x %02x %02x\n",
						vsem_if_buf[i],
						vsem_if_buf[i + 1],
						vsem_if_buf[i + 2],
						vsem_if_buf[i + 3],
						vsem_if_buf[i + 4],
						vsem_if_buf[i + 5],
						vsem_if_buf[i + 6],
						vsem_if_buf[i + 7]);
				}
			}
		}
		dv_unique_drm = is_dv_unique_drm(vf);
		dv_inst[dv_id].dv_unique_drm = dv_unique_drm;
		/* w/t vsif and no dv_vsem */
		if (vf->vsif.size && !dv_vsem) {
			memset(vsem_if_buf, 0, VSEM_IF_BUF_SIZE);
			memcpy(vsem_if_buf, vf->vsif.addr, vf->vsif.size);
			vsem_if_size = vf->vsif.size;
			if (debug_dolby & 4) {
				pr_info("vsif size = %d\n", vf->vsif.size);
				for (i = 0; i < vsem_if_size; i += 8) {
					pr_info("%02x %02x %02x %02x %02x %02x %02x %02x\n",
						vsem_if_buf[i],
						vsem_if_buf[i + 1],
						vsem_if_buf[i + 2],
						vsem_if_buf[i + 3],
						vsem_if_buf[i + 4],
						vsem_if_buf[i + 5],
						vsem_if_buf[i + 6],
						vsem_if_buf[i + 7]);
				}
			}
		} else if (dv_unique_drm && !dv_vsem) { /* dv unique drm and no dv_vsem */
			memset(vsem_if_buf, 0, VSEM_IF_BUF_SIZE);
			if (force_hdmin_fmt >= 3 && force_hdmin_fmt <= 9) {
				memcpy(vsem_if_buf, force_drm, 32);
				vsem_if_size = 32;
			} else if (vf->drm_if.size > 0 && vf->drm_if.addr) {
				memcpy(vsem_if_buf, vf->drm_if.addr, vf->drm_if.size);
				vsem_if_size = vf->drm_if.size;
			}
			if (debug_dolby & 4) {
				pr_info("drm size = %d\n", vf->drm_if.size);
				for (i = 0; i < vsem_if_size; i += 8) {
					pr_info("%02x %02x %02x %02x %02x %02x %02x %02x\n",
						vsem_if_buf[i],
						vsem_if_buf[i + 1],
						vsem_if_buf[i + 2],
						vsem_if_buf[i + 3],
						vsem_if_buf[i + 4],
						vsem_if_buf[i + 5],
						vsem_if_buf[i + 6],
						vsem_if_buf[i + 7]);
				}
			}
		}

		if (debug_dolby & 1) {
			if (dv_vsem || vsem_if_size)
				pr_dv_dbg("get %s:%d, md:%p %d,ll:%d,bit %x,type %x\n",
					dv_vsem ? "vsem" : "vsif",
					dv_vsem ? vsem_size : vsem_if_size,
					req.aux_buf, req.aux_size,
					req.low_latency,
					vf->bitdepth, vf->type);
			else
				pr_dv_dbg("get aux data %p %d,ll:%d,bit %x,type %x\n",
					req.aux_buf, req.aux_size,
					req.low_latency, vf->bitdepth, vf->type);
		}

		/*check vsem_if_buf */
		if (!dv_unique_drm && vsem_if_size &&
			vsem_if_buf[0] != 0x81) {
			/*not vsif, continue to check vsem*/
			if (!(vsem_if_buf[0] == 0x7F &&
				vsem_if_buf[1] == 0x80 &&
				vsem_if_buf[10] == 0x46 &&
				vsem_if_buf[11] == 0xd0 &&
				vsem_if_buf[12] == 0x00)) {
				vsem_if_size = 0;
				pr_dv_dbg("vsem_if_buf is invalid!\n");
				pr_dv_dbg("%x %x %x %x %x %x %x %x %x %x %x %x\n",
					vsem_if_buf[0],
					vsem_if_buf[1],
					vsem_if_buf[2],
					vsem_if_buf[3],
					vsem_if_buf[4],
					vsem_if_buf[5],
					vsem_if_buf[6],
					vsem_if_buf[7],
					vsem_if_buf[8],
					vsem_if_buf[9],
					vsem_if_buf[10],
					vsem_if_buf[11]);
			}
		}

		/*stb mode: hdmi LL in case, regard as HDR10*/
		if (req.low_latency == 1) {
			if (hdmi_source_led_as_hdr10) {
				src_format = FORMAT_HDR10;
				if (!vf_is_hdr10(vf)) {
					vf->signal_type &= 0xff0000ff;
					vf->signal_type |= 0x00091000;
				}
				p_mdc = &vf->prop.master_display_colour;
				prepare_hdr10_param(p_mdc, &dv_inst[dv_id].hdr10_param);

				src_bdp = 10;
				req.aux_size = 0;
				req.aux_buf = NULL;
			} else {
				src_format = FORMAT_DOVI_LL;
				input_mode = IN_MODE_HDMI;
				src_bdp = 12;
				req.aux_size = 0;
				req.aux_buf = NULL;
			}
		} else if (req.aux_size) {
			if (req.aux_buf) {
				dv_inst[dv_id].current_id = dv_inst[dv_id].current_id ^ 1;
				memcpy(dv_inst[dv_id].md_buf[dv_inst[dv_id].current_id],
				       req.aux_buf, req.aux_size);
			}
			src_format = FORMAT_DOVI;
			input_mode = IN_MODE_HDMI;
			src_bdp = 12;
			meta_flag_bl = 0;
			el_flag = 0;
			mel_flag = 0;
			if ((debug_dolby & 4) && dump_enable_f(dv_id)) {
				pr_dv_dbg("[inst%d]metadata(%d):\n", dv_id + 1, req.aux_size);
				cur_md_id = dv_inst[dv_id].current_id;
				for (i = 0; i < req.aux_size; i += 8)
					pr_info("\t%02x %02x %02x %02x %02x %02x %02x %02x\n",
						dv_inst[dv_id].md_buf[cur_md_id][i],
						dv_inst[dv_id].md_buf[cur_md_id][i + 1],
						dv_inst[dv_id].md_buf[cur_md_id][i + 2],
						dv_inst[dv_id].md_buf[cur_md_id][i + 3],
						dv_inst[dv_id].md_buf[cur_md_id][i + 4],
						dv_inst[dv_id].md_buf[cur_md_id][i + 5],
						dv_inst[dv_id].md_buf[cur_md_id][i + 6],
						dv_inst[dv_id].md_buf[cur_md_id][i + 7]);
			}
		} else {
			if (toggle_mode == 2)
				src_format = m_dovi_setting.input[dv_id].src_format;

			if (dv_unique_drm) {
				src_format = FORMAT_DOVI_LL;
				input_mode = IN_MODE_HDMI;
				src_bdp = 12;
				req.aux_size = 0;
				req.aux_buf = NULL;
			} else {
				if (is_hdr10_frame(vf) || force_hdmin_fmt == 1) {
					src_format = FORMAT_HDR10;
					p_mdc = &vf->prop.master_display_colour;
					if (p_mdc->present_flag)
						p_mdc->luminance[0] *= 10000;
					prepare_hdr10_param(p_mdc, &dv_inst[dv_id].hdr10_param);
				}

				if (is_hlg_frame(vf) || force_hdmin_fmt == 2)
					src_format = FORMAT_HLG;

				if (is_hdr10plus_frame(vf))
					src_format = FORMAT_HDR10PLUS;

				input_mode = IN_MODE_HDMI;
				src_bdp = 10;
			}
		}
		total_md_size = req.aux_size;
		total_comp_size = 0;
		if (toggle_mode != 2) {
			dv_inst[dv_id].last_total_md_size = total_md_size;
			dv_inst[dv_id].last_total_comp_size = total_comp_size;
		}
	}

	if (src_format == FORMAT_DOVI && meta_flag_bl && meta_flag_el) {
		/* dovi frame no meta or meta error */
		/* use old setting for this frame   */
		pr_dv_dbg("no meta or meta err!\n");
		return -1;
	}

	/* if not DOVI, release metadata_parser */
	/*src fmt from dv-> no dv*/
	//if (vf && src_format != FORMAT_DOVI &&
	//    dv_parser[dv_id].parser_src_format == FORMAT_DOVI &&
	//    dv_parser[dv_id].metadata_parser && !bypass_release) {
	//	if (m_dovi_setting.input[dv_id].src_format == FORMAT_DOVI) {
	//		p_funcs_stb->multi_mp_reset(dv_parser[dv_id].metadata_parser, 1);
	//		pr_dv_dbg("reset mp\n");
	//	}
	//}
	//dv_parser[dv_id].parser_src_format = src_format;

	if (drop_flag) {
		if (dv_inst_valid(dv_id))
			pr_dv_dbg("drop frame_count %d\n", dv_inst[dv_id].frame_count);
		return 2; //no need update new_m_dovi_setting for dropping frame
	}

	check_format = src_format;
	if (vf) {
		update_src_format(check_format, vf);
		last_current_format = check_format;
	}

	if (vd_path != VD2_PATH) {//only policy process for pri input, VD1.
		if (dolby_vision_request_mode != 0xff) {
			dolby_vision_mode = dolby_vision_request_mode;
			dolby_vision_request_mode = 0xff;
		}
		current_mode = dolby_vision_mode;
		if (amdv_policy_process
			(vf, &current_mode, check_format)) {
			if (!dv_inst[pri_input].amdv_wait_init) {
				amdv_set_toggle_flag(1);
			    amdv_wait_on = true;
			}
			pr_info("[%s] output change from %d to %d(%d, %p, %d)\n",
				__func__, dolby_vision_mode, current_mode,
				toggle_mode, vf, src_format);
			amdv_target_mode = current_mode;
			dolby_vision_mode = current_mode;
			if (is_amdv_stb_mode())
				new_m_dovi_setting.mode_changed = 1;
		} else {
			/*not clear target mode when:*/
			/*no mode change && no vf && target is not bypass */
			if ((!vf && amdv_target_mode != dolby_vision_mode &&
			    amdv_target_mode !=
			    AMDV_OUTPUT_MODE_BYPASS)) {
				if (debug_dolby & 8)
					pr_dv_dbg("not update target mode %d\n",
						  amdv_target_mode);
			} else if (amdv_target_mode != dolby_vision_mode) {
				if (debug_dolby & 8)
					pr_dv_dbg("update target mode %d=>%d\n",
						  amdv_target_mode, dolby_vision_mode);
				amdv_target_mode = dolby_vision_mode;
			}
		}
	}

	if (vf && (debug_dolby & 8))
		pr_dv_dbg("[inst%d]parse_metadata: vf %p(index %d), mode %d\n",
			  dv_id + 1, vf, vf->omx_index, dolby_vision_mode);

	if (get_hdr_module_status(VD1_PATH, VPP_TOP0) != HDR_MODULE_ON &&
	    check_format == FORMAT_SDR) {
		/* insert 2 SDR frames before send DOVI */
		if ((dolby_vision_mode == AMDV_OUTPUT_MODE_IPT_TUNNEL ||
		    dolby_vision_mode == AMDV_OUTPUT_MODE_IPT) &&
		    (last_current_format == FORMAT_HLG ||
		     last_current_format == FORMAT_PRIMESL ||
		    last_current_format == FORMAT_HDR10PLUS)) {
			/* TODO: if need add primesl */
			bypass_frame = 0;
			pr_dv_dbg("[%s] source transition from %d to %d\n",
				     __func__, last_current_format, check_format);
		}
		last_current_format = check_format;
	}

	if (bypass_frame >= 0 && bypass_frame < MIN_TRANSITION_DELAY) {
		dolby_vision_mode = AMDV_OUTPUT_MODE_BYPASS;
		bypass_frame++;
	} else {
		bypass_frame = -1;
	}

	if (dolby_vision_mode == AMDV_OUTPUT_MODE_BYPASS) {
		for (i = 0; i < NUM_IPCORE1; i++) {
			new_m_dovi_setting.input[i].video_width = 0;
			new_m_dovi_setting.input[i].video_height = 0;
		}
		new_m_dovi_setting.mode_changed = 0;
		if (amdv_target_mode == AMDV_OUTPUT_MODE_BYPASS)
			amdv_wait_on = false;
		if (debug_dolby & 8)
			pr_dv_dbg("now bypass mode, target %d, wait %d\n",
				     amdv_target_mode,
				     amdv_wait_on);
		if (get_hdr_module_status(VD1_PATH, VPP_TOP0) == HDR_MODULE_BYPASS)
			return 1;
		return -1;
	}

	/* update input mode for HDMI in STB core */
	if ((is_aml_tm2_stbmode() || is_aml_t7_stbmode()) &&
	    vf && dv_id == hdmi_inst_id && tv_dovi_setting) {
		tv_dovi_setting->input_mode = input_mode;
		if (is_aml_stb_hdmimode() && !core1_detunnel()) {
			tv_dovi_setting->src_format = src_format;
			tv_dovi_setting->video_width = w;
			tv_dovi_setting->video_height = h;
			tv_dovi_setting->el_flag = false;
			tv_dovi_setting->el_halfsize_flag = false;
			amdv_run_mode_delay = RUN_MODE_DELAY;
		} else {
			amdv_run_mode_delay = 0;
		}
	}

	/* check dst format */
	if (dolby_vision_mode == AMDV_OUTPUT_MODE_IPT_TUNNEL ||
	    dolby_vision_mode == AMDV_OUTPUT_MODE_IPT)
		dst_format = FORMAT_DOVI;
	else if (dolby_vision_mode == AMDV_OUTPUT_MODE_HDR10)
		dst_format = FORMAT_HDR10;
	else
		dst_format = FORMAT_SDR;

	/* STB core */
	/* check target luminance */
	graphic_min = amdv_graphic_min;

	if (multi_dv_mode) {
		if (amdv_graphic_max != 0) {
			graphic_max = amdv_graphic_max;
		} else if (dolby_vision_flags & FLAG_CERTIFICATION) {
			/*if max not set in cmdline, cmodel use default 1000 for all fmt*/
			if (graphic_fmt == FORMAT_SDR)
				graphic_max = 1000;/*300*/
			else if (graphic_fmt == FORMAT_HDR8)
				graphic_max = 1000;
			else
				graphic_max = 300;
		} else {
			if (vd_path == VD1_PATH) {//graphic max only update with pri input, VD1.
				if (src_format >= 0 && src_format <
				    ARRAY_SIZE(dv_target_graphics_max_26))
					tmp_fmt = src_format;
				if ((dolby_vision_flags & FLAG_FORCE_DV_LL) ||
				    dolby_vision_ll_policy >= DOLBY_VISION_LL_YUV422) {
					graphic_max =
						dv_target_graphics_LL_max_26
						[tmp_fmt][dst_format];
				} else {
					graphic_max =
						dv_target_graphics_max_26
						[tmp_fmt][dst_format];
				}
			}
			/*NTS HDR-001-TC3 is conflict with SDK test*/
			if (dv_graphic_blend_test && (dst_format == FORMAT_HDR10 ||
				src_format == FORMAT_HDR10))
				graphic_max = dv_HDR10_graphics_max;

		}
	} else {
		if (amdv_graphic_max != 0) {
			graphic_max = amdv_graphic_max;
		} else {
			if (src_format >= 0 && src_format <
			    ARRAY_SIZE(dv_target_graphics_max))
				tmp_fmt = src_format;
			if ((dolby_vision_flags & FLAG_FORCE_DV_LL) ||
			    dolby_vision_ll_policy >= DOLBY_VISION_LL_YUV422) {
				graphic_max =
					dv_target_graphics_LL_max
					[tmp_fmt][dst_format];
			} else {
				graphic_max =
					dv_target_graphics_max
					[tmp_fmt][dst_format];
			}
			if (dv_graphic_blend_test && dst_format == FORMAT_HDR10)
				graphic_max = dv_HDR10_graphics_max;
		}
	}

	if (dolby_vision_flags & FLAG_USE_SINK_MIN_MAX) {
		if (vinfo->vout_device->dv_info->ieeeoui == 0x00d046) {
			if (vinfo->vout_device->dv_info->ver == 0) {
				/* need lookup PQ table ... */
			} else if (vinfo->vout_device->dv_info->ver == 1) {
				if (vinfo->vout_device->dv_info->tmax_lum) {
					/* Target max luminance = 100+50*CV */
					graphic_max =
					target_lumin_max =
					(vinfo->vout_device->dv_info->tmax_lum * 50 + 100);
					/* Target min luminance = (CV/127)^2 */
					graphic_min =
					amdv_target_min =
					(vinfo->vout_device->dv_info->tmin_lum ^ 2) *
					10000 / (127 * 127);
				}
			}
		} else if (sink_hdr_support(vinfo) & HDR_SUPPORT) {
			if (vinfo->hdr_info.lumi_max) {
				/* Luminance value = 50 * (2 ^ (CV/32)) */
				graphic_max =
				target_lumin_max = 50 *
					(2 ^ (vinfo->hdr_info.lumi_max >> 5));
				/* Desired Content Min Luminance =*/
				/*	Desired Content Max Luminance*/
				/*	* (CV/255) * (CV/255) / 100	*/
				graphic_min =
				amdv_target_min =
					target_lumin_max * 10000
					* vinfo->hdr_info.lumi_min
					* vinfo->hdr_info.lumi_min
					/ (255 * 255 * 100);
			}
		}
		if (target_lumin_max) {
			amdv_target_max[0][0] =
			amdv_target_max[0][1] =
			amdv_target_max[1][0] =
			amdv_target_max[1][1] =
			amdv_target_max[2][0] =
			amdv_target_max[2][1] =
				target_lumin_max;
		} else {
			memcpy(amdv_target_max,
			       amdv_default_max,
			       sizeof(amdv_target_max));
		}
	}

	if (is_osd_off && !multi_dv_mode) {
		graphic_min = 0;
		graphic_max = 0;
	}
	if (!vsvdb_config_set_flag) {
		memset(&new_m_dovi_setting.vsvdb_tbl[0],
			0, sizeof(new_m_dovi_setting.vsvdb_tbl));
		new_m_dovi_setting.vsvdb_len = 0;
		new_m_dovi_setting.vsvdb_changed = 1;
		vsvdb_config_set_flag = true;
	}
	if ((dolby_vision_flags & FLAG_DISABLE_LOAD_VSVDB) == 0) {
		/* check if vsvdb is changed */
		if (vinfo &&  vinfo->vout_device &&
		    vinfo->vout_device->dv_info &&
		    vinfo->vout_device->dv_info->ieeeoui == 0x00d046 &&
		    vinfo->vout_device->dv_info->block_flag == CORRECT) {
			if (new_m_dovi_setting.vsvdb_len
				!= vinfo->vout_device->dv_info->length + 1)
				new_m_dovi_setting.vsvdb_changed = 1;
			else if (memcmp(&new_m_dovi_setting.vsvdb_tbl[0],
				&vinfo->vout_device->dv_info->rawdata[0],
				vinfo->vout_device->dv_info->length + 1))
				new_m_dovi_setting.vsvdb_changed = 1;
			memset(&new_m_dovi_setting.vsvdb_tbl[0],
				0, sizeof(new_m_dovi_setting.vsvdb_tbl));
			memcpy(&new_m_dovi_setting.vsvdb_tbl[0],
				&vinfo->vout_device->dv_info->rawdata[0],
				vinfo->vout_device->dv_info->length + 1);
			new_m_dovi_setting.vsvdb_len =
				vinfo->vout_device->dv_info->length + 1;
			if (new_m_dovi_setting.vsvdb_changed &&
			    new_m_dovi_setting.vsvdb_len) {
				int k = 0;

				pr_dv_dbg("new vsvdb[%d]:\n",
						new_m_dovi_setting.vsvdb_len);
				pr_dv_dbg
					("---%02x, %02x, %02x, %02x, %02x, %02x, %02x, %02x\n",
					 new_m_dovi_setting.vsvdb_tbl[k + 0],
					 new_m_dovi_setting.vsvdb_tbl[k + 1],
					 new_m_dovi_setting.vsvdb_tbl[k + 2],
					 new_m_dovi_setting.vsvdb_tbl[k + 3],
					 new_m_dovi_setting.vsvdb_tbl[k + 4],
					 new_m_dovi_setting.vsvdb_tbl[k + 5],
					 new_m_dovi_setting.vsvdb_tbl[k + 6],
					 new_m_dovi_setting.vsvdb_tbl[k + 7]);
				k += 8;
				pr_dv_dbg
					("---%02x, %02x, %02x, %02x, %02x, %02x, %02x, %02x\n",
					 new_m_dovi_setting.vsvdb_tbl[k + 0],
					 new_m_dovi_setting.vsvdb_tbl[k + 1],
					 new_m_dovi_setting.vsvdb_tbl[k + 2],
					 new_m_dovi_setting.vsvdb_tbl[k + 3],
					 new_m_dovi_setting.vsvdb_tbl[k + 4],
					 new_m_dovi_setting.vsvdb_tbl[k + 5],
					 new_m_dovi_setting.vsvdb_tbl[k + 6],
					 new_m_dovi_setting.vsvdb_tbl[k + 7]);
				k += 8;
				pr_dv_dbg
					("---%02x, %02x, %02x, %02x, %02x, %02x, %02x, %02x\n",
					 new_m_dovi_setting.vsvdb_tbl[k + 0],
					 new_m_dovi_setting.vsvdb_tbl[k + 1],
					 new_m_dovi_setting.vsvdb_tbl[k + 2],
					 new_m_dovi_setting.vsvdb_tbl[k + 3],
					 new_m_dovi_setting.vsvdb_tbl[k + 4],
					 new_m_dovi_setting.vsvdb_tbl[k + 5],
					 new_m_dovi_setting.vsvdb_tbl[k + 6],
					 new_m_dovi_setting.vsvdb_tbl[k + 7]);
				k += 8;
				pr_dv_dbg
					("---%02x, %02x, %02x, %02x, %02x, %02x, %02x, %02x\n",
					 new_m_dovi_setting.vsvdb_tbl[k + 0],
					 new_m_dovi_setting.vsvdb_tbl[k + 1],
					 new_m_dovi_setting.vsvdb_tbl[k + 2],
					 new_m_dovi_setting.vsvdb_tbl[k + 3],
					 new_m_dovi_setting.vsvdb_tbl[k + 4],
					 new_m_dovi_setting.vsvdb_tbl[k + 5],
					 new_m_dovi_setting.vsvdb_tbl[k + 6],
					 new_m_dovi_setting.vsvdb_tbl[k + 7]);
			}
		} else {
			if (new_m_dovi_setting.vsvdb_len)
				new_m_dovi_setting.vsvdb_changed = 1;
			memset(&new_m_dovi_setting.vsvdb_tbl[0],
				0, sizeof(new_m_dovi_setting.vsvdb_tbl));
			new_m_dovi_setting.vsvdb_len = 0;
		}
	}

	if (vf && dv_inst_valid(dv_id)) {
		dv_inst[dv_id].in_md =
			dv_inst[dv_id].md_buf[dv_inst[dv_id].current_id];
		dv_inst[dv_id].in_comp =
			dv_inst[dv_id].comp_buf[dv_inst[dv_id].current_id];
		if (src_format == FORMAT_DOVI) {
			dv_inst[dv_id].in_md_size = total_md_size;
			dv_inst[dv_id].in_comp_size = total_comp_size;
		} else {
			dv_inst[dv_id].in_md_size = 0;
			dv_inst[dv_id].in_comp_size = 0;
		}

		dv_inst[dv_id].src_format = src_format;
		dv_inst[dv_id].set_chroma_format = CP_P420;
		dv_inst[dv_id].set_yuv_range = SIGNAL_RANGE_SMPTE;
		dv_inst[dv_id].color_format = CP_YUV;
		/*set_bit_depth is used to calc Yuv2Rgb  for hdr/hlg/sdr case */
		dv_inst[dv_id].set_bit_depth = 12;/*fixed 12bit, no use src_bdp*/

		if (input_mode == IN_MODE_HDMI) {
			if (src_format == FORMAT_DOVI_LL) {
				/*color_format and set_chroma_format are only*/
				/*used for Dovi low latency or non DoVi input.*/
				dv_inst[dv_id].color_format = CP_YUV; //rgb case?
				dv_inst[dv_id].set_chroma_format = CP_UYVY;
			}
			dv_inst[dv_id].vsem_if_size = vsem_if_size;
			dv_inst[dv_id].vsem_if = vsem_if_buf;
		}
		dv_inst[dv_id].input_mode = input_mode;
		dv_inst[dv_id].el_flag = el_flag;
		dv_inst[dv_id].el_halfsize_flag = el_halfsize_flag;
		dv_inst[dv_id].video_width = w;
		dv_inst[dv_id].video_height = h;
	}

	/* cert: some graphic test also need video pri 5223,5243,5253,5263 */
	if (dolby_vision_flags & FLAG_CERTIFICATION) {
		if ((dolby_vision_flags & FLAG_PRIORITY_GRAPHIC))
			pri_mode = G_PRIORITY;
		else
			pri_mode = V_PRIORITY;
	} else {
		/*auto mode: check video/graphics priority on the fly */
		if (get_video_enabled() && is_graphics_output_off())
			pri_mode = V_PRIORITY;
		else
			pri_mode = G_PRIORITY;
		/*user debug mode*/
		if (force_priority == 1)
			pri_mode = G_PRIORITY;
		else if (force_priority == 2)
			pri_mode = V_PRIORITY;

		/*video priority only valid in sink-led,set to graphic pri when in other mode*/
		if (dst_format != FORMAT_DOVI ||
		    (dst_format == FORMAT_DOVI &&
		    (dolby_vision_ll_policy >= DOLBY_VISION_LL_YUV422 ||
		    (dolby_vision_flags & FLAG_FORCE_DV_LL))))
			pri_mode = G_PRIORITY;
	}

	if (dst_format == FORMAT_DOVI) {
		cur_use_ll_flag = new_m_dovi_setting.use_ll_flag;
		cur_ll_rgb_desired = new_m_dovi_setting.ll_rgb_desired;
		if ((dolby_vision_flags & FLAG_FORCE_DV_LL) ||
		    dolby_vision_ll_policy >= DOLBY_VISION_LL_YUV422)
			new_m_dovi_setting.use_ll_flag = 1;
		else
			new_m_dovi_setting.use_ll_flag = 0;
		if (dolby_vision_ll_policy == DOLBY_VISION_LL_RGB444 ||
		    (dolby_vision_flags & FLAG_FORCE_RGB_OUTPUT))
			new_m_dovi_setting.ll_rgb_desired = 1;
		else
			new_m_dovi_setting.ll_rgb_desired = 0;

		if (new_m_dovi_setting.use_ll_flag != cur_use_ll_flag ||
		    new_m_dovi_setting.ll_rgb_desired != cur_ll_rgb_desired) {
			pr_dv_dbg
				("[inst%d]reset cp,use_ll_flag:%d-%d,ll_rgb_desired:%d-%d\n",
				dv_id + 1,
				cur_use_ll_flag, new_m_dovi_setting.use_ll_flag,
				cur_ll_rgb_desired, new_m_dovi_setting.ll_rgb_desired);
			p_funcs_stb->multi_control_path(&invalid_m_dovi_setting);
		}
	} else {
		new_m_dovi_setting.use_ll_flag = 0;
		new_m_dovi_setting.ll_rgb_desired = 0;
	}
	if (dst_format == FORMAT_HDR10 &&
		(dolby_vision_flags & FLAG_DOVI2HDR10_NOMAPPING))
		new_m_dovi_setting.dovi2hdr10_nomapping = 1;
	else
		new_m_dovi_setting.dovi2hdr10_nomapping = 0;

	/*update diagnostic_enable, diagnostic_mux_select,dovi_ll_enable from control_path*/
	/*sometimes set use_ll_flag but vsvdb not support ll*/
	new_m_dovi_setting.diagnostic_enable = 0;
	new_m_dovi_setting.diagnostic_mux_select = 0;
	new_m_dovi_setting.dovi_ll_enable = 0;
	if (vinfo) {
		new_m_dovi_setting.vout_width = vinfo->width;
		new_m_dovi_setting.vout_height = vinfo->height;
	} else {
		new_m_dovi_setting.vout_width = 0;
		new_m_dovi_setting.vout_height = 0;
	}
	memset(&new_m_dovi_setting.ext_md, 0, sizeof(struct ext_md_s));
	new_m_dovi_setting.input[IPCORE2_ID].input_mode = IN_MODE_GRAPHICS;
	new_m_dovi_setting.input[IPCORE2_ID].src_format = graphic_fmt;
	new_m_dovi_setting.input[IPCORE2_ID].set_bit_depth = 8;
	new_m_dovi_setting.input[IPCORE2_ID].set_chroma_format = CP_I444;
	new_m_dovi_setting.input[IPCORE2_ID].set_yuv_range = SIGNAL_RANGE_FULL;
	new_m_dovi_setting.input[IPCORE2_ID].in_comp = NULL;
	new_m_dovi_setting.input[IPCORE2_ID].in_comp_size = 0;
	new_m_dovi_setting.input[IPCORE2_ID].color_format = CP_RGB;
	if (graphic_fmt == FORMAT_DOVI) {
		new_m_dovi_setting.input[IPCORE2_ID].in_md = graphic_md_buf;
		new_m_dovi_setting.input[IPCORE2_ID].in_md_size = graphic_md_size;
	} else {
		new_m_dovi_setting.input[IPCORE2_ID].in_md = NULL;
		new_m_dovi_setting.input[IPCORE2_ID].in_md_size = 0;
	}

	new_m_dovi_setting.set_graphic_min_lum = graphic_min;
	new_m_dovi_setting.set_graphic_max_lum = graphic_max * 10000;
	new_m_dovi_setting.set_target_min_lum = amdv_target_min;
	if (vd_path == VD1_PATH || !vf) {//only update for vd1
		if (src_format < 0  || src_format >= ARRAY_SIZE(amdv_target_max))
			new_m_dovi_setting.set_target_max_lum =
			amdv_target_max[FORMAT_SDR][dst_format] * 10000;
		else
			new_m_dovi_setting.set_target_max_lum =
			amdv_target_max[src_format][dst_format] * 10000;
	}
	new_m_dovi_setting.input[IPCORE2_ID].video_width = dv_cert_graphic_width;
	new_m_dovi_setting.input[IPCORE2_ID].video_height = dv_cert_graphic_height;
	dv_core1[vd_path].amdv_setting_video_flag = video_frame;
	g_dst_format = dst_format;

	if (debug_dolby & 0x1000)
		pr_dv_dbg("[inst%d]parse_metadata done, video %d_%d, %d\n",
			     dv_id + 1, w, h, video_frame);

	return 0;
}

/*vf: display on vd1, vf_2: dislpay on vd2*/
/* ret 0: setting generated for this frame */
/* ret -1: do nothing */
int amdv_control_path(struct vframe_s *vf, struct vframe_s *vf_2)
{
	unsigned long time_use = 0;
	struct timeval start;
	struct timeval end;
	int flag;
	int ret = -1;
	int id = 0;
	int inst_id_1 = 0;
	int inst_id_2 = 0;
	static int last_inst_id_1 = -1;
	static int last_inst_id_2 = -1;
	int i;
	enum signal_format_enum dst_format = g_dst_format;
	enum signal_format_enum src_format;
	int valid_video_num = 0;
	static int last_valid_video_num;
	static int last_pri_input;
	bool video_num_change = false;
	bool pri_change = false;
	bool res_change = false;
	bool input_inst_change = false;

	enum signal_format_enum cur_src_format;
	enum signal_format_enum cur_dst_format;
	enum input_mode_enum input_mode = IN_MODE_OTT;
	enum input_mode_enum cur_input_mode = IN_MODE_OTT;
	struct vframe_s *p_vf;

	if (!dolby_vision_enable || !module_installed || !p_funcs_stb)
		return -1;

	for (i = 0; i < NUM_IPCORE1; i++) {
		new_m_dovi_setting.input[i].valid = 0;
		new_m_dovi_setting.input[i].in_md_size = 0;
		new_m_dovi_setting.input[i].in_comp_size = 0;
	}
	for (i = 0; i < NUM_INST; i++)
		dv_inst[i].valid = 0;
	/*update new_m_dovi_setting.input, choose the two inst that display on vd1/vd2*/
	for (i = 0; i < NUM_IPCORE1; i++) {
		if (i == 0)	{
			if (!vf)
				continue;
			p_vf = vf;
			id = vf->src_fmt.dv_id;
			inst_id_1 = vf->src_fmt.dv_id;
			if (id != vd1_inst_id)
				pr_dv_dbg("check id1 %d %d\n", id, vd1_inst_id);
		}
		if (i == 1)	{
			if (!vf_2 || (!support_multi_core1() && !force_two_valid))
				break;
			p_vf = vf_2;
			id = vf_2->src_fmt.dv_id;
			inst_id_2 = vf_2->src_fmt.dv_id;
			if (id != vd2_inst_id)
				pr_dv_dbg("check id2 %d %d\n", id, vd2_inst_id);
		}
		new_m_dovi_setting.input[i].valid = 1;
		++valid_video_num;
		if (dv_inst_valid(id)) {
			dv_inst[id].valid = 1;
			input_mode = dv_inst[id].input_mode;
			src_format = dv_inst[id].src_format;

			if (new_m_dovi_setting.input[i].video_width &&
			    new_m_dovi_setting.input[i].video_height) {
				/* Toggle multiple frames in one vsync case: */
				/* new_dovi_setting.video_width will be available,*/
				/* but not be applied. So use new_dovi_setting as */
				/* reference instead of dovi_setting. */
				/* To avoid unnecessary reset control_path. */
				cur_dst_format = new_m_dovi_setting.dst_format;
				cur_src_format = new_m_dovi_setting.input[i].src_format;
				cur_input_mode = new_m_dovi_setting.input[i].input_mode;
			} else {
				cur_dst_format = m_dovi_setting.dst_format;
				cur_src_format = m_dovi_setting.input[i].src_format;
				cur_input_mode = m_dovi_setting.input[i].input_mode;
			}

			if (new_m_dovi_setting.input[i].video_width != dv_inst[id].video_width ||
			    new_m_dovi_setting.input[i].video_height != dv_inst[id].video_height) {
				res_change = true;
				pr_dv_dbg("[inst%d vd%d]res changed %dx%d=> %dx%d\n",
					  id + 1, i + 1,
					  new_m_dovi_setting.input[i].video_width,
					  new_m_dovi_setting.input[i].video_height,
					  dv_inst[id].video_width, dv_inst[id].video_height);
			}
			if (i == 0 && inst_id_1 != last_inst_id_1) {
				input_inst_change = true;
				pr_dv_dbg("[inst%d vd%d]input inst %d=>%d\n",
					  id + 1, i + 1,
					  last_inst_id_1,
					  inst_id_1);
				last_inst_id_1 = inst_id_1;
			}
			if (i == 1 && inst_id_2 != last_inst_id_2) {
				input_inst_change = true;
				pr_dv_dbg("[inst%d vd%d]input inst %d=>%d\n",
					  id + 1, i + 1,
					  last_inst_id_2,
					  inst_id_2);
				last_inst_id_2 = inst_id_2;
			}
			if (src_format != cur_src_format ||
			    dst_format != cur_dst_format ||
			    input_mode != cur_input_mode ||
			    res_change ||
			    input_inst_change) {
				pr_dv_dbg
				("[inst%d]reset cp,src:%d-%d,dst:%d-%d,m:%d-%d,c:%d,f:%x\n",
				id + 1,
				cur_src_format, src_format,
				cur_dst_format, dst_format,
				cur_input_mode, input_mode,
				dv_inst[id].frame_count, dolby_vision_flags);
				p_funcs_stb->multi_control_path(&invalid_m_dovi_setting);
			}
			new_m_dovi_setting.input[i].input_mode = dv_inst[id].input_mode;
			new_m_dovi_setting.input[i].video_width = dv_inst[id].video_width;
			new_m_dovi_setting.input[i].video_height = dv_inst[id].video_height;
			new_m_dovi_setting.input[i].in_md =
				dv_inst[id].in_md;
			new_m_dovi_setting.input[i].in_comp =
				dv_inst[id].in_comp;

			if (src_format == FORMAT_DOVI) {
				new_m_dovi_setting.input[i].in_md_size = dv_inst[id].in_md_size;
				new_m_dovi_setting.input[i].in_comp_size = dv_inst[id].in_comp_size;
			} else {
				new_m_dovi_setting.input[i].in_md_size = 0;
				new_m_dovi_setting.input[i].in_comp_size = 0;
			}
			new_m_dovi_setting.input[i].src_format = dv_inst[id].src_format;
			new_m_dovi_setting.input[i].p_hdr10_param = &dv_inst[id].hdr10_param;
			new_m_dovi_setting.input[i].set_chroma_format =
				dv_inst[id].set_chroma_format;
			new_m_dovi_setting.input[i].set_yuv_range = dv_inst[id].set_yuv_range;
			new_m_dovi_setting.input[i].color_format = dv_inst[id].color_format;
			/*set_bit_depth is used to calc Yuv2Rgb  for hdr/hlg/sdr case */
			/*fixed 12bit, no use src_bdp*/
			new_m_dovi_setting.input[i].set_bit_depth = dv_inst[id].set_bit_depth;

			if (input_mode == IN_MODE_HDMI) {
				if (src_format == FORMAT_DOVI_LL) {
					/*color_format and set_chroma_format are only*/
					/*used for Dovi low latency or non DoVi input.*/
					new_m_dovi_setting.input[i].color_format =
						dv_inst[id].color_format;
					new_m_dovi_setting.input[i].set_chroma_format =
						dv_inst[id].set_chroma_format;
				}
				new_m_dovi_setting.input[i].vsem_if_size = dv_inst[id].vsem_if_size;
				new_m_dovi_setting.input[i].vsem_if = dv_inst[id].vsem_if;
			}
			new_m_dovi_setting.input[i].input_mode = input_mode;
			new_m_dovi_setting.input[i].el_flag = dv_inst[id].el_flag;
			new_m_dovi_setting.input[i].el_halfsize_flag = dv_inst[id].el_halfsize_flag;
		}
	}
	new_m_dovi_setting.dst_format = dst_format;
	new_m_dovi_setting.enable_debug = debug_ko;
	new_m_dovi_setting.enable_multi_core1 = enable_multi_core1;
	new_m_dovi_setting.pri_input = pri_input;

	/*update L11 info for hdmi in allm and local allm*/
	if (hdmi_in_allm || local_allm) {
		new_m_dovi_setting.reserved[0] = 1;/*user l11*/
		new_m_dovi_setting.reserved[1] = 2;/*user content type*/
		new_m_dovi_setting.reserved[2] = 8;/*user white point*/
		new_m_dovi_setting.reserved[3] = 0;/*byte2*/
		new_m_dovi_setting.reserved[4] = 0;/*byte3*/
	} else {
		new_m_dovi_setting.reserved[0] = 0;/*user l11*/
		new_m_dovi_setting.reserved[1] = 0;/*user content type*/
		new_m_dovi_setting.reserved[2] = 0;/*user white point*/
		new_m_dovi_setting.reserved[3] = 0;/*byte2*/
		new_m_dovi_setting.reserved[4] = 0;/*byte3*/
	}

	if (new_m_dovi_setting.set_priority != pri_mode) {
		pr_dv_dbg("pri_mode changed %d=>%d\n", new_m_dovi_setting.set_priority, pri_mode);
		p_funcs_stb->multi_control_path(&invalid_m_dovi_setting);
		new_m_dovi_setting.set_priority = pri_mode;
	}

	if (enable_multi_core1) {
		cur_valid_video_num = valid_video_num;
		if (last_pri_input != pri_input) {
			pri_change = true;
			pr_dv_dbg("pri change changed %d->%d, reset cp\n",
				  last_pri_input, pri_input);
			last_pri_input = pri_input;
		}
		if (last_valid_video_num != valid_video_num) {
			video_num_change = true;
			pr_dv_dbg("valid videonum changed %d->%d, reset cp\n",
				  last_valid_video_num, valid_video_num);
			last_valid_video_num = valid_video_num;
		}
		if (video_num_change || pri_change)
			p_funcs_stb->multi_control_path(&invalid_m_dovi_setting);
	} else {/*only choose primary video to dv*/
		new_m_dovi_setting.input[0].valid = 1;
		new_m_dovi_setting.input[1].valid = 0;
	}
	new_m_dovi_setting.input[IPCORE2_ID].valid = 1;

	if (debug_dolby & 0x400)
		do_gettimeofday(&start);

	if (is_aml_stb_hdmimode() && dv_inst_valid(hdmi_inst_id) &&
		hdmi_path_id <= NUM_IPCORE1) {
		/*new_m_dovi_setting.set_priority = V_PRIORITY;*/
		flag = p_funcs_stb->multi_control_path(&new_m_dovi_setting);
		/* overwrite core3 meta by hdmi sink-led input */
		if (0 && dst_format == FORMAT_DOVI &&
		    new_m_dovi_setting.input[hdmi_path_id].src_format == FORMAT_DOVI &&
		    dolby_vision_ll_policy == DOLBY_VISION_LL_DISABLE)
			prepare_dv_meta
				(&new_m_dovi_setting.md_reg3,
				 dv_inst[hdmi_inst_id].md_buf[dv_inst[inst_id_1].current_id],
				 new_m_dovi_setting.input[hdmi_path_id].in_md_size);
	} else {
		flag = p_funcs_stb->multi_control_path(&new_m_dovi_setting);
	}
	if (debug_dolby & 0x400) {
		do_gettimeofday(&end);
		time_use = (end.tv_sec - start.tv_sec) * 1000000 +
				(end.tv_usec - start.tv_usec);
		pr_info("stb controlpath time: %5ld us\n", time_use);
	}
	if (debug_dolby & 1) {
		if (is_video_output_off(vf)) {
			if (vf && vf_2) {
				pr_dv_dbg("setting %d/%d->%d(T:%d-%d), osd:%d x %d\n",
				new_m_dovi_setting.input[0].src_format,
				new_m_dovi_setting.input[1].src_format,
				dst_format,
				new_m_dovi_setting.set_target_min_lum,
				new_m_dovi_setting.set_target_max_lum,
				osd_graphic_width[OSD1_INDEX],
				osd_graphic_height[OSD1_INDEX]);
			}
		}
		if (vf)
			pr_dv_dbg
			("[inst%d]video:%dx%d,fmt:%d->%d,md:%d,comp:%d,fr:%d,allm %d %d\n",
			 inst_id_1 + 1,
			 new_m_dovi_setting.input[0].video_width,
			 new_m_dovi_setting.input[0].video_height,
			 new_m_dovi_setting.input[0].src_format,
			 dst_format,
			 new_m_dovi_setting.input[0].in_md_size,
			 new_m_dovi_setting.input[0].in_comp_size,
			 dv_inst[inst_id_1].frame_count,
			 hdmi_in_allm,
			 local_allm);
		if (vf_2)
			pr_dv_dbg
			("[inst%d]video:%dx%d,fmt:%d->%d,md:%d,comp:%d,fr:%d\n",
			 inst_id_2 + 1,
			 new_m_dovi_setting.input[1].video_width,
			 new_m_dovi_setting.input[1].video_height,
			 new_m_dovi_setting.input[1].src_format,
			 dst_format,
			 new_m_dovi_setting.input[1].in_md_size,
			 new_m_dovi_setting.input[1].in_comp_size,
			 dv_inst[inst_id_2].frame_count);
		pr_dv_dbg
		("osd %d,osd size:%dx%d,g min-max %d-%d,t min-max:%d-%d,pri:%s,flag=%x\n",
		 !is_graphics_output_off(),
		 osd_graphic_width[OSD1_INDEX],
		 osd_graphic_height[OSD1_INDEX],
		 new_m_dovi_setting.set_graphic_min_lum,
		 new_m_dovi_setting.set_graphic_max_lum,
		 new_m_dovi_setting.set_target_min_lum,
		 new_m_dovi_setting.set_target_max_lum,
		 new_m_dovi_setting.set_priority == V_PRIORITY ?
		 "vpr" : "gpr",
		 flag);
	}

	if (flag >= 0) {
		update_stb_core_setting_flag(flag);
		if (debug_dolby & 2)
			pr_dv_dbg
				("ll_enable=%d,diagnostic=%d,ll_policy=%d\n",
				 new_m_dovi_setting.dovi_ll_enable,
				 new_m_dovi_setting.diagnostic_enable,
				 dolby_vision_ll_policy);
		dump_m_setting(&new_m_dovi_setting, dv_inst[pri_input].frame_count,
					   debug_dolby, inst_id_1);
		ret = 0; /* setting updated */
	}

	if (flag < 0) {
		ret = -1;
		if (vf) {
			new_m_dovi_setting.input[0].video_width = 0;
			new_m_dovi_setting.input[0].video_height = 0;
		} else if (vf_2) {
			new_m_dovi_setting.input[1].video_width = 0;
			new_m_dovi_setting.input[1].video_height = 0;
		}
		pr_dv_error("control_path(%d/%d, %d) failed %d\n",
			       new_m_dovi_setting.input[0].src_format,
			       new_m_dovi_setting.input[1].src_format,
			       dst_format, flag);
	}
	return ret;
}

/* toggle mode: 0: not toggle; 1: toggle frame; 2: use keep frame */
/* ret 0: setting generated for this frame for v1 or parser done for v2*/
/* ret 1: both dolby and hdr module bypass */
/* ret 2: no generating setting for this frame */
/* ret -1: no parse or generating setting */
int amdv_parse_metadata(struct vframe_s *vf,
				enum vd_path_e vd_path,
				u8 toggle_mode,
				bool bypass_release,
				bool drop_flag)
{
	int ret = -1;

	if (!dolby_vision_enable || !module_installed)
		return -1;

	if (multi_dv_mode) {
		if (is_aml_tvmode()) {
			ret = amdv_parse_metadata_v1(vf, toggle_mode,
								bypass_release,
								drop_flag);
		} else {
			ret = amdv_parse_metadata_v2_stb(vf, vd_path,
								 toggle_mode,
								 bypass_release,
								 drop_flag);
			if (debug_dolby & 0x2000)
				pr_dv_dbg("parse_metadata return %d\n", ret);

			if (ret == 0)
				update_control_path_flag = true;
			else
				update_control_path_flag = false;
		}
	} else {
		ret = amdv_parse_metadata_v1(vf, toggle_mode,
						     bypass_release, drop_flag);
	}

	return ret;
}
EXPORT_SYMBOL(amdv_parse_metadata);

/*dual_layer && parse_ret_flags   =1 =>mel*/
/*dual_layer && parse_ret_flags !=1 =>fel*/
bool vf_is_fel(struct vframe_s *vf)
{
	enum vframe_signal_fmt_e fmt;
	bool fel = false;

	if (!vf)
		return false;

	fmt = get_vframe_src_fmt(vf);

	if (fmt == VFRAME_SIGNAL_FMT_DOVI) {
		if (debug_dolby & 0x1)
			pr_dv_dbg("dual layer %d, parse ret flags %d\n",
				     vf->src_fmt.dual_layer,
				     vf->src_fmt.parse_ret_flags);
		if (vf->src_fmt.dual_layer && vf->src_fmt.parse_ret_flags != 1)
			fel = true;
	}
	return fel;
}

int amdv_wait_metadata_v1(struct vframe_s *vf)
{
	struct vframe_s *el_vf;
	int ret = 0;
	unsigned int mode = dolby_vision_mode;
	enum signal_format_enum check_format;
	const struct vinfo_s *vinfo = get_current_vinfo();
	bool vd1_on = false;

	if (!dolby_vision_enable || !module_installed)
		return 0;

	if (single_step_enable) {
		if (dolby_vision_flags & FLAG_SINGLE_STEP)
			/* wait fake el for "step" */
			return 1;

		dolby_vision_flags |= FLAG_SINGLE_STEP;
	}

	if (dolby_vision_flags & FLAG_CERTIFICATION) {
		bool ott_mode = true;

		if (is_aml_tvmode())
			ott_mode = tv_dovi_setting->input_mode !=
				IN_MODE_HDMI;
		if (debug_dolby & 0x1000)
			pr_dv_dbg("setting_update_count %d, crc_count %d, flag %x\n",
				setting_update_count, crc_count, dolby_vision_flags);
		if (setting_update_count > crc_count &&
			!(dolby_vision_flags & FLAG_DISABLE_CRC)) {
			if (ott_mode)
				return 1;
		}
	}

	if (is_dovi_dual_layer_frame(vf)) {
		el_vf = dvel_vf_peek();
		while (el_vf) {
			if (debug_dolby & 2)
				pr_dv_dbg("=== peek bl(%p-%lld) with el(%p-%lld) ===\n",
					     vf, vf->pts_us64,
					     el_vf, el_vf->pts_us64);
			if (el_vf->pts_us64 == vf->pts_us64 ||
			    !(dolby_vision_flags & FLAG_CHECK_ES_PTS)) {
				/* found el */
				ret = 3;
				break;
			} else if (el_vf->pts_us64 < vf->pts_us64) {
				if (debug_dolby & 2)
					pr_dv_dbg("bl(%p-%lld) => skip el pts(%p-%lld)\n",
						     vf, vf->pts_us64,
						     el_vf, el_vf->pts_us64);
				el_vf = dvel_vf_get();
				dvel_vf_put(el_vf);
				vf_notify_provider(DVEL_RECV_NAME,
					VFRAME_EVENT_RECEIVER_PUT, NULL);
				if (debug_dolby & 2)
					pr_dv_dbg("=== get & put el(%p-%lld) ===\n",
						el_vf, el_vf->pts_us64);

				/* skip old el and peek new */
				el_vf = dvel_vf_peek();
			} else {
				/* no el found */
				ret = 2;
				break;
			}
		}
		/* need wait el */
		if (!el_vf) {
			if (debug_dolby & 2)
				pr_dv_dbg("=== bl wait el(%p-%lld) ===\n",
					     vf, vf->pts_us64);
			ret = 1;
		}
	}
	if (ret == 1)
		return ret;

	if (!amdv_wait_init && !amdv_core1_on) {
		ret = is_amdv_frame(vf);
		if (ret) {
			/* STB hdmi LL input as HDR */
			if (vf->source_type == VFRAME_SOURCE_TYPE_HDMI &&
			    ret == 2 && hdmi_to_stb_policy &&
			    (is_aml_tm2_stbmode() || is_aml_t7_stbmode()) &&
				hdmi_source_led_as_hdr10)
				check_format = FORMAT_HDR10;
			else
				check_format = FORMAT_DOVI;
			ret = 0;
		} else if (is_primesl_frame(vf)) {
			check_format = FORMAT_PRIMESL;
		} else if (is_hdr10_frame(vf)) {
			check_format = FORMAT_HDR10;
		} else if (is_hlg_frame(vf)) {
			check_format = FORMAT_HLG;
		} else if (is_hdr10plus_frame(vf)) {
			check_format = FORMAT_HDR10PLUS;
		} else if (is_mvc_frame(vf)) {
			check_format = FORMAT_MVC;
		} else if (is_cuva_frame(vf)) {
			check_format = FORMAT_CUVA;
		} else {
			check_format = FORMAT_SDR;
		}

		if (vf)
			update_src_format(check_format, vf);

		if (amdv_policy_process(vf, &mode, check_format)) {
			if (mode != AMDV_OUTPUT_MODE_BYPASS &&
			    dolby_vision_mode ==
			    AMDV_OUTPUT_MODE_BYPASS) {
				amdv_wait_init = true;
				amdv_target_mode = mode;
				amdv_wait_on = true;

				/*dv off->on, delay vfream*/
				if (dolby_vision_policy ==
				    AMDV_FOLLOW_SOURCE &&
				    dolby_vision_mode ==
				    AMDV_OUTPUT_MODE_BYPASS &&
				    mode ==
				    AMDV_OUTPUT_MODE_IPT_TUNNEL &&
				    amdv_wait_delay > 0 &&
				    !vf_is_fel(vf)) {
					amdv_wait_count =
					amdv_wait_delay;
				} else {
					amdv_wait_count = 0;
				}

				pr_dv_dbg("dolby_vision_need_wait src=%d mode=%d\n",
					check_format, mode);
			}
		}
		/*chip after g12 not used bit VPP_MISC 9:11*/
		if (is_aml_gxm() || is_aml_txlx() || is_aml_g12()) {
			if (READ_VPP_DV_REG(VPP_MISC) & (1 << 10))
				vd1_on = true;
		} else if (is_aml_tm2() || is_aml_sc2() || is_aml_t7() ||
			   is_aml_t3() || is_aml_s4d() || is_aml_t5w()) {
			if (READ_VPP_DV_REG(VD1_BLEND_SRC_CTRL) & (1 << 0))
				vd1_on = true;
		} else if (is_aml_s5()) {
			if (READ_VPP_DV_REG(S5_VD1_BLEND_SRC_CTRL) & (0xf << 0))
				vd1_on = true;
		}
		/* don't use run mode when sdr -> dv and vd1 not disable */
		if (/*amdv_wait_init && */vd1_on && is_aml_tvmode() && !force_runmode)
			amdv_on_count =
				amdv_run_mode_delay + 1;
		if (debug_dolby & 8)
			pr_dv_dbg("amdv_on_count %d, vd1_on %d\n",
				      amdv_on_count, vd1_on);
	}

	if (amdv_wait_init && amdv_wait_count > 0) {
		if (debug_dolby & 8)
			pr_dv_dbg("delay wait %d\n",
				amdv_wait_count);

		if (!get_disable_video_flag(VD1_PATH)) {
			/*update only after app enable video display,*/
			/* to distinguish play start and netflix exit*/
			send_hdmi_pkt_ahead(FORMAT_DOVI, vinfo);
			amdv_wait_count--;
		} else {
			/*exit netflix, still process vf after video disable,*/
			/*wait init will be on, need reset wait init */
			amdv_wait_init = false;
			amdv_wait_count = 0;
			if (debug_dolby & 8)
				pr_dv_dbg("clear amdv_wait_on\n");
		}
		ret = 1;
	} else if (amdv_core1_on &&
		(amdv_on_count <=
		amdv_run_mode_delay))
		ret = 1;

	if (vf && (debug_dolby & 8))
		pr_dv_dbg("wait return %d, vf %p(index %d), core1_on %d\n",
			      ret, vf, vf->omx_index, amdv_core1_on);

	return ret;
}

int amdv_wait_metadata_v2(struct vframe_s *vf, enum vd_path_e vd_path)
{
	int ret = 0;
	unsigned int mode = dolby_vision_mode;
	enum signal_format_enum check_format;
	const struct vinfo_s *vinfo = get_current_vinfo();
	int dv_id = 0;
	int layer_id = 0;
	bool vd_on = false;

	if (!dolby_vision_enable || !module_installed)
		return 0;

	if (vf) {
		dv_id = vf->src_fmt.dv_id;
		if (!dv_inst_valid(dv_id)) {
			pr_err("[%s]err inst %d\n", __func__, dv_id);
			dv_id = 0;
		}
	} else {
		return -1;
	}
	if (vd_path < NUM_IPCORE1)
		layer_id = vd_path;
	else
		pr_info("dv not support vd%d\n", vd_path + 1);

	if (debug_dolby & 0x2000)
		pr_dv_dbg("[inst%d]wait %p on vd%d,flags 0x%x,debug %x,%d %d\n",
			     dv_id + 1, vf, vd_path + 1,
			     dolby_vision_flags, debug_dolby,
			     setting_update_count, crc_count);

	if (single_step_enable_v2(dv_id, layer_id)) {
		if (vd_path == VD1_PATH) {
			if (dolby_vision_flags & FLAG_SINGLE_STEP)
				return 1;

			dolby_vision_flags |= FLAG_SINGLE_STEP;
		} else if (vd_path == VD2_PATH) {
			if (dolby_vision_flags & FLAG_SINGLE_STEP_PIP)
				return 1;

			dolby_vision_flags |= FLAG_SINGLE_STEP_PIP;
		}
	}

	if (dolby_vision_flags & FLAG_CERTIFICATION) {
		bool ott_mode = true;

		if (is_aml_tvmode())
			ott_mode = tv_dovi_setting->input_mode !=
				IN_MODE_HDMI;

		if (setting_update_count > crc_count &&
		    vd_path == VD1_PATH &&
		    !(dolby_vision_flags & FLAG_DISABLE_CRC)) {
			if (ott_mode) {
				if (debug_dolby & 8)
					pr_dv_dbg("[inst%d]need wait,vf %p(index %d),%d/%d\n",
						  dv_id + 1, vf, vf->omx_index,
						  setting_update_count, crc_count);
				return 1;
			}
		}
	}

	if (!dv_inst[dv_id].amdv_wait_init && !dv_core1[layer_id].core1_on) {
		ret = is_amdv_frame(vf);
		if (ret) {
			/* STB hdmi LL input as HDR */
			if (vf->source_type == VFRAME_SOURCE_TYPE_HDMI &&
			    ret == 2 && hdmi_to_stb_policy &&
			    (is_aml_tm2_stbmode() || is_aml_t7_stbmode()) &&
			    hdmi_source_led_as_hdr10)
				check_format = FORMAT_HDR10;
			else
				check_format = FORMAT_DOVI;
			ret = 0;
		} else if (is_primesl_frame(vf)) {
			check_format = FORMAT_PRIMESL;
		} else if (is_hdr10_frame(vf)) {
			if (is_dv_unique_drm(vf))
				check_format = FORMAT_DOVI_LL;
			else
				check_format = FORMAT_HDR10;
		} else if (is_hlg_frame(vf)) {
			check_format = FORMAT_HLG;
		} else if (is_hdr10plus_frame(vf)) {
			check_format = FORMAT_HDR10PLUS;
		} else if (is_mvc_frame(vf)) {
			check_format = FORMAT_MVC;
		} else if (is_cuva_frame(vf)) {
			check_format = FORMAT_CUVA;
		} else {
			check_format = FORMAT_SDR;
		}

		if (vf)
			update_src_format(check_format, vf);

		if (amdv_policy_process
			(vf, &mode, check_format)) {
			if (mode != AMDV_OUTPUT_MODE_BYPASS &&
			    dolby_vision_mode == AMDV_OUTPUT_MODE_BYPASS) {
				dv_inst[dv_id].amdv_wait_init = true;
				amdv_target_mode = mode;
				amdv_wait_on = true;

				/*dv off->on, delay vfream*/
				if (dolby_vision_policy ==
				    AMDV_FOLLOW_SOURCE &&
				    dolby_vision_mode ==
				    AMDV_OUTPUT_MODE_BYPASS &&
				    mode ==
				    AMDV_OUTPUT_MODE_IPT_TUNNEL &&
				    amdv_wait_delay > 0 &&
				    !vf_is_fel(vf)) {
					dv_inst[dv_id].amdv_wait_count =
					amdv_wait_delay;
				} else {
					dv_inst[dv_id].amdv_wait_count = 0;
				}
				pr_dv_dbg("dolby_vision_need_wait src=%d mode=%d\n",
					check_format, mode);
			}
		}
		/*chip after g12 not used bit VPP_MISC 9:11*/
		if (is_aml_gxm() || is_aml_txlx() || is_aml_g12()) {
			if (READ_VPP_DV_REG(VPP_MISC) & (1 << 10))
				vd_on = true;
		} else if (is_aml_tm2() || is_aml_sc2() || is_aml_t7() ||
		    is_aml_t3() || is_aml_s4d() || is_aml_t5w()) {
			if (vd_path == VD1_PATH) {
				if (READ_VPP_DV_REG(VD1_BLEND_SRC_CTRL) & (1 << 0))
					vd_on = true;
			} else if (vd_path == VD2_PATH) {
				if (READ_VPP_DV_REG(VD2_BLEND_SRC_CTRL) & (1 << 0))
					vd_on = true;
			}
		} else if (is_aml_s5()) {
			if (vd_path == VD1_PATH) {
				if (READ_VPP_DV_REG(S5_VD1_BLEND_SRC_CTRL) & (1 << 0))
					vd_on = true;
			} else if (vd_path == VD2_PATH) {
				if (READ_VPP_DV_REG(S5_VD2_BLEND_SRC_CTRL) & (1 << 0))
					vd_on = true;
			}
		}
		/* don't use run mode when sdr -> dv and vd1 not disable */
		if (/*dv_inst[dv_id].amdv_wait_init &&*/ vd_on &&
			is_aml_tvmode() && !force_runmode)
			dv_core1[layer_id].run_mode_count =
				amdv_run_mode_delay + 1;
		if (debug_dolby & 8)
			pr_dv_dbg("run_mode_count %d, vd1_on %d\n",
				     dv_core1[layer_id].run_mode_count, vd_on);
	}

	if (dv_inst[dv_id].amdv_wait_init &&
	    dv_inst[dv_id].amdv_wait_count) {
		if (debug_dolby & 8)
			pr_dv_dbg("delay wait %d\n",
				dv_inst[dv_id].amdv_wait_count);
		if (!get_disable_video_flag(VD1_PATH)) {
			/*update only after app enable video display,*/
			/* to distinguish play start and netflix exit*/
			send_hdmi_pkt_ahead(FORMAT_DOVI, vinfo);
			dv_inst[dv_id].amdv_wait_count--;
		} else {
			/*exit netflix, still process vf after video disable,*/
			/*wait init will be on, need reset wait init */
			dv_inst[dv_id].amdv_wait_init = false;
			dv_inst[dv_id].amdv_wait_count = 0;
			if (debug_dolby & 8)
				pr_dv_dbg("clear amdv_wait_on\n");
		}
		ret = 1;
	} else if (dv_core1[layer_id].core1_on &&
		   dv_core1[layer_id].run_mode_count <=
		   amdv_run_mode_delay)
		ret = 1;

	if (vf && (debug_dolby & 8))
		pr_dv_dbg("[inst%d]wait return %d, vf %p(index %d), core1_on %d\n",
			     dv_id + 1, ret, vf, vf->omx_index,
			     dv_core1[layer_id].core1_on);

	return ret;
}

/*-1: invalid*/
/* 0: no el; >0: with el */
/* 1: need wait el vf    */
/* 2: no match el found  */
/* 3: found match el     */
int amdv_wait_metadata(struct vframe_s *vf, enum vd_path_e vd_path)
{
	int ret = -1;

	if (multi_dv_mode)
		ret = amdv_wait_metadata_v2(vf, vd_path);
	else
		ret = amdv_wait_metadata_v1(vf);
	return ret;
}

int amdv_update_metadata(struct vframe_s *vf, enum vd_path_e vd_path, bool drop_flag)
{
	int ret = -1;
	int dv_id = 0;

	if (!dolby_vision_enable)
		return -1;

	if (vf && dv_inst_valid(vf->src_fmt.dv_id))
		dv_id = vf->src_fmt.dv_id;

	/*clear dv_vf before render first frame */
	if (!multi_dv_mode) {
		if (vf && vf->omx_index == 0 &&
		    vf->source_type == VFRAME_SOURCE_TYPE_OTHERS) {
			amdv_clear_buf(dv_id);
		}
	}

	if (vf && amdv_vf_check(vf)) {
		ret = amdv_parse_metadata
			(vf, vd_path, 1, false, drop_flag);

		if (multi_dv_mode) {
			(dv_inst[dv_id].frame_count)++;
			if (debug_dolby & 0x20000)
				pr_dv_dbg("update dv_inst[%d].frame_count %d\n",
				dv_id + 1, dv_inst[dv_id].frame_count);
		} else {
			frame_count++;
		}
	}

	return ret;
}
EXPORT_SYMBOL(amdv_update_metadata);

int amdv_update_src_format_v1(struct vframe_s *vf, u8 toggle_mode)
{
	unsigned int mode = dolby_vision_mode;
	enum signal_format_enum check_format;
	int ret = 0;

	if (!dolby_vision_enable || !vf)
		return -1;

	/* src_format is valid, need not re-init */
	if (amdv_src_format != 0)
		return 0;

	/* vf is not in the dv queue, new frame case */
	if (amdv_vf_check(vf))
		return 0;

	ret = is_amdv_frame(vf);
	if (ret) {
		/* STB hdmi LL input as HDR */
		if (vf->source_type == VFRAME_SOURCE_TYPE_HDMI &&
		    (is_aml_tm2_stbmode() || is_aml_t7_stbmode()) &&
		    hdmi_to_stb_policy && ret == 2)
			check_format = FORMAT_HDR10;
		else
			check_format = FORMAT_DOVI;
	} else if (is_primesl_frame(vf)) {
		check_format = FORMAT_PRIMESL;
	} else if (is_hdr10_frame(vf)) {
		check_format = FORMAT_HDR10;
	} else if (is_hlg_frame(vf)) {
		check_format = FORMAT_HLG;
	} else if (is_hdr10plus_frame(vf)) {
		check_format = FORMAT_HDR10PLUS;
	} else if (is_mvc_frame(vf)) {
		check_format = FORMAT_MVC;
	} else if (is_cuva_frame(vf)) {
		check_format = FORMAT_CUVA;
	} else {
		check_format = FORMAT_SDR;
	}
	if (vf)
		update_src_format(check_format, vf);

	if (!amdv_wait_init &&
	    !amdv_core1_on &&
	    amdv_src_format != 0) {
		if (amdv_policy_process
			(vf, &mode, check_format)) {
			if (mode != AMDV_OUTPUT_MODE_BYPASS &&
			    dolby_vision_mode ==
			    AMDV_OUTPUT_MODE_BYPASS) {
				amdv_wait_init = true;
				amdv_target_mode = mode;
				amdv_wait_on = true;
				pr_dv_dbg
					("dolby_vision_need_wait src=%d mode=%d\n",
					 check_format, mode);
			}
		}
		/* don't use run mode when sdr -> dv and vd1 not disable */
		if (amdv_wait_init &&
		    (READ_VPP_DV_REG(VPP_MISC) & (1 << 10)))
			amdv_on_count =
				amdv_run_mode_delay + 1;
	}
	pr_dv_dbg
		("%s done vf:%p, src=%d, toggle mode:%d\n",
		__func__, vf, amdv_src_format, toggle_mode);
	return 1;
}

/*to do, which inst*/
int amdv_update_src_format_v2(struct vframe_s *vf, u8 toggle_mode, enum vd_path_e vd_path)
{
	unsigned int mode = dolby_vision_mode;
	enum signal_format_enum check_format;
	int ret = 0;
	int dv_id = 0;
	int layer_id = 0;

	if (!dolby_vision_enable || !vf)
		return -1;

	if (vf)
		dv_id = vf->src_fmt.dv_id;

	if (!dv_inst_valid(dv_id)) {
		pr_err("[%s]err inst %d\n", __func__, dv_id);
		dv_id = 0;
	}
	if (vd_path < NUM_IPCORE1)
		layer_id = vd_path;
	else
		pr_dv_dbg("[%s]not support vd_path %d\n", __func__, vd_path);

	/* src_format is valid, need not re-init */
	if (dv_inst[dv_id].amdv_src_format != 0)
		return 0;

	/* vf is not in the dv queue, new frame case */
	if (amdv_vf_check(vf))
		return 0;

	ret = is_amdv_frame(vf);
	if (ret) {
		/* STB hdmi LL input as HDR */
		if (vf->source_type == VFRAME_SOURCE_TYPE_HDMI &&
		    is_aml_tm2_stbmode() &&
		    hdmi_to_stb_policy && ret == 2) {
			if (hdmi_source_led_as_hdr10)
				check_format = FORMAT_HDR10;
			else
				check_format = FORMAT_DOVI_LL;
		} else {
			check_format = FORMAT_DOVI;
		}
	} else if (is_primesl_frame(vf)) {
		check_format = FORMAT_PRIMESL;
	} else if (is_hdr10_frame(vf)) {
		check_format = FORMAT_HDR10;
	} else if (is_hlg_frame(vf)) {
		check_format = FORMAT_HLG;
	} else if (is_hdr10plus_frame(vf)) {
		check_format = FORMAT_HDR10PLUS;
	} else if (is_mvc_frame(vf)) {
		check_format = FORMAT_MVC;
	} else if (is_cuva_frame(vf)) {
		check_format = FORMAT_CUVA;
	} else {
		check_format = FORMAT_SDR;
	}
	if (vf)
		update_src_format(check_format, vf);

	if (!dv_inst[dv_id].amdv_wait_init &&
	    !dv_core1[layer_id].core1_on &&
	    dv_inst[dv_id].amdv_src_format != 0) {
		if (amdv_policy_process
			(vf, &mode, check_format)) {
			if (mode != AMDV_OUTPUT_MODE_BYPASS &&
			    dolby_vision_mode ==
			     AMDV_OUTPUT_MODE_BYPASS) {
				dv_inst[dv_id].amdv_wait_init = true;
				amdv_target_mode = mode;
				amdv_wait_on = true;
				pr_dv_dbg
				("dolby_vision_need_wait src=%d mode=%d\n",
				 check_format, mode);
			}
		}
	}
	pr_dv_dbg
	("[%d]amdv_update_src_format done vf:%p, src=%d, toggle:%d\n",
	 dv_id, vf, dv_inst[dv_id].amdv_src_format, toggle_mode);
	return 1;
}

/* to re-init the src format after video off -> on case */
int amdv_update_src_format(struct vframe_s *vf, u8 toggle_mode, enum vd_path_e vd_path)
{
	int ret = -1;

	if (multi_dv_mode)
		ret = amdv_update_src_format_v2(vf, toggle_mode, vd_path);
	else
		ret = amdv_update_src_format_v1(vf, toggle_mode);

	return ret;
}
EXPORT_SYMBOL(amdv_update_src_format);

static void update_amdv_status(enum signal_format_enum src_format)
{
	if ((src_format == FORMAT_DOVI || src_format == FORMAT_DOVI_LL) &&
	    dolby_vision_status != DV_PROCESS) {
		pr_dv_dbg("Dolby Vision mode changed to DV_PROCESS %d\n",
			     src_format);
		dolby_vision_status = DV_PROCESS;
	} else if (src_format == FORMAT_HDR10 &&
		   dolby_vision_status != HDR_PROCESS) {
		pr_dv_dbg("Dolby Vision mode changed to HDR_PROCESS %d\n",
			     src_format);
		dolby_vision_status = HDR_PROCESS;
	} else if (src_format == FORMAT_HLG &&
		   (is_aml_tvmode() || is_multi_dv_mode()) &&
		   (dolby_vision_status != HLG_PROCESS)) {
		pr_dv_dbg
			("Dolby Vision mode changed to HLG_PROCESS %d\n",
			src_format);
		dolby_vision_status = HLG_PROCESS;

	} else if ((src_format == FORMAT_SDR || src_format == FORMAT_INVALID) &&
		   dolby_vision_status != SDR_PROCESS) {
		pr_dv_dbg("Dolby Vision mode changed to SDR_PROCESS %d\n",
			     src_format);
		dolby_vision_status = SDR_PROCESS;
	}
}

static u8 last_pps_state;
static void bypass_pps_path(u8 pps_state)
{
	if (is_meson_txlx_package_962E()) {
		if (pps_state == 2) {
			VSYNC_WR_DV_REG_BITS(VPP_AMDV_CTRL, 1, 0, 1);
			VSYNC_WR_DV_REG(VPP_DAT_CONV_PARA0, 0x08000800);
		} else if (pps_state == 1) {
			VSYNC_WR_DV_REG_BITS(VPP_AMDV_CTRL, 0, 0, 1);
			VSYNC_WR_DV_REG(VPP_DAT_CONV_PARA0, 0x20002000);
		}
	} else if (is_amdv_stb_mode()) {
		/*not bypass:hdmi pip case may need pps scaler even if in cert mode*/
		if (force_bypass_pps_sr_cm == 2)
			VSYNC_WR_DV_REG_BITS(VPP_AMDV_CTRL, 0, 0, 1);
		/*bypass: force_bypass_pps_sr_cm =1 or in cert mode*/
		else if (force_bypass_pps_sr_cm == 1 ||
			   (dolby_vision_flags & FLAG_CERTIFICATION))
			VSYNC_WR_DV_REG_BITS(VPP_AMDV_CTRL, 1, 0, 1);
#ifdef REMOVE_OLD_DV_FUNC
		else
			if (pps_state == 2)
				VSYNC_WR_DV_REG_BITS
				(VPP_AMDV_CTRL, 1, 0, 1);
			else if (pps_state == 1)
				VSYNC_WR_DV_REG_BITS
				(VPP_AMDV_CTRL, 0, 0, 1);
#endif
	}
	if (pps_state && last_pps_state != pps_state) {
		pr_dv_dbg("pps_state %d => %d\n",
			last_pps_state, pps_state);
		last_pps_state = pps_state;
	}
}

void enable_tunnel_for_capture(void)
{
	/*for vdin1 loop back, 444,12bit->422,12bit->444,8bit*/
	if (enable_tunnel) {
		if (vpp_data_422T0444_backup == 0) {
			vpp_data_422T0444_backup =
			VSYNC_RD_DV_REG(VPU_422T0444_CTRL1);
			pr_dv_dbg("vpp_data_422T0444_backup %x\n",
				     vpp_data_422T0444_backup);
		}
		VSYNC_WR_DV_REG(VPU_422T0444_CTRL1, 0x04c0ba14);
		/* reset RST bit1 to reset tunnel module */
		VSYNC_WR_DV_REG(VPU_422T0444_RST, 0x2);
		VSYNC_WR_DV_REG(VPU_422T0444_RST, 0);
	} else {
		if (vpp_data_422T0444_backup) {
			VSYNC_WR_DV_REG(VPU_422T0444_CTRL1,
			vpp_data_422T0444_backup);
		}
	}
}

void calculate_crc(void)
{
	bool ott_mode = true;

	if (debug_dolby & 0x2000)
		pr_info("setting_update_count %d,crc_read_delay %d,crc_count %d\n",
			setting_update_count, crc_read_delay, crc_count);

	if ((dolby_vision_flags & FLAG_CERTIFICATION) &&
	    !(dolby_vision_flags & FLAG_DISABLE_CRC) &&
	    setting_update_count > crc_count &&
	    is_amdv_on()) {
		s32 delay_count =
			(dolby_vision_flags >>
			FLAG_FRAME_DELAY_SHIFT)
			& FLAG_FRAME_DELAY_MASK;

		if (is_aml_tvmode())
			ott_mode =
				(tv_dovi_setting->input_mode !=
				IN_MODE_HDMI);

		if (is_amdv_stb_mode() &&
			setting_update_count == 1 &&
			crc_read_delay == 1) {
			/* work around to enable crc for frame 0 */
			VSYNC_WR_DV_REG(AMDV_CORE3_CRC_CTRL, 1);
			crc_read_delay++;
		} else {
			crc_read_delay++;
			if (crc_read_delay > delay_count) {
				if (ott_mode) {
					amdv_insert_crc
					((crc_count == 0) ? true : false);
					crc_read_delay = 0;
				} else if (is_aml_tvmode()) {
					/* hdmi mode*/
					if (READ_VPP_DV_REG(AMDV_TV_DIAG_CTRL) == 0xb) {
						u32 crc =
						READ_VPP_DV_REG
						(AMDV_TV_OUTPUT_DM_CRC);
						snprintf(cur_crc, sizeof(cur_crc), "0x%08x", crc);
					}
					crc_count++;
					crc_read_delay = 0;
				}
			}
		}
	}
}

/*In some cases, from full screen to small window, the L5 metadata of the stream does*/
/*not change, and the AOI area does not change, lead to the display to be incomplete*/
/*if vpp disp size smaller than stream source size,  update AOI info*/
static void update_aoi_flag(struct vframe_s *vf, u32 display_size)
{
	int tmp_h;
	int tmp_v;
	int h_ratio = 1;
	int v_ratio = 1;
	int disp_h;
	int disp_v;

	tmp_h = (vf->type & VIDTYPE_COMPRESS) ?
		vf->compWidth : vf->width;
	tmp_v = (vf->type & VIDTYPE_COMPRESS) ?
		vf->compHeight : vf->height;
	if (tmp_h != ((display_size >> 16) & 0xffff) ||
		tmp_v != (display_size & 0xffff)) {
		disp_h = (display_size >> 16) & 0xffff;
		disp_v = display_size & 0xffff;
		if (debug_dolby & 1)
			pr_dv_dbg
			("disp size != src size %d %d->%d %d\n",
			 tmp_h, tmp_v,
			 (display_size >> 16) & 0xffff,
			 display_size & 0xffff);
		aoi_info[0][0] = (tv_dovi_setting->core1_reg_lut[44] >> 12) & 0xfff;
		aoi_info[0][1] = (tv_dovi_setting->core1_reg_lut[44]) & 0xfff;
		aoi_info[0][2] = (tv_dovi_setting->core1_reg_lut[45] >> 12) & 0xfff;
		aoi_info[0][3] = (tv_dovi_setting->core1_reg_lut[45]) & 0xfff;

		if (debug_dolby & 1)
			pr_dv_dbg
			("ori AOI info %d %d %d %d\n",
			 aoi_info[0][0], aoi_info[0][1],
			 aoi_info[0][2], aoi_info[0][3]);

		aoi_info[0][2] = tmp_v - 1 > aoi_info[0][2] ? tmp_v - 1 - aoi_info[0][2] : 0;
		aoi_info[0][3] = tmp_h - 1 > aoi_info[0][3] ? tmp_h - 1 - aoi_info[0][3] : 0;

		h_ratio = tmp_h / ((display_size >> 16) & 0xffff);
		v_ratio = tmp_v / (display_size & 0xffff);
		if (debug_dolby & 1)
			pr_dv_dbg
			("ori crop info %d %d %d %d, ratio %d %d\n",
			 aoi_info[0][0], aoi_info[0][1],
			 aoi_info[0][2], aoi_info[0][3],
			 h_ratio, v_ratio);

		if ((h_ratio >= 2 || v_ratio >= 2) &&
			(aoi_info[0][0] || aoi_info[0][1] || aoi_info[0][2] || aoi_info[0][3])) {
			if (h_ratio >= 2) {
				aoi_info[1][1] = aoi_info[0][1] / h_ratio;
				aoi_info[1][3] = aoi_info[0][3] / h_ratio;
			} else {
				aoi_info[1][1] = aoi_info[0][1];
				aoi_info[1][3] = aoi_info[0][3];
			}
			if (v_ratio >= 2) {
				aoi_info[1][0] = aoi_info[0][0] / v_ratio;
				aoi_info[1][2] = aoi_info[0][2] / v_ratio;
			} else {
				aoi_info[1][0] = aoi_info[0][0];
				aoi_info[1][2] = aoi_info[0][2];
			}
			if (debug_dolby & 1)
				pr_dv_dbg("update crop info %d %d %d %d\n",
						aoi_info[1][0], aoi_info[1][1],
						aoi_info[1][2], aoi_info[1][3]);

			aoi_info[1][2] = disp_v - 1 > aoi_info[1][2] ?
				disp_v - 1 - aoi_info[1][2] : disp_v - 1;
			aoi_info[1][3] = disp_h - 1 > aoi_info[1][3] ?
				disp_h - 1 - aoi_info[1][3] : disp_h - 1;

			if (debug_dolby & 1)
				pr_dv_dbg("update AOI info %d %d %d %d\n",
						aoi_info[1][0], aoi_info[1][1],
						aoi_info[1][2], aoi_info[1][3]);
			update_aoi_info = true;
		} else {
			disable_aoi = true;
			update_aoi_info = false;
		}
	} else {
		disable_aoi = false;
		update_aoi_info = false;
	}
}

static void update_amdv_core2_reg(void)
{
	u32 reg_val, reg_val_set, reg1_val, reg1_val_set, reg_changed = 0;

	if (is_aml_t7() && !tv_mode && update_mali_top_ctrl) {
		reg_val = VSYNC_RD_DV_REG(MALI_AFBCD_TOP_CTRL);
		reg_val_set = reg_val & (~mali_afbcd_top_ctrl_mask);
		if (is_amdv_graphic_on())
			reg_val_set &= ~(1 << 14);
		else
			reg_val_set |= 1 << 14;
		reg_val_set |=
			(mali_afbcd_top_ctrl & mali_afbcd_top_ctrl_mask);
		if (reg_val != reg_val_set) {
			VSYNC_WR_DV_REG(MALI_AFBCD_TOP_CTRL, reg_val_set);
			reg_changed++;
		}

		reg1_val = VSYNC_RD_DV_REG(MALI_AFBCD1_TOP_CTRL);
		reg1_val_set = reg1_val & (~mali_afbcd1_top_ctrl_mask);
		if (is_amdv_on() &&
		    (amdv_mask & 2) &&
		    (core2_sel & 2))
			reg1_val_set &= ~(1 << 19);
		else
			reg1_val_set |= 1 << 19;
		reg1_val_set |=
			(mali_afbcd1_top_ctrl & mali_afbcd1_top_ctrl_mask);
		if (reg1_val != reg1_val_set) {
			VSYNC_WR_DV_REG(MALI_AFBCD1_TOP_CTRL, reg1_val_set);
			reg_changed++;
		}
		if (reg_changed)
			pr_dv_dbg
				("%s reg changed from: (%04x):%08x->%08x, (%04x):%08x->%08x",
				__func__,
				MALI_AFBCD_TOP_CTRL,
				reg_val, reg_val_set,
				MALI_AFBCD1_TOP_CTRL,
				reg1_val, reg1_val_set);
		update_mali_top_ctrl = false;
	}
}

int amdolby_vision_process_v1(struct vframe_s *vf,
			 u32 display_size,
			 u8 toggle_mode, u8 pps_state)
{
	int src_chroma_format = 0;
	u32 h_size = (display_size >> 16) & 0xffff;
	u32 v_size = display_size & 0xffff;
	const struct vinfo_s *vinfo = get_current_vinfo();
	bool reset_flag = false;
	bool force_set = false;
	static int sdr_delay;
	unsigned int mode = dolby_vision_mode;
	static bool video_turn_off = true;
	static bool video_on[VD_PATH_MAX];
	int video_status = 0;
	int graphic_status = 0;
	int policy_changed = 0;
	int sink_changed = 0;
	int format_changed = 0;
	bool src_is_42210bit = false;
	u8 core_mask = 0x7;
	static bool reverse_status;
	bool reverse = false;
	bool reverse_changed = false;
	static u8 last_toggle_mode;
	u32 in_size;
	struct vout_device_s *p_vout = NULL;
	static struct vframe_s *last_vf;

	if (recovery_mode && dolby_vision_on) {/*recovery mode*/
		enable_amdv(0);
		amdv_on_in_uboot = 0;
	}

	if (dolby_vision_flags & FLAG_CERTIFICATION) {
		if (vf) {
			h_size = (vf->type & VIDTYPE_COMPRESS) ?
				vf->compWidth : vf->width;
			v_size = (vf->type & VIDTYPE_COMPRESS) ?
				vf->compHeight : vf->height;
		} else {
			h_size = 0;
			v_size = 0;
		}
		amdv_on_count = 1 +
			amdv_run_mode_delay;

		if (is_aml_tm2())
			enable_tunnel_for_capture();
	} else {
		if (vf && vf != last_vf && tv_dovi_setting)
			update_aoi_flag(vf, display_size);
		last_vf = vf;
	}

	if (vf && (debug_dolby & 0x8))
		pr_dv_dbg("%s: vf %p(index %d), mode %d, core1_on %d\n",
			     __func__, vf, vf->omx_index,
			     dolby_vision_mode, amdv_core1_on);

	if (dolby_vision_flags & FLAG_TOGGLE_FRAME) {
		h_size = (display_size >> 16) & 0xffff;
		v_size = display_size & 0xffff;
		if (!is_aml_tvmode()) {
			/* stb control path case */
			if (new_dovi_setting.video_width & 0xffff &&
			    new_dovi_setting.video_height & 0xffff) {
				if (new_dovi_setting.video_width != h_size ||
				    new_dovi_setting.video_height != v_size) {
					if (debug_dolby & 8)
						pr_dv_dbg
						("stb update disp size %d %d->%d %d\n",
						 new_dovi_setting.video_width,
						 new_dovi_setting.video_height,
						 h_size, v_size);
				}
				if (h_size && v_size) {
					new_dovi_setting.video_width = h_size;
					new_dovi_setting.video_height = v_size;
				} else {
					new_dovi_setting.video_width = 0xffff;
					new_dovi_setting.video_height = 0xffff;
				}

				/* tvcore need a force config for resolution change */
				if (is_aml_stb_hdmimode() && !core1_detunnel() &&
				    (core1_disp_hsize != h_size ||
				     core1_disp_vsize != v_size))
					force_set = true;
			} else if (core1_disp_hsize != h_size ||
				core1_disp_vsize != v_size) {
				if (debug_dolby & 8)
					pr_dv_dbg
					("stb update disp size %d %d->%d %d\n",
					 core1_disp_hsize,
					 core1_disp_vsize,
					 h_size, v_size);
				if (h_size && v_size) {
					new_dovi_setting.video_width = h_size;
					new_dovi_setting.video_height = v_size;
				} else {
					new_dovi_setting.video_width = 0xffff;
					new_dovi_setting.video_height = 0xffff;
				}
			}
		} else {
			/* tv control path case */
			if (core1_disp_hsize != h_size ||
				core1_disp_vsize != v_size) {
				/* tvcore need force config for res change */
				force_set = true;
				if (debug_dolby & 8)
					pr_dv_dbg
					("tv update disp size %d %d -> %d %d\n",
					 core1_disp_hsize,
					 core1_disp_vsize,
					 h_size, v_size);
			}
		}
		if ((!vf || toggle_mode != 1) && !sdr_delay) {
			/* log to monitor if has dv toggles not needed */
			/* !sdr_delay: except in transition from DV to SDR */
			pr_dv_dbg("NULL/RPT frame %p, hdr module %s, video %s\n",
				     vf,
				     get_hdr_module_status(VD1_PATH, VPP_TOP0)
				     == HDR_MODULE_ON ? "on" : "off",
				     get_video_enabled() ? "on" : "off");
		}
	}

	last_toggle_mode = toggle_mode;

	if (debug_dolby & 0x2000)
		pr_info("setting_update_count %d,crc_read_delay %d,crc_count %d\n",
			setting_update_count, crc_read_delay, crc_count);

	if ((dolby_vision_flags & FLAG_CERTIFICATION) &&
	    !(dolby_vision_flags & FLAG_DISABLE_CRC) &&
	    setting_update_count > crc_count &&
	    is_amdv_on()) {
		s32 delay_count =
			(dolby_vision_flags >>
			FLAG_FRAME_DELAY_SHIFT)
			& FLAG_FRAME_DELAY_MASK;
		bool ott_mode = true;

		if (is_aml_tvmode())
			ott_mode =
				(tv_dovi_setting->input_mode !=
				IN_MODE_HDMI);

		if (is_amdv_stb_mode() &&
			setting_update_count == 1 &&
			crc_read_delay == 1) {
			/* work around to enable crc for frame 0 */
			VSYNC_WR_DV_REG(AMDV_CORE3_CRC_CTRL, 1);
			crc_read_delay++;
		} else {
			crc_read_delay++;
			if (crc_read_delay > delay_count) {
				if (ott_mode) {
					amdv_insert_crc
					((crc_count == 0) ? true : false);
					crc_read_delay = 0;
				} else if (is_aml_tvmode()) {
					/* hdmi mode*/
					if (READ_VPP_DV_REG(AMDV_TV_DIAG_CTRL) == 0xb) {
						u32 crc =
						READ_VPP_DV_REG
						(AMDV_TV_OUTPUT_DM_CRC);
						snprintf(cur_crc, sizeof(cur_crc), "0x%08x", crc);
					}
					crc_count++;
					crc_read_delay = 0;
				}
			}
		}
	}

	video_status = is_video_turn_on(video_on, VD1_PATH);
	if (video_status == -1) {
		video_turn_off = true;
		pr_dv_dbg("VD1 video off, video_status -1\n");
	} else if (video_status == 1) {
		pr_dv_dbg("VD1 video on, video_status 1\n");
		video_turn_off = false;
	}

	if (dolby_vision_mode != amdv_target_mode)
		format_changed = 1;

	graphic_status = is_graphic_changed();

	/* monitor policy changes */
	policy_changed = is_policy_changed();

	if (policy_changed || format_changed ||
	    (graphic_status & 2) || osd_update) {
		amdv_set_toggle_flag(1);
		if (osd_update)
			osd_update = false;
	}

	if (!is_amdv_on())
		dolby_vision_flags &= ~FLAG_FORCE_HDMI_PKT;

	sink_changed = (is_sink_cap_changed(vinfo,
		&current_hdr_cap, &current_sink_available, VPP_TOP0) & 2) ? 1 : 0;

	if (is_aml_tvmode()) {
		reverse = get_video_reverse();
		if (reverse != reverse_status) {
			reverse_status = reverse;
			reverse_changed = true;
		}
		sink_changed = false;
		graphic_status = 0;
		dolby_vision_flags &= ~FLAG_FORCE_HDMI_PKT;
		if (policy_changed || format_changed ||
			video_status == 1 || reverse_changed)
			pr_dv_dbg("tv %s %s %s %s\n",
				policy_changed ? "policy changed" : "",
				video_status ? "video_status changed" : "",
				format_changed ? "format_changed" : "",
				reverse_changed ? "reverse_changed" : "");
	}

	if (sink_changed || policy_changed || format_changed ||
	    (video_status == 1 && !(dolby_vision_flags & FLAG_CERTIFICATION)) ||
	    (graphic_status & 2) ||
	    (dolby_vision_flags & FLAG_FORCE_HDMI_PKT) ||
	    need_update_cfg || reverse_changed) {
		if (debug_dolby & 1)
			pr_dv_dbg("sink %s,cap 0x%x,video %s,osd %s,vf %p,toggle %d\n",
				     current_sink_available ? "on" : "off",
				     current_hdr_cap,
				     video_turn_off ? "off" : "on",
				     is_graphics_output_off() ? "off" : "on",
				     vf, toggle_mode);
		/* do not toggle a new el vf */
		if (toggle_mode == 1)
			toggle_mode = 0;
		if (vf &&
		    !amdv_parse_metadata
		    (vf, VD1_PATH, toggle_mode, false, false)) {
			h_size = (display_size >> 16) & 0xffff;
			v_size = display_size & 0xffff;
			if (!is_aml_tvmode()) {
				new_dovi_setting.video_width = h_size;
				new_dovi_setting.video_height = v_size;
			}
			amdv_set_toggle_flag(1);
		}
		need_update_cfg = false;
	}

	if (debug_dolby & 8)
		pr_dv_dbg("vf %p, turn_off %d, video_status %d, toggle %d, flag %x\n",
			vf, video_turn_off, video_status,
			toggle_mode, dolby_vision_flags);

	if ((!vf && video_turn_off) ||
	    (video_status == -1)) {
		if (amdv_policy_process(vf, &mode, FORMAT_SDR)) {
			pr_dv_dbg("Fake SDR, mode->%d\n", mode);
			if (dolby_vision_policy == AMDV_FOLLOW_SOURCE &&
			    mode == AMDV_OUTPUT_MODE_BYPASS) {
				amdv_target_mode =
					AMDV_OUTPUT_MODE_BYPASS;
				dolby_vision_mode =
					AMDV_OUTPUT_MODE_BYPASS;
				amdv_set_toggle_flag(0);
				amdv_wait_on = false;
				amdv_wait_init = false;
			} else {
				amdv_set_toggle_flag(1);
			}
		}
		if ((dolby_vision_flags & FLAG_TOGGLE_FRAME) ||
		((video_status == -1) && amdv_core1_on)) {
			pr_dv_dbg("update when video off\n");
			amdv_parse_metadata
				(NULL, VD1_PATH, 1, false, false);
			amdv_set_toggle_flag(1);
		}
		if (!vf && video_turn_off &&
			!amdv_core1_on &&
			amdv_src_format != 0) {
			pr_dv_dbg("update src_fmt when video off\n");
			amdv_src_format = 0;
		}
	}

	if (dolby_vision_mode == AMDV_OUTPUT_MODE_BYPASS) {
		if (!is_aml_tvmode()) {
			if (vinfo && vinfo->vout_device &&
			    !vinfo->vout_device->dv_info &&
			    vsync_count < FLAG_VSYNC_CNT) {
				vsync_count++;
				update_amdv_core2_reg();
				return 0;
			}
		}
		if (dolby_vision_status != BYPASS_PROCESS) {
			if (vinfo && !is_aml_tvmode()) {
				if (vf && is_primesl_frame(vf)) {
					/* disable dv immediately */
					pr_dv_dbg("DV bypass: PRIMESL: Switched to SDR first\n");
					send_hdmi_pkt(FORMAT_PRIMESL,
						      FORMAT_SDR, vinfo, vf);
					enable_amdv(0);
				} else if (vf && is_hdr10plus_frame(vf)) {
					/* disable dv immediately */
					pr_dv_dbg("DV bypass: HDR10+: Switched to SDR first\n");
					send_hdmi_pkt(FORMAT_HDR10PLUS,
						      FORMAT_SDR, vinfo, vf);
					enable_amdv(0);
				} else if (vf && is_hlg_frame(vf)) {
					/* disable dv immediately */
					pr_dv_dbg("DV bypass: HLG: Switched to SDR first\n");
					send_hdmi_pkt(FORMAT_HLG,
						      FORMAT_SDR, vinfo, vf);
					enable_amdv(0);
				} else if (vf && is_cuva_frame(vf)) {
					/* disable dolby immediately */
					pr_dv_dbg("Dolby bypass: cuva: Switched to SDR first\n");
					send_hdmi_pkt(FORMAT_CUVA,
						      FORMAT_SDR, vinfo, vf);
					enable_amdv(0);
				} else if (last_dst_format != FORMAT_DOVI) {
					/* disable dv immediately:
					 * non-dv alwyas hdr to adaptive
					 */
					pr_dv_dbg("DV bypass: Switched %d to SDR\n",
						     last_dst_format);
					send_hdmi_pkt(amdv_src_format,
						      FORMAT_SDR, vinfo, vf);
					enable_amdv(0);
				} else {
					/* disable dv after sdr delay:
					 * dv always hdr to adaptive or dv
					 * playback exit in adaptive mode on
					 * a dv tv
					 */
					if (sdr_delay == 0) {
						pr_dv_dbg("DV bypass: Start - Switched to SDR\n");
						amdv_set_toggle_flag(1);
					}
					in_size = (dovi_setting.video_width << 16) |
							(dovi_setting.video_height);

					if ((get_video_mute() ==
					VIDEO_MUTE_ON_DV &&
					!(dolby_vision_flags & FLAG_MUTE)) ||
					(get_video_mute() == VIDEO_MUTE_OFF &&
					dolby_vision_flags & FLAG_MUTE)) {
						/* core 3 only */
						apply_stb_core_settings
						(dma_paddr,
						 amdv_setting_video_flag,
						 false,
						 amdv_mask & 0x4,
						 0, 0, in_size, 0, pps_state);
					}
					send_hdmi_pkt(amdv_src_format,
						      FORMAT_SDR, vinfo, vf);
					if (sdr_delay >= MAX_TRANSITION_DELAY) {
						pr_dv_dbg("DV bypass: Done - Switched to SDR\n");
						enable_amdv(0);
						sdr_delay = 0;
					} else {
						sdr_delay++;
					}
				}
				/* if tv ko loaded for tm2/T7 stb*/
				/* switch to tv vore */
				if ((is_aml_tm2_stbmode() ||
				     is_aml_t7_stbmode()) &&
					p_funcs_tv && !p_funcs_stb &&
					!is_amdv_on()) {
					pr_dv_dbg("Switch from STB to TV core Done\n");
					switch_to_tv_mode();
				}
			} else {
				enable_amdv(0);
			}
		}
		if (sdr_delay == 0)
			dolby_vision_flags &= ~FLAG_TOGGLE_FRAME;
		update_amdv_core2_reg();
		return 0;
	} else if (sdr_delay != 0) {
		/* in case mode change to a mode requiring dolby block */
		sdr_delay = 0;
	}

	if ((dolby_vision_flags & FLAG_CERTIFICATION) ||
	    (dolby_vision_flags & FLAG_BYPASS_VPP))
		video_effect_bypass(1);

	if (!p_funcs_stb && !p_funcs_tv) {
		dolby_vision_flags &= ~FLAG_TOGGLE_FRAME;
		tv_dovi_setting_change_flag = false;
		new_dovi_setting.video_width = 0;
		new_dovi_setting.video_height = 0;
		update_amdv_core2_reg();
		return 0;
	}
	if ((debug_dolby & 2) && force_set &&
	    !(dolby_vision_flags & FLAG_CERTIFICATION))
		pr_dv_dbg
			("core1 size changed--old: %d x %d, new: %d x %d\n",
			 core1_disp_hsize, core1_disp_vsize,
			 h_size, v_size);

	if (!(dolby_vision_flags & FLAG_CERTIFICATION)) {
		if (amdv_core1_on &&
		    amdv_core1_on_cnt < DV_CORE1_RECONFIG_CNT &&
		    !(dolby_vision_flags & FLAG_TOGGLE_FRAME) &&
		    !is_aml_tvmode()) {
			amdv_set_toggle_flag(1);
			if (!(dolby_vision_flags & FLAG_CERTIFICATION))
				pr_dv_dbg
			("Need update core1 setting first %d times, force toggle frame\n",
			 amdv_core1_on_cnt);
		}
	}
	if (dolby_vision_on && !amdv_core1_on &&
	    amdv_core2_on_cnt &&
	    amdv_core2_on_cnt < DV_CORE2_RECONFIG_CNT &&
	    !(dolby_vision_flags & FLAG_TOGGLE_FRAME) &&
	    !is_aml_tvmode() &&
	    !(dolby_vision_flags & FLAG_CERTIFICATION)) {
		force_set_lut = true;
		amdv_set_toggle_flag(1);
		if (debug_dolby & 2)
			pr_dv_dbg("Need update core2 first %d times\n",
			     amdv_core2_on_cnt);
	}

	if (dolby_vision_flags & FLAG_TOGGLE_FRAME) {
		if (!(dolby_vision_flags & FLAG_CERTIFICATION))
			reset_flag =
				(amdv_reset & 1) &&
				(!amdv_core1_on) &&
				(amdv_on_count == 0);
		if (is_aml_tvmode()) {
			if (tv_dovi_setting_change_flag || force_set) {
				if (vf && (vf->type & VIDTYPE_VIU_422))
					src_chroma_format = 2;
				else if (vf)
					src_chroma_format = 1;
				/* mark by brian.zhu 2021.4.27 */
#ifdef NEED_REMOVE
				if (force_set &&
				    !(dolby_vision_flags & FLAG_CERTIFICATION))
					reset_flag = true;
#endif
				if ((dolby_vision_flags & FLAG_CERTIFICATION)) {
					if (tv_dovi_setting->src_format ==
						FORMAT_HDR10 ||
						tv_dovi_setting->src_format ==
						FORMAT_HLG ||
						tv_dovi_setting->src_format ==
						FORMAT_SDR)
						src_is_42210bit = true;
				} else {
					if (tv_dovi_setting->src_format ==
					    FORMAT_HDR10 ||
					    tv_dovi_setting->src_format ==
					    FORMAT_HLG)
						src_is_42210bit = true;
				}

				tv_dv_core1_set
				(tv_dovi_setting->core1_reg_lut,
				 dma_paddr, h_size, v_size,
				 amdv_setting_video_flag, /* BL enable */
				 amdv_setting_video_flag &&
				 (tv_dovi_setting->el_flag),
				 tv_dovi_setting->el_halfsize_flag,
				 src_chroma_format,
				 tv_dovi_setting->input_mode == IN_MODE_HDMI,
				 src_is_42210bit, reset_flag);
				if (!h_size || !v_size)
					amdv_setting_video_flag = false;
				if (amdv_setting_video_flag &&
				    amdv_on_count == 0)
					pr_dv_dbg("first frame reset %d\n",
						     reset_flag);
				enable_amdv(1);
				if (tv_dovi_setting->backlight !=
				    tv_backlight ||
				    (amdv_setting_video_flag &&
				    amdv_on_count == 0) ||
				    tv_backlight_force_update) {
					pr_dv_dbg("backlight %d -> %d\n",
						tv_backlight,
						tv_dovi_setting->backlight);
					tv_backlight =
						tv_dovi_setting->backlight;
					tv_backlight_changed = true;
					bl_delay_cnt = 0;
					tv_backlight_force_update = false;
				}

				tv_dovi_setting_change_flag = false;
				core1_disp_hsize = h_size;
				core1_disp_vsize = v_size;
				update_amdv_status
					(tv_dovi_setting->src_format);
			}
		} else {
			if (((new_dovi_setting.video_width & 0xffff) &&
			    (new_dovi_setting.video_height & 0xffff)) ||
			    force_set_lut) {
				if (new_dovi_setting.video_width == 0xffff)
					new_dovi_setting.video_width = 0;
				if (new_dovi_setting.video_height == 0xffff)
					new_dovi_setting.video_height = 0;
				if (vf && (vf->type & VIDTYPE_VIU_422))
					src_chroma_format = 2;
				else if (vf)
					src_chroma_format = 1;

				if (is_aml_stb_hdmimode() && !core1_detunnel()) {
					core_mask = 0x6;
					tv_dv_core1_set
					(tv_dovi_setting->core1_reg_lut,
					 dma_paddr,
					 new_dovi_setting.video_width,
					 new_dovi_setting.video_height,
					 amdv_setting_video_flag,
					 false, false, 2, true, false,
					 reset_flag);
				}

				if (force_set &&
					!(dolby_vision_flags
					& FLAG_CERTIFICATION))
					reset_flag = true;

				apply_stb_core_settings
					(dma_paddr,
					 amdv_setting_video_flag,
					 false,
					 amdv_mask & core_mask,
					 reset_flag, 0,
					 (new_dovi_setting.video_width << 16)
					 | new_dovi_setting.video_height,
					 0, pps_state);
				memcpy(&dovi_setting, &new_dovi_setting,
					sizeof(dovi_setting));
				if ((core1_disp_hsize !=
					dovi_setting.video_width ||
					core1_disp_vsize !=
					dovi_setting.video_height) &&
					core1_disp_hsize && core1_disp_vsize)
					pr_dv_dbg
					("frame size %d %d->%d %d\n",
					 core1_disp_hsize,
					 core1_disp_vsize,
					 dovi_setting.video_width,
					 dovi_setting.video_height);
				new_dovi_setting.video_width = 0;
				new_dovi_setting.video_height = 0;
				if (!dovi_setting.video_width ||
				    !dovi_setting.video_height)
					amdv_setting_video_flag = false;
				if (amdv_setting_video_flag &&
				    amdv_on_count == 0)
					pr_dv_dbg("first frame reset %d\n",
						     reset_flag);
				/* clr hdr+ pkt when enable dv */
				if (!dolby_vision_on &&
				    vinfo && vinfo->vout_device) {
					p_vout = vinfo->vout_device;
					if (p_vout->fresh_tx_hdr10plus_pkt)
						p_vout->fresh_tx_hdr10plus_pkt
							(0, NULL);
				}
				enable_amdv(1);
				bypass_pps_path(pps_state);
				core1_disp_hsize =
					dovi_setting.video_width;
				core1_disp_vsize =
					dovi_setting.video_height;
				/* send HDMI packet according to dst_format */
				if (vinfo)
					send_hdmi_pkt
						(dovi_setting.src_format,
						 dovi_setting.dst_format,
						 vinfo, vf);
				update_amdv_status
					(dovi_setting.src_format);
			} else {
				if ((get_video_mute() == VIDEO_MUTE_ON_DV &&
				     !(dolby_vision_flags & FLAG_MUTE)) ||
				    (get_video_mute() == VIDEO_MUTE_OFF &&
				     dolby_vision_flags & FLAG_MUTE) ||
				     last_dolby_vision_ll_policy !=
				     dolby_vision_ll_policy)
					/* core 3 only */
					apply_stb_core_settings
					(dma_paddr,
					 amdv_setting_video_flag,
					 false,
					 amdv_mask & 0x4,
					 reset_flag, 0,
					 (dovi_setting.video_width << 16)
					 | dovi_setting.video_height,
					 0, pps_state);
				/* force send hdmi pkt */
				if (dolby_vision_flags & FLAG_FORCE_HDMI_PKT) {
					if (vinfo)
						send_hdmi_pkt
						(dovi_setting.src_format,
						 dovi_setting.dst_format,
						 vinfo, vf);
				}
			}
		}
		dolby_vision_flags &= ~FLAG_TOGGLE_FRAME;
	} else if (amdv_core1_on &&
		!(dolby_vision_flags & FLAG_CERTIFICATION)) {
		bool reset_flag =
			(amdv_reset & 2) &&
			(amdv_on_count <=
			(amdv_reset_delay >> 8)) &&
			(amdv_on_count >=
			(amdv_reset_delay & 0xff));

		if (is_aml_txlx_stbmode()) {
			if (amdv_on_count <= amdv_run_mode_delay ||
			    force_set) {
				if (force_set)
					reset_flag = true;
				apply_stb_core_settings
				(dma_paddr,
				 amdv_setting_video_flag,
				 false,
				 /* core 1 only */
				 amdv_mask & 0x1,
				 reset_flag, 0,
				 (h_size << 16) | v_size,
				 0, pps_state);
				bypass_pps_path(pps_state);
				core1_disp_hsize = h_size;
				core1_disp_vsize = v_size;
				if (amdv_on_count <=
					amdv_run_mode_delay)
					pr_dv_dbg("fake frame %d reset %d\n",
						amdv_on_count,
						reset_flag);
			}
		} else if (is_aml_tvmode()) {
			if (amdv_on_count <= amdv_run_mode_delay + 1 ||
			    force_set) {
				if (force_set)
					reset_flag = true;
				if (dolby_vision_flags & FLAG_CERTIFICATION) {
					if (tv_dovi_setting->src_format ==
					FORMAT_HDR10 ||
					tv_dovi_setting->src_format ==
					FORMAT_HLG ||
					tv_dovi_setting->src_format ==
					FORMAT_SDR)
						src_is_42210bit = true;
				} else {
					if (tv_dovi_setting->src_format ==
					    FORMAT_HDR10 ||
					    tv_dovi_setting->src_format ==
					    FORMAT_HLG)
						src_is_42210bit = true;
				}
				tv_dv_core1_set
				(tv_dovi_setting->core1_reg_lut,
				 dma_paddr, h_size, v_size,
				 amdv_setting_video_flag, /* BL enable */
				 amdv_setting_video_flag &&
				 tv_dovi_setting->el_flag && !mel_mode, /*ELenable*/
				 tv_dovi_setting->el_halfsize_flag,
				 src_chroma_format,
				 tv_dovi_setting->input_mode == IN_MODE_HDMI,
				 src_is_42210bit,
				 reset_flag);
				core1_disp_hsize = h_size;
				core1_disp_vsize = v_size;
				if (amdv_on_count <=
					amdv_run_mode_delay + 1)
					pr_dv_dbg("fake frame %d reset %d\n",
						amdv_on_count,
						reset_flag);
			}
		} else if (is_amdv_stb_mode()) {
			if (amdv_on_count <= amdv_run_mode_delay ||
			    force_set) {
				if (force_set)
					reset_flag = true;
				if (is_aml_stb_hdmimode() && !core1_detunnel())
					tv_dv_core1_set
					(tv_dovi_setting->core1_reg_lut,
					 dma_paddr,
					 core1_disp_hsize, core1_disp_vsize,
					 amdv_setting_video_flag,
					 false, false, 2, true, false, reset_flag);
				else
					apply_stb_core_settings
					(dma_paddr,
					 true, /* always enable */false,
					 /* core 1 only */
					 amdv_mask & 0x1,
					 reset_flag, 0,
					 (core1_disp_hsize << 16) | core1_disp_vsize,
					 0, pps_state);
				if (amdv_on_count <
					amdv_run_mode_delay)
					pr_dv_dbg("fake frame (%d %d) %d reset %d\n",
						     core1_disp_hsize,
						     core1_disp_vsize,
						     amdv_on_count,
						     reset_flag);
			}
		}
	}
	if (amdv_core1_on) {
		if (amdv_on_count <=
			amdv_run_mode_delay + 1)
			amdv_on_count++;
		if (debug_dolby & 8)
			pr_dv_dbg("%s: amdv_on_count %d\n",
				     __func__, amdv_on_count);
	} else {
		amdv_on_count = 0;
	}
	update_amdv_core2_reg();
	return 0;
}

#ifdef ADD_NEW_DV_FUNC
/*multi-inst tv mode, to do*/
static int amdolby_vision_process_v2_tv(struct vframe_s *vf,
						u32 display_size,
						u8 toggle_mode,
						u8 pps_state)
{
	return -1;
}
#endif

/*vf: display on vd1, vf_2: dislpay on vd2*/
/*display_size: video size before core1*/
static int amdolby_vision_process_v2_stb
	(struct vframe_s *vf, u32 display_size,
	 struct vframe_s *vf_2, u32 display_size_2,
	 u8 toggle_mode_1, u8 toggle_mode_2, u8 pps_state)
{
	u32 h_size[2] = {0};
	u32 v_size[2] = {0};
	const struct vinfo_s *vinfo = get_current_vinfo();
	bool reset_flag[NUM_IPCORE1] = {false};
	bool force_set = false;
	static int sdr_delay;
	unsigned int mode = dolby_vision_mode;
	static bool video_turn_off[2] = {true, true};
	static bool video_on[2] = {false};
	int video_status[2] = {0, 0};
	int graphic_status = 0;
	int policy_changed = 0;
	int sink_changed = 0;
	int format_changed = 0;
	u8 core_mask = 0x7;
	u32 in_size;
	u32 in_size_2;
	int i;
	int dv_id = 0;
	bool path_switch = false;
	enum vd_path_e vd_path = VD1_PATH;
	enum vd_path_e vd_path_2 = VD2_PATH;
	static bool stb_hdmi_mode;
	bool cur_stb_hdmi_mode;
	bool need_cp = false;
	enum signal_format_enum cur_src_format;
	static int last_pri_input;
	bool pri_change = false;

	if (vf) {
		if (dv_inst_valid(vf->src_fmt.dv_id))
			dv_id = vf->src_fmt.dv_id;
		else
			dv_id = 0;
		//if (dv_id != vd_path)
			//path_switch = true;

		dv_inst[dv_id].layer_id = vd_path;
		vd1_inst_id = dv_id;

		/*in one_core1 case, set layer id invalid for other instance*/
		/*if (!support_multi_core1())*/
			/*dv_inst[dv_id ^ 1].layer_id = VD_PATH_MAX;*/

		if (debug_dolby & 0x1000)
			pr_dv_dbg("[inst%d]process %p on vd%d, toggle %d\n",
				     dv_id + 1, vf, vd_path + 1, toggle_mode_1);
	} else {
		vd1_inst_id = -1;
	}
	if (vf_2) {
		if (dv_inst_valid(vf_2->src_fmt.dv_id))
			dv_id = vf_2->src_fmt.dv_id;
		else
			dv_id = 1;
		//if (dv_id != vd_path_2)
			//path_switch = true;

		dv_inst[dv_id].layer_id = vd_path_2;
		vd2_inst_id = dv_id;

		if (debug_dolby & 0x1000)
			pr_dv_dbg("[inst%d]process %p on vd%d, toggle %d\n",
				     dv_id + 1, vf_2, vd_path_2 + 1, toggle_mode_2);
	} else {
		vd2_inst_id = -1;
	}

	/*update layer info*/
	/*coverity[check_after_sink] No negative array index reading occurred.*/
	if (vd1_inst_id >= 0 && vd2_inst_id >= 0) {
		for (i = 0; i < NUM_INST; i++) {
			if (i != vd1_inst_id && i != vd2_inst_id)
				dv_inst[i].layer_id = VD_PATH_MAX;
		}
		if (!(dolby_vision_flags & FLAG_CERTIFICATION))
			pri_input = 0;
		if (vf && vf->source_type == VFRAME_SOURCE_TYPE_HDMI) {
			hdmi_path_id = VD1_PATH;
			hdmi_inst_id = vd1_inst_id;
		} else if (vf_2 && vf_2->source_type == VFRAME_SOURCE_TYPE_HDMI) {
			hdmi_path_id = VD2_PATH;
			hdmi_inst_id = vd2_inst_id;
		} else {
			hdmi_path_id = VD_PATH_MAX;
			hdmi_inst_id = -1;
		}
	} else if (vd1_inst_id == -1 && vd2_inst_id >= 0) {
		for (i = 0; i < NUM_INST; i++) {
			if (i != vd2_inst_id)
				dv_inst[i].layer_id = VD_PATH_MAX;
		}
		if (!(dolby_vision_flags & FLAG_CERTIFICATION))
			pri_input = 1;
		if (vf_2 && vf_2->source_type == VFRAME_SOURCE_TYPE_HDMI) {
			hdmi_path_id = VD2_PATH;
			hdmi_inst_id = vd2_inst_id;
		} else {
			hdmi_path_id = VD_PATH_MAX;
			hdmi_inst_id = -1;
		}
	} else if (vd2_inst_id == -1 && vd1_inst_id >= 0) {
		for (i = 0; i < NUM_INST; i++) {
			if (i != vd1_inst_id)
				dv_inst[i].layer_id = VD_PATH_MAX;
		}
		if (!(dolby_vision_flags & FLAG_CERTIFICATION))
			pri_input = 0;
		if (vf && vf->source_type == VFRAME_SOURCE_TYPE_HDMI) {
			hdmi_path_id = VD1_PATH;
			hdmi_inst_id = vd1_inst_id;
		} else {
			hdmi_path_id = VD_PATH_MAX;
			hdmi_inst_id = -1;
		}
	} else if (vd1_inst_id == -1 && vd2_inst_id == -1) {
		for (i = 0; i < NUM_INST; i++)
			dv_inst[i].layer_id = VD_PATH_MAX;
		hdmi_path_id = VD_PATH_MAX;
		hdmi_inst_id = -1;
		if (!(dolby_vision_flags & FLAG_CERTIFICATION))
			pri_input = 0;
	}

	if (!layerid_valid(hdmi_path_id) || !dv_inst_valid(hdmi_inst_id)) {
		if (tv_dovi_setting)
			tv_dovi_setting->input_mode = 0;
		hdmi_in_allm = false;
	}

	if (force_pri_input == 0 || force_pri_input == 1) {
		if (debug_dolby & 0x1000)
			pr_dv_dbg("use force_pri_input %d\n", force_pri_input);
		pri_input = force_pri_input;
	}

	if (path_switch != core1a_core1b_switch) {
		core1a_core1b_switch = path_switch;
		pr_dv_dbg("core1a core1b setting switched %d, vd1_inst_id %d, pri_input %d\n",
		core1a_core1b_switch, vd1_inst_id, pri_input);
	}

	if (update_control_path_flag) {
		amdv_control_path(vf, vf_2);
		update_control_path_flag = false;
	}

	if (dolby_vision_flags & FLAG_CERTIFICATION) {
		if (vf) {
			h_size[0] = (vf->type & VIDTYPE_COMPRESS) ?
				vf->compWidth : vf->width;
			v_size[0] = (vf->type & VIDTYPE_COMPRESS) ?
				vf->compHeight : vf->height;
		}
		if (vf_2) {
			h_size[1] = (vf_2->type & VIDTYPE_COMPRESS) ?
				vf_2->compWidth : vf_2->width;
			v_size[1] = (vf_2->type & VIDTYPE_COMPRESS) ?
				vf_2->compHeight : vf_2->height;
		}

		/*don't use run mode for cert*/
		for (i = 0; i < NUM_IPCORE1; i++)
			dv_core1[i].run_mode_count = 1 +
				amdv_run_mode_delay;

		if (is_aml_tm2())
			enable_tunnel_for_capture();
	}

	if (vf && (debug_dolby & 0x8))
		pr_dv_dbg("%s:vf %p(index %d),mode %d,%x,core1_on %d %d,pri %d\n",
		     __func__, vf, vf->omx_index,
		     dolby_vision_mode, dolby_vision_flags, dv_core1[0].core1_on,
		     dv_core1[1].core1_on, pri_input);

	if (dolby_vision_flags & FLAG_TOGGLE_FRAME) {
		if (vf) {
			h_size[0] = (display_size >> 16) & 0xffff;
			v_size[0] = display_size & 0xffff;
		}
		if (vf_2) {
			h_size[1] = (display_size_2 >> 16) & 0xffff;
			v_size[1] = display_size_2 & 0xffff;
		}

		for (i = 0; i < NUM_IPCORE1; i++) {
			if (dv_core1[i].core1_disp_hsize != h_size[i] ||
				dv_core1[i].core1_disp_vsize != v_size[i]) {
				if (debug_dolby & 1)
					pr_dv_dbg
					("stb update core1_%s disp size %d %d -> %d %d\n",
					 i == 0 ? "a" : "b",
					 dv_core1[i].core1_disp_hsize,
					 dv_core1[i].core1_disp_vsize,
					 h_size[i], v_size[i]);
#ifdef REMOVE_OLD_DV_FUNC
				/* tvcore need a force config for resolution change */
				if (is_aml_stb_hdmimode() &&
				    (dv_core1[i].core1_disp_hsize != h_size[i] ||
				     dv_core1[i].core1_disp_vsize != v_size[i])) {
					force_set = true;
					if (debug_dolby & 1)
						pr_dv_dbg
						("core1_%s display size %d %d -> %d %d\n",
						 i == 0 ? "a" : "b",
						 dv_core1[i].core1_disp_hsize,
						 dv_core1[i].core1_disp_vsize,
						 h_size[i], v_size[i]);
				}
#endif
			}
			if (h_size[i] && v_size[i]) {
				dv_core1[i].core1_disp_hsize = h_size[i];
				dv_core1[i].core1_disp_vsize = v_size[i];
			} else {
				dv_core1[i].core1_disp_hsize = 0xffff;
				dv_core1[i].core1_disp_vsize = 0xffff;
			}
		}
		if (((!vf && !vf_2) || (toggle_mode_1 != 1 && toggle_mode_2 != 1)) && !sdr_delay) {
			/* log to monitor if has dv toggles not needed */
			/* !sdr_delay: except in transition from DV to SDR */
			if (debug_dolby & 1)
				pr_dv_dbg("NULL/RPT %p %p,toggle %d %d,hdr %s,video %s\n",
					     vf, vf_2, toggle_mode_1, toggle_mode_2,
					     get_hdr_module_status(VD1_PATH, VPP_TOP0)
					     == HDR_MODULE_ON ? "on" : "off",
					     get_video_enabled() ? "on" : "off");
		}
	}

	calculate_crc();

	/*stb hdmi mode change, force set reg and lut*/
	cur_stb_hdmi_mode = is_aml_stb_hdmimode();
	if (cur_stb_hdmi_mode != stb_hdmi_mode)
		force_set = true;
	stb_hdmi_mode = cur_stb_hdmi_mode;

	video_status[0] = is_video_turn_on(video_on, VD1_PATH);
	if (video_status[0] == -1) {
		video_turn_off[0] = true;
		pr_dv_dbg("VD1 video off, video_status -1\n");
	} else if (video_status[0] == 1) {
		pr_dv_dbg("VD1 video on, video_status 1\n");
		video_turn_off[0] = false;
	}

	video_status[1] = is_video_turn_on(video_on, VD2_PATH);
	if (video_status[1] == -1) {
		video_turn_off[1] = true;
		pr_dv_dbg("VD2 video off, video_status -1\n");
	} else if (video_status[1] == 1) {
		pr_dv_dbg("VD2 video on, video_status 1\n");
		video_turn_off[1] = false;
	}

	if (dolby_vision_mode != amdv_target_mode)
		format_changed = 1;

	graphic_status = is_graphic_changed();

	/* monitor policy changes */
	policy_changed = is_policy_changed();

	if (policy_changed || format_changed ||
	    (graphic_status & 2) || osd_update) {
		amdv_set_toggle_flag(1);
		if (osd_update)
			osd_update = false;
	}

	if (!is_amdv_on())
		dolby_vision_flags &= ~FLAG_FORCE_HDMI_PKT;

	sink_changed = (is_sink_cap_changed(vinfo,
		&current_hdr_cap, &current_sink_available, VPP_TOP0) & 2) ? 1 : 0;

	if (last_pri_input != pri_input) {
		pri_change = true;
		if (debug_dolby & 1)
			pr_dv_dbg("pri_change changed %d=> %d\n",
				  last_pri_input, pri_input);
		last_pri_input = pri_input;
	}

	if (sink_changed || policy_changed || format_changed ||
	    video_status[0] == 1 || video_status[1] == 1 ||
	    (graphic_status & 2) || pri_change ||
	    (dolby_vision_flags & FLAG_FORCE_HDMI_PKT)) {
		if (debug_dolby & 1)
			pr_dv_dbg("sink %s,cap 0x%x,vd1 %s,vd2 %s,osd %s,vf %p %p,toggle %d %d\n",
				     current_sink_available ? "on" : "off",
				     current_hdr_cap,
				     video_turn_off[0] ? "off" : "on",
				     video_turn_off[1] ? "off" : "on",
				     is_graphics_output_off() ? "off" : "on",
				     vf, vf_2, toggle_mode_1, toggle_mode_2);
		/* do not toggle a new el vf */
		if (toggle_mode_1 == 1)
			toggle_mode_1 = 0;
		if (toggle_mode_2 == 1)
			toggle_mode_2 = 0;
		if (vf &&
		    !amdv_parse_metadata
		    (vf, vd_path, toggle_mode_1, false, false)) {
			amdv_set_toggle_flag(1);
			need_cp = true;
		}
		if (vf_2 &&
		    !amdv_parse_metadata
		    (vf_2, vd_path_2, toggle_mode_2, false, false)) {
			amdv_set_toggle_flag(1);
			need_cp = true;
		}
		if (need_cp)
			amdv_control_path(vf, vf_2);

		if (vf) {
			h_size[0] = (display_size >> 16) & 0xffff;
			v_size[0] = display_size & 0xffff;
			if (h_size[0] && v_size[0]) {
				dv_core1[0].core1_disp_hsize = h_size[0];
				dv_core1[0].core1_disp_vsize = v_size[0];
			} else {
				dv_core1[0].core1_disp_hsize = 0xffff;
				dv_core1[0].core1_disp_vsize = 0xffff;
			}
		}
		if (vf_2) {
			h_size[1] = (display_size_2 >> 16) & 0xffff;
			v_size[1] = display_size_2 & 0xffff;
			if (h_size[1] && v_size[1]) {
				dv_core1[1].core1_disp_hsize = h_size[1];
				dv_core1[1].core1_disp_vsize = v_size[1];
			} else {
				dv_core1[1].core1_disp_hsize = 0xffff;
				dv_core1[1].core1_disp_vsize = 0xffff;
			}
		}
		force_set_lut = true;
	}
	if (inst_debug[0]) {
		dv_core1[0].core1_disp_hsize = inst_res_debug[0];
		dv_core1[0].core1_disp_vsize = inst_res_debug[1];
	} else if (inst_debug[1]) {
		dv_core1[1].core1_disp_hsize = inst_res_debug[2];
		dv_core1[1].core1_disp_vsize = inst_res_debug[3];
	}
	if (debug_dolby & 0x1000)
		pr_dv_dbg("dv_core %d_%d,  %d_%d\n",
			     dv_core1[0].core1_disp_hsize,
			     dv_core1[0].core1_disp_vsize,
			     dv_core1[1].core1_disp_hsize,
			     dv_core1[1].core1_disp_vsize);

	if (debug_dolby & 0x1000)
		pr_dv_dbg("vf %p,vd1 %s,st %d,vf_2 %p,vd2 %s,st %d\n",
			     vf, video_turn_off[0] ? "off" : "on",
			     video_status[0],
			     vf_2, video_turn_off[1] ? "off" : "on",
			     video_status[1]);

	if (debug_dolby & 8)
		pr_dv_dbg("vf %p %p,toggle %d %d,flag %x,mode %d,status %d\n",
			vf, vf_2, toggle_mode_1, toggle_mode_2, dolby_vision_flags,
			dolby_vision_mode, dolby_vision_status);

	if (support_multi_core1()) {
		/*case1: no vd1 vf and no vd2 vf */
		/*case2: no vd1 vf or vd1 turn off*/
		/*case3: no vd2 vf or vd2 turn off*/
		if ((!vf && video_turn_off[0] && !vf_2 && video_turn_off[1]) ||
		    (video_status[0] == -1 && video_turn_off[0]) ||
		    (video_status[1] == -1 && video_turn_off[1])) {
			if (amdv_policy_process(vf, &mode, FORMAT_SDR)) {
				pr_dv_dbg("Fake SDR, mode->%d\n", mode);
				if (dolby_vision_policy == AMDV_FOLLOW_SOURCE &&
				    mode == AMDV_OUTPUT_MODE_BYPASS) {
					amdv_target_mode =
						AMDV_OUTPUT_MODE_BYPASS;
					dolby_vision_mode =
						AMDV_OUTPUT_MODE_BYPASS;
					amdv_set_toggle_flag(0);
					amdv_wait_on = false;
					dv_inst[dv_id].amdv_wait_init = false;
				} else {
					amdv_set_toggle_flag(1);
				}
			}
			need_cp = false;
			if ((!vf && video_turn_off[0]) ||
			    video_status[0] == -1) {
				if ((dolby_vision_flags & FLAG_TOGGLE_FRAME) ||
				    ((video_status[0] == -1) && dv_core1[0].core1_on)) {
					pr_dv_dbg("update when vd1 off\n");
					if (!amdv_parse_metadata(NULL, VD1_PATH,
									 1, false, false)) {
						amdv_set_toggle_flag(1);
						need_cp = true;
						pr_dv_dbg("vd1 off, reset cp\n");
						p_funcs_stb->multi_control_path
							(&invalid_m_dovi_setting);
					}
				}
			}
			if ((!vf_2 && video_turn_off[1]) ||
			    video_status[1] == -1) {
				if ((dolby_vision_flags & FLAG_TOGGLE_FRAME) ||
				    ((video_status[1] == -1) && dv_core1[1].core1_on)) {
					pr_dv_dbg("update when vd2 off\n");
					if (!amdv_parse_metadata(NULL, VD2_PATH,
									1, false, false)) {
						amdv_set_toggle_flag(1);
						need_cp = true;
						pr_dv_dbg("vd2 off, reset cp\n");
						p_funcs_stb->multi_control_path
							(&invalid_m_dovi_setting);
					}
				}
			}
			if (need_cp)
				amdv_control_path(vf, vf_2);
		}
	} else {
		/*No vd1 vf or vd1 turn off */
		if ((!vf && video_turn_off[0]) ||
		    (video_status[0] == -1)) {
			if (amdv_policy_process(vf, &mode, FORMAT_SDR)) {
				pr_dv_dbg("Fake SDR, mode->%d\n", mode);
				if (dolby_vision_policy == AMDV_FOLLOW_SOURCE &&
				    mode == AMDV_OUTPUT_MODE_BYPASS) {
					amdv_target_mode =
						AMDV_OUTPUT_MODE_BYPASS;
					dolby_vision_mode =
						AMDV_OUTPUT_MODE_BYPASS;
					amdv_set_toggle_flag(0);
					amdv_wait_on = false;
					dv_inst[dv_id].amdv_wait_init = false;
				} else {
					amdv_set_toggle_flag(1);
				}
			}
			need_cp = false;
			if ((dolby_vision_flags & FLAG_TOGGLE_FRAME) ||
			    ((video_status[0] == -1) && dv_core1[0].core1_on)) {
				if (debug_dolby & 8)
					pr_dv_dbg("update when vd1 off\n");
				if (!amdv_parse_metadata(NULL, VD1_PATH,
							 1, false, false)) {
					amdv_set_toggle_flag(1);
					need_cp = true;
				}
			}
			if (need_cp) {
				pr_dv_dbg("vd1 off, reset cp\n");
				p_funcs_stb->multi_control_path(&invalid_m_dovi_setting);
				amdv_control_path(vf, vf_2);
			}
		}
	}

	if (dolby_vision_mode == AMDV_OUTPUT_MODE_BYPASS) {
		if (vinfo && vinfo->vout_device &&
			!vinfo->vout_device->dv_info &&
			vsync_count < FLAG_VSYNC_CNT) {
			vsync_count++;
			update_amdv_core2_reg();
			return 0;
		}
		if (dolby_vision_status != BYPASS_PROCESS) {
			if (vinfo) {
				if (vf && is_primesl_frame(vf)) {
					/* disable dv immediately */
					pr_dv_dbg("DV bypass: PRIMESL: Switched to SDR first\n");
					send_hdmi_pkt(FORMAT_PRIMESL,
						      FORMAT_SDR, vinfo, vf);
					enable_amdv(0);
				} else if (vf && is_hdr10plus_frame(vf)) {
					/* disable dv immediately */
					pr_dv_dbg("DV bypass: HDR10+: Switched to SDR first\n");
					send_hdmi_pkt(FORMAT_HDR10PLUS,
						      FORMAT_SDR, vinfo, vf);
					enable_amdv(0);
				} else if (vf && is_hlg_frame(vf)) {
					/* disable dv immediately */
					pr_dv_dbg("DV bypass: HLG: Switched to SDR first\n");
					send_hdmi_pkt(FORMAT_HLG,
						      FORMAT_SDR, vinfo, vf);
					enable_amdv(0);
				} else if (vf && is_cuva_frame(vf)) {
					/* disable dolby immediately */
					pr_dv_dbg("Dolby bypass: cuva: Switched to SDR first\n");
					send_hdmi_pkt(FORMAT_CUVA,
						      FORMAT_SDR, vinfo, vf);
					enable_amdv(0);
				} else if (last_dst_format != FORMAT_DOVI) {
					/* disable dv immediately:
					 * non-dv always hdr to adaptive
					 */
					pr_dv_dbg("DV bypass: Switched %d to SDR\n",
						     last_dst_format);
					if (m_dovi_setting.input[pri_input].src_format ==
						FORMAT_INVALID)
						cur_src_format = FORMAT_SDR;
					else
						cur_src_format =
						m_dovi_setting.input[pri_input].src_format;
					send_hdmi_pkt(cur_src_format,
						      FORMAT_SDR, vinfo, vf);
					enable_amdv(0);
				} else {
					/* disable dv after sdr delay:
					 * dv always hdr to adaptive or dv
					 * playback exit in adaptive mode on
					 * a dv tv
					 */
					if (sdr_delay == 0) {
						pr_dv_dbg("DV bypass: Start - Switched to SDR\n");
						amdv_set_toggle_flag(1);
					}

					in_size =
					(m_dovi_setting.input[0].video_width << 16) |
					(m_dovi_setting.input[0].video_height);
					in_size_2 =
					(m_dovi_setting.input[1].video_width << 16) |
					(m_dovi_setting.input[1].video_height);

					if ((get_video_mute() ==
					VIDEO_MUTE_ON_DV &&
					!(dolby_vision_flags & FLAG_MUTE)) ||
					(get_video_mute() == VIDEO_MUTE_OFF &&
					dolby_vision_flags & FLAG_MUTE)) {
						/* core 3 only */
						apply_stb_core_settings
						(dma_paddr, 0, 0,
						amdv_mask & 0x4,
						false, 0, in_size, in_size_2,
						pps_state);
					}
					if (m_dovi_setting.input[pri_input].src_format ==
						FORMAT_INVALID)
						cur_src_format = FORMAT_SDR;
					else
						cur_src_format =
						m_dovi_setting.input[pri_input].src_format;
					send_hdmi_pkt(cur_src_format,
						      FORMAT_SDR, vinfo, vf);
					if (sdr_delay >= MAX_TRANSITION_DELAY) {
						pr_dv_dbg("DV bypass: Done - Switched to SDR\n");
						enable_amdv(0);
						sdr_delay = 0;
					} else {
						sdr_delay++;
					}
				}
				/* if tv ko loaded for tm2/t7 stb*/
				/* switch to tv vore */
				if ((is_aml_tm2_stbmode() || is_aml_t7_stbmode()) &&
					p_funcs_tv && !p_funcs_stb &&
					!is_amdv_on()) {
					switch_to_tv_mode();
				}
			} else {
				enable_amdv(0);
			}
		} else {
			if (debug_dolby & 0x2000)
				pr_dv_dbg("[inst%d]%p,flags 0x%x, last_dst_format %d\n",
				dv_id + 1, vf,
				dolby_vision_flags, last_dst_format);
			/* disable dv after send dv pkt ahead:
			 * one DV frame is inserted between the sdr frames in adaptive mode,
			 * send dv pkt ahead once but dv not on, status is still bypass.
			 * need to send sdr pkt after that.
			 */
			if (vinfo && last_dst_format == FORMAT_DOVI &&
				dolby_vision_policy == AMDV_FOLLOW_SOURCE) {
				if (sdr_delay == 0) {
					pr_dv_dbg("DV bypass: Start - Switched to SDR\n");
					amdv_set_toggle_flag(1);
				}
				if (m_dovi_setting.input[pri_input].src_format ==
					FORMAT_INVALID)
					cur_src_format = FORMAT_SDR;
				else
					cur_src_format =
					m_dovi_setting.input[pri_input].src_format;
				send_hdmi_pkt(cur_src_format,
						  FORMAT_SDR, vinfo, vf);
				if (sdr_delay >= MAX_TRANSITION_DELAY) {
					pr_dv_dbg("DV bypass: Done - Switched to SDR\n");
					enable_amdv(0);
					sdr_delay = 0;
				} else {
					sdr_delay++;
				}
			}
		}
		if (sdr_delay == 0)
			dolby_vision_flags &= ~FLAG_TOGGLE_FRAME;
		update_amdv_core2_reg();
		return 0;
	} else if (sdr_delay != 0) {
		/* in case mode change to a mode requiring dolby block */
		sdr_delay = 0;
	}

	if ((dolby_vision_flags & FLAG_CERTIFICATION) ||
	    (dolby_vision_flags & FLAG_BYPASS_VPP))
		video_effect_bypass(1);

	if (!p_funcs_stb && !p_funcs_tv) {
		dolby_vision_flags &= ~FLAG_TOGGLE_FRAME;
		tv_dovi_setting_change_flag = false;
		for (i = 0; i < NUM_IPCORE1; i++) {
			new_m_dovi_setting.input[i].video_width = 0;
			new_m_dovi_setting.input[i].video_height = 0;
			dv_core1[i].core1_disp_hsize = 0;
			dv_core1[i].core1_disp_vsize = 0;
		}
		update_amdv_core2_reg();
		return 0;
	}

	if (debug_dolby & 8)
		pr_dv_dbg("dolby_vision_on %d,flag %x,amdv_core2_on_cnt %d,%d\n",
			dolby_vision_on, dolby_vision_flags,
			amdv_core2_on_cnt, dv_core1[0].core1_on_cnt);

	if (!(dolby_vision_flags & FLAG_CERTIFICATION)) {
		for (i = 0; i < NUM_IPCORE1; i++) {
			if (dv_core1[i].core1_on &&
			    dv_core1[i].core1_on_cnt < DV_CORE1_RECONFIG_CNT &&
			    !(dolby_vision_flags & FLAG_TOGGLE_FRAME)) {
				amdv_set_toggle_flag(1);
				pr_dv_dbg
				("Need update core1_%s setting first %d times, force toggle\n",
				 i == 0 ? "a" : "b",
				 dv_core1[i].core1_on_cnt);
			}
		}
	}
	if (dolby_vision_on && !dv_core1[0].core1_on &&
	    !dv_core1[1].core1_on &&
	    amdv_core2_on_cnt &&
	    amdv_core2_on_cnt < DV_CORE2_RECONFIG_CNT &&
	    !(dolby_vision_flags & FLAG_TOGGLE_FRAME) &&
	    !is_aml_tvmode() &&
	    !(dolby_vision_flags & FLAG_CERTIFICATION)) {
		force_set_lut = true;
		amdv_set_toggle_flag(1);
		if (debug_dolby & 2)
			pr_dv_dbg("Need update core2 first %d times\n",
				     amdv_core2_on_cnt);
	}

	if (dolby_vision_flags & FLAG_TOGGLE_FRAME) {
		if (!(dolby_vision_flags & FLAG_CERTIFICATION)) {
			for (i = 0; i < NUM_IPCORE1; i++) {
				reset_flag[i] =
					(amdv_reset & 1) &&
					!dv_core1[i].core1_on &&
					(dv_core1[i].run_mode_count == 0);
			}
		}
		if (((dv_core1[0].core1_disp_hsize & 0xffff) &&
		    (dv_core1[0].core1_disp_vsize & 0xffff)) ||
		    ((dv_core1[1].core1_disp_hsize & 0xffff) &&
		    (dv_core1[1].core1_disp_vsize & 0xffff)) ||
		    force_set_lut) {
			if (dv_core1[0].core1_disp_hsize == 0xffff)
				dv_core1[0].core1_disp_hsize = 0;
			if (dv_core1[0].core1_disp_vsize == 0xffff)
				dv_core1[0].core1_disp_vsize = 0;
			if (dv_core1[1].core1_disp_hsize == 0xffff)
				dv_core1[1].core1_disp_hsize = 0;
			if (dv_core1[1].core1_disp_vsize == 0xffff)
				dv_core1[1].core1_disp_vsize = 0;

			if (is_aml_stb_hdmimode() && !core1_detunnel() &&
				layerid_valid(hdmi_path_id) && dv_inst_valid(hdmi_inst_id)) {
				core_mask = 0x6;
				tv_dv_core1_set
				(tv_dovi_setting->core1_reg_lut,
				 dma_paddr,
				 dv_core1[hdmi_path_id].core1_disp_hsize,
				 dv_core1[hdmi_path_id].core1_disp_vsize,
				 dv_core1[hdmi_path_id].amdv_setting_video_flag,
				 false, false, 2, true, false,
				 reset_flag[hdmi_path_id]);
			}
			if (force_set &&
				!(dolby_vision_flags
				& FLAG_CERTIFICATION)) {
				for (i = 0; i < NUM_IPCORE1; i++)
					reset_flag[i] = true;
			}
			in_size = (dv_core1[0].core1_disp_hsize << 16) |
					(dv_core1[0].core1_disp_vsize);
			in_size_2 = (dv_core1[1].core1_disp_hsize << 16) |
					(dv_core1[1].core1_disp_vsize);
			apply_stb_core_settings
				(dma_paddr,
				 dv_core1[0].amdv_setting_video_flag,
				 dv_core1[1].amdv_setting_video_flag,
				 amdv_mask & core_mask,
				 reset_flag[0],
				 reset_flag[1],
				 in_size,
				 in_size_2,
				 pps_state);
			memcpy(&m_dovi_setting, &new_m_dovi_setting,
				sizeof(m_dovi_setting));
#ifdef REMOVE_OLD_DV_FUNC
			for (i = 0; i < NUM_IPCORE1; i++) {
				new_m_dovi_setting.input[i].video_width =
				new_m_dovi_setting.input[i].video_height = 0;
				if (!m_dovi_setting.input[i].video_width ||
				    !m_dovi_setting.input[i].video_height) {
					if (dv_core1[dv_inst[i].layer_id].amdv_setting_video_flag)
						pr_dv_dbg("[inst%d]update core1_%s video flag to false\n",
							     i + 1,
							     (dv_inst[i].layer_id ==
							     VD1_PATH ? "a" : "b"));
					dv_core1[dv_inst[i].layer_id].amdv_setting_video_flag =
									false;
				}
			}
#endif
			for (i = 0; i < NUM_IPCORE1; i++) {
				if (dv_core1[i].amdv_setting_video_flag &&
				    dv_core1[i].run_mode_count == 0) {
					if (i == 0 || support_multi_core1())
						pr_dv_dbg("first frame reset core1_%s %s\n",
							     i == 0 ? "a" : "b",
							     reset_flag[i] ? "true" : "false");
				}
			}

			/* clr hdr+ pkt when enable dv */
			if (!dolby_vision_on &&
			    vinfo && vinfo->vout_device &&
			    vinfo->vout_device->fresh_tx_hdr10plus_pkt)
				vinfo->vout_device->fresh_tx_hdr10plus_pkt
				(0, NULL);

			enable_amdv(1);
			bypass_pps_path(pps_state);

			if (m_dovi_setting.input[pri_input].src_format == FORMAT_INVALID)
				cur_src_format = FORMAT_SDR;
			else
				cur_src_format = m_dovi_setting.input[pri_input].src_format;
			/* send HDMI packet according to dst_format */
			if (vinfo)
				send_hdmi_pkt
					(cur_src_format,
					 g_dst_format,
					 vinfo, vf);
			update_amdv_status
				(cur_src_format);
		} else {
			if ((get_video_mute() == VIDEO_MUTE_ON_DV &&
			     !(dolby_vision_flags & FLAG_MUTE)) ||
			    (get_video_mute() == VIDEO_MUTE_OFF &&
			     (dolby_vision_flags & FLAG_MUTE)) ||
			     last_dolby_vision_ll_policy !=
			     dolby_vision_ll_policy)
				/* core 3 only */
				apply_stb_core_settings
				(dma_paddr,
				 false,
				 false,
				 amdv_mask & 0x4,
				 reset_flag[0],
				 reset_flag[1],
				 0, 0,
				 pps_state);
			/* force send hdmi pkt */
			if ((dolby_vision_flags & FLAG_FORCE_HDMI_PKT) && vinfo) {
				if (m_dovi_setting.input[pri_input].src_format == FORMAT_INVALID)
					cur_src_format = FORMAT_SDR;
				else
					cur_src_format = m_dovi_setting.input[pri_input].src_format;
				send_hdmi_pkt
					(cur_src_format,
					 g_dst_format,
					 vinfo, vf);
			}
		}
		dolby_vision_flags &= ~FLAG_TOGGLE_FRAME;
	} else if ((dv_core1[0].core1_on ||
		   dv_core1[1].core1_on) &&
		!(dolby_vision_flags & FLAG_CERTIFICATION)) {
		if (is_amdv_stb_mode()) {
			if ((dv_core1[0].core1_on &&
			    dv_core1[0].run_mode_count <= amdv_run_mode_delay) ||
			    (dv_core1[1].core1_on &&
			    dv_core1[1].run_mode_count <= amdv_run_mode_delay) ||
			    force_set) {
				if (force_set) {
					for (i = 0; i < NUM_IPCORE1; i++)
						reset_flag[i] = true;
				}
				if (is_aml_stb_hdmimode() && !core1_detunnel() &&
				    layerid_valid(hdmi_path_id) && dv_inst_valid(hdmi_inst_id))
					tv_dv_core1_set
					(tv_dovi_setting->core1_reg_lut,
					  dma_paddr,
					  dv_core1[hdmi_path_id].core1_disp_hsize,
					  dv_core1[hdmi_path_id].core1_disp_vsize,
					  dv_core1[hdmi_path_id].amdv_setting_video_flag,
					  false,
					  false,
					  2,
					  true,
					  false,
					  reset_flag[hdmi_path_id]
					);
				else
					apply_stb_core_settings
					(dma_paddr,
					 true, /* core1a always enable */
					 dv_core1[1].amdv_setting_video_flag,
					 /* core 1 only */
					 amdv_mask & 0x1,
					 reset_flag[0],
					 reset_flag[1],
					 (dv_core1[0].core1_disp_hsize << 16) |
					 dv_core1[0].core1_disp_vsize,
					 (dv_core1[1].core1_disp_hsize << 16) |
					 dv_core1[1].core1_disp_vsize,
					 pps_state);

				for (i = 0; i < NUM_IPCORE1; i++) {
					if (dv_core1[i].run_mode_count <
					    amdv_run_mode_delay)
						pr_dv_dbg("fake frame (%d %d) %d reset %d\n",
							     dv_core1[i].core1_disp_hsize,
							     dv_core1[i].core1_disp_vsize,
							     dv_core1[i].run_mode_count,
							     reset_flag[i]);
				}
			}
		}
	}
	for (i = 0; i < NUM_IPCORE1; i++) {
		if (dv_core1[i].core1_on) {
			if (dv_core1[i].run_mode_count <=
				amdv_run_mode_delay + 1)
				dv_core1[i].run_mode_count++;
			if (debug_dolby & 8)
				pr_dv_dbg("%s: core1_%d run_mode_count %d\n",
					     __func__, i + 1, dv_core1[i].run_mode_count);
		} else {
			dv_core1[i].run_mode_count = 0;
		}
	}
	update_amdv_core2_reg();
	return 0;
}

/* toggle mode: 0: not toggle; 1: toggle frame; 2: use keep frame */
/* pps_state 0: no change, 1: pps enable, 2: pps disable */
int amdolby_vision_process(struct vframe_s *vf, u32 display_size,
				struct vframe_s *vf_2, u32 display_size_2,
				u8 toggle_mode_1, u8 toggle_mode_2, u8 pps_state)
{
	u8 toggle_mode;

	if (!is_amdv_stb_mode() && !is_aml_tvmode())
		return -1;

	if (!module_installed)
		return -1;

	/* vd1 toggle_mode priority is high than vd2*/
	toggle_mode = toggle_mode_1 ? toggle_mode_1 : toggle_mode_2;

	if (dolby_vision_enable == 1 && tv_mode == 1)
		amdolby_vision_wakeup_queue();

	if (multi_dv_mode) {
		if (is_aml_tvmode())
			amdolby_vision_process_v1(vf, display_size, toggle_mode, pps_state);
		else
			amdolby_vision_process_v2_stb(vf, display_size, vf_2,
						    display_size_2, toggle_mode_1,
						    toggle_mode_2, pps_state);
	} else {
		amdolby_vision_process_v1(vf, display_size, toggle_mode, pps_state);
	}
	return 0;
}
EXPORT_SYMBOL(amdolby_vision_process);

/* when dv on in uboot, other module cannot get dv status
 * in time through dolby_vision_on due to dolby_vision_on
 * is set in register_dv_functions
 * Add amdv_on_in_uboot condition for this case.
 */
bool is_amdv_on(void)
{
	return dolby_vision_on ||
		amdv_wait_on ||
		amdv_on_in_uboot;
}
EXPORT_SYMBOL(is_amdv_on);

bool is_amdv_video_on(void)
{
	if (multi_dv_mode)
		return (dv_core1[0].core1_on ||
			dv_core1[1].core1_on);
	else
		return amdv_core1_on;
}
EXPORT_SYMBOL(is_amdv_video_on);

bool is_amdv_graphic_on(void)
{
	/* TODO: check (core2_sel & 2) for core2c */
	return is_amdv_on() && !tv_mode &&
		(amdv_mask & 2) && (core2_sel & 1);
}
EXPORT_SYMBOL(is_amdv_graphic_on);

bool for_amdv_certification(void)
{
	return is_amdv_on() &&
		dolby_vision_flags & FLAG_CERTIFICATION;
}
EXPORT_SYMBOL(for_amdv_certification);

bool for_amdv_video_effect(void)
{
	return is_amdv_on() &&
		dolby_vision_flags & FLAG_BYPASS_VPP;
}
EXPORT_SYMBOL(for_amdv_video_effect);

void amdv_set_toggle_flag(int flag)
{
	if (flag) {
		dolby_vision_flags |= FLAG_TOGGLE_FRAME;
		if (flag & 2)
			dolby_vision_flags |= FLAG_FORCE_HDMI_PKT;
	} else {
		dolby_vision_flags &= ~FLAG_TOGGLE_FRAME;
	}
}
EXPORT_SYMBOL(amdv_set_toggle_flag);

bool is_dv_control_backlight(void)
{
	return dv_control_backlight_status;
}
EXPORT_SYMBOL(is_dv_control_backlight);

void set_dv_control_backlight_status(bool flag)
{
	dv_control_backlight_status = flag;
}

void set_amdv_mode(int mode)
{
	if ((is_amdv_stb_mode() || is_aml_tvmode()) &&
	    dolby_vision_enable &&
	    dolby_vision_request_mode == 0xff) {
		if (amdv_policy_process
			(NULL, &mode, get_cur_src_format())) {
			amdv_set_toggle_flag(1);
			if (mode != AMDV_OUTPUT_MODE_BYPASS &&
			    dolby_vision_mode ==
			    AMDV_OUTPUT_MODE_BYPASS)
				amdv_wait_on = true;
			pr_info("DOVI output change from %d to %d\n",
				dolby_vision_mode, mode);
			amdv_target_mode = mode;
			dolby_vision_mode = mode;
		}
	}
}
EXPORT_SYMBOL(set_amdv_mode);

int get_amdv_mode(void)
{
	return dolby_vision_mode;
}
EXPORT_SYMBOL(get_amdv_mode);

int get_amdv_target_mode(void)
{
	return amdv_target_mode;
}
EXPORT_SYMBOL(get_amdv_target_mode);

bool is_amdv_enable(void)
{
	return dolby_vision_enable;
}
EXPORT_SYMBOL(is_amdv_enable);

bool is_hdmi_ll_as_hdr10(void)
{
	return hdmi_source_led_as_hdr10;
}
EXPORT_SYMBOL(is_hdmi_ll_as_hdr10);

bool is_multi_dv_mode(void)
{
	return multi_dv_mode;
}
EXPORT_SYMBOL(is_multi_dv_mode);

bool support_multi_core1(void)
{
	if (multi_dv_mode && enable_multi_core1 &&
	    (is_aml_tm2_stbmode() || is_aml_t7_stbmode() || is_aml_s5()))
		return true;
	else
		return false;
}
EXPORT_SYMBOL(support_multi_core1);

bool is_amdv_el_disable(void)
{
	return amdv_el_disable;
}
EXPORT_SYMBOL(is_amdv_el_disable);

void set_amdv_ll_policy(int policy)
{
	dolby_vision_ll_policy = policy;
}
EXPORT_SYMBOL(set_amdv_ll_policy);

void set_amdv_policy(int policy)
{
	dolby_vision_policy = policy;
}
EXPORT_SYMBOL(set_amdv_policy);

int get_amdv_policy(void)
{
	return dolby_vision_policy;
}
EXPORT_SYMBOL(get_amdv_policy);

void set_amdv_enable(bool enable)
{
	dolby_vision_enable = enable;
}
EXPORT_SYMBOL(set_amdv_enable);

/* bit 0 for HDR10: 1=by dv, 0-by vpp */
/* bit 1 for HLG: 1=by dv, 0-by vpp */
/* bit 5 for SDR: 1=by dv, 0-by vpp */
int get_amdv_hdr_policy(void)
{
	int ret = 0;

	if (!is_amdv_enable())
		return 0;
	if (dolby_vision_policy == AMDV_FOLLOW_SOURCE) {
		/* policy == FOLLOW_SRC, check hdr/hlg policy */
		ret |= (dolby_vision_hdr10_policy & HDR_BY_DV_F_SRC) ? 1 : 0;
		ret |= (dolby_vision_hdr10_policy & HLG_BY_DV_F_SRC) ? 2 : 0;
		ret |= (dolby_vision_hdr10_policy & SDR_BY_DV_F_SRC) ? 0x40 : 0;
	} else if (dolby_vision_policy == AMDV_FOLLOW_SINK) {
		/* policy == FOLLOW_SINK, check hdr/hlg policy */
		ret |= (dolby_vision_hdr10_policy & HDR_BY_DV_F_SINK) ? 1 : 0;
		ret |= (dolby_vision_hdr10_policy & HLG_BY_DV_F_SINK) ? 2 : 0;
		ret |= (dolby_vision_hdr10_policy & SDR_BY_DV_F_SINK) ? 0x20 : 0;
	} else {
		/* policy == FORCE, check hdr/hlg policy */
		ret |= (dolby_vision_hdr10_policy & HDR_BY_DV_F_SRC) ? 1 : 0;
		ret |= (dolby_vision_hdr10_policy & HLG_BY_DV_F_SRC) ? 2 : 0;
		ret |= (dolby_vision_hdr10_policy & HDR_BY_DV_F_SINK) ? 1 : 0;
		ret |= (dolby_vision_hdr10_policy & HLG_BY_DV_F_SINK) ? 2 : 0;
		ret |= (dolby_vision_hdr10_policy & SDR_BY_DV_F_SINK) ? 0x20 : 0;
		ret |= (dolby_vision_hdr10_policy & SDR_BY_DV_F_SRC) ? 0x40 : 0;
	}
	return ret;
}
EXPORT_SYMBOL(get_amdv_hdr_policy);

void amdv_update_backlight(void)
{
#ifdef CONFIG_AMLOGIC_LCD
	u32 new_bl = 0;

	if (is_aml_tvmode()) {
		if (!force_disable_dv_backlight) {
			bl_delay_cnt++;
			if (tv_backlight_changed &&
			    set_backlight_delay_vsync == bl_delay_cnt) {
				new_bl = use_12b_bl ? tv_backlight << 4 :
					tv_backlight;
				pr_dv_dbg("dv set backlight %d\n", new_bl);
				aml_lcd_atomic_notifier_call_chain
					(LCD_EVENT_BACKLIGHT_GD_DIM,
					 &new_bl);
				tv_backlight_changed = false;
			}
		}
	}
#endif
}
EXPORT_SYMBOL(amdv_update_backlight);

void amdv_disable_backlight(void)
{
#ifdef CONFIG_AMLOGIC_LCD
	int gd_en = 0;

	pr_dv_dbg("disable dv backlight\n");

	if (is_aml_tvmode()) {
		aml_lcd_atomic_notifier_call_chain
		(LCD_EVENT_BACKLIGHT_GD_SEL, &gd_en);
		dv_control_backlight_status = false;
	}
#endif
}

static unsigned long __invoke_psci_fn_smc(unsigned long function_id,
			unsigned long arg0, unsigned long arg1,
			unsigned long arg2)
{
	struct arm_smccc_res res;

	arm_smccc_smc(function_id, arg0, arg1, arg2, 0, 0, 0, 0, &res);
	pr_info("arm_smccc_smc 0x%lx, get res.a0 0x%lx\n", function_id, res.a0);
	return res.a0;
}

void parse_param_amdv(char *buf_orig, char **parm)
{
	char *ps, *token;
	unsigned int n = 0;
	char delim1[3] = " ";
	char delim2[2] = "\n";

	ps = buf_orig;
	strcat(delim1, delim2);
	while (1) {
		token = strsep(&ps, delim1);
		if (!token)
			break;
		if (*token == '\0')
			continue;
		parm[n++] = token;
		if (n >= MAX_PARAM)
			break;
	}
}

static ssize_t amdolby_vision_use_inter_pq_show
		(struct class *cla,
		 struct class_attribute *attr,
		 char *buf)
{
	return snprintf(buf, 40, "%d\n",
		get_inter_pq_flag());
}

static ssize_t amdolby_vision_use_inter_pq_store
		(struct class *cla,
		 struct class_attribute *attr,
		 const char *buf, size_t count)

{
	size_t r;
	int flag;

	pr_info("cmd: %s\n", buf);
	r = kstrtoint(buf, 0, &flag);
	if (r != 0)
		return -EINVAL;
	set_inter_pq_flag(flag);
	return count;
}

static ssize_t amdolby_vision_load_cfg_status_show
		(struct class *cla,
		 struct class_attribute *attr,
		 char *buf)
{
	int value = 0;

	value = get_load_config_status();
	return snprintf(buf, 40, "%d\n", value);
}

static ssize_t amdolby_vision_load_cfg_status_store
		(struct class *cla,
		 struct class_attribute *attr,
		 const char *buf, size_t count)
{
	size_t r;
	int value = 0;

	pr_info("%s: cmd: %s\n", __func__, buf);
	r = kstrtoint(buf, 0, &value);
	if (r != 0)
		return -EINVAL;

	if (value) {
		set_load_config_status(true);
		update_cp_cfg();
	} else {
		set_load_config_status(false);
		pq_config_set_flag = false;
	}
	return count;
}

static ssize_t  amdolby_vision_pq_info_show
		(struct class *cla,
		 struct class_attribute *attr, char *buf)
{
	int pos = get_dv_pq_info(buf);
	return pos;
}

static ssize_t amdolby_vision_pq_info_store
		(struct class *cla,
		 struct class_attribute *attr,
		 const char *buf, size_t count)
{
	int ret = set_dv_pq_info(buf, count);
	return ret;
}

static ssize_t amdolby_vision_bin_config_show
	(struct class *cla,
	 struct class_attribute *attr,
	 char *buf)
{
	get_dv_bin_config();
	return 0;
}

static ssize_t amdolby_vision_core1_detunnel_show
	 (struct class *cla,
	  struct class_attribute *attr,
	  char *buf)
{
	return snprintf(buf, 40, "%d\n", dv_core1_detunnel);
}

static ssize_t amdolby_vision_core1_detunnel_store
	 (struct class *cla,
	  struct class_attribute *attr,
	  const char *buf, size_t count)
{
	size_t r;
	int value = 0;

	pr_info("%s: cmd: %s\n", __func__, buf);
	r = kstrtoint(buf, 0, &value);
	if (r != 0)
		return -EINVAL;
	dv_core1_detunnel = value;

	return count;
}

static int get_chip_name(void)
{
	char *check = "chip_name = ";
	int check_len = 0;
	int name_len = 0;
	int total_name_len;

	if (is_aml_g12())
		snprintf(chip_name, sizeof("g12"), "%s", "g12");
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
	else if (is_aml_txlx())
		snprintf(chip_name, sizeof("txlx"), "%s", "txlx");
	else if (is_aml_gxm())
		snprintf(chip_name, sizeof("gxm"), "%s", "gxm");
#endif
	else if (is_aml_tm2())
		snprintf(chip_name, sizeof("tm2"), "%s", "tm2");
	else if (is_aml_sc2())
		snprintf(chip_name, sizeof("sc2"), "%s", "sc2");
	else if (is_aml_t7())
		snprintf(chip_name, sizeof("t7"), "%s", "t7");
	else if (is_aml_t3())
		snprintf(chip_name, sizeof("t3"), "%s", "t3");
	else if (is_aml_s4d())
		snprintf(chip_name, sizeof("s4d"), "%s", "s4d");
	else if (is_aml_t5w())
		snprintf(chip_name, sizeof("t5w"), "%s", "t5w");
	else if (is_aml_s5())
		snprintf(chip_name, sizeof("s5"), "%s", "s5");
	else
		snprintf(chip_name, sizeof("null"), "%s", "null");

	check_len = strlen(check);
	name_len = strlen(chip_name);

	total_name_len =
		check_len + name_len + 1;

	if (total_name_len < sizeof(total_chip_name))
		snprintf(total_chip_name,
			total_name_len,
			"%s%s",
			check,
			chip_name);
	return total_name_len;
}

bool chip_support_dv(void)
{
	if (is_aml_txlx() || is_aml_gxm() ||
	    is_aml_g12() || is_aml_tm2() ||
	    is_aml_sc2() || is_aml_t7() ||
	    is_aml_t3() || is_aml_s4d() ||
	    is_aml_t5w() ||
	    is_aml_s5())
		return true;
	else
		return false;
}

int register_dv_functions(const struct dolby_vision_func_s *func)
{
	int ret = -1;
	unsigned int reg_clk;
	unsigned int reg_value;
	struct pq_config *pq_cfg;
	const struct vinfo_s *vinfo = get_current_vinfo();
	unsigned int ko_info_len = 0;
	int i;
	int total_name_len = 0;
	char *get_ko = NULL;

	if (dolby_vision_probe_ok == 0) {
		pr_info("error:(%s) dv probe fail cannot register\n", __func__);
		return -ENOMEM;
	}
	multi_dv_mode = false;
	/*when dv ko load into kernel, this flag will be disabled
	 *otherwise it will effect hdr module
	 */
	if (amdv_on_in_uboot) {
		if (is_vinfo_available(vinfo))
			is_sink_cap_changed(vinfo,
					    &current_hdr_cap,
					    &current_sink_available,
					    VPP_TOP0);
		else
			pr_info("sink not available\n");
		dolby_vision_on = true;
		amdv_wait_on = false;
		amdv_wait_init = false;
		amdv_on_in_uboot = 0;
		for (i = 0; i < NUM_INST; i++)
			dv_inst[i].amdv_wait_init = false;
	}

	if (!chip_support_dv()) {
		pr_info("chip not support dv\n");
		return ret;
	}
	if (is_aml_t7() || is_aml_t3() || is_aml_s4d() ||
	    is_aml_t5w() || is_aml_s5()) {
		total_name_len = get_chip_name();
		get_ko = strstr(func->version_info, total_chip_name);

		if (!get_ko) {
			pr_info("error: dolby vision get fail ko, version: %s", func->version_info);
			return ret;
		}
	}

	if ((!p_funcs_stb || !p_funcs_tv) && func) {
		if (func->control_path && !p_funcs_stb) {
			pr_info("*** register_dv_stb_functions.***\n");
			if (!ko_info) {
				ko_info_len = strlen(func->version_info);
				ko_info = vmalloc(ko_info_len + 1);
				if (ko_info) {
					strncpy(ko_info, func->version_info, ko_info_len);
					ko_info[ko_info_len] = '\0';
				}
			}
			p_funcs_stb = func;
			if (is_aml_tm2() || is_aml_t7() || is_aml_t3()) {
				tv_dovi_setting =
				vmalloc(sizeof(struct tv_dovi_setting_s));
				if (!tv_dovi_setting)
					return -ENOMEM;
			}
			hdmi_source_led_as_hdr10 = true;
			enable_multi_core1 = 0;
			dolby_vision_hdr10_policy |= SDR_BY_DV_F_SINK;
			dolby_vision_hdr10_policy |= HDR_BY_DV_F_SINK;
			last_dolby_vision_hdr10_policy = dolby_vision_hdr10_policy;
			if (ko_info)
				pr_info("hdr10_policy %d, ko_info %s\n",
					dolby_vision_hdr10_policy, ko_info);

		} else if (func->tv_control_path && !p_funcs_tv) {
			pr_info("*** register_dv_tv_functions\n");
			if (!ko_info) {
				ko_info_len = strlen(func->version_info);
				ko_info = vmalloc(ko_info_len + 1);
				if (ko_info) {
					strncpy(ko_info, func->version_info, ko_info_len);
					ko_info[ko_info_len] = '\0';
				}
			}
			p_funcs_tv = func;
			if (is_aml_tvmode()) {
				pq_cfg =
					vmalloc(sizeof(struct pq_config));
				if (!pq_cfg)
					return -ENOMEM;
				pq_config_fake =
					(struct pq_config *)pq_cfg;
				tv_dovi_setting =
				vmalloc(sizeof(struct tv_dovi_setting_s));
				if (!tv_dovi_setting) {
					vfree(pq_config_fake);
					pq_config_fake = NULL;
					p_funcs_tv = NULL;
					return -ENOMEM;
				}
				tv_dovi_setting->src_format = FORMAT_SDR;

				tv_input_info =
				vmalloc(sizeof(struct tv_input_info_s));
				if (!tv_input_info) {
					vfree(pq_config_fake);
					pq_config_fake = NULL;
					p_funcs_tv = NULL;
					vfree(tv_dovi_setting);
					tv_dovi_setting = NULL;
					return -ENOMEM;
				}
				memset(tv_input_info, 0,
				       sizeof(struct tv_input_info_s));
				if (enable_amvs12_for_tv) {
					dolby_vision_hdr10_policy |= HLG_BY_DV_F_SINK;
					dolby_vision_hdr10_policy |= HDR_BY_DV_F_SINK;
				}
				last_dolby_vision_hdr10_policy = dolby_vision_hdr10_policy;
				if (ko_info)
					pr_info("hdr10_policy %d, ko_info %s\n",
						dolby_vision_hdr10_policy, ko_info);

			}
		} else if (func->multi_control_path && !p_funcs_stb) {
			pr_info("*** register_dv_multi_stb_functions.***\n");

			if (!is_aml_tm2revb() && !is_aml_t7_stbmode() && !is_aml_s5() &&
				enable_multi_core1) {
				enable_multi_core1 = false;
				pr_info("*** only has one core1. please check***\n");
			}
			if (!ko_info) {
				ko_info_len = strlen(func->version_info);
				ko_info = vmalloc(ko_info_len + 1);
				if (ko_info) {
					strncpy(ko_info, func->version_info, ko_info_len);
					ko_info[ko_info_len] = '\0';
				} else {
					return -ENOMEM;
				}
			}
			if (!func->multi_mp_init || !func->multi_mp_release ||
			    !func->multi_mp_reset || !func->multi_mp_process) {
				pr_info("ko register error!\n");
				vfree(ko_info);
				ko_info = NULL;
				return -ENOMEM;
			}

			p_funcs_stb = func;

			/*two video + one osd in default*/
			new_m_dovi_setting.num_input = NUM_IPCORE1 + NUM_IPCORE2;
			new_m_dovi_setting.num_video = NUM_IPCORE1;
			new_m_dovi_setting.pri_input = pri_input;
			invalid_m_dovi_setting.num_input = 0;

			new_m_dovi_setting.output_ctrl_data = vmalloc(OUTPUT_CONTROL_DATA_SIZE);
			if (!new_m_dovi_setting.output_ctrl_data) {
				vfree(ko_info);
				ko_info = NULL;
				pr_info("output_ctrl_data malloc error\n");
				return -ENOMEM;
			}
			/*initialize parser for two inst in advance to prevent some cases*/
			/*where the decoder does not call the map and dv can also work normally.*/
			/*Usually parser will be automatically created when play and map*/
			for (i = 0; i < 2; i++) {
				dv_inst[i].metadata_parser =
				p_funcs_stb->multi_mp_init(dolby_vision_flags
							   & FLAG_CHANGE_SEQ_HEAD
							   ? 1 : 0);
				p_funcs_stb->multi_mp_reset
					(dv_inst[i].metadata_parser, 1);
			}

			for (i = 0; i < new_m_dovi_setting.num_input; i++) {
				invalid_m_dovi_setting.input[i].src_format = FORMAT_INVALID;
				new_m_dovi_setting.input[i].src_format = FORMAT_INVALID;
				m_dovi_setting.input[i].src_format = FORMAT_INVALID;
			}

			if (is_aml_tm2() || is_aml_t7()) {
				tv_dovi_setting =
				vmalloc(sizeof(struct tv_dovi_setting_s));
				if (!tv_dovi_setting) {
					vfree(ko_info);
					ko_info = NULL;
					pr_info("tv setting malloc error\n");
					return -ENOMEM;
				}
			}

			multi_dv_mode = true;
			hdmi_source_led_as_hdr10 = false; /*not treat hdmi in LL as hdr10*/
			dolby_vision_hdr10_policy |= HLG_BY_DV_F_SINK;
			dolby_vision_hdr10_policy |= HDR_BY_DV_F_SINK;
			dolby_vision_hdr10_policy |= SDR_BY_DV_F_SINK;
			last_dolby_vision_hdr10_policy = dolby_vision_hdr10_policy;
			dolby_vision_flags |= FLAG_RX_EMP_VSEM;
			pr_info("enable DV HLG when stb v2.6. policy %d\n",
				dolby_vision_hdr10_policy);
		} else {
			return ret;
		}
		ret = 0;
		/* get efuse flag*/

		if (is_aml_txlx() || is_aml_tm2() || is_aml_t7() ||
		    is_aml_t3() || is_aml_t5w()) {
			reg_clk = READ_VPP_DV_REG(AMDV_TV_CLKGATE_CTRL);
			WRITE_VPP_DV_REG(AMDV_TV_CLKGATE_CTRL, 0x2800);
			reg_value = READ_VPP_DV_REG(AMDV_TV_REG_START + 1);
			if ((reg_value & 0x400) == 0)
				efuse_mode = 0;
			else
				efuse_mode = 1;
			WRITE_VPP_DV_REG(AMDV_TV_CLKGATE_CTRL, reg_clk);
		} else {
			reg_value = READ_VPP_DV_REG(AMDV_CORE1A_REG_START + 1);
			if ((reg_value & 0x100) == 0)
				efuse_mode = 0;
			else
				efuse_mode = 1;
		}
		pr_info("efuse_mode=%d reg_value = 0x%x\n",
			efuse_mode, reg_value);

		support_info = efuse_mode ? 0 : 1;/*bit0=1 => no efuse*/
		support_info = support_info | (1 << 1); /*bit1=1 => ko loaded*/
		support_info = support_info | (1 << 2); /*bit2=1 => updated*/
		if (tv_mode)
			support_info |= 1 << 3; /*bit3=1 => tv*/

		pr_info("dv capability %d\n", support_info);

		/*stb core doesn't need run mode*/
		/*TV core need run mode and the value is 2*/
		if (is_aml_txlx_stbmode() ||
		    is_aml_tm2_stbmode() || is_aml_t7_stbmode() ||
		    is_aml_sc2() || is_aml_s4d() || is_aml_s5())
			amdv_run_mode_delay = 0;
		else if (is_aml_g12())
			amdv_run_mode_delay = RUN_MODE_DELAY_G12;
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
		else if (is_aml_gxm())
			amdv_run_mode_delay = RUN_MODE_DELAY_GXM;
#endif
		else
			amdv_run_mode_delay = RUN_MODE_DELAY;

		adjust_vpotch(osd_graphic_width[OSD1_INDEX], osd_graphic_height[OSD1_INDEX]);
		adjust_vpotch_tv();

		if ((is_aml_tm2_stbmode() || is_aml_t7_stbmode()) &&
			p_funcs_tv && !p_funcs_stb) {
			/* if tv ko loaded and no stb ko */
			/* turn off stb core and force SDR output */
			set_force_output(BT709);
			if (is_amdv_on()) {
				pr_dv_dbg("Switch from STB to TV core ...\n");
				dolby_vision_mode =
					AMDV_OUTPUT_MODE_BYPASS;
			}
		}
	}
	if (is_aml_s5()) {
		malloc_md_pkt();
		core2_sel = 3;
		copy_core1a = 1;
	}
	module_installed = true;
	return ret;
}
EXPORT_SYMBOL(register_dv_functions);

int unregister_dv_functions(void)
{
	int ret = -1;
	int i;

	module_installed = false;
	if (p_funcs_stb || p_funcs_tv) {
		pr_info("*** %s ***\n", __func__);
		if (pq_config_fake) {
			vfree(pq_config_fake);
			pq_config_fake = NULL;
		}
		if (tv_dovi_setting) {
			vfree(tv_dovi_setting);
			tv_dovi_setting = NULL;
		}
		if (tv_input_info) {
			vfree(tv_input_info);
			tv_input_info = NULL;
		}
		if (bin_to_cfg) {
			vfree(bin_to_cfg);
			bin_to_cfg = NULL;
		}
		if (ko_info) {
			vfree(ko_info);
			ko_info = NULL;
		}

		if (multi_dv_mode) {
			for (i = 0; i < NUM_INST; i++) {
				if (dv_inst[i].metadata_parser && p_funcs_stb) {
					p_funcs_stb->multi_mp_release
						(&dv_inst[i].metadata_parser);
					dv_inst[i].metadata_parser = NULL;
				}
				if (dv_inst[i].mapped)
					dv_inst[i].mapped = false;
			}
			if (new_m_dovi_setting.output_ctrl_data) {
				vfree(new_m_dovi_setting.output_ctrl_data);
				new_m_dovi_setting.output_ctrl_data = NULL;
			}
		}
		p_funcs_stb = NULL;
		p_funcs_tv = NULL;
		ret = 0;
	}
	if (is_aml_s5())
		free_md_pkt();

	return ret;
}
EXPORT_SYMBOL(unregister_dv_functions);

void amdv_crc_clear(int flag)
{
	int i;

	crc_output_buff_off = 0;
	crc_count = 0;
	crc_bypass_count = 0;
	setting_update_count = 0;
	if (multi_dv_mode) {
		for (i = 0; i < NUM_INST; i++)
			dv_inst[i].frame_count = 0;
	}
	if (!crc_output_buf)
		crc_output_buf = vmalloc(CRC_BUFF_SIZE);
	pr_info("clear crc_output_buf\n");
	if (crc_output_buf)
		memset(crc_output_buf, 0, CRC_BUFF_SIZE);
	strcpy(cur_crc, "invalid");
}

char *amdv_get_crc(u32 *len)
{
	if (!crc_output_buf || !len || crc_output_buff_off == 0)
		return NULL;
	*len = crc_output_buff_off;
	return crc_output_buf;
}

void amdv_insert_crc(bool print)
{
	char str[64];
	int len;
	bool crc_enable;
	u32 crc;

	if (dolby_vision_flags & FLAG_DISABLE_CRC) {
		crc_bypass_count++;
		crc_count++;
		return;
	}
	if (is_aml_tvmode()) {
		crc_enable = (READ_VPP_DV_REG(AMDV_TV_DIAG_CTRL) == 0xb);
		crc = READ_VPP_DV_REG(AMDV_TV_OUTPUT_DM_CRC);
	} else {
		crc_enable = true;/*(READ_VPP_DV_REG(AMDV_CORE3_CRC_CTRL) & 1);*/
		crc = READ_VPP_DV_REG(AMDV_CORE3_OUTPUT_CSC_CRC);
	}
	if (crc == 0 || !crc_enable || !crc_output_buf) {
		crc_bypass_count++;
		crc_count++;
		return;
	}
	if (crc_count < crc_bypass_count)
		crc_bypass_count = crc_count;
	memset(str, 0, sizeof(str));
	snprintf(str, 64, "crc(%d) = 0x%08x",
		crc_count - crc_bypass_count, crc);
	len = strlen(str);
	str[len] = 0xa;
	len++;
	if (crc_output_buff_off + len < CRC_BUFF_SIZE)
		memcpy(&crc_output_buf[crc_output_buff_off], &str[0], len);

	crc_output_buff_off += len;
	if (print || (debug_dolby & 2))
		pr_info("%s\n", str);
	crc_count++;

	if ((debug_dolby & 0x10000) && is_aml_tvmode()) {
		pr_info("tvcore crc 0x%x, diag ctrl 0x%x\n",
			READ_VPP_DV_REG(AMDV_TV_OUTPUT_DM_CRC),
			READ_VPP_DV_REG(AMDV_TV_DIAG_CTRL));
	} else if ((debug_dolby & 0x10000) && is_amdv_stb_mode()) {
		pr_info("core1 bl crc 0x%x,dm 0x%x,core3 in 0x%x,out 0x%x,enable %d,off %d\n",
			READ_VPP_DV_REG(AMDV_CORE1_BL_CRC),
			READ_VPP_DV_REG(AMDV_CORE1_CSC_OUTPUT_CRC),
			READ_VPP_DV_REG(AMDV_CORE3_INPUT_CSC_CRC),
			READ_VPP_DV_REG(AMDV_CORE3_OUTPUT_CSC_CRC),
			READ_VPP_DV_REG(AMDV_CORE3_CRC_CTRL),
			crc_output_buff_off);
	}

	snprintf(cur_crc, sizeof(cur_crc), "0x%08x", crc);
}

void tv_amdv_dma_table_modify(u32 tbl_id, u64 value)
{
	u64 *tbl = NULL;

	if (!dma_vaddr || tbl_id >= 3754) {
		pr_info("No dma table %p to write or id %d overflow\n",
			dma_vaddr, tbl_id);
		return;
	}
	tbl = (u64 *)dma_vaddr;
	pr_info("dma_vaddr:%p, modify table[%d]=0x%llx -> 0x%llx\n",
		dma_vaddr, tbl_id, tbl[tbl_id], value);
	tbl[tbl_id] = value;
}

void tv_amdv_efuse_info(void)
{
	if (p_funcs_tv) {
		pr_info("\n dv efuse info:\n");
		pr_info("efuse_mode:%d, version: %s\n",
			efuse_mode, p_funcs_tv->version_info);
	} else {
		pr_info("\n p_funcs is NULL\n");
		pr_info("efuse_mode:%d\n", efuse_mode);
	}
}

void tv_amdv_el_info(void)
{
	if (multi_dv_mode) {
		pr_info("inst0: el_flag: %d\n", new_m_dovi_setting.input[0].el_flag);
		pr_info("inst1: el_flag: %d\n", new_m_dovi_setting.input[1].el_flag);
	} else {
		pr_info("el_flag:%d\n", new_dovi_setting.el_flag);
	}
}

static int amdolby_vision_open(struct inode *inode, struct file *file)
{
	struct amdolby_vision_dev_s *devp;
	/* Get the per-device structure that contains this cdev */
	devp = container_of(inode->i_cdev, struct amdolby_vision_dev_s, cdev);
	file->private_data = devp;
	return 0;
}

static char *pq_config_buf;
static u32 pq_config_level;
static ssize_t amdolby_vision_write
	(struct file *file,
	const char __user *buf,
	size_t len,
	loff_t *off)
{
	int max_len, w_len;

	if (!pq_config_buf) {
		pq_config_buf = vmalloc(108 * 1024);
		pq_config_level = 0;
		if (!pq_config_buf)
			return -ENOSPC;
	}
	max_len = sizeof(struct pq_config) - pq_config_level;
	w_len = len < max_len ? len : max_len;

	pr_info("write len %d, w_len %d, level %d\n",
		(int)len, w_len, pq_config_level);
	if (copy_from_user(pq_config_buf + pq_config_level, buf, w_len))
		return -EFAULT;

	pq_config_level += w_len;
	if (pq_config_level == sizeof(struct pq_config)) {
		amdv_update_pq_config(pq_config_buf);
		pq_config_level = 0;
	}

	if (len <= 0x1f) {
		amdv_update_vsvdb_config(pq_config_buf, len);
		pq_config_level = 0;
	}
	return len;
}

static ssize_t amdolby_vision_read
	(struct file *file, char __user *buf,
	size_t count, loff_t *ppos)
{
	char *out;
	u32 data_size = 0, res, ret_val = -1;

	if (!is_amdv_enable())
		return ret_val;
	out = amdv_get_crc(&data_size);
	if (data_size > CRC_BUFF_SIZE) {
		pr_err("crc_output_buff_off is out of bound\n");
		amdv_crc_clear(0);
		return ret_val;
	}

	if (out && data_size > 0) {
		res = copy_to_user((void *)buf,
			(void *)out,
			data_size);
		ret_val = data_size - res;
		pr_info
			("%s crc size %d, res: %d, ret: %d\n",
			__func__, data_size, res, ret_val);
		amdv_crc_clear(0);
	}
	return ret_val;
}

static int amdolby_vision_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}

static long amdolby_vision_ioctl(struct file *file,
		unsigned int cmd, unsigned long arg)
{
#define MAX_BYTES (128)
	int ret = 0;
	int mode_num = 0;
	int mode_id = 0;
	s16 pq_value = 0;
	enum pq_item_e pq_item;
	enum pq_reset_e pq_reset;

	struct pic_mode_info_s pic_info;
	struct dv_pq_info_s pq_info;
	struct dv_full_pq_info_s pq_full_info;
	struct dv_config_file_s config_file;
	void __user *argp = (void __user *)arg;
	unsigned char bin_name[MAX_BYTES] = "";
	unsigned char cfg_name[MAX_BYTES] = "";
	int dark_detail = 0;

	if (debug_dolby & 0x200)
		pr_info("[DV]: %s: cmd_nr = 0x%x\n",
			__func__, _IOC_NR(cmd));

	if (!module_installed) {
		pr_info("[DV] module not install\n");
		return ret;
	}

	if (!get_load_config_status() && cmd != DV_IOC_SET_DV_CONFIG_FILE) {
		pr_info("[DV] no config file, pq ioctl disable!\n");
		return ret;
	}
	switch (cmd) {
	case DV_IOC_GET_DV_PIC_MODE_NUM:
		mode_num = get_pic_mode_num();
		put_user(mode_num, (u32 __user *)argp);
		break;
	case DV_IOC_GET_DV_PIC_MODE_NAME:
		if (copy_from_user(&pic_info, argp,
				   sizeof(struct pic_mode_info_s)) == 0) {
			mode_id = pic_info.pic_mode_id;
			strcpy(pic_info.name, get_pic_mode_name(mode_id));
			if (debug_dolby & 0x200)
				pr_info("[DV]: get mode %d, name %s\n",
					pic_info.pic_mode_id, pic_info.name);
			if (copy_to_user(argp,
					 &pic_info,
					 sizeof(struct pic_mode_info_s)))
				ret = -EFAULT;
		} else {
			ret = -EFAULT;
		}
		break;
	case DV_IOC_GET_DV_PIC_MODE_ID:
		mode_id = get_pic_mode();
		put_user(mode_id, (u32 __user *)argp);
		break;
	case DV_IOC_SET_DV_PIC_MODE_ID:
		if (copy_from_user(&mode_id, argp, sizeof(s32)) == 0) {
			if (debug_dolby & 0x200)
				pr_info("[DV]: set mode %d\n", mode_id);
			set_pic_mode(mode_id);
		} else {
			ret = -EFAULT;
		}
		break;
	case DV_IOC_GET_DV_SINGLE_PQ_VALUE:
		/*coverity[tainted_argument] copy same type will not pollute data.*/
		if (copy_from_user(&pq_info, argp,
				   sizeof(struct dv_pq_info_s)) == 0) {
			mode_id = pq_info.pic_mode_id;
			pq_item = pq_info.item;
			pq_info.value = get_single_pq_value(mode_id, pq_item);

			if (debug_dolby & 0x200)
				pr_info("[DV]: get mode %d, pq %s, value %d\n",
					mode_id,
					pq_item_str[pq_item],
					pq_info.value);

			if (copy_to_user(argp,
					 &pq_info,
					 sizeof(struct dv_pq_info_s)))
				ret = -EFAULT;
		} else {
			ret = -EFAULT;
		}
		break;
	case DV_IOC_GET_DV_FULL_PQ_VALUE:
		if (copy_from_user(&pq_full_info, argp,
				   sizeof(struct dv_full_pq_info_s)) == 0) {
			mode_id = pq_full_info.pic_mode_id;
			pq_full_info = get_full_pq_value(mode_id);

			if (copy_to_user(argp,
					 &pq_full_info,
					 sizeof(struct dv_full_pq_info_s)))
				ret = -EFAULT;
		} else {
			ret = -EFAULT;
		}
		break;
	case DV_IOC_SET_DV_SINGLE_PQ_VALUE:
		/*coverity[tainted_argument] copy same type will not pollute data.*/
		if (copy_from_user(&pq_info, argp,
				   sizeof(struct dv_pq_info_s)) == 0) {
			mode_id = pq_info.pic_mode_id;
			pq_item = pq_info.item;
			pq_value = pq_info.value;
			set_single_pq_value(mode_id, pq_item, pq_value);

			if (debug_dolby & 0x200)
				pr_info("[DV]: set mode %d, pq %s, value %d\n",
					mode_id,
					pq_item_str[pq_item],
					pq_value);

		} else {
			ret = -EFAULT;
		}
		break;
	case DV_IOC_SET_DV_FULL_PQ_VALUE:
		if (copy_from_user(&pq_full_info, argp,
				   sizeof(struct dv_full_pq_info_s)) == 0) {
			set_full_pq_value(pq_full_info);
		} else {
			ret = -EFAULT;
		}
		break;
	case DV_IOC_SET_DV_PQ_RESET:
		if (copy_from_user(&pq_reset, argp,
				   sizeof(enum pq_reset_e)) == 0) {
			restore_dv_pq_setting(pq_reset);
			if (debug_dolby & 0x200)
				pr_info("[DV]: reset mode %d\n", pq_reset);
		} else {
			ret = -EFAULT;
		}
		break;
	case DV_IOC_SET_DV_CONFIG_FILE:
		if (copy_from_user(&config_file, argp,
				   sizeof(struct dv_config_file_s)) == 0) {
			if (is_aml_tm2_tvmode() ||
			    is_aml_t7_tvmode() ||
			    is_aml_t3_tvmode() ||
			    is_aml_t5w() ||
			    p_funcs_tv) {
				memcpy(bin_name, config_file.bin_name, MAX_BYTES - 1);
				memcpy(cfg_name, config_file.cfg_name, MAX_BYTES - 1);
				bin_name[MAX_BYTES - 1] = '\0';
				cfg_name[MAX_BYTES - 1] = '\0';
				load_dv_pq_config_data(bin_name, cfg_name);
			}
			if (debug_dolby & 0x200)
				pr_info("[DV]: config_file %s, %s\n",
					config_file.bin_name,
					config_file.cfg_name);
		} else {
			ret = -EFAULT;
		}
		break;
	case DV_IOC_CONFIG_DV_BL:
		if (copy_from_user(&force_disable_dv_backlight, argp, sizeof(s32)) == 0) {
			if (debug_dolby & 0x200)
				pr_info("[DV]: disable dv bl %d\n", force_disable_dv_backlight);
			if (force_disable_dv_backlight)
				amdv_disable_backlight();
			else
				update_pwm_control();
		} else {
			ret = -EFAULT;
		}
		break;
	case DV_IOC_SET_DV_AMBIENT:
		if (copy_from_user(&ambient_config_new, argp,
			sizeof(struct ambient_cfg_s)) == 0) {
			ambient_update = true;
		} else {
			ret = -EFAULT;
		}
		break;
	case DV_IOC_SET_DV_DARK_DETAIL:
		mode_id = get_pic_mode();
		if (copy_from_user(&dark_detail, argp,
			sizeof(s32)) == 0) {
			if (debug_dolby & 0x200)
				pr_info("[DV]: set mode %d darkdetail %d\n",
					mode_id, dark_detail);
			dark_detail = dark_detail > 0 ? 1 : 0;
			if (dark_detail != cfg_info[mode_id].dark_detail) {
				need_update_cfg = true;
				cfg_info[mode_id].dark_detail = dark_detail;
			}
		} else {
			ret = -EFAULT;
		}
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

#ifdef CONFIG_COMPAT
static long amdolby_vision_compat_ioctl(struct file *file, unsigned int cmd,
	unsigned long arg)
{
	unsigned long ret;

	arg = (unsigned long)compat_ptr(arg);
	ret = amdolby_vision_ioctl(file, cmd, arg);
	return ret;
}
#endif

static const struct file_operations amdolby_vision_fops = {
	.owner   = THIS_MODULE,
	.open    = amdolby_vision_open,
	.write   = amdolby_vision_write,
	.read = amdolby_vision_read,
	.release = amdolby_vision_release,
	.unlocked_ioctl   = amdolby_vision_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = amdolby_vision_compat_ioctl,
#endif
	.poll = amdolby_vision_poll,
};

static const char *amdolby_vision_debug_usage_str = {
	"Usage:\n"
	"echo amdv_crc 0/1 > /sys/class/amdolby_vision/debug; amdv_crc insert or clr\n"
	"echo amdv_dma index(D) value(H) > /sys/class/amdolby_vision/debug; dma table modify\n"
	"echo dv_efuse > /sys/class/amdolby_vision/debug; get dv efuse info\n"
	"echo dv_el > /sys/class/amdolby_vision/debug; get dv enhanced layer info\n"
	"echo force_support_emp 1/0 > /sys/class/amdolby_vision/debug; send emp\n"
	"echo set_backlight_delay 0 > /sys/class/amdolby_vision/debug; set backlight no delay\n"
	"echo set_backlight_delay 1 > /sys/class/amdolby_vision/debug; set backlight delay one vysnc\n"
	"echo enable_vpu_probe 1 > /sys/class/amdolby_vision/debug; enable vpu probe\n"
	"echo debug_bypass_vpp_pq 0 > /sys/class/amdolby_vision/debug; not debug mode\n"
	"echo debug_bypass_vpp_pq 1 > /sys/class/amdolby_vision/debug; force disable vpp pq\n"
	"echo debug_bypass_vpp_pq 2 > /sys/class/amdolby_vision/debug; force enable vpp pq\n"
	"echo force_cert_bypass_vpp_pq 1 > /sys/class/amdolby_vision/debug; force bypass vpp pq in cert mode\n"
	"echo enable_vpu_probe 1 > /sys/class/amdolby_vision/debug; enable vpu probe\n"
	"echo ko_info > /sys/class/amdolby_vision/debug; query ko info\n"
	"echo enable_vf_check 1 > /sys/class/amdolby_vision/debug;\n"
	"echo enable_vf_check 0 > /sys/class/amdolby_vision/debug;\n"
	"echo force_hdmin_fmt value > /sys/class/amdolby_vision/debug; 1:HDR10 2:HLG 3:DV LL\n"
	"echo debug_cp_res value > /sys/class/amdolby_vision/debug; bit0~bit15 h, bit16~bit31 w\n"
	"echo debug_disable_aoi value > /sys/class/amdolby_vision/debug; 1:disable aoi;>1 not disable\n"
	"echo debug_dma_start_line value > /sys/class/amdolby_vision/debug;\n"
	"echo debug_vpotch value > /sys/class/amdolby_vision/debug;\n"
	"echo debug_ko value > /sys/class/amdolby_vision/debug;\n"
	"echo force_unmap > /sys/class/amdolby_vision/debug;\n"
};

static ssize_t  amdolby_vision_debug_show
		(struct class *cla,
		 struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n",  amdolby_vision_debug_usage_str);
}

static ssize_t amdolby_vision_debug_store
		(struct class *cla,
		 struct class_attribute *attr,
		 const char *buf, size_t count)
{
	char *buf_orig, *parm[MAX_PARAM] = {NULL};
	long val = 0;

	if (!buf)
		return count;
	buf_orig = kstrdup(buf, GFP_KERNEL);
	parse_param_amdv(buf_orig, (char **)&parm);
	if (!strcmp(parm[0], "amdv_crc")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			return -EINVAL;
		if (val == 1)
			amdv_crc_clear(val);
		else if (val == 2)
			crc_count = 0;
		else
			amdv_insert_crc(true);
	} else if (!strcmp(parm[0], "amdv_dma")) {
		long tbl_id;
		long value;

		if (kstrtoul(parm[1], 10, &tbl_id) < 0)
			return -EINVAL;
		if (kstrtoul(parm[2], 16, &value) < 0)
			return -EINVAL;
		tv_amdv_dma_table_modify((u32)tbl_id, (u64)value);
	} else if (!strcmp(parm[0], "dv_efuse")) {
		tv_amdv_efuse_info();
	} else if (!strcmp(parm[0], "dv_el")) {
		tv_amdv_el_info();
#ifdef V2_4_3
	} else if (!strcmp(parm[0], "force_support_emp")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			return -EINVAL;
		if (val == 0)
			force_support_emp = 0;
		else
			force_support_emp = 1;
		pr_info("force_support_emp %d\n", force_support_emp);
#endif
	} else if (!strcmp(parm[0], "enable_vpu_probe")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			return -EINVAL;
		if (val == 0) {
			enable_vpu_probe = 0;
		} else {
			enable_vpu_probe = 1;
			if ((is_aml_tm2() || is_meson_sm1_cpu()))
				__invoke_psci_fn_smc(0x82000080, 0, 0, 0);
		}
		pr_info("set enable_vpu_probe %d\n",
			enable_vpu_probe);
	} else if (!strcmp(parm[0], "debug_bypass_vpp_pq")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			return -EINVAL;
		set_debug_bypass_vpp_pq(val);
		pr_info("set debug_bypass_vpp_pq %d\n",
			(int)val);
	} else if (!strcmp(parm[0], "force_cert_bypass_vpp_pq")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			return -EINVAL;
		set_bypass_all_vpp_pq(val);
		pr_info("set bypass_all_vpp_pq %d\n",
			(int)val);
	} else if (!strcmp(parm[0], "enable_fel")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			return -EINVAL;
		enable_fel = val;
		pr_info("enable_fel %d\n", enable_fel);
	} else if (!strcmp(parm[0], "enable_mel")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			return -EINVAL;
		enable_mel = val;
		pr_info("enable_mel %d\n", enable_mel);
	} else if (!strcmp(parm[0], "enable_tunnel")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			return -EINVAL;
		enable_tunnel = val;
		pr_info("enable_tunnel %d\n", enable_tunnel);
		if (is_aml_sc2() || is_aml_t7() || is_aml_t3() ||
		    is_aml_s4d() || is_aml_t5w() || is_aml_s5()) {/*not include tm2*/
			/*for vdin1 loop back, 444,12bit->422,12bit->444,8bit*/
			if (enable_tunnel) {
				if (vpp_data_422T0444_backup == 0) {
					vpp_data_422T0444_backup =
					VSYNC_RD_DV_REG(VPU_422T0444_CTRL1);
					pr_dv_dbg("vpp_data_422T0444_backup %x\n",
						     vpp_data_422T0444_backup);
				}
				/*go_field_en and go_line_en bit 24 25 =1*/
				VSYNC_WR_DV_REG(VPU_422T0444_CTRL1, 0x07c0ba14);
			} else {
				if (vpp_data_422T0444_backup != 0)
					VSYNC_WR_DV_REG(VPU_422T0444_CTRL1,
						vpp_data_422T0444_backup);
			}
		}
	} else if (!strcmp(parm[0], "ko_info")) {
		if (ko_info)
			pr_info("ko info: %s\n", ko_info);
	} else if (!strcmp(parm[0], "enable_vf_check")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			return -EINVAL;
		if (val == 0)
			enable_vf_check = 0;
		else
			enable_vf_check = 1;
		pr_info("set enable_vf_check %d\n",
			enable_vf_check);
	} else if (!strcmp(parm[0], "force_hdmin_fmt")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			return -EINVAL;
		force_hdmin_fmt = val;
		pr_info("set force_hdmin_fmt %d\n", force_hdmin_fmt);
	} else if (!strcmp(parm[0], "force_bypass_pps_sr_cm")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			return -EINVAL;
		force_bypass_pps_sr_cm = val;
		pr_info("set force_skip_pps_sr_cm %d\n", force_bypass_pps_sr_cm);
	} else if (!strcmp(parm[0], "pri_input")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			return -EINVAL;
		pri_input = val;
		pr_info("set pri_input %d\n", pri_input);
	} else if (!strcmp(parm[0], "debug_ko")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			return -EINVAL;
		debug_ko = val;
		pr_info("set debug_ko %d\n", debug_ko);
	} else if (!strcmp(parm[0], "debug_disable_aoi")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			return -EINVAL;
		debug_disable_aoi = val;
		pr_info("set debug_disable_aoi %d\n", debug_disable_aoi);
	} else if (!strcmp(parm[0], "debug_cp_res")) {
		if (kstrtoul(parm[1], 16, &val) < 0)
			return -EINVAL;
		debug_cp_res = val;
		pr_info("set debug_cp_res 0x%x\n", debug_cp_res);
	} else if (!strcmp(parm[0], "debug_dma_start_line")) {
		if (kstrtoul(parm[1], 16, &val) < 0)
			return -EINVAL;
		debug_dma_start_line = val;
		pr_info("set debug_dma_start_line %d\n", debug_dma_start_line);
	} else if (!strcmp(parm[0], "debug_vpotch")) {
		if (kstrtoul(parm[1], 16, &val) < 0)
			return -EINVAL;
		debug_vpotch = val;
		pr_info("set debug_vpotch %d\n", debug_vpotch);
	} else if (!strcmp(parm[0], "force_runmode")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			return -EINVAL;
		if (val == 0)
			force_runmode = 0;
		else
			force_runmode = 1;
		pr_info("set force_runmode %d\n",
			force_runmode);
	} else if (!strcmp(parm[0], "force_unmap")) {
		pr_info("force unmap\n");
		force_unmap_all_inst();
	} else {
		pr_info("unsupport cmd\n");
	}

	kfree(buf_orig);
	return count;
}

static ssize_t	amdolby_vision_primary_show
	(struct class *cla,
	 struct class_attribute *attr, char *buf)
{
	return get_dv_debug_tprimary(buf);
}

static ssize_t amdolby_vision_primary_store
	(struct class *cla,
	 struct class_attribute *attr,
	 const char *buf, size_t count)
{
	int ret = set_dv_debug_tprimary(buf, count);
	return ret;
}

static ssize_t	amdolby_vision_config_file_show
	(struct class *cla,
	struct class_attribute *attr, char *buf)
{
	const char *str =
		"echo bin cfg > /sys/class/amdolby_vision/config_file";
	return sprintf(buf, "%s\n", str);
}

static ssize_t amdolby_vision_config_file_store
	(struct class *cla,
	struct class_attribute *attr,
	const char *buf, size_t count)
{
	char *buf_orig, *parm[MAX_PARAM] = {NULL};

	if (!buf)
		return -EFAULT;

	if (!module_installed)
		return -EAGAIN;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	parse_param_amdv(buf_orig, (char **)&parm);
	if (!parm[0] || !parm[1]) {
		pr_info("missing parameter... param1:bin param2:cfg\n");
		kfree(buf_orig);
		return -EINVAL;
	}
	pr_info("parm[0]: %s, parm[1]: %s\n", parm[0], parm[1]);
	if (is_aml_tm2_tvmode() || p_funcs_tv)
		load_dv_pq_config_data(parm[0], parm[1]);

	kfree(buf_orig);
	return count;
}

static ssize_t	amdolby_vision_dv_provider_show
	(struct class *cla,
	struct class_attribute *attr, char *buf)
{
	if (multi_dv_mode)
		return sprintf(buf, "%s %s\n", dv_provider[0], dv_provider[1]);
	else
		return sprintf(buf, "%s\n", dv_provider[0]);
}

static ssize_t	amdolby_vision_content_fps_show
	(struct class *cla,
	struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "content_fps: %d\n", content_fps);
}

static ssize_t amdolby_vision_content_fps_store
	(struct class *cla,
	struct class_attribute *attr,
	const char *buf, size_t count)
{
	size_t r;

	if (!buf)
		return count;

	r = kstrtoint(buf, 0, &content_fps);
	if (r != 0)
		return -EINVAL;

	return count;
}

static ssize_t	amdolby_vision_gd_rf_adjust_show
	(struct class *cla,
	struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "gd_rf_adjust: %d\n", gd_rf_adjust);
}

static ssize_t amdolby_vision_gd_rf_adjust_store
	(struct class *cla,
	struct class_attribute *attr,
	const char *buf, size_t count)
{
	size_t r;

	if (!buf)
		return count;

	r = kstrtoint(buf, 0, &gd_rf_adjust);
	if (r != 0)
		return -EINVAL;

	return count;
}

static ssize_t	amdolby_vision_force_disable_bl_show
	(struct class *cla,
	struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "force disable dv bl: %d\n", force_disable_dv_backlight);
}

static ssize_t amdolby_vision_force_disable_bl_store
	(struct class *cla,
	struct class_attribute *attr,
	const char *buf, size_t count)
{
	size_t r;

	if (!buf)
		return count;

	r = kstrtoint(buf, 0, &force_disable_dv_backlight);
	if (r != 0)
		return -EINVAL;

	pr_info("update force_disable_dv_backlight to %d\n", force_disable_dv_backlight);
	if (force_disable_dv_backlight)
		amdv_disable_backlight();
	else
		update_pwm_control();

	return count;
}

static ssize_t	amdolby_vision_use_cfg_target_lum_show
	(struct class *cla,
	 struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "use_target_lum_from_cfg: %d\n", use_target_lum_from_cfg);
}

static ssize_t amdolby_vision_use_cfg_target_lum_store
	(struct class *cla,
	 struct class_attribute *attr,
	 const char *buf, size_t count)
{
	size_t r;

	if (!buf)
		return count;

	r = kstrtoint(buf, 0, &use_target_lum_from_cfg);
	if (r != 0)
		return -EINVAL;

	pr_info("update use_target_lum_from_cfg to %d\n", use_target_lum_from_cfg);

	return count;
}

static ssize_t	amdolby_vision_hdmi_in_allm_show
	(struct class *cla,
	 struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "hdmi_in_allm: %d\n", hdmi_in_allm);
}

static ssize_t amdolby_vision_hdmi_in_allm_store
	(struct class *cla,
	 struct class_attribute *attr,
	 const char *buf, size_t count)
{
	size_t r;
	int tmp;

	if (!buf)
		return count;

	r = kstrtoint(buf, 0, &tmp);
	if (r != 0)
		return -EINVAL;

	hdmi_in_allm = tmp > 0 ? true : false;
	pr_info("update hdmi_in_allm to %d\n", hdmi_in_allm);

	return count;
}

static ssize_t	amdolby_vision_local_allm_show
	(struct class *cla,
	 struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "local_allm: %d\n", local_allm);
}

static ssize_t amdolby_vision_local_allm_store
	(struct class *cla,
	 struct class_attribute *attr,
	 const char *buf, size_t count)
{
	size_t r;
	int tmp;

	if (!buf)
		return count;

	r = kstrtoint(buf, 0, &tmp);
	if (r != 0)
		return -EINVAL;

	local_allm = tmp > 0 ? true : false;
	pr_info("update local_allm to %d\n", local_allm);

	return count;
}

static ssize_t	amdolby_vision_brightness_off_show
	(struct class *cla,
	struct class_attribute *attr, char *buf)
{
	ssize_t len = 0;

	len += sprintf(buf + len, "DV OTT DM3, DM4: %d %d\n",
			brightness_off[0][0], brightness_off[0][1]);
	len += sprintf(buf + len, "DV Sink-led DM3, DM4: %d %d\n",
			brightness_off[1][0], brightness_off[1][1]);
	len += sprintf(buf + len, "DV Source-led DM3, DM4: %d %d\n",
			brightness_off[2][0], brightness_off[2][1]);
	len += sprintf(buf + len, "HDR10 HDMI, OTT: %d %d\n",
			brightness_off[3][0], brightness_off[3][1]);
	len += sprintf(buf + len, "HLG HDMI, OTT: %d %d\n",
			brightness_off[4][0], brightness_off[4][1]);

	return len;
}

/* supported mode: IPT_TUNNEL/HDR10/SDR10 */
static const int dv_mode_table[6] = {
	5, /*AMDV_OUTPUT_MODE_BYPASS*/
	0, /*AMDV_OUTPUT_MODE_IPT*/
	1, /*AMDV_OUTPUT_MODE_IPT_TUNNEL*/
	2, /*AMDV_OUTPUT_MODE_HDR10*/
	3, /*AMDV_OUTPUT_MODE_SDR10*/
	4, /*AMDV_OUTPUT_MODE_SDR8*/
};

static const char dv_mode_str[6][12] = {
	"IPT",
	"IPT_TUNNEL",
	"HDR10",
	"SDR10",
	"SDR8",
	"BYPASS"
};

unsigned int amdv_check_enable(void)
{
	int uboot_dv_mode = 0;
	int uboot_dv_source_led_yuv = 0;
	int uboot_dv_source_led_rgb = 0;
	int uboot_dv_sink_led = 0;
	const struct vinfo_s *vinfo = get_current_vinfo();

	/*first step: check tv mode, dv is disabled by default */
	if (is_aml_tm2_tvmode() || is_aml_t7_tvmode() ||
	    is_aml_t3() || is_aml_t5w()) {
		if (!amdv_on_in_uboot) {
			/* tm2/t7 also has stb core, should power off stb core*/
			if (is_aml_tm2_tvmode() || is_aml_t7_tvmode()) {
				/* core1a */
				dv_mem_power_off(VPU_DOLBY1A);
				dv_mem_power_off(VPU_PRIME_DOLBY_RAM);
				WRITE_VPP_DV_REG
					(AMDV_CORE1A_CLKGATE_CTRL,
					0x55555455);
				/* core1b */
				dv_mem_power_off(VPU_DOLBY1B);
				WRITE_VPP_DV_REG
					(AMDV_CORE1B_CLKGATE_CTRL,
					0x55555455);
				/* core2 */
				dv_mem_power_off(VPU_DOLBY2);
				WRITE_VPP_DV_REG
					(AMDV_CORE2A_CLKGATE_CTRL,
					0x55555555);
				/* core3 */
				dv_mem_power_off(VPU_DOLBY_CORE3);
				WRITE_VPP_DV_REG
					(AMDV_CORE3_CLKGATE_CTRL,
					0x55555555);
			}
			/* tv core */
			WRITE_VPP_DV_REG(AMDV_TV_AXI2DMA_CTRL0,
				0x01000042);
			WRITE_VPP_DV_REG_BITS
				(AMDV_TV_SWAP_CTRL7,
				0xf, 4, 4);
			dv_mem_power_off(VPU_DOLBY0);
			WRITE_VPP_DV_REG
				(AMDV_TV_CLKGATE_CTRL,
				0x55555555);
			if (is_aml_t3() || is_aml_t5w())
				vpu_module_clk_disable(0, DV_TVCORE, 1);
			pr_info("dovi disable in uboot\n");
		}
	}

	/*second step: check ott mode*/
	if (is_aml_g12() || is_aml_sc2() || is_aml_tm2_stbmode() ||
		is_aml_t7_stbmode() || is_aml_s4d() || is_aml_s5()) {
		if (amdv_on_in_uboot) {
			if (is_aml_s5()) {
				if (amdv_uboot_on == 2) {
					if ((READ_VPP_DV_REG
						(AMDV_CORE3_DIAG_CTRL) & 0xff) == 0x3) {
						/*LL RGB444 mode*/
						uboot_dv_mode = dv_mode_table[2];
						uboot_dv_source_led_rgb = 1;
					} else {
						/*LL YUV422 mode*/
						uboot_dv_mode = dv_mode_table[2];
						uboot_dv_source_led_yuv = 1;
					}
				} else if (amdv_uboot_on == 3) {
					/*HDR mode*/
					uboot_dv_mode = dv_mode_table[3];
					uboot_dv_source_led_yuv = 1;
				} else if (amdv_uboot_on == 4) {
					/*SDR mode*/
					uboot_dv_mode = dv_mode_table[5];
					uboot_dv_source_led_yuv = 1;
				} else {
					/*STANDARD RGB444 mode*/
					uboot_dv_mode = dv_mode_table[2];
					uboot_dv_sink_led = 1;
				}
			} else {
				if ((READ_VPP_DV_REG(AMDV_CORE3_DIAG_CTRL)
					& 0xff) == 0x20) {
					/*LL YUV422 mode*/
					uboot_dv_mode = dv_mode_table[2];
					uboot_dv_source_led_yuv = 1;
				} else if ((READ_VPP_DV_REG
					(AMDV_CORE3_DIAG_CTRL)
					& 0xff) == 0x3) {
					/*LL RGB444 mode*/
					uboot_dv_mode = dv_mode_table[2];
					uboot_dv_source_led_rgb = 1;
				} else {
					if (READ_VPP_DV_REG
						(AMDV_CORE3_REG_START + 1)
						== 2) {
						/*HDR10 mode*/
						uboot_dv_mode = dv_mode_table[3];
					} else if (READ_VPP_DV_REG
						(AMDV_CORE3_REG_START + 1)
						== 4) {
						/*SDR mode*/
						uboot_dv_mode = dv_mode_table[5];
					} else {
						/*STANDARD RGB444 mode*/
						uboot_dv_mode = dv_mode_table[2];
						uboot_dv_sink_led = 1;
					}
				}
			}
			if (recovery_mode) {/*recovery mode*/
				pr_info("recovery_mode\n");
				dolby_vision_on = true;
				if (uboot_dv_source_led_yuv ||
					uboot_dv_sink_led) {
					#ifdef CONFIG_AMLOGIC_HDMITX
					if (uboot_dv_source_led_yuv)
						setup_attr("422,12bit");
					else
						setup_attr("444,8bit");
					#endif
				}
				if (vinfo && vinfo->vout_device &&
				    vinfo->vout_device->fresh_tx_vsif_pkt) {
					vinfo->vout_device->fresh_tx_vsif_pkt
						(0, 0, NULL, true);
				}
				// enable_amdv(0);
				// amdv_on_in_uboot = 0;
			} else {
				dolby_vision_enable = 1;
				if (uboot_dv_mode == dv_mode_table[2] &&
					uboot_dv_source_led_yuv == 1) {
					/*LL YUV422 mode*/
					/*set_amdv_mode(dv_mode);*/
					dolby_vision_mode = uboot_dv_mode;
					dolby_vision_status = DV_PROCESS;
					dolby_vision_ll_policy =
						DOLBY_VISION_LL_YUV422;
					last_dst_format = FORMAT_DOVI;
					pr_info("dovi enable in uboot and mode is LL 422\n");
				} else if ((uboot_dv_mode ==
						dv_mode_table[2]) &&
						(uboot_dv_source_led_rgb ==
						1)) {
					/*LL RGB444 mode*/
					/*set_amdv_mode(dv_mode);*/
					dolby_vision_mode = uboot_dv_mode;
					dolby_vision_status = DV_PROCESS;
					dolby_vision_ll_policy =
						DOLBY_VISION_LL_RGB444;
					last_dst_format = FORMAT_DOVI;
					pr_info("dovi enable in uboot and mode is LL RGB\n");
				} else {
					if (uboot_dv_mode == dv_mode_table[3]) {
						/*HDR10 mode*/
						dolby_vision_hdr10_policy |=
							HDR_BY_DV_F_SINK;
						dolby_vision_mode =
							uboot_dv_mode;
						dolby_vision_status =
							HDR_PROCESS;
						pr_info("dovi enable in uboot and mode is HDR10\n");
						last_dst_format = FORMAT_HDR10;
					} else if (uboot_dv_mode ==
						dv_mode_table[5]) {
						/*SDR mode*/
						dolby_vision_mode =
							uboot_dv_mode;
						dolby_vision_status =
							SDR_PROCESS;
						pr_info("dovi enable in uboot and mode is SDR\n");
						last_dst_format = FORMAT_SDR;
					} else {
						/*STANDARD RGB444 mode*/
						dolby_vision_mode =
							uboot_dv_mode;
						dolby_vision_status =
							DV_PROCESS;
						dolby_vision_ll_policy =
							DOLBY_VISION_LL_DISABLE;
						last_dst_format = FORMAT_DOVI;
						pr_info("dovi enable in uboot and mode is DV ST\n");
					}
				}
				amdv_target_mode = dolby_vision_mode;
			}
		} else {
			/* core1a */
			dv_mem_power_off(VPU_DOLBY1A);
			dv_mem_power_off(VPU_PRIME_DOLBY_RAM);
			WRITE_VPP_DV_REG
				(AMDV_CORE1A_CLKGATE_CTRL,
				0x55555455);
			/* core1b */
			dv_mem_power_off(VPU_DOLBY1B);
			WRITE_VPP_DV_REG
				(AMDV_CORE1B_CLKGATE_CTRL,
				0x55555455);
			/* core2 */
			dv_mem_power_off(VPU_DOLBY2);
			WRITE_VPP_DV_REG
				(AMDV_CORE2A_CLKGATE_CTRL,
				0x55555555);
			if (is_aml_s5() || is_aml_t7_stbmode())
				WRITE_VPP_DV_REG
					(AMDV_CORE2C_CLKGATE_CTRL,
					0x55555555);
			/* core3 */
			dv_mem_power_off(VPU_DOLBY_CORE3);
			WRITE_VPP_DV_REG
				(AMDV_CORE3_CLKGATE_CTRL,
				0x55555555);
			/*tm2/t7 has tvcore, should also power off tvcore*/
			if (is_aml_tm2_stbmode() || is_aml_t7_stbmode()) {
				/* tv core */
				WRITE_VPP_DV_REG(AMDV_TV_AXI2DMA_CTRL0,
					0x01000042);
				WRITE_VPP_DV_REG_BITS
					(AMDV_TV_SWAP_CTRL7,
					0xf, 4, 4);
				dv_mem_power_off(VPU_DOLBY0);
				WRITE_VPP_DV_REG
					(AMDV_TV_CLKGATE_CTRL,
					0x55555555);
			}
			pr_info("dovi disable in uboot\n");
		}
	}
	return 0;
}

static const char *amdolby_vision_mode_str = {
	"usage: echo mode > /sys/class/amdolby_vision/dv_mode\n"
	"\tAMDV_OUTPUT_MODE_BYPASS		0\n"
	"\tAMDV_OUTPUT_MODE_IPT		1\n"
	"\tAMDV_OUTPUT_MODE_IPT_TUNNEL	2\n"
	"\tAMDV_OUTPUT_MODE_HDR10		3\n"
	"\tAMDV_OUTPUT_MODE_SDR10		4\n"
	"\tAMDV_OUTPUT_MODE_SDR8		5\n"
};

static ssize_t amdolby_vision_dv_mode_show(struct class *cla,
			struct class_attribute *attr, char *buf)
{
	ssize_t len = 0;

	len += sprintf(buf + len, "%s\n", amdolby_vision_mode_str);
	if (is_amdv_enable())
		len += sprintf(buf + len, "current dv_mode = %s\n",
			       dv_mode_str[get_amdv_mode()]);
	else
		len += sprintf(buf + len, "current dv_mode = off\n");
	return len;
}

static ssize_t amdolby_vision_dv_mode_store
		(struct class *cla,
		 struct class_attribute *attr,
		 const char *buf, size_t count)
{
	size_t r;
	int val;

	r = kstrtoint(buf, 0, &val);
	if (r != 0)
		return -EINVAL;

	if (val >= 0 && val < 6)
		set_amdv_mode(dv_mode_table[val]);
	else if (val & 0x200)
		amdv_dump_struct();
	else if (val & 0x70)
		amdv_dump_setting(val);
	return count;
}

static void parse_param(char *buf_orig, char **parm)
{
	char *ps, *token;
	unsigned int n = 0;
	char delim1[3] = " ";
	char delim2[2] = "\n";

	ps = buf_orig;
	strcat(delim1, delim2);
	while (1) {
		token = strsep(&ps, delim1);
		if (!token)
			break;
		if (*token == '\0')
			continue;
		parm[n++] = token;
	}
}

static ssize_t amdolby_vision_reg_store
		(struct class *cla,
		 struct class_attribute *attr,
		 const char *buf, size_t count)
{
	char *buf_orig, *parm[8] = {NULL};
	long val = 0;
	unsigned int reg_addr, reg_val;

	if (!buf)
		return count;
	buf_orig = kstrdup(buf, GFP_KERNEL);
	parse_param(buf_orig, (char **)&parm);
	if (!strcmp(parm[0], "rv")) {
		if (kstrtoul(parm[1], 16, &val) < 0) {
			kfree(buf_orig);
			buf_orig =  NULL;
			return -EINVAL;
		}
		reg_addr = val;
		reg_val = READ_VPP_DV_REG(reg_addr);
		pr_info("reg[0x%04x]=0x%08x\n", reg_addr, reg_val);
	} else if (!strcmp(parm[0], "wv")) {
		if (kstrtoul(parm[1], 16, &val) < 0) {
			kfree(buf_orig);
			buf_orig =  NULL;
			return -EINVAL;
		}
		reg_addr = val;
		if (kstrtoul(parm[2], 16, &val) < 0) {
			kfree(buf_orig);
			buf_orig =  NULL;
			return -EINVAL;
		}
		reg_val = val;
		WRITE_VPP_DV_REG(reg_addr, reg_val);
	}
	kfree(buf_orig);
	buf_orig = NULL;
	return count;
}

static ssize_t amdolby_vision_core1_switch_show
		(struct class *cla,
		 struct class_attribute *attr, char *buf)
{
	return snprintf(buf, 40, "%d\n",
		core1_switch);
}

static ssize_t amdolby_vision_core1_switch_store
		(struct class *cla,
		 struct class_attribute *attr,
		 const char *buf, size_t count)
{
	size_t r;
	u32 reg = 0, mask = 0xfaa1f00, set = 0;

	r = kstrtoint(buf, 0, &core1_switch);
	if (r != 0)
		return -EINVAL;
	if (is_aml_tm2_stbmode() || is_aml_t7_stbmode()) {
		reg = VSYNC_RD_DV_REG(AMDV_PATH_CTRL);
		switch (core1_switch) {
		case NO_SWITCH:
			reg &= ~mask;
			set = reg | 0x0c880c00;
			VSYNC_WR_DV_REG(AMDV_PATH_CTRL, set);
			break;
		case SWITCH_BEFORE_DVCORE_1:
			reg &= ~mask;
			set = reg | 0x0c881c00;
			VSYNC_WR_DV_REG(AMDV_PATH_CTRL, set);
			break;
		case SWITCH_BEFORE_DVCORE_2:
			reg &= ~mask;
			set = reg | 0x0c820300;
			VSYNC_WR_DV_REG(AMDV_PATH_CTRL, set);
			break;
		case SWITCH_AFTER_DVCORE:
			reg &= ~mask;
			set = reg | 0x03280c00;
			VSYNC_WR_DV_REG(AMDV_PATH_CTRL, set);
			break;
		}
	}
	return count;
}

static ssize_t amdolby_vision_core3_switch_show
		(struct class *cla,
		 struct class_attribute *attr, char *buf)
{
	return snprintf(buf, 40, "%d\n",
		core3_switch);
}

static ssize_t amdolby_vision_dv_support_info_show
	(struct class *cla,
	 struct class_attribute *attr, char *buf)
{
	pr_dv_dbg("show dv capability %d\n", support_info);
	return snprintf(buf, 40, "%d\n",
		support_info);
}

static ssize_t amdolby_vision_core3_switch_store
		(struct class *cla,
		 struct class_attribute *attr,
		 const char *buf, size_t count)
{
	size_t r;

	r = kstrtoint(buf, 0, &core3_switch);
	if (r != 0)
		return -EINVAL;
	if (is_aml_tm2_stbmode() || is_aml_t7_stbmode()) {
		switch (core3_switch) {
		case CORE3_AFTER_WM:
			VSYNC_WR_DV_REG_BITS(VPP_AMDV_CTRL,
					     0, 24, 2);
			break;
		case CORE3_AFTER_OSD1_HDR:
			VSYNC_WR_DV_REG_BITS(VPP_AMDV_CTRL,
					     1, 24, 2);
			break;
		case CORE3_AFTER_VD2_HDR:
			VSYNC_WR_DV_REG_BITS(VPP_AMDV_CTRL,
					     2, 24, 2);
			break;
		}
	}
	return count;
}

static ssize_t amdolby_vision_copy_core1a_show
	(struct class *cla,
	 struct class_attribute *attr, char *buf)
{
	pr_dv_dbg("copy_core1a %d\n", copy_core1a);
	return snprintf(buf, 40, "%d\n",
		copy_core1a);
}

static ssize_t amdolby_vision_copy_core1a_store
		(struct class *cla,
		 struct class_attribute *attr,
		 const char *buf, size_t count)
{
	size_t r;

	r = kstrtoint(buf, 0, &copy_core1a);
	if (r != 0)
		return -EINVAL;
	return count;
}

static ssize_t amdolby_vision_core2_sel_show
	(struct class *cla,
	 struct class_attribute *attr, char *buf)
{
	pr_dv_dbg("core2_sel %d\n", core2_sel);
	return snprintf(buf, 40, "%d\n",
		core2_sel);
}

static ssize_t amdolby_vision_core2_sel_store
		(struct class *cla,
		 struct class_attribute *attr,
		 const char *buf, size_t count)
{
	size_t r;

	if (!is_aml_t7() && !is_aml_s5())
		return -EINVAL;
	r = kstrtoint(buf, 0, &core2_sel);
	if (r != 0)
		return -EINVAL;
	return count;
}

static ssize_t  amdolby_vision_crc_show(struct class *cla,
		struct class_attribute *attr, char *buf)
{
	if ((debug_dolby & 0x10000))
		pr_dv_dbg("get crc %s\n", cur_crc);
	return sprintf(buf, "%s\n", cur_crc);
}

static ssize_t amdolby_vision_enable_multi_core1_show
	(struct class *cla,
	 struct class_attribute *attr, char *buf)
{
	pr_dv_dbg("enable_multi_core1 %d\n", enable_multi_core1);
	return snprintf(buf, 40, "%d\n",
		enable_multi_core1);
}

static ssize_t amdolby_vision_enable_multi_core1_store
		(struct class *cla,
		 struct class_attribute *attr,
		 const char *buf, size_t count)
{
	size_t r;
	int ret;

	if (!(is_multi_dv_mode() && (is_aml_t7() || is_aml_tm2() || is_aml_s5()))) {
		pr_info("not support mulit core1\n");
		return -EINVAL;
	}
	r = kstrtoint(buf, 0, &ret);
	if (r != 0)
		return -EINVAL;
	enable_multi_core1 = ret > 0 ? true : false;
	return count;
}

static ssize_t amdolby_vision_force_pri_input_show
	(struct class *cla,
	 struct class_attribute *attr, char *buf)
{
	pr_dv_dbg("force_pri_input %d\n", force_pri_input);
	return snprintf(buf, 40, "%d\n",
		force_pri_input);
}

static ssize_t amdolby_vision_force_pri_input_store
		(struct class *cla,
		 struct class_attribute *attr,
		 const char *buf, size_t count)
{
	size_t r;
	int ret;

	if (!(support_multi_core1() && (is_aml_t7() || is_aml_tm2() || is_aml_s5()))) {
		pr_info("not support mulit core1\n");
		return -EINVAL;
	}
	r = kstrtoint(buf, 0, &ret);
	if (r != 0)
		return -EINVAL;
	force_pri_input = ret;
	pr_dv_dbg("force_pri_input %d\n", force_pri_input);
	return count;
}

static const char *signal_format_str[12] = {
	"FORMAT_INVALID",
	"FORMAT_DOVI",
	"FORMAT_HDR10",
	"FORMAT_SDR",
	"FORMAT_DOVI_LL",
	"FORMAT_HLG",
	"FORMAT_HDR10PLUS",
	"FORMAT_SDR_2020",
	"FORMAT_MVC",
	"FORMAT_SDR10",
	"FORMAT_HDR8"
};

static const char *debug_format_str[9] = {
	"NONE",
	"HDR10",
	"HDR10+",
	"DV",
	"PRIME",
	"HLG",
	"SDR",
	"MVC"
};

static ssize_t amdolby_vision_src_format_show
	(struct class *cla,
	 struct class_attribute *attr, char *buf)
{
	ssize_t len = 0;
	int i;
	int fmt = 0;
	int fmt_inside = 0;
	int dv_id;

	if (multi_dv_mode) {
		for (i = 0; i < NUM_IPCORE1; i++) {
			if (i == 0)
				dv_id = layer_id_to_dv_id(VD1_PATH);
			else
				dv_id = layer_id_to_dv_id(VD2_PATH);

			if (i == 1 && !enable_multi_core1)
				break;

			if (dv_inst_valid(dv_id))
				fmt = dv_inst[dv_id].amdv_src_format;

			if (fmt < 0 || fmt >= sizeof(debug_format_str) /
				sizeof(debug_format_str[0]))
				fmt = 0;
			fmt_inside = m_dovi_setting.input[i].src_format + 1;
			if (fmt_inside < 0 ||
				fmt_inside >= sizeof(signal_format_str) /
				sizeof(signal_format_str[0]))
				fmt_inside = 0;
			len += sprintf(buf + len, "vd%d(inst%d): %s %s\n", i + 1, dv_id + 1,
				       debug_format_str[fmt],
				       signal_format_str[fmt_inside]);
		}
		fmt = graphic_fmt + 1;
		if (fmt < 0 ||
		    fmt >= sizeof(signal_format_str) /
		    sizeof(signal_format_str[0]))
			fmt = 0;
		len += sprintf(buf + len, "graphic: %s\n",
			       signal_format_str[fmt]);
	} else {
		if (is_aml_tvmode()) {
			if (tv_dovi_setting)
				fmt_inside = tv_dovi_setting->src_format + 1;
			if (fmt_inside < 0 ||
				fmt_inside >= sizeof(signal_format_str) /
				sizeof(signal_format_str[0]))
				fmt_inside = 0;
			len += sprintf(buf + len, "%s, inside: %s\n",
				       input_str[amdv_src_format],
				       signal_format_str[fmt_inside]);
		} else {
			fmt_inside = dovi_setting.src_format + 1;
			if (fmt_inside < 0 ||
				fmt_inside >= sizeof(signal_format_str) /
				sizeof(signal_format_str[0]))
				fmt_inside = 0;
			len += sprintf(buf + len, "%s, inside: %s\n",
					   input_str[amdv_src_format],
					   signal_format_str[fmt_inside]);
		}
	}
	return len;
}

static ssize_t amdolby_vision_force_priority_show
		(struct class *cla,
		 struct class_attribute *attr,
		 char *buf)
{
	ssize_t len = 0;
	char *priority_str = {
	"force graphic priority:1\n"
	"force video   priority:2\n"
	};

	len += sprintf(buf + len, "%s\n", priority_str);
	len += sprintf(buf + len, "current %d\n",
		force_priority);
	return len;
}

static ssize_t amdolby_vision_force_priority_store
		(struct class *cla,
		 struct class_attribute *attr,
		 const char *buf, size_t count)

{
	size_t r;
	int flag;

	r = kstrtoint(buf, 0, &flag);
	if (r != 0)
		return -EINVAL;
	force_priority = flag;
	return count;
}

static ssize_t amdolby_vision_inst_status_show
		(struct class *cla,
		 struct class_attribute *attr,
		 char *buf)
{
	ssize_t len = 0;
	int i;
	int fmt;
	char *str1 = {"dv inst status:"};
	char *str2 = {"dv core status:"};

	len += sprintf(buf + len, "enable multi core1: %d\n", enable_multi_core1);
	len += sprintf(buf + len, "valid num %d, pri_input: %d\n",
		       cur_valid_video_num, pri_input);
	len += sprintf(buf + len, "vd1_inst_id %d, vd2_inst_id: %d\n",
		       vd1_inst_id + 1, vd2_inst_id + 1);

	len += sprintf(buf + len, "%s\n", str1);
	for (i = 0; i < NUM_INST; i++) {
		fmt = dv_inst[i].amdv_src_format;
		if (fmt < 0 || fmt >= sizeof(debug_format_str) /
			sizeof(debug_format_str[0]))
			fmt = 0;
		len += sprintf(buf + len, "[inst%d]:mapped %d,valid %d,vd%d,fmt %s,%dx%d\n",
			       i + 1, dv_inst[i].mapped, dv_inst[i].valid,
			       dv_inst[i].layer_id + 1,
				   debug_format_str[fmt],
			       dv_inst[i].video_width,
			       dv_inst[i].video_height);
	}

	len += sprintf(buf + len, "%s\n", str2);
	for (i = 0; i < NUM_IPCORE1; i++)
		len += sprintf(buf + len, "[core1%s]: video flag %d, core on %d\n",
			       i == 0 ? "a" : "b", dv_core1[i].amdv_setting_video_flag,
			       dv_core1[i].core1_on);

	 len += sprintf(buf + len, "core1a_core1b_switch %d\n", core1a_core1b_switch);

	return len;
}

static ssize_t amdolby_vision_inst_debug_show
		(struct class *cla,
		 struct class_attribute *attr,
		 char *buf)
{
	ssize_t len = 0;

	len += sprintf(buf + len, "inst0 %d, %x, %x\n",
				   inst_debug[0], inst_res_debug[0], inst_res_debug[1]);
	len += sprintf(buf + len, "inst1 %d, %x, %x\n",
				   inst_debug[1], inst_res_debug[2], inst_res_debug[3]);
	return len;
}

static ssize_t amdolby_vision_inst_debug_store
		(struct class *cla,
		 struct class_attribute *attr,
		 const char *buf, size_t count)

{
	char *buf_orig, *parm[8] = {NULL};
	long val1 = 0;
	long val2 = 0;

	if (!buf)
		return count;
	buf_orig = kstrdup(buf, GFP_KERNEL);
	parse_param(buf_orig, (char **)&parm);
	if (!strcmp(parm[0], "inst0")) {
		if (kstrtoul(parm[1], 10, &val1) < 0) {
			kfree(buf_orig);
			buf_orig =  NULL;
			return -EINVAL;
		}
		if (kstrtoul(parm[2], 10, &val2) < 0) {
			kfree(buf_orig);
			buf_orig =  NULL;
			return -EINVAL;
		}
		inst_res_debug[0] = val1;
		inst_res_debug[1] = val2;
		inst_debug[0] = 1;
	} else if (!strcmp(parm[0], "inst1")) {
		if (kstrtoul(parm[1], 10, &val1) < 0) {
			kfree(buf_orig);
			buf_orig =  NULL;
			return -EINVAL;
		}
		if (kstrtoul(parm[2], 10, &val2) < 0) {
			kfree(buf_orig);
			buf_orig =  NULL;
			return -EINVAL;
		}
		inst_res_debug[2] = val1;
		inst_res_debug[3] = val2;
		inst_debug[1] = 1;
	}
	kfree(buf_orig);
	buf_orig = NULL;
	return count;
}

static ssize_t amdolby_vision_operate_mode_show
	 (struct class *cla,
	  struct class_attribute *attr,
	  char *buf)
{
	return snprintf(buf, 40, "%d\n", get_operate_mode());
}

static ssize_t amdolby_vision_operate_mode_store
	 (struct class *cla,
	  struct class_attribute *attr,
	  const char *buf, size_t count)
{
	size_t r;
	int value = 0;

	pr_info("%s: cmd: %s\n", __func__, buf);
	r = kstrtoint(buf, 0, &value);
	if (r != 0)
		return -EINVAL;

	set_operate_mode(value);
	return count;
}

static ssize_t amdolby_vision_graphic_fmt_show
	 (struct class *cla,
	  struct class_attribute *attr,
	  char *buf)
{
	ssize_t len = 0;
	int fmt;

	fmt = graphic_fmt + 1;
	if (fmt < 0 ||
	    fmt >= sizeof(signal_format_str) /
	    sizeof(signal_format_str[0]))
		fmt = 0;
	len += sprintf(buf + len, "%s\n",
		       signal_format_str[fmt]);

	return len;
}

static ssize_t amdolby_vision_graphic_fmt_store
	 (struct class *cla,
	  struct class_attribute *attr,
	  const char *buf, size_t count)
{
	size_t r;
	int value = 0;

	pr_info("set graphic_fmt: %s\n", buf);
	r = kstrtoint(buf, 0, &value);
	if (r != 0)
		return -EINVAL;
	graphic_fmt = value;
	return count;
}

static ssize_t amdolby_vision_graphic_md_show
	 (struct class *cla,
	  struct class_attribute *attr,
	  char *buf)
{
	int i = 0;

	if (graphic_md_size > 0 && graphic_md_buf) {
		for (i = 0; i < graphic_md_size; i += 8) {
			pr_info("%02x %02x %02x %02x %02x %02x %02x %02x\n",
				graphic_md_buf[i],
				graphic_md_buf[i + 1],
				graphic_md_buf[i + 2],
				graphic_md_buf[i + 3],
				graphic_md_buf[i + 4],
				graphic_md_buf[i + 5],
				graphic_md_buf[i + 6],
				graphic_md_buf[i + 7]);
		}
	} else {
		pr_info("no graphic md!\n");
	}
	return 0;
}

static ssize_t amdolby_vision_graphic_md_store
	(struct class *cla,
	 struct class_attribute *attr,
	 const char *buf, size_t count)
{
	char *buf_orig, *parm[MAX_PARAM] = {NULL};
	struct file *filp = NULL;

	loff_t pos = 0;
	mm_segment_t old_fs = get_fs();
	struct kstat stat;

	if (!buf)
		return -EFAULT;

	if (!module_installed || !graphic_md_buf)
		return -EAGAIN;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	parse_param_amdv(buf_orig, (char **)&parm);
	if (!parm[0]) {
		pr_info("missing parameter.. param1:graphic md file\n");
		kfree(buf_orig);
		return -EINVAL;
	}
	pr_info("parm[0]: %s\n", parm[0]);

	set_fs(KERNEL_DS);
	filp = filp_open(parm[0], O_RDONLY, 0444);
	if (IS_ERR(filp)) {
		pr_info("failed to open file: |%s|\n", parm[0]);
		goto LOAD_END;
	}
	if (vfs_stat(parm[0], &stat) == 0) {
		if (stat.size > MD_BUF_SIZE) {
			graphic_md_size = MD_BUF_SIZE;
			pr_err("graphic md file %lld > %d\n", stat.size, MD_BUF_SIZE);
		} else {
			graphic_md_size = stat.size;
		}
		vfs_read(filp, graphic_md_buf, graphic_md_size, &pos);
	}
	filp_close(filp, NULL);

LOAD_END:
	set_fs(old_fs);
	kfree(buf_orig);
	return count;
}

static ssize_t amdolby_vision_dual_layer_show
	 (struct class *cla,
	  struct class_attribute *attr,
	  char *buf)
{
	return sprintf(buf, "%d\n", dv_dual_layer);
}

static ssize_t amdolby_vision_dual_layer_store
	 (struct class *cla,
	  struct class_attribute *attr,
	  const char *buf, size_t count)
{
	size_t r;
	int value = 0;

	pr_info("set dual layer: %s\n", buf);
	r = kstrtoint(buf, 0, &value);
	if (r != 0)
		return -EINVAL;
	dv_dual_layer = value;
	pr_info("current dual layer is %d\n", value);
	return count;
}

static int amdv_notify_callback(struct notifier_block *block,
	unsigned long cmd,
	void *para)
{
	u32 *p, val;

	switch (cmd) {
	case AMDV_UPDATE_OSD_MODE:
		p = (u32 *)para;
		if (!update_mali_top_ctrl) {
			mali_afbcd_top_ctrl_mask = p[1];
			mali_afbcd1_top_ctrl_mask = p[3];
		}
		val = mali_afbcd_top_ctrl
			& (~mali_afbcd_top_ctrl_mask);
		val |= (p[0] & mali_afbcd_top_ctrl_mask);
		mali_afbcd_top_ctrl = val;

		val = mali_afbcd1_top_ctrl
			& (~mali_afbcd1_top_ctrl_mask);
		val |= (p[2] & mali_afbcd1_top_ctrl_mask);
		mali_afbcd1_top_ctrl = val;

		if (!update_mali_top_ctrl)
			update_mali_top_ctrl = true;
		break;
	default:
		break;
	}
	return 0;
}

static struct notifier_block amdv_notifier = {
	.notifier_call = amdv_notify_callback,
};

static RAW_NOTIFIER_HEAD(amdv_notifier_list);
int amdv_register_client(struct notifier_block *nb)
{
	return raw_notifier_chain_register(&amdv_notifier_list, nb);
}
EXPORT_SYMBOL(amdv_register_client);

int amdv_unregister_client(struct notifier_block *nb)
{
	return raw_notifier_chain_unregister(&amdv_notifier_list, nb);
}
EXPORT_SYMBOL(amdv_unregister_client);

int amdv_notifier_call_chain(unsigned long val, void *v)
{
	return raw_notifier_call_chain(&amdv_notifier_list, val, v);
}
EXPORT_SYMBOL_GPL(amdv_notifier_call_chain);

static struct class_attribute amdolby_vision_class_attrs[] = {
	__ATTR(debug, 0644,
	       amdolby_vision_debug_show,
	       amdolby_vision_debug_store),
	__ATTR(dv_mode, 0644,
	       amdolby_vision_dv_mode_show,
	       amdolby_vision_dv_mode_store),
	__ATTR(dv_reg, 0220,
	       NULL, amdolby_vision_reg_store),
	__ATTR(core1_switch, 0644,
	       amdolby_vision_core1_switch_show,
	       amdolby_vision_core1_switch_store),
	__ATTR(core3_switch, 0644,
	       amdolby_vision_core3_switch_show,
	       amdolby_vision_core3_switch_store),
	__ATTR(dv_bin_config, 0644,
	       amdolby_vision_bin_config_show, NULL),
	__ATTR(dv_pq_info, 0644,
	       amdolby_vision_pq_info_show,
	       amdolby_vision_pq_info_store),
	__ATTR(dv_use_inter_pq, 0644,
	       amdolby_vision_use_inter_pq_show,
	       amdolby_vision_use_inter_pq_store),
	__ATTR(dv_load_cfg_status, 0644,
	       amdolby_vision_load_cfg_status_show,
	       amdolby_vision_load_cfg_status_store),
	__ATTR(support_info, 0444,
	       amdolby_vision_dv_support_info_show, NULL),
	__ATTR(tprimary, 0644,
	       amdolby_vision_primary_show,
	       amdolby_vision_primary_store),
	__ATTR(config_file, 0644,
	       amdolby_vision_config_file_show,
	       amdolby_vision_config_file_store),
	__ATTR(copy_core1a, 0644,
	       amdolby_vision_copy_core1a_show,
	       amdolby_vision_copy_core1a_store),
	__ATTR(core2_sel, 0644,
	       amdolby_vision_core2_sel_show,
	       amdolby_vision_core2_sel_store),
	__ATTR(dv_provider, 0644,
	       amdolby_vision_dv_provider_show,
	       NULL),
	__ATTR(content_fps, 0644,
	       amdolby_vision_content_fps_show,
	       amdolby_vision_content_fps_store),
	__ATTR(gd_rf_adjust, 0644,
	       amdolby_vision_gd_rf_adjust_show,
	       amdolby_vision_gd_rf_adjust_store),
	__ATTR(cur_crc, 0644,
	       amdolby_vision_crc_show,
	       NULL),
	__ATTR(force_disable_dv_backlight, 0644,
	       amdolby_vision_force_disable_bl_show,
	       amdolby_vision_force_disable_bl_store),
	__ATTR(use_target_lum_from_cfg, 0644,
	       amdolby_vision_use_cfg_target_lum_show,
	       amdolby_vision_use_cfg_target_lum_store),
	__ATTR(src_format, 0444,
	       amdolby_vision_src_format_show, NULL),
	__ATTR(force_priority, 0644,
	       amdolby_vision_force_priority_show,
	       amdolby_vision_force_priority_store),
	__ATTR(dv_inst_status, 0644,
	       amdolby_vision_inst_status_show,
	       NULL),
	__ATTR(dv_core1_detunnel, 0644,
	       amdolby_vision_core1_detunnel_show,
	       amdolby_vision_core1_detunnel_store),
	__ATTR(operate_mode, 0644,
	       amdolby_vision_operate_mode_show,
	       amdolby_vision_operate_mode_store),
	__ATTR(graphic_fmt, 0644,
	       amdolby_vision_graphic_fmt_show,
	       amdolby_vision_graphic_fmt_store),
	__ATTR(graphic_md_file, 0644,
	       amdolby_vision_graphic_md_show,
	       amdolby_vision_graphic_md_store),
	__ATTR(brightness_off, 0644,
	       amdolby_vision_brightness_off_show,
	       NULL),
	__ATTR(enable_multi_core1, 0644,
	       amdolby_vision_enable_multi_core1_show,
	       amdolby_vision_enable_multi_core1_store),
	__ATTR(force_pri_input, 0644,
	       amdolby_vision_force_pri_input_show,
	       amdolby_vision_force_pri_input_store),
	__ATTR(hdmi_in_allm, 0644,
	       amdolby_vision_hdmi_in_allm_show,
	       amdolby_vision_hdmi_in_allm_store),
	__ATTR(local_allm, 0644,
	       amdolby_vision_local_allm_show,
	       amdolby_vision_local_allm_store),
	__ATTR(inst_res_debug, 0644,
	       amdolby_vision_inst_debug_show,
	       amdolby_vision_inst_debug_store),
	__ATTR(dv_dual_layer, 0644,
	       amdolby_vision_dual_layer_show,
		   amdolby_vision_dual_layer_store),
	__ATTR_NULL
};

#ifndef CONFIG_AMLOGIC_REMOVE_OLD
static struct dv_device_data_s dolby_vision_gxm = {
	.cpu_id = _CPU_MAJOR_ID_GXM,
};

static struct dv_device_data_s dolby_vision_txlx = {
	.cpu_id = _CPU_MAJOR_ID_TXLX,
};
#endif

static struct dv_device_data_s dolby_vision_g12 = {
	.cpu_id = _CPU_MAJOR_ID_G12,
};

static struct dv_device_data_s dolby_vision_tm2 = {
	.cpu_id = _CPU_MAJOR_ID_TM2,
};

static struct dv_device_data_s dolby_vision_tm2_revb = {
	.cpu_id = _CPU_MAJOR_ID_TM2_REVB,
};

static struct dv_device_data_s dolby_vision_sc2 = {
	.cpu_id = _CPU_MAJOR_ID_SC2,
};

static struct dv_device_data_s dolby_vision_t7 = {
	.cpu_id = _CPU_MAJOR_ID_T7,
};

static struct dv_device_data_s dolby_vision_t3 = {
	.cpu_id = _CPU_MAJOR_ID_T3,
};

static struct dv_device_data_s dolby_vision_s4d = {
	.cpu_id = _CPU_MAJOR_ID_S4D,
};

static struct dv_device_data_s dolby_vision_t5w = {
	.cpu_id = _CPU_MAJOR_ID_T5W,
};

static struct dv_device_data_s dolby_vision_s5 = {
	.cpu_id = _CPU_MAJOR_ID_S5,
};

static const struct of_device_id amlogic_dolby_vision_match[] = {
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
	{
		.compatible = "amlogic, dolby_vision_gxm",
		.data = &dolby_vision_gxm,
	},
	{
		.compatible = "amlogic, dolby_vision_txlx",
		.data = &dolby_vision_txlx,
	},
#endif
	{
		.compatible = "amlogic, dolby_vision_g12a",
		.data = &dolby_vision_g12,
	},
	{
		.compatible = "amlogic, dolby_vision_g12b",
		.data = &dolby_vision_g12,
	},
	{
		.compatible = "amlogic, dolby_vision_sm1",
		.data = &dolby_vision_g12,
	},
	{
		.compatible = "amlogic, dolby_vision_tm2",
		.data = &dolby_vision_tm2,
	},
	{
		.compatible = "amlogic, dolby_vision_tm2_revb",
		.data = &dolby_vision_tm2_revb,
	},
	{
		.compatible = "amlogic, dolby_vision_sc2",
		.data = &dolby_vision_sc2,
	},
	{
		.compatible = "amlogic, dolby_vision_t7",
		.data = &dolby_vision_t7,
	},
	{
		.compatible = "amlogic, dolby_vision_t3",
		.data = &dolby_vision_t3,
	},
	{
		.compatible = "amlogic, dolby_vision_s4d",
		.data = &dolby_vision_s4d,
	},
	{
		.compatible = "amlogic, dolby_vision_t5w",
		.data = &dolby_vision_t5w,
	},
	{
		.compatible = "amlogic, dolby_vision_s5",
		.data = &dolby_vision_s5,
	},
	{},
};

void set_vsync_count(int val)
{
	vsync_count = val;
}

static int amdolby_vision_probe(struct platform_device *pdev)
{
	int ret = 0;
	int i = 0;
	struct amdolby_vision_dev_s *devp = &amdolby_vision_dev;
	unsigned int val;

	pr_info("\n amdolby_vision probe start & ver: %s\n", DRIVER_VER);
	if (pdev->dev.of_node) {
		const struct of_device_id *match;
		struct dv_device_data_s *dv_meson;
		struct device_node *of_node = pdev->dev.of_node;

		match = of_match_node(amlogic_dolby_vision_match, of_node);
		if (match) {
			dv_meson = (struct dv_device_data_s *)match->data;
			if (dv_meson) {
				memcpy(&dv_meson_dev, dv_meson,
				       sizeof(struct dv_device_data_s));
			} else {
				pr_err("%s data NOT match\n", __func__);
				dolby_vision_probe_ok = 0;
				return -ENODEV;
			}
		} else {
			pr_err("%s NOT match\n", __func__);
			dolby_vision_probe_ok = 0;
			return -ENODEV;
		}
		ret = of_property_read_u32(of_node, "tv_mode", &val);
		if (ret)
			pr_info("Can't find  tv_mode.\n");
		else
			tv_mode = val;
		if (tv_mode)
			support_info |= 1 << 3;

		ret = of_property_read_u32(of_node, "multi_core1", &val);
		if (ret)
			pr_info("Can't find multi_core1.\n");
		else
			enable_multi_core1 = val;
	}
	pr_info("\n cpu_id=%d tvmode=%d, enable multi_core1 %d\n",
		dv_meson_dev.cpu_id, tv_mode, enable_multi_core1);
	memset(devp, 0, (sizeof(struct amdolby_vision_dev_s)));
	if (is_aml_tvmode()) {
		dolby_vision_flags |= FLAG_RX_EMP_VSEM;
		pr_info("enable DV VSEM.\n");
	}
	ret = alloc_chrdev_region(&devp->devno, 0, 1, AMDOLBY_VISION_NAME);
	if (ret < 0)
		goto fail_alloc_region;
	devp->clsp = class_create(THIS_MODULE,
		AMDOLBY_VISION_CLASS_NAME);
	if (IS_ERR(devp->clsp)) {
		ret = PTR_ERR(devp->clsp);
		goto fail_create_class;
	}
	for (i = 0;  amdolby_vision_class_attrs[i].attr.name; i++) {
		if (class_create_file(devp->clsp,
			&amdolby_vision_class_attrs[i]) < 0)
			goto fail_class_create_file;
	}
	cdev_init(&devp->cdev, &amdolby_vision_fops);
	devp->cdev.owner = THIS_MODULE;
	ret = cdev_add(&devp->cdev, devp->devno, 1);
	if (ret)
		goto fail_add_cdev;

	devp->dev = device_create(devp->clsp, NULL, devp->devno,
			NULL, AMDOLBY_VISION_NAME);
	if (IS_ERR(devp->dev)) {
		ret = PTR_ERR(devp->dev);
		goto fail_create_device;
	}
	amdv_addr();
	amdv_init_receiver(pdev);
	amdv_create_inst();
	init_waitqueue_head(&devp->dv_queue);
	pr_info("%s: ok\n", __func__);
	amdv_check_enable();
	dolby_vision_probe_ok = 1;
	return 0;

fail_create_device:
	pr_info("[amdolby_vision.] : amdolby_vision device create error.\n");
	cdev_del(&devp->cdev);
fail_add_cdev:
	pr_info("[amdolby_vision.] : amdolby_vision add device error.\n");
fail_class_create_file:
	pr_info("[amdolby_vision.] : amdolby_vision class create file error.\n");
	for (i = 0; amdolby_vision_class_attrs[i].attr.name; i++) {
		class_remove_file(devp->clsp,
		&amdolby_vision_class_attrs[i]);
	}
	class_destroy(devp->clsp);
fail_create_class:
	pr_info("[amdolby_vision.] : amdolby_vision class create error.\n");
	unregister_chrdev_region(devp->devno, 1);
fail_alloc_region:
	pr_info("[amdolby_vision.] : amdolby_vision alloc error.\n");
	pr_info("[amdolby_vision.] : amdolby_vision_init.\n");
	dolby_vision_probe_ok = 0;
	return ret;
}

static int __exit amdolby_vision_remove(struct platform_device *pdev)
{
	struct amdolby_vision_dev_s *devp = &amdolby_vision_dev;
	int i;

	if (pq_config_buf) {
		vfree(pq_config_buf);
		pq_config_buf = NULL;
	}
	for (i = 0; i < 2; i++) {
		if (md_buf[i]) {
			vfree(md_buf[i]);
			md_buf[i] = NULL;
		}
		if (comp_buf[i]) {
			vfree(comp_buf[i]);
			comp_buf[i] = NULL;
		}
		if (drop_md_buf[i]) {
			vfree(drop_md_buf[i]);
			drop_md_buf[i] = NULL;
		}
		if (drop_comp_buf[i]) {
			vfree(drop_comp_buf[i]);
			drop_comp_buf[i] = NULL;
		}
	}
	if (vsem_if_buf) {
		vfree(vsem_if_buf);
		vsem_if_buf = NULL;
	}
	if (vsem_md_buf) {
		vfree(vsem_md_buf);
		vsem_md_buf = NULL;
	}

	for (i = 0; i < NUM_INST; i++) {
		if (dv_inst[i].metadata_parser) {
			p_funcs_stb->multi_mp_release
				(&dv_inst[i].metadata_parser);
			dv_inst[i].metadata_parser = NULL;
		}
		if (dv_inst[i].md_buf[0]) {
			vfree(dv_inst[i].md_buf[0]);
			dv_inst[i].md_buf[0] = NULL;
		}
		if (dv_inst[i].comp_buf[0]) {
			vfree(dv_inst[i].comp_buf[0]);
			dv_inst[i].comp_buf[0] = NULL;
		}
		if (dv_inst[i].md_buf[1]) {
			vfree(dv_inst[i].md_buf[1]);
			dv_inst[i].md_buf[1] = NULL;
		}
		if (dv_inst[i].comp_buf[1]) {
			vfree(dv_inst[i].comp_buf[1]);
			dv_inst[i].comp_buf[1] = NULL;
		}
	}
	if (graphic_md_buf) {
		vfree(graphic_md_buf);
		graphic_md_buf = NULL;
	}

	device_destroy(devp->clsp, devp->devno);
	cdev_del(&devp->cdev);
	class_destroy(devp->clsp);
	unregister_chrdev_region(devp->devno, 1);
	pr_info("[ amdolby_vision.] :  amdolby_vision_exit.\n");
	return 0;
}

static struct platform_driver aml_amdolby_vision_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "aml_amdolby_vision_driver",
		.of_match_table = amlogic_dolby_vision_match,
	},
	.probe = amdolby_vision_probe,
	.remove = __exit_p(amdolby_vision_remove),
};

static int get_amdv_uboot_status(char *str)
{
	char uboot_dolby_status[DV_NAME_LEN_MAX] = {0};
	amdv_uboot_on = *str - '0';

	snprintf(uboot_dolby_status, DV_NAME_LEN_MAX, "%s", str);
	pr_info("get_amdv_on: %s\n", uboot_dolby_status);

	if (!strcmp(uboot_dolby_status, "1") ||
		!strcmp(uboot_dolby_status, "2") ||
		!strcmp(uboot_dolby_status, "3") ||
		!strcmp(uboot_dolby_status, "4")) {
		amdv_on_in_uboot = 1;
		dolby_vision_enable = 1;
	}

	return 0;
}

__setup("dolby_vision_on=", get_amdv_uboot_status);

static int recovery_mode_check(char *str)
{
	char recovery_status[DV_NAME_LEN_MAX] = {0};

	snprintf(recovery_status, DV_NAME_LEN_MAX, "%s", str);
	pr_info("recovery_status: %s\n", recovery_status);

	if (strlen(recovery_status) > 0)
		recovery_mode = true;
	return 0;
}
__setup("recovery_part=", recovery_mode_check);

int __init amdolby_vision_init(void)
{
	pr_info("%s:module init\n", __func__);
	if (platform_driver_register(&aml_amdolby_vision_driver)) {
		pr_err("failed to register amdolby_vision module\n");
		return -ENODEV;
	}
	amdv_register_client(&amdv_notifier);
	return 0;
}

void __exit amdolby_vision_exit(void)
{
	pr_info("%s:module exit\n", __func__);
	amdv_unregister_client(&amdv_notifier);
	platform_driver_unregister(&aml_amdolby_vision_driver);
}

//MODULE_DESCRIPTION("AMLOGIC amdolby_vision driver");
//MODULE_LICENSE("GPL");

