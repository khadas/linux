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
#include <linux/mutex.h>
#include <linux/firmware.h>
#include <linux/delay.h>
#include <linux/of_gpio.h>
#include <linux/sched/clock.h>

//#include <linux/switch.h>

#include "aml_cimax.h"
#include "./usb/SRC/cimax+usb-driver.h"

#define MOD_NAME       "aml_cimax_usb"

#define pr_dbg(fmt...)\
	do {\
		if (cimax_usb_debug)\
			pr_info("cimax_usb: "fmt);\
	} while (0)
#define pr_inf(fmt...) pr_info("cimax_usb: "fmt)
#define pr_error(fmt...) pr_err("AML_CIMAX_USB: " fmt)

/*
  Uncomment below and enable permanent power in cfg
  to disable dynamic power control mechanism
*/
/*#define DISABLE_POWER_PATCH*/

#define BUFFIN_CFG                        0x0000
#define BUFFIN_ADDR_LSB                   0x0001
#define BUFFIN_ADDR_MSB                   0x0002
#define BUFFIN_DATA                       0x0003
#define BUFFOUT_CFG                       0x0004
#define BUFFOUT_ADDR_LSB                  0x0005
#define BUFFOUT_ADDR_MSB                  0x0006
#define BUFFOUT_DATA                      0x0007
#define BOOT_Key                          0x0008
#define BOOT_Status                       0x0009
#define BOOT_Test                         0x000A
#define usb2_0_irq_mask                   0x0010
#define usb2_0_status                     0x0011
#define usb2_0_rx                         0x0012
#define usb2_0_tx                         0x0013
#define SPI_Slave_Ctrl                    0x0018
#define SPI_Slave_Status                  0x0019
#define SPI_Slave_Rx                      0x001A
#define SPI_Slave_Tx                      0x001B
#define SPI_Slave_Mask                    0x001C
#define UCSG_Ctrl                         0x0020
#define UCSG_Status                       0x0021
#define UCSG_RxData                       0x0022
#define UCSG_TxData                       0x0023
#define PCtrl_Ctrl                        0x0028
#define PCtrl_Status                      0x0029
#define PCtrl_NbByte_LSB                  0x002A
#define PCtrl_NbByte_MSB                  0x002B
#define SPI_Master_Ctl                    0x0030
#define SPI_Master_NCS                    0x0031
#define SPI_Master_Status                 0x0032
#define SPI_Master_TxBuf                  0x0033
#define SPI_Master_RxBuf                  0x0034
#define BISTRAM_Ctl                       0x0038
#define BISTRAM_Bank                      0x0039
#define BISTRAM_Pat                       0x003A
#define BISTRAM_SM                        0x003B
#define BISTRAM_AddrLSB                   0x003C
#define BISTROM_Config                    0x0040
#define BISTROM_SignatureLSB              0x0041
#define BISTROM_SignatureMSB              0x0042
#define BISTROM_StartAddrLSB              0x0043
#define BISTROM_StartAddrMSB              0x0043
#define BISTROM_StopAddrLSB               0x0043
#define BISTROM_StopAddrMSB               0x0043
#define CkMan_Config                      0x0048
#define CkMan_Select                      0x0049
#define CkMan_Test                        0x004A
#define Revision_Number                   0x004B
#define ResMan_Config                     0x0050
#define ResMan_Status                     0x0051
#define ResMan_WD                         0x0052
#define ResMan_WD_MSB                     0x0053
#define CPU_Test                          0x0060
#define IrqMan_Config0                    0x0068
#define IrqMan_Config1                    0x0069
#define IrqMan_Irq0                       0x006A
#define IrqMan_NMI                        0x006B
#define IrqMan_SleepKey                   0x006C
#define Tim_Config                        0x0070
#define Tim_Value_LSB                     0x0071
#define Tim_Value_MSB                     0x0072
#define Tim_Comp_LSB                      0x0073
#define Tim_Comp_MSB                      0x0074
#define TI_Config                         0x0076
#define TI_Data                           0x0077
#define TI_Reg0                           0x0078
#define TI_Reg1                           0x0079
#define TI_Reg2                           0x007A
#define TI_Reg3                           0x007B
#define TI_Reg4                           0x007C
#define TI_ROM1                           0x007D
#define TI_ROM2                           0x007E
#define TI_ROM3                           0x007F
#define DVBCI_START_ADDR                  0x0100
#define DVBCI_END_ADDR                    0x017F
#define DATA                              0x0180
/*#define CTRL                            0x0181*/
#define QB_HOST                           0x0182
#define LEN_HOST_LSB                      0x0183
#define LEN_HOST_MSB                      0x0184
#define FIFO_TX_TH_LSB                    0x0185
#define FIFO_TX_TH_MSB                    0x0186
#define FIFO_TX_D_NB_LSB                  0x0187
#define FIFO_TX_D_NB_MSB                  0x0188
#define QB_MOD_CURR                       0x0189
#define LEN_MOD_CURR_LSB                  0x018A
#define LEN_MOD_CURR_MSB                  0x018B
#define QB_MOD                            0x018C
#define LEN_MOD_LSB                       0x018D
#define LEN_MOD_MSB                       0x018E
#define FIFO_RX_TH_LSB                    0x018F
#define FIFO_RX_TH_MSB                    0x0190
#define FIFO_RX_D_NB_LSB                  0x0191
#define FIFO_RX_D_NB_MSB                  0x0192
#define IT_STATUS_0                       0x0193
#define IT_STATUS_1                       0x0194
#define IT_MASK_0                         0x0195
#define IT_MASK_1                         0x0196
#define IT_HOST_PIN_CFG                   0x0200
#define CFG_0                             0x0201
#define CFG_1                             0x0202
#define CFG_2                             0x0203
#define IT_HOST                           0x0204
#define MOD_IT_STATUS                     0x0205
#define MOD_IT_MASK                       0x0206
#define MOD_CTRL_A                        0x0207
#define MOD_CTRL_B                        0x0208
#define DEST_SEL                          0x0209
#define CAM_MSB_ADD                       0x020A
#define GPIO0_DIR                         0x020B
#define GPIO0_DATA_IN                     0x020C
#define GPIO0_DATA_OUT                    0x020D
#define GPIO0_STATUS                      0x020E
#define GPIO0_IT_MASK                     0x020F
#define GPIO0_DFT                         0x0210
#define GPIO0_MASK_DATA                   0x0211
#define GPIO1_DIR                         0x0212
#define GPIO1_DATA_IN                     0x0213
#define GPIO1_DATA_OUT                    0x0214
#define GPIO1_STATUS                      0x0215
#define GPIO1_IT_MASK                     0x0216
#define MEM_ACC_TIME_A                    0x0217
#define MEM_ACC_TIME_B                    0x0218
#define IO_ACC_TIME_A                     0x0219
#define IO_ACC_TIME_B                     0x021A
#define EXT_CH_ACC_TIME_A                 0x021B
#define EXT_CH_ACC_TIME_B                 0x021C
#define PAR_IF_0                          0x021D
#define PAR_IF_1                          0x021E
#define PAR_IF_CTRL                       0x021F
#define PCK_LENGTH                        0x0220
#define USB2TS_CTRL                       0x0221
#define USB2TS0_RDL                       0x0222
#define USB2TS1_RDL                       0x0223
#define TS2USB_CTRL                       0x0224
#define TSOUT_PAR_CTRL                    0x0225
#define TSOUT_PAR_CLK_SEL                 0x0226
#define S2P_CH0_CTRL                      0x0227
#define S2P_CH1_CTRL                      0x0228
#define P2S_CH0_CTRL                      0x0229
#define P2S_CH1_CTRL                      0x022A
#define TS_IT_STATUS                      0x022B
#define TS_IT_MASK                        0x022C
#define IN_SEL                            0x022D
#define OUT_SEL                           0x022E
#define ROUTER_CAM_CH                     0x022F
#define ROUTER_CAM_MOD                    0x0230
#define FIFO_CTRL                         0x0231
#define FIFO1_2_STATUS                    0x0232
#define FIFO3_4_STATUS                    0x0233
#define GAP_REMOVER_CH0_CTRL              0x0234
#define GAP_REMOVER_CH1_CTRL              0x0235
#define SYNC_RTV_CTRL                     0x0236
#define SYNC_RTV_CH0_SYNC_NB              0x0237
#define SYNC_RTV_CH0_PATTERN              0x0238
#define SYNC_RTV_CH1_SYNC_NB              0x0239
#define SYNC_RTV_CH1_PATTERN              0x023A
#define SYNC_RTV_OFFSET_PATT              0x023B
#define CTRL_FILTER                       0x023D
#define PID_EN_FILTER_CH0                 0x023E
#define PID_EN_FILTER_CH1                 0x023F
#define PID_LSB_FILTER_CH0_0              0x0240
#define PID_MSB_FILTER_CH0_0              0x0241
#define PID_LSB_FILTER_CH0_1              0x0242
#define PID_MSB_FILTER_CH0_1              0x0243
#define PID_LSB_FILTER_CH0_2              0x0244
#define PID_MSB_FILTER_CH0_2              0x0245
#define PID_LSB_FILTER_CH0_3              0x0246
#define PID_MSB_FILTER_CH0_3              0x0247
#define PID_LSB_FILTER_CH0_4              0x0248
#define PID_MSB_FILTER_CH0_4              0x0249
#define PID_LSB_FILTER_CH0_5              0x024A
#define PID_MSB_FILTER_CH0_5              0x024B
#define PID_LSB_FILTER_CH0_6              0x024C
#define PID_MSB_FILTER_CH0_6              0x024D
#define PID_LSB_FILTER_CH0_7              0x024E
#define PID_MSB_FILTER_CH0_7              0x024F
#define PID_LSB_FILTER_CH1_0              0x0260
#define PID_MSB_FILTER_CH1_0              0x0261
#define PID_LSB_FILTER_CH1_1              0x0262
#define PID_MSB_FILTER_CH1_1              0x0263
#define PID_LSB_FILTER_CH1_2              0x0264
#define PID_MSB_FILTER_CH1_2              0x0265
#define PID_LSB_FILTER_CH1_3              0x0266
#define PID_MSB_FILTER_CH1_3              0x0267
#define PID_LSB_FILTER_CH1_4              0x0268
#define PID_MSB_FILTER_CH1_4              0x0269
#define PID_LSB_FILTER_CH1_5              0x026A
#define PID_MSB_FILTER_CH1_5              0x026B
#define PID_LSB_FILTER_CH1_6              0x026C
#define PID_MSB_FILTER_CH1_6              0x026D
#define PID_LSB_FILTER_CH1_7              0x026E
#define PID_MSB_FILTER_CH1_7              0x026F
#define PID_OLD_LSB_REMAPPER_0            0x0280
#define PID_OLD_MSB_REMAPPER_0            0x0281
#define PID_OLD_LSB_REMAPPER_1            0x0282
#define PID_OLD_MSB_REMAPPER_1            0x0283
#define PID_OLD_LSB_REMAPPER_2            0x0284
#define PID_OLD_MSB_REMAPPER_2            0x0285
#define PID_OLD_LSB_REMAPPER_3            0x0286
#define PID_OLD_MSB_REMAPPER_3            0x0287
#define PID_OLD_LSB_REMAPPER_4            0x0288
#define PID_OLD_MSB_REMAPPER_4            0x0289
#define PID_OLD_LSB_REMAPPER_5            0x028A
#define PID_OLD_MSB_REMAPPER_5            0x028B
#define PID_OLD_LSB_REMAPPER_6            0x028C
#define PID_OLD_MSB_REMAPPER_6            0x028D
#define PID_OLD_LSB_REMAPPER_7            0x028E
#define PID_OLD_MSB_REMAPPER_7            0x028F
#define PID_NEW_LSB_REMAPPER_0            0x02A0
#define PID_NEW_MSB_REMAPPER_0            0x02A1
#define PID_NEW_LSB_REMAPPER_1            0x02A2
#define PID_NEW_MSB_REMAPPER_1            0x02A3
#define PID_NEW_LSB_REMAPPER_2            0x02A4
#define PID_NEW_MSB_REMAPPER_2            0x02A5
#define PID_NEW_LSB_REMAPPER_3            0x02A6
#define PID_NEW_MSB_REMAPPER_3            0x02A7
#define PID_NEW_LSB_REMAPPER_4            0x02A8
#define PID_NEW_MSB_REMAPPER_4            0x02A9
#define PID_NEW_LSB_REMAPPER_5            0x02AA
#define PID_NEW_MSB_REMAPPER_5            0x02AB
#define PID_NEW_LSB_REMAPPER_6            0x02AC
#define PID_NEW_MSB_REMAPPER_6            0x02AD
#define PID_NEW_LSB_REMAPPER_7            0x02AE
#define PID_NEW_MSB_REMAPPER_7            0x02AF
#define MERGER_DIV_MICLK                  0x02C0
#define PID_AND_SYNC_REMAPPER_CTRL        0x02C1
#define PID_EN_REMAPPER                   0x02C2
#define SYNC_SYMBOL                       0x02C3
#define PID_AND_SYNC_REMAPPER_INV_CTRL    0x02C4
#define BITRATE_CH0_LSB                   0x02C5
#define BITRATE_CH0_MSB                   0x02C6
#define BITRATE_CH1_LSB                   0x02C7
#define BITRATE_CH1_MSB                   0x02C8
#define STATUS_CLK_SWITCH_0               0x02C9
#define STATUS_CLK_SWITCH_1               0x02CA
#define RESET_CLK_SWITCH_0                0x02CB
#define RESET_CLK_SWITCH_1                0x02CC
#define PAD_DRVSTR_CTRL                   0x02CD
#define PAD_PUPD_CTRL                     0x02CE
#define PRE_HEADER_ADDER_CH0_0            0x02D0
#define PRE_HEADER_ADDER_CH0_1            0x02D1
#define PRE_HEADER_ADDER_CH0_2            0x02D2
#define PRE_HEADER_ADDER_CH0_3            0x02D3
#define PRE_HEADER_ADDER_CH0_4            0x02D4
#define PRE_HEADER_ADDER_CH0_5            0x02D5
#define PRE_HEADER_ADDER_CH0_6            0x02D6
#define PRE_HEADER_ADDER_CH0_7            0x02D7
#define PRE_HEADER_ADDER_CH0_8            0x02D8
#define PRE_HEADER_ADDER_CH0_9            0x02D9
#define PRE_HEADER_ADDER_CH0_10           0x02DA
#define PRE_HEADER_ADDER_CH0_11           0x02DB
#define PRE_HEADER_ADDER_CH1_0            0x02E0
#define PRE_HEADER_ADDER_CH1_1            0x02E1
#define PRE_HEADER_ADDER_CH1_2            0x02E2
#define PRE_HEADER_ADDER_CH1_3            0x02E3
#define PRE_HEADER_ADDER_CH1_4            0x02E4
#define PRE_HEADER_ADDER_CH1_5            0x02E5
#define PRE_HEADER_ADDER_CH1_6            0x02E6
#define PRE_HEADER_ADDER_CH1_7            0x02E7
#define PRE_HEADER_ADDER_CH1_8            0x02E8
#define PRE_HEADER_ADDER_CH1_9            0x02E9
#define PRE_HEADER_ADDER_CH1_10           0x02EA
#define PRE_HEADER_ADDER_CH1_11           0x02EB
#define PRE_HEADER_ADDER_CTRL             0x02EC
#define PRE_HEADER_ADDER_LEN              0x02ED
#define PRE_HEADER_REMOVER_CTRL           0x02EE
#define FSM_DVB                           0x02F0
#define TS2USB_FSM_DEBUG                  0x02F2
#define TSOUT_PAR_FSM_DEBUG               0x02F3
#define GAP_REMOVER_FSM_DEBUG             0x02F4
#define PID_AND_SYNC_REMAPPER_FSM_DEBUG   0x02F5
#define PRE_HEADER_ADDER_FSM_DEBUG        0x02F6
#define SYNC_RTV_FSM_DEBUG                0x02F7
#define CHECK_PHY_CLK                     0x0E00
#define USB_CTRL1                         0x0E01
#define USB_ISO2_out                      0x0800
#define USB_ISO1_out                      0x1000
#define USB_Interrupt_out                 0x1E00
#define USB_Bulk_in                       0x1F00
#define CC2_Buffer_out                    0x2000
#define USB_EP0                           0x30C0
#define CC2_Buffer_in                     0x4000
#define USB_ISO2_in                       0x5800
#define USB_ISO1_in                       0x6000
#define nmb_vector_address_lsb            0xFFFA
#define nmb_vector_address_msb            0xFFFB
#define reset_vector_address_lsb          0xFFFC
#define reset_vector_address_msb          0xFFFD
#define irb_vector_address_lsb            0xFFFE
#define irb_vector_address_msb            0xFFFF


#define CIMAX_REG_HDR_SIZE 4
#define CIMAX_REG_PLD_SIZE 255
#define CIMAX_CAM_HDR_SIZE 4
#define CIMAX_CAM_PLD_SIZE 65535

#define DEF_LOCK(_l_) struct mutex _l_

struct cimax_usb {
	struct platform_device *pdev;
	struct device_s *dev;

	struct aml_cimax *cimax;

	u8 buf[CIMAX_REG_HDR_SIZE + CIMAX_CAM_HDR_SIZE + CIMAX_CAM_PLD_SIZE];
	int buf_size;

	int cam_inserted[2];
#define IN_INSERTED 0x01
#define IN_POWERED  0x02
#define IN_LINKED   0x04
	int cam_data_ready[2];

	int poll_mode;
#define STOP_MODE 0
#define POLL_MODE 1
#define INT_MODE  2

	int rst_io;

	struct workqueue_struct *workq;
	struct delayed_work work;
	int work_auto_restart;
	int work_cnt;

	struct delayed_work power_work;
	int power_work_cnt;
	int cam_det;

	DEF_LOCK(lock);
#define lock_init(_usb) mutex_init(&(_usb)->lock)
#define lock_lock(_usb) do {\
	int err = mutex_lock_interruptible(&(_usb)->lock);\
	if (err)\
		return err;\
} while (0)
#define lock_unlock(_usb) mutex_unlock(&(_usb)->lock)

	u8 *cis;
#define CIS_MAX 512
};

static struct cimax_usb *g_usb;

MODULE_PARM_DESC(usbdebug, "enable verbose debug messages");
static int cimax_usb_debug = 1;
module_param_named(usbdebug, cimax_usb_debug, int, 0644);

MODULE_PARM_DESC(usbpoll_interval, "interval for usb poll");
static int usb_poll_interval = 100;
module_param_named(usbpoll_interval, usb_poll_interval, int, 0644);

MODULE_PARM_DESC(usbpoll_mode, "set cimax poll mode, need reset");
static int cimax_poll_mode = 1;
module_param_named(usbpoll_mode, cimax_poll_mode, int, 0644);

MODULE_PARM_DESC(usbcam_irq_mode, "set cam irq mode, need reset");
static int cam_irq_mode;
module_param_named(usbcam_irq_mode, cam_irq_mode, int, 0644);


#define CIMAX_REG_READ     0xff
#define CIMAX_REG_READ_OK  0x4c
#define CIMAX_REG_WRITE    0x7f
#define CIMAX_REG_WRITE_OK 0x4d
#define CIMAX_REG_INIT     0x00
#define CIMAX_REG_INIT_OK  0x4b
#define CIMAX_REG_CMD_ERROR 0x51

#define CIMAX_CAM_RESET    0x01
#define CIMAX_CAM_RESET_OK 0x40
#define CIMAX_CAM_CIS      0x02
#define CIMAX_CAM_CIS_OK   0x41
#define CIMAX_CAM_COR      0x03
#define CIMAX_CAM_COR_OK   0x42
#define CIMAX_CAM_NEG      0x04
#define CIMAX_CAM_NEG_OK   0x43
#define CIMAX_CAM_WLPDU    0x05
#define CIMAX_CAM_WLPDU_OK 0x44
#define CIMAX_CAM_RLPDU    0x06
#define CIMAX_CAM_RLPDU_OK 0x46
#define CIMAX_CAM_EVT         0x0d
#define CIMAX_CAM_DET_OK      0x45
#define CIMAX_CAM_NOCAM       0x49
#define CIMAX_CAM_ERROR       0x4a
#define CIMAX_CAM_NOEVT       0x55
#define CIMAX_CAM_DATA_READY  0x4e
#define CIMAX_CAM_WBUSY       0x54
#define CIMAX_CAM_PENDING     0x56
#define CIMAX_CAM_REGSTAT     0x0e
#define CIMAX_CAM_REGSTAT_OK  0x57


#define CIMAX_CAM_PKT_CNT_VAL 1

#define CIMAX_SLOT_A 0
#define CIMAX_SLOT_B 1

#define CIMAX_CMD_RESP_MASK 0x7f

#define cimax_to_usb(_c) ((struct cimax_usb *)((_c)->priv))
#define dev_to_usb(_d) ((struct cimax_usb *)usb_get_drvdata(_d))

#define byte_to_u16(_b1, _b2)   (((_b1)<<8) | (_b2))

#define hdr_cmd_resp(_s)     ((_s)->buf[0] & CIMAX_CMD_RESP_MASK)

#define reg_hdr(_s)          ((_s)->buf)
#define reg_addr(_s)         byte_to_u16((_s)->buf[1], (_s)->buf[2])
#define reg_hdr_dat_size(_s) ((_s)->buf[3])
#define reg_dat(_s)          (&((_s)->buf[CIMAX_REG_HDR_SIZE]))

#define cam_hdr(_s)          ((_s)->buf)
#define cam_hdr_slot(_s)     (((_s)->buf[0] & 0x80) ? 1 : 0)
#define cam_hdr_pkt_cnt(_s)  ((_s)->buf[1])
#define cam_hdr_dat_size(_s) byte_to_u16((_s)->buf[2], (_s)->buf[3])
#define cam_dat(_s)          (&((_s)->buf[CIMAX_CAM_HDR_SIZE]))

#define REG_TIMEOUT 500
#define CAM_TIMEOUT 5000

static int aml_cimax_usb_mod_init(struct platform_device *pdev);
static void aml_cimax_usb_mod_exit(struct platform_device *pdev);

static int cimax_usb_set_loop(struct cimax_usb *usb, int loop);

static void dump(char *title, u8 *buf, int size)
{
	int i;
	pr_info("%s\n", title);
	for (i = 0; i < size; i++) {
		if (!(i & 0xf))
			pr_info("\n\t");
		pr_info("%02x ", *(buf+i));
	}
	pr_info("\n");
}

static void perr(char *err, struct cimax_usb *usb)
{
	pr_error("error: %s\n", err);
	dump("dump:", usb->buf, 16);
}

static inline unsigned long get_jiffies(void)
{
	return (unsigned long)(sched_clock()/10000000);
}

static int cam_usb_cam_detect(struct cimax_usb *usb, int slot, int flag)
{
	usb->cam_inserted[slot] = flag;
	pr_inf("detect slot(%d): 0x%x(%s)\n",
		slot, usb->cam_inserted[slot],
		(!flag) ? "none" :
		(flag & IN_LINKED) ? "linked" :
		(flag & IN_POWERED) ? "powered" :
		(flag & IN_INSERTED) ? "inserted" :
		"unknown");
	aml_cimax_slot_state_changed(usb->cimax, slot,
		usb->cam_inserted[slot]);
	return 0;
}

static inline void set_usb_cam_ready(struct cimax_usb *usb, int slot)
{
	if (usb->cam_inserted[slot] & IN_POWERED) {
		cam_usb_cam_detect(usb, slot,
			usb->cam_inserted[slot] | IN_LINKED);
		cimax_usb_set_loop(usb, 1);/*set auto-loop*/
	}
}

static int init_reg_hdr(u8 *hdr, u8 tag, int addr, int size)
{
	hdr[0] = tag;
	hdr[1] = (addr>>8) & 0xff;
	hdr[2] = addr & 0xff;
	hdr[3] = size;
	return 0;
}

static int check_reg_hdr(u8 *hdr, u8 tag, int addr, int size)
{
	return hdr[0] != tag
		|| hdr[1] != ((addr>>8) & 0xff)
		|| hdr[2] != (addr & 0xff)
		|| hdr[3] != size;
}

static int aml_cimax_usb_read_reg(struct aml_cimax *cimax, int addr,
		u8 *buf, int size)
{
	struct cimax_usb *usb = cimax_to_usb(cimax);
	struct device_s *dev = usb->dev;
	u8 out[CIMAX_REG_HDR_SIZE];
	int err = 0;

	init_reg_hdr(out, CIMAX_REG_READ, addr, size);

	lock_lock(usb);

	/*pr_dbg("rd %02x:%02x:%02x:%02x\n",
		out[0], out[1],
		out[2], out[3]);*/
	err = cimax_usb_ci_write(dev,
			out, CIMAX_REG_HDR_SIZE, usb->buf, sizeof(usb->buf));
	if (err)
		goto end;
	if (check_reg_hdr(reg_hdr(usb), CIMAX_REG_READ_OK, addr, size) != 0) {
		pr_dbg("rd %02x:%02x:%02x:%02x\n",
			out[0], out[1],
			out[2], out[3]);
		perr("read reg fail.", usb);
		err = -EINVAL;
		goto end;
	}
	memcpy(buf, reg_dat(usb), size);
end:
	lock_unlock(usb);
	return err;
}

static int aml_cimax_usb_write_reg(struct aml_cimax *cimax, int addr,
		u8 *buf, int size)
{
	struct cimax_usb *usb = cimax_to_usb(cimax);
	struct device_s *dev = usb->dev;
	u8 out[CIMAX_REG_HDR_SIZE + CIMAX_REG_PLD_SIZE];
	int err = 0;

	init_reg_hdr(out, CIMAX_REG_WRITE, addr, size);
	memcpy(&out[CIMAX_REG_HDR_SIZE], buf, size);

	lock_lock(usb);

	pr_dbg("wr %02x:%02x:%02x:%02x\n",
		out[0], out[1],
		out[2], out[3]);
	err = cimax_usb_ci_write(dev,
		out, CIMAX_REG_HDR_SIZE + size, usb->buf, sizeof(usb->buf));
	if (err)
		goto end;
	if (check_reg_hdr(reg_hdr(usb), CIMAX_REG_WRITE_OK, addr, 0) != 0) {
		perr("write reg fail.", usb);
		err = -EINVAL;
		goto end;
	}
end:
	lock_unlock(usb);
	return err;
}

static inline int init_cam_hdr(u8 *hdr, int cmd, int size)
{
	hdr[0] = cmd;
	hdr[1] = CIMAX_CAM_PKT_CNT_VAL;
	hdr[2] = (size>>8) & 0xff;
	hdr[3] = size & 0xff;
	return 0;
}

static inline int cam_err(struct cimax_usb *usb)
{
	if (hdr_cmd_resp(usb) != CIMAX_CAM_ERROR
		|| cam_hdr_pkt_cnt(usb) != CIMAX_CAM_PKT_CNT_VAL
		|| cam_hdr_dat_size(usb) != 2)
		return 0;
	return byte_to_u16(cam_dat(usb)[0], cam_dat(usb)[1]);
}

static inline char *cam_err_str(int err)
{
#define CAMERROR_RESET           0x0101
#define CAMERROR_CIS_BUF         0x0201
#define CAMERROR_CIS_SIZE        0x0202
#define CAMERROR_CAM_NOT_ACT     0x0203
#define CAMERROR_COR_NOT_READY   0x0301
#define CAMERROR_COR_VAL_CHK     0x0302
#define CAMERROR_NEG_NO_RESP     0x0401
#define CAMERROR_NEG_BAD_SIZE    0x0402
#define CAMERROR_NEG_NOT_READY   0x0403
#define CAMERROR_LPDU_NOT_AVAIL  0x0601
	struct { int err; char *str; } cam_err_strings[] = {
		{CAMERROR_RESET, "reset error, not ready."},
		{CAMERROR_CIS_BUF, "cis error, buffer not allocated."},
		{CAMERROR_CIS_SIZE, "cis error, bad cis size."},
		{CAMERROR_CAM_NOT_ACT, "cam not activated."},
		{CAMERROR_COR_NOT_READY, "cam not ready during write COR."},
		{CAMERROR_COR_VAL_CHK, "COR value check failed."},
		{CAMERROR_NEG_NO_RESP, "cam not responding when negotiation."},
		{CAMERROR_NEG_BAD_SIZE, "cam buf size length != 2."},
		{CAMERROR_NEG_NOT_READY, "cam not ready during negotiation."},
		{CAMERROR_LPDU_NOT_AVAIL, "lpdu not available."}
	};
	int i;
	for (i = 0;
		i < sizeof(cam_err_strings)/sizeof(cam_err_strings[0]); i++) {
		if (cam_err_strings[i].err == err)
			return cam_err_strings[i].str;
	}
	return "err unknown.";
}

static int cimax_usb_access_cam(struct cimax_usb *usb, int slot,
		int cmd, u8 *tx, int tx_size, u8 *rx, int rx_size)
{
	struct device_s *dev = usb->dev;
	u8 *out = NULL;
	int err = 0;

	lock_lock(usb);
	out = kzalloc(CIMAX_CAM_HDR_SIZE + CIMAX_CAM_PLD_SIZE, GFP_KERNEL);
	if (!out) {
		pr_err("no mem for access cam.\n");
		lock_unlock(usb);
		return -ENOMEM;
	}

	cmd |= slot ? 0x80 : 0;
	init_cam_hdr(out, cmd, tx_size);
	memcpy(&out[CIMAX_CAM_HDR_SIZE], tx, tx_size);
	/*dump("access cam:", out, CIMAX_CAM_HDR_SIZE+size);*/

	err = cimax_usb_ci_write(dev,
			out, CIMAX_CAM_HDR_SIZE + tx_size, rx, rx_size);
	if (err)
		goto end;
	if (cam_hdr_slot(usb) != slot) {
		pr_error("expect slot(%d), but slot(%d)\n",
			slot, cam_hdr_slot(usb));
		err = -EINVAL;
		goto end;
	}
	switch (hdr_cmd_resp(usb)) {
	case CIMAX_CAM_NOCAM:
		pr_dbg("no cam\n");
		err = -ENODEV;
		break;
	case CIMAX_CAM_ERROR:
		pr_error("cam error\n");
		pr_error("err code: 0x%04x(%s)\n", cam_err(usb),
			cam_err_str(cam_err(usb)));
		err = -ENODEV;
		break;
	case CIMAX_CAM_WBUSY:
		pr_dbg("cam busy\n");
		err = -EBUSY;
		break;
	case CIMAX_CAM_PENDING:
		pr_dbg("cam pending\n");
		err = -EAGAIN;
		break;
	}
end:
	kfree(out);
	lock_unlock(usb);
	return err;
}

static int aml_cimax_usb_read_cis(struct aml_cimax *cimax, int slot,
		u8 *buf, int size)
{
	struct cimax_usb *usb = cimax_to_usb(cimax);
	int err = 0;
	int len;

	err = cimax_usb_access_cam(usb, slot, CIMAX_CAM_CIS,
		NULL, 0, usb->buf, sizeof(usb->buf));
	if (err)
		goto end;
	if (hdr_cmd_resp(usb) != CIMAX_CAM_CIS_OK
		|| cam_hdr_pkt_cnt(usb) != CIMAX_CAM_PKT_CNT_VAL) {
		perr("read cis fail.", usb);
		err = -EINVAL;
		goto end;
	}
	len = cam_hdr_dat_size(usb);
	if (size < len) {
		pr_error("cis size too large, expect<%d, but:%d\n", size, len);
		perr("cis fail.", usb);
		err = -EINVAL;
		goto end;
	}
	memcpy(buf, cam_dat(usb), len);

	if (!usb->cis)
		usb->cis = kzalloc((len < 512) ? 512 : len, GFP_KERNEL);
	if (usb->cis)
		memcpy(usb->cis, cam_dat(usb), len);

end:
	return err;
}
#define CIMAX_CAM_COR_PLD_SIZE 5
static int aml_cimax_usb_write_cor(struct aml_cimax *cimax, int slot,
		int addr, u8 *buf)
{
	struct cimax_usb *usb = cimax_to_usb(cimax);
	int err = 0;
	u8 out[CIMAX_CAM_COR_PLD_SIZE + 8];
	int sz = CIMAX_CAM_COR_PLD_SIZE;

	out[0] = addr>>8 & 0xff;
	out[1] = addr & 0xff;
	out[2] = buf[0];
	out[3] = 0;
	out[4] = 0;

	if (!cam_irq_mode) {
		out[5] = 0x40;/*cam poll mode*/
		sz++;
	}

	err = cimax_usb_access_cam(usb, slot, CIMAX_CAM_COR,
			out, sz, usb->buf, sizeof(usb->buf));
	if (err)
		goto end;
	if (hdr_cmd_resp(usb) != CIMAX_CAM_COR_OK
		|| cam_hdr_pkt_cnt(usb) != CIMAX_CAM_PKT_CNT_VAL
		|| cam_hdr_dat_size(usb) != 0) {
		perr("write cor fail.", usb);
		err = -EINVAL;
		goto end;
	}
end:
	return err;
}
#define CIMAX_CAM_NEG_PLD_SIZE 2
static int aml_cimax_usb_negotiate(struct aml_cimax *cimax, int slot, int size)
{
	struct cimax_usb *usb = cimax_to_usb(cimax);
	int ret = 0;
	u8 out[CIMAX_CAM_NEG_PLD_SIZE];

	out[0] = (size>>8) & 0xff;
	out[1] = size & 0xff;

	ret = cimax_usb_access_cam(usb, slot, CIMAX_CAM_NEG,
			out, CIMAX_CAM_NEG_PLD_SIZE,
			usb->buf, sizeof(usb->buf));
	if (ret)
		goto end;
	if (hdr_cmd_resp(usb) != CIMAX_CAM_NEG_OK
		|| cam_hdr_pkt_cnt(usb) != CIMAX_CAM_PKT_CNT_VAL
		|| cam_hdr_dat_size(usb) != 2) {
		perr("negotiate fail.", usb);
		ret = -EINVAL;
		goto end;
	}
	ret = byte_to_u16(cam_dat(usb)[0], cam_dat(usb)[1]);

	set_usb_cam_ready(usb, slot);
end:
	return ret;
}

static int aml_cimax_usb_write_lpdu(struct aml_cimax *cimax, int slot,
		u8 *buf, int size)
{
	struct cimax_usb *usb = cimax_to_usb(cimax);
	int ret = 0;

	/*dump("lpdu ->", buf, size);*/
	ret = cimax_usb_access_cam(usb, slot, CIMAX_CAM_WLPDU,
		buf, size, usb->buf, sizeof(usb->buf));
	if (ret)
		goto end;
	if (hdr_cmd_resp(usb) != CIMAX_CAM_WLPDU_OK
		|| cam_hdr_pkt_cnt(usb) != CIMAX_CAM_PKT_CNT_VAL
		|| cam_hdr_dat_size(usb) != 0) {
		perr("write lpdu fail.", usb);
		ret = -EINVAL;
		goto end;
	}
	ret = size;
end:
	return ret;
}

static int aml_cimax_usb_read_lpdu(struct aml_cimax *cimax, int slot,
		u8 *buf, int size)
{
	struct cimax_usb *usb = cimax_to_usb(cimax);
	int ret = 0;

	ret = cimax_usb_access_cam(usb, slot, CIMAX_CAM_RLPDU,
		NULL, 0, usb->buf, sizeof(usb->buf));
	if (ret)
		goto end;
	if (hdr_cmd_resp(usb) != CIMAX_CAM_RLPDU_OK
		|| cam_hdr_pkt_cnt(usb) != CIMAX_CAM_PKT_CNT_VAL) {
		perr("read lpdu fail.", usb);
		ret = -EINVAL;
		goto end;
	}
	ret = cam_hdr_dat_size(usb);
	memcpy(buf, cam_dat(usb), ret);

	/*dump("lpdu <-", buf, ret);*/

	usb->cam_data_ready[slot] = 0;
end:
	return ret;
}

static int aml_cimax_usb_read_cam_status(struct aml_cimax *cimax, int slot)
{
	struct cimax_usb *usb = cimax_to_usb(cimax);
	int ret = 0;

	if (cam_irq_mode && usb->cam_data_ready[slot])
		return 0x80;

	ret = cimax_usb_access_cam(usb, slot, CIMAX_CAM_REGSTAT,
		NULL, 0, usb->buf, sizeof(usb->buf));
	if (ret)
		goto end;
	if (hdr_cmd_resp(usb) != CIMAX_CAM_REGSTAT_OK
		|| cam_hdr_pkt_cnt(usb) != CIMAX_CAM_PKT_CNT_VAL
		|| cam_hdr_dat_size(usb) != 1) {
		perr("read cam status fail.", usb);
		ret = -EINVAL;
		goto end;
	}

	ret = cam_dat(usb)[0];
end:
	return ret;
}

static int aml_cimax_usb_slot_reset(struct aml_cimax *cimax, int slot)
{
	struct cimax_usb *usb = cimax_to_usb(cimax);
	int ret = 0;

	usb->cam_data_ready[slot] = 0;

	ret = cimax_usb_access_cam(usb, slot, CIMAX_CAM_RESET,
		NULL, 0, usb->buf, sizeof(usb->buf));
	if (ret)
		goto end;
	if (hdr_cmd_resp(usb) != CIMAX_CAM_RESET_OK
		|| cam_hdr_pkt_cnt(usb) != CIMAX_CAM_PKT_CNT_VAL
		|| cam_hdr_dat_size(usb) != 0) {
		perr("slot reset fail.", usb);
		ret = -EINVAL;
		goto end;
	}
end:
	return ret;
}

static int aml_cimax_usb_cam_reset(struct aml_cimax *cimax, int slot)
{
	pr_dbg("Slot(%d): camreset\n", slot);
	return 0;
}

static int aml_cimax_usb_slot_shutdown(struct aml_cimax *cimax, int slot)
{
	pr_dbg("Slot(%d): shutdown\n", slot);
	return 0;
}
static int aml_cimax_usb_slot_ts_enable(struct aml_cimax *cimax, int slot)
{
	pr_dbg("Slot(%d): ts control\n", slot);
	return 0;
}
static int aml_cimax_usb_slot_status(struct aml_cimax *cimax, int slot)
{
	struct cimax_usb *usb = cimax_to_usb(cimax);
	if (usb->cam_inserted[slot] & IN_POWERED) {
		/*pr_dbg("CA Module present and ready\n");*/
		return DVB_CA_EN50221_POLL_CAM_PRESENT |
			DVB_CA_EN50221_POLL_CAM_READY;
	} else {
		/*pr_error("CA Module not present or not ready\n");*/
	}
	return 0;
}

static int cimax_usb_cam_plugin(struct cimax_usb *usb, int slot, int plugin)
{
	pr_dbg("cam plug: slot(%d) %s\n",
		slot, plugin ? "plugged" : "unplugged");
	return aml_cimax_camchanged(usb->cimax, slot, plugin);
}

static int cimax_usb_set_power(struct cimax_usb *usb, int on)
{
	u8 reg = 0;
	int err = 0;
	if (!on) {
		reg = 0;
		err = aml_cimax_usb_read_reg(usb->cimax, MOD_IT_MASK, &reg, 1);
		if (err)
			return err;
		reg |= 0x03;
		reg &= 0xf3;

		err = aml_cimax_usb_write_reg(usb->cimax, MOD_IT_MASK, &reg, 1);
		if (err)
			return err;
	}
	reg = on ? 0x3 : 0x0;
	return aml_cimax_usb_write_reg(usb->cimax, GPIO0_DATA_OUT, &reg, 1);
}

static int cimax_usb_check_poe(struct cimax_usb *usb, int *on)
{
	u8 reg = 0;
	int err = 0;

	*on = 0;

	err = aml_cimax_usb_read_reg(usb->cimax, CFG_1, &reg, 1);
	if (err)
		return err;
	if (reg & 0x20) {/*if VCCEN*/
		reg |= 0x08;/*set POE*/
		err = aml_cimax_usb_write_reg(usb->cimax, CFG_1, &reg, 1);
		if (err)
			return err;
		err = aml_cimax_usb_read_reg(usb->cimax, CFG_1, &reg, 1);
		if (err)
			return err;
		if (reg & 0x08)/*if POE ok*/
			*on = 1;
	}
	return err;
}

static void cimax_usb_power_work(struct work_struct *work)
{
	struct cimax_usb *usb = container_of(to_delayed_work(work),
		struct cimax_usb, power_work);
	int power = 0;
	int err = 0;

	usb->power_work_cnt++;
	err = cimax_usb_set_power(usb, 1);
	if (err)
		return;

	err = cimax_usb_check_poe(usb, &power);
	if (err)
		return;

	if (power) {
		return;
	}

	schedule_delayed_work(&usb->power_work, usb_poll_interval);
}

static int cimax_usb_cam_powerctrl(struct cimax_usb *usb,
		int slot, int power)
{
	if (slot != 0)
		return 0;

#ifdef DISABLE_POWER_PATCH
	if (power) {
		cam_usb_cam_detect(usb, slot,
			usb->cam_inserted[slot] | IN_POWERED);
		cimax_usb_cam_plugin(usb, slot, 1);
	}
	return 0;
#else
	pr_inf("cancel power ctrl previous\n");
	cancel_delayed_work_sync(&usb->power_work);

	if (!power) {
		int err = 0;
		err = cimax_usb_set_power(usb, 0);
		pr_inf("slot[%d] power off\n", slot);
		return 0;
	}

	INIT_DELAYED_WORK(&usb->power_work, &cimax_usb_power_work);
	schedule_delayed_work(&usb->power_work, usb_poll_interval);
	pr_inf("slot[%d] power ctrl started\n", slot);
#endif
	return 0;
}

static int cimax_usb_poll(struct cimax_usb *usb)
{
	struct device_s *dev = usb->dev;
	int power = 0;
	int err = 0;
	int slot = 0;

	if (!usb->cam_det) {
		for (slot = 0; slot < 2; slot++) {
			int addr = (!slot) ? MOD_CTRL_A : MOD_CTRL_B;
			u8 reg = 0;
			err = aml_cimax_usb_read_reg(usb->cimax,
					addr, &reg, 1);
			if (reg & 1) {
				cam_usb_cam_detect(usb, slot,
					(reg & 1) ? IN_INSERTED : 0);
				msleep(200);
				err = cimax_usb_set_power(usb, (reg & 1));
				err = cimax_usb_check_poe(usb, &power);
				pr_inf("slot[%d] power on\n", slot);
				msleep(200);
				if (power) {
					cam_usb_cam_detect(usb, slot,
					usb->cam_inserted[slot] | IN_POWERED);
					cimax_usb_cam_plugin(usb, slot, 1);
					usb->cam_det = 1;
				}
			}
		}
		return 0;
	}
	err = cimax_usb_ci_read_evt(dev, CIMAX_SLOT_A,
			usb->buf, sizeof(usb->buf));
	if (err)
		goto end;

	switch (hdr_cmd_resp(usb)) {
	case CIMAX_CAM_DET_OK: {
		int slot = cam_hdr_slot(usb);
		int insert = cam_dat(usb)[0];
		if ((!!usb->cam_inserted[slot]) != insert) {
			cam_usb_cam_detect(usb, slot,
				insert ? IN_INSERTED : 0);
			cimax_usb_cam_powerctrl(usb, slot, insert);
		}
		if (!insert)
			usb->cam_det = 0;
		} break;
	case CIMAX_CAM_DATA_READY: {
		int slot = cam_hdr_slot(usb);
		usb->cam_data_ready[slot] = 1;
		} break;
	case CIMAX_CAM_NOEVT:
		break;
	default:
		pr_error("unknown resp:%02x\n", hdr_cmd_resp(usb));
		break;
	}
end:
	return 0;
}

static void cimax_usb_poll_work(struct work_struct *work)
{
	struct cimax_usb *usb =
		container_of(to_delayed_work(work), struct cimax_usb, work);
	usb->work_cnt++;
	cimax_usb_poll(usb);
	if (usb->work_auto_restart)
		queue_delayed_work(usb->workq, &usb->work, usb_poll_interval);
}

#define CTRL_DISABLE -1
#define CTRL_STOP     0
#define CTRL_START    1

static inline int cimax_usb_poll_ctrl(struct cimax_usb *usb, int ctrl)
{
	if (ctrl == CTRL_START) {
		if (usb->workq)
			return 0;
		usb->work_auto_restart = 1;
		usb->workq = create_singlethread_workqueue("cimax_usb");
		INIT_DELAYED_WORK(&usb->work, &cimax_usb_poll_work);
		queue_delayed_work(usb->workq,
			&usb->work, usb_poll_interval);
		pr_inf("poll started\n");
	} else {
		if (!usb->workq)
			return 0;
		usb->work_auto_restart = 0;
		cancel_delayed_work_sync(&usb->work);
		destroy_workqueue(usb->workq);
		usb->workq = NULL;
		pr_inf("poll stopped\n");
	}
	return 0;
}

static int cimax_usb_setup_poll(struct cimax_usb *usb, int poll_mode)
{
	if (poll_mode == usb->poll_mode)
		return 0;
	switch (poll_mode) {
	case POLL_MODE:
		cimax_usb_poll_ctrl(usb, CTRL_START);
		usb->poll_mode = POLL_MODE;
		break;
	case STOP_MODE:
		if (usb->poll_mode == POLL_MODE)
			cimax_usb_poll_ctrl(usb, CTRL_DISABLE);
		usb->poll_mode = STOP_MODE;
		break;
	default:
		break;
	}
	return 0;
}

static int cimax_usb_hw_reset(struct cimax_usb *usb, int reset_val)
{
	/*trigger reset io*/
	if (usb->rst_io) {
		gpio_direction_output(usb->rst_io, reset_val ? 1 : 0);
		msleep(50);
		gpio_direction_output(usb->rst_io, reset_val ? 0 : 1);
	}
	return 0;
}

static int cimax_usb_set_loop(struct cimax_usb *usb, int loop)
{
	int a = usb->cam_inserted[0];
	int b = usb->cam_inserted[1];
	u8 cm[2];

	pr_inf("set loop: %d\n", loop);

	cm[0] = loop ? (b ? 0x85 : 0x80) : 0x81;/*CH*/
	cm[1] = loop ? (a ? 0x51 : 0x11) : 0x00;/*MOD*/

	return aml_cimax_usb_write_reg(usb->cimax, ROUTER_CAM_CH, cm, 2);
}

int cimax_usb_dev_add(struct device_s *dev, int id)
{
	pr_inf("dev add\n");
	if (!g_usb)
		return 0;

	(void)id;

	cimax_usb_device_open(dev);
	cimax_usb_select_interface(dev, 3);

	lock_lock(g_usb);
	g_usb->dev = dev;
	lock_unlock(g_usb);

	if (0)
	{
		/*
		  the cimax's fw do not report cam status
		  when board power on with cam plugged,
		  have to check manually here.
		*/
		int slot = 0;
		int err = 0;
		for (slot = 0; slot < 2; slot++) {
			int addr = (!slot) ? MOD_CTRL_A : MOD_CTRL_B;
			u8 reg = 0;
			err = aml_cimax_usb_read_reg(g_usb->cimax,
					addr, &reg, 1);
			cam_usb_cam_detect(g_usb, slot,
				(reg & 1) ? IN_INSERTED : 0);
			cimax_usb_cam_powerctrl(g_usb, slot, (reg & 1));
		}
	}
	cimax_usb_set_power(g_usb, 0);
	cimax_usb_setup_poll(g_usb, cimax_poll_mode ? POLL_MODE : INT_MODE);
	return 0;
}
EXPORT_SYMBOL(cimax_usb_dev_add);

int cimax_usb_dev_remove(struct device_s *dev, int id)
{
	pr_dbg("dev remove\n");
	if (!g_usb)
		return 0;
	(void)id;
	pr_dbg("setup poll -> stop\n");
	cimax_usb_setup_poll(g_usb, STOP_MODE);
	pr_dbg("setup poll end\n");
	lock_lock(g_usb);
	g_usb->dev = NULL;
	lock_unlock(g_usb);
	return 0;
}
EXPORT_SYMBOL(cimax_usb_dev_remove);

static int cimax_usb_get_config_from_dts(struct cimax_usb *usb)
{
	struct device_node *child = NULL;
	struct platform_device *pdev = usb->pdev;
	struct device_node *np = pdev->dev.of_node;
	pr_dbg("fetch cimax usb in dts\n");

	child = of_get_child_by_name(np, "cimax");
	if (child == NULL) {
		pr_error("cimax not found in dts\n");
		return -1;
	}
	child = of_get_child_by_name(child, "usb");
	if (!child) {
		pr_error("usb not found in cimax");
		return -1;
	}
/*
dvbci {
	compatible = "amlogic, dvbci";
	dev_name = "dvbci";
	io_type = <2>;//0:iobus,1:usb,2:cimax
	cimax {
		io_type = <1> //0:spi 1:usb
		usb {
			rst_gpio = <&gpio_ao GPIOAO_2 GPIO_ACTIVE_HIGH>;
		};
	};

};
*/
	{
		int ret = 0;
		int gpio = -1;
		gpio = of_get_named_gpio_flags(child, "rst-gpios", 0, NULL);
		if (gpio != -1) {
			ret = gpio_request(gpio, "cimax");
			if (ret < 0) {
				pr_error("rst-gpios request fail.\n");
				return ret;
			}
			usb->rst_io = gpio;
			cimax_usb_hw_reset(usb, 1);
			pr_dbg("rst: %d\n", usb->rst_io);
		} else {
			pr_error("rst io got fail, %d\n", gpio);
		}
	}
	return 0;
}

int aml_cimax_usb_init(struct platform_device *pdev, struct aml_cimax *cimax)
{
	struct cimax_usb *cimax_usb;

	cimax_usb = kzalloc(sizeof(struct cimax_usb), GFP_KERNEL);
	if (!cimax_usb)
		return -ENOMEM;

	cimax_usb->pdev = pdev;
	cimax_usb->cimax = cimax;
	cimax_usb_get_config_from_dts(cimax_usb);

	/*init usb_lock*/
	lock_init(cimax_usb);

	/*init cimax used api.*/
#define WI(_f)\
	cimax->ops._f = aml_cimax_usb_##_f
	WI(read_cis);
	WI(write_cor);
	WI(negotiate);
	WI(read_lpdu);
	WI(write_lpdu);
	WI(read_cam_status);
	WI(cam_reset);
	WI(slot_reset);
	WI(slot_shutdown);
	WI(slot_ts_enable);
	WI(slot_status);
	WI(read_reg);
	WI(write_reg);

	cimax->priv = cimax_usb;

	g_usb = cimax_usb;

	aml_cimax_usb_mod_init(pdev);

	cimax_usb_set_cb(cimax_usb_dev_add, cimax_usb_dev_remove);

	return 0;
}
EXPORT_SYMBOL(aml_cimax_usb_init);

int aml_cimax_usb_exit(struct aml_cimax *cimax)
{
	struct cimax_usb *usb = cimax_to_usb(cimax);

	if (!usb)
		return -ENODEV;

	aml_cimax_usb_mod_exit(usb->pdev);

	cimax_usb_device_close(usb->dev);
	cimax_usb_setup_poll(usb, STOP_MODE);

	if (usb->rst_io)
		gpio_free(usb->rst_io);

	kfree(usb->cis);

	kfree(usb);
	cimax->priv = NULL;

	g_usb = NULL;
	return 0;
}
EXPORT_SYMBOL(aml_cimax_usb_exit);

static int cimax_usb_reset(struct cimax_usb *usb, int reset_val)
{
	pr_dbg("reset usb:%p, rst:%d\n", usb, usb ? usb->rst_io : -1);
	if (!usb)
		return -ENODEV;

	pr_inf("cimax usb reset\n");

	/*notify unplugged*/
	aml_cimax_camchanged(usb->cimax, 0, 0);
	aml_cimax_camchanged(usb->cimax, 1, 0);

	if (usb->dev)
		cimax_usb_device_close(usb->dev);

	cimax_usb_setup_poll(usb, STOP_MODE);

	usb->cam_inserted[0] = usb->cam_inserted[1] = 0;
	usb->cam_data_ready[0] = usb->cam_data_ready[1] = 0;

	cimax_usb_hw_reset(usb, reset_val);

	pr_inf("cimax usb reset end\n");
	return 0;
}

static ssize_t reset_show(struct class *class,
	struct class_attribute *attr, char *buf)
{
	int ret;
	ret = sprintf(buf, "echo 1 > %s\n", attr->attr.name);
	return ret;
}

static ssize_t reset_store(struct class *class,
	struct class_attribute *attr, const char *buf, size_t size)
{
	int ret;
	int val = 0;
	if (!g_usb)
		return size;
	ret = sscanf(buf, "%i", &val);
	if (ret == 1)
		ret = cimax_usb_reset(g_usb, val);
	return size;
}

static ssize_t debug_show(struct class *class,
	struct class_attribute *attr, char *buf)
{
	int ret = 0;
	if (!g_usb)
		return ret;

	ret = sprintf(buf, "poll mode: %d\n", g_usb->poll_mode);
	ret += sprintf(buf+ret, "status slot[0]=[%d] slot[1]=[%d]\n",
		g_usb->cam_inserted[0], g_usb->cam_inserted[1]);
	{
		int power = 0;
		int err = cimax_usb_check_poe(g_usb, &power);
		ret += sprintf(buf+ret, "power slot[0]=[%d] slot[1]=[%d]\n",
			err ? -1 : power, 0);
	}
	ret += sprintf(buf+ret, "data slot[0]=[%d] slot[1]=[%d]\n",
		g_usb->cam_data_ready[0], g_usb->cam_data_ready[1]);
	ret += sprintf(buf+ret, "work cnt:%d\n", g_usb->work_cnt);
	ret += sprintf(buf+ret, "pwr work cnt:%d\n", g_usb->power_work_cnt);
	return ret;
}

static int reg_addr;
static ssize_t addr_show(struct class *class,
	struct class_attribute *attr, char *buf)
{
	int ret = 0;
	ret = sprintf(buf, "addr = 0x%04x\n", reg_addr);
	return ret;
}

static ssize_t addr_store(struct class *class,
	struct class_attribute *attr, const char *buf, size_t size)
{
	if (!g_usb)
		return size;
	if (sscanf(buf, "%i", &reg_addr) != 1)
		return size;
	return size;
}

static ssize_t reg_show(struct class *class,
	struct class_attribute *attr, char *buf)
{
	int ret = 0;
	u8 reg_val = 0;
	struct aml_cimax *cimax = NULL;

	if (!g_usb)
		return ret;

	cimax = g_usb->cimax;
	ret = aml_cimax_usb_read_reg(cimax, reg_addr, &reg_val, 1);
	if (ret)
		ret = sprintf(buf, "read fail, err=%d\n", ret);
	else
		ret = sprintf(buf, "reg[0x%04x] = 0x%02x\n", reg_addr, reg_val);
	return ret;
}

static ssize_t reg_store(struct class *class,
	struct class_attribute *attr, const char *buf, size_t size)
{
	int ret = 0;
	struct aml_cimax *cimax = NULL;
	int val = 0;
	u8 reg_val = 0;

	if (!g_usb)
		return size;

	if (sscanf(buf, "%i", &val) != 1)
		return size;
	reg_val = val;
	cimax = g_usb->cimax;
	ret = aml_cimax_usb_write_reg(cimax, reg_addr, &reg_val, 1);
	if (ret)
		return ret;
	return size;
}

static int cis_mode; /*0:hex 1:binary*/
static ssize_t cis_show(struct class *class,
	struct class_attribute *attr, char *buf)
{
	int ret = 0;

	if (!g_usb || !g_usb->cis)
		return ret;

	if (cis_mode == 0) {
		int i;
		for (i = 0; i < CIS_MAX; i++) {
			if (i && !(i & 0xf))
				ret += sprintf(buf+ret, "\n");
			ret += sprintf(buf+ret, "%02X ", g_usb->cis[i]);
		}
		ret += sprintf(buf+ret, "\n");
		return ret;
	} else {
		memcpy(buf, g_usb->cis, CIS_MAX);
		return CIS_MAX;
	}
	return ret;
}

static ssize_t cis_store(struct class *class,
	struct class_attribute *attr, const char *buf, size_t size)
{
	if (size >= 3
		&& !memcmp(buf, "bin", 3))
		cis_mode = 1;
	else
		cis_mode = 0;
	return size;
}

static ssize_t ts_rate_show(struct class *class,
	struct class_attribute *attr, char *buf)
{
	int ret = 0;
	u8 lsb = 0, msb = 0, plen = 0;
	struct aml_cimax *cimax = NULL;
	int err = 0;

	if (!g_usb)
		return ret;

	cimax = g_usb->cimax;
	err = aml_cimax_usb_read_reg(cimax, PCK_LENGTH, &plen, 1);
	err |= aml_cimax_usb_read_reg(cimax, BITRATE_CH0_LSB, &lsb, 1);
	err |= aml_cimax_usb_read_reg(cimax, BITRATE_CH0_MSB, &msb, 1);
	if (err)
		ret += sprintf(buf+ret, "read fail, err=%d\n", err);
	else if (!byte_to_u16(msb, lsb))
		ret += sprintf(buf+ret, "rate[0] = 0 Kbps\n");
	else
		ret += sprintf(buf+ret, "rate[0] = %d Kbps\n",
			540*plen*8/byte_to_u16(msb, lsb));
	if (err)
		return ret;

	err = aml_cimax_usb_read_reg(cimax, BITRATE_CH1_LSB, &lsb, 1);
	err |= aml_cimax_usb_read_reg(cimax, BITRATE_CH1_MSB, &msb, 1);
	if (err)
		ret += sprintf(buf+ret, "read fail, err=%d\n", err);
	else if (!byte_to_u16(msb, lsb))
		ret += sprintf(buf+ret, "rate[1] = 0 Kbps\n");
	else
		ret += sprintf(buf+ret, "rate[1] = %d Kbps\n",
			540*plen*8/byte_to_u16(msb, lsb));
	return ret;
}

static ssize_t loop_show(struct class *class,
	struct class_attribute *attr, char *buf)
{
	int ret = 0;
	u8 ch = 0, mod = 0;
	struct aml_cimax *cimax = NULL;
	int err = 0;

	if (!g_usb)
		return ret;

	cimax = g_usb->cimax;
	err = aml_cimax_usb_read_reg(cimax, ROUTER_CAM_CH, &ch, 1);
	err |= aml_cimax_usb_read_reg(cimax, ROUTER_CAM_MOD, &mod, 1);
	if (err) {
		ret = sprintf(buf, "read fail, err=%d\n", err);
		return ret;
	}
	ret += sprintf(buf + ret, "OUT-0 <= ");
	switch (ch & 0x0f) {
	case 0x0:
		ret += sprintf(buf + ret, "CAM-A"); break;
	case 0x1:
		ret += sprintf(buf + ret, "CH0-IN"); break;
	case 0x2:
		ret += sprintf(buf + ret, "CH1-IN"); break;
	case 0x3:
		ret += sprintf(buf + ret, "REMAPPER"); break;
	case 0x4:
		ret += sprintf(buf + ret, "PREHEADER"); break;
	case 0x5:
		ret += sprintf(buf + ret, "CAM-B"); break;
	case 0x6:
		ret += sprintf(buf + ret, "GAPREMOVER-0"); break;
	case 0x7:
		ret += sprintf(buf + ret, "GAPREMOVER-1"); break;
	case 0x8:
		ret += sprintf(buf + ret, "NONE"); break;
	default:
		ret += sprintf(buf + ret, "UNKNOWN"); break;
	}
	ret += sprintf(buf + ret, "\nCAM-A <= ");
	switch (mod & 0x07) {
	case 0x1:
		ret += sprintf(buf + ret, "CH0-IN"); break;
	case 0x2:
		ret += sprintf(buf + ret, "CH1-IN"); break;
	case 0x3:
		ret += sprintf(buf + ret, "REMAPPER"); break;
	case 0x4:
		ret += sprintf(buf + ret, "PREHEADER"); break;
	case 0x5:
		ret += sprintf(buf + ret, "CAM-B"); break;
	case 0x6:
		ret += sprintf(buf + ret, "GAPREMOVER-0"); break;
	case 0x7:
		ret += sprintf(buf + ret, "GAPREMOVER-1"); break;
	default:
		ret += sprintf(buf + ret, "NONE"); break;
	}
	ret += sprintf(buf + ret, "\n");

	return ret;
}

static ssize_t loop_store(struct class *class,
	struct class_attribute *attr, const char *buf, size_t size)
{
	int loop = 0;

	if (!g_usb)
		return size;

	if (sscanf(buf, "%i", &loop) == 1)
		cimax_usb_set_loop(g_usb, loop);
	return size;
}

static ssize_t slot_reset_store(struct class *class,
	struct class_attribute *attr, const char *buf, size_t size)
{
	int err = 0;
	int slot = 0;
	struct aml_cimax *cimax = NULL;

	if (!g_usb)
		return size;

	if (sscanf(buf, "%i", &slot) == 1) {
		if (slot == 0 || slot == 1) {
			pr_dbg("reset slot %d\n", slot);
			cimax = g_usb->cimax;
			err = aml_cimax_usb_slot_reset(cimax, slot);
		}
	}
	return size;
}

static ssize_t detect_store(struct class *class,
	struct class_attribute *attr, const char *buf, size_t size)
{
	int err = 0;
	int slot = 0;
	struct aml_cimax *cimax = NULL;

	if (!g_usb)
		return size;

	if (sscanf(buf, "%i", &slot) == 1) {
		if (slot == 0 || slot == 1) {
			int addr = (!slot) ? MOD_CTRL_A : MOD_CTRL_B;
			u8 reg = 0;
			cimax = g_usb->cimax;
			err = aml_cimax_usb_read_reg(cimax, addr, &reg, 1);
			cam_usb_cam_detect(g_usb, slot,
				(reg & 1) ? IN_INSERTED : 0);
			cimax_usb_cam_powerctrl(g_usb, slot, (reg & 1));
		}
	}
	return size;
}

static CLASS_ATTR_RW(reset);
static CLASS_ATTR_RO(debug);
static CLASS_ATTR_RW(addr);
static CLASS_ATTR_RW(reg);
static CLASS_ATTR_RW(cis);
static CLASS_ATTR_RO(ts_rate);
static CLASS_ATTR_RW(loop);
static CLASS_ATTR_WO(slot_reset);
static CLASS_ATTR_WO(detect);

#define CLASS_ATTR(name) &class_attr_##name.attr

static struct attribute *cimax_usb_class_attrs[] = {
	CLASS_ATTR(reset),
	CLASS_ATTR(debug),
	CLASS_ATTR(addr),
	CLASS_ATTR(reg),
	CLASS_ATTR(cis),
	CLASS_ATTR(ts_rate),
	CLASS_ATTR(loop),
	CLASS_ATTR(slot_reset),
	CLASS_ATTR(detect),
	NULL
};


ATTRIBUTE_GROUPS(cimax_usb_class);

static struct class cimax_usb_class = {
	.name = "cimax_usb",
	.class_groups = cimax_usb_class_groups,
};

static int aml_cimax_usb_mod_init(struct platform_device *pdev)
{
	int ret;
	pr_dbg("Amlogic CIMAX USB Init\n");

	ret = class_register(&cimax_usb_class);

	return 0;
}

static void aml_cimax_usb_mod_exit(struct platform_device *pdev)
{
	pr_dbg("Amlogic CIMAX USB Exit\n");
	class_unregister(&cimax_usb_class);
}

