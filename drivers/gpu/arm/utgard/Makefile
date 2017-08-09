$(warning src is $(src))
$(warning MAKEFILE_LIST is $(MAKEFILE_LIST))

GPU_DRV_VERSION?=r7p0

KDIR?=/home/jiyu.yang/source/br-wayland/output/meson8b_m201/build/linux/
ARCH?=arm
CROSS_COMPILE?=arm-linux-gnueabihf-
MALI400?=m
EXTRA_CFLAGS+="-DCONFIG_MALI400=m"
UTGARD_MAKEFILE:=$(shell pwd)/$(lastword $(MAKEFILE_LIST))
UTGARD_DIR=$(dir $(UTGARD_MAKEFILE))

.ONESHELL:
gpu:
	echo "utgard makefile dir is $(UTGARD_MAKEFILE)"; \
	echo "utgard dir is $(UTGARD_DIR)"; \
	echo "gpu build"; \
	echo "MAKE is $(MAKE)"; \
	echo "MAKE1 is $(MAKE1)"; \
	if [ ! -d $(GPU_DRV_VERSION) ]; then \
		ln -s $(UTGARD_DIR)../mali $(GPU_DRV_VERSION); \
	fi ;\
	cd $(GPU_DRV_VERSION); \
	if [ ! -d platform ]; then \
		ln -s $(UTGARD_DIR)platform platform; \
	fi ;\
	$(MAKE) -C $(KDIR) M=`pwd` CONFIG_MALI400=$(MALI400) modules

clean:
	echo "clean this module"
	cd $(UTGARD_DIR)
	-cd $(GPU_DRV_VERSION)
	find . -name "*.o" -o -name "*.cmd" -o -name "*~" | xargs rm -rf
	find . -name "Module.symvers" -o -name "*.ko" -o -name "*.mod.c" \
	-o -name "modules.order" -o -name "*pyc" -o -name __malidrv_build_info.c | xargs rm -f
	-cd $(UTGARD_DIR)
	find . -name "*.o" -o -name "*.cmd" -o -name "*~" | xargs rm -rf
	find . -name "Module.symvers" -o -name "*.ko" -o -name "*.mod.c" \
	-o -name "modules.order" -o -name "*pyc" -o -name __malidrv_build_info.c | xargs rm -f
