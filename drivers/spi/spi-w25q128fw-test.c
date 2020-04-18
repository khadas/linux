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

#include <linux/fs.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/gpio.h>
#include <linux/input.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/delay.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <linux/ioctl.h>
#include <linux/soc/rockchip/rk_vendor_storage.h>


#define w25q128fw_DEVICE_ID 0x6018
#define w25q128fw_SPI_READ_ID_CMD 0x9F
static int w25q128fw_id=0;//0:NG,1:OK
int usb2_id=0;//0:NG,1:OK
int usb3_id=0;//0:NG,1:OK
int fusb302_1=0;//0:NG,1:OK
int fusb302_2=0;//0:NG,1:OK
int charge_id=0;//0:NG,1:OK
int key_test_flag=0;//0:NG,1:OK

#define PWM_IO 149
typedef enum {
    PWM_DISABLE = 0,
    PWM_ENABLE,
}PWM_STATUS_t;

//pwmµÄÉè±¸¶ÔÏó
struct pwm_chip{
    unsigned long period; //PWMÖÜÆÚ
    unsigned long duty;   //PWMÕ¼¿Õ±È
    struct hrtimer mytimer;
    ktime_t kt;           //ÉèÖÃ¶¨Ê±Ê±¼ä
    PWM_STATUS_t status;
};
struct pwm_chip *pwm_dev;

int StringToHex(char *str, unsigned char *out, unsigned int *outlen)
{
	char *p = str;
	char high = 0, low = 0;
	int tmplen = strlen(p), cnt = 0;
	tmplen = strlen(p);
	while(cnt < (tmplen / 2))
	{
		high = ((*p > '9') && ((*p <= 'F') || (*p <= 'f'))) ? *p - 48 - 7 : *p - 48;
		low = (*(++ p) > '9' && ((*p <= 'F') || (*p <= 'f'))) ? *(p) - 48 - 7 : *(p) - 48;
		out[cnt] = ((high & 0x0f) << 4 | (low & 0x0f));
		p ++;
		cnt ++;
	}
	if(tmplen % 2 != 0) out[cnt] = ((*p > '9') && ((*p <= 'F') || (*p <= 'f'))) ? *p - 48 - 7 : *p - 48;
	
	if(outlen != NULL) *outlen = tmplen / 2 + tmplen % 2;
	return tmplen / 2 + tmplen % 2;
}

void HexToAscii(unsigned char *pHex, unsigned char *pAscii, int nLen)
{
    unsigned char Nibble[2];
    unsigned int i,j;
    for (i = 0; i < nLen; i++){
        Nibble[0] = (pHex[i] & 0xF0) >> 4;
        Nibble[1] = pHex[i] & 0x0F;
        for (j = 0; j < 2; j++){
            if (Nibble[j] < 10){            
                Nibble[j] += 0x30;
            }
            else{
                if (Nibble[j] < 16)
                    Nibble[j] = Nibble[j] - 10 + 'A';
            }
            *pAscii++ = Nibble[j];
        }
    }
}

static enum hrtimer_restart hrtimer_handler(struct hrtimer *timer)
{    
    if (gpio_get_value(PWM_IO) == 1) {
        if (pwm_dev->duty != pwm_dev->period) {     //Õ¼¿Õ±È100%µÄÇé¿öÏÂÃ»±ØÒªÀ­µÍ
            gpio_set_value(PWM_IO, 0);
            pwm_dev->kt = ktime_set(0, pwm_dev->period-pwm_dev->duty);
        }
        hrtimer_forward_now(&pwm_dev->mytimer, pwm_dev->kt);
    } else {
        if (pwm_dev->duty != 0) {                   //Õ¼¿Õ±È£°%µÄÇé¿öÏÂÃ»±ØÒªÀ­¸ß
            gpio_set_value(PWM_IO, 1);
            pwm_dev->kt = ktime_set(0, pwm_dev->duty);
        }
        hrtimer_forward_now(&pwm_dev->mytimer, pwm_dev->kt);
    }
    return HRTIMER_RESTART;    
}

static void pwm_gpio_start(void)
{    
    //¸ß¾«¶È¶¨Ê±Æ÷
    hrtimer_init(&pwm_dev->mytimer,CLOCK_MONOTONIC,HRTIMER_MODE_REL);
    pwm_dev->mytimer.function = hrtimer_handler;
    pwm_dev->kt = ktime_set(0, pwm_dev->period-pwm_dev->duty);
    hrtimer_start(&pwm_dev->mytimer,pwm_dev->kt,HRTIMER_MODE_REL);
    
}

static int pwm_gpio_init(void)
{
    int ret = -1;

    pwm_dev = kzalloc(sizeof(struct pwm_chip),GFP_KERNEL);
    if(pwm_dev == NULL){
        printk("kzalloc error");
        return -ENOMEM;
    }
    //³õÊ¼»¯Õ¼¿Õ±È
    pwm_dev->period = 500000;//2000HZ
    pwm_dev->duty = 250000;

    if(gpio_request(PWM_IO, "PWM_GPIO")){
        printk("error pwm_gpio_init");
        return ret;

    }else{
        gpio_direction_output(PWM_IO, 1);
        gpio_set_value(PWM_IO, 0);
		pwm_dev->status = PWM_DISABLE;
    }

    return 0;
}

static ssize_t store_buzzer(struct class *cls, struct class_attribute *attr,
		        const char *buf, size_t count)
{
	int enable;

	if (kstrtoint(buf, 0, &enable)){
		printk("store_buzzer error\n");
		return -EINVAL;
	}
	printk("pwm enable=%d\n",enable);
	if(enable && pwm_dev->status==PWM_DISABLE){
		//Æô¶¯¶¨Ê±Æ÷  
		pwm_gpio_start();
		pwm_dev->status = PWM_ENABLE;
	}
	else if(!enable && pwm_dev->status==PWM_ENABLE){
		hrtimer_cancel(&pwm_dev->mytimer);
        gpio_set_value(PWM_IO, 0);
		pwm_dev->status = PWM_DISABLE;
	}

	return count;
}

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

static ssize_t show_mac_addr(struct class *cls,
				struct class_attribute *attr, char *buf)
{
	int ret;
	unsigned char addr_Ascii[12]={0};
	unsigned char addr[6]={0};
	ret = rk_vendor_read(LAN_MAC_ID, addr, 6);
	if (ret != 6) 
		printk("%s: rk_vendor_read eth mac address failed (%d)",__func__, ret);
	/*printk("%s: mac address: %02x:%02x:%02x:%02x:%02x:%02x\n",
				__func__, addr[0], addr[1], addr[2],
				addr[3], addr[4], addr[5]);*/
    HexToAscii(addr,addr_Ascii,6);
	return sprintf(buf, "%s\n", addr_Ascii);
}	

static ssize_t store_mac_addr(struct class *cls, struct class_attribute *attr,
		        const char *buf, size_t count)
{
	int ret;
	char addr_Ascii[12]={0};
	unsigned char addr[6]={0};
    int outlen = 0;
	
	sscanf(buf,"%s",addr_Ascii);

    StringToHex(addr_Ascii,addr,&outlen);
	printk("%s: mac address: %02x:%02x:%02x:%02x:%02x:%02x\n",
			__func__, addr[0], addr[1], addr[2],
			addr[3], addr[4], addr[5]);
	ret = rk_vendor_write(LAN_MAC_ID, &addr, 6);
	if (ret != 0)
		printk("%s: rk_vendor_write eth mac address failed (%d)\n",__func__, ret);
	return count;
}
				
static struct class_attribute w25q128fw_attrs[] = {
	__ATTR(id, 0644, show_w25q128fw_id, NULL),
	__ATTR(usb2, 0644, show_usb2, NULL),
	__ATTR(usb3, 0644, show_usb3, NULL),
	__ATTR(fusb302, 0644, show_fusb302, NULL),
	__ATTR(charge, 0644, show_charge, NULL),
	__ATTR(key, 0644, show_key, NULL),
	__ATTR(buzzer, 0644, NULL, store_buzzer),
	__ATTR(mac_addr, 0644, show_mac_addr, store_mac_addr),
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
	pwm_gpio_init();
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
