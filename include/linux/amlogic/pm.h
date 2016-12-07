#ifndef __AML_PM_H__
#define __AML_PM_H__
/* wake up reason*/
#define	UDEFINED_WAKEUP	0
#define	CHARGING_WAKEUP	1
#define	REMOTE_WAKEUP		2
#define	RTC_WAKEUP			3
#define	BT_WAKEUP			4
#define	WIFI_WAKEUP			5
#define	POWER_KEY_WAKEUP	6
#define	AUTO_WAKEUP			7
#define	CEC_WAKEUP			8
#define	REMOTE_CUS_WAKEUP		9
#define ETH_PHY_WAKEUP      10
#ifdef CONFIG_GXBB_SUSPEND
unsigned int get_resume_method(void);
#else
static inline unsigned int get_resume_method(void)
{
	return 0;
}
#endif
#endif
