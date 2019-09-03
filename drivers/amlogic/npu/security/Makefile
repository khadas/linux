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


TARGET	= ../../driver/galcore.ta

TEE_PATH := $(AQROOT)/../../vendor/marvell-sdk/tee

INCLUDES := \
	$(AQROOT)/hal/inc \
	$(AQROOT)/hal/user \
	$(AQROOT)/compiler/libVSC/include \
	$(AQROOT)/hal/os/linux/user \
	$(AQARCH)/cmodel/inc \


CFLAGS = -Wa, -O2 -nostdlib -fno-builtin -march=armv7-a -fno-short-enums
CFLAGS += $(addprefix -I, $(sort $(dir $(objs))))
CFLAGS += -I. -I$(TEE_PATH)/include/ -I$(TEE_PATH)/tee/common/
CFLAGS += -I$(TEE_PATH)/tee/internal_api/ -I$(TEE_PATH)/tee/mgr/
CFLAGS += -fPIC -DCONFIG_SO
CFLAGS += -I$(AQROOT)/hal/inc
CFLAGS += -I$(AQROOT)/hal/user
CFLAGS += -I$(AQROOT)/compiler/libVSC/include
CFLAGS += -I$(AQROOT)/hal/os/linux/user
CFLAGS += -I$(AQARCH)/cmodel/inc

PLATFORM_LIBS = -L $(shell dirname `$(CC) $(CFLAGS) -print-libgcc-file-name`) -lgcc

CROSS_COMPILE = arm-marvell-eabi-
CC	= $(CROSS_COMPILE)gcc
OBJDUMP	= $(CROSS_COMPILE)objdump
OBJCOPY	= $(CROSS_COMPILE)objcopy
LD	= $(CROSS_COMPILE)ld
AR	= $(CROSS_COMPILE)ar

objs := gc_hal_ta.o \
        gc_hal_ta_command.o \
        gc_hal_ta_mmu.o \
        gc_hal_ta_hardware.o \
        os/linux/gc_hal_ta_linux.o \
        os/linux/gc_hal_ta_driver.o

headers := os/linux/gc_hal_ta_linux.h

all: $(TARGET) $(objs)

%.o: %.c $(headers)
	$(CC) -c $(CFLAGS) $< -o $@

$(TARGET): $(objs)
	$(CC) $(CFLAGS) $(LDFLAGS) -shared -Wl,-soname,galcore.ta -o $@ $^ $(PLATFORM_LIBS)

clean:
	@rm -f $(TARGET) *.o
