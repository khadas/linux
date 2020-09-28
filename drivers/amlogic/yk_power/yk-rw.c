#include <linux/init.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <mach/irqs.h>
#include <linux/module.h>
#include <linux/apm_bios.h>
#include <linux/apm-emulation.h>
#include <linux/mfd/yk-mfd.h>
#include "yk-cfg.h"
#include "yk-rw.h"

int yk_register_notifier(struct device *dev, struct notifier_block *nb,
				uint64_t irqs)
{
	struct yk_dev *chip = dev_get_drvdata(dev);

	chip->ops->enable_irqs(chip, irqs);
	if(NULL != nb) {
	    return blocking_notifier_chain_register(&chip->notifier_list, nb);
	}

    return 0;
}
EXPORT_SYMBOL_GPL(yk_register_notifier);

int yk_unregister_notifier(struct device *dev, struct notifier_block *nb,
				uint64_t irqs)
{
	struct yk_dev *chip = dev_get_drvdata(dev);

	chip->ops->disable_irqs(chip, irqs);
	if(NULL != nb) {
	    return blocking_notifier_chain_unregister(&chip->notifier_list, nb);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(yk_unregister_notifier);

int yk_write(struct device *dev, int reg, uint8_t val)
{


	return __yk_write(to_i2c_client(dev), reg, val);
}
EXPORT_SYMBOL_GPL(yk_write);

int yk_writes(struct device *dev, int reg, int len, uint8_t *val)
{


	return  __yk_writes(to_i2c_client(dev), reg, len, val);
}
EXPORT_SYMBOL_GPL(yk_writes);

int yk_read(struct device *dev, int reg, uint8_t *val)
{


	return __yk_read(to_i2c_client(dev), reg, val);
}
EXPORT_SYMBOL_GPL(yk_read);

int yk_reads(struct device *dev, int reg, int len, uint8_t *val)
{


	return __yk_reads(to_i2c_client(dev), reg, len, val);
}
EXPORT_SYMBOL_GPL(yk_reads);

int yk_set_bits(struct device *dev, int reg, uint8_t bit_mask)
{
	uint8_t reg_val;
	int ret = 0;
	struct yk_dev *chip;

	chip = dev_get_drvdata(dev);

	mutex_lock(&chip->lock);
	ret = __yk_read(chip->client, reg, &reg_val);
	if (ret)
		goto out;

	if ((reg_val & bit_mask) != bit_mask) {
		reg_val |= bit_mask;
		ret = __yk_write(chip->client, reg, reg_val);
	}
out:
	mutex_unlock(&chip->lock);
	return ret;
}
EXPORT_SYMBOL_GPL(yk_set_bits);

int yk_clr_bits(struct device *dev, int reg, uint8_t bit_mask)
{
	uint8_t reg_val;
	int ret = 0;
	struct yk_dev *chip;

	chip = dev_get_drvdata(dev);

	mutex_lock(&chip->lock);

	ret = __yk_read(chip->client, reg, &reg_val);
	if (ret)
		goto out;

	if (reg_val & bit_mask) {
		reg_val &= ~bit_mask;
		ret = __yk_write(chip->client, reg, reg_val);
	}
out:
	mutex_unlock(&chip->lock);
	return ret;
}
EXPORT_SYMBOL_GPL(yk_clr_bits);

int yk_update(struct device *dev, int reg, uint8_t val, uint8_t mask)
{
	struct yk_dev *chip = dev_get_drvdata(dev);
	uint8_t reg_val;
	int ret = 0;

	mutex_lock(&chip->lock);

	ret = __yk_read(chip->client, reg, &reg_val);
	if (ret)
		goto out;

	if ((reg_val & mask) != val) {
		reg_val = (reg_val & ~mask) | val;
		ret = __yk_write(chip->client, reg, reg_val);
	}
out:
	mutex_unlock(&chip->lock);
	return ret;
}
EXPORT_SYMBOL_GPL(yk_update);

