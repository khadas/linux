// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_fdt.h>
#include <linux/of_reserved_mem.h>
#include <linux/of_irq.h>
#include <linux/string.h>
#include <linux/time.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/errno.h>
#include <linux/uaccess.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/ctype.h>
#include <linux/vmalloc.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/time.h>
//#include <asm/div64.h>
//#include <linux/sched/clock.h
//#include <linux/amlogic/media/vpu/vpu.h>
//#include <linux/amlogic/media/vfm/vframe.h>
//#include <linux/amlogic/media/vfm/vframe_provider.h>
//#include <linux/amlogic/media/vfm/vframe_receiver.h>
//#include <linux/amlogic/media/frame_sync/timestamp.h>
//#include <linux/amlogic/media/frame_sync/tsync.h>
#include "frc_common.h"
#include "frc_drv.h"
#include "frc_scene.h"
#include "frc_bbd.h"
#include "frc_vp.h"
#include "frc_proc.h"
#include "frc_logo.h"
#include "frc_film.h"
#include "frc_me.h"
#include "frc_mc.h"
#include "frc_hw.h"
#include "frc_reg.h"

struct st_cadence_table_item g_stCadence_table[] = {
	{EN_VIDEO,		0x1,		0x1,		0x1,		0x1,		0x01,		1,		0},
	{EN_FILM22,		0x3,		0x2,		0x2,		0x1,		0x01,		2,		0},
	{EN_FILM32,		0x1F,		0x14,		0x5,		0x2,		0x03,		2,		0},
	{EN_FILM3223,		0x3FF,		0x2A4,		0xA,		0x4,		0x06,		2,		0},
	{EN_FILM2224,		0x3FF,		0x2A8,		0xA,		0x4,		0x06,		2,		0},
	{EN_FILM32322,		0xFFF,		0xA94,		0xC,		0x5,		0x07,		2,		0},
	{EN_FILM44,		0xF,		0x8,		0x4,		0x1,		0x03,		4,		0},
	{EN_FILM21111,		0x3F,		0x3E,		0x6,		0x5,		0x01,		1,		0},
	{EN_FILM23322,		0xFFF,		0xAA4,		0xC,		0x5,		0x07,		2,		0},
	{EN_FILM2111,		0x1F,		0x1E,		0x5,		0x4,		0x01,		1,		0},
	{EN_FILM22224,		0xFFF,		0xAA8,		0xC,		0x5,		0x07,		2,		0},
	{EN_FILM33,         	0x7,		0x4,		0x3,		0x1,		0x02,		2,		0},
	{EN_FILM334,        	0x3ff,      0x248,      0xa,		0x3,		0x07,		2,		0},
	{EN_FILM55,         	0x1f,       0x10,       0x5,		0x1,		0x04,		2,		0},
	{EN_FILM64,         	0x3ff,      0x220,      0xa,		0x2,		0x08,		2,		0},
	{EN_FILM66,         	0x3f,       0x20,       0x6,		0x1,		0x05,		2,		0},
	{EN_FILM87,         	0x7fff,     0x4080,     0xf,		0x2,		0x0D,		2,		0},
	{EN_FILM212,		0x1f,       0x1A,       0x5,		0x3,		0x02,		2,		0},
};

struct st_phase_table_item  g_stPhase_table_Item;
u8 Incr_phase[64];
u8 ip_film_end[64];

void sel_use_case(void)
{
	u8 i, shift_i, matchdata;

	//set frm delay mode
	g_stPhase_table_Item.Frm_delay_mode = EN_DELAY_NORMAL;

	//for phase fallback table
	if(g_stPhase_table_Item.gen_phase_fallback_table == 1)
	{
		g_stPhase_table_Item.phase_out_div2 = 1;
	}

	g_stPhase_table_Item.gen_one_mode = 1;

	//set ip film end
	matchdata = g_stCadence_table[g_stPhase_table_Item.film_mode].match_data;

	for(i=0; i<g_stPhase_table_Item.cadence_cnt;i++)
	{
		shift_i = (i + 1)%g_stPhase_table_Item.cadence_cnt;
		ip_film_end[i] = (matchdata>>(g_stPhase_table_Item.cadence_cnt-1-shift_i))&0x1;
	}
}

void get_table_count(void)
{
	//calculate table cnt
	if((g_stPhase_table_Item.output_m*g_stPhase_table_Item.cadence_cnt%g_stPhase_table_Item.input_n)!=0)
	{
		g_stPhase_table_Item.table_cnt = g_stPhase_table_Item.output_m*g_stPhase_table_Item.cadence_cnt;
	}
	else
	{
		g_stPhase_table_Item.table_cnt = g_stPhase_table_Item.output_m*g_stPhase_table_Item.cadence_cnt/g_stPhase_table_Item.input_n;
	}

	g_stPhase_table_Item.table_cnt = (g_stPhase_table_Item.phase_out_div2==1&&g_stPhase_table_Item.table_cnt%2!=0)? g_stPhase_table_Item.table_cnt<<1:g_stPhase_table_Item.table_cnt;

	//calculate mode cnt
	if(g_stPhase_table_Item.input_n!=1)
	{
		g_stPhase_table_Item.mode_cnt = (g_stPhase_table_Item.cadence_cnt < g_stPhase_table_Item.input_n)?g_stPhase_table_Item.cadence_cnt:g_stPhase_table_Item.input_n;
	}
	else
	{
		g_stPhase_table_Item.mode_cnt = 1;
	}

}

void set_frc_step(void)
{
	u8 i, next_idx, i_next_idx;
	u8 cadence_length_by_input_n = g_stPhase_table_Item.cadence_length*g_stPhase_table_Item.input_n;
	u8 cadence_cnt_by_output_m = g_stPhase_table_Item.cadence_cnt*g_stPhase_table_Item.output_m;

	//mc phase
	for(i=0; i < g_stPhase_table_Item.table_cnt; i++)
	{
		g_stPhase_table_Item.mc_phase_step[i] = (128*i*cadence_length_by_input_n/cadence_cnt_by_output_m)%128;

		if((g_stPhase_table_Item.phase_out_div2 ==1)&&(i%2!=0))
		{
			g_stPhase_table_Item.mc_phase_step[i] = g_stPhase_table_Item.mc_phase_step[i-1];
		}
	}

	//me phase
	next_idx = ((g_stPhase_table_Item.phase_out_div2 ==1)? 2: 1);
	i_next_idx = 0;
	for(i=0; i< g_stPhase_table_Item.table_cnt; i++)
	{
		if(g_stPhase_table_Item.mc_phase_step[i] == 0)
		{
			i_next_idx = (i + next_idx > g_stPhase_table_Item.table_cnt)?(i + next_idx - g_stPhase_table_Item.table_cnt):(i + next_idx);
			g_stPhase_table_Item.me_phase_step[i] = g_stPhase_table_Item.mc_phase_step[i_next_idx];
		}
		else
		{
			g_stPhase_table_Item.me_phase_step[i] = g_stPhase_table_Item.mc_phase_step[i];
		}
	}
}

void set_addr_lut(void)
{
	u8 i=0, j=0, tb_idx=0;
	u8 idx, delay_mode_shift;
	u8 film_phase_org[65],frc_phase_org[65], A1A2_change_flag_org[65], AB_change_flag_org[65], pre_film_phase_org[65];
	u8 A1A2_change_temp = 0;
	u32 matchdata;

	A1A2_change_flag_org[0]=1;

	//get film phase ,frc phase ,A1A2 change
	while(tb_idx < g_stPhase_table_Item.table_cnt)
	{
		if(((j*g_stPhase_table_Item.input_n + g_stPhase_table_Item.mode_shift) >= (i * g_stPhase_table_Item.output_m))
		&&((j*g_stPhase_table_Item.input_n + g_stPhase_table_Item.mode_shift) < ((i+1) * g_stPhase_table_Item.output_m)))
		{
			film_phase_org[tb_idx] = i % g_stPhase_table_Item.cadence_cnt;
			frc_phase_org[tb_idx] = (j + g_stPhase_table_Item.frc_phase_shift)%g_stPhase_table_Item.output_m;

			tb_idx ++ ;
			j++;
			A1A2_change_temp = 0;
		}
		else if((j*g_stPhase_table_Item.input_n + g_stPhase_table_Item.mode_shift) >= ((i+1)*g_stPhase_table_Item.output_m))
		{
			A1A2_change_temp = 1;
			i++;
		}
		A1A2_change_flag_org[tb_idx] = A1A2_change_temp ;
	}

	//get AB change flag org
	matchdata = g_stCadence_table[g_stPhase_table_Item.film_mode].match_data;

	if(g_stPhase_table_Item.film_mode == EN_VIDEO)
	{
		for(i=0; i<g_stPhase_table_Item.table_cnt; i++)
		{
			AB_change_flag_org[i] = A1A2_change_flag_org[i];
		}
	}
	else
	{
		for(i=0; i<g_stPhase_table_Item.table_cnt; i++)
		{
			pre_film_phase_org[i] = (film_phase_org[i]< 1)?(g_stPhase_table_Item.cadence_cnt -1):(film_phase_org[i]-1);
		}

		for(i=0; i<g_stPhase_table_Item.table_cnt; i++)
		{
			int pre_i = (i < 1)?(g_stPhase_table_Item.table_cnt -1) : (i-1);

			AB_change_flag_org[i] = (pre_film_phase_org[i]==pre_film_phase_org[pre_i])? 0 : (matchdata>>(g_stPhase_table_Item.cadence_cnt - pre_film_phase_org[i] - 1)&0x1);
 		}
	}

	//find the first film phase 1
	tb_idx = 0;	//for video
	if(g_stPhase_table_Item.film_mode!= EN_VIDEO)	//for film
	{
		for(tb_idx=0; tb_idx<g_stPhase_table_Item.table_cnt; tb_idx++)
		{
			if(film_phase_org[tb_idx]==1)
			{
				break;
			}
		}
	}

	//re-order film phase and frc phase according to first phase 1  and also consider the delay mode
	delay_mode_shift = g_stPhase_table_Item.Frm_delay_mode - EN_DELAY_NORMAL;

	for(i=0; i<g_stPhase_table_Item.table_cnt; i++)
	{
		idx = (i+tb_idx+delay_mode_shift >= g_stPhase_table_Item.table_cnt)?(i+tb_idx+delay_mode_shift-g_stPhase_table_Item.table_cnt)
					:((i+tb_idx+delay_mode_shift< 0)?(i+tb_idx+delay_mode_shift+g_stPhase_table_Item.table_cnt):(i+tb_idx+delay_mode_shift));

		g_stPhase_table_Item.film_phase[i] = film_phase_org[idx];
		g_stPhase_table_Item.frc_phase[i] = frc_phase_org[idx];
	}

	//re-order A1A2 change flag and AB change flag according to first phase 1, for later use in data_lut function
	for(i=0; i<g_stPhase_table_Item.table_cnt; i++)
	{
		idx = (i+tb_idx >= g_stPhase_table_Item.table_cnt)?(i+tb_idx-g_stPhase_table_Item.table_cnt):(i+tb_idx);

		g_stPhase_table_Item.A1A2_change_flag[i] = A1A2_change_flag_org[idx];
		g_stPhase_table_Item.AB_change_flag[i] = AB_change_flag_org[idx];
	}
}

void set_data_lut(void)
{
	//set table start , me_start, me_end
	u8 i, j, idx, idx_cnt,next_idx,film_phase_idx;
	u8 pre_idx=0, nex_idx=0;
	//	int shift_film_phase;		//for C diff
	u8 AB_change_flag_temp[65];	//for C diff
	//	int A1A2_change_flag_temp[65];	//for C diff and p diff
	u8 usefrm, frmcnt;
	u8 last_frm_mask[65];
	u8 base_logo_p_diff;
	u8 base_logo_c_diff;
	u8	offset_flag = 1;
	u8	n_table_start_diff =1;
	u32 matchdata;

	g_stPhase_table_Item.table_start[0] = 1;

	if(g_stPhase_table_Item.table_cnt>1)
	{
		for(i=1; i< g_stPhase_table_Item.table_cnt; i++)
		{
			if(g_stPhase_table_Item.mc_phase_step[i] < g_stPhase_table_Item.mc_phase_step[i-1])
			{
				g_stPhase_table_Item.table_start[i]=1;
			}
			else
			{
				g_stPhase_table_Item.table_start[i]=0;
			}
			//xil_printf("tbl=%d\n\r",g_stPhase_table_Item.table_start[i]);
		}
	}

	idx_cnt = (g_stPhase_table_Item.phase_out_div2==1&&g_stPhase_table_Item.gen_phase_fallback_table==0)?2:1;
	next_idx = 0;
	for(i=0; i<g_stPhase_table_Item.table_cnt; i++)
	{
		g_stPhase_table_Item.me_start[i] = g_stPhase_table_Item.table_start[i-i%idx_cnt];
	}

	for(i=0; i<g_stPhase_table_Item.table_cnt; i++)
	{
		next_idx = (i+idx_cnt>=g_stPhase_table_Item.table_cnt)?(i+idx_cnt-g_stPhase_table_Item.table_cnt):(i+idx_cnt);
		g_stPhase_table_Item.me_end[i] = g_stPhase_table_Item.me_start[next_idx];
	}

	//me scan inverse
	idx = 0;
	for(i=0; i<g_stPhase_table_Item.table_cnt; i++)
	{
		if(g_stPhase_table_Item.table_start[i]==1)
		{
			idx=0;
		}
		else
		{
			idx++;
		}
		if(g_stPhase_table_Item.phase_out_div2==1)
		{
			g_stPhase_table_Item.me_scan_inverse[i] = (idx%4 < 2)?0:1;
		}
		else
		{
			g_stPhase_table_Item.me_scan_inverse[i] = (idx%2 < 1)?0:1;
		}
	}

	//lr bit
	for(i=0; i<g_stPhase_table_Item.table_cnt; i++)
	{
		if(g_stPhase_table_Item.phase_out_div2==1&&g_stPhase_table_Item.gen_phase_fallback_table==0)
		{
			g_stPhase_table_Item.lr_bit[i] = (i%2 < 1)?0:1;
		}
		else
		{
			g_stPhase_table_Item.lr_bit[i] = 0;
		}
	}

	//N_diff(next)
	/*j = 0;
	k = 0;

	for(i=0; i<g_stPhase_table_Item.table_cnt; i++)
	{
		if(g_stPhase_table_Item.AB_change_flag[i]==1)
		{
			j++;
		}

		if((j!=0)&&(g_stPhase_table_Item.A1A2_change_flag[i]==1)&&(g_stPhase_table_Item.AB_change_flag[i]==0))
		{
			k++;
		}

		if(g_stPhase_table_Item.table_start[i]==1)
		{
			g_stPhase_table_Item.N_diff[i] = j+k;
			j=0;
			k=0;
		}
		else
		{
			g_stPhase_table_Item.N_diff[i] = (g_stPhase_table_Item.A1A2_change_flag[i]==1)?(g_stPhase_table_Item.N_diff[i-1]+1):g_stPhase_table_Item.N_diff[i-1];
		}
		//printf("phase_n_diff = %4d\n",g_stPhase_table_Item.N_diff[i]);
	}*/

	//N_diff(next)
	offset_flag = 1;
	matchdata = g_stCadence_table[g_stPhase_table_Item.film_mode].match_data;
	g_stPhase_table_Item.N_diff[0] = 1;
	for(i=1; i<g_stPhase_table_Item.table_cnt; i++)
	{
		n_table_start_diff = 1;
		if(g_stPhase_table_Item.table_start[i]==1)
		{
			for(j=offset_flag; j<g_stPhase_table_Item.cadence_cnt; j++)
			{
				if(((matchdata>>(g_stPhase_table_Item.cadence_cnt-j-1))&0x1)==1)
				{
					break;
				}
				else
				{
					n_table_start_diff++;
				}
			}
			offset_flag = (1+j)%g_stPhase_table_Item.cadence_cnt;
			g_stPhase_table_Item.N_diff[i] = (g_stPhase_table_Item.A1A2_change_flag[i]==1)?(g_stPhase_table_Item.N_diff[i-1]+1):g_stPhase_table_Item.N_diff[i-1];
			g_stPhase_table_Item.N_diff[i] = g_stPhase_table_Item.N_diff[i]-n_table_start_diff;
		}
		else
		{
			g_stPhase_table_Item.N_diff[i] = (g_stPhase_table_Item.A1A2_change_flag[i]==1)?(g_stPhase_table_Item.N_diff[i-1]+1):g_stPhase_table_Item.N_diff[i-1];
		}
		//printf("phase_n_diff = %4d\n",g_stPhase_table_Item.N_diff[i]);
	}

	//C_diff (current)
	//j = 0;
	//k = 0;
	//m = 0;
	for(i=0; i<g_stPhase_table_Item.table_cnt; i++)
	{
		if(g_stPhase_table_Item.table_start[i]==1)
		{
			pre_idx = (i<1)?(g_stPhase_table_Item.table_cnt-1):(i-1);
			g_stPhase_table_Item.C_diff[i] = (g_stPhase_table_Item.A1A2_change_flag[i]==1)?(g_stPhase_table_Item.N_diff[pre_idx]+1):g_stPhase_table_Item.N_diff[pre_idx];
			//DBGPRINT_PT("pre_idx = %4d: g_stPhase_table_Item.N_diff[pre_idx] = %4d: g_stPhase_table_Item.C_diff[i] = %4d\n", pre_idx,g_stPhase_table_Item.N_diff[pre_idx],g_stPhase_table_Item.C_diff[i]);
		}
	}

	for(i=0; i<g_stPhase_table_Item.table_cnt; i++)
	{
		if(g_stPhase_table_Item.table_start[i]==0)
		{
			pre_idx = (i<1)?(g_stPhase_table_Item.table_cnt-1):(i-1);

			if(g_stPhase_table_Item.film_phase[i]<g_stPhase_table_Item.C_diff[i-1])
			{
				film_phase_idx = g_stPhase_table_Item.cadence_cnt-(g_stPhase_table_Item.C_diff[i-1]-g_stPhase_table_Item.film_phase[i]);
			}
			else
			{
				film_phase_idx = g_stPhase_table_Item.film_phase[i]-g_stPhase_table_Item.C_diff[i-1];
			}

			if(g_stPhase_table_Item.film_phase[i]==g_stPhase_table_Item.film_phase[pre_idx])
			{
				AB_change_flag_temp[i] = 0;
			}
			else
			{
				AB_change_flag_temp[i] = g_stCadence_table[g_stPhase_table_Item.film_mode].match_data>>(g_stPhase_table_Item.cadence_cnt-film_phase_idx-1)&0x1;

			}

			g_stPhase_table_Item.C_diff[i] = (AB_change_flag_temp[i]==1)?(g_stPhase_table_Item.C_diff[i-1]+1):g_stPhase_table_Item.C_diff[i-1];
		}
		else
		{
			AB_change_flag_temp[i]=1;
		}
	}

	//reserve for no use last frm
	if(g_stCadence_table[g_stPhase_table_Item.film_mode].no_use_lastfrm==1)
	{
		usefrm = g_stCadence_table[g_stPhase_table_Item.film_mode].min_cadence_num;
		frmcnt = 0;

		for(i=0; i<g_stPhase_table_Item.table_cnt; i++)
		{
			if(AB_change_flag_temp[i]==1)
			{
				frmcnt = 1;
			}
			else if(g_stPhase_table_Item.A1A2_change_flag[i]==1)
			{
				frmcnt++;
			}

			last_frm_mask[i]=(frmcnt>usefrm)?(frmcnt-usefrm):0;
			g_stPhase_table_Item.C_diff[i] = g_stPhase_table_Item.C_diff[i] + last_frm_mask[i];
		}
	}

	//P_diff (previous)
	for(i=0; i<g_stPhase_table_Item.table_cnt; i++)
	{
		pre_idx = (i-1 < 0)?(i-1+g_stPhase_table_Item.table_cnt):(i-1);
		if(g_stPhase_table_Item.table_start[i]==1)
		{
			g_stPhase_table_Item.P_diff[i] = g_stPhase_table_Item.C_diff[pre_idx] + ((g_stPhase_table_Item.A1A2_change_flag[i]==1)?1:0);
		}
		else
		{
			g_stPhase_table_Item.P_diff[i] = g_stPhase_table_Item.P_diff[i-1] + ((g_stPhase_table_Item.A1A2_change_flag[i]==1)?1:0);
		}
	}

	//logo p/c diff
	if(g_stPhase_table_Item.film_mode == EN_VIDEO)
	{
		base_logo_p_diff = 3;
		base_logo_c_diff = 2;
	}
	else
	{
		base_logo_p_diff = 3;
		base_logo_c_diff = 2;
	}

	for(i=0; i<g_stPhase_table_Item.table_cnt; i++)
	{
		nex_idx = (i+1 > g_stPhase_table_Item.table_cnt-1)?0:(i+1);
		pre_idx = (i-1 < 0)?(i-1+g_stPhase_table_Item.table_cnt):(i-1);
		if(g_stPhase_table_Item.film_phase[i]==g_stPhase_table_Item.film_phase[0])
		{
			g_stPhase_table_Item.logo_p_diff[i] = base_logo_p_diff;
			g_stPhase_table_Item.logo_c_diff[i] = base_logo_c_diff;
		}
		else
		{
			g_stPhase_table_Item.logo_p_diff[i] = g_stPhase_table_Item.logo_p_diff[pre_idx] + g_stPhase_table_Item.AB_change_flag[i] - g_stPhase_table_Item.table_start[i];
			g_stPhase_table_Item.logo_c_diff[i] = g_stPhase_table_Item.logo_c_diff[pre_idx] + g_stPhase_table_Item.AB_change_flag[i] - g_stPhase_table_Item.table_start[i];
		}
	}
}

void set_frame_delay(int delay_mode, int* p_diff_out, int* c_diff_out, int* n_diff_out)
{
	u8 i, j;
	u8 A1A2_change[65];
	u8 A1A2_change_temp;

	for(i=0; i< g_stPhase_table_Item.table_cnt;i++)
	{
		*(p_diff_out+i) = g_stPhase_table_Item.P_diff[i];
		*(c_diff_out+i) = g_stPhase_table_Item.C_diff[i];
		*(n_diff_out+i) = g_stPhase_table_Item.N_diff[i];
	}

	for(i=0; i<g_stPhase_table_Item.table_cnt; i++)
	{
		A1A2_change[i] = g_stPhase_table_Item.A1A2_change_flag[i];
	}

	if(delay_mode > EN_DELAY_NORMAL)
	{
		for(i=0; i<(delay_mode-EN_DELAY_NORMAL); i++)
		{
			A1A2_change_temp = A1A2_change[0];
			for(j=0;j<(g_stPhase_table_Item.table_cnt-1);j++)
			{
				A1A2_change[j] = A1A2_change[j+1];
			}
			A1A2_change[g_stPhase_table_Item.table_cnt-1] = A1A2_change_temp;

			for(j=0; j<g_stPhase_table_Item.table_cnt;j++)
			{
				if(A1A2_change[j]==1)
				{
					*(p_diff_out + j)= *(p_diff_out+j)+1;
					*(c_diff_out + j)= *(c_diff_out+j)+1;
					*(n_diff_out + j)= *(n_diff_out+j)+1;
				}
				else
				{
					*(p_diff_out + j)= *(p_diff_out+j);
					*(c_diff_out + j)= *(c_diff_out+j);
					*(n_diff_out + j)= *(n_diff_out+j);

				}
			}
		}

	}
	else if(delay_mode < EN_DELAY_NORMAL)
	{
		int shift_less = EN_DELAY_NORMAL - delay_mode;
		for(i=0;i<(g_stPhase_table_Item.table_cnt-1);i++)
		{
			A1A2_change[i] = A1A2_change[i+1];
		}
		A1A2_change[g_stPhase_table_Item.table_cnt-1] = g_stPhase_table_Item.A1A2_change_flag[0];

		for(i=0; i< shift_less; i++)
		{
			A1A2_change_temp = A1A2_change[g_stPhase_table_Item.table_cnt-1];

			for(j=1;j<(g_stPhase_table_Item.table_cnt-1);j++)
			{
				A1A2_change[j] = A1A2_change[j-1];
			}
			A1A2_change[0] = A1A2_change_temp;

			for(j=0; j<g_stPhase_table_Item.table_cnt;j++)
			{
				if(A1A2_change[j]==1)
				{
					*(p_diff_out + j)= (*(p_diff_out+j) > 0)?(*(p_diff_out+j)-1):0;
					*(c_diff_out + j)= (*(c_diff_out+j) > 0)?(*(c_diff_out+j)-1):0;
					*(n_diff_out + j)= (*(n_diff_out+j) > 0)?(*(n_diff_out+j)-1):0;
				}
				else
				{
					*(p_diff_out + j)= *(p_diff_out+j);
					*(c_diff_out + j)= *(c_diff_out+j);
					*(n_diff_out + j)= *(n_diff_out+j);

				}
			}
		}
	}
}

int p_diff_incr[65], c_diff_incr[65], n_diff_incr[65];
void set_increase(void)
{
	int i;
	int Incr_indx=0,Incr_flag=0,Incr_num=0,idx=0,Pre_idx=0, Incr_effect_cnt=0;
	int Time_out_cnt=0, phase_diff=0, phase_diff1=0;
	int MC_DELAY_EN,j,k, Be_overphase=0, over_phase=0, phase_cnt=0, diff_cnt=0, T1,T2;
	int Incr_Total=0, Index=0, Max_V_flip_Diff;
	int Incr_P_flag[65];
	int Incr_C_flag[65];
	int Incr_N_flag[65];
	int Incr_cur_flag, Incr_next_flag, Incr_next2_flag;


	int MatchData = g_stCadence_table[g_stPhase_table_Item.film_mode].match_data;
	int cadence_cnt = g_stPhase_table_Item.cadence_cnt;
	int i_next, i_next2,cur_phase, next_phase, next2_phase;
	int cycle, T3;
	//consider V flip case
	int V_flip =0;

	for(i=0; i<cadence_cnt; i++)
	{
		Incr_phase[i]=0;
	}

	//xil_printf("pcn_dif1:\n\r");
	for(i=0; i<g_stPhase_table_Item.table_cnt;i++)
	{
		Incr_P_flag[i]= 0;
		Incr_C_flag[i]= 0;
		Incr_N_flag[i]= 0;
		p_diff_incr[i]=g_stPhase_table_Item.P_diff[i];
		c_diff_incr[i]=g_stPhase_table_Item.C_diff[i];
		n_diff_incr[i]=g_stPhase_table_Item.N_diff[i];

		//xil_printf("%d%d%d\n\r",p_diff_incr[i],c_diff_incr[i],n_diff_incr[i]);
	}

	if(g_stPhase_table_Item.input_n*2%g_stPhase_table_Item.output_m!=0)
	{
		Max_V_flip_Diff = 2*g_stPhase_table_Item.input_n/g_stPhase_table_Item.output_m+1;
	}
	else
	{
		Max_V_flip_Diff = 2*g_stPhase_table_Item.input_n/g_stPhase_table_Item.output_m;
	}


	MC_DELAY_EN = 0;	//for meander

	while(1)
	{
		if(Time_out_cnt>=1000)
		{
			break;
		}

		for(i=0; i<g_stPhase_table_Item.table_cnt; i++)
		{
			Incr_flag = 0;
			Incr_num = 0;
			phase_cnt = 0;
			Incr_cur_flag = 0;
			Incr_next_flag = 0;
			Incr_next2_flag = 0;

            i_next = (i+1>=g_stPhase_table_Item.table_cnt)?(i+1-g_stPhase_table_Item.table_cnt):(i+1);
			i_next2 = (i+2>=g_stPhase_table_Item.table_cnt)?(i+2-g_stPhase_table_Item.table_cnt):(i+2);

			cur_phase = g_stPhase_table_Item.film_phase[i];
			next_phase = (cur_phase+1>=g_stPhase_table_Item.cadence_cnt)?(cur_phase+1-g_stPhase_table_Item.cadence_cnt):(cur_phase+1);
			next2_phase = (cur_phase+2>=g_stPhase_table_Item.cadence_cnt)?(cur_phase+2-g_stPhase_table_Item.cadence_cnt):(cur_phase+2);

			if(MC_DELAY_EN==1)
			{
				if(V_flip==1)
				{
					if(Max_V_flip_Diff==1)
					{
						if((((p_diff_incr[i] + Incr_phase[next_phase])%g_stPhase_table_Item.buf_num)==(g_stPhase_table_Item.buf_num-Max_V_flip_Diff))
							&&(g_stPhase_table_Item.film_phase[i]!=g_stPhase_table_Item.film_phase[i_next2]))
						{
							Incr_next2_flag = 1;
							phase_diff = p_diff_incr[i];
							over_phase = next_phase;
							phase_cnt = 1;
						}

						if((((c_diff_incr[i] + Incr_phase[next_phase])%g_stPhase_table_Item.buf_num)==(g_stPhase_table_Item.buf_num-Max_V_flip_Diff))
							&&(g_stPhase_table_Item.film_phase[i]!=g_stPhase_table_Item.film_phase[i_next2]))
						{
							Incr_next2_flag = 1;
							phase_diff = c_diff_incr[i];
							over_phase = next_phase;
							phase_cnt = 1;
						}

						if((((n_diff_incr[i] + Incr_phase[next_phase])%g_stPhase_table_Item.buf_num)==(g_stPhase_table_Item.buf_num-Max_V_flip_Diff))
							&&(g_stPhase_table_Item.film_phase[i]!=g_stPhase_table_Item.film_phase[i_next2]))
						{
							Incr_next2_flag = 1;
							phase_diff = n_diff_incr[i];
							over_phase = next_phase;
							phase_cnt = 1;
						}
					}
					else
					{
						if(((p_diff_incr[i] + Incr_phase[next_phase] + Incr_phase[next2_phase])%g_stPhase_table_Item.buf_num)==(g_stPhase_table_Item.buf_num-Max_V_flip_Diff))
						{
							Incr_next2_flag = 1;
							phase_diff = p_diff_incr[i];
							over_phase = next2_phase;
							phase_cnt = 2;
						}


						if(((c_diff_incr[i] + Incr_phase[next_phase] + Incr_phase[next2_phase])%g_stPhase_table_Item.buf_num)==(g_stPhase_table_Item.buf_num-Max_V_flip_Diff))
						{
							Incr_next2_flag = 1;
							phase_diff = c_diff_incr[i];
							over_phase = next2_phase;
							phase_cnt = 2;
						}

						if(((n_diff_incr[i] + Incr_phase[next_phase] + Incr_phase[next2_phase])%g_stPhase_table_Item.buf_num)==(g_stPhase_table_Item.buf_num-Max_V_flip_Diff))
						{
							Incr_next2_flag = 1;
							phase_diff = n_diff_incr[i];
							over_phase = next2_phase;
							phase_cnt = 2;
						}
					}

				}	//end of V flip
				if((((p_diff_incr[i] + Incr_phase[next_phase])%g_stPhase_table_Item.buf_num)==(g_stPhase_table_Item.buf_num-1))
					&&(g_stPhase_table_Item.film_phase[i]!=g_stPhase_table_Item.film_phase[i_next]))
				{
					Incr_next_flag = 1;
					phase_diff = p_diff_incr[i];
					over_phase = next_phase;
					phase_cnt = 1;
				}

				if((((c_diff_incr[i] + Incr_phase[next_phase])%g_stPhase_table_Item.buf_num)==(g_stPhase_table_Item.buf_num-1))
					&&(g_stPhase_table_Item.film_phase[i]!=g_stPhase_table_Item.film_phase[i_next]))
				{
					Incr_next_flag = 1;
					phase_diff = c_diff_incr[i];
					over_phase = next_phase;
					phase_cnt = 1;
				}

				if((((n_diff_incr[i] + Incr_phase[next_phase])%g_stPhase_table_Item.buf_num)==(g_stPhase_table_Item.buf_num-1))
					&&(g_stPhase_table_Item.film_phase[i]!=g_stPhase_table_Item.film_phase[i_next]))
				{
					Incr_next_flag = 1;
					phase_diff = n_diff_incr[i];
					over_phase = next_phase;
					phase_cnt = 1;
				}

				if((p_diff_incr[i]%g_stPhase_table_Item.buf_num==0)&&(p_diff_incr[i]!=0))
				{
					Incr_cur_flag = 1;
					phase_diff = p_diff_incr[i];
					over_phase = cur_phase;
					phase_cnt = 0;
				}

				if((c_diff_incr[i]%g_stPhase_table_Item.buf_num==0)&&(c_diff_incr[i]!=0))
				{
					Incr_cur_flag = 1;
					phase_diff = c_diff_incr[i];
					over_phase = cur_phase;
					phase_cnt = 0;
				}

				if((n_diff_incr[i]%g_stPhase_table_Item.buf_num==0)&&(n_diff_incr[i]!=0))
				{
					Incr_cur_flag = 1;
					phase_diff = n_diff_incr[i];
					over_phase = cur_phase;
					phase_cnt = 0;
				}
			}
			else
			{
				if(V_flip==1)
				{
					if((((p_diff_incr[i] + Incr_phase[next_phase])%g_stPhase_table_Item.buf_num)==(g_stPhase_table_Item.buf_num-1))
						&&(g_stPhase_table_Item.film_phase[i]!=g_stPhase_table_Item.film_phase[i_next]))
					{
						Incr_next_flag = 1;
						phase_diff = p_diff_incr[i];
						over_phase = next_phase;
						phase_cnt = 1;
					}

					if((((c_diff_incr[i] + Incr_phase[next_phase])%g_stPhase_table_Item.buf_num)==(g_stPhase_table_Item.buf_num-1))
						&&(g_stPhase_table_Item.film_phase[i]!=g_stPhase_table_Item.film_phase[i_next]))
					{
						Incr_next_flag = 1;
						phase_diff = c_diff_incr[i];
						over_phase = next_phase;
						phase_cnt = 1;
					}

					if((((n_diff_incr[i] + Incr_phase[next_phase])%g_stPhase_table_Item.buf_num)==(g_stPhase_table_Item.buf_num-1))
						&&(g_stPhase_table_Item.film_phase[i]!=g_stPhase_table_Item.film_phase[i_next]))
					{
						Incr_next_flag = 1;
						phase_diff = n_diff_incr[i];
						over_phase = next_phase;
						phase_cnt = 1;
					}

				}
				if((p_diff_incr[i]%g_stPhase_table_Item.buf_num==0)&&(p_diff_incr[i]!=0))
				{
					Incr_cur_flag = 1;
					phase_diff = p_diff_incr[i];
					over_phase = cur_phase;
					phase_cnt = 0;
				}

				if((c_diff_incr[i]%g_stPhase_table_Item.buf_num==0)&&(c_diff_incr[i]!=0))
				{
					Incr_cur_flag = 1;
					phase_diff = c_diff_incr[i];
					over_phase = cur_phase;
					phase_cnt = 0;
				}

				if((n_diff_incr[i]%g_stPhase_table_Item.buf_num==0)&&(n_diff_incr[i]!=0))
				{
					Incr_cur_flag = 1;
					phase_diff = n_diff_incr[i];
					over_phase = cur_phase;
					phase_cnt = 0;
				}
			}
			for(T2=0; T2<3;T2++)
			{
				if(T2==0)
				{
					phase_diff1 = p_diff_incr[i];
				}
				else if(T2==1)
				{
					phase_diff1 = c_diff_incr[i];
				}
				else if(T2==2)
				{
					phase_diff1 = n_diff_incr[i];
				}

				for(j=0; j<cadence_cnt*5;j++)
				{
					Incr_Total = 0;
					for(k=0; k<j;k++)
					{
						Index =((cur_phase-k)<0)?(cur_phase+cadence_cnt*5-k):(cur_phase-k);
						Index = Index%cadence_cnt;
						Incr_Total = Incr_phase[Index]+1+Incr_Total;
					}

					if(phase_diff1==Incr_Total)
					{
						Be_overphase = ((cur_phase-j)<0)?(cur_phase+cadence_cnt*5-j):(cur_phase-j);
						Be_overphase = Be_overphase%cadence_cnt;
						break;
					}
				}

				cycle = phase_diff1/g_stPhase_table_Item.buf_num;

				for(T3=0; T3<cycle; T3++)
				{
					for(T1=0; T1<j;T1++)
					{
						Incr_Total = 0;
						for(k=0; k<T1; k++)
						{
							Index =((cur_phase-k)<0)?(cur_phase+cadence_cnt*5-k):(cur_phase-k);
							Index = Index%cadence_cnt;
							Incr_Total = Incr_phase[Index]+1+Incr_Total;
						}

						if(((phase_diff1%g_stPhase_table_Item.buf_num)+T3*g_stPhase_table_Item.buf_num)==Incr_Total)
						{
							for(k=0;k<j-T1;k++)
							{
								Index = (Be_overphase+k+1)%cadence_cnt;
								if(MatchData>>(cadence_cnt-Index-1)&0x1)
								{
									Incr_flag=1;
									over_phase=((cur_phase-T1)<0)?(cur_phase+cadence_cnt*5-T1):(cur_phase-T1);
									over_phase=over_phase%cadence_cnt;
								}
							}
							break;
						}
					}
				}
				if(Incr_flag==1)
				{
					break;
				}
			}
			if(Incr_flag==0)
			{
				if((Incr_cur_flag==1)||(Incr_next_flag==1)||(Incr_next2_flag==1))
				{
					for(j=0; j<cadence_cnt*5;j++)
					{
						Incr_Total=0;
						for(k=0; k<j; k++)
						{
							Index =((cur_phase-k)<0)?(cur_phase+cadence_cnt*5-k):(cur_phase-k);
							Index = Index%cadence_cnt;
							Incr_Total = Incr_phase[Index]+1+Incr_Total;
						}
						if(phase_diff==Incr_Total)
						{
							Be_overphase = ((cur_phase-j)<0)?(cur_phase+cadence_cnt*5-j):(cur_phase-j);
							Be_overphase = Be_overphase%cadence_cnt;
							break;
						}

					}
					if((V_flip==1)&&((phase_cnt!=0)||((phase_cnt==0)&&(g_stPhase_table_Item.film_phase[i]==g_stPhase_table_Item.film_phase[i_next]))))
					{
						Incr_flag = 1;
					}
					else
					{
						j = j + phase_cnt;
						for(k=0; k<j;k++)
						{
							Index = (Be_overphase+k+1)%cadence_cnt;
							if(MatchData>>(cadence_cnt-Index-1)&0x1)
							{
								Incr_flag=1;
							}
						}

						if((V_flip==1)&&(Incr_flag==0))
						{
							if(Incr_next_flag==1)
							{
								over_phase = next_phase;
								Incr_flag = 1;
							}
							else if(Incr_next2_flag==1)
							{
								over_phase = next2_phase;
								Incr_flag = 1;
							}
						}
					}
				}

			}

			if(Incr_flag==1)
			{
				for(i=0; i<g_stPhase_table_Item.table_cnt;i++)
				{
					cur_phase = g_stPhase_table_Item.film_phase[i];
					//check p diff
					for(j=0; j<cadence_cnt*5;j++)
					{
						Incr_Total=0;
						for(k=0;k<j;k++)
						{
							Index =((cur_phase-k)<0)?(cur_phase+cadence_cnt*5-k):(cur_phase-k);
							Index = Index%cadence_cnt;
							Incr_Total = Incr_phase[Index]+1+Incr_Total;
						}

						if(p_diff_incr[i]==Incr_Total)
						{
							break;
						}
					}

					diff_cnt=0;

					for(k=0;k<j;k++)
					{
						Index =((cur_phase-k)<0)?(cur_phase+cadence_cnt*5-k):(cur_phase-k);
						Index = Index%cadence_cnt;
						if(Index==over_phase)
						{
							diff_cnt++;
						}
					}
					Incr_P_flag[i] = ((diff_cnt==0)?0:(diff_cnt-1));

					//check C diff
					for(j=0; j<cadence_cnt*5;j++)
					{
						Incr_Total=0;
						for(k=0;k<j;k++)
						{
							Index =((cur_phase-k)<0)?(cur_phase+cadence_cnt*5-k):(cur_phase-k);
							Index = Index%cadence_cnt;
							Incr_Total = Incr_phase[Index]+1+Incr_Total;
						}

						if(c_diff_incr[i]==Incr_Total)
						{
							break;
						}
					}

					diff_cnt=0;

					for(k=0;k<j;k++)
					{
						Index =((cur_phase-k)<0)?(cur_phase+cadence_cnt*5-k):(cur_phase-k);
						Index = Index%cadence_cnt;
						if(Index==over_phase)
						{
							diff_cnt++;
						}
					}
					Incr_C_flag[i] = ((diff_cnt==0)?0:(diff_cnt-1));

					//check N diff
					for(j=0; j<cadence_cnt*5;j++)
					{
						Incr_Total=0;
						for(k=0;k<j;k++)
						{
							Index =((cur_phase-k)<0)?(cur_phase+cadence_cnt*5-k):(cur_phase-k);
							Index = Index%cadence_cnt;
							Incr_Total = Incr_phase[Index]+1+Incr_Total;
						}

						if(n_diff_incr[i]==Incr_Total)
						{
							break;
						}
					}

					diff_cnt=0;

					for(k=0;k<j;k++)
					{
						Index =((cur_phase-k)<0)?(cur_phase+cadence_cnt*5-k):(cur_phase-k);
						Index = Index%cadence_cnt;
						if(Index==over_phase)
						{
							diff_cnt++;
						}
					}
					Incr_N_flag[i] = ((diff_cnt==0)?0:(diff_cnt-1));
				}
				Incr_phase[over_phase]++;
				Incr_num++;
				Time_out_cnt++;
				if((Incr_phase[over_phase]+1)%g_stPhase_table_Item.buf_num==0)
				{
					if(MatchData>>(cadence_cnt-over_phase-1)&0x1)
					{
						Incr_phase[over_phase]++;
						Incr_num++;
					}
				}
				break;
			}
		}

		if(Incr_flag==1)
		{
			Incr_effect_cnt = 0;
			//find first increase table from current table ,it is very important!!!
			//i is from above forloop , means increase start point
			for(j=0; j<g_stPhase_table_Item.table_cnt;j++)
			{
				idx = ((j+i)>=g_stPhase_table_Item.table_cnt)?(j+i-g_stPhase_table_Item.table_cnt):(j+i);
				if(g_stPhase_table_Item.film_phase[idx]==over_phase)
				{
					Incr_indx=idx;
					break;
				}
			}

			//record Incr indx
			Pre_idx = Incr_indx;

			for(i=0;i<g_stPhase_table_Item.table_cnt;i++)
			{
				idx = ((i+Incr_indx)>=g_stPhase_table_Item.table_cnt)?(i+Incr_indx-g_stPhase_table_Item.table_cnt):(i+Incr_indx);

				if(g_stPhase_table_Item.film_phase[Pre_idx]!=g_stPhase_table_Item.film_phase[idx])
				{
					Incr_effect_cnt++;
					Incr_effect_cnt+=Incr_phase[g_stPhase_table_Item.film_phase[idx]];
					if(g_stPhase_table_Item.film_phase[idx]==over_phase)
					{
						Incr_effect_cnt = 0;
					}
				}

				p_diff_incr[idx] = (p_diff_incr[idx]<=Incr_effect_cnt)?p_diff_incr[idx]:(p_diff_incr[idx]+Incr_num*(Incr_P_flag[idx]+1));
				c_diff_incr[idx] = (c_diff_incr[idx]<=Incr_effect_cnt)?c_diff_incr[idx]:(c_diff_incr[idx]+Incr_num*(Incr_C_flag[idx]+1));
				n_diff_incr[idx] = (n_diff_incr[idx]<=Incr_effect_cnt)?n_diff_incr[idx]:(n_diff_incr[idx]+Incr_num*(Incr_N_flag[idx]+1));
				Pre_idx = idx;
			}
		}
		else
		{
			break;
		}
	}

	for(i=0; i<cadence_cnt;i++)
	{
		Incr_phase[i] = Incr_phase[i]%g_stPhase_table_Item.buf_num;
	}

	//xil_printf("pcn_dif2:\n\r");
	for(i=0; i<g_stPhase_table_Item.table_cnt;i++)
	{
		g_stPhase_table_Item.P_diff[i] = p_diff_incr[i]%g_stPhase_table_Item.buf_num;
		g_stPhase_table_Item.C_diff[i] = c_diff_incr[i]%g_stPhase_table_Item.buf_num;
		g_stPhase_table_Item.N_diff[i] = n_diff_incr[i]%g_stPhase_table_Item.buf_num;
		//xil_printf("%d%d%d\n\r",g_stPhase_table_Item.P_diff[i],g_stPhase_table_Item.C_diff[i],g_stPhase_table_Item.N_diff[i]);
	}
}

int p_diff_shift[2][65], c_diff_shift[2][65], n_diff_shift[2][65];
void set_use_case(void)
{
	int i;

	//frame delay mode just need consider A1A2 change.
	if(g_stPhase_table_Item.phase_out_div2==0)
	{
		int p_diff_shift[65], c_diff_shift[65], n_diff_shift[65];

		set_frame_delay(g_stPhase_table_Item.Frm_delay_mode, &(p_diff_shift[0]), &(c_diff_shift[0]), &(n_diff_shift[0]));
		for(i=0; i<g_stPhase_table_Item.table_cnt; i++)
		{
			g_stPhase_table_Item.P_diff[i] = p_diff_shift[i];
			g_stPhase_table_Item.C_diff[i] = c_diff_shift[i];
			g_stPhase_table_Item.N_diff[i] = n_diff_shift[i];
		}

	}
	else
	{
		for(i=0; i<2; i++)
		{
			set_frame_delay(g_stPhase_table_Item.Frm_delay_mode+i, &(p_diff_shift[i][0]), &(c_diff_shift[i][0]), &(n_diff_shift[i][0]));
		}

		for(i=0; i<g_stPhase_table_Item.table_cnt; i++)
		{
			g_stPhase_table_Item.P_diff[i] = p_diff_shift[i%2][(i/2)*2];
			g_stPhase_table_Item.C_diff[i] = c_diff_shift[i%2][(i/2)*2];
			g_stPhase_table_Item.N_diff[i] = n_diff_shift[i%2][(i/2)*2];
		}

	}

	//set_increase();
}

void gen_table(void)
{
	int Mode_cnt_use;

	//multi mode table, gen one table
	if(g_stPhase_table_Item.mode_cnt > 1 && g_stPhase_table_Item.gen_one_mode == 1)
	{
		//
	}
	//multi mode table, gen multi table
	else if(g_stPhase_table_Item.mode_cnt > 1 && g_stPhase_table_Item.gen_one_mode == 0)
	{

	}
	//one mode table
	else
	{
		Mode_cnt_use = 1;
		g_stPhase_table_Item.mode_shift = 0;
		g_stPhase_table_Item.frc_phase_shift = 0;
		set_addr_lut();
		set_data_lut();
		set_use_case();
		//xil_printf("tbl=%d\n\r",g_stPhase_table_Item.table_start[8]);
	}
	//xil_printf("tbl=%d\n\r",g_stPhase_table_Item.table_start[8]);
}

void phase_table_core(u8 input_n, u8 output_m,u8 frame_buffer_num, u8 mode)
{
	u8 i,j,k;
	u32 mem_l = 0;
	u32 mem_h = 0;
	//u64 mem = 0;

	//	init parameters
	g_stPhase_table_Item.input_n 	= input_n;
	g_stPhase_table_Item.output_m 	= output_m;
	g_stPhase_table_Item.film_mode 	= mode;
	g_stPhase_table_Item.buf_num 	= frame_buffer_num;

	g_stPhase_table_Item.cadence_cnt = g_stCadence_table[g_stPhase_table_Item.film_mode].cadence_cnt;
	g_stPhase_table_Item.cadence_length = g_stCadence_table[g_stPhase_table_Item.film_mode].cadence_diff_cnt;
	//-----------------
	sel_use_case();
	get_table_count();
	//xil_printf("tbl cnt=%d\n\r",g_stPhase_table_Item.table_cnt);
	set_frc_step();
	gen_table();
	//xil_printf("tbl=%d\n\r",g_stPhase_table_Item.table_start[8]);
	UPDATE_FRC_REG_BITS(FRC_REG_PD_PAT_NUM, g_stPhase_table_Item.cadence_cnt, 0xFF);
	UPDATE_FRC_REG_BITS(FRC_REG_TOP_CTRL17, 1<<3, 0x8);    //  write flag
	WRITE_FRC_REG(FRC_TOP_LUT_ADDR,0);          //DDR address

	for(i=0;i<g_stPhase_table_Item.table_cnt;i++)
	{
		//xil_printf("tbl=%d\n\r",g_stPhase_table_Item.table_start[8]);
		mem_l = (g_stPhase_table_Item.mc_phase_step[i]<<24)|(g_stPhase_table_Item.me_phase_step[i]<<16)|(g_stPhase_table_Item.film_phase[i]<<8)|(g_stPhase_table_Item.frc_phase[i]);
		mem_h = (g_stPhase_table_Item.table_start[i]<<20)|(g_stPhase_table_Item.logo_p_diff[i]<<16)|(g_stPhase_table_Item.logo_c_diff[i]<<12)|(g_stPhase_table_Item.P_diff[i]<<8)|(g_stPhase_table_Item.C_diff[i]<<4)|(g_stPhase_table_Item.N_diff[i]);
		WRITE_FRC_REG(FRC_TOP_LUT_DATA,mem_l);        //mem[0:31]
		WRITE_FRC_REG(FRC_TOP_LUT_DATA,mem_h&0x1FFFFF);    //mem[32:63)
		//xil_printf("mc=%d,me=%d,fil=%d,frc=%d\n\r", g_stPhase_table_Item.mc_phase_step[i], g_stPhase_table_Item.me_phase_step[i], g_stPhase_table_Item.film_phase[i], g_stPhase_table_Item.frc_phase[i] );
		// xil_printf("tbl=%d,P=%d, C=%d, N=%d \n\r",g_stPhase_table_Item.table_start[i],g_stPhase_table_Item.P_diff[i], g_stPhase_table_Item.C_diff[i], g_stPhase_table_Item.N_diff[i]);
		//xil_printf("tbl=%d\n\r",g_stPhase_table_Item.table_start[i]);
	}

	UPDATE_FRC_REG_BITS(FRC_REG_TOP_CTRL17, 1<<4, 0x10);   //write end flag

	for(i=0; i<g_stPhase_table_Item.cadence_cnt;i++)
	{
		j = i/4;
		k = i%4 == 0 ? 28 : i%4 == 1 ? 20 : i%4 == 2 ? 12 : 4;
		UPDATE_FRC_REG_BITS(FRC_REG_LOAD_ORG_FRAME_0+(FRC_REG_LOAD_ORG_FRAME_1-FRC_REG_LOAD_ORG_FRAME_0)*j,Incr_phase[i]<<k,(1<<(k+4))-(1<<k));
		UPDATE_FRC_REG_BITS(FRC_REG_LOAD_ORG_FRAME_0+(FRC_REG_LOAD_ORG_FRAME_1-FRC_REG_LOAD_ORG_FRAME_0)*j,ip_film_end[i]<<(k-1),1<<(k-1));
	}

	g_stPhase_table_Item.table_cnt = g_stPhase_table_Item.table_cnt;
	UPDATE_FRC_REG_BITS(FRC_REG_OUT_FID, g_stPhase_table_Item.table_cnt<<24, 0xFF000000);

	if(g_stPhase_table_Item.film_mode==EN_VIDEO)
		UPDATE_FRC_REG_BITS(FRC_REG_PAT_POINTER, 3<<24, 0xFF000000);
	else if(g_stPhase_table_Item.film_mode==EN_FILM22)
		UPDATE_FRC_REG_BITS(FRC_REG_PAT_POINTER, 5<<24, 0xFF000000);
	else if(g_stPhase_table_Item.film_mode==EN_FILM44)
		UPDATE_FRC_REG_BITS(FRC_REG_PAT_POINTER, 9<<24, 0xFF000000);
	else
		UPDATE_FRC_REG_BITS(FRC_REG_PAT_POINTER, (g_stPhase_table_Item.cadence_cnt+1)<<24, 0xFF000000);
}

