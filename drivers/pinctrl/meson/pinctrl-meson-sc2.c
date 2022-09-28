// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <dt-bindings/gpio/meson-sc2-gpio.h>
#include "pinctrl-meson.h"
#include "pinctrl-meson-axg-pmx.h"
#include "pinctrl-module-init.h"

static const struct pinctrl_pin_desc meson_sc2_periphs_pins[] = {
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
	MESON_PIN(GPIOD_11),
	MESON_PIN(GPIOE_0),
	MESON_PIN(GPIOE_1),
	MESON_PIN(GPIOE_2),
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
	MESON_PIN(GPIOB_14),
	MESON_PIN(GPIOB_15),
	MESON_PIN(GPIOC_0),
	MESON_PIN(GPIOC_1),
	MESON_PIN(GPIOC_2),
	MESON_PIN(GPIOC_3),
	MESON_PIN(GPIOC_4),
	MESON_PIN(GPIOC_5),
	MESON_PIN(GPIOC_6),
	MESON_PIN(GPIOC_7),
	MESON_PIN(GPIOX_0),
	MESON_PIN(GPIOX_1),
	MESON_PIN(GPIOX_2),
	MESON_PIN(GPIOX_3),
	MESON_PIN(GPIOX_4),
	MESON_PIN(GPIOX_5),
	MESON_PIN(GPIOX_6),
	MESON_PIN(GPIOX_7),
	MESON_PIN(GPIOX_8),
	MESON_PIN(GPIOX_9),
	MESON_PIN(GPIOX_10),
	MESON_PIN(GPIOX_11),
	MESON_PIN(GPIOX_12),
	MESON_PIN(GPIOX_13),
	MESON_PIN(GPIOX_14),
	MESON_PIN(GPIOX_15),
	MESON_PIN(GPIOX_16),
	MESON_PIN(GPIOX_17),
	MESON_PIN(GPIOX_18),
	MESON_PIN(GPIOX_19),
	MESON_PIN(GPIOH_0),
	MESON_PIN(GPIOH_1),
	MESON_PIN(GPIOH_2),
	MESON_PIN(GPIOH_3),
	MESON_PIN(GPIOH_4),
	MESON_PIN(GPIOH_5),
	MESON_PIN(GPIOH_6),
	MESON_PIN(GPIOH_7),
	MESON_PIN(GPIOH_8),
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
	MESON_PIN(GPIOA_14),
	MESON_PIN(GPIOA_15),
	MESON_PIN(GPIO_TEST_N),
};

/*bank D func1 */
static const unsigned int uart_b_tx_d_pins[]		= { GPIOD_0 };
static const unsigned int uart_b_rx_d_pins[]		= { GPIOD_1 };
static const unsigned int i2c4_scl_d_pins[]		= { GPIOD_2 };
static const unsigned int i2c4_sda_d_pins[]		= { GPIOD_3 };
static const unsigned int remote_out_d4_pins[]		= { GPIOD_4 };
static const unsigned int remote_input_d5_pins[]	= { GPIOD_5 };
static const unsigned int jtag_2_clk_pins[]		= { GPIOD_6 };
static const unsigned int jtag_2_tms_pins[]		= { GPIOD_7 };
static const unsigned int jtag_2_tdi_pins[]		= { GPIOD_8 };
static const unsigned int jtag_2_tdo_pins[]		= { GPIOD_9 };
static const unsigned int cec_a_d_pins[]		= { GPIOD_10 };

/*bank D func2 */
static const unsigned int uart_a_tx_d2_pins[]		= { GPIOD_2 };
static const unsigned int uart_a_rx_d3_pins[]		= { GPIOD_3 };
static const unsigned int clk_32k_in_pins[]		= { GPIOD_4 };
static const unsigned int remote_out_d9_pins[]		= { GPIOD_9 };
static const unsigned int cec_b_d_pins[]		= { GPIOD_10 };
static const unsigned int pwm_g_hiz_pins[]		= { GPIOD_11 };

/*bank D func3 */
static const unsigned int i2c_slave_scl_pins[]		= { GPIOD_2 };
static const unsigned int i2c_slave_sda_pins[]		= { GPIOD_3 };
static const unsigned int pwm_i_d4_pins[]		= { GPIOD_4 };
static const unsigned int pwm_j_d5_pins[]		= { GPIOD_5 };
static const unsigned int pwm_i_d6_pins[]		= { GPIOD_6 };
static const unsigned int uart_a_tx_d8_pins[]		= { GPIOD_8 };
static const unsigned int uart_a_rx_d9_pins[]		= { GPIOD_9 };
static const unsigned int pwm_j_d10_pins[]		= { GPIOD_10 };
static const unsigned int pwm_g_pins[]			= { GPIOD_11 };

/*bank D func4 */
static const unsigned int pwm_i_hiz_pins[]		= { GPIOD_4 };
static const unsigned int tsin_a_sop_d_pins[]		= { GPIOD_6 };
static const unsigned int tsin_a_din0_d_pins[]		= { GPIOD_7 };
static const unsigned int tsin_a_clk_d_pins[]		= { GPIOD_8 };
static const unsigned int tsin_a_valid_d_pins[]		= { GPIOD_9 };
static const unsigned int spdif_out_d_pins[]		= { GPIOD_10 };
static const unsigned int gen_clk_d_pins[]		= { GPIOD_11 };

/*bank D func5 */
static const unsigned int mic_mute_en_pins[]		= { GPIOD_2 };
static const unsigned int tdm_d0_pins[]			= { GPIOD_4 };
static const unsigned int tdm_d1_pins[]			= { GPIOD_6 };
static const unsigned int tdm_fs1_pins[]		= { GPIOD_7 };
static const unsigned int tdm_sclk1_pins[]		= { GPIOD_8 };
static const unsigned int mclk1_pins[]			= { GPIOD_9 };
static const unsigned int tdm_d2_d_pins[]		= { GPIOD_10 };

/*bank D func6 */
static const unsigned int clk12_24_d_pins[]		= { GPIOD_10 };

/*bank E func1 */
static const unsigned int uart_b_cts_pins[]		= { GPIOE_0 };
static const unsigned int uart_b_rts_pins[]		= { GPIOE_1 };
static const unsigned int clk12_24_e_pins[]		= { GPIOE_2 };

/*bank E funsc2 */
static const unsigned int uart_a_cts_pins[]		= { GPIOE_0 };
static const unsigned int uart_a_rts_pins[]		= { GPIOE_1 };
static const unsigned int clk25_pins[]			= { GPIOE_2 };

/*bank E func3 */
static const unsigned int pwm_h_pins[]			= { GPIOE_0 };
static const unsigned int pwm_j_e_pins[]		= { GPIOE_1 };
static const unsigned int pwm_a_e_pins[]		= { GPIOE_2 };

/*bank E func4 */
static const unsigned int i2c4_scl_e_pins[]		= { GPIOE_0 };
static const unsigned int i2c4_sda_e_pins[]		= { GPIOE_1 };

/*bank E func5 */
static const unsigned int mic_mute_key_pins[]		= { GPIOE_2 };

/*bank B func1 */
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
static const unsigned int emmc_nand_dqs_pins[]		= { GPIOB_13 };

/*bank B funsc2 */
static const unsigned int nand_wen_clk_pins[]		= { GPIOB_8 };
static const unsigned int nand_ale_pins[]		= { GPIOB_9 };
static const unsigned int nand_cle_pins[]		= { GPIOB_10 };
static const unsigned int nand_ce0_pins[]		= { GPIOB_11 };
static const unsigned int nand_ren_wr_pins[]		= { GPIOB_12 };
static const unsigned int nand_rb0_pins[]		= { GPIOB_14 };
static const unsigned int nand_ce1_pins[]		= { GPIOB_15 };

/*bank B func3 */
static const unsigned int nor_hold_pins[]		= { GPIOB_3 };
static const unsigned int nor_d_pins[]			= { GPIOB_4 };
static const unsigned int nor_q_pins[]			= { GPIOB_5 };
static const unsigned int nor_c_pins[]			= { GPIOB_6 };
static const unsigned int nor_wp_pins[]			= { GPIOB_7 };
static const unsigned int nor_cs_pins[]			= { GPIOB_14 };

/*bank C func1 */
static const unsigned int sdcard_d0_c_pins[]		= { GPIOC_0 };
static const unsigned int sdcard_d1_c_pins[]		= { GPIOC_1 };
static const unsigned int sdcard_d2_c_pins[]		= { GPIOC_2 };
static const unsigned int sdcard_d3_c_pins[]		= { GPIOC_3 };
static const unsigned int sdcard_clk_c_pins[]		= { GPIOC_4 };
static const unsigned int sdcard_cmd_c_pins[]		= { GPIOC_5 };
static const unsigned int sdcard_cd_pins[]		= { GPIOC_6 };
static const unsigned int pcieck_reqn_pins[]		= { GPIOC_7 };

/*bank C funsc2 */
static const unsigned int jtag_1_tdo_pins[]		= { GPIOC_0 };
static const unsigned int jtag_1_tdi_pins[]		= { GPIOC_1 };
static const unsigned int uart_b_rx_c_pins[]		= { GPIOC_2 };
static const unsigned int uart_b_tx_c_pins[]		= { GPIOC_3 };
static const unsigned int jtag_1_clk_pins[]		= { GPIOC_4 };
static const unsigned int jtag_1_tms_pins[]		= { GPIOC_5 };

/*bank C func3 */
static const unsigned int tsin_c_valid_c_pins[]		= { GPIOC_0 };
static const unsigned int tsin_c_sop_c_pins[]		= { GPIOC_1 };
static const unsigned int tsin_c_din0_c_pins[]		= { GPIOC_2 };
static const unsigned int tsin_c_clk_c_pins[]		= { GPIOC_3 };
static const unsigned int i2c0_sda_c_pins[]		= { GPIOC_5 };
static const unsigned int i2c0_scl_c_pins[]		= { GPIOC_6 };

/*bank C func4 */
static const unsigned int pdm_din0_c_pins[]		= { GPIOC_0 };
static const unsigned int pdm_din1_c1_pins[]		= { GPIOC_1 };
static const unsigned int pdm_din2_c_pins[]		= { GPIOC_2 };
static const unsigned int pdm_din3_c_pins[]		= { GPIOC_3 };
static const unsigned int pdm_dclk_c_pins[]		= { GPIOC_4 };
static const unsigned int pdm_din1_c7_pins[]		= { GPIOC_7 };

/*bank C func5 */
static const unsigned int spi_a_mosi_c_pins[]		= { GPIOC_0 };
static const unsigned int spi_a_miso_c_pins[]		= { GPIOC_1 };
static const unsigned int spi_a_ss0_c_pins[]		= { GPIOC_2 };
static const unsigned int spi_a_sclk_c_pins[]		= { GPIOC_3 };
static const unsigned int pwm_c_c_pins[]		= { GPIOC_4 };
static const unsigned int iso7816_clk_c_pins[]		= { GPIOC_5 };
static const unsigned int iso7816_data_c_pins[]		= { GPIOC_6 };

/*bank X func1*/
static const unsigned int sdio_d0_x_pins[]		= { GPIOX_0 };
static const unsigned int sdio_d1_x_pins[]		= { GPIOX_1 };
static const unsigned int sdio_d2_x_pins[]		= { GPIOX_2 };
static const unsigned int sdio_d3_x_pins[]		= { GPIOX_3 };
static const unsigned int sdio_clk_x_pins[]		= { GPIOX_4 };
static const unsigned int sdio_cmd_x_pins[]		= { GPIOX_5 };
static const unsigned int pwm_a_x_pins[]		= { GPIOX_6 };
static const unsigned int pwm_f_x_pins[]		= { GPIOX_7 };
static const unsigned int tdm_d3_pins[]			= { GPIOX_8 };
static const unsigned int tdm_d4_pins[]			= { GPIOX_9 };
static const unsigned int tdm_fs0_pins[]		= { GPIOX_10 };
static const unsigned int tdm_sclk0_pins[]		= { GPIOX_11 };
static const unsigned int uart_e_tx_pins[]		= { GPIOX_12 };
static const unsigned int uart_e_rx_pins[]		= { GPIOX_13 };
static const unsigned int uart_e_cts_pins[]		= { GPIOX_14 };
static const unsigned int uart_e_rts_pins[]		= { GPIOX_15 };
static const unsigned int pwm_e_pins[]			= { GPIOX_16 };
static const unsigned int i2c2_sda_x_pins[]		= { GPIOX_17 };
static const unsigned int i2c2_scl_x_pins[]		= { GPIOX_18 };
static const unsigned int pwm_b_x19_pins[]		= { GPIOX_19 };

/*bank X func2*/
static const unsigned int pdm_din0_x_pins[]		= { GPIOX_0 };
static const unsigned int pdm_din1_x_pins[]		= { GPIOX_1 };
static const unsigned int pdm_din2_x_pins[]		= { GPIOX_2 };
static const unsigned int pdm_din3_x_pins[]		= { GPIOX_3 };
static const unsigned int pdm_dclk_x_pins[]		= { GPIOX_4 };
static const unsigned int mclk_0_pins[]			= { GPIOX_5 };
static const unsigned int uart_d_tx_x6_pins[]		= { GPIOX_6 };
static const unsigned int uart_d_rx_x7_pins[]		= { GPIOX_7 };
static const unsigned int uart_d_cts_pins[]		= { GPIOX_8 };
static const unsigned int uart_d_rts_pins[]		= { GPIOX_9 };
static const unsigned int uart_d_tx_x10_pins[]		= { GPIOX_10 };
static const unsigned int uart_d_rx_x11_pins[]		= { GPIOX_11};
static const unsigned int tdm_d15_pins[]		= { GPIOX_19};

/*bank X func3*/
static const unsigned int tsin_a_din0_x_pins[]		= { GPIOX_0 };
static const unsigned int tsin_a_sop_x_pins[]		= { GPIOX_1 };
static const unsigned int tsin_a_valid_x_pins[]		= { GPIOX_2 };
static const unsigned int tsin_a_clk_x_pins[]		= { GPIOX_3 };
static const unsigned int tsin_d_sop_x_pins[]		= { GPIOX_8 };
static const unsigned int tsin_d_valid_x_pins[]		= { GPIOX_9 };
static const unsigned int tsin_d_din0_x_pins[]		= { GPIOX_10 };
static const unsigned int tsin_d_clk_x_pins[]		= { GPIOX_11 };

/*bank X func4*/
static const unsigned int pwm_d_x3_pins[]		= { GPIOX_3 };
static const unsigned int pwm_c_x_pins[]		= { GPIOX_5 };
static const unsigned int pwm_d_x6_pins[]		= { GPIOX_6 };
static const unsigned int pwm_b_x7_pins[]		= { GPIOX_7 };
static const unsigned int spi_a_mosi_x_pins[]		= { GPIOX_8 };
static const unsigned int spi_a_miso_x_pins[]		= { GPIOX_9 };
static const unsigned int spi_a_ss0_x_pins[]		= { GPIOX_10 };
static const unsigned int spi_a_sclk_x_pins[]		= { GPIOX_11 };

/*bank X func5 */
static const unsigned int sdcard_d0_x_pins[]		= { GPIOX_0 };
static const unsigned int sdcard_d1_x_pins[]		= { GPIOX_1 };
static const unsigned int sdcard_d2_x_pins[]		= { GPIOX_2 };
static const unsigned int sdcard_d3_x_pins[]		= { GPIOX_3 };
static const unsigned int sdcard_clk_x_pins[]		= { GPIOX_4 };
static const unsigned int sdcard_cd_x_pins[]		= { GPIOX_5 };
static const unsigned int iso7816_clk_x_pins[]		= { GPIOX_8 };
static const unsigned int iso7816_data_x_pins[]		= { GPIOX_9 };
static const unsigned int i2c1_sda_x_pins[]		= { GPIOX_10 };
static const unsigned int i2c1_scl_x_pins[]		= { GPIOX_11 };
static const unsigned int gen_clk_x_pins[]		= { GPIOX_19 };

/*bank H func1 */
static const unsigned int hdmitx_sda_pins[]		= { GPIOH_0 };
static const unsigned int hdmitx_sck_pins[]		= { GPIOH_1 };
static const unsigned int hdmitx_hpd_in_pins[]		= { GPIOH_2 };
static const unsigned int spdif_out_h_pins[]		= { GPIOH_4 };
static const unsigned int spdif_in_h_pins[]		= { GPIOH_5 };
static const unsigned int iso7816_clk_h_pins[]		= { GPIOH_6 };
static const unsigned int iso7816_data_h_pins[]		= { GPIOH_7 };

/*bank H func2 */
static const unsigned int i2c3_sda_h_pins[]		= { GPIOH_0 };
static const unsigned int i2c3_scl_h_pins[]		= { GPIOH_1 };
static const unsigned int i2c1_sda_h2_pins[]		= { GPIOH_2 };
static const unsigned int i2c1_scl_h3_pins[]		= { GPIOH_3 };
static const unsigned int uart_c_rts_pins[]		= { GPIOH_4 };
static const unsigned int uart_c_cts_pins[]		= { GPIOH_5 };
static const unsigned int uart_c_rx_pins[]		= { GPIOH_6 };
static const unsigned int uart_c_tx_pins[]		= { GPIOH_7 };

/*bank H func3 */
static const unsigned int spi_b_mosi_h_pins[]		= { GPIOH_4 };
static const unsigned int spi_b_miso_h_pins[]		= { GPIOH_5 };
static const unsigned int spi_b_ss0_h_pins[]		= { GPIOH_6 };
static const unsigned int spi_b_sclk_h_pins[]		= { GPIOH_7 };

/*bank H func4 */
static const unsigned int cec_a_h_pins[]		= { GPIOH_3 };
static const unsigned int pwm_f_h_pins[]		= { GPIOH_5 };
static const unsigned int i2c1_sda_h6_pins[]		= { GPIOH_6 };
static const unsigned int i2c1_scl_h7_pins[]		= { GPIOH_7 };

/*bank H func5 */
static const unsigned int cec_b_h_pins[]		= { GPIOH_3 };
static const unsigned int tdm_d5_pins[]			= { GPIOH_5 };
static const unsigned int remote_out_h_pins[]		= { GPIOH_6 };
static const unsigned int pwm_b_h_pins[]		= { GPIOH_7 };

/*bank H func6 */
static const unsigned int i2c0_sda_h_pins[]		= { GPIOH_4 };
static const unsigned int i2c0_scl_h_pins[]		= { GPIOH_5 };

/*bank Z func1 */
static const unsigned int eth_mdio_pins[]		= { GPIOZ_0 };
static const unsigned int eth_mdc_pins[]		= { GPIOZ_1 };
static const unsigned int eth_rgmii_rx_clk_pins[]	= { GPIOZ_2 };
static const unsigned int eth_rx_dv_pins[]		= { GPIOZ_3 };
static const unsigned int eth_rxd0_pins[]		= { GPIOZ_4 };
static const unsigned int eth_rxd1_pins[]		= { GPIOZ_5 };
static const unsigned int eth_rxd2_rgmii_pins[]		= { GPIOZ_6 };
static const unsigned int eth_rxd3_rgmii_pins[]		= { GPIOZ_7 };
static const unsigned int eth_rgmii_tx_clk_pins[]	= { GPIOZ_8 };
static const unsigned int eth_txen_pins[]		= { GPIOZ_9 };
static const unsigned int eth_txd0_pins[]		= { GPIOZ_10 };
static const unsigned int eth_txd1_pins[]		= { GPIOZ_11 };
static const unsigned int eth_txd2_rgmii_pins[]		= { GPIOZ_12 };
static const unsigned int eth_txd3_rgmii_pins[]		= { GPIOZ_13 };
static const unsigned int eth_link_led_pins[]		= { GPIOZ_14 };
static const unsigned int eth_act_led_pins[]		= { GPIOZ_15 };

/*bank Z func2 */
static const unsigned int pwm_d_z_pins[]		= { GPIOZ_2 };
static const unsigned int clk12_24_z_pins[]		= { GPIOZ_13 };

/*bank Z func3 */
static const unsigned int iso7816_clk_z0_pins[]		= { GPIOZ_0 };
static const unsigned int iso7816_data_z1_pins[]	= { GPIOZ_1 };
static const unsigned int tsin_b_valid_pins[]		= { GPIOZ_2 };
static const unsigned int tsin_b_sop_pins[]		= { GPIOZ_3 };
static const unsigned int tsin_b_din0_pins[]		= { GPIOZ_4 };
static const unsigned int tsin_b_clk_pins[]		= { GPIOZ_5 };
static const unsigned int tsin_b_fail_pins[]		= { GPIOZ_6 };
static const unsigned int tsin_b_din1_pins[]		= { GPIOZ_7 };
static const unsigned int tsin_b_din2_pins[]		= { GPIOZ_8 };
static const unsigned int tsin_b_din3_pins[]		= { GPIOZ_9 };
static const unsigned int tsin_b_din4_pins[]		= { GPIOZ_10 };
static const unsigned int tsin_b_din5_pins[]		= { GPIOZ_11 };
static const unsigned int tsin_b_din6_pins[]		= { GPIOZ_12 };
static const unsigned int tsin_b_din7_pins[]		= { GPIOZ_13 };
static const unsigned int i2c2_sda_z14_pins[]		= { GPIOZ_14 };
static const unsigned int i2c2_scl_z15_pins[]		= { GPIOZ_15 };

/*bank Z func4 */
static const unsigned int i2c0_sda_z0_pins[]		= { GPIOZ_0 };
static const unsigned int i2c0_scl_z1_pins[]		= { GPIOZ_1 };
static const unsigned int tdm_d6_pins[]			= { GPIOZ_2 };
static const unsigned int tdm_d7_pins[]			= { GPIOZ_3 };
static const unsigned int tdm_d8_pins[]			= { GPIOZ_4 };
static const unsigned int tdm_d9_pins[]			= { GPIOZ_5 };
static const unsigned int tdm_fs2_pins[]		= { GPIOZ_6 };
static const unsigned int tdm_sclk2_pins[]		= { GPIOZ_7 };
static const unsigned int mclk_2_pins[]			= { GPIOZ_8 };
static const unsigned int tdm_d10_pins[]		= { GPIOZ_9 };
static const unsigned int tdm_d11_pins[]		= { GPIOZ_10 };
static const unsigned int tdm_d12_pins[]		= { GPIOZ_11 };
static const unsigned int tdm_d13_pins[]		= { GPIOZ_12 };
static const unsigned int tdm_d14_pins[]		= { GPIOZ_13 };

/*bank Z func5 */
static const unsigned int pwm_b_z0_pins[]		= { GPIOZ_0 };
static const unsigned int pwm_c_z_pins[]		= { GPIOZ_1 };
static const unsigned int sdcard_d0_z_pins[]		= { GPIOZ_2 };
static const unsigned int sdcard_d1_z_pins[]		= { GPIOZ_3 };
static const unsigned int sdcard_d2_z_pins[]		= { GPIOZ_4 };
static const unsigned int sdcard_d3_z_pins[]		= { GPIOZ_5 };
static const unsigned int sdcard_clk_z_pins[]		= { GPIOZ_6 };
static const unsigned int sdcard_cmd_z_pins[]		= { GPIOZ_7 };
static const unsigned int iso7816_clk_z8_pins[]		= { GPIOZ_8 };
static const unsigned int iso7816_data_z9_pins[]	= { GPIOZ_9 };
static const unsigned int remote_out_z_pins[]		= { GPIOZ_10 };
static const unsigned int pwm_f_z_pins[]		= { GPIOZ_12 };
static const unsigned int pwm_b_z13_pins[]		= { GPIOZ_13 };

/*bank Z func6 */
static const unsigned int i2c1_sda_z_pins[]		= { GPIOZ_0 };
static const unsigned int i2c1_scl_z_pins[]		= { GPIOZ_1 };
static const unsigned int tdm_fs3_pins[]		= { GPIOZ_2 };
static const unsigned int tdm_sclk3_pins[]		= { GPIOZ_3 };
static const unsigned int tdm_fs4_pins[]		= { GPIOZ_4 };
static const unsigned int tdm_sclk4_pins[]		= { GPIOZ_5 };
static const unsigned int tsin_c_valid_z_pins[]		= { GPIOZ_6 };
static const unsigned int tsin_c_sop_z_pins[]		= { GPIOZ_7 };
static const unsigned int tsin_c_din0_z_pins[]		= { GPIOZ_8 };
static const unsigned int tsin_c_clk_z_pins[]		= { GPIOZ_9 };
static const unsigned int tsin_d_valid_z_pins[]		= { GPIOZ_10 };
static const unsigned int tsin_d_sop_z_pins[]		= { GPIOZ_11 };
static const unsigned int tsin_d_din0_z_pins[]		= { GPIOZ_12 };
static const unsigned int tsin_d_clk_z_pins[]		= { GPIOZ_13 };

/*bank Z func7 */
static const unsigned int pdm_din0_z_pins[]		= { GPIOZ_2 };
static const unsigned int pdm_din1_z_pins[]		= { GPIOZ_3 };
static const unsigned int pdm_din2_z_pins[]		= { GPIOZ_4 };
static const unsigned int pdm_din3_z_pins[]		= { GPIOZ_5 };
static const unsigned int pdm_dclk_z_pins[]		= { GPIOZ_6 };
static const unsigned int i2c0_sda_z7_pins[]		= { GPIOZ_7 };
static const unsigned int i2c0_scl_z8_pins[]		= { GPIOZ_8 };
static const unsigned int i2c2_sda_z10_pins[]		= { GPIOZ_10 };
static const unsigned int i2c2_scl_z11_pins[]		= { GPIOZ_11 };
static const unsigned int gen_clk_z_pins[]		= { GPIOZ_13 };

/*bank A func1 */
static const unsigned int remote_input_a_pins[]		= { GPIOA_15 };
/*bank A func3 */
static const unsigned int i2c3_sda_a_pins[]		= { GPIOA_14 };
static const unsigned int i2c3_scl_a_pins[]		= { GPIOA_15 };
/*bank A func5 */
static const unsigned int pdm_din0_a_pins[]		= { GPIOA_14 };
static const unsigned int pdm_dclk_a_pins[]		= { GPIOA_15 };
/*bank A func7 */
static const unsigned int gen_clk_a_pins[]		= { GPIOA_15 };

static struct meson_pmx_group meson_sc2_periphs_groups[] __initdata = {
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
	GPIO_GROUP(GPIOD_11),
	GPIO_GROUP(GPIOE_0),
	GPIO_GROUP(GPIOE_1),
	GPIO_GROUP(GPIOE_2),
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
	GPIO_GROUP(GPIOB_14),
	GPIO_GROUP(GPIOB_15),
	GPIO_GROUP(GPIOC_0),
	GPIO_GROUP(GPIOC_1),
	GPIO_GROUP(GPIOC_2),
	GPIO_GROUP(GPIOC_3),
	GPIO_GROUP(GPIOC_4),
	GPIO_GROUP(GPIOC_5),
	GPIO_GROUP(GPIOC_6),
	GPIO_GROUP(GPIOC_7),
	GPIO_GROUP(GPIOX_0),
	GPIO_GROUP(GPIOX_1),
	GPIO_GROUP(GPIOX_2),
	GPIO_GROUP(GPIOX_3),
	GPIO_GROUP(GPIOX_4),
	GPIO_GROUP(GPIOX_5),
	GPIO_GROUP(GPIOX_6),
	GPIO_GROUP(GPIOX_7),
	GPIO_GROUP(GPIOX_8),
	GPIO_GROUP(GPIOX_9),
	GPIO_GROUP(GPIOX_10),
	GPIO_GROUP(GPIOX_11),
	GPIO_GROUP(GPIOX_12),
	GPIO_GROUP(GPIOX_13),
	GPIO_GROUP(GPIOX_14),
	GPIO_GROUP(GPIOX_15),
	GPIO_GROUP(GPIOX_16),
	GPIO_GROUP(GPIOX_17),
	GPIO_GROUP(GPIOX_18),
	GPIO_GROUP(GPIOX_19),
	GPIO_GROUP(GPIOH_0),
	GPIO_GROUP(GPIOH_1),
	GPIO_GROUP(GPIOH_2),
	GPIO_GROUP(GPIOH_3),
	GPIO_GROUP(GPIOH_4),
	GPIO_GROUP(GPIOH_5),
	GPIO_GROUP(GPIOH_6),
	GPIO_GROUP(GPIOH_7),
	GPIO_GROUP(GPIOH_8),
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
	GPIO_GROUP(GPIOA_14),
	GPIO_GROUP(GPIOA_15),
	GPIO_GROUP(GPIO_TEST_N),

	/* bank D func1 */
	GROUP(uart_b_tx_d,		1),
	GROUP(uart_b_rx_d,		1),
	GROUP(i2c4_scl_d,		1),
	GROUP(i2c4_sda_d,		1),
	GROUP(remote_out_d4,		1),
	GROUP(remote_input_d5,		1),
	GROUP(jtag_2_clk,		1),
	GROUP(jtag_2_tms,		1),
	GROUP(jtag_2_tdi,		1),
	GROUP(jtag_2_tdo,		1),
	GROUP(cec_a_d,			1),

	/* bank D funsc2 */
	GROUP(uart_a_tx_d2,		2),
	GROUP(uart_a_rx_d3,		2),
	GROUP(clk_32k_in,		2),
	GROUP(remote_out_d9,		2),
	GROUP(cec_b_d,			2),
	GROUP(pwm_g_hiz,		2),

	/* bank D func3 */
	GROUP(i2c_slave_scl,		3),
	GROUP(i2c_slave_sda,		3),
	GROUP(pwm_i_d4,			3),
	GROUP(pwm_j_d5,			3),
	GROUP(pwm_i_d6,			3),
	GROUP(uart_a_tx_d8,		3),
	GROUP(uart_a_rx_d9,		3),
	GROUP(pwm_j_d10,		3),
	GROUP(pwm_g,			3),

	/* bank D func4 */
	GROUP(pwm_i_hiz,		4),
	GROUP(tsin_a_sop_d,		4),
	GROUP(tsin_a_din0_d,		4),
	GROUP(tsin_a_clk_d,		4),
	GROUP(tsin_a_valid_d,		4),
	GROUP(spdif_out_d,		4),
	GROUP(gen_clk_d,		4),

	/* bank D func5 */
	GROUP(mic_mute_en,		5),
	GROUP(tdm_d0,			5),
	GROUP(tdm_d1,			5),
	GROUP(tdm_fs1,			5),
	GROUP(tdm_sclk1,		5),
	GROUP(mclk1,			5),
	GROUP(tdm_d2_d,			5),

	/* bank D func6 */
	GROUP(clk12_24_d,		6),

	/* bank E func1 */
	GROUP(uart_b_cts,		1),
	GROUP(uart_b_rts,		1),
	GROUP(clk12_24_e,		1),

	/* bank E funsc2 */
	GROUP(uart_a_cts,		2),
	GROUP(uart_a_rts,		2),
	GROUP(clk25,			2),

	/* bank E func3 */
	GROUP(pwm_h,			3),
	GROUP(pwm_j_e,			3),
	GROUP(pwm_a_e,			3),

	/* bank E func4 */
	GROUP(i2c4_scl_e,		4),
	GROUP(i2c4_sda_e,		4),

	/*bank E func5 */
	GROUP(mic_mute_key,		5),

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
	GROUP(emmc_nand_dqs,		1),

	/* bank B funsc2 */
	GROUP(nand_wen_clk,		2),
	GROUP(nand_ale,			2),
	GROUP(nand_cle,			2),
	GROUP(nand_ce0,			2),
	GROUP(nand_ren_wr,		2),
	GROUP(nand_rb0,			2),
	GROUP(nand_ce1,			2),

	/* bank B func3 */
	GROUP(nor_hold,			3),
	GROUP(nor_d,			3),
	GROUP(nor_q,			3),
	GROUP(nor_c,			3),
	GROUP(nor_wp,			3),
	GROUP(nor_cs,			3),

	/* bank C func1 */
	GROUP(sdcard_d0_c,		1),
	GROUP(sdcard_d1_c,		1),
	GROUP(sdcard_d2_c,		1),
	GROUP(sdcard_d3_c,		1),
	GROUP(sdcard_clk_c,		1),
	GROUP(sdcard_cmd_c,		1),
	GROUP(sdcard_cd,		1),
	GROUP(pcieck_reqn,		1),

	/* bank C funsc2 */
	GROUP(jtag_1_tdo,		2),
	GROUP(jtag_1_tdi,		2),
	GROUP(uart_b_rx_c,		2),
	GROUP(uart_b_tx_c,		2),
	GROUP(jtag_1_clk,		2),
	GROUP(jtag_1_tms,		2),

	/* bank C func3 */
	GROUP(tsin_c_valid_c,		3),
	GROUP(tsin_c_sop_c,		3),
	GROUP(tsin_c_din0_c,		3),
	GROUP(tsin_c_clk_c,		3),
	GROUP(i2c0_sda_c,		3),
	GROUP(i2c0_scl_c,		3),

	/* bank C func4 */
	GROUP(pdm_din0_c,		4),
	GROUP(pdm_din1_c1,		4),
	GROUP(pdm_din2_c,		4),
	GROUP(pdm_din3_c,		4),
	GROUP(pdm_dclk_c,		4),
	GROUP(pdm_din1_c7,		4),

	/* bank C func5 */
	GROUP(spi_a_mosi_c,		5),
	GROUP(spi_a_miso_c,		5),
	GROUP(spi_a_ss0_c,		5),
	GROUP(spi_a_sclk_c,		5),
	GROUP(pwm_c_c,			5),
	GROUP(iso7816_clk_c,		5),
	GROUP(iso7816_data_c,		5),

	/* bank X func1 */
	GROUP(sdio_d0_x,		1),
	GROUP(sdio_d1_x,		1),
	GROUP(sdio_d2_x,		1),
	GROUP(sdio_d3_x,		1),
	GROUP(sdio_clk_x,		1),
	GROUP(sdio_cmd_x,		1),
	GROUP(pwm_a_x,			1),
	GROUP(pwm_f_x,			1),
	GROUP(tdm_d3,			1),
	GROUP(tdm_d4,			1),
	GROUP(tdm_fs0,			1),
	GROUP(tdm_sclk0,		1),
	GROUP(uart_e_tx,		1),
	GROUP(uart_e_rx,		1),
	GROUP(uart_e_cts,		1),
	GROUP(uart_e_rts,		1),
	GROUP(pwm_e,			1),
	GROUP(i2c2_sda_x,		1),
	GROUP(i2c2_scl_x,		1),
	GROUP(pwm_b_x19,		1),

	/* bank X funsc2 */
	GROUP(pdm_din0_x,		2),
	GROUP(pdm_din1_x,		2),
	GROUP(pdm_din2_x,		2),
	GROUP(pdm_din3_x,		2),
	GROUP(pdm_dclk_x,		2),
	GROUP(mclk_0,			2),
	GROUP(uart_d_tx_x6,		2),
	GROUP(uart_d_rx_x7,		2),
	GROUP(uart_d_rts,		2),
	GROUP(uart_d_cts,		2),
	GROUP(uart_d_tx_x10,		2),
	GROUP(uart_d_rx_x11,		2),
	GROUP(tdm_d15,			2),

	/* bank X func3 */
	GROUP(tsin_a_din0_x,		3),
	GROUP(tsin_a_sop_x,		3),
	GROUP(tsin_a_valid_x,		3),
	GROUP(tsin_a_clk_x,		3),
	GROUP(tsin_d_sop_x,		3),
	GROUP(tsin_d_valid_x,		3),
	GROUP(tsin_d_din0_x,		3),
	GROUP(tsin_d_clk_x,		3),

	/* bank X func4 */
	GROUP(pwm_d_x3,			4),
	GROUP(pwm_c_x,			4),
	GROUP(pwm_d_x6,			4),
	GROUP(pwm_b_x7,			4),
	GROUP(spi_a_mosi_x,		4),
	GROUP(spi_a_miso_x,		4),
	GROUP(spi_a_ss0_x,		4),
	GROUP(spi_a_sclk_x,		4),

	/* bank X func5 */
	GROUP(sdcard_d0_x,		5),
	GROUP(sdcard_d1_x,		5),
	GROUP(sdcard_d2_x,		5),
	GROUP(sdcard_d3_x,		5),
	GROUP(sdcard_clk_x,		5),
	GROUP(sdcard_cd_x,		5),
	GROUP(iso7816_clk_x,		5),
	GROUP(iso7816_data_x,		5),
	GROUP(i2c1_sda_x,		5),
	GROUP(i2c1_scl_x,		5),
	GROUP(gen_clk_x,		5),

	/* bank H func1 */
	GROUP(hdmitx_sda,		1),
	GROUP(hdmitx_sck,		1),
	GROUP(hdmitx_hpd_in,		1),
	GROUP(spdif_out_h,		1),
	GROUP(spdif_in_h,		1),
	GROUP(iso7816_data_h,		1),
	GROUP(iso7816_clk_h,		1),

	/* bank H funsc2 */
	GROUP(i2c3_sda_h,		2),
	GROUP(i2c3_scl_h,		2),
	GROUP(i2c1_sda_h2,		2),
	GROUP(i2c1_scl_h3,		2),
	GROUP(uart_c_rts,		2),
	GROUP(uart_c_cts,		2),
	GROUP(uart_c_rx,		2),
	GROUP(uart_c_tx,		2),

	/* bank H func3 */
	GROUP(spi_b_mosi_h,		3),
	GROUP(spi_b_miso_h,		3),
	GROUP(spi_b_ss0_h,		3),
	GROUP(spi_b_sclk_h,		3),

	/* bank H func4 */
	GROUP(cec_a_h,			4),
	GROUP(pwm_f_h,			4),
	GROUP(i2c1_sda_h6,		4),
	GROUP(i2c1_scl_h7,		4),

	/* bank H func5 */
	GROUP(cec_b_h,			5),
	GROUP(tdm_d5,			5),
	GROUP(remote_out_h,		5),
	GROUP(pwm_b_h,			5),

	/* bank H func6 */
	GROUP(i2c0_sda_h,		6),
	GROUP(i2c0_scl_h,		6),

	/* bank Z func1 */
	GROUP(eth_mdio,			1),
	GROUP(eth_mdc,			1),
	GROUP(eth_rgmii_rx_clk,		1),
	GROUP(eth_rx_dv,		1),
	GROUP(eth_rxd0,			1),
	GROUP(eth_rxd1,			1),
	GROUP(eth_rxd2_rgmii,		1),
	GROUP(eth_rxd3_rgmii,		1),
	GROUP(eth_rgmii_tx_clk,		1),
	GROUP(eth_txen,			1),
	GROUP(eth_txd0,			1),
	GROUP(eth_txd1,			1),
	GROUP(eth_txd2_rgmii,		1),
	GROUP(eth_txd3_rgmii,		1),
	GROUP(eth_link_led,		1),
	GROUP(eth_act_led,		1),

	/* bank Z funsc2 */
	GROUP(pwm_d_z,			2),
	GROUP(clk12_24_z,		2),

	/* bank Z func3 */
	GROUP(iso7816_clk_z0,		3),
	GROUP(iso7816_data_z1,		3),
	GROUP(tsin_b_valid,		3),
	GROUP(tsin_b_sop,		3),
	GROUP(tsin_b_din0,		3),
	GROUP(tsin_b_clk,		3),
	GROUP(tsin_b_fail,		3),
	GROUP(tsin_b_din1,		3),
	GROUP(tsin_b_din2,		3),
	GROUP(tsin_b_din3,		3),
	GROUP(tsin_b_din4,		3),
	GROUP(tsin_b_din5,		3),
	GROUP(tsin_b_din6,		3),
	GROUP(tsin_b_din7,		3),
	GROUP(i2c2_sda_z14,		3),
	GROUP(i2c2_scl_z15,		3),

	/* bank Z func4 */
	GROUP(i2c0_sda_z0,		4),
	GROUP(i2c0_scl_z1,		4),
	GROUP(tdm_d6,			4),
	GROUP(tdm_d7,			4),
	GROUP(tdm_d8,			4),
	GROUP(tdm_d9,			4),
	GROUP(tdm_fs2,			4),
	GROUP(tdm_sclk2,		4),
	GROUP(mclk_2,			4),
	GROUP(tdm_d10,			4),
	GROUP(tdm_d11,			4),
	GROUP(tdm_d12,			4),
	GROUP(tdm_d13,			4),
	GROUP(tdm_d14,			4),

	/* bank Z func5 */
	GROUP(pwm_b_z0,			5),
	GROUP(pwm_c_z,			5),
	GROUP(sdcard_d0_z,		5),
	GROUP(sdcard_d1_z,		5),
	GROUP(sdcard_d2_z,		5),
	GROUP(sdcard_d3_z,		5),
	GROUP(sdcard_clk_z,		5),
	GROUP(sdcard_cmd_z,		5),
	GROUP(iso7816_clk_z8,		5),
	GROUP(iso7816_data_z9,		5),
	GROUP(remote_out_z,		5),
	GROUP(pwm_f_z,			5),
	GROUP(pwm_b_z13,		5),

	/* bank Z func6 */
	GROUP(i2c1_sda_z,		6),
	GROUP(i2c1_scl_z,		6),
	GROUP(tdm_fs3,			6),
	GROUP(tdm_sclk3,		6),
	GROUP(tdm_fs4,			6),
	GROUP(tdm_sclk4,		6),
	GROUP(tsin_c_valid_z,		6),
	GROUP(tsin_c_sop_z,		6),
	GROUP(tsin_c_din0_z,		6),
	GROUP(tsin_c_clk_z,		6),
	GROUP(tsin_d_valid_z,		6),
	GROUP(tsin_d_sop_z,		6),
	GROUP(tsin_d_din0_z,		6),
	GROUP(tsin_d_clk_z,		6),

	/* bank Z func7 */
	GROUP(pdm_din0_z,		7),
	GROUP(pdm_din1_z,		7),
	GROUP(pdm_din2_z,		7),
	GROUP(pdm_din3_z,		7),
	GROUP(pdm_dclk_z,		7),
	GROUP(i2c0_sda_z7,		7),
	GROUP(i2c0_scl_z8,		7),
	GROUP(i2c2_sda_z10,		7),
	GROUP(i2c2_scl_z11,		7),
	GROUP(gen_clk_z,		7),

	/* bank A func1 */
	GROUP(remote_input_a,		1),
	/* bank A func3 */
	GROUP(i2c3_sda_a,		3),
	GROUP(i2c3_scl_a,		3),
	/* bank A func5 */
	GROUP(pdm_din0_a,		5),
	GROUP(pdm_dclk_a,		5),
	/* bank A func7 */
	GROUP(gen_clk_a,		7),

};

static const char * const gpio_periphs_groups[] = {
	"GPIO_TEST_N",

	"GPIOD_0", "GPIOD_1", "GPIOD_2", "GPIOD_3", "GPIOD_4",
	"GPIOD_5", "GPIOD_6", "GPIOD_7", "GPIOD_8", "GPIOD_9",
	"GPIOD_10", "GPIOD_11", "GPIOD_12",

	"GPIOE_0", "GPIOE_1", "GPIOE_2",

	"GPIOB_0", "GPIOB_1", "GPIOB_2", "GPIOB_3", "GPIOB_4",
	"GPIOB_5", "GPIOB_6", "GPIOB_7", "GPIOB_8", "GPIOB_9",
	"GPIOB_10", "GPIOB_11", "GPIOB_12", "GPIOB_13",
	"GPIOB_14", "GPIOB_15",

	"GPIOC_0", "GPIOC_1", "GPIOC_2", "GPIOC_3", "GPIOC_4",
	"GPIOC_5", "GPIOC_6", "GPIOC_7",

	"GPIOX_0", "GPIOX_1", "GPIOX_2", "GPIOX_3", "GPIOX_4",
	"GPIOX_5", "GPIOX_6", "GPIOX_7", "GPIOX_8", "GPIOX_9",
	"GPIOX_10", "GPIOX_11", "GPIOX_12", "GPIOX_13", "GPIOX_14",
	"GPIOX_15", "GPIOX_16", "GPIOX_17", "GPIOX_18", "GPIOX_19",

	"GPIOH_0", "GPIOH_1", "GPIOH_2", "GPIOH_3", "GPIOH_4",
	"GPIOH_5", "GPIOH_6", "GPIOH_7", "GPIOH_8",

	"GPIOZ_0", "GPIOZ_1", "GPIOZ_2", "GPIOZ_3", "GPIOZ_4",
	"GPIOZ_5", "GPIOZ_6", "GPIOZ_7", "GPIOZ_8", "GPIOZ_9",
	"GPIOZ_10", "GPIOZ_11", "GPIOZ_12", "GPIOZ_13",
	"GPIOZ_14", "GPIOZ_15",

	"GPIOA_14", "GPIOA_15",
};

static const char * const uart_a_groups[] = {
	"uart_a_cts", "uart_a_rts", "uart_a_tx_d2", "uart_a_rx_d3",
	"uart_a_tx_d8", "uart_a_rx_d9",
};

static const char * const uart_b_groups[] = {
	"uart_b_tx_d", "uart_b_rx_d", "uart_b_cts", "uart_b_rts",
	"uart_b_rx_c", "uart_b_tx_c",
};

static const char * const uart_c_groups[] = {
	"uart_c_rts", "uart_c_cts", "uart_c_tx", "uart_c_rx",
};

static const char * const uart_d_groups[] = {
	"uart_d_rts", "uart_d_cts", "uart_d_tx_x6", "uart_d_rx_x7",
	"uart_d_tx_x10", "uart_d_rx_x11",
};

static const char * const uart_e_groups[] = {
	"uart_e_tx", "uart_e_rx", "uart_e_cts", "uart_e_rts",
};

static const char * const i2c0_groups[] = {
	"i2c0_scl_c", "i2c0_sda_c", "i2c0_scl_h", "i2c0_sda_h",
	"i2c0_scl_z1", "i2c0_sda_z0", "i2c0_scl_z8", "i2c0_sda_z7",
};

static const char * const i2c1_groups[] = {
	"i2c1_sda_z", "i2c1_scl_z", "i2c1_sda_x", "i2c1_scl_x",
	"i2c1_sda_h2", "i2c1_scl_h3", "i2c1_sda_h6", "i2c1_scl_h7",
};

static const char * const i2c2_groups[] = {
	"i2c2_sda_x", "i2c2_scl_x", "i2c2_sda_z10", "i2c2_scl_z11",
	"i2c2_sda_z14", "i2c2_scl_z15",
};

static const char * const i2c3_groups[] = {
	"i2c3_sda_h", "i2c3_scl_h", "i2c3_sda_a", "i2c3_scl_a",
};

static const char * const i2c4_groups[] = {
	"i2c4_sda_d", "i2c4_scl_d", "i2c4_sda_e", "i2c4_scl_e",
};

static const char * const i2c_slave_groups[] = {
	"i2c_slave_scl", "i2c_slave_sda",
};

static const char * const pwm_a_groups[] = {
	"pwm_a_e", "pwm_a_x",
};

static const char * const pwm_b_groups[] = {
	"pwm_b_h", "pwm_b_z0", "pwm_b_z13", "pwm_b_x7",
	"pwm_b_x19",
};

static const char * const pwm_c_groups[] = {
	"pwm_c_c", "pwm_c_x", "pwm_c_z",
};

static const char * const pwm_d_groups[] = {
	"pwm_d_z", "pwm_d_x3", "pwm_d_x6",
};

static const char * const pwm_e_groups[] = {
	"pwm_e",
};

static const char * const pwm_f_groups[] = {
	"pwm_f_x", "pwm_f_h", "pwm_f_z"
};

static const char * const pwm_g_groups[] = {
	"pwm_g",
};

static const char * const pwm_h_groups[] = {
	"pwm_h",
};

static const char * const pwm_i_groups[] = {
	"pwm_i_d4", "pwm_i_d6",
};

static const char * const pwm_j_groups[] = {
	"pwm_j_e", "pwm_j_d5", "pwm_j_d10",
};

static const char * const pwm_i_hiz_groups[] = {
	"pwm_i_hiz",
};

static const char * const pwm_g_hiz_groups[] = {
	"pwm_g_hiz",
};

static const char * const remote_out_groups[] = {
	"remote_out_h", "remote_out_z", "remote_out_d4", "remote_out_d9",
};

static const char * const remote_input_groups[] = {
	"remote_input_a", "remote_input_d5",
};

static const char * const jtag_1_groups[] = {
	"jtag_1_clk", "jtag_1_tms", "jtag_1_tdi", "jtag_1_tdo",
};

static const char * const jtag_2_groups[] = {
	"jtag_2_tdo", "jtag_2_tdi", "jtag_2_clk", "jtag_2_tms",
};

static const char * const gen_clk_groups[] = {
	"gen_clk_d", "gen_clk_z", "gen_clk_a", "gen_clk_x",
};

static const char * const clk12_24_groups[] = {
	"clk12_24_d", "clk12_24_e", "clk12_24_z",
};

static const char * const clk25_groups[] = {
	"clk25",
};

static const char * const clk_32k_in_groups[] = {
	"clk_32k_in",
};

static const char * const emmc_groups[] = {
	"emmc_nand_d0", "emmc_nand_d1", "emmc_nand_d2", "emmc_nand_d3",
	"emmc_nand_d4", "emmc_nand_d5", "emmc_nand_d6", "emmc_nand_d7",
	"emmc_clk", "emmc_cmd", "emmc_nand_dqs",
};

static const char * const nand_groups[] = {
	"emmc_nand_d0", "emmc_nand_d1", "emmc_nand_d2", "emmc_nand_d3",
	"emmc_nand_d4", "emmc_nand_d5", "emmc_nand_d6", "emmc_nand_d7",
	"emmc_nand_dqs", "nand_wen_clk", "nand_ale", "nand_ren_wr",
	"nand_cle", "nand_ce0", "nand_ce1", "nand_rb0",
};

static const char * const sdcard_groups[] = {
	"sdcard_d0_c", "sdcard_d1_c", "sdcard_d2_c", "sdcard_d3_c",
	"sdcard_cd_c", "sdcard_clk_c", "sdcard_cmd_c",

	"sdcard_d0_x", "sdcard_d1_x", "sdcard_d2_x", "sdcard_d3_x",
	"sdcard_clk_x", "sdcard_cmd_x",

	"sdcard_d0_z", "sdcard_d1_z", "sdcard_d2_z", "sdcard_d3_z",
	"sdcard_clk_z", "sdcard_cmd_z",
};

static const char * const nor_groups[] = {
	"nor_hold", "nor_d", "nor_q", "nor_c",
	"nor_wp", "nor_cs",
};

static const char * const spi_a_groups[] = {
	"spi_a_mosi_c", "spi_a_miso_c", "spi_a_ss0_c", "spi_a_sclk_c",
	"spi_a_mosi_x", "spi_a_miso_x", "spi_a_sclk_x", "spi_a_ss0_x",
};

static const char * const spi_b_groups[] = {
	"spi_b_mosi_h", "spi_b_miso_h", "spi_b_ss0_h", "spi_b_sclk_h",
};

static const char * const pdm_groups[] = {
	"pdm_din0_c", "pdm_din1_c1", "pdm_din2_c", "pdm_din3_c",
	"pdm_din1_c7", "pdm_dclk_c",

	"pdm_din0_x", "pdm_din1_x", "pdm_din2_x", "pdm_din3_x",
	"pdm_dclk_x",

	"pdm_din0_z", "pdm_din1_z", "pdm_din2_z", "pdm_din3_z",
	"pdm_dclk_z",

	"pdm_din0_a", "pdm_dclk_a",

};

static const char * const sdio_groups[] = {
	"sdio_d0_x", "sdio_d1_x", "sdio_d2_x", "sdio_d3_x",
	"sdio_clk_x", "sdio_cmd_x",
};

static const char * const eth_groups[] = {
	"eth_mdc", "eth_mdio", "eth_rxd0", "eth_rxd1",
	"eth_txen", "eth_txd0", "eth_txd1", "eth_rx_dv",
	"eth_act_led", "eth_link_led", "eth_rxd2_rgmii", "eth_rxd3_rgmii",
	"eth_txd2_rgmii", "eth_txd3_rgmii", "eth_rgmii_rx_clk",
	"eth_rgmii_tx_clk",
};

static const char * const mic_mute_groups[] = {
	"mic_mute_en", "mic_mute_key",
};

static const char * const mclk_groups[] = {
	"mclk1", "mclk_0", "mclk_2",
};

static const char * const tdm_groups[] = {
	"tdm_d0", "tdm_d1", "tdm_d2", "tdm_d3",
	"tdm_d4", "tdm_d5", "tdm_d6", "tdm_d7",
	"tdm_d8", "tdm_d9", "tdm_d10", "tdm_d11",
	"tdm_d12", "tdm_d13", "tdm_d14", "tdm_d15",
	"tdm_fs0", "tdm_fs1", "tdm_fs2", "tdm_fs3",
	"tdm_fs4", "tdm_sclk0", "tdm_sclk1", "tdm_sclk2",
	"tdm_sclk3", "tdm_sclk4",
};

static const char * const tsin_a_groups[] = {
	"tsin_a_sop_d", "tsin_a_clk_d", "tsin_a_clk_x", "tsin_a_sop_x",
	"tsin_a_din0_d", "tsin_a_din0_x", "tsin_a_valid_d", "tsin_a_valid_x",
};

static const char * const tsin_b_groups[] = {
	"tsin_b_sop", "tsin_b_clk", "tsin_b_din0", "tsin_b_fail",
	"tsin_b_din1", "tsin_b_din2", "tsin_b_din3", "tsin_b_din4",
	"tsin_b_din5", "tsin_b_din6", "tsin_b_din7", "tsin_b_valid",
};

static const char * const tsin_c_groups[] = {
	"tsin_c_sop_c", "tsin_c_clk_c", "tsin_c_sop_z", "tsin_c_clk_z",
	"tsin_c_din0_c", "tsin_c_din0_z", "tsin_c_valid_c", "tsin_c_valid_z",
};

static const char * const tsin_d_groups[] = {
	"tsin_d_sop_x", "tsin_d_clk_x", "tsin_d_sop_z", "tsin_d_clk_z",
	"tsin_d_din0_x", "tsin_d_din0_z", "tsin_d_valid_x", "tsin_d_valid_z",
};

static const char * const cec_a_groups[] = {
	"cec_a_h", "cec_a_d",
};

static const char * const cec_b_groups[] = {
	"cec_b_h", "cec_b_d",
};

static const char * const spdif_in_groups[] = {
	"spdif_in_h",
};

static const char * const spdif_out_groups[] = {
	"spdif_out_d", "spdif_out_h",
};

static const char * const pcieck_reqn_groups[] = {
	"pcieck_reqn",
};

static const char * const iso7816_groups[] = {
	"iso7816_clk_c", "iso7816_clk_h", "iso7816_data_c", "iso7816_clk_x",
	"iso7816_data_x", "iso7816_data_h", "iso7816_clk_z0",
	"iso7816_clk_z8", "iso7816_data_z1", "iso7816_data_z9",
};

static const char * const hdmitx_groups[] = {
	"hdmitx_sda", "hdmitx_sck", "hdmitx_hpd_in",
};

static struct meson_pmx_func meson_sc2_periphs_functions[] __initdata = {
	FUNCTION(gpio_periphs),
	FUNCTION(uart_a),
	FUNCTION(uart_b),
	FUNCTION(uart_c),
	FUNCTION(uart_d),
	FUNCTION(uart_e),
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
	FUNCTION(pwm_i_hiz),
	FUNCTION(pwm_g_hiz),
	FUNCTION(remote_out),
	FUNCTION(remote_input),
	FUNCTION(jtag_1),
	FUNCTION(jtag_2),
	FUNCTION(gen_clk),
	FUNCTION(clk12_24),
	FUNCTION(clk25),
	FUNCTION(clk_32k_in),
	FUNCTION(emmc),
	FUNCTION(nand),
	FUNCTION(nor),
	FUNCTION(sdcard),
	FUNCTION(spi_a),
	FUNCTION(spi_b),
	FUNCTION(pdm),
	FUNCTION(sdio),
	FUNCTION(eth),
	FUNCTION(mic_mute),
	FUNCTION(mclk),
	FUNCTION(tdm),
	FUNCTION(tsin_a),
	FUNCTION(tsin_b),
	FUNCTION(tsin_c),
	FUNCTION(tsin_d),
	FUNCTION(cec_a),
	FUNCTION(cec_b),
	FUNCTION(spdif_in),
	FUNCTION(spdif_out),
	FUNCTION(pcieck_reqn),
	FUNCTION(iso7816),
	FUNCTION(hdmitx),
};

static struct meson_bank meson_sc2_periphs_banks[] = {
	/* name  first  last  irq  pullen  pull  dir  out  in */
	BANK_DS("Z", GPIOZ_0,    GPIOZ_15,  70, 85,
		0x3,   0,  0x4,  0,   0x2,  0,  0x1,  0,  0x0,  0, 0x07, 0),
	BANK_DS("X", GPIOX_0,    GPIOX_19,  50, 69,
		0x13,  0,  0x14,  0,  0x12, 0,  0x11, 0,  0x10, 0, 0x17, 0),
	BANK_DS("H", GPIOH_0,    GPIOH_8,   41, 49,
		0x23,  0,  0x24,  0,  0x22, 0,  0x21, 0,  0x20, 0, 0x27, 0),
	BANK_DS("D", GPIOD_0,    GPIOD_11,  29, 49,
		0x33,  0,  0x34,  0,  0x32, 0,  0x31, 0,  0x30, 0, 0x37, 0),
	BANK_DS("E", GPIOE_0,    GPIOE_2,   26, 28,
		0x43,  0,  0x44,  0,  0x42, 0,  0x41, 0,  0x40, 0, 0x47, 0),
	BANK_DS("C", GPIOC_0,    GPIOC_7,   18, 25,
		0x53,  0,  0x54,  0,  0x52, 0,  0x51, 0,  0x50, 0, 0x57, 0),
	BANK_DS("B", GPIOB_0,    GPIOB_15,   2, 17,
		0x63,  0,  0x64,  0,  0x62, 0,  0x61, 0,  0x60, 0, 0x67, 0),
	BANK_DS("A", GPIOA_14,   GPIOA_15,   0, 1,
		0x73, 14,  0x74,  14, 0x72, 14, 0x71, 14, 0x70, 14, 0x77, 28),
	BANK_DS("TEST_N", GPIO_TEST_N,    GPIO_TEST_N,   86, 86,
		0x83,  0,  0x84,  0,  0x82, 0,  0x81,  0, 0x80, 0, 0x87, 0),
};

static struct meson_pmx_bank meson_sc2_periphs_pmx_banks[] = {
	/*name	            first	 lask        reg offset*/
	BANK_PMX("Z",      GPIOZ_0,     GPIOZ_15,    0x6, 0),
	BANK_PMX("X",      GPIOX_0,     GPIOX_19,    0x3, 0),
	BANK_PMX("H",      GPIOH_0,     GPIOH_8,     0xb, 0),
	BANK_PMX("D",      GPIOD_0,     GPIOD_11,    0x10, 0),
	BANK_PMX("E",      GPIOE_0,     GPIOE_2,     0x12, 0),
	BANK_PMX("C",      GPIOC_0,     GPIOC_7,     0x9, 0),
	BANK_PMX("B",      GPIOB_0,     GPIOB_15,    0x0, 0),
	BANK_PMX("A",      GPIOA_14,    GPIOA_15,    0xe, 24),
	BANK_PMX("TEST_N", GPIO_TEST_N, GPIO_TEST_N, 0xf, 0),
};

static struct meson_axg_pmx_data meson_sc2_periphs_pmx_banks_data = {
	.pmx_banks	= meson_sc2_periphs_pmx_banks,
	.num_pmx_banks	= ARRAY_SIZE(meson_sc2_periphs_pmx_banks),
};

static struct meson_pinctrl_data meson_sc2_periphs_pinctrl_data __refdata = {
	.name		= "periphs-banks",
	.pins		= meson_sc2_periphs_pins,
	.groups		= meson_sc2_periphs_groups,
	.funcs		= meson_sc2_periphs_functions,
	.banks		= meson_sc2_periphs_banks,
	.num_pins	= ARRAY_SIZE(meson_sc2_periphs_pins),
	.num_groups	= ARRAY_SIZE(meson_sc2_periphs_groups),
	.num_funcs	= ARRAY_SIZE(meson_sc2_periphs_functions),
	.num_banks	= ARRAY_SIZE(meson_sc2_periphs_banks),
	.pmx_ops	= &meson_axg_pmx_ops,
	.pmx_data	= &meson_sc2_periphs_pmx_banks_data,
	.parse_dt	= &meson_a1_parse_dt_extra,
};

static const struct of_device_id meson_sc2_pinctrl_dt_match[] = {
	{
		.compatible = "amlogic,meson-sc2-periphs-pinctrl",
		.data = &meson_sc2_periphs_pinctrl_data,
	},
	{ },
};

static struct platform_driver meson_sc2_pinctrl_driver = {
	.probe  = meson_pinctrl_probe,
	.driver = {
		.name	= "meson-sc2-pinctrl",
	},
};

#ifndef MODULE
static int __init sc2_pmx_init(void)
{
	meson_sc2_pinctrl_driver.driver.of_match_table =
			meson_sc2_pinctrl_dt_match;
	return platform_driver_register(&meson_sc2_pinctrl_driver);
}
arch_initcall(sc2_pmx_init);

#else
int __init meson_sc2_pinctrl_init(void)
{
	meson_sc2_pinctrl_driver.driver.of_match_table =
			meson_sc2_pinctrl_dt_match;
	return platform_driver_register(&meson_sc2_pinctrl_driver);
}
#endif
