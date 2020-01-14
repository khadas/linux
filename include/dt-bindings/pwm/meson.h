/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _DT_BINDINGS_PWM_MESON_H
#define _DT_BINDINGS_PWM_MESON_H

/*defination for meson pwm channel index
 *	for example:
 *	1.there are four pwm controllers for axg:
 *	pwm A/B ,pwm C/D, pwm AOA/AOB, pwm AOC/AOD.
 *	each controller has four pwm channels:
 *	MESON_PWM_0,MESON_PWM_1,MESON_PWM_2,MESON_PWM_3
 *	when double pwm channels used, pwm channel
 *	[ MESON_PWM_0 and MESON_PWM_2 ],
 *	[ MESON_PWM_1 and MESON_PWM_3 ],
 *	should be used together.
 *
 *	2.there are two three pwm controllers for m8b:
 *	pwm A/B,pwm C/D,pwm E/F.
 *	each controllere has two pwm channels:
 *	MESON_PWM_0 and MESON_PWM_1.
 */
#define		MESON_PWM_0  0
#define		MESON_PWM_1  1
#define		MESON_PWM_2  2
#define		MESON_PWM_3  3

#endif
