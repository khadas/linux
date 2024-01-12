// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2023 Amlogic, Inc. All rights reserved.
 */

#include <linux/types.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/amlogic/meson_mhu_common.h>
#include <linux/amlogic/scpi_protocol.h>
#include "dsp_client_api.h"
#include "bridge_common.h"

int Mbox_invoke(struct device *dev, u32 dspid, int cmd, void *data, u32 len)
{
	int ret = 0;

	ret = mbox_message_send_data_sync(dev, cmd, data, len, dspid);
	if (ret < 0)
		dev_err(dev, "Mbox invoke write error: %d\n", ret);
	return ret;
}

u32 pcm_client_format_to_bytes(enum DSP_PCM_FORMAT format)
{
	switch (format) {
	case DSP_PCM_FORMAT_S32_LE:
	case DSP_PCM_FORMAT_S32_BE:
	case DSP_PCM_FORMAT_S24_LE:
	case DSP_PCM_FORMAT_S24_BE:
		return 32 >> 3;		/*4 Bytes*/
	case DSP_PCM_FORMAT_S24_3LE:
	case DSP_PCM_FORMAT_S24_3BE:
		return 24 >> 3;		/*3 Bytes*/
	default:
	case DSP_PCM_FORMAT_S16_LE:
	case DSP_PCM_FORMAT_S16_BE:
		return 16 >> 3;		/*2 Bytes*/
	case DSP_PCM_FORMAT_S8:
		return 8 >> 3;		/*1 Bytes*/
	};
}

u32 pcm_client_frame_to_bytes(void           *hdl, u32 frame)
{
	struct aml_pcm_ctx *p_aml_pcm_ctx = (struct aml_pcm_ctx *)hdl;
	int bytes_per_sample = pcm_client_format_to_bytes(p_aml_pcm_ctx->config.format);

	return (frame * bytes_per_sample * (p_aml_pcm_ctx->config.channels));
}

void *pcm_client_open(u32 card, u32 device, u32 flags, struct rpc_pcm_config *config,
		struct device *dev, u32 dspid)
{
	struct pcm_open_st arg;
	struct aml_pcm_ctx *p_aml_pcm_ctx = kmalloc(sizeof(*p_aml_pcm_ctx), GFP_KERNEL);

	memset(p_aml_pcm_ctx, 0, sizeof(struct aml_pcm_ctx));
	memset(&arg, 0, sizeof(arg));
	arg.card = card;
	arg.device = device;
	arg.flags = flags;
	memcpy(&arg.pcm_config, config, sizeof(struct rpc_pcm_config));
	arg.out_pcm = 0;

	Mbox_invoke(dev, dspid, MBX_TINYALSA_OPEN, &arg, sizeof(arg));
	if (!arg.out_pcm) {
		kfree(p_aml_pcm_ctx);
		return NULL;
	}
	p_aml_pcm_ctx->pcm_srv_hdl = arg.out_pcm;
	memcpy(&p_aml_pcm_ctx->config, config, sizeof(struct rpc_pcm_config));
	return p_aml_pcm_ctx;
}

int pcm_client_close(void        *hdl, struct device *dev, u32 dspid)
{
	struct pcm_close_st arg;
	struct aml_pcm_ctx *p_aml_pcm_ctx = (struct aml_pcm_ctx *)hdl;

	memset(&arg, 0, sizeof(arg));
	arg.pcm = p_aml_pcm_ctx->pcm_srv_hdl;
	arg.out_ret = -EINVAL;

	Mbox_invoke(dev, dspid, MBX_TINYALSA_CLOSE, &arg, sizeof(arg));

	kfree(p_aml_pcm_ctx);
	return arg.out_ret;
}

int pcm_client_writei(void *hdl, const phys_addr_t data, u32 count,
		struct device *dev, u32 dspid)
{
	struct pcm_io_st arg;
	struct aml_pcm_ctx *p_aml_pcm_ctx = (struct aml_pcm_ctx *)hdl;

	memset(&arg, 0, sizeof(arg));
	arg.pcm = p_aml_pcm_ctx->pcm_srv_hdl;
	arg.data = (u64)data;
	arg.count = count;
	arg.out_ret = 0;

	Mbox_invoke(dev, dspid, MBX_TINYALSA_WRITEI, &arg, sizeof(arg));

	return arg.out_ret;
}

int pcm_process_client_writei_to_speaker(void *hdl, const phys_addr_t data, u32 count,
		u32 bypass, struct device *dev, u32 dspid)
{
	struct pcm_process_io_st arg;
	struct aml_pcm_ctx *p_aml_pcm_ctx = (struct aml_pcm_ctx *)hdl;

	memset(&arg, 0, sizeof(arg));
	arg.pcm = p_aml_pcm_ctx->pcm_srv_hdl;
	arg.data = (u64)data;
	arg.count = count;
	arg.out_ret = 0;
	arg.id = bypass;

	Mbox_invoke(dev, dspid, MBX_CMD_APROCESS_WRITE_SPEAKER, &arg, sizeof(arg));

	return arg.out_ret;
}

int pcm_client_readi(void *hdl, const phys_addr_t data, u32 count,
		struct device *dev, u32 dspid)
{
	struct pcm_io_st arg;
	struct aml_pcm_ctx *p_aml_pcm_ctx = (struct aml_pcm_ctx *)hdl;

	memset(&arg, 0, sizeof(arg));
	arg.pcm = p_aml_pcm_ctx->pcm_srv_hdl;
	arg.data = (u64)data;
	arg.count = count;
	arg.out_ret = 0;

	Mbox_invoke(dev, dspid, MBX_TINYALSA_READI, &arg, sizeof(arg));

	return arg.out_ret;
}

void *pcm_process_client_open(u32 card, u32 device, u32 flags,
		struct rpc_pcm_config *config, struct device *dev, u32 dspid)
{
	struct pcm_process_open_st arg;
	struct aml_pro_pcm_ctx *p_aml_pcm_ctx = kmalloc(sizeof(*p_aml_pcm_ctx), GFP_KERNEL);

	memset(p_aml_pcm_ctx, 0, sizeof(struct aml_pro_pcm_ctx));
	memset(&arg, 0, sizeof(arg));
	arg.card = card;
	arg.device = device;
	arg.flags = flags;
	memcpy(&arg.pcm_config, config, sizeof(struct rpc_pcm_config));
	arg.out_pcm = 0;

	Mbox_invoke(dev, dspid, MBX_CMD_APROCESS_OPEN, &arg, sizeof(arg));
	if (!arg.out_pcm) {
		kfree(p_aml_pcm_ctx);
		return NULL;
	}
	p_aml_pcm_ctx->pcm_srv_hdl = arg.out_pcm;
	memcpy(&p_aml_pcm_ctx->config, config, sizeof(struct rpc_pcm_config));
	return p_aml_pcm_ctx;
}

int pcm_process_client_close(void        *hdl, struct device *dev, u32 dspid)
{
	struct pcm_process_close_st arg;
	struct aml_pro_pcm_ctx *p_aml_pcm_ctx = (struct aml_pro_pcm_ctx *)hdl;

	memset(&arg, 0, sizeof(arg));
	arg.pcm = p_aml_pcm_ctx->pcm_srv_hdl;
	arg.out_ret = -EINVAL;

	Mbox_invoke(dev, dspid, MBX_CMD_APROCESS_CLOSE, &arg, sizeof(arg));

	kfree(p_aml_pcm_ctx);
	return arg.out_ret;
}

int pcm_process_client_start(void *hdl, struct device *dev, u32 dspid)
{
	struct pcm_process_io_st arg;
	struct aml_pro_pcm_ctx *p_aml_pcm_ctx = (struct aml_pro_pcm_ctx *)hdl;

	memset(&arg, 0, sizeof(arg));
	arg.pcm = p_aml_pcm_ctx->pcm_srv_hdl;
	arg.out_ret = -EINVAL;

	Mbox_invoke(dev, dspid, MBX_CMD_APROCESS_START, &arg, sizeof(arg));

	return arg.out_ret;
}

int pcm_process_client_stop(void *hdl, struct device *dev, u32 dspid)
{
	struct pcm_process_io_st arg;
	struct aml_pro_pcm_ctx *p_aml_pcm_ctx = (struct aml_pro_pcm_ctx *)hdl;

	memset(&arg, 0, sizeof(arg));
	arg.pcm = p_aml_pcm_ctx->pcm_srv_hdl;
	arg.out_ret = -EINVAL;

	Mbox_invoke(dev, dspid, MBX_CMD_APROCESS_STOP, &arg, sizeof(arg));

	return arg.out_ret;
}

int pcm_process_client_dqbuf(void *hdl, struct buf_info *buf, struct buf_info *release_buf,
		u32 type, struct device *dev, u32 dspid)
{
	struct aml_pro_pcm_ctx *p_aml_pcm_ctx = (struct aml_pro_pcm_ctx *)hdl;
	struct pcm_process_buf_st arg;

	if (type == PROCESSBUF)
		arg.id = 0;
	else
		arg.id = 1;

	arg.pcm = p_aml_pcm_ctx->pcm_srv_hdl;
	arg.buf_handle = 0;
	arg.data = 0;
	arg.count = buf->size;
	arg.out_ret = -EINVAL;
	if (release_buf && release_buf->handle &&
		release_buf->phyaddr && release_buf->viraddr &&
		release_buf->size &&
		release_buf->viraddr == __va((phys_addr_t)release_buf->phyaddr))
		arg.release_buf_handle = release_buf->handle;
	else
		arg.release_buf_handle = 0;

	Mbox_invoke(dev, dspid, CMD_APROCESS_DQBUF, &arg, sizeof(arg));

	buf->handle = arg.buf_handle;
	buf->phyaddr = arg.data;
	buf->size = arg.count;

	if (arg.buf_handle)
		buf->viraddr = __va((phys_addr_t)buf->phyaddr);

	return arg.out_ret;
}

int pcm_process_client_qbuf(void *hdl, struct buf_info *buf, u32 type,
		struct device *dev, u32 dspid)
{
	struct aml_pro_pcm_ctx *p_aml_pcm_ctx = (struct aml_pro_pcm_ctx *)hdl;
	struct pcm_process_buf_st arg;

	if (type == PROCESSBUF)
		arg.id = 0;
	else
		arg.id = 1;
	arg.pcm = p_aml_pcm_ctx->pcm_srv_hdl;
	arg.buf_handle = buf->handle;
	arg.data = buf->phyaddr;
	arg.count = buf->size;
	arg.out_ret = -EINVAL;

	Mbox_invoke(dev, dspid, CMD_APROCESS_QBUF, &arg, sizeof(arg));

	buf->handle = arg.buf_handle;
	buf->phyaddr = arg.data;
	buf->size = arg.count;

	if (arg.buf_handle)
		buf->viraddr = __va((phys_addr_t)buf->phyaddr);

	return arg.out_ret;
}

int pcm_process_client_get_volume_gain(void *hdl, int *gain,
		int is_out, struct device *dev, u32 dspid)
{
	struct aml_pro_pcm_ctx *p_aml_pcm_ctx = (struct aml_pro_pcm_ctx *)hdl;
	struct pcm_process_gain_st arg;

	arg.pcm = p_aml_pcm_ctx->pcm_srv_hdl;
	arg.gain = 0;
	arg.is_out = is_out;
	arg.out_ret = -EINVAL;

	Mbox_invoke(dev, dspid, CMD_APROCESS_GET_GAIN, &arg, sizeof(arg));

	if (!arg.out_ret)
		*gain = arg.gain;
	return arg.out_ret;
}

int pcm_process_client_set_volume_gain(void *hdl, int gain,
		int is_out, struct device *dev, u32 dspid)
{
	struct aml_pro_pcm_ctx *p_aml_pcm_ctx = (struct aml_pro_pcm_ctx *)hdl;
	struct pcm_process_gain_st arg;

	arg.pcm = p_aml_pcm_ctx->pcm_srv_hdl;
	arg.gain = gain;
	arg.is_out = is_out;
	arg.out_ret = -EINVAL;

	Mbox_invoke(dev, dspid, CMD_APROCESS_SET_GAIN, &arg, sizeof(arg));

	return arg.out_ret;
}

void *aml_dsp_mem_allocate(phys_addr_t *phy, size_t size,
		struct device *dev, u32 dspid)
{
	struct acodec_shm_alloc_st arg;

	arg.size = size;
	arg.pid = 0;
	arg.phy = 0;

	Mbox_invoke(dev, dspid, MBX_CMD_SHM_ALLOC, &arg, sizeof(arg));
	if (!arg.phy)
		return NULL;
	*phy = (phys_addr_t)arg.phy;
	return __va((phys_addr_t)arg.phy);
}

void aml_dsp_mem_free(phys_addr_t phy, struct device *dev, u32 dspid)
{
	struct acodec_shm_free_st arg;

	arg.phy = (u64)phy;

	Mbox_invoke(dev, dspid, MBX_CMD_SHM_FREE, &arg, sizeof(arg));
}

void *audio_device_open(u32 card, u32 device, u32 flags,
		struct rpc_pcm_config *config, struct device *dev, u32 dspid)
{
	void *phandle = NULL;

	if (flags & PCM_IN) {
		config->start_threshold = 0;
		config->silence_threshold = 0;
		config->stop_threshold = 0;
		phandle = pcm_process_client_open(card, device, PCM_IN, config, dev, dspid);
		if (!phandle) {
			pr_err("pcm client open fail\n");
			return NULL;
		}
	} else {
		config->start_threshold = 0;
		config->silence_threshold = 0;
		config->stop_threshold = 0;
		phandle = pcm_process_client_open(card, device, PCM_OUT, config, dev, dspid);
		if (!phandle) {
			pr_err("pcm client open fail\n");
			return NULL;
		}
	}
	return phandle;
}
