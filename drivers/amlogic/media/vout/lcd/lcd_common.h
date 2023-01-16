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

/* 20200211: initial version*/
/* 20200827: add tm2 support*/
/* 20201116: add t5,t5d,t7 support*/
/* 20210222: add multi driver support*/
/* 20210409: update lvds & vbyone regs for t7*/
/* 20210412: add t3 support*/
/* 20210514: add extern driver support*/
/* 20210518: update t7 & t3 lvds lane setting*/
/* 20210616: fix t3 clk ss mistake*/
/* 20210621: separate resume(PREPARE) and late_resume(ENABLE)*/
/* 20210625: add tcon multi lut support*/
/* 20210705: add lcd mute and test state protection*/
/* 20211009: support 59 & 47 frame rate for tv mode*/
/* 20211102: add t5w support*/
/* 20211106: support vrr config*/
/* 20211210: support load tcon bin by ioctl*/
/* 20211216: support phy adjust by lane*/
/* 20211229: update multi lut init and switch flow*/
/* 20211230: support lcd driver DLG switch flow*/
/* 20220105: support measure vinfo sync_duration*/
/* 20220119: support tcon multi list threshold overlap*/
/* 20220121: update custom_pinmux and fr_auto_dis support*/
/* 20220216: update vrr config when display mode change*/
/* 20220408: update tcon reserved memory usage*/
/* 20220421: fix lcd clk mistake for vmode change with fr_adj_type 4*/
/* 20220426: update lcd mute control for DLG switch*/
/* 20220428: fix t5w regs mistake*/
/* 20220622: support tcon dynamic gamma*/
/* 20220719: support t5,t5w,t3 set vswing level in low common type*/
/* 20220729: optimize tcon switch flow before dlg timing change*/
/* 20220809: fix tcon axi mem mistake for DLG tcon bin*/
/* 20221012: correct t5w vbyone reset reg*/
/* 20221028: fix lane lock && fix t7 mipi lprx reg set*/
/* 20221111: modify edp transmit_unit_size to 48(temporary)*/
/* 20221115: support force unfit mipi-dsi bit_rate_max*/
/* 20221116: enable lcd clk for drm resume*/
/* 20221123: add ioctl functions, include power, mute, phy, ss*/
/* 20221215: remove unnecessary tcon top reset*/
/* 20221202: optimize lcd clk code*/
/* 20221229: optimize lcd venc code*/
/* 20230105: update clk ss support*/
/* 20230110: optimize config probe workqueue*/
/* 20230112: support drm display mode timing for different frame rate*/
#define LCD_DRV_VERSION    "20230112"

extern struct mutex lcd_vout_mutex;
extern spinlock_t lcd_reg_spinlock;
extern int lcd_vout_serve_bypass;

static inline unsigned int lcd_do_div(unsigned long long num, unsigned int den)
{
	unsigned long long ret = num;

	do_div(ret, den);

	return (unsigned int)ret;
}

/* lcd common */
void lcd_delay_us(int us);
void lcd_delay_ms(int ms);
unsigned char aml_lcd_i2c_bus_get_str(const char *str);
int lcd_type_str_to_type(const char *str);
char *lcd_type_type_to_str(int type);
unsigned char lcd_mode_str_to_mode(const char *str);
char *lcd_mode_mode_to_str(int mode);
u8 *lcd_vmap(ulong addr, u32 size);
void lcd_unmap_phyaddr(u8 *vaddr);

void lcd_cpu_gpio_probe(struct aml_lcd_drv_s *pdrv, unsigned int index);
void lcd_cpu_gpio_set(struct aml_lcd_drv_s *pdrv, unsigned int index, int value);
unsigned int lcd_cpu_gpio_get(struct aml_lcd_drv_s *pdrv, unsigned int index);
void lcd_ttl_pinmux_set(struct aml_lcd_drv_s *pdrv, int status);
void lcd_vbyone_pinmux_set(struct aml_lcd_drv_s *pdrv, int status);
void lcd_mlvds_pinmux_set(struct aml_lcd_drv_s *pdrv, int status);
void lcd_p2p_pinmux_set(struct aml_lcd_drv_s *pdrv, int status);
void lcd_edp_pinmux_set(struct aml_lcd_drv_s *pdrv, int status);

int lcd_get_config(struct aml_lcd_drv_s *pdrv);
void lcd_optical_vinfo_update(struct aml_lcd_drv_s *pdrv);

void lcd_vbyone_config_set(struct aml_lcd_drv_s *pdrv);
void lcd_mlvds_config_set(struct aml_lcd_drv_s *pdrv);
void lcd_p2p_config_set(struct aml_lcd_drv_s *pdrv);
void lcd_mipi_dsi_config_set(struct aml_lcd_drv_s *pdrv);
void lcd_edp_config_set(struct aml_lcd_drv_s *pdrv);
void lcd_vrr_config_update(struct aml_lcd_drv_s *pdrv);
void lcd_basic_timing_range_init(struct aml_lcd_drv_s *pdrv);
void lcd_timing_init_config(struct aml_lcd_drv_s *pdrv);

int lcd_fr_is_fixed(struct aml_lcd_drv_s *pdrv);
int lcd_vmode_change(struct aml_lcd_drv_s *pdrv);
void lcd_clk_change(struct aml_lcd_drv_s *pdrv);
void lcd_if_enable_retry(struct aml_lcd_drv_s *pdrv);
void lcd_vout_notify_mode_change_pre(struct aml_lcd_drv_s *pdrv);
void lcd_vout_notify_mode_change(struct aml_lcd_drv_s *pdrv);
void lcd_vinfo_update(struct aml_lcd_drv_s *pdrv);
unsigned int lcd_vrr_lfc_switch(void *dev_data, int fps);
int lcd_vrr_disable_cb(void *dev_data);
void lcd_vrr_dev_update(struct aml_lcd_drv_s *pdrv);

void lcd_queue_work(struct work_struct *work);
inline void lcd_queue_delayed_work(struct delayed_work *delayed_work, int ms);
unsigned int cal_crc32(unsigned int crc, const unsigned char *buf, int buf_len);

/* lcd phy */
void lcd_phy_tcon_chpi_bbc_init_tl1(struct lcd_config_s *pconf);
void lcd_phy_set(struct aml_lcd_drv_s *pdrv, int status);
int lcd_phy_probe(struct aml_lcd_drv_s *pdrv);
int lcd_phy_config_init(struct aml_lcd_drv_s *pdrv);
unsigned int lcd_phy_vswing_level_to_value(struct aml_lcd_drv_s *pdrv, unsigned int level);
unsigned int lcd_phy_preem_level_to_value(struct aml_lcd_drv_s *pdrv, unsigned int level);

/*lcd vbyone*/
void lcd_vbyone_enable_dft(struct aml_lcd_drv_s *pdrv);
void lcd_vbyone_disable_dft(struct aml_lcd_drv_s *pdrv);
void lcd_vbyone_enable_t7(struct aml_lcd_drv_s *pdrv);
void lcd_vbyone_disable_t7(struct aml_lcd_drv_s *pdrv);
void lcd_vbyone_link_maintain_clear(void);
void lcd_vbyone_wait_timing_stable(struct aml_lcd_drv_s *pdrv);
void lcd_vbyone_cdr_training_hold(struct aml_lcd_drv_s *pdrv, int flag);
void lcd_vbyone_wait_hpd(struct aml_lcd_drv_s *pdrv);
void lcd_vbyone_power_on_wait_stable(struct aml_lcd_drv_s *pdrv);
void lcd_vbyone_wait_stable(struct aml_lcd_drv_s *pdrv);
void lcd_vbyone_interrupt_enable(struct aml_lcd_drv_s *pdrv, int flag);
int lcd_vbyone_interrupt_up(struct aml_lcd_drv_s *pdrv);
void lcd_vbyone_interrupt_down(struct aml_lcd_drv_s *pdrv);
void lcd_vbyone_debug_cdr(struct aml_lcd_drv_s *pdrv);
void lcd_vbyone_debug_lock(struct aml_lcd_drv_s *pdrv);
void lcd_vbyone_debug_reset(struct aml_lcd_drv_s *pdrv);

/* lcd tcon */
unsigned int lcd_tcon_reg_read(struct aml_lcd_drv_s *pdrv, unsigned int addr);
void lcd_tcon_reg_write(struct aml_lcd_drv_s *pdrv,
			unsigned int addr, unsigned int val);
int lcd_tcon_probe(struct aml_lcd_drv_s *pdrv);
void lcd_tcon_global_reset(struct aml_lcd_drv_s *pdrv);
int lcd_tcon_gamma_set_pattern(struct aml_lcd_drv_s *pdrv,
			       unsigned int bit_width, unsigned int gamma_r,
			       unsigned int gamma_g, unsigned int gamma_b);
unsigned int lcd_tcon_table_read(unsigned int addr);
unsigned int lcd_tcon_table_write(unsigned int addr, unsigned int val);
int lcd_tcon_core_update(struct aml_lcd_drv_s *pdrv);
int lcd_tcon_od_set(struct aml_lcd_drv_s *pdrv, int flag);
int lcd_tcon_od_get(struct aml_lcd_drv_s *pdrv);
int lcd_tcon_core_reg_get(struct aml_lcd_drv_s *pdrv,
			  unsigned char *buf, unsigned int size);
int lcd_tcon_enable(struct aml_lcd_drv_s *pdrv);
int lcd_tcon_reload(struct aml_lcd_drv_s *pdrv);
int lcd_tcon_reload_pre(struct aml_lcd_drv_s *pdrv);
void lcd_tcon_disable(struct aml_lcd_drv_s *pdrv);
void lcd_tcon_vsync_isr(struct aml_lcd_drv_s *pdrv);

/* lcd debug */
int lcd_debug_info_len(int num);
int lcd_debug_probe(struct aml_lcd_drv_s *pdrv);
int lcd_debug_remove(struct aml_lcd_drv_s *pdrv);

/* lcd venc */
unsigned int lcd_get_encl_lint_cnt(struct aml_lcd_drv_s *pdrv);
void lcd_debug_test(struct aml_lcd_drv_s *pdrv, unsigned int num);
void lcd_wait_vsync(struct aml_lcd_drv_s *pdrv);
void lcd_gamma_debug_test_en(struct aml_lcd_drv_s *pdrv, int flag);
void lcd_set_venc_timing(struct aml_lcd_drv_s *pdrv);
void lcd_set_venc(struct aml_lcd_drv_s *pdrv);
void lcd_venc_change(struct aml_lcd_drv_s *pdrv);
void lcd_venc_vrr_recovery(struct aml_lcd_drv_s *pdrv);
int lcd_venc_probe(struct aml_lcd_drv_s *pdrv);
void lcd_venc_enable(struct aml_lcd_drv_s *pdrv, int flag);
void lcd_mute_set(struct aml_lcd_drv_s *pdrv, unsigned char flag);
int lcd_get_venc_init_config(struct aml_lcd_drv_s *pdrv);
void lcd_screen_black(struct aml_lcd_drv_s *pdrv);
void lcd_screen_restore(struct aml_lcd_drv_s *pdrv);

/* lcd clk */
extern spinlock_t lcd_clk_lock;
int meson_clk_measure(unsigned int clk_mux);
int lcd_debug_info_len(int num);
void lcd_clk_generate_parameter(struct aml_lcd_drv_s *pdrv);
int lcd_get_ss(struct aml_lcd_drv_s *pdrv, char *buf);
int lcd_get_ss_num(struct aml_lcd_drv_s *pdrv, unsigned int *level,
				unsigned int *freq, unsigned int *mode);
int lcd_set_ss(struct aml_lcd_drv_s *pdrv, unsigned int level,
	       unsigned int freq, unsigned int mode);
void lcd_clk_ss_config_init(struct aml_lcd_drv_s *pdrv);
int lcd_encl_clk_msr(struct aml_lcd_drv_s *pdrv);
void lcd_pll_reset(struct aml_lcd_drv_s *pdrv);
void lcd_update_clk(struct aml_lcd_drv_s *pdrv);
void lcd_set_clk(struct aml_lcd_drv_s *pdrv);
void lcd_disable_clk(struct aml_lcd_drv_s *pdrv);
void lcd_clk_gate_switch(struct aml_lcd_drv_s *pdrv, int status);
int lcd_clk_clkmsr_print(struct aml_lcd_drv_s *pdrv, char *buf, int offset);
int lcd_clk_config_print(struct aml_lcd_drv_s *pdrv, char *buf, int offset);
int lcd_clk_path_change(struct aml_lcd_drv_s *pdrv, int sel);
void lcd_clk_config_probe(struct aml_lcd_drv_s *pdrv);
void lcd_clk_config_remove(struct aml_lcd_drv_s *pdrv);
void lcd_clk_config_init(void);
void aml_lcd_prbs_test(struct aml_lcd_drv_s *pdrv, unsigned int ms, unsigned int mode_flag);

/* lcd driver */
#ifdef CONFIG_AMLOGIC_LCD_TV
void lcd_tv_vout_server_init(struct aml_lcd_drv_s *pdrv);
void lcd_tv_vout_server_remove(struct aml_lcd_drv_s *pdrv);
void lcd_tv_clk_config_change(struct aml_lcd_drv_s *pdrv);
void lcd_tv_clk_update(struct aml_lcd_drv_s *pdrv);
int lcd_mode_tv_init(struct aml_lcd_drv_s *pdrv);
int lcd_mode_tv_remove(struct aml_lcd_drv_s *pdrv);
#endif
#ifdef CONFIG_AMLOGIC_LCD_TABLET
int lcd_mipi_dsi_init_table_detect(struct aml_lcd_drv_s *pdrv,
				   struct device_node *m_node, int on_off);
void mipi_dsi_config_init(struct aml_lcd_drv_s *pdrv);
int lcd_mipi_test_read(struct aml_lcd_drv_s *pdrv, struct dsi_read_s *dread);
int dsi_set_operation_mode(struct aml_lcd_drv_s *pdrv, unsigned char op_mode);
int dptx_edid_dump(struct aml_lcd_drv_s *pdrv);
int dptx_edid_timing_probe(struct aml_lcd_drv_s *pdrv);
int dptx_dpcd_read(struct aml_lcd_drv_s *pdrv, unsigned char *buf,
		   unsigned int reg, int len);
int dptx_dpcd_write(struct aml_lcd_drv_s *pdrv, unsigned int reg, unsigned char val);
void dptx_dpcd_dump(struct aml_lcd_drv_s *pdrv);
void lcd_tablet_vout_server_init(struct aml_lcd_drv_s *pdrv);
void lcd_tablet_vout_server_remove(struct aml_lcd_drv_s *pdrv);
void lcd_tablet_clk_config_change(struct aml_lcd_drv_s *pdrv);
void lcd_tablet_clk_update(struct aml_lcd_drv_s *pdrv);
int lcd_mode_tablet_init(struct aml_lcd_drv_s *pdrv);
int lcd_mode_tablet_remove(struct aml_lcd_drv_s *pdrv);
#endif

int lcd_drm_add(struct device *dev);
void lcd_drm_remove(struct device *dev);

#endif
