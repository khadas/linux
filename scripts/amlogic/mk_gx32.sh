#! /bin/bash

export PATH=/opt/clang-r377782b/bin/:/opt/gcc-arm-8.3-2019.03-x86_64-arm-linux-gnueabihf/bin/arm-linux-gnueabihf-:$PATH
MAKE_CLANG='make ARCH=arm LLVM=1 CC=clang HOSTCC=clang LD=ld.lld NM=llvm-nm OBJCOPY=llvm-objcopy CLANG_TRIPLE=arm-linux-gnueabihf- CROSS_COMPILE=arm-linux-gnueabihf- -j8'
export INSTALL_MOD_PATH=./modules_install

$MAKE_CLANG meson64_a32_defconfig

set -ex
$MAKE_CLANG uImage
$MAKE_CLANG dtbs
$MAKE_CLANG modules

