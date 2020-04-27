// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/err.h>
#include <linux/export.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/mailbox_client.h>
#include <linux/amlogic/scpi_protocol.h>
#include <linux/slab.h>
#include "meson_mhu.h"

#define CMD_TAG			(2 << 25)

struct scpi_data_buf {
	int client_id;
	struct mhu_data_buf *data;
	struct completion complete;
};

enum scpi_error_codes {
	SCPI_SUCCESS = 0, /* Success */
	SCPI_ERR_PARAM = 1, /* Invalid parameter(s) */
	SCPI_ERR_ALIGN = 2, /* Invalid alignment */
	SCPI_ERR_SIZE = 3, /* Invalid size */
	SCPI_ERR_HANDLER = 4, /* Invalid handler/callback */
	SCPI_ERR_ACCESS = 5, /* Invalid access/permission denied */
	SCPI_ERR_RANGE = 6, /* Value out of range */
	SCPI_ERR_TIMEOUT = 7, /* Timeout has occurred */
	SCPI_ERR_NOMEM = 8, /* Invalid memory area or pointer */
	SCPI_ERR_PWRSTATE = 9, /* Invalid power state */
	SCPI_ERR_SUPPORT = 10, /* Not supported or disabled */
	SCPI_ERR_DEVICE = 11, /* Device error */
	SCPI_ERR_MAX
};

static int scpi_linux_errmap[SCPI_ERR_MAX] = {
	0, -EINVAL, -ENOEXEC, -EMSGSIZE,
	-EINVAL, -EACCES, -ERANGE, -ETIMEDOUT,
	-ENOMEM, -EINVAL, -EOPNOTSUPP, -EIO,
};

struct hifi4syslog hifi4logbuffer[2];
EXPORT_SYMBOL(hifi4logbuffer);

static inline int scpi_to_linux_errno(int errno)
{
	if (errno >= SCPI_SUCCESS && errno < SCPI_ERR_MAX)
		return scpi_linux_errmap[errno];
	return -EIO;
}

static void scpi_rx_callback(struct mbox_client *cl, void *msg)
{
	struct mhu_data_buf *data = (struct mhu_data_buf *)msg;
	struct scpi_data_buf *scpi_buf = data->cl_data;

	complete(&scpi_buf->complete);
}

static int send_scpi_cmd(struct scpi_data_buf *scpi_buf, int client_id)
{
	struct mbox_chan *chan;
	struct mbox_client cl = {0};
	struct mhu_data_buf *data = scpi_buf->data;
	u32 status;

	cl.dev = dsp_scpi_device;
	cl.rx_callback = scpi_rx_callback;
	pr_err("dsp %p\n", dsp_scpi_device);
	chan = mbox_request_channel(&cl, client_id);
	if (IS_ERR(chan))
		return PTR_ERR(chan);

	init_completion(&scpi_buf->complete);
	if (mbox_send_message(chan, (void *)data) < 0) {
		status = SCPI_ERR_TIMEOUT;
		goto free_channel;
	}

	wait_for_completion(&scpi_buf->complete);
	status = *(u32 *)(data->rx_buf); /* read first word */

free_channel:
	mbox_free_channel(chan);

	return scpi_to_linux_errno(status);
}

#define SCPI_SETUP_DBUF_SIZE(scpi_buf, mhu_buf, _client_id,\
			_cmd, _tx_buf, _tx_size, _rx_buf, _rx_size) \
do {						\
	struct mhu_data_buf *pdata = &(mhu_buf);\
	struct scpi_data_buf *psdata = &(scpi_buf); \
	pdata->cmd = _cmd;			\
	pdata->tx_buf = _tx_buf;		\
	pdata->tx_size = _tx_size;		\
	pdata->rx_buf = _rx_buf;		\
	pdata->rx_size = _rx_size;		\
	psdata->client_id = _client_id;		\
	psdata->data = pdata;			\
} while (0)

static int scpi_execute_cmd(struct scpi_data_buf *scpi_buf)
{
	struct mhu_data_buf *data;

	if (!scpi_buf || !scpi_buf->data)
		return -EINVAL;
	data = scpi_buf->data;
	data->cmd = (data->cmd & 0xffff)
		    | (data->tx_size & 0x1ff) << 16
		    | CMD_TAG; //Sync Flag
	data->cl_data = scpi_buf;

	return send_scpi_cmd(scpi_buf, scpi_buf->client_id);
}

int scpi_send_dsp_data(void *data, int size, bool to_dspa)
{
	struct scpi_data_buf sdata;
	struct mhu_data_buf mdata;
	int client_id = 0;

	struct __packed {
		u32 status;
	} buf;

	if (!to_dspa)
		client_id = 3;
	else
		client_id = 1;

	SCPI_SETUP_DBUF_SIZE(sdata, mdata, client_id,
			     SCPI_CMD_SEND_DSP_DATA, data,
			     size, &buf, sizeof(buf));
	if (scpi_execute_cmd(&sdata))
		return -EPERM;
	return buf.status;
}
EXPORT_SYMBOL_GPL(scpi_send_dsp_data);

int scpi_send_dsp_cmd(void *data, int size, bool to_dspa,
		      int cmd, int taskid)
{
	struct scpi_data_buf sdata;
	struct mhu_data_buf mdata;
	int client_id = 0;

	struct __packed {
		u32 status;
	} buf;

	struct txbuf {
		u64 taskid;
		char data[240];
	} txbuf;

	if (!to_dspa)
		client_id = 3;
	else
		client_id = 1;

	txbuf.taskid = taskid;
	size = size > sizeof(txbuf.data) ? sizeof(txbuf.data) : size;
	memcpy(txbuf.data, data, size);

	SCPI_SETUP_DBUF_SIZE(sdata, mdata, client_id,
			     cmd, (void *)&txbuf,
			sizeof(txbuf), &buf, sizeof(buf));
	if (scpi_execute_cmd(&sdata))
		return -EPERM;
	return buf.status;
}
EXPORT_SYMBOL_GPL(scpi_send_dsp_cmd);

int scpi_req_handle(void *p, u32 size, u32 cmd, int dspid)
{
	int ret = 0;
	struct hifi4syslog *phifilogbuf = NULL;

	if (!p)
		return 0;

	switch (cmd) {
	case SCPI_REQ_INVALID:
		break;
	case SCPI_CMD_HIFI4SYSTLOG:
		phifilogbuf = (struct hifi4syslog *)p;
		strcpy(hifi4logbuffer[dspid].syslogstate,
		       phifilogbuf->syslogstate);
		hifi4logbuffer[dspid].logbaseaddr = phifilogbuf->logbaseaddr;
		hifi4logbuffer[dspid].syslogsize = phifilogbuf->syslogsize;
		hifi4logbuffer[dspid].loghead = phifilogbuf->loghead;
		hifi4logbuffer[dspid].logtail = phifilogbuf->logtail;
		ret = 1;
		break;
	default:
		break;
	}
	return ret;
}

