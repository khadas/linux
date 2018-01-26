#
# Makefile for the Amlogic NAND drivers.
#
KBUILD_CFLAGS += -Werror=declaration-after-statement \
		 -Werror=maybe-uninitialized \
		 -Werror=parentheses \
		 -Werror=return-type \
		 -Werror=strict-prototypes \
		 -Werror=uninitialized \
		 -Werror=unused-function \
		 -Werror=unused-value \
		 -Werror=array-bounds \
		 -Werror=comment \
		 -Werror=format \
		 -Werror=missing-braces \
		 -Werror=declaration-after-statement \
		 -Werror=unused-label \
		 -Werror=unused-result \
		 -Werror=unused-value \
		 -Werror=builtin-macro-redefined

obj-$(CONFIG_AML_NEXT_GEN_NAND)	+= dev/
obj-$(CONFIG_AML_NEXT_GEN_NAND)	+= ntd/
obj-$(CONFIG_AML_NEXT_GEN_NAND)	+= phy/

#CURRENT_DIR = $(shell pwd)

#INCLUDE_DIR_BASE = $(CURRENT_DIR)/../../../../../..

#$(warning "$(INCLUDE_DIR_BASE)")

#EXTRA_CFLAGS += -I$(INCLUDE_DIR_BASE)/common/drivers/amlogic/amlnf/include
#EXTRA_CFLAGS += -I$(INCLUDE_DIR_BASE)/common/drivers/amlogic/amlnf/ntd

#EXTRA_CFLAGS += -I$drivers/amlogic/amlnf/include
#EXTRA_CFLAGS += -I$drivers/amlogic/amlnf/ntd

#obj-$(CONFIG_AML_NFTL_NEW)	+= nftl/

obj-$(CONFIG_AML_NFTL_NEW) += aml_nftl_dev.o
aml_nftl_dev-objs =
aml_nftl_dev-objs += block/aml_nftl_block.o
aml_nftl_dev-objs += block/aml_nftl_init.o
aml_nftl_dev-objs += block/aml_nftl_hw_interface.o
aml_nftl_dev-objs += nftl/aml_nftl_core_20160602.o_shipped
