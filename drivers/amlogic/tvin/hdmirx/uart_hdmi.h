#ifndef __UART_HDMI_H
#define __UART_HDMI_H
#include <linux/platform_device.h>
#include <linux/notifier.h>

#define BOARD_MOD_NAME  "hdmi_uart_board"
extern int uart_hdmi_probe(struct platform_device *pdev);
extern void HPD_controller(void);

#endif
