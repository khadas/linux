/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2023 Amlogic, Inc. All rights reserved.
 */

#ifndef _DSP_CLIENT_API_H
#define _DSP_CLIENT_API_H

/*cmd mode*/
#define MBX_SYSTEM                 0x0
#define MBX_CODEC                  0x1
#define MBX_TINYALSA               0x2
#define MBX_PIPELINE               0x3

/* 0x20 ~ 0x3F reserved for Customer */

/*sys cmd*/
#define CMD_SHM_ALLOC              0x3
#define CMD_SHM_FREE               0x4

/*AWE cmd*/
#define CMD_APROCESS_OPEN     0x34
#define CMD_APROCESS_CLOSE    0x35
#define CMD_APROCESS_START    0x36
#define CMD_APROCESS_STOP     0x37
#define CMD_APROCESS_READ     0x38
#define CMD_APROCESS_WRITE    0x39
#define CMD_APROCESS_WRITE_SPEAKER 0x3a
#define CMD_APROCESS_DQBUF    0x3c
#define CMD_APROCESS_QBUF     0x3d
#define CMD_APROCESS_GET_GAIN 0x3e
#define CMD_APROCESS_SET_GAIN 0x3f

/*tinyalsa cmd*/
#define TINYALSA_API_OPEN   0x4
#define TINYALSA_API_CLOSE  0x5
#define TINYALSA_API_WRITEI  0x6
#define TINYALSA_API_READI   0x7
#define TINYALSA_API_GETLATENCY   0x8

/*Message composition with module(6bits), function(10bits)*/
#define __MBX_COMPOSE_MSG(mod, func)           (((mod) << 10) | ((func) & 0x3FF))

/*Mssage composition*/
#define MBX_CMD_SHM_ALLOC        __MBX_COMPOSE_MSG(MBX_SYSTEM, CMD_SHM_ALLOC)
#define MBX_CMD_SHM_FREE        __MBX_COMPOSE_MSG(MBX_SYSTEM, CMD_SHM_FREE)

#define MBX_TINYALSA_OPEN	__MBX_COMPOSE_MSG(MBX_TINYALSA, TINYALSA_API_OPEN)
#define MBX_TINYALSA_CLOSE	__MBX_COMPOSE_MSG(MBX_TINYALSA, TINYALSA_API_CLOSE)
#define MBX_TINYALSA_WRITEI	__MBX_COMPOSE_MSG(MBX_TINYALSA, TINYALSA_API_WRITEI)
#define MBX_TINYALSA_READI	__MBX_COMPOSE_MSG(MBX_TINYALSA, TINYALSA_API_READI)
#define MBX_TINYALSA_GETLATENCY	__MBX_COMPOSE_MSG(MBX_TINYALSA, TINYALSA_API_GETLATENCY)

#define MBX_CMD_APROCESS_OPEN       __MBX_COMPOSE_MSG(MBX_SYSTEM, CMD_APROCESS_OPEN)
#define MBX_CMD_APROCESS_CLOSE      __MBX_COMPOSE_MSG(MBX_SYSTEM, CMD_APROCESS_CLOSE)
#define MBX_CMD_APROCESS_START      __MBX_COMPOSE_MSG(MBX_SYSTEM, CMD_APROCESS_START)
#define MBX_CMD_APROCESS_STOP       __MBX_COMPOSE_MSG(MBX_SYSTEM, CMD_APROCESS_STOP)
#define MBX_CMD_APROCESS_READ       __MBX_COMPOSE_MSG(MBX_SYSTEM, CMD_APROCESS_READ)
#define MBX_CMD_APROCESS_WRITE      __MBX_COMPOSE_MSG(MBX_SYSTEM, CMD_APROCESS_WRITE)
#define MBX_CMD_APROCESS_DQBUF	 __MBX_COMPOSE_MSG(MBX_SYSTEM, CMD_APROCESS_DQBUF)
#define MBX_CMD_APROCESS_QBUF	 __MBX_COMPOSE_MSG(MBX_SYSTEM, CMD_APROCESS_QBUF)
#define MBX_CMD_APROCESS_GET_GAIN   __MBX_COMPOSE_MSG(MBX_SYSTEM, CMD_APROCESS_GET_GAIN)
#define MBX_CMD_APROCESS_SET_GAIN   __MBX_COMPOSE_MSG(MBX_SYSTEM, CMD_APROCESS_SET_GAIN)
#define MBX_CMD_APROCESS_WRITE_SPEAKER   __MBX_COMPOSE_MSG(MBX_SYSTEM, CMD_APROCESS_WRITE_SPEAKER)

/*pcm*/
/** A flag that specifies that the PCM is an output.
 * May not be bitwise AND'd with @ref PCM_IN.
 * Used in @ref pcm_open.
 * @ingroup libtinyalsa-pcm
 */
#define PCM_OUT 0x00000000

/** Specifies that the PCM is an input.
 * May not be bitwise AND'd with @ref PCM_OUT.
 * Used in @ref pcm_open.
 * @ingroup libtinyalsa-pcm
 */
#define PCM_IN 0x10000000

/** Specifies that the PCM will use mmap read and write methods.
 * Used in @ref pcm_open.
 * @ingroup libtinyalsa-pcm
 */
#define PCM_MMAP 0x00000001

/** Specifies no interrupt requests.
 * May only be bitwise AND'd with @ref PCM_MMAP.
 * Used in @ref pcm_open.
 * @ingroup libtinyalsa-pcm
 */
#define PCM_NOIRQ 0x00000002

/** When set, calls to @ref pcm_write
 * for a playback stream will not attempt
 * to restart the stream in the case of an
 * underflow, but will return -EPIPE instead.
 * After the first -EPIPE error, the stream
 * is considered to be stopped, and a second
 * call to pcm_write will attempt to restart
 * the stream.
 * Used in @ref pcm_open.
 * @ingroup libtinyalsa-pcm
 */
#define PCM_NORESTART 0x00000004

/** Specifies monotonic timestamps.
 * Used in @ref pcm_open.
 * @ingroup libtinyalsa-pcm
 */
#define PCM_MONOTONIC 0x00000008

/** If used with @ref pcm_open and @ref pcm_params_get,
 * it will not cause the function to block if
 * the PCM is not available. It will also cause
 * the functions @ref pcm_readi and @ref pcm_writei
 * to exit if they would cause the caller to wait.
 * @ingroup libtinyalsa-pcm
 */
#define PCM_NONBLOCK 0x00000010

/** Means a PCM STATE MASK
 * @ingroup libtinyalsa-pcm
 */
#define PCM_STATE_MASK	0x000F

/** Means a PCM is prepared
 * @ingroup libtinyalsa-pcm
 */
#define PCM_STATE_PREPARED 0x02

/** For inputs, this means the PCM is recording audio samples.
 * For outputs, this means the PCM is playing audio samples.
 * @ingroup libtinyalsa-pcm
 */
#define PCM_STATE_RUNNING 0x03

/** For inputs, this means an overrun occurred.
 * For outputs, this means an underrun occurred.
 */
#define PCM_STATE_XRUN 0x04

/** For outputs, this means audio samples are played.
 * A PCM is in a draining state when it is coming to a stop.
 */
#define PCM_STATE_DRAINING 0x05

/** Means a PCM is suspended.
 * @ingroup libtinyalsa-pcm
 */
#define PCM_STATE_SUSPENDED 0x07

/** Means a PCM has been disconnected.
 * @ingroup libtinyalsa-pcm
 */
#define PCM_STATE_DISCONNECTED 0x08

/*tinyalsa*/
enum ALSA_DEVICE_IN {
	DEVICE_TDMIN_A	= 0,
	DEVICE_TDMIN_B	= 1,
	DEVICE_SPDIFIN	= 2,
	DEVICE_LOOPBACK	= 3,
	DEVICE_PDMIN	= 4
};

enum ALSA_DEVICE_OUT {
	DEVICE_TDMOUT_A	= 0,
	DEVICE_TDMOUT_B	= 1
};

enum LOOPBACK_BUF {
	PROCESSBUF = 0,
	RAWBUF,
};

struct rpc_pcm_config {
	/** The number of channels in a frame */
	u32 channels;
	/** The number of frames per second */
	u32 rate;
	/** The number of frames in a period */
	u32 period_size;
	/** The number of periods in a PCM */
	u32 period_count;
	/** The sample format of a PCM */
	int format;
	/* Values to use for the ALSA start, stop and silence thresholds.  Setting
	 * any one of these values to 0 will cause the default tinyalsa values to be
	 * used instead.  Tinyalsa defaults are as follows.
	 *
	 * start_threshold   : period_count * period_size
	 * stop_threshold    : period_count * period_size
	 * silence_threshold : 0
	 */
	/** The minimum number of frames required to start the PCM */
	u32 start_threshold;
	/** The minimum number of frames required to stop the PCM */
	u32 stop_threshold;
	/** The minimum number of frames to silence the PCM */
	u32 silence_threshold;
} __packed;

/** Enumeration of a PCM's hardware parameters.
 * Each of these parameters is either a mask or an interval.
 * @ingroup libtinyalsa-pcm
 */
struct pcm_open_st {
	u32 card;
	u32 device;
	u32 flags;
	struct rpc_pcm_config pcm_config; // dsp also could access this address
	u64 out_pcm;
} __packed;

struct pcm_close_st {
	u64 pcm;
	s32 out_ret;
} __packed;

struct aml_pcm_ctx {
	u64 pcm_srv_hdl;
	struct rpc_pcm_config config;
};

struct acodec_shm_alloc_st {
	u64 phy;
	u64 size;
	s32 pid;
} __packed;

struct acodec_shm_free_st {
	u64 phy;
} __packed;

struct pcm_io_st {
	u64 pcm;
	u64 data;    /*dsp need access this address*/
	u32 count;
	s32 out_ret;
} __packed;

struct pcm_process_open_st {
	u32 card;
	u32 device;
	u32 flags;
	struct rpc_pcm_config pcm_config;   /*dsp also could access this address*/
	u64 out_pcm;
} __packed;

struct pcm_process_close_st {
	u64 pcm;
	s32 out_ret;
} __packed;

struct pcm_process_buf_st {
	u64 pcm;
	u64 release_buf_handle;
	u64 buf_handle;
	u64 data;
	u32 count;
	u32 id;
	s32 out_ret;
} __packed;

struct pcm_process_gain_st {
	u64 pcm;
	u32 is_out;
	s32 gain;
	s32 out_ret;
} __packed;

struct aml_dsp_id {
	int value;
	int id;
};

/*loopback pcm operation*/
struct aml_pro_pcm_ctx {
	u64 pcm_srv_hdl;
	struct rpc_pcm_config config;
};

struct pcm_process_io_st {
	u64 pcm;
	u64 data;   /*dsp need access this address*/
	u32 count;
	u32 id;
	s32 out_ret;
} __packed;

enum DSP_PCM_FORMAT {
	/** Signed 16-bit, little endian */
	DSP_PCM_FORMAT_S16_LE = 0,
	/** Signed, 8-bit */
	DSP_PCM_FORMAT_S8 = 1,
	/** Signed, 16-bit, big endian */
	DSP_PCM_FORMAT_S16_BE = 2,
	/** Signed, 24-bit (32-bit in memory), little endian */
	DSP_PCM_FORMAT_S24_LE,
	/** Signed, 24-bit (32-bit in memory), big endian */
	DSP_PCM_FORMAT_S24_BE,
	/** Signed, 24-bit, little endian */
	DSP_PCM_FORMAT_S24_3LE,
	/** Signed, 24-bit, big endian */
	DSP_PCM_FORMAT_S24_3BE,
	/** Signed, 32-bit, little endian */
	DSP_PCM_FORMAT_S32_LE,
	/** Signed, 32-bit, big endian */
	DSP_PCM_FORMAT_S32_BE,
	/** Max of the enumeration list, not an actual format. */
	DSP_PCM_FORMAT_MAX
};

struct buf_info {
	u64 handle;
	u64 phyaddr;
	void *viraddr;
	u32 size;
};

u32 pcm_client_format_to_bytes(enum DSP_PCM_FORMAT format);
u32 pcm_client_frame_to_bytes(void *hdl, u32 frame);
void *pcm_client_open(u32 card, u32 device, u32 flags,
		struct rpc_pcm_config *config, struct device *dev, u32 dspid);
int pcm_client_close(void *hdl, struct device *dev, u32 dspid);
int pcm_client_writei(void *hdl, const phys_addr_t data, u32 count,
		struct device *dev, u32 dspid);
int pcm_client_readi(void *hdl, const phys_addr_t data, u32 count,
		struct device *dev, u32 dspid);
int pcm_process_client_writei_to_speaker(void *hdl, const phys_addr_t data, u32 count,
		u32 bypass, struct device *dev, u32 dspid);
void *pcm_process_client_open(u32 card, u32 device, u32 flags,
		struct rpc_pcm_config *config, struct device *dev, u32 dspid);
int pcm_process_client_close(void        *hdl, struct device *dev, u32 dspid);
int pcm_process_client_start(void *hdl, struct device *dev, u32 dspid);
int pcm_process_client_stop(void *hdl, struct device *dev, u32 dspid);
int pcm_process_client_dqbuf(void *hdl, struct buf_info *buf, struct buf_info *release_buf,
		u32 type, struct device *dev, u32 dspid);
int pcm_process_client_qbuf(void *hdl, struct buf_info *buf, u32 type,
		struct device *dev, u32 dspid);
int pcm_process_client_get_volume_gain(void *hdl, int *gain,
		int is_out, struct device *dev, u32 dspid);
int pcm_process_client_set_volume_gain(void *hdl, int gain,
		int is_out, struct device *dev, u32 dspid);
void *aml_dsp_mem_allocate(phys_addr_t *phy, size_t size, struct device *dev, u32 dspid);
void aml_dsp_mem_free(phys_addr_t phy, struct device *dev, u32 dspid);

void *audio_device_open(u32 card, u32 device, u32 flags,
		struct rpc_pcm_config *config, struct device *dev, u32 dspid);

#endif /*_DSP_CLIENT_API_H*/
