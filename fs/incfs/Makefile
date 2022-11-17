# SPDX-License-Identifier: GPL-2.0
obj-$(CONFIG_INCREMENTAL_FS)	+= incrementalfs.o

ifdef CONFIG_AMLOGIC_ZSTD
incrementalfs-y := \
	aml_data_mgmt.o \
	format.o \
	integrity.o \
	main.o \
	pseudo_files.o \
	sysfs.o \
	vfs.o
else
incrementalfs-y := \
	data_mgmt.o \
	format.o \
	integrity.o \
	main.o \
	pseudo_files.o \
	sysfs.o \
	vfs.o
endif

incrementalfs-$(CONFIG_FS_VERITY) += verity.o
