#ifndef _LINUX_YK_MFD_H_
#define _LINUX_YK_MFD_H_

#include <linux/regmap.h>


/* YK618 Regulator Registers */

#define YK_ELDO1DAC 				0x19
#define YK_ELDO2DAC				0x1A
#define YK_DCDC1DAC	        0x21
#define YK_DCDC2DAC	        0x22
#define YK_DCDC3DAC	        0x23
#define YK_DCDC4DAC	        0x24
#define YK_DCDC5DAC	        0x25
#define YK_ALDO1DAC					0x28
#define YK_ALDO2DAC					0x29
#define YK_ALDO3DAC					0x29

#define YK_DCDC1EN				0x10
#define YK_DCDC2EN				0x10
#define YK_DCDC3EN				0x10
#define YK_DCDC4EN				0x10
#define YK_DCDC5EN				0x10
#define YK_ALDO1EN				0x10
#define YK_ALDO2EN				0X10
#define YK_ELDO1EN				0x12
#define YK_ELDO2EN				0x12
#define YK_ALDO3EN				0X12

#define YK_BUCKMODE		0x80
#define YK_BUCKFREQ		0x3B

#define YK_MAXREG    0x93


enum {
	YK_DCDC1,
	YK_DCDC2,
	YK_DCDC3,
	YK_DCDC4,
	YK_DCDC5,
	YK_ALDO1,   //ALDO1
	YK_ALDO2,   //ALDO2
	YK_ALDO3,   //ALDO3
	YK_ELDO1,   //ELDO1
	YK_ELDO2,  //ELDO2
	YK_REG_NUM
};

struct yk618 {
	struct regmap *regmap;
	long variant;
	struct device *dev;
};

#endif
