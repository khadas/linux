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
#include <linux/vmalloc.h>

#include "pts_server_core.h"

#define pts_pr_info(number,fmt,args...) pr_info("multi_ptsserv:[%d] " fmt, number,##args)


static u32 ptsserver_debuglevel = 0;

#define MAX_INSTANCE_NUM 10
PtsServerManage vPtsServerInsList[MAX_INSTANCE_NUM];

long ptsserver_ins_init_syncinfo(ptsserver_ins* pInstance,ptsserver_alloc_para* allocParm) {

	if (pInstance == NULL) {
		return -1;
	}

	if (allocParm != NULL) {
		pInstance->mMaxCount = allocParm->mMaxCount; //500
		pInstance->mLookupThreshold = allocParm->mLookupThreshold; //2500
		pInstance->kDoubleCheckThredhold = allocParm->kDoubleCheckThredhold; //5
	} else {
		pInstance->mMaxCount = 500;
		pInstance->mLookupThreshold = 2500;
		pInstance->kDoubleCheckThredhold = 5;
	}

	pInstance->mPtsCheckinStarted = 0;
	pInstance->mPtsCheckoutStarted = 0;
	pInstance->mPtsCheckoutFailCount = 0;
	pInstance->mFrameDuration = 0;
	pInstance->mFrameDuration64 = 0;
	pInstance->mFirstCheckinPts = 0;
	pInstance->mFirstCheckinOffset = 0;
	pInstance->mFirstCheckinSize = 0;
	pInstance->mLastCheckinPts = 0;
	pInstance->mLastCheckinOffset = 0;
	pInstance->mLastCheckinSize = 0;
	pInstance->mAlignmentOffset = 0;
	pInstance->mLastCheckoutPts = 0;
	pInstance->mLastCheckoutOffset = 0;
	pInstance->mFirstCheckinPts64 = 0;
	pInstance->mLastCheckinPts64 = 0;
	pInstance->mLastCheckoutPts64 = 0;
	pInstance->mDoubleCheckFrameDuration = 0;
	pInstance->mDoubleCheckFrameDuration64 = 0;
	pInstance->mDoubleCheckFrameDurationCount = 0;
	pInstance->mLastDoubleCheckoutPts = 0;
	pInstance->mLastDoubleCheckoutPts64 = 0;
	pInstance->mDecoderDuration = 0;
	pInstance->mListSize = 0;
	pInstance->mLastCheckoutCurOffset = 0;
	mutex_init(&pInstance->mPtsListLock);
	INIT_LIST_HEAD(&pInstance->pts_list);
	return 0;
}


long ptsserver_init(void) {
	int i = 0;
	for (i = 0;i < MAX_INSTANCE_NUM;i++) {
		vPtsServerInsList[i].pInstance = NULL;
		mutex_init(&(vPtsServerInsList[i].mListLock));
	}
	return 0;
}


long ptsserver_ins_alloc(s32 *pServerInsId,
							ptsserver_ins **pIns,
							ptsserver_alloc_para* allocParm) {

	s32 index = 0;
	ptsserver_ins* pInstance = NULL;
	pInstance = kzalloc(sizeof(ptsserver_ins), GFP_KERNEL);
	if (pInstance == NULL) {
		return -1;
	}
	pr_info("multi_ptsserv: ptsserver_ins_alloc in\n");
	for (index = 0; index < MAX_INSTANCE_NUM - 1; index++) {
		mutex_lock(&vPtsServerInsList[index].mListLock);
		if (vPtsServerInsList[index].pInstance == NULL) {
			vPtsServerInsList[index].pInstance = pInstance;
			pInstance->mPtsServerInsId = index;
			*pServerInsId = index;
			ptsserver_ins_init_syncinfo(pInstance,allocParm);
			pts_pr_info(index,"ptsserver_ins_alloc --> index:%d\n", index);
			mutex_unlock(&vPtsServerInsList[index].mListLock);
			break;
		}
		mutex_unlock(&vPtsServerInsList[index].mListLock);
	}

	if (index == MAX_INSTANCE_NUM) {
		kzfree(pInstance);
		return -1;
	}

	*pIns = pInstance;
	pts_pr_info(index,"ptsserver_ins_alloc ok\n");
	return 0;
}


long ptsserver_set_first_checkin_offset(s32 pServerInsId,start_offset* mStartOffset) {
	ptsserver_ins* pInstance = NULL;
	PtsServerManage* vPtsServerIns = NULL;
	s32 index = pServerInsId;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;
	vPtsServerIns = &(vPtsServerInsList[index]);

	mutex_lock(&vPtsServerIns->mListLock);
	pInstance = vPtsServerIns->pInstance;
	if (pInstance == NULL) {
		mutex_unlock(&vPtsServerIns->mListLock);
		return -1;
	}
	pts_pr_info(index,"mBaseOffset:%d mAlignmentOffset:%d\n",
						mStartOffset->mBaseOffset,
						mStartOffset->mAlignmentOffset);
	pInstance->mLastCheckinOffset = mStartOffset->mBaseOffset;
	pInstance->mAlignmentOffset = mStartOffset->mAlignmentOffset;
	mutex_unlock(&vPtsServerIns->mListLock);
	return 0;
}


long ptsserver_checkin_pts_size(s32 pServerInsId,checkin_pts_size* mCheckinPtsSize) {
	PtsServerManage* vPtsServerIns = NULL;
	ptsserver_ins* pInstance = NULL;
	pts_node* ptn = NULL;
	pts_node* del_ptn = NULL;
	s32 index = pServerInsId;
	s32 level = 0;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	vPtsServerIns = &(vPtsServerInsList[index]);
	mutex_lock(&vPtsServerIns->mListLock);
	pInstance = vPtsServerIns->pInstance;
	if (pInstance == NULL) {
		mutex_unlock(&vPtsServerIns->mListLock);
		return -1;
	}
	ptn = vmalloc(sizeof(struct ptsnode));
	ptn->offset = pInstance->mLastCheckinOffset + pInstance->mLastCheckinSize;
	ptn->pts = mCheckinPtsSize->pts;
	ptn->pts_64 = mCheckinPtsSize->pts_64;
	if (pInstance->mListSize >= pInstance->mMaxCount) {
		del_ptn = list_first_entry(&pInstance->pts_list, struct ptsnode, node);
		list_del(&del_ptn->node);
		if (ptsserver_debuglevel >= 1) {
			pts_pr_info(index,"Checkin delete node size:%d pts:0x%x pts_64:%lld\n",
								del_ptn->offset,
								del_ptn->pts,
								del_ptn->pts_64);
		}
		vfree(del_ptn);
		pInstance->mListSize--;
	}


	list_add_tail(&ptn->node, &pInstance ->pts_list);

	pInstance->mListSize++;
	if (!pInstance->mPtsCheckinStarted) {
		pts_pr_info(index,"-->Checkin(First) ListSize:%d CheckinPtsSize(%d)-> offset:0x%x pts(32:0x%x 64:%lld)\n",
							pInstance->mListSize,
							mCheckinPtsSize->size,
							ptn->offset,ptn->pts,
							ptn->pts_64);
		pInstance->mFirstCheckinOffset = 0;
		pInstance->mFirstCheckinSize = mCheckinPtsSize->size;
		pInstance->mFirstCheckinPts = mCheckinPtsSize->pts;
		pInstance->mFirstCheckinPts64 = mCheckinPtsSize->pts_64;
		pInstance->mPtsCheckinStarted = 1;
	}

	if (ptsserver_debuglevel >= 1) {
		level = ptn->offset - pInstance->mLastCheckoutCurOffset;
		pts_pr_info(index,"Checkin ListCount:%d LastCheckinOffset:0x%x LastCheckinSize:0x%x\n",
								pInstance->mListSize,
								pInstance->mLastCheckinOffset,
								pInstance->mLastCheckinSize);
		pts_pr_info(index,"Checkin Size(%d) %s:0x%x (%s:%d) pts(32:0x%x 64:%lld)\n",
							mCheckinPtsSize->size,
							ptn->offset < pInstance->mLastCheckinOffset?"rest offset":"offset",
							ptn->offset,
							level >= 5242880 ? ">5M level": "level",
							level,
							ptn->pts,
							ptn->pts_64);
	}

	if (pInstance->mAlignmentOffset != 0) {
		pInstance->mLastCheckinOffset = pInstance->mAlignmentOffset;
		pInstance->mAlignmentOffset = 0;
	} else {
		pInstance->mLastCheckinOffset = ptn->offset;
	}

	pInstance->mLastCheckinSize = mCheckinPtsSize->size;
	pInstance->mLastCheckinPts = mCheckinPtsSize->pts;
	pInstance->mLastCheckinPts64 = mCheckinPtsSize->pts_64;

	mutex_unlock(&vPtsServerIns->mListLock);
	return 0;
}

long ptsserver_checkout_pts_offset(s32 pServerInsId,checkout_pts_offset* mCheckoutPtsOffset) {
	PtsServerManage* vPtsServerIns = NULL;
	ptsserver_ins* pInstance = NULL;
	s32 index = pServerInsId;
	pts_node* ptn = NULL;
	pts_node* find_ptn = NULL;
	pts_node* ptn_tmp = NULL;

	u32 cur_offset = 0xFFFFFFFF & mCheckoutPtsOffset->offset;
	s32 cur_duration = (mCheckoutPtsOffset->offset >>= 32) & 0xFFFFFFFF;

	u32 offsetDiff = 2500;
	s32 find_framenum = 0;
	s32 find = 0;
	s32 offsetAbs = 0;
	u32 FrameDur = 0;
	u64 FrameDur64 = 0;
	s32 number = -1;
	s32 i = 0;
	if (index < 0 || index >= MAX_INSTANCE_NUM) {
		return -1;
	}
	vPtsServerIns = &(vPtsServerInsList[index]);
	mutex_lock(&vPtsServerIns->mListLock);
	pInstance = vPtsServerIns->pInstance;

	if (pInstance == NULL) {
		mutex_unlock(&vPtsServerIns->mListLock);
		return -1;
	}


	offsetDiff = pInstance->mLookupThreshold;

	if (!pInstance->mPtsCheckoutStarted) {
		pts_pr_info(index,"Checkout(First) ListSize:%d offset:0x%x duration:%d\n",
							pInstance->mListSize,
							cur_offset,
							cur_duration);
	}

	if (ptsserver_debuglevel >= 1) {
		pts_pr_info(index,"checkout ListCount:%d offset:%lld cur_offset:0x%x cur_duration:%d\n",
							pInstance->mListSize,
							mCheckoutPtsOffset->offset,
							cur_offset,
							cur_duration);
	}
/*
	if (list_empty(&pInstance->pts_list)) {
		pts_pr_info(index,"checkout list_empty \n");
		mutex_unlock(&vPtsServerIns->mListLock);
		return -1;
	}
*/
	if (cur_offset != 0xFFFFFFFF &&
		!list_empty(&pInstance->pts_list)) {
		find_framenum = 0;
		find = 0;
		list_for_each_entry_safe(ptn,ptn_tmp,&pInstance->pts_list, node) {

			if (ptn != NULL) {
				offsetAbs = abs(cur_offset - ptn->offset);
				if (ptsserver_debuglevel > 1) {
					pts_pr_info(index,"Checkout i:%d offset(diff:%d L:0x%x C:0x%x)\n",i,offsetAbs,ptn->offset,cur_offset);
				}
				if (offsetAbs <=  pInstance->mLookupThreshold) {
					if (offsetAbs <= offsetDiff && cur_offset > ptn->offset) {
						offsetDiff = offsetAbs;
						find = 1;
						number = i;
						find_ptn = ptn;
					}
				} else if (/*cur_offset > ptn->offset && */offsetAbs > 251658240) {
					// > 240M (15M*16)
					if (ptsserver_debuglevel >= 1) {
						pts_pr_info(index,"Checkout delete(%d) diff:%d offset:0x%x pts:0x%x pts_64:%lld\n",
											i,
											offsetAbs,
											ptn->offset,
											ptn->pts,
											ptn->pts_64);
					}
					list_del(&ptn->node);
					pInstance->mListSize--;
					vfree(ptn);

				} else if (find_framenum > 5) {
					break;
				}
				if (find) {
					find_framenum++;
				}
			}
			i++;
		}
		if (find) {
			mCheckoutPtsOffset->pts = find_ptn->pts;
			mCheckoutPtsOffset->pts_64 = find_ptn->pts_64;
			list_del(&find_ptn->node);
			if (ptsserver_debuglevel >= 1 ||
				!pInstance->mPtsCheckoutStarted) {
				pts_pr_info(index,"Checkout ok ListCount:%d find:%d offset(diff:%d L:0x%x) pts(32:0x%x 64:%lld)\n",
									pInstance->mListSize,number,offsetDiff,
									find_ptn->offset,find_ptn->pts,find_ptn->pts_64);
			}
			vfree(find_ptn);
			pInstance->mListSize--;
		}
	}
	if (!find) {
		pInstance->mPtsCheckoutFailCount++;
		if (ptsserver_debuglevel >= 1 || pInstance->mPtsCheckoutFailCount % 30 == 0) {
			pts_pr_info(index,"Checkout fail mPtsCheckoutFailCount:%d level:%d \n",
				pInstance->mPtsCheckoutFailCount,pInstance->mLastCheckinOffset - cur_offset);
		}
		if (pInstance->mDecoderDuration != 0) {
			pInstance->mLastCheckoutPts = pInstance->mLastCheckoutPts + pInstance->mDecoderDuration;
			pInstance->mLastCheckoutPts64 = pInstance->mLastCheckoutPts64 + pInstance->mDecoderDuration * 1000 / 96;
			if (ptsserver_debuglevel >= 1) {
				pts_pr_info(index,"Checkout fail mDecoderDuration:%d pts(32:%d 64:%lld)\n",
									pInstance->mDecoderDuration,
									pInstance->mLastCheckoutPts,
									pInstance->mLastCheckoutPts64);
			}
		} else {
			if (pInstance->mFrameDuration != 0) {
				pInstance->mLastCheckoutPts = pInstance->mLastCheckoutPts + pInstance->mFrameDuration;
			}
			if (pInstance->mFrameDuration64 != 0) {
				pInstance->mLastCheckoutPts64 = pInstance->mLastCheckoutPts64 + pInstance->mFrameDuration64;
			}
			if (ptsserver_debuglevel >= 1) {
				pts_pr_info(index,"Checkout fail FrameDuration(32:%d 64:%lld) pts(32:%d 64:%lld)\n",
									pInstance->mFrameDuration,
									pInstance->mFrameDuration64,
									pInstance->mLastCheckoutPts,
									pInstance->mLastCheckoutPts64);
			}

		}
		mCheckoutPtsOffset->pts = pInstance->mLastCheckoutPts;
		mCheckoutPtsOffset->pts_64 = pInstance->mLastCheckoutPts64;
		pInstance->mLastCheckoutCurOffset = cur_offset;
		mutex_unlock(&vPtsServerIns->mListLock);
		return 0;
	}

	if (pInstance->mPtsCheckoutStarted) {
		if (pInstance->mFrameDuration == 0 && pInstance->mFrameDuration64 == 0) {

			pInstance->mFrameDuration = div_u64(mCheckoutPtsOffset->pts - pInstance->mLastCheckoutPts,
													pInstance->mPtsCheckoutFailCount + 1);

			pInstance->mFrameDuration64 = div_u64(mCheckoutPtsOffset->pts_64 - pInstance->mLastCheckoutPts64,
													pInstance->mPtsCheckoutFailCount + 1);

			if (pInstance->mFrameDuration < 0 ||
				pInstance->mFrameDuration > 9000) {
				pInstance->mFrameDuration = 0;
			}
			if (pInstance->mFrameDuration64 < 0 ||
				pInstance->mFrameDuration64 > 100000) {
				pInstance->mFrameDuration64 = 0;
			}
		} else {

			FrameDur = div_u64(mCheckoutPtsOffset->pts - pInstance->mLastDoubleCheckoutPts,
								pInstance->mPtsCheckoutFailCount + 1);

			FrameDur64 = div_u64(mCheckoutPtsOffset->pts_64 - pInstance->mLastDoubleCheckoutPts64,
									pInstance->mPtsCheckoutFailCount + 1);

			if (ptsserver_debuglevel > 1) {
				pts_pr_info(index,"checkout FrameDur(32:%d 64:%lld) DoubleCheckFrameDuration(32:%d 64:%lld)\n",
									FrameDur,
									FrameDur64,
									pInstance->mDoubleCheckFrameDuration,
									pInstance->mDoubleCheckFrameDuration64);
				pts_pr_info(index,"checkout LastDoubleCheckoutPts(32:%d 64:%lld) pts(32:%d 64:%lld) PtsCheckoutFailCount:%d\n",
									pInstance->mLastDoubleCheckoutPts,
									pInstance->mLastDoubleCheckoutPts64,
									mCheckoutPtsOffset->pts,
									mCheckoutPtsOffset->pts_64,
									pInstance->mPtsCheckoutFailCount);
			}


			if ((FrameDur == pInstance->mDoubleCheckFrameDuration) ||
				(FrameDur64 == pInstance->mDoubleCheckFrameDuration64)) {
				pInstance->mDoubleCheckFrameDurationCount ++;
			} else {
				pInstance->mDoubleCheckFrameDuration = FrameDur;
				pInstance->mDoubleCheckFrameDuration64 = FrameDur64;
				pInstance->mDoubleCheckFrameDurationCount = 0;
			}
			if (pInstance->mDoubleCheckFrameDurationCount > pInstance->kDoubleCheckThredhold) {
				if (ptsserver_debuglevel > 1) {
					pts_pr_info(index,"checkout DoubleCheckFrameDurationCount(%d) DoubleCheckFrameDuration(32:%d 64:%lld)\n",
										pInstance->mDoubleCheckFrameDurationCount,
										pInstance->mDoubleCheckFrameDuration,
										pInstance->mDoubleCheckFrameDuration64);
				}
				pInstance->mFrameDuration = pInstance->mDoubleCheckFrameDuration;
				pInstance->mFrameDuration64 = pInstance->mDoubleCheckFrameDuration64;
				pInstance->mDoubleCheckFrameDurationCount = 0;
			}
		}
	} else {
		pInstance->mPtsCheckoutStarted = 1;
	}

	pInstance->mPtsCheckoutFailCount = 0;
	pInstance->mLastCheckoutOffset = mCheckoutPtsOffset->offset;
	pInstance->mLastCheckoutPts = mCheckoutPtsOffset->pts;
	pInstance->mLastCheckoutPts64 = mCheckoutPtsOffset->pts_64;
	pInstance->mLastDoubleCheckoutPts = mCheckoutPtsOffset->pts;
	pInstance->mLastDoubleCheckoutPts64 = mCheckoutPtsOffset->pts_64;
	pInstance->mDecoderDuration = cur_duration;
	pInstance->mLastCheckoutCurOffset = cur_offset;
	mutex_unlock(&vPtsServerIns->mListLock);
	return 0;
}

long ptsserver_peek_pts_offset(s32 pServerInsId,checkout_pts_offset* mCheckoutPtsOffset) {
	PtsServerManage* vPtsServerIns = NULL;
	ptsserver_ins* pInstance = NULL;
	s32 index = pServerInsId;
	pts_node* ptn = NULL;
	pts_node* find_ptn = NULL;
	u32 cur_offset = 0xFFFFFFFF & mCheckoutPtsOffset->offset;
	//s32 cur_duration = (mCheckoutPtsOffset->offset >>= 32) & 0xFFFFFFFF;

	u32 offsetDiff = 2500;
	s32 find_framenum = 0;
	s32 find = 0;
	s32 offsetTmp = 0;

	s32 number = -1;
	s32 i = 0;
	u32 mCalculateLastCheckoutPts = 0;
	u64 mCalculateLastCheckoutPts64 = 0;
	//pts_pr_info(pServerInsId,"ptsserver_peek_pts_offset in \n");
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	vPtsServerIns = &(vPtsServerInsList[index]);
	mutex_lock(&vPtsServerIns->mListLock);
	pInstance = vPtsServerIns->pInstance;
	if (pInstance == NULL) {
		pts_pr_info(pServerInsId,"ptsserver_peek_pts_offset pInstance == NULL ok \n");
		mutex_unlock(&vPtsServerIns->mListLock);
		return -1;
	}
/*
	if (list_empty(&pInstance->pts_list)) {
		pts_pr_info(index,"checkout list_empty \n");
		mutex_unlock(&vPtsServerIns->mListLock);
		return -1;
	}
*/
	if (cur_offset != 0xFFFFFFFF &&
		!list_empty(&pInstance->pts_list)) {
		find_framenum = 0;
		find = 0;
		list_for_each_entry(ptn, &pInstance->pts_list, node) {
			if (ptn != NULL) {
				offsetTmp = abs(ptn->offset - cur_offset);
				if (offsetTmp <=  pInstance->mLookupThreshold) {
					if (offsetTmp <= offsetDiff && cur_offset > ptn->offset) {
						offsetDiff = offsetTmp;
						find = 1;
						number = i;
						find_ptn = ptn;
					}
				} else if (find_framenum > 5) {
					break;
				}
				if (find) {
					find_framenum++;
				}
			}
			i++;
		}
		if (find) {
			mCheckoutPtsOffset->pts = find_ptn->pts;
			mCheckoutPtsOffset->pts_64 = find_ptn->pts_64;

			if (ptsserver_debuglevel >= 1) {
				pts_pr_info(index,"peek ok ListSize:%d find:%d offset(diff:%d L:0x%x C:0x%x) pts(32:0x%x 64:%lld)\n",
									pInstance->mListSize,number,offsetDiff,
									find_ptn->offset ,cur_offset,find_ptn->pts, find_ptn->pts_64);
			}
		}
	}

	if (!find) {
		if (pInstance->mDecoderDuration != 0) {
			mCalculateLastCheckoutPts = pInstance->mLastCheckoutPts + pInstance->mDecoderDuration;
			mCalculateLastCheckoutPts64 = pInstance->mLastCheckoutPts64 + div_u64(pInstance->mDecoderDuration * 1000,96);
		} else {
			if (pInstance->mFrameDuration != 0) {
				mCalculateLastCheckoutPts = pInstance->mLastCheckoutPts + pInstance->mFrameDuration;
			}
			if (pInstance->mFrameDuration64 != 0) {
				mCalculateLastCheckoutPts64 = pInstance->mLastCheckoutPts64 + pInstance->mFrameDuration64;
			}
		}

		mCheckoutPtsOffset->pts = mCalculateLastCheckoutPts;
		mCheckoutPtsOffset->pts_64 = mCalculateLastCheckoutPts64;
	}


	mutex_unlock(&vPtsServerIns->mListLock);

	return 0;
}
EXPORT_SYMBOL(ptsserver_peek_pts_offset);

long ptsserver_get_last_checkin_pts(s32 pServerInsId,last_checkin_pts* mLastCheckinPts) {
	PtsServerManage* vPtsServerIns = NULL;
	ptsserver_ins* pInstance = NULL;
	s32 index = pServerInsId;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	vPtsServerIns = &(vPtsServerInsList[index]);
	mutex_lock(&vPtsServerIns->mListLock);
	pInstance = vPtsServerIns->pInstance;
	if (pInstance == NULL) {
		mutex_unlock(&vPtsServerIns->mListLock);
		return -1;
	}
	mLastCheckinPts->mLastCheckinPts = pInstance->mLastCheckinPts;
	mLastCheckinPts->mLastCheckinPts64 = pInstance->mLastCheckinPts64;
	mutex_unlock(&vPtsServerIns->mListLock);

	return 0;
}

long ptsserver_get_last_checkout_pts(s32 pServerInsId,last_checkout_pts* mLastCheckOutPts) {

	PtsServerManage* vPtsServerIns = NULL;
	ptsserver_ins* pInstance = NULL;
	s32 index = pServerInsId;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	vPtsServerIns = &(vPtsServerInsList[index]);
	mutex_lock(&vPtsServerIns->mListLock);
	pInstance = vPtsServerIns->pInstance;
	if (pInstance == NULL) {
		mutex_unlock(&vPtsServerIns->mListLock);
		return -1;
	}
	mLastCheckOutPts->mLastCheckoutPts = pInstance->mLastCheckoutPts;
	mLastCheckOutPts->mLastCheckoutPts64 = pInstance->mLastCheckoutPts64;
	mutex_unlock(&vPtsServerIns->mListLock);
	return 0;
}

long ptsserver_ins_release(s32 pServerInsId) {

	PtsServerManage* vPtsServerIns = NULL;
	ptsserver_ins* pInstance = NULL;
	s32 index = pServerInsId;
	pts_node *ptn = NULL;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	pts_pr_info(pServerInsId,"ptsserver_ins_release in \n");
	vPtsServerIns = &(vPtsServerInsList[index]);
	mutex_lock(&vPtsServerIns->mListLock);
	pInstance = vPtsServerIns->pInstance;
	if (pInstance == NULL) {
		mutex_unlock(&vPtsServerIns->mListLock);
		return -1;
	}

	pts_pr_info(index,"ptsserver_ins_release ListSize:%d\n",pInstance->mListSize);
	while (!list_empty(&pInstance->pts_list)) {
		ptn = list_entry(pInstance->pts_list.next,
						struct ptsnode, node);
		if (ptn != NULL) {
			list_del(&ptn->node);
			vfree(ptn);
			ptn = NULL;
		}
	}

	kzfree(pInstance);
	pInstance = NULL;
	vPtsServerInsList[index].pInstance = NULL;
	mutex_unlock(&vPtsServerIns->mListLock);
	pts_pr_info(pServerInsId,"ptsserver_ins_release ok \n");
	return 0;
}


module_param(ptsserver_debuglevel, uint, 0664);
MODULE_PARM_DESC(ptsserver_debuglevel, "\n pts server debug level\n");

