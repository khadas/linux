# SPDX-License-Identifier: GPL-2.0
#
# Makefile for the linux pstorefs routines.
#

obj-$(CONFIG_PSTORE) += pstore.o

ifdef CONFIG_AMLOGIC_ZSTD
pstore-objs += inode.o aml_platform.o
else
pstore-objs += inode.o platform.o
endif
pstore-$(CONFIG_PSTORE_FTRACE)	+= ftrace.o

pstore-$(CONFIG_PSTORE_PMSG)	+= pmsg.o

ramoops-objs += ram.o ram_core.o
obj-$(CONFIG_PSTORE_RAM)	+= ramoops.o

pstore_zone-objs += zone.o
obj-$(CONFIG_PSTORE_ZONE)	+= pstore_zone.o

pstore_blk-objs += blk.o
obj-$(CONFIG_PSTORE_BLK)	+= pstore_blk.o
