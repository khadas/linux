#ifndef _GE2D_WQ_H_
#define _GE2D_WQ_H_


extern ssize_t work_queue_status_show(struct class *cla,
		struct class_attribute *attr, char *buf);

extern ssize_t free_queue_status_show(struct class *cla,
		struct class_attribute *attr, char *buf);

extern int ge2d_setup(int irq, struct reset_control *rstc);
extern int ge2d_wq_init(void);
extern int ge2d_wq_deinit(void);

#endif
