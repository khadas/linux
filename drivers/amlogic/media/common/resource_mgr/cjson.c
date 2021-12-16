// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/slab.h>
#include "cjson.h"

static const char *ep;

const char *cjson_geterrorptr(void)
{
	return ep;
}

unsigned long mypow(unsigned long num, unsigned long n)
{
	unsigned long value = 1;
	int i = 1;

	if (n == 0) {
		value = 1;
	} else {
		while (i++ <= n)
			value *= num;
	}
	return value;
}

int  mytolower(int c)
{
	if (c >= 'A' && c <= 'Z')
		return c + 32;
	else
		return c;
}

static int cjson_strcasecmp(const char *s1, const char *s2)
{
	if (!s1)
		return (s1 == s2) ? 0 : 1;
	if (!s2)
		return 1;
	for (; mytolower(*s1) == mytolower(*s2); ++s1, ++s2)
		if (*s1 == 0)
			return 0;
	return mytolower(*(const unsigned char *)s1) - mytolower(*(const unsigned char *)s2);
}

void *cjson_malloc(size_t sz)
{
	void *alloc = NULL;

	alloc = kzalloc(sz, GFP_KERNEL);
	return alloc;
}

void cjson_free(void *ptr)
{
	kfree(ptr);
	ptr = NULL;
}

/* Internal constructor. */
static struct cjson *cjson_new_item(void)
{
	struct cjson *node = (struct cjson *)cjson_malloc(sizeof(struct cjson));

	if (node)
		memset(node, 0, sizeof(struct cjson));
	return node;
}

/* Delete a cjson structure. */
void cjson_delete(struct cjson *c)
{
	struct cjson *next;

	if (c && c->pure_json)
		cjson_free(c->pure_json);

	while (c) {
		next =  c->next;
		if (!(c->type & cjson_isreference) && c->child)
			cjson_delete(c->child);
		if (!(c->type & cjson_isreference) && c->valuestring)
			cjson_free(c->valuestring);
		if (!(c->type & cjson_stringisconst) && c->string)
			cjson_free(c->string);
		cjson_free(c);
		c  = next;
	}
}

/* Parse the input text to generate a number, and populate the result into item. */
static const char *parse_number(struct cjson *item, const char *num)
{
	unsigned long n = 0, sign = 1, scale = 0;
	int subscale = 0, signsubscale = 1;

	if (*num == '-') {
		sign = -1;
		num++;
	}
	if (*num == '0')
		num++;
	if (*num >= '1' && *num <= '9') {
		do {
			n = (n * 10) + (*num++ - '0');
		} while (*num >= '0' && *num <= '9');
	}

	if (*num == '.' && num[1] >= '0' && num[1] <= '9') {
		num++;
		do {
			n = (n * 10) + (*num++ - '0');
			scale--;
		} while (*num >= '0' && *num <= '9');
	}

	if (*num == 'e' || *num == 'E') {
		num++;
		if (*num == '+') {
			num++;
		} else if (*num == '-') {
			signsubscale = -1;
			num++;
		}

		while (*num >= '0' && *num <= '9')
			subscale = (subscale * 10) + (*num++ - '0');
	}

	n = sign * n * mypow(10, (scale + subscale * signsubscale));

	//item->valuedouble = (double)n;
	item->valueint = (int)n;
	item->type = cjson_number;

	return num;
}

static unsigned int parse_hex4(const char *str)
{
	unsigned int h = 0;

	if (*str >= '0' && *str <= '9')
		h += (*str) - '0';
	else if (*str >= 'A' && *str <= 'F')
		h += 10 + (*str) - 'A';
	else if (*str >= 'a' && *str <= 'f')
		h += 10 + (*str) - 'a';
	else
		return 0;

	h = h << 4;
	str++;
	if (*str >= '0' && *str <= '9')
		h += (*str) - '0';
	else if (*str >= 'A' && *str <= 'F')
		h += 10 + (*str) - 'A';
	else if (*str >= 'a' && *str <= 'f')
		h += 10 + (*str) - 'a';
	else
		return 0;

	h = h <<  4;
	str++;
	if (*str >= '0' && *str <= '9')
		h += (*str) - '0';
	else if (*str >= 'A' && *str <= 'F')
		h += 10 + (*str) - 'A';
	else if (*str >= 'a' && *str <= 'f')
		h += 10 + (*str) - 'a';
	else
		return 0;

	h = h << 4;
	str++;
	if (*str >= '0' && *str <= '9')
		h += (*str) - '0';
	else if (*str >= 'A' && *str <= 'F')
		h += 10 + (*str) - 'A';
	else if (*str >= 'a' && *str <= 'f')
		h += 10 + (*str) - 'a';
	else
		return 0;

	return h;
}

/* Parse the input text into an unescaped cstring, and populate item. */
static const unsigned char firstByteMark[7] = { 0x00, 0x00, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC };
static const char *parse_string(struct cjson *item, const char *str)
{
	const char *ptr = str +  1;
	char *ptr2;
	char *out;
	int len = 0;
	unsigned int uc, uc2, tmp;

	if (*str != '\"') {
		ep = str;
		return 0;
	}

	while (*ptr != '\"' && *ptr && ++len)
		if (*ptr++ == '\\')
			ptr++;

	out = (char *)cjson_malloc(len + 1);
	if (!out)
		return 0;

	ptr = str + 1;
	ptr2 = out;
	while (*ptr != '\"' && *ptr) {
		if (*ptr != '\\') {
			*ptr2++ = *ptr++;
		} else {
			ptr++;
			switch (*ptr) {
			case 'b':
				*ptr2++ = '\b';
				break;
			case 'f':
				*ptr2++ = '\f';
				break;
			case 'n':
				*ptr2++ = '\n';
				break;
			case 'r':
				*ptr2++ = '\r';
				break;
			case 't':
				*ptr2++ = '\t';
				break;
			case 'u':
				uc = parse_hex4(ptr +  1);
				ptr += 4;
				if ((uc >= 0xDC00 && uc <= 0xDFFF) || uc == 0)
					break;

				if (uc >= 0xD800 && uc <= 0xDBFF) {
					if (ptr[1] != '\\' || ptr[2] != 'u')
						break;
					uc2 = parse_hex4(ptr + 3);
					ptr += 6;
					if (uc2 < 0xDC00 || uc2 > 0xDFFF)
						break;
					uc = 0x10000 + (((uc & 0x3FF) << 10) | (uc2 & 0x3FF));
				}

				len = 4;
				if (uc < 0x80)
					len = 1;
				else if (uc < 0x800)
					len = 2;
				else if (uc < 0x10000)
					len = 3;
				ptr2 += len;

				switch (len) {
				case 4:
				case 3:
				case 2:
					*--ptr2 = ((uc | 0x80) & 0xBF);
					tmp = uc >> 6;
					uc = tmp;
					break;
				case 1:
					*--ptr2 = (uc | firstByteMark[len]);
					break;
				}
				ptr2 += len;
				break;
			default:
				*ptr2++ = *ptr;
				break;
			}
			ptr++;
		}
	}
	*ptr2 = 0;
	if (*ptr == '\"')
		ptr++;
	item->valuestring = out;
	item->type = cjson_string;

	return ptr;
}

static const char *parse_value(struct cjson *item, const char *value);
static const char *parse_array(struct cjson *item, const char *value);
static const char *parse_object(struct cjson *item, const char *value);

/* Utility to jump whitespace and cr/lf */
static const char *skip(const char *in)
{
	while (in && *in && (unsigned char)*in <= 32)
		in++;
	return in;
}

static const char *get_pure_json_str(const char *in)
{
	int i = 0;
	int j = 0;
	char cur_char = 0;
	char pre_char = 0;
	char is_in_str = 0;
	const char *str;
	int len = 0;

	if (!in)
		return in;
	str = in;
	while (*(str + i) != 0) {
		len++;
		i++;
	}

	char *json_pure = (char *)cjson_malloc(len);

	if (!json_pure)
		return 0;

	for (i = 0, j = 0; i < len; i++) {
		cur_char = *(in + i);

		if (cur_char == 0) {
			pr_debug("skip\n");
		} else if (cur_char == ' ' || cur_char == '\n' || cur_char == '\r') {
			if (is_in_str) {
				//don't remove space in strings
				*(json_pure + j++) = cur_char;
			}
		} else {
			*(json_pure + j++) = cur_char;
			if (cur_char == '"')
				if (pre_char != '\\')
					is_in_str = !is_in_str;
		}

	pre_char = cur_char;
	}

	return json_pure;
}

/* Parse an object - create a new root, and populate. */
struct cjson *cjson_parsewithopts(const char *value, const char **return_parse_end,
									int require_null_terminated)
{
	const char *end = 0;
	char *json_pure = NULL;
	struct cjson *c = cjson_new_item();

	ep = 0;
	if (!c)
		return 0;

	json_pure = (char *)get_pure_json_str(value);
	c->pure_json = json_pure;
	end = parse_value(c, skip(json_pure));
	if (!end)	{
		cjson_delete(c);
		return 0;
	}
	/* parse failure. ep is set.*/

	/*skip and then check for a null terminator*/
	if (require_null_terminated) {
		end = skip(end);
		if (*end) {
			cjson_delete(c);
			ep = end;
			return 0;
		}
	}
	if (return_parse_end)
		*return_parse_end = end;
	return c;
}

/* Default options for cjson_parse */
struct cjson *cjson_parse(const char *value)
{
	return cjson_parsewithopts(value, 0, 0);
}

/* Render a cjson item/entity/structure to text. */

/* Parser core - when encountering text, process appropriately. */
static const char *parse_value(struct cjson *item, const char *value)
{
	if (!value)
		return 0;

	if (!strncmp(value, "null", 4)) {
		item->type = cjson_null;
		return value + 4;
	}

	if (!strncmp(value, "false", 5)) {
		item->type = cjson_false;
		return value + 5;
	}

	if (!strncmp(value, "true", 4)) {
		item->type = cjson_true;
		item->valueint = 1;
		return value + 4;
	}

	if (*value == '\"')
		return parse_string(item, value);

	if (*value == '-' || (*value >= '0' && *value <= '9'))
		return parse_number(item, value);

	if (*value == '[')
		return parse_array(item, value);

	if (*value == '{')
		return parse_object(item, value);

	ep = value;
	return 0;
}

/* Build an array from input text. */
static const char *parse_array(struct cjson *item, const char *value)
{
	struct cjson *child;

	if (*value != '[') {
		ep = value;
		return 0;
	}

	item->type = cjson_array;
	value = skip(value + 1);
	if (*value == ']')
		return value + 1;

	child = cjson_new_item();
	item->child = child;
	if (!item->child)
		return 0;

	value = skip(parse_value(child, skip(value)));
	// skip any spacing, get the value.
	if (!value)
		return 0;

	while (*value == ',') {
		struct cjson *new_item;

		new_item = cjson_new_item();
		if (!new_item)
			return 0;
		child->next = new_item;
		new_item->prev = child;
		child = new_item;
		value = skip(parse_value(child, skip(value + 1)));
		if (!value)
			return 0;
		//JSON array
	}

	if (*value == ']')
		return value + 1;
	ep = value;

	return 0;
}

/* Build an object from the text. */
static const char *parse_object(struct cjson *item, const char *value)
{
	struct cjson *child;

	if (*value != '{') {
		ep = value;
		return 0;
	}

	item->type = cjson_object;
	value = skip(value + 1);
	if (*value == '}')
		return value + 1;

	child = cjson_new_item();
	item->child = child;
	if (!item->child)
		return 0;

	value = skip(parse_string(child, skip(value)));

	if (!value)
		return 0;

	child->string = child->valuestring;
	child->valuestring = 0;
	if (*value != ':') {
		ep = value;
		return 0;
	}

	value = skip(parse_value(child, skip(value + 1)));
	if (!value)
		return 0;

	while (*value == ',') {
		struct cjson *new_item;

		new_item = cjson_new_item();
		if (!new_item)
			return 0;
		child->next = new_item;
		new_item->prev = child;
		child = new_item;
		value = skip(parse_string(child, skip(value + 1)));
		if (!value)
			return 0;
		child->string = child->valuestring;
		child->valuestring = 0;
		if (*value != ':') {
			ep = value;
			return 0;
		}
		value = skip(parse_value(child, skip(value + 1)));
		if (!value)
			return 0;
	}

	if (*value == '}')
		return value + 1;
	ep = value;

	return 0;
}

/* Get Array size/item / object item. */
int cjson_getarraysize(struct cjson *array)
{
	struct cjson *c = array->child;
	int i = 0;

	while (c)
		i++, c  = c->next;
	return i;
}

struct cjson *cjson_getarrayitem(struct cjson *array, int item)
{
	struct cjson *c = array->child;

	while (c && item > 0)
		item--, c = c->next;
	return c;
}

struct cjson *cjson_getobjectitem(struct cjson *object, const char *string)
{
	struct cjson *c = object->child;

	while (c && cjson_strcasecmp(c->string, string))
		c = c->next;
	return c;
}
