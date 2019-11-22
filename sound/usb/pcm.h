#ifndef __USBAUDIO_PCM_H
#define __USBAUDIO_PCM_H

snd_pcm_uframes_t snd_usb_pcm_delay(struct snd_usb_substream *subs,
				    unsigned int rate);

void snd_usb_set_pcm_ops(struct snd_pcm *pcm, int stream);

int snd_usb_init_pitch(struct snd_usb_audio *chip, int iface,
		       struct usb_host_interface *alts,
		       struct audioformat *fmt);

#ifdef CONFIG_AMLOGIC_SND_USB_CAPTURE_DATA
int usb_set_capture_status(bool isrunning);
int usb_audio_capture_init(void);
int usb_audio_capture_deinit(void);
int retire_capture_usb(
	struct snd_pcm_runtime *runtime,
	unsigned char *cp, unsigned int bytes,
	unsigned int oldptr, unsigned int stride);
#endif

#endif /* __USBAUDIO_PCM_H */
