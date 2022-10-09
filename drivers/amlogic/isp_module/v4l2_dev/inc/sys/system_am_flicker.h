#include <linux/module.h>
#include <linux/of.h>
#include <linux/slab.h>
#include "acamera_command_api.h"
#include "acamera_firmware_settings.h"
#include "acamera.h"
#include "acamera_fw.h"
#include <linux/ioport.h>

//========================================================================
// MIPI_ISP  fliker
//========================================================================
// -----------------------------------------------
// REG_BASE:  REGISTER_BASE_ADDR = 0xfe3b2c00
// -----------------------------------------------
// synopsys translate_off
// synopsys translate_on

#define ISP_FLICKER_SIZE 2560//4096

#define ISP_DEFLICKER_CTRL0                        (0x0000  << 2)
//Bit 31: 0        reg_ctrl0                 // unsigned ,    RW, default = 4  latch control
#define ISP_DEFLICKER_CTRL1                        (0x0001  << 2)
//Bit 31: 0        reg_ctrl1                 // unsigned ,    RW, default = 4  latch control
#define ISP_DEFLICKER_CTRL2                        (0x0002  << 2)
//Bit 31: 0        reg_ctrl2                 // unsigned ,    RW, default = 201326608  latch control
#define ISP_DEFLICKER_COMMON                       (0x0003  << 2)
//Bit 31:28        reg_num_cmp               // unsigned ,    RW, default = 4  number cmp
//Bit 27:20        reg_bit_depth             // unsigned ,    RW, default = 20  bit_depth
//Bit 19:12        reg_gclk                  // unsigned ,    RW, default = 0  clk gate
//Bit 11: 8        reg_insel                 // unsigned ,    RW, default = 0  input data channel select
//Bit  7           reg_bypass                // unsigned ,    RW, default = 0  axi wr bypass
//Bit  6: 4        reg_raw_mode              // unsigned ,    RW, default = 1  0: mono, 1:G R  ,2 :   I R     ,3:  G R   ,4:  G R G B, 5~7: other cases
//Bit  3: 2        reg_wdr_inp_chn           // unsigned ,    RW, default = 0  the input channels for wdr stitch, support (1+inp_chn) as 1/2/3/4 images as wdr stitch's input;
//Bit  1           reg_soft_rst              // unsigned ,    RW, default = 0  soft_rst
//Bit  0           reg_flkr_stat_en          // unsigned ,    RW, default = 0  enable row statistic for flicker detection
#define ISP_DEFLICKER_COMMON_SIZE                  (0x0004  << 2)
//Bit 31:30        reserved
//Bit 29:16        reg_lXSizeIn              // unsigned ,    RW, default = 4096  horizontal size
//Bit 15:14        reserved
//Bit 13: 0        reg_lYSizeIn              // unsigned ,    RW, default = 2160  vertical size
#define ISP_DEFLICKER_AXI_CTRL                     (0x0005  << 2)
//Bit 31:24        reg_axi_bsize             // unsigned ,    RW, default = 4  each framesize buffer_size
//Bit 23:22        reg_wr_burst_lens         // unsigned ,    RW, default = 2  busrt lens reg_wr_burst_lens   0: 1x128, 1: 2x128, 2‾3: 4x128 
//Bit 21:18        reg_wr_req_th             // unsigned ,    RW, default = 4  fifo depth req_th * 8 *128 bits in fifo.             
//Bit 17:10        reg_wr_awid               // unsigned ,    RW, default = 0  wid            
//Bit  9: 7        reg_wr_awport             // unsigned ,    RW, default = 0  urgent control
//Bit  6: 0        reg_axi_reserved          // unsigned ,    RW, default = 0  reserved
#define ISP_DEFLICKER_AXI_CTRL0                    (0x0006  << 2)
//Bit 31: 0        reg_wr_urgent_ctrl        // unsigned ,    RW, default = 0  urgent control
#define ISP_DEFLICKER_AXI_BUFFSIZE                 (0x0007  << 2)
//Bit 31:30        reg_axi_mode              // unsigned ,    RW, default = 0  0:always axi_addr, 1: axi_addr-axi_addrp pingpong, 2~3:buffer_mode, start with axi_addr
//Bit 29           reg_axi_buff_rst          // unsigned ,    RW, default = 4  frame buffer reset
//Bit 28: 0        reg_axi_fbuf              // unsigned ,    RW, default = 4096  axi_buffer size
#define ISP_DEFLICKER_AXI_FRAMESIZE                (0x0008  << 2)
//Bit 31:29        reserved
//Bit 28: 0        reg_axi_fsize             // unsigned ,    RW, default = 4096  axi_fsize
#define ISP_DEFLICKER_AXI_ADDR                     (0x0009  << 2)
//Bit 31: 0        reg_axi_addr              // unsigned ,    RW, default = 268435456  axi_addr
#define ISP_DEFLICKER_AXI_ADDRP                    (0x000a  << 2)
//Bit 31: 0        reg_axi_addrp             // unsigned ,    RW, default = 268435456  axi_addrp
#define ISP_DEFLICKER_OFST                         (0x000b  << 2)
//Bit 31: 6        reserved
//Bit  5: 4        reg_flkr_xphs_ofst        // unsigned ,    RW, default = 1  horizontal phase offset
//Bit  3: 2        reserved
//Bit  1: 0        reg_flkr_yphs_ofst        // unsigned ,    RW, default = 0  vertical phase offset
#define ISP_DEFLICKER_STAT_YPOSITION               (0x000c  << 2)
//Bit 31:30        reserved
//Bit 29:16        reg_flkr_stat_yst         // unsigned ,    RW, default = 0  row start for row statistic for flicker statistic
//Bit 15:14        reserved
//Bit 13: 0        reg_flkr_stat_yed         // unsigned ,    RW, default = 2159  row end for row statistic for flicker statistic
#define ISP_DEFLICKER_STAT_XPOSITION               (0x000d  << 2)
//Bit 31:30        reserved
//Bit 29:16        reg_flkr_stat_xst         // unsigned ,    RW, default = 0  column start for flicker statistic
//Bit 15:14        reserved
//Bit 13: 0        reg_flkr_stat_xed         // unsigned ,    RW, default = 4095  column end for flicker statistic
#define ISP_DEFLICKER_DIV_COEF                     (0x000e  << 2)
//Bit 31:16        reserved
//Bit 15: 0        reg_flkr_stat_div_coef    // unsigned ,    RW, default = 1920  divide coefficent for sum diff for row statistic for flicker detection
#define ISP_DEFLICKER_WDR_RATIO                    (0x000f  << 2)
//Bit 31:15        reserved
//Bit 14: 0        reg_flkr_wdr_ratio        // unsigned ,    RW, default = 64  exposure ratio between long and short1 for wdr mode. T7 wdr mode short frame input=long_expo/short_expo, else=1. default 64
#define ISP_DEFLICKER_CNTL                         (0x0010  << 2)
//Bit 31:19        reserved
//Bit 18:14        reg_flkr_avg_chnen        // unsigned ,    RW, default = 31  
//Bit 13            reserved
//Bit 12           reg_flkr_sta_input_format // unsigned ,    RW, default = 1  STA input select ISP output, 0: raw input 1: YUV/RGB
//Bit 11:10        reserved
//Bit  9: 8        reg_flkr_sta_cmpsel       // unsigned ,    RW, default = 2  component combine to Y when reg_flkr_sta_input_format=1. 0: dat[0],Y, 1:dat[3], IR; 2/3: (dat[0]+ 2*dat[1]+ dat[2])/4, RGB
//Bit  7: 5        reserved
//Bit  4: 2        reg_flkr_binning_rs       // unsigned ,    RW, default = 0  row average binning step= 2^x. 0: RO for each row avg; 1: each RO for two rows; 2: each RO for 4rows; 3: each RO for 8rows;
//Bit  1            reserved
//Bit  0           reg_flkr_ro_mode          // unsigned ,    RW, default = 1  mode of RO-RAM, 0: avg(cur-p1); 1: avg(cur), default = 0.
#define ISP_DEFLICKER_STAT_INFO0                   (0x0011  << 2)
//Bit 31: 0        ro_flkr_ro0               // unsigned ,    RO, default = 0  debug info0 
#define ISP_DEFLICKER_STAT_INFO1                   (0x0012  << 2)
//Bit 31: 0        ro_flkr_ro1               // unsigned ,    RO, default = 0  debug info1 
#define ISP_DEFLICKER_STAT_INFO2                   (0x0013  << 2)
//Bit 31: 0        ro_flkr_ro2               // unsigned ,    RO, default = 0  debug info2 
#define ISP_DEFLICKER_STAT_INFO3                   (0x0014  << 2)
//Bit 31: 0        ro_flkr_ro3               // unsigned ,    RO, default = 0  debug info3 
#define ISP_DEFLICKER_STAT_INFO4                   (0x0015  << 2)

struct flicker_pram {   
    int reg_ctrl0;
    int reg_ctrl1;
    int reg_ctrl2;
    int reg_num_cmp;
    int reg_bit_depth;
    int reg_gclk;
    int reg_insel;
    int reg_bypass;
    int reg_raw_mode;
    int reg_wdr_inp_chn;
    int reg_soft_rst;
    int reg_flkr_stat_en;
    int reg_lXSizeIn;
    int reg_lYSizeIn;
    int reg_axi_bsize;
    int reg_wr_burst_lens;
    int reg_wr_req_th;
    int reg_wr_awid;
    int reg_wr_awport;
    int reg_axi_reserved;
    int reg_wr_urgent_ctrl;
    int reg_axi_mode;
    int reg_axi_buff_rst;
    int reg_axi_fbuf;
    int reg_axi_fsize;
    int reg_axi_addr;
    int reg_axi_addrp;
    int reg_flkr_xphs_ofst;
    int reg_flkr_yphs_ofst;
    int reg_flkr_stat_yst;
    int reg_flkr_stat_yed;
    int reg_flkr_stat_xst;
    int reg_flkr_stat_xed;
    int reg_flkr_stat_div_coef;
    int reg_flkr_wdr_ratio;
    int reg_flkr_avg_chnen;
    int reg_flkr_sta_input_format;
    int reg_flkr_sta_cmpsel;
    int reg_flkr_binning_rs;
    int reg_flkr_ro_mode;
    int ro_flkr_ro0;
    int ro_flkr_ro1;
    int ro_flkr_ro2;
    int ro_flkr_ro3;
};

struct am_flicker {
    struct device_node *of_node;
    struct platform_device *p_dev;
    struct resource reg;
    void __iomem *base_addr;
    int irq;
    int ctx_id;
};

int am_flicker_parse_dt(struct device_node *node);
int aml_flicker_init(void);
int aml_flicker_deinit(void);
int aml_flicker_start(int xsize, int ysize);
int aml_flicker_stop(void);
void aml_flicker_get_data(uint32_t* dst);