/*
 * Driver for the amlogic vpu controller
 *
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/delay.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/vpu.h>
#if 0
#ifdef CONFIG_SMP
#include <mach/smp.h>
#endif
#endif
#include <linux/amlogic/vout/vinfo.h>
#include "vpu_reg.h"
#include "vpu.h"

#define VPU_VERION        "v01"

static spinlock_t vpu_lock;
static spinlock_t vpu_mem_lock;
static DEFINE_MUTEX(vpu_mutex);

static unsigned int clk_vmod[50];

static struct VPU_Conf_t vpu_conf = {
	.mem_pd0 = 0,
	.mem_pd1 = 0,
	.chip_type = VPU_CHIP_MAX,
	.clk_level_dft = 0,
	.clk_level_max = 1,
	.clk_level = 0,
};

static enum vpu_mod_t get_vpu_mod(unsigned int vmod)
{
	unsigned int vpu_mod = VPU_MAX;
	if (vmod < VMODE_MAX) {
		switch (vmod) {
		case VMODE_480I:
		case VMODE_480I_RPT:
		case VMODE_576I:
		case VMODE_576I_RPT:
		case VMODE_480CVBS:
		case VMODE_576CVBS:
			vpu_mod = VPU_VENCI;
			break;
		case VMODE_LCD:
		case VMODE_LVDS_1080P:
		case VMODE_LVDS_1080P_50HZ:
			vpu_mod = VPU_VENCL;
			break;
		default:
			vpu_mod = VPU_VENCP;
			break;
		}
	} else if ((vmod >= VPU_MOD_START) && (vmod < VPU_MAX)) {
		vpu_mod = vmod;
	} else {
		vpu_mod = VPU_MAX;
	}
	return vpu_mod;
}

#ifdef CONFIG_VPU_DYNAMIC_ADJ
static unsigned int get_vpu_clk_level_max_vmod(void)
{
	unsigned int max_level;
	int i;

	max_level = 0;
	for (i = VPU_MOD_START; i < VPU_MAX; i++) {
		if (clk_vmod[i-VPU_MOD_START] > max_level)
			max_level = clk_vmod[i-VPU_MOD_START];
	}

	return max_level;
}
#endif

static unsigned int get_vpu_clk_level(unsigned int video_clk)
{
	unsigned int video_bw;
	unsigned clk_level;
	int i;

	video_bw = video_clk + 1000000;

	for (i = 0; i < vpu_conf.clk_level_max; i++) {
		if (video_bw <= vpu_clk_table[i][0])
			break;
	}
	clk_level = i;

	return clk_level;
}

unsigned int get_vpu_clk(void)
{
	unsigned int clk_freq;
	unsigned int clk_source, clk_div;

	switch (vpu_reg_getb(HHI_VPU_CLK_CNTL, 9, 3)) {
	case 0:
		clk_source = 637500000;
		break;
	case 1:
		clk_source = 850000000;
		break;
	case 2:
		clk_source = 510000000;
		break;
	case 3:
		if (vpu_conf.chip_type == VPU_CHIP_M8M2)
			clk_source = 364000000;
		else
			clk_source = 364300000;
		break;
	case 7:
		if (vpu_conf.chip_type == VPU_CHIP_G9TV)
			clk_source = 696000000;
		else
			clk_source = 0;
		break;
	default:
		clk_source = 0;
		break;
	}

	clk_div = vpu_reg_getb(HHI_VPU_CLK_CNTL, 0, 7) + 1;
	clk_freq = clk_source / clk_div;

	return clk_freq;
}

static int switch_gp_pll_m8m2(int flag)
{
	int cnt = 100;
	int ret = 0;

	if (flag) { /* enable gp_pll */
		/* M=182, N=3, OD=2. gp_pll=24*M/N/2^OD=364M */
		/* set gp_pll frequency fixed to 364M */
		vpu_reg_write(HHI_GP_PLL_CNTL, 0x206b6);
		vpu_reg_setb(HHI_GP_PLL_CNTL, 1, 30, 1); /* enable */
		do {
			udelay(10);
			/* reset */
			vpu_reg_setb(HHI_GP_PLL_CNTL, 1, 29, 1);
			udelay(50);
			/* release reset */
			vpu_reg_setb(HHI_GP_PLL_CNTL, 0, 29, 1);
			udelay(50);
			cnt--;
			if (cnt == 0)
				break;
		} while (vpu_reg_getb(HHI_GP_PLL_CNTL, 31, 1) == 0);
		if (cnt == 0) {
			ret = 1;
			vpu_reg_setb(HHI_GP_PLL_CNTL, 0, 30, 1);
			pr_info("[error]: GP_PLL lock failed, can't use the clk source!\n");
		}
	} else { /* disable gp_pll */
		vpu_reg_setb(HHI_GP_PLL_CNTL, 0, 30, 1);
	}

	return ret;
}

static int switch_gp1_pll_g9tv(int flag)
{
	int cnt = 100;
	int ret = 0;

	if (flag) { /* enable gp1_pll */
		/* GP1 DPLL 696MHz output*/
		vpu_reg_write(HHI_GP1_PLL_CNTL, 0x6a01023a);
		vpu_reg_write(HHI_GP1_PLL_CNTL2, 0x69c80000);
		vpu_reg_write(HHI_GP1_PLL_CNTL3, 0x0a5590c4);
		vpu_reg_write(HHI_GP1_PLL_CNTL4, 0x0000500d);
		vpu_reg_write(HHI_GP1_PLL_CNTL, 0x4a01023a);
		do {
			udelay(10);
			/* reset */
			vpu_reg_setb(HHI_GP1_PLL_CNTL, 1, 29, 1);
			udelay(50);
			/* release reset */
			vpu_reg_setb(HHI_GP1_PLL_CNTL, 0, 29, 1);
			udelay(50);
			cnt--;
			if (cnt == 0)
				break;
		} while (vpu_reg_getb(HHI_GP1_PLL_CNTL, 31, 1) == 0);
		if (cnt == 0) {
			ret = 1;
			vpu_reg_setb(HHI_GP1_PLL_CNTL, 0, 30, 1);
			pr_info("[error]: GP_PLL lock failed, can't use the clk source!\n");
		}
	} else { /* disable gp1_pll */
		vpu_reg_setb(HHI_GP1_PLL_CNTL, 0, 30, 1);
	}

	return ret;
}

static int change_vpu_clk(void *vconf1)
{
	unsigned int clk_level, temp;
	struct VPU_Conf_t *vconf = (struct VPU_Conf_t *)vconf1;

	clk_level = vconf->clk_level;
	temp = (vpu_clk_table[clk_level][1] << 9) |
		(vpu_clk_table[clk_level][2] << 0);
	vpu_reg_write(HHI_VPU_CLK_CNTL, ((1 << 8) | temp));

	return 0;
}

static void switch_vpu_clk(void)
{
	unsigned int temp;

	/* switch to second vpu clk patch */
	temp = vpu_clk_table[0][1];
	vpu_reg_setb(HHI_VPU_CLK_CNTL, temp, 25, 3);
	temp = vpu_clk_table[0][2];
	vpu_reg_setb(HHI_VPU_CLK_CNTL, temp, 16, 7);
	vpu_reg_setb(HHI_VPU_CLK_CNTL, 1, 24, 1);
	vpu_reg_setb(HHI_VPU_CLK_CNTL, 1, 31, 1);
	udelay(10);
	/* adjust first vpu clk frequency */
	vpu_reg_setb(HHI_VPU_CLK_CNTL, 0, 8, 1);
	temp = vpu_clk_table[vpu_conf.clk_level][1];
	vpu_reg_setb(HHI_VPU_CLK_CNTL, temp, 9, 3);
	temp = vpu_clk_table[vpu_conf.clk_level][2];
	vpu_reg_setb(HHI_VPU_CLK_CNTL, temp, 0, 7);
	vpu_reg_setb(HHI_VPU_CLK_CNTL, 1, 8, 1);
	udelay(20);
	/* switch back to first vpu clk patch */
	vpu_reg_setb(HHI_VPU_CLK_CNTL, 0, 31, 1);
	vpu_reg_setb(HHI_VPU_CLK_CNTL, 0, 24, 1);
}

static int adjust_vpu_clk(unsigned int clk_level)
{
	unsigned long flags = 0;
	int ret = 0;

	if (vpu_conf.chip_type == VPU_CHIP_MAX) {
		pr_info("invalid VPU in current CPU type\n");
		return 0;
	}

	spin_lock_irqsave(&vpu_lock, flags);

	vpu_conf.clk_level = clk_level;
	switch (vpu_conf.chip_type) {
	case VPU_CHIP_M8:
#ifdef CONFIG_SMP
		/* try_exclu_cpu_exe(change_vpu_clk, &vpu_conf); */
		change_vpu_clk(&vpu_conf);
#else
		change_vpu_clk(&vpu_conf);
#endif
		break;
	case VPU_CHIP_M8M2:
		if (clk_level == 6) {
			ret = switch_gp_pll_m8m2(1);
			if (ret) {
				clk_level = 5;
				vpu_conf.clk_level = clk_level;
			}
		} else
			ret = switch_gp_pll_m8m2(0);

		switch_vpu_clk();
		break;
	case VPU_CHIP_G9TV:
		if (clk_level == 10) {
			ret = switch_gp1_pll_g9tv(1);
			if (ret) {
				clk_level = 9;
				vpu_conf.clk_level = clk_level;
			}
		} else
			ret = switch_gp1_pll_g9tv(0);

		switch_vpu_clk();
		break;
	case VPU_CHIP_M8B:
	case VPU_CHIP_G9BB:
		switch_vpu_clk();
		break;
	default:
		break;
	}

	if (vpu_reg_getb(HHI_VPU_CLK_CNTL, 8, 1) == 0)
		vpu_reg_setb(HHI_VPU_CLK_CNTL, 1, 8, 1);

	pr_info("set vpu clk: %uHz, readback: %uHz(0x%x)\n",
			vpu_clk_table[clk_level][0],
			get_vpu_clk(), (vpu_reg_read(HHI_VPU_CLK_CNTL)));

	spin_unlock_irqrestore(&vpu_lock, flags);
	return ret;
}

static int set_vpu_clk(unsigned int vclk)
{
	int ret = 0;
	unsigned int clk_level, mux, div;
	mutex_lock(&vpu_mutex);

	if (vclk >= 100) { /* regard as vpu_clk */
		clk_level = get_vpu_clk_level(vclk);
	} else { /* regard as clk_level */
		clk_level = vclk;
	}

	if (clk_level >= vpu_conf.clk_level_max) {
		ret = 1;
		pr_info("set vpu clk out of supported range\n");
		goto set_vpu_clk_limit;
	}
#ifdef LIMIT_VPU_CLK_LOW
	else if (clk_level < vpu_conf.clk_level_dft) {
		ret = 3;
		pr_info("set vpu clk less than system default\n");
		goto set_vpu_clk_limit;
	}
#endif

	mux = vpu_reg_getb(HHI_VPU_CLK_CNTL, 9, 3);
	div = vpu_reg_getb(HHI_VPU_CLK_CNTL, 0, 7);
	if ((mux != vpu_clk_table[clk_level][1])
		|| (div != vpu_clk_table[clk_level][2])) {
		adjust_vpu_clk(clk_level);
	}

set_vpu_clk_limit:
	mutex_unlock(&vpu_mutex);
	return ret;
}

/* *********************************************** */
/* VPU_CLK control */
/* *********************************************** */
/*
 *  Function: get_vpu_clk_vmod
 *      Get vpu clk holding frequency with specified vmod
 *
 *      Parameters:
 *      vmod - unsigned int, must be one of the following constants:
 *                 VMODE, VMODE is supported by VOUT
 *                 VPU_MOD, supported by vpu_mod_t
 *
 *  Returns:
 *      unsigned int, vpu clk frequency unit in Hz
 *
 *      Example:
 *      video_clk = get_vpu_clk_vmod(VMODE_720P);
 *      video_clk = get_vpu_clk_vmod(VPU_VIU_OSD1);
 *
*/
unsigned int get_vpu_clk_vmod(unsigned int vmod)
{
	unsigned int vpu_mod;
	unsigned int vpu_clk;
	mutex_lock(&vpu_mutex);

	vpu_mod = get_vpu_mod(vmod);
	if ((vpu_mod >= VPU_MOD_START) && (vpu_mod < VPU_MAX)) {
		vpu_clk = clk_vmod[vpu_mod - VPU_MOD_START];
		vpu_clk = vpu_clk_table[vpu_clk][0];
	} else {
		vpu_clk = 0;
		pr_info("unsupport vmod\n");
	}

	mutex_unlock(&vpu_mutex);
	return vpu_clk;
}

/*
 *  Function: request_vpu_clk_vmod
 *      Request a new vpu clk holding frequency with specified vmod
 *      Will change vpu clk if the max level in all vmod vpu clk holdings
 *      is unequal to current vpu clk level
 *
 *  Parameters:
 *      vclk - unsigned int, vpu clk frequency unit in Hz
 *      vmod - unsigned int, must be one of the following constants:
 *                 VMODE, VMODE is supported by VOUT
 *                 VPU_MOD, supported by vpu_mod_t
 *
 *  Returns:
 *      int, 0 for success, 1 for failed
 *
 *  Example:
 *      ret = request_vpu_clk_vmod(100000000, VMODE_720P);
 *      ret = request_vpu_clk_vmod(300000000, VPU_VIU_OSD1);
 *
*/
int request_vpu_clk_vmod(unsigned int vclk, unsigned int vmod)
{
	int ret = 0;
#ifdef CONFIG_VPU_DYNAMIC_ADJ
	unsigned clk_level;
	unsigned vpu_mod;

	mutex_lock(&vpu_mutex);

	if (vclk >= 100) { /* regard as vpu_clk */
		clk_level = get_vpu_clk_level(vclk);
	} else { /* regard as clk_level */
		clk_level = vclk;
	}

	if (clk_level >= vpu_conf.clk_level_max) {
		ret = 1;
		pr_info("set vpu clk out of supported range\n");
		goto request_vpu_clk_limit;
	}

	vpu_mod = get_vpu_mod(vmod);
	if (vpu_mod == VPU_MAX) {
		ret = 2;
		pr_info("unsupport vmod\n");
		goto request_vpu_clk_limit;
	}

	clk_vmod[vpu_mod - VPU_MOD_START] = clk_level;
	pr_info("request vpu clk: %s %uHz\n",
			vpu_mod_table[vpu_mod - VPU_MOD_START],
			vpu_clk_table[clk_level][0]);

	clk_level = get_vpu_clk_level_max_vmod();
	if (clk_level != vpu_conf.clk_level)
		adjust_vpu_clk(clk_level);

request_vpu_clk_limit:
	mutex_unlock(&vpu_mutex);
#endif
	return ret;
}

/*
 *  Function: release_vpu_clk_vmod
 *      Release vpu clk holding frequency to 0 with specified vmod
 *      Will change vpu clk if the max level in all vmod vpu clk holdings
 *      is unequal to current vpu clk level
 *
 *  Parameters:
 *      vmod - unsigned int, must be one of the following constants:
 *                 VMODE, VMODE is supported by VOUT
 *                 VPU_MOD, supported by vpu_mod_t
 *
 *  Returns:
 *      int, 0 for success, 1 for failed
 *
 *  Example:
 *      ret = release_vpu_clk_vmod(VMODE_720P);
 *      ret = release_vpu_clk_vmod(VPU_VIU_OSD1);
 *
*/
int release_vpu_clk_vmod(unsigned int vmod)
{
	int ret = 0;
#ifdef CONFIG_VPU_DYNAMIC_ADJ
	unsigned clk_level;
	unsigned vpu_mod;

	mutex_lock(&vpu_mutex);

	clk_level = 0;
	vpu_mod = get_vpu_mod(vmod);
	if (vpu_mod == VPU_MAX) {
		ret = 2;
		pr_info("unsupport vmod\n");
		goto release_vpu_clk_limit;
	}

	clk_vmod[vpu_mod - VPU_MOD_START] = clk_level;
	pr_info("release vpu clk: %s\n",
		vpu_mod_table[vpu_mod - VPU_MOD_START]);

	clk_level = get_vpu_clk_level_max_vmod();
	if (clk_level != vpu_conf.clk_level)
		adjust_vpu_clk(clk_level);

release_vpu_clk_limit:
	mutex_unlock(&vpu_mutex);
#endif
	return ret;
}

/* *********************************************** */
/* VPU_MEM_PD control */
/* *********************************************** */
/*
 *  Function: switch_vpu_mem_pd_vmod
 *      switch vpu memory power down by specified vmod
 *
 *  Parameters:
 *      vmod - unsigned int, must be one of the following constants:
 *                 VMODE, VMODE is supported by VOUT
 *                 VPU_MOD, supported by vpu_mod_t
 *      flag - int, on/off switch flag, must be one of the following constants:
 *                 VPU_MEM_POWER_ON
 *                 VPU_MEM_POWER_DOWN
 *
 *  Example:
 *      switch_vpu_mem_pd_vmod(VMODE_720P, VPU_MEM_POWER_ON);
 *      switch_vpu_mem_pd_vmod(VPU_VIU_OSD1, VPU_MEM_POWER_DOWN);
 *
*/
void switch_vpu_mem_pd_vmod(unsigned int vmod, int flag)
{
	unsigned vpu_mod;
	unsigned mem_bit = 0;
	unsigned long flags = 0;
	unsigned int _reg0;
	unsigned int _reg1;

	if (vpu_conf.chip_type == VPU_CHIP_MAX) {
		pr_info("invalid VPU in current CPU type\n");
		return;
	}

	spin_lock_irqsave(&vpu_mem_lock, flags);

	flag = (flag > 0) ? 1 : 0;
	_reg0 = HHI_VPU_MEM_PD_REG0;
	_reg1 = HHI_VPU_MEM_PD_REG1;
	vpu_mod = get_vpu_mod(vmod);
	if ((vpu_mod >= VPU_VIU_OSD1) && (vpu_mod <= VPU_DI_POST)) {
		mem_bit = (vpu_mod - VPU_VIU_OSD1) * 2;
		if (flag)
			vpu_reg_setb(_reg0, 0x3, mem_bit, 2);
		else
			vpu_reg_setb(_reg0, 0, mem_bit, 2);
	} else if (vpu_mod == VPU_SHARP) {
		if ((vpu_conf.chip_type == VPU_CHIP_G9TV) ||
			(vpu_conf.chip_type == VPU_CHIP_G9BB)) {
			if (flag)
				vpu_reg_setb(_reg0, 0x3, 30, 2);
			else
				vpu_reg_setb(_reg0, 0, 30, 2);
		}
	} else if ((vpu_mod >= VPU_VIU2_OSD1) &&
			(vpu_mod <= VPU_VIU2_OSD_SCALE)) {
		if (vpu_conf.chip_type != VPU_CHIP_G9TV) {
			mem_bit = (vpu_mod - VPU_VIU2_OSD1) * 2;
			if (flag)
				vpu_reg_setb(_reg1, 0x3, mem_bit, 2);
			else
				vpu_reg_setb(_reg1, 0, mem_bit, 2);
		}
	} else if ((vpu_mod >= VPU_VDIN_AM_ASYNC) &&
			(vpu_mod <= VPU_VPUARB2_AM_ASYNC)) {
		if (vpu_conf.chip_type == VPU_CHIP_G9TV) {
			mem_bit = (vpu_mod - VPU_VDIN_AM_ASYNC + 7) * 2;
			if (flag)
				vpu_reg_setb(_reg1, 0x3, mem_bit, 2);
			else
				vpu_reg_setb(_reg1, 0, mem_bit, 2);
		}
	} else if ((vpu_mod >= VPU_VENCP) && (vpu_mod <= VPU_VENCI)) {
		mem_bit = (vpu_mod - VPU_VENCP + 10) * 2;
		if (flag)
			vpu_reg_setb(_reg1, 0x3, mem_bit, 2);
		else
			vpu_reg_setb(_reg1, 0, mem_bit, 2);
	} else if (vpu_mod == VPU_ISP) {
		if (vpu_conf.chip_type != VPU_CHIP_G9TV) {
			if (flag)
				vpu_reg_setb(_reg1, 0x3, 26, 2);
			else
				vpu_reg_setb(_reg1, 0, 26, 2);
		}
	} else if ((vpu_mod >= VPU_CVD2) && (vpu_mod <= VPU_ATV_DMD)) {
		if ((vpu_conf.chip_type == VPU_CHIP_G9TV) ||
			(vpu_conf.chip_type == VPU_CHIP_G9BB)) {
			mem_bit = (vpu_mod - VPU_CVD2 + 7) * 2;
			if (flag)
				vpu_reg_setb(_reg1, 0x3, mem_bit, 2);
			else
				vpu_reg_setb(_reg1, 0, mem_bit, 2);
		}
	} else if ((vpu_mod >= VPU_VIU_SRSCL) && (vpu_mod <= VPU_REV)) {
		if (vpu_conf.chip_type == VPU_CHIP_G9TV) {
			mem_bit = (vpu_mod - VPU_VIU_SRSCL + 10) * 2;
			if (flag)
				vpu_reg_setb(_reg0, 0x3, mem_bit, 2);
			else
				vpu_reg_setb(_reg0, 0, mem_bit, 2);
		}
	} else if (vpu_mod == VPU_D2D3) {
		if (vpu_conf.chip_type == VPU_CHIP_G9TV) {
			if (flag)
				vpu_reg_setb(_reg1, 0xf, 0, 4);
			else
				vpu_reg_setb(_reg1, 0, 0, 4);
		}
	} else {
		pr_info("switch_vpu_mem_pd: unsupport vpu mod\n");
	}
	/* pr_info("switch_vpu_mem_pd: %s %s\n",
		vpu_mod_table[vpu_mod - VPU_MOD_START],
		((flag > 0) ? "OFF" : "ON")); */
	spin_unlock_irqrestore(&vpu_mem_lock, flags);
}
/* *********************************************** */

int get_vpu_mem_pd_vmod(unsigned int vmod)
{
	unsigned int vpu_mod;
	unsigned int mem_bit = 0;
	unsigned int _reg0;
	unsigned int _reg1;

	if (vpu_conf.chip_type == VPU_CHIP_MAX) {
		pr_info("invalid VPU in current CPU type\n");
		return 0;
	}

	_reg0 = HHI_VPU_MEM_PD_REG0;
	_reg1 = HHI_VPU_MEM_PD_REG1;
	vpu_mod = get_vpu_mod(vmod);
	if ((vpu_mod >= VPU_VIU_OSD1) && (vpu_mod <= VPU_DI_POST)) {
		mem_bit = (vpu_mod - VPU_VIU_OSD1) * 2;
		return (vpu_reg_getb(_reg0, mem_bit, 2) == 0) ?
				VPU_MEM_POWER_ON : VPU_MEM_POWER_DOWN;
	} else if (vpu_mod == VPU_SHARP) {
		if ((vpu_conf.chip_type == VPU_CHIP_G9TV) ||
			(vpu_conf.chip_type == VPU_CHIP_G9BB)) {
			return (vpu_reg_getb(_reg0, 30, 2) == 0) ?
				VPU_MEM_POWER_ON : VPU_MEM_POWER_DOWN;
		} else
			return -1;
	} else if ((vpu_mod >= VPU_VIU2_OSD1) &&
			(vpu_mod <= VPU_VIU2_OSD_SCALE)) {
		if (vpu_conf.chip_type != VPU_CHIP_G9TV) {
			mem_bit = (vpu_mod - VPU_VIU2_OSD1) * 2;
			return (vpu_reg_getb(_reg1, mem_bit, 2) == 0) ?
				VPU_MEM_POWER_ON : VPU_MEM_POWER_DOWN;
		} else
			return -1;
	} else if ((vpu_mod >= VPU_VDIN_AM_ASYNC) &&
			(vpu_mod <= VPU_VPUARB2_AM_ASYNC)) {
		if (vpu_conf.chip_type == VPU_CHIP_G9TV) {
			mem_bit = (vpu_mod - VPU_VDIN_AM_ASYNC + 7) * 2;
			return (vpu_reg_getb(_reg1, mem_bit, 2) == 0) ?
				VPU_MEM_POWER_ON : VPU_MEM_POWER_DOWN;
		} else
			return -1;
	} else if ((vpu_mod >= VPU_VENCP) && (vpu_mod <= VPU_VENCI)) {
		mem_bit = (vpu_mod - VPU_VENCP + 10) * 2;
		return (vpu_reg_getb(_reg1, mem_bit, 2) == 0) ?
			VPU_MEM_POWER_ON : VPU_MEM_POWER_DOWN;
	} else if (vpu_mod == VPU_ISP) {
		if (vpu_conf.chip_type != VPU_CHIP_G9TV) {
			return (vpu_reg_getb(_reg1, 26, 2) == 0) ?
				VPU_MEM_POWER_ON : VPU_MEM_POWER_DOWN;
		} else
			return -1;
	} else if ((vpu_mod >= VPU_CVD2) && (vpu_mod <= VPU_ATV_DMD)) {
		if ((vpu_conf.chip_type == VPU_CHIP_G9TV) ||
			(vpu_conf.chip_type == VPU_CHIP_G9BB)) {
			mem_bit = (vpu_mod - VPU_CVD2 + 7) * 2;
			return (vpu_reg_getb(_reg1, mem_bit, 2) == 0) ?
				VPU_MEM_POWER_ON : VPU_MEM_POWER_DOWN;
		} else
			return -1;
	} else if ((vpu_mod >= VPU_VIU_SRSCL) && (vpu_mod <= VPU_REV)) {
		if (vpu_conf.chip_type == VPU_CHIP_G9TV) {
			mem_bit = (vpu_mod - VPU_VIU_SRSCL + 10) * 2;
			return (vpu_reg_getb(_reg0, mem_bit, 2) == 0) ?
				VPU_MEM_POWER_ON : VPU_MEM_POWER_DOWN;
		} else
			return -1;
	} else if (vpu_mod == VPU_D2D3) {
		if (vpu_conf.chip_type == VPU_CHIP_G9TV) {
			return (vpu_reg_getb(_reg1, 0, 4) == 0) ?
				VPU_MEM_POWER_ON : VPU_MEM_POWER_DOWN;
		} else
			return -1;
	} else {
		return -1;
	}
}

/* *********************************************** */
/* VPU sysfs function */
/* *********************************************** */
static const char *vpu_usage_str = {
"Usage:\n"
"	echo get > clk ; print current vpu clk\n"
"	echo set <vclk> > clk ; force to set vpu clk\n"
"	echo dump [vmod] > clk ; dump vpu clk by vmod, [vmod] is unnecessary\n"
"	echo request <vclk> <vmod> > clk ; request vpu clk holding by vmod\n"
"	echo release <vmod> > clk ; release vpu clk holding by vmod\n"
"\n"
"	request & release will change vpu clk if the max level in all vmod vpu clk holdings is unequal to current vpu clk level.\n"
"	vclk both support level(1~10) and frequency value (unit in Hz).\n"
"	vclk level & frequency:\n"
"		0: 106.25M        1: 159.375M        2: 182.15M\n"
"		3: 212.50M        4: 255.00M         5: 318.75M\n"
"		6: 364.30M        7: 425.00M         8: 510.00M\n"
"		9: 637.50M        10:696.00M\n"
};

static ssize_t vpu_debug_help(struct class *class,
				struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", vpu_usage_str);
}

static ssize_t vpu_debug(struct class *class, struct class_attribute *attr,
				const char *buf, size_t count)
{
	unsigned int ret;
	int i;
	unsigned int tmp[2], temp;

	switch (buf[0]) {
	case 'g': /* get */
		pr_info("get current vpu clk: %uHz\n", get_vpu_clk());
		break;
	case 's': /* set */
		tmp[0] = 4;
		ret = sscanf(buf, "set %u", &tmp[0]);
		if (tmp[0] > 100)
			pr_info("set vpu clk frequency: %uHz\n", tmp[0]);
		else
			pr_info("set vpu clk level: %u\n", tmp[0]);
		set_vpu_clk(tmp[0]);
		break;
	case 'r':
		if (buf[2] == 'q') { /* request */
			tmp[0] = 0;
			tmp[1] = VPU_MAX;
			ret = sscanf(buf, "request %u %u", &tmp[0], &tmp[1]);
			request_vpu_clk_vmod(tmp[0], tmp[1]);
		} else if (buf[2] == 'l') { /* release */
			tmp[0] = VPU_MAX;
			ret = sscanf(buf, "release %u", &tmp[0]);
			release_vpu_clk_vmod(tmp[0]);
		}
		break;
	case 'd':
		tmp[0] = VPU_MAX;
		ret = sscanf(buf, "dump %u", &tmp[0]);
		tmp[1] = get_vpu_mod(tmp[0]);
		pr_info("vpu clk holdings:\n");
		if (tmp[1] == VPU_MAX) {
			for (i = VPU_MOD_START; i < VPU_MAX; i++) {
				temp = i - VPU_MOD_START;
				pr_info("%s:  %uHz(%u)\n", vpu_mod_table[temp],
				vpu_clk_table[clk_vmod[temp]][0],
				clk_vmod[temp]);
			}
		} else {
			temp = tmp[1] - VPU_MOD_START;
			pr_info("%s:  %uHz(%u)\n", vpu_mod_table[temp],
			vpu_clk_table[clk_vmod[temp]][0],
			clk_vmod[temp]);
		}
		break;
	default:
		pr_info("wrong format of vpu debug command.\n");
		break;
	}

	if (ret != 1 || ret != 2)
		return -EINVAL;

	return count;
	/* return 0; */
}

static struct class_attribute vpu_debug_class_attrs[] = {
	__ATTR(clk, S_IRUGO | S_IWUSR, vpu_debug_help, vpu_debug),
	__ATTR(help, S_IRUGO | S_IWUSR, vpu_debug_help, NULL),
	__ATTR_NULL
};

static struct class aml_vpu_debug_class = {
	.name = "vpu",
	.class_attrs = vpu_debug_class_attrs,
};
/* ********************************************************* */
#if 0
static void vpu_driver_init(void)
{
	set_vpu_clk(vpu_conf.clk_level);

	vpu_ao_setb(AO_RTI_GEN_PWR_SLEEP0, 0, 8, 1); /* [8] power on */
	vpu_reg_write(HHI_VPU_MEM_PD_REG0, vpu_conf.mem_pd0);
	vpu_reg_write(HHI_VPU_MEM_PD_REG1, vpu_conf.mem_pd1);
	vpu_reg_setb(HHI_MEM_PD_REG0, 0, 8, 8); /* MEM-PD */
	udelay(2);

	/* Reset VIU + VENC */
	/* Reset VENCI + VENCP + VADC + VENCL */
	/* Reset HDMI-APB + HDMI-SYS + HDMI-TX + HDMI-CEC */
	vpu_clr_mask(RESET0_MASK, ((1 << 5) | (1<<10)));
	vpu_clr_mask(RESET4_MASK, ((1 << 6) | (1<<7) | (1<<9) | (1<<13)));
	vpu_clr_mask(RESET2_MASK, ((1 << 2) | (1<<3) | (1<<11) | (1<<15)));
	vpu_reg_write(RESET2_REGISTER, ((1 << 2) | (1<<3) | (1<<11) | (1<<15)));
	/* reset this will cause VBUS reg to 0 */
	vpu_reg_write(RESET4_REGISTER, ((1 << 6) | (1<<7) | (1<<9) | (1<<13)));
	vpu_reg_write(RESET0_REGISTER, ((1 << 5) | (1<<10)));
	vpu_reg_write(RESET4_REGISTER, ((1 << 6) | (1<<7) | (1<<9) | (1<<13)));
	vpu_reg_write(RESET2_REGISTER, ((1 << 2) | (1<<3) | (1<<11) | (1<<15)));
	vpu_set_mask(RESET0_MASK, ((1 << 5) | (1<<10)));
	vpu_set_mask(RESET4_MASK, ((1 << 6) | (1<<7) | (1<<9) | (1<<13)));
	vpu_set_mask(RESET2_MASK, ((1 << 2) | (1<<3) | (1<<11) | (1<<15)));

	/* Remove VPU_HDMI ISO */
	vpu_ao_setb(AO_RTI_GEN_PWR_SLEEP0, 0, 9, 1); /* [9] VPU_HDMI */
}
static void vpu_driver_disable(void)
{
	vpu_conf.mem_pd0 = vpu_reg_read(HHI_VPU_MEM_PD_REG0);
	vpu_conf.mem_pd1 = vpu_reg_read(HHI_VPU_MEM_PD_REG1);

	/* Power down VPU_HDMI */
	/* Enable Isolation */
	vpu_ao_setb(AO_RTI_GEN_PWR_SLEEP0, 1, 9, 1); /* ISO */
	/* Power off memory */
	vpu_reg_write(HHI_VPU_MEM_PD_REG0, 0xffffffff);
	vpu_reg_write(HHI_VPU_MEM_PD_REG1, 0xffffffff);
	vpu_reg_setb(HHI_MEM_PD_REG0, 0xff, 8, 8); /* HDMI MEM-PD */

	/* Power down VPU domain */
	vpu_ao_setb(AO_RTI_GEN_PWR_SLEEP0, 1, 8, 1); /* PDN */

	vpu_reg_setb(HHI_VPU_CLK_CNTL, 0, 8, 1);
}
#endif
#ifdef CONFIG_PM
static int vpu_suspend(struct platform_device *pdev, pm_message_t state)
{
	/* vpu_driver_disable(); */
	return 0;
}

static int vpu_resume(struct platform_device *pdev)
{
	/* vpu_driver_init(); */
	return 0;
}
#endif

static void detect_vpu_chip(void)
{
	unsigned int cpu_type;

	cpu_type = get_cpu_type();
	switch (cpu_type) {
	case MESON_CPU_MAJOR_ID_M8:
		vpu_conf.chip_type = VPU_CHIP_M8;
		vpu_conf.clk_level_dft = CLK_LEVEL_DFT_M8;
		vpu_conf.clk_level_max = CLK_LEVEL_MAX_M8;
		break;
	case MESON_CPU_MAJOR_ID_M8B:
		vpu_conf.chip_type = VPU_CHIP_M8B;
		vpu_conf.clk_level_dft = CLK_LEVEL_DFT_M8B;
		vpu_conf.clk_level_max = CLK_LEVEL_MAX_M8B;
		break;
	case MESON_CPU_MAJOR_ID_M8M2:
		vpu_conf.chip_type = VPU_CHIP_M8M2;
		vpu_conf.clk_level_dft = CLK_LEVEL_DFT_M8M2;
		vpu_conf.clk_level_max = CLK_LEVEL_MAX_M8M2;
		break;
	case MESON_CPU_MAJOR_ID_MG9TV:
		vpu_conf.chip_type = VPU_CHIP_G9TV;
		vpu_conf.clk_level_dft = CLK_LEVEL_DFT_G9TV;
		vpu_conf.clk_level_max = CLK_LEVEL_MAX_G9TV;
		break;
	/* case MESON_CPU_MAJOR_ID_MG9BB:
		vpu_conf.chip_type = VPU_CHIP_G9BB;
		vpu_conf.clk_level_dft = CLK_LEVEL_DFT_G9BB;
		vpu_conf.clk_level_max = CLK_LEVEL_MAX_G9BB;
		break; */
	case MESON_CPU_MAJOR_ID_GXBB:
		vpu_conf.chip_type = VPU_CHIP_GXBB;
		vpu_conf.clk_level_dft = CLK_LEVEL_DFT_GXBB;
		vpu_conf.clk_level_max = CLK_LEVEL_MAX_GXBB;
		break;
	default:
		vpu_conf.chip_type = VPU_CHIP_MAX;
		vpu_conf.clk_level_dft = 0;
		vpu_conf.clk_level_max = 1;
	}

	pr_info("vpu driver detect cpu type: %s\n",
			vpu_chip_name[vpu_conf.chip_type]);
}

static int get_vpu_config(struct platform_device *pdev)
{
	int ret = 0;
	unsigned int val;
	struct device_node *vpu_np;

	vpu_np = pdev->dev.of_node;
	if (!vpu_np) {
		pr_info("don't find match vpu node\n");
		return -1;
	}

	ret = of_property_read_u32(vpu_np, "clk_level", &val);
	if (ret) {
		pr_info("don't find to match clk_level, use default setting.\n");
	} else {
		if (val >= vpu_conf.clk_level_max) {
			pr_info("vpu clk_level in dts is out of support range, use default setting\n");
			val = vpu_conf.clk_level_dft;
		}

		vpu_conf.clk_level = val;
		pr_info("load vpu_clk in dts: %uHz(%u)\n",
			vpu_clk_table[val][0],
			vpu_conf.clk_level);
	}

	return ret;
}

static struct of_device_id vpu_of_table[] = {
	{
		.compatible = "amlogic, vpu",
	},
	{},
};

static int vpu_probe(struct platform_device *pdev)
{
	int ret;

	spin_lock_init(&vpu_lock);
	spin_lock_init(&vpu_mem_lock);

	pr_info("VPU driver version: %s\n", VPU_VERION);
	memset(clk_vmod, 0, sizeof(clk_vmod));
	detect_vpu_chip();
	if (vpu_conf.chip_type == VPU_CHIP_MAX) {
		pr_info("invalid VPU in current CPU type\n");
		return 0;
	}

	get_vpu_config(pdev);
	set_vpu_clk(vpu_conf.clk_level);

	ret = class_register(&aml_vpu_debug_class);
	if (ret)
		pr_info("class register aml_vpu_debug_class fail!\n");

	pr_info("%s OK\n", __func__);
	return 0;
}

static int vpu_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver vpu_driver = {
	.driver = {
		.name = "vpu",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(vpu_of_table),
	},
	.probe = vpu_probe,
	.remove = vpu_remove,
#ifdef CONFIG_PM
	.suspend    = vpu_suspend,
	.resume     = vpu_resume,
#endif
};

static int __init vpu_init(void)
{
	return platform_driver_register(&vpu_driver);
}

static void __exit vpu_exit(void)
{
	platform_driver_unregister(&vpu_driver);
}

postcore_initcall(vpu_init);
module_exit(vpu_exit);

MODULE_DESCRIPTION("meson vpu driver");
MODULE_LICENSE("GPL v2");
