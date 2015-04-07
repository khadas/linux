#include "../include/phynand.h"
#include <linux/module.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/ioport.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/bitops.h>
#include <linux/crc32.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/amlogic/securitykey.h>

/* #define KEYSIZE (CONFIG_KEYSIZE - (sizeof(uint))) */

static struct amlnand_chip *aml_chip_key;

int aml_nand_update_key(struct amlnand_chip *aml_chip, char *key_ptr)
{
	int ret = 0;
	int malloc_flag = 0;
	char *key_buf = NULL;

	if (key_buf == NULL) {
		key_buf = kzalloc(CONFIG_KEYSIZE, GFP_KERNEL);
		malloc_flag = 1;
		if (key_buf == NULL)
			return -ENOMEM;
		memset(key_buf, 0, CONFIG_KEYSIZE);
		ret = amlnand_read_info_by_name(aml_chip,
			(unsigned char *)&(aml_chip->nand_key),
			key_buf,
			KEY_INFO_HEAD_MAGIC,
			CONFIG_KEYSIZE);
		if (ret) {
			aml_nand_msg("read key error,%s\n", __func__);
			ret = -EFAULT;
			goto exit;
		}
	} else
		key_buf = key_ptr;

	aml_nand_msg("aml_chip->nand_key : ");
	aml_nand_msg("arg_t %d valid %d,update_flag %d,blkaddr %d,pageaddr %d",
		aml_chip->nand_key.arg_type,
		aml_chip->nand_key.arg_valid,
		aml_chip->nand_key.update_flag,
		aml_chip->nand_key.valid_blk_addr,
		aml_chip->nand_key.valid_page_addr);

	ret = amlnand_save_info_by_name(aml_chip,
		(unsigned char *)&(aml_chip->nand_key),
		key_buf,
		KEY_INFO_HEAD_MAGIC,
		CONFIG_KEYSIZE);
	if (ret < 0)
		aml_nand_msg("aml_nand_update_key : save key info failed");

exit:
	if (malloc_flag && (key_buf)) {
		kfree(key_buf);
		key_buf = NULL;
	}
	return 0;
}

/*
 * This funcion reads the u-boot keyionment variables.
 * The f_pos points directly to the key location.
 */
static int32_t nand_key_read(struct aml_keybox_provider_s *provider,
		uint8_t *buf,
		int len,
		int flags)
{
	struct amlnand_chip *aml_chip = provider->priv;
	struct nand_menson_key *key_ptr = NULL;
	int error = 0;

	if (len > KEYSIZE) {
		aml_nand_msg("key data len too much,%s", __func__);
		return -EFAULT;
	}

	key_ptr = kzalloc(CONFIG_KEYSIZE, GFP_KERNEL);
	if (key_ptr == NULL)
		return -ENOMEM;

	amlnand_get_device(aml_chip, CHIP_READING);
	memset(key_ptr, 0, CONFIG_KEYSIZE);
	error = amlnand_read_info_by_name(aml_chip,
		(unsigned char *)&(aml_chip->nand_key),
		(unsigned char *)key_ptr,
		KEY_INFO_HEAD_MAGIC,
		CONFIG_KEYSIZE);
	if (error) {
		aml_nand_msg("read key error,%s", __func__);
		error = -EFAULT;
		goto exit;
	}

	memcpy(buf, key_ptr->data, len);
exit:
	amlnand_release_device(aml_chip);
	kfree(key_ptr);
	return 0;
}

static int32_t nand_key_write(struct aml_keybox_provider_s *provider,
		uint8_t *buf,
		int len)
{
	struct amlnand_chip *aml_chip = provider->priv;
	struct nand_menson_key *key_ptr = NULL;
	int error = 0;

	if (len > KEYSIZE) {
		aml_nand_msg("key data len too much,%s", __func__);
		return -EFAULT;
	}

	key_ptr = kzalloc(CONFIG_KEYSIZE, GFP_KERNEL);
	if (key_ptr == NULL)
		return -ENOMEM;

	memset(key_ptr, 0, CONFIG_KEYSIZE);
	memcpy(key_ptr->data + 0, buf, len);
	amlnand_get_device(aml_chip, CHIP_WRITING);

	error = amlnand_save_info_by_name(aml_chip,
		(unsigned char *) &(aml_chip->nand_key),
		(unsigned char *)key_ptr,
		KEY_INFO_HEAD_MAGIC,
		CONFIG_KEYSIZE);
	if (error) {
		aml_nand_msg("save key error,%s", __func__);
		error = -EFAULT;
		goto exit;
	}
exit:
	amlnand_release_device(aml_chip);
	kfree(key_ptr);
	return error;
}

static struct aml_keybox_provider_s nand_provider = {
	.name = "nand_key",
	.read = nand_key_read,
	.write = nand_key_write,
};

int aml_key_init(struct amlnand_chip *aml_chip)
{
	int ret = 0;
	struct nand_menson_key *key_ptr = NULL;
	struct aml_keybox_provider_s *provider;

	key_ptr = aml_nand_malloc(CONFIG_KEYSIZE);
	if (key_ptr == NULL) {
		aml_nand_msg("nand malloc for key_ptr failed");
		ret = -1;
		goto exit_error0;
	}
	memset(key_ptr, 0x0, CONFIG_KEYSIZE);
	aml_nand_dbg("nand key: nand_key_probe. ");

	ret = amlnand_info_init(aml_chip,
		(unsigned char *)&(aml_chip->nand_key),
		(unsigned char *)key_ptr,
		KEY_INFO_HEAD_MAGIC,
		CONFIG_KEYSIZE);
	if (ret < 0)
		aml_nand_msg("invalid nand key\n");

	aml_chip_key = aml_chip;
	nand_provider.priv = aml_chip_key;

	provider = aml_keybox_provider_get(nand_provider.name);
	if (provider)
		return ret;

	ret = aml_keybox_provider_register(&nand_provider);
	if (ret)
		BUG();

exit_error0:
	if (key_ptr) {
		aml_nand_free(key_ptr);
		key_ptr = NULL;
	}
	return ret;
}



