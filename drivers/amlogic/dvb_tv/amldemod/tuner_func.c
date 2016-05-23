#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/dvb/aml_demod.h>
#include "demod_func.h"
#include "../aml_fe.h"

int tuner_get_ch_power(struct aml_fe_dev *adap)
{
	int strength = 0;
	int agc_if_gain;

	struct dvb_frontend *dvbfe;
	dvbfe = get_si2177_tuner();
	if (dvbfe != NULL)
		if (dvbfe->ops.tuner_ops.get_strength)
			strength = dvbfe->ops.tuner_ops.get_strength(dvbfe);
	if (strength <= -56) {
		agc_if_gain =
			((dtmb_read_reg(DTMB_TOP_FRONT_AGC))&0x3ff);
		strength = dtmb_get_power_strength(agc_if_gain);
	 }

	return strength;
}

struct dvb_tuner_info *tuner_get_info(int type, int mode)
{
	/*type :  0-NULL, 1-DCT7070, 2-Maxliner, 3-FJ2207, 4-TD1316 */
	/*mode: 0-DVBC 1-DVBT */
	static struct dvb_tuner_info tinfo_null = { };

	static struct dvb_tuner_info tinfo_MXL5003S[2] = {
		[1] = { /*DVBT*/ .name		= "Maxliner",
				 .frequency_min = 44000000,
				 .frequency_max = 885000000, }
	};
	static struct dvb_tuner_info tinfo_FJ2207[2] = {
		[0] = { /*DVBC*/ .name		= "FJ2207",
				 .frequency_min = 54000000,
				 .frequency_max = 870000000, },
		[1] = { /*DVBT*/ .name		= "FJ2207",
				 .frequency_min = 174000000,
				 .frequency_max = 864000000, },
	};
	static struct dvb_tuner_info tinfo_DCT7070[2] = {
		[0] = { /*DVBC*/ .name		= "DCT7070",
				 .frequency_min = 51000000,
				 .frequency_max = 860000000, }
	};
	static struct dvb_tuner_info tinfo_TD1316[2] = {
		[1] = { /*DVBT*/ .name		= "TD1316",
				 .frequency_min = 51000000,
				 .frequency_max = 858000000, }
	};
	static struct dvb_tuner_info tinfo_SI2176[2] = {
		[0] = { /*DVBC*/
			/*#error please add SI2176 code*/
			.name		= "SI2176",
			.frequency_min	= 51000000,
			.frequency_max	= 860000000,
		}
	};

	struct dvb_tuner_info *tinfo[] = {
		&tinfo_null,
		tinfo_DCT7070,
		tinfo_MXL5003S,
		tinfo_FJ2207,
		tinfo_TD1316,
		tinfo_SI2176
	};

	if ((type < 0) || (type > 4) || (mode < 0) || (mode > 1))
		return tinfo[0];

	return &tinfo[type][mode];
}

struct agc_power_tab *tuner_get_agc_power_table(int type)
{
	/*type :  0-NULL, 1-DCT7070, 2-Maxliner, 3-FJ2207, 4-TD1316 */
	static int calcE_FJ2207[31] = {
		87, 118, 138, 154, 172, 197, 245,
		273, 292, 312, 327, 354, 406, 430,
		448, 464, 481, 505, 558, 583, 599,
		616, 632, 653, 698, 725, 745, 762,
		779, 801, 831 };
	static int calcE_Maxliner[79] = {
		543, 552, 562, 575, 586, 596, 608,
		618, 627, 635, 645, 653, 662, 668,
		678, 689, 696, 705, 715, 725, 733,
		742, 752, 763, 769, 778, 789, 800,
		807, 816, 826, 836, 844, 854, 864,
		874, 884, 894, 904, 913, 923, 932,
		942, 951, 961, 970, 980, 990, 1000,
		1012, 1022, 1031, 1040, 1049, 1059,
		1069, 1079, 1088, 1098, 1107, 1115,
		1123, 1132, 1140, 1148, 1157, 1165,
		1173, 1179, 1186, 1192, 1198, 1203,
		1208, 1208, 1214, 1217, 1218, 1220 };

	static struct agc_power_tab power_tab[] = {
		[0] = { "null", 0, 0, NULL },
		[1] = {
			.name	= "DCT7070",
			.level	= 0,
			.ncalcE = 0,
			.calcE	= NULL,
		},
		[2] = {
			.name	= "Maxlear",
			.level	= -22,
			.ncalcE = sizeof(calcE_Maxliner) / sizeof(int),
			.calcE	= calcE_Maxliner,
		},
		[3] = {
			.name	= "FJ2207",
			.level	= -62,
			.ncalcE = sizeof(calcE_FJ2207) / sizeof(int),
			.calcE	= calcE_FJ2207,
		},
		[4] = {
			.name	= "TD1316",
			.level	= 0,
			.ncalcE = 0,
			.calcE	= NULL,
		},
	};

	if (type >= 2 && type <= 3)
		return &power_tab[type];
	else
		return &power_tab[3];
};

int agc_power_to_dbm(int agc_gain, int ad_power, int offset, int tuner)
{
	struct agc_power_tab *ptab = tuner_get_agc_power_table(tuner);
	int est_rf_power;
	int j;

	for (j = 0; j < ptab->ncalcE; j++)
		if (agc_gain <= ptab->calcE[j])
			break;

	est_rf_power = ptab->level - j - (ad_power >> 4) + 12 + offset;

	return est_rf_power;
}

int dtmb_get_power_strength(int agc_gain)
{
	int strength;
	int j;
	static int calcE_R840[13] = {
		1010, 969, 890, 840, 800,
		760, 720, 680, 670, 660,
		510, 440, 368};
	for (j = 0; j < sizeof(calcE_R840)/sizeof(int); j++)
		if (agc_gain >= calcE_R840[j])
			break;
	if (agc_gain >= 440)
		strength = -90+j*3;
	else
		strength = -56;
	return strength;
}


