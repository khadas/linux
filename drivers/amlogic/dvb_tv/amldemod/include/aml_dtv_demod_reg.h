#ifndef _DTV_REG_H_
#define _DTV_REG_H_

#include <linux/amlogic/iomap.h>

#define DTV_WRITE_CBUS_REG(_r, _v)   aml_write_cbus(_r, _v)
#define DTV_READ_CBUS_REG(_r)        aml_read_cbus(_r)

#endif	/*  */
