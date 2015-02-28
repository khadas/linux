#! /bin/bash

make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- -j20 \
	UIMAGE_LOADADDR=0x1008000 || echo "Compile uImage Fail !!"

make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- gxbaby.dtb \
	|| echo "Compile dtb Fail !!"


#rootfs.cpio -- original buildroot rootfs, busybox
#ROOTFS="rootfs.cpio"

#./mkbootimg --kernel ./arch/arm/boot/uImage \
#	--ramdisk ./${ROOTFS} \
#	--second ./arch/arm/boot/dts/amlogic/meson8m2_n200_2G.dtb \
#	--output ./m8boot.img || echo "Compile m8boot.img Fail !!"
