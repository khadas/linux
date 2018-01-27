#define pr_fmt(fmt) "rtc: %s: " fmt, __func__

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/bcd.h>
#include <linux/rtc.h>
#include <linux/delay.h>
#include <linux/wakelock.h>
#include <linux/slab.h>
#include <linux/of_gpio.h>
#include <linux/irqdomain.h>

#define   RTC_CTL1      0x00
#define   RTC_CTL2      0x01
#define   RTC_SEC       0x02
#define   RTC_MIN       0x03
#define   RTC_HOUR      0x04
#define   RTC_DAY       0x05
#define   RTC_WEEK      0x06
#define   RTC_MON       0x07
#define   RTC_YEAR      0x08
#define   RTC_A_MIN     0x09
#define   RTC_A_HOUR    0x0A
#define   RTC_A_DAY     0x0B
#define   RTC_A_WEEK    0x0C
#define   RTC_CLKOUT    0x0D
#define   RTC_T_CTL     0x0E
#define   RTC_T_COUNT   0x0F
#define   CENTURY   0x80
#define   TI        0x10
#define   AF        0x08
#define   TF        0x04
#define   AIE       0x02
#define   TIE       0x01
#define   FE        0x80
#define   TE        0x80
#define   FD1       0x02
#define   FD0       0x01
#define   TD1       0x02
#define   TD0       0x01
#define   VL        0x80

#define RTC_SECTION_LEN 0x07
#define ALM_BIT_DISABLE BIT(7)
#define MIN_MASK    0x7f
#define HOUR_MASK   0x3f
#define DAY_MASK    0x3f
#define MONTH_MASK  0x1f
#define WEEKDAY_MASK    0x07
#define RTC_CTL2_AIE    BIT(1)

/* #define HY8563_DEBUG */
#ifdef HY8563_DEBUG
    #define debug_info(msg...) printk(msg);
#else
    #define debug_info(msg...)
#endif

struct hym8563 {
	int irq;
	struct i2c_client *client;
	struct mutex mutex;
	struct rtc_device *rtc;
	struct rtc_wkalrm alarm;
	struct wake_lock wake_lock;
};
static struct i2c_client *gClient;
static int i2c_master_reg8_send(const struct i2c_client *client,
		const char reg, const char *buf, int count)
{
	struct i2c_adapter *adap = client->adapter;
	struct i2c_msg msg;
	int ret;
	char *tx_buf = kzalloc(count + 1, GFP_KERNEL);
	if (!tx_buf)
		return -ENOMEM;
	tx_buf[0] = reg;
	memcpy(tx_buf+1, buf, count);

	msg.addr = client->addr;
	msg.flags = client->flags;
	msg.len = count + 1;
	msg.buf = (char *)tx_buf;

	ret = i2c_transfer(adap, &msg, 1);
	kfree(tx_buf);
	return (ret == 1) ? count : ret;
}

static int i2c_master_reg8_recv(const struct i2c_client *client,
		const char reg, char *buf, int count)
{
	struct i2c_adapter *adap = client->adapter;
	struct i2c_msg msgs[2];
	int ret;
	char reg_buf = reg;

	msgs[0].addr = client->addr;
	msgs[0].flags = client->flags;
	msgs[0].len = 1;
	msgs[0].buf = &reg_buf;

	msgs[1].addr = client->addr;
	msgs[1].flags = client->flags | I2C_M_RD;
	msgs[1].len = count;
	msgs[1].buf = (char *)buf;

	ret = i2c_transfer(adap, msgs, 2);

	return (ret == 2) ? count : ret;
}

static int hym8563_i2c_read_regs(struct i2c_client *client,
		u8 reg, u8 buf[], unsigned len)
{
	int ret;
	ret = i2c_master_reg8_recv(client, reg, buf, len);
	return ret;
}

static int hym8563_i2c_set_regs(struct i2c_client *client,
		u8 reg, u8 const buf[], __u16 len)
{
	int ret;
	ret = i2c_master_reg8_send(client, reg, buf, (int)len);
	return ret;
}

int hym8563_enable_count(struct i2c_client *client, int en)
{
	struct hym8563 *hym8563 = i2c_get_clientdata(client);
	u8 regs[2];

	if (!hym8563)
		return -1;

	if (en) {
		hym8563_i2c_read_regs(client, RTC_CTL2, regs, 1);
		regs[0] |= TIE;
		hym8563_i2c_set_regs(client, RTC_CTL2, regs, 1);
		regs[0] = 0;
		regs[0] |= (TE | TD1);
		hym8563_i2c_set_regs(client, RTC_T_CTL, regs, 1);
	} else {
		hym8563_i2c_read_regs(client, RTC_CTL2, regs, 1);
		regs[0] &= ~TIE;
		hym8563_i2c_set_regs(client, RTC_CTL2, regs, 1);
		regs[0] = 0;
		regs[0] |= (TD0 | TD1);
		hym8563_i2c_set_regs(client, RTC_T_CTL, regs, 1);
	}
	return 0;

}

int hym8563_set_count(struct i2c_client *client, int sec)
{
	struct hym8563 *hym8563 = i2c_get_clientdata(client);
	u8 regs[2];

	if (!hym8563)
		return -1;

	if (sec >= 255)
		regs[0] = 255;
	else if (sec <= 1)
		regs[0] = 1;
	else
		regs[0] = sec;

	hym8563_i2c_set_regs(client, RTC_T_COUNT, regs, 1);

	return 0;
}

static int hym8563_init_device(struct i2c_client *client)
{
	struct hym8563 *hym8563 = i2c_get_clientdata(client);
	u8 regs[2];
	int sr;

	mutex_lock(&hym8563->mutex);
	regs[0] = 0;
	sr = hym8563_i2c_set_regs(client, RTC_CTL1, regs, 1);
	if (sr < 0)
		goto exit;

	/*disable clkout*/
	regs[0] = 0x80;
	sr = hym8563_i2c_set_regs(client, RTC_CLKOUT, regs, 1);
	if (sr < 0)
		goto exit;

	/*enable alarm && count interrupt*/
	sr = hym8563_i2c_read_regs(client, RTC_CTL2, regs, 1);
	if (sr < 0)
		goto exit;
	regs[0] = 0x0;
	regs[0] |= (AIE | TIE);
	sr = hym8563_i2c_set_regs(client, RTC_CTL2, regs, 1);
	if (sr < 0)
		goto exit;
	sr = hym8563_i2c_read_regs(client, RTC_CTL2, regs, 1);
	if (sr < 0)
		goto exit;

	sr = hym8563_i2c_read_regs(client, RTC_CTL2, regs, 1);
	if (sr < 0) {
		pr_err("read CTL2 err\n");
		goto exit;
	}

	if (regs[0] & (AF|TF)) {
		regs[0] &= ~(AF|TF);
		sr = hym8563_i2c_set_regs(client, RTC_CTL2, regs, 1);
	}
exit:
	mutex_unlock(&hym8563->mutex);
	return sr;
}

static int hym8563_read_datetime(struct i2c_client *client, struct rtc_time *tm)
{
	struct hym8563 *hym8563 = i2c_get_clientdata(client);
	u8 regs[RTC_SECTION_LEN] = { 0, };
	mutex_lock(&hym8563->mutex);
	hym8563_i2c_read_regs(client, RTC_SEC, regs, RTC_SECTION_LEN);
	mutex_unlock(&hym8563->mutex);

	tm->tm_sec = bcd2bin(regs[0x00] & 0x7F);
	tm->tm_min = bcd2bin(regs[0x01] & 0x7F);
	tm->tm_hour = bcd2bin(regs[0x02] & 0x3F);
	tm->tm_mday = bcd2bin(regs[0x03] & 0x3F);
	tm->tm_wday = bcd2bin(regs[0x04] & 0x07);
	tm->tm_mon = bcd2bin(regs[0x05] & 0x1F);
	tm->tm_mon -= 1;
	tm->tm_year = bcd2bin(regs[0x06] & 0xFF);

	if (regs[5] & 0x80)
		tm->tm_year += 1900;
	else
		tm->tm_year += 2000;

	tm->tm_year -= 1900;
	if (tm->tm_year < 0)
		tm->tm_year = 0;
	tm->tm_isdst = 0;

	debug_info("%4d-%02d-%02d(%d) %02d:%02d:%02d\n",
			1900 + tm->tm_year, tm->tm_mon + 1, tm->tm_mday,
			tm->tm_wday, tm->tm_hour, tm->tm_min, tm->tm_sec);

	return 0;

}

static int hym8563_rtc_read_time(struct device *dev, struct rtc_time *tm)
{
	return hym8563_read_datetime(to_i2c_client(dev), tm);
}

static int hym8563_set_time(struct i2c_client *client, struct rtc_time *tm)
{
	struct hym8563 *hym8563 = i2c_get_clientdata(client);
	u8 regs[RTC_SECTION_LEN] = { 0, };
	u8 mon_day;

	debug_info("%4d-%02d-%02d(%d) %02d:%02d:%02d\n",
			1900 + tm->tm_year, tm->tm_mon + 1, tm->tm_mday,
			tm->tm_wday, tm->tm_hour, tm->tm_min, tm->tm_sec);

	mon_day = rtc_month_days((tm->tm_mon), tm->tm_year + 1900);

	if (tm->tm_sec >= 60 || tm->tm_sec < 0)
		regs[0x00] = bin2bcd(0x00);
	else
		regs[0x00] = bin2bcd(tm->tm_sec);

	if (tm->tm_min >= 60 || tm->tm_min < 0)
		regs[0x01] = bin2bcd(0x00);
	else
		regs[0x01] = bin2bcd(tm->tm_min);

	if (tm->tm_hour >= 24 || tm->tm_hour < 0)
		regs[0x02] = bin2bcd(0x00);
	else
		regs[0x02] = bin2bcd(tm->tm_hour);

	if ((tm->tm_mday) > mon_day)
		regs[0x03] = bin2bcd(mon_day);
	else if ((tm->tm_mday) > 0)
		regs[0x03] = bin2bcd(tm->tm_mday);
	else if ((tm->tm_mday) <= 0)
		regs[0x03] = bin2bcd(0x01);

	if (tm->tm_year >= 200)
		regs[0x06] = bin2bcd(99);
	else if (tm->tm_year >= 100)
		regs[0x06] = bin2bcd(tm->tm_year - 100);
	else if (tm->tm_year >= 0) {
		regs[0x06] = bin2bcd(tm->tm_year);
		regs[0x05] |= 0x80;
	} else {
		regs[0x06] = bin2bcd(0);
		regs[0x05] |= 0x80;
	}
	regs[0x04] = bin2bcd(tm->tm_wday);
	regs[0x05] = (regs[0x05] & 0x80) | (bin2bcd(tm->tm_mon + 1) & 0x7F);

	mutex_lock(&hym8563->mutex);
	hym8563_i2c_set_regs(client, RTC_SEC, regs, RTC_SECTION_LEN);
	mutex_unlock(&hym8563->mutex);

	return 0;
}

static int hym8563_rtc_set_time(struct device *dev, struct rtc_time *tm)
{
	return hym8563_set_time(to_i2c_client(dev), tm);
}

static int hym8563_rtc_read_alarm(struct device *dev, struct rtc_wkalrm *alm)
{
	u8 buf[4];
	u8 regs[2];
	int ret;

	ret = hym8563_i2c_read_regs(gClient, RTC_A_MIN , buf, 4);
	if (ret < 0)
		return ret;
	alm->time.tm_sec = -1;
	alm->time.tm_min = (buf[0] & ALM_BIT_DISABLE) ?
						-1 :
						bcd2bin(buf[0] & MIN_MASK);
	alm->time.tm_hour = (buf[1] & ALM_BIT_DISABLE) ?
						-1 :
						bcd2bin(buf[1] & HOUR_MASK);
	alm->time.tm_mday = (buf[2] & ALM_BIT_DISABLE) ?
						-1 :
						bcd2bin(buf[2] & DAY_MASK);
	alm->time.tm_wday = (buf[3] & ALM_BIT_DISABLE) ?
						-1 :
						bcd2bin(buf[3] & WEEKDAY_MASK);

	alm->time.tm_mon = -1;
	alm->time.tm_year = -1;
	ret = hym8563_i2c_read_regs(gClient, RTC_CTL2, regs, 1);
	if (ret < 0)
		return ret;
	if (regs[0] & RTC_CTL2_AIE)
		alm->enabled = 1;

	return 0;
}

static int hym8563_rtc_set_alarm(struct device *dev, struct rtc_wkalrm *alarm)
{
	struct hym8563 *hym8563 = i2c_get_clientdata(gClient);
	struct rtc_time now, *tm = &alarm->time;
	u8 regs[4] = { 0, };
	u8 mon_day;
	unsigned long   alarm_sec, now_sec;
	int diff_sec = 0;

	debug_info("%4d-%02d-%02d(%d) %02d:%02d:%02d enabled %d\n",
			1900 + tm->tm_year, tm->tm_mon + 1,
			tm->tm_mday, tm->tm_wday, tm->tm_hour, tm->tm_min,
			tm->tm_sec, alarm->enabled);

	hym8563_read_datetime(gClient, &now);
	mutex_lock(&hym8563->mutex);
	rtc_tm_to_time(tm, &alarm_sec);
	rtc_tm_to_time(&now, &now_sec);

	diff_sec = alarm_sec - now_sec;
	if ((diff_sec > 0) && (diff_sec < 256)) {
		if (alarm->enabled == 1) {
			hym8563_set_count(gClient, diff_sec);
			hym8563_enable_count(gClient, 1);
		} else {
			hym8563_enable_count(gClient, 0);
		}
	} else {
		hym8563_enable_count(gClient, 0);
		if (tm->tm_sec > 0) {
			rtc_tm_to_time(tm, &alarm_sec);
			rtc_time_to_tm(alarm_sec, tm);
		}
		hym8563->alarm = *alarm;
		regs[0] = 0x0;
		hym8563_i2c_set_regs(gClient, RTC_CTL2, regs, 1);
		mon_day = rtc_month_days(tm->tm_mon, tm->tm_year + 1900);
		hym8563_i2c_read_regs(gClient, RTC_A_MIN, regs, 4);

		if (tm->tm_min >= 60 || tm->tm_min < 0)
			regs[0x00] = bin2bcd(0x00) & 0x7f;
		else
			regs[0x00] = bin2bcd(tm->tm_min) & 0x7f;

		if (tm->tm_hour >= 24 || tm->tm_hour < 0)
			regs[0x01] = bin2bcd(0x00) & 0x7f;
		else
			regs[0x01] = bin2bcd(tm->tm_hour) & 0x7f;

		regs[0x03] = bin2bcd(tm->tm_wday) & 0x7f;

		if (tm->tm_mday > mon_day)
			regs[0x02] = bin2bcd(mon_day) & 0x7f;
		else if (tm->tm_mday > 0)
			regs[0x02] = bin2bcd(tm->tm_mday) & 0x7f;
		else if (tm->tm_mday <= 0)
			regs[0x02] = bin2bcd(0x01) & 0x7f;

		hym8563_i2c_set_regs(gClient, RTC_A_MIN, regs, 4);
		hym8563_i2c_read_regs(gClient, RTC_A_MIN, regs, 4);
		hym8563_i2c_read_regs(gClient, RTC_CTL2, regs, 1);

		if (alarm->enabled == 1)
			regs[0] |= AIE;
		else
			regs[0] &= 0x0;

		hym8563_i2c_set_regs(gClient, RTC_CTL2, regs, 1);
		hym8563_i2c_read_regs(gClient, RTC_CTL2, regs, 1);

		if (diff_sec <= 0)
			debug_info("alarm sec  <= now sec\n");

	}

	mutex_unlock(&hym8563->mutex);

	return 0;
}

void hym8563_close_alarm(void)
{
	u8 data;
	hym8563_i2c_read_regs(gClient, RTC_CTL2, &data, 1);
	data &= ~AIE;
	hym8563_i2c_set_regs(gClient, RTC_CTL2, &data, 1);

}

static const struct rtc_class_ops hym8563_rtc_ops = {
	.read_time  = hym8563_rtc_read_time,
	.set_time   = hym8563_rtc_set_time,
	.read_alarm = hym8563_rtc_read_alarm,
	.set_alarm  = hym8563_rtc_set_alarm,
	.ioctl      = NULL,
	.proc       = NULL
};

static ssize_t rtc_sysfs_show_bootalarm(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t retval;
	unsigned long alarm;
	struct rtc_wkalrm alm;

	retval = hym8563_rtc_read_alarm(dev, &alm);
	if (retval == 0 && alm.enabled) {
		rtc_tm_to_time(&alm.time, &alarm);
		retval = sprintf(buf, "%lu\n", alarm);
	}
	return retval;
}

static ssize_t
rtc_sysfs_set_bootalarm(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t n)
{
	ssize_t retval;
	unsigned long now, alarm;
	struct rtc_wkalrm alm;
	struct rtc_device *rtc = to_rtc_device(dev);
	char *buf_ptr;
	retval = rtc_read_time(rtc, &alm.time);
	if (retval < 0)
		return retval;

	rtc_tm_to_time(&alm.time, &now);
	buf_ptr = (char *)buf;

	if (*buf_ptr == '+')
		alm.enabled = 1;
	else if (*buf_ptr == '-')
		alm.enabled = 0;
	else if (*buf_ptr == '=')
		hym8563_close_alarm();

	buf_ptr++;
	if (kstrtoul(buf_ptr, 10, &alarm))
		return -1;

	rtc_time_to_tm(alarm, &alm.time);
	retval = hym8563_rtc_set_alarm(dev, &alm);
	return (retval < 0) ? retval : n;
}

static DEVICE_ATTR(bootalarm, 0666,
		rtc_sysfs_show_bootalarm, rtc_sysfs_set_bootalarm);

void rtc_sysfs_add_bootalarm(struct rtc_device *rtc)
{
	int err;
	err = device_create_file(&rtc->dev, &dev_attr_bootalarm);
	if (err)
		pr_err("failed to create boot alarm attribute");

}

void rtc_sysfs_del_bootalarm(struct rtc_device *rtc)
{
	device_remove_file(&rtc->dev, &dev_attr_bootalarm);
}

static int hym8563_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	int rc = 0;
	u8 reg = 0;
	struct hym8563 *hym8563;
	struct rtc_device *rtc;
	struct rtc_time tm_read, tm = {
		.tm_wday = 6,
		.tm_year = 111,
		.tm_mon = 0,
		.tm_mday = 1,
		.tm_hour = 12,
		.tm_min = 0,
		.tm_sec = 0,
	};

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
		return -ENODEV;

	hym8563 = devm_kzalloc(&client->dev, sizeof(*hym8563), GFP_KERNEL);
	if (!hym8563)
		return -ENOMEM;

	gClient = client;
	hym8563->client = client;
	hym8563->alarm.enabled = 0;
	client->irq = 0;
	mutex_init(&hym8563->mutex);
	wake_lock_init(&hym8563->wake_lock, WAKE_LOCK_SUSPEND, "rtc_hym8563");
	i2c_set_clientdata(client, hym8563);

	hym8563_init_device(client);
	hym8563_enable_count(client, 0);

	hym8563_i2c_read_regs(client, RTC_SEC, &reg, 1);
	if (reg&0x80) {
		debug_info("clock/calendar information is no longer guaranteed\n");
		hym8563_set_time(client, &tm);
	}

	hym8563_read_datetime(client, &tm_read);

	if (((tm_read.tm_year < 70) | (tm_read.tm_year > 137)) |
			(tm_read.tm_mon == -1) | (rtc_valid_tm(&tm_read) != 0))
		hym8563_set_time(client, &tm);

	rtc = rtc_device_register(client->name, &client->dev,
			&hym8563_rtc_ops, THIS_MODULE);

	if (IS_ERR(rtc)) {
		rc = PTR_ERR(rtc);
		rtc = NULL;
		goto exit;
	}
	hym8563->rtc = rtc;
	rtc_sysfs_add_bootalarm(rtc);

	return 0;

exit:
	if (hym8563)
		wake_lock_destroy(&hym8563->wake_lock);

	return rc;
}

static int  hym8563_remove(struct i2c_client *client)
{
	struct hym8563 *hym8563 = i2c_get_clientdata(client);
	wake_lock_destroy(&hym8563->wake_lock);

	return 0;
}

void hym8563_shutdown(struct i2c_client *client)
{
	u8 regs[2];
	int ret;
	regs[0] = 0x00;
	ret = hym8563_i2c_set_regs(client, RTC_CLKOUT, regs, 1);
	if (ret < 0)
		pr_err("rtc shutdown is error\n");
}

static const struct i2c_device_id hym8563_id[] = {
	{ "rtc_hym8563", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, hym8563_id);

static struct of_device_id rtc_dt_ids[] = {
	{ .compatible = "amlogic, rtc_hym8563" },
	{},
};

struct i2c_driver hym8563_driver = {
	.driver     = {
		.name   = "rtc-aml_hym8563",
		.owner  = THIS_MODULE,
		.of_match_table = of_match_ptr(rtc_dt_ids),
	},
	.probe      = hym8563_probe,
	.remove     = hym8563_remove,
	.id_table   = hym8563_id,
};

static int __init hym8563_init(void)
{
	return i2c_add_driver(&hym8563_driver);
}

static void __exit hym8563_exit(void)
{
	i2c_del_driver(&hym8563_driver);
}

MODULE_AUTHOR("terry terry@szwesion.com");
MODULE_DESCRIPTION("HYM8563 RTC driver");
MODULE_LICENSE("GPL");

module_init(hym8563_init);
module_exit(hym8563_exit);
