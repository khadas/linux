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

obj-m += iv009_isp_lens.o

M_PATH := drivers/amlogic/isp_module/subdev/lens

FW_SRC_OBJ := src/platform/system_i2c.o \
			  src/platform/system_stdlib.o \
			  src/platform/system_spi.o \
			  src/platform/system_hw_io.o \
			  src/platform/system_sw_io.o \
			  src/platform/system_sensor.o \
			  src/platform/system_spinlock.o \
			  src/platform/system_timer.o \
			  src/platform/system_log.o \
			  src/fw_lib/acamera_math.o \
			  src/fw_lib/sensor_init.o \
			  src/fw_lib/acamera_sbus_isp_sw.o \
			  src/fw_lib/acamera_sbus_i2c.o \
			  src/fw_lib/acamera_logger.o \
			  src/fw_lib/acamera_sbus_spi.o \
			  src/fw_lib/acamera_sbus_isp.o \
			  src/fw_lib/acamera_sbus.o \
			  src/driver/sensor/Dummy_drv.o \
			  src/driver/sensor/ACameraDefault.o \
			  src/driver/sensor/sensor_bus_config.o \
			  src/driver/lens/null_vcm.o \
			  src/driver/lens/lens_init.o \
			  app/soc_lens.o


iv009_isp_lens-objs += $(FW_SRC_OBJ)

LOCAL_INCLUDES := -I$(M_PATH)/app -I$(M_PATH)/inc -I$(M_PATH)/app/control \
                -I$(M_PATH)/inc/api -I$(M_PATH)/inc/isp -I$(M_PATH)/inc/sys \
                -I$(M_PATH)/src/platform -I$(M_PATH)/src/fw \
                -I$(M_PATH)/src/fw_lib -I$(M_PATH)/src/calibration \
                -I$(M_PATH)/src/driver/sensor -I$(M_PATH)/src/driver/lens

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
