#ifdef ATSC_FUNC_H
#else
#define ATSC_FUNC_H

#include "demod_func.h"

enum atsc_state_machine {
	Idle = 0x20,
	CR_Lock = 0x50,
	CR_Peak_Lock = 0x62,
	Atsc_sync_lock = 0x70,
	Atsc_Lock = 0x79
};

#define Lock	1
#define UnLock	0
#define Cfo_Ok	1
#define Cfo_Fail 0
#define Dagc_Open 1
#define Dagc_Close 0
#define Atsc_BandWith 6000

/* atsc */

int atsc_set_ch(struct aml_demod_sta *demod_sta,
		struct aml_demod_i2c *demod_i2c,
		struct aml_demod_atsc *demod_atsc);
int check_atsc_fsm_status(void);

void atsc_write_reg(int reg_addr, int reg_data);

unsigned long atsc_read_reg(int reg_addr);

unsigned long atsc_read_iqr_reg(void);

int atsc_qam_set(fe_modulation_t mode);

void qam_initial(int qam_id);

void set_cr_ck_rate(void);

void atsc_reset(void);

int atsc_find(unsigned int data, unsigned int *ptable, int len);

int atsc_read_snr(void);

int atsc_read_ser(void);

void atsc_thread(void);


#endif
