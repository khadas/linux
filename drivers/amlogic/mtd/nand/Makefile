#
# Makefile for AMLOGIC MTD nfc for mtd subsystem
#

MODULE_NAME = meson_mtd_nfc
obj-$(CONFIG_MTD_NAND_MESON) = $(MODULE_NAME).o
$(MODULE_NAME)-$(CONFIG_MTD_NAND_MESON) += meson_nand.o meson_parse_part.o nand_flash_id.o nand_bootpart.o
