// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <dt-bindings/gpio/meson-s5-gpio.h>
#include "pinctrl-meson.h"
#include "pinctrl-meson-axg-pmx.h"
#include "pinctrl-module-init.h"

static const struct pinctrl_pin_desc meson_s5_storage_pins[] = {
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
};

static const struct pinctrl_pin_desc meson_s5_periphs_pins[] = {
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
	MESON_PIN(GPIOE_3),
	MESON_PIN(GPIOE_4),

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

	MESON_PIN(GPIOT_0),
	MESON_PIN(GPIOT_1),
	MESON_PIN(GPIOT_2),
	MESON_PIN(GPIOT_3),
	MESON_PIN(GPIOT_4),
	MESON_PIN(GPIOT_5),
	MESON_PIN(GPIOT_6),
	MESON_PIN(GPIOT_7),
	MESON_PIN(GPIOT_8),
	MESON_PIN(GPIOT_9),
	MESON_PIN(GPIOT_10),
	MESON_PIN(GPIOT_11),
	MESON_PIN(GPIOT_12),
	MESON_PIN(GPIOT_13),
	MESON_PIN(GPIOT_14),
	MESON_PIN(GPIOT_15),
	MESON_PIN(GPIOT_16),
	MESON_PIN(GPIOT_17),
	MESON_PIN(GPIOT_18),
	MESON_PIN(GPIOT_19),
	MESON_PIN(GPIOT_20),
	MESON_PIN(GPIOT_21),
	MESON_PIN(GPIOT_22),
	MESON_PIN(GPIOT_23),
	MESON_PIN(GPIOT_24),

	MESON_PIN(GPIOA_0),
	MESON_PIN(GPIOA_1),
	MESON_PIN(GPIOA_2),
	MESON_PIN(GPIOA_3),
	MESON_PIN(GPIOA_4),
	MESON_PIN(GPIOA_5),
	MESON_PIN(GPIOA_6),
	MESON_PIN(GPIOA_7),
	MESON_PIN(GPIOA_8),
	MESON_PIN(GPIOA_9),
	MESON_PIN(GPIOA_10),

	MESON_PIN(GPIO_TEST_N)
};

/* BANK D func1 */
static const unsigned int uart_a_tx_d_pins[]		= { GPIOD_0 };
static const unsigned int uart_a_rx_d_pins[]		= { GPIOD_1 };
static const unsigned int i2c0_scl_d_pins[]		= { GPIOD_2 };
static const unsigned int i2c0_sda_d_pins[]		= { GPIOD_3 };
static const unsigned int remote_out_d4_pins[]		= { GPIOD_4 };
static const unsigned int remote_in_d_pins[]		= { GPIOD_5 };
static const unsigned int jtag_a_clk_pins[]		= { GPIOD_6 };
static const unsigned int jtag_a_tms_pins[]		= { GPIOD_7 };
static const unsigned int jtag_a_tdi_pins[]		= { GPIOD_8 };
static const unsigned int jtag_a_tdo_pins[]		= { GPIOD_9 };
static const unsigned int pwm_g_hiz_pins[]		= { GPIOD_11 };

/* BANK D func2 */
static const unsigned int uart_c_tx_d_pins[]		= { GPIOD_2 };
static const unsigned int uart_c_rx_d_pins[]		= { GPIOD_3 };
static const unsigned int clk_32k_in_pins[]		= { GPIOD_4 };
static const unsigned int pwm_h_d_pins[]		= { GPIOD_5 };
static const unsigned int pwm_i_d_pins[]		= { GPIOD_6 };
static const unsigned int pwm_j_d_pins[]		= { GPIOD_7 };
static const unsigned int remote_out_d9_pins[]		= { GPIOD_9 };
static const unsigned int cec_b_d_pins[]		= { GPIOD_10 };
static const unsigned int pwm_g_d_pins[]		= { GPIOD_11 };

/* BANK D func3 */
static const unsigned int i2c_slave_scl_pins[]		= { GPIOD_2 };
static const unsigned int i2c_slave_sda_pins[]		= { GPIOD_3 };
static const unsigned int uart_d_cts_pins[]		= { GPIOD_6 };
static const unsigned int uart_d_rts_pins[]		= { GPIOD_7 };
static const unsigned int uart_d_tx_pins[]		= { GPIOD_8 };
static const unsigned int uart_d_rx_pins[]		= { GPIOD_9 };
static const unsigned int pwm_d_d_pins[]		= { GPIOD_10 };
static const unsigned int gen_clk_out_d_pins[]		= { GPIOD_11 };

/* BANK D func4 */
static const unsigned int spdif_out_d_pins[]		= { GPIOD_10 };
static const unsigned int clk12m_24m_pins[]		= { GPIOD_11 };

/* BANK D func5 */
static const unsigned int clk25m_pins[]			= { GPIOD_11 };

/* BANK E func1 */
static const unsigned int pwm_b_e_pins[]		= { GPIOE_0 };
static const unsigned int pwm_d_e_pins[]		= { GPIOE_1 };
static const unsigned int pwm_a_e_pins[]		= { GPIOE_2 };
static const unsigned int pwm_c_e_pins[]		= { GPIOE_3 };
static const unsigned int pwm_e_pins[]			= { GPIOE_4 };

/* BANK E func2 */
static const unsigned int i2c0_scl_e_pins[]		= { GPIOE_0 };
static const unsigned int i2c0_sda_e_pins[]		= { GPIOE_1 };
static const unsigned int i2c4_scl_e_pins[]		= { GPIOE_2 };
static const unsigned int i2c4_sda_e_pins[]		= { GPIOE_3 };

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
static const unsigned int emmc_rst_pins[]		= { GPIOB_9 };
static const unsigned int emmc_cmd_pins[]		= { GPIOB_10 };
static const unsigned int emmc_nand_ds_pins[]		= { GPIOB_11 };

/* Bank B func2 */
static const unsigned int spif_hold_pins[]		= { GPIOB_3 };
static const unsigned int spif_mo_pins[]		= { GPIOB_4 };
static const unsigned int spif_mi_pins[]		= { GPIOB_5 };
static const unsigned int spif_clk_pins[]		= { GPIOB_6 };
static const unsigned int spif_wp_pins[]		= { GPIOB_7 };
static const unsigned int spif_cs_pins[]		= { GPIOB_12 };

/* Bank C func1 */
static const unsigned int sdcard_d0_pins[]		= { GPIOC_0 };
static const unsigned int sdcard_d1_pins[]		= { GPIOC_1 };
static const unsigned int sdcard_d2_pins[]		= { GPIOC_2 };
static const unsigned int sdcard_d3_pins[]		= { GPIOC_3 };
static const unsigned int sdcard_clk_pins[]		= { GPIOC_4 };
static const unsigned int sdcard_cmd_pins[]		= { GPIOC_5 };
static const unsigned int sdcard_cd_pins[]		= { GPIOC_6 };

/* Bank C func2 */
static const unsigned int jtag_b_tdo_pins[]		= { GPIOC_0 };
static const unsigned int jtag_b_tdi_pins[]		= { GPIOC_1 };
static const unsigned int uart_a_rx_c_pins[]		= { GPIOC_2 };
static const unsigned int uart_a_tx_c_pins[]		= { GPIOC_3 };
static const unsigned int jtag_b_clk_pins[]		= { GPIOC_4 };
static const unsigned int jtag_b_tms_pins[]		= { GPIOC_5 };

/* Bank C func3 */
static const unsigned int spi_a_mosi_c_pins[]		= { GPIOC_0 };
static const unsigned int spi_a_miso_c_pins[]		= { GPIOC_1 };
static const unsigned int spi_a_ss0_c_pins[]		= { GPIOC_2 };
static const unsigned int spi_a_clk_c_pins[]		= { GPIOC_3 };
static const unsigned int pwm_c_c_pins[]		= { GPIOC_4 };

/* Bank X func1 */
static const unsigned int sdio_d0_pins[]		= { GPIOX_0 };
static const unsigned int sdio_d1_pins[]		= { GPIOX_1 };
static const unsigned int sdio_d2_pins[]		= { GPIOX_2 };
static const unsigned int sdio_d3_pins[]		= { GPIOX_3 };
static const unsigned int sdio_clk_pins[]		= { GPIOX_4 };
static const unsigned int sdio_cmd_pins[]		= { GPIOX_5 };
static const unsigned int pwm_a_x_pins[]		= { GPIOX_6 };
static const unsigned int pwm_g_x_pins[]		= { GPIOX_7 };
static const unsigned int tdm_d1_pins[]			= { GPIOX_8 };
static const unsigned int tdm_d0_pins[]			= { GPIOX_9 };
static const unsigned int tdm_fs0_pins[]		= { GPIOX_10 };
static const unsigned int tdm_sclk0_pins[]		= { GPIOX_11 };
static const unsigned int uart_b_tx_pins[]		= { GPIOX_12 };
static const unsigned int uart_b_rx_pins[]		= { GPIOX_13 };
static const unsigned int uart_b_cts_pins[]		= { GPIOX_14 };
static const unsigned int uart_b_rts_pins[]		= { GPIOX_15 };
static const unsigned int pwm_f_pins[]			= { GPIOX_16 };
static const unsigned int i2c2_scl_x_pins[]		= { GPIOX_17 };
static const unsigned int i2c2_sda_x_pins[]		= { GPIOX_18 };
static const unsigned int pcieck_c_reqn_pins[]		= { GPIOX_19 };

/* Bank X func2 */
static const unsigned int spi_c_mosi_x_pins[]		= { GPIOX_0 };
static const unsigned int spi_c_miso_x_pins[]		= { GPIOX_1 };
static const unsigned int spi_c_ss0_x_pins[]		= { GPIOX_2 };
static const unsigned int spi_c_clk_x_pins[]		= { GPIOX_3 };
static const unsigned int world_sync_x_pins[]		= { GPIOX_19 };

/* Bank X func3 */
static const unsigned int pdm_din2_x_pins[]		= { GPIOX_0 };
static const unsigned int pdm_dclk_x_pins[]		= { GPIOX_1 };
static const unsigned int pdm_din0_x_pins[]		= { GPIOX_2 };
static const unsigned int pdm_din1_x_pins[]		= { GPIOX_3 };
static const unsigned int gen_clk_out_x_pins[]		= { GPIOX_4 };

/* Bank X func4 */
static const unsigned int mclk_0_x_pins[]		= { GPIOX_0 };
static const unsigned int tdm_sclk1_x_pins[]		= { GPIOX_1 };
static const unsigned int tdm_fs1_x_pins[]		= { GPIOX_2 };
static const unsigned int tdm_d2_x_pins[]		= { GPIOX_3 };

/* Bank H func1 */
static const unsigned int hdmitx_sda_pins[]		= { GPIOH_0 };
static const unsigned int hdmitx_sck_pins[]		= { GPIOH_1 };
static const unsigned int hdmitx_hpd_in_pins[]		= { GPIOH_2 };
static const unsigned int spdif_out_h_pins[]		= { GPIOH_4 };
static const unsigned int spdif_in_h_pins[]		= { GPIOH_5 };
static const unsigned int iso7816_clk_h_pins[]		= { GPIOH_6 };
static const unsigned int iso7816_data_h_pins[]		= { GPIOH_7 };

/* Bank H func2 */
static const unsigned int i2c3_scl_h_pins[]		= { GPIOH_0 };
static const unsigned int i2c3_sda_h_pins[]		= { GPIOH_1 };
static const unsigned int i2c1_scl_h2_pins[]		= { GPIOH_2 };
static const unsigned int i2c1_sda_h3_pins[]		= { GPIOH_3 };
static const unsigned int uart_c_tx_h_pins[]		= { GPIOH_4 };
static const unsigned int uart_c_rx_h_pins[]		= { GPIOH_5 };
static const unsigned int uart_c_cts_h_pins[]		= { GPIOH_6 };
static const unsigned int uart_c_rts_h_pins[]		= { GPIOH_7 };

/* Bank H func3 */
static const unsigned int spi_b_mosi_pins[]		= { GPIOH_4 };
static const unsigned int spi_b_miso_pins[]		= { GPIOH_5 };
static const unsigned int spi_b_ss0_pins[]		= { GPIOH_6 };
static const unsigned int spi_b_sclk_pins[]		= { GPIOH_7 };

/* Bank H func4 */
static const unsigned int cec_b_h_pins[]		= { GPIOH_3 };
static const unsigned int i2c1_scl_h6_pins[]		= { GPIOH_6 };
static const unsigned int i2c1_sda_h7_pins[]		= { GPIOH_7 };

/* Bank H func5 */
static const unsigned int remote_out_h_pins[]		= { GPIOH_6 };
static const unsigned int pwm_b_h_pins[]		= { GPIOH_7 };

/* Bank Z func1 */
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
static const unsigned int eth_txd0_pins[]		= { GPIOZ_10};
static const unsigned int eth_txd1_pins[]		= { GPIOZ_11 };
static const unsigned int eth_txd2_rgmii_pins[]		= { GPIOZ_12 };
static const unsigned int eth_txd3_rgmii_pins[]		= { GPIOZ_13 };
static const unsigned int i2c5_scl_z_pins[]		= { GPIOZ_14 };
static const unsigned int i2c5_sda_z_pins[]		= { GPIOZ_15};

/* Bank T func1 */
static const unsigned int i2c2_scl_t_pins[]		= { GPIOT_1 };
static const unsigned int i2c2_sda_t_pins[]		= { GPIOT_0 };
static const unsigned int tsin_b_valid_pins[]		= { GPIOT_2 };
static const unsigned int tsin_b_sop_pins[]		= { GPIOT_3 };
static const unsigned int tsin_b_din0_pins[]		= { GPIOT_4 };
static const unsigned int tsin_b_clk_pins[]		= { GPIOT_5 };
static const unsigned int tsin_b_fail_pins[]		= { GPIOT_6 };
static const unsigned int tsin_b_din1_pins[]		= { GPIOT_7 };
static const unsigned int tsin_b_din2_pins[]		= { GPIOT_8 };
static const unsigned int tsin_b_din3_pins[]		= { GPIOT_9 };
static const unsigned int tsin_b_din4_pins[]		= { GPIOT_10 };
static const unsigned int tsin_b_din5_pins[]		= { GPIOT_11 };
static const unsigned int tsin_b_din6_pins[]		= { GPIOT_12 };
static const unsigned int tsin_b_din7_pins[]		= { GPIOT_13 };
static const unsigned int tsin_a_valid_pins[]		= { GPIOT_14 };
static const unsigned int tsin_a_sop_pins[]		= { GPIOT_15 };
static const unsigned int tsin_a_din0_pins[]		= { GPIOT_16 };
static const unsigned int tsin_a_clk_pins[]		= { GPIOT_17 };
static const unsigned int tsin_c_valid_t18_pins[]	= { GPIOT_18 };
static const unsigned int tsin_c_sop_t19_pins[]		= { GPIOT_19 };
static const unsigned int tsin_c_din0_t20_pins[]	= { GPIOT_20 };
static const unsigned int tsin_c_clk_t21_pins[]		= { GPIOT_21 };
static const unsigned int i2c3_scl_t_pins[]		= { GPIOT_22 };
static const unsigned int i2c3_sda_t_pins[]		= { GPIOT_23 };

/* Bank T func2 */
static const unsigned int spi_a_mosi_t_pins[]		= { GPIOT_2 };
static const unsigned int spi_a_miso_t_pins[]		= { GPIOT_3 };
static const unsigned int spi_a_ss0_t_pins[]		= { GPIOT_4 };
static const unsigned int spi_a_sclk_t_pins[]		= { GPIOT_5 };
static const unsigned int tsin_d_valid_pins[]		= { GPIOT_6 };
static const unsigned int tsin_d_sop_pins[]		= { GPIOT_7 };
static const unsigned int tsin_d_din0_pins[]		= { GPIOT_8 };
static const unsigned int tsin_d_clk_pins[]		= { GPIOT_9 };
static const unsigned int tsin_c_valid_t10_pins[]	= { GPIOT_10 };
static const unsigned int tsin_c_sop_t11_pins[]		= { GPIOT_11 };
static const unsigned int tsin_c_din0_t12_pins[]	= { GPIOT_12 };
static const unsigned int tsin_c_clk_t13_pins[]		= { GPIOT_13 };
static const unsigned int uart_e_cts_pins[]		= { GPIOT_14 };
static const unsigned int uart_e_rx_pins[]		= { GPIOT_15 };
static const unsigned int uart_e_tx_pins[]		= { GPIOT_16 };
static const unsigned int uart_e_rts_pins[]		= { GPIOT_17 };
static const unsigned int uart_f_rts_pins[]		= { GPIOT_18 };
static const unsigned int uart_f_cts_pins[]		= { GPIOT_19 };
static const unsigned int uart_f_rx_pins[]		= { GPIOT_20 };
static const unsigned int uart_f_tx_pins[]		= { GPIOT_21 };
static const unsigned int pcieck_b_reqn_pins[]		= { GPIOT_23 };

/* Bank T func3 */
static const unsigned int tdm_d4_t_pins[]	= { GPIOT_0 };
static const unsigned int tdm_d5_t_pins[]	= { GPIOT_1 };
static const unsigned int tdm_d6_t_pins[]	= { GPIOT_2 };
static const unsigned int tdm_d7_t_pins[]	= { GPIOT_3 };
static const unsigned int tdm_d8_t_pins[]	= { GPIOT_4 };
static const unsigned int tdm_d9_t_pins[]	= { GPIOT_5 };
static const unsigned int tdm_d10_pins[]	= { GPIOT_6 };
static const unsigned int tdm_fs2_t_pins[]	= { GPIOT_7 };
static const unsigned int tdm_sclk2_t_pins[]	= { GPIOT_8 };
static const unsigned int mclk_2_pins[]		= { GPIOT_9 };
static const unsigned int tdm_d11_pins[]	= { GPIOT_10 };
static const unsigned int tdm_d12_pins[]	= { GPIOT_11 };
static const unsigned int tdm_d13_pins[]	= { GPIOT_12 };
static const unsigned int tdm_d14_pins[]	= { GPIOT_13 };
static const unsigned int tdm_d15_pins[]	= { GPIOT_14 };
static const unsigned int pwm_h_t_pins[]	= { GPIOT_15 };
static const unsigned int pwm_i_t_pins[]	= { GPIOT_16 };
static const unsigned int pwm_j_t_pins[]	= { GPIOT_17 };
static const unsigned int pdm_din3_t_pins[]	= { GPIOT_18 };
static const unsigned int pdm_din2_t_pins[]	= { GPIOT_19 };
static const unsigned int pdm_dclk_t_pins[]	= { GPIOT_20 };
static const unsigned int pdm_din0_t_pins[]	= { GPIOT_21 };
static const unsigned int pdm_din1_t_pins[]	= { GPIOT_22 };

/* Bank T func4 */
static const unsigned int i2c5_scl_t_pins[]	= { GPIOT_14 };
static const unsigned int i2c5_sda_t_pins[]	= { GPIOT_15 };
static const unsigned int i2c4_scl_t_pins[]	= { GPIOT_16 };
static const unsigned int i2c4_sda_t_pins[]	= { GPIOT_17 };
static const unsigned int tdm_sclk1_t_pins[]	= { GPIOT_18 };
static const unsigned int tdm_fs1_t_pins[]	= { GPIOT_19 };
static const unsigned int tdm_d2_t_pins[]	= { GPIOT_20 };
static const unsigned int tdm_d3_t_pins[]	= { GPIOT_21 };
static const unsigned int mclk_0_t_pins[]	= { GPIOT_22 };

/* Bank T func5 */
static const unsigned int iso7816_clk_t_pins[]	= { GPIOT_20 };
static const unsigned int iso7816_data_t_pins[]	= { GPIOT_21 };

/* Bank A func1 */
static const unsigned int pdm_din3_a_pins[]	= { GPIOA_0 };
static const unsigned int pdm_din2_a_pins[]	= { GPIOA_1 };
static const unsigned int pdm_dclk_a_pins[]	= { GPIOA_2 };
static const unsigned int pdm_din0_a_pins[]	= { GPIOA_3 };
static const unsigned int pdm_din1_a_pins[]	= { GPIOA_4 };
static const unsigned int spdif_in_a5_pins[]	= { GPIOA_5 };
static const unsigned int spdif_out_a6_pins[]	= { GPIOA_6 };
static const unsigned int spdif_in_a7_pins[]	= { GPIOA_7 };
static const unsigned int spdif_out_a8_pins[]	= { GPIOA_8 };
static const unsigned int world_sync_a_pins[]	= { GPIOA_9 };
static const unsigned int remote_in_a10_pins[]	= { GPIOA_10 };

/* Bank A func2 */
static const unsigned int tdm_d4_a_pins[]	= { GPIOA_0 };
static const unsigned int tdm_d5_a_pins[]	= { GPIOA_1 };
static const unsigned int tdm_d6_a_pins[]	= { GPIOA_2 };
static const unsigned int tdm_d7_a_pins[]	= { GPIOA_3 };
static const unsigned int tdm_d8_a_pins[]	= { GPIOA_4 };
static const unsigned int tdm_d9_a_pins[]	= { GPIOA_5 };
static const unsigned int mclk_1_pins[]		= { GPIOA_6 };
static const unsigned int tdm_sclk2_a_pins[]	= { GPIOA_7 };
static const unsigned int tdm_fs2_a_pins[]	= { GPIOA_8 };
static const unsigned int i2c1_scl_a_pins[]	= { GPIOA_9 };
static const unsigned int i2c1_sda_a_pins[]	= { GPIOA_10 };

/* Bank A func3 */
static const unsigned int spi_c_mosi_a_pins[]	= { GPIOA_0 };
static const unsigned int spi_c_miso_a_pins[]	= { GPIOA_1 };
static const unsigned int spi_c_ss0_a_pins[]	= { GPIOA_2 };
static const unsigned int spi_c_sclk_a_pins[]	= { GPIOA_3 };
static const unsigned int i2c2_sda_a_pins[]	= { GPIOA_4 };
static const unsigned int i2c2_scl_a_pins[]	= { GPIOA_5 };
static const unsigned int i2c5_sda_a_pins[]	= { GPIOA_7 };
static const unsigned int i2c5_scl_a_pins[]	= { GPIOA_8 };

/* Bank A func4 */
static const unsigned int uart_c_rts_a_pins[]	= { GPIOA_0 };
static const unsigned int uart_c_cts_a_pins[]	= { GPIOA_1 };
static const unsigned int uart_c_rx_a_pins[]	= { GPIOA_2 };
static const unsigned int uart_c_tx_a_pins[]	= { GPIOA_3 };
static const unsigned int mute_en_pins[]	= { GPIOA_7 };
static const unsigned int mute_led_pins[]	= { GPIOA_8 };

static struct meson_pmx_group meson_s5_periphs_groups[] = {
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
	GPIO_GROUP(GPIOE_3),
	GPIO_GROUP(GPIOE_4),

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

	GPIO_GROUP(GPIOT_0),
	GPIO_GROUP(GPIOT_1),
	GPIO_GROUP(GPIOT_2),
	GPIO_GROUP(GPIOT_3),
	GPIO_GROUP(GPIOT_4),
	GPIO_GROUP(GPIOT_5),
	GPIO_GROUP(GPIOT_6),
	GPIO_GROUP(GPIOT_7),
	GPIO_GROUP(GPIOT_8),
	GPIO_GROUP(GPIOT_9),
	GPIO_GROUP(GPIOT_10),
	GPIO_GROUP(GPIOT_11),
	GPIO_GROUP(GPIOT_12),
	GPIO_GROUP(GPIOT_13),
	GPIO_GROUP(GPIOT_14),
	GPIO_GROUP(GPIOT_15),
	GPIO_GROUP(GPIOT_16),
	GPIO_GROUP(GPIOT_17),
	GPIO_GROUP(GPIOT_18),
	GPIO_GROUP(GPIOT_19),
	GPIO_GROUP(GPIOT_20),
	GPIO_GROUP(GPIOT_21),
	GPIO_GROUP(GPIOT_22),
	GPIO_GROUP(GPIOT_23),
	GPIO_GROUP(GPIOT_24),

	GPIO_GROUP(GPIOA_0),
	GPIO_GROUP(GPIOA_1),
	GPIO_GROUP(GPIOA_2),
	GPIO_GROUP(GPIOA_3),
	GPIO_GROUP(GPIOA_4),
	GPIO_GROUP(GPIOA_5),
	GPIO_GROUP(GPIOA_6),
	GPIO_GROUP(GPIOA_7),
	GPIO_GROUP(GPIOA_8),
	GPIO_GROUP(GPIOA_9),
	GPIO_GROUP(GPIOA_10),

	GPIO_GROUP(GPIO_TEST_N),

	/* BANK D func1 */
	GROUP(uart_a_tx_d,		1),
	GROUP(uart_a_rx_d,		1),
	GROUP(i2c0_scl_d,		1),
	GROUP(i2c0_sda_d,		1),
	GROUP(remote_out_d4,		1),
	GROUP(remote_in_d,		1),
	GROUP(jtag_a_clk,		1),
	GROUP(jtag_a_tms,		1),
	GROUP(jtag_a_tdi,		1),
	GROUP(jtag_a_tdo,		1),
	GROUP(pwm_g_hiz,		1),

	/* BANK D func2 */
	GROUP(uart_c_tx_d,		2),
	GROUP(uart_c_rx_d,		2),
	GROUP(clk_32k_in,		2),
	GROUP(pwm_h_d,			2),
	GROUP(pwm_i_d,			2),
	GROUP(pwm_j_d,			2),
	GROUP(remote_out_d9,		2),
	GROUP(cec_b_d,			2),
	GROUP(pwm_g_d,			2),

	/* BANK D func3 */
	GROUP(i2c_slave_scl,		3),
	GROUP(i2c_slave_sda,		3),
	GROUP(uart_d_cts,		3),
	GROUP(uart_d_rts,		3),
	GROUP(uart_d_tx,		3),
	GROUP(uart_d_rx,		3),
	GROUP(pwm_d_d,			3),
	GROUP(gen_clk_out_d,		3),

	/* BANK D func4 */
	GROUP(spdif_out_d,		4),
	GROUP(clk12m_24m,		4),

	/* BANK D func5 */
	GROUP(clk25m,			5),

	/* BANK E func1 */
	GROUP(pwm_b_e,			1),
	GROUP(pwm_d_e,			1),
	GROUP(pwm_a_e,			1),
	GROUP(pwm_c_e,			1),
	GROUP(pwm_e,			1),

	/* BANK E func2 */
	GROUP(i2c0_scl_e,		2),
	GROUP(i2c0_sda_e,		2),
	GROUP(i2c4_scl_e,		2),
	GROUP(i2c4_sda_e,		2),

	/* Bank C func1 */
	GROUP(sdcard_d0,		1),
	GROUP(sdcard_d1,		1),
	GROUP(sdcard_d2,		1),
	GROUP(sdcard_d3,		1),
	GROUP(sdcard_clk,		1),
	GROUP(sdcard_cmd,		1),
	GROUP(sdcard_cd,		1),

	/* Bank C func2 */
	GROUP(jtag_b_tdo,		2),
	GROUP(jtag_b_tdi,		2),
	GROUP(uart_a_rx_c,		2),
	GROUP(uart_a_tx_c,		2),
	GROUP(jtag_b_clk,		2),
	GROUP(jtag_b_tms,		2),

	/* Bank C func3 */
	GROUP(spi_a_mosi_c,		3),
	GROUP(spi_a_miso_c,		3),
	GROUP(spi_a_ss0_c,		3),
	GROUP(spi_a_clk_c,		3),
	GROUP(pwm_c_c,			3),

	/* Bank X func1 */
	GROUP(sdio_d0,			1),
	GROUP(sdio_d1,			1),
	GROUP(sdio_d2,			1),
	GROUP(sdio_d3,			1),
	GROUP(sdio_clk,			1),
	GROUP(sdio_cmd,			1),
	GROUP(pwm_a_x,			1),
	GROUP(pwm_g_x,			1),
	GROUP(tdm_d1,			1),
	GROUP(tdm_d0,			1),
	GROUP(tdm_fs0,			1),
	GROUP(tdm_sclk0,		1),
	GROUP(uart_b_tx,		1),
	GROUP(uart_b_rx,		1),
	GROUP(uart_b_cts,		1),
	GROUP(uart_b_rts,		1),
	GROUP(pwm_f,			1),
	GROUP(i2c2_scl_x,		1),
	GROUP(i2c2_sda_x,		1),
	GROUP(pcieck_c_reqn,		1),

	/* Bank X func2 */
	GROUP(spi_c_mosi_x,		2),
	GROUP(spi_c_miso_x,		2),
	GROUP(spi_c_ss0_x,		2),
	GROUP(spi_c_clk_x,		2),
	GROUP(world_sync_x,		2),

	/* Bank X func3 */
	GROUP(pdm_din2_x,		3),
	GROUP(pdm_dclk_x,		3),
	GROUP(pdm_din0_x,		3),
	GROUP(pdm_din1_x,		3),
	GROUP(gen_clk_out_x,		3),

	/* Bank X func4 */
	GROUP(mclk_0_x,			4),
	GROUP(tdm_sclk1_x,		4),
	GROUP(tdm_fs1_x,		4),
	GROUP(tdm_d2_x,			4),

	/* Bank H func1 */
	GROUP(hdmitx_sda,		1),
	GROUP(hdmitx_sck,		1),
	GROUP(hdmitx_hpd_in,		1),
	GROUP(spdif_out_h,		1),
	GROUP(spdif_in_h,		1),
	GROUP(iso7816_clk_h,		1),
	GROUP(iso7816_data_h,		1),

	/* Bank H func2 */
	GROUP(i2c3_scl_h,		2),
	GROUP(i2c3_sda_h,		2),
	GROUP(i2c1_scl_h2,		2),
	GROUP(i2c1_sda_h3,		2),
	GROUP(uart_c_tx_h,		2),
	GROUP(uart_c_rx_h,		2),
	GROUP(uart_c_cts_h,		2),
	GROUP(uart_c_rts_h,		2),

	/* Bank H func3 */
	GROUP(spi_b_mosi,		3),
	GROUP(spi_b_miso,		3),
	GROUP(spi_b_ss0,		3),
	GROUP(spi_b_sclk,		3),

	/* Bank H func4 */
	GROUP(cec_b_h,			4),
	GROUP(i2c1_scl_h6,		4),
	GROUP(i2c1_sda_h7,		4),

	/* Bank H func5 */
	GROUP(remote_out_h,		5),
	GROUP(pwm_b_h,			5),

	/* Bank Z func1 */
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
	GROUP(i2c5_scl_z,		1),
	GROUP(i2c5_sda_z,		1),

	/* Bank T func1 */
	GROUP(i2c2_scl_t,		1),
	GROUP(i2c2_sda_t,		1),
	GROUP(tsin_b_valid,		1),
	GROUP(tsin_b_sop,		1),
	GROUP(tsin_b_din0,		1),
	GROUP(tsin_b_clk,		1),
	GROUP(tsin_b_fail,		1),
	GROUP(tsin_b_din1,		1),
	GROUP(tsin_b_din2,		1),
	GROUP(tsin_b_din3,		1),
	GROUP(tsin_b_din4,		1),
	GROUP(tsin_b_din5,		1),
	GROUP(tsin_b_din6,		1),
	GROUP(tsin_b_din7,		1),
	GROUP(tsin_a_valid,		1),
	GROUP(tsin_a_sop,		1),
	GROUP(tsin_a_din0,		1),
	GROUP(tsin_a_clk,		1),
	GROUP(tsin_c_valid_t18,		1),
	GROUP(tsin_c_sop_t19,		1),
	GROUP(tsin_c_din0_t20,		1),
	GROUP(tsin_c_clk_t21,		1),
	GROUP(i2c3_scl_t,		1),
	GROUP(i2c3_sda_t,		1),

	/* Bank T func2 */
	GROUP(spi_a_mosi_t,		2),
	GROUP(spi_a_miso_t,		2),
	GROUP(spi_a_ss0_t,		2),
	GROUP(spi_a_sclk_t,		2),
	GROUP(tsin_d_valid,		2),
	GROUP(tsin_d_sop,		2),
	GROUP(tsin_d_din0,		2),
	GROUP(tsin_d_clk,		2),
	GROUP(tsin_c_valid_t10,		2),
	GROUP(tsin_c_sop_t11,		2),
	GROUP(tsin_c_din0_t12,		2),
	GROUP(tsin_c_clk_t13,		2),
	GROUP(uart_e_cts,		2),
	GROUP(uart_e_rx,		2),
	GROUP(uart_e_tx,		2),
	GROUP(uart_e_rts,		2),
	GROUP(uart_f_rts,		2),
	GROUP(uart_f_cts,		2),
	GROUP(uart_f_rx,		2),
	GROUP(uart_f_tx,		2),
	GROUP(pcieck_b_reqn,		2),

	/* Bank T func3 */
	GROUP(tdm_d4_t,			3),
	GROUP(tdm_d5_t,			3),
	GROUP(tdm_d6_t,			3),
	GROUP(tdm_d7_t,			3),
	GROUP(tdm_d8_t,			3),
	GROUP(tdm_d9_t,			3),
	GROUP(tdm_d10,			3),
	GROUP(tdm_fs2_t,		3),
	GROUP(tdm_sclk2_t,		3),
	GROUP(mclk_2,			3),
	GROUP(tdm_d11,			3),
	GROUP(tdm_d12,			3),
	GROUP(tdm_d13,			3),
	GROUP(tdm_d14,			3),
	GROUP(tdm_d15,			3),
	GROUP(pwm_h_t,			3),
	GROUP(pwm_i_t,			3),
	GROUP(pwm_j_t,			3),
	GROUP(pdm_din3_t,		3),
	GROUP(pdm_din2_t,		3),
	GROUP(pdm_dclk_t,		3),
	GROUP(pdm_din0_t,		3),
	GROUP(pdm_din1_t,		3),

	/* Bank T func4 */
	GROUP(i2c5_scl_t,		4),
	GROUP(i2c5_sda_t,		4),
	GROUP(i2c4_scl_t,		4),
	GROUP(i2c4_sda_t,		4),
	GROUP(tdm_sclk1_t,		4),
	GROUP(tdm_fs1_t,		4),
	GROUP(tdm_d2_t,			4),
	GROUP(tdm_d3_t,			4),
	GROUP(mclk_0_t,			4),

	/* Bank T func5 */
	GROUP(iso7816_clk_t,		5),
	GROUP(iso7816_data_t,		5),

	/* Bank A func1 */
	GROUP(pdm_din3_a,		1),
	GROUP(pdm_din2_a,		1),
	GROUP(pdm_dclk_a,		1),
	GROUP(pdm_din0_a,		1),
	GROUP(pdm_din1_a,		1),
	GROUP(spdif_in_a5,		1),
	GROUP(spdif_out_a6,		1),
	GROUP(spdif_in_a7,		1),
	GROUP(spdif_out_a8,		1),
	GROUP(world_sync_a,		1),
	GROUP(remote_in_a10,		1),

	/* Bank A func2 */
	GROUP(tdm_d4_a,			2),
	GROUP(tdm_d5_a,			2),
	GROUP(tdm_d6_a,			2),
	GROUP(tdm_d7_a,			2),
	GROUP(tdm_d8_a,			2),
	GROUP(tdm_d9_a,			2),
	GROUP(mclk_1,			2),
	GROUP(tdm_sclk2_a,		2),
	GROUP(tdm_fs2_a,		2),
	GROUP(i2c1_scl_a,		2),
	GROUP(i2c1_sda_a,		2),

	/* Bank A func3 */
	GROUP(spi_c_mosi_a,		3),
	GROUP(spi_c_miso_a,		3),
	GROUP(spi_c_ss0_a,		3),
	GROUP(spi_c_sclk_a,		3),
	GROUP(i2c2_sda_a,		3),
	GROUP(i2c2_scl_a,		3),
	GROUP(i2c5_sda_a,		3),
	GROUP(i2c5_scl_a,		3),

	/* Bank A func4 */
	GROUP(uart_c_rts_a,		4),
	GROUP(uart_c_cts_a,		4),
	GROUP(uart_c_rx_a,		4),
	GROUP(uart_c_tx_a,		4),
	GROUP(mute_en,			4),
	GROUP(mute_led,			4),
};

static struct meson_pmx_group meson_s5_storage_groups[] = {
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

	/* BANK B func1 */
	GROUP(emmc_nand_d0,		1),
	GROUP(emmc_nand_d1,		1),
	GROUP(emmc_nand_d2,		1),
	GROUP(emmc_nand_d3,		1),
	GROUP(emmc_nand_d4,		1),
	GROUP(emmc_nand_d5,		1),
	GROUP(emmc_nand_d6,		1),
	GROUP(emmc_nand_d7,		1),
	GROUP(emmc_clk,			1),
	GROUP(emmc_rst,			1),
	GROUP(emmc_cmd,			1),
	GROUP(emmc_nand_ds,		1),

	/* BANK B func2 */
	GROUP(spif_hold,		2),
	GROUP(spif_mo,			2),
	GROUP(spif_mi,			2),
	GROUP(spif_clk,			2),
	GROUP(spif_wp,			2),
	GROUP(spif_cs,			2)
};

static const char * const gpio_storage_groups[] = {
	"GPIOB_0", "GPIOB_1", "GPIOB_2", "GPIOB_3", "GPIOB_4", "GPIOB_5",
	"GPIOB_6", "GPIOB_7", "GPIOB_8", "GPIOB_9", "GPIOB_10", "GPIOB_11",
	"GPIOB_12",
};

static const char * const gpio_periphs_groups[] = {
	"GPIOD_0", "GPIOD_1", "GPIOD_2", "GPIOD_3", "GPIOD_4", "GPIOD_5",
	"GPIOD_6", "GPIOD_7", "GPIOD_8", "GPIOD_9", "GPIOD_10",
	"GPIOD_11",

	"GPIOE_0", "GPIOE_1", "GPIOE_2", "GPIOE_3", "GPIOE_4",

	"GPIOB_0", "GPIOB_1", "GPIOB_2", "GPIOB_3", "GPIOB_4", "GPIOB_5",
	"GPIOB_6", "GPIOB_7", "GPIOB_8", "GPIOB_9", "GPIOB_10",
	"GPIOB_11", "GPIOB_12",

	"GPIOC_0", "GPIOC_1", "GPIOC_2", "GPIOC_3", "GPIOC_4",
	"GPIOC_5", "GPIOC_6", "GPIOC_7",

	"GPIOX_0", "GPIOX_1", "GPIOX_2", "GPIOX_3", "GPIOX_4", "GPIOX_5",
	"GPIOX_6", "GPIOX_7", "GPIOX_8", "GPIOX_9", "GPIOX_10", "GPIOX_11",
	"GPIOX_12", "GPIOX_13", "GPIOX_14", "GPIOX_15", "GPIOX_16", "GPIOX_17",
	"GPIOX_18", "GPIOX_19",

	"GPIOH_0", "GPIOH_1", "GPIOH_2", "GPIOH_3", "GPIOH_4", "GPIOH_5",
	"GPIOH_6", "GPIOH_7", "GPIOH_8",

	"GPIOZ_0", "GPIOZ_1", "GPIOZ_2", "GPIOZ_3", "GPIOZ_4", "GPIOZ_5",
	"GPIOZ_6", "GPIOZ_7", "GPIOZ_8", "GPIOZ_9", "GPIOZ_10", "GPIOZ_11",
	"GPIOZ_12", "GPIOZ_13", "GPIOZ_14", "GPIOZ_15",

	"GPIOT_0", "GPIOT_1", "GPIOT_2", "GPIOT_3", "GPIOT_4", "GPIOT_5",
	"GPIOT_6", "GPIOT_7", "GPIOT_8", "GPIOT_9", "GPIOT_10", "GPIOT_11",
	"GPIOT_12", "GPIOT_13", "GPIOT_14", "GPIOT_15", "GPIOT_16", "GPIOT_17",
	"GPIOT_18", "GPIOT_19", "GPIOT_20", "GPIOT_21", "GPIOT_22",
	"GPIOT_23", "GPIOT_24",

	"GPIOA_0", "GPIOA_1", "GPIOA_2", "GPIOA_3", "GPIOA_4", "GPIOA_5",
	"GPIOA_6", "GPIOA_7", "GPIOA_8", "GPIOA_9", "GPIOA_10",

	"GPIO_TEST_N"
};

static const char * const i2c0_groups[] = {
	"i2c0_scl_d", "i2c0_sda_d", "i2c0_scl_e", "i2c0_sda_e"
};

static const char * const i2c1_groups[] = {
	"i2c1_scl_a", "i2c1_sda_a",
	"i2c1_scl_h2", "i2c1_sda_h3",
	"i2c1_scl_h6", "i2c1_sda_h7"
};

static const char * const i2c2_groups[] = {
	"i2c2_sda_a", "i2c2_scl_a",
	"i2c2_scl_t", "i2c2_sda_t",
	"i2c2_scl_x", "i2c2_sda_x"
};

static const char * const i2c3_groups[] = {
	"i2c3_scl_h", "i2c3_sda_h",
	"i2c3_scl_t", "i2c3_sda_t"
};

static const char * const i2c4_groups[] = {
	"i2c4_scl_e", "i2c4_sda_e",
	"i2c4_scl_t", "i2c4_sda_t"
};

static const char * const i2c5_groups[] = {
	"i2c5_scl_z", "i2c5_sda_z",
	"i2c5_scl_t", "i2c5_sda_t",
	"i2c5_sda_a", "i2c5_scl_a"
};

static const char * const uart_a_groups[] = {
	"uart_a_tx_d", "uart_a_rx_d",
	"uart_a_rx_c", "uart_a_tx_c"
};

static const char * const uart_b_groups[] = {
	"uart_b_tx", "uart_b_rx", "uart_b_cts", "uart_b_rts"
};

static const char * const uart_c_groups[] = {
	"uart_c_rx_a", "uart_c_tx_a",
	"uart_c_tx_d", "uart_c_rx_d", "uart_c_tx_h",
	"uart_c_rts_a", "uart_c_cts_a",
	"uart_c_cts_h", "uart_c_rts_h", "uart_c_rx_h",
};

static const char * const uart_d_groups[] = {
	"uart_d_tx", "uart_d_rx", "uart_d_cts", "uart_d_rts"
};

static const char * const uart_e_groups[] = {
	"uart_e_rx", "uart_e_tx", "uart_e_cts", "uart_e_rts"
};

static const char * const uart_f_groups[] = {
	"uart_f_rx", "uart_f_tx", "uart_f_rts", "uart_f_cts"
};

static const char * const emmc_groups[] = {
	"emmc_nand_d0", "emmc_nand_d1", "emmc_nand_d2", "emmc_nand_d3",
	"emmc_nand_d4", "emmc_nand_d5", "emmc_nand_d6", "emmc_nand_d7",
	"emmc_clk", "emmc_rst", "emmc_cmd", "emmc_nand_ds"
};

static const char * const spif_groups[] = {
	"spif_mo", "spif_mi", "spif_wp", "spif_cs",
	"spif_clk", "spif_hold",
};

static const char * const spi_a_groups[] = {
	"spi_a_ss0_c", "spi_a_clk_c", "spi_a_ss0_t", "spi_a_mosi_c",
	"spi_a_miso_c", "spi_a_mosi_t", "spi_a_miso_t", "spi_a_sclk_t"
};

static const char * const spi_b_groups[] = {
	"spi_b_ss0", "spi_b_mosi", "spi_b_miso", "spi_b_sclk"
};

static const char * const spi_c_groups[] = {
	"spi_c_ss0_a", "spi_c_ss0_x", "spi_c_clk_x", "spi_c_mosi_a",
	"spi_c_miso_a", "spi_c_sclk_a", "spi_c_mosi_x", "spi_c_miso_x"
};

static const char * const sdcard_groups[] = {
	"sdcard_d0", "sdcard_d1", "sdcard_d2", "sdcard_d3", "sdcard_cd",
	"sdcard_clk", "sdcard_cmd"
};

static const char * const jtag_a_groups[] = {
	"jtag_a_tms", "jtag_a_clk", "jtag_a_tdi", "jtag_a_tdo"
};

static const char * const jtag_b_groups[] = {
	"jtag_b_tms", "jtag_b_tdo", "jtag_b_tdi", "jtag_b_clk"
};

static const char * const pdm_groups[] = {
	"pdm_din3_a", "pdm_din2_a", "pdm_dclk_a", "pdm_din0_a", "pdm_din1_a",
	"pdm_dclk_x", "pdm_din2_x", "pdm_din0_x", "pdm_din1_x",
	"pdm_din3_t", "pdm_din2_t", "pdm_dclk_t", "pdm_din0_t", "pdm_din1_t"
};

static const char * const iso7816_groups[] = {
	"iso7816_clk_h", "iso7816_data_h",
	"iso7816_data_t", "iso7816_clk_t",
};

static const char * const tdm_groups[] = {
	"tdm_d0", "tdm_d1", "tdm_d4_t", "tdm_d5_t", "tdm_d6_t",
	"tdm_d7_t", "tdm_d8_t", "tdm_d9_t", "tdm_d4_a", "tdm_d5_a",
	"tdm_d6_a", "tdm_d7_a", "tdm_d8_a", "tdm_d9_a", "tdm_d10",
	"tdm_d11", "tdm_d12", "tdm_d13", "tdm_d14", "tdm_d15",
	"tdm_fs0", "tdm_d2_x", "tdm_d2_t", "tdm_d3_t", "tdm_fs2_t",
	"tdm_fs2_a", "tdm_fs1_x", "tdm_fs1_t", "tdm_sclk0",
	"tdm_sclk2_t", "tdm_sclk2_a", "tdm_sclk1_x", "tdm_sclk1_t",
};

static const char * const mclk_groups[] = {
	"mclk_2", "mclk_1", "mclk_0_x", "mclk_0_t"
};

static const char * const remote_out_groups[] = {
	"remote_out_h", "remote_out_d4", "remote_out_d9", "remote_out_a10"
};

static const char * const remote_in_groups[] = {
	"remote_in_d", "remote_in_a10"
};

static const char * const clk12m_24m_groups[] = {
	"clk12m_24m"
};

static const char * const clk_32k_in_groups[] = {
	"clk_32k_in"
};

static const char * const pwm_g_hiz_groups[] = {
	"pwm_g_hiz"
};

static const char * const pwm_a_groups[] = {
	"pwm_a_e", "pwm_a_x"
};

static const char * const pwm_b_groups[] = {
	"pwm_b_e", "pwm_b_h"
};

static const char * const pwm_c_groups[] = {
	"pwm_c_e", "pwm_c_c"
};

static const char * const pwm_d_groups[] = {
	"pwm_d_e", "pwm_d_d"
};

static const char * const pwm_e_groups[] = {
	"pwm_e"
};

static const char * const pwm_f_groups[] = {
	"pwm_f",
};

static const char * const pwm_g_groups[] = {
	"pwm_g_d", "pwm_g_x"
};

static const char * const pwm_h_groups[] = {
	"pwm_h_d", "pwm_h_t"
};

static const char * const pwm_i_groups[] = {
	"pwm_i_d", "pwm_i_t"
};

static const char * const pwm_j_groups[] = {
	"pwm_j_d", "pwm_j_t"
};

static const char * const mute_groups[] = {
	"mute_en", "mute_led"
};

static const char * const hdmitx_groups[] = {
	"hdmitx_sda", "hdmitx_sck", "hdmitx_hpd_in"
};

static const char * const cec_b_groups[] = {
	"cec_b_h", "cec_b_d"
};

static const char * const spdif_out_groups[] = {
	"spdif_out_h", "spdif_out_a6", "spdif_out_a8", "spdif_out_d"
};

static const char * const spdif_in_groups[] = {
	"spdif_in_h", "spdif_in_a5", "spdif_in_a7"
};

static const char * const eth_groups[] = {
	"eth_mdc", "eth_mdio", "eth_rxd0", "eth_rxd1", "eth_txen",
	"eth_txd0", "eth_txd1", "eth_rx_dv", "eth_rxd2_rgmii", "eth_rxd3_rgmii",
	"eth_txd2_rgmii", "eth_txd3_rgmii", "eth_rgmii_rx_clk",
	"eth_rgmii_tx_clk"
};

static const char * const gen_clk_groups[] = {
	"gen_clk_out_d", "gen_clk_out_x"
};

static const char * const sdio_groups[] = {
	"sdio_d0", "sdio_d1", "sdio_d2", "sdio_d3",
	"sdio_clk", "sdio_cmd"
};

static const char * const i2c_slave_groups[] = {
	"i2c_slave_scl", "i2c_slave_sda"
};

static const char * const tsin_a_groups[] = {
	"tsin_a_sop", "tsin_a_clk", "tsin_a_din0", "tsin_a_valid"
};

static const char * const tsin_b_groups[] = {
	"tsin_b_sop", "tsin_b_clk", "tsin_b_din0", "tsin_b_fail",
	"tsin_b_din1", "tsin_b_din2", "tsin_b_din3", "tsin_b_din4",
	"tsin_b_din5", "tsin_b_din6", "tsin_b_din7", "tsin_b_valid"
};

static const char * const tsin_c_groups[] = {
	"tsin_c_sop_t19", "tsin_c_clk_t21", "tsin_c_sop_t11",
	"tsin_c_clk_t13", "tsin_c_din0_t20", "tsin_c_din0_t12",
	"tsin_c_valid_t18", "tsin_c_valid_t10"
};

static const char * const tsin_d_groups[] = {
	"tsin_d_sop", "tsin_d_clk", "tsin_d_din0", "tsin_d_valid"
};

static const char * const pcieck_groups[] = {
	"pcieck_c_reqn", "pcieck_b_reqn"
};

static struct meson_pmx_func meson_s5_storage_functions[] = {
	FUNCTION(gpio_storage),
	FUNCTION(emmc),
	FUNCTION(spif),
};

static struct meson_pmx_func meson_s5_periphs_functions[] = {
	FUNCTION(gpio_periphs),
	FUNCTION(i2c0),
	FUNCTION(i2c1),
	FUNCTION(i2c2),
	FUNCTION(i2c3),
	FUNCTION(i2c4),
	FUNCTION(i2c5),
	FUNCTION(uart_a),
	FUNCTION(uart_b),
	FUNCTION(uart_c),
	FUNCTION(uart_d),
	FUNCTION(uart_e),
	FUNCTION(uart_f),
	FUNCTION(spi_a),
	FUNCTION(spi_b),
	FUNCTION(spi_c),
	FUNCTION(sdcard),
	FUNCTION(jtag_a),
	FUNCTION(jtag_b),
	FUNCTION(pdm),
	FUNCTION(iso7816),
	FUNCTION(tdm),
	FUNCTION(mclk),
	FUNCTION(remote_out),
	FUNCTION(remote_in),
	FUNCTION(clk12m_24m),
	FUNCTION(clk_32k_in),
	FUNCTION(pwm_g_hiz),
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
	FUNCTION(mute),
	FUNCTION(hdmitx),
	FUNCTION(cec_b),
	FUNCTION(spdif_out),
	FUNCTION(spdif_in),
	FUNCTION(eth),
	FUNCTION(spi_a),
	FUNCTION(gen_clk),
	FUNCTION(sdio),
	FUNCTION(i2c_slave),
	FUNCTION(tsin_a),
	FUNCTION(tsin_b),
	FUNCTION(tsin_c),
	FUNCTION(tsin_d),
	FUNCTION(pcieck)
};

static struct meson_bank meson_s5_storage_banks[] = {
	BANK_DS("B", GPIOB_0,    GPIOB_12, 0, 12,
		0x03,  0,  0x04,  0,  0x02, 0,  0x01, 0,  0x00, 0, 0x07, 0),
};

static struct meson_pmx_bank meson_s5_storage_pmx_banks[] = {
	BANK_PMX("B",      GPIOB_0,     GPIOB_12,    0x0,   0),
};

static struct meson_bank meson_s5_periphs_banks[] = {
	/* name  first  last  irq  pullen  pull  dir  out  in ds */
	BANK_DS("TEST_N", GPIO_TEST_N,    GPIO_TEST_N,   -1, -1,
		0x03,  0,  0x04,  0,  0x02, 0,  0x01, 0,  0x00, 0, 0x07, 0),
	BANK_DS("A", GPIOA_0,    GPIOA_10,  13, 23,
		0x13,  0,  0x14,  0,  0x12, 0,  0x11, 0,  0x10, 0, 0x17, 0),
	BANK_DS("C", GPIOC_0,    GPIOC_7,   24, 31,
		0x23,  0,  0x24,  0,  0x22, 0,  0x21, 0,  0x20, 0, 0x27, 0),
	BANK_DS("D", GPIOD_0,    GPIOD_11,  32, 43,
		0x33,  0,  0x34,  0,  0x32, 0,  0x31, 0,  0x30, 0, 0x37, 0),
	BANK_DS("E", GPIOE_0,    GPIOE_4,   44, 48,
		0x43,  0,  0x44,  0,  0x42, 0,  0x41, 0,  0x40, 0, 0x47, 0),
	BANK_DS("H", GPIOH_0,    GPIOH_8,   49, 57,
		0x53,  0,  0x54,  0,  0x52, 0,  0x51, 0,  0x50, 0, 0x57, 0),
	BANK_DS("T", GPIOT_0,    GPIOT_24,  58, 82,
		0x73,  0,  0x74,  0,  0x72, 0,  0x71, 0,  0x70, 0, 0x77, 0),
	BANK_DS("X", GPIOX_0,    GPIOX_19,  83, 102,
		0x83,  0,  0x84,  0,  0x82, 0,  0x81, 0,  0x80, 0, 0x87, 0),
	BANK_DS("Z", GPIOZ_0,    GPIOZ_15,  103, 118,
		0x93,  0,  0x94,  0,  0x92, 0,  0x91, 0,  0x90, 0, 0x97, 0),
};

static struct meson_pmx_bank meson_s5_periphs_pmx_banks[] = {
	/*name	            first	 lask        reg offset*/
	BANK_PMX("TEST_N", GPIO_TEST_N, GPIO_TEST_N, 0xf,  0),
	BANK_PMX("A",      GPIOA_0,     GPIOA_10,    0x00, 0),
	BANK_PMX("C",      GPIOC_0,     GPIOC_7,     0x3,  0),
	BANK_PMX("D",      GPIOD_0,     GPIOD_11,    0x5,  0),
	BANK_PMX("E",      GPIOE_0,     GPIOE_4,     0x7,  0),
	BANK_PMX("H",      GPIOH_0,     GPIOH_8,     0x8,  0),
	BANK_PMX("T",      GPIOT_0,     GPIOT_24,    0xa,  0),
	BANK_PMX("X",      GPIOX_0,     GPIOX_19,    0xe,  0),
	BANK_PMX("Z",      GPIOZ_0,     GPIOZ_15,    0x11,  0),
};

static struct meson_axg_pmx_data meson_s5_storage_pmx_banks_data = {
	.pmx_banks	= meson_s5_storage_pmx_banks,
	.num_pmx_banks	= ARRAY_SIZE(meson_s5_storage_pmx_banks),
};

static struct meson_axg_pmx_data meson_s5_periphs_pmx_banks_data = {
	.pmx_banks	= meson_s5_periphs_pmx_banks,
	.num_pmx_banks	= ARRAY_SIZE(meson_s5_periphs_pmx_banks),
};

static struct meson_pinctrl_data meson_s5_storage_pinctrl_data = {
	.name		= "storage-banks",
	.pins		= meson_s5_storage_pins,
	.groups		= meson_s5_storage_groups,
	.funcs		= meson_s5_storage_functions,
	.banks		= meson_s5_storage_banks,
	.num_pins	= ARRAY_SIZE(meson_s5_storage_pins),
	.num_groups	= ARRAY_SIZE(meson_s5_storage_groups),
	.num_funcs	= ARRAY_SIZE(meson_s5_storage_functions),
	.num_banks	= ARRAY_SIZE(meson_s5_storage_banks),
	.pmx_ops	= &meson_axg_pmx_ops,
	.pmx_data	= &meson_s5_storage_pmx_banks_data,
	.parse_dt	= &meson_a1_parse_dt_extra,
};

static struct meson_pinctrl_data meson_s5_periphs_pinctrl_data = {
	.name		= "periphs-banks",
	.pins		= meson_s5_periphs_pins,
	.groups		= meson_s5_periphs_groups,
	.funcs		= meson_s5_periphs_functions,
	.banks		= meson_s5_periphs_banks,
	.num_pins	= ARRAY_SIZE(meson_s5_periphs_pins),
	.num_groups	= ARRAY_SIZE(meson_s5_periphs_groups),
	.num_funcs	= ARRAY_SIZE(meson_s5_periphs_functions),
	.num_banks	= ARRAY_SIZE(meson_s5_periphs_banks),
	.pmx_ops	= &meson_axg_pmx_ops,
	.pmx_data	= &meson_s5_periphs_pmx_banks_data,
	.parse_dt	= &meson_a1_parse_dt_extra,
};

static const struct of_device_id meson_s5_pinctrl_dt_match[] = {
	{
		.compatible = "amlogic,meson-s5-periphs-pinctrl",
		.data = &meson_s5_periphs_pinctrl_data,
	},
	{
		.compatible = "amlogic,meson-s5-storage-pinctrl",
		.data = &meson_s5_storage_pinctrl_data,
	},
	{ },
};

static struct platform_driver meson_s5_pinctrl_driver = {
	.probe  = meson_pinctrl_probe,
	.driver = {
		.name	= "meson-s5-pinctrl",
	},
};

#ifndef MODULE
static int __init s5_pmx_init(void)
{
	meson_s5_pinctrl_driver.driver.of_match_table =
			meson_s5_pinctrl_dt_match;
	return platform_driver_register(&meson_s5_pinctrl_driver);
}
arch_initcall(s5_pmx_init);

#else
int __init meson_s5_pinctrl_init(void)
{
	meson_s5_pinctrl_driver.driver.of_match_table =
			meson_s5_pinctrl_dt_match;
	return platform_driver_register(&meson_s5_pinctrl_driver);
}
#endif
