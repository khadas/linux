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

DIR = drivers/amlogic/isp_module/v4l2_dev

obj-m += iv009_isp.o


FW_SRC_OBJ := app/main_kernel_juno_v4l2.o \
			  app/v4l2_interface/isp-vb2-cmalloc.o \
			  app/v4l2_interface/isp-vb2.o \
			  app/v4l2_interface/fw-interface.o \
			  app/v4l2_interface/isp-v4l2-stream.o \
			  app/v4l2_interface/isp-v4l2.o \
			  app/v4l2_interface/isp-v4l2-ctrl.o \
			  app/main_firmware.o \
			  app/control/acamera_connection.o \
			  app/control/acamera_chardev.o \
			  app/control/application_command_api.o \
			  src/platform/system_stdlib.o \
			  src/platform/system_hdmi.o \
			  src/platform/system_timer.o \
			  src/platform/system_log.o \
			  src/platform/system_spinlock.o \
			  src/platform/system_semaphore.o \
			  src/platform/system_interrupts.o \
			  src/platform/system_am_sc.o \
			  src/platform/system_hw_io.o \
			  src/platform/system_spi.o \
			  src/platform/system_chardev.o \
			  src/platform/system_dma.o \
			  src/platform/system_profiler.o \
			  src/platform/system_control.o \
			  src/platform/system_i2c.o \
			  src/platform/system_sw_io.o \
			  src/fw_lib/metadata_func.o \
			  src/fw_lib/general_func.o \
			  src/fw_lib/sensor_fsm.o \
			  src/fw_lib/monitor_fsm.o \
			  src/fw_lib/fsm_param.o \
			  src/fw_lib/acamera.o \
			  src/fw_lib/af_manual_fsm.o \
			  src/fw_lib/cmos_intf.o \
			  src/fw_lib/matrix_yuv_fsm.o \
			  src/fw_lib/general_intf.o \
			  src/fw_lib/dma_writer.o \
			  src/fw_lib/iridix8_manual_func.o \
			  src/fw_lib/metadata_fsm.o \
			  src/fw_lib/gamma_manual_fsm.o \
			  src/fw_lib/acamera_sbus_isp.o \
			  src/fw_lib/sensor_func.o \
			  src/fw_lib/acamera_sbus.o \
			  src/fw_lib/ae_manual_func.o \
			  src/fw_lib/sensor_intf.o \
			  src/fw_lib/ae_manual_intf.o \
			  src/fw_lib/monitor_func.o \
			  src/fw_lib/acamera_command_api.o \
			  src/fw_lib/acamera_calibrations.o \
			  src/fw_lib/dma_writer_func.o \
			  src/fw_lib/fsm_util.o \
			  src/fw_lib/general_fsm.o \
			  src/fw_lib/acamera_sbus_isp_sw.o \
			  src/fw_lib/iridix8_manual_fsm.o \
			  src/fw_lib/acamera_sbus_i2c.o \
			  src/fw_lib/color_matrix_func.o \
			  src/fw_lib/af_manual_intf.o \
			  src/fw_lib/awb_manual_fsm.o \
			  src/fw_lib/noise_reduction_fsm.o \
			  src/fw_lib/acamera_math.o \
			  src/fw_lib/matrix_yuv_func.o \
			  src/fw_lib/acamera_logger.o \
			  src/fw_lib/sensor_init.o \
			  src/fw_lib/gamma_manual_intf.o \
			  src/fw_lib/sharpening_intf.o \
			  src/fw_lib/awb_manual_intf.o \
			  src/fw_lib/acamera_command_api_impl.o \
			  src/fw_lib/cmos_func.o \
			  src/fw_lib/acamera_event_queue.o \
			  src/fw_lib/sbuf_intf.o \
			  src/fw_lib/metadata_intf.o \
			  src/fw_lib/crop_fsm.o \
			  src/fw_lib/noise_reduction_intf.o \
			  src/fw_lib/acamera_ctrl_channel_k2u.o \
			  src/fw_lib/ae_manual_fsm.o \
			  src/fw_lib/cmos_fsm.o \
			  src/fw_lib/sbuf_fsm.o \
			  src/fw_lib/monitor_intf.o \
			  src/fw_lib/sharpening_fsm.o \
			  src/fw_lib/acamera_fsm_mgr.o \
			  src/fw_lib/matrix_yuv_intf.o \
			  src/fw_lib/awb_manual_func.o \
			  src/fw_lib/af_manual_func.o \
			  src/fw_lib/acamera_fw.o \
			  src/fw_lib/sharpening_func.o \
			  src/fw_lib/iridix8_manual_intf.o \
			  src/fw_lib/color_matrix_fsm.o \
			  src/fw_lib/sbuf_func.o \
			  src/fw_lib/gamma_manual_func.o \
			  src/fw_lib/dma_writer_fsm.o \
			  src/fw_lib/dma_writer_intf.o \
			  src/fw_lib/crop_func.o \
			  src/fw_lib/noise_reduction_func.o \
			  src/fw_lib/crop_intf.o \
			  src/fw_lib/acamera_loop_buf.o \
			  src/fw_lib/color_matrix_intf.o \
			  src/fw_lib/acamera_sbus_spi.o \
			  src/calibration/acamera_get_calibrations_v4l2.o \
			  src/calibration/soc_iq_calibrations.o \
			  src/driver/sensor/sensor_bus_config.o \
			  src/driver/sensor/V4L2_drv.o \
			  src/driver/sensor/ACameraDefault.o \
			  src/driver/lens/v4l2_vcm.o \
			  src/driver/lens/lens_init.o




iv009_isp-objs += $(FW_SRC_OBJ) 

INCLUDE_DIRS := $(addprefix -I,$(shell find ../ -type d 2> /dev/null))

ccflags-y:=-I$(DIR)/app -I$(DIR)/inc -I$(DIR)/app/control -I$(DIR)/inc/api -I$(DIR)/inc/isp -I$(DIR)/inc/sys -I$(DIR)/src/platform -I$(DIR)/src/fw  -I$(DIR)/src/fw_lib -I$(DIR)/src/calibration -I$(DIR)/src/driver/sensor -I$(DIR)/src/driver/lens -I$(_KDIR)/include/linux/

ccflags-y += -Wno-declaration-after-statement
