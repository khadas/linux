/***************************************************************************
 * Copyright (c) 2014 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 *
***************************************************************************/

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
#include <linux/mutex.h>
#include <linux/firmware.h>
#include <linux/delay.h>

#include "aml_cimax.h"

#define MOD_NAME       "aml_cimax_spi"

#define pr_dbg(fmt...)\
	do {\
		if (cimax_spi_debug)\
			pr_info("cimax_spi: "fmt);\
	} while (0)
#define pr_error(fmt...) pr_err("AML_CIMAX_SPI: " fmt)

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

struct cimax_spi {
	struct platform_device *pdev;
	struct spi_device *dev;

	struct aml_cimax *cimax;

	u8 buf[CIMAX_REG_HDR_SIZE + CIMAX_CAM_HDR_SIZE + CIMAX_CAM_PLD_SIZE];

	int cam_inserted[2];
	int cam_data_ready[2];

	int poll_mode;
#define STOP_MODE 0
#define POLL_MODE 1
#define INT_MODE  2

	int irq;
	int irq_io;
	int rst_io;

	struct workqueue_struct *workq;
	struct delayed_work work;
	int work_cnt;

	DEF_LOCK(lock);
#define lock_init(_spi) mutex_init(&(_spi)->lock)
#define lock_lock(_spi) do {\
	int err = mutex_lock_interruptible(&(_spi)->lock);\
	if (err)\
		return err;\
} while (0)
#define lock_unlock(_spi) mutex_unlock(&(_spi)->lock)

	u8 *cis;
#define CIS_MAX 512
};

static struct cimax_spi *g_spi;

MODULE_PARM_DESC(debug, "enable verbose debug messages");
static int cimax_spi_debug = 1;
module_param_named(debug, cimax_spi_debug, int, 0644);

MODULE_PARM_DESC(poll_interval, "interval for spi poll");
static int spi_poll_interval = 100;
module_param_named(poll_interval, spi_poll_interval, int, 0644);

MODULE_PARM_DESC(poll_mode, "set cimax poll mode, need reset");
static int cimax_poll_mode = 1;
module_param_named(poll_mode, cimax_poll_mode, int, 0644);

MODULE_PARM_DESC(cam_irq_mode, "set cam irq mode, need reset");
static int cam_irq_mode;
module_param_named(cam_irq_mode, cam_irq_mode, int, 0644);

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

#define cimax_to_spi(_c) ((struct cimax_spi *)((_c)->priv))
#define dev_to_spi(_d) ((struct cimax_spi *)spi_get_drvdata(_d))

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

#define USE_INT_PIO

static int aml_cimax_spi_mod_init(void);
static void aml_cimax_spi_mod_exit(void);

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

static void perr(char *err, struct cimax_spi *spi)
{
	pr_error("error: %s\n", err);
	dump("dump:", spi->buf, 16);
}

static inline unsigned long get_jiffies(void)
{
	return (unsigned long)(sched_clock()/10000000);
}

static inline void set_spi_cam_ready(struct cimax_spi *spi, int slot)
{
	if (spi->cam_inserted[slot] == 1) {
		spi->cam_inserted[slot] = 2;
		aml_cimax_slot_state_changed(spi->cimax,
			slot, spi->cam_inserted[slot]);
	}
}


static int cimax_spi_get_resp(struct cimax_spi *spi, int timeout)
{
	struct spi_device *dev = spi->dev;
	int ret = 0;
	unsigned long start = get_jiffies();
#ifndef USE_INT_PIO
	u8 t = 0;

	while (!t && jiffies_to_msecs(get_jiffies() - start) < timeout) {
		ret = spi_read(dev, &t, 1);
		if (ret)
			return ret;
		usleep_range(1000, 2000);
	}
	if (!t) {
		pr_error("resp timeout: %dms\n", timeout);
		return -EIO;
	}
	cam_hdr(spi)[0] = t;
	ret = spi_read(dev, cam_hdr(spi)+1, CIMAX_CAM_HDR_SIZE-1);
	if (ret)
		return ret;
#else
	do {
		if (jiffies_to_msecs(get_jiffies() - start) >= timeout) {
			pr_error("resp timeout: %dms\n", timeout);
			return -EIO;
		}
		usleep_range(1000, 2000);
	} while (gpio_get_value(spi->irq_io));

	ret = spi_read(dev, cam_hdr(spi), CIMAX_CAM_HDR_SIZE);
	if (ret)
		return ret;
#endif
	/*pr_dbg("rp: %02x:%02x:%02x:%02x\n",
		cam_hdr(spi)[0], cam_hdr(spi)[1],
		cam_hdr(spi)[2], cam_hdr(spi)[3]);*/
	switch (hdr_cmd_resp(spi)) {
	case CIMAX_REG_READ_OK:
	case CIMAX_REG_WRITE_OK:
	case CIMAX_REG_INIT_OK:
	case CIMAX_REG_CMD_ERROR: {
		int len = reg_hdr_dat_size(spi);
		if (len) {
			ret = spi_read(dev, reg_dat(spi), len);
			if (ret != 0)
				return ret;
		}
		} break;
	case CIMAX_CAM_RESET_OK:
	case CIMAX_CAM_CIS_OK:
	case CIMAX_CAM_COR_OK:
	case CIMAX_CAM_NEG_OK:
	case CIMAX_CAM_WLPDU_OK:
	case CIMAX_CAM_RLPDU_OK:
	case CIMAX_CAM_NOCAM:
	case CIMAX_CAM_ERROR:
	case CIMAX_CAM_WBUSY:
	case CIMAX_CAM_DET_OK:
	case CIMAX_CAM_DATA_READY:
	case CIMAX_CAM_PENDING:
	case CIMAX_CAM_REGSTAT_OK:
	case CIMAX_CAM_NOEVT: {
		int len = cam_hdr_dat_size(spi);
		if (len) {
			ret = spi_read(dev, cam_dat(spi), len);
			if (ret != 0)
				return ret;
			/*if (len)
				dump("dat:", cam_dat(spi), len);*/
		}
		}
		break;
	default:
		pr_error("unknown resp:0x%02x\n", hdr_cmd_resp(spi));
		return -EINVAL;
	}
	return 0;
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

static int aml_cimax_spi_read_reg(struct aml_cimax *cimax, int addr,
		u8 *buf, int size)
{
	struct cimax_spi *spi = cimax_to_spi(cimax);
	struct spi_device *dev = spi->dev;
	int err = 0;

	init_reg_hdr(spi->buf, CIMAX_REG_READ, addr, size);

	lock_lock(spi);

	pr_dbg("rd %02x:%02x:%02x:%02x\n",
		reg_hdr(spi)[0], reg_hdr(spi)[1],
		reg_hdr(spi)[2], reg_hdr(spi)[3]);
	err = spi_write(dev, spi->buf, CIMAX_REG_HDR_SIZE);
	if (err)
		goto end;
	err = cimax_spi_get_resp(spi, REG_TIMEOUT);
	if (err)
		goto end;
	if (check_reg_hdr(reg_hdr(spi), CIMAX_REG_READ_OK, addr, size) != 0) {
		perr("read reg fail.", spi);
		err = -EINVAL;
		goto end;
	}
	memcpy(buf, reg_dat(spi), size);
end:
	lock_unlock(spi);
	return err;
}

static int aml_cimax_spi_write_reg(struct aml_cimax *cimax, int addr,
		u8 *buf, int size)
{
	struct cimax_spi *spi = cimax_to_spi(cimax);
	struct spi_device *dev = spi->dev;
	int err = 0;

	init_reg_hdr(spi->buf, CIMAX_REG_WRITE, addr, size);
	memcpy(&spi->buf[CIMAX_REG_HDR_SIZE], buf, size);

	lock_lock(spi);

	pr_dbg("wr %02x:%02x:%02x:%02x\n",
		reg_hdr(spi)[0], reg_hdr(spi)[1],
		reg_hdr(spi)[2], reg_hdr(spi)[3]);
	err = spi_write(dev, spi->buf, CIMAX_REG_HDR_SIZE + size);
	if (err)
		goto end;
	err = cimax_spi_get_resp(spi, REG_TIMEOUT);
	if (err)
		goto end;
	if (check_reg_hdr(reg_hdr(spi), CIMAX_REG_WRITE_OK, addr, 0) != 0) {
		perr("write reg fail.", spi);
		err = -EINVAL;
		goto end;
	}
end:
	lock_unlock(spi);
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

static inline int cam_err(struct cimax_spi *spi)
{
	if (hdr_cmd_resp(spi) != CIMAX_CAM_ERROR
		|| cam_hdr_pkt_cnt(spi) != CIMAX_CAM_PKT_CNT_VAL
		|| cam_hdr_dat_size(spi) != 2)
		return 0;
	return byte_to_u16(cam_dat(spi)[0], cam_dat(spi)[1]);
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

static int cimax_spi_access_cam(struct cimax_spi *spi, int slot,
		int cmd, u8 *buf, int size)
{
	struct spi_device *dev = spi->dev;
	int err = 0;

	cmd |= slot ? 0x80 : 0;
	init_cam_hdr(cam_hdr(spi), cmd, size);
	memcpy(cam_dat(spi), buf, size);
	/*dump("access cam:", cam_hdr(spi), CIMAX_CAM_HDR_SIZE+size);*/
	err = spi_write(dev, cam_hdr(spi), CIMAX_CAM_HDR_SIZE + size);
	if (err)
		return err;
	err = cimax_spi_get_resp(spi, CAM_TIMEOUT);
	if (err)
		return err;
	if (cam_hdr_slot(spi) != slot) {
		pr_error("expect slot(%d), but slot(%d)\n",
			slot, cam_hdr_slot(spi));
		return -EINVAL;
	}
	switch (hdr_cmd_resp(spi)) {
	case CIMAX_CAM_NOCAM:
		pr_dbg("no cam\n");
		err = -ENODEV;
		break;
	case CIMAX_CAM_ERROR:
		pr_error("cam error\n");
		pr_error("err code: 0x%04x(%s)\n", cam_err(spi),
			cam_err_str(cam_err(spi)));
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
	return err;
}

static int aml_cimax_spi_read_cis(struct aml_cimax *cimax, int slot,
		u8 *buf, int size)
{
	struct cimax_spi *spi = cimax_to_spi(cimax);
	int err = 0;
	int len;

	lock_lock(spi);

	err = cimax_spi_access_cam(spi, slot, CIMAX_CAM_CIS, NULL, 0);
	if (err)
		goto end;
	if (hdr_cmd_resp(spi) != CIMAX_CAM_CIS_OK
		|| cam_hdr_pkt_cnt(spi) != CIMAX_CAM_PKT_CNT_VAL) {
		perr("read cis fail.", spi);
		err = -EINVAL;
		goto end;
	}
	len = cam_hdr_dat_size(spi);
	if (size < len) {
		pr_error("cis size too large, expect<%d, but:%d\n", size, len);
		perr("cis fail.", spi);
		err = -EINVAL;
		goto end;
	}
	memcpy(buf, cam_dat(spi), len);

	if (!spi->cis)
		spi->cis = kzalloc((len < 512) ? 512 : len, GFP_KERNEL);
	if (spi->cis)
		memcpy(spi->cis, cam_dat(spi), len);

end:
	lock_unlock(spi);
	return err;
}
#define CIMAX_CAM_COR_PLD_SIZE 5
static int aml_cimax_spi_write_cor(struct aml_cimax *cimax, int slot,
		int addr, u8 *buf)
{
	struct cimax_spi *spi = cimax_to_spi(cimax);
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

	lock_lock(spi);

	err = cimax_spi_access_cam(spi, slot, CIMAX_CAM_COR, out, sz);
	if (err)
		goto end;
	if (hdr_cmd_resp(spi) != CIMAX_CAM_COR_OK
		|| cam_hdr_pkt_cnt(spi) != CIMAX_CAM_PKT_CNT_VAL
		|| cam_hdr_dat_size(spi) != 0) {
		perr("write cor fail.", spi);
		err = -EINVAL;
		goto end;
	}
end:
	lock_unlock(spi);
	return err;
}
#define CIMAX_CAM_NEG_PLD_SIZE 2
static int aml_cimax_spi_negotiate(struct aml_cimax *cimax, int slot, int size)
{
	struct cimax_spi *spi = cimax_to_spi(cimax);
	int ret = 0;
	u8 out[CIMAX_CAM_NEG_PLD_SIZE];

	out[0] = (size>>8) & 0xff;
	out[1] = size & 0xff;

	lock_lock(spi);

	ret = cimax_spi_access_cam(spi, slot, CIMAX_CAM_NEG,
			out, CIMAX_CAM_NEG_PLD_SIZE);
	if (ret)
		goto end;
	if (hdr_cmd_resp(spi) != CIMAX_CAM_NEG_OK
		|| cam_hdr_pkt_cnt(spi) != CIMAX_CAM_PKT_CNT_VAL
		|| cam_hdr_dat_size(spi) != 2) {
		perr("negotiate fail.", spi);
		ret = -EINVAL;
		goto end;
	}
	ret = byte_to_u16(cam_dat(spi)[0], cam_dat(spi)[1]);

	set_spi_cam_ready(spi, slot);
end:
	lock_unlock(spi);
	return ret;
}

static int aml_cimax_spi_write_lpdu(struct aml_cimax *cimax, int slot,
		u8 *buf, int size)
{
	struct cimax_spi *spi = cimax_to_spi(cimax);
	int ret = 0;

	lock_lock(spi);

	/*dump("lpdu ->", buf, size);*/
	ret = cimax_spi_access_cam(spi, slot, CIMAX_CAM_WLPDU, buf, size);
	if (ret)
		goto end;
	if (hdr_cmd_resp(spi) != CIMAX_CAM_WLPDU_OK
		|| cam_hdr_pkt_cnt(spi) != CIMAX_CAM_PKT_CNT_VAL
		|| cam_hdr_dat_size(spi) != 0) {
		perr("write lpdu fail.", spi);
		ret = -EINVAL;
		goto end;
	}
	ret = size;
end:
	lock_unlock(spi);
	return ret;
}

static int aml_cimax_spi_read_lpdu(struct aml_cimax *cimax, int slot,
		u8 *buf, int size)
{
	struct cimax_spi *spi = cimax_to_spi(cimax);
	int ret = 0;

	lock_lock(spi);

	ret = cimax_spi_access_cam(spi, slot, CIMAX_CAM_RLPDU, NULL, 0);
	if (ret)
		goto end;
	if (hdr_cmd_resp(spi) != CIMAX_CAM_RLPDU_OK
		|| cam_hdr_pkt_cnt(spi) != CIMAX_CAM_PKT_CNT_VAL) {
		perr("read lpdu fail.", spi);
		ret = -EINVAL;
		goto end;
	}
	ret = cam_hdr_dat_size(spi);
	memcpy(buf, cam_dat(spi), ret);

	/*dump("lpdu <-", buf, ret);*/

	spi->cam_data_ready[slot] = 0;
end:
	lock_unlock(spi);
	return ret;
}

static int aml_cimax_spi_read_cam_status(struct aml_cimax *cimax, int slot)
{
	struct cimax_spi *spi = cimax_to_spi(cimax);
	int ret = 0;

	if (cam_irq_mode && spi->cam_data_ready[slot])
		return 0x80;

	lock_lock(spi);

	ret = cimax_spi_access_cam(spi, slot, CIMAX_CAM_REGSTAT, NULL, 0);
	if (ret)
		goto end;
	if (hdr_cmd_resp(spi) != CIMAX_CAM_REGSTAT_OK
		|| cam_hdr_pkt_cnt(spi) != CIMAX_CAM_PKT_CNT_VAL
		|| cam_hdr_dat_size(spi) != 1) {
		perr("read cam status fail.", spi);
		ret = -EINVAL;
		goto end;
	}

	ret = cam_dat(spi)[0];
end:
	lock_unlock(spi);
	return ret;
}

static int aml_cimax_spi_slot_reset(struct aml_cimax *cimax, int slot)
{
	struct cimax_spi *spi = cimax_to_spi(cimax);
	int ret = 0;

	spi->cam_data_ready[slot] = 0;

	lock_lock(spi);

	ret = cimax_spi_access_cam(spi, slot, CIMAX_CAM_RESET, NULL, 0);
	if (ret)
		goto end;
	if (hdr_cmd_resp(spi) != CIMAX_CAM_RESET_OK
		|| cam_hdr_pkt_cnt(spi) != CIMAX_CAM_PKT_CNT_VAL
		|| cam_hdr_dat_size(spi) != 0) {
		perr("slot reset fail.", spi);
		ret = -EINVAL;
		goto end;
	}
end:
	lock_unlock(spi);
	return ret;
}

static int aml_cimax_spi_cam_reset(struct aml_cimax *cimax, int slot)
{
	pr_dbg("Slot(%d): camreset\n", slot);
	return 0;
}

static int aml_cimax_spi_slot_shutdown(struct aml_cimax *cimax, int slot)
{
	pr_dbg("Slot(%d): shutdown\n", slot);
	return 0;
}
static int aml_cimax_spi_slot_ts_enable(struct aml_cimax *cimax, int slot)
{
	pr_dbg("Slot(%d): ts control\n", slot);
	return 0;
}
static int aml_cimax_spi_slot_status(struct aml_cimax *cimax, int slot)
{
	struct cimax_spi *spi = cimax_to_spi(cimax);
	if (spi->cam_inserted[slot]) {
		/*pr_dbg("CA Module present and ready\n");*/
		return DVB_CA_EN50221_POLL_CAM_PRESENT |
			DVB_CA_EN50221_POLL_CAM_READY;
	} else {
		/*pr_error("CA Module not present or not ready\n");*/
	}
	return 0;
}

static int cimax_spi_cam_plugin(struct cimax_spi *spi, int slot, int plugin)
{
	pr_dbg("cam plug: slot(%d) %s\n",
		slot, plugin ? "plugged" : "unplugged");
	return aml_cimax_camchanged(spi->cimax, slot, plugin);
}

static int cimax_spi_poll(struct cimax_spi *spi)
{
	struct spi_device *dev = spi->dev;
	int err = 0;

	lock_lock(spi);

#ifdef USE_INT_PIO
	if (gpio_get_value(spi->irq_io))
		goto end;
#endif

	init_reg_hdr(spi->buf, CIMAX_CAM_EVT, 0x100, 0);
	err = spi_write(dev, spi->buf, CIMAX_REG_HDR_SIZE);
	if (err)
		goto end;
	err = cimax_spi_get_resp(spi, CAM_TIMEOUT);
	if (err)
		goto end;
	switch (hdr_cmd_resp(spi)) {
	case CIMAX_CAM_DET_OK: {
		int slot = cam_hdr_slot(spi);
		int insert = cam_dat(spi)[0];
		if (!!spi->cam_inserted[slot] != insert) {
			spi->cam_inserted[slot] = insert;
			cimax_spi_cam_plugin(spi, slot, insert);
			aml_cimax_slot_state_changed(spi->cimax, slot,
				spi->cam_inserted[slot]);
		}
		} break;
	case CIMAX_CAM_DATA_READY: {
		int slot = cam_hdr_slot(spi);
		spi->cam_data_ready[slot] = 1;
		} break;
	case CIMAX_CAM_NOEVT:
		break;
	default:
		pr_error("unknown resp:%02x\n", hdr_cmd_resp(spi));
		break;
	}
end:
	queue_delayed_work(spi->workq, &spi->work, spi_poll_interval);
	lock_unlock(spi);
	return 0;
}

static void cimax_spi_poll_work(struct work_struct *work)
{
	struct cimax_spi *spi =
		container_of(to_delayed_work(work), struct cimax_spi, work);
	spi->work_cnt++;
	cimax_spi_poll(spi);
}

static irqreturn_t cimax_irq_handler(int irq, void *para)
{
	return IRQ_HANDLED;
}

#define CTRL_DISABLE -1
#define CTRL_STOP     0
#define CTRL_START    1

static inline int cimax_spi_poll_ctrl(struct cimax_spi *spi, int ctrl)
{
	if (ctrl == CTRL_START) {
		spi->workq = create_singlethread_workqueue("cimax_spi");
		INIT_DELAYED_WORK(&spi->work, &cimax_spi_poll_work);
		queue_delayed_work(spi->workq,
			&spi->work, spi_poll_interval);
		pr_dbg("poll started\n");
	} else {
		if (!spi->workq)
			return 0;
		cancel_delayed_work_sync(&spi->work);
		destroy_workqueue(spi->workq);
		spi->workq = NULL;
		pr_dbg("poll stopped\n");
	}
	return 0;
}

static inline int cimax_spi_intr_ctrl(struct cimax_spi *spi, int ctrl)
{
	if (ctrl == CTRL_START) {
		int ret;
		if (spi->irq == -1) {
			pr_error("incorrect irq");
			return -1;
		}
		ret = request_irq(spi->irq, cimax_irq_handler,
			IRQF_SHARED|IRQF_TRIGGER_RISING,
			"cimax irq", spi);
		enable_irq(spi->irq);
	} else {
		if (spi->irq == -1)
			return 0;
		disable_irq(spi->irq);
		free_irq(spi->irq, spi);
	}
	return 0;
}

static int cimax_spi_setup_poll(struct cimax_spi *spi, int poll_mode)
{
	if (poll_mode == spi->poll_mode)
		return 0;
	switch (poll_mode) {
	case POLL_MODE:
		if (spi->poll_mode == INT_MODE)
			cimax_spi_intr_ctrl(spi, CTRL_DISABLE);
		cimax_spi_poll_ctrl(spi, CTRL_START);
		spi->poll_mode = POLL_MODE;
		break;
	case INT_MODE:
		if (spi->poll_mode == POLL_MODE)
			cimax_spi_poll_ctrl(spi, CTRL_DISABLE);
		cimax_spi_intr_ctrl(spi, CTRL_START);
		spi->poll_mode = INT_MODE;
		break;
	case STOP_MODE:
		if (spi->poll_mode == POLL_MODE)
			cimax_spi_poll_ctrl(spi, CTRL_DISABLE);
		else if (spi->poll_mode == INT_MODE)
			cimax_spi_intr_ctrl(spi, CTRL_DISABLE);
		spi->poll_mode = STOP_MODE;
		break;
	default:
		break;
	}
	return 0;
}

static int cimax_spi_hw_reset(struct cimax_spi *spi, int reset_val)
{
	/*trigger reset io*/
	if (spi->rst_io) {
		gpio_direction_output(spi->rst_io, reset_val ? 1 : 0);
		msleep(50);
		gpio_direction_output(spi->rst_io, reset_val ? 0 : 1);
	}
	return 0;
}


enum regOperation_e {
	/** Read register. */
	REG_OP_READ,
	/** Write register. */
	REG_OP_WRITE,
	/** Read register until some bits are set. */
	REG_OP_WAIT_TO_BE_SET,
	/** Read register until some bits are cleared. */
	REG_OP_WAIT_TO_BE_CLEARED,
	/** Read register until it's value is not equal to defined. */
	REG_OP_WAIT_EQUAL,
	/** Perform logical AND over register. */
	REG_OP_LOGICAL_AND,
	/** Perform logical OR over register. */
	REG_OP_LOGICAL_OR,
	/** Wait timeout in miliseconds. */
	REG_OP_WAIT
};

struct regSettings_s {
	/** CIMaX+ register address. */
	u16         reg;
	/** CIMaX+ register value. */
	u16         val;
	/** CIMaX+ register operation. */
	enum regOperation_e op;
};

static struct regSettings_s spiRegSettings[] = {
	/** TS interface init. */
	{IN_SEL,          0x00, REG_OP_WRITE},          /** Close TS input. */
	{OUT_SEL,         0x00, REG_OP_WRITE},          /** Close TS output. */
	{FIFO_CTRL,       0x0f, REG_OP_WRITE},          /** Reset TS FIFO.  */
	{SYNC_RTV_CTRL,   0x0f, REG_OP_WRITE},

	/** CAM power. */
	{GPIO0_DATA_OUT,  0x00, REG_OP_WRITE},
	/** Unlock CFG. */
	{CFG_2,           0x00, REG_OP_WRITE},
	/** 1) DVB/CI/CI+/SCARD 2slot. */
	{CFG_1,           0x00, REG_OP_WRITE},
	/** 2) Set the Default "power off" state
	such as VCC_MODA=VCC_MODB=VPPx_MODA=VPPx_MODB='Z'. */
	{GPIO0_DFT,       0x00, REG_OP_WRITE},
	/** 3) Set GPIO3 as external power switch driver. */
	{GPIO0_MASK_DATA, 0x07, REG_OP_WRITE},
	/** 4) Set "power on" state (VCC=VPP1=VPP2= 5V). */
	{GPIO0_DATA_OUT,  0x03, REG_OP_WRITE},
	/** 5) Lock config. */
	{CFG_2,           0x01, REG_OP_WRITE},
	/** 6) Write in the GPIO0_DIR_REG: defines the GPIOs,
	which are used to drive the external power switch, in output mode. */
	{GPIO0_DIR,       0x07, REG_OP_WRITE},
	/** 7) Check VCCENable. */
	{CFG_1,           0x20, REG_OP_WAIT_TO_BE_SET},
	/** 8) Set & wait for PcmciaOutputEnable. */
	{CFG_1,           0x08, REG_OP_LOGICAL_OR},
	{CFG_1,           0x08, REG_OP_WAIT_TO_BE_SET},

	/** Set router CAM. */
	/** CH0 & CH1 from CAM A & B, CAM A & B from CH0 & CH1. */
	{ROUTER_CAM_MOD,  0x21, REG_OP_WRITE},
	{ROUTER_CAM_CH,   0x00, REG_OP_WRITE},
	/** Wait 200 miliseconds. */
	{0x0000,          200,  REG_OP_WAIT},

	/** Set In/Out. */
	/** Route CAM Channel 0 to Channel 0, Channel 1 null. */
	{ROUTER_CAM_CH,   0x80, REG_OP_WRITE},

#ifdef PARALLEL_OUT
#else
#if 1
	/*72M internal clock source*/
	/*CLK Select SER0->72M*/
	{CkMan_Select,    0x20, REG_OP_WRITE},
	/*Enable SER0 clk source, Enable 72M clk source*/
	{CkMan_Config,    0x44, REG_OP_LOGICAL_OR},
#else
	/*108M internal clock source*/
	/*CLK Select SER0->108M*/
	{CkMan_Select,    0x30, REG_OP_WRITE},
	/*Enable SER0 clk source, Enable 108M clk source*/
	{CkMan_Config,    0x48, REG_OP_LOGICAL_OR},
#endif
	{P2S_CH0_CTRL,    0x19, REG_OP_WRITE},           /*Enable p2s*/
	{OUT_SEL,         0x02, REG_OP_WRITE},           /*Out1=p2s0*/
#endif /*Parallel out*/

	/** Input Ch0=Parallel, Ch1=null. */
	{IN_SEL,          0x01, REG_OP_WRITE},
};

int downloadCfg(struct cimax_spi *spi)
{
	u32   cnt;
	u8    buf[CIMAX_REG_PLD_SIZE];
	struct aml_cimax *cimax = spi->cimax;

	pr_info("Download CIMaX+ configuration(register settings):\n");

	for (cnt = 0; cnt < sizeof(spiRegSettings)/sizeof(struct regSettings_s);
			cnt++) {
		pr_dbg("reg:%04x, val:%02x, op:%d\n",
				spiRegSettings[cnt].reg,
				spiRegSettings[cnt].val,
				spiRegSettings[cnt].op);
		switch (spiRegSettings[cnt].op) {
		case REG_OP_READ:
			/* Read register. */
			if (aml_cimax_spi_read_reg(cimax,
					spiRegSettings[cnt].reg, buf, 1) < 0) {
				/* CIMaX+ read error. */
				pr_error("FAILED at REG_OP_READ operation.\n");
				return -1;
			}
			break;
		case REG_OP_WRITE:
			/* Write register. */
			if (aml_cimax_spi_write_reg(cimax,
					spiRegSettings[cnt].reg,
					(u8 *)&spiRegSettings[cnt].val,
					1) < 0) {
				/* CIMaX+ write error. */
				pr_error("FAILED at REG_OP_WRITE operation.\n");
				return -1;
			}
			break;
		case REG_OP_WAIT_TO_BE_SET:
			do {
				if (aml_cimax_spi_read_reg(cimax,
					spiRegSettings[cnt].reg, buf, 1) < 0) {
					/* CIMaX+ read error. */
					pr_error("E REG_OP_WAIT_TO_BE_SET\n");
					return -1;
				}
			} while ((buf[0] & spiRegSettings[cnt].val)
					!= spiRegSettings[cnt].val);
			break;
		case REG_OP_WAIT_TO_BE_CLEARED:
			do {
				if (aml_cimax_spi_read_reg(cimax,
					spiRegSettings[cnt].reg, buf, 1) < 0) {
					/* CIMaX+ read error. */
					pr_error("REG_OP_WAIT_TO_BE_CLEARED\n");
					return -1;
				}
			} while ((buf[0] & spiRegSettings[cnt].val) != 0);
			break;
		case REG_OP_WAIT_EQUAL:
			do {
				if (aml_cimax_spi_read_reg(cimax,
					spiRegSettings[cnt].reg, buf, 1) < 0) {
					/* CIMaX+ read error. */
					pr_error("REG_OP_WAIT_EQUAL.\n");
					return -1;
				}
			} while (buf[0] != spiRegSettings[cnt].val);
			break;
		case REG_OP_LOGICAL_AND:
			if (aml_cimax_spi_read_reg(cimax,
				spiRegSettings[cnt].reg, buf, 1) < 0) {
				/* CIMaX+ read error. */
				pr_error("FAILED at REG_OP_LOGICAL_AND(r).\n");
				return -1;
			}
			buf[0] &=  spiRegSettings[cnt].val;
			if (aml_cimax_spi_write_reg(cimax,
				spiRegSettings[cnt].reg, buf, 1) < 0) {
				/* CIMaX+ write error. */
				pr_error("FAILED at REG_OP_LOGICAL_AND(w).\n");
				return -1;
			}
			break;
		case REG_OP_LOGICAL_OR:
			if (aml_cimax_spi_read_reg(cimax,
				spiRegSettings[cnt].reg, buf, 1) < 0) {
				/* CIMaX+ read error. */
				pr_error("FAILED at REG_OP_LOGICAL_OR(r).\n");
				return -1;
			}
			buf[0] |=  spiRegSettings[cnt].val;
			if (aml_cimax_spi_write_reg(cimax,
				spiRegSettings[cnt].reg, buf, 1) < 0) {
				/* CIMaX+ write error. */
				pr_error("FAILED at REG_OP_LOGICAL_AND(w).\n");
				return -1;
			}
			break;
		case REG_OP_WAIT:
			msleep(spiRegSettings[cnt].val);
			break;
		default:
			pr_error("\nInvalid operation 0x%02x!\n",
				spiRegSettings[cnt].op);
		}
	}
	pr_info("config OK.\n");
	return 0;
}


#define CIMAX_FW_PKT_SIZE    128
#define CIMAX_FW_START_ADDR  0x8000
#define CIMAX_FW_STOP_ADDR   0xcff9
#define CIMAX_FW_VECT_ADDR   0xfffa
#define CIMAX_FW_VECT_SIZE   6

static u32 compute_bistrom(const u8 *ptr, int size, u32 sign)
{
	int k, i;
	u16 s;

	for (k = 0; k < size; k++) {
		s = ptr[k]&0x01;
		for (i = 0; i < 16; i++)
			if (0x88B7 & (1<<i))
				s ^= (sign>>i) & 0x01;
		s |= ((sign<<1) ^ (ptr[k])) & 0x00FE;
		s |= (sign<<1) & 0x00FF00;
		sign = s;
	}
	return sign;
}

static int cimax_spi_upload_firmware(struct cimax_spi *spi,
		const u8 *fw_data, u32 *sign)
{
	struct aml_cimax *cimax = spi->cimax;
	int err = 0;
	int addr;
	const u8 *ptr;
	int size;
	int debug = cimax_spi_debug;
	u8 *ptmp = kzalloc(CIMAX_FW_PKT_SIZE + CIMAX_REG_HDR_SIZE, GFP_KERNEL);

	if (!ptmp)
		return -ENOMEM;

	cimax_spi_debug = 0;

	addr = CIMAX_FW_START_ADDR;
	ptr = fw_data + addr;
	while (addr < CIMAX_FW_STOP_ADDR) {
		size = (addr <= (CIMAX_FW_STOP_ADDR+1-CIMAX_FW_PKT_SIZE)) ?
			CIMAX_FW_PKT_SIZE : (CIMAX_FW_STOP_ADDR+1-addr);

		*sign = compute_bistrom(ptr, size, *sign);

		pr_dbg(">>%x@%x\n", size, addr);

		/*dump("w:", (u8*)ptr, size);*/
		err = aml_cimax_spi_write_reg(cimax, addr, (u8 *)ptr, size);
		if (err)
			break;
		err = aml_cimax_spi_read_reg(cimax, addr, ptmp, size);
		if (err)
			break;
		/*dump("r:", ptmp, size);*/
		if (memcmp(ptr, ptmp, size)) {
			pr_error("fw write error.\n");
			err = -ENODEV;
			break;
		}

		addr += size;
		ptr += size;
	}

	if (!err) {
		addr = CIMAX_FW_VECT_ADDR;
		ptr = fw_data + addr;
		size = CIMAX_FW_VECT_SIZE;

		*sign = compute_bistrom(ptr, size, *sign);

		err = aml_cimax_spi_write_reg(cimax, addr, (u8 *)ptr, size);
		if (err)
			goto end;
		err = aml_cimax_spi_read_reg(cimax, addr, ptmp, size);
		if (err)
			goto end;
		if (memcmp(ptr, ptmp, size)) {
			pr_error("fw vect write error.\n");
			err = -ENODEV;
			goto end;
		}
	}
end:
	kfree(ptmp);

	cimax_spi_debug = debug;
	return err;
}

static int cimax_spi_check_bistrom(struct cimax_spi *spi,
		int start, int end, u32 sign)
{
	struct aml_cimax *cimax = spi->cimax;
	int err = 0;
	u8 buf[2];

	buf[0] = (0xd000-start) & 0xff;
	buf[1] = (0xd000-start) >> 8;
	err = aml_cimax_spi_write_reg(cimax, 0x8d, buf, 2);
	if (err)
		return err;
	buf[0] = sign & 0xff;
	buf[1] = sign >> 8;
	err = aml_cimax_spi_write_reg(cimax, 0x80, buf, 2);
	if (err)
		return err;
	buf[0] = 0xf;
	err = aml_cimax_spi_write_reg(cimax, 0x82, buf, 1);
	if (err)
		return err;
	err = aml_cimax_spi_read_reg(cimax, 0x41, buf, 2);
	if (err)
		return err;
	pr_dbg("bist checked: 0x%04x\n", byte_to_u16(buf[0], buf[1]));
	err = aml_cimax_spi_read_reg(cimax, 0x09, buf, 1);
	if (err)
		return err;
	pr_dbg("rom status: 0x%02x\n", buf[0]);
	return buf[0];
}

static int cimax_spi_init_firmware(struct cimax_spi *spi)
{
	struct spi_device *dev = spi->dev;
	int err = 0;

	init_reg_hdr(spi->buf, CIMAX_REG_INIT, 0, 0);
	err = spi_write(dev, spi->buf, CIMAX_REG_HDR_SIZE);
	if (err)
		return err;
	err = cimax_spi_get_resp(spi, REG_TIMEOUT);
	if (err)
		return err;
	if (check_reg_hdr(spi->buf, CIMAX_REG_INIT_OK, 0, 0)) {
		perr("init fw fail.", spi);
		return -EINVAL;
	}
	return err;
}

static void request_fw_callback(const struct firmware *fw, void *context)
{
	u32 sign = 0;
	int err = 0;
	struct cimax_spi *spi = (struct cimax_spi *)context;

	if (!fw)
		return;

	pr_dbg("got fw: %zd @ %p\n", fw->size, fw->data);

    /*cimax_spi_hw_reset(spi, 1);*/

	err = cimax_spi_upload_firmware(spi, fw->data, &sign);
	if (err)
		goto end;
	pr_dbg("upload fw done.\n");
	err = cimax_spi_check_bistrom(spi,
		CIMAX_FW_START_ADDR, CIMAX_FW_STOP_ADDR, sign);
	if (err != 0x2)
		goto end;
	pr_dbg("check bistrom done.\n");
	err = cimax_spi_init_firmware(spi);
	if (err)
		goto end;
end:
	if (fw)
		release_firmware(fw);
	if (err)
		return;

	if (downloadCfg(spi)) {
		pr_error("download config fail.\n");
		return;
	}

	cimax_spi_setup_poll(spi, cimax_poll_mode ? POLL_MODE : INT_MODE);

	return;
}

static int cimax_spi_load_fw(struct cimax_spi *spi)
{
	char *name = "cimax_spidvb.bin";
	return request_firmware_nowait(THIS_MODULE, 1, name,
			&spi->dev->dev, GFP_KERNEL, spi, request_fw_callback);
}

static int cimax_spi_dev_probe(struct spi_device *spi)
{
	int ret;
	struct cimax_spi *cimax_spi;

	pr_dbg("dev probe\n");
	/*setup again?*/
	spi->bits_per_word = 8;
	ret = spi_setup(spi);
	if (ret)
		pr_dbg("spi setup failed\n");

	cimax_spi = dev_get_platdata(&spi->dev);
	cimax_spi->dev = spi;

	spi_set_drvdata(spi, cimax_spi);

	return cimax_spi_load_fw(cimax_spi);
}

static int cimax_spi_dev_remove(struct spi_device *spi)
{
	struct cimax_spi *cimax_spi = dev_get_drvdata(&spi->dev);

	pr_dbg("dev remove\n");
	cimax_spi_setup_poll(cimax_spi, STOP_MODE);
	return 0;
}

static int cimax_spi_get_config_from_dts(struct cimax_spi *spi,
		struct spi_board_info *bdinfo)
{
	struct device_node *child = NULL;
	struct platform_device *pdev = spi->pdev;
	struct device_node *np = pdev->dev.of_node;
	unsigned int val;
	int ret = 0;
	pr_dbg("fetch cimax spi in dts\n");

	child = of_get_child_by_name(np, "cimax");
	if (child == NULL) {
		pr_error("cimax not found in dts\n");
		return -1;
	}
	child = of_get_child_by_name(child, "spi");
	if (!child) {
		pr_error("spi not found in cimax");
		return -1;
	}

	/* get spi config */
	ret = of_property_read_u32(child, "bus_num", &val);
	if (ret)
		pr_error("bus_num not found, use default.\n");
	else
		bdinfo->bus_num = val;
	pr_dbg("bus_num: %d\n", bdinfo->bus_num);
	ret = of_property_read_u32(child, "chip_select", &val);
	if (ret)
		pr_error("chip_select not found, use default.\n");
	else
		bdinfo->chip_select = val;
	pr_dbg("chip_select: %d\n", bdinfo->chip_select);
	ret = of_property_read_u32(child, "max_frequency", &val);
	if (ret)
		pr_error("max_frequency not found, use default.\n");
	else
		bdinfo->max_speed_hz = val;
	pr_dbg("max_speed_hz: %d\n", bdinfo->max_speed_hz);
	ret = of_property_read_u32(child, "mode", &val);
	if (ret)
		pr_error("mode not found, use default.\n");
	else
		bdinfo->mode = val;
	pr_dbg("mode: %d\n", bdinfo->mode);
/*
dvbci {
	compatible = "amlogic, dvbci";
	dev_name = "dvbci";
	io_type = <2>;//0:iobus,1:spi,2:cimax
	cimax {
		io_type = <0> //0:spi 1:usb
		spi {
			spi_bus_num = <0>;
			spi_chip_select = <0>;
			spi_max_frequency = <3000000>;

			rst_gpio = <&gpio_ao GPIOAO_2 GPIO_ACTIVE_HIGH>;

			irq_gpio = <&gpio_ao GPIOAO_2 GPIO_ACTIVE_HIGH>;
			irq = <2>;
			interrupts = <GIC_SPI 66 IRQ_TYPE_LEVEL_HIGH>;
		};
	};

};
*/
#ifdef USE_INT_PIO
	{
		int ret = 0;
		int gpio = -1;
		gpio = of_get_named_gpio_flags(child, "irq-gpios", 0, NULL);
		ret = gpio_request(gpio, "cimax-irq");
		if (ret < 0) {
			pr_error("irq-gpios request fail.\n");
			return ret;
		}

		ret = gpio_direction_input(gpio);

		spi->irq_io = gpio;
		pr_dbg("irq_io: %d\n", spi->irq_io);
	}
#ifdef CIMAX_IRQ
	{
		int irq;
		unsigned int irqflag;
		ret = of_property_read_u32(child, "irq", &gpio_irq);
		irq = irq_of_parse_and_map(child, 0);

		gpio_for_irq(gpio,
			AML_GPIO_IRQ(gpio_irq, FILTER_NUM7, GPIO_IRQ_FALLING));
		spi->irq = irq;
		pr_dbg("irq: %d\n", spi->irq);
	}
#else
	spi->irq = -1;
#endif
#endif/*USE_INT_PIO*/
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

			cimax_spi_hw_reset(spi, 1);

			spi->rst_io = gpio;
			pr_dbg("rst: %d\n", spi->rst_io);
		} else {
			pr_error("rst io got fail, %d\n", gpio);
		}
	}
	return 0;
}

static struct spi_board_info cimax_spi_bdinfo = {
	.modalias = "cimax_spi",
	.mode = SPI_MODE_3,
	.max_speed_hz = 1000000, /* 1MHz */
	.bus_num = 0, /* SPI bus No. */
	.chip_select = 0, /* the device index on the spi bus */
	.controller_data = NULL,
};

static struct spi_driver cimax_spi_dev_driver = {
	.probe = cimax_spi_dev_probe,
	.remove = cimax_spi_dev_remove,
	.driver = {
		.name = "cimax_spi",
		.owner = THIS_MODULE,
	},
};

int aml_cimax_spi_init(struct platform_device *pdev, struct aml_cimax *cimax)
{
	int ret;
	struct cimax_spi *cimax_spi;

	cimax_spi = kzalloc(sizeof(struct cimax_spi), GFP_KERNEL);
	if (!cimax_spi)
		return -ENOMEM;

	cimax_spi->pdev = pdev;
	cimax_spi->cimax = cimax;
	cimax_spi_get_config_from_dts(cimax_spi, &cimax_spi_bdinfo);

	/*init spi_lock*/
	lock_init(cimax_spi);

	/*register device*/
	cimax_spi_bdinfo.platform_data = cimax_spi;
	spi_register_board_info(&cimax_spi_bdinfo, 1);

	/*register driver*/
	ret = spi_register_driver(&cimax_spi_dev_driver);
	if (ret) {
		pr_error("register cimax spi driver failed\n");
		return ret;
	}

	/*init cimax used api.*/
#define WI(_f)\
	cimax->ops._f = aml_cimax_spi_##_f
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
	/*WI(start);*/
	/*WI(stop);*/
	WI(read_reg);
	WI(write_reg);

	cimax->priv = cimax_spi;

	g_spi = cimax_spi;

	aml_cimax_spi_mod_init();
	
	return 0;
}
EXPORT_SYMBOL(aml_cimax_spi_init);

int aml_cimax_spi_exit(struct aml_cimax *cimax)
{
	struct cimax_spi *spi = cimax_to_spi(cimax);

	if (!spi)
		return -ENODEV;

	aml_cimax_spi_mod_exit();

	/*unregister driver*/
	spi_unregister_driver(&cimax_spi_dev_driver);
	/*unregister device*/
	spi_unregister_device(spi->dev);

	if (spi->irq_io)
		gpio_free(spi->irq_io);
	if (spi->rst_io)
		gpio_free(spi->rst_io);

	kfree(spi->cis);

	kfree(spi);
	cimax->priv = NULL;

	g_spi = NULL;
	return 0;
}
EXPORT_SYMBOL(aml_cimax_spi_exit);

static int cimax_spi_reset(struct cimax_spi *spi, int reset_val)
{
	pr_dbg("reset spi:%p, rst:%d\n", spi, spi ? spi->rst_io : -1);
	if (!spi)
		return -ENODEV;

	pr_dbg("cimax spi reset\n");

	cimax_spi_setup_poll(spi, STOP_MODE);

	cimax_spi_hw_reset(spi, reset_val);

	/*notify unplugged*/
	aml_cimax_camchanged(spi->cimax, 0, 0);
	aml_cimax_camchanged(spi->cimax, 1, 0);

	spi->cam_inserted[0] = spi->cam_inserted[1] = 0;
	spi->cam_data_ready[0] = spi->cam_data_ready[1] = 0;

	/*async start fw*/
	cimax_spi_load_fw(spi);

	/*cimax_spi_setup_poll(spi, cimax_poll_mode? POLL_MODE : INT_MODE);*/
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
	if (!g_spi)
		return size;
	ret = sscanf(buf, "%i", &val);
	if (ret == 1)
		ret = cimax_spi_reset(g_spi, val);
	return size;
}

static ssize_t debug_show(struct class *class,
	struct class_attribute *attr, char *buf)
{
	int ret = 0;
	if (!g_spi)
		return ret;

	ret = sprintf(buf, "poll mode: %d\n", g_spi->poll_mode);
	ret += sprintf(buf+ret, "status slot[0]=[%d] slot[1]=[%d]\n",
		g_spi->cam_inserted[0], g_spi->cam_inserted[1]);
	ret += sprintf(buf+ret, "data slot[0]=[%d] slot[1]=[%d]\n",
		g_spi->cam_data_ready[0], g_spi->cam_data_ready[1]);
	ret += sprintf(buf+ret, "work cnt:%d\n", g_spi->work_cnt);
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
	if (!g_spi)
		return size;
	if (sscanf(buf, "%i", &reg_addr) == 1)
		return size;
	return size;
}

static ssize_t reg_show(struct class *class,
	struct class_attribute *attr, char *buf)
{
	int ret = 0;
	u8 reg_val = 0;
	struct aml_cimax *cimax = NULL;

	if (!g_spi)
		return ret;

	cimax = g_spi->cimax;
	ret = aml_cimax_spi_read_reg(cimax, reg_addr, &reg_val, 1);
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

	if (!g_spi)
		return size;

	if (sscanf(buf, "%i", &val) != 1)
		return size;
	reg_val = val;
	cimax = g_spi->cimax;
	ret = aml_cimax_spi_write_reg(cimax, reg_addr, &reg_val, 1);
	if (ret)
		return ret;
	return size;
}

static int cis_mode; /*0:hex 1:binary*/
static ssize_t cis_show(struct class *class,
	struct class_attribute *attr, char *buf)
{
	int ret = 0;

	if (!g_spi || !g_spi->cis)
		return ret;

	if (cis_mode == 0) {
		int i;
		for (i = 0; i < CIS_MAX; i++) {
			if (i && !(i & 0xf))
				ret += sprintf(buf+ret, "\n");
			ret += sprintf(buf+ret, "%02X ", g_spi->cis[i]);
		}
		ret += sprintf(buf+ret, "\n");
		return ret;
	} else {
		memcpy(buf, g_spi->cis, CIS_MAX);
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

	if (!g_spi)
		return ret;

	cimax = g_spi->cimax;
	err = aml_cimax_spi_read_reg(cimax, PCK_LENGTH, &plen, 1);
	err |= aml_cimax_spi_read_reg(cimax, BITRATE_CH0_LSB, &lsb, 1);
	err |= aml_cimax_spi_read_reg(cimax, BITRATE_CH0_MSB, &msb, 1);
	if (err || !byte_to_u16(msb, lsb))
		ret += sprintf(buf+ret, "read fail, err=%d\n", err);
	else
		ret += sprintf(buf+ret, "rate[0] = %d Kbps\n",
			540*plen*8/byte_to_u16(msb, lsb));
	if (err)
		return ret;

	err = aml_cimax_spi_read_reg(cimax, BITRATE_CH1_LSB, &lsb, 1);
	err |= aml_cimax_spi_read_reg(cimax, BITRATE_CH1_MSB, &msb, 1);
	if (err || !byte_to_u16(msb, lsb))
		ret += sprintf(buf+ret, "read fail, err=%d\n", err);
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

	if (!g_spi)
		return ret;

	cimax = g_spi->cimax;
	err = aml_cimax_spi_read_reg(cimax, ROUTER_CAM_CH, &ch, 1);
	err |= aml_cimax_spi_read_reg(cimax, ROUTER_CAM_MOD, &mod, 1);
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
	int err = 0;
	struct aml_cimax *cimax = NULL;

	if (!g_spi)
		return size;

	if (sscanf(buf, "%i", &loop) == 1) {
		int a = g_spi->cam_inserted[0];
		int b = g_spi->cam_inserted[1];
		u8 cm[2];
		cm[0] = loop ? (b ? 0x85 : 0x80) : 0x81;/*CH*/
		cm[1] = loop ? (a ? 0x51 : 0x11) : 0x00;/*MOD*/
		cimax = g_spi->cimax;
		err = aml_cimax_spi_write_reg(cimax, ROUTER_CAM_CH, cm, 2);
	}
	return size;
}

static ssize_t slot_reset_store(struct class *class,
	struct class_attribute *attr, const char *buf, size_t size)
{
	int err = 0;
	int slot = 0;
	struct aml_cimax *cimax = NULL;

	if (!g_spi)
		return size;

	if (sscanf(buf, "%i", &slot) == 1) {
		if (slot == 0 || slot == 1) {
			pr_dbg("reset slot %d\n", slot);
			cimax = g_spi->cimax;
			err = aml_cimax_spi_slot_reset(cimax, slot);
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

	if (!g_spi)
		return size;

	if (sscanf(buf, "%i", &slot) == 1) {
		if (slot == 0 || slot == 1) {
			int addr = (!slot) ? MOD_CTRL_A : MOD_CTRL_B;
			u8 reg = 0;
			cimax = g_spi->cimax;
			err = aml_cimax_spi_read_reg(cimax, addr, &reg, 1);
			g_spi->cam_inserted[slot] = reg & 1;
			pr_dbg("detect slot(%d): %d\n", slot, reg & 1);
		}
	}
	return size;
}

static struct class_attribute cimax_spi_class_attrs[] = {
	__ATTR_RW(reset),
	__ATTR_RO(debug),
	__ATTR_RW(addr),
	__ATTR_RW(reg),
	__ATTR_RW(cis),
	__ATTR_RO(ts_rate),
	__ATTR_RW(loop),
	__ATTR_WO(slot_reset),
	__ATTR_WO(detect),
	__ATTR_NULL
};

static struct class cimax_spi_class = {
	.name = "cimax_spi",
	.class_attrs = cimax_spi_class_attrs,
};

static int aml_cimax_spi_mod_init(void)
{
	int ret;
	pr_dbg("Amlogic CIMAX SPI Init\n");
	ret = class_register(&cimax_spi_class);
	return 0;
}

static void aml_cimax_spi_mod_exit(void)
{
	pr_dbg("Amlogic CIMAX SPI Exit\n");
	class_unregister(&cimax_spi_class);
}

