/*
 * Amlogic iw7019 Driver for LCD Panel Backlight
 *
 * Author:
 *
 * Copyright (C) 2015 Amlogic Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __IW7019_LPF_H
#define __IW7019_LPF_H

unsigned short get_luma_hist(int win);
unsigned short get_bri_final(int win);

void set_lpf_speed(unsigned int speed);
void set_user_limit(unsigned int min, unsigned int max);
void set_user_dimrate(unsigned int dimming);
void set_luma_hist(unsigned short luma[16]);
void set_bri_for_channels(unsigned short bri[16]);

void lpf_init(void);
void lpf_work(void);
void print_map_lookup(void);
void print_luma_hist(void);
void print_bri_target(void);
void print_bri_current(void);
void print_bri_final(void);

#endif /* __IW7019_LPF_H */

