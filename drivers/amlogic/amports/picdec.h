#ifndef _PICDEC_INCLUDE__
#define _PICDEC_INCLUDE__

#include <linux/interrupt.h>
/* #include <mach/am_regs.h> */
#include <linux/amlogic/amports/canvas.h>
#include <linux/fb.h>
#include <linux/list.h>
#include <linux/uaccess.h>
#include <linux/sysfs.h>
#include  <linux/spinlock.h>
#include <linux/kthread.h>
#include <linux/io-mapping.h>

/**************************************************************
**                                                                   **
**  macro define                                                         **
**                                                                   **
***************************************************************/

#define PICDEC_IOC_MAGIC  'P'
#define PICDEC_IOC_FRAME_RENDER     _IOW(PICDEC_IOC_MAGIC, 0x00, unsigned int)
#define PICDEC_IOC_FRAME_POST     _IOW(PICDEC_IOC_MAGIC, 0X01, unsigned int)
#define PICDEC_IOC_CONFIG_FRAME  _IOW(PICDEC_IOC_MAGIC, 0X02, unsigned int)

struct picdec_device_s {
	char name[20];
	struct platform_device *pdev;
	int task_running;
	int dump;
	char *dump_path;
	unsigned int open_count;
	int major;
	unsigned int dbg_enable;
	struct class *cla;
	struct device *dev;
	resource_size_t buffer_start;
	unsigned int buffer_size;
	resource_size_t assit_buf_start;
	const vinfo_t *vinfo;
	int disp_width;
	int disp_height;
	int frame_render;
	int frame_post;
	struct ge2d_context_s *context;
	int cur_index;
	struct io_mapping *mapping;
};

struct source_input_s {
	char *input;
	int frame_width;
	int frame_height;
	int format;
	int rotate;
};

void stop_picdec_task(void);
int picdec_buffer_init(void);
void get_picdec_buf_info(resource_size_t *start, unsigned int *size,
						 struct io_mapping **mapping);
int picdec_fill_buffer(struct vframe_s *vf, struct ge2d_context_s *context,
	struct config_para_ex_s *ge2d_config);
extern void set_freerun_mode(int mode);

extern int start_picdec_task(void);
extern int start_picdec_simulate_task(void);
#endif				/* _PICDEC_INCLUDE__ */
