#ifndef _AML_SPDIF_DAI_H
#define _AML_SPDIF_DAI_H

/* HDMI audio stream type ID */
#define AOUT_EVENT_IEC_60958_PCM                0x1
#define AOUT_EVENT_RAWDATA_AC_3                 0x2
#define AOUT_EVENT_RAWDATA_MPEG1                0x3
#define AOUT_EVENT_RAWDATA_MP3                  0x4
#define AOUT_EVENT_RAWDATA_MPEG2                0x5
#define AOUT_EVENT_RAWDATA_AAC                  0x6
#define AOUT_EVENT_RAWDATA_DTS                  0x7
#define AOUT_EVENT_RAWDATA_ATRAC                0x8
#define AOUT_EVENT_RAWDATA_ONE_BIT_AUDIO        0x9
#define AOUT_EVENT_RAWDATA_DOBLY_DIGITAL_PLUS   0xA
#define AOUT_EVENT_RAWDATA_DTS_HD               0xB
#define AOUT_EVENT_RAWDATA_MAT_MLP              0xC
#define AOUT_EVENT_RAWDATA_DST                  0xD
#define AOUT_EVENT_RAWDATA_WMA_PRO              0xE

extern unsigned int IEC958_mode_codec;

/*
* special call by the audiodsp,add these code,
* as there are three cases for 958 s/pdif output
* 1)NONE-PCM  raw output ,only available when ac3/dts audio,
* when raw output mode is selected by user.
* 2)PCM  output for  all audio, when pcm mode is selected by user .
* 3)PCM  output for audios except ac3/dts,
* when raw output mode is selected by user
*/
void aml_hw_iec958_init(struct snd_pcm_substream *substream);

#endif  /* _AML_SPDIF_DAI_H */
