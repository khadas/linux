# SPDX-License-Identifier: (GPL-2.0+ OR MIT)
#
# Copyright (c) 2019 Amlogic, Inc. All rights reserved.
#

export ARCH=arm64
export CROSS_COMPILE=aarch64-linux-gnu-
export CLANG_TRIPLE=aarch64-linux-gnu-
export PATH=/opt/clang-r377782b/bin/:$PATH
clang_flags="ARCH=arm64 CC=clang HOSTCC=clang LD=ld.lld NM=llvm-nm OBJCOPY=llvm-objcopy"
export INSTALL_MOD_PATH=./modules_install

cp arch/arm64/configs/meson64_gki.fragment arch/arm64/configs/meson64_gki.fragment.m
#### set usb configs to y for bringup ####
sed -i 's/CONFIG_USB_DWC3=m/CONFIG_USB_DWC3=y/' arch/arm64/configs/meson64_gki.fragment.m
sed -i 's/CONFIG_AMLOGIC_CRG=m/CONFIG_AMLOGIC_CRG=y/' arch/arm64/configs/meson64_gki.fragment.m
sed -i 's/CONFIG_AMLOGIC_USB=m/CONFIG_AMLOGIC_USB=y/' arch/arm64/configs/meson64_gki.fragment.m
sed -i 's/CONFIG_AMLOGIC_USB_DWC_OTG_HCD=m/CONFIG_AMLOGIC_USB_DWC_OTG_HCD=y/' arch/arm64/configs/meson64_gki.fragment.m
sed -i 's/CONFIG_AMLOGIC_USB_HOST_ELECT_TEST=m/CONFIG_AMLOGIC_USB_HOST_ELECT_TEST=y/' arch/arm64/configs/meson64_gki.fragment.m
sed -i 's/CONFIG_AMLOGIC_USBPHY=m/CONFIG_AMLOGIC_USBPHY=y/' arch/arm64/configs/meson64_gki.fragment.m
sed -i 's/CONFIG_AMLOGIC_USB2PHY=m/CONFIG_AMLOGIC_USB2PHY=y/' arch/arm64/configs/meson64_gki.fragment.m
sed -i 's/CONFIG_AMLOGIC_USB3PHY=m/CONFIG_AMLOGIC_USB3PHY=y/' arch/arm64/configs/meson64_gki.fragment.m
sed -i 's/CONFIG_AMLOGIC_PCIE=m/CONFIG_AMLOGIC_PCIE=y/' arch/arm64/configs/meson64_gki.fragment.m
#### usb depends on gx_pm.ko ####
echo CONFIG_AMLOGIC_GX_SUSPEND=y >> arch/arm64/configs/meson64_gki.fragment.m
echo CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND=y >> arch/arm64/configs/meson64_gki.fragment.m
cp arch/arm64/configs/meson64_gki_module_config arch/arm64/configs/meson64_gki_module_config.tmp
sed -i 's/.*CONFIG_AMLOGIC_GX_SUSPEND=.*//' arch/arm64/configs/meson64_gki_module_config.tmp
sed -i 's/.*CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND=.*//' arch/arm64/configs/meson64_gki_module_config.tmp
cat arch/arm64/configs/gki_defconfig arch/arm64/configs/meson64_gki.fragment.m > arch/arm64/configs/meson64_tmp_defconfig
rm arch/arm64/configs/meson64_gki.fragment.m

#make ${clang_flags} mrproper
#make ${clang_flags} clean
make ${clang_flags} meson64_tmp_defconfig

rm arch/arm64/configs/meson64_tmp_defconfig
find -type f | grep "\.ko$" | xargs rm

set -ex
which clang
make ${clang_flags} Image -j12
make ${clang_flags} dtbs -j12
make ${clang_flags} modules -j12
mkimage -A arm64 -O linux -T kernel -C none -a 0x1080000 -e 0x1080000 -n linux-next -d arch/arm64/boot/Image uImage
set +ex


######################## build meson64_gki_module_config modules #########################
GKI_EXT_MODULE_CFG=arch/arm64/configs/meson64_gki_module_config.tmp

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

rm $GKI_EXT_MODULE_CFG

# ----------- basic common modules build as extern --------------
EXT_MODULES="drivers/amlogic
sound/soc/amlogic
sound/soc/codecs/amlogic
"

KERNEL_SRC=${PWD}
KERNEL_SRC=`readlink -f $KERNEL_SRC`
echo KERNEL_SRC=$KERNEL_SRC

if [[ -z "${SKIP_EXT_MODULES}" ]] && [[ -n "${EXT_MODULES}" ]]; then
  echo "========================================================"
  echo " Building external modules"

  for EXT_MOD in ${EXT_MODULES}; do
    EXT_MOD=`readlink -f $EXT_MOD`
    M=`echo $KERNEL_SRC | sed 's#/[^/]*#../#g'`$EXT_MOD
    set -ex
    make ${clang_flags} -C ${EXT_MOD} M=${M} KERNEL_SRC=${KERNEL_SRC} -j12
    set +ex
  done

fi

############################ install builtin/extern modules #########################
echo "=================================================="
echo "install modules...."
rm -rf ${INSTALL_MOD_PATH}
mkdir -p ${INSTALL_MOD_PATH}/target/
mkdir -p ${INSTALL_MOD_PATH}/lib/modules/0.0/lib/modules/
for loop in `find -type f | grep ko$`;
do
	echo cp $loop ${INSTALL_MOD_PATH}/lib/modules/0.0/lib/modules/
	cp $loop ${INSTALL_MOD_PATH}/lib/modules/0.0/lib/modules/
done

set -x
depmod -b ${INSTALL_MOD_PATH}/ 0.0

cp ${INSTALL_MOD_PATH}/lib/modules/0.0/lib/modules/*.ko  ${INSTALL_MOD_PATH}/target/
cp ${INSTALL_MOD_PATH}/lib/modules/0.0/modules.dep ${INSTALL_MOD_PATH}/target/
set +x

############### generate modules_install.sh ################
> ${INSTALL_MOD_PATH}/target/modules_install.sh
sed -i 's#[^ ]*/##g'  ${INSTALL_MOD_PATH}/target/modules.dep

function mod_probe()
{
        local ko=$1
        local loop
        for loop in `grep "$ko:" ${INSTALL_MOD_PATH}/target/modules.dep | sed 's/.*://'`;
        do
                mod_probe $loop
                echo insmod $loop >> ${INSTALL_MOD_PATH}/target/modules_install.sh
        done
}

for loop in `cat ${INSTALL_MOD_PATH}/target/modules.dep | sed 's/:.*//'`;
do
        mod_probe $loop
        echo insmod $loop >> ${INSTALL_MOD_PATH}/target/modules_install.sh
done

cat ${INSTALL_MOD_PATH}/target/modules_install.sh  | awk '
{
        if (!cnt[$2]) {
                print $0;
                cnt[$2]++;
        }
}' > ${INSTALL_MOD_PATH}/target/modules_install.sh.tmp

mv ${INSTALL_MOD_PATH}/target/modules_install.sh.tmp ${INSTALL_MOD_PATH}/target/modules_install.sh

sed -i '1s/^/#!\/bin\/sh\n&/' ${INSTALL_MOD_PATH}/target/modules_install.sh

###### done #####
echo ""
echo "Done! ${INSTALL_MOD_PATH}/target/: "`wc -l ${INSTALL_MOD_PATH}/target/modules.dep | awk '{print $1}'`" modules."
