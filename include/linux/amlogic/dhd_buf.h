#ifndef _DHD_BUF_H
#define _DHD_BUF_H
#ifdef CONFIG_DHD_USE_STATIC_BUF
int bcmdhd_init_wlan_mem(void);
#else
static inline int bcmdhd_init_wlan_mem(void)
{
	return 0;
}
#endif
#endif
