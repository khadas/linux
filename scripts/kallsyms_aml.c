/* Generate assembler source containing symbol information
 *
 * Copyright 2002       by Kai Germaschewski
 *
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 *
 * Usage: nm -n vmlinux | scripts/kallsyms [--all-symbols] > symbols.S
 *
 *      Table compression uses all the unused char codes on the symbols and
 *  maps these to the most used substrings (tokens). For instance, it might
 *  map char code 0xF7 to represent "write_" and then in every symbol where
 *  "write_" appears it can be replaced by 0xF7, saving 5 bytes.
 *      The used codes themselves are also placed in the table so that the
 *  decompresion can work without "special cases".
 *      Applied to kernel symbols, this usually produces a compression ratio
 *  of about 50%.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <time.h>

/*----------------- import code for rb_tree ------------------*/
#define	RB_RED		0
#define	RB_BLACK	1

struct rb_node {
	unsigned long  __rb_parent_color;
	struct rb_node *rb_right;
	struct rb_node *rb_left;
};
    /* The alignment might seem pointless, but allegedly CRIS needs it */

struct rb_root {
	struct rb_node *rb_node;
};

#undef offsetof
#ifdef __compiler_offsetof
#define offsetof(TYPE, MEMBER)	__compiler_offsetof(TYPE, MEMBER)
#else
#define offsetof(TYPE, MEMBER)	((size_t)&((TYPE *)0)->MEMBER)
#endif

#define container_of(ptr, type, member) ({			\
	const typeof( ((type *)0)->member ) *__mptr = (ptr);	\
	(type *)( (char *)__mptr - offsetof(type,member) );})

#define __rb_color(pc)     ((pc) & 1)
#define __rb_is_black(pc)  __rb_color(pc)
#define __rb_is_red(pc)    (!__rb_color(pc))
#define rb_color(rb)       __rb_color((rb)->__rb_parent_color)
#define rb_is_red(rb)      __rb_is_red((rb)->__rb_parent_color)
#define rb_is_black(rb)    __rb_is_black((rb)->__rb_parent_color)

#define rb_parent(r)   ((struct rb_node *)((r)->__rb_parent_color & ~3))

#define	rb_entry(ptr, type, member) container_of(ptr, type, member)

/* 'empty' nodes are nodes that are known not to be inserted in an rbtree */
#define RB_EMPTY_NODE(node)  \
	((node)->__rb_parent_color == (unsigned long)(node))
static void dummy_rotate(struct rb_node *old, struct rb_node *new)
{

}

static inline void rb_set_parent_color(struct rb_node *rb,
				       struct rb_node *p, int color)
{
	rb->__rb_parent_color = (unsigned long)p | color;
}

static inline void
__rb_change_child(struct rb_node *old, struct rb_node *new,
		  struct rb_node *parent, struct rb_root *root)
{
	if (parent) {
		if (parent->rb_left == old)
			parent->rb_left = new;
		else
			parent->rb_right = new;
	} else
		root->rb_node = new;
}

static inline void
__rb_rotate_set_parents(struct rb_node *old, struct rb_node *new,
			struct rb_root *root, int color)
{
	struct rb_node *parent = rb_parent(old);

	new->__rb_parent_color = old->__rb_parent_color;
	rb_set_parent_color(old, new, color);
	__rb_change_child(old, new, parent, root);
}


static inline void rb_link_node(struct rb_node *node, struct rb_node *parent,
				struct rb_node **rb_link)
{
	node->__rb_parent_color = (unsigned long)parent;
	node->rb_left = node->rb_right = NULL;

	*rb_link = node;
}

static inline struct rb_node *rb_red_parent(struct rb_node *red)
{
	return (struct rb_node *)red->__rb_parent_color;
}

static void __rb_insert(struct rb_node *node,  struct rb_root *root,
			void (*augment_rotate)(struct rb_node *old,
					       struct rb_node *new))
{
	struct rb_node *parent = rb_red_parent(node), *gparent, *tmp;

	while (1) {
		if (!parent) {
			rb_set_parent_color(node, NULL, RB_BLACK);
			break;
		} else if (rb_is_black(parent))
			break;

		gparent = rb_red_parent(parent);

		tmp = gparent->rb_right;
		if (parent != tmp) {	/* parent == gparent->rb_left */
			if (tmp && rb_is_red(tmp)) {
				rb_set_parent_color(tmp, gparent, RB_BLACK);
				rb_set_parent_color(parent, gparent, RB_BLACK);
				node = gparent;
				parent = rb_parent(node);
				rb_set_parent_color(node, parent, RB_RED);
				continue;
			}

			tmp = parent->rb_right;
			if (node == tmp) {
				tmp = node->rb_left;
				parent->rb_right = tmp;
				node->rb_left = parent;
				if (tmp)
					rb_set_parent_color(tmp, parent,
							    RB_BLACK);
				rb_set_parent_color(parent, node, RB_RED);
				augment_rotate(parent, node);
				parent = node;
				tmp = node->rb_right;
			}

			gparent->rb_left = tmp; /* == parent->rb_right */
			parent->rb_right = gparent;
			if (tmp)
				rb_set_parent_color(tmp, gparent, RB_BLACK);
			__rb_rotate_set_parents(gparent, parent, root, RB_RED);
			augment_rotate(gparent, parent);
			break;
		} else {
			tmp = gparent->rb_left;
			if (tmp && rb_is_red(tmp)) {
				rb_set_parent_color(tmp, gparent, RB_BLACK);
				rb_set_parent_color(parent, gparent, RB_BLACK);
				node = gparent;
				parent = rb_parent(node);
				rb_set_parent_color(node, parent, RB_RED);
				continue;
			}

			tmp = parent->rb_left;
			if (node == tmp) {
				tmp = node->rb_right;
				parent->rb_left = tmp;
				node->rb_right = parent;
				if (tmp)
					rb_set_parent_color(tmp, parent,
							    RB_BLACK);
				rb_set_parent_color(parent, node, RB_RED);
				augment_rotate(parent, node);
				parent = node;
				tmp = node->rb_left;
			}

			/* Case 3 - left rotate at gparent */
			gparent->rb_right = tmp; /* == parent->rb_left */
			parent->rb_left = gparent;
			if (tmp)
				rb_set_parent_color(tmp, gparent, RB_BLACK);
			__rb_rotate_set_parents(gparent, parent, root, RB_RED);
			augment_rotate(gparent, parent);
			break;
		}
	}
}

static void rb_insert_color(struct rb_node *node, struct rb_root *root)
{
	__rb_insert(node, root, dummy_rotate);
}

/*
 * This function returns the first node (in sort order) of the tree.
 */
static struct rb_node *rb_first(const struct rb_root *root)
{
	struct rb_node	*n;

	n = root->rb_node;
	if (!n)
		return NULL;
	while (n->rb_left)
		n = n->rb_left;
	return n;
}

static struct rb_node *rb_next(const struct rb_node *node)
{
	struct rb_node *parent;

	if (RB_EMPTY_NODE(node))
		return NULL;

	if (node->rb_right) {
		node = node->rb_right;
		while (node->rb_left)
			node = node->rb_left;
		return (struct rb_node *)node;
	}

	while ((parent = rb_parent(node)) && node == parent->rb_right)
		node = parent;

	return parent;
}

/*--------------------- end rb_tree porting ---------------------*/

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(arr[0]))
#endif

#define KSYM_NAME_LEN		128

struct sym_entry {
	unsigned long long addr;
	unsigned int len;
	unsigned int start_pos;
	unsigned char *sym;
	unsigned int percpu_absolute;
};

struct addr_range {
	const char *start_sym, *end_sym;
	unsigned long long start, end;
};

#define SPLIT_CHARS	2
#define IDX_TYPE	(SPLIT_CHARS * 4)
struct key_word {
	struct key_word *next;
	char word[KSYM_NAME_LEN];
	unsigned int  total_cnt;
	unsigned short word_len;
	unsigned short idx_type[IDX_TYPE];
};

#define KEY_WORD_BASE		0x8000
#define KEY_WORD_HASH_SIZE	4096

struct final_word {
	unsigned char *str;
	int            len;
	int            cnt;
	int            node_cnt;
	struct rb_root tree;
};

struct merge_word {
	struct rb_node entry;
	unsigned int code;
	unsigned int count;
};

struct merge_tree {
	struct rb_node entry;
	unsigned int key;
	unsigned int new;
};

struct short_base {
	unsigned long   base;
	unsigned int    start_idx;
	unsigned int    syms;
	unsigned short *table;
};
static int short_base_cnt;

struct rb_root merge_trees;

static struct final_word final_words[KEY_WORD_BASE] = {};
static int finnal_sym_len;
static int final_words_cnt = 8;
static int max_sym_words;

static unsigned long long _text;
static struct addr_range text_ranges[] = {
	{ "_stext",     "_etext"     },
	{ "_sinittext", "_einittext" },
	{ "_stext_l1",  "_etext_l1"  },	/* Blackfin on-chip L1 inst SRAM */
	{ "_stext_l2",  "_etext_l2"  },	/* Blackfin on-chip L2 SRAM */
};
#define text_range_text     (&text_ranges[0])
#define text_range_inittext (&text_ranges[1])

static struct addr_range percpu_range = {
	"__per_cpu_start", "__per_cpu_end", -1ULL, 0
};

static struct sym_entry *table;
static unsigned int table_size, table_cnt;
static int all_symbols = 0;
static char symbol_prefix_char = '\0';

static unsigned long input_len;

int token_profit[128 * 128];

#define DEBUG_COMPRESS		0
#if DEBUG_COMPRESS
#define tprintf(format, args...)  printf(format, ##args)
#else
#define tprintf(format, args...)
#endif

/* the table that holds the result of the compression */
struct best_table {
	unsigned char table[128][2];
	unsigned char len[128];
};

struct best_table sym_best = {};
struct best_table word_best = {};
static unsigned long long start_time;

static unsigned long long nanotime(void)
{
	struct timespec t;

	t.tv_sec = t.tv_nsec = 0;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return t.tv_sec * 1000000000LL + t.tv_nsec;
}

static void usage(void)
{
	fprintf(stderr, "Usage: kallsyms [--all-symbols] "
			"[--symbol-prefix=<prefix char>] "
			"[--page-offset=<CONFIG_PAGE_OFFSET>] "
			"[--base-relative] < in.map > out.S\n");
	exit(1);
}

/*
 * This ignores the intensely annoying "mapping symbols" found
 * in ARM ELF files: $a, $t and $d.
 */
static inline int is_arm_mapping_symbol(const char *str)
{
	return str[0] == '$' && strchr("axtd", str[1])
	       && (str[2] == '\0' || str[2] == '.');
}

static int check_symbol_range(const char *sym, unsigned long long addr,
			      struct addr_range *ranges, int entries)
{
	size_t i;
	struct addr_range *ar;

	for (i = 0; i < entries; ++i) {
		ar = &ranges[i];

		if (strcmp(sym, ar->start_sym) == 0) {
			ar->start = addr;
			return 0;
		} else if (strcmp(sym, ar->end_sym) == 0) {
			ar->end = addr;
			return 0;
		}
	}

	return 1;
}

static int read_symbol(FILE *in, struct sym_entry *s)
{
	char str[500];
	char *sym, stype;
	int rc;

	rc = fscanf(in, "%llx %c %499s\n", &s->addr, &stype, str);
	if (rc != 3) {
		if (rc != EOF && fgets(str, 500, in) == NULL)
			fprintf(stderr, "Read error or end of file.\n");
		return -1;
	}
	if (strlen(str) > KSYM_NAME_LEN) {
		fprintf(stderr, "Symbol %s too long for kallsyms (%zu vs %d).\n"
				"Please increase KSYM_NAME_LEN both in kernel and kallsyms.c\n",
			str, strlen(str), KSYM_NAME_LEN);
		return -1;
	}

	sym = str;
	/* skip prefix char */
	if (symbol_prefix_char && str[0] == symbol_prefix_char)
		sym++;

	/* Ignore most absolute/undefined (?) symbols. */
	if (strcmp(sym, "_text") == 0) {
		_text = s->addr;
	} else if (check_symbol_range(sym, s->addr, text_ranges,
				    ARRAY_SIZE(text_ranges)) == 0) {
		/* nothing to do */;
	} else if (toupper(stype) == 'A') {
		/* Keep these useful absolute symbols */
		if (strcmp(sym, "__kernel_syscall_via_break") &&
		    strcmp(sym, "__kernel_syscall_via_epc") &&
		    strcmp(sym, "__kernel_sigtramp") &&
		    strcmp(sym, "__gp"))
			return -1;

	} else if (toupper(stype) == 'U' ||
		 is_arm_mapping_symbol(sym))
		return -1;
	/* exclude also MIPS ELF local symbols ($L123 instead of .L123) */
	else if (str[0] == '$')
		return -1;
	/* exclude debugging symbols */
	else if (stype == 'N')
		return -1;

	/* include the type field in the symbol name, so that it gets
	 * compressed together
	 */
	s->len = strlen(str) + 1;
	s->sym = malloc(s->len + 1);
	if (!s->sym) {
		fprintf(stderr, "kallsyms failure: "
			"unable to allocate required amount of memory\n");
		exit(EXIT_FAILURE);
	}
	strcpy((char *)s->sym + 1, str);
	s->sym[0] = stype;

	s->percpu_absolute = 0;

	/* Record if we've found __per_cpu_start/end. */
	check_symbol_range(sym, s->addr, &percpu_range, 1);

	return 0;
}

static int symbol_in_range(struct sym_entry *s, struct addr_range *ranges,
			   int entries)
{
	size_t i;
	struct addr_range *ar;

	for (i = 0; i < entries; ++i) {
		ar = &ranges[i];

		if (s->addr >= ar->start && s->addr <= ar->end)
			return 1;
	}

	return 0;
}

static int symbol_valid(struct sym_entry *s)
{
	/* Symbols which vary between passes.  Passes 1 and 2 must have
	 * identical symbol lists.  The kallsyms_* symbols below are only added
	 * after pass 1, they would be included in pass 2 when --all-symbols is
	 * specified so exclude them to get a stable symbol list.
	 */
	static char *special_symbols[] = {
		"kallsyms_addresses",
		"kallsyms_offsets",
		"kallsyms_relative_base",
		"kallsyms_num_syms",
		"kallsyms_names",
		"kallsyms_markers",
		"kallsyms_token_table",
		"kallsyms_token_index",
	/* add for word compress */
		"kallsyms_short_base",
		"kallsyms_short_cnt",
		"kallsyms_num_words",
		"kallsyms_words",
		"kallsyms_word_markers",
		"kallsyms_word_table",
		"kallsyms_word_index",

	/* Exclude linker generated symbols which vary between passes */
		"_SDA_BASE_",		/* ppc */
		"_SDA2_BASE_",		/* ppc */
		NULL };

	static char *special_suffixes[] = {
		"_veneer",		/* arm */
		"_from_arm",		/* arm */
		"_from_thumb",		/* arm */
		NULL };

	int i;
	char *sym_name = (char *)s->sym + 1;

	/* skip prefix char */
	if (symbol_prefix_char && *sym_name == symbol_prefix_char)
		sym_name++;


	/* if --all-symbols is not specified, then symbols outside the text
	 * and inittext sections are discarded */
	if (!all_symbols) {
		if (symbol_in_range(s, text_ranges,
				    ARRAY_SIZE(text_ranges)) == 0)
			return 0;
		/* Corner case.  Discard any symbols with the same value as
		 * _etext _einittext; they can move between pass 1 and 2 when
		 * the kallsyms data are added.  If these symbols move then
		 * they may get dropped in pass 2, which breaks the kallsyms
		 * rules.
		 */
		if ((s->addr == text_range_text->end &&
				strcmp(sym_name,
				       text_range_text->end_sym)) ||
		    (s->addr == text_range_inittext->end &&
				strcmp(sym_name,
				       text_range_inittext->end_sym)))
			return 0;
	}

	/* Exclude symbols which vary between passes. */
	for (i = 0; special_symbols[i]; i++)
		if (strcmp(sym_name, special_symbols[i]) == 0)
			return 0;

	for (i = 0; special_suffixes[i]; i++) {
		int l = strlen(sym_name) - strlen(special_suffixes[i]);

		if (l >= 0 && strcmp(sym_name + l, special_suffixes[i]) == 0)
			return 0;
	}

	return 1;
}

static void read_map(FILE *in)
{
	while (!feof(in)) {
		if (table_cnt >= table_size) {
			table_size += 10000;
			table = realloc(table, sizeof(*table) * table_size);
			if (!table) {
				fprintf(stderr, "out of memory\n");
				exit (1);
			}
		}
		if (read_symbol(in, &table[table_cnt]) == 0) {
			table[table_cnt].start_pos = table_cnt;
			table_cnt++;
		}
	}
#if DEBUG_COMPRESS
	tprintf("%s, table_cnt:%d\n", __func__, table_cnt);
#endif
}

static void output_label(char *label)
{
	if (symbol_prefix_char)
		printf(".globl %c%s\n", symbol_prefix_char, label);
	else
		printf(".globl %s\n", label);
	printf("\tALGN\n");
	if (symbol_prefix_char)
		printf("%c%s:\n", symbol_prefix_char, label);
	else
		printf("%s:\n", label);
}

/* uncompress a compressed symbol. When this function is called, the best table
 * might still be compressed itself, so the function needs to be recursive
 */
static int expand_symbol(unsigned char *data, int len, char *result, struct best_table *b)
{
	int c, rlen, total = 0;

	while (len) {
		c = *data;
		/* if the table holds a single char that is the same as the one
		 * we are looking for, then end the search
		 */
		if (b->table[c][0] == c && b->len[c] == 1) {
			*result++ = c;
			total++;
		} else {
			/* if not, recurse and expand */
			rlen = expand_symbol(b->table[c], b->len[c], result, b);
			total += rlen;
			result += rlen;
		}
		data++;
		len--;
	}
	*result = 0;

	return total;
}

static void *zalloc(int size)
{
	void *ret;

	ret = malloc(size);
	if (ret) {
		memset(ret, 0, size);
	} else {
		fprintf(stderr, "alloc memory %d bytes failed\n", size);
		exit(-1);
	}
	return ret;
}

static void process_for_short_base(void)
{
	long long offset;
	unsigned long long base_addr = 0;
	int syms = 0, bc = 0, i;
	struct short_base *base;

	printf("\tALGN\n");
	base = zalloc((short_base_cnt + 1) * sizeof(*base));
	for (i = 0; i < table_cnt; i++) {
		if (!base_addr) {
			base[bc].base = table[i].addr;
			base[bc].start_idx = i;
			base_addr = table[i].addr;
			printf(".Lshort_group%d:\n", bc);
			bc++;
		}
		if (table[i].addr - base_addr < 0x10000) {	/* in 64k range */
			offset = table[i].addr - base_addr;
			printf("\t.short\t%#x\n", (unsigned short)offset);
			syms++;
		} else {	/* overflow */
			base[bc - 1].syms = syms;
			base[bc].base = table[i].addr;
			base[bc].start_idx = i;
			base_addr = table[i].addr;
			syms = 1;
			printf("\n.Lshort_group%d:\n", bc);
			printf("\t.short\t%#x\n", (unsigned short)0);
			bc++;
		}
	}
	base[bc - 1].syms = syms;

	output_label("kallsyms_short_base");
	for (i = 0; i < short_base_cnt; i++) {
		printf("\tPTR\t%#lx\n", base[i].base);
		printf("\t.word\t%#x\n", base[i].start_idx);
		printf("\t.word\t%#x\n", base[i].syms);
		printf("\tPTR\t.Lshort_group%d\n", i);
	}
	output_label("kallsyms_short_cnt");
	printf("\t.word\t%#x\n", short_base_cnt);
}

static void write_src(void)
{
	unsigned int i, k, off;
	unsigned int best_idx[128];
	unsigned int *markers;
	unsigned int sym_size, sym_token_size;
	unsigned int word_size, word_token_size;
	char buf[KSYM_NAME_LEN];

	printf("#include <asm/types.h>\n");
	printf("#if BITS_PER_LONG == 64\n");
	printf("#define PTR .quad\n");
	printf("#define ALGN .align 3\n");
	printf("#else\n");
	printf("#define PTR .long\n");
	printf("#define ALGN .align 2\n");
	printf("#endif\n");

	printf("\t.section .rodata, \"a\"\n");

	/* Provide proper symbols relocatability by their relativeness
	 * to a fixed anchor point in the runtime image, either '_text'
	 * for absolute address tables, in which case the linker will
	 * emit the final addresses at build time. Otherwise, use the
	 * offset relative to the lowest value encountered of all relative
	 * symbols, and emit non-relocatable fixed offsets that will be fixed
	 * up at runtime.
	 *
	 * The symbol names cannot be used to construct normal symbol
	 * references as the list of symbols contains symbols that are
	 * declared static and are private to their .o files.  This prevents
	 * .tmp_kallsyms.o or any other object from referencing them.
	 */
	process_for_short_base();
	printf("\n");

	output_label("kallsyms_num_syms");
	printf("\t.word\t%d\n", table_cnt);
	printf("\n");

	/* table of offset markers, that give the offset in the compressed stream
	 * every 256 symbols
	 */
	markers = malloc(sizeof(unsigned int) * ((table_cnt + 255) / 256));
	if (!markers) {
		fprintf(stderr, "kallsyms failure: "
			"unable to allocate required memory\n");
		exit(EXIT_FAILURE);
	}

	output_label("kallsyms_names");
	off = 0;
	for (i = 0; i < table_cnt; i++) {
		if ((i & 0xFF) == 0)
			markers[i >> 8] = off;

		printf("\t.byte 0x%02x", table[i].len);
		for (k = 0; k < table[i].len; k++)
			printf(", 0x%02x", table[i].sym[k]);
		printf("\n");

		off += table[i].len + 1;
	}
	printf("\n");
	sym_size = off;

	output_label("kallsyms_markers");
	for (i = 0; i < ((table_cnt + 255) >> 8); i++)
		printf("\t.word\t%d\n", markers[i]);
	printf("\n");

	free(markers);

	output_label("kallsyms_token_table");
	off = 0;
	for (i = 0; i < 128; i++) {
		best_idx[i] = off;
		expand_symbol(sym_best.table[i], sym_best.len[i], buf, &sym_best);
		printf("\t.asciz\t\"%s\"\n", buf);
		off += strlen(buf) + 1;
	}
	printf("\n");
	sym_token_size = off;

	output_label("kallsyms_token_index");
	for (i = 0; i < 128; i++)
		printf("\t.short\t%d\n", best_idx[i]);
	printf("\n");

	/*---- output for word compress ----*/
	output_label("kallsyms_num_words");
	printf("\t.word\t%d\n", final_words_cnt);
	printf("\n");

	markers = malloc(sizeof(unsigned int) * ((final_words_cnt + 255) / 256));
	if (!markers) {
		fprintf(stderr, "kallsyms failure: "
			"unable to allocate required memory\n");
		exit(EXIT_FAILURE);
	}
	output_label("kallsyms_words");
	off = 0;
	for (i = 0; i < final_words_cnt; i++) {
		if ((i & 0xFF) == 0)
			markers[i >> 8] = off;

		printf("\t.byte 0x%02x", final_words[i].len);
		for (k = 0; k < final_words[i].len; k++)
			printf(", 0x%02x", final_words[i].str[k]);
		printf("\n");

		off += final_words[i].len + 1;
	}
	printf("\n");
	word_size = off;

	output_label("kallsyms_word_markers");
	for (i = 0; i < ((final_words_cnt + 255) >> 8); i++)
		printf("\t.word\t%d\n", markers[i]);
	printf("\n");

	free(markers);

	output_label("kallsyms_word_table");
	off = 0;
	for (i = 0; i < 128; i++) {
		best_idx[i] = off;
		expand_symbol(word_best.table[i], word_best.len[i], buf, &word_best);
		printf("\t.asciz\t\"%s\"\n", buf);
		off += strlen(buf) + 1;
	}
	printf("\n");
	word_token_size = off;

	output_label("kallsyms_word_index");
	for (i = 0; i < 128; i++)
		printf("\t.short\t%d\n", best_idx[i]);
	printf("\n");

	/* print something for statistics */
	printf("/* ------ kallsym statistics ------\n");
	printf(" * kallsyms_num_syms:%d, offset size:%ld\n",
		table_cnt, table_cnt * sizeof(int));
	printf(" * kallsyms_names:%d, marker size:%ld\n",
		sym_size, (table_cnt + 255) / 256 * sizeof(int));
	printf(" * kallsyms_token size:%d, token table size:%ld\n",
		sym_token_size, 128 * sizeof(short));
	printf(" * kallsyms_word_cnt:%d\n", final_words_cnt);
	printf(" * kallsyms_words:%d, marker size:%ld\n",
		word_size, (final_words_cnt + 255) / 256 * sizeof(int));
	printf(" * kallsyms_token size:%d, token table size:%ld\n",
		word_token_size, 128 * sizeof(short));
	printf(" * input size:%ld, final cost size:%ld, compress time:%lld us\n",
		input_len, sym_size + (table_cnt + 255) / 256 * sizeof(int) +
		sym_token_size + 128 * sizeof(short) + word_size +
		(final_words_cnt + 255) / 256 * sizeof(int) + word_token_size +
		128 * sizeof(short), start_time);
	printf(" */\n");
}


static unsigned int hash_str(unsigned char *str, int len)
{
	unsigned int hash = 0, i;

	for (i = 0; i < len; i++) {
		/* simple prim number for hash */
		hash = hash * 131 + str[i];
	}

	return hash & (KEY_WORD_HASH_SIZE - 1);
}

/*--------------------  process for key workd  ------------------*/
static struct key_word *key_words[KEY_WORD_HASH_SIZE] = {};
static unsigned int hash_cnt[KEY_WORD_HASH_SIZE] = {};
static unsigned int total_word;

static int is_split_char(char c)
{
	if (c == '_' || c == '.')
		return 1;
	return 0;
}

static struct key_word *find_exist_key_word(unsigned int hash,
					    unsigned char *str,
					    int len)
{
	struct key_word *k;

	if (key_words[hash] == NULL)
		return NULL;

	k = key_words[hash];
	while (k) {
		if ((k->word_len == len) &&
		    (memcmp(k->word, str, len) == 0)) {
			return k;
		}
		k = k->next;
	}
	return NULL;
}

static void remember_word(unsigned char *str, struct sym_entry *s,
			  int len)
{
	unsigned int hash, i;
	struct key_word *k;

	if (len < 2)	/* no need remember too small string */
		return;
	hash = hash_str(str, len);
	k = find_exist_key_word(hash, str, len);
	if (!k) {
		k = zalloc(sizeof(*k));
		memcpy(k->word, str, len);
		k->word_len  = len;
		k->total_cnt = 1;
		for (i = 0; i < IDX_TYPE; i++)
			k->idx_type[i] = 0xffff;

		if (key_words[hash])	/* insert new key word */
			k->next = key_words[hash];
		key_words[hash] = k;
		hash_cnt[hash]++;
		total_word++;
	} else {
		k->total_cnt++;
	}
}

static void split_symbol(struct sym_entry *s)
{
	unsigned char *p1, *p2 = NULL;
	int i = 0, sublen = 0, splited = 0;

	p1 = s->sym;
	for (i = 0; i < s->len; i++) {
		if (is_split_char(s->sym[i])) {
			p2 = p1 + sublen;
			remember_word(p1, s, sublen);
			do {	/* ignore contigous split char */
				i++;
				p2++;
			} while (is_split_char(s->sym[i]));
			p1 = p2;
			sublen = 0;
			splited++;
		}
		sublen++;
	}
	if (!p2) { /* none of split sub string */
		remember_word(p1, s, s->len);
	}
	if (splited) {	/* end of string */
		remember_word(p1, s, sublen);
	}
	if (splited > max_sym_words)
		max_sym_words = splited;
}

#if DEBUG_COMPRESS
static void show_keyword(struct key_word *k, int hash)
{
	int i;

	tprintf("k:%p, hash:%4d, len:%d, total:%d, ",
		k, hash, k->word_len, k->total_cnt);
	for (i = 0; i < k->word_len; i++)
		tprintf("%c", k->word[i]);

	tprintf("\n");
}

static void show_all_key_word(struct key_word *words[])
{
	int i;
	struct key_word *k;
	int total = 0;

	for (i = 0; i < KEY_WORD_HASH_SIZE; i++) {
		tprintf("hash:%4d, cnt:%4d\n", i, hash_cnt[i]);
		k = words[i];
		while (k) {
			show_keyword(k, i);
			k = k->next;
			total++;
		}
	}
	tprintf("%s, total word:%d\n", __func__, total);
}
#endif

static void remove_valueless_key_word(void)
{
	int i;
	struct key_word *k, *p, *n;

	for (i = 0; i < KEY_WORD_HASH_SIZE; i++) {
		p = key_words[i];
		k = p;
		while (k) {
			n = k->next;
			if ((k->total_cnt < 2) || (k->word_len <= 2)) {
				if (k == key_words[i])	/* remove first node */
					key_words[i] = n;
				else
					p->next = n;
			} else {
				p = k;
			}
			k = n;
		}
	}
#if DEBUG_COMPRESS
	show_all_key_word(key_words);
#endif
}

static int build_store_word(struct key_word *k, char split, int type)
{
	unsigned char *store_word = NULL;

	if (final_words_cnt >= (KEY_WORD_BASE - 2)) {
		fprintf(stderr, "final_keyword out of range\n");
		return -1;
	}

	store_word = zalloc(KSYM_NAME_LEN);
	final_words[final_words_cnt].str = store_word;
	switch (type) {
	case 0:		/* xxxxxx */
		memcpy(store_word, k->word, k->word_len);
		final_words[final_words_cnt].len = k->word_len;
		break;
	case 1:		/* xxx_ */
		memcpy(store_word, k->word, k->word_len);
		store_word[k->word_len] = split;
		final_words[final_words_cnt].len = k->word_len + 1;
		break;
	case 2:		/* _xxxx */
	case 5:		/* .xx */
		store_word[0] = split;
		memcpy(store_word + 1, k->word, k->word_len);
		final_words[final_words_cnt].len = k->word_len + 1;
		break;
	case 3:		/* _xx_ */
	case 4:		/* .xx. */
		store_word[0] = split;
		memcpy(store_word + 1, k->word, k->word_len);
		store_word[k->word_len + 1] = split;
		final_words[final_words_cnt].len = k->word_len + 2;
		break;
	default:
		return -1;
	}
	k->idx_type[type] = final_words_cnt;
	finnal_sym_len += final_words[final_words_cnt].len;
#if DEBUG_COMPRESS
	tprintf("%s, used:%4d, type:%d, split:%c, word:%s\n",
		__func__, final_words_cnt, type, split,
		final_words[final_words_cnt].str);
#endif
	final_words_cnt++;
	return 0;
}

#if DEBUG_COMPRESS
static int decode_final_str(struct final_word *f, char *buf)
{
	int i, dlen = 0;
	int data;

	if (f->str[0] < 0x80) {
		strcpy(buf, (char *)f->str);
		return f->len;
	}

	for (i = 0; i < f->len; i += 2) {
		data  = (f->str[i] << 8) | f->str[i + 1];
		data &= ~KEY_WORD_BASE;
		dlen += decode_final_str(final_words + data, buf + dlen);
	}
	return dlen;
}

static void show_sym(struct sym_entry *s, int decode)
{
	int i, data;
	struct final_word *f;
	char buf[128];

	for (i = 0; i < s->len; i++) {
		if (decode && (s->sym[i] >= 0x80)) {
			memset(buf, 0, sizeof(buf));
			data  = (s->sym[i] << 8) | s->sym[i + 1];
			data -= KEY_WORD_BASE;
			f = &final_words[data];
			decode_final_str(f, buf);
			tprintf("%s", buf);
			i++;
		} else if (!decode && (s->sym[i] >= 0x80)) {
			tprintf("<%02x %02x>", s->sym[i], s->sym[i + 1]);
			i++;
		} else {
			tprintf("%c", s->sym[i]);
		}
	}
	tprintf(", len:%d\n", s->len);
}
#endif

static unsigned char *compress_a_keyword(unsigned char *str,
					 struct sym_entry *s,
					 int len, int *relen)
{
	unsigned int hash, final_idx, data, type;
	unsigned int raw_len, remain_len, word_len = len, pre_len;
	struct key_word *k;
	char *word, first_split, end_split, st;

	if (len < 2)	/* no need remember too small string */
		return NULL;

	word = (char *)str;
	first_split = str[0];
	end_split = str[len];
	while (is_split_char(word[0])) {	/* skip split char for hash */
		word++;
		word_len--;
	}
	hash = hash_str((unsigned char *)word, word_len);
	k = find_exist_key_word(hash, (unsigned char *)word, word_len);
	if (!k)
		return NULL;

	if (!is_split_char(first_split)) {
		if (end_split == '_') {
			/* xx_ */
			st   = end_split;
			type = 1;
		} else {
			/* xx or xx */
			st   = 0;
			type = 0;
		}
	} else {
		st   = first_split;
		if (first_split == '.') {
			if (end_split == '.')
				type = 4;
			else
				type = 5;
		} else{
			if (end_split == '_')
				type = 3;
			else
				type = 2;
		}
	}
	if (k->idx_type[type] == 0xffff) {
		/* init for this type */
		if (build_store_word(k, st, type) < 0)
			return NULL;
	}
	final_idx  = k->idx_type[type];
	raw_len    = final_words[final_idx].len;
	data       = final_idx + KEY_WORD_BASE;
	str[0]     = data >> 8;
	str[1]     = data & 0xff;
	pre_len    = (unsigned long)str - (unsigned long)s->sym;
	remain_len = s->len - pre_len - raw_len;
	*relen     = remain_len;
	memmove(str + 2, str + raw_len, remain_len + 1);
	s->len -= (raw_len - 2);
	final_words[final_idx].cnt++;

	return str + 2;
}

static int restore_valueless_keyword(void)
{
	int i, mov_len, j, idx, valueless_cnt = 0;
	struct final_word *f;
	struct sym_entry *s;
	unsigned char *p;

	for (i = 0; i < table_cnt; i++) {
		s = table + i;
		for (j = 0; j < s->len; j++) {
			if (s->sym[j] >= 0x80) {
				idx  = (s->sym[j] << 8) +
				       (s->sym[j + 1] & 0xff);
				idx -= KEY_WORD_BASE;
				f    = &final_words[idx];
				if (f->cnt * f->len <= (f->len + 2 * f->cnt)) {
					mov_len = s->len - (j + 2);
					p = s->sym + j + 2;
					/* move to high area and restore word */
					memmove(p + (f->len - 2), p, mov_len);
					memcpy(s->sym + j, f->str, f->len);
					s->len += (f->len - 2);
					if (f->cnt) {
						/* mark as invalid entry*/
						f->cnt = 0;
						finnal_sym_len -= f->len;
						valueless_cnt++;
					}
				} else {
					j++;
				}
			}
		}
	}
	return valueless_cnt;
}

static void init_special_token(void)
{
	int i;
	struct final_word *f;
	char *special_toke[] = {
		"t_",
		"T_",
		"d_",
		"D_",
		"r_",
		"R_",
		"b_",
		"B_"
	};

	for (i = 0; i < 8; i++) {
		f = &final_words[i];
		f->str = zalloc(KSYM_NAME_LEN);
		strcpy((char *)f->str, special_toke[i]);
		f->len          = 2;
		f->cnt          = 0;
		f->node_cnt     = 0;
		f->tree.rb_node = NULL;
	}
}

static void process_special_token(struct sym_entry *s)
{
	int i, data;
	struct final_word *f;

	for (i = 0; i < 8; i++) {
		f = &final_words[i];
		if (s->sym[0] == f->str[0] && s->sym[1] == f->str[1]) {
			data = KEY_WORD_BASE + i;
			s->sym[0] = data >> 8;
			s->sym[1] = (data & 0xff);
			f->cnt++;
			break;
		}
	}
}

static void count_merge_info(unsigned int d1, unsigned int d2, int *max)
{
	struct final_word *f1;
	struct merge_word *m;
	struct rb_node **rb, *parent = NULL;

	f1 = final_words + d1;
	rb = &f1->tree.rb_node;
	while (*rb) {
		parent = *rb;
		m = rb_entry(parent, struct merge_word, entry);
		if (m->code == d2) {	/* find */
			m->count++;
			if (m->count > *max)
				*max = m->count;
			return;
		} else if (d2 < m->code) {
			rb = &m->entry.rb_left;
		} else
			rb = &m->entry.rb_right;
	}

	/* not found, build a new node */
	m        = zalloc(sizeof(*m));
	m->code  = d2;
	m->count = 1;
	rb_link_node(&m->entry, parent, rb);
	rb_insert_color(&m->entry, &f1->tree);
	f1->node_cnt++;
}

static void build_merge_word(struct sym_entry *sym, int *max_cnt)
{
	int d1, d2, i;
	unsigned char *s;

	s = sym->sym;
	for (i = 0; i < sym->len - 3; i++) {
		if (s[i] >= 0x80) {	/* first large sym */
			if (s[i + 2] >= 0x80) {
				d1  = (s[i] << 8) + (s[i + 1] & 0xff);
				d2  = (s[i + 2] << 8) + (s[i + 3] & 0xff);
				d1 -= KEY_WORD_BASE;
				d2 -= KEY_WORD_BASE;
				count_merge_info(d1, d2, max_cnt);
			}
			i++;
		}
	}
}

#if DEBUG_COMPRESS
static void show_merge_word(struct final_word *f)
{
	struct rb_node *rb;
	struct merge_word *m;
	int i;
	struct final_word *f2;
	char buf1[KSYM_NAME_LEN] = {};
	char buf2[KSYM_NAME_LEN] = {};

	decode_final_str(f, buf1);
	tprintf("%s, cnt:%d, node:%d, tree:%p, s:%s\n",
		__func__, f->cnt, f->node_cnt, f->tree.rb_node, buf1);
	for (rb = rb_first(&f->tree); rb; rb = rb_next(rb)) {
		m  = rb_entry(rb, struct merge_word, entry);
		f2 = &final_words[m->code];
		tprintf("%5d, cnt:%5d, %s -> ", m->code, m->count, buf1);
		decode_final_str(f2, buf2);
		tprintf("%s\n", buf2);
	}
}
#endif

static void count_merge_count(struct final_word *f, unsigned int *cnt)
{
	struct rb_node *rb;
	struct merge_word *m;

	for (rb = rb_first(&f->tree); rb; rb = rb_next(rb)) {
		m  = rb_entry(rb, struct merge_word, entry);
		cnt[m->count]++;
	}
}

static int calculate_merge_cnt_threshold(int available, int max_cnt)
{
	unsigned int *cnt_cnt, i, ret = 0, node_cnt = 0;

	if (available < 0)
		return -1;

	cnt_cnt = zalloc((max_cnt + 2) * sizeof(int));
	for (i = 0; i < final_words_cnt; i++) {
		if (final_words[i].node_cnt) {
			count_merge_count(final_words + i, cnt_cnt);
			node_cnt += final_words[i].node_cnt;
		}
	}
	if (node_cnt < available) {
		ret = 5;
	} else {
		for (i = 0; i <= max_cnt; i++) {
			if (cnt_cnt[i]) {
				node_cnt -= cnt_cnt[i];
				if (node_cnt < available && !ret) {
					ret = i + 1;
					break;
				}
			}
		}
	}
	free(cnt_cnt);
	if (!ret)
		return -1;
	if (ret < 5)
		ret = 5;
	return ret;
}

static int find_empty_idx(int last)
{
	int ret;

	for (ret = last; ret < KEY_WORD_BASE; ret++) {
		if (final_words[ret].cnt == 0)
			return ret;
	}
	return -1;
}

static struct merge_tree *find_merge_code(int d1, int d2)
{
	struct merge_tree *mt;
	struct rb_node *rb;
	unsigned int data = (d1 << 16 | d2);

	rb = merge_trees.rb_node;
	while (rb) {
		mt = rb_entry(rb, struct merge_tree, entry);
		if (mt->key == data) {	/* find */
			return mt;
		} else if (data < mt->key) {
			rb = mt->entry.rb_left;
		} else
			rb = mt->entry.rb_right;
	}
	return NULL;
}

static int merge_able(struct final_word *f1, int d2, int thresh)
{
	struct merge_word *mw;
	struct rb_node *rb;

	rb = f1->tree.rb_node;
	while (rb) {
		mw = rb_entry(rb, struct merge_word, entry);
		if (mw->code == d2) {	/* find */
			if (mw->count >= thresh)
				return 1;
			return 0;
		} else if (d2 < mw->code) {
			rb = mw->entry.rb_left;
		} else
			rb = mw->entry.rb_right;
	}
	return 0;
}

static struct merge_tree *build_merge_tree(int d1, int d2, int *last)
{
	int new_code;
	unsigned int key;
	struct merge_tree *mt;
	struct rb_node **rb, *parent = NULL;

	new_code = find_empty_idx(*last);
	if (new_code < 0)
		return NULL;

	*last = new_code;
	key = (d1 << 16) | d2;
	rb = &merge_trees.rb_node;
	while (*rb) {
		parent = *rb;
		mt = rb_entry(parent, struct merge_tree, entry);
		if (mt->key == key) {	/* find */
			fprintf(stderr, "BUG: %s find exist merge tree:%d\n",
				__func__, key);
			exit(-1);
		} else if (key < mt->key) {
			rb = &mt->entry.rb_left;
		} else
			rb = &mt->entry.rb_right;
	}

	/* not found, build a new node */
	mt      = zalloc(sizeof(*mt));
	mt->key = key;
	mt->new = new_code;
	rb_link_node(&mt->entry, parent, rb);
	rb_insert_color(&mt->entry, &merge_trees);
	return mt;
}

#if DEBUG_COMPRESS
static void show_final_str(struct final_word *f, int decode)
{
	int i;
	char buf[KSYM_NAME_LEN] = {};

	tprintf("final_str, len:%2d, cnt:%4d, nodes:%4d, str:",
		f->len, f->cnt, f->node_cnt);
	if (decode) {
		if (f->str) {
			decode_final_str(f, buf);
			tprintf("%s", buf);
		}
	} else if (f->str) {
		for (i = 0; i < f->len; i++) {
			if (f->str[i] >= 0x80) {
				tprintf("<%02x %02x>", f->str[i], f->str[i + 1]);
				i++;
			} else {
				tprintf("%c", f->str[i]);
			}
		}
	}
	tprintf("\n");
}
#endif

static void init_merge_tree(struct merge_tree *mt, int d1, int d2)
{
	struct final_word *newf;

	newf = &final_words[mt->new];
	d1 |= KEY_WORD_BASE;
	d2 |= KEY_WORD_BASE;
	if (!newf->str)
		newf->str = zalloc(KSYM_NAME_LEN);
	newf->str[0]       = d1 >> 8;
	newf->str[1]       = d1 & 0xff;
	newf->str[2]       = d2 >> 8;
	newf->str[3]       = d2 & 0xff;
	newf->len          = 4;
	newf->cnt          = 0;
	newf->node_cnt     = 0;
	newf->tree.rb_node = NULL;
}

static int merge_words(struct sym_entry *s, int *last, int thresh)
{
	int i, d1, d2, new, rlen, len, used = 0;
	struct final_word *f1, *nf;
	struct merge_tree *mt;

	len = s->len;
	for (i = 0; i < len - 3; i++) {
		if (s->sym[i] < 0x80)
			continue;

		if (s->sym[i + 2] >= 0x80) {
			d1  = (s->sym[i] << 8) + s->sym[i + 1];
			d2  = (s->sym[i + 2] << 8) + s->sym[i + 3];
			d1 -= KEY_WORD_BASE;
			f1  = &final_words[d1];
			if (!f1->node_cnt) { /* this word can't merge */
				i++;
				continue;
			}
			d2 -= KEY_WORD_BASE;
			mt = find_merge_code(d1, d2);
			if (!mt) {	/* check it can merge? */
				if (!merge_able(f1, d2, thresh)) {
					i++;
					continue;
				}
				mt = build_merge_tree(d1, d2, last);
				init_merge_tree(mt, d1, d2);
				used = 1;
			}
			/* merge word */
			new           = mt->new;
			nf            = &final_words[new];
			new          |= KEY_WORD_BASE;
			s->sym[i]     = (new >> 8);
			s->sym[i + 1] = (new & 0xff);
			rlen          = s->len - i - 4;
			memmove(&s->sym[i] + 2, &s->sym[i] + 4, rlen);
			len          -= 2;
			s->len       -= 2;
			nf->cnt++;
		#if DEBUG_COMPRESS
			tprintf("%s, merge %04x %04x to %04x, i:%d last:%d\n",
				__func__, d1, d2, new, i, *last);
			show_final_str(final_words + d1, 0);
			show_final_str(final_words + d2, 0);
			show_final_str(nf, 0);
		#endif
			break;
		}
		/* no contiguous word */
		i++;
	}
	return used;
}

static void delete_merge_word(struct rb_node *rb)
{
	struct merge_word *m;
	struct rb_node *parent;

	if (!rb->rb_left && !rb->rb_right) {
		parent = rb_parent(rb);
		if (parent) {
			if (rb == parent->rb_left)
				parent->rb_left = NULL;
			else
				parent->rb_right = NULL;
		}
		m = rb_entry(rb, struct merge_word, entry);
		free(m);
		return;
	}

	if (rb->rb_left)
		delete_merge_word(rb->rb_left);
	if (rb->rb_right)
		delete_merge_word(rb->rb_right);
	delete_merge_word(rb);
}

static void free_merge_word(void)
{
	struct final_word *f;
	int i;

	for (i = 0; i < KEY_WORD_BASE; i++) {
		f = final_words + i;
		if (!f->node_cnt)
			continue;
		delete_merge_word(f->tree.rb_node);
		f->tree.rb_node = NULL;
		f->node_cnt = 0;
	}
}

static void delete_merge_tree(struct rb_node *rb)
{
	struct merge_tree *m;
	struct rb_node *parent;

	if (!rb->rb_left && !rb->rb_right) {
		parent = rb_parent(rb);
		if (parent) {
			if (rb == parent->rb_left)
				parent->rb_left = NULL;
			else
				parent->rb_right = NULL;
		}
		m = rb_entry(rb, struct merge_tree, entry);
		free(m);
		return;
	}

	if (rb->rb_left)
		delete_merge_tree(rb->rb_left);
	if (rb->rb_right)
		delete_merge_tree(rb->rb_right);
	delete_merge_tree(rb);
}

static void free_merge_tree(void)
{
	delete_merge_tree(merge_trees.rb_node);
	merge_trees.rb_node = NULL;
}

#if DEBUG_COMPRESS
static void print_output_size(void)
{
	int i, output_len = 0, fstr_len = 0;

	for (i = 0; i < table_cnt; i++)
		output_len += table[i].len;

	for (i = 0; i < final_words_cnt; i++) {
		if (final_words[i].cnt)
			fstr_len += final_words[i].len;
	}
	tprintf("%s, input len:%ld, output len:%d, fstr_len:%d, words:%d, total:%d\n",
		__func__, input_len, output_len, fstr_len, final_words_cnt,
		output_len + fstr_len + final_words_cnt);
}
#endif

static void compress_key_word(void)
{
	unsigned char *p1, *r, *p2 = NULL;
	int i = 0, j, k, sublen = 0, splited = 0, relen;
	struct sym_entry *s;
	int max, last_idx, thresh, value_less, code_room;

	/* 1st step compress each symbol by word */
	for (i = 0; i < table_cnt; i++) {
		s      = &table[i];
		p1     = s->sym;
		p2     = p1;
		k      = 0;
		relen  = s->len;
		sublen = 0;
		while (k < relen) {
			if (is_split_char(p1[k])) {
				r = compress_a_keyword(p2, s, sublen, &relen);
				splited++;
				if (!r) {	/* not compressed */
					p2 += sublen;
					sublen = 1;
					k++;
				} else {	/* compressed */
					p2 = r;
					p1 = r;
					k = 0;
					sublen = 0;
				}
			} else {
				sublen++;
				k++;
			}
		}
		if (p2 == s->sym) { /* none of split sub string */
			compress_a_keyword(p2, s, s->len, &relen);
		}
		if (splited) {	/* end of string */
			compress_a_keyword(p2, s, sublen, &relen);
		}
	}
	value_less = restore_valueless_keyword();

	init_special_token();
	for (i = 0; i < table_cnt; i++)
		process_special_token(table + i);

	/* 2nd step, merge high freq keyword */
	code_room = (int)KEY_WORD_BASE - final_words_cnt + value_less;
	for (k = 0; k < max_sym_words && code_room > 0; k++) {
		/* merge words every round */
		max = 0;
		for (i = 0; i < table_cnt; i++) {
			if (table[i].len >= 4)
				build_merge_word(table + i, &max);
		}

	#if DEBUG_COMPRESS
		for (i = 0; i < final_words_cnt; i++) {
			if (final_words[i].node_cnt)
				show_merge_word(final_words + i);
		}
	#endif

		thresh = calculate_merge_cnt_threshold(code_room, max);
		if (thresh < 0 || thresh > max) {
			free_merge_word();
			break;
		}

		j        = 0;
		last_idx = 0;
		for (i = 0; i < table_cnt; i++)
			j += merge_words(table + i, &last_idx, thresh);

		code_room -= j;
		if (KEY_WORD_BASE - code_room > final_words_cnt)
			final_words_cnt = KEY_WORD_BASE - code_room;
		free_merge_word();
		free_merge_tree();
	}

#if DEBUG_COMPRESS
	print_output_size();
	for (i = 0; i < table_cnt; i++)
		show_sym(&table[i], 1);

	for (i = 0; i < final_words_cnt; i++) {
		tprintf("%5d, ", i);
		show_final_str(final_words + i, 1);
	}
#endif
}

/* table lookup compression functions */

/* count all the possible tokens in a symbol */
static void learn_symbol(unsigned char *symbol, int len)
{
	int i;

	for (i = 0; i < len - 1; i++) {
		if (symbol[i] >= 0x80) {	/* skip high word */
			i++;
			continue;
		}
		if (symbol[i + 1] < 0x80)
			token_profit[symbol[i] + (symbol[i + 1] * 128)]++;
		else
			i += 2;
	}
}

/* decrease the count for all the possible tokens in a symbol */
static void forget_symbol(unsigned char *symbol, int len)
{
	int i;

	for (i = 0; i < len - 1; i++) {
		if (symbol[i] >= 0x80) {	/* skip high word */
			i++;
			continue;
		}
		if (symbol[i + 1] < 0x80)
			token_profit[symbol[i] + (symbol[i + 1] * 128)]--;
		else
			i += 2;
	}
}

/* remove all the invalid symbols from the table and do the initial token count */
static void build_initial_key_word(void)
{
	unsigned int i, pos;
	unsigned long long base_addr = 0;

	pos = 0;
	for (i = 0; i < table_cnt; i++) {
		if (symbol_valid(&table[i])) {
			if (pos != i)
				table[pos] = table[i];

			if (!base_addr) {
				base_addr = table[i].addr;
				short_base_cnt++;
			}
			if (table[i].addr - base_addr >= 0x10000) {
				base_addr = table[i].addr;
				short_base_cnt++;
			}
			input_len += table[pos].len;
			split_symbol(&table[pos]);
			pos++;
		}
	}
	table_cnt = pos;
#if DEBUG_COMPRESS
	tprintf("%s, table_cnt:%d, max_sym_words:%d, nr_short:%d\n",
		__func__, table_cnt, max_sym_words, short_base_cnt);
#endif
}

/*------------- token compress ---------------------*/
static unsigned char *get_sym_char(int i)
{
	return table[i].sym;
}

static unsigned int get_sym_len(int i)
{
	return table[i].len;
}

static unsigned char *get_word_char(int i)
{
	return final_words[i].str;
}

static unsigned int get_word_len(int i)
{
	return final_words[i].len;
}

static void set_sym_len(int i, int len)
{
	table[i].len = len;
}

static void set_word_len(int i, int len)
{
	final_words[i].len = len;
}

typedef unsigned char *get_char(int i);
typedef unsigned int  get_len(int i);
typedef void set_len(int i, int len);

static void *find_token(unsigned char *str, int len, unsigned char *token)
{
	int i;

	for (i = 0; i < len - 1; i++) {
		if (str[i] >= 0x80) {	/* skip large word */
			i++;
			continue;
		}
		if (str[i] == token[0] && str[i+1] == token[1])
			return &str[i];
	}
	return NULL;
}

/* replace a given token in all the valid symbols. Use the sampled symbols
 * to update the counts
 */
static void compress_symbols(unsigned char *str, int idx, int count,
			     get_char get_chars, get_len get_lens,
			     set_len set_lens)
{
	unsigned int i, len, size;
	unsigned char *p1, *p2;

	for (i = 0; i < count; i++) {

		len = get_lens(i);
		p1  = get_chars(i);
		if (!p1)
			continue;

		/* find the token on the symbol */
		p2 = find_token(p1, len, str);
		if (!p2)
			continue;

		/* decrease the counts for this symbol's tokens */
		forget_symbol(p1, len);

		size = len;

		do {
			*p2 = idx;
			p2++;
			size = size - (p2 - p1) - 1;
			memmove(p2, p2 + 1, size);
			p1 = p2;
			len--;

			if (size < 2)
				break;

			/* find the token on the symbol */
			p2 = find_token(p1, size, str);

		} while (p2);

		set_lens(i, len);
		p1  = get_chars(i);

		/* increase the counts for this symbol's new tokens */
		learn_symbol(p1, len);
	}
}

/* search the token with the maximum profit */
static int find_best_token(void)
{
	int i, best, bestprofit;

	bestprofit = -10000;
	best = 0;

	for (i = 0; i < 128 * 128; i++) {
		if (token_profit[i] > bestprofit) {
			best = i;
			bestprofit = token_profit[i];
		}
	}
	return best;
}

/* this is the core of the algorithm: calculate the "best" table */
static void optimize_result(struct best_table *b, int count,
			    get_char get_chars, get_len get_lens,
			    set_len set_lens)
{
	int i, best = 0;

	/* using the '\0' symbol last allows compress_symbols to use standard
	 * fast string functions
	 */
	for (i = 127; i >= 0; i--) {

		/* if this table slot is empty (it is not used by an actual
		 * original char code
		 */
		if (!b->len[i]) {

			/* find the token with the breates profit value */
			best = find_best_token();
			if (token_profit[best] == 0)
				break;

			/* place it in the "best" table */
			b->len[i] = 2;
			b->table[i][0] = best & 0x7F;
			b->table[i][1] = (best / 128) & 0x7F;

			/* replace this token in all the valid symbols */
			compress_symbols(b->table[i], i, count,
					 get_chars, get_lens, set_lens);
		}
	}
}

/* remove all the invalid symbols from the table and do the initial token count */
static void build_initial_tok_table(int count, get_char get_chars,
				    get_len get_lens)
{
	unsigned int i;
	unsigned char *p;
	int len;

	for (i = 0; i < count; i++) {
		p   = get_chars(i);
		if (!p)
			continue;
		len = get_lens(i);
		learn_symbol(p, len);
	}
}

/* start by placing the symbols that are actually used on the table */
static void insert_real_symbols_in_table(struct best_table *best, int count,
					 get_char get_chars, get_len get_lens)
{
	unsigned int i, j, c, len;
	unsigned char *p;

	memset(best, 0, sizeof(*best));

	for (i = 0; i < count; i++) {
		len = get_lens(i);
		p   = get_chars(i);
		if (!p)
			continue;

		for (j = 0; j < len; j++) {
			c = p[j];
			if (c >= 0x80) {
				j++;
				continue;
			}
			best->table[c][0] = c;
			best->len[c] = 1;
		}
	}
}

static void optimize_token_table(void)
{
	/* 1st, compress with splited key word */
	start_time = nanotime();
	build_initial_key_word();
	if (!table_cnt) {
		fprintf(stderr, "No valid symbol.\n");
		exit(1);
	}
	remove_valueless_key_word();
	compress_key_word();

	/* 2nd, compress remain chars / final_str table */
	build_initial_tok_table(table_cnt, get_sym_char, get_sym_len);
	insert_real_symbols_in_table(&sym_best, table_cnt,
				     get_sym_char, get_sym_len);
	optimize_result(&sym_best, table_cnt,
			get_sym_char, get_sym_len, set_sym_len);

	memset(token_profit, 0, sizeof(token_profit));
	build_initial_tok_table(final_words_cnt,
				get_word_char, get_word_len);
	insert_real_symbols_in_table(&word_best, final_words_cnt,
				     get_word_char, get_word_len);
	optimize_result(&word_best, final_words_cnt,
			get_word_char, get_word_len, set_word_len);

	start_time = nanotime() - start_time;
#if DEBUG_COMPRESS
	print_output_size();
#endif
}

/* guess for "linker script provide" symbol */
static int may_be_linker_script_provide_symbol(const struct sym_entry *se)
{
	const char *symbol = (char *)se->sym + 1;
	int len = se->len - 1;

	if (len < 8)
		return 0;

	if (symbol[0] != '_' || symbol[1] != '_')
		return 0;

	/* __start_XXXXX */
	if (!memcmp(symbol + 2, "start_", 6))
		return 1;

	/* __stop_XXXXX */
	if (!memcmp(symbol + 2, "stop_", 5))
		return 1;

	/* __end_XXXXX */
	if (!memcmp(symbol + 2, "end_", 4))
		return 1;

	/* __XXXXX_start */
	if (!memcmp(symbol + len - 6, "_start", 6))
		return 1;

	/* __XXXXX_end */
	if (!memcmp(symbol + len - 4, "_end", 4))
		return 1;

	return 0;
}

static int prefix_underscores_count(const char *str)
{
	const char *tail = str;

	while (*tail == '_')
		tail++;

	return tail - str;
}

static int compare_symbols(const void *a, const void *b)
{
	const struct sym_entry *sa;
	const struct sym_entry *sb;
	int wa, wb;

	sa = a;
	sb = b;

	/* sort by address first */
	if (sa->addr > sb->addr)
		return 1;
	if (sa->addr < sb->addr)
		return -1;

	/* sort by "weakness" type */
	wa = (sa->sym[0] == 'w') || (sa->sym[0] == 'W');
	wb = (sb->sym[0] == 'w') || (sb->sym[0] == 'W');
	if (wa != wb)
		return wa - wb;

	/* sort by "linker script provide" type */
	wa = may_be_linker_script_provide_symbol(sa);
	wb = may_be_linker_script_provide_symbol(sb);
	if (wa != wb)
		return wa - wb;

	/* sort by the number of prefix underscores */
	wa = prefix_underscores_count((const char *)sa->sym + 1);
	wb = prefix_underscores_count((const char *)sb->sym + 1);
	if (wa != wb)
		return wa - wb;

	/* sort by initial order, so that other symbols are left undisturbed */
	return sa->start_pos - sb->start_pos;
}

static void sort_symbols(void)
{
	qsort(table, table_cnt, sizeof(struct sym_entry), compare_symbols);
}

/* find the minimum non-absolute symbol address */
int main(int argc, char **argv)
{
	if (argc >= 2) {
		int i;

		for (i = 1; i < argc; i++) {
			if (strcmp(argv[i], "--all-symbols") == 0) {
				all_symbols = 1;
			} else if (strcmp(argv[i], "--absolute-percpu") == 0) {
				/* not supported */
			} else if (strncmp(argv[i], "--symbol-prefix=", 16) == 0) {
				char *p = &argv[i][16];
				/* skip quote */
				if ((*p == '"' && *(p+2) == '"') || (*p == '\'' && *(p+2) == '\''))
					p++;
				symbol_prefix_char = *p;
			} else if (strcmp(argv[i], "--base-relative") == 0) {
				/* not supported */
			} else {
				usage();
			}
		}
	} else if (argc != 1)
		usage();

	read_map(stdin);
	sort_symbols();
	optimize_token_table();
	write_src();

	return 0;
}
