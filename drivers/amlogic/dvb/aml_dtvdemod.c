// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/amlogic/aml_dtvdemod.h>

#ifdef MODULE
AML_DEMOD_ATTACH_FUNCTION(aml_dtvdm);
#endif
AML_DEMOD_ATTACH_FUNCTION(m1);
AML_DEMOD_ATTACH_FUNCTION(mxl101);
AML_DEMOD_ATTACH_FUNCTION(avl6211);
AML_DEMOD_ATTACH_FUNCTION(si2168);
AML_DEMOD_ATTACH_FUNCTION(ite9133);
AML_DEMOD_ATTACH_FUNCTION(ite9173);
AML_DEMOD_ATTACH_FUNCTION(dib8096);
AML_DEMOD_ATTACH_FUNCTION(atbm8869);
AML_DEMOD_ATTACH_FUNCTION(mxl241);
AML_DEMOD_ATTACH_FUNCTION(avl68xx);
AML_DEMOD_ATTACH_FUNCTION(mxl683);
AML_DEMOD_ATTACH_FUNCTION(atbm8881);
AML_DEMOD_ATTACH_FUNCTION(atbm7821);
AML_DEMOD_ATTACH_FUNCTION(avl6762);
AML_DEMOD_ATTACH_FUNCTION(cxd2856);
AML_DEMOD_ATTACH_FUNCTION(mxl248);
AML_DEMOD_ATTACH_FUNCTION(m88dm6k);
AML_DEMOD_ATTACH_FUNCTION(mn88436);
AML_DEMOD_ATTACH_FUNCTION(mxl212c);
AML_DEMOD_ATTACH_FUNCTION(mxl213c);
AML_DEMOD_ATTACH_FUNCTION(mxl214c);
AML_DEMOD_ATTACH_FUNCTION(mxl252c);
AML_DEMOD_ATTACH_FUNCTION(mxl254c);
AML_DEMOD_ATTACH_FUNCTION(mxl256c);
AML_DEMOD_ATTACH_FUNCTION(mxl258c);
AML_DEMOD_ATTACH_FUNCTION(si2169);
AML_DEMOD_ATTACH_FUNCTION(avl6221c);
AML_DEMOD_ATTACH_FUNCTION(cxd2878);

static struct dvb_frontend *aml_demod_attach(const struct demod_module *module,
		const struct demod_config *cfg);
static int aml_demod_detach(const struct demod_module *module);
static int aml_demod_match(const struct demod_module *module, int std);
static int aml_demod_detect(const struct demod_config *cfg);
static int aml_dvb_register_frontend(struct dvb_adapter *dvb,
		struct dvb_frontend *fe);
static int aml_dvb_unregister_frontend(struct dvb_frontend *fe);

static const struct demod_module demod_modules[] = {
	{
		.name = "internal",
		.id = AM_DTV_DEMOD_AMLDTV,
		.type = { },
		.attach_symbol = NULL,
		.attach = aml_demod_attach,
		.detach = aml_demod_detach,
		.match = aml_demod_match,
		.detect = aml_demod_detect,
		.register_frontend = aml_dvb_register_frontend,
		.unregister_frontend = aml_dvb_unregister_frontend
	},
	{
		.name = "m1",
		.id = AM_DTV_DEMOD_M1,
		.type = { AML_FE_UNDEFINED },
		.attach_symbol = NULL,
		.attach = aml_demod_attach,
		.detach = aml_demod_detach,
		.match = aml_demod_match,
		.detect = aml_demod_detect,
		.register_frontend = aml_dvb_register_frontend,
		.unregister_frontend = aml_dvb_unregister_frontend
	},
	{
		.name = "mxl101",
		.id = AM_DTV_DEMOD_MXL101,
		.type = { FE_OFDM, AML_FE_UNDEFINED },
		.attach_symbol = NULL,
		.attach = aml_demod_attach,
		.detach = aml_demod_detach,
		.match = aml_demod_match,
		.detect = aml_demod_detect,
		.register_frontend = aml_dvb_register_frontend,
		.unregister_frontend = aml_dvb_unregister_frontend
	},
	{
		.name = "avl6211",
		.id = AM_DTV_DEMOD_AVL6211,
		.type = { FE_QPSK, AML_FE_UNDEFINED },
		.attach_symbol = NULL,
		.attach = aml_demod_attach,
		.detach = aml_demod_detach,
		.match = aml_demod_match,
		.detect = aml_demod_detect,
		.register_frontend = aml_dvb_register_frontend,
		.unregister_frontend = aml_dvb_unregister_frontend
	},
	{
		.name = "si2168",
		.id = AM_DTV_DEMOD_SI2168,
		.type = { FE_QAM, FE_OFDM, AML_FE_UNDEFINED },
		.attach_symbol = NULL,
		.attach = aml_demod_attach,
		.detach = aml_demod_detach,
		.match = aml_demod_match,
		.detect = aml_demod_detect,
		.register_frontend = aml_dvb_register_frontend,
		.unregister_frontend = aml_dvb_unregister_frontend
	},
	{
		.name = "ite9173",
		.id = AM_DTV_DEMOD_ITE9173,
		.type = { FE_ISDBT, AML_FE_UNDEFINED },
		.attach_symbol = NULL,
		.attach = aml_demod_attach,
		.detach = aml_demod_detach,
		.match = aml_demod_match,
		.detect = aml_demod_detect,
		.register_frontend = aml_dvb_register_frontend,
		.unregister_frontend = aml_dvb_unregister_frontend
	},
	{
		.name = "dib8096",
		.id = AM_DTV_DEMOD_DIB8096,
		.type = { FE_ISDBT, AML_FE_UNDEFINED },
		.attach_symbol = NULL,
		.attach = aml_demod_attach,
		.detach = aml_demod_detach,
		.match = aml_demod_match,
		.detect = aml_demod_detect,
		.register_frontend = aml_dvb_register_frontend,
		.unregister_frontend = aml_dvb_unregister_frontend
	},
	{
		.name = "atbm8869",
		.id = AM_DTV_DEMOD_ATBM8869,
		.type = { FE_DTMB, AML_FE_UNDEFINED },
		.attach_symbol = NULL,
		.attach = aml_demod_attach,
		.detach = aml_demod_detach,
		.match = aml_demod_match,
		.detect = aml_demod_detect,
		.register_frontend = aml_dvb_register_frontend,
		.unregister_frontend = aml_dvb_unregister_frontend
	},
	{
		.name = "mxl241",
		.id = AM_DTV_DEMOD_MXL241,
		.type = { FE_QAM, AML_FE_UNDEFINED },
		.attach_symbol = NULL,
		.attach = aml_demod_attach,
		.detach = aml_demod_detach,
		.match = aml_demod_match,
		.detect = aml_demod_detect,
		.register_frontend = aml_dvb_register_frontend,
		.unregister_frontend = aml_dvb_unregister_frontend
	},
	{
		.name = "avl68xx",
		.id = AM_DTV_DEMOD_AVL68xx,
		.type = { },
		.attach_symbol = NULL,
		.attach = aml_demod_attach,
		.detach = aml_demod_detach,
		.match = aml_demod_match,
		.detect = aml_demod_detect,
		.register_frontend = aml_dvb_register_frontend,
		.unregister_frontend = aml_dvb_unregister_frontend
	},
	{
		.name = "mxl683",
		.id = AM_DTV_DEMOD_MXL683,
		.type = { FE_ISDBT, AML_FE_UNDEFINED },
		.attach_symbol = NULL,
		.attach = aml_demod_attach,
		.detach = aml_demod_detach,
		.match = aml_demod_match,
		.detect = aml_demod_detect,
		.register_frontend = aml_dvb_register_frontend,
		.unregister_frontend = aml_dvb_unregister_frontend
	},
	{
		.name = "atbm8881",
		.id = AM_DTV_DEMOD_ATBM8881,
		.type = { FE_QAM, FE_OFDM, FE_DTMB, AML_FE_UNDEFINED },
		.attach_symbol = NULL,
		.attach = aml_demod_attach,
		.detach = aml_demod_detach,
		.match = aml_demod_match,
		.detect = aml_demod_detect,
		.register_frontend = aml_dvb_register_frontend,
		.unregister_frontend = aml_dvb_unregister_frontend
	},
	{
		.name = "atbm7821",
		.id = AM_DTV_DEMOD_ATBM7821,
		.type = { FE_QAM, FE_OFDM, AML_FE_UNDEFINED },
		.attach_symbol = NULL,
		.attach = aml_demod_attach,
		.detach = aml_demod_detach,
		.match = aml_demod_match,
		.detect = aml_demod_detect,
		.register_frontend = aml_dvb_register_frontend,
		.unregister_frontend = aml_dvb_unregister_frontend
	},
	{
		.name = "avl6762",
		.id = AM_DTV_DEMOD_AVL6762,
		.type = { FE_QAM, FE_OFDM, AML_FE_UNDEFINED },
		.attach_symbol = NULL,
		.attach = aml_demod_attach,
		.detach = aml_demod_detach,
		.match = aml_demod_match,
		.detect = aml_demod_detect,
		.register_frontend = aml_dvb_register_frontend,
		.unregister_frontend = aml_dvb_unregister_frontend
	},
	{
		.name = "cxd2856",
		.id = AM_DTV_DEMOD_CXD2856,
		.type = { FE_QPSK, FE_QAM, FE_OFDM, AML_FE_UNDEFINED },
		.attach_symbol = NULL,
		.attach = aml_demod_attach,
		.detach = aml_demod_detach,
		.match = aml_demod_match,
		.detect = aml_demod_detect,
		.register_frontend = aml_dvb_register_frontend,
		.unregister_frontend = aml_dvb_unregister_frontend
	},
	{
		.name = "mxl248",
		.id = AM_DTV_DEMOD_MXL248,
		.type = { FE_QAM, AML_FE_UNDEFINED },
		.attach_symbol = NULL,
		.attach = aml_demod_attach,
		.detach = aml_demod_detach,
		.match = aml_demod_match,
		.detect = aml_demod_detect,
		.register_frontend = aml_dvb_register_frontend,
		.unregister_frontend = aml_dvb_unregister_frontend
	},
	{
		.name = "m88dm6k",
		.id = AM_DTV_DEMOD_M88DM6K,
		.type = { FE_QPSK, FE_QAM, FE_OFDM, AML_FE_UNDEFINED },
		.attach_symbol = NULL,
		.attach = aml_demod_attach,
		.detach = aml_demod_detach,
		.match = aml_demod_match,
		.detect = aml_demod_detect,
		.register_frontend = aml_dvb_register_frontend,
		.unregister_frontend = aml_dvb_unregister_frontend
	},
	{
		.name = "mn88436",
		.id = AM_DTV_DEMOD_MN88436,
		.type = { FE_ATSC, AML_FE_UNDEFINED },
		.attach_symbol = NULL,
		.attach = aml_demod_attach,
		.detach = aml_demod_detach,
		.match = aml_demod_match,
		.detect = aml_demod_detect,
		.register_frontend = aml_dvb_register_frontend,
		.unregister_frontend = aml_dvb_unregister_frontend
	},
	{
		.name = "mxl212c",
		.id = AM_DTV_DEMOD_MXL212C,
		.type = { FE_QAM, AML_FE_UNDEFINED },
		.attach_symbol = NULL,
		.attach = aml_demod_attach,
		.detach = aml_demod_detach,
		.match = aml_demod_match,
		.detect = aml_demod_detect,
		.register_frontend = aml_dvb_register_frontend,
		.unregister_frontend = aml_dvb_unregister_frontend
	},
	{
		.name = "mxl213c",
		.id = AM_DTV_DEMOD_MXL213C,
		.type = { FE_QAM, AML_FE_UNDEFINED },
		.attach_symbol = NULL,
		.attach = aml_demod_attach,
		.detach = aml_demod_detach,
		.match = aml_demod_match,
		.detect = aml_demod_detect,
		.register_frontend = aml_dvb_register_frontend,
		.unregister_frontend = aml_dvb_unregister_frontend
	},
	{
		.name = "mxl214c",
		.id = AM_DTV_DEMOD_MXL214C,
		.type = { FE_QAM, AML_FE_UNDEFINED },
		.attach_symbol = NULL,
		.attach = aml_demod_attach,
		.detach = aml_demod_detach,
		.match = aml_demod_match,
		.detect = aml_demod_detect,
		.register_frontend = aml_dvb_register_frontend,
		.unregister_frontend = aml_dvb_unregister_frontend
	},
	{
		.name = "mxl252c",
		.id = AM_DTV_DEMOD_MXL252C,
		.type = { FE_QAM, AML_FE_UNDEFINED },
		.attach_symbol = NULL,
		.attach = aml_demod_attach,
		.detach = aml_demod_detach,
		.match = aml_demod_match,
		.detect = aml_demod_detect,
		.register_frontend = aml_dvb_register_frontend,
		.unregister_frontend = aml_dvb_unregister_frontend
	},
	{
		.name = "mxl254c",
		.id = AM_DTV_DEMOD_MXL254C,
		.type = { FE_QAM, AML_FE_UNDEFINED },
		.attach_symbol = NULL,
		.attach = aml_demod_attach,
		.detach = aml_demod_detach,
		.match = aml_demod_match,
		.detect = aml_demod_detect,
		.register_frontend = aml_dvb_register_frontend,
		.unregister_frontend = aml_dvb_unregister_frontend
	},
	{
		.name = "mxl256c",
		.id = AM_DTV_DEMOD_MXL256C,
		.type = { FE_QAM, AML_FE_UNDEFINED },
		.attach_symbol = NULL,
		.attach = aml_demod_attach,
		.detach = aml_demod_detach,
		.match = aml_demod_match,
		.detect = aml_demod_detect,
		.register_frontend = aml_dvb_register_frontend,
		.unregister_frontend = aml_dvb_unregister_frontend
	},
	{
		.name = "mxl258c",
		.id = AM_DTV_DEMOD_MXL258C,
		.type = { FE_QAM, AML_FE_UNDEFINED },
		.attach_symbol = NULL,
		.attach = aml_demod_attach,
		.detach = aml_demod_detach,
		.match = aml_demod_match,
		.detect = aml_demod_detect,
		.register_frontend = aml_dvb_register_frontend,
		.unregister_frontend = aml_dvb_unregister_frontend
	},
	{
		.name = "si2169",
		.id = AM_DTV_DEMOD_SI2169,
		.type = { FE_QPSK, FE_QAM, FE_OFDM, AML_FE_UNDEFINED },
		.attach_symbol = NULL,
		.attach = aml_demod_attach,
		.detach = aml_demod_detach,
		.match = aml_demod_match,
		.detect = aml_demod_detect,
		.register_frontend = aml_dvb_register_frontend,
		.unregister_frontend = aml_dvb_unregister_frontend
	},
	{
		.name = "avl6221c",
		.id = AM_DTV_DEMOD_AVL6221C,
		.type = { FE_QPSK, AML_FE_UNDEFINED },
		.attach_symbol = NULL,
		.attach = aml_demod_attach,
		.detach = aml_demod_detach,
		.match = aml_demod_match,
		.detect = aml_demod_detect,
		.register_frontend = aml_dvb_register_frontend,
		.unregister_frontend = aml_dvb_unregister_frontend
	},
	{
		.name = "cxd2878",
		.id = AM_DTV_DEMOD_CXD2878,
		.type = { FE_QPSK, FE_QAM, FE_OFDM, FE_ATSC, FE_ISDBT, AML_FE_UNDEFINED },
		.attach_symbol = NULL,
		.attach = aml_demod_attach,
		.detach = aml_demod_detach,
		.match = aml_demod_match,
		.detect = aml_demod_detect,
		.register_frontend = aml_dvb_register_frontend,
		.unregister_frontend = aml_dvb_unregister_frontend
	}
};

static dm_attach_cb pt[AM_DTV_DEMOD_MAX];

void aml_set_demod_attach_cb(const enum dtv_demod_type type, dm_attach_cb funcb)
{
	if (type > AM_DTV_DEMOD_NONE && type < AM_DTV_DEMOD_MAX && !pt[type])
		pt[type] = funcb;
}

static struct dvb_frontend *aml_attach_detach_dtvdemod(
		enum dtv_demod_type type,
		const struct demod_config *cfg,
		int attch)
{
	struct dvb_frontend *p = 0;

	if (pt[type]) {
		pr_err("%s: cb id %d\n", __func__, type);
		return pt[type](cfg);
	}

	switch (type) {
	case AM_DTV_DEMOD_AMLDTV:
		attch ? (p = aml_dvb_attach(aml_dtvdm_attach, cfg)) :
				aml_dvb_detach(aml_dtvdm_attach);
		break;
	case AM_DTV_DEMOD_M1:
		attch ? (p = aml_dvb_attach(m1_attach, cfg)) :
				aml_dvb_detach(m1_attach);
		break;
	case AM_DTV_DEMOD_MXL101:
		attch ? (p = aml_dvb_attach(mxl101_attach, cfg)) :
				aml_dvb_detach(mxl101_attach);
		break;
	case AM_DTV_DEMOD_AVL6211:
		attch ? (p = aml_dvb_attach(avl6211_attach, cfg)) :
				aml_dvb_detach(avl6211_attach);
		break;
	case AM_DTV_DEMOD_SI2168:
		attch ? (p = aml_dvb_attach(si2168_attach, cfg)) :
				aml_dvb_detach(si2168_attach);
		break;
	case AM_DTV_DEMOD_ITE9133:
		attch ? (p = aml_dvb_attach(ite9133_attach, cfg)) :
				aml_dvb_detach(ite9133_attach);
		break;
	case AM_DTV_DEMOD_ITE9173:
		attch ? (p = aml_dvb_attach(ite9173_attach, cfg)) :
				aml_dvb_detach(ite9173_attach);
		break;
	case AM_DTV_DEMOD_DIB8096:
		attch ? (p = aml_dvb_attach(dib8096_attach, cfg)) :
				aml_dvb_detach(dib8096_attach);
		break;
	case AM_DTV_DEMOD_ATBM8869:
		attch ? (p = aml_dvb_attach(atbm8869_attach, cfg)) :
				aml_dvb_detach(atbm8869_attach);
		break;
	case AM_DTV_DEMOD_MXL241:
		attch ? (p = aml_dvb_attach(mxl241_attach, cfg)) :
				aml_dvb_detach(mxl241_attach);
		break;
	case AM_DTV_DEMOD_AVL68xx:
		attch ? (p = aml_dvb_attach(avl68xx_attach, cfg)) :
				aml_dvb_detach(avl68xx_attach);
		break;
	case AM_DTV_DEMOD_MXL683:
		attch ? (p = aml_dvb_attach(mxl683_attach, cfg)) :
				aml_dvb_detach(mxl683_attach);
		break;
	case AM_DTV_DEMOD_ATBM8881:
		attch ? (p = aml_dvb_attach(atbm8881_attach, cfg)) :
				aml_dvb_detach(atbm8881_attach);
		break;
	case AM_DTV_DEMOD_ATBM7821:
		attch ? (p = aml_dvb_attach(atbm7821_attach, cfg)) :
				aml_dvb_detach(atbm7821_attach);
		break;
	case AM_DTV_DEMOD_AVL6762:
		attch ? (p = aml_dvb_attach(avl6762_attach, cfg)) :
				aml_dvb_detach(avl6762_attach);
		break;
	case AM_DTV_DEMOD_CXD2856:
		attch ? (p = aml_dvb_attach(cxd2856_attach, cfg)) :
				aml_dvb_detach(cxd2856_attach);
		break;
	case AM_DTV_DEMOD_MXL248:
		attch ? (p = aml_dvb_attach(mxl248_attach, cfg)) :
				aml_dvb_detach(mxl248_attach);
		break;
	case AM_DTV_DEMOD_M88DM6K:
		attch ? (p = aml_dvb_attach(m88dm6k_attach, cfg)) :
				aml_dvb_detach(m88dm6k_attach);
		break;
	case AM_DTV_DEMOD_MN88436:
		attch ? (p = aml_dvb_attach(mn88436_attach, cfg)) :
				aml_dvb_detach(mn88436_attach);
		break;
	case AM_DTV_DEMOD_MXL212C:
		attch ? (p = aml_dvb_attach(mxl212c_attach, cfg)) :
				aml_dvb_detach(mxl212c_attach);
		break;
	case AM_DTV_DEMOD_MXL213C:
		attch ? (p = aml_dvb_attach(mxl213c_attach, cfg)) :
				aml_dvb_detach(mxl213c_attach);
		break;
	case AM_DTV_DEMOD_MXL214C:
		attch ? (p = aml_dvb_attach(mxl214c_attach, cfg)) :
				aml_dvb_detach(mxl214c_attach);
		break;
	case AM_DTV_DEMOD_MXL252C:
		attch ? (p = aml_dvb_attach(mxl252c_attach, cfg)) :
				aml_dvb_detach(mxl252c_attach);
		break;
	case AM_DTV_DEMOD_MXL254C:
		attch ? (p = aml_dvb_attach(mxl254c_attach, cfg)) :
				aml_dvb_detach(mxl254c_attach);
		break;
	case AM_DTV_DEMOD_MXL256C:
		attch ? (p = aml_dvb_attach(mxl256c_attach, cfg)) :
				aml_dvb_detach(mxl256c_attach);
		break;
	case AM_DTV_DEMOD_MXL258C:
		attch ? (p = aml_dvb_attach(mxl258c_attach, cfg)) :
				aml_dvb_detach(mxl258c_attach);
		break;
	case AM_DTV_DEMOD_SI2169:
		attch ? (p = aml_dvb_attach(si2169_attach, cfg)) :
				aml_dvb_detach(si2169_attach);
		break;
	case AM_DTV_DEMOD_AVL6221C:
		attch ? (p = aml_dvb_attach(avl6221c_attach, cfg)) :
				aml_dvb_detach(avl6221c_attach);
		break;
	case AM_DTV_DEMOD_CXD2878:
		attch ? (p = aml_dvb_attach(cxd2878_attach, cfg)) :
				aml_dvb_detach(cxd2878_attach);
		break;
	default:
		p = NULL;
		break;
	}

	return p;
}

static struct dvb_frontend *aml_demod_attach(const struct demod_module *module,
		const struct demod_config *cfg)
{
	if (!IS_ERR_OR_NULL(module))
		return aml_attach_detach_dtvdemod(module->id, cfg, true);

	return NULL;
}

static int aml_demod_detach(const struct demod_module *module)
{
	if (!IS_ERR_OR_NULL(module))
		aml_attach_detach_dtvdemod(module->id, NULL, false);

	return 0;
}

static int aml_demod_match(const struct demod_module *module, int std)
{
	int n = 0;

	if (IS_ERR_OR_NULL(module))
		return -EFAULT;

	/* std = FE type. Now use this method. */
	n = 0;
	while (n < AML_MAX_FE && module->type[n] != AML_FE_UNDEFINED) {
		if (module->type[n] == std)
			return 0;

		n++;
	}

	return -EINVAL;
}

static int aml_demod_detect(const struct demod_config *cfg)
{
	return 0;
}

static int aml_dvb_register_frontend(struct dvb_adapter *dvb,
		struct dvb_frontend *fe)
{
	return dvb_register_frontend(dvb, fe);
}

static int aml_dvb_unregister_frontend(struct dvb_frontend *fe)
{
	return dvb_unregister_frontend(fe);
}

int aml_get_dts_demod_config(struct device_node *node,
		struct demod_config *cfg, int index)
{
	int ret = 0, i = 0;
	u32 value = 0;
	u32 data[32] = { 0 };
	struct device_node *node_i2c = NULL;
	char buf[32] = { 0 };
	const char *str = NULL;

	if (IS_ERR_OR_NULL(node) || IS_ERR_OR_NULL(cfg)) {
		pr_err("Demod: NULL or error pointer of node or cfg.\n");

		return -EFAULT;
	}

	memset(buf, 0, 32);
	snprintf(buf, sizeof(buf), "fe%d_demod", index);
	ret = of_property_read_string(node, buf, &str);
	if (!ret) {
		cfg->id = aml_get_dtvdemod_type(str);
		if (cfg->id == AM_DTV_DEMOD_NONE)
			pr_err("Demod: can't support demod type: %s.\n", str);
	} else {
		pr_err("Demod: can't get %s ret[%d].\n", buf, ret);

		cfg->id = AM_DTV_DEMOD_NONE;

		return ret;
	}

	memset(buf, 0, 32);
	snprintf(buf, sizeof(buf), "fe%d_i2c_adap_id", index);
	node_i2c = of_parse_phandle(node, buf, 0);
	if (!IS_ERR_OR_NULL(node_i2c)) {
		cfg->i2c_adap = of_find_i2c_adapter_by_node(node_i2c);
		of_node_put(node_i2c);
		if (IS_ERR_OR_NULL(cfg->i2c_adap)) {
			pr_err("Demod: can't get i2c_adapter ret[NULL].\n");

			//return -1;
		} else
			cfg->i2c_id = i2c_adapter_id(cfg->i2c_adap);
	} else {
		pr_err("Demod: can't get %s ret[NULL].\n", buf);

		cfg->i2c_adap = NULL;

		//return -1;
	}

	memset(buf, 0, 32);
	snprintf(buf, sizeof(buf), "fe%d_demod_i2c_addr", index);
	ret = of_property_read_u32(node, buf, &value);
	if (!ret) {
		cfg->i2c_addr = value;
	} else {
		pr_err("Demod: can't get %s ret[%d].\n", buf, ret);

		cfg->i2c_addr = 0;
		ret = 0;

		//return ret;
	}

	memset(buf, 0, 32);
	snprintf(buf, sizeof(buf), "fe%d_ts", index);
	ret = of_property_read_u32(node, buf, &value);
	if (!ret) {
		cfg->ts = value;
	} else {
		pr_err("Demod: can't get %s ret[%d].\n", buf, ret);

		cfg->ts = 0;
		ret = 0;

		//return ret;
	}

	memset(buf, 0, 32);
	snprintf(buf, sizeof(buf), "fe%d_demod_code", index);
	ret = of_property_read_u32(node, buf, &value);
	if (!ret) {
		cfg->code = value;
	} else {
		//pr_err("Demod: can't get %s ret[%d].\n", buf, ret);

		cfg->code = 0;
		ret = 0;
	}

	memset(buf, 0, 32);
	snprintf(buf, sizeof(buf), "fe%d_demod_xtal", index);
	ret = of_property_read_u32(node, buf, &value);
	if (!ret) {
		cfg->xtal = value;
	} else {
		//pr_err("Demod: can't get %s ret[%d].\n", buf, ret);

		cfg->xtal = 0;
		ret = 0;
	}

	memset(buf, 0, 32);
	snprintf(buf, sizeof(buf), "fe%d_ts_out_mode", index);
	ret = of_property_read_u32(node, buf, &value);
	if (!ret) {
		cfg->ts_out_mode = value;
	} else {
		//pr_err("Demod: can't get %s ret[%d].\n", buf, ret);

		cfg->ts_out_mode = 0;
		ret = 0;
	}

	memset(buf, 0, 32);
	snprintf(buf, sizeof(buf), "fe%d_ts_data_pin", index);
	ret = of_property_read_u32(node, buf, &value);
	if (!ret) {
		cfg->ts_data_pin = value;
	} else {
		//pr_err("Demod: can't get %s ret[%d].\n", buf, ret);

		cfg->ts_data_pin = 0;
		ret = 0;
	}

	memset(buf, 0, 32);
	snprintf(buf, sizeof(buf), "fe%d_ts_wire_mode", index);
	ret = of_property_read_u32(node, buf, &value);
	if (!ret) {
		cfg->ts_wire_mode = value;
	} else {
		//pr_err("Demod: can't get %s ret[%d].\n", buf, ret);

		cfg->ts_wire_mode = 0;
		ret = 0;
	}

	memset(buf, 0, 32);
	snprintf(buf, sizeof(buf), "fe%d_ts_mux_mode", index);
	ret = of_property_read_u32(node, buf, &value);
	if (!ret) {
		cfg->ts_mux_mode = value;
	} else {
		//pr_err("Demod: can't get %s ret[%d].\n", buf, ret);

		cfg->ts_mux_mode = 0;
		ret = 0;
	}

	memset(buf, 0, 32);
	snprintf(buf, sizeof(buf), "fe%d_ts_out_order", index);
	ret = of_property_read_u32(node, buf, &value);
	if (!ret) {
		cfg->ts_out_order = value;
	} else {
		//pr_err("Demod: can't get %s ret[%d].\n", buf, ret);

		cfg->ts_out_order = 0;
		ret = 0;
	}

	memset(buf, 0, 32);
	snprintf(buf, sizeof(buf), "fe%d_ts_out_bits", index);
	ret = of_property_read_u32(node, buf, &value);
	if (!ret) {
		cfg->ts_out_bits = value;
	} else {
		//pr_err("Demod: can't get %s ret[%d].\n", buf, ret);

		cfg->ts_out_bits = 0;
		ret = 0;
	}

	memset(buf, 0, 32);
	snprintf(buf, sizeof(buf), "fe%d_ts_sync_width", index);
	ret = of_property_read_u32(node, buf, &value);
	if (!ret) {
		cfg->ts_sync_width = value;
	} else {
		//pr_err("Demod: can't get %s ret[%d].\n", buf, ret);

		cfg->ts_sync_width = 0;
		ret = 0;
	}

	memset(buf, 0, 32);
	snprintf(buf, sizeof(buf), "fe%d_ts_packet_mode", index);
	ret = of_property_read_u32(node, buf, &value);
	if (!ret) {
		cfg->ts_packet_mode = value;
	} else {
		//pr_err("Demod: can't get %s ret[%d].\n", buf, ret);

		cfg->ts_packet_mode = 0;
		ret = 0;
	}

	memset(buf, 0, 32);
	if (cfg->ts_packet_mode) {
		snprintf(buf, sizeof(buf), "fe%d_ts_remap_cnt", index);
		ret = of_property_read_u32(node, buf, &value);
		if (!ret) {
			cfg->ts_remap_cnt = value;
		} else {
			//pr_err("Demod: can't get %s ret[%d].\n", buf, ret);

			cfg->ts_remap_cnt = 0;
			ret = 0;
		}

		if (cfg->ts_remap_cnt)
			;/* TODO: need init in dvb */

	} else {
		snprintf(buf, sizeof(buf), "fe%d_ts_header_bytes", index);
		ret = of_property_read_u32(node, buf, &value);
		if (!ret) {
			cfg->ts_header_bytes = value;
		} else {
			//pr_err("Demod: can't get %s ret[%d].\n", buf, ret);

			cfg->ts_header_bytes = 0;
			ret = 0;
		}

		if (cfg->ts_header_bytes) {
			memset(buf, 0, 32);
			snprintf(buf, sizeof(buf), "fe%d_ts_header_data",
					index);
			ret = of_property_read_u32_array(node, buf, data,
					cfg->ts_header_bytes);
			if (!ret) {
				for (i = 0; i < cfg->ts_header_bytes; ++i)
					cfg->ts_header_data[i] = data[i] & 0xFF;
			} else {
				//pr_err("Demod: can't get %s ret[%d].\n", buf, ret);

				memset(cfg->ts_header_data, 0,
						sizeof(cfg->ts_header_data));

				ret = 0;
			}
		}
	}

	memset(buf, 0, 32);
	snprintf(buf, sizeof(buf), "fe%d_ts_clk", index);
	ret = of_property_read_u32(node, buf, &value);
	if (!ret) {
		cfg->ts_clk = value;
	} else {
		//pr_err("Demod: can't get %s ret[%d].\n", buf, ret);

		cfg->ts_clk = 0;
		ret = 0;
	}

	memset(buf, 0, 32);
	snprintf(buf, sizeof(buf), "fe%d_ts_clk_pol", index);
	ret = of_property_read_u32(node, buf, &value);
	if (!ret) {
		cfg->ts_clk_pol = value;
	} else {
		//pr_err("Demod: can't get %s ret[%d].\n", buf, ret);

		cfg->ts_clk_pol = 0;
		ret = 0;
	}

	memset(buf, 0, 32);
	snprintf(buf, sizeof(buf), "fe%d_detect", index);
	ret = of_property_read_u32(node, buf, &value);
	if (!ret) {
		cfg->detect = value;
	} else {
		//pr_err("Demod: can't get %s ret[%d].\n", buf, ret);

		cfg->detect = 0;
		ret = 0;
	}

	memset(buf, 0, 32);
	str = NULL;
	snprintf(buf, sizeof(buf), "fe%d_reset_gpio", index);
	ret = of_property_read_string(node, buf, &str);
	if (ret) {
		//pr_err("Demod: can't get %s ret[%d].\n", buf, ret);

		cfg->reset.pin = -1;
		ret = 0;
	} else {
		cfg->reset.pin = of_get_named_gpio_flags(node, buf, 0, NULL);
		pr_err("Demod: get %s: %d.\n", buf, cfg->reset.pin);
	}

	memset(buf, 0, 32);
	snprintf(buf, sizeof(buf), "fe%d_reset_dir", index);
	ret = of_property_read_u32(node, buf, &value);
	if (ret) {
		//pr_err("Demod: can't get %s ret[%d].\n", buf, ret);

		cfg->reset.dir = 0;
		ret = 0;
	} else
		cfg->reset.dir = value;

	memset(buf, 0, 32);
	snprintf(buf, sizeof(buf), "fe%d_reset_value", index);
	ret = of_property_read_u32(node, buf, &value);
	if (ret) {
		//pr_err("Demod: can't get %s ret[%d].\n", buf, ret);

		cfg->reset.value = 0;
		ret = 0;
	} else
		cfg->reset.value = value;

	memset(buf, 0, 32);
	str = NULL;
	snprintf(buf, sizeof(buf), "fe%d_ant_power_gpio", index);
	ret = of_property_read_string(node, buf, &str);
	if (ret) {
		//pr_err("Demod: can't get %s ret[%d].\n", buf, ret);

		cfg->ant_power.pin = -1;
		ret = 0;
	} else {
		cfg->ant_power.pin = of_get_named_gpio_flags(
				node, buf, 0, NULL);
		pr_err("Demod: get %s: %d\n", buf, cfg->ant_power.pin);
	}

	memset(buf, 0, 32);
	snprintf(buf, sizeof(buf), "fe%d_ant_power_dir", index);
	ret = of_property_read_u32(node, buf, &value);
	if (ret) {
		//pr_err("Demod: can't get %s ret[%d].\n", buf, ret);

		cfg->ant_power.dir = 0;
		ret = 0;
	} else
		cfg->ant_power.dir = value;

	memset(buf, 0, 32);
	snprintf(buf, sizeof(buf), "fe%d_ant_poweron_value", index);
	ret = of_property_read_u32(node, buf, &value);
	if (ret) {
		//pr_err("Demod: can't get %s ret[%d].\n", buf, ret);

		cfg->ant_power.value = 0;
		ret = 0;
	} else
		cfg->ant_power.value = value;

	memset(buf, 0, 32);
	str = NULL;
	snprintf(buf, sizeof(buf), "fe%d_lnb_en_gpio", index);
	ret = of_property_read_string(node, buf, &str);
	if (ret) {
		//pr_err("Demod: can't get %s ret[%d].\n", buf, ret);

		cfg->lnb_en.pin = -1;
		ret = 0;
	} else {
		cfg->lnb_en.pin = of_get_named_gpio_flags(
				node, buf, 0, NULL);
		pr_err("Demod: get %s: %d\n", buf, cfg->lnb_en.pin);
	}

	memset(buf, 0, 32);
	snprintf(buf, sizeof(buf), "fe%d_lnb_en_dir", index);
	ret = of_property_read_u32(node, buf, &value);
	if (ret) {
		//pr_err("Demod: can't get %s ret[%d].\n", buf, ret);

		cfg->lnb_en.dir = 0;
		ret = 0;
	} else {
		cfg->lnb_en.dir = value;
	}

	memset(buf, 0, 32);
	snprintf(buf, sizeof(buf), "fe%d_lnb_en_value", index);
	ret = of_property_read_u32(node, buf, &value);
	if (ret) {
		//pr_err("Demod: can't get %s ret[%d].\n", buf, ret);

		cfg->lnb_en.value = 0;
		ret = 0;
	} else {
		cfg->lnb_en.value = value;
	}

	memset(buf, 0, 32);
	str = NULL;
	snprintf(buf, sizeof(buf), "fe%d_lnb_sel_gpio", index);
	ret = of_property_read_string(node, buf, &str);
	if (ret) {
		//pr_err("Demod: can't get %s ret[%d].\n", buf, ret);

		cfg->lnb_sel.pin = -1;
		ret = 0;
	} else {
		cfg->lnb_sel.pin = of_get_named_gpio_flags(
				node, buf, 0, NULL);
		pr_err("Demod: get %s: %d\n", buf, cfg->lnb_sel.pin);
	}

	memset(buf, 0, 32);
	snprintf(buf, sizeof(buf), "fe%d_lnb_sel_dir", index);
	ret = of_property_read_u32(node, buf, &value);
	if (ret) {
		//pr_err("Demod: can't get %s ret[%d].\n", buf, ret);

		cfg->lnb_sel.dir = 0;
		ret = 0;
	} else {
		cfg->lnb_sel.dir = value;
	}

	memset(buf, 0, 32);
	snprintf(buf, sizeof(buf), "fe%d_lnb_sel_value", index);
	ret = of_property_read_u32(node, buf, &value);
	if (ret) {
		//pr_err("Demod: can't get %s ret[%d].\n", buf, ret);

		cfg->lnb_sel.value = 0;
		ret = 0;
	} else {
		cfg->lnb_sel.value = value;
	}

	memset(buf, 0, 32);
	str = NULL;
	snprintf(buf, sizeof(buf), "fe%d_other_gpio", index);
	ret = of_property_read_string(node, buf, &str);
	if (ret) {
		//pr_err("Demod: can't get %s ret[%d].\n", buf, ret);

		cfg->other.pin = -1;
		ret = 0;
	} else {
		cfg->other.pin = of_get_named_gpio_flags(
				node, buf, 0, NULL);
		pr_err("Demod: get %s: %d\n", buf, cfg->other.pin);
	}

	memset(buf, 0, 32);
	snprintf(buf, sizeof(buf), "fe%d_other_dir", index);
	ret = of_property_read_u32(node, buf, &value);
	if (ret) {
		//pr_err("Demod: can't get %s ret[%d].\n", buf, ret);

		cfg->other.dir = 0;
		ret = 0;
	} else {
		cfg->other.dir = value;
	}

	memset(buf, 0, 32);
	snprintf(buf, sizeof(buf), "fe%d_other_value", index);
	ret = of_property_read_u32(node, buf, &value);
	if (ret) {
		//pr_err("Demod: can't get %s ret[%d].\n", buf, ret);

		cfg->other.value = 0;
		ret = 0;
	} else {
		cfg->other.value = value;
	}

	memset(buf, 0, 32);
	snprintf(buf, sizeof(buf), "fe%d_reserved0", index);
	ret = of_property_read_u32(node, buf, &value);
	if (ret) {
		//pr_err("Demod: can't get %s ret[%d].\n", buf, ret);

		cfg->reserved0 = 0;
		ret = 0;
	} else {
		cfg->reserved0 = value;
	}

	memset(buf, 0, 32);
	snprintf(buf, sizeof(buf), "fe%d_reserved1", index);
	ret = of_property_read_u32(node, buf, &value);
	if (ret) {
		//pr_err("Demod: can't get %s ret[%d].\n", buf, ret);

		cfg->reserved1 = 0;
		ret = 0;
	} else {
		cfg->reserved1 = value;
	}

	return 0;
}
EXPORT_SYMBOL(aml_get_dts_demod_config);

void aml_show_demod_config(const char *title, const struct demod_config *cfg)
{
	int i = 0;

	if (IS_ERR_OR_NULL(title) || IS_ERR_OR_NULL(cfg))
		return;

	pr_err("[%s] name: %s, type: %d, code: 0x%x, mode: %d (0:Internal, 1:External).\n",
			title,
			cfg->name ? cfg->name : "",
			cfg->id,
			cfg->code,
			cfg->mode);
	pr_err("[%s] xtal %d.\n", title, cfg->xtal);
	pr_err("[%s] ts: %d, ts_out_mode: %d (0:Serial, 1:Parallel).\n",
			title, cfg->ts, cfg->ts_out_mode);
	pr_err("[%s] ts_data_pin: %d, ts_out_order: %d (0:LSB, 1:MSB).\n",
			title, cfg->ts_data_pin, cfg->ts_out_order);
	pr_err("[%s] ts_wire_mode: %d (0:Four-wire, 1:Three-wire).\n",
			title, cfg->ts_wire_mode);
	pr_err("[%s] ts_mux_mode: %d (0:NO_MUX_4, 1:MUX_1, 2:MUX_2, 3:MUX_3, 4:MUX_4, 5:MUX_2_B, 6:NO_MUX_2).\n",
			title, cfg->ts_mux_mode);
	pr_err("[%s] ts_out_bits: %d, ts_sync_width: %d (0:Bit, 1:Byte).\n",
			title, cfg->ts_out_bits, cfg->ts_sync_width);
	pr_err("[%s] ts_packet_mode: %d (0:Add header, 1:Remapping pid).\n",
			title, cfg->ts_packet_mode);
	if (cfg->ts_packet_mode) {
		pr_err("[%s] ts_remap_cnt: %d.\n", title, cfg->ts_remap_cnt);
		for (i = 0; i < cfg->ts_remap_cnt && cfg->ts_pid && cfg->ts_remap_pid; ++i)
			pr_err("[%s] ts_pid: 0x%x, ts_remap_pid: 0x%x.\n",
					title, cfg->ts_pid[i], cfg->ts_remap_pid[i]);
	} else {
		pr_err("[%s] ts_header_bytes: %d.\n", title, cfg->ts_header_bytes);
		for (i = 0; i < cfg->ts_header_bytes; ++i)
			pr_err("[%s] ts_header_data: 0x%x.\n",
					title, cfg->ts_header_data[i]);
	}
	pr_err("[%s] ts_clk: %d, ts_clk_pol: %d (0:Negative, 1:Positive).\n",
			title, cfg->ts_clk, cfg->ts_clk_pol);
	pr_err("[%s] reset pin: %d, dir: %d, value: %d.\n",
			title,
			cfg->reset.pin, cfg->reset.dir, cfg->reset.value);
	pr_err("[%s] ant power pin: %d, dir: %d, value: %d.\n",
			title,
			cfg->ant_power.pin, cfg->ant_power.dir, cfg->ant_power.value);
	pr_err("[%s] lnb en pin: %d, dir: %d, value: %d.\n",
			title,
			cfg->lnb_en.pin, cfg->lnb_en.dir, cfg->lnb_en.value);
	pr_err("[%s] lnb sel pin: %d, dir: %d, value: %d.\n",
			title,
			cfg->lnb_sel.pin, cfg->lnb_sel.dir, cfg->lnb_sel.value);
	pr_err("[%s] other pin: %d, dir: %d, value: %d.\n",
			title,
			cfg->other.pin, cfg->other.dir, cfg->other.value);
	pr_err("[%s] i2c_addr: 0x%x, i2c_id: 0x%x, i2c_adap: 0x%p.\n",
			title, cfg->i2c_addr, cfg->i2c_id, cfg->i2c_adap);
	pr_err("[%s] tuner0 name: %s, code: 0x%x, id: %d, i2c_addr: 0x%x, i2c_id: %d, i2c_adap: 0x%p, xtal: %d; xtal_cap: %d, xtal_mode: %d, lt_out: %d.\n",
			title,
			cfg->tuner0.name ? cfg->tuner0.name : "",
			cfg->tuner0.code,
			cfg->tuner0.id,
			cfg->tuner0.i2c_addr,
			cfg->tuner0.i2c_id,
			cfg->tuner0.i2c_adap,
			cfg->tuner0.xtal,
			cfg->tuner0.xtal_cap,
			cfg->tuner0.xtal_mode,
			cfg->tuner0.lt_out);
	pr_err("[%s] tuner0 reset pin: %d, dir: %d, value: %d.\n",
			title,
			cfg->tuner0.reset.pin,
			cfg->tuner0.reset.dir,
			cfg->tuner0.reset.value);
	pr_err("[%s] tuner0 power pin: %d, dir: %d, value: %d.\n",
			title,
			cfg->tuner0.power.pin,
			cfg->tuner0.power.dir,
			cfg->tuner0.power.value);
	pr_err("[%s] tuner0 dual_power: %d (0:3.3v, 1:1.8v and 3.3v).\n",
			title,
			cfg->tuner0.dual_power);
	pr_err("[%s] tuner0 if_agc: %d (0:Self, 1:External), if_hz: %d Hz, if_invert: %d (0:Normal, 1:Inverted), if_amp: %d.\n",
			title,
			cfg->tuner0.if_agc,
			cfg->tuner0.if_hz,
			cfg->tuner0.if_invert,
			cfg->tuner0.if_amp);
	pr_err("[%s] tuner0 detect: %d, reserved0: %d, reserved1: %d.\n",
			title,
			cfg->tuner0.detect,
			cfg->tuner0.reserved0,
			cfg->tuner0.reserved1);
	pr_err("[%s] tuner1 name: %s, code: 0x%x, id: %d, i2c_addr: 0x%x, i2c_id: %d, i2c_adap: 0x%p, xtal: %d; xtal_cap: %d, xtal_mode: %d, lt_out: %d.\n",
			title,
			cfg->tuner1.name ? cfg->tuner1.name : "",
			cfg->tuner1.code,
			cfg->tuner1.id,
			cfg->tuner1.i2c_addr,
			cfg->tuner1.i2c_id,
			cfg->tuner1.i2c_adap,
			cfg->tuner1.xtal,
			cfg->tuner1.xtal_cap,
			cfg->tuner1.xtal_mode,
			cfg->tuner1.lt_out);
	pr_err("[%s] tuner1 dual_power: %d (0:3.3v, 1:1.8v and 3.3v).\n",
			title,
			cfg->tuner1.dual_power);
	pr_err("[%s] tuner1 if_agc: %d (0:Self, 1:External), if_hz: %d Hz, if_invert: %d (0:Normal, 1:Inverted), if_amp: %d.\n",
			title,
			cfg->tuner1.if_agc,
			cfg->tuner1.if_hz,
			cfg->tuner1.if_invert,
			cfg->tuner1.if_amp);
	pr_err("[%s] tuner1 reset pin: %d, dir: %d, value: %d.\n",
			title,
			cfg->tuner1.reset.pin,
			cfg->tuner1.reset.dir,
			cfg->tuner1.reset.value);
	pr_err("[%s] tuner1 power pin: %d, dir: %d, value: %d.\n",
			title,
			cfg->tuner1.power.pin,
			cfg->tuner1.power.dir,
			cfg->tuner1.power.value);
	pr_err("[%s] tuner1 detect: %d, reserved0: %d, reserved1: %d.\n",
			title,
			cfg->tuner1.detect,
			cfg->tuner1.reserved0,
			cfg->tuner1.reserved1);
	pr_err("[%s] detect: %d, reserved0: %d, reserved1: %d.\n",
			title, cfg->detect, cfg->reserved0, cfg->reserved1);
}
EXPORT_SYMBOL(aml_show_demod_config);

enum dtv_demod_type aml_get_dtvdemod_type(const char *name)
{
	int i = 0;
	int count = ARRAY_SIZE(demod_modules);

	for (i = 0; i < count; ++i) {
		if (!strncasecmp(name, demod_modules[i].name,
				strlen(demod_modules[i].name)))
			return demod_modules[i].id;
	}

	return AM_DTV_DEMOD_NONE;
}
EXPORT_SYMBOL(aml_get_dtvdemod_type);

const struct demod_module *aml_get_demod_module(enum dtv_demod_type type)
{
	int i = 0;
	int count = ARRAY_SIZE(demod_modules);

	for (i = 0; i < count; ++i) {
		if (demod_modules[i].id == type)
			return &demod_modules[i];
	}

	pr_err("Demod: get demod [%d] module fail.\n", type);

	return NULL;
}
EXPORT_SYMBOL(aml_get_demod_module);
