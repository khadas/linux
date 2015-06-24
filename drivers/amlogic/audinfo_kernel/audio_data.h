#ifndef _AUDIO_DATA_ANDROID_H_
#define _AUDIO_DATA_ANDROID_H_

#include <linux/cdev.h>
#include <linux/semaphore.h>
#define AUDIO_DATA_DEVICE_NODE_NAME "audio_data_debug"
#define AUDIO_DATA_DEVICE_FILE_NAME "audio_data_debug"
#define AUDIO_DATA_DEVICE_PROC_NAME "audio_data_debug"
#define AUDIO_DATA_DEVICE_CLASS_NAME "audio_data_debug"

#define EFUSE_DOLBY_AUDIO_EFFECT 1

struct audio_data_android_dev {
	unsigned int val;
	struct semaphore sem;
	struct cdev dev;
};
#endif
