# SPDX-License-Identifier: (GPL-2.0+ OR MIT)
#
# Copyright (c) 2019 Amlogic, Inc. All rights reserved.
#

target_defconfig=meson64_a64_R_defconfig
MKIMAGE=./scripts/amlogic/mkimage
if [ -z "$base_ramdisk" ];then
	base_ramdisk=./scripts/amlogic/rootfs_base.cpio.gz.uboot
fi

echo "defconfig="$target_defconfig
echo "base_ramdisk="$base_ramdisk
echo "MKIMAGE="$MKIMAGE
echo ""

if [ ! -f $MKIMAGE -o ! -f $base_ramdisk ];then
	skip_generate_ramdisk=true
	echo "###################################### NOTICE ###################################################"
	echo "## $MKIMAGE or $base_ramdisk doesn't exsit!"
	echo "## Can't generate rootfs_gki.cpio.gz.uboot automatically, you can download these files from wiki:"
	echo "##    https://wiki-china.amlogic.com/Platform/Kernel/Kernel5.4/Build"
	echo "##"
	echo "## Also you can use your own base_ramdisk with below command:"
	echo "##    export base_ramdisk=\"your ramdisk path\""
	echo "#################################################################################################"
	echo ""
fi

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

if [ $target_defconfig != meson64_a64_R_defconfig ];then
	exit 0;
fi

###########################################################################################
######################## step 3:  build external modules ##################################
###########################################################################################
echo "######################## step 3:  build external modules ##################################"
GKI_EXT_MODULE_CFG=arch/arm64/configs/meson64_gki_module_config

function read_ext_module_config() {
	ALL_LINE=""
	while read LINE
	do
		if [[ $LINE != \#*  &&  $LINE != "" ]]; then
			ALL_LINE="$ALL_LINE"" ""$LINE"
		fi
	done < $1
	echo $ALL_LINE
}

function read_ext_module_predefine() {
	PRE_DEFINE=""

	for y_config in `cat $1 | grep "^CONFIG_.*=y" | sed 's/=y//'`;
	do
		PRE_DEFINE="$PRE_DEFINE"" -D"${y_config}
	done

	for m_config in `cat $1 | grep "^CONFIG_.*=m" | sed 's/=m//'`;
	do
		PRE_DEFINE="$PRE_DEFINE"" -D"${m_config}_MODULE
	done

	echo $PRE_DEFINE
}

GKI_EXT_MODULE_CONFIG=$(read_ext_module_config $GKI_EXT_MODULE_CFG)
export GKI_EXT_MODULE_CONFIG

GKI_EXT_MODULE_PREDEFINE=$(read_ext_module_predefine $GKI_EXT_MODULE_CFG)
export GKI_EXT_MODULE_PREDEFINE

# ----------- basic common modules build as extern --------------
EXT_MODULES="drivers/amlogic
sound/soc/amlogic
sound/soc/codecs/amlogic
"

KERNEL_SRC=${PWD}
KERNEL_SRC=`readlink -f $KERNEL_SRC`
echo KERNEL_SRC=$KERNEL_SRC

if [[ -z "${SKIP_EXT_MODULES}" ]] && [[ -n "${EXT_MODULES}" ]]; then

  for EXT_MOD in ${EXT_MODULES}; do
    EXT_MOD=`readlink -f $EXT_MOD`
    M=`echo $KERNEL_SRC | sed 's#/[^/]*#../#g'`$EXT_MOD
    set -ex
    $MAKE_CLANG -C ${EXT_MOD} M=${M} KERNEL_SRC=${KERNEL_SRC}
    set +ex
  done
fi

###########################################################################################
######################## step 4:  install builtin&external modules ########################
###########################################################################################
echo "######################## step 4:  install builtin&external modules ########################"
mkdir -p ${INSTALL_MOD_PATH}/target/
mkdir -p ${INSTALL_MOD_PATH}/lib/modules/0.0/lib/modules/
for loop in `find -type f | grep ko$`;
do
	echo cp $loop ${INSTALL_MOD_PATH}/lib/modules/0.0/lib/modules/
	cp $loop ${INSTALL_MOD_PATH}/lib/modules/0.0/lib/modules/
done

set -ex
depmod -b ${INSTALL_MOD_PATH}/ 0.0

cp ${INSTALL_MOD_PATH}/lib/modules/0.0/lib/modules/*.ko  ${INSTALL_MOD_PATH}/target/
cp ${INSTALL_MOD_PATH}/lib/modules/0.0/modules.dep ${INSTALL_MOD_PATH}/target/
aarch64-linux-gnu-strip -d ${INSTALL_MOD_PATH}/target/*.ko
set +ex

if [ "$BUILD_FOR_AUTOSH" == true ];then
	echo "BUILD_FOR_AUTOSH=true, exit"
	exit 0
fi

############### generate __install.sh ################
echo > ${INSTALL_MOD_PATH}/target/__install.sh
sed -i 's#[^ ]*/##g'  ${INSTALL_MOD_PATH}/target/modules.dep

function mod_probe() {
        local ko=$1
        local loop
        for loop in `grep "$ko:" ${INSTALL_MOD_PATH}/target/modules.dep | sed 's/.*://'`;
        do
                mod_probe $loop
                echo insmod $loop >> ${INSTALL_MOD_PATH}/target/__install.sh
        done
}

for loop in `cat ${INSTALL_MOD_PATH}/target/modules.dep | sed 's/:.*//'`; do
        mod_probe $loop
        echo insmod $loop >> ${INSTALL_MOD_PATH}/target/__install.sh
done

cat ${INSTALL_MOD_PATH}/target/__install.sh  | awk ' {
        if (!cnt[$2]) {
                print $0;
                cnt[$2]++;
        }
}' > ${INSTALL_MOD_PATH}/target/__install.sh.tmp

mv ${INSTALL_MOD_PATH}/target/__install.sh.tmp ${INSTALL_MOD_PATH}/target/__install.sh

sed -i '1s/^/#!\/bin\/sh\n\nset -ex\n/' ${INSTALL_MOD_PATH}/target/__install.sh
echo "echo Install modules success!" >> ${INSTALL_MOD_PATH}/target/__install.sh
chmod 777 ${INSTALL_MOD_PATH}/target/__install.sh

echo "#!/bin/sh" > ${INSTALL_MOD_PATH}/target/install.sh
echo "./__install.sh || reboot" >> ${INSTALL_MOD_PATH}/target/install.sh
chmod 777 ${INSTALL_MOD_PATH}/target/*.sh

echo ""
echo "${INSTALL_MOD_PATH}/target/: all `wc -l ${INSTALL_MOD_PATH}/target/modules.dep | awk '{print $1}'` modules."


if [ "$skip_generate_ramdisk" == true ];then
	echo "skip_generate_ramdisk=true, exit"
	echo ""
	exit 0
fi

###########################################################################################
######################## step 5: generate rootfs_gki.cpio.gz.uboot ########################
###########################################################################################
echo "######################## step 5: generate rootfs_gki.cpio.gz.uboot ########################"

rm ${INSTALL_MOD_PATH}/ramdisk/ -fr
mkdir ${INSTALL_MOD_PATH}/ramdisk/ -p

(cd ${INSTALL_MOD_PATH}/ramdisk/ && cp ../target/ . -r && find . | cpio -o -H newc | gzip > ../rootfs_target.cpio.gz) && 
dd if=$base_ramdisk of=${INSTALL_MOD_PATH}/rootfs_base.cpio.gz bs=64 skip=1 &&
(cd ${INSTALL_MOD_PATH}/ && cat rootfs_base.cpio.gz rootfs_target.cpio.gz > rootfs_gki.cpio.gz) &&
(cd ${INSTALL_MOD_PATH}/ && ../$MKIMAGE -A arm -T ramdisk -C none -d rootfs_gki.cpio.gz rootfs_gki.cpio.gz.uboot)

rm -fr ${INSTALL_MOD_PATH}/*.gz

if [ -f ${INSTALL_MOD_PATH}/rootfs_gki.cpio.gz.uboot ];then
	echo "Success generate ramdisk:"
	ls -l ${INSTALL_MOD_PATH}/rootfs_gki.cpio.gz.uboot
else
	echo 
	echo "Error, generate rootfs_gki.cpio.gz.uboot failed!!!!!!!!!!!!!!!!!!!"
	echo 
fi
echo ""
