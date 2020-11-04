######################################################################################
# attention: this script only for extern module compile, eg: media_modules,tdk,wifi...
######################################################################################

if [ $# -ne 1 ];then
	echo "Use format: $0 extern_module_path"
	echo "        eg: $0 drivers/amlogic"
	echo "        eg: $0 sound/soc/amlogic"
	echo "        eg: $0 sound/soc/codecs/amlogic"
	echo "        eg: $0 ~/work/android_r/hardware/amlogic/media_modules"
	echo "        eg: $0 /home/username/work/android_r/vendor/amlogic/common/tdk/linuxdriver"
	echo "        eg: $0 /home/username/work/android_r/vendor/wifi_driver/qualcomm/qca6174/AIO/build"
	echo "        eg: $0 /home/username/work/android_r/vendor/amlogic/common/gpu/bifrost"
	exit 0
fi

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


function read_ext_module_predefine()
{
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

export CROSS_COMPILE=aarch64-linux-gnu-
export CLANG_TRIPLE=aarch64-linux-gnu-
export PATH=/opt/clang-r377782b/bin/:$PATH
clang_flags="ARCH=arm64 CC=clang HOSTCC=clang LD=ld.lld NM=llvm-nm OBJCOPY=llvm-objcopy"

KERNEL_SRC=${PWD}
KERNEL_SRC=`readlink -f $KERNEL_SRC`
echo KERNEL_SRC=$KERNEL_SRC

EXT_MODULE=$1
EXT_MODULE=`readlink -f $EXT_MODULE`
echo EXT_MODULE=$EXT_MODULE

M=`echo $KERNEL_SRC | sed 's#/[^/]*#../#g'`$EXT_MODULE

GKI_EXT_MODULE_CONFIG=$(read_ext_module_config $GKI_EXT_MODULE_CFG)
export GKI_EXT_MODULE_CONFIG
GKI_EXT_MODULE_PREDEFINE=$(read_ext_module_predefine $GKI_EXT_MODULE_CFG)
export GKI_EXT_MODULE_PREDEFINE

set -x
make ${clang_flags} -C $EXT_MODULE KERNEL_SRC=$KERNEL_SRC M=$M clean
make ${clang_flags} -C $EXT_MODULE KERNEL_SRC=$KERNEL_SRC M=$M -j12
set +x
