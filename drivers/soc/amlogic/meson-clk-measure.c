// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2018 BayLibre, SAS
 * Author: Neil Armstrong <narmstrong@baylibre.com>
 */

#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/bitfield.h>
#include <linux/seq_file.h>
#include <linux/debugfs.h>
#include <linux/regmap.h>
#include <linux/amlogic/clk_measure.h>
#include <linux/module.h>

#define MSR_DURATION		GENMASK(15, 0)
#define MSR_ENABLE		BIT(16)
#define MSR_CONT		BIT(17) /* continuous measurement */
#define MSR_INTR		BIT(18) /* interrupts */
#define MSR_RUN			BIT(19)
#define MSR_CLK_SRC		GENMASK(27, 20)
#define MSR_BUSY		BIT(31)

#define MSR_VAL_MASK		GENMASK(19, 0)

#define DIV_MIN			32
#define DIV_STEP		32
#define DIV_MAX			640

/*
 * According to Datasheet, The max clk measure num
 * is 256, not 128, remove it
 */

struct meson_msr_id {
	struct meson_msr *priv;
	unsigned int id;
	const char *name;
};

struct meson_msr_data {
	struct meson_msr_id *msr_table;
	unsigned int table_size;
	unsigned int duty_offset;
	unsigned int reg0_offset;
	unsigned int reg1_offset;
	unsigned int reg2_offset;
};

struct meson_msr {
	struct regmap *regmap;
	struct meson_msr_data *data;
};

#define CLK_MSR_ID(__id, __name) \
	[__id] = {.id = __id, .name = __name,}

static spinlock_t measure_lock;
static struct meson_msr *glo_meson_msr;
static unsigned int measure_num;

static struct meson_msr_id clk_msr_m8[] __initdata = {
	CLK_MSR_ID(0, "ring_osc_out_ee0"),
	CLK_MSR_ID(1, "ring_osc_out_ee1"),
	CLK_MSR_ID(2, "ring_osc_out_ee2"),
	CLK_MSR_ID(3, "a9_ring_osck"),
	CLK_MSR_ID(6, "vid_pll"),
	CLK_MSR_ID(7, "clk81"),
	CLK_MSR_ID(8, "encp"),
	CLK_MSR_ID(9, "encl"),
	CLK_MSR_ID(11, "eth_rmii"),
	CLK_MSR_ID(13, "amclk"),
	CLK_MSR_ID(14, "fec_clk_0"),
	CLK_MSR_ID(15, "fec_clk_1"),
	CLK_MSR_ID(16, "fec_clk_2"),
	CLK_MSR_ID(18, "a9_clk_div16"),
	CLK_MSR_ID(19, "hdmi_sys"),
	CLK_MSR_ID(20, "rtc_osc_clk_out"),
	CLK_MSR_ID(21, "i2s_clk_in_src0"),
	CLK_MSR_ID(22, "clk_rmii_from_pad"),
	CLK_MSR_ID(23, "hdmi_ch0_tmds"),
	CLK_MSR_ID(24, "lvds_fifo"),
	CLK_MSR_ID(26, "sc_clk_int"),
	CLK_MSR_ID(28, "sar_adc"),
	CLK_MSR_ID(30, "mpll_clk_test_out"),
	CLK_MSR_ID(31, "audac_clkpi"),
	CLK_MSR_ID(32, "vdac"),
	CLK_MSR_ID(33, "sdhc_rx"),
	CLK_MSR_ID(34, "sdhc_sd"),
	CLK_MSR_ID(35, "mali"),
	CLK_MSR_ID(36, "hdmi_tx_pixel"),
	CLK_MSR_ID(38, "vdin_meas"),
	CLK_MSR_ID(39, "pcm_sclk"),
	CLK_MSR_ID(40, "pcm_mclk"),
	CLK_MSR_ID(41, "eth_rx_tx"),
	CLK_MSR_ID(42, "pwm_d"),
	CLK_MSR_ID(43, "pwm_c"),
	CLK_MSR_ID(44, "pwm_b"),
	CLK_MSR_ID(45, "pwm_a"),
	CLK_MSR_ID(46, "pcm2_sclk"),
	CLK_MSR_ID(47, "ddr_dpll_pt"),
	CLK_MSR_ID(48, "pwm_f"),
	CLK_MSR_ID(49, "pwm_e"),
	CLK_MSR_ID(59, "hcodec"),
	CLK_MSR_ID(60, "usb_32k_alt"),
	CLK_MSR_ID(61, "gpio"),
	CLK_MSR_ID(62, "vid2_pll"),
	CLK_MSR_ID(63, "mipi_csi_cfg"),
};

static struct meson_msr_id clk_msr_gx[] __initdata = {
	CLK_MSR_ID(0, "ring_osc_out_ee_0"),
	CLK_MSR_ID(1, "ring_osc_out_ee_1"),
	CLK_MSR_ID(2, "ring_osc_out_ee_2"),
	CLK_MSR_ID(3, "a53_ring_osc"),
	CLK_MSR_ID(4, "gp0_pll"),
	CLK_MSR_ID(6, "enci"),
	CLK_MSR_ID(7, "clk81"),
	CLK_MSR_ID(8, "encp"),
	CLK_MSR_ID(9, "encl"),
	CLK_MSR_ID(10, "vdac"),
	CLK_MSR_ID(11, "rgmii_tx"),
	CLK_MSR_ID(12, "pdm"),
	CLK_MSR_ID(13, "amclk"),
	CLK_MSR_ID(14, "fec_0"),
	CLK_MSR_ID(15, "fec_1"),
	CLK_MSR_ID(16, "fec_2"),
	CLK_MSR_ID(17, "sys_pll_div16"),
	CLK_MSR_ID(18, "sys_cpu_div16"),
	CLK_MSR_ID(19, "hdmitx_sys"),
	CLK_MSR_ID(20, "rtc_osc_out"),
	CLK_MSR_ID(21, "i2s_in_src0"),
	CLK_MSR_ID(22, "eth_phy_ref"),
	CLK_MSR_ID(23, "hdmi_todig"),
	CLK_MSR_ID(26, "sc_int"),
	CLK_MSR_ID(28, "sar_adc"),
	CLK_MSR_ID(31, "mpll_test_out"),
	CLK_MSR_ID(32, "vdec"),
	CLK_MSR_ID(35, "mali"),
	CLK_MSR_ID(36, "hdmi_tx_pixel"),
	CLK_MSR_ID(37, "i958"),
	CLK_MSR_ID(38, "vdin_meas"),
	CLK_MSR_ID(39, "pcm_sclk"),
	CLK_MSR_ID(40, "pcm_mclk"),
	CLK_MSR_ID(41, "eth_rx_or_rmii"),
	CLK_MSR_ID(42, "mp0_out"),
	CLK_MSR_ID(43, "fclk_div5"),
	CLK_MSR_ID(44, "pwm_b"),
	CLK_MSR_ID(45, "pwm_a"),
	CLK_MSR_ID(46, "vpu"),
	CLK_MSR_ID(47, "ddr_dpll_pt"),
	CLK_MSR_ID(48, "mp1_out"),
	CLK_MSR_ID(49, "mp2_out"),
	CLK_MSR_ID(50, "mp3_out"),
	CLK_MSR_ID(51, "nand_core"),
	CLK_MSR_ID(52, "sd_emmc_b"),
	CLK_MSR_ID(53, "sd_emmc_a"),
	CLK_MSR_ID(55, "vid_pll_div_out"),
	CLK_MSR_ID(56, "cci"),
	CLK_MSR_ID(57, "wave420l_c"),
	CLK_MSR_ID(58, "wave420l_b"),
	CLK_MSR_ID(59, "hcodec"),
	CLK_MSR_ID(60, "alt_32k"),
	CLK_MSR_ID(61, "gpio_msr"),
	CLK_MSR_ID(62, "hevc"),
	CLK_MSR_ID(66, "vid_lock"),
	CLK_MSR_ID(70, "pwm_f"),
	CLK_MSR_ID(71, "pwm_e"),
	CLK_MSR_ID(72, "pwm_d"),
	CLK_MSR_ID(73, "pwm_c"),
	CLK_MSR_ID(75, "aoclkx2_int"),
	CLK_MSR_ID(76, "aoclk_int"),
	CLK_MSR_ID(77, "rng_ring_osc_0"),
	CLK_MSR_ID(78, "rng_ring_osc_1"),
	CLK_MSR_ID(79, "rng_ring_osc_2"),
	CLK_MSR_ID(80, "rng_ring_osc_3"),
	CLK_MSR_ID(81, "vapb"),
	CLK_MSR_ID(82, "ge2d"),
};

static struct meson_msr_id clk_msr_axg[] __initdata = {
	CLK_MSR_ID(0, "ring_osc_out_ee_0"),
	CLK_MSR_ID(1, "ring_osc_out_ee_1"),
	CLK_MSR_ID(2, "ring_osc_out_ee_2"),
	CLK_MSR_ID(3, "a53_ring_osc"),
	CLK_MSR_ID(4, "gp0_pll"),
	CLK_MSR_ID(5, "gp1_pll"),
	CLK_MSR_ID(7, "clk81"),
	CLK_MSR_ID(9, "encl"),
	CLK_MSR_ID(17, "sys_pll_div16"),
	CLK_MSR_ID(18, "sys_cpu_div16"),
	CLK_MSR_ID(20, "rtc_osc_out"),
	CLK_MSR_ID(23, "mmc_clk"),
	CLK_MSR_ID(28, "sar_adc"),
	CLK_MSR_ID(31, "mpll_test_out"),
	CLK_MSR_ID(40, "mod_eth_tx_clk"),
	CLK_MSR_ID(41, "mod_eth_rx_clk_rmii"),
	CLK_MSR_ID(42, "mp0_out"),
	CLK_MSR_ID(43, "fclk_div5"),
	CLK_MSR_ID(44, "pwm_b"),
	CLK_MSR_ID(45, "pwm_a"),
	CLK_MSR_ID(46, "vpu"),
	CLK_MSR_ID(47, "ddr_dpll_pt"),
	CLK_MSR_ID(48, "mp1_out"),
	CLK_MSR_ID(49, "mp2_out"),
	CLK_MSR_ID(50, "mp3_out"),
	CLK_MSR_ID(51, "sd_emmm_c"),
	CLK_MSR_ID(52, "sd_emmc_b"),
	CLK_MSR_ID(61, "gpio_msr"),
	CLK_MSR_ID(66, "audio_slv_lrclk_c"),
	CLK_MSR_ID(67, "audio_slv_lrclk_b"),
	CLK_MSR_ID(68, "audio_slv_lrclk_a"),
	CLK_MSR_ID(69, "audio_slv_sclk_c"),
	CLK_MSR_ID(70, "audio_slv_sclk_b"),
	CLK_MSR_ID(71, "audio_slv_sclk_a"),
	CLK_MSR_ID(72, "pwm_d"),
	CLK_MSR_ID(73, "pwm_c"),
	CLK_MSR_ID(74, "wifi_beacon"),
	CLK_MSR_ID(75, "tdmin_lb_lrcl"),
	CLK_MSR_ID(76, "tdmin_lb_sclk"),
	CLK_MSR_ID(77, "rng_ring_osc_0"),
	CLK_MSR_ID(78, "rng_ring_osc_1"),
	CLK_MSR_ID(79, "rng_ring_osc_2"),
	CLK_MSR_ID(80, "rng_ring_osc_3"),
	CLK_MSR_ID(81, "vapb"),
	CLK_MSR_ID(82, "ge2d"),
	CLK_MSR_ID(84, "audio_resample"),
	CLK_MSR_ID(85, "audio_pdm_sys"),
	CLK_MSR_ID(86, "audio_spdifout"),
	CLK_MSR_ID(87, "audio_spdifin"),
	CLK_MSR_ID(88, "audio_lrclk_f"),
	CLK_MSR_ID(89, "audio_lrclk_e"),
	CLK_MSR_ID(90, "audio_lrclk_d"),
	CLK_MSR_ID(91, "audio_lrclk_c"),
	CLK_MSR_ID(92, "audio_lrclk_b"),
	CLK_MSR_ID(93, "audio_lrclk_a"),
	CLK_MSR_ID(94, "audio_sclk_f"),
	CLK_MSR_ID(95, "audio_sclk_e"),
	CLK_MSR_ID(96, "audio_sclk_d"),
	CLK_MSR_ID(97, "audio_sclk_c"),
	CLK_MSR_ID(98, "audio_sclk_b"),
	CLK_MSR_ID(99, "audio_sclk_a"),
	CLK_MSR_ID(100, "audio_mclk_f"),
	CLK_MSR_ID(101, "audio_mclk_e"),
	CLK_MSR_ID(102, "audio_mclk_d"),
	CLK_MSR_ID(103, "audio_mclk_c"),
	CLK_MSR_ID(104, "audio_mclk_b"),
	CLK_MSR_ID(105, "audio_mclk_a"),
	CLK_MSR_ID(106, "pcie_refclk_n"),
	CLK_MSR_ID(107, "pcie_refclk_p"),
	CLK_MSR_ID(108, "audio_locker_out"),
	CLK_MSR_ID(109, "audio_locker_in"),
};

static struct meson_msr_id clk_msr_g12a[] __initdata = {
	CLK_MSR_ID(0, "ring_osc_out_ee_0"),
	CLK_MSR_ID(1, "ring_osc_out_ee_1"),
	CLK_MSR_ID(2, "ring_osc_out_ee_2"),
	CLK_MSR_ID(3, "sys_cpu_ring_osc"),
	CLK_MSR_ID(4, "gp0_pll"),
	CLK_MSR_ID(6, "enci"),
	CLK_MSR_ID(7, "clk81"),
	CLK_MSR_ID(8, "encp"),
	CLK_MSR_ID(9, "encl"),
	CLK_MSR_ID(10, "vdac"),
	CLK_MSR_ID(11, "eth_tx"),
	CLK_MSR_ID(12, "hifi_pll"),
	CLK_MSR_ID(13, "mod_tcon"),
	CLK_MSR_ID(14, "fec_0"),
	CLK_MSR_ID(15, "fec_1"),
	CLK_MSR_ID(16, "fec_2"),
	CLK_MSR_ID(17, "sys_pll_div16"),
	CLK_MSR_ID(18, "sys_cpu_div16"),
	CLK_MSR_ID(19, "lcd_an_ph2"),
	CLK_MSR_ID(20, "rtc_osc_out"),
	CLK_MSR_ID(21, "lcd_an_ph3"),
	CLK_MSR_ID(22, "eth_phy_ref"),
	CLK_MSR_ID(23, "mpll_50m"),
	CLK_MSR_ID(24, "eth_125m"),
	CLK_MSR_ID(25, "eth_rmii"),
	CLK_MSR_ID(26, "sc_int"),
	CLK_MSR_ID(27, "in_mac"),
	CLK_MSR_ID(28, "sar_adc"),
	CLK_MSR_ID(29, "pcie_inp"),
	CLK_MSR_ID(30, "pcie_inn"),
	CLK_MSR_ID(31, "mpll_test_out"),
	CLK_MSR_ID(32, "vdec"),
	CLK_MSR_ID(33, "sys_cpu_ring_osc_1"),
	CLK_MSR_ID(34, "eth_mpll_50m"),
	CLK_MSR_ID(35, "mali"),
	CLK_MSR_ID(36, "hdmi_tx_pixel"),
	CLK_MSR_ID(37, "cdac"),
	CLK_MSR_ID(38, "vdin_meas"),
	CLK_MSR_ID(39, "bt656"),
	CLK_MSR_ID(41, "eth_rx_or_rmii"),
	CLK_MSR_ID(42, "mp0_out"),
	CLK_MSR_ID(43, "fclk_div5"),
	CLK_MSR_ID(44, "pwm_b"),
	CLK_MSR_ID(45, "pwm_a"),
	CLK_MSR_ID(46, "vpu"),
	CLK_MSR_ID(47, "ddr_dpll_pt"),
	CLK_MSR_ID(48, "mp1_out"),
	CLK_MSR_ID(49, "mp2_out"),
	CLK_MSR_ID(50, "mp3_out"),
	CLK_MSR_ID(51, "sd_emmc_c"),
	CLK_MSR_ID(52, "sd_emmc_b"),
	CLK_MSR_ID(53, "sd_emmc_a"),
	CLK_MSR_ID(54, "vpu_clkc"),
	CLK_MSR_ID(55, "vid_pll_div_out"),
	CLK_MSR_ID(56, "wave420l_a"),
	CLK_MSR_ID(57, "wave420l_c"),
	CLK_MSR_ID(58, "wave420l_b"),
	CLK_MSR_ID(59, "hcodec"),
	CLK_MSR_ID(61, "gpio_msr"),
	CLK_MSR_ID(62, "hevcb"),
	CLK_MSR_ID(63, "dsi_meas"),
	CLK_MSR_ID(64, "spicc_1"),
	CLK_MSR_ID(65, "spicc_0"),
	CLK_MSR_ID(66, "vid_lock"),
	CLK_MSR_ID(67, "dsi_phy"),
	CLK_MSR_ID(68, "hdcp22_esm"),
	CLK_MSR_ID(69, "hdcp22_skp"),
	CLK_MSR_ID(70, "pwm_f"),
	CLK_MSR_ID(71, "pwm_e"),
	CLK_MSR_ID(72, "pwm_d"),
	CLK_MSR_ID(73, "pwm_c"),
	CLK_MSR_ID(75, "hevcf"),
	CLK_MSR_ID(77, "rng_ring_osc_0"),
	CLK_MSR_ID(78, "rng_ring_osc_1"),
	CLK_MSR_ID(79, "rng_ring_osc_2"),
	CLK_MSR_ID(80, "rng_ring_osc_3"),
	CLK_MSR_ID(81, "vapb"),
	CLK_MSR_ID(82, "ge2d"),
	CLK_MSR_ID(83, "co_rx"),
	CLK_MSR_ID(84, "co_tx"),
	CLK_MSR_ID(89, "hdmi_todig"),
	CLK_MSR_ID(90, "hdmitx_sys"),
	CLK_MSR_ID(91, "sys_cpub_div16"),
	CLK_MSR_ID(92, "sys_pll_cpub_div16"),
	CLK_MSR_ID(94, "eth_phy_rx"),
	CLK_MSR_ID(95, "eth_phy_pll"),
	CLK_MSR_ID(96, "vpu_b"),
	CLK_MSR_ID(97, "cpu_b_tmp"),
	CLK_MSR_ID(98, "ts"),
	CLK_MSR_ID(99, "ring_osc_out_ee_3"),
	CLK_MSR_ID(100, "ring_osc_out_ee_4"),
	CLK_MSR_ID(101, "ring_osc_out_ee_5"),
	CLK_MSR_ID(102, "ring_osc_out_ee_6"),
	CLK_MSR_ID(103, "ring_osc_out_ee_7"),
	CLK_MSR_ID(104, "ring_osc_out_ee_8"),
	CLK_MSR_ID(105, "ring_osc_out_ee_9"),
	CLK_MSR_ID(106, "ephy_test"),
	CLK_MSR_ID(107, "au_dac_g128x"),
	CLK_MSR_ID(108, "audio_locker_out"),
	CLK_MSR_ID(109, "audio_locker_in"),
	CLK_MSR_ID(110, "audio_tdmout_c_sclk"),
	CLK_MSR_ID(111, "audio_tdmout_b_sclk"),
	CLK_MSR_ID(112, "audio_tdmout_a_sclk"),
	CLK_MSR_ID(113, "audio_tdmin_lb_sclk"),
	CLK_MSR_ID(114, "audio_tdmin_c_sclk"),
	CLK_MSR_ID(115, "audio_tdmin_b_sclk"),
	CLK_MSR_ID(116, "audio_tdmin_a_sclk"),
	CLK_MSR_ID(117, "audio_resample"),
	CLK_MSR_ID(118, "audio_pdm_sys"),
	CLK_MSR_ID(119, "audio_spdifout_b"),
	CLK_MSR_ID(120, "audio_spdifout"),
	CLK_MSR_ID(121, "audio_spdifin"),
	CLK_MSR_ID(122, "audio_pdm_dclk"),
};

static struct meson_msr_id clk_msr_sm1[] __initdata = {
	CLK_MSR_ID(0, "ring_osc_out_ee_0"),
	CLK_MSR_ID(1, "ring_osc_out_ee_1"),
	CLK_MSR_ID(2, "ring_osc_out_ee_2"),
	CLK_MSR_ID(3, "ring_osc_out_ee_3"),
	CLK_MSR_ID(4, "gp0_pll"),
	CLK_MSR_ID(5, "gp1_pll"),
	CLK_MSR_ID(6, "enci"),
	CLK_MSR_ID(7, "clk81"),
	CLK_MSR_ID(8, "encp"),
	CLK_MSR_ID(9, "encl"),
	CLK_MSR_ID(10, "vdac"),
	CLK_MSR_ID(11, "eth_tx"),
	CLK_MSR_ID(12, "hifi_pll"),
	CLK_MSR_ID(13, "mod_tcon"),
	CLK_MSR_ID(14, "fec_0"),
	CLK_MSR_ID(15, "fec_1"),
	CLK_MSR_ID(16, "fec_2"),
	CLK_MSR_ID(17, "sys_pll_div16"),
	CLK_MSR_ID(18, "sys_cpu_div16"),
	CLK_MSR_ID(19, "lcd_an_ph2"),
	CLK_MSR_ID(20, "rtc_osc_out"),
	CLK_MSR_ID(21, "lcd_an_ph3"),
	CLK_MSR_ID(22, "eth_phy_ref"),
	CLK_MSR_ID(23, "mpll_50m"),
	CLK_MSR_ID(24, "eth_125m"),
	CLK_MSR_ID(25, "eth_rmii"),
	CLK_MSR_ID(26, "sc_int"),
	CLK_MSR_ID(27, "in_mac"),
	CLK_MSR_ID(28, "sar_adc"),
	CLK_MSR_ID(29, "pcie_inp"),
	CLK_MSR_ID(30, "pcie_inn"),
	CLK_MSR_ID(31, "mpll_test_out"),
	CLK_MSR_ID(32, "vdec"),
	CLK_MSR_ID(34, "eth_mpll_50m"),
	CLK_MSR_ID(35, "mali"),
	CLK_MSR_ID(36, "hdmi_tx_pixel"),
	CLK_MSR_ID(37, "cdac"),
	CLK_MSR_ID(38, "vdin_meas"),
	CLK_MSR_ID(39, "bt656"),
	CLK_MSR_ID(40, "arm_ring_osc_out_4"),
	CLK_MSR_ID(41, "eth_rx_or_rmii"),
	CLK_MSR_ID(42, "mp0_out"),
	CLK_MSR_ID(43, "fclk_div5"),
	CLK_MSR_ID(44, "pwm_b"),
	CLK_MSR_ID(45, "pwm_a"),
	CLK_MSR_ID(46, "vpu"),
	CLK_MSR_ID(47, "ddr_dpll_pt"),
	CLK_MSR_ID(48, "mp1_out"),
	CLK_MSR_ID(49, "mp2_out"),
	CLK_MSR_ID(50, "mp3_out"),
	CLK_MSR_ID(51, "sd_emmc_c"),
	CLK_MSR_ID(52, "sd_emmc_b"),
	CLK_MSR_ID(53, "sd_emmc_a"),
	CLK_MSR_ID(54, "vpu_clkc"),
	CLK_MSR_ID(55, "vid_pll_div_out"),
	CLK_MSR_ID(56, "wave420l_a"),
	CLK_MSR_ID(57, "wave420l_c"),
	CLK_MSR_ID(58, "wave420l_b"),
	CLK_MSR_ID(59, "hcodec"),
	CLK_MSR_ID(60, "arm_ring_osc_out_5"),
	CLK_MSR_ID(61, "gpio_msr"),
	CLK_MSR_ID(62, "hevcb"),
	CLK_MSR_ID(63, "dsi_meas"),
	CLK_MSR_ID(64, "spicc_1"),
	CLK_MSR_ID(65, "spicc_0"),
	CLK_MSR_ID(66, "vid_lock"),
	CLK_MSR_ID(67, "dsi_phy"),
	CLK_MSR_ID(68, "hdcp22_esm"),
	CLK_MSR_ID(69, "hdcp22_skp"),
	CLK_MSR_ID(70, "pwm_f"),
	CLK_MSR_ID(71, "pwm_e"),
	CLK_MSR_ID(72, "pwm_d"),
	CLK_MSR_ID(73, "pwm_c"),
	CLK_MSR_ID(74, "arm_ring_osc_out_6"),
	CLK_MSR_ID(75, "hevcf"),
	CLK_MSR_ID(76, "arm_ring_osc_out_7"),
	CLK_MSR_ID(77, "rng_ring_osc_0"),
	CLK_MSR_ID(78, "rng_ring_osc_1"),
	CLK_MSR_ID(79, "rng_ring_osc_2"),
	CLK_MSR_ID(80, "rng_ring_osc_3"),
	CLK_MSR_ID(81, "vapb"),
	CLK_MSR_ID(82, "ge2d"),
	CLK_MSR_ID(83, "co_rx"),
	CLK_MSR_ID(84, "co_tx"),
	CLK_MSR_ID(85, "arm_ring_osc_out_8"),
	CLK_MSR_ID(86, "arm_ring_osc_out_9"),
	CLK_MSR_ID(87, "mipi_dsi_phy"),
	CLK_MSR_ID(88, "cis2_adapt"),
	CLK_MSR_ID(89, "hdmi_todig"),
	CLK_MSR_ID(90, "hdmitx_sys"),
	CLK_MSR_ID(91, "nna_core"),
	CLK_MSR_ID(92, "nna_axi"),
	CLK_MSR_ID(93, "vad"),
	CLK_MSR_ID(94, "eth_phy_rx"),
	CLK_MSR_ID(95, "eth_phy_pll"),
	CLK_MSR_ID(96, "vpu_b"),
	CLK_MSR_ID(97, "cpu_b_tmp"),
	CLK_MSR_ID(98, "ts"),
	CLK_MSR_ID(99, "arm_ring_osc_out_10"),
	CLK_MSR_ID(100, "arm_ring_osc_out_11"),
	CLK_MSR_ID(101, "arm_ring_osc_out_12"),
	CLK_MSR_ID(102, "arm_ring_osc_out_13"),
	CLK_MSR_ID(103, "arm_ring_osc_out_14"),
	CLK_MSR_ID(104, "arm_ring_osc_out_15"),
	CLK_MSR_ID(105, "arm_ring_osc_out_16"),
	CLK_MSR_ID(106, "ephy_test"),
	CLK_MSR_ID(107, "au_dac_g128x"),
	CLK_MSR_ID(108, "audio_locker_out"),
	CLK_MSR_ID(109, "audio_locker_in"),
	CLK_MSR_ID(110, "audio_tdmout_c_sclk"),
	CLK_MSR_ID(111, "audio_tdmout_b_sclk"),
	CLK_MSR_ID(112, "audio_tdmout_a_sclk"),
	CLK_MSR_ID(113, "audio_tdmin_lb_sclk"),
	CLK_MSR_ID(114, "audio_tdmin_c_sclk"),
	CLK_MSR_ID(115, "audio_tdmin_b_sclk"),
	CLK_MSR_ID(116, "audio_tdmin_a_sclk"),
	CLK_MSR_ID(117, "audio_resample"),
	CLK_MSR_ID(118, "audio_pdm_sys"),
	CLK_MSR_ID(119, "audio_spdifout_b"),
	CLK_MSR_ID(120, "audio_spdifout"),
	CLK_MSR_ID(121, "audio_spdifin"),
	CLK_MSR_ID(122, "audio_pdm_dclk"),
	CLK_MSR_ID(123, "audio_resampled"),
	CLK_MSR_ID(124, "earcrx_pll"),
	CLK_MSR_ID(125, "earcrx_pll_test"),
	CLK_MSR_ID(126, "csi_phy0"),
	CLK_MSR_ID(127, "csi2_data"),
};

static struct meson_msr_id clk_msr_tm2[] __initdata = {
	CLK_MSR_ID(0, "am_ring_osc_clk_out_ee[0]"),
	CLK_MSR_ID(1, "am_ring_osc_clk_out_ee[1]"),
	CLK_MSR_ID(2, "am_ring_osc_clk_out_ee[2]"),
	CLK_MSR_ID(3, "sys_cpu_ring_osc_clk[0]"),
	CLK_MSR_ID(4, "gp0_pll_clk"),
	CLK_MSR_ID(5, "gp1_pll_clk"),
	CLK_MSR_ID(6, "cts_enci_clk"),
	CLK_MSR_ID(7, "clk81"),
	CLK_MSR_ID(8, "cts_encp_clk"),
	CLK_MSR_ID(9, "cts_encl_clk"),
	CLK_MSR_ID(10, "cts_vdac_clk"),
	CLK_MSR_ID(11, "mac_eth_tx_clk"),
	CLK_MSR_ID(12, "hifi_pll_clk"),
	CLK_MSR_ID(13, "mod_tcon_clko"),
	CLK_MSR_ID(14, "cts_FEC_CLK_0"),
	CLK_MSR_ID(15, "cts_FEC_CLK_1"),
	CLK_MSR_ID(16, "cts_FEC_CLK_2"),
	CLK_MSR_ID(17, "sys_pll_div16"),
	CLK_MSR_ID(18, "sys_cpu_clk_div16"),
	CLK_MSR_ID(19, "lcd_an_clk_ph2"),
	CLK_MSR_ID(20, "rtc_osc_clk_out"),
	CLK_MSR_ID(21, "lcd_an_clk_ph3"),
	CLK_MSR_ID(22, "mac_eth_phy_ref_clk"),
	CLK_MSR_ID(23, "mpll_clk_50m"),
	CLK_MSR_ID(24, "cts_eth_clk125Mhz"),
	CLK_MSR_ID(25, "cts_eth_clk_rmii"),
	CLK_MSR_ID(26, "sc_clk_int"),
	CLK_MSR_ID(27, "co_clkin_to_mac"),
	CLK_MSR_ID(28, "cts_sar_adc_clk"),
	CLK_MSR_ID(29, "hdmirx_apll_clk_out_div"),
	CLK_MSR_ID(30, "hdmirx_cable_clk"),
	CLK_MSR_ID(31, "mpll_clk_test_out"),
	CLK_MSR_ID(32, "cts_vdec_clk"),
	CLK_MSR_ID(33, "sys_cpu_ring_osc_clk[1]"),
	CLK_MSR_ID(34, "eth_mppll_50m_ckout"),
	CLK_MSR_ID(35, "cts_mali_clk"),
	CLK_MSR_ID(36, "cts_hdmi_tx_pixel_clk"),
	CLK_MSR_ID(37, "cts_cdac_clk_c"),
	CLK_MSR_ID(38, "cts_vdin_meas_clk"),
	CLK_MSR_ID(39, "cts_bt656_clk0"),
	CLK_MSR_ID(40, "cts_hdmirx_cfg_clk"),
	CLK_MSR_ID(41, "mac_eth_rx_clk_rmii"),
	CLK_MSR_ID(42, "mp0_clk_out"),
	CLK_MSR_ID(43, "fclk_div5"),
	CLK_MSR_ID(44, "cts_pwm_B_clk"),
	CLK_MSR_ID(45, "cts_pwm_A_clk"),
	CLK_MSR_ID(46, "cts_vpu_clk"),
	CLK_MSR_ID(47, "ddr_dpll_pt_clk"),
	CLK_MSR_ID(48, "mp1_clk_out"),
	CLK_MSR_ID(49, "mp2_clk_out"),
	CLK_MSR_ID(50, "mp3_clk_out"),
	CLK_MSR_ID(51, "cts_sd_emmc_clk_C"),
	CLK_MSR_ID(52, "cts_sd_emmc_clk_B"),
	CLK_MSR_ID(53, "ddr_2xclk"),
	CLK_MSR_ID(54, "cts_vpu_clkc"),
	CLK_MSR_ID(55, "vid_pll_div_clk_out"),
	CLK_MSR_ID(56, "cts_atv_dmd_sys_clk"),
	CLK_MSR_ID(57, "cts_atv_dmd_vdac_clk"),
	CLK_MSR_ID(58, "cts_vafe_datack"),
	CLK_MSR_ID(59, "cts_hcodec_clk"),
	CLK_MSR_ID(60, "cts_hdmirx_aud_pll_clk"),
	CLK_MSR_ID(61, "gpio_clk_msr"),
	CLK_MSR_ID(62, "cts_hevcb_clk"),
	CLK_MSR_ID(63, "hdmirx_tmds_clk"),
	CLK_MSR_ID(64, "cts_spicc_1_clk"),
	CLK_MSR_ID(65, "cts_spicc_0_clk"),
	CLK_MSR_ID(66, "cts_vid_lock_clk"),
	CLK_MSR_ID(67, "hdmirx_apll_clk_audio"),
	CLK_MSR_ID(68, "cts_hdcp22_esmclk"),
	CLK_MSR_ID(69, "cts_hdcp22_skpclk"),
	CLK_MSR_ID(70, "cts_pwm_F_clk"),
	CLK_MSR_ID(71, "cts_pwm_E_clk"),
	CLK_MSR_ID(72, "cts_pwm_D_clk"),
	CLK_MSR_ID(73, "cts_pwm_C_clk"),
	CLK_MSR_ID(74, "hdmirx_aud_pll_clk"),
	CLK_MSR_ID(75, "cts_hevcf_clk"),
	CLK_MSR_ID(76, "hdmix_aud_clk"),
	CLK_MSR_ID(77, "rng_ring_osc_clk[0]"),
	CLK_MSR_ID(78, "rng_ring_osc_clk[1]"),
	CLK_MSR_ID(79, "rng_ring_osc_clk[2]"),
	CLK_MSR_ID(80, "rng_ring_osc_clk[3]"),
	CLK_MSR_ID(81, "cts_vapbclk"),
	CLK_MSR_ID(82, "cts_ge2d_clk"),
	CLK_MSR_ID(83, "co_rx_clk"),
	CLK_MSR_ID(84, "co_tx_cl"),
	CLK_MSR_ID(85, "cts_hdmirx_acr_ref_clk"),
	CLK_MSR_ID(86, "cts_hdmirx_modet_clk"),
	CLK_MSR_ID(87, "hdmitx_tmds_clk"),
	CLK_MSR_ID(88, "cts_hdmirx_meter_clk"),
	CLK_MSR_ID(89, "am_ring_osc_clk_out_ee[10]"),
	CLK_MSR_ID(90, "am_ring_osc_clk_out_ee[11]"),
	CLK_MSR_ID(91, "hdmirx_audmeas_clk"),
	CLK_MSR_ID(92, "sys_cpu_ring_osc_clk[2]"),
	CLK_MSR_ID(93, "sys_cpu_ring_osc_clk[3]"),
	CLK_MSR_ID(94, "eth_phy_exclk"),
	CLK_MSR_ID(95, "eth_phy_plltxclk"),
	CLK_MSR_ID(96, "cts_vpu_clkb"),
	CLK_MSR_ID(97, "cts_vpu_clkb_tmp"),
	CLK_MSR_ID(98, "cts_ts_clk"),
	CLK_MSR_ID(99, "am_ring_osc_clk_out_ee[3]"),
	CLK_MSR_ID(100, "am_ring_osc_clk_out_ee[4]"),
	CLK_MSR_ID(101, "am_ring_osc_clk_out_ee[5]"),
	CLK_MSR_ID(102, "am_ring_osc_clk_out_ee[6]"),
	CLK_MSR_ID(103, "am_ring_osc_clk_out_ee[7]"),
	CLK_MSR_ID(104, "am_ring_osc_clk_out_ee[8]"),
	CLK_MSR_ID(105, "am_ring_osc_clk_out_ee[9]"),
	CLK_MSR_ID(106, "ephy_test_clk"),
	CLK_MSR_ID(107, "au_dac_clk_g128x"),
	CLK_MSR_ID(108, "acodec_i2sout_bclk"),
	CLK_MSR_ID(109, "o_vad_clk"),
	CLK_MSR_ID(110, "audio_tdmout_c_sclk"),
	CLK_MSR_ID(111, "audio_tdmout_b_sclk"),
	CLK_MSR_ID(112, "audio_tdmout_a_sclk"),
	CLK_MSR_ID(113, "audio_tdmin_lb_sclk"),
	CLK_MSR_ID(114, "audio_tdmin_c_sclk"),
	CLK_MSR_ID(115, "audio_tdmin_b_sclk"),
	CLK_MSR_ID(116, "audio_tdmin_a_sclk"),
	CLK_MSR_ID(117, "audio_resampleb_clk"),
	CLK_MSR_ID(118, "audio_resamplea_clk"),
	CLK_MSR_ID(119, "audio_pdm_sysclk"),
	CLK_MSR_ID(120, "audio_spdifout_b_mst_clk"),
	CLK_MSR_ID(121, "audio_spdifout_mst_clk"),
	CLK_MSR_ID(122,	"audio_spdifin_mst_clk"),
	CLK_MSR_ID(123,	"mod_audio_pdm_dclk_o"),
	CLK_MSR_ID(124,	"cts_demod_core_clk"),
	CLK_MSR_ID(125, "cts_hdmi_axi_clk"),
	CLK_MSR_ID(126, "sar_ring_osc_clk"),
	CLK_MSR_ID(127, "hdmirx_vid_clk"),
	CLK_MSR_ID(128, "cts_tcon_pll_clk"),
	CLK_MSR_ID(129, "lvds_fifo_clk"),
	CLK_MSR_ID(130, "au_dac2_clk_gf128x"),
	CLK_MSR_ID(131, "ts_sar_clk"),
	CLK_MSR_ID(132, "audio_toacode_mclk"),
	CLK_MSR_ID(133, "atv_dmd_mono_clk_32"),
	CLK_MSR_ID(134, "adc_extclk_in"),
	CLK_MSR_ID(135, "tvfe_sample_clk"),
	CLK_MSR_ID(136, "vipnanoq_axi_clk"),
	CLK_MSR_ID(137, "atv_dmd_i2c_sclk"),
	CLK_MSR_ID(138, "vipnanoq_core_clk"),
	CLK_MSR_ID(139, "aud_adc_clk_g128x"),
	CLK_MSR_ID(140, "audio_toacodec_bclk"),
	CLK_MSR_ID(141, "ts_ddr_clk"),
	CLK_MSR_ID(142, "demode_ts_clk"),
	CLK_MSR_ID(143, "mainclk"),
	CLK_MSR_ID(144, "ts_pll_clk"),
	CLK_MSR_ID(145, "hdmitx_sys_clk"),
	CLK_MSR_ID(146, "pcie0_clk_inn"),
	CLK_MSR_ID(147, "pcie0_clk_inp"),
	CLK_MSR_ID(148, "dpll_clk_b3"),
	CLK_MSR_ID(149, "dpll_clk_b2"),
	CLK_MSR_ID(150, "dpll_clk_a2"),
	CLK_MSR_ID(151, "dpll_intclk"),
	CLK_MSR_ID(152, "c_alocker_in_clk"),
	CLK_MSR_ID(153, "c_alocker_out_clk"),
	CLK_MSR_ID(154, "p20_usb2_clkout"),
	CLK_MSR_ID(155, "p21_usb2_clkout"),
	CLK_MSR_ID(156, "p22_usb2_clkout"),
	CLK_MSR_ID(157, "vpu_dmc_clk"),
	CLK_MSR_ID(159, "dspa_clk"),
	CLK_MSR_ID(160, "dspb_clk"),
	CLK_MSR_ID(161, "audio_t0_hdmitx_spdif_clk"),
	CLK_MSR_ID(162, "audio_t0_hdmitx_bclk"),
	CLK_MSR_ID(163, "hdmirx_aud_sck"),
	CLK_MSR_ID(164, "au_dac2r_en_dac_clk"),
	CLK_MSR_ID(165, "au_dac2l_en_dac_clk"),
	CLK_MSR_ID(166, "au_dac1r_en_dac_clk"),
	CLK_MSR_ID(167, "au_dac1l_en_dac_clk"),
	CLK_MSR_ID(168, "pcie1_phy_bs_clk"),
	CLK_MSR_ID(169, "pcie0_phy_bs_clk"),
	CLK_MSR_ID(170, "pcie1_clk_inp"),
	CLK_MSR_ID(171, "pcie1_clk_inn"),
};

static struct meson_msr_id clk_msr_sc2[] __initdata = {
	CLK_MSR_ID(0, "cts_sys_clk"),
	CLK_MSR_ID(1, "cts_axi_clk"),
	CLK_MSR_ID(2, "cts_rtc_clk"),
	CLK_MSR_ID(3, "cts_dspa_clk"),
	CLK_MSR_ID(5, "cts_mali_clk"),
	CLK_MSR_ID(6, "sys_cpu_clk_div16"),
	CLK_MSR_ID(7, "cts_ceca_clk"),
	CLK_MSR_ID(8, "cts_cecb_clk"),
	CLK_MSR_ID(10, "fclk_div5"),
	CLK_MSR_ID(11, "mp0_clk_out"),
	CLK_MSR_ID(12, "mp1_clk_out"),
	CLK_MSR_ID(13, "mp2_clk_out"),
	CLK_MSR_ID(14, "mp3_clk_out"),
	CLK_MSR_ID(15, "mpll_clk_50m"),
	CLK_MSR_ID(16, "pcie_clk_inp"),
	CLK_MSR_ID(17, "pcie_clk_inn"),
	CLK_MSR_ID(18, "mpll_clk_test_out"),
	CLK_MSR_ID(19, "hifi_pll_clk"),
	CLK_MSR_ID(20, "gp0_pll_clk"),
	CLK_MSR_ID(21, "gp1_pll_clk"),
	CLK_MSR_ID(22, "eth_mppll_50m_ckout"),
	CLK_MSR_ID(23, "sys_pll_div16"),
	CLK_MSR_ID(24, "ddr_dpll_pt_clk"),
	CLK_MSR_ID(25, "earcrx_pll_ckout"),
	CLK_MSR_ID(30, "mod_eth_phy_ref_clk"),
	CLK_MSR_ID(31, "mod_eth_tx_clk"),
	CLK_MSR_ID(32, "cts_eth_clk125Mhz"),
	CLK_MSR_ID(33, "cts_eth_clk_rmii"),
	CLK_MSR_ID(34, "co_clkin_to_mac"),
	CLK_MSR_ID(35, "mod_eth_rx_clk_rmii"),
	CLK_MSR_ID(36, "co_rx_clk"),
	CLK_MSR_ID(37, "co_tx_clk"),
	CLK_MSR_ID(38, "eth_phy_rxclk"),
	CLK_MSR_ID(39, "eth_phy_plltxclk"),
	CLK_MSR_ID(40, "ephy_test_clk"),
	CLK_MSR_ID(50, "vid_pll_div_clk_out"),
	CLK_MSR_ID(51, "cts_enci_clk"),
	CLK_MSR_ID(52, "cts_encp_clk"),
	CLK_MSR_ID(53, "cts_encl_clk"),
	CLK_MSR_ID(54, "cts_vdac_clk"),
	CLK_MSR_ID(55, "cts_cdac_clk_c"),
	CLK_MSR_ID(56, "mod_tcon_clko"),
	CLK_MSR_ID(57, "lcd_an_clk_ph2"),
	CLK_MSR_ID(58, "lcd_an_clk_ph3"),
	CLK_MSR_ID(59, "cts_hdmi_tx_pixel_clk"),
	CLK_MSR_ID(60, "cts_vdin_meas_clk"),
	CLK_MSR_ID(61, "cts_vpu_clk"),
	CLK_MSR_ID(62, "cts_vpu_clkb"),
	CLK_MSR_ID(63, "cts_vpu_clkb_tmp"),
	CLK_MSR_ID(64, "cts_vpu_clkc"),
	CLK_MSR_ID(65, "cts_vid_lock_clk"),
	CLK_MSR_ID(66, "cts_vapbclk"),
	CLK_MSR_ID(67, "cts_ge2d_clk"),
	CLK_MSR_ID(68, "cts_hdcp22_esmclk"),
	CLK_MSR_ID(69, "cts_hdcp22_skpclk"),
	CLK_MSR_ID(76, "hdmitx_tmds_clk"),
	CLK_MSR_ID(77, "cts_hdmitx_sys_clk"),
	CLK_MSR_ID(78, "cts_hdmitx_fe_clk"),
	CLK_MSR_ID(79, "cts_rama_clk"),
	CLK_MSR_ID(93, "cts_vdec_clk"),
	CLK_MSR_ID(94, "cts_wave420_aclk"),
	CLK_MSR_ID(95, "cts_wave420_cclk"),
	CLK_MSR_ID(96, "cts_wave420_bclk"),
	CLK_MSR_ID(97, "cts_hcodec_clk"),
	CLK_MSR_ID(98, "cts_hevcb_clk"),
	CLK_MSR_ID(99, "cts_hevcf_clk"),
	CLK_MSR_ID(110, "cts_sc_clk(smartcard)"),
	CLK_MSR_ID(111, "cts_sar_adc_clk"),
	CLK_MSR_ID(113, "cts_sd_emmc_C_clk(nand)"),
	CLK_MSR_ID(114, "cts_sd_emmc_B_clk"),
	CLK_MSR_ID(115, "cts_sd_emmc_A_clk"),
	CLK_MSR_ID(116, "gpio_msr_clk"),
	CLK_MSR_ID(117, "cts_spicc_1_clk"),
	CLK_MSR_ID(118, "cts_spicc_0_clk"),
	CLK_MSR_ID(121, "cts_ts_clk(temp sensor)"),
	CLK_MSR_ID(130, "audio_vad_clk"),
	CLK_MSR_ID(131, "acodec_dac_clk_x128"),
	CLK_MSR_ID(132, "audio_locker_out_clk"),
	CLK_MSR_ID(133, "audio_locker_in_clk"),
	CLK_MSR_ID(134, "audio_tdmout_c_sclk"),
	CLK_MSR_ID(135, "audio_tdmout_b_sclk"),
	CLK_MSR_ID(136, "audio_tdmout_a_sclk"),
	CLK_MSR_ID(137, "audio_tdmin_lb_sclk"),
	CLK_MSR_ID(138, "audio_tdmin_c_sclk"),
	CLK_MSR_ID(139, "audio_tdmin_b_sclk"),
	CLK_MSR_ID(140, "audio_tdmin_a_sclk"),
	CLK_MSR_ID(141, "audio_resamplea_clk"),
	CLK_MSR_ID(142, "audio_pdm_sysclk"),
	CLK_MSR_ID(143, "audio_spdifoutb_mst_clk"),
	CLK_MSR_ID(144, "audio_spdifout_mst_clk"),
	CLK_MSR_ID(145, "audio_spdifin_mst_clk"),
	CLK_MSR_ID(146, "audio_pdm_dclk"),
	CLK_MSR_ID(147, "audio_resampleb_clk"),
	CLK_MSR_ID(148, "earcrx_pll_dmac_ck"),
	CLK_MSR_ID(160, "pwm_j_clk"),
	CLK_MSR_ID(161, "pwm_i_clk"),
	CLK_MSR_ID(162, "pwm_h_clk"),
	CLK_MSR_ID(163, "pwm_g_clk"),
	CLK_MSR_ID(164, "pwm_f_clk"),
	CLK_MSR_ID(165, "pwm_e_clk"),
	CLK_MSR_ID(166, "pwm_d_clk"),
	CLK_MSR_ID(167, "pwm_c_clk"),
	CLK_MSR_ID(168, "pwm_b_clk"),
	CLK_MSR_ID(169, "pwm_a_clk"),
	CLK_MSR_ID(176, "rng_ring_0"),
	CLK_MSR_ID(177, "rng_ring_1"),
	CLK_MSR_ID(178, "rng_ring_2"),
	CLK_MSR_ID(179, "rng_ring_3"),
	CLK_MSR_ID(180, "dmc_osc_ring(LVT16)"),
	CLK_MSR_ID(181, "gpu_osc_ring0(LVT16)"),
	CLK_MSR_ID(182, "gpu_osc_ring1(ULVT16)"),
	CLK_MSR_ID(183, "gpu_osc_ring2(SLVT16)"),
	CLK_MSR_ID(184, "vpu_osc_ring0(SVT24)"),
	CLK_MSR_ID(185, "vpu_osc_ring1(LVT20)"),
	CLK_MSR_ID(186, "vpu_osc_ring2(LVT16)"),
	CLK_MSR_ID(187, "dos_osc_ring0(SVT24)"),
	CLK_MSR_ID(188, "dos_osc_ring1(SVT16)"),
	CLK_MSR_ID(189, "dos_osc_ring2(LVT16)"),
	CLK_MSR_ID(190, "dos_osc_ring3(ULVT20)"),
	CLK_MSR_ID(191, "ddr_osc_ring(LVT16)"),
	CLK_MSR_ID(192, "sys_cpu_osc_ring0(ULVT16)"),
	CLK_MSR_ID(193, "sys_cpu_osc_ring1(ULVT20)"),
	CLK_MSR_ID(194, "sys_cpu_osc_ring2(ULVT16)"),
	CLK_MSR_ID(195, "sys_cpu_osc_ring3(LVT16)"),
	CLK_MSR_ID(196, "axi_sram_osc_ring(SVT16)"),
	CLK_MSR_ID(197, "dspa_osc_ring(SVT16)")
};

static struct meson_msr_id clk_msr_t5d[] __initdata = {
	CLK_MSR_ID(210, "am_ring_osc_ee24"),
	CLK_MSR_ID(209, "am_ring_osc_ee23"),
	CLK_MSR_ID(208, "am_ring_osc_ee22"),
	CLK_MSR_ID(207, "am_ring_osc_ee21"),
	CLK_MSR_ID(206, "am_ring_osc_ee20"),
	CLK_MSR_ID(205, "am_ring_osc_ee19"),
	CLK_MSR_ID(204, "am_ring_osc_ee18"),
	CLK_MSR_ID(203, "am_ring_osc_ee17"),
	CLK_MSR_ID(202, "am_ring_osc_ee16"),
	CLK_MSR_ID(201, "am_ring_osc_ee15"),
	CLK_MSR_ID(200, "am_ring_osc_ee14"),
	CLK_MSR_ID(199, "am_ring_osc_ee13"),
	CLK_MSR_ID(198, "am_ring_osc_ee12"),
	CLK_MSR_ID(197, "sys_cpu_ring_osc27"),
	CLK_MSR_ID(196, "sys_cpu_ring_osc26"),
	CLK_MSR_ID(195, "sys_cpu_ring_osc25"),
	CLK_MSR_ID(194, "sys_cpu_ring_osc24"),
	CLK_MSR_ID(193, "sys_cpu_ring_osc23"),
	CLK_MSR_ID(192, "sys_cpu_ring_osc22"),
	CLK_MSR_ID(191, "sys_cpu_ring_osc21"),
	CLK_MSR_ID(190, "sys_cpu_ring_osc20"),
	CLK_MSR_ID(189, "sys_cpu_ring_osc19"),
	CLK_MSR_ID(188, "sys_cpu_ring_osc18"),
	CLK_MSR_ID(187, "sys_cpu_ring_osc17"),
	CLK_MSR_ID(186, "sys_cpu_ring_osc16"),
	CLK_MSR_ID(185, "sys_cpu_ring_osc15"),
	CLK_MSR_ID(184, "sys_cpu_ring_osc14"),
	CLK_MSR_ID(183, "sys_cpu_ring_osc13"),
	CLK_MSR_ID(182, "sys_cpu_ring_osc12"),
	CLK_MSR_ID(181, "sys_cpu_ring_osc11"),
	CLK_MSR_ID(180, "sys_cpu_ring_osc10"),
	CLK_MSR_ID(179, "sys_cpu_ring_osc9"),
	CLK_MSR_ID(178, "sys_cpu_ring_osc8"),
	CLK_MSR_ID(177, "sys_cpu_ring_osc7"),
	CLK_MSR_ID(176, "sys_cpu_ring_osc6"),
	CLK_MSR_ID(175, "sys_cpu_ring_osc5"),
	CLK_MSR_ID(174, "sys_cpu_ring_osc4"),
	CLK_MSR_ID(173, "adc_lr_outrdy"),
	CLK_MSR_ID(172, "atv_dmd_mono_out"),
	CLK_MSR_ID(167, "au_dac1l_en_dac_clk"),
	CLK_MSR_ID(166, "au_dac1r_en_dac_clk"),
	CLK_MSR_ID(163, "hdmirx_aud_sck"),
	CLK_MSR_ID(157, "vpu_dmc_clk"),
	CLK_MSR_ID(156, "p22_usb2_clkout"),
	CLK_MSR_ID(155, "p21_usb2_clkout"),
	CLK_MSR_ID(154, "p20_usb2_clkout"),
	CLK_MSR_ID(153, "c_alocker_out_clk"),
	CLK_MSR_ID(152, "c_alocker_in_clk"),
	CLK_MSR_ID(151, "dpll_intclk"),
	CLK_MSR_ID(150, "dpll_clk_a2"),
	CLK_MSR_ID(149, "dpll_clk_b2"),
	CLK_MSR_ID(148, "dpll_clk_b3"),
	CLK_MSR_ID(144, "ts_pll_clk"),
	CLK_MSR_ID(143, "mainclk"),
	CLK_MSR_ID(142, "demode_ts_clk"),
	CLK_MSR_ID(140, "audio_toacodec_bclk"),
	CLK_MSR_ID(139, "aud_adc_clk_g128x"),
	CLK_MSR_ID(137, "atv_dmd_i2c_sclk"),
	CLK_MSR_ID(135, "tvfe_sample_clk"),
	CLK_MSR_ID(134, "adc_extclk_in"),
	CLK_MSR_ID(133, "atv_dmd_mono_clk_32"),
	CLK_MSR_ID(132, "audio_toacode_mclk"),
	CLK_MSR_ID(129, "lvds_fifo_clk"),
	CLK_MSR_ID(128, "cts_tcon_pll_clk"),
	CLK_MSR_ID(127, "hdmirx_vid_clk"),
	CLK_MSR_ID(125, "cts_hdmi_axi_clk"),
	CLK_MSR_ID(124,  "cts_demod_core_clk"),
	CLK_MSR_ID(123,  "mod_audio_pdm_dclk_o"),
	CLK_MSR_ID(122,  "audio_spdifin_mst_clk"),
	CLK_MSR_ID(121, "audio_spdifout_mst_clk"),
	CLK_MSR_ID(120, "audio_spdifout_b_mst_clk"),
	CLK_MSR_ID(119, "audio_pdm_sysclk"),
	CLK_MSR_ID(118, "audio_resamplea_clk"),
	CLK_MSR_ID(117, "audio_resampleb_clk"),
	CLK_MSR_ID(116, "audio_tdmin_a_sclk"),
	CLK_MSR_ID(115, "audio_tdmin_b_sclk"),
	CLK_MSR_ID(114, "audio_tdmin_c_sclk"),
	CLK_MSR_ID(113, "audio_tdmin_lb_sclk"),
	CLK_MSR_ID(112, "audio_tdmout_a_sclk"),
	CLK_MSR_ID(111, "audio_tdmout_b_sclk"),
	CLK_MSR_ID(110, "audio_tdmout_c_sclk"),
	CLK_MSR_ID(109, "o_vad_clk"),
	CLK_MSR_ID(108, "acodec_i2sout_bclk"),
	CLK_MSR_ID(107, "au_dac_clk_g128x"),
	CLK_MSR_ID(106, "ephy_test_clk"),
	CLK_MSR_ID(105, "am_ring_osc_clk_out_ee[9]"),
	CLK_MSR_ID(104, "am_ring_osc_clk_out_ee[8]"),
	CLK_MSR_ID(103, "am_ring_osc_clk_out_ee[7]"),
	CLK_MSR_ID(102, "am_ring_osc_clk_out_ee[6]"),
	CLK_MSR_ID(101, "am_ring_osc_clk_out_ee[5]"),
	CLK_MSR_ID(100, "am_ring_osc_clk_out_ee[4]"),
	CLK_MSR_ID(99, "am_ring_osc_clk_out_ee[3]"),
	CLK_MSR_ID(98, "cts_ts_clk"),
	CLK_MSR_ID(97, "cts_vpu_clkb_tmp"),
	CLK_MSR_ID(96, "cts_vpu_clkb"),
	CLK_MSR_ID(95, "eth_phy_plltxclk"),
	CLK_MSR_ID(94, "eth_phy_exclk"),
	CLK_MSR_ID(93, "sys_cpu_ring_osc_clk[3]"),
	CLK_MSR_ID(92, "sys_cpu_ring_osc_clk[2]"),
	CLK_MSR_ID(91, "hdmirx_audmeas_clk"),
	CLK_MSR_ID(90, "am_ring_osc_clk_out_ee[11]"),
	CLK_MSR_ID(89, "am_ring_osc_clk_out_ee[10]"),
	CLK_MSR_ID(88, "cts_hdmirx_meter_clk"),
	CLK_MSR_ID(86, "cts_hdmirx_modet_clk"),
	CLK_MSR_ID(85, "cts_hdmirx_acr_ref_clk"),
	CLK_MSR_ID(84, "co_tx_cl"),
	CLK_MSR_ID(83, "co_rx_clk"),
	CLK_MSR_ID(82, "cts_ge2d_clk"),
	CLK_MSR_ID(81, "cts_vapbclk"),
	CLK_MSR_ID(80, "rng_ring_osc_clk[3]"),
	CLK_MSR_ID(79, "rng_ring_osc_clk[2]"),
	CLK_MSR_ID(78, "rng_ring_osc_clk[1]"),
	CLK_MSR_ID(77, "rng_ring_osc_clk[0]"),
	CLK_MSR_ID(76, "hdmix_aud_clk"),
	CLK_MSR_ID(75, "cts_hevcf_clk"),
	CLK_MSR_ID(74, "hdmirx_aud_pll_clk"),
	CLK_MSR_ID(73, "cts_pwm_C_clk"),
	CLK_MSR_ID(72, "cts_pwm_D_clk"),
	CLK_MSR_ID(71, "cts_pwm_E_clk"),
	CLK_MSR_ID(70, "cts_pwm_F_clk"),
	CLK_MSR_ID(69, "cts_hdcp22_skpclk"),
	CLK_MSR_ID(68, "cts_hdcp22_esmclk"),
	CLK_MSR_ID(67, "hdmirx_apll_clk_audio"),
	CLK_MSR_ID(66, "cts_vid_lock_clk"),
	CLK_MSR_ID(65, "cts_spicc_0_clk"),
	CLK_MSR_ID(63, "hdmirx_tmds_clk"),
	CLK_MSR_ID(61, "gpio_clk_msr"),
	CLK_MSR_ID(60, "cts_hdmirx_aud_pll_clk"),
	CLK_MSR_ID(58, "cts_vafe_datack"),
	CLK_MSR_ID(57, "cts_atv_dmd_vdac_clk"),
	CLK_MSR_ID(56, "cts_atv_dmd_sys_clk"),
	CLK_MSR_ID(55, "vid_pll_div_clk_out"),
	CLK_MSR_ID(54, "cts_vpu_clkc"),
	CLK_MSR_ID(53, "ddr_2xclk"),
	CLK_MSR_ID(51, "cts_sd_emmc_clk_C"),
	CLK_MSR_ID(50, "mp3_clk_out"),
	CLK_MSR_ID(49, "mp2_clk_out"),
	CLK_MSR_ID(48, "mp1_clk_out"),
	CLK_MSR_ID(47, "ddr_dpll_pt_clk"),
	CLK_MSR_ID(46, "cts_vpu_clk"),
	CLK_MSR_ID(45, "cts_pwm_A_clk"),
	CLK_MSR_ID(44, "cts_pwm_B_clk"),
	CLK_MSR_ID(43, "fclk_div5"),
	CLK_MSR_ID(42, "mp0_clk_out"),
	CLK_MSR_ID(41, "mac_eth_rx_clk_rmii"),
	CLK_MSR_ID(40, "cts_hdmirx_cfg_clk"),
	CLK_MSR_ID(38, "cts_vdin_meas_clk"),
	CLK_MSR_ID(37, "cts_cdac_clk_c"),
	CLK_MSR_ID(35, "cts_mali_clk"),
	CLK_MSR_ID(34, "eth_mppll_50m_ckout"),
	CLK_MSR_ID(33, "sys_cpu_ring_osc_clk[1]"),
	CLK_MSR_ID(32, "cts_vdec_clk"),
	CLK_MSR_ID(31, "mpll_clk_test_out"),
	CLK_MSR_ID(30, "hdmirx_cable_clk"),
	CLK_MSR_ID(29, "hdmirx_apll_clk_out_div"),
	CLK_MSR_ID(28, "cts_sar_adc_clk"),
	CLK_MSR_ID(27, "co_clkin_to_mac"),
	CLK_MSR_ID(25, "cts_eth_clk_rmii"),
	CLK_MSR_ID(24, "cts_eth_clk125Mhz"),
	CLK_MSR_ID(23, "mpll_clk_50m"),
	CLK_MSR_ID(22, "mac_eth_phy_ref_clk"),
	CLK_MSR_ID(21, "lcd_an_clk_ph3"),
	CLK_MSR_ID(20, "rtc_osc_clk_out"),
	CLK_MSR_ID(19, "lcd_an_clk_ph2"),
	CLK_MSR_ID(18, "sys_cpu_clk_div16"),
	CLK_MSR_ID(17, "sys_pll_div16"),
	CLK_MSR_ID(16, "cts_FEC_CLK_2"),
	CLK_MSR_ID(15, "cts_FEC_CLK_1"),
	CLK_MSR_ID(14, "cts_FEC_CLK_0"),
	CLK_MSR_ID(13, "mod_tcon_clko"),
	CLK_MSR_ID(12, "hifi_pll_clk"),
	CLK_MSR_ID(11, "mac_eth_tx_clk"),
	CLK_MSR_ID(10, "cts_vdac_clk"),
	CLK_MSR_ID(9, "cts_encl_clk"),
	CLK_MSR_ID(8, "cts_encp_clk"),
	CLK_MSR_ID(7, "clk81"),
	CLK_MSR_ID(6, "cts_enci_clk"),
	CLK_MSR_ID(4, "gp0_pll_clk"),
	CLK_MSR_ID(3, "sys_cpu_ring_osc_clk[0]"),
	CLK_MSR_ID(2, "am_ring_osc_clk_out_ee[2]"),
	CLK_MSR_ID(1, "am_ring_osc_clk_out_ee[1]"),
	CLK_MSR_ID(0, "am_ring_osc_clk_out_ee[0]"),
};

static struct meson_msr_id clk_msr_t7[] __initdata = {
	CLK_MSR_ID(0, "cts_sys_clk"),
	CLK_MSR_ID(1, "cts_axi_clk"),
	CLK_MSR_ID(2, "cts_rtc_clk"),
	CLK_MSR_ID(3, "cts_dspa_clk"),
	CLK_MSR_ID(4, "cts_dspb_clk"),
	CLK_MSR_ID(5, "cts_mali_clk"),
	CLK_MSR_ID(6, "sys_cpu_clk_div16"),
	CLK_MSR_ID(7, "cts_ceca_clk"),
	CLK_MSR_ID(8, "cts_cecb_clk"),
	CLK_MSR_ID(10, "fclk_div5"),
	CLK_MSR_ID(11, "mp0_clk_out"),
	CLK_MSR_ID(12, "mp1_clk_out"),
	CLK_MSR_ID(13, "mp2_clk_out"),
	CLK_MSR_ID(14, "mp3_clk_out"),
	CLK_MSR_ID(15, "mpll_clk_50m"),
	CLK_MSR_ID(16, "pcie_clk_inp"),
	CLK_MSR_ID(17, "pcie_clk_inn"),
	CLK_MSR_ID(18, "mpll_clk_test_out"),
	CLK_MSR_ID(19, "hifi_pll_clk"),
	CLK_MSR_ID(20, "gp0_pll_clk"),
	CLK_MSR_ID(21, "gp1_pll_clk"),
	CLK_MSR_ID(22, "eth_mppll_50m_ckout"),
	CLK_MSR_ID(23, "sys_pll_div16"),
	CLK_MSR_ID(24, "ddr_dpll_pt_clk"),
	CLK_MSR_ID(25, "earcrx_pll_ckout"),
	CLK_MSR_ID(26, "paie1_clk_inp"),
	CLK_MSR_ID(27, "paie1_clk_inn"),
	CLK_MSR_ID(28, "cts_amlgdc_clk"),
	CLK_MSR_ID(29, "cts_gdc_clk"),
	CLK_MSR_ID(30, "mod_eth_phy_ref_clk"),
	CLK_MSR_ID(31, "mod_eth_tx_clk"),
	CLK_MSR_ID(32, "cts_eth_clk125Mhz"),
	CLK_MSR_ID(33, "cts_eth_clk_rmii"),
	CLK_MSR_ID(34, "co_clkin_to_mac"),
	CLK_MSR_ID(35, "mod_eth_rx_clk_rmii"),
	CLK_MSR_ID(36, "co_rx_clk"),
	CLK_MSR_ID(37, "co_tx_clk"),
	CLK_MSR_ID(38, "eth_phy_rxclk"),
	CLK_MSR_ID(39, "eth_phy_plltxclk"),
	CLK_MSR_ID(40, "ephy_test_clk"),
	CLK_MSR_ID(41, "cts_dsi_b_meas_clk"),
	CLK_MSR_ID(42, "hdmirx_apll_clk_out"),
	CLK_MSR_ID(43, "hdmirx_tmds_clk"),
	CLK_MSR_ID(44, "hdmirx_cable_clk"),
	CLK_MSR_ID(45, "hdmirx_apll_clk_audio"),
	CLK_MSR_ID(46, "hdmirx_5m_clk"),
	CLK_MSR_ID(47, "hdmirx_2m_clk"),
	CLK_MSR_ID(48, "hdmirx_cfg_clk"),
	CLK_MSR_ID(49, "hdmirx_hdcp2x_eclk"),
	CLK_MSR_ID(50, "vid_pll0_div_clk_out"),
	CLK_MSR_ID(51, "hdmi_vid_pll_clk"),
	CLK_MSR_ID(54, "cts_vdac_clk"),
	CLK_MSR_ID(55, "cts_vpu_clk_buf"),
	CLK_MSR_ID(56, "mod_tcon_clko"),
	CLK_MSR_ID(57, "lcd_an_clk_ph2"),
	CLK_MSR_ID(58, "lcd_an_clk_ph3"),
	CLK_MSR_ID(59, "cts_hdmi_tx_pixel_clk"),
	CLK_MSR_ID(60, "cts_vdin_meas_clk"),
	CLK_MSR_ID(61, "cts_vpu_clk"),
	CLK_MSR_ID(62, "cts_vpu_clkb"),
	CLK_MSR_ID(63, "cts_vpu_clkb_tmp"),
	CLK_MSR_ID(64, "cts_vpu_clkc"),
	CLK_MSR_ID(65, "cts_vid_lock_clk"),
	CLK_MSR_ID(66, "cts_vapbclk"),
	CLK_MSR_ID(67, "cts_ge2d_clk"),
	CLK_MSR_ID(68, "cts_aud_pll_clk"),
	CLK_MSR_ID(69, "cts_aud_sck"),
	CLK_MSR_ID(70, "cts_dsi_a_meas_clk"),
	CLK_MSR_ID(72, "cts_mipi_csi_phy"),
	CLK_MSR_ID(73, "cts_mipi_isp_clk"),
	CLK_MSR_ID(76, "hdmitx_tmds_clk"),
	CLK_MSR_ID(77, "cts_hdmitx_sys_clk"),
	CLK_MSR_ID(78, "cts_hdmitx_fe_clk"),
	CLK_MSR_ID(80, "cts_hdmitx_prif_clk"),
	CLK_MSR_ID(81, "cts_hdmitx_200m_clk"),
	CLK_MSR_ID(82, "cts_hdmitx_aud_clk"),
	CLK_MSR_ID(83, "cts_hdmitx_pnx_clk"),
	CLK_MSR_ID(84, "cts_spicc5"),
	CLK_MSR_ID(85, "cts_spicc4"),
	CLK_MSR_ID(86, "cts_spicc3"),
	CLK_MSR_ID(87, "cts_spicc2"),
	CLK_MSR_ID(93, "cts_vdec_clk"),
	CLK_MSR_ID(94, "cts_wave521_aclk"),
	CLK_MSR_ID(95, "cts_wave521_cclk"),
	CLK_MSR_ID(96, "cts_wave521_bclk"),
	CLK_MSR_ID(97, "cts_hcodec_clk"),
	CLK_MSR_ID(98, "cts_hevcb_clk"),
	CLK_MSR_ID(99, "cts_hevcf_clk"),
	CLK_MSR_ID(100, "cts_hdmi_aud_pll_clk"),
	CLK_MSR_ID(101, "cts_hdmi_acr_ref_clk"),
	CLK_MSR_ID(102, "cts_hdmi_meter_clk"),
	CLK_MSR_ID(103, "cts_hdmi_vid_clk"),
	CLK_MSR_ID(104, "cts_hdmi_aud_clk"),
	CLK_MSR_ID(105, "cts_hdmi_dsd_clk"),
	CLK_MSR_ID(108, "cts_dsi1_phy_clk"),
	CLK_MSR_ID(109, "cts_dsi0_phy_clk"),
	CLK_MSR_ID(110, "cts_sc_clk(smartcard)"),
	CLK_MSR_ID(111, "cts_sar_adc_clk"),
	CLK_MSR_ID(113, "cts_sd_emmc_C_clk(nand)"),
	CLK_MSR_ID(114, "cts_sd_emmc_B_clk"),
	CLK_MSR_ID(115, "cts_sd_emmc_A_clk"),
	CLK_MSR_ID(116, "gpio_msr_clk"),
	CLK_MSR_ID(117, "cts_spicc_1_clk"),
	CLK_MSR_ID(118, "cts_spicc_0_clk"),
	CLK_MSR_ID(118, "cts_anakin_clk"),
	CLK_MSR_ID(121, "cts_ts_clk(temp sensor)"),
	CLK_MSR_ID(122, "ts_a73_clk"),
	CLK_MSR_ID(123, "ts_a53_clk"),
	CLK_MSR_ID(124, "ts_nna_clk"),
	CLK_MSR_ID(130, "audio_vad_clk"),
	CLK_MSR_ID(131, "acodec_dac_clk_x128"),
	CLK_MSR_ID(132, "audio_locker_out_clk"),
	CLK_MSR_ID(133, "audio_locker_in_clk"),
	CLK_MSR_ID(134, "audio_tdmout_c_sclk"),
	CLK_MSR_ID(135, "audio_tdmout_b_sclk"),
	CLK_MSR_ID(136, "audio_tdmout_a_sclk"),
	CLK_MSR_ID(137, "audio_tdmin_lb_sclk"),
	CLK_MSR_ID(138, "audio_tdmin_c_sclk"),
	CLK_MSR_ID(139, "audio_tdmin_b_sclk"),
	CLK_MSR_ID(140, "audio_tdmin_a_sclk"),
	CLK_MSR_ID(141, "audio_resamplea_clk"),
	CLK_MSR_ID(142, "audio_pdm_sysclk"),
	CLK_MSR_ID(143, "audio_spdifoutb_mst_clk"),
	CLK_MSR_ID(144, "audio_spdifout_mst_clk"),
	CLK_MSR_ID(145, "audio_spdifin_mst_clk"),
	CLK_MSR_ID(146, "audio_pdm_dclk"),
	CLK_MSR_ID(147, "audio_resampleb_clk"),
	CLK_MSR_ID(148, "earcrx_pll_dmac_ck"),
	CLK_MSR_ID(156, "pwm_ao_h_clk"),
	CLK_MSR_ID(157, "pwm_ao_g_clk"),
	CLK_MSR_ID(158, "pwm_ao_f_clk"),
	CLK_MSR_ID(159, "pwm_ao_e_clk"),
	CLK_MSR_ID(160, "pwm_ao_d_clk"),
	CLK_MSR_ID(161, "pwm_ao_c_clk"),
	CLK_MSR_ID(162, "pwm_ao_b_clk"),
	CLK_MSR_ID(163, "pwm_ao_a_clk"),
	CLK_MSR_ID(164, "pwm_f_clk"),
	CLK_MSR_ID(165, "pwm_e_clk"),
	CLK_MSR_ID(166, "pwm_d_clk"),
	CLK_MSR_ID(167, "pwm_c_clk"),
	CLK_MSR_ID(168, "pwm_b_clk"),
	CLK_MSR_ID(169, "pwm_a_clk"),
	CLK_MSR_ID(170, "cts_ACLKM"),
	CLK_MSR_ID(171, "mclk_pll_clk"),
	CLK_MSR_ID(172, "a73_sys_pll_div16"),
	CLK_MSR_ID(173, "a73_cpu_clk_div16"),
	CLK_MSR_ID(176, "rng_ring_0"),
	CLK_MSR_ID(177, "rng_ring_1"),
	CLK_MSR_ID(178, "rng_ring_2"),
	CLK_MSR_ID(179, "rng_ring_3"),
	CLK_MSR_ID(180, "am_ring_out0"),
	CLK_MSR_ID(181, "am_ring_out1"),
	CLK_MSR_ID(182, "am_ring_out2"),
	CLK_MSR_ID(183, "am_ring_out3"),
	CLK_MSR_ID(184, "am_ring_out4"),
	CLK_MSR_ID(185, "am_ring_out5"),
	CLK_MSR_ID(186, "am_ring_out6"),
	CLK_MSR_ID(187, "am_ring_out7"),
	CLK_MSR_ID(188, "am_ring_out8"),
	CLK_MSR_ID(189, "am_ring_out9"),
	CLK_MSR_ID(190, "am_ring_out10"),
	CLK_MSR_ID(191, "am_ring_out11"),
	CLK_MSR_ID(192, "am_ring_out12"),
	CLK_MSR_ID(193, "am_ring_out13"),
	CLK_MSR_ID(194, "am_ring_out14"),
	CLK_MSR_ID(195, "am_ring_out15"),
	CLK_MSR_ID(196, "am_ring_out16"),
	CLK_MSR_ID(197, "am_ring_out17"),
	CLK_MSR_ID(198, "am_ring_out18"),
	CLK_MSR_ID(199, "am_ring_out19"),
	CLK_MSR_ID(200, "mipi_csi_phy0"),
	CLK_MSR_ID(201, "mipi_csi_phy1"),
	CLK_MSR_ID(202, "mipi_csi_phy2"),
	CLK_MSR_ID(203, "mipi_csi_phy3"),
	CLK_MSR_ID(204, "vid_pll1_div_clk"),
	CLK_MSR_ID(205, "vid_pll2_div_clk"),
	CLK_MSR_ID(206, "am_ring_out20"),
	CLK_MSR_ID(207, "am_ring_out21"),
	CLK_MSR_ID(208, "am_ring_out22"),
	CLK_MSR_ID(209, "am_ring_out23"),
	CLK_MSR_ID(210, "am_ring_out24"),
	CLK_MSR_ID(211, "am_ring_out25"),
	CLK_MSR_ID(212, "am_ring_out26"),
	CLK_MSR_ID(213, "am_ring_out27"),
	CLK_MSR_ID(214, "am_ring_out28"),
	CLK_MSR_ID(215, "am_ring_out29"),
	CLK_MSR_ID(216, "am_ring_out30"),
	CLK_MSR_ID(217, "am_ring_out31"),
	CLK_MSR_ID(218, "am_ring_out32"),
	CLK_MSR_ID(219, "cts_enc0_if_clk"),
	CLK_MSR_ID(220, "cts_enc2_clk"),
	CLK_MSR_ID(221, "cts_enc1_clk"),
	CLK_MSR_ID(222, "cts_enc0_clk")
};

static struct meson_msr_id clk_msr_s4[] __initdata = {
	CLK_MSR_ID(0, "cts_sys_clk"),
	CLK_MSR_ID(1, "cts_axi_clk"),
	CLK_MSR_ID(2, "cts_rtc_clk"),
	CLK_MSR_ID(5, "cts_mali_clk"),
	CLK_MSR_ID(6, "sys_cpu_clk_div16"),
	CLK_MSR_ID(7, "cts_ceca_clk"),
	CLK_MSR_ID(8, "cts_cecb_clk"),
	CLK_MSR_ID(10, "fclk_div5"),
	CLK_MSR_ID(11, "mp0_clk_out"),
	CLK_MSR_ID(12, "mp1_clk_out"),
	CLK_MSR_ID(13, "mp2_clk_out"),
	CLK_MSR_ID(14, "mp3_clk_out"),
	CLK_MSR_ID(15, "mpll_clk_50m"),
	CLK_MSR_ID(16, "pcie_clk_inp"),
	CLK_MSR_ID(17, "pcie_clk_inn"),
	CLK_MSR_ID(18, "mpll_clk_test_out"),
	CLK_MSR_ID(19, "hifi_pll_clk"),
	CLK_MSR_ID(20, "gp0_pll_clk"),
	CLK_MSR_ID(22, "eth_mppll_50m_ckout"),
	CLK_MSR_ID(23, "sys_pll_div16"),
	CLK_MSR_ID(24, "ddr_dpll_pt_clk"),
	CLK_MSR_ID(30, "mod_eth_phy_ref_clk"),
	CLK_MSR_ID(31, "mod_eth_tx_clk"),
	CLK_MSR_ID(32, "cts_eth_clk125Mhz"),
	CLK_MSR_ID(33, "cts_eth_clk_rmii"),
	CLK_MSR_ID(34, "co_clkin_to_mac"),
	CLK_MSR_ID(35, "mod_eth_rx_clk_rmii"),
	CLK_MSR_ID(36, "co_rx_clk"),
	CLK_MSR_ID(37, "co_tx_clk"),
	CLK_MSR_ID(38, "eth_phy_rxclk"),
	CLK_MSR_ID(39, "eth_phy_plltxclk"),
	CLK_MSR_ID(40, "ephy_test_clk"),
	CLK_MSR_ID(50, "vid_pll_div_clk_out"),
	CLK_MSR_ID(51, "cts_enci_clk"),
	CLK_MSR_ID(52, "cts_encp_clk"),
	CLK_MSR_ID(53, "cts_encl_clk"),
	CLK_MSR_ID(54, "cts_vdac_clk"),
	CLK_MSR_ID(55, "cts_cdac_clk_c"),
	CLK_MSR_ID(56, "mod_tcon_clko"),
	CLK_MSR_ID(57, "lcd_an_clk_ph2"),
	CLK_MSR_ID(58, "lcd_an_clk_ph3"),
	CLK_MSR_ID(59, "cts_hdmi_tx_pixel_clk"),
	CLK_MSR_ID(60, "cts_vdin_meas_clk"),
	CLK_MSR_ID(61, "cts_vpu_clk"),
	CLK_MSR_ID(62, "cts_vpu_clkb"),
	CLK_MSR_ID(63, "cts_vpu_clkb_tmp"),
	CLK_MSR_ID(64, "cts_vpu_clkc"),
	CLK_MSR_ID(65, "cts_vid_lock_clk"),
	CLK_MSR_ID(66, "cts_vapbclk"),
	CLK_MSR_ID(67, "cts_ge2d_clk"),
	CLK_MSR_ID(68, "cts_hdcp22_esmclk"),
	CLK_MSR_ID(69, "cts_hdcp22_skpclk"),
	CLK_MSR_ID(76, "hdmitx_tmds_clk"),
	CLK_MSR_ID(77, "cts_hdmitx_sys_clk"),
	CLK_MSR_ID(78, "cts_hdmitx_fe_clk"),
	CLK_MSR_ID(79, "cts_rama_clk"),
	CLK_MSR_ID(93, "cts_vdec_clk"),
	CLK_MSR_ID(99, "cts_hevcf_clk"),
	CLK_MSR_ID(100, "cts_demod_core_clk"),
	CLK_MSR_ID(101, "adc_extclk_in"),
	CLK_MSR_ID(103, "adc_dpll_intclk"),
	CLK_MSR_ID(104, "adc_dpll_clk_b3"),
	CLK_MSR_ID(105, "s2_adc_clk"),
	CLK_MSR_ID(106, "deskew_pll_clk_div32_out"),
	CLK_MSR_ID(110, "cts_sc_clk(smartcard)"),
	CLK_MSR_ID(111, "cts_sar_adc_clk"),
	CLK_MSR_ID(113, "cts_sd_emmc_C_clk(nand)"),
	CLK_MSR_ID(114, "cts_sd_emmc_B_clk"),
	CLK_MSR_ID(115, "cts_sd_emmc_A_clk"),
	CLK_MSR_ID(116, "gpio_msr_clk"),
	CLK_MSR_ID(118, "cts_spicc_0_clk"),
	CLK_MSR_ID(121, "cts_ts_clk(temp sensor)"),
	CLK_MSR_ID(130, "audio_vad_clk"),
	CLK_MSR_ID(131, "acodec_dac_clk_x128"),
	CLK_MSR_ID(132, "audio_locker_in_clk"),
	CLK_MSR_ID(133, "audio_locker_out_clk"),
	CLK_MSR_ID(134, "audio_tdmout_c_sclk"),
	CLK_MSR_ID(135, "audio_tdmout_b_sclk"),
	CLK_MSR_ID(136, "audio_tdmout_a_sclk"),
	CLK_MSR_ID(137, "audio_tdmin_lb_sclk"),
	CLK_MSR_ID(138, "audio_tdmin_c_sclk"),
	CLK_MSR_ID(139, "audio_tdmin_b_sclk"),
	CLK_MSR_ID(140, "audio_tdmin_a_sclk"),
	CLK_MSR_ID(141, "audio_resamplea_clk"),
	CLK_MSR_ID(142, "audio_pdm_sysclk"),
	CLK_MSR_ID(143, "audio_spdifout_b_mst_clk"),
	CLK_MSR_ID(144, "audio_spdifout_mst_clk"),
	CLK_MSR_ID(145, "audio_spdifin_mst_clk"),
	CLK_MSR_ID(146, "audio_pdm_dclk"),
	CLK_MSR_ID(147, "audio_resampleb_clk"),
	CLK_MSR_ID(160, "pwm_j_clk"),
	CLK_MSR_ID(161, "pwm_i_clk"),
	CLK_MSR_ID(162, "pwm_h_clk"),
	CLK_MSR_ID(163, "pwm_g_clk"),
	CLK_MSR_ID(164, "pwm_f_clk"),
	CLK_MSR_ID(165, "pwm_e_clk"),
	CLK_MSR_ID(166, "pwm_d_clk"),
	CLK_MSR_ID(167, "pwm_c_clk"),
	CLK_MSR_ID(168, "pwm_b_clk"),
	CLK_MSR_ID(169, "pwm_a_clk"),
	CLK_MSR_ID(176, "rng_ring_0"),
	CLK_MSR_ID(177, "rng_ring_1"),
	CLK_MSR_ID(178, "rng_ring_2"),
	CLK_MSR_ID(179, "rng_ring_3"),
	CLK_MSR_ID(180, "dmc_osc_ring(LVT16)"),
	CLK_MSR_ID(181, "gpu_osc_ring0(LVT16)"),
	CLK_MSR_ID(182, "gpu_osc_ring1(ULVT16)"),
	CLK_MSR_ID(183, "gpu_osc_ring2(SLVT16)"),
	CLK_MSR_ID(184, "vpu_osc_ring0(SVT24)"),
	CLK_MSR_ID(185, "vpu_osc_ring1(LVT20)"),
	CLK_MSR_ID(186, "vpu_osc_ring2(LVT16)"),
	CLK_MSR_ID(187, "dos_osc_ring0(SVT24)"),
	CLK_MSR_ID(188, "dos_osc_ring1(SVT16)"),
	CLK_MSR_ID(189, "dos_osc_ring2(LVT16)"),
	CLK_MSR_ID(190, "dos_osc_ring3(ULVT20)"),
	CLK_MSR_ID(192, "axi_sram_osc_ring(SVT16)"),
	CLK_MSR_ID(193, "demod_osc_ring0"),
	CLK_MSR_ID(194, "demod_osc_ring1"),
	CLK_MSR_ID(195, "sar_osc_ring"),
	CLK_MSR_ID(196, "sys_cpu_osc_ring0"),
	CLK_MSR_ID(197, "sys_cpu_osc_ring1"),
	CLK_MSR_ID(198, "sys_cpu_osc_ring2"),
	CLK_MSR_ID(199, "sys_cpu_osc_ring3"),
	CLK_MSR_ID(200, "sys_cpu_osc_ring4"),
	CLK_MSR_ID(201, "sys_cpu_osc_ring5"),
	CLK_MSR_ID(202, "sys_cpu_osc_ring6"),
	CLK_MSR_ID(203, "sys_cpu_osc_ring7"),
	CLK_MSR_ID(204, "sys_cpu_osc_ring8"),
	CLK_MSR_ID(205, "sys_cpu_osc_ring9"),
	CLK_MSR_ID(206, "sys_cpu_osc_ring10"),
	CLK_MSR_ID(207, "sys_cpu_osc_ring11"),
	CLK_MSR_ID(208, "sys_cpu_osc_ring12"),
	CLK_MSR_ID(209, "sys_cpu_osc_ring13"),
	CLK_MSR_ID(210, "sys_cpu_osc_ring14"),
	CLK_MSR_ID(211, "sys_cpu_osc_ring15"),
	CLK_MSR_ID(212, "sys_cpu_osc_ring16"),
	CLK_MSR_ID(213, "sys_cpu_osc_ring17"),
	CLK_MSR_ID(214, "sys_cpu_osc_ring18"),
	CLK_MSR_ID(215, "sys_cpu_osc_ring19"),
	CLK_MSR_ID(216, "sys_cpu_osc_ring20"),
	CLK_MSR_ID(217, "sys_cpu_osc_ring21"),
	CLK_MSR_ID(218, "sys_cpu_osc_ring22"),
	CLK_MSR_ID(219, "sys_cpu_osc_ring23"),
	CLK_MSR_ID(220, "sys_cpu_osc_ring24"),
	CLK_MSR_ID(221, "sys_cpu_osc_ring25"),
	CLK_MSR_ID(222, "sys_cpu_osc_ring26"),
	CLK_MSR_ID(223, "sys_cpu_osc_ring27"),
};

static int meson_measure_id(struct meson_msr_id *clk_msr_id,
			       unsigned int duration)
{
	struct meson_msr *priv = clk_msr_id->priv;
	unsigned int val;
	unsigned long flags;
	int cnt = 0;

	spin_lock_irqsave(&measure_lock, flags);

	regmap_write(priv->regmap, priv->data->reg0_offset, 0);

	/* Set measurement duration */
	regmap_update_bits(priv->regmap, priv->data->reg0_offset, MSR_DURATION,
			   FIELD_PREP(MSR_DURATION, duration - 1));

	/* Set ID */
	regmap_update_bits(priv->regmap, priv->data->reg0_offset, MSR_CLK_SRC,
			   FIELD_PREP(MSR_CLK_SRC, clk_msr_id->id));

	/* Enable & Start */
	regmap_update_bits(priv->regmap, priv->data->reg0_offset,
			   MSR_RUN | MSR_ENABLE,
			   MSR_RUN | MSR_ENABLE);

	do {
		regmap_read(priv->regmap, priv->data->reg0_offset, &val);
		udelay(10);
		cnt++;
		if (cnt > 1000)
			pr_err("measure timeout\n");
	} while (val & MSR_BUSY);

	/* Disable */
	regmap_update_bits(priv->regmap, priv->data->reg0_offset,
			   MSR_ENABLE, 0);

	/* Get the value in multiple of gate time counts */
	regmap_read(priv->regmap, priv->data->reg2_offset, &val);

	spin_unlock_irqrestore(&measure_lock, flags);

	if (val > MSR_VAL_MASK) {
		pr_err("measure val error\n");
		return -EINVAL;
	} else if (val == MSR_VAL_MASK) {
		return 0;
	}

	return DIV_ROUND_CLOSEST_ULL((val & MSR_VAL_MASK) * 1000000ULL,
				     duration);
}

static int meson_measure_best_id(struct meson_msr_id *clk_msr_id,
				    unsigned int *precision)
{
	unsigned int duration = DIV_MAX;
	int ret;

	/* Start from max duration and down to min duration */
	do {
		ret = meson_measure_id(clk_msr_id, duration);
		if (ret >= 0)
			*precision = (2 * 1000000) / duration;
		else
			duration -= DIV_STEP;
	} while (duration >= DIV_MIN && ret == -EINVAL);

	return ret;
}

int meson_clk_measure(unsigned int id)
{
	struct meson_msr_id *clk_msr_id = NULL;
	unsigned int precision = 0;
	int val;

	clk_msr_id = &glo_meson_msr->data->msr_table[id];

	val = meson_measure_best_id(clk_msr_id, &precision);
	if (val < 0) {
		pr_err("measure failed\n");
		return val;
	}

	return val;
}
EXPORT_SYMBOL_GPL(meson_clk_measure);

static int clk_msr_show(struct seq_file *s, void *data)
{
	struct meson_msr_id *clk_msr_id = s->private;
	unsigned int precision = 0;
	int val;

	val = meson_measure_best_id(clk_msr_id, &precision);
	if (val < 0)
		return val;

	seq_printf(s, "%d\t+/-%dHz\n", val, precision);

	return 0;
}
DEFINE_SHOW_ATTRIBUTE(clk_msr);

static int clk_msr_summary_show(struct seq_file *s, void *data)
{
	struct meson_msr_id *msr_table = s->private;
	unsigned int precision = 0;
	int val, i;

	seq_puts(s, "  clock                     rate    precision\n");
	seq_puts(s, "---------------------------------------------\n");

	for (i = 0 ; i < measure_num; ++i) {
		if (!msr_table[i].name)
			continue;

		val = meson_measure_best_id(&msr_table[i], &precision);
		if (val < 0) {
			pr_err("measure failed\n");
			return val;
		}

		seq_printf(s, "[%d] %-20s %10d    +/-%dHz\n", i,
			   msr_table[i].name, val, precision);
	}

	return 0;
}
DEFINE_SHOW_ATTRIBUTE(clk_msr_summary);

static struct regmap_config meson_clk_msr_regmap_config = {
	.reg_bits = 32,
	.val_bits = 32,
	.reg_stride = 4,
};

static int meson_msr_probe(struct platform_device *pdev)
{
	struct meson_msr *priv;
	struct resource *res;
	struct dentry *root, *clks;
	void __iomem *base;
	int i;
	struct meson_msr_id *table;
	struct meson_msr_data *msr_data;

	priv = devm_kzalloc(&pdev->dev, sizeof(struct meson_msr),
			    GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->data = (struct meson_msr_data *)device_get_match_data(&pdev->dev);
	if (!priv->data) {
		dev_err(&pdev->dev, "failed to get match data\n");
		return -ENODEV;
	}
	table = devm_kzalloc(&pdev->dev,
			     priv->data->table_size * sizeof(*table),
			     GFP_KERNEL);
	if (!table)
		return -ENOMEM;

	memcpy(table, priv->data->msr_table,
	       priv->data->table_size * sizeof(*table));
	priv->data->msr_table = table;
	measure_num = priv->data->table_size;

	/* alloc space for measure data, store the platform data */
	msr_data = devm_kzalloc(&pdev->dev, sizeof(struct meson_msr_data),
				GFP_KERNEL);
	if (!msr_data)
		return -ENOMEM;
	memcpy(msr_data, priv->data, sizeof(struct meson_msr_data));
	priv->data = msr_data;

	spin_lock_init(&measure_lock);
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(base)) {
		dev_err(&pdev->dev, "io resource mapping failed\n");
		return PTR_ERR(base);
	}

	meson_clk_msr_regmap_config.max_register = priv->data->reg2_offset;

	priv->regmap = devm_regmap_init_mmio(&pdev->dev, base,
					     &meson_clk_msr_regmap_config);
	if (IS_ERR(priv->regmap))
		return PTR_ERR(priv->regmap);

	root = debugfs_create_dir("meson-clk-msr", NULL);
	clks = debugfs_create_dir("clks", root);

	debugfs_create_file("measure_summary", 0444, root,
			    priv->data->msr_table, &clk_msr_summary_fops);

	glo_meson_msr = priv;

	for (i = 0 ; i < priv->data->table_size; ++i) {
		if (!priv->data->msr_table[i].name)
			continue;

		priv->data->msr_table[i].priv = priv;

		debugfs_create_file(priv->data->msr_table[i].name, 0444, clks,
				    &priv->data->msr_table[i], &clk_msr_fops);
	}

	return 0;
}

static struct meson_msr_data meson_gx_data __initdata = {
	.msr_table = (struct meson_msr_id *)&clk_msr_gx,
	.table_size = ARRAY_SIZE(clk_msr_gx),
	.duty_offset = 0x0,
	.reg0_offset = 0x4,
	.reg1_offset = 0x8,
	.reg2_offset = 0xc,
};

static struct meson_msr_data meson_m8_data __initdata = {
	.msr_table = (struct meson_msr_id *)&clk_msr_m8,
	.table_size = ARRAY_SIZE(clk_msr_m8),
	.duty_offset = 0x0,
	.reg0_offset = 0x4,
	.reg1_offset = 0x8,
	.reg2_offset = 0xc,
};

static struct meson_msr_data meson_axg_data __initdata = {
	.msr_table = (struct meson_msr_id *)&clk_msr_axg,
	.table_size = ARRAY_SIZE(clk_msr_axg),
	.duty_offset = 0x0,
	.reg0_offset = 0x4,
	.reg1_offset = 0x8,
	.reg2_offset = 0xc,
};

static struct meson_msr_data meson_g12a_data __initdata = {
	.msr_table = (struct meson_msr_id *)&clk_msr_g12a,
	.table_size = ARRAY_SIZE(clk_msr_g12a),
	.duty_offset = 0x0,
	.reg0_offset = 0x4,
	.reg1_offset = 0x8,
	.reg2_offset = 0xc,
};

static struct meson_msr_data meson_sm1_data __initdata = {
	.msr_table = (struct meson_msr_id *)&clk_msr_sm1,
	.table_size = ARRAY_SIZE(clk_msr_sm1),
	.duty_offset = 0x0,
	.reg0_offset = 0x4,
	.reg1_offset = 0x8,
	.reg2_offset = 0xc,
};

static struct meson_msr_data meson_tm2_data __initdata = {
	.msr_table = (struct meson_msr_id *)&clk_msr_tm2,
	.table_size = ARRAY_SIZE(clk_msr_tm2),
	.duty_offset = 0x0,
	.reg0_offset = 0x4,
	.reg1_offset = 0x8,
	.reg2_offset = 0xc,
};

static struct meson_msr_data meson_sc2_data __initdata = {
	.msr_table = (struct meson_msr_id *)&clk_msr_sc2,
	.table_size = ARRAY_SIZE(clk_msr_sc2),
	.duty_offset = (0x6 * 4),
	.reg0_offset = 0x0,
	.reg1_offset = 0x4,
	.reg2_offset = 0x8,
};

static struct meson_msr_data meson_t5d_data __initdata = {
	.msr_table = (struct meson_msr_id *)&clk_msr_t5d,
	.table_size = ARRAY_SIZE(clk_msr_t5d),
	.duty_offset = 0x0,
	.reg0_offset = 0x4,
	.reg1_offset = 0x8,
	.reg2_offset = 0xc,
};

static struct meson_msr_data meson_t7_data __initdata = {
	.msr_table = (struct meson_msr_id *)&clk_msr_t7,
	.table_size = ARRAY_SIZE(clk_msr_t7),
	.duty_offset = (0x6 * 4),
	.reg0_offset = 0x0,
	.reg1_offset = 0x4,
	.reg2_offset = 0x8,
};

static struct meson_msr_data meson_s4_data __initdata = {
	.msr_table = (struct meson_msr_id *)&clk_msr_s4,
	.table_size = ARRAY_SIZE(clk_msr_s4),
	.duty_offset = (0x6 * 4),
	.reg0_offset = 0x0,
	.reg1_offset = 0x4,
	.reg2_offset = 0x8,
};

static const struct of_device_id meson_msr_match_table[] = {
	{
		.compatible = "amlogic,meson-gx-clk-measure",
		.data = &meson_gx_data,
	},
	{
		.compatible = "amlogic,meson8-clk-measure",
		.data = &meson_m8_data,
	},
	{
		.compatible = "amlogic,meson8b-clk-measure",
		.data = &meson_m8_data,
	},
	{
		.compatible = "amlogic,meson-axg-clk-measure",
		.data = &meson_axg_data,
	},
	{
		.compatible = "amlogic,meson-g12a-clk-measure",
		.data = &meson_g12a_data,
	},

	{
		.compatible = "amlogic,meson-sm1-clk-measure",
		.data = &meson_sm1_data,
	},

	{
		.compatible = "amlogic,meson-tm2-clk-measure",
		.data = &meson_tm2_data,
	},

	{
		.compatible = "amlogic,meson-sc2-clk-measure",
		.data = &meson_sc2_data,
	},

	{
		.compatible = "amlogic,meson-t5d-clk-measure",
		.data = &meson_t5d_data,
	},

	{
		.compatible = "amlogic,meson-t7-clk-measure",
		.data = &meson_t7_data,
	},

	{
		.compatible = "amlogic,meson-s4-clk-measure",
		.data = &meson_s4_data,
	},

	{ /* sentinel */ }
};

static struct platform_driver meson_msr_driver = {
	.probe	= meson_msr_probe,
	.driver = {
		.name		= "meson_msr",
		.of_match_table	= meson_msr_match_table,
	},
};
builtin_platform_driver(meson_msr_driver);

MODULE_LICENSE("GPL v2");
