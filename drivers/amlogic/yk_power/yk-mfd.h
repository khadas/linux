#ifndef __LINUX_yk_MFD_H_
#define __LINUX_yk_MFD_H_

#include "yk-mfd-618.h"

#define YK_MFD_ATTR(_name)					\
{									\
	.attr = { .name = #_name,.mode = 0644 },					\
	.show =  _name##_show,				\
	.store = _name##_store, \
}

/* YK battery charger data */
struct power_supply_info;

struct yk_supply_init_data {
	/* battery parameters */
	struct power_supply_info *battery_info;

	/* current and voltage to use for battery charging */
	unsigned int chgcur;
	unsigned int chgvol;
	unsigned int chgend;
	/*charger control*/
	bool chgen;
	bool limit_on;
	/*charger time */
	int chgpretime;
	int chgcsttime;

	/*adc sample time */
	unsigned int sample_time;

	/* platform callbacks for battery low and critical IRQs */
	void (*battery_low)(void);
	void (*battery_critical)(void);
};

struct yk_funcdev_info {
	int		id;
	const char	*name;
	void	*platform_data;
};

struct yk_platform_data {
	int num_sply_devs;
	struct yk_funcdev_info *sply_devs;
};
extern struct device *yk_get_dev(void);



/* NOTE: the functions below are not intended for use outside
 * of the yk sub-device drivers
 */
extern int yk_write(struct device *dev, int reg, uint8_t val);
extern int yk_writes(struct device *dev, int reg, int len, uint8_t *val);
extern int yk_read(struct device *dev, int reg, uint8_t *val);
extern int yk_reads(struct device *dev, int reg, int len, uint8_t *val);
extern int yk_update(struct device *dev, int reg, uint8_t val, uint8_t mask);
extern int yk_set_bits(struct device *dev, int reg, uint8_t bit_mask);
extern int yk_clr_bits(struct device *dev, int reg, uint8_t bit_mask);
extern struct i2c_client *yk;
#endif /* __LINUX_PMIC_yk_H */
