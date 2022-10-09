#include <linux/irqreturn.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/ioport.h>
#include <linux/of_platform.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/dma-contiguous.h>
#include <linux/of_reserved_mem.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/of_fdt.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include "system_am_flicker.h"
#include "nr_fw.h"

#define AM_FLICKER_NAME        "amlogic, isp-flicker"

static struct am_flicker *g_flicker;
extern resource_size_t paddr_flicker;

static struct flicker_pram param = {
    4,//reg_ctrl0;
    4,//reg_ctrl1;
    201326608,//reg_ctrl2;
    4,//reg_num_cmp;
    20,//reg_bit_depth;
    0,//reg_gclk;
    0,//reg_insel;
    0,//reg_bypass;
    1,//reg_raw_mode;
    0,//reg_wdr_inp_chn;
    0,//reg_soft_rst;
    1,//reg_flkr_stat_en;
    0,//reg_lXSizeIn;
    0,//reg_lYSizeIn;
    4,//reg_axi_bsize;
    2,//reg_wr_burst_lens;
    4,//reg_wr_req_th;
    0,//reg_wr_awid;
    0,//reg_wr_awport;
    0,//reg_axi_reserved;
    0,//reg_wr_urgent_ctrl;
    0,//reg_axi_mode;
    0,//reg_axi_buff_rst;
    4096*2,//reg_axi_fbuf;
    4096*2,//reg_axi_fsize;
    0,//reg_axi_addr;
    0,//reg_axi_addrp;
    1,//reg_flkr_xphs_ofst;
    0,//reg_flkr_yphs_ofst;
    0,//reg_flkr_stat_yst;
    0,//reg_flkr_stat_yed;
    0,//reg_flkr_stat_xst;
    0,//reg_flkr_stat_xed;
    2592,//reg_flkr_stat_div_coef;
    64,//reg_flkr_wdr_ratio;
    31,//reg_flkr_avg_chnen;
    1,//reg_flkr_sta_input_format;
    0,//reg_flkr_sta_cmpsel;
    0,//reg_flkr_binning_rs;
    1,//reg_flkr_ro_mode;
    0,//ro_flkr_ro0;
    0,//ro_flkr_ro1;
    0,//ro_flkr_ro2;
    0//ro_flkr_ro3;
};

int am_flicker_parse_dt(struct device_node *node)
{
    int rtn = -1;
#if 1
    int irq = -1;
#endif
    struct resource rs;
    struct am_flicker *t_flicker = NULL;

    if (node == NULL) {
        pr_err("%s: Error input param\n", __func__);
        return -1;
    }

    rtn = of_device_is_compatible(node, AM_FLICKER_NAME);
    if (rtn == 0) {
        pr_err("%s: Error match compatible\n", __func__);
        return -1;
    }

    t_flicker = kzalloc(sizeof(*t_flicker), GFP_KERNEL);
    if (t_flicker == NULL) {
        pr_err("%s: Failed to alloc isp-flicker\n", __func__);
        return -1;
    }

    t_flicker->of_node = node;

    rtn = of_address_to_resource(node, 0, &rs);
    if (rtn != 0) {
        pr_err("%s:Error get isp-flicker reg resource\n", __func__);
        goto reg_error;
    }

    irq = irq_of_parse_and_map(node, 0);
    if (irq <= 0) {
        pr_err("%s:Error get flicker irq\n", __func__);
    }
    t_flicker->irq = irq;
    pr_info("%s:t_flicker info: irq: %d, ds%d\n", __func__, t_flicker->irq, 0);

    t_flicker->reg = rs;
    t_flicker->base_addr = ioremap_nocache(t_flicker->reg.start, resource_size(&t_flicker->reg));

    t_flicker->p_dev = of_find_device_by_node(node);

    g_flicker = t_flicker;

    return 0;

#if 0
irq_error:
    iounmap(t_flicker->base_addr);
    t_flicker->base_addr = NULL;
#endif

reg_error:
    if (t_flicker != NULL)
        kfree(t_flicker);

    return -1;
}

static inline void update_wr_bits(
    unsigned int reg,
    unsigned int mask,
    unsigned int val)
{
    unsigned int tmp, orig;
    void __iomem *base = g_flicker->base_addr;

    if (base !=  NULL) {
        orig = readl(base + reg);
        tmp = orig & ~mask;
        tmp |= val & mask;
        writel(tmp, base + reg);
    }
}

static inline void flicker_wr_bits(
    unsigned int adr, unsigned int val,
    unsigned int start, unsigned int len)
{
    update_wr_bits(adr,
        ((1 << len) - 1) << start, val << start);
}

static inline void flicker_wr(
    int addr, uint32_t val)
{
    void __iomem *base = g_flicker->base_addr;

    if (base != NULL) {
        base = base + addr;
        writel(val, base);
    } else
        pr_err("isp-flicker write register failed.\n");

}

static inline void flicker_rd(
    int addr, uint32_t *val)
{
    void __iomem *base = g_flicker->base_addr;

    if (base != NULL && val) {
        base = base + addr;
        *val = readl(base);
    } else
        pr_err("isp-flicker read register failed.\n");

}

int flicker_cfg(struct flicker_pram *pram)
{
    //int k;
    //int i,j;
    //double a;
    //double temp0,temp1,temp;
    //int temp2;
    //int lcge_ratio[4] = {10, 16, 24, 32};
    //int lcge_luma_scal[9] = { 8, 8, 9, 10, 11, 12, 13, 14, 14};

//FLICKER REGISTERS START
    int flkr_winw = 0;//,flkr_wdr_ratio = 0;
    flicker_wr(ISP_DEFLICKER_CTRL0,
        pram->reg_ctrl0 );  //u32, latch control
    flicker_wr(ISP_DEFLICKER_CTRL1,
        pram->reg_ctrl1 );   //u32, latch control
    flicker_wr(ISP_DEFLICKER_CTRL2,
        pram->reg_ctrl2);    //u32, latch control
    flicker_wr(ISP_DEFLICKER_COMMON,
        (pram->reg_num_cmp      <<28)      |   //u4, number cmp
        (pram->reg_bit_depth    <<20)      |   //u8, bit_depth
        (pram->reg_gclk         <<12)      |   //u8, clk gate
        (pram->reg_insel        << 8)      |   //u4, input data channel select
        (pram->reg_bypass       << 7)      |
        (pram->reg_raw_mode     << 4)      |   //u3,0:mono,1:GR,2:IR,3:GR,4:GRGB,5~7:other
        (pram->reg_wdr_inp_chn  << 2)      |   //u2, the input channels for wdr stitch, support (1+inp_chn) as 1/2/3/4 images as wdr stitch's input;
        (pram->reg_soft_rst     << 1)      |   //u1, soft_rst
        (pram->reg_flkr_stat_en ));            //u1, enable row statistic for flicker detection

    flicker_wr(ISP_DEFLICKER_COMMON_SIZE,
        (  pram->reg_lXSizeIn  <<16)       |   //u14, horizontal size
        (  pram->reg_lYSizeIn  ));//u14, vertical size

    flicker_wr(ISP_DEFLICKER_AXI_CTRL,
        (pram->reg_axi_bsize        << 24) |   //u8, each framesize buffer_size
        (pram->reg_wr_burst_lens    << 22) |   //u2, 0: 1x128, 1: 2x128, 2‾3: 4x128
        (pram->reg_wr_req_th        << 18) |   //u4,  fifo depth req_th * 8 *128 bits in fifo.
        (pram->reg_wr_awid          << 10) |   //u8,  wid
        (pram->reg_wr_awport        <<  7)     //u3, urgent control
        //(pram->reg_axi_reserved   << )     //u7, reserved
    );

    flicker_wr(ISP_DEFLICKER_AXI_BUFFSIZE,
        (pram->reg_axi_mode     <<  30  )|     //u2, 0:always axi_addr, 1: axi_addr-axi_addrp pingpong, 2~3:buffer_mode, start with axi_addr
        (pram->reg_axi_buff_rst <<  29  )|     //u1, frame buffer reset
        (pram->reg_axi_fbuf) );          //u29, axi_buffer size

    flicker_wr(ISP_DEFLICKER_AXI_FRAMESIZE,    //u3, reserved
        pram->reg_axi_fsize );           //u29, axi_fsize

    flicker_wr(ISP_DEFLICKER_AXI_ADDR,
        pram->reg_axi_addr);             //u32, axi_addr

    flicker_wr(ISP_DEFLICKER_AXI_ADDRP,
        pram->reg_axi_addrp);            //u32, axi_addrp

    flicker_wr(ISP_DEFLICKER_STAT_YPOSITION,   //u2
        (pram->reg_flkr_stat_yst <<16)|  //u14, row start for row statistic for flicker statistic
        pram->reg_flkr_stat_yed);        //u14, row end for row statistic for flicker statistic

    flicker_wr(ISP_DEFLICKER_STAT_XPOSITION,
        (pram->reg_flkr_stat_xst <<16)|  //u14, column start for flicker statistic
        pram->reg_flkr_stat_xed);        //u14, column end for flicker statistic

    flkr_winw = pram->reg_flkr_stat_xed - pram->reg_flkr_stat_xst;
    pram->reg_flkr_stat_div_coef = MIN((1<<16)-1 , (1<<(16+6)) / flkr_winw);
    flicker_wr(ISP_DEFLICKER_DIV_COEF,pram->reg_flkr_stat_div_coef);

    //flicker_wr(ISP_DEFLICKER_CNTL, 0x0007c201);
    return 0;
}

int aml_flicker_init(void){
    if (!g_flicker) {
        pr_info("%d, g_flicker is NULL.\n", __LINE__);
        return -1;
    }
    param.reg_axi_mode = 1;
    param.reg_insel = 6;
    param.reg_ctrl2 = 0x1388;//0x8001388;//0x8000010;//vvalid p used as vsync
    pr_err("flicker init success.\n");
    return 0;
}

int aml_flicker_deinit(void)
{
    if (!g_flicker) {
        pr_info("%d, g_flicker is NULL.\n", __LINE__);
        return -1;
    }

    iounmap(g_flicker->base_addr);
    g_flicker->base_addr = NULL;

    kfree(g_flicker);
    g_flicker = NULL;
    return 0;
}

int aml_flicker_start(int xsize, int ysize){
    if (!g_flicker) {
        pr_info("%d, g_flicker is NULL.\n", __LINE__);
        return -1;
    }

    param.reg_lXSizeIn  = xsize;
    param.reg_lYSizeIn  = ysize;
    param.reg_flkr_stat_yst  = 0;
    param.reg_flkr_stat_yed  = ysize-1;
    param.reg_flkr_stat_xst  = 0;
    param.reg_flkr_stat_xed  = xsize-1;
    param.reg_axi_addr  = (unsigned int)paddr_flicker;
    param.reg_axi_addrp  = (unsigned int)(paddr_flicker + 0x2000);
    flicker_cfg(&param);
    flicker_wr_bits(ISP_DEFLICKER_COMMON, 1, 0, 1);

/* not use flicker irq
    ret = request_irq(g_flicker->irq, aml_flicker_isr, IRQF_SHARED | IRQF_TRIGGER_RISING,
        "isp_flicker", (void *)g_flicker);
    pr_err("flicker irq:%d ret:%d\n",g_flicker->irq,ret);
*/
    pr_err("flicker start ok.\n");
    return 0;
}

int aml_flicker_stop(void){
    flicker_wr_bits(ISP_DEFLICKER_COMMON, 0, 0, 1);
    return 0;
}

extern void cache_flush(uint32_t buf_start, uint32_t buf_size);

void aml_flicker_get_data(uint32_t* dst){
    static int get_stats_counter = 0;
    //uint32_t data0 = 0, data1 = 0,data2 = 0,data3 = 0;
    get_stats_counter++;
    if (!paddr_flicker) {
        pr_info("%d, paddr_flicker is NULL\n", __LINE__);
        return;
    }
    /*
    flicker_rd(ISP_DEFLICKER_STAT_INFO0, &data0);
    flicker_rd(ISP_DEFLICKER_STAT_INFO1, &data1);
    flicker_rd(ISP_DEFLICKER_STAT_INFO2, &data2);
    flicker_rd(ISP_DEFLICKER_STAT_INFO3, &data3);
    */
    uint32_t data = 0;
    flicker_rd(ISP_DEFLICKER_STAT_INFO4, &data);

    //pr_err("GETDATA ISP_DEFLICKER_STAT_INFO0~4 0:%x,1:%x,2:%x,3:%x 4:%x get_stats_counter: %d\n",data0,data1,data2,data3,data,get_stats_counter);
    data = data & 0x1;
    char* buf;
    //pr_err("flkr get data ok.pp: %d, %d\n", data, param.reg_axi_fsize);
    if (data == 0)
        buf = phys_to_virt(paddr_flicker + 0x2000);
    else
        buf = phys_to_virt(paddr_flicker);

    //memcpy((void *)dst, buf, param.reg_axi_fsize);
    memcpy((void *)dst, buf, FED_FLKR_STAT_MAX*2);
}
