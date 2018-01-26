#ifndef __AML_CI_H_
#define __AML_CI_H_

#include "drivers/media/dvb-core/dvb_ca_en50221.h"
#include "aml_dvb.h"

enum aml_dvb_io_type_e {
	AML_DVB_IO_TYPE_IOBUS = 0,
	AML_DVB_IO_TYPE_SPI,
	AML_DVB_IO_TYPE_MAX,
};

struct aml_ci {
	struct dvb_ca_en50221		en50221;
	struct mutex				ci_lock;
	int	io_type;
	void						*priv;
	int                         id;
	struct class                class;

	/* NOTE: the read_*, write_* and poll_slot_status functions will be
	 * called for different slots concurrently and need to use locks where
	 * and if appropriate. There will be no concurrent access to one slot.
	 */

	/* functions for accessing attribute memory on the CAM */
	int (*ci_mem_read)(struct aml_ci *ca, int slot, int address);
	int (*ci_mem_write)(struct aml_ci *ca, int slot, int address, u8 value);

	/* functions for accessing the control interface on the CAM */
	int (*ci_io_read)(struct aml_ci *ca, int slot, int address);
	int (*ci_io_write)(struct aml_ci *ca, int slot, int address, u8 value);

	/* Functions for controlling slots */
	int (*ci_slot_reset)(struct aml_ci *ca, int slot);
	int (*ci_slot_shutdown)(struct aml_ci *ca, int slot);
	int (*ci_slot_ts_enable)(struct aml_ci *ca, int slot);

	/*
	* Poll slot status.
	* Only necessary if DVB_CA_FLAG_EN50221_IRQ_CAMCHANGE is not set
	*/
	int (*ci_poll_slot_status)(struct aml_ci *ca, int slot, int open);

	/* private data, used by caller */
	void *data;
};

struct ci_dev_config_s {
	char name[20];
	unsigned char type;
	int cs_hold_delay;
	int cs_clk_delay;
};
extern int aml_ci_init(struct platform_device
	*pdev, struct aml_dvb *dvb, struct aml_ci **cip);
extern void aml_ci_exit(struct aml_ci *ci);

#endif /* __AML_CI_H_ */

