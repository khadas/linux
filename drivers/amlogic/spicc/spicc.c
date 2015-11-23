#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/spi/spi.h>
#include <linux/of_address.h>
#include <linux/amlogic/aml_gpio_consumer.h>
#include <linux/of.h>
#include <linux/reset.h>
#include "spicc.h"

/**
 * struct spicc
 * @lock: spinlock for SPICC controller.
 * @msg_queue: link with the spi message list.
 * @wq: work queque
 * @work: work
 * @master: spi master alloc for this driver.
 * @spi: spi device on working.
 * @regs: the start register address of this SPICC controller.
 */
struct spicc {
	spinlock_t lock;
	struct list_head msg_queue;
	struct workqueue_struct *wq;
	struct work_struct work;
	struct spi_master *master;
	struct spi_device *spi;
	struct class cls;

	int device_id;
	struct clk *clk;
	void __iomem *regs;
	struct pinctrl *pinctrl;
	struct pinctrl_state *pullup;
	struct pinctrl_state *pulldown;
	int bits_per_word;
	int mode;
	int speed;
};


static void spicc_chip_select(struct spi_device *spi, bool select)
{
	if (spi->mode & SPI_NO_CS)
		return;
	if (!(spi->mode & SPI_CS_HIGH))
		select = !select;
	if (spi->cs_gpio)
		gpio_direction_output(spi->cs_gpio, select);
}

static inline void spicc_set_bit_width(struct spicc *spicc, u8 bw)
{
	setb(spicc->regs, CON_BITS_PER_WORD, bw-1);
	spicc->bits_per_word = bw;
}

static void spicc_set_mode(struct spicc *spicc, u8 mode)
{
	bool cpol = (mode & SPI_CPOL) ? 1:0;
	bool cpha = (mode & SPI_CPHA) ? 1:0;

	if (mode == spicc->mode)
		return;

	spicc->mode = mode;
	if (cpol)
		pinctrl_select_state(spicc->pinctrl, spicc->pullup);
	else
		pinctrl_select_state(spicc->pinctrl, spicc->pulldown);

	setb(spicc->regs, CON_CLK_PHA, cpha);
	setb(spicc->regs, CON_CLK_POL, cpol);
	setb(spicc->regs, CON_DRCTL, 0);
}

static void spicc_set_clk(struct spicc *spicc, int speed)
{
	unsigned sys_clk_rate;
	unsigned div, mid_speed;

	if (!speed) {
		clk_disable(spicc->clk);
		return;
	}
	if (speed == spicc->speed)
		return;

	spicc->speed = speed;
	clk_prepare_enable(spicc->clk);
	sys_clk_rate = clk_get_rate(spicc->clk);

	/* actually, speed = sys_clk_rate / 2^(conreg.data_rate_div+2) */
	mid_speed = (sys_clk_rate * 3) >> 4;
	for (div = 0; div < 7; div++) {
		if (speed >= mid_speed)
			break;
		mid_speed >>= 1;
	}
	setb(spicc->regs, CON_DATA_RATE_DIV, div);
}

static inline void spicc_set_txfifo(struct spicc *spicc, u32 dat)
{
	writel(dat, spicc->regs + SPICC_REG_TXDATA);
}

static inline u32 spicc_get_rxfifo(struct spicc *spicc)
{
	return readl(spicc->regs + SPICC_REG_RXDATA);
}

static inline void spicc_enable(struct spicc *spicc, bool en)
{
	setb(spicc->regs, CON_ENABLE, en);
}

static int spicc_wait_complete(struct spicc *spicc, int max)
{
	void __iomem *mem_base = spicc->regs;
	int i;

	for (i = 0; i < max; i++) {
		if (getb(mem_base, STA_RX_READY))
			return 0;
	}
	dev_err(spicc->master->dev.parent, "timeout\n");
	return -1;
}

static int spicc_hw_xfer(struct spicc *spicc, u8 *txp, u8 *rxp, int len)
{
	int num, i, j, bytes;
	unsigned int dat;

	bytes = ((spicc->bits_per_word - 1)>>3) + 1;
	len /= bytes;

	while (len > 0) {
		num = (len > SPICC_FIFO_SIZE) ? SPICC_FIFO_SIZE : len;
		for (i = 0; i < num; i++) {
			dat = 0;
			if (txp) {
				for (j = 0; j < bytes; j++) {
					dat <<= 8;
					dat += *txp++;
				}
			}
			spicc_set_txfifo(spicc, dat);
			/* printk("txdata[%d] = 0x%x\n", i, dat); */
		}

		for (i = 0; i < num; i++) {
			if (spicc_wait_complete(spicc, 1000))
				return -ETIMEDOUT;
			dat = spicc_get_rxfifo(spicc);
			/* printk("rxdata[%d] = 0x%x\n", i, dat); */
			if (rxp) {
				for (j = 0; j < bytes; j++) {
					*rxp++ = dat & 0xff;
					dat >>= 8;
				}
			}
		}
		len -= num;
	}
	return 0;
}

static void spicc_hw_init(struct spicc *spicc)
{
	void __iomem *mem_base = spicc->regs;

	spicc->bits_per_word = -1;
	spicc->mode = -1;
	spicc->speed = -1;
	spicc_enable(spicc, 0);
	setb(mem_base, CLK_FREE_EN, 1);
	setb(mem_base, CON_MODE, 1); /* 0-slave, 1-master */
	setb(mem_base, CON_XCH, 0);
	setb(mem_base, CON_SMC, 1); /* 0-dma, 1-pio */
	setb(mem_base, CON_SS_CTL, 1);
	spicc_set_bit_width(spicc, 8);
	spicc_set_mode(spicc, SPI_MODE_0);
	spicc_set_clk(spicc, 3000000);
}


int dirspi_xfer(struct spi_device *spi, u8 *tx_buf, u8 *rx_buf, int len)
{
	struct spicc *spicc;

	spicc = spi_master_get_devdata(spi->master);
	return spicc_hw_xfer(spicc, tx_buf, rx_buf, len);
}
EXPORT_SYMBOL(dirspi_xfer);

int dirspi_write(struct spi_device *spi, u8 *buf, int len)
{
	struct spicc *spicc;

	spicc = spi_master_get_devdata(spi->master);
	return spicc_hw_xfer(spicc, buf, 0, len);
}
EXPORT_SYMBOL(dirspi_write);

int dirspi_read(struct spi_device *spi, u8 *buf, int len)
{
	struct spicc *spicc;

	spicc = spi_master_get_devdata(spi->master);
	return spicc_hw_xfer(spicc, 0, buf, len);
}
EXPORT_SYMBOL(dirspi_read);

void dirspi_start(struct spi_device *spi)
{
	spicc_chip_select(spi, 1);
}
EXPORT_SYMBOL(dirspi_start);

void dirspi_stop(struct spi_device *spi)
{
	spicc_chip_select(spi, 0);
}
EXPORT_SYMBOL(dirspi_stop);


/* setting clock and pinmux here */
static int spicc_setup(struct spi_device *spi)
{
	struct spicc *spicc;

	spicc = spi_master_get_devdata(spi->master);
	if (!spi->cs_gpio &&
	(spi->chip_select < spi->master->num_chipselect))
		spi->cs_gpio = spi->master->cs_gpios[spi->chip_select];
	dev_info(&spi->dev, "cs_gpio=%d,mode=%d, speed=%d\n",
			spi->cs_gpio, spi->mode, spi->max_speed_hz);
	if (spi->cs_gpio)
		gpio_request(spi->cs_gpio, "spicc");
	spicc_set_bit_width(spicc, spi->bits_per_word);
	spicc_set_clk(spicc, spi->max_speed_hz);
	spicc_set_mode(spicc, spi->mode);
	spicc_enable(spicc, 1);
	spicc_chip_select(spi, 1);

	return 0;
}

static void spicc_cleanup(struct spi_device *spi)
{
	struct spicc *spicc;

	spicc = spi_master_get_devdata(spi->master);
	spicc_chip_select(spi, 0);
	spicc_enable(spicc, 0);
	spicc_set_clk(spicc, 0);
	if (spi->cs_gpio)
		gpio_free(spi->cs_gpio);
}

static void spicc_handle_one_msg(struct spicc *spicc, struct spi_message *m)
{
	struct spi_device *spi = m->spi;
	struct spi_transfer *t;
	int ret = 0;

	spicc_set_bit_width(spicc, spi->bits_per_word);
	spicc_set_clk(spicc, spi->max_speed_hz);
	spicc_set_mode(spicc, spi->mode);
	spicc_enable(spicc, 1);
	spicc_chip_select(spi, 1);
	list_for_each_entry(t, &m->transfers, transfer_list) {
		if (t->speed_hz)
			spicc_set_clk(spicc, t->speed_hz);
		if (spicc_hw_xfer(spicc, (u8 *)t->tx_buf,
				(u8 *)t->rx_buf, t->len) < 0)
			goto spicc_handle_end;
		m->actual_length += t->len;
		if (t->delay_usecs)
			udelay(t->delay_usecs);
	}

spicc_handle_end:
	spicc_chip_select(spi, 0);
	spicc_enable(spicc, 0);
	spicc_set_clk(spicc, 0);

	m->status = ret;
	if (m->context)
		m->complete(m->context);
}

static int spicc_transfer(struct spi_device *spi, struct spi_message *m)
{
	struct spicc *spicc = spi_master_get_devdata(spi->master);
	unsigned long flags;

	m->actual_length = 0;
	m->status = -EINPROGRESS;

	spin_lock_irqsave(&spicc->lock, flags);
	list_add_tail(&m->queue, &spicc->msg_queue);
	queue_work(spicc->wq, &spicc->work);
	spin_unlock_irqrestore(&spicc->lock, flags);

	return 0;
}

static void spicc_work(struct work_struct *work)
{
	struct spicc *spicc = container_of(work, struct spicc, work);
	struct spi_message *m;
	unsigned long flags;

	spin_lock_irqsave(&spicc->lock, flags);
	while (!list_empty(&spicc->msg_queue)) {
		m = container_of(spicc->msg_queue.next,
				struct spi_message, queue);
		list_del_init(&m->queue);
		spin_unlock_irqrestore(&spicc->lock, flags);
		spicc_handle_one_msg(spicc, m);
		spin_lock_irqsave(&spicc->lock, flags);
	}
	spin_unlock_irqrestore(&spicc->lock, flags);
}


static ssize_t store_test(
		struct class *class,
		struct class_attribute *attr,
		const char *buf, size_t count)
{
	struct spicc *spicc = container_of(class, struct spicc, cls);
	unsigned int i, cs_gpio, speed, mode, num;
	u8 wbuf[4] = {0}, rbuf[128] = {0};
	unsigned long flags;
	struct device *dev = spicc->master->dev.parent;

	if (buf[0] == 'h') {
		dev_info(dev, "SPI device test help\n");
		dev_info(dev, "You can test the SPI device even without its driver through this sysfs node\n");
		dev_info(dev, "echo cs_gpio speed num [wdata1 wdata2 wdata3 wdata4] >test\n");
		return count;
	}

	i = sscanf(buf, "%d%d%d%d%x%x%x%x", &cs_gpio, &speed, &mode, &num,
			(unsigned int *)&wbuf[0], (unsigned int *)&wbuf[1],
			(unsigned int *)&wbuf[2], (unsigned int *)&wbuf[3]);
	dev_info(dev, "cs_gpio=%d, speed=%d, mode=%d, num=%d\n",
			cs_gpio, speed, mode, num);
	if ((i < (num+4)) || (!cs_gpio) || (!speed) || (num > sizeof(wbuf))) {
		dev_info(dev, "invalid data\n");
		return -EINVAL;
	}

	spin_lock_irqsave(&spicc->lock, flags);
	gpio_request(cs_gpio, "spicc_cs");
	gpio_direction_output(cs_gpio, 0);
	spicc_set_clk(spicc, speed);
	spicc_set_mode(spicc, mode);
	setb(spicc->regs, CON_ENABLE, 1); /* enable SPICC */
/* spicc_dump(spicc); */

	spicc_hw_xfer(spicc, wbuf, rbuf, num);
	dev_info(dev, "read back data: ");
	for (i = 0; i < num; i++)
		dev_info(dev, "0x%x, ", rbuf[i]);
	dev_info(dev, "\n");

	setb(spicc->regs, CON_ENABLE, 0); /* disable SPICC */
	spicc_set_clk(spicc, 0);
	gpio_direction_input(cs_gpio);
	gpio_free(cs_gpio);
	spin_unlock_irqrestore(&spicc->lock, flags);

	return count;
}
static struct class_attribute spicc_class_attrs[] = {
		__ATTR(test,  S_IWUSR, NULL,    store_test),
		__ATTR_NULL
};


static int of_spicc_get_data(
		struct spicc *spicc,
		struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct resource *res;
	struct reset_control *rst;
	int err;

	err = of_property_read_u32(np, "device_id", &spicc->device_id);
	if (err) {
		dev_err(&pdev->dev, "get device_id fail\n");
		return err;
	}

	spicc->pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(spicc->pinctrl)) {
		dev_err(&pdev->dev, "get pinctrl fail\n");
		return PTR_ERR(spicc->pinctrl);
	}
	spicc->pullup = pinctrl_lookup_state(
			spicc->pinctrl, "spicc_pullup");
	if (IS_ERR(spicc->pullup)) {
		dev_err(&pdev->dev, "lookup pullup fail\n");
		return PTR_ERR(spicc->pullup);
	}
	spicc->pulldown = pinctrl_lookup_state(
			spicc->pinctrl, "spicc_pulldown");
	if (IS_ERR(spicc->pulldown)) {
		dev_err(&pdev->dev, "lookup spicc_pulldown fail\n");
		return PTR_ERR(spicc->pulldown);
	}
	pinctrl_select_state(spicc->pinctrl, spicc->pullup);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	spicc->regs = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(spicc->regs)) {
		dev_err(&pdev->dev, "get resource fail\n");
		return PTR_ERR(spicc->regs);
	}

	rst = devm_reset_control_get(&pdev->dev, NULL);
	if (!IS_ERR(rst))
		reset_control_deassert(rst);

	spicc->clk = devm_clk_get(&pdev->dev, "spicc_clk");
	if (IS_ERR(spicc->clk)) {
		dev_err(&pdev->dev, "get clk fail\n");
		return PTR_ERR(spicc->clk);
	}
	return 0;
}

static int spicc_probe(struct platform_device *pdev)
{
	struct spi_master *master;
	struct spicc *spicc;
	int i, ret;

	BUG_ON(!pdev->dev.of_node);
	master = spi_alloc_master(&pdev->dev, sizeof(*spicc));
	if (IS_ERR(master)) {
		dev_err(&pdev->dev, "allocate spi master failed!\n");
		return -ENOMEM;
	}

	spicc = spi_master_get_devdata(master);
	spicc->master = master;
	dev_set_drvdata(&pdev->dev, spicc);
	ret = of_spicc_get_data(spicc, pdev);
	if (ret) {
		dev_err(&pdev->dev, "of error=%d\n", ret);
		goto err;
	}
	master->dev.of_node = pdev->dev.of_node;
	master->bus_num = spicc->device_id;
	master->setup = spicc_setup;
	master->transfer = spicc_transfer;
	master->cleanup = spicc_cleanup;
	master->mode_bits = SPI_MODE_3;
	ret = spi_register_master(master);
	if (ret < 0) {
		dev_err(&pdev->dev, "register spi master failed! (%d)\n", ret);
		goto err;
	}

	INIT_WORK(&spicc->work, spicc_work);
	spicc->wq = create_singlethread_workqueue(dev_name(master->dev.parent));
	if (spicc->wq == NULL) {
		ret = -EBUSY;
		goto err;
	}
	spin_lock_init(&spicc->lock);
	INIT_LIST_HEAD(&spicc->msg_queue);

	spicc_hw_init(spicc);

	/*setup class*/
	spicc->cls.name = kzalloc(10, GFP_KERNEL);
	sprintf((char *)spicc->cls.name, "spicc%d", master->bus_num);
	spicc->cls.class_attrs = spicc_class_attrs;
	ret = class_register(&spicc->cls);
	if (ret < 0)
		dev_err(&pdev->dev, "register class failed! (%d)\n", ret);

	dev_info(&pdev->dev, "cs_num=%d\n", master->num_chipselect);
	for (i = 0; i < master->num_chipselect; i++)
		dev_info(&pdev->dev, "cs[%d]=%d\n", i, master->cs_gpios[i]);
	return ret;
err:
	spi_master_put(master);
	return ret;
}

static int spicc_remove(struct platform_device *pdev)
{
	struct spicc *spicc;

	spicc = (struct spicc *)dev_get_drvdata(&pdev->dev);
	spi_unregister_master(spicc->master);
	destroy_workqueue(spicc->wq);
	if (spicc->pinctrl)
		devm_pinctrl_put(spicc->pinctrl);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id spicc_of_match[] = {
	{	.compatible = "amlogic, spicc", },
	{},
};
#else
#define spicc_of_match NULL
#endif

static struct platform_driver spicc_driver = {
	.probe = spicc_probe,
	.remove = spicc_remove,
	.driver = {
			.name = "spicc",
			.of_match_table = spicc_of_match,
			.owner = THIS_MODULE,
		},
};

static int __init spicc_init(void)
{
	return platform_driver_register(&spicc_driver);
}

static void __exit spicc_exit(void)
{
	platform_driver_unregister(&spicc_driver);
}

subsys_initcall(spicc_init);
module_exit(spicc_exit);

MODULE_DESCRIPTION("Amlogic SPICC driver");
MODULE_LICENSE("GPL");
