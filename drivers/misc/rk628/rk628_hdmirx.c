// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2021 Rockchip Electronics Co. Ltd.
 *
 * Author: Chen Shunqing <csq@rock-chips.com>
 */
#include <linux/debugfs.h>
#include <linux/gpio/consumer.h>
#include <linux/of_device.h>

#include "rk628.h"
#include "rk628_combrxphy.h"
#include "rk628_config.h"
#include "rk628_cru.h"
#include "rk628_hdmirx.h"

#define POLL_INTERVAL_MS			1000
#define MODETCLK_CNT_NUM			1000
#define MODETCLK_HZ				49500000
#define RXPHY_CFG_MAX_TIMES			1

static u8 debug;

static u8 edid_init_data[] = {
0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x49, 0x78, 0x28, 0x06, 0x01, 0x00, 0x00, 0x00,
0xFF, 0x21, 0x01, 0x03, 0x80, 0x8B, 0x4E, 0x78, 0x2A, 0x0D, 0xC9, 0xA0, 0x57, 0x47, 0x98, 0x27,
0x12, 0x48, 0x4C, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x02, 0x3A, 0x80, 0x18, 0x71, 0x38, 0x2D, 0x40, 0x58, 0x2C,
0x45, 0x00, 0x20, 0xC2, 0x31, 0x00, 0x00, 0x1E, 0x01, 0x1D, 0x00, 0x72, 0x51, 0xD0, 0x1E, 0x20,
0x6E, 0x28, 0x55, 0x00, 0x20, 0xC2, 0x31, 0x00, 0x00, 0x1E, 0x00, 0x00, 0x00, 0xFC, 0x00, 0x52,
0x4B, 0x36, 0x32, 0x38, 0x0A, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0xFD,
0x00, 0x17, 0x4C, 0x0F, 0x50, 0x1E, 0x00, 0x0A, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x01, 0xB4,

0x02, 0x03, 0x32, 0xF0, 0x41, 0x61, 0x23, 0x09, 0x17, 0x07, 0x83, 0x47, 0x00, 0x00, 0x6E, 0x03,
0x0C, 0x00, 0x20, 0x00, 0xB8, 0x3C, 0x20, 0x00, 0x80, 0x01, 0x02, 0x03, 0x04, 0x67, 0xD8, 0x5D,
0xC4, 0x01, 0x78, 0x80, 0x01, 0xE5, 0x0F, 0x01, 0x00, 0x00, 0x00, 0xE3, 0x05, 0xC3, 0x01, 0xE2,
0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0C,
};

struct rk628_hdmi_mode {
	u32 hdisplay;
	u32 hstart;
	u32 hend;
	u32 htotal;
	u32 vdisplay;
	u32 vstart;
	u32 vend;
	u32 vtotal;
	u32 clock;
	unsigned int flags;
};

struct rk628_hdmirx {
	bool plugin;
	bool res_change;
	struct rk628_hdmi_mode mode;
	u32 input_format;
	u32 fs_audio;
	bool audio_present;
	bool hpd_output_inverted;
	bool src_mode_4K_yuv420;
	bool src_depth_10bit;
	bool phy_lock;
	bool is_hdmi2;
};

void rk628_hdmirx_reset_control_assert(struct rk628 *rk628)
{
	/* presetn_hdmirx */
	rk628_i2c_write(rk628, CRU_SOFTRST_CON02, 0x40004);
	/* resetn_hdmirx_pon */
	rk628_i2c_write(rk628, CRU_SOFTRST_CON02, 0x10001000);
}

void rk628_hdmirx_reset_control_deassert(struct rk628 *rk628)
{
	/* presetn_hdmirx */
	rk628_i2c_write(rk628, CRU_SOFTRST_CON02, 0x40000);
	/* resetn_hdmirx_pon */
	rk628_i2c_write(rk628, CRU_SOFTRST_CON02, 0x10000000);
}

static void hdmirx_phy_write(struct rk628 *rk628, u32 offset, u32 val)
{
	rk628_i2c_write(rk628, HDMI_RX_I2CM_PHYG3_ADDRESS, offset);
	rk628_i2c_write(rk628, HDMI_RX_I2CM_PHYG3_DATAO, val);
	rk628_i2c_write(rk628, HDMI_RX_I2CM_PHYG3_OPERATION, 1);
}

static u32 hdmirx_phy_read(struct rk628 *rk628, u32 offset)
{
	u32 val;

	rk628_i2c_write(rk628, HDMI_RX_I2CM_PHYG3_ADDRESS, offset);
	rk628_i2c_write(rk628, HDMI_RX_I2CM_PHYG3_OPERATION, 2);
	rk628_i2c_read(rk628, HDMI_RX_I2CM_PHYG3_DATAI, &val);

	return val;
}

void rk628_hdmirx_phy_enable(struct rk628 *rk628, bool is_hdmi2)
{
	hdmirx_phy_write(rk628, 0x02, 0x1860);
	hdmirx_phy_write(rk628, 0x03, 0x0060);
	hdmirx_phy_write(rk628, 0x27, 0x1c94);
	hdmirx_phy_write(rk628, 0x28, 0x3713);
	hdmirx_phy_write(rk628, 0x29, 0x24da);
	hdmirx_phy_write(rk628, 0x2a, 0x5492);
	hdmirx_phy_write(rk628, 0x2b, 0x4b0d);
	hdmirx_phy_write(rk628, 0x2d, 0x008c);
	hdmirx_phy_write(rk628, 0x2e, 0x0001);

	if (is_hdmi2)
		hdmirx_phy_write(rk628, 0x0e, 0x0108);
	else
		hdmirx_phy_write(rk628, 0x0e, 0x0008);
}

static int rk628_hdmirx_phy_show(struct seq_file *s, void *v)
{
	struct rk628 *rk628 = s->private;
	u32 i = 0;

	seq_puts(s, "\n>>>rxphy reg ");
	for (i = 0; i <= 0xb7; i++)
		seq_printf(s, "regs %02x val %08x\n",
			   i, hdmirx_phy_read(rk628, i));

	return 0;
}

static int rk628_hdmirx_phy_open(struct inode *inode, struct file *file)
{
	return single_open(file, rk628_hdmirx_phy_show, inode->i_private);
}

static ssize_t
rk628_hdmirx_phy_write(struct file *file, const char __user *buf,
		     size_t count, loff_t *ppos)
{
	struct rk628 *rk628 =
		((struct seq_file *)file->private_data)->private;
	u32 reg, val;
	char kbuf[25];

	if (count > 24) {
		dev_err(rk628->dev, "out of buf range\n");
		return count;
	}

	if (copy_from_user(kbuf, buf, count))
		return -EFAULT;
	kbuf[count - 1] = '\0';

	if (sscanf(kbuf, "%x %x", &reg, &val) == -1)
		return -EFAULT;
	if (reg > 0xb7) {
		dev_err(rk628->dev, "it is no a rxphy register\n");
		return count;
	}
	dev_info(rk628->dev, "/**********rxphy register config******/");
	dev_info(rk628->dev, "\n reg=%x val=%x\n", reg, val);
	hdmirx_phy_write(rk628, reg, val);

	return count;
}

static const struct file_operations rk628_hdmirx_phy_fops = {
	.owner = THIS_MODULE,
	.open = rk628_hdmirx_phy_open,
	.read = seq_read,
	.write = rk628_hdmirx_phy_write,
	.llseek = seq_lseek,
	.release = single_release,
};

void rk628_hdmirx_phy_set_clrdpt(struct rk628 *rk628, bool is_8bit)
{
	if (is_8bit)
		hdmirx_phy_write(rk628, 0x03, 0x0000);
	else
		hdmirx_phy_write(rk628, 0x03, 0x0060);
}

static void rk628_hdmirx_ctrl_enable(struct rk628 *rk628)
{
	rk628_i2c_update_bits(rk628, GRF_SYSTEM_CON0,
	     SW_INPUT_MODE_MASK,
	     SW_INPUT_MODE(INPUT_MODE_HDMI));

	rk628_i2c_write(rk628, HDMI_RX_HDMI20_CONTROL, 0x10001f11);
	/* Support DVI mode */
	rk628_i2c_write(rk628, HDMI_RX_HDMI_TIMER_CTRL, 0xa78);
	rk628_i2c_write(rk628, HDMI_RX_HDMI_MODE_RECOVER, 0x00000021);
	rk628_i2c_write(rk628, HDMI_RX_PDEC_CTRL, 0xbfff8011);
	rk628_i2c_write(rk628, HDMI_RX_PDEC_ASP_CTRL, 0x00000040);
	rk628_i2c_write(rk628, HDMI_RX_HDMI_RESMPL_CTRL, 0x00000001);
	rk628_i2c_write(rk628, HDMI_RX_HDMI_SYNC_CTRL, 0x00000014);
	rk628_i2c_write(rk628, HDMI_RX_PDEC_ERR_FILTER, 0x00000008);
	rk628_i2c_write(rk628, HDMI_RX_SCDC_I2CCONFIG, 0x01000000);
	rk628_i2c_write(rk628, HDMI_RX_SCDC_CONFIG, 0x00000001);
	rk628_i2c_write(rk628, HDMI_RX_SCDC_WRDATA0, 0xabcdef01);
	rk628_i2c_write(rk628, HDMI_RX_CHLOCK_CONFIG, 0x0030c15c);
	rk628_i2c_write(rk628, HDMI_RX_HDMI_ERROR_PROTECT, 0x000d0c98);
	rk628_i2c_write(rk628, HDMI_RX_MD_HCTRL1, 0x00000010);
	/*
	 * rk628f should set start of horizontal measurement to 3/8 of frame duration
	 * to pass hdmi 2.0 cts
	 */
	if (rk628->version == RK628D_VERSION)
		rk628_i2c_write(rk628, HDMI_RX_MD_HCTRL2, 0x00001738);
	else
		rk628_i2c_write(rk628, HDMI_RX_MD_HCTRL2, 0x0000173a);
	rk628_i2c_write(rk628, HDMI_RX_MD_VCTRL, 0x00000002);
	rk628_i2c_write(rk628, HDMI_RX_MD_VTH, 0x0000073a);
	rk628_i2c_write(rk628, HDMI_RX_MD_IL_POL, 0x00000004);
	rk628_i2c_write(rk628, HDMI_RX_PDEC_ACRM_CTRL, 0x00000000);
	rk628_i2c_write(rk628, HDMI_RX_HDMI_DCM_CTRL, 0x00040414);
	rk628_i2c_write(rk628, HDMI_RX_HDMI_CKM_EVLTM, 0x00103e70);
	rk628_i2c_write(rk628, HDMI_RX_HDMI_CKM_F, 0x0c1c0b54);
	rk628_i2c_write(rk628, HDMI_RX_HDMI_RESMPL_CTRL, 0x00000001);

	rk628_i2c_update_bits(rk628, HDMI_RX_HDCP_SETTINGS,
	     HDMI_RESERVED_MASK |
	     FAST_I2C_MASK |
	     ONE_DOT_ONE_MASK |
	     FAST_REAUTH_MASK,
	     HDMI_RESERVED(1) |
	     FAST_I2C(0) |
	     ONE_DOT_ONE(1) |
	     FAST_REAUTH(1));
}

static void rk628_hdmirx_video_unmute(struct rk628 *rk628, u8 unmute)
{
	rk628_i2c_update_bits(rk628, HDMI_RX_DMI_DISABLE_IF, VID_ENABLE_MASK, VID_ENABLE(unmute));
}

static void rk628_hdmirx_hpd_ctrl(struct rk628 *rk628, bool en)
{
	u8 en_level, set_level;
	struct rk628_hdmirx *hdmirx = rk628->hdmirx;

	dev_dbg(rk628->dev, "%s: %sable, hpd invert:%d\n", __func__,
		en ? "en" : "dis", hdmirx->hpd_output_inverted);
	en_level = hdmirx->hpd_output_inverted ? 0 : 1;
	set_level = en ? en_level : !en_level;

	rk628_misc_gpio_direction_output(rk628, GPIO1_B0, set_level);
}

static void rk628_hdmirx_disable_edid(struct rk628 *rk628)
{
	rk628_hdmirx_hpd_ctrl(rk628, false);
	rk628_hdmirx_video_unmute(rk628, 0);
}

static void rk628_hdmirx_enable_edid(struct rk628 *rk628)
{
	rk628_i2c_write(rk628, HDMI_RX_SCDC_CONFIG, 0x00000001);
	rk628_hdmirx_hpd_ctrl(rk628, true);
}

static int tx_5v_power_present(struct rk628 *rk628)
{
	bool ret;
	int val, i, cnt;

	/* Direct Mode */
	if (!rk628->plugin_det_gpio)
		return 1;

	cnt = 0;
	for (i = 0; i < 5; i++) {
		val = gpiod_get_value(rk628->plugin_det_gpio);
		if (val > 0)
			cnt++;
		usleep_range(500, 600);
	}

	ret = (cnt == 5) ? 1 : 0;
	dev_dbg(rk628->dev, "%s: %d\n", __func__, ret);

	return ret;
}

static int rk628_hdmirx_init_edid(struct rk628 *rk628)
{
	struct rk628_display_mode *src_mode;
	struct rk628_hdmirx *hdmirx = rk628->hdmirx;
	u32 val;
	u8 csum = 0;
	int i, base = 0x36;

	src_mode = rk628_display_get_src_mode(rk628);
	csum = 0;
	/* clock-frequency */
	edid_init_data[base + 1] = ((src_mode->clock / 10) & 0xff00) >> 8;
	edid_init_data[base] = (src_mode->clock / 10) & 0xff;
	/* hactive low 8 bits */
	edid_init_data[base + 2]  = src_mode->hdisplay & 0xff;
	/* hblanking low 8 bits */
	val = src_mode->htotal - src_mode->hdisplay;
	edid_init_data[base + 3] = val & 0xff;
	/* hactive high 4 bits & hblanking low 4 bits */
	edid_init_data[base + 4] =
		((src_mode->hdisplay & 0xf00) >> 4) + ((val & 0xf00) >> 8);
	/* vactive low 8 bits */
	edid_init_data[base + 5] = src_mode->vdisplay & 0xff;
	/* vblanking low 8 bits */
	val = src_mode->vtotal - src_mode->vdisplay;
	edid_init_data[base + 6] = val & 0xff;
	/* vactive high 4 bits & vblanking low 4 bits */
	edid_init_data[base + 7] =
		((src_mode->vdisplay & 0xf00) >> 4) + ((val & 0xf00) >> 8);
	/* hsync pulse offset low 8 bits */
	val = src_mode->hsync_start - src_mode->hdisplay;
	edid_init_data[base + 8] = val & 0xff;
	/* hsync pulse width low 8 bits */
	val = src_mode->hsync_end - src_mode->hsync_start;
	edid_init_data[base + 9] = val & 0xff;
	/* vsync pulse offset low 4 bits & vsync pulse width low 4 bits */
	val = ((src_mode->vsync_start - src_mode->vdisplay) & 0xf) << 4;
	edid_init_data[base + 10] = val;
	edid_init_data[base + 10] += (src_mode->vsync_end - src_mode->vsync_start) & 0xf;
	/* 6~7bits:hsync pulse offset;
	 * 4~6bits:hsync pulse width;
	 * 2~3bits:vsync pulse offset;
	 * 0~1bits:vsync pulse width
	 */
	edid_init_data[base + 11] =
		((src_mode->hsync_start - src_mode->hdisplay) & 0x300) >> 2;
	edid_init_data[base + 11] +=
		((src_mode->hsync_end - src_mode->hsync_start) & 0x700) >> 4;
	edid_init_data[base + 11] +=
		((src_mode->vsync_start - src_mode->vdisplay) & 0x30) >> 2;
	edid_init_data[base + 11] +=
		((src_mode->vsync_end - src_mode->vsync_start) & 0x30) >> 4;
	edid_init_data[base + 17]  = 0x18;
	if (src_mode->flags & DRM_MODE_FLAG_PHSYNC)
		edid_init_data[base + 17] |= 0x2;
	if (src_mode->flags & DRM_MODE_FLAG_PVSYNC)
		edid_init_data[base + 17] |= 0x4;
	/* set edid max tmds clk to 340m, hdmi2.0 only support yuv420 */
	if (hdmirx->src_mode_4K_yuv420)
		edid_init_data[0x80 + 34] = 0x44;
	for (i = 0; i < 127; i++)
		csum += edid_init_data[i];
	edid_init_data[127] = (u8)0 - csum;

	return 0;
}

static int rk628_hdmirx_set_edid(struct rk628 *rk628)
{
	int i;
	u32 val;
	u16 edid_len;

	rk628_hdmirx_disable_edid(rk628);

	if (!rk628->plugin_det_gpio)
		return 0;

	/* edid access by apb when write, i2c slave addr: 0x0 */
	rk628_i2c_update_bits(rk628, GRF_SYSTEM_CON0,
	     SW_ADAPTER_I2CSLADR_MASK |
	     SW_EDID_MODE_MASK,
	     SW_ADAPTER_I2CSLADR(0) |
	     SW_EDID_MODE(1));

	rk628_hdmirx_init_edid(rk628);

	edid_len = ARRAY_SIZE(edid_init_data);
	for (i = 0; i < edid_len; i++)
		rk628_i2c_write(rk628, EDID_BASE + i * 4, edid_init_data[i]);

	/* read out for debug */
	if (debug >= 3) {
		pr_info("====== Read EDID: ======\n");
		for (i = 0; i < edid_len; i++) {
			rk628_i2c_read(rk628, EDID_BASE + i * 4, &val);
			pr_info("0x%02x ", val);
			if ((i + 1) % 8 == 0)
				pr_info("\n");
		}
		pr_info("============\n");
	}

	/* edid access by RX's i2c, i2c slave addr: 0x0 */
	rk628_i2c_update_bits(rk628, GRF_SYSTEM_CON0,
	     SW_ADAPTER_I2CSLADR_MASK |
	     SW_EDID_MODE_MASK,
	     SW_ADAPTER_I2CSLADR(0) |
	     SW_EDID_MODE(0));

	mdelay(1);

	return 0;
}

static int rk628d_hdmirx_phy_power_on(struct rk628 *rk628, int f)
{
	int ret;
	bool rxphy_pwron = false;

	if (rxphy_pwron) {
		dev_info(rk628->dev, "rxphy already power on, power off!\n");
		ret = rk628_combrxphy_power_off(rk628);
		if (ret)
			dev_info(rk628->dev, "hdmi rxphy power off failed!\n");
		else
			rxphy_pwron = false;
	}

	udelay(1000);
	if (rxphy_pwron == false) {
		ret = rk628_combrxphy_power_on(rk628, f);
		if (ret) {
			rxphy_pwron = false;
			dev_info(rk628->dev, "hdmi rxphy power on failed\n");
		} else {
			rxphy_pwron = true;
			dev_info(rk628->dev, "hdmi rxphy power on success\n");
		}
	}

	dev_info(rk628->dev, "%s:rxphy_pwron=%d\n", __func__, rxphy_pwron);
	return ret;
}

static void rk628_hdmirx_get_timing(struct rk628 *rk628)
{
	u32 hact, vact, htotal, vtotal, fps, status;
	u32 val;
	u32 modetclk_cnt_hs, modetclk_cnt_vs, hs, vs;
	u32 hofs_pix, hbp, hfp, vbp, vfp;
	u32 tmds_clk, tmdsclk_cnt, modetclk_hz;
	u64 tmp_data;
	u32 interlaced;
	u32 hfrontporch, hsync, hbackporch, vfrontporch, vsync, vbackporch;
	unsigned long long pixelclock;
	unsigned long flags = 0;
	struct rk628_hdmirx *hdmirx = rk628->hdmirx;

	rk628_i2c_read(rk628, HDMI_RX_SCDC_REGS1, &val);
	status = val;

	rk628_i2c_read(rk628, HDMI_RX_MD_STS, &val);
	interlaced = val & ILACE_STS ? 1 : 0;

	rk628_i2c_read(rk628, HDMI_RX_MD_HACT_PX, &val);
	hact = val & 0xffff;
	rk628_i2c_read(rk628, HDMI_RX_MD_VAL, &val);
	vact = val & 0xffff;
	rk628_i2c_read(rk628, HDMI_RX_MD_HT1, &val);
	htotal = (val >> 16) & 0xffff;
	rk628_i2c_read(rk628, HDMI_RX_MD_VTL, &val);
	vtotal = val & 0xffff;
	rk628_i2c_read(rk628, HDMI_RX_MD_HT1, &val);
	hofs_pix = val & 0xffff;
	rk628_i2c_read(rk628, HDMI_RX_MD_VOL, &val);
	vbp = (val & 0xffff) + 1;

	modetclk_hz = rk628_cru_clk_get_rate(rk628, CGU_CLK_CPLL) / 24;
	rk628_i2c_read(rk628, HDMI_RX_HDMI_CKM_RESULT, &val);
	tmdsclk_cnt = val & 0xffff;
	tmp_data = tmdsclk_cnt;
	/* rk628d modet clk is always 49.5m, rk628f's freq changes with ref clock */
	if (rk628->version != RK628D_VERSION)
		tmp_data = ((tmp_data * modetclk_hz) + MODETCLK_CNT_NUM / 2);
	else
		tmp_data = ((tmp_data * MODETCLK_HZ) + MODETCLK_CNT_NUM / 2);
	do_div(tmp_data, MODETCLK_CNT_NUM);
	tmds_clk = tmp_data;
	if (!(htotal && vtotal)) {
		dev_info(rk628->dev, "timing err, htotal:%d, vtotal:%d\n", htotal, vtotal);
		return;
	}
	/* rk628f should get exact frame rate frequency to pass hdmi2.0 cts */
	if (rk628->version != RK628D_VERSION)
		fps = tmds_clk  / (htotal * vtotal);
	else
		fps = (tmds_clk + (htotal * vtotal) / 2) / (htotal * vtotal);

	rk628_i2c_read(rk628, HDMI_RX_MD_HT0, &val);
	modetclk_cnt_hs = val & 0xffff;
	hs = (tmdsclk_cnt * modetclk_cnt_hs + MODETCLK_CNT_NUM / 2) /
		MODETCLK_CNT_NUM;

	rk628_i2c_read(rk628, HDMI_RX_MD_VSC, &val);
	modetclk_cnt_vs = val & 0xffff;
	vs = (tmdsclk_cnt * modetclk_cnt_vs + MODETCLK_CNT_NUM / 2) /
		MODETCLK_CNT_NUM;
	vs = (vs + htotal / 2) / htotal;

	rk628_i2c_read(rk628, HDMI_RX_HDMI_STS, &val);
	if (val & BIT(8))
		flags |= DRM_MODE_FLAG_PHSYNC;
	else
		flags |= DRM_MODE_FLAG_NHSYNC;
	if (val & BIT(9))
		flags |= DRM_MODE_FLAG_PVSYNC;
	else
		flags |= DRM_MODE_FLAG_NVSYNC;

	if ((hofs_pix < hs) || (htotal < (hact + hofs_pix)) ||
	    (vtotal < (vact + vs + vbp))) {
		dev_info(rk628->dev,
			 "timing err, total:%dx%d, act:%dx%d, hofs:%d, hs:%d, vs:%d, vbp:%d\n",
			 htotal, vtotal, hact, vact, hofs_pix, hs, vs, vbp);
		return;
	}
	hbp = hofs_pix - hs;
	hfp = htotal - hact - hofs_pix;
	vfp = vtotal - vact - vs - vbp;

	dev_info(rk628->dev, "cnt_num:%d, tmds_cnt:%d, hs_cnt:%d, vs_cnt:%d, hofs:%d\n",
		 MODETCLK_CNT_NUM, tmdsclk_cnt, modetclk_cnt_hs, modetclk_cnt_vs, hofs_pix);

	hfrontporch = hfp;
	hsync = hs;
	hbackporch = hbp;
	vfrontporch = vfp;
	vsync = vs;
	vbackporch = vbp;
	/* rk628f should get exact pixel clk frequency to pass hdmi2.0 cts */
	if (rk628->version != RK628D_VERSION)
		pixelclock = tmds_clk;
	else
		pixelclock = htotal * vtotal * fps;

	if (interlaced == 1) {
		vsync = vsync + 1;
		pixelclock /= 2;
	}

	hdmirx->mode.clock = pixelclock / 1000;
	hdmirx->mode.hdisplay = hact;
	hdmirx->mode.hstart = hdmirx->mode.hdisplay + hfrontporch;
	hdmirx->mode.hend = hdmirx->mode.hstart + hsync;
	hdmirx->mode.htotal = hdmirx->mode.hend + hbackporch;

	hdmirx->mode.vdisplay = vact;
	hdmirx->mode.vstart = hdmirx->mode.vdisplay + vfrontporch;
	hdmirx->mode.vend = hdmirx->mode.vstart + vsync;
	hdmirx->mode.vtotal = hdmirx->mode.vend + vbackporch;
	hdmirx->mode.flags = flags;

	dev_info(rk628->dev, "SCDC_REGS1:%#x, act:%dx%d, total:%dx%d, fps:%d, pixclk:%llu\n",
		 status, hact, vact, htotal, vtotal, fps, pixelclock);
	dev_info(rk628->dev, "hfp:%d, hs:%d, hbp:%d, vfp:%d, vs:%d, vbp:%d, interlace:%d\n",
		 hfrontporch, hsync, hbackporch, vfrontporch, vsync, vbackporch, interlaced);
}

static void rk628f_hdmirx_phy_power_on(struct rk628 *rk628)
{
	struct rk628_hdmirx *hdmirx = rk628->hdmirx;

	/* power down phy */
	rk628_i2c_write(rk628, GRF_SW_HDMIRXPHY_CRTL, 0x17);
	usleep_range(20, 30);
	rk628_i2c_write(rk628, GRF_SW_HDMIRXPHY_CRTL, 0x15);
	/* init phy i2c */
	rk628_i2c_write(rk628, HDMI_RX_SNPS_PHYG3_CTRL, 0);
	rk628_i2c_write(rk628, HDMI_RX_I2CM_PHYG3_SS_CNTS, 0x018c01d2);
	rk628_i2c_write(rk628, HDMI_RX_I2CM_PHYG3_FS_HCNT, 0x003c0081);
	rk628_i2c_write(rk628, HDMI_RX_I2CM_PHYG3_MODE, 1);
	rk628_i2c_write(rk628, GRF_SW_HDMIRXPHY_CRTL, 0x11);
	/* enable rx phy */
	rk628_hdmirx_phy_enable(rk628, hdmirx->is_hdmi2);
	rk628_i2c_write(rk628, GRF_SW_HDMIRXPHY_CRTL, 0x14);
}

static void rk628_hdmirx_phy_prepclk_cfg(struct rk628 *rk628)
{
	u32 format;
	bool is_clrdpt_8bit = false;

	rk628_i2c_read(rk628, HDMI_RX_PDEC_AVI_PB, &format);
	format = (format & VIDEO_FORMAT) >> 5;

	/* yuv420 should set phy color depth 8bit */
	if (format == 3)
		is_clrdpt_8bit = true;

	rk628_i2c_read(rk628, HDMI_RX_PDEC_GCP_AVMUTE, &format);
	format = (format & PKTDEC_GCP_CD_MASK) >> 4;

	/* 10bit color depth should set phy color depth 8bit */
	if (format == 5)
		is_clrdpt_8bit = true;

	rk628_hdmirx_phy_set_clrdpt(rk628, is_clrdpt_8bit);
}

static int rk628_hdmirx_phy_setup(struct rk628 *rk628)
{
	u32 i, cnt, val, status, vs;
	u32 width, height, frame_width, frame_height, tmdsclk_cnt, modetclk_cnt_hs, modetclk_cnt_vs;
	struct rk628_display_mode *src_mode;
	struct rk628_hdmirx *hdmirx = rk628->hdmirx;
	u32 f;
	struct rk628_display_mode *dst_mode;
	bool signal_input, timing_detected;

	dst_mode = rk628_display_get_dst_mode(rk628);
	src_mode = rk628_display_get_src_mode(rk628);
	f = src_mode->clock;
	/* Bit31 is used to distinguish HDMI cable mode and direct connection
	 * mode in the rk628_combrxphy driver.
	 * Bit31: 0 -direct connection mode;
	 *    1 -cable mode;
	 * The cable mode is to know the input clock frequency through cdr_mode
	 * in the rk628_combrxphy driver, and the cable mode supports up to
	 * 297M, so 297M is passed uniformly here.
	 */
	if (rk628->plugin_det_gpio)
		f |= BIT(31);
	/*
	 * force 594m mode to yuv420 format.
	 * bit30 is used to indicate whether it is yuv420 format.
	 */
	if (hdmirx->src_mode_4K_yuv420)
		f |= BIT(30);

	if (!rk628->plugin_det_gpio) {
		u32 tmds_rate = src_mode->clock;

		if (hdmirx->src_mode_4K_yuv420)
			tmds_rate /= 2;
		if (hdmirx->src_depth_10bit)
			tmds_rate = tmds_rate * 10 / 8;
		if (tmds_rate >= 340000)
			hdmirx->is_hdmi2 = true;
		else
			hdmirx->is_hdmi2 = false;
	}

	/* enable scramble in hdmi2.0 mode */
	if (hdmirx->is_hdmi2 && !rk628->plugin_det_gpio)
		rk628_i2c_write(rk628, HDMI_RX_HDMI20_CONTROL, 0x10001f13);

	for (i = 0; i < RXPHY_CFG_MAX_TIMES; i++) {
		if (rk628->version == RK628D_VERSION)
			rk628d_hdmirx_phy_power_on(rk628, f);
		else if (rk628->version == RK628F_VERSION)
			rk628f_hdmirx_phy_power_on(rk628);

		signal_input = false;

		cnt = 0;

		do {
			if (signal_input || rk628->plugin_det_gpio)
				cnt++;
			msleep(100);
			rk628_i2c_read(rk628, HDMI_RX_MD_HACT_PX, &val);
			width = val & 0xffff;
			rk628_i2c_read(rk628, HDMI_RX_MD_HT1, &val);
			frame_width = (val >> 16) & 0xffff;

			rk628_i2c_read(rk628, HDMI_RX_MD_VAL, &val);
			height = val & 0xffff;
			rk628_i2c_read(rk628, HDMI_RX_MD_VTL, &val);
			frame_height = val & 0xffff;

			rk628_i2c_read(rk628, HDMI_RX_HDMI_CKM_RESULT, &val);
			tmdsclk_cnt = val & 0xffff;
			rk628_i2c_read(rk628, HDMI_RX_MD_HT0, &val);
			modetclk_cnt_hs = val & 0xffff;
			rk628_i2c_read(rk628, HDMI_RX_MD_VSC, &val);
			modetclk_cnt_vs = val & 0xffff;

			vs = (tmdsclk_cnt * modetclk_cnt_vs + MODETCLK_CNT_NUM / 2) /
				MODETCLK_CNT_NUM;
			vs = (vs + frame_width / 2) / frame_width;

			if (width && height && frame_width && frame_height && tmdsclk_cnt &&
			    modetclk_cnt_hs && modetclk_cnt_vs && vs)
				timing_detected = true;
			else
				timing_detected = false;

			rk628_i2c_read(rk628, HDMI_RX_SCDC_REGS1, &val);
			status = val;

			dev_info(rk628->dev,
				 "tmdsclk_cnt:%d, modetclk_cnt_hs:%d, modetclk_cnt_vs:%d,vs:%d\n",
				 tmdsclk_cnt, modetclk_cnt_hs, modetclk_cnt_vs, vs);
			dev_info(rk628->dev,
				 "read wxh:%dx%d, total:%dx%d, SCDC_REGS1:%#x, cnt:%d\n",
				 width, height, frame_width,
				 frame_height, status, cnt);

			/* no signal input, wait */
			if (status & 0xf00)
				signal_input = true;

			if (cnt >= 15)
				break;
		} while (((status & 0xfff) != 0xf00) || !timing_detected);

		if (((status & 0xfff) != 0xf00) || ((status >> 16) > 0xc000)) {
			dev_info(rk628->dev, "%s hdmi rxphy lock failed, retry:%d, status:0x%x\n",
				 __func__, i, status);
			if (((status >> 16) > 0xc000))
				dev_info(rk628->dev, "((status >> 16) > 0xc000)\n");
			continue;
		} else {
			if (rk628->version == RK628F_VERSION)
				rk628_hdmirx_phy_prepclk_cfg(rk628);

			rk628_hdmirx_get_timing(rk628);

			if (hdmirx->mode.flags & DRM_MODE_FLAG_INTERLACE) {
				dev_info(rk628->dev, "interlace mode is unsupported\n");
				continue;
			}

			if (hdmirx->mode.clock == 0)
				return -EINVAL;

			src_mode->clock = hdmirx->mode.clock;
			src_mode->hdisplay = hdmirx->mode.hdisplay;
			src_mode->hsync_start = hdmirx->mode.hstart;
			src_mode->hsync_end = hdmirx->mode.hend;
			src_mode->htotal = hdmirx->mode.htotal;

			src_mode->vdisplay = hdmirx->mode.vdisplay;
			src_mode->vsync_start = hdmirx->mode.vstart;
			src_mode->vsync_end = hdmirx->mode.vend;
			src_mode->vtotal = hdmirx->mode.vtotal;
			src_mode->flags = hdmirx->mode.flags;
			if (hdmirx->src_mode_4K_yuv420 && dst_mode->clock == 594000) {
				rk628_mode_copy(src_mode, dst_mode);
				src_mode->flags = DRM_MODE_FLAG_PHSYNC|DRM_MODE_FLAG_PVSYNC;
			}

			break;
		}
	}

	if (i == RXPHY_CFG_MAX_TIMES) {
		hdmirx->phy_lock = false;
		return -1;
	}
	hdmirx->phy_lock = true;

	return 0;
}

static u32 rk628_hdmirx_get_input_format(struct rk628 *rk628)
{
	u32 val, format, avi_pb = 0;
	u8 i;
	u8 cnt = 0, max_cnt = 2;
	struct rk628_hdmirx *hdmirx = rk628->hdmirx;

	rk628_i2c_read(rk628, HDMI_RX_PDEC_ISTS, &val);
	if (val & AVI_RCV_ISTS) {
		for (i = 0; i < 100; i++) {
			rk628_i2c_read(rk628, HDMI_RX_PDEC_AVI_PB, &format);
			dev_dbg(rk628->dev, "%s PDEC_AVI_PB:%#x\n", __func__, format);
			if (format && format == avi_pb) {
				if (++cnt >= max_cnt)
					break;
			} else {
				cnt = 0;
				avi_pb = format;
			}
			msleep(30);
		}
		format  = (avi_pb & VIDEO_FORMAT) >> 5;
		switch (format) {
		case 0:
			hdmirx->input_format = BUS_FMT_RGB;
			break;
		case 1:
			hdmirx->input_format = BUS_FMT_YUV422;
			break;
		case 2:
			hdmirx->input_format = BUS_FMT_YUV444;
			break;
		case 3:
			hdmirx->input_format = BUS_FMT_YUV420;
			break;
		default:
			hdmirx->input_format = BUS_FMT_RGB;
			break;
		}
		rk628_i2c_write(rk628, HDMI_RX_PDEC_ICLR, AVI_RCV_ISTS);
	}

	return hdmirx->input_format;
}

static int rk628_check_signal(struct rk628 *rk628)
{
	u32 hact, vact, val;

	rk628_i2c_read(rk628, HDMI_RX_MD_HACT_PX, &val);
	hact = val & 0xffff;
	rk628_i2c_read(rk628, HDMI_RX_MD_VAL, &val);
	vact = val & 0xffff;

	if (!hact || !vact) {
		dev_info(rk628->dev, "no signal\n");
		return 0;
	}

	return 1;
}

static bool rk628_hdmirx_status_change(struct rk628 *rk628)
{
	u32 hact, vact, val;
	struct rk628_hdmirx *hdmirx = rk628->hdmirx;

	rk628_i2c_read(rk628, HDMI_RX_MD_HACT_PX, &val);
	hact = val & 0xffff;
	rk628_i2c_read(rk628, HDMI_RX_MD_VAL, &val);
	vact = val & 0xffff;
	if (!rk628->plugin_det_gpio && !hact && !vact)
		return true;

	if (hact != hdmirx->mode.hdisplay || vact != hdmirx->mode.vdisplay) {
		dev_info(rk628->dev, "new: hdisplay=%d, vdisplay=%d\n", hact, vact);
		dev_info(rk628->dev, "old: hdisplay=%d, vdisplay=%d\n",
			 hdmirx->mode.hdisplay, hdmirx->mode.vdisplay);
		return true;
	}

	rk628_hdmirx_get_input_format(rk628);
	if (hdmirx->input_format != rk628_get_input_bus_format(rk628))
		return true;

	return false;
}

static int rk628_hdmirx_init(struct rk628 *rk628)
{
	struct rk628_hdmirx *hdmirx;
	struct device *dev = rk628->dev;

	hdmirx = devm_kzalloc(rk628->dev, sizeof(*hdmirx), GFP_KERNEL);
	if (!hdmirx)
		return -ENOMEM;
	rk628->hdmirx = hdmirx;

	/*
	 * rk628f hdmirx phy can only access via hdmirx controller inner i2c.
	 * rk628d phy regs can access via rk628 i2c, there is no need to create
	 * debugfs.
	 */
	if (rk628->version != RK628D_VERSION && !IS_ERR(rk628->debug_dir))
		debugfs_create_file("hdmirx_phy", 0600, rk628->debug_dir,
				    rk628, &rk628_hdmirx_phy_fops);

	hdmirx->hpd_output_inverted = of_property_read_bool(dev->of_node, "hpd-output-inverted");

	hdmirx->src_mode_4K_yuv420 = of_property_read_bool(dev->of_node, "src-mode-4k-yuv420");

	hdmirx->src_depth_10bit = of_property_read_bool(dev->of_node, "src-depth-10bit");

	/* HDMIRX IOMUX */
	rk628_i2c_write(rk628, GRF_GPIO1AB_SEL_CON, HIWORD_UPDATE(0x7, 10, 8));
	//i2s pinctrl
	rk628_i2c_write(rk628, GRF_GPIO0AB_SEL_CON, 0x155c155c);

	/* if GVI and HDMITX OUT, HDMIRX missing signal */
	rk628_i2c_update_bits(rk628, GRF_SYSTEM_CON0,
			      SW_OUTPUT_RGB_MODE_MASK,
			      SW_OUTPUT_RGB_MODE(OUTPUT_MODE_RGB >> 3));

	rk628_i2c_update_bits(rk628, GRF_SYSTEM_CON0,
			      SW_INPUT_MODE_MASK, SW_INPUT_MODE(INPUT_MODE_HDMI));
	rk628_hdmirx_set_edid(rk628);
	/* clear avi rcv interrupt */
	rk628_i2c_write(rk628, HDMI_RX_PDEC_ICLR, AVI_RCV_ISTS);

	dev_info(rk628->dev, "hdmirx driver version: %s\n", DRIVER_VERSION);

	return 0;
}

void rk628_hdmirx_enable_interrupts(struct rk628 *rk628, bool en)
{
	u32 md_ien;
	u32 md_mask = 0;

	md_mask = VACT_LIN_ENSET | HACT_PIX_ENSET | HS_CLK_ENSET | DE_ACTIVITY_ENSET |
		VS_ACT_ENSET | HS_ACT_ENSET;

	dev_dbg(rk628->dev, "%s: %sable\n", __func__, en ? "en" : "dis");
	if (en) {
		/* clr irq */
		rk628_i2c_write(rk628, HDMI_RX_MD_ICLR, md_mask);
		rk628_i2c_write(rk628, HDMI_RX_MD_IEN_SET, md_mask);
	} else {
		rk628_i2c_write(rk628, HDMI_RX_MD_IEN_CLR, md_mask);
		rk628_i2c_write(rk628, HDMI_RX_AUD_FIFO_IEN_CLR, 0x1f);
	}
	usleep_range(5000, 5000);
	rk628_i2c_read(rk628, HDMI_RX_MD_IEN, &md_ien);
	dev_dbg(rk628->dev, "%s MD_IEN:%#x\n", __func__, md_ien);
}

int rk628_hdmirx_enable(struct rk628 *rk628)
{
	int ret;
	u32 val;
	struct rk628_hdmirx *hdmirx;

	if (!rk628->hdmirx) {
		ret = rk628_hdmirx_init(rk628);
		if (ret < 0)
			return HDMIRX_PLUGOUT;
	}

	hdmirx = rk628->hdmirx;

	if (tx_5v_power_present(rk628)) {
		int i;

		hdmirx->plugin = true;
		hdmirx->is_hdmi2 = false;
		rk628_hdmirx_enable_edid(rk628);
		for (i = 0; i < 20; i++) {
			msleep(50);
			rk628_i2c_read(rk628, HDMI_RX_SCDC_REGS0, &val);
			dev_dbg(rk628->dev, "HDMI_RX_SCDC_REGS0:0x%x\n", val);
			if (val & SCDC_TMDSBITCLKRATIO) {
				hdmirx->is_hdmi2 = true;
				break;
			}
		}

		/* rst for hdmirx */
		rk628_hdmirx_reset_control_assert(rk628);
		usleep_range(20, 40);
		rk628_hdmirx_reset_control_deassert(rk628);
		usleep_range(20, 40);

		rk628_hdmirx_ctrl_enable(rk628);
		ret = rk628_hdmirx_phy_setup(rk628);
		if (ret < 0)
			return HDMIRX_PLUGIN | HDMIRX_NOSIGNAL;

		rk628_hdmirx_get_input_format(rk628);
		rk628_set_input_bus_format(rk628, hdmirx->input_format);
		dev_info(rk628->dev, "hdmirx plug in\n");
		dev_info(rk628->dev, "input: %d, output: %d\n", hdmirx->input_format,
			 rk628_get_output_bus_format(rk628));
		if (!rk628_check_signal(rk628) || !hdmirx->phy_lock)
			return HDMIRX_PLUGIN | HDMIRX_NOSIGNAL;

		dev_info(rk628->dev, "hdmirx success\n");
		rk628_hdmirx_video_unmute(rk628, 1);

		return HDMIRX_PLUGIN;
	}

	hdmirx->plugin = false;
	rk628_hdmirx_disable_edid(rk628);
	rk628_i2c_update_bits(rk628, GRF_SYSTEM_CON0, SW_I2S_DATA_OEN_MASK, SW_I2S_DATA_OEN(1));

	return HDMIRX_PLUGOUT;
}

void rk628_hdmirx_disable(struct rk628 *rk628)
{
	int ret;
	struct rk628_hdmirx *hdmirx;

	if (!rk628->hdmirx) {
		ret = rk628_hdmirx_init(rk628);
		if (ret < 0)
			return;
	}

	hdmirx = rk628->hdmirx;
	if (!tx_5v_power_present(rk628)) {
		hdmirx->plugin = false;
		rk628_hdmirx_disable_edid(rk628);
		rk628_i2c_update_bits(rk628, GRF_SYSTEM_CON0, SW_I2S_DATA_OEN_MASK,
				      SW_I2S_DATA_OEN(1));
		dev_info(rk628->dev, "hdmirx plug out\n");
	}
}

int rk628_hdmirx_detect(struct rk628 *rk628)
{
	int ret = 0;
	u32 val;
	struct rk628_hdmirx *hdmirx;

	if (!rk628->hdmirx) {
		ret = rk628_hdmirx_init(rk628);
		if (ret < 0 || !rk628->hdmirx)
			return HDMIRX_PLUGOUT;
	}
	hdmirx = rk628->hdmirx;

	if (tx_5v_power_present(rk628)) {
		ret |= HDMIRX_PLUGIN;
		if (!hdmirx->plugin)
			ret |= HDMIRX_CHANGED;
		if (rk628_hdmirx_status_change(rk628))
			ret |= HDMIRX_CHANGED;

		if (hdmirx->phy_lock) {
			rk628_i2c_read(rk628, HDMI_RX_SCDC_REGS1, &val);
			if ((val & 0xfff) != 0xf00)
				hdmirx->phy_lock = false;
		}

		if (!hdmirx->phy_lock)
			ret |= HDMIRX_NOLOCK;
		hdmirx->plugin = true;
	} else {
		ret |= HDMIRX_PLUGOUT;
		if (hdmirx->plugin)
			ret |= HDMIRX_CHANGED;
		hdmirx->plugin = false;
	}

	return ret;
}
