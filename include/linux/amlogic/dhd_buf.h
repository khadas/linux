#ifndef _BCMDHD_BUF_H
#define _BCMDHD_BUF_H
#ifdef CONFIG_BCMDHD_USE_STATIC_BUF
int bcmdhd_init_wlan_mem(void);
#else
static inline int bcmdhd_init_wlan_mem(void)
{
	return 0;
}
#endif
#endif
