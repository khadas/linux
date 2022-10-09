#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/clk.h>
#include <linux/pinctrl/consumer.h>
#include <linux/amlogic/aml_gpio_consumer.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include "acamera_command_api.h"
#include "acamera_sensor_api.h"
#include "system_am_mipi.h"
#include "sensor_bsp_common.h"

int sensor_bp_init(sensor_bringup_t* sbp, struct device* dev)
{
    sbp->dev = dev;
    sbp->np = dev->of_node;
    sbp->vana[0] = -1;
    sbp->vana[1] = -1;
    sbp->vana[2] = -1;
    sbp->vana[3] = -1;
    sbp->vdig = 0;
    sbp->power = 0;
    sbp->reset[0] = -1;
    sbp->reset[1] = -1;
    sbp->reset[2] = -1;
    sbp->reset[3] = -1;
    sbp->mclk[0] = NULL;
    sbp->mclk[1] = NULL;

    pr_info("sensor bsp init\n");

    return 0;
}

int pwr_am_enable(sensor_bringup_t* sensor_bp, const char* propname, int idx, int val)
{
    struct device_node *np = NULL;
    int ret = -1;

    np = sensor_bp->np;
    sensor_bp->vana[idx] = of_get_named_gpio(np, propname, 0);
    ret = sensor_bp->vana[idx];

    if (ret >= 0) {
        devm_gpio_request(sensor_bp->dev, sensor_bp->vana[idx], "POWER");
        if (gpio_is_valid(sensor_bp->vana[idx])) {
            gpio_direction_output(sensor_bp->vana[idx], val);
        } else {
            pr_err("pwr_enable[%d]: gpio %s is not valid\n", idx, propname);
            return -1;
        }
    } else {
        pr_err("pwr_enable[%d]: get_named_gpio %s fail\n", idx, propname);
    }

    return ret;
}

int pwr_am_disable(sensor_bringup_t *sensor_bp, int idx)
{
    if (gpio_is_valid(sensor_bp->vana[idx])) {
        gpio_direction_output(sensor_bp->vana[idx], 0);
        devm_gpio_free(sensor_bp->dev, sensor_bp->vana[idx]);
    } else {
        pr_err("Error invalid pwr gpio, idx [%d]\n", idx);
    }

    return 0;
}

int pwr_ir_cut_enable(sensor_bringup_t* sensor_bp, int propname, int val)
{
    int ret = -1;
    ret = propname;

    if (ret >= 0) {
        devm_gpio_request(sensor_bp->dev, propname, "POWER");
        if (gpio_is_valid(propname)) {
            gpio_direction_output(propname, val);
        } else {
            pr_err("pwr_enable: gpio %d is not valid\n", propname);
            return -1;
        }
    } else {
        pr_err("pwr_enable: get_named_gpio %d fail\n", propname);
    }
    return ret;
}

int reset_am_enable(sensor_bringup_t* sensor_bp, const char* propname, int idx, int val)
{
    struct device_node *np = NULL;
    int ret = -1;

    np = sensor_bp->np;
    sensor_bp->reset[idx] = of_get_named_gpio(np, propname, 0);
    ret = sensor_bp->reset[idx];

    if (ret >= 0) {
        devm_gpio_request(sensor_bp->dev, sensor_bp->reset[idx], "RESET");
        if (gpio_is_valid(sensor_bp->reset[idx])) {
            gpio_direction_output(sensor_bp->reset[idx], val);
        } else {
            pr_err("reset_enable[%d]: gpio %s is not valid\n", idx, propname);
            return -1;
        }
    } else {
        pr_err("reset_enable[%d]: get_named_gpio %s fail\n", idx, propname);
    }

    return ret;
}

int reset_am_disable(sensor_bringup_t* sensor_bp, int idx)
{
    if (gpio_is_valid(sensor_bp->reset[idx])) {
        gpio_direction_output(sensor_bp->reset[idx], 0);
        devm_gpio_free(sensor_bp->dev, sensor_bp->reset[idx]);
    } else {
        pr_err("Error invalid reset gpio, idx [%d]\n", idx);
    }

    return 0;
}

int pwren_am_enable(sensor_bringup_t* sensor_bp, const char* propname, int val)
{
    struct device_node *np = NULL;
    int ret = -1;

    np = sensor_bp->np;
    sensor_bp->pwren = of_get_named_gpio(np, propname, 0);
    ret = sensor_bp->pwren;

    if (ret >= 0) {
        devm_gpio_request(sensor_bp->dev, sensor_bp->pwren, "PWREN");
        if (gpio_is_valid(sensor_bp->pwren)) {
            gpio_direction_output(sensor_bp->pwren, val);
        } else {
            pr_err("power_enable: gpio %s is not valid\n", propname);
            return -1;
        }
    } else {
        pr_err("power_enable: get_named_gpio %s fail\n", propname);
    }

    return ret;
}

int pwren_am_disable(sensor_bringup_t* sensor_bp)
{
    if (gpio_is_valid(sensor_bp->pwren)) {
        gpio_direction_output(sensor_bp->pwren, 0);
        devm_gpio_free(sensor_bp->dev, sensor_bp->pwren);
    } else {
        pr_err("Error invalid pwren gpio\n");
    }

    return 0;
}
int clk_am_enable(sensor_bringup_t* sensor_bp, const char* propname)
{
    struct clk *clk;
    int clk_val;
    clk = devm_clk_get(sensor_bp->dev, propname);
    if (IS_ERR(clk)) {
        pr_err("cannot get %s clk\n", propname);
        clk = NULL;
        return -1;
    }

    clk_prepare_enable(clk);
    clk_val = clk_get_rate(clk);
    pr_info("init mclock is %d MHZ\n",clk_val/1000000);

    sensor_bp->mclk[0] = clk;
    return 0;
}

int clk_am_disable(sensor_bringup_t *sensor_bp)
{
    struct clk *mclk = NULL;

    if (sensor_bp == NULL || sensor_bp->mclk[0] == NULL) {
        pr_err("Error input param\n");
        return -EINVAL;
    }

    mclk = sensor_bp->mclk[0];

    clk_disable_unprepare(mclk);

    devm_clk_put(sensor_bp->dev, mclk);

    pr_info("Success disable mclk\n");

    return 0;
}

int gp_pl_am_enable(sensor_bringup_t* sensor_bp, const char* propname, uint32_t rate)
{
    int ret;
    struct clk *clk = NULL,*clk0_pre = NULL,*clk1_pre = NULL,*clk_p = NULL,*clk_x = NULL;
    int clk_val;
    clk = devm_clk_get(sensor_bp->dev, propname);
    if (IS_ERR(clk)) {
        pr_err("cannot get %s clk\n", propname);
        clk = NULL;
        return -1;
    }

    if (rate == 24000000) {
        clk_x = devm_clk_get(sensor_bp->dev, "mclk_x");
        if (IS_ERR(clk_x) == 0)
            clk_set_parent(clk, clk_x);

        if (strcmp(propname, "mclk_0") == 0)
            clk0_pre = devm_clk_get(sensor_bp->dev, "mclk_0_pre");
        else
            clk1_pre = devm_clk_get(sensor_bp->dev, "mclk_1_pre");
    } else {
        clk_p = devm_clk_get(sensor_bp->dev, "mclk_p");
        if (IS_ERR(clk_p) == 0) {
            clk_set_parent(clk, clk_p);
            clk_set_rate(clk_p, rate * 2);
        }
    }

    ret = clk_set_rate(clk, rate);
    if (ret < 0)
        pr_err("clk_set_rate failed\n");
    udelay(30);

    ret = clk_prepare_enable(clk);
    if (ret < 0)
        pr_err(" clk_prepare_enable failed\n");

    if ((strcmp(propname, "mclk_0") == 0) && (sensor_bp->mclk[0] == NULL)) {
        if (IS_ERR(clk0_pre) == 0) {
            clk_disable_unprepare(clk0_pre);
            sensor_bp->mclk[0] = clk;
        }
    }

    if ((strcmp(propname, "mclk_1") == 0) && (sensor_bp->mclk[1] == NULL)) {
        if (IS_ERR(clk1_pre) == 0) {
            clk_disable_unprepare(clk1_pre);
            sensor_bp->mclk[1] = clk;
        }
    }

    clk_val = clk_get_rate(clk);
    pr_err("init mclk is %d MHZ\n",clk_val/1000000);

    return 0;
}

int gp_pl_am_disable(sensor_bringup_t* sensor_bp, const char* propname)
{
    struct clk *clk;
    int clk_val;

    if (sensor_bp == NULL) {
        pr_err("Error input param\n");
        return -EINVAL;
    }

    clk = devm_clk_get(sensor_bp->dev, propname);
    if (IS_ERR(clk)) {
        pr_err("cannot get %s clk\n", propname);
        clk = NULL;
        return -1;
    }

    clk_val = clk_get_rate(clk);
    if ((clk_val != 12000000) && (clk_val != 24000000)) {
        clk_disable_unprepare(clk);
    }

    devm_clk_put(sensor_bp->dev, clk);

    pr_info("Success disable mclk: %d\n", clk_val);

    return 0;
}

void sensor_set_iface(sensor_mode_t *mode, exp_offset_t offset, sensor_context_t *p_ctx)
{
    am_mipi_info_t mipi_info;
    struct am_adap_info info;

    if (mode == NULL) {
        pr_info( "Error input param\n");
        return;
    }

    memset(&mipi_info, 0, sizeof(mipi_info));
    memset(&info, 0, sizeof(struct am_adap_info));
    mipi_info.lanes = mode->lanes;
    mipi_info.ui_val = 1000 / mode->bps;

    if ((1000 % mode->bps) != 0)
        mipi_info.ui_val += 1;

    am_mipi_init(&mipi_info);

    switch (mode->bits) {
    case 8:
        info.fmt = MIPI_CSI_RAW8;
        break;
    case 10:
        info.fmt = MIPI_CSI_RAW10;
        break;
    case 12:
        info.fmt = MIPI_CSI_RAW12;
        break;
    default:
        info.fmt = MIPI_CSI_RAW10;
        break;
    }

    if ( mode->dol_type == DOL_YUV ) {
        if ( mode->bits == 8 )
            info.fmt = MIPI_CSI_YUV422_8BIT;
        else
            info.fmt = MIPI_CSI_YUV422_10BIT;
    }
    //p_ctx->dcam_mode = 1;

    info.path = p_ctx->cam_isp_path;
    info.frontend = p_ctx->cam_fe_path;
    info.img.width = mode->resolution.width;
    info.img.height = mode->resolution.height;
    info.offset.offset_x = offset.offset_x;
    info.offset.offset_y = offset.offset_y;
    if (mode->wdr_mode == WDR_MODE_FS_LIN) {
        info.mode = DOL_MODE;
        if (p_ctx->dcam_mode)
            info.mode = DCAM_DOL_MODE;
        info.type = mode->dol_type;
        if (info.type == DOL_LINEINFO) {
           info.offset.long_offset = offset.long_offset;
           info.offset.short_offset = offset.short_offset;
        }
    } else {
        info.type = mode->dol_type;
        if (p_ctx->dcam_mode)
            info.mode = DCAM_MODE;
        else
            info.mode = DIR_MODE;
    }
    uint32_t isp_clk_rate = 0;
    camera_notify(NOTIFY_GET_ISP_CLKRATE, &isp_clk_rate);
    isp_clk_rate = (isp_clk_rate / 10) * 9;
    info.align_width = isp_clk_rate / ((mode->resolution.height + 64) * (mode->fps / 256) * 2);
    if (info.align_width < (mode->resolution.width + 64))
        info.align_width = mode->resolution.width + 64;
    pr_info("Dcam%x:%d, aligh:%d\n",p_ctx->cam_isp_path, p_ctx->dcam_mode, info.align_width);

    am_adap_set_info(&info);
    am_adap_init(p_ctx->cam_isp_path);
    am_adap_start(p_ctx->cam_isp_path, p_ctx->dcam_mode);
}

void sensor_iface_disable(sensor_context_t *p_ctx)
{
    am_adap_deinit(p_ctx->cam_isp_path);
    am_mipi_deinit();
}

void sensor_set_iface1(sensor_mode_t *mode, exp_offset_t offset, sensor_context_t *p_ctx)
{
    am_mipi_info_t mipi_info;
    struct am_adap_info info;

    if (mode == NULL) {
        pr_info( "Error input param\n");
        return;
    }

    memset(&mipi_info, 0, sizeof(mipi_info));
    memset(&info, 0, sizeof(struct am_adap_info));
    mipi_info.lanes = mode->lanes;
    mipi_info.ui_val = 1000 / mode->bps;

    if ((1000 % mode->bps) != 0)
        mipi_info.ui_val += 1;

    am_mipi1_init(&mipi_info);

    switch (mode->bits) {
    case 8:
        info.fmt = MIPI_CSI_RAW8;
        break;
    case 10:
        info.fmt = MIPI_CSI_RAW10;
        break;
    case 12:
        info.fmt = MIPI_CSI_RAW12;
        break;
    default:
        info.fmt = MIPI_CSI_RAW10;
        break;
    }

    if ( mode->dol_type == DOL_YUV ) {
        if ( mode->bits == 8 )
            info.fmt = MIPI_CSI_YUV422_8BIT;
        else
            info.fmt = MIPI_CSI_YUV422_10BIT;
    }
    //p_ctx->dcam_mode = 1;

    info.path = p_ctx->cam_isp_path;
    info.frontend = p_ctx->cam_fe_path;
    info.img.width = mode->resolution.width;
    info.img.height = mode->resolution.height;
    info.offset.offset_x = offset.offset_x;
    info.offset.offset_y = offset.offset_y;
    if (mode->wdr_mode == WDR_MODE_FS_LIN) {
        info.mode = DOL_MODE;
        if (p_ctx->dcam_mode)
            info.mode = DCAM_DOL_MODE;
        info.type = mode->dol_type;
        if (info.type == DOL_LINEINFO) {
           info.offset.long_offset = offset.long_offset;
           info.offset.short_offset = offset.short_offset;
        }
    } else {
        info.type = mode->dol_type;
        if (p_ctx->dcam_mode)
            info.mode = DCAM_MODE;
        else
            info.mode = DIR_MODE;
    }
    uint32_t isp_clk_rate = 0;
    camera_notify(NOTIFY_GET_ISP_CLKRATE, &isp_clk_rate);
    isp_clk_rate = (isp_clk_rate / 10) * 9;
    info.align_width = isp_clk_rate / ((mode->resolution.height + 64) * (mode->fps / 256) * 2);
    if (info.align_width < (mode->resolution.width + 64))
        info.align_width = mode->resolution.width + 64;
    pr_info("Dcam%x:%d, aligh:%d\n",p_ctx->cam_isp_path, p_ctx->dcam_mode, info.align_width);

    am_adap_set_info(&info);
    am_adap_init(p_ctx->cam_isp_path);
    am_adap_start(p_ctx->cam_isp_path, p_ctx->dcam_mode);
}

void sensor_iface1_disable(sensor_context_t *p_ctx)
{
    am_adap_deinit(p_ctx->cam_isp_path);
    am_mipi1_deinit();
}

void sensor_set_iface2(sensor_mode_t *mode, exp_offset_t offset, sensor_context_t *p_ctx)
{
    am_mipi_info_t mipi_info;
    struct am_adap_info info;

    if (mode == NULL) {
        pr_info( "Error input param\n");
        return;
    }

    memset(&mipi_info, 0, sizeof(mipi_info));
    memset(&info, 0, sizeof(struct am_adap_info));
    mipi_info.lanes = mode->lanes;
    mipi_info.ui_val = 1000 / mode->bps;

    if ((1000 % mode->bps) != 0)
        mipi_info.ui_val += 1;

    am_mipi2_init(&mipi_info);

    switch (mode->bits) {
    case 8:
        info.fmt = MIPI_CSI_RAW8;
        break;
    case 10:
        info.fmt = MIPI_CSI_RAW10;
        break;
    case 12:
        info.fmt = MIPI_CSI_RAW12;
        break;
    default:
        info.fmt = MIPI_CSI_RAW10;
        break;
    }

    if ( mode->dol_type == DOL_YUV ) {
        if ( mode->bits == 8 )
            info.fmt = MIPI_CSI_YUV422_8BIT;
        else
            info.fmt = MIPI_CSI_YUV422_10BIT;
    }
    //p_ctx->dcam_mode = 1;

    info.path = p_ctx->cam_isp_path;
    info.frontend = p_ctx->cam_fe_path;
    info.img.width = mode->resolution.width;
    info.img.height = mode->resolution.height;
    info.offset.offset_x = offset.offset_x;
    info.offset.offset_y = offset.offset_y;
    if (mode->wdr_mode == WDR_MODE_FS_LIN) {
        info.mode = DOL_MODE;
        if (p_ctx->dcam_mode)
            info.mode = DCAM_DOL_MODE;
        info.type = mode->dol_type;
        if (info.type == DOL_LINEINFO) {
           info.offset.long_offset = offset.long_offset;
           info.offset.short_offset = offset.short_offset;
        }
    } else {
        info.type = mode->dol_type;
        if (p_ctx->dcam_mode)
            info.mode = DCAM_MODE;
        else
            info.mode = DIR_MODE;
    }
    uint32_t isp_clk_rate = 0;
    camera_notify(NOTIFY_GET_ISP_CLKRATE, &isp_clk_rate);
    isp_clk_rate = (isp_clk_rate / 10) * 9;
    info.align_width = isp_clk_rate / ((mode->resolution.height + 64) * (mode->fps / 256) * 2);
    if (info.align_width < (mode->resolution.width + 64))
        info.align_width = mode->resolution.width + 64;
    pr_info("Dcam%x:%d, aligh:%d\n",p_ctx->cam_isp_path, p_ctx->dcam_mode, info.align_width);

    am_adap_set_info(&info);
    am_adap_init(p_ctx->cam_isp_path);
    am_adap_start(p_ctx->cam_isp_path, p_ctx->dcam_mode);
}

void sensor_iface2_disable(sensor_context_t *p_ctx)
{
    am_adap_deinit(p_ctx->cam_isp_path);
    am_mipi2_deinit();
}

void sensor_set_iface3(sensor_mode_t *mode, exp_offset_t offset, sensor_context_t *p_ctx)
{
    am_mipi_info_t mipi_info;
    struct am_adap_info info;

    if (mode == NULL) {
        pr_info( "Error input param\n");
        return;
    }

    memset(&mipi_info, 0, sizeof(mipi_info));
    memset(&info, 0, sizeof(struct am_adap_info));
    mipi_info.lanes = mode->lanes;
    mipi_info.ui_val = 1000 / mode->bps;

    if ((1000 % mode->bps) != 0)
        mipi_info.ui_val += 1;

    am_mipi3_init(&mipi_info);

    switch (mode->bits) {
    case 8:
        info.fmt = MIPI_CSI_RAW8;
        break;
    case 10:
        info.fmt = MIPI_CSI_RAW10;
        break;
    case 12:
        info.fmt = MIPI_CSI_RAW12;
        break;
    default:
        info.fmt = MIPI_CSI_RAW10;
        break;
    }

    if ( mode->dol_type == DOL_YUV ) {
        if ( mode->bits == 8 )
            info.fmt = MIPI_CSI_YUV422_8BIT;
        else
            info.fmt = MIPI_CSI_YUV422_10BIT;
    }
    //p_ctx->dcam_mode = 1;

    info.path = p_ctx->cam_isp_path;
    info.frontend = p_ctx->cam_fe_path;
    info.img.width = mode->resolution.width;
    info.img.height = mode->resolution.height;
    info.offset.offset_x = offset.offset_x;
    info.offset.offset_y = offset.offset_y;
    if (mode->wdr_mode == WDR_MODE_FS_LIN) {
        info.mode = DOL_MODE;
        if (p_ctx->dcam_mode)
            info.mode = DCAM_DOL_MODE;
        info.type = mode->dol_type;
        if (info.type == DOL_LINEINFO) {
           info.offset.long_offset = offset.long_offset;
           info.offset.short_offset = offset.short_offset;
        }
    } else {
        info.type = mode->dol_type;
        if (p_ctx->dcam_mode)
            info.mode = DCAM_MODE;
        else
            info.mode = DIR_MODE;
    }
    uint32_t isp_clk_rate = 0;
    camera_notify(NOTIFY_GET_ISP_CLKRATE, &isp_clk_rate);
    isp_clk_rate = (isp_clk_rate / 10) * 9;
    info.align_width = isp_clk_rate / ((mode->resolution.height + 64) * (mode->fps / 256) * 2);
    if (info.align_width < (mode->resolution.width + 64))
        info.align_width = mode->resolution.width + 64;
    pr_info("Dcam%x:%d, aligh:%d\n",p_ctx->cam_isp_path, p_ctx->dcam_mode, info.align_width);

    am_adap_set_info(&info);
    am_adap_init(p_ctx->cam_isp_path);
    am_adap_start(p_ctx->cam_isp_path, p_ctx->dcam_mode);
}

void sensor_iface3_disable(sensor_context_t *p_ctx)
{
    am_adap_deinit(p_ctx->cam_isp_path);
    am_mipi3_deinit();
}

