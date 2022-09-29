export PATH=../prebuilts/clang/host/linux-x86/clang-r416183b/bin:$PATH 
alias msk='make CROSS_COMPILE=aarch64-linux-gnu- LLVM=1 LLVM_IAS=1'
msk ARCH=arm64 kedge2_defconfig android-11.config pcie_wifi.config && msk ARCH=arm64 BOOT_IMG=../rockdev/Image-kedge2/boot.img rk3588s-khadas-edge2.img -j24
date


