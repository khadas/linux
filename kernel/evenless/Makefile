obj-$(CONFIG_EVENLESS) += evenless.o sched/

ccflags-y += -Ikernel

evenless-y :=		\
	clock.o		\
	control.o	\
	factory.o	\
	file.o		\
	init.o		\
	irq.o		\
	ksem.o		\
	lock.o		\
	logger.o	\
	mapper.o	\
	memory.o	\
	monitor.o	\
	mutex.o		\
	poller.o	\
	sem.o		\
	synch.o		\
	syscall.o	\
	thread.o	\
	tick.o		\
	timer.o		\
	timerfd.o	\
	xbuf.o

evenless-$(CONFIG_FTRACE) +=	trace.o
