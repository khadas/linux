#ifndef REGISTER_OPS_HEADER
#define REGISTER_OPS_HEADER
#include <linux/kernel.h>
#include <linux/amlogic/iomap.h>
#include <linux/io.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/types.h>
#include <linux/init.h>

enum
amports_bus_type_e {
	IO_DOS_BUS = 0,
	IO_AO_BUS,
	IO_HHI_BUS,
	IO_C_BUS,
	IO_VC_BUS,
	IO_DEMUX_BUS,
	IO_PARSER_BUS,
	IO_VLD_BUS,
	IO_AIU_BUS,
	IO_VDEC_BUS,
	IO_VDEC2_BUS,
	IO_HCODEC_BUS,
	IO_HEVC_BUS,
	IO_VPP_BUS,
	BUS_MAX
};

struct chip_register_ops {
	int bus_type;
	int ext_offset;
	int (*read)(unsigned int);
	void (*write)(unsigned int, unsigned int);
};

int codec_reg_read(u32 bus_type, unsigned int reg);
void codec_reg_write(u32 bus_type, unsigned int reg, unsigned int val);
void codec_reg_write_bits(u32 bus_type, unsigned int reg, u32 val, int start,
						  int len);

int register_reg_ops_per_cpu(int cputype, struct chip_register_ops *sops,
							 int ops_size);

int register_reg_ops_mgr(int cputype[], struct chip_register_ops *sops_list,
						 int ops_size);

#define DEF_BUS_OPS(BUS_TYPE, name)\
static inline void codec_##name##bus_write(unsigned int reg, u32 val)\
{\
	return codec_reg_write(BUS_TYPE, reg, val);\
} \
static inline int codec_##name##bus_read(unsigned int reg)\
{\
	return codec_reg_read(BUS_TYPE, reg);\
} \
static inline void codec_clear_##name##bus_mask(unsigned int r, u32 mask)\
{\
	codec_reg_write(BUS_TYPE, r, codec_reg_read(BUS_TYPE, r) & ~(mask));\
} \
static inline void codec_set_##name##bus_mask(unsigned int r, u32 mask)\
{\
	codec_reg_write(BUS_TYPE, r, codec_reg_read(BUS_TYPE, r) | (mask));\
} \
static inline void codec_set_##name##bus_bits(\
				unsigned int reg, u32 val, int start, int len)\
{\
	codec_reg_write_bits(BUS_TYPE, reg, val, start, len);\
} \


/*
sample:
DEF_BUS_OPS(IO_DOS_BUS,dos);
is define:
codec_dosbus_write();
codec_dosbus_read();
codec_clear_dosbus_mask();
codec_set_dosbus_mask();
codec_set_dosbus_bits();
...
*/

DEF_BUS_OPS(IO_DOS_BUS, dos);
DEF_BUS_OPS(IO_AO_BUS, ao);
DEF_BUS_OPS(IO_C_BUS, c);
DEF_BUS_OPS(IO_VC_BUS, vc);
DEF_BUS_OPS(IO_HHI_BUS, hhi);

#endif
