obj-$(CONFIG_EVL) += evenless.o

ccflags-y += -Ikernel

evenless-y :=	\
	core.o	\
	rt.o	\
	idle.o	\
	weak.o

evenless-$(CONFIG_EVL_SCHED_QUOTA) += quota.o
