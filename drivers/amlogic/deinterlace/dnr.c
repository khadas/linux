#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/amlogic/iomap.h>
#include "register.h"
#include "dnr.h"
#include "deinterlace.h"

static DNR_PRM_t dnr_param;
static DNR_PRM_t *pDnrPrm = &dnr_param;
static bool dnr_pr;
module_param(dnr_pr, bool, 0644);
MODULE_PARM_DESC(dnr_pr, "/n print dnr debug information /n");

bool dnr_dm_en;/*gxtvbb can't work normal,must set to 0*/
module_param(dnr_dm_en, bool, 0644);
MODULE_PARM_DESC(dnr_dm_en, "/n dnr dm enable debug /n");

bool dnr_reg_update = 1;/*gxtvbb can't work normal,must set to 0*/
module_param(dnr_reg_update, bool, 0644);
MODULE_PARM_DESC(dnr_reg_update, "/n dnr dm enable debug /n");

static unsigned int dnr_stat_coef = 3;/*gxtvbb default is 3*/
module_param(dnr_stat_coef, uint, 0644);
MODULE_PARM_DESC(dnr_stat_coef, "/n dnr stat coef /n");

static int dnr_prm_init(DNR_PRM_t *pPrm)
{
	pPrm->prm_sw_gbs_ctrl = 0;
	/* 0: update gbs, 1: update hoffst & gbs,
	2: update voffst & gbs, 3: update all (hoffst & voffst & gbs).*/

	pPrm->prm_gbs_vldcntthd  =	4;
	pPrm->prm_gbs_cnt_min	 = 32;
	pPrm->prm_gbs_ratcalcmod =	1;/* 0: use LR, 1: use Dif */
	pPrm->prm_gbs_ratthd[0]  = 40;
	pPrm->prm_gbs_ratthd[1]  = 80;
	pPrm->prm_gbs_ratthd[2]  = 120;
	pPrm->prm_gbs_difthd[0]  = 25;
	pPrm->prm_gbs_difthd[1]  = 75;
	pPrm->prm_gbs_difthd[2]  = 125;
	pPrm->prm_gbs_bsdifthd	 = 1;
	pPrm->prm_gbs_calcmod	 = 1; /* 0:dif0, 1:dif1, 2: dif2 */

	pPrm->sw_gbs = 0;
	pPrm->sw_gbs_vld_flg = 0;
	pPrm->sw_gbs_vld_cnt = 0;

	pPrm->prm_hbof_minthd	 = 32;
	pPrm->prm_hbof_ratthd0	 = 150;
	pPrm->prm_hbof_ratthd1	 = 150;
	pPrm->prm_hbof_vldcntthd = 4;
	pPrm->sw_hbof			 = 0;
	pPrm->sw_hbof_vld_flg	 = 0;
	pPrm->sw_hbof_vld_cnt	 = 0;

	pPrm->prm_vbof_minthd	 = 32;
	pPrm->prm_vbof_ratthd0	 = 150;
	pPrm->prm_vbof_ratthd1	 = 120;
	pPrm->prm_vbof_vldcntthd = 4;
	pPrm->sw_vbof		  = 0;
	pPrm->sw_vbof_vld_flg = 0;
	pPrm->sw_vbof_vld_cnt = 0;

	return 0;
}
int global_bs_calc_sw(int *pGbsVldCnt,
			  int *pGbsVldFlg,
			  int *pGbs,
			  int nGbsStatLR,
			  int nGbsStatLL,
			  int nGbsStatRR,
			  int nGbsStatDif,
			  int nGbsStatCnt,
			  int prm_gbs_vldcntthd, /* prm below */
			  int prm_gbs_cnt_min,
			  int prm_gbs_ratcalcmod,
			  int prm_gbs_ratthd[3],
			  int prm_gbs_difthd[3],
			  int prm_gbs_bsdifthd,
			  int prm_gbs_calcmod)
{
	int nMax, nMin;
	int nDif0, nDif1, nDif2;
	int nDif, nRat;
	int nCurGbs;

	nMax = max(max(nGbsStatLR, nGbsStatLL), nGbsStatRR);
	nMin = min(min(nGbsStatLR, nGbsStatLL), nGbsStatRR);

	nDif0 = nMax == 0 ? 0 : ((nMax - nMin) << 9)/nMax;
	nDif0 = min(511, nDif0);

	nDif1 = nGbsStatLR == 0 ? 0 :
	(abs(nGbsStatLR - (nGbsStatLL + nGbsStatRR)/2) << 9)/nGbsStatLR;
	nDif1 = min(511, nDif1);

	nDif2 = nGbsStatLR == 0 ? 0 :
	(abs(nGbsStatLR - max(nGbsStatLL, nGbsStatRR)) << 9)/nGbsStatLR;
	nDif2 = min(511, nDif2);

	if (0 == prm_gbs_ratcalcmod)
		nRat = (nGbsStatLR << 4) / max(prm_gbs_cnt_min, nGbsStatCnt);
	else
		nRat = (nGbsStatDif << 4) / max(prm_gbs_cnt_min, nGbsStatCnt);

	nDif = 0 == prm_gbs_calcmod ? nDif0 :
(1 == prm_gbs_calcmod ? nDif1 : nDif2);

	if (nGbsStatLR < max(nGbsStatLL, nGbsStatRR)) {
		if (nGbsStatCnt <= prm_gbs_cnt_min || nRat <= prm_gbs_ratthd[0])
			nCurGbs = 0;
		else if (nRat <= prm_gbs_ratthd[1])
			nCurGbs = 1;
		else if (nRat <= prm_gbs_ratthd[2])
			nCurGbs = 2;
		else
			nCurGbs = 3;
	} else {
		if (nGbsStatCnt <= prm_gbs_cnt_min || nDif <= prm_gbs_difthd[0])
			nCurGbs = 0;
		else if (nDif <= prm_gbs_difthd[1])
			nCurGbs = 1;
		else if (nDif <= prm_gbs_difthd[2])
			nCurGbs = 2;
		else
			nCurGbs = 3;
	}

	/*	*/
	if ((nCurGbs != 0 && 0 == *pGbs) ||
(nCurGbs != 0 && abs(nCurGbs - *pGbs) <= prm_gbs_bsdifthd))
		(*pGbsVldCnt)++;
	else
		*pGbsVldCnt = 0;

	if (*pGbsVldCnt >= prm_gbs_vldcntthd)
		*pGbsVldFlg = 1;
	else
		*pGbsVldFlg = 0;

	*pGbs = nCurGbs;

	/* print debug info. */
	/* printk("GBS info at Field: LR = %6d, LL = %6d, RR = %6d.\n",
	nGbsStatLR, nGbsStatLL, nGbsStatRR; */

	return 0;
}

#ifdef DNR_HV_SHIFT
int hor_blk_ofst_calc_sw(int *pHbOfVldCnt,
			 int *pHbOfVldFlg,
			 int *pHbOfst,
			 int nHbOfStatCnt[32],
			 int nXst,
			 int nXed,
			 int prm_hbof_minthd,
			 int prm_hbof_ratthd0,
			 int prm_hbof_ratthd1,
			 int prm_hbof_vldcntthd,
			 int nRow,
			 int nCol)
{
	int i = 0;

	int nCurHbOfst = 0;
	int nRat0 = 0, nRat1 = 0;

	int nMax1 = 0;
	int nMax2 = 0;
	int nMaxIdx = 0;

	/* get 2 maximum, move to RTL part */
	nMax1 = nMax2 = 0;
	for (i = 0; i < 8; i++) {
		if (nHbOfStatCnt[i] > nMax1) {
			nMax2 = nMax1;
			nMax1 = nHbOfStatCnt[i];
			nMaxIdx = i;
		} else if (nHbOfStatCnt[i] > nMax2) {
			nMax2 = nHbOfStatCnt[i];
		}
	} /* i */

	/* decide if offset valid */
	nCurHbOfst = -1;
	nRat0 = 256*nMax1/((nXed - nXst)/8)/nRow;
	nRat1 = 128*nMax1/max(nMax2, prm_hbof_minthd);
	if (nRat0 >= prm_hbof_ratthd0 && nRat1 >= prm_hbof_ratthd1)
		nCurHbOfst = (nMaxIdx+1)%8;

	if (nCurHbOfst == *pHbOfst)
		(*pHbOfVldCnt)++;
	else
		*pHbOfVldCnt = 0;

	if (*pHbOfVldCnt >= prm_hbof_vldcntthd)
		*pHbOfVldFlg = 1;
	else
		*pHbOfVldFlg = 0;

	*pHbOfst = (nCurHbOfst == -1) ? 0 : nCurHbOfst;

	/* print for debug
	printk("Hoff info at Field: ");
	for ( i = 0; i < 32; i++ ) {
	printk("%5d, ",  nHbOfStatCnt[i]);
	}
	*/
	if (dnr_pr) {
		pr_dbg("Max1 = %5d, Max2 = %5d, MaxIdx = %5d, Rat0 = %5d,Rat1 = %5d.\n",
				nMax1, nMax2, nMaxIdx, nRat0, nRat1);
		pr_dbg("CurHbOfst = %5d, HbOfVldFlg = %d, HbOfVldCnt = %d.\n",
				nCurHbOfst, *pHbOfVldFlg, *pHbOfVldCnt);
	}

	return 0;
}
int ver_blk_ofst_calc_sw(int *pVbOfVldCnt,
			 int *pVbOfVldFlg,
			 int *pVbOfst,
			 int nVbOfStatCnt[32],
			 int nYst,
			 int nYed,
			 int prm_vbof_minthd,
			 int prm_vbof_ratthd0,
			 int prm_vbof_ratthd1,
			 int prm_vbof_vldcntthd,
			 int nRow,
			 int nCol)
{
	int i = 0;

	int nCurVbOfst = 0;
	int nRat0 = 0, nRat1 = 0;

	int nMax1 = 0;
	int nMax2 = 0;
	int nMaxIdx = 0;

	/* get 2 maximum, move to RTL part */
	nMax1 = nMax2 = 0;
	for (i = 0; i < 8; i++) {
		if (nVbOfStatCnt[i] > nMax1) {
			nMax2 = nMax1;
			nMax1 = nVbOfStatCnt[i];
			nMaxIdx = i;
		}
	else if (nVbOfStatCnt[i] > nMax2) {
		nMax2 = nVbOfStatCnt[i];
		}
	}

	/* decide if offset valid */
	nCurVbOfst = -1;
	nRat0 = 256*nMax1/((nYed - nYst)/8)/nCol;
	nRat1 = 128*nMax1/max(nMax2, prm_vbof_minthd);
	if (nRat0 >= prm_vbof_ratthd0 && nRat1 >= prm_vbof_ratthd1)
		nCurVbOfst = (nMaxIdx+1)%8;

	if (nCurVbOfst == *pVbOfst)
		(*pVbOfVldCnt)++;
	else
		*pVbOfVldCnt = 0;

	if (*pVbOfVldCnt >= prm_vbof_vldcntthd)
		*pVbOfVldFlg = 1;
	else
		*pVbOfVldFlg = 0;

	*pVbOfst = (nCurVbOfst == -1) ? 0 : nCurVbOfst;

	/* print for debug
	printk("Voff info at Field: ");
	for ( i = 0; i < 32; i++ ) {
	printk("%5d, ",  nVbOfStatCnt[i]);
	}//i
	//printk("Max1 = %5d, Max2 = %5d, MaxIdx = %5d, Rat0 = %5d, Rat1 = %5d,
	CurVbOfst = %5d, VbOfVldFlg = %d, VbOfVldCnt = %d.\n", nMax1, nMax2,
	nMaxIdx, nRat0, nRat1, nCurVbOfst, *pVbOfVldFlg, *pVbOfVldCnt);*/
	return 0;
}
#endif

void run_dnr_in_irq(unsigned short nCol, unsigned short nRow)
{
	static int ro_gbs_stat_lr = 0, ro_gbs_stat_ll = 0, ro_gbs_stat_rr = 0,
	ro_gbs_stat_dif = 0, ro_gbs_stat_cnt = 0;
	/* int reg_dnr_stat_xst=0,reg_dnr_stat_xed=0,
reg_dnr_stat_yst=0,reg_dnr_stat_yed=0; */
#ifdef DNR_HV_SHIFT
	int ro_hbof_stat_cnt[32], ro_vbof_stat_cnt[32], i = 0;
#endif
	if (dnr_reg_update == 0)
		return;

	DI_Wr(DNR_CTRL, 0x1df00);
	DI_Wr(DNR_DM_CTRL, Rd(DNR_DM_CTRL)|(1 << 11));
	/* dm for sd, hd will slower */
	if (nCol >= 1920)
		DI_Wr_reg_bits(DNR_DM_CTRL, 0, 9, 1);
	else
		DI_Wr_reg_bits(DNR_DM_CTRL, dnr_dm_en, 9, 1);
	DI_Wr(DNR_HVSIZE, nCol<<16|nRow);
	DI_Wr(DNR_STAT_X_START_END, (((dnr_stat_coef<<3)&0x3fff) << 16)
		|((nCol-((dnr_stat_coef<<3)+1))&0x3fff));
	DI_Wr(DNR_STAT_Y_START_END, (((dnr_stat_coef<<3)&0x3fff) << 16)
		|((nRow-((dnr_stat_coef<<3)+1))&0x3fff));
	if (ro_gbs_stat_lr != Rd(DNR_RO_GBS_STAT_LR) ||
		ro_gbs_stat_ll != Rd(DNR_RO_GBS_STAT_LL) ||
		ro_gbs_stat_rr != Rd(DNR_RO_GBS_STAT_RR) ||
		ro_gbs_stat_dif != Rd(DNR_RO_GBS_STAT_DIF) ||
		ro_gbs_stat_cnt != Rd(DNR_RO_GBS_STAT_CNT)) {

		ro_gbs_stat_lr = Rd(DNR_RO_GBS_STAT_LR);
		ro_gbs_stat_ll = Rd(DNR_RO_GBS_STAT_LL);
		ro_gbs_stat_rr = Rd(DNR_RO_GBS_STAT_RR);
		ro_gbs_stat_dif = Rd(DNR_RO_GBS_STAT_DIF);
		ro_gbs_stat_cnt = Rd(DNR_RO_GBS_STAT_CNT);
	} else {
		return;
	}

	global_bs_calc_sw(&pDnrPrm->sw_gbs_vld_cnt,
			  &pDnrPrm->sw_gbs_vld_flg,
			  &pDnrPrm->sw_gbs,
			  ro_gbs_stat_lr,
			  ro_gbs_stat_ll,
			  ro_gbs_stat_rr,
			  ro_gbs_stat_dif,
			  ro_gbs_stat_cnt,
			  pDnrPrm->prm_gbs_vldcntthd, /* prm below */
			  pDnrPrm->prm_gbs_cnt_min,
			  pDnrPrm->prm_gbs_ratcalcmod,
			  pDnrPrm->prm_gbs_ratthd,
			  pDnrPrm->prm_gbs_difthd,
			  pDnrPrm->prm_gbs_bsdifthd,
			  pDnrPrm->prm_gbs_calcmod);
#ifdef DNR_HV_SHIFT
	for (i = 0; i < 32; i++)
		ro_hbof_stat_cnt[i] = Rd(DNR_RO_HBOF_STAT_CNT_0+i);
	for (i = 0; i < 32; i++)
		ro_vbof_stat_cnt[i] = Rd(DNR_RO_VBOF_STAT_CNT_0+i);
		hor_blk_ofst_calc_sw(&pDnrPrm->sw_hbof_vld_cnt,
			 &pDnrPrm->sw_hbof_vld_flg,
			 &pDnrPrm->sw_hbof,
			 ro_hbof_stat_cnt,
			 0,
			 nCol-1,
			 pDnrPrm->prm_hbof_minthd,
			 pDnrPrm->prm_hbof_ratthd0,
			 pDnrPrm->prm_hbof_ratthd1,
			 pDnrPrm->prm_hbof_vldcntthd,
			 nRow,
			 nCol);

	ver_blk_ofst_calc_sw(&pDnrPrm->sw_vbof_vld_cnt,
			 &pDnrPrm->sw_vbof_vld_flg,
			 &pDnrPrm->sw_vbof,
			 ro_vbof_stat_cnt,
			 0,
			 nRow-1,
			 pDnrPrm->prm_vbof_minthd,
			 pDnrPrm->prm_vbof_ratthd0,
			 pDnrPrm->prm_vbof_ratthd1,
			 pDnrPrm->prm_vbof_vldcntthd,
			 nRow,
			 nCol);
#endif
	/* update hardware registers */
	if (0 == pDnrPrm->prm_sw_gbs_ctrl) {
		DI_Wr(DNR_GBS,
			(1 == pDnrPrm->sw_gbs_vld_flg)?pDnrPrm->sw_gbs : 0);
	} else if (1 == pDnrPrm->prm_sw_gbs_ctrl) {
		DI_Wr_reg_bits(DNR_BLK_OFFST,
1 == pDnrPrm->sw_hbof_vld_flg?pDnrPrm->sw_hbof:0, 4, 3);
		DI_Wr(DNR_GBS,
(1 == pDnrPrm->sw_hbof_vld_flg &&
1 == pDnrPrm->sw_gbs_vld_flg)?pDnrPrm->sw_gbs:0);
	} else if (2 == pDnrPrm->prm_sw_gbs_ctrl) {
		DI_Wr_reg_bits(DNR_BLK_OFFST,
1 == pDnrPrm->sw_vbof_vld_flg?pDnrPrm->sw_vbof:0, 0, 3);
		DI_Wr(DNR_GBS,
(1 == pDnrPrm->sw_vbof_vld_flg &&
1 == pDnrPrm->sw_gbs_vld_flg)?pDnrPrm->sw_gbs:0);
	} else if (3 == pDnrPrm->prm_sw_gbs_ctrl) {
		DI_Wr_reg_bits(DNR_BLK_OFFST,
1 == pDnrPrm->sw_hbof_vld_flg ? pDnrPrm->sw_hbof : 0, 4, 3);
	DI_Wr_reg_bits(DNR_BLK_OFFST,
1 == pDnrPrm->sw_vbof_vld_flg ? pDnrPrm->sw_vbof : 0, 0, 3);
	DI_Wr(DNR_GBS,
(1 == pDnrPrm->sw_hbof_vld_flg && 1 == pDnrPrm->sw_vbof_vld_flg &&
1 == pDnrPrm->sw_gbs_vld_flg)?pDnrPrm->sw_gbs:0);
	}
}

static void parse_cmd_params(char *buf_orig, char **parm)
{
	char *ps, *token;
	unsigned int n = 0;

	ps = buf_orig;
	while (1) {
		token = strsep(&ps, " \n");
		if (token == NULL)
			break;
		if (*token == '\0')
			continue;
		parm[n++] = token;
	}
}

static dnr_param_t dnr_params[] = {
	{"prm_sw_gbs_ctrl", &(dnr_param.prm_sw_gbs_ctrl)},
	{"prm_gbs_vldcntthd", &(dnr_param.prm_gbs_vldcntthd)},
	{"prm_gbs_cnt_min", &(dnr_param.prm_gbs_cnt_min)},
	{"prm_gbs_ratcalcmod", &(dnr_param.prm_gbs_ratcalcmod)},
	{"prm_gbs_ratthd[0]", &(dnr_param.prm_gbs_ratthd[0])},
	{"prm_gbs_ratthd[1]", &(dnr_param.prm_gbs_ratthd[1])},
	{"prm_gbs_ratthd[2]", &(dnr_param.prm_gbs_ratthd[2])},
	{"prm_gbs_difthd[0]", &(dnr_param.prm_gbs_difthd[0])},
	{"prm_gbs_difthd[1]", &(dnr_param.prm_gbs_difthd[1])},
	{"prm_gbs_difthd[2]", &(dnr_param.prm_gbs_difthd[2])},
	{"prm_gbs_bsdifthd", &(dnr_param.prm_gbs_bsdifthd)},
	{"prm_gbs_calcmod", &(dnr_param.prm_gbs_calcmod)},
	{"sw_gbs", &(dnr_param.sw_gbs)},
	{"sw_gbs_vld_flg", &(dnr_param.sw_gbs_vld_flg)},
	{"sw_gbs_vld_cnt", &(dnr_param.sw_gbs_vld_cnt)},
	{"prm_hbof_minthd", &(dnr_param.prm_hbof_minthd)},
	{"prm_hbof_ratthd0", &(dnr_param.prm_hbof_ratthd0)},
	{"prm_hbof_ratthd1", &(dnr_param.prm_hbof_ratthd1)},
	{"prm_hbof_vldcntthd", &(dnr_param.prm_hbof_vldcntthd)},
	{"sw_hbof", &(dnr_param.sw_hbof)},
	{"sw_hbof_vld_flg", &(dnr_param.sw_hbof_vld_flg)},
	{"sw_hbof_vld_cnt", &(dnr_param.sw_hbof_vld_cnt)},
	{"prm_vbof_minthd", &(dnr_param.prm_vbof_minthd)},
	{"prm_vbof_ratthd0", &(dnr_param.prm_vbof_ratthd0)},
	{"prm_vbof_ratthd1", &(dnr_param.prm_vbof_ratthd1)},
	{"prm_vbof_vldcntthd", &(dnr_param.prm_vbof_vldcntthd)},
	{"sw_vbof", &(dnr_param.sw_vbof)},
	{"sw_vbof_vld_flg", &(dnr_param.sw_vbof_vld_flg)},
	{"sw_vbof_vld_cnt", &(dnr_param.sw_vbof_vld_cnt)},
	{"", NULL}
};
static ssize_t dnr_param_store(struct device *dev,
				struct device_attribute *attr,
				const char *buff, size_t count)
{
	int i = 0, vaule = 0;
	char *parm[2] = {NULL}, *buf_orig;

	buf_orig = kstrdup(buff, GFP_KERNEL);
	parse_cmd_params(buf_orig, (char **)(&parm));
	for (i = 0; dnr_params[i].addr; i++) {
		if (!strcmp(parm[0], dnr_params[i].name)) {
			vaule = kstrtol(parm[1], 10, NULL);
			*(dnr_params[i].addr) = vaule;
			pr_dbg("%s=%d.\n", dnr_params[i].name,
*(dnr_params[i].addr));
		}
	}

	return count;
}

static ssize_t dnr_param_show(struct device *dev,
				struct device_attribute *attr,
				char *buff)
{
	ssize_t len = 0;
	int i = 0;
	for (i = 0; dnr_params[i].addr; i++)
		len += sprintf(buff+len, "%s=%d.\n",
dnr_params[i].name, *(dnr_params[i].addr));
	return len;
}
static DEVICE_ATTR(dnr_param, 0664, dnr_param_show, dnr_param_store);

void dnr_init(struct device *dev)
{
	dnr_prm_init(&dnr_param);
	device_create_file(dev, &dev_attr_dnr_param);
}

