# SPDX-License-Identifier: (GPL-2.0+ OR MIT)
#
# Copyright (c) 2019 Amlogic, Inc. All rights reserved.
#

export CROSS_COMPILE=/opt/gcc-arm-8.3-2019.03-x86_64-aarch64-linux-gnu/bin/aarch64-linux-gnu-
#ARCH=arm64 ./scripts/kconfig/merge_config.sh arch/arm64/configs/gki_defconfig arch/arm64/configs/meson64_gki.fragment
export INSTALL_MOD_PATH=./modules_install
ARCH=arm64 ./scripts/kconfig/merge_config.sh arch/arm64/configs/gki_defconfig arch/arm64/configs/meson64_gki.fragment
make ARCH=arm64 Image -j12
make ARCH=arm64 dtbs -j12
make ARCH=arm64 modules -j12


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

GKI_EXT_MODULE_CONFIG=$(read_ext_module_config $GKI_EXT_MODULE_CFG)

export GKI_EXT_MODULE_CONFIG

function read_ext_module_predefine() {
	PRE_DEFINE=""
	while read LINE
	do
		if [[ $LINE != \#* &&  $LINE != "" ]]; then
			TMP_CFG=${LINE%=*}
			PRE_DEFINE="$PRE_DEFINE"" -D""$TMP_CFG"
		fi
	done < $1
	echo $PRE_DEFINE
}

GKI_EXT_MODULE_PREDEFINE=$(read_ext_module_predefine $GKI_EXT_MODULE_CFG)

export GKI_EXT_MODULE_PREDEFINE

# ----------- basic common modules build as extern --------------
EXT_MODULES="drivers/amlogic
sound/soc/amlogic
sound/soc/codecs/amlogic
"

if [[ -z "${SKIP_EXT_MODULES}" ]] && [[ -n "${EXT_MODULES}" ]]; then
  echo "========================================================"
  echo " Building external modules"

  for EXT_MOD in ${EXT_MODULES}; do
    set -x
    make -C . M=${EXT_MOD} ARCH=arm64 ${GKI_EXT_MODULE_CONFIG} -j
    make -C . M=${EXT_MOD} ARCH=arm64 ${GKI_EXT_MODULE_CONFIG} modules_install -j
    set +x
  done

fi

rm -rfv ${INSTALL_MOD_PATH}
mkdir -p ${INSTALL_MOD_PATH}/target
make modules_install
find ${INSTALL_MOD_PATH}/lib -name *.ko -exec cp {} ${INSTALL_MOD_PATH}/target/ \;
find ${INSTALL_MOD_PATH}/lib -name modules.dep -exec cp {} ${INSTALL_MOD_PATH}/target/ \;
cp scripts/amlogic/module_install.sh  ${INSTALL_MOD_PATH}/target/
