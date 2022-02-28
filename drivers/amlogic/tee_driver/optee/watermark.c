/*
 * Copyright (c) 2017, Amlogic, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/irqreturn.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include "optee_smc.h"
#include "optee_private.h"

#define SRC_IRQ_NAME        "viu-vsync"
#define DST_IRQ_NAME        "wm-vsync"

static int g_irq_id = 0;

static uint32_t check_wm_status(void)
{
	struct arm_smccc_res res;

	arm_smccc_smc(OPTEE_SMC_CHECK_WM_STATUS, 0, 0, 0, 0, 0, 0, 0, &res);

	return res.a0;
}

static uint32_t flush_wm(void)
{
	struct arm_smccc_res res;

	arm_smccc_smc(OPTEE_SMC_FLUSH_WM, 0, 0, 0, 0, 0, 0, 0, &res);

	return res.a0;
}

static irqreturn_t vsync_isr(int irq, void *dev_id)
{
	flush_wm();

	return IRQ_HANDLED;
}

void parse_soc(char *src, char soc[32])
{
	strcpy(soc, src + strlen("amlogic, "));
	if (strcmp(soc, "s4d") == 0)
		soc[2] = '\0';
}

static int get_wm_irq_id(void)
{
	int irq_id = 0;
	struct device_node *root_node = of_find_node_by_path("/");
	struct property *root_prop = NULL;
	char soc[32] = { '\0' };
	char com_val_meson[32] = {"amlogic, meson-"};
	char com_val_fb[32] = {"amlogic, fb-"};
	struct device_node *com_node = NULL;

	for (root_prop = root_node->properties; root_prop; root_prop = root_prop->next) {
		if (of_prop_cmp(root_prop->name, "compatible") == 0) {
			parse_soc((char *)root_prop->value, soc);

			strcat(com_val_meson, soc);
			com_node = of_find_compatible_node(NULL, NULL, com_val_meson);
			if (com_node) {
				irq_id = of_irq_get_byname(com_node, SRC_IRQ_NAME);
				goto exit;
			}

			strcat(com_val_fb, soc);
			com_node = of_find_compatible_node(NULL, NULL, com_val_fb);
			if (com_node) {
				irq_id = of_irq_get_byname(com_node, SRC_IRQ_NAME);
				goto exit;
			}
		}
	}

exit:
	if (irq_id <= 0) {
		pr_err("SOC: %s; node: %p; node compatible value: %s/%s; interrupt name: %s; interrupt id: %d;\n",
				soc, com_node, com_val_meson, com_val_fb,
				SRC_IRQ_NAME, irq_id);
		pr_err("not found %s interrupt\n", SRC_IRQ_NAME);
	}

	return irq_id;
}

int optee_wm_irq_register(void)
{
	uint32_t wm_sts = 0;
	int err_num = 0;

	wm_sts = check_wm_status();
	if (wm_sts) {
		pr_info("checking watermark status return 0x%08X\n", wm_sts);
		return -1;
	}

	g_irq_id = get_wm_irq_id();
	pr_info("%s interrupt id: %d\n", DST_IRQ_NAME, g_irq_id);

	err_num = request_irq(g_irq_id, &vsync_isr, IRQF_SHARED, DST_IRQ_NAME, (void *)&g_irq_id);
	if (err_num)
		pr_err("can't request %s interrupt for vsync, err_num = %d\n", DST_IRQ_NAME, -err_num);

	return -1;
}

void optee_wm_irq_free(void)
{
	if (g_irq_id > 0)
		free_irq(g_irq_id, (void *)&g_irq_id);
}
