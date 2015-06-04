#ifndef CODECIO_REGISTER_MAP_H
#define CODECIO_REGISTER_MAP_H
int codecio_read_cbus(unsigned int reg);
void codecio_write_cbus(unsigned int reg, unsigned int val);
int codecio_read_dosbus(unsigned int reg);
void codecio_write_dosbus(unsigned int reg, unsigned int val);
int codecio_read_hiubus(unsigned int reg);
void codecio_write_hiubus(unsigned int reg, unsigned int val);
int codecio_read_aobus(unsigned int reg);
void codecio_write_aobus(unsigned int reg, unsigned int val);
int codecio_read_vcbus(unsigned int reg);
void codecio_write_vcbus(unsigned int reg, unsigned int val);
int codecio_read_dmcbus(unsigned int reg);
void codecio_write_dmcbus(unsigned int reg, unsigned int val);
#endif
