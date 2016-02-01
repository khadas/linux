/* Designer: Xin.Hu@amlogic.com */
/* Date: 15/11/25 */
#include <linux/kernel.h>
#include <linux/module.h>

#include "film_vof_soft.h"


int VOFSftTop(UINT8 *rFlmPstGCm, UINT8 *rFlmSltPre, UINT8 *rFlmPstMod,
		UShort *rPstCYWnd0, UShort *rPstCYWnd1,
		UShort *rPstCYWnd2, UShort *rPstCYWnd3, int nMod,
		UINT32 *rROCmbInf, struct sFlmDatSt *pRDat,
		struct sFlmSftPar *pPar, int nROW, int nCOL)
{
	/* HSCMB[hist10][9(32bit)] */
	static UINT32 HSCMB[HISCMBNUM][ROWCMBLEN];
	/* 6-history,10-5Wnd(bgn/end) */
	static int CWND[HISDETNUM][2 * VOFWNDNUM];
	static UINT8 BGN;
	static UINT8 frmNoCmb; /* counter from No combing */
	static UINT8 CmbFlds;  /* counter of combing field */
	static UINT8 NumSmFd;  /* counter for same field */

	int mDly = pPar->mPstDlyPre;

	/* UINT8 *PREWV = pRDat.pFlg32; or pRDat.pFlg22 */
	static int TCNm[HISCMBNUM];/* history: the number of combing-rows */
	static int NWND[HISDETNUM];/* 6-history,the number of combing windows */
	static int WGlb[HISDETNUM];	/* Global combing */

	UINT8 *pFlg32 = pRDat->pFlg32;	/* [HISDETNUM]; //history information */
	UINT8 *pMod32 = pRDat->pMod32;	/* [HISDETNUM]; */
	UINT8 *pFlg22 = pRDat->pFlg22;
	UINT8 *pMod22 = pRDat->pMod22;
	UINT8 *mNum32 = pRDat->mNum32;
	UINT32 *DIF02 = pRDat->rROFrmDif02; /* windows */

	int VOFWnd[2 * VOFWNDNUM]; /* VOF windows 5*(bgn/end) */
	int nCSum = 0;		/* Combine sum */
	int nWCmb = 0; /* Total Window combing */
	int nBCmb = 0; /* combing line of below */

	UINT32 nCb32 = 0;

	/* unsigned short nCb32c=0; */
	/* unsigned short nCb32t=0; */
	UINT32 nRCmbAd[ROWCMBLEN];
	UINT32 nRCmbOr[ROWCMBLEN];
	int nT0 = 0;
	int nT1 = 0;

	char debug_str[256];

	/* Initialization */
	if (BGN == 0) {
		for (nT0 = 0; nT0 < HISCMBNUM; nT0++) {
			/* (288/32) */
			for (nT1 = 0; nT1 < ROWCMBLEN; nT1++) {
				/* 32-bit (all 1) */
				HSCMB[nT0][nT1] = 0xffffffff;
			}
			TCNm[nT0] = 0;
		}

		for (nT0 = 0; nT0 < HISDETNUM; nT0++) {
			NWND[nT0] = 15;
			WGlb[nT0] = 0;
			for (nT1 = 0; nT1 < 2 * VOFWNDNUM; nT1++)
				CWND[nT0][nT1] = 0;
		}

		BGN = 1;
	}

	for (nT0 = 1; nT0 < HISCMBNUM; nT0++) {
		for (nT1 = 0; nT1 < ROWCMBLEN; nT1++)
			HSCMB[nT0 - 1][nT1] = HSCMB[nT0][nT1];
		TCNm[nT0 - 1] = TCNm[nT0];
	}

	/* static int CWND[6][10]; */
    /* 6-history, 10-5Wnd(bgn/end) */
	for (nT0 = 0; nT0 < HISDETNUM - 1; nT0++) {
		for (nT1 = 0; nT1 < 2 * VOFWNDNUM; nT1++)
			CWND[nT0][nT1] = CWND[nT0 + 1][nT1];
	}

	for (nT1 = 0; nT1 < 2 * VOFWNDNUM; nT1++) {
		CWND[HISDETNUM - 1][nT1] = 0;/* f(t-0) vs f(t-1) */
		VOFWnd[nT1] = 0; /* initialization */
	}

	/* nS0 = 0; */
	nCSum = 0;
	for (nT0 = 0; nT0 < ROWCMBLEN; nT0++) {
		nCb32 = rROCmbInf[nT0];
		/* Inf[0]-[31:0], First-[31], Lst-[0] */
		HSCMB[HISCMBNUM - 1][nT0] = nCb32;
		for (nT1 = 0; nT1 < 32; nT1++) {
			nCSum += (nCb32 & 0x1);
			nCb32 = nCb32 >> 1;
		}
	}

	if (nCSum > nROW)
		nCSum = nROW;
	TCNm[HISCMBNUM - 1] = nCSum;	/* the number of combing row */

	for (nT0 = 0; nT0 < HISDETNUM-1; nT0++) {
		NWND[nT0] = NWND[nT0+1];
		WGlb[nT0] = WGlb[nT0+1];
	}

	nT1 = 0;
	if (pMod32[HISDETNUM - 1] == 3) {
		if (pFlg32[HISDETNUM - 1] & 0x1)
			nT1 = 1;
		else
			nT1 = 2;

		/* TODO: Check here */
		WGlb[HISDETNUM-1] = 0;

		for (nT0 = 0; nT0 < ROWCMBLEN; nT0++) {
			nRCmbAd[nT0] = HSCMB[HISCMBNUM - nT1][nT0];
			nRCmbOr[nT0] = HSCMB[HISCMBNUM - nT1][nT0];
		}

		for (nT1 = nT1 + 1; nT1 <= 5; nT1++) {
			if (pFlg32[HISDETNUM - nT1] & 0x1) {
				for (nT0 = 0; nT0 < ROWCMBLEN; nT0++) {
					nRCmbAd[nT0] =
						(nRCmbAd[nT0] &
						HSCMB[HISCMBNUM - nT1][nT0]);
					nRCmbOr[nT0] =
						(nRCmbOr[nT0] |
						HSCMB[HISCMBNUM - nT1][nT0]);
				}
			}
		}

		/* return: the number of windows */
		nT1 = VOFDetSub1(VOFWnd, &nCSum, 3, nRCmbAd, nROW, pPar);
		nWCmb = 0;
		nBCmb = 0;
		for (nT0 = 0; nT0 < nT1; nT0++) {
			if (VOFWnd[2 * nT0] > (3*nROW >> 2)) {
				/* window expand to +-4 */
				/* can be set as param */
				CWND[HISDETNUM - 1][2 * nT0]
					= VOFWnd[2 * nT0] - 4;
				CWND[HISDETNUM - 1][2 * nT0 + 1] =
					VOFWnd[2 * nT0 + 1] + 4;

				nBCmb = VOFWnd[2*nT0+1]-VOFWnd[2*nT0]+1;
				/* patch for MIT32Mix ending vof */
				if ((CWND[4][2*nT0] < CWND[5][2*nT0]+4) &&
				(CWND[5][2*nT0] < CWND[4][2*nT0]+4) &&
				(CWND[4][2*nT0+1] < CWND[5][2*nT0+1]+4) &&
				(CWND[5][2*nT0+1] < CWND[4][2*nT0+1]+4)) {
					if (CWND[4][2*nT0] < CWND[5][2*nT0])
						CWND[5][2*nT0] = CWND[4][2*nT0];

					if (CWND[4][2*nT0+1] > CWND[5][2*nT0+1])
						CWND[5][2*nT0+1]
						= CWND[4][2*nT0+1];
				}
				/* patch for MIT32Mix ending vof */
			}
			nWCmb += VOFWnd[2*nT0+1]-VOFWnd[2*nT0]+1;
		}

		/* VOF using last ones */
		if (nBCmb == 0) {
			if (frmNoCmb > 40 || DIF02[0] < 1024)
				CmbFlds = 0;

			if (frmNoCmb < 255)
				frmNoCmb = frmNoCmb+1;
		} else {
			/* noCombing => combing => noCombing */
			if (frmNoCmb >= 10)
				CmbFlds = 0;

			frmNoCmb = 0;

			if (CmbFlds < 255)
				CmbFlds = CmbFlds+1;
		}

		/* parameter setting */
		/* if (nBCmb==0 && frmNoCmb<10 && CmbFlds>10) */
		if (nBCmb == 0 && frmNoCmb < 40 && CmbFlds > 5
			&& DIF02[0] > 1024) {
			for (nT0 = 0; nT0 < 2*VOFWNDNUM; nT0++)
				CWND[HISDETNUM-1][nT0] = CWND[HISDETNUM-2][nT0];
		}
		/* VOF using last ones */
		/* patch for cadence-32 */
		if (NumSmFd > 4 && mNum32[HISDETNUM-1] > 20) {
			for (nT0 = 0; nT0 < 2*VOFWNDNUM; nT0++)
				CWND[HISDETNUM-1][nT0] = 0;
		}
		/* patch for cadence-32 */

		/* here can set as parameters */
		/* circus's not good if following is on */
		/*
		if ( nCSum > (nROW>>1) ) {
			WGlb[HISDETNUM-1] = 1;
		}
		*/
		/* here for vertical moving VOF */
		/* HQV: Mix32- piano+vertical VOF */
		/* here can be set as parameters */
		if (pFlg32[HISDETNUM-1] == 5) {
			/* circus's not good if following is on */
			/*
			if (nWCmb>(100*nROW>>8)) {
				mNum32[HISDETNUM-1] = 0;
			}
			*/
			/* patch for cadence-32 */
			if (DIF02[0] < 1024) {
				if (NumSmFd < 255)
					NumSmFd = NumSmFd+1;
			} else {
				NumSmFd = 0;
			}
			/* patch for cadence-32 */

			/* 256/16 = 16 */
			/* 64/16  = 4 for 8-bit */
			/* vertical vof copyright */
			if (nWCmb < 10)
				nWCmb = 10;

			nT1 = nWCmb*nCOL*64>>4;

			if ((DIF02[0] >= nT1) && (nWCmb > nBCmb+10))
				mNum32[HISDETNUM-1] = 0;
		}
		/* here for vertical moving VOF */
	} else if (pMod22[HISDETNUM-1] == 2) {
		if ((pFlg22[HISDETNUM-1] & 0x1) && (nCSum > nROW/2))
			WGlb[HISDETNUM-1] = 1; /*global combing*/

		for (nT0 = 0; nT0 < ROWCMBLEN; nT0++) {
			nRCmbAd[nT0] = HSCMB[HISCMBNUM-1][nT0];
			nRCmbOr[nT0] = HSCMB[HISCMBNUM-1][nT0];
		}

		for (nT1 = 3; nT1 <= 5; nT1 += 2)
			for (nT0 = 0; nT0 < ROWCMBLEN; nT0++) {
				nRCmbAd[nT0] = nRCmbAd[nT0]
					& HSCMB[HISCMBNUM-nT1][nT0];
				nRCmbOr[nT0] = nRCmbOr[nT0]
					| HSCMB[HISCMBNUM-nT1][nT0];
			}

		/* return: the number of windows */
		nT1 = VOFDetSub1(VOFWnd, &nCSum, 2, nRCmbAd, nROW, pPar);
		for (nT0 = 0; nT0 < nT1; nT0++) {
			/* window used for dejaggies */
			CWND[HISDETNUM-1][2*nT0]   = VOFWnd[2*nT0];
			CWND[HISDETNUM-1][2*nT0+1] = VOFWnd[2*nT0+1];
		}
	}

	/* film-mode: pMod22[5-mDly] or pMod32[5-mDly] */
	if (*rFlmPstMod != 0) {
		/* weaver with pre-field */
		if (*rFlmSltPre == 1) {
			/* Interpolation method:0-mtn,1-with-buffer,2-ei,3-di */
			rPstCYWnd0[0] = CWND[HISDETNUM - 1 - mDly][0];/* bgn */
			rPstCYWnd0[1] = CWND[HISDETNUM - 1 - mDly][1];/* end */
			rPstCYWnd0[2] = 3;/* 0-mtn,1-with-buffer,2-ei,3-di */

			rPstCYWnd1[0] = CWND[HISDETNUM - 1 - mDly][2];/* bgn */
			rPstCYWnd1[1] = CWND[HISDETNUM - 1 - mDly][3];/* end */
			rPstCYWnd1[2] = 3;/* 0-mtn,1-with-buffer,2-ei,3-di */

			rPstCYWnd2[0] = CWND[HISDETNUM - 1 - mDly][4];/* bgn */
			rPstCYWnd2[1] = CWND[HISDETNUM - 1 - mDly][5];/* end */
			rPstCYWnd2[2] = 3;/* 0-mtn,1-with-buffer,2-ei,3-di */

			rPstCYWnd3[0] = CWND[HISDETNUM - 1 - mDly][6];/* bgn */
			rPstCYWnd3[1] = CWND[HISDETNUM - 1 - mDly][7];/* end */
			rPstCYWnd3[2] = 3;/* 0-mtn,1-with-buffer,2-ei,3-di */

			/* pFMReg->rFlmPstGCm = WGlb[5-mDly]; */
			*rFlmPstGCm = WGlb[HISDETNUM - 1 - mDly];

			if (pr_pd & 0xf0) /* VOF */
				pr_info("\tfilm weave prev: rFlmPstGCm=%1d, pFlg32=%3d, pFlg22=%3d\n",
					*rFlmPstGCm,
					pFlg32[HISDETNUM - 1 - mDly],
					pMod22[HISDETNUM - 1 - mDly]);
		} else {
			/* weaver with nxt-field */
			/* Interpolation method: 0-EI,1-MTN,2-MA,3-Weaver */
			rPstCYWnd0[0] = CWND[HISDETNUM - mDly][0];/* bgn */
			rPstCYWnd0[1] = CWND[HISDETNUM - mDly][1];/* end */
			rPstCYWnd0[2] = 3;/* 0-mtn,1-with-buffer,2-ei,3-di */
			rPstCYWnd1[0] = CWND[HISDETNUM - mDly][2];/* bgn */
			rPstCYWnd1[1] = CWND[HISDETNUM - mDly][3];/* end */
			rPstCYWnd1[2] = 3;/* 0-mtn,1-with-buffer,2-ei,3-di */
			rPstCYWnd2[0] = CWND[HISDETNUM - mDly][4];/* bgn */
			rPstCYWnd2[1] = CWND[HISDETNUM - mDly][5];/* end */
			rPstCYWnd2[2] = 3;/* 0-mtn,1-with-buffer,2-ei,3-di */
			rPstCYWnd3[0] = CWND[HISDETNUM - mDly][6];/* bgn */
			rPstCYWnd3[1] = CWND[HISDETNUM - mDly][7];/* end */
			rPstCYWnd3[2] = 3;/* 0-mtn,1-with-buffer,2-ei,3-di */
			*rFlmPstGCm = WGlb[HISDETNUM - mDly];
			if (pr_pd & 0xf0) /* VOF */
				pr_info("\tfilm weave next: rFlmPstGCm=%1d, pFlg32=%3d, pFlg22=%3d\n",
					*rFlmPstGCm,
					pFlg32[HISDETNUM - 1 - mDly],
					pMod22[HISDETNUM - 1 - mDly]);
		}
	} else {
		rPstCYWnd0[0] = 0;	/* bgn */
		rPstCYWnd0[1] = 0;	/* end */
		rPstCYWnd0[2] = 3;	/* 0-mtn,1-with-buffer,2-ei,3-di */
		rPstCYWnd1[0] = 0;	/* bgn */
		rPstCYWnd1[1] = 0;	/* end */
		rPstCYWnd1[2] = 3;	/* 0-mtn,1-with-buffer,2-ei,3-di */
		rPstCYWnd2[0] = 0;	/* bgn */
		rPstCYWnd2[1] = 0;	/* end */
		rPstCYWnd2[2] = 3;	/* 0-mtn,1-with-buffer,2-ei,3-di */
		rPstCYWnd3[0] = 0;	/* bgn */
		rPstCYWnd3[1] = 0;	/* end */
		rPstCYWnd3[2] = 3;	/* 0-mtn,1-with-buffer,2-ei,3-di */
		*rFlmPstGCm = 1;

		if (pr_pd & 0xf0) /* VOF */
			pr_info("\tvideo: rFlmPstGCm=%1d\n", *rFlmPstGCm);
	}

	if (pr_pd & 0xf0) { /* VOF */
		if ((pMod32[HISDETNUM - 1] == 3)
		|| (pMod22[HISDETNUM-1] == 2)) {
			if (pr_pd & 0x10) { /* combing */
				sprintf(debug_str,
					"\tCombing: nCSum=%3d WGld=%1d Wnd=%1d",
					nCSum, WGlb[HISDETNUM-1], nT1);
				for (nT0 = 0; nT0 < ROWCMBLEN; nT0++)
					sprintf(debug_str+strlen(debug_str),
						" %08x",
						rROCmbInf[nT0]);
				pr_info("%s\n", debug_str);
			}
			if (pr_pd & 0x20) { /* combing and & or */
				sprintf(debug_str, "\tCombing AND:");
				for (nT0 = 0; nT0 < ROWCMBLEN; nT0++)
					sprintf(debug_str+strlen(debug_str),
						" %08x",
						nRCmbAd[nT0]);
				pr_info("%s\n", debug_str);
				sprintf(debug_str, "\tCombing OR :");
				for (nT0 = 0; nT0 < ROWCMBLEN; nT0++)
					sprintf(debug_str+strlen(debug_str),
						" %08x",
						nRCmbOr[nT0]);
				pr_info("%s\n", debug_str);
			}
			if ((pr_pd & 0x40) && (*rFlmPstMod != 0)) {
				sprintf(debug_str, "\tVofWindow:");
				for (nT0 = 0; nT0 < nT1; nT0++)
					sprintf(debug_str+strlen(debug_str),
						" %3d~%3d",
						VOFWnd[nT0*2],
						VOFWnd[nT0*2+1]
					);
				pr_info("%s\n", debug_str);
			}
		}
		if ((pr_pd & 0x80) && (*rFlmPstMod != 0))
			pr_info("\tPstCYWind: %3d~%3d %3d~%3d %3d~%3d %3d~%3d\n",
				rPstCYWnd0[0], rPstCYWnd0[1],
				rPstCYWnd1[0], rPstCYWnd1[1],
				rPstCYWnd2[0], rPstCYWnd2[1],
				rPstCYWnd3[0], rPstCYWnd3[1]
			);
	}
	return nWCmb;
}

/* int *PREWV:5*2 */
/* nCNum: total rows of combing */
int VOFDetSub1(int *VOFWnd, int *nCNum, int nMod, UINT32 *nRCmb, int nROW,
	       struct sFlmSftPar *pPar)
{
	int rCmbRwMinCt0 = pPar->rCmbRwMinCt0;	/* 8; //for film 3-2 */
	int rCmbRwMinCt1 = pPar->rCmbRwMinCt1;	/* =7; //for film 2-2 */
	/* int rCmbRwMaxStp=1; //fill in the hole */
	int rCmbRwMinCt = rCmbRwMinCt1;
	int nCSUM = 0;	/* Combing sum (nCSUM>rCmbRwMinCt0) */
	int nMIN = 0;
	int nT0 = 0;
	int nT1 = 0;
	int nT2 = 0;
	int nCNM = 0;
	int nBgn = 0;
	int nEnd = 0;
	int fEND = 0;
	int pIDx[VOFWNDNUM + 1][2];	/* Maximum-5windows */
	int nIDx = 0;

	if (nMod == 3)
		rCmbRwMinCt = rCmbRwMinCt0;

	for (nT0 = 0; (nT0 < nROW) && (nIDx <= VOFWNDNUM); nT0++) {
		fEND = 0;
		nT1 = nROW - 1 - nT0;

		/* if(nRCmb[nT0]==1) */
		if (Get1RCmb(nRCmb, nT1)) {
			nCSUM += 1;	/* Total */
			if (nT0 == 0)
				nBgn = nT0;
			else if (nT0 == nROW - 1) {
				if (Get1RCmb(nRCmb, 1)) {
					/* at least (2-Row combing) */
					nEnd = nT0;
					fEND = 1;
				}
			} else if (!Get1RCmb(nRCmb, nT1 + 1)) {
				/* (nRCmb[nT0-1]==0) */
				nBgn = nT0;
			}
		} else {
			/* nRCmb[nT0]==0 */
			if (nT0 != 0 && Get1RCmb(nRCmb, nT1 + 1)) {
				nEnd = nT0;	/* nT0-1 */
				fEND = 1;
			}
		}

		if (fEND == 0)
			continue;

		nCNM = nEnd - nBgn + 1;
		if (nCNM > rCmbRwMinCt || nEnd == nROW - 1) {
			if (nIDx == VOFWNDNUM) {
				nMIN = nCNM;
				nT2 = VOFWNDNUM;
				for (nT1 = 0; nT1 < VOFWNDNUM; nT1++)
					if ((pIDx[nT1][1] -
					pIDx[nT1][0] + 1) < nMIN) {
						nMIN = pIDx[nT1][1]
						- pIDx[nT1][0] + 1;
						nT2 = nT1;
					}
				if (nT2 != VOFWNDNUM) {
					pIDx[nT2][0] = nBgn;
					pIDx[nT2][1] = nEnd;
				}
			} else {
				pIDx[nIDx][0] = nBgn;
				pIDx[nIDx][1] = nEnd;
				nIDx += 1;
			}
		}
	}
	*nCNum = nCSUM;
	for (nT0 = 0; nT0 < nIDx; nT0++) {
		VOFWnd[2 * nT0] = pIDx[nT0][0];/* nBgn */
		VOFWnd[2 * nT0 + 1] = pIDx[nT0][1];/* nEnd */
	}
	return nIDx;
}


/* Get 1-Row combing information, 1bit */
/* iHSCMB[9]; 9x32=288 */
UINT8 Get1RCmb(UINT32 *iHSCMB, UINT32 iRow)
{
	UINT8 nR1 = 0;
	UINT8 nBt = 0;
	nR1 = ((iRow >> 5) & 0xf);/* iRow/32; 0--8 */
	nBt = (iRow & 0x1f);/* iRow%32 */
	return (iHSCMB[nR1] >> nBt) & 0x1;
}


/* Get 1-Row combing history */
UINT8 Get1RHstCmb(UINT32 HSCMB[HISCMBNUM][ROWCMBLEN], UINT8 *wPRE,
			UINT32 iRow)
{
	UINT8 nSM = 0;
	int nT1 = 0;
	for (nT1 = HISCMBNUM - 1; nT1 >= 0; nT1--) {
		/* f(t-1) + f(t-0) */
		if (wPRE[nT1] == 1) {
			/* Get 1-Row combing information, 1bit */
			nSM += Get1RCmb(HSCMB[nT1], iRow);
		}
	}
	return nSM;
}
