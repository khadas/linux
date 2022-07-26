// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */
#define SKIP_IO_TRACE

#if defined(CONFIG_AMLOGIC_SERIAL_MESON_CONSOLE) && defined(CONFIG_MAGIC_SYSRQ)
#define SUPPORT_SYSRQ
#endif

#include <linux/clk.h>
#include <linux/console.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/serial.h>
#include <linux/serial_core.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/pinctrl/consumer.h>

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
#define AML_UART_RX_FIFO_OVERFLOW	BIT(24)
#define AML_UART_ERR			(AML_UART_PARITY_ERR | \
					 AML_UART_FRAME_ERR  | \
					 AML_UART_RX_FIFO_OVERFLOW)

/* AML_UART_CONTROL bits */
#define AML_UART_TWO_WIRE_EN		BIT(15)
#define AML_UART_PARITY_TYPE		BIT(18)
#define AML_UART_PARITY_EN		BIT(19)
#define AML_UART_CLEAR_ERR		BIT(24)
#define AML_UART_STOP_BIN_LEN_MASK	(0x03 << 16)
#define AML_UART_STOP_BIN_1SB		(0x00 << 16)
#define AML_UART_STOP_BIN_2SB		(0x01 << 16)
#define UART_CTS_EN		(0x01 << 31)

/* AML_UART_MISC bits */
#define AML_UART_XMIT_IRQ(c)		(((c) & 0xff) << 8)
#define AML_UART_RECV_IRQ(c)		((c) & 0xff)

/* AML_UART_REG5 bits */
#define AML_UART_BAUD_MASK		0x7fffff
#define AML_UART_BAUD_USE		BIT(23)
#define AML_UART_BAUD_XTAL		BIT(24)
#define AML_UART_BAUD_XTAL_TICK	BIT(26)
#define AML_UART_BAUD_XTAL_DIV2	BIT(27)

#define AML_UART_PORT_MAX		12
#define AML_UART_PORT_OFFSET		6

#define AML_UART_DEV_NAME		"ttyS"

static struct uart_driver meson_uart_driver;
static int support_sysrq;
unsigned int xtal_tick_en;

struct meson_uart_port {
	struct uart_port	port;
	spinlock_t wr_lock;
	unsigned long baud;
	struct pinctrl *p;
	unsigned long for_bt;
};

static struct meson_uart_port *meson_ports[AML_UART_PORT_MAX];

#define to_meson_port(uport)  container_of(uport, struct meson_uart_port, port)

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

	val = readl_relaxed(port->membase + AML_UART_STATUS);
	return (val & AML_UART_TX_EMPTY) ? TIOCSER_TEMT : 0;
}

static void meson_uart_stop_tx(struct uart_port *port)
{
	u32 val;
	if (port->line == 0)
		return;
	val = readl_relaxed(port->membase + AML_UART_CONTROL);
	val &= ~AML_UART_TX_EN;
	writel_relaxed(val, port->membase + AML_UART_CONTROL);
}

static void meson_uart_stop_rx(struct uart_port *port)
{
	u32 val;

	val = readl_relaxed(port->membase + AML_UART_CONTROL);
	val &= ~AML_UART_RX_EN;
	writel_relaxed(val, port->membase + AML_UART_CONTROL);
}

static void meson_uart_shutdown(struct uart_port *port)
{
	unsigned long flags;
	u32 val;

	if (port->line == 0)
		return;

	spin_lock_irqsave(&port->lock, flags);

	val = readl_relaxed(port->membase + AML_UART_CONTROL);
	val &= ~(AML_UART_RX_EN | AML_UART_TX_EN);
	val &= ~(AML_UART_RX_INT_EN | AML_UART_TX_INT_EN);
	val |= UART_CTS_EN;
	writel_relaxed(val, port->membase + AML_UART_CONTROL);

	spin_unlock_irqrestore(&port->lock, flags);
}

static void meson_uart_start_tx(struct uart_port *port)
{
	struct circ_buf *xmit = &port->state->xmit;
	unsigned int ch;
	struct meson_uart_port *mup = to_meson_port(port);
	unsigned long flags;

	spin_lock_irqsave(&mup->wr_lock, flags);
	while (!uart_circ_empty(xmit)) {
		if (readl_relaxed(port->membase + AML_UART_STATUS)
			& AML_UART_TX_FULL)
			break;

		ch = xmit->buf[xmit->tail];
		writel_relaxed(ch, port->membase + AML_UART_WFIFO);
		xmit->tail = (xmit->tail + 1) & (SERIAL_XMIT_SIZE - 1);
		port->icount.tx++;
	}
	spin_unlock_irqrestore(&mup->wr_lock, flags);
}

static void meson_transmit_chars(struct uart_port *port)
{
	struct circ_buf *xmit = &port->state->xmit;
	struct meson_uart_port *mup = to_meson_port(port);
	unsigned int ch;
	int count = 256;

	spin_lock(&port->lock);
	if (port->x_char) {
		writel_relaxed(port->x_char, port->membase + AML_UART_WFIFO);
		port->icount.tx++;
		port->x_char = 0;
		goto clear_and_return;
	}

	if (uart_circ_empty(xmit) || uart_tx_stopped(port))
		goto clear_and_return;

	spin_lock(&mup->wr_lock);
	while (!uart_circ_empty(xmit) && count-- > 0) {
		if (readl_relaxed(port->membase + AML_UART_STATUS)
			& AML_UART_TX_FULL)
			break;
		ch = xmit->buf[xmit->tail];
		writel_relaxed(ch, port->membase + AML_UART_WFIFO);
		xmit->tail = (xmit->tail + 1) & (SERIAL_XMIT_SIZE - 1);
		port->icount.tx++;
	}
	spin_unlock(&mup->wr_lock);

	if (uart_circ_chars_pending(xmit) < WAKEUP_CHARS)
		uart_write_wakeup(port);

 clear_and_return:
	spin_unlock(&port->lock);
	return;

}

static void meson_receive_chars(struct uart_port *port)
{
	struct tty_port *tport = &port->state->port;
	char flag;
	u32 status, ch, mode;

	spin_lock(&port->lock);
	do {
		flag = TTY_NORMAL;
		port->icount.rx++;
		status = readl_relaxed(port->membase + AML_UART_STATUS);

		if (status & AML_UART_ERR) {
			if (status & AML_UART_RX_FIFO_OVERFLOW)
				port->icount.overrun++;
			else if (status & AML_UART_FRAME_ERR)
				port->icount.frame++;
			else if (status & AML_UART_PARITY_ERR)
				port->icount.frame++;

			mode = readl_relaxed(port->membase + AML_UART_CONTROL);
			mode |= AML_UART_CLEAR_ERR;
			writel_relaxed(mode, port->membase + AML_UART_CONTROL);

			/* It doesn't clear to 0 automatically */
			mode &= ~AML_UART_CLEAR_ERR;
			writel_relaxed(mode, port->membase + AML_UART_CONTROL);

			status &= port->read_status_mask;
			if (status & (AML_UART_FRAME_ERR |
				AML_UART_RX_FIFO_OVERFLOW))
				flag = TTY_FRAME;
			else if (status & AML_UART_PARITY_ERR)
				flag = TTY_PARITY;
		}

		ch = readl_relaxed(port->membase + AML_UART_RFIFO);
		ch &= 0xff;

#ifdef SUPPORT_SYSRQ
		if (support_sysrq == 1) {
			if (status == 0 && ch == 0) {
				port->icount.brk++;
				if (uart_handle_break(port))
					continue;
			}

			if (port->sysrq)
				flag = TTY_BREAK;

			if (uart_handle_sysrq_char(port, ch))
				continue;
		}
#endif

		uart_insert_char(port, status, AML_UART_RX_FIFO_OVERFLOW,
				 ch, flag);
		/*
		 * if ((status & port->ignore_status_mask) == 0)
		 *	tty_insert_flip_char(tport, ch, flag);
		 *
		 * if (status & AML_UART_TX_FIFO_WERR)
		 *	tty_insert_flip_char(tport, 0, TTY_OVERRUN);
		 */
	} while (!(readl_relaxed(port->membase + AML_UART_STATUS) & AML_UART_RX_EMPTY));
	spin_unlock(&port->lock);

	tty_flip_buffer_push(tport);

}

static irqreturn_t meson_uart_interrupt(int irq, void *dev_id)
{
	struct uart_port *port = (struct uart_port *)dev_id;
	u32 val;

	spin_lock(&port->lock);
	val = readl_relaxed(port->membase + AML_UART_CONTROL);
	spin_unlock(&port->lock);
	if (!(readl_relaxed(port->membase + AML_UART_STATUS) & AML_UART_RX_EMPTY))
		meson_receive_chars(port);

	if ((val & AML_UART_TX_EN) && (!(readl_relaxed(port->membase + AML_UART_STATUS) &
				AML_UART_TX_FULL)))
		meson_transmit_chars(port);

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

	val = readl_relaxed(port->membase + AML_UART_CONTROL);
	val |= (AML_UART_RX_RST | AML_UART_TX_RST | AML_UART_CLR_ERR);
	writel_relaxed(val, port->membase + AML_UART_CONTROL);

	val &= ~(AML_UART_RX_RST | AML_UART_TX_RST | AML_UART_CLR_ERR);
	writel_relaxed(val, port->membase + AML_UART_CONTROL);

	val |= (AML_UART_RX_EN | AML_UART_TX_EN);
	writel_relaxed(val, port->membase + AML_UART_CONTROL);

	val |= (AML_UART_RX_INT_EN | AML_UART_TX_INT_EN);
	val &= ~UART_CTS_EN;
	writel_relaxed(val, port->membase + AML_UART_CONTROL);


	return ret;
}

static void meson_uart_change_speed(struct uart_port *port, unsigned long baud)
{
	u32 val;
	struct meson_uart_port *mup = to_meson_port(port);
	struct platform_device *pdev = to_platform_device(port->dev);

	while (!(readl_relaxed(port->membase + AML_UART_STATUS) & AML_UART_TX_EMPTY))
		cpu_relax();

#ifdef UART_TEST_DEBUG
	if (port->line != 0)
		baud = 115200;
#endif
	val = readl_relaxed(port->membase + AML_UART_REG5);
	val &= ~AML_UART_BAUD_MASK;
	if (port->uartclk == 24000000) {
		if (xtal_tick_en) {
#ifdef UART_TEST_DEBUG
			dev_info(&pdev->dev, "ttyS%d use xtal(12M) %d change %ld to %ld\n",
				 port->line, port->uartclk / 2,
				 mup->baud, baud);
#endif
			val = (port->uartclk / 2 + baud / 2) / baud  - 1;
			val |= (AML_UART_BAUD_USE | AML_UART_BAUD_XTAL |
				AML_UART_BAUD_XTAL_DIV2);
		} else {
#ifdef UART_TEST_DEBUG
			dev_info(&pdev->dev, "ttyS%d use xtal(8M) %d change %ld to %ld\n",
				 port->line, port->uartclk / 3,
				 mup->baud, baud);
#endif
			val = ((port->uartclk / 3) + baud / 2) / baud  - 1;
			val &= (~(AML_UART_BAUD_XTAL_TICK |
				AML_UART_BAUD_XTAL_DIV2));
			val |= (AML_UART_BAUD_USE | AML_UART_BAUD_XTAL);
		}
	} else {
		dev_info(&pdev->dev, "ttyS%d use clk81 %d change %ld to %ld\n",
				port->line, port->uartclk,
				mup->baud, baud);
		val = ((port->uartclk * 10 / (baud * 4) + 5) / 10) - 1;
		val &= (~(AML_UART_BAUD_XTAL | AML_UART_BAUD_XTAL_TICK |
			AML_UART_BAUD_XTAL_DIV2));
		val |= AML_UART_BAUD_USE;
	}
	writel_relaxed(val, port->membase + AML_UART_REG5);

	mup->baud = baud;
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

	val = readl_relaxed(port->membase + AML_UART_CONTROL);

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

	writel_relaxed(val, port->membase + AML_UART_CONTROL);
	spin_unlock_irqrestore(&port->lock, flags);

	baud = uart_get_baud_rate(port, termios, old, 2400, 4000000);
	meson_uart_change_speed(port, baud);

	port->read_status_mask = AML_UART_RX_FIFO_OVERFLOW;
	if (iflags & INPCK)
		port->read_status_mask |= AML_UART_PARITY_ERR |
		    AML_UART_FRAME_ERR;

	port->ignore_status_mask = 0;
	if (iflags & IGNPAR)
		port->ignore_status_mask |= AML_UART_PARITY_ERR |
		    AML_UART_FRAME_ERR;

	uart_update_timeout(port, termios->c_cflag, baud);
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
	struct meson_uart_port *mup = to_meson_port(port);
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
		if (!port->membase)
			return -ENOMEM;
	}

	dev_info(&pdev->dev, "==uart%d reg addr = %p\n",
						port->line, port->membase);
	val = (AML_UART_RECV_IRQ(1) | AML_UART_XMIT_IRQ(port->fifosize / 2));
	writel_relaxed(val, port->membase + AML_UART_MISC);

	writel_relaxed(readl_relaxed(port->membase + AML_UART_CONTROL) | UART_CTS_EN,
			port->membase + AML_UART_CONTROL);

	if (mup->for_bt)
		ret = devm_request_threaded_irq(port->dev, port->irq, NULL,
						meson_uart_interrupt,
						IRQF_ONESHOT | IRQF_SHARED,
						meson_uart_type(port), port);
	else
		ret = devm_request_irq(port->dev, port->irq,
				       meson_uart_interrupt,
				       IRQF_SHARED, port->name, port);
	return ret;

}

static void meson_uart_config_port(struct uart_port *port, int flags)
{
	if (flags & UART_CONFIG_TYPE) {
		port->type = PORT_MESON;
		meson_uart_request_port(port);
	}
}

static const struct uart_ops meson_uart_ops = {
	.set_mctrl      = meson_uart_set_mctrl,
	.get_mctrl      = meson_uart_get_mctrl,
	.tx_empty	= meson_uart_tx_empty,
	.start_tx	= meson_uart_start_tx,
	.stop_tx	= meson_uart_stop_tx,
	.stop_rx	= meson_uart_stop_rx,
	.startup	= meson_uart_startup,
	.shutdown	= meson_uart_shutdown,
	.set_termios	= meson_uart_set_termios,
	.type		= meson_uart_type,
	.config_port	= meson_uart_config_port,
	.request_port	= meson_uart_request_port,
	.release_port	= meson_uart_release_port,
	.verify_port	= meson_uart_verify_port,
};

#ifdef CONFIG_AMLOGIC_SERIAL_MESON_CONSOLE
static void meson_uart_enable_tx_engine(struct uart_port *port)
{
	u32 val;

	val = readl_relaxed(port->membase + AML_UART_CONTROL);
	val |= AML_UART_TX_EN;
	writel_relaxed(val, port->membase + AML_UART_CONTROL);
}

static void meson_console_putchar(struct uart_port *port, int ch)
{
	if (!port->membase)
		return;

	while (readl_relaxed(port->membase + AML_UART_STATUS) &
	       AML_UART_TX_FULL)
		cpu_relax();
	writel_relaxed(ch, port->membase + AML_UART_WFIFO);
}

static void meson_serial_port_write(struct uart_port *port, const char *s,
				    u_int count)
{
	unsigned long flags;
	int locked;

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

static void meson_serial_console_write(struct console *co, const char *s,
				       u_int count)
{
	struct uart_port *port;

	port = &meson_ports[co->index]->port;
	if (!port)
		return;

	meson_serial_port_write(port, s, count);
}

static int meson_serial_console_setup(struct console *co, char *options)
{
	struct uart_port *port;
	int baud = 115200;
	int bits = 8;
	int parity = 'n';
	int flow = 'n';

	if (co->index < 0 || co->index >= AML_UART_PORT_MAX)
		return -EINVAL;

	port = &meson_ports[co->index]->port;
	if (!port || !port->membase)
		return -ENODEV;

	meson_uart_enable_tx_engine(port);

	if (options)
		uart_parse_options(options, &baud, &parity, &bits, &flow);

	return uart_set_options(port, co, baud, parity, bits, flow);
}

static struct console meson_serial_console = {
	.name		= AML_UART_DEV_NAME,
	.write		= meson_serial_console_write,
	.device		= uart_console_device,
	.setup		= meson_serial_console_setup,
	.flags		= CON_PRINTBUFFER,
	.index		= -1,
	.data		= &meson_uart_driver,
};

static int __init meson_serial_console_init(void)
{
	register_console(&meson_serial_console);
	return 0;
}
console_initcall(meson_serial_console_init);

static void meson_serial_early_console_write(struct console *co,
					     const char *s,
					     u_int count)
{
	struct earlycon_device *dev = co->data;

	meson_serial_port_write(&dev->port, s, count);
}

static int __init
meson_serial_early_console_setup(struct earlycon_device *device,
				 const char *opt)
{
	if (!device->port.membase)
		return -ENODEV;

	meson_uart_enable_tx_engine(&device->port);
	device->con->write = meson_serial_early_console_write;
	return 0;
}

OF_EARLYCON_DECLARE(meson, "amlogic,meson-uart",
		    meson_serial_early_console_setup);
OF_EARLYCON_DECLARE(aml_uart, "amlogic,meson-uart",
		    meson_serial_early_console_setup);
OF_EARLYCON_DECLARE_COMP(aml-uart, "amlogic,meson-uart",
			 meson_serial_early_console_setup);

#define MESON_SERIAL_CONSOLE	(&meson_serial_console)
#else
#define MESON_SERIAL_CONSOLE	NULL
#endif

static struct uart_driver meson_uart_driver = {
	.owner		= THIS_MODULE,
	.driver_name	= "meson_uart",
	.dev_name	= AML_UART_DEV_NAME,
	.nr		= AML_UART_PORT_MAX,
	.cons		= MESON_SERIAL_CONSOLE,
};

#ifdef CONFIG_HIBERNATION
static u32 save_mode;

static int meson_uart_freeze(struct device *dev)
{
	struct platform_device *pdev;
	struct uart_port *port;

	pdev = to_platform_device(dev);
	port = platform_get_drvdata(pdev);

	save_mode = readl_relaxed(port->membase + AML_UART_CONTROL);

	pr_debug("uart freeze, mode: %x\n", save_mode);

	return 0;
}

static int meson_uart_thaw(struct device *dev)
{
	return 0;
}

static int meson_uart_restore(struct device *dev)
{
	struct platform_device *pdev;
	struct uart_port *port;

	pdev = to_platform_device(dev);
	port = platform_get_drvdata(pdev);

	writel_relaxed(save_mode, port->membase + AML_UART_CONTROL);
	pr_debug("uart restore, mode: %x\n", save_mode);
	return 0;
}

static int meson_uart_suspend(struct platform_device *pdev,
			      pm_message_t state);
static int meson_uart_resume(struct platform_device *pdev);
static int meson_uart_pm_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);

	return meson_uart_suspend(pdev, PMSG_SUSPEND);
}

static int meson_uart_pm_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);

	return meson_uart_resume(pdev);
}

const struct dev_pm_ops meson_uart_pm = {
	.freeze		= meson_uart_freeze,
	.thaw		= meson_uart_thaw,
	.restore	= meson_uart_restore,
	.suspend	= meson_uart_pm_suspend,
	.resume		= meson_uart_pm_resume,
};
#endif

static int meson_uart_probe(struct platform_device *pdev)
{
	struct resource *res_mem, *res_irq;
	struct uart_port *port;
	struct meson_uart_port *mup;
	struct clk *clk;
	const void *prop;
	int ret = 0;

	if (pdev->dev.of_node)
		pdev->id = of_alias_get_id(pdev->dev.of_node, "serial");

	if (pdev->id < 0 || pdev->id >= AML_UART_PORT_MAX)
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

	mup = devm_kzalloc(&pdev->dev,
		sizeof(struct meson_uart_port), GFP_KERNEL);
	if (!mup)
		return -ENOMEM;

	spin_lock_init(&mup->wr_lock);
	port = &mup->port;

	prop = of_get_property(pdev->dev.of_node, "uart_for_bt", NULL);
	if (prop)
		mup->for_bt = of_read_ulong(prop, 1);

	clk = devm_clk_get(&pdev->dev, "clk_gate");
	if (IS_ERR(clk)) {
//		pr_err("%s: clock gate not found\n", dev_name(&pdev->dev));
		/* return PTR_ERR(clk); */
	} else {
		ret = clk_prepare_enable(clk);
		if (ret) {
			pr_err("uart: clock failed to prepare+enable: %d\n",
				ret);
			clk_put(clk);
			/* return ret; */
		}
	}

	clk = devm_clk_get(&pdev->dev, "clk_uart");
	if (IS_ERR(clk)) {
		pr_err("%s: clock source not found\n", dev_name(&pdev->dev));
		/* return PTR_ERR(clk); */
	}
	if (!IS_ERR(clk))
		port->uartclk = clk_get_rate(clk);

	port->fifosize = 64;

	prop = of_get_property(pdev->dev.of_node, "fifosize", NULL);
	if (prop)
		port->fifosize = of_read_ulong(prop, 1);

	if (!xtal_tick_en) {
		prop = of_get_property(pdev->dev.of_node, "xtal_tick_en", NULL);
		if (prop)
			xtal_tick_en = of_read_ulong(prop, 1);
	}

	port->iotype = UPIO_MEM;
	port->mapbase = res_mem->start;
	port->irq = res_irq->start;
	port->flags = UPF_BOOT_AUTOCONF | UPF_IOREMAP | UPF_LOW_LATENCY;
	port->dev = &pdev->dev;
	port->line = pdev->id;
	port->type = PORT_MESON;
	port->x_char = 0;
	port->ops = &meson_uart_ops;

	meson_ports[pdev->id] = mup;
	platform_set_drvdata(pdev, port);
	if (of_get_property(pdev->dev.of_node, "pinctrl-names", NULL)) {
		mup->p = devm_pinctrl_get_select_default(&pdev->dev);
		/* if (!mup->p) */
		/* return -1; */
	}
	ret = uart_add_one_port(&meson_uart_driver, port);
	if (ret)
		meson_ports[pdev->id] = NULL;

	prop = of_get_property(pdev->dev.of_node, "support-sysrq", NULL);
	if (prop)
		support_sysrq = of_read_ulong(prop, 1);

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

static int meson_uart_resume(struct platform_device *pdev)
{
	struct uart_port *port;
	u32 val;

	port = platform_get_drvdata(pdev);
	if (!port) {
		dev_err(&pdev->dev, "port is NULL");
		return 0;
	}

	if (port->line == 0)
		return 0;
	uart_resume_port(&meson_uart_driver, port);

	val = readl_relaxed(port->membase + AML_UART_CONTROL);
	if (!(val & AML_UART_TWO_WIRE_EN)) {
		val &= ~(0x1 << 31);
		writel_relaxed(val, port->membase + AML_UART_CONTROL);
	}

	return 0;
}

static int meson_uart_suspend(struct platform_device *pdev,
			      pm_message_t state)
{
	struct uart_port *port;
	u32 val;

	port = platform_get_drvdata(pdev);
	if (!port) {
		dev_err(&pdev->dev, "port is NULL");
		return 0;
	}

	if (port->line == 0)
		return 0;
	uart_suspend_port(&meson_uart_driver, port);

	val = readl_relaxed(port->membase + AML_UART_CONTROL);
	/* if rts/cts is open, pull up rts pin
	 * when in suspend
	 */
	if (!(val & AML_UART_TWO_WIRE_EN)) {
		dev_info(&pdev->dev, "pull up rts");
		val |= (0x1 << 31);
		writel_relaxed(val, port->membase + AML_UART_CONTROL);
	}

	return 0;
}

static const struct of_device_id meson_uart_dt_match[] = {
	{ .compatible = "amlogic,meson-uart" },
	{ .compatible = "amlogic, meson-uart" },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, meson_uart_dt_match);

static  struct platform_driver meson_uart_platform_driver = {
	.probe		= meson_uart_probe,
	.remove		= meson_uart_remove,
	.suspend	= meson_uart_suspend,
	.resume		= meson_uart_resume,
	.driver = {
		   .owner = THIS_MODULE,
		   .name = "meson_uart",
		   .of_match_table = meson_uart_dt_match,
#ifdef CONFIG_HIBERNATION
		.pm = &meson_uart_pm,
#endif
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
