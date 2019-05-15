/*
* Copyright (C) 2017 Amlogic, Inc. All rights reserved.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful, but WITHOUT
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
* FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
* more details.
*
* You should have received a copy of the GNU General Public License along
* with this program; if not, write to the Free Software Foundation, Inc.,
* 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*
* Description:
*/

#ifndef _AML_PCMCIA_
#define _AML_PCMCIA_

enum aml_slot_state {
	MODULE_INSERTED			= 3,
	MODULE_XTRACTED			= 4
};

enum aml_pwr_cmd {
	AML_PWR_OPEN			= 0,
	AML_PWR_CLOSE			= 1
};
enum aml_reset_cmd {
	AML_L	    = 0,
	AML_H		= 1
};
struct aml_pcmcia {
	enum aml_slot_state		slot_state;
	struct work_struct		pcmcia_work;
	int run_type;/*0：irq;1:poll*/
	int irq;
	int (*init_irq)(struct aml_pcmcia *pc, int flag);
	int (*get_cd1)(struct aml_pcmcia *pc);
	int (*get_cd2)(struct aml_pcmcia *pc);
	int (*pwr)(struct aml_pcmcia *pc, int enable);
	int (*rst)(struct aml_pcmcia *pc, int enable);

	int (*pcmcia_plugin)(struct aml_pcmcia *pc, int plugin);

	void *priv;
};

int aml_pcmcia_init(struct aml_pcmcia *pc);
int aml_pcmcia_exit(struct aml_pcmcia *pc);
int aml_pcmcia_reset(struct aml_pcmcia *pc);


#endif /*_AML_PCMCIA_*/

