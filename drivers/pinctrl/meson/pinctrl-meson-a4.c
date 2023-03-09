// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <dt-bindings/gpio/meson-a4-gpio.h>
#include "pinctrl-meson.h"
#include "pinctrl-meson-axg-pmx.h"
#include "pinctrl-module-init.h"

static const struct pinctrl_pin_desc meson_a4_aobus_pins[] = {
	MESON_PIN(GPIOAO_0),
	MESON_PIN(GPIOAO_1),
	MESON_PIN(GPIOAO_2),
	MESON_PIN(GPIOAO_3),
	MESON_PIN(GPIOAO_4),
	MESON_PIN(GPIOAO_5),
	MESON_PIN(GPIOAO_6),

	MESON_PIN(GPIO_TEST_N)
};

static const struct pinctrl_pin_desc meson_a4_periphs_pins[] = {
	MESON_PIN(GPIOE_0),
	MESON_PIN(GPIOE_1),

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
	MESON_PIN(GPIOT_22)
};

/* GPIOAO func1 */
static const unsigned int i2c_slaveo_a_sda_ao0_pins[]		= { GPIOAO_0 };
static const unsigned int i2c_slaveo_a_scl_ao1_pins[]		= { GPIOAO_1 };
static const unsigned int remote_ao_input_ao3_pins[]		= { GPIOAO_3 };
static const unsigned int uart_ao_c_tx_ao4_pins[]		= { GPIOAO_4 };
static const unsigned int uart_ao_c_rx_ao5_pins[]		= { GPIOAO_5 };
static const unsigned int clk_32k_ao_out_pins[]			= { GPIOAO_6 };

/* GPIOAO func2 */
static const unsigned int uart_ao_c_tx_ao0_pins[]		= { GPIOAO_0 };
static const unsigned int uart_ao_c_rx_ao1_pins[]		= { GPIOAO_1 };
static const unsigned int i2c_slaveo_a_sda_ao2_pins[]		= { GPIOAO_2 };
static const unsigned int i2c_slaveo_a_scl_ao3_pins[]		= { GPIOAO_3 };
static const unsigned int remote_ao_input_ao4_pins[]		= { GPIOAO_4 };
static const unsigned int pwm_g_pins[]				= { GPIOAO_6 };

/* GPIOAO func3 */
static const unsigned int remote_ao_input_ao1_pins[]		= { GPIOAO_1 };
static const unsigned int remote_ao_input_ao6_pins[]		= { GPIOAO_6 };

/* GPIOAO func4 */
static const unsigned int pmic_sleep_ao2_pins[]			= { GPIOAO_2 };
static const unsigned int pmic_sleep_ao4_pins[]			= { GPIOAO_4 };

static struct meson_pmx_group meson_a4_aobus_groups[] __initdata = {
	GPIO_GROUP(GPIOAO_0),
	GPIO_GROUP(GPIOAO_1),
	GPIO_GROUP(GPIOAO_2),
	GPIO_GROUP(GPIOAO_3),
	GPIO_GROUP(GPIOAO_4),
	GPIO_GROUP(GPIOAO_5),
	GPIO_GROUP(GPIOAO_6),

	GPIO_GROUP(GPIO_TEST_N),

	/* GPIOAO func1 */
	GROUP(i2c_slaveo_a_sda_ao0,			1),
	GROUP(i2c_slaveo_a_scl_ao1,			1),
	GROUP(remote_ao_input_ao3,			1),
	GROUP(uart_ao_c_tx_ao4,				1),
	GROUP(uart_ao_c_rx_ao5,				1),
	GROUP(clk_32k_ao_out,				1),

	/* GPIOAO func2 */
	GROUP(uart_ao_c_tx_ao0,				2),
	GROUP(uart_ao_c_rx_ao1,				2),
	GROUP(i2c_slaveo_a_sda_ao2,			2),
	GROUP(i2c_slaveo_a_scl_ao3,			2),
	GROUP(remote_ao_input_ao4,			2),
	GROUP(pwm_g,					2),

	/* GPIOAO func3 */
	GROUP(remote_ao_input_ao1,			3),
	GROUP(remote_ao_input_ao6,			3),

	/* GPIOAO func4 */
	GROUP(pmic_sleep_ao2,				4),
	GROUP(pmic_sleep_ao4,				4)
};

/* GPIOE func1 */
static const unsigned int pwm_e_e0_pins[]			= { GPIOE_0 };
static const unsigned int pwm_f_pins[]				= { GPIOE_1 };

/* GPIOE func3 */
static const unsigned int uart_b_tx_e0_pins[]			= { GPIOE_0 };
static const unsigned int uart_b_rx_e1_pins[]			= { GPIOE_1 };

/* GPIOE func5 */
static const unsigned int i2c0_sda_e0_pins[]			= { GPIOE_0 };
static const unsigned int i2c0_scl_e1_pins[]			= { GPIOE_1 };

/* GPIOD func1 */
static const unsigned int pdm_din1_d2_pins[]			= { GPIOD_2 };
static const unsigned int pdm_din0_d3_pins[]			= { GPIOD_3 };
static const unsigned int pdm_dclk_d4_pins[]			= { GPIOD_4 };
static const unsigned int remote_out_pins[]			= { GPIOD_5 };
static const unsigned int jtag_1_clk_pins[]			= { GPIOD_6 };
static const unsigned int jtag_1_tms_pins[]			= { GPIOD_7 };
static const unsigned int jtag_1_tdi_pins[]			= { GPIOD_8 };
static const unsigned int jtag_1_tdo_pins[]			= { GPIOD_9 };
static const unsigned int clk12_24_pins[]			= { GPIOD_10 };
static const unsigned int tdm_d3_d11_pins[]			= { GPIOD_11 };
static const unsigned int tdm_d2_d12_pins[]			= { GPIOD_12 };
static const unsigned int mclk1_d13_pins[]			= { GPIOD_13 };
static const unsigned int tdm_sclk1_d14_pins[]			= { GPIOD_14 };
static const unsigned int tdm_fs1_d15_pins[]			= { GPIOD_15 };

/* GPIOD func2 */
static const unsigned int uart_b_tx_d0_pins[]			= { GPIOD_0 };
static const unsigned int uart_b_rx_d1_pins[]			= { GPIOD_1 };
static const unsigned int uart_b_cts_pins[]			= { GPIOD_2 };
static const unsigned int uart_b_rts_pins[]			= { GPIOD_3 };
static const unsigned int remote_in_pins[]			= { GPIOD_5 };
static const unsigned int pwm_a_hiz_pins[]			= { GPIOD_6 };
static const unsigned int pwm_b_hiz_pins[]			= { GPIOD_7 };
static const unsigned int pwm_c_hiz_pins[]			= { GPIOD_8 };
static const unsigned int pwm_d_hiz_pins[]			= { GPIOD_9 };
static const unsigned int gen_clk_pins[]			= { GPIOD_10 };
static const unsigned int pwm_e_hiz_pins[]			= { GPIOD_11 };
static const unsigned int i2c3_sda_d12_pins[]			= { GPIOD_12 };
static const unsigned int i2c3_scl_d13_pins[]			= { GPIOD_13 };
static const unsigned int i2c2_sda_d14_pins[]			= { GPIOD_14 };
static const unsigned int i2c2_scl_d15_pins[]			= { GPIOD_15 };

/* GPIOD func3 */
static const unsigned int uart_a_tx_d2_pins[]			= { GPIOD_2 };
static const unsigned int uart_a_rx_d3_pins[]			= { GPIOD_3 };
static const unsigned int pwm_a_d6_pins[]			= { GPIOD_6 };
static const unsigned int pwm_b_d7_pins[]			= { GPIOD_7 };
static const unsigned int pwm_c_d8_pins[]			= { GPIOD_8 };
static const unsigned int pwm_d_d9_pins[]			= { GPIOD_9 };
static const unsigned int pwm_e_d11_pins[]			= { GPIOD_11 };
static const unsigned int pwm_h_d12_pins[]			= { GPIOD_12 };

/* GPIOD func4 */
static const unsigned int spi_b_miso_d2_pins[]			= { GPIOD_2 };
static const unsigned int spi_b_mosi_d3_pins[]			= { GPIOD_3 };
static const unsigned int spi_b_clk_d4_pins[]			= { GPIOD_4 };
static const unsigned int spi_b_ss0_d5_pins[]			= { GPIOD_5 };
static const unsigned int spi_b_dq2_pins[]			= { GPIOD_6 };
static const unsigned int spi_b_dq3_pins[]			= { GPIOD_7 };
static const unsigned int tdm_d12_d8_pins[]			= { GPIOD_8 };
static const unsigned int tdm_d13_d9_pins[]			= { GPIOD_9 };
static const unsigned int i2c1_sda_d10_pins[]			= { GPIOD_10 };
static const unsigned int i2c1_scl_d11_pins[]			= { GPIOD_11 };

/* GPIOD func5 */
static const unsigned int i2c1_sda_d2_pins[]			= { GPIOD_2 };
static const unsigned int i2c1_scl_d3_pins[]			= { GPIOD_3 };
static const unsigned int eth_mdio_d6_pins[]			= { GPIOD_6 };
static const unsigned int eth_mdc_d7_pins[]			= { GPIOD_7 };
static const unsigned int dcon_led_d8_pins[]			= { GPIOD_8 };
static const unsigned int mic_mute_en_pins[]			= { GPIOD_10 };
static const unsigned int mic_mute_key_pins[]			= { GPIOD_11 };
static const unsigned int dcon_led_d12_pins[]			= { GPIOD_12 };
static const unsigned int mclk0_d13_pins[]			= { GPIOD_13 };
static const unsigned int tdm_sclk0_d14_pins[]			= { GPIOD_14 };
static const unsigned int tdm_fs0_d15_pins[]			= { GPIOD_15 };

/* GPIOB func1 */
static const unsigned int emmc_nand_d0_pins[]			= { GPIOB_0 };
static const unsigned int emmc_nand_d1_pins[]			= { GPIOB_1 };
static const unsigned int emmc_nand_d2_pins[]			= { GPIOB_2 };
static const unsigned int emmc_nand_d3_pins[]			= { GPIOB_3 };
static const unsigned int emmc_nand_d4_pins[]			= { GPIOB_4 };
static const unsigned int emmc_nand_d5_pins[]			= { GPIOB_5 };
static const unsigned int emmc_nand_d6_pins[]			= { GPIOB_6 };
static const unsigned int emmc_nand_d7_pins[]			= { GPIOB_7 };
static const unsigned int emmc_clk_pins[]			= { GPIOB_8 };
static const unsigned int emmc_rst_pins[]			= { GPIOB_9 };
static const unsigned int emmc_cmd_pins[]			= { GPIOB_10 };
static const unsigned int emmc_nand_ds_pins[]			= { GPIOB_11 };

/* GPIOB func2 */
static const unsigned int nand_wen_clk_pins[]			= { GPIOB_8 };
static const unsigned int nand_ale_pins[]			= { GPIOB_9 };
static const unsigned int nand_ren_wr_pins[]			= { GPIOB_10 };
static const unsigned int nand_cle_pins[]			= { GPIOB_11 };
static const unsigned int nand_ce0_pins[]			= { GPIOB_12 };

/* GPIOB func4 */
static const unsigned int spinf_mo_d0_pins[]			= { GPIOB_0 };
static const unsigned int spinf_mi_d1_pins[]			= { GPIOB_1 };
static const unsigned int spinf_wp_d2_pins[]			= { GPIOB_2 };
static const unsigned int spinf_hold_d3_pins[]			= { GPIOB_3 };
static const unsigned int spinf_d4_pins[]			= { GPIOB_4 };
static const unsigned int spinf_d5_pins[]			= { GPIOB_5 };
static const unsigned int spinf_d6_pins[]			= { GPIOB_6 };
static const unsigned int spinf_d7_pins[]			= { GPIOB_7 };
static const unsigned int spinf_rst_pins[]			= { GPIOB_9 };
static const unsigned int spinf_clk_pins[]			= { GPIOB_10 };
static const unsigned int spinf_cs1_pins[]			= { GPIOB_12 };
static const unsigned int spinf_cs0_pins[]			= { GPIOB_13 };

/* GPIOX func1 */
static const unsigned int sdio_d0_pins[]			= { GPIOX_0 };
static const unsigned int sdio_d1_pins[]			= { GPIOX_1 };
static const unsigned int sdio_d2_pins[]			= { GPIOX_2 };
static const unsigned int sdio_d3_pins[]			= { GPIOX_3 };
static const unsigned int sdio_clk_pins[]			= { GPIOX_4 };
static const unsigned int sdio_cmd_pins[]			= { GPIOX_5 };
static const unsigned int mclk0_x6_pins[]			= { GPIOX_6 };
static const unsigned int tdm_d1_pins[]				= { GPIOX_7 };
static const unsigned int tdm_d0_x8_pins[]			= { GPIOX_8 };
static const unsigned int tdm_fs0_x9_pins[]			= { GPIOX_9 };
static const unsigned int tdm_sclk0_x10_pins[]			= { GPIOX_10 };
static const unsigned int uart_a_tx_x11_pins[]			= { GPIOX_11 };
static const unsigned int uart_a_rx_x12_pins[]			= { GPIOX_12 };
static const unsigned int uart_a_cts_pins[]			= { GPIOX_13 };
static const unsigned int uart_a_rts_pins[]			= { GPIOX_14 };
static const unsigned int pwm_g_x15_pins[]			= { GPIOX_15 };
static const unsigned int pwm_b_x17_pins[]			= { GPIOX_17 };

/* GPIOX func2 */
static const unsigned int pwm_a_x6_pins[]			= { GPIOX_6 };
static const unsigned int spi_a_mosi_x7_pins[]			= { GPIOX_7 };
static const unsigned int spi_a_miso_x8_pins[]			= { GPIOX_8 };
static const unsigned int spi_a_ss0_x9_pins[]			= { GPIOX_9 };
static const unsigned int spi_a_clk_x10_pins[]			= { GPIOX_10 };
static const unsigned int i2c0_sda_x16_pins[]			= { GPIOX_16 };
static const unsigned int i2c0_scl_x17_pins[]			= { GPIOX_17 };

/* GPIOX func5 */
static const unsigned int eth_mdio_x0_pins[]			= { GPIOX_0 };
static const unsigned int eth_mdc_x1_pins[]			= { GPIOX_1 };
static const unsigned int eth_rgmii_rx_clk_pins[]		= { GPIOX_2 };
static const unsigned int eth_rx_dv_pins[]			= { GPIOX_3 };
static const unsigned int eth_rxd0_pins[]			= { GPIOX_4 };
static const unsigned int eth_rxd1_pins[]			= { GPIOX_5 };
static const unsigned int eth_rxd2_rgmii_pins[]			= { GPIOX_6 };
static const unsigned int eth_rxd3_rgmii_pins[]			= { GPIOX_7 };
static const unsigned int eth_rgmii_tx_clk_pins[]		= { GPIOX_8 };
static const unsigned int eth_txen_pins[]			= { GPIOX_9 };
static const unsigned int eth_txd0_pins[]			= { GPIOX_10 };
static const unsigned int eth_txd1_pins[]			= { GPIOX_11 };
static const unsigned int eth_txd2_rgmii_pins[]			= { GPIOX_12 };
static const unsigned int eth_txd3_rgmii_pins[]			= { GPIOX_13 };
static const unsigned int spi_b_mosi_x14_pins[]			= { GPIOX_14 };
static const unsigned int spi_b_ss0_x15_pins[]			= { GPIOX_15 };
static const unsigned int spi_b_clk_x16_pins[]			= { GPIOX_16 };
static const unsigned int spi_b_miso_x17_pins[]			= { GPIOX_17 };

/* GPIOT func1 */
static const unsigned int tdm_d2_t0_pins[]			= { GPIOT_0 };
static const unsigned int tdm_d3_t1_pins[]			= { GPIOT_1 };
static const unsigned int tdm_fs1_t2_pins[]			= { GPIOT_2 };
static const unsigned int tdm_sclk1_t3_pins[]			= { GPIOT_3 };
static const unsigned int mclk1_t4_pins[]			= { GPIOT_4 };
static const unsigned int tdm_d4_t5_pins[]			= { GPIOT_5 };
static const unsigned int tdm_d5_t6_pins[]			= { GPIOT_6 };
static const unsigned int tdm_d6_t7_pins[]			= { GPIOT_7 };
static const unsigned int tdm_d7_pins[]				= { GPIOT_8 };
static const unsigned int spdif_out_pins[]			= { GPIOT_9 };
static const unsigned int spdif_in_t10_pins[]			= { GPIOT_10 };
static const unsigned int tdm_fs2_t11_pins[]			= { GPIOT_11 };
static const unsigned int tdm_sclk2_t12_pins[]			= { GPIOT_12 };
static const unsigned int tdm_d8_pins[]				= { GPIOT_13 };
static const unsigned int tdm_d9_pins[]				= { GPIOT_14 };
static const unsigned int tdm_d10_pins[]			= { GPIOT_15 };
static const unsigned int tdm_d11_pins[]			= { GPIOT_16 };
static const unsigned int mclk2_t17_pins[]			= { GPIOT_17 };
static const unsigned int tdm_d12_t18_pins[]			= { GPIOT_18 };
static const unsigned int tdm_d13_t19_pins[]			= { GPIOT_19 };
static const unsigned int i2c0_sda_t21_pins[]			= { GPIOT_21 };
static const unsigned int i2c0_scl_t22_pins[]			= { GPIOT_22 };

/* GPIOT func2 */
static const unsigned int i2c3_sda_t0_pins[]			= { GPIOT_0 };
static const unsigned int i2c3_scl_t1_pins[]			= { GPIOT_1 };
static const unsigned int uart_d_tx_t7_pins[]			= { GPIOT_7 };
static const unsigned int uart_d_rx_t8_pins[]			= { GPIOT_8 };
static const unsigned int uart_d_cts_pins[]			= { GPIOT_9 };
static const unsigned int uart_d_rts_pins[]			= { GPIOT_10 };
static const unsigned int spi_a_dq2_pins[]			= { GPIOT_11 };
static const unsigned int spi_a_dq3_pins[]			= { GPIOT_12 };
static const unsigned int spi_a_clk_t13_pins[]			= { GPIOT_13 };
static const unsigned int spi_a_mosi_t14_pins[]			= { GPIOT_14 };
static const unsigned int spi_a_miso_t15_pins[]			= { GPIOT_15 };
static const unsigned int spi_a_ss0_t16_pins[]			= { GPIOT_16 };
static const unsigned int spi_a_ss1_pins[]			= { GPIOT_17 };
static const unsigned int spi_a_ss2_pins[]			= { GPIOT_18 };
static const unsigned int pwm_g_t19_pins[]			= { GPIOT_19 };
static const unsigned int pwm_h_t20_pins[]			= { GPIOT_20 };
static const unsigned int eth_link_led_t21_pins[]		= { GPIOT_21 };
static const unsigned int eth_act_led_t22_pins[]		= { GPIOT_22 };

/* GPIOT func3 */
static const unsigned int pdm_din1_t0_pins[]			= { GPIOT_0 };
static const unsigned int pdm_din0_t1_pins[]			= { GPIOT_1 };
static const unsigned int pdm_dclk_t2_pins[]			= { GPIOT_2 };
static const unsigned int spi_b_clk_t5_pins[]			= { GPIOT_5 };
static const unsigned int spi_b_mosi_t6_pins[]			= { GPIOT_6 };
static const unsigned int spi_b_miso_t7_pins[]			= { GPIOT_7 };
static const unsigned int spi_b_ss0_t8_pins[]			= { GPIOT_8 };
static const unsigned int spi_b_ss1_pins[]			= { GPIOT_9 };
static const unsigned int spi_b_ss2_pins[]			= { GPIOT_10 };
static const unsigned int uart_e_tx_pins[]			= { GPIOT_14 };
static const unsigned int uart_e_rx_pins[]			= { GPIOT_15 };
static const unsigned int uart_e_rts_pins[]			= { GPIOT_16 };
static const unsigned int uart_e_cts_pins[]			= { GPIOT_17 };
static const unsigned int eth_link_led_t18_pins[]		= { GPIOT_18 };
static const unsigned int eth_act_led_t19_pins[]		= { GPIOT_19 };
static const unsigned int tdm_d4_t20_pins[]			= { GPIOT_20 };
static const unsigned int tdm_d5_t21_pins[]			= { GPIOT_21 };
static const unsigned int tdm_d6_t22_pins[]			= { GPIOT_22 };

/* GPIOT func4 */
static const unsigned int dcon_led_t4_pins[]			= { GPIOT_4 };
static const unsigned int mclk2_t5_pins[]			= { GPIOT_5 };
static const unsigned int tdm_fs2_t6_pins[]			= { GPIOT_6 };
static const unsigned int tdm_sclk2_t7_pins[]			= { GPIOT_7 };
static const unsigned int spdif_in_t9_pins[]			= { GPIOT_9 };
static const unsigned int i2c2_sda_t11_pins[]			= { GPIOT_11 };
static const unsigned int i2c2_scl_t12_pins[]			= { GPIOT_12 };
static const unsigned int pwm_d_t15_pins[]			= { GPIOT_15 };
static const unsigned int pwm_c_t16_pins[]			= { GPIOT_16 };
static const unsigned int uart_d_tx_t18_pins[]			= { GPIOT_18 };
static const unsigned int uart_d_rx_t19_pins[]			= { GPIOT_19 };
static const unsigned int tdm_d0_t20_pins[]			= { GPIOT_20 };
static const unsigned int tdm_fs0_t21_pins[]			= { GPIOT_21 };
static const unsigned int tdm_sclk0_t22_pins[]			= { GPIOT_22 };

/* GPIOT func5 */
static const unsigned int lcd_d0_pins[]				= { GPIOT_0 };
static const unsigned int lcd_d1_pins[]				= { GPIOT_1 };
static const unsigned int lcd_d2_pins[]				= { GPIOT_2 };
static const unsigned int lcd_d3_pins[]				= { GPIOT_3 };
static const unsigned int lcd_d4_pins[]				= { GPIOT_4 };
static const unsigned int lcd_d5_pins[]				= { GPIOT_5 };
static const unsigned int lcd_d6_pins[]				= { GPIOT_6 };
static const unsigned int lcd_d7_pins[]				= { GPIOT_7 };
static const unsigned int lcd_d8_pins[]				= { GPIOT_8 };
static const unsigned int lcd_d9_pins[]				= { GPIOT_9 };
static const unsigned int lcd_d10_pins[]			= { GPIOT_10 };
static const unsigned int lcd_d11_pins[]			= { GPIOT_11 };
static const unsigned int lcd_d12_pins[]			= { GPIOT_12 };
static const unsigned int lcd_d13_pins[]			= { GPIOT_13 };
static const unsigned int lcd_d14_pins[]			= { GPIOT_14 };
static const unsigned int lcd_d15_pins[]			= { GPIOT_15 };
static const unsigned int lcd_d16_pins[]			= { GPIOT_16 };
static const unsigned int lcd_d17_pins[]			= { GPIOT_17 };
static const unsigned int lcd_clk_pins[]			= { GPIOT_18 };
static const unsigned int lcd_den_pins[]			= { GPIOT_19 };
static const unsigned int lcd_hsync_pins[]			= { GPIOT_20 };
static const unsigned int lcd_vsync_pins[]			= { GPIOT_21 };

static struct meson_pmx_group meson_a4_periphs_groups[] __initdata = {
	/* func0 as GPIO */
	GPIO_GROUP(GPIOE_0),
	GPIO_GROUP(GPIOE_1),

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

	/* GPIOE func1 */
	GROUP(pwm_e_e0,					1),
	GROUP(pwm_f,					1),

	/* GPIOE func3 */
	GROUP(uart_b_tx_e0,				3),
	GROUP(uart_b_rx_e1,				3),

	/* GPIOE func5 */
	GROUP(i2c0_sda_e0,				5),
	GROUP(i2c0_scl_e1,				5),

	/* GPIOD func1 */
	GROUP(pdm_din1_d2,				1),
	GROUP(pdm_din0_d3,				1),
	GROUP(pdm_dclk_d4,				1),
	GROUP(remote_out,				1),
	GROUP(jtag_1_clk,				1),
	GROUP(jtag_1_tms,				1),
	GROUP(jtag_1_tdi,				1),
	GROUP(jtag_1_tdo,				1),
	GROUP(clk12_24,					1),
	GROUP(tdm_d3_d11,				1),
	GROUP(tdm_d2_d12,				1),
	GROUP(mclk1_d13,				1),
	GROUP(tdm_sclk1_d14,				1),
	GROUP(tdm_fs1_d15,				1),

	/* GPIOD func2 */
	GROUP(uart_b_tx_d0,				2),
	GROUP(uart_b_rx_d1,				2),
	GROUP(uart_b_cts,				2),
	GROUP(uart_b_rts,				2),
	GROUP(remote_in,				2),
	GROUP(pwm_a_hiz,				2),
	GROUP(pwm_b_hiz,				2),
	GROUP(pwm_c_hiz,				2),
	GROUP(pwm_d_hiz,				2),
	GROUP(gen_clk,					2),
	GROUP(pwm_e_hiz,				2),
	GROUP(i2c3_sda_d12,				2),
	GROUP(i2c3_scl_d13,				2),
	GROUP(i2c2_sda_d14,				2),
	GROUP(i2c2_scl_d15,				2),

	/* GPIOD func3 */
	GROUP(uart_a_tx_d2,				3),
	GROUP(uart_a_rx_d3,				3),
	GROUP(pwm_a_d6,					3),
	GROUP(pwm_b_d7,					3),
	GROUP(pwm_c_d8,					3),
	GROUP(pwm_d_d9,					3),
	GROUP(pwm_e_d11,				3),
	GROUP(pwm_h_d12,				3),

	/* GPIOD func4 */
	GROUP(spi_b_miso_d2,				4),
	GROUP(spi_b_mosi_d3,				4),
	GROUP(spi_b_clk_d4,				4),
	GROUP(spi_b_ss0_d5,				4),
	GROUP(spi_b_dq2,				4),
	GROUP(spi_b_dq3,				4),
	GROUP(tdm_d12_d8,				4),
	GROUP(tdm_d13_d9,				4),
	GROUP(i2c1_sda_d10,				4),
	GROUP(i2c1_scl_d11,				4),

	/* GPIOD func5 */
	GROUP(i2c1_sda_d2,				5),
	GROUP(i2c1_scl_d3,				5),
	GROUP(eth_mdio_d6,				5),
	GROUP(eth_mdc_d7,				5),
	GROUP(dcon_led_d8,				5),
	GROUP(mic_mute_en,				5),
	GROUP(mic_mute_key,				5),
	GROUP(dcon_led_d12,				5),
	GROUP(mclk0_d13,				5),
	GROUP(tdm_sclk0_d14,				5),
	GROUP(tdm_fs0_d15,				5),

	/* GPIOB func1 */
	GROUP(emmc_nand_d0,				1),
	GROUP(emmc_nand_d1,				1),
	GROUP(emmc_nand_d2,				1),
	GROUP(emmc_nand_d3,				1),
	GROUP(emmc_nand_d4,				1),
	GROUP(emmc_nand_d5,				1),
	GROUP(emmc_nand_d6,				1),
	GROUP(emmc_nand_d7,				1),
	GROUP(emmc_clk,					1),
	GROUP(emmc_rst,					1),
	GROUP(emmc_cmd,					1),
	GROUP(emmc_nand_ds,				1),

	/* GPIOB func2 */
	GROUP(nand_wen_clk,				2),
	GROUP(nand_ale,					2),
	GROUP(nand_ren_wr,				2),
	GROUP(nand_cle,					2),
	GROUP(nand_ce0,					2),

	/* GPIOB func4 */
	GROUP(spinf_mo_d0,				4),
	GROUP(spinf_mi_d1,				4),
	GROUP(spinf_wp_d2,				4),
	GROUP(spinf_hold_d3,				4),
	GROUP(spinf_d4,					4),
	GROUP(spinf_d5,					4),
	GROUP(spinf_d6,					4),
	GROUP(spinf_d7,					4),
	GROUP(spinf_rst,				4),
	GROUP(spinf_clk,				4),
	GROUP(spinf_cs1,				4),
	GROUP(spinf_cs0,				4),

	/* GPIOX func1 */
	GROUP(sdio_d0,					1),
	GROUP(sdio_d1,					1),
	GROUP(sdio_d2,					1),
	GROUP(sdio_d3,					1),
	GROUP(sdio_clk,					1),
	GROUP(sdio_cmd,					1),
	GROUP(mclk0_x6,					1),
	GROUP(tdm_d1,					1),
	GROUP(tdm_d0_x8,				1),
	GROUP(tdm_fs0_x9,				1),
	GROUP(tdm_sclk0_x10,				1),
	GROUP(uart_a_tx_x11,				1),
	GROUP(uart_a_rx_x12,				1),
	GROUP(uart_a_cts,				1),
	GROUP(uart_a_rts,				1),
	GROUP(pwm_g_x15,				1),
	GROUP(pwm_b_x17,				1),

	/* GPIOX func2 */
	GROUP(pwm_a_x6,					2),
	GROUP(spi_a_mosi_x7,				2),
	GROUP(spi_a_miso_x8,				2),
	GROUP(spi_a_ss0_x9,				2),
	GROUP(spi_a_clk_x10,				2),
	GROUP(i2c0_sda_x16,				2),
	GROUP(i2c0_scl_x17,				2),

	/* GPIOX func5 */
	GROUP(eth_mdio_x0,				5),
	GROUP(eth_mdc_x1,				5),
	GROUP(eth_rgmii_rx_clk,				5),
	GROUP(eth_rx_dv,				5),
	GROUP(eth_rxd0,					5),
	GROUP(eth_rxd1,					5),
	GROUP(eth_rxd2_rgmii,				5),
	GROUP(eth_rxd3_rgmii,				5),
	GROUP(eth_rgmii_tx_clk,				5),
	GROUP(eth_txen,					5),
	GROUP(eth_txd0,					5),
	GROUP(eth_txd1,					5),
	GROUP(eth_txd2_rgmii,				5),
	GROUP(eth_txd3_rgmii,				5),
	GROUP(spi_b_mosi_x14,				5),
	GROUP(spi_b_ss0_x15,				5),
	GROUP(spi_b_clk_x16,				5),
	GROUP(spi_b_miso_x17,				5),

	/* GPIOT func1 */
	GROUP(tdm_d2_t0,				1),
	GROUP(tdm_d3_t1,				1),
	GROUP(tdm_fs1_t2,				1),
	GROUP(tdm_sclk1_t3,				1),
	GROUP(mclk1_t4,					1),
	GROUP(tdm_d4_t5,				1),
	GROUP(tdm_d5_t6,				1),
	GROUP(tdm_d6_t7,				1),
	GROUP(tdm_d7,					1),
	GROUP(spdif_out,				1),
	GROUP(spdif_in_t10,				1),
	GROUP(tdm_fs2_t11,				1),
	GROUP(tdm_sclk2_t12,				1),
	GROUP(tdm_d8,					1),
	GROUP(tdm_d9,					1),
	GROUP(tdm_d10,					1),
	GROUP(tdm_d11,					1),
	GROUP(mclk2_t17,				1),
	GROUP(tdm_d12_t18,				1),
	GROUP(tdm_d13_t19,				1),
	GROUP(i2c0_sda_t21,				1),
	GROUP(i2c0_scl_t22,				1),

	/* GPIOT func2 */
	GROUP(i2c3_sda_t0,				2),
	GROUP(i2c3_scl_t1,				2),
	GROUP(uart_d_tx_t7,				2),
	GROUP(uart_d_rx_t8,				2),
	GROUP(uart_d_cts,				2),
	GROUP(uart_d_rts,				2),
	GROUP(spi_a_dq2,				2),
	GROUP(spi_a_dq3,				2),
	GROUP(spi_a_clk_t13,				2),
	GROUP(spi_a_mosi_t14,				2),
	GROUP(spi_a_miso_t15,				2),
	GROUP(spi_a_ss0_t16,				2),
	GROUP(spi_a_ss1,				2),
	GROUP(spi_a_ss2,				2),
	GROUP(pwm_g_t19,				2),
	GROUP(pwm_h_t20,				2),
	GROUP(eth_link_led_t21,				2),
	GROUP(eth_act_led_t22,				2),

	/* GPIOT func3 */
	GROUP(pdm_din1_t0,				3),
	GROUP(pdm_din0_t1,				3),
	GROUP(pdm_dclk_t2,				3),
	GROUP(spi_b_clk_t5,				3),
	GROUP(spi_b_mosi_t6,				3),
	GROUP(spi_b_miso_t7,				3),
	GROUP(spi_b_ss0_t8,				3),
	GROUP(spi_b_ss1,				3),
	GROUP(spi_b_ss2,				3),
	GROUP(uart_e_tx,				3),
	GROUP(uart_e_rx,				3),
	GROUP(uart_e_rts,				3),
	GROUP(uart_e_cts,				3),
	GROUP(eth_link_led_t18,				3),
	GROUP(eth_act_led_t19,				3),
	GROUP(tdm_d4_t20,				3),
	GROUP(tdm_d5_t21,				3),
	GROUP(tdm_d6_t22,				3),

	/* GPIOT func4 */
	GROUP(dcon_led_t4,				4),
	GROUP(mclk2_t5,					4),
	GROUP(tdm_fs2_t6,				4),
	GROUP(tdm_sclk2_t7,				4),
	GROUP(spdif_in_t9,				4),
	GROUP(i2c2_sda_t11,				4),
	GROUP(i2c2_scl_t12,				4),
	GROUP(pwm_d_t15,				4),
	GROUP(pwm_c_t16,				4),
	GROUP(uart_d_tx_t18,				4),
	GROUP(uart_d_rx_t19,				4),
	GROUP(tdm_d0_t20,				4),
	GROUP(tdm_fs0_t21,				4),
	GROUP(tdm_sclk0_t22,				4),

	/* GPIOT func5 */
	GROUP(lcd_d0,					5),
	GROUP(lcd_d1,					5),
	GROUP(lcd_d2,					5),
	GROUP(lcd_d3,					5),
	GROUP(lcd_d4,					5),
	GROUP(lcd_d5,					5),
	GROUP(lcd_d6,					5),
	GROUP(lcd_d7,					5),
	GROUP(lcd_d8,					5),
	GROUP(lcd_d9,					5),
	GROUP(lcd_d10,					5),
	GROUP(lcd_d11,					5),
	GROUP(lcd_d12,					5),
	GROUP(lcd_d13,					5),
	GROUP(lcd_d14,					5),
	GROUP(lcd_d15,					5),
	GROUP(lcd_d16,					5),
	GROUP(lcd_d17,					5),
	GROUP(lcd_clk,					5),
	GROUP(lcd_den,					5),
	GROUP(lcd_hsync,				5),
	GROUP(lcd_vsync,				5)
};

static const char * const gpio_aobus_groups[] = {
	"GPIOAO_0", "GPIOAO_1", "GPIOAO_2", "GPIOAO_3",
	"GPIOAO_4", "GPIOAO_5", "GPIOAO_6",
	"GPIO_TEST_N"
};

static const char * const i2c_slave_ao_groups[] = {
	"i2c_slaveo_a_sda_ao0", "i2c_slaveo_a_scl_ao1",
	"i2c_slaveo_a_sda_ao2", "i2c_slaveo_a_scl_ao3"
};

static const char * const remote_in_ao_groups[] = {
	"remote_ao_input_ao1", "remote_ao_input_ao3", "remote_ao_input_ao4",
	"remote_ao_input_ao6"
};

static const char * const uart_c_ao_groups[] = {
	"uart_ao_c_tx_ao0", "uart_ao_c_rx_ao1", "uart_ao_c_tx_ao4",
	"uart_ao_c_rx_ao5"
};

static const char * const clk32_ao_groups[] = {
	"clk_32k_ao_out"
};

static const char * const pwm_g_ao_groups[] = {
	"pwm_g"
};

static const char * const pmic_sleep_ao_groups[] = {
	"pmic_sleep_ao2", "pmic_sleep_ao4"
};

static struct meson_pmx_func meson_a4_aobus_functions[] __initdata = {
	FUNCTION(gpio_aobus),
	FUNCTION(i2c_slave_ao),
	FUNCTION(remote_in_ao),
	FUNCTION(uart_c_ao),
	FUNCTION(clk32_ao),
	FUNCTION(pwm_g_ao),
	FUNCTION(pmic_sleep_ao)
};

static const char * const gpio_periphs_groups[] = {
	"GPIOE_0", "GPIOE_1",  "GPIOD_0", "GPIOD_1", "GPIOD_2",
	"GPIOD_3", "GPIOD_4", "GPIOD_5", "GPIOD_6",
	"GPIOD_7", "GPIOD_8", "GPIOD_9", "GPIOD_10",
	"GPIOD_11", "GPIOD_12", "GPIOD_13", "GPIOD_14",
	"GPIOD_15", "GPIOB_0", "GPIOB_1", "GPIOB_2",
	"GPIOB_3", "GPIOB_4", "GPIOB_5", "GPIOB_6",
	"GPIOB_7", "GPIOB_8", "GPIOB_9", "GPIOB_10",
	"GPIOB_11", "GPIOB_12", "GPIOB_13", "GPIOX_0",
	"GPIOX_1", "GPIOX_2", "GPIOX_3", "GPIOX_4",
	"GPIOX_5", "GPIOX_6", "GPIOX_7", "GPIOX_8",
	"GPIOX_9", "GPIOX_10", "GPIOX_11", "GPIOX_12",
	"GPIOX_13", "GPIOX_14", "GPIOX_15", "GPIOX_16",
	"GPIOX_17", "GPIOT_0", "GPIOT_1", "GPIOT_2",
	"GPIOT_3", "GPIOT_4", "GPIOT_5", "GPIOT_6",
	"GPIOT_7", "GPIOT_8", "GPIOT_9", "GPIOT_10",
	"GPIOT_11", "GPIOT_12", "GPIOT_13", "GPIOT_14",
	"GPIOT_15", "GPIOT_16", "GPIOT_17", "GPIOT_18",
	"GPIOT_19", "GPIOT_20", "GPIOT_21", "GPIOT_22"
};

static const char * const uart_a_groups[] = {
	"uart_a_tx_d2", "uart_a_rx_d3", "uart_a_tx_x11",
	"uart_a_rx_x12", "uart_a_cts", "uart_a_rts"
};

static const char * const uart_b_groups[] = {
	"uart_b_tx_e0", "uart_b_rx_e1", "uart_b_tx_d0", "uart_b_cts",
	"uart_b_rts", "uart_b_rx_d1"
};

static const char * const uart_d_groups[] = {
	"uart_d_tx_t7", "uart_d_rx_t8", "uart_d_cts", "uart_d_rts",
	"uart_d_tx_t18", "uart_d_rx_t19"
};

static const char * const uart_e_groups[] = {
	"uart_e_tx", "uart_e_rx", "uart_e_rts", "uart_e_cts"
};

static const char * const i2c0_groups[] = {
	"i2c0_sda_e0", "i2c0_scl_e1", "i2c0_sda_x16", "i2c0_scl_x17",
	"i2c0_sda_t21", "i2c0_scl_t22"
};

static const char * const i2c1_groups[] = {
	"i2c1_sda_d2", "i2c1_scl_d3", "i2c1_sda_d10", "i2c1_scl_d11"
};

static const char * const i2c2_groups[] = {
	"i2c2_sda_d14", "i2c2_scl_d15", "i2c2_sda_t11", "i2c2_scl_t12"
};

static const char * const i2c3_groups[] = {
	"i2c3_sda_d12", "i2c3_scl_d13", "i2c3_sda_t0", "i2c3_scl_t1"
};

static const char * const pwm_a_groups[] = {
	"pwm_a_hiz", "pwm_a_x6", "pwm_a_d6"
};

static const char * const pwm_b_groups[] = {
	"pwm_b_hiz", "pwm_b_x17", "pwm_b_d7"
};

static const char * const pwm_c_groups[] = {
	"pwm_c_hiz", "pwm_c_t16", "pwm_c_d8"
};

static const char * const pwm_d_groups[] = {
	"pwm_d_hiz", "pwm_d_t15", "pwm_d_d9"
};

static const char * const pwm_e_groups[] = {
	"pwm_e_e0", "pwm_e_hiz", "pwm_e_d11"
};

static const char * const pwm_f_groups[] = {
	"pwm_f"
};

static const char * const pwm_g_groups[] = {
	"pwm_g_x15", "pwm_g_t19"
};

static const char * const pwm_h_groups[] = {
	"pwm_h_d12", "pwm_h_t20"
};

static const char * const remote_out_groups[] = {
	"remote_out"
};

static const char * const remote_in_groups[] = {
	"remote_in"
};

static const char * const dcon_led_groups[] = {
	"dcon_led_d8", "dcon_led_d12", "dcon_led_t4"
};

static const char * const spinf_groups[] = {
	"spinf_mo_d0", "spinf_mi_d1", "spinf_wp_d2", "spinf_hold_d3",
	"spinf_d4", "spinf_d5", "spinf_d6", "spinf_d7", "spinf_rst",
	"spinf_clk", "spinf_cs1", "spinf_cs0"
};

static const char * const lcd_groups[] = {
	"lcd_d0", "lcd_d1", "lcd_d2", "lcd_d3", "lcd_d4", "lcd_d5",
	"lcd_d6", "lcd_d7", "lcd_d8", "lcd_d9", "lcd_d10", "lcd_d11",
	"lcd_d12", "lcd_d13", "lcd_d14", "lcd_d15", "lcd_d16", "lcd_d17",
	"lcd_clk", "lcd_den", "lcd_hsync", "lcd_vsync"
};

static const char * const jtag_1_groups[] = {
	"jtag_1_clk", "jtag_1_tms", "jtag_1_tdi", "jtag_1_tdo"
};

static const char * const gen_clk_groups[] = {
	"gen_clk"
};

static const char * const clk12_24_groups[] = {
	"clk12_24"
};

static const char * const emmc_groups[] = {
	"emmc_nand_d0", "emmc_nand_d1", "emmc_nand_d2", "emmc_nand_d3",
	"emmc_nand_d4", "emmc_nand_d5", "emmc_nand_d6", "emmc_nand_d7",
	"emmc_clk", "emmc_rst", "emmc_cmd", "emmc_nand_ds"
};

static const char * const nand_groups[] = {
	"nand_wen_clk", "nand_ren_wr", "nand_cle", "nand_ale", "nand_ce0"
};

static const char * const spi_a_groups[] = {
	"spi_a_mosi_x7", "spi_a_miso_x8", "spi_a_ss0_x9", "spi_a_clk_x10",
	"spi_a_dq2", "spi_a_dq3", "spi_a_clk_t13", "spi_a_mosi_t14",
	"spi_a_miso_t15", "spi_a_ss0_t16", "spi_a_ss1", "spi_a_ss2"
};

static const char * const spi_b_groups[] = {
	"spi_b_miso_d2", "spi_b_mosi_d3", "spi_b_clk_d4", "spi_b_ss0_d5",
	"spi_b_dq2", "spi_b_dq3", "spi_b_mosi_x14", "spi_b_ss0_x15",
	"spi_b_clk_x16", "spi_b_miso_x17", "spi_b_clk_t5", "spi_b_mosi_t6",
	"spi_b_miso_t7", "spi_b_ss0_t8", "spi_b_ss1", "spi_b_ss2"
};

static const char * const pdm_groups[] = {
	"pdm_din1_d2", "pdm_dclk_d4", "pdm_din1_t0", "pdm_din0_t1",
	"pdm_dclk_t2", "pdm_din0_d3"
};

static const char * const sdio_groups[] = {
	"sdio_d0", "sdio_d1", "sdio_d2", "sdio_d3",
	"sdio_clk", "sdio_cmd"
};

static const char * const eth_groups[] = {
	"eth_mdio_d6", "eth_mdc_d7", "eth_mdio_x0", "eth_mdc_x1",
	"eth_rgmii_rx_clk", "eth_rx_dv", "eth_rxd0", "eth_rxd1",
	"eth_rxd2_rgmii", "eth_rxd3_rgmii", "eth_rgmii_tx_clk", "eth_txen",
	"eth_txd0", "eth_txd1", "eth_txd2_rgmii", "eth_txd3_rgmii",
	"eth_link_led_t18", "eth_act_led_t19", "eth_link_led_t21",
	"eth_act_led_t22"
};

static const char * const mic_mute_groups[] = {
	"mic_mute_en", "mic_mute_key"
};

static const char * const mclk_groups[] = {
	"mclk0_x6", "mclk0_d13", "mclk1_d13", "mclk1_t4",
	"mclk2_t5", "mclk2_t17"
};

static const char * const tdm_groups[] = {
	"tdm_d12_d8", "tdm_d13_d9", "tdm_d2_d12", "tdm_sclk1_d14",
	"tdm_fs1_d15", "tdm_d1", "tdm_d0_x8", "tdm_fs0_x9",
	"tdm_sclk0_x10", "tdm_d2_t0", "tdm_d3_t1", "tdm_fs1_t2",
	"tdm_sclk1_t3", "tdm_d4_t5", "tdm_d5_t6", "tdm_d6_t7",
	"tdm_d7", "tdm_fs2_t11", "tdm_sclk2_t12", "tdm_d8",
	"tdm_d9", "tdm_d10", "tdm_d11", "tdm_d12_t18",
	"tdm_d13_t19", "tdm_d4_t20", "tdm_d5_t21", "tdm_d6_t22",
	"tdm_d3_d11", "tdm_sclk0_d14", "tdm_fs0_d15", "tdm_fs2_t6",
	"tdm_sclk2_t7", "tdm_d0_t20", "tdm_fs0_t21", "tdm_sclk0_t22"
};

static const char * const spdif_in_groups[] = {
	"spdif_in_t9", "spdif_in_t10"
};

static const char * const spdif_out_groups[] = {
	"spdif_out"
};

static struct meson_pmx_func meson_a4_periphs_functions[] __initdata = {
	FUNCTION(gpio_periphs),
	FUNCTION(uart_a),
	FUNCTION(uart_b),
	FUNCTION(uart_d),
	FUNCTION(uart_e),
	FUNCTION(i2c0),
	FUNCTION(i2c1),
	FUNCTION(i2c2),
	FUNCTION(i2c3),
	FUNCTION(pwm_a),
	FUNCTION(pwm_b),
	FUNCTION(pwm_c),
	FUNCTION(pwm_d),
	FUNCTION(pwm_e),
	FUNCTION(pwm_f),
	FUNCTION(pwm_g),
	FUNCTION(pwm_h),
	FUNCTION(remote_out),
	FUNCTION(remote_in),
	FUNCTION(dcon_led),
	FUNCTION(spinf),
	FUNCTION(lcd),
	FUNCTION(jtag_1),
	FUNCTION(gen_clk),
	FUNCTION(clk12_24),
	FUNCTION(emmc),
	FUNCTION(nand),
	FUNCTION(spi_a),
	FUNCTION(spi_b),
	FUNCTION(pdm),
	FUNCTION(sdio),
	FUNCTION(eth),
	FUNCTION(mic_mute),
	FUNCTION(mclk),
	FUNCTION(tdm),
	FUNCTION(spdif_in),
	FUNCTION(spdif_out)
};

static struct meson_bank meson_a4_periphs_banks[] = {
	/* name  first  last  irq  pullen  pull  dir  out  in */
	BANK_DS("E",  GPIOE_0,  GPIOE_1,  14,  15,
		0x43,  0, 0x44,  0, 0x42,  0, 0x41,  0, 0x40,  0, 0x47,  0),
	BANK_DS("D",  GPIOD_0, GPIOD_15,  16, 31,
		0x33,  0, 0x34,  0, 0x32,  0, 0x31,  0, 0x30,  0, 0x37,  0),
	BANK_DS("B",  GPIOB_0, GPIOB_13, 0, 13,
		0x63,  0, 0x64,  0, 0x62,  0, 0x61,  0, 0x60,  0, 0x67,  0),
	BANK_DS("X",  GPIOX_0, GPIOX_17, 55, 72,
		0x13,  0, 0x14,  0, 0x12,  0, 0x11,  0, 0x10,  0, 0x17,  0),
	BANK_DS("T",  GPIOT_0, GPIOT_22, 32, 54,
		0x23,  0, 0x24,  0, 0x22,  0, 0x21,  0, 0x20,  0, 0x27,  0),
};

static struct meson_bank meson_a4_aobus_banks[] = {
	BANK_DS("AO", GPIOAO_0, GPIOAO_6,  0,  6,
		0x3,   0,  0x4,  0,   0x2,  0,  0x1,  0,  0x0,  0,  0x7, 0),
	BANK_DS("TEST_N", GPIO_TEST_N,    GPIO_TEST_N,   7, 7,
		0x13,  0,  0x14,  0,  0x12, 0,  0x11,  0, 0x10, 0, 0x17, 0),
};

static struct meson_pmx_bank meson_a4_periphs_pmx_banks[] = {
	/* name  first  lask  reg  offset */
	BANK_PMX("E",  GPIOE_0,  GPIOE_1, 0x12,  0),
	BANK_PMX("D",  GPIOD_0, GPIOD_15, 0x10,  0),
	BANK_PMX("B",  GPIOB_0, GPIOB_13, 0x00,  0),
	BANK_PMX("X",  GPIOX_0, GPIOX_17, 0x03,  0),
	BANK_PMX("T",  GPIOT_0, GPIOT_22, 0x0b,  0),
};

static struct meson_pmx_bank meson_a4_aobus_pmx_banks[] = {
	BANK_PMX("AO", GPIOAO_0, GPIOAO_6, 0x00,  0),
	BANK_PMX("TEST_N", GPIO_TEST_N, GPIO_TEST_N, 0x0,  28),
};

static struct meson_axg_pmx_data meson_a4_periphs_pmx_banks_data = {
	.pmx_banks	= meson_a4_periphs_pmx_banks,
	.num_pmx_banks	= ARRAY_SIZE(meson_a4_periphs_pmx_banks),
};

static struct meson_axg_pmx_data meson_a4_aobus_pmx_banks_data = {
	.pmx_banks	= meson_a4_aobus_pmx_banks,
	.num_pmx_banks	= ARRAY_SIZE(meson_a4_aobus_pmx_banks),
};

static struct meson_pinctrl_data meson_a4_periphs_pinctrl_data __refdata = {
	.name		= "periphs-banks",
	.pins		= meson_a4_periphs_pins,
	.groups		= meson_a4_periphs_groups,
	.funcs		= meson_a4_periphs_functions,
	.banks		= meson_a4_periphs_banks,
	.num_pins	= ARRAY_SIZE(meson_a4_periphs_pins),
	.num_groups	= ARRAY_SIZE(meson_a4_periphs_groups),
	.num_funcs	= ARRAY_SIZE(meson_a4_periphs_functions),
	.num_banks	= ARRAY_SIZE(meson_a4_periphs_banks),
	.pmx_ops	= &meson_axg_pmx_ops,
	.pmx_data	= &meson_a4_periphs_pmx_banks_data,
	.parse_dt	= &meson_a1_parse_dt_extra,
};

static struct meson_pinctrl_data meson_a4_aobus_pinctrl_data __refdata = {
	.name		= "aobus-banks",
	.pins		= meson_a4_aobus_pins,
	.groups		= meson_a4_aobus_groups,
	.funcs		= meson_a4_aobus_functions,
	.banks		= meson_a4_aobus_banks,
	.num_pins	= ARRAY_SIZE(meson_a4_aobus_pins),
	.num_groups	= ARRAY_SIZE(meson_a4_aobus_groups),
	.num_funcs	= ARRAY_SIZE(meson_a4_aobus_functions),
	.num_banks	= ARRAY_SIZE(meson_a4_aobus_banks),
	.pmx_ops	= &meson_axg_pmx_ops,
	.pmx_data	= &meson_a4_aobus_pmx_banks_data,
	.parse_dt	= &meson_a1_parse_dt_extra,
};

static const struct of_device_id meson_a4_pinctrl_dt_match[] = {
	{
		.compatible = "amlogic,meson-a4-periphs-pinctrl",
		.data = &meson_a4_periphs_pinctrl_data,
	},
	{
		.compatible = "amlogic,meson-a4-aobus-pinctrl",
		.data = &meson_a4_aobus_pinctrl_data,
	},
	{ },
};

static struct platform_driver meson_a4_pinctrl_driver = {
	.probe  = meson_pinctrl_probe,
	.driver = {
		.name	= "meson-a4-pinctrl",
	},
};

#ifndef MODULE
static int __init a4_pmx_init(void)
{
	meson_a4_pinctrl_driver.driver.of_match_table =
			meson_a4_pinctrl_dt_match;
	return platform_driver_register(&meson_a4_pinctrl_driver);
}
arch_initcall(a4_pmx_init);

#else
int __init meson_a4_pinctrl_init(void)
{
	meson_a4_pinctrl_driver.driver.of_match_table =
			meson_a4_pinctrl_dt_match;
	return platform_driver_register(&meson_a4_pinctrl_driver);
}
#endif
