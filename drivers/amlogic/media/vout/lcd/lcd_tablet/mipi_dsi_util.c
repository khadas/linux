// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 *
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
 */

#include <linux/types.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>
#ifdef CONFIG_AMLOGIC_LCD_EXTERN
#include <linux/amlogic/media/vout/lcd/lcd_extern.h>
#endif
#include "../lcd_reg.h"
#include "../lcd_common.h"
#include "mipi_dsi_util.h"

/* *************************************************************
 * Define MIPI DSI Default config
 */
/* Range [0,3] */
#define MIPI_DSI_VIRTUAL_CHAN_ID        0
/* Define DSI command transfer type: high speed or low power */
#define MIPI_DSI_CMD_TRANS_TYPE         DCS_TRANS_LP
/* Define if DSI command need ack: req_ack or no_ack */
#define MIPI_DSI_DCS_ACK_TYPE           MIPI_DSI_DCS_NO_ACK
/* Applicable only to video mode. Define data transfer method:
 *    non-burst sync pulse; non-burst sync event; or burst.
 */

#define MIPI_DSI_COLOR_18BIT            COLOR_18BIT_CFG_2
#define MIPI_DSI_COLOR_24BIT            COLOR_24BIT
#define MIPI_DSI_TEAR_SWITCH            MIPI_DCS_DISABLE_TEAR
#define CMD_TIMEOUT_CNT                 50000
/* ************************************************************* */

static char *operation_mode_table[] = {
	"VIDEO",
	"COMMAND",
};

static char *video_mode_type_table[] = {
	"SYNC_PULSE",
	"SYNC_EVENT",
	"BURST_MODE",
};

static char *video_data_type_table[] = {
	"COLOR_16BIT_CFG_1",
	"COLOR_16BIT_CFG_2",
	"COLOR_16BIT_CFG_3",
	"COLOR_18BIT_CFG_1",
	"COLOR_18BIT_CFG_2(loosely)",
	"COLOR_24BIT",
	"COLOR_20BIT_LOOSE",
	"COLOR_24_BIT_YCBCR",
	"COLOR_16BIT_YCBCR",
	"COLOR_30BIT",
	"COLOR_36BIT",
	"COLOR_12BIT",
	"COLOR_RGB_111",
	"COLOR_RGB_332",
	"COLOR_RGB_444",
	"un-support type",
};

static char *phy_switch_table[] = {
	"AUTO",
	"STANDARD",
	"SLOW",
};

static struct dsi_phy_s dsi_phy_config;
static struct dsi_vid_s dsi_vconf;
static unsigned short dsi_rx_n;

static void mipi_dsi_init_table_print(struct dsi_config_s *dconf, int on_off)
{
	int i, j, n;
	int n_max;
	unsigned char *dsi_table;
	char str[200];
	int len = 0;

	if (on_off) {
		if (!dconf->dsi_init_on)
			return;
		dsi_table = dconf->dsi_init_on;
		n_max = DSI_INIT_ON_MAX;
		pr_info("DSI INIT ON:\n");
	} else {
		if (!dconf->dsi_init_off)
			return;
		dsi_table = dconf->dsi_init_off;
		n_max = DSI_INIT_OFF_MAX;
		pr_info("DSI INIT OFF:\n");
	}
	i = 0;
	n = 0;
	while ((i + DSI_CMD_SIZE_INDEX) < n_max) {
		n = dsi_table[i + DSI_CMD_SIZE_INDEX];
		if (dsi_table[i] == LCD_EXT_CMD_TYPE_END) {
			if (n == 0xff) {
				pr_info("  0x%02x,0x%02x,\n",
					dsi_table[i], dsi_table[i + 1]);
				break;
			}
			if (n == 0) {
				pr_info("  0x%02x,%d,\n",
					dsi_table[i], dsi_table[i + 1]);
				break;
			}
			n = 0;
			pr_info("  0x%02x,%d,\n",
				dsi_table[i], dsi_table[i + 1]);
		} else if ((dsi_table[i] == LCD_EXT_CMD_TYPE_GPIO) ||
			(dsi_table[i] == LCD_EXT_CMD_TYPE_DELAY)) {
			len = sprintf(str, "  0x%02x,%d,", dsi_table[i], n);
			for (j = 0; j < n; j++) {
				len += sprintf(str + len, "%d,",
					dsi_table[i + 2 + j]);
			}
			pr_info("%s\n", str);
		} else if ((dsi_table[i] & 0xf) == 0x0) {
			pr_info("  dsi_init_%s wrong data_type: 0x%02x\n",
				on_off ? "on" : "off", dsi_table[i]);
			break;
		} else {
			len = sprintf(str, "  0x%02x,%d,", dsi_table[i], n);
			for (j = 0; j < n; j++) {
				len += sprintf(str + len, "0x%02x,",
					       dsi_table[i + 2 + j]);
			}
			pr_info("%s\n", str);
		}
		i += (n + 2);
	}
}

static void mipi_dsi_dphy_print_info(struct lcd_config_s *pconf)
{
	unsigned int temp;

	temp = lcd_do_div(pconf->timing.bit_rate, 1000);
	temp = ((1000000 * 100) / temp) * 8;
	pr_info("MIPI DSI DPHY timing (unit: ns)\n"
		"  UI:                %d.%02d\n"
		"  LP TESC:           %d\n"
		"  LP LPX:            %d\n"
		"  LP TA_SURE:        %d\n"
		"  LP TA_GO:          %d\n"
		"  LP TA_GET:         %d\n"
		"  HS EXIT:           %d\n"
		"  HS TRAIL:          %d\n"
		"  HS ZERO:           %d\n"
		"  HS PREPARE:        %d\n"
		"  CLK TRAIL:         %d\n"
		"  CLK POST:          %d\n"
		"  CLK ZERO:          %d\n"
		"  CLK PREPARE:       %d\n"
		"  CLK PRE:           %d\n"
		"  INIT:              %d\n"
		"  WAKEUP:            %d\n"
		"  state_change:      %d\n\n",
		(temp / 8 / 100), ((temp / 8) % 100),
		(temp * dsi_phy_config.lp_tesc / 100),
		(temp * dsi_phy_config.lp_lpx / 100),
		(temp * dsi_phy_config.lp_ta_sure / 100),
		(temp * dsi_phy_config.lp_ta_go / 100),
		(temp * dsi_phy_config.lp_ta_get / 100),
		(temp * dsi_phy_config.hs_exit / 100),
		(temp * dsi_phy_config.hs_trail / 100),
		(temp * dsi_phy_config.hs_zero / 100),
		(temp * dsi_phy_config.hs_prepare / 100),
		(temp * dsi_phy_config.clk_trail / 100),
		(temp * dsi_phy_config.clk_post / 100),
		(temp * dsi_phy_config.clk_zero / 100),
		(temp * dsi_phy_config.clk_prepare / 100),
		(temp * dsi_phy_config.clk_pre / 100),
		(temp * dsi_phy_config.init / 100),
		(temp * dsi_phy_config.wakeup / 100),
		dsi_phy_config.state_change);
}

static void mipi_dsi_video_print_info(struct dsi_config_s *dconf)
{
	if (dconf->video_mode_type != BURST_MODE) {
		pr_info("MIPI DSI NON-BURST setting:\n"
			"  multi_pkt_en:          %d\n"
			"  vid_num_chunks:        %d\n"
			"  pixel_per_chunk:       %d\n"
			"  byte_per_chunk:        %d\n"
			"  vid_null_size:         %d\n\n",
			dsi_vconf.multi_pkt_en, dsi_vconf.vid_num_chunks,
			dsi_vconf.pixel_per_chunk, dsi_vconf.byte_per_chunk,
			dsi_vconf.vid_null_size);
	}
}

static void mipi_dsi_host_print_info(struct lcd_config_s *pconf)
{
	unsigned int esc_clk, factor;
	struct dsi_config_s *dconf;

	dconf = &pconf->control.mipi_cfg;
	esc_clk = lcd_do_div(pconf->timing.bit_rate,  (8 * dsi_phy_config.lp_tesc));
	factor = dconf->factor_numerator;
	factor = ((factor * 1000 / dconf->factor_denominator) + 5) / 10;

	if (dconf->check_en) {
		pr_info("MIPI DSI check state:\n"
			"  check_reg:             0x%02x\n"
			"  check_cnt:             %d\n"
			"  check_state            %d\n\n",
			dconf->check_reg, dconf->check_cnt, dconf->check_state);
	}

	pr_info("MIPI DSI Config:\n"
		"  lane num:              %d\n"
		"  bit rate max:          %dMHz\n"
		"  bit rate:              %lldHz\n"
		"  pclk lanebyte factor:  %d(/100)\n"
		"  operation mode:\n"
		"      init:              %s(%d)\n"
		"      display:           %s(%d)\n"
		"  video mode type:       %s(%d)\n"
		"  clk always hs:         %d\n"
		"  phy switch:            %s(%d)\n"
		"  data format:           %s\n"
		"  lp escape clock:       %d.%03dMHz\n",
		dconf->lane_num, dconf->bit_rate_max,
		pconf->timing.bit_rate,
		factor,
		operation_mode_table[dconf->operation_mode_init],
		dconf->operation_mode_init,
		operation_mode_table[dconf->operation_mode_display],
		dconf->operation_mode_display,
		video_mode_type_table[dconf->video_mode_type],
		dconf->video_mode_type,
		dconf->clk_always_hs,
		phy_switch_table[dconf->phy_switch],
		dconf->phy_switch,
		video_data_type_table[dconf->dpi_data_format],
		(esc_clk / 1000000), (esc_clk % 1000000) / 1000);

	mipi_dsi_init_table_print(dconf, 1); /* dsi_init_on table */
	mipi_dsi_init_table_print(dconf, 0); /* dsi_init_off table */

	pr_info("extern init:             %d\n\n", dconf->extern_init);
}

void mipi_dsi_print_info(struct lcd_config_s *pconf)
{
	mipi_dsi_host_print_info(pconf);

	mipi_dsi_dphy_print_info(pconf);
	mipi_dsi_video_print_info(&pconf->control.mipi_cfg);
}

int lcd_mipi_dsi_init_table_detect(struct aml_lcd_drv_s *pdrv,
				   struct device_node *m_node, int on_off)
{
	struct dsi_config_s *dconf = &pdrv->config.control.mipi_cfg;
	int ret = 0;
	unsigned char *dsi_table;
	unsigned char propname[15];
	int i, j = 0, n_max;
	unsigned int *para; /* num 100 array */
	unsigned char type, cmd_size;
	unsigned int val;

	if (on_off) {
		n_max = DSI_INIT_ON_MAX;
		sprintf(propname, "dsi_init_on");
	} else {
		n_max = DSI_INIT_OFF_MAX;
		sprintf(propname, "dsi_init_off");
	}

	dsi_table = kzalloc(n_max, GFP_KERNEL);
	if (!dsi_table) {
		LCDERR("%s: Not enough memory\n", __func__);
		return -1;
	}

	para = kcalloc(n_max, sizeof(unsigned int), GFP_KERNEL);
	if (!para) {
		LCDERR("%s: Not enough memory\n", __func__);
		kfree(dsi_table);
		return -1;
	}

	ret = of_property_read_u32_index(m_node, propname, 0, &para[0]);
	if (ret) {
		LCDERR("failed to get %s\n", propname);
		goto lcd_mipi_dsi_init_table_detect_err;
	}
	i = 0;
	while ((i + DSI_CMD_SIZE_INDEX) < n_max) {
		ret = of_property_read_u32_index(m_node, propname, i, &val);
		if (ret) {
			LCDERR("failed to get %s\n", propname);
			goto lcd_mipi_dsi_init_table_detect_err;
		}
		type = (unsigned char)val;
		ret = of_property_read_u32_index(m_node, propname,
						 (i + DSI_CMD_SIZE_INDEX),
						 &val);
		if (ret) {
			LCDERR("failed to get %s\n", propname);
			goto lcd_mipi_dsi_init_table_detect_err;
		}
		cmd_size = (unsigned char)val;
		if (type == LCD_EXT_CMD_TYPE_END) {
			if (val == 0xff || val == 0)
				break;
			cmd_size = 0;
		}
		if (cmd_size == 0) {
			i += 2;
			continue;
		}
		if ((i + 2 + cmd_size) > n_max) {
			LCDERR("%s cmd_size out of support\n", propname);
			goto lcd_mipi_dsi_init_table_detect_err;
		}

		if (type == LCD_EXT_CMD_TYPE_GPIO) {
			/* probe gpio */
			ret = of_property_read_u32_index(m_node, propname,
							 (i + 2), &val);
			if (ret) {
				LCDERR("failed to get %s\n", propname);
				goto lcd_mipi_dsi_init_table_detect_err;
			}
			lcd_cpu_gpio_probe(pdrv, val);
		} else if (type == LCD_EXT_CMD_TYPE_CHECK) {
			ret = of_property_read_u32_index(m_node, propname,
							 (j + 1), &para[0]);
			if (ret) {
				LCDERR("failed to get %s\n", propname);
				goto lcd_mipi_dsi_init_table_detect_err;
			}
			ret = of_property_read_u32_index(m_node, propname,
							 (j + 2), &para[1]);
			if (ret) {
				LCDERR("failed to get %s\n", propname);
				goto lcd_mipi_dsi_init_table_detect_err;
			}
			dconf->check_reg = para[0];
			dconf->check_cnt = para[1];
			if (dconf->check_cnt > 0)
				dconf->check_en = 1;
		}
		i += (cmd_size + 2);
	}
	i += 2;
	ret = of_property_read_u32_array(m_node, propname, &para[0], i);
	if (ret) {
		LCDERR("failed to get %s\n", propname);
		goto lcd_mipi_dsi_init_table_detect_err;
	}
	for (j = 0; j < i; j++)
		dsi_table[j] = (unsigned char)(para[j] & 0xff);

	if (on_off)
		dconf->dsi_init_on = dsi_table;
	else
		dconf->dsi_init_off = dsi_table;

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		mipi_dsi_init_table_print(dconf, on_off);

	kfree(para);
	return 0;

lcd_mipi_dsi_init_table_detect_err:
	kfree(para);
	kfree(dsi_table);
	return ret;
}

/* *************************************************************
 * Function: mipi_dcs_set
 * Configure relative registers in command mode
 * Parameters:   int trans_type, // 0: high speed, 1: low power
 *               int req_ack,    // 1: request ack, 0: do not need ack
 *               int tear_en     // 1: enable tear ack, 0: disable tear ack
 */
static void mipi_dcs_set(struct aml_lcd_drv_s *pdrv, int trans_type,
			 int req_ack, int tear_en)
{
	dsi_host_write(pdrv, MIPI_DSI_DWC_CMD_MODE_CFG_OS,
		       (trans_type << BIT_MAX_RD_PKT_SIZE) |
		       (trans_type << BIT_DCS_LW_TX)    |
		       (trans_type << BIT_DCS_SR_0P_TX) |
		       (trans_type << BIT_DCS_SW_1P_TX) |
		       (trans_type << BIT_DCS_SW_0P_TX) |
		       (trans_type << BIT_GEN_LW_TX)    |
		       (trans_type << BIT_GEN_SR_2P_TX) |
		       (trans_type << BIT_GEN_SR_1P_TX) |
		       (trans_type << BIT_GEN_SR_0P_TX) |
		       (trans_type << BIT_GEN_SW_2P_TX) |
		       (trans_type << BIT_GEN_SW_1P_TX) |
		       (trans_type << BIT_GEN_SW_0P_TX) |
		       (req_ack << BIT_ACK_RQST_EN)     |
		       (tear_en << BIT_TEAR_FX_EN));

	if (tear_en == MIPI_DCS_ENABLE_TEAR) {
		/* Enable Tear Interrupt if tear_en is valid */
		dsi_host_set_mask(pdrv, MIPI_DSI_TOP_INTR_CNTL_STAT,
				  (0x1 << BIT_EDPITE_INT_EN));
		/* Enable Measure Vsync */
		dsi_host_set_mask(pdrv, MIPI_DSI_TOP_MEAS_CNTL,
				  (0x1 << BIT_VSYNC_MEAS_EN) |
				  (0x1 << BIT_TE_MEAS_EN));
	}

	/* Packet header settings */
	dsi_host_write(pdrv, MIPI_DSI_DWC_PCKHDL_CFG_OS,
		       (1 << BIT_CRC_RX_EN)  |
		       (1 << BIT_ECC_RX_EN)  |
		       (req_ack << BIT_BTA_EN)     |
		       (0 << BIT_EOTP_RX_EN) |
		       (0 << BIT_EOTP_TX_EN));
}

/* *************************************************************
 * Function: check_phy_st
 * Check the status of the dphy: phylock and stopstateclklane,
 *  to decide if the DPHY is ready
 */
#define DPHY_TIMEOUT    200000
static void check_phy_status(struct aml_lcd_drv_s *pdrv)
{
	int i = 0;

	while (dsi_host_getb(pdrv, MIPI_DSI_DWC_PHY_STATUS_OS,
			     BIT_PHY_LOCK, 1) == 0) {
		if (i++ >= DPHY_TIMEOUT) {
			LCDERR("%s: phy_lock timeout\n", __func__);
			break;
		}
		lcd_delay_us(6);
	}

	i = 0;
	lcd_delay_us(10);
	while (dsi_host_getb(pdrv, MIPI_DSI_DWC_PHY_STATUS_OS,
			     BIT_PHY_STOPSTATECLKLANE, 1) == 0) {
		if (i == 0)
			LCDPR(" Waiting STOP STATE LANE\n");
		if (i++ >= DPHY_TIMEOUT) {
			LCDERR("%s: lane_state timeout\n", __func__);
			break;
		}
		lcd_delay_us(6);
	}
}

static void dsi_phy_init(struct aml_lcd_drv_s *pdrv, struct dsi_phy_s *dphy)
{
	struct lcd_config_s *pconf = &pdrv->config;
	unsigned char lane_num;

	lane_num = pdrv->config.control.mipi_cfg.lane_num;

	/* enable phy clock. */
	dsi_phy_write(pdrv, MIPI_DSI_PHY_CTRL,  0x1); /* enable DSI top clock. */
	dsi_phy_write(pdrv, MIPI_DSI_PHY_CTRL,
		      (1 << 0)  | /* enable the DSI PLL clock . */
		      (1 << 7)  |
			/* enable pll clock which connected to
			 * DDR clock path
			 */
		      (1 << 8)  | /* enable the clock divider counter */
		      (0 << 9)  | /* enable the divider clock out */
		      (0 << 10) | /* clock divider. 1: freq/4, 0: freq/2 */
		      (0 << 11) |
			/* 1: select the mipi DDRCLKHS from clock divider,
			 * 0: from PLL clock
			 */
		      (0 << 12)); /* enable the byte clock generateion. */
	/* enable the divider clock out */
	dsi_phy_setb(pdrv, MIPI_DSI_PHY_CTRL,  1, 9, 1);
	/* enable the byte clock generateion. */
	dsi_phy_setb(pdrv, MIPI_DSI_PHY_CTRL,  1, 12, 1);
	dsi_phy_setb(pdrv, MIPI_DSI_PHY_CTRL,  1, 31, 1);
	dsi_phy_setb(pdrv, MIPI_DSI_PHY_CTRL,  0, 31, 1);

	/* 0x05210f08);//0x03211c08 */
	dsi_phy_write(pdrv, MIPI_DSI_CLK_TIM,
		      (dphy->clk_trail |
			  ((dphy->clk_post + dphy->hs_trail) << 8) |
			  (dphy->clk_zero << 16) |
		      (dphy->clk_prepare << 24)));
	dsi_phy_write(pdrv, MIPI_DSI_CLK_TIM1, dphy->clk_pre); /* ?? */
	/* 0x050f090d */
	if (pconf->timing.bit_rate > 500000000UL) { /*more than 500M*/
		dsi_phy_write(pdrv, MIPI_DSI_HS_TIM,
			      (dphy->hs_exit | (dphy->hs_trail << 8) |
			      (dphy->hs_zero << 16) |
			      (dphy->hs_prepare << 24)));
	} else {
		LCDPR("[%d]: %s: bit_rate = %lld\n",
			pdrv->index, __func__, pconf->timing.bit_rate);
		dsi_phy_write(pdrv, MIPI_DSI_HS_TIM,
			      (dphy->hs_exit | ((dphy->hs_trail / 2) << 8) |
			      (dphy->hs_zero << 16) |
			      (dphy->hs_prepare << 24)));
	}
	/* 0x4a370e0e */
	dsi_phy_write(pdrv, MIPI_DSI_LP_TIM,
		      (dphy->lp_lpx | (dphy->lp_ta_sure << 8) |
		      (dphy->lp_ta_go << 16) | (dphy->lp_ta_get << 24)));
	/* ?? //some number to reduce sim time. */
	dsi_phy_write(pdrv, MIPI_DSI_ANA_UP_TIM, 0x0100);
	/* 0xe20   //30d4 -> d4 to reduce sim time. */
	dsi_phy_write(pdrv, MIPI_DSI_INIT_TIM, dphy->init);
	/* 0x8d40  //1E848-> 48 to reduct sim time. */
	dsi_phy_write(pdrv, MIPI_DSI_WAKEUP_TIM, dphy->wakeup);
	/* wait for the LP analog ready. */
	dsi_phy_write(pdrv, MIPI_DSI_LPOK_TIM,  0x7C);
	/* 1/3 of the tWAKEUP. */
	dsi_phy_write(pdrv, MIPI_DSI_ULPS_CHECK,  0x927C);
	/* phy TURN watch dog. */
	dsi_phy_write(pdrv, MIPI_DSI_LP_WCHDOG,  0x1000);
	/* phy ESC command watch dog. */
	dsi_phy_write(pdrv, MIPI_DSI_TURN_WCHDOG,  0x1000);

	/* Powerup the analog circuit. */
	switch (lane_num) {
	case 1:
		dsi_phy_write(pdrv, MIPI_DSI_CHAN_CTRL, 0x0e);
		break;
	case 2:
		dsi_phy_write(pdrv, MIPI_DSI_CHAN_CTRL, 0x0c);
		break;
	case 3:
		dsi_phy_write(pdrv, MIPI_DSI_CHAN_CTRL, 0x08);
		break;
	case 4:
	default:
		dsi_phy_write(pdrv, MIPI_DSI_CHAN_CTRL, 0);
		break;
	}
}

static void set_dsi_phy_config(struct aml_lcd_drv_s *pdrv)
{
	struct dsi_config_s *dconf;

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("%s\n", __func__);

	dconf = &pdrv->config.control.mipi_cfg;

	/* Digital */
	/* Power up DSI */
	dsi_host_write(pdrv, MIPI_DSI_DWC_PWR_UP_OS, 1);

	/* Setup Parameters of DPHY */
	dsi_host_write(pdrv, MIPI_DSI_DWC_PHY_TST_CTRL1_OS, 0x00010044);/*testcode*/
	dsi_host_write(pdrv, MIPI_DSI_DWC_PHY_TST_CTRL0_OS, 0x2);
	dsi_host_write(pdrv, MIPI_DSI_DWC_PHY_TST_CTRL0_OS, 0x0);
	dsi_host_write(pdrv, MIPI_DSI_DWC_PHY_TST_CTRL1_OS, 0x00000074);/*testwrite*/
	dsi_host_write(pdrv, MIPI_DSI_DWC_PHY_TST_CTRL0_OS, 0x2);
	dsi_host_write(pdrv, MIPI_DSI_DWC_PHY_TST_CTRL0_OS, 0x0);

	/* Power up D-PHY */
	dsi_host_write(pdrv, MIPI_DSI_DWC_PHY_RSTZ_OS, 0xf);

	/* Analog */
	dsi_phy_init(pdrv, &dsi_phy_config);

	/* Check the phylock/stopstateclklane to decide if the DPHY is ready */
	check_phy_status(pdrv);

	/* Trigger a sync active for esc_clk */
	dsi_phy_set_mask(pdrv, MIPI_DSI_PHY_CTRL, (1 << 1));
}

static void startup_mipi_dsi_host(struct aml_lcd_drv_s *pdrv)
{
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("%s\n", __func__);

	/* Enable dwc mipi_dsi_host's clock */
	dsi_host_set_mask(pdrv, MIPI_DSI_TOP_CNTL, ((1 << 4) | (1 << 5) | (0 << 6)));
	/* mipi_dsi_host's reset */
	dsi_host_set_mask(pdrv, MIPI_DSI_TOP_SW_RESET, 0xf);
	/* Release mipi_dsi_host's reset */
	dsi_host_clr_mask(pdrv, MIPI_DSI_TOP_SW_RESET, 0xf);
	/* Enable dwc mipi_dsi_host's clock */
	dsi_host_set_mask(pdrv, MIPI_DSI_TOP_CLK_CNTL, 0x3);

	dsi_host_write(pdrv, MIPI_DSI_TOP_MEM_PD, 0);

	lcd_delay_ms(10);
}

static void mipi_dsi_lpclk_ctrl(struct aml_lcd_drv_s *pdrv)
{
	struct dsi_config_s *dconf;
	unsigned int lpclk;

	dconf = &pdrv->config.control.mipi_cfg;
	/* when lpclk = 1, enable clk lp state */
	lpclk = (dconf->clk_always_hs) ? 0 : 1;

	dsi_host_write(pdrv, MIPI_DSI_DWC_LPCLK_CTRL_OS,
		       (lpclk << BIT_AUTOCLKLANE_CTRL) |
		       (0x1 << BIT_TXREQUESTCLKHS));
}

/* *************************************************************
 * Function: set_mipi_dsi_host
 * Parameters: vcid, // virtual id
 *		chroma_subsample, // chroma_subsample for YUV422 or YUV420 only
 *		operation_mode,   // video mode/command mode
 *		p,                //lcd config
 */
static void set_mipi_dsi_host(struct aml_lcd_drv_s *pdrv,
			      unsigned int vcid,
			      unsigned int chroma_subsample,
			      unsigned int operation_mode)
{
	unsigned int dpi_data_format, venc_data_width;
	unsigned int lane_num, vid_mode_type;
	unsigned int temp;
	struct dsi_config_s *dconf;

	dconf = &pdrv->config.control.mipi_cfg;
	dconf->current_mode = operation_mode;
	venc_data_width = dconf->venc_data_width;
	dpi_data_format = dconf->dpi_data_format;
	lane_num        = (unsigned int)(dconf->lane_num);
	vid_mode_type   = (unsigned int)(dconf->video_mode_type);

	/* ----------------------------------------------------- */
	/* Standard Configuration for Video Mode Operation */
	/* ----------------------------------------------------- */
	/* 1,    Configure Lane number and phy stop wait time */
	if (dsi_phy_config.state_change == 2) {
		dsi_host_write(pdrv, MIPI_DSI_DWC_PHY_IF_CFG_OS,
			       (0x28 << BIT_PHY_STOP_WAIT_TIME) |
			       ((lane_num - 1) << BIT_N_LANES));
	} else {
		dsi_host_write(pdrv, MIPI_DSI_DWC_PHY_IF_CFG_OS,
			       (1 << BIT_PHY_STOP_WAIT_TIME) |
			       ((lane_num - 1) << BIT_N_LANES));
	}

	/* 2.1,  Configure Virtual channel settings */
	dsi_host_write(pdrv, MIPI_DSI_DWC_DPI_VCID_OS, vcid);
	/* 2.2,  Configure Color format */
	dsi_host_write(pdrv, MIPI_DSI_DWC_DPI_COLOR_CODING_OS,
		       (((dpi_data_format == COLOR_18BIT_CFG_2) ?
		       1 : 0) << BIT_LOOSELY18_EN) |
		       (dpi_data_format << BIT_DPI_COLOR_CODING));
	/* 2.2.1 Configure Set color format for DPI register */
	temp = (dsi_host_read(pdrv, MIPI_DSI_TOP_CNTL) &
		~(0xf << BIT_DPI_COLOR_MODE) &
		~(0x7 << BIT_IN_COLOR_MODE) &
		~(0x3 << BIT_CHROMA_SUBSAMPLE));
	dsi_host_write(pdrv, MIPI_DSI_TOP_CNTL,
		       (temp |
		       (dpi_data_format  << BIT_DPI_COLOR_MODE)  |
		       (venc_data_width  << BIT_IN_COLOR_MODE)   |
		       (chroma_subsample << BIT_CHROMA_SUBSAMPLE)));
	/* 2.3   Configure Signal polarity */
	dsi_host_write(pdrv, MIPI_DSI_DWC_DPI_CFG_POL_OS,
		       (0x0 << BIT_COLORM_ACTIVE_LOW) |
		       (0x0 << BIT_SHUTD_ACTIVE_LOW)  |
		       (0 << BIT_HSYNC_ACTIVE_LOW)    |
		       (0 << BIT_VSYNC_ACTIVE_LOW)    |
		       (0x0 << BIT_DATAEN_ACTIVE_LOW));

	if (operation_mode == OPERATION_VIDEO_MODE) {
		/* 3.1   Configure Low power and video mode type settings */
		dsi_host_write(pdrv, MIPI_DSI_DWC_VID_MODE_CFG_OS,
			       (1 << BIT_LP_HFP_EN)  |       /* enable lp */
			       (1 << BIT_LP_HBP_EN)  |       /* enable lp */
			       (1 << BIT_LP_VCAT_EN) |       /* enable lp */
			       (1 << BIT_LP_VFP_EN)  |       /* enable lp */
			       (1 << BIT_LP_VBP_EN)  |       /* enable lp */
			       (1 << BIT_LP_VSA_EN)  |       /* enable lp */
			       (0 << BIT_FRAME_BTA_ACK_EN) |
			   /* enable BTA after one frame, TODO, need check */
			/* (1 << BIT_LP_CMD_EN) |  */
			   /* enable the command transmission only in lp mode */
			(vid_mode_type << BIT_VID_MODE_TYPE));
					/* burst non burst mode */
		/* [23:16]outvact, [7:0]invact */
		dsi_host_write(pdrv, MIPI_DSI_DWC_DPI_LP_CMD_TIM_OS,
			       (4 << 16) | (4 << 0));
		/* 3.2   Configure video packet size settings */
		dsi_host_write(pdrv, MIPI_DSI_DWC_VID_PKT_SIZE_OS,
			       dsi_vconf.pixel_per_chunk);
		dsi_host_write(pdrv, MIPI_DSI_DWC_VID_NUM_CHUNKS_OS,
			       dsi_vconf.vid_num_chunks);
		dsi_host_write(pdrv, MIPI_DSI_DWC_VID_NULL_SIZE_OS,
			       dsi_vconf.vid_null_size);
		/* 4 Configure the video relative parameters according to
		 *	   the output type
		 */
		/* include horizontal timing and vertical line */
		dsi_host_write(pdrv, MIPI_DSI_DWC_VID_HLINE_TIME_OS, dsi_vconf.hline);
		dsi_host_write(pdrv, MIPI_DSI_DWC_VID_HSA_TIME_OS, dsi_vconf.hsa);
		dsi_host_write(pdrv, MIPI_DSI_DWC_VID_HBP_TIME_OS, dsi_vconf.hbp);
		dsi_host_write(pdrv, MIPI_DSI_DWC_VID_VSA_LINES_OS, dsi_vconf.vsa);
		dsi_host_write(pdrv, MIPI_DSI_DWC_VID_VBP_LINES_OS, dsi_vconf.vbp);
		dsi_host_write(pdrv, MIPI_DSI_DWC_VID_VFP_LINES_OS, dsi_vconf.vfp);
		dsi_host_write(pdrv, MIPI_DSI_DWC_VID_VACTIVE_LINES_OS,
			       dsi_vconf.vact);
	}  /* operation_mode == OPERATION_VIDEO_MODE */

	/* ----------------------------------------------------- */
	/* Finish Configuration */
	/* ----------------------------------------------------- */

	/* Inner clock divider settings */
	dsi_host_write(pdrv, MIPI_DSI_DWC_CLKMGR_CFG_OS,
		       (0x1 << BIT_TO_CLK_DIV) |
		       (dsi_phy_config.lp_tesc << BIT_TX_ESC_CLK_DIV));
	/* Packet header settings  //move to mipi_dcs_set */
	/* dsi_host_write(pdrv,  MIPI_DSI_DWC_PCKHDL_CFG_OS,
	 *	(1 << BIT_CRC_RX_EN) |
	 *	(1 << BIT_ECC_RX_EN) |
	 *	(0 << BIT_BTA_EN) |
	 *	(0 << BIT_EOTP_RX_EN) |
	 *	(0 << BIT_EOTP_TX_EN) );
	 */
	/* operation mode setting: video/command mode */
	dsi_host_write(pdrv, MIPI_DSI_DWC_MODE_CFG_OS, operation_mode);

	/* Phy Timer */
	if (dsi_phy_config.state_change == 2)
		dsi_host_write(pdrv, MIPI_DSI_DWC_PHY_TMR_CFG_OS, 0x03320000);
	else
		dsi_host_write(pdrv, MIPI_DSI_DWC_PHY_TMR_CFG_OS, 0x090f0000);

	if (dsi_phy_config.state_change == 2)
		dsi_host_write(pdrv, MIPI_DSI_DWC_PHY_TMR_LPCLK_CFG_OS, 0x870025);
	else
		dsi_host_write(pdrv, MIPI_DSI_DWC_PHY_TMR_LPCLK_CFG_OS, 0x260017);
}

int dsi_set_operation_mode(struct aml_lcd_drv_s *pdrv, unsigned char op_mode)
{
	unsigned char cur_mode;

	op_mode = (op_mode == 0) ? 0 : 1;
	cur_mode = pdrv->config.control.mipi_cfg.current_mode;
	if (cur_mode != op_mode) {
		set_mipi_dsi_host(pdrv,
				  MIPI_DSI_VIRTUAL_CHAN_ID,
				  0, /* Chroma sub sample, only for
				      * YUV 422 or 420, even or odd
				      */
				  /* DSI operation mode, video or command */
				  op_mode);
		LCDPR("set mipi-dsi operation mode: %s(%d)\n",
		      operation_mode_table[op_mode], op_mode);
	} else {
		LCDPR("same mipi-dsi operation mode: %s(%d), exit\n",
		      operation_mode_table[op_mode], op_mode);
	}

	return 0;
}

/* *************************************************************
 * mipi dsi command support
 */

static inline void print_mipi_cmd_status(struct aml_lcd_drv_s *pdrv,
					 int cnt, unsigned int status)
{
	if (cnt == 0) {
		LCDPR("[%d]: cmd error: status=0x%04x, int0=0x%06x, int1=0x%06x\n",
		      pdrv->index, status,
		      dsi_host_read(pdrv, MIPI_DSI_DWC_INT_ST0_OS),
		      dsi_host_read(pdrv, MIPI_DSI_DWC_INT_ST1_OS));
	}
}

#ifdef DSI_CMD_READ_VALID
static void dsi_bta_control(struct aml_lcd_drv_s *pdrv, int en)
{
	if (en) {
		dsi_host_setb(pdrv, MIPI_DSI_DWC_CMD_MODE_CFG_OS,
			      MIPI_DSI_DCS_REQ_ACK, BIT_ACK_RQST_EN, 1);
		dsi_host_setb(pdrv, MIPI_DSI_DWC_PCKHDL_CFG_OS,
			      MIPI_DSI_DCS_REQ_ACK, BIT_BTA_EN, 1);
	} else {
		dsi_host_setb(pdrv, MIPI_DSI_DWC_PCKHDL_CFG_OS,
			      MIPI_DSI_DCS_NO_ACK, BIT_BTA_EN, 1);
		dsi_host_setb(pdrv, MIPI_DSI_DWC_CMD_MODE_CFG_OS,
			      MIPI_DSI_DCS_NO_ACK, BIT_ACK_RQST_EN, 1);
	}
}

/* *************************************************************
 * Function: generic_if_rd
 * Generic interface read, address has to be MIPI_DSI_DWC_GEN_PLD_DATA_OS
 */
static unsigned int generic_if_rd(struct aml_lcd_drv_s *pdrv, unsigned int address)
{
	unsigned int data_out;

	if (address != MIPI_DSI_DWC_GEN_PLD_DATA_OS)
		LCDERR(" Error Address : %x\n", address);

	data_out = dsi_host_read(pdrv, address);
	return data_out;
}
#endif

/* *************************************************************
 * Function: generic_if_wr
 * Generic interface write, address has to be
 *			MIPI_DSI_DWC_GEN_PLD_DATA_OS,
 *			MIPI_DSI_DWC_GEN_HDR_OS,
 *			MIPI_DSI_DWC_GEN_VCID_OS
 */
static unsigned int generic_if_wr(struct aml_lcd_drv_s *pdrv,
				  unsigned int address, unsigned int data_in)
{
	if (address != MIPI_DSI_DWC_GEN_HDR_OS &&
	    address != MIPI_DSI_DWC_GEN_PLD_DATA_OS) {
		LCDERR(" Error Address : 0x%x\n", address);
	}

	if (lcd_debug_print_flag & LCD_DBG_PR_ADV)
		LCDPR("address 0x%x = 0x%08x\n", address, data_in);

	dsi_host_write(pdrv, address, data_in);

	return 0;
}

/* *************************************************************
 * Function: wait_bta_ack
 * Poll to check if the BTA ack is finished
 */
static int wait_bta_ack(struct aml_lcd_drv_s *pdrv)
{
	unsigned int phy_status;
	int i;

	/* Check if phydirection is RX */
	i = CMD_TIMEOUT_CNT;
	do {
		udelay(1);
		i--;
		phy_status = dsi_host_read(pdrv, MIPI_DSI_DWC_PHY_STATUS_OS);
	} while ((((phy_status & 0x2) >> BIT_PHY_DIRECTION) == 0x0) && (i > 0));
	if (i == 0) {
		LCDERR("phy direction error: RX\n");
		return -1;
	}

	/* Check if phydirection is return to TX */
	i = CMD_TIMEOUT_CNT;
	do {
		udelay(1);
		i--;
		phy_status = dsi_host_read(pdrv, MIPI_DSI_DWC_PHY_STATUS_OS);
	} while ((((phy_status & 0x2) >> BIT_PHY_DIRECTION) == 0x1) && (i > 0));
	if (i == 0) {
		LCDERR("phy direction error: TX\n");
		return -1;
	}

	return 0;
}

/* *************************************************************
 * Function: wait_cmd_fifo_empty
 * Poll to check if the generic command fifo is empty
 */
static int wait_cmd_fifo_empty(struct aml_lcd_drv_s *pdrv)
{
	unsigned int cmd_status;
	int i = CMD_TIMEOUT_CNT;

	do {
		lcd_delay_us(10);
		i--;
		cmd_status = dsi_host_getb(pdrv, MIPI_DSI_DWC_CMD_PKT_STATUS_OS,
					   BIT_GEN_CMD_EMPTY, 1);
	} while ((cmd_status != 0x1) && (i > 0));

	if (cmd_status == 0) {
		cmd_status = dsi_host_read(pdrv, MIPI_DSI_DWC_CMD_PKT_STATUS_OS);
		print_mipi_cmd_status(pdrv, i, cmd_status);
		return -1;
	}

	return 0;
}

static void dsi_set_max_return_pkt_size(struct aml_lcd_drv_s *pdrv,
					struct dsi_cmd_request_s *req)
{
	unsigned int d_para[2];

	d_para[0] = (unsigned int)(req->payload[2] & 0xff);
	d_para[1] = (unsigned int)(req->payload[3] & 0xff);
	dsi_rx_n = (unsigned short)((d_para[1] << 8) | d_para[0]);
	generic_if_wr(pdrv, MIPI_DSI_DWC_GEN_HDR_OS,
		      ((d_para[1] << BIT_GEN_WC_MSBYTE)          |
		      (d_para[0] << BIT_GEN_WC_LSBYTE)           |
		      (((unsigned int)req->vc_id) << BIT_GEN_VC) |
		      (DT_SET_MAX_RET_PKT_SIZE << BIT_GEN_DT)));
	if (req->req_ack == MIPI_DSI_DCS_REQ_ACK)
		wait_bta_ack(pdrv);
	else if (req->req_ack == MIPI_DSI_DCS_NO_ACK)
		wait_cmd_fifo_empty(pdrv);
}

#ifdef DSI_CMD_READ_VALID
static int dsi_generic_read_packet(struct aml_lcd_drv_s *pdrv,
				   struct dsi_cmd_request_s *req,
				   unsigned char *r_data)
{
	unsigned int d_para[2], read_data;
	unsigned int i, j, done;
	int ret = 0;

	if (pdrv->data->chip_type == LCD_CHIP_T7)
		dsi_phy_setb(pdrv, MIPI_DSI_CHAN_CTRL, 0x3, 20, 2);
	switch (req->data_type) {
	case DT_GEN_RD_1:
		d_para[0] = (req->pld_count == 0) ?
			0 : (((unsigned int)req->payload[2]) & 0xff);
		d_para[1] = 0;
		break;
	case DT_GEN_RD_2:
		d_para[0] = (req->pld_count == 0) ?
			0 : (((unsigned int)req->payload[2]) & 0xff);
		d_para[1] = (req->pld_count < 2) ?
			0 : (((unsigned int)req->payload[3]) & 0xff);
		break;
	case DT_GEN_RD_0:
	default:
		d_para[0] = 0;
		d_para[1] = 0;
		break;
	}

	if (MIPI_DSI_DCS_ACK_TYPE == MIPI_DSI_DCS_NO_ACK)
		dsi_bta_control(pdrv, 1);
	generic_if_wr(pdrv, MIPI_DSI_DWC_GEN_HDR_OS,
		      ((d_para[1] << BIT_GEN_WC_MSBYTE)          |
		      (d_para[0] << BIT_GEN_WC_LSBYTE)           |
		      (((unsigned int)req->vc_id) << BIT_GEN_VC) |
		      (((unsigned int)req->data_type) << BIT_GEN_DT)));
	ret = wait_bta_ack(pdrv);
	if (ret)
		goto dsi_generic_read_packet_done;

	i = 0;
	done = 0;
	while (done == 0) {
		read_data = generic_if_rd(pdrv, MIPI_DSI_DWC_GEN_PLD_DATA_OS);
		for (j = 0; j < 4; j++) {
			if (i < dsi_rx_n) {
				r_data[i] = (unsigned char)
					((read_data >> (j * 8)) & 0xff);
				i++;
			} else {
				done = 1;
				break;
			}
		}
	}

dsi_generic_read_packet_done:
	if (MIPI_DSI_DCS_ACK_TYPE == MIPI_DSI_DCS_NO_ACK)
		dsi_bta_control(pdrv, 0);
	if (pdrv->data->chip_type == LCD_CHIP_T7)
		dsi_phy_setb(pdrv, MIPI_DSI_CHAN_CTRL, 0x0, 20, 2);
	if (ret)
		return -1;
	return dsi_rx_n;
}

static int dsi_dcs_read_packet(struct aml_lcd_drv_s *pdrv,
			       struct dsi_cmd_request_s *req,
			       unsigned char *r_data)
{
	unsigned int d_command, read_data;
	unsigned int i, j, done;
	int ret = 0;

	if (pdrv->data->chip_type == LCD_CHIP_T7)
		dsi_phy_setb(pdrv, MIPI_DSI_CHAN_CTRL, 0x3, 20, 2);

	d_command = ((unsigned int)req->payload[2]) & 0xff;

	if (MIPI_DSI_DCS_ACK_TYPE == MIPI_DSI_DCS_NO_ACK)
		dsi_bta_control(pdrv, 1);
	generic_if_wr(pdrv, MIPI_DSI_DWC_GEN_HDR_OS,
		      ((0 << BIT_GEN_WC_MSBYTE)                  |
		      (d_command << BIT_GEN_WC_LSBYTE)           |
		      (((unsigned int)req->vc_id) << BIT_GEN_VC) |
		      (((unsigned int)req->data_type) << BIT_GEN_DT)));
	ret = wait_bta_ack(pdrv);
	if (ret)
		goto dsi_dcs_read_packet_done;

	i = 0;
	done = 0;
	while (done == 0) {
		read_data = generic_if_rd(pdrv, MIPI_DSI_DWC_GEN_PLD_DATA_OS);
		for (j = 0; j < 4; j++) {
			if (i < dsi_rx_n) {
				r_data[i] = (unsigned char)
					((read_data >> (j * 8)) & 0xff);
				i++;
			} else {
				done = 1;
				break;
			}
		}
	}

dsi_dcs_read_packet_done:
	if (MIPI_DSI_DCS_ACK_TYPE == MIPI_DSI_DCS_NO_ACK)
		dsi_bta_control(pdrv, 0);
	if (pdrv->data->chip_type == LCD_CHIP_T7)
		dsi_phy_setb(pdrv, MIPI_DSI_CHAN_CTRL, 0x0, 20, 2);
	if (ret)
		return -1;
	return dsi_rx_n;
}
#endif

/* *************************************************************
 * Function: generic_write_short_packet
 * Generic Write Short Packet with Generic Interface
 * Supported Data Type: DT_GEN_SHORT_WR_0,
			DT_GEN_SHORT_WR_1,
			DT_GEN_SHORT_WR_2,
 */
static int dsi_generic_write_short_packet(struct aml_lcd_drv_s *pdrv,
					  struct dsi_cmd_request_s *req)
{
	unsigned int d_para[2];
	int ret = 0;

	switch (req->data_type) {
	case DT_GEN_SHORT_WR_1:
		d_para[0] = (req->pld_count == 0) ?
			0 : (((unsigned int)req->payload[2]) & 0xff);
		d_para[1] = 0;
		break;
	case DT_GEN_SHORT_WR_2:
		d_para[0] = (req->pld_count == 0) ?
			0 : (((unsigned int)req->payload[2]) & 0xff);
		d_para[1] = (req->pld_count < 2) ?
			0 : (((unsigned int)req->payload[3]) & 0xff);
		break;
	case DT_GEN_SHORT_WR_0:
	default:
		d_para[0] = 0;
		d_para[1] = 0;
		break;
	}

	generic_if_wr(pdrv, MIPI_DSI_DWC_GEN_HDR_OS,
		      ((d_para[1] << BIT_GEN_WC_MSBYTE)          |
		      (d_para[0] << BIT_GEN_WC_LSBYTE)           |
		      (((unsigned int)req->vc_id) << BIT_GEN_VC) |
		      (((unsigned int)req->data_type) << BIT_GEN_DT)));
	if (req->req_ack == MIPI_DSI_DCS_REQ_ACK)
		ret = wait_bta_ack(pdrv);
	else if (req->req_ack == MIPI_DSI_DCS_NO_ACK)
		ret = wait_cmd_fifo_empty(pdrv);

	return ret;
}

/* *************************************************************
 * Function: dcs_write_short_packet
 * DCS Write Short Packet with Generic Interface
 * Supported Data Type: DT_DCS_SHORT_WR_0, DT_DCS_SHORT_WR_1,
 */
static int dsi_dcs_write_short_packet(struct aml_lcd_drv_s *pdrv,
				      struct dsi_cmd_request_s *req)
{
	unsigned int d_command, d_para;
	int ret = 0;

	d_command = ((unsigned int)req->payload[2]) & 0xff;
	d_para = (req->pld_count < 2) ?
		0 : (((unsigned int)req->payload[3]) & 0xff);

	generic_if_wr(pdrv, MIPI_DSI_DWC_GEN_HDR_OS,
		      ((d_para << BIT_GEN_WC_MSBYTE)             |
		      (d_command << BIT_GEN_WC_LSBYTE)           |
		      (((unsigned int)req->vc_id) << BIT_GEN_VC) |
		      (((unsigned int)req->data_type) << BIT_GEN_DT)));
	if (req->req_ack == MIPI_DSI_DCS_REQ_ACK)
		ret = wait_bta_ack(pdrv);
	else if (req->req_ack == MIPI_DSI_DCS_NO_ACK)
		ret = wait_cmd_fifo_empty(pdrv);

	return ret;
}

/* *************************************************************
 * Function: dsi_write_long_packet
 * Write Long Packet with Generic Interface
 * Supported Data Type: DT_GEN_LONG_WR, DT_DCS_LONG_WR
 */
static int dsi_write_long_packet(struct aml_lcd_drv_s *pdrv,
				 struct dsi_cmd_request_s *req)
{
	unsigned int d_command, payload_data, header_data;
	unsigned int cmd_status;
	unsigned int i, j, data_index, n, temp;
	int ret = 0;

	/* payload[2] start (payload[0]: data_type, payload[1]: data_cnt) */
	data_index = DSI_CMD_SIZE_INDEX + 1;
	d_command = ((unsigned int)req->payload[data_index]) & 0xff;

	/* Write Payload Register First */
	n = (req->pld_count + 3) / 4;
	for (i = 0; i < n; i++) {
		payload_data = 0;
		if (i < (req->pld_count / 4))
			temp = 4;
		else
			temp = req->pld_count % 4;
		for (j = 0; j < temp; j++) {
			payload_data |= (((unsigned int)
				req->payload[data_index + (i * 4) + j]) <<
				(j * 8));
		}

		/* Check the pld fifo status before write to it,
		 * do not need check every word
		 */
		if ((i == (n / 3)) || (i == (n / 2))) {
			j = CMD_TIMEOUT_CNT;
			do {
				lcd_delay_us(10);
				j--;
				cmd_status = dsi_host_read(pdrv,
					MIPI_DSI_DWC_CMD_PKT_STATUS_OS);
			} while ((((cmd_status >> BIT_GEN_PLD_W_FULL) & 0x1) ==
				0x1) && (j > 0));
			print_mipi_cmd_status(pdrv, j, cmd_status);
		}
		/* Use direct memory write to save time when in
		 * WRITE_MEMORY_CONTINUE
		 */
		if (d_command == DCS_WRITE_MEMORY_CONTINUE) {
			dsi_host_write(pdrv, MIPI_DSI_DWC_GEN_PLD_DATA_OS,
				       payload_data);
		} else {
			generic_if_wr(pdrv, MIPI_DSI_DWC_GEN_PLD_DATA_OS,
				      payload_data);
		}
	}

	/* Check cmd fifo status before write to it */
	j = CMD_TIMEOUT_CNT;
	do {
		lcd_delay_us(10);
		j--;
		cmd_status = dsi_host_read(pdrv, MIPI_DSI_DWC_CMD_PKT_STATUS_OS);
	} while ((((cmd_status >> BIT_GEN_CMD_FULL) & 0x1) == 0x1) && (j > 0));
	print_mipi_cmd_status(pdrv, j, cmd_status);
	/* Write Header Register */
	/* include command */
	header_data = ((((unsigned int)req->pld_count) << BIT_GEN_WC_LSBYTE) |
			(((unsigned int)req->vc_id) << BIT_GEN_VC)           |
			(((unsigned int)req->data_type) << BIT_GEN_DT));
	generic_if_wr(pdrv, MIPI_DSI_DWC_GEN_HDR_OS, header_data);
	if (req->req_ack == MIPI_DSI_DCS_REQ_ACK)
		ret = wait_bta_ack(pdrv);
	else if (req->req_ack == MIPI_DSI_DCS_NO_ACK)
		ret = wait_cmd_fifo_empty(pdrv);

	return ret;
}

#ifdef DSI_CMD_READ_VALID
/* *************************************************************
 * Function: dsi_read_single
 * Supported Data Type: DT_GEN_RD_0, DT_GEN_RD_1, DT_GEN_RD_2,
 *		DT_DCS_RD_0
 * Return:              data count, -1 for error
 */
int dsi_read_single(struct aml_lcd_drv_s *pdrv, unsigned char *payload,
		    unsigned char *rd_data, unsigned int rd_byte_len)
{
	int num = 0;
	unsigned char temp[4];
	unsigned char vc_id = MIPI_DSI_VIRTUAL_CHAN_ID;
	unsigned int req_ack;
	struct dsi_cmd_request_s dsi_cmd_req;

	req_ack = MIPI_DSI_DCS_ACK_TYPE;
	dsi_cmd_req.data_type = DT_SET_MAX_RET_PKT_SIZE;
	dsi_cmd_req.vc_id = (vc_id & 0x3);
	temp[0] = dsi_cmd_req.data_type;
	temp[1] = 2;
	temp[2] = (unsigned char)((rd_byte_len >> 0) & 0xff);
	temp[3] = (unsigned char)((rd_byte_len >> 8) & 0xff);
	dsi_cmd_req.payload = &temp[0];
	dsi_cmd_req.pld_count = 2;
	dsi_cmd_req.req_ack = req_ack;
	dsi_set_max_return_pkt_size(pdrv, &dsi_cmd_req);

	/* payload struct: */
	/* data_type, data_cnt, command, parameters... */
	req_ack = MIPI_DSI_DCS_REQ_ACK; /* need BTA ack */
	dsi_cmd_req.data_type = payload[0];
	dsi_cmd_req.vc_id = (vc_id & 0x3);
	dsi_cmd_req.payload = &payload[0];
	dsi_cmd_req.pld_count = payload[DSI_CMD_SIZE_INDEX];
	dsi_cmd_req.req_ack = req_ack;
	switch (dsi_cmd_req.data_type) {/* analysis data_type */
	case DT_GEN_RD_0:
	case DT_GEN_RD_1:
	case DT_GEN_RD_2:
		num = dsi_generic_read_packet(pdrv, &dsi_cmd_req, rd_data);
		break;
	case DT_DCS_RD_0:
		num = dsi_dcs_read_packet(pdrv, &dsi_cmd_req, rd_data);
		break;
	default:
		LCDPR("read un-support data_type: 0x%02x\n",
		      dsi_cmd_req.data_type);
		break;
	}

	if (num < 0)
		LCDERR("mipi-dsi read error\n");

	return num;
}
#else
int dsi_read_single(struct aml_lcd_drv_s *pdrv, unsigned char *payload,
		    unsigned char *rd_data, unsigned int rd_byte_len)
{
	LCDPR("Don't support mipi-dsi read command\n");
	return -1;
}
#endif

static int mipi_dsi_check_state(struct aml_lcd_drv_s *pdrv, unsigned char reg, int cnt)
{
	int ret = 0, i, len;
	unsigned char *rd_data;
	unsigned char payload[3] = {DT_GEN_RD_1, 1, 0x04};
	char str[100];
	struct dsi_config_s *dconf;
	unsigned int offset;

	dconf = &pdrv->config.control.mipi_cfg;
	if (dconf->check_en == 0)
		return 0;
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("%s\n", __func__);

	rd_data = kmalloc_array(cnt, sizeof(unsigned char), GFP_KERNEL);
	if (!rd_data) {
		LCDERR("%s: rd_data error\n", __func__);
		return 0;
	}

	offset = pdrv->data->offset_venc_if[pdrv->index];

	payload[2] = reg;
	ret = dsi_read_single(pdrv, payload, rd_data, cnt);
	if (ret < 0)
		goto mipi_dsi_check_state_err;
	if (ret > cnt) {
		LCDERR("%s: read back cnt is wrong\n", __func__);
		goto mipi_dsi_check_state_err;
	}

	len = sprintf(str, "read reg 0x%02x: ", reg);
	for (i = 0; i < ret; i++) {
		if (i == 0)
			len += sprintf(str + len, "0x%02x", rd_data[i]);
		else
			len += sprintf(str + len, ",0x%02x", rd_data[i]);
	}
	pr_info("%s\n", str);

	dconf->check_state = 1;
	lcd_vcbus_setb(L_VCOM_VS_ADDR + offset, 1, 12, 1);
	pdrv->config.retry_enable_flag = 0;
	LCDPR("%s: %d\n", __func__, dconf->check_state);
	kfree(rd_data);
	return 0;

mipi_dsi_check_state_err:
	if (pdrv->config.retry_enable_cnt >= LCD_ENABLE_RETRY_MAX) {
		dconf->check_state = 0;
		lcd_vcbus_setb(L_VCOM_VS_ADDR + offset, 0, 12, 1);
		LCDPR("%s: %d\n", __func__, dconf->check_state);
	}
	pdrv->config.retry_enable_flag = 1;
	kfree(rd_data);
	return -1;
}

/* *************************************************************
 * Function: dsi_write_cmd
 * Supported Data Type: DT_GEN_SHORT_WR_0, DT_GEN_SHORT_WR_1, DT_GEN_SHORT_WR_2,
 *			DT_DCS_SHORT_WR_0, DT_DCS_SHORT_WR_1,
 *			DT_GEN_LONG_WR, DT_DCS_LONG_WR,
 *			DT_SET_MAX_RET_PKT_SIZE
 *			DT_GEN_RD_0, DT_GEN_RD_1, DT_GEN_RD_2,
 *			DT_DCS_RD_0
 * Return:              command number
 */
int dsi_write_cmd(struct aml_lcd_drv_s *pdrv, unsigned char *payload)
{
	int i = 0, j = 0, step = 0;
	unsigned char cmd_size;
#ifdef DSI_CMD_READ_VALID
	int k = 0, n = 0;
	unsigned char rd_data[100];
	unsigned char str[200];
	int len;

#endif
	struct dsi_cmd_request_s dsi_cmd_req;
	unsigned char vc_id = MIPI_DSI_VIRTUAL_CHAN_ID;
	unsigned int req_ack = MIPI_DSI_DCS_ACK_TYPE;
	int delay_ms, ret = 0;

	if (!pdrv) {
		LCDERR("%s: pdrv is null\n", __func__);
		return 0;
	}

	/* mipi command(payload) */
	/* format:  data_type, cmd_size, data.... */
	/*	data_type=0xff,
	 *		cmd_size<0xff means delay ms,
	 *		cmd_size=0xff or 0 means ending.
	 *	data_type=0xf0, for gpio control
	 *		data0=gpio_index, data1=gpio_value.
	 *		data0=gpio_index, data1=gpio_value, data2=delay.
	 *	data_type=0xfd, for delay ms
	 *		data0=delay, data_1=delay, ..., data_n=delay.
	 */
	while ((i + DSI_CMD_SIZE_INDEX) < DSI_CMD_SIZE_MAX) {
		if (ret) {
			LCDERR("[%d]: %s: error, exit\n", pdrv->index,  __func__);
			break;
		}
		cmd_size = payload[i + DSI_CMD_SIZE_INDEX];
		if (payload[i] == LCD_EXT_CMD_TYPE_END) {
			if (cmd_size == 0xff || cmd_size == 0)
				break;
			cmd_size = 0;
			lcd_delay_ms(payload[i + 1]);
		}

		if (cmd_size == 0) {
			i += (cmd_size + 2);
			continue;
		}
		if (i + 2 + cmd_size > DSI_CMD_SIZE_MAX) {
			LCDERR("[%d]: step %d: cmd_size out of support\n",
			       pdrv->index, step);
			break;
		}

		if (payload[i] == LCD_EXT_CMD_TYPE_DELAY) {
			delay_ms = 0;
			for (j = 0; j < cmd_size; j++)
				delay_ms += payload[i + 2 + j];
			if (delay_ms > 0)
				lcd_delay_ms(delay_ms);
		} else if (payload[i] == LCD_EXT_CMD_TYPE_GPIO) {
			if (cmd_size < 2) {
				LCDERR("[%d]: step %d: invalid cmd_size %d for gpio\n",
				       pdrv->index, step, cmd_size);
				break;
			}
			lcd_cpu_gpio_set(pdrv, payload[i + 2], payload[i + 3]);
			if (cmd_size > 2) {
				if (payload[i + 4])
					lcd_delay_ms(payload[i + 4]);
			}
		} else if (payload[i] == LCD_EXT_CMD_TYPE_CHECK) {
			if (cmd_size < 2) {
				LCDERR("[%d]: step %d: invalid cmd_size %d for check state\n",
				       pdrv->index, step, cmd_size);
				break;
			}
			if (payload[i + 3] > 0) {
				ret = mipi_dsi_check_state(pdrv,
							   payload[i + 2],
							   payload[i + 3]);
			}
		} else {
			/* payload[i+DSI_CMD_SIZE_INDEX] is data count */
			dsi_cmd_req.data_type = payload[i];
			dsi_cmd_req.vc_id = (vc_id & 0x3);
			dsi_cmd_req.payload = &payload[i];
			dsi_cmd_req.pld_count = payload[i + DSI_CMD_SIZE_INDEX];
			dsi_cmd_req.req_ack = req_ack;
			switch (dsi_cmd_req.data_type) {/* analysis data_type */
			case DT_GEN_SHORT_WR_0:
			case DT_GEN_SHORT_WR_1:
			case DT_GEN_SHORT_WR_2:
				ret = dsi_generic_write_short_packet(pdrv,
								     &dsi_cmd_req);
				break;
			case DT_DCS_SHORT_WR_0:
			case DT_DCS_SHORT_WR_1:
				ret = dsi_dcs_write_short_packet(pdrv, &dsi_cmd_req);
				break;
			case DT_DCS_LONG_WR:
			case DT_GEN_LONG_WR:
				ret = dsi_write_long_packet(pdrv, &dsi_cmd_req);
				break;
			case DT_TURN_ON:
				dsi_host_setb(pdrv, MIPI_DSI_TOP_CNTL, 1, 2, 1);
				lcd_delay_ms(20); /* wait for vsync trigger */
				dsi_host_setb(pdrv, MIPI_DSI_TOP_CNTL, 0, 2, 1);
				lcd_delay_ms(20); /* wait for vsync trigger */
				break;
			case DT_SHUT_DOWN:
				dsi_host_setb(pdrv, MIPI_DSI_TOP_CNTL, 1, 2, 1);
				lcd_delay_ms(20); /* wait for vsync trigger */
				break;
			case DT_SET_MAX_RET_PKT_SIZE:
				dsi_set_max_return_pkt_size(pdrv, &dsi_cmd_req);
				break;
#ifdef DSI_CMD_READ_VALID
			case DT_GEN_RD_0:
			case DT_GEN_RD_1:
			case DT_GEN_RD_2:
				/* need BTA ack */
				dsi_cmd_req.req_ack = MIPI_DSI_DCS_REQ_ACK;
				dsi_cmd_req.pld_count =
					(dsi_cmd_req.pld_count > 2) ?
					2 : dsi_cmd_req.pld_count;

				n = dsi_generic_read_packet(pdrv, &dsi_cmd_req,
							    &rd_data[0]);
				len = 0;
				for (k = 0; k < dsi_cmd_req.pld_count; k++) {
					len += sprintf(str + len, " 0x%02x",
						       dsi_cmd_req.payload[k + 2]);
				}
				pr_info("[%d]: generic read data%s:\n",
					pdrv->index, str);
				len = 0;
				for (k = 0; k < n; k++) {
					len += sprintf(str + len, "0x%02x ",
						       rd_data[k]);
				}
				pr_info("  %s\n", str);
				break;
			case DT_DCS_RD_0:
				/* need BTA ack */
				dsi_cmd_req.req_ack = MIPI_DSI_DCS_REQ_ACK;
				n = dsi_dcs_read_packet(pdrv, &dsi_cmd_req,
							&rd_data[0]);
				pr_info("[%d]: dcs read data 0x%02x:\n",
					pdrv->index, dsi_cmd_req.payload[2]);
				len = 0;
				for (k = 0; k < n; k++) {
					len += sprintf(str + len, "0x%02x ",
						       rd_data[k]);
				}
				pr_info("  %s\n", str);
				break;
#endif
			default:
				LCDPR("[%d]: step %d: un-support data_type: 0x%02x\n",
				      pdrv->index, step, dsi_cmd_req.data_type);
				break;
			}
		}
		i += (cmd_size + 2);
		step++;
	}

	return step;
}

static void mipi_dsi_phy_config(struct dsi_phy_s *dphy, unsigned long long dsi_ui)
{
	unsigned int temp, t_ui, t_req_min, t_req_max;
	unsigned int val;

	if (dsi_ui == 0) {
		LCDERR("%s: dsi_ui is 0\n", __func__);
		return;
	}

	t_ui = lcd_do_div(dsi_ui, 1000);
	t_ui = (1000000 * 100) / t_ui; /*100*ns */
	temp = t_ui * 8; /* lane_byte cycle time */

	dphy->lp_tesc = ((DPHY_TIME_LP_TESC(t_ui) + temp - 1) / temp) & 0xff;
	dphy->lp_lpx = ((DPHY_TIME_LP_LPX(t_ui) + temp - 1) / temp) & 0xff;
	dphy->lp_ta_sure =
		((DPHY_TIME_LP_TA_SURE(t_ui) + temp - 1) / temp) & 0xff;
	dphy->lp_ta_go = ((DPHY_TIME_LP_TA_GO(t_ui) + temp - 1) / temp) & 0xff;
	dphy->lp_ta_get =
		((DPHY_TIME_LP_TA_GETX(t_ui) + temp - 1) / temp) & 0xff;
	dphy->init = (DPHY_TIME_INIT(t_ui) + temp - 1) / temp;
	dphy->wakeup = (DPHY_TIME_WAKEUP(t_ui) + temp - 1) / temp;

	dphy->clk_pre = ((DPHY_TIME_CLK_PRE(t_ui) + temp - 1) / temp) & 0xff;

	t_req_max = 95 * 100;
	t_req_min = 38 * 100;
	val = (t_req_max + t_req_min) / 2;
	dphy->clk_prepare = val / temp;
	if ((dphy->clk_prepare * temp) < t_req_min)
		dphy->clk_prepare += 1;

	t_req_min = 300 * 100 - dphy->clk_prepare * temp + 10 * 100;
	dphy->clk_zero = t_req_min / temp;
	if ((dphy->clk_zero * temp) < t_req_min)
		dphy->clk_zero += 1;

	//t_req_max = 105 + (12 * t_ui) / 100;
	t_req_min = 60 * 100;
	//val = (t_req_max + t_req_min) / 2;
	dphy->clk_trail = t_req_min / temp;
	if ((dphy->clk_trail * temp) < t_req_min)
		dphy->clk_trail += 1;

	t_req_min = 60 * 100 + 52 * t_ui + 30 * 100;
	dphy->clk_post = t_req_min / temp;
	if ((dphy->clk_post * temp) < t_req_min)
		dphy->clk_post += 1;

	t_req_min = 100 * 100;
	dphy->hs_exit = t_req_min / temp;
	if ((dphy->hs_exit * temp) < t_req_min)
		dphy->hs_exit += 1;

	//t_req_max = 105 + (12 * t_ui) / 100;
	t_req_min = ((8 * t_ui) > (60 * 100 + 4 * t_ui)) ?
		(8 * t_ui) : (60 * 100 + 4 * t_ui);
	//val = (t_req_max + t_req_min) / 2;
	dphy->hs_trail = t_req_min / temp;
	if ((dphy->hs_trail * temp) < t_req_min)
		dphy->hs_trail += 1;

	t_req_min = 40 * 100 + 4 * t_ui;
	t_req_max = 85 * 100 + 6 * t_ui;
	val = (t_req_max + t_req_min) / 2;
	dphy->hs_prepare = val / temp;
	if ((dphy->hs_prepare * temp) < t_req_min)
		dphy->hs_prepare += 1;

	t_req_min = 145 * 100 + 10 * t_ui - dphy->hs_prepare * temp;
	dphy->hs_zero = t_req_min / temp;
	if ((dphy->hs_zero * temp) < t_req_min)
		dphy->hs_zero += 1;

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
		LCDPR("%s:\n"
			"lp_tesc     = 0x%02x\n"
			"lp_lpx      = 0x%02x\n"
			"lp_ta_sure  = 0x%02x\n"
			"lp_ta_go    = 0x%02x\n"
			"lp_ta_get   = 0x%02x\n"
			"hs_exit     = 0x%02x\n"
			"hs_trail    = 0x%02x\n"
			"hs_zero     = 0x%02x\n"
			"hs_prepare  = 0x%02x\n"
			"clk_trail   = 0x%02x\n"
			"clk_post    = 0x%02x\n"
			"clk_zero    = 0x%02x\n"
			"clk_prepare = 0x%02x\n"
			"clk_pre     = 0x%02x\n"
			"init        = 0x%02x\n"
			"wakeup      = 0x%02x\n\n",
			__func__,
			dphy->lp_tesc, dphy->lp_lpx, dphy->lp_ta_sure,
			dphy->lp_ta_go, dphy->lp_ta_get, dphy->hs_exit,
			dphy->hs_trail, dphy->hs_zero, dphy->hs_prepare,
			dphy->clk_trail, dphy->clk_post,
			dphy->clk_zero, dphy->clk_prepare, dphy->clk_pre,
			dphy->init, dphy->wakeup);
	}
}

static void mipi_dsi_video_config(struct lcd_config_s *pconf)
{
	unsigned short h_period, hs_width, hs_bp;
	unsigned int den, num;
	unsigned short v_period, v_active, vs_width, vs_bp;

	h_period = pconf->basic.h_period;
	hs_width = pconf->timing.hsync_width;
	hs_bp = pconf->timing.hsync_bp;
	den = pconf->control.mipi_cfg.factor_denominator;
	num = pconf->control.mipi_cfg.factor_numerator;

	dsi_vconf.hline = (h_period * den + num - 1) / num;
	dsi_vconf.hsa = (hs_width * den + num - 1) / num;
	dsi_vconf.hbp = (hs_bp * den + num - 1) / num;

	v_period = pconf->basic.v_period;
	v_active = pconf->basic.v_active;
	vs_width = pconf->timing.vsync_width;
	vs_bp = pconf->timing.vsync_bp;
	dsi_vconf.vsa = vs_width;
	dsi_vconf.vbp = vs_bp;
	dsi_vconf.vfp = v_period - v_active - vs_bp - vs_width;
	dsi_vconf.vact = v_active;

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
		LCDPR("MIPI DSI video timing:\n"
			"  HLINE     = %d\n"
			"  HSA       = %d\n"
			"  HBP       = %d\n"
			"  VSA       = %d\n"
			"  VBP       = %d\n"
			"  VFP       = %d\n"
			"  VACT      = %d\n\n",
			dsi_vconf.hline, dsi_vconf.hsa, dsi_vconf.hbp,
			dsi_vconf.vsa, dsi_vconf.vbp, dsi_vconf.vfp,
			dsi_vconf.vact);
	}
}

#define DSI_PACKET_HEADER_CRC      6 /* 4(header)+2(CRC) */
static void mipi_dsi_non_burst_packet_config(struct lcd_config_s *pconf)
{
	struct dsi_config_s *dconf = &pconf->control.mipi_cfg;
	unsigned int lane_num, clk_factor, hactive, multi_pkt_en;
	unsigned long long bit_rate_required;
	unsigned int pixel_per_chunk = 0, vid_num_chunks = 0;
	unsigned int byte_per_chunk = 0, vid_pkt_byte_per_chunk = 0;
	unsigned int total_bytes_per_chunk = 0;
	unsigned int chunk_overhead = 0, vid_null_size = 0;
	int i = 1, done = 0;

	lane_num = (int)(dconf->lane_num);
	clk_factor = dconf->clk_factor;
	hactive = pconf->basic.h_active;
	bit_rate_required = pconf->timing.lcd_clk;
	bit_rate_required = bit_rate_required * 3 * dsi_vconf.data_bits;
	bit_rate_required = lcd_do_div(bit_rate_required, lane_num);
	if (pconf->timing.bit_rate > bit_rate_required)
		multi_pkt_en = 1;
	else
		multi_pkt_en = 0;
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
		LCDPR("non-burst: bit_rate_required=%lld, bit_rate=%lld\n",
		      bit_rate_required, pconf->timing.bit_rate);
	}

	if (multi_pkt_en == 0) {
		pixel_per_chunk = hactive;
		if (dconf->dpi_data_format == COLOR_18BIT_CFG_1) {
			/* 18bit (4*18/8=9byte) */
			byte_per_chunk = pixel_per_chunk * 9 / 4;
		} else {
			/* 24bit or 18bit-loosely */
			byte_per_chunk = pixel_per_chunk * 3;
		}
		vid_pkt_byte_per_chunk = 4 + byte_per_chunk + 2;
		total_bytes_per_chunk =
			(lane_num * pixel_per_chunk * clk_factor) / 8;

		vid_num_chunks = 0;
		vid_null_size = 0;
	} else {
		i = 2;
		while ((i < hactive) && (done == 0)) {
			vid_num_chunks = i;
			pixel_per_chunk = hactive / vid_num_chunks;

			if (dconf->dpi_data_format == COLOR_18BIT_CFG_1) {
				if ((pixel_per_chunk % 4) > 0)
					continue;
				/* 18bit (4*18/8=9byte) */
				byte_per_chunk = pixel_per_chunk * 9 / 4;
			} else {
				/* 24bit or 18bit-loosely */
				byte_per_chunk = pixel_per_chunk * 3;
			}
			vid_pkt_byte_per_chunk = 4 + byte_per_chunk + 2;
			total_bytes_per_chunk =
				(lane_num * pixel_per_chunk * clk_factor) / 8;

			chunk_overhead =
				total_bytes_per_chunk - vid_pkt_byte_per_chunk;
			if (chunk_overhead >= 12) {
				vid_null_size = chunk_overhead - 12;
				done = 1;
			} else if (chunk_overhead >= 6) {
				vid_null_size = 0;
				done = 1;
			}
			i += 2;
		}
		if (done == 0) {
			LCDERR("Packet no room for chunk_overhead\n");
			//pixel_per_chunk = hactive;
			//vid_num_chunks = 0;
			//vid_null_size = 0;
		}
	}

	dsi_vconf.pixel_per_chunk = pixel_per_chunk;
	dsi_vconf.vid_num_chunks = vid_num_chunks;
	dsi_vconf.vid_null_size = vid_null_size;
	dsi_vconf.byte_per_chunk = byte_per_chunk;
	dsi_vconf.multi_pkt_en = multi_pkt_en;

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
		LCDPR("MIPI DSI NON-BURST setting:\n"
			"  multi_pkt_en             = %d\n"
			"  vid_num_chunks           = %d\n"
			"  pixel_per_chunk          = %d\n"
			"  byte_per_chunk           = %d\n"
			"  vid_pkt_byte_per_chunk   = %d\n"
			"  total_bytes_per_chunk    = %d\n"
			"  chunk_overhead           = %d\n"
			"  vid_null_size            = %d\n\n",
			multi_pkt_en, vid_num_chunks,
			pixel_per_chunk, byte_per_chunk,
			vid_pkt_byte_per_chunk, total_bytes_per_chunk,
			chunk_overhead, vid_null_size);
	}
}

static void mipi_dsi_vid_mode_config(struct lcd_config_s *pconf)
{
	if (pconf->control.mipi_cfg.video_mode_type == BURST_MODE) {
		dsi_vconf.pixel_per_chunk = pconf->basic.h_active;
		dsi_vconf.vid_num_chunks = 0;
		dsi_vconf.vid_null_size = 0;
	} else {
		mipi_dsi_non_burst_packet_config(pconf);
	}

	mipi_dsi_video_config(pconf);
}

static void mipi_dsi_link_on(struct aml_lcd_drv_s *pdrv)
{
	unsigned int op_mode_init, op_mode_disp;
	struct dsi_config_s *dconf;
#ifdef CONFIG_AMLOGIC_LCD_EXTERN
	struct lcd_extern_driver_s *edrv;
	struct lcd_extern_dev_s *edev;
#endif

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: %s\n", pdrv->index, __func__);

	dconf = &pdrv->config.control.mipi_cfg;
	op_mode_init = dconf->operation_mode_init;
	op_mode_disp = dconf->operation_mode_display;

	if (dconf->dsi_init_on) {
		dsi_write_cmd(pdrv, dconf->dsi_init_on);
		LCDPR("[%d]: dsi init on\n", pdrv->index);
	}

#ifdef CONFIG_AMLOGIC_LCD_EXTERN
	if (dconf->extern_init < LCD_EXTERN_INDEX_INVALID) {
		edrv = lcd_extern_get_driver(pdrv->index);
		edev = lcd_extern_get_dev(edrv, dconf->extern_init);
		if (!edrv || !edev) {
			LCDPR("[%d]: no ext_%d dev\n", pdrv->index, dconf->extern_init);
		} else {
			if (edev->config.table_init_on) {
				dsi_write_cmd(pdrv, edev->config.table_init_on);
				LCDPR("[%d]: ext_%d(%s) dsi init on\n",
				      pdrv->index, dconf->extern_init,
				      edev->config.name);
			}
		}
	}
#endif

	if (op_mode_disp != op_mode_init) {
		set_mipi_dsi_host(pdrv,
				  MIPI_DSI_VIRTUAL_CHAN_ID,
				  0, /* Chroma sub sample, only for
				      * YUV 422 or 420, even or odd
				      */
				 /* DSI operation mode, video or command */
				  op_mode_disp);
		if (op_mode_disp == MIPI_DSI_OPERATION_MODE_VIDEO)
			lcd_venc_enable(pdrv, 1);
	}
}

void mipi_dsi_link_off(struct aml_lcd_drv_s *pdrv)
{
	unsigned int op_mode_init, op_mode_disp;
	struct dsi_config_s *dconf;
#ifdef CONFIG_AMLOGIC_LCD_EXTERN
	struct lcd_extern_driver_s *edrv;
	struct lcd_extern_dev_s *edev;
#endif

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: %s\n", pdrv->index, __func__);

	dconf = &pdrv->config.control.mipi_cfg;
	op_mode_init = dconf->operation_mode_init;
	op_mode_disp = dconf->operation_mode_display;

	if (op_mode_disp != op_mode_init) {
		set_mipi_dsi_host(pdrv,
				  MIPI_DSI_VIRTUAL_CHAN_ID,
				  0, /* Chroma sub sample, only for
				      * YUV 422 or 420, even or odd
				      */
				  /* DSI operation mode, video or command */
				  op_mode_init);
	}

#ifdef CONFIG_AMLOGIC_LCD_EXTERN
	if (dconf->extern_init < LCD_EXTERN_INDEX_INVALID) {
		edrv = lcd_extern_get_driver(pdrv->index);
		edev = lcd_extern_get_dev(edrv, dconf->extern_init);
		if (!edrv || !edev) {
			LCDPR("[%d]: no ext_%d dev\n", pdrv->index, dconf->extern_init);
		} else {
			if (edev->config.table_init_off) {
				dsi_write_cmd(pdrv, edev->config.table_init_off);
				LCDPR("[%d]: ext_%d(%s) dsi init off\n",
				      pdrv->index, dconf->extern_init,
				      edev->config.name);
			}
		}
	}
#endif

	if (dconf->dsi_init_off) {
		dsi_write_cmd(pdrv, dconf->dsi_init_off);
		LCDPR("[%d]: dsi init off\n", pdrv->index);
	}
}

void mipi_dsi_config_init(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_config_s *pconf = &pdrv->config;
	struct dsi_config_s *dconf = &pdrv->config.control.mipi_cfg;
	unsigned int offset;

	/* data format */
	if (pconf->basic.lcd_bits == 6) {
		dconf->venc_data_width = MIPI_DSI_VENC_COLOR_18B;
		dconf->dpi_data_format = MIPI_DSI_COLOR_18BIT;
		if (dconf->dpi_data_format == COLOR_18BIT_CFG_2)
			dconf->data_bits = 8;
		else
			dconf->data_bits = 6;
	} else {
		dconf->venc_data_width = MIPI_DSI_VENC_COLOR_24B;
		dconf->dpi_data_format  = MIPI_DSI_COLOR_24BIT;
		dconf->data_bits = 8;
	}
	dsi_vconf.data_bits = dconf->data_bits;

	/* update check_state */
	if (dconf->check_en) {
		offset = pdrv->data->offset_venc_if[pdrv->index];
		dconf->check_state = lcd_vcbus_getb(L_VCOM_VS_ADDR + offset, 12, 1);
	}

	/* Venc resolution format */
	switch (dconf->phy_switch) {
	case 1: /* standard */
		dsi_phy_config.state_change = 1;
		break;
	case 2: /* slow */
		dsi_phy_config.state_change = 2;
		break;
	case 0: /* auto */
	default:
		if (pconf->basic.h_active != 240 &&
		    pconf->basic.h_active != 768 &&
		    pconf->basic.h_active != 1920 &&
		    pconf->basic.h_active != 2560) {
			dsi_phy_config.state_change = 2;
		} else {
			dsi_phy_config.state_change = 1;
		}
		break;
	}
}

/* bit_rate is confirm by clk_genrate, so internal clk config must after that */
void lcd_mipi_dsi_config_post(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_config_s *pconf = &pdrv->config;
	struct dsi_config_s *dconf = &pdrv->config.control.mipi_cfg;
	unsigned int pclk, lanebyteclk;
	unsigned int den, num;

	pclk = pconf->timing.lcd_clk;

	/* pclk lanebyteclk factor */
	if (dconf->factor_numerator == 0) {
		lanebyteclk = lcd_do_div(pconf->timing.bit_rate, 8);
		LCDPR("pixel_clk = %dHz, bit_rate = %lldHz\n",
		      pclk, pconf->timing.bit_rate);
		dconf->factor_numerator = 8;
		dconf->factor_denominator = dconf->clk_factor;
	}
	num = dconf->factor_numerator;
	den = dconf->factor_denominator;
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
		LCDPR("num=%d, den=%d, factor=%d.%02d\n",
		      num, den, (den / num), ((den % num) * 100 / num));
	}

	if (dconf->operation_mode_display == OPERATION_VIDEO_MODE)
		mipi_dsi_vid_mode_config(pconf);

	/* phy config */
	mipi_dsi_phy_config(&dsi_phy_config, pconf->timing.bit_rate);
}

static void mipi_dsi_host_on(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_config_s *pconf = &pdrv->config;
	unsigned int op_mode_init;

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("%s\n", __func__);

	/* disable encl */
	lcd_venc_enable(pdrv, 0);
	lcd_delay_us(100);

	startup_mipi_dsi_host(pdrv);

	set_dsi_phy_config(pdrv);

	op_mode_init = pconf->control.mipi_cfg.operation_mode_init;
	mipi_dcs_set(pdrv,
		     MIPI_DSI_CMD_TRANS_TYPE, /* 0: high speed, 1: low power */
		     MIPI_DSI_DCS_ACK_TYPE,	   /* if need bta ack check */
		     MIPI_DSI_TEAR_SWITCH);	   /* enable tear ack */

	set_mipi_dsi_host(pdrv,
			  MIPI_DSI_VIRTUAL_CHAN_ID,   /* Virtual channel id */
	/* Chroma sub sample, only for YUV 422 or 420, even or odd */
			  0,
	/* DSI operation mode, video or command */
			  op_mode_init);

	/* Startup transfer */
	mipi_dsi_lpclk_ctrl(pdrv);
	if (op_mode_init == MIPI_DSI_OPERATION_MODE_VIDEO)
		lcd_venc_enable(pdrv, 1);

	mipi_dsi_link_on(pdrv);

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		mipi_dsi_host_print_info(&pdrv->config);
}

static void mipi_dsi_host_off(struct aml_lcd_drv_s *pdrv)
{
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("%s\n", __func__);

	/* Power down DSI */
	dsi_host_write(pdrv, MIPI_DSI_DWC_PWR_UP_OS, 0);

	/* Power down D-PHY, do not have to close dphy */
	/* dsi_host_write(pdrv, MIPI_DSI_DWC_PHY_RSTZ_OS,
	 *	(dsi_host_read(pdrv,  MIPI_DSI_DWC_PHY_RSTZ_OS ) & 0xc));
	 */
	/* dsi_host_write(pdrv, MIPI_DSI_DWC_PHY_RSTZ_OS, 0xc); */

	dsi_phy_write(pdrv, MIPI_DSI_CHAN_CTRL, 0x1f);
	//LCDPR("MIPI_DSI_PHY_CTRL=0x%x\n", dsi_phy_read(pdrv, MIPI_DSI_PHY_CTRL));
	dsi_phy_setb(pdrv, MIPI_DSI_PHY_CTRL, 0, 7, 1);
}

void mipi_dsi_tx_ctrl(struct aml_lcd_drv_s *pdrv, int status)
{
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("%s: %d\n", __func__, status);

	if (status)
		mipi_dsi_host_on(pdrv);
	else
		mipi_dsi_host_off(pdrv);
}

int lcd_mipi_test_read(struct aml_lcd_drv_s *pdrv, struct dsi_read_s *dread)
{
	unsigned char payload[3] = {DT_GEN_RD_1, 1, 0x04};
	unsigned int reg, offset;
	int ret = 0;

	if (!dread)
		return 1;

	if (pdrv->data->chip_type == LCD_CHIP_T7) {
		offset = pdrv->data->offset_venc[pdrv->index];
		reg = VPU_VENCP_STAT + offset;
	} else {
		reg = ENCL_INFO_READ;
	}

	dread->line_start = lcd_vcbus_getb(reg, 16, 13);

	payload[2] = dread->reg;
	ret = dsi_read_single(pdrv, payload, dread->value, dread->cnt);
	if (ret < 0) {
		dread->ret_code = 2;
		return 2;
	}
	if (ret > dread->cnt) {
		dread->ret_code = 3;
		return 3;
	}

	dread->line_end = lcd_vcbus_getb(reg, 16, 13);

	dread->ret_code = 0;
	return 0;
}
