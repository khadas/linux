#!/bin/bash

RESOURCE_TOOL=scripts/resource_tool
DTB=arch/arm64/boot/dts/rockchip/rk3399-khadas-captain-linux.dtb
UBOOT_LOGO=logo.bmp
KERNEL_LOGO=logo_kernel.bmp


$RESOURCE_TOOL $DTB $UBOOT_LOGO $KERNEL_LOGO > /dev/null

cp resource.img logo.img
echo logo.img is ready
