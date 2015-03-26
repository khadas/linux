#ifndef __AML_BL_REG_H__
#define __AML_BL_REG_H__
#include <linux/amlogic/iomap.h>
#include <linux/amlogic/vout/aml_tablet_bl.h>
#include <linux/amlogic/vout/aml_bl.h>



#define PWM_PWM_A			0x2154
#define PWM_PWM_B			0x2155
#define PWM_MISC_REG_AB			0x2156
#define PWM_PWM_C			0x2194
#define PWM_PWM_D			0x2195
#define PWM_MISC_REG_CD			0x2196
#define PWM_PWM_E			0x21b0
#define PWM_PWM_F			0x21b1
#define PWM_MISC_REG_EF			0x21b2
#define LED_PWM_REG0			0x21da


static inline void bl_write_reg(unsigned int reg, unsigned int value)
{
		aml_write_cbus(reg, value);
};

static inline void bl_reg_setb(unsigned int reg, unsigned int value,
		unsigned int _start, unsigned int _len)
{
	aml_write_cbus(reg, ((aml_read_cbus(reg) &
				(~(((1L << _len)-1) << _start))) |
				((value & ((1L << _len)-1)) << _start)));

}



#endif

