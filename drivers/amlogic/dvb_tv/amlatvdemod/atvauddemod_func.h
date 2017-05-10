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
void set_nicam_outputmode(uint32_t outmode);
void set_a2_eiaj_outputmode(uint32_t outmode);
void set_btsc_outputmode(uint32_t outmode);
void update_nicam_mode(int *nicam_flag, int *nicam_mono_flag,
		int *nicam_stereo_flag, int *nicam_dual_flag);
void update_btsc_mode(int auto_en, int *stereo_flag, int *sap_flag);
void update_a2_eiaj_mode(int auto_en, int *stereo_flag, int *dual_flag);

#endif /* __ATVAUDDEMOD_H_ */
