#ifndef DSP_CONTROL_HEADER
#define DSP_CONTROL_HEADER

#include <asm/cacheflush.h>
#include "audiodsp_module.h"
#include "dsp_microcode.h"
#include <asm/system.h>
#include <linux/dma-mapping.h>

void halt_dsp(struct audiodsp_priv *priv);
void reset_dsp(struct audiodsp_priv *priv);
int dsp_start(struct audiodsp_priv *priv, struct audiodsp_microcode *mcode);
int dsp_stop(struct audiodsp_priv *priv);
int dsp_check_status(struct audiodsp_priv *priv);

extern unsigned IEC958_mode_raw;
extern unsigned IEC958_mode_codec;
extern unsigned audioin_mode;

#define AUDIODSP_RESET
#endif				/*  */
