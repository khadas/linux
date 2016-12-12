#ifndef RDMA_VSYNC_H_
#define RDMA_VSYNC_H_
void vsync_rdma_config(void);
void vsync_rdma_config_pre(void);
bool is_vsync_rdma_enable(void);
void enable_rdma_log(int flag);
void enable_rdma(int enable_flag);
extern int rdma_watchdog_setting(int flag);

#endif

