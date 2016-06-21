#
# Makefile for MMC/SD host controller drivers
#

ifeq ($(CONFIG_MMC_DEBUG),y)
	EXTRA_CFLAGS		+= -DDEBUG
endif

obj-$(CONFIG_MMC_AML)	+= aml_sdio.o amlsd.o \
                           amlsd_of.o emmc_partitions.o aml_sd_emmc.o \
                           amlmmc_dtb.o emmc_key.o \
                           mmc_notify.o



