/****************************************************************************
*
*    Copyright (c) 2005 - 2020 by Vivante Corp.  All rights reserved.
*
*    The material in this file is confidential and contains trade secrets
*    of Vivante Corporation. This is proprietary information owned by
*    Vivante Corporation. No part of this work may be disclosed,
*    reproduced, copied, transmitted, or used in any way for any purpose,
*    without the express written permission of Vivante Corporation.
*
*****************************************************************************/


#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/of_reserved_mem.h>
#include <linux/cma.h>
#include <linux/dma-contiguous.h>
#include <linux/delay.h>

#include "gc_hal_kernel_linux.h"
#include "gc_hal_kernel_platform.h"

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 10, 0))
#include <dt-bindings/clock/g12a-clkc.h>
#else
#include <dt-bindings/clock/amlogic,g12a-clkc.h>
#endif

/*======== power version 0 hardware reg begin ===========*/
#define AO_RTI_BASE           0xff800000
#define AO_RTI_GEN_PWR_SLEEP0 (AO_RTI_BASE + (0x3a<<2))   //0xff8000e8
#define AO_RTI_GEN_PWR_ISO0   (AO_RTI_BASE + (0x3b<<2))   //0xff8000ec


/*======== power version 1 hardware reg begin ===========*/
#define P_PWRCTRL_ISO_EN1      0xfe007818
#define P_PWRCTRL_PWR_OFF1     0xfe00781c

static    unsigned int HHI_NANOQ_MEM_PD_REG0 = 0xff63c10c;
static    unsigned int HHI_NANOQ_MEM_PD_REG1 = 0xff63c110;
static  unsigned int RESET_LEVEL2 = 0xffd01088;//RESET_LEVEL2 = P_PWRCTRL_FOCRSTN1

static  unsigned int NN_clk = 0xff63c1c8;
static  unsigned int nn_power_version = 0;

//static  unsigned int P_PWRCTRL_FOCRSTN1 = 0xfe007824
//#define HHI_NANOQ_MEM_PD_REG0 0xfe0078a0
//#define HHI_NANOQ_MEM_PD_REG1 0xfe0078a4




//#define PWR_OFF               0
//#define MAX_NANOQ_FREQ        800000000

static int hardwareResetNum = 0;
module_param(hardwareResetNum, int, 0644);
//static int nanoqFreq = 800000000;
//module_param(nanoqFreq, int, 0644);

static void _InitDtsRegValue(IN gcsPLATFORM *Platform)
{
    struct platform_device *pdev = Platform->device;
    struct resource *res;
    int ret;
    res = platform_get_resource(pdev, IORESOURCE_MEM, 2);
    if (res)
    {
        printk("reg resource 2, start: %ld,end: %lx\n",(unsigned long)res->start,(unsigned long)res->end);
        HHI_NANOQ_MEM_PD_REG0 = (unsigned long)res->start;
    }

    res = platform_get_resource(pdev, IORESOURCE_MEM, 3);
    if (res)
    {
        printk("reg resource 3, start: %ld,end: %lx\n",(unsigned long)res->start,(unsigned long)res->end);
        HHI_NANOQ_MEM_PD_REG1 = (unsigned long)res->start;
    }

    res = platform_get_resource(pdev, IORESOURCE_MEM, 4);
    if (res)
    {
        printk("reg resource 4, start: %ld,end: %lx\n",(unsigned long)res->start,(unsigned long)res->end);
        RESET_LEVEL2 = (unsigned long)res->start;
    }
    res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "NN_CLK");
    if (res)
    {
        printk("reg resource NN_CLK, start: %ld,end: %lx\n",(unsigned long)res->start,(unsigned long)res->end);
        NN_clk = (unsigned long)res->start;
    }
    ret = of_property_read_u32(pdev->dev.of_node,"nn_power_version",&nn_power_version);
    return;
}

gceSTATUS _AdjustParam(IN gcsPLATFORM *Platform,OUT gcsMODULE_PARAMETERS *Args)
{
    struct platform_device *pdev = Platform->device;
    struct resource *res;
    int irqLine = platform_get_irq_byname(pdev, "galcore");

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
        //printk("reg resource 0,start:%ld,end:%ld",(unsigned long)res->start,(unsigned long)res->end);
        Args->registerBases[0] = (gctPHYS_ADDR_T)res->start;
        Args->registerSizes[0] = (gctSIZE_T)(res->end - res->start+1);
        //printk("read from dts,regbase:0x%llx,size:0x%lx\n",Args->registerBases[0],Args->registerSizes[0]);
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
        //printk("reg resource 1,start:%ld,end:%ld",(unsigned long)res->start,(unsigned long)res->end);
        Args->extSRAMBases[0] = (gctPHYS_ADDR_T)res->start;
//        Args->sRAMSizes[0][1] = 0x480000;
        Args->contiguousBase = 0;
        Args->contiguousSize = 0x400000;
//        Args->contiguousSize = (gctSIZE_T)(res->end - res->start+1);
    }
    else
    {
        printk("no memory resource 1\n");
        Args->contiguousBase = 0;
        Args->contiguousSize = 0x400000;
        Args->extSRAMBases[0] = 0xFF000000;
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
/*
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
    clk_set_rate(npu_axi_clk, nanoqFreq);

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
    clk_set_rate(npu_core_clk, nanoqFreq);
    return;
}*/
gceSTATUS _GetPower(IN gcsPLATFORM *Platform)
{
    unsigned int readReg=0;
    _InitDtsRegValue(Platform);
    if (nn_power_version == 1)
    {
        _RegRead(RESET_LEVEL2,&readReg);
        readReg = (readReg & (~(1<<3)));
        _RegWrite(RESET_LEVEL2, readReg);

        _RegRead(P_PWRCTRL_PWR_OFF1,&readReg);
        readReg = (readReg & (~(1<<3)));
        _RegWrite(P_PWRCTRL_PWR_OFF1, readReg);

        _RegWrite(HHI_NANOQ_MEM_PD_REG0, 0x0);
        _RegWrite(HHI_NANOQ_MEM_PD_REG1, 0x0);

        _RegRead(P_PWRCTRL_ISO_EN1,&readReg);
        readReg = (readReg & (~(1<<3)));
        _RegWrite(P_PWRCTRL_ISO_EN1, readReg);

        _RegRead(RESET_LEVEL2,&readReg);
        readReg = (readReg | (1<<3));
        _RegWrite(RESET_LEVEL2, readReg);

        _RegWrite(NN_clk, 0x3000300);
    }
    else
    {
        _RegRead(AO_RTI_GEN_PWR_SLEEP0,&readReg);
        readReg = (readReg & 0xfffcffff);
        _RegWrite(AO_RTI_GEN_PWR_SLEEP0, readReg);

        _RegWrite(HHI_NANOQ_MEM_PD_REG0, 0x0);
        _RegWrite(HHI_NANOQ_MEM_PD_REG1, 0x0);

        _RegRead(RESET_LEVEL2,&readReg);
        readReg = (readReg & 0xffffefff);
        _RegWrite(RESET_LEVEL2, readReg);

        _RegRead(AO_RTI_GEN_PWR_ISO0,&readReg);
        readReg = (readReg & 0xfffcffff);
        _RegWrite(AO_RTI_GEN_PWR_ISO0, readReg);

        _RegRead(RESET_LEVEL2,&readReg);
        readReg = (readReg | (0x1<<12));
        _RegWrite(RESET_LEVEL2, readReg);

//        set_clock(Platform->device);
        _RegWrite(NN_clk, 0x7000700);
    }
    return gcvSTATUS_OK;
}

gceSTATUS _DownPower(IN gcsPLATFORM *Platform)
{

    unsigned int readReg=0;
    printk("====>>>>downpower for putpower\n");
    if (nn_power_version == 1)
    {
        _RegRead(P_PWRCTRL_ISO_EN1,&readReg);
        readReg = (readReg | (1<<3));
        _RegWrite(P_PWRCTRL_ISO_EN1, readReg);

        _RegWrite(HHI_NANOQ_MEM_PD_REG0, 0xffffffff);
        _RegWrite(HHI_NANOQ_MEM_PD_REG1, 0xffffffff);

        _RegRead(P_PWRCTRL_PWR_OFF1,&readReg);
        readReg = (readReg | (1<<3));
        _RegWrite(P_PWRCTRL_PWR_OFF1, readReg);
    }
    else
    {
        _RegRead(AO_RTI_GEN_PWR_ISO0,&readReg);
        readReg = (readReg | 0x30000);
        _RegWrite(AO_RTI_GEN_PWR_ISO0, readReg);

        _RegWrite(HHI_NANOQ_MEM_PD_REG0, 0xffffffff);
        _RegWrite(HHI_NANOQ_MEM_PD_REG1, 0xffffffff);

        _RegRead(AO_RTI_GEN_PWR_SLEEP0,&readReg);
        readReg = (readReg | 0x30000);
        _RegWrite(AO_RTI_GEN_PWR_SLEEP0, readReg);
    }
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

    mdelay(10);
    /*==========power on===============*/
    _RegRead(AO_RTI_GEN_PWR_SLEEP0,&readReg);
    readReg = (readReg & 0xfffcffff);
    _RegWrite(AO_RTI_GEN_PWR_SLEEP0, readReg);
    _RegRead(AO_RTI_GEN_PWR_ISO0,&readReg);
    readReg = (readReg & 0xfffcffff);
    _RegWrite(AO_RTI_GEN_PWR_ISO0, readReg);
    _RegWrite(HHI_NANOQ_MEM_PD_REG0, 0x0);
    _RegWrite(HHI_NANOQ_MEM_PD_REG1, 0x0);
    _RegWrite(NN_clk, 0x3000300);
//    set_clock(Platform->device);
    mdelay(1);
    _RegWrite(RESET_LEVEL2, 0xffffffff);
    mdelay(2);
    printk("====>>>>npu hardware reset end!\n");
    hardwareResetNum++;
    if(hardwareResetNum > 10000)
    {
        printk("hardwareResetNum is too large over 10000,just set zero\n");
        hardwareResetNum = 0;
    }

    return gcvSTATUS_OK;   //gcvSTATUS_NOT_SUPPORTED;
}

gceSTATUS  _SetPower(IN gcsPLATFORM * Platform,IN gceCORE GPU,IN gctBOOL Enable)
{
    unsigned int readReg=0;
    printk("_setpower,control status:%d\n",Enable);
    if (nn_power_version == 1)
    {
        if(Enable == 0)
        {
            printk("====>>>>poweroff in _SetPower\n");
            _RegRead(P_PWRCTRL_ISO_EN1,&readReg);
            readReg = (readReg | (1<<3));
            _RegWrite(P_PWRCTRL_ISO_EN1, readReg);

            _RegWrite(HHI_NANOQ_MEM_PD_REG0, 0xffffffff);
            _RegWrite(HHI_NANOQ_MEM_PD_REG1, 0xffffffff);

            _RegRead(P_PWRCTRL_PWR_OFF1,&readReg);
            readReg = (readReg | (1<<3));
            _RegWrite(P_PWRCTRL_PWR_OFF1, readReg);
        }
        else
        {
            printk("====>>>>poweron in _SetPower\n");
            _RegRead(RESET_LEVEL2,&readReg);
            readReg = (readReg & (~(1<<3)));
            _RegWrite(RESET_LEVEL2, readReg);

            _RegRead(P_PWRCTRL_PWR_OFF1,&readReg);
            readReg = (readReg & (~(1<<3)));
            _RegWrite(P_PWRCTRL_PWR_OFF1, readReg);

            _RegWrite(HHI_NANOQ_MEM_PD_REG0, 0x0);
            _RegWrite(HHI_NANOQ_MEM_PD_REG1, 0x0);

            _RegRead(P_PWRCTRL_ISO_EN1,&readReg);
            readReg = (readReg & (~(1<<3)));
            _RegWrite(P_PWRCTRL_ISO_EN1, readReg);

            _RegRead(RESET_LEVEL2,&readReg);
            readReg = (readReg | (1<<3));
            _RegWrite(RESET_LEVEL2, readReg);

            _RegWrite(NN_clk, 0x3000300);
        }
    }
    else
    {
        if(Enable == 0)
        {
            printk("====>>>>poweroff in _SetPower\n");
            _RegRead(AO_RTI_GEN_PWR_ISO0,&readReg);
            readReg = (readReg | 0x30000);
            _RegWrite(AO_RTI_GEN_PWR_ISO0, readReg);
            _RegWrite(HHI_NANOQ_MEM_PD_REG0, 0xffffffff);
            _RegWrite(HHI_NANOQ_MEM_PD_REG1, 0xffffffff);
            _RegRead(AO_RTI_GEN_PWR_SLEEP0,&readReg);
            readReg = (readReg | 0x30000);
            _RegWrite(AO_RTI_GEN_PWR_SLEEP0, readReg);
        }
        else
        {
            printk("====>>>>poweron in _SetPower\n");
            _RegRead(AO_RTI_GEN_PWR_SLEEP0,&readReg);
            readReg = (readReg & 0xfffcffff);
            _RegWrite(AO_RTI_GEN_PWR_SLEEP0, readReg);
            _RegWrite(HHI_NANOQ_MEM_PD_REG0, 0x0);
            _RegWrite(HHI_NANOQ_MEM_PD_REG1, 0x0);
            _RegRead(RESET_LEVEL2,&readReg);
            readReg = (readReg & 0xffffefff);
            _RegWrite(RESET_LEVEL2, readReg);
            _RegRead(AO_RTI_GEN_PWR_ISO0,&readReg);
            readReg = (readReg & 0xfffcffff);
            _RegWrite(AO_RTI_GEN_PWR_ISO0, readReg);
            _RegRead(RESET_LEVEL2,&readReg);
            readReg = (readReg | (0x1<<12));
            _RegWrite(RESET_LEVEL2, readReg);
            _RegWrite(NN_clk, 0x7000700);
        }
    }

    return gcvSTATUS_OK;
}


static gcsPLATFORM_OPERATIONS default_ops =
{
    .adjustParam   = _AdjustParam,
    .getPower  = _GetPower,
//    .reset = _Reset,
    .putPower = _DownPower,
    .setPower = _SetPower,
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

