/*
 * dvb_ca_cimax.h: generic DVB functions for EN50221 CA CIMAX interfaces
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 */

#ifndef _DVB_CA_EN50221_CIMAX_H_
#define _DVB_CA_EN50221_CIMAX_H_

#include <linux/list.h>
#include <linux/dvb/ca.h>

#include "dvbdev.h"

#define DVB_CA_EN50221_POLL_CAM_PRESENT	1
#define DVB_CA_EN50221_POLL_CAM_CHANGED	2
#define DVB_CA_EN50221_POLL_CAM_READY		4

#define DVB_CA_EN50221_FLAG_IRQ_CAMCHANGE	1
#define DVB_CA_EN50221_FLAG_IRQ_FR		2
#define DVB_CA_EN50221_FLAG_IRQ_DA		4

#define DVB_CA_EN50221_CAMCHANGE_REMOVED		0
#define DVB_CA_EN50221_CAMCHANGE_INSERTED		1

#define DVB_CA_EN50221_CAP_IRQ	1

/* Structure describing a CA interface */
struct dvb_ca_en50221_cimax {

	/* the module owning this structure */
	struct module *owner;

	/* NOTE: the read_*, write_* and poll_slot_status functions will be
	 * called for different slots concurrently and need to use locks where
	 * and if appropriate. There will be no concurrent access to one slot.
	 */

	/* functions for accessing attribute memory on the CAM */
	int (*read_cis)(struct dvb_ca_en50221_cimax *ca,
			int slot, u8 *buf, int size);
	int (*write_cor)(struct dvb_ca_en50221_cimax *ca,
			int slot, int address, u8 *buf);
      /*return the final size or -1 for error*/
	int (*negotiate)(struct dvb_ca_en50221_cimax *ca, int slot, int size);

	/* functions for accessing the control interface on the CAM */
	int (*read_lpdu)(struct dvb_ca_en50221_cimax *ca,
			int slot, u8 *buf, int size);
	int (*write_lpdu)(struct dvb_ca_en50221_cimax *ca,
			int slot, u8 *buf, int size);

	int (*cam_reset)(struct dvb_ca_en50221_cimax *ca, int slot);
	int (*read_cam_status)(struct dvb_ca_en50221_cimax *ca, int slot);
	int (*get_capbility)(struct dvb_ca_en50221_cimax *ca, int slot);

	/* Functions for controlling slots */
	int (*slot_reset)(struct dvb_ca_en50221_cimax *ca, int slot);
	int (*slot_shutdown)(struct dvb_ca_en50221_cimax *ca, int slot);
	int (*slot_ts_enable)(struct dvb_ca_en50221_cimax *ca, int slot);

	/*
	* Poll slot status.
	* Only necessary if DVB_CA_FLAG_EN50221_IRQ_CAMCHANGE is not set
	*/
	int (*poll_slot_status)(struct dvb_ca_en50221_cimax *ca,
			int slot, int open);

	/* private data, used by caller */
	void *data;

	/* Opaque data used by the dvb_ca core. Do not modify! */
	void *private;
};




/* ************************************************************************* */
/* Functions for reporting IRQ events */

/**
 * A CAMCHANGE IRQ has occurred.
 *
 * @param ca CA instance.
 * @param slot Slot concerned.
 * @param change_type One of the DVB_CA_CAMCHANGE_* values
 */
void dvb_ca_en50221_cimax_camchange_irq(struct dvb_ca_en50221_cimax *pubca,
		int slot, int change_type);

/**
 * A CAMREADY IRQ has occurred.
 *
 * @param ca CA instance.
 * @param slot Slot concerned.
 */
void dvb_ca_en50221_cimax_camready_irq(struct dvb_ca_en50221_cimax *pubca,
		int slot);

/**
 * An FR or a DA IRQ has occurred.
 *
 * @param ca CA instance.
 * @param slot Slot concerned.
 */
void dvb_ca_en50221_cimax_frda_irq(struct dvb_ca_en50221_cimax *ca, int slot);



/* ************************************************************************** */
/* Initialisation/shutdown functions */

/**
 * Initialise a new DVB CA device.
 *
 * @param dvb_adapter DVB adapter to attach the new CA device to.
 * @param ca The dvb_ca instance.
 * @param flags Flags describing the CA device (DVB_CA_EN50221_FLAG_*).
 * @param slot_count Number of slots supported.
 *
 * @return 0 on success, nonzero on failure
 */
extern int dvb_ca_en50221_cimax_init(struct dvb_adapter *dvb_adapter,
		struct dvb_ca_en50221_cimax *ca, int flags, int slot_count);

/**
 * Release a DVB CA device.
 *
 * @param ca The associated dvb_ca instance.
 */
extern void dvb_ca_en50221_cimax_release(struct dvb_ca_en50221_cimax *ca);



#endif
