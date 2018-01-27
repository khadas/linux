
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

