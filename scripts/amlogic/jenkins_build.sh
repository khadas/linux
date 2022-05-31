#!/bin/bash
# SPDX-License-Identifier: (GPL-2.0+ OR MIT)
#
# Copyright (c) 2019 Amlogic, Inc. All rights reserved.
#

function cmd()
{
	$1
	if [ "$?" != 0 ];then
		err_exit "failed: $1"
#	else
#		echo "ok: $1"
	fi
}

function build_kernel_arm64_clean()
{
	cmd "make ${ARM64_KCONST} distclean"
}

function build_kernel_arm_clean()
{
	cmd "make ${ARM_KCONST} distclean"
}

function build_kernel_arm64_prepare()
{
	echo "===> build kernel arm64 prepare"
	#build the Kernel image to output dir
	build_kernel_arm64_clean

	cat arch/arm64/configs/meson64_gki_module_config arch/arm64/configs/meson64_a64_R_defconfig > arch/arm64/configs/meson64_gki.fragment.y
	sed -i 's/=m/=y/' arch/arm64/configs/meson64_gki.fragment.y
	sed -i 's/OUTPUT=\./OUTPUT=${WORKSPACE}/' ./scripts/kconfig/merge_config.sh
	ARCH=arm64 ./scripts/kconfig/merge_config.sh arch/arm64/configs/meson64_gki.fragment.y
	rm -fr arch/arm64/configs/meson64_gki.fragment.y

	git checkout .
	cmd "make ARCH=arm64 mrproper"
}

function build_kernel_arm64_config()
{
	echo "===> build kernel arm64 config"
	if [ $KERNEL_VERSION = "5.4" ];then
		build_kernel_arm64_prepare

		if [ "$SUBMIT_TYPE" != "merge" -a "$COVERITY_STREAM_64" != "" ]; then
			cmd "cov-build --dir ${KERNEL_COVERITY_64_PATH} --add-arg --no_use_stage_emit make ${ARM64_KCONST} UIMAGE_LOADADDR=0x108000 Image -j16"
#			cov-build --dir ${KERNEL_COVERITY_64_PATH} --add-arg --no_use_stage_emit make ${ARM64_KCONST} UIMAGE_LOADADDR=0x108000 Image -j16 || err_exit "coverity: make Image failed"
		else
			cmd "make ${ARM64_KCONST} UIMAGE_LOADADDR=0x108000 Image -j16"
		fi
	else
		err_exit "Only kernel 5.4 was supported now"
	fi
}

function build_kernel_arm64_with_clang()
{
        export BUILD_FOR_AUTOSH=true

	#build mk_p_builtin with clang
	cmd "./scripts/amlogic/mk_p_builtin.sh"
	cmd "make ARCH=arm64 distclean"

	#build mk_r_gki with clang
	cmd "./scripts/amlogic/mk_r_gki.sh"
	cmd "make ARCH=arm64 distclean"

	#build meson64_a64_R_defconfig + diff_config with clang
	export PATH=/opt/clang-r377782b/bin/:$PATH
	clang_flags="ARCH=arm64 LLVM=1 CC=clang HOSTCC=clang LD=ld.lld NM=llvm-nm OBJCOPY=llvm-objcopy CLANG_TRIPLE=aarch64-linux-gnu- CROSS_COMPILE=aarch64-linux-gnu-"
	cat arch/arm64/configs/meson64_gki_module_config arch/arm64/configs/meson64_a64_R_defconfig scripts/amlogic/meson64_a64_r_user_diffconfig > arch/arm64/configs/meson64_gki_r_diff_defconfig
	cmd "make ${clang_flags} meson64_gki_r_diff_defconfig"
	rm arch/arm64/configs/meson64_gki_r_diff_defconfig
	cmd "make ${clang_flags} Image dtbs modules -j12"
	cmd "make ARCH=arm64 distclean"
}

function build_kernel_arm_config()
{
	echo "===> build kernel arm config"
	config=$1

	echo "---- build ${config} ----"
	#build the Kernel image to output dir
	build_kernel_arm_clean
	cmd "make ${ARM_KCONST} ${config}"
	if [ "$config" = "meson64_a32_defconfig" -a "$SUBMIT_TYPE" != "merge" -a "$COVERITY_STREAM_32" != "" ]; then
		cmd "cov-build --dir ${KERNEL_COVERITY_32_PATH} --add-arg --no_use_stage_emit make ${ARM_KCONST} UIMAGE_LOADADDR=0x12000000 uImage -j16"
	else
		cmd "make ${ARM_KCONST} UIMAGE_LOADADDR=0x12000000 uImage -j16"
	fi
}

function build_arm64_kernels()
{
	echo "===> build arm64 kernels"
	if [ $KERNEL_VERSION = "5.4" ];then
		build_kernel_arm64_config
	else
		err_exit "build_arm64_kernels:  Only kernel 5.4 was supported now"
	fi
}

function build_arm_kernels()
{
	echo "===> build arm kernels"
	#build special configs
	for MESON_CONFIG in ${ARM_CONFIG_ALL}
	do
		build_kernel_arm_config "meson32_"${MESON_CONFIG}"_defconfig"
	done

	#build defconfig
	build_kernel_arm_config meson32_defconfig
	ARM_DEPLOY_LIST="$ARM_DEPLOY_LIST"" ""uImage"
}


function build_arm64_32_kernels()
{
	echo "===> build arm64_32 kernels"
	build_kernel_arm_config meson64_a32_defconfig
}

function build_arm64_dtb()
{
	cmd "make ${ARM64_KCONST} $1"
}

function build_arm64_dtbs()
{
	echo "===> build arm64 dtbs"

	for ONE_DTB_FILE in ${ARM64_DTB_ALL}
	do
		build_arm64_dtb ${ONE_DTB_FILE}.dtb
		ARM64_DEPLOY_LIST="$ARM64_DEPLOY_LIST"" ""${ONE_DTB_FILE}.dtb"
	done
}

function build_arm_dtb()
{
	cmd "make ${ARM_KCONST} $1"
}

function build_arm_dtbs()
{
	echo "===> build arm dtbs"

	for ONE_DTB_FILE in ${ARM_DTB_ALL}
	do
		build_arm_dtb ${ONE_DTB_FILE}.dtb
		ARM_DEPLOY_LIST="$ARM_DEPLOY_LIST"" ""${ONE_DTB_FILE}.dtb"
	done
}

function build_arm64_internal_module()
{
	echo "===> build arm64 internal module"

	rm -rf ${MODULE_INSTALL_PATH}/lib/modules/*
	cmd "make ${ARM64_KCONST} modules O=${WORKSPACE} -j16"
	cmd "make ${ARM64_KCONST} INSTALL_MOD_PATH=${MODULE_INSTALL_PATH} modules_install"

	rm -f ${MODULE_INSTALL_PATH}/lib/modules/*/build
	rm -f ${MODULE_INSTALL_PATH}/lib/modules/*/source
	ARM64_DEPLOY_LIST="$ARM64_DEPLOY_LIST"" ""modules.tar.gz"
}

function build_arm_internal_module()
{
	echo "===> build arm internal module"

	rm -rf ${MODULE_INSTALL_PATH}/lib/modules/*
	cmd "make ${ARM_KCONST} modules O=${WORKSPACE} -j16"
	cmd "make ${ARM_KCONST} INSTALL_MOD_PATH=${MODULE_INSTALL_PATH} modules_install"
	rm -f ${MODULE_INSTALL_PATH}/lib/modules/*/build
	rm -f ${MODULE_INSTALL_PATH}/lib/modules/*/source
	ARM_DEPLOY_LIST="$ARM_DEPLOY_LIST"" ""modules.tar.gz"
}

function build_arm64_external_module()
{
	echo "===> build arm64 external module"

	# start build broadcom wifi ap6xxx
	AP6XXX_PATH=${MODULE_SOURCE_PATH}/ap6xxx
	git -C ${AP6XXX_PATH} pull
	make ${ARM64_MC} -C ${WORKSPACE} M=${AP6XXX_PATH}/bcmdhd_1_201_59_x KCPPFLAGS='-DCONFIG_BCMDHD_FW_PATH=\"/etc/wifi/fw_bcmdhd.bin\" -DCONFIG_BCMDHD_NVRAM_PATH=\"/etc/wifi/nvram.txt\"' modules || err_exit "make ap6xxx failed"
	make ${ARM64_MC} -C ${WORKSPACE} M=${AP6XXX_PATH}/bcmdhd-usb.1.201.88.27.x KCPPFLAGS='-DCONFIG_BCMDHD_FW_PATH=\"/etc/wifi/fw_bcmdhd.bin\" -DCONFIG_BCMDHD_NVRAM_PATH=\"/etc/wifi/nvram.txt\"' modules || err_exit "make ap6xxx failed"
	make ${ARM64_MC} -C ${WORKSPACE} M=${AP6XXX_PATH}/bcmdhd_1_201_59_x INSTALL_MOD_PATH=${MODULE_INSTALL_PATH} modules_install
	make ${ARM64_MC} -C ${WORKSPACE} M=${AP6XXX_PATH}/bcmdhd-usb.1.201.88.27.x INSTALL_MOD_PATH=${MODULE_INSTALL_PATH} modules_install

	# start build realtek wifi 8189es
	RTK8189ES_PATH=${MODULE_SOURCE_PATH}/8189es
	git -C ${RTK8189ES_PATH} pull
	make ${ARM64_MC} -C ${WORKSPACE} M=${RTK8189ES_PATH}/rtl8189ES modules || err_exit "make 8189es failed"
	make ${ARM64_MC} -C ${WORKSPACE} M=${RTK8189ES_PATH}/rtl8189ES INSTALL_MOD_PATH=${MODULE_INSTALL_PATH} modules_install

	# start build gpu
	#GPU_PATH=${MODULE_SOURCE_PATH}/gpu
	#git -C ${GPU_PATH} pull
	#make ${ARM64_MC} -C ${WORKSPACE} M=${GPU_PATH}/mali CONFIG_MALI400=m CONFIG_MALI450=m modules || err_exit "make mali failed"
	#make ${ARM64_MC} -C ${WORKSPACE} M=${GPU_PATH}/mali INSTALL_MOD_PATH=${MODULE_INSTALL_PATH} modules_install
}

case $1 in
	arm64_external_module)
		build_arm64_external_module
	;;
	arm_internal_module)
		build_arm_internal_module
	;;
	arm64_internal_module)
		build_arm64_internal_module
	;;
	arm_dtbs)
		build_arm_dtbs
	;;
	arm_dtb)
		build_arm_dtb $2
	;;
	arm64_dtbs)
		build_arm64_dtbs
	;;
	arm64_dtb)
		build_arm64_dtb $2
	;;
	arm64_32_kernels)
		build_arm64_32_kernels
	;;
	arm_kernels)
		build_arm_kernels
	;;
	arm64_kernels)
		build_arm64_kernels
	;;
	kernel_arm_config)
		build_kernel_arm_config $2
	;;
	kernel_arm64_gki_config)
		build_kernel_arm64_with_clang
	;;
	kernel_arm64_prepare)
		build_kernel_arm64_prepare
	;;
	kernel_arm64_config)
		build_kernel_arm64_config $2
	;;
	kernel_arm_clean)
		build_kernel_arm_clean
	;;
	kernel_arm64_clean)
		build_kernel_arm64_clean
	;;
	*)
	echo "err parameter"
	;;
esac
