#include <linux/amlogic/saradc.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/scpi_protocol.h>
#include <linux/printk.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/cpufreq.h>
#include <linux/cpu_cooling.h>
#include <linux/cpucore_cooling.h>
#include <linux/gpucore_cooling.h>
#include <linux/gpu_cooling.h>
#include <linux/thermal_core.h>
#include <linux/opp.h>
#include <linux/cpu.h>

#define NOT_WRITE_EFUSE		0x0
#define EFUEE_PRIVATE		0x4
#define EFUSE_OPS		0xa

struct aml_thermal_sensor {
	int chip_trimmed;
	struct cpumask mask;
	struct thermal_cooling_device *cpufreq_cdev;
	struct thermal_cooling_device *cpucore_cdev;
	struct thermal_cooling_device *gpufreq_cdev;
	struct thermal_cooling_device *gpucore_cdev;
	struct thermal_zone_device    *tzd;
};

static struct aml_thermal_sensor soc_sensor;
static int min_buf[4] = {};
static int min_exist;

int thermal_firmware_init(void)
{
	int ret;

	ret = scpi_get_sensor("aml_thermal");
	soc_sensor.chip_trimmed = ret < 0 ? 0 : 1;
	return ret;
}
EXPORT_SYMBOL(thermal_firmware_init);

int get_cpu_temp(void)
{
	unsigned int val = 0;

	if (soc_sensor.chip_trimmed) {	/* only supported trimmed chips */
		if (scpi_get_sensor_value(0, &val) < 0)
			return -1000;
		return val;
	} else {
		return -1000;
	}
}
EXPORT_SYMBOL(get_cpu_temp);

static int get_cur_temp(void *data, long *temp)
{
	int val;
	val = get_cpu_temp();
	if (val == -1000)
		return -EINVAL;

	*temp = val * 1000;

	return 0;
}

int aml_thermal_min_update(struct thermal_cooling_device *cdev)
{
	struct gpufreq_cooling_device *gf_cdev;
	struct gpucore_cooling_device *gc_cdev;
	struct thermal_instance *ins;
	long min_state;
	int i;

	if (!min_exist)
		return 0;
	if (strstr(cdev->type, "gpufreq")) {
		gf_cdev = (struct gpufreq_cooling_device *)cdev->devdata;
		min_state = gf_cdev->get_gpu_freq_level(min_buf[1]);
		soc_sensor.gpufreq_cdev = cdev;
		for (i = 0; i < soc_sensor.tzd->trips; i++) {
			ins = get_thermal_instance(soc_sensor.tzd,
						   cdev, i);
			if (ins) {
				ins->upper = min_state;
				pr_info("%s set upper to %ld\n",
					 ins->name, ins->upper);
			}
		}
	}

	if (strstr(cdev->type, "gpucore")) {
		gc_cdev = (struct gpucore_cooling_device *)cdev->devdata;
		soc_sensor.gpucore_cdev = cdev;
		cdev->ops->get_max_state(cdev, &min_state);
		min_state = min_state - min_buf[3];
		for (i = 0; i < soc_sensor.tzd->trips; i++) {
			ins = get_thermal_instance(soc_sensor.tzd,
						   cdev, i);
			if (ins) {
				ins->upper = min_state;
				pr_info("%s set upper to %ld\n",
					 ins->name, ins->upper);
			}
		}
	}

	return 0;
}
EXPORT_SYMBOL(aml_thermal_min_update);

static struct thermal_zone_of_device_ops aml_thermal_ops = {
	.get_temp = get_cur_temp,
};

static int aml_thermal_probe(struct platform_device *pdev)
{
	int cpu, i, pp;
	int dyn_coeff = 0;
	unsigned long min_state;
	char node_name[32] = {};
	struct device_node *np;
	struct thermal_instance *ins;

	memset(&soc_sensor, 0, sizeof(struct aml_thermal_sensor));
	if (!cpufreq_frequency_get_table(0)) {
		dev_info(&pdev->dev,
			"Frequency table not initialized. Deferring probe...\n");
		return -EPROBE_DEFER;
	}

	if (thermal_firmware_init() < 0) {
		dev_err(&pdev->dev, "chip is not trimmed, disable thermal\n");
		return -EINVAL;
	}

	for_each_possible_cpu(cpu) {
		cpumask_set_cpu(cpu, &soc_sensor.mask);
	}

	np = pdev->dev.of_node;
	if (of_property_read_u32_array(np, "min_state", min_buf, 4)) {
		pr_debug("read min_state failed\n");
		min_exist = 0;
	} else {
		pr_debug("min state:%d %d %d %d\n",
			 min_buf[0], min_buf[1], min_buf[2], min_buf[3]);
		min_exist = 1;
	}

	/*
	 * Many different cooling devices, we need to get their configs
	 * 1. cpu core cooling
	 */
	snprintf(node_name, sizeof(node_name), "%s", "thermal_cpu_cores");
	np = of_find_node_by_name(NULL, node_name);
	if (!np)
		pr_debug("not found cpucore node\n");
	else {
		soc_sensor.cpucore_cdev = cpucore_cooling_register(np);
		if (IS_ERR(soc_sensor.cpucore_cdev)) {
			pr_debug("Error cpu core cooling device, cdev:%p\n",
				soc_sensor.cpucore_cdev);
		}
	}

	/* 2. cpu freq cooling */
	np = pdev->dev.of_node;
	if (of_property_read_u32(np, "cpu_dyn_coeff", &dyn_coeff))
		pr_debug("read cpu_dyn_coeff failed\n");
	memset(node_name, 0, sizeof(node_name));
	snprintf(node_name, 16, "cpus");
	np = of_find_node_by_name(NULL, node_name);
	if (!np) {
		pr_debug("not found cpu node\n");
	} else {
		soc_sensor.cpufreq_cdev = of_cpufreq_power_cooling_register(np,
							&soc_sensor.mask,
							dyn_coeff,
							NULL);
		if (IS_ERR(soc_sensor.cpufreq_cdev)) {
			pr_debug("Error cpu freq cooling device, cdev:%p\n",
				soc_sensor.cpufreq_cdev);
		}
	}

	/* 3. gpu core cooling */
	np = pdev->dev.of_node;
	snprintf(node_name, sizeof(node_name), "%s", "thermal_gpu_cores");
	np = of_find_node_by_name(NULL, node_name);
	if (!np) {
		pr_debug("not found gpucore node\n");
	} else {
		/*
		 * gpu is ko, save parsed parameters to wating ko insert
		 */
		save_gpucore_thermal_para(np);
	}

	/* 4. gpu frequent cooling */
	np = pdev->dev.of_node;
	if (of_property_read_u32(np, "gpu_dyn_coeff", &dyn_coeff)) {
		pr_debug("read gpu_dyn_coeff failed\n");
		goto next;
	}
	snprintf(node_name, sizeof(node_name), "%s", "mali");
	np = of_find_node_by_name(NULL, node_name);
	if (!np) {
		pr_debug("not found mali node\n");
	} else {
		of_property_read_u32(np, "num_of_pp", &pp);
		pr_debug("gpu coef:%d, pp:%d\n", dyn_coeff, pp);
		save_gpu_cool_para(dyn_coeff, np, pp);
	}

next:
	soc_sensor.tzd = thermal_zone_of_sensor_register(&pdev->dev,
							  3,
							  &soc_sensor,
							  &aml_thermal_ops);

	if (IS_ERR(soc_sensor.tzd)) {
		dev_warn(&pdev->dev, "Error registering sensor: %p\n",
			 soc_sensor.tzd);
		return PTR_ERR(soc_sensor.tzd);
	}

	thermal_zone_device_update(soc_sensor.tzd);
	if (!min_exist)
		return 0;

	/*
	 * adjust upper and lower for each instance
	 */
	min_state = cpufreq_cooling_get_level(0, min_buf[0]);
	for (i = 0; i < soc_sensor.tzd->trips; i++) {
		ins = get_thermal_instance(soc_sensor.tzd,
					   soc_sensor.cpufreq_cdev, i);
		if (ins) {
			ins->upper = min_state;
			pr_debug("%s set upper to %ld\n",
				 ins->name, ins->upper);
		}
	}
	soc_sensor.cpucore_cdev->ops->get_max_state(soc_sensor.cpucore_cdev,
		&min_state);
	min_state = min_state - min_buf[2];
	for (i = 0; i < soc_sensor.tzd->trips; i++) {
		ins = get_thermal_instance(soc_sensor.tzd,
					   soc_sensor.cpucore_cdev, i);
		if (ins) {
			ins->upper = min_state;
			pr_debug("%s set upper to %ld\n",
				 ins->name, ins->upper);
		}
	}


	thermal_zone_device_update(soc_sensor.tzd);
	return 0;
}

static int aml_thermal_remove(struct platform_device *pdev)
{
	return 0;
}

static struct of_device_id aml_thermal_of_match[] = {
	{ .compatible = "amlogic, aml-thermal" },
	{},
};

static struct platform_driver aml_thermal_platdrv = {
	.driver = {
		.name		= "aml-thermal",
		.owner		= THIS_MODULE,
		.of_match_table = aml_thermal_of_match,
	},
	.probe	= aml_thermal_probe,
	.remove	= aml_thermal_remove,
};


static int __init aml_thermal_platdrv_init(void)
{
	 return platform_driver_register(&(aml_thermal_platdrv));
}
late_initcall(aml_thermal_platdrv_init);
