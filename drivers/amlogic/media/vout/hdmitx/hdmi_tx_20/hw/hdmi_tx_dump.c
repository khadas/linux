// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/pwm.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/amlogic/media/vout/hdmi_tx/hdmi_common.h>
#include <linux/amlogic/media/vout/hdmi_tx/hdmi_tx_module.h>
#include <linux/amlogic/media/vout/hdmi_tx/hdmi_version.h>
#include "mach_reg.h"
#include "reg_ops.h"
#include "hdmi_tx_reg.h"

#define PR_BUS(a) \
	do { \
		typeof(a) addr = (a); \
		seq_printf(s, "[%08x][%04x] = %08x\n", \
			   TO_PHY_ADDR(addr), \
			   (TO_PHY_ADDR(addr) & 0xffff) >> 2, \
			   hd_read_reg(addr)); \
	} while (0)

static inline unsigned int get_msr_cts(void);

static int dump_regs_show(struct seq_file *s, void *p)
{
	int i;

	if (reg_maps[HHI_REG_IDX].phy_addr) {
		seq_puts(s, "\n--------HHI registers--------\n");
		for (i = 0; i < 0x80; i++)
			PR_BUS(HHI_REG_ADDR(i));
		for (i = 0x80; i < 0x100; i++)
			PR_BUS(HHI_REG_ADDR(i));
	}

	if (reg_maps[VCBUS_REG_IDX].phy_addr) {
		seq_puts(s, "\n--------ENCP registers--------\n");
		for (i = 0x1b00; i < 0x1b80; i++)
			PR_BUS(VCBUS_REG_ADDR(i));
		for (i = 0x1b80; i < 0x1c00; i++)
			PR_BUS(VCBUS_REG_ADDR(i));
		for (i = 0x1c00; i < 0x1c80; i++)
			PR_BUS(VCBUS_REG_ADDR(i));
		for (i = 0x1c80; i < 0x1d00; i++)
			PR_BUS(VCBUS_REG_ADDR(i));
		for (i = 0x2700; i < 0x2780; i++)
			PR_BUS(VCBUS_REG_ADDR(i));
		for (i = 0x2780; i < 0x2800; i++)
			PR_BUS(VCBUS_REG_ADDR(i));
	}

	if (reg_maps[CBUS_REG_IDX].phy_addr) {
		seq_puts(s, "\n--------CBUS registers--------\n");
		PR_BUS(P_AIU_HDMI_CLK_DATA_CTRL);
		PR_BUS(P_ISA_DEBUG_REG0);
	}

	if (reg_maps[ANACTRL_REG_IDX].phy_addr) {
		seq_puts(s, "\n--------ANACTRL registers--------\n");
		for (i = 0; i < 0xff; i++)
			PR_BUS(ANACTRL_REG_ADDR(i));
	}

	if (reg_maps[PWRCTRL_REG_IDX].phy_addr) {
		seq_puts(s, "\n--------PWRCTRL registers--------\n");
		for (i = 0; i < 0xff; i++)
			PR_BUS(PWRCTRL_REG_ADDR(i));
	}

	if (reg_maps[SYSCTRL_REG_IDX].phy_addr) {
		seq_puts(s, "\n--------SYSCTRL registers--------\n");
		for (i = 0; i < 0xff; i++)
			PR_BUS(SYSCTRL_REG_ADDR(i));
	}

	if (reg_maps[CLKCTRL_REG_IDX].phy_addr) {
		seq_puts(s, "\n--------CLKCTRL registers--------\n");
		for (i = 0; i < 0xff; i++)
			PR_BUS(CLKCTRL_REG_ADDR(i));
	}

	seq_puts(s, "\n");

	return 0;
}

static ssize_t dump_regs_write(struct file *file, const char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	/* TODO */
	return count;
}

static int dump_regs_open(struct inode *inode, struct file *file)
{
	return single_open(file, dump_regs_show, inode->i_private);
}

static const struct file_operations dump_busreg_fops = {
	.open		= dump_regs_open,
	.read		= seq_read,
	.write		= dump_regs_write,
	.release	= single_release,
};

#define DUMP_SECTION(_a, _b) \
	do { \
		typeof(_a) a = (_a); \
		typeof(_b) b = (_b); \
		for (reg_adr = (a); \
		     reg_adr < (b) + 1; reg_adr++) { \
			reg_val = hdmitx_rd_reg(reg_adr); \
			seq_printf(s, "[0x%x]: 0x%x\n", reg_adr, reg_val); \
		} \
	} while (0)

#define DUMP_HDCP_SECTION(_a, _b) \
	do { \
		typeof(_a) a = _a; \
		typeof(_b) b = _b; \
		for (reg_adr = (a); \
		     reg_adr < (b) + 1; reg_adr++) { \
			hdmitx_wr_reg(HDMITX_DWC_A_KSVMEMCTRL, 0x1); \
			hdmitx_poll_reg(HDMITX_DWC_A_KSVMEMCTRL, 1 << 1, \
					2 * HZ); \
			reg_val = hdmitx_rd_reg(reg_adr); \
			seq_printf(s, "[0x%x]: 0x%x\n", reg_adr, reg_val); \
		} \
	} while (0)

static int dump_hdmireg_show(struct seq_file *s, void *p)
{
	unsigned int reg_val = 0;
	unsigned int reg_adr = 0;

	seq_puts(s, "\n--------HDMITX registers--------\n");

	DUMP_SECTION(HDMITX_TOP_SW_RESET, HDMITX_TOP_STAT0);
	DUMP_SECTION(HDMITX_TOP_SKP_CNTL_STAT, HDMITX_TOP_DUK_3);
	DUMP_SECTION(HDMITX_TOP_INFILTER, HDMITX_TOP_NSEC_SCRATCH);
	DUMP_SECTION(HDMITX_TOP_SEC_SCRATCH, HDMITX_TOP_SEC_SCRATCH);
	DUMP_SECTION(HDMITX_TOP_EMP_CNTL0, HDMITX_TOP_I2C_BUSY_CNT_STAT);
	DUMP_SECTION(HDMITX_TOP_HDCP22_BSOD, HDMITX_TOP_HDCP22_BSOD);
	DUMP_SECTION(HDMITX_TOP_DDC_CNTL, HDMITX_TOP_HDCP22_MIN_SIZE);
	if (0)
		DUMP_HDCP_SECTION(HDMITX_DWC_HDCP_BSTATUS_0,
			HDMITX_DWC_HDCP_REVOC_LIST_END);
	DUMP_SECTION(HDMITX_DWC_DESIGN_ID, HDMITX_DWC_CONFIG3_ID);
	DUMP_SECTION(HDMITX_DWC_IH_FC_STAT0, HDMITX_DWC_IH_MUTE);
	DUMP_SECTION(HDMITX_DWC_TX_INVID0, HDMITX_DWC_TX_BCBDATA1);
	DUMP_SECTION(HDMITX_DWC_VP_STATUS, HDMITX_DWC_VP_MASK);
	DUMP_SECTION(HDMITX_DWC_FC_INVIDCONF, HDMITX_DWC_FC_DBGTMDS2);
	DUMP_SECTION(HDMITX_DWC_PHY_CONF0, HDMITX_DWC_I2CM_PHY_SDA_HOLD);
	DUMP_SECTION(HDMITX_DWC_AUD_CONF0, HDMITX_DWC_AUD_INT1);
	DUMP_SECTION(HDMITX_DWC_AUD_N1, HDMITX_DWC_AUD_INPUTCLKFS);
	DUMP_SECTION(HDMITX_DWC_AUD_SPDIF0, HDMITX_DWC_AUD_SPDIFINT1);
	DUMP_SECTION(HDMITX_DWC_MC_CLKDIS, HDMITX_DWC_MC_CLKDIS);
	DUMP_SECTION(HDMITX_DWC_MC_CLKDIS_SC2, HDMITX_DWC_MC_CLKDIS_SC2);
	DUMP_SECTION(HDMITX_DWC_MC_SWRSTZREQ, HDMITX_DWC_MC_LOCKONCLOCK);
	DUMP_SECTION(HDMITX_DWC_CSC_CFG, HDMITX_DWC_CSC_LIMIT_DN_LSB);
	DUMP_SECTION(HDMITX_DWC_A_HDCPCFG0, HDMITX_DWC_A_HDCPCFG1);
	DUMP_SECTION(HDMITX_DWC_A_HDCPOBS0, HDMITX_DWC_A_BSTATUS_LO);
	DUMP_SECTION(HDMITX_DWC_HDCPREG_BKSV0, HDMITX_DWC_HDCPREG_RMLSTS);
	DUMP_SECTION(HDMITX_DWC_HDCPREG_SEED0, HDMITX_DWC_HDCPREG_DPK6);
	DUMP_SECTION(HDMITX_DWC_HDCP22REG_ID, HDMITX_DWC_HDCP22REG_ID);
	DUMP_SECTION(HDMITX_DWC_HDCP22REG_CTRL, HDMITX_DWC_HDCP22REG_CTRL);
	DUMP_SECTION(HDMITX_DWC_HDCP22REG_CTRL1, HDMITX_DWC_HDCP22REG_MUTE);
	DUMP_SECTION(HDMITX_DWC_CEC_CTRL, HDMITX_DWC_CEC_WAKEUPCTRL);
	DUMP_SECTION(HDMITX_DWC_I2CM_SLAVE, HDMITX_DWC_I2CM_SCDC_UPDATE1);
	return 0;
}

static int dump_hdmireg_open(struct inode *inode, struct file *file)
{
	return single_open(file, dump_hdmireg_show, inode->i_private);
}

static const struct file_operations dump_hdmireg_fops = {
	.open		= dump_hdmireg_open,
	.read		= seq_read,
	.release	= single_release,
};

#define CONNECT2REG(_reg) ({				\
	typeof(_reg) reg = (_reg);					\
	hdmitx_rd_reg(reg) + (hdmitx_rd_reg(reg + 1) << 8); })

static int dump_hdmitiming_show(struct seq_file *s, void *p)
{
	unsigned int tmp = 0;

	seq_puts(s, "\n--------HDMITX timing--------\n");
	tmp = CONNECT2REG(HDMITX_DWC_FC_INHACTV0);
	seq_printf(s, "Hactive = %d\n", tmp);

	tmp = CONNECT2REG(HDMITX_DWC_FC_INHBLANK0);
	seq_printf(s, "Hblank = %d\n", tmp);

	tmp = CONNECT2REG(HDMITX_DWC_FC_INVACTV0);
	seq_printf(s, "Vactive = %d\n", tmp);

	tmp = hdmitx_rd_reg(HDMITX_DWC_FC_INVBLANK);
	seq_printf(s, "Vblank = %d\n", tmp);

	tmp = CONNECT2REG(HDMITX_DWC_FC_HSYNCINDELAY0);
	seq_printf(s, "Hfront = %d\n", tmp);

	tmp = CONNECT2REG(HDMITX_DWC_FC_HSYNCINWIDTH0);
	seq_printf(s, "Hsync = %d\n", tmp);

	tmp = hdmitx_rd_reg(HDMITX_DWC_FC_VSYNCINDELAY);
	seq_printf(s, "Vfront = %d\n", tmp);

	tmp = hdmitx_rd_reg(HDMITX_DWC_FC_VSYNCINWIDTH);
	seq_printf(s, "Vsync = %d\n", tmp);

	return 0;
}

static int dump_hdmitiming_open(struct inode *inode, struct file *file)
{
	return single_open(file, dump_hdmitiming_show, inode->i_private);
}

static const struct file_operations dump_hdmitiming_fops = {
	.open		= dump_hdmitiming_open,
	.read		= seq_read,
	.release	= single_release,
};

static void hdmitx_parsing_avipkt(struct seq_file *s)
{
	unsigned int reg_val;
	unsigned int reg_adr;
	unsigned char *conf;

	seq_puts(s, "\n--------parsing AVIInfo--------\n");

	if (hdmitx_get_bit(HDMITX_DWC_FC_PACKET_TX_EN, 2))
		conf = "enable";
	else
		conf = "disable";
	seq_printf(s, "AVIInfo.enable: %s\n", conf);

	reg_adr = HDMITX_DWC_FC_AVICONF0;
	reg_val = hdmitx_rd_reg(HDMITX_DWC_FC_AVICONF0);
	seq_printf(s, "[0x%x]: 0x%x\n", reg_adr, reg_val);
	switch (reg_val & 0x3) {
	case 0:
		conf = "RGB";
		break;
	case 1:
		conf = "422";
		break;
	case 2:
		conf = "444";
		break;
	case 3:
		conf = "420";
	}
	seq_printf(s, "colorspace: %s\n", conf);

	switch ((reg_val & 0x40) >> 6) {
	case 0:
		conf = "disable";
		break;
	case 1:
		conf = "enable";
	}
	seq_printf(s, "active_format_present: %s\n", conf);

	switch ((reg_val & 0x0c) >> 2) {
	case 0:
		conf = "disable";
		break;
	case 1:
		conf = "vert bar";
		break;
	case 2:
		conf = "horiz bar";
		break;
	case 3:
		conf = "vert and horiz bar";
	}
	seq_printf(s, "bar: %s\n", conf);

	switch ((reg_val & 0x30) >> 4) {
	case 0:
		conf = "disable";
		break;
	case 1:
		conf = "overscan";
		break;
	case 2:
		conf = "underscan";
		break;
	default:
		conf = "disable";
	}
	seq_printf(s, "scan: %s\n", conf);

	reg_adr = HDMITX_DWC_FC_AVICONF1;
	reg_val = hdmitx_rd_reg(reg_adr);
	seq_printf(s, "[0x%x]: 0x%x\n", reg_adr, reg_val);
	switch ((reg_val & 0xc0) >> 6) {
	case 0:
		conf = "disable";
		break;
	case 1:
		conf = "BT.601";
		break;
	case 2:
		conf = "BT.709";
		break;
	case 3:
		conf = "Extended";
	}
	seq_printf(s, "colorimetry: %s\n", conf);

	switch ((reg_val & 0x30) >> 4) {
	case 0:
		conf = "disable";
		break;
	case 1:
		conf = "4:3";
		break;
	case 2:
		conf = "16:9";
		break;
	default:
		conf = "disable";
	}
	seq_printf(s, "picture_aspect: %s\n", conf);

	switch (reg_val & 0xf) {
	case 8:
		conf = "Same as picture_aspect";
		break;
	case 9:
		conf = "4:3";
		break;
	case 10:
		conf = "16:9";
		break;
	case 11:
		conf = "14:9";
		break;
	default:
		conf = "Same as picture_aspect";
	}
	seq_printf(s, "active_aspect: %s\n", conf);

	reg_adr = HDMITX_DWC_FC_AVICONF2;
	reg_val = hdmitx_rd_reg(reg_adr);
	seq_printf(s, "[0x%x]: 0x%x\n", reg_adr, reg_val);
	switch ((reg_val & 0x80) >> 7) {
	case 0:
		conf = "disable";
		break;
	case 1:
		conf = "enable";
	}
	seq_printf(s, "itc: %s\n", conf);

	switch ((reg_val & 0x70) >> 4) {
	case 0:
		conf = "xvYCC601";
		break;
	case 1:
		conf = "xvYCC709";
		break;
	case 2:
		conf = "sYCC601";
		break;
	case 3:
		conf = "Adobe_YCC601";
		break;
	case 4:
		conf = "Adobe_RGB";
		break;
	case 5:
	case 6:
		conf = "BT.2020";
		break;
	default:
		conf = "xvYCC601";
	}
	seq_printf(s, "extended_colorimetry: %s\n", conf);

	switch ((reg_val & 0xc) >> 2) {
	case 0:
		conf = "default";
		break;
	case 1:
		conf = "limited";
		break;
	case 2:
		conf = "full";
		break;
	default:
		conf = "default";
	}
	seq_printf(s, "quantization_range: %s\n", conf);

	switch (reg_val & 0x3) {
	case 0:
		conf = "unknown";
		break;
	case 1:
		conf = "horiz";
		break;
	case 2:
		conf = "vert";
		break;
	case 3:
		conf = "horiz and vert";
	}
	seq_printf(s, "nups: %s\n", conf);

	reg_adr = HDMITX_DWC_FC_AVIVID;
	reg_val = hdmitx_rd_reg(reg_adr);
	seq_printf(s, "[0x%x]: 0x%x\n", reg_adr, reg_val);
	seq_printf(s, "vic: %d\n", reg_val);

	reg_adr = HDMITX_DWC_FC_AVICONF3;
	reg_val = hdmitx_rd_reg(reg_adr);
	seq_printf(s, "[0x%x]: 0x%x\n", reg_adr, reg_val);

	switch ((reg_val & 0xc) >> 2) {
	case 0:
	default:
		conf = "limited";
		break;
	case 1:
		conf = "full";
	}
	seq_printf(s, "ycc_quantization_range: %s\n", conf);

	switch (reg_val & 0x3) {
	case 0:
		conf = "graphics";
		break;
	case 1:
		conf = "photo";
		break;
	case 2:
		conf = "cinema";
		break;
	case 3:
		conf = "game";
	}
	seq_printf(s, "content_type: %s\n", conf);

	reg_adr = HDMITX_DWC_FC_PRCONF;
	reg_val = hdmitx_rd_reg(reg_adr);
	seq_printf(s, "[0x%x]: 0x%x\n", reg_adr, reg_val);
	switch ((reg_val & 0xf0) >> 4) {
	case 0:
	case 1:
	default:
		conf = "no";
		break;
	}
	if (((reg_val & 0xf0) >> 4) < 2)
		seq_printf(s, "pixel_repetition: %s\n", conf);
	else
		seq_printf(s, "pixel_repetition: %d times\n",
			   (reg_val & 0xf0) >> 4);

	reg_adr = HDMITX_DWC_FC_DATAUTO3;
	reg_val = hdmitx_rd_reg(reg_adr);
	seq_printf(s, "[0x%x]: 0x%x\n", reg_adr, reg_val);
	switch ((reg_val & 0x8) >> 3) {
	case 0:
		conf = "RDRB";
		break;
	case 1:
		conf = "auto";
	}
	seq_printf(s, "datauto: %s\n", conf);

	reg_adr = HDMITX_DWC_FC_RDRB6;
	reg_val = hdmitx_rd_reg(reg_adr);
	seq_printf(s, "[0x%x]: 0x%x\n", reg_adr, reg_val);
	seq_printf(s, "rdrb_interpolation: %d\n", reg_val & 0xf);

	reg_adr = HDMITX_DWC_FC_RDRB7;
	reg_val = hdmitx_rd_reg(reg_adr);
	seq_printf(s, "[0x%x]: 0x%x\n", reg_adr, reg_val);
	seq_printf(s, "rdrb_perframe: %d\n", (reg_val & 0xf0) >> 4);
	seq_printf(s, "rdrb_linespace: %d\n", reg_val & 0xf);
}

static void hdmitx_parsing_spdpkt(struct seq_file *s)
{
	unsigned int reg_val;
	unsigned int reg_adr;
	unsigned char *conf;
	unsigned char vend_name[9] = {0};
	unsigned char prod_name[17] = {0};
	int i;

	seq_puts(s, "\n--------parsing SPDInfo--------\n");

	if (hdmitx_get_bit(HDMITX_DWC_FC_PACKET_TX_EN, 4) ||
	    hdmitx_get_bit(HDMITX_DWC_FC_DATAUTO0, 4))
		conf = "enable";
	else
		conf = "disable";
	seq_printf(s, "SPDInfo.enable: %s\n", conf);

	DUMP_SECTION(HDMITX_DWC_FC_SPDVENDORNAME0,
		     (HDMITX_DWC_FC_SPDVENDORNAME0 + 24));

	for (i = 0; i < 8; i++)
		vend_name[i] = hdmitx_rd_reg(HDMITX_DWC_FC_SPDVENDORNAME0 + i);

	for (i = 0; i < 16; i++)
		prod_name[i] = hdmitx_rd_reg(HDMITX_DWC_FC_SPDVENDORNAME0 +
					     8 + i);

	if (vend_name[0])
		seq_printf(s, "vendor name: %s\n", vend_name);
	if (prod_name[0])
		seq_printf(s, "product description: %s\n", prod_name);
}

static void hdmitx_parsing_audpkt(struct seq_file *s)
{
	unsigned int reg_val;
	unsigned int reg_adr;
	unsigned char *conf;

	seq_puts(s, "\n--------parsing AudioInfo--------\n");

	if (hdmitx_get_bit(HDMITX_DWC_FC_PACKET_TX_EN, 3))
		conf = "enable";
	else
		conf = "disable";
	seq_printf(s, "AudioInfo.enable: %s\n", conf);

	reg_adr = HDMITX_DWC_FC_AUDICONF0;
	reg_val = hdmitx_rd_reg(reg_adr);
	seq_printf(s, "[0x%x]: 0x%x\n", reg_adr, reg_val);
	switch (reg_val & 0xf) {
	case CT_REFER_TO_STREAM:
		conf = "refer to stream header";
		break;
	case CT_PCM:
		conf = "L-PCM";
		break;
	case CT_AC_3:
		conf = "AC-3";
		break;
	case CT_MPEG1:
		conf = "MPEG1";
		break;
	case CT_MP3:
		conf = "MP3";
		break;
	case CT_MPEG2:
		conf = "MPEG2";
		break;
	case CT_AAC:
		conf = "AAC";
		break;
	case CT_DTS:
		conf = "DTS";
		break;
	case CT_ATRAC:
		conf = "ATRAC";
		break;
	case CT_ONE_BIT_AUDIO:
		conf = "One Bit Audio";
		break;
	case CT_DD_P:
		conf = "Dobly Digital+";
		break;
	case CT_DTS_HD:
		conf = "DTS_HD";
		break;
	case CT_MAT:
		conf = "MAT";
		break;
	case CT_DST:
		conf = "DST";
		break;
	case CT_WMA:
		conf = "WMA";
		break;
	default:
		conf = "MAX";
	}
	seq_printf(s, "coding_type: %s\n", conf);

	switch ((reg_val & 0x70) >> 4) {
	case CC_2CH:
		conf = "2 channels";
		break;
	case CC_3CH:
		conf = "3 channels";
		break;
	case CC_4CH:
		conf = "4 channels";
		break;
	case CC_5CH:
		conf = "5 channels";
		break;
	case CC_6CH:
		conf = "6 channels";
		break;
	case CC_7CH:
		conf = "7 channels";
		break;
	case CC_8CH:
		conf = "8 channels";
		break;
	case CC_REFER_TO_STREAM:
	default:
		conf = "refer to stream header";
		break;
	}
	seq_printf(s, "channel_count: %s\n", conf);

	reg_adr = HDMITX_DWC_FC_AUDICONF1;
	reg_val = hdmitx_rd_reg(reg_adr);
	seq_printf(s, "[0x%x]: 0x%x\n", reg_adr, reg_val);
	switch (reg_val & 0x7) {
	case FS_REFER_TO_STREAM:
		conf = "refer to stream header";
		break;
	case FS_32K:
		conf = "32kHz";
		break;
	case FS_44K1:
		conf = "44.1kHz";
		break;
	case FS_48K:
		conf = "48kHz";
		break;
	case FS_88K2:
		conf = "88.2kHz";
		break;
	case FS_96K:
		conf = "96kHz";
		break;
	case FS_176K4:
		conf = "176.4kHz";
		break;
	case FS_192K:
		conf = "192kHz";
	}
	seq_printf(s, "sample_frequency: %s\n", conf);

	switch ((reg_val & 0x30) >> 4) {
	case SS_16BITS:
		conf = "16bit";
		break;
	case SS_20BITS:
		conf = "20bit";
		break;
	case SS_24BITS:
		conf = "24bit";
		break;
	case SS_REFER_TO_STREAM:
	default:
		conf = "refer to stream header";
		break;
	}
	seq_printf(s, "sample_size: %s\n", conf);

	reg_adr = HDMITX_DWC_FC_AUDICONF2;
	reg_val = hdmitx_rd_reg(reg_adr);
	seq_printf(s, "[0x%x]: 0x%x\n", reg_adr, reg_val);
	seq_printf(s, "channel_allocation: %d\n", reg_val);

	reg_adr = HDMITX_DWC_FC_AUDICONF3;
	reg_val = hdmitx_rd_reg(reg_adr);
	seq_printf(s, "[0x%x]: 0x%x\n", reg_adr, reg_val);
	seq_printf(s, "level_shift_value: %d\n", reg_val & 0xf);
	seq_printf(s, "down_mix_enable: %d\n", (reg_val & 0x10) >> 4);
	seq_printf(s, "LFE_playback_info: %d\n", (reg_val & 0x60) >> 5);

	reg_adr = HDMITX_DWC_FC_DATAUTO3;
	reg_val = hdmitx_rd_reg(reg_adr);
	seq_printf(s, "[0x%x]: 0x%x\n", reg_adr, reg_val);
	switch ((reg_val & 0x2) >> 1) {
	case 0:
		conf = "RDRB";
		break;
	case 1:
		conf = "auto";
	}
	seq_printf(s, "datauto: %s\n", conf);
}

static void hdmitx_parsing_gcppkt(struct seq_file *s)
{
	unsigned int reg_val;
	unsigned int reg_adr;
	unsigned char *conf;

	seq_puts(s, "\n--------parsing GCP--------\n");

	if (hdmitx_get_bit(HDMITX_DWC_FC_PACKET_TX_EN, 1))
		conf = "enable";
	else
		conf = "disable";

	seq_printf(s, "GCP.enable: %s\n", conf);

	reg_adr = HDMITX_DWC_FC_GCP;
	reg_val = hdmitx_rd_reg(reg_adr);
	seq_printf(s, "[0x%x]: 0x%x\n", reg_adr, reg_val);
	seq_printf(s, "clear_avmute: %d\n", reg_val & 0x1);
	seq_printf(s, "set_avmute: %d\n", (reg_val & 0x2) >> 1);
	seq_printf(s, "default_phase: %d\n", (reg_val & 0x4) >> 2);

	reg_adr = HDMITX_DWC_VP_STATUS;
	reg_val = hdmitx_rd_reg(reg_adr);
	seq_printf(s, "[0x%x]: 0x%x\n", reg_adr, reg_val);
	seq_printf(s, "packing_phase: %d\n", reg_val & 0xf);

	reg_adr = HDMITX_DWC_VP_PR_CD;
	reg_val = hdmitx_rd_reg(reg_adr);
	seq_printf(s, "[0x%x]: 0x%x\n", reg_adr, reg_val);
	switch ((reg_val & 0xf0) >> 4) {
	case 0:
	case 4:
		conf = "24bit";
		break;
	case 5:
		conf = "30bit";
		break;
	case 6:
		conf = "36bit";
		break;
	case 7:
		conf = "48bit";
		break;
	default:
		conf = "reserved";
	}
	seq_printf(s, "color_depth: %s\n", conf);

	reg_adr = HDMITX_DWC_VP_REMAP;
	reg_val = hdmitx_rd_reg(reg_adr);
	seq_printf(s, "[0x%x]: 0x%x\n", reg_adr, reg_val);
	switch (reg_val & 0x3) {
	case 0:
		conf = "16bit";
		break;
	case 1:
		conf = "20bit";
		break;
	case 2:
		conf = "24bit";
		break;
	default:
		conf = "reserved";
	}
	seq_printf(s, "YCC 422 size: %s\n", conf);

	reg_adr = HDMITX_DWC_VP_CONF;
	reg_val = hdmitx_rd_reg(reg_adr);
	seq_printf(s, "[0x%x]: 0x%x\n", reg_adr, reg_val);
	switch (reg_val & 0x3) {
	case 0:
		conf = "pixel_packing";
		break;
	case 1:
		conf = "YCC 422";
		break;
	case 2:
	case 3:
		conf = "8bit bypass";
	}
	seq_printf(s, "output selector: %s\n", conf);
	seq_printf(s, "bypass select: %d\n", (reg_val & 0x4) >> 2);
	seq_printf(s, "YCC 422 enable: %d\n", (reg_val & 0x8) >> 3);
	seq_printf(s, "pixel repeater enable: %d\n", (reg_val & 0x10) >> 4);
	seq_printf(s, "pixel packing enable: %d\n", (reg_val & 0x20) >> 5);
	seq_printf(s, "bypass enable: %d\n", (reg_val & 0x40) >> 6);

	reg_adr = HDMITX_DWC_FC_DATAUTO3;
	reg_val = hdmitx_rd_reg(reg_adr);
	seq_printf(s, "[0x%x]: 0x%x\n", reg_adr, reg_val);
	switch ((reg_val & 0x4) >> 2) {
	case 0:
		conf = "RDRB";
		break;
	case 1:
		conf = "auto";
	}
	seq_printf(s, "datauto mode: %s\n", conf);
}

static void hdmitx_parsing_acrpkt(struct seq_file *s)
{
	unsigned int reg_val;
	unsigned int reg_adr;
	unsigned char *conf;
	unsigned int n = 0;

	seq_puts(s, "\n--------parsing ACR--------\n");

	if (hdmitx_get_bit(HDMITX_DWC_FC_PACKET_TX_EN, 1))
		conf = "enable";
	else
		conf = "disable";
	seq_printf(s, "ACR.enable: %s\n", conf);

	reg_adr = HDMITX_DWC_AUD_INPUTCLKFS;
	reg_val = hdmitx_rd_reg(reg_adr);
	seq_printf(s, "[0x%x]: 0x%x\n", reg_adr, reg_val);

	switch (reg_val & 0x7) {
	case 0:
		conf = "128xFs";
		break;
	case 1:
		conf = "512xFs";
		break;
	case 4:
		conf = "64xFs";
		break;
	default:
		conf = "reserved";
	}
	seq_printf(s, "ifsfactor: %s\n", conf);

	n = hdmitx_rd_reg(HDMITX_DWC_AUD_N1) +
	    (hdmitx_rd_reg(HDMITX_DWC_AUD_N2) << 8) +
	    ((hdmitx_rd_reg(HDMITX_DWC_AUD_N3) & 0xf) << 16);
	seq_printf(s, "N: %d\n", n);

	reg_val = hdmitx_get_bit(HDMITX_DWC_AUD_N3, 7);
	seq_printf(s, "ncts_atomic_write: %d\n", reg_val);

	reg_val = get_msr_cts();
	seq_printf(s, "CTS: %d\n", reg_val);

	reg_val = hdmitx_get_bit(HDMITX_DWC_AUD_CTS3, 4);
	seq_printf(s, "CTS_manual: %d\n", (reg_val & 0x10) >> 4);

	switch ((reg_val & 0xe0) >> 5) {
	case 0:
		conf = "1";
		break;
	case 1:
		conf = "16";
		break;
	case 2:
		conf = "32";
		break;
	case 3:
		conf = "64";
		break;
	case 4:
		conf = "128";
		break;
	case 5:
		conf = "256";
		break;
	default:
		conf = "128";
	}
	seq_printf(s, "N_shift: %s\n", conf);
	seq_puts(s, "actual N = audN[19:0]/N_shift\n");
}

static void hdmitx_parsing_audsamp(struct seq_file *s)
{
	unsigned int reg_val;
	unsigned int reg_adr;
	unsigned char *conf;

	seq_puts(s, "\n--------parsing AudSample--------\n");

	if (hdmitx_get_bit(HDMITX_TOP_CLK_CNTL, 2) ||
	    hdmitx_get_bit(HDMITX_TOP_CLK_CNTL, 3))
		conf = "enable";
	else
		conf = "disable";
	seq_printf(s, "AudSample.enable: %s\n", conf);

	reg_adr = HDMITX_DWC_AUD_CONF0;
	reg_val = hdmitx_rd_reg(reg_adr);
	seq_printf(s, "[0x%x]: 0x%x\n", reg_adr, reg_val);

	switch ((reg_val & 0x20) >> 5) {
	case 0:
	default:
		conf = "SPDIF";
		break;
	case 1:
		conf = "I2S";
	}
	seq_printf(s, "i2s_select: %s\n", conf);

	seq_printf(s, "I2S_in_en: 0x%x\n", reg_val & 0xf);

	reg_adr = HDMITX_DWC_AUD_CONF1;
	reg_val = hdmitx_rd_reg(reg_adr);
	seq_printf(s, "[0x%x]: 0x%x\n", reg_adr, reg_val);
	seq_printf(s, "I2S_width: %d bit\n", reg_val & 0x1f);

	switch ((reg_val & 0xe0) >> 5) {
	case 0:
		conf = "standard";
		break;
	case 1:
		conf = "Right-justified";
		break;
	case 2:
		conf = "Left-justified";
		break;
	case 3:
		conf = "Burst 1 mode";
		break;
	case 4:
		conf = "Burst 2 mode";
		break;
	default:
		conf = "standard";
	}
	seq_printf(s, "I2S_mode: %s\n", conf);

	reg_adr = HDMITX_DWC_AUD_CONF2;
	reg_val = hdmitx_rd_reg(reg_adr);
	seq_printf(s, "[0x%x]: 0x%x\n", reg_adr, reg_val);
	seq_printf(s, "HBR mode enable: %d\n", reg_val & 0x1);
	seq_printf(s, "NLPCM mode enable: %d\n", (reg_val & 0x2) >> 1);

	reg_adr = HDMITX_DWC_AUD_SPDIF1;
	reg_val = hdmitx_rd_reg(reg_adr);
	seq_printf(s, "[0x%x]: 0x%x\n", reg_adr, reg_val);
	seq_printf(s, "SPDIF_width: %d bit\n", reg_val & 0x1f);
	seq_printf(s, "SPDIF_HBR_MODE: %d\n", (reg_val & 0x40) >> 6);
	seq_printf(s, "SPDIF_NLPCM_MODE: %d\n", (reg_val & 0x80) >> 7);

	reg_adr = HDMITX_DWC_FC_AUDSCONF;
	reg_val = hdmitx_rd_reg(reg_adr);
	seq_printf(s, "[0x%x]: 0x%x\n", reg_adr, reg_val);
	seq_printf(s, "layout: %d\n", reg_val & 0x1);
	seq_printf(s, "sample flat: %d\n", (reg_val & 0xf0) >> 4);

	reg_adr = HDMITX_DWC_FC_AUDSSTAT;
	reg_val = hdmitx_rd_reg(reg_adr);
	seq_printf(s, "[0x%x]: 0x%x\n", reg_adr, reg_val);
	seq_printf(s, "sample present: %d\n", reg_val & 0xf);

	reg_adr = HDMITX_DWC_FC_AUDSV;
	reg_val = hdmitx_rd_reg(reg_adr);
	seq_printf(s, "[0x%x]: 0x%x\n", reg_adr, reg_val);
	seq_puts(s, "audio sample validity flag\n");
	seq_printf(s, "channel 0/1/2/3, Left: %d/%d/%d/%d\n",
		   reg_val & 0x1,
		   (reg_val & 0x2) >> 1,
		   (reg_val & 0x4) >> 2,
		   (reg_val & 0x8) >> 3);
	seq_printf(s, "channel 0/1/2/3, Right: %d/%d/%d/%d\n",
		   (reg_val & 0x10) >> 4,
		   (reg_val & 0x20) >> 5,
		   (reg_val & 0x40) >> 6,
		   (reg_val & 0x80) >> 7);

	reg_adr = HDMITX_DWC_FC_AUDSU;
	reg_val = hdmitx_rd_reg(reg_adr);
	seq_printf(s, "[0x%x]: 0x%x\n", reg_adr, reg_val);
	seq_puts(s, "audio sample user flag\n");
	seq_printf(s, "channel 0/1/2/3, Left: %d/%d/%d/%d\n",
		   reg_val & 0x1,
		   (reg_val & 0x2) >> 1,
		   (reg_val & 0x4) >> 2,
		   (reg_val & 0x8) >> 3);
	seq_printf(s, "channel 0/1/2/3, Right: %d/%d/%d/%d\n",
		   (reg_val & 0x10) >> 4,
		   (reg_val & 0x20) >> 5,
		   (reg_val & 0x40) >> 6,
		   (reg_val & 0x80) >> 7);
}

static void hdmitx_parsing_audchannelst(struct seq_file *s)
{
	unsigned int reg_val;
	unsigned int reg_adr;
	unsigned char *conf;

	seq_puts(s, "\n--------parsing AudChannelSt--------\n");

	reg_adr = HDMITX_DWC_FC_AUDSCHNLS0;
	reg_val = hdmitx_rd_reg(reg_adr);
	seq_printf(s, "[0x%x]: 0x%x\n", reg_adr, reg_val);
	seq_printf(s, "iec_copyright: %d\n", reg_val & 0x1);
	seq_printf(s, "iec_cgmsa: %d\n", (reg_val & 0x30) >> 4);

	reg_adr = HDMITX_DWC_FC_AUDSCHNLS1;
	reg_val = hdmitx_rd_reg(reg_adr);
	seq_printf(s, "[0x%x]: 0x%x\n", reg_adr, reg_val);
	seq_printf(s, "iec_categorycode: %d\n", reg_val);

	reg_adr = HDMITX_DWC_FC_AUDSCHNLS2;
	reg_val = hdmitx_rd_reg(reg_adr);
	seq_printf(s, "[0x%x]: 0x%x\n", reg_adr, reg_val);
	seq_printf(s, "iec_sourcenumber: %d\n", reg_val & 0xf);
	seq_printf(s, "iec_pcmaudiomode: %d\n", (reg_val & 0x30) >> 4);

	reg_adr = HDMITX_DWC_FC_AUDSCHNLS3;
	reg_val = hdmitx_rd_reg(reg_adr);
	seq_printf(s, "[0x%x]: 0x%x\n", reg_adr, reg_val);
	seq_printf(s, "iec_channelnumcr0: %d\n", reg_val & 0xf);
	seq_printf(s, "iec_channelnumcr1: %d\n", (reg_val & 0xf0) >> 4);

	reg_adr = HDMITX_DWC_FC_AUDSCHNLS4;
	reg_val = hdmitx_rd_reg(reg_adr);
	seq_printf(s, "iec_channelnumcr2: %d\n", reg_val & 0xf);
	seq_printf(s, "iec_channelnumcr3: %d\n", (reg_val & 0xf0) >> 4);

	reg_adr = HDMITX_DWC_FC_AUDSCHNLS5;
	reg_val = hdmitx_rd_reg(reg_adr);
	seq_printf(s, "[0x%x]: 0x%x\n", reg_adr, reg_val);
	seq_printf(s, "iec_channelnumcl0: %d\n", reg_val & 0xf);
	seq_printf(s, "iec_channelnumcl1: %d\n", (reg_val & 0xf0) >> 4);

	reg_adr = HDMITX_DWC_FC_AUDSCHNLS6;
	reg_val = hdmitx_rd_reg(reg_adr);
	seq_printf(s, "[0x%x]: 0x%x\n", reg_adr, reg_val);
	seq_printf(s, "iec_channelnumcl2: %d\n", reg_val & 0xf);
	seq_printf(s, "iec_channelnumcl3: %d\n", (reg_val & 0xf0) >> 4);

	reg_adr = HDMITX_DWC_FC_AUDSCHNLS7;
	reg_val = hdmitx_rd_reg(reg_adr);
	seq_printf(s, "[0x%x]: 0x%x\n", reg_adr, reg_val);

	switch (reg_val & 0xf) {
	case 0:
		conf = "44.1kHz";
		break;
	case 1:
		conf = "not indicated";
		break;
	case 2:
		conf = "48kHz";
		break;
	case 3:
		conf = "32kHz";
		break;
	case 8:
		conf = "88.2kHz";
		break;
	case 9:
		conf = "768kHz";
		break;
	case 10:
		conf = "96kHz";
		break;
	case 12:
		conf = "176.4kHz";
		break;
	case 14:
		conf = "192kHz";
		break;
	default:
		conf = "not indicated";
	}
	seq_printf(s, "iec_sampfreq: %s\n", conf);

	seq_printf(s, "iec_clk: %d\n", (reg_val & 0x30) >> 4);
	seq_printf(s, "iec_sampfreq_ext: %d\n", (reg_val & 0xc0) >> 6);

	reg_adr = HDMITX_DWC_FC_AUDSCHNLS8;
	reg_val = hdmitx_rd_reg(reg_adr);
	seq_printf(s, "[0x%x]: 0x%x\n", reg_adr, reg_val);

	switch (reg_val & 0xf) {
	case 0:
	case 1:
		conf = "not indicated";
		break;
	case 2:
		conf = "16bit";
		break;
	case 4:
		conf = "18bit";
		break;
	case 8:
		conf = "19bit";
		break;
	case 10:
		conf = "20bit";
		break;
	case 12:
		conf = "17bit";
		break;
	case 3:
		conf = "20bit";
		break;
	case 5:
		conf = "22bit";
		break;
	case 9:
		conf = "23bit";
		break;
	case 11:
		conf = "24bit";
		break;
	case 13:
		conf = "21bit";
		break;
	default:
		conf = "not indicated";
	}
	seq_printf(s, "iec_worldlength: %s\n", conf);

	switch ((reg_val & 0xf0) >> 4) {
	case 0:
		conf = "not indicated";
		break;
	case 1:
		conf = "192kHz";
		break;
	case 3:
		conf = "176.4kHz";
		break;
	case 5:
		conf = "96kHz";
		break;
	case 7:
		conf = "88.2kHz";
		break;
	case 13:
		conf = "48kHz";
		break;
	case 15:
		conf = "44.1kHz";
		break;
	default:
		conf = "not indicated";
	}
	seq_printf(s, "iec_origsamplefreq: %s\n", conf);
}

static void hdmitx_parsing_hdrpkt(struct seq_file *s)
{
	unsigned int reg_val;
	unsigned int reg_adr;
	unsigned char *conf;
	unsigned int hcnt, vcnt;
	struct hdmitx_dev *hdev = get_hdmitx_device();

	seq_puts(s, "\n--------parsing DRM/HDR--------\n");
	seq_printf(s, "hdr_transfer_feature: 0x%x\n",
		   hdev->hdr_transfer_feature);
	seq_printf(s, "hdmi_current_hdr_mode: 0x%x\n",
		   hdev->hdmi_current_hdr_mode);
	seq_printf(s, "hdmi_last_hdr_mode: 0x%x\n", hdev->hdmi_last_hdr_mode);

	if (hdmitx_get_bit(HDMITX_DWC_FC_DATAUTO3, 6) &&
	    hdmitx_get_bit(HDMITX_DWC_FC_PACKET_TX_EN, 7))
		conf = "enable";
	else
		conf = "disable";
	seq_printf(s, "DRM.enable: %s\n", conf);

	reg_adr = HDMITX_DWC_FC_DRM_HB01;
	reg_val = hdmitx_rd_reg(reg_adr);
	seq_printf(s, "[0x%x]: 0x%x\n", reg_adr, reg_val);
	seq_printf(s, "version: %d\n", reg_val);
	reg_adr = HDMITX_DWC_FC_DRM_HB02;
	reg_val = hdmitx_rd_reg(reg_adr);
	seq_printf(s, "[0x%x]: 0x%x\n", reg_adr, reg_val);
	seq_printf(s, "size: %d\n", reg_val);

	reg_adr = HDMITX_DWC_FC_DRM_PB00;
	reg_val = hdmitx_rd_reg(reg_adr);
	seq_printf(s, "[0x%x]: 0x%x\n", reg_adr, reg_val);

	switch (reg_val) {
	case 0:
		conf = "sdr";
		break;
	case 1:
		conf = "hdr";
		break;
	case 2:
		conf = "ST 2084";
		break;
	case 3:
		conf = "HLG";
		break;
	default:
		conf = "sdr";
	}
	seq_printf(s, "eotf: %s\n", conf);

	reg_adr = HDMITX_DWC_FC_DRM_PB01;
	reg_val = hdmitx_rd_reg(reg_adr);
	seq_printf(s, "[0x%x]: 0x%x\n", reg_adr, reg_val);

	switch (reg_val) {
	case 0:
		conf = "static metadata";
		break;
	default:
		conf = "reserved";
	}
	seq_printf(s, "metadata_id: %s\n", conf);

	seq_puts(s, "primaries:\n");
	for (vcnt = 0; vcnt < 3; vcnt++) {
		for (hcnt = 0; hcnt < 2; hcnt++) {
			reg_adr = HDMITX_DWC_FC_DRM_PB02 +
					   (vcnt * 2 + hcnt) * 2;
			reg_val = hdmitx_rd_reg(reg_adr);
			reg_adr = reg_adr + 1;
			reg_val = hdmitx_rd_reg(reg_adr) << 8 | reg_val;
			seq_printf(s, "%u, ", reg_val);
		}
		seq_puts(s, "\n");
	}

	seq_puts(s, "white_point: ");
	for (hcnt = 0; hcnt < 2; hcnt++) {
		reg_adr = HDMITX_DWC_FC_DRM_PB14 +
				   hcnt * 2;
		reg_val = hdmitx_rd_reg(reg_adr);
		reg_adr = reg_adr + 1;
		reg_val = hdmitx_rd_reg(reg_adr) << 8 | reg_val;
		seq_printf(s, "%u, ", reg_val);
	}
	seq_puts(s, "\n");

	seq_puts(s, "luminance: ");
	for (hcnt = 0; hcnt < 2; hcnt++) {
		reg_adr = HDMITX_DWC_FC_DRM_PB18 +
				   hcnt * 2;
		reg_val = hdmitx_rd_reg(reg_adr);
		reg_adr = reg_adr + 1;
		reg_val = hdmitx_rd_reg(reg_adr) << 8 | reg_val;
		seq_printf(s, "%u, ", reg_val);
	}
	seq_puts(s, "\n");

	reg_adr = HDMITX_DWC_FC_DRM_PB22;
	reg_val = hdmitx_rd_reg(reg_adr);
	reg_adr = reg_adr + 1;
	reg_val = hdmitx_rd_reg(reg_adr) << 8 | reg_val;
	seq_printf(s, "max_content: %u\n", reg_val);
	reg_adr = HDMITX_DWC_FC_DRM_PB24;
	reg_val = hdmitx_rd_reg(reg_adr);
	reg_adr = reg_adr + 1;
	reg_val = hdmitx_rd_reg(reg_adr) << 8 | reg_val;
	seq_printf(s, "max_frame_average: %u\n", reg_val);
}

static void print_current_dv_hdr_(struct seq_file *s)
{
	struct hdmitx_dev *hdev = get_hdmitx_device();

	seq_printf(s, "hdmi_current_eotf_type: 0x%x\n",
		   hdev->hdmi_current_eotf_type);
	seq_printf(s, "hdmi_current_tunnel_mode: 0x%x\n",
		   hdev->hdmi_current_tunnel_mode);
	seq_printf(s, "dv_src_feature: %d\n", hdev->dv_src_feature);
	seq_printf(s, "hdr_transfer_feature: %d\n",
		   hdev->hdr_transfer_feature);
	seq_printf(s, "hdr_color_feature: %d\n", hdev->hdr_color_feature);
	seq_printf(s, "colormetry: %d\n", hdev->colormetry);
}

static void hdmitx_parsing_vsifpkt(struct seq_file *s)
{
	unsigned int reg_val;
	unsigned int reg_adr;
	unsigned char vsd_ieee_id[3];
	unsigned char pb4;
	unsigned int tmp;
	unsigned char *conf;
	unsigned int ieee_code = 0;
	unsigned int count;

	seq_puts(s, "\n--------parsing VSIF--------\n");

	if (!hdmitx_get_bit(HDMITX_DWC_FC_DATAUTO0, 3) ||
	    !hdmitx_get_bit(HDMITX_DWC_FC_PACKET_TX_EN, 4))
		conf = "enable";
	else
		conf = "disable";
	seq_printf(s, "VSIF.enable: %s\n", conf);

	DUMP_SECTION(HDMITX_DWC_FC_VSDIEEEID0, HDMITX_DWC_FC_VSDPAYLOAD23);

	reg_val = hdmitx_rd_reg(HDMITX_DWC_FC_VSDSIZE);
	seq_printf(s, "size: %d\n", reg_val);

	/* check VSIF type */
	vsd_ieee_id[0] = hdmitx_rd_reg(HDMITX_DWC_FC_VSDIEEEID0);
	vsd_ieee_id[1] = hdmitx_rd_reg(HDMITX_DWC_FC_VSDIEEEID1);
	vsd_ieee_id[2] = hdmitx_rd_reg(HDMITX_DWC_FC_VSDIEEEID2);
	ieee_code = vsd_ieee_id[0] |
		    vsd_ieee_id[1] << 8 |
		    vsd_ieee_id[2] << 16;
	seq_printf(s, "ieee_id: 0x%x\n", ieee_code);
	pb4 = hdmitx_rd_reg(HDMITX_DWC_FC_VSDPAYLOAD0);
	if (ieee_code == HDMI_IEEEOUI && pb4 == 0x20) {
		/* HDMI 4K */
		seq_printf(s, "HDMI 4K %s[%d]\n", __func__, __LINE__);
		reg_adr = HDMITX_DWC_FC_VSDPAYLOAD1;
		tmp = hdmitx_rd_reg(reg_adr);
		seq_printf(s, "vic: 0x%x\n", tmp);
		return;
	}
	if (ieee_code == HDMI_IEEEOUI && pb4 == 0x40) {
		/* HDMI 3D */
		seq_printf(s, "HDMI 3D %s[%d]\n", __func__, __LINE__);
		reg_adr = HDMITX_DWC_FC_VSDPAYLOAD1;
		tmp = hdmitx_rd_reg(reg_adr);
		seq_printf(s, "3D_structure: 0x%x\n", tmp);
		reg_adr = HDMITX_DWC_FC_VSDPAYLOAD2;
		tmp = hdmitx_rd_reg(reg_adr);
		seq_printf(s, "3D_Ext_Data: 0x%x\n", tmp);
		return;
	}
	if (hdmitx_get_cur_dv_st() == HDMI_DV_VSIF_STD) {
		/* DV STD */
		seq_printf(s, "DV STD %s[%d]\n", __func__, __LINE__);
		print_current_dv_hdr_(s);
		return;
	}
	if (hdmitx_get_cur_dv_st() == HDMI_DV_VSIF_LL) {
		/* DV LL */
		seq_printf(s, "DV LL %s[%d]\n", __func__, __LINE__);
		print_current_dv_hdr_(s);
		return;
	}
	if (hdmitx_get_cur_hdr10p_st() == HDMI_HDR10P_DV_VSIF) {
		/* HDR10plus */
		seq_printf(s, "HDR10+ %s[%d]\n", __func__, __LINE__);
		print_current_dv_hdr_(s);
		reg_adr = HDMITX_DWC_FC_VSDPAYLOAD0;
		reg_val = hdmitx_rd_reg(reg_adr);
		/*hdr 10+ vsif data information*/
		seq_printf(s, "app_ver: %u\t", (reg_val >> 6) & 0x3);
		seq_printf(s, "tar_max_lum: %u\t", (reg_val >> 1) & 0x1f);
		reg_adr = HDMITX_DWC_FC_VSDPAYLOAD1;
		tmp = hdmitx_rd_reg(reg_adr);
		seq_printf(s, "avg_maxrgb: %d\t", tmp);

		for (count = 0; count < 9; count++) {
			reg_adr = HDMITX_DWC_FC_VSDPAYLOAD2 + count;
			tmp = hdmitx_rd_reg(reg_adr);
			seq_printf(s, "dist_values: %d\t", tmp);
			if (count == 3 || count == 7 || count == 8)
				seq_puts(s, "\n");
		}

		reg_adr = HDMITX_DWC_FC_VSDPAYLOAD11;
		tmp = hdmitx_rd_reg(reg_adr);
		seq_printf(s, "nb_curve_anchors: %u\n", (tmp >> 4) & 0xf);
		reg_val = (tmp & 0xf) << 6;
		reg_adr = HDMITX_DWC_FC_VSDPAYLOAD12;
		tmp = hdmitx_rd_reg(reg_adr);
		reg_val = reg_val | ((tmp >> 2) & 0x3f);
		seq_printf(s, "knee_point_x: %u\t", reg_val);

		reg_val = (tmp & 0x3) << 8;
		reg_adr = HDMITX_DWC_FC_VSDPAYLOAD13;
		tmp = hdmitx_rd_reg(reg_adr);
		reg_val = reg_val | (tmp & 0xff);
		seq_printf(s, "knee_point_y: %u\n", reg_val);

		for (count = 0; count < 9; count++) {
			reg_adr = HDMITX_DWC_FC_VSDPAYLOAD14 + count;
			tmp = hdmitx_rd_reg(reg_adr);
			seq_printf(s, "bc_anchors: %d\t", tmp);
			if (count == 3 || count == 7 || count == 8)
				seq_puts(s, "\n");
		}

		reg_adr = HDMITX_DWC_FC_VSDPAYLOAD23;
		tmp = hdmitx_rd_reg(reg_adr);
		seq_printf(s, "graph_overlay_flag: %u\t", (tmp >> 7) & 0x1);
		seq_printf(s, "no_delay_flag: %u\n", (tmp >> 6) & 0x1);
		return;
	}
}

static int dump_hdmipkt_show(struct seq_file *s, void *p)
{
	seq_puts(s, "\n--------HDMITX packets--------\n");
	hdmitx_parsing_acrpkt(s);
	hdmitx_parsing_audsamp(s);
	hdmitx_parsing_gcppkt(s);
	hdmitx_parsing_vsifpkt(s);
	hdmitx_parsing_avipkt(s);
	hdmitx_parsing_spdpkt(s);
	hdmitx_parsing_audpkt(s);
	hdmitx_parsing_audchannelst(s);
	hdmitx_parsing_hdrpkt(s);

	return 0;
}

static int dump_hdmipkt_open(struct inode *inode, struct file *file)
{
	return single_open(file, dump_hdmipkt_show, inode->i_private);
}

static const struct file_operations dump_hdmipkt_fops = {
	.open		= dump_hdmipkt_open,
	.read		= seq_read,
	.release	= single_release,
};

static int dump_hdmiver_show(struct seq_file *s, void *p)
{
	const char *hdmi_ver = HDMITX20_VERSIONS_LOG;

	seq_puts(s, "\n--------HDMITX version log--------\n");
	seq_printf(s, "%s", hdmi_ver);

	return 0;
}

static int dump_hdmiver_open(struct inode *inode, struct file *file)
{
	return single_open(file, dump_hdmiver_show, inode->i_private);
}

static const struct file_operations dump_hdmiver_fops = {
	.open		= dump_hdmiver_open,
	.read		= seq_read,
	.release	= single_release,
};

static inline unsigned int get_msr_cts(void)
{
	unsigned int ret = 0;

	ret = hdmitx_rd_reg(HDMITX_DWC_AUD_CTS1);
	ret += (hdmitx_rd_reg(HDMITX_DWC_AUD_CTS2) << 8);
	ret += ((hdmitx_rd_reg(HDMITX_DWC_AUD_CTS3) & 0xf) << 16);

	return ret;
}

#define AUD_CTS_LOG_NUM	1000
unsigned int cts_buf[AUD_CTS_LOG_NUM];
static int dump_audcts_show(struct seq_file *s, void *p)
{
	int i;
	unsigned int min = 0, max = 0, total = 0;

	seq_puts(s, "\n--------HDMITX audio cts--------\n");

	memset(cts_buf, 0, sizeof(cts_buf));
	for (i = 0; i < AUD_CTS_LOG_NUM; i++) {
		cts_buf[i] = get_msr_cts();
		mdelay(1);
	}

	seq_puts(s, "cts change:\n");
	for (i = 1; i < AUD_CTS_LOG_NUM; i++) {
		if (cts_buf[i] > cts_buf[i - 1])
			seq_printf(s, "dis: +%d  [%d] %d  [%d] %d\n",
				cts_buf[i] - cts_buf[i - 1], i,
				cts_buf[i], i - 1, cts_buf[i - 1]);
		if (cts_buf[i] < cts_buf[i - 1])
			seq_printf(s, "dis: %d  [%d] %d  [%d] %d\n",
				cts_buf[i] - cts_buf[i - 1], i,
				cts_buf[i], i - 1, cts_buf[i - 1]);
		}

	for (i = 0, min = max = cts_buf[0]; i < AUD_CTS_LOG_NUM; i++) {
		total += cts_buf[i];
		if (min > cts_buf[i])
			min = cts_buf[i];
		if (max < cts_buf[i])
			max = cts_buf[i];
	}
	seq_printf(s, "\nCTS Min: %d   Max: %d   Avg: %d/1000\n\n",
		   min, max, total);

	return 0;
}

static int dump_audcts_open(struct inode *inode, struct file *file)
{
	return single_open(file, dump_audcts_show, inode->i_private);
}

static const struct file_operations dump_audcts_fops = {
	.open		= dump_audcts_open,
	.read		= seq_read,
	.release	= single_release,
};

struct hdmitx_dbg_files_s {
	const char *name;
	const umode_t mode;
	const struct file_operations *fops;
};

static struct hdmitx_dbg_files_s hdmitx_dbg_files[] = {
	{"bus_reg", S_IFREG | 0444, &dump_busreg_fops},
	{"hdmi_reg", S_IFREG | 0444, &dump_hdmireg_fops},
	{"hdmi_timing", S_IFREG | 0444, &dump_hdmitiming_fops},
	{"hdmi_pkt", S_IFREG | 0444, &dump_hdmipkt_fops},
	{"hdmi_ver", S_IFREG | 0444, &dump_hdmiver_fops},
	{"aud_cts", S_IFREG | 0444, &dump_audcts_fops},
};

static struct dentry *hdmitx_dbgfs;
void hdmitx_debugfs_init(void)
{
	struct dentry *entry;
	int i;

	if (hdmitx_dbgfs)
		return;

	hdmitx_dbgfs = debugfs_create_dir(DEVICE_NAME, NULL);
	if (!hdmitx_dbgfs) {
		pr_err("can't create %s debugfs dir\n", DEVICE_NAME);
		return;
	}

	for (i = 0; i < ARRAY_SIZE(hdmitx_dbg_files); i++) {
		entry = debugfs_create_file(hdmitx_dbg_files[i].name,
			hdmitx_dbg_files[i].mode,
			hdmitx_dbgfs, NULL,
			hdmitx_dbg_files[i].fops);
		if (!entry)
			pr_err("debugfs create file %s failed\n",
				hdmitx_dbg_files[i].name);
	}
}

struct dentry *hdmitx_get_dbgfsdentry(void)
{
	return hdmitx_dbgfs;
}
