TARGET = cimax+usb_driver
OBJS = cimax+usb_driver.o
MDIR = drivers/misc

ccflags-y = -DEXPORT_SYMTAB
#ccflags-y = -DEXPORT_SYMTAB -DDEBUG
CURRENT = $(shell uname -r)
KDIR = /lib/modules/$(CURRENT)/build
PWD = $(shell pwd)
DEST = /lib/modules/$(CURRENT)/kernel/$(MDIR)

obj-m := $(TARGET).o
cimax+usb_driver-y :=  cimax+usb-driver.o cimax+usb_fw.o
cimax+usb_driver-y +=  cimax+usb_config.o
cimax+usb_driver-y +=  cimax+usb_time.o

default:
	make -C /usr/src/linux SUBDIRS=$(PWD) modules
#	make -C /lib/modules/`uname -r`/build M=$(PWD) modules

$(TARGET).o: $(OBJS)
	$(LD) $(LD_RFLAG) -r -o $@ $(OBJS)

ifneq (,$(findstring 2.4.,$(CURRENT)))
install:
	su -c "cp -v $(TARGET).o $(DEST) && /sbin/depmod -a"
else
install:
	su -c "cp -v $(TARGET).ko $(DEST) && /sbin/depmod -a"
endif

clean:
	-rm -f *.o *.ko .*.cmd .*.flags *.mod.c

-include $(KDIR)/Rules.make
