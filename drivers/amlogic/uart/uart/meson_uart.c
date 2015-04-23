/*
 *  Based on meson_uart.c, by AMLOGIC, INC.
 *
 * Copyright (C) 2014 Carlo Caione <carlo@caione.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/clk.h>
#include <linux/console.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/reset.h>
#include <linux/serial.h>
#include <linux/serial_core.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/pinctrl/consumer.h>
#include <linux/clk-private.h>
#include <linux/clk-provider.h>

/* Register offsets */
#define AML_UART_WFIFO			0x00
#define AML_UART_RFIFO			0x04
#define AML_UART_CONTROL		0x08
#define AML_UART_STATUS			0x0c
#define AML_UART_MISC			0x10
#define AML_UART_REG5			0x14

/* AML_UART_CONTROL bits */
#define AML_UART_TX_EN			BIT(12)
#define AML_UART_RX_EN			BIT(13)
#define AML_UART_TX_RST			BIT(22)
#define AML_UART_RX_RST			BIT(23)
#define AML_UART_CLR_ERR		BIT(24)
#define AML_UART_RX_INT_EN		BIT(27)
#define AML_UART_TX_INT_EN		BIT(28)
#define AML_UART_DATA_LEN_MASK		(0x03 << 20)
#define AML_UART_DATA_LEN_8BIT		(0x00 << 20)
#define AML_UART_DATA_LEN_7BIT		(0x01 << 20)
#define AML_UART_DATA_LEN_6BIT		(0x02 << 20)
#define AML_UART_DATA_LEN_5BIT		(0x03 << 20)

/* AML_UART_STATUS bits */
#define AML_UART_PARITY_ERR		BIT(16)
#define AML_UART_FRAME_ERR		BIT(17)
#define AML_UART_TX_FIFO_WERR		BIT(18)
#define AML_UART_RX_EMPTY		BIT(20)
#define AML_UART_TX_FULL		BIT(21)
#define AML_UART_TX_EMPTY		BIT(22)
#define AML_UART_ERR			(AML_UART_PARITY_ERR | \
					 AML_UART_FRAME_ERR  | \
					 AML_UART_TX_FIFO_WERR)

/* AML_UART_CONTROL bits */
#define AML_UART_TWO_WIRE_EN		BIT(15)
#define AML_UART_PARITY_TYPE		BIT(18)
#define AML_UART_PARITY_EN		BIT(19)
#define AML_UART_CLEAR_ERR		BIT(24)
#define AML_UART_STOP_BIN_LEN_MASK	(0x03 << 16)
#define AML_UART_STOP_BIN_1SB		(0x00 << 16)
#define AML_UART_STOP_BIN_2SB		(0x01 << 16)

/* AML_UART_MISC bits */
#define AML_UART_XMIT_IRQ(c)		(((c) & 0xff) << 8)
#define AML_UART_RECV_IRQ(c)		((c) & 0xff)

/* AML_UART_REG5 bits */
#define AML_UART_BAUD_MASK		0x7fffff
#define AML_UART_BAUD_USE		BIT(23)

#define AML_UART_PORT_NUM		6
#define AML_UART_DEV_NAME		"ttyS"

static struct uart_driver meson_uart_driver;

static struct uart_port *meson_ports[AML_UART_PORT_NUM];

static void meson_uart_set_mctrl(struct uart_port *port, unsigned int mctrl)
{
}

static unsigned int meson_uart_get_mctrl(struct uart_port *port)
{
	return TIOCM_CTS;
}

static unsigned int meson_uart_tx_empty(struct uart_port *port)
{
	u32 val;

	val = readl(port->membase + AML_UART_STATUS);
	return (val & AML_UART_TX_EMPTY) ? TIOCSER_TEMT : 0;
}

static void meson_uart_stop_tx(struct uart_port *port)
{
	u32 val;

	val = readl(port->membase + AML_UART_CONTROL);
	val &= ~AML_UART_TX_EN;
	writel(val, port->membase + AML_UART_CONTROL);
}

static void meson_uart_stop_rx(struct uart_port *port)
{
	u32 val;

	val = readl(port->membase + AML_UART_CONTROL);
	val &= ~AML_UART_RX_EN;
	writel(val, port->membase + AML_UART_CONTROL);
}

static void meson_uart_shutdown(struct uart_port *port)
{
	unsigned long flags;
	u32 val;

	if (port->line == 0)
		return;

	spin_lock_irqsave(&port->lock, flags);

	val = readl(port->membase + AML_UART_CONTROL);
	val &= ~(AML_UART_RX_EN | AML_UART_TX_EN);
	val &= ~(AML_UART_RX_INT_EN | AML_UART_TX_INT_EN);
	writel(val, port->membase + AML_UART_CONTROL);

	spin_unlock_irqrestore(&port->lock, flags);
}

static void meson_uart_start_tx(struct uart_port *port)
{
	struct circ_buf *xmit = &port->state->xmit;
	unsigned int ch;

	while (!uart_circ_empty(xmit)) {
		if (!(readl(port->membase + AML_UART_STATUS) &
			AML_UART_TX_FULL)) {
			ch = xmit->buf[xmit->tail];
			writel(ch, port->membase + AML_UART_WFIFO);
			xmit->tail = (xmit->tail + 1) & (SERIAL_XMIT_SIZE - 1);
			port->icount.tx++;
		} else
			break;
	}
}

static void meson_transmit_chars(struct uart_port *port)
{
	struct circ_buf *xmit = &port->state->xmit;
	unsigned int ch;
	int count = 256;

	if (port->x_char) {
		writel(port->x_char, port->membase + AML_UART_WFIFO);
		port->icount.tx++;
		port->x_char = 0;
		goto clear_and_return;
	}

	if (uart_circ_empty(xmit) || uart_tx_stopped(port))
		goto clear_and_return;

	while (!uart_circ_empty(xmit) && count-- > 0) {
		if (!(readl(port->membase + AML_UART_STATUS) &
			AML_UART_TX_FULL)) {
			ch = xmit->buf[xmit->tail];
			writel(ch, port->membase + AML_UART_WFIFO);
			xmit->tail = (xmit->tail + 1) & (SERIAL_XMIT_SIZE - 1);
			port->icount.tx++;
		} else
			break;
	}

	if (uart_circ_chars_pending(xmit) < WAKEUP_CHARS)
		uart_write_wakeup(port);

 clear_and_return:
	return;

}

static void meson_receive_chars(struct uart_port *port)
{
	struct tty_port *tport = &port->state->port;
	char flag;
	u32 status, ch, mode;

	do {
		flag = TTY_NORMAL;
		port->icount.rx++;
		status = readl(port->membase + AML_UART_STATUS);

		if (status & AML_UART_ERR) {
			if (status & AML_UART_TX_FIFO_WERR)
				port->icount.overrun++;
			else if (status & AML_UART_FRAME_ERR)
				port->icount.frame++;
			else if (status & AML_UART_PARITY_ERR)
				port->icount.frame++;

			mode = readl(port->membase + AML_UART_CONTROL);
			mode |= AML_UART_CLEAR_ERR;
			writel(mode, port->membase + AML_UART_CONTROL);

			/* It doesn't clear to 0 automatically */
			mode &= ~AML_UART_CLEAR_ERR;
			writel(mode, port->membase + AML_UART_CONTROL);

			status &= port->read_status_mask;
			if (status & AML_UART_FRAME_ERR)
				flag = TTY_FRAME;
			else if (status & AML_UART_PARITY_ERR)
				flag = TTY_PARITY;
		}

		ch = readl(port->membase + AML_UART_RFIFO);
		ch &= 0xff;

		if ((status & port->ignore_status_mask) == 0)
			tty_insert_flip_char(tport, ch, flag);

		if (status & AML_UART_TX_FIFO_WERR)
			tty_insert_flip_char(tport, 0, TTY_OVERRUN);

	} while (!(readl(port->membase + AML_UART_STATUS) & AML_UART_RX_EMPTY));

	spin_unlock(&port->lock);
	tty_flip_buffer_push(tport);
	spin_lock(&port->lock);
}

static irqreturn_t meson_uart_interrupt(int irq, void *dev_id)
{
	struct uart_port *port = (struct uart_port *)dev_id;
	u32 val;

	spin_lock(&port->lock);

	val = readl(port->membase + AML_UART_CONTROL);
	if (!(readl(port->membase + AML_UART_STATUS) & AML_UART_RX_EMPTY))
		meson_receive_chars(port);

	if ((val & AML_UART_TX_EN)
	    && (!(readl(port->membase + AML_UART_STATUS) & AML_UART_TX_FULL)))
		meson_transmit_chars(port);

	spin_unlock(&port->lock);

	return IRQ_HANDLED;
}

static const char *meson_uart_type(struct uart_port *port)
{
	return (port->type == PORT_MESON) ? "meson_uart" : NULL;
}

static int meson_uart_startup(struct uart_port *port)
{
	u32 val;
	int ret = 0;
	val = readl(port->membase + AML_UART_CONTROL);
	val |= (AML_UART_RX_RST | AML_UART_TX_RST | AML_UART_CLR_ERR);
	writel(val, port->membase + AML_UART_CONTROL);

	val &= ~(AML_UART_RX_RST | AML_UART_TX_RST | AML_UART_CLR_ERR);
	writel(val, port->membase + AML_UART_CONTROL);

	val |= (AML_UART_RX_EN | AML_UART_TX_EN);
	writel(val, port->membase + AML_UART_CONTROL);

	val |= (AML_UART_RX_INT_EN | AML_UART_TX_INT_EN);
	writel(val, port->membase + AML_UART_CONTROL);

	return ret;
}

static void meson_uart_change_speed(struct uart_port *port, unsigned long baud)
{
	u32 val;
	while (!(readl(port->membase + AML_UART_STATUS) & AML_UART_TX_EMPTY))
		cpu_relax();

	val = readl(port->membase + AML_UART_REG5);
	val &= ~AML_UART_BAUD_MASK;
	val = ((port->uartclk * 10 / (baud * 4) + 5) / 10) - 1;
	val |= AML_UART_BAUD_USE;
	writel(val, port->membase + AML_UART_REG5);
}

static void meson_uart_set_termios(struct uart_port *port,
				   struct ktermios *termios,
				   struct ktermios *old)
{
	unsigned int cflags, iflags, baud;
	unsigned long flags;
	u32 val;
	spin_lock_irqsave(&port->lock, flags);

	cflags = termios->c_cflag;
	iflags = termios->c_iflag;

	val = readl(port->membase + AML_UART_CONTROL);

	val &= ~AML_UART_DATA_LEN_MASK;
	switch (cflags & CSIZE) {
	case CS8:
		val |= AML_UART_DATA_LEN_8BIT;
		break;
	case CS7:
		val |= AML_UART_DATA_LEN_7BIT;
		break;
	case CS6:
		val |= AML_UART_DATA_LEN_6BIT;
		break;
	case CS5:
		val |= AML_UART_DATA_LEN_5BIT;
		break;
	}

	if (cflags & PARENB)
		val |= AML_UART_PARITY_EN;
	else
		val &= ~AML_UART_PARITY_EN;

	if (cflags & PARODD)
		val |= AML_UART_PARITY_TYPE;
	else
		val &= ~AML_UART_PARITY_TYPE;

	val &= ~AML_UART_STOP_BIN_LEN_MASK;
	if (cflags & CSTOPB)
		val |= AML_UART_STOP_BIN_2SB;
	else
		val &= ~AML_UART_STOP_BIN_1SB;

	if (cflags & CRTSCTS)
		val &= ~AML_UART_TWO_WIRE_EN;
	else
		val |= AML_UART_TWO_WIRE_EN;

	writel(val, port->membase + AML_UART_CONTROL);

	baud = uart_get_baud_rate(port, termios, old, 9600, 115200);
	meson_uart_change_speed(port, baud);

	port->read_status_mask = AML_UART_TX_FIFO_WERR;
	if (iflags & INPCK)
		port->read_status_mask |= AML_UART_PARITY_ERR |
		    AML_UART_FRAME_ERR;

	port->ignore_status_mask = 0;
	if (iflags & IGNPAR)
		port->ignore_status_mask |= AML_UART_PARITY_ERR |
		    AML_UART_FRAME_ERR;

	uart_update_timeout(port, termios->c_cflag, baud);
	spin_unlock_irqrestore(&port->lock, flags);
}

static int meson_uart_verify_port(struct uart_port *port,
				  struct serial_struct *ser)
{
	int ret = 0;

	if (port->type != PORT_MESON)
		ret = -EINVAL;
	if (port->irq != ser->irq)
		ret = -EINVAL;
	if (ser->baud_base < 9600)
		ret = -EINVAL;
	return ret;
}

static void meson_uart_release_port(struct uart_port *port)
{
	if (port->flags & UPF_IOREMAP) {
		iounmap(port->membase);
		port->membase = NULL;
	}
}

static int meson_uart_request_port(struct uart_port *port)
{
	struct platform_device *pdev = to_platform_device(port->dev);
	struct resource *res;
	int size, ret;
	u32 val;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "cannot obtain I/O memory region");
		return -ENODEV;
	}
	size = resource_size(res);

	if (!devm_request_mem_region(port->dev, port->mapbase, size,
				     dev_name(port->dev))) {
		dev_err(port->dev, "Memory region busy\n");
		return -EBUSY;
	}

	if (port->flags & UPF_IOREMAP) {
		port->membase = devm_ioremap_nocache(port->dev,
						     port->mapbase, size);
		if (port->membase == NULL)
			return -ENOMEM;
	}

	pr_info("==uart%d reg addr = %p\n", port->line, port->membase);
	val = (AML_UART_RECV_IRQ(1) | AML_UART_XMIT_IRQ(port->fifosize / 2));
	writel(val, port->membase + AML_UART_MISC);

	ret = request_irq(port->irq, meson_uart_interrupt, 0,
			  meson_uart_type(port), port);
	return 0;
}

static void meson_uart_config_port(struct uart_port *port, int flags)
{
	if (flags & UART_CONFIG_TYPE) {
		port->type = PORT_MESON;
		meson_uart_request_port(port);
	}
}

static struct uart_ops meson_uart_ops = {
	.set_mctrl = meson_uart_set_mctrl,
	.get_mctrl = meson_uart_get_mctrl,
	.tx_empty = meson_uart_tx_empty,
	.start_tx = meson_uart_start_tx,
	.stop_tx = meson_uart_stop_tx,
	.stop_rx = meson_uart_stop_rx,
	.startup = meson_uart_startup,
	.shutdown = meson_uart_shutdown,
	.set_termios = meson_uart_set_termios,
	.type = meson_uart_type,
	.config_port = meson_uart_config_port,
	.request_port = meson_uart_request_port,
	.release_port = meson_uart_release_port,
	.verify_port = meson_uart_verify_port,
};

#ifdef CONFIG_SERIAL_MESON_CONSOLE

static void meson_console_putchar(struct uart_port *port, int ch)
{
	if (!port->membase)
		return;

	while (readl(port->membase + AML_UART_STATUS) & AML_UART_TX_FULL)
		cpu_relax();
	writel(ch, port->membase + AML_UART_WFIFO);
}

static void meson_serial_console_write(struct console *co, const char *s,
				       u_int count)
{
	struct uart_port *port;
	unsigned long flags;
	int locked;

	port = meson_ports[co->index];
	if (!port)
		return;

	local_irq_save(flags);
	if (port->sysrq) {
		locked = 0;
	} else if (oops_in_progress) {
		locked = spin_trylock(&port->lock);
	} else {
		spin_lock(&port->lock);
		locked = 1;
	}

	uart_console_write(port, s, count, meson_console_putchar);

	if (locked)
		spin_unlock(&port->lock);
	local_irq_restore(flags);
}

static int meson_serial_console_setup(struct console *co, char *options)
{
	struct uart_port *port;
	int baud = 115200;
	int bits = 8;
	int parity = 'n';
	int flow = 'n';

	if (co->index < 0 || co->index >= AML_UART_PORT_NUM)
		return -EINVAL;

	port = meson_ports[co->index];
	if (!port || !port->membase)
		return -ENODEV;

	if (options)
		uart_parse_options(options, &baud, &parity, &bits, &flow);

	return uart_set_options(port, co, baud, parity, bits, flow);
}

static struct console meson_serial_console = {
	.name = AML_UART_DEV_NAME,
	.write = meson_serial_console_write,
	.device = uart_console_device,
	.setup = meson_serial_console_setup,
	.flags = CON_PRINTBUFFER,
	.index = -1,
	.data = &meson_uart_driver,
};

static int __init meson_serial_console_init(void)
{
	register_console(&meson_serial_console);
	return 0;
}

console_initcall(meson_serial_console_init);

#define MESON_SERIAL_CONSOLE	(&meson_serial_console)
#else
#define MESON_SERIAL_CONSOLE	NULL
#endif

static struct uart_driver meson_uart_driver = {
	.owner = THIS_MODULE,
	.driver_name = "meson_uart",
	.dev_name = AML_UART_DEV_NAME,
	.nr = AML_UART_PORT_NUM,
	.cons = MESON_SERIAL_CONSOLE,
};

static int meson_uart_probe(struct platform_device *pdev)
{
	struct resource *res_mem, *res_irq;
	struct uart_port *port;
	struct clk *clk;
	const void *prop;
	struct reset_control *uart_rst;
	struct pinctrl *p;
	int ret = 0;

	if (pdev->dev.of_node)
		pdev->id = of_alias_get_id(pdev->dev.of_node, "serial");

	if (pdev->id < 0 || pdev->id >= AML_UART_PORT_NUM)
		return -EINVAL;

	res_mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res_mem)
		return -ENODEV;

	res_irq = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res_irq)
		return -ENODEV;

	if (meson_ports[pdev->id]) {
		dev_err(&pdev->dev, "port %d already allocated\n", pdev->id);
		return -EBUSY;
	}

	port = devm_kzalloc(&pdev->dev, sizeof(struct uart_port), GFP_KERNEL);
	if (!port)
		return -ENOMEM;

	clk = clk_get(&pdev->dev, "clk_uart");
	if (IS_ERR(clk)) {
		pr_err("%s: clock not found\n", dev_name(&pdev->dev));
		return PTR_ERR(clk);
	}
	ret = clk_prepare_enable(clk);
	if (ret) {
		pr_err("uart: clock failed to prepare+enable: %d\n", ret);
		clk_put(clk);
		return ret;
	}

	uart_rst = devm_reset_control_get(&pdev->dev, NULL);
	if (!IS_ERR(uart_rst))
		reset_control_deassert(uart_rst);

	port->fifosize = 64;
	prop = of_get_property(pdev->dev.of_node, "fifosize", NULL);
	if (prop)
		port->fifosize = of_read_ulong(prop, 1);
	port->uartclk = clk->rate;
	port->iotype = UPIO_MEM;
	port->mapbase = res_mem->start;
	port->irq = res_irq->start;
	port->flags = UPF_BOOT_AUTOCONF | UPF_IOREMAP | UPF_LOW_LATENCY;
	port->dev = &pdev->dev;
	port->line = pdev->id;
	port->type = PORT_MESON;
	port->x_char = 0;
	port->ops = &meson_uart_ops;

	meson_ports[pdev->id] = port;
	platform_set_drvdata(pdev, port);
	if (of_get_property(pdev->dev.of_node, "pinctrl-names", NULL)) {
		p = devm_pinctrl_get_select_default(&pdev->dev);
		if (!p)
			return -1;
	}
	ret = uart_add_one_port(&meson_uart_driver, port);
	if (ret)
		meson_ports[pdev->id] = NULL;

	return ret;
}

static int meson_uart_remove(struct platform_device *pdev)
{
	struct uart_port *port;

	port = platform_get_drvdata(pdev);
	uart_remove_one_port(&meson_uart_driver, port);
	meson_ports[pdev->id] = NULL;

	return 0;
}

static const struct of_device_id meson_uart_dt_match[] = {
	{.compatible = "amlogic, meson-uart"},
	{ /* sentinel */ },
};

MODULE_DEVICE_TABLE(of, meson_uart_dt_match);

static struct platform_driver meson_uart_platform_driver = {
	.probe = meson_uart_probe,
	.remove = meson_uart_remove,
	.driver = {
		   .owner = THIS_MODULE,
		   .name = "meson_uart",
		   .of_match_table = meson_uart_dt_match,
		   },
};

static int __init meson_uart_init(void)
{
	int ret;

	ret = uart_register_driver(&meson_uart_driver);
	if (ret)
		return ret;

	ret = platform_driver_register(&meson_uart_platform_driver);
	if (ret)
		uart_unregister_driver(&meson_uart_driver);

	return ret;
}

static void __exit meson_uart_exit(void)
{
	platform_driver_unregister(&meson_uart_platform_driver);
	uart_unregister_driver(&meson_uart_driver);
}

module_init(meson_uart_init);
module_exit(meson_uart_exit);

MODULE_AUTHOR("Carlo Caione <carlo@caione.org>");
MODULE_DESCRIPTION("Amlogic Meson serial port driver");
MODULE_LICENSE("GPL v2");
