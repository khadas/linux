LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE		:= vcode_m2m
LOCAL_MODULE_TAGS	:= optional
LOCAL_SRC_FILES		:= vcodec_m2m_test.c
LOCAL_ARM_MODE		:= arm

LOCAL_C_INCLUDES := \
	$(JNI_H_INCLUDE) \
	$(BOARD_AML_VENDOR_PATH)/vendor/amlogic/external/ffmpeg

LOCAL_SHARED_LIBRARIES := \
	libamffmpeg

include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)

LOCAL_PREBUILT_LIBS:= \
#        libavcodec:ffmpeg/lib/libavcodec.so \

include $(BUILD_MULTI_PREBUILT)
