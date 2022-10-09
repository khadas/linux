#include "nr_fw.h"
#include "system_am_flicker.h"

int param_flkr_ctrl_init(T_FLKR_CTRL_PRM *prm_flkr_ctrl)
{
    prm_flkr_ctrl->flkr_det_en = 1;
    prm_flkr_ctrl->flkr_ctrl_raw_mode     = 1;   // 0: mono, 1:G R  ,2 :   I R     ,3:  G R   ,4:  G R G B, 5~7: other cases
                                              //               B G         B G          B I        I G I G
                                              //                                                   G B G R
                                              //                                                   I G I G

    prm_flkr_ctrl->flkr_ctrl_avg_chnen[CH_GR]  = 1;      // enable of averaging of channel data during line sum calculation. for T7, mono/GrRBGb/GrRBIr/IrRBGb, default [1 1 1 1]; for isp GrRBGbIr, default=[1 0 0 1 0]
    prm_flkr_ctrl->flkr_ctrl_avg_chnen[CH_RED] = 1;      //
    prm_flkr_ctrl->flkr_ctrl_avg_chnen[CH_BLU] = 1;      //
    prm_flkr_ctrl->flkr_ctrl_avg_chnen[CH_GB]  = 1;      //
    prm_flkr_ctrl->flkr_ctrl_avg_chnen[CH_IR]  = 1;      //

    prm_flkr_ctrl->flkr_ctrl_binning_rs        = 0;      //row average binning step= 2^x. 0: RO for each row avg; 1: each RO for two rows; 2: each RO for 4rows; 3: each RO for 8rows;

    prm_flkr_ctrl->flkr_ctrl_ro_mode           = 1;      // mode of RO-RAM, 0: avg(cur-p1); 1: avg(cur), default = 0.

    return 0;
}

int param_flkr_det_init(T_FLKR_DET_PRM *prm_flkr_det)
{

    prm_flkr_det->flkr_det_valid_sel         = 0;  // whether delete invalid flicker
    prm_flkr_det->flkr_det_avg_chnen_mode    = 0;  // 0: half (reg_flkr_stat_yed-reg_flkr_stat_yst) statistic, 1: the whole (reg_flkr_stat_yed-reg_flkr_stat_yst) statistic.
    prm_flkr_det->flkr_det_t100          = 338;// line numbers (period) for 100hz, based on exposure updated by software t100 = 1/(29.6*10e(-6))/100
    prm_flkr_det->flkr_det_t120          = 282;// line numbers (period) for 120hz, based on exposure updated by software t120 = 1/(29.6*10e(-6))/120
    prm_flkr_det->flkr_det_lpf           =   1;// 0:no lpf,1: [1 2 1]/4, 2: [1 2 2 2 1]/8, 3: [1 1 1 2 1 1 1]/8, 4 or else: [1 2 2 2 2 2 2 2 1]/16, lpf of row avg for flicker detection
    prm_flkr_det->flkr_det_50hz          =   1;// 1: 50hz, 0: 60hz, result of flicker detection

    prm_flkr_det->flkr_det_stat_pk_dis_rang  = 440; // peaks/valleys interval thrd for peaks/valleys finding
    prm_flkr_det->flkr_det_stat_pk_dis_thr   = 3200;// peaks/valleys interval thrd for valid wave
    prm_flkr_det->flkr_det_stat_pk_val_thr   = 5;   // peaks/valleys value for valid wave


    return 0;
}

int fw_get_peak_valey (int *pDataIn, int *peak_idx_val, int *valey_idx_val, int len, T_FLKR_DET_PRM *prm_flkr_det, int *flk_num)
{
    int j, jp1;
    int stat_down_up = 0; //0: unknown; 1: down; 2: up
    int peak_num = 0;
    int valey_num = 0;
    int rangsize = prm_flkr_det->flkr_det_stat_pk_dis_rang;

    for (j=0; j<len; j++) {

        jp1 = (j+1) > (len-1) ? (len-1) : (j+1);

        if (stat_down_up == 0) {
            if (pDataIn[j] > pDataIn[jp1])  stat_down_up = 1;
            else if (pDataIn[j] < pDataIn[jp1])  stat_down_up = 2;
            else  stat_down_up = 0;
        } else if ( (stat_down_up == 1) && (pDataIn[j] < pDataIn[jp1])) { //from down to up, valey

                stat_down_up = 2;
                //check if need merge
                if ( ((j - valey_idx_val[valey_num]) < rangsize) && (valey_idx_val[len + valey_num] > pDataIn[j])) {

                        valey_idx_val[valey_num] = j;
                        valey_idx_val[len + valey_num] = pDataIn[j];

                } else if (((j - valey_idx_val[valey_num]) >= rangsize)) {
                        valey_num ++;
                        valey_idx_val[valey_num] = j;
                        valey_idx_val[len + valey_num] = pDataIn[j];
                    }
        } else if ( (stat_down_up == 2) && (pDataIn[j] > pDataIn[jp1])) {

                stat_down_up = 1; //from up to down, peak
                //check if need merge
                if ( ((j - peak_idx_val[peak_num]) < rangsize) && (peak_idx_val[len + peak_num] < pDataIn[j])) {

                        peak_idx_val[peak_num] = j;
                        peak_idx_val[len + peak_num] = pDataIn[j];
                } else if (((j - peak_idx_val[peak_num]) >= rangsize)) {
                        peak_num ++;
                        peak_idx_val[peak_num] = j;
                        peak_idx_val[len + peak_num] = pDataIn[j];
                }
        }
    }//j

    *flk_num = MIN(peak_num, valey_num) + 1;
    //printf("flk_num %d\n", *flk_num);
    return 0;

}

int fw_period_pattern_det (int *peak_idx_val, int *valey_idx_val, T_FLKR_DET_PRM *prm_flkr_det, int *flk_num, int len, int *flkr_vld_flag)
{
    int j, var;
    int tmp_peak, tmp_valey;
    uint32_t var_thr        = prm_flkr_det->flkr_det_stat_pk_dis_thr; //distance variance
    uint32_t peak_valey_thrd   = prm_flkr_det->flkr_det_stat_pk_val_thr;
    int peak_valey_flag = 1;
    int got_wave_rang = 0;
    int num = *flk_num;
    int norm;

    var = 0;

    if (num == 0 || num>5) {
        *flkr_vld_flag = 0;
        pr_err("%s-%d No flicker! num:%d \n",__FUNCTION__,__LINE__,num);
        return 1;
    } else if(num == 1) {
        *flkr_vld_flag = 0;
        pr_err("%s-%d Invalid flicker!\n",__FUNCTION__,__LINE__); // peak_valey_val < thrd
        return 1;
    } else{
        norm = (num == 2) ? 1 : ((num==3) ? 2 : 3);
    }
    num = MIN(5,num);

    // wave peak/valey
    for (j = 0; j < (num-1); j++) {

        got_wave_rang += (( peak_idx_val[j+1] - peak_idx_val[j] ) + ( valey_idx_val[j+1] - valey_idx_val[j] ));

        if (peak_idx_val[len+j] < peak_valey_thrd || ABS(valey_idx_val[len+j]) < peak_valey_thrd) { // if peak/valey > thrd, useful wave;
            peak_valey_flag = 0;
        }
    }
    // wave range
    for (j = 0; j < (num-1); j++) {

        tmp_peak = (peak_idx_val[j+1] - peak_idx_val[j]) - (got_wave_rang >> norm);
        tmp_valey = (valey_idx_val[j+1] - valey_idx_val[j]) - (got_wave_rang >> norm);
        var += tmp_peak*tmp_peak + tmp_valey*tmp_valey;
    }

    var = var >> (norm+1);
    if (var > var_thr) {

            *flkr_vld_flag = 0;
            pr_err("%s-%d No flicker! var:%d \n",__FUNCTION__,__LINE__,var);
            return 1;
    }
    if (peak_valey_flag == 0) {

        *flkr_vld_flag = 0;
        pr_err("%s-%d Invalid flicker!\n",__FUNCTION__,__LINE__);
        return 1;
    }
    return 0;
}

int fw_flt1d(int *pDataOut, int *pDataIn, int *pFlt, int data_len, int flt_len, int flt_norm)
{
    int k, j, jj;
    int sum;

    for (j = 0; j < data_len; j++) {
        sum = 0;
        for (k = -flt_len/2; k <= flt_len/2; k++) {
            jj = j+k < 0 ? 0 : (j+k > data_len-1 ? data_len-1 : j+k);
            sum += pDataIn[jj] * pFlt[k + flt_len/2];
        }
        pDataOut[j] = (sum + (1<<(flt_norm-1)))>>flt_norm;
    }

    return 0;
}

static int ro_flkr_stat_pre[FED_FLKR_STAT_MAX];
static int ro_flkr_stat_cur[FED_FLKR_STAT_MAX];
static int peak_idx_val[FED_FLKR_STAT_MAX*2];  //= (int*)kzalloc( FED_FLKR_STAT_MAX*2, GFP_KERNEL );
static int valey_idx_val[FED_FLKR_STAT_MAX*2];  //= (int*)kzalloc( FED_FLKR_STAT_MAX*2, GFP_KERNEL );
static int pDif[FED_FLKR_STAT_MAX];           //= (int*)kzalloc( FED_FLKR_STAT_MAX, GFP_KERNEL );
static int pDifLpf[FED_FLKR_STAT_MAX];        //= (int*)kzalloc( FED_FLKR_STAT_MAX, GFP_KERNEL );
static char dump_name[100];
static loff_t pos;

int fw_flicker_det(T_FLKR_CTRL_PRM *prm_flkr_ctrl, T_FLKR_DET_PRM *prm_flkr_det, int frame_id_current)
{
    int j, jm1, jp1, k;
    int rtn = 0;
    int h100, h120, r100, r120;
    int flk_num; // min(peak_num, valley num)
    int reg_flkr_det_t100,reg_flkr_det_t120;
    int flkr_vld_flag = 1;
    int got_1st_zcrss = 0;
    int zcrss_idx     = 0;
    int len           = FED_FLKR_STAT_MAX;

    int lpf_1x3[] = {1, 2, 1};
    int lpf_1x5[] = {1, 2, 2, 2, 1};
    int lpf_1x7[] = {1, 1, 1, 2, 1, 1, 1};
    int lpf_1x9[] = {1, 2, 2, 2, 2, 2, 2, 2, 1};

    if ((prm_flkr_ctrl->flkr_ctrl_raw_mode == 2 || prm_flkr_ctrl->flkr_ctrl_raw_mode == 3) && prm_flkr_det->flkr_det_avg_chnen_mode == 0) {
        reg_flkr_det_t100 = prm_flkr_det->flkr_det_t100 >> (prm_flkr_ctrl->flkr_ctrl_binning_rs + 1);
        reg_flkr_det_t120 = prm_flkr_det->flkr_det_t120 >> (prm_flkr_ctrl->flkr_ctrl_binning_rs + 1);
    } else{
        reg_flkr_det_t100 = prm_flkr_det->flkr_det_t100 >> prm_flkr_ctrl->flkr_ctrl_binning_rs;
        reg_flkr_det_t120 = prm_flkr_det->flkr_det_t120 >> prm_flkr_ctrl->flkr_ctrl_binning_rs;
    }

    //detect flicker and valid wave
    if (prm_flkr_ctrl->flkr_det_en == 1) {
        memcpy(pDif, prm_flkr_ctrl->ro_flkr_stat_avg_dif, len*sizeof(int));
        // if ro_mode=1, need to calculate the pDifLpf= ro_flkr_stat_avg_dif[cur]- ro_flkr_stat_avg_dif[p1], TODO
        if (prm_flkr_ctrl->flkr_ctrl_ro_mode == 1) // calculate the diff from HW RO (avg(cur))
        {
            for (k = 0; k < FED_FLKR_STAT_MAX; k++)
            {
                ro_flkr_stat_pre[k] = ro_flkr_stat_cur[k];
                ro_flkr_stat_cur[k] = pDif[k];
                pDif[k] = (ro_flkr_stat_pre[k] - ro_flkr_stat_cur[k]);
                pDif[k] = MIN(MAX(pDif[k], -2048), 2047);
            }
        }// else pDifLpf[] is s12 from HW RO (avg(pre-cur))

        //whether flicker is invalid
        if (prm_flkr_det->flkr_det_valid_sel == 1) {
            fw_get_peak_valey(pDif, peak_idx_val, valey_idx_val, len, prm_flkr_det, &flk_num);
            rtn = fw_period_pattern_det(peak_idx_val, valey_idx_val, prm_flkr_det, &flk_num, len, &flkr_vld_flag);
            if (rtn == 1) return 0;
        }
    }

    if (prm_flkr_ctrl->flkr_det_en == 1 && flkr_vld_flag == 1) {
        //lpf
        if (prm_flkr_det->flkr_det_lpf == 0) {
            memcpy(pDifLpf, pDif, len*sizeof(int));
        } else if (prm_flkr_det->flkr_det_lpf == 1) {
            fw_flt1d(pDifLpf, pDif, lpf_1x3, len, 3, 2);
        } else if (prm_flkr_det->flkr_det_lpf == 2) {
            fw_flt1d(pDifLpf, pDif, lpf_1x5, len, 5, 3);
        } else if (prm_flkr_det->flkr_det_lpf == 3) {
            fw_flt1d(pDifLpf, pDif, lpf_1x7, len, 7, 3);
        } else {
            fw_flt1d(pDifLpf, pDif, lpf_1x9, len, 9, 4);
        }

        got_1st_zcrss = 0;
        r100 = r120 = 0;

        for (j = 0; j < len; j++) {

            jm1 = j-1 < 0 ? 0 : j-1;
            jp1 = j+1 > len-1 ? len-1 : j+1;

            // find the first zero-cross
            if (got_1st_zcrss == 0 && (pDifLpf[jm1]*pDifLpf[jp1] < 0 || pDifLpf[j] == 0 )) {
                got_1st_zcrss = 1;
                zcrss_idx = j;
            }

            if (1 == got_1st_zcrss) {
                //for haar 100
                if ((j - zcrss_idx)%reg_flkr_det_t100 <= reg_flkr_det_t100/2) {
                    h100 = 1;
                } else {
                    h100 = -1;
                }
                r100 += h100 * pDifLpf[j];

                //for haar 120
                if ((j - zcrss_idx)%reg_flkr_det_t120 <= reg_flkr_det_t120/2) {
                    h120 = 1;
                } else {
                    h120 = -1;
                }
                r120 += h120 * pDifLpf[j];
            }
        }//j

        // decide 50/60hz
        if (ABS(r100) > ABS(r120)) {
            prm_flkr_det->flkr_det_50hz = 1;
        } else {
            prm_flkr_det->flkr_det_50hz = 0;
        }

       //pr_err("Flicker Detection: reg_flkr_det_50hz = %d, r100: %d, r120:%d frame_id_current:%d\n", prm_flkr_det->flkr_det_50hz, ABS(r100), ABS(r120),frame_id_current);
       return 1;
    }

    //kfree(pDif);
    //kfree(pDifLpf);
    //kfree(peak_idx_val);
    //kfree(valey_idx_val);

    return 0;
}
