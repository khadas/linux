// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/version.h>
#include <linux/string.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/vmalloc.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/amlogic/media/vout/lcd/aml_ldim.h>
#include <linux/amlogic/media/vout/lcd/aml_bl.h>
#include <linux/amlogic/media/vout/lcd/ldim_fw.h>
#include "../../lcd_common.h"
#include "ldim_drv.h"
#include "ldim_reg.h"

unsigned int ldc_gain_lut_array[16][64] = {
	{
		3216, 3152, 3088, 3024, 2960, 2896, 2832, 2668,
		2525, 2404, 2299, 2208, 2128, 2056, 1991, 1933,
		1879, 1831, 1785, 1744, 1705, 1669, 1635, 1604,
		1574, 1546, 1519, 1494, 1470, 1447, 1426, 1405,
		1386, 1367, 1349, 1331, 1315, 1299, 1284, 1269,
		1254, 1241, 1227, 1215, 1202, 1190, 1179, 1167,
		1157, 1146, 1135, 1125, 1116, 1106, 1097, 1088,
		1079, 1071, 1062, 1054, 1046, 1038, 1031, 1024
	},
	{
		3216, 3152, 3088, 3024, 2960, 2896, 2832, 2668,
		2525, 2404, 2299, 2208, 2128, 2056, 1991, 1933,
		1879, 1831, 1785, 1744, 1705, 1669, 1635, 1604,
		1574, 1546, 1519, 1494, 1470, 1447, 1426, 1405,
		1386, 1367, 1349, 1331, 1315, 1299, 1284, 1269,
		1254, 1241, 1227, 1215, 1202, 1190, 1179, 1167,
		1157, 1146, 1135, 1125, 1116, 1106, 1097, 1088,
		1079, 1071, 1062, 1054, 1046, 1038, 1031, 1024
	},
	{
		3216, 3152, 3088, 3024, 2960, 2896, 2832, 2668,
		2525, 2404, 2299, 2208, 2128, 2056, 1991, 1933,
		1879, 1831, 1785, 1744, 1705, 1669, 1635, 1604,
		1574, 1546, 1519, 1494, 1470, 1447, 1426, 1405,
		1386, 1367, 1349, 1331, 1315, 1299, 1284, 1269,
		1254, 1241, 1227, 1215, 1202, 1190, 1179, 1167,
		1157, 1146, 1135, 1125, 1116, 1106, 1097, 1088,
		1079, 1071, 1062, 1054, 1046, 1038, 1031, 1024
	},
	{
		3216, 3152, 3088, 3024, 2960, 2896, 2832, 2668,
		2525, 2404, 2299, 2208, 2128, 2056, 1991, 1933,
		1879, 1831, 1785, 1744, 1705, 1669, 1635, 1604,
		1574, 1546, 1519, 1494, 1470, 1447, 1426, 1405,
		1386, 1367, 1349, 1331, 1315, 1299, 1284, 1269,
		1254, 1241, 1227, 1215, 1202, 1190, 1179, 1167,
		1157, 1146, 1135, 1125, 1116, 1106, 1097, 1088,
		1079, 1071, 1062, 1054, 1046, 1038, 1031, 1024
	},
	{
		3216, 3152, 3088, 3024, 2960, 2896, 2832, 2668,
		2525, 2404, 2299, 2208, 2128, 2056, 1991, 1933,
		1879, 1831, 1785, 1744, 1705, 1669, 1635, 1604,
		1574, 1546, 1519, 1494, 1470, 1447, 1426, 1405,
		1386, 1367, 1349, 1331, 1315, 1299, 1284, 1269,
		1254, 1241, 1227, 1215, 1202, 1190, 1179, 1167,
		1157, 1146, 1135, 1125, 1116, 1106, 1097, 1088,
		1079, 1071, 1062, 1054, 1046, 1038, 1031, 1024
	},
	{
		2720, 2703, 2686, 2669, 2652, 2635, 2617, 2591,
		2525, 2404, 2299, 2208, 2128, 2056, 1991, 1933,
		1879, 1831, 1785, 1744, 1705, 1669, 1635, 1604,
		1574, 1546, 1519, 1494, 1470, 1447, 1426, 1405,
		1386, 1367, 1349, 1331, 1315, 1299, 1284, 1269,
		1254, 1241, 1227, 1215, 1202, 1190, 1179, 1167,
		1157, 1146, 1135, 1125, 1116, 1106, 1097, 1088,
		1079, 1071, 1062, 1054, 1046, 1038, 1031, 1024
	},
	{
		2336, 2323, 2310, 2297, 2284, 2271, 2258, 2245,
		2232, 2219, 2203, 2176, 2128, 2056, 1991, 1933,
		1879, 1831, 1785, 1744, 1705, 1669, 1635, 1604,
		1574, 1546, 1519, 1494, 1470, 1447, 1426, 1405,
		1386, 1367, 1349, 1331, 1315, 1299, 1284, 1269,
		1254, 1241, 1227, 1215, 1202, 1190, 1179, 1167,
		1157, 1146, 1135, 1125, 1116, 1106, 1097, 1088,
		1079, 1071, 1062, 1054, 1046, 1038, 1031, 1024
	},
	{
		2048, 2039, 2031, 2023, 2014, 2004, 1994, 1983,
		1971, 1958, 1944, 1928, 1911, 1893, 1872, 1850,
		1826, 1799, 1770, 1739, 1705, 1669, 1635, 1604,
		1574, 1546, 1519, 1494, 1470, 1447, 1426, 1405,
		1386, 1367, 1349, 1331, 1315, 1299, 1284, 1269,
		1254, 1241, 1227, 1215, 1202, 1190, 1179, 1167,
		1157, 1146, 1135, 1125, 1116, 1106, 1097, 1088,
		1079, 1071, 1062, 1054, 1046, 1038, 1031, 1024
	},
	{
		1808, 1805, 1803, 1801, 1798, 1795, 1791, 1787,
		1783, 1777, 1771, 1764, 1756, 1747, 1737, 1726,
		1713, 1699, 1683, 1667, 1649, 1630, 1610, 1589,
		1566, 1543, 1519, 1494, 1470, 1447, 1426, 1405,
		1386, 1367, 1349, 1331, 1315, 1299, 1284, 1269,
		1254, 1241, 1227, 1215, 1202, 1190, 1179, 1167,
		1157, 1146, 1135, 1125, 1116, 1106, 1097, 1088,
		1079, 1071, 1062, 1054, 1046, 1038, 1031, 1024
	},
	{
		1632, 1630, 1628, 1626, 1624, 1622, 1620, 1617,
		1615, 1612, 1608, 1605, 1601, 1596, 1591, 1586,
		1580, 1574, 1567, 1559, 1551, 1541, 1532, 1521,
		1510, 1498, 1484, 1470, 1455, 1440, 1423, 1405,
		1386, 1367, 1349, 1331, 1315, 1299, 1284, 1269,
		1254, 1241, 1227, 1215, 1202, 1190, 1179, 1167,
		1157, 1146, 1135, 1125, 1116, 1106, 1097, 1088,
		1079, 1071, 1062, 1054, 1046, 1038, 1031, 1024
	},
	{
		1488, 1485, 1482, 1480, 1477, 1475, 1472, 1469,
		1467, 1464, 1461, 1459, 1456, 1453, 1451, 1448,
		1445, 1442, 1439, 1437, 1434, 1431, 1428, 1425,
		1422, 1419, 1416, 1413, 1409, 1405, 1400, 1392,
		1381, 1367, 1349, 1331, 1315, 1299, 1284, 1269,
		1254, 1241, 1227, 1215, 1202, 1190, 1179, 1167,
		1157, 1146, 1135, 1125, 1116, 1106, 1097, 1088,
		1079, 1071, 1062, 1054, 1046, 1038, 1031, 1024
	},
	{
		1360, 1357, 1355, 1353, 1351, 1348, 1346, 1344,
		1342, 1340, 1337, 1335, 1333, 1330, 1328, 1326,
		1323, 1321, 1319, 1316, 1314, 1311, 1309, 1306,
		1304, 1301, 1298, 1296, 1293, 1290, 1287, 1284,
		1281, 1279, 1275, 1272, 1269, 1264, 1259, 1253,
		1246, 1237, 1227, 1215, 1202, 1190, 1179, 1167,
		1157, 1146, 1135, 1125, 1116, 1106, 1097, 1088,
		1079, 1071, 1062, 1054, 1046, 1038, 1031, 1024
	},
	{
		1248, 1246, 1244, 1242, 1241, 1239, 1237, 1236,
		1234, 1232, 1231, 1229, 1227, 1226, 1224, 1222,
		1220, 1218, 1217, 1215, 1213, 1211, 1209, 1208,
		1206, 1204, 1202, 1200, 1198, 1196, 1194, 1192,
		1190, 1188, 1186, 1184, 1181, 1179, 1177, 1175,
		1173, 1170, 1168, 1165, 1162, 1159, 1155, 1150,
		1145, 1139, 1132, 1125, 1116, 1106, 1097, 1088,
		1079, 1071, 1062, 1054, 1046, 1038, 1031, 1024
	},
	{
		1168, 1167, 1167, 1166, 1166, 1166, 1165, 1165,
		1164, 1164, 1163, 1163, 1162, 1162, 1161, 1161,
		1160, 1160, 1159, 1159, 1158, 1157, 1157, 1156,
		1155, 1154, 1153, 1153, 1152, 1151, 1150, 1149,
		1148, 1146, 1145, 1144, 1143, 1142, 1140, 1139,
		1137, 1136, 1134, 1133, 1131, 1129, 1127, 1126,
		1124, 1121, 1118, 1115, 1110, 1104, 1097, 1088,
		1079, 1071, 1062, 1054, 1046, 1038, 1031, 1024
	},
	{
		1088, 1086, 1085, 1084, 1083, 1082, 1081, 1080,
		1079, 1078, 1077, 1076, 1075, 1074, 1073, 1072,
		1071, 1070, 1069, 1068, 1067, 1066, 1066, 1065,
		1064, 1063, 1063, 1062, 1061, 1061, 1060, 1059,
		1059, 1059, 1058, 1058, 1057, 1057, 1057, 1057,
		1056, 1056, 1056, 1056, 1056, 1056, 1057, 1057,
		1057, 1057, 1058, 1058, 1059, 1059, 1060, 1061,
		1061, 1061, 1059, 1054, 1046, 1038, 1031, 1024
	},
	{
		1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024,
		1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024,
		1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024,
		1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024,
		1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024,
		1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024,
		1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024,
		1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024
	}

};

unsigned int ldc_min_gain_lut[64] = {
	64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
	64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
	64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
	64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64
};

unsigned int ldc_dither_lut[32][16] = {
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, //4*4 frame00@00
	{1, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1}, //4*4 frame00@01
	{0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1}, //4*4 frame00@10
	{0, 1, 1, 1, 1, 1, 0, 1, 1, 0, 1, 1, 1, 1, 1, 0}, //4*4 frame00@11

	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, //4*4 frame01@00
	{0, 0, 0, 1, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0}, //4*4 frame01@01
	{1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0}, //4*4 frame01@10
	{1, 1, 1, 0, 1, 0, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1}, //4*4 frame01@11

	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, //4*4 frame02@00
	{0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0}, //4*4 frame02@01
	{0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1}, //4*4 frame02@10
	{1, 1, 0, 1, 0, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 1}, //4*4 frame02@11

	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, //4*4 frame03@00
	{0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 1, 0, 0, 0}, //4*4 frame03@01
	{1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0}, //4*4 frame03@10
	{1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 0, 1, 0, 1, 1, 1}, //4*4 frame03@11

	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, //4*4 frame04@00
	{0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 1, 0, 0, 0}, //4*4 frame04@01
	{1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1}, //4*4 frame04@10
	{1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 0, 1, 0, 1, 1, 1}, //4*4 frame04@11

	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, //4*4 frame05@00
	{1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 1, 0, 0}, //4*4 frame05@01
	{1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1}, //4*4 frame05@10
	{0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0, 1, 0, 1, 1}, //4*4 frame05@11

	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, //4*4 frame06@00
	{0, 0, 0, 1, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0}, //4*4 frame06@01
	{0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0}, //4*4 frame06@10
	{1, 1, 1, 0, 1, 0, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1}, //4*4 frame06@11

	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, //4*4 frame07@00
	{0, 0, 1, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1}, //4*4 frame07@01
	{0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0}, //4*4 frame07@10
	{1, 1, 0, 1, 0, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0}  //4*4 frame07@11
};

void ldc_gain_lut_set_t7(void)
{
	unsigned int *p;
	unsigned int data, ram_base = 0;
	int i, j;

//	LDIMPR("%s\n", __func__);

	for (i = 0; i < 16; i++) {
		p = ldc_gain_lut_array[i];
		for (j = 0; j < 64; j = j + 2) {
			VSYNC_WR_MPEG_REG(LDC_GAIN_LUT_ADDR, ram_base);
			data = ((p[j + 1] << 12) + p[j]);
			VSYNC_WR_MPEG_REG(LDC_GAIN_LUT_DATA, data);
			VSYNC_WR_MPEG_REG(LDC_GAIN_LUT_CTRL0, 0x1);
			VSYNC_WR_MPEG_REG(LDC_GAIN_LUT_CTRL1, 0x0);
			VSYNC_WR_MPEG_REG(LDC_GAIN_LUT_CTRL1, 0x1);

			ram_base = ram_base + 2;
			lcd_vcbus_read(LDC_REG_PANEL_SIZE);
			lcd_vcbus_read(LDC_REG_PANEL_SIZE);
			lcd_vcbus_read(LDC_REG_PANEL_SIZE);
		}
	}
}

void ldc_gain_lut_set_t3(void)
{
	unsigned int *p;
	unsigned int data_wr;
	int i, j;

	//switch to cbus clock for gain lut ram
	VSYNC_WR_MPEG_REG(LDC_GAIN_LUT_CTRL0, 2);
	VSYNC_WR_MPEG_REG(LDC_GAIN_LUT_ADDR, 0);

	for (i = 0; i < 16; i++) {
		p = ldc_gain_lut_array[i];
		for (j = 0; j < 64; j = j + 2) {
			data_wr = ((p[j + 1] << 12) + p[j]);
			VSYNC_WR_MPEG_REG(LDC_GAIN_LUT_DATA, data_wr);
		}
	}
	//switch to ip clock for gain lut ram
	VSYNC_WR_MPEG_REG(LDC_GAIN_LUT_CTRL0, 0);

	lcd_vcbus_read(LDC_REG_PANEL_SIZE);
	lcd_vcbus_read(LDC_REG_PANEL_SIZE);
}

void ldc_gain_lut_get_t3(void)
{
	unsigned int *p;
	unsigned int data_wr, data_rd;
	int i, j;

	LDIMPR("%s\n", __func__);

	//switch to cbus clock for gain lut ram
	lcd_vcbus_write(LDC_GAIN_LUT_CTRL0, 2);
	lcd_vcbus_write(LDC_GAIN_LUT_ADDR, 0);
	lcd_vcbus_read(LDC_RO_GAIN_SMP_DATA);// for toggle read
	for (i = 0; i < 16; i++) {
		p = ldc_gain_lut_array[i];
		for (j = 0; j < 64; j = j + 2) {
			data_wr = ((p[j + 1] << 12) + p[j]);
			data_rd = lcd_vcbus_read(LDC_RO_GAIN_SMP_DATA);
			if (data_wr == data_rd) {
				LDIMPR("%d: data_wr=0x%x, data_rd=0x%x\n",
						i * 64 + j,
						data_wr, data_rd);
			} else {
				LDIMERR("%d: data_wr=0x%x, data_rd=0x%x\n",
						i * 64 + j,
						data_wr, data_rd);
			}
		}
	}
	//switch to ip clock for gain lut ram
	lcd_vcbus_write(LDC_GAIN_LUT_CTRL0, 0);

	lcd_vcbus_read(LDC_REG_PANEL_SIZE);
	lcd_vcbus_read(LDC_REG_PANEL_SIZE);
}

void ldc_min_gain_lut_set(void)
{
	unsigned int *p;
	unsigned int data;
	int i, j;

//	LDIMPR("%s\n", __func__);

	p = ldc_min_gain_lut;
	for (i = 0; i < 16; i++) {
		data = 0;
		for (j = 0; j < 4; j++)
			data |= ((p[i * 4 + j] & 0xff) << (j * 8));
		VSYNC_WR_MPEG_REG(LDC_REG_MIN_GAIN_LUT_0 + i, data);
	}
}

void ldc_dither_lut_set(void)
{
	unsigned int data, temp, offset, index;
	int i, j, k;

//	LDIMPR("%s\n", __func__);

	for (i = 0; i < 4; i++) { /* ls2b */
		for (j = 0; j < 8; j++) { /* frm_idx */
			data = 0;
			offset = i * 8 + j;
			index = i + j * 4;
			for (k = 0; k < 16; k++)
				data |= (ldc_dither_lut[index][k] << k);
			temp = (data << 16) | data;
			VSYNC_WR_MPEG_REG(LDC_REG_DITHER_LUT_1_0 + offset, temp);
		}
	}
}

static void ldim_profile_load(struct aml_ldim_driver_s *ldim_drv)
{
	struct ldim_profile_s *profile = ldim_drv->dev_drv->bl_profile;

	if (!profile)
		return;

	if (profile->mode != 2)
		return;
	if (strcmp(profile->file_path, "null") == 0) {
		LDIMERR("%s: file_path is null\n", __func__);
		return;
	}
	if (ldim_drv->rmem->flag == 0) {
		LDIMERR("%s: ldc resv mem invalid\n", __func__);
		return;
	}

	LDIMPR("%s: k=%d, bits=%d\n", __func__,
	       profile->profile_k, profile->profile_bits);
	lcd_vcbus_setb(LDC_REG_PROFILE_MODE, profile->profile_k, 8, 16);
	lcd_vcbus_setb(LDC_REG_PROFILE_MODE, profile->profile_bits, 0, 8);

	ldc_mem_write(profile->file_path,
		      ldim_drv->rmem->profile_mem_paddr,
		      ldim_drv->rmem->profile_mem_size);
}

static void ldc_factor_init(unsigned int width, unsigned int height,
			    unsigned int col_num, unsigned int row_num)
{
	unsigned int factor_div_00, factor_div_01, factor_div_10, factor_div_11;
	unsigned int bits_div_00, bits_div_01, bits_div_10, bits_div_11;
	unsigned int dis_x0, dis_y0;
	unsigned int dis_x1, dis_y1;
	unsigned int block_xnum, block_ynum;
	unsigned int dis[4], multifactor[4], bits_for_div[4];
	int i;

	block_xnum = (1 << 7);
	block_ynum = (1 << 6);

	dis_x0 = width / block_xnum;
	dis_y0 = height / block_ynum;

	if (width % block_xnum)
		dis_x1 = dis_x0 + 1;
	else
		dis_x1 = dis_x0;

	if (height % block_ynum)
		dis_y1 = dis_y0 + 1;
	else
		dis_y1 = dis_y0;

	dis[0] = dis_x0 * dis_y0;
	dis[1] = dis_x1 * dis_y0;
	dis[2] = dis_x0 * dis_y1;
	dis[3] = dis_x1 * dis_y1;

	memset(multifactor, 0, 4 * sizeof(unsigned int));
	memset(bits_for_div, 0, 4 * sizeof(unsigned int));
	for (i = 0; i < 4; i++) {
		while (multifactor[i] < (1 << 16)) {
			bits_for_div[i] = bits_for_div[i] + 1;
			multifactor[i] = (1 << bits_for_div[i]) / dis[i];
		}
		bits_for_div[i] = bits_for_div[i] - 1;
		multifactor[i] = (1 << bits_for_div[i]) / dis[i];
	}

	factor_div_00 = multifactor[0];
	factor_div_01 = multifactor[1];
	factor_div_10 = multifactor[2];
	factor_div_11 = multifactor[3];
	bits_div_00 = bits_for_div[0];
	bits_div_01 = bits_for_div[1];
	bits_div_10 = bits_for_div[2];
	bits_div_11 = bits_for_div[3];

//	LDIMPR("ldc_factor_for_div_00/01/10/11:%d, %d, %d, %d\n",
//	       factor_div_00, factor_div_01,
//	       factor_div_10, factor_div_11);
//	LDIMPR("ldc_bits_for_div_00/01/10/11:%d, %d, %d, %d\n",
//	       bits_div_00, bits_div_01,
//	       bits_div_10, bits_div_11);

	lcd_vcbus_setb(LDC_REG_FACTOR_DIV_0, factor_div_00, 16, 16);
	lcd_vcbus_setb(LDC_REG_FACTOR_DIV_0, factor_div_01, 0, 16);
	lcd_vcbus_setb(LDC_REG_FACTOR_DIV_1, factor_div_10, 16, 16);
	lcd_vcbus_setb(LDC_REG_FACTOR_DIV_1, factor_div_11, 0, 16);

	lcd_vcbus_setb(LDC_REG_BITS_DIV, bits_div_00, 24, 8);
	lcd_vcbus_setb(LDC_REG_BITS_DIV, bits_div_01, 16, 8);
	lcd_vcbus_setb(LDC_REG_BITS_DIV, bits_div_10, 8, 8);
	lcd_vcbus_setb(LDC_REG_BITS_DIV, bits_div_11, 0, 8);
//	LDIMPR("%s: LDC_REG_BITS_DIV 0x%x=0x%x\n", __func__,
//	       LDC_REG_BITS_DIV, lcd_vcbus_read(LDC_REG_BITS_DIV));
}

#define MAX_SEG_COL_NUM 48
#define MAX_SEG_ROW_NUM 32
static void ldc_set_t7(struct aml_ldim_driver_s *ldim_drv,
		       unsigned int width, unsigned int height,
		       unsigned int col_num, unsigned int row_num)
{
	unsigned int seg_base;
	unsigned short seg_x_bdy[MAX_SEG_COL_NUM]; //col_num
	unsigned short seg_y_bdy[MAX_SEG_ROW_NUM]; //row_num
	unsigned int temp[2], data;
	unsigned int overlap = 4;
	int i;

//	LDIMPR("width:%d, height:%d, col_num:%d, row_num:%d\n",
//	       width, height, col_num, row_num);

	lcd_vcbus_setb(LDC_REG_PANEL_SIZE, width, 16, 16);
	lcd_vcbus_setb(LDC_REG_PANEL_SIZE, height, 0, 16);
//	LDIMPR("%s: LDC_REG_PANEL_SIZE 0x%x=0x%x\n", __func__,
//	       LDC_REG_PANEL_SIZE, lcd_vcbus_read(LDC_REG_PANEL_SIZE));

	ldc_factor_init(width, height, col_num, row_num);

	lcd_vcbus_setb(LDC_REG_BLOCK_NUM, row_num, 8, 6);
	lcd_vcbus_setb(LDC_REG_BLOCK_NUM, col_num, 14, 6);
	lcd_vcbus_setb(LDC_REG_BLOCK_NUM, 7, 4, 4);
	lcd_vcbus_setb(LDC_REG_BLOCK_NUM, 6, 0, 4);

	lcd_vcbus_setb(LDC_REG_DOWNSAMPLE, 1, 23, 1);
	lcd_vcbus_setb(LDC_REG_DOWNSAMPLE, 0x40, 15, 8);
	lcd_vcbus_setb(LDC_REG_DOWNSAMPLE, 3, 12, 3);
	lcd_vcbus_setb(LDC_REG_DOWNSAMPLE, 1, 11, 1);
	lcd_vcbus_setb(LDC_REG_DOWNSAMPLE, 0x60, 4, 7);
	lcd_vcbus_setb(LDC_REG_DOWNSAMPLE, 13, 0, 4);

	data = width / (overlap * col_num);
	lcd_vcbus_setb(LDC_REG_HIST_OVERLAP, data, 18, 10);
	data = height / (overlap * row_num);
	lcd_vcbus_setb(LDC_REG_HIST_OVERLAP, data, 8, 10);
	lcd_vcbus_setb(LDC_REG_HIST_OVERLAP, 10, 0, 8);

	lcd_vcbus_setb(LDC_REG_BLEND_DIFF_TH, 256, 12, 12);
	lcd_vcbus_setb(LDC_REG_BLEND_DIFF_TH, 640, 0, 12);

	lcd_vcbus_setb(LDC_REG_CURVE_COEF, 0x70, 18, 8);
	lcd_vcbus_setb(LDC_REG_CURVE_COEF, 0x40, 10, 8);
	lcd_vcbus_setb(LDC_REG_CURVE_COEF, 2, 4, 6);
	lcd_vcbus_setb(LDC_REG_CURVE_COEF, 4, 0, 4);

	lcd_vcbus_setb(LDC_REG_INIT_BL, 0, 12, 12);
	lcd_vcbus_setb(LDC_REG_INIT_BL, 0xfff, 0, 12);

	lcd_vcbus_setb(LDC_REG_SF_MODE, 2, 24, 2);
	lcd_vcbus_setb(LDC_REG_SF_MODE, 0x600, 12, 12);
	lcd_vcbus_setb(LDC_REG_SF_MODE, 0xc00, 0, 12);

	lcd_vcbus_setb(LDC_REG_SF_GAIN, 0x20, 8, 8);
	lcd_vcbus_setb(LDC_REG_SF_GAIN, 0x00, 0, 8);

	lcd_vcbus_setb(LDC_REG_BS_MODE, 0, 12, 3);
	lcd_vcbus_setb(LDC_REG_BS_MODE, 0, 0, 12);

	lcd_vcbus_setb(LDC_REG_APL, 0x20, 0, 8);

	lcd_vcbus_setb(LDC_REG_GLB_BOOST, 0x200, 16, 12);
	lcd_vcbus_setb(LDC_REG_GLB_BOOST, 0x20, 8, 8);
	lcd_vcbus_setb(LDC_REG_GLB_BOOST, 0x60, 0, 8);

	lcd_vcbus_setb(LDC_REG_LOCAL_BOOST, 0x20, 20, 8);
	lcd_vcbus_setb(LDC_REG_LOCAL_BOOST, 0x20, 12, 8);
	lcd_vcbus_setb(LDC_REG_LOCAL_BOOST, 0x600, 0, 12);

	lcd_vcbus_setb(LDC_REG_TF, 0x20, 24, 8);
	lcd_vcbus_setb(LDC_REG_TF, 0x20, 16, 8);
	lcd_vcbus_setb(LDC_REG_TF, 0x40, 8, 8);
	lcd_vcbus_setb(LDC_REG_TF, 0x40, 0, 8);

	lcd_vcbus_setb(LDC_DGB_CTRL, 1, 9, 1);
	lcd_vcbus_setb(LDC_REG_TF_SC, 0, 8, 1);
	lcd_vcbus_setb(LDC_REG_TF_SC, 7, 4, 4);
	lcd_vcbus_setb(LDC_REG_TF_SC, 7, 0, 4);

	lcd_vcbus_setb(LDC_REG_BLK_FILTER, 56, 8, 8);
	lcd_vcbus_setb(LDC_REG_BLK_FILTER, 37, 0, 8);

	lcd_vcbus_setb(LDC_REG_BLK_FILTER_COEF, 20, 24, 8);
	lcd_vcbus_setb(LDC_REG_BLK_FILTER_COEF, 10, 16, 8);
	lcd_vcbus_setb(LDC_REG_BLK_FILTER_COEF, 5, 8, 8);
	lcd_vcbus_setb(LDC_REG_BLK_FILTER_COEF, 2, 0, 8);

	lcd_vcbus_setb(LDC_REG_BL_MEMORY, 0, 0, 3);
	//ldc_bl_buf_num=4

	lcd_vcbus_setb(LDC_REG_GLB_GAIN, 896, 0, 12);
	lcd_vcbus_setb(LDC_REG_DITHER, 1, 1, 1);
	lcd_vcbus_setb(LDC_REG_DITHER, 1, 0, 1);

	lcd_vcbus_setb(LDC_DGB_CTRL, 1, 10, 1);
	lcd_vcbus_setb(LDC_DGB_CTRL, 1, 13, 1);
	lcd_vcbus_setb(LDC_DGB_CTRL, 1, 14, 1);

	memset(seg_x_bdy, 0, MAX_SEG_COL_NUM * sizeof(unsigned short));
	memset(seg_y_bdy, 0, MAX_SEG_ROW_NUM * sizeof(unsigned short));
	//data = width / col_num;
	for (i = 0; i < col_num; i++) {
		seg_x_bdy[i] = width * (i + 1) / col_num;
//		LDIMPR("seg_x_bdy[%d]: %d\n", i, seg_x_bdy[i]);
	}
	//data = height / row_num;
	for (i = 0; i < row_num; i++) {
		seg_y_bdy[i] = height * (i + 1) / row_num;
//		LDIMPR("seg_y_bdy[%d]: %d\n", i, seg_y_bdy[i]);
	}

	seg_base = LDC_REG_SEG_X_BOUNDARY_0_1;
	for (i = 0; i < MAX_SEG_COL_NUM; i = i + 2) {
		if (i < col_num) {
			lcd_vcbus_setb(seg_base, seg_x_bdy[i], 14, 14);
			temp[0] = seg_x_bdy[i];
		} else {
			lcd_vcbus_setb(seg_base, 0, 14, 14);
			temp[0] = 0;
		}

		if ((i + 1) < col_num) {
			lcd_vcbus_setb(seg_base, seg_x_bdy[i + 1], 0, 14);
			temp[1] = seg_x_bdy[i + 1];
		} else {
			lcd_vcbus_setb(seg_base, 0, 0, 14);
			temp[1] = 0;
		}

//		LDIMPR("SEG_X addr: 0x%x, %d,%d, readback: 0x%x\n",
//		       seg_base, temp[0], temp[1],
//		       lcd_vcbus_read(seg_base));
		seg_base = seg_base + 1;
	}

	seg_base = LDC_REG_SEG_Y_BOUNDARY_0_1;
	for (i = 0; i < MAX_SEG_ROW_NUM; i = i + 2) {
		if (i < row_num) {
			lcd_vcbus_setb(seg_base, seg_y_bdy[i], 13, 13);
			temp[0] = seg_y_bdy[i];
		} else {
			lcd_vcbus_setb(seg_base, 0, 13, 13);
			temp[0] = 0;
		}

		if ((i + 1) < row_num) {
			lcd_vcbus_setb(seg_base, seg_y_bdy[i + 1], 0, 13);
			temp[1] = seg_y_bdy[i + 1];
		} else {
			lcd_vcbus_setb(seg_base, 0, 0, 13);
			temp[1] = 0;
		}

//		LDIMPR("SEG_Y addr: 0x%x, %d,%d, readback: 0x%x\n",
//		       seg_base, temp[0], temp[1],
//		       lcd_vcbus_read(seg_base));
		seg_base = seg_base + 1;
	}

	lcd_vcbus_setb(LDC_CTRL_MISC0, 0, 28, 1);
	lcd_vcbus_setb(LDC_CTRL_MISC0, 1, 17, 1);

	lcd_vcbus_setb(LDC_CTRL_MISC1, 0, 2, 1);
	lcd_vcbus_setb(LDC_CTRL_MISC1, 1, 2, 1);
	lcd_vcbus_setb(LDC_CTRL_MISC1, 0, 2, 1);

	lcd_vcbus_setb(LDC_CTRL_MISC1, 0, 7, 1);
	lcd_vcbus_setb(LDC_CTRL_MISC1, 0, 6, 1);
	lcd_vcbus_setb(LDC_CTRL_MISC1, 0, 5, 1);
	lcd_vcbus_setb(LDC_CTRL_MISC1, 0, 4, 1);

	lcd_vcbus_setb(LDC_CTRL_MISC1, 2000, 8, 14);

	lcd_vcbus_setb(LDC_ADJ_VS_CTRL, 10, 0, 16);

	LDIMPR("%s ok\n", __func__);
}

static void ldc_rmem_global_hist_get(struct aml_ldim_driver_s *ldim_drv)
{
	struct ldim_stts_s *stts;
	unsigned int *global_hist;
	unsigned char *buf, *p;
	unsigned int row, col, temp = 0;
	int i, j, k;

	if (!ldim_drv->stts || !ldim_drv->stts->global_hist) {
		LDIMERR("%s: global_hist buf is null\n", __func__);
		return;
	}

	stts = ldim_drv->stts;
	global_hist = ldim_drv->stts->global_hist;
	buf = ldim_drv->rmem->global_hist_vaddr;
	if (!buf)
		return;

	row = ldim_drv->conf->seg_row;
	col = ldim_drv->conf->seg_col;

	p = buf + 0x10; //valid data begin at offset 0x10
	j = 0;
	for (i = 0; i < 64; i++) {
		temp = 0;
		if (i % 8 == 0)
			j++;
		for (k = 0; k < 4; k++)
			temp |= (p[i * 3 + k + j - 1] << (k * 8));
		global_hist[i] = (temp >> (i % 8)) & 0x1ffffff;
	}

	stts->global_hist_sum = lcd_vcbus_read(LDC_RO_GLB_HIST_SUM);
	stts->global_hist_cnt = lcd_vcbus_read(LDC_RO_GLB_HIST_CNT);
	stts->global_apl = (stts->global_hist_cnt == 0) ? 0xfff :
		(stts->global_hist_sum / stts->global_hist_cnt) << 4;
	lcd_vcbus_setb(LDC_REG_BS_MODE, stts->global_apl, 0, 12);
	ldim_drv->fw->ctrl->prm_ldc->ldc_glb_apl = stts->global_apl;
}

static unsigned int ldc_seg_hist_offset[2] = {
	0,
	(LDC_SEG_HIST1_OFFSET - LDC_SEG_HIST0_OFFSET)
};

static void ldc_rmem_seg_hist_get(struct aml_ldim_driver_s *ldim_drv)
{
	struct ldim_seg_hist_s *seg_hist;
	unsigned char *buf, *p;
	unsigned int fid, row, col, index, temp = 0;
	int i, j, k;

	if (!ldim_drv->stts->seg_hist) {
		LDIMERR("%s: seg_hist buf is null\n", __func__);
		return;
	}
	if (!ldim_drv->rmem->seg_hist_vaddr) {
		LDIMERR("%s: duty_vaddr is null\n", __func__);
		return;
	}

	fid = lcd_vcbus_getb(LDC_RO_BL_MEMORY_IDX, 8, 1) & 0x1;
	ldim_drv->rmem->hist_fid = fid;

	seg_hist = ldim_drv->stts->seg_hist;
	buf = ldim_drv->rmem->seg_hist_vaddr + ldc_seg_hist_offset[fid];

	row = ldim_drv->conf->seg_row;
	col = ldim_drv->conf->seg_col;

	index = 0; /* 6bytes per seg */
	for (i = 0; i < row; i++) {
		p = buf + (i * 0x120);
		for (j = 0; j < col; j++) {
			temp = 0;
			for (k = 0; k < 3; k++)
				temp |= (p[j * 6 + k] << (k * 8));
			seg_hist[index].weight_avg = temp & 0xfff;
			seg_hist[index].weight_avg_95 = (temp >> 12) & 0xfff;
			temp = 0;
			for (k = 0; k < 3; k++)
				temp |= (p[j * 6 + k + 3] << (k * 8));
			seg_hist[index].max_index = temp & 0xfff;
			seg_hist[index].min_index = (temp >> 12) & 0xfff;
			index++;
		}
	}
}

static unsigned int ldc_duty_offset[4] = {
	0,
	(LDC_SEG_DUTY1_OFFSET - LDC_SEG_DUTY0_OFFSET),
	(LDC_SEG_DUTY2_OFFSET - LDC_SEG_DUTY0_OFFSET),
	(LDC_SEG_DUTY3_OFFSET - LDC_SEG_DUTY0_OFFSET)
};

static void ldc_rmem_duty_get(struct aml_ldim_driver_s *ldim_drv)
{
	unsigned int *bl_matrix;
	unsigned char *buf, *p;
	unsigned int fid, row, col, n, zone_num, index;
	unsigned int temp = 0, m, max;
	int i, j, k;

	if (!ldim_drv->fw->bl_matrix) {
		LDIMERR("%s: bl_matrix buf is null\n", __func__);
		return;
	}
	if (!ldim_drv->rmem->duty_vaddr) {
		LDIMERR("%s: duty_vaddr is null\n", __func__);
		return;
	}

	fid = lcd_vcbus_getb(LDC_RO_BL_MEMORY_IDX, 4, 4) & 0x3;
	ldim_drv->rmem->duty_fid = fid;

	bl_matrix = ldim_drv->fw->bl_matrix;
	buf = ldim_drv->rmem->duty_vaddr + ldc_duty_offset[fid];

	row = ldim_drv->conf->seg_row;
	col = ldim_drv->conf->seg_col;
	zone_num = row * col;

	n = 3; /* 3bytes 2 seg */
	for (i = 0; i < row; i++) {
		if (ldim_drv->hist_en == 0)
			pr_info("%s: row[%d]:\n", __func__, i);
		p = buf + (i * 0x50);
		for (j = 0; j < ((col + 1) / 2); j++) {
			max = (i + 1) * col;
			if (max > zone_num)
				break;
			temp = 0;
			for (k = 0; k < n; k++) {
				temp |= (p[j * n + k] << (k * 8));
				m = i * 0x50 + j * n + k;
				if (ldim_drv->hist_en == 0) {
					pr_info("    buf[0x%x]=0x%x, p[0x%x]=0x%x\n",
						m, buf[m], j * n + k, p[j * n + k]);
				}
			}
			index = i * col + j * 2;
			if (ldim_drv->hist_en == 0) {
				pr_info("  index=%d %d, zone_num=%d\n",
					index, index + 1, zone_num);
			}
			if (index >= max)
				break;
			bl_matrix[index] = temp & 0xfff;
			if ((index + 1) >= max)
				break;
			bl_matrix[index + 1] = (temp >> 12) & 0xfff;
		}
	}
}

static void ldc_rmem_duty_set(struct aml_ldim_driver_s *ldim_drv)
{
	unsigned int *bl_matrix;
	unsigned char *buf, *p;
	unsigned int fid, row, col, n, zone_num, index;
	unsigned int temp = 0, max;
	int i, j, k;

	if (!ldim_drv->fw->bl_matrix) {
		LDIMERR("%s: bl_matrix buf is null\n", __func__);
		return;
	}
	if (!ldim_drv->rmem->duty_vaddr) {
		LDIMERR("%s: duty_vaddr is null\n", __func__);
		return;
	}

	bl_matrix = ldim_drv->fw->bl_matrix;

	if (ldim_drv->test_remap_en) {
		ldim_drv->rmem->duty_fid = 0;
		fid = ldim_drv->rmem->duty_fid;
		buf = ldim_drv->rmem->duty_vaddr;
		memset(buf + ldc_duty_offset[1], 0, 0xa00);
		memset(buf + ldc_duty_offset[2], 0x1f, 0xa00);
		memset(buf + ldc_duty_offset[3], 0xff, 0xa00);
	} else {
		ldim_drv->rmem->duty_fid++;
		ldim_drv->rmem->duty_fid %= 4;
		fid = ldim_drv->rmem->duty_fid;
		buf = ldim_drv->rmem->duty_vaddr + ldc_duty_offset[fid];
	}

	row = ldim_drv->conf->seg_row;
	col = ldim_drv->conf->seg_col;
	zone_num = row * col;

	n = 3; /* 3bytes 2 seg */
	for (i = 0; i < row; i++) {
		p = buf + (i * 0x50);
		for (j = 0; j < ((col + 1) / 2); j++) {
			max = (i + 1) * col;
			if (max > zone_num)
				break;

			index = i * col + j * 2;
			if (index >= max)
				break;
			temp = bl_matrix[index] & 0xfff;
			if ((index + 1) < max)
				temp |= ((bl_matrix[index + 1] & 0xfff) << 12);

			for (k = 0; k < n; k++)
				p[j * n + k] = (temp >> (k * 8)) & 0xff;
		}
	}

	lcd_vcbus_setb(LDC_CTRL_MISC0, fid, 23, 2);
}

void ldim_hw_remap_en_t7(struct aml_ldim_driver_s *ldim_drv, int flag)
{
	unsigned int data;
	unsigned int wflag;

	data = VSYNC_RD_MPEG_REG(LDC_DGB_CTRL);
	wflag = (data >> 14) & 0x0001;
	LDIMPR("%s, data = 0x%x, pre = %d, cur = %d\n", __func__, data, wflag, flag);
	if (wflag != flag) {
		data = data & 0x3E00;
		if (flag) {
			ldim_drv->comp->ldc_comp_en = 1;
			ldim_drv->state |= LDIM_STATE_REMAP_EN;
			data = data | (1 << 14);
		} else {
			ldim_drv->state &= ~LDIM_STATE_REMAP_EN;
			ldim_drv->comp->ldc_comp_en = 0;
		}
		ldim_wr_reg_bits_rdma(LDC_DGB_CTRL, data, 0, 15);
	}
}

void ldim_config_update_t7(struct aml_ldim_driver_s *ldim_drv)
{
	if ((ldim_drv->fw->fw_sel & 0x01) == 0) { //hw update duty
		ldim_wr_reg_bits_rdma(LDC_SEG_INFO_SEL, 0, 0, 1);
		ldim_wr_reg_bits_rdma(LDC_CTRL_MISC0, 0, 25, 1);
	} else {//sw update duty
		ldim_wr_reg_bits_rdma(LDC_CTRL_MISC0, 0, 23, 2);
		ldim_wr_reg_bits_rdma(LDC_CTRL_MISC0, 1, 25, 1);
		ldim_wr_reg_bits_rdma(LDC_SEG_INFO_SEL, 1, 0, 1);
	}

	ldim_wr_reg_bits_rdma(LDC_REG_BL_MEMORY,
		       ldim_drv->comp->ldc_bl_buf_diff, 0, 2);
	ldim_wr_reg_bits_rdma(LDC_REG_GLB_GAIN, ldim_drv->comp->ldc_glb_gain, 0, 12);
	ldim_wr_reg_bits_rdma(LDC_REG_DITHER, ldim_drv->comp->ldc_dth_en, 1, 1);
	ldim_wr_reg_bits_rdma(LDC_REG_DITHER, ldim_drv->comp->ldc_dth_bw, 0, 1);
}

void ldim_vs_arithmetic_t7(struct aml_ldim_driver_s *ldim_drv)
{
	unsigned int size;

	if (!ldim_drv->stts)
		return;
	if (!ldim_drv->stts->seg_hist)
		return;

	if (ldim_drv->hist_en == 0)
		return;

	size = ldim_drv->conf->seg_row * ldim_drv->conf->seg_col;
	ldc_rmem_global_hist_get(ldim_drv);
	ldc_rmem_seg_hist_get(ldim_drv);

	if (ldim_drv->alg_en == 0)
		return;

	if ((ldim_drv->fw->fw_sel & 0x01) == 0) {
		ldc_rmem_duty_get(ldim_drv);
		memcpy(ldim_drv->local_bl_matrix, ldim_drv->fw->bl_matrix,
		       size * (sizeof(unsigned int)));
	} else {
		if (!ldim_drv->fw->fw_alg_frm) {
			if (ldim_drv->dbg_vs_cnt == 0) {
				LDIMERR("%s: ldim_fw ko is not installed\n",
					__func__);
			}
			return;
		}

		if (ldim_drv->fw_cus_pre->fw_alg_frm) {
			ldim_drv->fw->fw_sel |= 0x2;  //bit1: 1=have fw_cus_pre
			ldim_drv->fw_cus_pre->fw_alg_frm(ldim_drv->fw_cus_pre,
				ldim_drv->stts);
			memcpy(ldim_drv->fw->fdat->initial_bl,
				ldim_drv->fw_cus_pre->bl_matrix,
				size * (sizeof(unsigned int)));
		} else {
			ldim_drv->fw->fw_sel &= 0xfd;  //bit1: 0=no fw_cus_pre
		}

		ldim_drv->fw->fw_alg_frm(ldim_drv->fw, ldim_drv->stts);

		if (ldim_drv->fw_cus_post->fw_alg_frm) {
			ldim_drv->fw->fw_sel |= 0x4;  //bit2: 1=have fw_cus_post
			memcpy(ldim_drv->fw_cus_post->bl_matrix,
					ldim_drv->fw->bl_matrix,
					size * (sizeof(unsigned int)));
			ldim_drv->fw_cus_post->fw_alg_frm(ldim_drv->fw_cus_post,
				ldim_drv->stts);
			memcpy(ldim_drv->fw->bl_matrix,
				ldim_drv->fw_cus_post->bl_matrix,
				size * (sizeof(unsigned int)));
		} else {
			ldim_drv->fw->fw_sel &= 0xfb;  //bit2: 0=no fw_cus_post
		}

		memcpy(ldim_drv->local_bl_matrix, ldim_drv->fw->bl_matrix,
		       size * (sizeof(unsigned int)));
		ldc_rmem_duty_set(ldim_drv);
	}
}

void ldim_func_ctrl_t7(struct aml_ldim_driver_s *ldim_drv, int flag)
{
	if (ldim_drv->in_vsync_flag == 0) {
		if (ldim_debug_print)
			LDIMPR("%s: exit for in_vsync_flag=0\n", __func__);
		return;
	}

	if (flag) {
		if (ldim_drv->ld_sel == 0) {
			if (ldim_debug_print)
				LDIMPR("%s: exit for ld_sel=0\n", __func__);
			return;
		}

		if (ldim_drv->func_en == ldim_drv->conf->func_en) {
			if (ldim_debug_print)
				LDIMPR("%s: func_en is same as conf.func_en\n", __func__);
			return;
		}

		ldim_wr_reg_bits_rdma(LDC_REG_BLOCK_NUM, 1, 20, 1);

		ldim_drv->switch_ld_cnt++;
		if (ldim_debug_print)
			LDIMPR("%s: switch_ld_cnt = %d\n", __func__, ldim_drv->switch_ld_cnt);
		if (ldim_drv->switch_ld_cnt < 4)
			return;
		ldim_drv->switch_ld_cnt = 0;
		ldim_drv->remap_en = ldim_drv->conf->remap_en;
		ldim_hw_remap_en_t7(ldim_drv, ldim_drv->conf->remap_en);

		ldim_drv->top_en = 1;
		ldim_drv->hist_en = 1;
		ldim_drv->alg_en = 1;
		ldim_drv->func_en = 1;
		ldim_drv->dev_drv->config_update(ldim_drv);

		ldim_drv->state |= LDIM_STATE_FUNC_EN;
	} else {
		ldim_drv->state &= ~LDIM_STATE_FUNC_EN;

		ldim_drv->func_en = 0;
		ldim_drv->top_en = 0;
		ldim_drv->hist_en = 0;
		ldim_drv->alg_en = 0;
		ldim_drv->switch_ld_cnt = 0;

		ldim_wr_reg_bits_rdma(LDC_REG_BLOCK_NUM, 0, 20, 1);
		ldim_hw_remap_en_t7(ldim_drv, 0);

		ldim_drv->remap_en = 0;
		ldim_drv->dev_drv->config_update(ldim_drv);

		/* refresh system brightness */
		ldim_drv->level_update = 1;
	}

	LDIMPR("%s: %d\n", __func__, flag);
}

void ldim_drv_init_t7(struct aml_ldim_driver_s *ldim_drv)
{
	unsigned int width, height, col_num, row_num;

	if (!ldim_drv)
		return;

	/*init */
	if (ldim_drv->dev_drv)
		ldim_profile_load(ldim_drv);

	width = ldim_drv->conf->hsize;
	height = ldim_drv->conf->vsize;
	col_num = ldim_drv->conf->seg_col;
	row_num = ldim_drv->conf->seg_row;

	lcd_vcbus_write(VPU_RDARB_MODE_L2C1, 0); //change ldc to vpu read0

	lcd_vcbus_write(LDC_REG_BLOCK_NUM, 0);
	lcd_vcbus_write(LDC_DDR_ADDR_BASE, (ldim_drv->rmem->profile_mem_paddr >> 2));

	ldc_set_t7(ldim_drv, width, height, col_num, row_num);
	ldim_drv->pq_updating = 1;

	LDIMPR("drv_init: col: %d, row: %d, axi paddr: 0x%lx\n",
		col_num, row_num, (unsigned long)ldim_drv->rmem->rsv_mem_paddr);
}

void ldim_drv_init_t3(struct aml_ldim_driver_s *ldim_drv)
{
	unsigned int width, height, col_num, row_num;

	if (!ldim_drv)
		return;

	/*init */
	if (ldim_drv->dev_drv)
		ldim_profile_load(ldim_drv);

	width = ldim_drv->conf->hsize;
	height = ldim_drv->conf->vsize;
	col_num = ldim_drv->conf->seg_col;
	row_num = ldim_drv->conf->seg_row;

	lcd_vcbus_write(LDC_REG_BLOCK_NUM, 0);
	lcd_vcbus_write(LDC_DDR_ADDR_BASE, (ldim_drv->rmem->profile_mem_paddr >> 2));

	ldc_set_t7(ldim_drv, width, height, col_num, row_num);
	ldim_drv->pq_updating = 1;

	LDIMPR("drv_init: col: %d, row: %d, axi paddr: 0x%lx\n",
		col_num, row_num, (unsigned long)ldim_drv->rmem->rsv_mem_paddr);
}
