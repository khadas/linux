#ifndef AMPORT_GATE_H
#define AMPORT_GATE_H
#include <linux/device.h>

extern int amports_clock_gate_init(struct device *dev);
extern int amports_switch_gate(const char *name, int enable);

#endif

