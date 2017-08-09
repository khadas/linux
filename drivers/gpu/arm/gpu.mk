#
# Copyright (C) 2015 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
ifneq (,$(wildcard device/amlogic/$(TARGET_PRODUCT)/BoardConfig.mk))
include device/amlogic/$(TARGET_PRODUCT)/BoardConfig.mk
else
ifneq (,$(wildcard device/customer/$(TARGET_PRODUCT)/BoardConfig.mk))
include device/customer/$(TARGET_PRODUCT)/BoardConfig.mk
else
ifneq (,$(wildcard $(TARGET_DEVICE_DIR)/BoardConfig.mk))
include $(TARGET_DEVICE_DIR)/BoardConfig.mk
else
$(error "find the BoardConfig file, then include it")
endif
endif
endif

LOCAL_KK=0
ifeq ($(GPU_TYPE), t82x)
	LOCAL_KK=1
endif

ifeq ($(GPU_TYPE), t83x)
	LOCAL_KK=1
endif

ifeq ($(LOCAL_KK),0)
GPU_DRV_VERSION?=r6p1
GPU_DRV_VERSION:=$(strip $(GPU_DRV_VERSION))
MALI_PLAT=hardware/arm/gpu/utgard/platform
MALI=hardware/arm/gpu/utgard/${GPU_DRV_VERSION}
MALI_OUT=$(PRODUCT_OUT)/obj/mali
KERNEL_ARCH ?= arm
#TODO rm shell cmd
define gpu-modules

$(MALI_KO):
	rm $(MALI_OUT) -rf
	mkdir -p $(MALI_OUT)
	cp $(MALI)/* $(MALI_OUT)/ -airf
	cp $(MALI_PLAT) $(MALI_OUT)/ -airf
	@echo "make mali module KERNEL_ARCH is $(KERNEL_ARCH)"
	@echo "make mali module MALI_OUT is $(MALI_OUT)"
	@echo "make mali module MAKE is $(MAKE)"
	@echo "GPU_DRV_VERSION is ${GPU_DRV_VERSION}"
	$(MAKE) -C $(shell pwd)/$(PRODUCT_OUT)/obj/KERNEL_OBJ M=$(shell pwd)/$(MALI_OUT)/ \
	ARCH=$(KERNEL_ARCH) CROSS_COMPILE=$(PREFIX_CROSS_COMPILE) CONFIG_MALI400=m  CONFIG_MALI450=m    \
	EXTRA_CFLAGS="-DCONFIG_MALI400=m -DCONFIG_MALI450=m" \
	CONFIG_GPU_THERMAL=y CONFIG_AM_VDEC_H264_4K2K=y modules

	mkdir -p $(PRODUCT_OUT)/system/lib
	cp $(MALI_OUT)/mali.ko $(PRODUCT_OUT)/system/lib/mali.ko
endef

else

GPU_DRV_VERSION?=r11p0
GPU_DRV_VERSION:=$(strip $(GPU_DRV_VERSION))
MALI=hardware/arm/gpu/midgard/${GPU_DRV_VERSION}
MALI_OUT=$(PRODUCT_OUT)/obj/t83x
KERNEL_ARCH ?= arm

define gpu-modules

$(MALI_KO):
	rm $(MALI_OUT) -rf
	mkdir -p $(MALI_OUT)
	cp $(MALI)/* $(MALI_OUT)/ -airf
	@echo "make mali module KERNEL_ARCH is $(KERNEL_ARCH) current dir is $(shell pwd)"
	@echo "MALI is $(MALI), MALI_OUT is $(MALI_OUT)"
	@echo "GPU_DRV_VERSION is ${GPU_DRV_VERSION}"
	$(MAKE) -C $(shell pwd)/$(PRODUCT_OUT)/obj/KERNEL_OBJ M=$(shell pwd)/$(MALI_OUT)/kernel/drivers/gpu/arm/midgard \
	ARCH=$(KERNEL_ARCH) CROSS_COMPILE=$(PREFIX_CROSS_COMPILE) \
	EXTRA_CFLAGS="-DCONFIG_MALI_PLATFORM_DEVICETREE -DCONFIG_MALI_MIDGARD_DVFS -DCONFIG_MALI_BACKEND=gpu" \
	CONFIG_MALI_MIDGARD=m CONFIG_MALI_PLATFORM_DEVICETREE=y CONFIG_MALI_MIDGARD_DVFS=y CONFIG_MALI_BACKEND=gpu modules

	mkdir -p $(PRODUCT_OUT)/system/lib
	cp $(MALI_OUT)/kernel/drivers/gpu/arm/midgard/mali_kbase.ko $(PRODUCT_OUT)/system/lib/mali.ko
	@echo "make mali module finished current dir is $(shell pwd)"
endef
endif
