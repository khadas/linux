#! /bin/bash
#
#  author: yao.zhang1@amlogic.com
#  2021.05.20

MAIN_FOLDER=`pwd`
CONFIG_COVERITY_ALL="0"
CC64="/opt/gcc-linaro-6.3.1-2017.02-x86_64_aarch64-linux-gnu/bin/aarch64-linux-gnu-"
CC32="/opt/gcc-linaro-6.3.1-2017.02-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-"
CC64_5_4="/opt/gcc-arm-8.3-2019.03-x86_64-aarch64-linux-gnu/bin/aarch64-linux-gnu-"
CONFIG_GKI_MODULE="arch/arm64/configs/meson64_gki_module_config"
CONFIG_A64_R="arch/arm64/configs/meson64_a64_R_defconfig"

function usage() {
  cat << EOF
  Usage:
    ./$(basename $0) --help

		code check kernel build script.
		This script will coverity the latest code commit or all code coverity locally.

		In the common\html-result directory, you can open the index.html and summary.html files through a browser and view code defects

    command list:
        ./$(basename $0)  [CONFIG_COVERITY_ALL]

    Example:
    1) ./scripts/amlogic/code_check.sh --all //kernel 5.4 or kernel 4.9, then coverity all

    2) ./scripts/amlogic/code_check.sh //kernel 5.4 or kernel 4.9, then coverity own local last commit

    3) ./scripts/amlogic/code_check.sh drivers/amlogic/power //kernel 5.4 or kernel 4.9, Scan code in a directory

    4) ./scripts/amlogic/code_check.sh -s STREAM  -k /mnt/fileroot/user.key -p	//Submit result to coverity server using user.key and STREAM

EOF
  exit 1
}

function build_kernel() {
	#echo "=====build kernel======"
	if [ "$CONFIG_KERNEL_VERSION" = "4.9" ]; then
		if [ "$CONFIG_ARCH" = "arm64" ]; then
			echo "===========CONFIG_ARCH $CONFIG_ARCH ==========="
			make ARCH=arm64 clean
			make ARCH=arm64 mrproper
			make ARCH=arm64 O=./kernel-output clean
			make ARCH=arm64 O=./kernel-output mrproper
			make ARCH=arm64 CROSS_COMPILE=$CC64 O=./kernel-output meson64_defconfig
			cov-build --dir ./im-dir/ make ARCH=arm64 CROSS_COMPILE=$CC64 O=./kernel-output Image -j32
		elif [ "$CONFIG_ARCH" = "arm32" ]; then
			echo "===========CONFIG_ARCH $CONFIG_ARCH ==========="
			make ARCH=arm clean
			make ARCH=arm mrproper
			make ARCH=arm O=./kernel-output clean
			make ARCH=arm O=./kernel-output mrproper
			make ARCH=arm CROSS_COMPILE=$CC32 O=../kernel_output/ meson64_a32_defconfig
			#make ARCH=arm mrproper
			cov-build --dir ./im-dir/ make ARCH=arm CROSS_COMPILE=$CC32 O=kernel_output uImage -j32
		else
			echo "===========CONFIG_ARCH ? ==========="
			usage
		fi
	elif [ "$CONFIG_KERNEL_VERSION" = "5.4" ]; then
		#rm -rf ./im-dir/ ./html-result/ ./kernel-output*
		make ARCH=arm64 mrproper
		make ARCH=arm64 clean
		git checkout scripts/kconfig/merge_config.sh
		make ARCH=arm64 O=./kernel-output mrproper
		make ARCH=arm64 O=./kernel-output clean
		cat $CONFIG_GKI_MODULE $CONFIG_A64_R > arch/arm64/configs/cover_defconfig
		sed -i 's/=m/=y/' arch/arm64/configs/cover_defconfig
		sed -i 's/OUTPUT=\./OUTPUT=.\/kernel-output/' ./scripts/kconfig/merge_config.sh
		make ARCH=arm64 CROSS_COMPILE=$CC64_5_4 O=./kernel-output cover_defconfig
		rm arch/arm64/configs/cover_defconfig
		git checkout scripts/kconfig/merge_config.sh
		#git checkout .
		cov-build --dir ./im-dir/ make ARCH=arm64 CROSS_COMPILE=$CC64_5_4 O=./kernel-output Image -j32
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

	FILE_LIST="--include-files \"${filelist}\""

	if [ -n "$1" ]; then
		if [ -d "$WORKSPACE/$1" ]; then
			echo "scan path: $1"
			tupattern="file(\"$1\")"
			FILE_LIST=""
			filelist=""
		fi
	fi

	echo "===========filelist $filelist==========="
	#echo "===========FILE_LIST $FILE_LIST==========="
	result1=`echo $tupattern |grep "arch/arm/"`
	result2=`echo $tupattern |grep "arch/arm64/"`
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
		cov-analyze --dir ./im-dir/ --strip-path $WORKSPACE/ --all
		sleep 1
		cov-format-errors --dir ./im-dir/ --html-output ./html-result --filesort --strip-path $WORKSPACE/ -x
	elif [ "$COVERITY_SUBMIT" = "1" ]; then
		cov-import-scm --scm git --dir ./im-dir/
		cov-analyze --dir ./im-dir/ --strip-path $WORKSPACE/ --all  --disable-parse-warnings
		sleep 1
		cov-commit-defects --dir ./im-dir/ --auth-key-file $COVERITY_KEY --host 10.18.11.122 --stream $STREAM  --noxrefs --on-new-cert trust
	else
		#echo "===========filelist ${filelist}"
		echo "===========tupattern ${tupattern}"
		if [ "$filelist" = "" ]; then
			echo "===========only coverity .c and .h !!!================="
		fi
		cov-analyze --dir ./im-dir/ --strip-path $WORKSPACE/ --tu-pattern "${tupattern}" --all
		#cov-analyze --dir ./im-dir/ --strip-path $WORKSPACE/ --tu-pattern "${tupattern}"
		sleep 1
		cov-format-errors --dir ./im-dir/ $FILE_LIST --html-output ./html-result --filesort --strip-path $WORKSPACE/ -x
		#cov-format-errors --dir ./im-dir/ --html-output ./html-result --filesort --strip-path $WORKSPACE/ -x
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
			-k)
				COVERITY_KEY="${argv[$i]}"
				echo "COVERITY_KEY=$COVERITY_KEY"
				continue ;;
			-s)
				STREAM="${argv[$i]}"
				echo "STREAM=$STREAM"
				continue ;;
			-p)
				COVERITY_SUBMIT="1"
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
