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
cat arch/arm64/configs/gki_defconfig arch/arm64/configs/meson64_gki.fragment > arch/arm64/configs/meson64_gki_r_defconfig
make ${clang_flags} meson64_gki_r_defconfig
rm arch/arm64/configs/meson64_gki_r_defconfig
make ${clang_flags} Image modules -j12
make ${clang_flags} dtbs -j12
