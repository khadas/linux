##############################################################################
#
#    Copyright (c) 2005 - 2019 by Vivante Corp.  All rights reserved.
#
#    The material in this file is confidential and contains trade secrets
#    of Vivante Corporation. This is proprietary information owned by
#    Vivante Corporation. No part of this work may be disclosed,
#    reproduced, copied, transmitted, or used in any way for any purpose,
#    without the express written permission of Vivante Corporation.
#
##############################################################################


LOCAL_PATH := $(call my-dir)
include $(LOCAL_PATH)/../../Android.mk.def

#
# galcore.ta
#
include $(CLEAR_VARS)
.PHONY: TABUILD

GALCORE := \
	$(LOCAL_PATH)/../../driver/galcore.ta

$(GALCORE): $(AQREG) TABUILD
	@cd $(AQROOT)/hal/security/
	@$(MAKE) -C $(AQROOT)/hal/security/ \
		AQROOT=$(abspath $(AQROOT)) \
		AQARCH=$(abspath $(AQARCH)) \
		DEBUG=$(DEBUG)

LOCAL_SRC_FILES := \
	../../driver/galcore.ta

LOCAL_GENERATED_SOURCES := \
	$(GALCORE)

LOCAL_MODULE       := galcore.ta
LOCAL_MODULE_TAGS  := optional
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_PATH  := $(TARGET_OUT_SHARED_LIBRARIES)/modules
include $(BUILD_PREBUILT)

