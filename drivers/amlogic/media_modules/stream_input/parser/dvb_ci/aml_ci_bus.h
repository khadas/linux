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
#ifndef __AML_CI_BUS_H_
#define __AML_CI_BUS_H_

#include <linux/amlogic/iomap.h>
#include <linux/amlogic/aml_gpio_consumer.h>
#include <linux/gpio/consumer.h>
#include "aml_pcmcia.h"
#include "aml_ci.h"
#include "dvb_ca_en50221.h"

/*
aml spi dev
*/
struct aml_ci_bus {
	struct mutex mutex;
	struct class	cls;
	/* add gpio pin */
	struct gpio_desc *cd_pin1;
	int cd_pin1_value;
	struct gpio_desc *cd_pin2;
	int cd_pin2_value;
	struct gpio_desc *pwr_pin;
	int pwr_pin_value;
	struct gpio_desc *le_pin;
	int le_pin_value;
	int le_enable_level;
	int addr_ts_mode_multiplex;
	/* cam irq */
	int irq;
	int irq_cmp;
	int irq_timeout;
	struct aml_pcmcia pc;
	void *priv;
	/*device type*/
	int io_device_type;
	/*select gpio group*/
	int select;
	struct pinctrl *pinctrl;
	struct pinctrl *bus_pinctrl;
	struct platform_device *pdev;
	/*save cam cur state*/
	enum aml_slot_state		slot_state;
	/*wake up thread mode at cimcu thread*/
	int wakeup_thread;
	/*for raw mode used*/
	int used;
	int raw_mode;
};

enum aml_gpio_select_e {
		AML_GPIO_ADDR = 0,
		AML_GPIO_TS,
};


#define IORD 		0x0
#define IOWR 		0x1
#define MEMRD 		0x2
#define MEMWR 		0x3

//24M cyccles, 42 ns
#define CYCLES 		              (42)
//mem rd
#define DELAY_RMEM_INIT_ADDR      (0)
#define DELAY_RMEM_ADDR_CE 	      (0)
#define DELAY_RMEM_CE_RD 	      (3)
#define DELAY_RMEM_RD_RWAIT 	  (0)
#define DELAY_RMEM_RWAIT_DATA     (0)
#define DELAY_RMEM_DATA_DRD 	  (0)
#define DELAY_RMEM_DRD_DCE 	      (0)
#define DELAY_RMEM_DCE_INIT 	  (0)
//mem wr
#define DELAY_WMEM_INIT_ADDR      (0)
#define DELAY_WMEM_ADDR_CE 	      (1)
#define DELAY_WMEM_CE_WR 	      (1)
#define DELAY_WMEM_WR_RWAIT 	  (0)
#define DELAY_WMEM_RWAIT_DATA     (0)
#define DELAY_WMEM_DATA_DWR 	  (0)
#define DELAY_WMEM_DWR_DCE 	      (1)
#define DELAY_WMEM_DCE_INIT 	  (1)
//iord
#define DELAY_RIO_INIT_ADDR       (0)
#define DELAY_RIO_ADDR_CE 	      (0)
#define DELAY_RIO_CE_RD 	      (2)
#define DELAY_RIO_RD_RWAIT 	  	  (0)
#define DELAY_RIO_RWAIT_DATA      (0)
#define DELAY_RIO_DATA_DRD 	      (0)
#define DELAY_RIO_DRD_DCE 	      (0)
#define DELAY_RIO_DCE_INIT 	      (0)
//iowr
#define DELAY_WIO_INIT_ADDR       (0)
#define DELAY_WIO_ADDR_CE 	      (1)
#define DELAY_WIO_CE_WR 	      (1)
#define DELAY_WIO_WR_RWAIT 	  	  (0)
#define DELAY_WIO_RWAIT_DATA      (0)
#define DELAY_WIO_DATA_DWR 	      (0)
#define DELAY_WIO_DWR_DCE 	      (1)
#define DELAY_WIO_DCE_INIT 	      (1)

//timeout irq hold time,10MS
#define TIMEOUT_IRQ_HOLD_TIME 	  (200)

//reg define
#define CIPLUS_CMD_REG 0x0
/*
31:24  rw, wdata:the data write in cam
23     reserved
22:8   addr A0-A14
7:6    reserved
5:4    cmd type:00: iord,01:iowr 10: oe 11:we
3:1    reserved
0      cmd valid,1:cmd is valid.need be executed.
       0: cmd is invalid
*/
#define CI_CMD_WDATA 24
#define CI_CMD_ADDR  8
#define CI_CMD_TYPE  4
#define CI_CMD_VALID 0


#define CIPLUS_RDATA_REG 0x4
/*
31:8   reserved
7:0    data read from cam
*/
#define CIPLUS_CTRL_REG 0x8
/*
31:21   reserved
10      ctrl clk 1:disable clk gated;
				0:enable clk gated
9       completion irq en,1: disable;0:able
8       inv_cam_iowrn
7       inv_cam_iordn
6       inv_cam_wen
5       inv_cam_oen
4       inv_cam_cen
3       clear transfer complete irq;1:clear
2       clear timeout ieq, 1:clear
1       cam reset: =1 assert =0 deassert
0       control enable: 1:enable ciplus ctrl,0:disable
*/
#define ENABLE_CMP_IRQ	9
#define INV_CAM_IOWR	8
#define INV_CAM_IORD	7
#define INV_CAM_WE	    6
#define INV_CAM_OE	    5
#define INV_CAM_CE	    4
#define CLEAR_CMP_IRQ	3
#define CLEAR_TIMEOUT_IRQ 2
#define CAM_RESET	    1
#define CI_ENABLE       0



#define CIPLUS_DELAY_CTRL0 0xc
/*
31:24   0xf,delay cycles between assert IORD/IOWR/MEMRD/MEMWR
		and WAIT# RELEASED
23:16   0xf,delay cycles between assert CE and  assert IORD/IOWR/MEMRD/MEMWR
15:8    0x8,delay cycles between send addr and assert CE
7:0     0x4,delay cycles between INIT and send ADDR/DATA
*/
#define CIPLUS_DELAY_CTRL1 0x10
/*
31:24   0xf,delay cycles between DEassert CE and IDLE
23:16   0xf,delay cycles between DEassert IORD/IOWR/MEMRD/MEMWR and deassert CE
15:8    0x8,delay cycles between SAMPLE DATA and DEassert IORD/IOWR/MEMRD/MEMWR
7:0     0xF,delay cycles between WAIT# RELEASE and (SAMPLEDATA OR DEASSERT IORD
		IOWR/MEMRD/MEMWR)
*/
#define CIPLUS_WAIT_TIMEOUT 0x14
/*
31     wait timeout irq enable,default 1;
30:0   data read from cam;time out threshold
       if wait time > threshold,than timeout irq.
*/
#define ENABLE_TIMEOUT_IRQ  31
#define WATT_TIMEOUT_TIME   0


#define CIPLUS_STATUS_REG 0x18
/*
31:6   reserved
5:2    fsm state
1	   timeout irq
0      complete irq
*/
#define TIMEOUT_IRQ_STATE    1
#define COMPLETE_IRQ_STATE   0


extern void dvb_ca_en50221_camready_irq(struct dvb_ca_en50221 *pubca, int slot);
extern int aml_ci_bus_init(struct platform_device *pdev, struct aml_ci *ci_dev);
extern int aml_ci_bus_exit(struct aml_ci *ci);
#endif				/* __AML_CI_BUS_H_ */
