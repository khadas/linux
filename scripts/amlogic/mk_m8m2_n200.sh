#! /bin/bash

#make UIMAGE_COMPRESSION=none uImage -j
rm -rf m8boot.img
make  ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- uImage -j20 UIMAGE_LOADADDR=0x1008000
#make modules

make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- meson8m2_n200_2G.dtb

#cd ../root/g18
#find .| cpio -o -H newc | gzip -9 > ../ramdisk.img

#rootfs.cpio -- original buildroot rootfs, busybox
#m8rootfs.cpio -- build from buildroot
ROOTFS="rootfs.cpio"

#cd ..
./mkbootimg --kernel ./arch/arm/boot/uImage --ramdisk ./${ROOTFS} --second ./arch/arm/boot/dts/amlogic/meson8m2_n200_2G.dtb --output ./m8boot.img
ls -l ./m8boot.img
echo "m8boot.img done"
