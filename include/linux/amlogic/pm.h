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
struct late_suspend {
	struct list_head link;
	int level;
	void (*suspend)(struct late_suspend *h);
	void (*resume)(struct late_suspend *h);
	void *param;
};
void register_late_suspend(struct late_suspend *handler);


#endif
