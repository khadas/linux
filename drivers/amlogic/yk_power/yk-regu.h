#ifndef _LINUX_YK_REGU_H_
#define _LINUX_YK_REGU_H_

#include "yk-cfg.h"
#include "yk-mfd.h"

/* YK618 Regulator Registers */
#define YK_LDO1			YK_STATUS
#define YK_LDO5	        YK_DLDO1OUT_VOL
#define YK_LDO6	        YK_DLDO2OUT_VOL
#define YK_LDO7	        YK_DLDO3OUT_VOL
#define YK_LDO8	        YK_DLDO4OUT_VOL
#define YK_LDO9			YK_ELDO1OUT_VOL
#define YK_LDO10			YK_ELDO2OUT_VOL
#define YK_LDO11			YK_ELDO3OUT_VOL
#define YK_LDO12			YK_DC5LDOOUT_VOL
#define YK_DCDC1	        YK_DC1OUT_VOL
#define YK_DCDC2	        YK_DC2OUT_VOL
#define YK_DCDC3	        YK_DC3OUT_VOL
#define YK_DCDC4	        YK_DC4OUT_VOL
#define YK_DCDC5	        YK_DC5OUT_VOL

#define YK_LDOIO0		YK_GPIO0LDOOUT_VOL
#define YK_LDOIO1		YK_GPIO1LDOOUT_VOL
#define YK_LDO2	        YK_ALDO1OUT_VOL
#define YK_LDO3	        YK_ALDO2OUT_VOL
#define YK_LDO4	        YK_ALDO3OUT_VOL
#define YK_SW0		YK_STATUS

#define YK_LDO1EN		YK_STATUS
#define YK_LDO2EN		YK_LDO_DC_EN1
#define YK_LDO3EN		YK_LDO_DC_EN1

#define YK_LDO4EN		YK_LDO_DC_EN2

#define YK_LDO5EN		YK_LDO_DC_EN2
#define YK_LDO6EN		YK_LDO_DC_EN2
#define YK_LDO7EN		YK_LDO_DC_EN2
#define YK_LDO8EN		YK_LDO_DC_EN2
#define YK_LDO9EN		YK_LDO_DC_EN2
#define YK_LDO10EN		YK_LDO_DC_EN2
#define YK_LDO11EN		YK_LDO_DC_EN2
#define YK_LDO12EN		YK_LDO_DC_EN1
#define YK_DCDC1EN		YK_LDO_DC_EN1
#define YK_DCDC2EN		YK_LDO_DC_EN1
#define YK_DCDC3EN		YK_LDO_DC_EN1
#define YK_DCDC4EN		YK_LDO_DC_EN1
#define YK_DCDC5EN		YK_LDO_DC_EN1
#define YK_LDOIO0EN		YK_GPIO0_CTL
#define YK_LDOIO1EN		YK_GPIO1_CTL
#define YK_DC1SW1EN		YK_LDO_DC_EN2
#define YK_SW0EN		YK_LDO_DC_EN2

#define YK_BUCKMODE		YK_DCDC_MODESET
#define YK_BUCKFREQ		YK_DCDC_FREQSET

#define YK_LDO(_pmic, _id, min, max, step1, vreg, shift, nbits, ereg, ebit, switch_vol, step2, new_level)	\
{									\
	.desc	= {							\
		.name	= #_pmic"_LDO" #_id,					\
		.type	= REGULATOR_VOLTAGE,				\
		.id	= _pmic##_ID_##_id,				\
		.n_voltages = (step1) ? ( (switch_vol) ? ((switch_vol - min) / step1 +	\
				(max - switch_vol) / step2  +1):	\
				((max - min) / step1 +1) ): 1,		\
		.owner	= THIS_MODULE,					\
	},								\
	.min_uV		= (min) * 1000,					\
	.max_uV		= (max) * 1000,					\
	.step1_uV	= (step1) * 1000,				\
	.vol_reg	= _pmic##_##vreg,				\
	.vol_shift	= (shift),					\
	.vol_nbits	= (nbits),					\
	.enable_reg	= _pmic##_##ereg,				\
	.enable_bit	= (ebit),					\
	.switch_uV	= (switch_vol)*1000,				\
	.step2_uV	= (step2)*1000,					\
	.new_level_uV	= (new_level)*1000,				\
}

#define YK_BUCK(_pmic, _id, min, max, step1, vreg, shift, nbits, ereg, ebit, switch_vol, step2, new_level) \
{									\
	.desc	= {							\
		.name	= #_pmic"_BUCK" #_id,					\
		.type	= REGULATOR_VOLTAGE,				\
		.id	= _pmic##_ID_BUCK##_id,				\
		.n_voltages = (step1) ? ( (switch_vol) ? ((new_level)?((switch_vol - min) / step1 +	\
				(max - new_level) / step2  +2) : ((switch_vol - min) / step1 +	\
				(max - switch_vol) / step2  +1)):	\
				((max - min) / step1 +1) ): 1,		\
	},								\
	.min_uV		= (min) * 1000,					\
	.max_uV		= (max) * 1000,					\
	.step1_uV	= (step1) * 1000,				\
	.vol_reg	= _pmic##_##vreg,				\
	.vol_shift	= (shift),					\
	.vol_nbits	= (nbits),					\
	.enable_reg	= _pmic##_##ereg,				\
	.enable_bit	= (ebit),					\
	.switch_uV	= (switch_vol)*1000,				\
	.step2_uV	= (step2)*1000,					\
	.new_level_uV	= (new_level)*1000,				\
}

#define YK_DCDC(_pmic, _id, min, max, step1, vreg, shift, nbits, ereg, ebit, switch_vol, step2, new_level)	\
{									\
	.desc	= {							\
		.name	= #_pmic"_DCDC" #_id,					\
		.type	= REGULATOR_VOLTAGE,				\
		.id	= _pmic##_ID_DCDC##_id,				\
		.n_voltages = (step1) ? ( (switch_vol) ? ((new_level)?((switch_vol - min) / step1 +	\
				(max - new_level) / step2  +2) : ((switch_vol - min) / step1 +	\
				(max - switch_vol) / step2  +1)):	\
				((max - min) / step1 +1) ): 1,		\
		.owner	= THIS_MODULE,					\
	},								\
	.min_uV		= (min) * 1000,					\
	.max_uV		= (max) * 1000,					\
	.step1_uV	= (step1) * 1000,				\
	.vol_reg	= _pmic##_##vreg,				\
	.vol_shift	= (shift),					\
	.vol_nbits	= (nbits),					\
	.enable_reg	= _pmic##_##ereg,				\
	.enable_bit	= (ebit),					\
	.switch_uV	= (switch_vol)*1000,				\
	.step2_uV	= (step2)*1000,					\
	.new_level_uV	= (new_level)*1000,				\
}
//YK_LDO2EN
#define YK_SW(_pmic, _id, min, max, step1, vreg, shift, nbits, ereg, ebit, switch_vol, step2, new_level) \
{									\
	.desc	= {							\
		.name	= #_pmic"_SW" #_id,					\
		.type	= REGULATOR_VOLTAGE,				\
		.id	= _pmic##_ID_SW##_id,				\
		.n_voltages = (step1) ? ( (switch_vol) ? ((new_level)?((switch_vol - min) / step1 +	\
				(max - new_level) / step2  +2) : ((switch_vol - min) / step1 +	\
				(max - switch_vol) / step2  +1)):	\
				((max - min) / step1 +1) ): 1,		\
		.owner	= THIS_MODULE,					\
	},								\
	.min_uV		= (min) * 1000,					\
	.max_uV		= (max) * 1000,					\
	.step1_uV	= (step1) * 1000,				\
	.vol_reg	= _pmic##_##vreg,				\
	.vol_shift	= (shift),					\
	.vol_nbits	= (nbits),					\
	.enable_reg	= _pmic##_##ereg,				\
	.enable_bit	= (ebit),					\
	.switch_uV	= (switch_vol)*1000,				\
	.step2_uV	= (step2)*1000,					\
	.new_level_uV	= (new_level)*1000,				\
}

#define YK_REGU_ATTR(_name)						\
{									\
	.attr = { .name = #_name,.mode = 0644 },			\
	.show =  _name##_show,						\
	.store = _name##_store,						\
}

struct yk_regulator_info {
	struct regulator_desc desc;

	int	min_uV;
	int	max_uV;
	int	step1_uV;
	int	vol_reg;
	int	vol_shift;
	int	vol_nbits;
	int	enable_reg;
	int	enable_bit;
	int	switch_uV;
	int	step2_uV;
	int	new_level_uV;
};

struct  yk_reg_init {
	struct regulator_init_data yk_reg_init_data;
	struct yk_regulator_info *info;
};

#endif
