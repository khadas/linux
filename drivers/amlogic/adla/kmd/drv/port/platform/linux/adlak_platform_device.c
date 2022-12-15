/*******************************************************************************
 * Copyright (C) 2022 Amlogic, Inc. All rights reserved.
 ******************************************************************************/

/*****************************************************************************/
/**
 *
 * @file adlak_platform_device.c
 * @brief
 *
 * <pre>
 * MODIFICATION HISTORY:
 *
 * Ver   	Who				Date				Changes
 * ----------------------------------------------------------------------------
 * 1.00a shiwei.sun@amlogic.com	2021/06/05	Initial release
 * </pre>
 *
 ******************************************************************************/

/***************************** Include Files *********************************/

#include "adlak_common.h"
#include "adlak_debugfs.h"
#include "adlak_device.h"
#include "adlak_dpm.h"
#include "adlak_hw.h"
#include "adlak_platform_config.h"
#include "adlak_profile.h"
#include "adlak_submit.h"

/************************** Constant Definitions *****************************/

/**************************** Type Definitions *******************************/

/***************** Macros (Inline Functions) Definitions *********************/

/************************** Variable Definitions *****************************/
int g_adlak_log_level;
#if ADLAK_DEBUG
int g_adlak_log_level_pre;
#endif

static bool   misc_dev_en = false;
static int    adlak_major = -1;
struct class *adlak_class = NULL;
static DEFINE_IDA(adlak_ida);

#ifdef CONFIG_OF
static const struct of_device_id adlak_child_pdev_match[] = {{
                                                                 .name       = "adla",
                                                                 .compatible = "amlogic,adla",
                                                             },
                                                             {}};

MODULE_DEVICE_TABLE(of, adlak_child_pdev_match);
#endif

struct regulator            *regulator_nna;
#define REGULATOR_NAME      "vdd_npu"
/************************** Function Prototypes ******************************/

/*****************************************************************************/
/**
 *
 * @brief         :remove operation registered to platfom_driver struct
 *        This function will be called while the module is unloading.
 * @pdev[in]     :platform device struct pointer
 * @return        :0 if successful; others if failed.
 *
 ******************************************************************************/

#include "adlak_fops.c"
static int adlak_destroy_cdev(struct adlak_device *padlak) {
    if (!padlak) {
        return ERR(ENOMEM);
    }
    AML_LOG_DEBUG("%s", __func__);

#ifdef CONFIG_ADLAK_ENABLE_SYSFS
    adlak_destroy_sysfs(padlak);
#endif
    device_destroy(padlak->class, padlak->cdev.dev);
    cdev_del(&padlak->cdev);
    ida_simple_remove(&adlak_ida, MINOR(padlak->cdev.dev));
    return 0;
}

static int adlak_create_cdev(struct adlak_device *padlak) {
    int            id, ret;
    struct device *sysdev;
    AML_LOG_DEBUG("%s", __func__);
    if (!padlak) {
        return ERR(ENOMEM);
    }
    id = ida_simple_get(&adlak_ida, 0, ADLAK_MAX_DEVICES, ADLAK_GFP_KERNEL);
    if (id < 0) {
        return id;
    }

    padlak->devt = MKDEV(adlak_major, id);

    padlak->major = adlak_major;
    AML_LOG_INFO("adla id:%d.%d", padlak->major, MINOR(padlak->devt));

    cdev_init(&padlak->cdev, &padlak->fops);
    padlak->cdev.owner = THIS_MODULE;

    ret = cdev_add(&padlak->cdev, padlak->devt, 1);
    if (ret) {
        AML_LOG_ERR("unable to add character device\n");
        goto err_early_exit;
    }

    padlak->class = adlak_class;

    sysdev = device_create(padlak->class, padlak->dev, padlak->devt, padlak, DEVICE_NAME "%d", id);
    if (IS_ERR(sysdev)) {
        AML_LOG_ERR("device register failed\n");
        ret = ADLAK_PTR_ERR(sysdev);
        goto err_remove_chardev;
    }

#ifdef CONFIG_ADLAK_ENABLE_SYSFS
    ret = adlak_create_sysfs(padlak);
    if (ret) {
        goto err_destroy_device;
    }
#endif

    return 0;

err_destroy_device:
    device_destroy(padlak->class, padlak->devt);
err_remove_chardev:
    cdev_del(&padlak->cdev);
err_early_exit:
    return ret;
}

static int adlak_create_misc(struct adlak_device *padlak) {
    int ret = 0;

    if (!padlak) return ERR(EINVAL);

    padlak->misc = devm_kzalloc(padlak->dev, sizeof(struct miscdevice), ADLAK_GFP_KERNEL);
    if (!padlak->misc) {
        AML_LOG_ERR("no memory in allocating misc struct");
        return ERR(ENOMEM);
    }

    /* misc init */
    padlak->misc->minor = MISC_DYNAMIC_MINOR;
    padlak->misc->name  = DEVICE_NAME;
    padlak->misc->fops  = &padlak->fops;
    padlak->misc->mode  = 0666;
    ret                 = misc_register(padlak->misc);
    if (ret) {
        AML_LOG_ERR("ADLAK misc register failed");
    }

    return ret;
}

static int adlak_destroy_misc(struct adlak_device *padlak) {
    if (padlak) {
        misc_deregister(padlak->misc);
    }
    return 0;
}

int adlak_voltage_init(void *data) {
    int ret = 0;
    struct adlak_device *padlak = (struct adlak_device *)data;

    regulator_nna = devm_regulator_get(padlak->dev, REGULATOR_NAME);
    if (IS_ERR(regulator_nna)) {
        AML_LOG_ERR("regulator_get vddnpu fail!\n");
    }

    ret = regulator_enable(regulator_nna);
    if (ret < 0)
    {
        AML_LOG_ERR("regulator_enable error\n");
    }

    AML_LOG_INFO("ADLA KMD voltage init success ");

    return ret;
}

int adlak_voltage_uninit(void *data) {
    int ret = 0;

    if (regulator_nna == NULL)
    {
        AML_LOG_ERR("regulator supply is NULL\n");
        return -1;
    }
    ret = regulator_disable(regulator_nna);
    if (ret < 0)
    {
        AML_LOG_ERR("regulator_disable error\n");
    }

    devm_regulator_put(regulator_nna);

    AML_LOG_INFO("ADLA KMD voltage uninit success ");
    return ret;
}

static int adlak_platform_remove(struct platform_device *pdev) {
    int                  ret    = 0;
    struct adlak_device *padlak = platform_get_drvdata(pdev);
    AML_LOG_DEBUG("%s", __func__);
    adlak_device_deinit((void *)padlak);
    adlak_os_mutex_lock(&padlak->dev_mutex);

    if (padlak->regulator_nn_en)
    {
        ret = adlak_voltage_uninit(padlak);
        if (ret < 0)
        {
            AML_LOG_ERR("voltage uninit fail!\n");
        }
    }

    adlak_platform_free_resource(padlak);

    if (misc_dev_en == true) {
        ret = adlak_destroy_misc(padlak);
    } else {
        ret = adlak_destroy_cdev(padlak);
    }
    adlak_os_mutex_unlock(&padlak->dev_mutex);

    adlak_os_spinlock_destroy(&padlak->spinlock);
    adlak_os_mutex_destroy(&padlak->dev_mutex);
    adlak_os_free(padlak);
    /* success */
    AML_LOG_INFO("ADLA KMD remove done");
    return 0;
}

/**
 * @brief probe operation registered to platfom_driver struct
 *        This function will be called while the module is loading.
 *
 * @param pdev: platform device struct pointer
 * @return 0 if successful; others if failed.
 */
static int adlak_platform_probe(struct platform_device *pdev) {
    int                  ret    = 0;
    struct adlak_device *padlak = NULL;
    AML_LOG_DEBUG("%s", __func__);

    padlak = adlak_os_zalloc(sizeof(struct adlak_device), ADLAK_GFP_KERNEL);
    if (!padlak) {
        ret = ERR(ENOMEM);
        goto err_alloc_data;
    }

    dev_set_drvdata(&pdev->dev, padlak);
    padlak->dev  = &pdev->dev;
    padlak->pdev = pdev;

    /* Set the internal DMA address mask
     * This is the max address of the ADLA's internal address space.
     */
    if (dma_set_mask_and_coherent(padlak->dev, DMA_BIT_MASK(34))) {
        AML_LOG_WARN("set device dma mask failed,No suitable DMA available!");
    }
    padlak->net_id = 0;
    ret            = adlak_platform_get_resource(padlak);
    if (ret) {
        goto err_get_res;
    }
    /* set voltage */
    if (padlak->regulator_nn_en)
    {
        ret = adlak_voltage_init(padlak);
        if (ret < 0)
        {
            AML_LOG_ERR("voltage init fail!\n");
        }
    }
    ret = adlak_platform_request_resource(padlak);
    if (ret) {
        goto err_req_res;
    }
    ret = adlak_device_init(padlak);
    if (ret) {
        goto err_dev_init;
    }
    ret = adlak_register_fops(padlak);
    if (ret) {
        goto err_reg_fops;
    }
    /*device create*/
    if (misc_dev_en == true) {
        ret = adlak_create_misc(padlak);
    } else {
        ret = adlak_create_cdev(padlak);
    }
    if (ret) {
        goto err_create_dev;
    }

    AML_LOG_INFO("ADLAK probe done");

    return 0;
err_create_dev:
err_reg_fops:
err_dev_init:
    adlak_platform_free_resource(padlak);
err_req_res:
    adlak_device_deinit(padlak);
err_get_res:
    adlak_os_free(padlak);
err_alloc_data:
    return ret;
}


static int adlak_platform_sys_suspend(struct platform_device *pdev,pm_message_t state) {
    int                  ret    = 0;
    struct adlak_device *padlak = platform_get_drvdata(pdev);
    AML_LOG_DEBUG("%s", __func__);
    adlak_platform_suspend(padlak);
    /* success */
    AML_LOG_INFO("ADLA KMD suspend done");
    return 0;
}

static int adlak_platform_sys_resume(struct platform_device *pdev) {
    int                  ret    = 0;
    struct adlak_device *padlak = platform_get_drvdata(pdev);
    AML_LOG_DEBUG("%s", __func__);
    adlak_platform_resume(padlak);
    /* success */
    AML_LOG_INFO("ADLA KMD resume done");
    return 0;
}

static int adlak_platform_sys_pm_suspend(struct device *dev)
{
    pm_message_t state={0};
    return adlak_platform_sys_suspend(to_platform_device(dev), state);
}

static int adlak_platform_sys_pm_resume(struct device *dev)
{
    return adlak_platform_sys_resume(to_platform_device(dev));
}
static const struct dev_pm_ops viv_dev_pm_ops = {
    SET_SYSTEM_SLEEP_PM_OPS(adlak_platform_sys_pm_suspend, adlak_platform_sys_pm_resume)
};

static struct platform_driver adlak_platform_driver = {
    .probe  = adlak_platform_probe,
    .remove = adlak_platform_remove,
    .suspend = adlak_platform_sys_suspend,
    .resume = adlak_platform_sys_resume,
    .driver =
        {
            .name  = DEVICE_NAME,
            .owner = THIS_MODULE,
#ifdef CONFIG_OF
            .of_match_table = of_match_ptr(adlak_child_pdev_match),
#endif
            .pm     = &viv_dev_pm_ops,
        },
};

/*****************************************************************************
 * Module initialization and destruction
 *****************************************************************************/

static int adlak_major_init(void) {
    dev_t devt;
    int   ret;

    ret = alloc_chrdev_region(&devt, 0, ADLAK_MAX_DEVICES, DEVICE_NAME);
    if (ret) {
        AML_LOG_ERR("alloc_chrdev_region failed.");
        return ret;
    }
    adlak_major = MAJOR(devt);
    return 0;
}

static void adlak_major_cleanup(void) {
    unregister_chrdev_region(MKDEV(adlak_major, 0), ADLAK_MAX_DEVICES);
}

static int adlak_class_init(void) {
    int ret;

    /* This is the first time in here, set everything up properly */
    if (misc_dev_en == false) {
        ret = adlak_major_init();
        if (ret) {
            return ret;
        }
    }
    adlak_class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(adlak_class)) {
        AML_LOG_ERR("class_create failed for adla.");
        ret = ADLAK_PTR_ERR(adlak_class);
        goto cleanup;
    }
    if (adlak_create_class_file(adlak_class)) {
        ret = ADLAK_PTR_ERR(adlak_class);
        goto cleanup;
    }
    return 0;
cleanup:
    adlak_major_cleanup();

    return ret;
}

static void adlak_class_release(void) {
    adlak_destroy_class_file(adlak_class);
    class_destroy(adlak_class);
    if (misc_dev_en == false) {
        adlak_major_cleanup();
    }
}

static int __init adlak_module_init(void) {
    int ret           = 0;
    g_adlak_log_level = ADLAK_LOG_LEVEL;
#if ADLAK_DEBUG
    g_adlak_log_level_pre = g_adlak_log_level;
#endif
    AML_LOG_DEBUG("%s", __func__);
    ret = adlak_class_init();

    if (ret) {
        return ret;
    }

#ifndef CONFIG_OF
    ret = adlak_platform_device_init();
    if (0 > ret) {
        AML_LOG_ERR("Soc platform driver init failed.");
        return ERR(ENODEV);
    }
#endif
    ret = platform_driver_register(&adlak_platform_driver);
    if (0 > ret) {
        AML_LOG_ERR("platform driver register failed.");
        return ERR(ENODEV);
    }
    AML_LOG_DEBUG("platform_driver_register successed.");
    return 0;
}

static void __exit adlak_module_exit(void) {
    AML_LOG_DEBUG("%s", __func__);
    platform_driver_unregister(&adlak_platform_driver);
#ifndef CONFIG_OF
    if (0 > adlak_platform_device_uninit()) {
        AML_LOG_ERR("Soc platform driver uninit failed.");
    }
#endif
    adlak_class_release();
}
module_init(adlak_module_init) module_exit(adlak_module_exit) MODULE_LICENSE("GPL");
MODULE_AUTHOR("Amlogic ADLA GROUP");
MODULE_DESCRIPTION("Amlogic Deep Learn Accelarator Driver");
MODULE_VERSION(ADLAK_VERSION);