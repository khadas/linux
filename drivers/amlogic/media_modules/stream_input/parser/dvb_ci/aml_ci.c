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
#include <linux/init.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/sysfs.h>
#include <linux/of.h>
#include "aml_ci.h"
//#include "aml_spi.h"
#include "aml_ci_bus.h"
//#include "cimax/aml_cimax.h"

//#include "dvb_ca_en50221.h"
#include <dvbdev.h>

MODULE_PARM_DESC(aml_ci_debug, "\n\t\t dvb ci debug");
static int aml_ci_debug = 1;
module_param(aml_ci_debug, int, S_IRUGO);

#define pr_dbg(args...)\
	do {\
		if (aml_ci_debug)\
			printk(args);\
	} while (0)
#define pr_error(fmt, args...) printk("DVBCI: " fmt, ## args)

/**\brief aml_ci_mem_read:mem read from cam
 * \param en50221: en50221 obj,used this data to get dvb_ci obj
 * \param slot: slot index
 * \param addr: read addr
 * \return
 *   - read value:ok
 *   - -EINVAL : error
 */
static int aml_ci_mem_read(struct dvb_ca_en50221_cimcu *en50221, int slot, int addr)
{
	struct aml_ci *ci = en50221->data;

	if (slot != 0) {
		pr_error("slot !=0 %s :%d\r\n", __func__, slot);
		return -EINVAL;
	}

	if (ci->ci_mem_read != NULL)
		return ci->ci_mem_read(ci, slot, addr);

	pr_error("ci_mem_read is null %s\r\n", __func__);
	return -EINVAL;
}
/**\brief aml_ci_mem_write:mem write to cam
 * \param en50221: en50221 obj,used this data to get dvb_ci obj
 * \param slot: slot index
 * \param addr: write addr
 * \param addr: write value
 * \return
 *   - 0:ok
 *   - -EINVAL : error
 */
static int aml_ci_mem_write(struct dvb_ca_en50221_cimcu *en50221,
		int slot, int addr, u8 data)
{

	struct aml_ci *ci = en50221->data;

	if (slot != 0) {
		pr_error("slot not 0 %s :%d\r\n", __func__, slot);
		return -EINVAL;
	}

	if (ci->ci_mem_write != NULL)
		return ci->ci_mem_write(ci, slot, addr, data);
	pr_error("ci_mem_write is null %s\r\n", __func__);
	return -EINVAL;
}
/**\brief aml_ci_io_read:io read from cam
 * \param en50221: en50221 obj,used this data to get dvb_ci obj
 * \param slot: slot index
 * \param addr: read addr
 * \return
 *   - read value:ok
 *   - -EINVAL : error
 */
static int aml_ci_io_read(struct dvb_ca_en50221_cimcu *en50221, int slot, u8 addr)
{
	struct aml_ci *ci = en50221->data;

	if (slot != 0) {
		pr_error("slot !=0 %s :%d\r\n", __func__, slot);
		return -EINVAL;
	}

	if (ci->ci_io_read != NULL)
		return ci->ci_io_read(ci, slot, addr);

	pr_error("ci_io_read is null %s\r\n", __func__);
	return -EINVAL;
}
/**\brief aml_ci_io_write:io write to cam
 * \param en50221: en50221 obj,used this data to get dvb_ci obj
 * \param slot: slot index
 * \param addr: write addr
 * \param addr: write value
 * \return
 *   - 0:ok
 *   - -EINVAL : error
 */
static int aml_ci_io_write(struct dvb_ca_en50221_cimcu *en50221,
		int slot, u8 addr, u8 data)
{
	struct aml_ci *ci = en50221->data;

	if (slot != 0) {
		pr_error("slot !=0 %s :%d\r\n", __func__, slot);
		return -EINVAL;
	}

	if (ci->ci_mem_write != NULL)
		return ci->ci_io_write(ci, slot, addr, data);

	pr_error("ci_io_write is null %s\r\n", __func__);
	return -EINVAL;
}
/**\brief aml_ci_slot_reset:reset slot
 * \param en50221: en50221 obj,used this data to get dvb_ci obj
 * \param slot: slot index
 * \return
 *   - 0:ok
 *   - -EINVAL : error
 */
static int aml_ci_slot_reset(struct dvb_ca_en50221_cimcu *en50221, int slot)
{
	struct aml_ci *ci = en50221->data;
	pr_dbg("Slot(%d): Slot RESET\n", slot);
	if (ci->ci_slot_reset != NULL) {
		ci->ci_slot_reset(ci, slot);
	} else {
		pr_error("ci_slot_reset is null %s\r\n", __func__);
		return -EINVAL;
	}
	return 0;
}
/**\brief aml_ci_slot_shutdown:show slot
 * \param en50221: en50221 obj,used this data to get dvb_ci obj
 * \param slot: slot index
 * \return
 *   - 0:ok
 *   - -EINVAL : error
 */
static int aml_ci_slot_shutdown(struct dvb_ca_en50221_cimcu *en50221, int slot)
{
	struct aml_ci *ci = en50221->data;
	pr_dbg("Slot(%d): Slot shutdown\n", slot);
	if (ci->ci_slot_shutdown != NULL) {
		ci->ci_slot_shutdown(ci, slot);
	} else {
		pr_error("aml_ci_slot_shutdown is null %s\r\n", __func__);
		return -EINVAL;
	}
	return 0;
}
/**\brief aml_ci_ts_control:control slot ts
 * \param en50221: en50221 obj,used this data to get dvb_ci obj
 * \param slot: slot index
 * \return
 *   - 0:ok
 *   - -EINVAL : error
 */
static int aml_ci_ts_control(struct dvb_ca_en50221_cimcu *en50221, int slot)
{

		struct aml_ci *ci = en50221->data;
		pr_dbg("Slot(%d): TS control\n", slot);
		if (ci->ci_slot_ts_enable != NULL) {
			ci->ci_slot_ts_enable(ci, slot);
		} else {
			pr_error("aml_ci_ts_control is null %s\r\n", __func__);
			return -EINVAL;
		}
		return 0;
}
/**\brief aml_ci_slot_status:get slot status
 * \param en50221: en50221 obj,used this data to get dvb_ci obj
 * \param slot: slot index
 * \param open: no used
 * \return
 *   - cam status
 *   - -EINVAL : error
 */
static int aml_ci_slot_status(struct dvb_ca_en50221_cimcu *en50221,
		int slot, int open)
{
	struct aml_ci *ci = en50221->data;

	//pr_dbg("Slot(%d): Poll Slot status\n", slot);

	if (ci->ci_poll_slot_status != NULL) {
		return ci->ci_poll_slot_status(ci, slot, open);
	} else {
		/*pr_error("ci_poll_slot_status is null %s\r\n", __func__);*/
	}

	return 0;
}
/**\brief aml_ci_slot_status:get slot status
 * \param en50221: en50221 obj,used this data to get dvb_ci obj
 * \param slot: slot index
 * \param open: no used
 * \return
 *   - cam status
 *   - -EINVAL : error
 */
static int aml_ci_slot_wakeup(struct dvb_ca_en50221_cimcu *en50221,
		int slot)
{
	struct aml_ci *ci = en50221->data;

	if (ci->ci_get_slot_wakeup != NULL) {
		return ci->ci_get_slot_wakeup(ci, slot);
	} else {
		/*pr_error("aml_ci_slot_wakeup is null %s\r\n", __func__);*/
	}

	return 1;
}

#if 0
static int aml_ci_cimax_slot_reset(struct dvb_ca_en50221_cimax *en50221,
		int slot)
{
	struct aml_ci *ci = en50221->data;
	pr_dbg("Slot(%d): Slot RESET\n", slot);
	if (ci->ci_slot_reset != NULL) {
		ci->ci_slot_reset(ci, slot);
	} else {
		pr_error("ci_slot_reset is null %s\r\n", __func__);
		return -EINVAL;
	}
	return 0;
}
/**\brief aml_ci_slot_shutdown:show slot
 * \param en50221: en50221 obj,used this data to get dvb_ci obj
 * \param slot: slot index
 * \return
 *   - 0:ok
 *   - -EINVAL : error
 */
static int aml_ci_cimax_slot_shutdown(struct dvb_ca_en50221_cimax *en50221,
		int slot)
{
	struct aml_ci *ci = en50221->data;
	pr_dbg("Slot(%d): Slot shutdown\n", slot);
	if (ci->ci_slot_shutdown != NULL) {
		ci->ci_slot_shutdown(ci, slot);
	} else {
		pr_error("aml_ci_slot_shutdown is null %s\r\n", __func__);
		return -EINVAL;
	}
	return 0;
}
/**\brief aml_ci_ts_control:control slot ts
 * \param en50221: en50221 obj,used this data to get dvb_ci obj
 * \param slot: slot index
 * \return
 *   - 0:ok
 *   - -EINVAL : error
 */
static int aml_ci_cimax_ts_control(struct dvb_ca_en50221_cimax *en50221,
		int slot)
{

	struct aml_ci *ci = en50221->data;
	pr_dbg("Slot(%d): TS control\n", slot);
	if (ci->ci_slot_ts_enable != NULL) {
		ci->ci_slot_ts_enable(ci, slot);
	} else {
		pr_error("aml_ci_ts_control is null %s\r\n", __func__);
		return -EINVAL;
	}
	return 0;
}
/**\brief aml_ci_cimax_slot_status:get slot status
 * \param en50221: en50221_cimax obj,used this data to get dvb_ci obj
 * \param slot: slot index
 * \param open: no used
 * \return
 *   - cam status
 *   - -EINVAL : error
 */
static int aml_ci_cimax_slot_status(
	struct dvb_ca_en50221_cimax *en50221, int slot, int open)
{
	struct aml_ci *ci = en50221->data;

	/*pr_dbg("Slot(%d): Poll Slot status\n", slot);*/

	if (ci->ci_poll_slot_status != NULL) {
		return ci->ci_poll_slot_status(ci, slot, open);
	} else {
		/*pr_error("ci_poll_slot_status is null %s\r\n", __func__);*/
	}

	return 0;
}

/**\brief aml_ci_read_cis: read cis
 * \param en50221_max: en50221 obj,used this data to get dvb_ci obj
 * \param slot: slot index
 * \param buf: buf for cis data
 * \param size: buf size
 * \return
 *   --EINVAL : error
 *   -        : actual size read
 */
static int aml_ci_read_cis(struct dvb_ca_en50221_cimax *en50221,
		int slot, u8 *buf, int size)
{
	struct aml_ci *ci = en50221->data;

	if (slot != 0) {
		pr_error("slot !=0 %s :%d\r\n", __func__, slot);
		return -EINVAL;
	}

	if (ci->ci_read_cis != NULL)
		return ci->ci_read_cis(ci, slot, buf, size);

	pr_error("ci_read_cis is null %s\r\n", __func__);
	return -EINVAL;
}
/**\brief aml_ci_write_cor: write cor
 * \param en50221_max: en50221 obj,used this data to get dvb_ci obj
 * \param slot: slot index
 * \param addr:
 * \param buf:
 * \return
 *   --EINVAL : error
 *   -0       : ok
 */
static int aml_ci_write_cor(struct dvb_ca_en50221_cimax *en50221,
		int slot, int address, u8 *buf)
{
	struct aml_ci *ci = en50221->data;

	if (slot != 0) {
		pr_error("slot !=0 %s :%d\r\n", __func__, slot);
		return -EINVAL;
	}

	if (ci->ci_write_cor != NULL)
		return ci->ci_write_cor(ci, slot, address, buf);

	pr_error("ci_write_cor is null %s\r\n", __func__);
	return -EINVAL;
}
/**\brief aml_ci_negociate: negotiate
 * \param en50221_max: en50221 obj,used this data to get dvb_ci obj
 * \param slot: slot index
 * \param size: suggested size
 * \return
 *   --EINVAL : error
 *   -        : size negotiated
 */
static int aml_ci_negotiate(struct dvb_ca_en50221_cimax *en50221,
		int slot, int size)
{
	struct aml_ci *ci = en50221->data;

	if (slot != 0) {
		pr_error("slot !=0 %s :%d\r\n", __func__, slot);
		return -EINVAL;
	}

	if (ci->ci_negotiate != NULL)
		return ci->ci_negotiate(ci, slot, size);

	pr_error("ci_negotiate is null %s\r\n", __func__);
	return -EINVAL;
}
/**\brief aml_ci_read_lpdu: read lpdu
 * \param en50221_max: en50221 obj,used this data to get dvb_ci obj
 * \param slot: slot index
 * \param buf:  buf
 * \param size: buf size
 * \return
 *   --EINVAL : error
 *   -        : size read
 */
static int aml_ci_read_lpdu(struct dvb_ca_en50221_cimax *en50221,
		int slot, u8 *buf, int size)
{
	struct aml_ci *ci = en50221->data;

	if (slot != 0) {
		pr_error("slot !=0 %s :%d\r\n", __func__, slot);
		return -EINVAL;
	}

	if (ci->ci_read_lpdu != NULL)
		return ci->ci_read_lpdu(ci, slot, buf, size);

	pr_error("ci_read_lpdu is null %s\r\n", __func__);
	return -EINVAL;
}

/**\brief aml_ci_write_lpdu: write lpdu
 * \param en50221_max: en50221 obj,used this data to get dvb_ci obj
 * \param slot: slot index
 * \param buf:  buf
 * \param size: write size
 * \return
 *   --EINVAL : error
 *   -        : size written
 */
static int aml_ci_write_lpdu(struct dvb_ca_en50221_cimax *en50221,
		int slot, u8 *buf, int size)
{
	struct aml_ci *ci = en50221->data;

	if (slot != 0) {
		pr_error("slot !=0 %s :%d\r\n", __func__, slot);
		return -EINVAL;
	}

	if (ci->ci_write_lpdu != NULL)
		return ci->ci_write_lpdu(ci, slot, buf, size);

	pr_error("ci_write_lpdu is null %s\r\n", __func__);
	return -EINVAL;
}

static int aml_ci_read_cam_status(struct dvb_ca_en50221_cimax *en50221,
		int slot)
{
	struct aml_ci *ci = en50221->data;

	if (slot != 0) {
		pr_error("slot !=0 %s :%d\r\n", __func__, slot);
		return -EINVAL;
	}

	if (ci->ci_read_cam_status != NULL)
		return ci->ci_read_cam_status(ci, slot);

	pr_error("ci_read_cam_status is null %s\r\n", __func__);
	return -EINVAL;
}

static int aml_ci_cam_reset(struct dvb_ca_en50221_cimax *en50221, int slot)
{
	struct aml_ci *ci = en50221->data;

	if (slot != 0) {
		pr_error("slot !=0 %s :%d\r\n", __func__, slot);
		return -EINVAL;
	}

	if (ci->ci_cam_reset != NULL)
		return ci->ci_cam_reset(ci, slot);

	pr_error("ci_cam_reset is null %s\r\n", __func__);
	return -EINVAL;
}

/**\brief aml_ci_get_capbility
 * \param en50221_max: en50221 obj,used this data to get dvb_ci obj
 * \param slot: slot index
 * \return
 *   -        : capbilities
 */
static int aml_ci_get_capbility(struct dvb_ca_en50221_cimax *en50221, int slot)
{
	struct aml_ci *ci = en50221->data;

	if (slot != 0) {
		pr_error("slot !=0 %s :%d\r\n", __func__, slot);
		return 0;
	}

	if (ci->ci_get_capbility != NULL)
		return ci->ci_get_capbility(ci, slot);

	pr_error("ci_get_capbility is null %s\r\n", __func__);
	return 0;
}
#endif

/**\brief get ci config from dts
 * \param np: device node
 * \return
 *   - 0 成功
 *   - 其他值 :
 */
static int aml_ci_get_config_from_dts(struct platform_device *pdev,
		struct aml_ci *ci)
{
	char buf[32];
	int ret = 0;
	int value;

	snprintf(buf, sizeof(buf), "%s", "io_type");
	ret = of_property_read_u32(pdev->dev.of_node, buf, &value);
	if (!ret) {
		pr_dbg("%s: 0x%x\n", buf, value);
		ci->io_type = value;
	}
	snprintf(buf, sizeof(buf), "%s", "raw_mode");
	ret = of_property_read_u32(pdev->dev.of_node, buf, &value);
	if (!ret) {
		pr_dbg("%s: 0x%x\n", buf, value);
		ci->raw_mode = value;
	}
	return 0;
}

/**\brief aml_ci_init:ci dev init
 * \param pdev: platform_device device node,used to get dts info
 * \param dvb: aml_dvb obj,used to get dvb_adapter for en0211 to use
 * \param cip: ci_dev pp
 * \return
 *   - 0 成功
 *   - 其他值 :
 */
int aml_ci_init(struct platform_device *pdev,
	struct dvb_adapter *dvb_adapter, struct aml_ci **cip)
{
	struct aml_ci *ci = NULL;
	int ca_flags = 0, result;

	ci = kzalloc(sizeof(struct aml_ci), GFP_KERNEL);
	if (!ci) {
		pr_error("Out of memory!, exiting ..\n");
		result = -ENOMEM;
		goto err;
	}
	ci->id = 0;
	aml_ci_get_config_from_dts(pdev, ci);

//	ci->priv		= dvb;
	/* register CA interface */
#if 0
	if (ci->io_type == AML_DVB_IO_TYPE_CIMAX) {
		ci->en50221_cimax.owner = THIS_MODULE;
		ci->en50221_cimax.read_cis = aml_ci_read_cis;
		ci->en50221_cimax.write_cor = aml_ci_write_cor;
		ci->en50221_cimax.negotiate = aml_ci_negotiate;
		ci->en50221_cimax.read_lpdu = aml_ci_read_lpdu;
		ci->en50221_cimax.write_lpdu = aml_ci_write_lpdu;
		ci->en50221_cimax.read_cam_status = aml_ci_read_cam_status;
		ci->en50221_cimax.cam_reset = aml_ci_cam_reset;
		ci->en50221_cimax.get_capbility = aml_ci_get_capbility;
		ci->en50221_cimax.slot_reset = aml_ci_cimax_slot_reset;
		ci->en50221_cimax.slot_shutdown = aml_ci_cimax_slot_shutdown;
		ci->en50221_cimax.slot_ts_enable = aml_ci_cimax_ts_control;
		ci->en50221_cimax.poll_slot_status = aml_ci_cimax_slot_status;
		ci->en50221_cimax.data = ci;

		pr_dbg("Registering EN50221 CIMAX device\n");
		result = dvb_ca_en50221_cimax_init(dvb_adapter,
			&ci->en50221_cimax, ca_flags, 1);
		if (result != 0) {
			pr_error("EN50221 CIMAX: Initialization failed <%d>\n",
				result);
			goto err;
		}
	} else
#endif
	{
		ca_flags		= ~DVB_CA_EN50221_FLAG_IRQ_CAMCHANGE;
		/* register CA interface */
		ci->en50221_cimcu.owner		= THIS_MODULE;
		ci->en50221_cimcu.read_attribute_mem	= aml_ci_mem_read;
		ci->en50221_cimcu.write_attribute_mem	= aml_ci_mem_write;
		ci->en50221_cimcu.read_cam_control	= aml_ci_io_read;
		ci->en50221_cimcu.write_cam_control	= aml_ci_io_write;
		ci->en50221_cimcu.slot_reset		= aml_ci_slot_reset;
		ci->en50221_cimcu.slot_shutdown	= aml_ci_slot_shutdown;
		ci->en50221_cimcu.slot_ts_enable	= aml_ci_ts_control;
		ci->en50221_cimcu.poll_slot_status	= aml_ci_slot_status;
		ci->en50221_cimcu.get_slot_wakeup	= aml_ci_slot_wakeup;

		ci->en50221_cimcu.data		= ci;

		if (ci->raw_mode == 0) {
			pr_dbg("Registering EN50221 device\n");
			result = dvb_ca_en50221_cimcu_init(dvb_adapter,
				&ci->en50221_cimcu, ca_flags, 1);
			if (result != 0) {
				pr_error("EN50221_cimcu: Initialization failed <%d>\n",
					result);
				goto err;
			}
		}
	}
	*cip = ci;
	pr_dbg("Registered EN50221 device\n");
#if 0
	if (ci->io_type == AML_DVB_IO_TYPE_SPI || ci->io_type == AML_DVB_IO_TYPE_SPI_T312) {
		/* spi init */
		ci->ci_init = aml_spi_init;
		ci->ci_exit = aml_spi_exit;
	} else if (ci->io_type == AML_DVB_IO_TYPE_CIMAX) {
		ci->ci_init = aml_cimax_init;
		ci->ci_exit = aml_cimax_exit;
	} else
#endif
	if (ci->io_type == AML_DVB_IO_TYPE_CIBUS) {
		ci->ci_init = aml_ci_bus_init;
		ci->ci_exit = aml_ci_bus_exit;
	} else {
		/* no io dev init,is error */
		pr_dbg("unknown io type, please check io_type in dts file\r\n");
	}

	if (ci->ci_init)
		result = ci->ci_init(pdev, ci);
#if 0
	if (ci->io_type == AML_DVB_IO_TYPE_CIMAX) {
		if (result)
			dvb_ca_en50221_cimax_release(&ci->en50221_cimax);
	}
#endif
	return result;
err:
	kfree(ci);
	return result;
}

void aml_ci_exit(struct aml_ci *ci)
{
	pr_dbg("Unregistering EN50221 device\n");
	if (ci) {
#if 0
		if (ci->io_type == AML_DVB_IO_TYPE_CIMAX)
			dvb_ca_en50221_cimax_release(&ci->en50221_cimax);
		else
#endif
		if (ci->raw_mode == 0)
			dvb_ca_en50221_cimcu_release(&ci->en50221_cimcu);
		if (ci->ci_exit)
			ci->ci_exit(ci);
		kfree(ci);
	}
}

static struct aml_ci *ci_dev;

static ssize_t ts_show(struct class *class,
	struct class_attribute *attr, char *buf)
{
		int ret;
		ret = sprintf(buf, "ts%d\n", 1);
	return ret;
}
static CLASS_ATTR_RO(ts);

static struct attribute *aml_ci_attrs[] = {
	&class_attr_ts.attr,
	NULL
};

ATTRIBUTE_GROUPS(aml_ci);

static int aml_ci_register_class(struct aml_ci *ci)
{
	#define CLASS_NAME_LEN 48
	int ret;
	struct class *clp;

	clp = &(ci->class);

	clp->name = kzalloc(CLASS_NAME_LEN, GFP_KERNEL);
	if (!clp->name)
		return -ENOMEM;

	snprintf((char *)clp->name, CLASS_NAME_LEN, "amlci-%d", ci->id);
	clp->owner = THIS_MODULE;
	clp->class_groups = aml_ci_groups;
	ret = class_register(clp);
	if (ret)
		kfree(clp->name);

	return 0;
}

static int aml_ci_unregister_class(struct aml_ci *ci)
{
	class_unregister(&ci->class);
	kzfree(ci->class.name);
	return 0;
}

static struct dvb_adapter *aml_get_dvb_adapter(void)
{
	return NULL;
}

static int aml_ci_probe(struct platform_device *pdev)
{
	struct dvb_adapter *dvb_adapter;
	int err = 0;
	typeof(&aml_get_dvb_adapter) __a;

	__a = symbol_request(aml_get_dvb_adapter);
	if (__a) {
		dvb_adapter = (struct dvb_adapter *)__a();
		if (!dvb_adapter) {
			pr_dbg("aml_get_dvb_adapter is null\n");
			symbol_put_addr(aml_get_dvb_adapter);
			return 0;
		}
	} else {
		pr_dbg("load aml_get_dvb_adapter fail\n");
		return 0;
	}
	pr_dbg("---Amlogic CI Init---[%p]\n", dvb_adapter);

	err = aml_ci_init(pdev, dvb_adapter, &ci_dev);
	if (err < 0) {
		symbol_put_addr(aml_get_dvb_adapter);
		return err;
	}
	platform_set_drvdata(pdev, ci_dev);
	aml_ci_register_class(ci_dev);
#if 0
	if (ci_dev->io_type == AML_DVB_IO_TYPE_SPI ||
		ci_dev->io_type == AML_DVB_IO_TYPE_SPI_T312)
			aml_spi_mod_init();
#endif
	return 0;
}

static int aml_ci_remove(struct platform_device *pdev)
{
	aml_ci_unregister_class(ci_dev);
	platform_set_drvdata(pdev, NULL);
#if 0
	if (ci_dev->io_type == AML_DVB_IO_TYPE_SPI ||
		ci_dev->io_type == AML_DVB_IO_TYPE_SPI_T312) {
			aml_spi_exit(ci_dev);
			aml_spi_mod_exit();
		}
	else if (ci_dev->io_type == AML_DVB_IO_TYPE_CIMAX)
		aml_cimax_exit(ci_dev);
	else
#endif
	if (ci_dev->io_type == AML_DVB_IO_TYPE_CIBUS)
		aml_ci_bus_exit(ci_dev);
	else
		pr_dbg("---Amlogic CI remove unkown io type---\n");

	aml_ci_exit(ci_dev);
	symbol_put_addr(aml_get_dvb_adapter);
	return 0;
}

static int aml_ci_suspend(struct platform_device *pdev, pm_message_t state)
{
	pr_dbg("Amlogic CI Suspend!\n");
#if 0
	if (ci_dev->io_type == AML_DVB_IO_TYPE_SPI ||
		ci_dev->io_type == AML_DVB_IO_TYPE_SPI_T312) {
		  aml_spi_exit(ci_dev);
		}
	else if (ci_dev->io_type == AML_DVB_IO_TYPE_CIMAX)
		aml_cimax_exit(ci_dev);
	else
#endif
	if (ci_dev->io_type == AML_DVB_IO_TYPE_CIBUS)
		aml_ci_bus_exit(ci_dev);
	else
		pr_dbg("---Amlogic CI remove unkown io type---\n");

	return 0;
}

static int aml_ci_resume(struct platform_device *pdev)
{
	int err = 0;
	pr_dbg("Amlogic CI Resume!\n");
#if 0
	if (ci_dev->io_type == AML_DVB_IO_TYPE_SPI ||
		ci_dev->io_type == AML_DVB_IO_TYPE_SPI_T312) {
		  aml_spi_init(pdev, ci_dev);
		}
	else if (ci_dev->io_type == AML_DVB_IO_TYPE_CIMAX)
		aml_cimax_init(pdev, ci_dev);
	else
#endif
	if (ci_dev->io_type == AML_DVB_IO_TYPE_CIBUS)
		aml_ci_bus_init(pdev, ci_dev);
	else
		pr_dbg("---Amlogic CI remove unkown io type---\n");
	return err;
}

static const struct of_device_id dvbci_dev_dt_match[] = {
	{
		.compatible = "amlogic, dvbci",
	},
	{},
};



static struct platform_driver aml_ci_driver = {
	.probe		= aml_ci_probe,
	.remove		= aml_ci_remove,
	.suspend        = aml_ci_suspend,
	.resume         = aml_ci_resume,
	.driver		= {
		.name	= "dvbci",
		.of_match_table = dvbci_dev_dt_match,
		.owner	= THIS_MODULE,
	}
};

static int  aml_ci_mod_init(void)
{
	pr_dbg("Amlogic CI mode init\n");
	return platform_driver_register(&aml_ci_driver);
}

static void  aml_ci_mod_exit(void)
{
	pr_dbg("Amlogic CI mode Exit\n");
	platform_driver_unregister(&aml_ci_driver);
}

module_init(aml_ci_mod_init);
module_exit(aml_ci_mod_exit);
MODULE_LICENSE("GPL");

