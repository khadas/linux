#ifndef __INSTABOOT_KERNEL_H_
#define __INSTABOOT_KERNEL_H_

extern int snapshot_read_next(struct snapshot_handle *handle);
extern int snapshot_write_next(struct snapshot_handle *handle);
extern void snapshot_write_finalize(struct snapshot_handle *handle);
extern unsigned long snapshot_get_image_size(void);
extern int snapshot_image_loaded(struct snapshot_handle *handle);
extern dev_t swsusp_resume_device;
extern void end_swap_bio_read(struct bio *bio, int err);
extern unsigned int nr_free_highpages(void);
extern void bio_set_pages_dirty(struct bio *bio);

#endif
