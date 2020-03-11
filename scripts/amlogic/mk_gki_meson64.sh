# SPDX-License-Identifier: (GPL-2.0+ OR MIT)
#
# Copyright (c) 2019 Amlogic, Inc. All rights reserved.
#

export CROSS_COMPILE=/opt/gcc-arm-8.3-2019.03-x86_64-aarch64-linux-gnu/bin/aarch64-linux-gnu-
#ARCH=arm64 ./scripts/kconfig/merge_config.sh arch/arm64/configs/gki_defconfig arch/arm64/configs/meson64_gki.fragment
ARCH=arm64 ./scripts/kconfig/merge_config.sh arch/arm64/configs/meson64_gki_defconfig arch/arm64/configs/meson64_gki.fragment
make ARCH=arm64 Image -j12
make ARCH=arm64 dtbs -j12
make ARCH=arm64 modules -j12
