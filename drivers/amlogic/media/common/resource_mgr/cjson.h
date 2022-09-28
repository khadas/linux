/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _CJSON_H
#define _CJSON_H

/* cjson Types: */
#define cjson_false 0
#define cjson_true 1
#define cjson_null 2
#define cjson_number 3
#define cjson_string 4
#define cjson_array 5
#define cjson_object 6

#define cjson_isreference 256
#define cjson_stringisconst 512

/* The cjson structure: */
struct cjson {
	struct cjson *next, *prev;	/* next/prev allow you to walk array/object chains*/
	struct cjson *child;
	/* item will have a child pointer pointing to a chain of the items in the array/object. */

	int type;					/* The type of the item, as above. */

	char *valuestring;			/* The item's string, if type==cjson_string */
	int valueint;				/* The item's number, if type==cjson_number */
	double valuedouble;			/* The item's number, if type==cjson_number */

	char *string;				/* The item's name string*/
	char *pure_json;	/*chrome add it at 2020-01-03 17:39.*/
};

struct cjson *cjson_parse(const char *value);
void   cjson_delete(struct cjson *c);
struct cjson *cjson_getobjectitem(struct cjson *object, const char *string);

#endif
