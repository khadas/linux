/*drivers/spi/spi-rockchip-test.c -spi test driver
 *
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>

#define w25q128fw_DEVICE_ID 0x6018
#define w25q128fw_SPI_READ_ID_CMD 0x9F
static int w25q128fw_id=0;//0:NG,1:OK
int usb2_id=0;//0:NG,1:OK
int usb3_id=0;//0:NG,1:OK
int fusb302_1=0;//0:NG,1:OK
int fusb302_2=0;//0:NG,1:OK
int charge_id=0;//0:NG,1:OK
int key_test_flag=0;//0:NG,1:OK

static ssize_t show_w25q128fw_id(struct class *cls,
		        struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", w25q128fw_id);
}

static ssize_t show_usb2(struct class *cls,
				struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", usb2_id);
}

static ssize_t show_usb3(struct class *cls,
				struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", usb3_id);
}

static ssize_t show_fusb302(struct class *cls,
				struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", fusb302_2<<1|fusb302_1);
}

static ssize_t show_charge(struct class *cls,
				struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", charge_id);
}

static ssize_t show_key(struct class *cls,
				struct class_attribute *attr, char *buf)
{
	key_test_flag=1;
	return sprintf(buf, "%d\n", key_test_flag);
}
static struct class_attribute w25q128fw_attrs[] = {
	__ATTR(id, 0644, show_w25q128fw_id, NULL),
	__ATTR(usb2, 0644, show_usb2, NULL),
	__ATTR(usb3, 0644, show_usb3, NULL),
	__ATTR(fusb302, 0644, show_fusb302, NULL),
	__ATTR(charge, 0644, show_charge, NULL),
	__ATTR(key, 0644, show_key, NULL),
};

static void create_w25q128fw_attrs(void)
{
	int i;
	struct class *w25q128fw_class;
	printk("%s\n",__func__);
	w25q128fw_class = class_create(THIS_MODULE, "w25q128fw");
	if (IS_ERR(w25q128fw_class)) {
		pr_err("create w25q128fw_class debug class fail\n");
		return;
	}
	for (i = 0; i < ARRAY_SIZE(w25q128fw_attrs); i++) {
		if (class_create_file(w25q128fw_class, &w25q128fw_attrs[i]))
			pr_err("create w25q128fw attribute %s fail\n", w25q128fw_attrs[i].attr.name);
	}
}

static int w25q128fw_spi_read_id(struct spi_device *spi)
{
	int	status;
	char tbuf[] = {w25q128fw_SPI_READ_ID_CMD};
	char rbuf[3];
	status = spi_write_then_read(spi, tbuf, sizeof(tbuf), rbuf, sizeof(rbuf));
	if (status == 0){ 
		printk("w25q128fw: ID = %02x %02x %02x\n",rbuf[0], rbuf[1], rbuf[2]); 
		if(w25q128fw_DEVICE_ID==(rbuf[1]<<8|rbuf[2])){
			printk("w25q128fw is ok\n"); 
			w25q128fw_id=1;
		}
		else
			w25q128fw_id=0;
	}
	else {
		printk("%s: read ID error\n", __FUNCTION__); 
		w25q128fw_id=0;
	}
	return status;
}

static int w25q128fw_spi_probe(struct spi_device *spi)
{
    int ret = 0;
    struct device_node __maybe_unused *np = spi->dev.of_node;
	
    printk("w25q128fw_spi_probe\n");
	
	if(!spi)	
		return -ENOMEM;
	printk("w25q128fw_spi_probe: setup mode %d, %s%s%s%s%u bits/w, %u Hz max\n",
			(int) (spi->mode & (SPI_CPOL | SPI_CPHA)),
			(spi->mode & SPI_CS_HIGH) ? "cs_high, " : "",
			(spi->mode & SPI_LSB_FIRST) ? "lsb, " : "",
			(spi->mode & SPI_3WIRE) ? "3wire, " : "",
			(spi->mode & SPI_LOOP) ? "loopback, " : "",
			spi->bits_per_word, spi->max_speed_hz);
	w25q128fw_spi_read_id(spi);	
	create_w25q128fw_attrs();
    return ret;
}

static struct of_device_id w25q128fw_match_table[] = {
	{ .compatible = "khadas,rk3399-spi",},
	{},
};
	
static struct spi_driver w25q128fw_spi_driver = {
	.driver = {
		.name = "w25q128fw-spi",
		.owner = THIS_MODULE,
		.of_match_table = w25q128fw_match_table,
	},
	.probe = w25q128fw_spi_probe,
};
		
static int w25q128fw_spi_init(void)
{
	return spi_register_driver(&w25q128fw_spi_driver);
}
module_init(w25q128fw_spi_init);

static void w25q128fw_spi_exit(void)
{
	spi_unregister_driver(&w25q128fw_spi_driver);
}
module_exit(w25q128fw_spi_exit);

MODULE_AUTHOR("goenjoy <goenjoy@khadas.com>");
MODULE_DESCRIPTION("khadas SPI TEST Driver");
MODULE_LICENSE("GPL");
