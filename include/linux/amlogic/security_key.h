#ifndef _AML_SECURITY_KEY_H_
#define _AML_SECURITY_KEY_H_

/* internal return value*/
#define RET_OK		0
#define RET_EFAIL	1		/*not found*/
#define RET_EINVAL	2	/*name length*/
#define RET_EMEM	3	/*no enough memory*/

/* keyattr: 0: normal, 1: secure*/
int32_t secure_storage_write(uint8_t *keyname, uint8_t *keybuf,
			uint32_t keylen, uint32_t keyattr);
int32_t secure_storage_read(uint8_t *keyname, uint8_t *keybuf,
			uint32_t keylen, uint32_t *readlen);
int32_t secure_storage_verify(uint8_t *keyname, uint8_t *hashbuf);
int32_t secure_storage_query(uint8_t *keyname, uint32_t *retval);
int32_t secure_storage_tell(uint8_t *keyname, uint32_t *retval);
int32_t secure_storage_status(uint8_t *keyname, uint32_t *retval);
void *secure_storage_getbuffer(uint32_t *size);
int32_t secure_storage_set_enctype(uint32_t type);
int32_t secure_storage_get_enctype(void);
int32_t secure_storage_version(void);
#endif
