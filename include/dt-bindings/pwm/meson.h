/*
 * This header provides constants for amlogic PWM bindings.
 *
 * Most PWM bindings can include a flags cell as part of the PWM specifier.
 * In most cases, the format of the flags cell uses the standard values
 * defined in this header.
 */

#ifndef _DT_BINDINGS_PWM_MESON_H
#define _DT_BINDINGS_PWM_MESON_H


#define	PWM_A			0
#define	PWM_B			1
#define	PWM_C			2
#define	PWM_D			3
#define	PWM_E			4
#define	PWM_F			5
#define	PWM_AO_A		6
#define	PWM_AO_B		7

/*
 * Addtional 8 channels for gxtvbb , gxl ,gxm and txl
 */
#define	PWM_A2			8
#define	PWM_B2			9
#define	PWM_C2			10
#define	PWM_D2			11
#define	PWM_E2			12
#define	PWM_F2			13
#define	PWM_AO_A2		14
#define	PWM_AO_B2		15


/*
 * 4 clock sources to choose
 * keep the same order with pwm_aml_clk function in pwm driver
 */
#define	XTAL_CLK			0
#define	VID_PLL_CLK			1
#define	FCLK_DIV4_CLK		2
#define	FCLK_DIV3_CLK		3

#endif
