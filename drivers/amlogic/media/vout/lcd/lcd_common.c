// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 *
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
 */

#include <linux/init.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/reset.h>
#include <linux/clk.h>
#include <linux/amlogic/media/vout/lcd/aml_lcd.h>
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>
#include <linux/amlogic/media/vout/lcd/lcd_extern.h>
#include <linux/amlogic/media/vout/lcd/lcd_notify.h>
#include <linux/amlogic/media/vout/lcd/lcd_unifykey.h>
#include <linux/amlogic/media/vout/lcd/aml_bl.h>
#include <linux/amlogic/media/vout/vinfo.h>
#include <linux/page-flags.h>
#include <linux/mm.h>
#include "lcd_common.h"
#include "lcd_reg.h"

void lcd_delay_us(int us)
{
	if (us > 0 && us < 20000)
		usleep_range(us, us + 1);
	else if (us >= 20000)
		msleep(us / 1000);
}

void lcd_delay_ms(int ms)
{
	if (ms > 0 && ms < 20)
		usleep_range(ms * 1000, ms * 1000 + 1);
	else if (ms >= 20)
		msleep(ms);
}

static struct lcd_i2c_match_s lcd_i2c_match_table[] = {
	{LCD_EXT_I2C_BUS_0,   "i2c_0"},
	{LCD_EXT_I2C_BUS_1,   "i2c_1"},
	{LCD_EXT_I2C_BUS_2,   "i2c_2"},
	{LCD_EXT_I2C_BUS_3,   "i2c_3"},
	{LCD_EXT_I2C_BUS_4,   "i2c_4"},
	{LCD_EXT_I2C_BUS_0,   "i2c_a"},
	{LCD_EXT_I2C_BUS_1,   "i2c_b"},
	{LCD_EXT_I2C_BUS_2,   "i2c_c"},
	{LCD_EXT_I2C_BUS_3,   "i2c_d"},
	{LCD_EXT_I2C_BUS_4,   "i2c_ao"},
	{LCD_EXT_I2C_BUS_0,   "i2c_bus_0"},
	{LCD_EXT_I2C_BUS_1,   "i2c_bus_1"},
	{LCD_EXT_I2C_BUS_2,   "i2c_bus_2"},
	{LCD_EXT_I2C_BUS_3,   "i2c_bus_3"},
	{LCD_EXT_I2C_BUS_4,   "i2c_bus_4"},
	{LCD_EXT_I2C_BUS_0,   "i2c_bus_a"},
	{LCD_EXT_I2C_BUS_1,   "i2c_bus_b"},
	{LCD_EXT_I2C_BUS_2,   "i2c_bus_c"},
	{LCD_EXT_I2C_BUS_3,   "i2c_bus_d"},
	{LCD_EXT_I2C_BUS_4,   "i2c_bus_ao"},
	{LCD_EXT_I2C_BUS_MAX, "invalid"}
};

unsigned char aml_lcd_i2c_bus_get_str(const char *str)
{
	unsigned char i2c_bus = LCD_EXT_I2C_BUS_MAX;
	int i;

	for (i = 0; i < ARRAY_SIZE(lcd_i2c_match_table); i++) {
		if (strcmp(lcd_i2c_match_table[i].bus_str, str) == 0) {
			i2c_bus = lcd_i2c_match_table[i].bus_id;
			break;
		}
	}

	if (i2c_bus == LCD_EXT_I2C_BUS_MAX)
		LCDERR("%s: invalid i2c_bus: %s\n", __func__, str);

	return i2c_bus;
}

/* **********************************
 * lcd type
 * **********************************
 */
struct lcd_type_match_s {
	char *name;
	enum lcd_type_e type;
};

static struct lcd_type_match_s lcd_type_match_table[] = {
	{"ttl",      LCD_TTL},
	{"lvds",     LCD_LVDS},
	{"vbyone",   LCD_VBYONE},
	{"mipi",     LCD_MIPI},
	{"minilvds", LCD_MLVDS},
	{"p2p",      LCD_P2P},
	{"edp",      LCD_EDP},
	{"invalid",  LCD_TYPE_MAX},
};

int lcd_type_str_to_type(const char *str)
{
	int type = LCD_TYPE_MAX;
	int i;

	for (i = 0; i < ARRAY_SIZE(lcd_type_match_table); i++) {
		if (!strcmp(str, lcd_type_match_table[i].name)) {
			type = lcd_type_match_table[i].type;
			break;
		}
	}
	return type;
}

char *lcd_type_type_to_str(int type)
{
	char *name = lcd_type_match_table[LCD_TYPE_MAX].name;
	int i;

	for (i = 0; i < ARRAY_SIZE(lcd_type_match_table); i++) {
		if (type == lcd_type_match_table[i].type) {
			name = lcd_type_match_table[i].name;
			break;
		}
	}
	return name;
}

static char *lcd_mode_table[] = {
	"tv",
	"tablet",
	"invalid",
};

unsigned char lcd_mode_str_to_mode(const char *str)
{
	unsigned char mode;

	for (mode = 0; mode < ARRAY_SIZE(lcd_mode_table); mode++) {
		if (!strcmp(str, lcd_mode_table[mode]))
			break;
	}
	return mode;
}

char *lcd_mode_mode_to_str(int mode)
{
	return lcd_mode_table[mode];
}

u8 *lcd_vmap(ulong addr, u32 size)
{
	u8 *vaddr = NULL;
	struct page **pages = NULL;
	u32 i, npages, offset = 0;
	ulong phys, page_start;
	/*pgprot_t pgprot = pgprot_noncached(PAGE_KERNEL);*/
	pgprot_t pgprot = PAGE_KERNEL;

	if (!PageHighMem(phys_to_page(addr)))
		return phys_to_virt(addr);

	offset = offset_in_page(addr);
	page_start = addr - offset;
	npages = DIV_ROUND_UP(size + offset, PAGE_SIZE);

	pages = kmalloc_array(npages, sizeof(struct page *), GFP_KERNEL);
	if (!pages)
		return NULL;
	for (i = 0; i < npages; i++) {
		phys = page_start + i * PAGE_SIZE;
		pages[i] = pfn_to_page(phys >> PAGE_SHIFT);
	}

	vaddr = vmap(pages, npages, VM_MAP, pgprot);
	if (!vaddr) {
		LCDERR("the phy(%lx) vmaped fail, size: %d\n",
		       page_start, npages << PAGE_SHIFT);
		kfree(pages);
		return NULL;
	}
	kfree(pages);

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
		LCDPR("[lcd HIGH-MEM-MAP] %s, pa(%lx) to va(%p), size: %d\n",
		      __func__, page_start, vaddr, npages << PAGE_SHIFT);
	}

	return vaddr + offset;
}

void lcd_unmap_phyaddr(u8 *vaddr)
{
	void *addr = (void *)(PAGE_MASK & (ulong)vaddr);

	if (is_vmalloc_or_module_addr(vaddr)) {
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
			LCDPR("lcd unmap v: %p\n", addr);
		vunmap(addr);
	}
}

/* **********************************
 * lcd gpio
 * **********************************
 */

void lcd_cpu_gpio_probe(struct aml_lcd_drv_s *pdrv, unsigned int index)
{
	struct lcd_cpu_gpio_s *cpu_gpio;
	const char *str;
	int ret;

	if (!pdrv->dev->of_node) {
		LCDERR("[%d]: %s: dev of_node is null\n", pdrv->index, __func__);
		return;
	}

	if (index >= LCD_CPU_GPIO_NUM_MAX) {
		LCDERR("[%d]: gpio index %d, exit\n", pdrv->index, index);
		return;
	}
	cpu_gpio = &pdrv->config.power.cpu_gpio[index];
	if (cpu_gpio->probe_flag) {
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
			LCDPR("[%d]: gpio %s[%d] is already registered\n",
			      pdrv->index, cpu_gpio->name, index);
		}
		return;
	}

	/* get gpio name */
	ret = of_property_read_string_index(pdrv->dev->of_node, "lcd_cpu_gpio_names", index, &str);
	if (ret) {
		LCDERR("[%d]: failed to get lcd_cpu_gpio_names: %d\n",
		       pdrv->index, index);
		str = "unknown";
	}
	strcpy(cpu_gpio->name, str);

	/* init gpio flag */
	cpu_gpio->probe_flag = 1;
	cpu_gpio->register_flag = 0;
}

static int lcd_cpu_gpio_register(struct aml_lcd_drv_s *pdrv, unsigned int index, int init_value)
{
	struct lcd_cpu_gpio_s *cpu_gpio;
	int value;

	if (index >= LCD_CPU_GPIO_NUM_MAX) {
		LCDERR("[%d]: %s: gpio index %d, exit\n",
		       pdrv->index, __func__, index);
		return -1;
	}
	cpu_gpio = &pdrv->config.power.cpu_gpio[index];
	if (cpu_gpio->probe_flag == 0) {
		LCDERR("[%d]: %s: gpio [%d] is not probed, exit\n",
		       pdrv->index, __func__, index);
		return -1;
	}
	if (cpu_gpio->register_flag) {
		LCDPR("[%d]: %s: gpio %s[%d] is already registered\n",
		      pdrv->index, __func__, cpu_gpio->name, index);
		return 0;
	}

	switch (init_value) {
	case LCD_GPIO_OUTPUT_LOW:
		value = GPIOD_OUT_LOW;
		break;
	case LCD_GPIO_OUTPUT_HIGH:
		value = GPIOD_OUT_HIGH;
		break;
	case LCD_GPIO_INPUT:
	default:
		value = GPIOD_IN;
		break;
	}

	/* request gpio */
	cpu_gpio->gpio = devm_gpiod_get_index(pdrv->dev, "lcd_cpu", index, value);
	if (IS_ERR(cpu_gpio->gpio)) {
		LCDERR("[%d]: register gpio %s[%d]: %p, err: %d\n",
		       pdrv->index, cpu_gpio->name, index, cpu_gpio->gpio,
		       IS_ERR(cpu_gpio->gpio));
		return -1;
	}
	cpu_gpio->register_flag = 1;
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
		LCDPR("[%d]: register gpio %s[%d]: %p, init value: %d\n",
		      pdrv->index, cpu_gpio->name, index,
		      cpu_gpio->gpio, init_value);
	}

	return 0;
}

void lcd_cpu_gpio_set(struct aml_lcd_drv_s *pdrv, unsigned int index, int value)
{
	struct lcd_cpu_gpio_s *cpu_gpio;

	if (index >= LCD_CPU_GPIO_NUM_MAX) {
		LCDERR("[%d]: gpio index %d, exit\n", pdrv->index, index);
		return;
	}
	cpu_gpio = &pdrv->config.power.cpu_gpio[index];
	if (cpu_gpio->probe_flag == 0) {
		LCDERR("[%d]: %s: gpio [%d] is not probed, exit\n",
		       pdrv->index, __func__, index);
		return;
	}
	if (cpu_gpio->register_flag == 0) {
		lcd_cpu_gpio_register(pdrv, index, value);
		return;
	}

	if (IS_ERR_OR_NULL(cpu_gpio->gpio)) {
		LCDERR("[%d]: gpio %s[%d]: %p, err: %ld\n",
		       pdrv->index, cpu_gpio->name, index, cpu_gpio->gpio,
		       PTR_ERR(cpu_gpio->gpio));
		return;
	}

	switch (value) {
	case LCD_GPIO_OUTPUT_LOW:
	case LCD_GPIO_OUTPUT_HIGH:
		gpiod_direction_output(cpu_gpio->gpio, value);
		break;
	case LCD_GPIO_INPUT:
	default:
		gpiod_direction_input(cpu_gpio->gpio);
		break;
	}
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
		LCDPR("[%d]: set gpio %s[%d] value: %d\n",
		      pdrv->index, cpu_gpio->name, index, value);
	}
}

unsigned int lcd_cpu_gpio_get(struct aml_lcd_drv_s *pdrv, unsigned int index)
{
	struct lcd_cpu_gpio_s *cpu_gpio;

	cpu_gpio = &pdrv->config.power.cpu_gpio[index];
	if (cpu_gpio->probe_flag == 0) {
		LCDERR("[%d]: %s: gpio [%d] is not probed\n",
		       pdrv->index, __func__, index);
		return -1;
	}
	if (cpu_gpio->register_flag == 0) {
		LCDERR("[%d]: %s: gpio %s[%d] is not registered\n",
		       pdrv->index, __func__, cpu_gpio->name, index);
		return -1;
	}
	if (IS_ERR_OR_NULL(cpu_gpio->gpio)) {
		LCDERR("[%d]: gpio[%d]: %p, err: %ld\n",
		       pdrv->index, index, cpu_gpio->gpio,
		       PTR_ERR(cpu_gpio->gpio));
		return -1;
	}

	return gpiod_get_value(cpu_gpio->gpio);
}

static void lcd_custom_pinmux_set(struct aml_lcd_drv_s *pdrv, int status)
{
	struct lcd_config_s *pconf;
	unsigned int index;
	char pinmux_str[35];

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: %s: %d\n", pdrv->index, __func__, status);

	pconf = &pdrv->config;

	memset(pinmux_str, 0, sizeof(pinmux_str));
	if (status) {
		index = 0;
		sprintf(pinmux_str, "%s", pconf->basic.model_name);
	} else {
		index = 1;
		sprintf(pinmux_str, "%s_off", pconf->basic.model_name);
	}

	if (pconf->pinmux_flag == index) {
		LCDPR("pinmux %s is already selected\n", pinmux_str);
		return;
	}

	pconf->pin = devm_pinctrl_get_select(pdrv->dev, pinmux_str);
	if (IS_ERR(pconf->pin)) {
		LCDERR("set custom_pinmux %s error\n", pinmux_str);
	} else {
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
			LCDPR("set custom_pinmux %s: 0x%p\n",
			      pinmux_str, pconf->pin);
		}
	}
	pconf->pinmux_flag = index;
}

static char *lcd_ttl_pinmux_str[] = {
	"ttl_6bit_hvsync_on",      /* 0 */
	"ttl_6bit_de_on",          /* 1 */
	"ttl_6bit_hvsync_de_on",   /* 2 */
	"ttl_6bit_hvsync_de_off",  /* 3 */
	"ttl_8bit_hvsync_on",      /* 4 */
	"ttl_8bit_de_on",          /* 5 */
	"ttl_8bit_hvsync_de_on",   /* 6 */
	"ttl_8bit_hvsync_de_off",  /* 7 */
};

void lcd_ttl_pinmux_set(struct aml_lcd_drv_s *pdrv, int status)
{
	struct lcd_config_s *pconf;
	unsigned int base, index;

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: %s: %d\n", pdrv->index, __func__, status);

	pconf = &pdrv->config;
	if (pconf->basic.lcd_bits == 6)
		base = 0;
	else
		base = 4;

	if (status) {
		switch (pconf->control.ttl_cfg.sync_valid) {
		case 1: /* hvsync */
			index = base + 0;
			break;
		case 2: /* DE */
			index = base + 1;
			break;
		default:
		case 3: /* DE + hvsync */
			index = base + 2;
			break;
		}
	} else {
		index = base + 3;
	}

	if (pconf->pinmux_flag == index) {
		LCDPR("[%d]: pinmux %s is already selected\n",
		      pdrv->index, lcd_ttl_pinmux_str[index]);
		return;
	}

	/* request pinmux */
	pconf->pin = devm_pinctrl_get_select(pdrv->dev, lcd_ttl_pinmux_str[index]);
	if (IS_ERR(pconf->pin)) {
		LCDERR("[%d]: set ttl pinmux %s error\n",
		       pdrv->index, lcd_ttl_pinmux_str[index]);
	} else {
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
			LCDPR("[%d]: set ttl pinmux %s: 0x%p\n",
			      pdrv->index, lcd_ttl_pinmux_str[index],
			      pconf->pin);
		}
	}
	pconf->pinmux_flag = index;
}

static char *lcd_vbyone_pinmux_str[] = {
	"vbyone",
	"vbyone_off",
	"none",
};

/* set VX1_LOCKN && VX1_HTPDN */
void lcd_vbyone_pinmux_set(struct aml_lcd_drv_s *pdrv, int status)
{
	struct lcd_config_s *pconf;
	unsigned int index;

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: %s: %d\n", pdrv->index, __func__, status);

	pconf = &pdrv->config;
	index = (status) ? 0 : 1;

	if (pconf->pinmux_flag == index) {
		LCDPR("[%d]: pinmux %s is already selected\n",
		      pdrv->index, lcd_vbyone_pinmux_str[index]);
		return;
	}

	pconf->pin = devm_pinctrl_get_select(pdrv->dev, lcd_vbyone_pinmux_str[index]);
	if (IS_ERR(pconf->pin)) {
		LCDERR("[%d]: set vbyone pinmux %s error\n",
		       pdrv->index, lcd_vbyone_pinmux_str[index]);
	} else {
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
			LCDPR("[%d]: set vbyone pinmux %s: 0x%p\n",
			      pdrv->index, lcd_vbyone_pinmux_str[index],
			      pconf->pin);
		}
	}
	pconf->pinmux_flag = index;
}

static char *lcd_tcon_pinmux_str[] = {
	"tcon_p2p",       /* 0 */
	"tcon_p2p_usit",  /* 1 */
	"tcon_p2p_off",   /* 2 */
	"tcon_mlvds",     /* 3 */
	"tcon_mlvds_off", /* 4 */
	"none"		      /* 5 */
};

void lcd_mlvds_pinmux_set(struct aml_lcd_drv_s *pdrv, int status)
{
	struct lcd_config_s *pconf;
	unsigned int index;

	if (!pdrv)
		return;

	if (pdrv->config.custom_pinmux) {
		lcd_custom_pinmux_set(pdrv, status);
		return;
	}

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: %s: %d\n", pdrv->index, __func__, status);

	pconf = &pdrv->config;
	index = (status) ? 3 : 4;

	if (pconf->pinmux_flag == index) {
		LCDPR("[%d]: pinmux %s is already selected\n",
		      pdrv->index, lcd_tcon_pinmux_str[index]);
		return;
	}

	pconf->pin = devm_pinctrl_get_select(pdrv->dev, lcd_tcon_pinmux_str[index]);
	if (IS_ERR(pconf->pin)) {
		LCDERR("[%d]: set mlvds pinmux %s error\n",
		       pdrv->index, lcd_tcon_pinmux_str[index]);
	} else {
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
			LCDPR("[%d]: set mlvds pinmux %s: 0x%p\n",
			      pdrv->index, lcd_tcon_pinmux_str[index],
			      pconf->pin);
		}
	}
	pconf->pinmux_flag = index;
}

void lcd_p2p_pinmux_set(struct aml_lcd_drv_s *pdrv, int status)
{
	struct lcd_config_s *pconf;
	unsigned int index, p2p_type;

	if (!pdrv)
		return;

	if (pdrv->config.custom_pinmux) {
		lcd_custom_pinmux_set(pdrv, status);
		return;
	}

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: %s: %d\n", pdrv->index, __func__, status);

	pconf = &pdrv->config;
	p2p_type = pconf->control.p2p_cfg.p2p_type & 0x1f;
	if (p2p_type == P2P_USIT)
		index = (status) ? 1 : 2;
	else
		index = (status) ? 0 : 2;

	if (pconf->pinmux_flag == index) {
		LCDPR("[%d]: pinmux %s is already selected\n",
		      pdrv->index, lcd_tcon_pinmux_str[index]);
		return;
	}

	pconf->pin = devm_pinctrl_get_select(pdrv->dev, lcd_tcon_pinmux_str[index]);
	if (IS_ERR(pconf->pin)) {
		LCDERR("[%d]: set p2p pinmux %s error\n",
		       pdrv->index, lcd_tcon_pinmux_str[index]);
	} else {
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
			LCDPR("[%d]: set p2p pinmux %s: 0x%p\n",
			      pdrv->index, lcd_tcon_pinmux_str[index],
			      pconf->pin);
		}
	}
	pconf->pinmux_flag = index;
}

static char *lcd_edp_pinmux_str[] = {
	"edp",
	"edp_off",
	"none",
};

void lcd_edp_pinmux_set(struct aml_lcd_drv_s *pdrv, int status)
{
	struct lcd_config_s *pconf;
	unsigned int index;

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: %s: %d\n", pdrv->index, __func__, status);

	pconf = &pdrv->config;
	index = (status) ? 0 : 1;

	if (pconf->pinmux_flag == index) {
		LCDPR("[%d]: pinmux %s is already selected\n",
		      pdrv->index, lcd_edp_pinmux_str[index]);
		return;
	}

	pconf->pin = devm_pinctrl_get_select(pdrv->dev, lcd_edp_pinmux_str[index]);
	if (IS_ERR(pconf->pin)) {
		LCDERR("[%d]: set edp pinmux %s error\n",
		       pdrv->index, lcd_edp_pinmux_str[index]);
	} else {
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
			LCDPR("[%d]: set edp pinmux %s: 0x%p\n",
			      pdrv->index, lcd_edp_pinmux_str[index],
			      pconf->pin);
		}
	}
	pconf->pinmux_flag = index;
}

/* ************************************************** *
 * lcd config
 * **************************************************
 */
static void lcd_config_load_print(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_config_s *pconf = &pdrv->config;
	union lcd_ctrl_config_u *pctrl;

	LCDPR("[%d]: %s, %s, %dbit, %dx%d\n",
	      pdrv->index,
	      pconf->basic.model_name,
	      lcd_type_type_to_str(pconf->basic.lcd_type),
	      pconf->basic.lcd_bits,
	      pconf->basic.h_active, pconf->basic.v_active);

	if ((lcd_debug_print_flag & LCD_DBG_PR_NORMAL) == 0)
		return;

	LCDPR("h_period = %d\n", pconf->basic.h_period);
	LCDPR("v_period = %d\n", pconf->basic.v_period);
	LCDPR("screen_width = %d\n", pconf->basic.screen_width);
	LCDPR("screen_height = %d\n", pconf->basic.screen_height);

	LCDPR("h_period_min = %d\n", pconf->basic.h_period_min);
	LCDPR("h_period_max = %d\n", pconf->basic.h_period_max);
	LCDPR("v_period_min = %d\n", pconf->basic.v_period_min);
	LCDPR("v_period_max = %d\n", pconf->basic.v_period_max);
	LCDPR("frame_rate_min = %d\n", pconf->basic.frame_rate_min);
	LCDPR("frame_rate_max = %d\n", pconf->basic.frame_rate_max);
	LCDPR("pclk_min = %d\n", pconf->basic.lcd_clk_min);
	LCDPR("pclk_max = %d\n", pconf->basic.lcd_clk_max);

	LCDPR("hsync_width = %d\n", pconf->timing.hsync_width);
	LCDPR("hsync_bp = %d\n", pconf->timing.hsync_bp);
	LCDPR("hsync_pol = %d\n", pconf->timing.hsync_pol);
	LCDPR("vsync_width = %d\n", pconf->timing.vsync_width);
	LCDPR("vsync_bp = %d\n", pconf->timing.vsync_bp);
	LCDPR("vsync_pol = %d\n", pconf->timing.vsync_pol);

	LCDPR("fr_adjust_type = %d\n", pconf->timing.fr_adjust_type);
	LCDPR("ss_level = %d\n", pconf->timing.ss_level);
	LCDPR("ss_freq = %d\n", pconf->timing.ss_freq);
	LCDPR("ss_mode = %d\n", pconf->timing.ss_mode);
	LCDPR("clk_auto = %d\n", pconf->timing.clk_auto);
	LCDPR("pixel_clk = %d\n", pconf->timing.lcd_clk);

	LCDPR("custom_pinmux = %d\n", pconf->custom_pinmux);
	LCDPR("fr_auto_dis = %d\n", pconf->fr_auto_dis);

	pctrl = &pconf->control;
	if (pconf->basic.lcd_type == LCD_TTL) {
		LCDPR("clk_pol = %d\n", pctrl->ttl_cfg.clk_pol);
		LCDPR("sync_valid = %d\n", pctrl->ttl_cfg.sync_valid);
		LCDPR("swap_ctrl = %d\n", pctrl->ttl_cfg.swap_ctrl);
	} else if (pconf->basic.lcd_type == LCD_LVDS) {
		LCDPR("lvds_repack = %d\n", pctrl->lvds_cfg.lvds_repack);
		LCDPR("pn_swap = %d\n", pctrl->lvds_cfg.pn_swap);
		LCDPR("dual_port = %d\n", pctrl->lvds_cfg.dual_port);
		LCDPR("port_swap = %d\n", pctrl->lvds_cfg.port_swap);
		LCDPR("lane_reverse = %d\n", pctrl->lvds_cfg.lane_reverse);
		LCDPR("phy_vswing = 0x%x\n", pctrl->lvds_cfg.phy_vswing);
		LCDPR("phy_preem = 0x%x\n", pctrl->lvds_cfg.phy_preem);
	} else if (pconf->basic.lcd_type == LCD_VBYONE) {
		LCDPR("lane_count = %d\n", pctrl->vbyone_cfg.lane_count);
		LCDPR("byte_mode = %d\n", pctrl->vbyone_cfg.byte_mode);
		LCDPR("region_num = %d\n", pctrl->vbyone_cfg.region_num);
		LCDPR("color_fmt = %d\n", pctrl->vbyone_cfg.color_fmt);
		LCDPR("phy_vswing = 0x%x\n", pctrl->vbyone_cfg.phy_vswing);
		LCDPR("phy_preem = 0x%x\n", pctrl->vbyone_cfg.phy_preem);
	} else if (pconf->basic.lcd_type == LCD_MLVDS) {
		LCDPR("channel_num = %d\n", pctrl->mlvds_cfg.channel_num);
		LCDPR("channel_sel0 = %d\n", pctrl->mlvds_cfg.channel_sel0);
		LCDPR("channel_sel1 = %d\n", pctrl->mlvds_cfg.channel_sel1);
		LCDPR("clk_phase = %d\n", pctrl->mlvds_cfg.clk_phase);
		LCDPR("phy_vswing = 0x%x\n", pctrl->mlvds_cfg.phy_vswing);
		LCDPR("phy_preem = 0x%x\n", pctrl->mlvds_cfg.phy_preem);
	} else if (pconf->basic.lcd_type == LCD_P2P) {
		LCDPR("p2p_type = %d\n", pctrl->p2p_cfg.p2p_type);
		LCDPR("lane_num = %d\n", pctrl->p2p_cfg.lane_num);
		LCDPR("channel_sel0 = %d\n", pctrl->p2p_cfg.channel_sel0);
		LCDPR("channel_sel1 = %d\n", pctrl->p2p_cfg.channel_sel1);
		LCDPR("phy_vswing = 0x%x\n", pctrl->p2p_cfg.phy_vswing);
		LCDPR("phy_preem = 0x%x\n", pctrl->p2p_cfg.phy_preem);
	} else if (pconf->basic.lcd_type == LCD_MIPI) {
		if (pctrl->mipi_cfg.check_en) {
			LCDPR("check_reg = 0x%02x\n",
				pctrl->mipi_cfg.check_reg);
			LCDPR("check_cnt = %d\n",
				pctrl->mipi_cfg.check_cnt);
		}
		LCDPR("lane_num = %d\n", pctrl->mipi_cfg.lane_num);
		LCDPR("bit_rate_max = %d\n", pctrl->mipi_cfg.bit_rate_max);
		LCDPR("pclk_lanebyteclk_factor = %d\n",
		      pctrl->mipi_cfg.factor_numerator);
		LCDPR("operation_mode_init = %d\n",
		      pctrl->mipi_cfg.operation_mode_init);
		LCDPR("operation_mode_disp = %d\n",
		      pctrl->mipi_cfg.operation_mode_display);
		LCDPR("video_mode_type = %d\n",
		      pctrl->mipi_cfg.video_mode_type);
		LCDPR("clk_always_hs = %d\n", pctrl->mipi_cfg.clk_always_hs);
		LCDPR("phy_switch = %d\n", pctrl->mipi_cfg.phy_switch);
		LCDPR("extern_init = %d\n", pctrl->mipi_cfg.extern_init);
	} else if (pconf->basic.lcd_type == LCD_EDP) {
		LCDPR("max_lane_count      = %d\n",
		      pctrl->edp_cfg.max_lane_count);
		LCDPR("max_link_rate       = %d\n",
		      pctrl->edp_cfg.max_link_rate);
		LCDPR("training_mode       = %d\n",
		      pctrl->edp_cfg.training_mode);
		LCDPR("edid_en             = %d\n",
		      pctrl->edp_cfg.edid_en);
		LCDPR("dpcd_caps_en        = %d\n",
		      pctrl->edp_cfg.dpcd_caps_en);
		LCDPR("sync_clk_mode       = %d\n",
		      pctrl->edp_cfg.sync_clk_mode);

		LCDPR("lane_count          = %d\n", pctrl->edp_cfg.lane_count);
		LCDPR("link_rate           = %d\n", pctrl->edp_cfg.link_rate);
		LCDPR("bit_rate            = %d\n", pctrl->edp_cfg.bit_rate);
		LCDPR("training_settings   = %d\n",
		      pctrl->edp_cfg.training_settings);
		LCDPR("main_stream_enable  = %d\n",
		      pctrl->edp_cfg.main_stream_enable);

		LCDPR("phy_vswing = 0x%x\n", pctrl->edp_cfg.phy_vswing);
		LCDPR("phy_preem  = 0x%x\n", pctrl->edp_cfg.phy_preem);
	}
}

static int lcd_power_load_from_dts(struct aml_lcd_drv_s *pdrv, struct device_node *child)
{
	struct lcd_power_ctrl_s *power_step = &pdrv->config.power;
	int ret = 0;
	unsigned int para[5];
	unsigned int val;
	int i, j, temp;
	unsigned int index;

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: %s\n", pdrv->index, __func__);

	if (!child) {
		LCDERR("[%d]: error: failed to get %s\n",
		      pdrv->index, pdrv->config.propname);
		return -1;
	}

	ret = of_property_read_u32_array(child, "power_on_step", &para[0], 4);
	if (ret) {
		LCDPR("[%d]: failed to get power_on_step\n", pdrv->index);
		power_step->power_on_step[0].type = LCD_POWER_TYPE_MAX;
	} else {
		i = 0;
		while (i < LCD_PWR_STEP_MAX) {
			power_step->power_on_step_max = i;
			j = 4 * i;
			ret = of_property_read_u32_index(child, "power_on_step", j, &val);
			if (ret) {
				LCDPR("[%d]: failed to get power_on_step %d\n",
				      pdrv->index, i);
				power_step->power_on_step[i].type = 0xff;
				break;
			}
			power_step->power_on_step[i].type = (unsigned char)val;
			if (val == 0xff) /* ending */
				break;
			j = 4 * i + 1;
			ret = of_property_read_u32_index(child, "power_on_step", j, &val);
			if (ret) {
				LCDPR("[%d]: failed to get power_on_step %d\n",
				      pdrv->index, i);
				power_step->power_on_step[i].type = 0xff;
				break;
			}
			power_step->power_on_step[i].index = val;
			j = 4 * i + 2;
			ret = of_property_read_u32_index(child, "power_on_step", j, &val);
			if (ret) {
				LCDPR("[%d]: failed to get power_on_step %d\n",
				      pdrv->index, i);
				power_step->power_on_step[i].type = 0xff;
				break;
			}
			power_step->power_on_step[i].value = val;
			j = 4 * i + 3;
			ret = of_property_read_u32_index(child, "power_on_step", j, &val);
			if (ret) {
				LCDPR("[%d]: failed to get power_on_step %d\n",
				      pdrv->index, i);
				power_step->power_on_step[i].type = 0xff;
				break;
			}
			power_step->power_on_step[i].delay = val;

			/* gpio/extern probe */
			index = power_step->power_on_step[i].index;
			switch (power_step->power_on_step[i].type) {
			case LCD_POWER_TYPE_CPU:
			case LCD_POWER_TYPE_WAIT_GPIO:
				if (index < LCD_CPU_GPIO_NUM_MAX)
					lcd_cpu_gpio_probe(pdrv, index);
				break;
			case LCD_POWER_TYPE_EXTERN:
				lcd_extern_dev_index_add(pdrv->index, index);
				break;
			case LCD_POWER_TYPE_CLK_SS:
				temp = power_step->power_on_step[i].value;
				pdrv->config.timing.ss_freq = temp & 0xf;
				pdrv->config.timing.ss_mode = (temp >> 4) & 0xf;
				if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
					LCDPR("[%d]: clk_ss value=0x%x, final ss_level=0x%x\n",
					      pdrv->index, temp, pdrv->config.timing.ss_level);
				}
				break;
			default:
				break;
			}
			if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
				LCDPR("[%d]: power_on %d type: %d\n",
				      pdrv->index, i,
				      power_step->power_on_step[i].type);
				LCDPR("[%d]: power_on %d index: %d\n",
				      pdrv->index, i,
				      power_step->power_on_step[i].index);
				LCDPR("[%d]: power_on %d value: %d\n",
				      pdrv->index, i,
				      power_step->power_on_step[i].value);
				LCDPR("[%d]: power_on %d delay: %d\n",
				      pdrv->index, i,
				      power_step->power_on_step[i].delay);
			}
			i++;
		}
	}

	ret = of_property_read_u32_array(child, "power_off_step", &para[0], 4);
	if (ret) {
		LCDPR("[%d]: failed to get power_off_step\n", pdrv->index);
		power_step->power_off_step[0].type = LCD_POWER_TYPE_MAX;
	} else {
		i = 0;
		while (i < LCD_PWR_STEP_MAX) {
			power_step->power_off_step_max = i;
			j = 4 * i;
			ret = of_property_read_u32_index(child, "power_off_step", j, &val);
			if (ret) {
				LCDPR("[%d]: failed to get power_off_step %d\n",
				      pdrv->index, i);
				power_step->power_off_step[i].type = 0xff;
				break;
			}
			power_step->power_off_step[i].type = (unsigned char)val;
			if (val == 0xff) /* ending */
				break;
			j = 4 * i + 1;
			ret = of_property_read_u32_index(child, "power_off_step", j, &val);
			if (ret) {
				LCDPR("[%d]: failed to get power_off_step %d\n",
				      pdrv->index, i);
				power_step->power_off_step[i].type = 0xff;
				break;
			}
			power_step->power_off_step[i].index = val;
			j = 4 * i + 2;
			ret = of_property_read_u32_index(child, "power_off_step", j, &val);
			if (ret) {
				LCDPR("[%d]: failed to get power_off_step %d\n",
				      pdrv->index, i);
				power_step->power_off_step[i].type = 0xff;
				break;
			}
			power_step->power_off_step[i].value = val;
			j = 4 * i + 3;
			ret = of_property_read_u32_index(child, "power_off_step", j, &val);
			if (ret) {
				LCDPR("[%d]: failed to get power_off_step %d\n",
				      pdrv->index, i);
				power_step->power_off_step[i].type = 0xff;
				break;
			}
			power_step->power_off_step[i].delay = val;

			/* gpio/extern probe */
			index = power_step->power_off_step[i].index;
			switch (power_step->power_off_step[i].type) {
			case LCD_POWER_TYPE_CPU:
			case LCD_POWER_TYPE_WAIT_GPIO:
				if (index < LCD_CPU_GPIO_NUM_MAX)
					lcd_cpu_gpio_probe(pdrv, index);
				break;
			case LCD_POWER_TYPE_EXTERN:
				lcd_extern_dev_index_add(pdrv->index, index);
				break;
			default:
				break;
			}
			if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
				LCDPR("[%d]: power_off %d type: %d\n", i,
				      pdrv->index,
				      power_step->power_off_step[i].type);
				LCDPR("[%d]: power_off %d index: %d\n", i,
				      pdrv->index,
				      power_step->power_off_step[i].index);
				LCDPR("[%d]: power_off %d value: %d\n", i,
				      pdrv->index,
				      power_step->power_off_step[i].value);
				LCDPR("[%d]: power_off %d delay: %d\n", i,
				      pdrv->index,
				      power_step->power_off_step[i].delay);
			}
			i++;
		}
	}

	return ret;
}

static int lcd_power_load_from_unifykey(struct aml_lcd_drv_s *pdrv,
					unsigned char *buf, int key_len, int len)
{
	struct lcd_power_ctrl_s *power_step = &pdrv->config.power;
	int i, j, temp;
	unsigned char *p;
	unsigned int index;
	int ret;

	/* power: (5byte * n) */
	p = buf + len;
	i = 0;
	while (i < LCD_PWR_STEP_MAX) {
		power_step->power_on_step_max = i;
		len += 5;
		ret = lcd_unifykey_len_check(key_len, len);
		if (ret < 0) {
			power_step->power_on_step[i].type = 0xff;
			power_step->power_on_step[i].index = 0;
			power_step->power_on_step[i].value = 0;
			power_step->power_on_step[i].delay = 0;
			LCDERR("[%d]: unifykey power_on length is incorrect\n", pdrv->index);
			return -1;
		}
		power_step->power_on_step[i].type =
			*(p + LCD_UKEY_PWR_TYPE + 5 * i);
		power_step->power_on_step[i].index =
			*(p + LCD_UKEY_PWR_INDEX + 5 * i);
		power_step->power_on_step[i].value =
			*(p + LCD_UKEY_PWR_VAL + 5 * i);
		power_step->power_on_step[i].delay =
			(*(p + LCD_UKEY_PWR_DELAY + 5 * i) |
			((*(p + LCD_UKEY_PWR_DELAY + 5 * i + 1)) << 8));

		/* gpio/extern probe */
		index = power_step->power_on_step[i].index;
		switch (power_step->power_on_step[i].type) {
		case LCD_POWER_TYPE_CPU:
		case LCD_POWER_TYPE_WAIT_GPIO:
			if (index < LCD_CPU_GPIO_NUM_MAX)
				lcd_cpu_gpio_probe(pdrv, index);
			break;
		case LCD_POWER_TYPE_EXTERN:
			lcd_extern_dev_index_add(pdrv->index, index);
			break;
		case LCD_POWER_TYPE_CLK_SS:
			temp = power_step->power_on_step[i].value;
			pdrv->config.timing.ss_freq = temp & 0xf;
			pdrv->config.timing.ss_mode = (temp >> 4) & 0xf;
			if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
				LCDPR("[%d]: clk_ss value=0x%x, final ss_level=0x%x\n",
				      pdrv->index, temp, pdrv->config.timing.ss_level);
			}
			break;
		default:
			break;
		}
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
			LCDPR("[%d]: %d: type=%d, index=%d, value=%d, delay=%d\n",
			      pdrv->index, i,
			      power_step->power_on_step[i].type,
			      power_step->power_on_step[i].index,
			      power_step->power_on_step[i].value,
			      power_step->power_on_step[i].delay);
		}
		if (power_step->power_on_step[i].type >= LCD_POWER_TYPE_MAX)
			break;
		i++;
	}

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: power_off step:\n", pdrv->index);
	p += (5 * (i + 1));
	j = 0;
	while (j < LCD_PWR_STEP_MAX) {
		power_step->power_off_step_max = j;
		len += 5;
		ret = lcd_unifykey_len_check(key_len, len);
		if (ret < 0) {
			power_step->power_off_step[j].type = 0xff;
			power_step->power_off_step[j].index = 0;
			power_step->power_off_step[j].value = 0;
			power_step->power_off_step[j].delay = 0;
			LCDERR("[%d]: unifykey power_off length is incorrect\n", pdrv->index);
			return -1;
		}
		power_step->power_off_step[j].type = *(p + LCD_UKEY_PWR_TYPE + 5 * j);
		power_step->power_off_step[j].index = *(p + LCD_UKEY_PWR_INDEX + 5 * j);
		power_step->power_off_step[j].value = *(p + LCD_UKEY_PWR_VAL + 5 * j);
		power_step->power_off_step[j].delay =
				(*(p + LCD_UKEY_PWR_DELAY + 5 * j) |
				((*(p + LCD_UKEY_PWR_DELAY + 5 * j + 1)) << 8));

		/* gpio/extern probe */
		index = power_step->power_off_step[j].index;
		switch (power_step->power_off_step[j].type) {
		case LCD_POWER_TYPE_CPU:
		case LCD_POWER_TYPE_WAIT_GPIO:
			if (index < LCD_CPU_GPIO_NUM_MAX)
				lcd_cpu_gpio_probe(pdrv, index);
			break;
		case LCD_POWER_TYPE_EXTERN:
			lcd_extern_dev_index_add(pdrv->index, index);
			break;
		default:
			break;
		}
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
			LCDPR("[%d]: %d: type=%d, index=%d, value=%d, delay=%d\n",
			      pdrv->index, j,
			      power_step->power_off_step[j].type,
			      power_step->power_off_step[j].index,
			      power_step->power_off_step[j].value,
			      power_step->power_off_step[j].delay);
		}
		if (power_step->power_off_step[j].type >= LCD_POWER_TYPE_MAX)
			break;
		j++;
	}

	return 0;
}

static int lcd_vlock_param_load_from_dts(struct aml_lcd_drv_s *pdrv, struct device_node *child)
{
	unsigned int para[4];
	int ret;

	pdrv->config.vlock_param[0] = LCD_VLOCK_PARAM_BIT_UPDATE;

	ret = of_property_read_u32_array(child, "vlock_attr", &para[0], 4);
	if (ret == 0) {
		LCDPR("[%d]: find vlock_attr\n", pdrv->index);
		pdrv->config.vlock_param[0] |= LCD_VLOCK_PARAM_BIT_VALID;
		pdrv->config.vlock_param[1] = para[0];
		pdrv->config.vlock_param[2] = para[1];
		pdrv->config.vlock_param[3] = para[2];
		pdrv->config.vlock_param[4] = para[3];
	}

	return 0;
}

static int lcd_vlock_param_load_from_unifykey(struct aml_lcd_drv_s *pdrv, unsigned char *buf)
{
	unsigned char *p;

	p = buf;

	pdrv->config.vlock_param[0] = LCD_VLOCK_PARAM_BIT_UPDATE;
	pdrv->config.vlock_param[1] = *(p + LCD_UKEY_VLOCK_VAL_0);
	pdrv->config.vlock_param[2] = *(p + LCD_UKEY_VLOCK_VAL_1);
	pdrv->config.vlock_param[3] = *(p + LCD_UKEY_VLOCK_VAL_2);
	pdrv->config.vlock_param[4] = *(p + LCD_UKEY_VLOCK_VAL_3);
	if (pdrv->config.vlock_param[1] ||
	    pdrv->config.vlock_param[2] ||
	    pdrv->config.vlock_param[3] ||
	    pdrv->config.vlock_param[4]) {
		LCDPR("[%d]: find vlock_attr\n", pdrv->index);
		pdrv->config.vlock_param[0] |= LCD_VLOCK_PARAM_BIT_VALID;
	}

	return 0;
}

static int lcd_optical_load_from_dts(struct aml_lcd_drv_s *pdrv, struct device_node *child)
{
	unsigned int para[13];
	int ret;

	ret = of_property_read_u32_array(child, "optical_attr", &para[0], 13);
	if (ret == 0) {
		LCDPR("[%d]: find optical_attr\n", pdrv->index);
		pdrv->config.optical.hdr_support = para[0];
		pdrv->config.optical.features = para[1];
		pdrv->config.optical.primaries_r_x = para[2];
		pdrv->config.optical.primaries_r_y = para[3];
		pdrv->config.optical.primaries_g_x = para[4];
		pdrv->config.optical.primaries_g_y = para[5];
		pdrv->config.optical.primaries_b_x = para[6];
		pdrv->config.optical.primaries_b_y = para[7];
		pdrv->config.optical.white_point_x = para[8];
		pdrv->config.optical.white_point_y = para[9];
		pdrv->config.optical.luma_max = para[10];
		pdrv->config.optical.luma_min = para[11];
		pdrv->config.optical.luma_avg = para[12];
	}
	ret = of_property_read_u32_array(child, "optical_adv_val", &para[0], 13);
	if (ret == 0) {
		LCDPR("[%d]: find optical_adv_val\n", pdrv->index);
		pdrv->config.optical.ldim_support = para[0];
		pdrv->config.optical.luma_peak = para[4];
	}

	lcd_optical_vinfo_update(pdrv);

	return 0;
}

static int lcd_optical_load_from_unifykey(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_optical_info_s *opt_info = &pdrv->config.optical;
	char key_str[15];
	unsigned char *para, *p;
	int key_len, len;
	int ret;

	memset(key_str, 0, 15);
	if (pdrv->index == 0)
		sprintf(key_str, "lcd_optical");
	else
		sprintf(key_str, "lcd%d_optical", pdrv->index);

	ret = lcd_unifykey_check(key_str);
	if (ret < 0)
		return -1;

	LCDPR("[%d]: %s: find ukey %s\n", pdrv->index, __func__, key_str);

	key_len = LCD_UKEY_OPTICAL_SIZE;
	para = kzalloc(key_len, GFP_KERNEL);
	if (!para)
		return -1;

	ret = lcd_unifykey_get(key_str, para, &key_len);
	if (ret < 0) {
		kfree(para);
		return -1;
	}

	/* step 1: check header */
	len = LCD_UKEY_HEAD_SIZE;
	ret = lcd_unifykey_len_check(key_len, len);
	if (ret < 0) {
		LCDERR("[%d]: %s unifykey header length is incorrect\n",
		       pdrv->index, key_str);
		kfree(para);
		return -1;
	}

	len = LCD_UKEY_OPTICAL_SIZE;

	/* step 2: check parameters */
	ret = lcd_unifykey_len_check(key_len, len);
	if (ret < 0) {
		LCDERR("[%d]: %s unifykey parameters length is incorrect\n",
		       pdrv->index, key_str);
		kfree(para);
		return -1;
	}

	/* attr (52Byte) */
	p = para;

	opt_info->hdr_support = (*(p + LCD_UKEY_OPT_HDR_SUPPORT) |
		((*(p + LCD_UKEY_OPT_HDR_SUPPORT + 1)) << 8) |
		((*(p + LCD_UKEY_OPT_HDR_SUPPORT + 2)) << 16) |
		((*(p + LCD_UKEY_OPT_HDR_SUPPORT + 3)) << 24));
	opt_info->features = (*(p + LCD_UKEY_OPT_FEATURES) |
		((*(p + LCD_UKEY_OPT_FEATURES + 1)) << 8) |
		((*(p + LCD_UKEY_OPT_FEATURES + 2)) << 16) |
		((*(p + LCD_UKEY_OPT_FEATURES + 3)) << 24));
	opt_info->primaries_r_x = (*(p + LCD_UKEY_OPT_PRI_R_X) |
		((*(p + LCD_UKEY_OPT_PRI_R_X + 1)) << 8) |
		((*(p + LCD_UKEY_OPT_PRI_R_X + 2)) << 16) |
		((*(p + LCD_UKEY_OPT_PRI_R_X + 3)) << 24));
	opt_info->primaries_r_y = (*(p + LCD_UKEY_OPT_PRI_R_Y) |
		((*(p + LCD_UKEY_OPT_PRI_R_Y + 1)) << 8) |
		((*(p + LCD_UKEY_OPT_PRI_R_Y + 2)) << 16) |
		((*(p + LCD_UKEY_OPT_PRI_R_Y + 3)) << 24));
	opt_info->primaries_g_x = (*(p + LCD_UKEY_OPT_PRI_G_X) |
		((*(p + LCD_UKEY_OPT_PRI_G_X + 1)) << 8) |
		((*(p + LCD_UKEY_OPT_PRI_G_X + 2)) << 16) |
		((*(p + LCD_UKEY_OPT_PRI_G_X + 3)) << 24));
	opt_info->primaries_g_y = (*(p + LCD_UKEY_OPT_PRI_G_Y) |
		((*(p + LCD_UKEY_OPT_PRI_G_Y + 1)) << 8) |
		((*(p + LCD_UKEY_OPT_PRI_G_Y + 2)) << 16) |
		((*(p + LCD_UKEY_OPT_PRI_G_Y + 3)) << 24));
	opt_info->primaries_b_x = (*(p + LCD_UKEY_OPT_PRI_B_X) |
		((*(p + LCD_UKEY_OPT_PRI_B_X + 1)) << 8) |
		((*(p + LCD_UKEY_OPT_PRI_B_X + 2)) << 16) |
		((*(p + LCD_UKEY_OPT_PRI_B_X + 3)) << 24));
	opt_info->primaries_b_y = (*(p + LCD_UKEY_OPT_PRI_B_Y) |
		((*(p + LCD_UKEY_OPT_PRI_B_Y + 1)) << 8) |
		((*(p + LCD_UKEY_OPT_PRI_B_Y + 2)) << 16) |
		((*(p + LCD_UKEY_OPT_PRI_B_Y + 3)) << 24));
	opt_info->white_point_x = (*(p + LCD_UKEY_OPT_WHITE_X) |
		((*(p + LCD_UKEY_OPT_WHITE_X + 1)) << 8) |
		((*(p + LCD_UKEY_OPT_WHITE_X + 2)) << 16) |
		((*(p + LCD_UKEY_OPT_WHITE_X + 3)) << 24));
	opt_info->white_point_y = (*(p + LCD_UKEY_OPT_WHITE_Y) |
		((*(p + LCD_UKEY_OPT_WHITE_Y + 1)) << 8) |
		((*(p + LCD_UKEY_OPT_WHITE_Y + 2)) << 16) |
		((*(p + LCD_UKEY_OPT_WHITE_Y + 3)) << 24));
	opt_info->luma_max = (*(p + LCD_UKEY_OPT_LUMA_MAX) |
		((*(p + LCD_UKEY_OPT_LUMA_MAX + 1)) << 8) |
		((*(p + LCD_UKEY_OPT_LUMA_MAX + 2)) << 16) |
		((*(p + LCD_UKEY_OPT_LUMA_MAX + 3)) << 24));
	opt_info->luma_min = (*(p + LCD_UKEY_OPT_LUMA_MIN) |
		((*(p + LCD_UKEY_OPT_LUMA_MIN + 1)) << 8) |
		((*(p + LCD_UKEY_OPT_LUMA_MIN + 2)) << 16) |
		((*(p + LCD_UKEY_OPT_LUMA_MIN + 3)) << 24));
	opt_info->luma_avg = (*(p + LCD_UKEY_OPT_LUMA_AVG) |
		((*(p + LCD_UKEY_OPT_LUMA_AVG + 1)) << 8) |
		((*(p + LCD_UKEY_OPT_LUMA_AVG + 2)) << 16) |
		((*(p + LCD_UKEY_OPT_LUMA_AVG + 3)) << 24));

	opt_info->ldim_support = *(p + LCD_UKEY_OPT_ADV_FLAG0);
	opt_info->luma_peak = *(unsigned int *)(p + LCD_UKEY_OPT_ADV_VAL1);

	kfree(para);

	lcd_optical_vinfo_update(pdrv);

	return 0;
}

static int lcd_config_load_from_dts(struct aml_lcd_drv_s *pdrv)
{
	struct device_node *child;
	struct lcd_config_s *pconf = &pdrv->config;
	union lcd_ctrl_config_u *pctrl = &pdrv->config.control;
	struct phy_config_s *phy_cfg = &pdrv->config.phy_cfg;
	unsigned int para[10], val;
	const char *str;
	int i, ret = 0;

	if (!pdrv->dev->of_node) {
		LCDERR("[%d]: dev of_node is null\n", pdrv->index);
		return -1;
	}

	child = of_get_child_by_name(pdrv->dev->of_node, pconf->propname);
	if (!child) {
		LCDERR("[%d]: failed to get %s\n",
		       pdrv->index, pconf->propname);
		return -1;
	}

	ret = of_property_read_string(child, "model_name", &str);
	if (ret) {
		LCDERR("[%d]: failed to get model_name\n", pdrv->index);
		strncpy(pconf->basic.model_name, pconf->propname,
			MOD_LEN_MAX);
	} else {
		strncpy(pconf->basic.model_name, str, MOD_LEN_MAX);
	}
	/* ensure string ending */
	pconf->basic.model_name[MOD_LEN_MAX - 1] = '\0';

	ret = of_property_read_string(child, "interface", &str);
	if (ret) {
		LCDERR("[%d]: failed to get interface\n", pdrv->index);
		str = "invalid";
	}
	pconf->basic.lcd_type = lcd_type_str_to_type(str);

	ret = of_property_read_u32_array(child, "basic_setting", &para[0], 7);
	if (ret) {
		LCDERR("[%d]: failed to get basic_setting\n", pdrv->index);
		return -1;
	}
	pconf->basic.h_active = para[0];
	pconf->basic.v_active = para[1];
	pconf->basic.h_period = para[2];
	pconf->basic.v_period = para[3];
	pconf->basic.lcd_bits = para[4];
	pconf->basic.screen_width = para[5];
	pconf->basic.screen_height = para[6];

	ret = of_property_read_u32_array(child, "range_setting", &para[0], 6);
	if (ret) {
		LCDPR("[%d]: no range_setting\n", pdrv->index);
		pconf->basic.h_period_min = pconf->basic.h_period;
		pconf->basic.h_period_max = pconf->basic.h_period;
		pconf->basic.v_period_min = pconf->basic.v_period;
		pconf->basic.v_period_max = pconf->basic.v_period;
		pconf->basic.lcd_clk_min = 0;
		pconf->basic.lcd_clk_max = 0;
	} else {
		pconf->basic.h_period_min = para[0];
		pconf->basic.h_period_max = para[1];
		pconf->basic.v_period_min = para[2];
		pconf->basic.v_period_max = para[3];
		pconf->basic.lcd_clk_min = para[4];
		pconf->basic.lcd_clk_max = para[5];
	}

	ret = of_property_read_u32_array(child, "range_frame_rate", &para[0], 6);
	if (ret) {
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
			LCDPR("[%d]: no range_frame_rate\n", pdrv->index);
		pconf->basic.frame_rate_min = 0;
		pconf->basic.frame_rate_max = 0;
	} else {
		pconf->basic.frame_rate_min = para[0];
		pconf->basic.frame_rate_max = para[1];
	}

	ret = of_property_read_u32_array(child, "lcd_timing", &para[0], 6);
	if (ret) {
		LCDERR("[%d]: failed to get lcd_timing\n", pdrv->index);
		return -1;
	}
	pconf->timing.hsync_width = (unsigned short)(para[0]);
	pconf->timing.hsync_bp = (unsigned short)(para[1]);
	pconf->timing.hsync_pol = (unsigned short)(para[2]);
	pconf->timing.vsync_width = (unsigned short)(para[3]);
	pconf->timing.vsync_bp = (unsigned short)(para[4]);
	pconf->timing.vsync_pol = (unsigned short)(para[5]);

	ret = of_property_read_u32_array(child, "clk_attr", &para[0], 4);
	if (ret) {
		LCDERR("[%d]: failed to get clk_attr\n", pdrv->index);
		pconf->timing.fr_adjust_type = 0;
		pconf->timing.ss_level = 0;
		pconf->timing.ss_freq = 0;
		pconf->timing.ss_mode = 0;
		pconf->timing.clk_auto = 1;
		pconf->timing.lcd_clk = 60;
	} else {
		pconf->timing.fr_adjust_type = (unsigned char)(para[0]);
		pconf->timing.ss_level = para[1] & 0xff;
		pconf->timing.ss_freq = (para[1] >> 8) & 0xf;
		pconf->timing.ss_mode = (para[1] >> 12) & 0xf;
		pconf->timing.clk_auto = (unsigned char)(para[2]);
		pconf->timing.lcd_clk = para[3];
		if (pconf->timing.lcd_clk == 0) { /* avoid 0 mistake */
			pconf->timing.lcd_clk = 60;
			if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
				LCDPR("[%d]: lcd_clk is 0, default to 60Hz\n", pdrv->index);
		}
	}

	ret = of_property_read_u32(child, "custom_pinmux", &val);
	if (ret) {
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
			LCDPR("[%d]: failed to get custom_pinmux\n", pdrv->index);
		ret = of_property_read_u32(child, "customer_pinmux", &val);
		if (ret) {
			pconf->custom_pinmux = 0;
			if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
				LCDPR("[%d]: failed to get customer_pinmux\n", pdrv->index);
		} else {
			pconf->custom_pinmux = val;
			LCDPR("[%d]: find custom_pinmux: %d\n",
				pdrv->index, pconf->custom_pinmux);
		}
	} else {
		pconf->custom_pinmux = val;
		LCDPR("[%d]: find custom_pinmux: %d\n",
			pdrv->index, pconf->custom_pinmux);
	}

	ret = of_property_read_u32(child, "fr_auto_disable", &val);
	if (ret) {
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
			LCDPR("[%d]: failed to get fr_auto_disable\n", pdrv->index);
	} else {
		pconf->fr_auto_dis = val;
		LCDPR("[%d]: find fr_auto_dis: %d\n",
			pdrv->index, pconf->fr_auto_dis);
	}

	switch (pconf->basic.lcd_type) {
	case LCD_TTL:
		ret = of_property_read_u32_array(child, "ttl_attr", &para[0], 5);
		if (ret) {
			LCDERR("[%d]: failed to get ttl_attr\n", pdrv->index);
			return -1;
		}
		pctrl->ttl_cfg.clk_pol = para[0];
		pctrl->ttl_cfg.sync_valid =
			((para[1] << 1) | (para[2] << 0));
		pctrl->ttl_cfg.swap_ctrl =
			((para[3] << 1) | (para[4] << 0));
		break;
	case LCD_LVDS:
		ret = of_property_read_u32_array(child, "lvds_attr", &para[0], 5);
		if (ret) {
			LCDPR("[%d]: failed to get lvds_attr\n", pdrv->index);
			return -1;
		}
		pctrl->lvds_cfg.lvds_repack = para[0];
		pctrl->lvds_cfg.dual_port = para[1];
		pctrl->lvds_cfg.pn_swap = para[2];
		pctrl->lvds_cfg.port_swap = para[3];
		pctrl->lvds_cfg.lane_reverse = para[4];

		ret = of_property_read_u32_array(child, "phy_attr", &para[0], 2);
		if (ret) {
			LCDPR("[%d]: failed to get phy_attr\n", pdrv->index);
			pctrl->lvds_cfg.phy_vswing = 0x5;
			pctrl->lvds_cfg.phy_preem  = 0x1;
		} else {
			pctrl->lvds_cfg.phy_vswing = para[0];
			pctrl->lvds_cfg.phy_preem = para[1];
			if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
				LCDPR("[%d]: phy vswing_level=0x%x, preem_level=0x%x\n",
				      pdrv->index,
				      pctrl->lvds_cfg.phy_vswing,
				      pctrl->lvds_cfg.phy_preem);
			}
		}
		phy_cfg->lane_num = 12;
		phy_cfg->vswing_level = pctrl->lvds_cfg.phy_vswing & 0xf;
		phy_cfg->ext_pullup = (pctrl->lvds_cfg.phy_vswing >> 4) & 0x3;
		phy_cfg->vswing = lcd_phy_vswing_level_to_value(pdrv, phy_cfg->vswing_level);
		phy_cfg->preem_level = pctrl->lvds_cfg.phy_preem;
		val = lcd_phy_preem_level_to_value(pdrv, phy_cfg->preem_level);
		for (i = 0; i < phy_cfg->lane_num; i++) {
			phy_cfg->lane[i].amp = 0;
			phy_cfg->lane[i].preem = val;
		}
		break;
	case LCD_VBYONE:
		ret = of_property_read_u32_array(child, "vbyone_attr", &para[0], 4);
		if (ret) {
			LCDERR("[%d]: failed to get vbyone_attr\n", pdrv->index);
			return -1;
		}
		pctrl->vbyone_cfg.lane_count = para[0];
		pctrl->vbyone_cfg.region_num = para[1];
		pctrl->vbyone_cfg.byte_mode = para[2];
		pctrl->vbyone_cfg.color_fmt = para[3];

		ret = of_property_read_u32_array(child, "vbyone_intr_enable", &para[0], 2);
		if (ret) {
			if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
				LCDERR("[%d]: failed to get vbyone_intr_enable\n", pdrv->index);
		} else {
			pctrl->vbyone_cfg.intr_en = para[0];
			pctrl->vbyone_cfg.vsync_intr_en = para[1];
		}
		ret = of_property_read_u32_array(child, "phy_attr", &para[0], 2);
		if (ret) {
			LCDPR("[%d]: failed to get phy_attr\n", pdrv->index);
			pctrl->vbyone_cfg.phy_vswing = 0x5;
			pctrl->vbyone_cfg.phy_preem  = 0x1;
		} else {
			pctrl->vbyone_cfg.phy_vswing = para[0];
			pctrl->vbyone_cfg.phy_preem = para[1];
			if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
				LCDPR("[%d]: phy vswing_level=0x%x, preem_level=0x%x\n",
				      pdrv->index,
				      pctrl->vbyone_cfg.phy_vswing,
				      pctrl->vbyone_cfg.phy_preem);
			}
		}
		phy_cfg->lane_num = 8;
		phy_cfg->vswing_level = pctrl->vbyone_cfg.phy_vswing & 0xf;
		phy_cfg->ext_pullup = (pctrl->vbyone_cfg.phy_vswing >> 4) & 0x3;
		phy_cfg->vswing = lcd_phy_vswing_level_to_value(pdrv, phy_cfg->vswing_level);
		phy_cfg->preem_level = pctrl->vbyone_cfg.phy_preem;
		val = lcd_phy_preem_level_to_value(pdrv, phy_cfg->preem_level);
		for (i = 0; i < phy_cfg->lane_num; i++) {
			phy_cfg->lane[i].amp = 0;
			phy_cfg->lane[i].preem = val;
		}

		ret = of_property_read_u32(child, "vbyone_ctrl_flag", &val);
		if (ret) {
			if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
				LCDPR("[%d]: failed to get vbyone_ctrl_flag\n", pdrv->index);
		} else {
			pctrl->vbyone_cfg.ctrl_flag = val;
			LCDPR("[%d]: vbyone ctrl_flag=0x%x\n",
			      pdrv->index, pctrl->vbyone_cfg.ctrl_flag);
		}
		if (pctrl->vbyone_cfg.ctrl_flag & 0x7) {
			ret = of_property_read_u32_array(child, "vbyone_ctrl_timing", &para[0], 3);
			if (ret) {
				LCDPR("[%d]: failed to get vbyone_ctrl_timing\n", pdrv->index);
			} else {
				pctrl->vbyone_cfg.power_on_reset_delay = para[0];
				pctrl->vbyone_cfg.hpd_data_delay = para[1];
				pctrl->vbyone_cfg.cdr_training_hold = para[2];
			}
			if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
				LCDPR("[%d]: power_on_reset_delay: %d\n",
				      pdrv->index,
				      pctrl->vbyone_cfg.power_on_reset_delay);
				LCDPR("[%d]: hpd_data_delay: %d\n",
				      pdrv->index,
				      pctrl->vbyone_cfg.hpd_data_delay);
				LCDPR("[%d]: cdr_training_hold: %d\n",
				      pdrv->index,
				      pctrl->vbyone_cfg.cdr_training_hold);
			}
		}
		ret = of_property_read_u32_array(child, "hw_filter", &para[0], 2);
		if (ret) {
			if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
				LCDPR("[%d]: failed to get hw_filter\n", pdrv->index);
		} else {
			pctrl->vbyone_cfg.hw_filter_time = para[0];
			pctrl->vbyone_cfg.hw_filter_cnt = para[1];
			if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
				LCDPR("[%d]: vbyone hw_filter=0x%x 0x%x\n",
				      pdrv->index,
				      pctrl->vbyone_cfg.hw_filter_time,
				      pctrl->vbyone_cfg.hw_filter_cnt);
			}
		}
		break;
	case LCD_MLVDS:
		ret = of_property_read_u32_array(child, "minilvds_attr", &para[0], 6);
		if (ret) {
			LCDERR("[%d]: failed to get minilvds_attr\n", pdrv->index);
			return -1;
		}
		pctrl->mlvds_cfg.channel_num = para[0];
		pctrl->mlvds_cfg.channel_sel0 = para[1];
		pctrl->mlvds_cfg.channel_sel1 = para[2];
		pctrl->mlvds_cfg.clk_phase = para[3];
		pctrl->mlvds_cfg.pn_swap = para[4];
		pctrl->mlvds_cfg.bit_swap = para[5];

		ret = of_property_read_u32_array(child, "phy_attr", &para[0], 2);
		if (ret) {
			LCDPR("[%d]: failed to get phy_attr\n", pdrv->index);
			pctrl->mlvds_cfg.phy_vswing = 0x5;
			pctrl->mlvds_cfg.phy_preem  = 0x1;
		} else {
			pctrl->mlvds_cfg.phy_vswing = para[0];
			pctrl->mlvds_cfg.phy_preem = para[1];
			if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
				LCDPR("[%d]: phy vswing_level=0x%x, preem_level=0x%x\n",
				      pdrv->index,
				      pctrl->mlvds_cfg.phy_vswing,
				      pctrl->mlvds_cfg.phy_preem);
			}
		}
		phy_cfg->lane_num = 12;
		phy_cfg->vswing_level = pctrl->mlvds_cfg.phy_vswing & 0xf;
		phy_cfg->ext_pullup = (pctrl->mlvds_cfg.phy_vswing >> 4) & 0x3;
		phy_cfg->vswing = lcd_phy_vswing_level_to_value(pdrv, phy_cfg->vswing_level);
		phy_cfg->preem_level = pctrl->mlvds_cfg.phy_preem;
		val = lcd_phy_preem_level_to_value(pdrv, phy_cfg->preem_level);
		for (i = 0; i < phy_cfg->lane_num; i++) {
			phy_cfg->lane[i].amp = 0;
			phy_cfg->lane[i].preem = val;
		}
		break;
	case LCD_P2P:
		ret = of_property_read_u32_array(child, "p2p_attr", &para[0], 6);
		if (ret) {
			LCDERR("[%d]: failed to get p2p_attr\n", pdrv->index);
			return -1;
		}
		pctrl->p2p_cfg.p2p_type = para[0];
		pctrl->p2p_cfg.lane_num = para[1];
		pctrl->p2p_cfg.channel_sel0 = para[2];
		pctrl->p2p_cfg.channel_sel1 = para[3];
		pctrl->p2p_cfg.pn_swap = para[4];
		pctrl->p2p_cfg.bit_swap = para[5];

		ret = of_property_read_u32_array(child, "phy_attr", &para[0], 2);
		if (ret) {
			LCDPR("[%d]: failed to get phy_attr\n", pdrv->index);
			pctrl->p2p_cfg.phy_vswing = 0x5;
			pctrl->p2p_cfg.phy_preem  = 0x1;
		} else {
			pctrl->p2p_cfg.phy_vswing = para[0];
			pctrl->p2p_cfg.phy_preem = para[1];
			if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
				LCDPR("[%d]: phy vswing_level=0x%x, preem_level=0x%x\n",
				      pdrv->index,
				      pctrl->p2p_cfg.phy_vswing,
				      pctrl->p2p_cfg.phy_preem);
			}
		}
		phy_cfg->lane_num = 12;
		phy_cfg->vswing_level = pctrl->p2p_cfg.phy_vswing & 0xf;
		phy_cfg->ext_pullup = (pctrl->p2p_cfg.phy_vswing >> 4) & 0x3;
		phy_cfg->vswing = lcd_phy_vswing_level_to_value(pdrv, phy_cfg->vswing_level);
		phy_cfg->preem_level = pctrl->p2p_cfg.phy_preem;
		val = lcd_phy_preem_level_to_value(pdrv, phy_cfg->preem_level);
		for (i = 0; i < phy_cfg->lane_num; i++) {
			phy_cfg->lane[i].amp = 0;
			phy_cfg->lane[i].preem = val;
		}
		break;
	case LCD_MIPI:
		ret = of_property_read_u32_array(child, "mipi_attr", &para[0], 8);
		if (ret) {
			LCDERR("[%d]: failed to get mipi_attr\n", pdrv->index);
			return -1;
		}
		pctrl->mipi_cfg.lane_num = para[0];
		pctrl->mipi_cfg.bit_rate_max = para[1];
		pctrl->mipi_cfg.factor_numerator = para[2];
		pctrl->mipi_cfg.factor_denominator = 100;
		pctrl->mipi_cfg.operation_mode_init = para[3];
		pctrl->mipi_cfg.operation_mode_display = para[4];
		pctrl->mipi_cfg.video_mode_type = para[5];
		pctrl->mipi_cfg.clk_always_hs = para[6];
		pctrl->mipi_cfg.phy_switch = para[7];

#ifdef CONFIG_AMLOGIC_LCD_TABLET
		lcd_mipi_dsi_init_table_detect(pdrv, child, 1);
		lcd_mipi_dsi_init_table_detect(pdrv, child, 0);
#endif
		ret = of_property_read_u32_array(child, "extern_init", &para[0], 1);
		if (ret) {
			LCDPR("[%d]: failed to get extern_init\n", pdrv->index);
		} else {
			pctrl->mipi_cfg.extern_init = para[0];
			lcd_extern_dev_index_add(pdrv->index, para[0]);
		}
#ifdef CONFIG_AMLOGIC_LCD_TABLET
		mipi_dsi_config_init(pdrv);
#endif
		break;
	case LCD_EDP:
		ret = of_property_read_u32_array(child, "edp_attr", &para[0], 9);
		if (ret) {
			LCDERR("[%d]: failed to get edp_attr\n", pdrv->index);
			return -1;
		}
		pctrl->edp_cfg.max_lane_count = (unsigned char)para[0];
		pctrl->edp_cfg.max_link_rate = (unsigned char)para[1];
		pctrl->edp_cfg.training_mode = (unsigned char)para[2];
		pctrl->edp_cfg.edid_en = (unsigned char)para[3];
		pctrl->edp_cfg.dpcd_caps_en = (unsigned char)para[4];
		pctrl->edp_cfg.sync_clk_mode = (unsigned char)para[5];
		pctrl->edp_cfg.scramb_mode = (unsigned char)para[6];
		pctrl->edp_cfg.enhanced_framing_en = (unsigned char)para[7];
		pctrl->edp_cfg.pn_swap = (unsigned char)para[8];

		pctrl->edp_cfg.lane_count = pctrl->edp_cfg.max_lane_count;
		pctrl->edp_cfg.link_rate = pctrl->edp_cfg.max_link_rate;

		ret = of_property_read_u32_array(child, "phy_attr", &para[0], 2);
		if (ret) {
			LCDPR("[%d]: failed to get phy_attr\n", pdrv->index);
			pctrl->edp_cfg.phy_vswing = 0x5;
			pctrl->edp_cfg.phy_preem  = 0x1;
		} else {
			pctrl->edp_cfg.phy_vswing = para[0];
			pctrl->edp_cfg.phy_preem = para[1];
			if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
				LCDPR("[%d]: phy vswing_level=0x%x, preem_level=0x%x\n",
				      pdrv->index,
				      pctrl->edp_cfg.phy_vswing,
				      pctrl->edp_cfg.phy_preem);
			}
		}
		phy_cfg->lane_num = 4;
		phy_cfg->vswing_level = pctrl->edp_cfg.phy_vswing & 0xf;
		phy_cfg->preem_level = pctrl->edp_cfg.phy_preem;
		break;
	default:
		LCDERR("[%d]: invalid lcd type\n", pdrv->index);
		break;
	}

	lcd_vlock_param_load_from_dts(pdrv, child);
	ret = lcd_power_load_from_dts(pdrv, child);

	lcd_optical_load_from_dts(pdrv, child);

	ret = of_property_read_u32(child, "backlight_index", &para[0]);
	if (ret) {
		LCDPR("[%d]: failed to get backlight_index\n", pdrv->index);
		pconf->backlight_index = 0xff;
	} else {
		pconf->backlight_index = para[0];
#ifdef CONFIG_AMLOGIC_BACKLIGHT
		aml_bl_index_add(pdrv->index, pconf->backlight_index);
#endif
	}

	return ret;
}

static int lcd_config_load_from_unifykey_v2(struct lcd_config_s *pconf,
					    unsigned char *p,
					    unsigned int key_len,
					    unsigned int offset)
{
	struct aml_lcd_unifykey_header_s *lcd_header;
	struct phy_config_s *phy_cfg = &pconf->phy_cfg;
	struct cus_ctrl_config_s *cus_ctrl = &pconf->cus_ctrl;
	unsigned int temp, len;
	int i, ret;

	lcd_header = (struct aml_lcd_unifykey_header_s *)p;
	LCDPR("unifykey version: 0x%04x\n", lcd_header->version);
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
		LCDPR("unifykey header:\n");
		LCDPR("crc32             = 0x%08x\n", lcd_header->crc32);
		LCDPR("data_len          = %d\n", lcd_header->data_len);
		LCDPR("block_next_flag   = %d\n", lcd_header->block_next_flag);
		LCDPR("block_cur_size    = %d\n", lcd_header->block_cur_size);
	}

	/* step 2: check lcd parameters */
	len = offset + lcd_header->block_cur_size;
	ret = lcd_unifykey_len_check(key_len, len);
	if (ret < 0) {
		LCDERR("unifykey parameters length is incorrect\n");
		return -1;
	}

	/*phy 356byte*/
	phy_cfg->flag = (*(p + LCD_UKEY_PHY_ATTR_FLAG) |
		((*(p + LCD_UKEY_PHY_ATTR_FLAG + 1)) << 8) |
		((*(p + LCD_UKEY_PHY_ATTR_FLAG + 2)) << 16) |
		((*(p + LCD_UKEY_PHY_ATTR_FLAG + 3)) << 24));
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("%s: ctrl_flag=0x%x\n", __func__, phy_cfg->flag);

	phy_cfg->vcm = (*(p + LCD_UKEY_PHY_ATTR_1) |
			*(p + LCD_UKEY_PHY_ATTR_1 + 1) << 8);
	phy_cfg->ref_bias = (*(p + LCD_UKEY_PHY_ATTR_2) |
			*(p + LCD_UKEY_PHY_ATTR_2 + 1) << 8);
	phy_cfg->odt = (*(p + LCD_UKEY_PHY_ATTR_3) |
			*(p + LCD_UKEY_PHY_ATTR_3 + 1) << 8);
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
		LCDPR("%s: vcm=0x%x, ref_bias=0x%x, odt=0x%x\n",
		      __func__, phy_cfg->vcm, phy_cfg->ref_bias,
		      phy_cfg->odt);
	}

	if (phy_cfg->flag & (1 << 12)) {
		for (i = 0; i < phy_cfg->lane_num; i++) {
			phy_cfg->lane[i].preem =
				*(p + LCD_UKEY_PHY_LANE_CTRL + 4 * i) |
				(*(p + LCD_UKEY_PHY_LANE_CTRL + 4 * i + 1) << 8);
			phy_cfg->lane[i].amp =
				*(p + LCD_UKEY_PHY_LANE_CTRL + 4 * i + 2) |
				(*(p + LCD_UKEY_PHY_LANE_CTRL + 4 * i + 3) << 8);
			if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
				LCDPR("%s: lane[%d]: preem=0x%x, amp=0x%x\n",
				      __func__, i,
				      phy_cfg->lane[i].preem,
				      phy_cfg->lane[i].amp);
			}
		}
	}

	/* cus_ctrl_config */
	cus_ctrl->flag = (*(p + LCD_UKEY_CUS_CTRL_ATTR_FLAG) |
		((*(p + LCD_UKEY_CUS_CTRL_ATTR_FLAG + 1)) << 8) |
		((*(p + LCD_UKEY_CUS_CTRL_ATTR_FLAG + 2)) << 16) |
		((*(p + LCD_UKEY_CUS_CTRL_ATTR_FLAG + 3)) << 24));
	if (lcd_debug_print_flag)
		LCDPR("%s: cus_ctrl_flag=0x%x\n", __func__, cus_ctrl->flag);

	if (cus_ctrl->flag & 0x1) {
		temp = (*(p + LCD_UKEY_CUS_CTRL_ATTR_0) |
			*(p + LCD_UKEY_CUS_CTRL_ATTR_0 + 1) << 8);
		cus_ctrl->dlg_flag = temp & 0xf;
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
			LCDPR("%s: cus_ctrl dlg_flag=%d\n",
			      __func__, cus_ctrl->dlg_flag);
		}
		temp = (*(p + LCD_UKEY_CUS_CTRL_ATTR_0_PARM0) |
			*(p + LCD_UKEY_CUS_CTRL_ATTR_0_PARM0 + 1) << 8);
		cus_ctrl->attr_0_para0 = temp;
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
			LCDPR("%s: cus_ctrl attr_0_para0=%d\n",
			      __func__, cus_ctrl->attr_0_para0);
		}
		temp = (*(p + LCD_UKEY_CUS_CTRL_ATTR_0_PARM1) |
			*(p + LCD_UKEY_CUS_CTRL_ATTR_0_PARM1 + 1) << 8);
		cus_ctrl->attr_0_para1 = temp;
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
			LCDPR("%s: cus_ctrl attr_0_para1=%d\n",
			      __func__, cus_ctrl->attr_0_para1);
		}
	}

	return 0;
}

static int lcd_config_load_from_unifykey(struct aml_lcd_drv_s *pdrv, char *key_str)
{
	unsigned char *para;
	int key_len, len;
	unsigned char *p;
	const char *str;
	struct aml_lcd_unifykey_header_s lcd_header;
	struct lcd_config_s *pconf = &pdrv->config;
	struct phy_config_s *phy_cfg = &pdrv->config.phy_cfg;
	union lcd_ctrl_config_u *pctrl = &pdrv->config.control;
	unsigned int temp;
	int ret, i = 0;

	key_len = LCD_UKEY_LCD_SIZE;
	para = kzalloc(key_len, GFP_KERNEL);
	if (!para)
		return -1;

	ret = lcd_unifykey_get(key_str, para, &key_len);
	if (ret < 0) {
		kfree(para);
		return -1;
	}

	/* step 1: check header */
	len = LCD_UKEY_HEAD_SIZE;
	ret = lcd_unifykey_len_check(key_len, len);
	if (ret < 0) {
		LCDERR("[%d]: unifykey header length is incorrect\n", pdrv->index);
		kfree(para);
		return -1;
	}

	lcd_unifykey_header_check(para, &lcd_header);
	LCDPR("[%d]: unifykey version: 0x%04x\n",
	      pdrv->index, lcd_header.version);
	len = LCD_UKEY_DATA_LEN_V1; /*10+36+18+31+20*/
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
		LCDPR("[%d]: unifykey header:\n", pdrv->index);
		LCDPR("crc32             = 0x%08x\n", lcd_header.crc32);
		LCDPR("data_len          = %d\n", lcd_header.data_len);
		LCDPR("block_next_flag   = %d\n", lcd_header.block_next_flag);
		LCDPR("block_cur_size    = 0x%04x\n", lcd_header.block_cur_size);
	}

	/* step 2: check lcd parameters */
	ret = lcd_unifykey_len_check(key_len, len);
	if (ret < 0) {
		LCDERR("[%d]: unifykey parameters length is incorrect\n",
		       pdrv->index);
		kfree(para);
		return -1;
	}

	/* panel_type update */
	sprintf(pconf->propname, "%s", "unifykey");

	/* basic: 36byte */
	p = para;
	str = (const char *)(p + LCD_UKEY_HEAD_SIZE);
	strncpy(pconf->basic.model_name, str, MOD_LEN_MAX);
	/* ensure string ending */
	pconf->basic.model_name[MOD_LEN_MAX - 1] = '\0';
	pconf->basic.lcd_type = *(p + LCD_UKEY_INTERFACE);
	pconf->basic.lcd_bits = *(p + LCD_UKEY_LCD_BITS);
	pconf->basic.screen_width = (*(p + LCD_UKEY_SCREEN_WIDTH) |
		((*(p + LCD_UKEY_SCREEN_WIDTH + 1)) << 8));
	pconf->basic.screen_height = (*(p + LCD_UKEY_SCREEN_HEIGHT) |
		((*(p + LCD_UKEY_SCREEN_HEIGHT + 1)) << 8));

	/* timing: 18byte */
	pconf->basic.h_active = (*(p + LCD_UKEY_H_ACTIVE) |
		((*(p + LCD_UKEY_H_ACTIVE + 1)) << 8));
	pconf->basic.v_active = (*(p + LCD_UKEY_V_ACTIVE)) |
		((*(p + LCD_UKEY_V_ACTIVE + 1)) << 8);
	pconf->basic.h_period = (*(p + LCD_UKEY_H_PERIOD)) |
		((*(p + LCD_UKEY_H_PERIOD + 1)) << 8);
	pconf->basic.v_period = (*(p + LCD_UKEY_V_PERIOD)) |
		((*(p + LCD_UKEY_V_PERIOD + 1)) << 8);
	pconf->timing.hsync_width = (*(p + LCD_UKEY_HS_WIDTH) |
		((*(p + LCD_UKEY_HS_WIDTH + 1)) << 8));
	pconf->timing.hsync_bp = (*(p + LCD_UKEY_HS_BP) |
		((*(p + LCD_UKEY_HS_BP + 1)) << 8));
	pconf->timing.hsync_pol = *(p + LCD_UKEY_HS_POL);
	pconf->timing.vsync_width = (*(p + LCD_UKEY_VS_WIDTH) |
		((*(p + LCD_UKEY_VS_WIDTH + 1)) << 8));
	pconf->timing.vsync_bp = (*(p + LCD_UKEY_VS_BP) |
		((*(p + LCD_UKEY_VS_BP + 1)) << 8));
	pconf->timing.vsync_pol = *(p + LCD_UKEY_VS_POL);

	/* customer: 31byte */
	pconf->timing.fr_adjust_type = *(p + LCD_UKEY_FR_ADJ_TYPE);
	pconf->timing.ss_level = *(p + LCD_UKEY_SS_LEVEL);
	pconf->timing.clk_auto = *(p + LCD_UKEY_CLK_AUTO_GEN);
	pconf->timing.lcd_clk = (*(p + LCD_UKEY_PCLK) |
		((*(p + LCD_UKEY_PCLK + 1)) << 8) |
		((*(p + LCD_UKEY_PCLK + 2)) << 16) |
		((*(p + LCD_UKEY_PCLK + 3)) << 24));
	if (pconf->timing.lcd_clk == 0) { /* avoid 0 mistake */
		pconf->timing.lcd_clk = 60;
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
			LCDPR("[%d]: lcd_clk is 0, default to 60Hz\n", pdrv->index);
	}
	pconf->basic.h_period_min = (*(p + LCD_UKEY_H_PERIOD_MIN) |
		((*(p + LCD_UKEY_H_PERIOD_MIN + 1)) << 8));
	pconf->basic.h_period_max = (*(p + LCD_UKEY_H_PERIOD_MAX) |
		((*(p + LCD_UKEY_H_PERIOD_MAX + 1)) << 8));
	pconf->basic.v_period_min = (*(p + LCD_UKEY_V_PERIOD_MIN) |
		((*(p + LCD_UKEY_V_PERIOD_MIN + 1)) << 8));
	pconf->basic.v_period_max = (*(p + LCD_UKEY_V_PERIOD_MAX) |
		((*(p + LCD_UKEY_V_PERIOD_MAX + 1)) << 8));
	pconf->basic.lcd_clk_min = (*(p + LCD_UKEY_PCLK_MIN) |
		((*(p + LCD_UKEY_PCLK_MIN + 1)) << 8) |
		((*(p + LCD_UKEY_PCLK_MIN + 2)) << 16) |
		((*(p + LCD_UKEY_PCLK_MIN + 3)) << 24));
	pconf->basic.lcd_clk_max = (*(p + LCD_UKEY_PCLK_MAX) |
		((*(p + LCD_UKEY_PCLK_MAX + 1)) << 8) |
		((*(p + LCD_UKEY_PCLK_MAX + 2)) << 16) |
		((*(p + LCD_UKEY_PCLK_MAX + 3)) << 24));
	pconf->basic.frame_rate_min = *(p + LCD_UKEY_FRAME_RATE_MIN);
	pconf->basic.frame_rate_max = *(p + LCD_UKEY_FRAME_RATE_MAX);

	pconf->custom_pinmux = *(p + LCD_UKEY_CUST_PINMUX);
	pconf->fr_auto_dis = *(p + LCD_UKEY_FR_AUTO_DIS);

	/* interface: 20byte */
	switch (pconf->basic.lcd_type) {
	case LCD_LVDS:
		pctrl->lvds_cfg.lvds_repack = *(p + LCD_UKEY_IF_ATTR_0) |
			((*(p + LCD_UKEY_IF_ATTR_0 + 1)) << 8);
		pctrl->lvds_cfg.dual_port = *(p + LCD_UKEY_IF_ATTR_1) |
			((*(p + LCD_UKEY_IF_ATTR_1 + 1)) << 8);
		pctrl->lvds_cfg.pn_swap = *(p + LCD_UKEY_IF_ATTR_2) |
			((*(p + LCD_UKEY_IF_ATTR_2 + 1)) << 8);
		pctrl->lvds_cfg.port_swap = *(p + LCD_UKEY_IF_ATTR_3) |
			((*(p + LCD_UKEY_IF_ATTR_3 + 1)) << 8);
		pctrl->lvds_cfg.phy_vswing = *(p + LCD_UKEY_IF_ATTR_4) |
			((*(p + LCD_UKEY_IF_ATTR_4 + 1)) << 8);
		pctrl->lvds_cfg.phy_preem = *(p + LCD_UKEY_IF_ATTR_5) |
			((*(p + LCD_UKEY_IF_ATTR_5 + 1)) << 8);
		pctrl->lvds_cfg.lane_reverse = *(p + LCD_UKEY_IF_ATTR_8) |
			((*(p + LCD_UKEY_IF_ATTR_8 + 1)) << 8);

		phy_cfg->lane_num = 12;
		phy_cfg->vswing_level = pctrl->lvds_cfg.phy_vswing & 0xf;
		phy_cfg->ext_pullup = (pctrl->lvds_cfg.phy_vswing >> 4) & 0x3;
		phy_cfg->vswing = lcd_phy_vswing_level_to_value(pdrv, phy_cfg->vswing_level);
		phy_cfg->preem_level = pctrl->lvds_cfg.phy_preem;
		temp = lcd_phy_preem_level_to_value(pdrv, phy_cfg->preem_level);
		for (i = 0; i < phy_cfg->lane_num; i++) {
			phy_cfg->lane[i].amp = 0;
			phy_cfg->lane[i].preem = temp;
		}
		break;
	case LCD_VBYONE:
		pctrl->vbyone_cfg.lane_count = *(p + LCD_UKEY_IF_ATTR_0) |
			((*(p + LCD_UKEY_IF_ATTR_0 + 1)) << 8);
		pctrl->vbyone_cfg.region_num = *(p + LCD_UKEY_IF_ATTR_1) |
			((*(p + LCD_UKEY_IF_ATTR_1 + 1)) << 8);
		pctrl->vbyone_cfg.byte_mode = *(p + LCD_UKEY_IF_ATTR_2) |
			((*(p + LCD_UKEY_IF_ATTR_2 + 1)) << 8);
		pctrl->vbyone_cfg.color_fmt = *(p + LCD_UKEY_IF_ATTR_3) |
			((*(p + LCD_UKEY_IF_ATTR_3 + 1)) << 8);
		pctrl->vbyone_cfg.phy_vswing = *(p + LCD_UKEY_IF_ATTR_4) |
			((*(p + LCD_UKEY_IF_ATTR_4 + 1)) << 8);
		pctrl->vbyone_cfg.phy_preem = *(p + LCD_UKEY_IF_ATTR_5) |
			((*(p + LCD_UKEY_IF_ATTR_5 + 1)) << 8);
		pctrl->vbyone_cfg.hw_filter_time =
			*(p + LCD_UKEY_IF_ATTR_8) |
			((*(p + LCD_UKEY_IF_ATTR_8 + 1)) << 8);
		pctrl->vbyone_cfg.hw_filter_cnt =
			*(p + LCD_UKEY_IF_ATTR_9) |
			((*(p + LCD_UKEY_IF_ATTR_9 + 1)) << 8);
		pctrl->vbyone_cfg.ctrl_flag = 0;
		pctrl->vbyone_cfg.power_on_reset_delay = VX1_PWR_ON_RESET_DLY_DFT;
		pctrl->vbyone_cfg.hpd_data_delay = VX1_HPD_DATA_DELAY_DFT;
		pctrl->vbyone_cfg.cdr_training_hold = VX1_CDR_TRAINING_HOLD_DFT;

		phy_cfg->lane_num = 8;
		phy_cfg->vswing_level = pctrl->vbyone_cfg.phy_vswing & 0xf;
		phy_cfg->ext_pullup = (pctrl->vbyone_cfg.phy_vswing >> 4) & 0x3;
		phy_cfg->vswing = lcd_phy_vswing_level_to_value(pdrv, phy_cfg->vswing_level);
		phy_cfg->preem_level = pctrl->vbyone_cfg.phy_preem;
		temp = lcd_phy_preem_level_to_value(pdrv, phy_cfg->preem_level);
		for (i = 0; i < phy_cfg->lane_num; i++) {
			phy_cfg->lane[i].amp = 0;
			phy_cfg->lane[i].preem = temp;
		}
		break;
	case LCD_MLVDS:
		pctrl->mlvds_cfg.channel_num = *(p + LCD_UKEY_IF_ATTR_0) |
			((*(p + LCD_UKEY_IF_ATTR_0 + 1)) << 8);
		pctrl->mlvds_cfg.channel_sel0 = *(p + LCD_UKEY_IF_ATTR_1) |
			((*(p + LCD_UKEY_IF_ATTR_1 + 1)) << 8) |
			(*(p + LCD_UKEY_IF_ATTR_2) << 16) |
			((*(p + LCD_UKEY_IF_ATTR_2 + 1)) << 24);
		pctrl->mlvds_cfg.channel_sel1 = *(p + LCD_UKEY_IF_ATTR_3) |
			((*(p + LCD_UKEY_IF_ATTR_3 + 1)) << 8) |
			(*(p + LCD_UKEY_IF_ATTR_4) << 16) |
			((*(p + LCD_UKEY_IF_ATTR_4 + 1)) << 24);
		pctrl->mlvds_cfg.clk_phase = *(p + LCD_UKEY_IF_ATTR_5) |
			((*(p + LCD_UKEY_IF_ATTR_5 + 1)) << 8);
		pctrl->mlvds_cfg.pn_swap = *(p + LCD_UKEY_IF_ATTR_6) |
			((*(p + LCD_UKEY_IF_ATTR_6 + 1)) << 8);
		pctrl->mlvds_cfg.bit_swap = *(p + LCD_UKEY_IF_ATTR_7) |
			((*(p + LCD_UKEY_IF_ATTR_7 + 1)) << 8);
		pctrl->mlvds_cfg.phy_vswing = *(p + LCD_UKEY_IF_ATTR_8) |
			((*(p + LCD_UKEY_IF_ATTR_8 + 1)) << 8);
		pctrl->mlvds_cfg.phy_preem = *(p + LCD_UKEY_IF_ATTR_9) |
			((*(p + LCD_UKEY_IF_ATTR_9 + 1)) << 8);

		phy_cfg->lane_num = 12;
		phy_cfg->vswing_level = pctrl->mlvds_cfg.phy_vswing & 0xf;
		phy_cfg->ext_pullup = (pctrl->mlvds_cfg.phy_vswing >> 4) & 0x3;
		phy_cfg->vswing = lcd_phy_vswing_level_to_value(pdrv, phy_cfg->vswing_level);
		phy_cfg->preem_level = pctrl->mlvds_cfg.phy_preem;
		temp = lcd_phy_preem_level_to_value(pdrv, phy_cfg->preem_level);
		for (i = 0; i < phy_cfg->lane_num; i++) {
			phy_cfg->lane[i].amp = 0;
			phy_cfg->lane[i].preem = temp;
		}
		break;
	case LCD_P2P:
		pctrl->p2p_cfg.p2p_type = *(p + LCD_UKEY_IF_ATTR_0) |
			((*(p + LCD_UKEY_IF_ATTR_0 + 1)) << 8);
		pctrl->p2p_cfg.lane_num = *(p + LCD_UKEY_IF_ATTR_1) |
			((*(p + LCD_UKEY_IF_ATTR_1 + 1)) << 8);
		pctrl->p2p_cfg.channel_sel0 = *(p + LCD_UKEY_IF_ATTR_2) |
			((*(p + LCD_UKEY_IF_ATTR_2 + 1)) << 8) |
			(*(p + LCD_UKEY_IF_ATTR_3) << 16) |
			((*(p + LCD_UKEY_IF_ATTR_3 + 1)) << 24);
		pctrl->p2p_cfg.channel_sel1 = *(p + LCD_UKEY_IF_ATTR_4) |
			((*(p + LCD_UKEY_IF_ATTR_4 + 1)) << 8) |
			(*(p + LCD_UKEY_IF_ATTR_5) << 16) |
			((*(p + LCD_UKEY_IF_ATTR_5 + 1)) << 24);
		pctrl->p2p_cfg.pn_swap = *(p + LCD_UKEY_IF_ATTR_6) |
			((*(p + LCD_UKEY_IF_ATTR_6 + 1)) << 8);
		pctrl->p2p_cfg.bit_swap = *(p + LCD_UKEY_IF_ATTR_7) |
			((*(p + LCD_UKEY_IF_ATTR_7 + 1)) << 8);
		pctrl->p2p_cfg.phy_vswing = *(p + LCD_UKEY_IF_ATTR_8) |
			((*(p + LCD_UKEY_IF_ATTR_8 + 1)) << 8);
		pctrl->p2p_cfg.phy_preem = *(p + LCD_UKEY_IF_ATTR_9) |
			((*(p + LCD_UKEY_IF_ATTR_9 + 1)) << 8);

		phy_cfg->lane_num = 12;
		phy_cfg->vswing_level = pctrl->p2p_cfg.phy_vswing & 0xf;
		phy_cfg->ext_pullup = (pctrl->p2p_cfg.phy_vswing >> 4) & 0x3;
		phy_cfg->vswing = lcd_phy_vswing_level_to_value(pdrv, phy_cfg->vswing_level);
		phy_cfg->preem_level = pctrl->p2p_cfg.phy_preem;
		temp = lcd_phy_preem_level_to_value(pdrv, phy_cfg->preem_level);
		for (i = 0; i < phy_cfg->lane_num; i++) {
			phy_cfg->lane[i].amp = 0;
			phy_cfg->lane[i].preem = temp;
		}
		break;
	default:
		LCDERR("[%d]: unsupport lcd_type: %d\n",
		       pdrv->index, pconf->basic.lcd_type);
		break;
	}

	lcd_vlock_param_load_from_unifykey(pdrv, para);

	/* step 3: check power sequence */
	ret = lcd_power_load_from_unifykey(pdrv, para, key_len, len);
	if (ret < 0) {
		kfree(para);
		return -1;
	}

	if (lcd_header.version == 2) {
		p = para + lcd_header.block_cur_size;
		lcd_config_load_from_unifykey_v2(pconf, p, key_len, lcd_header.block_cur_size);
	}

	kfree(para);

	lcd_optical_load_from_unifykey(pdrv);

#ifdef CONFIG_AMLOGIC_BACKLIGHT
	aml_bl_index_add(pdrv->index, 0);
#endif

	return 0;
}

static int lcd_config_load_init(struct aml_lcd_drv_s *pdrv)
{
	if (pdrv->config.fr_auto_dis)
		pdrv->fr_auto_policy = 0xff;

	if (pdrv->status & LCD_STATUS_ENCL_ON)
		lcd_clk_gate_switch(pdrv, 1);

	/* lock pinmux if lcd in on */
	if (pdrv->status & LCD_STATUS_IF_ON) {
		switch (pdrv->config.basic.lcd_type) {
		case LCD_TTL:
			lcd_ttl_pinmux_set(pdrv, 1);
			break;
		case LCD_VBYONE:
			lcd_vbyone_pinmux_set(pdrv, 1);
			break;
		case LCD_MLVDS:
			lcd_mlvds_pinmux_set(pdrv, 1);
			break;
		case LCD_P2P:
			lcd_p2p_pinmux_set(pdrv, 1);
			break;
		case LCD_EDP:
			lcd_edp_pinmux_set(pdrv, 1);
			break;
		default:
			break;
		}
	}

	return 0;
}

int lcd_get_config(struct aml_lcd_drv_s *pdrv)
{
	char key_str[10];
	int load_id = 0;
	int ret;

	memset(key_str, 0, 10);
	if (pdrv->index == 0)
		sprintf(key_str, "lcd");
	else
		sprintf(key_str, "lcd%d", pdrv->index);

	if (pdrv->key_valid) {
		ret = lcd_unifykey_check(key_str);
		if (ret < 0) {
			load_id = 0;
			LCDERR("[%d]: %s: can't find key %s\n",
			       pdrv->index, __func__, key_str);
		} else {
			load_id = 1;
		}
	}
	if (load_id) {
		LCDPR("[%d]: %s from unifykey\n", pdrv->index, __func__);
		pdrv->config_load = 1;
		ret = lcd_config_load_from_unifykey(pdrv, key_str);
	} else {
		LCDPR("[%d]: %s from dts\n", pdrv->index, __func__);
		pdrv->config_load = 0;
		ret = lcd_config_load_from_dts(pdrv);
	}
	if (ret)
		return -1;
	lcd_config_load_init(pdrv);
	lcd_config_load_print(pdrv);

	lcd_tcon_probe(pdrv);

	return 0;
}

void lcd_optical_vinfo_update(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_config_s *pconf;
	struct master_display_info_s *disp_vinfo;

	pconf = &pdrv->config;
	disp_vinfo = &pdrv->vinfo.master_display_info;
	disp_vinfo->present_flag = pconf->optical.hdr_support;
	disp_vinfo->features = pconf->optical.features;
	disp_vinfo->primaries[0][0] = pconf->optical.primaries_g_x;
	disp_vinfo->primaries[0][1] = pconf->optical.primaries_g_y;
	disp_vinfo->primaries[1][0] = pconf->optical.primaries_b_x;
	disp_vinfo->primaries[1][1] = pconf->optical.primaries_b_y;
	disp_vinfo->primaries[2][0] = pconf->optical.primaries_r_x;
	disp_vinfo->primaries[2][1] = pconf->optical.primaries_r_y;
	disp_vinfo->white_point[0] = pconf->optical.white_point_x;
	disp_vinfo->white_point[1] = pconf->optical.white_point_y;
	disp_vinfo->luminance[0] = pconf->optical.luma_max;
	disp_vinfo->luminance[1] = pconf->optical.luma_min;

	pdrv->vinfo.hdr_info.lumi_max = pconf->optical.luma_max;
	pdrv->vinfo.hdr_info.lumi_min = pconf->optical.luma_min;
	pdrv->vinfo.hdr_info.lumi_avg = pconf->optical.luma_avg;
	pdrv->vinfo.hdr_info.lumi_peak = pconf->optical.luma_peak;
	pdrv->vinfo.hdr_info.ldim_support = pconf->optical.ldim_support;
}

static unsigned int vbyone_lane_num[] = {
	1,
	2,
	4,
	8,
	8,
};

#define VBYONE_BIT_RATE_MAX		3100 //MHz
#define VBYONE_BIT_RATE_MIN		600
void lcd_vbyone_config_set(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_config_s *pconf = &pdrv->config;
	unsigned int band_width, bit_rate, pclk, phy_div;
	unsigned int byte_mode, lane_count, minlane;
	unsigned int temp, i;

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: %s\n", pdrv->index, __func__);

	//auto calculate bandwidth, clock
	lane_count = pconf->control.vbyone_cfg.lane_count;
	byte_mode = pconf->control.vbyone_cfg.byte_mode;
	/* byte_mode * byte2bit * 8/10_encoding * pclk =
	 *   byte_mode * 8 * 10 / 8 * pclk
	 */
	pclk = pconf->timing.lcd_clk / 1000; /* kHz */
	band_width = byte_mode * 10 * pclk;

	temp = VBYONE_BIT_RATE_MAX * 1000;
	temp = (band_width + temp - 1) / temp;
	for (i = 0; i < 4; i++) {
		if (temp <= vbyone_lane_num[i])
			break;
	}
	minlane = vbyone_lane_num[i];
	if (lane_count < minlane) {
		LCDERR("[%d]: vbyone lane_num(%d) is less than min(%d)\n",
			pdrv->index, lane_count, minlane);
		lane_count = minlane;
		pconf->control.vbyone_cfg.lane_count = lane_count;
		LCDPR("[%d]: change to min lane_num %d\n",
		      pdrv->index, minlane);
	}

	bit_rate = band_width / lane_count;
	phy_div = lane_count / lane_count;
	if (phy_div == 8) {
		phy_div /= 2;
		bit_rate /= 2;
	}
	if (bit_rate > (VBYONE_BIT_RATE_MAX * 1000)) {
		LCDERR("[%d]: vbyone bit rate(%dKHz) is out of max(%dKHz)\n",
			pdrv->index, bit_rate, (VBYONE_BIT_RATE_MAX * 1000));
	}
	if (bit_rate < (VBYONE_BIT_RATE_MIN * 1000)) {
		LCDERR("[%d]: vbyone bit rate(%dKHz) is out of min(%dKHz)\n",
			pdrv->index, bit_rate, (VBYONE_BIT_RATE_MIN * 1000));
	}
	bit_rate = bit_rate * 1000; /* Hz */

	pconf->control.vbyone_cfg.phy_div = phy_div;
	pconf->timing.bit_rate = bit_rate;

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
		LCDPR("[%d]: lane_count=%u, bit_rate = %uMHz, pclk=%u.%03uMhz\n",
		      pdrv->index, lane_count, (bit_rate / 1000000),
		      (pclk / 1000), (pclk % 1000));
	}
}

void lcd_mlvds_config_set(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_config_s *pconf = &pdrv->config;
	unsigned int bit_rate, pclk;
	unsigned int lcd_bits, channel_num;
	unsigned int channel_sel0, channel_sel1, pi_clk_sel = 0;
	unsigned int i, temp;

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: %s\n", pdrv->index, __func__);

	lcd_bits = pconf->basic.lcd_bits;
	channel_num = pconf->control.mlvds_cfg.channel_num;
	pclk = pconf->timing.lcd_clk / 1000;
	bit_rate = lcd_bits * 3 * pclk / channel_num;

	pconf->timing.bit_rate = bit_rate * 1000;

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
		LCDPR("[%d]: channel_num=%u, bit_rate=%u.%03uMHz, pclk=%u.%03uMhz\n",
		      pdrv->index, channel_num,
		      (bit_rate / 1000), (bit_rate % 1000),
		      (pclk / 1000), (pclk % 1000));
	}

	/* pi_clk select */
	channel_sel0 = pconf->control.mlvds_cfg.channel_sel0;
	channel_sel1 = pconf->control.mlvds_cfg.channel_sel1;
	/* mlvds channel:    //tx 12 channels
	 *    0: clk_a
	 *    1: d0_a
	 *    2: d1_a
	 *    3: d2_a
	 *    4: d3_a
	 *    5: d4_a
	 *    6: clk_b
	 *    7: d0_b
	 *    8: d1_b
	 *    9: d2_b
	 *   10: d3_b
	 *   11: d4_b
	 */
	for (i = 0; i < 8; i++) {
		temp = (channel_sel0 >> (i * 4)) & 0xf;
		if (temp == 0 || temp == 6)
			pi_clk_sel |= (1 << i);
	}
	for (i = 0; i < 4; i++) {
		temp = (channel_sel1 >> (i * 4)) & 0xf;
		if (temp == 0 || temp == 6)
			pi_clk_sel |= (1 << (i + 8));
	}

	pconf->control.mlvds_cfg.pi_clk_sel = pi_clk_sel;
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
		LCDPR("[%d]: channel_sel0=0x%08x, channel_sel1=0x%08x, pi_clk_sel=0x%03x\n",
		      pdrv->index, channel_sel0, channel_sel1, pi_clk_sel);
	}
}

void lcd_p2p_config_set(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_config_s *pconf = &pdrv->config;
	unsigned int bit_rate, pclk, p2p_type;
	unsigned int lcd_bits, lane_num;

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: %s\n", pdrv->index, __func__);

	lcd_bits = pconf->basic.lcd_bits;
	lane_num = pconf->control.p2p_cfg.lane_num;
	pclk = pconf->timing.lcd_clk / 1000;
	p2p_type = pconf->control.p2p_cfg.p2p_type & 0x1f;
	switch (p2p_type) {
	case P2P_CEDS:
		if (pclk >= 600000)
			bit_rate = pclk * 3 * lcd_bits / lane_num;
		else
			bit_rate = pclk * (3 * lcd_bits + 4) / lane_num;
		break;
	case P2P_CHPI: /* 8/10 coding */
		bit_rate = (pclk * 3 * lcd_bits * 10 / 8) / lane_num;
		break;
	default:
		bit_rate = pclk * 3 * lcd_bits / lane_num;
		break;
	}

	pconf->timing.bit_rate = bit_rate * 1000;

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
		LCDPR("[%d]: lane_num=%u, bit_rate=%u.%03uMHz, pclk=%u.%03uMhz\n",
		      pdrv->index, lane_num,
		      (bit_rate / 1000), (bit_rate % 1000),
		      (pclk / 1000), (pclk % 1000));
	}
}

void lcd_mipi_dsi_config_set(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_config_s *pconf = &pdrv->config;
	unsigned int pclk, bit_rate;
	unsigned int bit_rate_max, bit_rate_min, pll_out_fmin = 0;
	struct dsi_config_s *dconf = &pconf->control.mipi_cfg;
	struct lcd_clk_config_s *cconf;
	unsigned int temp, n;

	dconf = &pconf->control.mipi_cfg;

	/* unit in kHz for calculation */
	cconf = get_lcd_clk_config(pdrv);
	if (cconf && cconf->data)
		pll_out_fmin = cconf->data->pll_out_fmin;
	pclk = pconf->timing.lcd_clk / 1000;

	/* bit rate max */
	if (dconf->bit_rate_max == 0) { /* auto calculate */
		if (dconf->operation_mode_display == OPERATION_VIDEO_MODE &&
		    dconf->video_mode_type != BURST_MODE) {
			temp = pclk * 4 * dconf->data_bits;
			bit_rate = temp / dconf->lane_num;
		} else {
			temp = pclk * 3 * dconf->data_bits;
			bit_rate = temp / dconf->lane_num;
		}
		n = 0;
		bit_rate_min = 0;
		bit_rate_max = 0;
		while ((bit_rate_min < pll_out_fmin) && (n < 100)) {
			bit_rate_max = bit_rate + (pclk / 2) + (n * pclk);
			bit_rate_min = bit_rate_max - pclk;
			n++;
		}
		dconf->bit_rate_max = bit_rate_max / 1000; /* unit: MHz*/
		if (dconf->bit_rate_max > MIPI_BIT_RATE_MAX)
			dconf->bit_rate_max = MIPI_BIT_RATE_MAX;

		LCDPR("[%d]: mipi dsi bit_rate max=%dMHz\n",
		      pdrv->index, dconf->bit_rate_max);
	} else { /* user define */
		if (dconf->bit_rate_max < pll_out_fmin / 1000) {
			LCDERR("[%d]: invalid mipi-dsi bit_rate %dMHz (min=%dMHz)\n",
			       pdrv->index, dconf->bit_rate_max,
			       (pll_out_fmin / 1000));
		}
		if (dconf->bit_rate_max > MIPI_BIT_RATE_MAX) {
			LCDPR("[%d]: invalid mipi-dsi bit_rate_max %dMHz (max=%dMHz)\n",
			      pdrv->index, dconf->bit_rate_max,
			      MIPI_BIT_RATE_MAX);
		}
	}
}

void lcd_edp_config_set(struct aml_lcd_drv_s *pdrv)
{
	//todo
}

void lcd_vrr_config_update(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_config_s *pconf = &pdrv->config;
	unsigned int vmin, vmax;
	unsigned long long temp;

	temp = pconf->timing.sync_duration_num;
	temp *= pconf->basic.v_period;
	vmin = pconf->basic.v_period_min * pconf->timing.sync_duration_den;
	vmax = pconf->basic.v_period_max * pconf->timing.sync_duration_den;
	if (vmin > 0)
		pconf->basic.frame_rate_max = lcd_do_div(temp, vmin);
	if (vmax > 0)
		pconf->basic.frame_rate_min = lcd_do_div(temp, vmax);
}

void lcd_basic_timing_range_init(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_config_s *pconf = &pdrv->config;
	unsigned int sync_duration, h_period, v_period;
	unsigned long long temp;

	//for basic timing
	h_period = pconf->basic.h_period;
	v_period = pconf->basic.v_period;
	if (pconf->timing.lcd_clk < 200) { /* regard as frame_rate */
		sync_duration = pconf->timing.lcd_clk;
		pconf->timing.lcd_clk = sync_duration * h_period * v_period;
		pconf->timing.frame_rate = sync_duration;
		pconf->timing.sync_duration_num = sync_duration;
		pconf->timing.sync_duration_den = 1;
		pconf->timing.frac = 0;
	} else { /* regard as pixel clock */
		temp = pconf->timing.lcd_clk;
		temp *= 1000;
		sync_duration = lcd_do_div(temp, (v_period * h_period));
		pconf->timing.frame_rate = sync_duration / 1000;
		pconf->timing.sync_duration_num = sync_duration;
		pconf->timing.sync_duration_den = 1000;
		pconf->timing.frac = 0;
	}

	//for vrr range config
	lcd_vrr_config_update(pdrv);

	//save default config
	pconf->basic.v_period_min_dft = pconf->basic.v_period_min;
	pconf->basic.v_period_max_dft = pconf->basic.v_period_max;
	pconf->timing.lcd_clk_dft = pconf->timing.lcd_clk;
	pconf->timing.h_period_dft = pconf->basic.h_period;
	pconf->timing.v_period_dft = pconf->basic.v_period;
}

void lcd_timing_init_config(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_config_s *pconf = &pdrv->config;
	unsigned short h_period, v_period, h_active, v_active;
	unsigned short hsync_bp, hsync_width, vsync_bp, vsync_width;
	unsigned short de_hstart, de_vstart;
	unsigned short hs_start, hs_end, vs_start, vs_end;
	unsigned short h_delay;

	switch (pconf->basic.lcd_type) {
	case LCD_TTL:
		h_delay = TTL_DELAY;
		break;
	default:
		h_delay = 0;
		break;
	}
	/* use period_dft to avoid period changing offset */
	h_period = pconf->timing.h_period_dft;
	v_period = pconf->timing.v_period_dft;
	h_active = pconf->basic.h_active;
	v_active = pconf->basic.v_active;
	hsync_bp = pconf->timing.hsync_bp;
	hsync_width = pconf->timing.hsync_width;
	vsync_bp = pconf->timing.vsync_bp;
	vsync_width = pconf->timing.vsync_width;

	de_hstart = hsync_bp + hsync_width;
	de_vstart = vsync_bp + vsync_width;

	pconf->timing.hstart = de_hstart - h_delay;
	pconf->timing.vstart = de_vstart;
	pconf->timing.hend = pconf->basic.h_active + pconf->timing.hstart - 1;
	pconf->timing.vend = pconf->basic.v_active + pconf->timing.vstart - 1;

	pconf->timing.de_hs_addr = de_hstart;
	pconf->timing.de_he_addr = de_hstart + h_active;
	pconf->timing.de_vs_addr = de_vstart;
	pconf->timing.de_ve_addr = de_vstart + v_active - 1;

	hs_start = (de_hstart + h_period - hsync_bp - hsync_width) % h_period;
	hs_end = (de_hstart + h_period - hsync_bp) % h_period;
	pconf->timing.hs_hs_addr = hs_start;
	pconf->timing.hs_he_addr = hs_end;
	pconf->timing.hs_vs_addr = 0;
	pconf->timing.hs_ve_addr = v_period - 1;

	pconf->timing.vs_hs_addr = (hs_start + h_period) % h_period;
	pconf->timing.vs_he_addr = pconf->timing.vs_hs_addr;
	vs_start = (de_vstart + v_period - vsync_bp - vsync_width) % v_period;
	vs_end = (de_vstart + v_period - vsync_bp) % v_period;
	pconf->timing.vs_vs_addr = vs_start;
	pconf->timing.vs_ve_addr = vs_end;

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
		LCDPR("[%d]: hs_hs_addr=%d, hs_he_addr=%d\n"
		      "hs_vs_addr=%d, hs_ve_addr=%d\n"
		      "vs_hs_addr=%d, vs_he_addr=%d\n"
		      "vs_vs_addr=%d, vs_ve_addr=%d\n",
		      pdrv->index,
		      pconf->timing.hs_hs_addr, pconf->timing.hs_he_addr,
		      pconf->timing.hs_vs_addr, pconf->timing.hs_ve_addr,
		      pconf->timing.vs_hs_addr, pconf->timing.vs_he_addr,
		      pconf->timing.vs_vs_addr, pconf->timing.vs_ve_addr);
	}
}

int lcd_fr_is_fixed(struct aml_lcd_drv_s *pdrv)
{
	int ret = 0;

	switch (pdrv->config.timing.fr_adjust_type) {
	case 5: /* free run mode, vlock enabled nearby fixed frame rate */
	case 0xff: /* fix fr mode, vlock disabled */
		ret = 1;
		break;
	default:
		ret = 0;
		break;
	}

	return ret;
}

int lcd_vmode_change(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_config_s *pconf = &pdrv->config;
	unsigned char type = pconf->timing.fr_adjust_type;
	 /* use default value to avoid offset */
	unsigned int pclk = pconf->timing.lcd_clk_dft;
	unsigned int h_period = pconf->timing.h_period_dft;
	unsigned int v_period = pconf->timing.v_period_dft;
	unsigned int pclk_min = pconf->basic.lcd_clk_min;
	unsigned int pclk_max = pconf->basic.lcd_clk_max;
	unsigned int duration_num = pconf->timing.sync_duration_num;
	unsigned int duration_den = pconf->timing.sync_duration_den;
	unsigned long long temp;
	char str[100];
	int len = 0;

	pconf->timing.clk_change = 0; /* clear clk flag */
	switch (type) {
	case 0: /* pixel clk adjust */
		temp = duration_num;
		temp = temp * h_period * v_period;
		pclk = lcd_do_div(temp, duration_den);
		if (pconf->timing.lcd_clk != pclk)
			pconf->timing.clk_change = LCD_CLK_PLL_CHANGE;
		break;
	case 1: /* htotal adjust */
		temp = pclk;
		temp =  temp * duration_den * 100;
		h_period = v_period * duration_num;
		h_period = lcd_do_div(temp, h_period);
		h_period = (h_period + 99) / 100; /* round off */
		if (pconf->basic.h_period != h_period) {
			/* check clk frac update */
			temp = duration_num;
			temp = temp * h_period * v_period;
			pclk = lcd_do_div(temp, duration_den);
		}
		if (pconf->timing.lcd_clk != pclk)
			pconf->timing.clk_change = LCD_CLK_FRAC_UPDATE;
		break;
	case 2: /* vtotal adjust */
		temp = pclk;
		temp = temp * duration_den * 100;
		v_period = h_period * duration_num;
		v_period = lcd_do_div(temp, v_period);
		v_period = (v_period + 99) / 100; /* round off */
		if (pconf->basic.v_period != v_period) {
			/* check clk frac update */
			temp = duration_num;
			temp = temp * h_period * v_period;
			pclk = lcd_do_div(temp, duration_den);
		}
		if (pconf->timing.lcd_clk != pclk)
			pconf->timing.clk_change = LCD_CLK_FRAC_UPDATE;
		break;
	case 3: /* free adjust, use min/max range to calculate */
		temp = pclk;
		temp = temp * duration_den * 100;
		v_period = h_period * duration_num;
		v_period = lcd_do_div(temp, v_period);
		v_period = (v_period + 99) / 100; /* round off */
		if (v_period > pconf->basic.v_period_max) {
			v_period = pconf->basic.v_period_max;
			h_period = v_period * duration_num;
			h_period = lcd_do_div(temp, h_period);
			h_period = (h_period + 99) / 100; /* round off */
			if (h_period > pconf->basic.h_period_max) {
				h_period = pconf->basic.h_period_max;
				temp = duration_num;
				temp = temp * h_period * v_period;
				pclk = lcd_do_div(temp, duration_den);
				if (pclk > pclk_max) {
					LCDERR("[%d]:  %s: invalid vmode\n",
						pdrv->index, __func__);
					return -1;
				}
				if (pconf->timing.lcd_clk != pclk)
					pconf->timing.clk_change = LCD_CLK_PLL_CHANGE;
			}
		} else if (v_period < pconf->basic.v_period_min) {
			v_period = pconf->basic.v_period_min;
			h_period = v_period * duration_num;
			h_period = lcd_do_div(temp, h_period);
			h_period = (h_period + 99) / 100; /* round off */
			if (h_period < pconf->basic.h_period_min) {
				h_period = pconf->basic.h_period_min;
				temp = duration_num;
				temp = temp * h_period * v_period;
				pclk = lcd_do_div(temp, duration_den);
				if (pclk < pclk_min) {
					LCDERR("[%d]: %s: invalid vmode\n",
						pdrv->index, __func__);
					return -1;
				}
				if (pconf->timing.lcd_clk != pclk)
					pconf->timing.clk_change = LCD_CLK_PLL_CHANGE;
			}
		}
		/* check clk frac update */
		if ((pconf->timing.clk_change & LCD_CLK_PLL_CHANGE) == 0) {
			temp = duration_num;
			temp = temp * h_period * v_period;
			pclk = lcd_do_div(temp, duration_den);
			if (pconf->timing.lcd_clk != pclk)
				pconf->timing.clk_change = LCD_CLK_FRAC_UPDATE;
		}
		break;
	case 4: /* hdmi mode */
		if (((duration_num / duration_den) == 59) ||
		    ((duration_num / duration_den) == 47) ||
		    ((duration_num / duration_den) == 119) ||
		    ((duration_num / duration_den) == 95)) {
			/* pixel clk adjust */
			temp = duration_num;
			temp = temp * h_period * v_period;
			pclk = lcd_do_div(temp, duration_den);
			if (pconf->timing.lcd_clk != pclk)
				pconf->timing.clk_change = LCD_CLK_PLL_CHANGE;
		} else {
			/* htotal adjust */
			temp = pclk;
			temp = temp * duration_den * 100;
			h_period = v_period * duration_num;
			h_period = lcd_do_div(temp, h_period);
			h_period = (h_period + 99) / 100; /* round off */
			if (pconf->basic.h_period != h_period) {
				/* check clk frac update */
				temp = duration_num;
				temp = temp * h_period * v_period;
				pclk = lcd_do_div(temp, duration_den);
			}
			if (pconf->timing.lcd_clk != pclk)
				pconf->timing.clk_change = LCD_CLK_FRAC_UPDATE;
		}
		break;
	default:
		LCDERR("[%d]: %s: invalid fr_adjust_type: %d\n",
		       pdrv->index, __func__, type);
		return 0;
	}

	memset(str, 0, 100);
	if (pconf->basic.v_period != v_period) {
		len += sprintf(str + len, "v_period %u->%u",
			pconf->basic.v_period, v_period);
		/* update v_period */
		pconf->basic.v_period = v_period;
	}
	if (pconf->basic.h_period != h_period) {
		if (len > 0)
			len += sprintf(str + len, ", ");
		len += sprintf(str + len, "h_period %u->%u",
			pconf->basic.h_period, h_period);
		/* update h_period */
		pconf->basic.h_period = h_period;
	}
	if (pconf->timing.lcd_clk != pclk) {
		if (len > 0)
			len += sprintf(str + len, ", ");
		len += sprintf(str + len, "pclk %u.%03uMHz->%u.%03uMHz, clk_change:%d",
			       (pconf->timing.lcd_clk / 1000000),
			       ((pconf->timing.lcd_clk / 1000) % 1000),
			       (pclk / 1000000), ((pclk / 1000) % 1000),
			       pconf->timing.clk_change);
		pconf->timing.lcd_clk = pclk;
	}
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
		if (len > 0)
			LCDPR("[%d]: %s: %s\n", pdrv->index, __func__, str);
	}

	return 0;
}

void lcd_clk_change(struct aml_lcd_drv_s *pdrv)
{
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
		LCDPR("[%d]: %s: clk_change:%d\n",
			pdrv->index, __func__, pdrv->config.timing.clk_change);
	}

	switch (pdrv->config.timing.clk_change) {
	case LCD_CLK_PLL_CHANGE:
		lcd_clk_generate_parameter(pdrv);
		lcd_set_clk(pdrv);
		break;
	case LCD_CLK_FRAC_UPDATE:
		lcd_update_clk(pdrv);
		break;
	default:
		break;
	}
}

void lcd_if_enable_retry(struct aml_lcd_drv_s *pdrv)
{
	pdrv->config.retry_enable_cnt = 0;
	while (pdrv->config.retry_enable_flag) {
		if (pdrv->config.retry_enable_cnt++ >= LCD_ENABLE_RETRY_MAX)
			break;
		LCDPR("[%d]: retry enable...%d\n",
		      pdrv->index, pdrv->config.retry_enable_cnt);
		aml_lcd_notifier_call_chain(LCD_EVENT_IF_POWER_OFF, (void *)pdrv);
		msleep(1000);
		aml_lcd_notifier_call_chain(LCD_EVENT_IF_POWER_ON, (void *)pdrv);
	}
	pdrv->config.retry_enable_cnt = 0;
}

void lcd_vout_notify_mode_change_pre(struct aml_lcd_drv_s *pdrv)
{
	if (pdrv->viu_sel == 1) {
		vout_notifier_call_chain(VOUT_EVENT_MODE_CHANGE_PRE, &pdrv->vinfo.mode);
	} else if (pdrv->viu_sel == 2) {
#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
		vout2_notifier_call_chain(VOUT_EVENT_MODE_CHANGE_PRE, &pdrv->vinfo.mode);
#endif
	} else if (pdrv->viu_sel == 3) {
#ifdef CONFIG_AMLOGIC_VOUT3_SERVE
		vout3_notifier_call_chain(VOUT_EVENT_MODE_CHANGE_PRE, &pdrv->vinfo.mode);
#endif
	}
}

void lcd_vout_notify_mode_change(struct aml_lcd_drv_s *pdrv)
{
	if (pdrv->viu_sel == 1) {
		vout_notifier_call_chain(VOUT_EVENT_MODE_CHANGE, &pdrv->vinfo.mode);
	} else if (pdrv->viu_sel == 2) {
#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
		vout2_notifier_call_chain(VOUT_EVENT_MODE_CHANGE, &pdrv->vinfo.mode);
#endif
	} else if (pdrv->viu_sel == 3) {
#ifdef CONFIG_AMLOGIC_VOUT3_SERVE
		vout3_notifier_call_chain(VOUT_EVENT_MODE_CHANGE, &pdrv->vinfo.mode);
#endif
	}
}

void lcd_vinfo_update(struct aml_lcd_drv_s *pdrv)
{
	struct vinfo_s *vinfo;
	struct lcd_config_s *pconf;
	unsigned int temp;

	vinfo = &pdrv->vinfo;
	pconf = &pdrv->config;

	vinfo->width = pconf->basic.h_active;
	vinfo->height = pconf->basic.v_active;
	vinfo->field_height = pconf->basic.v_active;
	vinfo->aspect_ratio_num = pconf->basic.screen_width;
	vinfo->aspect_ratio_den = pconf->basic.screen_height;
	vinfo->screen_real_width = pconf->basic.screen_width;
	vinfo->screen_real_height = pconf->basic.screen_height;
	vinfo->sync_duration_num = pconf->timing.sync_duration_num;
	vinfo->sync_duration_den = pconf->timing.sync_duration_den;
	vinfo->frac = pconf->timing.frac;
	vinfo->std_duration = pconf->timing.frame_rate;
	vinfo->vfreq_max = pconf->basic.frame_rate_max;
	vinfo->vfreq_min = pconf->basic.frame_rate_min;
	vinfo->video_clk = pconf->timing.lcd_clk;
	vinfo->htotal = pconf->basic.h_period;
	vinfo->vtotal = pconf->basic.v_period;
	vinfo->hsw = pconf->timing.hsync_width;
	vinfo->hbp = pconf->timing.hsync_bp;
	temp = pconf->basic.h_period - pconf->basic.h_active -
		pconf->timing.hsync_width - pconf->timing.hsync_bp;
	vinfo->hfp = temp;
	vinfo->vsw = pconf->timing.vsync_width;
	vinfo->vbp = pconf->timing.vsync_bp;
	temp = pconf->basic.v_period - pconf->basic.v_active -
		pconf->timing.vsync_width - pconf->timing.vsync_bp;
	vinfo->vfp = temp;

	lcd_vout_notify_mode_change(pdrv);
}

unsigned int lcd_vrr_lfc_switch(void *dev_data, int fps)
{
	struct aml_lcd_drv_s *pdrv;
	unsigned long long temp;
	unsigned int h_period, v_period;

	pdrv = (struct aml_lcd_drv_s *)dev_data;
	if (!pdrv) {
		LCDERR("%s: vrr dev_data is null\n", __func__);
		return 0;
	}
	h_period = pdrv->config.basic.h_period;
	v_period = pdrv->config.basic.v_period;

	temp = pdrv->config.timing.lcd_clk;
	temp *= 100;
	h_period = h_period * fps * 2;
	v_period = lcd_do_div(temp, h_period);
	v_period = (v_period + 99) / 100; /* round off */

	return v_period;
}

void lcd_vrr_dev_update(struct aml_lcd_drv_s *pdrv)
{
	if (!pdrv->vrr_dev)
		return;

	if (pdrv->config.timing.fr_adjust_type == 2) /* vtotal adj */
		pdrv->vrr_dev->enable = 1;
	else
		pdrv->vrr_dev->enable = 0;

	pdrv->vrr_dev->vline = pdrv->config.basic.v_period;
	pdrv->vrr_dev->vline_max = pdrv->config.basic.v_period_max;
	pdrv->vrr_dev->vline_min = pdrv->config.basic.v_period_min;
	pdrv->vrr_dev->vfreq_max = pdrv->config.basic.frame_rate_max;
	pdrv->vrr_dev->vfreq_min = pdrv->config.basic.frame_rate_min;
}

