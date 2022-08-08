/*
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Description:
 */
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/vmalloc.h>
#include <linux/uaccess.h>
#include <linux/slab.h>

#include <linux/platform_device.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/major.h>
#include "media_sync_core.h"
#include "media_sync_dev.h"

#define MEDIASYNC_DEVICE_NAME   "mediasync"
static struct device *mediasync_dev;

typedef struct alloc_para {
	s32 mDemuxId;
	s32 mPcrPid;
} mediasync_alloc_para;

typedef struct systime_para {
       s64 mStcUs;
       s64 mSystemTimeUs;
}mediasync_systime_para;

typedef struct updatetime_para {
	int64_t mMediaTimeUs;
	int64_t mSystemTimeUs;
	bool mForceUpdate;
}mediasync_updatetime_para;

typedef struct arthortime_para {
	int64_t mMediaTimeUs;
	int64_t mSystemTimeUs;
	int64_t mStcTimeUs;
}mediasync_arthortime_para;

typedef struct priv_s {
	s32 mSyncInsId;
	mediasync_ins *mSyncIns;
}mediasync_priv_s;

static int mediasync_open(struct inode *inode, struct file *file)
{
	mediasync_priv_s *priv = {0};
	priv = kzalloc(sizeof(mediasync_priv_s), GFP_KERNEL);
	if (priv == NULL)
		return -ENOMEM;
	priv->mSyncInsId = -1;
	priv->mSyncIns = NULL;
	file->private_data = priv;
	return 0;
}

static int mediasync_release(struct inode *inode, struct file *file)
{
	long ret = 0;
	mediasync_priv_s *priv = (mediasync_priv_s *)file->private_data;
	if (priv == NULL) {
		return -ENOMEM;
	}

	if (priv->mSyncInsId != -1) {
		ret = mediasync_ins_unbinder(priv->mSyncInsId);
		priv->mSyncInsId = -1;
		priv->mSyncIns = NULL;
	}
	kfree(priv);
	return 0;
}

static long mediasync_ioctl(struct file *file, unsigned int cmd, ulong arg)
{
	long ret = 0;
	mediasync_speed SyncSpeed = {0};
	mediasync_speed PcrSlope = {0};
	mediasync_frameinfo FrameInfo = {-1, -1};
	mediasync_video_packets_info videoPacketsInfo = {-1, -1};
	mediasync_audio_packets_info audioPacketsInfo = {-1,-1,1,0,-1};
	mediasync_audioinfo AudioInfo = {0, 0};
	mediasync_videoinfo VideoInfo = {0, 0};
	mediasync_audio_format AudioFormat;
	mediasync_clocktype ClockType = UNKNOWN_CLOCK;
	mediasync_clockprovider_state state;
	s32 SyncInsId = -1;
	s32 SyncPaused = 0;
	s32 SyncMode = -1;
	s32 SyncState = 0;
	s64 NextVsyncSystemTime = 0;
	s64 TrackMediaTime = 0;
	int HasAudio = -1;
	int HasVideo = -1;
	s32 StartThreshold = 0;
	s32 PtsAdjust = 0;
	s64 VideoWorkMode = 0;
	s64 FccEnable = 0;
	int mute_flag  = 0;
	int PauseResumeFlag  = 0;
	int AvRefFlag = 0;
	int AvRef = 0;
	mediasync_priv_s *priv = (mediasync_priv_s *)file->private_data;
	mediasync_ins *SyncIns = NULL;
	mediasync_alloc_para parm = {0};
	mediasync_arthortime_para ArthorTime = {0};
	mediasync_updatetime_para UpdateTime = {0};
	mediasync_systime_para SystemTime = {0};
	aml_Source_Type sourceType = TS_DEMOD;
	s64 UpdateTimeThreshold = 0;
	s64 StartMediaTime = -1;


	switch (cmd) {
		case MEDIASYNC_IOC_INSTANCE_ALLOC:
			if (copy_from_user ((void *)&parm,
						(void *)arg,
						sizeof(parm)))
				return -EFAULT;
			if (mediasync_ins_alloc(parm.mDemuxId,
						parm.mPcrPid,
						&SyncInsId,
						&SyncIns) < 0) {
				return -EFAULT;
			}
			if (SyncIns == NULL) {
				return -EFAULT;
			}
			if (priv != NULL) {
				priv->mSyncInsId = SyncInsId;
				priv->mSyncIns = SyncIns;
			}

		break;
		case MEDIASYNC_IOC_INSTANCE_GET:
			if (priv->mSyncIns == NULL) {
				return -EFAULT;
			}

			SyncInsId = priv->mSyncInsId;
			if (copy_to_user((void *)arg,
					&SyncInsId,
					sizeof(SyncInsId))) {
				return -EFAULT;
			}
		break;
		case MEDIASYNC_IOC_INSTANCE_BINDER:
			if (copy_from_user((void *)&SyncInsId,
						(void *)arg,
						sizeof(SyncInsId))) {
				return -EFAULT;
			}
			ret = mediasync_ins_binder(SyncInsId, &SyncIns);
			if (SyncIns == NULL) {
				return -EFAULT;
			}

			priv->mSyncInsId = SyncInsId;
			priv->mSyncIns = SyncIns;
		break;
		case MEDIASYNC_IOC_INSTANCE_STATIC_BINDER:
			if (copy_from_user((void *)&SyncInsId,
						(void *)arg,
						sizeof(SyncInsId))) {
				return -EFAULT;
			}
			ret = mediasync_static_ins_binder(SyncInsId, &SyncIns);
			if (SyncIns == NULL) {
				return -EFAULT;
			}

			priv->mSyncInsId = SyncInsId;
			priv->mSyncIns = SyncIns;
		break;

		case MEDIASYNC_IOC_UPDATE_MEDIATIME:
			if (copy_from_user((void *)&UpdateTime,
						(void *)arg,
						sizeof(UpdateTime))) {
				return -EFAULT;
			}
			if (priv->mSyncIns == NULL) {
				return -EFAULT;
			}

			ret = mediasync_ins_update_mediatime(priv->mSyncInsId,
							UpdateTime.mMediaTimeUs,
							UpdateTime.mSystemTimeUs,
							UpdateTime.mForceUpdate);
		break;

		case MEDIASYNC_IOC_GET_MEDIATIME:
			if (priv->mSyncIns == NULL) {
				return -EFAULT;
			}
			ret = mediasync_ins_get_anchor_time(priv->mSyncInsId,
							&(ArthorTime.mMediaTimeUs),
							&(ArthorTime.mStcTimeUs),
							&(ArthorTime.mSystemTimeUs));
			if (ret == 0) {
				if (copy_to_user((void *)arg,
						&ArthorTime,
						sizeof(ArthorTime))) {
					return -EFAULT;
				}
			}
		break;

		case MEDIASYNC_IOC_GET_SYSTEMTIME:

			if (priv->mSyncIns == NULL) {
				return -EFAULT;
			}

			ret = mediasync_ins_get_systemtime(priv->mSyncInsId,
							&(SystemTime.mStcUs),
							&(SystemTime.mSystemTimeUs));
			if (ret == 0) {
				if (copy_to_user((void *)arg,
						&SystemTime,
						sizeof(SystemTime))) {
					return -EFAULT;
				}
			}
		break;

		case MEDIASYNC_IOC_GET_NEXTVSYNC_TIME:
			if (priv->mSyncIns == NULL)
				return -EFAULT;

			ret = mediasync_ins_get_nextvsync_systemtime(priv->mSyncInsId,
								&NextVsyncSystemTime);
			if (ret == 0) {
				if (copy_to_user((void *)arg,
						&NextVsyncSystemTime,
						sizeof(NextVsyncSystemTime)))
					return -EFAULT;
			}
		break;

		case MEDIASYNC_IOC_SET_SPEED:
			if (copy_from_user((void *)&SyncSpeed,
					(void *)arg,
					sizeof(SyncSpeed)))
				return -EFAULT;

			if (priv->mSyncIns == NULL)
				return -EFAULT;

			ret = mediasync_ins_set_mediatime_speed(priv->mSyncInsId,
								SyncSpeed);
		break;

		case MEDIASYNC_IOC_GET_SPEED:
			if (priv->mSyncIns == NULL)
				return -EFAULT;

			ret = mediasync_ins_get_mediatime_speed(priv->mSyncInsId,
								&SyncSpeed);
			if (ret == 0) {
				if (copy_to_user((void *)arg,
						&SyncSpeed,
						sizeof(SyncSpeed)))
					return -EFAULT;
			}
		break;

		case MEDIASYNC_IOC_SET_PAUSE:
			if (copy_from_user((void *)&SyncPaused,
						(void *)arg,
						sizeof(SyncPaused)))
				return -EFAULT;

			if (priv->mSyncIns == NULL)
				return -EFAULT;

			ret = mediasync_ins_set_paused(priv->mSyncInsId,
							SyncPaused);
		break;

		case MEDIASYNC_IOC_GET_PAUSE:
			if (priv->mSyncIns == NULL)
				return -EFAULT;

			ret = mediasync_ins_get_paused(priv->mSyncInsId,
							&SyncPaused);
			if (ret == 0) {
				if (copy_to_user((void *)arg,
						&SyncPaused,
						sizeof(SyncPaused)))
					return -EFAULT;
			}
		break;

		case MEDIASYNC_IOC_SET_SYNCMODE:
			if (copy_from_user((void *)&SyncMode,
						(void *)arg,
						sizeof(SyncMode)))
				return -EFAULT;

			if (priv->mSyncIns == NULL)
				return -EFAULT;

			ret = mediasync_ins_set_syncmode(priv->mSyncInsId,
							SyncMode);
		break;

		case MEDIASYNC_IOC_GET_SYNCMODE:
			if (priv->mSyncIns == NULL)
				return -EFAULT;

			ret = mediasync_ins_get_syncmode(priv->mSyncInsId,
							&SyncMode);
			if (ret == 0) {
				if (copy_to_user((void *)arg,
						&SyncMode,
						sizeof(SyncMode)))
					return -EFAULT;
			}
		break;
		case MEDIASYNC_IOC_GET_TRACKMEDIATIME:
			if (priv->mSyncIns == NULL) {
				return -EFAULT;
			}

			ret = mediasync_ins_get_trackmediatime(priv->mSyncInsId,
								&TrackMediaTime);
			if (ret == 0) {
				if (copy_to_user((void *)arg,
						&TrackMediaTime,
						sizeof(TrackMediaTime))) {
					return -EFAULT;
				}
			}
		break;

		case MEDIASYNC_IOC_SET_FIRST_AFRAME_INFO:
			if (copy_from_user((void *)&FrameInfo,
					(void *)arg,
					sizeof(FrameInfo)))
				return -EFAULT;

			if (priv->mSyncIns == NULL)
				return -EFAULT;

			ret = mediasync_ins_set_firstaudioframeinfo(priv->mSyncInsId,
								FrameInfo);
		break;

		case MEDIASYNC_IOC_GET_FIRST_AFRAME_INFO:
			if (priv->mSyncIns == NULL)
				return -EFAULT;

			ret = mediasync_ins_get_firstaudioframeinfo(priv->mSyncInsId,
								&FrameInfo);
			if (ret == 0) {
				if (copy_to_user((void *)arg,
						&FrameInfo,
						sizeof(FrameInfo)))
					return -EFAULT;
			}
		break;

		case MEDIASYNC_IOC_SET_FIRST_VFRAME_INFO:
			if (copy_from_user((void *)&FrameInfo,
					(void *)arg,
					sizeof(FrameInfo)))
				return -EFAULT;

			if (priv->mSyncIns == NULL)
				return -EFAULT;

			ret = mediasync_ins_set_firstvideoframeinfo(priv->mSyncInsId,
								FrameInfo);
		break;

		case MEDIASYNC_IOC_GET_FIRST_VFRAME_INFO:
			if (priv->mSyncIns == NULL)
				return -EFAULT;

			ret = mediasync_ins_get_firstvideoframeinfo(priv->mSyncInsId,
								&FrameInfo);
			if (ret == 0) {
				if (copy_to_user((void *)arg,
						&FrameInfo,
						sizeof(FrameInfo)))
					return -EFAULT;
			}
		break;


		case MEDIASYNC_IOC_SET_FIRST_DMXPCR_INFO:
			if (copy_from_user((void *)&FrameInfo,
					(void *)arg,
					sizeof(FrameInfo)))
				return -EFAULT;

			if (priv->mSyncIns == NULL)
				return -EFAULT;

			ret = mediasync_ins_set_firstdmxpcrinfo(priv->mSyncInsId,
								FrameInfo);
		break;

		case MEDIASYNC_IOC_GET_FIRST_DMXPCR_INFO:
			if (priv->mSyncIns == NULL)
				return -EFAULT;

			ret = mediasync_ins_get_firstdmxpcrinfo(priv->mSyncInsId,
								&FrameInfo);
			if (ret == 0) {
				if (copy_to_user((void *)arg,
						&FrameInfo,
						sizeof(FrameInfo)))
					return -EFAULT;
			}
		break;

		case MEDIASYNC_IOC_SET_REFCLOCK_INFO:
			if (copy_from_user((void *)&FrameInfo,
					(void *)arg,
					sizeof(FrameInfo)))
				return -EFAULT;

			if (priv->mSyncIns == NULL)
				return -EFAULT;

			ret = mediasync_ins_set_refclockinfo(priv->mSyncInsId,
								FrameInfo);
		break;

		case MEDIASYNC_IOC_GET_REFCLOCK_INFO:
			if (priv->mSyncIns == NULL)
				return -EFAULT;

			ret = mediasync_ins_get_refclockinfo(priv->mSyncInsId,
								&FrameInfo);
			if (ret == 0) {
				if (copy_to_user((void *)arg,
						&FrameInfo,
						sizeof(FrameInfo)))
					return -EFAULT;
			}
		break;

		case MEDIASYNC_IOC_SET_CUR_AFRAME_INFO:
			if (copy_from_user((void *)&FrameInfo,
					(void *)arg,
					sizeof(FrameInfo)))
				return -EFAULT;

			if (priv->mSyncIns == NULL)
				return -EFAULT;

			ret = mediasync_ins_set_curaudioframeinfo(priv->mSyncInsId,
								FrameInfo);
		break;

		case MEDIASYNC_IOC_GET_CUR_AFRAME_INFO:
			if (priv->mSyncIns == NULL)
				return -EFAULT;

			ret = mediasync_ins_get_curaudioframeinfo(priv->mSyncInsId,
								&FrameInfo);
			if (ret == 0) {
				if (copy_to_user((void *)arg,
						&FrameInfo,
						sizeof(FrameInfo)))
					return -EFAULT;
			}
		break;

		case MEDIASYNC_IOC_SET_CUR_VFRAME_INFO:
			if (copy_from_user((void *)&FrameInfo,
					(void *)arg,
					sizeof(FrameInfo)))
				return -EFAULT;

			if (priv->mSyncIns == NULL)
				return -EFAULT;

			ret = mediasync_ins_set_curvideoframeinfo(priv->mSyncInsId,
								FrameInfo);
		break;

		case MEDIASYNC_IOC_GET_CUR_VFRAME_INFO:
			if (priv->mSyncIns == NULL)
				return -EFAULT;

			ret = mediasync_ins_get_curvideoframeinfo(priv->mSyncInsId,
								&FrameInfo);
			if (ret == 0) {
				if (copy_to_user((void *)arg,
						&FrameInfo,
						sizeof(FrameInfo)))
					return -EFAULT;
			}
		break;

		case MEDIASYNC_IOC_SET_CUR_DMXPCR_INFO:
			if (copy_from_user((void *)&FrameInfo,
					(void *)arg,
					sizeof(FrameInfo)))
				return -EFAULT;

			if (priv->mSyncIns == NULL)
				return -EFAULT;

			ret = mediasync_ins_set_curdmxpcrinfo(priv->mSyncInsId,
								FrameInfo);
		break;

		case MEDIASYNC_IOC_GET_CUR_DMXPCR_INFO:
			if (priv->mSyncIns == NULL)
				return -EFAULT;

			ret = mediasync_ins_get_curdmxpcrinfo(priv->mSyncInsId,
								&FrameInfo);
			if (ret == 0) {
				if (copy_to_user((void *)arg,
						&FrameInfo,
						sizeof(FrameInfo)))
					return -EFAULT;
			}
		break;

		case MEDIASYNC_IOC_SET_AUDIO_INFO:
			if (copy_from_user((void *)&AudioInfo,
					(void *)arg,
					sizeof(AudioInfo)))
				return -EFAULT;

			if (priv->mSyncIns == NULL)
				return -EFAULT;

			ret = mediasync_ins_set_audioinfo(priv->mSyncInsId,
								AudioInfo);
		break;

		case MEDIASYNC_IOC_GET_AUDIO_INFO:
			if (priv->mSyncIns == NULL)
				return -EFAULT;

			ret = mediasync_ins_get_audioinfo(priv->mSyncInsId,
								&AudioInfo);
			if (ret == 0) {
				if (copy_to_user((void *)arg,
						&AudioInfo,
						sizeof(AudioInfo)))
					return -EFAULT;
			}
		break;


		case MEDIASYNC_IOC_SET_AUDIO_MUTEFLAG:
			if (copy_from_user((void *)&mute_flag,
					(void *)arg,
					sizeof(mute_flag))) {
				return -EFAULT;
			}

			if (priv->mSyncIns == NULL)
				return -EFAULT;

			ret = mediasync_ins_set_audiomute(priv->mSyncInsId,
								mute_flag);
		break;

		case MEDIASYNC_IOC_GET_AUDIO_MUTEFLAG:
			if (priv->mSyncIns == NULL)
				return -EFAULT;

			ret = mediasync_ins_get_audiomute(priv->mSyncInsId,
								&mute_flag);
			if (ret == 0) {
				if (copy_to_user((void *)arg,
						&mute_flag,
						sizeof(mute_flag)))
					return -EFAULT;
			}
		break;

		case MEDIASYNC_IOC_SET_VIDEO_INFO:
			if (copy_from_user((void *)&VideoInfo,
					(void *)arg,
					sizeof(VideoInfo)))
				return -EFAULT;

			if (priv->mSyncIns == NULL)
				return -EFAULT;

			ret = mediasync_ins_set_videoinfo(priv->mSyncInsId,
								VideoInfo);
		break;

		case MEDIASYNC_IOC_GET_VIDEO_INFO:
			if (priv->mSyncIns == NULL)
				return -EFAULT;

			ret = mediasync_ins_get_videoinfo(priv->mSyncInsId,
								&VideoInfo);
			if (ret == 0) {
				if (copy_to_user((void *)arg,
						&VideoInfo,
						sizeof(VideoInfo)))
					return -EFAULT;
			}
		break;

		case MEDIASYNC_IOC_SET_HASAUDIO:
			if (copy_from_user((void *)&HasAudio,
					(void *)arg,
					sizeof(HasAudio)))
				return -EFAULT;

			if (priv->mSyncIns == NULL)
				return -EFAULT;

			ret = mediasync_ins_set_hasaudio(priv->mSyncInsId,
								HasAudio);
		break;

		case MEDIASYNC_IOC_GET_HASAUDIO:
			if (priv->mSyncIns == NULL)
				return -EFAULT;

			ret = mediasync_ins_get_hasaudio(priv->mSyncInsId,
								&HasAudio);
			if (ret == 0) {
				if (copy_to_user((void *)arg,
						&HasAudio,
						sizeof(HasAudio)))
					return -EFAULT;
			}
		break;

		case MEDIASYNC_IOC_SET_HASVIDEO:
			if (copy_from_user((void *)&HasVideo,
					(void *)arg,
					sizeof(HasVideo)))
				return -EFAULT;

			if (priv->mSyncIns == NULL)
				return -EFAULT;

			ret = mediasync_ins_set_hasvideo(priv->mSyncInsId,
								HasVideo);
		break;

		case MEDIASYNC_IOC_GET_HASVIDEO:
			if (priv->mSyncIns == NULL)
				return -EFAULT;

			ret = mediasync_ins_get_hasvideo(priv->mSyncInsId,
								&HasVideo);
			if (ret == 0) {
				if (copy_to_user((void *)arg,
						&HasVideo,
						sizeof(HasVideo)))
					return -EFAULT;
			}
		break;

		case MEDIASYNC_IOC_SET_AVSTATE:
			if (copy_from_user((void *)&SyncState,
					(void *)arg,
					sizeof(SyncState)))
				return -EFAULT;

			if (priv->mSyncIns == NULL)
				return -EFAULT;

			ret = mediasync_ins_set_avsyncstate(priv->mSyncInsId,
								SyncState);
		break;

		case MEDIASYNC_IOC_GET_AVSTATE:
			if (priv->mSyncIns == NULL)
				return -EFAULT;

			ret = mediasync_ins_get_avsyncstate(priv->mSyncInsId,
								&SyncState);
			if (ret == 0) {
				if (copy_to_user((void *)arg,
						&SyncState,
						sizeof(SyncState)))
					return -EFAULT;
			}
		break;

		case MEDIASYNC_IOC_SET_CLOCKTYPE:
			if (copy_from_user((void *)&ClockType,
					(void *)arg,
					sizeof(ClockType)))
				return -EFAULT;

			if (priv->mSyncIns == NULL)
				return -EFAULT;

			ret = mediasync_ins_set_clocktype(priv->mSyncInsId,
								ClockType);
		break;

		case MEDIASYNC_IOC_GET_CLOCKTYPE:
			if (priv->mSyncIns == NULL)
				return -EFAULT;

			ret = mediasync_ins_get_clocktype(priv->mSyncInsId,
								&ClockType);
			if (ret == 0) {
				if (copy_to_user((void *)arg,
						&ClockType,
						sizeof(ClockType)))
					return -EFAULT;
			}
		break;

		case MEDIASYNC_IOC_SET_CLOCKSTATE:
			if (copy_from_user((void *)&state,
					(void *)arg,
					sizeof(state)))
				return -EFAULT;

			if (priv->mSyncIns == NULL)
				return -EFAULT;

			ret = mediasync_ins_set_clockstate(priv->mSyncInsId,
								state);
		break;

		case MEDIASYNC_IOC_GET_CLOCKSTATE:
			if (priv->mSyncIns == NULL)
				return -EFAULT;

			ret = mediasync_ins_get_clockstate(priv->mSyncInsId,
								&state);
			if (ret == 0) {
				if (copy_to_user((void *)arg,
						&state,
						sizeof(state)))
					return -EFAULT;
			}
		break;

		case MEDIASYNC_IOC_SET_STARTTHRESHOLD:
			if (copy_from_user((void *)&StartThreshold,
					(void *)arg,
					sizeof(StartThreshold)))
				return -EFAULT;

			if (priv->mSyncIns == NULL)
				return -EFAULT;

			ret = mediasync_ins_set_startthreshold(priv->mSyncInsId,
								StartThreshold);
		break;

		case MEDIASYNC_IOC_GET_STARTTHRESHOLD:
			if (priv->mSyncIns == NULL)
				return -EFAULT;

			ret = mediasync_ins_get_startthreshold(priv->mSyncInsId,
								&StartThreshold);
			if (ret == 0) {
				if (copy_to_user((void *)arg,
						&StartThreshold,
						sizeof(StartThreshold)))
					return -EFAULT;
			}
		break;

		case MEDIASYNC_IOC_SET_PTSADJUST:
			if (copy_from_user((void *)&PtsAdjust,
					(void *)arg,
					sizeof(PtsAdjust)))
				return -EFAULT;

			if (priv->mSyncIns == NULL)
				return -EFAULT;

			ret = mediasync_ins_set_ptsadjust(priv->mSyncInsId,
								PtsAdjust);
		break;

		case MEDIASYNC_IOC_GET_PTSADJUST:
			if (priv->mSyncIns == NULL)
				return -EFAULT;

			ret = mediasync_ins_get_ptsadjust(priv->mSyncInsId,
								&PtsAdjust);
			if (ret == 0) {
				if (copy_to_user((void *)arg,
						&PtsAdjust,
						sizeof(PtsAdjust)))
					return -EFAULT;
			}
		break;

		case MEDIASYNC_IOC_SET_VIDEOWORKMODE:
			if (copy_from_user((void *)&VideoWorkMode,
					(void *)arg,
					sizeof(VideoWorkMode)))
				return -EFAULT;

			if (priv->mSyncIns == NULL)
				return -EFAULT;

			ret = mediasync_ins_set_videoworkmode(priv->mSyncInsId,
								VideoWorkMode);
		break;

		case MEDIASYNC_IOC_GET_VIDEOWORKMODE:
			if (priv->mSyncIns == NULL)
				return -EFAULT;

			ret = mediasync_ins_get_videoworkmode(priv->mSyncInsId,
								&VideoWorkMode);
			if (ret == 0) {
				if (copy_to_user((void *)arg,
						&VideoWorkMode,
						sizeof(VideoWorkMode)))
					return -EFAULT;
			}
		break;

		case MEDIASYNC_IOC_SET_FCCENABLE:
			if (copy_from_user((void *)&FccEnable,
					(void *)arg,
					sizeof(FccEnable)))
				return -EFAULT;

			if (priv->mSyncIns == NULL)
				return -EFAULT;

			ret = mediasync_ins_set_fccenable(priv->mSyncInsId,
								FccEnable);
		break;

		case MEDIASYNC_IOC_GET_FCCENABLE:
			if (priv->mSyncIns == NULL)
				return -EFAULT;

			ret = mediasync_ins_get_fccenable(priv->mSyncInsId,
								&FccEnable);
			if (ret == 0) {
				if (copy_to_user((void *)arg,
						&FccEnable,
						sizeof(FccEnable)))
					return -EFAULT;
			}
		break;

		case MEDIASYNC_IOC_SET_SOURCE_TYPE:
			if (copy_from_user((void *)&sourceType,
					(void *)arg,
					sizeof(sourceType)))
				return -EFAULT;

			if (priv->mSyncIns == NULL)
				return -EFAULT;

			ret = mediasync_ins_set_source_type(priv->mSyncInsId,
								sourceType);
		break;

		case MEDIASYNC_IOC_GET_SOURCE_TYPE:
			if (priv->mSyncIns == NULL)
				return -EFAULT;

			ret = mediasync_ins_get_source_type(priv->mSyncInsId,
								&sourceType);
			if (ret == 0) {
				if (copy_to_user((void *)arg,
						&sourceType,
						sizeof(sourceType)))
					return -EFAULT;
			}
		break;

		case MEDIASYNC_IOC_SET_UPDATETIME_THRESHOLD:
			if (copy_from_user((void *)&UpdateTimeThreshold,
					(void *)arg,
					sizeof(UpdateTimeThreshold)))
				return -EFAULT;

			if (priv->mSyncIns == NULL)
				return -EFAULT;

			ret = mediasync_ins_set_updatetime_threshold(priv->mSyncInsId,
								UpdateTimeThreshold);
		break;

		case MEDIASYNC_IOC_GET_UPDATETIME_THRESHOLD:
			if (priv->mSyncIns == NULL)
				return -EFAULT;

			ret = mediasync_ins_get_updatetime_threshold(priv->mSyncInsId,
								&UpdateTimeThreshold);
			if (ret == 0) {
				if (copy_to_user((void *)arg,
						&UpdateTimeThreshold,
						sizeof(UpdateTimeThreshold)))
					return -EFAULT;
			}
		break;
		case MEDIASYNC_IOC_SET_START_MEDIA_TIME:
			if (priv->mSyncIns == NULL)
				return -EFAULT;

			if (copy_from_user((void *)&StartMediaTime,
				(void *)arg,
				sizeof(StartMediaTime)))
			return -EFAULT;
			ret = mediasync_ins_set_start_media_time(priv->mSyncInsId, StartMediaTime);
		break;

		case MEDIASYNC_IOC_GET_START_MEDIA_TIME:
			if (priv->mSyncIns == NULL)
				return -EFAULT;

			ret = mediasync_ins_get_start_media_time(priv->mSyncInsId, &StartMediaTime);
			if (ret == 0) {
				if (copy_to_user((void *)arg,
					&StartMediaTime,
					sizeof(StartMediaTime)))
				return -EFAULT;
			}
		break;

		case MEDIASYNC_IOC_SET_AUDIO_FORMAT:
			if (copy_from_user((void *)&AudioFormat,
					(void *)arg,
					sizeof(AudioFormat)))
				return -EFAULT;

			if (priv->mSyncIns == NULL)
				return -EFAULT;

			ret = mediasync_ins_set_audioformat(priv->mSyncInsId,
								AudioFormat);
		break;

		case MEDIASYNC_IOC_GET_AUDIO_FORMAT:
			if (priv->mSyncIns == NULL)
				return -EFAULT;

			ret = mediasync_ins_get_audioformat(priv->mSyncInsId,
								&AudioFormat);
			if (ret == 0) {
				if (copy_to_user((void *)arg,
						&AudioFormat,
						sizeof(AudioFormat)))
					return -EFAULT;
			}
		break;

		case MEDIASYNC_IOC_SET_PAUSERESUME_FLAG:
			if (copy_from_user((void *)&PauseResumeFlag,
					(void *)arg,
					sizeof(PauseResumeFlag)))
				return -EFAULT;

			if (priv->mSyncIns == NULL)
				return -EFAULT;

			ret = mediasync_ins_set_pauseresume(priv->mSyncInsId,
								PauseResumeFlag);
		break;

		case MEDIASYNC_IOC_GET_PAUSERESUME_FLAG:
			if (priv->mSyncIns == NULL)
				return -EFAULT;

			ret = mediasync_ins_get_pauseresume(priv->mSyncInsId,
								&PauseResumeFlag);
			if (ret == 0) {
				if (copy_to_user((void *)arg,
						&PauseResumeFlag,
						sizeof(PauseResumeFlag)))
					return -EFAULT;
			}
		break;

		case MEDIASYNC_IOC_SET_PCRSLOPE:
			if (copy_from_user((void *)&PcrSlope,
					(void *)arg,
					sizeof(PcrSlope)))
				return -EFAULT;

			if (priv->mSyncIns == NULL)
				return -EFAULT;

			ret = mediasync_ins_set_pcrslope(priv->mSyncInsId,
								PcrSlope);
		break;

		case MEDIASYNC_IOC_GET_PCRSLOPE:
			if (priv->mSyncIns == NULL)
				return -EFAULT;

			ret = mediasync_ins_get_pcrslope(priv->mSyncInsId,
								&PcrSlope);
			if (ret == 0) {
				if (copy_to_user((void *)arg,
						&PcrSlope,
						sizeof(PcrSlope)))
					return -EFAULT;
			}
		break;

		case MEDIASYNC_IOC_UPDATE_AVREF:
			if (copy_from_user((void *)&AvRefFlag,
						(void *)arg,
						sizeof(AvRefFlag)))
				return -EFAULT;

			if (priv->mSyncIns == NULL)
				return -EFAULT;

			ret = mediasync_ins_update_avref(priv->mSyncInsId,
							AvRefFlag);
		break;

		case MEDIASYNC_IOC_GET_AVREF:
			if (priv->mSyncIns == NULL)
				return -EFAULT;

			ret = mediasync_ins_get_avref(priv->mSyncInsId,
							&AvRef);
			if (ret == 0) {
				if (copy_to_user((void *)arg,
						&AvRef,
						sizeof(AvRef)))
					return -EFAULT;
			}
		break;
		case MEDIASYNC_IOC_SET_QUEUE_AUDIO_INFO:
			if (copy_from_user((void *)&FrameInfo,
					(void *)arg,
					sizeof(FrameInfo)))
				return -EFAULT;

			if (priv->mSyncIns == NULL)
				return -EFAULT;

			ret = mediasync_ins_set_queueaudioinfo(priv->mSyncInsId,
								FrameInfo);
		break;

		case MEDIASYNC_IOC_GET_QUEUE_AUDIO_INFO:
			if (priv->mSyncIns == NULL)
				return -EFAULT;

			ret = mediasync_ins_get_queueaudioinfo(priv->mSyncInsId,
								&FrameInfo);
			if (ret == 0) {
				if (copy_to_user((void *)arg,
						&FrameInfo,
						sizeof(FrameInfo)))
					return -EFAULT;
			}
		break;
		case MEDIASYNC_IOC_SET_QUEUE_VIDEO_INFO:
			if (copy_from_user((void *)&FrameInfo,
					(void *)arg,
					sizeof(FrameInfo)))
				return -EFAULT;

			if (priv->mSyncIns == NULL)
				return -EFAULT;

			ret = mediasync_ins_set_queuevideoinfo(priv->mSyncInsId,
								FrameInfo);
		break;

		case MEDIASYNC_IOC_GET_QUEUE_VIDEO_INFO:
			if (priv->mSyncIns == NULL)
				return -EFAULT;

			ret = mediasync_ins_get_queuevideoinfo(priv->mSyncInsId,
								&FrameInfo);
			if (ret == 0) {
				if (copy_to_user((void *)arg,
						&FrameInfo,
						sizeof(FrameInfo)))
					return -EFAULT;
			}
		break;

		case  MEDIASYNC_IOC_SET_AUDIO_PACKETC_INFO :
			if (copy_from_user((void *)&audioPacketsInfo,
					(void *)arg,
					sizeof(audioPacketsInfo)))
				return -EFAULT;

			if (priv->mSyncIns == NULL)
				return -EFAULT;
			ret = mediasync_ins_set_audio_packets_info(priv->mSyncInsId,
								audioPacketsInfo);
		break;

		case  MEDIASYNC_IOC_GET_AUDIO_CACHE_INFO :
			if (priv->mSyncIns == NULL)
				return -EFAULT;

			ret = mediasync_ins_get_audio_cache_info(priv->mSyncInsId,
								&AudioInfo);
			if (ret == 0) {
				if (copy_to_user((void *)arg,
						&AudioInfo,
						sizeof(AudioInfo)))
					return -EFAULT;
			}
		break;

		case  MEDIASYNC_IOC_SET_VIDEO_PACKETC_INFO :
			if (copy_from_user((void *)&videoPacketsInfo,
					(void *)arg,
					sizeof(videoPacketsInfo)))
				return -EFAULT;

			if (priv->mSyncIns == NULL)
				return -EFAULT;
			ret = mediasync_ins_set_video_packets_info(priv->mSyncInsId,
								videoPacketsInfo);
		break;

		case  MEDIASYNC_IOC_GET_VIDEO_CACHE_INFO :

			if (priv->mSyncIns == NULL)
				return -EFAULT;

			ret = mediasync_ins_get_video_cache_info(priv->mSyncInsId,
								&VideoInfo);
			if (ret == 0) {
				if (copy_to_user((void *)arg,
						&VideoInfo,
						sizeof(VideoInfo)))
					return -EFAULT;
			}
		break;

		case MEDIASYNC_IOC_SET_FIRST_QUEUE_AUDIO_INFO:
			if (copy_from_user((void *)&FrameInfo,
					(void *)arg,
					sizeof(FrameInfo)))
				return -EFAULT;

			if (priv->mSyncIns == NULL)
				return -EFAULT;

			ret = mediasync_ins_set_firstqueueaudioinfo(priv->mSyncInsId,
								FrameInfo);
		break;

		case MEDIASYNC_IOC_GET_FIRST_QUEUE_AUDIO_INFO:
			if (priv->mSyncIns == NULL)
				return -EFAULT;

			ret = mediasync_ins_get_firstqueueaudioinfo(priv->mSyncInsId,
								&FrameInfo);
			if (ret == 0) {
				if (copy_to_user((void *)arg,
						&FrameInfo,
						sizeof(FrameInfo)))
					return -EFAULT;
			}
		break;
		case MEDIASYNC_IOC_SET_FIRST_QUEUE_VIDEO_INFO:
			if (copy_from_user((void *)&FrameInfo,
					(void *)arg,
					sizeof(FrameInfo)))
				return -EFAULT;

			if (priv->mSyncIns == NULL)
				return -EFAULT;

			ret = mediasync_ins_set_firstqueuevideoinfo(priv->mSyncInsId,
								FrameInfo);
		break;

		case MEDIASYNC_IOC_GET_FIRST_QUEUE_VIDEO_INFO:
			if (priv->mSyncIns == NULL)
				return -EFAULT;

			ret = mediasync_ins_get_firstqueuevideoinfo(priv->mSyncInsId,
								&FrameInfo);
			if (ret == 0) {
				if (copy_to_user((void *)arg,
						&FrameInfo,
						sizeof(FrameInfo)))
					return -EFAULT;
			}
		break;

		default:
			pr_info("invalid cmd:%d\n", cmd);
		break;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
static long mediasync_compat_ioctl(struct file *file, unsigned int cmd, ulong arg)
{
	long ret = 0;
	switch (cmd) {
		case MEDIASYNC_IOC_INSTANCE_ALLOC:
		case MEDIASYNC_IOC_INSTANCE_GET:
		case MEDIASYNC_IOC_INSTANCE_BINDER:
		case MEDIASYNC_IOC_INSTANCE_STATIC_BINDER:
		case MEDIASYNC_IOC_UPDATE_MEDIATIME:
		case MEDIASYNC_IOC_GET_MEDIATIME:
		case MEDIASYNC_IOC_GET_SYSTEMTIME:
		case MEDIASYNC_IOC_GET_NEXTVSYNC_TIME:
		case MEDIASYNC_IOC_SET_SPEED:
		case MEDIASYNC_IOC_GET_SPEED:
		case MEDIASYNC_IOC_SET_PAUSE:
		case MEDIASYNC_IOC_GET_PAUSE:
		case MEDIASYNC_IOC_SET_SYNCMODE:
		case MEDIASYNC_IOC_GET_SYNCMODE:
		case MEDIASYNC_IOC_GET_TRACKMEDIATIME:
		case MEDIASYNC_IOC_SET_FIRST_AFRAME_INFO:
		case MEDIASYNC_IOC_GET_FIRST_AFRAME_INFO:
		case MEDIASYNC_IOC_SET_FIRST_VFRAME_INFO:
		case MEDIASYNC_IOC_GET_FIRST_VFRAME_INFO:
		case MEDIASYNC_IOC_SET_FIRST_DMXPCR_INFO:
		case MEDIASYNC_IOC_GET_FIRST_DMXPCR_INFO:
		case MEDIASYNC_IOC_SET_REFCLOCK_INFO:
		case MEDIASYNC_IOC_GET_REFCLOCK_INFO:
		case MEDIASYNC_IOC_SET_CUR_AFRAME_INFO:
		case MEDIASYNC_IOC_GET_CUR_AFRAME_INFO:
		case MEDIASYNC_IOC_SET_CUR_VFRAME_INFO:
		case MEDIASYNC_IOC_GET_CUR_VFRAME_INFO:
		case MEDIASYNC_IOC_SET_CUR_DMXPCR_INFO:
		case MEDIASYNC_IOC_GET_CUR_DMXPCR_INFO:
		case MEDIASYNC_IOC_SET_AUDIO_INFO:
		case MEDIASYNC_IOC_GET_AUDIO_INFO:
		case MEDIASYNC_IOC_SET_VIDEO_INFO:
		case MEDIASYNC_IOC_GET_VIDEO_INFO:
		case MEDIASYNC_IOC_SET_AVSTATE:
		case MEDIASYNC_IOC_GET_AVSTATE:
		case MEDIASYNC_IOC_SET_HASAUDIO:
		case MEDIASYNC_IOC_GET_HASAUDIO:
		case MEDIASYNC_IOC_SET_HASVIDEO:
		case MEDIASYNC_IOC_GET_HASVIDEO:
		case MEDIASYNC_IOC_GET_CLOCKTYPE:
		case MEDIASYNC_IOC_SET_CLOCKTYPE:
		case MEDIASYNC_IOC_GET_CLOCKSTATE:
		case MEDIASYNC_IOC_SET_CLOCKSTATE:
		case MEDIASYNC_IOC_SET_STARTTHRESHOLD:
		case MEDIASYNC_IOC_GET_STARTTHRESHOLD:
		case MEDIASYNC_IOC_SET_PTSADJUST:
		case MEDIASYNC_IOC_GET_PTSADJUST:
		case MEDIASYNC_IOC_SET_VIDEOWORKMODE:
		case MEDIASYNC_IOC_GET_VIDEOWORKMODE:
		case MEDIASYNC_IOC_SET_FCCENABLE:
		case MEDIASYNC_IOC_GET_FCCENABLE:
		case MEDIASYNC_IOC_SET_AUDIO_MUTEFLAG:
		case MEDIASYNC_IOC_GET_AUDIO_MUTEFLAG:
		case MEDIASYNC_IOC_SET_SOURCE_TYPE:
		case MEDIASYNC_IOC_GET_SOURCE_TYPE:
		case MEDIASYNC_IOC_SET_UPDATETIME_THRESHOLD:
		case MEDIASYNC_IOC_GET_UPDATETIME_THRESHOLD:
		case MEDIASYNC_IOC_SET_START_MEDIA_TIME:
		case MEDIASYNC_IOC_GET_START_MEDIA_TIME:
		case MEDIASYNC_IOC_SET_AUDIO_FORMAT:
		case MEDIASYNC_IOC_GET_AUDIO_FORMAT:
		case MEDIASYNC_IOC_SET_PAUSERESUME_FLAG:
		case MEDIASYNC_IOC_GET_PAUSERESUME_FLAG:
		case MEDIASYNC_IOC_SET_PCRSLOPE:
		case MEDIASYNC_IOC_GET_PCRSLOPE:
		case MEDIASYNC_IOC_UPDATE_AVREF:
		case MEDIASYNC_IOC_GET_AVREF:
		case MEDIASYNC_IOC_SET_QUEUE_AUDIO_INFO:
		case MEDIASYNC_IOC_GET_QUEUE_AUDIO_INFO:
		case MEDIASYNC_IOC_SET_QUEUE_VIDEO_INFO:
		case MEDIASYNC_IOC_GET_QUEUE_VIDEO_INFO:
		case MEDIASYNC_IOC_SET_FIRST_QUEUE_AUDIO_INFO:
		case MEDIASYNC_IOC_GET_FIRST_QUEUE_AUDIO_INFO:
		case MEDIASYNC_IOC_SET_FIRST_QUEUE_VIDEO_INFO:
		case MEDIASYNC_IOC_GET_FIRST_QUEUE_VIDEO_INFO:
		case  MEDIASYNC_IOC_SET_AUDIO_PACKETC_INFO :
		case  MEDIASYNC_IOC_GET_AUDIO_CACHE_INFO :
		case  MEDIASYNC_IOC_SET_VIDEO_PACKETC_INFO :
		case  MEDIASYNC_IOC_GET_VIDEO_CACHE_INFO :

			return mediasync_ioctl(file, cmd, arg);
		default:
			return -EINVAL;
	}
	return ret;
}
#endif

static const struct file_operations mediasync_fops = {
	.owner = THIS_MODULE,
	.open = mediasync_open,
	.release = mediasync_release,
	.unlocked_ioctl = mediasync_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = mediasync_compat_ioctl,
#endif
};

static struct attribute *mediasync_class_attrs[] = {
	NULL
};

ATTRIBUTE_GROUPS(mediasync_class);

static struct class mediasync_class = {
	.name = "mediasync",
	.class_groups = mediasync_class_groups,
};

static int __init mediasync_module_init(void)
{
	int r;

	r = class_register(&mediasync_class);

	if (r) {
		pr_err("mediasync class create fail.\n");
		return r;
	}

	/* create tsync device */
	r = register_chrdev(MEDIASYNC_MAJOR, "mediasync", &mediasync_fops);
	if (r < 0) {
		pr_info("Can't register major for tsync\n");
		goto err2;
	}

	mediasync_dev = device_create(&mediasync_class, NULL,
				MKDEV(MEDIASYNC_MAJOR, 0), NULL, MEDIASYNC_DEVICE_NAME);

	if (IS_ERR(mediasync_dev)) {
		pr_err("Can't create mediasync_dev device\n");
		goto err1;
	}
	mediasync_init();
	return 0;

err1:
	unregister_chrdev(MEDIASYNC_MAJOR, "mediasync");
err2:
	class_unregister(&mediasync_class);

	return 0;
}

static void __exit mediasync_module_exit(void)
{
	device_destroy(&mediasync_class, MKDEV(MEDIASYNC_MAJOR, 0));
	unregister_chrdev(MEDIASYNC_MAJOR, "mediasync");
	class_unregister(&mediasync_class);
}

module_init(mediasync_module_init);
module_exit(mediasync_module_exit);

MODULE_DESCRIPTION("AMLOGIC media sync management driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Lifeng Cao <lifeng.cao@amlogic.com>");

