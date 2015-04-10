#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/uaccess.h>

#define AMAUDIO_MODULE_NAME "amaudio"
#define AMAUDIO_DRIVER_NAME "amaudio"
#define AMAUDIO_DEVICE_NAME "amaudio"
#define AMAUDIO_CLASS_NAME  "amaudio"

extern void audio_in_i2s_enable(int flag);
extern int if_audio_in_i2s_enable(void);
extern int if_audio_out_enable(void);
extern int if_958_audio_out_enable(void);
extern unsigned int read_i2s_rd_ptr(void);
extern unsigned int audio_in_i2s_wr_ptr(void);
extern unsigned int read_i2s_mute_swap_reg(void);
extern void audio_i2s_swap_left_right(unsigned int flag);
extern void audio_in_i2s_set_buf(u32 addr, u32 size);
extern void audio_mute_left_right(unsigned flag);
extern void audio_i2s_unmute(void);
extern void audio_i2s_mute(void);
extern unsigned audioin_mode;
extern int audio_out_buf_ready;
extern int audio_in_buf_ready;

extern unsigned int aml_i2s_playback_start_addr;
extern unsigned int aml_i2s_capture_start_addr;
extern unsigned int aml_i2s_capture_buf_size;

extern int aml_i2s_playback_enable;
extern unsigned int dac_mute_const;
extern unsigned int timestamp_enable_resample_flag;
extern unsigned int timestamp_resample_type_flag;

