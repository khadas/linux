/* tbff check head file */
#ifndef TBFF_H_
#define TBFF_H_

struct tbff_stats {
	int buf_row;
	int buf_col;

	/* set the registers */
	int reg_polar5_v_mute;
	int reg_polar5_h_mute;
	int reg_polar5_ro_reset;
	int reg_polar5_mot_thrd;
	int reg_polar5_edge_rat;
	int reg_polar5_ratio;
	int reg_polar5_ofset;

	/* read-only for tff/bff decision */
	int ro_polar5_numofpix;
	int ro_polar5_f4_m2;
	int ro_polar5_f4_p2;
	int ro_polar5_f6_m2;
	int ro_polar5_f6_p2;
	int ro_polar5_f2_m2;
	int ro_polar5_f2_p2;
	int ro_polar5_f4_i5;
	int ro_polar5_f4_i3;
};

typedef void (*tbff_stats_init)(struct tbff_stats *pReg, int irow, int icol);
typedef void (*tbff_stats_get)(unsigned long *in, struct tbff_stats *pReg);
typedef void (*tbff_fwalg_init)(int init_mode);
typedef int (*tbff_fwalg_get)(
	struct tbff_stats *pReg, int fld_id,
	int is_tff, int frm, int skip_flg, int print_flg);
typedef int  (*tbff_majority_get_flg)(void);

struct TB_DetectFuncPtr {
	tbff_stats_init stats_init;
	tbff_stats_get stats_get;
	tbff_fwalg_init fwalg_init;
	tbff_fwalg_get fwalg_get;
	tbff_majority_get_flg majority_get;
};

int RegisterTB_Function(struct TB_DetectFuncPtr *func, const char *ver);
int UnRegisterTB_Function(struct TB_DetectFuncPtr *func);
#endif
