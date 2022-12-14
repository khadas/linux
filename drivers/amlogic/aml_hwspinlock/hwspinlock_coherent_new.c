// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2013-2015, ARM Limited and Contributors. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * Neither the name of ARM nor the names of its contributors may be used
 * to endorse or promote products derived from this software without specific
 * prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <linux/spinlock.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/hwspinlock.h>
#include <linux/pm_runtime.h>
#include "hwspinlock_internal.h"
#include "hwspinlock_coherent_new.h"

#define DRIVER_NAME		"aml_bak_hwspinlock"
#define HWSPINLOCK_DEFAULT_VALUE	0xa5a5a5a5
#define HWSPINLOCK_MAX_CPUS	5
#define HWSPINLOCK_NUMS		5
/*****************************************************************************
 * Internal helper macros used by the amlspinlock implementation.
 ****************************************************************************/
/* Convert a ticket to priority */
#define PRIORITY(t, pos)	(((t) << 8) | (pos))

#define CHOOSING_TICKET		0x1
#define CHOSEN_TICKET		0x0

#define bakery_is_choosing(info)	((info) & 0x1)
#define bakery_ticket_number(info)	(((info) >> 1) & 0x7FFF)
#define make_bakery_data(choosing, number) \
		((((choosing) & 0x1) | ((number) << 1)) & 0xFFFF)

struct aml_hwspinlock_t {
	struct hwspinlock_device *bank;
	u32 bakery_cpus;
};

/*****************************************************************************
 * External bakery lock interface.
 ****************************************************************************/
/*
 * Bakery locks are stored in coherent memory
 *
 * Each lock's data is contiguous and fully allocated by the compiler
 */

struct bakery_lock {
	/*
	 * The lock_data is a bit-field of 2 members:
	 * Bit[0]       : choosing. This field is set when the CPU is
	 *                choosing its bakery number.
	 * Bits[1 - 15] : number. This is the bakery number allocated.
	 * Bits[16 - 31] : reserve. device memory may do not support 16bit width access?
	 */
	volatile u32 lock_data[HWSPINLOCK_MAX_CPUS];
};

struct aml_hwspinlock_t *aml_spinlock;
/*
 * Functions in this file implement Bakery Algorithm for mutual exclusion with the
 * bakery lock data structures in coherent memory.
 *
 * ARM architecture offers a family of exclusive access instructions to
 * efficiently implement mutual exclusion with hardware support. However, as
 * well as depending on external hardware, the these instructions have defined
 * behavior only on certain memory types (cacheable and Normal memory in
 * particular; see ARMv8 Architecture Reference Manual section B2.10). Use cases
 * in trusted firmware are such that mutual exclusion implementation cannot
 * expect that accesses to the lock have the specific type required by the
 * architecture for these primitives to function (for example, not all
 * contenders may have address translation enabled).
 *
 * This implementation does not use mutual exclusion primitives. It expects
 * memory regions where the locks reside to be fully ordered and coherent
 * (either by disabling address translation, or by assigning proper attributes
 * when translation is enabled).
 *
 * Note that the ARM architecture guarantees single-copy atomicity for aligned
 * accesses regardless of status of address translation.
 */
void __assert(const char *function, const char *file, u32 line,
		const char *assertion)
{
	pr_err("ASSERT: %s <%d> : %s\n", function, line, assertion);
	while (1)
		;
}

#define	assert(e)	((e) ? (void)0 : __assert(__func__, __FILE__, \
			    __LINE__, #e))

#define assert_bakery_entry_valid(entry, bakery) do {	\
	assert(bakery);					\
	assert((entry) < aml_spinlock->bakery_cpus);		\
} while (0)

static inline u32 plat_my_core_pos(void)
{
	return 0;
}

/* Obtain a ticket for a given CPU */
static unsigned int pBakery_get_ticket(struct bakery_lock *bakery,
				       u32 me, u32 cpus)
{
	unsigned int my_ticket, their_ticket;
	unsigned int they;

	/* Prevent recursive acquisition */
	assert(!bakery_ticket_number(bakery->lock_data[me]));

	/*
	 * Flag that we're busy getting our ticket. All CPUs are iterated in the
	 * order of their ordinal position to decide the maximum ticket value
	 * observed so far. Our priority is set to be greater than the maximum
	 * observed priority
	 *
	 * Note that it's possible that more than one contender gets the same
	 * ticket value. That's OK as the lock is acquired based on the priority
	 * value, not the ticket value alone.
	 */
	my_ticket = 0;
	bakery->lock_data[me] = make_bakery_data(CHOOSING_TICKET, my_ticket);
	for (they = 0; they < cpus; they++) {
		if (they == me || bakery->lock_data[they] == HWSPINLOCK_DEFAULT_VALUE)
			continue;
		their_ticket = bakery_ticket_number(bakery->lock_data[they]);
		if (their_ticket > my_ticket)
			my_ticket = their_ticket;
	}

	/*
	 * Compute ticket; then signal to other contenders waiting for us to
	 * finish calculating our ticket value that we're done
	 */
	++my_ticket;
	bakery->lock_data[me] = make_bakery_data(CHOSEN_TICKET, my_ticket);

	return my_ticket;
}

/*
 * Acquire bakery lock
 *
 * Contending CPUs need first obtain a non-zero ticket and then calculate
 * priority value. A contending CPU iterate over all other CPUs in the platform,
 * which may be contending for the same lock, in the order of their ordinal
 * position (CPU0, CPU1 and so on). A non-contending CPU will have its ticket
 * (and priority) value as 0. The contending CPU compares its priority with that
 * of others'. The CPU with the highest priority (lowest numerical value)
 * acquires the lock
 */
static int aml_hwspinlock_trylock(struct hwspinlock *hwlock)
{
	unsigned int they, me;
	unsigned int my_ticket, my_prio, their_ticket;
	unsigned int their_bakery_data;
	struct bakery_lock *bakery = (struct bakery_lock *)hwlock->priv;
	int bakery_cpus = aml_spinlock->bakery_cpus;

	me = plat_my_core_pos();

	assert_bakery_entry_valid(me, bakery);

	/* Get a ticket */
	my_ticket = pBakery_get_ticket(bakery, me, bakery_cpus);

	/*
	 * Now that we got our ticket, compute our priority value, then compare
	 * with that of others, and proceed to acquire the lock
	 */
	my_prio = PRIORITY(my_ticket, me);
	for (they = 0; they < bakery_cpus; they++) {
		if (me == they || bakery->lock_data[they] == HWSPINLOCK_DEFAULT_VALUE)
			continue;

		/* Wait for the contender to get their ticket */
		do {
			their_bakery_data = bakery->lock_data[they];
		} while (bakery_is_choosing(their_bakery_data));

		/*
		 * If the other party is a contender, they'll have non-zero
		 * (valid) ticket value. If they do, compare priorities
		 */
		their_ticket = bakery_ticket_number(their_bakery_data);
		if (their_ticket && (PRIORITY(their_ticket, they) < my_prio)) {
			/*
			 * They have higher priority (lower value). Wait for
			 * their ticket value to change (either release the lock
			 * to have it dropped to 0; or drop and probably content
			 * again for the same lock to have an even higher value)
			 */
			do {
				;
			} while (their_ticket ==
				bakery_ticket_number(bakery->lock_data[they]));
		}
	}
	/* Lock acquired */
	return true;
}

/* Release the lock and signal contenders */
static void aml_hwspinlock_unlock(struct hwspinlock *hwlock)
{
	unsigned int me = plat_my_core_pos();
	struct bakery_lock *bakery = (struct bakery_lock *)hwlock->priv;

	assert_bakery_entry_valid(me, bakery);
	assert(bakery_ticket_number(bakery->lock_data[me]));

	/*
	 * Release lock by resetting ticket. Then signal other
	 * waiting contenders
	 */
	bakery->lock_data[me] = 0;
	/* data clear before spin unlock*/
	mb();
}

#ifdef CONFIG_AML_HWSPINLOCK_TEST
void test_hwspin_lock(struct hwspinlock *hwlock)
{
	u32 i = 200;
	void *hwlock_addr = hwlock->priv;
	int hwlock_id = hwlock_to_id(hwlock);
	u32 addr_off = sizeof(struct bakery_lock) * (HWSPINLOCK_NUMS - hwlock_id);
	int val = 0x1234 + hwlock_id;

	addr_off += sizeof(u32) * hwlock_id;
	pr_debug("armv8 bakery test get %px %px %x\n",
		hwlock_addr, ((char *)hwlock_addr + addr_off),
		readl((char *)hwlock_addr + addr_off));
	/*Enter critical section*/
	do {
		writel(val, (char *)hwlock_addr + addr_off);
		if (readl((char *)hwlock_addr + addr_off) != val) {
			pr_err("armv8 Error: bakery_lock test fail idx %d. %x\n",
				hwlock_id, readl((char *)hwlock_addr + addr_off));
			return;
		}
		if (readl((char *)hwlock_addr + addr_off) != val) {
			pr_err("armv8 Error: bakery_lock test fail idx %d. %x\n",
				hwlock_id, readl((char *)hwlock_addr + addr_off));
			return;
		}
	} while (i--);

	pr_debug("armv8 bakery_lock_release done.\n");
}
#endif

static const struct hwspinlock_ops aml_hwspinlock_ops = {
	.trylock = aml_hwspinlock_trylock,
	.unlock = aml_hwspinlock_unlock,
};

static int aml_hwspinlock_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct device *dev = &pdev->dev;
	struct hwspinlock_device *bank;
	struct hwspinlock *hwlock;
	struct bakery_lock *bakery_lock;
	void __iomem *addr;
	u32 bakery_cpus;
	int err, i;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(dev, "failed to get bakery memory resource\n");
		return -ENXIO;
	}
	addr = devm_ioremap_nocache(dev, res->start, res->end - res->start);
	if (IS_ERR_OR_NULL(addr))
		return PTR_ERR(addr);

	err = of_property_read_u32(dev->of_node,
				   "bakery-cpus", &bakery_cpus);
	if (err || bakery_cpus < 2) {
		dev_err(dev, "not have bakery %d\n", err);
		err = -ENODEV;
		goto probe_err;
	}

	bank = devm_kzalloc(dev, struct_size(bank, lock, HWSPINLOCK_NUMS),
			    GFP_KERNEL);
	if (!bank) {
		err = -ENOMEM;
		goto probe_err;
	}

	aml_spinlock = devm_kzalloc(dev, sizeof(*aml_spinlock), GFP_KERNEL);
	if (!aml_spinlock) {
		err = -ENOMEM;
		goto probe_err;
	}
	aml_spinlock->bank = bank;
	aml_spinlock->bakery_cpus = bakery_cpus;

	for (i = 0; i < HWSPINLOCK_NUMS; i++) {
		hwlock = &bank->lock[i];
		hwlock->priv = (char *)addr + i * sizeof(*bakery_lock);
		bakery_lock = hwlock->priv;
		bakery_lock->lock_data[plat_my_core_pos()] = 0;
	}
	pm_runtime_enable(dev);
	err = hwspin_lock_register(bank, dev, &aml_hwspinlock_ops, 0, HWSPINLOCK_NUMS);
	if (err) {
		pm_runtime_disable(dev);
		goto probe_err;
	}
	platform_set_drvdata(pdev, aml_spinlock);

	pr_info("%s success %px\n", __func__, addr);
	return 0;
probe_err:
	return err;
}

static int aml_hwspinlock_remove(struct platform_device *pdev)
{
	hwspin_lock_unregister(aml_spinlock->bank);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

static const struct of_device_id hwlock_of_match[] = {
	{ .compatible = "amlogic, meson-hwspinlock" },
	{},
};

static struct platform_driver aml_hwspinlock_driver = {
	.probe = aml_hwspinlock_probe,
	.remove = aml_hwspinlock_remove,
	.driver = {
		.owner		= THIS_MODULE,
		.name = DRIVER_NAME,
		.of_match_table = hwlock_of_match,
	},
};

int __init aml_bak_hwspinlock_init(void)
{
	return platform_driver_register(&aml_hwspinlock_driver);
}

void __exit aml_bak_hwspinlock_exit(void)
{
	platform_driver_unregister(&aml_hwspinlock_driver);
}
