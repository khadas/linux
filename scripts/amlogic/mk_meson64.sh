# SPDX-License-Identifier: (GPL-2.0+ OR MIT)
#
# Copyright (c) 2019 Amlogic, Inc. All rights reserved.
#

export CROSS_COMPILE=/opt/gcc-arm-8.3-2019.03-x86_64-aarch64-linux-gnu/bin/aarch64-linux-gnu-
make ARCH=arm64 meson64_a64_defconfig
make ARCH=arm64 Image modules -j12
make ARCH=arm64 dtbs -j12
