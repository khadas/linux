
obj-m += imx290.o

M_PATH := $(shell dirname $(lastword $(MAKEFILE_LIST)))

DRV_SRC := $(wildcard $(M_PATH)/*.c)

M_DRV_SRC := $(DRV_SRC:$(M_PATH)/%=%)

DRV_SRC_OBJ := $(M_DRV_SRC:.c=.o)

imx290-objs := $(DRV_SRC_OBJ)
