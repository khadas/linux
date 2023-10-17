/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __SPICC_TEST_H__
#define __SPICC_TEST_H__

struct test_device {
	struct spi_device		*spi;
	struct list_head		list;
	struct spicc_controller_data	cdata;
	struct spi_message		msg;
	int				nxfers;
	bool				hexdump;
	bool				compare;
	bool				pr_diff;
};

#define spicc_hexdump(buf, len)						\
	print_hex_dump(KERN_INFO, "", DUMP_PREFIX_OFFSET,		\
	16, 1, buf, len, true)

int spicc_make_argv(char *s, int argvsz, char *argv[], char *delim);
int spicc_getopt(int argc, char *argv[], char *name,
		 unsigned long *value, char **str, unsigned int base);
void spicc_strtohex(char *str, int pass, u8 *buf, int len);
int spicc_compare(u8 *buf1, u8 *buf2, int len, bool pr_diff);

ssize_t testdev_dump(struct test_device *testdev, char *buf);
struct test_device *testdev_get(struct device *dev);
struct test_device *testdev_new(struct device *dev, int argc, char *argv[]);
int testdev_free(struct test_device *testdev);
int testdev_setup(struct test_device *testdev, int argc, char *argv[]);
int testdev_new_xfer(struct test_device *testdev, int argc, char *argv[]);
void testdev_free_xfer(struct test_device *testdev);
int testdev_run(struct test_device *testdev, int argc, char *argv[]);
int test(struct device *dev, const char *buf);

static inline ssize_t testdev_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct test_device *testdev = testdev_get(dev);
	char *kstr, *argv[20];
	int argc;

	kstr = kstrdup(buf, GFP_KERNEL);
	if (IS_ERR_OR_NULL(kstr)) {
		dev_err(dev, "kstrdup failed\n");
		return count;
	}
	memset(argv, 0, sizeof(argv));
	argc = spicc_make_argv(kstr, ARRAY_SIZE(argv), argv, "-");

	if (!spicc_getopt(argc, argv, "new", NULL, NULL, 0))
		testdev = testdev_new(dev, argc, argv);
	else if (!testdev)
		dev_warn(dev, "testdev unavailable\n");
	else if (!spicc_getopt(argc, argv, "free", NULL, NULL, 0))
		testdev_free(testdev);
	else if (!spicc_getopt(argc, argv, "setup", NULL, NULL, 0))
		testdev_setup(testdev, argc, argv);
	else if (!spicc_getopt(argc, argv, "free_xfer", NULL, NULL, 0))
		testdev_free_xfer(testdev);
	else if (!spicc_getopt(argc, argv, "new_xfer", NULL, NULL, 0))
		testdev_new_xfer(testdev, argc, argv);
	else if (!spicc_getopt(argc, argv, "run", NULL, NULL, 0))
		testdev_run(testdev, argc, argv);
	else
		dev_err(dev, "entry unsupported\n");

	kfree(kstr);
	return count;
}

static inline ssize_t testdev_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct test_device *testdev = testdev_get(dev);

	if (!testdev) {
		dev_warn(dev, "testdev unavailable\n");
		return 0;
	}

	return testdev_dump(testdev, buf);
}

static inline ssize_t test_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	test(dev, buf);
	return count;
}

#endif
