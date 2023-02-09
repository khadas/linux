// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/i2c.h>
#include <linux/amlogic/aml_tuner.h>

AML_TUNER_ATTACH_FUNCTION(si2176);
AML_TUNER_ATTACH_FUNCTION(si2196);
AML_TUNER_ATTACH_FUNCTION(fq1216);
AML_TUNER_ATTACH_FUNCTION(htm);
AML_TUNER_ATTACH_FUNCTION(ctc703);
AML_TUNER_ATTACH_FUNCTION(si2177);
AML_TUNER_ATTACH_FUNCTION(r840);
AML_TUNER_ATTACH_FUNCTION(si2157);
AML_TUNER_ATTACH_FUNCTION(si2151);
AML_TUNER_ATTACH_FUNCTION(mxl661);
AML_TUNER_ATTACH_FUNCTION(mxl608);
AML_TUNER_ATTACH_FUNCTION(si2159);
AML_TUNER_ATTACH_FUNCTION(r842);
AML_TUNER_ATTACH_FUNCTION(atbm2040);
AML_TUNER_ATTACH_FUNCTION(atbm253);
AML_TUNER_ATTACH_FUNCTION(si2124);
AML_TUNER_ATTACH_FUNCTION(av2011);
AML_TUNER_ATTACH_FUNCTION(av2012);
AML_TUNER_ATTACH_FUNCTION(av2018);
AML_TUNER_ATTACH_FUNCTION(mxl603);
AML_TUNER_ATTACH_FUNCTION(r836);
AML_TUNER_ATTACH_FUNCTION(r848);
AML_TUNER_ATTACH_FUNCTION(rt710);
AML_TUNER_ATTACH_FUNCTION(r850);
AML_TUNER_ATTACH_FUNCTION(rda5160);
AML_TUNER_ATTACH_FUNCTION(rda5815m);

static struct dvb_frontend *aml_tuner_attach(const struct tuner_module *module,
		struct dvb_frontend *fe, const struct tuner_config *cfg);
static int aml_tuner_detach(const struct tuner_module *module);
static int aml_tuner_match(const struct tuner_module *module, int std);
static int aml_tuner_detect(const struct tuner_config *cfg);

static const struct tuner_module tuner_modules[] = {
	{
		.name = "si2176",
		.id = AM_TUNER_SI2176,
		.delsys = { SYS_DVBC_ANNEX_A, SYS_DVBC_ANNEX_B, SYS_DVBT,
				SYS_ISDBT, SYS_ISDBC, SYS_ATSC, SYS_DTMB,
				SYS_DVBT2, SYS_DVBC_ANNEX_C, SYS_ANALOG
		},
		.type = { FE_OFDM, FE_ATSC, FE_QAM,
				FE_DTMB, FE_ISDBT, FE_ANALOG,
				AML_FE_UNDEFINED
		},
		.attach_symbol = NULL,
		.attach = aml_tuner_attach,
		.detach = aml_tuner_detach,
		.match = aml_tuner_match,
		.detect = aml_tuner_detect
	},
	{
		.name = "si2196",
		.id = AM_TUNER_SI2196,
		.delsys = { SYS_DVBC_ANNEX_A, SYS_DVBC_ANNEX_B, SYS_DVBT,
				SYS_ISDBT, SYS_ATSC, SYS_DTMB, SYS_DVBT2,
				SYS_DVBC_ANNEX_C, SYS_ANALOG
		},
		.type = { FE_OFDM, FE_ATSC, FE_QAM,
				FE_DTMB, FE_ISDBT, FE_ANALOG,
				AML_FE_UNDEFINED
		},
		.attach_symbol = NULL,
		.attach = aml_tuner_attach,
		.detach = aml_tuner_detach,
		.match = aml_tuner_match,
		.detect = aml_tuner_detect
	},
	{
		.name = "fq1216",
		.id = AM_TUNER_FQ1216,
		.delsys = { SYS_ANALOG },
		.type = { FE_ANALOG, AML_FE_UNDEFINED },
		.attach = aml_tuner_attach,
		.detach = aml_tuner_detach,
		.match = aml_tuner_match,
		.detect = aml_tuner_detect
	},
	{
		.name = "htm",
		.id = AM_TUNER_HTM,
		.delsys = { SYS_ANALOG },
		.type = { FE_ANALOG, AML_FE_UNDEFINED },
		.attach_symbol = NULL,
		.attach_symbol = NULL,
		.detach = aml_tuner_detach,
		.attach = aml_tuner_attach,
		.match = aml_tuner_match,
		.detect = aml_tuner_detect
	},
	{
		.name = "ctc703",
		.id = AM_TUNER_CTC703,
		.delsys = { SYS_ANALOG },
		.type = { FE_ANALOG, AML_FE_UNDEFINED },
		.attach_symbol = NULL,
		.attach = aml_tuner_attach,
		.detach = aml_tuner_detach,
		.match = aml_tuner_match,
		.detect = aml_tuner_detect
	},
	{
		.name = "si2177",
		.id = AM_TUNER_SI2177,
		.delsys = { SYS_DVBC_ANNEX_A, SYS_DVBC_ANNEX_B, SYS_DVBT,
				SYS_ISDBT, SYS_ISDBC, SYS_ATSC, SYS_DTMB,
				SYS_DVBT2, SYS_DVBC_ANNEX_C, SYS_ANALOG
		},
		.type = { FE_OFDM, FE_ATSC, FE_QAM,
				FE_DTMB, FE_ISDBT, FE_ANALOG,
				AML_FE_UNDEFINED
		},
		.attach_symbol = NULL,
		.attach = aml_tuner_attach,
		.detach = aml_tuner_detach,
		.match = aml_tuner_match,
		.detect = aml_tuner_detect
	},
	{
		.name = "r840",
		.id = AM_TUNER_R840,
		.delsys = { SYS_DVBC_ANNEX_A, SYS_DVBC_ANNEX_B, SYS_DVBT,
				SYS_ISDBT, SYS_ISDBC, SYS_ATSC, SYS_DTMB,
				SYS_DVBT2, SYS_DVBC_ANNEX_C, SYS_ANALOG
		},
		.type = { FE_OFDM, FE_ATSC, FE_QAM,
				FE_DTMB, FE_ISDBT, FE_ANALOG,
				AML_FE_UNDEFINED
		},
		.attach_symbol = NULL,
		.attach = aml_tuner_attach,
		.detach = aml_tuner_detach,
		.match = aml_tuner_match,
		.detect = aml_tuner_detect
	},
	{
		.name = "si2157",
		.id = AM_TUNER_SI2157,
		.delsys = { SYS_DVBC_ANNEX_A, SYS_DVBC_ANNEX_B, SYS_DVBT,
				SYS_ISDBT, SYS_ISDBC, SYS_ATSC, SYS_DTMB,
				SYS_DVBT2, SYS_DVBC_ANNEX_C, SYS_ANALOG
		},
		.type = { FE_OFDM, FE_ATSC, FE_QAM,
				FE_DTMB, FE_ISDBT, FE_ANALOG,
				AML_FE_UNDEFINED
		},
		.attach_symbol = NULL,
		.attach = aml_tuner_attach,
		.detach = aml_tuner_detach,
		.match = aml_tuner_match,
		.detect = aml_tuner_detect
	},
	{
		.name = "si2151",
		.id = AM_TUNER_SI2151,
		.delsys = { SYS_DVBC_ANNEX_A, SYS_DVBC_ANNEX_B, SYS_DVBT,
				SYS_ISDBT, SYS_ATSC, SYS_DTMB, SYS_DVBT2,
				SYS_DVBC_ANNEX_C, SYS_ANALOG
		},
		.type = { FE_OFDM, FE_ATSC, FE_QAM,
				FE_DTMB, FE_ISDBT, FE_ANALOG,
				AML_FE_UNDEFINED
		},
		.attach_symbol = NULL,
		.attach = aml_tuner_attach,
		.detach = aml_tuner_detach,
		.match = aml_tuner_match,
		.detect = aml_tuner_detect
	},
	{
		.name = "mxl661",
		.id = AM_TUNER_MXL661,
		.delsys = { SYS_DVBC_ANNEX_A, SYS_DVBC_ANNEX_B, SYS_DVBT,
				SYS_ISDBT, SYS_ATSC, SYS_DTMB, SYS_DVBT2,
				SYS_DVBC_ANNEX_C, SYS_ANALOG
		},
		.type = { FE_OFDM, FE_ATSC, FE_QAM,
				FE_DTMB, FE_ISDBT, FE_ANALOG,
				AML_FE_UNDEFINED
		},
		.attach_symbol = NULL,
		.attach = aml_tuner_attach,
		.detach = aml_tuner_detach,
		.match = aml_tuner_match,
		.detect = aml_tuner_detect
	},
	{
		.name = "mxl608",
		.id = AM_TUNER_MXL608,
		.delsys = { SYS_DVBC_ANNEX_A, SYS_DVBC_ANNEX_B, SYS_DVBT,
				SYS_ISDBT, SYS_ATSC, SYS_DTMB, SYS_DVBT2,
				SYS_DVBC_ANNEX_C
		},
		.type = { FE_OFDM, FE_ATSC, FE_QAM,
				FE_DTMB, FE_ISDBT,
				AML_FE_UNDEFINED
		},
		.attach_symbol = NULL,
		.attach = aml_tuner_attach,
		.detach = aml_tuner_detach,
		.match = aml_tuner_match,
		.detect = aml_tuner_detect
	},
	{
		.name = "si2159",
		.id = AM_TUNER_SI2159,
		.delsys = { SYS_DVBC_ANNEX_A, SYS_DVBC_ANNEX_B, SYS_DVBT,
				SYS_ISDBT, SYS_ISDBC, SYS_ATSC, SYS_DTMB,
				SYS_DVBT2, SYS_DVBC_ANNEX_C, SYS_ANALOG
		},
		.type = { FE_OFDM, FE_ATSC, FE_QAM,
				FE_DTMB, FE_ISDBT, FE_ANALOG,
				AML_FE_UNDEFINED
		},
		.attach_symbol = NULL,
		.attach = aml_tuner_attach,
		.detach = aml_tuner_detach,
		.match = aml_tuner_match,
		.detect = aml_tuner_detect
	},
	{
		.name = "r842",
		.id = AM_TUNER_R842,
		.delsys = { SYS_DVBC_ANNEX_A, SYS_DVBC_ANNEX_B, SYS_DVBT,
				SYS_ISDBT, SYS_ISDBC, SYS_ATSC, SYS_DTMB,
				SYS_DVBT2, SYS_DVBC_ANNEX_C, SYS_ANALOG
		},
		.type = { FE_OFDM, FE_ATSC, FE_QAM,
				FE_DTMB, FE_ISDBT, FE_ANALOG,
				AML_FE_UNDEFINED
		},
		.attach_symbol = NULL,
		.attach = aml_tuner_attach,
		.detach = aml_tuner_detach,
		.match = aml_tuner_match,
		.detect = aml_tuner_detect
	},
	{
		.name = "atbm2040",
		.id = AM_TUNER_ATBM2040,
		.delsys = { SYS_DVBC_ANNEX_A, SYS_DVBC_ANNEX_B, SYS_DVBT,
				SYS_ISDBT, SYS_ISDBC, SYS_ATSC, SYS_DTMB,
				SYS_DVBT2, SYS_DVBC_ANNEX_C, SYS_ANALOG
		},
		.type = { FE_OFDM, FE_ATSC, FE_QAM,
				FE_DTMB, FE_ISDBT, FE_ANALOG,
				AML_FE_UNDEFINED
		},
		.attach_symbol = NULL,
		.attach = aml_tuner_attach,
		.detach = aml_tuner_detach,
		.match = aml_tuner_match,
		.detect = aml_tuner_detect
	},
	{
		.name = "atbm253",
		.id = AM_TUNER_ATBM253,
		.delsys = { SYS_DVBC_ANNEX_A, SYS_DVBC_ANNEX_B, SYS_DVBT,
				SYS_ISDBT, SYS_ISDBC, SYS_ATSC, SYS_DTMB,
				SYS_DVBT2, SYS_DVBC_ANNEX_C, SYS_ANALOG
		},
		.type = { FE_OFDM, FE_ATSC, FE_QAM,
				FE_DTMB, FE_ISDBT, FE_ANALOG,
				AML_FE_UNDEFINED
		},
		.attach_symbol = NULL,
		.attach = aml_tuner_attach,
		.detach = aml_tuner_detach,
		.match = aml_tuner_match,
		.detect = aml_tuner_detect
	},
	{
		.name = "si2124",
		.id = AM_TUNER_SI2124,
		.delsys = { SYS_DVBT, SYS_ISDBT, SYS_ATSC, SYS_DVBT2 },
		.type = { FE_OFDM, FE_ATSC,
				FE_ISDBT,
				AML_FE_UNDEFINED
		},
		.attach_symbol = NULL,
		.attach = aml_tuner_attach,
		.detach = aml_tuner_detach,
		.match = aml_tuner_match,
		.detect = aml_tuner_detect
	},
	{
		.name = "av2011",
		.id = AM_TUNER_AV2011,
		.delsys = { SYS_DVBS, SYS_DVBS2 },
		.type = { FE_QPSK, AML_FE_UNDEFINED },
		.attach_symbol = NULL,
		.attach = aml_tuner_attach,
		.detach = aml_tuner_detach,
		.match = aml_tuner_match,
		.detect = aml_tuner_detect
	},
	{
		.name = "av2012",
		.id = AM_TUNER_AV2012,
		.delsys = { SYS_DVBS, SYS_DVBS2 },
		.type = { FE_QPSK, AML_FE_UNDEFINED },
		.attach_symbol = NULL,
		.attach = aml_tuner_attach,
		.detach = aml_tuner_detach,
		.match = aml_tuner_match,
		.detect = aml_tuner_detect
	},
	{
		.name = "av2018",
		.id = AM_TUNER_AV2018,
		.delsys = { SYS_DVBS, SYS_DVBS2 },
		.type = { FE_QPSK, AML_FE_UNDEFINED },
		.attach_symbol = NULL,
		.attach = aml_tuner_attach,
		.detach = aml_tuner_detach,
		.match = aml_tuner_match,
		.detect = aml_tuner_detect
	},
	{
		.name = "r836",
		.id = AM_TUNER_R836,
		.delsys = { SYS_DVBC_ANNEX_A, SYS_DVBC_ANNEX_B, SYS_DVBT,
				SYS_ISDBT, SYS_ISDBC, SYS_ATSC, SYS_DTMB,
				SYS_DVBT2, SYS_DVBC_ANNEX_C
		},
		.type = { FE_OFDM, FE_ATSC, FE_QAM,
				FE_DTMB, FE_ISDBT,
				AML_FE_UNDEFINED
		},
		.attach_symbol = NULL,
		.attach = aml_tuner_attach,
		.detach = aml_tuner_detach,
		.match = aml_tuner_match,
		.detect = aml_tuner_detect
	},
	{
		.name = "r848",
		.id = AM_TUNER_R848,
		.delsys = { SYS_DVBC_ANNEX_A, SYS_DVBC_ANNEX_B, SYS_DVBT,
				SYS_ISDBT, SYS_ISDBC, SYS_ATSC, SYS_DTMB,
				SYS_DVBT2, SYS_DVBC_ANNEX_C, SYS_ANALOG
		},
		.type = { FE_OFDM, FE_ATSC, FE_QAM,
				FE_DTMB, FE_ISDBT, FE_ANALOG,
				AML_FE_UNDEFINED
		},
		.attach_symbol = NULL,
		.attach = aml_tuner_attach,
		.detach = aml_tuner_detach,
		.match = aml_tuner_match,
		.detect = aml_tuner_detect
	},
	{
		.name = "mxl603",
		.id = AM_TUNER_MXL603,
		.delsys = { SYS_DVBC_ANNEX_A, SYS_DVBC_ANNEX_B, SYS_DVBT,
				SYS_ISDBT, SYS_ATSC, SYS_ATSCMH, SYS_DTMB,
				SYS_DVBT2, SYS_DVBC_ANNEX_C, SYS_ANALOG
		},
		.type = { FE_OFDM, FE_ATSC, FE_QAM,
				FE_DTMB, FE_ISDBT, FE_ANALOG,
				AML_FE_UNDEFINED
		},
		.attach_symbol = NULL,
		.attach = aml_tuner_attach,
		.detach = aml_tuner_detach,
		.match = aml_tuner_match,
		.detect = aml_tuner_detect
	},
	{
		.name = "rt710",
		.id = AM_TUNER_RT710,
		.delsys = { SYS_DVBS, SYS_DVBS2, SYS_ISDBS },
		.type = { FE_QPSK, AML_FE_UNDEFINED },
		.attach_symbol = NULL,
		.attach = aml_tuner_attach,
		.detach = aml_tuner_detach,
		.match = aml_tuner_match,
		.detect = aml_tuner_detect
	},
	{
		.name = "r850",
		.id = AM_TUNER_R850,
		.delsys = { SYS_DVBC_ANNEX_A, SYS_DVBC_ANNEX_B, SYS_DVBT,
				SYS_ISDBT, SYS_ATSC, SYS_DTMB, SYS_DVBT2,
				SYS_DVBC_ANNEX_C
		},
		.type = { FE_OFDM, FE_ATSC, FE_QAM,
				FE_DTMB, FE_ISDBT,
				AML_FE_UNDEFINED
		},
		.attach_symbol = NULL,
		.attach = aml_tuner_attach,
		.detach = aml_tuner_detach,
		.match = aml_tuner_match,
		.detect = aml_tuner_detect
	},
	{
		.name = "cxd2871",
		.id = AM_TUNER_CXD2871,
		.delsys = { SYS_DVBC_ANNEX_A, SYS_DVBC_ANNEX_B, SYS_DVBT,
				SYS_ISDBT, SYS_ISDBC, SYS_ATSC, SYS_ATSCMH, SYS_DTMB,
				SYS_DVBT2, SYS_DVBC_ANNEX_C
		},
		.type = { FE_OFDM, FE_ATSC, FE_QAM,
				FE_DTMB, FE_ISDBT,
				AML_FE_UNDEFINED
		},
		.attach_symbol = NULL,
		.attach = aml_tuner_attach,
		.detach = aml_tuner_detach,
		.match = aml_tuner_match,
		.detect = aml_tuner_detect
	},
	{
		.name = "cxd6866",
		.id = AM_TUNER_CXD6866,
		.delsys = { SYS_DVBC_ANNEX_A, SYS_DVBC_ANNEX_B, SYS_DVBT,
				SYS_ISDBT, SYS_ISDBC, SYS_ATSC, SYS_ATSCMH, SYS_DTMB,
				SYS_DVBT2, SYS_DVBC_ANNEX_C, SYS_DVBS, SYS_DVBS2, SYS_ISDBS
		},
		.type = { FE_OFDM, FE_ATSC, FE_QAM,
				FE_DTMB, FE_ISDBT, FE_QPSK,
				AML_FE_UNDEFINED
		},
		.attach_symbol = NULL,
		.attach = aml_tuner_attach,
		.detach = aml_tuner_detach,
		.match = aml_tuner_match,
		.detect = aml_tuner_detect
	},
	{
		.name = "rda5160",
		.id = AM_TUNER_RDA5160,
		.delsys = { SYS_DVBC_ANNEX_A, SYS_DVBC_ANNEX_B, SYS_DVBT,
				SYS_ISDBT, SYS_ATSC, SYS_DTMB, SYS_DVBT2,
				SYS_DVBC_ANNEX_C, SYS_ANALOG
		},
		.type = { FE_OFDM, FE_ATSC, FE_QAM,
				FE_DTMB, FE_ISDBT, FE_ANALOG,
				AML_FE_UNDEFINED
		},
		.attach_symbol = NULL,
		.attach = aml_tuner_attach,
		.detach = aml_tuner_detach,
		.match = aml_tuner_match,
		.detect = aml_tuner_detect
	},
	{
		.name = "rda5815m",
		.id = AM_TUNER_RDA5815M,
		.delsys = { SYS_DVBS, SYS_DVBS2 },
		.type = { FE_QPSK, AML_FE_UNDEFINED },
		.attach_symbol = NULL,
		.attach = aml_tuner_attach,
		.detach = aml_tuner_detach,
		.match = aml_tuner_match,
		.detect = aml_tuner_detect
	},
	{
		.name = "r856",
		.id = AM_TUNER_R856,
		.delsys = { SYS_ATSC },
		.type = {  FE_ATSC, AML_FE_UNDEFINED },
		.attach_symbol = NULL,
		.attach = aml_tuner_attach,
		.detach = aml_tuner_detach,
		.match = aml_tuner_match,
		.detect = aml_tuner_detect
	},
};

static tn_attach_cb pt[AM_TUNER_MAX];
void aml_set_tuner_attach_cb(const enum tuner_type type, tn_attach_cb funcb)
{
	if (type > AM_TUNER_NONE && type < AM_TUNER_MAX && !pt[type])
		pt[type] = funcb;
}

static struct dvb_frontend *aml_attach_detach_tuner(
		const enum tuner_type type,
		struct dvb_frontend *fe,
		const struct tuner_config *cfg,
		int attach)
{
	struct dvb_frontend *p = NULL;

	if (pt[type]) {
		pr_err("%s: cb id %d\n", __func__, type);
		return pt[type](fe, cfg);
	}

	switch (type) {
	case AM_TUNER_SI2176:
		attach ? (p = aml_dvb_attach(si2176_attach, fe, cfg)) :
				aml_dvb_detach(si2176_attach);
		break;
	case AM_TUNER_SI2196:
		attach ? (p = aml_dvb_attach(si2196_attach, fe, cfg)) :
				aml_dvb_detach(si2196_attach);
		break;
	case AM_TUNER_FQ1216:
		attach ? (p = aml_dvb_attach(fq1216_attach, fe, cfg)) :
				aml_dvb_detach(fq1216_attach);
		break;
	case AM_TUNER_HTM:
		attach ? (p = aml_dvb_attach(htm_attach, fe, cfg)) :
				aml_dvb_detach(htm_attach);
		break;
	case AM_TUNER_CTC703:
		attach ? (p = aml_dvb_attach(ctc703_attach, fe, cfg)) :
				aml_dvb_detach(ctc703_attach);
		break;
	case AM_TUNER_SI2177:
		attach ? (p = aml_dvb_attach(si2177_attach, fe, cfg)) :
				aml_dvb_detach(si2177_attach);
		break;
	case AM_TUNER_R840:
		attach ? (p = aml_dvb_attach(r840_attach, fe, cfg)) :
				aml_dvb_detach(r840_attach);
		break;
	case AM_TUNER_SI2157:
		attach ? (p = aml_dvb_attach(si2157_attach, fe, cfg)) :
				aml_dvb_detach(si2157_attach);
		break;
	case AM_TUNER_SI2151:
		attach ? (p = aml_dvb_attach(si2151_attach, fe, cfg)) :
				aml_dvb_detach(si2151_attach);
		break;
	case AM_TUNER_MXL661:
		attach ? (p = aml_dvb_attach(mxl661_attach, fe, cfg)) :
				aml_dvb_detach(mxl661_attach);
		break;
	case AM_TUNER_MXL608:
		attach ? (p = aml_dvb_attach(mxl608_attach, fe, cfg)) :
				aml_dvb_detach(mxl608_attach);
		break;
	case AM_TUNER_SI2159:
		attach ? (p = aml_dvb_attach(si2159_attach, fe, cfg)) :
				aml_dvb_detach(si2159_attach);
		break;
	case AM_TUNER_R842:
		attach ? (p = aml_dvb_attach(r842_attach, fe, cfg)) :
				aml_dvb_detach(r842_attach);
		break;
	case AM_TUNER_ATBM2040:
		attach ? (p = aml_dvb_attach(atbm2040_attach, fe, cfg)) :
				aml_dvb_detach(atbm2040_attach);
		break;
	case AM_TUNER_ATBM253:
		attach ? (p = aml_dvb_attach(atbm253_attach, fe, cfg)) :
				aml_dvb_detach(atbm253_attach);
		break;
	case AM_TUNER_SI2124:
		attach ? (p = aml_dvb_attach(si2124_attach, fe, cfg)) :
				aml_dvb_detach(si2124_attach);
		break;
	case AM_TUNER_AV2011:
		attach ? (p = aml_dvb_attach(av2011_attach, fe, cfg)) :
				aml_dvb_detach(av2011_attach);
		break;
	case AM_TUNER_AV2012:
		attach ? (p = aml_dvb_attach(av2012_attach, fe, cfg)) :
				aml_dvb_detach(av2012_attach);
		break;
	case AM_TUNER_AV2018:
		attach ? (p = aml_dvb_attach(av2018_attach, fe, cfg)) :
				aml_dvb_detach(av2018_attach);
		break;
	case AM_TUNER_MXL603:
		attach ? (p = aml_dvb_attach(mxl603_attach, fe, cfg)) :
				aml_dvb_detach(mxl603_attach);
		break;
	case AM_TUNER_R836:
		attach ? (p = aml_dvb_attach(r836_attach, fe, cfg)) :
				aml_dvb_detach(r836_attach);
		break;
	case AM_TUNER_R848:
		attach ? (p = aml_dvb_attach(r848_attach, fe, cfg)) :
				aml_dvb_detach(r848_attach);
		break;
	case AM_TUNER_RT710:
		attach ? (p = aml_dvb_attach(rt710_attach, fe, cfg)) :
				aml_dvb_detach(rt710_attach);
		break;
	case AM_TUNER_R850:
		attach ? (p = aml_dvb_attach(r850_attach, fe, cfg)) :
				aml_dvb_detach(r850_attach);
		break;
	case AM_TUNER_RDA5160:
		attach ? (p = aml_dvb_attach(rda5160_attach, fe, cfg)) :
				aml_dvb_detach(rda5160_attach);
		break;
	case AM_TUNER_RDA5815M:
		attach ? (p = aml_dvb_attach(rda5815m_attach, fe, cfg)) :
				aml_dvb_detach(rda5815m_attach);
		break;
	default:
		p = NULL;
		break;
	}

	return p;
}

static struct dvb_frontend *aml_tuner_attach(const struct tuner_module *module,
		struct dvb_frontend *fe, const struct tuner_config *cfg)
{
	if (!IS_ERR_OR_NULL(module))
		return aml_attach_detach_tuner(module->id, fe, cfg, true);

	return NULL;
}

static int aml_tuner_detach(const struct tuner_module *module)
{
	if (!IS_ERR_OR_NULL(module))
		aml_attach_detach_tuner(module->id, NULL, NULL, false);

	return 0;
}

static int aml_tuner_match(const struct tuner_module *module, int std)
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

	/* Now, does not match delivery system.
	 * After the FE type is deactivated, use DTV_ENUM_DELSYS instead.
	 */
	return -EINVAL;

#if 0
	/* std = delivery system. */
	n = 0;
	while (n < AML_MAX_DELSYS && module->delsys[n] != SYS_UNDEFINED) {
		if (module->delsys[n] == std)
			return 0;

		n++;
	}

	return -EINVAL;
#endif
}

static int aml_tuner_rw(struct i2c_adapter *i2c_adap,
		struct i2c_msg *msg, int msg_num)
{
	int i2c_try_cnt = 1;
	int ret = 0;
	int i = 0;

repeat:
	ret = i2c_transfer(i2c_adap, msg, msg_num);
	if (ret < 0) {
		pr_err("Tuner: error write or read ret = %d.\n", ret);
		if (i++ < i2c_try_cnt)
			goto repeat;
		else
			return ret;
	}

	return 0;
}

static int aml_tuner_detect(const struct tuner_config *cfg)
{
	int i = 0;
	int ret = 0;
	unsigned char data_w[4] = { 0, 0, 0, 0 };
	unsigned char data_r[4] = { 0, 0, 0, 0 };
	struct i2c_msg msg_w, msg_r;

	if (IS_ERR_OR_NULL(cfg) || IS_ERR_OR_NULL(cfg->i2c_adap))
		return -EFAULT;

	msg_w.addr = (cfg->i2c_addr & 0x80) ?
			(cfg->i2c_addr >> 1) : cfg->i2c_addr;
	msg_w.flags = 0;
	msg_w.len = 0;
	msg_w.buf = data_w;

	msg_r.addr = (cfg->i2c_addr & 0x80) ?
			(cfg->i2c_addr >> 1) : cfg->i2c_addr;
	msg_r.flags = I2C_M_RD;
	msg_r.len = 0;
	msg_r.buf = data_r;

	pr_err("Tuner: id %d %s.\n", cfg->id, cfg->name ? cfg->name : "");
	switch (cfg->id) {
	case AM_TUNER_ATBM2040:
	case AM_TUNER_ATBM253:
		msg_w.len = 2;
		data_w[0] = 0x00;
		data_w[1] = 0x00;
		ret = aml_tuner_rw(cfg->i2c_adap, &msg_w, 1);
		if (ret)
			return ret;

		msg_r.len = 1;
		data_r[0] = 0x00;
		ret = aml_tuner_rw(cfg->i2c_adap, &msg_r, 1);
		if (ret)
			return ret;

#define ATBM_LEOG_CHIP_ID (0xAA)
#define ATBM_LEOF_CHIP_ID (0xE1)
#define ATBM_LEOB_CHIP_ID (0xF0)
#define ATBM_LEO_LITE_G_CHIP_ID (0x59) /* 253 */

		pr_err("Tuner: atbm2040/253 data_r: 0x%x.\n", data_r[0]);
		if ((data_r[0] == ATBM_LEOG_CHIP_ID) ||
			(data_r[0] == ATBM_LEOF_CHIP_ID) ||
			(data_r[0] == ATBM_LEOB_CHIP_ID) ||
			(data_r[0] == ATBM_LEO_LITE_G_CHIP_ID))
			return 0;
		else
			return -ENXIO;

		break;

	case AM_TUNER_SI2151:
	case AM_TUNER_SI2159:
		i = 0;
		while (i++ < 10) {
			msg_r.len = 1;
			data_r[0] = 0x00;
			ret = aml_tuner_rw(cfg->i2c_adap, &msg_r, 1);
			if (ret)
				return ret;

			if (data_r[0] & 0x80)
				break;

			mdelay(2);
		}

		pr_err("Tuner: si2151/si2159 data_r: 0x%x.\n",
				data_r[0]);
		if (!(data_r[0] & 0x80))
			return -ENXIO;

		msg_w.len = 1;
		data_w[0] = 0x11;
		ret = aml_tuner_rw(cfg->i2c_adap, &msg_w, 1);
		if (ret)
			return ret;

		i = 0;
		while (i++ < 10) {
			msg_r.len = 2;
			data_r[0] = 0x00;
			data_r[1] = 0x00;
			ret = aml_tuner_rw(cfg->i2c_adap, &msg_r, 1);
			if (ret)
				return ret;

			pr_err("Tuner: si2151/si2159 data_r: 0x%x 0x%x.\n",
					data_r[0], data_r[1]);
			if (data_r[0] & 0x80)
				break;

			msleep(50);
		}

		if (data_r[0] == 0xFE && data_r[1] == 0xFE) {
			msg_w.len = 2;
			data_w[0] = 0xFF;
			data_w[1] = 0x00;
			ret = aml_tuner_rw(cfg->i2c_adap, &msg_w, 1);
			if (ret)
				return ret;

			msg_w.len = 2;
			data_w[0] = 0xC0;
			data_w[1] = 0x00;
			ret = aml_tuner_rw(cfg->i2c_adap, &msg_w, 1);
			if (ret)
				return ret;

			msg_r.len = 1;
			data_r[0] = 0x00;
			ret = aml_tuner_rw(cfg->i2c_adap, &msg_r, 1);
			if (ret)
				return ret;

			msg_w.len = 2;
			data_w[0] = 0xFE;
			data_w[1] = 0x00;
			ret = aml_tuner_rw(cfg->i2c_adap, &msg_w, 1);
			if (ret)
				return ret;

			pr_err("Tuner: si2151/si2159 data_r: 0x%x.\n",
					data_r[0]);
			if (data_r[0] == 0x1D)
				return 0;
		} else if ((data_r[0] & 0x80) &&
				(data_r[1] == 0x33 || data_r[1] == 0x3b)) {
			return 0;
		} else {
			return -ENXIO;
		}

		break;

	case AM_TUNER_R840:
	case AM_TUNER_R842:
		msg_r.len = 1;
		data_r[0] = 0x00;

		ret = aml_tuner_rw(cfg->i2c_adap, &msg_r, 1);
		if (ret)
			return ret;

		pr_err("Tuner: r840/r842 data_r: 0x%x.\n", data_r[0]);
		if (data_r[0] == 0x96 || data_r[0] == 0x69 ||
			data_r[0] == 0x97 || data_r[0] == 0xE9)
			return 0;
		else
			return -ENXIO;

		break;

	case AM_TUNER_MXL661:
		msg_w.len = 2;
		data_w[0] = 0xFB;
		data_w[1] = 0x18;
		msg_r.len = 1;
		data_r[0] = 0x00;

		ret = aml_tuner_rw(cfg->i2c_adap, &msg_w, 1);
		if (ret)
			return ret;

		ret = aml_tuner_rw(cfg->i2c_adap, &msg_r, 1);
		if (ret)
			return ret;

		pr_err("Tuner: mxl661 data_r: 0x%x.\n", data_r[0]);
		if (data_r[0] == 0x3)
			return 0;
		else
			return -ENXIO;

		break;

	default:
		return -EINVAL;
	}

	return 0;
}

int aml_get_dts_tuner_config(struct device_node *node,
		struct tuner_config *cfg, int index)
{
	int ret = 0;
	u32 value = 0;
	struct device_node *node_i2c = NULL;
	char buf[32] = { 0 };
	const char *str = NULL;

	if (IS_ERR_OR_NULL(node) || IS_ERR_OR_NULL(cfg)) {
		pr_err("Tuner: NULL or error pointer of node or cfg.\n");

		return -EFAULT;
	}

	memset(buf, 0, 32);
	snprintf(buf, sizeof(buf), "tuner%d_name", index);
	ret = of_property_read_string(node, buf, &str);
	if (!ret) {
		cfg->id = aml_get_tuner_type(str);
		if (cfg->id == AM_TUNER_NONE)
			pr_err("Tuner: can't support tuner type: %s.\n", str);
	} else {
		pr_err("Tuner: can't get %s ret[%d].\n", buf, ret);

		cfg->id = AM_TUNER_NONE;

		return ret;
	}

	memset(buf, 0, 32);
	snprintf(buf, sizeof(buf), "tuner%d_i2c_adap", index);
	node_i2c = of_parse_phandle(node, buf, 0);
	if (!IS_ERR_OR_NULL(node_i2c)) {
		cfg->i2c_adap = of_find_i2c_adapter_by_node(node_i2c);
		of_node_put(node_i2c);
		if (IS_ERR_OR_NULL(cfg->i2c_adap)) {
			pr_err("Tuner: can't get i2c_get_adapter ret[NULL].\n");

			/* return -1; */
		}
	} else {
		pr_err("Tuner: can't get %s ret[NULL].\n", buf);

		cfg->i2c_adap = NULL;

		/* return -1; */
	}

	memset(buf, 0, 32);
	snprintf(buf, sizeof(buf), "tuner%d_i2c_addr", index);
	ret = of_property_read_u32(node, buf, &value);
	if (!ret) {
		cfg->i2c_addr = value;
	} else {
		pr_err("Tuner: can't get %s ret[%d].\n", buf, ret);

		cfg->i2c_addr = 0;
		ret = 0;

		/* return ret; */
	}

	memset(buf, 0, 32);
	snprintf(buf, sizeof(buf), "tuner%d_code", index);
	ret = of_property_read_u32(node, buf, &value);
	if (!ret) {
		cfg->code = value;
	} else {
		/* pr_err("Tuner: can't get %s ret[%d].\n", buf, ret); */

		cfg->code = 0;
		ret = 0;
	}

	memset(buf, 0, 32);
	snprintf(buf, sizeof(buf), "tuner%d_xtal", index);
	ret = of_property_read_u32(node, buf, &value);
	if (ret) {
		/* pr_err("Tuner: can't get %s ret[%d].\n", buf, ret); */

		cfg->xtal = 0;
		ret = 0;
	} else {
		cfg->xtal = value;
	}

	memset(buf, 0, 32);
	snprintf(buf, sizeof(buf), "tuner%d_xtal_mode", index);
	ret = of_property_read_u32(node, buf, &value);
	if (ret) {
		/* pr_err("Tuner: can't get %s ret[%d].\n", buf, ret); */

		cfg->xtal_mode = 0;
		ret = 0;
	} else {
		cfg->xtal_mode = value;
	}

	memset(buf, 0, 32);
	snprintf(buf, sizeof(buf), "tuner%d_xtal_cap", index);
	ret = of_property_read_u32(node, buf, &value);
	if (ret) {
		/* pr_err("Tuner: can't get %s ret[%d].\n", buf, ret); */

		cfg->xtal_cap = 0;
		ret = 0;
	} else {
		cfg->xtal_cap = value;
	}

	memset(buf, 0, 32);
	snprintf(buf, sizeof(buf), "tuner%d_lt_out", index);
	ret = of_property_read_u32(node, buf, &value);
	if (ret) {
		/* pr_err("Tuner: can't get %s ret[%d].\n", buf, ret); */

		cfg->lt_out = 0;
		ret = 0;
	} else {
		cfg->lt_out = value;
	}

	memset(buf, 0, 32);
	snprintf(buf, sizeof(buf), "tuner%d_dual_power", index);
	ret = of_property_read_u32(node, buf, &value);
	if (ret) {
		/* pr_err("Tuner: can't get %s ret[%d].\n", buf, ret); */

		cfg->dual_power = 0;
		ret = 0;
	} else {
		cfg->dual_power = value;
	}

	memset(buf, 0, 32);
	snprintf(buf, sizeof(buf), "tuner%d_if_agc", index);
	ret = of_property_read_u32(node, buf, &value);
	if (ret) {
		/* pr_err("Tuner: can't get %s ret[%d].\n", buf, ret); */

		cfg->if_agc = 0;
		ret = 0;
	} else {
		cfg->if_agc = value;
	}

	memset(buf, 0, 32);
	snprintf(buf, sizeof(buf), "tuner%d_if_hz", index);
	ret = of_property_read_u32(node, buf, &value);
	if (ret) {
		/* pr_err("Tuner: can't get %s ret[%d].\n", buf, ret); */

		cfg->if_hz = 0;
		ret = 0;
	} else {
		cfg->if_hz = value;
	}

	memset(buf, 0, 32);
	snprintf(buf, sizeof(buf), "tuner%d_if_invert", index);
	ret = of_property_read_u32(node, buf, &value);
	if (ret) {
		/* pr_err("Tuner: can't get %s ret[%d].\n", buf, ret); */

		cfg->if_invert = 0;
		ret = 0;
	} else {
		cfg->if_invert = value;
	}

	memset(buf, 0, 32);
	snprintf(buf, sizeof(buf), "tuner%d_if_amp", index);
	ret = of_property_read_u32(node, buf, &value);
	if (ret) {
		/* pr_err("Tuner: can't get %s ret[%d].\n", buf, ret); */

		cfg->if_amp = 0;
		ret = 0;
	} else {
		cfg->if_amp = value;
	}

	memset(buf, 0, 32);
	snprintf(buf, sizeof(buf), "tuner%d_detect", index);
	ret = of_property_read_u32(node, buf, &value);
	if (ret) {
		/* pr_err("Tuner: can't get %s ret[%d].\n", buf, ret); */

		cfg->detect = 0;
		ret = 0;
	} else {
		cfg->detect = value;
	}

	memset(buf, 0, 32);
	str = NULL;
	snprintf(buf, sizeof(buf), "tuner%d_reset_gpio", index);
	ret = of_property_read_string(node, buf, &str);
	if (ret) {
		/* pr_err("Tuner: can't get %s ret[%d].\n", buf, ret); */

		cfg->reset.pin = -1;
		ret = 0;
	} else {
		cfg->reset.pin = of_get_named_gpio_flags(node, buf, 0, NULL);
		pr_err("Tuner: get %s: %d.\n", buf, cfg->reset.pin);
	}

	memset(buf, 0, 32);
	snprintf(buf, sizeof(buf), "tuner%d_reset_dir", index);
	ret = of_property_read_u32(node, buf, &value);
	if (ret) {
		/* pr_err("Tuner: can't get %s ret[%d].\n", buf, ret); */

		cfg->reset.dir = 0;
		ret = 0;
	} else {
		cfg->reset.dir = value;
	}

	memset(buf, 0, 32);
	snprintf(buf, sizeof(buf), "tuner%d_reset_value", index);
	ret = of_property_read_u32(node, buf, &value);
	if (ret) {
		/* pr_err("Tuner: can't get %s ret[%d].\n", buf, ret); */

		cfg->reset.value = 0;
		ret = 0;
	} else {
		cfg->reset.value = value;
	}

	memset(buf, 0, 32);
	str = NULL;
	snprintf(buf, sizeof(buf), "tuner%d_power_gpio", index);
	ret = of_property_read_string(node, buf, &str);
	if (ret) {
		/* pr_err("Tuner: can't get %s ret[%d].\n", buf, ret); */

		cfg->power.pin = -1;
		ret = 0;
	} else {
		cfg->power.pin = of_get_named_gpio_flags(node, buf, 0, NULL);
		pr_err("Tuner: get %s: %d.\n", buf, cfg->power.pin);
	}

	memset(buf, 0, 32);
	snprintf(buf, sizeof(buf), "tuner%d_power_dir", index);
	ret = of_property_read_u32(node, buf, &value);
	if (ret) {
		/* pr_err("Tuner: can't get %s ret[%d].\n", buf, ret); */

		cfg->power.dir = 0;
		ret = 0;
	} else {
		cfg->power.dir = value;
	}

	memset(buf, 0, 32);
	snprintf(buf, sizeof(buf), "tuner%d_power_value", index);
	ret = of_property_read_u32(node, buf, &value);
	if (ret) {
		/* pr_err("Tuner: can't get %s ret[%d].\n", buf, ret); */

		cfg->power.value = 0;
		ret = 0;
	} else {
		cfg->power.value = value;
	}

	memset(buf, 0, 32);
	snprintf(buf, sizeof(buf), "tuner%d_reserved0", index);
	ret = of_property_read_u32(node, buf, &value);
	if (ret) {
		/* pr_err("Tuner: can't get %s ret[%d].\n", buf, ret); */

		cfg->reserved0 = 0;
		ret = 0;
	} else {
		cfg->reserved0 = value;
	}

	memset(buf, 0, 32);
	snprintf(buf, sizeof(buf), "tuner%d_reserved1", index);
	ret = of_property_read_u32(node, buf, &value);
	if (ret) {
		/* pr_err("Tuner: can't get %s ret[%d].\n", buf, ret); */

		cfg->reserved1 = 0;
		ret = 0;
	} else {
		cfg->reserved1 = value;
	}

	return 0;
}
EXPORT_SYMBOL(aml_get_dts_tuner_config);

void aml_show_tuner_config(const char *title, const struct tuner_config *cfg)
{
	if (IS_ERR_OR_NULL(title) || IS_ERR_OR_NULL(cfg))
		return;

	pr_err("[%s] name: %s, code: 0x%x, id: %d, i2c_addr: 0x%x, i2c_id: %d, i2c_adap: 0x%p, xtal: %d; xtal_cap: %d, xtal_mode: %d, lt_out: %d.\n",
			title,
			cfg->name ? cfg->name : "",
			cfg->code,
			cfg->id,
			cfg->i2c_addr,
			cfg->i2c_id,
			cfg->i2c_adap,
			cfg->xtal,
			cfg->xtal_cap,
			cfg->xtal_mode,
			cfg->lt_out);
	pr_err("[%s] reset pin: %d, dir: %d, value: %d.\n",
			title,
			cfg->reset.pin,
			cfg->reset.dir,
			cfg->reset.value);
	pr_err("[%s] power pin: %d, dir: %d, value: %d.\n",
			title,
			cfg->power.pin,
			cfg->power.dir,
			cfg->power.value);
	pr_err("[%s] tuner0 dual_power: %d (0:3.3v, 1:1.8v and 3.3v).\n",
			title,
			cfg->dual_power);
	pr_err("[%s] if_agc: %d (0:Self, 1:External), if_hz: %d Hz, if_invert: %d (0:Normal, 1:Inverted), if_amp: %d.\n",
			title,
			cfg->if_agc,
			cfg->if_hz,
			cfg->if_invert,
			cfg->if_amp);
	pr_err("[%s] detect: %d, reserved0: %d, reserved1: %d.\n",
			title,
			cfg->detect,
			cfg->reserved0,
			cfg->reserved1);
}
EXPORT_SYMBOL(aml_show_tuner_config);

enum tuner_type aml_get_tuner_type(const char *name)
{
	int i = 0;
	int count = ARRAY_SIZE(tuner_modules);

	for (i = 0; i < count; ++i) {
		if (!strncasecmp(name, tuner_modules[i].name,
				strlen(tuner_modules[i].name)))
			return tuner_modules[i].id;
	}

	return AM_TUNER_NONE;
}
EXPORT_SYMBOL(aml_get_tuner_type);

const struct tuner_module *aml_get_tuner_module(enum tuner_type type)
{
	int i = 0;
	int count = ARRAY_SIZE(tuner_modules);

	for (i = 0; i < count; ++i) {
		if (tuner_modules[i].id == type)
			return &tuner_modules[i];
	}

	pr_err("Tuner: get tuner [%d] module fail.\n", type);

	return NULL;
}
EXPORT_SYMBOL(aml_get_tuner_module);
