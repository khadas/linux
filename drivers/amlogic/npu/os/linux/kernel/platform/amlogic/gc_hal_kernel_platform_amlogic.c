/****************************************************************************
*
*    The MIT License (MIT)
*
*    Copyright (c) 2014 - 2018 Vivante Corporation
*
*    Permission is hereby granted, free of charge, to any person obtaining a
*    copy of this software and associated documentation files (the "Software"),
*    to deal in the Software without restriction, including without limitation
*    the rights to use, copy, modify, merge, publish, distribute, sublicense,
*    and/or sell copies of the Software, and to permit persons to whom the
*    Software is furnished to do so, subject to the following conditions:
*
*    The above copyright notice and this permission notice shall be included in
*    all copies or substantial portions of the Software.
*
*    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
*    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
*    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
*    DEALINGS IN THE SOFTWARE.
*
*****************************************************************************
*
*    The GPL License (GPL)
*
*    Copyright (C) 2014 - 2018 Vivante Corporation
*
*    This program is free software; you can redistribute it and/or
*    modify it under the terms of the GNU General Public License
*    as published by the Free Software Foundation; either version 2
*    of the License, or (at your option) any later version.
*
*    This program is distributed in the hope that it will be useful,
*    but WITHOUT ANY WARRANTY; without even the implied warranty of
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*    GNU General Public License for more details.
*
*    You should have received a copy of the GNU General Public License
*    along with this program; if not, write to the Free Software Foundation,
*    Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*
*****************************************************************************
*
*    Note: This software is released under dual MIT and GPL licenses. A
*    recipient may use this file under the terms of either the MIT license or
*    GPL License. If you wish to use only one license not the other, you can
*    indicate your decision by deleting one of the above license notices in your
*    version of this file.
*
*****************************************************************************/

#include <linux/io.h>
#include <linux/kernel.h>
#include <dt-bindings/clock/amlogic,g12a-clkc.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/of_reserved_mem.h>
#include <linux/cma.h>
#include <linux/dma-contiguous.h>
#include <linux/delay.h>

#include "gc_hal_kernel_linux.h"
#include "gc_hal_kernel_platform.h"

/*======== add by zxw for g12b hardware reg begin ===========*/
#define AO_RTI_BASE           0xff800000
#define AO_RTI_GEN_PWR_SLEEP0 (AO_RTI_BASE + (0x3a<<2))   //0xff8000e8
#define AO_RTI_GEN_PWR_ISO0   (AO_RTI_BASE + (0x3b<<2))   //0xff8000ec

#define HHI_BASE_ADDR         0xff63c000
#define HHI_NANOQ_MEM_PD_REG0 (HHI_BASE_ADDR+(0x43<<2))
#define HHI_NANOQ_MEM_PD_REG1 (HHI_BASE_ADDR+(0x44<<2))

#define RESET_LEVEL2          0xffd01088

#define MAX_NANOQ_FREQ        800000000

static int hardwareResetNum = 0;
module_param(hardwareResetNum, int, 0644);
/*======== add by zxw for g12b hardware reg define end========*/
/*
static gceSTATUS _CmaAlloc(struct platform_device *pdev,gctSIZE_T NumPages,unsigned long *pmem)
{
	struct page *nano_pages;
	
	nano_pages = dma_alloc_from_contiguous(&pdev->dev,NumPages, 0);
	if(nano_pages == NULL)
	{
		printk("dma_alloc_from contiguous fail\n");
		return gcvSTATUS_OUT_OF_MEMORY;
	}
	else
	{
		*pmem = page_to_phys(nano_pages);
	}
    return gcvSTATUS_OK;	
}

static gceSTATUS _DmaAlloc(struct platform_device *pdev,gctSIZE_T memsize,dma_addr_t *pmem)
{
	u32 gfp = GFP_KERNEL | gcdNOWARN;
	
	dma_alloc_coherent(&pdev->dev, memsize, pmem, gfp);
	if(pmem == NULL)
	{
		printk("dma_alloc_from contiguous fail\n");
		return gcvSTATUS_OUT_OF_MEMORY;
	}

    return gcvSTATUS_OK;	
}
*/

gceSTATUS _AdjustParam(IN gcsPLATFORM *Platform,OUT gcsMODULE_PARAMETERS *Args)
{
    struct platform_device *pdev = Platform->device;
	struct resource *res;
    int irqLine = platform_get_irq_byname(pdev, "galcore");
	//dma_addr_t dma_start = 0;
	//gceSTATUS ret;
    printk("galcore irq number is %d.\n", irqLine);
    if (irqLine < 0) {
        printk("get galcore irq resource error\n");
        irqLine = platform_get_irq(pdev, 0);
        printk("galcore irq number is %d\n", irqLine);
    }
    if (irqLine < 0) return gcvSTATUS_OUT_OF_RESOURCES;
    Args->irqs[gcvCORE_MAJOR] = irqLine;
    /*================read reg value from dts===============*/
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res) 
	{
		/*printk("reg resource 0,start:%ld,end:%ld",(unsigned long)res->start,(unsigned long)res->end);*/
		Args->registerBases[0] = (gctPHYS_ADDR_T)res->start;
		Args->registerSizes[0] = (gctSIZE_T)(res->end - res->start+1);
		/*printk("read from dts,regbase:0x%llx,size:0x%lx\n",Args->registerBases[0],Args->registerSizes[0]);*/
	} 
	else 
	{
		printk("no memory resource 0\n");
		Args->registerBases[0] = 0xFF100000;
		Args->registerSizes[0] = 2 << 10;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (res) 
	{
		/*printk("reg resource 1,start:%ld,end:%ld",(unsigned long)res->start,(unsigned long)res->end);*/
		Args->sRAMBases[0][0] = 0xFFFFFFFF;
		Args->sRAMBases[0][1] = (gctPHYS_ADDR_T)res->start;
		Args->sRAMBases[0][2] = 0xFFFFFFFF;
		Args->contiguousBase = 0;
		Args->contiguousSize = (gctSIZE_T)(res->end - res->start+1);
		/*printk("read from dts,srambase:0x%llx,contiguousize:0x%lx\n",Args->sRAMBases[0][1],Args->contiguousSize);
		if(Args->contiguousSize > 0)
		{
			ret = _DmaAlloc(pdev,Args->contiguousSize,&dma_start);
			if(ret == gcvSTATUS_OK)
			{
				Args->contiguousBase = (gctPHYS_ADDR_T)dma_start;
				printk("contiguousBase use from cma,page size is %ld\n",Args->contiguousSize/PAGE_SIZE);
			}
		}*/
	} 
	else 
	{
		printk("no memory resource 1\n");
		Args->contiguousBase = 0;
		Args->contiguousSize = 0x400000;
		Args->sRAMBases[0][0] = 0xFFFFFFFF;
		Args->sRAMBases[0][1] = 0xFF000000;
		Args->sRAMBases[0][2] = 0xFFFFFFFF;
	}	
	Args->registerSizes[0] = 0x20000;
    return gcvSTATUS_OK;
}

int _RegWrite(unsigned int reg, unsigned int writeval)
{
	void __iomem *vaddr;
	reg = round_down(reg, 0x3);
	
	vaddr = ioremap(reg, 0x4);
	writel(writeval, vaddr);
	iounmap(vaddr);
	
	return 0;
}

int _RegRead(unsigned int reg,unsigned int *readval)
{
	void __iomem *vaddr;
	reg = round_down(reg, 0x3);
	vaddr = ioremap(reg, 0x4);
	*readval = readl(vaddr);
	iounmap(vaddr);
	return 0;
}
//us
void delay(unsigned int time)
{
	int i,j;
	for(j=0;j<1000;j++)
	{
		for(i = 0;i<time;i++);
	}
}

static void set_clock(struct platform_device *pdev)
{
	struct clk *npu_axi_clk = NULL;
	struct clk *npu_core_clk = NULL;
	npu_axi_clk = clk_get(&pdev->dev, "cts_vipnanoq_axi_clk_composite");
	if (IS_ERR(npu_axi_clk))
	{
		printk("%s: get npu_axi_clk error!!!\n", __func__);
		return;
	}
	else
	{
		clk_prepare_enable(npu_axi_clk);
	}
	clk_set_rate(npu_axi_clk, MAX_NANOQ_FREQ);
	
	npu_core_clk = clk_get(&pdev->dev, "cts_vipnanoq_core_clk_composite");
	if (IS_ERR(npu_core_clk))
	{
		printk("%s: get npu_core_clk error!!!\n", __func__);
		return;
	}
	else
	{
		clk_prepare_enable(npu_core_clk);
	}
	clk_set_rate(npu_core_clk, MAX_NANOQ_FREQ);
	return;
}
gceSTATUS _GetPower(IN gcsPLATFORM *Platform)
{
	unsigned int readReg=0;
	_RegRead(AO_RTI_GEN_PWR_SLEEP0,&readReg);
	readReg = (readReg & 0xfffcffff);
	_RegWrite(AO_RTI_GEN_PWR_SLEEP0, readReg);
	_RegRead(AO_RTI_GEN_PWR_ISO0,&readReg);
	readReg = (readReg & 0xfffcffff);
	_RegWrite(AO_RTI_GEN_PWR_ISO0, readReg);
	_RegWrite(HHI_NANOQ_MEM_PD_REG0, 0x0);
	_RegWrite(HHI_NANOQ_MEM_PD_REG1, 0x0);
	set_clock(Platform->device);
	delay(500);
    return gcvSTATUS_OK;
}

gceSTATUS _Reset(IN gcsPLATFORM * Platform, IN gceCORE GPU)
{
	unsigned int readReg=0;

	printk("====>>>>begin npu hardware reset!\n");
	_RegWrite(RESET_LEVEL2, 0xffffefff);
	/*==========power off=============*/
	_RegWrite(HHI_NANOQ_MEM_PD_REG0, 0xffffffff);
	_RegWrite(HHI_NANOQ_MEM_PD_REG1, 0xffffffff);
	_RegRead(AO_RTI_GEN_PWR_ISO0,&readReg);
	readReg = (readReg | 0x30000);
	_RegWrite(AO_RTI_GEN_PWR_ISO0, readReg);
	
	_RegRead(AO_RTI_GEN_PWR_SLEEP0,&readReg);
	readReg = (readReg | 0x30000);
	_RegWrite(AO_RTI_GEN_PWR_SLEEP0, readReg);

	mdelay(100);	
	/*==========power on===============*/	
	_RegRead(AO_RTI_GEN_PWR_SLEEP0,&readReg);
	readReg = (readReg & 0xfffcffff);
	_RegWrite(AO_RTI_GEN_PWR_SLEEP0, readReg);
	_RegRead(AO_RTI_GEN_PWR_ISO0,&readReg);
	readReg = (readReg & 0xfffcffff);
	_RegWrite(AO_RTI_GEN_PWR_ISO0, readReg);
	_RegWrite(HHI_NANOQ_MEM_PD_REG0, 0x0);
	_RegWrite(HHI_NANOQ_MEM_PD_REG1, 0x0);
	set_clock(Platform->device);
	mdelay(10);
	_RegWrite(RESET_LEVEL2, 0xffffffff);
	mdelay(20);	      
	printk("====>>>>npu hardware reset end!\n");
	hardwareResetNum++;
	if(hardwareResetNum > 10000)
	{
		printk("hardwareResetNum is too large over 10000,just set zero\n");
		hardwareResetNum = 0;
	}

    return gcvSTATUS_NOT_SUPPORTED;
}

static gcsPLATFORM_OPERATIONS default_ops =
{
    .adjustParam   = _AdjustParam,
	.getPower  = _GetPower,
	.reset = _Reset,
};

static gcsPLATFORM default_platform =
{
    .name = __FILE__,
    .ops  = &default_ops,
};


static struct platform_device *default_dev;

static const struct of_device_id galcore_dev_match[] = {
    {
        .compatible = "amlogic, galcore"
    },
    { },
};

int gckPLATFORM_Init(struct platform_driver *pdrv, gcsPLATFORM **platform)
{
    pdrv->driver.of_match_table = galcore_dev_match;

    *platform = &default_platform;
	/*  default_dev = platform;  hot plug just not support  */
    return 0;
}

int gckPLATFORM_Terminate(gcsPLATFORM *platform)
{
    if (default_dev) {
        platform_device_unregister(default_dev);
        default_dev = NULL;
    }

    return 0;
}

