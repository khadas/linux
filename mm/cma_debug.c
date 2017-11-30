/*
 * CMA DebugFS Interface
 *
 * Copyright (c) 2015 Sasha Levin <sasha.levin@oracle.com>
 */


#include <linux/debugfs.h>
#include <linux/cma.h>
#include <linux/list.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/mm_types.h>

#include <linux/cma.h>

struct cma_mem {
	struct hlist_node node;
	struct page *p;
	unsigned long n;
};

static struct dentry *cma_debugfs_root;

static int cma_debugfs_get(void *data, u64 *val)
{
	unsigned long *p = data;

	*val = *p;

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(cma_debugfs_fops, cma_debugfs_get, NULL, "%llx\n");

static int cma_used_get(void *data, u64 *val)
{
	struct cma *cma = data;
	unsigned long used;

	mutex_lock(&cma->lock);
	/* pages counter is smaller than sizeof(int) */
	used = bitmap_weight(cma->bitmap, (int)cma_bitmap_maxno(cma));
	mutex_unlock(&cma->lock);
	*val = (u64)used << cma->order_per_bit;

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(cma_used_fops, cma_used_get, NULL, "%llu\n");

static int cma_maxchunk_get(void *data, u64 *val)
{
	struct cma *cma = data;
	unsigned long maxchunk = 0;
	unsigned long start, end = 0;
	unsigned long bitmap_maxno = cma_bitmap_maxno(cma);

	mutex_lock(&cma->lock);
	for (;;) {
		start = find_next_zero_bit(cma->bitmap, bitmap_maxno, end);
		if (start >= cma->count)
			break;
		end = find_next_bit(cma->bitmap, bitmap_maxno, start);
		maxchunk = max(end - start, maxchunk);
	}
	mutex_unlock(&cma->lock);
	*val = (u64)maxchunk << cma->order_per_bit;

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(cma_maxchunk_fops, cma_maxchunk_get, NULL, "%llu\n");

static void cma_add_to_cma_mem_list(struct cma *cma, struct cma_mem *mem)
{
	spin_lock(&cma->mem_head_lock);
	hlist_add_head(&mem->node, &cma->mem_head);
	spin_unlock(&cma->mem_head_lock);
}

static struct cma_mem *cma_get_entry_from_list(struct cma *cma)
{
	struct cma_mem *mem = NULL;

	spin_lock(&cma->mem_head_lock);
	if (!hlist_empty(&cma->mem_head)) {
		mem = hlist_entry(cma->mem_head.first, struct cma_mem, node);
		hlist_del_init(&mem->node);
	}
	spin_unlock(&cma->mem_head_lock);

	return mem;
}

static int cma_free_mem(struct cma *cma, int count)
{
	struct cma_mem *mem = NULL;

	while (count) {
		mem = cma_get_entry_from_list(cma);
		if (mem == NULL)
			return 0;

		if (mem->n <= count) {
			cma_release(cma, mem->p, mem->n);
			count -= mem->n;
			kfree(mem);
		} else if (cma->order_per_bit == 0) {
			cma_release(cma, mem->p, count);
			mem->p += count;
			mem->n -= count;
			count = 0;
			cma_add_to_cma_mem_list(cma, mem);
		} else {
			pr_debug("cma: cannot release partial block when order_per_bit != 0\n");
			cma_add_to_cma_mem_list(cma, mem);
			break;
		}
	}

	return 0;

}

static int cma_free_write(void *data, u64 val)
{
	int pages = val;
	struct cma *cma = data;

	return cma_free_mem(cma, pages);
}
DEFINE_SIMPLE_ATTRIBUTE(cma_free_fops, NULL, cma_free_write, "%llu\n");

static int cma_alloc_mem(struct cma *cma, int count)
{
	struct cma_mem *mem;
	struct page *p;

	mem = kzalloc(sizeof(*mem), GFP_KERNEL);
	if (!mem)
		return -ENOMEM;

	p = cma_alloc(cma, count, 0);
	if (!p) {
		kfree(mem);
		return -ENOMEM;
	}

	mem->p = p;
	mem->n = count;

	cma_add_to_cma_mem_list(cma, mem);

	return 0;
}

static int cma_alloc_write(void *data, u64 val)
{
	int pages = val;
	struct cma *cma = data;

	return cma_alloc_mem(cma, pages);
}
DEFINE_SIMPLE_ATTRIBUTE(cma_alloc_fops, NULL, cma_alloc_write, "%llu\n");

struct array_data {
	void *array;
	u32 elements;
};

static size_t u32h_format_array(char *buf, size_t bufsize,
			       u32 *array, int array_size)
{
	size_t ret = 0;
	int i;
	char term;

	for (i = 0; i < array_size; i++) {
		term = ((i & 0x7) != 0x7) ? ' ' : '\n';

		ret += snprintf(buf + ret, bufsize - ret,
			"%08x%c", array[i], term);
	}
	ret += snprintf(buf + ret, bufsize - ret,
		"\nbitmap address:%p\n", array);
	return ret;
}

static int u32h_array_open(struct inode *inode, struct file *file)
{
	struct array_data *data = inode->i_private;
	int size, ret, elements = data->elements;
	char *buf;

	size = elements * 9;
	buf = kmalloc(size + 64, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;
	buf[size] = 0;

	file->private_data = buf;
	ret = u32h_format_array(buf, size + 64, data->array, data->elements);

	return nonseekable_open(inode, file);
}

static ssize_t u32h_array_read(struct file *file, char __user *buf, size_t len,
			      loff_t *ppos)
{
	size_t size = strlen(file->private_data);

	return simple_read_from_buffer(buf, len, ppos,
					file->private_data, size);
}

static int u32h_array_release(struct inode *inode, struct file *file)
{
	kfree(file->private_data);

	return 0;
}

static const struct file_operations u32h_array_fops = {
	.owner	 = THIS_MODULE,
	.open	 = u32h_array_open,
	.release = u32h_array_release,
	.read	 = u32h_array_read,
	.llseek  = no_llseek,
};

static struct dentry *debugfs_create_bitmap(const char *name, umode_t mode,
					    struct dentry *parent,
					    u32 *array, u32 elements)
{
	struct array_data *data = kmalloc(sizeof(*data), GFP_KERNEL);

	if (data == NULL)
		return NULL;

	data->array = array;
	data->elements = elements;

	return debugfs_create_file(name, mode, parent, data, &u32h_array_fops);
}

static void cma_debugfs_add_one(struct cma *cma, int idx)
{
	struct dentry *tmp;
	char name[16];
	int u32s;

	sprintf(name, "cma-%d", idx);

	tmp = debugfs_create_dir(name, cma_debugfs_root);

	debugfs_create_file("alloc", S_IWUSR, tmp, cma,
				&cma_alloc_fops);

	debugfs_create_file("free", S_IWUSR, tmp, cma,
				&cma_free_fops);

	debugfs_create_file("base_pfn", S_IRUGO, tmp,
				&cma->base_pfn, &cma_debugfs_fops);
	debugfs_create_file("count", S_IRUGO, tmp,
				&cma->count, &cma_debugfs_fops);
	debugfs_create_file("order_per_bit", S_IRUGO, tmp,
				&cma->order_per_bit, &cma_debugfs_fops);
	debugfs_create_file("used", S_IRUGO, tmp, cma, &cma_used_fops);
	debugfs_create_file("maxchunk", S_IRUGO, tmp, cma, &cma_maxchunk_fops);

	u32s = DIV_ROUND_UP(cma_bitmap_maxno(cma), BITS_PER_BYTE * sizeof(u32));
	debugfs_create_bitmap("bitmap", S_IRUGO, tmp, (u32 *)cma->bitmap, u32s);
}

static bool cma_debug __initdata;

static int __init early_cma_debug(char *buf)
{
	if (!buf)
		return -EINVAL;

	if (strcmp(buf, "off") == 0)
		cma_debug = false;
	else if (strcmp(buf, "on") == 0)
		cma_debug = true;

	pr_info("cma debug %sabled\n", cma_debug ? "en" : "dis");

	return 0;
}
early_param("cma_debug", early_cma_debug);

static int __init cma_debugfs_init(void)
{
	int i;

	if (!cma_debug)
		return 0;

	cma_debugfs_root = debugfs_create_dir("cma", NULL);
	if (!cma_debugfs_root)
		return -ENOMEM;

	for (i = 0; i < cma_area_count; i++)
		cma_debugfs_add_one(&cma_areas[i], i);

	return 0;
}
late_initcall(cma_debugfs_init);
