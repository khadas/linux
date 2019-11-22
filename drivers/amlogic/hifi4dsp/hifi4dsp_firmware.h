/*
 * drivers/amlogic/hifi4dsp/hifi4dsp_firmware.h
 *
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
 */

#ifndef _HIFI4DSP_FIRMWARE_H
#define _HIFI4DSP_FIRMWARE_H

struct firmware;
struct hifi4dsp_priv;
struct hifi4dsp_dsp;

struct hifi4dsp_firmware_block {
	int id;
	struct list_head list;
	phys_addr_t paddr;
};

struct hifi4dsp_firmware {
	int  id;
	char name[32];
	int  size;
	int  fmt;
	struct list_head list;	/*fw list of DSP*/
	phys_addr_t paddr;		/*physical address of fw data*/
	void	*buf;			/*virtual  address of fw data*/

	struct hifi4dsp_priv *priv;
	struct hifi4dsp_dsp  *dsp;

	void *private;
};

extern int  hifi4dsp_fw_load(struct hifi4dsp_firmware *dsp_fw);
extern int  hifi4dsp_fw_unload(struct hifi4dsp_firmware *dsp_fw);
extern void hifi4dsp_fw_free(struct hifi4dsp_firmware *dsp_fw);
extern int  hifi4dsp_fw_add(struct hifi4dsp_firmware *dsp_fw);
extern void hifi4dsp_fw_free_all(struct hifi4dsp_dsp *dsp);
extern struct hifi4dsp_firmware *hifi4dsp_fw_new(struct hifi4dsp_dsp *dsp,
			       const struct firmware *fw, void *private);
extern struct hifi4dsp_firmware *hifi4dsp_fw_register(struct hifi4dsp_dsp *dsp,
					char *fw_name);
extern int hifi4dsp_dump_memory(const void *buf, unsigned int bytes, int col);


#endif /*_HIFI4DSP_FIRMWARE_H*/
