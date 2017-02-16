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
#include <linux/dma-mapping.h>
#include <linux/dmaengine.h>
#include <linux/of_irq.h>
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
	unsigned int dma_tx_threshold;
	unsigned int dma_rx_threshold;
	unsigned int dma_num_per_read_burst;
	unsigned int dma_num_per_write_burst;
	int irq;
	struct completion completion;
#define FLAG_DMA_EN 0
#define FLAG_TEST_DATA_AUTO_INC 1
	unsigned int flags;
	u8 test_data;
};


/* Note this is chip_select enable or disable */
void spicc_chip_select(struct spi_device *spi, bool select)
{
	struct spicc *spicc;
	unsigned int level;

	spicc = spi_master_get_devdata(spi->master);
	level = (spi->mode & SPI_CS_HIGH) ? select : !select;

	if (spi->mode & SPI_NO_CS)
		return;
	if (spi->cs_gpio >= 0) {
		gpio_direction_output(spi->cs_gpio, level);
	} else {
		setb(spicc->regs, CON_CHIP_SELECT, spi->chip_select);
		setb(spicc->regs, CON_SS_POL, !!(spi->mode & SPI_CS_HIGH));
	}
}
EXPORT_SYMBOL(spicc_chip_select);

static inline bool spicc_get_flag(struct spicc *spicc, unsigned int flag)
{
	bool ret;
	ret = (spicc->flags >> flag) & 1;
	return ret;
}

static inline void spicc_set_flag(
		struct spicc *spicc,
		unsigned int flag,
		bool value)
{
	if (value)
		spicc->flags |= 1<<flag;
	else
		spicc->flags &= ~(1<<flag);
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

static int spicc_wait_complete(struct spicc *spicc, int max_us)
{
	void __iomem *mem_base = spicc->regs;
	int i, err = -ETIMEDOUT;

	if (spicc->irq) {
		wait_for_completion_interruptible_timeout(
			&spicc->completion, msecs_to_jiffies(10));
		err = 0;
	}
	for (i = 0; i < max_us; i++) {
		if (getb(mem_base, STA_XFER_COM)) {
			err = 0;
			break;
		}
		udelay(1);
	}
	setb(mem_base, STA_XFER_COM, 1);
	if (err)
		dev_err(spicc->master->dev.parent, "timeout\n");
	return err;
}

static irqreturn_t spicc_xfer_complete_isr(int irq, void *dev_id)
{
	struct spicc *spicc = (struct spicc *)dev_id;
	complete(&spicc->completion);
	return IRQ_HANDLED;
}

#define INVALID_DMA_ADDRESS 0xffffffff
/*
 * For DMA, tx_buf/tx_dma have the same relationship as rx_buf/rx_dma:
 *  - The buffer is either valid for CPU access, else NULL
 *  - If the buffer is valid, so is its DMA address
 *
 * This driver manages the dma address unless message->is_dma_mapped.
 */
static int spicc_dma_map(struct spicc *spicc, struct spi_transfer *t)
{
	struct device *dev = spicc->master->dev.parent;
	u8 buf[8], *p = (u8 *)t->tx_buf;
	int i, len = 0;

	t->tx_dma = t->rx_dma = INVALID_DMA_ADDRESS;
	if (t->tx_buf) {
		while (len < t->len) {
			memcpy(buf, p, 8);
			for (i = 0; i < 8; i++)
				*p++ = buf[7-i];
			len += 8;
		}
		t->tx_dma = dma_map_single(dev,
				(void *)t->tx_buf, t->len, DMA_TO_DEVICE);
		if (dma_mapping_error(dev, t->tx_dma)) {
			dev_err(dev, "tx_dma map failed\n");
			return -ENOMEM;
		}
	}
	if (t->rx_buf) {
		t->rx_dma = dma_map_single(dev,
				t->rx_buf, t->len, DMA_FROM_DEVICE);
		if (dma_mapping_error(dev, t->rx_dma)) {
			if (t->tx_buf) {
				dev_err(dev, "rx_dma map failed\n");
				dma_unmap_single(dev, t->tx_dma, t->len,
						DMA_TO_DEVICE);
			}
			return -ENOMEM;
		}
	}
	return 0;
}

static void spicc_dma_unmap(struct spicc *spicc, struct spi_transfer *t)
{
	struct device *dev = spicc->master->dev.parent;
	u8 buf[8], *p = (u8 *)t->rx_buf;
	int i, len = 0;

	if (t->tx_dma != INVALID_DMA_ADDRESS)
		dma_unmap_single(dev, t->tx_dma, t->len, DMA_TO_DEVICE);
	if (t->rx_dma != INVALID_DMA_ADDRESS) {
		dma_unmap_single(dev, t->rx_dma, t->len, DMA_FROM_DEVICE);
		while (len < t->len) {
			memcpy(buf, p, 8);
			for (i = 0; i < 8; i++)
				*p++ = buf[7-i];
			len += 8;
		}
	}
}


static int spicc_dma_xfer(struct spicc *spicc, struct spi_transfer *t)
{
	void __iomem *mem_base = spicc->regs;
	int burst_len, remain;

	setb(mem_base, RX_FIFO_RESET, 1);
	setb(mem_base, TX_FIFO_RESET, 1);
	setb(mem_base, CON_SMC, 0);
	setb(mem_base, CON_XCH, 0);
	spicc_set_bit_width(spicc, 64);
	if (t->tx_dma != INVALID_DMA_ADDRESS)
		writel(t->tx_dma, mem_base + SPICC_REG_DRADDR);
	if (t->rx_dma != INVALID_DMA_ADDRESS)
		writel(t->rx_dma, mem_base + SPICC_REG_DWADDR);
	setb(mem_base, DMA_EN, 1);
	if (spicc->irq) {
		setb(mem_base, INT_XFER_COM_EN, 1);
		enable_irq(spicc->irq);
	}
	setb(mem_base, STA_XFER_COM, 1);

	remain = ((t->len - 1) >> 3) + 1;
	while (remain > 0) {
		burst_len = min_t(size_t, remain, BURST_LEN_MAX);
		setb(mem_base, CON_BURST_LEN, burst_len - 1);
		setb(mem_base, CON_XCH, 1);
		spicc_wait_complete(spicc, burst_len<<4);
		remain -= burst_len;
	}

	setb(mem_base, DMA_EN, 0);
	if (spicc->irq) {
		disable_irq_nosync(spicc->irq);
		setb(mem_base, INT_XFER_COM_EN, 0);
	}
	return 0;
}

static int spicc_hw_xfer(struct spicc *spicc, u8 *txp, u8 *rxp, int len)
{
	void __iomem *mem_base = spicc->regs;
	int burst_len, remain, bytes_per_word;
	int i, j;
	unsigned int dat;

	setb(mem_base, RX_FIFO_RESET, 1);
	setb(mem_base, TX_FIFO_RESET, 1);
	setb(mem_base, CON_SMC, 0);
	setb(mem_base, CON_XCH, 0);
	if (spicc->irq) {
		setb(mem_base, INT_XFER_COM_EN, 1);
		enable_irq(spicc->irq);
	}
	setb(mem_base, STA_XFER_COM, 1);

	bytes_per_word = ((spicc->bits_per_word - 1)>>3) + 1;
	remain = len / bytes_per_word;
	while (remain > 0) {
		burst_len = min_t(size_t, remain, SPICC_FIFO_SIZE);
		for (i = 0; i < burst_len; i++) {
			dat = 0;
			if (txp) {
				for (j = 0; j < bytes_per_word; j++) {
					dat <<= 8;
					dat += *txp++;
				}
			}
			spicc_set_txfifo(spicc, dat);
		}
		setb(mem_base, CON_BURST_LEN, burst_len - 1);
		setb(mem_base, CON_XCH, 1);
		spicc_wait_complete(spicc, (burst_len*bytes_per_word)<<1);
		setb(mem_base, STA_XFER_COM, 1);

		for (i = 0; i < burst_len; i++) {
			dat = spicc_get_rxfifo(spicc);
			if (rxp) {
				for (j = 0; j < bytes_per_word; j++) {
					*rxp++ = dat & 0xff;
					dat >>= 8;
				}
			}
		}
		remain -= burst_len;
	}
	if (spicc->irq) {
		disable_irq_nosync(spicc->irq);
		setb(mem_base, INT_XFER_COM_EN, 0);
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
	setb(mem_base, CON_SMC, 1);
	setb(mem_base, CON_SS_CTL, 1);
	spicc_enable(spicc, 1);
	writel((0x00<<26 |
					0x00<<20 |
					0x0<<19 |
					spicc->dma_num_per_write_burst<<15 |
					spicc->dma_num_per_read_burst<<11 |
					spicc->dma_rx_threshold<<6 |
					spicc->dma_tx_threshold<<1 |
					0x0<<0),
					mem_base + SPICC_REG_DMA);
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
	spicc_chip_select(spi, 0);

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

	/* spicc_enable(spicc, 1); */
	if (spi) {
		spicc_set_bit_width(spicc,
				spi->bits_per_word ? : SPICC_DEFAULT_BIT_WIDTH);
		spicc_set_clk(spicc,
				spi->max_speed_hz ? : SPICC_DEFAULT_SPEED_HZ);
		spicc_set_mode(spicc, spi->mode);
		spicc_chip_select(spi, 1);
	}
	list_for_each_entry(t, &m->transfers, transfer_list) {
		if (t->speed_hz)
			spicc_set_clk(spicc, t->speed_hz);
		if (spicc_get_flag(spicc, FLAG_DMA_EN)) {
			if (!m->is_dma_mapped)
				spicc_dma_map(spicc, t);
			spicc_dma_xfer(spicc, t);
			if (!m->is_dma_mapped)
				spicc_dma_unmap(spicc, t);
		} else if (spicc_hw_xfer(spicc, (u8 *)t->tx_buf,
				(u8 *)t->rx_buf, t->len) < 0)
			goto spicc_handle_end;
		m->actual_length += t->len;
		if (t->delay_usecs)
			udelay(t->delay_usecs);
	}

spicc_handle_end:
	if (spi)
		spicc_chip_select(spi, 0);
	/* spicc_enable(spicc, 0); */

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

static ssize_t show_setting(struct class *class,
	struct class_attribute *attr, char *buf)
{
	int ret = 0;
	struct spicc *spicc = container_of(class, struct spicc, cls);

	if (!strcmp(attr->attr.name, "speed"))
		ret = sprintf(buf, "speed=%d\n", spicc->speed);
	else if (!strcmp(attr->attr.name, "mode"))
		ret = sprintf(buf, "mode=%d\n", spicc->mode);
	else if (!strcmp(attr->attr.name, "bit_width"))
		ret = sprintf(buf, "bit_width=%d\n", spicc->bits_per_word);
	else if (!strcmp(attr->attr.name, "flags"))
		ret = sprintf(buf, "flags=0x%x\n", spicc->flags);
	else if (!strcmp(attr->attr.name, "test_data"))
		ret = sprintf(buf, "test_data=0x%x\n", spicc->test_data);
	return ret;
}


static ssize_t store_setting(
		struct class *class,
		struct class_attribute *attr,
		const char *buf, size_t count)
{
	unsigned int value;
	struct spicc *spicc = container_of(class, struct spicc, cls);

	if (sscanf(buf, "%d", &value) != 1)
		return -EINVAL;
	if (!strcmp(attr->attr.name, "speed"))
		spicc_set_clk(spicc, value);
	else if (!strcmp(attr->attr.name, "mode"))
		spicc_set_mode(spicc, value);
	else if (!strcmp(attr->attr.name, "bit_width"))
		spicc_set_bit_width(spicc, value);
	else if (!strcmp(attr->attr.name, "flags"))
		spicc->flags = value;
	else if (!strcmp(attr->attr.name, "test_data"))
		spicc->test_data = (u8)(value & 0xff);
	return count;
}

#define TRANSFER_BYTES_MAX 128
#define ARGS_MAX (TRANSFER_BYTES_MAX+2)
static ssize_t store_test(
		struct class *class,
		struct class_attribute *attr,
		const char *buf, size_t count)
{
	struct spicc *spicc = container_of(class, struct spicc, cls);
	unsigned int chip_select, num;
	u8 *tx_buf = 0, *rx_buf = 0;
	unsigned long value;
	struct device *dev = spicc->master->dev.parent;
	char *kbuf, *p, *argv[ARGS_MAX];
	int argn;
	int cs_gpio = -1;
	int i;
	struct spi_transfer t;
	struct spi_message m;

	memset(argv, 0, sizeof(argv));
	kbuf = kstrdup(buf, GFP_KERNEL);
	p = kbuf;
	for (argn = 0; argn < ARGS_MAX; argn++) {
		argv[argn] = strsep(&p, " ");
		if (argv[argn] == NULL)
			break;
	}

	if (buf[0] == 'h') {
		dev_info(dev, "SPI device test help\n");
		dev_info(dev, "echo chip_select num [wbyte1 wbyte2...] >test\n");
		return count;
	}

	if ((argv[0] == NULL) || (argv[1] == NULL) ||
		 (kstrtoul(argv[0], 0, (long *)&chip_select)) ||
		 (kstrtoul(argv[1], 0, (long *)&num))) {
		dev_info(dev, "invalid data\n");
		return -EINVAL;
	}
	tx_buf = kzalloc(num, GFP_KERNEL | GFP_DMA);
	rx_buf = kzalloc(num, GFP_KERNEL | GFP_DMA);
	if (IS_ERR(tx_buf) || IS_ERR(rx_buf)) {
		dev_err(dev, "failed to alloc tx rx buffer\n");
		return -ENOMEM;
	}

	spi_message_init(&m);
	memset(&t, 0, sizeof(t));
	t.tx_buf = (void *)tx_buf;
	t.rx_buf = (void *)rx_buf;
	t.len = num;
	spi_message_add_tail(&t, &m);

	for (i = 0; i < num; i++) {
		if (!argv[i+2] || (kstrtoul(argv[i+2], 16, &value))) {
			tx_buf[i] = spicc->test_data;
			if (spicc_get_flag(spicc, FLAG_TEST_DATA_AUTO_INC))
				spicc->test_data++;
		} else {
			tx_buf[i] = (u8)(value & 0xff);
		}
		dev_info(dev, "tx_data[%d]=0x%x\n", i, tx_buf[i]);
	}

	dev_info(dev, "\nchip_select=%d, num=%d\n", chip_select, num);
	if (chip_select >= spicc->master->num_chipselect) {
		dev_info(dev, "error chip select\n");
		return -EINVAL;
	}
	if (spicc->master->cs_gpios)
		cs_gpio = spicc->master->cs_gpios[chip_select];
	if (cs_gpio >= 0) {
		dev_info(dev, "set gpio %d to 0\n", cs_gpio);
		gpio_request(cs_gpio, "spicc_cs");
		gpio_direction_output(cs_gpio, !!(spicc->mode & SPI_CS_HIGH));
	} else {
		dev_info(dev, "use spicc ss\n");
		setb(spicc->regs, CON_CHIP_SELECT, chip_select);
		setb(spicc->regs, CON_SS_POL, !!(spicc->mode & SPI_CS_HIGH));
	}
	spicc_handle_one_msg(spicc, &m);
	if (cs_gpio >= 0) {
		gpio_direction_output(cs_gpio, !(spicc->mode & SPI_CS_HIGH));
		gpio_free(cs_gpio);
	}
	dev_info(dev, "read back data ok\n");
	for (i = 0; i < min_t(size_t, 32, num); i++)
		dev_info(dev, "rx_data[%d]=0x%x\n", i, rx_buf[i]);

	kfree(tx_buf);
	kfree(rx_buf);
	kfree(kbuf);
	return count;
}

static struct class_attribute spicc_class_attrs[] = {
		__ATTR(test, S_IWUSR, NULL, store_test),
		__ATTR(test_data, S_IRUGO|S_IWUSR, show_setting, store_setting),
		__ATTR(speed, S_IRUGO|S_IWUSR, show_setting, store_setting),
		__ATTR(mode, S_IRUGO|S_IWUSR, show_setting, store_setting),
		__ATTR(bit_width, S_IRUGO|S_IWUSR, show_setting, store_setting),
		__ATTR(flags, S_IRUGO|S_IWUSR, show_setting, store_setting),
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
	unsigned int value;

	err = of_property_read_u32(np, "device_id", &spicc->device_id);
	if (err) {
		dev_err(&pdev->dev, "get device_id fail\n");
		return err;
	}

	err = of_property_read_u32(np, "dma_en", &value);
	spicc_set_flag(spicc, FLAG_DMA_EN, err ? 0 : (!!value));
	dev_info(&pdev->dev, "dma_en=%d\n", spicc_get_flag(spicc, FLAG_DMA_EN));
	err = of_property_read_u32(np, "dma_tx_threshold", &value);
	spicc->dma_tx_threshold = err ? 3 : value;
	err = of_property_read_u32(np, "dma_rx_threshold", &value);
	spicc->dma_rx_threshold = err ? 3 : value;
	err = of_property_read_u32(np, "dma_num_per_read_burst", &value);
	spicc->dma_num_per_read_burst = err ? 3 : value;
	err = of_property_read_u32(np, "dma_num_per_write_burst", &value);
	spicc->dma_num_per_write_burst = err ? 3 : value;
	spicc->irq = irq_of_parse_and_map(np, 0);
	dev_info(&pdev->dev, "irq = 0x%x\n", spicc->irq);

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
	init_completion(&spicc->completion);
	if (spicc->irq) {
		if (request_irq(spicc->irq, spicc_xfer_complete_isr,
				IRQF_DISABLED, "spicc", spicc)) {
			dev_err(&pdev->dev, "master %d request irq(%d) failed!\n",
					master->bus_num, spicc->irq);
			spicc->irq = 0;
		} else
			disable_irq_nosync(spicc->irq);
	}

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
