/* Film Detection and VOF detection Software implementation
* Designer: Xin.Hu@amlogic.com
* Date: 12/06/13 */
#include <linux/kernel.h>
#include <linux/module.h>
#include "film_vof_soft.h"

/* Software parameters (registers) */
UINT8 FlmVOFSftInt(struct sFlmSftPar *pPar)
{
	pPar->sFrmDifAvgRat = 16;	/* 0~32 */
	/* The Large Decision should be: (large>average+LgDifThrd) */
	pPar->sFrmDifLgTDif = 4096;
	pPar->sF32StpWgt01 = 15;
	pPar->sF32StpWgt02 = 15;
	/* Dif>Rat*Min  --> Larger */
	pPar->sF32DifLgRat = 16;

	pPar->sFlm2MinAlpha = 32;	/* [0~63] */
	pPar->sFlm2MinBelta = 32;	/* [0~63] */
	pPar->sFlm20ftAlpha = 16;	/* [0~63] */
	pPar->sFlm2LgDifThd = 4096;	/* [0~63] %LgDifThrd */
	pPar->sFlm2LgFlgThd = 8;

	pPar->sF32Dif01A1 = 65;
	pPar->sF32Dif01T1 = 128;
	pPar->sF32Dif01A2 = 65;
	pPar->sF32Dif01T2 = 128;

	pPar->rCmbRwMinCt0 = 8;	/* for film 3-2 */
	pPar->rCmbRwMinCt1 = 7;	/* for film 2-2 */

	pPar->sFlm32NCmb = 8;	/* absolute no combing for film 32 */
	/* pre-processing (t-0), post-processing f(t-mPstDlyPre); No RTL */
	pPar->mPstDlyPre = 1;
	/* pre-processing (t-0), pre-processing f(t+mNxtDlySft);
	No RTL,default=1 */
	pPar->mNxtDlySft = 1;

	pPar->sF32Dif02M0 = 4096;	/* mpeg-4096, cvbs-8192 */
	pPar->sF32Dif02M1 = 4096;

	return 0;
}

/* Outputs:
*     rCmb32Spcl:  1-bit, 0/1
*    rPstCYWnd0~4[0]: bgn,
*     rPstCYWnd0~4[1]: end,
*     rPstCYWnd0~4[2]: 0-mtn,1-with-buffer,2-ei,3-di,
*     rFlmPstGCm: 1-bit, global combing (maybe unused)
*     rFlmSltPre: 1-bit, 0-next field, 1-previous field
*     rFlmPstMod: 2-bit, 00-no, 01-22, 10-23, 11-other
* Inputs:
*     rROFldDif01:  difference f(t) and f(t-1), U32
*     rROFldDif02:  difference f(t) and f(t-2), U32
*     rROCmbInf[9]: U32 x 9
*     nROW: (240 for 480i) */

uint pr_pd = 0;
module_param(pr_pd, uint, 0644);
MODULE_PARM_DESC(pr_pd, "/n printk /n");

/* if flmxx level > flmxx_first_num */
/* flmxx first: even when 2-2 3-2 detected */
unsigned int flmxx_first_num = 50;
module_param(flmxx_first_num, uint, 0644);
MODULE_PARM_DESC(flmxx_first_num,
"/n flmxx first: even when 2-2 3-2 detected /n");

/* if flmxx level > flmxx_maybe_num */
/* mabye flmxx: when 2-2 3-2 not detected */
unsigned int flmxx_maybe_num = 15;
module_param(flmxx_maybe_num, uint, 0644);
MODULE_PARM_DESC(flmxx_maybe_num,
"/n mabye flmxx: when 2-2 3-2 not detected /n");

int FlmVOFSftTop(UINT8 *rCmb32Spcl, unsigned short *rPstCYWnd0,
		 unsigned short *rPstCYWnd1, unsigned short *rPstCYWnd2,
		 unsigned short *rPstCYWnd3, unsigned short *rPstCYWnd4,
		 UINT8 *rFlmPstGCm, UINT8 *rFlmSltPre, UINT8 *rFlmPstMod,
		 UINT32 *rROFldDif01, UINT32 *rROFrmDif02, UINT32 *rROCmbInf,
		 struct sFlmSftPar *pPar, int nROW, int nCOL)
{
	static UINT32 DIF01[HISDIFNUM]; /* Last one is global */
	static UINT32 DIF02[HISDIFNUM]; /* Last one is global */
    /* Dif01 of 5th windows used for 2-2 */
	static UINT32 DifW5[HISDIFNUM];


	static struct sFlmDatSt pRDat;
	static int pre22lvl;

	char debug_str[256];

	/* replaced by pFlg32/pFlg22... */
	/* static UINT8 PREWV[HISCMBNUM];1-weaver with Previous field,
	0:with next */

	/* struct sFlmDatSt pRGlb; */

	int nDIF01[HISDIFNUM];
	int nDIF02[HISDIFNUM];
	/* UINT32 nCb32=0; */

	/* int nRCMB[ROWCMBNUM]; */
	int mDly = pPar->mPstDlyPre;
	int mNDly = pPar->mNxtDlySft;
	/* int rSChgNumRCmb = (nROW/2);
	Scene changed (the number of R-Combing) */
	/* int mF32CmbSpcl = 0; //combing special processing for 3-2 film */
	/* UINT32 sF32Dif02M0 = pPar->sF32Dif02M0; */
	/* UINT32 sF32Dif02M1 = pPar->sF32Dif02M1; */

	int nT0 = 0;
	int nT1 = 0;
	/* int nT2=0; */
	int nS0 = 0;
	int nS1 = 0;
	int nMod = 0;

	/* difference */
	pRDat.rROFrmDif02 = rROFrmDif02;
	/* size of the image */
	pRDat.iHeight = nROW; /* field height */
	pRDat.iWidth  = nCOL;

	/* Initialization */
	if (field_count == 0) {
		for (nT1 = 0; nT1 < HISDIFNUM; nT1++) {
			DIF01[nT1] = 0;
			DIF02[nT1] = 0;
			DifW5[nT1] = 0;
		}

		for (nT1 = 0; nT1 < HISDETNUM; nT1++) {
			pRDat.pFlg32[nT1] = 0;
			pRDat.pMod32[nT1] = 0;
			pRDat.mNum32[nT1] = 0;

			pRDat.pFld32[nT1] = 0;
			pRDat.pFrm32[nT1] = 0;
			pRDat.pFrm32t[nT1] = 0;

			pRDat.pFlg22[nT1] = 0;
			pRDat.pMod22[nT1] = 0;
			pRDat.mNum22[nT1] = 0;

			pRDat.pStp22[nT1] = 0;
			pRDat.pSmp22[nT1] = 0;

			/* HISDETNUM hist */
			pRDat.pFlgXx[nT1] = 0; /* pre-1, nxt-0 */
			pRDat.pMdLXx[nT1] = 0;  /* mode level */
		}
		field_count = 1;
	} else {
		for (nT1 = 1; nT1 < HISDETNUM; nT1++) {
			pRDat.mNum32[nT1 - 1] = pRDat.mNum32[nT1];
			pRDat.mNum22[nT1 - 1] = pRDat.mNum22[nT1];
		}
	}

	/* -------------------------------------------------------------- */
	nS0 = 0;
	nS1 = 0;
	/* int rROFrmDif02[6]; // Read only */
	/* int rROFldDif01[6]; // Read only */
	for (nT1 = 1; nT1 < HISDIFNUM; nT1++) {
		DIF01[nT1 - 1] = DIF01[nT1];
		DIF02[nT1 - 1] = DIF02[nT1];
		DifW5[nT1-1]   = DifW5[nT1];
	}

	DIF01[HISDIFNUM - 1] = rROFldDif01[0];	/* 5windows+global */
	DIF02[HISDIFNUM - 1] = rROFrmDif02[0];	/* 5windows+global */

	/* --------------------------------------------------------------- */
	/* int nDIF01[HISDIFNUM]; */
	/* int nDIF02[HISDIFNUM]; */
	for (nT1 = 0; nT1 < HISDIFNUM; nT1++) {
		nDIF01[nT1] = DIF01[nT1];
		nDIF02[nT1] = DIF02[nT1];
	}
	/* --------------------------------------------------------- */
	/* Film-Detection */
	nS1 = FlmDetSft(&pRDat, nDIF01, nDIF02, nT0, pPar);

	FlmModsDet(&pRDat, rROFldDif01[0], rROFrmDif02[0]);
	/* --------------------------------------------------------- */

	/* for panda 2-2 : flag	1, 3 */
	if (pRDat.pFlg22[HISDETNUM-1] &	0x1) {
		/* diff01 of the 5th window	*/
		/* for panda 2-2: VOF */
		DifW5[HISDIFNUM-1] =
			(16 * rROFldDif01[5]) / (rROFldDif01[0] + 16);
		nT1	= (DifW5[HISDIFNUM-1] << 2);
		if (nS1	> nT1)
			nS1	-= nT1;	/* reset the 5th window	*/

		pre22lvl = nS1;
	} else if (pRDat.pFlg22[HISDETNUM-1])
		nS1	= pre22lvl;
	else
		pre22lvl = 0;

	/* Current f(t-0) with previous */
	/* pFMReg->rCmb32Spcl =0; */
	*rCmb32Spcl = 0;
	if (pRDat.pMod32[HISDETNUM - 1] == 3) {
		nMod = pRDat.pMod32[HISDETNUM - 1];
		nT0 = pRDat.pFlg32[HISDETNUM - 1] % 2;

		if (pRDat.mNum32[HISDETNUM - 1] < 255)	/* maximum */
			pRDat.mNum32[HISDETNUM - 1] += 1;

		/* 3-2 film combing special precessing (A-A-A) */
		if (pRDat.pFlg32[HISDETNUM - 1] == (mNDly % 5))
			*rCmb32Spcl = 1;
	} else if (pRDat.pMod22[HISDETNUM - 1] == 2) {
		nMod = pRDat.pMod22[HISDETNUM - 1];
		nT0 = pRDat.pFlg22[HISDETNUM - 1] % 2;

		if (pRDat.mNum22[HISDETNUM - 1] < 255)	/* maximum */
			pRDat.mNum22[HISDETNUM - 1] += 1;
	} else {
		nMod = 0;

		pRDat.mNum32[HISDETNUM - 1] = 0;
		pRDat.mNum22[HISDETNUM - 1] = 0;
	}

	/* Only frame (t-1) */
	/* pFMReg->rFlmPstGCm = 0; */
	*rFlmPstGCm = 0;
	/* rFlmPstGCm = 1; */
	if (pRDat.pMod32[HISDETNUM - 1 - mDly] == 3) {
		nT0 = pRDat.pFlg32[HISDETNUM - 1 - mDly] % 2;
		*rFlmSltPre = nT0;
		/* Post-processing: film mode,00: global combing,
		01: 2-2 film, 10: 2-3 film, 11:-others */
		*rFlmPstMod = 2;
		/* param: at least 5 field+5 */
		if (pRDat.mNum32[HISDETNUM - 1] < 6) {
			*rFlmSltPre = 0;
			*rFlmPstMod = 0;
		}
		nS1 = 0;
	} else if (pRDat.pMod22[HISDETNUM - 1 - mDly] == 2) {
		nT0 = pRDat.pFlg22[HISDETNUM - 1 - mDly] % 2;
		*rFlmSltPre = nT0;
		/* Post-processing: film mode,
		00: global combing, 01: 2-2 film, 10: 2-3 film, 11:-others */
		*rFlmPstMod = 1;

		/* param: at least 60 field+4 */
		if (pRDat.mNum22[HISDETNUM - 1] < 60) {
			*rFlmSltPre = 0;
			*rFlmPstMod = 0;
		}
	} else {
		*rFlmSltPre = 0;
		/* Post-processing: film mode,00: global combing,
		01: 2-2 film, 10: 2-3 film, 11:-others */
		*rFlmPstMod = 0;
		nS1 = 0;
	}

	nT1 = pRDat.pMdLXx[HISDETNUM - 1 - mDly];
	if (((*rFlmPstMod == 0) && (nT1 > flmxx_maybe_num)) ||
		((*rFlmPstMod != 0) && (nT1 > flmxx_first_num))) {
		*rFlmSltPre = pRDat.pFlgXx[HISDETNUM - 1 - mDly];
		*rFlmPstMod = nT1;
		nS1 = 0;
	}

	VOFSftTop(rFlmPstGCm, rFlmSltPre, rFlmPstMod,
		rPstCYWnd0, rPstCYWnd1, rPstCYWnd2, rPstCYWnd3,
		nMod, rROCmbInf, &pRDat, pPar, nROW, nCOL);

	if (pr_pd & 0xff) {
		sprintf(debug_str, "Field#%5d:", field_count);
		if (pr_pd & 1) /* Field/Frame Diff */
			sprintf(debug_str+strlen(debug_str),
				"\tDif012=%9u,%9u",
				DIF01[HISDIFNUM - 1], DIF02[HISDIFNUM - 1]);
		if (pr_pd & 2) /* Pulldown Field # */
			sprintf(debug_str+strlen(debug_str),
				"\tmNum32=%3d, mNum22=%3d",
				pRDat.mNum32[HISDETNUM - 1],
				pRDat.mNum22[HISDETNUM - 1]);
		if (pr_pd % 4) /* Film mode and global combing */
			sprintf(debug_str+strlen(debug_str),
				"\tFlmPstMod=%1d, rFlmPstGCm=%1d",
				*rFlmPstMod, *rFlmPstGCm);
		sprintf(debug_str+strlen(debug_str), "\n");
		pr_info("%s", debug_str);
	}
	return nS1;
}

/* Film Detection Software implementation */
/* nDif01: Field Difference */
/* nDif02: Frame Difference */
/* WND: The index of Window */
int FlmDetSft(struct sFlmDatSt *pRDat, int *nDif01, int *nDif02,
	      int WND, struct sFlmSftPar *pPar)
{
	int nT0 = 0;
	/* 3-2 */
	Flm32DetSft(pRDat, nDif02, nDif01, pPar);

	/* Film2-2 Detection */
	/* debug0304 */
	nT0 = Flm22DetSft(pRDat, nDif02, nDif01, pPar);
	/* ---------------------------------------- */

	return nT0;
}

/* pFlm02[0:nLEN-1] : recursive, 0-2 dif */
/* pFlm01[0:nLEN-1] : recursive, 0-1 dif */
int Flm32DetSft(struct sFlmDatSt *pRDat, int *nDif02,
		int *nDif01, struct sFlmSftPar *pPar)
{
	int sFrmDifAvgRat = pPar->sFrmDifAvgRat;	/* 16;  //0~32 */
	/*  The Large Decision should be: (large>average+LgDifThrd) */
	int sFrmDifLgTDif = pPar->sFrmDifLgTDif; /* 4096 */
	int sF32StpWgt01 = pPar->sF32StpWgt01;	/* 15; */
	int sF32StpWgt02 = pPar->sF32StpWgt02;	/* 15; */
	int sF32DifLgRat = pPar->sF32DifLgRat;	/* 16; Dif>Rat*Min-->Larger */

	/* int sF32DifSmRat = 16;  //Dif*Rat<Max  --> Smaller */

	UINT8 *pFlm02 = pRDat->pFrm32;
	UINT8 *pFlm02t = pRDat->pFrm32t;
	UINT8 *pFlm01 = pRDat->pFld32;
	UINT8 *pFlg32 = pRDat->pFlg32;

	static UINT8 sFld32[6] = { 0, 0, 0, 0, 0, 0 };

	UINT8 FIX02[2][5] = { {2, 3, 4, 5, 1}, {4, 5, 1, 2, 3} };

	int nT0 = 0;
	int nT1 = 0;
	int nT2 = 0;

	int nMn = nDif02[HISDIFNUM - 1];
	int nMx = nDif02[HISDIFNUM - 1];
	int nSM = nDif02[HISDIFNUM - 1];
	UINT8 nFlg01[6] = { 0, 0, 0, 0, 0, 0 };
	UINT8 nFlg02[6] = { 0, 0, 0, 0, 0, 0 };
	UINT8 nFlg12[6] = { 0, 0, 0, 0, 0, 0 };

	int nAV10 = 0;
	int nAV11 = 0;
	int nAV12 = 0;
	int nAV1 = 0;
	int nSTP = 0;
	UINT8 nMIX = 0;

	int nFlgChk1 = 0;
	int nFlgChk2 = 0;
	int nFlgChk3 = 0; /* for Mit32VHLine */

	/* ---------------------------------- */
	/* Get min/max from the last fives */
	for (nT0 = 2; nT0 <= 5; nT0++) {
		nT1 = nDif02[HISDIFNUM - nT0];
		nSM += nT1;

		if (nT1 < nMn)
			nMn = nT1;

		if (nT1 > nMx)
			nMx = nT1;
	}
	nAV10 = ((nSM - nMx + 2) >> 2);
	nAV12 = ((nSM + 3 * nMn + 4) >> 3);

	nSM = 0;
	nT2 = 0;
	for (nT0 = 1; nT0 <= 5; nT0++) {
		nT1 = nDif02[HISDIFNUM - nT0];
		if (nT1 >= nAV10) {
			nT2 += 1;
			nSM += nT1;
		}
	}
	nAV11 = (nSM - nMx + nT2 / 2) / (nT2 - 1);

	nAV1 = (sFrmDifAvgRat * nAV11 + (32 - sFrmDifAvgRat) * nAV12);
	nAV1 = ((nAV1 + 16) >> 5);

	/* for Mit32VHLine */
	if (nDif02[HISDIFNUM-1] > nDif02[HISDIFNUM-6])
		nFlgChk1 = nDif02[HISDIFNUM-1] - nDif02[HISDIFNUM-6];
	else
		nFlgChk1 = nDif02[HISDIFNUM-6] - nDif02[HISDIFNUM-1];

	/* if (pFlg32[HISDETNUM-1] == 4) { */
	/* B-B A-A-A X-Y-Z */
	/* ---------=>Sceen changed */
	if (pFlg32[HISDETNUM-1] == 4 || pFlg32[HISDETNUM-1] == 5) {
		nFlgChk3 = nFlgChk1;
		for (nT0 = 2; nT0 <= 5; nT0++) {
			nFlgChk2 = nDif02[HISDIFNUM-nT0]
					- nDif02[HISDIFNUM-nT0-5];
			if (nFlgChk2 < 0)
				nFlgChk2 = -nFlgChk2;

			/* 5-loop: maximum */
			if (nFlgChk2 > nFlgChk3)
				nFlgChk3 = nFlgChk2;
		}
		nFlgChk3 = 16 * nFlgChk3 / nAV1;
	} else
		nFlgChk3 = 255;
	/* for Mit32VHLine */

	if (pFlg32[HISDETNUM - 1] == 2 || pFlg32[HISDETNUM - 1] == 4
		|| pFlg32[HISDETNUM - 1] == 3) {
		/* ========================================== */
		if (nDif02[HISDIFNUM - 2] > nDif02[HISDIFNUM - 7])
			nFlgChk2 =
			    nDif02[HISDIFNUM - 2] - nDif02[HISDIFNUM - 7];
		else
			nFlgChk2 =
			    nDif02[HISDIFNUM - 7] - nDif02[HISDIFNUM - 2];

		if (nFlgChk1 > nFlgChk2)
			nFlgChk2 = nFlgChk1 - nFlgChk2;
		else
			nFlgChk2 = nFlgChk2 - nFlgChk1;

		nFlgChk2 = 16 * nFlgChk2 / nAV1;
		/* ============================================ */

		/* please check the DI-skateboard */
		/* the next should be 1 and 3 */
		/* dif02(flg=2 vs 1) almost same */
		/* dif02(flg=4 vs 3) almost same */
		if (nDif02[HISDIFNUM - 1] > nDif02[HISDIFNUM - 2])
			nFlgChk1 =
			    nDif02[HISDIFNUM - 1] - nDif02[HISDIFNUM - 2];
		else
			nFlgChk1 =
			    nDif02[HISDIFNUM - 2] - nDif02[HISDIFNUM - 1];

		nFlgChk1 = 16 * nFlgChk1 / nAV1;

		if (pr_pd & 8) /* flag check */
			pr_info("\tnFlgChk1/2=(%2d,%2d)\n", nFlgChk1, nFlgChk2);
	} else {
		nFlgChk1 = 0;
		nFlgChk2 = 0;
	}
	/* ============================================= */

	nT2 = 5 * nDif02[HISDIFNUM - 1] / (nMn + sFrmDifLgTDif + 1);
	nT2 = nT2>>1;
	if (nMn <= 2048) {
		nSTP = nT2;
	} else {
		nSTP =
		    16 * (nDif02[HISDIFNUM - 1] - nMn) + (nAV1 - nMn +
							  sFrmDifLgTDif) / 2;
		nSTP = nSTP / (nAV1 - nMn + sFrmDifLgTDif);

		/* ======================== */
		/* patch for DI1 3:2, [ 16 16 9 16 0] */
		if (nT2 > nSTP)
			nSTP = nT2;
		/* ======================== */
	}

	if (nSTP > 16)
		nSTP = 16;

	for (nT0 = 1; nT0 < HISDETNUM; nT0++) {
		pFlm02[nT0 - 1] = pFlm02[nT0];
		pFlm02t[nT0 - 1] = pFlm02t[nT0];
	}

	if (nDif02[HISDIFNUM - 1] > (nMn + sFrmDifLgTDif) * sF32DifLgRat) {
		pFlm02t[HISDETNUM - 1] =
		    nDif02[HISDIFNUM - 1] / (nMn + sFrmDifLgTDif);
	} else {
		pFlm02t[HISDETNUM - 1] = nSTP;
	}

	pFlm02[HISDETNUM - 1] = nSTP;

	/* -------------------------------- */
	nMn = pFlm02[HISDETNUM - 1];
	nMIX = 5;
	for (nT0 = 0; nT0 < 6; nT0++) {
		nFlg02[5 - nT0] = pFlm02[HISDETNUM - 1 - nT0];
		if (nFlg02[5 - nT0] < nMn && nT0 <= 4) {
			nMn = nFlg02[5 - nT0];
			nMIX = 5 - nT0;
		}
	}
	nFlg02[nMIX] = 16 - nFlg02[nMIX];
	if (nMIX == 5)
		nFlg02[0] = 16 - nFlg02[0];

	/* -------------------------------------------- */
	/* field difference */
	/* pFlm01 */
	/* length of pFlm01/nDif01: [0:5]; */
	/* iDx: index of minimum dif02 ([0:5] */
	Cal32Flm01(pFlm01, nDif01, nMIX, pPar);
	for (nT0 = 0; nT0 < 6; nT0++) {
		if ((nT0 == FIX02[0][nMIX - 1]) || (nT0 == FIX02[1][nMIX - 1]))
			nFlg01[nT0] = pFlm01[HISDETNUM - 6 + nT0];
		else
			nFlg01[nT0] = 16 - pFlm01[HISDETNUM - 6 + nT0];
	}

	if (FIX02[0][nMIX - 1] == 5 || FIX02[1][nMIX - 1] == 5)
		nFlg01[0] = pFlm01[HISDETNUM - 6];

	/* A-A-A B-B C-C-C D-D E-E-E */
	/* 0-1-1 0-0 0-1-1 0-0 */
	for (nT0 = 0; nT0 < 6; nT0++) {

		if (nMIX == 1 && nT0 == 5) {
			nSTP =
			    sF32StpWgt02 * nFlg02[nT0] + (16 -
							  sF32StpWgt02) *
			    nFlg01[nT0];
		} else if (nMIX == 5 && nT0 == 0) {
			nSTP =
			    sF32StpWgt02 * nFlg02[nT0] + (16 -
							  sF32StpWgt02) *
			    nFlg01[nT0];
		} else if (nT0 == nMIX || nT0 == nMIX - 1) {
			nSTP =
			    sF32StpWgt02 * nFlg02[nT0] + (16 -
							  sF32StpWgt02) *
			    nFlg01[nT0];
		} else {
			nSTP =
			    sF32StpWgt01 * nFlg01[nT0] + (16 -
							  sF32StpWgt02) *
			    nFlg02[nT0];
		}

		nFlg12[nT0] = ((nSTP + 8) >> 4);
	}
	/* -------------------------------------------- */

	/* -------------------------------------------- */
	Flm32DetSub1(pRDat, nFlg12, pFlm02t, nFlg01, nFlg02, nMIX);
	/* -------------------------------------------- */

	/* 150213-patch */
	nSTP = pRDat->pFrm32[HISDETNUM - 6 + nMIX];
	for (nT1 = 5; nT1 > 0; nT1--) {
		if (nT1 != nMIX)
			nSTP += (16 - pRDat->pFrm32[HISDETNUM - 6 + nT1]);
	}

	if (nMn < 2048 && nSTP <= 2) {
		nSTP = 0;
		for (nT1 = 4; nT1 >= 0; nT1--) {
			if (sFld32[nT1] >= pRDat->pFld32[HISDETNUM - 6 + nT1]) {
				nSTP +=
				    (sFld32[nT1] -
				     pRDat->pFld32[HISDETNUM - 6 + nT1]);
			} else {
				nSTP +=
				    (pRDat->pFld32[HISDETNUM - 6 + nT1] -
				     sFld32[nT1]);
			}
		}

		if (nSTP <= 2) {
			pRDat->pMod32[HISDETNUM - 1] = 3;
			pRDat->pFlg32[HISDETNUM - 1] = nMIX;
		}
	}

	for (nT1 = 1; nT1 <= 5; nT1++)
		sFld32[nT1 - 1] = sFld32[nT1];

	sFld32[5] = pRDat->pFld32[HISDETNUM - 5];
	/* -------------------------------------------- */

	/* ============================================= */
	/* please check the DI-skateboard */
	/* the next should be 1 and 3 */
	/* dif02(flg=2 vs 1) almost same */
	/* dif02(flg=4 vs 3) almost same */
	/* nFlgChk3: for Mit32VHLine */
	/* last: for sceen change */
	if ((nFlgChk1 > 25 && nFlgChk3 > 8) || nFlgChk2 > 16
		|| (pFlg32[HISDETNUM-1] == 4 && nFlgChk3 > 16)) {
		pRDat->pMod32[HISDETNUM - 1] = 0;
		pRDat->pFlg32[HISDETNUM - 1] = 0;
	}
	/* ============================================= */

	return 0;
}

/* length of pFlm01/nDif01: [0:5]; */
/* iDx: index of minimum dif02 ([0:5] */
int Cal32Flm01(UINT8 *pFlm01, int *nDif01, int iDx,
		struct sFlmSftPar *pPar)
{
	int sF32Dif01A1 = pPar->sF32Dif01A1;	/* 65; */
	int sF32Dif01T1 = pPar->sF32Dif01T1;	/* 128; */

	int sF32Dif01A2 = pPar->sF32Dif01A2;	/* 65; */
	int sF32Dif01T2 = pPar->sF32Dif01T2;	/* 128; */

	int dDif05[5]; /* patch for MIT32-Line */

	int nT0 = 0;
	int nT1 = 0;
	/* int nT2=0; */
	int nSP = 0;

	int CP = nDif01[HISDIFNUM - 1];	/* Last */
	int PP = nDif01[HISDIFNUM - 2];	/* Prev */

	int nMn = ((CP < PP) ? CP : PP);
	int nMx = ((CP > PP) ? CP : PP);

	for (nT0 = 0; nT0 < HISDETNUM - 1; nT0++)
		pFlm01[nT0] = pFlm01[nT0 + 1];

	nSP = nDif01[HISDIFNUM-1];
	for (nT0 = 0; nT0 < 5; nT0++) {
		if (nDif01[HISDIFNUM-1-nT0] >= nDif01[HISDIFNUM-6-nT0])
			dDif05[nT0] = nDif01[HISDIFNUM-1-nT0]
				- nDif01[HISDIFNUM-6-nT0];
		else
			dDif05[nT0] = nDif01[HISDIFNUM-6-nT0]
				- nDif01[HISDIFNUM-1-nT0];

		if (nDif01[HISDIFNUM-1-nT0] < nSP)
			nSP = nDif01[HISDIFNUM-1-nT0];
	}

	for (nT0 = 0; nT0 < 5; nT0++)
		dDif05[nT0] = 8 * dDif05[nT0] / (nSP + 1024);


	if (iDx == 5) {
		/* Last three */
		if (nDif01[HISDIFNUM - 3] > nMx)
			nMx = nDif01[HISDIFNUM - 3];

		if (nDif01[HISDIFNUM - 3] < nMn)
			nMn = nDif01[HISDIFNUM - 3];

		nSP = 16 * (CP - nMn) + (nMx - nMn) / 2;
		nSP = nSP / (nMx - nMn + 1);
		if (nSP > 16)
			nSP = 16;

		pFlm01[HISDETNUM - 1] = nSP;
	} else if (iDx == 4) {
		nT0 = sF32Dif01T1 + ((CP * sF32Dif01A1 + 32) >> 6); /* x/64 */
		nT1 = sF32Dif01T1 + ((PP * sF32Dif01A1 + 32) >> 6); /* x/64 */

		if (nT0 <= PP) {
			pFlm01[HISDETNUM - 1] = 0;
			pFlm01[HISDETNUM - 2] = 16;
		} else if (nT1 <= CP) {
			pFlm01[HISDETNUM - 2] = 0;
			pFlm01[HISDETNUM - 1] = 16;
		} else {
			pFlm01[HISDETNUM - 2] = 8;	/* overlap */
			pFlm01[HISDETNUM - 1] = 8;	/* overlap */
		}
	} else {
		nT0 = sF32Dif01T2 + ((CP * sF32Dif01A2 + 32) >> 6); /* x/64 */
		nT1 = sF32Dif01T2 + ((PP * sF32Dif01A2 + 32) >> 6); /* x/64 */

		if (nT0 <= PP)
			pFlm01[HISDETNUM - 1] = 0;
		else if (nT1 <= CP)
			pFlm01[HISDETNUM - 1] = 16;
		else
			pFlm01[HISDETNUM - 1] = 8;	/* overlap */
	}

	nSP = dDif05[0];
	for (nT0 = 1; nT0 < 5; nT0++) {
		if (nSP < dDif05[nT0])
			nSP = dDif05[nT0]; /* maximum */
	}
	if (nSP <= 3) {
		if (iDx == 2) {
			pFlm01[HISDETNUM-1] = 16;
			pFlm01[HISDETNUM-2] = 0;
		} else if (iDx == 1) {
			pFlm01[HISDETNUM-1] = 0;
			pFlm01[HISDETNUM-2] = 16;
		}
	}


	return 0;
}

/* length: [0:5] */
/* MIX: [1~5] */
int Flm32DetSub1(struct sFlmDatSt *pRDat, UINT8 *nFlg12, UINT8 *pFlm02t,
		 UINT8 *nFlg01, UINT8 *nFlg02, UINT8 MIX)
{
	UINT8 *pFlg = pRDat->pFlg32;
	UINT8 *pMod = pRDat->pMod32;

	int CFg = pFlg[HISDETNUM - 1];
	int RFlg[5] = { 5, 1, 2, 3, 4 };
	int nT0 = 0;
	int CNT = 0;

	UINT8 MN0 = nFlg12[5];
	UINT8 MN1 = nFlg01[5];
	UINT8 MN2 = nFlg02[5];

	int ID0 = 0;
	int ID1 = 0;

	for (nT0 = 5; nT0 >= 0; nT0--) {
		if (pMod[nT0] == 3)
			CNT++;
		else
			break;
	}

	for (nT0 = 0; nT0 < HISDETNUM - 1; nT0++) {
		pMod[nT0] = pMod[nT0 + 1];
		pFlg[nT0] = pFlg[nT0 + 1];
	}

	for (nT0 = 0; nT0 < 5; nT0++) {
		if (nFlg12[nT0] < MN0)
			MN0 = nFlg12[nT0];

		if (nFlg01[nT0] < MN1)
			MN1 = nFlg01[nT0];

		if (nFlg02[nT0] < MN2)
			MN2 = nFlg02[nT0];
	}

	if (CFg == 0 && MN0 >= 10 && MN1 >= 10 && MN2 >= 10) {
		pMod[HISDETNUM - 1] = 3;
		pFlg[HISDETNUM - 1] = MIX;
	} else if (CFg != 0 && RFlg[CFg - 1] == MIX && MN0 >= 8) {
		pMod[HISDETNUM - 1] = 3;
		pFlg[HISDETNUM - 1] = MIX;
		if (CNT <= 2 && (MN1 <= 8 || MN2 <= 8))
			pFlg[HISDETNUM - 1] = 0;
	} else {
		pMod[HISDETNUM - 1] = 0;
		pFlg[HISDETNUM - 1] = 0;

		MN0 = pFlm02t[HISDETNUM - 1];
		MN1 = pFlm02t[HISDETNUM - 1];
		ID0 = 5;
		ID1 = 5;
		for (nT0 = 4; nT0 >= 1; nT0--) {
			if (pFlm02t[HISDETNUM - 6 + nT0] < MN0) {
				MN1 = MN0;
				ID1 = ID0;
				MN0 = pFlm02t[HISDETNUM - 6 + nT0];
				ID0 = nT0;
			} else if (pFlm02t[HISDETNUM - 6 + nT0] < MN1) {
				MN1 = pFlm02t[HISDETNUM - 6 + nT0];
				ID1 = nT0;
			}
		}

		MIX = ID0;
		if (ID0 == 5 && ID1 == 5) {
			if (pFlm02t[HISDETNUM - 6] > MN0)
				MN0 = pFlm02t[HISDETNUM - 6];

			CNT = 0;
			for (nT0 = 1; nT0 <= 4; nT0++) {
				if (pFlm02t[HISDETNUM - 6 + nT0] >
				    (MN0 + 2) * 32) {
					CNT++;
				}
			}

			if (CNT == 4) {
				if (CFg == 0) {
					pMod[HISDETNUM - 1] = 3;
					pFlg[HISDETNUM - 1] = MIX;
				} else if (RFlg[CFg - 1] == MIX) {
					pMod[HISDETNUM - 1] = 3;
					pFlg[HISDETNUM - 1] = MIX;
				}
			}
		} else if (MN1 > (MN0 + 2) * 32) {
			/* All >64 */
			if (ID0 != 5) {
				if (CFg == 0) {
					pMod[HISDETNUM - 1] = 3;
					pFlg[HISDETNUM - 1] = MIX;
				} else if (RFlg[CFg - 1] == MIX) {
					pMod[HISDETNUM - 1] = 3;
					pFlg[HISDETNUM - 1] = MIX;
				}
			}	/* if(ID0!=5) */
		}		/* if(MN1>(MN0+2)*32) */
	}

	return 0;
}

/* Film2-2 Detection */
int Flm22DetSft(struct sFlmDatSt *pRDat, int *nDif02,
			int *nDif01, struct sFlmSftPar *pPar)
{
	UINT8 *pFlg = pRDat->pFlg22;
	UINT8 *pMod = pRDat->pMod22;

	UINT8 *pStp = pRDat->pStp22;
	UINT8 *pSmp = pRDat->pSmp22;
	UINT8 *mNum22 = pRDat->mNum22;

	int sFlm2MinAlpha = pPar->sFlm2MinAlpha;	/* 32; // [0~63] */
	int sFlm2MinBelta = pPar->sFlm2MinBelta;	/* 32; // [0~63] */
	int sFlm20ftAlpha = pPar->sFlm20ftAlpha;	/* 16; // [0~63] */
	int sFlm2LgDifThd = pPar->sFlm2LgDifThd;	/* 4096; */
	int sFlm2LgFlgThd = pPar->sFlm2LgFlgThd;	/* 8; */

	int cFlg = pFlg[HISDETNUM - 1];
	int rFlg[4] = { 2, 3, 4, 1 };

	int nT0 = 0;
	int nT1 = 0;
	/* int nT2=0; */
	int CNT0 = 0;
	int CNT1 = 0;

	int nMn = nDif01[HISDIFNUM - 1];
	int nMx = nDif01[HISDIFNUM - 1];

	int nSM20 = nDif01[HISDIFNUM - 1];
	int nSM21 = 0;
	int nSM22 = 0;
	int nL21 = 0;
	int nL22 = 0;
	int Mx56 = 0;
	int Mn56 = 0;

	int nAV20 = 0;
	int nAV21 = 0;
	int nAV22 = 0;
	int nOfst = 0;
	int tMgn = 0;
	int BtMn = 0;

	int FdTg[6];

	int nFlgChk1 = 0; /* chk1 */
	int nFlgCk20 = 0; /* chk2-0 */
	int nFlgCk21 = 0; /* chk2-1 */
	int nFlgChk3 = 0; /* chk3 */
	int nFlgChk4 = 0; /* chk4 */
	int nFlgChk5 = 0; /* chk5 */
	int nFlgChk6 = 0; /* dif02-small */

	static UINT8 nCk20Cnt;
	static UINT8 nCk21Cnt;
	/* check 2-2: for panda sequence */
	static UINT8 nCk22Flg[HISDETNUM];
	static UINT8 nCk22Cnt;

	/* size of image */
	int iWidth  = pRDat->iWidth;
	int iHeight = pRDat->iHeight;
	int nFlm22Lvl = 0;

	for (nT0 = 0; nT0 < HISDETNUM - 1; nT0++) {
		pFlg[nT0] = pFlg[nT0 + 1];
		pMod[nT0] = pMod[nT0 + 1];
		pStp[nT0] = pStp[nT0 + 1];
		pSmp[nT0] = pSmp[nT0 + 1];
		nCk22Flg[nT0] = nCk22Flg[nT0+1];
	}



	/* ========== check1/3 2-2 mode  ========== */
	/* |dif02(t-1) - dif02(t-0)| => should be small */
	/* |dif01(t-1) - (dif01(t)+dif02(t))| => should be small */
	nFlgChk1 = 0;
	nFlgChk3 = 0;
	nAV20 = 0;
	if (pFlg[HISDETNUM-1] == 0) {
		nFlgChk1 = 255;
		nFlgChk3 = 255;
		nFlgChk4 = 0;
		nCk22Cnt = 0;
	} else if (pFlg[HISDETNUM-1] == 2
			|| pFlg[HISDETNUM-1] == 4) {
		for (nT0 = 1; nT0 <= 7; nT0 = nT0+2) {
			if (nDif02[HISDIFNUM-nT0] > nDif02[HISDIFNUM-nT0-1]) {
				nOfst = nDif02[HISDIFNUM-nT0]
					- nDif02[HISDIFNUM-nT0-1];
				nAV20 = nAV20 + nDif02[HISDIFNUM-nT0-1];
			} else {
				nOfst = nDif02[HISDIFNUM-nT0-1]
					- nDif02[HISDIFNUM-nT0];
				nAV20 = nAV20 + nDif02[HISDIFNUM-nT0];
			}

			/* maximum */
			if (nOfst > nFlgChk1)
				nFlgChk1 = nOfst;

			tMgn = nDif02[HISDIFNUM-nT0]+nDif01[HISDIFNUM-nT0];
			if (tMgn > nDif01[HISDIFNUM-nT0-1])
				BtMn = tMgn - nDif01[HISDIFNUM-nT0-1];
			else
				BtMn = nDif01[HISDIFNUM-nT0-1] - tMgn;

			if (BtMn > nFlgChk3)
				nFlgChk3 = BtMn;

		}
		nAV20 = nAV20>>2;
		nFlgChk1 = 16*nFlgChk1/(nAV20+1024);
		nFlgChk3 = 16*nFlgChk3/(nAV20+1024);
	} else {
		nFlgChk1 = 0;
		nFlgChk3 = 0;

		for (nT0 = 1; nT0 <= 8; nT0 = nT0 + 2) {
			if (nDif02[HISDIFNUM-nT0] > nDif01[HISDIFNUM-nT0])
				nFlgChk4++;
		}
	}
	/* ========== check1/3 2-2 mode  ========== */

	for (nT0 = 2; nT0 <= 4; nT0++) {
		nT1 = nDif01[HISDIFNUM - nT0];

		nSM20 += nT1;

		if (nMx < nT1)
			nMx = nT1;

		if (nMn > nT1)
			nMn = nT1;
	}
	/* sum(0~5), last-6 */
	nSM20 += (nDif01[HISDIFNUM - 5] + nDif01[HISDIFNUM - 6]);
	nAV20 = (nSM20 - nMx + 2) / 5;

	nFlgChk6 = 0;
	for (nT0 = 1; nT0 <= 6; nT0++) {
		nT1 = nDif01[HISDIFNUM - nT0];
		if (nT1 >= nAV20) {
			nL21 += 1;
			nSM21 += nT1;
		}

		if (nT1 <= nAV20) {
			nL22 += 1;
			nSM22 += nT1;
		}
		nFlgChk6 += (nDif02[HISDIFNUM-nT0] >> 8);
	}
	nFlgChk6 = nFlgChk6 / 6;

	nAV21 = (nSM21 + nL21 / 2) / nL21;	/* High average */
	nAV22 = (nSM22 + nL22 / 2) / nL22;	/* Low average */
	nOfst = nAV21 - nAV22;

	/* ========== check2 2-2 mode  ========== */
	/* |dif01(t-0) - dif01(t-2)| => should be small */
	/* |dif01(t-0) - dif01(t-4)| => should be small */
	nFlgCk20 = 0;
	nFlgCk21 = 0;
	for (nT0 = 1; nT0 <= 4; nT0++) {
		nOfst = nDif01[HISDIFNUM-nT0] - nDif01[HISDIFNUM-nT0-2];
		if (nOfst < 0)
			nOfst = -nOfst;
		if (nOfst > nFlgCk20) /* maximum */
			nFlgCk20 = nOfst;

		nOfst = nDif01[HISDIFNUM-nT0] - nDif01[HISDIFNUM-nT0-4];
		if (nOfst < 0)
			nOfst = -nOfst;
		if (nOfst > nFlgCk21) /* maximum */
			nFlgCk21 = nOfst;
	}
	nFlgCk20 = 16*nFlgCk20/(nAV22+1024);
	nFlgCk21 = 16*nFlgCk21/(nAV22+1024);

	if (nFlgCk20 < 6) {
		if (nCk20Cnt < 255)
			nCk20Cnt++;
	} else
		nCk20Cnt = 0;

	if (nFlgCk21 < 6) {
		if (nCk21Cnt < 255)
			nCk21Cnt++;
	} else
		nCk21Cnt = 0;

	/* ========== check2 2-2 mode  ========== */

	/* ------------------------------ */
	/* Max or min of (5/6) */
	if (nDif01[HISDIFNUM - 1] > nDif01[HISDIFNUM - 2]) {
		Mx56 = nDif01[HISDIFNUM - 1];
		Mn56 = nDif01[HISDIFNUM - 2];
	} else {
		Mx56 = nDif01[HISDIFNUM - 2];
		Mn56 = nDif01[HISDIFNUM - 1];
	}
	/* ------------------------------ */

	CNT0 = 0;
	for (nT0 = 5; nT0 >= 0; nT0--) {
		if (pMod[HISDETNUM - 6 + nT0] == 2)
			CNT0++;
		else
			break;
	}

	if (CNT0 >= 1) {
		sFlm20ftAlpha = sFlm20ftAlpha - ((CNT0 * 25 + 8) >> 4);
		sFlm2MinBelta = sFlm2MinBelta - ((CNT0 * 36 + 8) >> 4);
	}

	/* water girl: part-2 */
	if (nCk21Cnt < 10) {
		nT0 = sFlm2MinAlpha*Mn56+sFlm20ftAlpha*nOfst;
		tMgn = ((nT0+16)>>5);

		nT1 = sFlm2MinBelta*Mn56;
		BtMn = ((nT1+32)>>6);

		/* ----------------------------------- */
		/* int *pStp = pRDat->pStp22; */
		/* int *pSmp = pRDat->pSmp22; */
		nT0 = 16*(Mx56-tMgn) + (BtMn+sFlm2LgDifThd)/2;
		nT1 = nT0/(BtMn+sFlm2LgDifThd);
		if (nT1 > 16)
			nT1 = 16;
		else if (nT1 < 0)
			nT1 = 0;
	} else {
		nT1 = 16;
	}

	pStp[HISDETNUM - 1] = nT1;
	if (Mx56 == nDif01[HISDIFNUM - 1])
		pSmp[HISDETNUM - 1] = 1;
	else
		pSmp[HISDETNUM - 1] = 0;

	/* ------------------------------------ */
	CNT0 = 0;
	CNT1 = 0;
	for (nT0 = 0; nT0 < 6; nT0++) {
		if (pStp[HISDETNUM - 6 + nT0] >= sFlm2LgFlgThd) {
			FdTg[CNT0] = nT0;
			CNT0++;
		}
		CNT1 += pSmp[HISDETNUM - 6 + nT0];
	}

	nT0 = 0;
	nT1 = 0;
	nFlgChk5 = 0;
	if (CNT0 == 6 && CNT1 == 3) {
		if (pSmp[HISDETNUM - 6] && pSmp[HISDETNUM - 4] &&
			pSmp[HISDETNUM - 2]) {
			nT1 = 2;
			if (cFlg != 0) {
				if (cFlg % 2 == 0)
					nT0 = rFlg[cFlg - 1];
			} else {
				nT0 = 1;
			}
		} else if (pSmp[HISDETNUM - 5] && pSmp[HISDETNUM - 3] &&
			pSmp[HISDETNUM - 1]) {	/* All 1 */
			nT1 = 2;
			if (cFlg != 0) {
				if (cFlg % 2 == 1)
					nT0 = rFlg[cFlg - 1];
			} else {
				nT0 = 4;
			}
		}

		/* --------------------------------------- */
		/* patch for toilet paper */
		/* Low average avg>(totoal*x) x>1 */
		/* tMgn = (nAV22 * 64) >> 8; */
		/* if(tMgn > 720*240) */
		/* if(tMgn > iWidth*iHeight*32) */ /*toilet paper*/
		/* parameter */
		nOfst = nAV21 - nAV22;
		nL22 = iWidth * iHeight + 1;
		if (nAV22 > (nL22 << 4)) {/* low average */
			/* nT0 = 0; */
			/* nT1 = 0; */
			nFlgChk5 = 16;
		} else if ((nAV22<<4) > (nL22 * 50)) {
			/* nT0 = 0; */
			/* nT1 = 0; */
			nFlgChk5 = nAV22 / nL22;
		} else if (nOfst < ((nAV22 * 46) >> 7)) {
			/* nT0=0; */
			/* nT1=0; */
			nFlgChk5 = (nAV22 << 2) / (nOfst + 32);
		}
		if (nFlgChk5 > 32)
			nFlgChk5 = 32;
		/* --------------------------------------- */

		nL22 = (nL22 >> 9) + 1;
		if (nFlgChk6 < nL22)
			nFlgChk6 = nL22 / (nFlgChk6 + 1);
		else if (nFlgChk6 > (nL22 << 1))
			nFlgChk6 = nFlgChk6 / nL22;
		else
			nFlgChk6 = 0;

		if (nFlgChk6 > 20)
			nFlgChk6 = 20;
	}
	pFlg[HISDETNUM - 1] = nT0;
	pMod[HISDETNUM - 1] = nT1;
	/* ----------------------------------- */

	/* for panda */
	/* check bug */
	if (pFlg[HISDETNUM-1] & 0x1) {
		nCk22Flg[HISDETNUM-1] = nT0;

		if (nT0 == 0)
			nCk22Cnt = nCk22Cnt+1;
		else
			nCk22Cnt = 0;

		if (nCk22Cnt > 254)
			nCk22Cnt = 254;
	}

	/* debug 2-2 mode */
	/* if(pr_pd && (nT0 != 0) && pFlg[HISDETNUM-1]!=0) { */
	if (pr_pd && (pFlg[HISDETNUM-1] & 0x1)) {
		if (nT0 != 0) {
			pr_info("2-2: nCk1/3/4=(%2d,%2d,%2d)\n",
				nFlgChk1, nFlgChk3, nFlgChk4);
			pr_info("2-2: nCk20/1Cnt=(%2d,%2d)\n",
				nCk20Cnt, nCk21Cnt);
			pr_info("2-2: nT0=(%d)\n", nT0);
		} else if (nCk22Flg[HISDETNUM-2] != 0)
			pr_info("2-2: nCk22Flg 1==>0\n");
	}

	/* ========== check2 2-2 mode  ========== */
	nFlm22Lvl	= (mNum22[HISDETNUM-1] >> 2);
	if (nFlm22Lvl	> 64)
		nFlm22Lvl	= 64;

	/* panda */
	if (pFlg[HISDETNUM-1] && nCk22Cnt < 20)
		nFlm22Lvl = nFlm22Lvl + nCk22Cnt - 20;

	/* 2-2 but with combing: force dejaggies */
	/* return information */
	if (nFlgChk1 < 61) {
		nT1 = ((61 - nFlgChk1) >> 2);
		nFlm22Lvl += nT1;
	}

	nT1 = ((nCk20Cnt > nCk21Cnt) ? nCk20Cnt : nCk21Cnt);
	if (nT1 > 128)
		nT1 = 128;
	nT1 = ((nT1 + 4) >> 3);
	nFlm22Lvl += nT1;

	if (nFlgChk3 < 140) {
		nT1 = ((140 - nFlgChk3) >> 3);
		nFlm22Lvl += nT1;
	}

	if (nFlgChk4 < 128) {
		nT1 = ((128 - nFlgChk4) >> 3);
		nFlm22Lvl += nT1;
	}

	/* for sony-mp3 */
	if ((nFlgChk1 > 32) || (nFlgChk4 > 80)) {
		if (nFlgChk1 > (nFlgChk4 + 48))
			nT1 = nFlgChk1 - 32;
		else
			nT1 = nFlgChk4 - 80;

		if (nT1 > 128)
			nT1 = 128;

		nT1 = (nT1>>2);

		nFlm22Lvl -= nT1;
	}
	/* for sony-mp3 */

	nFlm22Lvl -= nFlgChk5;
	nFlm22Lvl -= nFlgChk6;

	if (nFlm22Lvl < 0)
		nFlm22Lvl = 0;

	return nFlm22Lvl;
}

int GetMaxNIdx(int *nMax4, int *nXId4, int N, int *nQt01, int nLen)
{
	int nT1 = 0;
	int nT3 = 0;
	int nT4 = 0;
	int nTmp = 0;

	for (nT1 = 0; nT1 <	N; nT1++) {
		nMax4[nT1] = 0;
		nXId4[nT1] = 0;
	}

	for (nT1 = 0; nT1 <	nLen; nT1++) {
		nTmp = nQt01[nT1];

		/* maximum */
		for (nT3 = 0; nT3 < N; nT3++) {
			if (nTmp > nMax4[nT3]) {
				for (nT4 = 3; nT4 >= nT3+1; nT4--) {
					nMax4[nT4] = nMax4[nT4-1];
					nXId4[nT4] = nXId4[nT4-1];
				}
				nMax4[nT3] = nTmp;
				nXId4[nT3] = nT1;
				break;
			}
		}
	}
	return 0;
}

int GetMinNIdx(int *nMax4, int *nXId4, int N, int *nQt01, int nLen)
{
	int nT1 = 0;
	int nT3 = 0;
	int nT4 = 0;
	int nTmp = 0;

	for (nT1 = 0; nT1 <	N; nT1++) {
		nMax4[nT1] = 17;
		nXId4[nT1] = 0;
	}

	for (nT1 = 0; nT1 <	nLen; nT1++) {
		nTmp = nQt01[nT1];

		/* minimum */
		for (nT3 = 0; nT3 < N; nT3++) {
			if (nTmp < nMax4[nT3]) {
				for (nT4 = 3; nT4 >= nT3+1; nT4--) {
					nMax4[nT4] = nMax4[nT4-1];
					nXId4[nT4] = nXId4[nT4-1];
				}
				nMax4[nT3] = nTmp;
				nXId4[nT3] = nT1;
				break;
			}
		}
	}
	return 0;
}

/* 15: 8-7 */
/* 12: 3-2-3-2-2 */
/* 10: 6-4 */
/* 10: 5-5 */
/* 10: 2-2-2-4 */
/* 10: 2-3-3-2 */
/* 10: 3-2-3-2 */
/* pulldown pattern number */
#define PD_PT_NUM 7
/* define outside function, for reslove compile warn */
static int nQt01[15];
int	FlmModsDet(struct sFlmDatSt *pRDat, int nDif01, int nDif02)
{
	int iWidth  = pRDat->iWidth;
	int iHeight = pRDat->iHeight;
	int iMxDif  = (iWidth * iHeight >> 6);
	int nPrtLog[PD_PT_NUM] = {87, 32322, 64, 55, 2224, 2332, 3232};

	/* HISDETNUM hist */
	UINT8 *pFlgXx = pRDat->pFlgXx; /* pre-1, nxt-0 */
	UINT8 *pMdLXx = pRDat->pMdLXx;  /* mode level */

	static unsigned int sModFlg01[PD_PT_NUM]; /* flags */
	static unsigned int sModFlg02[PD_PT_NUM]; /* flags */
	static int nModCnt[PD_PT_NUM]; /* mode counter */

	unsigned int tModFlg01[PD_PT_NUM]; /* current flags */
	unsigned int tModFlg02[PD_PT_NUM]; /* current flags */

	int nMxMn[PD_PT_NUM][2] = { {2, 4}, {-2, -2}, {2, 4},
	{2, 4}, {4, -2}, {4, -2}, {4, -2} };

	int nModLvl[PD_PT_NUM] = {0, 0, 0, 0, 0, 0, 0}; /* mode level */
	/* int nQt01[15]; *//* Make this define for resolve cmpl warn */
	int nQt02[15];

	int aMax01[15]; /* maximum 4 */
	int aXId01[15];
	int aMin01[15]; /* minimum 4 */
	int aNId01[15];
	int aXMI01[15];

	int aMax02[15]; /* maximum 4 */
	int aXId02[15];
	int aMin02[15]; /* minimum 4 */
	int aNId02[15];
	int aXMI02[15];

	int	nT0	= 0;
	int	nT1	= 0;
	int	nT2	= 0;
	int	nT3	= 0;
	int	nT4	= 0;

	int	tT0	= 0;
	int	tT1	= 0;
	int	tT2	= 0;
	int	tT3	= 0;

	unsigned int uT01 = 0;
	unsigned int uT02 = 0;
	unsigned int uT03 = 0;

	int	nS01[PD_PT_NUM]	= {0, 0, 0,	0, 0, 0, 0};
	int	nS02[PD_PT_NUM]	= {0, 0, 0,	0, 0, 0, 0};
	int	nStp[PD_PT_NUM]	= {15, 12, 10, 10, 10, 10};

	static int	pDif01[30];
	static int	pDif02[30];

	int nLen1 = 0;
	int nLen2 = 0;

	int	nMin01 = 0;
	int	nMax01 = 0;
	int	nMin02 = 0;
	int	nMax02 = 0;
	int tModLvl = 0;
	iMxDif = (iMxDif >> 4);

	for (nT0 = 1; nT0 < HISDETNUM; nT0++) {
		pFlgXx[nT0 - 1] = pFlgXx[nT0];
		pMdLXx[nT0 - 1] = pMdLXx[nT0];
	}

	for	(nT0 = 0; nT0 <	29;	nT0++) {
		pDif01[nT0]	= pDif01[nT0 + 1];
		pDif02[nT0]	= pDif02[nT0 + 1];
	}
	pDif01[29] = (nDif01 >> 6);
	pDif02[29] = (nDif02 >> 6);

	for	(nT0 = 0; nT0 <	3; nT0++) {
		nT2	= nStp[nT0];

		nT3	= pDif01[29];
		nT4	= pDif02[29];

		nMin01 = nT3;
		nMax01 = nT3;
		nMin02 = nT4;
		nMax02 = nT4;

		nT3 = pDif01[29] - pDif01[29 - nT2];
		nT4 = pDif02[29] - pDif02[29 - nT2];
		if (nT3 < 0)
			nT3 = -nT3;
		if (nT4 < 0)
			nT4 = -nT4;
		nS01[nT0] =	nT3;
		nS02[nT0] =	nT4;

		/* nS01, nS02: sum of difference */
		for	(nT1 = 1; nT1 <	nT2; nT1++)	{
			nT3	= pDif01[29	- nT1];
			nT4	= pDif02[29	- nT1];

			if (nT3 > nMax01)
				nMax01 = nT3;
			if (nT3 < nMin01)
				nMin01 = nT3;

			if (nT4 > nMax02)
				nMax02 = nT4;
			if (nT4 < nMin02)
				nMin02 = nT4;

			/* diff max */
			nT3 = pDif01[29	- nT1] - pDif01[29 - nT1 - nT2];
			nT4	= pDif02[29	- nT1] - pDif02[29 - nT1 - nT2];
			if (nT3 < 0)
				nT3 = -nT3;
			if (nT4 < 0)
				nT4 = -nT4;
			if (nT3 > nS01[nT0])
				nS01[nT0] = nT3;
			if (nT4 > nS02[nT0])
				nS02[nT0] = nT4;
		}

		for (nT1 = 0; nT1 <	nT2; nT1++)	{
			nT3 = pDif01[29 - nT1] - nMin01;
			nT4 = nMax01 - nMin01 + 32;
			nT3 = (16 * nT3) + (nT4 / 2);
			nQt01[nT1] = (nT3 / nT4);

			nT3 = pDif02[29 - nT1] - nMin02;
			nT4 = nMax02 - nMin02 + 32;
			nT3 = (16 * nT3) + (nT4 / 2);
			nQt02[nT1] = (nT3 / nT4);
		}

		if (nT0 == 2)
			tT2 = PD_PT_NUM - 2;
		else
			tT2 = 1;

		for (tT1 = 0; tT1 < tT2; tT1++) {
			tT0	= nT0 + tT1;

			tModLvl = ((nModCnt[tT0] + 2) >> 2);
			if (tModLvl > 64)
				tModLvl = 64;

			if (nS01[nT0] > nMax01)
				nT3 = 8;
			else {
				nT3 = (nS01[nT0] << 3);
				nT3 = nT3 + (nMax01 >> 1);
				nT3 = nT3 / (nMax01 + 1);
				if (nT3 > 8)
					nT3 = 8;
			}
			tModLvl -= nT3;

			if (nS02[nT0] > nMax02)
				nT4 = 8;
			else {
				nT4 = (nS02[nT0] << 3);
				nT4 = nT4 + (nMax02 >> 1);
				nT4 = nT4 / (nMax02 + 1);
				if (nT4 > 8)
					nT4 = 8;
			}
			tModLvl -= nT4;

			if (nMxMn[tT0][0] > 0) {
				nLen1 = nMxMn[tT0][0];
				GetMaxNIdx(aMax01, aXId01, nLen1, nQt01, nT2);

				nT4 = 0;
				for (tT3 = 0; tT3 < nLen1; tT3++) {
					aXMI01[tT3] = aXId01[tT3];
					nT4 += (16 - aMax01[tT3]);
				}
				nT4 /= nLen1;
				tModLvl -= nT4;
			} else {
				nLen1 = -nMxMn[tT0][0];
				GetMinNIdx(aMin01, aNId01, nLen1, nQt01, nT2);

				nT4 = 0;
				for (tT3 = 0; tT3 < nLen1; tT3++) {
					aXMI01[tT3] = aNId01[tT3];
					nT4 += aMin01[tT3];
				}
				nT4 /= nLen1;
				tModLvl -= nT4;
			}

			if (nMxMn[tT0][1] > 0) {
				nLen2 = nMxMn[tT0][1];
				GetMaxNIdx(aMax02, aXId02, nLen2, nQt02, nT2);

				nT4 = 0;
				for (tT3 = 0; tT3 < nLen2; tT3++) {
					aXMI02[tT3] = aXId02[tT3];
					nT4 += (16 - aMax02[tT3]);
				}
				nT4 /= nLen1;
				tModLvl -= nT4;
			} else {
				nLen2 = -nMxMn[tT0][1];
				GetMinNIdx(aMin02, aNId02, nLen2, nQt02, nT2);

				nT3 = 0;
				nT4 = 0;
				for (tT3 = 0; tT3 < nLen2; tT3++) {
					aXMI02[tT3] = aNId02[tT3];
					nT4 += aMin02[tT3];

					nT3 = pDif02[29 - aNId02[tT3]];
					if (nT3 > iMxDif)
						nT4 += 8;
					else {
						nT3 = (nT3 << 3)
							+ (iMxDif >> 1);
						nT3 /= (iMxDif + 1);
						nT4 += nT3;
					}
				}
				nT4 /= (2 * nLen2);
				tModLvl -= nT4;
			}

			tModFlg01[tT0] = 0;
			for (tT3 = 0; tT3 < nLen1; tT3++)
				tModFlg01[tT0] |= (1 << aXMI01[tT3]);

			tModFlg02[tT0] = 0;
			for (tT3 = 0; tT3 < nLen2; tT3++)
				tModFlg02[tT0] |= (1 << aXMI02[tT3]);

			uT03 = (1 << nT2) - 1;
			tModFlg01[tT0] &= uT03;
			tModFlg02[tT0] &= uT03;

			uT01 = (sModFlg01[tT0] << 1);
			uT01 |= (uT01 >> nT2);
			uT01 &= uT03;

			uT02 = (sModFlg02[tT0] << 1);
			uT02 |= (uT02 >> nT2);
			uT02 &= uT03;

			/* minimum check */
			nLen2 = 0;
			if (tT0 == 0) { /* 8-7 */
				nLen2 = 11;
			} else if (tT0 == 2) { /* 6-4 */
				nLen2 = 6;
			} else if (tT0 == 3) { /* 5-5 */
				nLen2 = 6;
			}

			if (nLen2 > 0) {
				GetMinNIdx(aMin02, aNId02, nLen2, nQt02, nT2);
				nT4 = 0;
				for (tT3 = 0; tT3 < nLen2; tT3++) {
					nT3 = pDif02[29 - aNId02[tT3]];
					if (nT3 > iMxDif)
						nT4 += 8;
					else {
						nT3 = (nT3 << 3)
							+ (iMxDif >> 1);
						nT3 /= (iMxDif + 1);
						nT4 += nT3;
					}
				}
				nT4 /= nLen2;
				tModLvl -= nT4;
			}

			if (nMin02 > iMxDif) {
				tModLvl -= 16;
				nModCnt[tT0] = 0;
			} else {
				nT4 = (nMin02 << 4);
				nT4 = nT4 / iMxDif;
				tModLvl -= nT4;
			}


			/* Distance between maximum-2 dif01*/
			if (aXMI01[1] > aXMI01[0])
				nT3 = aXMI01[1] - aXMI01[0];
			else
				nT3 = aXMI01[0] - aXMI01[1];

			/* Distance between minimium-2 dif02 */
			if (aXMI02[1] > aXMI02[0])
				nT4 = aXMI02[1] - aXMI02[0];
			else
				nT4 = aXMI02[0] - aXMI02[1];

			if ((uT01 == tModFlg01[tT0]) &&
				(uT02 == tModFlg02[tT0]) &&
				(nT3 > 0) && (uT01 > 0) &&
				(uT02 > 0)) {
				if (tT0 == 0) {
					if (nT3 == 7 || nT3 == 8) {
						nModCnt[tT0] += 1;
						tModLvl += 2;
					} else
						nModCnt[tT0] = 0;
				} else if (tT0 == 2) {
					if (nT3 == 4 || nT3 == 6) {
						nModCnt[tT0] += 1;
						tModLvl += 2;
					} else
						nModCnt[tT0] = 0;
				} else if (tT0 == 3) {
					if (nT3 == 5) {
						nModCnt[tT0] += 1;
						tModLvl += 2;
					} else
						nModCnt[tT0] = 0;
				}
			}

			if ((uT02 == tModFlg02[tT0]) &&
				(nT4 > 0) && (uT02 > 0)) {
				if ((uT01 == tModFlg01[tT0]) && (uT01 > 0))
					tModLvl += 1;

				if (tT0 == 1) {
					if (nT4 == 5 || nT4 == 7) {
						nModCnt[tT0] += 1;
						tModLvl += 1;
					} else
						nModCnt[tT0] = 0;
				}  else if (tT0 == 4) {
					if (nT4 == 1 || nT4 == 9) {
						nModCnt[tT0] += 1;
						tModLvl += 1;
					} else
						nModCnt[tT0] = 0;
				} else if (tT0 == 5) {
					if (nT4 == 3 || nT4 == 7) {
						nModCnt[tT0] += 1;
						tModLvl += 1;
					} else
						nModCnt[tT0] = 0;
				} else if (tT0 == 6) {
					if (nT4 == 5) {
						nModCnt[tT0] += 1;
						tModLvl += 1;
					} else
						nModCnt[tT0] = 0;
				}
			}

			if (nModCnt[tT0] > 254)
				nModCnt[tT0] = 254;

			if (tModLvl < 0)
				tModLvl = 0;

			nModLvl[tT0] = tModLvl;

			sModFlg01[tT0] = tModFlg01[tT0];
			sModFlg02[tT0] = tModFlg02[tT0];
		} /* 2-3-4-5*/
	}

	tModLvl = nModLvl[0];
	nT1 = 0;
	for	(nT0 = 1; nT0 <	PD_PT_NUM; nT0++) {
		if (nModLvl[nT0] > tModLvl) {
			tModLvl = nModLvl[nT0];
			nT1 = nT0;
		}
	}
	pMdLXx[HISDETNUM - 1] = tModLvl;
	pFlgXx[HISDETNUM - 1] = (pDif01[29] < pDif01[28]);

	/* recheck */
	if ((pFlgXx[HISDETNUM - 2] == 0) &&
		(pDif01[29] > pDif01[28]))
		pFlgXx[HISDETNUM - 2] = 1;

	if (pr_pd && tModLvl > 0) {
		pr_info("#FM%5d detected ct(%3d) lvl(%2d)\n",
		nPrtLog[nT1], nModCnt[nT1], nModLvl[nT1]);
		if (pDif01[29] < pDif01[28])
			pr_info("#Pre: A<-A\n");
		else
			pr_info("#Nxt: A B->\n");
	}

	return 0;
}
