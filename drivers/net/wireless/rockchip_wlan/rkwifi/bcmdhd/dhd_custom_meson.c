/*
 * Platform Dependent file for Khadas VIM3
 *
 * Copyright (C) 2025 Synaptics Incorporated. All rights reserved.
 *
 * This software is licensed to you under the terms of the
 * GNU General Public License version 2 (the "GPL") with Broadcom special exception.
 *
 * INFORMATION CONTAINED IN THIS DOCUMENT IS PROVIDED "AS-IS," AND SYNAPTICS
 * EXPRESSLY DISCLAIMS ALL EXPRESS AND IMPLIED WARRANTIES, INCLUDING ANY
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE,
 * AND ANY WARRANTIES OF NON-INFRINGEMENT OF ANY INTELLECTUAL PROPERTY RIGHTS.
 * IN NO EVENT SHALL SYNAPTICS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, PUNITIVE, OR CONSEQUENTIAL DAMAGES ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OF THE INFORMATION CONTAINED IN THIS DOCUMENT, HOWEVER CAUSED
 * AND BASED ON ANY THEORY OF LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, AND EVEN IF SYNAPTICS WAS ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE. IF A TRIBUNAL OF COMPETENT JURISDICTION
 * DOES NOT PERMIT THE DISCLAIMER OF DIRECT DAMAGES OR ANY OTHER DAMAGES,
 * SYNAPTICS' TOTAL CUMULATIVE LIABILITY TO ANY PARTY SHALL NOT
 * EXCEED ONE HUNDRED U.S. DOLLARS
 *
 * Copyright (C) 2025, Broadcom.
 *
 *      Unless you and Broadcom execute a separate written software license
 * agreement governing use of this software, this software is licensed to you
 * under the terms of the GNU General Public License version 2 (the "GPL"),
 * available at http://www.broadcom.com/licenses/GPLv2.php, with the
 * following added to such license:
 *
 *      As a special exception, the copyright holders of this software give you
 * permission to link this software with independent modules, and to copy and
 * distribute the resulting executable under terms of your choice, provided that
 * you also meet, for each linked independent module, the terms and conditions of
 * the license of that module.  An independent module is a module which is not
 * derived from this software.  The special exception does not apply to any
 * modifications of the software.
 *
 *
 * <<Broadcom-WL-IPTag/Open:>>
 *
 * $Id$
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/skbuff.h>
#include <linux/fcntl.h>
#include <linux/fs.h>
#include <linux/of_gpio.h>
#ifdef CONFIG_WIFI_CONTROL_FUNC
#include <linux/wlan_plat.h>
#else
#include <dhd_plat.h>
#endif /* CONFIG_WIFI_CONTROL_FUNC */
#include <dhd_dbg.h>
#include <dhd.h>

#ifdef CUSTOMER_HW_AMLOGIC
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 8, 0))
#include <linux/amlogic/aml_gpio_consumer.h>
extern int wifi_irq_trigger_level(void);
extern u8 *wifi_get_mac(void);
extern u8 *wifi_get_ap_mac(void);
#endif
extern  void sdio_reinit(void);
extern void extern_wifi_set_enable(int is_on);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0))
extern int wifi_irq_num(void);
#endif
#endif /* CUSTOMER_HW_AMLOGIC */

#ifdef CONFIG_BROADCOM_WIFI_RESERVED_MEM
extern int dhd_init_wlan_mem(void);
extern void dhd_exit_wlan_mem(void);
#ifdef CUSTOMER_HW_AMLOGIC
extern void *bcmdhd_mem_prealloc(int section, unsigned long size);
#define dhd_wlan_mem_prealloc bcmdhd_mem_prealloc
#else
extern void *dhd_wlan_mem_prealloc(int section, unsigned long size);
#endif /* CUSTOMER_HW_AMLOGIC */
#endif /* CONFIG_BROADCOM_WIFI_RESERVED_MEM */

#ifdef CUSTOMER_HW_AMLOGIC
/* Other mechanism gets the pins toggled */
#define WLAN_REG_ON_GPIO		(-1)
#define WLAN_HOST_WAKE_GPIO		(-1)
#else
#define WLAN_REG_ON_GPIO		491
#define WLAN_HOST_WAKE_GPIO		493
#endif /* CUSTOMER_HW_AMLOGIC */

static int wlan_reg_on = -1;
static int wlan_gpio_reg_on = -1;
#define DHD_DT_COMPAT_ENTRY		"android,bcmdhd_wlan"
#define WIFI_WL_REG_ON_PROPNAME		"wl_reg_on"
#define WIFI_GPIO_WL_REG_ON_PROPNAME	"gpio_wl_reg_on"

static int wlan_host_wake_up = -1;
static int wlan_gpio_host_wake_up = -1;
static int wlan_host_wake_irq = 0;
static int wlan_host_wake_up_initial = 0;
static int wlan_gpio_host_wake_up_initial = 0;
#define WIFI_WLAN_HOST_WAKE_PROPNAME    "wl_host_wake"
#define WIFI_GPIO_WLAN_HOST_WAKE_PROPNAME    "gpio_wl_host_wake"

#ifdef BCMSDIO
extern bool mmc_sdcard_slot_present(void);
#endif /* BCMSDIO */

struct resource dhd_wlan_resources = {
	.name  = "bcmdhd_wlan_irq",
	.start = 0, /* Dummy */
	.end   = 0, /* Dummy */
	.flags = IORESOURCE_IRQ | IORESOURCE_IRQ_SHAREABLE |
#ifdef BCMPCIE
	IORESOURCE_IRQ_HIGHEDGE,
#else /* non-BCMPCIE */
	IORESOURCE_IRQ_HIGHLEVEL,
#endif /* BCMPCIE */
};

static int
dhd_wifi_init_gpio(void)
{
	int reg_on_val;
	int gpio_reg_on_val;
	/* ========== WLAN_PWR_EN ============ */
	char *wlan_node = DHD_DT_COMPAT_ENTRY;
	struct device_node *root_node = NULL;

#ifdef HOST_WAKE_IRQ_CPUCORE
	struct cpumask host_wake_cpu_mask;
	cpumask_clear(&host_wake_cpu_mask);
	/* Set on cpu HOST_WAKE_IRQ_CPUCORE */
	cpumask_set_cpu(HOST_WAKE_IRQ_CPUCORE, &host_wake_cpu_mask);
#endif /* HOST_WAKE_IRQ_CPUCORE */

	root_node = of_find_compatible_node(NULL, NULL, wlan_node);
	if (root_node) {
		wlan_gpio_reg_on = of_get_named_gpio(root_node, WIFI_GPIO_WL_REG_ON_PROPNAME, 0);
		wlan_gpio_host_wake_up =
			of_get_named_gpio(root_node, WIFI_GPIO_WLAN_HOST_WAKE_PROPNAME, 0);
		wlan_reg_on = of_get_named_gpio(root_node, WIFI_WL_REG_ON_PROPNAME, 0);
		wlan_host_wake_up = of_get_named_gpio(root_node, WIFI_WLAN_HOST_WAKE_PROPNAME, 0);
	} else {
		DHD_ERROR(("failed to get device node of BRCM WLAN, use default GPIOs\n"));
		wlan_reg_on = WLAN_REG_ON_GPIO;
		wlan_host_wake_up = WLAN_HOST_WAKE_GPIO;
	}

	// add to dump the DTS to confirm in case some platform special changes
	if (root_node) {
		struct property * pp = root_node->properties;

		while (NULL != pp) {
			char  str[128] = {0};
			memset(str, 0, sizeof(str));
			memcpy(str, pp->value, pp->length);
			DHD_ERROR(("%s: name='%s', len=%d, val='%s'\n",
				__func__, pp->name, pp->length, str));
			pp = pp->next;
		}
	}

	/* ========== WLAN_PWR_EN ============ */
	DHD_ERROR(("%s: wlan_power('%s'): %d\n", __func__,
		WIFI_WL_REG_ON_PROPNAME, wlan_reg_on));
	DHD_ERROR(("%s: gpio_wlan_power('%s'): %d\n", __func__,
		WIFI_GPIO_WL_REG_ON_PROPNAME, wlan_gpio_reg_on));

#ifdef CUSTOMER_HW_AMLOGIC
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 14, 0))
	wlan_host_wake_irq = INT_GPIO_4;
#else
	wlan_host_wake_irq = wifi_irq_num();
#endif
	BCM_REFERENCE(reg_on_val);
	BCM_REFERENCE(gpio_reg_on_val);
	BCM_REFERENCE(wlan_host_wake_up_initial);
	BCM_REFERENCE(wlan_gpio_host_wake_up_initial);

	/* Override to low level if needed */
#if defined(HW_OOB) && (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 8, 0))
	if (wifi_irq_trigger_level() == GPIO_IRQ_LOW) {
		dhd_wlan_resources.flags = IORESOURCE_IRQ |
			IORESOURCE_IRQ_LOWLEVEL | IORESOURCE_IRQ_SHAREABLE;
	}
#endif /* HW_OOB && KERNEL_VERSION(4, 8, 0) */
//	dhd_wlan_resources.flags &= IRQF_TRIGGER_MASK;
#else
#ifdef BCMSDIO
	if (mmc_sdcard_slot_present()) { // use ext card
		wlan_host_wake_up_initial = GPIOF_OUT_INIT_HIGH;
		wlan_gpio_host_wake_up_initial = GPIOF_OUT_INIT_HIGH;
	} else
#endif /* BCMSDIO */
	{
		wlan_host_wake_up_initial = GPIOF_OUT_INIT_HIGH;
		wlan_gpio_host_wake_up_initial = GPIOF_OUT_INIT_LOW;
	}
	/*
	 * For reg_on, gpio_request will fail if the gpio is configured to output-high
	 * in the dts using gpio-hog, so do not return error for failure.
	 */
	if (gpio_request_one(wlan_reg_on, wlan_host_wake_up_initial, "WL_REG_ON")) {
		DHD_ERROR(("%s: Failed to request gpio %d for WL_REG_ON, "
			"might have configured in the dts\n",
			__func__, wlan_reg_on));
	} else {
		DHD_ERROR(("%s: gpio_request WL_REG_ON done - WLAN_EN: GPIO %d\n",
			__func__, wlan_reg_on));
	}

	if (gpio_request_one(wlan_gpio_reg_on, wlan_gpio_host_wake_up_initial, "GPIO_WL_REG_ON")) {
		DHD_ERROR(("%s: Failed to request gpio %d for GPIO_WL_REG_ON, "
			"might have configured in the dts\n",
			__func__, wlan_gpio_reg_on));
	} else {
		DHD_ERROR(("%s: gpio_request GPIO_WL_REG_ON done - WLAN_EN: GPIO %d\n",
			__func__, wlan_gpio_reg_on));
	}

	reg_on_val = gpio_get_value(wlan_reg_on);
	gpio_reg_on_val = gpio_get_value(wlan_gpio_reg_on);
	DHD_ERROR(("%s: Initial WL_REG_ON: [%d], GPIO_WL_REG_ON: [%d]\n",
		__func__, reg_on_val, gpio_reg_on_val));

	if (reg_on_val == 0 && wlan_host_wake_up_initial == GPIOF_OUT_INIT_HIGH) {
		DHD_INFO(("%s: WL_REG_ON is LOW, drive it HIGH\n", __func__));
		if (gpio_direction_output(wlan_reg_on, 1)) {
			DHD_ERROR(("%s: WL_REG_ON is failed to pull up\n", __func__));
			return -EIO;
		}
	}

	if (reg_on_val == 1 && wlan_host_wake_up_initial == GPIOF_OUT_INIT_LOW) {
		DHD_INFO(("%s: WL_REG_ON is HIGH, drive it LOW\n", __func__));
		if (gpio_direction_output(wlan_reg_on, 0)) {
			DHD_ERROR(("%s: WL_REG_ON is failed to pull low\n", __func__));
			return -EIO;
		}
	}

	if (gpio_reg_on_val == 0 && wlan_gpio_host_wake_up_initial == GPIOF_OUT_INIT_HIGH) {
		DHD_INFO(("%s: GPIO_WL_REG_ON is LOW, drive it HIGH\n", __func__));
		if (gpio_direction_output(wlan_gpio_reg_on, 1)) {
			DHD_ERROR(("%s: GPIO_WL_REG_ON is failed to pull up\n", __func__));
			return -EIO;
		}
	}

	if (gpio_reg_on_val == 1 && wlan_host_wake_up_initial == GPIOF_OUT_INIT_LOW) {
		DHD_INFO(("%s: GPIO_WL_REG_ON is LOW, drive it LOW\n", __func__));
		if (gpio_direction_output(wlan_gpio_reg_on, 0)) {
			DHD_ERROR(("%s: GPIO_WL_REG_ON is failed to pull down\n", __func__));
			return -EIO;
		}
	}

#ifdef BCMSDIO
	if (mmc_sdcard_slot_present()) {
		wlan_reg_on = wlan_gpio_reg_on;
	}
#endif /* BCMSDIO */
	/* Wait for WIFI_TURNON_DELAY due to power stability */
	msleep(WIFI_TURNON_DELAY);

	/* ========== WLAN_HOST_WAKE ============ */
	DHD_ERROR(("%s: wlan_host_wake('%s'): %d, gpio_wlan_host_wake('%s'): %d\n",
		__func__, WIFI_WLAN_HOST_WAKE_PROPNAME, wlan_host_wake_up,
		WIFI_GPIO_WLAN_HOST_WAKE_PROPNAME, wlan_gpio_host_wake_up));

#ifdef BCMSDIO
	if (mmc_sdcard_slot_present()) {
		wlan_host_wake_up = wlan_gpio_host_wake_up;
	}
#endif /* BCMSDIO */
	//if (gpio_request_one(wlan_host_wake_up, GPIOF_IN, "WLAN_HOST_WAKE")) {
	if (gpio_request(wlan_host_wake_up, "bcmdhd")) {
		DHD_ERROR(("%s: Failed to request gpio %d for WLAN_HOST_WAKE\n",
			__func__, wlan_host_wake_up));
			return -ENODEV;
	} else {
		DHD_ERROR(("%s: gpio_request WLAN_HOST_WAKE done"
			" - WLAN_HOST_WAKE: GPIO %d\n",
			__func__, wlan_host_wake_up));
	}

	if (gpio_direction_input(wlan_host_wake_up)) {
		DHD_ERROR(("%s: Failed to set WL_HOST_WAKE gpio direction\n", __func__));
		return -EIO;
	}

	wlan_host_wake_irq = gpio_to_irq(wlan_host_wake_up);
#endif /* CUSTOMER_HW_AMLOGIC */
	DHD_ERROR(("%s: wlan_host_wake_irq %d\n", __func__, wlan_host_wake_irq));

#ifdef HOST_WAKE_IRQ_CPUCORE
	/* Core 0 and 1, A53, are not as fast as 2-5 (A73), to process the SDIO interrupt */
	if (
#ifdef BCMDHD_MODULAR
		irq_set_affinity_hint(wlan_host_wake_irq, &host_wake_cpu_mask) == 0)
#else
		irq_set_affinity(wlan_host_wake_irq, &host_wake_cpu_mask) == 0)
#endif /* BCMDHD_MODULAR */
	{
		DHD_ERROR(("%s: IRQ %d assigned to CPU %d\n", __func__,
			wlan_host_wake_irq, HOST_WAKE_IRQ_CPUCORE));
	} else {
		DHD_ERROR(("%s: Failed to set IRQ affinity for host wake\n", __func__));
	}
#endif /* HOST_WAKE_IRQ_CPUCORE */

	return 0;
}

// add for free GPIO resource
static int
dhd_wifi_deinit_gpio(void)
{
	if (wlan_reg_on >= 0) {
		gpio_free(wlan_reg_on);
	}
	if (wlan_host_wake_up >= 0) {
		gpio_free(wlan_host_wake_up);
	}

	return 0;
}

static int
dhd_wlan_power(int onoff)
{
	DHD_INFO(("------------------------------------------------"));
	DHD_INFO(("------------------------------------------------\n"));
	DHD_ERROR(("%s Enter: power %s(gpio %d)\n", __func__, onoff ? "on" : "off", wlan_reg_on));

	if (onoff) {
		DHD_ERROR(("======== PULL WL_REG_ON(%d) HIGH! ========\n", wlan_reg_on));
#ifdef CUSTOMER_HW_AMLOGIC
#ifdef BCMSDIO
		extern_wifi_set_enable(0);
		mdelay(WIFI_TURNON_DELAY);
		extern_wifi_set_enable(1);
		mdelay(WIFI_TURNON_DELAY);
#endif
#else
		if (wlan_reg_on == -1) {
			DHD_ERROR(("%s: Skip setting gpio direction as gpio is invalid", __func__));
			return 0;
		}
		if (gpio_direction_output(wlan_reg_on, 1)) {
			DHD_ERROR(("%s: WL_REG_ON is failed to pull up\n", __func__));
			return -EIO;
		}
		if (gpio_get_value(wlan_reg_on)) {
			DHD_INFO(("WL_REG_ON on-step-2 : [%d]\n",
				gpio_get_value(wlan_reg_on)));
		} else {
			DHD_ERROR(("[%s] gpio value is 0. We need reinit.\n", __func__));
			if (gpio_direction_output(wlan_reg_on, 1)) {
				DHD_ERROR(("%s: WL_REG_ON is "
					"failed to pull up\n", __func__));
			}
		}
#endif /* CUSTOMER_HW_AMLOGIC */

		/* Wait for WIFI_TURNON_DELAY due to power stability */
		msleep(WIFI_TURNON_DELAY);
	} else {
#ifdef CUSTOMER_HW_AMLOGIC
#ifdef BCMSDIO
		extern_wifi_set_enable(0);
		mdelay(WIFI_TURNON_DELAY);
#endif /* BCMSDIO */
#else
		if (wlan_reg_on == -1) {
			DHD_ERROR(("%s: Skip setting gpio direction as gpio is invalid", __func__));
			return 0;
		}
		if (gpio_direction_output(wlan_reg_on, 0)) {
			DHD_ERROR(("%s: WL_REG_ON is failed to pull up\n", __func__));
			return -EIO;
		}
		if (gpio_get_value(wlan_reg_on)) {
			DHD_INFO(("WL_REG_ON on-step-2 : [%d]\n",
				gpio_get_value(wlan_reg_on)));
		}
#endif /* CUSTOMER_HW_AMLOGIC */
	}
	return 0;
}

static int
dhd_wlan_reset(int onoff)
{
	return 0;
}

// add for card detect
#if defined(BCMSDIO) && defined(BCMDHD_MODULAR) && defined(ENABLE_INSMOD_NO_FW_LOAD) && \
	!defined(GKI_NO_SDIO_PATCH)

#ifndef EMPTY_CARD_DETECT
extern int wifi_card_detect(void);
#endif /* EMPTY_CARD_DETECT */
#endif // BCMSDIO && BCMDHD_MODULAR && ENABLE_INSMOD_NO_FW_LOAD && !GKI_NO_SDIO_PATCH

static int
dhd_wlan_set_carddetect(int val)
{
// add for card detect
#if defined(BCMSDIO) && defined(CUSTOMER_HW_AMLOGIC)
	if (val) {
		DHD_ERROR(("======== Card detection to detect SDIO card! ========\n"));
		sdio_reinit();
	} else {
		DHD_ERROR(("======== Card detection to remove SDIO card! ========\n"));
		extern_wifi_set_enable(0);
		msleep(WIFI_TURNON_DELAY); /* was mdelay */
	}
#elif defined(BCMSDIO) && defined(BCMDHD_MODULAR) && defined(ENABLE_INSMOD_NO_FW_LOAD) \
	&&\
	!defined(GKI_NO_SDIO_PATCH)
#ifndef EMPTY_CARD_DETECT
	int ret = 0;

	ret = wifi_card_detect();
	if (0 > ret) {
		DHD_ERROR(("%s-%d: * error hapen, ret=%d (ignore when remove)\n",
			__func__, __LINE__, ret));
	}
#endif /* EMPTY_CARD_DETECT */
#elif defined(BCMPCIE) && defined(CUSTOMER_HW_AMLOGIC)
	if (val) {
		printf("======== Card detection to detect PCIE card! ========\n");
		msleep(WIFI_TURNON_DELAY); /* was mdelay */
	} else {
		printf("======== Card detection to remove PCIE card! ========\n");
	}
#endif // BCMSDIO && BCMDHD_MODULAR && ENABLE_INSMOD_NO_FW_LOAD && !GKI_NO_SDIO_PATCH
	return 0;
}

#ifdef CUSTOMER_HW_AMLOGIC
#ifdef CUSTOM_MULTI_MAC
static int dhd_wlan_get_mac_addr(unsigned char *buf, char *name)
#else
static int dhd_wlan_get_mac_addr(unsigned char *buf)
#endif /* CUSTOM_MULTI_MAC */
{
	int err = 0;

#ifdef CUSTOM_MULTI_MAC
	if (!strcmp("wlan1", name)) {
#ifdef CUSTOM_AP_MAC
		bcopy((char *)wifi_get_ap_mac(), buf, sizeof(struct ether_addr));
		if (buf[0] == 0xff) {
			printf("custom wifi ap mac is not set\n");
			err = -1;
		} else
			printf("custom wifi ap mac-addr: %02x:%02x:%02x:%02x:%02x:%02x\n",
				buf[0], buf[1], buf[2],
				buf[3], buf[4], buf[5]);
#else
		err = -1;
#endif
	} else
#endif /* CUSTOM_MULTI_MAC */
	{
#ifdef CUSTOMER_HW_AMLOGIC
		bcopy((char *)wifi_get_mac(), buf, sizeof(struct ether_addr));
		if (buf[0] == 0xff) {
			printf("custom wifi mac is not set\n");
			err = -1;
		} else
			printf("custom wifi mac-addr: %02x:%02x:%02x:%02x:%02x:%02x\n",
				buf[0], buf[1], buf[2],
				buf[3], buf[4], buf[5]);
#endif
	}

#ifdef EXAMPLE_GET_MAC_VER2
	/* EXAMPLE code */
	{
		char macpad[56] = {
		0x00, 0xaa, 0x9c, 0x84, 0xc7, 0xbc, 0x9b, 0xf6,
		0x02, 0x33, 0xa9, 0x4d, 0x5c, 0xb4, 0x0a, 0x5d,
		0xa8, 0xef, 0xb0, 0xcf, 0x8e, 0xbf, 0x24, 0x8a,
		0x87, 0x0f, 0x6f, 0x0d, 0xeb, 0x83, 0x6a, 0x70,
		0x4a, 0xeb, 0xf6, 0xe6, 0x3c, 0xe7, 0x5f, 0xfc,
		0x0e, 0xa7, 0xb3, 0x0f, 0x00, 0xe4, 0x4a, 0xaf,
		0x87, 0x08, 0x16, 0x6d, 0x3a, 0xe3, 0xc7, 0x80};
		bcopy(macpad, buf+6, sizeof(macpad));
	}
#endif /* EXAMPLE_GET_MAC_VER2 */

	printf("======== %s err=%d ========\n", __func__, err);

	return err;
}

static struct cntry_locales_custom brcm_wlan_translate_custom_table[] = {
	/* Table should be filled out based on custom platform regulatory requirement */
#ifdef EXAMPLE_TABLE
	{"",   "XT", 49},  /* Universal if Country code is unknown or empty */
	{"US", "US", 0},
#endif /* EXMAPLE_TABLE */
};

#ifdef CUSTOM_FORCE_NODFS_FLAG
struct cntry_locales_custom brcm_wlan_translate_nodfs_table[] = {
#ifdef EXAMPLE_TABLE
	{"",   "XT", 50},  /* Universal if Country code is unknown or empty */
	{"US", "US", 0},
#endif /* EXMAPLE_TABLE */
};
#endif /* CUSTOM_FORCE_NODFS_FLAG */

#ifdef CUSTOM_COUNTRY_CODE
static void *dhd_wlan_get_country_code(char *ccode, u32 flags)
#else
static void *dhd_wlan_get_country_code(char *ccode)
#endif /* CUSTOM_COUNTRY_CODE */
{
	struct cntry_locales_custom *locales;
	int size;
	int i;

	if (!ccode)
		return NULL;

#ifdef CUSTOM_FORCE_NODFS_FLAG
	if (flags & WLAN_PLAT_NODFS_FLAG) {
		locales = brcm_wlan_translate_nodfs_table;
		size = ARRAY_SIZE(brcm_wlan_translate_nodfs_table);
	} else {
#endif
		locales = brcm_wlan_translate_custom_table;
		size = ARRAY_SIZE(brcm_wlan_translate_custom_table);
#ifdef CUSTOM_FORCE_NODFS_FLAG
	}
#endif

	for (i = 0; i < size; i++)
		if (strcmp(ccode, locales[i].iso_abbrev) == 0)
			return &locales[i];
	return NULL;
}
#endif /* CUSTOMER_HW_AMLOGIC */

#ifdef DHD_USE_HOST_WAKE
static int
dhd_wlan_get_wake_irq(void)
{
	return gpio_to_irq(wlan_host_wake_up);
}

static int
dhd_get_wlan_oob_gpio_level(void)
{
	return gpio_is_valid(wlan_host_wake_up) ?
		gpio_get_value_cansleep(wlan_host_wake_up) : -1;
}

int
dhd_get_wlan_oob_gpio(void)
{
	return dhd_get_wlan_oob_gpio_level();
}
#endif /* DHD_USE_HOST_WAKE */

struct wifi_platform_data dhd_wlan_control = {
	.set_power      = dhd_wlan_power,
	.set_reset      = dhd_wlan_reset,
	.set_carddetect = dhd_wlan_set_carddetect,
#ifdef CONFIG_BROADCOM_WIFI_RESERVED_MEM
	.mem_prealloc   = dhd_wlan_mem_prealloc,
#endif /* CONFIG_BROADCOM_WIFI_RESERVED_MEM */
#ifdef DHD_USE_HOST_WAKE
	.get_wake_irq   = dhd_wlan_get_wake_irq,
	.get_oob_gpio_level   = dhd_get_wlan_oob_gpio_level,
#endif /* DHD_USE_HOST_WAKE */
#ifdef CUSTOMER_HW_AMLOGIC
	.get_mac_addr	= dhd_wlan_get_mac_addr,
	.get_country_code = dhd_wlan_get_country_code,
#endif /* CUSTOMER_HW_AMLOGIC */
};

int
dhd_wlan_init(void)
{
	int ret = 0;
	DHD_ERROR(("%s: START.......\n", __func__));
	ret = dhd_wifi_init_gpio();
	if (ret < 0) {
		DHD_ERROR(("%s: failed to initiate GPIO, ret=%d\n",
			__func__, ret));
		goto fail;
	}

#ifdef DHD_USE_HOST_WAKE
	dhd_wlan_resources.start = wlan_host_wake_irq;
	dhd_wlan_resources.end = wlan_host_wake_irq;

	DHD_ERROR(("%s: WL_HOST_WAKE=%d, oob_irq=%d, oob_irq_flags=%ld\n", __func__,
		wlan_host_wake_up, wlan_host_wake_irq, dhd_wlan_resources.flags));
#else
	BCM_REFERENCE(ret);
#endif /* DHD_USE_HOST_WAKE */

#ifdef CONFIG_BROADCOM_WIFI_RESERVED_MEM
#ifndef CUSTOMER_HW_AMLOGIC
	/* Allocated by kernel */
	ret = dhd_init_wlan_mem();
	if (ret < 0) {
		DHD_ERROR(("%s: failed to alloc reserved memory,"
				" ret=%d\n", __func__, ret));
	}
#endif /* CUSTOMER_HW_AMLOGIC */
#endif /* CONFIG_BROADCOM_WIFI_RESERVED_MEM */

fail:
	DHD_INFO(("%s: FINISH.......\n", __func__));
	// add to free gpio resource
	if (0 > ret) {
		dhd_wifi_deinit_gpio();
	}
	return ret;
}

void
dhd_wlan_deinit(void)
{
	if (wlan_host_wake_up >= 0) {
		gpio_free(wlan_host_wake_up);
	}
	wlan_host_wake_up = -1;

	if (wlan_reg_on >= 0) {
		gpio_free(wlan_reg_on);
	}
	wlan_reg_on = -1;

#ifdef CONFIG_BROADCOM_WIFI_RESERVED_MEM
#ifndef CUSTOMER_HW_AMLOGIC
	dhd_exit_wlan_mem();
#endif /* CUSTOMER_HW_AMLOGIC */
#endif /* CONFIG_BROADCOM_WIFI_RESERVED_MEM */
}

#ifdef DHD_COREDUMP
void
dhd_plat_register_coredump(void)
{
	return;
}

void
dhd_plat_unregister_coredump(void)
{
	return;
}
#endif /* DHD_COREDUMP */

#ifndef BCMDHD_MODULAR
/* Required only for Built-in DHD */
device_initcall(dhd_wlan_init);
#endif /* BOARD_HIKEY_MODULAR */
