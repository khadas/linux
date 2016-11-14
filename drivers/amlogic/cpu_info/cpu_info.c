#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/platform_device.h>
#include <linux/amlogic/iomap.h>
#ifndef CONFIG_ARM64
#include <asm/opcodes-sec.h>
#endif
#include <linux/amlogic/secmon.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/amlogic/cpu_version.h>
static int cpuinfo_func_id;

unsigned int system_serial_low0;
unsigned int system_serial_low1;
unsigned int system_serial_high0;
unsigned int system_serial_high1;

#undef pr_fmt
#define pr_fmt(fmt) "cpuinfo: " fmt
struct aml_cpu_info {
	unsigned int version;
	u8 chipid[12];
	unsigned int  cpuid;
	unsigned int reserved[102];
};
#define MAGIC_31		0xB31B31
#define MATCH_31(magic)	(((magic >> 8) & 0xffffff) == MAGIC_31)
static void __iomem *sharemem_output;
static struct aml_cpu_info *cpu_info_buf;
static noinline int fn_smc(u64 function_id, u64 arg0, u64 arg1,
					 u64 arg2)
{
	register long x0 asm("x0") = function_id;
	register long x1 asm("x1") = arg0;
	register long x2 asm("x2") = arg1;
	register long x3 asm("x3") = arg2;
	asm volatile(
			__asmeq("%0", "x0")
			__asmeq("%1", "x1")
			__asmeq("%2", "x2")
			__asmeq("%3", "x3")
			"smc	#0\n"
		: "+r" (x0)
		: "r" (x1), "r" (x2), "r" (x3));

	return function_id;
}

static int cpuinfo_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	unsigned id;
	unsigned int *p = NULL;

	if (!of_property_read_u32(np, "cpuinfo_cmd", &id))
		cpuinfo_func_id = id;
	cpu_info_buf = kzalloc(sizeof(struct aml_cpu_info), GFP_KERNEL);
	if (!cpu_info_buf) {
		pr_info("No memory to alloc\n");
		return  -ENOMEM;
	}
	sharemem_output = get_secmon_sharemem_output_base();
	if (!sharemem_output) {
		pr_info("secmon share mem prepare not okay\n");
		return  -ENOMEM;
	}
	sharemem_mutex_lock();
	fn_smc(cpuinfo_func_id, 1, 0, 0);
	memcpy((void *)cpu_info_buf,
		(const void *)sharemem_output, sizeof(struct aml_cpu_info));
	sharemem_mutex_unlock();
#if 0
	int i;
	unsigned int *p = (unsigned int *)(cpu_info_buf->chipid);
	for (i = 0; i < 12; i++)
		pr_info("cpu_info_buf->chipid[%d]=%x\n",
					i, cpu_info_buf->chipid[i]);
#endif
	p = (unsigned int *)(cpu_info_buf->chipid);
	system_serial_low0 = *p;
	system_serial_low1 = *(p+1);
	system_serial_high0 = *(p+2);
	if (MATCH_31(cpu_info_buf->version) && (cpu_info_buf->version & 0xff))
		system_serial_high1 = cpu_info_buf->cpuid;
	else {
		unsigned int version =
		(get_meson_cpu_version(MESON_CPU_VERSION_LVL_MAJOR) << 24) |
		(get_meson_cpu_version(MESON_CPU_VERSION_LVL_MINOR) << 16) |
		(get_meson_cpu_version(MESON_CPU_VERSION_LVL_PACK) << 8);
		system_serial_high1 = version;
	}

	pr_info("probe done\n");
	return 0;
}

void cpuinfo_get_chipid(unsigned int *low0, unsigned int *low1,
		unsigned int *high0, unsigned int *high1)
{
	*low0 = system_serial_low0;
	*low1 = system_serial_low1;
	*high0 = system_serial_high0;
	*high1 = system_serial_high1;
}

static const struct of_device_id cpuinfo_dt_match[] = {
	{ .compatible = "amlogic, cpuinfo" },
	{ /* sentinel */ },
};

static  struct platform_driver cpuinfo_platform_driver = {
	.probe		= cpuinfo_probe,
	.driver		= {
		.owner		= THIS_MODULE,
		.name		= "cpuinfo",
		.of_match_table	= cpuinfo_dt_match,
	},
};

int __init meson_cpuinfo_init(void)
{
	return  platform_driver_register(&cpuinfo_platform_driver);
}
module_init(meson_cpuinfo_init);

