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

int FlmVOFSftTop(UINT8 *rCmb32Spcl, unsigned short *rPstCYWnd0,
		 unsigned short *rPstCYWnd1, unsigned short *rPstCYWnd2,
		 unsigned short *rPstCYWnd3, unsigned short *rPstCYWnd4,
		 UINT8 *rFlmPstGCm, UINT8 *rFlmSltPre, UINT8 *rFlmPstMod,
		 UINT32 *rROFldDif01, UINT32 *rROFrmDif02, UINT32 *rROCmbInf,
		 struct sFlmSftPar *pPar, int nROW, int nCOL)
{
	static UINT32 DIF01[HISDIFNUM]; /* Last one is global */
	static UINT32 DIF02[HISDIFNUM]; /* Last one is global */

	static struct sFlmDatSt pRDat;

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

	/* Initialization */
	if (field_count == 0) {
		for (nT1 = 0; nT1 < HISDIFNUM; nT1++) {
			DIF01[nT1] = 0;
			DIF02[nT1] = 0;
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
	FlmDetSft(&pRDat, nDIF01, nDIF02, nT0, pPar);
	/* FlmDetSft((&pRGlb), nDIF01, nDIF02, nT0); */
	/* --------------------------------------------------------- */

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
	} else if (pRDat.pMod22[HISDETNUM - 1 - mDly] == 2) {
		nT0 = pRDat.pFlg22[HISDETNUM - 1 - mDly] % 2;
		*rFlmSltPre = nT0;
		/* Post-processing: film mode,
		00: global combing, 01: 2-2 film, 10: 2-3 film, 11:-others */
		*rFlmPstMod = 1;

		/* param: at least 10 field+4 */
		if (pRDat.mNum22[HISDETNUM - 1] < 10) {
			*rFlmSltPre = 0;
			*rFlmPstMod = 0;
		}
	} else {
		*rFlmSltPre = 0;
		/* Post-processing: film mode,00: global combing,
		01: 2-2 film, 10: 2-3 film, 11:-others */
		*rFlmPstMod = 0;
	}

	nS1 = VOFSftTop(rFlmPstGCm, rFlmSltPre, rFlmPstMod,
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
	return 0;
}

/* Film Detection Software implementation */
/* nDif01: Fild Difference */
/* nDif02: Frame Difference */
/* WND: The index of Window */
int FlmDetSft(struct sFlmDatSt *pRDat, int *nDif01, int *nDif02,
	      int WND, struct sFlmSftPar *pPar)
{
	/* int nT0=0; */
	/* 3-2 */
	Flm32DetSft(pRDat, nDif02, nDif01, pPar);

	/* Film2-2 Detection */
	/* debug0304 */
	Flm22DetSft(pRDat, nDif01, pPar);
	/* ---------------------------------------- */

	return 0;
}

/* pFlm02[0:nLEN-1] : recursive, 0-2 dif */
/* pFlm01[0:nLEN-1] : recursive, 0-1 dif */
int Flm32DetSft(struct sFlmDatSt *pRDat, int *nDif02,
		int *nDif01, struct sFlmSftPar *pPar)
{
	#if 0
	int sFrmDifAvgRat = 16;  /* 0~32 */
	The Large Decision should be : /*(large>average+LgDifThrd) */
	int sFrmDifLgTDif = 4096;
	int sF32StpWgt01 = 15;
	int sF32StpWgt02 = 15;
	int sF32DifLgRat = 16;	/* Dif>Rat*Min  --> Larger */
	*/
	#endif
	int sFrmDifAvgRat = pPar->sFrmDifAvgRat;	/* 16;  //0~32 */
	/* 4096; //The Large Decision should be: (large>average+LgDifThrd) */
	int sFrmDifLgTDif = pPar->sFrmDifLgTDif;
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

	if (pFlg32[HISDETNUM - 1] == 2 || pFlg32[HISDETNUM - 1] == 4
		|| pFlg32[HISDETNUM - 1] == 3) {
		/* ========================================== */
		if (nDif02[HISDIFNUM - 1] > nDif02[HISDIFNUM - 6])
			nFlgChk1 =
			    nDif02[HISDIFNUM - 1] - nDif02[HISDIFNUM - 6];
		else
			nFlgChk1 =
			    nDif02[HISDIFNUM - 6] - nDif02[HISDIFNUM - 1];

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
	if (nFlgChk1 > 25 || nFlgChk2 > 16) {
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
	if (nSP <= 1) {
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
int Flm22DetSft(struct sFlmDatSt *pRDat, int *nDif01,
			struct sFlmSftPar *pPar)
{
	UINT8 *pFlg = pRDat->pFlg22;
	UINT8 *pMod = pRDat->pMod22;

	UINT8 *pStp = pRDat->pStp22;
	UINT8 *pSmp = pRDat->pSmp22;

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

	for (nT0 = 0; nT0 < HISDETNUM - 1; nT0++) {
		pFlg[nT0] = pFlg[nT0 + 1];
		pMod[nT0] = pMod[nT0 + 1];
		pStp[nT0] = pStp[nT0 + 1];
		pSmp[nT0] = pSmp[nT0 + 1];
	}

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
	}

	nAV21 = (nSM21 + nL21 / 2) / nL21;	/* High average */
	nAV22 = (nSM22 + nL22 / 2) / nL22;	/* Low average */
	nOfst = nAV21 - nAV22;

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

	nT0 = sFlm2MinAlpha * Mn56 + sFlm20ftAlpha * nOfst;
	tMgn = ((nT0 + 16) >> 5);

	nT1 = sFlm2MinBelta * Mn56;
	BtMn = ((nT1 + 32) >> 6);

	/* ----------------------------------- */
	/* int *pStp = pRDat->pStp22; */
	/* int *pSmp = pRDat->pSmp22; */
	nT0 = 16 * (Mx56 - tMgn) + (BtMn + sFlm2LgDifThd) / 2;
	nT1 = nT0 / (BtMn + sFlm2LgDifThd);
	if (nT1 > 16)
		nT1 = 16;
	else if (nT1 < 0)
		nT1 = 0;

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
		tMgn = (nAV22 * 64) >> 8;
		if (tMgn > 720 * 240) {
			nT0 = 0;
			nT1 = 0;
		}
		/* --------------------------------------- */
	}
	pFlg[HISDETNUM - 1] = nT0;
	pMod[HISDETNUM - 1] = nT1;
	/* ----------------------------------- */

	return 0;
}
