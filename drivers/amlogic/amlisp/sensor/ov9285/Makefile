
obj-m += ov9285.o

M_PATH := $(shell dirname $(lastword $(MAKEFILE_LIST)))

DRV_SRC := $(wildcard $(M_PATH)/*.c)

M_DRV_SRC := $(DRV_SRC:$(M_PATH)/%=%)

DRV_SRC_OBJ := $(M_DRV_SRC:.c=.o)

ov9285-objs := $(DRV_SRC_OBJ)

LOCAL_INCLUDES := -I$(M_PATH)/../include -I$(M_PATH)/../../amlcam/cam_common

ccflags-y += $(LOCAL_INCLUDES)
