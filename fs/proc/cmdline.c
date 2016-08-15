#include <linux/fs.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#include <asm/setup.h>  /* for COMMAND_LINE_SIZE */
#include <asm/uaccess.h>    /* copy_from_user */

char proc_cmdline[COMMAND_LINE_SIZE];

static int cmdline_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%s\n", proc_cmdline);
	return 0;
}

static int cmdline_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, cmdline_proc_show, NULL);
}

static int cmdline_proc_write(struct file *file, const char __user *buf,
				size_t len, loff_t *ppos)
{
	char str[COMMAND_LINE_SIZE];
	if (copy_from_user(str, buf, len)) {
	   printk( KERN_INFO "[cmdline] copy_from_user failed.\n");
	   return -EFAULT;
	}
	str[len] = '\0';
	strlcpy(proc_cmdline, str, min((int)len, COMMAND_LINE_SIZE));

	return len;
}

static const struct file_operations cmdline_proc_fops = {
	.open		= cmdline_proc_open,
	.read		= seq_read,
	.write		= cmdline_proc_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init proc_cmdline_init(void)
{
	strcpy(proc_cmdline, saved_command_line);
	proc_create("cmdline", 0, NULL, &cmdline_proc_fops);
	return 0;
}
fs_initcall(proc_cmdline_init);
