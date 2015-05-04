#ifndef RDMA_VSYNC_H_
#define RDMA_VSYNC_H_
void vsync_rdma_config(void);
void vsync_rdma_config_pre(void);
bool is_vsync_rdma_enable(void);
void start_rdma(void);
void enable_rdma_log(int flag);
void enable_rdma(int enable_flag);

int rdma_init2(void);

#endif

