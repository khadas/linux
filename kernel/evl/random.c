/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2023 Lionel Minne  <lionel.minne@exail.com>
 * Copyright (C) 2023 Philippe Gerum  <rpm@xenomai.org>
 */

#include <linux/types.h>
#include <evl/factory.h>
#include <evl/random.h>
#include <evl/uaccess.h>
#include <uapi/evl/random-abi.h>

static const u64 EVL_RNG_PRIME_u8 = 409;

static const u64 EVL_RNG_PRIME_u16 = 106013;

static const u64 EVL_RNG_PRIME_u32 = 949402981;

#define EVL_RNG_SIZE	55
#define EVL_RNG_J	24
#define EVL_RNG_K	EVL_RNG_SIZE

#define EVL_DEFINE_RNG(__type)						\
	static struct evl_rng_ ## __type {				\
		__type array[EVL_RNG_SIZE];				\
		size_t index;						\
	} evl_rng_ ## __type;						\
									\
	static void init_rng_ ## __type(struct evl_rng_ ## __type *rng, __type seed) \
	{								\
		u64 val;						\
		size_t n;						\
									\
		rng->array[0] = seed;					\
		for (n = 0, val = seed; n < ARRAY_SIZE(rng->array); n++) { \
			val += EVL_RNG_PRIME_ ## __type;		\
			rng->array[n] = (__type)val;			\
		}							\
	}								\
									\
	static __type read_rng_ ## __type(struct evl_rng_ ## __type *rng) \
	{								\
		size_t n_j, n_k;					\
		__type res;						\
									\
		n_j = rng->index - EVL_RNG_J;				\
		if (EVL_RNG_J > rng->index)				\
			n_j += ARRAY_SIZE(rng->array);			\
									\
		n_k = rng->index - EVL_RNG_K;				\
		if (EVL_RNG_K > rng->index)				\
			n_k += ARRAY_SIZE(rng->array);			\
									\
		res = (__type)((u64)rng->array[n_j] + (u64)rng->array[n_k]); \
		rng->array[rng->index] = res;				\
		rng->index = (rng->index + 1) % ARRAY_SIZE(rng->array);	\
									\
		return res;						\
	}								\
									\
	__type evl_read_rng_ ## __type(void)				\
	{								\
		return read_rng_ ## __type(&evl_rng_ ## __type);	\
	}

EVL_DEFINE_RNG(u8);
EVL_DEFINE_RNG(u16);
EVL_DEFINE_RNG(u32);

static ssize_t rng_common_read(struct file *filp,
			char __user *u_buf, size_t count)
{
	u32 val32;

	if (count != sizeof(val32))
		return -EINVAL;

	val32 = evl_read_rng_u32();

	if (raw_put_user(val32, (u32 __user *)u_buf))
		return -EFAULT;

	return count;
}

static ssize_t rng_oob_read(struct file *filp,
			char __user *u_buf, size_t count)
{
	return rng_common_read(filp, u_buf, count);
}

static ssize_t rng_read(struct file *filp, char __user *u_buf,
			size_t count, loff_t *ppos)
{
	return rng_common_read(filp, u_buf, count);
}

static long rng_common_ioctl(struct file *filp, unsigned int cmd,
			unsigned long arg)
{
	__u32 val32;
	__u16 val16;
	__u8 val8;
	int ret;

	switch (cmd) {
	case EVL_RNGIOC_U8:
		val8 = evl_read_rng_u8();
		ret = raw_put_user(val8, (u8 __user *)arg);
		break;
	case EVL_RNGIOC_U16:
		val16 = evl_read_rng_u16();
		ret = raw_put_user(val16, (u16 __user *)arg);
		break;
	case EVL_RNGIOC_U32:
		val32 = evl_read_rng_u32();
		ret = raw_put_user(val32, (u32 __user *)arg);
		break;
	default:
		ret = -ENOTTY;
	}

	return ret > 0 ? -EFAULT : ret;
}

static long rng_oob_ioctl(struct file *filp, unsigned int cmd,
			unsigned long arg)
{
	return rng_common_ioctl(filp, cmd, arg);
}

static long rng_ioctl(struct file *filp, unsigned int cmd,
			unsigned long arg)
{
	return rng_common_ioctl(filp, cmd, arg);
}

void evl_init_rng(void)
{
	u64 now = ktime_get();

	init_rng_u8(&evl_rng_u8, (u8)now);
	init_rng_u16(&evl_rng_u16, (u16)now);
	init_rng_u32(&evl_rng_u32, (u32)now);
}

static const struct file_operations rng_fops = {
	.oob_ioctl	=	rng_oob_ioctl,
	.oob_read	=	rng_oob_read,
	.unlocked_ioctl	=	rng_ioctl,
	.read		=	rng_read,
#ifdef CONFIG_COMPAT
	.compat_ioctl	=	compat_ptr_ioctl,
	.compat_oob_ioctl =	compat_ptr_oob_ioctl,
#endif
};

struct evl_factory evl_rng_factory = {
	.name	=	"random",
	.fops	=	&rng_fops,
	.flags	=	EVL_FACTORY_SINGLE,
};
