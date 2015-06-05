/*
 * SCPI Message Protocol driver header
 *
 * Copyright (C) 2014 ARM Ltd.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include <linux/types.h>

struct scpi_opp_entry {
	u32 freq_hz;
	u32 volt_mv;
} __packed;

struct scpi_opp {
	struct scpi_opp_entry *opp;
	u32 latency; /* in usecs */
	int count;
} __packed;


unsigned long scpi_clk_get_val(u16 clk_id);
int scpi_clk_set_val(u16 clk_id, unsigned long rate);
int scpi_dvfs_get_idx(u8 domain);
int scpi_dvfs_set_idx(u8 domain, u8 idx);
struct scpi_opp *scpi_dvfs_get_opps(u8 domain);
int scpi_get_sensor(char *name);
int scpi_get_sensor_value(u16 sensor, u32 *val);
