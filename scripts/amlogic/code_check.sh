#! /bin/bash
#
#  author: yao.zhang1@amlogic.com
#  2021.05.20

MAIN_FOLDER=`pwd`
CONFIG_COVERITY_ALL="0"
CC64="/opt/gcc-linaro-6.3.1-2017.02-x86_64_aarch64-linux-gnu/bin/aarch64-linux-gnu-"
CC32="/opt/gcc-linaro-6.3.1-2017.02-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-"
CONFIG_GKI_MODULE="arch/arm64/configs/meson64_gki_module_config"
CONFIG_A64_R="arch/arm64/configs/meson64_a64_R_defconfig"

function usage() {
  cat << EOF
  Usage:
    ./$(basename $0) --help

		code check kernel build script.
		This script will coverity the latest code commit or all code coverity locally.

    command list:
        ./$(basename $0)  [CONFIG_COVERITY_ALL]

    Example:
    1) ./scripts/amlogic/code_check.sh --all //kernel 5.4 or kernel 4.9, then coverity all

    2) ./scripts/amlogic/code_check.sh //kernel 5.4 or kernel 4.9, then coverity own local last commit

EOF
  exit 1
}

function build_kernel() {
	#echo "=====build kernel======"
	if [ "$CONFIG_KERNEL_VERSION" = "4.9" ]; then
		if [ "$CONFIG_ARCH" = "arm64" ]; then
			echo "===========CONFIG_ARCH $CONFIG_ARCH ==========="
			make ARCH=arm64 O=../kernel-output distclean
			make ARCH=arm64 CROSS_COMPILE=$CC64 O=../kernel-output meson64_defconfig
			make ARCH=arm64 mrproper
			cov-build --dir ../im-dir/ make ARCH=arm64 CROSS_COMPILE=$CC64 O=../kernel-output Image -j32
		elif [ "$CONFIG_ARCH" = "arm32" ]; then
			echo "===========CONFIG_ARCH $CONFIG_ARCH ==========="
			make ARCH=arm O=../kernel-output distclean
			make ARCH=arm CROSS_COMPILE=$CC32 O=../kernel_output/ meson64_a32_defconfig
			make ARCH=arm mrproper
			cov-build --dir ../im-dir/ make ARCH=arm CROSS_COMPILE=$CC32 O=../kernel_output uImage -j32
		else
			echo "===========CONFIG_ARCH ? ==========="
			usage
		fi
	elif [ "$CONFIG_KERNEL_VERSION" = "5.4" ]; then
		#rm -rf ../im-dir/ ../html-result/ ../kernel-output*
		git checkout scripts/kconfig/merge_config.sh
		make ARCH=arm64 O=../kernel-output distclean
		cat $CONFIG_GKI_MODULE $CONFIG_A64_R > arch/arm64/configs/meson64_gki.fragment.y
		sed -i 's/=m/=y/' arch/arm64/configs/meson64_gki.fragment.y
		sed -i 's/OUTPUT=\./OUTPUT=..\/kernel-output/' ./scripts/kconfig/merge_config.sh
		ARCH=arm64 ./scripts/kconfig/merge_config.sh arch/arm64/configs/meson64_gki.fragment.y
		rm arch/arm64/configs/meson64_gki.fragment.y
		git checkout scripts/kconfig/merge_config.sh
		#git checkout .
		make ARCH=arm64 mrproper
		cov-build --dir ../im-dir/ make ARCH=arm64 CROSS_COMPILE=$CC64 O=../kernel-output Image -j32
	else
		usage
	fi
}

function build() {
	WORKSPACE=`pwd`
	#echo "===========WORKSPACE $WORKSPACE==========="

	#git diff HEAD@{1} --name-only | grep -e '\.c$' -e '\.h$' > file.list
	#git diff HEAD@{0} --name-only | grep -v -e '\.dtsi$' -e '\.dts$' > file.list
	#git diff HEAD~1 HEAD~0 --name-only | grep -e '\.c$' -e '\.h$' > file.list
	git diff HEAD^ --name-only | grep -e '\.c$' -e '\.h$' > file.list

	tupattern="0"
	firstline="0"
	while read line
	do
		if [ "$firstline" = "0" ]; then
			filelist="$line"
			tupattern="file('/$line')"
			firstline="1"
		else
			filelist="$line|$filelist"
			tupattern="file('/$line')||$tupattern"
		fi
	done < file.list

	echo "===========filelist $filelist==========="
	result1=`echo $filelist |grep "arch/arm/"`
	result2=`echo $filelist |grep "arch/arm64/"`
	if [ "$result1" != "" ]&&[ "$result2" != "" ]; then
		CONFIG_ARCH="arm64"
	elif [ "$result1" != "" ]; then
		CONFIG_ARCH="arm32"
	else
		CONFIG_ARCH="arm64"
	fi

	sleep 1

	build_kernel

	sleep 1

	if [ "$CONFIG_COVERITY_ALL" = "1" ]; then
		echo "===========CONFIG_COVERITY_ALL==========="
		cov-analyze --dir ../im-dir/ --strip-path $WORKSPACE/ --all
		sleep 1
		cov-format-errors --dir ../im-dir/ --html-output ../html-result --filesort --strip-path $WORKSPACE/ -x
	else
		#echo "===========filelist ${filelist}"
		#echo "===========tupattern ${tupattern}"
		if [ "$filelist" = "" ]; then
			echo "===========only coverity .c and .h !!!================="
		fi
		cov-analyze --dir ../im-dir/ --strip-path $WORKSPACE/ --tu-pattern "${tupattern}" --all
		sleep 1
		cov-format-errors --dir ../im-dir/ --include-files "${filelist}" --html-output ../html-result
	fi
}

function parser() {
	local i=0
	local j=0
	local argv=()
	for arg in "$@" ; do
		argv[$i]="$arg"
		i=$((i + 1))
	done
	i=0
	j=0
	while [ $i -lt $# ]; do
		arg="${argv[$i]}"
		i=$((i + 1)) # must place here
		case "$arg" in
			-h|--help|help)
				usage
				exit ;;
			#-a)
			#	CONFIG_ARCH="${argv[$i]}"
			#	echo "CONFIG_ARCH: ${CONFIG_ARCH}"
			#	continue ;;
			#-v)
			#	CONFIG_KERNEL_VERSION="${argv[$i]}"
			#	echo "CONFIG_KERNEL_VERSION: ${CONFIG_KERNEL_VERSION}"
			#	continue ;;
			--all)
				CONFIG_COVERITY_ALL="1"
				continue ;;
			*)
		esac
	done

	CONFIG_ARCH="arm64"
	cat Makefile |grep "VERSION " > version_cat
	cat Makefile |grep "PATCHLEVEL " > patchlevel_cat
	VERSION=`sed -n '1p' version_cat | cut -d ' ' -f 3`
	PATCHLEVEL=`sed -n '1p' patchlevel_cat | cut -d ' ' -f 3`
	rm version_cat patchlevel_cat
	CONFIG_KERNEL_VERSION=$VERSION.$PATCHLEVEL

	echo "===========CONFIG_KERNEL_VERSION $CONFIG_KERNEL_VERSION==========="
	#if [ "$j" == "0" ]; then
		#usage
		#exit
	#fi
}

function main() {
	if [ -z $0 ]
	then
		usage
		return
	fi
	parser $@
	build $@
}

main $@
