/*
 * drivers/amlogic/cpufreq/cpufreq_table.h
 *
 * Copyright (C) 2015 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
*/

#include <linux/cpufreq.h>

static struct cpufreq_frequency_table meson_freq_table[] = {
	{0, 96000    },
	{1, 192000   },
	{2, 312000   },
	{3, 408000   },
	{4, 504000   },
	{5, 600000   },
	{6, 696000   },
	{7, 816000   },
	{8, 912000   },
	{9, 1008000  },
	{10, 1104000  },
	{11, 1200000  },
	{12, 1296000  },
	{13, 1416000  },
	{14, 1512000  },
	{15, 1608000  },
	{16, 1800000  },
	{17, 1992000  },
	{18, CPUFREQ_TABLE_END},
};

