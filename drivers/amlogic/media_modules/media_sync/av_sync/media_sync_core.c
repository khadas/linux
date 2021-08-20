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
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/syscalls.h>
#include <linux/times.h>
#include <linux/time.h>
#include <linux/time64.h>
#include "media_sync_core.h"
#define KERNEL_ATRACE_TAG KERNEL_ATRACE_TAG_MEDIA_SYNC
#include <trace/events/meson_atrace.h>

#define MAX_INSTANCE_NUM 10
MediaSyncManage vMediaSyncInsList[MAX_INSTANCE_NUM];
u64 last_system;
u64 last_pcr;

typedef int (*pfun_amldemux_pcrscr_get)(int demux_device_index, int index,
					u64 *stc);
static pfun_amldemux_pcrscr_get amldemux_pcrscr_get = NULL;
//extern int demux_get_stc(int demux_device_index, int index,
//                  u64 *stc, unsigned int *base);
extern int demux_get_pcr(int demux_device_index, int index, u64 *pcr);

static u64 get_llabs(s64 value){
	u64 llvalue;
	if (value > 0) {
		return value;
	} else {
		llvalue = (u64)(0-value);
		return llvalue;
	}
}

static u64 get_stc_time_us(s32 sSyncInsId)
{
    /*mediasync_ins* pInstance = NULL;
	u64 stc;
	unsigned int base;
      s32 index = sSyncInsId;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	pInstance = vMediaSyncInsList[index];
	demux_get_stc(pInstance->mDemuxId, 0, &stc, &base);*/
	mediasync_ins* pInstance = NULL;
	int ret = -1;
	u64 stc;
	u64 timeus;
	u64 pcr;
	s64 pcr_diff;
	s64 time_diff;
	s32 index = sSyncInsId;
	struct timespec64 ts_monotonic;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;
	pInstance = vMediaSyncInsList[index].pInstance;
	if (pInstance->mSyncMode != MEDIA_SYNC_PCRMASTER)
		return 0;
	if (!amldemux_pcrscr_get)
		amldemux_pcrscr_get = symbol_request(demux_get_pcr);
	if (!amldemux_pcrscr_get)
		return 0;
	ktime_get_ts64(&ts_monotonic);
	timeus = ts_monotonic.tv_sec * 1000000LL + div_u64(ts_monotonic.tv_nsec , 1000);
	if (pInstance->mDemuxId < 0)
		return timeus;


	ret = amldemux_pcrscr_get(pInstance->mDemuxId, 0, &pcr);

	if (ret != 0) {
		stc = timeus;
	} else {
		if (last_pcr == 0) {
			stc = timeus;
			last_pcr = div_u64(pcr * 100 , 9);
			last_system = timeus;
		} else {
			pcr_diff = div_u64(pcr * 100 , 9) - last_pcr;
			time_diff = timeus - last_system;
			if (time_diff && (div_u64(get_llabs(pcr_diff) , (s32)time_diff)
					    > 100)) {
				last_pcr = div_u64(pcr * 100 , 9);
				last_system = timeus;
				stc = timeus;
			} else {
				if (time_diff)
					stc = last_system + pcr_diff;
				else
					stc = timeus;

				last_pcr = div_u64(pcr * 100 , 9);
				last_system = stc;
			}
		}
	}
	pr_debug("get_stc_time_us stc:%lld pcr:%lld system_time:%lld\n", stc,  div_u64(pcr * 100 , 9),  timeus);
	return stc;
}

static s64 get_system_time_us(void) {
	s64 TimeUs;
	struct timespec64 ts_monotonic;
	ktime_get_ts64(&ts_monotonic);
	TimeUs = ts_monotonic.tv_sec * 1000000LL + div_u64(ts_monotonic.tv_nsec , 1000);
	pr_debug("get_system_time_us %lld\n", TimeUs);
	return TimeUs;
}

long mediasync_init(void) {
	int index = 0;
	for (index = 0; index < MAX_INSTANCE_NUM; index++) {
		vMediaSyncInsList[index].pInstance = NULL;
		mutex_init(&(vMediaSyncInsList[index].m_lock));
	}
	return 0;
}

long mediasync_ins_alloc(s32 sDemuxId,
			s32 sPcrPid,
			s32 *sSyncInsId,
			mediasync_ins **pIns){
	s32 index = 0;
	mediasync_ins* pInstance = NULL;
	pInstance = kzalloc(sizeof(mediasync_ins), GFP_KERNEL);
	if (pInstance == NULL) {
		return -1;
	}

	for (index = 0; index < MAX_INSTANCE_NUM; index++) {
		mutex_lock(&(vMediaSyncInsList[index].m_lock));
		if (vMediaSyncInsList[index].pInstance == NULL) {
			vMediaSyncInsList[index].pInstance = pInstance;
			pInstance->mSyncInsId = index;
			*sSyncInsId = index;
			pr_info("mediasync_ins_alloc index:%d, demuxid:%d.\n", index, sDemuxId);

			pInstance->mDemuxId = sDemuxId;
			pInstance->mPcrPid = sPcrPid;
			mediasync_ins_init_syncinfo(pInstance->mSyncInsId);
			pInstance->mHasAudio = -1;
			pInstance->mHasVideo = -1;
			pInstance->mVideoWorkMode = 0;
			pInstance->mFccEnable = 0;
			pInstance->mSourceClockType = UNKNOWN_CLOCK;
			pInstance->mSyncInfo.state = MEDIASYNC_INIT;
			pInstance->mSourceClockState = CLOCK_PROVIDER_NORMAL;
			pInstance->mute_flag = false;
			pInstance->mSourceType = TS_DEMOD;
			pInstance->mUpdateTimeThreshold = MIN_UPDATETIME_THRESHOLD_US;
			pInstance->mRef++;
			snprintf(pInstance->atrace_video,
				sizeof(pInstance->atrace_video), "msync_v_%d", *sSyncInsId);
			snprintf(pInstance->atrace_audio,
				sizeof(pInstance->atrace_audio), "msync_a_%d", *sSyncInsId);
			snprintf(pInstance->atrace_pcrscr,
				sizeof(pInstance->atrace_pcrscr), "msync_s_%d", *sSyncInsId);

			mutex_unlock(&(vMediaSyncInsList[index].m_lock));
			break;
		}
		mutex_unlock(&(vMediaSyncInsList[index].m_lock));
	}

	if (index == MAX_INSTANCE_NUM) {
		kzfree(pInstance);
		return -1;
	}


	*pIns = pInstance;

	return 0;
}


long mediasync_ins_delete(s32 sSyncInsId) {
	mediasync_ins* pInstance = NULL;
	s32 index = sSyncInsId;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	pInstance = vMediaSyncInsList[index].pInstance;
	if (pInstance == NULL)
		return -1;

	kzfree(pInstance);
	vMediaSyncInsList[index].pInstance = NULL;
	return 0;
}

long mediasync_ins_binder(s32 sSyncInsId,
			mediasync_ins **pIns) {
	mediasync_ins* pInstance = NULL;
	s32 index = sSyncInsId;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;
	mutex_lock(&(vMediaSyncInsList[index].m_lock));
	pInstance = vMediaSyncInsList[index].pInstance;
	if (pInstance == NULL) {
		mutex_unlock(&(vMediaSyncInsList[index].m_lock));
		return -1;
	}

	pInstance->mRef++;
	*pIns = pInstance;
	mutex_unlock(&(vMediaSyncInsList[index].m_lock));

	return 0;
}

long mediasync_ins_unbinder(s32 sSyncInsId) {
	mediasync_ins* pInstance = NULL;
	s32 index = sSyncInsId;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	pInstance = vMediaSyncInsList[index].pInstance;
	if (pInstance == NULL)
		return -1;
	mutex_lock(&(vMediaSyncInsList[index].m_lock));
	pInstance->mRef--;

	if (pInstance->mRef > 0 && pInstance->mAVRef == 0)
		mediasync_ins_reset(sSyncInsId);

	if (pInstance->mRef <= 0)
		mediasync_ins_delete(sSyncInsId);
	mutex_unlock(&(vMediaSyncInsList[index].m_lock));
	return 0;
}

long mediasync_ins_reset(s32 sSyncInsId) {
	mediasync_ins* pInstance = NULL;
	s32 index = sSyncInsId;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	pInstance = vMediaSyncInsList[index].pInstance;
	if (pInstance == NULL)
		return -1;

	mediasync_ins_init_syncinfo(pInstance->mSyncInsId);
	pInstance->mHasAudio = -1;
	pInstance->mHasVideo = -1;
	pInstance->mVideoWorkMode = 0;
	pInstance->mFccEnable = 0;
	pInstance->mPaused = 0;
	pInstance->mSourceClockType = UNKNOWN_CLOCK;
	pInstance->mSyncInfo.state = MEDIASYNC_INIT;
	pInstance->mSourceClockState = CLOCK_PROVIDER_NORMAL;
	pInstance->mute_flag = false;
	pInstance->mSourceType = TS_DEMOD;
	pInstance->mUpdateTimeThreshold = MIN_UPDATETIME_THRESHOLD_US;
	pr_info("mediasync_ins_reset.");

	return 0;
}

long mediasync_ins_init_syncinfo(s32 sSyncInsId) {
	mediasync_ins* pInstance = NULL;
	s32 index = sSyncInsId;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	pInstance = vMediaSyncInsList[index].pInstance;
	if (pInstance == NULL)
		return -1;
	pInstance->mSyncInfo.state = MEDIASYNC_INIT;
	pInstance->mSyncInfo.firstAframeInfo.framePts = -1;
	pInstance->mSyncInfo.firstAframeInfo.frameSystemTime = -1;
	pInstance->mSyncInfo.firstVframeInfo.framePts = -1;
	pInstance->mSyncInfo.firstVframeInfo.frameSystemTime = -1;
	pInstance->mSyncInfo.firstDmxPcrInfo.framePts = -1;
	pInstance->mSyncInfo.firstDmxPcrInfo.frameSystemTime = -1;
	pInstance->mSyncInfo.refClockInfo.framePts = -1;
	pInstance->mSyncInfo.refClockInfo.frameSystemTime = -1;
	pInstance->mSyncInfo.curAudioInfo.framePts = -1;
	pInstance->mSyncInfo.curAudioInfo.frameSystemTime = -1;
	pInstance->mSyncInfo.curVideoInfo.framePts = -1;
	pInstance->mSyncInfo.curVideoInfo.frameSystemTime = -1;
	pInstance->mSyncInfo.curDmxPcrInfo.framePts = -1;
	pInstance->mSyncInfo.curDmxPcrInfo.frameSystemTime = -1;
	pInstance->mSyncInfo.queueAudioInfo.framePts = -1;
	pInstance->mSyncInfo.queueAudioInfo.frameSystemTime = -1;
	pInstance->mSyncInfo.queueVideoInfo.framePts = -1;
	pInstance->mSyncInfo.queueVideoInfo.frameSystemTime = -1;
	pInstance->mSyncInfo.firstQAudioInfo.framePts = -1;
	pInstance->mSyncInfo.firstQAudioInfo.frameSystemTime = -1;
	pInstance->mSyncInfo.firstQVideoInfo.framePts = -1;
	pInstance->mSyncInfo.firstQVideoInfo.frameSystemTime = -1;

	pInstance->mAudioInfo.cacheSize = -1;
	pInstance->mAudioInfo.cacheDuration = -1;
	pInstance->mVideoInfo.cacheSize = -1;
	pInstance->mVideoInfo.cacheDuration = -1;
	pInstance->mPauseResumeFlag = 0;
	pInstance->mSpeed.mNumerator = 1;
	pInstance->mSpeed.mDenominator = 1;
	pInstance->mPcrSlope.mNumerator = 1;
	pInstance->mPcrSlope.mDenominator = 1;
	pInstance->mAVRef = 0;

	return 0;
}

long mediasync_ins_update_mediatime(s32 sSyncInsId,
				s64 lMediaTime,
				s64 lSystemTime, bool forceUpdate) {
	mediasync_ins* pInstance = NULL;
	u64 current_stc = 0;
	s64 current_systemtime = 0;
	s64 diff_system_time = 0;
	s64 diff_mediatime = 0;
	s32 index = sSyncInsId;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	mutex_lock(&(vMediaSyncInsList[index].m_lock));
	pInstance = vMediaSyncInsList[index].pInstance;
	if (pInstance == NULL) {
		mutex_unlock(&(vMediaSyncInsList[index].m_lock));
		return -1;
	}

	current_stc = get_stc_time_us(sSyncInsId);
	current_systemtime = get_system_time_us();
#if 0
	pInstance->mSyncMode = MEDIA_SYNC_PCRMASTER;
#endif
	if (pInstance->mSyncMode == MEDIA_SYNC_PCRMASTER) {
		if (lSystemTime == 0) {
			if (current_stc != 0) {
				diff_system_time = current_stc - pInstance->mLastStc;
				diff_mediatime = lMediaTime - pInstance->mLastMediaTime;
			} else {
				diff_system_time = current_systemtime - pInstance->mLastRealTime;
				diff_mediatime = lMediaTime - pInstance->mLastMediaTime;
			}
			if (pInstance->mSyncModeChange == 1
				|| diff_mediatime < 0
				|| ((diff_mediatime > 0)
				&& (get_llabs(diff_system_time - diff_mediatime) > pInstance->mUpdateTimeThreshold))) {
				pr_info("MEDIA_SYNC_PCRMASTER update time\n");
				pInstance->mLastMediaTime = lMediaTime;
				pInstance->mLastRealTime = current_systemtime;
				pInstance->mLastStc = current_stc;
				pInstance->mSyncModeChange = 0;
			}
		} else {
			if (current_stc != 0) {
				diff_system_time = lSystemTime - pInstance->mLastRealTime;
				diff_mediatime = lMediaTime - pInstance->mLastMediaTime;
			} else {
				diff_system_time = lSystemTime - pInstance->mLastRealTime;
				diff_mediatime = lMediaTime - pInstance->mLastMediaTime;
			}

			if (pInstance->mSyncModeChange == 1
				|| diff_mediatime < 0
				|| ((diff_mediatime > 0)
				&& (get_llabs(diff_system_time - diff_mediatime) > pInstance->mUpdateTimeThreshold))) {
				pInstance->mLastMediaTime = lMediaTime;
				pInstance->mLastRealTime = lSystemTime;
				pInstance->mLastStc = current_stc + lSystemTime - current_systemtime;
				pInstance->mSyncModeChange = 0;
			}
		}
	} else {
		if (lSystemTime == 0) {
			diff_system_time = current_systemtime - pInstance->mLastRealTime;
			diff_mediatime = lMediaTime - pInstance->mLastMediaTime;

			if (pInstance->mSyncModeChange == 1
				|| forceUpdate
				|| diff_mediatime < 0
				|| ((diff_mediatime > 0)
				&& (get_llabs(diff_system_time - diff_mediatime) > pInstance->mUpdateTimeThreshold))) {
				pr_info("mSyncMode:%d update time system diff:%lld media diff:%lld current:%lld\n",
					pInstance->mSyncMode,
					diff_system_time,
					diff_mediatime,
					current_systemtime);
				pInstance->mLastMediaTime = lMediaTime;
				pInstance->mLastRealTime = current_systemtime;
				pInstance->mLastStc = current_stc;
				pInstance->mSyncModeChange = 0;
			}
	} else {
			diff_system_time = lSystemTime - pInstance->mLastRealTime;
			diff_mediatime = lMediaTime - pInstance->mLastMediaTime;
			if (pInstance->mSyncModeChange == 1
				|| forceUpdate
				|| diff_mediatime < 0
				|| ((diff_mediatime > 0)
				&& (get_llabs(diff_system_time - diff_mediatime) > pInstance->mUpdateTimeThreshold))) {
				pr_info("mSyncMode:%d update time stc diff:%lld media diff:%lld lSystemTime:%lld lMediaTime:%lld\n",
					pInstance->mSyncMode,
					diff_system_time,
					diff_mediatime,
					lSystemTime,
					lMediaTime);
				pInstance->mLastMediaTime = lMediaTime;
				pInstance->mLastRealTime = lSystemTime;
				pInstance->mLastStc = current_stc + lSystemTime - current_systemtime;
				pInstance->mSyncModeChange = 0;
			}
		}
	}
	pInstance->mTrackMediaTime = lMediaTime;
	mutex_unlock(&(vMediaSyncInsList[index].m_lock));
	return 0;
}

long mediasync_ins_set_mediatime_speed(s32 sSyncInsId,
					mediasync_speed fSpeed) {
	mediasync_ins* pInstance = NULL;
	s32 index = sSyncInsId;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	mutex_lock(&(vMediaSyncInsList[index].m_lock));
	pInstance = vMediaSyncInsList[index].pInstance;
	if (pInstance == NULL) {
		mutex_unlock(&(vMediaSyncInsList[index].m_lock));
		return -1;
	}

	pInstance->mSpeed.mNumerator = fSpeed.mNumerator;
	pInstance->mSpeed.mDenominator = fSpeed.mDenominator;
	mutex_unlock(&(vMediaSyncInsList[index].m_lock));

	return 0;
}

long mediasync_ins_set_paused(s32 sSyncInsId, s32 sPaused) {

	mediasync_ins* pInstance = NULL;
	u64 current_stc = 0;
	s64 current_systemtime = 0;
	s32 index = sSyncInsId;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	mutex_lock(&(vMediaSyncInsList[index].m_lock));
	pInstance = vMediaSyncInsList[index].pInstance;
	if (pInstance == NULL) {
		mutex_unlock(&(vMediaSyncInsList[index].m_lock));
		return -1;
	}

	if (sPaused != 0 && sPaused != 1) {
		mutex_unlock(&(vMediaSyncInsList[index].m_lock));
		return -1;
	}
	if (sPaused == pInstance->mPaused) {
		mutex_unlock(&(vMediaSyncInsList[index].m_lock));
		return 0;
	}

	current_stc = get_stc_time_us(sSyncInsId);
	current_systemtime = get_system_time_us();

	pInstance->mPaused = sPaused;

	if (pInstance->mSyncMode == MEDIA_SYNC_AMASTER)
		pInstance->mLastMediaTime = pInstance->mLastMediaTime +
		(current_systemtime - pInstance->mLastRealTime);

	pInstance->mLastRealTime = current_systemtime;
	pInstance->mLastStc = current_stc;
	mutex_unlock(&(vMediaSyncInsList[index].m_lock));

	return 0;
}

long mediasync_ins_get_paused(s32 sSyncInsId, s32* spPaused) {

	mediasync_ins* pInstance = NULL;
	s32 index = sSyncInsId;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	mutex_lock(&(vMediaSyncInsList[index].m_lock));
	pInstance = vMediaSyncInsList[index].pInstance;
	if (pInstance == NULL) {
		mutex_unlock(&(vMediaSyncInsList[index].m_lock));
		return -1;
	}
	*spPaused = pInstance->mPaused ;
	mutex_unlock(&(vMediaSyncInsList[index].m_lock));

	return 0;
}

long mediasync_ins_set_syncmode(s32 sSyncInsId, s32 sSyncMode){
	mediasync_ins* pInstance = NULL;
	s32 index = sSyncInsId;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	mutex_lock(&(vMediaSyncInsList[index].m_lock));
	pInstance = vMediaSyncInsList[index].pInstance;
	if (pInstance == NULL) {
		mutex_unlock(&(vMediaSyncInsList[index].m_lock));
		return -1;
	}

	pInstance->mSyncMode = sSyncMode;
	pInstance->mSyncModeChange = 1;
	mutex_unlock(&(vMediaSyncInsList[index].m_lock));

	return 0;
}

long mediasync_ins_get_syncmode(s32 sSyncInsId, s32 *sSyncMode) {
	mediasync_ins* pInstance = NULL;
	s32 index = sSyncInsId;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	mutex_lock(&(vMediaSyncInsList[index].m_lock));
	pInstance = vMediaSyncInsList[index].pInstance;
	if (pInstance == NULL) {
		mutex_unlock(&(vMediaSyncInsList[index].m_lock));
		return -1;
	}
	*sSyncMode = pInstance->mSyncMode;
	mutex_unlock(&(vMediaSyncInsList[index].m_lock));

	return 0;
}

long mediasync_ins_get_mediatime_speed(s32 sSyncInsId, mediasync_speed *fpSpeed) {
	mediasync_ins* pInstance = NULL;
	s32 index = sSyncInsId;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	mutex_lock(&(vMediaSyncInsList[index].m_lock));
	pInstance = vMediaSyncInsList[index].pInstance;
	if (pInstance == NULL) {
		mutex_unlock(&(vMediaSyncInsList[index].m_lock));
		return -1;
	}
	fpSpeed->mNumerator = pInstance->mSpeed.mNumerator;
	fpSpeed->mDenominator = pInstance->mSpeed.mDenominator;
	mutex_unlock(&(vMediaSyncInsList[index].m_lock));

	return 0;
}

long mediasync_ins_get_anchor_time(s32 sSyncInsId,
				s64* lpMediaTime,
				s64* lpSTCTime,
				s64* lpSystemTime) {
	mediasync_ins* pInstance = NULL;
	s32 index = sSyncInsId;

	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	mutex_lock(&(vMediaSyncInsList[index].m_lock));
	pInstance = vMediaSyncInsList[index].pInstance;
	if (pInstance == NULL) {
		mutex_unlock(&(vMediaSyncInsList[index].m_lock));
		return -1;
	}

	*lpMediaTime = pInstance->mLastMediaTime;
	*lpSTCTime = pInstance->mLastStc;
	*lpSystemTime = pInstance->mLastRealTime;
	mutex_unlock(&(vMediaSyncInsList[index].m_lock));

	return 0;
}

long mediasync_ins_get_systemtime(s32 sSyncInsId, s64* lpSTC, s64* lpSystemTime){
	mediasync_ins* pInstance = NULL;
	u64 current_stc = 0;
	s64 current_systemtime = 0;
	s32 index = sSyncInsId;

	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	mutex_lock(&(vMediaSyncInsList[index].m_lock));
	pInstance = vMediaSyncInsList[index].pInstance;
	if (pInstance == NULL) {
		mutex_unlock(&(vMediaSyncInsList[index].m_lock));
		return -1;
	}

	current_stc = get_stc_time_us(sSyncInsId);
	current_systemtime = get_system_time_us();

	*lpSTC = current_stc;
	*lpSystemTime = current_systemtime;
	mutex_unlock(&(vMediaSyncInsList[index].m_lock));

	return 0;
}

long mediasync_ins_get_nextvsync_systemtime(s32 sSyncInsId, s64* lpSystemTime) {

	return 0;
}

long mediasync_ins_set_updatetime_threshold(s32 sSyncInsId, s64 lTimeThreshold) {
	mediasync_ins* pInstance = NULL;
	s32 index = sSyncInsId;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	mutex_lock(&(vMediaSyncInsList[index].m_lock));
	pInstance = vMediaSyncInsList[index].pInstance;
	if (pInstance == NULL) {
		mutex_unlock(&(vMediaSyncInsList[index].m_lock));
		return -1;
	}
	pInstance->mUpdateTimeThreshold = lTimeThreshold;
	mutex_unlock(&(vMediaSyncInsList[index].m_lock));

	return 0;
}

long mediasync_ins_get_updatetime_threshold(s32 sSyncInsId, s64* lpTimeThreshold) {
	mediasync_ins* pInstance = NULL;
	s32 index = sSyncInsId;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	mutex_lock(&(vMediaSyncInsList[index].m_lock));
	pInstance = vMediaSyncInsList[index].pInstance;
	if (pInstance == NULL) {
		mutex_unlock(&(vMediaSyncInsList[index].m_lock));
		return -1;
	}
	*lpTimeThreshold = pInstance->mUpdateTimeThreshold;
	mutex_unlock(&(vMediaSyncInsList[index].m_lock));

	return 0;
}

long mediasync_ins_get_trackmediatime(s32 sSyncInsId, s64* lpTrackMediaTime) {

	mediasync_ins* pInstance = NULL;
	s32 index = sSyncInsId;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	mutex_lock(&(vMediaSyncInsList[index].m_lock));
	pInstance = vMediaSyncInsList[index].pInstance;
	if (pInstance == NULL) {
		mutex_unlock(&(vMediaSyncInsList[index].m_lock));
		return -1;
	}
	*lpTrackMediaTime = pInstance->mTrackMediaTime;
	mutex_unlock(&(vMediaSyncInsList[index].m_lock));

	return 0;
}
long mediasync_ins_set_clocktype(s32 sSyncInsId, mediasync_clocktype type) {
	mediasync_ins* pInstance = NULL;
	s32 index = sSyncInsId;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	mutex_lock(&(vMediaSyncInsList[index].m_lock));
	pInstance = vMediaSyncInsList[index].pInstance;
	if (pInstance == NULL) {
		mutex_unlock(&(vMediaSyncInsList[index].m_lock));
		return -1;
	}

	pInstance->mSourceClockType = type;
	mutex_unlock(&(vMediaSyncInsList[index].m_lock));

	return 0;
}

long mediasync_ins_get_clocktype(s32 sSyncInsId, mediasync_clocktype* type) {
	mediasync_ins* pInstance = NULL;
	s32 index = sSyncInsId;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	mutex_lock(&(vMediaSyncInsList[index].m_lock));
	pInstance = vMediaSyncInsList[index].pInstance;
	if (pInstance == NULL) {
		mutex_unlock(&(vMediaSyncInsList[index].m_lock));
		return -1;
	}

	*type = pInstance->mSourceClockType;
	mutex_unlock(&(vMediaSyncInsList[index].m_lock));

	return 0;
}

long mediasync_ins_set_clockstate(s32 sSyncInsId, mediasync_clockprovider_state state) {
	mediasync_ins* pInstance = NULL;
	s32 index = sSyncInsId;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	mutex_lock(&(vMediaSyncInsList[index].m_lock));
	pInstance = vMediaSyncInsList[index].pInstance;
	if (pInstance == NULL) {
		mutex_unlock(&(vMediaSyncInsList[index].m_lock));
		return -1;
	}

	pInstance->mSourceClockState = state;
	mutex_unlock(&(vMediaSyncInsList[index].m_lock));

	return 0;
}

long mediasync_ins_get_clockstate(s32 sSyncInsId, mediasync_clockprovider_state* state) {
	mediasync_ins* pInstance = NULL;
	s32 index = sSyncInsId;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	mutex_lock(&(vMediaSyncInsList[index].m_lock));
	pInstance = vMediaSyncInsList[index].pInstance;
	if (pInstance == NULL) {
		mutex_unlock(&(vMediaSyncInsList[index].m_lock));
		return -1;
	}

	*state = pInstance->mSourceClockState;
	mutex_unlock(&(vMediaSyncInsList[index].m_lock));

	return 0;
}

long mediasync_ins_set_hasaudio(s32 sSyncInsId, int hasaudio) {
	mediasync_ins* pInstance = NULL;
	s32 index = sSyncInsId;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	mutex_lock(&(vMediaSyncInsList[index].m_lock));
	pInstance = vMediaSyncInsList[index].pInstance;
	if (pInstance == NULL) {
		mutex_unlock(&(vMediaSyncInsList[index].m_lock));
		return -1;
	}

	pInstance->mHasAudio = hasaudio;
	mutex_unlock(&(vMediaSyncInsList[index].m_lock));

	return 0;
}

long mediasync_ins_get_hasaudio(s32 sSyncInsId, int* hasaudio) {
	mediasync_ins* pInstance = NULL;
	s32 index = sSyncInsId;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	mutex_lock(&(vMediaSyncInsList[index].m_lock));
	pInstance = vMediaSyncInsList[index].pInstance;
	if (pInstance == NULL) {
		mutex_unlock(&(vMediaSyncInsList[index].m_lock));
		return -1;
	}

	*hasaudio = pInstance->mHasAudio;
	mutex_unlock(&(vMediaSyncInsList[index].m_lock));

	return 0;
}
long mediasync_ins_set_hasvideo(s32 sSyncInsId, int hasvideo) {
	mediasync_ins* pInstance = NULL;
	s32 index = sSyncInsId;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	mutex_lock(&(vMediaSyncInsList[index].m_lock));
	pInstance = vMediaSyncInsList[index].pInstance;
	if (pInstance == NULL) {
		mutex_unlock(&(vMediaSyncInsList[index].m_lock));
		return -1;
	}

	pInstance->mHasVideo = hasvideo;
	mutex_unlock(&(vMediaSyncInsList[index].m_lock));

	return 0;
}

long mediasync_ins_get_hasvideo(s32 sSyncInsId, int* hasvideo) {
	mediasync_ins* pInstance = NULL;
	s32 index = sSyncInsId;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	mutex_lock(&(vMediaSyncInsList[index].m_lock));
	pInstance = vMediaSyncInsList[index].pInstance;
	if (pInstance == NULL) {
		mutex_unlock(&(vMediaSyncInsList[index].m_lock));
		return -1;
	}

	*hasvideo = pInstance->mHasVideo;
	mutex_unlock(&(vMediaSyncInsList[index].m_lock));

	return 0;
}

long mediasync_ins_set_firstaudioframeinfo(s32 sSyncInsId, mediasync_frameinfo info) {
	mediasync_ins* pInstance = NULL;
	s32 index = sSyncInsId;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	mutex_lock(&(vMediaSyncInsList[index].m_lock));
	pInstance = vMediaSyncInsList[index].pInstance;
	if (pInstance == NULL) {
		mutex_unlock(&(vMediaSyncInsList[index].m_lock));
		return -1;
	}

	pInstance->mSyncInfo.firstAframeInfo.framePts = info.framePts;
	pInstance->mSyncInfo.firstAframeInfo.frameSystemTime = info.frameSystemTime;
	mutex_unlock(&(vMediaSyncInsList[index].m_lock));

	return 0;
}

long mediasync_ins_get_firstaudioframeinfo(s32 sSyncInsId, mediasync_frameinfo* info) {
	mediasync_ins* pInstance = NULL;
	s32 index = sSyncInsId;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	mutex_lock(&(vMediaSyncInsList[index].m_lock));
	pInstance = vMediaSyncInsList[index].pInstance;
	if (pInstance == NULL) {
		mutex_unlock(&(vMediaSyncInsList[index].m_lock));
		return -1;
	}

	info->framePts = pInstance->mSyncInfo.firstAframeInfo.framePts;
	info->frameSystemTime = pInstance->mSyncInfo.firstAframeInfo.frameSystemTime;
	mutex_unlock(&(vMediaSyncInsList[index].m_lock));

	return 0;
}

long mediasync_ins_set_firstvideoframeinfo(s32 sSyncInsId, mediasync_frameinfo info) {
	mediasync_ins* pInstance = NULL;
	s32 index = sSyncInsId;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	mutex_lock(&(vMediaSyncInsList[index].m_lock));
	pInstance = vMediaSyncInsList[index].pInstance;
	if (pInstance == NULL) {
		mutex_unlock(&(vMediaSyncInsList[index].m_lock));
		return -1;
	}

	pInstance->mSyncInfo.firstVframeInfo.framePts = info.framePts;
	pInstance->mSyncInfo.firstVframeInfo.frameSystemTime = info.frameSystemTime;
	mutex_unlock(&(vMediaSyncInsList[index].m_lock));

	return 0;
}

long mediasync_ins_get_firstvideoframeinfo(s32 sSyncInsId, mediasync_frameinfo* info) {
	mediasync_ins* pInstance = NULL;
	s32 index = sSyncInsId;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	mutex_lock(&(vMediaSyncInsList[index].m_lock));
	pInstance = vMediaSyncInsList[index].pInstance;
	if (pInstance == NULL) {
		mutex_unlock(&(vMediaSyncInsList[index].m_lock));
		return -1;
	}

	info->framePts = pInstance->mSyncInfo.firstVframeInfo.framePts;
	info->frameSystemTime = pInstance->mSyncInfo.firstVframeInfo.frameSystemTime;
	mutex_unlock(&(vMediaSyncInsList[index].m_lock));

	return 0;
}

long mediasync_ins_set_firstdmxpcrinfo(s32 sSyncInsId, mediasync_frameinfo info) {
	mediasync_ins* pInstance = NULL;
	s32 index = sSyncInsId;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	mutex_lock(&(vMediaSyncInsList[index].m_lock));
	pInstance = vMediaSyncInsList[index].pInstance;
	if (pInstance == NULL) {
		mutex_unlock(&(vMediaSyncInsList[index].m_lock));
		return -1;
	}

	pInstance->mSyncInfo.firstDmxPcrInfo.framePts = info.framePts;
	pInstance->mSyncInfo.firstDmxPcrInfo.frameSystemTime = info.frameSystemTime;
	mutex_unlock(&(vMediaSyncInsList[index].m_lock));

	return 0;
}

long mediasync_ins_get_firstdmxpcrinfo(s32 sSyncInsId, mediasync_frameinfo* info) {
	mediasync_ins* pInstance = NULL;
	s32 index = sSyncInsId;
	int64_t pcr = -1;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	mutex_lock(&(vMediaSyncInsList[index].m_lock));
	pInstance = vMediaSyncInsList[index].pInstance;
	if (pInstance == NULL) {
		mutex_unlock(&(vMediaSyncInsList[index].m_lock));
		return -1;
	}

	if (pInstance->mSyncInfo.firstDmxPcrInfo.framePts == -1) {
		if (amldemux_pcrscr_get && pInstance->mDemuxId >= 0) {
			amldemux_pcrscr_get(pInstance->mDemuxId, 0, &pcr);
			pInstance->mSyncInfo.firstDmxPcrInfo.framePts = pcr;
			pInstance->mSyncInfo.firstDmxPcrInfo.frameSystemTime = get_system_time_us();
		} else {
			pInstance->mSyncInfo.firstDmxPcrInfo.framePts = pcr;
			pInstance->mSyncInfo.firstDmxPcrInfo.frameSystemTime = get_system_time_us();
		}
	}

	info->framePts = pInstance->mSyncInfo.firstDmxPcrInfo.framePts;
	info->frameSystemTime = pInstance->mSyncInfo.firstDmxPcrInfo.frameSystemTime;
	mutex_unlock(&(vMediaSyncInsList[index].m_lock));

	return 0;
}

long mediasync_ins_set_refclockinfo(s32 sSyncInsId, mediasync_frameinfo info) {
	mediasync_ins* pInstance = NULL;
	s32 index = sSyncInsId;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	mutex_lock(&(vMediaSyncInsList[index].m_lock));
	pInstance = vMediaSyncInsList[index].pInstance;
	if (pInstance == NULL) {
		mutex_unlock(&(vMediaSyncInsList[index].m_lock));
		return -1;
	}

	pInstance->mSyncInfo.refClockInfo.framePts = info.framePts;
	pInstance->mSyncInfo.refClockInfo.frameSystemTime = info.frameSystemTime;
	mutex_unlock(&(vMediaSyncInsList[index].m_lock));

	return 0;
}

long mediasync_ins_get_refclockinfo(s32 sSyncInsId, mediasync_frameinfo* info) {
	mediasync_ins* pInstance = NULL;
	s32 index = sSyncInsId;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	mutex_lock(&(vMediaSyncInsList[index].m_lock));
	pInstance = vMediaSyncInsList[index].pInstance;
	if (pInstance == NULL) {
		mutex_unlock(&(vMediaSyncInsList[index].m_lock));
		return -1;
	}

	info->framePts = pInstance->mSyncInfo.refClockInfo.framePts;
	info->frameSystemTime = pInstance->mSyncInfo.refClockInfo.frameSystemTime;
	mutex_unlock(&(vMediaSyncInsList[index].m_lock));

	return 0;
}

long mediasync_ins_set_curaudioframeinfo(s32 sSyncInsId, mediasync_frameinfo info) {
	mediasync_ins* pInstance = NULL;
	s32 index = sSyncInsId;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	mutex_lock(&(vMediaSyncInsList[index].m_lock));
	pInstance = vMediaSyncInsList[index].pInstance;
	if (pInstance == NULL) {
		mutex_unlock(&(vMediaSyncInsList[index].m_lock));
		return -1;
	}

	ATRACE_COUNTER(pInstance->atrace_audio, info.framePts);
	pInstance->mSyncInfo.curAudioInfo.framePts = info.framePts;
	pInstance->mSyncInfo.curAudioInfo.frameSystemTime = info.frameSystemTime;
	mutex_unlock(&(vMediaSyncInsList[index].m_lock));

	return 0;
}

long mediasync_ins_get_curaudioframeinfo(s32 sSyncInsId, mediasync_frameinfo* info) {
	mediasync_ins* pInstance = NULL;
	s32 index = sSyncInsId;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	mutex_lock(&(vMediaSyncInsList[index].m_lock));
	pInstance = vMediaSyncInsList[index].pInstance;
	if (pInstance == NULL) {
		mutex_unlock(&(vMediaSyncInsList[index].m_lock));
		return -1;
	}

	info->framePts = pInstance->mSyncInfo.curAudioInfo.framePts;
	info->frameSystemTime = pInstance->mSyncInfo.curAudioInfo.frameSystemTime;
	mutex_unlock(&(vMediaSyncInsList[index].m_lock));

	return 0;
}

long mediasync_ins_set_curvideoframeinfo(s32 sSyncInsId, mediasync_frameinfo info) {
	mediasync_ins* pInstance = NULL;
	s32 index = sSyncInsId;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	mutex_lock(&(vMediaSyncInsList[index].m_lock));
	pInstance = vMediaSyncInsList[index].pInstance;
	if (pInstance == NULL) {
		mutex_unlock(&(vMediaSyncInsList[index].m_lock));
		return -1;
	}

	ATRACE_COUNTER(pInstance->atrace_video, info.framePts);
	pInstance->mSyncInfo.curVideoInfo.framePts = info.framePts;
	pInstance->mSyncInfo.curVideoInfo.frameSystemTime = info.frameSystemTime;
	pInstance->mTrackMediaTime = div_u64(info.framePts * 100 , 9);
	mutex_unlock(&(vMediaSyncInsList[index].m_lock));

	return 0;
}

long mediasync_ins_get_curvideoframeinfo(s32 sSyncInsId, mediasync_frameinfo* info) {
	mediasync_ins* pInstance = NULL;
	s32 index = sSyncInsId;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	mutex_lock(&(vMediaSyncInsList[index].m_lock));
	pInstance = vMediaSyncInsList[index].pInstance;
	if (pInstance == NULL) {
		mutex_unlock(&(vMediaSyncInsList[index].m_lock));
		return -1;
	}

	info->framePts = pInstance->mSyncInfo.curVideoInfo.framePts;
	info->frameSystemTime = pInstance->mSyncInfo.curVideoInfo.frameSystemTime;
	mutex_unlock(&(vMediaSyncInsList[index].m_lock));

	return 0;
}

long mediasync_ins_set_curdmxpcrinfo(s32 sSyncInsId, mediasync_frameinfo info) {
	mediasync_ins* pInstance = NULL;
	s32 index = sSyncInsId;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	mutex_lock(&(vMediaSyncInsList[index].m_lock));
	pInstance = vMediaSyncInsList[index].pInstance;
	if (pInstance == NULL) {
		mutex_unlock(&(vMediaSyncInsList[index].m_lock));
		return -1;
	}

	pInstance->mSyncInfo.curDmxPcrInfo.framePts = info.framePts;
	pInstance->mSyncInfo.curDmxPcrInfo.frameSystemTime = info.frameSystemTime;
	mutex_unlock(&(vMediaSyncInsList[index].m_lock));

	return 0;
}

long mediasync_ins_get_curdmxpcrinfo(s32 sSyncInsId, mediasync_frameinfo* info) {
	mediasync_ins* pInstance = NULL;
	s32 index = sSyncInsId;
	int64_t pcr = -1;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	mutex_lock(&(vMediaSyncInsList[index].m_lock));
	pInstance = vMediaSyncInsList[index].pInstance;
	if (pInstance == NULL) {
		mutex_unlock(&(vMediaSyncInsList[index].m_lock));
		return -1;
	}

	if (amldemux_pcrscr_get && pInstance->mDemuxId >= 0) {
		amldemux_pcrscr_get(pInstance->mDemuxId, 0, &pcr);
		pInstance->mSyncInfo.curDmxPcrInfo.framePts = pcr;
		pInstance->mSyncInfo.curDmxPcrInfo.frameSystemTime = get_system_time_us();
	} else {
		pInstance->mSyncInfo.curDmxPcrInfo.framePts = -1;
		pInstance->mSyncInfo.curDmxPcrInfo.frameSystemTime = -1;
	}

	info->framePts = pInstance->mSyncInfo.curDmxPcrInfo.framePts;
	info->frameSystemTime = pInstance->mSyncInfo.curDmxPcrInfo.frameSystemTime;
	mutex_unlock(&(vMediaSyncInsList[index].m_lock));

	return 0;
}

long mediasync_ins_set_audiomute(s32 sSyncInsId, int mute_flag) {
	mediasync_ins* pInstance = NULL;
	s32 index = sSyncInsId;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	mutex_lock(&(vMediaSyncInsList[index].m_lock));
	pInstance = vMediaSyncInsList[index].pInstance;
	if (pInstance == NULL) {
		mutex_unlock(&(vMediaSyncInsList[index].m_lock));
		return -1;
	}

	pInstance->mute_flag = mute_flag;
	mutex_unlock(&(vMediaSyncInsList[index].m_lock));

	return 0;
}

long mediasync_ins_get_audiomute(s32 sSyncInsId, int* mute_flag) {
	mediasync_ins* pInstance = NULL;
	s32 index = sSyncInsId;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	mutex_lock(&(vMediaSyncInsList[index].m_lock));
	pInstance = vMediaSyncInsList[index].pInstance;
	if (pInstance == NULL) {
		mutex_unlock(&(vMediaSyncInsList[index].m_lock));
		return -1;
	}

	*mute_flag = pInstance->mute_flag;
	mutex_unlock(&(vMediaSyncInsList[index].m_lock));

	return 0;
}

long mediasync_ins_set_audioinfo(s32 sSyncInsId, mediasync_audioinfo info) {
	mediasync_ins* pInstance = NULL;
	s32 index = sSyncInsId;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	mutex_lock(&(vMediaSyncInsList[index].m_lock));
	pInstance = vMediaSyncInsList[index].pInstance;
	if (pInstance == NULL) {
		mutex_unlock(&(vMediaSyncInsList[index].m_lock));
		return -1;
	}

	pInstance->mAudioInfo.cacheDuration = info.cacheDuration;
	pInstance->mAudioInfo.cacheSize = info.cacheSize;
	mutex_unlock(&(vMediaSyncInsList[index].m_lock));

	return 0;
}

long mediasync_ins_get_audioinfo(s32 sSyncInsId, mediasync_audioinfo* info) {
	mediasync_ins* pInstance = NULL;
	s32 index = sSyncInsId;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	mutex_lock(&(vMediaSyncInsList[index].m_lock));
	pInstance = vMediaSyncInsList[index].pInstance;
	if (pInstance == NULL) {
		mutex_unlock(&(vMediaSyncInsList[index].m_lock));
		return -1;
	}

	info->cacheDuration = pInstance->mAudioInfo.cacheDuration;
	info->cacheDuration = pInstance->mAudioInfo.cacheDuration;
	mutex_unlock(&(vMediaSyncInsList[index].m_lock));

	return 0;
}

long mediasync_ins_set_videoinfo(s32 sSyncInsId, mediasync_videoinfo info) {
	mediasync_ins* pInstance = NULL;
	s32 index = sSyncInsId;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	mutex_lock(&(vMediaSyncInsList[index].m_lock));
	pInstance = vMediaSyncInsList[index].pInstance;
	if (pInstance == NULL) {
		mutex_unlock(&(vMediaSyncInsList[index].m_lock));
		return -1;
	}

	pInstance->mVideoInfo.cacheDuration = info.cacheDuration;
	pInstance->mVideoInfo.cacheSize = info.cacheSize;
	pInstance->mVideoInfo.specialSizeCount = info.specialSizeCount;
	mutex_unlock(&(vMediaSyncInsList[index].m_lock));

	return 0;

}

long mediasync_ins_get_videoinfo(s32 sSyncInsId, mediasync_videoinfo* info) {
	mediasync_ins* pInstance = NULL;
	s32 index = sSyncInsId;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	mutex_lock(&(vMediaSyncInsList[index].m_lock));
	pInstance = vMediaSyncInsList[index].pInstance;
	if (pInstance == NULL) {
		mutex_unlock(&(vMediaSyncInsList[index].m_lock));
		return -1;
	}

	info->cacheDuration = pInstance->mVideoInfo.cacheDuration;
	info->cacheSize = pInstance->mVideoInfo.cacheSize;
	info->specialSizeCount = pInstance->mVideoInfo.specialSizeCount;
	mutex_unlock(&(vMediaSyncInsList[index].m_lock));

	return 0;
}

long mediasync_ins_set_avsyncstate(s32 sSyncInsId, s32 state) {
	mediasync_ins* pInstance = NULL;
	s32 index = sSyncInsId;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	mutex_lock(&(vMediaSyncInsList[index].m_lock));
	pInstance = vMediaSyncInsList[index].pInstance;
	if (pInstance == NULL) {
		mutex_unlock(&(vMediaSyncInsList[index].m_lock));
		return -1;
	}

	pInstance->mSyncInfo.state = state;
	mutex_unlock(&(vMediaSyncInsList[index].m_lock));

	return 0;
}

long mediasync_ins_get_avsyncstate(s32 sSyncInsId, s32* state) {
	mediasync_ins* pInstance = NULL;
	s32 index = sSyncInsId;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	mutex_lock(&(vMediaSyncInsList[index].m_lock));
	pInstance = vMediaSyncInsList[index].pInstance;
	if (pInstance == NULL) {
		mutex_unlock(&(vMediaSyncInsList[index].m_lock));
		return -1;
	}

	*state = pInstance->mSyncInfo.state;
	mutex_unlock(&(vMediaSyncInsList[index].m_lock));

	return 0;
}

long mediasync_ins_set_startthreshold(s32 sSyncInsId, s32 threshold) {
	mediasync_ins* pInstance = NULL;
	s32 index = sSyncInsId;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	mutex_lock(&(vMediaSyncInsList[index].m_lock));
	pInstance = vMediaSyncInsList[index].pInstance;
	if (pInstance == NULL) {
		mutex_unlock(&(vMediaSyncInsList[index].m_lock));
		return -1;
	}

	pInstance->mStartThreshold = threshold;
	mutex_unlock(&(vMediaSyncInsList[index].m_lock));

	return 0;
}

long mediasync_ins_get_startthreshold(s32 sSyncInsId, s32* threshold) {
	mediasync_ins* pInstance = NULL;
	s32 index = sSyncInsId;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	mutex_lock(&(vMediaSyncInsList[index].m_lock));
	pInstance = vMediaSyncInsList[index].pInstance;
	if (pInstance == NULL) {
		mutex_unlock(&(vMediaSyncInsList[index].m_lock));
		return -1;
	}

	*threshold = pInstance->mStartThreshold;
	mutex_unlock(&(vMediaSyncInsList[index].m_lock));

	return 0;
}

long mediasync_ins_set_ptsadjust(s32 sSyncInsId, s32 adujstpts) {
	mediasync_ins* pInstance = NULL;
	s32 index = sSyncInsId;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	mutex_lock(&(vMediaSyncInsList[index].m_lock));
	pInstance = vMediaSyncInsList[index].pInstance;
	if (pInstance == NULL) {
		mutex_unlock(&(vMediaSyncInsList[index].m_lock));
		return -1;
	}

	pInstance->mPtsAdjust = adujstpts;
	mutex_unlock(&(vMediaSyncInsList[index].m_lock));

	return 0;
}

long mediasync_ins_get_ptsadjust(s32 sSyncInsId, s32* adujstpts) {
	mediasync_ins* pInstance = NULL;
	s32 index = sSyncInsId;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	mutex_lock(&(vMediaSyncInsList[index].m_lock));
	pInstance = vMediaSyncInsList[index].pInstance;
	if (pInstance == NULL) {
		mutex_unlock(&(vMediaSyncInsList[index].m_lock));
		return -1;
	}

	*adujstpts = pInstance->mPtsAdjust;
	mutex_unlock(&(vMediaSyncInsList[index].m_lock));

	return 0;
}

long mediasync_ins_set_videoworkmode(s32 sSyncInsId, s64 mode) {
	mediasync_ins* pInstance = NULL;
	s32 index = sSyncInsId;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	mutex_lock(&(vMediaSyncInsList[index].m_lock));
	pInstance = vMediaSyncInsList[index].pInstance;
	if (pInstance == NULL) {
		mutex_unlock(&(vMediaSyncInsList[index].m_lock));
		return -1;
	}

	pInstance->mVideoWorkMode = mode;
	mutex_unlock(&(vMediaSyncInsList[index].m_lock));

	return 0;
}

long mediasync_ins_get_videoworkmode(s32 sSyncInsId, s64* mode) {
	mediasync_ins* pInstance = NULL;
	s32 index = sSyncInsId;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	mutex_lock(&(vMediaSyncInsList[index].m_lock));
	pInstance = vMediaSyncInsList[index].pInstance;
	if (pInstance == NULL) {
		mutex_unlock(&(vMediaSyncInsList[index].m_lock));
		return -1;
	}

	*mode = pInstance->mVideoWorkMode;
	mutex_unlock(&(vMediaSyncInsList[index].m_lock));

	return 0;
}

long mediasync_ins_set_fccenable(s32 sSyncInsId, s64 enable) {
	mediasync_ins* pInstance = NULL;
	s32 index = sSyncInsId;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	mutex_lock(&(vMediaSyncInsList[index].m_lock));
	pInstance = vMediaSyncInsList[index].pInstance;
	if (pInstance == NULL) {
		mutex_unlock(&(vMediaSyncInsList[index].m_lock));
		return -1;
	}

	pInstance->mFccEnable = enable;
	mutex_unlock(&(vMediaSyncInsList[index].m_lock));

	return 0;
}

long mediasync_ins_get_fccenable(s32 sSyncInsId, s64* enable) {
	mediasync_ins* pInstance = NULL;
	s32 index = sSyncInsId;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	mutex_lock(&(vMediaSyncInsList[index].m_lock));
	pInstance = vMediaSyncInsList[index].pInstance;
	if (pInstance == NULL) {
		mutex_unlock(&(vMediaSyncInsList[index].m_lock));
		return -1;
	}

	*enable = pInstance->mFccEnable;
	mutex_unlock(&(vMediaSyncInsList[index].m_lock));

	return 0;
}


long mediasync_ins_set_source_type(s32 sSyncInsId, aml_Source_Type sourceType) {
	mediasync_ins* pInstance = NULL;
	s32 index = sSyncInsId;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	mutex_lock(&(vMediaSyncInsList[index].m_lock));
	pInstance = vMediaSyncInsList[index].pInstance;
	if (pInstance == NULL) {
		mutex_unlock(&(vMediaSyncInsList[index].m_lock));
		return -1;
	}

	pInstance->mSourceType = sourceType;
	mutex_unlock(&(vMediaSyncInsList[index].m_lock));

	return 0;
}

long mediasync_ins_get_source_type(s32 sSyncInsId, aml_Source_Type* sourceType) {
	mediasync_ins* pInstance = NULL;
	s32 index = sSyncInsId;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	mutex_lock(&(vMediaSyncInsList[index].m_lock));
	pInstance = vMediaSyncInsList[index].pInstance;
	if (pInstance == NULL) {
		mutex_unlock(&(vMediaSyncInsList[index].m_lock));
		return -1;
	}

	*sourceType = pInstance->mSourceType;
	mutex_unlock(&(vMediaSyncInsList[index].m_lock));

	return 0;
}

long mediasync_ins_set_start_media_time(s32 sSyncInsId, s64 startime) {
	mediasync_ins* pInstance = NULL;
	s32 index = sSyncInsId;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	mutex_lock(&(vMediaSyncInsList[index].m_lock));
	pInstance = vMediaSyncInsList[index].pInstance;
	if (pInstance == NULL) {
		mutex_unlock(&(vMediaSyncInsList[index].m_lock));
		return -1;
	}
	pInstance->mStartMediaTime = startime;
	mutex_unlock(&(vMediaSyncInsList[index].m_lock));

	return 0;
}

long mediasync_ins_get_start_media_time(s32 sSyncInsId, s64* starttime) {
	mediasync_ins* pInstance = NULL;
	s32 index = sSyncInsId;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	mutex_lock(&(vMediaSyncInsList[index].m_lock));
	pInstance = vMediaSyncInsList[index].pInstance;
	if (pInstance == NULL) {
		mutex_unlock(&(vMediaSyncInsList[index].m_lock));
		return -1;
	}
	*starttime = pInstance->mStartMediaTime;
	mutex_unlock(&(vMediaSyncInsList[index].m_lock));

	return 0;
}

long mediasync_ins_set_audioformat(s32 sSyncInsId, mediasync_audio_format format) {

	mediasync_ins* pInstance = NULL;
	s32 index = sSyncInsId;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
	return -1;

	mutex_lock(&(vMediaSyncInsList[index].m_lock));
	pInstance = vMediaSyncInsList[index].pInstance;
	if (pInstance == NULL) {
		mutex_unlock(&(vMediaSyncInsList[index].m_lock));
		return -1;
	}
	pInstance->mAudioFormat.channels = format.channels;
	pInstance->mAudioFormat.datawidth = format.datawidth;
	pInstance->mAudioFormat.format = format.format;
	pInstance->mAudioFormat.samplerate = format.samplerate;
	mutex_unlock(&(vMediaSyncInsList[index].m_lock));

	return 0;

}

long mediasync_ins_get_audioformat(s32 sSyncInsId, mediasync_audio_format* format) {
	mediasync_ins* pInstance = NULL;
	s32 index = sSyncInsId;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
	return -1;

	mutex_lock(&(vMediaSyncInsList[index].m_lock));
	pInstance = vMediaSyncInsList[index].pInstance;
	if (pInstance == NULL) {
		mutex_unlock(&(vMediaSyncInsList[index].m_lock));
		return -1;
	}

	format->channels = pInstance->mAudioFormat.channels;
	format->datawidth = pInstance->mAudioFormat.datawidth;
	format->format = pInstance->mAudioFormat.format;
	format->samplerate = pInstance->mAudioFormat.samplerate;
	mutex_unlock(&(vMediaSyncInsList[index].m_lock));

	return 0;
}

long mediasync_ins_set_pauseresume(s32 sSyncInsId, int flag) {
	mediasync_ins* pInstance = NULL;
	s32 index = sSyncInsId;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	mutex_lock(&(vMediaSyncInsList[index].m_lock));
	pInstance = vMediaSyncInsList[index].pInstance;
	if (pInstance == NULL) {
		mutex_unlock(&(vMediaSyncInsList[index].m_lock));
		return -1;
	}

	pInstance->mPauseResumeFlag = flag;
	mutex_unlock(&(vMediaSyncInsList[index].m_lock));

	return 0;
}

long mediasync_ins_get_pauseresume(s32 sSyncInsId, int* flag) {
	mediasync_ins* pInstance = NULL;
	s32 index = sSyncInsId;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	mutex_lock(&(vMediaSyncInsList[index].m_lock));
	pInstance = vMediaSyncInsList[index].pInstance;
	if (pInstance == NULL) {
		mutex_unlock(&(vMediaSyncInsList[index].m_lock));
		return -1;
	}

	*flag = pInstance->mPauseResumeFlag;
	mutex_unlock(&(vMediaSyncInsList[index].m_lock));

	return 0;
}

long mediasync_ins_set_pcrslope(s32 sSyncInsId, mediasync_speed pcrslope) {
	mediasync_ins* pInstance = NULL;
	s32 index = sSyncInsId;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	mutex_lock(&(vMediaSyncInsList[index].m_lock));
	pInstance = vMediaSyncInsList[index].pInstance;
	if (pInstance == NULL) {
		mutex_unlock(&(vMediaSyncInsList[index].m_lock));
		return -1;
	}

	pInstance->mPcrSlope.mNumerator = pcrslope.mNumerator;
	pInstance->mPcrSlope.mDenominator = pcrslope.mDenominator;
	mutex_unlock(&(vMediaSyncInsList[index].m_lock));

	return 0;

}

long mediasync_ins_get_pcrslope(s32 sSyncInsId, mediasync_speed *pcrslope){
	mediasync_ins* pInstance = NULL;
	s32 index = sSyncInsId;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	mutex_lock(&(vMediaSyncInsList[index].m_lock));
	pInstance = vMediaSyncInsList[index].pInstance;
	if (pInstance == NULL) {
		mutex_unlock(&(vMediaSyncInsList[index].m_lock));
		return -1;
	}

	pcrslope->mNumerator = pInstance->mPcrSlope.mNumerator;
	pcrslope->mDenominator = pInstance->mPcrSlope.mDenominator;
	mutex_unlock(&(vMediaSyncInsList[index].m_lock));

	return 0;
}

long mediasync_ins_update_avref(s32 sSyncInsId, int flag) {
	mediasync_ins* pInstance = NULL;
	s32 index = sSyncInsId;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	mutex_lock(&(vMediaSyncInsList[index].m_lock));
	pInstance = vMediaSyncInsList[index].pInstance;
	if (pInstance == NULL) {
		mutex_unlock(&(vMediaSyncInsList[index].m_lock));
		return -1;
	}

	if (flag)
		pInstance->mAVRef ++;
	else
		pInstance->mAVRef --;
	mutex_unlock(&(vMediaSyncInsList[index].m_lock));

	return 0;

}

long mediasync_ins_get_avref(s32 sSyncInsId, int *ref) {
	mediasync_ins* pInstance = NULL;
	s32 index = sSyncInsId;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	mutex_lock(&(vMediaSyncInsList[index].m_lock));
	pInstance = vMediaSyncInsList[index].pInstance;
	if (pInstance == NULL) {
		mutex_unlock(&(vMediaSyncInsList[index].m_lock));
		return -1;
	}

	*ref = pInstance->mAVRef;
	mutex_unlock(&(vMediaSyncInsList[index].m_lock));

	return 0;
}

long mediasync_ins_set_queueaudioinfo(s32 sSyncInsId, mediasync_frameinfo info) {
	mediasync_ins* pInstance = NULL;
	s32 index = sSyncInsId;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	mutex_lock(&(vMediaSyncInsList[index].m_lock));
	pInstance = vMediaSyncInsList[index].pInstance;
	if (pInstance == NULL) {
		mutex_unlock(&(vMediaSyncInsList[index].m_lock));
		return -1;
	}

	pInstance->mSyncInfo.queueAudioInfo.framePts = info.framePts;
	pInstance->mSyncInfo.queueAudioInfo.frameSystemTime = info.frameSystemTime;
	mutex_unlock(&(vMediaSyncInsList[index].m_lock));
	return 0;
}

long mediasync_ins_get_queueaudioinfo(s32 sSyncInsId, mediasync_frameinfo* info) {
	mediasync_ins* pInstance = NULL;
	s32 index = sSyncInsId;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	mutex_lock(&(vMediaSyncInsList[index].m_lock));
	pInstance = vMediaSyncInsList[index].pInstance;
	if (pInstance == NULL) {
		mutex_unlock(&(vMediaSyncInsList[index].m_lock));
		return -1;
	}

	info->framePts = pInstance->mSyncInfo.queueAudioInfo.framePts;
	info->frameSystemTime = pInstance->mSyncInfo.queueAudioInfo.frameSystemTime;
	mutex_unlock(&(vMediaSyncInsList[index].m_lock));

	return 0;
}

long mediasync_ins_set_queuevideoinfo(s32 sSyncInsId, mediasync_frameinfo info) {
	mediasync_ins* pInstance = NULL;
	s32 index = sSyncInsId;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	mutex_lock(&(vMediaSyncInsList[index].m_lock));
	pInstance = vMediaSyncInsList[index].pInstance;
	if (pInstance == NULL) {
		mutex_unlock(&(vMediaSyncInsList[index].m_lock));
		return -1;
	}

	pInstance->mSyncInfo.queueVideoInfo.framePts = info.framePts;
	pInstance->mSyncInfo.queueVideoInfo.frameSystemTime = info.frameSystemTime;
	mutex_unlock(&(vMediaSyncInsList[index].m_lock));
	return 0;

}

long mediasync_ins_get_queuevideoinfo(s32 sSyncInsId, mediasync_frameinfo* info) {
	mediasync_ins* pInstance = NULL;
	s32 index = sSyncInsId;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	mutex_lock(&(vMediaSyncInsList[index].m_lock));
	pInstance = vMediaSyncInsList[index].pInstance;
	if (pInstance == NULL) {
		mutex_unlock(&(vMediaSyncInsList[index].m_lock));
		return -1;
	}

	info->framePts = pInstance->mSyncInfo.queueVideoInfo.framePts;
	info->frameSystemTime = pInstance->mSyncInfo.queueVideoInfo.frameSystemTime;
	mutex_unlock(&(vMediaSyncInsList[index].m_lock));

	return 0;
}

long mediasync_ins_set_firstqueueaudioinfo(s32 sSyncInsId, mediasync_frameinfo info) {
	mediasync_ins* pInstance = NULL;
	s32 index = sSyncInsId;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	mutex_lock(&(vMediaSyncInsList[index].m_lock));
	pInstance = vMediaSyncInsList[index].pInstance;
	if (pInstance == NULL) {
		mutex_unlock(&(vMediaSyncInsList[index].m_lock));
		return -1;
	}

	pInstance->mSyncInfo.firstQAudioInfo.framePts = info.framePts;
	pInstance->mSyncInfo.firstQAudioInfo.frameSystemTime = info.frameSystemTime;
	mutex_unlock(&(vMediaSyncInsList[index].m_lock));
	return 0;
}

long mediasync_ins_get_firstqueueaudioinfo(s32 sSyncInsId, mediasync_frameinfo* info) {
	mediasync_ins* pInstance = NULL;
	s32 index = sSyncInsId;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	mutex_lock(&(vMediaSyncInsList[index].m_lock));
	pInstance = vMediaSyncInsList[index].pInstance;
	if (pInstance == NULL) {
		mutex_unlock(&(vMediaSyncInsList[index].m_lock));
		return -1;
	}

	info->framePts = pInstance->mSyncInfo.firstQAudioInfo.framePts;
	info->frameSystemTime = pInstance->mSyncInfo.firstQAudioInfo.frameSystemTime;
	mutex_unlock(&(vMediaSyncInsList[index].m_lock));

	return 0;
}

long mediasync_ins_set_firstqueuevideoinfo(s32 sSyncInsId, mediasync_frameinfo info) {
	mediasync_ins* pInstance = NULL;
	s32 index = sSyncInsId;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	mutex_lock(&(vMediaSyncInsList[index].m_lock));
	pInstance = vMediaSyncInsList[index].pInstance;
	if (pInstance == NULL) {
		mutex_unlock(&(vMediaSyncInsList[index].m_lock));
		return -1;
	}

	pInstance->mSyncInfo.firstQVideoInfo.framePts = info.framePts;
	pInstance->mSyncInfo.firstQVideoInfo.frameSystemTime = info.frameSystemTime;
	mutex_unlock(&(vMediaSyncInsList[index].m_lock));
	return 0;

}

long mediasync_ins_get_firstqueuevideoinfo(s32 sSyncInsId, mediasync_frameinfo* info) {
	mediasync_ins* pInstance = NULL;
	s32 index = sSyncInsId;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	mutex_lock(&(vMediaSyncInsList[index].m_lock));
	pInstance = vMediaSyncInsList[index].pInstance;
	if (pInstance == NULL) {
		mutex_unlock(&(vMediaSyncInsList[index].m_lock));
		return -1;
	}

	info->framePts = pInstance->mSyncInfo.firstQVideoInfo.framePts;
	info->frameSystemTime = pInstance->mSyncInfo.firstQVideoInfo.frameSystemTime;
	mutex_unlock(&(vMediaSyncInsList[index].m_lock));

	return 0;
}

