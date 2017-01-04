
#ifndef _DT_BINDINGS_INPUT_MESON_RC_H
#define _DT_BINDINGS_INPUT_MESON_RC_H

#define REMOTE_KEY(scancode, keycode)\
		((((scancode) & 0xFFFF)<<16) | ((keycode) & 0xFFFF))


#define     REMOTE_TYPE_UNKNOW      0x00
#define     REMOTE_TYPE_NEC         0x01
#define     REMOTE_TYPE_DUOKAN      0x02
#define     REMOTE_TYPE_XMP_1       0x03
#define     REMOTE_TYPE_RC5         0x04
#define     REMOTE_TYPE_RAW_NEC     0x101
#define     REMOTE_TYPE_RAW_XMP_1   0x103

#endif
