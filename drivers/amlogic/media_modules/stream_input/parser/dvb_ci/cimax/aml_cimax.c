/*
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Description:
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/kthread.h>
#include <linux/of.h>
#include "../aml_ci.h"
#include "aml_cimax.h"
#include "aml_cimax_spi.h"
#include "aml_cimax_usb.h"

#define MODUDLE_NAME       "aml_cimax"

MODULE_PARM_DESC(cimax_debug, "enable verbose debug messages");
static int aml_cimax_debug = 1;
module_param_named(cimax_debug, aml_cimax_debug, int, 0644);

//static struct switch_dev slot_state = {
//	.name = "ci_slot",
//};

#define pr_dbg(fmt...)\
	do {\
		if (aml_cimax_debug)\
			pr_info("AML_CIMAX: " fmt);\
	} while (0)
#define pr_error(fmt...) pr_err("AML_CIMAX: " fmt)

static int aml_cimax_slot_reset(struct aml_ci *ci, int slot)
{
	int ret = 0;
	struct aml_cimax *cimax = ci->data;
	pr_dbg("cimax: slot(%d) reset\n", slot);
	if (cimax->ops.slot_reset)
		ret = cimax->ops.slot_reset(cimax, slot);
	return ret;
}

static int aml_cimax_slot_shutdown(struct aml_ci *ci, int slot)
{
	pr_dbg("slot(%d) shutdown\n", slot);
	return 0;
}

static int aml_cimax_slot_ts_enable(struct aml_ci *ci, int slot)
{
	pr_dbg("slot(%d) ts control\n", slot);
	return 0;
}

static int aml_cimax_slot_status(struct aml_ci *ci, int slot, int open)
{
	int ret = 0;
	struct aml_cimax *cimax = ci->data;

	/*pr_dbg("cimax: slot(%d) poll\n", slot);*/
	if (cimax->ops.slot_status)
		ret = cimax->ops.slot_status(cimax, slot);
	return ret;
}

#define DEF_FUNC_WRAPPER3(_pre, _fn, _S, _P1, _P2, _P3) \
static int _pre##_fn(_S s, _P1 p1, _P2 p2, _P3 p3)\
{\
	struct aml_cimax *cimax = s->data;\
	/*pr_dbg("%s\n", #_fn);*/\
	if (cimax->ops._fn)\
		return cimax->ops._fn(cimax, p1, p2, p3);\
	return 0;\
}

/*DEF_FUNC_WRAPPER3(aml_cimax_, read_reg, struct aml_ci*, int, u8*, int)*/
/*DEF_FUNC_WRAPPER3(aml_cimax_, write_reg, struct aml_ci*, int, u8*, int)*/
DEF_FUNC_WRAPPER3(aml_cimax_, read_cis, struct aml_ci*, int, u8*, int)
DEF_FUNC_WRAPPER3(aml_cimax_, read_lpdu, struct aml_ci*, int, u8*, int)
DEF_FUNC_WRAPPER3(aml_cimax_, write_lpdu, struct aml_ci*, int, u8*, int)

static int aml_cimax_write_cor(struct aml_ci *ci, int slot, int addr, u8 *buf)
{
	struct aml_cimax *cimax = ci->data;
	pr_dbg("write_cor\n");
	if (cimax->ops.write_cor)
		return cimax->ops.write_cor(cimax, slot, addr, buf);
	return 0;
}

static int aml_cimax_negotiate(struct aml_ci *ci, int slot, int size)
{
	struct aml_cimax *cimax = ci->data;
	pr_dbg("negotiate\n");
	if (cimax->ops.negotiate)
		return cimax->ops.negotiate(cimax, slot, size);
	return 0;
}

static int aml_cimax_read_cam_status(struct aml_ci *ci, int slot)
{
	struct aml_cimax *cimax = ci->data;
	if (cimax->ops.read_cam_status)
		return cimax->ops.read_cam_status(cimax, slot);
	return 0;
}

static int aml_cimax_cam_reset(struct aml_ci *ci, int slot)
{
	struct aml_cimax *cimax = ci->data;
	if (cimax->ops.cam_reset)
		return cimax->ops.cam_reset(cimax, slot);
	return 0;
}

static int aml_cimax_get_capbility(struct aml_ci *ci, int slot)
{
	return 0;
}

int aml_cimax_camchanged(struct aml_cimax *cimax, int slot, int plugin)
{
	struct aml_ci *ci = cimax->ci;
	if (plugin) {
		dvb_ca_en50221_cimax_camchange_irq(&ci->en50221_cimax,
			slot, DVB_CA_EN50221_CAMCHANGE_INSERTED);
	} else {
		dvb_ca_en50221_cimax_camchange_irq(&ci->en50221_cimax,
			slot, DVB_CA_EN50221_CAMCHANGE_REMOVED);
	}
	return 0;
}

static int aml_cimax_start(struct aml_cimax *cimax)
{
	int ret = 0;
	if (cimax->ops.start)
		ret = cimax->ops.start(cimax);
	return ret;
}

static int aml_cimax_stop(struct aml_cimax *cimax)
{
	int ret = 0;
	if (cimax->ops.stop)
		ret = cimax->ops.stop(cimax);
	return ret;
}

static int aml_cimax_get_config_from_dts(struct aml_cimax *cimax)
{
	struct device_node *child = NULL;
	struct platform_device *pdev = cimax->pdev;
	struct device_node *np = pdev->dev.of_node;
	unsigned int val;
	int ret = 0;
	pr_dbg("get cimax dts\n");

	child = of_get_child_by_name(np, "cimax");
	if (child == NULL) {
		pr_error("failed to get cimax\n");
		return -1;
	}
	ret = of_property_read_u32(child, "io_type", &val);
	if (ret)
		cimax->io_type = IO_TYPE_SPI;
	else
		cimax->io_type = val;

	return 0;
}

int aml_cimax_init(struct platform_device *pdev, struct aml_ci *ci)
{
	struct aml_cimax *cimax = NULL;
	int ret = 0;

	cimax = kzalloc(sizeof(struct aml_cimax), GFP_KERNEL);
	if (!cimax) {
		pr_error("Out of memory!, exiting ..\n");
		return -ENOMEM;
	}
	cimax->pdev = pdev;
	cimax->ci = ci;

	aml_cimax_get_config_from_dts(cimax);

	if (cimax->io_type == IO_TYPE_SPI) {
		//ret = aml_cimax_spi_init(pdev, cimax);
	}
	else {
		ret = aml_cimax_usb_init(pdev, cimax);
	}

	if (ret != 0) {
		kfree(cimax);
		cimax = NULL;
		return -EIO;
	}

	ret = aml_cimax_start(cimax);
	if (ret != 0)
		return ret;

	ci->data = cimax;

	ci->ci_read_cis = aml_cimax_read_cis;
	ci->ci_write_cor = aml_cimax_write_cor;
	ci->ci_negotiate = aml_cimax_negotiate;
	ci->ci_read_lpdu = aml_cimax_read_lpdu;
	ci->ci_write_lpdu = aml_cimax_write_lpdu;
	ci->ci_read_cam_status = aml_cimax_read_cam_status;
	ci->ci_cam_reset = aml_cimax_cam_reset;
	ci->ci_get_capbility = aml_cimax_get_capbility;
	ci->ci_slot_reset = aml_cimax_slot_reset;
	ci->ci_slot_shutdown = aml_cimax_slot_shutdown;
	ci->ci_slot_ts_enable = aml_cimax_slot_ts_enable;
	ci->ci_poll_slot_status = aml_cimax_slot_status;

	return 0;
}
EXPORT_SYMBOL(aml_cimax_init);

int aml_cimax_exit(struct aml_ci *ci)
{
	struct aml_cimax *cimax = ci->data;

	ci->ci_read_cis         = NULL;
	ci->ci_write_cor        = NULL;
	ci->ci_negotiate        = NULL;
	ci->ci_read_lpdu        = NULL;
	ci->ci_write_lpdu       = NULL;
	ci->ci_read_cam_status  = NULL;
	ci->ci_cam_reset        = NULL;
	ci->ci_get_capbility    = NULL;
	ci->ci_slot_reset       = NULL;
	ci->ci_slot_shutdown    = NULL;
	ci->ci_slot_ts_enable   = NULL;
	ci->ci_poll_slot_status = NULL;

	aml_cimax_stop(cimax);

	if (cimax->io_type == IO_TYPE_SPI) {
        //aml_cimax_spi_exit(cimax);
	}
	else {
		aml_cimax_usb_exit(cimax);
	}
	kfree(cimax);
	ci->data = NULL;

	return 0;
}
EXPORT_SYMBOL(aml_cimax_exit);

int aml_cimax_slot_state_changed(struct aml_cimax *cimax, int slot, int state)
{
	//if (slot == 0)
	//	switch_set_state(&slot_state, state);
	return 0;
}
EXPORT_SYMBOL(aml_cimax_slot_state_changed);
#if 0
static int __init aml_cimax_mod_init(void)
{
	pr_dbg("Amlogic DVB CIMAX Init\n");
	//switch_dev_register(&slot_state);
	//switch_set_state(&slot_state, 0);
	return 0;
}

static void __exit aml_cimax_mod_exit(void)
{
	pr_dbg("Amlogic DVB CIMAX Exit\n");
	//switch_dev_unregister(&slot_state);
}

module_init(aml_cimax_mod_init);
module_exit(aml_cimax_mod_exit);

MODULE_LICENSE("GPL");
#endif

