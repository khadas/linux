#include <linux/i2c.h>
#include <linux/i2c-algo-bit.h>
#include <linux/i2c-gpio.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/pinctrl/consumer.h>

#define pr_dbg(args...)\
    do {\
        printk(args);\
    } while (0)
#define pr_error(fmt, args...) printk("dtv_params: " fmt, ## args)
#define pr_inf(fmt, args...)   printk("dtv_params: " fmt, ## args)


static struct pinctrl *s_pinctrl = NULL;

static unsigned int s_demodReset_1_Pin, s_antPower_1_Pin, s_demodReset_2_Pin, s_antPower_2_Pin, s_tunerPowerEnable_1_Pin, s_tunerPowerEnable_2_Pin, s_userDefined_1_Pin, s_userDefined_2_Pin;
static unsigned char demodPin_1_Detected = 0, antPowerPin_1_Detected = 0, demodPin_2_Detected = 0, antPowerPin_2_Detected = 0, tunerPowerEnablePin_1_Detected = 0, tunerPowerEnablePin_2_Detected = 0, userDefined_1_Detected = 0, userDefined_2_Detected = 0;

static void set_demod_reset_1_pin(unsigned char val)
{
    if(demodPin_1_Detected){
        gpio_set_value(s_demodReset_1_Pin, val);
        pr_err("set_demod_reset_1_pin : %d\r\n", val);
    }
    else{
        pr_err("error : demod reset 1 pin not config..\r\n");
    }
}

static void set_ant_power_1_pin(unsigned char val)
{
    if(antPowerPin_1_Detected){
        gpio_set_value(s_antPower_1_Pin, val);
        pr_err("set_ant_power_1_pin : %d\r\n", val);
    }
    else{
        pr_err("error : ant power 1 pin not config..\r\n");
    }
}

static void set_demod_reset_2_pin(unsigned char val)
{
    if(demodPin_2_Detected){
        gpio_set_value(s_demodReset_2_Pin, val);
        pr_err("set_demod_reset_2_pin : %d\r\n", val);
    }
    else{
        pr_err("error : demod reset 2 pin not config..\r\n");
    }
}

static void set_ant_power_2_pin(unsigned char val)
{
    if(antPowerPin_2_Detected){
        gpio_set_value(s_antPower_2_Pin, val);
        pr_err("set_ant_power_2_pin : %d\r\n", val);
    }
    else{
        pr_err("error : ant power 2 pin not config..\r\n");
    }
}

static void set_tuner_power_enable_1_pin(unsigned char val)
{
    if(tunerPowerEnablePin_1_Detected){
        gpio_set_value(s_tunerPowerEnable_1_Pin, val);
        pr_err("set_tuner_power_enable_1_pin : %d\r\n", val);
    }
    else{
        pr_err("error : tuner power enable 1 pin not config..\r\n");
    }
}

static void set_tuner_power_enable_2_pin(unsigned char val)
{
    if(tunerPowerEnablePin_2_Detected){
        gpio_set_value(s_tunerPowerEnable_2_Pin, val);
        pr_err("set_tuner_power_enable_2_pin : %d\r\n", val);
    }
    else{
        pr_err("error : tuner power enable 2 pin not config..\r\n");
    }
}

static void set_user_defined_1_pin(unsigned char val)
{
    if(userDefined_1_Detected){
        gpio_set_value(s_userDefined_1_Pin, val);
        pr_err("set_user_defined_1_pin : %d\r\n", val);
    }
    else{
        pr_err("error : user defined 1 pin not config..\r\n");
    }
}

static void set_user_defined_2_pin(unsigned char val)
{
    if(userDefined_2_Detected){
        gpio_set_value(s_userDefined_2_Pin, val);
        pr_err("set_user_defined_2_pin : %d\r\n", val);
    }
    else{
        pr_err("error : user defined 2 pin not config..\r\n");
    }
}

static int get_demod_reset_1_pin(void)
{
    int val = 0;
    if(demodPin_1_Detected){
        val = gpio_get_value(s_demodReset_1_Pin);
    }

    return val;
}

static int get_ant_power_1_pin(void)
{
    int val = 0;
    if(antPowerPin_1_Detected){
        val = gpio_get_value(s_antPower_1_Pin);
    }

    return val;
}

static int get_demod_reset_2_pin(void)
{
    int val = 0;
    if(demodPin_2_Detected){
        val = gpio_get_value(s_demodReset_2_Pin);
    }

    return val;
}

static int get_ant_power_2_pin(void)
{
    int val = 0;
    if(antPowerPin_2_Detected){
        val = gpio_get_value(s_antPower_2_Pin);
    }

    return val;
}

static int get_tuner_power_enable_1_pin(void)
{
    int val = 0;
    if(tunerPowerEnablePin_1_Detected){
        val = gpio_get_value(s_tunerPowerEnable_1_Pin);
    }

    return val;
}

static int get_tuner_power_enable_2_pin(void)
{
    int val = 0;
    if(tunerPowerEnablePin_2_Detected){
        val = gpio_get_value(s_tunerPowerEnable_2_Pin);
    }

    return val;
}

static int get_user_defined_1_pin(void)
{
    int val = 0;
    if(userDefined_1_Detected){
        val = gpio_get_value(s_userDefined_1_Pin);
    }

    return val;
}

static int get_user_defined_2_pin(void)
{
    int val = 0;
    if(userDefined_2_Detected){
        val = gpio_get_value(s_userDefined_2_Pin);
    }

    return val;
}

static ssize_t stb_show_demod_reset_1_pin(struct class *class,
                   struct class_attribute *attr, char *buf)
{
    ssize_t ret = 0;
    if(demodPin_1_Detected)
        ret = sprintf(buf, "%d\n", get_demod_reset_1_pin());
    else
        ret = sprintf(buf, "error : demod reset 1 pin not config..\n");

    return ret;
}

static ssize_t stb_store_demod_reset_1_pin(struct class *class,
                struct class_attribute *attr, const char *buf,
                size_t size)
{
    if (!strncmp("0", buf, 1))
        set_demod_reset_1_pin(0);
    else
        set_demod_reset_1_pin(1);

    return size;
}

static ssize_t stb_show_ant_power_1_pin(struct class *class,
                   struct class_attribute *attr, char *buf)
{
    ssize_t ret = 0;

    if(antPowerPin_1_Detected)
        ret = sprintf(buf, "%d\n", get_ant_power_1_pin());
    else
        ret = sprintf(buf, "error : ant power 1 pin not config..\n");

    return ret;
}

static ssize_t stb_store_ant_power_1_pin(struct class *class,
                struct class_attribute *attr, const char *buf,
                size_t size)
{
    if (!strncmp("0", buf, 1))
        set_ant_power_1_pin(0);
    else
        set_ant_power_1_pin(1);

    return size;
}

static ssize_t stb_show_tuner_power_enable_1_pin(struct class *class,
                   struct class_attribute *attr, char *buf)
{
    ssize_t ret = 0;

    if(tunerPowerEnablePin_1_Detected)
        ret = sprintf(buf, "%d\n", get_tuner_power_enable_1_pin());
    else
        ret = sprintf(buf, "error : tuner power enable 1 pin not config..\n");

    return ret;
}

static ssize_t stb_store_tuner_power_enable_1_pin(struct class *class,
                struct class_attribute *attr, const char *buf,
                size_t size)
{
    if (!strncmp("0", buf, 1))
        set_tuner_power_enable_1_pin(0);
    else
        set_tuner_power_enable_1_pin(1);

    return size;
}

static ssize_t stb_show_demod_reset_2_pin(struct class *class,
                   struct class_attribute *attr, char *buf)
{
    ssize_t ret = 0;

    if(demodPin_2_Detected)
        ret = sprintf(buf, "%d\n", get_demod_reset_2_pin());
    else
        ret = sprintf(buf, "error : demod reset 2 pin not config..\n");

    return ret;
}

static ssize_t stb_store_demod_reset_2_pin(struct class *class,
                struct class_attribute *attr, const char *buf,
                size_t size)
{
    if (!strncmp("0", buf, 1))
        set_demod_reset_2_pin(0);
    else
        set_demod_reset_2_pin(1);

    return size;
}

static ssize_t stb_show_ant_power_2_pin(struct class *class,
                   struct class_attribute *attr, char *buf)
{
    ssize_t ret = 0;

    if(antPowerPin_2_Detected)
        ret = sprintf(buf, "%d\n", get_ant_power_2_pin());
    else
        ret = sprintf(buf, "error : ant power 2 pin not config..\n");

    return ret;
}

static ssize_t stb_store_ant_power_2_pin(struct class *class,
                struct class_attribute *attr, const char *buf,
                size_t size)
{
    if (!strncmp("0", buf, 1))
        set_ant_power_2_pin(0);
    else
        set_ant_power_2_pin(1);

    return size;
}

static ssize_t stb_show_tuner_power_enable_2_pin(struct class *class,
                   struct class_attribute *attr, char *buf)
{
    ssize_t ret = 0;

    if(tunerPowerEnablePin_2_Detected)
        ret = sprintf(buf, "%d\n", get_tuner_power_enable_2_pin());
    else
        ret = sprintf(buf, "error : tuner power enable 2 pin not config..\n");

    return ret;
}

static ssize_t stb_store_tuner_power_enable_2_pin(struct class *class,
                struct class_attribute *attr, const char *buf,
                size_t size)
{
    if (!strncmp("0", buf, 1))
        set_tuner_power_enable_2_pin(0);
    else
        set_tuner_power_enable_2_pin(1);

    return size;
}

static ssize_t stb_show_user_defined_1_pin(struct class *class,
                   struct class_attribute *attr, char *buf)
{
    ssize_t ret = 0;

    if(userDefined_1_Detected)
        ret = sprintf(buf, "%d\n", get_user_defined_1_pin());
    else
        ret = sprintf(buf, "error : user defined 1 pin not config..\n");

    return ret;
}

static ssize_t stb_store_user_defined_1_pin(struct class *class,
                struct class_attribute *attr, const char *buf,
                size_t size)
{
    if (!strncmp("0", buf, 1))
        set_user_defined_1_pin(0);
    else
        set_user_defined_1_pin(1);

    return size;
}

static ssize_t stb_show_user_defined_2_pin(struct class *class,
                   struct class_attribute *attr, char *buf)
{
    ssize_t ret = 0;

    if(userDefined_2_Detected)
        ret = sprintf(buf, "%d\n", get_user_defined_2_pin());
    else
        ret = sprintf(buf, "error : user defined 2 pin not config..\n");

    return ret;
}

static ssize_t stb_store_user_defined_2_pin(struct class *class,
                struct class_attribute *attr, const char *buf,
                size_t size)
{
    if (!strncmp("0", buf, 1))
        set_user_defined_2_pin(0);
    else
        set_user_defined_2_pin(1);

    return size;
}

static struct class_attribute dtv_params_class_attrs[] = {
    __ATTR(demod_reset_1_pin, S_IRUGO | S_IWUGO, stb_show_demod_reset_1_pin,
           stb_store_demod_reset_1_pin),
    __ATTR(ant_power_1_pin, S_IRUGO | S_IWUGO, stb_show_ant_power_1_pin,
           stb_store_ant_power_1_pin),
    __ATTR(tuner_power_enable_1_pin, S_IRUGO | S_IWUGO, stb_show_tuner_power_enable_1_pin,
           stb_store_tuner_power_enable_1_pin),
    __ATTR(demod_reset_2_pin, S_IRUGO | S_IWUGO, stb_show_demod_reset_2_pin,
           stb_store_demod_reset_2_pin),
    __ATTR(ant_power_2_pin, S_IRUGO | S_IWUGO, stb_show_ant_power_2_pin,
           stb_store_ant_power_2_pin),
    __ATTR(tuner_power_enable_2_pin, S_IRUGO | S_IWUGO, stb_show_tuner_power_enable_2_pin,
           stb_store_tuner_power_enable_2_pin),
    __ATTR(user_defined_1_pin, S_IRUGO | S_IWUGO, stb_show_user_defined_1_pin,
           stb_store_user_defined_1_pin),
    __ATTR(user_defined_2_pin, S_IRUGO | S_IWUGO, stb_show_user_defined_2_pin,
           stb_store_user_defined_2_pin),

    __ATTR_NULL
};


static struct class dtv_params_class = {
    .name = "dtv-params",
    .class_attrs = dtv_params_class_attrs,
};

static int dtv_params_probe(struct platform_device *pdev)
{
    int ret = 0;

    pr_inf("probe dtv params driver : start\n");

    if (pdev->dev.of_node) {
        s_demodReset_1_Pin = of_get_named_gpio_flags(pdev->dev.of_node, "demod_reset_1-gpio", 0, NULL);
        s_antPower_1_Pin = of_get_named_gpio_flags(pdev->dev.of_node, "ant_power_1-gpio", 0, NULL);
        s_tunerPowerEnable_1_Pin = of_get_named_gpio_flags(pdev->dev.of_node, "tuner_power_enable_1-gpio", 0, NULL);
        s_demodReset_2_Pin = of_get_named_gpio_flags(pdev->dev.of_node, "demod_reset_2-gpio", 0, NULL);
        s_antPower_2_Pin = of_get_named_gpio_flags(pdev->dev.of_node, "ant_power_2-gpio", 0, NULL);
        s_tunerPowerEnable_2_Pin = of_get_named_gpio_flags(pdev->dev.of_node, "tuner_power_enable_2-gpio", 0, NULL);
        s_userDefined_1_Pin = of_get_named_gpio_flags(pdev->dev.of_node, "user_defined_1-gpio", 0, NULL);
        s_userDefined_2_Pin = of_get_named_gpio_flags(pdev->dev.of_node, "user_defined_2-gpio", 0, NULL);

        if (!gpio_is_valid(s_demodReset_1_Pin)) {
            pr_err("%s: invalid GPIO pin, s_demodReset_1_Pin=%d\n",
                   pdev->dev.of_node->full_name, s_demodReset_1_Pin);
        }
        else{
            demodPin_1_Detected = 1;
            gpio_request(s_demodReset_1_Pin, "dtv_params");
            gpio_direction_output(s_demodReset_1_Pin, 1);
        }

        if (!gpio_is_valid(s_antPower_1_Pin)) {
            pr_err("%s: invalid GPIO pin, s_antPower_1_Pin=%d, skip this pin\n",
                   pdev->dev.of_node->full_name, s_antPower_1_Pin);
        }
        else{
            antPowerPin_1_Detected = 1;
            gpio_request(s_antPower_1_Pin, "dtv_params");
            gpio_direction_output(s_antPower_1_Pin, 0); //ant power default set to low
        }

        if (!gpio_is_valid(s_tunerPowerEnable_1_Pin)) {
            pr_err("%s: invalid GPIO pin, s_tunerPowerEnable_1_Pin=%d, skip this pin\n",
                   pdev->dev.of_node->full_name, s_tunerPowerEnable_1_Pin);
        }
        else{
            tunerPowerEnablePin_1_Detected = 1;
            gpio_request(s_tunerPowerEnable_1_Pin, "dtv_params");
            gpio_direction_output(s_tunerPowerEnable_1_Pin, 0); //ant tuner power enable default set to off
        }

        if (!gpio_is_valid(s_demodReset_2_Pin)) {
            pr_err("%s: invalid GPIO pin, s_demodReset_2_Pin=%d\n",
                   pdev->dev.of_node->full_name, s_demodReset_2_Pin);
        }
        else{
            demodPin_2_Detected = 1;
            gpio_request(s_demodReset_2_Pin, "dtv_params");
            gpio_direction_output(s_demodReset_2_Pin, 1);
        }

        if (!gpio_is_valid(s_antPower_2_Pin)) {
            pr_err("%s: invalid GPIO pin, s_antPower_2_Pin=%d, skip this pin\n",
                   pdev->dev.of_node->full_name, s_antPower_2_Pin);
        }
        else{
            antPowerPin_2_Detected = 1;
            gpio_request(s_antPower_2_Pin, "dtv_params");
            gpio_direction_output(s_antPower_2_Pin, 0); //ant power default set to low
        }

        if (!gpio_is_valid(s_tunerPowerEnable_2_Pin)) {
            pr_err("%s: invalid GPIO pin, s_tunerPowerEnable_2_Pin=%d, skip this pin\n",
                   pdev->dev.of_node->full_name, s_tunerPowerEnable_2_Pin);
        }
        else{
            tunerPowerEnablePin_2_Detected = 1;
            gpio_request(s_tunerPowerEnable_2_Pin, "dtv_params");
            gpio_direction_output(s_tunerPowerEnable_2_Pin, 0); //ant tuner power enable default set to off
        }

        if (!gpio_is_valid(s_userDefined_1_Pin)) {
            pr_err("%s: invalid GPIO pin, s_userDefined_1_Pin=%d, skip this pin\n",
                   pdev->dev.of_node->full_name, s_userDefined_1_Pin);
        }
        else{
            userDefined_1_Detected = 1;
            gpio_request(s_userDefined_1_Pin, "dtv_params");
            gpio_direction_output(s_userDefined_1_Pin, 0); //default set to low
        }

        if (!gpio_is_valid(s_userDefined_2_Pin)) {
            pr_err("%s: invalid GPIO pin, s_userDefined_2_Pin=%d, skip this pin\n",
                   pdev->dev.of_node->full_name, s_userDefined_2_Pin);
        }
        else{
            userDefined_2_Detected = 1;
            gpio_request(s_userDefined_2_Pin, "dtv_params");
            gpio_direction_output(s_userDefined_2_Pin, 0); //default set to low
        }

        s_pinctrl = devm_pinctrl_get_select(&pdev->dev, "default");
        pr_err("set pinctrl : %p\n", s_pinctrl);

        if (class_register(&dtv_params_class) < 0) {
            pr_error("register class error\n");
            goto error;
        }
    }

    pr_inf("probe dtv params driver : end\n");

    return 0;
error:

    return ret;
}

static int dtv_params_remove(struct platform_device *pdev)
{
    if (s_pinctrl) {
        devm_pinctrl_put(s_pinctrl);
        s_pinctrl = NULL;
    }

    return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id dtv_params_dt_match[] = {
    {
     .compatible = "dtv-params",
     },
    {},
};
#endif /*CONFIG_OF */

static struct platform_driver dtv_params_driver = {
    .probe = dtv_params_probe,
    .remove = dtv_params_remove,
    .driver = {
           .name = "dtv-params",
           .owner = THIS_MODULE,
#ifdef CONFIG_OF
           .of_match_table = dtv_params_dt_match,
#endif
           }
};

static int __init dtv_params_init(void)
{
    pr_dbg("dtv params init\n");
    return platform_driver_register(&dtv_params_driver);
}

static void __exit dtv_params_exit(void)
{
    pr_dbg("dtv params exit\n");
    platform_driver_unregister(&dtv_params_driver);
}

module_init(dtv_params_init);
module_exit(dtv_params_exit);

MODULE_DESCRIPTION("driver for the AMLogic dtv params");
MODULE_AUTHOR("AMLOGIC");
MODULE_LICENSE("GPL");
