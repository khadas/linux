# SPDX-License-Identifier: (GPL-2.0+ OR MIT)
#
# Copyright (c) 2019 Amlogic, Inc. All rights reserved.
#

export ARCH=arm64
export CROSS_COMPILE=aarch64-linux-gnu-
export CLANG_TRIPLE=aarch64-linux-gnu-
export PATH=/opt/clang-r377782b/bin/:$PATH
clang_flags="ARCH=arm64 CC=clang HOSTCC=clang LD=ld.lld NM=llvm-nm OBJCOPY=llvm-objcopy"

which clang

cat arch/arm64/configs/meson64_gki_module_config arch/arm64/configs/meson64_gki.fragment > arch/arm64/configs/meson64_gki.fragment.y
sed -i 's/=m/=y/' arch/arm64/configs/meson64_gki.fragment.y
cat ./scripts/kconfig/merge_config.sh arch/arm64/configs/gki_defconfig arch/arm64/configs/meson64_gki.fragment.y > arch/arm64/configs/meson64_tmp_defconfig
rm -fr arch/arm64/configs/meson64_gki.fragment.y

make ${clang_flags} mrproper -j12
make ${clang_flags} clean -j12
make ${clang_flags} meson64_tmp_defconfig -j12
rm arch/arm64/configs/meson64_tmp_defconfig

make ${clang_flags} Image -j12
make ${clang_flags} dtbs -j12

mkimage -A arm64 -O linux -T kernel -C none -a 0x1080000 -e 0x1080000 -n linux-next -d arch/arm64/boot/Image uImage
