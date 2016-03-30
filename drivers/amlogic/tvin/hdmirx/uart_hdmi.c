#include <dt-bindings/gpio/gxtvbb.h>
#include <linux/timer.h>
#include <linux/device.h>
#include <linux/workqueue.h>
#include <linux/pinctrl/consumer.h>
#include <linux/platform_device.h>
#include <linux/of_gpio.h>
#include <linux/module.h>
#include "hdmirx_drv.h"
#include "uart_hdmi.h"

#define HDMI_STATE_CHECK_FREQ     (20*5)
#define TIMER_STATE_CHECK		(1*HZ/HDMI_STATE_CHECK_FREQ)

/*unsigned char into_hdmirx_flag = 0;*/
struct gpio_desc *g_uart_pin[3];
struct timer_list  uart_hdmi_timer;
struct work_struct work_update;
struct device *hdmirx_dev;
unsigned int hu_number_choise;

unsigned char power_gpio_det0_times[3] = {0, 0, 0};
unsigned char power_gpio_det1_times[3] = {0, 0, 0};
unsigned char hpd_gpio_mode_flag[3] = {1, 1, 1};

unsigned char uart_gpio_into_times[3] = {0, 0, 0};
unsigned char uart_gpio_out_times[3] = {0, 0, 0};
char uart_pre_status[3] = {-1, -1, -1};

enum hdmi_uart_function_select {
	es_uart = 0,
	es_hdmi = 1,
};

enum hdmi_uart_status {
	enum_hu_status_out = 0,
	enum_hu_status_in = 1,
	enum_hu_status_null = 2,
};

unsigned char uart_previous_status[3] = {
	enum_hu_status_null,
	enum_hu_status_null,
	enum_hu_status_null,
};
unsigned char uart_current_status[3] = {
	enum_hu_status_out,
	enum_hu_status_out,
	enum_hu_status_out,
};

/*
	[3][2]: 3 means 3 hdmirx channel
			2 means hpd pin and hdmirx pins function select.
			(hdmi-in / uart functions.)
	uart function: array value set 0;
	hdmi function: array value set 1.
*/
unsigned char hdmi_uart_status_flag[3][2] = {
	{es_hdmi, es_hdmi},
	{es_hdmi, es_hdmi},
	{es_hdmi, es_hdmi},
};

#define PIN_SET_S(a, b, c, d, e, f)\
	((a<<0) | (b<<1) | (c<<2) | (d<<3) | (e<<4) | (f<<5))

struct hdmi_uart_all_state {
	unsigned char hdmi_uart_all_state_gather;
	char *pinmux_setting;
};
static const struct hdmi_uart_all_state hdmi_uart_all_state_arr[] = {
	{PIN_SET_S(es_uart, es_hdmi, es_uart, es_hdmi, es_uart, es_hdmi),
		"hu_det_none"},
	{PIN_SET_S(es_uart, es_uart, es_uart, es_hdmi, es_uart, es_hdmi),
		"hu_det_u1"},
	{PIN_SET_S(es_uart, es_hdmi, es_uart, es_uart, es_uart, es_hdmi),
		"hu_det_u2"},
	{PIN_SET_S(es_uart, es_hdmi, es_uart, es_hdmi, es_uart, es_uart),
		"hu_det_u3"},

	{PIN_SET_S(es_hdmi, es_hdmi, es_uart, es_hdmi, es_uart, es_hdmi),
		"hu_det_h1"},
	{PIN_SET_S(es_uart, es_hdmi, es_hdmi, es_hdmi, es_uart, es_hdmi),
		"hu_det_h2"},
	{PIN_SET_S(es_uart, es_hdmi, es_uart, es_hdmi, es_hdmi, es_hdmi),
		"hu_det_h3"},
	{PIN_SET_S(es_hdmi, es_hdmi, es_hdmi, es_hdmi, es_uart, es_hdmi),
		"hu_det_h1h2"},
	{PIN_SET_S(es_hdmi, es_hdmi, es_uart, es_hdmi, es_hdmi, es_hdmi),
		"hu_det_h1h3"},
	{PIN_SET_S(es_uart, es_hdmi, es_hdmi, es_hdmi, es_hdmi, es_hdmi),
		"hu_det_h2h3"},
	{PIN_SET_S(es_hdmi, es_hdmi, es_hdmi, es_hdmi, es_hdmi, es_hdmi),
		"hu_det_h1h2h3"},

	{PIN_SET_S(es_uart, es_uart, es_hdmi, es_hdmi, es_uart, es_hdmi),
		"hu_det_u1_h2"},
	{PIN_SET_S(es_uart, es_uart, es_uart, es_hdmi, es_hdmi, es_hdmi),
		"hu_det_u1_h3"},
	{PIN_SET_S(es_uart, es_uart, es_hdmi, es_hdmi, es_hdmi, es_hdmi),
		"hu_det_u1_h2h3"},

	{PIN_SET_S(es_hdmi, es_hdmi, es_uart, es_uart, es_uart, es_hdmi),
		"hu_det_u2_h1"},
	{PIN_SET_S(es_uart, es_hdmi, es_uart, es_uart, es_hdmi, es_hdmi),
		"hu_det_u2_h3"},
	{PIN_SET_S(es_hdmi, es_hdmi, es_uart, es_uart, es_hdmi, es_hdmi),
		"hu_det_u2_h1h3"},

	{PIN_SET_S(es_hdmi, es_hdmi, es_uart, es_hdmi, es_uart, es_uart),
		"hu_det_u3_h1"},
	{PIN_SET_S(es_uart, es_hdmi, es_hdmi, es_hdmi, es_uart, es_uart),
		"hu_det_u3_h2"},
	{PIN_SET_S(es_hdmi, es_hdmi, es_hdmi, es_hdmi, es_uart, es_uart),
		"hu_det_u3_h1h2"},
};



void hdmi_uart_board_set_hdmiin_pinmux(unsigned char port, unsigned char status)
{
	if (status == enum_hu_status_in) {
		switch (port) {
		case 0:
			hdmi_uart_status_flag[0][1] = es_uart;
			schedule_work(&work_update);
			break;
		case 1:
			hdmi_uart_status_flag[1][1] = es_uart;
			schedule_work(&work_update);
			break;
		case 2:
			hdmi_uart_status_flag[2][1] = es_uart;
			schedule_work(&work_update);
			break;
		default:
			break;
		}
	} else {
		switch (port) {
		case 0:
			hdmi_uart_status_flag[0][1] = es_hdmi;
			schedule_work(&work_update);
			break;
		case 1:
			hdmi_uart_status_flag[1][1] = es_hdmi;
			schedule_work(&work_update);
			break;
		case 2:
			hdmi_uart_status_flag[2][1] = es_hdmi;
			schedule_work(&work_update);
			break;
		default:
			break;
		}
	}
}

void hdmi_uart_board_func(unsigned char port_choise, char flag)
{
	switch (flag) {
	case 0:
		if (uart_gpio_into_times[port_choise]++ >= 10) {
			uart_gpio_out_times[port_choise] = 0;
			uart_gpio_into_times[port_choise] = 0;
			uart_current_status[port_choise] = enum_hu_status_in;
			if (uart_current_status[port_choise] !=
				uart_previous_status[port_choise]) {
				uart_previous_status[port_choise] =
					uart_current_status[port_choise];
				/*printk("port %d uart in\n",port_choise);*/
				hdmi_uart_board_set_hdmiin_pinmux(port_choise,
					enum_hu_status_in);
			}
		}
		break;
	case 1:
		if (uart_gpio_out_times[port_choise]++ >= 3) {
			uart_gpio_out_times[port_choise] = 0;
			uart_gpio_into_times[port_choise] = 0;
			uart_current_status[port_choise] = enum_hu_status_out;
			if ((uart_current_status[port_choise] !=
				uart_previous_status[port_choise]) &&
				(uart_previous_status[port_choise] !=
				enum_hu_status_null)) {
				uart_previous_status[port_choise] =
					uart_current_status[port_choise];
				/*printk("port %d uart out\n",port_choise);*/
				hdmi_uart_board_set_hdmiin_pinmux(port_choise,
					enum_hu_status_out);
			}
		}
		break;
	default:
		break;
	}
}


void hdmi_uart_board_detect(void)
{
	int uart_value[3];

	if (rx.portA_pow5v_state != uart_pre_status[0]) {
		if (rx.portA_pow5v_state == 0) {
			power_gpio_det1_times[0] = 0;
			if (power_gpio_det0_times[0]++ >= 5) {
				hpd_gpio_mode_flag[0] = 0;
				uart_pre_status[0] = rx.portA_pow5v_state;
				hdmi_uart_status_flag[0][0] = es_uart;
				schedule_work(&work_update);
			}
		} else if (rx.portA_pow5v_state == 4) {
			power_gpio_det0_times[0] = 0;
			if (power_gpio_det1_times[0]++ >= 0) {
				hpd_gpio_mode_flag[0] = 1;
				uart_pre_status[0] = rx.portA_pow5v_state;
				hdmi_uart_status_flag[0][0] = es_hdmi;
				schedule_work(&work_update);
			}
		}
	}
	if (rx.portB_pow5v_state != uart_pre_status[1]) {
		if (rx.portB_pow5v_state == 0) {
			power_gpio_det1_times[1] = 0;
			if (power_gpio_det0_times[1]++ >= 5) {
				hpd_gpio_mode_flag[1] = 0;
				uart_pre_status[1] = rx.portB_pow5v_state;
				hdmi_uart_status_flag[1][0] = es_uart;
				schedule_work(&work_update);
			}
		} else if (rx.portB_pow5v_state == 4) {
			power_gpio_det0_times[1] = 0;
			if (power_gpio_det1_times[1]++ >= 0) {
				hpd_gpio_mode_flag[1] = 1;
				uart_pre_status[1] = rx.portB_pow5v_state;
				hdmi_uart_status_flag[1][0] = es_hdmi;
				schedule_work(&work_update);
			}
		}
	}
	if (rx.portC_pow5v_state != uart_pre_status[2]) {
		if (rx.portC_pow5v_state == 0) {
			power_gpio_det1_times[2] = 0;
			if (power_gpio_det0_times[2]++ >= 5) {
				hpd_gpio_mode_flag[2] = 0;
				uart_pre_status[2] = rx.portC_pow5v_state;
				hdmi_uart_status_flag[2][0] = es_uart;
				schedule_work(&work_update);
			}
		} else if (rx.portC_pow5v_state == 4) {
			power_gpio_det0_times[2] = 0;
			if (power_gpio_det1_times[2]++ >= 0) {
				hpd_gpio_mode_flag[2] = 1;
				uart_pre_status[2] = rx.portC_pow5v_state;
				hdmi_uart_status_flag[2][0] = es_hdmi;
				schedule_work(&work_update);
			}
		}
	}

	uart_value[0] =
		(hpd_gpio_mode_flag[0] == 1)?0:gpiod_get_value(g_uart_pin[0]);
	uart_value[1] =
		(hpd_gpio_mode_flag[1] == 1)?0:gpiod_get_value(g_uart_pin[1]);
	uart_value[2] =
		(hpd_gpio_mode_flag[2] == 1)?0:gpiod_get_value(g_uart_pin[2]);

	if ((rx.portA_pow5v_state == 0) && uart_value[0])
		hdmi_uart_board_func(0, 0);
	else
		hdmi_uart_board_func(0, 1);

	if ((rx.portB_pow5v_state == 0) && uart_value[1])
		hdmi_uart_board_func(1, 0);
	else
		hdmi_uart_board_func(1, 1);

	if ((rx.portC_pow5v_state == 0) && uart_value[2])
		hdmi_uart_board_func(2, 0);
	else
		hdmi_uart_board_func(2, 1);
}


void uart_hdmirx_timer_handler(unsigned long arg)
{
	/*if (into_hdmirx_flag == 1)
		into_hdmirx_flag = 0;
	else*/
		HPD_controller();

	hdmi_uart_board_detect();
	uart_hdmi_timer.expires = jiffies + TIMER_STATE_CHECK;
	add_timer(&uart_hdmi_timer);
}

void hdmirx_uart_mode_switch(void)
{
	int i = 0;
	struct pinctrl *p;
	unsigned char value = 0;
	static unsigned char pre_value;

	if (!(hu_number_choise & 0x1)) {
		hdmi_uart_status_flag[0][0] = es_hdmi;
		hdmi_uart_status_flag[0][1] = es_hdmi;
	}
	if (!(hu_number_choise & 0x2)) {
		hdmi_uart_status_flag[1][0] = es_hdmi;
		hdmi_uart_status_flag[1][1] = es_hdmi;
	}
	if (!(hu_number_choise & 0x4)) {
		hdmi_uart_status_flag[2][0] = es_hdmi;
		hdmi_uart_status_flag[2][1] = es_hdmi;
	}
	value = PIN_SET_S(
			hdmi_uart_status_flag[0][0],
			hdmi_uart_status_flag[0][1],
			hdmi_uart_status_flag[1][0],
			hdmi_uart_status_flag[1][1],
			hdmi_uart_status_flag[2][0],
			hdmi_uart_status_flag[2][1]
			);
	if (pre_value == value) {
		pr_info("hdmi->pinmux switch:same value.\n");
		return;
	} else
		pre_value = value;

	for (i = 0; i < ARRAY_SIZE(hdmi_uart_all_state_arr); i++) {
		if (value ==
			hdmi_uart_all_state_arr[i].hdmi_uart_all_state_gather)
			break;
	}

	if (i == ARRAY_SIZE(hdmi_uart_all_state_arr)) {
		pr_info("hdmi->pinmux switch:can not find.\n");
		return;
	}

	pr_info("hdmi->pinmux switch:%s\n",
		hdmi_uart_all_state_arr[i].pinmux_setting);
	p = devm_pinctrl_get_select(hdmirx_dev,
		hdmi_uart_all_state_arr[i].pinmux_setting);
	if (IS_ERR(p))
		pr_info("pinmux_setting fail, %ld\n", PTR_ERR(p));

	pr_info("aml_uart_hdmi_pinctrl_notifier end.\n");
}

static void update_work_mode_switch(struct work_struct *work)
{
	hdmirx_uart_mode_switch();
}

int uart_hdmi_probe(struct platform_device *pdev)
{
	int ret;
	hdmirx_dev = &pdev->dev;

	ret = of_property_read_u32(pdev->dev.of_node,
			"hdmiuart_use_num", &(hu_number_choise));
	if (ret) {
		pr_err("%s:don't find hu_nums.\n", __func__);
		return 1;
	} else if (hu_number_choise == 0) {
		pr_err("%s:hu_numbers = 0.\n", __func__);
		return 1;
	}
	pr_info("%s:hu_numbers = %d\n", __func__, hu_number_choise);

	g_uart_pin[0] = of_get_named_gpiod_flags(pdev->dev.of_node,
			"hdmiuart1_pin", 0, NULL);
	g_uart_pin[1] = of_get_named_gpiod_flags(pdev->dev.of_node,
			"hdmiuart2_pin", 0, NULL);
	g_uart_pin[2] = of_get_named_gpiod_flags(pdev->dev.of_node,
			"hdmiuart3_pin", 0, NULL);

	INIT_WORK(&work_update, update_work_mode_switch);

	/* init timer */
	init_timer(&uart_hdmi_timer);
	uart_hdmi_timer.data = 0;
	uart_hdmi_timer.function = uart_hdmirx_timer_handler;
	uart_hdmi_timer.expires = jiffies + TIMER_STATE_CHECK;
	add_timer(&uart_hdmi_timer);
	pr_info("uart_hdmi_probe ok!\n");
	return 0;
}

static int uart_hdmi_suspend(struct platform_device *pdev ,
	pm_message_t state)
{
	return 0;
}

static int uart_hdmi_resume(struct platform_device *pdev)
{
	return 0;
}

int uart_hdmi_remove(struct platform_device *pdev)
{
	del_timer_sync(&uart_hdmi_timer);
	cancel_work_sync(&work_update);
	gpio_free(desc_to_gpio(g_uart_pin[0]));
	gpio_free(desc_to_gpio(g_uart_pin[1]));
	gpio_free(desc_to_gpio(g_uart_pin[2]));
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id uart_hdmi_dt_match[] = {
	{	.compatible = "amlogic, hdmirx_uart",
	},
	{},
};
#else
#define uart_hdmi_dt_match NULL
#endif

static struct platform_driver uart_hdmirx_det_driver = {
	.probe      = uart_hdmi_probe,
	.remove     = uart_hdmi_remove,
	.suspend    = uart_hdmi_suspend,
	.resume     = uart_hdmi_resume,
	.driver     = {
		.name   = "uart_hdmi",
		.of_match_table = uart_hdmi_dt_match,
	},
};

static int __init uart_hdmi_det_init(void)
{
	return platform_driver_register(&uart_hdmirx_det_driver);
}

static void __exit uart_hdmi_det_exit(void)
{
	platform_driver_unregister(&uart_hdmirx_det_driver);
}

module_init(uart_hdmi_det_init);
module_exit(uart_hdmi_det_exit);

MODULE_DESCRIPTION("Meson Hdmirx-Uart Detect Driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Amlogic, Inc.");
