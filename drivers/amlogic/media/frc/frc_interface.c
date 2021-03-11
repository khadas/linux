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

#include "frc_reg.h"
#include "frc_common.h"
#include "frc_drv.h"
#include "frc_proc.h"

/*
 * every vsync handle
 * vf : curret input vf
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

	if (!devp->probe_ok)
		return -1;

	/*update vs time*/
	devp->vs_duration = timestamp - devp->vs_timestamp;
	devp->vs_timestamp = timestamp;

	/*vframe change detect and video state detects*/
	frc_input_vframe_handle(devp, vf, cur_video_sts);

	/*frc work mode handle*/
	frc_state_handle(devp);

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
 */
int frc_get_video_latency(void)
{
	struct frc_dev_s *devp = get_frc_devp();

	if (!devp)
		return 0;

	if (devp->frc_sts.state == FRC_STATE_BYPASS || devp->frc_sts.state == FRC_STATE_DISABLE)
		return 0;

	return -1;
}

