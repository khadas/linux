#ifndef _DI_HW_H
#define _DI_HW_H
enum gate_mode_e {
	GATE_AUTO,
	GATE_ON,
	GATE_OFF,
};
void di_top_gate_control(bool enable);
void di_pre_gate_control(bool enable);
void di_post_gate_control(bool enable);
void enable_di_pre_mif(bool enable);
void enable_di_post_mif(enum gate_mode_e mode);
void di_hw_uninit(void);
void init_field_mode(unsigned short height);
#endif
