

obj-$(CONFIG_AML_NEXT_GEN_NAND)	+= amlnf_api.o
amlnf_api-objs =
amlnf_api-objs += amlnf_ctrl.o
amlnf_api-objs += amlnf_dev.o
amlnf_api-objs += amlnf_env.o
amlnf_api-objs += amlnf_dtb.o
amlnf_api-objs += phydev.o


#ifeq ($(CONFIG_AML_NAND_KEY), y)
amlnf_api-objs  +=nandkey.o
#endif

#ifeq ($(CONFIG_SECURE_NAND), y)
amlnf_api-objs  +=secure_storage.o
#endif


