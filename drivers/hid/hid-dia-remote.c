/*
   HID driver for the Android TV remote
   providing keys and microphone audio functionality

  Copyright (C) 2014 Google, Inc.

  This software is licensed under the terms of the GNU General Public
  License version 2, as published by the Free Software Foundation, and
  may be copied, distributed, and modified under those terms.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
 */

#include <linux/device.h>
#include <linux/module.h>
#include <linux/hid.h>
#include <linux/hiddev.h>
#include <linux/hardirq.h>
#include <linux/jiffies.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/wait.h>
#include <linux/timer.h>
#include <linux/spinlock.h>
#include <linux/vmalloc.h>
#include <linux/proc_fs.h>
#include <sound/core.h>
#include <sound/control.h>
#include <sound/info.h>
#include <sound/initval.h>
#include <sound/pcm.h>
#include <linux/delay.h>
#include "hid-ids.h"

MODULE_LICENSE("GPL v2");

/* Driver names (maximum sizes may depend on kernel version) */
#define DIA_REMOTE_HID_DRIVER_NAME	"DIA Motion and Audio remote"
#define DIA_REMOTE_AUDIO_DEVICE_ID	"DIAAudio" /* max=16 */
		/* Used by Audio HAL (/proc/asound/card<n>/id) */
		/* to identify the virtual sound card. */
#define DIA_REMOTE_AUDIO_DRIVER_NAME "DIA Audio" /* max=16 */
#define DIA_REMOTE_AUDIO_DEVICE_NAME_SHORT "DIAAudio" /* max=32 */
/* max=80 */
#define DIA_REMOTE_AUDIO_DEVICE_NAME_LONG "DIALOG Remote %i audio"

#define DIA_REMOTE_PCM_DEVICE_ID	"DIALOG PCM AUDIO" /* max=64 */
#define DIA_REMOTE_PCM_DEVICE_NAME	"DIA PCM" /* max=80 */

/* Supported devices GAP names */
static const char * const supported_devices[] = {
	"DA14582 M&VRemote",
	"DA14582 IR&M&VRemote",
	"DA1458x RCU",
	"BleRemote8",
	"RemoteB008",
	"Remote"
};

#define snd_dialog_log(...) pr_err("snd_dia: " __VA_ARGS__)




#define ADPCM_AUDIO_REPORT_ID_0 (5)
#define ADPCM_AUDIO_REPORT_ID_1 (6)
#define ADPCM_AUDIO_REPORT_ID_2 (7)
#define ADPCM_AUDIO_REPORT_ID_3 (8)

#define MSBC_AUDIO1_REPORT_ID 0xF7
#define MSBC_AUDIO2_REPORT_ID 0xFA
#define MSBC_AUDIO3_REPORT_ID 0xFB

#define INPUT_REPORT_ID 2

/* defaults */
#define MAX_PCM_DEVICES     1
#define MAX_PCM_SUBSTREAMS  4
#define MAX_MIDI_DEVICES    0

/* Define these all in one place so they stay in sync. */
#define USE_RATE_MIN          16000
#define USE_RATE_MAX          16000
#define USE_RATES_ARRAY      {USE_RATE_MIN}
/* only this enum matters for ALSA PCM INFO*/
#define USE_RATES_MASK       (SNDRV_PCM_RATE_16000)

#ifndef DECODER_BITS_PER_SAMPLE
#define DECODER_BITS_PER_SAMPLE         3
#endif
#define DECODER_BUFFER_LATENCY_MS       200
#define DECODER_MAX_ADJUST_FRAMES       USE_RATE_MAX

#define MAX_FRAMES_PER_BUFFER  (16384) /* doubling the buffer */

#define USE_CHANNELS_MIN   1
#define USE_CHANNELS_MAX   1
#define USE_PERIODS_MIN    1
#define USE_PERIODS_MAX    4

#define MAX_PCM_BUFFER_SIZE  (MAX_FRAMES_PER_BUFFER * sizeof(int16_t))
#define MIN_PERIOD_SIZE      2
#define MAX_PERIOD_SIZE      (MAX_FRAMES_PER_BUFFER * sizeof(int16_t))
#define USE_FORMATS          (SNDRV_PCM_FMTBIT_S16_LE)

#define PACKET_TYPE_ADPCM 0

struct fifo_packet {
	uint8_t  type;
	uint8_t  num_bytes;
	uint8_t  start;
	uint8_t  reset;
	uint32_t pos;
	/* Expect no more than 20 bytes. But align struct size to power of 2. */
	uint8_t  raw_data[24];
};

#define MAX_SAMPLES_PER_PACKET 128
#define MIN_SAMPLES_PER_PACKET_P2  32
#define MAX_PACKETS_PER_BUFFER  \
		(MAX_FRAMES_PER_BUFFER / MIN_SAMPLES_PER_PACKET_P2)
#define MAX_BUFFER_SIZE  \
		(MAX_PACKETS_PER_BUFFER * sizeof(struct fifo_packet))

#define SND_ATVR_RUNNING_TIMEOUT_MSEC    (500)

#define TIMER_STATE_UNINIT           255
#define TIMER_STATE_BEFORE_DECODE    0
#define TIMER_STATE_DURING_DECODE    1
#define TIMER_STATE_AFTER_DECODE     2

static int packet_counter;
static int num_remotes;
static bool card_created;
static int dev;
static int index[SNDRV_CARDS] = SNDRV_DEFAULT_IDX;  /* Index 0-MAX */
static char *id[SNDRV_CARDS] = SNDRV_DEFAULT_STR;   /* ID for this card */
static bool enable[SNDRV_CARDS] = {true, false};
/* Linux does not like NULL initialization. */
static char *model[SNDRV_CARDS]; /* = {[0 ... (SNDRV_CARDS - 1)] = NULL}; */
static int pcm_devs[SNDRV_CARDS] = {[0 ... (SNDRV_CARDS - 1)] = 1};
static int pcm_substreams[SNDRV_CARDS] = {[0 ... (SNDRV_CARDS - 1)] = 1};

module_param_array(index, int, NULL, 0444);
MODULE_PARM_DESC(index, "Index value for DIALOG Remote soundcard.");
module_param_array(id, charp, NULL, 0444);
MODULE_PARM_DESC(id, "ID string for DIALOG Remote soundcard.");
module_param_array(enable, bool, NULL, 0444);
MODULE_PARM_DESC(enable, "Enable this DIALOG Remote soundcard.");
module_param_array(model, charp, NULL, 0444);
MODULE_PARM_DESC(model, "Soundcard model.");
module_param_array(pcm_devs, int, NULL, 0444);
MODULE_PARM_DESC(pcm_devs, "PCM devices # (0-4) for DIALOG Remote driver.");
module_param_array(pcm_substreams, int, NULL, 0444);
MODULE_PARM_DESC(pcm_substreams,
	"PCM substreams # (1-128) for DIALOG Remote driver?");

/* Debug feature to save captured raw and decoded audio into buffers
  and make them available for reading from misc devices.
  It will record the last session only and only up to the buffer size.
  The recording is cleared on read.
 */
#define DEBUG_WITH_MISC_DEVICE 0

/* Debug feature to trace audio packets being received */
#define DEBUG_AUDIO_RECEPTION 1

/* Debug feature to trace HID reports we see */
#define DEBUG_HID_RAW_INPUT (0)

/* Driver data (virtual sound card private data) */
struct snd_dialog *s_dia_snd;

/*
  Static substream is needed so Bluetooth can pass encoded audio
  to a running stream.
  This also serves to enable or disable the decoding of audio in the callback.
 */
static struct snd_pcm_substream *s_substream_for_btle;
static DEFINE_SPINLOCK(s_substream_lock);

struct simple_atomic_fifo {
	/* Read and write cursors are modified by different threads. */
	uint read_cursor;
	uint write_cursor;
	/* Size must be a power of two. */
	uint size;
	/* internal mask is 2*size - 1
	 * This allows us to tell the difference between full and empty. */
	uint internal_mask;
	uint external_mask;
};

struct snd_dialog {
	struct snd_card *card;
	struct snd_pcm *pcm;
	struct snd_pcm_hardware pcm_hw;

	uint32_t sample_rate;

	uint previous_jiffies; /* Used to detect underflows. */
	uint timeout_jiffies;
	struct timer_list decoding_timer;
	uint timer_state;
	bool timer_enabled;
	uint timer_callback_count;
	uint32_t decoder_buffer_size;
	uint32_t num_bytes;
	uint32_t buffer_start_pos;
	uint32_t stream_enable_pos;
	uint32_t stream_disable_pos;
	uint32_t stream_reset_pos;
	uint32_t decoder_pos;
	int32_t decoder_frames_adjust;

	int16_t peak_level;
	struct simple_atomic_fifo fifo_controller;
	struct fifo_packet *fifo_packet_buffer;

	/* IMA/DVI ADPCM Decoder */
	int pcm_value;
	int step_index;

	/*
	  Write_index is the circular buffer position.
	  It is advanced by the BTLE thread after decoding.
	  It is read by ALSA in snd_dialog_pcm_pointer().
	  It is not declared volatile because that is not
	  allowed in the Linux kernel.
	 */
	uint32_t write_index;
	uint32_t frames_per_buffer;
	/* count frames generated so far in this period */
	uint32_t frames_in_period;
	int16_t *pcm_buffer;

};

/***************************************************************************/
/************* Atomic FIFO *************************************************/
/***************************************************************************/
/*
  This FIFO is atomic if used by no more than 2 threads.
  One thread modifies the read cursor and the other
  thread modifies the write_cursor.
  Size and mask are not modified while being used.

  The read and write cursors range internally from 0 to (2*size)-1.
  This allows us to tell the difference between full and empty.
  When we get the cursors for external use we mask with size-1.

  Memory barriers required on SMP platforms.
 */
static int atomic_fifo_init(struct simple_atomic_fifo *fifo_ptr, uint size)
{
	/* Make sure size is a power of 2. */
	if ((size & (size-1)) != 0) {
		pr_err("%s:%d - ERROR FIFO size = %d, not power of 2!\n",
			__func__, __LINE__, size);
		return -EINVAL;
	}
	fifo_ptr->read_cursor = 0;
	fifo_ptr->write_cursor = 0;
	fifo_ptr->size = size;
	fifo_ptr->internal_mask = (size * 2) - 1;
	fifo_ptr->external_mask = size - 1;
	smp_wmb(); /*   */
	return 0;
}


static uint atomic_fifo_available_to_read(struct simple_atomic_fifo *fifo_ptr)
{
	smp_rmb(); /*   */
	return (fifo_ptr->write_cursor - fifo_ptr->read_cursor)
			& fifo_ptr->internal_mask;
}

static uint atomic_fifo_available_to_write(struct simple_atomic_fifo *fifo_ptr)
{
	smp_rmb(); /*   */
	return fifo_ptr->size - atomic_fifo_available_to_read(fifo_ptr);
}

static void atomic_fifo_advance_read(
		struct simple_atomic_fifo *fifo_ptr,
		uint frames)
{
	smp_rmb(); /*   */
	BUG_ON(frames > atomic_fifo_available_to_read(fifo_ptr));
	fifo_ptr->read_cursor = (fifo_ptr->read_cursor + frames)
			& fifo_ptr->internal_mask;
	smp_wmb(); /*   */
}

static void atomic_fifo_advance_write(
		struct simple_atomic_fifo *fifo_ptr,
		uint frames)
{
	smp_rmb(); /*   */
	BUG_ON(frames > atomic_fifo_available_to_write(fifo_ptr));
	fifo_ptr->write_cursor = (fifo_ptr->write_cursor + frames)
		& fifo_ptr->internal_mask;
	smp_wmb(); /*   */
}

static uint atomic_fifo_get_read_index(struct simple_atomic_fifo *fifo_ptr)
{
	smp_rmb(); /*   */
	return fifo_ptr->read_cursor & fifo_ptr->external_mask;
}

static uint atomic_fifo_get_write_index(struct simple_atomic_fifo *fifo_ptr)
{
	smp_rmb(); /*   */
	return fifo_ptr->write_cursor & fifo_ptr->external_mask;
}

/****************************************************************************/
static void snd_dialog_handle_frame_advance(
		struct snd_pcm_substream *substream, uint num_frames)
{
	struct snd_dialog *dia_snd = snd_pcm_substream_chip(substream);

	dia_snd->frames_in_period += num_frames;
	/* Tell ALSA if we have advanced by one or more periods. */
	if (dia_snd->frames_in_period >= substream->runtime->period_size) {
		snd_pcm_period_elapsed(substream);
		dia_snd->frames_in_period %= substream->runtime->period_size;
	}
}

static uint32_t snd_dialog_bump_write_index(
			struct snd_pcm_substream *substream,
			uint32_t num_samples)
{
	struct snd_dialog *dia_snd = snd_pcm_substream_chip(substream);
	uint32_t pos = dia_snd->write_index;

	/* Advance write position. */
	pos += num_samples;
	/* Wrap around at end of the circular buffer. */
	pos %= dia_snd->frames_per_buffer;
	dia_snd->write_index = pos;

	snd_dialog_handle_frame_advance(substream, num_samples);

	return pos;
}

/*
  Decode an IMA/DVI ADPCM packet and write the PCM data into a circular buffer.
  ADPCM is 4:1 16kHz@256kbps -> 16kHz@64kbps.
  ADPCM is 4:1 8kHz@128kbps -> 8kHz@32kbps.
 */
static const int ima_index_table[16] =  {
		-1, -1, -1, -1,
		2, 4, 6, 8,
		-1, -1, -1, -1,
		2, 4, 6, 8};

static const int16_t ima_step_table[89] = {
	7, 8, 9, 10, 11, 12, 13, 14, 16, 17,
	19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
	50, 55, 60, 66, 73, 80, 88, 97, 107, 118,
	130, 143, 157, 173, 190, 209, 230, 253, 279, 307,
	337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
	876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066,
	2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358,
	5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
	15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
};

static void decode_adpcm_nibble(uint8_t nibble, struct snd_dialog *dia_snd,
				struct snd_pcm_substream *substream)
{
	int step_index = dia_snd->step_index;
	int value = dia_snd->pcm_value;
	int step = ima_step_table[step_index];
	int diff;

	/* pr_err("Decoding ADPCM nibble....."); */

/* diff = step >> 3; */
/* if (nibble & 1) */
/* diff += (step >> 2); */
/* if (nibble & 2) */
/* diff += (step >> 1); */
/* if (nibble & 4) */
/* diff += step; */

	/* diff >>= 3; */
	diff = step;
	if ((nibble & 4) != 0)
		diff += (int) (step << 3);
	if ((nibble & 2) != 0)
		diff += (int) (step << 2);
	if ((nibble & 1) != 0)
		diff += (int) (step << 1);

	diff >>= 3;

	if (nibble & 8) {
		value -= diff;
		if (value < -32768)
			value = -32768;
	} else {
		value += diff;
		if (value > 32767)
			value = 32767;
	}
	dia_snd->pcm_value = value;

	/* copy to stream */
	if (substream != NULL) {
		dia_snd->pcm_buffer[dia_snd->write_index] = value;
		snd_dialog_bump_write_index(substream, 1);
	}
	if (value > dia_snd->peak_level)
		dia_snd->peak_level = value;

	/* update step_index */
	step_index += ima_index_table[nibble];
	/* clamp step_index */
	if (step_index < 0)
		step_index = 0;
	else if (step_index >= ARRAY_SIZE(ima_step_table))
		step_index = ARRAY_SIZE(ima_step_table) - 1;
	dia_snd->step_index = step_index;
}


/* static int snd_dialog_decode_adpcm_packet( */
/* struct snd_pcm_substream *substream, */
/* const uint8_t *adpcm_input, */
/* size_t num_bytes) */
/* { */
/* struct snd_dialog *dia_snd = snd_pcm_substream_chip(substream); */
/* static short last_predicted_sample = 0; */
/* uint i = 0; */
/* int inp_msb = 1; */
/* uint8_t inp_buf = 0; */
/* int step_size; */
/* int index = dia_snd->step_index; */
/* int predicted_sample = (int)last_predicted_sample; */
/* int inp_idx = 0; */
/*  */
/* for (i = 0; i < num_bytes; i++) { */
/* uint8_t inp; */
/* int diff; */
/*  */
/* step_size = ima_step_table[index];  Find new quantizer stepsize  */
/*  read the MSB or LSB nibbel from the input byte  */
/* if (inp_msb == 1) */
/* { */
/* inp_buf = adpcm_input[inp_idx]; */
/* inp = ((int) inp_buf >> 4) & 0x0F; */
/* inp_msb = 0; */
/* inp_idx++; */
/* } else */
/* { */
/* inp = ((int) inp_buf & 0x0F); */
/* inp_msb = 1; */
/* } */
/*  */
/*  Compute predicted sample estimate                 */
/*  This part is different from regular IMA-ADPCM     */
/*  The prediction is calculated with full precision   */
/* diff = step_size; the 4 is for rounding  */
/* if ((inp & 4) != 0) */
/* { */
/* diff += (int) (step_size << 3); */
/* } */
/* if ((inp & 2) != 0) */
/* { */
/* diff += (int) (step_size << 2); */
/* } */
/* if ((inp & 1) != 0) */
/* { */
/* diff += (int) (step_size << 1); */
/* } */
/* diff >>= 3; */
/*  */
/* Adjust predicted_sample based on calculated difference */
/* predicted_sample += diff; */
/* if ((inp & 8) != 0) */
/* predicted_sample -= diff; */
/* else */
/* predicted_sample += diff; */
/*  */
/*  Saturate if there is overflow */
/* if (predicted_sample > 32767) */
/* predicted_sample = 32767; */
/* if (predicted_sample < -32768) */
/* predicted_sample = -32768; */
/*  */
/* 16-bit output sample can be stored at this point  */
/* optr[i] = (short) predicted_sample; */
/* dia_snd->pcm_value = predicted_sample; */
/*  */
/* copy to stream  */
/* dia_snd->pcm_buffer[dia_snd->write_index] = predicted_sample; */
/* snd_dialog_bump_write_index(substream, 1); */
/* if (predicted_sample > dia_snd->peak_level) */
/* dia_snd->peak_level = predicted_sample; */
/*  */
/*  Compute new stepsize  */
/* Adjust index into step_sizeTable using newIMA  */
/* index += ima_index_table[inp]; */
/* if (index < 0) */
/* index = 0; */
/* if (index > 88) */
/* index = 88; */
/* } */
/*  */
/* dia_snd->step_index = index; */
/* last_predicted_sample = (short)predicted_sample; */
/*  */
/* return num_bytes * 2; */
/* } */

#if (DECODER_BITS_PER_SAMPLE == 4)
static int snd_dialog_decode_adpcm_packet(
			struct snd_pcm_substream *substream,
			const uint8_t *adpcm_input,
			size_t num_bytes
			)
{
	uint i;
	struct snd_dialog *dia_snd = substream != NULL ?
		snd_pcm_substream_chip(substream) : s_dia_snd;

/*Decode IMA ADPCM data to PCM. */
/* if (dia_snd->first_packet) { */
/*  the first two bytes of the first packet */
/* is the unencoded first 16-bit sample, high */
/* byte first. */
/* */
/* int value = ((int)adpcm_input[0] << 8) | adpcm_input[1]; */
/* pr_err("%s: first packet, initial value is %d (0x%x, 0x%x)\n", */
/* __func__, value, adpcm_input[0], adpcm_input[1]); */
/* dia_snd->pcm_value = value; */
/* dia_snd->pcm_buffer[dia_snd->write_index] = value; */
/* #if (DEBUG_WITH_MISC_DEVICE == 1) */
/* if (raw_adpcm_index < ARRAY_SIZE(raw_adpcm_buffer)) */
/* raw_adpcm_buffer[raw_adpcm_index++] = adpcm_input[0]; */
/* if (raw_adpcm_index < ARRAY_SIZE(raw_adpcm_buffer)) */
/* raw_adpcm_buffer[raw_adpcm_index++] = adpcm_input[1]; */
/* if (large_pcm_index < ARRAY_SIZE(large_pcm_buffer)) */
/* large_pcm_buffer[large_pcm_index++] = value; */
/* #endif */
/* snd_dialog_bump_write_index(substream, 1); */
/* dia_snd->peak_level = value; */
/* dia_snd->first_packet = false; */
/* i = 2; */
/* } else { */
/* i = 0; */
/* } */

	for (i = 0; i < num_bytes; i++) {
		uint8_t raw = adpcm_input[i];
		uint8_t nibble;

#if (DEBUG_WITH_MISC_DEVICE == 1)
		if (raw_adpcm_index < ARRAY_SIZE(raw_adpcm_buffer))
			raw_adpcm_buffer[raw_adpcm_index++] = raw;
#endif

		/* process first nibble */
		nibble = (raw >> 4) & 0x0f;
		decode_adpcm_nibble(nibble, dia_snd, substream);

		/* process second nibble */
		nibble = raw & 0x0f;
		decode_adpcm_nibble(nibble, dia_snd, substream);
	}

	return num_bytes * 2;
}
#endif /* (DECODER_BITS_PER_SAMPLE == 4) */

#if (DECODER_BITS_PER_SAMPLE == 3)
static int snd_dialog_decode_adpcm_packet(
			struct snd_pcm_substream *substream,
			const uint8_t *adpcm_input,
			size_t num_bytes
			)
{
	uint i, j, size;
	uint frames = 0;
	uint32_t packed;
	uint8_t nibble;
	struct snd_dialog *dia_snd = substream != NULL ?
		snd_pcm_substream_chip(substream) : s_dia_snd;

	i = 0;
	while (i < num_bytes) {
		/* Pack 8 samples (at most) in 3 bytes (MSB) */
		packed = 0;
		size = 0;
		for (j = 0; j < 3 && i < num_bytes; ++j, ++i) {
			packed |= adpcm_input[i] << (24 - size);
			size += 8;
		}
		size /= 3; /* number of packed samples */
		/* Create and decode 4-bit nibbles */
		for (j = 0; j < size; ++j) {
			nibble = ((packed >> 28) & 0x0E) | 0x01;
			packed <<= 3;
			decode_adpcm_nibble(nibble, dia_snd, substream);
			++frames;
		}
	}
	return frames;
}
#endif /* (DECODER_BITS_PER_SAMPLE == 3) */

/*
  This is called by the event filter when it gets an audio packet
  from the AndroidTV remote.  It writes the packet into a FIFO
  which is then read and decoded by the timer task.
  @param input pointer to data to be decoded
  @param num_bytes how many bytes in raw_input
  @return number of samples decoded or negative error.
 */
static void audio_dec(const uint8_t *raw_input, int type, size_t num_bytes)
{
	bool dropped_packet = false;
	struct snd_pcm_substream *substream;

	spin_lock(&s_substream_lock);
	substream = s_substream_for_btle;
	if (substream != NULL) {
		struct snd_dialog *dia_snd = snd_pcm_substream_chip(substream);
		/* Write data to a FIFO for decoding by the timer task. */
		uint writable = atomic_fifo_available_to_write(
			&dia_snd->fifo_controller);

		/* snd_dialog_log("Writing data into a FIFO..."); */
		if (writable > 0) {
			uint fifo_index = atomic_fifo_get_write_index(
				&dia_snd->fifo_controller);
			struct fifo_packet *packet =
				&dia_snd->fifo_packet_buffer[fifo_index];
			packet->type = type;
			packet->num_bytes = (uint8_t)num_bytes;
			packet->pos = dia_snd->num_bytes;
			packet->start =
				packet->pos == dia_snd->stream_enable_pos;
			packet->reset =
				packet->pos == dia_snd->stream_reset_pos;
			memcpy(packet->raw_data, raw_input, num_bytes);
			dia_snd->num_bytes += num_bytes;

			atomic_fifo_advance_write(
				&dia_snd->fifo_controller, 1);
		} else {
			dropped_packet = true;
			s_substream_for_btle = NULL; /* Stop decoding. */
		}
	} else {
		/* pr_err("Error! Substream is NULL!"); */
		 pr_err_ratelimited("Decoded when null substream!");
		/* Decode received packet to keep
		decoder in sync with the RCU */
		if (s_dia_snd != NULL) {
			/* Reset decoder values */
			if (s_dia_snd->stream_reset_pos ==
			s_dia_snd->num_bytes) {
				s_dia_snd->pcm_value = 0;
				s_dia_snd->step_index = 0;
			}
			/* If we are waiting for the substream to close,
			 and there are data in the FIFO, put */
			/* the packet in the FIFO. It will be
			decoded when snd_dialog_pcm_close is called. */
			if (atomic_fifo_available_to_read
			(&s_dia_snd->fifo_controller) > 0
			&& atomic_fifo_available_to_write
			(&s_dia_snd->fifo_controller) > 0) {
				uint fifo_index =
					atomic_fifo_get_write_index(
					&s_dia_snd->fifo_controller);

				struct fifo_packet *packet =
				&s_dia_snd->fifo_packet_buffer[fifo_index];
				packet->type = type;
				packet->num_bytes = (uint8_t)num_bytes;
				packet->pos = s_dia_snd->num_bytes;
				memcpy(packet->raw_data, raw_input, num_bytes);
				atomic_fifo_advance_write(
				&s_dia_snd->fifo_controller, 1);
			} else {
				snd_dialog_decode_adpcm_packet(
				NULL, raw_input, num_bytes);
			}
			s_dia_snd->num_bytes += num_bytes;
		}
	}
	packet_counter++;
	spin_unlock(&s_substream_lock);

	if (dropped_packet)
		snd_dialog_log(
	  "WARNING, raw audio packet dropped, FIFO full\n");
}

/*
  Note that smp_rmb() is called by snd_dialog_timer_callback()
  before calling this function.

  Reads:
     jiffies
     dia_snd->previous_jiffies
  Writes:
     dia_snd->previous_jiffies
 Returns:
    num_frames needed to catch up to the current time
 */
static uint snd_dialog_calc_frame_advance(struct snd_dialog *dia_snd)
{
	/* Determine how much time passed. */
	uint now_jiffies = jiffies;
	uint elapsed_jiffies = now_jiffies - dia_snd->previous_jiffies;
	/* Convert jiffies to frames. */
	uint frames_by_time = jiffies_to_msecs(elapsed_jiffies)
		* dia_snd->sample_rate / 1000;
	dia_snd->previous_jiffies = now_jiffies;

	/* Don't write more than one buffer full. */
	if (frames_by_time > (dia_snd->frames_per_buffer - 4))
		frames_by_time  = dia_snd->frames_per_buffer - 4;

	return frames_by_time;
}

/* Write zeros into the PCM buffer. */
static uint32_t snd_dialog_write_silence(struct snd_dialog *dia_snd,
			uint32_t pos,
			int frames_to_advance)
{
	/* Does it wrap? */
	if ((pos + frames_to_advance) > dia_snd->frames_per_buffer) {
		/* Write to end of buffer. */
		int16_t *destination = &dia_snd->pcm_buffer[pos];
		size_t num_frames = dia_snd->frames_per_buffer - pos;
		size_t num_bytes = num_frames * sizeof(int16_t);
		memset(destination, 0, num_bytes);
		/* Write from start of buffer to new pos. */
		destination = &dia_snd->pcm_buffer[0];
		num_frames = frames_to_advance - num_frames;
		num_bytes = num_frames * sizeof(int16_t);
		memset(destination, 0, num_bytes);
	} else {
		/* Write within the buffer. */
		int16_t *destination = &dia_snd->pcm_buffer[pos];
		size_t num_bytes = frames_to_advance * sizeof(int16_t);
		memset(destination, 0, num_bytes);
	}
	/* Advance and wrap write_index */
	pos += frames_to_advance;
	pos %= dia_snd->frames_per_buffer;
	return pos;
}

/*
 Called by timer task to decode raw
 audio data from the FIFO into the PCM
 buffer.  Returns the number of packets decoded.
 */
static uint snd_dialog_decode_from_fifo(
struct snd_pcm_substream *substream,
uint *max_frames)
{
	uint i, frames;
	struct snd_dialog *dia_snd = snd_pcm_substream_chip(substream);
	uint readable = atomic_fifo_available_to_read(
		&dia_snd->fifo_controller);
	frames = 0;
	for (i = 0; i < readable; i++) {
		uint fifo_index = atomic_fifo_get_read_index(
			&dia_snd->fifo_controller);
		struct fifo_packet *packet =
			&dia_snd->fifo_packet_buffer[fifo_index];

		if (packet->start)
			break;

		/* Check buffer size */
		if (frames + packet->num_bytes * 8 /
		DECODER_BITS_PER_SAMPLE >
		dia_snd->frames_per_buffer - 4) {

			*max_frames = frames;
			break;
		}

		frames += snd_dialog_decode_adpcm_packet(substream,
						     packet->raw_data,
						     packet->num_bytes);

		dia_snd->decoder_pos = packet->pos + packet->num_bytes;
		atomic_fifo_advance_read(&dia_snd->fifo_controller, 1);

		if (frames >= *max_frames)
			break;
	}

	return frames;
}

static int snd_dialog_schedule_timer(struct snd_pcm_substream *substream)
{
	int ret;
	struct snd_dialog *dia_snd = snd_pcm_substream_chip(substream);
	uint msec_to_sleep = (substream->runtime->period_size * 1000)
			/ dia_snd->sample_rate;
	uint jiffies_to_sleep = msecs_to_jiffies(msec_to_sleep);
	if (jiffies_to_sleep < 2)
		jiffies_to_sleep = 2;
	ret = mod_timer(&dia_snd->decoding_timer, jiffies + jiffies_to_sleep);
	if (ret < 0)
		pr_err("%s:%d - ERROR in mod_timer, ret = %d\n",
			   __func__, __LINE__, ret);
	return ret;
}

static void snd_dialog_timer_callback(unsigned long data)
{
	uint readable;
	uint frames_decoded;
	uint frames_to_advance;
	uint frames_to_silence;
	uint fifo_index;
	struct fifo_packet *packet;
	bool need_silence = false;
	struct snd_pcm_substream *substream = (struct snd_pcm_substream *)data;
	struct snd_dialog *dia_snd = snd_pcm_substream_chip(substream);

	/* timer_enabled will be false when stopping a stream. */
	smp_rmb(); /*   */
	if (!dia_snd->timer_enabled)
		return;
	dia_snd->timer_callback_count++;

	frames_to_advance = frames_to_silence =
	snd_dialog_calc_frame_advance(dia_snd);

	switch (dia_snd->timer_state) {
	case TIMER_STATE_BEFORE_DECODE:
		readable = atomic_fifo_available_to_read(
				&dia_snd->fifo_controller);
		if (readable > 0) {
			dia_snd->timer_state = TIMER_STATE_DURING_DECODE;
			/* Fall through into next state. */
		} else {
			need_silence = true;
			break;
		}

	case TIMER_STATE_DURING_DECODE:
		/* Check for start packet */
		if (atomic_fifo_available_to_read(
		&dia_snd->fifo_controller) > 0) {

			fifo_index =
			atomic_fifo_get_read_index(&dia_snd->fifo_controller);
			packet = &dia_snd->fifo_packet_buffer[fifo_index];

			if (packet->start) {
				packet->start = 0;
				snd_dialog_log("Start packet: %u\n",
				packet->pos);

				dia_snd->buffer_start_pos = packet->pos;
				dia_snd->decoder_frames_adjust = 0;
				/* Reset decoder values */
				if (packet->reset) {
					dia_snd->pcm_value = 0;
					dia_snd->step_index = 0;
				}
			}
		}
		/* Check buffer */
		if (s_substream_for_btle != NULL
			&& dia_snd->buffer_start_pos ==
			dia_snd->stream_enable_pos
			&& (int) (dia_snd->buffer_start_pos -
			dia_snd->stream_disable_pos) >= 0
			&& dia_snd->num_bytes - dia_snd->buffer_start_pos
			< dia_snd->decoder_buffer_size){
			pr_info("BUFFER: %d of %d",
			dia_snd->num_bytes - dia_snd->buffer_start_pos,
			dia_snd->decoder_buffer_size);

			need_silence = true;
			break;
		} else {
			pr_info("TIMER: %d %d", dia_snd->num_bytes,
			atomic_fifo_available_to_read(
			&dia_snd->fifo_controller));
		}
		/* Check if we have already decoded the needed packets */
		if (dia_snd->decoder_frames_adjust < 0
		&& -dia_snd->decoder_frames_adjust >= frames_to_advance) {
			dia_snd->decoder_frames_adjust += frames_to_advance;
			break;
		}
		/* Adjust frames and check buffer size */
		frames_to_advance += dia_snd->decoder_frames_adjust;
		if (frames_to_advance > dia_snd->frames_per_buffer - 4) {

			dia_snd->decoder_frames_adjust =
			frames_to_advance - (dia_snd->frames_per_buffer - 4);

			frames_to_advance = dia_snd->frames_per_buffer - 4;
		} else {
			dia_snd->decoder_frames_adjust = 0;
		}
		/* Decode packets and update adjust frames */
		frames_decoded =
		snd_dialog_decode_from_fifo(substream, &frames_to_advance);

		dia_snd->decoder_frames_adjust +=
		frames_to_advance - frames_decoded;

		if (dia_snd->decoder_frames_adjust > DECODER_MAX_ADJUST_FRAMES)
			dia_snd->decoder_frames_adjust =
			DECODER_MAX_ADJUST_FRAMES;

		snd_dialog_log("DECODE: %u of %u, %u %d\n",
		frames_decoded, frames_to_advance,
		dia_snd->decoder_pos,
		dia_snd->decoder_frames_adjust);
		if (frames_decoded > 0) {
			if ((int) (dia_snd->decoder_pos -
			dia_snd->stream_disable_pos)
				> 0 && frames_to_advance > frames_decoded)
				pr_err("Missing %u frames, decoded %u\n",
				frames_to_advance - frames_decoded,
				frames_decoded);
			break;
		} else {
			if ((int) (dia_snd->decoder_pos -
			dia_snd->stream_disable_pos) > 0)
				pr_err("Buffer empty, generating silence\n");
			dia_snd->decoder_frames_adjust = 0;
			/* no packets read */
			/* snd_dialog_log("No packets read,
			generating silence"); */
			need_silence = true;
		}

		if (s_substream_for_btle == NULL) {
			dia_snd->timer_state = TIMER_STATE_AFTER_DECODE;
			frames_to_silence =
			frames_decoded > frames_to_silence
			? 0 : frames_to_silence - frames_decoded;
			/* Decoder died. Overflowed?
			 Fall through into next state. */
		} else if ((jiffies - dia_snd->previous_jiffies) >
			   dia_snd->timeout_jiffies) {
			snd_dialog_log("audio UNDERFLOW detected\n");
			/*  Not fatal.  Reset timeout. */
			dia_snd->previous_jiffies = jiffies;
			/* ....and generate silence */
			/* need_silence = true; */
			break;
		} else
			break;

	case TIMER_STATE_AFTER_DECODE:
		need_silence = true;
		break;
	}

	/* Write silence before and after decoding. */
	if (need_silence) {
		dia_snd->write_index = snd_dialog_write_silence(
				dia_snd,
				dia_snd->write_index,
				frames_to_silence);
		/* This can cause snd_dialog_pcm_trigger() to be called, which
		 * may try to stop the timer. */
		snd_dialog_handle_frame_advance(substream, frames_to_silence);
	}

	smp_rmb(); /*   */
	if (dia_snd->timer_enabled)
		snd_dialog_schedule_timer(substream);
}

static void snd_dialog_timer_start(struct snd_pcm_substream *substream)
{
	struct snd_dialog *dia_snd = snd_pcm_substream_chip(substream);
	dia_snd->timer_enabled = true;
	dia_snd->previous_jiffies = jiffies;
	dia_snd->timeout_jiffies =
		msecs_to_jiffies(SND_ATVR_RUNNING_TIMEOUT_MSEC);
	dia_snd->timer_callback_count = 0;
	smp_wmb(); /*   */
	setup_timer(&dia_snd->decoding_timer,
		snd_dialog_timer_callback,
		(unsigned long)substream);

	snd_dialog_schedule_timer(substream);
}

static void snd_dialog_timer_stop(
	struct snd_pcm_substream *substream, bool sync)
{
	int ret;
	struct snd_dialog *dia_snd = snd_pcm_substream_chip(substream);

	/* Tell timer function not to reschedule itself if it runs. */
	dia_snd->timer_enabled = false;
	smp_wmb(); /*   */
	if (!in_interrupt()) {
		/* del_timer_sync will hang if called in the timer callback. */
		if (sync)
			ret = del_timer_sync(&dia_snd->decoding_timer);
		else
			ret = del_timer(&dia_snd->decoding_timer);
		if (ret < 0)
			pr_err("%s:%d - ERROR del_timer_sync failed, %d\n",
				__func__, __LINE__, ret);
	}
	/*
	 Else if we are in an interrupt then we are being called from the
	 middle of the snd_dialog_timer_callback(). The timer will not get
	 rescheduled because dia_snd->timer_enabled will be false
	 at the end of snd_dialog_timer_callback().
	 We do not need to "delete" the timer.
	 The del_timer functions just cancel pending timers.
	 There are no resources that need to be cleaned up.
	 */
}

/* ===================================================================== */
/*
 * PCM interface
 */

static int snd_dialog_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct snd_dialog *dia_snd = snd_pcm_substream_chip(substream);

	snd_dialog_log("%s snd_dialog_pcm_trigger called", __func__);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		pr_err("%s starting audio\n", __func__);
		/* Check if timer is still running */
		if (dia_snd->timer_state !=
		TIMER_STATE_UNINIT &&
		try_to_del_timer_sync(&dia_snd->decoding_timer) < 0)
			return -EBUSY;

#if (DEBUG_WITH_MISC_DEVICE == 1)
		large_pcm_index = 0;
		raw_adpcm_index = 0;
		raw_mSBC_index = 0;
#endif
		packet_counter = 0;
		dia_snd->peak_level = -32768;
		dia_snd->previous_jiffies = jiffies;
		dia_snd->timer_state = TIMER_STATE_BEFORE_DECODE;

		/* ADPCM decoder state */
		/* dia_snd->step_index = 0; */
		/* dia_snd->pcm_value = 0; */
		/* dia_snd->num_bytes = 0; */
		/* dia_snd->buffer_start_pos = 0; */
		dia_snd->stream_enable_pos = dia_snd->num_bytes;
		/* dia_snd->stream_disable_pos = 0; */
		/* dia_snd->decoder_pos = 0; */
		dia_snd->decoder_frames_adjust = 0;

		/* mSBC decoder */
		/* dia_snd->packet_in_frame = 0; */
		/* dia_snd->seq_index = 0; */

		snd_dialog_timer_start(substream);
		 /* Enables callback from BTLE driver. */
		s_substream_for_btle = substream;
		/* so other thread will see s_substream_for_btle */
		smp_wmb();
		return 0;

	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		pr_err("Audio stop or suspend.....");
		snd_dialog_log(
		"%s stopping audio, peak = %d, # packets = %d\n",
			__func__, dia_snd->peak_level, packet_counter);

		s_substream_for_btle = NULL;
		/* so other thread will see s_substream_for_btle */
		smp_wmb();
		/* snd_dialog_timer_stop(substream, false); */
		return 0;
	}
	return -EINVAL;
}

static int snd_dialog_pcm_prepare(struct snd_pcm_substream *substream)
{
	struct snd_dialog *dia_snd = snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;

	snd_dialog_log("%s, rate = %d, period_size = %d, buffer_size = %d\n",
		__func__, (int) runtime->rate,
		(int) runtime->period_size,
		(int) runtime->buffer_size);

	if (runtime->buffer_size > MAX_FRAMES_PER_BUFFER)
		return -EINVAL;

	dia_snd->sample_rate = runtime->rate;
	dia_snd->frames_per_buffer = runtime->buffer_size;
	dia_snd->decoder_buffer_size =
		runtime->rate *
		DECODER_BUFFER_LATENCY_MS *
		DECODER_BITS_PER_SAMPLE / 1000 / 8;

	return 0; /* TODO - review */
}

static struct snd_pcm_hardware dia_pcm_hardware = {
	.info =			(SNDRV_PCM_INFO_MMAP |
				 SNDRV_PCM_INFO_INTERLEAVED |
				 SNDRV_PCM_INFO_RESUME |
				 SNDRV_PCM_INFO_MMAP_VALID),
	.formats =		USE_FORMATS,
	.rates =		USE_RATES_MASK,
	.rate_min =		USE_RATE_MIN,
	.rate_max =		USE_RATE_MAX,
	.channels_min =		USE_CHANNELS_MIN,
	.channels_max =		USE_CHANNELS_MAX,
	.buffer_bytes_max =	MAX_PCM_BUFFER_SIZE,
	.period_bytes_min =	MIN_PERIOD_SIZE,
	.period_bytes_max =	MAX_PERIOD_SIZE,
	.periods_min =		USE_PERIODS_MIN,
	.periods_max =		USE_PERIODS_MAX,
	.fifo_size =		0,
};

static int snd_dialog_pcm_hw_params(struct snd_pcm_substream *substream,
					struct snd_pcm_hw_params *hw_params)
{
	int ret = 0;
	struct snd_dialog *dia_snd = snd_pcm_substream_chip(substream);

	dia_snd->write_index = 0;
	smp_wmb(); /*   */

	return ret;
}

static int snd_dialog_pcm_hw_free(struct snd_pcm_substream *substream)
{
	return 0;
}

static int snd_dialog_pcm_open(struct snd_pcm_substream *substream)
{
	struct snd_dialog *dia_snd = snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;

	int ret = atomic_fifo_init(&dia_snd->fifo_controller,
				   MAX_PACKETS_PER_BUFFER);
	if (ret)
		return ret;

	runtime->hw = dia_snd->pcm_hw;
	if (substream->pcm->device & 1) {
		runtime->hw.info &= ~SNDRV_PCM_INFO_INTERLEAVED;
		runtime->hw.info |= SNDRV_PCM_INFO_NONINTERLEAVED;
	}
	if (substream->pcm->device & 2)
		runtime->hw.info &= ~(SNDRV_PCM_INFO_MMAP
			| SNDRV_PCM_INFO_MMAP_VALID);

	/* snd_dialog_log("%s, built %s %s\n",
		__func__, __DATE__, __TIME__); */

	/*
	 Allocate the maximum buffer now
	 and then just use part of it when
	 the substream starts.
	 We don't need DMA because it will just
	 get written to by the BTLE code.
	 */
	/* We only use this buffer in the kernel and we do not do
	 DMA so vmalloc should be OK. */
	dia_snd->pcm_buffer = vmalloc(MAX_PCM_BUFFER_SIZE);
	if (dia_snd->pcm_buffer == NULL) {
		pr_err("%s:%d - ERROR PCM buffer allocation failed\n",
			__func__, __LINE__);
		return -ENOMEM;
	}

	/* We only use this buffer in the kernel and we do not do
	 DMA so vmalloc should be OK.
	 */
	dia_snd->fifo_packet_buffer = vmalloc(MAX_BUFFER_SIZE);
	if (dia_snd->fifo_packet_buffer == NULL) {
		pr_err("%s:%d - ERROR buffer allocation failed\n",
			__func__, __LINE__);
		vfree(dia_snd->pcm_buffer);
		dia_snd->pcm_buffer = NULL;
		return -ENOMEM;
	}

	/* Set minimum buffer size, to avoid overflow of the pcm buffer. */
	/* This affects the period_size, and, as a result, the timer period. */
	/* snd_pcm_hw_constraint_minmax(runtime,
	SNDRV_PCM_HW_PARAM_BUFFER_SIZE,
	USE_RATE_MAX * 200 / 1000,
	MAX_FRAMES_PER_BUFFER); */

	return 0;
}

static int snd_dialog_pcm_close(struct snd_pcm_substream *substream)
{
	struct snd_dialog *dia_snd =
		snd_pcm_substream_chip(substream);

	/* Make sure the timer is not running */
	if (dia_snd->timer_state != TIMER_STATE_UNINIT)
		snd_dialog_timer_stop(substream, true);

	if (dia_snd->timer_callback_count > 0)
		snd_dialog_log("processed %d packets in %d timer callbacks\n",
			packet_counter, dia_snd->timer_callback_count);

	if (dia_snd->pcm_buffer) {
		vfree(dia_snd->pcm_buffer);
		dia_snd->pcm_buffer = NULL;
	}

	/*
	 Use spinlock so we don't free the FIFO when the
	 driver is writing to it.
	 The s_substream_for_btle should already be NULL by now.
	 */
	spin_lock(&s_substream_lock);
	if (dia_snd->fifo_packet_buffer) {
		/* Decode all remaining data */
		uint i, readable =
			atomic_fifo_available_to_read(
				&dia_snd->fifo_controller);
		if (readable)
			snd_dialog_log(
			"%s: Decoding %d remaining FIFO packets\n",
			__func__, readable);
		for (i = 0; i < readable; i++) {
			uint fifo_index =
				atomic_fifo_get_read_index(
					&dia_snd->fifo_controller);
			struct fifo_packet *packet =
				&dia_snd->fifo_packet_buffer[fifo_index];
			snd_dialog_decode_adpcm_packet
				(NULL, packet->raw_data, packet->num_bytes);
			dia_snd->decoder_pos = packet->pos + packet->num_bytes;
			atomic_fifo_advance_read(&dia_snd->fifo_controller, 1);
		}
		vfree(dia_snd->fifo_packet_buffer);
		dia_snd->fifo_packet_buffer = NULL;
	}
	spin_unlock(&s_substream_lock);
	return 0;
}

static snd_pcm_uframes_t snd_dialog_pcm_pointer(
		struct snd_pcm_substream *substream)
{
	struct snd_dialog *dia_snd = snd_pcm_substream_chip(substream);
	/* write_index is written by another driver thread */
	smp_rmb(); /*   */
	return dia_snd->write_index;
}

static int snd_dialog_pcm_copy(struct snd_pcm_substream *substream,
			  int channel, snd_pcm_uframes_t pos,
			  void __user *dst, snd_pcm_uframes_t count)
{
	struct snd_dialog *dia_snd = snd_pcm_substream_chip(substream);
	short *output = (short *)dst;

	/* TODO Needs to be modified if we support more than 1 channel. */
	/*
	 Copy from PCM buffer to user memory.
	 Are we reading past the end of the buffer?
	 */
	if ((pos + count) > dia_snd->frames_per_buffer) {
		const int16_t *source = &dia_snd->pcm_buffer[pos];
		int16_t *destination = output;
		size_t num_frames = dia_snd->frames_per_buffer - pos;
		size_t num_bytes = num_frames * sizeof(int16_t);
		memcpy(destination, source, num_bytes);

		source = &dia_snd->pcm_buffer[0];
		destination += num_frames;
		num_frames = count - num_frames;
		num_bytes = num_frames * sizeof(int16_t);
		memcpy(destination, source, num_bytes);
	} else {
		const int16_t *source = &dia_snd->pcm_buffer[pos];
		int16_t *destination = output;
		size_t num_bytes = count * sizeof(int16_t);
		memcpy(destination, source, num_bytes);
	}

	return 0;
}

static int snd_dialog_pcm_silence(struct snd_pcm_substream *substream,
				int channel, snd_pcm_uframes_t pos,
				snd_pcm_uframes_t count)
{
	return 0; /* Do nothing. Only used by output? */
}

static struct snd_pcm_ops snd_dialog_pcm_ops_no_buf = {
	.open =		snd_dialog_pcm_open,
	.close =	snd_dialog_pcm_close,
	.ioctl =	snd_pcm_lib_ioctl,
	.hw_params =	snd_dialog_pcm_hw_params,
	.hw_free =	snd_dialog_pcm_hw_free,
	.prepare =	snd_dialog_pcm_prepare,
	.trigger =	snd_dialog_pcm_trigger,
	.pointer =	snd_dialog_pcm_pointer,
	.copy =		snd_dialog_pcm_copy,
	.silence =	snd_dialog_pcm_silence,
};

static int snd_card_dia_pcm(struct snd_dialog *dia_snd,
			     int device,
			     int substreams)
{
	struct snd_pcm *pcm;
	struct snd_pcm_ops *ops;
	int err;

	err = snd_pcm_new(dia_snd->card, DIA_REMOTE_PCM_DEVICE_ID, device,
			  0, /* no playback substreams */
			  1, /* 1 capture substream */
			  &pcm);
	if (err < 0)
		return err;
	dia_snd->pcm = pcm;
	ops = &snd_dialog_pcm_ops_no_buf;
	snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_CAPTURE, ops);
	pcm->private_data = dia_snd;
	pcm->info_flags = 0;
	strncpy(pcm->name, DIA_REMOTE_PCM_DEVICE_NAME, sizeof(pcm->name));

	return 0;
}

static int dia_snd_initialize(struct hid_device *hdev)
{
	struct snd_card *card;
	struct snd_dialog *dia_snd;
	int err;
	int i;

	pr_err("Initializing DIA_NEX HID driver");
	if (dev >= SNDRV_CARDS)
		return -ENODEV;
	if (!enable[dev]) {
		dev++;
		return -ENOENT;
	}
	err = snd_card_create(index[dev],
		DIA_REMOTE_AUDIO_DEVICE_ID, THIS_MODULE,
		sizeof(struct snd_dialog), &card);
	if (err < 0) {
		pr_err("%s: snd_card_create() returned err %d\n",
		       __func__, err);
		return err;
	}
	hid_set_drvdata(hdev, card);
	dia_snd = card->private_data;
	dia_snd->card = card;
	dia_snd->timer_state = TIMER_STATE_UNINIT;
	dia_snd->num_bytes = 0;
	dia_snd->buffer_start_pos = 0;
	dia_snd->stream_enable_pos = 0;
	dia_snd->stream_disable_pos = 0;
	dia_snd->stream_reset_pos = 0;
	dia_snd->decoder_pos = 0;
	for (i = 0; i < MAX_PCM_DEVICES && i < pcm_devs[dev]; i++) {
		if (pcm_substreams[dev] < 1)
			pcm_substreams[dev] = 1;
		if (pcm_substreams[dev] > MAX_PCM_SUBSTREAMS)
			pcm_substreams[dev] = MAX_PCM_SUBSTREAMS;
		err = snd_card_dia_pcm(dia_snd, i, pcm_substreams[dev]);
		if (err < 0) {
			pr_err("%s: snd_card_dia_pcm() returned err %d\n",
			       __func__, err);
			goto __nodev;
		}
	}


	dia_snd->pcm_hw = dia_pcm_hardware;

	strncpy(card->driver,
		DIA_REMOTE_AUDIO_DRIVER_NAME, sizeof(card->driver));
	strncpy(card->shortname,
		DIA_REMOTE_AUDIO_DEVICE_NAME_SHORT, sizeof(card->shortname));
	snprintf(card->longname, sizeof(card->longname),
		DIA_REMOTE_AUDIO_DEVICE_NAME_LONG, dev + 1);

	snd_card_set_dev(card, &hdev->dev);

	err = snd_card_register(card);
	if (!err) {
		/* Store snd_dialog struct */
		s_dia_snd = dia_snd;
		return 0;
	}

__nodev:
	snd_card_free(card);
	return err;
}

static void input_dev_report(struct hid_report *report)
{
	struct input_dev *idev = report->field[0]->hidinput->input;
	int key = KEY_SEARCH;
	if (idev == NULL)
		return;
	input_event(idev, EV_KEY, key, 1);
	input_sync(idev);


	msleep(20);
	input_event(idev, EV_KEY, key, 0);
	input_sync(idev);

	pr_info("%s key=%d reported\n", __func__,  key);
}


static int dia_raw_event(struct hid_device *hdev, struct hid_report *report,
	u8 *data, int size)
{
#if (DEBUG_HID_RAW_INPUT == 1)
	pr_info("%s: report->id = 0x%x, size = %d\n",
		__func__, report->id, size);
	if (size < 20) {
		int i;
		for (i = 1; i < size; i++)
			pr_info("data[%d] = 0x%02x\n", i, data[i]);
	}
#endif

if (report->id == ADPCM_AUDIO_REPORT_ID_0 && data[2] == 0) {
	struct snd_pcm_substream *substream = s_substream_for_btle;
	if (substream != NULL || s_dia_snd != NULL) {
		struct snd_dialog *dia_snd = substream != NULL ?
		snd_pcm_substream_chip(substream) : s_dia_snd;
		snd_dialog_log("Stream Enable %d\n", data[1]);
		if (data[1] == 1)			/*voice function begin*/
			input_dev_report(report);
		/* Mark stream enable/disable position */
		if (data[1]) {
			dia_snd->stream_enable_pos = dia_snd->num_bytes;
			dia_snd->stream_reset_pos = dia_snd->num_bytes;
			if (substream == NULL) {
				snd_dialog_log
		    ("Stream enabled with no substream. Reset decoder.\n");
	while (dia_snd->timer_state != TIMER_STATE_UNINIT
	&& try_to_del_timer_sync(&dia_snd->decoding_timer) < 0)
						;
					/* Flush the FIFO */
					spin_lock(&s_substream_lock);
	  atomic_fifo_init(&dia_snd->fifo_controller,
						MAX_PACKETS_PER_BUFFER);
					spin_unlock(&s_substream_lock);
				}
			} else {
				dia_snd->stream_disable_pos =
					dia_snd->num_bytes;
			}
		}
	}
	if (report->id == ADPCM_AUDIO_REPORT_ID_1
			|| report->id == ADPCM_AUDIO_REPORT_ID_2
			|| report->id == ADPCM_AUDIO_REPORT_ID_3){
		/* send the data, minus the report-id in data[0], to the
		 alsa audio decoder driver for ADPCM
		 */
		/* pr_err("Report size %d", size); */
		audio_dec(&data[1], PACKET_TYPE_ADPCM, size - 1);
		/* we've handled the event */
		return -1;
	}


	if (report->id == ADPCM_AUDIO_REPORT_ID_0)
		return -1;
	else
		return 0;
}

static int dia_probe(struct hid_device *hdev, const struct hid_device_id *id)
{
	int ret;
	/*int i; */
	bool supported = false;
	pr_info("@@@@@@@@@@___________in %s()\n", __func__);
	/* since vendor/product id filter doesn't work yet, because
	 Bluedroid is unable to get the vendor/product id, we
	 have to filter on name
	 */
	pr_info("%s: hdev->name = %s, vendor_id = %d, product_id = %d, num %d\n",
		__func__, hdev->name, hdev->vendor, hdev->product, num_remotes);
	/*for (i = 0; i < ARRAY_SIZE(supported_devices); i++) {
		if (strcmp(hdev->name, supported_devices[i]) == 0) {
			supported = true;
			break;
		}
	}*/
	if ((hdev->vendor == USB_VENDOR_ID_DIA) &&
	  (hdev->product == USB_DEVICE_ID_DIA_REMOTE))
		supported = true;

	if (!supported) {
		ret = -ENODEV;
		pr_err("Error! Device name not compatible with DIA HID: %s",
				hdev->name);
		goto err_match;
	}
	pr_info("%s: Found target remote %s\n", __func__, hdev->name);

	ret = hid_parse(hdev);
	if (ret) {
		hid_err(hdev, "hid parse failed\n");
		goto err_parse;
	}

	ret = hid_hw_start(hdev, HID_CONNECT_DEFAULT);
	if (ret) {
		hid_err(hdev, "hw start failed\n");
		goto err_start;
	}

	/* Lazy-creation of the soundcard, and enable the
	   wired headset only then */
	/* to avoid race conditions on subsequent connections. */
	/* AudioService.java delays enabling the output */
	if (!card_created) {
		ret = dia_snd_initialize(hdev);
		if (ret)
			goto err_stop;
		card_created = true;
		/* switch_set_state(&h2w_switch, BIT_HEADSET); */
	}
	pr_info("%s: num_remotes %d->%d\n", __func__,
			num_remotes, num_remotes + 1);
	num_remotes++;

	return 0;
err_stop:
	hid_hw_stop(hdev);
err_start:
err_parse:
err_match:
	return ret;
}

static void dia_remove(struct hid_device *hdev)
{
	pr_info("%s: hdev->name = %s removed, num %d->%d\n",
		__func__, hdev->name, num_remotes, num_remotes - 1);
	num_remotes--;
	hid_hw_stop(hdev);
}

static const struct hid_device_id dia_devices[] = {
	{HID_BLUETOOTH_DEVICE(USB_VENDOR_ID_DIA,
			      USB_DEVICE_ID_DIA_REMOTE)},

	{ }
};
MODULE_DEVICE_TABLE(hid, dia_devices);

static struct hid_driver dia_driver = {
	.name = DIA_REMOTE_HID_DRIVER_NAME,
	.id_table = dia_devices,
	.raw_event = dia_raw_event,
	.probe = dia_probe,
	.remove = dia_remove,
};

static int proc_num_remotes_read(struct seq_file *sfp, void *data)
{
	if (sfp)
		/* seq_printf(sfp, "%d\n", num_remotes); */
		seq_write(sfp, &num_remotes, sizeof(num_remotes));
	return 0;
}

static int
proc_num_remotes_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_num_remotes_read, PDE_DATA(inode));
}

static const struct file_operations dia_proc_fops = {
	.owner = THIS_MODULE,
	.open = proc_num_remotes_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int dia_init(void)
{
	int ret;
	struct proc_dir_entry *r;
	pr_info("@@@@@@@@@@_____2______in %s()\n", __func__);
	pr_err("ATVR driver - init called!");
	r = proc_create_data("dia-num-remotes", 0, NULL, &dia_proc_fops,
			     NULL);
	if (r == NULL)
		pr_err("%s: can't register Proc Entry\n", __func__);

	/* ret = switch_dev_register(&h2w_switch); */
	/* if (ret) */
		/* pr_err("%s: failed to create h2w switch\n", __func__); */

	ret = hid_register_driver(&dia_driver);
	if (ret)
		pr_err("%s: can't register Remote driver\n", __func__);

#if (DEBUG_WITH_MISC_DEVICE == 1)
	pcm_dev_node.minor = MISC_DYNAMIC_MINOR;
	pcm_dev_node.name = "snd_dialog_pcm";
	pcm_dev_node.fops = &pcm_fops;
	ret = misc_register(&pcm_dev_node);
	if (ret)
		pr_err("%s: failed to create pcm misc device %d\n",
		       __func__, ret);
	else
		pr_info("%s: succeeded creating misc device %s\n",
			__func__, pcm_dev_node.name);

	adpcm_dev_node.minor = MISC_DYNAMIC_MINOR;
	adpcm_dev_node.name = "snd_dialog_adpcm";
	adpcm_dev_node.fops = &adpcm_fops;
	ret = misc_register(&adpcm_dev_node);
	if (ret)
		pr_err("%s: failed to create adpcm misc device %d\n",
		       __func__, ret);
	else
		pr_info("%s: succeeded creating misc device %s\n",
			__func__, adpcm_dev_node.name);

	mSBC_dev_node.minor = MISC_DYNAMIC_MINOR;
	mSBC_dev_node.name = "snd_dialog_mSBC";
	mSBC_dev_node.fops = &mSBC_fops;
	ret = misc_register(&mSBC_dev_node);
	if (ret)
		pr_err("%s: failed to create mSBC misc device %d\n",
		       __func__, ret);
	else
		pr_info("%s: succeeded creating misc device %s\n",
			__func__, mSBC_dev_node.name);
#endif

	return ret;
}

static void dia_exit(void)
{
#if (DEBUG_WITH_MISC_DEVICE == 1)
	misc_deregister(&mSBC_dev_node);
	misc_deregister(&adpcm_dev_node);
	misc_deregister(&pcm_dev_node);
#endif

	hid_unregister_driver(&dia_driver);

	/* switch_set_state(&h2w_switch, BIT_NO_HEADSET); */
	/* switch_dev_unregister(&h2w_switch); */
	remove_proc_entry("dia-num-remotes", NULL);
}

module_init(dia_init);
module_exit(dia_exit);
MODULE_LICENSE("GPL");
