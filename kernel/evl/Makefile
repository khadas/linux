obj-$(CONFIG_EVL) += evl.o sched/

ccflags-y += -Ikernel

evl-y :=		\
	clock.o		\
	control.o	\
	factory.o	\
	file.o		\
	init.o		\
	lock.o		\
	memory.o	\
	monitor.o	\
	mutex.o		\
	poll.o		\
	proxy.o		\
	sem.o		\
	syscall.o	\
	thread.o	\
	tick.o		\
	timer.o		\
	wait.o		\
	xbuf.o

evl-$(CONFIG_FTRACE) +=	trace.o
