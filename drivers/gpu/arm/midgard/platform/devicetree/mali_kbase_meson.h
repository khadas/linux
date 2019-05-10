

#include <linux/types.h>
#include <linux/workqueue.h>
#include <linux/clk-provider.h>
#include <linux/clk.h>

#define RESET0_MASK     0x00
#define RESET1_MASK     0x01
#define RESET2_MASK     0x02

#define RESET0_LEVEL    0x10
#define RESET1_LEVEL    0x11
#define RESET2_LEVEL    0x12

#define Mali_WrReg(regnum, value)   writel((value),(kbdev->reg + (regnum)))
#define Mali_RdReg(regnum)          readl(kbdev->reg + (regnum))
#define MESON_PRINT(...)            

struct meson_context {
	struct mutex gpu_clock_lock;
	struct mutex gpu_dvfs_handler_lock;
	spinlock_t gpu_dvfs_spinlock;
#ifdef CONFIG_MALI_MIDGARD_DVFS
	int utilization;
	int util_gl_share;
	int util_cl_share[2];
#ifdef CONFIG_CPU_THERMAL_IPA
	int norm_utilisation;
	int freq_for_normalisation;
	unsigned long long power;
#endif /* CONFIG_CPU_THERMAL_IPA */
	int max_lock;
	int min_lock;
#if 0
	int user_max_lock[NUMBER_LOCK];
	int user_min_lock[NUMBER_LOCK];
#endif
	int target_lock_type;
	int down_requirement;
	bool wakeup_lock;
	int governor_num;
	int governor_type;
	char governor_list[100];
	bool dvfs_status;
#ifdef CONFIG_CPU_THERMAL_IPA
	int time_tick;
	u32 time_busy;
	u32 time_idle;
#endif /* CONFIG_CPU_THERMAL_IPA */
#endif
	int cur_clock;
	int cur_voltage;
	int voltage_margin;
	bool tmu_status;
	int debug_level;
	int polling_speed;
	struct workqueue_struct *dvfs_wq;
    void __iomem *reg_base_reset;
    void __iomem *reg_base_aobus;
    void __iomem *reg_base_hiubus;
    struct clk  *clk_mali;
    struct clk  *clk_gp;
};

