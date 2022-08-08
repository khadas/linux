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
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/amlogic/aml_gpio_consumer.h>
#include <linux/gpio/consumer.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/of_irq.h>
#include <linux/irq.h>
#include <linux/compat.h>
#include "aml_ci_bus.h"
#include "aml_ci.h"
#include "amci.h"


//can see jtag dts and driver to select gpio function.
//write dts config for cam/tsin/out
//gpio irq is can used.
//
static struct aml_ci_bus ci_bus;
static int aml_ci_bus_debug = 1;
static int aml_ci_bus_time = 500;
static int aml_ci_bus_set_delay = 0;

static DECLARE_WAIT_QUEUE_HEAD(wq);
static u32 fetch_done;

module_param_named(ci_bus_debug, aml_ci_bus_debug, int, 0644);
MODULE_PARM_DESC(ci_bus_debug, "enable verbose debug messages");

module_param_named(ci_bus_set_delay, aml_ci_bus_set_delay, int, 0644);
MODULE_PARM_DESC(ci_bus_set_delay, "enable ci bus delay set");

module_param_named(ci_bus_time, aml_ci_bus_time, int, 0644);
MODULE_PARM_DESC(ci_bus_time, "set ci bus time");


#define pr_dbg(args...)\
	do {\
		if (aml_ci_bus_debug)\
			printk(args);\
	} while (0)
#define pr_error(fmt, args...) printk("AML_CI_BUS: " fmt, ## args)


#define INPUT 0
#define OUTPUT 1
#define OUTLEVEL_LOW 0
#define OUTLEVEL_HIGH 1
#define PULLLOW 1
#define PULLHIGH 0

#define AML_MODE_NAME       "aml_dvbci_bus"

int  aml_ci_bus_mod_init(void);
void  aml_ci_bus_mod_exit(void);
static  int aml_read_self(unsigned int reg);
static void aml_write_self(unsigned int reg, unsigned int val);
static int aml_set_gpio_out(struct gpio_desc *gpio, int val);
static int aml_set_gpio_in(struct gpio_desc *gpio);

#define WRITE_CIBUS_REG(_r, _v)   aml_write_self(_r, _v)
#define READ_CIBUS_REG(_r)        aml_read_self(_r)

#define USED_IRQ  0

static void *p_hw_base;
//write reg
static void aml_write_self(unsigned int reg, unsigned int val)
{
	void *ptr = (void *)(p_hw_base + reg);
	writel(val, ptr);
}
//read reg
static  int aml_read_self(unsigned int reg)
{
	void *addr = p_hw_base + reg;
	int ret = readl(addr);
	return ret;
}

/**\brief init_ci_addr:ci bus init mem addr
* \param pdev: 
* \return
*   - read value:ok
*   - -EINVAL : error
*/
int init_ci_addr(struct platform_device *pdev)
{
	struct resource *res;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		pr_dbg("%s fail\n", __func__);
		return -1;
	}

	p_hw_base = devm_ioremap_nocache(&pdev->dev, res->start,
					 resource_size(res));
	if (p_hw_base) {
		pr_dbg("%s base addr = %lx\n", __func__,
		       (unsigned long)p_hw_base);
	} else {
		pr_dbg("%s base addr error\n", __func__);
	}
	return 0;
}


/**\brief aml_ci_bus_io:ci bus read or write api with bus
* \param ci_bus_dev: ci_bus_dev obj,used this data to ctl
* \param val: read or write value
* \param addr: rw addr
* \param mode: cmd
* \return
*   - read value:ok
*   - -EINVAL : error
*/
static int aml_ci_bus_select_gpio(struct aml_ci_bus *ci_bus_dev,
				int select/*, int enable*/)
{
	//SELECT GPIO FUNCTION
	unsigned int old_select = ci_bus_dev->select;
	struct pinctrl_state *s;
	int ret = 0;

	if (old_select == select)
		return 0;

	if (ci_bus_dev->addr_ts_mode_multiplex == 0) {
		//not multiplex ts and ci addr, so no need to
		//change ts and addr
		if (select == AML_GPIO_ADDR) {
			return 0;
		}
	}
	if (!ci_bus_dev->pinctrl) {
		ci_bus_dev->pinctrl = devm_pinctrl_get(&ci_bus_dev->pdev->dev);
		if (IS_ERR_OR_NULL(ci_bus_dev->pinctrl)) {
			dev_err(&ci_bus_dev->pdev->dev, "could not get pinctrl handle\n");
			return -EINVAL;
		}
	}
	if (IS_ERR_OR_NULL(ci_bus_dev->pinctrl)) {
			dev_err(&ci_bus_dev->pdev->dev, "return, could not get pinctrl handle\n");
			return -EINVAL;
	}
	/* set pinmux */
	switch (select) {
		case AML_GPIO_ADDR:
			s = pinctrl_lookup_state(ci_bus_dev->pinctrl, "ci_addr_pins");
			if (IS_ERR_OR_NULL(s)) {
				dev_err(&ci_bus_dev->pdev->dev,
						"could not get ci_addr_pins state\n");
				return -EINVAL;
			}
			ret = pinctrl_select_state(ci_bus_dev->pinctrl, s);
			if (ret) {
				dev_err(&ci_bus_dev->pdev->dev, "failed to set ci_addr_pins pinctrl\n");
				return -EINVAL;
			}
			if (ci_bus_dev->le_pin) {
				aml_set_gpio_out(ci_bus_dev->le_pin, ci_bus_dev->le_enable_level);
				pr_dbg("set le pin to low");
			}
			break;
		case AML_GPIO_TS:
			if (ci_bus_dev->le_pin) {
				aml_set_gpio_out(ci_bus_dev->le_pin, !ci_bus_dev->le_enable_level);
				pr_dbg("set le pin to high");
			}
			s = pinctrl_lookup_state(ci_bus_dev->pinctrl, "ci_ts_pins");
			if (IS_ERR_OR_NULL(s)) {
				dev_err(&ci_bus_dev->pdev->dev,
						"could not get ci_ts_pins state\n");
				return -EINVAL;
			}
			ret = pinctrl_select_state(ci_bus_dev->pinctrl, s);
			if (ret) {
				dev_err(&ci_bus_dev->pdev->dev, "failed to set ci_ts_pins pinctrl\n");
				return -EINVAL;
			}
			break;
		default:
			break;
	}
    ci_bus_dev->select = select;
	return 0;
}

/**\brief aml_ci_bus_set_delay_time:set ci bus delay time
* \param mode: cmd,io rd/wr or mem rd/wr
* \return
*   - read value:ok
*   - -EINVAL : error
*/
static int aml_ci_bus_set_delay_time(int mode)
{
	u32 delay0 = 0, delay1 = 0;
	//set cmd delay
	if (mode == AM_CI_CMD_IOR) {
		//delay0
		delay0 = delay0 | (DELAY_RIO_INIT_ADDR << 0);
		delay0 = delay0 | (DELAY_RIO_ADDR_CE << 8);
		delay0 = delay0 | (DELAY_RIO_CE_RD << 16);
		delay0 = delay0 | (DELAY_RIO_RD_RWAIT << 24);
		//delay1
		delay1 = delay1 | (DELAY_RIO_RWAIT_DATA << 0);
		delay1 = delay1 | (DELAY_RIO_DATA_DRD << 8);
		delay1 = delay1 | (DELAY_RIO_DRD_DCE << 16);
		delay1 = delay1 | (DELAY_RIO_DCE_INIT << 24);
	} else if (mode == AM_CI_CMD_IOW) {
		//delay0
		delay0 = delay0 | (DELAY_WIO_INIT_ADDR << 0);
		delay0 = delay0 | (DELAY_WIO_ADDR_CE << 8);
		delay0 = delay0 | (DELAY_WIO_CE_WR << 16);
		delay0 = delay0 | (DELAY_WIO_WR_RWAIT << 24);
		//delay1
		delay1 = delay1 | (DELAY_WIO_RWAIT_DATA << 0);
		delay1 = delay1 | (DELAY_WIO_DATA_DWR << 8);
		delay1 = delay1 | (DELAY_WIO_DWR_DCE << 16);
		delay1 = delay1 | (DELAY_WIO_DCE_INIT << 24);
	} else if (mode == AM_CI_CMD_MEMR) {
		//delay0
		delay0 = delay0 | (DELAY_RMEM_INIT_ADDR << 0);
		delay0 = delay0 | (DELAY_RMEM_ADDR_CE << 8);
		delay0 = delay0 | (DELAY_RMEM_CE_RD << 16);
		delay0 = delay0 | (DELAY_RMEM_RD_RWAIT << 24);
		//delay1
		delay1 = delay1 | (DELAY_RMEM_RWAIT_DATA << 0);
		delay1 = delay1 | (DELAY_RMEM_DATA_DRD << 8);
		delay1 = delay1 | (DELAY_RMEM_DRD_DCE << 16);
		delay1 = delay1 | (DELAY_RMEM_DCE_INIT << 24);
	} else if (mode == AM_CI_CMD_MEMW) {
		//delay0
		delay0 = delay0 | (DELAY_WMEM_INIT_ADDR << 0);
		delay0 = delay0 | (DELAY_WMEM_ADDR_CE << 8);
		delay0 = delay0 | (DELAY_WMEM_CE_WR << 16);
		delay0 = delay0 | (DELAY_WMEM_WR_RWAIT << 24);
		//delay1
		delay1 = delay1 | (DELAY_WMEM_RWAIT_DATA << 0);
		delay1 = delay1 | (DELAY_WMEM_DATA_DWR << 8);
		delay1 = delay1 | (DELAY_WMEM_DWR_DCE << 16);
		delay1 = delay1 | (DELAY_WMEM_DCE_INIT << 24);
	}
	//Wwrite cmd
	WRITE_CIBUS_REG(CIPLUS_DELAY_CTRL0, delay0);
	WRITE_CIBUS_REG(CIPLUS_DELAY_CTRL1, delay1);
	return 0;
}


/**\brief aml_ci_bus_io:ci bus read or write api with bus
* \param ci_bus_dev: ci_bus_dev obj,used this data to ctl
* \param val: read or write value
* \param addr: rw addr
* \param mode: cmd
* \return
*   - read value:ok
*   - -EINVAL : error
*/
static int aml_ci_bus_io(struct aml_ci_bus *ci_bus_dev,
				u8 val, u16 addr, int mode)
{
	int rd;
	int ret = -1;
	u32 address = addr;
	u32 data = val;
	u32 reg = 0;
	u32 ctrl = 0;
	int enable = 0;
	int count = 0;
	u32 int_status;
	//only used hi addr. we to change tsout to addr
	if (addr >= 4) {
		enable = 1;
	}
	//clear irq
	ctrl = READ_CIBUS_REG(CIPLUS_CTRL_REG);
	ctrl = ctrl | (1 << CLEAR_CMP_IRQ);
	ctrl = ctrl | (1 << CLEAR_TIMEOUT_IRQ);
	//Wwrite cmd crtl
	WRITE_CIBUS_REG(CIPLUS_CTRL_REG, ctrl);
	fetch_done = 0;
	//gpio select gpio func
	//aml_ci_bus_select_gpio(ci_bus_dev, enable ? AML_GPIO_ADDR : AML_GPIO_TS);
	while (1) {
		count++;
		if (count < aml_ci_bus_time)
			break;
	}
	//enable delay reg,defalue is disable
	if (aml_ci_bus_set_delay)
		aml_ci_bus_set_delay_time(mode);
	//cmd vilad
	reg = reg | (1 << CI_CMD_VALID);
	//set addr
	reg = reg | ((address & 0xeFFF) << CI_CMD_ADDR);
	//set cmd and write data
	if (mode == AM_CI_CMD_IOR) {
		reg = reg | (IORD << CI_CMD_TYPE);
	} else if (mode == AM_CI_CMD_IOW) {
		reg = reg | (data << CI_CMD_WDATA);
		reg = reg | (IOWR << CI_CMD_TYPE);
	} else if (mode == AM_CI_CMD_MEMR) {
		reg = reg | (MEMRD << CI_CMD_TYPE);
	} else if (mode == AM_CI_CMD_MEMW) {
		reg = reg | (data << CI_CMD_WDATA);
		reg = reg | (MEMWR << CI_CMD_TYPE);
	}
	//clear irq
	ctrl = READ_CIBUS_REG(CIPLUS_CTRL_REG);
	ctrl = ctrl | (1 << CLEAR_CMP_IRQ);
	ctrl = ctrl | (1 << CLEAR_TIMEOUT_IRQ);
	//Wwrite cmd crtl
	WRITE_CIBUS_REG(CIPLUS_CTRL_REG, ctrl);
	//Wwrite cmd reg
	WRITE_CIBUS_REG(CIPLUS_CMD_REG, reg);
	//wait cmp irq or timwout irq
	if (USED_IRQ == 1) {
		ret =
			wait_event_interruptible_timeout(wq, fetch_done != 0,
					HZ / 100);//10ms
	} else {
		count = 0;
		while(1) {
			count++;
			int_status = READ_CIBUS_REG(CIPLUS_STATUS_REG);
			if ((int_status&(1 << COMPLETE_IRQ_STATE)) == (1 << COMPLETE_IRQ_STATE)) {
				break;
			}
			if (count > 50) {
				printk("count timeout:%d\r\n", count);
				break;
			}
		}
	}
	rd = READ_CIBUS_REG(CIPLUS_RDATA_REG);
	//gpio select tsout func
	//aml_ci_bus_select_gpio(ci_bus_dev, AML_GPIO_TS, enable);
	return rd;
}
/**\brief aml_ci_bus_init_reg:ci bus init reg,enable ci bus
* \param ci_bus_dev: ci_bus_dev obj,used this data to ctl
* \return
*   - 0:ok
*/
static int aml_ci_bus_init_reg(struct aml_ci_bus *ci_bus_dev)
{
	u32 ctrl = 0;

	if (ci_bus_dev->addr_ts_mode_multiplex == 0) {
		aml_ci_bus_select_gpio(ci_bus_dev,AML_GPIO_TS);
	} else {
		aml_ci_bus_select_gpio(ci_bus_dev,AML_GPIO_ADDR);
	}

	//init ci bus reg
	pr_dbg("aml_ci_bus_init_reg---\r\n");
    ctrl = READ_CIBUS_REG(CIPLUS_CTRL_REG);
	ctrl = ctrl | (1 << CI_ENABLE);
	ctrl = ctrl | (1 << ENABLE_CMP_IRQ);
	WRITE_CIBUS_REG(CIPLUS_CTRL_REG, ctrl);

    ctrl = 0;
	ctrl = ctrl | (1 << ENABEL_TIMEOUT_IRQ);
	ctrl = ctrl | (TIMEOUT_IRQ_HOLD_TIME << WATT_TIMEOUT_TIME);
	//timeout hold time
	//WRITE_CIBUS_REG(CIPLUS_WAIT_TIMEOUT, ctrl);
	//aml_ci_bus_select_gpio(ci_bus_dev,AML_GPIO_TS);
	return 0;
}
/********************************************************/
/********************************************************/
/*******             gpio api               *************/
/********************************************************/
/********************************************************/
/**\brief aml_set_gpio_out:set gio out and set val value
* \param gpio: gpio_desc obj,
* \param val:  set val
* \return
*   - 0:ok
*   - -EINVAL : error
*/
static int aml_set_gpio_out(struct gpio_desc *gpio, int val)
{
	int ret = 0;
	if (val < 0) {
		pr_dbg("gpio out val = -1.\n");
		return -1;
	}
	if (val != 0)
		val = 1;
	ret = gpiod_direction_output(gpio, val);
	pr_dbg("dvb ci gpio out ret %d set val:%d\n", ret, val);
	return ret;
}

/**\brief aml_set_gpio_in:set gio in
* \param gpio: gpio_desc obj,
* \return
*   - 0:ok
*   - -EINVAL : error
*/
static int aml_set_gpio_in(struct gpio_desc *gpio)
{
	gpiod_direction_input(gpio);
	return 0;
}

/**\brief aml_get_gpio_value:get gio value
* \param gpio: gpio_desc obj,
* \return
*   - gpio value:ok
*   - -EINVAL : error
*/
static int aml_get_gpio_value(struct gpio_desc *gpio)
{
	int ret = 0;
	ret = gpiod_get_value(gpio);
	return ret;
}
/**\brief aml_gpio_free:free gio
* \param gpio: gpio_desc obj,
* \return
*   - 0:ok
*   - -EINVAL : error
*/
static int aml_gpio_free(struct gpio_desc *gpio)
{
	gpiod_put(gpio);
	return 0;
}
/**\brief ci_bus_get_gpio_by_name:get gpio desc from dts file
* \param ci_bus_dev: aml_ci_bus obj
* \param gpiod:   gpio_desc * obj
* \param str: gpio name at dts file
* \param input_output: gpio input or output type
* \param output_value: gpio out put value
* \return
*   - 0:ok
*   - -EINVAL : error
*/
static int ci_bus_get_gpio_by_name(struct aml_ci_bus *ci_bus_dev,
struct gpio_desc **gpiod, int *pin_value,
char *str, int input_output, int output_level)
{
	int ret = 0;
	struct device_node *child = NULL;
	struct platform_device *pdev = ci_bus_dev->pdev;
	struct device_node *np = pdev->dev.of_node;

	/*get gpio config from dts*/
	/* get device config for dvbci_io*/
	child = of_get_child_by_name(np, "dvbci_io");
	if (IS_ERR(*gpiod)) {
		pr_dbg("dvb ci bus %s request failed\n", str);
		return -1;
	}

	*pin_value = of_get_named_gpio_flags(child, str, 0, NULL);
	*gpiod = gpio_to_desc(*pin_value);
	if (IS_ERR(*gpiod)) {
		pr_dbg("ci bus %s request failed\n", str);
		return -1;
	}
	pr_dbg("ci bus get_gpio %s %p  %d\n", str, *gpiod, *pin_value);
	gpio_request(*pin_value, AML_MODE_NAME);

	if (input_output == OUTPUT) {
		ret = gpiod_direction_output(*gpiod, output_level);
	} else if (input_output == INPUT)	{
		ret = gpiod_direction_input(*gpiod);
		/*ret |= gpiod_set_pullup(*gpiod, 1);*/
	} else {
		pr_error("ci bus Request gpio direction invalid\n");
	}
	return ret;
}
/********************************************************/
/********************************************************/
/*******             gpio ci bus api end    *************/
/********************************************************/
/********************************************************/
/**\brief aml_ci_bus_mem_read:io read from cam
* \param ci_dev: aml_ci obj,used this data to get ci_bus_dev obj
* \param slot: slot index
* \param addr: read addr
* \return
*   - read value:ok
*   - -EINVAL : error
*/
static  int aml_ci_bus_mem_read(
	struct aml_ci *ci_dev, int slot, int addr)
{
	u8  data = 0;
	u16 addres = addr;
	int value = 0;
	struct aml_ci_bus *ci_bus_dev = ci_dev->data;
	mutex_lock(&(ci_bus_dev->mutex));
	aml_ci_bus_select_gpio(ci_bus_dev, AML_GPIO_ADDR);
	value = aml_ci_bus_io(ci_bus_dev, data, addres, AM_CI_CMD_MEMR);
	mutex_unlock(&(ci_bus_dev->mutex));
	return value;
}
/**\brief aml_ci_bus_mem_write:io write to cam by bus api
* \param ci_dev: aml_ci obj,used this data to get ci_bus_dev obj
* \param slot: slot index
* \param addr: write addr
* \param addr: write value
* \return
*   - 0:ok
*   - -EINVAL : error
*/
static int aml_ci_bus_mem_write(
	struct aml_ci *ci_dev, int slot, int addr,  u8 val)
{
	u8  data = val;
	u16 addres = addr;
	int value = 0;
	struct aml_ci_bus *ci_bus_dev = ci_dev->data;
	mutex_lock(&(ci_bus_dev->mutex));
	aml_ci_bus_select_gpio(ci_bus_dev, AML_GPIO_ADDR);
	value = aml_ci_bus_io(ci_bus_dev, data, addres, AM_CI_CMD_MEMW);
	mutex_unlock(&(ci_bus_dev->mutex));
	return value;
}
/**\brief aml_ci_bus_io_read:io read from cam by bus api
* \param ci_dev: aml_ci obj,used this data to get ci_bus_dev obj
* \param slot: slot index
* \param addr: read addr
* \return
*   - read value:ok
*   - -EINVAL : error
*/
static int aml_ci_bus_io_read(
	struct aml_ci *ci_dev, int slot, int addr)
{
	u8  data = 0;
	u16 addres = addr;
	int value = 0;
	struct aml_ci_bus *ci_bus_dev = ci_dev->data;
	mutex_lock(&(ci_bus_dev->mutex));
	aml_ci_bus_select_gpio(ci_bus_dev, AML_GPIO_TS);
	value = aml_ci_bus_io(ci_bus_dev, data, addres, AM_CI_CMD_IOR);
	mutex_unlock(&(ci_bus_dev->mutex));
	return value;
}
/**\brief aml_ci_bus_io_write:io write to cam
* \param ci_dev: aml_ci obj,used this data to get ci_bus_dev obj
* \param slot: slot index
* \param addr: write addr
* \param addr: write value
* \return
*   - 0:ok
*   - -EINVAL : error
*/
static int aml_ci_bus_io_write(
	struct aml_ci *ci_dev, int slot, int addr, u8 val)
{
	u8  data = val;
	u16 addres = addr;
	int value = 0;
	struct aml_ci_bus *ci_bus_dev = ci_dev->data;
	mutex_lock(&(ci_bus_dev->mutex));
	aml_ci_bus_select_gpio(ci_bus_dev, AML_GPIO_TS);
	value = aml_ci_bus_io(ci_bus_dev, data, addres, AM_CI_CMD_IOW);
	mutex_unlock(&(ci_bus_dev->mutex));
	return value;
}


/**\brief aml_ci_bus_rst:reset cam by ci bus
* \param ci_dev: aml_ci obj,used this data to get ci_bus_dev obj
* \param slot: slot index
* \return
*   - 0:ok
*   - -EINVAL : error
*/
static int aml_ci_bus_rst(
	struct aml_ci *ci_dev, int slot, int level)
{
	int value = 0;
	u32 ctrl = 0;
	struct aml_ci_bus *ci_bus_dev = ci_dev->data;
	mutex_lock(&(ci_bus_dev->mutex));
	ctrl = READ_CIBUS_REG(CIPLUS_CTRL_REG);
	if (level == AML_H)
		ctrl = ctrl | (1 << CAM_RESET);
	else
		ctrl = ctrl & (~(1 << CAM_RESET));
	//Wwrite cmd crtl
	WRITE_CIBUS_REG(CIPLUS_CTRL_REG, ctrl);
	mutex_unlock(&(ci_bus_dev->mutex));
	return value;
}

/**\brief aml_ci_slot_reset:reset slot
* \param ci_dev: aml_ci obj,used this data to get ci_bus_dev obj
* \param slot: slot index
* \return
*   - 0:ok
*   - -EINVAL : error
*/
static int aml_ci_slot_reset(struct aml_ci *ci_dev, int slot)
{
	struct aml_ci_bus *ci_bus_dev = ci_dev->data;
	pr_dbg("Slot(%d): Slot RESET CAM\n", slot);
	aml_pcmcia_reset(&ci_bus_dev->pc);
	if (ci_bus_dev->raw_mode == 0)
		dvb_ca_en50221_cimcu_camready_irq(&ci_dev->en50221_cimcu, 0);
	return 0;
}
/**\brief aml_ci_slot_shutdown:show slot
* \param ci_dev: aml_ci obj,used this data to get ci_bus_dev obj
* \param slot: slot index
* \return
*   - 0:ok
*   - -EINVAL : error
* readme:no use this api
*/
static int aml_ci_slot_shutdown(struct aml_ci *ci_dev, int slot)
{
	pr_dbg("Slot(%d): Slot shutdown\n", slot);
	return 0;
}
/**\brief aml_ci_ts_control:control slot ts
* \param ci_dev: aml_ci obj,used this data to get ci_bus_dev obj
* \param slot: slot index
* \return
*   - 0:ok
*   - -EINVAL : error
* readme:no use this api
*/
static int aml_ci_ts_control(struct aml_ci *ci_dev, int slot)
{
	pr_dbg("Slot(%d): TS control\n", slot);
	return 0;
}
/**\brief aml_ci_slot_status:get slot status
* \param ci_dev: aml_ci obj,used this data to get ci_bus_dev obj
* \param slot: slot index
* \param open: no used
* \return
*   - cam status
*   - -EINVAL : error
*/
static int aml_ci_slot_status(struct aml_ci *ci_dev, int slot, int open)
{
	struct aml_ci_bus *ci_bus_dev = ci_dev->data;
	int state = 0;
	if (ci_bus_dev->pc.start_work == 0) {
		return 0;
	}
	if (ci_bus_dev->pc.slot_state == MODULE_INSERTED) {
		state = DVB_CA_EN50221_POLL_CAM_PRESENT |
		DVB_CA_EN50221_POLL_CAM_READY;
	}
	if (ci_bus_dev->slot_state != ci_bus_dev->pc.slot_state)
	{
		printk("cam crad change-----\r\n");
		ci_bus_dev->slot_state = ci_bus_dev->pc.slot_state;
		state = state | DVB_CA_EN50221_POLL_CAM_CHANGED;
	}
	return state;
}
/**\brief aml_ci_slot_wakeup:get slot wake up thread flag
* \param ci_dev: aml_ci obj,used this data to get ci_bus_dev obj
* \param slot: slot index
* \return
*   - cam wake up flag
*   - -EINVAL : error
*/
static int aml_ci_slot_wakeup(struct aml_ci *ci_dev, int slot)
{
	struct aml_ci_bus *ci_bus_dev = ci_dev->data;
	if (ci_bus_dev) {
		return ci_bus_dev->wakeup_thread;
	}
	return 1;
}

/**\brief aml_ci_gio_get_irq:get gpio cd1 irq pin value
* \return
*   - irq pin value
*   - -EINVAL : error
*/
static int aml_ci_gio_get_irq(void)
{
	int ret = 0;
	ret = aml_get_gpio_value(ci_bus.cd_pin1);
	return ret;
}

/********************************************************/
/********************************************************/
/*******        for pcmcid api              *************/
/********************************************************/
/********************************************************/
/**\brief aml_gio_power:set power gpio hi or low
* \param pc: aml_pcmcia obj,used this priv to get ci_bus_dev obj
* \param enable: power pin hi or low
* \return
*   - 0
*   - -EINVAL : error
*/
static int aml_gio_power(struct aml_pcmcia *pc, int enable)
{
	int ret = 0;
	struct aml_ci_bus *ci_bus_dev = pc->priv;
	if (ci_bus_dev == NULL) {
		pr_dbg("ci bus dev is null %s : %d\r\n", __func__, enable);
		return -1;
	}
	pr_dbg("%s : %d\r\n", __func__, enable);

	if (enable == AML_PWR_OPEN) {
		/*hi level ,open power*/
		ret = aml_set_gpio_out(ci_bus_dev->pwr_pin, AML_GPIO_LOW);
	} else {
		/*low level ,close power*/
		ret = aml_set_gpio_in(ci_bus_dev->pwr_pin);
	}

	return ret;
}
/**\brief aml_gio_reset:set reset gpio hi or low
* \param pc: aml_pcmcia obj,used this priv to get ci_bus_dev obj
* \param enable: reset pin hi or low
* \return
*   - 0
*   - -EINVAL : error
*/
static int aml_gio_reset(struct aml_pcmcia *pc, int enable)
{
	/*need set hi and sleep set low*/
	int ret = 0;
	struct aml_ci_bus *ci_bus_dev = pc->priv;

	if (ci_bus_dev == NULL) {
		pr_dbg("ci bus dev is null %s : %d\r\n", __func__, enable);
		return -1;
	}

	pr_dbg("%s : %d  type: %d\r\n", __func__, enable, ci_bus_dev->io_device_type);
	if (ci_bus_dev == NULL || ci_bus_dev->priv == NULL) {
		pr_dbg("rst by ci bus- ci bus dev-null-\r\n");
		return -1;
	}
	/*if (enable == AML_H) {*/
		aml_ci_bus_select_gpio(ci_bus_dev, AML_GPIO_ADDR);
		/*ci_bus_dev->select = AML_GPIO_TS;
	}*/

	aml_ci_bus_rst((struct aml_ci *)ci_bus_dev->priv, 0, enable);
	pr_dbg("rst by ci bus- ci bus [%d]-\r\n", ci_bus_dev->select);
	/*if (enable == AML_L)
		aml_ci_bus_select_gpio(ci_bus_dev, AML_GPIO_TS);*/
	return ret;
}

/**\brief aml_gio_init_irq:set gpio irq
* \param pc: aml_pcmcia obj,used this priv to get ci_bus_dev obj
* \param flag: rising or falling or hi or low
* \return
*   - 0
*   - -EINVAL : error
*/
/*need change*/
static int aml_gio_init_irq(struct aml_pcmcia *pc, int flag)
{
	struct aml_ci_bus *ci_bus_dev = (struct aml_ci_bus *)pc->priv;
	gpiod_to_irq(ci_bus_dev->cd_pin1);
	return 0;
}

/**\brief aml_gio_get_cd1:get gpio cd1 pin value
* \param pc: aml_pcmcia obj,used this priv to get ci_bus_dev obj
* \return
*   - cd1 pin value
*   - -EINVAL : error
*/
static int aml_gio_get_cd1(struct aml_pcmcia *pc)
{
	int ret = 1;
	struct aml_ci_bus *ci_bus_dev = pc->priv;
	ret = aml_get_gpio_value(ci_bus_dev->cd_pin1);
	pr_dbg("%s :cd: %d\r\n", __func__, ret);
	return ret;
}
/**\brief aml_gio_get_cd2:get gpio cd2 pin value
* \param pc: aml_pcmcia obj,used this priv to get ci_bus_dev obj
* \return
*   - cd2 pin value
*   - -EINVAL : error
*/
static int aml_gio_get_cd2(struct aml_pcmcia *pc)
{
	int ret = 0;
	struct aml_ci_bus *ci_bus_dev = pc->priv;
	ret = aml_get_gpio_value(ci_bus_dev->cd_pin1);
	pr_dbg("%s :cd: %d\r\n", __func__, ret);
	return ret;
}
/**\brief aml_cam_plugin:notify en50221 cam card in or out
* \param pc: aml_pcmcia obj,used this priv to get ci_bus_dev obj
* \plugin: 0:remove;1:in
* \return
*   - 0
*   - -EINVAL : error
*/
static int aml_cam_plugin(struct aml_pcmcia *pc, int plugin)
{
	struct aml_ci *ci = (struct aml_ci *)
	((struct aml_ci_bus *)(pc->priv))->priv;
	pr_dbg("%s : %d\r\n", __func__, plugin);
	if (((struct aml_ci_bus *)(pc->priv))->raw_mode == 0) {
		if (plugin) {
			aml_ci_bus_select_gpio((struct aml_ci_bus *)(pc->priv), AML_GPIO_TS);
			dvb_ca_en50221_cimcu_camchange_irq(&ci->en50221_cimcu,
				0, DVB_CA_EN50221_CAMCHANGE_INSERTED);
		} else {
			aml_ci_bus_select_gpio((struct aml_ci_bus *)(pc->priv), AML_GPIO_ADDR);
			dvb_ca_en50221_cimcu_camchange_irq(&ci->en50221_cimcu,
				0, DVB_CA_EN50221_CAMCHANGE_REMOVED);
		}
	}
	return 0;
}
/**\brief aml_pcmcia_alloc:alloc nad init pcmcia obj
* \param ci_bus_dev: aml_ci_bus obj,
* \param pcmcia: aml_pcmcia * obj,
* \return
*   - 0
*   - -EINVAL : error
*/
static void aml_pcmcia_alloc(struct aml_ci_bus *ci_bus_dev,
	struct aml_pcmcia **pcmcia)
{
	pr_dbg("aml_pcmcia_alloc----\n");
	*pcmcia = &ci_bus_dev->pc;
	(*pcmcia)->irq = ci_bus_dev->irq;
	(*pcmcia)->init_irq = aml_gio_init_irq;
	(*pcmcia)->get_cd1 = aml_gio_get_cd1;
	(*pcmcia)->get_cd2 = aml_gio_get_cd2;
	(*pcmcia)->pwr = aml_gio_power;
	(*pcmcia)->rst = aml_gio_reset;
	(*pcmcia)->pcmcia_plugin = aml_cam_plugin;
	(*pcmcia)->slot_state = MODULE_XTRACTED;
	(*pcmcia)->priv = ci_bus_dev;
	(*pcmcia)->run_type = 0;/*0:irq;1:poll*/
	(*pcmcia)->io_device_type = AML_DVB_IO_TYPE_CIMAX;
	(*pcmcia)->start_work = 0;
}

/**\brief aml_ci_bus_get_config_from_dts:
 * get gpio config from dts
* \param ci_bus_dev: aml_ci_bus obj,
* \return
*   - 0
*   - -EINVAL : error
*/
static int aml_ci_bus_get_config_from_dts(struct aml_ci_bus *ci_bus_dev)
{
	struct device_node *child = NULL;
	struct platform_device *pdev = ci_bus_dev->pdev;
	struct device_node *np = pdev->dev.of_node;
	int ret = 0;
	pr_dbg("into get ci bus dts -----\r\n");
	/*get gpio config from dts*/
	/* get device config for dvbci_io*/
	child = of_get_child_by_name(np, "dvbci_io");
	if (child == NULL) {
		pr_error("failed to get dvbci_io\n");
		return -1;
	}
	//below is get cd1 cd2 pwr irq reset gpio info
	if (ci_bus_dev->io_device_type == AML_DVB_IO_TYPE_CIBUS) {
		struct resource *res;
		char buf[32];
		int ival;

		/*get irq value*/
		ci_bus_dev->irq_cmp = 186;
		memset(buf, 0, 32);
		snprintf(buf, sizeof(buf), "%s", "irq_cmp");
		res = platform_get_resource_byname(pdev, IORESOURCE_IRQ, buf);
		if (res)
			ci_bus_dev->irq_cmp = res->start;
		else
			pr_err("get irq cmp error\r\n");

		ci_bus_dev->irq_timeout = 187;
		memset(buf, 0, 32);
		snprintf(buf, sizeof(buf), "%s", "irq_timeout");
		res = platform_get_resource_byname(pdev, IORESOURCE_IRQ, buf);
		if (res)
			ci_bus_dev->irq_timeout = res->start;
		else
			pr_err("get irq irq_timeout error\r\n");
		//pin config
		pr_dbg("ci bus irq[%d]cmp[%d] \r\n",ci_bus_dev->irq_cmp, ci_bus_dev->irq_timeout);
		if (!ci_bus_dev->pinctrl) {
			ci_bus_dev->pinctrl = devm_pinctrl_get(&pdev->dev);
			if (IS_ERR_OR_NULL(ci_bus_dev->pinctrl)) {
				pr_error("get pinctl could not get pinctrl handle\n");
				return -EINVAL;
			}
		}

		if (ci_bus_dev->pinctrl) {
			struct pinctrl_state *s;
			pr_dbg("ci bus get ts pin\r\n");
			s = pinctrl_lookup_state(ci_bus_dev->pinctrl, "ci_ts_pins");
			if (IS_ERR_OR_NULL(s)) {
				pr_error("could not get jtag_apee_pins state\n");
				return -1;
			}
			pr_dbg("ci bus select ts pin\r\n");
			ret = pinctrl_select_state(ci_bus_dev->pinctrl, s);
			if (ret) {
				pr_error("failed to set pinctrl\n");
				return -1;
			}
			ci_bus_dev->select = AML_GPIO_TS;
		} else if (IS_ERR_OR_NULL(ci_bus_dev->pinctrl)) {
			pr_error("could not get pinctrl handle\n");
			return -EINVAL;
		}
		/*get reset pwd cd1 cd2 gpio pin*/
		ci_bus_dev->cd_pin1 = NULL;
		pr_dbg("ci bus get cd1\r\n");
		ret = ci_bus_get_gpio_by_name(ci_bus_dev,
			&ci_bus_dev->cd_pin1,
			&ci_bus_dev->cd_pin1_value, "cd_pin1",
			INPUT, OUTLEVEL_HIGH);
		if (ret) {
			pr_error("dvb ci cd_pin1 pin request failed\n");
			return -1;
		}
		ci_bus_dev->cd_pin2 = ci_bus_dev->cd_pin1;
		ci_bus_dev->cd_pin2_value = ci_bus_dev->cd_pin1_value;
		ci_bus_dev->pwr_pin = NULL;
		pr_dbg("ci_bus_dev->cd_pin1_value==%d\r\n", ci_bus_dev->cd_pin1_value);
		ci_bus_dev->irq = gpiod_to_irq(ci_bus_dev->cd_pin1) ;
		pr_dbg("ci_bus_dev->irq==%d  get from gpio cd1\r\n", ci_bus_dev->irq);

		ret = ci_bus_get_gpio_by_name(ci_bus_dev,
			&ci_bus_dev->pwr_pin, &ci_bus_dev->pwr_pin_value,
			"pwr_pin", OUTPUT, OUTLEVEL_HIGH);
		if (ret) {
			pr_error("dvb ci pwr_pin pin request failed\n");
			return -1;
		}
		aml_set_gpio_in(ci_bus_dev->pwr_pin);

		/*get le pin*/
		ci_bus_dev->le_pin = NULL;
		ci_bus_dev->le_enable_level = 1;
		ret = ci_bus_get_gpio_by_name(ci_bus_dev,
			&ci_bus_dev->le_pin, &ci_bus_dev->le_pin_value,
			"le_pin", OUTPUT, OUTLEVEL_HIGH);
		if (ret) {
			pr_error("dvb ci le_pin pin request failed\n");
		} else {
			pr_dbg("ci_bus_dev->le_value %d\n", ci_bus_dev->le_pin_value);
		}
		memset(buf, 0, 32);
		snprintf(buf, sizeof(buf), "%s", "le_enable_level");
		ret = of_property_read_u32(pdev->dev.of_node, buf, &ival);
		if (ret) {
			pr_error("dvb ci le_enable_level request failed\n");
		} else {
			ci_bus_dev->le_enable_level = ival;
			pr_dbg("ci_bus_dev->le_enable_level-- %d\n", ci_bus_dev->le_enable_level);
		}
		/*get addr_ts_mode_multiplex mode*/
		ci_bus_dev->addr_ts_mode_multiplex = 1;
		memset(buf, 0, 32);
		snprintf(buf, sizeof(buf), "%s", "addr_ts_mode_multiplex");
		ret = of_property_read_u32(pdev->dev.of_node, buf, &ival);
		if (ret) {
			pr_error("dvb ci addr_ts_mode_multiplex request failed\n");
		} else {
			ci_bus_dev->addr_ts_mode_multiplex = ival;
			pr_dbg("ci_bus_dev->addr_ts_mode_multiplex %d ******\n", ci_bus_dev->addr_ts_mode_multiplex);
		}


	}
	return 0;
}
/**\brief aml_ci_free_gpio:free ci gpio
* \param ci_bus_dev: aml_ci_bus obj,
* \return
*   - 0
*   - -EINVAL : error
*/
static void aml_ci_free_gpio(struct aml_ci_bus *ci_bus_dev)
{
	if (ci_bus_dev == NULL) {
		pr_error("ci_bus_dev is NULL,no need free gpio res\r\n");
		return;
	}

	if (ci_bus_dev->pwr_pin) {
		aml_gpio_free(ci_bus_dev->pwr_pin);
		ci_bus_dev->pwr_pin = NULL;
	}
	if (ci_bus_dev->cd_pin1) {
		aml_gpio_free(ci_bus_dev->cd_pin1);
		ci_bus_dev->cd_pin1 = NULL;
		ci_bus_dev->cd_pin2 = NULL;
	}
	if (ci_bus_dev->le_pin) {
		aml_gpio_free(ci_bus_dev->le_pin);
		ci_bus_dev->le_pin = NULL;
	}
	return;
}

static irqreturn_t timeout_isr(int irq, void *dev_id)
{
	u32 int_status = READ_CIBUS_REG(CIPLUS_STATUS_REG);

	if ((int_status & (1 << TIMEOUT_IRQ_STATE)) == (1 << TIMEOUT_IRQ_STATE)) {
		fetch_done = 1;
		wake_up_interruptible(&wq);
	}
	return IRQ_HANDLED;
}

static irqreturn_t cmp_isr(int irq, void *dev_id)
{
	u32 int_status = READ_CIBUS_REG(CIPLUS_STATUS_REG);

	if ((int_status&(1 << COMPLETE_IRQ_STATE)) == (1 << COMPLETE_IRQ_STATE)) {
		fetch_done = 1;
		wake_up_interruptible(&wq);
	}
	return IRQ_HANDLED;
}

/**\brief aml_ci_bus_init:ci_bus_dev init
* \param ci_dev: aml_ci obj,
* \param pdev: platform_device obj,used to get dts info
* \return
*   - 0
*   - -EINVAL : error
*/
int aml_ci_bus_init(struct platform_device *pdev, struct aml_ci *ci_dev)
{
	struct aml_ci_bus *ci_bus_dev = NULL;
	struct aml_pcmcia *pc;
	int result,irq;

	ci_bus_dev = &ci_bus;
	ci_bus_dev->pdev = pdev;
	ci_bus_dev->raw_mode = ci_dev->raw_mode;
	ci_bus_dev->priv = ci_dev;
	ci_bus_dev->bus_pinctrl = NULL;
	ci_bus_dev->pinctrl = NULL;
	/*default mode is wake up,when trans a lot data,used sleep mode*/
	ci_bus_dev->wakeup_thread = 1;
	mutex_init(&(ci_bus_dev->mutex));
	/*init io device type*/
	ci_bus_dev->io_device_type = ci_dev->io_type;
	pr_dbg("*********ci bus Dev type [%d]\n", ci_dev->io_type);
	/*get config from dts*/
	aml_ci_bus_get_config_from_dts(ci_bus_dev);
	//iomap ci reg
	init_ci_addr(pdev);
	/*Register irq handlers */
	if (ci_bus_dev->irq_cmp != -1) {
		if (USED_IRQ) {
			irq = request_irq(ci_bus_dev->irq_cmp,
					cmp_isr,
					IRQF_SHARED|IRQF_TRIGGER_RISING,
					"ciplus cmp irq", ci_bus_dev);
			if (irq == 0)
				pr_dbg("request cmp irq sucess\r\n");
			else if (irq == -EBUSY)
				pr_err("request cmp irq busy\r\n");
			else
				pr_err("request cmp irq error [%d]\r\n", irq);
		} else {
			disable_irq(ci_bus_dev->irq_cmp);
		}
	}
	/*Register irq handlers */
	if (ci_bus_dev->irq_timeout != -1) {
		if (USED_IRQ) {
			pr_dbg("request timeout irq\n");
			irq = request_irq(ci_bus_dev->irq_timeout,
					timeout_isr,
					IRQF_SHARED|IRQF_TRIGGER_RISING,
					"ciplus timeout irq", ci_bus_dev);
			if (irq == 0)
				pr_err("request timeout irq sucess\r\n");
			else if (irq == -EBUSY)
				pr_err("request timeout irq busy\r\n");
			else
				pr_err("request timeout irq error [%d]\r\n", irq);
		} else {
			disable_irq(ci_bus_dev->irq_timeout);
		}
	}
	pr_dbg("*********ci bus init bus reg\n");
	aml_ci_bus_init_reg(ci_bus_dev);
	/*init ci_dev used api.*/
	ci_dev->ci_mem_read  = aml_ci_bus_mem_read;
	ci_dev->ci_mem_write = aml_ci_bus_mem_write;
	ci_dev->ci_io_read = aml_ci_bus_io_read;
	ci_dev->ci_io_write = aml_ci_bus_io_write;
	ci_dev->ci_slot_reset = aml_ci_slot_reset;
	ci_dev->ci_slot_shutdown = aml_ci_slot_shutdown;
	ci_dev->ci_slot_ts_enable = aml_ci_ts_control;
	ci_dev->ci_poll_slot_status = aml_ci_slot_status;
	ci_dev->ci_get_slot_wakeup = aml_ci_slot_wakeup;
	ci_dev->data = ci_bus_dev;

	aml_pcmcia_alloc(ci_bus_dev, &pc);
	pc->io_device_type = ci_bus_dev->io_device_type;
	pr_dbg("*********ci bus aml_pcmcia_init start_work:%d\n", pc->start_work);
	result = aml_pcmcia_init(pc);
	if (result < 0) {
		pr_error("aml_pcmcia_init failed\n");
		goto fail1;
	}
	pr_dbg("*********ci bus aml_ci_bus_mod_init---\n");
	aml_ci_bus_mod_init();
	return 0;
fail1:
	kfree(ci_bus_dev);
	ci_bus_dev = NULL;
	return 0;
}
EXPORT_SYMBOL(aml_ci_bus_init);
/**\brief aml_ci_bus_exit:ci_bus exit
* \return
*   - 0
*   - -EINVAL : error
*/
int aml_ci_bus_exit(struct aml_ci *ci)
{
	aml_ci_bus_mod_exit();
	/*exit pc card*/
	aml_pcmcia_exit(&ci_bus.pc);
	/*free gpio*/
	aml_ci_free_gpio(&ci_bus);

	return 0;
}
EXPORT_SYMBOL(aml_ci_bus_exit);

#if 1
/********************************************************/
/********************************************************/
/*******        for ci bus test api            *************/
/********************************************************/
/********************************************************/

/*cam difines*/
#define DA	0x80
#define FR	0x40
#define WE	0x02
#define RE	0x01

#define RS	0x08
#define SR	0x04
#define SW	0x02
#define HC	0x01
#define DATA_REG	  0
#define COM_STA_REG	1
#define SIZE_REG_L	2
#define SIZE_REG_M	3

static void aml_ci_bus_full_test(struct aml_ci *ci_dev)
{
		unsigned int BUF_SIZE = 0;
		unsigned int i = 0;
		unsigned char cc = 0;
		unsigned char reg;
		unsigned int  bsize = 0;
		int cnt = 0;
		unsigned char buf[10];
		int count = 1000;
		mdelay(1000);
		pr_dbg("READ CIS START\r\n");
		for (i = 0; i < 200; i++) {
			mdelay(100);
			cc = aml_ci_bus_mem_read(ci_dev, 0, i);
			pr_dbg("0x%x ", cc);
			if ((i + 1) % 16 == 0)
				pr_dbg(" \r\n");
		}
		pr_dbg("READ CIS OVER\r\n");
		mdelay(1000);
		pr_dbg("SW rst CAM...\r\n");
		aml_ci_bus_io_write(ci_dev, 0, COM_STA_REG, RS);
		pr_dbg("SW rst over.\r\n");
		pr_dbg("-----------------------------------\r\n");
		pr_dbg("TO delay 2000ms\r\n");
		mdelay(2000);
		pr_dbg("\r\n");
		pr_dbg("--------------clear rs--!!!-YOU MUST CLEAR RS BIT--no sleep--------\r\n");
		aml_ci_bus_io_write(ci_dev, 0, COM_STA_REG, 0);
		pr_dbg("--------------sleep---------------------\r\n");
		mdelay(2000);
		pr_dbg("TO check sw-rst is OK\r\n");
		pr_dbg("start read fr \r\n");
		if (1) {
			unsigned char reg;
			unsigned char reg1;
			int count1 = 4000;
			while (1) {
				mdelay(20);
				count1--;
				reg1 = aml_ci_bus_io_read(
					ci_dev, 0, COM_STA_REG);
				if (FR != (FR & reg1)) {
						continue;
					} else {
						pr_dbg("CAM Reset Ok\r\n");
						break;
					}
			}
			reg = aml_ci_bus_io_read(ci_dev, 0, COM_STA_REG);
			pr_dbg("STA_REG = 0x%2.2x\r\n", reg);
			if (FR & reg) {
				pr_dbg("SW-RST is OK!\r\n");
			} else {
				pr_dbg("SW-RST is ERR!\r\n");
				goto end;
			}
		}
end:
		pr_dbg("TO check sw-rst over.\r\n");
		pr_dbg("\r\n");
		pr_dbg("-----------------------------------\r\n");
		pr_dbg("TO buffer size negotiation protocol...\r\n");
		pr_dbg("Get which buf size CAM can support\r\n");
		aml_ci_bus_io_write(ci_dev, 0, COM_STA_REG, SR);
		mdelay(1000);
		while (1) {

			reg = aml_ci_bus_io_read(ci_dev, 0, COM_STA_REG);
			if ((reg & DA) == DA) {
				pr_dbg("Buffer negotiate size date avalible.\r\n");
				break;
			} else {
				/*pr_dbg("Buffer negotiate
				size date NOT avalible\r\n");*/
				continue;
			}
			mdelay(100);
		}
		cnt = (aml_ci_bus_io_read(ci_dev, 0, SIZE_REG_L)) +
		((aml_ci_bus_io_read(ci_dev, 0, SIZE_REG_M)) * 256);
		pr_dbg("Moudle have <%d> Bytes send to host.\r\n", cnt);
		if (cnt != 2) {
			pr_dbg("The Bytes will be tx is ERR!\r\n");
			return;
		}
		for (i = 0; i < cnt; i++)
			buf[i] = aml_ci_bus_io_read(ci_dev, 0, DATA_REG);

		reg = aml_ci_bus_io_read(ci_dev, 0, COM_STA_REG);
		if (RE == (RE & reg)) {
			pr_dbg("(1)Read CAM buf size ERR!\r\n");
			return;
		}
		aml_ci_bus_io_write(ci_dev, 0, (COM_STA_REG), 0);

		mdelay(1000);

		while (count--) {
			reg = aml_ci_bus_io_read(ci_dev, 0, COM_STA_REG);
			if (FR != (FR & reg)) {
				pr_dbg("CAM is busy 2, waiting...\r\n");
				continue;
			} else {
				pr_dbg("CAM is OK 2.\r\n");
				break;
			}
		}
		reg = aml_ci_bus_io_read(ci_dev, 0, COM_STA_REG);
		if (FR != (FR & reg)) {
			pr_dbg("(2)Read CAM buf size ERR!-\r\n");
			return;
		}
		bsize = (buf[0] * 256) + buf[1];

		pr_dbg("CAM can support buf size is: <%d>B\r\n", bsize);

		pr_dbg("Tell CAM which size buf is be used\r\n");
		reg = aml_ci_bus_io_read(ci_dev, 0, COM_STA_REG);
		if (FR != (FR & reg))
			pr_dbg("CAM is busy, waiting free\r\n");
		while (1) {
			reg = aml_ci_bus_io_read(ci_dev, 0, COM_STA_REG);
			if (FR != (FR & reg)) {
				pr_dbg("CAM is busy 3, waiting\r\n");
				continue;
			} else {
				pr_dbg("CAM is OK 3\r\n");
				break;
			}
		}

		bsize = bsize - 0;
		BUF_SIZE = bsize;
		pr_dbg("We will use this  buf size: <%d>B\r\n", bsize);
		aml_ci_bus_io_write(ci_dev, 0, COM_STA_REG, SW);
		reg = aml_ci_bus_io_read(ci_dev, 0, COM_STA_REG);
		if (FR != (FR & reg))
			pr_dbg("CAM is busy, waiting\r\n");

		while (1) {
			reg = aml_ci_bus_io_read(ci_dev, 0, COM_STA_REG);
			if (FR != (FR & reg)) {
				pr_dbg("CAM is busy 4, waiting\r\n");
				continue;
			} else {
				pr_dbg("CAM is OK 4\r\n");
				break;
			}
		}
		/*SHOULD CHECK DA!!!!!*/
		/*PLS ADD THIS CHECK CODE:*/
		pr_dbg("PRIOR to check CAM'S DA\r\n");
		reg = aml_ci_bus_io_read(ci_dev, 0, COM_STA_REG);
		if ((reg & DA) == DA) {
			pr_dbg("CAM have data send to HOST\r\n");
			return;
		}


		buf[0] = (unsigned char)((bsize >> 8) & 0xff);
		buf[1] = (unsigned char)(bsize & 0xff);

		while (1) {
			mdelay(10);
			aml_ci_bus_io_write(ci_dev,
			0, COM_STA_REG, HC | SW);
			mdelay(100);
			reg = aml_ci_bus_io_read(ci_dev,
				0, COM_STA_REG);
			if (FR != (FR & reg)) {
				pr_dbg("CAM is busy 5, waiting\r\n");
				aml_ci_bus_io_write(ci_dev,
					0, COM_STA_REG, SW);
				continue;
			} else {
				pr_dbg("CAM is OK 5\r\n");
				break;
			}
		}
		pr_dbg("<2> Bytes send to CAM\r\n");
		aml_ci_bus_io_write(ci_dev, 0, SIZE_REG_M, 0);
		aml_ci_bus_io_write(ci_dev, 0, SIZE_REG_L, 2);
		for (i = 0; i < 2; i++)
			aml_ci_bus_io_write(ci_dev, 0, DATA_REG, buf[i]);

		reg = aml_ci_bus_io_read(ci_dev, 0, COM_STA_REG);
		if (WE == (WE & reg)) {
			pr_dbg("Write CAM ERR!\r\n");
			return;
		} else {
			aml_ci_bus_io_write(ci_dev, 0, COM_STA_REG, SW);
			mdelay(100);
			aml_ci_bus_io_write(ci_dev, 0, COM_STA_REG, 0);
			pr_dbg("Buffer size negotiation over!\r\n");
			pr_dbg("NOW, HOST can communicates with CAM\r\n");
			pr_dbg("NOW, TEST END\r\n");
		}
}

/**
* Read a tuple from attribute memory.
*
* @param ca CA instance.
* @param slot Slot id.
* @param address Address to read from. Updated.
* @param tupleType Tuple id byte. Updated.
* @param tupleLength Tuple length. Updated.
* @param tuple Dest buffer for tuple (must be 256 bytes). Updated.
*
* @return 0 on success, nonzero on error.
*/
static int dvb_ca_en50221_read_tuple(
int *address, int *tupleType, int *tupleLength, u8 *tuple)
{
	int i;
	int _tupleType;
	int _tupleLength;
	int _address = *address;

	/* grab the next tuple length and type */
	_tupleType = aml_ci_bus_mem_read((struct aml_ci *)
		ci_bus.priv, 0, _address);
	if (_tupleType < 0)
		return _tupleType;
	if (_tupleType == 0xff) {
		pr_dbg("END OF CHAIN TUPLE type:0x%x\n", _tupleType);
		*address += 2;
		*tupleType = _tupleType;
		*tupleLength = 0;
		return 0;
	}
	_tupleLength = aml_ci_bus_mem_read((struct aml_ci *)
		ci_bus.priv, 0, _address + 2);
	if (_tupleLength < 0)
		return _tupleLength;
	_address += 4;

	pr_dbg("TUPLE type:0x%x length:%i\n", _tupleType, _tupleLength);

	/* read in the whole tuple */
	for (i = 0; i < _tupleLength; i++) {
		tuple[i] = aml_ci_bus_mem_read((struct aml_ci *)
			ci_bus.priv, 0, _address + (i * 2));
		pr_dbg("  0x%02x: 0x%02x %c\n",
		i, tuple[i] & 0xff,
		((tuple[i] > 31) && (tuple[i] < 127)) ? tuple[i] : '.');
	}
	_address += (_tupleLength * 2);

	/* success */
	*tupleType = _tupleType;
	*tupleLength = _tupleLength;
	*address = _address;
	return 0;
}
static char *findstr(char *haystack, int hlen, char *needle, int nlen)
{
	int i;

	if (hlen < nlen)
		return NULL;

	for (i = 0; i <= hlen - nlen; i++) {
		if (!strncmp(haystack + i, needle, nlen))
			return haystack + i;
	}

	return NULL;
}

/**
* Parse attribute memory of a CAM module, extracting Config register, and checking
* it is a DVB CAM module.
*
* @param ca CA instance.
* @param slot Slot id.
*
* @return 0 on success, <0 on failure.
*/
static int dvb_ca_en50221_parse_attributes(void)
{
	int address = 0;
	int tupleLength;
	int tupleType;
	u8 tuple[257];
	char *dvb_str;
	int rasz;
	int status;
	int got_cftableentry = 0;
	int end_chain = 0;
	int i;
	u16 manfid = 0;
	u16 devid = 0;
	int config_base = 0;
	int config_option;

	/* CISTPL_DEVICE_0A */
	status = dvb_ca_en50221_read_tuple(&address,
	&tupleType, &tupleLength, tuple);
	if (status < 0) {
		pr_error("read status error\r\n");
		return status;
	}
	if (tupleType != 0x1D) {
		pr_error("read tupleType error [0x%x]\r\n", tupleType);
		return -EINVAL;
	}

	/* CISTPL_DEVICE_0C */
	status = dvb_ca_en50221_read_tuple(&address,
	&tupleType, &tupleLength, tuple);
	if (status < 0) {
		pr_error("read read cis  error\r\n");
		return status;
	}
	if (tupleType != 0x1C) {
		pr_error("read read cis type error\r\n");
		return -EINVAL;
	}

	/* CISTPL_VERS_1 */
	status = dvb_ca_en50221_read_tuple(&address,
	&tupleType, &tupleLength, tuple);
	if (status < 0) {
		pr_error("read read cis  version error\r\n");
		return status;
	}
	if (tupleType != 0x15) {
		pr_error("read read cis version type error\r\n");
		return -EINVAL;
	}

	/* CISTPL_MANFID */
	status = dvb_ca_en50221_read_tuple(&address, &tupleType,
	&tupleLength, tuple);
	if (status < 0) {
		pr_error("read read cis manfid error\r\n");
		return status;
	}
	if (tupleType != 0x20) {
		pr_error("read read cis manfid type error\r\n");
		return -EINVAL;
	}
	if (tupleLength != 4) {
		pr_error("read read cis manfid len error\r\n");
		return -EINVAL;
	}
	manfid = (tuple[1] << 8) | tuple[0];
	devid = (tuple[3] << 8) | tuple[2];

	/* CISTPL_CONFIG */
	status = dvb_ca_en50221_read_tuple(&address, &tupleType,
	&tupleLength, tuple);
	if (status < 0) {
		pr_error("read read cis config error\r\n");
		return status;
	}
	if (tupleType != 0x1A) {
		pr_error("read read cis config type error\r\n");
		return -EINVAL;
	}
	if (tupleLength < 3) {
		pr_error("read read cis config len error\r\n");
		return -EINVAL;
	}

	/* extract the configbase */
	rasz = tuple[0] & 3;
	if (tupleLength < (3 + rasz + 14)) {
		pr_error("read extract the configbase  error\r\n");
		return -EINVAL;
	}

	for (i = 0; i < rasz + 1; i++)
		config_base |= (tuple[2 + i] << (8 * i));

	/* check it contains the correct DVB string */
	dvb_str = findstr((char *)tuple, tupleLength, "DVB_CI_V", 8);
	if (dvb_str == NULL) {
		pr_error("find dvb str DVB_CI_V  error\r\n");
		return -EINVAL;
	}
	if (tupleLength < ((dvb_str - (char *) tuple) + 12)) {
		pr_error("find dvb str DVB_CI_V len error\r\n");
		return -EINVAL;
	}

	/* is it a version we support? */
	if (strncmp(dvb_str + 8, "1.00", 4)) {
		pr_error(" Unsupported DVB CAM module version %c%c%c%c\n",
		dvb_str[8], dvb_str[9], dvb_str[10], dvb_str[11]);
		return -EINVAL;
	}

	/* process the CFTABLE_ENTRY tuples, and any after those */
	while ((!end_chain) && (address < 0x1000)) {
		status = dvb_ca_en50221_read_tuple(&address, &tupleType,
		&tupleLength, tuple);
		if (status < 0) {
			pr_error("process the CFTABLE_ENTRY tuples error\r\n");
			return status;
		}

		switch (tupleType) {
			case 0x1B:	/* CISTPL_CFTABLE_ENTRY */
					if (tupleLength < (2 + 11 + 17))
						break;

					/* if we've already parsed one, just use it */
					if (got_cftableentry)
						break;

					/* get the config option */
					config_option = tuple[0] & 0x3f;

					/* OK, check it contains the correct strings */
					if ((findstr((char *)tuple,
						tupleLength, "DVB_HOST", 8) == NULL) ||
						(findstr((char *)tuple,
						tupleLength, "DVB_CI_MODULE", 13) == NULL))
						break;

					got_cftableentry = 1;
					break;

			case 0x14:	/* CISTPL_NO_LINK*/
					break;

			case 0xFF:	/* CISTPL_END */
					end_chain = 1;
					break;

			default:
				/* Unknown tuple type - just skip
				*this tuple and move to the next one
				*/
				pr_error("Skipping unknown tupletype:0x%x L:0x%x\n",
						tupleType, tupleLength);
					break;
				}
	}

	if ((address > 0x1000) || (!got_cftableentry)) {
		pr_error("got_cftableentry :%d\r\n", got_cftableentry);
		return -EINVAL;
	}

	pr_error("----------ci cis ok------\r\n");
	return 0;
}

static ssize_t reset_show(struct class *class,
struct class_attribute *attr, char *buf)
{
	int ret;
	ret = sprintf(buf, "echo 1 > %s\n\t", attr->attr.name);
	return ret;
}

static ssize_t reset_store(struct class *class,
struct class_attribute *attr, const char *buf, size_t size)
{
	int ret;
	struct aml_ci *ci = (struct aml_ci *)ci_bus.priv;
	ret = aml_ci_slot_reset(ci, 0);
	return size;
}
static CLASS_ATTR_RW(reset);
static ssize_t pwr_show(struct class *class,
struct class_attribute *attr, char *buf)
{
	int ret;
	ret = sprintf(buf, "echo 1|0> %s\n\t", attr->attr.name);
	return ret;
}

static ssize_t pwr_store(struct class *class,
struct class_attribute *attr, const char *buf, size_t size)
{
	int ret = 0;
	int enable = 0;
	long value;
	if (kstrtol(buf, 0, &value) == 0)
		enable = (int)value;
	ret = aml_gio_power(&ci_bus.pc, enable);
	return size;
}

static CLASS_ATTR_RW(pwr);

static ssize_t start_show(struct class *class,
struct class_attribute *attr, char *buf)
{
	int ret;
	ret = sprintf(buf, "start:%d\n", ci_bus.pc.start_work);
	return ret;
}

static ssize_t start_store(struct class *class,
struct class_attribute *attr, const char *buf, size_t size)
{
	int enable = 0;
	long value;
	if (kstrtol(buf, 0, &value) == 0) {
		enable = (int)value;
		ci_bus.pc.start_work = enable;
		printk("start set start\n");
		aml_pcmcia_detect_cam(&ci_bus.pc);
	}
	return size;
}

static CLASS_ATTR_RW(start);

static ssize_t wakeup_show(struct class *class,
struct class_attribute *attr, char *buf)
{
	int ret;
	ret = sprintf(buf, "wakeup:%d\n", ci_bus.wakeup_thread);
	return ret;
}

static ssize_t wakeup_store(struct class *class,
struct class_attribute *attr, const char *buf, size_t size)
{
	int enable = 0;
	long value;
	if (kstrtol(buf, 0, &value) == 0) {
		enable = (int)value;
		ci_bus.wakeup_thread = enable;
		printk("wakeup is set\n");
	}
	return size;
}

static CLASS_ATTR_RW(wakeup);

static ssize_t status_show(struct class *class,
struct class_attribute *attr, char *buf)
{
	int ret;
	struct aml_ci *ci = (struct aml_ci *)ci_bus.priv;
	ret = aml_ci_slot_status(ci, 0, 0);
	ret = sprintf(buf, "%s: %d;\n\t", attr->attr.name, ret);
	return ret;
}
static CLASS_ATTR_RO(status);

static ssize_t irq_show(struct class *class,
struct class_attribute *attr, char *buf)
{
	int ret;
	ret = aml_ci_gio_get_irq();
	ret = sprintf(buf, "%s irq: %d\n\t", attr->attr.name, ret);
	return ret;
}
static CLASS_ATTR_RO(irq);

static ssize_t iotest_show(struct class *class,
struct class_attribute *attr, char *buf)
{
	int ret;
	ret = sprintf(buf, "echo (r|w|f|c)(i|a) addr data > %s\n",
	attr->attr.name);
	return ret;
}

static ssize_t iotest_store(struct class *class,
struct class_attribute *attr, const char *buf, size_t size)
{
	int n = 0;
	int i = 0;
	char *buf_orig, *ps, *token;
	char *parm[3];
	unsigned int addr = 0, val = 0, retval = 0;
	long value = 0;
	struct aml_ci *ci = (struct aml_ci *)ci_bus.priv;
	buf_orig = kstrdup(buf, GFP_KERNEL);
	ps = buf_orig;
	while (1) {
		/*need set '\n' to ' \n'*/
		token = strsep(&ps, " ");
		if (token == NULL)
			break;
		if (*token == '\0')
			continue;
		parm[n++] = token;
	}

	if (!n || ((n > 0) && (strlen(parm[0]) != 2))) {
		pr_err("invalid command n[%x]p[%x][%s]\n", n,(int)strlen(parm[0]),parm[0]);
		kfree(buf_orig);
		return size;
	}

	if ((parm[0][0] == 'r')) {
		if (n > 2) {
			pr_err("read: invalid parameter\n");
			kfree(buf_orig);
			return size;
		}
	if (kstrtol(parm[1], 0, &value) == 0)
		addr = (int)value;
		pr_err("%s 0x%x\n", parm[0], addr);
		switch ((char)parm[0][1]) {
			case 'i':
				for (i = 0; i < 1000; i++)
					retval = aml_ci_bus_io_read(ci, 0, addr);
				break;
			case 'a':
				for (i = 0; i < 1000; i++)
					retval = aml_ci_bus_mem_read(ci, 0, addr);
				break;
			default:
				break;
		}
		pr_dbg("%s: 0x%x --> 0x%x\n", parm[0], addr, retval);
	} else if ((parm[0][0] == 'w')) {
		if (n != 3) {
			pr_err("write: invalid parameter\n");
			kfree(buf_orig);
			return size;
		}
		if (kstrtol(parm[1], 0, &value) == 0)
				addr = (int)value;
		if (kstrtol(parm[2], 0, &value) == 0)
				val = (int)value;

		pr_err("%s 0x%x 0x%x", parm[0], addr, val);
		switch ((char)parm[0][1]) {
			case 'i':
				retval = aml_ci_bus_io_write(ci, 0, addr, val);
				break;
			case 'a':
				retval = aml_ci_bus_mem_write(ci, 0, addr, val);
				break;
			default:
				break;
		}
		pr_dbg("%s: 0x%x <-- 0x%x\n", parm[0], addr, retval);
	} else if ((parm[0][0] == 'f')) {
		pr_dbg("full test----\r\n");
		aml_ci_bus_full_test(ci);
	}  else if ((parm[0][0] == 'p')) {
		pr_dbg("cis dvb_ca_en50221_parse_attributes----\r\n");
		dvb_ca_en50221_parse_attributes();
	}

	kfree(buf_orig);
	return size;
}

static CLASS_ATTR_RW(iotest);

static struct attribute *aml_ci_bus_attrs[] = {
	&class_attr_iotest.attr,
	&class_attr_status.attr,
	&class_attr_irq.attr,
	&class_attr_reset.attr,
	&class_attr_pwr.attr,
	&class_attr_start.attr,
	&class_attr_wakeup.attr,
	NULL
};

ATTRIBUTE_GROUPS(aml_ci_bus);


#define RAWCI_DEV_NAME "rawci"
int rawci_major = 0;

static int rawci_open(struct inode *inode, struct file *filp)
{
	int id = iminor(inode);

	if (ci_bus.used) {
		pr_error("smartcard %d already openned!", id);
		return -EBUSY;
	}

	ci_bus.used = 1;

	filp->private_data = &ci_bus;
	return 0;
}

static int rawci_close(struct inode *inode, struct file *filp)
{
	ci_bus.used = 0;
	return 0;
}

static ssize_t rawci_read(struct file *filp,
			char __user *buff, size_t size, loff_t *ppos)
{
	return size;
}

static ssize_t rawci_write(struct file *filp,
			 const char __user *buff, size_t size, loff_t *offp)
{
	return size;
}

static unsigned int rawci_poll(struct file *filp, struct poll_table_struct *wait)
{
	return 0;
}


static long rawci_ioctl(struct file *file, unsigned int cmd, ulong arg)
{
	struct aml_ci_bus *ci_bus_dev = (struct aml_ci_bus *)file->private_data;
	int ret = 0;
	long cr;
	int value;
	struct ci_rw_param param;
	memset(&param, 0, sizeof(struct ci_rw_param));

	switch (cmd) {
		case AMCI_IOC_RESET:
		{
			aml_pcmcia_reset(&(ci_bus_dev->pc));
		}
		break;
		case AMCI_IOC_IO:
		{
			cr = copy_from_user(&param, (void *)arg,
			sizeof(struct ci_rw_param));
			if (param.mode == AM_CI_IOW)
				aml_ci_bus_io_write(ci_bus_dev->priv, 0, param.addr, param.value);
			else if (param.mode == AM_CI_IOR)
				value = aml_ci_bus_io_read(ci_bus_dev->priv, 0, param.addr);
			else if (param.mode == AM_CI_MEMW)
				aml_ci_bus_mem_write(ci_bus_dev->priv, 0, param.addr, param.value);
			else if (param.mode == AM_CI_MEMR)
				value = aml_ci_bus_mem_read(ci_bus_dev->priv, 0, param.addr);

			param.value = (u8)value;
			cr = copy_to_user((void *)arg, &param, sizeof(struct ci_rw_param));
		}
		break;
		case AMCI_IOC_GET_DETECT:
		{
			int value = aml_gio_get_cd1(&(ci_bus_dev->pc));
			if (value == 1)
				value = 0;
			else
				value = 1;
			cr = copy_to_user((void *)arg, &value, sizeof(int));
		}
		break;
		case AMCI_IOC_SET_POWER:
		{
			int value = 0;
			cr = copy_from_user(&value, (void *)arg, sizeof(int));
			aml_gio_power(&(ci_bus_dev->pc), value > 0 ? AML_PWR_OPEN : AML_PWR_CLOSE);
		}
		break;
		default:
			ret = -EINVAL;
			break;
	}

	return ret;
}


#ifdef CONFIG_COMPAT
static long rawci_ioctl_compat(struct file *filp, unsigned int cmd,
			     unsigned long args)
{
	unsigned long ret;

	args = (unsigned long)compat_ptr(args);
	ret = rawci_ioctl(filp, cmd, args);
	return ret;
}
#endif

static const struct file_operations rawci_fops = {
	.owner = THIS_MODULE,
	.open = rawci_open,
	.write = rawci_write,
	.read = rawci_read,
	.release = rawci_close,
	.unlocked_ioctl = rawci_ioctl,
	.poll = rawci_poll,
#ifdef CONFIG_COMPAT
	.compat_ioctl = rawci_ioctl_compat,
#endif
};

int  aml_ci_bus_mod_init(void)
{
	int ret;
	struct class *clp;
	#define CLASS_NAME_LEN 48
	pr_dbg("Amlogic DVB CI BUS Init---\n");

	clp = &(ci_bus.cls);

	clp->name = kzalloc(CLASS_NAME_LEN, GFP_KERNEL);
	if (!clp->name)
		return -ENOMEM;

	snprintf((char *)clp->name, CLASS_NAME_LEN, "aml_ci_bus_%s", "test");
	clp->owner = THIS_MODULE;
	clp->class_groups = aml_ci_bus_groups;
	ret = class_register(clp);
	if (ret)
		kfree(clp->name);
	if (ci_bus.raw_mode == 1) {
		pr_dbg("register CI chrdev [%s]\n", RAWCI_DEV_NAME);
		rawci_major = register_chrdev(0, RAWCI_DEV_NAME, &rawci_fops);
		if (rawci_major <= 0) {
			pr_error("register CI chrdev error\n");
		} else {
			device_create(clp, NULL,
				MKDEV(rawci_major, 0), NULL,
				RAWCI_DEV_NAME);
		}
	} else {
		pr_dbg("not register CI chrdev [%s]\n", RAWCI_DEV_NAME);
	}

	return 0;
}

void  aml_ci_bus_mod_exit(void)
{
	pr_dbg("Amlogic DVB CI BUS Exit\n");
	if (ci_bus.raw_mode == 1 && rawci_major > 0)
		device_destroy(&(ci_bus.cls), MKDEV(rawci_major, 0));
	if (ci_bus.raw_mode == 1 && rawci_major > 0)
		unregister_chrdev(rawci_major, RAWCI_DEV_NAME);
	class_unregister(&(ci_bus.cls));
}

#endif
#if 0
module_init(aml_ci_bus_mod_init);
module_exit(aml_ci_bus_mod_exit);

MODULE_LICENSE("GPL");
#endif
