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
#include "common.h"
#include <linux/amlogic/media/vout/hdmi_tx21/hdmi_version.h>

#ifdef DEVICE_NAME
#undef DEVICE_NAME
#define DEVICE_NAME "amhdmitx" /* name is same as before */
#endif

static void dump32(struct seq_file *s, u32 start, u32 end)
{
	u32 value = 0;

	for (; start <= end; start += 4) {
		value = hd21_read_reg(start);
		if (value)
			seq_printf(s, "[0x%08x] 0x%08x\n",
				TO21_PHY_ADDR(start), value);
	}
}

static int dump_regs_show(struct seq_file *s, void *p)
{
	seq_puts(s, "\n--------misc registers--------\n");

	// ((0x0000 << 2) + 0xfe008000) ~ ((0x00f3 << 2) + 0xfe008000)
	dump32(s, ANACTRL_SYS0PLL_CTRL0, ANACTRL_DIF_PHY_STS);
	// ((0x0001 << 2) + 0xfe000000) ~ ((0x0128 << 2) + 0xfe000000)
	dump32(s, CLKCTRL_OSCIN_CTRL, CLKCTRL_FPLL_SYS);
	// ((0x0000 << 2) + 0xfe00c000) ~ ((0x027f << 2) + 0xfe00c000)
	dump32(s, PWRCTRL_PWR_ACK0, PWRCTRL_A73TOP_FSM_JUMP);
	// ((0x1b00 << 2) + 0xff000000) ~ ((0x1bea << 2) + 0xff000000)
	dump32(s, ENCI_VIDEO_MODE, ENCP_VRR_CTRL1);
	// ((0x1c00 << 2) + 0xff000000) ~((0x1cfc << 2) + 0xff000000)
	dump32(s, ENCI_DVI_HSO_BEGIN, VPU_VENCL_DITH_LUT_12);
	// ((0x2158 << 2) + 0xff000000) ~ ((0x2250 << 2) + 0xff000000)
	dump32(s, ENCP1_VFIFO2VD_CTL, ENCP1_VFIFO2VD_CTL2);
	// ((0x2358 << 2) + 0xff000000) ~ ((0x2450 << 2) + 0xff000000)
	dump32(s, ENCP2_VFIFO2VD_CTL, ENCP2_VFIFO2VD_CTL2);
	// ((0x2451 << 2) + 0xff000000) ~ ((0x24fc << 2) + 0xff000000)
	dump32(s, VENC2_DVI_SETTING_MORE, VPU2_VENCL_DITH_LUT_12);
	// ((0x2701 << 2) + 0xff000000) ~ ((0x24fc << 2) + 0xff000000)
	dump32(s, VPU_CRC_CTRL, VPUCTRL_REG_ADDR(0x27fd));

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

static void dumptop(struct seq_file *s, u32 start, u32 end)
{
	u32 value = 0;

	start = (((start) & 0xffff) | TOP_OFFSET_MASK);
	end = (((end) & 0xffff) | TOP_OFFSET_MASK);
	for (; start <= end; start += 4) {
		value = hdmitx21_rd_reg(start);
		if (value)
			seq_printf(s, "[0x%08x] 0x%02x\n", start, value);
	}
}

static void dumpcor(struct seq_file *s, u32 start, u32 end)
{
	u32 value = 0;

	for (; start <= end; start++) {
		value = hdmitx21_rd_reg(start);
		if (value)
			seq_printf(s, "[0x%08x] 0x%02x\n", start, value);
	}
}

static int dump_hdmireg_show(struct seq_file *s, void *p)
{
	seq_puts(s, "\n--------HDMITX registers--------\n");
	// 0xfe300000 ~ 0xfe300000 + (0x041 << 2)
	dumptop(s, HDMITX_TOP_SW_RESET, HDMITX_TOP_SECURE_DATA);
	// 0x00000000 - 0x00000018
	dumpcor(s, INTR3_IVCTX, AON_CYP_CTL_IVCTX);
	// 0x00000100 - 0x00000130
	dumpcor(s, VND_IDL_IVCTX, TOP_INTR_IVCTX);
	// 0x00000200 - 0x000002d5
	dumpcor(s, DEBUG_MODE_EN_IVCTX, DROP_GEN_TYPE_5_IVCTX);
	// 0x00000300 - 0x0000031a
	dumpcor(s, TX_ZONE_CTL0_IVCTX, FIFO_10TO20_CTRL_IVCTX);
	// 0x00000400 - 0x000004a0
	dumpcor(s, BIST_RST_IVCTX, CR_BLACK_HIGH_IVCTX);
	// 0x00000607 - 0x000006ff
	dumpcor(s, TPI_MISC_IVCTX, RSVD11_HW_TPI_IVCTX);
	// 0x00000800 - 0x00000876
	dumpcor(s, CP2TX_CTRL_0_IVCTX, CP2TX_GP1_IVCTX);
	// 0x000008a0 - 0x000008d0
	dumpcor(s, HDCP2X_DEBUG_CTRL0_IVCTX, HDCP2X_DEBUG_STAT16_IVCTX);
	// 0x00000900 - 0x00000934
	dumpcor(s, SCRCTL_IVCTX, RSVD1_HDMI2_IVCTX);
	// 0x00000a00 - 0x00000a6e
	dumpcor(s, RSVD0_AIP_IVCTX, AUDIO_CLK_DIV_IVCTX);
	// 0x00000b00 - 0x00000be1
	dumpcor(s, VP_FEATURES_IVCTX, VP_EMBD_SYNC_ENC_CONFIG_IVCTX);
	// 0x00000d00 - 0x00000d3c
	dumpcor(s, VP_CMS_CSC0_FEATURES_IVCTX,
		VP_CMS_CSC0_MULTI_CSC_OUT_RCR_OFFSET_IVCTX);
	//0x00000d80 - 0x00000ddc
	dumpcor(s, VP_CMS_CSC1_FEATURES_IVCTX, VP_CMS_CSC1_PIXCAP_OUT_CR_IVCTX);
	// 0x00000f00 - 0x00000f26
	dumpcor(s, D_HDR_GEN_CTL_IVCTX, D_HDR_SPARE_9_IVCTX);
	// 0x00000f80 - 0x00000fa9
	dumpcor(s, DSC_PKT_GEN_CTL_IVCTX, DSC_PKT_SPARE_9_IVCTX);
	dump_infoframe_packets(s);
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

#define CONNECT2REG(reg) ({ \
	typeof(reg) _reg = (reg); \
	(hdmitx21_rd_reg(_reg) & 0xff) + \
	((hdmitx21_rd_reg(_reg + 1) & 0xff) << 8); })

#define CONNECT3REG(reg) ({ \
	typeof(reg) _reg = (reg); \
	(hdmitx21_rd_reg(_reg) & 0xff) + \
	((hdmitx21_rd_reg(_reg + 1) & 0xff) << 8) + \
	((hdmitx21_rd_reg(_reg + 2) & 0xff) << 16); })

#define CONNECT4REG(reg) ({ \
	typeof(reg) _reg = (reg); \
	(hdmitx21_rd_reg(_reg) & 0xff) + \
	((hdmitx21_rd_reg(_reg + 1) & 0xff) << 8) + \
	((hdmitx21_rd_reg(_reg + 2) & 0xff) << 16) + \
	((hdmitx21_rd_reg(_reg + 3) & 0xff) << 24); })

static int dump_hdmivpfdet_show(struct seq_file *s, void *p)
{
	u32 reg;
	u32 val;
	u32 total, active, front, sync, back, blank;
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	seq_puts(s, "\n--------vp fdet info--------\n");

	hdmitx21_wr_reg(VP_FDET_CLEAR_IVCTX, 0);
	hdmitx21_wr_reg(VP_FDET_STATUS_IVCTX, 0);
	mdelay(hdev->pxp_mode ? 10 : 50); /* at least 1 frame? */

	reg = VP_FDET_FRAME_RATE_IVCTX;
	val = CONNECT3REG(reg);
	if (val) {
		u32 integer;
		u32 i;
		u32 reminder;
		u32 quotient;
		u32 result = 0;

		/* due to the vframe rate are always with decimals,
		 * manually calculate the decimal parts
		 */
		val--;
		integer = 200000000 / val;
		reminder = 200000000 - integer * val;
		for (i = 0, result = 0; i < 3; i++) {
			reminder = reminder * 10;
			result = result * 10;
			quotient = reminder / val;
			reminder = reminder - quotient * val;
			result += quotient;
		}
		seq_printf(s, "frame_rate [%x] 0x%x %d %d.%03d Hz\n",
			reg, val, val, integer, result);
	}

	reg = VP_FDET_PIXEL_COUNT_IVCTX;
	val = CONNECT2REG(reg);
	active = val;
	if (val)
		seq_printf(s, "hactive [%x] 0x%x %d\n", reg, val, val);

	reg = VP_FDET_HFRONT_COUNT_IVCTX;
	val = CONNECT2REG(reg);
	front = val;
	if (val)
		seq_printf(s, "hfront [%x] 0x%x %d\n", reg, val, val);

	reg = VP_FDET_HSYNC_HIGH_COUNT_IVCTX;
	val = CONNECT2REG(reg);
	sync = val;
	if (val)
		seq_printf(s, "hsync [%x] 0x%x %d\n", reg, val, val);

	reg = VP_FDET_HBACK_COUNT_IVCTX;
	val = CONNECT2REG(reg);
	back = val;
	if (val)
		seq_printf(s, "hback [%x] 0x%x %d\n", reg, val, val);

	blank = front + sync + back;
	total = active + blank;
	if (blank)
		seq_printf(s, "Calc hblank 0x%x %d\n", blank, blank);
	if (total)
		seq_printf(s, "Calc htotal 0x%x %d\n", total, total);

	reg = VP_FDET__LINE__COUNT_IVCTX;
	val = CONNECT2REG(reg);
	active = val;
	if (val)
		seq_printf(s, "vactive [%x] 0x%x %d\n", reg, val, val);

	reg = VP_FDET_VSYNC_HIGH_COUNT_EVEN_IVCTX;
	val = CONNECT2REG(reg);
	sync = val;
	if (val)
		seq_printf(s, "vsync_high_count_even [%x] 0x%x %d\n",
			reg, val, val);

	reg = VP_FDET_VFRONT_COUNT_EVEN_IVCTX;
	val = CONNECT2REG(reg);
	front = val;
	if (val)
		seq_printf(s, "vfront_count_even [%x] 0x%x %d\n",
			reg, val, val);

	reg = VP_FDET_VBACK_COUNT_EVEN_IVCTX;
	val = CONNECT2REG(reg);
	back = val;
	if (val)
		seq_printf(s, "vback_count_even [%x] 0x%x %d\n",
			reg, val, val);

	blank = front + sync + back;
	total = active + blank;
	if (blank)
		seq_printf(s, "Calc vblank 0x%x %d\n", blank, blank);
	if (total)
		seq_printf(s, "Calc vtotal 0x%x %d\n", total, total);

	reg = VP_FDET_CONFIG_IVCTX;
	val = hdmitx21_rd_reg(reg);
	if (val)
		seq_printf(s, "[%x] 0x%x %d\n", reg, val, val);

	reg = VP_FDET_STATUS_IVCTX;
	val = hdmitx21_rd_reg(reg);
	if (val) {
		seq_printf(s, "status [%x] 0x%x %d\n", reg, val, val);
		seq_printf(s, "  hsync_polarity %d\n", (val >> 0) & 1);
		seq_printf(s, "  vsync_polarity %d\n", (val >> 1) & 1);
		seq_printf(s, "  interlaced %d\n", (val >> 2) & 1);
		seq_printf(s, "  video656 %d\n", (val >> 3) & 1);
	}

	reg = VP_FDET_INTERLACE_THRESHOLD_IVCTX;
	val = hdmitx21_rd_reg(reg);
	if (val)
		seq_printf(s, "interlace_threshold [%x] %x %d\n",
			reg, val, val);

	reg = VP_FDET_FRAME_RATE_DELTA_THRESHOLD_IVCTX;
	val = CONNECT3REG(reg);
	if (val)
		seq_printf(s, "frame_rate_delta_threshold [%x] 0x%x %d\n",
			reg, val, val);

	reg = VP_FDET_HSYNC_LOW_COUNT_IVCTX;
	val = CONNECT2REG(reg);
	if (val)
		seq_printf(s, "hsync_low_count [%x] 0x%x %d\n", reg, val, val);

	reg = VP_FDET_VSYNC_LOW_COUNT_EVEN_IVCTX;
	val = CONNECT2REG(reg);
	if (val)
		seq_printf(s, "vsync_low_count_even [%x] 0x%x %d\n",
			reg, val, val);

	reg = VP_FDET_VSYNC_LOW_COUNT_ODD_IVCTX;
	val = CONNECT2REG(reg);
	if (val)
		seq_printf(s, "vsync_low_count_odd [%x] 0x%x %d\n",
			reg, val, val);

	reg = VP_FDET_VSYNC_HIGH_COUNT_ODD_IVCTX;
	val = CONNECT2REG(reg);
	if (val)
		seq_printf(s, "vsync_high_count_odd [%x] 0x%x %d\n",
			reg, val, val);

	reg = VP_FDET_VFRONT_COUNT_ODD_IVCTX;
	val = CONNECT2REG(reg);
	if (val)
		seq_printf(s, "vfront_count_odd [%x] 0x%x %d\n", reg, val, val);

	reg = VP_FDET_VBACK_COUNT_ODD_IVCTX;
	val = CONNECT2REG(reg);
	if (val)
		seq_printf(s, "frame_count [%x] 0x%x %d\n", reg, val, val);

	reg = VP_FDET_FRAME_COUNT_IVCTX;
	val = CONNECT2REG(reg);
	if (val)
		seq_printf(s, "frame_count [%x] 0x%x %d\n", reg, val, val);

	reg = VP_FDET__LINE__RATE_DELTA_THRESHOLD_IVCTX;
	val = CONNECT2REG(reg);
	if (val)
		seq_printf(s, "line_rate_delta_threshold [%x] 0x%x %d\n",
			reg, val, val);

	reg = VP_FDET__LINE__RATE_IVCTX;
	val = CONNECT2REG(reg);
	if (val)
		seq_printf(s, "line_rate [%x] 0x%x %d\n", reg, val, val);

	reg = VP_FDET_VSYNC_HSYNC_OFFSET_EVEN_IVCTX;
	val = CONNECT2REG(reg);
	if (val)
		seq_printf(s, "vsync_hsync_offset_even [%x] 0x%x %d\n",
			reg, val, val);

	reg = VP_FDET_VSYNC_HSYNC_OFFSET_ODD_IVCTX;
	val = CONNECT2REG(reg);
	if (val)
		seq_printf(s, "vsync_hsync_offset_odd [%x] 0x%x %d\n",
			reg, val, val);

#define PR_DETAIL(n, str) \
	do { \
		if ((val >> (n)) & 1) \
			seq_printf(s, " %s", str) ; \
	} while (0)

	reg = VP_FDET_IRQ_MASK_IVCTX;
	val = CONNECT3REG(reg);
	if (val) {
		seq_printf(s, "irq_mask [%x] 0x%x %d\n", reg, val, val);

		PR_DETAIL(0, "hsync_polarity");
		PR_DETAIL(1, "vsync_polarity");
		PR_DETAIL(2, "interlaced");
		PR_DETAIL(3, "video656");
		PR_DETAIL(4, "frame_rate");
		PR_DETAIL(5, "pixel_count");
		PR_DETAIL(6, "line_count");
		PR_DETAIL(7, "hsync_low_count");
		PR_DETAIL(8, "hsync_high_count");
		PR_DETAIL(9, "hfront_count");
		PR_DETAIL(10, "hback_count");
		PR_DETAIL(11, "vsync_low_count_even");
		PR_DETAIL(12, "vsync_high_count_even");
		PR_DETAIL(13, "vfront_count_even");
		PR_DETAIL(14, "vback_count_even");
		PR_DETAIL(15, "vsync_low_count_odd");
		PR_DETAIL(16, "vsync_high_count_odd");
		PR_DETAIL(17, "vfront_count_odd");
		PR_DETAIL(18, "vback_count_odd");
		PR_DETAIL(19, "line_rate");
		PR_DETAIL(20, "vsync_hsync_offset_even");
		PR_DETAIL(21, "vsync_hsync_offset_odd");
		PR_DETAIL(22, "frame_and_pixel_cnt_done");
		seq_puts(s, "\n");
	}

	reg = VP_FDET_IRQ_STATUS_IVCTX;
	val = CONNECT3REG(reg);
	if (val) {
		seq_printf(s, "irq_status [%x] 0x%x %d\n",
			reg, val, val);
		PR_DETAIL(0, "hsync_polarity");
		PR_DETAIL(1, "vsync_polarity");
		PR_DETAIL(2, "interlaced");
		PR_DETAIL(3, "video656");
		PR_DETAIL(4, "frame_rate");
		PR_DETAIL(5, "pixel_count");
		PR_DETAIL(6, "line_count");
		PR_DETAIL(7, "hsync_low_count");
		PR_DETAIL(8, "hsync_high_count");
		PR_DETAIL(9, "hfront_count");
		PR_DETAIL(10, "hback_count");
		PR_DETAIL(11, "vsync_low_count_even");
		PR_DETAIL(12, "vsync_high_count_even");
		PR_DETAIL(13, "vfront_count_even");
		PR_DETAIL(14, "vback_count_even");
		PR_DETAIL(15, "vsync_low_count_odd");
		PR_DETAIL(16, "vsync_high_count_odd");
		PR_DETAIL(17, "vfront_count_odd");
		PR_DETAIL(18, "vback_count_odd");
		PR_DETAIL(19, "line_rate");
		PR_DETAIL(20, "vsync_hsync_offset_even");
		PR_DETAIL(21, "vsync_hsync_offset_odd");
		PR_DETAIL(22, "frame_and_pixel_cnt_done");
		seq_puts(s, "\n");
	}
	return 0;
}

static int dump_hdmivpfdet_open(struct inode *inode, struct file *file)
{
	return single_open(file, dump_hdmivpfdet_show, inode->i_private);
}

static const struct file_operations dump_hdmivpfdet_fops = {
	.open		= dump_hdmivpfdet_open,
	.read		= seq_read,
	.release	= single_release,
};

static void hdmitx_parsing_avipkt(struct seq_file *s)
{
}

static void hdmitx_parsing_spdpkt(struct seq_file *s)
{
}

static void hdmitx_parsing_audpkt(struct seq_file *s)
{
}

static void hdmitx_parsing_gcppkt(struct seq_file *s)
{
}

static void hdmitx_parsing_acrpkt(struct seq_file *s)
{
}

static void hdmitx_parsing_audsamp(struct seq_file *s)
{
}

static void hdmitx_parsing_audchannelst(struct seq_file *s)
{
}

static void hdmitx_parsing_hdrpkt(struct seq_file *s)
{
}

static void hdmitx_parsing_vsifpkt(struct seq_file *s)
{
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
	const char *hdmi_ver = HDMITX21_VERSIONS_LOG;

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

	/* TODO */
	return ret;
}

#define AUD_CTS_LOG_NUM	1000
static unsigned int cts_buf[AUD_CTS_LOG_NUM];
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

static int dump_hdmivrr_show(struct seq_file *s, void *p)
{
	return hdmitx_dump_vrr_status(s, p);
}

static int dump_hdmivrr_open(struct inode *inode, struct file *file)
{
	return single_open(file, dump_hdmivrr_show, inode->i_private);
}

static const struct file_operations dump_hdmivrr_fops = {
	.open		= dump_hdmivrr_open,
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
	{"hdmi_vpfdet", S_IFREG | 0444, &dump_hdmivpfdet_fops},
	{"hdmi_pkt", S_IFREG | 0444, &dump_hdmipkt_fops},
	{"hdmi_ver", S_IFREG | 0444, &dump_hdmiver_fops},
	{"aud_cts", S_IFREG | 0444, &dump_audcts_fops},
	{"hdmi_vrr", S_IFREG | 0444, &dump_hdmivrr_fops},
};

static struct dentry *hdmitx_dbgfs;
void hdmitx21_debugfs_init(void)
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
