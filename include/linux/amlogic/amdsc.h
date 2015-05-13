#ifndef _AMDSC_H
#define _AMDSC_H

#include <linux/types.h>

enum am_dsc_key_type_t {
	AM_DSC_EVEN_KEY,
	AM_DSC_ODD_KEY
};

struct am_dsc_key {
	enum am_dsc_key_type_t    type;
	__u8                 key[8];
};

#define AMDSC_IOC_MAGIC  'D'

#define AMDSC_IOC_SET_PID      _IO(AMDSC_IOC_MAGIC, 0x00)
#define AMDSC_IOC_SET_KEY      _IOW(AMDSC_IOC_MAGIC, 0x01, struct am_dsc_key)

#endif

