# SPDX-License-Identifier: GPL-2.0
obj-$(CONFIG_F2FS_FS) += f2fs.o

ifdef CONFIG_AMLOGIC_ZSTD
f2fs-y		:= dir.o file.o inode.o namei.o hash.o aml_super.o inline.o
else
f2fs-y		:= dir.o file.o inode.o namei.o hash.o super.o inline.o
endif
f2fs-y		+= checkpoint.o gc.o data.o node.o segment.o recovery.o
f2fs-y		+= shrinker.o extent_cache.o sysfs.o
f2fs-$(CONFIG_F2FS_STAT_FS) += debug.o
f2fs-$(CONFIG_F2FS_FS_XATTR) += xattr.o
f2fs-$(CONFIG_F2FS_FS_POSIX_ACL) += acl.o
f2fs-$(CONFIG_FS_VERITY) += verity.o
ifdef CONFIG_AMLOGIC_ZSTD
f2fs-$(CONFIG_F2FS_FS_COMPRESSION) += aml_compress.o
else
f2fs-$(CONFIG_F2FS_FS_COMPRESSION) += compress.o
endif
f2fs-$(CONFIG_F2FS_IOSTAT) += iostat.o
