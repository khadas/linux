#define pr_fmt(fmt) "rtc: %s: " fmt, __func__

#include <linux/module.h>
#include <linux/clk-provider.h>
#include <linux/i2c.h>
#include <linux/bcd.h>
#include <linux/rtc.h>
#include <linux/delay.h>
#include <linux/wakelock.h>
#include <linux/slab.h>
#include <linux/of_gpio.h>
#include <linux/irqdomain.h>

#define HYM8563_CTL1		0x00
#define HYM8563_CTL1_TEST	BIT(7)
#define HYM8563_CTL1_STOP	BIT(5)
#define HYM8563_CTL1_TESTC	BIT(3)

#define HYM8563_CTL2		0x01
#define HYM8563_CTL2_TI_TP	BIT(4)
#define HYM8563_CTL2_AF		BIT(3)
#define HYM8563_CTL2_TF		BIT(2)
#define HYM8563_CTL2_AIE	BIT(1)
#define HYM8563_CTL2_TIE	BIT(0)

#define HYM8563_SEC		0x02
#define HYM8563_SEC_VL		BIT(7)
#define HYM8563_SEC_MASK	0x7f

#define HYM8563_MIN		0x03
#define HYM8563_MIN_MASK	0x7f

#define HYM8563_HOUR		0x04
#define HYM8563_HOUR_MASK	0x3f

#define HYM8563_DAY		0x05
#define HYM8563_DAY_MASK	0x3f

#define HYM8563_WEEKDAY		0x06
#define HYM8563_WEEKDAY_MASK	0x07

#define HYM8563_MONTH		0x07
#define HYM8563_MONTH_CENTURY	BIT(7)
#define HYM8563_MONTH_MASK	0x1f

#define HYM8563_YEAR		0x08

#define HYM8563_ALM_MIN		0x09
#define HYM8563_ALM_HOUR	0x0a
#define HYM8563_ALM_DAY		0x0b
#define HYM8563_ALM_WEEK	0x0c

/* Each alarm check can be disabled by setting this bit in the register */
#define HYM8563_ALM_BIT_DISABLE	BIT(7)

#define HYM8563_CLKOUT		0x0d
#define HYM8563_CLKOUT_ENABLE	BIT(7)
#define HYM8563_CLKOUT_32768	0
#define HYM8563_CLKOUT_1024	1
#define HYM8563_CLKOUT_32	2
#define HYM8563_CLKOUT_1	3
#define HYM8563_CLKOUT_MASK	3

#define HYM8563_TMR_CTL		0x0e
#define HYM8563_TMR_CTL_ENABLE	BIT(7)
#define HYM8563_TMR_CTL_4096	0
#define HYM8563_TMR_CTL_64	1
#define HYM8563_TMR_CTL_1	2
#define HYM8563_TMR_CTL_1_60	3
#define HYM8563_TMR_CTL_MASK	3

#define HYM8563_TMR_CNT		0x0f
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

//#define HY8563_DEBUG
#ifdef HY8563_DEBUG
    #define debug_info(msg...) printk(msg);
#else
    #define debug_info(msg...)
#endif

struct hym8563 {
	struct i2c_client	*client;
	struct mutex mutex;
	struct rtc_device	*rtc;
	struct rtc_wkalrm alarm;
	struct wake_lock wake_lock;
#ifdef CONFIG_COMMON_CLK
	struct clk_hw		clkout_hw;
#endif
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

	debug_info("%s\n", __func__);

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


static int hym8563_rtc_alarm_irq_enable(struct device *dev,
					unsigned int enabled)
{
	struct i2c_client *client = to_i2c_client(dev);
	int data;

	data = i2c_smbus_read_byte_data(client, HYM8563_CTL2);
	if (data < 0)
		return data;

	if (enabled)
		data |= HYM8563_CTL2_AIE;
	else
		data &= ~HYM8563_CTL2_AIE;

	return i2c_smbus_write_byte_data(client, HYM8563_CTL2, data);
};

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

static const struct rtc_class_ops hym8563_rtc_ops = {
	.read_time		= hym8563_rtc_read_time,
	.set_time		= hym8563_rtc_set_time,
	.alarm_irq_enable	= hym8563_rtc_alarm_irq_enable,
	.read_alarm		= hym8563_rtc_read_alarm,
	.set_alarm		= hym8563_rtc_set_alarm,
};

/*
 * Handling of the clkout
 */

#ifdef CONFIG_COMMON_CLK
#define clkout_hw_to_hym8563(_hw) container_of(_hw, struct hym8563, clkout_hw)

static int clkout_rates[] = {
	32768,
	1024,
	32,
	1,
};

static unsigned long hym8563_clkout_recalc_rate(struct clk_hw *hw,
						unsigned long parent_rate)
{
	struct hym8563 *hym8563 = clkout_hw_to_hym8563(hw);
	struct i2c_client *client = hym8563->client;
	int ret = i2c_smbus_read_byte_data(client, HYM8563_CLKOUT);

	if (ret < 0)
		return 0;

	ret &= HYM8563_CLKOUT_MASK;
	return clkout_rates[ret];
}

static long hym8563_clkout_round_rate(struct clk_hw *hw, unsigned long rate,
				      unsigned long *prate)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(clkout_rates); i++)
		if (clkout_rates[i] <= rate)
			return clkout_rates[i];

	return 0;
}

static int hym8563_clkout_set_rate(struct clk_hw *hw, unsigned long rate,
				   unsigned long parent_rate)
{
	struct hym8563 *hym8563 = clkout_hw_to_hym8563(hw);
	struct i2c_client *client = hym8563->client;
	int ret = i2c_smbus_read_byte_data(client, HYM8563_CLKOUT);
	int i;

	if (ret < 0)
		return ret;

	for (i = 0; i < ARRAY_SIZE(clkout_rates); i++)
		if (clkout_rates[i] == rate) {
			ret &= ~HYM8563_CLKOUT_MASK;
			ret |= i;
			return i2c_smbus_write_byte_data(client,
							 HYM8563_CLKOUT, ret);
		}

	return -EINVAL;
}

static int hym8563_clkout_control(struct clk_hw *hw, bool enable)
{
	struct hym8563 *hym8563 = clkout_hw_to_hym8563(hw);
	struct i2c_client *client = hym8563->client;
	int ret = i2c_smbus_read_byte_data(client, HYM8563_CLKOUT);

	if (ret < 0)
		return ret;

	if (enable)
		ret |= HYM8563_CLKOUT_ENABLE;
	else
		ret &= ~HYM8563_CLKOUT_ENABLE;

	return i2c_smbus_write_byte_data(client, HYM8563_CLKOUT, ret);
}

static int hym8563_clkout_prepare(struct clk_hw *hw)
{
	return hym8563_clkout_control(hw, 1);
}

static void hym8563_clkout_unprepare(struct clk_hw *hw)
{
	hym8563_clkout_control(hw, 0);
}

static int hym8563_clkout_is_prepared(struct clk_hw *hw)
{
	struct hym8563 *hym8563 = clkout_hw_to_hym8563(hw);
	struct i2c_client *client = hym8563->client;
	int ret = i2c_smbus_read_byte_data(client, HYM8563_CLKOUT);

	if (ret < 0)
		return ret;

	return !!(ret & HYM8563_CLKOUT_ENABLE);
}

static const struct clk_ops hym8563_clkout_ops = {
	.prepare = hym8563_clkout_prepare,
	.unprepare = hym8563_clkout_unprepare,
	.is_prepared = hym8563_clkout_is_prepared,
	.recalc_rate = hym8563_clkout_recalc_rate,
	.round_rate = hym8563_clkout_round_rate,
	.set_rate = hym8563_clkout_set_rate,
};

static struct clk *hym8563_clkout_register_clk(struct hym8563 *hym8563)
{
	struct i2c_client *client = hym8563->client;
	struct device_node *node = client->dev.of_node;
	struct clk *clk;
	struct clk_init_data init;

	init.name = "hym8563-clkout";
	init.ops = &hym8563_clkout_ops;
	init.flags = CLK_IS_CRITICAL;
	init.parent_names = NULL;
	init.num_parents = 0;
	hym8563->clkout_hw.init = &init;

	/* optional override of the clockname */
	of_property_read_string(node, "clock-output-names", &init.name);

	/* register the clock */
	clk = clk_register(&client->dev, &hym8563->clkout_hw);

	if (!IS_ERR(clk))
		of_clk_add_provider(node, of_clk_src_simple_get, clk);

	return clk;
}
#endif

void hym8563_close_alarm(void)
{
	u8 data;
	hym8563_i2c_read_regs(gClient, RTC_CTL2, &data, 1);
	data &= ~AIE;
	hym8563_i2c_set_regs(gClient, RTC_CTL2, &data, 1);

}

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

static DEVICE_ATTR(bootalarm, 0664,
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

/*
 * The alarm interrupt is implemented as a level-low interrupt in the
 * hym8563, while the timer interrupt uses a falling edge.
 * We don't use the timer at all, so the interrupt is requested to
 * use the level-low trigger.
 */
static irqreturn_t hym8563_irq(int irq, void *dev_id)
{
	struct hym8563 *hym8563 = (struct hym8563 *)dev_id;
	struct i2c_client *client = hym8563->client;
	struct mutex *lock = &hym8563->rtc->ops_lock;
	int data, ret;

	mutex_lock(lock);

	/* Clear the alarm flag */

	data = i2c_smbus_read_byte_data(client, HYM8563_CTL2);
	if (data < 0) {
		dev_err(&client->dev, "%s: error reading i2c data %d\n",
			__func__, data);
		goto out;
	}

	data &= ~HYM8563_CTL2_AF;

	ret = i2c_smbus_write_byte_data(client, HYM8563_CTL2, data);
	if (ret < 0) {
		dev_err(&client->dev, "%s: error writing i2c data %d\n",
			__func__, ret);
	}

out:
	mutex_unlock(lock);
	return IRQ_HANDLED;
}

#if 0
#ifdef CONFIG_PM_SLEEP
static int hym8563_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	int ret;

	if (device_may_wakeup(dev)) {
		ret = enable_irq_wake(client->irq);
		if (ret) {
			dev_err(dev, "enable_irq_wake failed, %d\n", ret);
			return ret;
		}
	}

	return 0;
}

static int hym8563_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);

	if (device_may_wakeup(dev))
		disable_irq_wake(client->irq);

	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(hym8563_pm_ops, hym8563_suspend, hym8563_resume);
#endif

static int hym8563_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct hym8563 *hym8563;
	int ret;
	/*
	 * hym8563 initial time(2021_1_1_12:00:00),
	 * avoid hym8563 read time error
	 */
	struct rtc_time tm_read, tm = {
		.tm_wday = 0,
		.tm_year = 121,
		.tm_mon = 0,
		.tm_mday = 1,
		.tm_hour = 12,
		.tm_min = 0,
		.tm_sec = 0,
	};

	hym8563 = devm_kzalloc(&client->dev, sizeof(*hym8563), GFP_KERNEL);
	if (!hym8563)
		return -ENOMEM;

	gClient = client;
	hym8563->client = client;
	mutex_init(&hym8563->mutex);
	wake_lock_init(&hym8563->wake_lock, WAKE_LOCK_SUSPEND, "rtc_hym8563");
	i2c_set_clientdata(client, hym8563);

	hym8563_init_device(client);

	if (client->irq > 0) {
		ret = devm_request_threaded_irq(&client->dev, client->irq,
						NULL, hym8563_irq,
						IRQF_TRIGGER_LOW | IRQF_ONESHOT,
						client->name, hym8563);
		if (ret < 0) {
			dev_err(&client->dev, "irq %d request failed, %d\n",
				client->irq, ret);
			return ret;
		}
	}
	hym8563_enable_count(client, 0);
	if (client->irq > 0 ||
	    device_property_read_bool(&client->dev, "wakeup-source")) {
		device_init_wakeup(&client->dev, true);
	}

	/* check state of calendar information */
	ret = i2c_smbus_read_byte_data(client, HYM8563_SEC);
	if (ret < 0)
		return ret;

	dev_info(&client->dev, "rtc information is %s\n",
		(ret & HYM8563_SEC_VL) ? "invalid" : "valid");

	hym8563_rtc_read_time(&client->dev, &tm_read);
	if ((ret & HYM8563_SEC_VL) || (tm_read.tm_year < 70) || (tm_read.tm_year > 200) ||
	    (tm_read.tm_mon == -1) || (rtc_valid_tm(&tm_read) != 0))
		hym8563_rtc_set_time(&client->dev, &tm);

	hym8563->rtc = devm_rtc_device_register(&client->dev, client->name,
						&hym8563_rtc_ops, THIS_MODULE);
	if (IS_ERR(hym8563->rtc))
		return PTR_ERR(hym8563->rtc);

	/* the hym8563 alarm only supports a minute accuracy */
	hym8563->rtc->uie_unsupported = 1;
	rtc_sysfs_add_bootalarm(hym8563->rtc);
#ifdef CONFIG_COMMON_CLK
	hym8563_clkout_register_clk(hym8563);
#endif

	return 0;
}

static const struct i2c_device_id hym8563_id[] = {
	{ "hym8563", 0 },
	{},
};
MODULE_DEVICE_TABLE(i2c, hym8563_id);

static const struct of_device_id hym8563_dt_idtable[] = {
	{ .compatible = "haoyu,hym8563" },
	{},
};
MODULE_DEVICE_TABLE(of, hym8563_dt_idtable);

static struct i2c_driver hym8563_driver = {
	.driver		= {
		.name	= "rtc-hym8563",
//		.pm	= &hym8563_pm_ops,
		.of_match_table	= hym8563_dt_idtable,
	},
	.probe		= hym8563_probe,
	.id_table	= hym8563_id,
};

module_i2c_driver(hym8563_driver);

MODULE_AUTHOR("Heiko Stuebner <heiko@sntech.de>");
MODULE_DESCRIPTION("HYM8563 RTC driver");
MODULE_LICENSE("GPL");
