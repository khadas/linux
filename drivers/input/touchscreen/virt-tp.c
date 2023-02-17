#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/input.h>

#define SCREEN_MAX_X     1920
#define SCREEN_MAX_Y     1080
#define PRESS_MAX        0xFF

struct input_dev *ts_input_dev;

int __init virtTp_init(void)
{
    int ret;

    ts_input_dev = input_allocate_device();
    if (ts_input_dev == NULL){
        printk("vtp input_allocate_device fail\n");
        return -ENOMEM;
    }

    ts_input_dev->name = "vtp";
    set_bit(ABS_MT_TOUCH_MAJOR, ts_input_dev->absbit);
    set_bit(ABS_MT_POSITION_X, ts_input_dev->absbit);
    set_bit(ABS_MT_POSITION_Y, ts_input_dev->absbit);
    set_bit(ABS_MT_WIDTH_MAJOR, ts_input_dev->absbit);
    set_bit(BTN_TOUCH, ts_input_dev->keybit);
    set_bit(EV_ABS, ts_input_dev->evbit);
    set_bit(EV_KEY, ts_input_dev->evbit);
    set_bit(INPUT_PROP_DIRECT, ts_input_dev->propbit);

    input_set_abs_params(ts_input_dev, ABS_X, 0, SCREEN_MAX_X, 0, 0);
    input_set_abs_params(ts_input_dev, ABS_Y, 0, SCREEN_MAX_Y, 0, 0);
    input_set_abs_params(ts_input_dev, ABS_MT_POSITION_X, 0, SCREEN_MAX_X, 0, 0);
    input_set_abs_params(ts_input_dev, ABS_MT_POSITION_Y, 0, SCREEN_MAX_Y, 0, 0);
    input_set_abs_params(ts_input_dev, ABS_MT_TOUCH_MAJOR, 0, PRESS_MAX, 0, 0);
    input_set_abs_params(ts_input_dev, ABS_MT_WIDTH_MAJOR, 0, 200, 0, 0);
    input_set_abs_params(ts_input_dev, ABS_MT_TRACKING_ID, 0, 5, 0, 0);

    ret = input_register_device(ts_input_dev);
    if (ret){
        printk("vtp input_register_device fail\n");
        return ret;
    }

    return 0;
}

void __exit virtTp_exit(void)
{
    input_unregister_device(ts_input_dev);
    input_free_device(ts_input_dev);
}

module_init(virtTp_init);
module_exit(virtTp_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Touchscreen driver");
