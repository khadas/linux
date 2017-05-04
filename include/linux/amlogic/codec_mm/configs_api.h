#ifndef AMLOGIC_MEDIA_CONFIG_API__
#define AMLOGIC_MEDIA_CONFIG_API__
#include <linux/ioctl.h>
#define MAX_ITEM_NAME 128
#define MAX_PREFIX_NAME 128
#define MAX_VALUE_NAME  256
struct media_config_io_str {
	union{
		int subcmd;
		int ret;
	};
	union {
		int para[10];
		char cmd_path[MAX_PREFIX_NAME + MAX_ITEM_NAME + 4];
	};
	union {
		char val[MAX_VALUE_NAME];
		char *ptr;
	};
};


#define AML_CONFIG  'C'
#define MEDIA_CONFIG_SET_CMD_STR _IOW((AML_CONFIG), 0x1,\
				struct media_config_io_str)
#define MEDIA_CONFIG_GET_CMD_STR _IOWR((AML_CONFIG), 0x2,\
				struct media_config_io_str)

#endif


