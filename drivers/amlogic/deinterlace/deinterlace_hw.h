#ifndef _DI_HW_H
#define _DI_HW_H
void di_top_gate_control(bool enable);
void di_pre_gate_control(bool enable);
void di_post_gate_control(bool enable);
void enable_di_pre_mif(bool enable);
void enable_di_post_mif(bool enable);
void di_hw_uninit(void);
#endif
