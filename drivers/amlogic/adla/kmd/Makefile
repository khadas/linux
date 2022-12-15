M ?= $(srctree)/drivers/amlogic/adla/kmd
KERNEL_SRC ?= $(srctree)

HAS_PM_DOMAIN ?= 1
DEBUG ?= 0

ifeq ($(DEBUG),1)
EXTRA_CFLAGS1 += -DCONFIG_ADLAK_DEBUG=1
endif

ifeq ($(HAS_PM_DOMAIN),0)
EXTRA_CFLAGS1 += -DCONFIG_HAS_PM_DOMAIN=0
endif


EXTRA_INCLUDE := -I $(M)/drv/port/platform/linux
EXTRA_INCLUDE += -I $(M)/drv/port/platform
EXTRA_INCLUDE += -I $(M)/drv/port/os/linux/mm
EXTRA_INCLUDE += -I $(M)/drv/port/os/linux
EXTRA_INCLUDE += -I $(M)/drv/port/os
EXTRA_INCLUDE += -I $(M)/drv/port
EXTRA_INCLUDE += -I $(M)/drv/uapi/linux
EXTRA_INCLUDE += -I $(M)/drv/uapi
EXTRA_INCLUDE += -I $(M)/drv/common/mm
EXTRA_INCLUDE += -I $(M)/drv/common
EXTRA_INCLUDE += -I $(M)/drv

ccflags-y += $(EXTRA_INCLUDE)
EXTRA_CFLAGS+= -Wno-error -Wno-unused-variable
EXTRA_CFLAGS += $(EXTRA_INCLUDE) $(EXTRA_CFLAGS1)

obj-m	+=	adla_core.o
#NPU_DIR := $(shell dirname $(lastword $(MAKEFILE_LIST)))
adla_core-objs += drv/port/platform/linux/adlak_debugfs.o
adla_core-objs += drv/port/platform/linux/adlak_interrupt.o
adla_core-objs += drv/port/platform/adlak_hw.o
adla_core-objs += drv/port/platform/linux/adlak_io.o
adla_core-objs += drv/port/platform/linux/adlak_platform_config.o
adla_core-objs += drv/port/platform/linux/adlak_platform_device.o
adla_core-objs += drv/port/os/linux/adlak_os.o
adla_core-objs += drv/port/os/linux/mm/adlak_mm_os_common.o
adla_core-objs += drv/port/os/linux/mm/adlak_mm_mbp.o
adla_core-objs += drv/common/mm/adlak_mm_common.o
adla_core-objs += drv/common/mm/adlak_mm_smmu.o
adla_core-objs += drv/common/mm/adlak_mm_smmu_tlb.o
adla_core-objs += drv/common/adlak_dpm.o
adla_core-objs += drv/common/adlak_queue.o
adla_core-objs += drv/common/adlak_context.o
adla_core-objs += drv/common/adlak_profile.o
adla_core-objs += drv/common/adlak_mm.o
adla_core-objs += drv/common/adlak_device.o
adla_core-objs += drv/common/adlak_submit.o
