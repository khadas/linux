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
#include <linux/delay.h>
#include <linux/time.h>
#include <asm/div64.h>
#include <linux/sched/clock.h>

#include <linux/amlogic/media/vpu/vpu.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/vfm/vframe_receiver.h>
#include <linux/amlogic/media/frame_sync/timestamp.h>
#include <linux/amlogic/media/frame_sync/tsync.h>
#include <linux/amlogic/media/vout/vinfo.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/media/frc/frc_reg.h>
#include <linux/amlogic/media/frc/frc_common.h>
#include <linux/amlogic/media/registers/cpu_version.h>

#include "frc_drv.h"
#include "frc_proc.h"
#include "frc_interface.h"

/*
 * every vsync handle
 * vf : current input vf
 * cur_video_sts: current video state
 * called in vpp vs ir :vsync_fisr_in
 * defined(CONFIG_AMLOGIC_MEDIA_FRC)
 */
int frc_input_handle(struct vframe_s *vf, struct vpp_frame_par_s *cur_video_sts)
{
	struct frc_dev_s *devp = get_frc_devp();
	u64 timestamp = sched_clock();

	if (!devp)
		return -1;

	if (!devp->probe_ok || !devp->power_on_flag)
		return -1;

	/*update vs time*/
	devp->frc_sts.vs_cnt++;
	timestamp = div64_u64(timestamp, 1000);
	devp->vs_duration = timestamp - devp->vs_timestamp;
	devp->vs_timestamp = timestamp;
	frc_vpp_vs_ir_chk_film(devp);
	/*vframe change detect and video state detects*/
	frc_input_vframe_handle(devp, vf, cur_video_sts);

	/*frc work mode handle*/
	// frc_state_handle_old(devp);
	// frc_state_handle(devp);
	frc_state_handle_new(devp);
	return 0;
}
EXPORT_SYMBOL(frc_input_handle);

/*
 * for other module control frc
 * FRC_STATE_ENABLE: FRC is working
 * FRC_STATE_DISABLE: video data input to frc hw module, but frc not works
 * FRC_STATE_BYPASS: video data not input to frc module
 *
 */
int frc_set_mode(enum frc_state_e state)
{
	if (state == FRC_STATE_DISABLE)
		frc_change_to_state(FRC_STATE_DISABLE);
	else if (state == FRC_STATE_BYPASS)
		frc_change_to_state(FRC_STATE_BYPASS);
	else if (state == FRC_STATE_ENABLE)
		frc_change_to_state(FRC_STATE_ENABLE);

	return 0;
}

/*
 * get current frc video latency
 * return: ms
 */
int frc_get_video_latency(void)
{
	struct frc_dev_s *devp = get_frc_devp();
	// u32 out_frm_dly_num;
	struct vinfo_s *vinfo = get_current_vinfo();
	u32 vout_hz = 0;
	u32 delay_time = 0;   /*ms*/
	u32 delay = 0;   /*ms*/

	if (vinfo && vinfo->sync_duration_den != 0)
		vout_hz = vinfo->sync_duration_num / vinfo->sync_duration_den;
	// out_frm_dly_num = READ_FRC_BITS(FRC_REG_TOP_CTRL9, 24, 4);
	if (vout_hz != 0)
		delay = 35 * 100 / vout_hz;
	// delay_time = out_frm_dly_num;
	if (!devp || !vinfo)
		return 0;
	if (devp->frc_sts.auto_ctrl == 1) {
		// if (devp->in_sts.vf_sts == VFRAME_NO)
		// delay_time = delay;
		// else if (devp->frc_sts.state == FRC_STATE_BYPASS ||
		//	devp->frc_sts.state == FRC_STATE_DISABLE)
		// delay_time = 0;
		// else if (devp->frc_sts.state == FRC_STATE_ENABLE)
		//	delay_time = delay;
		delay_time = delay;
	} else {
		delay_time = 0;
	}
	return delay_time;
}
EXPORT_SYMBOL(frc_get_video_latency);

int frc_is_on(void)
{
	enum chip_id chip;
	struct frc_dev_s *devp = get_frc_devp();
	struct frc_data_s *frc_data;

	if (!devp)
		return 0;

	if (!devp->probe_ok || !devp->power_on_flag)
		return 0;

	frc_data = (struct frc_data_s *)devp->data;
	chip = frc_data->match_data->chip;

	if (chip == ID_T3 && is_meson_rev_a() &&
		(READ_FRC_REG(FRC_TOP_CTRL) & 0x01) == FRC_STATE_ENABLE)
		return 1;

	return 0;
}
EXPORT_SYMBOL(frc_is_on);

int frc_is_supported(void)
{
	struct frc_dev_s *devp = get_frc_devp();

	if (!devp)
		return 0;
	if (!devp->probe_ok)
		return 0;

	return 1;
}
EXPORT_SYMBOL(frc_is_supported);

int frc_set_seg_display(u8 enable, u8 seg1, u8 seg2, u8 seg3)
{
	struct frc_dev_s *devp = get_frc_devp();

	if (!devp)
		return 0;
	if (!devp->probe_ok)
		return 0;
	if (enable) {
		UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_POSI_AND_NUM41_NUM42, 0xE1 << 24, 0xFF000000);
		UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM43_NUM44_NUM45_NUM46, 1 << 31, BIT_31);
		UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM43_NUM44_NUM45_NUM46, 1 << 23, BIT_23);
		UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM43_NUM44_NUM45_NUM46, 1 << 15, BIT_15);
		UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM43_NUM44_NUM45_NUM46, 2 << 28, 0x70000000);
		UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM43_NUM44_NUM45_NUM46, 1 << 20, 0x700000);
		UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM43_NUM44_NUM45_NUM46, 2 << 12, 0x7000);
		UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM43_NUM44_NUM45_NUM46,
						seg1 << 24, 0xF000000);
		UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM43_NUM44_NUM45_NUM46, seg2 << 16, 0xF0000);
		UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM43_NUM44_NUM45_NUM46, seg3 << 8, 0xF00);
	} else {
		UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM43_NUM44_NUM45_NUM46, 0, BIT_31);
		UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM43_NUM44_NUM45_NUM46, 0, BIT_23);
		UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM43_NUM44_NUM45_NUM46, 0, BIT_15);
		UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_POSI_AND_NUM41_NUM42, 0x03 << 24, 0xFF000000);
	}
	return 1;
}
EXPORT_SYMBOL(frc_set_seg_display);
