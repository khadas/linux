// SPDX-License-Identifier: GPL-2.0-only
/*
 * Elan Microelectronics touch panels with I2C interface
 *
 * Copyright (C) 2014 Elan Microelectronics Corporation.
 * Scott Liu <scott.liu@emc.com.tw>
 *
 * This code is partly based on hid-multitouch.c:
 *
 *  Copyright (c) 2010-2012 Stephane Chatty <chatty@enac.fr>
 *  Copyright (c) 2010-2012 Benjamin Tissoires <benjamin.tissoires@gmail.com>
 *  Copyright (c) 2010-2012 Ecole Nationale de l'Aviation Civile, France
 *
 *
 * This code is partly based on i2c-hid.c:
 *
 * Copyright (c) 2012 Benjamin Tissoires <benjamin.tissoires@gmail.com>
 * Copyright (c) 2012 Ecole Nationale de l'Aviation Civile, France
 * Copyright (c) 2012 Red Hat, Inc
 */

/*
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 */

#include <linux/module.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/platform_device.h>
#include <linux/async.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/buffer_head.h>
#include <linux/slab.h>
#include <linux/firmware.h>
#include <linux/input/mt.h>
#include <linux/acpi.h>
#include <linux/of.h>
#include <linux/gpio/consumer.h>
#include <linux/regulator/consumer.h>
#include <asm/unaligned.h>
#include <linux/miscdevice.h>  //for misc_register
#include <linux/gpio.h>
#include <linux/of_gpio.h>	//for of_get_named_gpio_flags()

/* Device, Driver information */
//#define DEVICE_NAME	"elants_i2c"
#define DEVICE_NAME	"elan_ktf"
#define DRV_VERSION	"0.0.6"

#define ELAN_HID_I2C	/* for ELAN HID over I2C Protocol */
#define ELAN_RAW_DATA   /* for ELAN to read Raw Data */

//#define AUTO_UPDATE

/* Convert from rows or columns into resolution */
#define ELAN_TS_RESOLUTION(n, m)   (((n) - 1) * (m))

/* FW header data */
#ifdef ELAN_HID_I2C
#define HEADER_SIZE		67
#else
#define HEADER_SIZE		4
#endif
#define FW_HDR_TYPE		0
#define FW_HDR_COUNT		1
#define FW_HDR_LENGTH		2

/* Buffer mode Queue Header information */
#define QUEUE_HEADER_SINGLE	0x62
#define QUEUE_HEADER_NORMAL	0X63
#define QUEUE_HEADER_WAIT	0x64

/* Command header definition */
#define CMD_HEADER_WRITE	0x54
#define CMD_HEADER_READ		0x53
#define CMD_HEADER_6B_READ	0x5B
#define CMD_HEADER_RESP		0x52
#define CMD_HEADER_6B_RESP	0x9B
#define CMD_HEADER_HELLO	0x55
#define CMD_HEADER_REK		0x66
#define CMD_HEADER_ROM_READ	0x96
#ifdef ELAN_HID_I2C
#define CMD_HEADER_HID_I2C	0x43
#endif

#define	ELAN_REMARK_ID_OF_NON_REMARK_IC     0xFFFF

/* FW position data */
#ifdef ELAN_HID_I2C
#define PACKET_SIZE		67
#else
#define PACKET_SIZE		55
#endif

#define MAX_CONTACT_NUM		10
#define FW_POS_HEADER		0
#define FW_POS_STATE		1
#define FW_POS_TOTAL		2
#define FW_POS_XY		3
#define FW_POS_CHECKSUM		34
#define FW_POS_WIDTH		35
#define FW_POS_PRESSURE		45

#define HID_I2C_FINGER_HEADER	0x3F
#define HID_I2C_PEN_HEADER		0x13	//0x0D  //0x22
#define HEADER_REPORT_10_FINGER	0x62
#define RSP_LEN               2

/* Header (4 bytes) plus 3 fill 10-finger packets */
#define MAX_PACKET_SIZE		169

#define BOOT_TIME_DELAY_MS	50

/* FW read command, 0x53 0x?? 0x0, 0x01 */
#define E_ELAN_INFO_FW_VER	0x00
#define E_ELAN_INFO_BC_VER	0x10
#define E_ELAN_INFO_TEST_VER	0xE0
#define E_ELAN_INFO_FW_ID	0xF0
#define E_INFO_OSR		0xD6
#define E_INFO_PHY_SCAN		0xD7
#define E_INFO_PHY_DRIVER	0xD8

#define MAX_RETRIES		3
#define MAX_FW_UPDATE_RETRIES	30

#define ELAN_FW_PAGESIZE	132
#define ACK_FAIL 0x00
#define ACK_OK   0xAA
#define ACK_REWRITE 0x55
/* calibration timeout definition */
#define ELAN_CALI_TIMEOUT_MSEC	12000

#define ELAN_POWERON_DELAY_USEC	500
#ifdef ELAN_HID_I2C
#define ELAN_RESET_DELAY_MSEC	300
#else
#define ELAN_RESET_DELAY_MSEC	20
#endif

// For Firmware Update
#ifdef ELAN_RAW_DATA
#define ELAN_IOCTLID	0xD0
#define IOCTL_I2C_SLAVE	_IOW(ELAN_IOCTLID,  1, int)
#define IOCTL_FW_INFO  _IOR(ELAN_IOCTLID, 2, int)
#define IOCTL_MINOR_FW_VER  _IOR(ELAN_IOCTLID, 3, int)
#define IOCTL_RESET  _IOR(ELAN_IOCTLID, 4, int)
#define IOCTL_IAP_MODE_LOCK  _IOR(ELAN_IOCTLID, 5, int)
#define IOCTL_CHECK_RECOVERY_MODE  _IOR(ELAN_IOCTLID, 6, int)
#define IOCTL_FW_VER  _IOR(ELAN_IOCTLID, 7, int)
#define IOCTL_X_RESOLUTION  _IOR(ELAN_IOCTLID, 8, int)
#define IOCTL_Y_RESOLUTION  _IOR(ELAN_IOCTLID, 9, int)
#define IOCTL_FW_ID  _IOR(ELAN_IOCTLID, 10, int)
#define IOCTL_ROUGH_CALIBRATE  _IOR(ELAN_IOCTLID, 11, int)
#define IOCTL_IAP_MODE_UNLOCK  _IOR(ELAN_IOCTLID, 12, int)
#define IOCTL_I2C_INT  _IOR(ELAN_IOCTLID, 13, int)
#define IOCTL_RESUME  _IOR(ELAN_IOCTLID, 14, int)
#define IOCTL_POWER_LOCK  _IOR(ELAN_IOCTLID, 15, int)
#define IOCTL_POWER_UNLOCK  _IOR(ELAN_IOCTLID, 16, int)
#define IOCTL_FW_UPDATE  _IOR(ELAN_IOCTLID, 17, int)
#define IOCTL_BC_VER  _IOR(ELAN_IOCTLID, 18, int)
#define IOCTL_2WIREICE  _IOR(ELAN_IOCTLID, 19, int)
#endif

enum elants_state {
	ELAN_STATE_NORMAL,
	ELAN_WAIT_QUEUE_HEADER,
	ELAN_WAIT_RECALIBRATION,
};

enum elants_iap_mode {
	ELAN_IAP_OPERATIONAL,
	ELAN_IAP_RECOVERY,
};

/* struct elants_data - represents state of Elan touchscreen device */
struct elants_data {
	struct i2c_client *client;
	struct input_dev *input;

	struct regulator *vcc33;
	struct regulator *vccio;
	struct gpio_desc *reset_gpio;
	int intr_gpio;

	u16 fw_version;
	u8 test_version;
	u8 solution_version;
	u8 bc_version;
	u8 iap_version;
	u16 hw_version;
	unsigned int x_res;	/* resolution in units/mm */
	unsigned int y_res;
#ifdef CONFIG_AMLOGIC_MODIFY
	unsigned int x_dis;     /* use to map touch to display */
	unsigned int y_dis;
#endif
	unsigned int x_max;
	unsigned int y_max;
	unsigned int cols;
	unsigned int rows;

	enum elants_state state;
	enum elants_iap_mode iap_mode;

	/* Guards against concurrent access to the device via sysfs */
	struct mutex sysfs_mutex;
	/* Skip CHECK: warning */
	struct mutex irq_mutex;

	u8 cmd_resp[HEADER_SIZE];
	struct completion cmd_done;

#ifdef ELAN_HID_I2C
	u8 buf[PACKET_SIZE * 2];
#else
	u8 buf[MAX_PACKET_SIZE];
#endif

	bool wake_irq_enabled;
	bool keep_power_in_suspend;
	bool unbinding;
	//int hover_flag;
	bool irq_enabled;

#ifdef AUTO_UPDATE
	struct workqueue_struct *elan_ic_update;
	struct delayed_work delay_work;
#endif

#ifdef ELAN_RAW_DATA
	struct miscdevice firmware;
#endif
	/*for irq bottom half*/
	struct workqueue_struct *elan_wq;
	struct work_struct ts_work;

};

#ifdef ELAN_RAW_DATA
static struct elants_data *private_ts;
//int power_lock = 0x00; ERROR:do not initialize globals to 0x00
int power_lock;
#endif

static int elants_i2c_send(struct i2c_client *client,
			   const void *data, size_t size)
{
	int ret;

	ret = i2c_master_send(client, data, size);
	if (ret == size)
		return 0;

	if (ret >= 0)
		ret = -EIO;

	dev_err(&client->dev, "%s failed (%*ph): %d\n", __func__, (int)size,
		data, ret);

	return ret;
}

static int elants_i2c_read(struct i2c_client *client, void *data, size_t size)
{
	int ret;

	ret = i2c_master_recv(client, data, size);
	if (ret == size)
		return 0;

	if (ret >= 0)
		ret = -EIO;

	dev_err(&client->dev, "%s failed: %d\n", __func__, ret);

	return ret;
}

static int elants_i2c_execute_command(struct i2c_client *client,
				      const u8 *cmd, size_t cmd_size,
				      u8 *resp, size_t resp_size)
{
	struct i2c_msg msgs[2];
	int ret;
	u8 expected_response;

	switch (cmd[0]) {
	case CMD_HEADER_READ:  //53
		expected_response = CMD_HEADER_RESP;
		break;

	case CMD_HEADER_6B_READ:  //5B
		expected_response = CMD_HEADER_6B_RESP;
		break;
	case 0x04:  //HID_I2C command header
		expected_response = 0x43;
		break;
	case CMD_HEADER_ROM_READ:  //I2C 96 header
		expected_response = 0x95;
		break;
	default:
		dev_err(&client->dev, "%s: invalid command %*ph\n", __func__,
			(int)cmd_size, cmd);
		return -EINVAL;
	}

	msgs[0].addr = client->addr;
	msgs[0].flags = client->flags & I2C_M_TEN;
	msgs[0].len = cmd_size;
	msgs[0].buf = (u8 *)cmd;

	msgs[1].addr = client->addr;
	msgs[1].flags = client->flags & I2C_M_TEN;
	msgs[1].flags |= I2C_M_RD;
	msgs[1].len = resp_size;
	msgs[1].buf = resp;

	ret = i2c_transfer(client->adapter, msgs, ARRAY_SIZE(msgs));
	if (ret < 0)
		return ret;

	if (ret != ARRAY_SIZE(msgs) || resp[FW_HDR_TYPE] != expected_response)
		return -EIO;

	return 0;
}

static int elan_ts_poll(void)
{
	int status = 0, retry = 500;

	do {
		status = gpio_get_value(private_ts->intr_gpio);
		dev_dbg(&private_ts->client->dev,
			"%s: status = %d\n", __func__, status);
		if (status == 0)
			break;
		retry--;
		//msleep(1);
		usleep_range(1000, 1005);
	} while (status == 1 && retry > 0);

	dev_dbg(&private_ts->client->dev,
		"%s: poll interrupt status %s\n", __func__,
		status == 1 ? "high" : "low");

	return status == 0 ? 0 : -ETIMEDOUT;
}

/*
 * If irq is disabled/enabled, can not disable/enable again
 * disable - status 0; enable - status not 0
 */
static void elan_set_irq_status(int irq, int status)
{
	mutex_lock(&private_ts->irq_mutex);
	if (private_ts->irq_enabled != (!!status)) {
		status ? enable_irq(irq) : disable_irq(irq);
		private_ts->irq_enabled = !!status;
		dev_dbg(&private_ts->client->dev, "%s: %s irq\n", __func__,
			status ? "enable" : "disable");
	}
	mutex_unlock(&private_ts->irq_mutex);
};

static int elants_i2c_calibrate(struct elants_data *ts)
{
	struct i2c_client *client = ts->client;
	int ret, error;
#ifdef ELAN_HID_I2C
	static const u8 w_flashkey[37] = { 0x04, 0x00, 0x23, 0x00, 0x03, 0x00,
					   0x04, 0x54, 0xC0, 0xE1, 0x5A };
	static const u8 rek[37] = { 0x04, 0x00, 0x23, 0x00, 0x03, 0x00, 0x04,
				    0x54, 0x29, 0x00, 0x01 };
	static const u8 rek_resp[] = { 0x43, 0x00, 0x02, 0x04, 0x66, 0x66, 0x66,
				     0x66 };
#else
	static const u8 w_flashkey[] = { 0x54, 0xC0, 0xE1, 0x5A };
	static const u8 rek[] = { 0x54, 0x29, 0x00, 0x01 };
	static const u8 rek_resp[] = { CMD_HEADER_REK, 0x66, 0x66, 0x66 };
#endif

	//disable_irq(client->irq);
	elan_set_irq_status(client->irq, 0);

	ts->state = ELAN_WAIT_RECALIBRATION;
	reinit_completion(&ts->cmd_done);

	error = elants_i2c_send(client, w_flashkey, sizeof(w_flashkey));
	if (error)
		dev_info(&client->dev, "send flah key failed\n");
	else
		dev_info(&client->dev,
			"[elan] flash key cmd = [%2x, %2x, %2x, %2x]\n",
			w_flashkey[7], w_flashkey[8], w_flashkey[9],
			w_flashkey[10]);

	//msleep(5);  // for debug
	usleep_range(5000, 5001);
	error = elants_i2c_send(client, rek, sizeof(rek));
	if (error)
		dev_info(&client->dev, "send calibrate failed\n");
	else
		dev_info(&client->dev,
			"[elan] calibration cmd = [%2x, %2x, %2x, %2x]\n",
			rek[7], rek[8], rek[9], rek[10]);

	//enable_irq(client->irq);
	elan_set_irq_status(client->irq, 1);

	ret = wait_for_completion_interruptible_timeout(&ts->cmd_done,
							msecs_to_jiffies(ELAN_CALI_TIMEOUT_MSEC));

	ts->state = ELAN_STATE_NORMAL;

	if (ret <= 0) {
		error = ret < 0 ? ret : -ETIMEDOUT;
		dev_err(&client->dev,
			"error while waiting for calibration to complete: %d\n",
			error);
		return error;
	}

	if (memcmp(rek_resp, ts->cmd_resp, sizeof(rek_resp))) {
		dev_err(&client->dev, "unexpected calibration response: %*ph\n",
			(int)sizeof(ts->cmd_resp), ts->cmd_resp);
		return -EINVAL;
	}

	return 0;
}

static int elants_i2c_hw_reset(struct i2c_client *client)
{
	struct elants_data *ts = i2c_get_clientdata(client);

	pr_info("[elan] enter %s....\n", __func__);
	gpiod_direction_output(ts->reset_gpio, 0);
	//msleep(1);
	usleep_range(1000, 1001);
	gpiod_direction_output(ts->reset_gpio, 1);
	//msleep(2);
	usleep_range(2000, 2001);
	gpiod_direction_output(ts->reset_gpio, 0);
	//msleep(1);
	usleep_range(1000, 1001);
	return 0;
}

static int elants_i2c_sw_reset(struct i2c_client *client)
{
#ifdef ELAN_HID_I2C
	const u8 soft_rst_cmd[37] = { 0x04, 0x00, 0x23, 0x00, 0x03, 0x00, 0x04,
				      0x77, 0x77, 0x77, 0x77 };
#else
	const u8 soft_rst_cmd[] = { 0x77, 0x77, 0x77, 0x77 };
#endif
	int error;

	error = elants_i2c_send(client, soft_rst_cmd, sizeof(soft_rst_cmd));
	if (error) {
		dev_err(&client->dev, "software reset failed: %d\n", error);
		return error;
	}

	/*
	 * We should wait at least 10 msec (but no more than 40) before
	 * sending fastboot or IAP command to the device.
	 */
	msleep(30);

	return 0;
}

#ifdef ELAN_HID_I2C
int check_isp_status(struct i2c_client *client)
{
	int len;
	int i;
	int err = 0;
	const u8 checkstatus[37] = {0x04, 0x00, 0x23, 0x00, 0x03, 0x18};
	u8 buf[67] = {0};

	len = elants_i2c_send(client, checkstatus, sizeof(checkstatus));
	if (len) {
		dev_err(&client->dev,
			"[elan] %s ERROR: Flash key fail!len=%d\n", __func__,
			len);
		err =  -EINVAL;
		goto err_send_flash_key_cmd_fail;
	} else {
		dev_dbg(&client->dev,
			"[elan] check status cmd = [%x:%x:%x:%x:%x:%x]\n",
			checkstatus[0], checkstatus[1], checkstatus[2],
			checkstatus[3], checkstatus[4], checkstatus[5]);
	}
	//msleep(20);

	len = elants_i2c_read(client, buf, sizeof(buf));
	if (len) {
		dev_err(&client->dev,
			"[elan] ERROR: Check Address Read Data error.len=%d\n",
			len);
		err = -EINVAL;
		goto err_recv_check_addr_fail;
	} else {
		dev_dbg(&client->dev, "[elan][Check status]: ");
		for (i = 0; i < (37 + 3) / 8; i++) {
			dev_dbg(&client->dev,
				"%02x %02x %02x %02x %02x %02x %02x %02x",
				buf[i * 8 + 0], buf[i * 8 + 1], buf[i * 8 + 2],
				buf[i * 8 + 3], buf[i * 8 + 4], buf[i * 8 + 5],
				buf[i * 8 + 6], buf[i * 8 + 7]);
		}
		if (buf[4] == 0x56)
			return 0x56; /* return recovery mode*/
		else if (buf[4] == 0x20)
			return 0x20;
		else
			return -1;
	}

 err_recv_check_addr_fail:
 err_send_flash_key_cmd_fail:
	return err;
}

//HID_I2C
static int elants_i2c_query_hw_version(struct elants_data *ts)
{
	struct i2c_client *client = ts->client;
	int error, retry_cnt;
	const u8 cmd[37] = {0x04, 0x00, 0x23, 0x00, 0x03, 0x00, 0x04, 0x53,
			    0xf0, 0x00, 0x01}; /*Get firmware ID 53 f0 00 01*/
	u8 resp[67] = {0};
	int major, minor;

	for (retry_cnt = 0; retry_cnt < MAX_RETRIES; retry_cnt++) {
		error = elants_i2c_execute_command(client, cmd, sizeof(cmd),
						   resp, sizeof(resp));

		pr_info("[elan] %s: (Firmware ID) %2x:%2x:%2x:%2x:%2x:%2x:%2x:%2x\n",
			__func__, resp[0], resp[1], resp[2], resp[3], resp[4],
			resp[5], resp[6], resp[7]);

		if (!error) {
			//ts->hw_version = elants_i2c_parse_version(resp);
			major = ((resp[5] & 0x0f) << 4) | ((resp[6] & 0xf0) >> 4);
			minor = ((resp[6] & 0x0f) << 4) | ((resp[7] & 0xf0) >> 4);
			ts->hw_version = major << 8 | minor;

			if (ts->hw_version != 0xffff)
				return 0;
		}

		dev_dbg(&client->dev, "read fw id error=%d, buf=%*phC\n",
			error, (int)sizeof(resp), resp);
	}

	if (error) {
		dev_err(&client->dev, "Failed to read fw id: %d\n", error);
		return error;
	}

	dev_err(&client->dev, "Invalid fw id: %#04x\n", ts->hw_version);

	return -EINVAL;
}

//HID_I2C
static int elants_i2c_query_fw_version(struct elants_data *ts)
{
	struct i2c_client *client = ts->client;
	int error, retry_cnt;
	//const u8 cmd[] = { CMD_HEADER_READ, E_ELAN_INFO_FW_VER, 0x00, 0x01 };
	const u8 cmd[37] = {0x04, 0x00, 0x23, 0x00, 0x03, 0x00, 0x04, 0x53,
			    0x00, 0x00, 0x01}; /*Get firmware version 53 00 00 01*/
	u8 resp[67] = {0};
	int major, minor;

	for (retry_cnt = 0; retry_cnt < MAX_RETRIES; retry_cnt++) {
		error = elants_i2c_execute_command(client, cmd, sizeof(cmd),
						   resp, sizeof(resp));

		pr_info("[elan] %s: (Firmware version) %2x:%2x:%2x:%2x:%2x:%2x:%2x:%2x\n",
			__func__, resp[0], resp[1], resp[2], resp[3], resp[4],
			resp[5], resp[6], resp[7]);

		if (!error) {
			//ts->fw_version = elants_i2c_parse_version(resp);
			major = ((resp[5] & 0x0f) << 4) | ((resp[6] & 0xf0) >> 4);
			minor = ((resp[6] & 0x0f) << 4) | ((resp[7] & 0xf0) >> 4);
			ts->fw_version = major << 8 | minor;

			if (ts->fw_version != 0x0000 &&
			    ts->fw_version != 0xffff)
				return 0;
		}

		dev_dbg(&client->dev, "read fw version error=%d, buf=%*phC\n",
			error, (int)sizeof(resp), resp);
	}

	dev_err(&client->dev,
		"Failed to read fw version or fw version is invalid\n");

	return -EINVAL;
}

//HID_I2C
static int elants_i2c_query_test_version(struct elants_data *ts)
{
	struct i2c_client *client = ts->client;
	int error, retry_cnt;
	u16 version;
	//const u8 cmd[] = { CMD_HEADER_READ, E_ELAN_INFO_TEST_VER, 0x00, 0x01 };
	const u8 cmd[37] = {0x04, 0x00, 0x23, 0x00, 0x03, 0x00, 0x04, 0x53,
			    0xe0, 0x00, 0x01}; /*Get test version 53 e0 00 01*/
	u8 resp[67] = {0};
	int major, minor;

	for (retry_cnt = 0; retry_cnt < MAX_RETRIES; retry_cnt++) {
		error = elants_i2c_execute_command(client, cmd, sizeof(cmd),
						   resp, sizeof(resp));
		pr_info("[elan] %s: (test version) %2x:%2x:%2x:%2x:%2x:%2x:%2x:%2x\n",
			__func__, resp[0], resp[1], resp[2], resp[3], resp[4],
			resp[5], resp[6], resp[7]);

		if (!error) {
			//version = elants_i2c_parse_version(resp);
			major = ((resp[5] & 0x0f) << 4) | ((resp[6] & 0xf0) >> 4);
			minor = ((resp[6] & 0x0f) << 4) | ((resp[7] & 0xf0) >> 4);
			version = major << 8 | minor;

			ts->test_version = version >> 8;
			ts->solution_version = version & 0xff;

			return 0;
		}

		dev_dbg(&client->dev,
			"read test version error rc=%d, buf=%*phC\n",
			error, (int)sizeof(resp), resp);
	}

	dev_err(&client->dev, "Failed to read test version\n");

	return -EINVAL;
}

//HID_I2C
static int elants_i2c_query_bc_version(struct elants_data *ts)
{
	struct i2c_client *client = ts->client;
	//const u8 cmd[] = { CMD_HEADER_READ, E_ELAN_INFO_BC_VER, 0x00, 0x01 };
	const u8 cmd[37] = {0x04, 0x00, 0x23, 0x00, 0x03, 0x00, 0x04, 0x53,
			    0x10, 0x00, 0x01}; /*Get test version 53 10 00 01*/
	u8 resp[67] = {0};
	int major, minor;
	u16 version;
	int error;

	error = elants_i2c_execute_command(client, cmd, sizeof(cmd), resp,
					   sizeof(resp));
	pr_info("[elan] %s: (BC version) %2x:%2x:%2x:%2x:%2x:%2x:%2x:%2x\n",
		__func__, resp[0], resp[1], resp[2], resp[3], resp[4], resp[5],
		resp[6], resp[7]);

	if (error) {
		dev_err(&client->dev, "read BC version error=%d, buf=%*phC\n",
			error, (int)sizeof(resp), resp);
		return error;
	}

	//version = elants_i2c_parse_version(resp);
	major = ((resp[5] & 0x0f) << 4) | ((resp[6] & 0xf0) >> 4);
	minor = ((resp[6] & 0x0f) << 4) | ((resp[7] & 0xf0) >> 4);
	version = major << 8 | minor;

	ts->bc_version = version >> 8;
	ts->iap_version = version & 0xff;

	return 0;
}

//HID_I2C
static int elants_i2c_query_ts_info(struct elants_data *ts)
{
	struct i2c_client *client = ts->client;
	int error;
	//u8 resp[17];
	u8 resp[67];
	u16 phy_x, phy_y, rows, cols, osr;
	/*
	 *const u8 get_resolution_cmd[] = {
	 *CMD_HEADER_6B_READ, 0x00, 0x00, 0x00, 0x00, 0x00
	 *};
	 *const u8 get_osr_cmd[] = {
	 *CMD_HEADER_READ, E_INFO_OSR, 0x00, 0x01
	 *};
	 *const u8 get_physical_scan_cmd[] = {
	 *CMD_HEADER_READ, E_INFO_PHY_SCAN, 0x00, 0x01
	 *};
	 *const u8 get_physical_drive_cmd[] = {
	 *CMD_HEADER_READ, E_INFO_PHY_DRIVER, 0x00, 0x01
	 *};
	 */
	const u8 get_resolution_cmd[37] = { 0x04, 0x00, 0x23, 0x00, 0x03, 0x00,
					   0x06, CMD_HEADER_6B_READ, 0x00, 0x00,
					   0x00, 0x00, 0x00
	};
	const u8 get_osr_cmd[37] = { 0x04, 0x00, 0x23, 0x00, 0x03, 0x00, 0x04,
				     CMD_HEADER_READ, E_INFO_OSR, 0x00, 0x01
	};
	const u8 get_physical_scan_cmd[37] = { 0x04, 0x00, 0x23, 0x00, 0x03,
					       0x00, 0x04,
					       CMD_HEADER_READ, E_INFO_PHY_SCAN,
					       0x00, 0x01
	};
	const u8 get_physical_drive_cmd[37] = { 0x04, 0x00, 0x23, 0x00, 0x03,
						0x00, 0x04, CMD_HEADER_READ,
						E_INFO_PHY_DRIVER, 0x00, 0x01
	};

	/* Get trace number */
	error = elants_i2c_execute_command(client, get_resolution_cmd,
					   sizeof(get_resolution_cmd), resp,
					   sizeof(resp));
	if (error) {
		dev_err(&client->dev, "get resolution command failed: %d\n",
			error);
		return error;
	}
	pr_info("[elan] %s: (get resolution) %2x:%2x:%2x:%2x:%2x:%2x:%2x:%2x\n",
		__func__, resp[0], resp[1], resp[2], resp[3], resp[4], resp[5],
		resp[6], resp[7]);
	/*
	 *rows = resp[2] + resp[6] + resp[10];
	 *cols = resp[3] + resp[7] + resp[11];
	 */
	rows = resp[6] + resp[10] + resp[14];
	cols = resp[7] + resp[11] + resp[15];

	ts->cols = cols;
	ts->rows = rows;

	/* Process mm_to_pixel information */
	error = elants_i2c_execute_command(client, get_osr_cmd,
					   sizeof(get_osr_cmd),
					   resp, sizeof(resp));
	if (error) {
		dev_err(&client->dev, "get osr command failed: %d\n", error);
		return error;
	}
	pr_info("[elan] %s: (get osr) %2x:%2x:%2x:%2x:%2x:%2x:%2x:%2x\n",
		__func__, resp[0], resp[1], resp[2], resp[3], resp[4], resp[5],
		resp[6], resp[7]);
	//osr = resp[3];
	osr = resp[7];

	error = elants_i2c_execute_command(client, get_physical_scan_cmd,
					   sizeof(get_physical_scan_cmd),
					   resp, sizeof(resp));
	if (error) {
		dev_err(&client->dev, "get physical scan command failed: %d\n",
			error);
		return error;
	}
	pr_info("[elan] %s:(get physical scan)%2x:%2x:%2x:%2x:%2x:%2x:%2x:%2x\n"
		, __func__, resp[0], resp[1], resp[2], resp[3], resp[4], resp[5],
		resp[6], resp[7]);

	//phy_x = get_unaligned_be16(&resp[2]);
	phy_x = get_unaligned_be16(&resp[6]);

	error = elants_i2c_execute_command(client, get_physical_drive_cmd,
					   sizeof(get_physical_drive_cmd),
					   resp, sizeof(resp));
	if (error) {
		dev_err(&client->dev, "get physical drive command failed: %d\n",
			error);
		return error;
	}

	//phy_y = get_unaligned_be16(&resp[2]);
	phy_y = get_unaligned_be16(&resp[6]);

	dev_dbg(&client->dev, "phy_x=%d, phy_y=%d\n", phy_x, phy_y);

	if (rows == 0 || cols == 0 || osr == 0) {
		dev_warn(&client->dev, "invalid trace number data: %d, %d, %d\n"
			 , rows, cols, osr);
	} else {
		/* translate trace number to TS resolution */
		ts->x_max = ELAN_TS_RESOLUTION(rows, osr);
		ts->x_res = DIV_ROUND_CLOSEST(ts->x_max, phy_x);
		ts->y_max = ELAN_TS_RESOLUTION(cols, osr);
		ts->y_res = DIV_ROUND_CLOSEST(ts->y_max, phy_y);
	}

	pr_info("[elan] %s: ts->cols = %d, ts->rows = %d, x_max = %d,x_res=%d, y_max=%d,y_res=%d\n"
		, __func__, ts->cols, ts->rows, ts->x_max,
	       ts->x_res, ts->y_max, ts->y_res);
	return 0;
}

#else
//for normal I2C
static u16 elants_i2c_parse_version(u8 *buf)
{
	return get_unaligned_be32(buf) >> 4;
}

static int elants_i2c_query_hw_version(struct elants_data *ts)
{
	struct i2c_client *client = ts->client;
	int error, retry_cnt;
	const u8 cmd[] = { CMD_HEADER_READ, E_ELAN_INFO_FW_ID, 0x00, 0x01 };
	u8 resp[HEADER_SIZE];

	for (retry_cnt = 0; retry_cnt < MAX_RETRIES; retry_cnt++) {
		error = elants_i2c_execute_command(client, cmd, sizeof(cmd),
						   resp, sizeof(resp));
		if (!error) {
			ts->hw_version = elants_i2c_parse_version(resp);
			if (ts->hw_version != 0xffff)
				return 0;
		}

		dev_dbg(&client->dev, "read fw id error=%d, buf=%*phC\n", error,
			(int)sizeof(resp), resp);
	}

	if (error) {
		dev_err(&client->dev, "Failed to read fw id: %d\n", error);
		return error;
	}

	dev_err(&client->dev, "Invalid fw id: %#04x\n", ts->hw_version);

	return -EINVAL;
}

static int elants_i2c_query_fw_version(struct elants_data *ts)
{
	struct i2c_client *client = ts->client;
	int error, retry_cnt;
	const u8 cmd[] = { CMD_HEADER_READ, E_ELAN_INFO_FW_VER, 0x00, 0x01 };
	u8 resp[HEADER_SIZE];

	for (retry_cnt = 0; retry_cnt < MAX_RETRIES; retry_cnt++) {
		error = elants_i2c_execute_command(client, cmd, sizeof(cmd),
						   resp, sizeof(resp));
		if (!error) {
			ts->fw_version = elants_i2c_parse_version(resp);
			if (ts->fw_version != 0x0000 &&
			    ts->fw_version != 0xffff)
				return 0;
		}

		dev_dbg(&client->dev, "read fw version error=%d, buf=%*phC\n",
			error, (int)sizeof(resp), resp);
	}

	dev_err(&client->dev,
		"Failed to read fw version or fw version is invalid\n");

	return -EINVAL;
}

static int elants_i2c_query_test_version(struct elants_data *ts)
{
	struct i2c_client *client = ts->client;
	int error, retry_cnt;
	u16 version;
	const u8 cmd[] = { CMD_HEADER_READ, E_ELAN_INFO_TEST_VER, 0x00, 0x01 };
	u8 resp[HEADER_SIZE];

	for (retry_cnt = 0; retry_cnt < MAX_RETRIES; retry_cnt++) {
		error = elants_i2c_execute_command(client, cmd, sizeof(cmd),
						   resp, sizeof(resp));
		if (!error) {
			version = elants_i2c_parse_version(resp);
			ts->test_version = version >> 8;
			ts->solution_version = version & 0xff;

			return 0;
		}

		dev_dbg(&client->dev,
			"read test version error rc=%d, buf=%*phC\n",
			error, (int)sizeof(resp), resp);
	}

	dev_err(&client->dev, "Failed to read test version\n");

	return -EINVAL;
}

static int elants_i2c_query_bc_version(struct elants_data *ts)
{
	struct i2c_client *client = ts->client;
	const u8 cmd[] = { CMD_HEADER_READ, E_ELAN_INFO_BC_VER, 0x00, 0x01 };
	u8 resp[HEADER_SIZE];
	u16 version;
	int error;

	error = elants_i2c_execute_command(client, cmd, sizeof(cmd), resp,
					   sizeof(resp));
	if (error) {
		dev_err(&client->dev,
			"read BC version error=%d, buf=%*phC\n",
			error, (int)sizeof(resp), resp);
		return error;
	}

	version = elants_i2c_parse_version(resp);
	ts->bc_version = version >> 8;
	ts->iap_version = version & 0xff;

	return 0;
}

static int elants_i2c_query_ts_info(struct elants_data *ts)
{
	struct i2c_client *client = ts->client;
	int error;
	u8 resp[17];
	u16 phy_x, phy_y, rows, cols, osr;
	const u8 get_resolution_cmd[] = {
		CMD_HEADER_6B_READ, 0x00, 0x00, 0x00, 0x00, 0x00
	};
	const u8 get_osr_cmd[] = {
		CMD_HEADER_READ, E_INFO_OSR, 0x00, 0x01
	};
	const u8 get_physical_scan_cmd[] = {
		CMD_HEADER_READ, E_INFO_PHY_SCAN, 0x00, 0x01
	};
	const u8 get_physical_drive_cmd[] = {
		CMD_HEADER_READ, E_INFO_PHY_DRIVER, 0x00, 0x01
	};

	/* Get trace number */
	error = elants_i2c_execute_command(client,
					   get_resolution_cmd,
					   sizeof(get_resolution_cmd),
					   resp, sizeof(resp));
	if (error) {
		dev_err(&client->dev, "get resolution command failed: %d\n",
			error);
		return error;
	}

	rows = resp[2] + resp[6] + resp[10];
	cols = resp[3] + resp[7] + resp[11];

	/* Process mm_to_pixel information */
	error = elants_i2c_execute_command(client,
					   get_osr_cmd, sizeof(get_osr_cmd),
					   resp, sizeof(resp));
	if (error) {
		dev_err(&client->dev, "get osr command failed: %d\n", error);
		return error;
	}

	osr = resp[3];

	error = elants_i2c_execute_command(client, get_physical_scan_cmd,
					   sizeof(get_physical_scan_cmd),
					   resp, sizeof(resp));
	if (error) {
		dev_err(&client->dev, "get physical scan command failed: %d\n",
			error);
		return error;
	}

	phy_x = get_unaligned_be16(&resp[2]);

	error = elants_i2c_execute_command(client, get_physical_drive_cmd,
					   sizeof(get_physical_drive_cmd),
					   resp, sizeof(resp));
	if (error) {
		dev_err(&client->dev, "get physical drive command failed: %d\n",
			error);
		return error;
	}

	phy_y = get_unaligned_be16(&resp[2]);

	dev_dbg(&client->dev, "phy_x=%d, phy_y=%d\n", phy_x, phy_y);

	if (rows == 0 || cols == 0 || osr == 0) {
		dev_warn(&client->dev,
			 "invalid trace number data: %d, %d, %d\n", rows, cols,
			 osr);
	} else {
		/* translate trace number to TS resolution */
		ts->x_max = ELAN_TS_RESOLUTION(rows, osr);
		ts->x_res = DIV_ROUND_CLOSEST(ts->x_max, phy_x);
		ts->y_max = ELAN_TS_RESOLUTION(cols, osr);
		ts->y_res = DIV_ROUND_CLOSEST(ts->y_max, phy_y);
	}

	return 0;
}

static int elants_i2c_fastboot(struct i2c_client *client)
{
#ifdef ELAN_HID_I2C
	const u8 boot_cmd[37] = { 0x04, 0x00, 0x23, 0x00, 0x03, 0x00, 0x04,
				  0x4D, 0x61, 0x69, 0x6E };
#else
	const u8 boot_cmd[] = { 0x4D, 0x61, 0x69, 0x6E };
#endif
	int error;

	error = elants_i2c_send(client, boot_cmd, sizeof(boot_cmd));
	if (error) {
		dev_err(&client->dev, "boot failed: %d\n", error);
		return error;
	}

	dev_dbg(&client->dev, "boot success -- 0x%x\n", client->addr);
	return 0;
}
#endif

static int elants_i2c_initialize(struct elants_data *ts)
{
	struct i2c_client *client = ts->client;
#ifdef ELAN_HID_I2C
	int error = 0, error2;
#else
	int error, error2, retry_cnt;
	const u8 hello_packet[] = { 0x55, 0x55, 0x55, 0x55 };
	const u8 recov_packet[] = { 0x55, 0x55, 0x80, 0x80 };
	u8 buf[HEADER_SIZE];
#endif
	pr_info("[elan] enter %s....\n", __func__);

#ifdef ELAN_HID_I2C
	error2 = check_isp_status(client);
	if (error2 == 0x56) {
		dev_err(&client->dev, "boot failed, in recovery mode: 0x%x\n",
			error2);
		//goto recovery_mode_fail;
	} else if (error2 < 0) {
		dev_err(&client->dev, "boot failed, i2c fail: %d\n", error2);
		goto i2c_fail;
	}
#else //Normal I2C
	for (retry_cnt = 0; retry_cnt < MAX_RETRIES; retry_cnt++) {
		error = elants_i2c_sw_reset(client);
		if (error) {
			/* Continue initializing if it's the last try */
			if (retry_cnt < MAX_RETRIES - 1)
				continue;
		}

		error = elants_i2c_fastboot(client);
		if (error) {
			/* Continue initializing if it's the last try */
			if (retry_cnt < MAX_RETRIES - 1)
				continue;
		}

		/*HID I2C doesn't have hello packet*/
		/* Wait for Hello packet */
		msleep(BOOT_TIME_DELAY_MS);

		error = elants_i2c_read(client, buf, sizeof(buf));

		/* WARNING: else is not generally useful after a break or return
		 * use to goto to skip
		 */
		if (error) {
			dev_err(&client->dev,
				"failed to read 'hello' packet: %d\n", error);
		} else if (!memcmp(buf, hello_packet, sizeof(hello_packet))) {
			ts->iap_mode = ELAN_IAP_OPERATIONAL;
			//break;
			goto out_of_for;
		} else if (!memcmp(buf, recov_packet, sizeof(recov_packet))) {
			/*
			 * Setting error code will mark device
			 * in recovery mode below.
			 */
			error = -EIO;
			//break;
			goto out_of_for;
		} else {
			error = -EINVAL;
			dev_err(&client->dev,
				"invalid 'hello' packet: %*ph\n",
				(int)sizeof(buf), buf);
		}
	}
out_of_for:
#endif
	/* hw version is available even if device in recovery state */
	error2 = elants_i2c_query_hw_version(ts);
	if (!error2)
		error2 = elants_i2c_query_bc_version(ts);
	if (!error)
		error = error2;

	if (!error)
		error = elants_i2c_query_fw_version(ts);
	if (!error)
		error = elants_i2c_query_test_version(ts);
	if (!error)
		error = elants_i2c_query_ts_info(ts);

	if (error)
		ts->iap_mode = ELAN_IAP_RECOVERY;

#ifdef ELAN_HID_I2C
//recovery_mode_fail:
i2c_fail:
#endif
	return 0;
}

#ifdef ELAN_HID_I2C
/*
 * Firmware update interface.
 */
int hid_recovery_isp(struct i2c_client *client)
{
	int len;
	int i;
	const u8 flash_key[37] = {0x04, 0x00, 0x23, 0x00, 0x03, 0x00,
				  0x04, 0x54, 0xc0, 0xe1, 0x5a};
	const u8 check_addr[37] = {0x04, 0x00, 0x23,	0x00, 0x03,
				   0x00, 0x01, 0x10};
	u8 buf[67] = {0};

	len = elants_i2c_send(client, flash_key, sizeof(flash_key));
	if (len) {
		dev_err(&client->dev,
			"[elan] %s ERROR: Flash key fail!len=%d\n", __func__,
			len);
		return -1;
	}

	dev_dbg(&client->dev,
		"[elan] FLASH key cmd = [%2x:%2x:%2x:%2x]\n",
		flash_key[7], flash_key[8],
		flash_key[9], flash_key[10]);

	//mdelay(40);

	len = elants_i2c_send(client, check_addr, sizeof(check_addr));
	if (len) {
		dev_err(&client->dev,
			"[elan] ERROR: Check Address fail!len=%d\n", len);
		return -1;
	}

	dev_dbg(&client->dev,
		"[elan] Check Address cmd = [%2x:%2x:%2x:%2x]\n",
		check_addr[7], check_addr[8],
		check_addr[9], check_addr[10]);
	//mdelay(20);
	len = elants_i2c_read(client, buf, sizeof(buf));
	if (len) {
		dev_dbg(&client->dev,
			"[elan] ERROR: Check Address Read Data error.len=%d\n",
			len);
		return -1;
	}

	dev_dbg(&client->dev, "[elan][Check Addr]: ");

	for (i = 0; i < (37 + 3) / 8; i++) {
		dev_dbg(&client->dev,
			"%02x %02x %02x %02x %02x %02x %02x %02x",
			buf[i * 8 + 0], buf[i * 8 + 1], buf[i * 8 + 2],
			buf[i * 8 + 3], buf[i * 8 + 4], buf[i * 8 + 5],
			buf[i * 8 + 6], buf[i * 8 + 7]);
	}
	return 0;
}

int hid_enter_isp_mode(struct i2c_client *client)
{
	int len;
	int i;
	const u8 flash_key[37]  = {0x04, 0x00, 0x23, 0x00, 0x03, 0x00,
				   0x04, 0x54, 0xc0, 0xe1, 0x5a};
	const u8 isp_cmd[37] = {0x04, 0x00, 0x23, 0x00, 0x03, 0x00,
				0x04, 0x54, 0x00, 0x12, 0x34};
	const u8 check_addr[37] = {0x04, 0x00, 0x23, 0x00, 0x03,
				   0x00, 0x01, 0x10};
	u8 buf[67] = {0};

	len = elants_i2c_send(client, flash_key, sizeof(flash_key));
	if (len) {
		dev_err(&client->dev,
			"[elan] %s ERROR: Flash key fail!len=%d\r\n",
			__func__, len);
		return -1;
	}

	dev_info(&client->dev,
		 "[elan] FLASH key cmd = [%2x, %2x, %2x, %2x]\n",
		 flash_key[7], flash_key[8],
		 flash_key[9], flash_key[10]);
	//mdelay(20);

	len = elants_i2c_send(client, isp_cmd, sizeof(isp_cmd));
	if (len) {
		dev_err(&client->dev,
			"[elan] ERROR: EnterISPMode fail!len=%d\r\n", len);
		return -1;
	}

	dev_info(&client->dev,
		 "[elan] IAPMode data cmd = [%2x, %2x, %2x, %2x]\n",
		 isp_cmd[7], isp_cmd[8],
		 isp_cmd[9], isp_cmd[10]);

	//mdelay(20);
	len = elants_i2c_send(client, check_addr, sizeof(check_addr));
	if (len) {
		dev_err(&client->dev,
				"[elan] ERROR: Check Address fail!len=%d\r\n",
				len);
		return -1;
	}

	dev_info(&client->dev,
		 "[elan] Check Address cmd = [%2x, %2x, %2x, %2x]\n",
		 check_addr[7], check_addr[8],
		 check_addr[9], check_addr[10]);

	//mdelay(20);
	len = elants_i2c_read(client, buf, sizeof(buf));
	if (len) {
		dev_err(&client->dev,
			"[elan] ERROR: Check Address Read Data error.len=%d\n",
			len);
		return -1;
	}

	dev_info(&client->dev, "[Check Addr]: ");
	for (i = 0; i < (37 + 3) / 8; i++)
		dev_info(&client->dev,
			 "%02x %02x %02x %02x %02x %02x %02x %02x",
			 buf[i * 8 + 0], buf[i * 8 + 1], buf[i * 8 + 2],
			 buf[i * 8 + 3], buf[i * 8 + 4], buf[i * 8 + 5],
			 buf[i * 8 + 6], buf[i * 8 + 7]);

	return 0;
}

int write_page(struct i2c_client *client, u8 *page_size, int byte, int which)
{
	int len;

	len = elants_i2c_send(client, page_size,  byte);
	if (len) {
		dev_err(&client->dev,
			"[elan] ERROR: write the %d th page error,len=%d\n",
			which, len);
		return -1;
	}

	return 0;
}

int get_ack_data_hid(struct i2c_client *client)
{
	int len;
	u8 ack_buf[67] = { 0 };

	len = elan_ts_poll();
	if (len) {
		dev_err(&client->dev, "get int failed\n");
		return ACK_FAIL;
	}
	//mdelay(100);
	len = elants_i2c_read(client, ack_buf, 67);

	dev_info(&client->dev, "[elan] %s: %x,%x\n", __func__, ack_buf[4],
		 ack_buf[5]);

	if (ack_buf[4] == 0xaa && ack_buf[5] == 0xaa)
		return ACK_OK;
	else if (ack_buf[4] == 0x55 && ack_buf[5] == 0x55)
		return ACK_REWRITE;
	else
		return ACK_FAIL;
}

int send_end_cmd(struct i2c_client *client)
{
	int len;
	const u8 send_cmd[37] = {0x04, 0x00, 0x23, 0x00, 0x03, 0x1A};

	len = elants_i2c_send(client, send_cmd, sizeof(send_cmd));
	if (len) {
		dev_err(&client->dev,
			"[elan] ERROR: Send Cmd fail!len=%d\r\n", len);
		return -1;
	}

	dev_info(&client->dev,
		 "[elan] check send cmd = [%x, %x, %x, %x, %x, %x]\n",
		 send_cmd[0], send_cmd[1], send_cmd[2],
		 send_cmd[3], send_cmd[4], send_cmd[5]);
	return 0;
}

static int elants_i2c_validate_remark_id_HID_I2C(struct elants_data *ts,
					 /*const struct firmware *fw*/
						 const u8 *fw_data, int fw_size)
{
	struct i2c_client *client = ts->client;
	int error = 0;
	const u8 cmd[37] = { 0x04, 0x00, 0x23, 0x00, 0x03, 0x00, 0x06,
	CMD_HEADER_ROM_READ, 0x80, 0x1F, 0x00, 0x00, 0x21 };
	u8 resp[67] = { 0 };
	u16 ts_remark_id = 0;
	u16 fw_remark_id = 0;
	/* Compare TS Remark ID and FW Remark ID */
	error = elants_i2c_execute_command(client, cmd, sizeof(cmd),
					resp, sizeof(resp));
	if (error) {
		dev_err(&client->dev, "failed to query Remark ID: %d\n", error);
		return error;
	}
	ts_remark_id = get_unaligned_be16(&resp[7]);
	fw_remark_id = get_unaligned_le16(&fw_data[fw_size - 4]);

	dev_info(&client->dev,
			"[elan] ts_remark_id=0x%04x, fw_remark_id=0x%04x.\n",
			ts_remark_id, fw_remark_id);
	/* WARNING: else is not generally useful after a break or return
	 * change below logic
	 */
	if (ts_remark_id == ELAN_REMARK_ID_OF_NON_REMARK_IC) {
		dev_info(&client->dev, "[ELAN] Pass, Non remark IC.\n");
	} else if (fw_remark_id != ts_remark_id) {
		dev_err(&client->dev, "RemarkIDMismatched:ts_remark_id=0x%04x,fw_remark_id=0x%04x.\n",
			ts_remark_id, fw_remark_id);
		error = -EINVAL;
	} else {
		dev_info(&client->dev, "[ELAN] Remark ID matched.\n");
	}
	return error;
}

static int elants_i2c_do_update_firmware_HID_I2C(struct i2c_client *client,
						 const struct firmware *fw)
{
	struct elants_data *ts = i2c_get_clientdata(client);
	int res = 0;
	int ipage = 0;
	int j = 0;
	int write_times = 142;
	//u8 data;
	int byte_count;
	const u8 *szbuff;
	const u8 *fw_data;
	//const struct firmware *p_fw_entry;
	u8 write_buf[37] = {0x04, 0x00, 0x23, 0x00, 0x03,
			    0x21, 0x00, 0x00, 0x1c};
	u8 cmd_iap_write[37] = {0x04, 0x00, 0x23, 0x00, 0x03, 0x22};
	//const u8 cmd_idel[37] = {0x04, 0x00, 0x23, 0x00, 0x03,
    //                         0x00, 0x04, 0x54, 0x2c, 0x03, 0x01};
	int cur_index = 0;
	int offset = 0;
	int retry_num = 0;
	int fw_size;
    //	char *fw_name;
	//char fw_local_path[50];
	int PageNum = 0;
	int flash_page_num = 30;
	int remain_page;
	int g;
	int PageSize = 132;
	int payload_len = 28;
	int payload_len_remain;
	int payload_len_remain_last;
	int error;
	bool check_remark_id = /*true*/ts->iap_version >= 0x60;

	dev_dbg(&ts->client->dev, "[elan] enter %s.\n", __func__);

	fw_data = fw->data;
	fw_size = fw->size;
	PageNum = fw_size / 132;

	//disable_irq(client->irq);  //disable outside
	//elan_set_irq_status(client->irq, 0);

	remain_page = PageNum % flash_page_num;
	g = PageNum - remain_page + 1;
	payload_len_remain = (flash_page_num * PageSize) % payload_len;
	payload_len_remain_last = (remain_page * PageSize) % payload_len;
	dev_info(&ts->client->dev,
		 "[ELAN] PageNum=%d, flash_page_num=%d, remain_page=%d, g=%d\n",
		 PageNum, flash_page_num, remain_page, g);
	dev_info(&ts->client->dev,
		 "[ELAN] PageSize=%d,payload_len=%d,payload_len_remain=%d,payload_len_remain_last=%d\n",
		 PageSize, payload_len, payload_len_remain, payload_len_remain_last);

 IAP_RESTART:
	//elants_i2c_sw_reset(client);
	//elants_i2c_hw_reset(client);
	//msleep(200);
	res = check_isp_status(ts->client);
	dev_info(&ts->client->dev,
		 "[elan] res: 0x%x\n", res);

	//
	if (check_remark_id) {
		error = elants_i2c_validate_remark_id_HID_I2C(ts, fw_data,
							      fw_size);
		if (error)
			return error;
	}

	if (res == 0x56) { /* 0xa6 recovery mode  */
		//elants_i2c_sw_reset(client);
		//elants_i2c_hw_reset(client);
		//mdelay(200);
		dev_info(&ts->client->dev, "[elan hid iap] Recovery mode\n");
		res = hid_recovery_isp(ts->client);
	} else if (res == 0x20) {
		dev_info(&ts->client->dev, "[elan hid iap] Normal mode\n");
		res = hid_enter_isp_mode(ts->client);/*enter HID ISP mode*/
	} else {
		dev_info(&ts->client->dev,
			 "[elan hid iap] check_isp_status fail\n");
	}

	//mdelay(1);

	for (ipage = 1; ipage <= PageNum; ipage += flash_page_num) {
		offset = 0;
		if (ipage == g)
			write_times = (remain_page * PageSize / payload_len) + 1;
		else
			write_times = (flash_page_num * PageSize / payload_len) + 1;
			/*30*132=3960, 3960=141*28+12*/

		//mdelay(5);
		for (byte_count = 1; byte_count <= write_times; byte_count++) {
			//mdelay(1);
			if (byte_count != write_times) {
				/* header + data = 9+28=37 bytes*/
				szbuff = fw_data + cur_index;
				/*payload length = 0x1c => 28??*/
				write_buf[8] = payload_len;
				write_buf[7] = offset & 0x00ff;/*===>0?*/
				write_buf[6] = offset >> 8;/*==0?*/
				offset += payload_len;
				cur_index =  cur_index + payload_len;
				for (j = 0; j < payload_len; j++)
					write_buf[j + 9] = szbuff[j];
				res = write_page(ts->client, write_buf, 37,
						ipage);
			} else if ((ipage == g) &&
				    (byte_count == write_times)) {
				/*the final page, header + data = 9+20=29 bytes,
				 *the rest of bytes are the previous page's data
				 */
				dev_dbg(&ts->client->dev,
					"[elan] Final Page...\n");
				szbuff = fw_data + cur_index;
				/*payload length = 0x14 => 20*/
				write_buf[8] = payload_len_remain_last;
				write_buf[7] = offset & 0x00ff;
				write_buf[6] = offset >> 8;
				cur_index =  cur_index + payload_len_remain_last;
				for (j = 0; j < payload_len_remain_last; j++)
					write_buf[j + 9] = szbuff[j];
				res = write_page(ts->client, write_buf, 37,
						ipage);
			} else {
				/*last run of this 30 page,
				 *header + data = 9+12=21 bytes,
				 *the rest of bytes are the previous page's data
				 */
				szbuff = fw_data + cur_index;
				/*payload length = 0x0c => 12*/
				write_buf[8] = payload_len_remain;
				write_buf[7] = offset & 0x00ff;
				write_buf[6] = offset >> 8;
				cur_index =  cur_index + payload_len_remain;
				for (j = 0; j < payload_len_remain; j++)
					write_buf[j + 9] = szbuff[j];
				res = write_page(ts->client, write_buf, 37,
						ipage);
			}
			//dev_info(&ts->client->dev, "IAP write_buf=%*phC\n",
			//(int)sizeof(write_buf), write_buf);
		} /*end of for(byte_count=1;byte_count<=17;byte_count++)*/

		//mdelay(200);
		res = write_page(ts->client, cmd_iap_write, 37, ipage);
		//mdelay(200);
		dev_dbg(&ts->client->dev, "[iap] ipage=%d :", ipage);
		res = get_ack_data_hid(ts->client);
		if (res != ACK_OK) {
			if (retry_num < 1) {
				dev_err(&ts->client->dev,
					"[elan] Update Firmware retry_num=%d fail!!\n", retry_num);
				retry_num++;
				cur_index = 0;
				goto IAP_RESTART;
			} else {
				dev_err(&ts->client->dev, "[elan] Update Firmware retry_num=%d!!\n",
					retry_num);
				goto IAP_END;
			}
		}
		//mdelay(1);
	} /*end of for(ipage = 1; ipage <= PageNum; ipage++)*/

IAP_END:

	res = send_end_cmd(ts->client);
	//mdelay(200);
	//elants_i2c_sw_reset(client);
	elants_i2c_hw_reset(client);
	mdelay(200);
	//elants_i2c_send(ts->client, cmd_idel, 37);
	dev_dbg(&ts->client->dev, "[elan] Update Firmware successfully!\n");
	//enable_irq(client->irq);   //enable outside
	//elan_set_irq_status(client->irq, 1);

	return 0;
}

#else	//for normal I2C
/*
 * Firmware update interface.
 */
static int elants_i2c_fw_write_page(struct i2c_client *client,
				    const void *page)
{
	const u8 ack_ok[] = { 0xaa, 0xaa };
	u8 buf[2];
	int retry;
	int error;

	for (retry = 0; retry < MAX_FW_UPDATE_RETRIES; retry++) {
		error = elants_i2c_send(client, page, ELAN_FW_PAGESIZE);
		if (error) {
			dev_err(&client->dev,
				"IAP Write Page failed: %d\n", error);
			continue;
		}

		error = elants_i2c_read(client, buf, RSP_LEN);
		if (error) {
			dev_err(&client->dev,
				"IAP Ack read failed: %d\n", error);
			return error;
		}

		if (!memcmp(buf, ack_ok, sizeof(ack_ok)))
			return 0;

		error = -EIO;
		dev_err(&client->dev,
			"IAP Get Ack Error [%02x:%02x]\n",
			buf[0], buf[1]);
	}

	return error;
}

static int elants_i2c_validate_remark_id(struct elants_data *ts,
					 /*const struct firmware *fw*/
					 const u8 *fw_data, int fw_size)
{
	struct i2c_client *client = ts->client;
	int error = 0;
	const u8 cmd[] = { CMD_HEADER_ROM_READ, 0x80, 0x1F, 0x00, 0x00, 0x21 };
	u8 resp[6] = { 0 };
	u16 ts_remark_id = 0;
	u16 fw_remark_id = 0;
	/* Compare TS Remark ID and FW Remark ID */
	error = elants_i2c_execute_command(client, cmd, sizeof(cmd),
					resp, sizeof(resp));
	if (error) {
		dev_err(&client->dev, "failed to query Remark ID: %d\n", error);
		return error;
	}
	ts_remark_id = get_unaligned_be16(&resp[3]);
	fw_remark_id = get_unaligned_le16(&fw_data[fw_size - 4]);
	dev_info(&client->dev,
			"ts_remark_id=0x%04x, fw_remark_id=0x%04x.\n",
			ts_remark_id, fw_remark_id);

	if (ts_remark_id == ELAN_REMARK_ID_OF_NON_REMARK_IC) {
		dev_info(&client->dev, "[ELAN] Pass, Non remark IC.\n");
	} else if (fw_remark_id != ts_remark_id) {
		dev_err(&client->dev,
			"Remark ID Mismatched: ts_remark_id=0x%04x, fw_remark_id=0x%04x.\n",
			ts_remark_id, fw_remark_id);
		error = -EINVAL;
	} else {
		dev_info(&client->dev, "Remark ID Matched!!\n");
	}
	return error;
}

static int elants_i2c_do_update_firmware(struct i2c_client *client,
					 const struct firmware *fw,
					 bool force)
{
	const u8 enter_iap[] = { 0x45, 0x49, 0x41, 0x50 };
	const u8 enter_iap2[] = { 0x54, 0x00, 0x12, 0x34 };
	const u8 iap_ack[] = { 0x55, 0xaa, 0x33, 0xcc };
	//const u8 close_idle[] = {0x54, 0x2c, 0x01, 0x01};
	u8 buf[HEADER_SIZE];
	u16 send_id;
	int page, n_fw_pages;
	int error;
	bool check_remark_id = /*true*/ts->iap_version >= 0x60;

	/* Recovery mode detection! */
	if (force) {
		dev_info(&client->dev, "Recovery mode procedure\n");
		if (check_remark_id) {
			error = elants_i2c_validate_remark_id(ts, fw->data,
							      fw->size);
			if (error)
				return error;
		}
		error = elants_i2c_send(client, enter_iap2, sizeof(enter_iap2));
	} else {
		/* Start IAP Procedure */
		dev_info(&client->dev, "Normal IAP procedure\n");
		/* Close idle mode */
		/*error = elants_i2c_send(client, close_idle, sizeof(close_idle));
		 *if (error)
		 *	dev_err(&client->dev, "Failed close idle: %d\n", error);
		 *msleep(60);
		 */
		elants_i2c_sw_reset(client);
		msleep(20);

		if (check_remark_id) {
			error = elants_i2c_validate_remark_id(ts, fw->data,
							      fw->size);
			if (error)
				return error;
		}
		error = elants_i2c_send(client, enter_iap, sizeof(enter_iap));
	}

	if (error) {
		dev_err(&client->dev, "failed to enter IAP mode: %d\n", error);
		return error;
	}

	msleep(20);

	/* check IAP state */
	error = elants_i2c_read(client, buf, 4);
	if (error) {
		dev_err(&client->dev,
			"failed to read IAP acknowledgment: %d\n", error);
		return error;
	}

	if (memcmp(buf, iap_ack, sizeof(iap_ack))) {
		dev_err(&client->dev,
			 "failed to enter IAP: %*ph (expected %*ph)\n",
			(int)sizeof(buf), buf, (int)sizeof(iap_ack), iap_ack);
		return -EIO;
	}

	dev_info(&client->dev, "successfully entered IAP mode");

	send_id = client->addr;
	error = elants_i2c_send(client, &send_id, 1);
	if (error) {
		dev_err(&client->dev, "sending dummy byte failed: %d\n",
			error);
		return error;
	}

	/* Clear the last page of Master */
	error = elants_i2c_send(client, fw->data, ELAN_FW_PAGESIZE);
	if (error) {
		dev_err(&client->dev, "clearing of the last page failed: %d\n",
			error);
		return error;
	}

	error = elants_i2c_read(client, buf, 2);
	if (error) {
		dev_err(&client->dev,
			"failed to read ACK for clearing the last page: %d\n",
			error);
		return error;
	}

	n_fw_pages = fw->size / ELAN_FW_PAGESIZE;
	dev_dbg(&client->dev, "IAP Pages = %d\n", n_fw_pages);

	for (page = 0; page < n_fw_pages; page++) {
		error = elants_i2c_fw_write_page(client,
						 fw->data + page *
						 ELAN_FW_PAGESIZE);
		if (error) {
			dev_err(&client->dev, "failed to write FW page %d: %d\n"
				, page, error);
			return error;
		}
	}

	/* Old iap needs to wait 200ms for WDT and rest is for hello packets */
	msleep(300);

	dev_info(&client->dev, "firmware update completed\n");
	return 0;
}

#endif

static int elants_i2c_fw_update(struct elants_data *ts)
{
	struct i2c_client *client = ts->client;
	const struct firmware *fw;
	char *fw_name;
	int error;

	fw_name = kasprintf(GFP_KERNEL, "elants_i2c_%04x.bin", ts->hw_version);
	if (!fw_name)
		return -ENOMEM;

	dev_info(&client->dev, "requesting fw name = %s\n", fw_name);
	error = request_firmware(&fw, fw_name, &client->dev);
	kfree(fw_name);
	if (error) {
		dev_info(&client->dev, "failed to request firmware: %d\n",
			 error);
		dev_info(&client->dev, "Falling back to 'elants_i2c.bin' instead\n");
		error = request_firmware(&fw, "elants_i2c.bin", &client->dev);
		if (error) {
			dev_info(&client->dev, "failed to request firmware: %d\n",
				 error);
			return error;
		}
	}

	if (fw->size % ELAN_FW_PAGESIZE) {
		dev_err(&client->dev, "invalid firmware length: %zu\n",
			fw->size);
		error = -EINVAL;
		goto out;
	}

	//disable_irq(client->irq);
	elan_set_irq_status(client->irq, 0);

#ifdef ELAN_HID_I2C
	error = elants_i2c_do_update_firmware_HID_I2C(client, fw);
#else
	error = elants_i2c_do_update_firmware(client, fw,
					      ts->iap_mode == ELAN_IAP_RECOVERY);
#endif
	if (error) {
		dev_err(&client->dev, "firmware update failed: %d\n", error);
		ts->iap_mode = ELAN_IAP_RECOVERY;
		goto out_enable_irq;
	}

	error = elants_i2c_initialize(ts);
	if (error) {
		dev_err(&client->dev,
			"failed to initialize device after firmware update: %d\n", error);
		ts->iap_mode = ELAN_IAP_RECOVERY;
		goto out_enable_irq;
	}

	ts->iap_mode = ELAN_IAP_OPERATIONAL;

 out_enable_irq:
	ts->state = ELAN_STATE_NORMAL;
	//enable_irq(client->irq);
	elan_set_irq_status(client->irq, 1);
	msleep(100);

	if (!error)
		elants_i2c_calibrate(ts);
 out:
	release_firmware(fw);
	return error;
}

#ifdef AUTO_UPDATE
static void elants_auto_update(struct work_struct *work)
{
	struct elants_data *ts = container_of(work,
			struct elants_data, delay_work.work);
	struct i2c_client *client = ts->client;
	const struct firmware *fw;
	char *fw_name;
	int error;
	u8 *fw_data;
	u16 new_fw_version;
	u16 new_hw_version;

	dev_info(&client->dev, "[elan]entry %s\n", __func__);
/*
 *	fw_name = kasprintf(GFP_KERNEL, "elants_i2c.bin");
 *	if (!fw_name){
 *		dev_err(&client->dev, "[elan]can't get fw_name,\n");
 *		return;
 *	}
 *
 *	dev_info(&client->dev, "requesting fw name = %s\n", fw_name);
 *	error = request_firmware(&fw, fw_name, &client->dev);
 *	kfree(fw_name);
 *	if (error) {
 *		dev_err(&client->dev, "failed to request firmware: %d\n",
 *			error);
 *		return;
 *	}
 */
	fw_name = kasprintf(GFP_KERNEL, "elants_i2c_%04x.bin", ts->hw_version);
	if (!fw_name) {
		dev_err(&client->dev, "[elan]can't get fw_name,\n");
		return;
	}

	dev_info(&client->dev, "requesting fw name = %s\n", fw_name);
	error = request_firmware(&fw, fw_name, &client->dev);
	kfree(fw_name);
	if (error) {
		dev_info(&client->dev, "failed to request firmware: %d\n",
			 error);
		dev_info(&client->dev,
			 "Falling back to 'elants_i2c.bin' instead\n");
		error = request_firmware(&fw, "elants_i2c.bin", &client->dev);
		if (error) {
			dev_info(&client->dev,
				"failed to request firmware: %d\n", error);
			return;
		}
	}

	if (fw->size % ELAN_FW_PAGESIZE) {
		dev_err(&client->dev, "invalid firmware length: %zu\n",
			fw->size);
		error = -EINVAL;
		goto out;
	}

	fw_data = (u8 *)fw->data;

	elants_i2c_hw_reset(client);
	msleep(200);

	//disable_irq(client->irq);
	elan_set_irq_status(client->irq, 0);
	error = check_isp_status(ts->client);
	//enable_irq(client->irq);
	elan_set_irq_status(client->irq, 1);

	/*recovery mode auto update*/
	if (error == 0x56) {
		release_firmware(fw);
		error = mutex_lock_interruptible(&ts->sysfs_mutex);
		if (error)
			return;

		error = elants_i2c_fw_update(ts);
		dev_dbg(&client->dev, "firmware update result: %d\n", error);

		mutex_unlock(&ts->sysfs_mutex);
		return;

	} else { /*normal mode check hw_version/fw_version*/
		//addr should be modified depends on different project
		new_hw_version  = fw_data[0x1FE67] << 8  | fw_data[0x1FE66];
		new_fw_version = fw_data[0x1F953] << 8  | fw_data[0x1F952];
		dev_info(&client->dev, "hw version=0x%x, new hw version=0x%x\n",
			 ts->hw_version, new_hw_version);
		dev_info(&client->dev, "fw version=0x%x, new fw version=0x%x\n",
			 ts->fw_version, new_fw_version);
		if (ts->hw_version == new_hw_version &&
			((ts->fw_version & 0xff) < (new_fw_version & 0xff))) {
			dev_dbg(&ts->client->dev, "start auto update\n");
			release_firmware(fw);

			error = mutex_lock_interruptible(&ts->sysfs_mutex);
			if (error)
				return;

			error = elants_i2c_fw_update(ts);
			dev_dbg(&client->dev, "firmware update result: %d\n",
				error);

			mutex_unlock(&ts->sysfs_mutex);
			return;
		}
	}
out:
	release_firmware(fw);
}
#endif

#ifdef ELAN_HID_I2C
/*
 * HID_I2C Event reporting.
 */

static int elan_ktf_hid_parse_xy(u8 *data, uint16_t *x, uint16_t *y)
{
	*x = *y = 0;

	*x = (data[6]);
	*x <<= 8;
	*x |= data[5];

	*y = (data[10]);
	*y <<= 8;
	*y |= data[9];

	return 0;
}

static int elan_ts_parse_pen(u8 *data, uint16_t *x, uint16_t *y, uint16_t *p)
{
	*x = *y = *p = 0;
	*x = data[5];
	*x <<= 8;
	*x |= data[4];

	*y = data[7];
	*y <<= 8;
	*y |= data[6];

	*p = data[9];
	*p <<= 8;
	*p |= data[8];

	return 0;
}

static int mtouch_status[MAX_CONTACT_NUM] = {0};
void force_release_pos(struct i2c_client *client)
{
	struct elants_data *ts = i2c_get_clientdata(client);
	struct input_dev *idev = ts->input;
	int i;

	for (i = 0; i < MAX_CONTACT_NUM; i++) {
		if (mtouch_status[i] == 0)
			continue;
		input_mt_slot(idev, i);
		input_mt_report_slot_state(idev, MT_TOOL_FINGER, 0);
		mtouch_status[i] = 0;
	}

	input_sync(idev);
}

static void elants_i2c_mt_event_hid_i2c_finger(struct elants_data *ts, u8 *buf)
{
	//struct input_dev *input = ts->input;
	struct input_dev *idev = ts->input;
	//unsigned int n_fingers;
	//u16 finger_state;
	int i;
	unsigned int idx, num;
	int finger_id;
	int finger_num;
	unsigned int active = 0;
	u16 x;
	u16 y;
	u16 p;

	//pr_info("[elan]:enter ELAN_HID_I2C_Finger_PKT\n");

	finger_num = buf[1];
	//if (finger_num > 5)
	//finger_num = 5;   /* support 5 fingers */
	idx = 3;
	num = finger_num;

	for (i = 0; i < finger_num; i++) {
		if ((buf[idx] & 0x03) == 0x00)
			active = 0;   /* 0x03: finger down, 0x00 finger up  */
		else
			active = 1;

		if ((buf[idx] & 0x03) == 0)
			num--;

		finger_id = (((buf[idx] & 0xfc) >> 2) - 1);

		input_report_key(idev, BTN_TOUCH, 1);
		input_mt_slot(idev, finger_id);
		input_mt_report_slot_state(idev, MT_TOOL_FINGER, true);

		if (active) {
			elan_ktf_hid_parse_xy(&buf[idx], &x, &y);
			p = ((((buf[idx + 2] & 0x0f) << 4) |
			     (buf[idx + 1] & 0x0f)) << 1);
			p = p > 254 ? 254 : p;

			/*x = x * 1600 / 1856;
			 *y = y * 2560 / 3008;
			 *y = 2560 - y;
			 */
#ifdef CONFIG_AMLOGIC_MODIFY
			/* remap touch to display */
			if (ts->x_dis && ts->y_dis) {
				x = x * ts->x_dis / ts->x_max;
				y = y * ts->y_dis / ts->y_max;
			}
#endif
			input_report_abs(idev, ABS_MT_TOUCH_MAJOR, p);
			input_report_abs(idev, ABS_MT_PRESSURE, p);
			input_report_abs(idev, ABS_MT_POSITION_X, x);
			input_report_abs(idev, ABS_MT_POSITION_Y, y);
			//pr_info("[elan hid] DOWN i=%d finger_id=%d x=%d y=%d
			//Finger NO.=%d \n", i, finger_id, x, y, finger_num);
		} else {
			input_mt_slot(idev, finger_id);
			input_mt_report_slot_state(idev, MT_TOOL_FINGER, false);
			//pr_info("[elan hid] UP i=%d finger_id=%d NO.=%d\n",
			//i, finger_id, finger_num);
		}
		mtouch_status[i] = active;
		idx += 11;
	}

	if (num == 0) {
		//pr_info("[elan] Release ALL Finger\n");
		input_report_key(idev, BTN_TOUCH, 0); //for all finger up
		force_release_pos(ts->client);
	}

	input_sync(idev);
}

static void elants_i2c_mt_event_hid_i2c_pen(struct elants_data *ts, u8 *buf)
{
	struct input_dev *idev = ts->input;
	int pen_hover = 0;
	int pen_down = 0;
	int pen_key = 0;
	u16 p = 0;
	u16 x = 0;
	u16 y = 0;

	pen_hover = buf[3] & 0x1;
	pen_down = buf[3] & 0x03;
	pen_key = buf[3];

	input_mt_slot(idev, 0);
	//input_mt_report_slot_state(idev, MT_TOOL_FINGER, pen_hover);
	input_mt_report_slot_state(idev, MT_TOOL_PEN, true);
	if (pen_key) {
		dev_dbg(&ts->client->dev,
			"[elan] report pen key %02x\n", pen_key);
		/*elan_ts_report_key(ts,pen_key);*/
	}

	if (pen_hover) {
		elan_ts_parse_pen(&buf[0], &x, &y, &p);
		//pr_info("[elan] ori x = %d, y = %d\n", x, y);
		y = y * ts->y_max / ((ts->cols - 1) * 256);
		x = x * ts->x_max / ((ts->rows - 1) * 256);
		//pr_info("[elan] after x = %d, y = %d\n", x, y);

		dev_dbg(&ts->client->dev,
			"[elan] pen id--------=%d x=%d y=%d\n", p, x, y);
		//input_mt_slot(ts->input, 0);
		//input_mt_report_slot_state(ts->input,MT_TOOL_PEN, true);
		if (pen_down == 0x01) {  /* report hover function  */
			input_report_abs(idev, ABS_MT_PRESSURE, 0);
			input_report_abs(idev, ABS_MT_DISTANCE, 15);
			dev_dbg(&ts->client->dev,
				"[elan pen] Hover DISTANCE=15\n");
		} else {
			input_report_abs(idev, ABS_MT_TOUCH_MAJOR, p);
			input_report_abs(idev, ABS_MT_PRESSURE, p);

			dev_dbg(&ts->client->dev,
				"[elan pen] PEN PRESSURE=%d\n", p);
		}

		input_report_key(ts->input, BTN_TOUCH, 1);
		// input_report_abs(ts->input, ABS_MT_TRACKING_ID, 0);
		input_report_abs(idev, ABS_MT_POSITION_X, x);
		input_report_abs(idev, ABS_MT_POSITION_Y, y);
/* #if 0
 *                 [>report pen key<]
 *                 input_report_key(idev, BTN_STYLUS, buf[3] & 0x04);	//btn1: barrel
 *                 input_report_key(idev, BTN_STYLUS2, buf[3] & 0x10);	//btn2: eraser
 * #endif
 */
		//	input_mt_sync(ts->input);

	} //else
		//input_mt_sync(ts->input);

		dev_dbg(&ts->client->dev, "[elan pen] %x:%x:%x:%x:%x:%x:%x:%x\n"
			, buf[0], buf[1], buf[2], buf[3], buf[4], buf[5],
			buf[6], buf[7]);
		dev_dbg(&ts->client->dev,
			"[elan] x=%d y=%d p=%d\n", x, y, p);

	if (pen_down == 0) {
		/*All Finger Up*/
		//input_report_key(ts->input, BTN_TOUCH, 0);
		//input_report_key(ts->input, BTN_TOOL_PEN, 0);
		//input_mt_sync(ts->input);
		input_mt_slot(idev, 0);
		input_mt_report_slot_state(idev, MT_TOOL_PEN, false);
		input_report_key(idev, BTN_TOUCH, 0); //for all finger up
		//force_release_pos(ts->client);
	}
	input_sync(idev);
}

#else	//for normal I2C
/*
 * Event reporting.
 */
static void elants_i2c_mt_event(struct elants_data *ts, u8 *buf)
{
	struct input_dev *input = ts->input;
	unsigned int n_fingers;
	u16 finger_state;
	int i;

	n_fingers = buf[FW_POS_STATE + 1] & 0x0f;
	finger_state = ((buf[FW_POS_STATE + 1] & 0x30) << 4) |
			buf[FW_POS_STATE];

	dev_dbg(&ts->client->dev,
		"n_fingers: %u, state: %04x\n",  n_fingers, finger_state);

	for (i = 0; i < MAX_CONTACT_NUM && n_fingers; i++) {
		if (finger_state & 1) {
			unsigned int x, y, p, w;
			u8 *pos;

			pos = &buf[FW_POS_XY + i * 3];
			x = (((u16)pos[0] & 0xf0) << 4) | pos[1];
			y = (((u16)pos[0] & 0x0f) << 8) | pos[2];
			p = buf[FW_POS_PRESSURE + i];
			w = buf[FW_POS_WIDTH + i];

			dev_dbg(&ts->client->dev, "i=%d x=%d y=%d p=%d w=%d\n",
				i, x, y, p, w);

			input_mt_slot(input, i);
			input_mt_report_slot_state(input, MT_TOOL_FINGER, true);
			input_event(input, EV_ABS, ABS_MT_POSITION_X, x);
			input_event(input, EV_ABS, ABS_MT_POSITION_Y, y);
			input_event(input, EV_ABS, ABS_MT_PRESSURE, p);
			input_event(input, EV_ABS, ABS_MT_TOUCH_MAJOR, w);

			n_fingers--;
		}

		finger_state >>= 1;
	}

	input_mt_sync_frame(input);
	input_sync(input);
}

static u8 elants_i2c_calculate_checksum(u8 *buf)
{
	u8 checksum = 0;
	u8 i;

	for (i = 0; i < FW_POS_CHECKSUM; i++)
		checksum += buf[i];

	return checksum;
}

static void elants_i2c_event(struct elants_data *ts, u8 *buf)
{
	u8 checksum = elants_i2c_calculate_checksum(buf);

	if (unlikely(buf[FW_POS_CHECKSUM] != checksum))
		dev_warn(&ts->client->dev,
			 "%s: invalid checksum for packet %02x: %02x vs. %02x\n"
			, __func__, buf[FW_POS_HEADER], checksum,
			buf[FW_POS_CHECKSUM]);
	else if (unlikely(buf[FW_POS_HEADER] != HEADER_REPORT_10_FINGER))
		dev_warn(&ts->client->dev, "%s: unknown packet type: %02x\n",
			 __func__, buf[FW_POS_HEADER]);
	else
		elants_i2c_mt_event(ts, buf);
}

#endif

static void  elan_ts_work_func(struct work_struct *work)
{
	const u8 wait_packet[] = { 0x64, 0x64, 0x64, 0x64 };
	struct elants_data *ts;// = private_ts;//_dev;
	struct i2c_client *client;// = ts->client;
#ifdef ELAN_HID_I2C
	u8 buf1[PACKET_SIZE] = { 0 };
#else
	int report_count, report_len;
#endif
	int i;
	int len;

	ts = container_of(work, struct elants_data, ts_work);
	client = ts->client;

	if (gpio_get_value(ts->intr_gpio)) {
		//pr_info("[elan] Detected the jitter on INT pin");
		//enable_irq(client->irq);
		elan_set_irq_status(client->irq, 1);
		return; //IRQ_HANDLED;
	}

	len = i2c_master_recv(client, ts->buf, 67/*sizeof(ts->buf)*/);
	if (len < 0) {
		dev_err(&client->dev, "%s: failed to read data: %d\n",
			 __func__, len);
		goto out;
	}
	//dev_err(&client->dev, "done nothing\n");
	//goto out;
	dev_dbg(&client->dev, "%s: packet %*ph\n", __func__, HEADER_SIZE,
		ts->buf);

	switch (ts->state) {
	case ELAN_WAIT_RECALIBRATION:
#ifdef ELAN_HID_I2C
		if (ts->buf[FW_HDR_TYPE + 4] == CMD_HEADER_REK) {
			memcpy(ts->cmd_resp, ts->buf, sizeof(ts->cmd_resp));
			complete(&ts->cmd_done);
			ts->state = ELAN_STATE_NORMAL;

			dev_info(&client->dev,
				 "[elan] Calibration: %2x,%2x,%2x,%2x,%2x,%2x,%2x,%2x\n",
				 ts->buf[0], ts->buf[1], ts->buf[2], ts->buf[3],
				 ts->buf[4], ts->buf[5], ts->buf[6],
				 ts->buf[7]);
		}
#else
		if (ts->buf[FW_HDR_TYPE] == CMD_HEADER_REK) {
			memcpy(ts->cmd_resp, ts->buf, sizeof(ts->cmd_resp));
			complete(&ts->cmd_done);
			ts->state = ELAN_STATE_NORMAL;
		}
#endif
		break;

	case ELAN_WAIT_QUEUE_HEADER:
		if (ts->buf[FW_HDR_TYPE] != QUEUE_HEADER_NORMAL)
			break;

		ts->state = ELAN_STATE_NORMAL;
		fallthrough;

	case ELAN_STATE_NORMAL:

		switch (ts->buf[FW_HDR_TYPE]) {
#ifdef ELAN_HID_I2C
		case CMD_HEADER_HID_I2C:  //0x43
			if (ts->buf[FW_HDR_TYPE + 4] == CMD_HEADER_REK)
				//43 00 02 04 66 XX XX XX
				dev_info(&client->dev, "[elan] Calibration:");
			if (ts->buf[FW_HDR_TYPE + 4] == CMD_HEADER_RESP)
				//43 00 02 04 52 XX XX XX
				dev_info(&client->dev, "[elan] Response:");

			dev_info(&client->dev, "[elan] %2x, %2x,%2x,%2x,%2x,%2x,%2x,%2x,%2x,%2x\n",
				 ts->buf[0], ts->buf[1], ts->buf[2], ts->buf[3],
				 ts->buf[4], ts->buf[5], ts->buf[6], ts->buf[7],
				 ts->buf[8], ts->buf[9]);

			break;
#else
		case CMD_HEADER_HELLO:  //filter special packet: 0x55 0x52 0x66
		case CMD_HEADER_RESP:
		case CMD_HEADER_REK:
			break;
#endif
		case QUEUE_HEADER_WAIT:  //0x64
			if (memcmp(ts->buf, wait_packet, sizeof(wait_packet))) {
				dev_err(&client->dev,
					"invalid wait packet %*ph\n",
					HEADER_SIZE, ts->buf);
			} else {
				ts->state = ELAN_WAIT_QUEUE_HEADER;
				/* CHECK: usleep_range is preferred over udelay;
				 * see Documentation/timers/timers-howto.rst
				 * udelay(30);
				 */
				usleep_range(30, 35);
			}
			break;

#ifdef ELAN_HID_I2C
		case HID_I2C_FINGER_HEADER: //HID_I2C header = 0x3F
			ts->buf[1] = ts->buf[62];  //store finger number
			if (ts->buf[62] > 5) {  //more than 5 finger
				 //dev_err(&client->dev, "%s: second packet
				 //buf[62]: %x\n",__func__, ts->buf[62]);
				len = i2c_master_recv(client, buf1,
						      sizeof(buf1));
				if (len < 0) {
					dev_err(&client->dev,
						"%s: failed to read data: %d\n",
						__func__, len);
					goto out;
				}
				for (i = 3; i < 67; i++)
					ts->buf[55 + i] = buf1[i];
			}

			elants_i2c_mt_event_hid_i2c_finger(ts, ts->buf);
			break;

		case HID_I2C_PEN_HEADER: //HID_I2C PEN header = 0x0D
			elants_i2c_mt_event_hid_i2c_pen(ts, ts->buf);
			break;
#else
		case QUEUE_HEADER_SINGLE:  //no buffer mode = 0x62
			elants_i2c_event(ts, &ts->buf[HEADER_SIZE]);
			break;

		case QUEUE_HEADER_NORMAL:  //buffer mode = 0x63
			report_count = ts->buf[FW_HDR_COUNT];
			if (report_count == 0 || report_count > 3) {
				dev_err(&client->dev,
					"bad report count: %*ph\n",
					HEADER_SIZE, ts->buf);
				break;
			}

			report_len = ts->buf[FW_HDR_LENGTH] / report_count;
			if (report_len != PACKET_SIZE) {
				dev_err(&client->dev,
					"mismatching report length: %*ph\n",
					HEADER_SIZE, ts->buf);
				break;
			}

			for (i = 0; i < report_count; i++) {
				u8 *buf = ts->buf + HEADER_SIZE +
					  i * PACKET_SIZE;
				elants_i2c_event(ts, buf);
			}
			break;
#endif
		default:
			dev_err(&client->dev, "unknown packet: %*ph\n",
				HEADER_SIZE, ts->buf);
			break;
		}
		break;
	}

 out:
	//enable_irq(client->irq);
	elan_set_irq_status(client->irq, 1);
	return;// IRQ_HANDLED;
}

static irqreturn_t elants_i2c_irq(int irq, void *_dev)
{
	struct elants_data *ts = (struct elants_data *)_dev;//private_ts;
	//dev_err(&ts->client->dev, " %s enter\n", __func__);
	//disable_irq_nosync(ts->client->irq);
	mutex_lock(&ts->irq_mutex);
	if (ts->irq_enabled) {
		disable_irq_nosync(ts->client->irq);
		ts->irq_enabled = 0;
		dev_dbg(&ts->client->dev, "%s out\n", __func__);
	}
	mutex_unlock(&ts->irq_mutex);
	queue_work(ts->elan_wq, &ts->ts_work);

	return IRQ_HANDLED;
}

#ifdef ELAN_RAW_DATA
/*
 * ioctl interface
 */

static int elan_iap_open(struct inode *inode, struct file *filp)
{
	struct elants_data *ts =
		container_of(((struct miscdevice *)filp->private_data),
			     struct elants_data, firmware);

	pr_info("[elan] into %s\n", __func__);

	if (!private_ts)
		pr_info("ts is NULL\n");
		//dev_err(&private_ts->client->dev,"private_ts is NULL\n");
	else
		pr_info("irq num = %d\n", private_ts->client->irq);

	if (!ts) {
		filp->private_data = ts;
		pr_info("irq num = %d\n", ts->client->irq);
	} else {
		pr_info("[elan]ts is NULL\n");
	}

	return 0;
}

static ssize_t elan_iap_read(struct file *filp, char *buff, size_t count,
			     loff_t *offp)
{
	char *tmp;
	int ret;
	long rc;
	//struct elants_data *ts = (struct elants_data *)filp->private_data;
    //struct i2c_client *client= ts->client;

	pr_info("[elan] into %s\n", __func__);

	if (count > 8192)
		count = 8192;

	tmp = kmalloc(count, GFP_KERNEL);

	if (!tmp)
		return -ENOMEM;

	ret = i2c_master_recv(private_ts->client, tmp, count);
	//ret = i2c_master_recv(ts->client, tmp, count);

	if (ret >= 0)
		rc = copy_to_user(buff, tmp, count);

	kfree(tmp);
	return ret;//(ret == 1) ? count : ret;
}

static ssize_t elan_iap_write(struct file *filp, const char *buff, size_t count,
			      loff_t *offp)
{
	int ret;
	char *tmp;

	//struct elants_data *ts = (struct elants_data *)filp->private_data;
    //struct i2c_client *client= ts->client;

	pr_info("[elan] into %s\n", __func__);

	if (count > 8192)
		count = 8192;

	tmp = kmalloc(count, GFP_KERNEL);

	if (!tmp)
		return -ENOMEM;

	if (copy_from_user(tmp, buff, count))
		return -EFAULT;

	ret = i2c_master_send(private_ts->client, tmp, count);
	//ret = i2c_master_send(ts->client, tmp, count);

	kfree(tmp);
	return ret;//(ret == 1) ? count : ret;
}

int elan_iap_release(struct inode *inode, struct file *filp)
{
	filp->private_data = NULL;

	return 0;
}

static long elan_iap_ioctl(struct file *filp, unsigned int cmd,
			   unsigned long arg)
{
	int __user *ip = (int __user *)arg;
	struct elants_data *ts = (struct elants_data *)filp->private_data;
    //struct i2c_client *client= ts->client;

	pr_info("[elan] into %s, cmd value %x\n", __func__, cmd);

	switch (cmd) {
	case IOCTL_RESET:
		//elants_i2c_hw_reset(private_ts->client);
		elants_i2c_hw_reset(ts->client);
		break;
	case IOCTL_IAP_MODE_LOCK:
	//disable_irq(private_ts->client->irq);
	//disable_irq(ts->client->irq);
		elan_set_irq_status(ts->client->irq, 0);
		pr_info("[elan] ts->fw_version %x\n",  ts->fw_version);
		break;
	case IOCTL_IAP_MODE_UNLOCK:
	// enable_irq(private_ts->client->irq);
	//enable_irq(ts->client->irq);
		elan_set_irq_status(ts->client->irq, 1);
		break;
	case IOCTL_ROUGH_CALIBRATE:
	// elants_i2c_calibrate(private_ts);
		elants_i2c_calibrate(ts);
		break;
	case IOCTL_I2C_INT:
	//put_user(gpio_get_value(private_ts->intr_gpio), ip);
		put_user(gpio_get_value(ts->intr_gpio), ip);
		break;
	case IOCTL_POWER_LOCK:
		power_lock = 1;
		break;
	case IOCTL_POWER_UNLOCK:
		power_lock = 0;
		break;
	case IOCTL_FW_INFO:
		//__fw_packet_handler(private_ts->client);
		pr_info("[elan] IOCTL_FW_INFO\n");
		elants_i2c_initialize(private_ts);
		break;
	case IOCTL_FW_VER:
		//return private_ts->fw_version;
		return ts->fw_version;
		//break;
	case IOCTL_FW_ID:
	//return private_ts->hw_version;
		return ts->hw_version;
		//break;
	case IOCTL_BC_VER:
		//return private_ts->bc_version;
		return ts->bc_version;
		//break;
	case IOCTL_X_RESOLUTION:
		//return private_ts->x_max;
		return ts->x_max;
		//break;
	case IOCTL_Y_RESOLUTION:
		//return private_ts->y_max;
		return ts->y_max;
		//break;
	default:
		pr_info("[elan] Un-known IOCTL Command %d\n", cmd);
		break;
	}
	return 0;
}

/* WARNING: struct file_operations should normally be const */
static const struct file_operations elan_touch_fops = {
	.open = elan_iap_open,
	.write = elan_iap_write,
	.read = elan_iap_read,
	.release = elan_iap_release,
	.unlocked_ioctl = elan_iap_ioctl,
	.compat_ioctl = elan_iap_ioctl,
};
#endif

/*
 * sysfs interface
 */
static ssize_t calibrate_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct elants_data *ts = i2c_get_clientdata(client);
	int error;

	error = mutex_lock_interruptible(&ts->sysfs_mutex);
	if (error)
		return error;

	error = elants_i2c_calibrate(ts);

	mutex_unlock(&ts->sysfs_mutex);
	return error ?: count;
}

static ssize_t update_fw_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct elants_data *ts = i2c_get_clientdata(client);
	int error;

	error = mutex_lock_interruptible(&ts->sysfs_mutex);
	if (error)
		return error;

	error = elants_i2c_fw_update(ts);
	dev_dbg(dev, "firmware update result: %d\n", error);

	mutex_unlock(&ts->sysfs_mutex);
	return error ?: count;
}

static ssize_t iap_mode_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct elants_data *ts = i2c_get_clientdata(client);

	return sprintf(buf, "%s\n", ts->iap_mode == ELAN_IAP_OPERATIONAL ?
		       "Normal" : "Recovery");
}

#ifdef ELAN_RAW_DATA
static ssize_t reset_h_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct elants_data *ts = i2c_get_clientdata(client);

	gpiod_direction_output(ts->reset_gpio, 0);
	return count;
}

static ssize_t reset_l_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct elants_data *ts = i2c_get_clientdata(client);

	gpiod_direction_output(ts->reset_gpio, 1);
	return count;
}

static ssize_t enable_irq_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	//struct elants_data *ts = i2c_get_clientdata(client);

	//enable_irq(client->irq);
	elan_set_irq_status(client->irq, 1);
	dev_info(&client->dev, "Enable IRQ.\n");
	return count;
}

static ssize_t disable_irq_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	//struct elants_data *ts = i2c_get_clientdata(client);

	//disable_irq(client->irq);
	elan_set_irq_status(client->irq, 0);
	dev_info(&client->dev, "Disable IRQ.\n");
	return count;
}

static ssize_t reset_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	//struct elants_data *ts = i2c_get_clientdata(client);

	elants_i2c_hw_reset(client);
	dev_info(&client->dev, "HW reset.\n");
	return count;
}

static ssize_t gpio_int_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct elants_data *ts = i2c_get_clientdata(client);

	dev_info(&client->dev, "ts->intr_gpio = %d\n",
		 gpio_get_value(ts->intr_gpio));
	return sprintf(buf, "%d\n", gpio_get_value(ts->intr_gpio));
	//dev_dbg(&client->dev, "Read gpio int.\n");
	//return count;
}
#endif
/* WARNING: Symbolic permissions 'S_XXX' are not preferred.
 * Consider using octal permissions '0xxx'
 */
static DEVICE_ATTR_WO(calibrate);
static DEVICE_ATTR_RO(iap_mode);
static DEVICE_ATTR_WO(update_fw);
#ifdef ELAN_RAW_DATA
static DEVICE_ATTR_WO(reset_h);
static DEVICE_ATTR_WO(reset_l);
static DEVICE_ATTR_WO(enable_irq);
static DEVICE_ATTR_WO(disable_irq);
static DEVICE_ATTR_WO(reset);
static DEVICE_ATTR_RO(gpio_int);
#endif

struct elants_version_attribute {
	struct device_attribute dattr;
	size_t field_offset;
	size_t field_size;
};

#define __ELANTS_FIELD_SIZE(_field)					\
	sizeof(((struct elants_data *)NULL)->_field)
#define __ELANTS_VERIFY_SIZE(_field)                        \
	(BUILD_BUG_ON_ZERO(__ELANTS_FIELD_SIZE(_field) > 2) +   \
	 __ELANTS_FIELD_SIZE(_field))
#define ELANTS_VERSION_ATTR(_field)                                 \
	struct elants_version_attribute elants_ver_attr_##_field = {	\
		.dattr = __ATTR(_field, 0444,                            \
			 elants_version_attribute_show, NULL),       \
		.field_offset = offsetof(struct elants_data, _field),       \
		.field_size = __ELANTS_VERIFY_SIZE(_field),                 \
	}

static ssize_t elants_version_attribute_show(struct device *dev,
					     struct device_attribute *dattr,
					     char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct elants_data *ts = i2c_get_clientdata(client);
	struct elants_version_attribute *attr =
		container_of(dattr, struct elants_version_attribute, dattr);
	u8 *field = (u8 *)((char *)ts + attr->field_offset);
	unsigned int fmt_size;
	unsigned int val;

	if (attr->field_size == 1) {
		val = *field;
		fmt_size = 2; /* 2 HEX digits */
	} else {
		val = *(u16 *)field;
		fmt_size = 4; /* 4 HEX digits */
	}

	return sprintf(buf, "%0*x\n", fmt_size, val);
}

static ELANTS_VERSION_ATTR(fw_version);
static ELANTS_VERSION_ATTR(hw_version);
static ELANTS_VERSION_ATTR(test_version);
static ELANTS_VERSION_ATTR(solution_version);
static ELANTS_VERSION_ATTR(bc_version);
static ELANTS_VERSION_ATTR(iap_version);

static struct attribute *elants_attributes[] = {
	&dev_attr_calibrate.attr,
	&dev_attr_update_fw.attr,
	&dev_attr_iap_mode.attr,
#ifdef ELAN_RAW_DATA
	&dev_attr_reset_h.attr,
	&dev_attr_reset_l.attr,
	&dev_attr_enable_irq.attr,
	&dev_attr_disable_irq.attr,
	&dev_attr_reset.attr,
	&dev_attr_gpio_int.attr,
#endif

	&elants_ver_attr_fw_version.dattr.attr,
	&elants_ver_attr_hw_version.dattr.attr,
	&elants_ver_attr_test_version.dattr.attr,
	&elants_ver_attr_solution_version.dattr.attr,
	&elants_ver_attr_bc_version.dattr.attr,
	&elants_ver_attr_iap_version.dattr.attr,
	NULL
};

static struct attribute_group elants_attribute_group = {
	.name = DEVICE_NAME,
	.attrs = elants_attributes,
};

static void elants_i2c_remove_sysfs_group(void *_data)
{
	struct elants_data *ts = _data;

	sysfs_remove_group(&ts->client->dev.kobj, &elants_attribute_group);
}

static int elants_i2c_power_on(struct elants_data *ts)
{
	int error;

	/*
	 * If we do not have reset gpio assume platform firmware
	 * controls regulators and does power them on for us.
	 */
	if (IS_ERR_OR_NULL(ts->reset_gpio))
		return 0;

	gpiod_set_value_cansleep(ts->reset_gpio, 1);	//set reset low

	error = regulator_enable(ts->vcc33);
	if (error) {
		dev_err(&ts->client->dev,
			"failed to enable vcc33 regulator: %d\n", error);
		goto release_reset_gpio;
	}

	error = regulator_enable(ts->vccio);
	if (error) {
		dev_err(&ts->client->dev,
			"failed to enable vccio regulator: %d\n", error);
		regulator_disable(ts->vcc33);
		goto release_reset_gpio;
	}

	/*
	 * We need to wait a bit after powering on controller before
	 * we are allowed to release reset GPIO.
	 */
	udelay(ELAN_POWERON_DELAY_USEC);

 release_reset_gpio:
	gpiod_set_value_cansleep(ts->reset_gpio, 0);	//set reset high
	if (error)
		return error;

	msleep(ELAN_RESET_DELAY_MSEC);

	return 0;
}

static void elants_i2c_power_off(void *_data)
{
	struct elants_data *ts = _data;

	if (ts->unbinding) {
		dev_info(&ts->client->dev,
			 "Not disabling regulators to continue allowing userspace i2c-dev access\n");
		return;
	}

	if (!IS_ERR_OR_NULL(ts->reset_gpio)) {
		/*
		 * Activate reset gpio to prevent leakage through the
		 * pin once we shut off power to the controller.
		 */
		gpiod_set_value_cansleep(ts->reset_gpio, 1);
		regulator_disable(ts->vccio);
		regulator_disable(ts->vcc33);
	}
}

static int elants_i2c_probe(struct i2c_client *client,
			    const struct i2c_device_id *id)
{
	union i2c_smbus_data dummy;
	struct elants_data *ts;
	unsigned long irqflags;
	int error;
#ifdef AUTO_UPDATE
	unsigned long delay = 3 * HZ;
#endif
	dev_info(&client->dev, "[elan] enter %s ...v%s\n", __func__,
		 DRV_VERSION);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "%s: i2c check functionality error\n",
			DEVICE_NAME);
		return -ENXIO;
	}

	ts = devm_kzalloc(&client->dev, sizeof(struct elants_data), GFP_KERNEL);
	if (!ts)
		return -ENOMEM;

	mutex_init(&ts->sysfs_mutex);
	mutex_init(&ts->irq_mutex);
	init_completion(&ts->cmd_done);

	ts->client = client;
	i2c_set_clientdata(client, ts);

#ifdef ELAN_RAW_DATA
	private_ts = ts;
#endif

	ts->vcc33 = devm_regulator_get(&client->dev, "vcc33");
	if (IS_ERR(ts->vcc33)) {
		error = PTR_ERR(ts->vcc33);
		if (error != -EPROBE_DEFER)
			dev_err(&client->dev,
				"Failed to get 'vcc33' regulator: %d\n", error);
		return error;
	}

	ts->vccio = devm_regulator_get(&client->dev, "vccio");
	if (IS_ERR(ts->vccio)) {
		error = PTR_ERR(ts->vccio);
		if (error != -EPROBE_DEFER)
			dev_err(&client->dev,
				"Failed to get 'vccio' regulator: %d\n", error);
		return error;
	}

	ts->reset_gpio = devm_gpiod_get(&client->dev, "elan,reset",
					GPIOD_OUT_LOW);
	if (IS_ERR(ts->reset_gpio)) {
		dev_err(&client->dev, "failed to get reset gpio\n");
		error = PTR_ERR(ts->reset_gpio);

		if (error == -EPROBE_DEFER)
			return error;
		/* WARNING: ENOSYS means 'invalid syscall nr' and nothing else
		 * if (error != -ENOENT && error != -ENOSYS) {
		 */
		if (error != -ENOENT) {
			dev_err(&client->dev, "failed to get reset gpio: %d\n",
				error);
			return error;
		}

		ts->keep_power_in_suspend = true;
	}

	ts->keep_power_in_suspend = true;  //expect power is on when suspend

	//get int status from dts
	ts->intr_gpio = of_get_named_gpio_flags(client->dev.of_node,
						"elan,irq-gpio", 0, NULL);
	pr_info("[elan] ts->intr_gpio = %d\n", ts->intr_gpio);
	if (!gpio_is_valid(ts->intr_gpio)) {
		dev_err(&client->dev, "failed to get intr gpio\n");
	} else {
		client->irq = gpio_to_irq(ts->intr_gpio);
		dev_dbg(&client->dev, "[elan] irq num = %d.\n", client->irq);
	}

	error = gpio_request(ts->intr_gpio, "tp_irq");
	gpio_direction_input(ts->intr_gpio);

	error = elants_i2c_power_on(ts);
	if (error)
		return error;

	error = devm_add_action(&client->dev, elants_i2c_power_off, ts);
	if (error) {
		dev_err(&client->dev,
			"failed to install power off action: %d\n", error);
		elants_i2c_power_off(ts);
		return error;
	}

	/* Make sure there is something at this address */
	if (i2c_smbus_xfer(client->adapter, client->addr, 0,
			   I2C_SMBUS_READ, 0, I2C_SMBUS_BYTE, &dummy) < 0) {
		dev_err(&client->dev, "nothing at this address\n");
		/* add to free gpio */
		gpio_free(ts->intr_gpio);
		return -ENXIO;
	}

	error = elants_i2c_initialize(ts);
	if (error) {
		dev_err(&client->dev, "failed to initialize: %d\n", error);
		return error;
	}

#ifdef AUTO_UPDATE
	/*Elan fw auto update*/
	dev_err(&client->dev,
		"[elan]init delayed work for elants_auto_update\n");
	INIT_DELAYED_WORK(&ts->delay_work, elants_auto_update);
	ts->elan_ic_update = create_singlethread_workqueue("elan_ic_update");
	queue_delayed_work(ts->elan_ic_update, &ts->delay_work, delay);
#endif

	ts->input = devm_input_allocate_device(&client->dev);
	if (!ts->input) {
		dev_err(&client->dev, "Failed to allocate input device\n");
		return -ENOMEM;
	}

	ts->input->name = "Elan Touchscreen";
	ts->input->id.bustype = BUS_I2C;

	__set_bit(BTN_TOUCH, ts->input->keybit);
	__set_bit(EV_ABS, ts->input->evbit);
	__set_bit(EV_KEY, ts->input->evbit);
	__set_bit(BTN_TOOL_PEN, ts->input->keybit);

/* WARNING: Consider removing the code enclosed by this #if 0 and its #endif */
/* #if 0
 *         [>for pen key<]
 *         __set_bit(BTN_STYLUS, ts->input->keybit);
 *         __set_bit(BTN_STYLUS2, ts->input->keybit);
 * #endif
 */

	/* Single touch input params setup */
	input_set_abs_params(ts->input, ABS_X, 0, ts->x_max, 0, 0);
	input_set_abs_params(ts->input, ABS_Y, 0, ts->y_max, 0, 0);
	input_set_abs_params(ts->input, ABS_PRESSURE, 0, 255, 0, 0);
	input_abs_set_res(ts->input, ABS_X, ts->x_res);
	input_abs_set_res(ts->input, ABS_Y, ts->y_res);

	/* Multitouch input params setup */
	error = input_mt_init_slots(ts->input, MAX_CONTACT_NUM,
				    INPUT_MT_DIRECT | INPUT_MT_DROP_UNUSED);
	if (error) {
		dev_err(&client->dev,
			"failed to initialize MT slots: %d\n", error);
		return error;
	}

#ifdef CONFIG_AMLOGIC_MODIFY
	error  = of_property_read_u32(client->dev.of_node, "elen,display-x", &ts->x_dis);
	if (error) {
		ts->x_dis = 0;
		ts->y_dis = 0;
	} else {
		error  = of_property_read_u32(client->dev.of_node, "elen,display-y", &ts->y_dis);
		if (error) {
			dev_err(&client->dev, "need to set y & x at the same time\n");
			return error;
		}
	}
#endif
	input_set_abs_params(ts->input, ABS_MT_TOOL_TYPE, 0, MT_TOOL_MAX, 0, 0);
#ifndef CONFIG_AMLOGIC_MODIFY
	input_set_abs_params(ts->input, ABS_MT_POSITION_X, 0, ts->x_max, 0, 0);
	input_set_abs_params(ts->input, ABS_MT_POSITION_Y, 0, ts->y_max, 0, 0);
#else
	input_set_abs_params(ts->input, ABS_MT_POSITION_X, 0, ts->x_dis, 0, 0);
	input_set_abs_params(ts->input, ABS_MT_POSITION_Y, 0, ts->y_dis, 0, 0);
#endif
	input_set_abs_params(ts->input, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);
	input_set_abs_params(ts->input, ABS_MT_PRESSURE, 0, 255, 0, 0);
	input_abs_set_res(ts->input, ABS_MT_POSITION_X, ts->x_res);
	input_abs_set_res(ts->input, ABS_MT_POSITION_Y, ts->y_res);

	input_set_drvdata(ts->input, ts);

	error = input_register_device(ts->input);
	if (error) {
		dev_err(&client->dev,
			"unable to register input device: %d\n", error);
		return error;
	}

	/*
	 * Platform code (ACPI, DTS) should normally set up interrupt
	 * for us, but in case it did not let's fall back to using falling
	 * edge to be compatible with older Chromebooks.
	 */
	irqflags = irq_get_trigger_type(client->irq);
	if (!irqflags)
		irqflags = IRQF_TRIGGER_FALLING /*IRQF_TRIGGER_LOW*/;

	error = devm_request_threaded_irq(&client->dev, client->irq, NULL,
					  elants_i2c_irq,
					  irqflags | IRQF_ONESHOT,
					  client->name, ts);
	if (error) {
		dev_err(&client->dev, "Failed to register interrupt\n");
		return error;
	}

	mutex_lock(&ts->irq_mutex);
	ts->irq_enabled = true;
	mutex_unlock(&ts->irq_mutex);

	/*
	 * Systems using device tree should set up wakeup via DTS,
	 * the rest will configure device as wakeup source by default.
	 */
	if (!client->dev.of_node)
		device_init_wakeup(&client->dev, true);

	error = sysfs_create_group(&client->dev.kobj, &elants_attribute_group);
	if (error) {
		dev_err(&client->dev, "failed to create sysfs attributes: %d\n",
			error);
		return error;
	}

#ifdef ELAN_RAW_DATA
	//for Firmware Update and read RAW Data
	ts->firmware.minor = MISC_DYNAMIC_MINOR;
	ts->firmware.name = "elan-iap";
	ts->firmware.fops = &elan_touch_fops;
	//ts->firmware.mode = S_IFREG | S_IRWXUGO; below fix warning
	ts->firmware.mode = 0777;

	if (misc_register(&ts->firmware) < 0)
		pr_info("[elan] misc_register failed!!\n");
	else
		pr_info("[elan] misc_register finished!!\n");

#endif

	error = devm_add_action(&client->dev, elants_i2c_remove_sysfs_group,
				ts);
	if (error) {
		elants_i2c_remove_sysfs_group(ts);
		dev_err(&client->dev,
			"Failed to add sysfs cleanup action: %d\n", error);
		return error;
	}

	ts->elan_wq = create_singlethread_workqueue("elan_wq");
	if (IS_ERR(ts->elan_wq)) {
		error = PTR_ERR(ts->elan_wq);
		dev_err(&client->dev,
			"[elan error] failed to create kernel thread: %d\n",
				error);
		return error;
		//goto err_create_workqueue_failed;
	}
	INIT_WORK(&ts->ts_work, elan_ts_work_func);

	return 0;
}

static int elants_i2c_remove(struct i2c_client *client)
{
	struct elants_data *ts = i2c_get_clientdata(client);

	/*
	 * Let elants_i2c_power_off know that it needs to keep
	 * regulators on.
	 */
	ts->unbinding = true;

	return 0;
}

static int __maybe_unused elants_i2c_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct elants_data *ts = i2c_get_clientdata(client);
#ifdef ELAN_HID_I2C
	const u8 set_sleep_cmd[37] = { 0x04, 0x00, 0x23, 0x00, 0x03, 0x00,
				       0x04, 0x54, 0x50, 0x00, 0x01 };
#else
	const u8 set_sleep_cmd[] = { 0x54, 0x50, 0x00, 0x01 };
#endif
	int retry_cnt;
	int error;

	/* Command not support in IAP recovery mode */
	if (ts->iap_mode != ELAN_IAP_OPERATIONAL)
		return -EBUSY;

	//disable_irq(client->irq);
	elan_set_irq_status(client->irq, 0);

	if (device_may_wakeup(dev)) {
		/*
		 * The device will automatically enter idle mode
		 * that has reduced power consumption.
		 */
		ts->wake_irq_enabled = (enable_irq_wake(client->irq) == 0);
	} else if (ts->keep_power_in_suspend) {
		for (retry_cnt = 0; retry_cnt < MAX_RETRIES; retry_cnt++) {
			error = elants_i2c_send(client, set_sleep_cmd,
						sizeof(set_sleep_cmd));
			if (!error)
				break;

			dev_err(&client->dev, "suspend command failed: %d\n",
				error);
		}
	} else {
		elants_i2c_power_off(ts);
	}

	return 0;
}

static int __maybe_unused elants_i2c_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct elants_data *ts = i2c_get_clientdata(client);
#ifdef ELAN_HID_I2C
	const u8 set_active_cmd[37] = { 0x04, 0x00, 0x23, 0x00, 0x03, 0x00,
					0x04, 0x54, 0x58, 0x00, 0x01 };
#else
	const u8 set_active_cmd[] = { 0x54, 0x58, 0x00, 0x01 };
#endif
	int retry_cnt;
	int error;

	if (device_may_wakeup(dev)) {
		if (ts->wake_irq_enabled)
			disable_irq_wake(client->irq);
		elants_i2c_sw_reset(client);
	} else if (ts->keep_power_in_suspend) {
		for (retry_cnt = 0; retry_cnt < MAX_RETRIES; retry_cnt++) {
			error = elants_i2c_send(client, set_active_cmd,
						sizeof(set_active_cmd));
			if (!error)
				break;

			dev_err(&client->dev, "resume command failed: %d\n",
				error);
		}
	} else {
		elants_i2c_power_on(ts);
		elants_i2c_initialize(ts);
	}

	ts->state = ELAN_STATE_NORMAL;
	//enable_irq(client->irq);
	elan_set_irq_status(client->irq, 1);

	return 0;
}

static SIMPLE_DEV_PM_OPS(elants_i2c_pm_ops, elants_i2c_suspend,
			 elants_i2c_resume);

static const struct i2c_device_id elants_i2c_id[] = {
	{ DEVICE_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, elants_i2c_id);

#ifdef CONFIG_ACPI
static const struct acpi_device_id elants_acpi_id[] = {
	{ "ELAN0001", 0 },
	{ }
};
MODULE_DEVICE_TABLE(acpi, elants_acpi_id);
#endif

#ifdef CONFIG_OF
static const struct of_device_id elants_of_match[] = {
	{ .compatible = "elan,ekth3500" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, elants_of_match);
#endif

static struct i2c_driver elants_i2c_driver = {
	.probe = elants_i2c_probe,
	.remove = elants_i2c_remove,
	.id_table = elants_i2c_id,
	.driver = {
		.name = DEVICE_NAME,
		.pm = &elants_i2c_pm_ops,
		.acpi_match_table = ACPI_PTR(elants_acpi_id),
		.of_match_table = of_match_ptr(elants_of_match),
		.probe_type = PROBE_PREFER_ASYNCHRONOUS, // Linux-4.2 or higher
	},
};
module_i2c_driver(elants_i2c_driver);

MODULE_AUTHOR("Scott Liu <scott.liu@emc.com.tw>");
MODULE_DESCRIPTION("Elan I2c Touchscreen driver");
MODULE_VERSION(DRV_VERSION);
MODULE_LICENSE("GPL");
