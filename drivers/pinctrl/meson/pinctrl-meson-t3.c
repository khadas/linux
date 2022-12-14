// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <dt-bindings/gpio/meson-t3-gpio.h>
#include "pinctrl-meson.h"
#include "pinctrl-meson-axg-pmx.h"
#include "pinctrl-module-init.h"

static const struct pinctrl_pin_desc meson_t3_analog_pins[] = {
	MESON_PIN(CDAC_IOUT),
	MESON_PIN(CVBS0),
	MESON_PIN(CVBS1)
};

static struct meson_pmx_group meson_t3_analog_groups[] __initdata = {
	GPIO_GROUP(CDAC_IOUT),
	GPIO_GROUP(CVBS0),
	GPIO_GROUP(CVBS1)
};

static const char * const gpio_analog_groups[] = {
	"CDAC_IOUT", "CVBS0", "CVBS1"
};

static struct meson_pmx_func meson_t3_analog_functions[] __initdata = {
	FUNCTION(gpio_analog)
};

static const struct pinctrl_pin_desc meson_t3_periphs_pins[] = {
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
	MESON_PIN(GPIOC_14),
	MESON_PIN(GPIOC_15),

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
	MESON_PIN(GPIOH_26),
	MESON_PIN(GPIOH_27),

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
	MESON_PIN(GPIOM_30),

	MESON_PIN(GPIO_TEST_N)
};

/* bank W func1 */
static const unsigned int hdmirx_hpd_a_pins[]		= { GPIOW_0 };
static const unsigned int hdmirx_5vdet_a_pins[]		= { GPIOW_1 };
static const unsigned int hdmirx_sda_a_pins[]		= { GPIOW_2 };
static const unsigned int hdmirx_scl_a_pins[]		= { GPIOW_3 };
static const unsigned int hdmirx_hpd_c_pins[]		= { GPIOW_4 };
static const unsigned int hdmirx_5vdet_c_pins[]		= { GPIOW_5 };
static const unsigned int hdmirx_sda_c_pins[]		= { GPIOW_6 };
static const unsigned int hdmirx_scl_c_pins[]		= { GPIOW_7 };
static const unsigned int hdmirx_hpd_b_pins[]		= { GPIOW_8 };
static const unsigned int hdmirx_5vdet_b_pins[]		= { GPIOW_9 };
static const unsigned int hdmirx_sda_b_pins[]		= { GPIOW_10 };
static const unsigned int hdmirx_scl_b_pins[]		= { GPIOW_11 };
static const unsigned int cec_pins[]			= { GPIOW_12 };

/* bank W func2 */
static const unsigned int uart_b_tx_w2_pins[]		= { GPIOW_2 };
static const unsigned int uart_b_rx_w3_pins[]		= { GPIOW_3 };
static const unsigned int uart_b_tx_w6_pins[]		= { GPIOW_6 };
static const unsigned int uart_b_rx_w7_pins[]		= { GPIOW_7 };
static const unsigned int uart_b_tx_w10_pins[]		= { GPIOW_10 };
static const unsigned int uart_b_rx_w11_pins[]		= { GPIOW_11 };

/* bank D func1 */
static const unsigned int uart_b_tx_d_pins[]		= { GPIOD_0 };
static const unsigned int uart_b_rx_d_pins[]		= { GPIOD_1 };
static const unsigned int i2c1_sck_d_pins[]		= { GPIOD_2 };
static const unsigned int i2c1_sda_d_pins[]		= { GPIOD_3 };
static const unsigned int clk_32k_in_pins[]		= { GPIOD_4 };
static const unsigned int remote_input_pins[]		= { GPIOD_5 };
static const unsigned int jtag_a_clk_pins[]		= { GPIOD_6 };
static const unsigned int jtag_a_tms_pins[]		= { GPIOD_7 };
static const unsigned int jtag_a_tdi_pins[]		= { GPIOD_8 };
static const unsigned int jtag_a_tdo_pins[]		= { GPIOD_9 };
static const unsigned int gen_clk_out_d_pins[]		= { GPIOD_10 };

/* bank D func2 */
static const unsigned int remote_out_d1_pins[]		= { GPIOD_1 };
static const unsigned int i2c_slave_sck_pins[]		= { GPIOD_2 };
static const unsigned int i2c_slave_sda_pins[]		= { GPIOD_3 };
static const unsigned int atv_if_agc_d_pins[]		= { GPIOD_4 };
static const unsigned int pwm_c_d5_pins[]		= { GPIOD_5 };
static const unsigned int pwm_d_d6_pins[]		= { GPIOD_6 };
static const unsigned int pwm_c_d7_pins[]		= { GPIOD_7 };
static const unsigned int spdif_out_d_pins[]		= { GPIOD_8 };
static const unsigned int pwm_d_d9_pins[]		= { GPIOD_9 };
static const unsigned int pwm_e_d_pins[]		= { GPIOD_10 };

/* bank D func3 */
static const unsigned int dtv_if_agc_d_pins[]		= { GPIOD_4 };
static const unsigned int pwm_d_hiz_d_pins[]		= { GPIOD_6 };
static const unsigned int pwm_c_hiz_d_pins[]		= { GPIOD_7 };
static const unsigned int spdif_in_d_pins[]		= { GPIOD_8 };
static const unsigned int remote_out_d9_pins[]		= { GPIOD_9 };

/* bank D func4 */
static const unsigned int uart_c_tx_d_pins[]		= { GPIOD_6 };
static const unsigned int uart_c_rx_d_pins[]		= { GPIOD_7 };
static const unsigned int uart_c_cts_d_pins[]		= { GPIOD_8 };
static const unsigned int uart_c_rts_d_pins[]		= { GPIOD_9 };

/* bank E func1 */
static const unsigned int pwm_a_e_pins[]		= { GPIOE_0 };
static const unsigned int pwm_b_e_pins[]		= { GPIOE_1 };

/* bank E func2 */
static const unsigned int i2c2_sck_e_pins[]		= { GPIOE_0 };
static const unsigned int i2c2_sda_e_pins[]		= { GPIOE_1 };

/* bank B func1 */
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

/* bank B func2 */
static const unsigned int nand_wen_clk_pins[]		= { GPIOB_8 };
static const unsigned int nand_ale_pins[]		= { GPIOB_9 };
static const unsigned int nand_ren_wr_pins[]		= { GPIOB_10 };
static const unsigned int nand_cle_pins[]		= { GPIOB_11 };
static const unsigned int nand_ce0_pins[]		= { GPIOB_12 };

/* bank B func3 */
static const unsigned int spif_hold_pins[]		= { GPIOB_3 };
static const unsigned int spif_mo_pins[]		= { GPIOB_4 };
static const unsigned int spif_mi_pins[]		= { GPIOB_5 };
static const unsigned int spif_clk_pins[]		= { GPIOB_6 };
static const unsigned int spif_wp_pins[]		= { GPIOB_7 };
static const unsigned int remote_out_b_pins[]		= { GPIOB_12 };
static const unsigned int spif_cs_pins[]		= { GPIOB_13 };

/* bank C func1 */
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
static const unsigned int pwm_a_c_pins[]		= { GPIOC_11 };
static const unsigned int pwm_d_c_pins[]		= { GPIOC_12 };
static const unsigned int i2c1_sda_c_pins[]		= { GPIOC_13 };
static const unsigned int i2c1_sck_c_pins[]		= { GPIOC_14 };
static const unsigned int pwm_b_c_pins[]		= { GPIOC_15 };

/* bank C func2 */
static const unsigned int spi_c_miso_pins[]		= { GPIOC_0 };
static const unsigned int spi_c_mosi_pins[]		= { GPIOC_1 };
static const unsigned int spi_c_clk_pins[]		= { GPIOC_2 };
static const unsigned int spi_c_ss0_pins[]		= { GPIOC_3 };
static const unsigned int spi_c_ss1_pins[]		= { GPIOC_4 };
static const unsigned int spi_c_ss2_pins[]		= { GPIOC_5 };
static const unsigned int sdcard_det_pins[]		= { GPIOC_10 };

/* bank C func3 */
static const unsigned int eth_mdio_c_pins[]		= { GPIOC_0 };
static const unsigned int eth_mdc_c_pins[]		= { GPIOC_1 };
static const unsigned int eth_rgmii_rx_clk_c_pins[]	= { GPIOC_2 };
static const unsigned int eth_rx_dv_c_pins[]		= { GPIOC_3 };
static const unsigned int eth_rxd0_c_pins[]		= { GPIOC_4 };
static const unsigned int eth_rxd1_c_pins[]		= { GPIOC_5 };
static const unsigned int eth_rxd2_rgmii_c_pins[]	= { GPIOC_6 };
static const unsigned int eth_rxd3_rgmii_c_pins[]	= { GPIOC_7 };
static const unsigned int eth_rgmii_tx_clk_c_pins[]	= { GPIOC_8 };
static const unsigned int eth_txen_c_pins[]		= { GPIOC_9 };
static const unsigned int eth_txd0_c_pins[]		= { GPIOC_10 };
static const unsigned int eth_txd1_c_pins[]		= { GPIOC_11 };
static const unsigned int eth_txd2_rgmii_c_pins[]	= { GPIOC_12 };
static const unsigned int eth_txd3_rgmii_c_pins[]	= { GPIOC_13 };
static const unsigned int eth_link_led_c_pins[]		= { GPIOC_14 };
static const unsigned int eth_act_led_c_pins[]		= { GPIOC_15 };

/* bank C func4 */
static const unsigned int pcieck_reqn_c_pins[]		= { GPIOC_15 };

/* bank C func5 */
static const unsigned int tdm_fs1_c_pins[]		= { GPIOC_6 };
static const unsigned int tdm_sclk1_c_pins[]		= { GPIOC_7 };
static const unsigned int tdm_d4_c_pins[]		= { GPIOC_8 };
static const unsigned int tdm_d5_c_pins[]		= { GPIOC_9 };
static const unsigned int tdm_d6_c_pins[]		= { GPIOC_10 };
static const unsigned int tdm_d7_c_pins[]		= { GPIOC_11 };

/* bank Z func1 */
static const unsigned int tdm_fs2_z_pins[]		= { GPIOZ_0 };
static const unsigned int tdm_sclk2_z_pins[]		= { GPIOZ_1 };
static const unsigned int tdm_d4_z_pins[]		= { GPIOZ_2 };
static const unsigned int tdm_d5_z_pins[]		= { GPIOZ_3 };
static const unsigned int tdm_d6_z_pins[]		= { GPIOZ_4 };
static const unsigned int tdm_d7_z_pins[]		= { GPIOZ_5 };
static const unsigned int mclk_2_z_pins[]		= { GPIOZ_6 };
static const unsigned int tsout_clk_z_pins[]		= { GPIOZ_7 };
static const unsigned int tsout_sop_z_pins[]		= { GPIOZ_8 };
static const unsigned int tsout_valid_z_pins[]		= { GPIOZ_9 };
static const unsigned int tsout_d0_z_pins[]		= { GPIOZ_10 };
static const unsigned int tsout_d1_z_pins[]		= { GPIOZ_11 };
static const unsigned int tsout_d2_z_pins[]		= { GPIOZ_12 };
static const unsigned int tsout_d3_z_pins[]		= { GPIOZ_13 };
static const unsigned int tsout_d4_z_pins[]		= { GPIOZ_14 };
static const unsigned int tsout_d5_z_pins[]		= { GPIOZ_15 };
static const unsigned int tsout_d6_z_pins[]		= { GPIOZ_16 };
static const unsigned int tsout_d7_z_pins[]		= { GPIOZ_17 };
static const unsigned int mclk_1_z_pins[]		= { GPIOZ_18 };
static const unsigned int spdif_out_z19_pins[]		= { GPIOZ_19 };

/* bank Z func2 */
static const unsigned int tsin_a_clk_pins[]		= { GPIOZ_0 };
static const unsigned int tsin_a_sop_pins[]		= { GPIOZ_1 };
static const unsigned int tsin_a_valid_pins[]		= { GPIOZ_2 };
static const unsigned int tsin_a_din0_pins[]		= { GPIOZ_3 };
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

/* bank Z func3 */
static const unsigned int iso7816_clk_z_pins[]		= { GPIOZ_0 };
static const unsigned int iso7816_data_z_pins[]		= { GPIOZ_1 };
static const unsigned int cicam_a13_pins[]		= { GPIOZ_2 };
static const unsigned int cicam_a14_pins[]		= { GPIOZ_3 };
static const unsigned int i2c0_sck_z_pins[]		= { GPIOZ_4 };
static const unsigned int i2c0_sda_z_pins[]		= { GPIOZ_5 };
static const unsigned int atv_if_agc_z6_pins[]		= { GPIOZ_6 };
static const unsigned int pcieck_reqn_z_pins[]		= { GPIOZ_17 };
static const unsigned int atv_if_agc_z18_pins[]		= { GPIOZ_18 };

/* bank Z func4 */
static const unsigned int spi_a_miso_z0_pins[]		= { GPIOZ_0 };
static const unsigned int spi_a_mosi_z1_pins[]		= { GPIOZ_1 };
static const unsigned int spi_a_clk_z2_pins[]		= { GPIOZ_2 };
static const unsigned int spi_a_ss0_z3_pins[]		= { GPIOZ_3 };
static const unsigned int spi_a_ss1_z4_pins[]		= { GPIOZ_4 };
static const unsigned int spi_a_ss2_z5_pins[]		= { GPIOZ_5 };
static const unsigned int dtv_if_agc_z6_pins[]		= { GPIOZ_6 };
static const unsigned int dtv_if_agc_z18_pins[]		= { GPIOZ_18 };

/* bank Z func5 */
static const unsigned int uart_a_tx_z_pins[]		= { GPIOZ_0 };
static const unsigned int uart_a_rx_z_pins[]		= { GPIOZ_1 };
static const unsigned int uart_a_cts_z_pins[]		= { GPIOZ_2 };
static const unsigned int uart_a_rts_z_pins[]		= { GPIOZ_3 };
static const unsigned int pwm_b_z_pins[]		= { GPIOZ_4 };
static const unsigned int pwm_d_z_pins[]		= { GPIOZ_5 };
static const unsigned int pwm_e_z_pins[]		= { GPIOZ_6 };
static const unsigned int uart_d_tx_z_pins[]		= { GPIOZ_8 };
static const unsigned int uart_d_rx_z_pins[]		= { GPIOZ_9 };
static const unsigned int pwm_g_z_pins[]		= { GPIOZ_12 };
static const unsigned int pwm_h_z_pins[]		= { GPIOZ_13 };
static const unsigned int pwm_i_z_pins[]		= { GPIOZ_14 };
static const unsigned int pwm_j_z_pins[]		= { GPIOZ_15 };
static const unsigned int spi_a_clk_z18_pins[]		= { GPIOZ_18 };

/* bank Z func6 */
static const unsigned int cicam_a12_pins[]		= { GPIOZ_0 };
static const unsigned int pdm_din1_z1_pins[]		= { GPIOZ_1 };
static const unsigned int pdm_dclk_z2_pins[]		= { GPIOZ_2 };
static const unsigned int pdm_din0_z3_pins[]		= { GPIOZ_3 };
static const unsigned int pdm_din1_z6_pins[]		= { GPIOZ_6 };
static const unsigned int tdm_d1_z_pins[]		= { GPIOZ_10 };
static const unsigned int tdm_d0_z_pins[]		= { GPIOZ_11 };
static const unsigned int tdm_sclk1_z_pins[]		= { GPIOZ_16 };
static const unsigned int tdm_fs1_z_pins[]		= { GPIOZ_17 };
static const unsigned int diseqc_out_z18_pins[]		= { GPIOZ_18 };

/* bank Z func7 */
static const unsigned int diseqc_out_z0_pins[]		= { GPIOZ_0 };
static const unsigned int diseqc_in_z1_pins[]		= { GPIOZ_1 };
static const unsigned int s2_demod_gpio0_z_pins[]	= { GPIOZ_1 };
static const unsigned int spdif_out_z2_pins[]		= { GPIOZ_2 };
static const unsigned int spi_a_ss1_z3_pins[]		= { GPIOZ_3 };
static const unsigned int eth_mdio_z_pins[]		= { GPIOZ_4 };
static const unsigned int eth_mdc_z_pins[]		= { GPIOZ_5 };
static const unsigned int eth_link_led_z_pins[]		= { GPIOZ_6 };
static const unsigned int eth_rgmii_rx_clk_z_pins[]	= { GPIOZ_7 };
static const unsigned int eth_rx_dv_z_pins[]		= { GPIOZ_8 };
static const unsigned int eth_rxd0_z_pins[]		= { GPIOZ_9 };
static const unsigned int eth_rxd1_z_pins[]		= { GPIOZ_10 };
static const unsigned int eth_rxd2_rgmii_z_pins[]	= { GPIOZ_11 };
static const unsigned int eth_rxd3_rgmii_z_pins[]	= { GPIOZ_12 };
static const unsigned int eth_rgmii_tx_clk_z_pins[]	= { GPIOZ_13 };
static const unsigned int eth_txen_z_pins[]		= { GPIOZ_14 };
static const unsigned int eth_txd0_z_pins[]		= { GPIOZ_15 };
static const unsigned int eth_txd1_z_pins[]		= { GPIOZ_16 };
static const unsigned int eth_txd2_rgmii_z_pins[]	= { GPIOZ_17 };
static const unsigned int eth_txd3_rgmii_z_pins[]	= { GPIOZ_18 };
static const unsigned int eth_act_led_z_pins[]		= { GPIOZ_19 };

/* bank H func1 */
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
static const unsigned int spi_b_ss0_h_pins[]		= { GPIOH_24 };
static const unsigned int spi_b_miso_h_pins[]		= { GPIOH_25 };
static const unsigned int spi_b_mosi_h_pins[]		= { GPIOH_26 };
static const unsigned int spi_b_clk_h_pins[]		= { GPIOH_27 };

/* bank H func2 */
static const unsigned int tcon_lock_pins[]		= { GPIOH_0 };
static const unsigned int uart_c_tx_h1_pins[]		= { GPIOH_1 };
static const unsigned int uart_c_rx_h2_pins[]		= { GPIOH_2 };
static const unsigned int uart_c_cts_h3_pins[]		= { GPIOH_3 };
static const unsigned int uart_c_rts_h4_pins[]		= { GPIOH_4 };
static const unsigned int sync_3d_out_pins[]		= { GPIOH_5 };
static const unsigned int tdm_d3_h6_pins[]		= { GPIOH_6 };
static const unsigned int spi_a_ss1_h7_pins[]		= { GPIOH_7 };
static const unsigned int spi_a_ss0_h8_pins[]		= { GPIOH_8 };
static const unsigned int spi_a_miso_h9_pins[]		= { GPIOH_9 };
static const unsigned int spi_a_mosi_h10_pins[]		= { GPIOH_10 };
static const unsigned int spi_a_clk_h11_pins[]		= { GPIOH_11 };
static const unsigned int spi_a_ss2_h12_pins[]		= { GPIOH_12 };
static const unsigned int tdm_d3_h13_pins[]		= { GPIOH_13 };
static const unsigned int mclk_1_h_pins[]		= { GPIOH_14 };
static const unsigned int tdm_fs1_h15_pins[]		= { GPIOH_15 };
static const unsigned int tdm_sclk1_h16_pins[]		= { GPIOH_16 };
static const unsigned int tdm_d0_h17_pins[]		= { GPIOH_17 };
static const unsigned int tdm_d1_h18_pins[]		= { GPIOH_18 };
static const unsigned int tdm_d2_h19_pins[]		= { GPIOH_19 };
static const unsigned int tdm_d3_h20_pins[]		= { GPIOH_20 };
static const unsigned int tdm_d4_h21_pins[]		= { GPIOH_21 };
static const unsigned int tdm_d5_h22_pins[]		= { GPIOH_22 };
static const unsigned int tdm_d6_h23_pins[]		= { GPIOH_23 };

/* bank H func3 */
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
static const unsigned int pdm_din2_h22_pins[]		= { GPIOH_22 };
static const unsigned int pdm_dclk_h23_pins[]		= { GPIOH_23 };
static const unsigned int i2c2_sck_h24_pins[]		= { GPIOH_24 };
static const unsigned int i2c2_sda_h25_pins[]		= { GPIOH_25 };
static const unsigned int i2c4_sck_h26_pins[]		= { GPIOH_26 };
static const unsigned int i2c4_sda_h27_pins[]		= { GPIOH_27 };

/* bank H func4 */
static const unsigned int vx1_a_lockn_pins[]		= { GPIOH_0 };
static const unsigned int pwm_d_h5_pins[]		= { GPIOH_5 };
static const unsigned int vx1_a_htpdn_pins[]		= { GPIOH_8 };
static const unsigned int vx1_b_lockn_pins[]		= { GPIOH_9 };
static const unsigned int vx1_b_htpdn_pins[]		= { GPIOH_10 };
static const unsigned int pwm_d_h12_pins[]		= { GPIOH_12 };
static const unsigned int pwm_f_h13_pins[]		= { GPIOH_13 };
static const unsigned int pwm_b_h14_pins[]		= { GPIOH_14 };
static const unsigned int eth_act_led_h_pins[]		= { GPIOH_18 };
static const unsigned int eth_link_led_h_pins[]		= { GPIOH_19 };
static const unsigned int uart_c_tx_h22_pins[]		= { GPIOH_22 };
static const unsigned int uart_c_rx_h23_pins[]		= { GPIOH_23 };
static const unsigned int mic_mute_key_pins[]		= { GPIOH_24 };
static const unsigned int mic_mute_led_pins[]		= { GPIOH_25 };

/* bank H func5 */
static const unsigned int bt656_a_vs_pins[]		= { GPIOH_1 };
static const unsigned int bt656_a_hs_pins[]		= { GPIOH_2 };
static const unsigned int bt656_a_clk_pins[]		= { GPIOH_3 };
static const unsigned int bt656_a_din0_pins[]		= { GPIOH_4 };
static const unsigned int bt656_a_din1_pins[]		= { GPIOH_5 };
static const unsigned int bt656_a_din2_pins[]		= { GPIOH_6 };
static const unsigned int bt656_a_din3_pins[]		= { GPIOH_7 };
static const unsigned int bt656_a_din4_pins[]		= { GPIOH_9 };
static const unsigned int bt656_a_din5_pins[]		= { GPIOH_10 };
static const unsigned int bt656_a_din6_pins[]		= { GPIOH_11 };
static const unsigned int bt656_a_din7_pins[]		= { GPIOH_12 };
static const unsigned int clk12_24m_h_pins[]		= { GPIOH_13 };
static const unsigned int spi_a_clk_h14_pins[]		= { GPIOH_14 };
static const unsigned int spi_a_ss1_h19_pins[]		= { GPIOH_19 };

/* bank H func6 */
static const unsigned int gen_clk_out_h_pins[]		= { GPIOH_13 };
static const unsigned int s2_demod_gpio0_h_pins[]	= { GPIOH_15 };
static const unsigned int s2_demod_gpio1_pins[]		= { GPIOH_16 };
static const unsigned int s2_demod_gpio2_pins[]		= { GPIOH_17 };
static const unsigned int s2_demod_gpio3_pins[]		= { GPIOH_18 };
static const unsigned int s2_demod_gpio4_pins[]		= { GPIOH_19 };
static const unsigned int s2_demod_gpio5_pins[]		= { GPIOH_20 };
static const unsigned int s2_demod_gpio6_pins[]		= { GPIOH_21 };
static const unsigned int s2_demod_gpio7_pins[]		= { GPIOH_22 };

/* bank H func7 */
static const unsigned int uart_c_tx_h18_pins[]		= { GPIOH_18 };
static const unsigned int uart_c_rx_h19_pins[]		= { GPIOH_19 };

/* bank M func1 */
static const unsigned int tsin_b_clk_pins[]		= { GPIOM_0 };
static const unsigned int tsin_b_sop_pins[]		= { GPIOM_1 };
static const unsigned int tsin_b_valid_pins[]		= { GPIOM_2 };
static const unsigned int tsin_b_din0_pins[]		= { GPIOM_3 };
static const unsigned int tsin_b_din1_pins[]		= { GPIOM_4 };
static const unsigned int tsin_b_din2_pins[]		= { GPIOM_5 };
static const unsigned int tsin_b_din3_pins[]		= { GPIOM_6 };
static const unsigned int tsin_b_din4_pins[]		= { GPIOM_7 };
static const unsigned int tsin_b_din5_pins[]		= { GPIOM_8 };
static const unsigned int tsin_b_din6_pins[]		= { GPIOM_9 };
static const unsigned int tsin_b_din7_pins[]		= { GPIOM_10 };
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
static const unsigned int cicam_cdn_pins[]		= { GPIOM_27 };
static const unsigned int cicam_5ven_pins[]		= { GPIOM_28 };
static const unsigned int cicam_waitn_pins[]		= { GPIOM_29 };
static const unsigned int cicam_irqn_pins[]		= { GPIOM_30 };

/* bank M func2 */
static const unsigned int pdm_din2_m_pins[]		= { GPIOM_8 };
static const unsigned int pdm_din3_m_pins[]		= { GPIOM_9 };
static const unsigned int pdm_din0_m_pins[]		= { GPIOM_10 };
static const unsigned int pdm_dclk_m_pins[]		= { GPIOM_11 };
static const unsigned int mclk_2_m_pins[]		= { GPIOM_12 };
static const unsigned int tdm_fs2_m_pins[]		= { GPIOM_13 };
static const unsigned int tdm_sclk2_m_pins[]		= { GPIOM_14 };
static const unsigned int tdm_d4_m_pins[]		= { GPIOM_15 };
static const unsigned int tdm_d5_m_pins[]		= { GPIOM_16 };
static const unsigned int tdm_d6_m_pins[]		= { GPIOM_17 };
static const unsigned int tdm_d7_m_pins[]		= { GPIOM_18 };
static const unsigned int iso7816_clk_m_pins[]		= { GPIOM_19 };
static const unsigned int iso7816_data_m_pins[]		= { GPIOM_20 };
static const unsigned int clk12_24_m_pins[]		= { GPIOM_26 };
static const unsigned int gen_clk_out_m_pins[]		= { GPIOM_30 };

/* bank M func3 */
static const unsigned int spi_a_miso_m_pins[]		= { GPIOM_19 };
static const unsigned int spi_a_mosi_m_pins[]		= { GPIOM_20 };
static const unsigned int spi_a_clk_m_pins[]		= { GPIOM_21 };
static const unsigned int spi_a_ss0_m_pins[]		= { GPIOM_22 };
static const unsigned int spi_a_ss1_m_pins[]		= { GPIOM_23 };
static const unsigned int spi_a_ss2_m_pins[]		= { GPIOM_24 };
static const unsigned int i2c2_sck_m_pins[]		= { GPIOM_25 };
static const unsigned int i2c2_sda_m_pins[]		= { GPIOM_26 };
static const unsigned int i2c3_sck_m_pins[]		= { GPIOM_27 };
static const unsigned int i2c3_sda_m_pins[]		= { GPIOM_28 };
static const unsigned int i2c4_sck_m_pins[]		= { GPIOM_29 };
static const unsigned int i2c4_sda_m_pins[]		= { GPIOM_30 };

/* bank M func4 */
static const unsigned int pwm_d_m1_pins[]		= { GPIOM_1 };
static const unsigned int uart_d_tx_m_pins[]		= { GPIOM_4 };
static const unsigned int uart_d_rx_m_pins[]		= { GPIOM_5 };
static const unsigned int pwm_g_m8_pins[]		= { GPIOM_8 };
static const unsigned int pwm_h_m9_pins[]		= { GPIOM_9 };
static const unsigned int pwm_i_m10_pins[]		= { GPIOM_10 };
static const unsigned int pwm_j_m11_pins[]		= { GPIOM_11 };
static const unsigned int uart_c_tx_m_pins[]		= { GPIOM_19 };
static const unsigned int uart_c_rx_m_pins[]		= { GPIOM_20 };
static const unsigned int uart_c_cts_m_pins[]		= { GPIOM_21 };
static const unsigned int uart_c_rts_m_pins[]		= { GPIOM_22 };
static const unsigned int pwm_f_m23_pins[]		= { GPIOM_23 };
static const unsigned int pwm_d_m24_pins[]		= { GPIOM_24 };
static const unsigned int pwm_f_m26_pins[]		= { GPIOM_26 };
static const unsigned int pcieck_reqn_m_pins[]		= { GPIOM_30 };

/* bank M func5 */
static const unsigned int demod_uart_tx_m12_pins[]	= { GPIOM_12 };
static const unsigned int demod_uart_rx_m13_pins[]	= { GPIOM_13 };
static const unsigned int demod_uart_tx_m14_pins[]	= { GPIOM_14 };
static const unsigned int demod_uart_rx_m15_pins[]	= { GPIOM_15 };
static const unsigned int spdif_in_m_pins[]		= { GPIOM_29 };
static const unsigned int spdif_out_m_pins[]		= { GPIOM_30 };

static struct meson_pmx_group meson_t3_periphs_groups[] __initdata = {
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
	GPIO_GROUP(GPIOC_15),
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
	GPIO_GROUP(GPIOH_26),
	GPIO_GROUP(GPIOH_27),
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
	GPIO_GROUP(GPIOM_30),
	GPIO_GROUP(GPIO_TEST_N),

	/* bank W func1 */
	GROUP(hdmirx_hpd_a,		1),
	GROUP(hdmirx_5vdet_a,		1),
	GROUP(hdmirx_sda_a,		1),
	GROUP(hdmirx_scl_a,		1),
	GROUP(hdmirx_hpd_c,		1),
	GROUP(hdmirx_5vdet_c,		1),
	GROUP(hdmirx_sda_c,		1),
	GROUP(hdmirx_scl_c,		1),
	GROUP(hdmirx_hpd_b,		1),
	GROUP(hdmirx_5vdet_b,		1),
	GROUP(hdmirx_sda_b,		1),
	GROUP(hdmirx_scl_b,		1),
	GROUP(cec,			1),

	/* bank W func2 */
	GROUP(uart_b_tx_w2,		2),
	GROUP(uart_b_rx_w3,		2),
	GROUP(uart_b_tx_w6,		2),
	GROUP(uart_b_rx_w7,		2),
	GROUP(uart_b_tx_w10,		2),
	GROUP(uart_b_rx_w11,		2),

	/* bank D func1 */
	GROUP(uart_b_tx_d,		1),
	GROUP(uart_b_rx_d,		1),
	GROUP(i2c1_sck_d,		1),
	GROUP(i2c1_sda_d,		1),
	GROUP(clk_32k_in,		1),
	GROUP(remote_input,		1),
	GROUP(jtag_a_clk,		1),
	GROUP(jtag_a_tms,		1),
	GROUP(jtag_a_tdi,		1),
	GROUP(jtag_a_tdo,		1),
	GROUP(gen_clk_out_d,		1),

	/* bank D func2 */
	GROUP(remote_out_d1,		2),
	GROUP(i2c_slave_sck,		2),
	GROUP(i2c_slave_sda,		2),
	GROUP(atv_if_agc_d,		2),
	GROUP(pwm_c_d5,			2),
	GROUP(pwm_d_d6,			2),
	GROUP(pwm_c_d7,			2),
	GROUP(spdif_out_d,		2),
	GROUP(pwm_d_d9,			2),
	GROUP(pwm_e_d,			2),

	/* bank D func3 */
	GROUP(dtv_if_agc_d,		3),
	GROUP(pwm_d_hiz_d,		3),
	GROUP(pwm_c_hiz_d,		3),
	GROUP(spdif_in_d,		3),
	GROUP(remote_out_d9,		3),

	/* bank D func4 */
	GROUP(uart_c_tx_d,		4),
	GROUP(uart_c_rx_d,		4),
	GROUP(uart_c_cts_d,		4),
	GROUP(uart_c_rts_d,		4),

	/* bank E func1 */
	GROUP(pwm_a_e,			1),
	GROUP(pwm_b_e,			1),

	/* bank E func2 */
	GROUP(i2c2_sck_e,		2),
	GROUP(i2c2_sda_e,		2),

	/* bank B func1 */
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

	/* bank B func2 */
	GROUP(nand_wen_clk,		2),
	GROUP(nand_ale,			2),
	GROUP(nand_ren_wr,		2),
	GROUP(nand_cle,			2),
	GROUP(nand_ce0,			2),

	/* bank B func3 */
	GROUP(spif_hold,		3),
	GROUP(spif_mo,			3),
	GROUP(spif_mi,			3),
	GROUP(spif_clk,			3),
	GROUP(spif_wp,			3),
	GROUP(remote_out_b,		3),
	GROUP(spif_cs,			3),

	/* bank C func1 */
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
	GROUP(pwm_a_c,			1),
	GROUP(pwm_d_c,			1),
	GROUP(i2c1_sda_c,		1),
	GROUP(i2c1_sck_c,		1),
	GROUP(pwm_b_c,			1),

	/* bank C func2 */
	GROUP(spi_c_miso,		2),
	GROUP(spi_c_mosi,		2),
	GROUP(spi_c_clk,		2),
	GROUP(spi_c_ss0,		2),
	GROUP(spi_c_ss1,		2),
	GROUP(spi_c_ss2,		2),
	GROUP(sdcard_det,		2),

	/* bank C func3 */
	GROUP(eth_mdio_c,		3),
	GROUP(eth_mdc_c,		3),
	GROUP(eth_rgmii_rx_clk_c,	3),
	GROUP(eth_rx_dv_c,		3),
	GROUP(eth_rxd0_c,		3),
	GROUP(eth_rxd1_c,		3),
	GROUP(eth_rxd2_rgmii_c,		3),
	GROUP(eth_rxd3_rgmii_c,		3),
	GROUP(eth_rgmii_tx_clk_c,	3),
	GROUP(eth_txen_c,		3),
	GROUP(eth_txd0_c,		3),
	GROUP(eth_txd1_c,		3),
	GROUP(eth_txd2_rgmii_c,		3),
	GROUP(eth_txd3_rgmii_c,		3),
	GROUP(eth_link_led_c,		3),
	GROUP(eth_act_led_c,		3),

	/* bank C func4 */
	GROUP(pcieck_reqn_c,		4),

	/* bank C func5 */
	GROUP(tdm_fs1_c,		5),
	GROUP(tdm_sclk1_c,		5),
	GROUP(tdm_d4_c,			5),
	GROUP(tdm_d5_c,			5),
	GROUP(tdm_d6_c,			5),
	GROUP(tdm_d7_c,			5),

	/* bank Z func1 */
	GROUP(tdm_fs2_z,		1),
	GROUP(tdm_sclk2_z,		1),
	GROUP(tdm_d4_z,			1),
	GROUP(tdm_d5_z,			1),
	GROUP(tdm_d6_z,			1),
	GROUP(tdm_d7_z,			1),
	GROUP(mclk_2_z,			1),
	GROUP(tsout_clk_z,		1),
	GROUP(tsout_sop_z,		1),
	GROUP(tsout_valid_z,		1),
	GROUP(tsout_d0_z,		1),
	GROUP(tsout_d1_z,		1),
	GROUP(tsout_d2_z,		1),
	GROUP(tsout_d3_z,		1),
	GROUP(tsout_d4_z,		1),
	GROUP(tsout_d5_z,		1),
	GROUP(tsout_d6_z,		1),
	GROUP(tsout_d7_z,		1),
	GROUP(mclk_1_z,			1),
	GROUP(spdif_out_z19,		1),

	/* bank Z func2 */
	GROUP(tsin_a_clk,		2),
	GROUP(tsin_a_sop,		2),
	GROUP(tsin_a_valid,		2),
	GROUP(tsin_a_din0,		2),
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

	/* bank Z func3 */
	GROUP(iso7816_clk_z,		3),
	GROUP(iso7816_data_z,		3),
	GROUP(cicam_a13,		3),
	GROUP(cicam_a14,		3),
	GROUP(i2c0_sck_z,		3),
	GROUP(i2c0_sda_z,		3),
	GROUP(atv_if_agc_z6,		3),
	GROUP(pcieck_reqn_z,		3),
	GROUP(atv_if_agc_z18,		3),

	/* bank Z func4 */
	GROUP(spi_a_miso_z0,		4),
	GROUP(spi_a_mosi_z1,		4),
	GROUP(spi_a_clk_z2,		4),
	GROUP(spi_a_ss0_z3,		4),
	GROUP(spi_a_ss1_z4,		4),
	GROUP(spi_a_ss2_z5,		4),
	GROUP(dtv_if_agc_z6,		4),
	GROUP(dtv_if_agc_z18,		4),

	/* bank Z func5 */
	GROUP(uart_a_tx_z,		5),
	GROUP(uart_a_rx_z,		5),
	GROUP(uart_a_cts_z,		5),
	GROUP(uart_a_rts_z,		5),
	GROUP(pwm_b_z,			5),
	GROUP(pwm_d_z,			5),
	GROUP(pwm_e_z,			5),
	GROUP(uart_d_tx_z,		5),
	GROUP(uart_d_rx_z,		5),
	GROUP(pwm_g_z,			5),
	GROUP(pwm_h_z,			5),
	GROUP(pwm_i_z,			5),
	GROUP(pwm_j_z,			5),
	GROUP(spi_a_clk_z18,		5),

	/* bank Z func6 */
	GROUP(cicam_a12,		6),
	GROUP(pdm_din1_z1,		6),
	GROUP(pdm_dclk_z2,		6),
	GROUP(pdm_din0_z3,		6),
	GROUP(pdm_din1_z6,		6),
	GROUP(tdm_d1_z,			6),
	GROUP(tdm_d0_z,			6),
	GROUP(tdm_sclk1_z,		6),
	GROUP(tdm_fs1_z,		6),
	GROUP(diseqc_out_z18,		6),

	/* bank Z func7 */
	GROUP(diseqc_out_z0,		7),
	GROUP(diseqc_in_z1,		7),
	GROUP(s2_demod_gpio0_z,		7),
	GROUP(spdif_out_z2,		7),
	GROUP(spi_a_ss1_z3,		7),
	GROUP(eth_mdio_z,		7),
	GROUP(eth_mdc_z,		7),
	GROUP(eth_link_led_z,		7),
	GROUP(eth_rgmii_rx_clk_z,	7),
	GROUP(eth_rx_dv_z,		7),
	GROUP(eth_rxd0_z,		7),
	GROUP(eth_rxd1_z,		7),
	GROUP(eth_rxd2_rgmii_z,		7),
	GROUP(eth_rxd3_rgmii_z,		7),
	GROUP(eth_rgmii_tx_clk_z,	7),
	GROUP(eth_txen_z,		7),
	GROUP(eth_txd0_z,		7),
	GROUP(eth_txd1_z,		7),
	GROUP(eth_txd2_rgmii_z,		7),
	GROUP(eth_txd3_rgmii_z,		7),
	GROUP(eth_act_led_z,		7),

	/* bank H func1 */
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
	GROUP(spi_b_ss0_h,		1),
	GROUP(spi_b_miso_h,		1),
	GROUP(spi_b_mosi_h,		1),
	GROUP(spi_b_clk_h,		1),

	/* bank H func2 */
	GROUP(tcon_lock,		2),
	GROUP(uart_c_tx_h1,		2),
	GROUP(uart_c_rx_h2,		2),
	GROUP(uart_c_cts_h3,		2),
	GROUP(uart_c_rts_h4,		2),
	GROUP(sync_3d_out,		2),
	GROUP(tdm_d3_h6,		2),
	GROUP(spi_a_ss1_h7,		2),
	GROUP(spi_a_ss0_h8,		2),
	GROUP(spi_a_miso_h9,		2),
	GROUP(spi_a_mosi_h10,		2),
	GROUP(spi_a_clk_h11,		2),
	GROUP(spi_a_ss2_h12,		2),
	GROUP(tdm_d3_h13,		2),
	GROUP(mclk_1_h,			2),
	GROUP(tdm_fs1_h15,		2),
	GROUP(tdm_sclk1_h16,		2),
	GROUP(tdm_d0_h17,		2),
	GROUP(tdm_d1_h18,		2),
	GROUP(tdm_d2_h19,		2),
	GROUP(tdm_d3_h20,		2),
	GROUP(tdm_d4_h21,		2),
	GROUP(tdm_d5_h22,		2),
	GROUP(tdm_d6_h23,		2),

	/* bank H func3 */
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
	GROUP(pdm_din2_h22,		3),
	GROUP(pdm_dclk_h23,		3),
	GROUP(i2c2_sck_h24,		3),
	GROUP(i2c2_sda_h25,		3),
	GROUP(i2c4_sck_h26,		3),
	GROUP(i2c4_sda_h27,		3),

	/* bank H func4 */
	GROUP(vx1_a_lockn,		4),
	GROUP(pwm_d_h5,			4),
	GROUP(vx1_a_htpdn,		4),
	GROUP(vx1_b_lockn,		4),
	GROUP(vx1_b_htpdn,		4),
	GROUP(pwm_d_h12,		4),
	GROUP(pwm_f_h13,		4),
	GROUP(pwm_b_h14,		4),
	GROUP(eth_act_led_h,		4),
	GROUP(eth_link_led_h,		4),
	GROUP(uart_c_tx_h22,		4),
	GROUP(uart_c_rx_h23,		4),
	GROUP(mic_mute_key,		4),
	GROUP(mic_mute_led,		4),

	/* bank H func5 */
	GROUP(bt656_a_vs,		5),
	GROUP(bt656_a_hs,		5),
	GROUP(bt656_a_clk,		5),
	GROUP(bt656_a_din0,		5),
	GROUP(bt656_a_din1,		5),
	GROUP(bt656_a_din2,		5),
	GROUP(bt656_a_din3,		5),
	GROUP(bt656_a_din4,		5),
	GROUP(bt656_a_din5,		5),
	GROUP(bt656_a_din6,		5),
	GROUP(bt656_a_din7,		5),
	GROUP(clk12_24m_h,		5),
	GROUP(spi_a_clk_h14,		5),
	GROUP(spi_a_ss1_h19,		5),

	/* bank H func6 */
	GROUP(gen_clk_out_h,		6),
	GROUP(s2_demod_gpio0_h,		6),
	GROUP(s2_demod_gpio1,		6),
	GROUP(s2_demod_gpio2,		6),
	GROUP(s2_demod_gpio3,		6),
	GROUP(s2_demod_gpio4,		6),
	GROUP(s2_demod_gpio5,		6),
	GROUP(s2_demod_gpio6,		6),
	GROUP(s2_demod_gpio7,		6),

	/* bank H func7 */
	GROUP(uart_c_tx_h18,		7),
	GROUP(uart_c_rx_h19,		7),

	/* bank M func1 */
	GROUP(tsin_b_clk,		1),
	GROUP(tsin_b_sop,		1),
	GROUP(tsin_b_valid,		1),
	GROUP(tsin_b_din0,		1),
	GROUP(tsin_b_din1,		1),
	GROUP(tsin_b_din2,		1),
	GROUP(tsin_b_din3,		1),
	GROUP(tsin_b_din4,		1),
	GROUP(tsin_b_din5,		1),
	GROUP(tsin_b_din6,		1),
	GROUP(tsin_b_din7,		1),
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
	GROUP(cicam_cdn,		1),
	GROUP(cicam_5ven,		1),
	GROUP(cicam_waitn,		1),
	GROUP(cicam_irqn,		1),

	/* bank M func2 */
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
	GROUP(clk12_24_m,		2),
	GROUP(gen_clk_out_m,		2),

	/* bank M func3 */
	GROUP(spi_a_miso_m,		3),
	GROUP(spi_a_mosi_m,		3),
	GROUP(spi_a_clk_m,		3),
	GROUP(spi_a_ss0_m,		3),
	GROUP(spi_a_ss1_m,		3),
	GROUP(spi_a_ss2_m,		3),
	GROUP(i2c2_sck_m,		3),
	GROUP(i2c2_sda_m,		3),
	GROUP(i2c3_sck_m,		3),
	GROUP(i2c3_sda_m,		3),
	GROUP(i2c4_sck_m,		3),
	GROUP(i2c4_sda_m,		3),

	/* bank M func4 */
	GROUP(pwm_d_m1,			4),
	GROUP(uart_d_tx_m,		4),
	GROUP(uart_d_rx_m,		4),
	GROUP(pwm_g_m8,			4),
	GROUP(pwm_h_m9,			4),
	GROUP(pwm_i_m10,		4),
	GROUP(pwm_j_m11,		4),
	GROUP(uart_c_tx_m,		4),
	GROUP(uart_c_rx_m,		4),
	GROUP(uart_c_cts_m,		4),
	GROUP(uart_c_rts_m,		4),
	GROUP(pwm_f_m23,		4),
	GROUP(pwm_d_m24,		4),
	GROUP(pwm_f_m26,		4),
	GROUP(pcieck_reqn_m,		4),

	/* bank M func5 */
	GROUP(demod_uart_tx_m12,	5),
	GROUP(demod_uart_rx_m13,	5),
	GROUP(demod_uart_tx_m14,	5),
	GROUP(demod_uart_rx_m15,	5),
	GROUP(spdif_in_m,		5),
	GROUP(spdif_out_m,		5)
};

static const char * const gpio_periphs_groups[] = {
	"GPIO_TEST_N",

	"GPIOW_0", "GPIOW_1", "GPIOW_2", "GPIOW_3", "GPIOW_4",
	"GPIOW_5", "GPIOW_6", "GPIOW_7", "GPIOW_8", "GPIOW_9",
	"GPIOW_10", "GPIOW_11", "GPIOW_12",

	"GPIOD_0", "GPIOD_1", "GPIOD_2", "GPIOD_3", "GPIOD_4",
	"GPIOD_5", "GPIOD_6", "GPIOD_7", "GPIOD_8", "GPIOD_9",
	"GPIOD_10",

	"GPIOE_0", "GPIOE_1",

	"GPIOB_0", "GPIOB_1", "GPIOB_2", "GPIOB_3", "GPIOB_4",
	"GPIOB_5", "GPIOB_6", "GPIOB_7", "GPIOB_8", "GPIOB_9",
	"GPIOB_10", "GPIOB_11", "GPIOB_12", "GPIOB_13",

	"GPIOC_0", "GPIOC_1", "GPIOC_2", "GPIOC_3", "GPIOC_4",
	"GPIOC_5", "GPIOC_6", "GPIOC_7", "GPIOC_8", "GPIOC_9",
	"GPIOC_10", "GPIOC_11", "GPIOC_12", "GPIOC_13", "GPIOC_14",
	"GPIOC_15",

	"GPIOZ_0", "GPIOZ_1", "GPIOZ_2", "GPIOZ_3", "GPIOZ_4",
	"GPIOZ_5", "GPIOZ_6", "GPIOZ_7", "GPIOZ_8", "GPIOZ_9",
	"GPIOZ_10", "GPIOZ_11", "GPIOZ_12", "GPIOZ_13",
	"GPIOZ_14", "GPIOZ_15", "GPIOZ_16", "GPIOZ_17",
	"GPIOZ_18", "GPIOZ_19",

	"GPIOH_0", "GPIOH_1", "GPIOH_2", "GPIOH_3", "GPIOH_4",
	"GPIOH_5", "GPIOH_6", "GPIOH_7", "GPIOH_8", "GPIOH_9",
	"GPIOH_10", "GPIOH_11", "GPIOH_12", "GPIOH_13", "GPIOH_14",
	"GPIOH_15", "GPIOH_16", "GPIOH_17", "GPIOH_18", "GPIOH_19",
	"GPIOH_20", "GPIOH_21", "GPIOH_22", "GPIOH_23", "GPIOH_24",
	"GPIOH_25", "GPIOH_26", "GPIOH_27",

	"GPIOM_0", "GPIOM_1", "GPIOM_2", "GPIOM_3", "GPIOM_4",
	"GPIOM_5", "GPIOM_6", "GPIOM_7", "GPIOM_8", "GPIOM_9",
	"GPIOM_10", "GPIOM_11", "GPIOM_12", "GPIOM_13", "GPIOM_14",
	"GPIOM_15", "GPIOM_16", "GPIOM_17", "GPIOM_18", "GPIOM_19",
	"GPIOM_20", "GPIOM_21", "GPIOM_22", "GPIOM_23", "GPIOM_24",
	"GPIOM_25", "GPIOM_26", "GPIOM_27", "GPIOM_28", "GPIOM_29",
	"GPIOM_30"
};

static const char * const uart_a_groups[] = {
	"uart_a_tx_c", "uart_a_rx_c", "uart_a_cts_c", "uart_a_rts_c",
	"uart_a_tx_z", "uart_a_rx_z", "uart_a_cts_z", "uart_a_rts_z"
};

static const char * const uart_b_groups[] = {
	"uart_b_tx_w2", "uart_b_rx_w3", "uart_b_tx_w6", "uart_b_rx_w7",
	"uart_b_tx_w10", "uart_b_rx_w11", "uart_b_tx_d", "uart_b_rx_d"
};

static const char * const uart_c_groups[] = {
	"uart_c_tx_d", "uart_c_rx_d", "uart_c_cts_d", "uart_c_rts_d",
	"uart_c_tx_h1", "uart_c_rx_h2", "uart_c_cts_h3", "uart_c_rts_h4",
	"uart_c_tx_h22", "uart_c_rx_h23", "uart_c_tx_h18", "uart_c_rx_h19",
	"uart_c_tx_m", "uart_c_rx_m", "uart_c_cts_m", "uart_c_rts_m"
};

static const char * const uart_d_groups[] = {
	"uart_d_tx_z", "uart_d_rx_z", "uart_d_tx_m", "uart_d_rx_m"
};

static const char * const demod_uart_groups[] = {
	"demod_uart_tx_m12", "demod_uart_rx_m13",
	"demod_uart_tx_m14", "demod_uart_rx_m15"
};

static const char * const i2c0_groups[] = {
	"i2c0_sck_z", "i2c0_sda_z"
};

static const char * const i2c1_groups[] = {
	"i2c1_sck_d", "i2c1_sda_d", "i2c1_sda_c", "i2c1_sck_c"
};

static const char * const i2c2_groups[] = {
	"i2c2_sck_e", "i2c2_sda_e", "i2c2_sck_h20", "i2c2_sda_h21",
	"i2c2_sck_h10", "i2c2_sda_h11", "i2c2_sck_h24", "i2c2_sda_h25",
	"i2c2_sck_m", "i2c2_sda_m"
};

static const char * const i2c3_groups[] = {
	"i2c3_sck_h22", "i2c3_sda_h23", "i2c3_sck_m", "i2c3_sda_m"
};

static const char * const i2c4_groups[] = {
	"i2c4_sck_h26", "i2c4_sda_h27", "i2c4_sck_m", "i2c4_sda_m"
};

static const char * const i2c_slave_groups[] = {
	"i2c_slave_sck", "i2c_slave_sda"
};

static const char * const pwm_a_groups[] = {
	"pwm_a_e", "pwm_a_c"
};

static const char * const pwm_b_groups[] = {
	"pwm_b_e", "pwm_b_c", "pwm_b_z", "pwm_b_h14"
};

static const char * const pwm_c_groups[] = {
	"pwm_c_d5", "pwm_c_d7"
};

static const char * const pwm_d_groups[] = {
	"pwm_d_d6", "pwm_d_d9", "pwm_d_c", "pwm_d_z",
	"pwm_d_h5", "pwm_d_h12", "pwm_d_m1", "pwm_d_m24"
};

static const char * const pwm_e_groups[] = {
	"pwm_e_d", "pwm_e_z"
};

static const char * const pwm_f_groups[] = {
	"pwm_f_c", "pwm_f_h13", "pwm_f_m23", "pwm_f_m26"
};

static const char * const pwm_g_groups[] = {
	"pwm_g_z", "pwm_g_m8"
};

static const char * const pwm_h_groups[] = {
	"pwm_h_z", "pwm_h_m9"
};

static const char * const pwm_i_groups[] = {
	"pwm_i_z", "pwm_i_m10"
};

static const char * const pwm_j_groups[] = {
	"pwm_j_z", "pwm_j_m11"
};

static const char * const pwm_c_hiz_groups[] = {
	"pwm_c_hiz_d"
};

static const char * const pwm_d_hiz_groups[] = {
	"pwm_d_hiz_d"
};

static const char * const pwm_vs_groups[] = {
	"pwm_vs_h12", "pwm_vs_h13"
};

static const char * const remote_out_groups[] = {
	"remote_out_d1", "remote_out_d9", "remote_out_b"
};

static const char * const remote_input_groups[] = {
	"remote_input"
};

static const char * const jtag_a_groups[] = {
	"jtag_a_clk", "jtag_a_tms", "jtag_a_tdi", "jtag_a_tdo"
};

static const char * const gen_clk_groups[] = {
	"gen_clk_out_d", "gen_clk_out_h", "gen_clk_out_m"
};

static const char * const clk12_24_groups[] = {
	"clk12_24m_h", "clk12_24_m"
};

static const char * const clk_32k_in_groups[] = {
	"clk_32k_in"
};

static const char * const emmc_groups[] = {
	"emmc_nand_d0", "emmc_nand_d1", "emmc_nand_d2", "emmc_nand_d3",
	"emmc_nand_d4", "emmc_nand_d5", "emmc_nand_d6", "emmc_nand_d7",
	"emmc_clk", "emmc_cmd", "emmc_nand_ds"
};

static const char * const nand_groups[] = {
	"emmc_nand_d0", "emmc_nand_d1", "emmc_nand_d2", "emmc_nand_d3",
	"emmc_nand_d4", "emmc_nand_d5", "emmc_nand_d6", "emmc_nand_d7",
	"emmc_nand_ds", "nand_wen_clk", "nand_ale", "nand_ren_wr",
	"nand_cle", "nand_ce0"
};

static const char * const sdcard_groups[] = {
	"sdcard_d0", "sdcard_d1", "sdcard_d2", "sdcard_d3",
	"sdcard_clk", "sdcard_cmd", "sdcard_det"
};

static const char * const spi_a_groups[] = {
	"spi_a_miso_z0", "spi_a_mosi_z1", "spi_a_clk_z2", "spi_a_ss0_z3",
	"spi_a_ss1_z4", "spi_a_ss2_z5", "spi_a_clk_z18", "spi_a_ss1_z3",
	"spi_a_ss1_h7", "spi_a_ss0_h8", "spi_a_miso_h9", "spi_a_mosi_h10",
	"spi_a_clk_h11", "spi_a_ss2_h12", "spi_a_clk_h14", "spi_a_ss1_h19",
	"spi_a_miso_m", "spi_a_mosi_m", "spi_a_clk_m", "spi_a_ss0_m",
	"spi_a_ss1_m", "spi_a_ss2_m"
};

static const char * const spi_b_groups[] = {
	"spi_b_ss0_h", "spi_b_miso_h", "spi_b_mosi_h", "spi_b_clk_h"
};

static const char * const spi_c_groups[] = {
	"spi_c_miso", "spi_c_mosi", "spi_c_clk", "spi_c_ss0",
	"spi_c_ss1", "spi_c_ss2"
};

static const char * const spif_groups[] = {
	"spif_hold", "spif_mo", "spif_mi", "spif_clk",
	"spif_wp", "spif_cs"
};

static const char * const pdm_groups[] = {
	"pdm_din1_z1", "pdm_dclk_z2", "pdm_din0_z3", "pdm_din1_z6",
	"pdm_din1_h14", "pdm_din2_h16", "pdm_din3_h17", "pdm_din0_h18",
	"pdm_dclk_h19", "pdm_din2_h22", "pdm_dclk_h23", "pdm_din2_m",
	"pdm_din3_m", "pdm_din0_m", "pdm_dclk_m"
};

static const char * const eth_groups[] = {
	"eth_mdio_c", "eth_mdc_c", "eth_rgmii_rx_clk_c", "eth_rx_dv_c",
	"eth_rxd0_c", "eth_rxd1_c", "eth_rxd2_rgmii_c", "eth_rxd3_rgmii_c",
	"eth_rgmii_tx_clk_c", "eth_txen_c", "eth_txd0_c", "eth_txd1_c",
	"eth_txd2_rgmii_c", "eth_txd3_rgmii_c", "eth_link_led_c",
	"eth_act_led_c",

	"eth_mdio_z", "eth_mdc_z", "eth_link_led_z", "eth_rgmii_rx_clk_z",
	"eth_rx_dv_z", "eth_rxd0_z", "eth_rxd1_z", "eth_rxd2_rgmii_z",
	"eth_rxd3_rgmii_z", "eth_rgmii_tx_clk_z", "eth_txen_z", "eth_txd0_z",
	"eth_txd1_z", "eth_txd2_rgmii_z", "eth_txd3_rgmii_z", "eth_act_led_z",

	"eth_act_led_h", "eth_link_led_h"
};

static const char * const mic_mute_groups[] = {
	"mic_mute_key", "mic_mute_led"
};

static const char * const mclk_groups[] = {
	"mclk_2_z", "mclk_1_z", "mclk_1_h", "mclk_2_m"
};

static const char * const tdm_groups[] = {
	"tdm_fs1_c", "tdm_sclk1_c", "tdm_d4_c", "tdm_d5_c",
	"tdm_d6_c", "tdm_d7_c", "tdm_fs2_z", "tdm_sclk2_z",
	"tdm_d4_z", "tdm_d5_z", "tdm_d6_z", "tdm_d7_z",
	"tdm_d1_z", "tdm_d0_z", "tdm_sclk1_z", "tdm_fs1_z",
	"tdm_d3_h6", "tdm_d3_h13", "tdm_fs1_h15", "tdm_sclk1_h16",
	"tdm_d0_h17", "tdm_d1_h18", "tdm_d2_h19", "tdm_d3_h20",
	"tdm_d4_h21", "tdm_d5_h22", "tdm_d6_h23", "tdm_fs2_m",
	"tdm_sclk2_m", "tdm_d4_m", "tdm_d5_m", "tdm_d6_m", "tdm_d7_m"
};

static const char * const cec_groups[] = {
	"cec"
};

static const char * const spdif_in_groups[] = {
	"spdif_in_d", "spdif_in_z", "spdif_in_m"
};

static const char * const spdif_out_groups[] = {
	"spdif_out_d", "spdif_out_z19", "spdif_out_z2", "spdif_out_m"
};

static const char * const pcieck_reqn_groups[] = {
	"pcieck_reqn_c", "pcieck_reqn_z", "pcieck_reqn_m"
};

static const char * const iso7816_groups[] = {
	"iso7816_clk_z", "iso7816_data_z", "iso7816_clk_m", "iso7816_data_m"
};

static const char * const hdmirx_groups[] = {
	"hdmirx_hpd_a", "hdmirx_5vdet_a", "hdmirx_sda_a", "hdmirx_scl_a",
	"hdmirx_hpd_c", "hdmirx_5vdet_c", "hdmirx_sda_c", "hdmirx_scl_c",
	"hdmirx_hpd_b", "hdmirx_5vdet_b", "hdmirx_sda_b", "hdmirx_scl_b"
};

static const char * const atv_if_agc_groups[] = {
	"atv_if_agc_d", "atv_if_agc_z6", "atv_if_agc_z18"
};

static const char * const dtv_rf_agc_groups[] = {
	"dtv_rf_agc_z6", "dtv_rf_agc_z18"
};

static const char * const dtv_if_agc_groups[] = {
	"dtv_if_agc_d", "dtv_if_agc_z6", "dtv_if_agc_z18"
};

static const char * const tsout_groups[] = {
	"tsout_clk_z", "tsout_sop_z", "tsout_valid_z", "tsout_d0_z",
	"tsout_d1_z", "tsout_d2_z", "tsout_d3_z", "tsout_d4_z",
	"tsout_d5_z", "tsout_d6_z", "tsout_d7_z"
};

static const char * const tsin_a_groups[] = {
	"tsin_a_clk", "tsin_a_sop", "tsin_a_valid", "tsin_a_din0"
};

static const char * const tsin_b_groups[] = {
	"tsin_b_clk", "tsin_b_sop", "tsin_b_valid", "tsin_b_din0",
	"tsin_b_din1", "tsin_b_din2", "tsin_b_din3", "tsin_b_din4",
	"tsin_b_din5", "tsin_b_din6", "tsin_b_din7"
};

static const char * const cicam_groups[] = {
	"cicam_a2", "cicam_a3", "cicam_a4", "cicam_a5",
	"cicam_a6", "cicam_a7", "cicam_a8", "cicam_a9",
	"cicam_a10", "cicam_a11", "cicam_a13", "cicam_a14",
	"cicam_a12", "cicam_data0", "cicam_data1", "cicam_data2",
	"cicam_data3", "cicam_data4", "cicam_data5", "cicam_data6",
	"cicam_data7", "cicam_a0", "cicam_a1", "cicam_cen",
	"cicam_oen", "cicam_wen", "cicam_iordn", "cicam_iowrn",
	"cicam_reset", "cicam_cdn", "cicam_5ven", "cicam_waitn",
	"cicam_irqn"
};

static const char * const diseqc_out_groups[] = {
	"diseqc_out_z18", "diseqc_out_z0"
};

static const char * const diseqc_in_groups[] = {
	"diseqc_in_z1"
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

static const char * const sync_3d_groups[] = {
	"sync_3d_out"
};

static const char * const vx1_a_groups[] = {
	"vx1_a_lockn", "vx1_a_htpdn"
};

static const char * const vx1_b_groups[] = {
	"vx1_b_lockn", "vx1_b_htpdn"
};

static const char * const bt656_a_groups[] = {
	"bt656_a_vs", "bt656_a_hs", "bt656_a_clk", "bt656_a_din0",
	"bt656_a_din1", "bt656_a_din2", "bt656_a_din3", "bt656_a_din4",
	"bt656_a_din5", "bt656_a_din6", "bt656_a_din7"
};

static const char * const s2_demod_gpio_groups[] = {
	"s2_demod_gpio0_z", "s2_demod_gpio0_h", "s2_demod_gpio1",
	"s2_demod_gpio2", "s2_demod_gpio3", "s2_demod_gpio4",
	"s2_demod_gpio5", "s2_demod_gpio6", "s2_demod_gpio7"
};

static struct meson_pmx_func meson_t3_periphs_functions[] __initdata = {
	FUNCTION(gpio_periphs),
	FUNCTION(uart_a),
	FUNCTION(uart_b),
	FUNCTION(uart_c),
	FUNCTION(uart_d),
	FUNCTION(demod_uart),
	FUNCTION(i2c0),
	FUNCTION(i2c1),
	FUNCTION(i2c2),
	FUNCTION(i2c3),
	FUNCTION(i2c4),
	FUNCTION(i2c_slave),
	FUNCTION(pwm_a),
	FUNCTION(pwm_b),
	FUNCTION(pwm_c),
	FUNCTION(pwm_d),
	FUNCTION(pwm_e),
	FUNCTION(pwm_f),
	FUNCTION(pwm_g),
	FUNCTION(pwm_h),
	FUNCTION(pwm_i),
	FUNCTION(pwm_j),
	FUNCTION(pwm_c_hiz),
	FUNCTION(pwm_d_hiz),
	FUNCTION(pwm_vs),
	FUNCTION(remote_out),
	FUNCTION(remote_input),
	FUNCTION(jtag_a),
	FUNCTION(gen_clk),
	FUNCTION(clk12_24),
	FUNCTION(clk_32k_in),
	FUNCTION(emmc),
	FUNCTION(nand),
	FUNCTION(sdcard),
	FUNCTION(spi_a),
	FUNCTION(spi_b),
	FUNCTION(spi_c),
	FUNCTION(spif),
	FUNCTION(pdm),
	FUNCTION(eth),
	FUNCTION(mic_mute),
	FUNCTION(mclk),
	FUNCTION(tdm),
	FUNCTION(tsin_a),
	FUNCTION(tsin_b),
	FUNCTION(cec),
	FUNCTION(spdif_in),
	FUNCTION(spdif_out),
	FUNCTION(pcieck_reqn),
	FUNCTION(iso7816),
	FUNCTION(hdmirx),
	FUNCTION(atv_if_agc),
	FUNCTION(dtv_rf_agc),
	FUNCTION(dtv_if_agc),
	FUNCTION(tsout),
	FUNCTION(tsin_a),
	FUNCTION(tsin_b),
	FUNCTION(cicam),
	FUNCTION(diseqc_out),
	FUNCTION(diseqc_in),
	FUNCTION(tcon),
	FUNCTION(hsync),
	FUNCTION(vsync),
	FUNCTION(sync_3d),
	FUNCTION(vx1_a),
	FUNCTION(vx1_b),
	FUNCTION(bt656_a),
	FUNCTION(s2_demod_gpio)
};

static struct meson_bank meson_t3_periphs_banks[] = {
	/* name  first  last irq pullen  pull  dir  out  in  ds*/
	BANK_DS("GPIOD", GPIOD_0,    GPIOD_10, 43, 53,
		0x3,   0,  0x4,  0,   0x2,  0,  0x1,  0,  0x0,  0,  0x7, 0),
	BANK_DS("GPIOE", GPIOE_0,    GPIOE_1, 54, 55,
		0xb,  0,   0xc,  0,   0xa, 0,   0x9, 0,   0x8, 0,  0xf, 0),
	BANK_DS("GPIOZ", GPIOZ_0,    GPIOZ_15, 56, 71,
		0x13,  0,  0x14,  0,  0x12, 0,  0x11, 0,  0x10, 0,  0x17, 0),
	BANK_DS("GPIOZ1", GPIOZ_16,    GPIOZ_19, 72, 75,
		0x13, 16,  0x14, 16,  0x12, 16,  0x11, 16,  0x10, 16,  0xc0, 0),
	BANK_DS("GPIOH", GPIOH_0,    GPIOH_15, 107, 122,
		0x1b,  0,  0x1c,  0,  0x1a, 0,  0x19, 0,  0x18, 0,  0x1f, 0),
	BANK_DS("GPIOH1", GPIOH_16,    GPIOH_27, 123, 134,
		0x1b, 16,  0x1c, 16,  0x1a, 16,  0x19, 16,  0x18, 16,  0xc1, 0),
	BANK_DS("GPIOB", GPIOB_0,    GPIOB_13, 0, 13,
		0x2b,  0,  0x2c,  0,  0x2a, 0,  0x29, 0,  0x28, 0,  0x2f, 0),
	BANK_DS("GPIOC", GPIOC_0,    GPIOC_15, 14, 29,
		0x33,  0,  0x34,  0,  0x32, 0,  0x31, 0,  0x30, 0,  0x37, 0),
	BANK_DS("GPIOW", GPIOW_0,    GPIOW_12, 30, 42,
		0x63,  0,  0x64,  0,  0x62, 0,  0x61, 0,  0x60, 0,  0x67, 0),
	BANK_DS("GPIOM", GPIOM_0,    GPIOM_15, 76, 91,
		0x73,  0,  0x74,  0,  0x72, 0,  0x71, 0,  0x70, 0,  0x77, 0),
	BANK_DS("GPIOM1", GPIOM_16,    GPIOM_30, 92, 106,
		0x73, 16,  0x74, 16,  0x72, 16,  0x71, 16,  0x70, 16,  0xc2, 0),
	BANK_DS("GPIO_TEST_N", GPIO_TEST_N,    GPIO_TEST_N, 135, 135,
		0x83,  0,  0x84,  0,  0x82, 0,  0x81,  0, 0x80, 0, 0x87, 0)
};

static struct meson_pmx_bank meson_t3_periphs_pmx_banks[] = {
	/*name	            first	 lask        reg offset*/
	BANK_PMX("D",      GPIOD_0,     GPIOD_10,    0xb,  0),
	BANK_PMX("E",      GPIOE_0,     GPIOE_1,     0xd,  0),
	BANK_PMX("Z",      GPIOZ_0,     GPIOZ_19,    0x4,  0),
	BANK_PMX("H",      GPIOH_0,     GPIOH_27,    0x7,  0),
	BANK_PMX("B",      GPIOB_0,     GPIOB_13,    0x0,  0),
	BANK_PMX("C",      GPIOC_0,     GPIOC_15,    0x2,  0),
	BANK_PMX("W",      GPIOW_0,     GPIOW_12,    0x12, 0),
	BANK_PMX("M",      GPIOM_0,     GPIOM_30,    0xe,  0),
	BANK_PMX("TEST_N", GPIO_TEST_N, GPIO_TEST_N, 0x14, 0)
};

static struct meson_bank meson_t3_analog_banks[] = {
	/* name  first  last irq pullen  pull  dir  out  in  ds*/
	BANK_DS("ANALOG", CDAC_IOUT,    CVBS1, 136, 138,
		0x0,  29,  0x0,  29,   0x1,  0,  0x0,  29,  0x0,  0,  0x0, 29)
};

static struct meson_pmx_bank meson_t3_analog_pmx_banks[] = {
	/*name	            first	 lask        reg offset*/
	BANK_PMX("ANALOG", CDAC_IOUT,   CVBS1,    0x0,  29)
};

static struct meson_axg_pmx_data meson_t3_periphs_pmx_banks_data = {
	.pmx_banks	= meson_t3_periphs_pmx_banks,
	.num_pmx_banks	= ARRAY_SIZE(meson_t3_periphs_pmx_banks)
};

static struct meson_pinctrl_data meson_t3_periphs_pinctrl_data __refdata = {
	.name		= "periphs-banks",
	.pins		= meson_t3_periphs_pins,
	.groups		= meson_t3_periphs_groups,
	.funcs		= meson_t3_periphs_functions,
	.banks		= meson_t3_periphs_banks,
	.num_pins	= ARRAY_SIZE(meson_t3_periphs_pins),
	.num_groups	= ARRAY_SIZE(meson_t3_periphs_groups),
	.num_funcs	= ARRAY_SIZE(meson_t3_periphs_functions),
	.num_banks	= ARRAY_SIZE(meson_t3_periphs_banks),
	.pmx_ops	= &meson_axg_pmx_ops,
	.pmx_data	= &meson_t3_periphs_pmx_banks_data,
	.parse_dt	= &meson_a1_parse_dt_extra
};

static struct meson_axg_pmx_data meson_t3_analog_pmx_banks_data = {
	.pmx_banks	= meson_t3_analog_pmx_banks,
	.num_pmx_banks	= ARRAY_SIZE(meson_t3_analog_pmx_banks)
};

static struct meson_pinctrl_data meson_t3_analog_pinctrl_data __refdata = {
	.name		= "analog-banks",
	.pins		= meson_t3_analog_pins,
	.groups		= meson_t3_analog_groups,
	.funcs		= meson_t3_analog_functions,
	.banks		= meson_t3_analog_banks,
	.num_pins	= ARRAY_SIZE(meson_t3_analog_pins),
	.num_groups	= ARRAY_SIZE(meson_t3_analog_groups),
	.num_funcs	= ARRAY_SIZE(meson_t3_analog_functions),
	.num_banks	= ARRAY_SIZE(meson_t3_analog_banks),
	.pmx_ops	= &meson_axg_pmx_ops,
	.pmx_data	= &meson_t3_analog_pmx_banks_data,
	.parse_dt	= &meson_a1_parse_dt_extra
};

static const struct of_device_id meson_t3_pinctrl_dt_match[] = {
	{
		.compatible = "amlogic,meson-t3-periphs-pinctrl",
		.data = &meson_t3_periphs_pinctrl_data,
	},
	{
		.compatible = "amlogic,meson-t3-analog-pinctrl",
		.data = &meson_t3_analog_pinctrl_data,
	},
	{ }
};

static struct platform_driver meson_t3_pinctrl_driver = {
	.probe  = meson_pinctrl_probe,
	.driver = {
		.name	= "meson-t3-pinctrl",
		.of_match_table = meson_t3_pinctrl_dt_match,
	}
};

#ifndef MODULE
static int __init t3_pmx_init(void)
{
	meson_t3_pinctrl_driver.driver.of_match_table =
			meson_t3_pinctrl_dt_match;
	return platform_driver_register(&meson_t3_pinctrl_driver);
}
arch_initcall(t3_pmx_init);

#else
int __init meson_t3_pinctrl_init(void)
{
	meson_t3_pinctrl_driver.driver.of_match_table =
			meson_t3_pinctrl_dt_match;
	return platform_driver_register(&meson_t3_pinctrl_driver);
}
#endif
