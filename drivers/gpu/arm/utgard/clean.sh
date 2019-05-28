#!/bin/sh
# rm *.o, *.cmd, *~
find . -name "*.o" -o -name "*.cmd" -o -name "*~" | xargs rm -rf
rm	Module.symvers
rm	mali.ko
rm	mali.mod.c
rm	modules.order
