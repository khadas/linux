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
* FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
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
#include <linux/spi/spi.h>
#include <linux/amlogic/aml_gpio_consumer.h>
#include <linux/gpio/consumer.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/amlogic/sd.h>
#include <linux/of_irq.h>
#include <linux/irq.h>
#include <gpiolib.h>
#include "aml_spi.h"
#include "aml_ci.h"

#define AML_MODE_NAME       "aml_dvbci_spi"
static int  AML_CI_GPIO_IRQ_BASE = 251;
static struct aml_spi *g_spi_dev;
static int aml_spi_debug = 1;
static int G_rec_flag = AM_SPI_STEP_INIT;


module_param_named(spi_debug, aml_spi_debug, int, 0644);
MODULE_PARM_DESC(spi_debug, "enable verbose debug messages");


#define pr_dbg(args...)\
	do {\
		if (aml_spi_debug)\
			printk(args);\
	} while (0)
#define pr_error(fmt, args...) printk("AML_CI_SPI: " fmt, ## args)

struct spi_board_info aml_ci_spi_bdinfo = {
	.modalias = "ci_spi_dev",
	.mode = SPI_MODE_0,
	.max_speed_hz = 1000000, /* 1MHz */
	.bus_num = 0, /* SPI bus No. */
	.chip_select = 0, /* the device index on the spi bus */
	.controller_data = NULL,
};

#define NORMAL_MSG      (0<<7)
#define BROADCAST_MSG   (1<<7)
#define BLOCK_DATA      (0<<6)
#define SINGLE_DATA     (1<<6)
#define CISPI_DEV_ADDR  1

#define INPUT 0
#define OUTPUT 1
#define OUTLEVEL_LOW 0
#define OUTLEVEL_HIGH 1
#define PULLLOW 1
#define PULLHIGH 0

/*
 *sendbuf data struct
 *----------------------------------------------------
 *|start flag| cmd   | data  |  addr   |end flag  |
 *----------------------------------------------------
 *| 2 byte   | 1byte | 1byte |   2 byte|   2 byte |
 *----------------------------------------------------
 */

#define SENDBUFLEN  8
static u8 sendbuf[SENDBUFLEN];/* send data */
static u8 rbuf[SENDBUFLEN];/*save get data */
/**\brief aml_init_send_buf:init spi send buf
 * \param cmd: ci cmd
 * \param data: write value
 * \param addr: read or write addr
 * \return
 *   - read value:ok
 *   - -EINVAL : error
 */
static int aml_init_send_buf(u8 cmd, u8 data, u16 addr)
{
	/* start flag */
	sendbuf[0] = DATASTART;
	sendbuf[1] = DATASTART;
	/* cmd */
	sendbuf[2] = cmd;
	/* data */
	sendbuf[3] = data;
	/* addr senf low 8 bit first,and then send hi 8bit */
	sendbuf[4] = addr & 0x00ff;
	sendbuf[5] = (addr>>8) & 0xff;
	/* end flag */
	sendbuf[6] = DATAEND;
	sendbuf[7] = DATAEND;
	return 0;
}
/**\brief aml_ci_spi_reciver
 * \param[out] None
 * \param[in] value,get from spi
 * \return
 *   - 0:reciver end,-1:reciver
 *   -
 */
/*
 *data strouct
 *----------------------------------------------------
 *|start flag| cmd   | data  |  addr   |end flag  |
 *----------------------------------------------------
 *| 2 byte   | 1byte | 1byte |   2 byte|   2 byte |
 *----------------------------------------------------
 */
int aml_ci_spi_paser_bit(uint8_t value)
{
	/* read spi data from slave */
	if (G_rec_flag == AM_SPI_STEP_INIT) {
		/* start type first */
		if (value == DATASTART) {
			rbuf[0] = value;
			G_rec_flag = AM_SPI_STEP_START1;
		}
	} else if (G_rec_flag == AM_SPI_STEP_START1) {
		/* start2 type seccond */
		if (value == DATASTART) {
			rbuf[1] = value;
			G_rec_flag = AM_SPI_STEP_START2;
		}
	} else if (G_rec_flag == AM_SPI_STEP_START2) {
		/* cmd type */
		/* pr_dbg("spi value=%d\r\n",value); */
		rbuf[2] = value;
		G_rec_flag = AM_SPI_STEP_CMD;
	} else if (G_rec_flag == AM_SPI_STEP_CMD) {
		/* data  */
		rbuf[3] = value;
		G_rec_flag = AM_SPI_STEP_DATA;
	} else if (G_rec_flag == AM_SPI_STEP_DATA) {
		/* ADDR1  */
		rbuf[4] = value;
		G_rec_flag = AM_SPI_STEP_ADDR1;
	} else if (G_rec_flag == AM_SPI_STEP_ADDR1) {
		/* ADDR2 type */
		rbuf[5] = value;
		G_rec_flag = AM_SPI_STEP_ADDR2;
	} else if (G_rec_flag == AM_SPI_STEP_ADDR2) {
		/*  END1 type */
		if (value == DATAEND) {
			rbuf[6] = value;
			G_rec_flag = AM_SPI_STEP_END1;
		}
	} else if (G_rec_flag == AM_SPI_STEP_END1) {
		/* END2 type */
		if (value == DATAEND) {
			rbuf[7] = value;
			G_rec_flag = AM_SPI_STEP_END2;
			/* pr_dbg("spi read value ok end\r\n"); */
			return 0;
		}
	}
	return -1;
}

/**\brief aml_spi_io_api:spi read or write api with mcu
 * \param spi_dev: aml_spi obj,used this data to get spi obj
 * \param val: write value
 * \param len: write value len
 * \param mode: read or write
 * \return
 *   - read value:ok
 *   - -EINVAL : error
 */
static int aml_spi_io_api(struct aml_spi *spi_dev, u8 *val, int len, int mode)
{
	u8 rb[32] = {0};
	int ret = 0;
	int i = 0;
	u8 rd = 0;

	if (spi_dev->spi == NULL) {
		pr_error("%s spi is null\r\n", __func__);
		return -EINVAL;
	}
	spin_lock(&spi_dev->spi_lock);
	if (spi_dev->cs_hold_delay)
		udelay(spi_dev->cs_hold_delay);
	dirspi_start(spi_dev->spi);
	if (spi_dev->cs_clk_delay)
		udelay(spi_dev->cs_clk_delay);

	dirspi_xfer(spi_dev->spi, val, rb, len);
	/* wait mcu io */
	udelay(1000);
	/* init rec flag */
	G_rec_flag = AM_SPI_STEP_INIT;
	memset(rbuf, 0, 8);
	for (i = 0; i < 4*len; i++) {
		udelay(50);
		ret = dirspi_read(spi_dev->spi, &rd, 1);
		if (ret != 0)
			pr_dbg("spi read value timeout:%x\r\n", rd);
		ret = aml_ci_spi_paser_bit(rd);
		if (ret == 0)
			break;
	}
	if (ret == 0) {
		rd = rbuf[3];/* data */
	} else {
		pr_dbg("spi read value error\r\n");
		rd = 0;
	}

	if (spi_dev->cs_clk_delay)
		udelay(spi_dev->cs_clk_delay);

	/* pr_error("ci spi is stop in %s rd=%d\r\n",__func__,rd);*/
	dirspi_stop(spi_dev->spi);

	spin_unlock(&spi_dev->spi_lock);

	return rd;
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
static inline int aml_set_gpio_in(struct gpio_desc *gpio)
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
static inline int aml_get_gpio_value(struct gpio_desc *gpio)
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
/**\brief spi_get_gpio_by_name:get gpio desc from dts file
 * \param spi_dev: aml_spi obj
 * \param gpiod:   gpio_desc * obj
 * \param str: gpio name at dts file
 * \param input_output: gpio input or output type
 * \param output_value: gpio out put value
 * \return
 *   - 0:ok
 *   - -EINVAL : error
 */
static int spi_get_gpio_by_name(struct aml_spi *spi_dev,
struct gpio_desc **gpiod, int *pin_value,
char *str, int input_output, int output_level)
{
	int ret = 0;
	struct device_node *child = NULL;
	struct platform_device *pdev = spi_dev->pdev;
	struct device_node *np = pdev->dev.of_node;

	/*get spi and gpio config from dts*/
	/* get device config for dvbci_io*/
	child = of_get_child_by_name(np, "dvbci_io");
	if (IS_ERR(*gpiod)) {
		pr_dbg("dvb ci spi %s request failed\n", str);
		return -1;
	}

	*pin_value = of_get_named_gpio_flags(child, str, 0, NULL);
	*gpiod = gpio_to_desc(*pin_value);
	if (IS_ERR(*gpiod)) {
		pr_dbg("spi %s request failed\n", str);
		return -1;
	}
	pr_dbg("spi get_gpio %s %p  %d\n", str, *gpiod, *pin_value);
	gpio_request(*pin_value, AML_MODE_NAME);

	if (input_output == OUTPUT) {
		ret = gpiod_direction_output(*gpiod, output_level);
	} else if (input_output == INPUT)	{
		ret = gpiod_direction_input(*gpiod);
		/*ret |= gpiod_set_pullup(*gpiod, 1);*/
	} else {
		pr_error("spi Request gpio direction invalid\n");
	}
	return ret;
}
/********************************************************/
/********************************************************/
/*******             gpio api end           *************/
/********************************************************/
/********************************************************/
/**\brief aml_ci_cis_test_by_spi:test cis
 * \param ci_dev: aml_ci obj,used this data to get spi_dev obj
 * \param slot: slot index
 * \param addr: read addr
 * \return
 *   - test :ok
 *   - -EINVAL : error
 */
/**\brief aml_ci_full_test_by_spi:ci full test
 * \param ci_dev: aml_ci obj,used this data to get spi_dev obj
 * \param slot: slot index
 * \param addr: read addr
 * \return
 *   - read value:ok
 *   - -EINVAL : error
 */
static  int aml_ci_full_test_by_spi(
	struct aml_ci *ci_dev, int slot, int addr)
{
	u8  data = 0;
	u16 address = addr;
	int value = 0;
	struct aml_spi *spi_dev = ci_dev->data;

	aml_init_send_buf(AM_CI_CMD_FULLTEST, data, address);
	value = aml_spi_io_api(spi_dev,
		sendbuf, SENDBUFLEN, AM_CI_CMD_FULLTEST);
	pr_dbg("FULL : TEST END \r\n");
	return value;
}

/**\brief aml_ci_mem_read_by_spi:io read from cam
 * \param ci_dev: aml_ci obj,used this data to get spi_dev obj
 * \param slot: slot index
 * \param addr: read addr
 * \return
 *   - read value:ok
 *   - -EINVAL : error
 */
static  int aml_ci_mem_read_by_spi(
	struct aml_ci *ci_dev, int slot, int addr)
{
	u8  data = 0;
	u16 address = addr;
	int value = 0;
	struct aml_spi *spi_dev = ci_dev->data;

	aml_init_send_buf(AM_CI_CMD_MEMR, data, address);
	value = aml_spi_io_api(spi_dev, sendbuf, SENDBUFLEN, AM_CI_CMD_MEMR);
	/*pr_dbg("Read : mem[%d] = 0x%x\n", addr, value);*/
	return value;
}
/**\brief aml_ci_mem_write_by_spi:io write to cam by spi api
 * \param ci_dev: aml_ci obj,used this data to get spi_dev obj
 * \param slot: slot index
 * \param addr: write addr
 * \param addr: write value
 * \return
 *   - 0:ok
 *   - -EINVAL : error
 */
static int aml_ci_mem_write_by_spi(
	struct aml_ci *ci_dev, int slot, int addr,  u8 val)
{
	u8  data = val;
	u16 address = addr;
	int value = 0;
	struct aml_spi *spi_dev = ci_dev->data;

	aml_init_send_buf(AM_CI_CMD_MEMW, data, address);
	value = aml_spi_io_api(spi_dev, sendbuf, SENDBUFLEN, AM_CI_CMD_MEMW);
	/*pr_dbg("write : mem[%d] = 0x%x\n", addr, data);*/
	return value;
}
/**\brief aml_ci_io_read_by_spi:io read from cam by spi api
 * \param ci_dev: aml_ci obj,used this data to get spi_dev obj
 * \param slot: slot index
 * \param addr: read addr
 * \return
 *   - read value:ok
 *   - -EINVAL : error
 */
static int aml_ci_io_read_by_spi(
	struct aml_ci *ci_dev, int slot, int addr)
{
	u8  data = 0;
	u16 address = addr;
	int value = 0;
	struct aml_spi *spi_dev = ci_dev->data;

	aml_init_send_buf(AM_CI_CMD_IOR, data, address);
	value = aml_spi_io_api(spi_dev, sendbuf, SENDBUFLEN, AM_CI_CMD_IOR);
	/*pr_dbg("read : io[%d] = 0x%x\n", addr, value);*/
	return value;
}
/**\brief aml_ci_io_write_by_spi:io write to cam
 * \param ci_dev: aml_ci obj,used this data to get spi_dev obj
 * \param slot: slot index
 * \param addr: write addr
 * \param addr: write value
 * \return
 *   - 0:ok
 *   - -EINVAL : error
 */
static int aml_ci_io_write_by_spi(
	struct aml_ci *ci_dev, int slot, int addr, u8 val)
{
	u8  data = val;
	u16 address = addr;
	int value = 0;
	struct aml_spi *spi_dev = ci_dev->data;
	/*add by chl,need add time delay*/
	mdelay(10);
	aml_init_send_buf(AM_CI_CMD_IOW, data, address);
	value = aml_spi_io_api(spi_dev, sendbuf, SENDBUFLEN, AM_CI_CMD_IOW);
	/*pr_dbg("write : ATTR[%d] = 0x%x\n", addr, data);*/
	return value;
}


/**\brief aml_ci_slot_reset:reset slot
 * \param ci_dev: aml_ci obj,used this data to get spi_dev obj
 * \param slot: slot index
 * \return
 *   - 0:ok
 *   - -EINVAL : error
 */
static int aml_ci_slot_reset(struct aml_ci *ci_dev, int slot)
{
	struct aml_spi *spi_dev = ci_dev->data;

	pr_dbg("Slot(%d): Slot RESET\n", slot);
	aml_pcmcia_reset(&spi_dev->pc);
	dvb_ca_en50221_camready_irq(&ci_dev->en50221, 0);
	return 0;
}
/**\brief aml_ci_slot_shutdown:show slot
 * \param ci_dev: aml_ci obj,used this data to get spi_dev obj
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
 * \param ci_dev: aml_ci obj,used this data to get spi_dev obj
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
 * \param ci_dev: aml_ci obj,used this data to get spi_dev obj
 * \param slot: slot index
 * \param open: no used
 * \return
 *   - cam status
 *   - -EINVAL : error
 */
static int aml_ci_slot_status(struct aml_ci *ci_dev, int slot, int open)
{
	struct aml_spi *spi_dev = ci_dev->data;

	pr_dbg("Slot(%d): Poll Slot status\n", slot);

	if (spi_dev->pc.slot_state == MODULE_INSERTED) {
		pr_dbg("CA Module present and ready\n");
		return DVB_CA_EN50221_POLL_CAM_PRESENT |
		DVB_CA_EN50221_POLL_CAM_READY;
	} else {
		pr_error("CA Module not present or not ready\n");
	}
	return -EINVAL;
}

/**\brief aml_ci_gio_get_irq:get gpio cam irq pin value
 * \return
 *   - irq pin value
 *   - -EINVAL : error
 */
static int aml_ci_gio_get_irq(void)
{
	int ret = 0;

	ret = aml_get_gpio_value(g_spi_dev->irq_cam_pin);
	return ret;
}
/********************************************************/
/********************************************************/
/*******        for pcmcid api              *************/
/********************************************************/
/********************************************************/
/**\brief aml_gio_power:set power gpio hi or low
 * \param pc: aml_pcmcia obj,used this priv to get spi_dev obj
 * \param enable: power pin hi or low
 * \return
 *   - 0
 *   - -EINVAL : error
 */
static int aml_gio_power(struct aml_pcmcia *pc, int enable)
{
	int ret = 0;
	struct aml_spi *spi_dev = pc->priv;

	if (spi_dev == NULL) {
		pr_dbg("spi dev is null %s : %d\r\n", __func__, enable);
		return -1;
	}
	pr_dbg("%s : %d\r\n", __func__, enable);
	if (enable == AML_PWR_OPEN) {
		/*hi level ,open power*/
		ret = aml_set_gpio_out(spi_dev->pwr_pin, AML_GPIO_HIGH);
	} else {
		/*low level ,close power*/
		ret = aml_set_gpio_out(spi_dev->pwr_pin, AML_GPIO_LOW);
	}
	return ret;
}
/**\brief aml_gio_reset:set reset gpio hi or low
 * \param pc: aml_pcmcia obj,used this priv to get spi_dev obj
 * \param enable: reset pin hi or low
 * \return
 *   - 0
 *   - -EINVAL : error
 */
static int aml_gio_reset(struct aml_pcmcia *pc, int enable)
{
	/*need set hi and sleep set low*/
	int ret = 0;
	struct aml_spi *spi_dev = pc->priv;

	pr_dbg("%s : %d\r\n", __func__, enable);
	if (enable == AML_L)
		ret = aml_set_gpio_out(spi_dev->reset_pin, AML_GPIO_LOW);
	else
		ret = aml_set_gpio_out(spi_dev->reset_pin, AML_GPIO_HIGH);
	return ret;
}

/**\brief aml_gio_init_irq:set gpio irq
 * \param pc: aml_pcmcia obj,used this priv to get spi_dev obj
 * \param flag: rising or falling or hi or low
 * \return
 *   - 0
 *   - -EINVAL : error
 */
/*need change*/
static int aml_gio_init_irq(struct aml_pcmcia *pc, int flag)
{
	struct aml_spi *spi_dev = (struct aml_spi *)pc->priv;

#if 0
	int cd1_pin = desc_to_gpio(spi_dev->cd_pin1);

	int irq = pc->irq-AML_CI_GPIO_IRQ_BASE;

	printk("----cd1_pin=%d irq=%d\r\n", cd1_pin, irq);
	aml_set_gpio_in(spi_dev->cd_pin1);

	if (flag == IRQF_TRIGGER_RISING)
		gpio_for_irq(cd1_pin,
			AML_GPIO_IRQ(irq, FILTER_NUM7, GPIO_IRQ_RISING));
	else if (flag == IRQF_TRIGGER_FALLING)
		gpio_for_irq(cd1_pin,
			AML_GPIO_IRQ(irq, FILTER_NUM7, GPIO_IRQ_FALLING));
	else if (flag == IRQF_TRIGGER_HIGH)
		gpio_for_irq(cd1_pin,
			AML_GPIO_IRQ(irq, FILTER_NUM7, GPIO_IRQ_HIGH));
	else if (flag == IRQF_TRIGGER_LOW)
		gpio_for_irq(cd1_pin,
			AML_GPIO_IRQ(irq, FILTER_NUM7, GPIO_IRQ_LOW));
	else
		return -1;
#endif
	gpiod_to_irq(spi_dev->cd_pin1);

	return 0;
}
/**\brief aml_gio_get_cd1:get gpio cd1 pin value
 * \param pc: aml_pcmcia obj,used this priv to get spi_dev obj
 * \return
 *   - cd1 pin value
 *   - -EINVAL : error
 */
static int aml_gio_get_cd1(struct aml_pcmcia *pc)
{
	int ret = 0;
	struct aml_spi *spi_dev = pc->priv;

	ret = aml_get_gpio_value(spi_dev->cd_pin1);
	return ret;
}
/**\brief aml_gio_get_cd2:get gpio cd2 pin value
 * \param pc: aml_pcmcia obj,used this priv to get spi_dev obj
 * \return
 *   - cd2 pin value
 *   - -EINVAL : error
 */
static int aml_gio_get_cd2(struct aml_pcmcia *pc)
{
	int ret = 0;
	struct aml_spi *spi_dev = pc->priv;

	ret = aml_get_gpio_value(spi_dev->cd_pin2);
	pr_dbg("%s : %d\r\n", __func__, ret);
	return ret;
}
/**\brief aml_cam_plugin:notify en50221 cam card in or out
 * \param pc: aml_pcmcia obj,used this priv to get spi_dev obj
 * \plugin: 0:remove;1:in
 * \return
 *   - 0
 *   - -EINVAL : error
 */
static int aml_cam_plugin(struct aml_pcmcia *pc, int plugin)
{
	struct aml_ci *ci = (struct aml_ci *)
	((struct aml_spi *)(pc->priv))->priv;
	pr_dbg("%s : %d\r\n", __func__, plugin);
	if (plugin) {
		dvb_ca_en50221_camchange_irq(&ci->en50221,
			0, DVB_CA_EN50221_CAMCHANGE_INSERTED);
	} else {
		dvb_ca_en50221_camchange_irq(&ci->en50221,
			0, DVB_CA_EN50221_CAMCHANGE_REMOVED);
	}
	return 0;
}
/**\brief aml_pcmcia_alloc:alloc nad init pcmcia obj
 * \param spi_dev: aml_spi obj,
 * \param pcmcia: aml_pcmcia * obj,
 * \return
 *   - 0
 *   - -EINVAL : error
 */
static void aml_pcmcia_alloc(struct aml_spi *spi_dev,
	struct aml_pcmcia **pcmcia)
{
	pr_dbg("aml_pcmcia_alloc----\n");
	*pcmcia = &spi_dev->pc;
	(*pcmcia)->irq = spi_dev->irq;
	(*pcmcia)->init_irq = aml_gio_init_irq;
	(*pcmcia)->get_cd1 = aml_gio_get_cd1;
	(*pcmcia)->get_cd2 = aml_gio_get_cd2;
	(*pcmcia)->pwr = aml_gio_power;
	(*pcmcia)->rst = aml_gio_reset;
	(*pcmcia)->pcmcia_plugin = aml_cam_plugin;
	(*pcmcia)->slot_state = MODULE_XTRACTED;
	(*pcmcia)->priv = spi_dev;
	(*pcmcia)->run_type = 0;/*0:irq;1:poll*/
}

/**\brief aml_spi_get_config_from_dts:get spi config and gpio config from dts
 * \param spi_dev: aml_spi obj,
 * \return
 *   - 0
 *   - -EINVAL : error
 */
static int aml_spi_get_config_from_dts(struct aml_spi *spi_dev)
{
	struct device_node *child = NULL;
	struct platform_device *pdev = spi_dev->pdev;
	struct device_node *np = pdev->dev.of_node;
	unsigned int temp[5], val;
	int ret = 0;

	pr_dbg("into get spi dts \r\n");

	/*get spi and gpio config from dts*/
	/* get device config for dvbci_io*/
	child = of_get_child_by_name(np, "dvbci_io");
	if (child == NULL) {
		pr_error("failed to get dvbci_io\n");
		return -1;
	}
	spi_dev->spi_bdinfo = &aml_ci_spi_bdinfo;
	/* get spi config */
	ret = of_property_read_u32_array(child, "spi_bus_num", temp, 1);
	if (ret) {
		pr_error("failed to get spi_bus_num\n");
	} else {
		aml_ci_spi_bdinfo.bus_num = temp[0];
		pr_dbg("bus_num: %d\n", aml_ci_spi_bdinfo.bus_num);
	}
	ret = of_property_read_u32_array(child, "spi_chip_select",
	temp, 1);
	if (ret) {
		pr_error("failed to get spi_chip_select\n");
	} else {
		aml_ci_spi_bdinfo.chip_select = temp[0];
		pr_dbg("chip_select: %d\n", aml_ci_spi_bdinfo.chip_select);
	}
	ret = of_property_read_u32_array(child, "spi_max_frequency",
	temp, 1);
	if (ret) {
		pr_error("failed to get spi_chip_select\n");
	} else {
		aml_ci_spi_bdinfo.max_speed_hz = temp[0];
		pr_dbg("max_speed_hz: %d\n", aml_ci_spi_bdinfo.max_speed_hz);
	}
	ret = of_property_read_u32_array(child, "spi_mode", temp, 1);
	if (ret) {
		pr_error("failed to get spi_mode\n");
	} else {
		aml_ci_spi_bdinfo.mode = temp[0];
		pr_dbg("mode: %d\n", aml_ci_spi_bdinfo.mode);
	}
	ret = of_property_read_u32_array(child, "spi_cs_delay",
	&temp[0], 2);
	if (ret) {
		spi_dev->cs_hold_delay = 0;
		spi_dev->cs_clk_delay = 0;
	} else {
		spi_dev->cs_hold_delay = temp[0];
		spi_dev->cs_clk_delay = temp[1];
	}
	ret = of_property_read_u32(child, "spi_write_check", &val);
	if (ret)
		spi_dev->write_check = 0;
	else
		spi_dev->write_check = (unsigned char)val;
	/*get  cd1 irq num*/
	ret = of_property_read_u32(child, "irq_cd1", &val);
	if (ret) {
		spi_dev->irq = 5;
	} else {
		/*set irq value need add
		 *AML_CI_GPIO_IRQ_BASE,but
		 *we need minus
		 *AML_CI_GPIO_IRQ_BASE
		 *when gpio request irq
		 */
		spi_dev->irq = val+AML_CI_GPIO_IRQ_BASE;
	}

	spi_dev->irq = irq_of_parse_and_map(
	pdev->dev.of_node, 0);
	AML_CI_GPIO_IRQ_BASE = spi_dev->irq - val;
	pr_dbg("get spi irq : %d  0:%d USEDBASE:%d val:%d\r\n",
		spi_dev->irq, INT_GPIO_0, AML_CI_GPIO_IRQ_BASE, val);
	/*get reset pwd cd1 cd2 gpio pin*/
	spi_dev->reset_pin = NULL;
	ret = spi_get_gpio_by_name(spi_dev, &spi_dev->reset_pin,
	&spi_dev->reset_pin_value, "reset_pin",
	OUTPUT, OUTLEVEL_HIGH);
	if (ret) {
		pr_error("dvb ci reset pin request failed\n");
		return -1;
	}
	spi_dev->cd_pin1 = NULL;
	ret = spi_get_gpio_by_name(spi_dev,
		&spi_dev->cd_pin1,
		&spi_dev->cd_pin1_value, "cd_pin1",
		INPUT, OUTLEVEL_HIGH);
	if (ret) {
		pr_error("dvb ci cd_pin1 pin request failed\n");
		return -1;
	}
	spi_dev->cd_pin2 = spi_dev->cd_pin1;
	spi_dev->cd_pin2_value = spi_dev->cd_pin1_value;
	/*set irq*/
	spi_dev->irq = gpiod_to_irq(spi_dev->cd_pin1);
	spi_dev->pwr_pin = NULL;
	pr_dbg("spi_dev->cd_pin1_value==%d irq=%d\r\n",
			spi_dev->cd_pin1_value, spi_dev->irq);
	ret = spi_get_gpio_by_name(spi_dev,
		&spi_dev->pwr_pin, &spi_dev->pwr_pin_value,
		"pwr_pin", OUTPUT, OUTLEVEL_HIGH);
	if (ret) {
		pr_error("dvb ci pwr_pin pin request failed\n");
		return -1;
	}
	spi_dev->irq_cam_pin = NULL;
	ret = spi_get_gpio_by_name(spi_dev,
		&spi_dev->irq_cam_pin, &spi_dev->irq_cam_pin_value,
		"irq_cam_pin", INPUT, OUTLEVEL_HIGH);
	if (ret) {
		pr_error("dvbci  irq_cam_pin pin request failed\n");
		return -1;
	}

	return 0;
}
/**\brief aml_ci_free_gpio:free ci gpio
 * \param spi_dev: aml_spi obj,
 * \return
 *   - 0
 *   - -EINVAL : error
 */
static void aml_ci_free_gpio(struct aml_spi *spi_dev)
{
	if (spi_dev == NULL) {
		pr_error("spi_dev is NULL,no need free gpio res\r\n");
		return;
	}

	if (spi_dev->pwr_pin) {
		aml_gpio_free(spi_dev->pwr_pin);
		spi_dev->pwr_pin = NULL;
	}
	if (spi_dev->cd_pin1) {
		aml_gpio_free(spi_dev->cd_pin1);
		spi_dev->cd_pin1 = NULL;
		spi_dev->cd_pin2 = NULL;
	}
	if (spi_dev->reset_pin) {
		aml_gpio_free(spi_dev->reset_pin);
		spi_dev->reset_pin = NULL;
	}
	if (spi_dev->irq_cam_pin) {
		aml_gpio_free(spi_dev->irq_cam_pin);
		spi_dev->irq_cam_pin = NULL;
	}
}


/**\brief ci_spi_dev_remove:spi probe api
 * \param spi: spi obj,
 * \return
 *   - 0
 *   - -EINVAL : error
 */
static int ci_spi_dev_probe(struct spi_device *spi)
{
	int ret;

	pr_dbg("spi Dev probe--\n");
	if (g_spi_dev)
		g_spi_dev->spi = spi;
	else
		pr_dbg("spi Dev probe-error-\n");
	spi->bits_per_word = 8;
	ret = spi_setup(spi);
	if (ret)
		pr_dbg("spi setup failed\n");
	return ret;
}
/**\brief ci_spi_dev_remove:spi remove api
 * \param spi: spi obj,
 * \return
 *   - 0
 *   - -EINVAL : error
 */
static int ci_spi_dev_remove(struct spi_device *spi)
{
	pr_dbg("spi Dev remove--\n");
	if (g_spi_dev)
		g_spi_dev->spi = NULL;

	return 0;
}

static struct spi_driver ci_spi_dev_driver = {
	.probe = ci_spi_dev_probe,
	.remove = ci_spi_dev_remove,
	.driver = {
		.name = "ci_spi_dev",/*set same with board info modalias*/
		.owner = THIS_MODULE,
	},
};
/**\brief aml_spi_init:spi_dev init
 * \param ci_dev: aml_ci obj,
 * \param pdev: platform_device obj,used to get dts info
 * \return
 *   - 0
 *   - -EINVAL : error
 */
int aml_spi_init(struct platform_device *pdev, struct aml_ci *ci_dev)
{
	struct aml_spi *spi_dev = NULL;
	struct aml_pcmcia *pc;
	int result;

	spi_dev = kmalloc(sizeof(struct aml_spi), GFP_KERNEL);
	if (!spi_dev) {
		pr_error("Out of memory!, exiting ..\n");
		result = -ENOMEM;
		goto err;
	}
	g_spi_dev = spi_dev;
	spi_dev->pdev = pdev;
	spi_dev->priv = ci_dev;
	/*get config from dts*/
	aml_spi_get_config_from_dts(spi_dev);
	/*regist api dev*/
	spi_register_board_info(spi_dev->spi_bdinfo, 1);
	result = spi_register_driver(&ci_spi_dev_driver);
	if (result) {
		pr_error("register amlspi_dev spi driver failed\n");
		goto fail1;
	}
	aml_pcmcia_alloc(spi_dev, &pc);
	result = aml_pcmcia_init(pc);
	if (result < 0)
		pr_error("aml_pcmcia_init failed\n");

	/*init ci_dev used api.*/
	ci_dev->ci_mem_read  = aml_ci_mem_read_by_spi;
	ci_dev->ci_mem_write = aml_ci_mem_write_by_spi;
	ci_dev->ci_io_read = aml_ci_io_read_by_spi;
	ci_dev->ci_io_write = aml_ci_io_write_by_spi;
	ci_dev->ci_slot_reset = aml_ci_slot_reset;
	ci_dev->ci_slot_shutdown = aml_ci_slot_shutdown;
	ci_dev->ci_slot_ts_enable = aml_ci_ts_control;
	ci_dev->ci_poll_slot_status = aml_ci_slot_status;
	ci_dev->data = spi_dev;
	/*init spi_lock*/
	spin_lock_init(&(spi_dev->spi_lock));
	return 0;
	spi_unregister_driver(&ci_spi_dev_driver);
fail1:
		kfree(spi_dev);
		spi_dev = NULL;
err:
	return -1;
}
EXPORT_SYMBOL(aml_spi_init);
/**\brief aml_spi_exit:spi exit
 * \return
 *   - 0
 *   - -EINVAL : error
 */
int aml_spi_exit(void)
{
	/*exit pc card*/
	aml_pcmcia_exit(&g_spi_dev->pc);
	/*un regist spi driver*/
	spi_unregister_driver(&ci_spi_dev_driver);
	/*free gpio*/
	aml_ci_free_gpio(g_spi_dev);
	/*free spi dev*/
	kfree(g_spi_dev);
	g_spi_dev = NULL;

	return 0;
}
EXPORT_SYMBOL(aml_spi_exit);

/********************************************************/
/********************************************************/
/*******        for spi test api            *************/
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
static void aml_spi_ca_full_test(struct aml_ci *ci_dev)
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
		for (i = 0; i < 267; i++) {
			mdelay(100);
			cc = aml_ci_mem_read_by_spi(ci_dev, 0, i);
			pr_dbg("0x%x ", cc);
			if ((i + 1) % 16 == 0)
				pr_dbg(" \r\n");
		}
		pr_dbg("READ CIS OVER\r\n");
		mdelay(1000);
		pr_dbg("SW rst CAM...\r\n");
		aml_ci_io_write_by_spi(ci_dev, 0, COM_STA_REG, RS);
		pr_dbg("SW rst over.\r\n");
		pr_dbg("-----------------------------------\r\n");
		pr_dbg("TO delay 2000ms\r\n");
		mdelay(2000);
		pr_dbg("\r\n");
		pr_dbg("--------------clear rs--!!!-YOU MUST CLEAR RS BIT--no sleep--------\r\n");
		aml_ci_io_write_by_spi(ci_dev, 0, COM_STA_REG, 0);
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
				reg1 = aml_ci_io_read_by_spi(
					ci_dev, 0, COM_STA_REG);
				if (FR != (FR & reg1)) {
					continue;
				} else {
					pr_dbg("CAM Reset Ok\r\n");
					break;
				}
			}
			reg = aml_ci_io_read_by_spi(ci_dev, 0, COM_STA_REG);
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
		aml_ci_io_write_by_spi(ci_dev, 0, COM_STA_REG, SR);
		mdelay(1000);
		while (1) {

			reg = aml_ci_io_read_by_spi(ci_dev, 0, COM_STA_REG);
			if ((reg & DA) == DA) {
				pr_dbg("Buffer negotiate size date avalible.\r\n");
				break;
			}
			{
				/*pr_dbg("Buffer negotiate
				 *size date NOT avalible\r\n");
				 */
				continue;
			}
			mdelay(100);
		}
		cnt = (aml_ci_io_read_by_spi(ci_dev, 0, SIZE_REG_L)) +
		((aml_ci_io_read_by_spi(ci_dev, 0, SIZE_REG_M)) * 256);
		pr_dbg("Moudle have <%d> Bytes send to host.\r\n", cnt);
		if (cnt != 2) {
			pr_dbg("The Bytes will be tx is ERR!\r\n");
			return;
		}
		for (i = 0; i < cnt; i++)
			buf[i] = aml_ci_io_read_by_spi(ci_dev, 0, DATA_REG);

		reg = aml_ci_io_read_by_spi(ci_dev, 0, COM_STA_REG);
		if (RE == (RE & reg)) {
			pr_dbg("(1)Read CAM buf size ERR!\r\n");
			return;
		}
		aml_ci_io_write_by_spi(ci_dev, 0, (COM_STA_REG), 0);

		mdelay(1000);

		while (count--) {
			reg = aml_ci_io_read_by_spi(ci_dev, 0, COM_STA_REG);
			if (FR != (FR & reg)) {
				pr_dbg("CAM is busy 2, waiting...\r\n");
				continue;
			} else {
				pr_dbg("CAM is OK 2.\r\n");
				break;
			}
		}
		reg = aml_ci_io_read_by_spi(ci_dev, 0, COM_STA_REG);
		if (FR != (FR & reg)) {
			pr_dbg("(2)Read CAM buf size ERR!-\r\n");
			return;
		}
		bsize = (buf[0] * 256) + buf[1];

		pr_dbg("CAM can support buf size is: <%d>B\r\n", bsize);

		pr_dbg("Tell CAM which size buf is be used\r\n");
		reg = aml_ci_io_read_by_spi(ci_dev, 0, COM_STA_REG);
		if (FR != (FR & reg))
			pr_dbg("CAM is busy, waiting free\r\n");
		while (1) {
			reg = aml_ci_io_read_by_spi(ci_dev, 0, COM_STA_REG);
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
		aml_ci_io_write_by_spi(ci_dev, 0, COM_STA_REG, SW);
		reg = aml_ci_io_read_by_spi(ci_dev, 0, COM_STA_REG);
		if (FR != (FR & reg))
			pr_dbg("CAM is busy, waiting\r\n");

		while (1) {
			reg = aml_ci_io_read_by_spi(ci_dev, 0, COM_STA_REG);
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
		reg = aml_ci_io_read_by_spi(ci_dev, 0, COM_STA_REG);
		if ((reg & DA) == DA) {
			pr_dbg("CAM have data send to HOST\r\n");
			return;
		}


		buf[0] = (unsigned char)((bsize >> 8) & 0xff);
		buf[1] = (unsigned char)(bsize & 0xff);

		while (1) {
			mdelay(10);
			aml_ci_io_write_by_spi(ci_dev,
			0, COM_STA_REG, HC | SW);
			mdelay(100);
			reg = aml_ci_io_read_by_spi(ci_dev,
				0, COM_STA_REG);
			if (FR != (FR & reg)) {
				pr_dbg("CAM is busy 5, waiting\r\n");
				aml_ci_io_write_by_spi(ci_dev,
					0, COM_STA_REG, SW);
				continue;
			} else {
				pr_dbg("CAM is OK 5\r\n");
				break;
			}
		}
		pr_dbg("<2> Bytes send to CAM\r\n");
		aml_ci_io_write_by_spi(ci_dev, 0, SIZE_REG_M, 0);
		aml_ci_io_write_by_spi(ci_dev, 0, SIZE_REG_L, 2);
		for (i = 0; i < 2; i++)
			aml_ci_io_write_by_spi(ci_dev, 0, DATA_REG, buf[i]);

		reg = aml_ci_io_read_by_spi(ci_dev, 0, COM_STA_REG);
		if (WE == (WE & reg)) {
			pr_dbg("Write CAM ERR!\r\n");
			return;
		}
		{
			aml_ci_io_write_by_spi(ci_dev, 0, COM_STA_REG, SW);
			mdelay(100);
			aml_ci_io_write_by_spi(ci_dev, 0, COM_STA_REG, 0);
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
	_tupleType = aml_ci_mem_read_by_spi((struct aml_ci *)
		g_spi_dev->priv, 0, _address);
	if (_tupleType < 0)
		return _tupleType;
	if (_tupleType == 0xff) {
		pr_dbg("END OF CHAIN TUPLE type:0x%x\n", _tupleType);
		*address += 2;
		*tupleType = _tupleType;
		*tupleLength = 0;
		return 0;
	}
	_tupleLength = aml_ci_mem_read_by_spi((struct aml_ci *)
		g_spi_dev->priv, 0, _address + 2);
	if (_tupleLength < 0)
		return _tupleLength;
	_address += 4;

	pr_dbg("TUPLE type:0x%x length:%i\n", _tupleType, _tupleLength);

	/* read in the whole tuple */
	for (i = 0; i < _tupleLength; i++) {
		tuple[i] = aml_ci_mem_read_by_spi((struct aml_ci *)
			g_spi_dev->priv, 0, _address + (i * 2));
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
 * Parse attribute memory of a CAM module,
 * extracting Config register, and checking
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

	pr_error("----------ci cis ok-----\r\n");
	return 0;
}

static ssize_t aml_spi_ci_reset_help(struct class *class,
struct class_attribute *attr, char *buf)
{
	int ret;

	ret = sprintf(buf, "echo 1 > %s\n\t", attr->attr.name);
	return ret;
}

static ssize_t aml_spi_ci_reset(struct class *class,
struct class_attribute *attr, const char *buf, size_t size)
{
	int ret;
	struct aml_ci *ci = (struct aml_ci *)g_spi_dev->priv;

	ret = aml_ci_slot_reset(ci, 0);
	return size;
}

static ssize_t aml_spi_ci_pwr_help(struct class *class,
struct class_attribute *attr, char *buf)
{
	int ret;

	ret = sprintf(buf, "echo 1|0> %s\n\t", attr->attr.name);
	return ret;
}

static ssize_t aml_spi_ci_pwr(struct class *class,
struct class_attribute *attr, const char *buf, size_t size)
{
	int ret = 0;
	int enable = 0;
	long value;

	if (kstrtol(buf, 0, &value) == 0)
		enable = (int)value;
	ret = aml_gio_power(&g_spi_dev->pc, enable);
	return size;
}
static ssize_t aml_spi_ci_state_show(struct class *class,
struct class_attribute *attr, char *buf)
{
	int ret;
	struct aml_ci *ci = (struct aml_ci *)g_spi_dev->priv;

	ret = aml_ci_slot_status(ci, 0, 0);
	ret = sprintf(buf, "%s: %d;\n\t", attr->attr.name, ret);
	return ret;
}

static ssize_t aml_spi_ci_irq_show(struct class *class,
struct class_attribute *attr, char *buf)
{
	int ret;

	ret = aml_ci_gio_get_irq();
	ret = sprintf(buf, "%s irq: %d\n\t", attr->attr.name, ret);
	return ret;
}

static ssize_t aml_spi_io_test_help(struct class *class,
struct class_attribute *attr, char *buf)
{
	int ret;

	ret = sprintf(buf, "echo (r|w|f|c)(i|a) addr data > %s\n",
	attr->attr.name);
	return ret;
}

static ssize_t aml_spi_io_test(struct class *class,
struct class_attribute *attr, const char *buf, size_t size)
{
	int n = 0;
	char *buf_orig, *ps, *token;
	char *parm[3];
	unsigned int addr = 0, val = 0, retval = 0;
	long value = 0;
	struct aml_ci *ci = (struct aml_ci *)g_spi_dev->priv;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	ps = buf_orig;
	while (1) {
		/*need set '\n' to ' \n'*/
		token = strsep(&ps, "\n");
		if (token == NULL)
			break;
		if (*token == '\0')
			continue;
		parm[n++] = token;
	}

	if ((n > 0) && (strlen(parm[0]) != 2)) {
		pr_err("invalid command\n");
		kfree(buf_orig);
		return size;
	}

	if ((parm[0][0] == 'r')) {
		if (n != 2) {
			pr_err("read: invalid parameter\n");
			kfree(buf_orig);
			return size;
		}
	if (kstrtol(parm[1], 0, &value) == 0)
		addr = (int)value;
		pr_err("%s 0x%x\n", parm[0], addr);
	/*	switch ((char)parm[0][1]) {
	 *		case 'i':
	 *			retval = aml_ci_io_read_by_spi(ci, 0, addr);
	 *			break;
	 *		case 'a':
	 *			retval = aml_ci_mem_read_by_spi(ci, 0, addr);
	 *			break;
	 *		default:
	 *			break;
	 *	}
	 */
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
		/*switch ((char)parm[0][1]) {
		 *	case 'i':
retval = aml_ci_io_write_by_spi(ci, 0, addr, val);
		 *		break;
		 *	case 'a':
retval = aml_ci_mem_write_by_spi(ci, 0, addr, val);
		 *		break;
		 *	default:
		 *		break;
		 *}
		 */
		pr_dbg("%s: 0x%x <-- 0x%x\n", parm[0], addr, retval);
	} else if ((parm[0][0] == 'f')) {
		pr_dbg("full test----\r\n");
		aml_spi_ca_full_test(ci);
	} else if ((parm[0][0] == 'c')) {
		pr_dbg("cis test----\r\n");
		aml_ci_full_test_by_spi(ci, 0, addr);
	} else if ((parm[0][0] == 'p')) {
		pr_dbg("cis dvb_ca_en50221_parse_attributes----\r\n");
		dvb_ca_en50221_parse_attributes();
	}

	kfree(buf_orig);
	return size;
}

static struct class_attribute aml_spi_class_attrs[] = {
	__ATTR(reset,  0644,
		aml_spi_ci_reset_help, aml_spi_ci_reset),
	__ATTR(pwr,  0644,
		aml_spi_ci_pwr_help, aml_spi_ci_pwr),
	__ATTR(irq,  0644,
		aml_spi_ci_irq_show, NULL),
	__ATTR(status,  0644,
		aml_spi_ci_state_show, NULL),
	__ATTR(iotest,  0644,
		aml_spi_io_test_help, aml_spi_io_test),
	__ATTR_NULL
};

static struct class aml_spi_class = {
	.name = "aml_dvb_spi_test",
	.class_attrs = aml_spi_class_attrs,
};

static int __init aml_spi_mod_init(void)
{
	pr_dbg("Amlogic DVB SPI Init\n");
	class_register(&aml_spi_class);
return 0;
}

static void __exit aml_spi_mod_exit(void)
{
	pr_dbg("Amlogic DVB SPI Exit\n");
	class_unregister(&aml_spi_class);
}

module_init(aml_spi_mod_init);
module_exit(aml_spi_mod_exit);

MODULE_LICENSE("GPL");
