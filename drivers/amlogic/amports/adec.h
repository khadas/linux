#ifndef ADEC_H
#define ADEC_H

#include "streambuf.h"

extern s32 adec_init(struct stream_port_s *port);

extern s32 adec_release(enum aformat_e af);

extern s32 astream_dev_register(void);

extern s32 astream_dev_unregister(void);

#endif /* ADEC_H */
