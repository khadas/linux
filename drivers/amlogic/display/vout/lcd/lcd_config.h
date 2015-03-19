
#ifndef LCD_CONFIG_H
#define LCD_CONFIG_H

#include <linux/types.h>

/* **********************************
 * lcd driver version
 * ********************************** */
#define LCD_DRV_DATE      "20150310"

/* **********************************
 * lcd driver version
 * ********************************** */
/* for GAMMA_CNTL_PORT */
	#define LCD_GAMMA_VCOM_POL       7
	#define LCD_GAMMA_RVS_OUT        6
	#define LCD_ADR_RDY              5
	#define LCD_WR_RDY               4
	#define LCD_RD_RDY               3
	#define LCD_GAMMA_TR             2
	#define LCD_GAMMA_SET            1
	#define LCD_GAMMA_EN             0
/* for GAMMA_ADDR_PORT */
	#define LCD_H_RD                 12
	#define LCD_H_AUTO_INC           11
	#define LCD_H_SEL_R              10
	#define LCD_H_SEL_G              9
	#define LCD_H_SEL_B              8
	#define LCD_HADR_MSB             7
	#define LCD_HADR                 0

/* for POL_CNTL_ADDR */
	#define LCD_DCLK_SEL             14
	#define LCD_TCON_VSYNC_SEL_DVI   11
	#define LCD_TCON_HSYNC_SEL_DVI   10
	#define LCD_TCON_DE_SEL_DVI      9
	#define LCD_CPH3_POL             8
	#define LCD_CPH2_POL             7
	#define LCD_CPH1_POL             6
	#define LCD_TCON_DE_SEL          5
	#define LCD_TCON_VS_SEL          4
	#define LCD_TCON_HS_SEL          3
	#define LCD_DE_POL               2
	#define LCD_VS_POL               1
	#define LCD_HS_POL               0

/* for DITH_CNTL_ADDR */
	#define LCD_DITH10_EN            10
	#define LCD_DITH8_EN             9
	#define LCD_DITH_MD              8
	#define LCD_DITH10_CNTL_MSB      7
	#define LCD_DITH10_CNTL          4
	#define LCD_DITH8_CNTL_MSB       3
	#define LCD_DITH8_CNTL           0

/* for INV_CNT_ADDR */
	#define LCD_INV_EN               4
	#define LCD_INV_CNT_MSB          3
	#define LCD_INV_CNT              0

/* for TCON_MISC_SEL_ADDR */
	#define LCD_STH2_SEL             12
	#define LCD_STH1_SEL             11
	#define LCD_OEH_SEL              10
	#define LCD_VCOM_SEL             9
	#define LCD_DB_LINE_SW           8
	#define LCD_CPV2_SEL             7
	#define LCD_CPV1_SEL             6
	#define LCD_STV2_SEL             5
	#define LCD_STV1_SEL             4
	#define LCD_OEV_UNITE            3
	#define LCD_OEV3_SEL             2
	#define LCD_OEV2_SEL             1
	#define LCD_OEV1_SEL             0

/* for DUAL_PORT_CNTL_ADDR */
	#define LCD_ANALOG_SEL_CPH3      8
	#define LCD_ANALOG_3PHI_CLK_SEL  7
	#define LCD_LVDS_SEL54           6
	#define LCD_LVDS_SEL27           5
	#define LCD_TTL_SEL              4
	#define LCD_DUAL_PIXEL           3
	#define LCD_PORT_SWP             2
	#define LCD_RGB_SWP              1
	#define LCD_BIT_SWP              0

/* for LVDS_PACK_CNTL_ADDR */
	#define LCD_LD_CNT_MSB           7
	#define LCD_LD_CNT               5
	#define LCD_PN_SWP               4
	#define LCD_RES                  3
	#define LCD_LVDS_PORT_SWP        2
	#define LCD_PACK_RVS             1
	#define LCD_PACK_LITTLE          0

/* for LVDS_PACK_CNTL_ADDR */
	#define LVDS_USE_TCON            7
	#define LVDS_DUAL                6
	#define PN_SWP                   5
	#define LSB_FIRST                4
	#define LVDS_RESV                3
	#define ODD_EVEN_SWP             2
	#define LVDS_REPACK              0

/* **********************************
 * delay between video encoder & tcon
 * ********************************** */
#define MIPI_DELAY                       8
#define LVDS_DELAY                       8
#define EDP_DELAY                        8
#define TTL_DELAY                        19

/* **********************************
 * lcd gamma control
 * ********************************** */
static const unsigned int gamma_sel_table[3] = {
	LCD_H_SEL_R,
	LCD_H_SEL_G,
	LCD_H_SEL_B,
};

/* **********************************
 * clk parameter auto generation
 * ********************************** */
/* clk parameters bit */
/* pll_ctrl */
#define PLL_CTRL_LOCK               31
#define PLL_CTRL_EN                 30
#define PLL_CTRL_RST                29
/* [10:9] */
#define PLL_CTRL_OD                 9
/* [28:24] */
#define PLL_CTRL_N                  24
/* [8:0] */
#define PLL_CTRL_M                  0

/* div_ctrl */
/* [26:24] */
#define DIV_CTRL_EDP_DIV1           24
/* [23:20] */
#define DIV_CTRL_EDP_DIV0           20
/* [14:12] */
#define DIV_CTRL_DIV_POST           12
#define DIV_CTRL_LVDS_CLK_EN        11
/*#define DIV_CTRL_PHY_CLK_DIV2       10*/
/* [9:8] */
#define DIV_CTRL_POST_SEL           8
/* [6:4] */
#define DIV_CTRL_DIV_PRE            4

/* clk_ctrl */
/* [27:16] */
#define CLK_CTRL_FRAC               16
/* [14:12] */
#define CLK_CTRL_LEVEL              12

#define PLL_WAIT_LOCK_CNT           200

/* clk frequency limit */
/* PLL */
#define PLL_M_MIN                   2
#define PLL_M_MAX                   511
#define PLL_N_MIN                   1
#define PLL_N_MAX                   1
#define PLL_FREF_MIN                (5 * 1000)
#define PLL_FREF_MAX                (25 * 1000)
#define PLL_VCO_MIN                 (1200 * 1000)
#define PLL_VCO_MAX                 (3000 * 1000)
/* VIDEO */
#define MIPI_PHY_MAX_CLK_IN         (1000 * 1000)
#define DIV_PRE_MAX_CLK_IN          (1500 * 1000)
#define DIV_POST_MAX_CLK_IN         (1000 * 1000)
#define CRT_VID_MAX_CLK_IN          (1300 * 1000)
#define ENCL_MAX_CLK_IN             (333 * 1000)

/* lcd interface video clk */
#define MIPI_MAX_VID_CLK_IN         ENCL_MAX_CLK_IN
#define LVDS_MAX_VID_CLK_IN         ENCL_MAX_CLK_IN
#define EDP_MAX_VID_CLK_IN          ENCL_MAX_CLK_IN
#define TTL_MAX_VID_CLK_IN          ENCL_MAX_CLK_IN

/* clk max error */
#define MAX_ERROR                   (2 * 1000)

#define CRT_VID_DIV_MAX             15

#define OD_SEL_MAX                  3
#define DIV_PRE_SEL_MAX             6
#define EDP_DIV0_SEL_MAX            15
#define EDP_DIV1_SEL_MAX            8

static const unsigned int od_table[4] = {
	1, 2, 4, 8
};
static const unsigned int div_pre_table[6] = {
	1, 2, 3, 4, 5, 6
};
static const unsigned int edp_div0_table[15] = {
	1, 2, 3, 4, 5, 7, 8, 9, 11, 13, 17, 19, 23, 29, 31
};
static const unsigned int edp_div1_table[8] = {
	1, 2, 4, 5, 6, 7, 9, 13
};

struct lcd_clk_config_s { /* unit: kHz */
	/* IN-OUT parameters */
	unsigned int fin;
	unsigned int fout;

	/* pll parameters */
	unsigned int pll_m;
	unsigned int pll_n;
	unsigned int pll_od;
	unsigned int pll_level;
	unsigned int pll_frac;
	unsigned int pll_fout;
	unsigned int ss_level;
	unsigned int edp_div0;
	unsigned int edp_div1;
	unsigned int div_pre;
	unsigned int div_post;
	unsigned int xd;
	unsigned int err_fmin;

	/* clk path node parameters */
	unsigned int pll_m_max;
	unsigned int pll_m_min;
	unsigned int pll_n_max;
	unsigned int pll_n_min;
	unsigned int pll_ref_fmax;
	unsigned int pll_ref_fmin;
	unsigned int pll_vco_fmax;
	unsigned int pll_vco_fmin;
	unsigned int pll_out_fmax;
	unsigned int pll_out_fmin;
	unsigned int div_pre_out_fmax;
	unsigned int div_post_out_fmax;
	unsigned int xd_out_fmax;
	unsigned int if_logic_fmax;
	unsigned int if_phy_fmax;
	unsigned int if_phy_fmin;
};

extern struct lcd_clk_config_s clk_Conf;

/* **********************************
 * PHY Config
 * ********************************** */
/* bit[15:11] */
#define BIT_DPHY_LANE           11

/* LVDS */
#define LVDS_LANE_0             (1 << 4)
#define LVDS_LANE_1             (1 << 3)
#define LVDS_LANE_2             (1 << 1)
#define LVDS_LANE_3             (1 << 0)
#define LVDS_LANE_CLK           (1 << 2)
#define LVDS_LANE_COUNT_3       (LVDS_LANE_CLK | LVDS_LANE_0 |\
					LVDS_LANE_1 | LVDS_LANE_2)
#define LVDS_LANE_COUNT_4       (LVDS_LANE_CLK | LVDS_LANE_0 |\
					LVDS_LANE_1 | LVDS_LANE_2 | LVDS_LANE_3)

/* MIPI-DSI */
#define DSI_LANE_0              (1 << 4)
#define DSI_LANE_1              (1 << 3)
#define DSI_LANE_2              (1 << 2)
#define DSI_LANE_3              (1 << 1)
#define DSI_LANE_CLK            (1 << 0)
#define DSI_LANE_COUNT_1        (DSI_LANE_CLK | DSI_LANE_0)
#define DSI_LANE_COUNT_2        (DSI_LANE_CLK | DSI_LANE_0 | DSI_LANE_1)
#define DSI_LANE_COUNT_3        (DSI_LANE_CLK | DSI_LANE_0 |\
					DSI_LANE_1 | DSI_LANE_2)
#define DSI_LANE_COUNT_4        (DSI_LANE_CLK | DSI_LANE_0 |\
					DSI_LANE_1 | DSI_LANE_2 | DSI_LANE_3)

/* eDP */
#define EDP_LANE_AUX            (1 << 4)
#define EDP_LANE_0              (1 << 3)
#define EDP_LANE_1              (1 << 2)
#define EDP_LANE_2              (1 << 1)
#define EDP_LANE_3              (1 << 0)
#define EDP_LANE_COUNT_1        (EDP_LANE_AUX | EDP_LANE_0)
#define EDP_LANE_COUNT_2        (EDP_LANE_AUX | EDP_LANE_0 | EDP_LANE_1)
#define EDP_LANE_COUNT_4        (EDP_LANE_AUX | EDP_LANE_0 | EDP_LANE_1 |\
					EDP_LANE_2 | EDP_LANE_3)
#endif
