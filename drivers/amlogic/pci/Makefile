#
# Amlogic PCIE specific Makefile
#

MODULE_NAME = aml_pcie_v2
obj-$(CONFIG_AMLOGIC_PCIE)	+= $(MODULE_NAME).o
$(MODULE_NAME)-y += pcie-amlogic-v2.o  pcie-amlogic.o
CFLAGS_pcie-amlogic.o  += -I $(srctree)/drivers/pci/controller/dwc

MODULE_NAME_V3 = aml_pcie_v3
obj-$(CONFIG_AMLOGIC_PCIE_V3)	+= $(MODULE_NAME_V3).o
$(MODULE_NAME_V3)-y += pcie-amlogic-v3-comm.o  pcie-amlogic-v3-host.o
$(MODULE_NAME_V3)-y += pcie-amlogic-v3-ep.o
