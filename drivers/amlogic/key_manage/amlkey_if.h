/*
 * \file        amlkey_if.h
 * \brief       APIs of secure key for users
 *
 * \version     1.0.0
 * \date        15/07/10
 * \author      Sam.Wu <yihui.wu@amlgic.com>
 *
 * Copyright (c) 2015 Amlogic. All Rights Reserved.
 *
 */
#ifndef __AMLKEY_IF_H__
#define __AMLKEY_IF_H__

#define AMLKEY_NAME_LEN_MAX     (80)

/*
init
*/
int32_t amlkey_init(uint8_t *seed, uint32_t len);
/*
query if the key already programmed
exsit 1, non 0
*/
int32_t amlkey_isexsit(const uint8_t *name);
/*
query if the prgrammed key is secure
secure 1, non 0
*/
int32_t amlkey_issecure(const uint8_t *name);
/*
 * query if the prgrammed key is encrypt
 * return encrypt 1, non 0;
 */
int32_t amlkey_isencrypt(const uint8_t *name);
/*
actual bytes of key value
*/
ssize_t amlkey_size(const uint8_t *name);
/*
read non-secure key in bytes, return byets readback actully.
*/
ssize_t amlkey_read(const uint8_t *name, uint8_t *buffer, uint32_t len);

/*
write key with attr in bytes , return bytes readback actully
	attr: bit0, secure/non-secure
		  bit8, encrypt/non-encrypt
*/
ssize_t amlkey_write(const uint8_t *name,
		uint8_t *buffer,
		uint32_t len,
		uint32_t attr);

/*
get the hash value of programmed secure key | 32bytes length, sha256
*/
int32_t amlkey_hash_4_secure(const uint8_t *name, uint8_t *hash);

extern int32_t nand_key_read(uint8_t *buf,
			uint32_t len, uint32_t *actual_lenth);

extern int32_t nand_key_write(uint8_t *buf,
			uint32_t len, uint32_t *actual_lenth);

extern int32_t emmc_key_read(uint8_t *buf,
			uint32_t len, uint32_t *actual_lenth);

extern int32_t emmc_key_write(uint8_t *buf,
			uint32_t len, uint32_t *actual_lenth);

#endif

