# SPDX-License-Identifier: (GPL-2.0+ OR MIT)
#
# Copyright (c) 2019 Amlogic, Inc. All rights reserved.
#

export CROSS_COMPILE=/opt/gcc-arm-10.2-2020.11-x86_64-aarch64-none-linux-gnu/bin/aarch64-none-linux-gnu-
#cat arch/arm64/configs/meson64_a64_smarthome_defconfig  scripts/amlogic/configs/user_config scripts/amlogic/configs/unsec_config  >  arch/arm64/configs/meson64_a64_smarthome_min_defconfig
#make ARCH=arm64 meson64_a64_smarthome_min_defconfig
#rm arch/arm64/configs/meson64_a64_smarthome_min_defconfig
make ARCH=arm64 meson64_a64_smarthome_defconfig
make ARCH=arm64 Image modules -j12
make ARCH=arm64 dtbs -j12

