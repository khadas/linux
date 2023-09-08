// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/major.h>
#include <linux/platform_device.h>
#include <linux/pinctrl/devinfo.h>
#include <linux/pinctrl/consumer.h>
#include <linux/amlogic/clk_measure.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <linux/ctype.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>
#include <linux/input.h>
#include <linux/irq.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/reboot.h>
#include <linux/i2c.h>
#include <linux/miscdevice.h>
//#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/media/vout/vinfo.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#if IS_ENABLED(CONFIG_AMLOGIC_SND_SOC)
#include <linux/amlogic/media/sound/aout_notify.h>
#endif
#include <linux/amlogic/media/vout/hdmi_tx21/hdmi_info_global.h>
#include <linux/amlogic/media/vout/hdmi_tx21/hdmi_tx_ddc.h>
#include <linux/amlogic/media/vout/hdmi_tx21/hdmi_tx_module.h>
#include <linux/amlogic/media/vout/hdmi_tx21/hdmi_config.h>
#include <linux/amlogic/media/vout/hdmi_tx_ext.h>
#include <linux/amlogic/media/vrr/vrr.h>
#include "hdmi_tx_ext.h"
#include "hdmi_tx.h"

#include <linux/amlogic/gki_module.h>
#include <linux/component.h>
#include <uapi/drm/drm_mode.h>
#include <drm/amlogic/meson_drm_bind.h>
#include <../../vin/tvin/tvin_global.h>
#include <../../vin/tvin/hdmirx/hdmi_rx_repeater.h>
#include "hw/common.h"

#define HDMI_TX_COUNT 32
#define HDMI_TX_POOL_NUM  6
#define HDMI_TX_RESOURCE_NUM 4
#define HDMI_TX_PWR_CTRL_NUM	6

static u8 hdmi_allm_passthough_en;

unsigned int rx_hdcp2_ver;
//static unsigned int hdcp_ctl_lvl;

#define TEE_HDCP_IOC_START _IOW('P', 0, int)

static struct class *hdmitx_class;
static int set_disp_mode_auto(void);
static void hdmitx_get_edid(struct hdmitx_dev *hdev);
static void hdmitx_set_drm_pkt(struct master_display_info_s *data);
static void hdmitx_set_sbtm_pkt(struct vtem_sbtm_st *data);
static void hdmitx_set_vsif_pkt(enum eotf_type type, enum mode_type
	tunnel_mode, struct dv_vsif_para *data, bool signal_sdr);
static void hdmitx_set_hdr10plus_pkt(u32 flag,
				     struct hdr10plus_para *data);
static void hdmitx_set_cuva_hdr_vsif(struct cuva_hdr_vsif_para *data);
static void hdmitx_set_cuva_hdr_vs_emds(struct cuva_hdr_vs_emds_para *data);
static void hdmitx_set_emp_pkt(u8 *data, u32 type, u32 size);

static int check_fbc_special(u8 *edid_dat);
static void clear_rx_vinfo(struct hdmitx_dev *hdev);
static void edidinfo_attach_to_vinfo(struct hdmitx_dev *hdev);
static void edidinfo_detach_to_vinfo(struct hdmitx_dev *hdev);
static void update_current_para(struct hdmitx_dev *hdev);
static void hdmi_tx_enable_ll_mode(bool enable);
static int meson_hdmitx_bind(struct device *dev,
			      struct device *master, void *data);
static void meson_hdmitx_unbind(struct device *dev,
				 struct device *master, void *data);
static void tee_comm_dev_reg(struct hdmitx_dev *hdev);
static void tee_comm_dev_unreg(struct hdmitx_dev *hdev);
static bool is_cur_tmds_div40(struct hdmitx_dev *hdev);
static void hdmitx_resend_div40(struct hdmitx_dev *hdev);
static int hdmitx_check_vic(int vic);
static unsigned int hdmitx_get_frame_duration(void);
static void hdmitx_set_frlrate_none(struct hdmitx_dev *hdev);

/*
 * Normally, after the HPD in or late resume, there will reading EDID, and
 * notify application to select a hdmi mode output. But during the mode
 * setting moment, there may be HPD out. It will clear the edid data, ..., etc.
 * To avoid such case, here adds the hdmimode_mutex to let the HPD in, HPD out
 * handler and mode setting sequentially.
 */
/* static DEFINE_MUTEX(hdmimode_mutex); */

/*
 * When systemcontrol and the upper layer call the valid_mode node
 * at the same time, it may cause concurrency errors.
 * Put the judgment of valid_mode in store_valid_mode
 */
static DEFINE_MUTEX(valid_mode_mutex);

#ifdef CONFIG_AMLOGIC_VOUT_SERVE
static struct vinfo_s *hdmitx_get_current_vinfo(void *data);
#else
static struct vinfo_s *hdmitx_get_current_vinfo(void *data)
{
	return NULL;
}
#endif

#ifdef CONFIG_OF
static struct amhdmitx_data_s amhdmitx_data_t7 = {
	.chip_type = MESON_CPU_ID_T7,
	.chip_name = "t7",
};

static struct amhdmitx_data_s amhdmitx_data_s5 = {
	.chip_type = MESON_CPU_ID_S5,
	.chip_name = "s5",
};

static const struct of_device_id meson_amhdmitx_of_match[] = {
	{
		.compatible	 = "amlogic, amhdmitx-t7",
		.data = &amhdmitx_data_t7,
	},
	{
		.compatible	 = "amlogic, amhdmitx-s5",
		.data = &amhdmitx_data_s5,
	},
	{},
};
#else
#define meson_amhdmitx_dt_match NULL
#endif

/*drm component bind*/
static const struct component_ops meson_hdmitx_bind_ops = {
	.bind	= meson_hdmitx_bind,
	.unbind	= meson_hdmitx_unbind,
};

static DEFINE_MUTEX(setclk_mutex);
static DEFINE_MUTEX(getedid_mutex);

static struct hdmitx_dev hdmitx21_device = {
	.frac_rate_policy = 1,
};

struct hdmitx_dev *get_hdmitx21_device(void)
{
	return &hdmitx21_device;
}
EXPORT_SYMBOL(get_hdmitx21_device);

static int get_hdmitx_hdcp_ctl_lvl_to_drm(void)
{
	pr_info("%s hdmitx21_%d\n", __func__, hdmitx21_device.hdcp_ctl_lvl);
	return hdmitx21_device.hdcp_ctl_lvl;
}

int get_hdmitx21_init(void)
{
	return hdmitx21_device.hdmi_init;
}

struct vsdb_phyaddr *get_hdmitx21_phy_addr(void)
{
	return &hdmitx21_device.hdmi_info.vsdb_phy_addr;
}

/* dv_dummy always be as 0, ext_dvinfo will be used for external module */
static const struct dv_info dv_dummy;
static struct dv_info ext_dvinfo;
static int log21_level;
static bool hdmitx_edid_done;
/* for SONY-KD-55A8F TV, need to mute more frames
 * when switch DV(LL)->HLG
 */
static int hdr_mute_frame = 20;

static struct vout_device_s hdmitx_vdev = {
	.dv_info = &ext_dvinfo,
	.fresh_tx_hdr_pkt = hdmitx_set_drm_pkt,
	.fresh_tx_sbtm_pkt = hdmitx_set_sbtm_pkt,
	.fresh_tx_vsif_pkt = hdmitx_set_vsif_pkt,
	.fresh_tx_hdr10plus_pkt = hdmitx_set_hdr10plus_pkt,
	.fresh_tx_cuva_hdr_vsif = hdmitx_set_cuva_hdr_vsif,
	.fresh_tx_cuva_hdr_vs_emds = hdmitx_set_cuva_hdr_vs_emds,
	.fresh_tx_emp_pkt = hdmitx_set_emp_pkt,
	.get_attr = get21_attr,
	.setup_attr = setup21_attr,
	/* .video_mute = hdmitx21_video_mute_op, */
};

static struct hdmitx_uevent hdmi_events[] = {
	{
		.type = HDMITX_HPD_EVENT,
		.env = "hdmitx_hpd=",
	},
	{
		.type = HDMITX_HDCP_EVENT,
		.env = "hdmitx_hdcp=",
	},
	{
		.type = HDMITX_AUDIO_EVENT,
		.env = "hdmitx_audio=",
	},
	{
		.type = HDMITX_HDCPPWR_EVENT,
		.env = "hdmitx_hdcppwr=",
	},
	{
		.type = HDMITX_HDR_EVENT,
		.env = "hdmitx_hdr=",
	},
	{
		.type = HDMITX_RXSENSE_EVENT,
		.env = "hdmitx_rxsense=",
	},
	{
		.type = HDMITX_CEDST_EVENT,
		.env = "hdmitx_cedst=",
	},
	{ /* end of hdmi_events[] */
		.type = HDMITX_NONE_EVENT,
	},
};

/* indicate plugout before systemcontrol boot  */
static bool plugout_mute_flg;
static char hdmichecksum[11] = {
	'i', 'n', 'v', 'a', 'l', 'i', 'd', 'c', 'r', 'c', '\0'
};

static char invalidchecksum[11] = {
	'i', 'n', 'v', 'a', 'l', 'i', 'd', 'c', 'r', 'c', '\0'
};

static char emptychecksum[11] = {0};

int hdmitx21_set_uevent_state(enum hdmitx_event type, int state)
{
	int ret = -1;

	struct hdmitx_uevent *event;

	for (event = hdmi_events; event->type != HDMITX_NONE_EVENT; event++) {
		if (type == event->type)
			break;
	}
	if (event->type == HDMITX_NONE_EVENT)
		return ret;
	event->state = state;

	if (log21_level == 0xfe)
		pr_info("[%s] event_type: %s%d\n", __func__, event->env, state);
	return 0;
}

static u32 is_passthrough_switch;
int hdmitx21_set_uevent(enum hdmitx_event type, int val)
{
	char env[MAX_UEVENT_LEN];
	struct hdmitx_uevent *event = hdmi_events;
	struct hdmitx_dev *hdev = get_hdmitx21_device();
	char *envp[2];
	int ret = -1;

	for (event = hdmi_events; event->type != HDMITX_NONE_EVENT; event++) {
		if (type == event->type)
			break;
	}
	if (event->type == HDMITX_NONE_EVENT)
		return ret;
	if (event->state == val)
		return ret;

	event->state = val;
	memset(env, 0, sizeof(env));
	envp[0] = env;
	envp[1] = NULL;
	snprintf(env, MAX_UEVENT_LEN, "%s%d", event->env, val);

	ret = kobject_uevent_env(&hdev->hdtx_dev->kobj, KOBJ_CHANGE, envp);
	if (log21_level == 0xfe)
		pr_info("%s[%d] %s %d\n", __func__, __LINE__, env, ret);
	return ret;
}

/* There are 3 callback functions for front HDR/DV/HDR10+ modules to notify
 * hdmi drivers to send out related HDMI infoframe
 * hdmitx_set_drm_pkt() is for HDR 2084 SMPTE, HLG, etc.
 * hdmitx_set_vsif_pkt() is for DV
 * hdmitx_set_hdr10plus_pkt is for HDR10+
 * Front modules may call the 2nd, and next call the 1st, and the realted flags
 * are remained the same. So, add hdr_status_pos and place it in the above 3
 * functions to record the position.
 */
static int hdr_status_pos;

static inline void hdmitx_notify_hpd(int hpd, void *p)
{
	if (hpd)
		hdmitx21_event_notify(HDMITX_PLUG, p);
	else
		hdmitx21_event_notify(HDMITX_UNPLUG, NULL);
}

#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
#include <linux/amlogic/pm.h>
static void hdmitx_early_suspend(struct early_suspend *h)
{
	struct hdmitx_dev *hdev = (struct hdmitx_dev *)h->param;
	unsigned int mute_us =
		hdev->debug_param.avmute_frame * hdmitx_get_frame_duration();

	if (hdev->aon_output) {
		pr_info("%s return, HDMI signal enabled\n", __func__);
		return;
	}

	/* Here remove the hpd_lock. Suppose at beginning, the hdmi cable is
	 * unconnected, and system goes to suspend, during this time, cable
	 * plugin. In this time, there will be no CEC physical address update
	 * and must wait the resume.
	 */
	mutex_lock(&hdev->hdmimode_mutex);
	/* under suspend, driver should not respond to mode setting,
	 * as it may cause logic abnormal, most importantly,
	 * it will enable hdcp and occupy DDC channel with high
	 * priority, though there's protection in system control,
	 * driver still need protection in case of old android version
	 */
	hdev->suspend_flag = true;
	rx_hdcp2_ver = 0;
	hdev->dw_hdcp22_cap = false;
	hdev->ready = 0;
	hdmitx_vrr_disable();
	hdev->hwop.cntlmisc(hdev, MISC_SUSFLAG, 1);
	usleep_range(10000, 10010);
	hdev->hwop.cntlmisc(hdev, MISC_AVMUTE_OP, SET_AVMUTE);
	if (hdev->debug_param.avmute_frame > 0)
		msleep(mute_us / 1000);
	else
		msleep(100);
	pr_info("HDMITX: Early Suspend\n");
	frl_tx_stop(hdev);
	fifo_flow_enable_intrs(0);
	hdev->hwop.cntl(hdev, HDMITX_EARLY_SUSPEND_RESUME_CNTL,
		HDMITX_EARLY_SUSPEND);

#ifdef CONFIG_AMLOGIC_DSC
	aml_dsc_enable(false);
	hdmitx_dsc_cvtem_pkt_disable();
#endif
	hdmitx21_disable_hdcp(hdev);
	hdmitx21_rst_stream_type(hdev->am_hdcp);
	hdmitx_set_frlrate_none(hdev);
	hdev->cur_VIC = HDMI_0_UNKNOWN;
	hdev->output_blank_flag = 0;
	hdmitx_set_vsif_pkt(0, 0, NULL, true);
	hdmitx_set_hdr10plus_pkt(0, NULL);
	clear_rx_vinfo(hdev);
	hdmitx21_edid_clear(hdev);
	hdmitx21_edid_ram_buffer_clear(hdev);
	hdmitx_edid_done = false;
	edidinfo_detach_to_vinfo(hdev);
	/* clear audio/video mute flag of stream type */
	hdmitx21_video_mute_op(1, VIDEO_MUTE_PATH_2);
	hdmitx21_audio_mute_op(1, AUDIO_MUTE_PATH_3);
	hdmitx21_set_uevent(HDMITX_HDCPPWR_EVENT, HDMI_SUSPEND);
	hdmitx21_set_uevent(HDMITX_AUDIO_EVENT, 0);
	hdev->hwop.cntlconfig(hdev, CONF_CLR_AVI_PACKET, 0);
	hdev->hwop.cntlconfig(hdev, CONF_CLR_VSDB_PACKET, 0);
	mutex_unlock(&hdev->hdmimode_mutex);
}

static int hdmitx_is_hdmi_vmode(char *mode_name)
{
	enum hdmi_vic vic = hdmitx21_edid_vic_tab_map_vic(mode_name);

	if (vic == HDMI_0_UNKNOWN)
		return 0;

	return 1;
}

static void hdmitx_late_resume(struct early_suspend *h)
{
	const struct vinfo_s *info = hdmitx_get_current_vinfo(NULL);
	struct hdmitx_dev *hdev = (struct hdmitx_dev *)h->param;

	if (hdev->aon_output) {
		pr_info("%s return, HDMI signal already enabled\n", __func__);
		return;
	}

	mutex_lock(&hdev->hdmimode_mutex);
	if (info && info->name && (hdmitx_is_hdmi_vmode(info->name) == 1))
		hdev->hwop.cntlmisc(hdev, MISC_HPLL_FAKE, 0);

	hdev->hpd_lock = 0;

	/* update status for hpd and switch/state */
	hdev->hpd_state = !!(hdev->hwop.cntlmisc(hdev, MISC_HPD_GPI_ST, 0));
	if (hdev->hpd_state)
		hdev->already_used = 1;

	pr_info("hdmitx hpd state: %d\n", hdev->hpd_state);

	/* force to get EDID after resume */
	if (hdev->hpd_state) {
		/*add i2c soft reset before read EDID */
		hdev->hwop.cntlddc(hdev, DDC_GLITCH_FILTER_RESET, 0);
		hdev->hwop.cntlmisc(hdev, MISC_I2C_RESET, 0);
		hdmitx_get_edid(hdev);
		hdmitx_edid_done = true;
	}
	if (hdev->tv_usage == 0)
		hdmitx_notify_hpd(hdev->hpd_state,
			  hdev->edid_parsing ?
			  hdev->edid_ptr : NULL);

	hdev->hwop.cntlconfig(hdev, CONF_AUDIO_MUTE_OP, AUDIO_MUTE);
	/* set_disp_mode_auto(); */
	/* force revert state to trigger uevent send */
	if (hdev->hpd_state) {
		hdmitx21_set_uevent_state(HDMITX_HPD_EVENT, 0);
		hdmitx21_set_uevent_state(HDMITX_AUDIO_EVENT, 0);
	} else {
		hdmitx21_set_uevent_state(HDMITX_HPD_EVENT, 1);
		hdmitx21_set_uevent_state(HDMITX_AUDIO_EVENT, 1);
	}

	hdev->suspend_flag = false;
	hdmitx21_set_uevent(HDMITX_HPD_EVENT, hdev->hpd_state);
	hdmitx21_set_uevent(HDMITX_HDCPPWR_EVENT, HDMI_WAKEUP);
	hdmitx21_set_uevent(HDMITX_AUDIO_EVENT, hdev->hpd_state);
	pr_info("%s: late resume module %d\n", DEVICE_NAME, __LINE__);
	hdev->hwop.cntl(hdev, HDMITX_EARLY_SUSPEND_RESUME_CNTL,
		HDMITX_LATE_RESUME);
	hdev->hwop.cntlmisc(hdev, MISC_SUSFLAG, 0);
	pr_info("HDMITX: Late Resume\n");

	/*notify to drm hdmi*/
	if (!hdev->suspend_flag && hdmitx21_device.drm_hpd_cb.callback)
		hdmitx21_device.drm_hpd_cb.callback(hdmitx21_device.drm_hpd_cb.data);
	mutex_unlock(&hdev->hdmimode_mutex);
}

/* Set avmute_set signal to HDMIRX */
static int hdmitx_reboot_notifier(struct notifier_block *nb,
				  unsigned long action, void *data)
{
	struct hdmitx_dev *hdev = container_of(nb, struct hdmitx_dev, nb);
	unsigned int mute_us =
		hdev->debug_param.avmute_frame * hdmitx_get_frame_duration();

	hdev->ready = 0;
	hdmitx_vrr_disable();
	hdev->hwop.cntlmisc(hdev, MISC_AVMUTE_OP, SET_AVMUTE);
	if (hdev->debug_param.avmute_frame > 0)
		msleep(mute_us / 1000);
	else
		msleep(100);
	frl_tx_stop(hdev);
#ifdef CONFIG_AMLOGIC_DSC
	aml_dsc_enable(false);
	hdmitx_dsc_cvtem_pkt_disable();
#endif
	if (hdev->rxsense_policy)
		cancel_delayed_work(&hdev->work_rxsense);
	if (hdev->cedst_policy)
		cancel_delayed_work(&hdev->work_cedst);
	hdmitx21_disable_hdcp(hdev);
	hdmitx21_rst_stream_type(hdev->am_hdcp);
	hdmitx_set_frlrate_none(hdev);
	hdev->hwop.cntlmisc(hdev, MISC_TMDS_PHY_OP, TMDS_PHY_DISABLE);
	hdev->hwop.cntl(hdev, HDMITX_EARLY_SUSPEND_RESUME_CNTL,
		HDMITX_EARLY_SUSPEND);

	return NOTIFY_OK;
}

static struct early_suspend hdmitx_early_suspend_handler = {
	.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN - 10,
	.suspend = hdmitx_early_suspend,
	.resume = hdmitx_late_resume,
	.param = &hdmitx21_device,
};
#endif

#define INIT_FLAG_VDACOFF		0x1
	/* unplug powerdown */
#define INIT_FLAG_POWERDOWN	 0x2

#define INIT_FLAG_NOT_LOAD 0x80

static u8 init_flag;
#undef DISABLE_AUDIO

static void restore_mute(void)
{
	struct hdmitx_dev *hdev = get_hdmitx21_device();
	atomic_t kref_video_mute = hdev->kref_video_mute;
	atomic_t kref_audio_mute = hdev->kref_audio_mute;

	if (!(atomic_sub_and_test(0, &kref_video_mute))) {
		pr_info("%s: hdmitx21_video_mute_op(0, 0) call\n", __func__);
		hdmitx21_video_mute_op(0, 0);
	}
	if (!(atomic_sub_and_test(0, &kref_audio_mute))) {
		pr_info("%s: hdmitx21_audio_mute_op(0,0) call\n", __func__);
		hdmitx21_audio_mute_op(0, 0);
	}
}

int get21_cur_vout_index(void)
/*
 * return value: 1, vout; 2, vout2;
 */
{
	int vout_index = 1;
	return vout_index;
}

static  int  set_disp_mode(const char *mode)
{
	int ret =  -1;
	enum hdmi_vic vic;
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	vic = hdmitx21_edid_get_VIC(hdev, mode, 1);
	if (strncmp(mode, "2160p30hz", strlen("2160p30hz")) == 0)
		vic = HDMI_95_3840x2160p30_16x9;
	else if (strncmp(mode, "2160p25hz", strlen("2160p25hz")) == 0)
		vic = HDMI_94_3840x2160p25_16x9;
	else if (strncmp(mode, "2160p24hz", strlen("2160p24hz")) == 0)
		vic = HDMI_93_3840x2160p24_16x9;
	else if (strncmp(mode, "smpte24hz", strlen("smpte24hz")) == 0)
		vic = HDMI_98_4096x2160p24_256x135;
	else
		;/* nothing */

	if (strncmp(mode, "1080p60hz", strlen("1080p60hz")) == 0)
		vic = HDMI_16_1920x1080p60_16x9;
	if (strncmp(mode, "1080p50hz", strlen("1080p50hz")) == 0)
		vic = HDMI_31_1920x1080p50_16x9;

	if (vic != HDMI_0_UNKNOWN) {
		hdev->mux_hpd_if_pin_high_flag = 1;
		if (hdev->vic_count == 0) {
			if (hdev->unplug_powerdown)
				return 0;
		}
	}

	hdev->cur_VIC = HDMI_0_UNKNOWN;
	ret = hdmitx21_set_display(hdev, vic);
	if (ret >= 0) {
		hdev->hwop.cntl(hdev, HDMITX_AVMUTE_CNTL, AVMUTE_CLEAR);
		hdev->cur_VIC = vic;
		hdev->audio_param_update_flag = 1;
		restore_mute();
	}

	if (hdev->cur_VIC == HDMI_0_UNKNOWN) {
		if (hdev->hpdmode == 2) {
			/* edid will be read again when hpd is muxed and
			 * it is high
			 */
			hdmitx21_edid_clear(hdev);
			hdev->mux_hpd_if_pin_high_flag = 0;
		}
		if (hdev->hwop.cntl) {
			fifo_flow_enable_intrs(0);
			hdev->hwop.cntl(hdev,
				HDMITX_HWCMD_TURNOFF_HDMIHW,
				(hdev->hpdmode == 2) ? 1 : 0);
		}
	}
	return ret;
}

static void hdmitx_set_frlrate_none(struct hdmitx_dev *hdev)
{
	u8 data;

	/* such as T7 unsupport FRL, skip frl flow */
	if (hdev->data && hdev->data->chip_type != MESON_CPU_ID_S5)
		return;

	if (hdev->rxcap.max_frl_rate > FRL_NONE &&
		hdev->rxcap.scdc_present == 1 &&
		hdev->hwop.cntlmisc(hdev, MISC_GET_FRL_MODE, 0) > FRL_NONE) {
		scdc_tx_frl_cfg1_set(0);
		data = scdc_tx_update_flags_get();
		if (data & FLT_UPDATE)
			scdc_tx_update_flags_set(FLT_UPDATE);
	}
}

static void hdmitx_pre_display_init(struct hdmitx_dev *hdev)
{
	u8 update_flags = 0;

	if (hdev->rxcap.max_frl_rate > FRL_NONE &&
		hdev->rxcap.scdc_present == 1 &&
		/* hdev->hwop.cntlmisc(hdev, MISC_GET_FRL_MODE, 0) && */
		hdev->frl_rate == FRL_NONE) {
		/* refer to LTS:L Source in FRL link training procedure:
		 * for FRL->legacy TMDS mode(LTS:4->LTS:L),
		 * or source init TMDS mode(LTS:1->LTS:L)
		 * 1. IF (SCDC_Present = 1)
		 * Source shall clear (=0) FRL_Rate indicating TMDS.
		 * IF FLT_update is currently set, Source shall clear
		 * FLT_update by writing "1".
		 * 2.Source shall start legacy TMDS operation when its
		 * content is ready.
		 */
		scdc_tx_frl_cfg1_set(0);
		update_flags = scdc_tx_update_flags_get();
		if ((update_flags & FLT_UPDATE) != 0)
			scdc_tx_update_flags_set(update_flags);
		hdev->hwop.cntlmisc(hdev, MISC_TMDS_PHY_OP, TMDS_PHY_DISABLE);
	} else if (hdev->rxcap.max_frl_rate > FRL_NONE &&
		hdev->rxcap.scdc_present == 1 &&
		hdev->frl_rate > FRL_NONE &&
		hdev->frl_rate < FRL_RATE_MAX &&
		hdev->hwop.cntlmisc(hdev, MISC_GET_FRL_MODE, 0) == FRL_NONE) {
		/* refer to LTS:L Source in FRL link training procedure:
		 * for case(LTS:L->LTS:2) switch from TMDS mode to FRL mode
		 *
		 * IF (Max_FRL_Rate > 0) AND (SCDC_Present = 1) AND (SCDC Sink Version != 0)
		 * IF Source chooses to operate in FRL mode
		 * Source should use AV mute
		 * Source shall stop TMDS transmission
		 * Source shall EXIT to LTS:2
		 * END IF
		 * END IF
		 */
		/* AV mute is set by system earlier */
		hdev->hwop.cntlmisc(hdev, MISC_TMDS_PHY_OP, TMDS_PHY_DISABLE);
	} else if (hdev->frl_rate == FRL_NONE &&
		hdev->hwop.cntlmisc(hdev, MISC_GET_FRL_MODE, 0) == FRL_NONE) {
		/* for cases switch between TMDS modes
		 * per hdmi2.1 spec chapter 6.1.3.2, when source change
		 * tmds_bit_clk_ratio, should follow the steps:
		 * 1.disable tmds clk/data
		 * 2.change tmds_bit_clk_ratio 0 <-> 1
		 * 3.resume tmds clk/data transmission in 1~100 ms
		 */
		hdev->hwop.cntlmisc(hdev, MISC_TMDS_PHY_OP, TMDS_PHY_DISABLE);
	}
	/* for cases switch between FRL modes: todo
	 * may refer to LTS:P Source in FRL link training procedure
	 * LTS:P->LTS:2
	 * IF Source initiates request for retraining
	 * Source should use AV mute if video is active
	 * Source shall stop FRL transmission including Gap-character-only transmission
	 * Source shall EXIT to LTS:2
	 */

	/* clear vsif/avi */
}
static void hdmi_physical_size_update(struct hdmitx_dev *hdev)
{
	u32 width, height;
	struct vinfo_s *info = NULL;

	info = hdmitx_get_current_vinfo(NULL);
	if (!info || !info->name) {
		pr_info("cann't get valid mode\n");
		return;
	}

	if (info->mode == VMODE_HDMI) {
		width = hdev->rxcap.physical_width;
		height = hdev->rxcap.physical_height;
		if (width == 0 || height == 0) {
			info->screen_real_width = info->aspect_ratio_num;
			info->screen_real_height = info->aspect_ratio_den;
		} else {
			info->screen_real_width = width;
			info->screen_real_height = height;
		}
		pr_info("update physical size: %d %d\n",
			info->screen_real_width, info->screen_real_height);
	}
}

static void hdrinfo_to_vinfo(struct hdr_info *hdrinfo, struct hdmitx_dev *hdev)
{
	memcpy(hdrinfo, &hdev->rxcap.hdr_info, sizeof(struct hdr_info));
	hdrinfo->colorimetry_support = hdev->rxcap.colorimetry_data;
}

static void rxlatency_to_vinfo(struct vinfo_s *info, struct rx_cap *rx)
{
	if (!info || !rx)
		return;
	info->rx_latency.vLatency = rx->vLatency;
	info->rx_latency.aLatency = rx->aLatency;
	info->rx_latency.i_vLatency = rx->i_vLatency;
	info->rx_latency.i_aLatency = rx->i_aLatency;
}

static void edidinfo_attach_to_vinfo(struct hdmitx_dev *hdev)
{
	struct vinfo_s *info = NULL;

	/* get current vinfo */
	info = hdmitx_get_current_vinfo(NULL);
	if (!info || !info->name)
		return;

	if (strncmp(info->name, "480cvbs", 7) == 0 ||
	    strncmp(info->name, "576cvbs", 7) == 0 ||
	    strncmp(info->name, "null", 4) == 0)
		return;

	mutex_lock(&getedid_mutex);
	hdrinfo_to_vinfo(&info->hdr_info, hdev);
	memcpy(&ext_dvinfo, &hdev->rxcap.dv_info, sizeof(ext_dvinfo));
	if (hdev->para && !hdev->hdr_8bit_en && hdev->para->cd == COLORDEPTH_24B)
		memset(&info->hdr_info, 0, sizeof(struct hdr_info));
	rxlatency_to_vinfo(info, &hdev->rxcap);
	hdmitx_vdev.dv_info = &ext_dvinfo;
	mutex_unlock(&getedid_mutex);
}

static void edidinfo_detach_to_vinfo(struct hdmitx_dev *hdev)
{
	struct vinfo_s *info = NULL;

	/* get current vinfo */
	info = hdmitx_get_current_vinfo(NULL);
	if (!info || !info->name)
		return;

	edidinfo_attach_to_vinfo(hdev);
	memset(&info->hdr_info, 0, sizeof(struct hdr_info));
	hdmitx_vdev.dv_info = &dv_dummy;
}

static void hdmitx_up_hdcp_timeout_handler(struct work_struct *work)
{
	struct hdmitx_dev *hdev = container_of((struct delayed_work *)work,
		struct hdmitx_dev, work_up_hdcp_timeout);

	if (hdcp_need_control_by_upstream(hdev)) {
		pr_info("hdmitx: enable hdcp as wait upstream hdcp timeout\n");
		/* note: hdcp should only be started when hdmi signal ready */
		mutex_lock(&hdev->hdmimode_mutex);
		if (!hdev->ready || !hdev->hpd_state) {
			pr_info("hdmitx: signal ready: %d, hpd_state: %d, eixt hdcp\n",
				hdev->ready, hdev->hpd_state);
			mutex_unlock(&hdev->hdmimode_mutex);
			return;
		}
		hdev->hwop.cntlmisc(hdev, MISC_AVMUTE_OP, CLR_AVMUTE);
		hdmitx21_enable_hdcp(hdev);
		mutex_unlock(&hdev->hdmimode_mutex);
	} else {
		pr_info("wait upstream hdcp timeout, but now not in hdmirx channel\n");
	}
}

static void hdmitx_start_hdcp_handler(struct work_struct *work)
{
	struct hdmitx_dev *hdev = container_of((struct delayed_work *)work,
		struct hdmitx_dev, work_start_hdcp);
	unsigned long timeout_sec;

	if (hdcp_need_control_by_upstream(hdev)) {
		if (is_passthrough_switch) {
			pr_info("hdmitx: enable hdcp by passthrough switch mode\n");
			/* note: hdcp should only be started when hdmi signal ready */
			mutex_lock(&hdev->hdmimode_mutex);
			if (!hdev->ready || !hdev->hpd_state) {
				pr_info("hdmitx: signal ready: %d, hpd_state: %d, eixt hdcp\n",
					hdev->ready, hdev->hpd_state);
				is_passthrough_switch = 0;
				mutex_unlock(&hdev->hdmimode_mutex);
				return;
			}
			hdev->hwop.cntlmisc(hdev, MISC_AVMUTE_OP, CLR_AVMUTE);
			hdmitx21_enable_hdcp(hdev);
			mutex_unlock(&hdev->hdmimode_mutex);
		} else {
			/* for source->hdmirx->hdmitx->tv, plug on tv side */
			/* 1.for repeater CTS, only start hdcp by upstream side
			 * 2.however if upstream source side no signal output
			 * or never start hdcp auth with hdmirx(such as PC),
			 * then we add 5S timeout period, after 5S timeout,
			 * it means that no input source start hdcp auth with
			 * hdmirx, or "no signal" on hdmirx side, then hdmitx
			 * side will start hdcp auth itself.
			 * thus both 1/2 will be satisfied.
			 */
			pr_info("hdmitx: hdcp should started by upstream, wait...\n");
			/* timeout period: hdcp1.4 5S, hdcp2.2 2S */
			if (rx_hdcp2_ver)
				timeout_sec = 2;
			else
				timeout_sec = hdev->up_hdcp_timeout_sec;
			queue_delayed_work(hdev->hdmi_wq, &hdev->work_up_hdcp_timeout,
				timeout_sec * HZ);
		}
	} else {
		mutex_lock(&hdev->hdmimode_mutex);
		if (!hdev->ready || !hdev->hpd_state) {
			pr_info("hdmitx: signal ready: %d, hpd_state: %d, eixt hdcp2\n",
				hdev->ready, hdev->hpd_state);
			is_passthrough_switch = 0;
			mutex_unlock(&hdev->hdmimode_mutex);
			return;
		}
		hdev->hwop.cntlmisc(hdev, MISC_AVMUTE_OP, CLR_AVMUTE);
		hdmitx21_enable_hdcp(hdev);
		mutex_unlock(&hdev->hdmimode_mutex);
	}
	/* clear after start hdcp */
	is_passthrough_switch = 0;
}

static int check_vic_4x3_and_16x9(struct hdmitx_dev *hdev, enum hdmi_vic vic)
{
	if (vic == HDMI_3_720x480p60_16x9 ||
		vic == HDMI_7_720x480i60_16x9 ||
		vic == HDMI_18_720x576p50_16x9 ||
		vic == HDMI_22_720x576i50_16x9) {
		if (hdmitx_check_vic(vic))
			return vic;
		return vic - 1;
	} else if (vic == HDMI_2_720x480p60_4x3 ||
		vic == HDMI_6_720x480i60_4x3 ||
		vic == HDMI_17_720x576p50_4x3 ||
		vic == HDMI_21_720x576i50_4x3) {
		if (hdmitx_check_vic(vic))
			return vic;
		return vic + 1;
	}

	return vic;
}

static int set_disp_mode_auto(void)
{
	int ret =  -1;

	struct vinfo_s *info = NULL;
	struct hdmitx_dev *hdev = get_hdmitx21_device();
	struct hdmi_format_para *para = NULL;
	u8 mode[32];
	enum hdmi_vic vic = HDMI_0_UNKNOWN;
#ifdef CONFIG_AMLOGIC_DSC
	struct dsc_notifier_data_s dsc_notifier_data;
#endif
	enum frl_rate_enum source_test_frl_rate = FRL_NONE;

	mutex_lock(&hdev->hdmimode_mutex);

	if (hdev->hpd_state == 0) {
		pr_info("current hpd_state0, exit %s\n", __func__);
		mutex_unlock(&hdev->hdmimode_mutex);
		return -1;
	}
	if (hdev->suspend_flag) {
		pr_info("currently under suspend, exit %s\n", __func__);
		mutex_unlock(&hdev->hdmimode_mutex);
		return -1;
	}
	memset(mode, 0, sizeof(mode));
	hdev->ready = 0;

	/* get current vinfo */
	info = hdmitx_get_current_vinfo(NULL);
	if (!info || !info->name) {
		mutex_unlock(&hdev->hdmimode_mutex);
		return -1;
	}
	pr_info("hdmitx: get current mode: %s\n", info->name);
	hdmitx_vrr_disable();
	if (strncmp(info->name, "invalid", strlen("invalid")) == 0) {
		hdmitx21_disable_hdcp(hdev);
		mutex_unlock(&hdev->hdmimode_mutex);
		return -1;
	}
	/*update hdmi checksum to vout*/
	memcpy(info->hdmichecksum, hdev->rxcap.chksum, 10);

	hdmi_physical_size_update(hdev);

	strncpy(mode, info->name, sizeof(mode));
	mode[31] = '\0';
	if (strstr(mode, "fp")) {
		int i = 0;

		for (; mode[i]; i++) {
			if ((mode[i] == 'f') && (mode[i + 1] == 'p')) {
				/* skip "f", 1080fp60hz -> 1080p60hz */
				do {
					mode[i] = mode[i + 1];
					i++;
				} while (mode[i]);
				break;
			}
		}
	}

	para = hdmitx21_get_fmtpara(mode, hdev->fmt_attr);
	if (!para) {
		pr_info("%s[%d] %s %s\n", __func__, __LINE__, mode,
			hdev->fmt_attr);
		mutex_unlock(&hdev->hdmimode_mutex);
		return -1;
	}
	/* disable hdcp before set mode if hdcp enabled.
	 * normally hdcp is disabled before setting mode
	 * when disable phy, but for special case of bootup,
	 * if mode changed as it's different with uboot mode,
	 * hdcp is not stopped firstly, and may hdcp fail
	 */
	if (!hdcp_need_control_by_upstream(hdev))
		hdmitx21_disable_hdcp(hdev);
	pr_info("setting hdmi mode %s %s\n", mode, hdev->fmt_attr);
	pr_info("cd/cs/cr: %d/%d/%d\n", para->cd, para->cs, para->cr);
	hdev->para = para;
	vic = hdmitx21_edid_get_VIC(hdev, mode, 1);
	if (strncmp(info->name, "2160p30hz", strlen("2160p30hz")) == 0) {
		vic = HDMI_95_3840x2160p30_16x9;
	} else if (strncmp(info->name, "2160p25hz",
		strlen("2160p25hz")) == 0) {
		vic = HDMI_94_3840x2160p25_16x9;
	} else if (strncmp(info->name, "2160p24hz",
		strlen("2160p24hz")) == 0) {
		vic = HDMI_93_3840x2160p24_16x9;
	} else if (strncmp(info->name, "smpte24hz",
		strlen("smpte24hz")) == 0) {
		vic = HDMI_98_4096x2160p24_256x135;
	} else {
	/* nothing */
	}

	hdev->frl_rate = FRL_NONE;
	if (hdev->rxcap.max_frl_rate && hdev->data->chip_type == MESON_CPU_ID_S5) {
		u8 sink_ver = scdc_tx_sink_version_get();
		u8 test_cfg = 0;

		if (!sink_ver)
			pr_info("sink version %d\n", sink_ver);
		scdc_tx_source_version_set(1);
		test_cfg = scdc_tx_source_test_cfg_get();
		hdev->frl_rate = hdmitx21_select_frl_rate(hdev->dsc_en, vic,
			hdev->para->cs, hdev->para->cd);
		/* per 2.1 spec, if both are set, then treat as if both are cleared */
		if ((test_cfg & FRL_MAX) && (test_cfg & DSC_FRL_MAX)) {
			pr_info("warning: both FRL_MAX and DSC_FRL_MAX are set, ignore\n");
		} else if (test_cfg & FRL_MAX) {
			source_test_frl_rate = min(hdev->tx_max_frl_rate, hdev->rxcap.max_frl_rate);
			pr_info("CTS: choose frl_max %d\n", source_test_frl_rate);
		} else if (test_cfg & DSC_FRL_MAX) {
			source_test_frl_rate =
				min(hdev->tx_max_frl_rate, hdev->rxcap.dsc_max_frl_rate);
			pr_info("CTS: choose dsc_frl_max %d\n", source_test_frl_rate);
		}
		if (hdev->frl_rate > hdev->tx_max_frl_rate)
			pr_info("error: Current frl_rate %d is larger than tx_max_frl_rate %d\n",
				hdev->frl_rate, hdev->tx_max_frl_rate);
#ifdef CONFIG_AMLOGIC_DSC
		/* DSC specific, automatically enable dsc if necessary */
		if (strcmp(mode, "7680x4320p60hz") == 0) {
			if (strstr(hdev->fmt_attr, "444") ||
				strstr(hdev->fmt_attr, "rgb")) {
				hdev->dsc_en = 1;
				/* note: previously spec FRL_6G4L can't work */
				hdev->frl_rate = FRL_6G4L;
				pr_debug("%s automatically dsc enable\n", __func__);
			} else if (strstr(hdev->fmt_attr, "420,12bit") ||
				strstr(hdev->fmt_attr, "422")) {
				hdev->dsc_en = 1;
				/* note: previously spec FRL_6G3L can't work */
				hdev->frl_rate = FRL_6G3L;
				pr_debug("%s automatically dsc enable\n", __func__);
			} else if (hdev->dsc_policy == 1) {
				/* for 420,8/10bit */
				/* force mode for dsc test, may need to also set manual_frl_rate */
				hdev->dsc_en = 1;
				/* note: previously spec FRL_6G3L can't work */
				hdev->frl_rate = FRL_6G3L;
			} else {
				hdev->dsc_en = 0;
			}
			if (hdev->dsc_en)
				pr_debug("forced DSC rate %d\n", hdev->frl_rate);
		} else if (strcmp(mode, "7680x4320p50hz") == 0) {
			if (strstr(hdev->fmt_attr, "444") ||
				strstr(hdev->fmt_attr, "rgb")) {
				hdev->dsc_en = 1;
				hdev->frl_rate = FRL_6G4L;
				pr_debug("%s automatically dsc enable\n", __func__);
			} else if (strstr(hdev->fmt_attr, "420,12bit") ||
				strstr(hdev->fmt_attr, "422")) {
				hdev->dsc_en = 1;
				hdev->frl_rate = FRL_6G3L;
				pr_debug("%s automatically dsc enable\n", __func__);
			} else if (hdev->dsc_policy == 1) {
				/* for 420,8/10bit */
				/* force mode for dsc test, may need to also set manual_frl_rate */
				hdev->dsc_en = 1;
				hdev->frl_rate = FRL_6G3L;
			} else {
				hdev->dsc_en = 0;
			}
			if (hdev->dsc_en)
				pr_debug("spec recommended DSC frl rate: %d\n", hdev->frl_rate);
		} else if (strcmp(mode, "7680x4320p25hz") == 0 ||
			strcmp(mode, "7680x4320p24hz") == 0) {
			if (strstr(hdev->fmt_attr, "444,12bit") ||
				strstr(hdev->fmt_attr, "rgb,12bit")) {
				hdev->dsc_en = 1;
				hdev->frl_rate = FRL_6G3L;
				pr_debug("%s automatically dsc enable\n", __func__);
			} else if (hdev->dsc_policy == 1) {
				/* force mode for test, may need to also set manual_frl_rate */
				hdev->dsc_en = 1;
				/* for y444/rgb,8/10bit */
				if (strstr(hdev->fmt_attr, "444") ||
					strstr(hdev->fmt_attr, "rgb"))
					hdev->frl_rate = FRL_6G3L;
				else
					hdev->frl_rate = FRL_3G3L; //for 422/420
			} else {
				hdev->dsc_en = 0;
			}
			if (hdev->dsc_en)
				pr_debug("spec recommended DSC frl rate: %d\n", hdev->frl_rate);
		} else if (strcmp(mode, "7680x4320p30hz") == 0) {
			if (strstr(hdev->fmt_attr, "444,12bit") ||
				strstr(hdev->fmt_attr, "rgb,12bit")) {
				hdev->dsc_en = 1;
				hdev->frl_rate = FRL_6G3L;
				pr_debug("%s automatically dsc enable\n", __func__);
			} else if (hdev->dsc_policy == 1) {
				/* force mode for test, may need to also set manual_frl_rate */
				hdev->dsc_en = 1;
				/* for 444/rgb,8/10bit */
				if (strstr(hdev->fmt_attr, "444") ||
					strstr(hdev->fmt_attr, "rgb"))
					hdev->frl_rate = FRL_6G3L;
				else /* for 422/420, note: previously spec FRL_3G3L can't work */
					hdev->frl_rate = FRL_3G3L;
			} else {
				hdev->dsc_en = 0;
			}
			if (hdev->dsc_en)
				pr_debug("forced DSC frl rate: %d\n", hdev->frl_rate);
		} else if (strcmp(mode, "2160p60hz") == 0 ||
			strcmp(mode, "2160p50hz") == 0) {
			if (hdev->dsc_policy == 1) {
				/* force mode for test, may need to also set manual_frl_rate */
				hdev->dsc_en = 1;
				hdev->frl_rate = FRL_3G3L;
			} else {
				hdev->dsc_en = 0;
			}
			if (hdev->dsc_en)
				pr_debug("spec recommended DSC frl rate: %d\n", hdev->frl_rate);
		} else if (strcmp(mode, "3840x2160p120hz") == 0 ||
			strcmp(mode, "3840x2160p100hz") == 0) {
			/* need 12G4L under uncompressed format */
			if (strstr(hdev->fmt_attr, "444,12bit") ||
				strstr(hdev->fmt_attr, "rgb,12bit")) {
				hdev->dsc_en = 1;
				hdev->frl_rate = FRL_6G3L;
				pr_debug("%s automatically dsc enable\n", __func__);
			} else if (hdev->dsc_policy == 1) {
				/* force mode for test, may need to also set manual_frl_rate */
				hdev->dsc_en = 1;
				/* for 444/rgb,8/10bit */
				if (strstr(hdev->fmt_attr, "444") ||
					strstr(hdev->fmt_attr, "rgb"))
					hdev->frl_rate = FRL_6G3L;
				else /* for 422/420, note: previously spec FRL_3G3L can't work */
					hdev->frl_rate = FRL_3G3L;
			} else {
				hdev->dsc_en = 0;
			}
			if (hdev->dsc_en)
				pr_debug("spec recommended DSC frl rate: %d\n", hdev->frl_rate);
		} else {
			/* when switch mode to lower resolution, need to back to non-dsc mode */
			if (hdev->dsc_policy != 2)
				hdev->dsc_en = 0;
		}
#endif
	}

	/* if (strstr(mode, "i")) { */
		/* hdev->dsc_en = 0; */
		/* pr_info("error: Sources shall not enable DSC for interlaced Timings\n"); */
	/* } */

	/* source_test_frl_rate has the highest priority */
	if (source_test_frl_rate > FRL_NONE && source_test_frl_rate < FRL_RATE_MAX)
		hdev->frl_rate = source_test_frl_rate;

#ifdef CONFIG_AMLOGIC_DSC

	if (hdev->dsc_en && hdev->data->chip_type == MESON_CPU_ID_S5) {
		/* notify hdmitx format to dsc, and dsc module will
		 * calculate pps data and venc/pixel clock
		 */
		dsc_notifier_data.pic_width = hdev->para->timing.h_active;
		dsc_notifier_data.pic_height = hdev->para->timing.v_active;
		dsc_notifier_data.color_format = hdev->para->cs;
		/* note: for y422 need set bpc to 8 in pps,
		 * otherwise y422 iter in cts HFR1-85 will fail
		 */
		if (hdev->para->cs == HDMI_COLORSPACE_YUV422)
			dsc_notifier_data.bits_per_component = 8;
		else if (hdev->para->cd == COLORDEPTH_24B)
			dsc_notifier_data.bits_per_component = 8;
		else if (hdev->para->cd == COLORDEPTH_30B)
			dsc_notifier_data.bits_per_component = 10;
		else if (hdev->para->cd == COLORDEPTH_36B)
			dsc_notifier_data.bits_per_component = 12;
		else
			dsc_notifier_data.bits_per_component = 8;
		dsc_notifier_data.fps = hdev->para->timing.v_freq;
		ret = aml_set_dsc_input_param(&dsc_notifier_data);
		if (ret < 0) {
			pr_info("[%s] set dsc input param error\n", __func__);
		} else {
			hdmitx_get_dsc_data(&hdev->dsc_data);
			pr_info("dsc provide enc0_clk: %d, cts_hdmi_pixel_clk: %d\n",
				hdev->dsc_data.enc0_clk,
				hdev->dsc_data.cts_hdmi_tx_pixel_clk);
		}
	}
#endif
	/* if manual_frl_rate is true, set to force frl_rate */
	if (hdev->manual_frl_rate) {
		hdev->frl_rate = hdev->manual_frl_rate;
		pr_info("manually frl rate %d\n", hdev->frl_rate);
	}
	hdev->cur_VIC = HDMI_0_UNKNOWN;
/* if vic is HDMI_0_UNKNOWN, hdmitx21_set_display will disable HDMI */
	edidinfo_detach_to_vinfo(hdev);
	vic = check_vic_4x3_and_16x9(hdev, vic);

	hdmitx_pre_display_init(hdev);
	ret = hdmitx21_set_display(hdev, vic);

	if (ret >= 0) {
		hdev->hwop.cntl(hdev, HDMITX_AVMUTE_CNTL, AVMUTE_CLEAR);
		hdev->cur_VIC = vic;
		hdev->audio_param_update_flag = 1;
		restore_mute();
	}
	if (hdev->cur_VIC == HDMI_0_UNKNOWN) {
		if (hdev->hpdmode == 2) {
			/* edid will be read again when hpd is muxed
			 * and it is high
			 */
			hdmitx21_edid_clear(hdev);
			hdev->mux_hpd_if_pin_high_flag = 0;
		}
		/* If current display is NOT panel, needn't TURNOFF_HDMIHW */
		if (strncmp(mode, "panel", 5) == 0) {
			fifo_flow_enable_intrs(0);
			hdev->hwop.cntl(hdev, HDMITX_HWCMD_TURNOFF_HDMIHW,
				(hdev->hpdmode == 2) ? 1 : 0);
		}
	}
	hdmitx21_set_audio(hdev, &hdev->cur_audio_param);
	if (hdev->cedst_policy) {
		cancel_delayed_work(&hdev->work_cedst);
		queue_delayed_work(hdev->cedst_wq, &hdev->work_cedst, 0);
	}
	hdev->output_blank_flag = 1;
	edidinfo_attach_to_vinfo(hdev);
	/* backup values need to be updated to latest values */
	memcpy(hdev->backup_fmt_attr, hdev->fmt_attr, 16);
	hdev->backup_frac_rate_policy = hdev->frac_rate_policy;
	/* wait for TV detect signal stable,
	 * otherwise hdcp may easily auth fail
	 */
	hdev->ready = 1;
	if (hdev->not_restart_hdcp) {
		/* self clear */
		hdev->not_restart_hdcp = 0;
		pr_info("special mode switch, not start hdcp\n");
	} else {
		/* below is only for tmds mode, for FRL mode
		 * hdcp is started after training passed
		 */
		if (hdev->frl_rate == FRL_NONE) {
			if (get_hdcp2_lstore())
				hdev->dw_hdcp22_cap = is_rx_hdcp2ver();
			queue_delayed_work(hdev->hdmi_wq, &hdev->work_start_hdcp, HZ / 4);
		}
	}
	mutex_unlock(&hdev->hdmimode_mutex);
	return ret;
}

/*disp_mode attr*/
static ssize_t disp_mode_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	int pos = 0;
	int i = 0;
	struct hdmitx_dev *hdev = get_hdmitx21_device();
	struct hdmi_format_para *para = hdev->para;
	struct hdmi_timing *timing = &para->timing;
	struct vinfo_s *vinfo = &para->hdmitx_vinfo;

	if (!para)
		return pos;

	pos += snprintf(buf + pos, PAGE_SIZE, "cd/cs/cr: %d/%d/%d\n", para->cd,
		para->cs, para->cr);
	pos += snprintf(buf + pos, PAGE_SIZE, "scramble/tmds_clk_div40: %d/%d\n",
		para->scrambler_en, para->tmds_clk_div40);
	pos += snprintf(buf + pos, PAGE_SIZE, "tmds_clk: %d\n", para->tmds_clk);
	pos += snprintf(buf + pos, PAGE_SIZE, "vic: %d\n", timing->vic);
	pos += snprintf(buf + pos, PAGE_SIZE, "name: %s\n", timing->name);
	pos += snprintf(buf + pos, PAGE_SIZE, "enc_idx: %d\n", hdev->enc_idx);
	if (timing->sname)
		pos += snprintf(buf + pos, PAGE_SIZE, "sname: %s\n",
			timing->sname);
	pos += snprintf(buf + pos, PAGE_SIZE, "pi_mode: %c\n",
		timing->pi_mode ? 'P' : 'I');
	pos += snprintf(buf + pos, PAGE_SIZE, "h/v_freq: %d/%d\n",
		timing->h_freq, timing->v_freq);
	pos += snprintf(buf + pos, PAGE_SIZE, "pixel_freq: %d\n",
		timing->pixel_freq);
	pos += snprintf(buf + pos, PAGE_SIZE, "h_total: %d\n", timing->h_total);
	pos += snprintf(buf + pos, PAGE_SIZE, "h_blank: %d\n", timing->h_blank);
	pos += snprintf(buf + pos, PAGE_SIZE, "h_front: %d\n", timing->h_front);
	pos += snprintf(buf + pos, PAGE_SIZE, "h_sync: %d\n", timing->h_sync);
	pos += snprintf(buf + pos, PAGE_SIZE, "h_back: %d\n", timing->h_back);
	pos += snprintf(buf + pos, PAGE_SIZE, "h_active: %d\n", timing->h_active);
	pos += snprintf(buf + pos, PAGE_SIZE, "v_total: %d\n", timing->v_total);
	pos += snprintf(buf + pos, PAGE_SIZE, "v_blank: %d\n", timing->v_blank);
	pos += snprintf(buf + pos, PAGE_SIZE, "v_front: %d\n", timing->v_front);
	pos += snprintf(buf + pos, PAGE_SIZE, "v_sync: %d\n", timing->v_sync);
	pos += snprintf(buf + pos, PAGE_SIZE, "v_back: %d\n", timing->v_back);
	pos += snprintf(buf + pos, PAGE_SIZE, "v_active: %d\n", timing->v_active);
	pos += snprintf(buf + pos, PAGE_SIZE, "v_sync_ln: %d\n", timing->v_sync_ln);
	pos += snprintf(buf + pos, PAGE_SIZE, "h/v_pol: %d/%d\n", timing->h_pol, timing->v_pol);
	pos += snprintf(buf + pos, PAGE_SIZE, "h/v_pict: %d/%d\n", timing->h_pict, timing->v_pict);
	pos += snprintf(buf + pos, PAGE_SIZE, "h/v_pixel: %d/%d\n",
		timing->h_pixel, timing->v_pixel);
	pos += snprintf(buf + pos, PAGE_SIZE, "name: %s\n", vinfo->name);
	pos += snprintf(buf + pos, PAGE_SIZE, "mode: %d\n", vinfo->mode);
	pos += snprintf(buf + pos, PAGE_SIZE, "ext_name: %s\n", vinfo->ext_name);
	pos += snprintf(buf + pos, PAGE_SIZE, "frac: %d\n", vinfo->frac);
	pos += snprintf(buf + pos, PAGE_SIZE, "width/height: %d/%d\n", vinfo->width, vinfo->height);
	pos += snprintf(buf + pos, PAGE_SIZE, "field_height: %d\n", vinfo->field_height);
	pos += snprintf(buf + pos, PAGE_SIZE, "aspect_ratio_num/den: %d/%d\n",
		vinfo->aspect_ratio_num, vinfo->aspect_ratio_den);
	pos += snprintf(buf + pos, PAGE_SIZE, "screen_real_width/height: %d/%d\n",
		vinfo->screen_real_width, vinfo->screen_real_height);
	pos += snprintf(buf + pos, PAGE_SIZE, "sync_duration_num/den: %d/%d\n",
		vinfo->sync_duration_num, vinfo->sync_duration_den);
	pos += snprintf(buf + pos, PAGE_SIZE, "video_clk: %d\n", vinfo->video_clk);
	pos += snprintf(buf + pos, PAGE_SIZE, "h/vtotal: %d/%d\n", vinfo->htotal, vinfo->vtotal);
	pos += snprintf(buf + pos, PAGE_SIZE, "hdmichecksum:\n");
	for (i = 0; i < sizeof(vinfo->hdmichecksum); i++)
		pos += snprintf(buf + pos, PAGE_SIZE, "%02x", vinfo->hdmichecksum[i]);
	pos += snprintf(buf + pos, PAGE_SIZE, "\n");
	pos += snprintf(buf + pos, PAGE_SIZE, "info_3d: %d\n", vinfo->info_3d);
	pos += snprintf(buf + pos, PAGE_SIZE, "fr_adj_type: %d\n", vinfo->fr_adj_type);
	pos += snprintf(buf + pos, PAGE_SIZE, "viu_color_fmt: %d\n", vinfo->viu_color_fmt);
	pos += snprintf(buf + pos, PAGE_SIZE, "viu_mux: %d\n", vinfo->viu_mux);
	/* master_display_info / hdr_info / rx_latency */

	return pos;
}

static ssize_t disp_mode_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t count)
{
	set_disp_mode(buf);
	return count;
}

static ssize_t attr_show(struct device *dev,
			 struct device_attribute *attr, char *buf)
{
	int pos = 0;
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	if (!memcmp(hdev->fmt_attr, "default,", 7)) {
		memset(hdev->fmt_attr, 0,
		       sizeof(hdev->fmt_attr));
		hdmitx21_fmt_attr(hdev);
	}
	pos += snprintf(buf + pos, PAGE_SIZE, "%s\n\r", hdev->fmt_attr);
	return pos;
}

static ssize_t attr_store(struct device *dev,
		   struct device_attribute *attr,
		   const char *buf, size_t count)
{
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	strncpy(hdev->fmt_attr, buf, sizeof(hdev->fmt_attr));
	hdev->fmt_attr[15] = '\0';
	if (!memcmp(hdev->fmt_attr, "rgb", 3))
		hdev->para->cs = HDMI_COLORSPACE_RGB;
	else if (!memcmp(hdev->fmt_attr, "422", 3))
		hdev->para->cs = HDMI_COLORSPACE_YUV422;
	else if (!memcmp(hdev->fmt_attr, "420", 3))
		hdev->para->cs = HDMI_COLORSPACE_YUV420;
	else
		hdev->para->cs = HDMI_COLORSPACE_YUV444;
	return count;
}

/*aud_mode attr*/

void setup21_attr(const char *buf)
{
	char attr[16] = {0};
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	memcpy(attr, buf, sizeof(attr));
	memcpy(hdev->fmt_attr, attr, sizeof(hdev->fmt_attr));
}

void get21_attr(char attr[16])
{
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	memcpy(attr, hdev->fmt_attr, sizeof(hdev->fmt_attr));
}

/*edid attr*/
static ssize_t edid_show(struct device *dev,
			 struct device_attribute *attr,
			 char *buf)
{
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	return hdmitx21_edid_dump(hdev, buf, PAGE_SIZE);
}

static int dump_edid_data(u32 type, char *path)
{
	struct hdmitx_dev *hdev = get_hdmitx21_device();
	struct file *filp = NULL;
	loff_t pos = 0;
	char line[128] = {0};
	mm_segment_t old_fs = get_fs();
	u32 i = 0, j = 0, k = 0, size = 0, block_cnt = 0;
	u32 index = 0, tmp = 0;

	set_fs(KERNEL_DS);
	filp = filp_open(path, O_RDWR | O_CREAT, 0666);
	if (IS_ERR(filp)) {
		pr_info("[%s] failed to open/create file: |%s|\n",
			__func__, path);
		goto PROCESS_END;
	}

	block_cnt = hdev->EDID_buf[0x7e] + 1;
	if (hdev->EDID_buf[0x7e] != 0 &&
		hdev->EDID_buf[128 + 4] == 0xe2 &&
		hdev->EDID_buf[128 + 5] == 0x78)
		block_cnt = hdev->EDID_buf[128 + 6] + 1;
	if (type == 1) {
		/* dump as bin file*/
		size = vfs_write(filp, hdev->EDID_buf,
				 block_cnt * 128, &pos);
	} else if (type == 2) {
		/* dump as txt file*/

		for (i = 0; i < block_cnt; i++) {
			for (j = 0; j < 8; j++) {
				for (k = 0; k < 16; k++) {
					index = i * 128 + j * 16 + k;
					tmp = hdev->EDID_buf[index];
					snprintf((char *)&line[k * 6], 7,
						 "0x%02x, ",
						 tmp);
				}
				line[16 * 6 - 1] = '\n';
				line[16 * 6] = 0x0;
				pos = (i * 8 + j) * 16 * 6;
				size += vfs_write(filp, line, 16 * 6, &pos);
			}
		}
	}

	pr_info("[%s] write %d bytes to file %s\n", __func__, size, path);

	vfs_fsync(filp, 0);
	filp_close(filp, NULL);

PROCESS_END:
	set_fs(old_fs);
	return 0;
}

static int load_edid_data(u32 type, char *path)
{
	struct file *filp = NULL;
	loff_t pos = 0;
	mm_segment_t old_fs = get_fs();
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	struct kstat stat;
	u32 length = 0, max_len = EDID_MAX_BLOCK * 128;
	char *buf = NULL;

	set_fs(KERNEL_DS);

	filp = filp_open(path, O_RDONLY, 0444);
	if (IS_ERR(filp)) {
		pr_info("[%s] failed to open file: |%s|\n", __func__, path);
		goto PROCESS_END;
	}

	WARN_ON(vfs_stat(path, &stat));

	length = (stat.size > max_len) ? max_len : stat.size;

	buf = kmalloc(length, GFP_KERNEL);
	if (!buf)
		goto PROCESS_END;

	vfs_read(filp, buf, length, &pos);

	memcpy(hdev->EDID_buf, buf, length);

	kfree(buf);
	filp_close(filp, NULL);

	pr_info("[%s] %d bytes loaded from file %s\n", __func__, length, path);

	hdmitx21_edid_clear(hdev);
	hdmitx21_edid_parse(hdev);
	pr_info("[%s] new edid loaded!\n", __func__);

PROCESS_END:
	set_fs(old_fs);
	return 0;
}

static ssize_t edid_store(struct device *dev,
			  struct device_attribute *attr,
			  const char *buf, size_t count)
{
	u32 argn = 0;
	char *p = NULL, *para = NULL, *argv[8] = {NULL};
	u32 path_length = 0;
	u32 index = 0, tmp = 0;
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	p = kstrdup(buf, GFP_KERNEL);
	if (!p)
		return count;

	do {
		para = strsep(&p, " ");
		if (para) {
			argv[argn] = para;
			argn++;
			if (argn > 7)
				break;
		}
	} while (para);

	if (buf[0] == 'h') {
		int i;

		pr_info("EDID hash value:\n");
		for (i = 0; i < 20; i++)
			pr_info("%02x", hdev->EDID_hash[i]);
		pr_info("\n");
	}
	if (buf[0] == 'd') {
		int ii, jj;
		unsigned long block_idx;
		int ret;

		ret = kstrtoul(buf + 1, 16, &block_idx);
		if (block_idx < EDID_MAX_BLOCK) {
			for (ii = 0; ii < 8; ii++) {
				for (jj = 0; jj < 16; jj++) {
					index = block_idx * 128 + ii * 16 + jj;
					tmp = hdev->EDID_buf1[index];
					pr_info("%02x ", tmp);
				}
				pr_info("\n");
			}
		pr_info("\n");
	}
	}
	if (buf[0] == 'e') {
		int ii, jj;
		unsigned long block_idx;
		int ret;

		ret = kstrtoul(buf + 1, 16, &block_idx);
		if (block_idx < EDID_MAX_BLOCK) {
			for (ii = 0; ii < 8; ii++) {
				for (jj = 0; jj < 16; jj++) {
					index = block_idx * 128 + ii * 16 + jj;
					tmp = hdev->EDID_buf1[index];
					pr_info("%02x ", tmp);
				}
				pr_info("\n");
			}
			pr_info("\n");
		}
	}

	if (!strncmp(argv[0], "save", strlen("save"))) {
		u32 type = 0;

		if (argn != 3) {
			pr_info("[%s] cmd format: save bin/txt edid_file_path\n",
				__func__);
			goto PROCESS_END;
		}
		if (!strncmp(argv[1], "bin", strlen("bin")))
			type = 1;
		else if (!strncmp(argv[1], "txt", strlen("txt")))
			type = 2;

		if (type == 1 || type == 2) {
			/* clean '\n' from file path*/
			path_length = strlen(argv[2]);
			if (argv[2][path_length - 1] == '\n')
				argv[2][path_length - 1] = 0x0;

			dump_edid_data(type, argv[2]);
		}
	} else if (!strncmp(argv[0], "load", strlen("load"))) {
		if (argn != 2) {
			pr_info("[%s] cmd format: load edid_file_path\n",
				__func__);
			goto PROCESS_END;
		}

		/* clean '\n' from file path*/
		path_length = strlen(argv[1]);
		if (argv[1][path_length - 1] == '\n')
			argv[1][path_length - 1] = 0x0;
		load_edid_data(0, argv[1]);
	}

PROCESS_END:
	kfree(p);
	return count;
}

/* rawedid attr */
static ssize_t rawedid_show(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	int pos = 0;
	int i;
	struct hdmitx_dev *hdev = get_hdmitx21_device();
	int num;
	int block_no = 0;

	/* prevent null prt */
	if (!hdev->edid_ptr)
		hdev->edid_ptr = hdev->EDID_buf;
	block_no = hdev->edid_ptr[126];
	if (block_no == 1)
		if (hdev->edid_ptr[128 + 4] == 0xe2 && hdev->edid_ptr[128 + 5] == 0x78)
			block_no = hdev->edid_ptr[128 + 6];
	if (block_no < 8)
		num = (block_no + 1) * 0x80;
	else
		num = 8 * 128;

	for (i = 0; i < num; i++)
		pos += snprintf(buf + pos, PAGE_SIZE, "%02x",
				hdev->edid_ptr[i]);

	pos += snprintf(buf + pos, PAGE_SIZE, "\n");

	return pos;
}

/*
 * edid_parsing attr
 * If RX edid data are all correct, HEAD(00ffffffffffff00), checksum,
 * version, etc), then return "ok". Otherwise, "ng"
 * Actually, in some old televisions, EDID is stored in EEPROM.
 * some bits in EEPROM may reverse with time.
 * But it does not affect  edid_parsing.
 * Therefore, we consider the RX edid data are all correct, return "OK"
 */
static ssize_t edid_parsing_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	int pos = 0;
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	if (hdmitx21_edid_notify_ng(hdev->edid_ptr))
		pos += snprintf(buf + pos, PAGE_SIZE, "ng\n");
	else
		pos += snprintf(buf + pos, PAGE_SIZE, "ok\n");
	return pos;
}

/*
 * sink_type attr
 * sink, or repeater
 */
static ssize_t sink_type_show(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	int pos = 0;
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	if (!hdev->hpd_state) {
		pos += snprintf(buf + pos, PAGE_SIZE, "none\n");
		return pos;
	}

	if (hdev->hdmi_info.vsdb_phy_addr.b)
		pos += snprintf(buf + pos, PAGE_SIZE, "repeater\n");
	else
		pos += snprintf(buf + pos, PAGE_SIZE, "sink\n");

	return pos;
}

static DEFINE_MUTEX(aud_mute_mutex);
void hdmitx21_audio_mute_op(u32 flag, unsigned int path)
{
	static unsigned int aud_mute_path;
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	mutex_lock(&aud_mute_mutex);
	if (flag == 0)
		aud_mute_path |= path;
	else
		aud_mute_path &= ~path;
	hdev->tx_aud_cfg = !aud_mute_path;

	if (flag == 0) {
		pr_info("%s: AUD_MUTE path=0x%x\n", __func__, path);
		hdev->hwop.cntlconfig(hdev, CONF_AUDIO_MUTE_OP, AUDIO_MUTE);
	} else {
		/* unmute only if none of the paths are muted */
		if (aud_mute_path == 0) {
			pr_info("%s: AUD_UNMUTE path=0x%x\n", __func__, path);
			hdev->hwop.cntlconfig(hdev, CONF_AUDIO_MUTE_OP, AUDIO_UNMUTE);
		}
	}
	mutex_unlock(&aud_mute_mutex);
}

static DEFINE_MUTEX(vid_mute_mutex);
void hdmitx21_video_mute_op(u32 flag, unsigned int path)
{
	static unsigned int vid_mute_path;
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	mutex_lock(&vid_mute_mutex);
	if (flag == 0)
		vid_mute_path |= path;
	else
		vid_mute_path &= ~path;

	if (flag == 0) {
		pr_info("%s: VID_MUTE path=0x%x\n", __func__, path);
		hdev->hwop.cntlconfig(hdev, CONF_VIDEO_MUTE_OP, VIDEO_MUTE);
	} else {
		/* unmute only if none of the paths are muted */
		if (vid_mute_path == 0) {
			pr_info("%s: VID_UNMUTE path=0x%x\n", __func__, path);
			hdev->hwop.cntlconfig(hdev, CONF_VIDEO_MUTE_OP, VIDEO_UNMUTE);
		}
	}
	mutex_unlock(&vid_mute_mutex);
}

/*
 *  SDR/HDR uevent
 *  1: SDR to HDR
 * 0: HDR to SDR
 */
static void hdmitx_sdr_hdr_uevent(struct hdmitx_dev *hdev)
{
	if (hdev->hdmi_current_hdr_mode != 0) {
		/* SDR -> HDR*/
		hdmitx21_set_uevent(HDMITX_HDR_EVENT, 1);
	} else if (hdev->hdmi_current_hdr_mode == 0) {
		/* HDR -> SDR*/
		hdmitx21_set_uevent(HDMITX_HDR_EVENT, 0);
	}
}

static unsigned int hdmitx_get_frame_duration(void)
{
	unsigned int frame_duration;
	struct vinfo_s *vinfo = hdmitx_get_current_vinfo(NULL);

	if (!vinfo || !vinfo->sync_duration_num)
		return 0;

	frame_duration =
		1000000 * vinfo->sync_duration_den / vinfo->sync_duration_num;
	return frame_duration;
}

static int hdmitx_check_vic(int vic)
{
	struct hdmitx_dev *hdev = get_hdmitx21_device();
	int i;

	for (i = 0; i < hdev->rxcap.VIC_count && i < VIC_MAX_NUM; i++) {
		if (vic == hdev->rxcap.VIC[i])
			return 1;
	}

	return 0;
}

static int hdmitx_check_valid_aspect_ratio(enum hdmi_vic vic, int aspect_ratio)
{
	switch (vic) {
	case HDMI_2_720x480p60_4x3:
		if (hdmitx_check_vic(HDMI_3_720x480p60_16x9)) {
			if (aspect_ratio == AR_16X9)
				return 1;
			pr_info("same aspect_ratio = %d\n", aspect_ratio);
		} else {
			pr_info("TV not support dual aspect_ratio\n");
		}
		break;
	case HDMI_3_720x480p60_16x9:
		if (hdmitx_check_vic(HDMI_2_720x480p60_4x3)) {
			if (aspect_ratio == AR_4X3)
				return 1;
			pr_info("same aspect_ratio = %d\n", aspect_ratio);
		} else {
			pr_info("TV not support dual aspect_ratio\n");
		}
		break;
	case HDMI_17_720x576p50_4x3:
		if (hdmitx_check_vic(HDMI_18_720x576p50_16x9)) {
			if (aspect_ratio == AR_16X9)
				return 1;
			pr_info("same aspect_ratio = %d\n", aspect_ratio);
		} else {
			pr_info("TV not support dual aspect_ratio\n");
		}
		break;
	case HDMI_18_720x576p50_16x9:
		if (hdmitx_check_vic(HDMI_17_720x576p50_4x3)) {
			if (aspect_ratio == AR_4X3)
				return 1;
			pr_info("same aspect_ratio = %d\n", aspect_ratio);
		} else {
			pr_info("TV not support dual aspect_ratio\n");
		}
		break;
	default:
		break;
	}
	pr_info("not support vic = %d\n", vic);
	return 0;
}

int hdmitx21_get_aspect_ratio(void)
{
	enum hdmi_vic vic = HDMI_0_UNKNOWN;
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	vic = hdev->hwop.getstate(hdev, STAT_VIDEO_VIC, 0);
	if (vic == HDMI_2_720x480p60_4x3 || vic == HDMI_17_720x576p50_4x3)
		return AR_4X3;
	if (vic == HDMI_3_720x480p60_16x9 || vic == HDMI_18_720x576p50_16x9)
		return AR_16X9;

	return 0;
}

struct aspect_ratio_list *hdmitx21_get_support_ar_list(void)
{
	static struct aspect_ratio_list ar_list[4];
	int i = 0;

	memset(ar_list, 0, sizeof(ar_list));
	if (hdmitx_check_vic(HDMI_2_720x480p60_4x3)) {
		ar_list[i].vic = HDMI_2_720x480p60_4x3;
		ar_list[i].flag = TRUE;
		ar_list[i].aspect_ratio_num = 4;
		ar_list[i].aspect_ratio_den = 3;
		i++;
	}
	if (hdmitx_check_vic(HDMI_3_720x480p60_16x9)) {
		ar_list[i].vic = HDMI_3_720x480p60_16x9;
		ar_list[i].flag = TRUE;
		ar_list[i].aspect_ratio_num = 16;
		ar_list[i].aspect_ratio_den = 9;
		i++;
	}
	if (hdmitx_check_vic(HDMI_17_720x576p50_4x3)) {
		ar_list[i].vic = HDMI_17_720x576p50_4x3;
		ar_list[i].flag = TRUE;
		ar_list[i].aspect_ratio_num = 4;
		ar_list[i].aspect_ratio_den = 3;
		i++;
	}
	if (hdmitx_check_vic(HDMI_18_720x576p50_16x9)) {
		ar_list[i].vic = HDMI_18_720x576p50_16x9;
		ar_list[i].flag = TRUE;
		ar_list[i].aspect_ratio_num = 16;
		ar_list[i].aspect_ratio_den = 9;
		i++;
	}
	return &ar_list[0];
}

int hdmitx21_get_aspect_ratio_value(void)
{
	int i;
	int value = 0;
	static struct aspect_ratio_list *ar_list;
	struct hdmitx_dev *hdev = get_hdmitx21_device();
	enum hdmi_vic vic = HDMI_0_UNKNOWN;

	ar_list = hdmitx21_get_support_ar_list();
	vic = hdev->hwop.getstate(hdev, STAT_VIDEO_VIC, 0);

	if (vic == HDMI_2_720x480p60_4x3 || vic == HDMI_3_720x480p60_16x9) {
		for (i = 0; i < 4; i++) {
			if (ar_list[i].vic == HDMI_2_720x480p60_4x3)
				value++;
			if (ar_list[i].vic == HDMI_3_720x480p60_16x9)
				value++;
		}
	}

	if (vic == HDMI_17_720x576p50_4x3 || vic == HDMI_18_720x576p50_16x9) {
		for (i = 0; i < 4; i++) {
			if (ar_list[i].vic == HDMI_17_720x576p50_4x3)
				value++;
			if (ar_list[i].vic == HDMI_18_720x576p50_16x9)
				value++;
		}
	}

	if (value > 1) {
		value = hdmitx21_get_aspect_ratio();
		return value;
	}

	return 0;
}

void hdmitx21_set_aspect_ratio(int aspect_ratio)
{
	enum hdmi_vic vic = HDMI_0_UNKNOWN;
	struct hdmitx_dev *hdev = get_hdmitx21_device();
	int ret;
	int aspect_ratio_vic = 0;

	if (aspect_ratio != AR_4X3 && aspect_ratio != AR_16X9) {
		pr_info("aspect ratio should be 1 or 2");
		return;
	}

	vic = hdev->hwop.getstate(hdev, STAT_VIDEO_VIC, 0);
	ret = hdmitx_check_valid_aspect_ratio(vic, aspect_ratio);
	pr_info("%s vic = %d, ret = %d\n", __func__, vic, ret);

	if (!ret)
		return;

	switch (vic) {
	case HDMI_2_720x480p60_4x3:
		aspect_ratio_vic = (HDMI_3_720x480p60_16x9 << 2) + aspect_ratio;
		break;
	case HDMI_3_720x480p60_16x9:
		aspect_ratio_vic = (HDMI_2_720x480p60_4x3 << 2) + aspect_ratio;
		break;
	case HDMI_17_720x576p50_4x3:
		aspect_ratio_vic = (HDMI_18_720x576p50_16x9 << 2) + aspect_ratio;
		break;
	case HDMI_18_720x576p50_16x9:
		aspect_ratio_vic = (HDMI_17_720x576p50_4x3 << 2) + aspect_ratio;
		break;
	default:
		break;
	}

	hdev->hwop.cntlconfig(hdev, CONF_ASPECT_RATIO, aspect_ratio_vic);
	hdev->aspect_ratio = aspect_ratio;
	pr_info("set new aspect ratio = %d\n", aspect_ratio);
}

static void hdr_unmute_work_func(struct work_struct *work)
{
	unsigned int mute_us;
	struct hdmitx_dev *hdev =
		container_of(work, struct hdmitx_dev, work_hdr_unmute);

	if (hdr_mute_frame) {
		pr_info("vid mute %d frames before play hdr/hlg video\n",
			hdr_mute_frame);
		mute_us = hdr_mute_frame * hdmitx_get_frame_duration();
		usleep_range(mute_us, mute_us + 10);
		pr_info("%s: VID_UNMUTE\n", __func__);
		hdev->hwop.cntlconfig(hdev, CONF_VIDEO_MUTE_OP, VIDEO_UNMUTE);
	}
}

static void hdr_work_func(struct work_struct *work)
{
	struct hdmitx_dev *hdev =
		container_of(work, struct hdmitx_dev, work_hdr);
	struct hdmi_drm_infoframe *info = &hdev->infoframes.drm.drm;

	if (hdev->hdr_transfer_feature == T_BT709 &&
	    hdev->hdr_color_feature == C_BT709) {
		pr_info("%s: send zero DRM\n", __func__);
		hdmi_drm_infoframe_init(info);
		hdmi_drm_infoframe_set(info);
		hdmi_avi_infoframe_config(CONF_AVI_BT2020, hdev->colormetry);

		msleep(1500);/*delay 1.5s*/
		/* disable DRM packets completely ONLY if hdr transfer
		 * feature and color feature still demand SDR.
		 */
		if (hdr_status_pos == 4) {
			/* zero hdr10+ VSIF being sent - disable it */
			pr_info("%s: disable hdr10+ vsif\n", __func__);
			/* hdmi_vend_infoframe_set(NULL); */
			hdmi_vend_infoframe_rawset(NULL, NULL);
			hdr_status_pos = 0;
		}
		if (hdev->hdr_transfer_feature == T_BT709 &&
		    hdev->hdr_color_feature == C_BT709) {
			pr_info("%s: disable DRM\n", __func__);
			hdmi_drm_infoframe_set(NULL);
			hdev->hdmi_current_hdr_mode = 0;
			hdmitx_sdr_hdr_uevent(hdev);
		}
	} else {
		hdmitx_sdr_hdr_uevent(hdev);
	}
}

#define hdmi_debug() \
	do { \
		if (log21_level == 0xff) \
			pr_info("%s[%d]\n", __func__, __LINE__); \
	} while (0)

static bool _check_hdmi_mode(void)
{
	struct vinfo_s *vinfo = NULL;

	vinfo = get_current_vinfo();
	if (vinfo && vinfo->mode == VMODE_HDMI)
		return 1;
	vinfo = get_current_vinfo2();
	if (vinfo && vinfo->mode == VMODE_HDMI)
		return 1;
	return 0;
}

bool is_cur_mode_hdmi(void)
{
	return _check_hdmi_mode();
}

#define GET_LOW8BIT(a)	((a) & 0xff)
#define GET_HIGH8BIT(a)	(((a) >> 8) & 0xff)
static void hdmitx_set_drm_pkt(struct master_display_info_s *data)
{
	struct hdmitx_dev *hdev = get_hdmitx21_device();
	struct hdr_info *hdr_info = &hdev->rxcap.hdr_info;
	u8 drm_hb[3] = {0x87, 0x1, 26};
	static u8 db[28] = {0x0};
	u8 *drm_db = &db[1]; /* db[0] is the checksum */
	unsigned long flags = 0;

	if (!_check_hdmi_mode())
		return;
	hdmi_debug();
	spin_lock_irqsave(&hdev->edid_spinlock, flags);
	if (hdr_status_pos == 4) {
		/* zero hdr10+ VSIF being sent - disable it */
		pr_info("%s: disable hdr10+ zero vsif\n", __func__);
		/* hdmi_vend_infoframe_set(NULL); */
		/* todo, maybe need recover hdmi1.4b_vsif when 4k*/
		hdmi_vend_infoframe_rawset(NULL, NULL);
		hdr_status_pos = 0;
	}

	/*
	 *hdr_color_feature: bit 23-16: color_primaries
	 *	1:bt709 0x9:bt2020
	 *hdr_transfer_feature: bit 15-8: transfer_characteristic
	 *	1:bt709 0xe:bt2020-10 0x10:smpte-st-2084 0x12:hlg(todo)
	 */
	if (data) {
		if ((hdev->hdr_transfer_feature !=
			((data->features >> 8) & 0xff)) ||
			(hdev->hdr_color_feature !=
			((data->features >> 16) & 0xff)) ||
			(hdev->colormetry !=
			((data->features >> 30) & 0x1))) {
			hdev->hdr_transfer_feature =
				(data->features >> 8) & 0xff;
			hdev->hdr_color_feature =
				(data->features >> 16) & 0xff;
			hdev->colormetry =
				(data->features >> 30) & 0x1;
			pr_info("%s: tf=%d, cf=%d, colormetry=%d\n",
				__func__,
				hdev->hdr_transfer_feature,
				hdev->hdr_color_feature,
				hdev->colormetry);
		}
	} else {
		pr_info("%s: disable drm pkt\n", __func__);
	}
	hdr_status_pos = 1;
	/* if VSIF/DV or VSIF/HDR10P packet is enabled, disable it */
	if (hdmitx21_dv_en()) {
		update_current_para(hdev);
		hdmi_avi_infoframe_config(CONF_AVI_CS, hdev->para->cs);
/* if using VSIF/DOVI, then only clear DV_VS10_SIG, else disable VSIF */
		if (hdev->hwop.cntlconfig(hdev, CONF_CLR_DV_VS10_SIG, 0) == 0)
			/* hdmi_vend_infoframe_set(NULL); */
			hdmi_vend_infoframe_rawset(NULL, NULL);
	}

	/* hdr10+ content on a hdr10 sink case */
	if (hdev->hdr_transfer_feature == 0x30) {
		if (hdr_info->hdr10plus_info.ieeeoui != 0x90848B ||
		    hdr_info->hdr10plus_info.application_version != 1) {
			hdev->hdr_transfer_feature = T_SMPTE_ST_2084;
			pr_info("%s: HDR10+ not supported, treat as hdr10\n",
				__func__);
		}
	}

	if (!data || !hdev->rxcap.hdr_info2.hdr_support) {
		drm_hb[1] = 0;
		drm_hb[2] = 0;
		drm_db[0] = 0;
		hdmi_drm_infoframe_set(NULL);
		hdmi_avi_infoframe_config(CONF_AVI_BT2020, hdev->colormetry);
		spin_unlock_irqrestore(&hdev->edid_spinlock, flags);
		return;
	}

	/*SDR*/
	if (hdev->hdr_transfer_feature == T_BT709 &&
		hdev->hdr_color_feature == C_BT709) {
		/* send zero drm only for HDR->SDR transition */
		if (drm_db[0] == 0x02 || drm_db[0] == 0x03) {
			pr_info("%s: HDR->SDR, drm_db[0]=%d\n",
				__func__, drm_db[0]);
			hdev->colormetry = 0;
			hdmi_avi_infoframe_config(CONF_AVI_BT2020, 0);
			schedule_work(&hdev->work_hdr);
			drm_db[0] = 0;
		}
		spin_unlock_irqrestore(&hdev->edid_spinlock, flags);
		return;
	}

	drm_db[1] = 0x0;
	drm_db[2] = GET_LOW8BIT(data->primaries[0][0]);
	drm_db[3] = GET_HIGH8BIT(data->primaries[0][0]);
	drm_db[4] = GET_LOW8BIT(data->primaries[0][1]);
	drm_db[5] = GET_HIGH8BIT(data->primaries[0][1]);
	drm_db[6] = GET_LOW8BIT(data->primaries[1][0]);
	drm_db[7] = GET_HIGH8BIT(data->primaries[1][0]);
	drm_db[8] = GET_LOW8BIT(data->primaries[1][1]);
	drm_db[9] = GET_HIGH8BIT(data->primaries[1][1]);
	drm_db[10] = GET_LOW8BIT(data->primaries[2][0]);
	drm_db[11] = GET_HIGH8BIT(data->primaries[2][0]);
	drm_db[12] = GET_LOW8BIT(data->primaries[2][1]);
	drm_db[13] = GET_HIGH8BIT(data->primaries[2][1]);
	drm_db[14] = GET_LOW8BIT(data->white_point[0]);
	drm_db[15] = GET_HIGH8BIT(data->white_point[0]);
	drm_db[16] = GET_LOW8BIT(data->white_point[1]);
	drm_db[17] = GET_HIGH8BIT(data->white_point[1]);
	drm_db[18] = GET_LOW8BIT(data->luminance[0]);
	drm_db[19] = GET_HIGH8BIT(data->luminance[0]);
	drm_db[20] = GET_LOW8BIT(data->luminance[1]);
	drm_db[21] = GET_HIGH8BIT(data->luminance[1]);
	drm_db[22] = GET_LOW8BIT(data->max_content);
	drm_db[23] = GET_HIGH8BIT(data->max_content);
	drm_db[24] = GET_LOW8BIT(data->max_frame_average);
	drm_db[25] = GET_HIGH8BIT(data->max_frame_average);

	/* bt2020 + gamma transfer */
	if (hdev->hdr_transfer_feature == T_BT709 &&
	    hdev->hdr_color_feature == C_BT2020) {
		if (hdev->sdr_hdr_feature == 0) {
			hdmi_drm_infoframe_set(NULL);
			hdmi_avi_infoframe_config(CONF_AVI_BT2020, SET_AVI_BT2020);
		} else if (hdev->sdr_hdr_feature == 1) {
			memset(db, 0, sizeof(db));
			hdmi_drm_infoframe_rawset(drm_hb, db);
			hdmi_avi_infoframe_config(CONF_AVI_BT2020, SET_AVI_BT2020);
		} else {
			drm_db[0] = 0x02; /* SMPTE ST 2084 */
			hdmi_drm_infoframe_rawset(drm_hb, db);
			hdmi_avi_infoframe_config(CONF_AVI_BT2020, SET_AVI_BT2020);
		}
		spin_unlock_irqrestore(&hdev->edid_spinlock, flags);
		return;
	}

	/*must clear hdr mode*/
	hdev->hdmi_current_hdr_mode = 0;

	/* SMPTE ST 2084 and (BT2020 or NON_STANDARD) */
	if (hdev->rxcap.hdr_info2.hdr_support & 0x4) {
		if (hdev->hdr_transfer_feature == T_SMPTE_ST_2084 &&
		    hdev->hdr_color_feature == C_BT2020)
			hdev->hdmi_current_hdr_mode = 1;
		else if (hdev->hdr_transfer_feature == T_SMPTE_ST_2084 &&
			 hdev->hdr_color_feature != C_BT2020)
			hdev->hdmi_current_hdr_mode = 2;
	}

	/*HLG and BT2020*/
	if (hdev->rxcap.hdr_info2.hdr_support & 0x8) {
		if (hdev->hdr_color_feature == C_BT2020 &&
		    (hdev->hdr_transfer_feature == T_BT2020_10 ||
		     hdev->hdr_transfer_feature == T_HLG))
			hdev->hdmi_current_hdr_mode = 3;
	}

	switch (hdev->hdmi_current_hdr_mode) {
	case 1:
		/*standard HDR*/
		drm_db[0] = 0x02; /* SMPTE ST 2084 */
		hdmi_drm_infoframe_rawset(drm_hb, db);
		hdmi_avi_infoframe_config(CONF_AVI_BT2020, SET_AVI_BT2020);
		break;
	case 2:
		/*non standard*/
		drm_db[0] = 0x02; /* no standard SMPTE ST 2084 */
		hdmi_drm_infoframe_rawset(drm_hb, db);
		hdmi_avi_infoframe_config(CONF_AVI_BT2020, CLR_AVI_BT2020);
		break;
	case 3:
		/*HLG*/
		drm_db[0] = 0x03;/* HLG is 0x03 */
		hdmi_drm_infoframe_rawset(drm_hb, db);
		hdmi_avi_infoframe_config(CONF_AVI_BT2020, SET_AVI_BT2020);
		break;
	case 0:
	default:
		/*other case*/
		hdmi_drm_infoframe_set(NULL);
		hdmi_avi_infoframe_config(CONF_AVI_BT2020, CLR_AVI_BT2020);
		break;
	}

	/* if sdr/hdr mode change ,notify uevent to userspace*/
	if (hdev->hdmi_current_hdr_mode != hdev->hdmi_last_hdr_mode) {
		/* NOTE: for HDR <-> HLG, also need update last mode */
		hdev->hdmi_last_hdr_mode = hdev->hdmi_current_hdr_mode;
		if (hdr_mute_frame) {
			pr_info("%s: VID_MUTE\n", __func__);
			hdev->hwop.cntlconfig(hdev, CONF_VIDEO_MUTE_OP, VIDEO_MUTE);
			pr_info("SDR->HDR enter mute\n");
			/* force unmute after specific frames,
			 * no need to check hdr status when unmute
			 */
			schedule_work(&hdev->work_hdr_unmute);
		}
		schedule_work(&hdev->work_hdr);
	}
	spin_unlock_irqrestore(&hdev->edid_spinlock, flags);
}

static struct emp_packet_st pkt_sbtm;
static void hdmitx_set_sbtm_pkt(struct vtem_sbtm_st *data)
{
	struct emp_packet_st *sbtm = &pkt_sbtm;

	if (!data) {
		hdmi_emp_infoframe_set(EMP_TYPE_SBTM, NULL);
		return;
	}
	memset(sbtm, 0, sizeof(*sbtm));
	sbtm->type = EMP_TYPE_SBTM;
	hdmi_emp_frame_set_member(sbtm, CONF_HEADER_INIT, HDMI_INFOFRAME_TYPE_EMP);
	hdmi_emp_frame_set_member(sbtm, CONF_HEADER_FIRST, 1);
	hdmi_emp_frame_set_member(sbtm, CONF_HEADER_LAST, 1);
	hdmi_emp_frame_set_member(sbtm, CONF_HEADER_SEQ_INDEX, 0);
	hdmi_emp_frame_set_member(sbtm, CONF_DS_TYPE, 1);
	hdmi_emp_frame_set_member(sbtm, CONF_SYNC, 1);
	hdmi_emp_frame_set_member(sbtm, CONF_VFR, 1);
	hdmi_emp_frame_set_member(sbtm, CONF_AFR, 0);
	hdmi_emp_frame_set_member(sbtm, CONF_NEW, 0);
	hdmi_emp_frame_set_member(sbtm, CONF_END, 0);
	hdmi_emp_frame_set_member(sbtm, CONF_ORG_ID, 1);
	hdmi_emp_frame_set_member(sbtm, CONF_DATA_SET_TAG, 3);
	hdmi_emp_frame_set_member(sbtm, CONF_DATA_SET_LENGTH, 4);
	hdmi_emp_frame_set_member(sbtm, CONF_SBTM_VER, data->sbtm_ver);
	hdmi_emp_frame_set_member(sbtm, CONF_SBTM_MODE, data->sbtm_mode);
	hdmi_emp_frame_set_member(sbtm, CONF_SBTM_TYPE, data->sbtm_type);
	hdmi_emp_frame_set_member(sbtm, CONF_SBTM_GRDM_MIN, data->grdm_min);
	hdmi_emp_frame_set_member(sbtm, CONF_SBTM_GRDM_LUM, data->grdm_lum);
	hdmi_emp_frame_set_member(sbtm, CONF_SBTM_FRMPBLIMITINT, data->frmpblimitint);
	hdmi_emp_infoframe_set(EMP_TYPE_SBTM, sbtm);
}

static void update_current_para(struct hdmitx_dev *hdev)
{
	struct vinfo_s *info = NULL;
	u8 mode[32];

	info = hdmitx_get_current_vinfo(NULL);
	if (!info || !info->name)
		return;

	memset(mode, 0, sizeof(mode));
	strncpy(mode, info->name, sizeof(mode) - 1);
	hdev->para = hdmitx21_get_fmtpara(mode, hdev->fmt_attr);
}

static struct vsif_debug_save vsif_debug_info;
static void hdmitx_set_vsif_pkt(enum eotf_type type,
				enum mode_type tunnel_mode,
				struct dv_vsif_para *data,
				bool signal_sdr)
{
	struct hdmitx_dev *hdev = get_hdmitx21_device();
	struct dv_vsif_para para = {0};
	u8 ven_hb[3] = {0x81, 0x01};
	u8 db1[28] = {0x00};
	u8 *ven_db1 = &db1[1];
	u8 db2[28] = {0x00};
	u8 *ven_db2 = &db2[1];
	u8 len = 0;
	u32 vic = hdev->cur_VIC;
	u32 hdmi_vic_4k_flag = 0;
	static enum eotf_type ltype = EOTF_T_NULL;
	static u8 ltmode = -1;
	enum hdmi_tf_type hdr_type = HDMI_NONE;
	unsigned long flags = 0;

	if (!_check_hdmi_mode())
		return;
	hdmi_debug();
	spin_lock_irqsave(&hdev->edid_spinlock, flags);
	if (hdev->bist_lock) {
		spin_unlock_irqrestore(&hdev->edid_spinlock, flags);
		return;
	}

	if (!data)
		memcpy(&vsif_debug_info.data, &para,
		       sizeof(struct dv_vsif_para));
	else
		memcpy(&vsif_debug_info.data, data,
		       sizeof(struct dv_vsif_para));
	vsif_debug_info.type = type;
	vsif_debug_info.tunnel_mode = tunnel_mode;
	vsif_debug_info.signal_sdr = signal_sdr;

	if (hdev->ready == 0) {
		ltype = EOTF_T_NULL;
		ltmode = -1;
		spin_unlock_irqrestore(&hdev->edid_spinlock, flags);
		return;
	}
	if (hdev->rxcap.dv_info.ieeeoui != DV_IEEE_OUI) {
		if (type == 0 && !data && signal_sdr)
			pr_info("TV not support DV, clr dv_vsif\n");
	}

	if (hdr_status_pos != 2)
		pr_info("%s: type = %d\n", __func__, type);
	hdr_status_pos = 2;

	/* if DRM/HDR packet is enabled, disable it */
	hdr_type = hdmitx21_get_cur_hdr_st();
	if (hdr_type != HDMI_NONE && hdr_type != HDMI_HDR_SDR) {
		hdev->hdr_transfer_feature = T_BT709;
		hdev->hdr_color_feature = C_BT709;
		hdev->colormetry = 0;
		hdmi_avi_infoframe_config(CONF_AVI_BT2020, hdev->colormetry);
		schedule_work(&hdev->work_hdr);
	}

	hdev->hdmi_current_eotf_type = type;
	hdev->hdmi_current_tunnel_mode = tunnel_mode;
	if (vic == HDMI_95_3840x2160p30_16x9 ||
		vic == HDMI_94_3840x2160p25_16x9 ||
		vic == HDMI_93_3840x2160p24_16x9 ||
		vic == HDMI_98_4096x2160p24_256x135)
		hdmi_vic_4k_flag = 1;

	/*ver0 and ver1_15 and ver1_12bit with ll= 0 use hdmi 1.4b VSIF*/
	if (hdev->rxcap.dv_info.ver == 0 ||
	    (hdev->rxcap.dv_info.ver == 1 &&
	    hdev->rxcap.dv_info.length == 0xE) ||
	    (hdev->rxcap.dv_info.ver == 1 &&
	    hdev->rxcap.dv_info.length == 0xB &&
	    hdev->rxcap.dv_info.low_latency == 0)) {

		switch (type) {
		case EOTF_T_DOLBYVISION:
			len = 0x18;
			hdev->dv_src_feature = 1;
			break;
		case EOTF_T_HDR10:
		case EOTF_T_SDR:
		case EOTF_T_NULL:
		default:
			len = 0x05;
			hdev->dv_src_feature = 0;
			break;
		}

		ven_hb[2] = len;
		ven_db1[0] = 0x03;
		ven_db1[1] = 0x0c;
		ven_db1[2] = 0x00;
		ven_db1[3] = 0x00;

		if (hdmi_vic_4k_flag) {
			ven_db1[3] = 0x20;
			if (vic == HDMI_95_3840x2160p30_16x9)
				ven_db1[4] = 0x1;
			else if (vic == HDMI_94_3840x2160p25_16x9)
				ven_db1[4] = 0x2;
			else if (vic == HDMI_93_3840x2160p24_16x9)
				ven_db1[4] = 0x3;
			else/*vic == HDMI_98_4096x2160p24_256x135*/
				ven_db1[4] = 0x4;
		}
		if (type == EOTF_T_DV_AHEAD) {
			hdmi_vend_infoframe_rawset(ven_hb, db1);
			spin_unlock_irqrestore(&hdev->edid_spinlock, flags);
			return;
		}
		if (type == EOTF_T_DOLBYVISION) {
			/* disable forced gaming in this mode because if we are
			 * here with forced gaming on, it means TV is not DV LL
			 * capable
			 */
			if (hdev->ll_user_set_mode == HDMI_LL_MODE_ENABLE &&
			   (hdev->allm_mode == 1 || hdev->ct_mode == 1)) {
				pr_info("hdmitx: Dolby H14b VSIF, disable forced game mode\n");
				hdmi_tx_enable_ll_mode(false);
			}
			/*first disable drm package*/
			hdmi_drm_infoframe_set(NULL);
			hdmi_vend_infoframe_rawset(ven_hb, db1);
			/* Dolby Vision Source System-on-Chip Platform Kit Version 2.6:
			 * 4.4.1 Expected AVI-IF for Dolby Vision output, need BT2020 for DV
			 */
			hdmi_avi_infoframe_config(CONF_AVI_BT2020, SET_AVI_BT2020);/*BT2020*/
			if (tunnel_mode == RGB_8BIT) {
				hdmi_avi_infoframe_config(CONF_AVI_CS, HDMI_COLORSPACE_RGB);
				hdmi_avi_infoframe_config(CONF_AVI_Q01, RGB_RANGE_FUL);
			} else {
				hdmi_avi_infoframe_config(CONF_AVI_CS, HDMI_COLORSPACE_YUV422);
				hdmi_avi_infoframe_config(CONF_AVI_YQ01, YCC_RANGE_FUL);
			}
		} else {
			if (hdmi_vic_4k_flag)
				hdmi_vend_infoframe_rawset(ven_hb, db1);
			else
				/* hdmi_vend_infoframe_set(NULL); */
				hdmi_vend_infoframe_rawset(NULL, NULL);
			if (signal_sdr) {
				pr_info("hdmitx: Dolby H14b VSIF, switching signal to SDR\n");
				update_current_para(hdev);
				hdmi_avi_infoframe_config(CONF_AVI_CS, hdev->para->cs);
				hdmi_avi_infoframe_config(CONF_AVI_Q01, RGB_RANGE_LIM);
				hdmi_avi_infoframe_config(CONF_AVI_YQ01, YCC_RANGE_LIM);
				hdmi_avi_infoframe_config(CONF_AVI_BT2020, CLR_AVI_BT2020);/*BT709*/
				/* re-enable forced game mode if selected by the user */
				if (hdev->ll_user_set_mode == HDMI_LL_MODE_ENABLE) {
					pr_info("hdmitx: Dolby H14b VSIF disabled, re-enable forced game mode\n");
					hdmi_tx_enable_ll_mode(true);
				}
			}
		}
	}
	/*ver1_12  with low_latency = 1 and ver2 use Dolby VSIF*/
	if (hdev->rxcap.dv_info.ver == 2 ||
	    (hdev->rxcap.dv_info.ver == 1 &&
	     hdev->rxcap.dv_info.length == 0xB &&
	     hdev->rxcap.dv_info.low_latency == 1) ||
	     type == EOTF_T_LL_MODE) {
		if (!data)
			data = &para;
		len = 0x1b;

		switch (type) {
		case EOTF_T_DOLBYVISION:
		case EOTF_T_LL_MODE:
			hdev->dv_src_feature = 1;
			break;
		case EOTF_T_HDR10:
		case EOTF_T_SDR:
		case EOTF_T_NULL:
		default:
			hdev->dv_src_feature = 0;
			break;
		}
		ven_hb[2] = len;
		ven_db2[0] = 0x46;
		ven_db2[1] = 0xd0;
		ven_db2[2] = 0x00;
		if (data->ver2_l11_flag == 1) {
			ven_db2[3] = data->vers.ver2_l11.low_latency |
				     data->vers.ver2_l11.dobly_vision_signal << 1 |
				     data->vers.ver2_l11.src_dm_version << 5;
			ven_db2[4] = data->vers.ver2_l11.eff_tmax_PQ_hi
				     | data->vers.ver2_l11.auxiliary_MD_present << 6
				     | data->vers.ver2_l11.backlt_ctrl_MD_present << 7
				     | 0x20; /*L11_MD_Present*/
			ven_db2[5] = data->vers.ver2_l11.eff_tmax_PQ_low;
			ven_db2[6] = data->vers.ver2_l11.auxiliary_runmode;
			ven_db2[7] = data->vers.ver2_l11.auxiliary_runversion;
			ven_db2[8] = data->vers.ver2_l11.auxiliary_debug0;
			ven_db2[9] = (data->vers.ver2_l11.content_type)
				| (data->vers.ver2_l11.content_sub_type << 4);
			ven_db2[10] = (data->vers.ver2_l11.intended_white_point)
				| (data->vers.ver2_l11.crf << 4);
			ven_db2[11] = data->vers.ver2_l11.l11_byte2;
			ven_db2[12] = data->vers.ver2_l11.l11_byte3;
		} else {
			ven_db2[3] = (data->vers.ver2.low_latency) |
				(data->vers.ver2.dobly_vision_signal << 1) |
				(data->vers.ver2.src_dm_version << 5);
			ven_db2[4] = (data->vers.ver2.eff_tmax_PQ_hi)
				| (data->vers.ver2.auxiliary_MD_present << 6)
				| (data->vers.ver2.backlt_ctrl_MD_present << 7);
			ven_db2[5] = data->vers.ver2.eff_tmax_PQ_low;
			ven_db2[6] = data->vers.ver2.auxiliary_runmode;
			ven_db2[7] = data->vers.ver2.auxiliary_runversion;
			ven_db2[8] = data->vers.ver2.auxiliary_debug0;
		}
		if (type == EOTF_T_DV_AHEAD) {
			hdmi_vend_infoframe_rawset(ven_hb, db2);
			spin_unlock_irqrestore(&hdev->edid_spinlock, flags);
			return;
		}
		/*Dolby Vision standard case*/
		if (type == EOTF_T_DOLBYVISION) {
			/* disable forced gaming in this mode because if we are
			 * here with forced gaming on, it means TV is not DV LL
			 * capable
			 */
			if (hdev->ll_user_set_mode == HDMI_LL_MODE_ENABLE &&
			   (hdev->allm_mode == 1 || hdev->ct_mode == 1)) {
				pr_info("hdmitx: Dolby VSIF, disable forced game mode\n");
				hdmi_tx_enable_ll_mode(false);
			}
			/*first disable drm package*/
			hdmi_drm_infoframe_set(NULL);
			hdmi_vend_infoframe_rawset(ven_hb, db2);
			/* Dolby Vision Source System-on-Chip Platform Kit Version 2.6:
			 * 4.4.1 Expected AVI-IF for Dolby Vision output, need BT2020 for DV
			 */
			hdmi_avi_infoframe_config(CONF_AVI_BT2020, SET_AVI_BT2020);/*BT.2020*/
			if (tunnel_mode == RGB_8BIT) {/*RGB444*/
				hdmi_avi_infoframe_config(CONF_AVI_CS, HDMI_COLORSPACE_RGB);
				hdmi_avi_infoframe_config(CONF_AVI_Q01, RGB_RANGE_FUL);
			} else {/*YUV422*/
				hdmi_avi_infoframe_config(CONF_AVI_CS, HDMI_COLORSPACE_YUV422);
				hdmi_avi_infoframe_config(CONF_AVI_YQ01, YCC_RANGE_FUL);
			}
			if (hdmi_vic_4k_flag)
				hdmitx21_set_avi_vic(vic);
		}
		/*Dolby Vision low-latency case*/
		else if  (type == EOTF_T_LL_MODE) {
			/* make sure forced game mode is enabled as there could be DV std
			 * to DV LL transition during uboot to kernel transition because
			 * of game mode forced enabled by user.
			 */
			if (hdev->ll_user_set_mode == HDMI_LL_MODE_ENABLE &&
			    hdev->allm_mode == 0 && hdev->ct_mode == 0) {
				pr_debug("hdmitx: Dolby LL VSIF, enable forced game mode\n");
				hdmi_tx_enable_ll_mode(true);
			}
			/*first disable drm package*/
			hdmi_drm_infoframe_set(NULL);
			hdmi_vend_infoframe_rawset(ven_hb, db2);
			/* Dolby vision HDMI Signaling Case25,
			 * UCD323 not declare bt2020 colorimetry,
			 * need to forcely send BT.2020
			 */
			hdmi_avi_infoframe_config(CONF_AVI_BT2020, SET_AVI_BT2020);
			if (tunnel_mode == RGB_10_12BIT) {/*10/12bit RGB444*/
				hdmi_avi_infoframe_config(CONF_AVI_CS, HDMI_COLORSPACE_RGB);
				hdmi_avi_infoframe_config(CONF_AVI_Q01, RGB_RANGE_LIM);
			} else if (tunnel_mode == YUV444_10_12BIT) {
				/*10/12bit YUV444*/
				hdmi_avi_infoframe_config(CONF_AVI_CS, HDMI_COLORSPACE_YUV444);
				hdmi_avi_infoframe_config(CONF_AVI_YQ01, YCC_RANGE_LIM);
			} else {/*YUV422*/
				hdmi_avi_infoframe_config(CONF_AVI_CS, HDMI_COLORSPACE_YUV422);
				hdmi_avi_infoframe_config(CONF_AVI_YQ01, YCC_RANGE_LIM);
			}
			/* save vic to AVI when send DV_VSIF*/
			if (hdmi_vic_4k_flag)
				hdmitx21_set_avi_vic(vic);
		} else { /*SDR case*/
			if (hdmi_vic_4k_flag) {
				/* recover HDMI1.4b_VSIF*/
				ven_hb[2] = 0x5;
				ven_db1[0] = 0x03;
				ven_db1[1] = 0x0c;
				ven_db1[2] = 0x00;
				ven_db1[3] = 0x20;
				if (vic == HDMI_95_3840x2160p30_16x9)
					ven_db1[4] = 0x1;
				else if (vic == HDMI_94_3840x2160p25_16x9)
					ven_db1[4] = 0x2;
				else if (vic == HDMI_93_3840x2160p24_16x9)
					ven_db1[4] = 0x3;
				else if (vic == HDMI_98_4096x2160p24_256x135)
					ven_db1[4] = 0x4;
				hdmi_vend_infoframe_rawset(ven_hb, db1);
				/* clear vic from AVI*/
				hdmitx21_set_avi_vic(0);
			} else {
				pr_info("hdmitx: Dolby VSIF, ven_db2[3]) = %d\n", ven_db2[3]);
				hdmi_vend_infoframe_rawset(ven_hb, db2);
			}
			if (signal_sdr) {
				pr_info("hdmitx: Dolby VSIF, switching signal to SDR\n");
				update_current_para(hdev);
				pr_info("vic:%d, cd:%d, cs:%d, cr:%d\n",
					hdev->para->timing.vic, hdev->para->cd,
					hdev->para->cs, hdev->para->cr);
				hdmi_avi_infoframe_config(CONF_AVI_CS, hdev->para->cs);
				hdmi_avi_infoframe_config(CONF_AVI_Q01, RGB_RANGE_DEFAULT);
				hdmi_avi_infoframe_config(CONF_AVI_YQ01, YCC_RANGE_LIM);
				/*BT709*/
				hdmi_avi_infoframe_config(CONF_AVI_BT2020, CLR_AVI_BT2020);
				/* re-enable forced game mode if selected by the user */
				if (hdev->ll_user_set_mode == HDMI_LL_MODE_ENABLE) {
					pr_info("hdmitx: Dolby VSIF disabled, re-enable forced game mode\n");
					hdmi_tx_enable_ll_mode(true);
				}
			}
		}
	}
	hdmitx21_dither_config(hdev);
	spin_unlock_irqrestore(&hdev->edid_spinlock, flags);
}

static void hdmitx_set_hdr10plus_pkt(u32 flag,
	struct hdr10plus_para *data)
{
	struct hdmitx_dev *hdev = get_hdmitx21_device();
	u32 vic = hdev->cur_VIC;
	u32 hdmi_vic_4k_flag = 0;
	u8 ven_hb[3] = {0x81, 0x01, 0x1b};
	u8 db[28] = {0x00};
	u8 *ven_db = &db[1];

	if (!_check_hdmi_mode())
		return;
	hdmi_debug();
	if (hdev->bist_lock)
		return;
	/* save vic to AVI when send HDR10P_VSIF*/
	if (vic == HDMI_95_3840x2160p30_16x9 ||
		vic == HDMI_94_3840x2160p25_16x9 ||
		vic == HDMI_93_3840x2160p24_16x9 ||
		vic == HDMI_98_4096x2160p24_256x135)
		hdmi_vic_4k_flag = 1;
	if (flag == HDR10_PLUS_ZERO_VSIF) {
		/* needed during hdr10+ to sdr transition */
		pr_info("%s: zero vsif\n", __func__);
		hdmi_vend_infoframe_rawset(ven_hb, db);
		hdmi_avi_infoframe_config(CONF_AVI_BT2020, CLR_AVI_BT2020);
		hdev->hdr10plus_feature = 0;
		if (hdmi_vic_4k_flag)
			hdmitx21_set_avi_vic(vic);
		hdr_status_pos = 4;
		return;
	}

	if (!data || !flag) {
		pr_info("%s: null vsif\n", __func__);
		/* hdmi_vend_infoframe_set(NULL); */
		hdmi_vend_infoframe_rawset(NULL, NULL);
		hdmi_avi_infoframe_config(CONF_AVI_BT2020, CLR_AVI_BT2020);
		hdev->hdr10plus_feature = 0;
		return;
	}

	if (hdev->hdr10plus_feature != 1)
		pr_info("%s: flag = %d\n", __func__, flag);
	hdev->hdr10plus_feature = 1;
	hdr_status_pos = 3;
	ven_db[0] = 0x8b;
	ven_db[1] = 0x84;
	ven_db[2] = 0x90;

	ven_db[3] = ((data->application_version & 0x3) << 6) |
		 ((data->targeted_max_lum & 0x1f) << 1);
	ven_db[4] = data->average_maxrgb;
	ven_db[5] = data->distribution_values[0];
	ven_db[6] = data->distribution_values[1];
	ven_db[7] = data->distribution_values[2];
	ven_db[8] = data->distribution_values[3];
	ven_db[9] = data->distribution_values[4];
	ven_db[10] = data->distribution_values[5];
	ven_db[11] = data->distribution_values[6];
	ven_db[12] = data->distribution_values[7];
	ven_db[13] = data->distribution_values[8];
	ven_db[14] = ((data->num_bezier_curve_anchors & 0xf) << 4) |
		((data->knee_point_x >> 6) & 0xf);
	ven_db[15] = ((data->knee_point_x & 0x3f) << 2) |
		((data->knee_point_y >> 8) & 0x3);
	ven_db[16] = data->knee_point_y  & 0xff;
	ven_db[17] = data->bezier_curve_anchors[0];
	ven_db[18] = data->bezier_curve_anchors[1];
	ven_db[19] = data->bezier_curve_anchors[2];
	ven_db[20] = data->bezier_curve_anchors[3];
	ven_db[21] = data->bezier_curve_anchors[4];
	ven_db[22] = data->bezier_curve_anchors[5];
	ven_db[23] = data->bezier_curve_anchors[6];
	ven_db[24] = data->bezier_curve_anchors[7];
	ven_db[25] = data->bezier_curve_anchors[8];
	ven_db[26] = ((data->graphics_overlay_flag & 0x1) << 7) |
		((data->no_delay_flag & 0x1) << 6);

	hdmi_vend_infoframe_rawset(ven_hb, db);
	hdmi_avi_infoframe_config(CONF_AVI_BT2020, SET_AVI_BT2020);
	if (hdmi_vic_4k_flag)
		hdmitx21_set_avi_vic(vic);
}

static void hdmitx_set_cuva_hdr_vsif(struct cuva_hdr_vsif_para *data)
{
	unsigned long flags = 0;
	struct hdmitx_dev *hdev = get_hdmitx21_device();
	unsigned int vic = hdev->cur_VIC;
	unsigned int hdmi_vic_4k_flag = 0;
	unsigned char ven_hb[3] = {0x81, 0x01, 0x1b};
	unsigned char db[28] = {0x00};
	unsigned char *ven_db = &db[1];
	unsigned int ieee_code;

	if (!_check_hdmi_mode())
		return;
	if (vic == HDMI_95_3840x2160p30_16x9 ||
		vic == HDMI_94_3840x2160p25_16x9 ||
		vic == HDMI_93_3840x2160p24_16x9 ||
		vic == HDMI_98_4096x2160p24_256x135)
		hdmi_vic_4k_flag = 1;
	spin_lock_irqsave(&hdev->edid_spinlock, flags);
	if (!data) {
		ieee_code = hdmitx21_get_vender_infoframe_ieee();
		if (ieee_code == CUVA_IEEEOUI) {
			if (hdmi_vic_4k_flag) {
				ven_hb[2] = 0x5;
				ven_db[0] = 0x03;
				ven_db[1] = 0x0c;
				ven_db[2] = 0x00;
				ven_db[3] = 0x20;
				if (vic == HDMI_95_3840x2160p30_16x9)
					ven_db[4] = 0x1;
				else if (vic == HDMI_94_3840x2160p25_16x9)
					ven_db[4] = 0x2;
				else if (vic == HDMI_93_3840x2160p24_16x9)
					ven_db[4] = 0x3;
				else if (vic == HDMI_98_4096x2160p24_256x135)
					ven_db[4] = 0x4;
				hdmi_vend_infoframe_rawset(ven_hb, db);
				hdmitx21_set_avi_vic(0);
				pr_info("%s: recover hdmi1.4b_vsif\n", __func__);
			} else {
				hdmi_vend_infoframe_rawset(NULL, NULL);
				pr_info("%s: clear vender infoframe\n", __func__);
			}
		}
		spin_unlock_irqrestore(&hdev->edid_spinlock, flags);
		return;
	}
	ven_db[0] = GET_OUI_BYTE0(CUVA_IEEEOUI);
	ven_db[1] = GET_OUI_BYTE1(CUVA_IEEEOUI);
	ven_db[2] = GET_OUI_BYTE2(CUVA_IEEEOUI);
	ven_db[3] = data->system_start_code;
	ven_db[4] = (data->version_code & 0xf) << 4;
	hdmi_vend_infoframe_rawset(ven_hb, db);
	if (hdmi_vic_4k_flag)
		hdmitx21_set_avi_vic(vic);
	spin_unlock_irqrestore(&hdev->edid_spinlock, flags);
}

static void hdmitx_set_cuva_hdr_vs_emds(struct cuva_hdr_vs_emds_para *data)
{
	struct hdmi_packet_t vs_emds[3];
	unsigned long flags;
	struct hdmitx_dev *hdev = get_hdmitx21_device();
	int max_size;

	if (!_check_hdmi_mode())
		return;

	memset(vs_emds, 0, sizeof(vs_emds));
	spin_lock_irqsave(&hdev->edid_spinlock, flags);
	if (!data) {
		hdmitx_dhdr_send(NULL, 0);
		spin_unlock_irqrestore(&hdev->edid_spinlock, flags);
		return;
	}

	hdr_status_pos = 4;
	max_size = sizeof(struct hdmi_packet_t) * 3;
	vs_emds[0].hb[0] = 0x7f;
	vs_emds[0].hb[1] = 1 << 7;
	vs_emds[0].hb[2] = 0; /* Sequence_Index */
	vs_emds[0].pb[0] = (1 << 7) | (1 << 4) | (1 << 2) | (1 << 1);
	vs_emds[0].pb[1] = 0; /* rsvd */
	vs_emds[0].pb[2] = 0; /* Organization_ID */
	vs_emds[0].pb[3] = 0; /* Data_Set_Tag_MSB */
	vs_emds[0].pb[4] = 2; /* Data_Set_Tag_LSB */
	vs_emds[0].pb[5] = 0; /* Data_Set_Length_MSB */
	vs_emds[0].pb[6] = 0x38; /* Data_Set_Length_LSB */
	vs_emds[0].pb[7] = GET_OUI_BYTE0(CUVA_IEEEOUI);
	vs_emds[0].pb[8] = GET_OUI_BYTE1(CUVA_IEEEOUI);
	vs_emds[0].pb[9] = GET_OUI_BYTE2(CUVA_IEEEOUI);
	vs_emds[0].pb[10] = data->system_start_code;
	vs_emds[0].pb[11] = ((data->version_code & 0xf) << 4) |
			     ((data->min_maxrgb_pq >> 8) & 0xf);
	vs_emds[0].pb[12] = data->min_maxrgb_pq & 0xff;
	vs_emds[0].pb[13] = (data->avg_maxrgb_pq >> 8) & 0xf;
	vs_emds[0].pb[14] = data->avg_maxrgb_pq & 0xff;
	vs_emds[0].pb[15] = (data->var_maxrgb_pq >> 8) & 0xf;
	vs_emds[0].pb[16] = data->var_maxrgb_pq & 0xff;
	vs_emds[0].pb[17] = (data->max_maxrgb_pq >> 8) & 0xf;
	vs_emds[0].pb[18] = data->max_maxrgb_pq & 0xff;
	vs_emds[0].pb[19] = (data->targeted_max_lum_pq >> 8) & 0xf;
	vs_emds[0].pb[20] = data->targeted_max_lum_pq & 0xff;
	vs_emds[0].pb[21] = ((data->transfer_character & 1) << 7) |
			     ((data->base_enable_flag & 0x1) << 6) |
			     ((data->base_param_m_p >> 8) & 0x3f);
	vs_emds[0].pb[22] = data->base_param_m_p & 0xff;
	vs_emds[0].pb[23] = data->base_param_m_m & 0x3f;
	vs_emds[0].pb[24] = (data->base_param_m_a >> 8) & 0x3;
	vs_emds[0].pb[25] = data->base_param_m_a & 0xff;
	vs_emds[0].pb[26] = (data->base_param_m_b >> 8) & 0x3;
	vs_emds[0].pb[27] = data->base_param_m_b & 0xff;
	vs_emds[1].hb[0] = 0x7f;
	vs_emds[1].hb[1] = 0;
	vs_emds[1].hb[2] = 1; /* Sequence_Index */
	vs_emds[1].pb[0] = data->base_param_m_n & 0x3f;
	vs_emds[1].pb[1] = (((data->base_param_k[0] & 3) << 4) |
			   ((data->base_param_k[1] & 3) << 2) |
			   ((data->base_param_k[2] & 3) << 0));
	vs_emds[1].pb[2] = data->base_param_delta_enable_mode & 0x7;
	vs_emds[1].pb[3] = data->base_param_enable_delta & 0x7f;
	vs_emds[1].pb[4] = (((data->_3spline_enable_num & 0x3) << 3) |
			    ((data->_3spline_enable_flag & 1)  << 2) |
			    (data->_3spline_data[0].th_enable_mode & 0x3));
	vs_emds[1].pb[5] = data->_3spline_data[0].th_enable_mb;
	vs_emds[1].pb[6] = (data->_3spline_data[0].th_enable >> 8) & 0xf;
	vs_emds[1].pb[7] = data->_3spline_data[0].th_enable & 0xff;
	vs_emds[1].pb[8] =
		(data->_3spline_data[0].th_enable_delta[0] >> 8) & 0x3;
	vs_emds[1].pb[9] = data->_3spline_data[0].th_enable_delta[0] & 0xff;
	vs_emds[1].pb[10] =
		(data->_3spline_data[0].th_enable_delta[1] >> 8) & 0x3;
	vs_emds[1].pb[11] = data->_3spline_data[0].th_enable_delta[1] & 0xff;
	vs_emds[1].pb[12] = data->_3spline_data[0].enable_strength;
	vs_emds[1].pb[13] = data->_3spline_data[1].th_enable_mode & 0x3;
	vs_emds[1].pb[14] = data->_3spline_data[1].th_enable_mb;
	vs_emds[1].pb[15] = (data->_3spline_data[1].th_enable >> 8) & 0xf;
	vs_emds[1].pb[16] = data->_3spline_data[1].th_enable & 0xff;
	vs_emds[1].pb[17] =
		(data->_3spline_data[1].th_enable_delta[0] >> 8) & 0x3;
	vs_emds[1].pb[18] = data->_3spline_data[1].th_enable_delta[0] & 0xff;
	vs_emds[1].pb[19] =
		(data->_3spline_data[1].th_enable_delta[1] >> 8) & 0x3;
	vs_emds[1].pb[20] = data->_3spline_data[1].th_enable_delta[1] & 0xff;
	vs_emds[1].pb[21] = data->_3spline_data[1].enable_strength;
	vs_emds[1].pb[22] = data->color_saturation_num;
	vs_emds[1].pb[23] = data->color_saturation_gain[0];
	vs_emds[1].pb[24] = data->color_saturation_gain[1];
	vs_emds[1].pb[25] = data->color_saturation_gain[2];
	vs_emds[1].pb[26] = data->color_saturation_gain[3];
	vs_emds[1].pb[27] = data->color_saturation_gain[4];
	vs_emds[2].hb[0] = 0x7f;
	vs_emds[2].hb[1] = (1 << 6);
	vs_emds[2].hb[2] = 2; /* Sequence_Index */
	vs_emds[2].pb[0] = data->color_saturation_gain[5];
	vs_emds[2].pb[1] = data->color_saturation_gain[6];
	vs_emds[2].pb[2] = data->color_saturation_gain[7];
	vs_emds[2].pb[3] = data->graphic_src_display_value;
	vs_emds[2].pb[4] = 0; /* Reserved */
	vs_emds[2].pb[5] = data->max_display_mastering_lum >> 8;
	vs_emds[2].pb[6] = data->max_display_mastering_lum & 0xff;

	hdmitx_dhdr_send((u8 *)&vs_emds, max_size);
	spin_unlock_irqrestore(&hdev->edid_spinlock, flags);
}

/* reserved,  left blank here, move to hdmi_tx_vrr.c file */
static void hdmitx_set_emp_pkt(u8 *data, u32 type, u32 size)
{
}

/*config attr*/
static ssize_t config_show(struct device *dev,
			   struct device_attribute *attr, char *buf)
{
	int pos = 0;
	u8 *conf;
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	pos += snprintf(buf + pos, PAGE_SIZE, "cur_VIC: %d\n", hdev->cur_VIC);

	if (hdev->para) {
		switch (hdev->para->cd) {
		case COLORDEPTH_24B:
			conf = "8bit";
			break;
		case COLORDEPTH_30B:
			conf = "10bit";
			break;
		case COLORDEPTH_36B:
			conf = "12bit";
			break;
		case COLORDEPTH_48B:
			conf = "16bit";
			break;
		default:
			conf = "reserved";
		}
		pos += snprintf(buf + pos, PAGE_SIZE, "colordepth: %s\n",
				conf);
		switch (hdev->para->cs) {
		case HDMI_COLORSPACE_RGB:
			conf = "RGB";
			break;
		case HDMI_COLORSPACE_YUV422:
			conf = "422";
			break;
		case HDMI_COLORSPACE_YUV444:
			conf = "444";
			break;
		case HDMI_COLORSPACE_YUV420:
			conf = "420";
			break;
		default:
			conf = "reserved";
		}
		pos += snprintf(buf + pos, PAGE_SIZE, "colorspace: %s\n",
				conf);
	}

	switch (hdev->tx_aud_cfg) {
	case 0:
		conf = "off";
		break;
	case 1:
		conf = "on";
		break;
	case 2:
		conf = "auto";
		break;
	default:
		conf = "none";
	}
	pos += snprintf(buf + pos, PAGE_SIZE, "audio config: %s\n", conf);

	switch (hdev->hdmi_audio_off_flag) {
	case 0:
		conf = "on";
		break;
	case 1:
		conf = "off";
		break;
	default:
		conf = "none";
	}
	pos += snprintf(buf + pos, PAGE_SIZE, "audio on/off: %s\n", conf);

	switch (hdev->tx_aud_src) {
	case 0:
		conf = "SPDIF";
		break;
	case 1:
		conf = "I2S";
		break;
	default:
		conf = "none";
	}
	pos += snprintf(buf + pos, PAGE_SIZE, "audio source: %s\n", conf);

	switch (hdev->cur_audio_param.type) {
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
	pos += snprintf(buf + pos, PAGE_SIZE, "audio type: %s\n", conf);

	switch (hdev->cur_audio_param.channel_num) {
	case CC_REFER_TO_STREAM:
		conf = "refer to stream header";
		break;
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
	default:
		conf = "MAX";
		break;
	}
	pos += snprintf(buf + pos, PAGE_SIZE, "audio channel num: %s\n", conf);

	switch (hdev->cur_audio_param.sample_rate) {
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
		break;
	case FS_768K:
		conf = "768kHz";
		break;
	default:
		conf = "MAX";
	}
	pos += snprintf(buf + pos, PAGE_SIZE, "audio sample rate: %s\n", conf);

	switch (hdev->cur_audio_param.sample_size) {
	case SS_REFER_TO_STREAM:
		conf = "refer to stream header";
		break;
	case SS_16BITS:
		conf = "16bit";
		break;
	case SS_20BITS:
		conf = "20bit";
		break;
	case SS_24BITS:
		conf = "24bit";
		break;
	default:
		conf = "MAX";
	}
	pos += snprintf(buf + pos, PAGE_SIZE, "audio sample size: %s\n", conf);

	if (hdev->flag_3dfp)
		conf = "FramePacking";
	else if (hdev->flag_3dss)
		conf = "SidebySide";
	else if (hdev->flag_3dtb)
		conf = "TopButtom";
	else
		conf = "off";
	pos += snprintf(buf + pos, PAGE_SIZE, "3D config: %s\n", conf);
	return pos;
}

static ssize_t config_store(struct device *dev,
			    struct device_attribute *attr,
			    const char *buf, size_t count)
{
	int ret = 0;
	struct master_display_info_s data = {0};
	struct hdr10plus_para hdr_data = {0x1, 0x2, 0x3};
	struct hdmitx_dev *hdev = get_hdmitx21_device();
	struct cuva_hdr_vs_emds_para cuva_data = {0x1, 0x2, 0x3};

	pr_info("hdmitx: config: %s\n", buf);

	if (strncmp(buf, "unplug_powerdown", 16) == 0) {
		if (buf[16] == '0')
			hdev->unplug_powerdown = 0;
		else
			hdev->unplug_powerdown = 1;
	} else if (strncmp(buf, "info", 4) == 0) {
		pr_info("%x %x %x %x %x %x\n",
			hdmitx21_get_cur_hdr_st(),
			hdmitx21_get_cur_dv_st(),
			hdmitx21_get_cur_hdr10p_st(),
			hdmitx21_hdr_en(),
			hdmitx21_dv_en(),
			hdmitx21_hdr10p_en()
			);
	} else if (strncmp(buf, "3d", 2) == 0) {
		/* Second, set 3D parameters */
		if (strncmp(buf + 2, "tb", 2) == 0) {
			hdev->flag_3dtb = 1;
			hdev->flag_3dss = 0;
			hdev->flag_3dfp = 0;
			hdmi21_set_3d(hdev, T3D_TAB, 0);
		} else if ((strncmp(buf + 2, "lr", 2) == 0) ||
			(strncmp(buf + 2, "ss", 2) == 0)) {
			unsigned long sub_sample_mode = 0;

			hdev->flag_3dtb = 0;
			hdev->flag_3dss = 1;
			hdev->flag_3dfp = 0;
			if (buf[2])
				ret = kstrtoul(buf + 2, 10,
					       &sub_sample_mode);
			/* side by side */
			hdmi21_set_3d(hdev, T3D_SBS_HALF,
				    sub_sample_mode);
		} else if (strncmp(buf + 2, "fp", 2) == 0) {
			hdev->flag_3dtb = 0;
			hdev->flag_3dss = 0;
			hdev->flag_3dfp = 1;
			hdmi21_set_3d(hdev, T3D_FRAME_PACKING, 0);
		} else if (strncmp(buf + 2, "off", 3) == 0) {
			hdev->flag_3dfp = 0;
			hdev->flag_3dtb = 0;
			hdev->flag_3dss = 0;
			hdmi21_set_3d(hdev, T3D_DISABLE, 0);
		}
	} else if (strncmp(buf, "sdr", 3) == 0) {
		data.features = 0x00010100;
		hdmitx_set_drm_pkt(&data);
	} else if (strncmp(buf, "hdr", 3) == 0) {
		data.features = 0x00091000;
		hdmitx_set_drm_pkt(&data);
	} else if (strncmp(buf, "sbtm", 4) == 0) {
		struct vtem_sbtm_st sbtm = {
			.sbtm_ver = 0x2,
			.sbtm_mode = 0x3,
			.sbtm_type = 0x1,
			.grdm_min = 0x1,
			.grdm_lum = 2,
			.frmpblimitint = 0xdcba, /* MD2/3 */
		};
		hdmitx_set_sbtm_pkt(&sbtm);
	} else if (strncmp(buf, "hlg", 3) == 0) {
		data.features = 0x00091200;
		hdmitx_set_drm_pkt(&data);
	} else if (strncmp(buf, "vsif", 4) == 0) {
		hdmitx_set_vsif_pkt(buf[4] - '0', buf[5] == '1', NULL, true);
	} else if (strncmp(buf, "emp", 3) == 0) {
		hdmitx_set_emp_pkt(NULL, 1, 1);
	} else if (strncmp(buf, "hdr10+", 6) == 0) {
		hdmitx_set_hdr10plus_pkt(1, &hdr_data);
	} else if (strncmp(buf, "cuva", 4) == 0) {
		hdmitx_set_cuva_hdr_vs_emds(&cuva_data);
	}
	return count;
}

void hdmitx21_ext_set_audio_output(int enable)
{
	hdmitx21_audio_mute_op(enable, AUDIO_MUTE_PATH_1);
	pr_info("%s enable:%d\n", __func__, enable);
}

int hdmitx21_ext_get_audio_status(void)
{
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	return !!hdev->tx_aud_cfg;
}

static ssize_t aud_mute_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int pos = 0;
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	pos += snprintf(buf + pos, PAGE_SIZE, "%d\n",
		atomic_read(&hdev->kref_audio_mute));
	return pos;
}

static ssize_t aud_mute_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct hdmitx_dev *hdev = get_hdmitx21_device();
	atomic_t kref_audio_mute = hdev->kref_audio_mute;

	if (buf[0] == '1') {
		atomic_inc(&kref_audio_mute);
		hdmitx21_audio_mute_op(0, AUDIO_MUTE_PATH_2);
	}
	if (buf[0] == '0') {
		if (!(atomic_sub_and_test(0, &kref_audio_mute)))
			atomic_dec(&kref_audio_mute);
		hdmitx21_audio_mute_op(1, AUDIO_MUTE_PATH_2);
	}
	hdev->kref_audio_mute = kref_audio_mute;

	return count;
}

static ssize_t vid_mute_show(struct device *dev,
			     struct device_attribute *attr,
			     char *buf)
{
	int pos = 0;
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	pos += snprintf(buf + pos, PAGE_SIZE, "%d\n",
		atomic_read(&hdev->kref_video_mute));
	return pos;
}

static ssize_t vid_mute_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
	struct hdmitx_dev *hdev = get_hdmitx21_device();
	atomic_t kref_video_mute = hdev->kref_video_mute;

	if (buf[0] == '1') {
		atomic_inc(&kref_video_mute);
		hdmitx21_video_mute_op(0, VIDEO_MUTE_PATH_1);
	}
	if (buf[0] == '0') {
		if (!(atomic_sub_and_test(0, &kref_video_mute))) {
			atomic_dec(&kref_video_mute);
		}
		hdmitx21_video_mute_op(1, VIDEO_MUTE_PATH_1);
	}
	hdev->kref_video_mute = kref_video_mute;

	return count;
}

static ssize_t debug_store(struct device *dev,
			   struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	hdev->hwop.debugfun(hdev, buf);
	return count;
}

bool is_current_4k_format(void)
{
	int i;
	struct vinfo_s *info = hdmitx_get_current_vinfo(NULL);
	static char const *hdmi4k[] = {
		"2160p",
		"smpte",
		NULL
	};

	if (!info || !info->name)
		return false;

	for (i = 0; hdmi4k[i]; i++) {
		if (strstr(info->name, hdmi4k[i]))
			return true;
	}
	return false;
}

/**/
static ssize_t disp_cap_show(struct device *dev,
			     struct device_attribute *attr,
			     char *buf)
{
	int i, pos = 0;
	struct hdmitx_dev *hdev = get_hdmitx21_device();
	struct rx_cap *prxcap = &hdev->rxcap;
	const struct hdmi_timing *timing = NULL;
	enum hdmi_vic vic;

	for (i = 0; i < prxcap->VIC_count; i++) {
		vic = prxcap->VIC[i];
		if (vic == HDMI_2_720x480p60_4x3 ||
			vic == HDMI_6_720x480i60_4x3 ||
			vic == HDMI_17_720x576p50_4x3 ||
			vic == HDMI_21_720x576i50_4x3) {
			if (hdmitx_check_vic(vic + 1))
				continue;
			timing = hdmitx21_gettiming_from_vic(vic + 1);
		} else {
			timing = hdmitx21_gettiming_from_vic(vic);
		}
		if (timing) {
			pos += snprintf(buf + pos, PAGE_SIZE, "%s",
				timing->sname ? timing->sname : timing->name);
			if (vic == prxcap->native_vic ||
				vic == prxcap->native_vic2)
				pos += snprintf(buf + pos, PAGE_SIZE, "*");
			pos += snprintf(buf + pos, PAGE_SIZE, "\n");
		}
		if (is_vic_support_y420(hdev, vic)) {
			/* backup only for old android */
			/* pos += snprintf(buf + pos, PAGE_SIZE, "%s420\n", */
				/* timing->sname ? timing->sname : timing->name); */
		}
	}

	return pos;
}

static ssize_t preferred_mode_show(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	int pos = 0;
	struct hdmitx_dev *hdev = get_hdmitx21_device();
	struct rx_cap *prxcap = &hdev->rxcap;

	pos += snprintf(buf + pos, PAGE_SIZE, "%s\n",
		hdmitx21_edid_vic_to_string(prxcap->preferred_mode));

	return pos;
}

/* cea_cap, a clone of disp_cap */
static ssize_t cea_cap_show(struct device *dev,
			    struct device_attribute *attr,
			    char *buf)
{
	return disp_cap_show(dev, attr, buf);
}

static ssize_t vesa_cap_show(struct device *dev,
			     struct device_attribute *attr,
			     char *buf)
{
	/* TODO */
	return 0;
}

static void _show_pcm_ch(struct rx_cap *prxcap, int i,
			 int *ppos, char *buf)
{
	const char * const aud_sample_size[] = {"ReferToStreamHeader",
		"16", "20", "24", NULL};
	int j = 0;

	for (j = 0; j < 3; j++) {
		if (prxcap->RxAudioCap[i].cc3 & (1 << j))
			*ppos += snprintf(buf + *ppos, PAGE_SIZE, "%s/",
				aud_sample_size[j + 1]);
	}
	*ppos += snprintf(buf + *ppos - 1, PAGE_SIZE, " bit\n") - 1;
}

/**/
static ssize_t aud_cap_show(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	struct hdmitx_dev *hdev = get_hdmitx21_device();
	int i, pos = 0, j;
	static const char * const aud_ct[] =  {
		"ReferToStreamHeader", "PCM", "AC-3", "MPEG1", "MP3",
		"MPEG2", "AAC", "DTS", "ATRAC",	"OneBitAudio",
		"Dolby_Digital+", "DTS-HD", "MAT", "DST", "WMA_Pro",
		"Reserved", NULL};
	static const char * const aud_sampling_frequency[] = {
		"ReferToStreamHeader", "32", "44.1", "48", "88.2", "96",
		"176.4", "192", NULL};
	struct rx_cap *prxcap = &hdev->rxcap;
	struct dolby_vsadb_cap *cap = &prxcap->dolby_vsadb_cap;

	pos += snprintf(buf + pos, PAGE_SIZE,
		"CodingType MaxChannels SamplingFreq SampleSize\n");
	for (i = 0; i < prxcap->AUD_count; i++) {
		if (prxcap->RxAudioCap[i].audio_format_code == CT_CXT) {
			if ((prxcap->RxAudioCap[i].cc3 >> 3) == 0xb) {
				pos += snprintf(buf + pos, PAGE_SIZE, "MPEG-H, 8ch, ");
				for (j = 0; j < 7; j++) {
					if (prxcap->RxAudioCap[i].freq_cc & (1 << j))
						pos += snprintf(buf + pos, PAGE_SIZE, "%s/",
							aud_sampling_frequency[j + 1]);
				}
				pos += snprintf(buf + pos - 1, PAGE_SIZE, " kHz\n");
			}
			continue;
		}
		pos += snprintf(buf + pos, PAGE_SIZE, "%s",
			aud_ct[prxcap->RxAudioCap[i].audio_format_code]);
		/* todo: cc3 & 3 */
		if (prxcap->RxAudioCap[i].audio_format_code == CT_DD_P &&
		    (prxcap->RxAudioCap[i].cc3 & 1))
			pos += snprintf(buf + pos, PAGE_SIZE, "/ATMOS");
		if (prxcap->RxAudioCap[i].audio_format_code != CT_CXT)
			pos += snprintf(buf + pos, PAGE_SIZE, ", %d ch, ",
				prxcap->RxAudioCap[i].channel_num_max + 1);
		for (j = 0; j < 7; j++) {
			if (prxcap->RxAudioCap[i].freq_cc & (1 << j))
				pos += snprintf(buf + pos, PAGE_SIZE, "%s/",
					aud_sampling_frequency[j + 1]);
		}
		pos += snprintf(buf + pos - 1, PAGE_SIZE, " kHz, ") - 1;
		switch (prxcap->RxAudioCap[i].audio_format_code) {
		case CT_PCM:
			_show_pcm_ch(prxcap, i, &pos, buf);
			break;
		case CT_AC_3:
		case CT_MPEG1:
		case CT_MP3:
		case CT_MPEG2:
		case CT_AAC:
		case CT_DTS:
		case CT_ATRAC:
		case CT_ONE_BIT_AUDIO:
			pos += snprintf(buf + pos, PAGE_SIZE,
				"MaxBitRate %dkHz\n",
				prxcap->RxAudioCap[i].cc3 * 8);
			break;
		case CT_DD_P:
		case CT_DTS_HD:
		case CT_MAT:
		case CT_DST:
			pos += snprintf(buf + pos, PAGE_SIZE, "DepValue 0x%x\n",
				prxcap->RxAudioCap[i].cc3);
			break;
		case CT_WMA:
		default:
			break;
		}
	}

	if (cap->ieeeoui == DOVI_IEEEOUI) {
		/*
		 *Dolby Vendor Specific:
		 *  headphone_playback_only:0,
		 *  center_speaker:1,
		 *  surround_speaker:1,
		 *  height_speaker:1,
		 *  Ver:1.0,
		 *  MAT_PCM_48kHz_only:1,
		 *  e61146d0007001,
		 */
		pos += snprintf(buf + pos, PAGE_SIZE,
				"Dolby Vendor Specific:\n");
		if (cap->dolby_vsadb_ver == 0)
			pos += snprintf(buf + pos, PAGE_SIZE, "  Ver:1.0,\n");
		else
			pos += snprintf(buf + pos, PAGE_SIZE,
				"  Ver:Reversed,\n");
		pos += snprintf(buf + pos, PAGE_SIZE,
			"  center_speaker:%d,\n", cap->spk_center);
		pos += snprintf(buf + pos, PAGE_SIZE,
			"  surround_speaker:%d,\n", cap->spk_surround);
		pos += snprintf(buf + pos, PAGE_SIZE,
			"  height_speaker:%d,\n", cap->spk_height);
		pos += snprintf(buf + pos, PAGE_SIZE,
			"  headphone_playback_only:%d,\n", cap->headphone_only);
		pos += snprintf(buf + pos, PAGE_SIZE,
			"  MAT_PCM_48kHz_only:%d,\n", cap->mat_48k_pcm_only);

		pos += snprintf(buf + pos, PAGE_SIZE, "  ");
		for (i = 0; i < 7; i++)
			pos += snprintf(buf + pos, PAGE_SIZE, "%02x",
				cap->rawdata[i]);
		pos += snprintf(buf + pos, PAGE_SIZE, ",\n");
	}
	return pos;
}

/**/
static ssize_t lipsync_cap_show(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	int pos = 0;
	struct hdmitx_dev *hdev = get_hdmitx21_device();
	struct rx_cap *prxcap = &hdev->rxcap;

	pos += snprintf(buf + pos, PAGE_SIZE, "Lipsync(in ms)\n");
	pos += snprintf(buf + pos, PAGE_SIZE, "%d, %d\n",
				prxcap->vLatency, prxcap->aLatency);
	return pos;
}

/**/
static ssize_t hdmi_hdr_status_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	int pos = 0;
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	/* pos = 4 */
	if (hdr_status_pos == 4) {
		pos += snprintf(buf + pos, PAGE_SIZE, "HDR10-GAMMA_CUVA");
		return pos;
	}

	/* pos = 3 */
	if (hdr_status_pos == 3 || hdev->hdr10plus_feature) {
		pos += snprintf(buf + pos, PAGE_SIZE, "HDR10Plus-VSIF");
		return pos;
	}

	/* pos = 2 */
	if (hdr_status_pos == 2) {
		if (hdev->hdmi_current_eotf_type == EOTF_T_DOLBYVISION) {
			pos += snprintf(buf + pos, PAGE_SIZE,
				"DolbyVision-Std");
			return pos;
		}
		if (hdev->hdmi_current_eotf_type == EOTF_T_LL_MODE) {
			pos += snprintf(buf + pos, PAGE_SIZE,
				"DolbyVision-Lowlatency");
			return pos;
		}
	}

	/* pos = 1 */
	if (hdr_status_pos == 1) {
		if (hdev->hdr_transfer_feature == T_SMPTE_ST_2084) {
			if (hdev->hdr_color_feature == C_BT2020) {
				pos += snprintf(buf + pos, PAGE_SIZE,
					"HDR10-GAMMA_ST2084");
				return pos;
			}
			pos += snprintf(buf + pos, PAGE_SIZE, "HDR10-others");
			return pos;
		}
		if (hdev->hdr_color_feature == C_BT2020 &&
		    (hdev->hdr_transfer_feature == T_BT2020_10 ||
		     hdev->hdr_transfer_feature == T_HLG)) {
			pos += snprintf(buf + pos, PAGE_SIZE,
				"HDR10-GAMMA_HLG");
			return pos;
		}
	}

	/* default is SDR */
	pos += snprintf(buf + pos, PAGE_SIZE, "SDR");

	return pos;
}

static int hdmi_hdr_status_to_drm(void)
{
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	/* pos = 3 */
	if (hdr_status_pos == 3 || hdev->hdr10plus_feature)
		return HDR10PLUS_VSIF;

	/* pos = 2 */
	if (hdr_status_pos == 2) {
		if (hdev->hdmi_current_eotf_type == EOTF_T_DOLBYVISION)
			return dolbyvision_std;

		if (hdev->hdmi_current_eotf_type == EOTF_T_LL_MODE)
			return dolbyvision_lowlatency;
	}

	/* pos = 1 */
	if (hdr_status_pos == 1) {
		if (hdev->hdr_transfer_feature == T_SMPTE_ST_2084) {
			if (hdev->hdr_color_feature == C_BT2020)
				return HDR10_GAMMA_ST2084;
			else
				return HDR10_others;
		}
		if (hdev->hdr_color_feature == C_BT2020 &&
		    (hdev->hdr_transfer_feature == T_BT2020_10 ||
		     hdev->hdr_transfer_feature == T_HLG))
			return HDR10_GAMMA_HLG;
	}

	/* default is SDR */
	return SDR;
}

/**/
static ssize_t dc_cap_show(struct device *dev,
			   struct device_attribute *attr, char *buf)
{
	int i;
	int pos = 0;
	struct hdmitx_dev *hdev = get_hdmitx21_device();
	struct rx_cap *prxcap = &hdev->rxcap;
	const struct dv_info *dv = &hdev->rxcap.dv_info;
	const struct dv_info *dv2 = &hdev->rxcap.dv_info2;

	if (prxcap->dc_36bit_420)
		pos += snprintf(buf + pos, PAGE_SIZE, "420,12bit\n");
	if (prxcap->dc_30bit_420)
		pos += snprintf(buf + pos, PAGE_SIZE, "420,10bit\n");

	for (i = 0; i < Y420_VIC_MAX_NUM; i++) {
		if (prxcap->y420_vic[i]) {
			pos += snprintf(buf + pos, PAGE_SIZE,
				"420,8bit\n");
			break;
		}
	}
	if (prxcap->native_Mode & (1 << 5)) {
		if (prxcap->dc_y444) {
			if (prxcap->dc_36bit || dv->sup_10b_12b_444 == 0x2 ||
			    dv2->sup_10b_12b_444 == 0x2)
				if (!hdev->vend_id_hit)
					pos += snprintf(buf + pos, PAGE_SIZE, "444,12bit\n");
			if (prxcap->dc_30bit || dv->sup_10b_12b_444 == 0x1 ||
			    dv2->sup_10b_12b_444 == 0x1) {
				if (!hdev->vend_id_hit)
					pos += snprintf(buf + pos, PAGE_SIZE, "444,10bit\n");
			}
		}
		pos += snprintf(buf + pos, PAGE_SIZE, "444,8bit\n");
	}
	/* y422, not check dc */
	if (prxcap->native_Mode & (1 << 4)) {
		pos += snprintf(buf + pos, PAGE_SIZE, "422,12bit\n");
		pos += snprintf(buf + pos, PAGE_SIZE, "422,10bit\n");
		pos += snprintf(buf + pos, PAGE_SIZE, "422,8bit\n");
	}
//nextrgb:
	if (prxcap->dc_36bit || dv->sup_10b_12b_444 == 0x2 ||
	    dv2->sup_10b_12b_444 == 0x2)
		if (!hdev->vend_id_hit)
			pos += snprintf(buf + pos, PAGE_SIZE, "rgb,12bit\n");
	if (prxcap->dc_30bit || dv->sup_10b_12b_444 == 0x1 ||
	    dv2->sup_10b_12b_444 == 0x1)
		if (!hdev->vend_id_hit)
			pos += snprintf(buf + pos, PAGE_SIZE, "rgb,10bit\n");
	pos += snprintf(buf + pos, PAGE_SIZE, "rgb,8bit\n");
	return pos;
}

static bool pre_process_str(char *name)
{
	int i;
	u32 flag = 0;
	char *color_format[4] = {"444", "422", "420", "rgb"};

	for (i = 0; i < 4 ; i++) {
		if (strstr(name, color_format[i]))
			flag++;
	}
	if (flag >= 2)
		return 0;
	else
		return 1;
}

static ssize_t valid_mode_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int ret;
	bool valid_mode;
	char cvalid_mode[32];
	struct hdmitx_dev *hdev = get_hdmitx21_device();
	struct hdmi_format_para *para = NULL;

	mutex_lock(&valid_mode_mutex);
	memset(cvalid_mode, 0, sizeof(cvalid_mode));
	strncpy(cvalid_mode, buf, sizeof(cvalid_mode));
	cvalid_mode[31] = '\0';

	if (cvalid_mode[0]) {
		valid_mode = pre_process_str(cvalid_mode);
		if (valid_mode == 0) {
			mutex_unlock(&valid_mode_mutex);
			return -1;
		}
		para = hdmitx21_tst_fmt_name(cvalid_mode, cvalid_mode);
		if (!para) {
			mutex_unlock(&valid_mode_mutex);
			return -1;
		}
	}
	valid_mode = hdmitx21_edid_check_valid_mode(hdev, para);
	ret = valid_mode ? count : -1;
	mutex_unlock(&valid_mode_mutex);
	if (log21_level)
		pr_info("hdmitx: valid_mode_show %s  valid: %d\n", cvalid_mode, ret);
	return ret;
}

static ssize_t allm_cap_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	int pos = 0;
	struct hdmitx_dev *hdev = get_hdmitx21_device();
	struct rx_cap *prxcap = &hdev->rxcap;

	pos += snprintf(buf + pos, PAGE_SIZE, "%d\n\r", prxcap->allm);
	return pos;
}

static ssize_t allm_mode_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	int pos = 0;
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	pos += snprintf(buf + pos, PAGE_SIZE, "%d\n\r", hdev->allm_mode);

	return pos;
}

static inline int com_str(const char *buf, const char *str)
{
	return strncmp(buf, str, strlen(str)) == 0;
}

static ssize_t allm_mode_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf,
			       size_t count)
{
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	pr_info("hdmitx: store allm_mode as %s\n", buf);

	if (com_str(buf, "0")) {
		// disable ALLM
		hdev->allm_mode = 0;
		hdmitx21_construct_vsif(hdev, VT_ALLM, 0, NULL);
		if (_is_hdmi14_4k(hdev->cur_VIC) &&
			!hdmitx21_dv_en() &&
			!hdmitx21_hdr10p_en())
			hdmitx21_construct_vsif(hdev, VT_HDMI14_4K, 1, NULL);
	}
	if (com_str(buf, "1")) {
		hdev->allm_mode = 1;
		hdmitx21_construct_vsif(hdev, VT_ALLM, 1, NULL);
		hdev->ct_mode = 0;
		hdev->hwop.cntlconfig(hdev, CONF_CT_MODE, SET_CT_OFF);
	}
	if (com_str(buf, "-1")) {
		if (hdev->allm_mode == 1) {
			hdev->allm_mode = 0;
			hdmi_vend_infoframe2_rawset(NULL, NULL);
		}
	}
	return count;
}

static ssize_t contenttype_cap_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	int pos = 0;
	struct hdmitx_dev *hdev = get_hdmitx21_device();
	struct rx_cap *prxcap = &hdev->rxcap;

	if (prxcap->cnc0)
		pos += snprintf(buf + pos, PAGE_SIZE, "graphics\n\r");
	if (prxcap->cnc1)
		pos += snprintf(buf + pos, PAGE_SIZE, "photo\n\r");
	if (prxcap->cnc2)
		pos += snprintf(buf + pos, PAGE_SIZE, "cinema\n\r");
	if (prxcap->cnc3)
		pos += snprintf(buf + pos, PAGE_SIZE, "game\n\r");

	return pos;
}

static ssize_t contenttype_mode_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	int pos = 0;
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	if (hdev->ct_mode == 0)
		pos += snprintf(buf + pos, PAGE_SIZE, "off\n\r");
	if (hdev->ct_mode == 1)
		pos += snprintf(buf + pos, PAGE_SIZE, "game\n\r");
	if (hdev->ct_mode == 2)
		pos += snprintf(buf + pos, PAGE_SIZE, "graphics\n\r");
	if (hdev->ct_mode == 3)
		pos += snprintf(buf + pos, PAGE_SIZE, "photo\n\r");
	if (hdev->ct_mode == 4)
		pos += snprintf(buf + pos, PAGE_SIZE, "cinema\n\r");

	return pos;
}

static ssize_t contenttype_mode_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	pr_info("hdmitx: store contenttype_mode as %s\n", buf);

	if (hdev->allm_mode == 1) {
		hdev->allm_mode = 0;
		hdmitx21_construct_vsif(hdev, VT_ALLM, 0, NULL);
	}
	if (_is_hdmi14_4k(hdev->cur_VIC) &&
		!hdmitx21_dv_en() &&
		!hdmitx21_hdr10p_en())
		hdmitx21_construct_vsif(hdev, VT_HDMI14_4K, 1, NULL);
	if (com_str(buf, "0") || com_str(buf, "off")) {
		hdev->ct_mode = 0;
		hdev->it_content = 0;
		hdev->hwop.cntlconfig(hdev, CONF_CT_MODE, SET_CT_OFF);
	}
	if (com_str(buf, "1") || com_str(buf, "game")) {
		hdev->ct_mode = 1;
		hdev->it_content = 1;
		hdev->hwop.cntlconfig(hdev, CONF_CT_MODE,
			SET_CT_GAME | IT_CONTENT << 4);
	}
	if (com_str(buf, "2") || com_str(buf, "graphics")) {
		hdev->ct_mode = 2;
		hdev->it_content = 1;
		hdev->hwop.cntlconfig(hdev, CONF_CT_MODE,
			SET_CT_GRAPHICS | IT_CONTENT << 4);
	}
	if (com_str(buf, "3") || com_str(buf, "photo")) {
		hdev->ct_mode = 3;
		hdev->it_content = 1;
		hdev->hwop.cntlconfig(hdev, CONF_CT_MODE,
			SET_CT_PHOTO | IT_CONTENT << 4);
	}
	if (com_str(buf, "4") || com_str(buf, "cinema")) {
		hdev->ct_mode = 4;
		hdev->it_content = 1;
		hdev->hwop.cntlconfig(hdev, CONF_CT_MODE,
			SET_CT_CINEMA | IT_CONTENT << 4);
	}

	return count;
}

static void hdmi_tx_enable_ll_mode(bool enable)
{
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	if (enable) {
		if (hdev->rxcap.allm) {
			/* dolby vision CTS case89 requirement: if IFDB with no
			 * additional VSIF support in EDID, then only
			 * send DV-VSIF, not send HF-VSIF
			 */
			if (hdmitx21_dv_en() &&
				(hdev->rxcap.ifdb_present && hdev->rxcap.additional_vsif_num < 1)) {
				pr_info("%s: can't send HF-VSIF, ifdb_present: %d, additional_vsif_num: %d\n",
					__func__, hdev->rxcap.ifdb_present,
					hdev->rxcap.additional_vsif_num);
				return;
			}
			hdev->allm_mode = 1;
			pr_info("%s: enabling ALLM, enable:%d, allm:%d, cnc3:%d\n",
				__func__, enable, hdev->rxcap.allm, hdev->rxcap.cnc3);
			hdmitx21_construct_vsif(hdev, VT_ALLM, 1, NULL);
			hdev->ct_mode = 0;
			hdev->it_content = 0;
			hdev->hwop.cntlconfig(hdev, CONF_CT_MODE, SET_CT_OFF);
		} else if (hdev->rxcap.cnc3) {
		    /* disable ALLM first if enabled*/
			if (hdev->allm_mode == 1) {
				hdev->allm_mode = 0;
				pr_info("%s: disabling ALLM before enabling game mode, enable:%d, allm:%d, cnc3:%d\n",
					__func__, enable, hdev->rxcap.allm, hdev->rxcap.cnc3);
				hdmitx21_construct_vsif(hdev, VT_ALLM, 0, NULL);
				if (_is_hdmi14_4k(hdev->cur_VIC))
					hdmitx21_construct_vsif(hdev, VT_HDMI14_4K, 1, NULL);
				/* if not hdmi1.4 4k, need to sent > 4 frames and shorter than 1S
				 * HF-VSIF with allm_mode = 0, and then disable HF-VSIF according
				 * 10.2.1 HF-VSIF Transitions in hdmi2.1a. TODO:
				 */
			}
			hdev->ct_mode = 1;
			hdev->it_content = 1;
			pr_info("%s: enabling GAME Mode, enable:%d, allm:%d, cnc3:%d\n",
				__func__, enable, hdev->rxcap.allm, hdev->rxcap.cnc3);
			hdev->hwop.cntlconfig(hdev, CONF_CT_MODE,
				SET_CT_GAME | IT_CONTENT << 4);
		} else {
			/* for safety, clear ALLM/HDMI1.X GAME if enabled */
			/* disable ALLM */
			if (hdev->allm_mode == 1) {
				hdev->allm_mode = 0;
				pr_info("%s: disabling ALLM, enable:%d, allm:%d, cnc3:%d\n",
					__func__, enable, hdev->rxcap.allm, hdev->rxcap.cnc3);
				hdmitx21_construct_vsif(hdev, VT_ALLM, 0, NULL);
				if (_is_hdmi14_4k(hdev->cur_VIC) &&
					!hdmitx21_dv_en() &&
					!hdmitx21_hdr10p_en())
					hdmitx21_construct_vsif(hdev, VT_HDMI14_4K, 1, NULL);
			}
			/* clear content type */
			if (hdev->ct_mode == 1) {
				hdev->ct_mode = 0;
				hdev->it_content = 0;
				pr_info("%s: disabling GAME Mode disabled, enable:%d, allm:%d, cnc3:%d\n",
					__func__, enable, hdev->rxcap.allm, hdev->rxcap.cnc3);
				hdev->hwop.cntlconfig(hdev, CONF_CT_MODE, SET_CT_OFF);
			}
		}
	} else {
		/* disable ALLM */
		if (hdev->allm_mode == 1) {
			hdev->allm_mode = 0;
			pr_info("%s: disabling ALLM, enable:%d, allm:%d, cnc3:%d\n",
				__func__, enable, hdev->rxcap.allm, hdev->rxcap.cnc3);
			hdmitx21_construct_vsif(hdev, VT_ALLM, 0, NULL);
			if (_is_hdmi14_4k(hdev->cur_VIC) &&
				!hdmitx21_dv_en() &&
				!hdmitx21_hdr10p_en())
				hdmitx21_construct_vsif(hdev, VT_HDMI14_4K, 1, NULL);
		}
		/* clear content type */
		if (hdev->ct_mode == 1) {
			hdev->ct_mode = 0;
			hdev->it_content = 0;
			pr_info("%s: disabling GAME Mode disabled, enable:%d, allm:%d, cnc3:%d\n",
				__func__, enable, hdev->rxcap.allm, hdev->rxcap.cnc3);
			hdev->hwop.cntlconfig(hdev, CONF_CT_MODE, SET_CT_OFF);
		}
	}
}

/* for decoder/hwc or sysctl to control the low latency mode,
 * as they don't care if sink support ALLM OR HDMI1.X game mode
 * so need hdmitx driver to device to send ALLM OR HDMI1.X game
 * mode according to capability of EDID
 */
static ssize_t ll_mode_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	int pos = 0;
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	if (hdev->rxcap.allm) {
		if (hdev->allm_mode == 1)
			pos += snprintf(buf + pos, PAGE_SIZE, "HDMI2.1_ALLM_ENABLED\n\r");
		else
			pos += snprintf(buf + pos, PAGE_SIZE, "HDMI2.1_ALLM_DISABLED\n\r");
	}
	if (hdev->rxcap.cnc3) {
		if (hdev->ct_mode == 1)
			pos += snprintf(buf + pos, PAGE_SIZE, "HDMI1.x_GAME_MODE_ENABLED\n\r");
		else
			pos += snprintf(buf + pos, PAGE_SIZE, "HDMI1.x_GAME_MODE_DISABLED\n\r");
	}

	if (!hdev->rxcap.allm && !hdev->rxcap.cnc3)
		pos += snprintf(buf + pos, PAGE_SIZE, "HDMI_LATENCY_MODE_UNKNOWN\n\r");
	return pos;
}

/* 1.echo 1 to enable ALLM OR HDMI1.X game mode
 * if sink support ALLM, then output ALLM mode;
 * else if support HDMI1.X game mode, then output
 * HDMI1.X game mode; else, do nothing
 * 2.echo 0 to disable ALLM and HDMI1.X game mode
 */
static ssize_t ll_mode_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf,
			       size_t count)
{
	struct hdmitx_dev *hdev = get_hdmitx21_device();
	hdev->ll_enabled_in_auto_mode = com_str(buf, "1");

	pr_info("hdmitx: store ll_enabled_in_auto_mode: %d, ll_user_set_mode:%d\n",
		hdev->ll_enabled_in_auto_mode, hdev->ll_user_set_mode);

	if (hdev->ll_user_set_mode == HDMI_LL_MODE_AUTO) {
		pr_info("hdmitx: store ll_mode as %s, calling hdmi_tx_enable_ll_mode()\n", buf);
		hdmi_tx_enable_ll_mode(hdev->ll_enabled_in_auto_mode);
	} else {
		pr_info("hdmitx: ll mode is forced on/off: %d\n", hdev->ll_user_set_mode);
	}

	return count;
}

/* for user to force enable/disable low-latency modes
 */
static ssize_t ll_user_mode_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	int pos = 0;
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	switch (hdev->ll_user_set_mode) {
	case HDMI_LL_MODE_ENABLE:
		pos += snprintf(buf + pos, PAGE_SIZE, "HDMI_LL_MODE_ENABLE\n\r");
		break;
	case HDMI_LL_MODE_DISABLE:
		pos += snprintf(buf + pos, PAGE_SIZE, "HDMI_LL_MODE_DISABLE\n\r");
		break;
	case HDMI_LL_MODE_AUTO:
	default:
		pos += snprintf(buf + pos, PAGE_SIZE, "HDMI_LL_MODE_AUTO\n\r");
		break;
	}
	return pos;
}

/* 1.echo enable to enable ALLM OR HDMI1.X game mode
 * if sink support ALLM, then output ALLM mode;
 * else if support HDMI1.X game mode, then output
 * HDMI1.X game mode; else, do nothing
 * 2.echo disable to disable ALLM and HDMI1.X game mode
 * 3.echo auto to enable/disable low-latency mode per
 * content type
 */
static ssize_t ll_user_mode_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf,
			       size_t count)
{
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	pr_info("hdmitx: store ll_user_set_mode as %s\n", buf);
	if (com_str(buf, "enable")) {
		hdev->ll_user_set_mode = HDMI_LL_MODE_ENABLE;
		hdmi_tx_enable_ll_mode(true);
	} else if (com_str(buf, "disable")) {
		hdev->ll_user_set_mode = HDMI_LL_MODE_DISABLE;
		hdmi_tx_enable_ll_mode(false);
	} else {
		hdev->ll_user_set_mode = HDMI_LL_MODE_AUTO;
		hdmi_tx_enable_ll_mode(hdev->ll_enabled_in_auto_mode);
	}
	return count;
}

/* for game console-> hdmirx -> hdmitx -> TV
 * interface for hdmirx module
 * ret: false if not update, true if updated
 */
bool hdmitx_update_latency_info(struct tvin_latency_s *latency_info)
{
	struct hdmitx_dev *hdev = get_hdmitx21_device();
	bool it_content = false;
	/* when switch between hdmirx source(ALLM) and hdmitx home(non-ALLM),
	 * the ALLM/1.4 Game will change, need to mute before change
	 */
	bool video_mute = false;

	if (!hdmi_allm_passthough_en)
		return false;
	if (!latency_info)
		return false;

	pr_info("%s: ll_enabled_in_auto_mode: %d, ll_user_set_mode:%d\n",
		__func__, hdev->ll_enabled_in_auto_mode, hdev->ll_user_set_mode);

	if (hdev->ll_user_set_mode != HDMI_LL_MODE_AUTO) {
		pr_info("%s: non-auto mode, return, allm_mode: %d, it_content: %d, cn_type: %d\n",
			__func__,
			latency_info->allm_mode,
			latency_info->it_content,
			latency_info->cn_type);
		return false;
	}
	pr_info("%s: allm_mode: %d, it_content: %d, cn_type: %d\n",
		__func__, latency_info->allm_mode, latency_info->it_content, latency_info->cn_type);
	if (hdev->allm_mode == latency_info->allm_mode &&
		hdev->it_content == latency_info->it_content &&
		hdev->ct_mode == latency_info->cn_type) {
		pr_info("latency_info not changed, exit\n");
		return false;
	}
	/* refer to allm_mode_store() */
	if (latency_info->allm_mode) {
		if (hdev->rxcap.allm) {
			//if (hdmitx21_dv_en() &&
				//(hdev->rxcap.ifdb_present &&
				//hdev->rxcap.additional_vsif_num < 1)) {
				//pr_info("%s: DV enabled, but ifdb_present: %d,
				//additional_vsif_num: %d\n",
				//__func__, hdev->rxcap.ifdb_present,
				//hdev->rxcap.additional_vsif_num);
				//return false;
			//}
			if (!get_rx_active_sts()) {
				video_mute = true;
				//hdmitx21_video_mute_op(0, VIDEO_MUTE_PATH_4);
				hdev->hwop.cntlconfig(hdev, CONF_VIDEO_MUTE_OP, VIDEO_MUTE);
			}
			hdev->allm_mode = 1;
			pr_info("%s: enabling ALLM\n", __func__);
			hdmitx21_construct_vsif(hdev, VT_ALLM, 1, NULL);
			hdev->ct_mode = 0;
			hdev->it_content = 0;
			hdev->hwop.cntlconfig(hdev, CONF_CT_MODE, SET_CT_OFF);
		}
	} else {
		if (!get_rx_active_sts()) {
			video_mute = true;
			//hdmitx21_video_mute_op(0, VIDEO_MUTE_PATH_4);
			hdev->hwop.cntlconfig(hdev, CONF_VIDEO_MUTE_OP, VIDEO_MUTE);
		}
		/* disable ALLM firstly */
		if (hdev->allm_mode == 1) {
			hdev->allm_mode = 0;
			pr_info("%s: disabling ALLM before enable/disable game mode\n", __func__);
			hdmitx21_construct_vsif(hdev, VT_ALLM, 0, NULL);
			if (_is_hdmi14_4k(hdev->cur_VIC) &&
				!hdmitx21_dv_en() &&
				!hdmitx21_hdr10p_en())
				hdmitx21_construct_vsif(hdev, VT_HDMI14_4K, 1, NULL);
		}
		hdev->it_content = latency_info->it_content;
		it_content = hdev->it_content;
		if (hdev->rxcap.cnc3 && latency_info->cn_type == GAME) {
			hdev->ct_mode = 1;
			pr_info("%s: enabling GAME mode\n", __func__);
			hdev->hwop.cntlconfig(hdev, CONF_CT_MODE,
				SET_CT_GAME | it_content << 4);
		} else if (hdev->rxcap.cnc0 && latency_info->cn_type == GRAPHICS &&
		    latency_info->it_content == 1) {
			hdev->ct_mode = 2;
			pr_info("%s: enabling GRAPHICS mode\n", __func__);
			hdev->hwop.cntlconfig(hdev, CONF_CT_MODE,
				SET_CT_GRAPHICS | it_content << 4);
		} else if (hdev->rxcap.cnc1 && latency_info->cn_type == PHOTO) {
			hdev->ct_mode = 3;
			pr_info("%s: enabling PHOTO mode\n", __func__);
			hdev->hwop.cntlconfig(hdev, CONF_CT_MODE,
				SET_CT_PHOTO | it_content << 4);
		} else if (hdev->rxcap.cnc2 && latency_info->cn_type == CINEMA) {
			hdev->ct_mode = 4;
			pr_info("%s: enabling CINEMA mode\n", __func__);
			hdev->hwop.cntlconfig(hdev, CONF_CT_MODE,
				SET_CT_CINEMA | it_content << 4);
		} else {
			hdev->ct_mode = 0;
			pr_info("%s: No GAME or CT mode\n", __func__);
			hdev->hwop.cntlconfig(hdev, CONF_CT_MODE,
				SET_CT_OFF | it_content << 4);
		}
	}
	return true;
}
EXPORT_SYMBOL(hdmitx_update_latency_info);

/**/
static ssize_t _hdr_cap_show(struct device *dev,
			     struct device_attribute *attr,
			     char *buf,
			     const struct hdr_info *hdr)
{
	int pos = 0;
	unsigned int i, j;
	int hdr10plugsupported = 0;
	const struct cuva_info *cuva = &hdr->cuva_info;
	const struct hdr10_plus_info *hdr10p = &hdr->hdr10plus_info;
	const struct sbtm_info *sbtm = &hdr->sbtm_info;

	if (hdr10p->ieeeoui == HDR10_PLUS_IEEE_OUI &&
		hdr10p->application_version != 0xFF)
		hdr10plugsupported = 1;
	pos += snprintf(buf + pos, PAGE_SIZE, "HDR10Plus Supported: %d\n",
		hdr10plugsupported);
	pos += snprintf(buf + pos, PAGE_SIZE, "HDR Static Metadata:\n");
	pos += snprintf(buf + pos, PAGE_SIZE, "    Supported EOTF:\n");
	pos += snprintf(buf + pos, PAGE_SIZE, "        Traditional SDR: %d\n",
		!!(hdr->hdr_support & 0x1));
	pos += snprintf(buf + pos, PAGE_SIZE, "        Traditional HDR: %d\n",
		!!(hdr->hdr_support & 0x2));
	pos += snprintf(buf + pos, PAGE_SIZE, "        SMPTE ST 2084: %d\n",
		!!(hdr->hdr_support & 0x4));
	pos += snprintf(buf + pos, PAGE_SIZE, "        Hybrid Log-Gamma: %d\n",
		!!(hdr->hdr_support & 0x8));
	pos += snprintf(buf + pos, PAGE_SIZE, "    Supported SMD type1: %d\n",
		hdr->static_metadata_type1);
	pos += snprintf(buf + pos, PAGE_SIZE, "    Luminance Data\n");
	pos += snprintf(buf + pos, PAGE_SIZE, "        Max: %d\n",
		hdr->lumi_max);
	pos += snprintf(buf + pos, PAGE_SIZE, "        Avg: %d\n",
		hdr->lumi_avg);
	pos += snprintf(buf + pos, PAGE_SIZE, "        Min: %d\n\n",
		hdr->lumi_min);
	pos += snprintf(buf + pos, PAGE_SIZE, "HDR Dynamic Metadata:");

	for (i = 0; i < 4; i++) {
		if (hdr->dynamic_info[i].type == 0)
			continue;
		pos += snprintf(buf + pos, PAGE_SIZE,
			"\n    metadata_version: %x\n",
			hdr->dynamic_info[i].type);
		pos += snprintf(buf + pos, PAGE_SIZE,
			"        support_flags: %x\n",
			hdr->dynamic_info[i].support_flags);
		pos += snprintf(buf + pos, PAGE_SIZE,
			"        optional_fields:");
		for (j = 0; j <
			(hdr->dynamic_info[i].of_len - 3); j++)
			pos += snprintf(buf + pos, PAGE_SIZE, " %x",
				hdr->dynamic_info[i].optional_fields[j]);
	}

	pos += snprintf(buf + pos, PAGE_SIZE, "\n\ncolorimetry_data: %x\n",
		hdr->colorimetry_support);
	if (cuva->ieeeoui == CUVA_IEEEOUI) {
		pos += snprintf(buf + pos, PAGE_SIZE, "CUVA supported: 1\n");
		pos += snprintf(buf + pos, PAGE_SIZE,
			"  system_start_code: %u\n", cuva->system_start_code);
		pos += snprintf(buf + pos, PAGE_SIZE,
			"  version_code: %u\n", cuva->version_code);
		pos += snprintf(buf + pos, PAGE_SIZE,
			"  display_maximum_luminance: %u\n",
			cuva->display_max_lum);
		pos += snprintf(buf + pos, PAGE_SIZE,
			"  display_minimum_luminance: %u\n",
			cuva->display_min_lum);
		pos += snprintf(buf + pos, PAGE_SIZE,
			"  monitor_mode_support: %u\n", cuva->monitor_mode_sup);
		pos += snprintf(buf + pos, PAGE_SIZE,
			"  rx_mode_support: %u\n", cuva->rx_mode_sup);
		for (i = 0; i < (cuva->length + 1); i++)
			pos += snprintf(buf + pos, PAGE_SIZE, "%02x",
				cuva->rawdata[i]);
		pos += snprintf(buf + pos, PAGE_SIZE, "\n");
	}
	/* sbtm capability show */
	if (sbtm->sbtm_support) {
		pos += snprintf(buf + pos, PAGE_SIZE, "SBTM supported: 1\n");
		if (sbtm->max_sbtm_ver)
			pos += snprintf(buf + pos, PAGE_SIZE, "  Max_SBTM_Ver: 0x%x\n",
				sbtm->max_sbtm_ver);
		if (sbtm->grdm_support)
			pos += snprintf(buf + pos, PAGE_SIZE, "  grdm_support: 0x%x\n",
				sbtm->grdm_support);
		if (sbtm->drdm_ind)
			pos += snprintf(buf + pos, PAGE_SIZE, "  drdm_ind: 0x%x\n",
				sbtm->drdm_ind);
		if (sbtm->hgig_cat_drdm_sel)
			pos += snprintf(buf + pos, PAGE_SIZE, "  hgig_cat_drdm_sel: 0x%x\n",
				sbtm->hgig_cat_drdm_sel);
		if (sbtm->use_hgig_drdm)
			pos += snprintf(buf + pos, PAGE_SIZE, "  use_hgig_drdm: 0x%x\n",
				sbtm->use_hgig_drdm);
		if (sbtm->maxrgb)
			pos += snprintf(buf + pos, PAGE_SIZE, "  maxrgb: 0x%x\n",
				sbtm->maxrgb);
		if (sbtm->gamut)
			pos += snprintf(buf + pos, PAGE_SIZE, "  gamut: 0x%x\n",
				sbtm->gamut);
		if (sbtm->red_x)
			pos += snprintf(buf + pos, PAGE_SIZE, "  red_x: 0x%x\n",
				sbtm->red_x);
		if (sbtm->red_y)
			pos += snprintf(buf + pos, PAGE_SIZE, "  red_y: 0x%x\n",
				sbtm->red_y);
		if (sbtm->green_x)
			pos += snprintf(buf + pos, PAGE_SIZE, "  green_x: 0x%x\n",
				sbtm->green_x);
		if (sbtm->green_y)
			pos += snprintf(buf + pos, PAGE_SIZE, "  green_y: 0x%x\n",
				sbtm->green_y);
		if (sbtm->blue_x)
			pos += snprintf(buf + pos, PAGE_SIZE, "  blue_x: 0x%x\n",
				sbtm->blue_x);
		if (sbtm->blue_y)
			pos += snprintf(buf + pos, PAGE_SIZE, "  blue_y: 0x%x\n",
				sbtm->blue_y);
		if (sbtm->white_x)
			pos += snprintf(buf + pos, PAGE_SIZE, "  white_x: 0x%x\n",
				sbtm->white_x);
		if (sbtm->white_y)
			pos += snprintf(buf + pos, PAGE_SIZE, "  white_y: 0x%x\n",
				sbtm->white_y);
		if (sbtm->min_bright_10)
			pos += snprintf(buf + pos, PAGE_SIZE, "  min_bright_10: 0x%x\n",
				sbtm->min_bright_10);
		if (sbtm->peak_bright_100)
			pos += snprintf(buf + pos, PAGE_SIZE, "  peak_bright_100: 0x%x\n",
				sbtm->peak_bright_100);
		if (sbtm->p0_exp)
			pos += snprintf(buf + pos, PAGE_SIZE, "  p0_exp: 0x%x\n",
				sbtm->p0_exp);
		if (sbtm->p0_mant)
			pos += snprintf(buf + pos, PAGE_SIZE, "  p0_mant: 0x%x\n",
				sbtm->p0_mant);
		if (sbtm->peak_bright_p0)
			pos += snprintf(buf + pos, PAGE_SIZE, "  peak_bright_p0: 0x%x\n",
				sbtm->peak_bright_p0);
		if (sbtm->p1_exp)
			pos += snprintf(buf + pos, PAGE_SIZE, "  p1_exp: 0x%x\n",
				sbtm->p1_exp);
		if (sbtm->p1_mant)
			pos += snprintf(buf + pos, PAGE_SIZE, "  p1_mant: 0x%x\n",
				sbtm->p1_mant);
		if (sbtm->peak_bright_p1)
			pos += snprintf(buf + pos, PAGE_SIZE, "  peak_bright_p1: 0x%x\n",
				sbtm->peak_bright_p1);
		if (sbtm->p2_exp)
			pos += snprintf(buf + pos, PAGE_SIZE, "  p2_exp: 0x%x\n",
				sbtm->p2_exp);
		if (sbtm->p2_mant)
			pos += snprintf(buf + pos, PAGE_SIZE, "  p2_mant: 0x%x\n",
				sbtm->p2_mant);
		if (sbtm->peak_bright_p2)
			pos += snprintf(buf + pos, PAGE_SIZE, "  peak_bright_p2: 0x%x\n",
				sbtm->peak_bright_p2);
		if (sbtm->p3_exp)
			pos += snprintf(buf + pos, PAGE_SIZE, "  p3_exp: 0x%x\n",
				sbtm->p3_exp);
		if (sbtm->p3_mant)
			pos += snprintf(buf + pos, PAGE_SIZE, "  p3_mant: 0x%x\n",
				sbtm->p3_mant);
		if (sbtm->peak_bright_p3)
			pos += snprintf(buf + pos, PAGE_SIZE, "  peak_bright_p3: 0x%x\n",
				sbtm->peak_bright_p3);
	}
	return pos;
}

static ssize_t hdr_cap_show(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	int pos = 0;
	struct hdmitx_dev *hdev = get_hdmitx21_device();
	const struct hdr_info *info = &hdev->rxcap.hdr_info;

	if (hdev->hdr_priority == 2) {
		pos += snprintf(buf + pos, PAGE_SIZE,
			"mask rx hdr capability\n");
		return pos;
	}

	return _hdr_cap_show(dev, attr, buf, info);
}

static ssize_t hdr_cap2_show(struct device *dev,
			    struct device_attribute *attr,
			    char *buf)
{
	struct hdmitx_dev *hdev = get_hdmitx21_device();
	const struct hdr_info *info2 = &hdev->rxcap.hdr_info2;

	return _hdr_cap_show(dev, attr, buf, info2);
}

static ssize_t vrr_cap_show(struct device *dev,
			    struct device_attribute *attr,
			    char *buf)
{
	return _vrr_cap_show(dev, attr, buf);
}

static ssize_t _show_dv_cap(struct device *dev,
			    struct device_attribute *attr,
			    char *buf,
			    const struct dv_info *dv)
{
	int pos = 0;
	int i;

	if (dv->ieeeoui != DV_IEEE_OUI || dv->block_flag != CORRECT) {
		pos += snprintf(buf + pos, PAGE_SIZE,
			"The Rx don't support DolbyVision\n");
		return pos;
	}
	pos += snprintf(buf + pos, PAGE_SIZE,
		"DolbyVision RX support list:\n");

	if (dv->ver == 0) {
		pos += snprintf(buf + pos, PAGE_SIZE,
			"VSVDB Version: V%d\n", dv->ver);
		pos += snprintf(buf + pos, PAGE_SIZE,
			"2160p%shz: 1\n",
			dv->sup_2160p60hz ? "60" : "30");
		pos += snprintf(buf + pos, PAGE_SIZE,
			"Support mode:\n");
		pos += snprintf(buf + pos, PAGE_SIZE,
			"  DV_RGB_444_8BIT\n");
		if (dv->sup_yuv422_12bit)
			pos += snprintf(buf + pos, PAGE_SIZE,
				"  DV_YCbCr_422_12BIT\n");
	}
	if (dv->ver == 1) {
		pos += snprintf(buf + pos, PAGE_SIZE,
			"VSVDB Version: V%d(%d-byte)\n",
			dv->ver, dv->length + 1);
		if (dv->length == 0xB) {
			pos += snprintf(buf + pos, PAGE_SIZE,
				"2160p%shz: 1\n",
				dv->sup_2160p60hz ? "60" : "30");
		pos += snprintf(buf + pos, PAGE_SIZE,
			"Support mode:\n");
		pos += snprintf(buf + pos, PAGE_SIZE,
			"  DV_RGB_444_8BIT\n");
		if (dv->sup_yuv422_12bit)
			pos += snprintf(buf + pos, PAGE_SIZE,
			"  DV_YCbCr_422_12BIT\n");
		if (dv->low_latency == 0x01)
			pos += snprintf(buf + pos, PAGE_SIZE,
				"  LL_YCbCr_422_12BIT\n");
		}

		if (dv->length == 0xE) {
			pos += snprintf(buf + pos, PAGE_SIZE,
				"2160p%shz: 1\n",
				dv->sup_2160p60hz ? "60" : "30");
			pos += snprintf(buf + pos, PAGE_SIZE,
				"Support mode:\n");
			pos += snprintf(buf + pos, PAGE_SIZE,
				"  DV_RGB_444_8BIT\n");
			if (dv->sup_yuv422_12bit)
				pos += snprintf(buf + pos, PAGE_SIZE,
				"  DV_YCbCr_422_12BIT\n");
		}
	}
	if (dv->ver == 2) {
		pos += snprintf(buf + pos, PAGE_SIZE,
			"VSVDB Version: V%d\n", dv->ver);
		pos += snprintf(buf + pos, PAGE_SIZE,
			"2160p%shz: 1\n",
			dv->sup_2160p60hz ? "60" : "30");
		pos += snprintf(buf + pos, PAGE_SIZE,
			"Support mode:\n");
		if (dv->Interface != 0x00 && dv->Interface != 0x01) {
			pos += snprintf(buf + pos, PAGE_SIZE,
				"  DV_RGB_444_8BIT\n");
			if (dv->sup_yuv422_12bit)
				pos += snprintf(buf + pos, PAGE_SIZE,
					"  DV_YCbCr_422_12BIT\n");
		}
		pos += snprintf(buf + pos, PAGE_SIZE,
			"  LL_YCbCr_422_12BIT\n");
		if (dv->Interface == 0x01 || dv->Interface == 0x03) {
			if (dv->sup_10b_12b_444 == 0x1) {
				pos += snprintf(buf + pos, PAGE_SIZE,
					"  LL_RGB_444_10BIT\n");
			}
			if (dv->sup_10b_12b_444 == 0x2) {
				pos += snprintf(buf + pos, PAGE_SIZE,
					"  LL_RGB_444_12BIT\n");
			}
		}
		pos += snprintf(buf + pos, PAGE_SIZE,
			"Parity: %d\n", dv->parity);
	}
	pos += snprintf(buf + pos, PAGE_SIZE,
		"IEEEOUI: 0x%06x\n", dv->ieeeoui);
	pos += snprintf(buf + pos, PAGE_SIZE,
		"EMP: %d\n", dv->dv_emp_cap);
	pos += snprintf(buf + pos, PAGE_SIZE, "VSVDB: ");
	for (i = 0; i < (dv->length + 1); i++)
		pos += snprintf(buf + pos, PAGE_SIZE, "%02x",
		dv->rawdata[i]);
	pos += snprintf(buf + pos, PAGE_SIZE, "\n");
	return pos;
}

static ssize_t dv_cap_show(struct device *dev,
			   struct device_attribute *attr,
			   char *buf)
{
	int pos = 0;
	struct hdmitx_dev *hdev = get_hdmitx21_device();
	const struct dv_info *dv = &hdev->rxcap.dv_info;

	if (dv->ieeeoui != DV_IEEE_OUI || hdev->hdr_priority) {
		pos += snprintf(buf + pos, PAGE_SIZE,
			"The Rx don't support DolbyVision\n");
		return pos;
	}
	return _show_dv_cap(dev, attr, buf, dv);
}

static ssize_t dv_cap2_show(struct device *dev,
			    struct device_attribute *attr,
			    char *buf)
{
	struct hdmitx_dev *hdev = get_hdmitx21_device();
	const struct dv_info *dv2 = &hdev->rxcap.dv_info2;

	return _show_dv_cap(dev, attr, buf, dv2);
}

static DEFINE_MUTEX(avmute_mutex);
void hdmitx21_av_mute_op(u32 flag, unsigned int path)
{
	struct hdmitx_dev *hdev = get_hdmitx21_device();
	static unsigned int avmute_path;
	unsigned int mute_us =
		hdev->debug_param.avmute_frame * hdmitx_get_frame_duration();

	mutex_lock(&avmute_mutex);
	if (flag == SET_AVMUTE) {
		avmute_path |= path;
		pr_info("%s: AVMUTE path=0x%x\n", __func__, path);
		hdev->hwop.cntlmisc(hdev, MISC_AVMUTE_OP, SET_AVMUTE);
	} else if (flag == CLR_AVMUTE) {
		avmute_path &= ~path;
		/* unmute only if none of the paths are muted */
		if (avmute_path == 0) {
			pr_info("%s: AV UNMUTE path=0x%x\n", __func__, path);
			hdev->hwop.cntlmisc(hdev, MISC_AVMUTE_OP, CLR_AVMUTE);
		}
	} else if (flag == OFF_AVMUTE) {
		hdev->hwop.cntlmisc(hdev, MISC_AVMUTE_OP, OFF_AVMUTE);
	}
	if (flag == SET_AVMUTE && hdev->debug_param.avmute_frame > 0)
		msleep(mute_us / 1000);
	mutex_unlock(&avmute_mutex);
}

/*
 *  1: set avmute
 * -1: clear avmute
 * 0: off avmute
 */
static ssize_t avmute_store(struct device *dev,
			    struct device_attribute *attr,
			    const char *buf, size_t count)
{
	int cmd = OFF_AVMUTE;
	static int mask0;
	static int mask1;

	pr_info("%s %s\n", __func__, buf);
	if (strncmp(buf, "-1", 2) == 0) {
		cmd = CLR_AVMUTE;
		mask0 = -1;
	} else if (strncmp(buf, "0", 1) == 0) {
		cmd = OFF_AVMUTE;
		mask0 = 0;
	} else if (strncmp(buf, "1", 1) == 0) {
		cmd = SET_AVMUTE;
		mask0 = 1;
	}
	if (strncmp(buf, "r-1", 3) == 0) {
		cmd = CLR_AVMUTE;
		mask1 = -1;
	} else if (strncmp(buf, "r0", 2) == 0) {
		cmd = OFF_AVMUTE;
		mask1 = 0;
	} else if (strncmp(buf, "r1", 2) == 0) {
		cmd = SET_AVMUTE;
		mask1 = 1;
	}
	if (mask0 == 1 || mask1 == 1)
		cmd = SET_AVMUTE;
	else if ((mask0 == -1) && (mask1 == -1))
		cmd = CLR_AVMUTE;
	hdmitx21_av_mute_op(cmd, AVMUTE_PATH_1);

	return count;
}

static ssize_t avmute_show(struct device *dev,
			   struct device_attribute *attr, char *buf)
{
	struct hdmitx_dev *hdev = get_hdmitx21_device();
	int ret = 0;
	int pos = 0;

	ret = hdev->hwop.cntlmisc(hdev, MISC_READ_AVMUTE_OP, 0);
	pos += snprintf(buf + pos, PAGE_SIZE, "%d", ret);

	return pos;
}

/*
 * 1: enable hdmitx phy
 * 0: disable hdmitx phy
 */
static ssize_t phy_store(struct device *dev,
			 struct device_attribute *attr,
			 const char *buf, size_t count)
{
	int cmd = TMDS_PHY_ENABLE;
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	pr_info("%s %s\n", __func__, buf);

	if (strncmp(buf, "0", 1) == 0) {
		/* used before switch mode */
		cmd = TMDS_PHY_DISABLE;
	} else if (strncmp(buf, "1", 1) == 0) {
		cmd = TMDS_PHY_ENABLE;
	} else if (strncmp(buf, "-1", 2) == 0) {
		/* used for debug disable phy */
		cmd = TMDS_PHY_DISABLE;
	} else {
		pr_info("set phy wrong: %s\n", buf);
	}
	hdev->hwop.cntlmisc(hdev, MISC_TMDS_PHY_OP, cmd);
	return count;
}

static ssize_t phy_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int pos = 0;

	pos += snprintf(buf + pos, PAGE_SIZE, "%d\n",
		hdmitx21_read_phy_status());

		return pos;
}

static ssize_t rxsense_policy_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	int val = 0;
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	if (isdigit(buf[0])) {
		val = buf[0] - '0';
		pr_info("hdmitx: set rxsense_policy as %d\n", val);
		if (val == 0 || val == 1)
			hdev->rxsense_policy = val;
		else
			pr_info("only accept as 0 or 1\n");
	}
	if (hdev->rxsense_policy)
		queue_delayed_work(hdev->rxsense_wq,
				   &hdev->work_rxsense, 0);
	else
		cancel_delayed_work(&hdev->work_rxsense);

	return count;
}

static ssize_t rxsense_policy_show(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	int pos = 0;
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	pos += snprintf(buf + pos, PAGE_SIZE, "%d\n",
		hdev->rxsense_policy);

	return pos;
}

/* cedst_policy: 0, no CED feature
 *	       1, auto mode, depends on RX scdc_present
 *	       2, forced CED feature
 */
static ssize_t cedst_policy_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf,
				  size_t count)
{
	int val = 0;
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	if (isdigit(buf[0])) {
		val = buf[0] - '0';
		pr_info("hdmitx: set cedst_policy as %d\n", val);
		if (val == 0 || val == 1 || val == 2) {
			hdev->cedst_policy = val;
			if (val == 1) { /* Auto mode, depends on Rx */
				/* check RX scdc_present */
				if (hdev->rxcap.scdc_present)
					hdev->cedst_policy = 1;
				else
					hdev->cedst_policy = 0;
			}
			if (val == 2) /* Force mode */
				hdev->cedst_policy = 1;
			/* assgin cedst_en from dts or here */
			hdev->cedst_en = hdev->cedst_policy;
		} else {
			pr_info("only accept as 0, 1(auto), or 2(force)\n");
		}
	}
	if (hdev->cedst_policy)
		queue_delayed_work(hdev->cedst_wq, &hdev->work_cedst, 0);
	else
		cancel_delayed_work(&hdev->work_cedst);

	return count;
}

static ssize_t cedst_policy_show(struct device *dev,
				 struct device_attribute *attr,
				 char *buf)
{
	int pos = 0;
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	pos += snprintf(buf + pos, PAGE_SIZE, "%d\n",
		hdev->cedst_policy);

	return pos;
}

static ssize_t cedst_count_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	int pos = 0;
	struct hdmitx_dev *hdev = get_hdmitx21_device();
	struct ced_cnt *ced = &hdev->ced_cnt;
	struct scdc_locked_st *ch_st = &hdev->chlocked_st;
	enum frl_rate_enum frl_rate;

	frl_rate = hdev->hwop.cntlmisc(hdev, MISC_GET_FRL_MODE, 0);
	if (!frl_rate && !ch_st->clock_detected)
		pos += snprintf(buf + pos, PAGE_SIZE, "clock undetected\n");
	if (!ch_st->ch0_locked)
		pos += snprintf(buf + pos, PAGE_SIZE, "CH0 unlocked\n");
	if (!ch_st->ch1_locked)
		pos += snprintf(buf + pos, PAGE_SIZE, "CH1 unlocked\n");
	if (!ch_st->ch2_locked)
		pos += snprintf(buf + pos, PAGE_SIZE, "CH2 unlocked\n");
	if (frl_rate && !ch_st->ch3_locked)
		pos += snprintf(buf + pos, PAGE_SIZE, "CH3 unlocked\n");
	if (ced->ch0_valid && ced->ch0_cnt)
		pos += snprintf(buf + pos, PAGE_SIZE, "CH0 ErrCnt 0x%x\n",
			ced->ch0_cnt);
	if (ced->ch1_valid && ced->ch1_cnt)
		pos += snprintf(buf + pos, PAGE_SIZE, "CH1 ErrCnt 0x%x\n",
			ced->ch1_cnt);
	if (ced->ch2_valid && ced->ch2_cnt)
		pos += snprintf(buf + pos, PAGE_SIZE, "CH2 ErrCnt 0x%x\n",
			ced->ch2_cnt);
	if (frl_rate >= FRL_6G4L && ced->ch3_valid && ced->ch3_cnt)
		pos += snprintf(buf + pos, PAGE_SIZE, "CH3 ErrCnt 0x%x\n",
			ced->ch3_cnt);
	if (frl_rate && ced->rs_c_valid && ced->rs_c_cnt)
		pos += snprintf(buf + pos, PAGE_SIZE, "RSCC ErrCnt 0x%x\n",
			ced->rs_c_cnt);

	memset(ced, 0, sizeof(*ced));

	return pos;
}

static ssize_t sspll_store(struct device *dev,
			   struct device_attribute *attr,
			   const char *buf,
			   size_t count)
{
	int val = 0;
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	if (isdigit(buf[0])) {
		val = buf[0] - '0';
		pr_info("set sspll : %d\n", val);
		if (val == 0 || val == 1)
			hdev->sspll = val;
		else
			pr_info("sspll only accept as 0 or 1\n");
	}

	return count;
}

static ssize_t sspll_show(struct device *dev,
			  struct device_attribute *attr,
			  char *buf)
{
	int pos = 0;
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	pos += snprintf(buf + pos, PAGE_SIZE, "%d\n",
		hdev->sspll);

	return pos;
}

static ssize_t frac_rate_policy_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf,
				      size_t count)
{
	int val = 0;
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	if (isdigit(buf[0])) {
		val = buf[0] - '0';
		pr_info("set frac_rate_policy as %d\n", val);
		if (val == 0 || val == 1)
			hdev->frac_rate_policy = val;
		else
			pr_info("only accept as 0 or 1\n");
	}

	return count;
}

static void set_frac_rate_policy(int val)
{
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	hdev->frac_rate_policy = val;
}

static int get_frac_rate_policy(void)
{
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	return hdev->frac_rate_policy;
}

static ssize_t frac_rate_policy_show(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	int pos = 0;
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	pos += snprintf(buf + pos, PAGE_SIZE, "%d\n",
		hdev->frac_rate_policy);

	return pos;
}

static ssize_t hdcp_type_policy_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t hdcp_type_policy_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int pos = 0;

	return pos;
}

static ssize_t hdcp_lstore_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	int pos = 0;
	int lstore = hdmitx21_device.lstore;

	if (lstore < 0x10) {
		lstore = 0;
		if (get_hdcp2_lstore())
			lstore |= BIT(1);
		if (get_hdcp1_lstore())
			lstore |= BIT(0);
	}
	if ((lstore & 0x3) == 0x3) {
		pos += snprintf(buf + pos, PAGE_SIZE, "22+14\n");
	} else {
		if (lstore & 0x1)
			pos += snprintf(buf + pos, PAGE_SIZE, "14\n");
		if (lstore & 0x2)
			pos += snprintf(buf + pos, PAGE_SIZE, "22\n");
	}
	return pos;
}

static ssize_t hdcp_lstore_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	/* debug usage for key store check
	 * echo value > hdcp_lstore. value can be
	 * -1: automatically check stored key when enable hdcp
	 * 0: same as no hdcp key stored
	 * 11: only hdcp1.x key stored
	 * 12: only hdcp2.x key stored
	 * 13: both hdcp1.x and hdcp2.x key stored
	 */
	pr_info("hdcp: set lstore as %s\n", buf);
	if (strncmp(buf, "-1", 2) == 0)
		hdmitx21_device.lstore = 0x0;
	if (strncmp(buf, "0", 1) == 0 ||
		strncmp(buf, "10", 2) == 0)
		hdmitx21_device.lstore = 0x10;
	if (strncmp(buf, "11", 2) == 0)
		hdmitx21_device.lstore = 0x11;
	if (strncmp(buf, "12", 2) == 0)
		hdmitx21_device.lstore = 0x12;
	if (strncmp(buf, "13", 2) == 0)
		hdmitx21_device.lstore = 0x13;

	return count;
}

static ssize_t div40_show(struct device *dev,
			  struct device_attribute *attr, char *buf)
{
	int pos = 0;
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	pos += snprintf(buf + pos, PAGE_SIZE, "%d\n", hdev->div40);

	return pos;
}

/* echo 1 > div40, force send 1:40 tmds bit clk ratio
 * echo 0 > div40, send 1:10 tmds bit clk ratio if scdc_present
 * echo 2 > div40, force send 1:10 tmds bit clk ratio
 */
static ssize_t div40_store(struct device *dev,
			   struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	hdev->hwop.cntlddc(hdev, DDC_SCDC_DIV40_SCRAMB, buf[0] - '0');

	return count;
}

static ssize_t hdcp_mode_show(struct device *dev,
			      struct device_attribute *attr,
			      char *buf)
{
	int pos = 0;
	unsigned int hdcp_ret = 0;
	u32 hdcp_mode = hdmitx21_get_hdcp_mode();

	switch (hdcp_mode) {
	case 1:
		pos += snprintf(buf + pos, PAGE_SIZE, "14");
		break;
	case 2:
		pos += snprintf(buf + pos, PAGE_SIZE, "22");
		break;
	default:
		pos += snprintf(buf + pos, PAGE_SIZE, "off");
		break;
	}

	if (hdmitx21_device.hdcp_ctl_lvl > 0 && hdcp_mode > 0) {
		if (hdcp_mode == 1)
			hdcp_ret = get_hdcp1_result();
		else if (hdcp_mode == 2)
			hdcp_ret = get_hdcp2_result();
		else
			hdcp_ret = 0;
		if (hdcp_ret == 1)
			pos += snprintf(buf + pos, PAGE_SIZE, ": succeed\n");
		else
			pos += snprintf(buf + pos, PAGE_SIZE, ": fail\n");
	}

	return pos;
}

/* note: below store is just for debug, no mutex in it */
static ssize_t hdcp_mode_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t count)
{
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	if (strncmp(buf, "f1", 2) == 0) {
		hdev->hdcp_mode = 0x1;
		hdcp_mode_set(1);
	}
	if (strncmp(buf, "f2", 2) == 0) {
		hdev->hdcp_mode = 0x2;
		hdcp_mode_set(2);
	}
	if (buf[0] == '0') {
		hdev->hdcp_mode = 0x00;
		hdcp_mode_set(0);
	}

	return count;
}

/* Indicate whether a rptx under repeater */
static ssize_t hdmi_repeater_tx_show(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	int pos = 0;
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	pos += snprintf(buf + pos, PAGE_SIZE, "%d\n",
		!!hdev->repeater_tx);

	return pos;
}

static ssize_t def_stream_type_show(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	int pos = 0;
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	pos += snprintf(buf + pos, PAGE_SIZE, "%d\n",
		hdev->def_stream_type);

	return pos;
}

static ssize_t def_stream_type_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf,
				      size_t count)
{
	u8 val = 0;
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	if (isdigit(buf[0])) {
		val = buf[0] - '0';
		pr_info("set def_stream_type as %d\n", val);
		if (val == 0 || val == 1)
			hdev->def_stream_type = val;
		else
			pr_info("only accept as 0 or 1\n");
	}

	return count;
}

static ssize_t propagate_stream_type_show(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	int pos = 0;
	struct hdmitx_dev *hdev = get_hdmitx21_device();
	struct hdcp_t *p_hdcp = (struct hdcp_t *)hdev->am_hdcp;

	if (p_hdcp->ds_repeater && p_hdcp->hdcp_type == HDCP_VER_HDCP2X) {
		pos += snprintf(buf + pos, PAGE_SIZE, "%d\n",
			p_hdcp->csm_message.streamid_type & 0xFF);
	} else {
		pr_info("no stream type, as ds_repeater: %d, hdcp_type: %d\n",
			p_hdcp->ds_repeater, p_hdcp->hdcp_type);
	}
	return pos;
}

static ssize_t cont_smng_method_show(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	int pos = 0;
	struct hdmitx_dev *hdev = get_hdmitx21_device();
	struct hdcp_t *p_hdcp = (struct hdcp_t *)hdev->am_hdcp;

	pos += snprintf(buf + pos, PAGE_SIZE, "%d\n",
		p_hdcp->cont_smng_method);

	return pos;
}

/* content stream management update method:
 * when upstream side received new content stream
 * management message, there're two method to
 * update content stream type that propagated
 * to downstream hdcp2.3 repeater:
 * 0(default): only send content stream management
 * message with new stream type to downstream
 * 1: init new re-auth with downstream
 */
static ssize_t cont_smng_method_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf,
				      size_t count)
{
	u8 val = 0;
	struct hdmitx_dev *hdev = get_hdmitx21_device();
	struct hdcp_t *p_hdcp = (struct hdcp_t *)hdev->am_hdcp;

	if (isdigit(buf[0])) {
		val = buf[0] - '0';
		pr_info("set cont_smng_method as %d\n", val);
		if (val == 0 || val == 1)
			p_hdcp->cont_smng_method = val;
		else
			pr_info("only accept as 0 or 1\n");
	}

	return count;
}

static ssize_t is_passthrough_switch_show(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	int pos = 0;

	pos += snprintf(buf + pos, PAGE_SIZE, "%d\n", is_passthrough_switch);

	return pos;
}

static ssize_t is_passthrough_switch_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf,
				      size_t count)
{
	u8 val = 0;

	if (isdigit(buf[0])) {
		val = buf[0] - '0';
		pr_info("set is_passthrough_switch as %d\n", val);
		if (val == 0 || val == 1)
			is_passthrough_switch = val;
		else
			pr_info("only accept as 0 or 1\n");
	}

	return count;
}

/* is hdcp cts test equipment */
static ssize_t is_hdcp_cts_te_show(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	int pos = 0;
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	pos += snprintf(buf + pos, PAGE_SIZE, "%d\n", hdmitx21_edid_only_support_sd(hdev));

	return pos;
}

static ssize_t frl_rate_show(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	int pos = 0;
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	pos += snprintf(buf + pos, PAGE_SIZE, "%d\n", hdev->frl_rate);
	switch (hdev->frl_rate) {
	case FRL_3G3L:
		pos += snprintf(buf + pos, PAGE_SIZE, "FRL_3G3L\n");
		break;
	case FRL_6G3L:
		pos += snprintf(buf + pos, PAGE_SIZE, "FRL_6G3L\n");
		break;
	case FRL_6G4L:
		pos += snprintf(buf + pos, PAGE_SIZE, "FRL_6G4L\n");
		break;
	case FRL_8G4L:
		pos += snprintf(buf + pos, PAGE_SIZE, "FRL_8G4L\n");
		break;
	case FRL_10G4L:
		pos += snprintf(buf + pos, PAGE_SIZE, "FRL_10G4L\n");
		break;
	case FRL_12G4L:
		pos += snprintf(buf + pos, PAGE_SIZE, "FRL_12G4L\n");
		break;
	default:
		break;
	}

	return pos;
}

static ssize_t frl_rate_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf,
				      size_t count)
{
	u8 val = 0;
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	/* if rx don't support FRL, return */
	if (!hdev->rxcap.max_frl_rate)
		pr_warning("rx not support FRL\n");

	/* forced FRL rate setting */
	if (buf[0] == 'f' && isdigit(buf[1])) {
		val = buf[1] - '0';
		if (val > FRL_12G4L) {
			pr_info("set frl_rate in 0 ~ 6\n");
			return count;
		}
		hdev->manual_frl_rate = val;
		pr_info("set tx frl_rate as %d\n", val);
	}
	if (hdev->manual_frl_rate > hdev->rxcap.max_frl_rate)
		pr_info("larger than rx max_frl_rate %d\n", hdev->rxcap.max_frl_rate);

	return count;
}

static ssize_t dsc_en_show(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	int pos = 0;
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	pos += snprintf(buf + pos, PAGE_SIZE, "%d\n", hdev->dsc_en);

	return pos;
}

/* dsc_policy:
 * 0 automatically enable dsc if necessary, but not support 8k Y444/rgb,12bit
 * or 4k100/120hz Y444/rgb,12bit
 * 1 force enable dsc for mode that can be supported both with dsc/non-dsc
 * 2 force enable dsc for mode test for new dsc mode(debug only)
 * 3 forcely filter out dsc mode output by valid_mode_check
 * 4 automatically enable dsc if necessary, include those mentioned in 0
 */
static ssize_t dsc_policy_show(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	int pos = 0;
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	pos += snprintf(buf + pos, PAGE_SIZE, "%d\n", hdev->dsc_policy);

	return pos;
}

static ssize_t dsc_policy_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf,
				      size_t count)
{
	u8 val = 0;
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	if (isdigit(buf[0])) {
		val = buf[0] - '0';
		if (val != 0 && val != 1 && val != 2 && val != 3 && val != 4) {
			pr_info("set dsc_policy in 0~4\n");
			return count;
		}
		hdev->dsc_policy = val;
		pr_info("set dsc_policy as %d\n", val);
	}

	return count;
}

static ssize_t hdr_mute_frame_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int pos = 0;

	pos += snprintf(buf + pos, PAGE_SIZE, "%d\r\n", hdr_mute_frame);
	return pos;
}

static ssize_t hdr_mute_frame_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long mute_frame = 0;

	pr_info("set hdr_mute_frame: %s\n", buf);
	if (kstrtoul(buf, 10, &mute_frame) == 0)
		hdr_mute_frame = mute_frame;
	return count;
}

#include <linux/amlogic/media/vout/hdmi_tx/hdmi_rptx.h>

void direct21_hdcptx14_opr(enum rptx_hdcp14_cmd cmd, void *args)
{
}
EXPORT_SYMBOL(direct21_hdcptx14_opr);

/* Special FBC check */
static int check_fbc_special(u8 *edid_dat)
{
	if (edid_dat[250] == 0xfb && edid_dat[251] == 0x0c)
		return 1;
	else
		return 0;
}

static ssize_t hdcp_ver_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	int pos = 0;
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	if (!hdev->hpd_state) {
		pr_info("%s: hpd low, just return 14\n", __func__);
		pos += snprintf(buf + pos, PAGE_SIZE, "14\n\r");
		return pos;
	}
	if (rx_hdcp2_ver) {
		pos += snprintf(buf + pos, PAGE_SIZE, "22\n\r");
	} else {
		//on hotplug case
		/* note that, when do hdcp repeater1.4 CTS,
		 * hdcp port access will affect item 3a-02 Irregular
		 * procedure: (First part of authentication) HDCP port
		 * access. Refer to hdcp1.4 cts spec: "If DUT does
		 * not read an HDCP register past 4 seconds after
		 * the previous attempt, then FAIL". after read hdcp
		 * version soon after plugin(access failed as TE
		 * not ack), our hdmitx side should keep retrying
		 * in 4S. but source TE start hdcp auth with
		 * hdmirx side too late(more than 4S), as hdcp auth
		 * of hdmitx side is started by hdmirx side, it will
		 * time out the access of hdcp port of 4 second.
		 * so for repeater CTS, should not read hdcp version
		 * whenever you want. Here add protect to only read
		 * hdcp version when currently not in hdmirx channel.
		 * special customer want to read downstream hdcp version
		 * after hotplug, and only sen 4K output when EDID
		 * support 4K && support hdcp2.2. so add is_4k_sink()
		 * decision, it won't affect hdcp repeater CTS.
		 */
		if (hdcp_need_control_by_upstream(hdev) && !is_4k_sink(hdev)) {
			pr_info("%s: currently should not read hdcp version\n", __func__);
		} else if (hdmitx21_get_hdcp_mode() == 0) {
			if (get_hdcp2_lstore() && is_rx_hdcp2ver()) {
				pos += snprintf(buf + pos, PAGE_SIZE, "22\n\r");
				rx_hdcp2_ver = 1;
			}
			pr_info("%s: hdev->hdcp_mode: 0, rx_hdcp2_ver = %d\n",
				__func__, rx_hdcp2_ver);
		}
	}
	/* Here, must assume RX support HDCP14, otherwise affect 1A-03 */
	pos += snprintf(buf + pos, PAGE_SIZE, "14\n\r");

	return pos;
}

static ssize_t hpd_state_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	int pos = 0;
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	pos += snprintf(buf + pos, PAGE_SIZE, "%d",
		hdev->hpd_state);
	return pos;
}

static ssize_t rxsense_state_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	int pos = 0;
	int sense;
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	sense = hdev->hwop.cntlmisc(hdev, MISC_TMDS_RXSENSE, 0);

	pos += snprintf(buf + pos, PAGE_SIZE, "%d", sense);
	return pos;
}

static ssize_t hdmi_used_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int pos = 0;
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	pos += snprintf(buf + pos, PAGE_SIZE, "%d",
		hdev->already_used);
	return pos;
}

static ssize_t fake_plug_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	return snprintf(buf, PAGE_SIZE, "%d", hdev->hpd_state);
}

static ssize_t fake_plug_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t count)
{
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	pr_info("hdmitx: fake plug %s\n", buf);

	if (strncmp(buf, "1", 1) == 0)
		hdev->hpd_state = 1;

	if (strncmp(buf, "0", 1) == 0) {
		hdev->hpd_state = 0;
		/* below is special for customer */
		//remove hdr caps from vinfo so that AMDV module does
		//not start sending HDR10 when changing HDR caps
		//until vinfo is updated while setting mode
		edidinfo_detach_to_vinfo(hdev);
	}

	/*notify to drm hdmi*/
	if (hdmitx21_device.drm_hpd_cb.callback)
		hdmitx21_device.drm_hpd_cb.callback
			(hdmitx21_device.drm_hpd_cb.data);

	hdmitx21_set_uevent(HDMITX_HPD_EVENT, hdev->hpd_state);

	return count;
}

static ssize_t ready_show(struct device *dev,
			  struct device_attribute *attr, char *buf)
{
	int pos = 0;
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	pos += snprintf(buf + pos, PAGE_SIZE, "%d\r\n",
		hdev->ready);
	return pos;
}

static ssize_t ready_store(struct device *dev,
			   struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	if (strncmp(buf, "0", 1) == 0)
		hdev->ready = 0;
	if (strncmp(buf, "1", 1) == 0)
		hdev->ready = 1;
	return count;
}

static ssize_t support_3d_show(struct device *dev,
			       struct device_attribute *attr,
			       char *buf)
{
	int pos = 0;
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	pos += snprintf(buf + pos, PAGE_SIZE, "%d\n",
			hdev->rxcap.threeD_present);
	return pos;
}

static ssize_t aon_output_show(struct device *dev,
			  struct device_attribute *attr, char *buf)
{
	int pos = 0;
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	pos += snprintf(buf + pos, PAGE_SIZE, "%d\r\n",
		hdev->aon_output);
	return pos;
}

static ssize_t aon_output_store(struct device *dev,
			   struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	if (strncmp(buf, "0", 1) == 0)
		hdev->aon_output = 0;
	if (strncmp(buf, "1", 1) == 0)
		hdev->aon_output = 1;
	return count;
}

static ssize_t hdr_priority_mode_show(struct device *dev,
			  struct device_attribute *attr, char *buf)
{
	int pos = 0;
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	pos += snprintf(buf + pos, PAGE_SIZE, "%d\r\n",
		hdev->hdr_priority);

	return pos;
}

/* hide or enable HDR capabilities.
 * 0 : No HDR capabilities are hidden
 * 1 : DV Capabilities are hidden
 * 2 : All HDR capabilities are hidden
 */
static ssize_t hdr_priority_mode_store(struct device *dev,
			   struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct hdmitx_dev *hdev = get_hdmitx21_device();
	unsigned int val = 0;
	struct vinfo_s *info = NULL;

	if ((strncmp("0", buf, 1) == 0) || (strncmp("1", buf, 1) == 0) ||
	    (strncmp("2", buf, 1) == 0)) {
		val = buf[0] - '0';
	}

	if (val == hdev->hdr_priority)
		return count;

	/* get current vinfo */
	info = hdmitx_get_current_vinfo(NULL);
	if (!info)
		return count;
	mutex_lock(&hdev->hdmimode_mutex);
	hdev->hdr_priority = val;
	if (hdev->hdr_priority == 1) {
		//clear dv support
		memset(&hdev->rxcap.dv_info, 0x00, sizeof(struct dv_info));
		hdmitx_vdev.dv_info = &dv_dummy;
		//restore hdr support
		memcpy(&hdev->rxcap.hdr_info, &hdev->rxcap.hdr_info2, sizeof(struct hdr_info));
		//restore BT2020 support
		hdev->rxcap.colorimetry_data = hdev->rxcap.colorimetry_data2;
		hdrinfo_to_vinfo(&info->hdr_info, hdev);
	} else if (hdev->hdr_priority == 2) {
		//clear dv support
		memset(&hdev->rxcap.dv_info, 0x00, sizeof(struct dv_info));
		hdmitx_vdev.dv_info = &dv_dummy;
		//clear hdr support
		memset(&hdev->rxcap.hdr_info, 0x00, sizeof(struct hdr_info));
		//clear BT2020 support
		hdev->rxcap.colorimetry_data = hdev->rxcap.colorimetry_data2 & 0x1F;
		memset(&info->hdr_info, 0, sizeof(struct hdr_info));
	} else {
		//restore dv support
		memcpy(&hdev->rxcap.dv_info, &hdev->rxcap.dv_info2, sizeof(struct dv_info));
		//restore hdr support
		memcpy(&hdev->rxcap.hdr_info, &hdev->rxcap.hdr_info2, sizeof(struct hdr_info));
		//restore BT2020 support
		hdev->rxcap.colorimetry_data = hdev->rxcap.colorimetry_data2;
		edidinfo_attach_to_vinfo(hdev);
	}
	/* hdmitx21_event_notify(HDMITX_HDR_PRIORITY, &hdev->hdr_priority); */
	/* force trigger plugin event
	 * hdmitx21_set_uevent_state(HDMITX_HPD_EVENT, 0);
	 * hdmitx21_set_uevent(HDMITX_HPD_EVENT, 1);
	 */
	mutex_unlock(&hdev->hdmimode_mutex);
	return count;
}

/* hdcp fail event method 1: add hdcp fail uevent filter
 * below need_filter_hdcp_off and filter_hdcp_off_period
 * sysfs node are for this filter
 */
static ssize_t need_filter_hdcp_off_show(struct device *dev,
			  struct device_attribute *attr, char *buf)
{
	int pos = 0;
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	pos += snprintf(buf + pos, PAGE_SIZE, "%d\r\n",
		hdev->need_filter_hdcp_off);

	return pos;
}

/* if need to filter hdcp fail uevent, systemcontrol
 * write 1 to this node. for example:
 * when start player game movie, it switch from DV STD to DV LL,
 * before switch mode, write 1 to this node. it means that it
 * won't send hdcp fail uevent if hdcp fail but retry auth
 * pass during filter_hdcp_off_period seconds.
 * note: need_filter_hdcp_off is self cleared after filter
 * period expired
 */
static ssize_t need_filter_hdcp_off_store(struct device *dev,
			   struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct hdmitx_dev *hdev = get_hdmitx21_device();
	unsigned int val = 0;

	if ((strncmp("0", buf, 1) == 0) || (strncmp("1", buf, 1) == 0))
		val = buf[0] - '0';

	hdev->need_filter_hdcp_off = val;
	pr_info("hdmitx: set need_filter_hdcp_off: %d\n", val);

	return count;
}

/* if hdcp fail but retry auth pass during this period(unit: second),
 * then won't sent hdcp fail uevent. the default is 6 second
 */
static ssize_t filter_hdcp_off_period_show(struct device *dev,
			  struct device_attribute *attr, char *buf)
{
	int pos = 0;
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	pos += snprintf(buf + pos, PAGE_SIZE, "%d\r\n",
		hdev->filter_hdcp_off_period);

	return pos;
}

/* example: write 5 to filter 5 second */
static ssize_t filter_hdcp_off_period_store(struct device *dev,
			   struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct hdmitx_dev *hdev = get_hdmitx21_device();
	unsigned long filter_second = 0;

	pr_info("set hdcp fail filter_second: %s\n", buf);
	if (kstrtoul(buf, 10, &filter_second) == 0)
		hdev->filter_hdcp_off_period = filter_second;

	return count;
}

/* hdcp fail event method 2: don't stop-restart hdcp auth */
/* when start play game movie, it will switch from
 * DV STD to DV LL, hdcp will stop and restart after
 * mode setting done.
 * now provide an option: when DV STD <-> DV LL
 * switch caused by game movie, systemcontrol write 1
 * to not_restart_hdcp node before do mode switch, it
 * means that this colorspace(mode) switch will only
 * switch mode, but won't stop->restart hdcp action to
 * prevent hdcp fail uevent sent to app.
 * note: 1.not sure if it will always work on different TV.
 * 2.after mode switch done, not_restart_hdcp will be self cleared
 */
static ssize_t not_restart_hdcp_show(struct device *dev,
			  struct device_attribute *attr, char *buf)
{
	int pos = 0;
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	pos += snprintf(buf + pos, PAGE_SIZE, "%d\r\n",
		hdev->not_restart_hdcp);

	return pos;
}

static ssize_t not_restart_hdcp_store(struct device *dev,
			   struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct hdmitx_dev *hdev = get_hdmitx21_device();
	unsigned int val = 0;

	if ((strncmp("0", buf, 1) == 0) || (strncmp("1", buf, 1) == 0))
		val = buf[0] - '0';

	hdev->not_restart_hdcp = val;
	pr_info("hdmitx: set not_restart_hdcp: %d\n", val);

	return count;
}

static ssize_t sysctrl_enable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int pos = 0;
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	pos += snprintf(buf + pos, PAGE_SIZE, "%d\r\n",
		hdev->systemcontrol_on);
	return pos;
}

static ssize_t sysctrl_enable_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	if (strncmp(buf, "0", 1) == 0)
		hdev->systemcontrol_on = false;
	if (strncmp(buf, "1", 1) == 0)
		hdev->systemcontrol_on = true;
	return count;
}

static ssize_t hdcp_ctl_lvl_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int pos = 0;

	pos += snprintf(buf + pos, PAGE_SIZE, "%d\r\n",
		hdmitx21_device.hdcp_ctl_lvl);
	return pos;
}

static ssize_t hdcp_ctl_lvl_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long ctl_lvl = 0xf;

	pr_info("set hdcp_ctl_lvl: %s\n", buf);
	if (kstrtoul(buf, 10, &ctl_lvl) == 0) {
		if (ctl_lvl <= 2)
			hdmitx21_device.hdcp_ctl_lvl = ctl_lvl;
	}
	return count;
}

static ssize_t hdmitx21_show(struct device *dev,
			       struct device_attribute *attr,
			       char *buf)
{
	return snprintf(buf, PAGE_SIZE, "1\n");
}

static DEVICE_ATTR_RW(disp_mode);
static DEVICE_ATTR_RW(attr);
static DEVICE_ATTR_RW(vid_mute);
static DEVICE_ATTR_RW(edid);
static DEVICE_ATTR_RO(rawedid);
static DEVICE_ATTR_RO(sink_type);
static DEVICE_ATTR_RO(edid_parsing);
static DEVICE_ATTR_RW(config);
static DEVICE_ATTR_WO(debug);
static DEVICE_ATTR_RO(disp_cap);
static DEVICE_ATTR_RO(vrr_cap);
static DEVICE_ATTR_RO(preferred_mode);
static DEVICE_ATTR_RO(cea_cap);
static DEVICE_ATTR_RO(vesa_cap);
static DEVICE_ATTR_RO(aud_cap);
static DEVICE_ATTR_RW(aud_mute);
static DEVICE_ATTR_RO(lipsync_cap);
static DEVICE_ATTR_RO(hdmi_hdr_status);
static DEVICE_ATTR_RO(hdr_cap);
static DEVICE_ATTR_RO(hdr_cap2);
static DEVICE_ATTR_RO(dv_cap);
static DEVICE_ATTR_RO(dv_cap2);
static DEVICE_ATTR_RO(dc_cap);
static DEVICE_ATTR_WO(valid_mode);
static DEVICE_ATTR_RO(allm_cap);
static DEVICE_ATTR_RW(allm_mode);
static DEVICE_ATTR_RO(contenttype_cap);
static DEVICE_ATTR_RW(contenttype_mode);
static DEVICE_ATTR_RW(ll_mode);
static DEVICE_ATTR_RW(ll_user_mode);
static DEVICE_ATTR_RW(avmute);
static DEVICE_ATTR_RW(phy);
static DEVICE_ATTR_RW(sspll);
static DEVICE_ATTR_RW(frac_rate_policy);
static DEVICE_ATTR_RW(rxsense_policy);
static DEVICE_ATTR_RW(cedst_policy);
static DEVICE_ATTR_RO(cedst_count);
static DEVICE_ATTR_RW(hdcp_mode);
static DEVICE_ATTR_RO(hdcp_ver);
static DEVICE_ATTR_RW(hdcp_type_policy);
static DEVICE_ATTR_RW(hdcp_lstore);
static DEVICE_ATTR_RO(hdmi_repeater_tx);
static DEVICE_ATTR_RW(div40);
static DEVICE_ATTR_RO(hpd_state);
static DEVICE_ATTR_RO(hdmi_used);
static DEVICE_ATTR_RO(rxsense_state);
static DEVICE_ATTR_RW(fake_plug);
static DEVICE_ATTR_RW(ready);
static DEVICE_ATTR_RO(support_3d);
static DEVICE_ATTR_RO(hdmitx21);
static DEVICE_ATTR_RW(def_stream_type);
static DEVICE_ATTR_RW(hdcp_ctl_lvl);
static DEVICE_ATTR_RW(sysctrl_enable);
static DEVICE_ATTR_RW(aon_output);
static DEVICE_ATTR_RO(propagate_stream_type);
static DEVICE_ATTR_RW(cont_smng_method);
static DEVICE_ATTR_RW(hdr_priority_mode);
static DEVICE_ATTR_RW(is_passthrough_switch);
static DEVICE_ATTR_RO(is_hdcp_cts_te);
static DEVICE_ATTR_RW(need_filter_hdcp_off);
static DEVICE_ATTR_RW(filter_hdcp_off_period);
static DEVICE_ATTR_RW(not_restart_hdcp);
static DEVICE_ATTR_RW(frl_rate);
static DEVICE_ATTR_RW(hdr_mute_frame);
static DEVICE_ATTR_RO(dsc_en);
static DEVICE_ATTR_RW(dsc_policy);

#ifdef CONFIG_AMLOGIC_VOUT_SERVE
static struct vinfo_s *hdmitx_get_current_vinfo(void *data)
{
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	if (!hdev->para)
		return NULL;
	return &hdev->para->hdmitx_vinfo;
}

static int hdmitx_set_current_vmode(enum vmode_e mode, void *data)
{
	struct vinfo_s *vinfo = NULL;
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	hdmitx_register_vrr(hdev);
	if (!(mode & VMODE_INIT_BIT_MASK)) {
		set_disp_mode_auto();
	} else {
		pr_info("already display in uboot\n");
		hdev->ready = 1;
		update_current_para(hdev);
		if (hdev->rxcap.max_frl_rate) {
			hdev->frl_rate = hdev->hwop.cntlmisc(hdev, MISC_GET_FRL_MODE, 0);
			if (hdev->frl_rate > hdev->tx_max_frl_rate)
				pr_info("Current frl_rate %d is larger than tx_max_frl_rate %d\n",
					hdev->frl_rate, hdev->tx_max_frl_rate);
		}
		edidinfo_attach_to_vinfo(hdev);
		vinfo = get_current_vinfo();
		if (vinfo) {
			vinfo->cur_enc_ppc = 1;
			if (hdev->hwop.cntlmisc(hdev, MISC_GET_FRL_MODE, 0))
				vinfo->cur_enc_ppc = 4;
#ifdef CONFIG_AMLOGIC_DSC
			if (get_dsc_en()) {
				hdev->dsc_en = 1;
				if (hdev->para->cs == HDMI_COLORSPACE_RGB)
					vinfo->vpp_post_out_color_fmt = 1;
				else
					vinfo->vpp_post_out_color_fmt = 0;
			} else {
				vinfo->vpp_post_out_color_fmt = 0;
			}
#endif
			pr_info("vinfo: set cur_enc_ppc as %d, vpp color: %d\n",
				vinfo->cur_enc_ppc, vinfo->vpp_post_out_color_fmt);
		}
	}
	return 0;
}

static enum vmode_e hdmitx_validate_vmode(char *_mode, u32 frac, void *data)
{
	struct hdmi_format_para *para = NULL;
	struct hdmitx_dev *hdev = get_hdmitx21_device();
	char mode[32] = {0};
	char *y420;

	strncpy(mode, _mode, sizeof(mode));
	mode[31] = 0;

	/* if current mode contains string 420, subtract 420 */
	y420 = strstr(mode, "420");
	if (y420)
		*y420 = '\0';

	para = hdmitx21_get_fmtpara(mode, hdev->fmt_attr);

	if (para) {
		/* //remove frac support for vout api
		 *if (frac)
		 *	hdev->frac_rate_policy = 1;
		 *else
		 *	hdev->frac_rate_policy = 0;
		 */
		hdev->para->hdmitx_vinfo.info_3d = NON_3D;
		hdev->para->hdmitx_vinfo.vout_device = &hdmitx_vdev;
		return VMODE_HDMI;
	}
	return VMODE_MAX;
}

static int hdmitx_vmode_is_supported(enum vmode_e mode, void *data)
{
	if ((mode & VMODE_MODE_BIT_MASK) == VMODE_HDMI)
		return true;
	else
		return false;
}

/* static int dsc_dis_dbg; */
static int hdmitx_module_disable(enum vmode_e cur_vmod, void *data)
{
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	fifo_flow_enable_intrs(0);
	hdev->hwop.cntlconfig(hdev, CONF_CLR_AVI_PACKET, 0);
	hdev->hwop.cntlconfig(hdev, CONF_CLR_VSDB_PACKET, 0);
	frl_tx_stop(hdev);
	hdmitx_set_frlrate_none(hdev);
	hdev->hwop.cntlmisc(hdev, MISC_TMDS_PHY_OP, TMDS_PHY_DISABLE);

	/* turn off encp timing gen->disable dsc encoder */
#ifdef CONFIG_AMLOGIC_DSC
	if (hdev->data->chip_type == MESON_CPU_ID_S5) {
		/* during drm sending data, it may plugout hdmi
		 * and disable venc, before enable dummy_mode,
		 * drm side may block on sending data to display
		 * as no vsync.
		 */
		/* hdmitx21_venc_en(0, 1); */
		/* if (dsc_dis_dbg >= 0) { */
		aml_dsc_enable(false);
		hdmitx_dsc_cvtem_pkt_disable();
		/* } */
	}
#endif
	/* hdmitx21_disable_clk(hdev); */
	hdev->para = hdmitx21_get_fmtpara("invalid", hdev->fmt_attr);
	hdmitx_validate_vmode("null", 0, NULL);
	if (hdev->cedst_policy)
		cancel_delayed_work(&hdev->work_cedst);
	if (hdev->rxsense_policy)
		queue_delayed_work(hdev->rxsense_wq, &hdev->work_rxsense, 0);
	hdmitx_unregister_vrr(hdev);

	return 0;
}

static int hdmitx_vout_state;
static int hdmitx_vout_set_state(int index, void *data)
{
	hdmitx_vout_state |= (1 << index);
	return 0;
}

static int hdmitx_vout_clr_state(int index, void *data)
{
	hdmitx_vout_state &= ~(1 << index);
	return 0;
}

static int hdmitx_vout_get_state(void *data)
{
	return hdmitx_vout_state;
}

/* if cs/cd/frac_rate is changed, then return 0 */
static int hdmitx_check_same_vmodeattr(char *name, void *data)
{
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	if (memcmp(hdev->backup_fmt_attr, hdev->fmt_attr, 16) == 0 &&
	    hdev->backup_frac_rate_policy == hdev->frac_rate_policy)
		return 1;
	return 0;
}

static int hdmitx_vout_get_disp_cap(char *buf, void *data)
{
	return disp_cap_show(NULL, NULL, buf);
}

static bool drm_hdmitx_get_vrr_cap(void)
{
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	if (hdev->rxcap.neg_mvrr || hdev->rxcap.fva || hdev->rxcap.vrr_max ||
				hdev->rxcap.vrr_min) {
		pr_info("%s support vrr\n", __func__);
		return true;
	}

	pr_info("%s not support vrr\n", __func__);
	return false;
}

static bool is_vic_supported(enum hdmi_vic brr_vic)
{
	int i;
	struct hdmitx_dev *hdev = get_hdmitx21_device();
	struct rx_cap *prxcap = &hdev->rxcap;

	for (i = 0; i < prxcap->VIC_count; i++) {
		if (brr_vic == prxcap->VIC[i])
			return 1;
	}

	return 0;
}

static void add_vic_to_group(enum hdmi_vic vic, struct drm_vrr_mode_group *group)
{
	const struct hdmi_timing *timing;

	timing = hdmitx21_gettiming_from_vic(vic);
	if (timing && is_vic_supported(vic)) {
		group->brr_vic = vic;
		group->width = timing->h_active;
		group->height = timing->v_active;
		group->vrr_min = 24; /* fixed value */
		group->vrr_max = timing->v_freq / 1000;
	}
}

static int drm_hdmitx_get_vrr_mode_group(struct drm_vrr_mode_group *group, int max_group)
{
	int i;
	enum hdmi_vic brr_vic[] = {
		HDMI_16_1920x1080p60_16x9,
		HDMI_63_1920x1080p120_16x9,
		HDMI_4_1280x720p60_16x9,
		HDMI_47_1280x720p120_16x9,
		HDMI_97_3840x2160p60_16x9,
		HDMI_102_4096x2160p60_256x135,
	};

	if (!drm_hdmitx_get_vrr_cap())
		return 0;

	if (!group || max_group == 0)
		return 0;

	for (i = 0; i < ARRAY_SIZE(brr_vic); i++)
		add_vic_to_group(brr_vic[i], group + i);

	return i;
}

static void hdmitx_set_bist(u32 num, void *data)
{
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	if (hdev->hwop.debug_bist)
		hdev->hwop.debug_bist(hdev, num);
}

static int hdmitx_vout_set_vframe_rate_hint(int duration, void *data)
{
	return hdmitx_set_fr_hint(duration, data);
}

static int hdmitx_vout_get_vframe_rate_hint(void *data)
{
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	if (!hdev)
		return 0;

	return hdev->fr_duration;
}

static struct vout_server_s hdmitx_vout_server = {
	.name = "hdmitx_vout_server",
	.op = {
		.get_vinfo = hdmitx_get_current_vinfo,
		.set_vmode = hdmitx_set_current_vmode,
		.validate_vmode = hdmitx_validate_vmode,
		.check_same_vmodeattr = hdmitx_check_same_vmodeattr,
		.vmode_is_supported = hdmitx_vmode_is_supported,
		.disable = hdmitx_module_disable,
		.set_state = hdmitx_vout_set_state,
		.clr_state = hdmitx_vout_clr_state,
		.get_state = hdmitx_vout_get_state,
		.get_disp_cap = hdmitx_vout_get_disp_cap,
		.set_vframe_rate_hint = hdmitx_vout_set_vframe_rate_hint,
		.get_vframe_rate_hint = hdmitx_vout_get_vframe_rate_hint,
		.set_bist = hdmitx_set_bist,
#ifdef CONFIG_PM
		.vout_suspend = NULL,
		.vout_resume = NULL,
#endif
	},
	.data = NULL,
};
#endif

#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
static struct vout_server_s hdmitx_vout2_server = {
	.name = "hdmitx_vout2_server",
	.op = {
		.get_vinfo = hdmitx_get_current_vinfo,
		.set_vmode = hdmitx_set_current_vmode,
		.validate_vmode = hdmitx_validate_vmode,
		.check_same_vmodeattr = hdmitx_check_same_vmodeattr,
		.vmode_is_supported = hdmitx_vmode_is_supported,
		.disable = hdmitx_module_disable,
		.set_state = hdmitx_vout_set_state,
		.clr_state = hdmitx_vout_clr_state,
		.get_state = hdmitx_vout_get_state,
		.get_disp_cap = hdmitx_vout_get_disp_cap,
		.set_vframe_rate_hint = NULL,
		.get_vframe_rate_hint = NULL,
		.set_bist = hdmitx_set_bist,
#ifdef CONFIG_PM
		.vout_suspend = NULL,
		.vout_resume = NULL,
#endif
	},
	.data = NULL,
};
#endif

#ifdef CONFIG_AMLOGIC_VOUT3_SERVE
static struct vout_server_s hdmitx_vout3_server = {
	.name = "hdmitx_vout3_server",
	.op = {
		.get_vinfo = hdmitx_get_current_vinfo,
		.set_vmode = hdmitx_set_current_vmode,
		.validate_vmode = hdmitx_validate_vmode,
		.check_same_vmodeattr = hdmitx_check_same_vmodeattr,
		.vmode_is_supported = hdmitx_vmode_is_supported,
		.disable = hdmitx_module_disable,
		.set_state = hdmitx_vout_set_state,
		.clr_state = hdmitx_vout_clr_state,
		.get_state = hdmitx_vout_get_state,
		.get_disp_cap = hdmitx_vout_get_disp_cap,
		.set_vframe_rate_hint = NULL,
		.get_vframe_rate_hint = NULL,
		.set_bist = hdmitx_set_bist,
#ifdef CONFIG_PM
		.vout_suspend = NULL,
		.vout_resume = NULL,
#endif
	},
	.data = NULL,
};
#endif

#if IS_ENABLED(CONFIG_AMLOGIC_SND_SOC)

#include <linux/soundcard.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/initval.h>
#include <sound/control.h>

static struct rate_map_fs map_fs[] = {
	{0,	  FS_REFER_TO_STREAM},
	{32000,  FS_32K},
	{44100,  FS_44K1},
	{48000,  FS_48K},
	{88200,  FS_88K2},
	{96000,  FS_96K},
	{176400, FS_176K4},
	{192000, FS_192K},
};

static enum hdmi_audio_fs aud_samp_rate_map(u32 rate)
{
	int i = 0;

	for (i = 0; i < ARRAY_SIZE(map_fs); i++) {
		if (map_fs[i].rate == rate)
			return map_fs[i].fs;
	}
	pr_info("get FS_MAX\n");
	return FS_MAX;
}

u32 aud_sr_idx_to_val(enum hdmi_audio_fs e_sr_idx)
{
	int i = 0;

	for (i = 0; i < ARRAY_SIZE(map_fs); i++) {
		if (map_fs[i].fs == e_sr_idx)
			return map_fs[i].rate / 1000;
	}
	pr_info("wrong idx: %d\n", e_sr_idx);
	return 1;
}

static u8 *aud_type_string[] = {
	"CT_REFER_TO_STREAM",
	"CT_PCM",
	"CT_AC_3",
	"CT_MPEG1",
	"CT_MP3",
	"CT_MPEG2",
	"CT_AAC",
	"CT_DTS",
	"CT_ATRAC",
	"CT_ONE_BIT_AUDIO",
	"CT_DOLBY_D",
	"CT_DTS_HD",
	"CT_MAT",
	"CT_DST",
	"CT_WMA",
	"CT_MAX",
};

static struct size_map aud_size_map_ss[] = {
	{0,	 SS_REFER_TO_STREAM},
	{16,	SS_16BITS},
	{20,	SS_20BITS},
	{24,	SS_24BITS},
	{32,	SS_MAX},
};

static enum hdmi_audio_sampsize aud_size_map(u32 bits)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(aud_size_map_ss); i++) {
		if (bits == aud_size_map_ss[i].sample_bits)
			return aud_size_map_ss[i].ss;
	}
	pr_info("get SS_MAX\n");
	return SS_MAX;
}

static bool hdmitx_set_i2s_mask(char ch_num, char ch_msk)
{
	struct hdmitx_dev *hdev = get_hdmitx21_device();
	static u32 update_flag = -1;

	if (!(ch_num == 2 || ch_num == 4 ||
	      ch_num == 6 || ch_num == 8)) {
		pr_info("err chn setting, must be 2, 4, 6 or 8, Rst as def\n");
		hdev->aud_output_ch = 0;
		if (update_flag != hdev->aud_output_ch) {
			update_flag = hdev->aud_output_ch;
			hdev->hdmi_ch = 0;
		}
		return 0;
	}
	if (ch_msk == 0) {
		pr_info("err chn msk, must larger than 0\n");
		return 0;
	}
	hdev->aud_output_ch = (ch_num << 4) + ch_msk;
	if (update_flag != hdev->aud_output_ch) {
		update_flag = hdev->aud_output_ch;
		hdev->hdmi_ch = 0;
	}
	return 1;
}

static int hdmitx_notify_callback_a(struct notifier_block *block,
				    unsigned long cmd, void *para);
static struct notifier_block hdmitx_notifier_nb_a = {
	.notifier_call	= hdmitx_notify_callback_a,
};

static int hdmitx_notify_callback_a(struct notifier_block *block,
				    unsigned long cmd, void *para)
{
	int i, audio_check = 0;
	struct hdmitx_dev *hdev = get_hdmitx21_device();
	struct rx_cap *prxcap = &hdev->rxcap;
	struct aud_para *aud_param = (struct aud_para *)para;
	struct hdmitx_audpara *audio_param = &hdev->cur_audio_param;
	enum hdmi_audio_fs n_rate = aud_samp_rate_map(aud_param->rate);
	enum hdmi_audio_sampsize n_size = aud_size_map(aud_param->size);

	hdev->audio_param_update_flag = 0;
	hdev->audio_notify_flag = 0;

	if (hdmitx_set_i2s_mask(aud_param->chs, aud_param->i2s_ch_mask))
		hdev->audio_param_update_flag = 1;

	if (audio_param->sample_rate != n_rate) {
		audio_param->sample_rate = n_rate;
		hdev->audio_param_update_flag = 1;
		pr_info("aout notify sample rate: %d\n", n_rate);
	}

	if (audio_param->type != cmd) {
		audio_param->type = cmd;
		pr_info("aout notify format %s\n",
			aud_type_string[audio_param->type & 0xff]);
		hdev->audio_param_update_flag = 1;
	}

	if (audio_param->sample_size != n_size) {
		audio_param->sample_size = n_size;
		hdev->audio_param_update_flag = 1;
		pr_info("aout notify sample size: %d\n", n_size);
	}

	if (audio_param->channel_num != (aud_param->chs - 1)) {
		audio_param->channel_num = aud_param->chs - 1;
		hdev->audio_param_update_flag = 1;
		pr_info("aout notify channel num: %d\n", aud_param->chs);
	}
	if (audio_param->aud_src_if != aud_param->aud_src_if) {
		pr_info("cur aud_src_if %d, new aud_src_if: %d\n",
			audio_param->aud_src_if, aud_param->aud_src_if);
		audio_param->aud_src_if = aud_param->aud_src_if;
		hdev->audio_param_update_flag = 1;
	}
	memcpy(audio_param->status, aud_param->status, sizeof(aud_param->status));
	if (log21_level == 1) {
		for (i = 0; i < sizeof(audio_param->status); i++)
			pr_info("%02x", audio_param->status[i]);
		pr_info("\n");
	}
	if (hdev->tx_aud_cfg == 2) {
		pr_info("auto mode\n");
	/* Detect whether Rx is support current audio format */
	for (i = 0; i < prxcap->AUD_count; i++) {
		if (prxcap->RxAudioCap[i].audio_format_code == cmd)
			audio_check = 1;
	}
	/* sink don't support current audio mode */
	if (!audio_check && cmd != CT_PCM) {
		pr_info("Sink not support this audio format %lu\n",
			cmd);
		hdev->hwop.cntlconfig(hdev,
			CONF_AUDIO_MUTE_OP, AUDIO_MUTE);
		hdev->audio_param_update_flag = 0;
	}
	}
	if (hdev->audio_param_update_flag == 0)
		;
	else
		hdev->audio_notify_flag = 1;

	if ((!(hdev->hdmi_audio_off_flag)) && hdev->audio_param_update_flag) {
		/* plug-in & update audio param */
		if (hdev->hpd_state == 1) {
			hdmitx21_set_audio(hdev,
					 &hdev->cur_audio_param);
			if (hdev->audio_notify_flag == 1 || hdev->audio_step == 1) {
				hdev->audio_notify_flag = 0;
				hdev->audio_step = 0;
			}
			hdev->audio_param_update_flag = 0;
			pr_info("set audio param\n");
		}
	}
	if (aud_param->fifo_rst)
		; /* hdev->hwop.cntlmisc(hdev, MISC_AUDIO_RESET, 1); */

	return 0;
}

#endif

static void hdmitx_get_edid(struct hdmitx_dev *hdev)
{
	mutex_lock(&getedid_mutex);
	/* TODO hdmitx21_edid_ram_buffer_clear(hdev); */
	hdev->hwop.cntlddc(hdev, DDC_RESET_EDID, 0);
	hdev->hwop.cntlddc(hdev, DDC_PIN_MUX_OP, PIN_MUX);
	/* start reading edid first time */
	hdev->hwop.cntlddc(hdev, DDC_EDID_READ_DATA, 0);
	hdev->hwop.cntlddc(hdev, DDC_EDID_GET_DATA, 0);
	if (hdmitx21_check_edid_all_zeros(hdev->EDID_buf)) {
		hdev->hwop.cntlddc(hdev, DDC_GLITCH_FILTER_RESET, 0);
		hdev->hwop.cntlddc(hdev, DDC_EDID_READ_DATA, 0);
		hdev->hwop.cntlddc(hdev, DDC_EDID_GET_DATA, 0);
	}
	/* If EDID is not correct at first time, then retry */
	if (!check21_dvi_hdmi_edid_valid(hdev->EDID_buf)) {
		msleep(100);
		/* start reading edid second time */
		hdev->hwop.cntlddc(hdev, DDC_EDID_READ_DATA, 0);
		hdev->hwop.cntlddc(hdev, DDC_EDID_GET_DATA, 1);
		if (hdmitx21_check_edid_all_zeros(hdev->EDID_buf1)) {
			hdev->hwop.cntlddc(hdev, DDC_GLITCH_FILTER_RESET, 0);
			hdev->hwop.cntlddc(hdev, DDC_EDID_READ_DATA, 0);
			hdev->hwop.cntlddc(hdev, DDC_EDID_GET_DATA, 1);
		}
	}
	hdmitx21_edid_clear(hdev);
	hdmitx21_edid_parse(hdev);
	hdmitx21_edid_buf_compare_print(hdev);

	if (hdev->hdr_priority == 1) { /* clear dv_info */
		struct dv_info *dv = &hdev->rxcap.dv_info;

		memset(dv, 0, sizeof(struct dv_info));
		pr_info("clear dv_info\n");
	}
	if (hdev->hdr_priority == 2) { /* clear dv_info/hdr_info */
		struct dv_info *dv = &hdev->rxcap.dv_info;
		struct hdr_info *hdr = &hdev->rxcap.hdr_info;

		memset(dv, 0, sizeof(struct dv_info));
		memset(hdr, 0, sizeof(struct hdr_info));
		pr_info("clear dv_info/hdr_info\n");
	}
	mutex_unlock(&getedid_mutex);
}

static void hdmitx_rxsense_process(struct work_struct *work)
{
	int sense;
	struct hdmitx_dev *hdev = container_of((struct delayed_work *)work,
		struct hdmitx_dev, work_rxsense);

	sense = hdev->hwop.cntlmisc(hdev, MISC_TMDS_RXSENSE, 0);
	hdmitx21_set_uevent(HDMITX_RXSENSE_EVENT, sense);
	queue_delayed_work(hdev->rxsense_wq, &hdev->work_rxsense, HZ);
}

static void hdmitx_cedst_process(struct work_struct *work)
{
	int ced;
	struct hdmitx_dev *hdev = container_of((struct delayed_work *)work,
		struct hdmitx_dev, work_cedst);

	ced = hdev->hwop.cntlmisc(hdev, MISC_TMDS_CEDST, 0);
	/* firstly send as 0, then real ced, A trigger signal */
	hdmitx21_set_uevent(HDMITX_CEDST_EVENT, 0);
	hdmitx21_set_uevent(HDMITX_CEDST_EVENT, ced);
	queue_delayed_work(hdev->cedst_wq, &hdev->work_cedst, HZ);
	queue_delayed_work(hdev->cedst_wq, &hdev->work_cedst, HZ);
}

static bool is_tv_changed(void)
{
	bool ret = false;
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	if (memcmp(hdmichecksum, hdev->rxcap.chksum, 10) &&
		memcmp(emptychecksum, hdev->rxcap.chksum, 10) &&
		memcmp(invalidchecksum, hdmichecksum, 10)) {
		ret = true;
		pr_info("hdmi crc is diff between uboot and kernel\n");
	}

	return ret;
}

static void hdmitx_hpd_plugin_handler(struct work_struct *work)
{
	struct vinfo_s *info = NULL;
	struct hdmitx_dev *hdev = container_of((struct delayed_work *)work,
		struct hdmitx_dev, work_hpd_plugin);

	mutex_lock(&setclk_mutex);
	mutex_lock(&hdev->hdmimode_mutex);
	hdev->already_used = 1;
	if (!(hdev->hdmitx_event & (HDMI_TX_HPD_PLUGIN))) {
		mutex_unlock(&hdev->hdmimode_mutex);
		mutex_unlock(&setclk_mutex);
		return;
	}
	if (hdev->rxsense_policy) {
		cancel_delayed_work(&hdev->work_rxsense);
		queue_delayed_work(hdev->rxsense_wq, &hdev->work_rxsense, 0);
	}
	pr_info("plugin\n");
	/* there's such case: plugin irq->hdmitx resume + read EDID +
	 * resume uevent->mode setting + hdcp auth->plugin handler read
	 * EDID, now EDID already read done and hdcp already started,
	 * not read EDID again.
	 */
	if (!hdmitx_edid_done) {
		hdev->hwop.cntlmisc(hdev, MISC_I2C_RESET, 0);
		hdmitx_get_edid(hdev);
		hdmitx_edid_done = true;
	}
	hdev->hdmitx_event &= ~HDMI_TX_HPD_PLUGIN;
	/* start reading E-EDID */
	if (hdev->repeater_tx)
		rx_repeat_hpd_state(1);
	hdev->cedst_policy = hdev->cedst_en & hdev->rxcap.scdc_present;
	hdmi_physical_size_update(hdev);
	if (hdev->rxcap.ieeeoui != HDMI_IEEE_OUI)
		hdev->hwop.cntlconfig(hdev,
			CONF_HDMI_DVI_MODE, DVI_MODE);
	else
		hdev->hwop.cntlconfig(hdev,
			CONF_HDMI_DVI_MODE, HDMI_MODE);
	if (hdev->repeater_tx) {
		if (check_fbc_special(&hdev->EDID_buf[0]) ||
		    check_fbc_special(&hdev->EDID_buf1[0]))
			rx_set_repeater_support(0);
		else
			rx_set_repeater_support(1);
	}

	info = hdmitx_get_current_vinfo(NULL);
	if (info && info->mode == VMODE_HDMI)
		hdmitx21_set_audio(hdev, &hdev->cur_audio_param);
	if (plugout_mute_flg) {
		/* 1.TV not changed: just clear avmute and continue output
		 * 2.if TV changed:
		 * keep avmute (will be cleared by systemcontrol);
		 * clear pkt, packets need to be cleared, otherwise,
		 * if plugout from DV/HDR TV, and plugin to non-DV/HDR
		 * TV, packets may not be cleared. pkt sending will
		 * be callbacked later after vinfo attached.
		 */
		if (is_cur_tmds_div40(hdev))
			hdmitx_resend_div40(hdev);
		if (!is_tv_changed()) {
			hdev->hwop.cntlmisc(hdev, MISC_AVMUTE_OP,
					    CLR_AVMUTE);
		} else {
			/* keep avmute & clear pkt */
			hdmitx_set_vsif_pkt(0, 0, NULL, true);
			hdmitx_set_hdr10plus_pkt(0, NULL);
			hdmitx_set_drm_pkt(NULL);
		}
		edidinfo_attach_to_vinfo(hdev);
		plugout_mute_flg = false;
	}
	hdev->hpd_state = 1;
	if (hdev->tv_usage == 0)
		hdmitx_notify_hpd(hdev->hpd_state,
				  hdev->edid_parsing ?
				  hdev->edid_ptr : NULL);
	/* under early suspend, only update uevent state, not
	 * post to system, in case old android system will
	 * set hdmi mode
	 */
	if (hdev->suspend_flag) {
		hdmitx21_set_uevent_state(HDMITX_HPD_EVENT, 1);
		hdmitx21_set_uevent_state(HDMITX_AUDIO_EVENT, 1);
	} else {
		hdmitx21_set_uevent(HDMITX_HPD_EVENT, 1);
		hdmitx21_set_uevent(HDMITX_AUDIO_EVENT, 1);
	}

	/* Should be started at end of output */
	cancel_delayed_work(&hdev->work_cedst);
	if (hdev->cedst_policy)
		queue_delayed_work(hdev->cedst_wq, &hdev->work_cedst, 0);
	mutex_unlock(&hdev->hdmimode_mutex);
	mutex_unlock(&setclk_mutex);
	/*notify to drm hdmi*/
	if (!hdev->suspend_flag && hdmitx21_device.drm_hpd_cb.callback)
		hdmitx21_device.drm_hpd_cb.callback(hdmitx21_device.drm_hpd_cb.data);
}

static void clear_rx_vinfo(struct hdmitx_dev *hdev)
{
	struct vinfo_s *info = hdmitx_get_current_vinfo(NULL);

	if (info) {
		memset(&info->hdr_info, 0, sizeof(info->hdr_info));
		memset(&info->rx_latency, 0, sizeof(info->rx_latency));
	}
}

static void hdmitx_aud_hpd_plug_handler(struct work_struct *work)
{
	int st;
	struct hdmitx_dev *hdev = container_of((struct delayed_work *)work,
		struct hdmitx_dev, work_aud_hpd_plug);

	st = hdev->hwop.cntlmisc(hdev, MISC_HPD_GPI_ST, 0);
	pr_info("%s state:%d\n", __func__, st);
	hdmitx21_set_uevent(HDMITX_AUDIO_EVENT, st);
}

static void hdmitx_hpd_plugout_handler(struct work_struct *work)
{
	struct hdmitx_dev *hdev = container_of((struct delayed_work *)work,
		struct hdmitx_dev, work_hpd_plugout);
	struct hdcp_t *p_hdcp = (struct hdcp_t *)hdev->am_hdcp;

	mutex_lock(&setclk_mutex);
	mutex_lock(&hdev->hdmimode_mutex);
	if (!(hdev->hdmitx_event & (HDMI_TX_HPD_PLUGOUT))) {
		mutex_unlock(&hdev->hdmimode_mutex);
		mutex_unlock(&setclk_mutex);
		return;
	}
	hdmitx_vrr_disable();
	if (hdev->cedst_policy)
		cancel_delayed_work(&hdev->work_cedst);
	edidinfo_detach_to_vinfo(hdev);
	frl_tx_stop(hdev);
	rx_hdcp2_ver = 0;
	hdev->dw_hdcp22_cap = false;
	is_passthrough_switch = 0;
	pr_info("plugout\n");
	/* when plugout before systemcontrol boot, setavmute
	 * but keep output not changed, and wait for plugin
	 * NOTE: TV maybe changed(such as DV <-> non-DV)
	 */
	if (!hdev->systemcontrol_on &&
		hdmitx21_uboot_already_display(hdev)) {
		plugout_mute_flg = true;
		/* edidinfo_detach_to_vinfo(hdev); */
		clear_rx_vinfo(hdev);
		hdev->hdmitx_event &= ~HDMI_TX_HPD_PLUGOUT;
		hdmitx21_edid_clear(hdev);
		hdmi_physical_size_update(hdev);
		hdmitx21_edid_ram_buffer_clear(hdev);
		hdmitx_edid_done = false;
		hdev->hpd_state = 0;
		if (hdev->tv_usage == 0) {
			rx_edid_physical_addr(0, 0, 0, 0);
			hdmitx_notify_hpd(hdev->hpd_state, NULL);
		}
		hdev->hwop.cntlmisc(hdev, MISC_AVMUTE_OP, SET_AVMUTE);
		hdmitx21_set_uevent(HDMITX_HPD_EVENT, 0);
		hdmitx21_set_uevent(HDMITX_AUDIO_EVENT, 0);
		mutex_unlock(&hdev->hdmimode_mutex);
		mutex_unlock(&setclk_mutex);
		/*notify to drm hdmi*/
		if (hdmitx21_device.drm_hpd_cb.callback)
			hdmitx21_device.drm_hpd_cb.callback(hdmitx21_device.drm_hpd_cb.data);
		return;
	}
	/*after plugout, DV mode can't be supported*/
	hdmitx_set_vsif_pkt(0, 0, NULL, true);
	hdmitx_set_hdr10plus_pkt(0, NULL);
	/* clear any VSIF packet left over because of vendor<->vendor2 switch */
	hdmi_vend_infoframe_rawset(NULL, NULL);
	/* stop ALLM packet by hdmitx itself? or stop by upstream
	 * hdmirx side(in hdmirx channel) / hwc when stop playing
	 * ALLM video(online stream)?
	 */
	/* dolby vision CTS case91: clear HF-VSIF for safety */
	hdmi_vend_infoframe2_rawset(NULL, NULL);
	hdev->ready = 0;
	if (hdev->repeater_tx)
		rx_repeat_hpd_state(0);
	hdev->hwop.cntlconfig(hdev, CONF_CLR_AVI_PACKET, 0);
	hdev->hwop.cntlmisc(hdev, MISC_TMDS_PHY_OP, TMDS_PHY_DISABLE);
	hdev->hwop.cntlddc(hdev, DDC_HDCP_SET_TOPO_INFO, 0);
	hdev->hdmitx_event &= ~HDMI_TX_HPD_PLUGOUT;
	hdmitx21_disable_hdcp(hdev);
	hdmitx21_rst_stream_type(hdev->am_hdcp);
	p_hdcp->saved_upstream_type = 0;
	p_hdcp->rx_update_flag = 0;
	clear_rx_vinfo(hdev);
	hdmitx21_edid_clear(hdev);
	hdmi_physical_size_update(hdev);
	hdmitx21_edid_ram_buffer_clear(hdev);
	hdmitx_edid_done = false;
	hdev->hpd_state = 0;
	hdev->pre_tmds_clk_div40 = false;
	hdev->ll_enabled_in_auto_mode = false;
	if (hdev->tv_usage == 0) {
		rx_edid_physical_addr(0, 0, 0, 0);
		hdmitx_notify_hpd(hdev->hpd_state, NULL);
	}
	/* clear audio/video mute flag of stream type */
	hdmitx21_video_mute_op(1, VIDEO_MUTE_PATH_2);
	hdmitx21_audio_mute_op(1, AUDIO_MUTE_PATH_3);
	/* under early suspend, only update uevent state, not
	 * post to system, in case old android system will
	 * set hdmi mode
	 */
	if (hdev->suspend_flag) {
		hdmitx21_set_uevent_state(HDMITX_HPD_EVENT, 0);
		hdmitx21_set_uevent_state(HDMITX_AUDIO_EVENT, 0);
	} else {
		hdmitx21_set_uevent(HDMITX_HPD_EVENT, 0);
		hdmitx21_set_uevent(HDMITX_AUDIO_EVENT, 0);
	}
	mutex_unlock(&hdev->hdmimode_mutex);
	mutex_unlock(&setclk_mutex);
	/*notify to drm hdmi*/
	if (hdmitx21_device.drm_hpd_cb.callback)
		hdmitx21_device.drm_hpd_cb.callback(hdmitx21_device.drm_hpd_cb.data);
}

int get21_hpd_state(void)
{
	int ret;
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	mutex_lock(&setclk_mutex);
	ret = hdev->hpd_state;
	mutex_unlock(&setclk_mutex);

	return ret;
}

/******************************
 *  hdmitx kernel task
 *******************************/

static bool is_cur_tmds_div40(struct hdmitx_dev *hdev)
{
	const struct hdmi_timing *tp;
	const char *name;
	unsigned int act_clk = 0;

	if (!hdev)
		return false;
	tp = hdmitx21_gettiming_from_vic(hdev->cur_VIC);
	if (tp) {
		name = tp->sname ? tp->sname : tp->name;
		hdev->para = hdmitx21_get_fmtpara(name,
			hdev->fmt_attr);
	}
	if (!hdev->para)
		return false;
	act_clk = hdev->para->tmds_clk / 1000;
	pr_info("hdmitx: get vic %d cscd %s act_clk %d\n",
		hdev->cur_VIC, hdev->fmt_attr, act_clk);

	if (act_clk > 340)
		return true;
	return false;
}

static void hdmitx_resend_div40(struct hdmitx_dev *hdev)
{
	hdev->hwop.cntlddc(hdev, DDC_SCDC_DIV40_SCRAMB, 1);
}

/*****************************
 *	hdmitx driver file_operations
 *
 ******************************/
static int amhdmitx_open(struct inode *node, struct file *file)
{
	struct hdmitx_dev *hdmitx_in_devp;

	/* Get the per-device structure that contains this cdev */
	hdmitx_in_devp = container_of(node->i_cdev, struct hdmitx_dev, cdev);
	file->private_data = hdmitx_in_devp;

	return 0;
}

static int amhdmitx_release(struct inode *node, struct file *file)
{
	return 0;
}

static const struct file_operations amhdmitx_fops = {
	.owner	= THIS_MODULE,
	.open	 = amhdmitx_open,
	.release = amhdmitx_release,
};

static int get_dt_vend_init_data(struct device_node *np,
				 struct vendor_info_data *vend)
{
	int ret;

	ret = of_property_read_string(np, "vendor_name",
				      (const char **)&vend->vendor_name);
	if (ret)
		pr_info("not find vendor name\n");

	ret = of_property_read_u32(np, "vendor_id", &vend->vendor_id);
	if (ret)
		pr_info("not find vendor id\n");

	ret = of_property_read_string(np, "product_desc",
				      (const char **)&vend->product_desc);
	if (ret)
		pr_info("not find product desc\n");
	return 0;
}

void hdmitx21_fmt_attr(struct hdmitx_dev *hdev)
{
	if (strlen(hdev->fmt_attr) >= 8) {
		pr_info("fmt_attr %s\n", hdev->fmt_attr);
		return;
	}
	memset(hdev->fmt_attr, 0, sizeof(hdev->fmt_attr));
	switch (hdev->para->cs) {
	case HDMI_COLORSPACE_RGB:
		memcpy(hdev->fmt_attr, "rgb,", 5);
		break;
	case HDMI_COLORSPACE_YUV422:
		memcpy(hdev->fmt_attr, "422,", 5);
		break;
	case HDMI_COLORSPACE_YUV444:
		memcpy(hdev->fmt_attr, "444,", 5);
		break;
	case HDMI_COLORSPACE_YUV420:
		memcpy(hdev->fmt_attr, "420,", 5);
		break;
	default:
		break;
	}
	switch (hdev->para->cd) {
	case COLORDEPTH_24B:
		strcat(hdev->fmt_attr, "8bit");
		break;
	case COLORDEPTH_30B:
		strcat(hdev->fmt_attr, "10bit");
		break;
	case COLORDEPTH_36B:
		strcat(hdev->fmt_attr, "12bit");
		break;
	case COLORDEPTH_48B:
		strcat(hdev->fmt_attr, "16bit");
		break;
	default:
		break;
	}
	pr_info("fmt_attr %s\n", hdev->fmt_attr);
}

static void hdmitx_init_fmt_attr(struct hdmitx_dev *hdev)
{
	if (!hdev->para)
		return;
	if (strlen(hdev->fmt_attr) >= 8) {
		pr_info("fmt_attr %s\n", hdev->fmt_attr);
		return;
	}
	memset(hdev->fmt_attr, 0, sizeof(hdev->fmt_attr));
	switch (hdev->para->cs) {
	case HDMI_COLORSPACE_RGB:
		memcpy(hdev->fmt_attr, "rgb,", 5);
		break;
	case HDMI_COLORSPACE_YUV422:
		memcpy(hdev->fmt_attr, "422,", 5);
		break;
	case HDMI_COLORSPACE_YUV444:
		memcpy(hdev->fmt_attr, "444,", 5);
		break;
	case HDMI_COLORSPACE_YUV420:
		memcpy(hdev->fmt_attr, "420,", 5);
		break;
	default:
		break;
	}
	switch (hdev->para->cd) {
	case COLORDEPTH_24B:
		strcat(hdev->fmt_attr, "8bit");
		break;
	case COLORDEPTH_30B:
		strcat(hdev->fmt_attr, "10bit");
		break;
	case COLORDEPTH_36B:
		strcat(hdev->fmt_attr, "12bit");
		break;
	case COLORDEPTH_48B:
		strcat(hdev->fmt_attr, "16bit");
		break;
	default:
		break;
	}
	pr_debug("fmt_attr %s\n", hdev->fmt_attr);
}

/* for notify to cec */
static BLOCKING_NOTIFIER_HEAD(hdmitx21_event_notify_list);
int hdmitx21_event_notifier_regist(struct notifier_block *nb)
{
	int ret = 0;
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	if (!nb)
		return ret;

	ret = blocking_notifier_chain_register(&hdmitx21_event_notify_list, nb);
	/* update status when register */
	if (!ret && nb->notifier_call) {
		if (hdev->tv_usage == 0)
			hdmitx_notify_hpd(hdev->hpd_state,
				  hdev->edid_parsing ?
				  hdev->edid_ptr : NULL);
		if (hdev->physical_addr != 0xffff) {
			if (hdev->tv_usage == 0)
				hdmitx21_event_notify(HDMITX_PHY_ADDR_VALID,
					    &hdev->physical_addr);
		}
	}

	return ret;
}

int hdmitx21_event_notifier_unregist(struct notifier_block *nb)
{
	int ret;
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	if (hdev->hdmi_init != 1)
		return 1;

	ret = blocking_notifier_chain_unregister(&hdmitx21_event_notify_list,
						 nb);

	return ret;
}

void hdmitx21_event_notify(unsigned long state, void *arg)
{
	blocking_notifier_call_chain(&hdmitx21_event_notify_list, state, arg);
}

void hdmitx21_hdcp_status(int hdmi_authenticated)
{
	hdmitx21_set_uevent(HDMITX_HDCP_EVENT, hdmi_authenticated);
}

static void hdmitx_init_parameters(struct hdmitx_info *info)
{
	memset(info, 0, sizeof(struct hdmitx_info));

	info->video_out_changing_flag = 1;

	info->audio_flag = 1;
	info->audio_info.type = CT_REFER_TO_STREAM;
	info->audio_info.format = AF_I2S;
	info->audio_info.fs = FS_44K1;
	info->audio_info.ss = SS_16BITS;
	info->audio_info.channels = CC_2CH;
	info->audio_out_changing_flag = 1;
}

static void hdmitx_hdr_state_init(struct hdmitx_dev *hdev)
{
	enum hdmi_tf_type hdr_type = HDMI_NONE;
	unsigned int colorimetry = 0;
	unsigned int hdr_mode = 0;

	hdr_type = hdmitx21_get_cur_hdr_st();
	colorimetry = get_extended_colorimetry_from_avi(hdev);
	/* 1:standard HDR, 2:non-standard, 3:HLG, 0:other */
	if (hdr_type == HDMI_HDR_SMPTE_2084) {
		if (colorimetry == HDMI_EXTENDED_COLORIMETRY_BT2020)
			hdr_mode = 1;
		else
			hdr_mode = 2;
	} else if (hdr_type == HDMI_HDR_HLG) {
		if (colorimetry == HDMI_EXTENDED_COLORIMETRY_BT2020)
			hdr_mode = 3;
	} else {
		hdr_mode = 0;
	}

	hdev->hdmi_last_hdr_mode = hdr_mode;
	hdev->hdmi_current_hdr_mode = hdr_mode;
}

static int amhdmitx21_device_init(struct hdmitx_dev *hdmi_dev)
{
	struct hdmitx_dev *hdev = get_hdmitx21_device();
	static struct hdmi_format_para para;

	if (!hdmi_dev)
		return 1;

	memset(&para, 0, sizeof(para));
	pr_info("Ver: %s\n", HDMITX_VER);

	hdmi_dev->hdtx_dev = NULL;

	hdev->para = &para;
	hdev->physical_addr = 0xffff;
	hdev->hdmi_last_hdr_mode = 0;
	hdev->hdmi_current_hdr_mode = 0;
	hdev->unplug_powerdown = 0;
	hdev->vic_count = 0;
	hdev->force_audio_flag = 0;
	hdev->ready = 0;
	hdev->systemcontrol_on = 0;
	hdev->rxsense_policy = 0; /* no RxSense by default */
	/* enable or disable HDMITX SSPLL, enable by default */
	hdev->sspll = 1;
	/*
	 * 0, do not unmux hpd when off or unplug ;
	 * 1, unmux hpd when unplug;
	 * 2, unmux hpd when unplug  or off;
	 */
	hdev->hpdmode = 1;

	hdev->flag_3dfp = 0;
	hdev->flag_3dss = 0;
	hdev->flag_3dtb = 0;
	hdev->def_stream_type = DEFAULT_STREAM_TYPE;
	if ((init_flag & INIT_FLAG_POWERDOWN) &&
	    hdev->hpdmode == 2)
		hdev->mux_hpd_if_pin_high_flag = 0;
	else
		hdev->mux_hpd_if_pin_high_flag = 1;

	hdev->audio_param_update_flag = 0;
	/* 1: 2ch */
	hdev->hdmi_ch = 1;
	/* default audio configure is on */
	hdev->tx_aud_cfg = 1;
	hdmitx_init_parameters(&hdev->hdmi_info);
	hdev->need_filter_hdcp_off = false;
	/* default 6S */
	hdev->filter_hdcp_off_period = 6;
	hdev->not_restart_hdcp = false;
	/* wait for upstream start hdcp auth 5S */
	hdev->up_hdcp_timeout_sec = 5;
	hdev->debug_param.avmute_frame = 0;

	return 0;
}

static int amhdmitx_get_dt_info(struct platform_device *pdev)
{
	int ret = 0;
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	struct pinctrl *pin;
	//const char *pin_name;
	//const struct of_device_id *of_id;

#ifdef CONFIG_OF
	int val;
	phandle phandler;
	struct device_node *init_data;
	const struct of_device_id *match;
#endif

	match = of_match_device(meson_amhdmitx_of_match, &pdev->dev);
	if (!match) {
		pr_info("unable to get matched device\n");
		return -1;
	}
	pr_info("get matched device\n");
	//hdev->data = match->data;

	/* pinmux set */
	if (pdev->dev.of_node) {
		pin = devm_pinctrl_get(&pdev->dev);
		if (!pin) {
			pr_info("get pin control fail\n");
			return -1;
		}

		hdev->pinctrl_default = pinctrl_lookup_state(pin, "hdmitx_hpd");
		pinctrl_select_state(pin, hdev->pinctrl_default);

		hdev->pinctrl_i2c = pinctrl_lookup_state(pin, "hdmitx_ddc");
		pinctrl_select_state(pin, hdev->pinctrl_i2c);
		/* rx_pr("hdmirx: pinmux:%p, name:%s\n", */
		/* pin, pin_name); */
		pr_info("get pin control\n");

		/* rx_pr("hdmirx: pinmux:%p, name:%s\n", */
		/* pin, pin_name); */
	} else {
		pr_info("node null\n");
	}

#ifdef CONFIG_OF
	if (pdev->dev.of_node) {
		int dongle_mode = 0;
		int pxp_mode = 0;

		memset(&hdev->config_data, 0,
		       sizeof(struct hdmi_config_platform_data));
		/* Get chip type and name information */
		match = of_match_device(meson_amhdmitx_of_match, &pdev->dev);

		if (!match) {
			pr_info("%s: no match table\n", __func__);
			return -1;
		}
		hdev->data = (struct amhdmitx_data_s *)match->data;

		pr_info("chip_type:%d chip_name:%s\n",
			hdev->data->chip_type,
			hdev->data->chip_name);

		/* Get pxp_mode information */
		ret = of_property_read_u32(pdev->dev.of_node, "pxp_mode",
					   &pxp_mode);
		hdev->pxp_mode = pxp_mode;
		if (!ret)
			pr_info("hdev->pxp_mode: %d\n", hdev->pxp_mode);

		/* Get dongle_mode information */
		ret = of_property_read_u32(pdev->dev.of_node, "dongle_mode",
					   &dongle_mode);
		hdev->dongle_mode = !!dongle_mode;
		if (!ret)
			pr_info("hdev->dongle_mode: %d\n",
				hdev->dongle_mode);
		/* Get repeater_tx information */
		ret = of_property_read_u32(pdev->dev.of_node,
					   "repeater_tx", &val);
		if (!ret)
			hdev->repeater_tx = val;
		else
			hdev->repeater_tx = 0;
		/* Get repeater_tx information */
		ret = of_property_read_u32(pdev->dev.of_node,
					   "tv_usage", &val);
		if (!ret)
			hdev->tv_usage = val;
		else
			hdev->tv_usage = 0;
		/* if for tv usage, then should not support hdcp repeater */
		if (hdev->tv_usage == 1)
			hdev->repeater_tx = 0;

		ret = of_property_read_u32(pdev->dev.of_node,
					   "cedst_en", &val);
		if (!ret)
			hdev->cedst_en = !!val;

		ret = of_property_read_u32(pdev->dev.of_node, "hdr_8bit_en", &val);
		if (!ret)
			hdev->hdr_8bit_en = !!val;

		/* not support FRL by default, unless enabled in dts */
		hdev->tx_max_frl_rate = FRL_NONE;
		ret = of_property_read_u32(pdev->dev.of_node, "tx_max_frl_rate", &val);
		if (!ret) {
			if (val > FRL_12G4L)
				pr_info("wrong tx_max_frl_rate %d\n", val);
			else
				hdev->tx_max_frl_rate = val;
		}
		ret = of_property_read_u32(pdev->dev.of_node,
					   "hdcp_type_policy", &val);
		if (!ret) {
			if (val == 2)
				;
			if (val == 1)
				;
		}
		ret = of_property_read_u32(pdev->dev.of_node,
					   "enc_idx", &val);
		hdev->enc_idx = 0; /* default 0 */
		if (!ret) {
			if (val == 2)
				hdev->enc_idx = 2;
		}

		ret = of_property_read_u32(pdev->dev.of_node, "vrr_type", &val);
		hdev->vrr_type = 0; /* default 0 */
		if (!ret) {
			if (val == 1 || val == 2)
				hdev->vrr_type = val;
		}

		/* hdcp ctrl 0:sysctrl, 1: drv, 2: linux app */
		ret = of_property_read_u32(pdev->dev.of_node,
			   "hdcp_ctl_lvl", &hdmitx21_device.hdcp_ctl_lvl);
		pr_info("hdcp_ctl_lvl[%d-%d]\n", hdmitx21_device.hdcp_ctl_lvl, ret);

		if (ret)
			hdmitx21_device.hdcp_ctl_lvl = 0;

		/* Get vendor information */
		ret = of_property_read_u32(pdev->dev.of_node,
					   "vend-data", &val);
		if (ret)
			pr_info("not find match init-data\n");
		if (ret == 0) {
			phandler = val;
			init_data = of_find_node_by_phandle(phandler);
			if (!init_data)
				pr_info("not find device node\n");
			hdev->config_data.vend_data =
			kzalloc(sizeof(struct vendor_info_data), GFP_KERNEL);
			if (!(hdev->config_data.vend_data))
				pr_info("not allocate memory\n");
			ret = get_dt_vend_init_data
			(init_data, hdev->config_data.vend_data);
			if (ret)
				pr_info("not find vend_init_data\n");
		}
		/* Get power control */
		ret = of_property_read_u32(pdev->dev.of_node,
					   "pwr-ctrl", &val);
		if (ret)
			pr_info("not find match pwr-ctl\n");
		if (ret == 0) {
			phandler = val;
			init_data = of_find_node_by_phandle(phandler);
			if (!init_data)
				pr_info("not find device node\n");
			hdev->config_data.pwr_ctl = kzalloc((sizeof(struct hdmi_pwr_ctl)) *
				HDMI_TX_PWR_CTRL_NUM, GFP_KERNEL);
			if (!hdev->config_data.pwr_ctl)
				pr_info("can not get pwr_ctl mem\n");
			else
				memset(hdev->config_data.pwr_ctl, 0, sizeof(struct hdmi_pwr_ctl));
			if (ret)
				pr_info("not find pwr_ctl\n");
		}

		/* Get reg information */
		ret = hdmitx21_init_reg_map(pdev);
		if (ret < 0)
			pr_err("ERROR: hdmitx io_remap fail!\n");
	}

#else
		hdmi_pdata = pdev->dev.platform_data;
		if (!hdmi_pdata) {
			pr_info("not get platform data\n");
			r = -ENOENT;
		} else {
			pr_info("get hdmi platform data\n");
		}
#endif
	hdev->irq_hpd = platform_get_irq_byname(pdev, "hdmitx_hpd");
	if (hdev->irq_hpd == -ENXIO) {
		pr_err("%s: ERROR: hdmitx hpd irq No not found\n",
		       __func__);
			return -ENXIO;
	}
	pr_info("hpd irq = %d\n", hdev->irq_hpd);
	tx_vrr_params_init();
	hdev->irq_vrr_vsync = platform_get_irq_byname(pdev, "vrr_vsync");
	if (hdev->irq_vrr_vsync == -ENXIO) {
		pr_err("%s: ERROR: hdmitx vrr_vsync irq No not found\n",
		       __func__);
			return -ENXIO;
	}
	pr_info("vrr vsync irq = %d\n", hdev->irq_vrr_vsync);
	ret = of_property_read_u32(pdev->dev.of_node, "arc_rx_en", &val);
	if (!ret)
		hdev->arc_rx_en = val;
	else
		hdev->arc_rx_en = 0;
	return ret;
}

/*
 * amhdmitx_clktree_probe
 * get clktree info from dts
 */
static void amhdmitx_clktree_probe(struct device *hdmitx_dev)
{
	struct hdmitx_dev *hdev = get_hdmitx21_device();
	struct clk *hdmi_clk_vapb, *hdmi_clk_vpu;
	struct clk *venci_top_gate, *venci_0_gate, *venci_1_gate;

	hdmi_clk_vapb = devm_clk_get(hdmitx_dev, "hdmi_vapb_clk");
	if (IS_ERR(hdmi_clk_vapb)) {
		pr_warn("vapb_clk failed to probe\n");
	} else {
		hdev->hdmitx_clk_tree.hdmi_clk_vapb = hdmi_clk_vapb;
		clk_prepare_enable(hdev->hdmitx_clk_tree.hdmi_clk_vapb);
	}

	hdmi_clk_vpu = devm_clk_get(hdmitx_dev, "hdmi_vpu_clk");
	if (IS_ERR(hdmi_clk_vpu)) {
		pr_warn("vpu_clk failed to probe\n");
	} else {
		hdev->hdmitx_clk_tree.hdmi_clk_vpu = hdmi_clk_vpu;
		clk_prepare_enable(hdev->hdmitx_clk_tree.hdmi_clk_vpu);
	}

	venci_top_gate = devm_clk_get(hdmitx_dev, "venci_top_gate");
	if (IS_ERR(venci_top_gate))
		pr_warn("venci_top_gate failed to probe\n");
	else
		hdev->hdmitx_clk_tree.venci_top_gate = venci_top_gate;

	venci_0_gate = devm_clk_get(hdmitx_dev, "venci_0_gate");
	if (IS_ERR(venci_0_gate))
		pr_warn("venci_0_gate failed to probe\n");
	else
		hdev->hdmitx_clk_tree.venci_0_gate = venci_0_gate;

	venci_1_gate = devm_clk_get(hdmitx_dev, "venci_1_gate");
	if (IS_ERR(venci_1_gate))
		pr_warn("venci_1_gate failed to probe\n");
	else
		hdev->hdmitx_clk_tree.venci_1_gate = venci_1_gate;
}

void amhdmitx21_vpu_dev_register(struct hdmitx_dev *hdev)
{
	hdev->hdmitx_vpu_clk_gate_dev =
	vpu_dev_register(VPU_VENCI, DEVICE_NAME);
}

static void amhdmitx_infoframe_init(struct hdmitx_dev *hdev)
{
	int ret = 0;

	ret = hdmi_vendor_infoframe_init(&hdev->infoframes.vend.vendor.hdmi);
	if (!ret)
		pr_info("%s[%d] init vendor infoframe failed %d\n", __func__, __LINE__, ret);
	ret = hdmi_avi_infoframe_init(&hdev->infoframes.avi.avi);
	if (ret)
		pr_info("init avi infoframe failed\n");
	// TODO, panic
	// hdmi_spd_infoframe_init(&hdev->infoframes.spd.spd,
	//	hdev->config_data.vend_data->vendor_name,
	//	hdev->config_data.vend_data->product_desc);
	hdmi_audio_infoframe_init(&hdev->infoframes.aud.audio);
	hdmi_drm_infoframe_init(&hdev->infoframes.drm.drm);
}

static int hdmitx21_status_check(void *data)
{
	int clk[3];
	int idx[3];
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	if (hdev->data && hdev->data->chip_type != MESON_CPU_ID_S5)
		return 0;

	/* for S5, here need check the clk index 89 & 16 */
	idx[0] = 92; /* cts_htx_tmds_clk */
	idx[1] = 16; /* vid_pll0_clk */
	idx[2] = 89; /* htx_tmds20_clk */

	while (1) {
		msleep_interruptible(1000);
		if (!hdev->ready)
			continue;
		/* skip FRL mode */
		if (hdev->hwop.cntlmisc(hdev, MISC_GET_FRL_MODE, 0))
			continue;
		clk[0] = meson_clk_measure(idx[0]);
		clk[1] = meson_clk_measure(idx[1]);
		if (clk[0] && clk[1])
			continue;

		if (!clk[0]) {
			pr_debug("the clock[%d] is %d\n", idx[0], clk[0]);
			hdev->hwop.cntlmisc(hdev, MISC_CLK_DIV_RST, idx[0]);
			pr_debug("reset the clock div for %d\n", idx[0]);
			pr_info("the clock[%d] is %d\n", idx[0], meson_clk_measure(idx[0]));
		}
		if (!clk[1]) {
			pr_debug("the clock[%d] is %d\n", idx[1], clk[1]);
			hdev->hwop.cntlmisc(hdev, MISC_CLK_DIV_RST, idx[1]);
			pr_debug("reset the clock div for %d\n", idx[1]);
			pr_info("the clock[%d] is %d\n", idx[1], meson_clk_measure(idx[1]));
		}
		/* resend the SCDC/DIV40 config */
		if (!clk[0] || !clk[1]) {
			clk[0] = meson_clk_measure(idx[0]);
			if (clk[0] >= 340000000)
				hdev->hwop.cntlddc(hdev, DDC_SCDC_DIV40_SCRAMB, 1);
			else
				hdev->hwop.cntlddc(hdev, DDC_SCDC_DIV40_SCRAMB, 0);
		}
	}
	return 0;
}

static int amhdmitx_probe(struct platform_device *pdev)
{
	int r, ret = 0;
	struct device *dev;
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	pr_debug("%s start\n", __func__);

	amhdmitx21_device_init(hdev);
	amhdmitx_infoframe_init(hdev);

	ret = amhdmitx_get_dt_info(pdev);
	/* if (ret) */
		/* return ret; */

	amhdmitx_clktree_probe(&pdev->dev);
	if (0) /* TODO */
		amhdmitx21_vpu_dev_register(hdev);

	r = alloc_chrdev_region(&hdev->hdmitx_id, 0, HDMI_TX_COUNT,
				DEVICE_NAME);
	cdev_init(&hdev->cdev, &amhdmitx_fops);
	hdev->cdev.owner = THIS_MODULE;
	r = cdev_add(&hdev->cdev, hdev->hdmitx_id, HDMI_TX_COUNT);

	hdmitx_class = class_create(THIS_MODULE, "amhdmitx");
	if (IS_ERR(hdmitx_class)) {
		unregister_chrdev_region(hdev->hdmitx_id, HDMI_TX_COUNT);
		return -1;
	}

	dev = device_create(hdmitx_class, NULL, hdev->hdmitx_id, NULL,
			    "amhdmitx%d", 0); /* kernel>=2.6.27 */

	if (!dev) {
		pr_info("device_create create error\n");
		class_destroy(hdmitx_class);
		r = -EEXIST;
		return r;
	}
	hdev->hdtx_dev = dev;
	ret = device_create_file(dev, &dev_attr_disp_mode);
	ret = device_create_file(dev, &dev_attr_attr);
	ret = device_create_file(dev, &dev_attr_vid_mute);
	ret = device_create_file(dev, &dev_attr_edid);
	ret = device_create_file(dev, &dev_attr_rawedid);
	ret = device_create_file(dev, &dev_attr_sink_type);
	ret = device_create_file(dev, &dev_attr_edid_parsing);
	ret = device_create_file(dev, &dev_attr_config);
	ret = device_create_file(dev, &dev_attr_debug);
	ret = device_create_file(dev, &dev_attr_disp_cap);
	ret = device_create_file(dev, &dev_attr_vrr_cap);
	ret = device_create_file(dev, &dev_attr_preferred_mode);
	ret = device_create_file(dev, &dev_attr_cea_cap);
	ret = device_create_file(dev, &dev_attr_vesa_cap);
	ret = device_create_file(dev, &dev_attr_aud_cap);
	ret = device_create_file(dev, &dev_attr_lipsync_cap);
	ret = device_create_file(dev, &dev_attr_hdmi_hdr_status);
	ret = device_create_file(dev, &dev_attr_hdr_cap);
	ret = device_create_file(dev, &dev_attr_hdr_cap2);
	ret = device_create_file(dev, &dev_attr_dv_cap);
	ret = device_create_file(dev, &dev_attr_dv_cap2);
	ret = device_create_file(dev, &dev_attr_avmute);
	ret = device_create_file(dev, &dev_attr_aud_mute);
	ret = device_create_file(dev, &dev_attr_phy);
	ret = device_create_file(dev, &dev_attr_frac_rate_policy);
	ret = device_create_file(dev, &dev_attr_sspll);
	ret = device_create_file(dev, &dev_attr_rxsense_policy);
	ret = device_create_file(dev, &dev_attr_rxsense_state);
	ret = device_create_file(dev, &dev_attr_cedst_policy);
	ret = device_create_file(dev, &dev_attr_cedst_count);
	ret = device_create_file(dev, &dev_attr_hdcp_mode);
	ret = device_create_file(dev, &dev_attr_hdcp_ver);
	ret = device_create_file(dev, &dev_attr_hdcp_type_policy);
	ret = device_create_file(dev, &dev_attr_hdmi_repeater_tx);
	ret = device_create_file(dev, &dev_attr_hdcp_lstore);
	ret = device_create_file(dev, &dev_attr_div40);
	ret = device_create_file(dev, &dev_attr_hpd_state);
	ret = device_create_file(dev, &dev_attr_hdmi_used);
	ret = device_create_file(dev, &dev_attr_fake_plug);
	ret = device_create_file(dev, &dev_attr_ready);
	ret = device_create_file(dev, &dev_attr_support_3d);
	ret = device_create_file(dev, &dev_attr_dc_cap);
	ret = device_create_file(dev, &dev_attr_valid_mode);
	ret = device_create_file(dev, &dev_attr_allm_cap);
	ret = device_create_file(dev, &dev_attr_allm_mode);
	ret = device_create_file(dev, &dev_attr_contenttype_cap);
	ret = device_create_file(dev, &dev_attr_contenttype_mode);
	ret = device_create_file(dev, &dev_attr_ll_mode);
	ret = device_create_file(dev, &dev_attr_ll_user_mode);
	ret = device_create_file(dev, &dev_attr_hdmitx21);
	ret = device_create_file(dev, &dev_attr_aon_output);
	ret = device_create_file(dev, &dev_attr_def_stream_type);
	ret = device_create_file(dev, &dev_attr_hdcp_ctl_lvl);
	ret = device_create_file(dev, &dev_attr_sysctrl_enable);
	ret = device_create_file(dev, &dev_attr_propagate_stream_type);
	ret = device_create_file(dev, &dev_attr_cont_smng_method);
	ret = device_create_file(dev, &dev_attr_frl_rate);
	ret = device_create_file(dev, &dev_attr_dsc_en);
	ret = device_create_file(dev, &dev_attr_dsc_policy);
	ret = device_create_file(dev, &dev_attr_hdr_priority_mode);
	ret = device_create_file(dev, &dev_attr_is_passthrough_switch);
	ret = device_create_file(dev, &dev_attr_is_hdcp_cts_te);
	ret = device_create_file(dev, &dev_attr_need_filter_hdcp_off);
	ret = device_create_file(dev, &dev_attr_filter_hdcp_off_period);
	ret = device_create_file(dev, &dev_attr_not_restart_hdcp);
	ret = device_create_file(dev, &dev_attr_hdr_mute_frame);

	hdev->task_hdmist_check = kthread_run(hdmitx21_status_check, (void *)hdev,
				      "kthread_hdmist_check");
#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
	register_early_suspend(&hdmitx_early_suspend_handler);
#endif
	hdev->nb.notifier_call = hdmitx_reboot_notifier;
	register_reboot_notifier(&hdev->nb);
	hdmitx21_meson_init(hdev);
	hdmitx_hdr_state_init(hdev);
	mutex_init(&hdev->hdmimode_mutex);
	hdev->hpd_state = !!(hdev->hwop.cntlmisc(hdev, MISC_HPD_GPI_ST, 0));
#ifdef CONFIG_AMLOGIC_VOUT_SERVE
	vout_register_server(&hdmitx_vout_server);
#endif
#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
	vout2_register_server(&hdmitx_vout2_server);
#endif
#ifdef CONFIG_AMLOGIC_VOUT3_SERVE
	vout3_register_server(&hdmitx_vout3_server);
#endif
#if IS_ENABLED(CONFIG_AMLOGIC_SND_SOC)
	if (!hdev->pxp_mode && hdmitx21_uboot_audio_en()) {
		struct hdmitx_audpara *audpara = &hdev->cur_audio_param;

		audpara->sample_rate = FS_48K;
		audpara->type = CT_PCM;
		audpara->sample_size = SS_16BITS;
		audpara->channel_num = 2 - 1;
	}
	if (!hdev->pxp_mode)
		aout_register_client(&hdmitx_notifier_nb_a);
#endif
	/* update fmt_attr */
	hdmitx_init_fmt_attr(hdev);

	hdev->hpd_state = !!hdev->hwop.cntlmisc(hdev, MISC_HPD_GPI_ST, 0);
	hdmitx21_set_uevent(HDMITX_HDCPPWR_EVENT, HDMI_WAKEUP);
	INIT_WORK(&hdev->work_hdr, hdr_work_func);
	INIT_WORK(&hdev->work_hdr_unmute, hdr_unmute_work_func);
/* When init hdmi, clear the hdmitx module edid ram and edid buffer. */
	hdmitx21_edid_clear(hdev);
	hdmitx21_edid_ram_buffer_clear(hdev);
	hdev->hdmi_wq = alloc_workqueue(DEVICE_NAME,
					WQ_HIGHPRI | WQ_CPU_INTENSIVE, 0);
	INIT_DELAYED_WORK(&hdev->work_hpd_plugin, hdmitx_hpd_plugin_handler);
	INIT_DELAYED_WORK(&hdev->work_hpd_plugout, hdmitx_hpd_plugout_handler);
	INIT_DELAYED_WORK(&hdev->work_start_hdcp, hdmitx_start_hdcp_handler);
	INIT_DELAYED_WORK(&hdev->work_up_hdcp_timeout, hdmitx_up_hdcp_timeout_handler);
	INIT_DELAYED_WORK(&hdev->work_aud_hpd_plug,
			  hdmitx_aud_hpd_plug_handler);
	INIT_DELAYED_WORK(&hdev->work_internal_intr, hdmitx_top_intr_handler);

	/* for rx sense feature */
	hdev->rxsense_wq = alloc_workqueue("hdmitx_rxsense",
					   WQ_SYSFS | WQ_FREEZABLE, 0);
	INIT_DELAYED_WORK(&hdev->work_rxsense, hdmitx_rxsense_process);
	/* for cedst feature */
	hdev->cedst_wq = alloc_workqueue("hdmitx_cedst",
					 WQ_SYSFS | WQ_FREEZABLE, 0);
	INIT_DELAYED_WORK(&hdev->work_cedst, hdmitx_cedst_process);

	hdev->tx_aud_cfg = 1; /* default audio configure is on */
	hdmitx21_hdcp_init();
	intr_status_init_clear();
	hdmitx_setupirqs(hdev);

	if (hdev->hpd_state) {
		/* need to get edid before vout probe */
		hdev->already_used = 1;
		hdmitx_get_edid(hdev);
	}
	/* Trigger HDMITX IRQ*/
	if (hdev->hwop.cntlmisc(hdev, MISC_HPD_GPI_ST, 0)) {
		/* When bootup mbox and TV simultaneously,
		 * TV may not handle SCDC/DIV40
		 */
		if (is_cur_tmds_div40(hdev))
			hdmitx_resend_div40(hdev);
		hdev->hwop.cntlmisc(hdev,
			MISC_TRIGGER_HPD, 1);
		hdev->already_used = 1;
	} else {
		/* may plugout during uboot finish--kernel start,
		 * treat it as normal hotplug out, for > 3.4G case
		 */
		hdev->hwop.cntlmisc(hdev,
			MISC_TRIGGER_HPD, 0);
	}
	hdev->hdmi_init = 1;
    /* ll mode init values */
	hdev->ll_enabled_in_auto_mode = false;
	hdev->ll_user_set_mode = HDMI_LL_MODE_AUTO;

	/*bind to drm.*/
	component_add(&pdev->dev, &meson_hdmitx_bind_ops);
	tee_comm_dev_reg(hdev);
	pr_info("%s end\n", __func__);

	return r;
}

static int amhdmitx_remove(struct platform_device *pdev)
{
	struct hdmitx_dev *hdev = get_hdmitx21_device();
	struct device *dev = hdev->hdtx_dev;

	tee_comm_dev_unreg(hdev);
	/*unbind from drm.*/
	if (hdmitx21_device.drm_hdmitx_id != 0)
		component_del(&pdev->dev, &meson_hdmitx_bind_ops);

	cancel_work_sync(&hdev->work_hdr);
	cancel_work_sync(&hdev->work_hdr_unmute);

	if (hdev->hwop.uninit)
		hdev->hwop.uninit(hdev);
	hdev->hpd_event = 0xff;
	kthread_stop(hdev->task_hdmist_check);
#ifdef CONFIG_AMLOGIC_VOUT_SERVE
	vout_unregister_server(&hdmitx_vout_server);
#endif
#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
	vout2_unregister_server(&hdmitx_vout2_server);
#endif
#ifdef CONFIG_AMLOGIC_VOUT3_SERVE
	vout3_unregister_server(&hdmitx_vout3_server);
#endif

#if IS_ENABLED(CONFIG_AMLOGIC_SND_SOC)
	aout_unregister_client(&hdmitx_notifier_nb_a);
#endif

	/* Remove the cdev */
	device_remove_file(dev, &dev_attr_disp_mode);
	device_remove_file(dev, &dev_attr_attr);
	device_remove_file(dev, &dev_attr_vid_mute);
	device_remove_file(dev, &dev_attr_edid);
	device_remove_file(dev, &dev_attr_rawedid);
	device_remove_file(dev, &dev_attr_sink_type);
	device_remove_file(dev, &dev_attr_edid_parsing);
	device_remove_file(dev, &dev_attr_config);
	device_remove_file(dev, &dev_attr_debug);
	device_remove_file(dev, &dev_attr_disp_cap);
	device_remove_file(dev, &dev_attr_vrr_cap);
	device_remove_file(dev, &dev_attr_preferred_mode);
	device_remove_file(dev, &dev_attr_cea_cap);
	device_remove_file(dev, &dev_attr_vesa_cap);
	device_remove_file(dev, &dev_attr_hdr_cap);
	device_remove_file(dev, &dev_attr_hdr_cap2);
	device_remove_file(dev, &dev_attr_dv_cap);
	device_remove_file(dev, &dev_attr_dv_cap2);
	device_remove_file(dev, &dev_attr_dc_cap);
	device_remove_file(dev, &dev_attr_valid_mode);
	device_remove_file(dev, &dev_attr_allm_cap);
	device_remove_file(dev, &dev_attr_allm_mode);
	device_remove_file(dev, &dev_attr_contenttype_cap);
	device_remove_file(dev, &dev_attr_contenttype_mode);
	device_remove_file(dev, &dev_attr_ll_mode);
	device_remove_file(dev, &dev_attr_ll_user_mode);
	device_remove_file(dev, &dev_attr_hpd_state);
	device_remove_file(dev, &dev_attr_hdmi_used);
	device_remove_file(dev, &dev_attr_fake_plug);
	device_remove_file(dev, &dev_attr_ready);
	device_remove_file(dev, &dev_attr_support_3d);
	device_remove_file(dev, &dev_attr_avmute);
	device_remove_file(dev, &dev_attr_aud_mute);
	device_remove_file(dev, &dev_attr_frac_rate_policy);
	device_remove_file(dev, &dev_attr_sspll);
	device_remove_file(dev, &dev_attr_rxsense_policy);
	device_remove_file(dev, &dev_attr_rxsense_state);
	device_remove_file(dev, &dev_attr_cedst_policy);
	device_remove_file(dev, &dev_attr_cedst_count);
	device_remove_file(dev, &dev_attr_div40);
	device_remove_file(dev, &dev_attr_hdcp_type_policy);
	device_remove_file(dev, &dev_attr_hdmi_repeater_tx);
	device_remove_file(dev, &dev_attr_hdmi_hdr_status);
	device_remove_file(dev, &dev_attr_hdmitx21);
	device_remove_file(dev, &dev_attr_hdcp_ver);
	device_remove_file(dev, &dev_attr_def_stream_type);
	device_remove_file(dev, &dev_attr_aon_output);
	device_remove_file(dev, &dev_attr_hdcp_ctl_lvl);
	device_remove_file(dev, &dev_attr_sysctrl_enable);
	device_remove_file(dev, &dev_attr_propagate_stream_type);
	device_remove_file(dev, &dev_attr_cont_smng_method);
	device_remove_file(dev, &dev_attr_frl_rate);
	device_remove_file(dev, &dev_attr_dsc_en);
	device_remove_file(dev, &dev_attr_dsc_policy);
	device_remove_file(dev, &dev_attr_hdr_priority_mode);
	device_remove_file(dev, &dev_attr_is_passthrough_switch);
	device_remove_file(dev, &dev_attr_is_hdcp_cts_te);
	device_remove_file(dev, &dev_attr_need_filter_hdcp_off);
	device_remove_file(dev, &dev_attr_filter_hdcp_off_period);
	device_remove_file(dev, &dev_attr_not_restart_hdcp);
	device_remove_file(dev, &dev_attr_hdr_mute_frame);
	cdev_del(&hdev->cdev);

	device_destroy(hdmitx_class, hdev->hdmitx_id);

	class_destroy(hdmitx_class);

	unregister_chrdev_region(hdev->hdmitx_id, HDMI_TX_COUNT);
	return 0;
}

static void hdmitx_s5_clk_ctrl(struct hdmitx_dev *hdev, bool en)
{
	if (!hdev)
		return;

	hdev->hwop.cntlmisc(hdev, MISC_HDMI_CLKS_CTRL, en);
}

static void amhdmitx_shutdown(struct platform_device *pdev)
{
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	if (hdev->aon_output) {
		hdmitx21_disable_hdcp(hdev);
		return;
	}

	if (hdev->data->chip_type >= MESON_CPU_ID_S5)
		hdmitx_s5_clk_ctrl(hdev, 0);
}

/* there's corner case:
 * when deep suspend, RTC wakeup kernel-->
 * hdmi plugout/in interrupt-->
 * plugin bottom handle, edid...
 * however it may re-enter RTC suspend and
 * disable hdmitx clk during hdmi register
 * access in plugin bottom handler, cause
 * system hard lock and crash. so need to keep
 * basic hdmitx clk enabled when suspend
 */
#ifdef CONFIG_PM
static int amhdmitx_suspend(struct platform_device *pdev,
			    pm_message_t state)
{
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	if (hdev->data->chip_type >= MESON_CPU_ID_S5)
		hdmitx_s5_clk_ctrl(hdev, 0);

	pr_info("%s\n", __func__);
	return 0;
}

static int amhdmitx_resume(struct platform_device *pdev)
{
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	pr_info("%s\n", __func__);

	if (hdev->data->chip_type >= MESON_CPU_ID_S5)
		hdmitx_s5_clk_ctrl(hdev, 1);
	return 0;
}
#endif

static struct platform_driver amhdmitx_driver = {
	.probe	 = amhdmitx_probe,
	.remove	 = amhdmitx_remove,
#ifdef CONFIG_PM
	.suspend	= amhdmitx_suspend,
	.resume	 = amhdmitx_resume,
#endif
	.shutdown = amhdmitx_shutdown,
	.driver	 = {
		.name = DEVICE_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(meson_amhdmitx_of_match),
#ifdef CONFIG_HIBERNATION
		.pm	= &amhdmitx_pm,
#endif
	}
};

int  __init amhdmitx21_init(void)
{
	if (init_flag & INIT_FLAG_NOT_LOAD)
		return 0;

	return platform_driver_register(&amhdmitx_driver);
}

void __exit amhdmitx21_exit(void)
{
	pr_info("%s...\n", __func__);
	// TODO stop hdcp
	platform_driver_unregister(&amhdmitx_driver);
}

//MODULE_DESCRIPTION("AMLOGIC HDMI TX driver");
//MODULE_LICENSE("GPL");
//MODULE_VERSION("1.0.0");

/* besides characters defined in separator, '\"' are used as separator;
 * and any characters in '\"' will not act as separator
 */
static char *next_token_ex(char *separator, char *buf, u32 size,
			   u32 offset, u32 *token_len,
			   u32 *token_offset)
{
	char *ptoken = NULL;
	char last_separator = 0;
	char trans_char_flag = 0;

	if (buf) {
		for (; offset < size; offset++) {
			int ii = 0;
		char ch;

		if (buf[offset] == '\\') {
			trans_char_flag = 1;
			continue;
		}
		while (((ch = separator[ii++]) != buf[offset]) && (ch))
			;
		if (ch) {
			if (!ptoken) {
				continue;
		} else {
			if (last_separator != '"') {
				*token_len = (unsigned int)
					(buf + offset - ptoken);
				*token_offset = offset;
				return ptoken;
			}
		}
		} else if (!ptoken) {
			if (trans_char_flag && (buf[offset] == '"'))
				last_separator = buf[offset];
			ptoken = &buf[offset];
		} else if ((trans_char_flag && (buf[offset] == '"')) &&
			   (last_separator == '"')) {
			*token_len = (unsigned int)(buf + offset - ptoken - 2);
			*token_offset = offset + 1;
			return ptoken + 1;
		}
		trans_char_flag = 0;
	}
	if (ptoken) {
		*token_len = (unsigned int)(buf + offset - ptoken);
		*token_offset = offset;
	}
	}
	return ptoken;
}

/* check the colorattribute from uboot */
static void check_hdmiuboot_attr(char *token)
{
	struct hdmitx_dev *hdev = get_hdmitx21_device();
	char attr[16] = {0};
	const char * const cs[] = {
		"444", "422", "rgb", "420", NULL};
	const char * const cd[] = {
		"8bit", "10bit", "12bit", "16bit", NULL};
	int i;

	if (hdev->fmt_attr[0] != 0)
		return;

	if (!token)
		return;

	for (i = 0; cs[i]; i++) {
		if (strstr(token, cs[i])) {
			if (strlen(cs[i]) < sizeof(attr))
				strcpy(attr, cs[i]);
			strcat(attr, ",");
			break;
		}
	}
	for (i = 0; cd[i]; i++) {
		if (strstr(token, cd[i])) {
			if (strlen(cd[i]) < sizeof(attr))
				if (strlen(cd[i]) <
					(sizeof(attr) - strlen(attr)))
					strcat(attr, cd[i]);
			strncpy(hdev->fmt_attr, attr,
				sizeof(hdev->fmt_attr));
			hdev->fmt_attr[15] = '\0';
			break;
		}
	}
	memcpy(hdev->backup_fmt_attr, hdev->fmt_attr,
	       sizeof(hdev->fmt_attr));
}

static int hdmitx21_boot_para_setup(char *s)
{
	struct hdmitx_dev *hdev = get_hdmitx21_device();
	char separator[] = {' ', ',', ';', 0x0};
	char *token;
	u32 token_len = 0;
	u32 token_offset = 0;
	u32 offset = 0;
	int size = strlen(s);

	memset(hdev->fmt_attr, 0, sizeof(hdev->fmt_attr));
	memset(hdev->backup_fmt_attr, 0,
	       sizeof(hdev->backup_fmt_attr));

	do {
		token = next_token_ex(separator, s, size, offset,
				      &token_len, &token_offset);
		if (token) {
			if (token_len == 3 &&
			    strncmp(token, "off", token_len) == 0) {
				init_flag |= INIT_FLAG_NOT_LOAD;
			}
			check_hdmiuboot_attr(token);
		}
		offset = token_offset;
	} while (token);
	return 0;
}

__setup("hdmitx=", hdmitx21_boot_para_setup);

static int hdmitx21_boot_frac_rate(char *str)
{
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	if (strncmp("0", str, 1) == 0)
		hdev->frac_rate_policy = 0;
	else
		hdev->frac_rate_policy = 1;

	pr_info("hdmitx boot frac_rate_policy: %d",
		hdev->frac_rate_policy);

	hdev->backup_frac_rate_policy = hdev->frac_rate_policy;
	return 0;
}

__setup("frac_rate_policy=", hdmitx21_boot_frac_rate);

static int hdmitx21_boot_hdr_priority(char *str)
{
	struct hdmitx_dev *hdev = get_hdmitx21_device();
	unsigned int val = 0;

	if ((strncmp("1", str, 1) == 0) || (strncmp("2", str, 1) == 0)) {
		val = str[0] - '0';
		hdev->hdr_priority = val;
		pr_info("hdmitx boot hdr_priority: %d\n", val);
	}
	return 0;
}

__setup("hdr_priority=", hdmitx21_boot_hdr_priority);

static int __init get_hdmi21_checksum(char *str)
{
	snprintf(hdmichecksum, sizeof(hdmichecksum), "%s", str);

	pr_info("get hdmi checksum: %s\n", hdmichecksum);
	return 0;
}

__setup("hdmichecksum=", get_hdmi21_checksum);

static int hdmitx21_boot_dsc_policy(char *str)
{
	struct hdmitx_dev *hdev = get_hdmitx21_device();
	unsigned int val = 0;

	if ((strncmp("0", str, 1) == 0) ||
		(strncmp("1", str, 1) == 0) ||
		(strncmp("2", str, 1) == 0) ||
		(strncmp("3", str, 1) == 0) ||
		(strncmp("4", str, 1) == 0)) {
		val = str[0] - '0';
		hdev->dsc_policy = val;
		pr_info("hdmitx boot dsc_policy: %d\n", val);
	} else {
		/* default policy */
		hdev->dsc_policy = 0;
	}
	return 0;
}
__setup("dsc_policy=", hdmitx21_boot_dsc_policy);

MODULE_PARM_DESC(log21_level, "\n log21_level\n");
module_param(log21_level, int, 0644);

/*************DRM connector API**************/
static int drm_hdmitx_detect_hpd(void)
{
	return hdmitx21_device.hpd_state;
}

static int drm_hdmitx_register_hpd_cb(struct connector_hpd_cb *hpd_cb)
{
	mutex_lock(&setclk_mutex);
	hdmitx21_device.drm_hpd_cb.callback = hpd_cb->callback;
	hdmitx21_device.drm_hpd_cb.data = hpd_cb->data;
	mutex_unlock(&setclk_mutex);
	return 0;
}

static int drm_hdmitx_get_vic_list(int **vics)
{
	struct hdmitx_dev *hdev = get_hdmitx21_device();
	struct rx_cap *prxcap = &hdev->rxcap;
	enum hdmi_vic vic;
	const struct hdmi_timing *timing;
	int len = prxcap->VIC_count;
	int i;
	int count = 0;
	int *viclist = 0;

	if (len == 0)
		return 0;

	viclist = kmalloc_array(len, sizeof(int), GFP_KERNEL);
	for (i = 0; i < len; i++) {
		vic = prxcap->VIC[i];
		timing = hdmitx21_gettiming_from_vic(vic);
		if (timing) {
			viclist[count] = vic;
			count++;
		}
	}

	/* TODO if count is non-zero, viclist will free in drm caller */
	if (count == 0)
		kfree(viclist);
	else
		*vics = viclist;

	return count;
}

static int drm_hdmitx_get_timing_para(int vic, struct drm_hdmitx_timing_para *para)
{
	const struct hdmi_timing *timing;

	if (vic == HDMI_2_720x480p60_4x3 ||
		vic == HDMI_6_720x480i60_4x3 ||
		vic == HDMI_17_720x576p50_4x3 ||
		vic == HDMI_21_720x576i50_4x3) {
		if (hdmitx_check_vic(vic + 1))
			return -1;
		vic++;
	}

	timing = hdmitx21_gettiming_from_vic(vic);
	if (!timing)
		return -1;

	if (timing->sname)
		memcpy(para->name, timing->sname, DRM_DISPLAY_MODE_LEN);
	else if (timing->name)
		memcpy(para->name, timing->name, DRM_DISPLAY_MODE_LEN);
	else
		return -1;

	if (timing->v_freq % 1000 == 0) {
		para->sync_dura_num = timing->v_freq / 1000;
		para->sync_dura_den = 1;
	} else {
		para->sync_dura_num = timing->v_freq;
		para->sync_dura_den = 1000;
	}

	para->pi_mode = timing->pi_mode;
	para->pix_repeat_factor = timing->pixel_repetition_factor;

	para->h_pol = timing->h_pol;
	para->v_pol = timing->v_pol;
	para->pixel_freq = timing->pixel_freq;

	para->h_active = timing->h_active;
	para->h_front = timing->h_front;
	para->h_sync = timing->h_sync;
	para->h_total = timing->h_total;
	if (!timing->pi_mode)
		para->v_active = timing->v_active / 2;
	else
		para->v_active = timing->v_active;
	para->v_front = timing->v_front;
	para->v_sync = timing->v_sync;
	para->v_total = timing->v_total;

	return 0;
}

static unsigned char *drm_hdmitx_get_raw_edid(void)
{
	if (hdmitx21_device.edid_ptr)
		return hdmitx21_device.edid_ptr;
	else
		return hdmitx21_device.EDID_buf;
}

static void drm_hdmitx_avmute(unsigned char mute)
{
	int cmd = OFF_AVMUTE;

	if (hdmitx21_device.hdmi_init != 1)
		return;
	if (mute == 0)
		cmd = CLR_AVMUTE;
	else
		cmd = SET_AVMUTE;
	hdmitx21_device.hwop.cntlmisc(&hdmitx21_device, MISC_AVMUTE_OP, cmd);
}

static void drm_hdmitx_set_phy(unsigned char en)
{
	int cmd = TMDS_PHY_ENABLE;

	if (hdmitx21_device.hdmi_init != 1)
		return;

	if (en == 0)
		cmd = TMDS_PHY_DISABLE;
	else
		cmd = TMDS_PHY_ENABLE;
	hdmitx21_device.hwop.cntlmisc(&hdmitx21_device, MISC_TMDS_PHY_OP, cmd);
}

static void drm_hdmitx_setup_attr(const char *buf)
{
	char attr[16] = {0};
	int len = strlen(buf);

	if (len <= 16)
		memcpy(attr, buf, len);
	memcpy(hdmitx21_device.fmt_attr, attr, sizeof(hdmitx21_device.fmt_attr));
}

static void drm_hdmitx_get_attr(char attr[16])
{
	memcpy(attr, hdmitx21_device.fmt_attr, sizeof(hdmitx21_device.fmt_attr));
}

static bool drm_hdmitx_chk_mode_attr_sup(char *mode, char *attr)
{
	struct hdmi_format_para *para = NULL;
	bool valid = false;
	mutex_lock(&valid_mode_mutex);

	if (hdmitx21_device.hdmi_init != 1)
		return valid;

	if (!mode || !attr)
		return valid;

	valid = pre_process_str(attr);
	if (!valid)
		return valid;
	para = hdmitx21_tst_fmt_name(mode, attr);

	if (para) {
		pr_info("sname = %s\n", para->hdmitx_vinfo.name);
		pr_info("char_clk = %d\n", para->tmds_clk);
		pr_info("cd = %d\n", para->cd);
		pr_info("cs = %d\n", para->cs);
	}

	valid = hdmitx21_edid_check_valid_mode(&hdmitx21_device, para);
	mutex_unlock(&valid_mode_mutex);
	return valid;
}

static unsigned int drm_hdmitx_get_contenttypes(void)
{
	unsigned int types = 1 << DRM_MODE_CONTENT_TYPE_NO_DATA;/*NONE DATA*/
	struct rx_cap *prxcap = &hdmitx21_device.rxcap;

	if (prxcap->cnc0)
		types |= 1 << DRM_MODE_CONTENT_TYPE_GRAPHICS;
	if (prxcap->cnc1)
		types |= 1 << DRM_MODE_CONTENT_TYPE_PHOTO;
	if (prxcap->cnc2)
		types |= 1 << DRM_MODE_CONTENT_TYPE_CINEMA;
	if (prxcap->cnc3)
		types |= 1 << DRM_MODE_CONTENT_TYPE_GAME;

	return types;
}

static int drm_hdmitx_set_contenttype(int content_type)
{
	struct hdmitx_dev *hdev = &hdmitx21_device;
	int ret = 0;

	if (_is_hdmi14_4k(hdev->cur_VIC))
		hdmitx21_construct_vsif(hdev, VT_HDMI14_4K, 1, NULL);
	hdev->ct_mode = 0;
	hdev->hwop.cntlconfig(hdev, CONF_CT_MODE, SET_CT_OFF);

	switch (content_type) {
	case DRM_MODE_CONTENT_TYPE_GRAPHICS:
		hdev->ct_mode = 2;
		hdev->hwop.cntlconfig(hdev,
			CONF_CT_MODE, SET_CT_GRAPHICS);
		break;
	case DRM_MODE_CONTENT_TYPE_PHOTO:
		hdev->ct_mode = 3;
		hdev->hwop.cntlconfig(hdev,
			CONF_CT_MODE, SET_CT_PHOTO);
		break;
	case DRM_MODE_CONTENT_TYPE_CINEMA:
		hdev->ct_mode = 4;
		hdev->hwop.cntlconfig(hdev,
			CONF_CT_MODE, SET_CT_CINEMA);
		break;
	case DRM_MODE_CONTENT_TYPE_GAME:
		hdev->ct_mode = 1;
		hdev->hwop.cntlconfig(hdev,
			CONF_CT_MODE, SET_CT_GAME);
		break;
	default:
		pr_err("[%s]: [%d] unsupported type\n",
			__func__, content_type);
		ret = -EINVAL;
		break;
	};

	return ret;
}

static const struct dv_info *drm_hdmitx_get_dv_info(void)
{
	const struct dv_info *dv = &ext_dvinfo;

	return dv;
}

static const struct hdr_info *drm_hdmitx_get_hdr_info(void)
{
	static struct hdr_info hdrinfo;

	hdrinfo_to_vinfo(&hdrinfo, &hdmitx21_device);

	return &hdrinfo;
}

static int drm_hdmitx_get_hdr_priority(void)
{
	return hdmitx21_device.hdr_priority;
}

/*hdcp functions*/
static void drm_hdmitx_hdcp_init(void)
{
}

static void drm_hdmitx_hdcp_exit(void)
{
}

static void drm_hdmitx_hdcp_enable(int hdcp_type)
{
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	switch (hdcp_type) {
	case HDCP_NULL:
		pr_err("%s enabled HDCP_NULL\n", __func__);
		break;
	case HDCP_MODE14:
		hdev->hdcp_mode = 0x1;
		hdcp_mode_set(1);
		break;
	case HDCP_MODE22:
		hdev->hdcp_mode = 0x2;
		hdcp_mode_set(2);
		break;
	default:
		pr_err("%s unknown hdcp %d\n", __func__, hdcp_type);
		break;
	};
}

static void drm_hdmitx_hdcp_disable(void)
{
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	hdev->hdcp_mode = 0x00;
	hdcp_mode_set(0);
}

static void drm_hdmitx_hdcp_disconnect(void)
{
	drm_hdmitx_hdcp_disable();
}

static unsigned int drm_hdmitx_get_tx_hdcp_cap(void)
{
	int lstore = 0;

	if (get_hdcp2_lstore())
		lstore |= HDCP_MODE22;
	if (get_hdcp1_lstore())
		lstore |= HDCP_MODE14;

	pr_info("%s tx hdcp [%d]\n", __func__, lstore);
	return lstore;
}

static unsigned int drm_hdmitx_get_rx_hdcp_cap(void)
{
	unsigned int rxhdcp = 0;
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	/* if TX don't have HDCP22 key, skip RX hdcp22 ver */
	/* note that during hdcp1.4 authentication, read hdcp version
	 * of connected TV set(capable of hdcp2.2) may cause TV
	 * switch its hdcp mode, and flash screen. should not
	 * read hdcp version of sink during hdcp1.4 authentication.
	 * if hdcp1.4 authentication currently, force return hdcp1.4
	 */
	if (hdev->hdcp_mode == 0x1) {
		rxhdcp = HDCP_MODE14;
	} else if (get_hdcp2_lstore() && is_rx_hdcp2ver()) {
		rx_hdcp2_ver = 1;
		rxhdcp = HDCP_MODE22 | HDCP_MODE14;
	} else {
		rx_hdcp2_ver = 0;
		rxhdcp = HDCP_MODE14;
	}

	pr_info("%s rx hdcp [%d]\n", __func__, rxhdcp);
	return rxhdcp;
}

static void drm_hdmitx_register_hdcp_notify(struct connector_hdcp_cb *cb)
{
	if (hdmitx21_device.drm_hdcp_cb.hdcp_notify)
		pr_err("Register hdcp notify again!?\n");

	hdmitx21_device.drm_hdcp_cb.hdcp_notify = cb->hdcp_notify;
	hdmitx21_device.drm_hdcp_cb.data = cb->data;
}

static struct meson_hdmitx_dev drm_hdmitx_instance = {
	.base = {
		.ver = MESON_DRM_CONNECTOR_V10,
	},
	.detect = drm_hdmitx_detect_hpd,
	.register_hpd_cb = drm_hdmitx_register_hpd_cb,
	.get_raw_edid = drm_hdmitx_get_raw_edid,
	.get_vic_list = drm_hdmitx_get_vic_list,
	.get_timing_para_by_vic = drm_hdmitx_get_timing_para,
	.get_content_types = drm_hdmitx_get_contenttypes,
	.set_content_type = drm_hdmitx_set_contenttype,
	.setup_attr = drm_hdmitx_setup_attr,
	.get_attr = drm_hdmitx_get_attr,
	.test_attr = drm_hdmitx_chk_mode_attr_sup,
	.get_dv_info = drm_hdmitx_get_dv_info,
	.get_hdr_info = drm_hdmitx_get_hdr_info,
	.get_hdr_priority = drm_hdmitx_get_hdr_priority,
	.avmute = drm_hdmitx_avmute,
	.set_phy = drm_hdmitx_set_phy,
	.get_hdmi_hdr_status = hdmi_hdr_status_to_drm,
	.set_aspect_ratio = hdmitx21_set_aspect_ratio,
	.get_aspect_ratio = hdmitx21_get_aspect_ratio_value,
	.set_frac = set_frac_rate_policy,
	.get_frac = get_frac_rate_policy,

	/*hdcp apis*/
	.hdcp_init = drm_hdmitx_hdcp_init,
	.hdcp_exit = drm_hdmitx_hdcp_exit,
	.hdcp_enable = drm_hdmitx_hdcp_enable,
	.hdcp_disable = drm_hdmitx_hdcp_disable,
	.hdcp_disconnect = drm_hdmitx_hdcp_disconnect,
	.get_tx_hdcp_cap = drm_hdmitx_get_tx_hdcp_cap,
	.get_rx_hdcp_cap = drm_hdmitx_get_rx_hdcp_cap,
	.register_hdcp_notify = drm_hdmitx_register_hdcp_notify,
	.get_vrr_cap = drm_hdmitx_get_vrr_cap,
	.get_vrr_mode_group = drm_hdmitx_get_vrr_mode_group,
	.get_hdcp_ctl_lvl = get_hdmitx_hdcp_ctl_lvl_to_drm,
};

static int meson_hdmitx_bind(struct device *dev,
			      struct device *master, void *data)
{
	struct meson_drm_bound_data *bound_data = data;
	struct hdmitx_dev *hdev = &hdmitx21_device;

	if (bound_data->connector_component_bind) {
		hdev->drm_hdmitx_id = bound_data->connector_component_bind
			(bound_data->drm,
			DRM_MODE_CONNECTOR_HDMIA,
			&drm_hdmitx_instance.base);
		pr_err("%s hdmi21_[%d]\n", __func__, hdev->drm_hdmitx_id);
	} else {
		pr_err("no bind func from drm.\n");
	}

	return 0;
}

static void meson_hdmitx_unbind(struct device *dev,
				 struct device *master, void *data)
{
	struct meson_drm_bound_data *bound_data = data;
	struct hdmitx_dev *hdev = &hdmitx21_device;

	if (bound_data->connector_component_unbind) {
		bound_data->connector_component_unbind(bound_data->drm,
			DRM_MODE_CONNECTOR_HDMIA, hdev->drm_hdmitx_id);
		pr_err("%s hdmi [%d]\n", __func__, hdev->drm_hdmitx_id);
	} else {
		pr_err("no unbind func from drm.\n");
	}

	hdev->drm_hdmitx_id = 0;
}

/*************DRM connector API end**************/

/****** tee_hdcp key related start ******/
static long hdcp_comm_ioctl(struct file *file,
	unsigned int cmd,
	unsigned long arg)
{
	int rtn_val;
	struct hdmitx_dev *hdev = &hdmitx21_device;
	u8 hdcp_key_store = 0;

	switch (cmd) {
	case TEE_HDCP_IOC_START:
		/* notify by TEE, hdcp key ready */
		rtn_val = 0;
		if (get_hdcp2_lstore())
			hdcp_key_store |= BIT(1);
		if (get_hdcp1_lstore())
			hdcp_key_store |= BIT(0);
		pr_info("tee load hdcp key ready: 0x%x\n", hdcp_key_store);
		mutex_lock(&hdev->hdmimode_mutex);
		if (hdev->hpd_state == 1 &&
			hdev->ready &&
			hdmitx21_get_hdcp_mode() == 0) {
			pr_info("hdmi ready but hdcp not enabled, enable now\n");
			if (hdcp_need_control_by_upstream(hdev)) {
				pr_info("hdmitx: currently hdcp should started by upstream\n");
			} else {
				if (hdcp_key_store & BIT(1))
					hdev->dw_hdcp22_cap = is_rx_hdcp2ver();
				hdmitx21_enable_hdcp(hdev);
			}
		}
		mutex_unlock(&hdev->hdmimode_mutex);
		break;
	default:
		rtn_val = -EPERM;
		break;
	}
	return rtn_val;
}

static const struct file_operations hdcp_comm_file_operations = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = hdcp_comm_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = hdcp_comm_ioctl,
#endif
};

static void tee_comm_dev_reg(struct hdmitx_dev *hdev)
{
	int ret;

	hdev->hdcp_comm_device.minor = MISC_DYNAMIC_MINOR;
	hdev->hdcp_comm_device.name = "tee_comm_hdcp";
	hdev->hdcp_comm_device.fops = &hdcp_comm_file_operations;

	ret = misc_register(&hdev->hdcp_comm_device);
	if (ret < 0)
		pr_err("%s misc_register fail\n", __func__);
}

static void tee_comm_dev_unreg(struct hdmitx_dev *hdev)
{
	misc_deregister(&hdev->hdcp_comm_device);
}

/****** tee_hdcp key related end ******/

