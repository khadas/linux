#include <linux/string.h>
#include <linux/spinlock.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/amlogic/amports/amstream.h>
#include <linux/amlogic/amvecm/ve.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/amports/vframe.h>
#include <linux/amlogic/amvecm/amvecm.h>
#include <linux/amlogic/vout/vinfo.h>
#include <linux/amlogic/vout/vout_notify.h>
#include "arch/vpp_regs.h"
#include "arch/vpp_dolbyvision_regs.h"
#include "bitdepth.h"


/*u2s_mode:0:true 12bit;1:false 12bit*/
static unsigned int u2s_mode;
module_param(u2s_mode, uint, 0664);
MODULE_PARM_DESC(u2s_mode, "\n u2s_mode\n");

/*u2s_mode:0:bypass;1:enable*/
static unsigned int dolby_core1_en;
module_param(dolby_core1_en, uint, 0664);
MODULE_PARM_DESC(dolby_core1_en, "\n dolby_core1_en\n");

static unsigned int dolby_core2_en;
module_param(dolby_core2_en, uint, 0664);
MODULE_PARM_DESC(dolby_core2_en, "\n dolby_core2_en\n");

static unsigned int vd2_en;
module_param(vd2_en, uint, 0664);
MODULE_PARM_DESC(vd2_en, "\n vd2_en\n");

static unsigned int osd2_en;
module_param(osd2_en, uint, 0664);
MODULE_PARM_DESC(osd2_en, "\n osd2_en\n");

static unsigned int dolby_core1_ext_mode;
module_param(dolby_core1_ext_mode, uint, 0664);
MODULE_PARM_DESC(dolby_core1_ext_mode, "\n dolby_core1_ext_mode\n");

static unsigned int dolby_core2_ext_mode;
module_param(dolby_core2_ext_mode, uint, 0664);
MODULE_PARM_DESC(dolby_core2_ext_mode, "\n dolby_core2_ext_mode\n");

static unsigned int vd2_ext_mode;
module_param(vd2_ext_mode, uint, 0664);
MODULE_PARM_DESC(vd2_ext_mode, "\n vd2_ext_mode\n");

static unsigned int osd2_ext_mode;
module_param(osd2_ext_mode, uint, 0664);
MODULE_PARM_DESC(osd2_ext_mode, "\n osd2_ext_mode\n");

/**/
static unsigned int vpp_dpath_sel0;
module_param(vpp_dpath_sel0, uint, 0664);
MODULE_PARM_DESC(vpp_dpath_sel0, "\n vpp_dpath_sel0\n");

static unsigned int vpp_dpath_sel1;
module_param(vpp_dpath_sel1, uint, 0664);
MODULE_PARM_DESC(vpp_dpath_sel1, "\n vpp_dpath_sel1\n");

static unsigned int vpp_dpath_sel2;
module_param(vpp_dpath_sel2, uint, 0664);
MODULE_PARM_DESC(vpp_dpath_sel2, "\n vpp_dpath_sel2\n");

static unsigned int vpp_dolby3_en;
module_param(vpp_dolby3_en, uint, 0664);
MODULE_PARM_DESC(vpp_dolby3_en, "\n vpp_dolby3_en\n");

static unsigned int core1_input_data_conv_mode;
module_param(core1_input_data_conv_mode, uint, 0664);
MODULE_PARM_DESC(core1_input_data_conv_mode, "\n core1_input_data_conv_mode\n");

static unsigned int core1_output_data_conv_mode;
module_param(core1_output_data_conv_mode, uint, 0664);
MODULE_PARM_DESC(core1_output_data_conv_mode, "\n core1_output_data_conv_mode\n");

static unsigned int vd1_input_clip_mode;
module_param(vd1_input_clip_mode, uint, 0664);
MODULE_PARM_DESC(vd1_input_clip_mode, "\n vd1_input_clip_mode\n");

static unsigned int vd2_input_clip_mode;
module_param(vd2_input_clip_mode, uint, 0664);
MODULE_PARM_DESC(vd2_input_clip_mode, "\n vd2_input_clip_mode\n");

static unsigned int vpp_output_clip_mode;
module_param(vpp_output_clip_mode, uint, 0664);
MODULE_PARM_DESC(vpp_output_clip_mode, "\n vpp_output_clip_mode\n");

static unsigned int vpp_dith_en;
module_param(vpp_dith_en, uint, 0664);
MODULE_PARM_DESC(vpp_dith_en, "\n vpp_dith_en\n");

/*U12toU10:1:cut low 2bit;0:cut high 2bit*/
static unsigned int vpp_dith_mode;
module_param(vpp_dith_mode, uint, 0664);
MODULE_PARM_DESC(vpp_dith_mode, "\n vpp_dith_mode\n");

static unsigned int dolby3_path_sel;
module_param(dolby3_path_sel, uint, 0664);
MODULE_PARM_DESC(dolby3_path_sel, "\n dolby3_path_sel\n");

/*u2s: scaler:>>n;offset:-n*/
void vpp_set_pre_u2s(enum data_conv_mode_e conv_mode)
{
	/*U12-->U10*/
	if (conv_mode == U12_TO_U10)
		WRITE_VPP_REG_BITS(VPP_DAT_CONV_PARA0, 0x2000, 16, 14);
	/*U12-->S12*/
	else if (conv_mode == U12_TO_S12)
		WRITE_VPP_REG_BITS(VPP_DAT_CONV_PARA0, 0x800, 16, 14);
	else
		WRITE_VPP_REG_BITS(VPP_DAT_CONV_PARA0, 0, 16, 14);
}
void vpp_set_post_u2s(enum data_conv_mode_e conv_mode)
{
	/*U12-->U10*/
	if (conv_mode == U12_TO_U10)
		WRITE_VPP_REG_BITS(VPP_DAT_CONV_PARA1, 0x2000, 16, 14);
	/*U12-->S12*/
	else if (conv_mode == U12_TO_S12)
		WRITE_VPP_REG_BITS(VPP_DAT_CONV_PARA1, 0x800, 16, 14);
	else
		WRITE_VPP_REG_BITS(VPP_DAT_CONV_PARA1, 0, 16, 14);
}
/*s2u: scaler:<<n;offset:+n*/
void vpp_set_pre_s2u(enum data_conv_mode_e conv_mode)
{
	/*U10-->U12*/
	if (conv_mode == U10_TO_U12)
		WRITE_VPP_REG_BITS(VPP_DAT_CONV_PARA0, 0x2000, 0, 14);
	/*S12-->U12*/
	else if (conv_mode == S12_TO_U12)
		WRITE_VPP_REG_BITS(VPP_DAT_CONV_PARA0, 0x800, 0, 14);
	else
		WRITE_VPP_REG_BITS(VPP_DAT_CONV_PARA0, 0, 0, 14);
}
void vpp_set_post_s2u(enum data_conv_mode_e conv_mode)
{
	/*U10-->U12*/
	if (conv_mode == U10_TO_U12)
		WRITE_VPP_REG_BITS(VPP_DAT_CONV_PARA1, 0x2000, 0, 14);
	/*S12-->U12*/
	else if (conv_mode == S12_TO_U12)
		WRITE_VPP_REG_BITS(VPP_DAT_CONV_PARA1, 0x800, 0, 14);
	else
		WRITE_VPP_REG_BITS(VPP_DAT_CONV_PARA1, 0, 0, 14);
}
/*skip:1-->skip;0-->un skip*/
void vpp_skip_pps(bool skip)
{
	WRITE_VPP_REG_BITS(VPP_DOLBY_CTRL, skip, 0, 1);
}
void vpp_skip_eotf_matrix3(bool skip)
{
	WRITE_VPP_REG_BITS(VPP_DOLBY_CTRL, skip, 1, 1);
}
void vpp_skip_vadj2_matrix3(bool skip)
{
	WRITE_VPP_REG_BITS(VPP_DOLBY_CTRL, skip, 2, 1);
}
/*bypass:1-->bypass;0-->un bypass*/
void vpp_bypas_core1(bool bypass)
{
	WRITE_VPP_REG_BITS(VIU_MISC_CTRL1, bypass, 16, 1);
}
void vpp_bypas_core2(bool bypass)
{
	WRITE_VPP_REG_BITS(VIU_MISC_CTRL1, bypass, 18, 1);
}
/*enable:1-->enable;0-->un enable*/
void vpp_enable_vd2(bool enable)
{
	WRITE_VPP_REG_BITS(VIU_MISC_CTRL1, enable, 17, 1);
}
void vpp_enable_osd2(bool enable)
{
	WRITE_VPP_REG_BITS(VIU_MISC_CTRL1, enable, 19, 1);
}
/*
extend mode:
0:extend 2bit 0 in low bits
1:extend 2bit 0 in high bits
*/
void vpp_extend_mode_core1(bool mode)
{
	WRITE_VPP_REG_BITS(VIU_MISC_CTRL1, mode, 20, 1);
}
void vpp_extend_mode_core2(bool mode)
{
	WRITE_VPP_REG_BITS(VIU_MISC_CTRL1, mode, 22, 1);
}
void vpp_extend_mode_vd2(bool mode)
{
	WRITE_VPP_REG_BITS(VIU_MISC_CTRL1, mode, 21, 1);
}
void vpp_extend_mode_osd2(bool mode)
{
	WRITE_VPP_REG_BITS(VIU_MISC_CTRL1, mode, 23, 1);
}
/**/
void vpp_vd1_if_bits_mode(enum vd_if_bits_mode_e bits_mode)
{
	WRITE_VPP_REG_BITS(VD1_IF0_GEN_REG3, (bits_mode & 0x3), 8, 2);
}
void vpp_vd2_if_bits_mode(enum vd_if_bits_mode_e bits_mode)
{
	WRITE_VPP_REG_BITS(VD2_IF0_GEN_REG3, (bits_mode & 0x3), 8, 2);
}
void vpp_enable_dither(bool enable)
{
	WRITE_VPP_REG_BITS(VPP_DOLBY_CTRL, enable, 12, 1);
}
/*mode:0:cut high 2 bits;1:cut low 2 bits*/
void vpp_dither_bits_comp_mode(bool mode)
{
	WRITE_VPP_REG_BITS(VPP_DOLBY_CTRL, mode, 14, 1);
}

void vpp_set_12bit_datapath1(void)
{
	/*after this step output data is U10*/
	vpp_vd1_if_bits_mode(BIT_MODE_10BIT_422);
	vpp_vd2_if_bits_mode(BIT_MODE_10BIT_422);

	/*after this step output data is U12*/
	vpp_extend_mode_core1(EXTMODE_LOW);
	vpp_extend_mode_vd2(EXTMODE_LOW);
	vpp_bypas_core1(1);
	/* vpp_enable_vd2(0); */

	/*don't skip pps & super scaler*/
	vpp_skip_pps(0);
	vpp_set_pre_u2s(U_TO_S_NULL);

	/*enable vpp dither after preblend,
	after this step output data is U10*/
	vpp_dither_bits_comp_mode(1);
	vpp_enable_dither(1);

	/*after this step output data is U12*/
	vpp_set_pre_s2u(U10_TO_U12);

	/*don't skip vadj2 & post_matrix & wb*/
	vpp_set_post_u2s(U12_TO_S12);
	vpp_skip_vadj2_matrix3(0);

	/*skip eotf & oetf*/
	vpp_skip_eotf_matrix3(1);
	vpp_set_post_s2u(S12_TO_U12);
}

/*  u12 to postblending, u10 to post matirx */
void vpp_set_12bit_datapath2(void)
{
	/*after this step output data is U10*/
	vpp_vd1_if_bits_mode(BIT_MODE_10BIT_422);
	vpp_vd2_if_bits_mode(BIT_MODE_10BIT_422);

	/*after this step output data is U12*/
	vpp_extend_mode_core1(EXTMODE_LOW);
	vpp_extend_mode_vd2(EXTMODE_LOW);
	vpp_bypas_core1(1);
	/* vpp_enable_vd2(0); */

	/*don't skip pps & super scaler*/
	vpp_skip_pps(0);
	vpp_set_pre_u2s(U_TO_S_NULL);

	/*enable vpp dither after preblend,
	after this step output data is U10*/
	vpp_dither_bits_comp_mode(1);
	vpp_enable_dither(1);

	/*after this step output data is U12*/
	vpp_set_pre_s2u(U10_TO_U12);

	/*don't skip vadj2 & post_matrix & wb*/
	vpp_set_post_u2s(U12_TO_U10);
	vpp_skip_vadj2_matrix3(0);

	/*skip eotf & oetf*/
	vpp_skip_eotf_matrix3(0);
	vpp_set_post_s2u(U_TO_S_NULL);
}

void vpp_set_10bit_datapath1(void)
{
	/*after this step output data is U10*/
	vpp_vd1_if_bits_mode(BIT_MODE_10BIT_422);
	vpp_vd2_if_bits_mode(BIT_MODE_10BIT_422);

	/*after this step output data is U12*/
	vpp_extend_mode_core1(EXTMODE_LOW);
	vpp_extend_mode_vd2(EXTMODE_LOW);
	vpp_bypas_core1(1);
	/* vpp_enable_vd2(0); */

	/*don't skip pps & super scaler*/
	vpp_skip_pps(0);
	vpp_set_pre_u2s(U_TO_S_NULL);

	/*enable vpp dither after preblend,
	after this step output data is U10*/
	vpp_dither_bits_comp_mode(1);
	vpp_enable_dither(1);

	/*after this step output data is U10*/
	vpp_set_pre_s2u(U_TO_S_NULL);

	/*don't skip vadj2 & post_matrix & wb*/
	vpp_set_post_u2s(U_TO_S_NULL);
	vpp_skip_vadj2_matrix3(0);

	/*don't skip eotf & oetf*/
	vpp_skip_eotf_matrix3(0);
	vpp_set_post_s2u(U_TO_S_NULL);
}

void  vpp_set_datapath(void)
{
	unsigned int w_data;

	WRITE_VPP_REG_BITS(VIU_MISC_CTRL1, dolby_core1_en, 16, 1);
	WRITE_VPP_REG_BITS(VIU_MISC_CTRL1, vd2_en, 17, 1);
	WRITE_VPP_REG_BITS(VIU_MISC_CTRL1, dolby_core2_en, 18, 1);
	WRITE_VPP_REG_BITS(VIU_MISC_CTRL1, osd2_en, 19, 1);

	WRITE_VPP_REG_BITS(VIU_MISC_CTRL1, dolby_core1_ext_mode, 20, 1);
	WRITE_VPP_REG_BITS(VIU_MISC_CTRL1, vd2_ext_mode, 21, 1);
	WRITE_VPP_REG_BITS(VIU_MISC_CTRL1, dolby_core2_ext_mode, 22, 1);
	WRITE_VPP_REG_BITS(VIU_MISC_CTRL1, osd2_ext_mode, 23, 1);

	/*if oetf_osd bypass ,the output can be 10/12bits
	*bit3:1:10bit;0:12bit*/
	/*WRITE_VPP_REG_BITS(VIU_OSD1_CTRL_STAT, 0x0,  3, 1);*/

	/*config sr4 position:0:before postblend,1:after postblend*/
	WRITE_VPP_REG_BITS(VPP_VE_ENABLE_CTRL, 0, 5, 1);

	/*config vpp dolby contrl*/
	WRITE_VPP_REG_BITS(VPP_DOLBY_CTRL, vpp_dpath_sel0, 0, 1);
	WRITE_VPP_REG_BITS(VPP_DOLBY_CTRL, vpp_dpath_sel1, 1, 1);
	WRITE_VPP_REG_BITS(VPP_DOLBY_CTRL, vpp_dpath_sel2, 2, 1);
	WRITE_VPP_REG_BITS(VPP_DOLBY_CTRL, vpp_dolby3_en, 3, 1);
	WRITE_VPP_REG_BITS(VPP_DOLBY_CTRL, core1_input_data_conv_mode, 6, 1);
	WRITE_VPP_REG_BITS(VPP_DOLBY_CTRL, core1_output_data_conv_mode, 7, 1);
	WRITE_VPP_REG_BITS(VPP_DOLBY_CTRL, vd1_input_clip_mode, 8, 1);
	WRITE_VPP_REG_BITS(VPP_DOLBY_CTRL, vd2_input_clip_mode, 9, 1);
	WRITE_VPP_REG_BITS(VPP_DOLBY_CTRL, vpp_output_clip_mode, 10, 1);
	WRITE_VPP_REG_BITS(VPP_DOLBY_CTRL, vpp_dith_en, 12, 1);
	WRITE_VPP_REG_BITS(VPP_DOLBY_CTRL, vpp_dith_mode, 14, 1);
	WRITE_VPP_REG_BITS(VPP_DOLBY_CTRL, dolby3_path_sel, 16, 1);

	/*config u2s & s2u*/
	if (u2s_mode)
		w_data = (2 << 12) | (2 << 28);
	else
		w_data = (1 << 11) | ((1 << 11) << 16);

	WRITE_VPP_REG_BITS(VPP_DAT_CONV_PARA0, w_data, 0, 32);
	WRITE_VPP_REG_BITS(VPP_DAT_CONV_PARA1, w_data, 0, 32);

	/*config clipping after VKeystone*/
	WRITE_VPP_REG_BITS(VPP_CLIP_MISC0, 0xffffffff, 0, 32);
	WRITE_VPP_REG_BITS(VPP_CLIP_MISC1, 0x0, 0, 32);
}

void vpp_datapath_config(unsigned int bitdepth)
{
	if (bitdepth == 10)
		vpp_set_10bit_datapath1();
	else if (bitdepth == 12)
		vpp_set_12bit_datapath1();
	else
		vpp_set_datapath();
}

