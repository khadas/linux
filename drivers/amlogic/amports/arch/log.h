#ifndef CODEC_LOG_HEADER
#define CODEC_LOG_HEADER

#include<linux/printk.h>

#ifndef INFO_PREFIX
#define INFO_PREFIX "codec"
#endif

#define codec_print(KERN_LEVEL, args...) \
		printk(KERN_LEVEL INFO_PREFIX ":" args)


#define codec_info(args...) codec_print(KERN_INFO, args)
#define codec_err(args...)  codec_print(KERN_WARNING, args)
#define codec_waring(args...)  codec_print(KERN_ERR, args)


#ifdef pr_info
#undef pr_info
#undef pr_err
#undef pr_warn
#undef pr_warning

#define pr_info(args...) codec_info(args)
#define pr_err(args...) codec_err(args)
#define pr_warn(args...) codec_waring(args)
#define pr_warning pr_warn

#endif


#endif

