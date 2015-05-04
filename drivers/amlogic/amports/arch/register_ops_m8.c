#include "register_ops.h"

#define REGISTER_FOR_CPU {\
			MESON_CPU_MAJOR_ID_M8,\
			MESON_CPU_MAJOR_ID_M8M2,\
			MESON_CPU_MAJOR_ID_GXBB, \
			0}

int codec_apb_read(unsigned int reg)
{
	unsigned int val = 0;
	aml_reg_read(IO_APB_BUS_BASE, reg << 2, &val);
	return val;
}

void codec_apb_write(unsigned int reg, unsigned int val)
{
	aml_reg_write(IO_APB_BUS_BASE, reg << 2, val);
	return;
}

static struct chip_register_ops m8_ops[] __initdata = {
	/*0x50000>>2 == 0x14000 */
	{IO_DOS_BUS, 0x14000, codec_apb_read, codec_apb_write},
	{IO_VC_BUS, 0, aml_read_vcbus, aml_write_vcbus},
	{IO_C_BUS, 0, aml_read_cbus, aml_write_cbus},
	{IO_HHI_BUS, 0, aml_read_cbus, aml_write_cbus},
	{IO_AO_BUS, 0, aml_read_aobus, aml_write_aobus},
	{IO_VPP_BUS, 0, aml_read_vcbus, aml_write_vcbus},
};

static struct chip_register_ops ex_gx_ops[] __initdata = {
	/*
	   #define HHI_VDEC_CLK_CNTL 0x1078
	   to
	   #define HHI_VDEC_CLK_CNTL 0x78
	   on Gxbb.
	   will changed later..
	 */
	{IO_HHI_BUS, -0x1000, aml_read_cbus, aml_write_cbus},
	{IO_DOS_BUS, 0, codec_apb_read, codec_apb_write},
};

static int __init vdec_reg_ops_init(void)
{
	int cpus[] = REGISTER_FOR_CPU;
	register_reg_ops_mgr(cpus, m8_ops,
		sizeof(m8_ops) / sizeof(struct chip_register_ops));

	register_reg_ops_per_cpu(MESON_CPU_MAJOR_ID_GXBB,
		ex_gx_ops, sizeof(ex_gx_ops) /
		sizeof(struct chip_register_ops));

	return 0;
}

postcore_initcall(vdec_reg_ops_init);
