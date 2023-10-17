/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _HIFI4DSP_FIRMWARE_H
#define _HIFI4DSP_FIRMWARE_H

struct firmware;
struct hifi4dsp_priv;
struct hifi4dsp_dsp;

struct hifi4dsp_firmware {
	int  id;
	char name[32];
	int  size;
	int  fmt;
	struct list_head list;	/*fw list of DSP*/
	phys_addr_t paddr;		/*physical address of fw data*/
	void	*buf;			/*virtual  address of fw data*/
	//struct hifi4dsp_priv *priv;
	struct hifi4dsp_dsp  *dsp;
	void *private;
};

int  hifi4dsp_fw_load(struct hifi4dsp_firmware *dsp_fw);
int  hifi4dsp_fw_sram_load(struct hifi4dsp_firmware *dsp_fw);
int  hifi4dsp_fw_unload(struct hifi4dsp_firmware *dsp_fw);
int  hifi4dsp_fw_add(struct hifi4dsp_firmware *dsp_fw);
struct hifi4dsp_firmware *hifi4dsp_fw_new(struct hifi4dsp_dsp *dsp,
					  const struct firmware *fw,
					  void *private);
struct hifi4dsp_firmware *hifi4dsp_fw_register(struct hifi4dsp_dsp *dsp,
					       char *fw_name);
int hifi4dsp_dump_memory(const void *buf, unsigned int bytes, int col);
void *get_hifi_fw_mem_type(void);
struct hifi4dsp_firmware *hifi4dsp_get_firmware(int dspid);
void hifi4dsp_shm_clean(int dspid, unsigned int paddr, unsigned int size);
void hifi4dsp_shm_invalidate(int dspid, unsigned int paddr, unsigned int size);

#endif /*_HIFI4DSP_FIRMWARE_H*/
