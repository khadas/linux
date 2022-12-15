/*******************************************************************************
 * Copyright (C) 2021 Amlogic, Inc. All rights reserved.
 ******************************************************************************/

/*****************************************************************************/
/**
 *
 * @file adlak_debugfs.c
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
#include "adlak_debugfs.h"

#include "adlak_common.h"
#include "adlak_device.h"
#include "adlak_hw.h"
#include "adlak_io.h"
#include "adlak_submit.h"
/************************** Constant Definitions *****************************/

/**************************** Type Definitions *******************************/

/***************** Macros (Inline Functions) Definitions *********************/

/************************** Variable Definitions *****************************/

/************************** Function Prototypes ******************************/

static ssize_t kmd_version_show(struct device *dev, struct device_attribute *attr, char *buf) {
    return adlak_os_snprintf(buf, MAX_CHAR_SYSFS, "ADLA Version: %s\n", ADLAK_VERSION);
}

static DEVICE_ATTR_RO(kmd_version);

static ssize_t tasks_show(struct device *dev, struct device_attribute *attr, char *buf) {
    struct adlak_device *padlak = dev_get_drvdata(dev);
    ASSERT(padlak);

    return adlak_debug_invoke_list_dump(padlak, 0);
}

static DEVICE_ATTR_RO(tasks);

static ssize_t clock_gating_show(struct device *dev, struct device_attribute *attr, char *buf) {
    struct adlak_device *padlak = dev_get_drvdata(dev);
    ASSERT(padlak);
    if (padlak->is_suspend) {
        return adlak_os_snprintf(buf, MAX_CHAR_SYSFS,
                                 "ADLA is in clock gating state and suspended.\n");
    } else if (padlak->is_reset) {
        return adlak_os_snprintf(buf, MAX_CHAR_SYSFS, "ADLA is in reset state and suspended.\n");
    } else {
        return adlak_os_snprintf(buf, MAX_CHAR_SYSFS, "ADLA is in normal working state.\n");
    }
    return 0;
}

static ssize_t clock_gating_store(struct device *dev, struct device_attribute *attr,
                                  const char *buf, size_t count) {
    int do_suspend = 0;
    int do_resume  = 0;
#if ADLAK_DEBUG
    struct adlak_device *padlak = dev_get_drvdata(dev);
    ASSERT(padlak);
#endif
    if ((strncmp(buf, "1", 1) == 0)) {
        do_suspend = 1;
    } else if ((strncmp(buf, "0", 1) == 0)) {
        do_resume = 1;
    }
    /*
        if ((!padlak->is_suspend) && adlak_dev_is_idle(padlak) && do_suspend) {
          AML_LOG_DEBUG( "enable clock gating\n");
          adlak_dev_enable_clk_gating(padlak);
          padlak->is_suspend = 1;
        } else if (padlak->is_suspend && do_resume)

    {
       AML_LOG_DEBUG( "disable clock gating\n");
          adlak_dev_disable_clk_gating(padlak);
          padlak->is_suspend = 0;
    }
    */

    return count;
}

static DEVICE_ATTR_RW(clock_gating);

static ssize_t reset_show(struct device *dev, struct device_attribute *attr, char *buf) {
    struct adlak_device *padlak = dev_get_drvdata(dev);
    ASSERT(padlak);
    return adlak_os_snprintf(buf, MAX_CHAR_SYSFS, "This feature is not support temporary.\n");
    if (padlak->is_reset) {
        return adlak_os_snprintf(buf, MAX_CHAR_SYSFS, "ADLA is in reset state and suspended.\n");
    } else if (padlak->is_suspend) {
        return adlak_os_snprintf(buf, MAX_CHAR_SYSFS,
                                 "ADLA is in clock gating state and suspended.\n");
    } else {
        return adlak_os_snprintf(buf, MAX_CHAR_SYSFS, "ADLA is in normal working state.\n");
    }
    return 0;
}
static ssize_t reset_store(struct device *dev, struct device_attribute *attr, const char *buf,
                           size_t count) {
    struct adlak_device *padlak = dev_get_drvdata(dev);
    ASSERT(padlak);
    if ((strncmp(buf, "1", 1) == 0)) {
        pr_info("release ADLA to normal state\n");
        //   adlak_dev_logic_release(padlak);
        //   adlak_dev_enable_interrupt(padlak);
        padlak->is_reset = 0;
    } else if ((strncmp(buf, "0", 1) == 0)) {
        if (!padlak->is_reset) {
            pr_info("reset ADLA\n");
            //  adlak_dev_logic_reset(padlak);
            padlak->is_reset = 1;
        }
    }
    // TODO(shiwei.sun) This api is not implemented
    return count;
}

static DEVICE_ATTR_RW(reset);

static int adlak_reg_check(struct adlak_device *padlak, int reg) {
    struct adlak_hw_info *phw_info = padlak->hw_info;
    int                   idx;
    for (idx = 0; idx < sizeof(phw_info->reg_lst) / sizeof(phw_info->reg_lst[0]); idx++) {
        if (phw_info->reg_lst[idx] == reg) {
            return 0;
        }
    }
    return -1;
}
static int adlak_print_reg_info(struct io_region *region, int offset, char *buf, size_t buf_size) {
    uint32_t val;
    char     reg_name[64];
    adlak_get_reg_name(offset, reg_name, sizeof(reg_name));
    val = adlak_read32(region, offset);
    return adlak_os_snprintf(buf, buf_size, "0x%-*x%-*s0x%08x", 6, offset, 22, reg_name, val);
}

static void adlak_sysfs_dump_reg(struct adlak_device *padlak) {
    struct io_region *    region   = padlak->hw_res.preg;
    struct adlak_hw_info *phw_info = padlak->hw_info;
    int                   idx, offset;
    char                  buf[128];
    for (idx = 0; idx < sizeof(phw_info->reg_lst) / sizeof(phw_info->reg_lst[0]); idx++) {
        offset = phw_info->reg_lst[idx];
        adlak_print_reg_info(region, offset, buf, sizeof(buf));
        pr_info("%s\n", buf);
    }
}

static int adlak_sysfs_read_reg(struct adlak_device *padlak, int argc, char **argv) {
    int               reg = 0;
    int               r   = 0;
    char              buf[128];
    struct io_region *region = padlak->hw_res.preg;

    if (argc < 2 || (!argv) || (!argv[0]) || (!argv[1])) {
        pr_err("Invalid syntax\n");
        return -1;
    }

    r = kstrtoint(argv[1], 0, &reg);
    if (r) {
        pr_err("kstrtoint failed\n");
        return -1;
    }
    if (0 != adlak_reg_check(padlak, reg)) {
        pr_info("Invalid parameter\n");
        return -1;
    }

    adlak_print_reg_info(region, reg, buf, sizeof(buf));
    pr_info("%s\n", buf);
    return 0;
}

static int adlak_sysfs_write_reg(struct adlak_device *padlak, int argc, char **argv) {
    int               reg;
    int               val;
    int               r;
    struct io_region *region = padlak->hw_res.preg;

    if ((argc < 3) || (!argv) || (!argv[0]) || (!argv[1]) || (!argv[2])) {
        pr_err("Invalid syntax\n");
        return -1;
    }

    r = kstrtoint(argv[1], 0, &reg);
    if (r) {
        pr_err("kstrtoint failed\n");
        return -1;
    }

    r = kstrtoint(argv[2], 0, &val);
    if (r) {
        pr_err("kstrtoint failed\n");
        return -1;
    }
    if (0 != adlak_reg_check(padlak, reg)) {
        pr_info("Invalid parameter\n");
        return -1;
    }

    adlak_write32(region, reg, val);
    pr_info("write reg [0x%x]=0x%x,confirm=0x%x\n", reg, val, adlak_read32(region, reg));

    return 0;
}
static const char *adlak_reg_help = {
    "Usage:\n"
    "    echo d >  reg;           //dump adlak reg\n"
    "    echo r reg >  reg;       //read adlak reg\n"
    "    echo w reg val > reg;    //write adlak reg\n"};

static ssize_t reg_show(struct device *dev, struct device_attribute *attr, char *buf) {
    return sprintf(buf, "%s\n", adlak_reg_help);
}
static ssize_t reg_store(struct device *dev, struct device_attribute *attr, const char *buf,
                         size_t count) {
    int                  argc;
    char *               buff, *p, *para;
    char *               argv[4];
    char                 cmd;
    struct adlak_device *padlak = dev_get_drvdata(dev);

    ASSERT(padlak);
    buff = kstrdup(buf, ADLAK_GFP_KERNEL);
    p    = buff;
    for (argc = 0; argc < 4; argc++) {
        para = strsep(&p, " ");
        if (!para) break;
        argv[argc] = para;
    }
    if (argc < 1 || argc > 4) goto end;

    cmd = argv[0][0];

    adlak_platform_resume(padlak);
    switch (cmd) {
        case 'r':
        case 'R':
            adlak_sysfs_read_reg(padlak, argc, argv);
            break;
        case 'w':
        case 'W':
            adlak_sysfs_write_reg(padlak, argc, argv);
            break;
        case 'd':
        case 'D':
            adlak_sysfs_dump_reg(padlak);
            break;
        default:
            goto end;
    }
    return count;
end:
    kfree(buff);
    return 0;
}

static DEVICE_ATTR_RW(reg);

static struct attribute *adlak_debug_attrs[] = {
    &dev_attr_tasks.attr,
    &dev_attr_clock_gating.attr,
    &dev_attr_reset.attr,
    &dev_attr_reg.attr,
    NULL,
};

static const struct attribute_group adlak_debug_attr_group = {
    .name  = "debug",
    .attrs = adlak_debug_attrs,
};

static const struct attribute_group *adlak_attr_groups[] = {
#if ADLAK_DEBUG
    &adlak_debug_attr_group,
#endif
    NULL,
};

int adlak_create_sysfs(void *adlak_device) {
    int                  ret    = 0;
    struct adlak_device *padlak = NULL;
    ASSERT(adlak_device);
    AML_LOG_DEBUG("%s", __func__);
    padlak = (struct adlak_device *)adlak_device;

    device_create_file(padlak->dev, &dev_attr_kmd_version);
    if (sysfs_create_groups(&padlak->dev->kobj, adlak_attr_groups)) {
        pr_err("create gropus attribute failed\n");
    }
    return ret;
}

void adlak_destroy_sysfs(void *adlak_device) {
    struct adlak_device *padlak = NULL;
    ASSERT(adlak_device);
    AML_LOG_DEBUG("%s", __func__);
    padlak = (struct adlak_device *)adlak_device;

    device_remove_file(padlak->dev, &dev_attr_kmd_version);
    sysfs_remove_groups(&padlak->dev->kobj, adlak_attr_groups);
}

static ssize_t loglevel_show(struct class *class, struct class_attribute *attr, char *buf) {
    ssize_t len = 0;
    len += sprintf(buf,
                   "Usage:\n"
                   "    echo %d > loglevel;          //set loglevel as LOG_ERR \n"
                   "    echo %d > loglevel;          //set loglevel as LOG_WARN \n"
                   "    echo %d > loglevel;          //set loglevel as LOG_INFO \n"
                   "    echo %d > loglevel;          //set loglevel as LOG_DEBUG \n"
                   "    echo %d > loglevel;          //set loglevel as LOG_DEFAULT \n",
                   LOG_ERR, LOG_WARN, LOG_INFO, LOG_DEBUG, LOG_DEFAULT);
    len += sprintf(buf + len, "\ncurrent loglevel = %d\n", g_adlak_log_level);
    return len;
}

static ssize_t loglevel_store(struct class *class, struct class_attribute *attr, const char *buf,
                              size_t count) {
    int res = 0;
    int ret = 0;
    ret     = kstrtoint(buf, 0, &res);
    if (ret) {
        pr_err("kstrtoint failed\n");
        return -1;
    }
    pr_info("log_level: %d->%d\n", g_adlak_log_level, res);
    g_adlak_log_level = res;
#if ADLAK_DEBUG
    g_adlak_log_level_pre = g_adlak_log_level;
#endif
    return count;
}

static CLASS_ATTR_RW(loglevel);

int adlak_create_class_file(struct class *adlak_class) {
    int ret = 0;
    ASSERT(adlak_class);
    AML_LOG_DEBUG("%s", __func__);

    ret = class_create_file(adlak_class, &class_attr_loglevel);
    if (ret) {
        pr_err("create class attribute %s failed\n", class_attr_loglevel.attr.name);
    }

    return ret;
}

void adlak_destroy_class_file(struct class *adlak_class) {
    ASSERT(adlak_class);
    AML_LOG_DEBUG("%s", __func__);

    class_remove_file(adlak_class, &class_attr_loglevel);
}
