#ifndef __NR_FW_H__
#define __NR_FW_H__
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/spinlock_types.h>
#include <linux/wait.h>
#include <linux/version.h>

#define CH_GR  0
#define CH_RED 1
#define CH_BLU 2
#define CH_GB  3
#define CH_IR  4

#define ISP_HAS_FLICKER_INTERNAL 1
#define FED_FLKR_STAT_MAX 1280

#ifndef MAX
#define MAX(x, y) ((x) >= (y) ? (x) : (y))
#endif

#ifndef MIN
#define MIN(x, y) ((x) <= (y) ? (x) : (y))
#endif

#ifndef ABS
#define ABS(a)   ( ((a)<0)? -(a):(a) )
#endif

//deflicker
typedef struct{

    int xsize;
    int ysize;
    int flkr_det_en;
    int flkr_ctrl_raw_mode;
    int flkr_ctrl_avg_chnen[5]; // the averaging of channel data during line sum calculation, GrRBGbIr; default=[1 0 0 1 0]
                                //only support gr/gb [1 1 1 1 1] or all channels [1 0 0 1 0]
    int flkr_ctrl_binning_rs;   //row average binning step= 2^x. 0: RO for each row avg; 1: each RO for two rows; 2: each RO for 4rows; 3: each RO for 8rows;
    int flkr_ctrl_ro_mode;      // mode of RO-RAM, 0: avg(cur-p1); 1: avg(cur), default = 0.
    int ro_flkr_stat_avg_dif[FED_FLKR_STAT_MAX];// row average for row statistic; mode=0, avg(cur-pre)= s12; mode=1, avg(cur)= u12; save to ram, FED_ROW_STAT_MAX x s12 default

}T_FLKR_CTRL_PRM;


typedef struct {

    // 50/60hz flicker detection
    uint32_t flkr_det_valid_sel;       // whether delete invalid flicker
    uint32_t flkr_det_avg_chnen_mode;  // 0: half (reg_flkr_stat_yed-reg_flkr_stat_yst) statistic, 1: the whole (reg_flkr_stat_yed-reg_flkr_stat_yst) statistic.
    uint32_t flkr_det_t100;        // line numbers (period) for 100hz, based on exposure updated by software, t100 = 1/(29.6*10e(-6))/100
    uint32_t flkr_det_t120;        // line numbers (period) for 120hz, based on exposure updated by software, t120 = 1/(29.6*10e(-6))/120
    uint32_t flkr_det_lpf;         // 0:no lpf,1: [1 2 1]/4, 2: [1 2 2 2 1]/8, 3: [1 1 1 2 1 1 1]/8, 4 or else: [1 2 2 2 2 2 2 2 1]/16, lpf of row avg for flicker detection
    uint32_t flkr_det_50hz;        // 1: 50hz, 0: 60hz, result of flicker detection
    uint32_t flkr_det_stat_pk_dis_rang;// peaks/valleys interval thrd for peaks/valleys finding
    uint32_t flkr_det_stat_pk_dis_thr; // peaks/valleys interval thrd for valid wave
    uint32_t flkr_det_stat_pk_val_thr; // peaks/valleys value for valid wave

}T_FLKR_DET_PRM;

int param_flkr_ctrl_init(T_FLKR_CTRL_PRM *prm_flkr_ctrl);
int param_flkr_det_init(T_FLKR_DET_PRM *prm_flkr_det);

//int fw_flicker_det(T_FLKR_CTRL_PRM *prm_flkr_ctrl, T_FLKR_DET_PRM *prm_flkr_det);
int fw_flicker_det(T_FLKR_CTRL_PRM *prm_flkr_ctrl, T_FLKR_DET_PRM *prm_flkr_det, int frame_id_current);


int fw_flt1d(int *pDataOut, int *pDataIn, int *pFlt, int data_len, int flt_len, int flt_norm);

int fw_get_peak_valey (int *pDataIn, int *peak_idx_val, int *valey_idx_val, int len, T_FLKR_DET_PRM *prm_flkr_det, int *flk_num);

int fw_period_pattern_det (int *peak_idx_val, int *valey_idx_val, T_FLKR_DET_PRM *prm_flkr_det, int *flk_num, int len, int *flkr_vld_flag);
#endif

