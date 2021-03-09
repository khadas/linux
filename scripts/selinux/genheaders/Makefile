# SPDX-License-Identifier: GPL-2.0
hostprogs-y	:= genheaders
HOST_EXTRACFLAGS += \
	-I$(srctree)/include/uapi -I$(srctree)/include \
	-I$(srctree)/security/selinux/include

ifneq ($(shell grep CONFIG_AMLOGIC_ANDROIDP= $(srctree)/.config), )
HOST_EXTRACFLAGS += -DCONFIG_AMLOGIC_ANDROIDP=1
else
ifneq ($(shell grep CONFIG_AMLOGIC_ANDROIDP= $(objtree)/.config), )
HOST_EXTRACFLAGS += -DCONFIG_AMLOGIC_ANDROIDP=1
endif
endif

always		:= $(hostprogs-y)
