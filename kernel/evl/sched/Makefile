obj-$(CONFIG_EVL) += evl.o

ccflags-y += -Ikernel

evl-y :=	\
	core.o	\
	rt.o	\
	idle.o	\
	weak.o

evl-$(CONFIG_EVL_SCHED_QUOTA) += quota.o
evl-$(CONFIG_EVL_SCHED_TP) += tp.o
