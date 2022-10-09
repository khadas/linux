#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/clk.h>
#include <linux/pinctrl/consumer.h>
#include <linux/amlogic/aml_gpio_consumer.h>
#include <linux/of_gpio.h>
#include "system_am_adap.h"

#define IR_CUT_GPIO_MAX_NUM 5   //config platform ir cut gpio max numbers

typedef struct sensor_bringup {
    struct device *dev;
    struct device_node *np;
    int vana[4];
    int vdig;
    int power;
    int pwren;
    int reset[4];
    struct clk *mclk[2];
    int ir_gname[IR_CUT_GPIO_MAX_NUM];  //there is a one-to-one correspondence between ir_gname[5] and ir_cut_gpio of g12b_a311d_skt.dts.
    int ir_gcount;
}sensor_bringup_t;

typedef struct _sensor_context_t {
    uint8_t address; // Sensor address for direct write (not used currently)
    uint8_t seq_width;
    uint8_t streaming_flg;

    uint8_t gain_cnt;
    uint8_t int_cnt;

    uint16_t again[4];
    uint16_t dgain[4];
    uint32_t again_limit;
    uint32_t dgain_limit;

    uint16_t int_time_S;
    uint16_t int_time_L;
    uint16_t max_S;
    uint16_t max_L;

    uint8_t again_delay;
    uint8_t dgain_delay;

    uint8_t again_change;
    uint8_t dgain_change;
    uint8_t integrate_change;

    uint8_t s_fps;
    uint32_t vmax;
    uint32_t pixel_clock;
    uint16_t frame;
    uint32_t wdr_mode;
    uint32_t vmax_adjust;
    uint32_t vmax_fps;
    acamera_sbus_t sbus;
    sensor_param_t param;
    exp_offset_t win_offset;
    void *sbp;
    void *sdrv;
    int dcam_mode;
    uint8_t cam_isp_path;
    uint8_t cam_fe_path;
} sensor_context_t;

int sensor_bp_init(sensor_bringup_t* sbp, struct device* dev);
int pwr_am_enable(sensor_bringup_t* sensor_bp, const char* propname, int idx, int val);
int pwr_am_disable(sensor_bringup_t* sensor_bp, int idx);
int pwren_am_enable(sensor_bringup_t* sensor_bp, const char* propname, int val);
int pwren_am_disable(sensor_bringup_t* sensor_bp);
int pwr_ir_cut_enable(sensor_bringup_t* sensor_bp, int propname, int val);
int reset_am_enable(sensor_bringup_t* sensor_bp, const char* propname, int idx, int val);
int reset_am_disable(sensor_bringup_t *sensor_bp,int idx);
int clk_am_enable(sensor_bringup_t* sensor_bp, const char* propname);
int gp_pl_am_enable(sensor_bringup_t* sensor_bp, const char* propname, uint32_t rate);
int gp_pl_am_disable(sensor_bringup_t* sensor_bp, const char* propname);
int clk_am_disable(sensor_bringup_t *sensor_bp);
void sensor_set_iface(sensor_mode_t *mode, exp_offset_t offset, sensor_context_t *p_ctx);
void sensor_iface_disable(sensor_context_t *p_ctx);
void sensor_set_iface1(sensor_mode_t *mode, exp_offset_t offset, sensor_context_t *p_ctx);
void sensor_iface1_disable(sensor_context_t *p_ctx);
void sensor_set_iface2(sensor_mode_t *mode, exp_offset_t offset, sensor_context_t *p_ctx);
void sensor_iface2_disable(sensor_context_t *p_ctx);
void sensor_set_iface3(sensor_mode_t *mode, exp_offset_t offset, sensor_context_t *p_ctx);
void sensor_iface3_disable(sensor_context_t *p_ctx);

