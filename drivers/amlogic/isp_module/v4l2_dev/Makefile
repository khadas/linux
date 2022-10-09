#
# SPDX-License-Identifier: GPL-2.0
#
# Copyright (C) 2011-2018 ARM or its affiliates
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2.
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
# for more details.
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#

obj-m += iv009_isp.o

M_PATH := drivers/amlogic/isp_module/v4l2_dev

FW_SRC_OBJ := src/platform/system_am_md.o \
			  src/platform/system_semaphore.o \
			  src/platform/system_i2c.o \
			  src/platform/system_stdlib.o \
			  src/platform/system_am_sc1.o \
			  src/platform/system_autowrite.o \
			  src/platform/system_am_decmpr.o \
			  src/platform/system_am_dma.o \
			  src/platform/system_am_sc.o \
			  src/platform/system_interrupts.o \
			  src/platform/system_control.o \
			  src/platform/system_spi.o \
			  src/platform/system_hw_io.o \
			  src/platform/system_apb_dma.o \
			  src/platform/system_sw_io.o \
			  src/platform/system_chardev.o \
			  src/platform/system_am_sc2.o \
			  src/platform/system_profiler.o \
			  src/platform/system_spinlock.o \
			  src/platform/system_timer.o \
			  src/platform/system_hdmi.o \
			  src/platform/system_am_flicker.o \
			  src/platform/system_am_sc3.o \
			  src/platform/system_dma.o \
			  src/platform/system_log.o \
			  src/calibration/acamera_get_calibrations_v4l2.o \
			  src/calibration/soc_iq_calibrations.o \
			  src/fw_lib/matrix_yuv_intf.o \
			  src/fw_lib/matrix_yuv_func.o \
			  src/fw_lib/awb_manual_fsm.o \
			  src/fw_lib/ae_manual_fsm.o \
			  src/fw_lib/iridix8_manual_fsm.o \
			  src/fw_lib/af_manual_intf.o \
			  src/fw_lib/acamera_math.o \
			  src/fw_lib/autocapture_fsm.o \
			  src/fw_lib/acamera_calibrations.o \
			  src/fw_lib/fsm_param.o \
			  src/fw_lib/crop_intf.o \
			  src/fw_lib/sensor_func.o \
			  src/fw_lib/metadata_func.o \
			  src/fw_lib/monitor_func.o \
			  src/fw_lib/dma_writer_func.o \
			  src/fw_lib/metadata_intf.o \
			  src/fw_lib/autocapture_func.o \
			  src/fw_lib/acamera_ctrl_channel_k2u.o \
			  src/fw_lib/crop_func.o \
			  src/fw_lib/sbuf_fsm.o \
			  src/fw_lib/nr_fw.o \
			  src/fw_lib/dma_writer_fsm.o \
			  src/fw_lib/autocapture_intf.o \
			  src/fw_lib/sensor_fsm.o \
			  src/fw_lib/sbuf_intf.o \
			  src/fw_lib/sensor_init.o \
			  src/fw_lib/acamera_command_api.o \
			  src/fw_lib/noise_reduction_func.o \
			  src/fw_lib/fsm_util.o \
			  src/fw_lib/sbuf_func.o \
			  src/fw_lib/ae_manual_intf.o \
			  src/fw_lib/dma_writer_intf.o \
			  src/fw_lib/acamera_sbus_isp_sw.o \
			  src/fw_lib/awb_manual_func.o \
			  src/fw_lib/general_func.o \
			  src/fw_lib/general_fsm.o \
			  src/fw_lib/acamera_sbus_i2c.o \
			  src/fw_lib/metadata_fsm.o \
			  src/fw_lib/gamma_manual_func.o \
			  src/fw_lib/af_manual_fsm.o \
			  src/fw_lib/sharpening_fsm.o \
			  src/fw_lib/acamera_loop_buf.o \
			  src/fw_lib/monitor_fsm.o \
			  src/fw_lib/color_matrix_func.o \
			  src/fw_lib/cmos_func.o \
			  src/fw_lib/noise_reduction_fsm.o \
			  src/fw_lib/crop_fsm.o \
			  src/fw_lib/general_intf.o \
			  src/fw_lib/iridix8_manual_intf.o \
			  src/fw_lib/sharpening_intf.o \
			  src/fw_lib/ae_manual_func.o \
			  src/fw_lib/acamera.o \
			  src/fw_lib/matrix_yuv_fsm.o \
			  src/fw_lib/gamma_manual_fsm.o \
			  src/fw_lib/dma_writer.o \
			  src/fw_lib/acamera_logger.o \
			  src/fw_lib/acamera_event_queue.o \
			  src/fw_lib/acamera_sbus_spi.o \
			  src/fw_lib/acamera_sbus_isp.o \
			  src/fw_lib/gamma_manual_intf.o \
			  src/fw_lib/acamera_fsm_mgr.o \
			  src/fw_lib/acamera_command_api_impl.o \
			  src/fw_lib/sharpening_func.o \
			  src/fw_lib/iridix8_manual_func.o \
			  src/fw_lib/noise_reduction_intf.o \
			  src/fw_lib/color_matrix_fsm.o \
			  src/fw_lib/cmos_intf.o \
			  src/fw_lib/awb_manual_intf.o \
			  src/fw_lib/color_matrix_intf.o \
			  src/fw_lib/monitor_intf.o \
			  src/fw_lib/acamera_fw.o \
			  src/fw_lib/cmos_fsm.o \
			  src/fw_lib/sensor_intf.o \
			  src/fw_lib/af_manual_func.o \
			  src/fw_lib/acamera_sbus.o \
			  src/driver/sensor/V4L2_drv.o \
			  src/driver/sensor/ACameraDefault.o \
			  src/driver/sensor/sensor_bus_config.o \
			  src/driver/lens/v4l2_vcm.o \
			  src/driver/lens/lens_init.o \
			  app/main_kernel_juno_v4l2.o \
			  app/main_firmware.o \
			  app/control/acamera_connection.o \
			  app/control/acamera_chardev.o \
			  app/control/application_command_api.o \
			  app/v4l2_interface/isp-v4l2-stream.o \
			  app/v4l2_interface/isp-v4l2.o \
			  app/v4l2_interface/isp-v4l2-ctrl.o \
			  app/v4l2_interface/isp-vb2-cmalloc.o \
			  app/v4l2_interface/fw-interface.o \
			  app/v4l2_interface/isp-vb2.o

iv009_isp-objs += $(FW_SRC_OBJ)

LOCAL_INCLUDES := -I$(M_PATH)/app -I$(M_PATH)/inc -I$(M_PATH)/app/control \
		-I$(M_PATH)/inc/api -I$(M_PATH)/inc/isp -I$(M_PATH)/inc/sys \
		-I$(M_PATH)/src/platform -I$(M_PATH)/src/fw \
		-I$(M_PATH)/src/fw_lib -I$(M_PATH)/src/calibration \
		-I$(M_PATH)/src/driver/sensor -I$(M_PATH)/src/driver/lens

ifeq ($(SYSTEM_VERSION),)
    ccflags-y += -DCONFIG_ANDROID_OS
else
    ccflags-y += -DCONFIG_$(SYSTEM_VERSION)
endif

ifeq ($(PLATFORM_VERSION),)
    ccflags-y += -DCONFIG_T7
else
    ccflags-y += -DCONFIG_$(PLATFORM_VERSION)
endif

ifeq ($(SEAMLESS_SUPPORT),)
    ccflags-y += -DCONFIG_SEAMLESS_N
else
    ccflags-y += -DCONFIG_SEAMLESS_$(SEAMLESS_SUPPORT)
endif

ccflags-y += $(LOCAL_INCLUDES)
ccflags-y += -Wno-declaration-after-statement -Idrivers/staging/android/ \
		-Wno-error
EXTRA_CFLAGS += $(LOCAL_INCLUDES)

all:
	@$(MAKE) -C $(KERNEL_SRC) M=$(M)  modules
modules_install:
	@echo "$(MAKE) INSTALL_MOD_STRIP=1 M=$(M) -C $(KERNEL_SRC) modules_install"
	@$(MAKE) INSTALL_MOD_STRIP=1 M=$(M) -C $(KERNEL_SRC) modules_install
	mkdir -p ${OUT_DIR}/../vendor_lib
	cp $(OUT_DIR)/$(M)/*.ko ${OUT_DIR}/../vendor_lib/modules/

clean:
	@$(MAKE) -C $(KERNEL_SRC) M=$(M) clean
