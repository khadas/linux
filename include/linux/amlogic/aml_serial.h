#ifndef _AML_SERIAL_H
#define _AML_SERIAL_H

#include <linux/bitops.h>

/* Based on AO_RTI_STATUS0 , 0xC8100000 */
#define MESON_AO_UART0_WFIFO			(0x4C0)
#define MESON_AO_UART0_STATUS			(0x4CC)


#define MESON_UART_TX_FULL			BIT(21)

#endif
