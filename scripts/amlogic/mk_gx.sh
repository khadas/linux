#! /bin/bash

make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- -j20 Image\
	UIMAGE_LOADADDR=0x1008000 || echo "Compile uImage Fail !!"

make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- gxbb_skt.dtb \
	|| echo "Compile dtb Fail !!"


make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- gxbb_p200.dtb \
	|| echo "Compile dtb Fail !!"

make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- gxbb_p200_2G.dtb \
	|| echo "Compile dtb Fail !!"

make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- gxbb_p201.dtb \
	|| echo "Compile dtb Fail !!"

make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- gxl_pxp.dtb \
	|| echo "Compile dtb Fail !!"
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- gxl_skt.dtb \
	|| echo "Compile dtb Fail !!"
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- gxl_p212.dtb \
	|| echo "Compile dtb Fail !!"
#rootfs.cpio -- original buildroot rootfs, busybox
#ROOTFS="rootfs.cpio"

#./mkbootimg --kernel ./arch/arm/boot/uImage \
#	--ramdisk ./${ROOTFS} \
#	--second ./arch/arm/boot/dts/amlogic/meson8m2_n200_2G.dtb \
#	--output ./m8boot.img || echo "Compile m8boot.img Fail !!"
