// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <dt-bindings/gpio/meson-t5w-gpio.h>
#include "pinctrl-meson.h"
#include "pinctrl-meson-axg-pmx.h"
#include "pinctrl-module-init.h"

static const struct pinctrl_pin_desc meson_t5w_analog_pins[] = {
	MESON_PIN(CDAC_IOUT),
	MESON_PIN(CVBS0),
	MESON_PIN(CVBS1)
};

static struct meson_pmx_group meson_t5w_analog_groups[] __initdata = {
	GPIO_GROUP(CDAC_IOUT),
	GPIO_GROUP(CVBS0),
	GPIO_GROUP(CVBS1)
};

static const char * const gpio_analog_groups[] = {
	"CDAC_IOUT", "CVBS0", "CVBS1"
};

static struct meson_pmx_func meson_t5w_analog_functions[] __initdata = {
	FUNCTION(gpio_analog)
};

static const struct pinctrl_pin_desc meson_t5w_aobus_pins[] = {
	MESON_PIN(GPIOD_0),
	MESON_PIN(GPIOD_1),
	MESON_PIN(GPIOD_2),
	MESON_PIN(GPIOD_3),
	MESON_PIN(GPIOD_4),
	MESON_PIN(GPIOD_5),
	MESON_PIN(GPIOD_6),
	MESON_PIN(GPIOD_7),
	MESON_PIN(GPIOD_8),
	MESON_PIN(GPIOD_9),
	MESON_PIN(GPIOD_10),
	MESON_PIN(GPIOE_0),
	MESON_PIN(GPIOE_1),
};

/* D func 1*/
static const unsigned int uart_ao_b_tx_pins[]		= { GPIOD_0 };
static const unsigned int uart_ao_b_rx_pins[]		= { GPIOD_1 };
static const unsigned int i2c1_ao_sck_pins[]		= { GPIOD_2 };
static const unsigned int i2c1_ao_sda_pins[]		= { GPIOD_3 };
static const unsigned int clk_32k_in_pins[]		= { GPIOD_4 };
static const unsigned int remote_input_ao_pins[]	= { GPIOD_5 };
static const unsigned int jtag_a_clk_pins[]		= { GPIOD_6 };
static const unsigned int jtag_a_tms_pins[]		= { GPIOD_7 };
static const unsigned int jtag_a_tdi_pins[]		= { GPIOD_8 };
static const unsigned int jtag_a_tdo_pins[]		= { GPIOD_9 };
static const unsigned int gen_clk_ee_d_pins[]		= { GPIOD_10};

/* D func 2 */
static const unsigned int remote_out_ao1_pins[]		= { GPIOD_1 };
static const unsigned int i2c_ao_slave_sck_pins[]	= { GPIOD_2 };
static const unsigned int i2c_ao_slave_sda_pins[]	= { GPIOD_3 };
static const unsigned int atv_if_agc_ao_pins[]		= { GPIOD_4 };
static const unsigned int pwm_ao_c_d5_pins[]		= { GPIOD_5 };
static const unsigned int pwm_ao_d_d6_pins[]		= { GPIOD_6 };
static const unsigned int pwm_ao_c_d7_pins[]		= { GPIOD_7 };
static const unsigned int spdif_out_a_ao_pins[]		= { GPIOD_8 };
static const unsigned int pwm_ao_d_d9_pins[]		= { GPIOD_9 };
static const unsigned int pwm_ao_e_pins[]		= { GPIOD_10 };

/* D func 3 */
static const unsigned int cicam_ao_waitn_pins[]		= { GPIOD_0 };
static const unsigned int dtv_if_agc_ao_pins[]		= { GPIOD_4 };
static const unsigned int pwm_ao_d_hiz_pins[]		= { GPIOD_6 };
static const unsigned int pwm_ao_c_hiz_pins[]		= { GPIOD_7 };
static const unsigned int spdif_in_ao_pins[]		= { GPIOD_8 };
static const unsigned int remote_out_ao9_pins[]		= { GPIOD_9 };
static const unsigned int gen_clk_ao_pins[]		= { GPIOD_10 };

/* D func 4 */
static const unsigned int uart_ao_c_tx_pins[]		= { GPIOD_6 };
static const unsigned int uart_ao_c_rx_pins[]		= { GPIOD_7 };
static const unsigned int uart_ao_c_cts_pins[]		= { GPIOD_8 };
static const unsigned int uart_ao_c_rts_pins[]		= { GPIOD_9 };

/* D func 5 */
static const unsigned int spi1_ss0_ao_pins[]		= { GPIOD_8 };
static const unsigned int spi1_miso_ao_pins[]		= { GPIOD_9 };

/* D func 6 */
static const unsigned int spdif_out_b_ao_pins[]		= { GPIOD_8 };

/* E func 1 */
static const unsigned int pwm_ao_a_pins[]		= { GPIOE_0 };
static const unsigned int pwm_ao_b_pins[]		= { GPIOE_1 };

/* E func 2 */
static const unsigned int i2c2_ao_sck_pins[]		= { GPIOE_0 };
static const unsigned int i2c2_ao_sda_pins[]		= { GPIOE_1 };

static struct meson_pmx_group meson_t5w_aobus_groups[] __initdata = {
	GPIO_GROUP(GPIOD_0),
	GPIO_GROUP(GPIOD_1),
	GPIO_GROUP(GPIOD_2),
	GPIO_GROUP(GPIOD_3),
	GPIO_GROUP(GPIOD_4),
	GPIO_GROUP(GPIOD_5),
	GPIO_GROUP(GPIOD_6),
	GPIO_GROUP(GPIOD_7),
	GPIO_GROUP(GPIOD_8),
	GPIO_GROUP(GPIOD_9),
	GPIO_GROUP(GPIOD_10),
	GPIO_GROUP(GPIOE_0),
	GPIO_GROUP(GPIOE_1),

	/* bank D */
	GROUP(uart_ao_b_tx,		1),
	GROUP(uart_ao_b_rx,		1),
	GROUP(i2c1_ao_sck,		1),
	GROUP(i2c1_ao_sda,		1),
	GROUP(clk_32k_in,		1),
	GROUP(remote_input_ao,		1),
	GROUP(jtag_a_clk,		1),
	GROUP(jtag_a_tms,		1),
	GROUP(jtag_a_tdi,		1),
	GROUP(jtag_a_tdo,		1),
	GROUP(gen_clk_ee_d,		1),

	GROUP(remote_out_ao1,		2),
	GROUP(i2c_ao_slave_sck,		2),
	GROUP(i2c_ao_slave_sda,		2),
	GROUP(atv_if_agc_ao,		2),
	GROUP(pwm_ao_c_d5,		2),
	GROUP(pwm_ao_d_d6,		2),
	GROUP(pwm_ao_c_d7,		2),
	GROUP(spdif_out_a_ao,		2),
	GROUP(pwm_ao_d_d9,		2),
	GROUP(pwm_ao_e,			2),

	GROUP(cicam_ao_waitn,		3),
	GROUP(dtv_if_agc_ao,		3),
	GROUP(pwm_ao_d_hiz,		3),
	GROUP(pwm_ao_c_hiz,		3),
	GROUP(spdif_in_ao,		3),
	GROUP(remote_out_ao9,		3),
	GROUP(gen_clk_ao,		3),

	GROUP(uart_ao_c_tx,		4),
	GROUP(uart_ao_c_rx,		4),
	GROUP(uart_ao_c_cts,		4),
	GROUP(uart_ao_c_rts,		4),

	GROUP(spi1_ss0_ao,		5),
	GROUP(spi1_miso_ao,		5),

	GROUP(spdif_out_b_ao,		6),

	/* bank E */
	GROUP(pwm_ao_a,			1),
	GROUP(pwm_ao_b,			1),

	GROUP(i2c2_ao_sck,		2),
	GROUP(i2c2_ao_sda,		2)

};

static const char * const gpio_aobus_groups[] = {
	"GPIOD_0", "GPIOD_1", "GPIOD_2", "GPIOD_3",
	"GPIOD_4", "GPIOD_5", "GPIOD_6", "GPIOD_7",
	"GPIOD_8", "GPIOD_9", "GPIOD_10", "GPIOE_0",
	"GPIOE_1"
};

static const char * const uart_ao_b_groups[] = {
	"uart_ao_b_tx", "uart_ao_b_rx"
};

static const char * const uart_ao_c_groups[] = {
	"uart_ao_c_tx", "uart_ao_c_rx", "uart_ao_c_cts", "uart_ao_c_rts"
};

static const char * const i2c1_ao_groups[] = {
	"i2c1_ao_sck", "i2c1_ao_sda"
};

static const char * const i2c2_ao_groups[] = {
	"i2c2_ao_sck", "i2c2_ao_sda"
};

static const char * const clk_32k_in_groups[] = {
	"clk_32k_in"
};

static const char * const remote_input_ao_groups[] = {
	"remote_input_ao"
};

static const char * const jtag_a_groups[] = {
	"jtag_a_clk", "jtag_a_tms", "jtag_a_tdi", "jtag_a_tdo"
};

static const char * const gen_clk_ee_ao_groups[] = {
	"gen_clk_ee_ao"
};

static const char * const remote_out_ao_groups[] = {
	"remote_out_ao1", "remote_out_ao9"
};

static const char * const pwm_ao_a_groups[] = {
	"pwm_ao_a"
};

static const char * const pwm_ao_b_groups[] = {
	"pwm_ao_b"
};

static const char * const pwm_ao_c_groups[] = {
	"pwm_ao_c_d7", "pwm_ao_c_d5"
};

static const char * const pwm_ao_d_groups[] = {
	"pwm_ao_d_d6", "pwm_ao_d_d9"
};

static const char * const pwm_ao_e_groups[] = {
	"pwm_ao_e"
};

static const char * const pwm_ao_c_hiz_groups[] = {
	"pwm_ao_c_hiz"
};

static const char * const pwm_ao_d_hiz_groups[] = {
	"pwm_ao_d_hiz"
};

static const char * const spdif_out_a_ao_groups[] = {
	"spdif_out_a_ao"
};

static const char * const spdif_out_b_ao_groups[] = {
	"spdif_out_b_ao"
};

static const char * const spdif_in_ao_groups[] = {
	"spdif_in_ao"
};

static const char * const dtv_ao_groups[] = {
	"dtv_if_agc_ao"
};

static const char * const atv_ao_groups[] = {
	"atv_if_agc_ao"
};

static const char * const gen_clk_ao_groups[] = {
	"gen_clk_ao"
};

static const char * const cicam_ao_groups[] = {
	"cicam_ao_waitn"
};

static const char * const spi1_ao_groups[] = {
	"spi1_sso_ao", "spi1_miso_ao"
};

static struct meson_pmx_func meson_t5w_aobus_functions[] __initdata = {
	FUNCTION(gpio_aobus),
	FUNCTION(uart_ao_b),
	FUNCTION(uart_ao_c),
	FUNCTION(i2c1_ao),
	FUNCTION(i2c2_ao),
	FUNCTION(clk_32k_in),
	FUNCTION(remote_input_ao),
	FUNCTION(remote_out_ao),
	FUNCTION(jtag_a),
	FUNCTION(gen_clk_ee_ao),
	FUNCTION(gen_clk_ao),
	FUNCTION(pwm_ao_a),
	FUNCTION(pwm_ao_b),
	FUNCTION(pwm_ao_c),
	FUNCTION(pwm_ao_d),
	FUNCTION(pwm_ao_e),
	FUNCTION(pwm_ao_c_hiz),
	FUNCTION(pwm_ao_d_hiz),
	FUNCTION(spdif_out_a_ao),
	FUNCTION(spdif_out_b_ao),
	FUNCTION(spdif_in_ao),
	FUNCTION(spi1_ao),
	FUNCTION(dtv_ao),
	FUNCTION(atv_ao),
	FUNCTION(cicam_ao)
};

static const struct pinctrl_pin_desc meson_t5w_periphs_pins[] = {
	MESON_PIN(GPIOB_0),
	MESON_PIN(GPIOB_1),
	MESON_PIN(GPIOB_2),
	MESON_PIN(GPIOB_3),
	MESON_PIN(GPIOB_4),
	MESON_PIN(GPIOB_5),
	MESON_PIN(GPIOB_6),
	MESON_PIN(GPIOB_7),
	MESON_PIN(GPIOB_8),
	MESON_PIN(GPIOB_9),
	MESON_PIN(GPIOB_10),
	MESON_PIN(GPIOB_11),
	MESON_PIN(GPIOB_12),
	MESON_PIN(GPIOB_13),
	MESON_PIN(GPIOW_0),
	MESON_PIN(GPIOW_1),
	MESON_PIN(GPIOW_2),
	MESON_PIN(GPIOW_3),
	MESON_PIN(GPIOW_4),
	MESON_PIN(GPIOW_5),
	MESON_PIN(GPIOW_6),
	MESON_PIN(GPIOW_7),
	MESON_PIN(GPIOW_8),
	MESON_PIN(GPIOW_9),
	MESON_PIN(GPIOW_10),
	MESON_PIN(GPIOW_11),
	MESON_PIN(GPIOW_12),
	MESON_PIN(GPIOZ_0),
	MESON_PIN(GPIOZ_1),
	MESON_PIN(GPIOZ_2),
	MESON_PIN(GPIOZ_3),
	MESON_PIN(GPIOZ_4),
	MESON_PIN(GPIOZ_5),
	MESON_PIN(GPIOZ_6),
	MESON_PIN(GPIOZ_7),
	MESON_PIN(GPIOZ_8),
	MESON_PIN(GPIOZ_9),
	MESON_PIN(GPIOZ_10),
	MESON_PIN(GPIOZ_11),
	MESON_PIN(GPIOZ_12),
	MESON_PIN(GPIOZ_13),
	MESON_PIN(GPIOZ_14),
	MESON_PIN(GPIOZ_15),
	MESON_PIN(GPIOZ_16),
	MESON_PIN(GPIOZ_17),
	MESON_PIN(GPIOZ_18),
	MESON_PIN(GPIOZ_19),
	MESON_PIN(GPIOH_0),
	MESON_PIN(GPIOH_1),
	MESON_PIN(GPIOH_2),
	MESON_PIN(GPIOH_3),
	MESON_PIN(GPIOH_4),
	MESON_PIN(GPIOH_5),
	MESON_PIN(GPIOH_6),
	MESON_PIN(GPIOH_7),
	MESON_PIN(GPIOH_8),
	MESON_PIN(GPIOH_9),
	MESON_PIN(GPIOH_10),
	MESON_PIN(GPIOH_11),
	MESON_PIN(GPIOH_12),
	MESON_PIN(GPIOH_13),
	MESON_PIN(GPIOH_14),
	MESON_PIN(GPIOH_15),
	MESON_PIN(GPIOH_16),
	MESON_PIN(GPIOH_17),
	MESON_PIN(GPIOH_18),
	MESON_PIN(GPIOH_19),
	MESON_PIN(GPIOH_20),
	MESON_PIN(GPIOH_21),
	MESON_PIN(GPIOH_22),
	MESON_PIN(GPIOH_23),
	MESON_PIN(GPIOH_24),
	MESON_PIN(GPIOH_25),
	MESON_PIN(GPIOM_0),
	MESON_PIN(GPIOM_1),
	MESON_PIN(GPIOM_2),
	MESON_PIN(GPIOM_3),
	MESON_PIN(GPIOM_4),
	MESON_PIN(GPIOM_5),
	MESON_PIN(GPIOM_6),
	MESON_PIN(GPIOM_7),
	MESON_PIN(GPIOM_8),
	MESON_PIN(GPIOM_9),
	MESON_PIN(GPIOM_10),
	MESON_PIN(GPIOM_11),
	MESON_PIN(GPIOM_12),
	MESON_PIN(GPIOM_13),
	MESON_PIN(GPIOM_14),
	MESON_PIN(GPIOM_15),
	MESON_PIN(GPIOM_16),
	MESON_PIN(GPIOM_17),
	MESON_PIN(GPIOM_18),
	MESON_PIN(GPIOM_19),
	MESON_PIN(GPIOM_20),
	MESON_PIN(GPIOM_21),
	MESON_PIN(GPIOM_22),
	MESON_PIN(GPIOM_23),
	MESON_PIN(GPIOM_24),
	MESON_PIN(GPIOM_25),
	MESON_PIN(GPIOM_26),
	MESON_PIN(GPIOM_27),
	MESON_PIN(GPIOM_28),
	MESON_PIN(GPIOM_29),
	MESON_PIN(GPIOC_0),
	MESON_PIN(GPIOC_1),
	MESON_PIN(GPIOC_2),
	MESON_PIN(GPIOC_3),
	MESON_PIN(GPIOC_4),
	MESON_PIN(GPIOC_5),
	MESON_PIN(GPIOC_6),
	MESON_PIN(GPIOC_7),
	MESON_PIN(GPIOC_8),
	MESON_PIN(GPIOC_9),
	MESON_PIN(GPIOC_10),
	MESON_PIN(GPIOC_11),
	MESON_PIN(GPIOC_12),
	MESON_PIN(GPIOC_13),
	MESON_PIN(GPIOC_14)
};

/* BANK W func1 */
static const unsigned int hdmirx_a_hpd_pins[]		= { GPIOW_0 };
static const unsigned int hdmirx_a_det_pins[]		= { GPIOW_1 };
static const unsigned int hdmirx_a_sda_pins[]		= { GPIOW_2 };
static const unsigned int hdmirx_a_sck_pins[]		= { GPIOW_3 };
static const unsigned int hdmirx_c_hpd_pins[]		= { GPIOW_4 };
static const unsigned int hdmirx_c_det_pins[]		= { GPIOW_5 };
static const unsigned int hdmirx_c_sda_pins[]		= { GPIOW_6 };
static const unsigned int hdmirx_c_sck_pins[]		= { GPIOW_7 };
static const unsigned int hdmirx_b_hpd_pins[]		= { GPIOW_8 };
static const unsigned int hdmirx_b_det_pins[]		= { GPIOW_9 };
static const unsigned int hdmirx_b_sda_pins[]		= { GPIOW_10 };
static const unsigned int hdmirx_b_sck_pins[]		= { GPIOW_11 };
static const unsigned int cec_pins[]			= { GPIOW_12 };

/* BANK W func2 */
static const unsigned int uart_b_tx_w2_pins[]		= { GPIOW_2 };
static const unsigned int uart_b_rx_w3_pins[]		= { GPIOW_3 };
static const unsigned int uart_b_tx_w6_pins[]		= { GPIOW_6 };
static const unsigned int uart_b_rx_w7_pins[]		= { GPIOW_7 };
static const unsigned int uart_b_tx_w10_pins[]		= { GPIOW_10 };
static const unsigned int uart_b_rx_w11_pins[]		= { GPIOW_11 };

/* BANK B func1 */
static const unsigned int emmc_nand_d0_pins[]		= { GPIOB_0 };
static const unsigned int emmc_nand_d1_pins[]		= { GPIOB_1 };
static const unsigned int emmc_nand_d2_pins[]		= { GPIOB_2 };
static const unsigned int emmc_nand_d3_pins[]		= { GPIOB_3 };
static const unsigned int emmc_nand_d4_pins[]		= { GPIOB_4 };
static const unsigned int emmc_nand_d5_pins[]		= { GPIOB_5 };
static const unsigned int emmc_nand_d6_pins[]		= { GPIOB_6 };
static const unsigned int emmc_nand_d7_pins[]		= { GPIOB_7 };
static const unsigned int emmc_clk_pins[]		= { GPIOB_8 };
static const unsigned int emmc_cmd_pins[]		= { GPIOB_10 };
static const unsigned int emmc_nand_ds_pins[]		= { GPIOB_11 };
static const unsigned int loop_gpio_b12_pins[]		= { GPIOB_13 };

/* BANK B func2 */
static const unsigned int nand_wen_clk_pins[]		= { GPIOB_8 };
static const unsigned int nand_ale_pins[]		= { GPIOB_9 };
static const unsigned int nand_ren_wr_pins[]		= { GPIOB_10 };
static const unsigned int nand_cle_pins[]		= { GPIOB_11 };
static const unsigned int nand_ce0_pins[]		= { GPIOB_12 };
static const unsigned int loop_gpio_h21_pins[]		= { GPIOB_13 };

/* BANK B func3 */
static const unsigned int spif_hold_pins[]		= { GPIOB_3 };
static const unsigned int spif_mo_pins[]		= { GPIOB_4 };
static const unsigned int spif_mi_pins[]		= { GPIOB_5 };
static const unsigned int spif_clk_pins[]		= { GPIOB_6 };
static const unsigned int spif_wp_pins[]		= { GPIOB_7 };
static const unsigned int remote_out_pins[]		= { GPIOB_12 };
static const unsigned int spif_cs_pins[]		= { GPIOB_13 };

/* BANK C func1 */
static const unsigned int sdcard_d0_pins[]		= { GPIOC_0 };
static const unsigned int sdcard_d1_pins[]		= { GPIOC_1 };
static const unsigned int sdcard_d2_pins[]		= { GPIOC_2 };
static const unsigned int sdcard_d3_pins[]		= { GPIOC_3 };
static const unsigned int sdcard_clk_pins[]		= { GPIOC_4 };
static const unsigned int sdcard_cmd_pins[]		= { GPIOC_5 };
static const unsigned int uart_a_tx_c_pins[]		= { GPIOC_6 };
static const unsigned int uart_a_rx_c_pins[]		= { GPIOC_7 };
static const unsigned int uart_a_cts_c_pins[]		= { GPIOC_8 };
static const unsigned int uart_a_rts_c_pins[]		= { GPIOC_9 };
static const unsigned int pwm_f_c_pins[]		= { GPIOC_10 };
static const unsigned int pwm_a_pins[]			= { GPIOC_11 };
static const unsigned int pwm_c_pins[]			= { GPIOC_12 };
static const unsigned int i2c1_sck_c_pins[]		= { GPIOC_13 };
static const unsigned int i2c1_sda_c_pins[]		= { GPIOC_14 };

/* BANK C func2 */
static const unsigned int spi2_miso_c_pins[]		= { GPIOC_0 };
static const unsigned int spi2_mosi_c_pins[]		= { GPIOC_1 };
static const unsigned int spi2_clk_c_pins[]		= { GPIOC_2 };
static const unsigned int spi2_ss0_c_pins[]		= { GPIOC_3 };
static const unsigned int spi2_ss1_c_pins[]		= { GPIOC_4 };
static const unsigned int spi2_ss2_c_pins[]		= { GPIOC_5 };

/* BANK C func3 */
static const unsigned int cicam_a2_c_pins[]		= { GPIOC_0 };
static const unsigned int cicam_a3_c_pins[]		= { GPIOC_1 };
static const unsigned int cicam_a4_c_pins[]		= { GPIOC_2 };
static const unsigned int cicam_a5_c_pins[]		= { GPIOC_3 };
static const unsigned int cicam_a6_c_pins[]		= { GPIOC_4 };
static const unsigned int cicam_a7_c_pins[]		= { GPIOC_5 };
static const unsigned int cicam_a8_c_pins[]		= { GPIOC_6 };
static const unsigned int cicam_a9_c_pins[]		= { GPIOC_7 };
static const unsigned int cicam_a10_c_pins[]		= { GPIOC_8 };
static const unsigned int cicam_a11_c_pins[]		= { GPIOC_9 };
static const unsigned int cicam_a12_c_pins[]		= { GPIOC_10 };

/* BANK C func4 */
static const unsigned int demod_uart_tx_c_pins[]	= { GPIOC_6 };
static const unsigned int demod_uart_rx_c_pins[]	= { GPIOC_7 };

/* BANK C func5 */
static const unsigned int tdm_fs1_c_pins[]		= { GPIOC_6 };
static const unsigned int tdm_sclk1_c_pins[]		= { GPIOC_7 };
static const unsigned int tdm_d4_c_pins[]		= { GPIOC_8 };
static const unsigned int tdm_d5_c_pins[]		= { GPIOC_9 };
static const unsigned int tdm_d6_c_pins[]		= { GPIOC_10 };
static const unsigned int tdm_d7_c_pins[]		= { GPIOC_11 };

/* BANK Z func1 */
static const unsigned int tdm_fs2_z_pins[]		= { GPIOZ_0 };
static const unsigned int tdm_sclk2_z_pins[]		= { GPIOZ_1 };
static const unsigned int tdm_d4_z_pins[]		= { GPIOZ_2 };
static const unsigned int tdm_d5_z_pins[]		= { GPIOZ_3 };
static const unsigned int tdm_d6_z_pins[]		= { GPIOZ_4 };
static const unsigned int tdm_d7_z_pins[]		= { GPIOZ_5 };
static const unsigned int mclk_2_z_pins[]		= { GPIOZ_6 };
static const unsigned int tsout_clk_pins[]		= { GPIOZ_7 };
static const unsigned int tsout_sop_pins[]		= { GPIOZ_8 };
static const unsigned int tsout_valid_pins[]		= { GPIOZ_9 };
static const unsigned int tsout_d0_pins[]		= { GPIOZ_10 };
static const unsigned int tsout_d1_pins[]		= { GPIOZ_11 };
static const unsigned int tsout_d2_pins[]		= { GPIOZ_12 };
static const unsigned int tsout_d3_pins[]		= { GPIOZ_13 };
static const unsigned int tsout_d4_pins[]		= { GPIOZ_14 };
static const unsigned int tsout_d5_pins[]		= { GPIOZ_15 };
static const unsigned int tsout_d6_pins[]		= { GPIOZ_16 };
static const unsigned int tsout_d7_pins[]		= { GPIOZ_17 };
static const unsigned int mclk_1_z_pins[]		= { GPIOZ_18 };
static const unsigned int spdif_out_a_z19_pins[]	= { GPIOZ_19 };

/* BANK Z func2 */
static const unsigned int tsin_a_clk_pins[]		= { GPIOZ_0 };
static const unsigned int tsin_a_sop_pins[]		= { GPIOZ_1 };
static const unsigned int tsin_a_valid_pins[]		= { GPIOZ_2 };
static const unsigned int tsin_a_d0_pins[]		= { GPIOZ_3 };
static const unsigned int dtv_rf_agc_z6_pins[]		= { GPIOZ_6 };
static const unsigned int cicam_a2_pins[]		= { GPIOZ_8 };
static const unsigned int cicam_a3_pins[]		= { GPIOZ_9 };
static const unsigned int cicam_a4_pins[]		= { GPIOZ_10 };
static const unsigned int cicam_a5_pins[]		= { GPIOZ_11 };
static const unsigned int cicam_a6_pins[]		= { GPIOZ_12 };
static const unsigned int cicam_a7_pins[]		= { GPIOZ_13 };
static const unsigned int cicam_a8_pins[]		= { GPIOZ_14 };
static const unsigned int cicam_a9_pins[]		= { GPIOZ_15 };
static const unsigned int cicam_a10_pins[]		= { GPIOZ_16 };
static const unsigned int cicam_a11_pins[]		= { GPIOZ_17 };
static const unsigned int dtv_rf_agc_z18_pins[]		= { GPIOZ_18 };
static const unsigned int spdif_in_z_pins[]		= { GPIOZ_19 };

/* BANK Z func3 */
static const unsigned int iso7816_clk_z_pins[]		= { GPIOZ_0 };
static const unsigned int iso7816_data_z_pins[]		= { GPIOZ_1 };
static const unsigned int cicam_a13_pins[]		= { GPIOZ_2 };
static const unsigned int cicam_a14_pins[]		= { GPIOZ_3 };
static const unsigned int i2c0_sck_z_pins[]		= { GPIOZ_4 };
static const unsigned int i2c0_sda_z_pins[]		= { GPIOZ_5 };
static const unsigned int atv_if_agc_z6_pins[]		= { GPIOZ_6 };
static const unsigned int atv_if_agc_z18_pins[]		= { GPIOZ_18 };
static const unsigned int spdif_out_b_pins[]		= { GPIOZ_19 };

/* BANK Z func4 */
static const unsigned int spi0_miso_z_pins[]		= { GPIOZ_0 };
static const unsigned int spi0_mosi_z_pins[]		= { GPIOZ_1 };
static const unsigned int spi0_clk_z_pins[]		= { GPIOZ_2 };
static const unsigned int spi0_ss0_z_pins[]		= { GPIOZ_3 };
static const unsigned int spi0_ss1_z_pins[]		= { GPIOZ_4 };
static const unsigned int spi0_ss2_z_pins[]		= { GPIOZ_5 };
static const unsigned int dtv_if_agc_z6_pins[]		= { GPIOZ_6 };
static const unsigned int dtv_if_agc_z18_pins[]		= { GPIOZ_18 };

/* BANK Z func5 */
static const unsigned int uart_a_tx_z_pins[]		= { GPIOZ_0 };
static const unsigned int uart_a_rx_z_pins[]		= { GPIOZ_1 };
static const unsigned int uart_a_cts_z_pins[]		= { GPIOZ_2 };
static const unsigned int uart_a_rts_z_pins[]		= { GPIOZ_3 };
static const unsigned int pwm_b_z_pins[]		= { GPIOZ_4 };
static const unsigned int pwm_d_z_pins[]		= { GPIOZ_5 };
static const unsigned int pwm_e_z_pins[]		= { GPIOZ_6 };
static const unsigned int pwm_g_z_pins[]		= { GPIOZ_12 };
static const unsigned int pwm_h_z_pins[]		= { GPIOZ_13 };
static const unsigned int spi1_ss0_z_pins[]		= { GPIOZ_18 };
static const unsigned int spi1_miso_z_pins[]		= { GPIOZ_19 };

/* BANK Z func6 */
static const unsigned int cicam_a12_pins[]		= { GPIOZ_0 };
static const unsigned int pdm_din1_z1_pins[]		= { GPIOZ_1 };
static const unsigned int pdm_dclk_z_pins[]		= { GPIOZ_2 };
static const unsigned int pdm_din0_z_pins[]		= { GPIOZ_3 };
static const unsigned int pdm_din1_z6_pins[]		= { GPIOZ_6 };
static const unsigned int tdm_d1_z_pins[]		= { GPIOZ_10 };
static const unsigned int tdm_d0_z_pins[]		= { GPIOZ_11 };
static const unsigned int tdm_sclk1_z_pins[]		= { GPIOZ_16 };
static const unsigned int tdm_fs1_z_pins[]		= { GPIOZ_17 };
static const unsigned int diseqc_out_z18_pins[]		= { GPIOZ_18 };

/* BANK Z func7 */
static const unsigned int diseqc_out_z0_pins[]		= { GPIOZ_0 };
static const unsigned int s2_demod_gpio0_z_pins[]	= { GPIOZ_1 };
static const unsigned int spdif_out_a_z2_pins[]		= { GPIOZ_2 };
static const unsigned int spi0_ss1_z3_pins[]		= { GPIOZ_3 };
static const unsigned int mic_mute_key_z_pins[]		= { GPIOZ_18 };
static const unsigned int mic_mute_led_z_pins[]		= { GPIOZ_19 };

/* BANK H func1 */
static const unsigned int tcon_0_pins[]			= { GPIOH_0 };
static const unsigned int tcon_1_pins[]			= { GPIOH_1 };
static const unsigned int tcon_2_pins[]			= { GPIOH_2 };
static const unsigned int tcon_3_pins[]			= { GPIOH_3 };
static const unsigned int tcon_4_pins[]			= { GPIOH_4 };
static const unsigned int tcon_5_pins[]			= { GPIOH_5 };
static const unsigned int tcon_6_pins[]			= { GPIOH_6 };
static const unsigned int tcon_7_pins[]			= { GPIOH_7 };
static const unsigned int tcon_8_pins[]			= { GPIOH_8 };
static const unsigned int tcon_9_pins[]			= { GPIOH_9 };
static const unsigned int tcon_10_pins[]		= { GPIOH_10 };
static const unsigned int tcon_11_pins[]		= { GPIOH_11 };
static const unsigned int tcon_12_pins[]		= { GPIOH_12 };
static const unsigned int tcon_13_pins[]		= { GPIOH_13 };
static const unsigned int tcon_14_pins[]		= { GPIOH_14 };
static const unsigned int tcon_15_pins[]		= { GPIOH_15 };
static const unsigned int hsync_pins[]			= { GPIOH_18 };
static const unsigned int vsync_pins[]			= { GPIOH_19 };
static const unsigned int i2c2_sck_h20_pins[]		= { GPIOH_20 };
static const unsigned int i2c2_sda_h21_pins[]		= { GPIOH_21 };
static const unsigned int i2c3_sck_h22_pins[]		= { GPIOH_22 };
static const unsigned int i2c3_sda_h23_pins[]		= { GPIOH_23 };
static const unsigned int i2c3_sck_h24_pins[]		= { GPIOH_24 };
static const unsigned int i2c3_sda_h25_pins[]		= { GPIOH_25 };

/* BANK H func2 */
static const unsigned int tcon_lock_pins[]		= { GPIOH_0 };
static const unsigned int uart_c_tx_h1_pins[]		= { GPIOH_1 };
static const unsigned int uart_c_rx_h2_pins[]		= { GPIOH_2 };
static const unsigned int uart_c_cts_h_pins[]		= { GPIOH_3 };
static const unsigned int uart_c_rts_h_pins[]		= { GPIOH_4 };
static const unsigned int sync_3d_out_pins[]		= { GPIOH_5 };
static const unsigned int tdm_d3_h6_pins[]		= { GPIOH_6 };
static const unsigned int spi0_ss1_h7_pins[]		= { GPIOH_7 };
static const unsigned int spi0_ss0_h8_pins[]		= { GPIOH_8 };
static const unsigned int spi0_miso_h9_pins[]		= { GPIOH_9 };
static const unsigned int spi0_mosi_h10_pins[]		= { GPIOH_10 };
static const unsigned int spi0_clk_h11_pins[]		= { GPIOH_11 };
static const unsigned int spi0_ss2_h12_pins[]		= { GPIOH_12 };
static const unsigned int tdm_d3_h13_pins[]		= { GPIOH_13 };
static const unsigned int mclk_1_h_pins[]		= { GPIOH_14 };
static const unsigned int tdm_fs1_h_pins[]		= { GPIOH_15 };
static const unsigned int tdm_sclk1_h_pins[]		= { GPIOH_16 };
static const unsigned int tdm_d0_h_pins[]		= { GPIOH_17 };
static const unsigned int tdm_d1_h_pins[]		= { GPIOH_18 };
static const unsigned int tdm_d2_h_pins[]		= { GPIOH_19 };
static const unsigned int tdm_d3_h20_pins[]		= { GPIOH_20 };
static const unsigned int tdm_d4_h_pins[]		= { GPIOH_21 };
static const unsigned int tdm_d5_h_pins[]		= { GPIOH_22 };
static const unsigned int tdm_d6_h_pins[]		= { GPIOH_23 };

/* BANK H func3 */
static const unsigned int tcon_sfc_h0_pins[]		= { GPIOH_0 };
static const unsigned int tcon_sfc_h8_pins[]		= { GPIOH_8 };
static const unsigned int i2c2_sck_h10_pins[]		= { GPIOH_10 };
static const unsigned int i2c2_sda_h11_pins[]		= { GPIOH_11 };
static const unsigned int pwm_vs_h12_pins[]		= { GPIOH_12 };
static const unsigned int pwm_vs_h13_pins[]		= { GPIOH_13 };
static const unsigned int pdm_din1_h14_pins[]		= { GPIOH_14 };
static const unsigned int pdm_din2_h16_pins[]		= { GPIOH_16 };
static const unsigned int pdm_din3_h17_pins[]		= { GPIOH_17 };
static const unsigned int pdm_din0_h18_pins[]		= { GPIOH_18 };
static const unsigned int pdm_dclk_h19_pins[]		= { GPIOH_19 };
static const unsigned int loop_gpio_b13_pins[]		= { GPIOH_21 };
static const unsigned int pdm_din2_h22_pins[]		= { GPIOH_22 };
static const unsigned int pdm_dclk_h23_pins[]		= { GPIOH_23 };
static const unsigned int i2c2_sck_h24_pins[]		= { GPIOH_24 };
static const unsigned int i2c2_sda_h25_pins[]		= { GPIOH_25 };

/* BANK H func4 */
static const unsigned int vx1_a_lockn_pins[]		= { GPIOH_0 };
static const unsigned int pwm_d_h5_pins[]		= { GPIOH_5 };
static const unsigned int vx1_a_htpdn_pins[]		= { GPIOH_8 };
static const unsigned int vx1_b_lockn_pins[]		= { GPIOH_9 };
static const unsigned int vx1_b_htpdn_pins[]		= { GPIOH_10 };
static const unsigned int pwm_d_h12_pins[]		= { GPIOH_12 };
static const unsigned int pwm_e_h_pins[]		= { GPIOH_13 };
static const unsigned int tdm_d3_h14_pins[]		= { GPIOH_14 };
static const unsigned int eth_act_led_pins[]		= { GPIOH_18 };
static const unsigned int eth_link_led_pins[]		= { GPIOH_19 };
static const unsigned int uart_c_tx_h22_pins[]		= { GPIOH_22 };
static const unsigned int uart_c_rx_h23_pins[]		= { GPIOH_23 };
static const unsigned int mic_mute_key_h_pins[]		= { GPIOH_24 };
static const unsigned int mic_mute_led_h_pins[]		= { GPIOH_25 };

/* BANK H func5 */
static const unsigned int gen_clk_ee_h13_pins[]		= { GPIOH_13 };
static const unsigned int spi0_clk_h14_pins[]		= { GPIOH_14 };
static const unsigned int spi0_ss1_h19_pins[]		= { GPIOH_19 };
static const unsigned int spi2_ss0_h_pins[]		= { GPIOH_22 };
static const unsigned int spi2_miso_h_pins[]		= { GPIOH_23 };
static const unsigned int spi2_mosi_h_pins[]		= { GPIOH_24 };
static const unsigned int spi2_clk_h_pins[]		= { GPIOH_25 };

/* BANK H func6 */
//static const unsigned int gen_clk_ee_h13_pins[]		= { GPIOH_13 };
static const unsigned int s2_demod_gpio0_h_pins[]	= { GPIOH_14 };
static const unsigned int s2_demod_gpio1_h_pins[]	= { GPIOH_15 };
static const unsigned int s2_demod_gpio2_h_pins[]	= { GPIOH_16 };
static const unsigned int s2_demod_gpio3_h_pins[]	= { GPIOH_17 };
static const unsigned int s2_demod_gpio4_h_pins[]	= { GPIOH_18 };
static const unsigned int s2_demod_gpio5_h_pins[]	= { GPIOH_19 };
static const unsigned int s2_demod_gpio6_h_pins[]	= { GPIOH_20 };
static const unsigned int s2_demod_gpio7_h_pins[]	= { GPIOH_21 };
static const unsigned int tdm_fs2_h_pins[]		= { GPIOH_22 };
static const unsigned int tdm_sclk2_h_pins[]		= { GPIOH_23 };

/* BANK H func7 */
static const unsigned int uart_c_tx_h18_pins[]		= { GPIOH_18 };
static const unsigned int uart_c_rx_h19_pins[]		= { GPIOH_19 };

/* BANK M func1 */
static const unsigned int tsin_b_clk_pins[]		= { GPIOM_0 };
static const unsigned int tsin_b_sop_pins[]		= { GPIOM_1 };
static const unsigned int tsin_b_valid_pins[]		= { GPIOM_2 };
static const unsigned int tsin_b_d0_pins[]		= { GPIOM_3 };
static const unsigned int tsin_b_d1_pins[]		= { GPIOM_4 };
static const unsigned int tsin_b_d2_pins[]		= { GPIOM_5 };
static const unsigned int tsin_b_d3_pins[]		= { GPIOM_6 };
static const unsigned int tsin_b_d4_pins[]		= { GPIOM_7 };
static const unsigned int tsin_b_d5_pins[]		= { GPIOM_8 };
static const unsigned int tsin_b_d6_pins[]		= { GPIOM_9 };
static const unsigned int tsin_b_d7_pins[]		= { GPIOM_10 };
static const unsigned int cicam_data0_pins[]		= { GPIOM_11 };
static const unsigned int cicam_data1_pins[]		= { GPIOM_12 };
static const unsigned int cicam_data2_pins[]		= { GPIOM_13 };
static const unsigned int cicam_data3_pins[]		= { GPIOM_14 };
static const unsigned int cicam_data4_pins[]		= { GPIOM_15 };
static const unsigned int cicam_data5_pins[]		= { GPIOM_16 };
static const unsigned int cicam_data6_pins[]		= { GPIOM_17 };
static const unsigned int cicam_data7_pins[]		= { GPIOM_18 };
static const unsigned int cicam_a0_pins[]		= { GPIOM_19 };
static const unsigned int cicam_a1_pins[]		= { GPIOM_20 };
static const unsigned int cicam_cen_pins[]		= { GPIOM_21 };
static const unsigned int cicam_oen_pins[]		= { GPIOM_22 };
static const unsigned int cicam_wen_pins[]		= { GPIOM_23 };
static const unsigned int cicam_iordn_pins[]		= { GPIOM_24 };
static const unsigned int cicam_iowrn_pins[]		= { GPIOM_25 };
static const unsigned int cicam_reset_pins[]		= { GPIOM_26 };
static const unsigned int cicam_waitn_pins[]		= { GPIOM_29 };

/* BANK M func2 */
static const unsigned int pdm_din2_m_pins[]		= { GPIOM_8 };
static const unsigned int pdm_din3_m_pins[]		= { GPIOM_9 };
static const unsigned int pdm_din0_m_pins[]		= { GPIOM_10 };
static const unsigned int pdm_dclk_m_pins[]		= { GPIOH_11 };
static const unsigned int mclk_2_m_pins[]		= { GPIOM_12 };
static const unsigned int tdm_fs2_m_pins[]		= { GPIOM_13 };
static const unsigned int tdm_sclk2_m_pins[]		= { GPIOM_14 };
static const unsigned int tdm_d4_m_pins[]		= { GPIOM_15 };
static const unsigned int tdm_d5_m_pins[]		= { GPIOM_16 };
static const unsigned int tdm_d6_m_pins[]		= { GPIOM_17 };
static const unsigned int tdm_d7_m_pins[]		= { GPIOM_18 };
static const unsigned int iso7816_clk_m_pins[]		= { GPIOM_19 };
static const unsigned int iso7816_data_m_pins[]		= { GPIOM_20 };

/* BANK M func3 */
static const unsigned int spi0_miso_m_pins[]		= { GPIOM_19 };
static const unsigned int spi0_mosi_m_pins[]		= { GPIOM_20 };
static const unsigned int spi0_clk_m_pins[]		= { GPIOM_21 };
static const unsigned int spi0_ss0_m_pins[]		= { GPIOM_22 };
static const unsigned int spi0_ss1_m_pins[]		= { GPIOM_23 };
static const unsigned int spi0_ss2_m_pins[]		= { GPIOM_24 };
static const unsigned int i2c2_sck_m_pins[]		= { GPIOM_25 };
static const unsigned int i2c2_sda_m_pins[]		= { GPIOM_26 };
static const unsigned int i2c3_sck_m_pins[]		= { GPIOM_27 };
static const unsigned int i2c3_sda_m_pins[]		= { GPIOM_28 };

/* BANK M func4 */
static const unsigned int pwm_d_m1_pins[]		= { GPIOM_1 };
static const unsigned int pwm_g_m_pins[]		= { GPIOM_8 };
static const unsigned int pwm_h_m_pins[]		= { GPIOM_9 };
static const unsigned int uart_c_tx_m_pins[]		= { GPIOM_19 };
static const unsigned int uart_c_rx_m_pins[]		= { GPIOM_20 };
static const unsigned int uart_c_cts_m_pins[]		= { GPIOM_21 };
static const unsigned int uart_c_rts_m_pins[]		= { GPIOM_22 };
static const unsigned int pwm_d_m23_pins[]		= { GPIOM_23 };
static const unsigned int pwm_e_m_pins[]		= { GPIOM_24 };
static const unsigned int pwm_f_m_pins[]		= { GPIOM_26 };

/* BANK M func5 */
static const unsigned int demod_uart_tx_m12_pins[]	= { GPIOM_12 };
static const unsigned int demod_uart_rx_m13_pins[]	= { GPIOM_13 };
static const unsigned int demod_uart_tx_m14_pins[]	= { GPIOM_14 };
static const unsigned int demod_uart_rx_m15_pins[]	= { GPIOM_15 };
static const unsigned int spdif_in_m18_pins[]		= { GPIOM_18 };
static const unsigned int spdif_in_m29_pins[]		= { GPIOM_29 };

static struct meson_pmx_group meson_t5w_periphs_groups[] __initdata = {
	GPIO_GROUP(GPIOW_0),
	GPIO_GROUP(GPIOW_1),
	GPIO_GROUP(GPIOW_2),
	GPIO_GROUP(GPIOW_3),
	GPIO_GROUP(GPIOW_4),
	GPIO_GROUP(GPIOW_5),
	GPIO_GROUP(GPIOW_6),
	GPIO_GROUP(GPIOW_7),
	GPIO_GROUP(GPIOW_8),
	GPIO_GROUP(GPIOW_9),
	GPIO_GROUP(GPIOW_10),
	GPIO_GROUP(GPIOW_11),
	GPIO_GROUP(GPIOW_12),
	GPIO_GROUP(GPIOB_0),
	GPIO_GROUP(GPIOB_1),
	GPIO_GROUP(GPIOB_2),
	GPIO_GROUP(GPIOB_3),
	GPIO_GROUP(GPIOB_4),
	GPIO_GROUP(GPIOB_5),
	GPIO_GROUP(GPIOB_6),
	GPIO_GROUP(GPIOB_7),
	GPIO_GROUP(GPIOB_8),
	GPIO_GROUP(GPIOB_9),
	GPIO_GROUP(GPIOB_10),
	GPIO_GROUP(GPIOB_11),
	GPIO_GROUP(GPIOB_12),
	GPIO_GROUP(GPIOB_13),
	GPIO_GROUP(GPIOC_0),
	GPIO_GROUP(GPIOC_1),
	GPIO_GROUP(GPIOC_2),
	GPIO_GROUP(GPIOC_3),
	GPIO_GROUP(GPIOC_4),
	GPIO_GROUP(GPIOC_5),
	GPIO_GROUP(GPIOC_6),
	GPIO_GROUP(GPIOC_7),
	GPIO_GROUP(GPIOC_8),
	GPIO_GROUP(GPIOC_9),
	GPIO_GROUP(GPIOC_10),
	GPIO_GROUP(GPIOC_11),
	GPIO_GROUP(GPIOC_12),
	GPIO_GROUP(GPIOC_13),
	GPIO_GROUP(GPIOC_14),
	GPIO_GROUP(GPIOZ_0),
	GPIO_GROUP(GPIOZ_1),
	GPIO_GROUP(GPIOZ_2),
	GPIO_GROUP(GPIOZ_3),
	GPIO_GROUP(GPIOZ_4),
	GPIO_GROUP(GPIOZ_5),
	GPIO_GROUP(GPIOZ_6),
	GPIO_GROUP(GPIOZ_7),
	GPIO_GROUP(GPIOZ_8),
	GPIO_GROUP(GPIOZ_9),
	GPIO_GROUP(GPIOZ_10),
	GPIO_GROUP(GPIOZ_11),
	GPIO_GROUP(GPIOZ_12),
	GPIO_GROUP(GPIOZ_13),
	GPIO_GROUP(GPIOZ_14),
	GPIO_GROUP(GPIOZ_15),
	GPIO_GROUP(GPIOZ_16),
	GPIO_GROUP(GPIOZ_17),
	GPIO_GROUP(GPIOZ_18),
	GPIO_GROUP(GPIOZ_19),
	GPIO_GROUP(GPIOH_0),
	GPIO_GROUP(GPIOH_1),
	GPIO_GROUP(GPIOH_2),
	GPIO_GROUP(GPIOH_3),
	GPIO_GROUP(GPIOH_4),
	GPIO_GROUP(GPIOH_5),
	GPIO_GROUP(GPIOH_6),
	GPIO_GROUP(GPIOH_7),
	GPIO_GROUP(GPIOH_8),
	GPIO_GROUP(GPIOH_9),
	GPIO_GROUP(GPIOH_10),
	GPIO_GROUP(GPIOH_11),
	GPIO_GROUP(GPIOH_12),
	GPIO_GROUP(GPIOH_13),
	GPIO_GROUP(GPIOH_14),
	GPIO_GROUP(GPIOH_15),
	GPIO_GROUP(GPIOH_16),
	GPIO_GROUP(GPIOH_17),
	GPIO_GROUP(GPIOH_18),
	GPIO_GROUP(GPIOH_19),
	GPIO_GROUP(GPIOH_20),
	GPIO_GROUP(GPIOH_21),
	GPIO_GROUP(GPIOH_22),
	GPIO_GROUP(GPIOH_23),
	GPIO_GROUP(GPIOH_24),
	GPIO_GROUP(GPIOH_25),
	GPIO_GROUP(GPIOM_0),
	GPIO_GROUP(GPIOM_1),
	GPIO_GROUP(GPIOM_2),
	GPIO_GROUP(GPIOM_3),
	GPIO_GROUP(GPIOM_4),
	GPIO_GROUP(GPIOM_5),
	GPIO_GROUP(GPIOM_6),
	GPIO_GROUP(GPIOM_7),
	GPIO_GROUP(GPIOM_8),
	GPIO_GROUP(GPIOM_9),
	GPIO_GROUP(GPIOM_10),
	GPIO_GROUP(GPIOM_11),
	GPIO_GROUP(GPIOM_12),
	GPIO_GROUP(GPIOM_13),
	GPIO_GROUP(GPIOM_14),
	GPIO_GROUP(GPIOM_15),
	GPIO_GROUP(GPIOM_16),
	GPIO_GROUP(GPIOM_17),
	GPIO_GROUP(GPIOM_18),
	GPIO_GROUP(GPIOM_19),
	GPIO_GROUP(GPIOM_20),
	GPIO_GROUP(GPIOM_21),
	GPIO_GROUP(GPIOM_22),
	GPIO_GROUP(GPIOM_23),
	GPIO_GROUP(GPIOM_24),
	GPIO_GROUP(GPIOM_25),
	GPIO_GROUP(GPIOM_26),
	GPIO_GROUP(GPIOM_27),
	GPIO_GROUP(GPIOM_28),
	GPIO_GROUP(GPIOM_29),

	/* bank B */
	GROUP(emmc_nand_d0,		1),
	GROUP(emmc_nand_d1,		1),
	GROUP(emmc_nand_d2,		1),
	GROUP(emmc_nand_d3,		1),
	GROUP(emmc_nand_d4,		1),
	GROUP(emmc_nand_d5,		1),
	GROUP(emmc_nand_d6,		1),
	GROUP(emmc_nand_d7,		1),
	GROUP(emmc_clk,			1),
	GROUP(emmc_cmd,			1),
	GROUP(emmc_nand_ds,		1),
	GROUP(loop_gpio_b12,		1),

	GROUP(nand_wen_clk,		2),
	GROUP(nand_ale,			2),
	GROUP(nand_ren_wr,		2),
	GROUP(nand_cle,			2),
	GROUP(nand_ce0,			2),
	GROUP(loop_gpio_h21,		2),

	GROUP(spif_hold,		3),
	GROUP(spif_mo,			3),
	GROUP(spif_mi,			3),
	GROUP(spif_clk,			3),
	GROUP(spif_wp,			3),
	GROUP(spif_cs,			3),
	GROUP(remote_out,		3),

	/*bank W */
	GROUP(hdmirx_a_hpd,		1),
	GROUP(hdmirx_a_det,		1),
	GROUP(hdmirx_a_sda,		1),
	GROUP(hdmirx_a_sck,		1),
	GROUP(hdmirx_c_hpd,		1),
	GROUP(hdmirx_c_det,		1),
	GROUP(hdmirx_c_sda,		1),
	GROUP(hdmirx_c_sck,		1),
	GROUP(hdmirx_b_hpd,		1),
	GROUP(hdmirx_b_det,		1),
	GROUP(hdmirx_b_sda,		1),
	GROUP(hdmirx_b_sck,		1),
	GROUP(cec,			1),

	GROUP(uart_b_tx_w2,		2),
	GROUP(uart_b_rx_w3,		2),
	GROUP(uart_b_tx_w6,		2),
	GROUP(uart_b_rx_w7,		2),
	GROUP(uart_b_tx_w10,		2),
	GROUP(uart_b_rx_w11,		2),

	/* bank C */
	GROUP(sdcard_d0,		1),
	GROUP(sdcard_d1,		1),
	GROUP(sdcard_d2,		1),
	GROUP(sdcard_d3,		1),
	GROUP(sdcard_clk,		1),
	GROUP(sdcard_cmd,		1),
	GROUP(uart_a_tx_c,		1),
	GROUP(uart_a_rx_c,		1),
	GROUP(uart_a_cts_c,		1),
	GROUP(uart_a_rts_c,		1),
	GROUP(pwm_f_c,			1),
	GROUP(pwm_a,			1),
	GROUP(pwm_c,			1),
	GROUP(i2c1_sck_c,		1),
	GROUP(i2c1_sda_c,		1),

	GROUP(spi2_miso_c,		2),
	GROUP(spi2_mosi_c,		2),
	GROUP(spi2_clk_c,		2),
	GROUP(spi2_ss0_c,		2),
	GROUP(spi2_ss1_c,		2),
	GROUP(spi2_ss2_c,		2),

	GROUP(cicam_a2_c,		3),
	GROUP(cicam_a3_c,		3),
	GROUP(cicam_a4_c,		3),
	GROUP(cicam_a5_c,		3),
	GROUP(cicam_a6_c,		3),
	GROUP(cicam_a7_c,		3),
	GROUP(cicam_a8_c,		3),
	GROUP(cicam_a9_c,		3),
	GROUP(cicam_a10_c,		3),
	GROUP(cicam_a11_c,		3),
	GROUP(cicam_a12_c,		3),

	GROUP(demod_uart_tx_c,		4),
	GROUP(demod_uart_rx_c,		4),

	GROUP(tdm_fs1_c,		5),
	GROUP(tdm_sclk1_c,		5),
	GROUP(tdm_d4_c,			5),
	GROUP(tdm_d5_c,			5),
	GROUP(tdm_d6_c,			5),
	GROUP(tdm_d7_c,			5),

	/* bank Z */
	GROUP(tdm_fs2_z,		1),
	GROUP(tdm_sclk2_z,		1),
	GROUP(tdm_d4_z,			1),
	GROUP(tdm_d5_z,			1),
	GROUP(tdm_d6_z,			1),
	GROUP(tdm_d7_z,			1),
	GROUP(mclk_2_z,			1),
	GROUP(tsout_clk,		1),
	GROUP(tsout_sop,		1),
	GROUP(tsout_valid,		1),
	GROUP(tsout_d0,			1),
	GROUP(tsout_d1,			1),
	GROUP(tsout_d2,			1),
	GROUP(tsout_d3,			1),
	GROUP(tsout_d4,			1),
	GROUP(tsout_d5,			1),
	GROUP(tsout_d6,			1),
	GROUP(tsout_d7,			1),
	GROUP(mclk_1_z,			1),
	GROUP(spdif_out_a_z19,		1),

	GROUP(tsin_a_clk,		2),
	GROUP(tsin_a_sop,		2),
	GROUP(tsin_a_valid,		2),
	GROUP(tsin_a_d0,		2),
	GROUP(dtv_rf_agc_z6,		2),
	GROUP(cicam_a2,			2),
	GROUP(cicam_a3,			2),
	GROUP(cicam_a4,			2),
	GROUP(cicam_a5,			2),
	GROUP(cicam_a6,			2),
	GROUP(cicam_a7,			2),
	GROUP(cicam_a8,			2),
	GROUP(cicam_a9,			2),
	GROUP(cicam_a10,		2),
	GROUP(cicam_a11,		2),
	GROUP(dtv_rf_agc_z18,		2),
	GROUP(spdif_in_z,		2),

	GROUP(iso7816_clk_z,		3),
	GROUP(iso7816_data_z,		3),
	GROUP(cicam_a13,		3),
	GROUP(cicam_a14,		3),
	GROUP(i2c0_sck_z,		3),
	GROUP(i2c0_sda_z,		3),
	GROUP(atv_if_agc_z6,		3),
	GROUP(atv_if_agc_z18,		3),
	GROUP(spdif_out_b,		3),

	GROUP(spi0_miso_z,		4),
	GROUP(spi0_mosi_z,		4),
	GROUP(spi0_clk_z,		4),
	GROUP(spi0_ss0_z,		4),
	GROUP(spi0_ss1_z,		4),
	GROUP(spi0_ss2_z,		4),
	GROUP(dtv_if_agc_z6,		4),
	GROUP(dtv_if_agc_z18,		4),

	GROUP(uart_a_tx_z,		5),
	GROUP(uart_a_rx_z,		5),
	GROUP(uart_a_cts_z,		5),
	GROUP(uart_a_rts_z,		5),
	GROUP(pwm_b_z,			5),
	GROUP(pwm_d_z,			5),
	GROUP(pwm_e_z,			5),
	GROUP(pwm_g_z,			5),
	GROUP(pwm_h_z,			5),
	GROUP(spi1_ss0_z,		5),
	GROUP(spi1_miso_z,		5),

	GROUP(cicam_a12,		6),
	GROUP(pdm_din1_z1,		6),
	GROUP(pdm_dclk_z,		6),
	GROUP(pdm_din0_z,		6),
	GROUP(pdm_din1_z6,		6),
	GROUP(tdm_d1_z,			6),
	GROUP(tdm_d0_z,			6),
	GROUP(tdm_sclk1_z,		6),
	GROUP(tdm_fs1_z,		6),
	GROUP(diseqc_out_z18,		6),

	GROUP(diseqc_out_z0,		7),
	GROUP(s2_demod_gpio0_z,		7),
	GROUP(spdif_out_a_z2,		7),
	GROUP(spi0_ss1_z3,		7),
	GROUP(mic_mute_key_z,		7),
	GROUP(mic_mute_led_z,		7),

	/* bank H */
	GROUP(tcon_0,			1),
	GROUP(tcon_1,			1),
	GROUP(tcon_2,			1),
	GROUP(tcon_3,			1),
	GROUP(tcon_4,			1),
	GROUP(tcon_5,			1),
	GROUP(tcon_6,			1),
	GROUP(tcon_7,			1),
	GROUP(tcon_8,			1),
	GROUP(tcon_9,			1),
	GROUP(tcon_10,			1),
	GROUP(tcon_11,			1),
	GROUP(tcon_12,			1),
	GROUP(tcon_13,			1),
	GROUP(tcon_14,			1),
	GROUP(tcon_15,			1),
	GROUP(hsync,			1),
	GROUP(vsync,			1),
	GROUP(i2c2_sck_h20,		1),
	GROUP(i2c2_sda_h21,		1),
	GROUP(i2c3_sck_h22,		1),
	GROUP(i2c3_sda_h23,		1),
	GROUP(i2c3_sck_h24,		1),
	GROUP(i2c3_sda_h25,		1),

	GROUP(tcon_lock,		2),
	GROUP(uart_c_tx_h1,		2),
	GROUP(uart_c_rx_h2,		2),
	GROUP(uart_c_cts_h,		2),
	GROUP(uart_c_rts_h,		2),
	GROUP(sync_3d_out,		2),
	GROUP(tdm_d3_h6,		2),
	GROUP(spi0_ss1_h7,		2),
	GROUP(spi0_ss0_h8,		2),
	GROUP(spi0_miso_h9,		2),
	GROUP(spi0_mosi_h10,		2),
	GROUP(spi0_clk_h11,		2),
	GROUP(spi0_ss2_h12,		2),
	GROUP(tdm_d3_h13,		2),
	GROUP(mclk_1_h,			2),
	GROUP(tdm_sclk1_h,		2),
	GROUP(tdm_fs1_h,		2),
	GROUP(tdm_d0_h,			2),
	GROUP(tdm_d1_h,			2),
	GROUP(tdm_d2_h,			2),
	GROUP(tdm_d3_h20,		2),
	GROUP(tdm_d4_h,			2),
	GROUP(tdm_d5_h,			2),
	GROUP(tdm_d6_h,			2),

	GROUP(tcon_sfc_h0,		3),
	GROUP(tcon_sfc_h8,		3),
	GROUP(i2c2_sck_h10,		3),
	GROUP(i2c2_sda_h11,		3),
	GROUP(pwm_vs_h12,		3),
	GROUP(pwm_vs_h13,		3),
	GROUP(pdm_din1_h14,		3),
	GROUP(pdm_din2_h16,		3),
	GROUP(pdm_din3_h17,		3),
	GROUP(pdm_din0_h18,		3),
	GROUP(pdm_dclk_h19,		3),
	GROUP(loop_gpio_b13,		3),
	GROUP(pdm_din2_h22,		3),
	GROUP(pdm_dclk_h23,		3),
	GROUP(i2c2_sck_h24,		3),
	GROUP(i2c2_sda_h25,		3),

	GROUP(vx1_a_lockn,		4),
	GROUP(pwm_d_h5,			4),
	GROUP(vx1_a_htpdn,		4),
	GROUP(vx1_b_lockn,		4),
	GROUP(vx1_b_htpdn,		4),
	GROUP(pwm_d_h12,		4),
	GROUP(pwm_e_h,			4),
	GROUP(tdm_d3_h14,		4),
	GROUP(eth_act_led,		4),
	GROUP(eth_link_led,		4),
	GROUP(uart_c_tx_h22,		4),
	GROUP(uart_c_rx_h23,		4),
	GROUP(mic_mute_key_h,		4),
	GROUP(mic_mute_led_h,		4),

	GROUP(gen_clk_ee_h13,		5),
	GROUP(spi0_clk_h14,		5),
	GROUP(spi0_ss1_h19,		5),
	GROUP(spi2_ss0_h,		5),
	GROUP(spi2_miso_h,		5),
	GROUP(spi2_mosi_h,		5),
	GROUP(spi2_clk_h,		5),

	GROUP(s2_demod_gpio0_h,		6),
	GROUP(s2_demod_gpio1_h,		6),
	GROUP(s2_demod_gpio2_h,		6),
	GROUP(s2_demod_gpio3_h,		6),
	GROUP(s2_demod_gpio4_h,		6),
	GROUP(s2_demod_gpio5_h,		6),
	GROUP(s2_demod_gpio6_h,		6),
	GROUP(s2_demod_gpio7_h,		6),
	GROUP(tdm_fs2_h,		6),
	GROUP(tdm_sclk2_h,		6),

	GROUP(uart_c_tx_h18,		7),
	GROUP(uart_c_rx_h19,		7),

	/* BANK M func1 */
	GROUP(tsin_b_clk,		1),
	GROUP(tsin_b_sop,		1),
	GROUP(tsin_b_valid,		1),
	GROUP(tsin_b_d0,		1),
	GROUP(tsin_b_d1,		1),
	GROUP(tsin_b_d2,		1),
	GROUP(tsin_b_d3,		1),
	GROUP(tsin_b_d4,		1),
	GROUP(tsin_b_d5,		1),
	GROUP(tsin_b_d6,		1),
	GROUP(tsin_b_d7,		1),
	GROUP(cicam_data0,		1),
	GROUP(cicam_data1,		1),
	GROUP(cicam_data2,		1),
	GROUP(cicam_data3,		1),
	GROUP(cicam_data4,		1),
	GROUP(cicam_data5,		1),
	GROUP(cicam_data6,		1),
	GROUP(cicam_data7,		1),
	GROUP(cicam_a0,			1),
	GROUP(cicam_a1,			1),
	GROUP(cicam_cen,		1),
	GROUP(cicam_oen,		1),
	GROUP(cicam_wen,		1),
	GROUP(cicam_iordn,		1),
	GROUP(cicam_iowrn,		1),
	GROUP(cicam_reset,		1),
	GROUP(cicam_waitn,		1),

	/* BANK M func2 */
	GROUP(pdm_din2_m,		2),
	GROUP(pdm_din3_m,		2),
	GROUP(pdm_din0_m,		2),
	GROUP(pdm_dclk_m,		2),
	GROUP(mclk_2_m,			2),
	GROUP(tdm_fs2_m,		2),
	GROUP(tdm_sclk2_m,		2),
	GROUP(tdm_d4_m,			2),
	GROUP(tdm_d5_m,			2),
	GROUP(tdm_d6_m,			2),
	GROUP(tdm_d7_m,			2),
	GROUP(iso7816_clk_m,		2),
	GROUP(iso7816_data_m,		2),

	/* BANK M func3 */
	GROUP(spi0_miso_m,		3),
	GROUP(spi0_mosi_m,		3),
	GROUP(spi0_clk_m,		3),
	GROUP(spi0_ss0_m,		3),
	GROUP(spi0_ss1_m,		3),
	GROUP(spi0_ss2_m,		3),
	GROUP(i2c2_sck_m,		3),
	GROUP(i2c2_sda_m,		3),
	GROUP(i2c3_sck_m,		3),
	GROUP(i2c3_sda_m,		3),

	/* BANK M func4 */
	GROUP(pwm_d_m1,			4),
	GROUP(pwm_g_m,			4),
	GROUP(pwm_h_m,			4),
	GROUP(uart_c_tx_m,		4),
	GROUP(uart_c_rx_m,		4),
	GROUP(uart_c_cts_m,		4),
	GROUP(uart_c_rts_m,		4),
	GROUP(pwm_d_m23,		4),
	GROUP(pwm_e_m,			4),
	GROUP(pwm_f_m,			4),

	/* BANK M func5 */
	GROUP(demod_uart_tx_m12,	5),
	GROUP(demod_uart_rx_m13,	5),
	GROUP(demod_uart_tx_m14,	5),
	GROUP(demod_uart_rx_m15,	5),
	GROUP(spdif_in_m18,		5),
	GROUP(spdif_in_m29,		5),
};

static const char * const gpio_periphs_groups[] = {
	"GPIOW_0", "GPIOW_1", "GPIOW_2", "GPIOW_3", "GPIOW_4", "GPIOW_5",
	"GPIOW_6", "GPIOW_7", "GPIOW_8", "GPIOW_9", "GPIOW_10", "GPIOW_11",
	"GPIOW_12",

	"GPIOB_0", "GPIOB_1", "GPIOB_2", "GPIOB_3", "GPIOB_4", "GPIOB_5",
	"GPIOB_6", "GPIOB_7", "GPIOB_8", "GPIOB_9", "GPIOB_10", "GPIOB_11",
	"GPIOB_12", "GPIOB_13",

	"GPIOC_0", "GPIOC_1", "GPIOC_2", "GPIOC_3", "GPIOC_4", "GPIOC_5",
	"GPIOC_6", "GPIOC_7", "GPIOC_8", "GPIOC_9", "GPIOC_10", "GPIOC_11",
	"GPIOC_12", "GPIOC_13", "GPIOC_14",

	"GPIOZ_0", "GPIOZ_1", "GPIOZ_2", "GPIOZ_3", "GPIOZ_4", "GPIOZ_5",
	"GPIOZ_6", "GPIOZ_7", "GPIOZ_8", "GPIOZ_9", "GPIOZ_10", "GPIOZ_11",
	"GPIOZ_12", "GPIOZ_13", "GPIOZ_14", "GPIOZ_15", "GPIOZ_16", "GPIOZ_17",
	"GPIOZ_18", "GPIOZ_19",

	"GPIOH_0", "GPIOH_1", "GPIOH_2", "GPIOH_3", "GPIOH_4", "GPIOH_5",
	"GPIOH_6", "GPIOH_7", "GPIOH_8", "GPIOH_9", "GPIOH_10", "GPIOH_11",
	"GPIOH_12", "GPIOH_13", "GPIOH_14", "GPIOH_15", "GPIOH_16", "GPIOH_17",
	"GPIOH_18", "GPIOH_19", "GPIOH_20", "GPIOH_21", "GPIOH_22", "GPIOH_23",
	"GPIOH_24", "GPIOH_25",

	"GPIOM_0", "GPIOM_1", "GPIOM_2", "GPIOM_3", "GPIOM_4", "GPIOM_5",
	"GPIOM_6", "GPIOM_7", "GPIOM_8", "GPIOM_9", "GPIOM_10", "GPIOM_11",
	"GPIOM_12", "GPIOM_13", "GPIOM_14", "GPIOM_15", "GPIOM_16", "GPIOM_17",
	"GPIOM_18", "GPIOM_19", "GPIOM_20", "GPIOM_21", "GPIOM_22", "GPIOM_23",
	"GPIOM_24", "GPIOM_25", "GPIOM_26", "GPIOM_27", "GPIOM_28", "GPIOM_29"
};

static const char * const emmc_groups[] = {
	"emmc_nand_d0", "emmc_nand_d1", "emmc_nand_d2",
	"emmc_nand_d3", "emmc_nand_d4", "emmc_nand_d5",
	"emmc_nand_d6", "emmc_nand_d7", "emmc_clk",
	"emmc_cmd", "emmc_nand_ds"
};

static const char * const nand_groups[] = {
	"emmc_nand_d0", "emmc_nand_d1", "emmc_nand_d2",
	"emmc_nand_d3", "emmc_nand_d4", "emmc_nand_d5",
	"emmc_nand_d6", "emmc_nand_d7",
	"nand_ce0", "nand_ale", "nand_cle",
	"nand_wen_clk", "nand_ren_wr", "emmc_nand_ds"
};

static const char * const spif_groups[] = {
	"spif_hold", "spif_mo", "spif_mi", "spif_clk",
	"spif_wp", "spif_cs"
};

static const char * const remote_out_groups[] = {
	"remote_out"
};

static const char * const hdmirx_a_groups[] = {
	"hdmirx_a_hpd", "hdmirx_a_det", "hdmirx_a_sda", "hdmirx_a_sck"
};

static const char * const hdmirx_b_groups[] = {
	"hdmirx_b_hpd", "hdmirx_b_det", "hdmirx_b_sda", "hdmirx_b_sck"

};

static const char * const hdmirx_c_groups[] = {
	"hdmirx_c_hpd", "hdmirx_c_det", "hdmirx_c_sda", "hdmirx_c_sck"
};

static const char * const cec_groups[] = {
	"cec"
};

static const char * const uart_a_groups[] = {
	"uart_a_tx_c", "uart_a_rx_c", "uart_a_cts_c", "uart_a_rts_c",
	"uart_a_tx_z", "uart_a_rx_z", "uart_a_cts_z", "uart_a_rts_z"
};

static const char * const uart_b_groups[] = {
	"uart_b_tx_w2", "uart_b_rx_w3", "uart_b_tx_w6", "uart_b_rx_w7",
	"uart_b_tx_w10", "uart_b_rx_w11"

};

static const char * const uart_c_groups[] = {
	"uart_c_tx_h1", "uart_c_rx_h2", "uart_c_cts_h", "uart_c_rts_h",
	"uart_c_tx_h22", "uart_c_rx_h23", "uart_c_tx_h18", "uart_c_rx_h19",

	"uart_c_tx_m", "uart_c_rx_m", "uart_c_cts_m", "uart_c_rts_m",
};

static const char * const sdcard_groups[] = {
	"sdcard_d0", "sdcard_d1", "sdcard_d2", "sdcard_d3", "sdcard_clk",
	"sdcard_cmd"
};

static const char * const tdm_groups[] = {
	"tdm_fs1_c", "tdm_sclk1_c", "tdm_d4_c", "tdm_d5_c", "tdm_d6_c",
	"tdm_d7_c",

	"tdm_fs2_z", "tdm_sclk2_z", "tdm_d4_z", "tdm_d5_z", "tdm_d6_z",
	"tdm_d7_z", "tdm_d1_z", "tdm_d0_z", "tdm_sclk1_z", "tdm_fs1_z",

	"tdm_sclk1_h", "tdm_fs1_h", "tdm_d0_h", "tdm_d1_h", "tdm_d2_h",
	"tdm_d3_h14", "tdm_fs2_h", "tdm_sclk2_h", "tdm_d3_h6", "tdm_d3_h13",
	"tdm_d4_h", "tdm_d5_h", "tdm_d6_h",

	"tdm_fs2_m", "tdm_sclk2_m", "tdm_d4_m", "tdm_d5_m", "tdm_d6_m",
	"tdm_d7_m"
};

static const char * const i2c0_groups[] = {
	"i2c0_sck_z", "i2c0_sda_z"
};

static const char * const i2c1_groups[] = {
	"i2c1_sck_c", "i2c1_sda_c"
};

static const char * const i2c2_groups[] = {
	"i2c2_sck_h20", "i2c2_sda_h21", "i2c2_sck_h10", "i2c2_sda_h11",
	"i2c2_sck_h24", "i2c2_sda_h25", "i2c2_sck_m", "i2c2_sck_m"
};

static const char * const i2c3_groups[] = {
	"i2c3_sck_h22", "i2c3_sda_h23", "i2c3_sck_h24", "i2c3_sda_h25",
};

static const char * const atv_groups[] = {
	"atv_if_agc_z6", "atv_if_agc_z18"
};

static const char * const dtv_groups[] = {
	"dtv_rf_agc_z6", "dtv_rf_agc_z18", "dtv_if_agc_z6", "dtv_if_agc_z18"
};

static const char * const pwm_a_groups[] = {
	"pwm_a"
};

static const char * const pwm_b_groups[] = {
	"pwm_b_z", "pwm_b_h"
};

static const char * const pwm_c_groups[] = {
	"pwm_c"
};

static const char * const pwm_d_groups[] = {
	"pwm_d_z", "pwm_d_h5", "pwm_d_h12", "pwm_d_m1", "pwm_d_m23"
};

static const char * const pwm_e_groups[] = {
	"pwm_e_z", "pwm_e_h", "pwm_e_m"
};

static const char * const pwm_f_groups[] = {
	"pwm_f_c", "pwm_f_z", "pwm_f_m",
};

static const char * const pwm_g_groups[] = {
	"pwm_g_z", "pwm_g_m"
};

static const char * const pwm_h_groups[] = {
	"pwm_h_z", "pwm_h_m"
};

static const char * const pwm_vs_groups[] = {
	"pwm_vs_h12", "pwm_vs_h13"
};

static const char * const pdm_groups[] = {
	"pdm_din1_z1", "pdm_dclk_z", "pdm_din0_z", "pdm_din1_z6",
	"pdm_din1_h14", "pdm_din2_h16", "pdm_din3_h17", "pdm_din0_h18",
	"pdm_dclk_h19", "pdm_din2_h22", "pdm_dclk_h23",

	"pdm_din2_m", "pdm_din3_m", "pdm_din0_m", "pdm_dclk_m",
};

static const char * const tcon_groups[] = {
	"tcon_0", "tcon_1", "tcon_2", "tcon_3",
	"tcon_4", "tcon_5", "tcon_6", "tcon_7",
	"tcon_8", "tcon_9", "tcon_10", "tcon_11",
	"tcon_12", "tcon_13", "tcon_14", "tcon_15",
	"tcon_lock", "tcon_sfc_h0", "tcon_sfc_h8"
};

static const char * const hsync_groups[] = {
	"hsync"
};

static const char * const vsync_groups[] = {
	"vsync"
};

static const char * const sync_3d_out_groups[] = {
	"sync_3d_out"
};

static const char * const spi0_groups[] = {
	"spi0_ss1_h8", "spi0_miso_h9", "spi0_mosi_h10", "spi0_clk_h11",
	"spi0_ss2_h12", "spi0_clk_h14", "spi0_ss1_h19", "spi0_ss0_h7",

	"spi0_miso_z", "spi0_mosi_z", "spi0_clk_z", "spi0_ss0_z",
	"spi0_ss1_z", "spi0_ss2_z", "spi0_ss1_z3",

	"spi0_miso_m", "spi0_mosi_m", "spi0_clk_m", "spi0_ss0_m", "spi0_ss1_m",
	"spi0_ss2_m"
};

static const char * const spi1_groups[] = {
	"spi1_ss0_z", "spi1_miso_z",
};

static const char * const spi2_groups[] = {
	"spi2_miso_c", "spi2_mosi_c", "spi2_clk_c", "spi2_ss0_c",
	"spi2_ss1_c", "spi2_ss2_c",

	"spi2_ss0_h", "spi2_miso_h", "spi2_mosi_h", "spi2_clk_h",
};

static const char * const mclk_groups[] = {
	"mclk_2_z", "mclk_1_z", "mclk_1_h", "mclk_2_m"
};

static const char * const spdif_out_a_groups[] = {
	"spdif_out_a_z19", "spdif_out_a_z2",
};

static const char * const spdif_out_b_groups[] = {
	"spdif_out_b"
};

static const char * const spdif_in_groups[] = {
	"spdif_in_z", "spdif_in_m18", "spdif_in_m29"
};

static const char * const vx1_groups[] = {
	"vx1_a_lockn", "vx1_a_htpdn", "vx1_b_lockn", "vx1_b_htpdn",
};

static const char * const eth_groups[] = {
	"eth_act_led", "eth_link_led"
};

static const char * const cicam_groups[] = {
	"cicam_a0", "cicam_a1", "cicam_a2", "cicam_a3", "cicam_a4",
	"cicam_a5", "cicam_a6", "cicam_a7", "cicam_a8", "cicam_a9",
	"cicam_a10", "cicam_a11", "cicam_a12", "cicam_a13", "cicam_a14",
	"cicam_a2_c", "cicam_a3_c", "cicam_a4_c",
	"cicam_a5_c", "cicam_a6_c", "cicam_a7_c", "cicam_a8_c", "cicam_a9_c",
	"cicam_a10_c", "cicam_a11_c", "cicam_a12_c",

	"cicam_data0", "cicam_data1", "cicam_data2", "cicam_data3",
	"cicam_data4", "cicam_data5", "cicam_data6", "cicam_data7",

	"cicam_cen", "cicam_oen", "cicam_wen", "cicam_iordn", "cicam_iowrn",
	"cicam_reset", "cicam_waitn"
};

static const char * const iso7816_groups[] = {
	"iso7816_clk_z", "iso7816_data_z", "iso7816_clk_m", "iso7816_data_m"
};

static const char * const tsout_groups[] = {
	"tsout_clk", "tsout_sop", "tsout_valid", "tsout_d0", "tsout_d1",
	"tsout_d2", "tsout_d3", "tsout_d4", "tsout_d5", "tsout_d6", "tsout_d7"
};

static const char * const tsin_a_groups[] = {
	"tsin_a_clk", "tsin_a_sop", "tsin_a_valid", "tsin_a_d0"
};

static const char * const tsin_b_groups[] = {
	"tsin_b_clk", "tsin_b_sop", "tsin_b_valid", "tsin_b_d0", "tsin_b_d1",
	"tsin_b_d2", "tsin_b_d3", "tsin_b_d4", "tsin_b_d5", "tsin_b_d6",
	"tsin_b_d7"
};

static const char * const diseqc_out_groups[] = {
	"diseqc_out_z0", "diseqc_out_z18"
};

static const char * const s2_demod_groups[] = {
	"s2_demod_gpio0_z", "s2_demod_gpio0_m", "s2_demod_gpio1",
	"s2_demod_gpio2", "s2_demod_gpio3", "s2_demod_gpio4",
	"s2_demod_gpio5", "s2_demod_gpio6", "s2_demod_gpio7",

	"s2_demod_gpio0_h", "s2_demod_gpio1_h", "s2_demod_gpio2_h",
	"s2_demod_gpio3_h", "s2_demod_gpio4_h", "s2_demod_gpio5_h",
	"s2_demod_gpio6_h", "s2_demod_gpio7_h"
};

static const char * const demod_uart_groups[] = {
	"demod_uart_tx_c", "demod_uart_rx_c",
	"demod_uart_tx_m12", "demod_uart_rx_m13",
	"demod_uart_tx_m14", "demod_uart_rx_m15"
};

static const char * const diseqc_groups[] = {
	"diseqc_out_z18", "diseqc_out_z0"
};

static const char * const mic_mute_groups[] = {
	"mic_mute_key_z", "mic_mute_led_z",
	"mic_mute_key_h", "mic_mute_led_h"
};

static struct meson_pmx_func meson_t5w_periphs_functions[] __initdata = {
	FUNCTION(gpio_periphs),
	FUNCTION(emmc),
	FUNCTION(nand),
	FUNCTION(spif),
	FUNCTION(remote_out),
	FUNCTION(hdmirx_a),
	FUNCTION(hdmirx_b),
	FUNCTION(hdmirx_c),
	FUNCTION(cec),
	FUNCTION(uart_a),
	FUNCTION(uart_b),
	FUNCTION(uart_c),
	FUNCTION(sdcard),
	FUNCTION(tdm),
	FUNCTION(i2c0),
	FUNCTION(i2c1),
	FUNCTION(i2c2),
	FUNCTION(i2c3),
	FUNCTION(atv),
	FUNCTION(dtv),
	FUNCTION(pwm_a),
	FUNCTION(pwm_b),
	FUNCTION(pwm_c),
	FUNCTION(pwm_d),
	FUNCTION(pwm_e),
	FUNCTION(pwm_f),
	FUNCTION(pwm_g),
	FUNCTION(pwm_h),
	FUNCTION(pwm_vs),
	FUNCTION(pdm),
	FUNCTION(tcon),
	FUNCTION(hsync),
	FUNCTION(vsync),
	FUNCTION(sync_3d_out),
	FUNCTION(spi0),
	FUNCTION(spi2),
	FUNCTION(mclk),
	FUNCTION(spdif_out_a),
	FUNCTION(spdif_out_b),
	FUNCTION(spdif_in),
	FUNCTION(vx1),
	FUNCTION(eth),
	FUNCTION(cicam),
	FUNCTION(iso7816),
	FUNCTION(tsout),
	FUNCTION(tsin_a),
	FUNCTION(tsin_b),
	FUNCTION(diseqc_out),
	FUNCTION(s2_demod),
	FUNCTION(demod_uart),
	FUNCTION(diseqc),
	FUNCTION(mic_mute)
};

static struct meson_bank meson_t5w_periphs_banks[] = {
	/* name  first  last  irq  pullen  pull  dir  out  in ds */
	BANK_DS("B",    GPIOB_0,    GPIOB_13, 52, 65,
	     0,  0,  0,  0,   0,  0,  1,   0,  2,  0, 0x20, 0),
	BANK_DS("Z",    GPIOZ_0,    GPIOZ_19,  66, 85,
	     1,  0,  1,  0,   3,  0,  4,  0,  5,  0, 0x21, 0),
	BANK_DS("H",    GPIOH_0,    GPIOH_25,  11, 36,
	     2,  0,  2,  0,   6,  0,  7,  0,  8,  0, 0x23, 0),
	BANK_DS("W",    GPIOW_0,    GPIOW_12, 86, 98,
	     3,  0,  3,  0,   9,  0,  10,  0,  11, 0, 0x25, 0),
	BANK_DS("M",    GPIOM_0,    GPIOM_29,  101, 130,
	     4,  0,  4,  0,  12, 0,   13,  0,  14, 0, 0x26, 0),
	BANK_DS("C",    GPIOC_0,    GPIOC_14,  37, 51,
	     5,  0,  5,  0,  16, 0,   17,  0,  18, 0, 0x28, 0)
};

static struct meson_bank meson_t5w_aobus_banks[] = {
	/* name  first  last  irq  pullen  pull  dir  out  in  */
	BANK_DS("D",   GPIOD_0,  GPIOD_10,  0, 10,
	     3,  0,  2, 0,  0,  0,  4, 0,  1,  0, 0x00, 0),
	BANK_DS("E",   GPIOE_0,  GPIOE_1,   99, 100,
	     3,  16, 2, 16, 0,  16, 4, 16, 1,  16, 0x01, 0)
};

static struct meson_bank meson_t5w_analog_banks[] = {
	BANK("ANALOG", CDAC_IOUT, CVBS1,   131, 133,
	     0,  31, 0, 30, 0, 0, 0, 0, 1, 0)
};

static struct meson_pmx_bank meson_t5w_analog_pmx_banks[] = {
	BANK_PMX("ANALOG", CDAC_IOUT, CVBS1, 0x0, 29)
};

static struct meson_axg_pmx_data meson_t5w_analog_pmx_banks_data = {
	.pmx_banks	= meson_t5w_analog_pmx_banks,
	.num_pmx_banks	= ARRAY_SIZE(meson_t5w_analog_pmx_banks),
};

static struct meson_pmx_bank meson_t5w_periphs_pmx_banks[] = {
	/*	 name	 first		lask	   reg	offset  */
	BANK_PMX("B",    GPIOB_0,  GPIOB_13, 0x0,  0),
	BANK_PMX("W",    GPIOW_0,  GPIOW_12, 0x2,  0),
	BANK_PMX("Z1",   GPIOZ_0,  GPIOZ_7,  0x4,  0),
	BANK_PMX("Z2",   GPIOZ_8,  GPIOZ_19, 0x8,  0),
	BANK_PMX("H1",	 GPIOH_0,  GPIOH_23, 0x5,  0),
	BANK_PMX("C1",	 GPIOC_0,  GPIOC_7,  0xf,  0),
	BANK_PMX("C2",	 GPIOC_8,  GPIOC_14, 0x2a, 0),
	BANK_PMX("H2",	 GPIOH_24, GPIOH_25, 0x2b, 0),

	BANK_PMX("M",    GPIOM_0,  GPIOM_29, 0xa,  0)
};

static struct meson_axg_pmx_data meson_t5w_periphs_pmx_banks_data = {
	.pmx_banks	= meson_t5w_periphs_pmx_banks,
	.num_pmx_banks	= ARRAY_SIZE(meson_t5w_periphs_pmx_banks),
};

static struct meson_pmx_bank meson_t5w_aobus_pmx_banks[] = {
	BANK_PMX("D", GPIOD_0, GPIOD_10, 0x0, 0),
	BANK_PMX("E", GPIOE_0, GPIOE_1,  0x1, 16)
};

static struct meson_axg_pmx_data meson_t5w_aobus_pmx_banks_data = {
	.pmx_banks	= meson_t5w_aobus_pmx_banks,
	.num_pmx_banks	= ARRAY_SIZE(meson_t5w_aobus_pmx_banks),
};

static struct meson_pinctrl_data meson_t5w_periphs_pinctrl_data __refdata = {
	.name		= "periphs-banks",
	.pins		= meson_t5w_periphs_pins,
	.groups		= meson_t5w_periphs_groups,
	.funcs		= meson_t5w_periphs_functions,
	.banks		= meson_t5w_periphs_banks,
	.num_pins	= ARRAY_SIZE(meson_t5w_periphs_pins),
	.num_groups	= ARRAY_SIZE(meson_t5w_periphs_groups),
	.num_funcs	= ARRAY_SIZE(meson_t5w_periphs_functions),
	.num_banks	= ARRAY_SIZE(meson_t5w_periphs_banks),
	.pmx_ops	= &meson_axg_pmx_ops,
	.pmx_data	= &meson_t5w_periphs_pmx_banks_data,
	.parse_dt	= meson_t5w_workaround_parse_dt_extra,
};

static struct meson_pinctrl_data meson_t5w_aobus_pinctrl_data __refdata = {
	.name		= "aobus-banks",
	.pins		= meson_t5w_aobus_pins,
	.groups		= meson_t5w_aobus_groups,
	.funcs		= meson_t5w_aobus_functions,
	.banks		= meson_t5w_aobus_banks,
	.num_pins	= ARRAY_SIZE(meson_t5w_aobus_pins),
	.num_groups	= ARRAY_SIZE(meson_t5w_aobus_groups),
	.num_funcs	= ARRAY_SIZE(meson_t5w_aobus_functions),
	.num_banks	= ARRAY_SIZE(meson_t5w_aobus_banks),
	.pmx_ops	= &meson_axg_pmx_ops,
	.pmx_data	= &meson_t5w_aobus_pmx_banks_data,
	.parse_dt	= meson_g12a_aobus_parse_dt_extra,
};

static struct meson_pinctrl_data meson_t5w_analog_pinctrl_data __refdata = {
	.name		= "analog-banks",
	.pins		= meson_t5w_analog_pins,
	.groups		= meson_t5w_analog_groups,
	.funcs		= meson_t5w_analog_functions,
	.banks		= meson_t5w_analog_banks,
	.num_pins	= ARRAY_SIZE(meson_t5w_analog_pins),
	.num_groups	= ARRAY_SIZE(meson_t5w_analog_groups),
	.num_funcs	= ARRAY_SIZE(meson_t5w_analog_functions),
	.num_banks	= ARRAY_SIZE(meson_t5w_analog_banks),
	.pmx_ops	= &meson_axg_pmx_ops,
	.pmx_data	= &meson_t5w_analog_pmx_banks_data,
	.parse_dt	= meson_g12a_aobus_parse_dt_extra,
};

static const struct of_device_id meson_t5w_pinctrl_dt_match[] = {
	{
		.compatible = "amlogic,meson-t5w-periphs-pinctrl",
		.data = &meson_t5w_periphs_pinctrl_data,
	},
	{
		.compatible = "amlogic,meson-t5w-aobus-pinctrl",
		.data = &meson_t5w_aobus_pinctrl_data,
	},
	{
		.compatible = "amlogic,meson-t5w-analog-pinctrl",
		.data = &meson_t5w_analog_pinctrl_data,
	},
	{ }
};

static struct platform_driver meson_t5w_pinctrl_driver = {
	.probe  = meson_pinctrl_probe,
	.driver = {
		.name	= "meson-t5w-pinctrl",
		.of_match_table = meson_t5w_pinctrl_dt_match,
	},
};

#ifndef MODULE
static int __init t5w_pmx_init(void)
{
	meson_t5w_pinctrl_driver.driver.of_match_table =
			meson_t5w_pinctrl_dt_match;
	return platform_driver_register(&meson_t5w_pinctrl_driver);
}
arch_initcall(t5w_pmx_init);

#else
int __init meson_t5w_pinctrl_init(void)
{
	meson_t5w_pinctrl_driver.driver.of_match_table =
			meson_t5w_pinctrl_dt_match;
	return platform_driver_register(&meson_t5w_pinctrl_driver);
}
#endif
