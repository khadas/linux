// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include "vpp_pq_table.h"
#include "vpp_common.h"
#include "vpp_data.h"

static struct data_vs_param_s cur_vs_param;

static struct vpp_pq_tuning_reg_s lc_param[EN_LC_PARAM_MAX] = {
	{0x00, 4, 5, 0},
	{0x00, 8, 9, 0},
	{0x02, 0, 7, 0},
	{0x02, 8, 15, 0},
	{0x03, 0, 9, 0},
	{0x03, 16, 25, 0},
	{0x04, 0, 7, 0},
	{0x04, 8, 15, 0},
	{0x04, 16, 23, 0},
	{0x04, 24, 31, 0},
	{0x05, 0, 7, 0},
	{0x05, 8, 15, 0},
	{0x06, 0, 7, 0},
	{0x06, 8, 15, 0},
	{0x07, 0, 7, 0},
	{0x07, 16, 19, 0},
	{0x09, 0, 7, 0},
	{0x09, 8, 15, 0},
};

/*Internal functions*/
static void _update_data_page_reg(struct vpp_pq_tuning_page_s *pdata)
{
	int i, j;
	int reg_count = 0;
	unsigned char start = 0;
	unsigned char len = 0;
	unsigned int tmp = 0;
	struct vpp_pq_tuning_reg_s *preg_list;
	struct vpp_pq_tuning_reg_s *pcur_reg;
	struct vpp_pq_page_s *pcur_page;

	if (!pdata)
		return;

	if (!pdata->preg_list || pdata->reg_count == 0)
		return;

	preg_list = kcalloc(pdata->reg_count,
		sizeof(struct vpp_pq_tuning_reg_s), GFP_KERNEL);

	if (!preg_list)
		return;

	memcpy(preg_list, pdata->preg_list,
		sizeof(struct vpp_pq_tuning_reg_s) * pdata->reg_count);

	for (i = 0; i < EN_PAGE_MODULE_MAX; i++) {
		if (pq_table[i].page_addr == pdata->page_addr) {
			pcur_page = &pq_table[i].page[0];
			reg_count = pq_table[i].count;
			break;
		}
	}

	for (i = 0; i < pdata->reg_count; i++) {
		pcur_reg = preg_list + i;
		for (j = 0; j < reg_count; j++) {
			if (pcur_page->reg[j].addr == pcur_reg->reg_addr) {
				start = pcur_reg->bit_start;
				len = pcur_reg->bit_end - pcur_reg->bit_start + 1;
				tmp = pcur_page->reg[j].val;
				tmp = vpp_insert_int(tmp, pcur_reg->val, start, len);
				pcur_page->reg[j].val = tmp;
				break;
			}
		}
	}

	kfree(preg_list);
}

static void _write_data_table(enum vpp_page_module_e module,
	int index)
{
	int i = 0;
	int val = 0;
	int tmp = 0;
	int mask_index = 0;
	unsigned int addr = 0;

	if (module > EN_PAGE_MODULE_MAX - 1 ||
		index > PAGE_TBL_COUNT_MAX - 1)
		return;

	for (i = 0; i < pq_table[module].count; i++) {
		val = pq_table[module].page[index].reg[i].val;
		mask_index = pq_table[module].page[index].reg[i].mask_type;
		val &= mask_list[mask_index];
		addr = ADDR_PARAM(pq_table[module].page_addr,
			pq_table[module].page[index].reg[i].addr);

		tmp = READ_VPP_REG_BY_MODE(EN_MODE_DIR, addr);
		val |= tmp & (~mask_list[mask_index]);
		WRITE_VPP_REG_BY_MODE(EN_MODE_RDMA, addr, val);
	}
}

/*External functions*/
int vpp_data_init(struct vpp_dev_s *pdev)
{
	enum vpp_chip_type_e chip_id;

	chip_id = pdev->pm_data->chip_id;

	memset(&cur_vs_param, 0, sizeof(cur_vs_param));

	return 0;
}

void vpp_data_update_data_table_reg(struct vpp_pq_tuning_table_s *pdata)
{
	int i;
	struct vpp_pq_tuning_page_s *ppage_list;

	if (!pdata)
		return;

	if (!pdata->ppage_list || pdata->page_count == 0)
		return;

	ppage_list = kcalloc(pdata->page_count,
		sizeof(struct vpp_pq_tuning_page_s), GFP_KERNEL);

	if (!ppage_list)
		return;

	memcpy(ppage_list, pdata->ppage_list,
		sizeof(struct vpp_pq_tuning_page_s) * pdata->page_count);

	for (i = 0; i < pdata->page_count; i++)
		_update_data_page_reg(ppage_list + i);

	kfree(ppage_list);
}

void vpp_data_updata_reg_lc(struct vpp_lc_param_s *pdata)
{
	int i;
	struct vpp_pq_tuning_page_s page;

	if (!pdata)
		return;

	for (i = 0; i < EN_LC_PARAM_MAX; i++)
		lc_param[i].val = pdata->param[i];

	page.page_addr = pq_table[EN_PAGE_MODULE_LC].page_addr;
	page.reg_count = EN_LC_PARAM_MAX;
	page.page_idx = 0;
	page.preg_list = &lc_param[0];

	_update_data_page_reg(&page);
}

void vpp_data_on_vs(struct data_vs_param_s *pvs_param)
{
	int i;

	if (!pvs_param)
		return;

	memcpy(&cur_vs_param, pvs_param, sizeof(cur_vs_param));

	for (i = 0; i < EN_PAGE_MODULE_MAX; i++)
		_write_data_table(i, 0);
}

