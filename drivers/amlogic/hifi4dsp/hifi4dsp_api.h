/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __AML_HIFI4DSP_API_H__
#define __AML_HIFI4DSP_API_H__

struct hifi4dsp_info_t {
	char id;			/*dsp_id 0,1,2...*/
	char fw_id;
	char fw_name[32];	/*name of firmware which used for dsp*/
	char fw1_name[32];	/*name of firmware which used for dsp ddr*/
	char fw2_name[32];	/*name of firmware which used for dsp sram*/
	unsigned int phy_addr;/*phy address of firmware wille be loaded on*/
	unsigned int size;		/*size of reserved hifi memory*/
} __packed;

#define HIFI4DSP_IOC_MAGIC  'H'

#define HIFI4DSP_LOAD	_IOWR(HIFI4DSP_IOC_MAGIC, 1, struct hifi4dsp_info_t)
#define HIFI4DSP_RESET	_IOWR(HIFI4DSP_IOC_MAGIC, 2, struct hifi4dsp_info_t)
#define HIFI4DSP_START	_IOWR(HIFI4DSP_IOC_MAGIC, 3, struct hifi4dsp_info_t)
#define HIFI4DSP_STOP	_IOWR(HIFI4DSP_IOC_MAGIC, 4, struct hifi4dsp_info_t)
#define HIFI4DSP_SLEEP	_IOWR(HIFI4DSP_IOC_MAGIC, 5, struct hifi4dsp_info_t)
#define HIFI4DSP_WAKE	_IOWR(HIFI4DSP_IOC_MAGIC, 6, struct hifi4dsp_info_t)
#define HIFI4DSP_2LOAD	_IOWR(HIFI4DSP_IOC_MAGIC, 7, struct hifi4dsp_info_t)

#define HIFI4DSP_GET_INFO	_IOWR((HIFI4DSP_IOC_MAGIC), (18), \
					struct hifi4dsp_info_t)

#define HIFI4DSP_TEST		_IO(HIFI4DSP_IOC_MAGIC, 255)

struct hifi4_shm_info_t {
	long addr;
	size_t size;
};

#define HIFI4DSP_SHM_CLEAN \
		_IOWR(HIFI4DSP_IOC_MAGIC, 64, struct hifi4_shm_info_t)
#define HIFI4DSP_SHM_INV _IOWR(HIFI4DSP_IOC_MAGIC, 65, struct hifi4_shm_info_t)

#endif /*__AML_AUDIO_HIFI4DSP_API_H__ */
