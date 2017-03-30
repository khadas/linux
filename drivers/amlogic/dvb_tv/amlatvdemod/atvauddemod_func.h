/* atvauddemod_func.h */
#ifndef __ATVAUDDEMOD_H_
#define __ATVAUDDEMOD_H_

#include "aud_demod_reg.h"

extern struct amlatvdemod_device_s *amlatvdemod_devp;
extern int atvaudiodem_reg_read(unsigned int reg, unsigned int *val);
extern int atvaudiodem_reg_write(unsigned int reg, unsigned int val);
extern uint32_t adec_rd_reg(uint32_t addr);

void set_outputmode(uint32_t standard, uint32_t outmode);
void aud_demod_clk_gate(int on);
void configure_adec(int Audio_mode);
void adec_soft_reset(void);

#endif /* __ATVAUDDEMOD_H_ */
