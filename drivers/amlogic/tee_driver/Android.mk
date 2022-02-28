LOCAL_PATH := $(call my-dir)

ifneq ($(TARGET_KERNEL_BUILT_FROM_SOURCE), false)

OPTEE_MODULES := $(shell pwd)/$(PRODUCT_OUT)/obj/optee_modules/
OPTEE_DRIVERS := $(shell pwd)/vendor/amlogic/common/tdk_linuxdriver/
KERNEL_OUT_DIR := $(shell pwd)/$(PRODUCT_OUT)/obj/KERNEL_OBJ/

##############################################################################
###
###  Build tee modules for Android. Since is in Android.mk, not standalone
###  module build script, all kernel related configurations(e.g:CROSS_COMPILE),
###  PLEASE PLEASE extends from the build system, DO NOT customization here!
##############################################################################
ifeq ($(shell test $(PLATFORM_SDK_VERSION) -ge 29 && echo OK),OK)
define build_optee_modules
	echo "$(1) $(2) $(3)"
	mkdir -p $(2)/
	cp -rfa $(1)/*  $(2)/
	PATH=$$(cd ./$(TARGET_HOST_TOOL_PATH); pwd):$$PATH \
	  $(MAKE) -C $(KERNEL_OUT_DIR) M=$(strip $(2))  \
	  KERNEL_A32_SUPPORT=$(KERNEL_A32_SUPPORT) ARCH=$(KERNEL_ARCH) \
	  CROSS_COMPILE=$(PREFIX_CROSS_COMPILE)
endef
else
define build_optee_modules
	echo "$(1) $(2) $(3)"
	mkdir -p $(2)/
	cp -rfa $(1)/*  $(2)/
	$(MAKE) -C $(KERNEL_OUT_DIR) M=$(strip $(2))  \
	  KERNEL_A32_SUPPORT=$(KERNEL_A32_SUPPORT) ARCH=$(KERNEL_ARCH) \
	  CROSS_COMPILE=$(PREFIX_CROSS_COMPILE)
endef
endif

$(PRODUCT_OUT)/obj/optee_modules/optee.ko: $(INSTALLED_KERNEL_TARGET)
	$(call build_optee_modules, $(OPTEE_DRIVERS), $(OPTEE_MODULES))

endif


include $(CLEAR_VARS)
LOCAL_MODULE := optee_armtz
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := SHARED_LIBRARIES

ifneq ($(TARGET_KERNEL_BUILT_FROM_SOURCE), false)
GEN_OPTEE_ARMTZ := $(local-generated-sources-dir)/optee_armtz.ko
$(GEN_OPTEE_ARMTZ): $(PRODUCT_OUT)/obj/optee_modules/optee.ko | $(ACP)
	cp $(PRODUCT_OUT)/obj/optee_modules/optee/optee_armtz.ko $(GEN_OPTEE_ARMTZ)

LOCAL_PREBUILT_MODULE_FILE := $(GEN_OPTEE_ARMTZ)
else
# TARGET_BOOTLOADER_BOARD_NAME currently defined the same as platform device name
LOCAL_SRC_FILES := device/amlogic/$(TARGET_BOOTLOADER_BOARD_NAME)-kernel/optee_armtz.ko
endif

LOCAL_MODULE_SUFFIX := .ko
LOCAL_MODULE_PATH := $(TARGET_OUT_VENDOR)/lib
LOCAL_STRIP_MODULE := false
include $(BUILD_PREBUILT)


include $(CLEAR_VARS)
LOCAL_MODULE := optee
LOCAL_MODULE_CLASS := SHARED_LIBRARIES

ifneq ($(TARGET_KERNEL_BUILT_FROM_SOURCE), false)
GEN_OPTEE := $(local-generated-sources-dir)/optee.ko
$(GEN_OPTEE): $(PRODUCT_OUT)/obj/optee_modules/optee.ko | $(ACP)
	cp $(PRODUCT_OUT)/obj/optee_modules/optee.ko $(GEN_OPTEE)

LOCAL_PREBUILT_MODULE_FILE := $(GEN_OPTEE)
else
LOCAL_SRC_FILES  :=  \
    device/amlogic/$(TARGET_BOOTLOADER_BOARD_NAME)-kernel/optee.ko

endif
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_SUFFIX := .ko
LOCAL_MODULE_PATH := $(TARGET_OUT_VENDOR)/lib
LOCAL_STRIP_MODULE := false
include $(BUILD_PREBUILT)
