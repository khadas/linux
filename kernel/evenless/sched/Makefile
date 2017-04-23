obj-$(CONFIG_EVENLESS) += evenless.o

ccflags-y += -Ikernel

evenless-y :=	\
	core.o	\
	rt.o	\
	idle.o	\
	weak.o

evenless-$(CONFIG_EVENLESS_SCHED_QUOTA) += quota.o
