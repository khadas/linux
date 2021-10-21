// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <dt-bindings/gpio/meson-a5-gpio.h>
#include "pinctrl-meson.h"
#include "pinctrl-meson-axg-pmx.h"
#include "pinctrl-module-init.h"

static const struct pinctrl_pin_desc meson_a5_periphs_pins[] = {
	MESON_PIN(GPIOE_0),
	MESON_PIN(GPIOE_1),

	MESON_PIN(GPIOH_0),
	MESON_PIN(GPIOH_1),
	MESON_PIN(GPIOH_2),
	MESON_PIN(GPIOH_3),
	MESON_PIN(GPIOH_4),

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
	MESON_PIN(GPIOD_12),
	MESON_PIN(GPIOD_13),
	MESON_PIN(GPIOD_14),
	MESON_PIN(GPIOD_15),

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

	MESON_PIN(GPIO_TEST_N)
};

/* BANK E func1 */
static const unsigned int pwm_e_e_pins[]		= { GPIOE_0 };
static const unsigned int pwm_f_e_pins[]		= { GPIOE_1 };

/* BANK E func2 */
static const unsigned int i2c_slave_sda_e_pins[]	= { GPIOE_0 };
static const unsigned int i2c_slave_scl_e_pins[]	= { GPIOE_1 };

/* BANK E func3 */
static const unsigned int uart_b_tx_e_pins[]		= { GPIOE_0 };
static const unsigned int uart_b_rx_e_pins[]		= { GPIOE_1 };

/* BANK E func5 */
static const unsigned int i2c0_sda_e_pins[]		= { GPIOE_0 };
static const unsigned int i2c0_scl_e_pins[]		= { GPIOE_1 };

/* Bank H func1 */
static const unsigned int pdm_din1_h_pins[]		= { GPIOH_0 };
static const unsigned int pdm_din0_h_pins[]		= { GPIOH_1 };
static const unsigned int pdm_dclk_h_pins[]		= { GPIOH_2 };
static const unsigned int pdm_din3_h_pins[]		= { GPIOH_3 };
static const unsigned int pdm_din2_h_pins[]		= { GPIOH_4 };

/* Bank H func2 */
static const unsigned int spi_a_miso_h_pins[]		= { GPIOH_0 };
static const unsigned int spi_a_mosi_h_pins[]		= { GPIOH_1 };
static const unsigned int spi_a_clk_h_pins[]		= { GPIOH_2 };
static const unsigned int spi_a_ss0_h_pins[]		= { GPIOH_3 };

/* Bank H func3 */
static const unsigned int uart_a_cts_h_pins[]		= { GPIOH_1 };
static const unsigned int uart_a_rts_h_pins[]		= { GPIOH_2 };
static const unsigned int uart_a_tx_h_pins[]		= { GPIOH_3 };
static const unsigned int uart_a_rx_h_pins[]		= { GPIOH_4 };

/* Bank H func4 */
static const unsigned int tdm_d1_h_pins[]		= { GPIOH_0 };
static const unsigned int tdm_d0_h_pins[]		= { GPIOH_1 };
static const unsigned int mclk_0_h_pins[]		= { GPIOH_2 };
static const unsigned int tdm_sclk0_h_pins[]		= { GPIOH_3 };
static const unsigned int tdm_fs0_h_pins[]		= { GPIOH_4 };

/* Bank H func5 */
static const unsigned int i2c1_sda_h_pins[]		= { GPIOH_0 };
static const unsigned int i2c1_scl_h_pins[]		= { GPIOH_1 };
static const unsigned int i2c2_sda_h_pins[]		= { GPIOH_3 };
static const unsigned int i2c2_scl_h_pins[]		= { GPIOH_4 };

/* Bank H func6 */
static const unsigned int gen_clk_h_pins[]		= { GPIOH_4 };

/* Bank D func1 */
static const unsigned int uart_b_tx_d_pins[]		= { GPIOD_0 };
static const unsigned int uart_b_rx_d_pins[]		= { GPIOD_1 };
static const unsigned int uart_b_cts_d_pins[]		= { GPIOD_2 };
static const unsigned int uart_b_rts_d_pins[]		= { GPIOD_3 };
static const unsigned int remote_out_pins[]		= { GPIOD_4 };
static const unsigned int remote_in_pins[]		= { GPIOD_5 };
static const unsigned int jtag_clk_pins[]		= { GPIOD_6 };
static const unsigned int jtag_tms_pins[]		= { GPIOD_7 };
static const unsigned int jtag_tdi_pins[]		= { GPIOD_8 };
static const unsigned int jtag_tdo_pins[]		= { GPIOD_9 };
static const unsigned int clk12_24_pins[]		= { GPIOD_10 };
static const unsigned int tdm_d3_d_pins[]		= { GPIOD_11 };
static const unsigned int tdm_d2_d_pins[]		= { GPIOD_12 };
static const unsigned int mclk_1_d_pins[]		= { GPIOD_13 };
static const unsigned int tdm_sclk1_d_pins[]		= { GPIOD_14 };
static const unsigned int tdm_fs1_d_pins[]		= { GPIOD_15 };

/* Bank D func2 */
static const unsigned int i2c0_sda_d_pins[]		= { GPIOD_2 };
static const unsigned int i2c0_scl_d_pins[]		= { GPIOD_3 };
static const unsigned int pwm_a_hiz_d_pins[]		= { GPIOD_6 };
static const unsigned int pwm_b_hiz_d_pins[]		= { GPIOD_7 };
static const unsigned int pwm_c_hiz_d_pins[]		= { GPIOD_8 };
static const unsigned int pwm_d_hiz_d_pins[]		= { GPIOD_9 };
static const unsigned int gen_clk_d_pins[]		= { GPIOD_10 };
static const unsigned int pwm_e_hiz_d_pins[]		= { GPIOD_11 };
static const unsigned int i2c3_sda_d_pins[]		= { GPIOD_12 };
static const unsigned int i2c3_scl_d_pins[]		= { GPIOD_13 };
static const unsigned int i2c2_sda_d_pins[]		= { GPIOD_14 };
static const unsigned int i2c2_scl_d_pins[]		= { GPIOD_15 };

/* Bank D func3 */
static const unsigned int uart_c_tx_d_pins[]		= { GPIOD_2 };
static const unsigned int uart_c_rx_d_pins[]		= { GPIOD_3 };
static const unsigned int pwm_a_d_pins[]		= { GPIOD_6 };
static const unsigned int pwm_b_d_pins[]		= { GPIOD_7 };
static const unsigned int pwm_c_d_pins[]		= { GPIOD_8 };
static const unsigned int pwm_d_d_pins[]		= { GPIOD_9 };
static const unsigned int pwm_e_d_pins[]		= { GPIOD_11 };

/* Bank D func4 */
static const unsigned int clk_32k_in_pins[]		= { GPIOD_2 };
static const unsigned int i2c1_sda_d_pins[]		= { GPIOD_10 };
static const unsigned int i2c1_scl_d_pins[]		= { GPIOD_11 };

/* Bank D func5 */
static const unsigned int mic_mute_en_pins[]		= { GPIOD_2 };
static const unsigned int mic_mute_key_pins[]		= { GPIOD_3 };

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
static const unsigned int nand_wen_clk_pins[]		= { GPIOB_8 };
static const unsigned int nand_ale_pins[]		= { GPIOB_9 };
static const unsigned int nand_ren_wr_pins[]		= { GPIOB_10 };
static const unsigned int nand_cle_pins[]		= { GPIOB_11 };
static const unsigned int nand_ce0_pins[]		= { GPIOB_12 };

/* Bank B func3 */
static const unsigned int spif_hold_pins[]		= { GPIOB_3 };
static const unsigned int spif_mo_pins[]		= { GPIOB_4 };
static const unsigned int spif_mi_pins[]		= { GPIOB_5 };
static const unsigned int spif_clk_pins[]		= { GPIOB_6 };
static const unsigned int spif_wp_pins[]		= { GPIOB_7 };
static const unsigned int spif_cs_pins[]		= { GPIOB_13 };

/* Bank X func1 */
static const unsigned int sdio_d0_pins[]		= { GPIOX_0 };
static const unsigned int sdio_d1_pins[]		= { GPIOX_1 };
static const unsigned int sdio_d2_pins[]		= { GPIOX_2 };
static const unsigned int sdio_d3_pins[]		= { GPIOX_3 };
static const unsigned int sdio_clk_pins[]		= { GPIOX_4 };
static const unsigned int sdio_cmd_pins[]		= { GPIOX_5 };
static const unsigned int pwm_a_x_pins[]		= { GPIOX_6 };
static const unsigned int mclk_0_x_pins[]		= { GPIOX_7 };
static const unsigned int tdm_d1_x_pins[]		= { GPIOX_8 };
static const unsigned int tdm_d0_x_pins[]		= { GPIOX_9 };
static const unsigned int tdm_fs0_x_pins[]		= { GPIOX_10 };
static const unsigned int tdm_sclk0_x_pins[]		= { GPIOX_11 };
static const unsigned int uart_a_tx_x_pins[]		= { GPIOX_12 };
static const unsigned int uart_a_rx_x_pins[]		= { GPIOX_13 };
static const unsigned int uart_a_cts_x_pins[]		= { GPIOX_14 };
static const unsigned int uart_a_rts_x_pins[]		= { GPIOX_15 };
static const unsigned int pwm_g_x_pins[]		= { GPIOX_16 };
static const unsigned int i2c0_sda_x_pins[]		= { GPIOX_17 };
static const unsigned int i2c0_scl_x_pins[]		= { GPIOX_18 };
static const unsigned int pwm_b_x_pins[]		= { GPIOX_19 };

/* Bank X func2 */
static const unsigned int spi_a_mosi_x_pins[]		= { GPIOX_8 };
static const unsigned int spi_a_miso_x_pins[]		= { GPIOX_9 };
static const unsigned int spi_a_ss0_x_pins[]		= { GPIOX_10 };
static const unsigned int spi_a_clk_x_pins[]		= { GPIOX_11 };

/* Bank X func3 */
static const unsigned int gen_clk_x_pins[]		= { GPIOX_6 };
static const unsigned int i2c_slave_sda_x_pins[]	= { GPIOX_10 };
static const unsigned int i2c_slave_scl_x_pins[]	= { GPIOX_11 };

/* Bank C func1 */
static const unsigned int tdm_d2_c_pins[]		= { GPIOC_0 };
static const unsigned int tdm_d3_c_pins[]		= { GPIOC_1 };
static const unsigned int tdm_fs1_c_pins[]		= { GPIOC_2 };
static const unsigned int tdm_sclk1_c_pins[]		= { GPIOC_3 };
static const unsigned int mclk_1_c_pins[]		= { GPIOC_4 };
static const unsigned int tdm_d4_c_pins[]		= { GPIOC_5 };
static const unsigned int tdm_d5_c_pins[]		= { GPIOC_6 };
static const unsigned int tdm_d6_c_pins[]		= { GPIOC_7 };
static const unsigned int tdm_d7_c_pins[]		= { GPIOC_8 };
static const unsigned int spdif_out_c_pins[]		= { GPIOC_9 };
static const unsigned int spdif_in_c10_pins[]		= { GPIOC_10 };

/* Bank C func2 */
static const unsigned int i2c2_sda_c_pins[]		= { GPIOC_0 };
static const unsigned int i2c2_scl_c_pins[]		= { GPIOC_1 };
static const unsigned int uart_d_tx_c_pins[]		= { GPIOC_7 };
static const unsigned int uart_d_rx_c_pins[]		= { GPIOC_8 };
static const unsigned int uart_d_cts_c_pins[]		= { GPIOC_9 };
static const unsigned int uart_d_rts_c_pins[]		= { GPIOC_10 };

/* Bank C func3 */
static const unsigned int pdm_din1_c_pins[]		= { GPIOC_0 };
static const unsigned int pdm_din0_c_pins[]		= { GPIOC_1 };
static const unsigned int pdm_dclk_c_pins[]		= { GPIOC_3 };
static const unsigned int pdm_din3_c_pins[]		= { GPIOC_4 };
static const unsigned int pdm_din2_c_pins[]		= { GPIOC_5 };

/* Bank C func4 */
static const unsigned int i2c3_sda_c_pins[]		= { GPIOC_7 };
static const unsigned int i2c3_scl_c_pins[]		= { GPIOC_8 };
static const unsigned int spdif_in_c9_pins[]		= { GPIOC_9 };

/* Bank C func5 */
static const unsigned int gen_clk_c_pins[]		= { GPIOC_0 };

/* Bank T func1 */
static const unsigned int tdm_fs2_t_pins[]		= { GPIOT_0 };
static const unsigned int tdm_sclk2_t_pins[]		= { GPIOT_1 };
static const unsigned int tdm_d8_t_pins[]		= { GPIOT_2 };
static const unsigned int tdm_d9_t_pins[]		= { GPIOT_3 };
static const unsigned int tdm_d10_t_pins[]		= { GPIOT_4 };
static const unsigned int tdm_d11_t_pins[]		= { GPIOT_5 };
static const unsigned int mclk_2_t_pins[]		= { GPIOT_6 };
static const unsigned int tdm_d12_t_pins[]		= { GPIOT_7 };
static const unsigned int tdm_d13_t_pins[]		= { GPIOT_8 };
static const unsigned int i2c0_sda_t_pins[]		= { GPIOT_10 };
static const unsigned int i2c0_scl_t_pins[]		= { GPIOT_11 };
static const unsigned int i2c_slave_sda_t_pins[]	= { GPIOT_12 };
static const unsigned int i2c_slave_scl_t_pins[]	= { GPIOT_13 };

/* Bank T func2 */
static const unsigned int spi_a_clk_t_pins[]		= { GPIOT_1 };
static const unsigned int spi_a_mosi_t_pins[]		= { GPIOT_2 };
static const unsigned int spi_a_miso_t_pins[]		= { GPIOT_3 };
static const unsigned int spi_a_ss0_t_pins[]		= { GPIOT_4 };
static const unsigned int spi_a_ss1_t_pins[]		= { GPIOT_5 };
static const unsigned int spi_a_ss2_t_pins[]		= { GPIOT_6 };
static const unsigned int pwm_h_t_pins[]		= { GPIOT_8 };
static const unsigned int uart_c_tx_t_pins[]		= { GPIOT_10 };
static const unsigned int uart_c_rx_t_pins[]		= { GPIOT_11 };
static const unsigned int uart_c_cts_t_pins[]		= { GPIOT_12 };
static const unsigned int uart_c_rts_t_pins[]		= { GPIOT_13 };

/* Bank T func3 */
static const unsigned int uart_e_tx_t_pins[]		= { GPIOT_3 };
static const unsigned int uart_e_rx_t_pins[]		= { GPIOT_4 };
static const unsigned int uart_e_cts_t_pins[]		= { GPIOT_5 };
static const unsigned int uart_e_rts_t_pins[]		= { GPIOT_6 };
static const unsigned int spi_b_ss2_t_pins[]		= { GPIOT_8 };
static const unsigned int spi_b_ss1_t_pins[]		= { GPIOT_9 };
static const unsigned int spi_b_ss0_t_pins[]		= { GPIOT_10 };
static const unsigned int spi_b_clk_t_pins[]		= { GPIOT_11 };
static const unsigned int spi_b_mosi_t_pins[]		= { GPIOT_12 };
static const unsigned int spi_b_miso_t_pins[]		= { GPIOT_13 };

/* Bank T func4 */
static const unsigned int pwm_d_t_pins[]		= { GPIOT_4 };
static const unsigned int pwm_c_t_pins[]		= { GPIOT_5 };
static const unsigned int i2c1_sda_t_pins[]		= { GPIOT_8 };
static const unsigned int i2c1_scl_t_pins[]		= { GPIOT_9 };
static const unsigned int gen_clk_t_pins[]		= { GPIOT_12 };

/* BANK Z func1 */
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
static const unsigned int i2c0_sda_z_pins[]		= { GPIOZ_14 };
static const unsigned int i2c0_scl_z_pins[]		= { GPIOZ_15 };

/* BANK Z func2 */
static const unsigned int tdm_d10_z_pins[]		= { GPIOZ_4 };
static const unsigned int tdm_d11_z_pins[]		= { GPIOZ_5 };
static const unsigned int tdm_fs2_z_pins[]		= { GPIOZ_6 };
static const unsigned int tdm_sclk2_z_pins[]		= { GPIOZ_7 };
static const unsigned int mclk_2_z_pins[]		= { GPIOZ_8 };
static const unsigned int tdm_d12_z_pins[]		= { GPIOZ_12 };
static const unsigned int tdm_d13_z_pins[]		= { GPIOZ_13 };

/* BANK Z func3 */
static const unsigned int uart_c_tx_z_pins[]		= { GPIOZ_4 };
static const unsigned int uart_c_rx_z_pins[]		= { GPIOZ_5 };
static const unsigned int uart_c_cts_z_pins[]		= { GPIOZ_6 };
static const unsigned int uart_c_rts_z_pins[]		= { GPIOZ_7 };
static const unsigned int pwm_b_z_pins[]		= { GPIOZ_11 };
static const unsigned int pwm_a_z_pins[]		= { GPIOZ_12 };
static const unsigned int pwm_c_z_pins[]		= { GPIOZ_13 };

/* BANK Z func4 */
static const unsigned int tdm_fs0_z_pins[]		= { GPIOZ_6 };
static const unsigned int tdm_sclk0_z_pins[]		= { GPIOZ_7 };
static const unsigned int mclk_0_z_pins[]		= { GPIOZ_8 };
static const unsigned int pwm_b_hiz_z_pins[]		= { GPIOD_11 };
static const unsigned int pwm_a_hiz_z_pins[]		= { GPIOD_12 };
static const unsigned int pwm_c_hiz_z_pins[]		= { GPIOD_13 };

/* BANK Z func6 */
static const unsigned int gen_clk_z_pins[]		= { GPIOZ_13 };

static struct meson_pmx_group meson_a5_periphs_groups[] __initdata = {
	GPIO_GROUP(GPIOE_0),
	GPIO_GROUP(GPIOE_1),

	GPIO_GROUP(GPIOH_0),
	GPIO_GROUP(GPIOH_1),
	GPIO_GROUP(GPIOH_2),
	GPIO_GROUP(GPIOH_3),
	GPIO_GROUP(GPIOH_4),

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
	GPIO_GROUP(GPIOD_12),
	GPIO_GROUP(GPIOD_13),
	GPIO_GROUP(GPIOD_14),
	GPIO_GROUP(GPIOD_15),

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

	GPIO_GROUP(GPIO_TEST_N),

	/* BANK E func1 */
	GROUP(pwm_e_e,			1),
	GROUP(pwm_f_e,			1),

	/* BANK E func2 */
	GROUP(i2c_slave_sda_e,		2),
	GROUP(i2c_slave_scl_e,		2),

	/* BANK E func3 */
	GROUP(uart_b_tx_e,		3),
	GROUP(uart_b_rx_e,		3),

	/* BANK E func5 */
	GROUP(i2c0_sda_e,		5),
	GROUP(i2c0_scl_e,		5),

	/* Bank H func1 */
	GROUP(pdm_din1_h,		1),
	GROUP(pdm_din0_h,		1),
	GROUP(pdm_dclk_h,		1),
	GROUP(pdm_din3_h,		1),
	GROUP(pdm_din2_h,		1),

	/* Bank H func2 */
	GROUP(spi_a_miso_h,		2),
	GROUP(spi_a_mosi_h,		2),
	GROUP(spi_a_clk_h,		2),
	GROUP(spi_a_ss0_h,		2),

	/* Bank H func3 */
	GROUP(uart_a_cts_h,		3),
	GROUP(uart_a_rts_h,		3),
	GROUP(uart_a_tx_h,		3),
	GROUP(uart_a_rx_h,		3),

	/* Bank H func4 */
	GROUP(tdm_d1_h,			4),
	GROUP(tdm_d0_h,			4),
	GROUP(mclk_0_h,			4),
	GROUP(tdm_sclk0_h,		4),
	GROUP(tdm_fs0_h,		4),

	/* Bank H func5 */
	GROUP(i2c1_sda_h,		5),
	GROUP(i2c1_scl_h,		5),
	GROUP(i2c2_sda_h,		5),
	GROUP(i2c2_scl_h,		5),

	/* Bank H func6 */
	GROUP(gen_clk_h,		6),

	/* Bank D func1 */
	GROUP(uart_b_tx_d,		1),
	GROUP(uart_b_rx_d,		1),
	GROUP(uart_b_cts_d,		1),
	GROUP(uart_b_rts_d,		1),
	GROUP(remote_out,		1),
	GROUP(remote_in,		1),
	GROUP(jtag_clk,			1),
	GROUP(jtag_tms,			1),
	GROUP(jtag_tdi,			1),
	GROUP(jtag_tdo,			1),
	GROUP(clk12_24,			1),
	GROUP(tdm_d3_d,			1),
	GROUP(tdm_d2_d,			1),
	GROUP(mclk_1_d,			1),
	GROUP(tdm_sclk1_d,		1),
	GROUP(tdm_fs1_d,		1),

	/* Bank D func2 */
	GROUP(i2c0_sda_d,		2),
	GROUP(i2c0_scl_d,		2),
	GROUP(pwm_a_hiz_d,		2),
	GROUP(pwm_b_hiz_d,		2),
	GROUP(pwm_c_hiz_d,		2),
	GROUP(pwm_d_hiz_d,		2),
	GROUP(gen_clk_d,		2),
	GROUP(pwm_e_hiz_d,		2),
	GROUP(i2c3_sda_d,		2),
	GROUP(i2c3_scl_d,		2),
	GROUP(i2c2_sda_d,		2),
	GROUP(i2c2_scl_d,		2),

	/* Bank D func3 */
	GROUP(uart_c_tx_d,		3),
	GROUP(uart_c_rx_d,		3),
	GROUP(pwm_a_d,			3),
	GROUP(pwm_b_d,			3),
	GROUP(pwm_c_d,			3),
	GROUP(pwm_d_d,			3),
	GROUP(pwm_e_d,			3),

	/* Bank D func4 */
	GROUP(clk_32k_in,		4),
	GROUP(i2c1_sda_d,		4),
	GROUP(i2c1_scl_d,		4),

	/* Bank D func5 */
	GROUP(mic_mute_en,		5),
	GROUP(mic_mute_key,		5),

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

	/* Bank B func2 */
	GROUP(nand_wen_clk,		2),
	GROUP(nand_ale,			2),
	GROUP(nand_ren_wr,		2),
	GROUP(nand_cle,			2),
	GROUP(nand_ce0,			2),

	/* Bank B func3 */
	GROUP(spif_hold,		3),
	GROUP(spif_mo,			3),
	GROUP(spif_mi,			3),
	GROUP(spif_clk,			3),
	GROUP(spif_wp,			3),
	GROUP(spif_cs,			3),

	/* Bank X func1 */
	GROUP(sdio_d0,			1),
	GROUP(sdio_d1,			1),
	GROUP(sdio_d2,			1),
	GROUP(sdio_d3,			1),
	GROUP(sdio_clk,			1),
	GROUP(sdio_cmd,			1),
	GROUP(pwm_a_x,			1),
	GROUP(mclk_0_x,			1),
	GROUP(tdm_d1_x,			1),
	GROUP(tdm_d0_x,			1),
	GROUP(tdm_fs0_x,		1),
	GROUP(tdm_sclk0_x,		1),
	GROUP(uart_a_tx_x,		1),
	GROUP(uart_a_rx_x,		1),
	GROUP(uart_a_cts_x,		1),
	GROUP(uart_a_rts_x,		1),
	GROUP(pwm_g_x,			1),
	GROUP(i2c0_sda_x,		1),
	GROUP(i2c0_scl_x,		1),
	GROUP(pwm_b_x,			1),

	/* Bank X func2 */
	GROUP(spi_a_mosi_x,		2),
	GROUP(spi_a_miso_x,		2),
	GROUP(spi_a_ss0_x,		2),
	GROUP(spi_a_clk_x,		2),

	/* Bank X func3 */
	GROUP(gen_clk_x,		3),
	GROUP(i2c_slave_sda_x,		3),
	GROUP(i2c_slave_scl_x,		3),

	/* Bank C func1 */
	GROUP(tdm_d2_c,			1),
	GROUP(tdm_d3_c,			1),
	GROUP(tdm_fs1_c,		1),
	GROUP(tdm_sclk1_c,		1),
	GROUP(mclk_1_c,			1),
	GROUP(tdm_d4_c,			1),
	GROUP(tdm_d5_c,			1),
	GROUP(tdm_d6_c,			1),
	GROUP(tdm_d7_c,			1),
	GROUP(spdif_out_c,		1),
	GROUP(spdif_in_c10,		1),

	/* Bank C func2 */
	GROUP(i2c2_sda_c,		2),
	GROUP(i2c2_scl_c,		2),
	GROUP(uart_d_tx_c,		2),
	GROUP(uart_d_rx_c,		2),
	GROUP(uart_d_cts_c,		2),
	GROUP(uart_d_rts_c,		2),

	/* Bank C func3 */
	GROUP(pdm_din1_c,		3),
	GROUP(pdm_din0_c,		3),
	GROUP(pdm_dclk_c,		3),
	GROUP(pdm_din3_c,		3),
	GROUP(pdm_din2_c,		3),

	/* Bank C func4 */
	GROUP(i2c3_sda_c,		4),
	GROUP(i2c3_scl_c,		4),
	GROUP(spdif_in_c9,		4),

	/* Bank C func5 */
	GROUP(gen_clk_c,		5),

	/* Bank T func1 */
	GROUP(tdm_fs2_t,		1),
	GROUP(tdm_sclk2_t,		1),
	GROUP(tdm_d8_t,			1),
	GROUP(tdm_d9_t,			1),
	GROUP(tdm_d10_t,		1),
	GROUP(tdm_d11_t,		1),
	GROUP(mclk_2_t,			1),
	GROUP(tdm_d12_t,		1),
	GROUP(tdm_d13_t,		1),
	GROUP(i2c0_sda_t,		1),
	GROUP(i2c0_scl_t,		1),
	GROUP(i2c_slave_sda_t,		1),
	GROUP(i2c_slave_scl_t,		1),

	/* Bank T func2 */
	GROUP(spi_a_clk_t,		2),
	GROUP(spi_a_mosi_t,		2),
	GROUP(spi_a_miso_t,		2),
	GROUP(spi_a_ss0_t,		2),
	GROUP(spi_a_ss1_t,		2),
	GROUP(spi_a_ss2_t,		2),
	GROUP(pwm_h_t,			2),
	GROUP(uart_c_tx_t,		2),
	GROUP(uart_c_rx_t,		2),
	GROUP(uart_c_cts_t,		2),
	GROUP(uart_c_rts_t,		2),

	/* Bank T func3 */
	GROUP(uart_e_tx_t,		3),
	GROUP(uart_e_rx_t,		3),
	GROUP(uart_e_cts_t,		3),
	GROUP(uart_e_rts_t,		3),
	GROUP(spi_b_ss2_t,		3),
	GROUP(spi_b_ss1_t,		3),
	GROUP(spi_b_ss0_t,		3),
	GROUP(spi_b_clk_t,		3),
	GROUP(spi_b_mosi_t,		3),
	GROUP(spi_b_miso_t,		3),

	/* Bank T func4 */
	GROUP(pwm_d_t,			4),
	GROUP(pwm_c_t,			4),
	GROUP(i2c1_sda_t,		4),
	GROUP(i2c1_scl_t,		4),
	GROUP(gen_clk_t,		4),

	/* BANK Z func1 */
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
	GROUP(i2c0_sda_z,		1),
	GROUP(i2c0_scl_z,		1),

	/* BANK Z func2 */
	GROUP(tdm_d10_z,		2),
	GROUP(tdm_d11_z,		2),
	GROUP(tdm_fs2_z,		2),
	GROUP(tdm_sclk2_z,		2),
	GROUP(mclk_2_z,			2),
	GROUP(tdm_d12_z,		2),
	GROUP(tdm_d13_z,		2),

	/* BANK Z func3 */
	GROUP(uart_c_tx_z,		3),
	GROUP(uart_c_rx_z,		3),
	GROUP(uart_c_cts_z,		3),
	GROUP(uart_c_rts_z,		3),
	GROUP(pwm_b_z,			3),
	GROUP(pwm_a_z,			3),
	GROUP(pwm_c_z,			3),

	/* BANK Z func4 */
	GROUP(tdm_fs0_z,		4),
	GROUP(tdm_sclk0_z,		4),
	GROUP(mclk_0_z,			4),
	GROUP(pwm_b_hiz_z,		4),
	GROUP(pwm_a_hiz_z,		4),
	GROUP(pwm_c_hiz_z,		4),

	/* BANK Z func6 */
	GROUP(gen_clk_z,		6)
};

static const char * const gpio_periphs_groups[] = {
	"GPIOE_0", "GPIOE_1",

	"GPIOH_0", "GPIOH_1", "GPIOH_2", "GPIOH_3", "GPIOH_4",

	"GPIOD_0", "GPIOD_1", "GPIOD_2", "GPIOD_3", "GPIOD_4", "GPIOD_5",
	"GPIOD_6", "GPIOD_7", "GPIOD_8", "GPIOD_9", "GPIOD_10", "GPIOD_11",
	"GPIOD_12", "GPIOD_13", "GPIOD_14", "GPIOD_15",

	"GPIOB_0", "GPIOB_1", "GPIOB_2", "GPIOB_3", "GPIOB_4", "GPIOB_5",
	"GPIOB_6", "GPIOB_7", "GPIOB_8", "GPIOB_9", "GPIOB_10", "GPIOB_11",
	"GPIOB_12", "GPIOB_13",

	"GPIOX_0", "GPIOX_1", "GPIOX_2", "GPIOX_3", "GPIOX_4", "GPIOX_5",
	"GPIOX_6", "GPIOX_7", "GPIOX_8", "GPIOX_9", "GPIOX_10", "GPIOX_11",
	"GPIOX_12", "GPIOX_13", "GPIOX_14", "GPIOX_15", "GPIOX_16", "GPIOX_17",
	"GPIOX_18", "GPIOX_19",

	"GPIOC_0", "GPIOC_1", "GPIOC_2", "GPIOC_3", "GPIOC_4", "GPIOC_5",
	"GPIOC_6", "GPIOC_7", "GPIOC_8", "GPIOC_9", "GPIOC_10",

	"GPIOT_0", "GPIOT_1", "GPIOT_2", "GPIOT_3", "GPIOT_4", "GPIOT_5",
	"GPIOT_6", "GPIOT_7", "GPIOT_8", "GPIOT_9", "GPIOT_10", "GPIOT_11",
	"GPIOT_12", "GPIOT_13",

	"GPIOZ_0", "GPIOZ_1", "GPIOZ_2", "GPIOZ_3", "GPIOZ_4", "GPIOZ_5",
	"GPIOZ_6", "GPIOZ_7", "GPIOZ_8", "GPIOZ_9", "GPIOZ_10",
	"GPIOZ_11", "GPIOZ_12", "GPIOZ_13", "GPIOZ_14", "GPIOZ_15",

	"GPIO_TEST_N"
};

static const char * const i2c0_groups[] = {
	"i2c0_sda_e", "i2c0_scl_e",
	"i2c0_sda_d", "i2c0_scl_d",
	"i2c0_sda_x", "i2c0_scl_x",
	"i2c0_sda_t", "i2c0_scl_t",
	"i2c0_sda_z", "i2c0_scl_z"
};

static const char * const i2c1_groups[] = {
	"i2c1_sda_d", "i2c1_scl_d",
	"i2c1_sda_h", "i2c1_scl_h",
	"i2c1_sda_t", "i2c1_scl_t"
};

static const char * const i2c2_groups[] = {
	"i2c2_sda_d", "i2c2_scl_d",
	"i2c2_sda_h", "i2c2_scl_h",
	"i2c2_sda_c", "i2c2_scl_c"
};

static const char * const i2c3_groups[] = {
	"i2c3_sda_c", "i2c3_scl_c",
	"i2c3_sda_d", "i2c3_scl_d"
};

static const char * const uart_a_groups[] = {
	"uart_a_tx_x", "uart_a_rx_x", "uart_a_cts_x", "uart_a_rts_x",
	"uart_a_tx_h", "uart_a_rx_h", "uart_a_cts_h", "uart_a_rts"
};

static const char * const uart_b_groups[] = {
	"uart_b_tx_e", "uart_b_rx_e",
	"uart_b_tx_d", "uart_b_rx_d", "uart_b_cts_d", "uart_b_rts_d"
};

static const char * const uart_c_groups[] = {
	"uart_c_tx_d", "uart_c_rx_d",
	"uart_c_tx_t", "uart_c_rx_t", "uart_c_cts_t", "uart_c_rts_t",
	"uart_c_tx_z", "uart_c_rx_z", "uart_c_cts_z", "uart_c_rts_z"
};

static const char * const uart_d_groups[] = {
	"uart_d_tx_c", "uart_d_rx_c", "uart_d_cts_c", "uart_d_rts_c"
};

static const char * const uart_e_groups[] = {
	"uart_e_tx_t", "uart_e_rx_t", "uart_e_cts_t", "uart_e_rts_t"
};

static const char * const emmc_groups[] = {
	"emmc_nand_d0", "emmc_nand_d1", "emmc_nand_d2", "emmc_nand_d3",
	"emmc_nand_d4", "emmc_nand_d5", "emmc_nand_d6", "emmc_nand_d7",
	"emmc_clk", "emmc_rst", "emmc_cmd", "emmc_nand_ds"
};

static const char * const nand_groups[] = {
	"emmc_nand_d0", "emmc_nand_d1", "emmc_nand_d2", "emmc_nand_d3",
	"emmc_nand_d4", "emmc_nand_d5", "emmc_nand_d6", "emmc_nand_d7",
	"nand_wen_clk", "nand_ale", "nand_ren_wr", "nand_cle", "nand_ce0"
};

static const char * const spif_groups[] = {
	"spif_hold", "spif_mo", "spif_mi", "spif_clk", "spif_wp",
	"spif_cs"
};

static const char * const jtag_1_groups[] = {
	"jtag_clk", "jtag_tms", "jtag_tdi", "jtag_tdo"
};

static const char * const pdm_groups[] = {
	"pdm_din0_h", "pdm_din1_h", "pdm_din2_h", "pdm_din3_h", "pdm_dclk_h",
	"pdm_din0_c", "pdm_din1_c",  "pdm_din2_c", "pdm_din3_c", "pdm_dclk_c"
};

static const char * const tdm_groups[] = {
	"tdm_d1_h", "tdm_d0_h", "tdm_sclk0_h", "tdm_fs0_h", "tdm_d3_d",
	"tdm_d2_d", "tdm_sclk1_d", "tdm_fs1_d", "tdm_d1_x", "tdm_d0_x",
	"tdm_fs0_x", "tdm_sclk0_x", "tdm_d2_c", "tdm_d3_c", "tdm_fs1_c",
	"tdm_sclk1_c", "tdm_d4_c", "tdm_d5_c", "tdm_d6_c", "tdm_d7_c",
	"tdm_fs2_t", "tdm_sclk2_t", "tdm_d8_t", "tdm_d9_t", "tdm_d10_t",
	"tdm_d11_t", "tdm_d12_t", "tdm_d13_t", "tdm_d10_z", "tdm_d11_z",
	"tdm_fs2_z", "tdm_sclk2_z", "tdm_d12_z", "tdm_d13_z", "tdm_fs0_z",
	"tdm_sclk0_z"
};

static const char * const mclk_groups[] = {
	"mclk_0_h", "mclk_1_d", "mclk_0_x", "mclk_1_c", "mclk_2_t",
	"mclk_2_z", "mclk_0_z"
};

static const char * const remote_out_groups[] = {
	"remote_out"
};

static const char * const remote_in_groups[] = {
	"remote_in"
};

static const char * const clk12_24_groups[] = {
	"clk12_24"
};

static const char * const clk_32k_in_groups[] = {
	"clk_32k_in"
};

static const char * const pwm_a_hiz_groups[] = {
	"pwm_a_hiz_d", "pwm_a_hiz_z"
};

static const char * const pwm_b_hiz_groups[] = {
	"pwm_b_hiz_d", "pwm_b_hiz_z"
};

static const char * const pwm_c_hiz_groups[] = {
	"pwm_c_hiz_d", "pwm_c_hiz_z"
};

static const char * const pwm_d_hiz_groups[] = {
	"pwm_d_hiz_d"
};

static const char * const pwm_e_hiz_groups[] = {
	"pwm_e_hiz_d"
};

static const char * const pwm_a_groups[] = {
	"pwm_a_d", "pwm_a_x", "pwm_a_z"
};

static const char * const pwm_b_groups[] = {
	"pwm_b_d", "pwm_b_x", "pwm_b_z"
};

static const char * const pwm_c_groups[] = {
	"pwm_c_d", "pwm_c_t", "pwm_c_z"
};

static const char * const pwm_d_groups[] = {
	"pwm_d_d", "pwm_d_t"
};

static const char * const pwm_e_groups[] = {
	"pwm_e_e", "pwm_e_d"
};

static const char * const pwm_f_groups[] = {
	"pwm_f_e"
};

static const char * const pwm_g_groups[] = {
	"pwm_g_x"
};

static const char * const pwm_h_groups[] = {
	"pwm_h_t",
};

static const char * const mic_mute_groups[] = {
	"mic_mute_en", "mic_mute_key"
};

static const char * const spdif_out_groups[] = {
	"spdif_out_c"
};

static const char * const spdif_in_groups[] = {
	"spdif_in_c9", "spdif_in_c10"
};

static const char * const eth_groups[] = {
	"eth_mdio", "eth_mdc", "eth_rgmii_rx_clk", "eth_rx_dv", "eth_rxd0",
	"eth_rxd1", "eth_rxd2_rgmii", "eth_rxd3_rgmii", "eth_rgmii_tx_clk",
	"eth_txen", "eth_txd0", "eth_txd1", "eth_txd2_rgmii", "eth_txd3_rgmii"
};

static const char * const spi_a_groups[] = {
	"spi_a_miso_h", "spi_a_mosi_h", "spi_a_clk_h", "spi_a_ss0_h",

	"spi_a_mosi_x", "spi_a_miso_x", "spi_a_ss0_x", "spi_a_clk_x",

	"spi_a_clk_t", "spi_a_mosi_t", "spi_a_miso_t", "spi_a_ss0_t",
	"spi_a_ss1_t", "spi_a_ss2_t"
};

static const char * const spi_b_groups[] = {
	"spi_b_clk_t", "spi_b_mosi_t", "spi_b_miso_t", "spi_b_ss0_t",
	"spi_b_ss1_t", "spi_b_ss2_t"
};

static const char * const gen_clk_groups[] = {
	"gen_clk_h", "gen_clk_d", "gen_clk_x",
	"gen_clk_c", "gen_clk_t", "gen_clk_z"
};

static const char * const sdio_groups[] = {
	"sdio_d0", "sdio_d1", "sdio_d2", "sdio_d3", "sdio_clk", "sdio_cmd"
};

static const char * const i2c_slave_groups[] = {
	"i2c_slave_scl_e", "i2c_slave_sda_e",
	"i2c_slave_scl_x", "i2c_slave_sda_x",
	"i2c_slave_scl_t", "i2c_slave_sda_t"
};

static struct meson_pmx_func meson_a5_periphs_functions[] __initdata = {
	FUNCTION(gpio_periphs),
	FUNCTION(i2c0),
	FUNCTION(i2c1),
	FUNCTION(i2c2),
	FUNCTION(i2c3),
	FUNCTION(uart_a),
	FUNCTION(uart_b),
	FUNCTION(uart_c),
	FUNCTION(uart_d),
	FUNCTION(uart_e),
	FUNCTION(emmc),
	FUNCTION(nand),
	FUNCTION(spif),
	FUNCTION(jtag_1),
	FUNCTION(pdm),
	FUNCTION(tdm),
	FUNCTION(mclk),
	FUNCTION(remote_out),
	FUNCTION(remote_in),
	FUNCTION(clk12_24),
	FUNCTION(clk_32k_in),
	FUNCTION(pwm_a_hiz),
	FUNCTION(pwm_b_hiz),
	FUNCTION(pwm_c_hiz),
	FUNCTION(pwm_d_hiz),
	FUNCTION(pwm_e_hiz),
	FUNCTION(pwm_a),
	FUNCTION(pwm_b),
	FUNCTION(pwm_c),
	FUNCTION(pwm_d),
	FUNCTION(pwm_e),
	FUNCTION(pwm_f),
	FUNCTION(pwm_g),
	FUNCTION(pwm_h),
	FUNCTION(mic_mute),
	FUNCTION(spdif_out),
	FUNCTION(spdif_in),
	FUNCTION(eth),
	FUNCTION(spi_a),
	FUNCTION(spi_b),
	FUNCTION(gen_clk),
	FUNCTION(sdio),
	FUNCTION(i2c_slave)
};

static struct meson_bank meson_a5_periphs_banks[] = {
	/* name  first  last  irq  pullen  pull  dir  out  in  ds*/
	BANK_DS("B", GPIOB_0,    GPIOB_13,  0, 13,
		0x63,  0,  0x64,  0,  0x62, 0,  0x61, 0,  0x60, 0, 0x67, 0),
	BANK_DS("C", GPIOC_0,    GPIOC_10,   14, 24,
		0x53,  0,  0x54,  0,  0x52, 0,  0x51, 0,  0x50, 0, 0x57, 0),
	BANK_DS("E", GPIOE_0,    GPIOE_1,   25, 26,
		0x43,  0,  0x44,  0,  0x42, 0,  0x41, 0,  0x40, 0, 0x47, 0),
	BANK_DS("H", GPIOH_0,    GPIOH_4,  27, 31,
		0x73,  0,  0x74,  0,  0x72, 0,  0x71, 0,  0x70, 0, 0x77, 0),
	BANK_DS("D", GPIOD_0,    GPIOD_15,  32, 47,
		0x33,  0,  0x34,  0,  0x32, 0,  0x31, 0,  0x30, 0, 0x37, 0),
	BANK_DS("T", GPIOT_0,    GPIOT_13,  48, 61,
		0x23,  0,  0x24,  0,  0x22, 0,  0x21, 0,  0x20, 0, 0x27, 0),
	BANK_DS("X", GPIOX_0,    GPIOX_19,  62, 81,
		0x13,  0,  0x14,  0,  0x12, 0,  0x11, 0,  0x10, 0, 0x17, 0),
	BANK_DS("Z", GPIOZ_0,    GPIOZ_15,  82, 97,
		0x03,  0,  0x04,  0,  0x02, 0,  0x01, 0,  0x00, 0, 0x07, 0),
	BANK_DS("TEST_N", GPIO_TEST_N,    GPIO_TEST_N,   -1, -1,
		0x83,  0,  0x84,  0,  0x82, 0,  0x81,  0, 0x80, 0, 0x87, 0),
};

static struct meson_pmx_bank meson_a5_periphs_pmx_banks[] = {
	/*name	            first	 lask        reg offset*/
	BANK_PMX("B",      GPIOB_0,     GPIOB_13,    0x00, 0),
	BANK_PMX("C",      GPIOC_0,     GPIOC_10,    0x9,  0),
	BANK_PMX("E",      GPIOE_0,     GPIOE_1,     0x12, 0),
	BANK_PMX("H",      GPIOH_0,     GPIOH_4,     0x13, 0),
	BANK_PMX("D",      GPIOD_0,     GPIOD_15,    0x10, 0),
	BANK_PMX("T",      GPIOT_0,     GPIOT_13,    0xb,  0),
	BANK_PMX("X",      GPIOX_0,     GPIOX_19,    0x3,  0),
	BANK_PMX("Z",      GPIOZ_0,     GPIOZ_15,    0x6,  0),
	BANK_PMX("TEST_N", GPIO_TEST_N, GPIO_TEST_N, 0xf,  0),
};

static struct meson_axg_pmx_data meson_a5_periphs_pmx_banks_data = {
	.pmx_banks	= meson_a5_periphs_pmx_banks,
	.num_pmx_banks	= ARRAY_SIZE(meson_a5_periphs_pmx_banks),
};

static struct meson_pinctrl_data meson_a5_periphs_pinctrl_data __refdata = {
	.name		= "periphs-banks",
	.pins		= meson_a5_periphs_pins,
	.groups		= meson_a5_periphs_groups,
	.funcs		= meson_a5_periphs_functions,
	.banks		= meson_a5_periphs_banks,
	.num_pins	= ARRAY_SIZE(meson_a5_periphs_pins),
	.num_groups	= ARRAY_SIZE(meson_a5_periphs_groups),
	.num_funcs	= ARRAY_SIZE(meson_a5_periphs_functions),
	.num_banks	= ARRAY_SIZE(meson_a5_periphs_banks),
	.pmx_ops	= &meson_axg_pmx_ops,
	.pmx_data	= &meson_a5_periphs_pmx_banks_data,
	.parse_dt	= &meson_a1_parse_dt_extra,
};

static const struct of_device_id meson_a5_pinctrl_dt_match[] = {
	{
		.compatible = "amlogic,meson-a5-periphs-pinctrl",
		.data = &meson_a5_periphs_pinctrl_data,
	},
	{ },
};

static struct platform_driver meson_a5_pinctrl_driver = {
	.probe  = meson_pinctrl_probe,
	.driver = {
		.name	= "meson-a5-pinctrl",
	},
};

#ifndef MODULE
static int __init a5_pmx_init(void)
{
	meson_a5_pinctrl_driver.driver.of_match_table =
			meson_a5_pinctrl_dt_match;
	return platform_driver_register(&meson_a5_pinctrl_driver);
}
arch_initcall(a5_pmx_init);

#else
int __init meson_a5_pinctrl_init(void)
{
	meson_a5_pinctrl_driver.driver.of_match_table =
			meson_a5_pinctrl_dt_match;
	return platform_driver_register(&meson_a5_pinctrl_driver);
}
#endif
