#
# Makefile for eFuse and unifykey
#
MODULE_NAME = aml_efuse_unifykey

obj-$(CONFIG_AMLOGIC_EFUSE_UNIFYKEY) = $(MODULE_NAME).o

$(MODULE_NAME)-y += efuse_unifykey.o
$(MODULE_NAME)-$(CONFIG_AMLOGIC_EFUSE) += efuse.o efuse_api.o
$(MODULE_NAME)-$(CONFIG_AMLOGIC_EFUSE_BURN) +=  efuse_burn.o
$(MODULE_NAME)-$(CONFIG_AMLOGIC_UNIFYKEY) += security_key.o unifykey.o unifykey_dts.o storage_key.o normal_key.o
