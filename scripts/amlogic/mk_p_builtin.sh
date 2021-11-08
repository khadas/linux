# SPDX-License-Identifier: (GPL-2.0+ OR MIT)
#
# Copyright (c) 2019 Amlogic, Inc. All rights reserved.
#

target_defconfig=meson64_a64_P_defconfig
echo "defconfig="$target_defconfig
echo ""

###########################################################################################
######################## step 1:  env setup ###############################################
###########################################################################################
echo "######################## step 1:  env setup ###############################################"
export PATH=/opt/clang-r383902/bin/:/opt/gcc-linaro-6.3.1-2017.02-x86_64_aarch64-linux-gnu/bin/:$PATH
MAKE_CLANG='make ARCH=arm64 CC=clang HOSTCC=clang LD=ld.lld NM=llvm-nm OBJCOPY=llvm-objcopy CLANG_TRIPLE=aarch64-linux-gnu- CROSS_COMPILE=aarch64-linux-gnu- -j8'
export INSTALL_MOD_PATH=./modules_install

find -type f | grep "\.ko$" | xargs rm -fr
rm -fr $INSTALL_MOD_PATH

###########################################################################################
######################## step 2:  build kernel & builtin modules ##########################
###########################################################################################
echo "######################## step 2:  build kernel & builtin modules ##########################"
#make ${clang_flags} mrproper
#make ${clang_flags} clean
$MAKE_CLANG $target_defconfig

set -ex
$MAKE_CLANG Image
$MAKE_CLANG dtbs
$MAKE_CLANG modules
set +ex

echo "Build success!"
