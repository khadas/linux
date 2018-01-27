#include <linux/init.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/string.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/spinlock.h>
#include <linux/fcntl.h>
#include <asm/irq.h>
#include <linux/uaccess.h>
#include <linux/poll.h>
#include <linux/delay.h>
#include <linux/delay.h>
#include <asm/cacheflush.h>
#include <linux/dma-mapping.h>
#include <linux/signal.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/spi/spi.h>
#include "aml_ci.h"
#include "aml_spi.h"

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
static int aml_ci_mem_read(struct dvb_ca_en50221 *en50221, int slot, int addr)
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
static int aml_ci_mem_write(struct dvb_ca_en50221 *en50221,
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
static int aml_ci_io_read(struct dvb_ca_en50221 *en50221, int slot, u8 addr)
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
static int aml_ci_io_write(
	struct dvb_ca_en50221 *en50221, int slot, u8 addr, u8 data)
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
static int aml_ci_slot_reset(struct dvb_ca_en50221 *en50221, int slot)
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
static int aml_ci_slot_shutdown(struct dvb_ca_en50221 *en50221, int slot)
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
static int aml_ci_ts_control(struct dvb_ca_en50221 *en50221, int slot)
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
static int aml_ci_slot_status(
	struct dvb_ca_en50221 *en50221, int slot, int open)
{
	struct aml_ci *ci = en50221->data;

	pr_dbg("Slot(%d): Poll Slot status\n", slot);

	if (ci->ci_poll_slot_status != NULL) {
		return ci->ci_poll_slot_status(ci, slot, open);
	} else {
		pr_error("ci_poll_slot_status is null %s\r\n", __func__);
		return -EINVAL;
	}

	return -EINVAL;
}

/**\brief get ci config from dts
 * \param np: device node
 * \return
 *   - 0 成功
 *   - 其他值 :
 */
static int aml_ci_get_config_from_dts(
	struct platform_device *pdev, struct aml_ci *ci)
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
	struct aml_dvb *dvb, struct aml_ci **cip)
{
	struct dvb_adapter *dvb_adapter	= &dvb->dvb_adapter;
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

	ci->priv		= dvb;
	ca_flags		= DVB_CA_EN50221_FLAG_IRQ_CAMCHANGE;
	/* register CA interface */
	ci->en50221.owner		= THIS_MODULE;
	ci->en50221.read_attribute_mem	= aml_ci_mem_read;
	ci->en50221.write_attribute_mem	= aml_ci_mem_write;
	ci->en50221.read_cam_control	= aml_ci_io_read;
	ci->en50221.write_cam_control	= aml_ci_io_write;
	ci->en50221.slot_reset		= aml_ci_slot_reset;
	ci->en50221.slot_shutdown	= aml_ci_slot_shutdown;
	ci->en50221.slot_ts_enable	= aml_ci_ts_control;
	ci->en50221.poll_slot_status	= aml_ci_slot_status;
	ci->en50221.data		= ci;


	pr_dbg("Registering EN50221 device\n");
	result = dvb_ca_en50221_init(dvb_adapter, &ci->en50221, ca_flags, 1);
	if (result != 0) {
		pr_error("EN50221: Initialization failed <%d>\n", result);
		goto err;
	}
	*cip = ci;
	pr_dbg("Registered EN50221 device\n");
	if (ci->io_type == AML_DVB_IO_TYPE_SPI) {
		/* spi init */
		aml_spi_init(pdev, ci);
	} else {
		/* no io dev init,is error */
		pr_dbg("io dev no init,we do not kown use spi or iobus or other,please check io_type in dts file\r\n");
	}
	return 0;
err:
	/* for init spi */
	aml_spi_exit();
	kfree(ci);
	return result;
}
EXPORT_SYMBOL_GPL(aml_ci_init);

void aml_ci_exit(struct aml_ci *ci)
{
	pr_dbg("Unregistering EN50221 device\n");
	if (ci) {
		dvb_ca_en50221_release(&ci->en50221);
		kfree(ci);
	}
}
EXPORT_SYMBOL_GPL(aml_ci_exit);

static struct aml_ci *ci_dev;

static ssize_t aml_ci_ts_show(struct class *class,
struct class_attribute *attr, char *buf)
{
		int ret;
		ret = sprintf(buf, "ts%d\n", 1);
	return ret;
}

static struct class_attribute amlci_class_attrs[] = {
	__ATTR(ts,  S_IRUGO | S_IWUSR, aml_ci_ts_show, NULL),
	__ATTR_NULL
};

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
	clp->class_attrs = amlci_class_attrs;
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


static int aml_ci_probe(struct platform_device *pdev)
{
	struct aml_dvb *dvb = aml_get_dvb_device();
	int err = 0;
	pr_dbg("---Amlogic CI Init---\n");
	err = aml_ci_init(pdev, dvb, &ci_dev);
	if (err < 0)
		return err;
	platform_set_drvdata(pdev, ci_dev);
	aml_ci_register_class(ci_dev);
	return 0;
}

static int aml_ci_remove(struct platform_device *pdev)
{
	aml_ci_unregister_class(ci_dev);
	platform_set_drvdata(pdev, NULL);
	if (ci_dev->io_type == AML_DVB_IO_TYPE_SPI)
		aml_spi_exit();
	else
	  pr_dbg("---Amlogic CI remove unkown io type---\n");

	aml_ci_exit(ci_dev);
	return 0;
}

static int aml_ci_suspend(struct platform_device *pdev, pm_message_t state)
{
	pr_dbg("Amlogic CI Suspend!\n");
	if (ci_dev->io_type == AML_DVB_IO_TYPE_SPI)
		aml_spi_exit();
	else
	  pr_dbg("---Amlogic CI remove unkown io type---\n");

	return 0;
}

static int aml_ci_resume(struct platform_device *pdev)
{
	int err = 0;
	pr_dbg("Amlogic CI Resume!\n");
	if (ci_dev->io_type == AML_DVB_IO_TYPE_SPI)
		aml_spi_init(pdev, ci_dev);
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

static int __init aml_ci_mod_init(void)
{
	pr_dbg("Amlogic CI mode init\n");
	return platform_driver_register(&aml_ci_driver);
}

static void __exit aml_ci_mod_exit(void)
{
	pr_dbg("Amlogic CI mode Exit\n");
	platform_driver_unregister(&aml_ci_driver);
}

module_init(aml_ci_mod_init);
module_exit(aml_ci_mod_exit);

MODULE_LICENSE("GPL");


