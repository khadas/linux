# SPDX-License-Identifier: (GPL-2.0+ OR MIT)
#
# Copyright (c) 2019 Amlogic, Inc. All rights reserved.
#

export CROSS_COMPILE=/opt/gcc-arm-8.3-2019.03-x86_64-aarch64-linux-gnu/bin/aarch64-linux-gnu-
#ARCH=arm64 ./scripts/kconfig/merge_config.sh arch/arm64/configs/gki_defconfig arch/arm64/configs/meson64_gki.fragment
export INSTALL_MOD_PATH=./modules_install
cat arch/arm64/configs/meson64_gki_module_config arch/arm64/configs/meson64_gki.fragment > arch/arm64/configs/meson64_gki.fragment.y
sed -i 's/=m/=y/' arch/arm64/configs/meson64_gki.fragment.y
ARCH=arm64 ./scripts/kconfig/merge_config.sh arch/arm64/configs/gki_defconfig arch/arm64/configs/meson64_gki.fragment.y
rm -fr arch/arm64/configs/meson64_gki.fragment.y

make ARCH=arm64 Image -j12
make ARCH=arm64 dtbs -j12

mkimage -A arm64 -O linux -T kernel -C none -a 0x1080000 -e 0x1080000 -n linux-next -d arch/arm64/boot/Image uImage
