/* SPDX-License-Identifier: GPL-2.0+ */
/*
 *
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
 */

#ifndef __AML_LCD_COMMON_H__
#define __AML_LCD_COMMON_H__
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>
#include "lcd_clk_config.h"

/* 20170505: add a113 support to linux4.9 */
/* 20170905: fix coverity errors */
/* 20180122: support txlx, optimize lcd noitfier event */
/* 20180226: g12a support */
/* 20180425: tvconfig suuport */
/* 20180620: fix coverity errors */
/* 20180626: txl suuport */
/* 20180718: mute: wait vsync for display shadow */
/* 20180827: add pinmux off support */
/* 20180928: tl1 support, optimize clk config */
/* 20181012: tl1 support tcon */
/* 20181212: tl1 update p2p config and pll setting */
/* 20181225: update phy config */
/* 20190108: tl1 support tablet mode */
/* 20190115: tl1 tcon all interface support */
/* 20190225: optimize unifykey read flow to avoid crash */
/* 20190308: add more panel clk_ss_level step for tl1 */
/* 20190520: add vbyone hw filter user define support */
/* 20190911: add lcd_init_level for tl1 */
/* 20191025: tcon chpi phy setting update */
/* 20191115: tcon add demura and vac function  for tl1*/
/* 20191227: vbyone hw filter disable support*/
/* 20200102: support resume type to avoid dual display interfere each other*/
/* 20200611: update tcon pinmux pre-lock*/
/* 20200619: correct P2P CHPI tx low common setting*/
/* 20200827: support T5 TV driver support */
#define LCD_DRV_VERSION    "20200827"

#define VPP_OUT_SATURATE            BIT(0)

extern struct mutex lcd_vout_mutex;
extern unsigned char lcd_resume_flag;
extern int lcd_vout_serve_bypass;

/* lcd common */
void lcd_delay_us(int us);
void lcd_delay_ms(int ms);
int lcd_type_str_to_type(const char *str);
char *lcd_type_type_to_str(int type);
unsigned char lcd_mode_str_to_mode(const char *str);
char *lcd_mode_mode_to_str(int mode);
u8 *lcd_vmap(ulong addr, u32 size);
void lcd_unmap_phyaddr(u8 *vaddr);

void lcd_cpu_gpio_probe(unsigned int index);
void lcd_cpu_gpio_set(unsigned int index, int value);
unsigned int lcd_cpu_gpio_get(unsigned int index);
void lcd_ttl_pinmux_set(int status);
void lcd_vbyone_pinmux_set(int status);
void lcd_mlvds_pinmux_set(int status);
void lcd_p2p_pinmux_set(int status);

int lcd_power_load_from_dts(struct lcd_config_s *pconf,
			    struct device_node *child);
int lcd_power_load_from_unifykey(struct lcd_config_s *pconf,
				 unsigned char *buf, int key_len, int len);
int lcd_vlock_param_load_from_dts(struct lcd_config_s *pconf,
				  struct device_node *child);
int lcd_vlock_param_load_from_unifykey(struct lcd_config_s *pconf,
				       unsigned char *buf);

void lcd_optical_vinfo_update(void);
void lcd_timing_init_config(struct lcd_config_s *pconf);
int lcd_vmode_change(struct lcd_config_s *pconf);
void lcd_clk_change(struct lcd_config_s *pconf);
void lcd_venc_change(struct lcd_config_s *pconf);
void lcd_if_enable_retry(struct lcd_config_s *pconf);
void lcd_vout_notify_mode_change_pre(void);
void lcd_vout_notify_mode_change(void);

/* lcd phy */
void lcd_lvds_phy_set(struct lcd_config_s *pconf, int status);
void lcd_vbyone_phy_set(struct lcd_config_s *pconf, int status);
void lcd_mlvds_phy_set(struct lcd_config_s *pconf, int status);
void lcd_p2p_phy_set(struct lcd_config_s *pconf, int status);
void lcd_mipi_phy_set(struct lcd_config_s *pconf, int status);
int lcd_phy_probe(void);
void lcd_phy_tcon_chpi_bbc_init_tl1(struct lcd_config_s *pconf);

/* lcd tcon */
unsigned int lcd_tcon_reg_read(unsigned int addr);
void lcd_tcon_reg_write(unsigned int addr, unsigned int val);
void lcd_tcon_reg_table_print(void);
void lcd_tcon_reg_readback_print(void);
int lcd_tcon_info_print(char *buf, int offset);
int lcd_tcon_od_set(int flag);
int lcd_tcon_od_get(void);
int lcd_tcon_reg_table_size_get(void);
unsigned char *lcd_tcon_reg_table_get(void);
int lcd_tcon_gamma_set_pattern(unsigned int bit_width, unsigned int gamma_r,
			       unsigned int gamma_g, unsigned int gamma_b);

int lcd_tcon_core_reg_get(unsigned char *buf, unsigned int size);
void lcd_tcon_core_reg_update(void);
int lcd_tcon_enable(struct lcd_config_s *pconf);
void lcd_tcon_disable(void);
int lcd_tcon_probe(struct aml_lcd_drv_s *lcd_drv);

/* lcd debug */
int lcd_debug_info_len(int num);
void lcd_debug_test(unsigned int num);
void lcd_mute_setting(unsigned char flag);
int lcd_debug_probe(void);
int lcd_debug_remove(void);

/* lcd driver */
#ifdef CONFIG_AMLOGIC_LCD_TV
void lcd_tv_vout_server_init(void);
void lcd_tv_vout_server_remove(void);
void lcd_vbyone_interrupt_enable(int flag);
void lcd_tv_clk_config_change(struct lcd_config_s *pconf);
void lcd_tv_clk_update(struct lcd_config_s *pconf);
int lcd_tv_probe(struct device *dev);
int lcd_tv_remove(struct device *dev);
#endif
#ifdef CONFIG_AMLOGIC_LCD_TABLET
int lcd_mipi_test_read(struct dsi_read_s *dread);
void lcd_tablet_vout_server_init(void);
void lcd_tablet_vout_server_remove(void);
void lcd_tablet_clk_config_change(struct lcd_config_s *pconf);
void lcd_tablet_clk_update(struct lcd_config_s *pconf);
int lcd_tablet_probe(struct device *dev);
int lcd_tablet_remove(struct device *dev);
#endif

#endif
