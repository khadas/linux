# SPDX-License-Identifier: (GPL-2.0+ OR MIT)
#
# Copyright (c) 2019 Amlogic, Inc. All rights reserved.
#

export CROSS_COMPILE=/opt/gcc-arm-8.3-2019.03-x86_64-arm-linux-gnueabihf/bin/arm-linux-gnueabihf-

make ARCH=arm meson64_a32_defconfig
make ARCH=arm uImage modules -j12 LOADADDR=0x108000
make ARCH=arm dtbs -j12 LOADADDR=0x108000

