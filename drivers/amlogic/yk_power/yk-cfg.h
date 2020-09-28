#ifndef __LINUX_YK_CFG_H_
#define __LINUX_YK_CFG_H_
#include "yk-mfd.h"

/*设备地址*/
/*
	一般不改变：
	YK618:0x34
*/
#define	YK_DEVICES_ADDR	(0x68 >> 1)
/*i2c控制器的设备号:具体看所使用平台硬件的连接*/
#define	YK_I2CBUS			1
/*电源芯片对应的中断号：具体看所使用的平台硬件的连接，
中断线nmi连接cpu的哪路irq或者gpio*/
//#define YK_IRQNO			IRQ_EINT(20)
#define YK_IRQNO			6//192//192//64

/*初始化各路输出，单位mV，0都为关闭*/
/*
	ldo1：
		由硬件决定输出电压，软件改不了，只是软件的显示电压：
*/
#define YK_LDO1_VALUE			3000
/*
	aldo1：
		YK:700~3300,100/step
*/
#define YK_ALDO1_VALUE		1800
/*
	aldo2：
		YK:700~3300,100/step
*/
#define YK_ALDO2_VALUE		1000
/*
	aldo3：
		YK:700~3300,100/step
*/
#define YK_ALDO3_VALUE		3300

/*
	eldo1：
		YK:700~3300,100/step
*/
#define YK_ELDO1_VALUE		3300
/*
	eldo2：
		YK:700~3300,100/step
*/
#define YK_ELDO2_VALUE		3300


/*
	DCDC1:
		YK:1600~3400,100/setp
*/
#define YK_DCDC1_VALUE		3200
/*
	DCDC2：
		YK:600~1540，20/step
*/
#define YK_DCDC2_VALUE		1100
/*
	DCDC3：
		YK:600~1860，20/step
*/
#define YK_DCDC3_VALUE		1100
/*
	DCDC4：
		YK:600~1540，20/step
*/
#define YK_DCDC4_VALUE		1000
/*
	DCDC5：
		YK:1000~2550，50/step
*/
#define YK_DCDC5_VALUE		1100

/*电池容量，mAh：根据实际电池容量来定义，对库仑计方法来说
这个参数很重要，必须配置*/
#define BATCAP				5000

/*初始化电池内阻，mΩ：一般在100~200之间，不过最好根据实际
测试出来的确定，方法是打开打印信息，不接电池烧好固件后，
上电池，不接充电器，开机，开机1分钟后，接上充电器，充
1~2分钟，看打印信息中的rdc值，填入这里*/
#define BATRDC				103
/*开路电压方法中的电池电压的缓存*/
#define YK_VOL_MAX			1
/*
	充电功能使能：
        YK:0-关闭，1-打开
*/
#define CHGEN       1

/*
	充电电流设置，uA，0为关闭：
		YK:300~2550,100/step
*/
/*开机充电电流，uA*/
#define STACHGCUR			900*1000
/*关屏充电电流，uA*/
#define EARCHGCUR			900*1000
/*休眠充电电流，uA*/
#define SUSCHGCUR			1500*1000
/*关机充电电流，uA*/
#define CLSCHGCUR			1500*1000

/*目标充电电压，mV*/
/*
	YK:4100000/4200000/4240000/4350000
*/
#define CHGVOL				4200000
/*充电电流小于设置电流的ENDCHGRATE%时，停止充电，%*/
/*
	YK:10\15
*/
#define ENDCHGRATE			10
/*关机电压，mV*/
/*
	系统设计的关机过后的电池端电压，需要与关机百分比、
	开路电压对应百分比表及低电警告电压相互配合才会有作用
*/
#define SHUTDOWNVOL			3300

/*adc采样率设置，Hz*/
/*
	YK:100\200\400\800
*/
#define ADCFREQ				100
/*预充电超时时间，min*/
/*
	YK:40\50\60\70
*/
#define CHGPRETIME			50
/*恒流充电超时时间，min*/
/*
	YK:360\480\600\720
*/
#define CHGCSTTIME			480


/*pek开机时间，ms*/
/*
	按power键硬件开机时间：
		YK:128/1000/2000/3000
*/
#define PEKOPEN				1000
/*pek长按时间，ms*/
/*
	按power键发长按中断的时间，短于此时间是短按，发短按键irq，
	长于此时间是长按，发长按键irq：
		YK:1000/1500/2000/2500
*/
#define PEKLONG				1500
/*pek长按关机使能*/
/*
	按power键超过关机时长硬件关机功能使能：
		YK:0-不关，1-关机
*/
#define PEKOFFEN			1
/*pek长按关机使能后开机选择*/
/*
	按power键超过关机时长硬件关机还是重启选择:
		YK:0-只关机不重启，1-关机后重启
*/
#define PEKOFFRESTART			0
/*pekpwr延迟时间，ms*/
/*
	开机后powerok信号的延迟时间：
		YK20:8/16/32/64
*/
#define PEKDELAY			32
/*pek长按关机时间，ms*/
/*
	按power键的关机时长：
		YK:4000/6000/8000/10000
*/
#define PEKOFF				6000
/*过温关机使能*/
/*
	YK内部温度过高硬件关机功能使能：
		YK:0-不关，1-关机
*/
#define OTPOFFEN			0
/* 充电电压限制使能*/
/*
	YK:0-关闭，1-打开
*/
#define USBVOLLIMEN		1
/*  充电限压，mV，0为不限制*/
/*
	YK:4000~4700，100/step
*/
#define USBVOLLIM			4500
/*  USB充电限压，mV，0为不限制*/
/*
	YK:4000~4700，100/step
*/
#define USBVOLLIMPC			4700

/* 充电电流限制使能*/
/*
	YK:0-关闭，1-打开
*/
#define USBCURLIMEN		1
/* 充电限流，mA，0为不限制*/
/*
	YK:500/900
*/
#define USBCURLIM			0
/* usb 充电限流，mA，0为不限制*/
/*
	YK:500/900
*/
#define USBCURLIMPC			500
/* PMU 中断触发唤醒使能*/
/*
	YK:0-不唤醒，1-唤醒
*/
#define IRQWAKEUP			0
/* N_VBUSEN PIN 功能控制*/
/*
	YK:0-输出，驱动OTG升压模块，1-输入，控制VBUS通路
*/
#define VBUSEN			1
/* ACIN/VBUS In-short 功能设置*/
/*
	YK:0-AC VBUS分开，1-使用VBUS当AC,无单独AC
*/
#define VBUSACINSHORT			0
/* CHGLED 管脚控制设置*/
/*
	YK:0-驱动马达，1-由充电功能控制
*/
#define CHGLEDFUN			1
/* CHGLED LED 类型设置*/
/*
	YK:0-充电时led长亮，1-充电时led闪烁
*/
#define CHGLEDTYPE			0
/* 电池总容量校正使能*/
/*
	YK:0-关闭，1-打开
*/
#define BATCAPCORRENT			0
/* 充电完成后，充电输出使能*/
/*
	YK:0-关闭，1-打开
*/
#define BATREGUEN			0
/* 电池检测功能使能*/
/*
	YK:0-关闭，1-打开
*/
#define BATDET		1
/* PMU重置使能*/
/*
	YK:0-关闭，1-打开按电源键16秒重置PMU功能
*/
#define PMURESET		0
/*低电警告电压1，%*/
/*
	根据系统设计来定：
	YK:5%~20%
*/
#define BATLOWLV1    15
/*低电警告电压2，%*/
/*
	根据系统设计来定：
	YK:0%~15%
*/
#define BATLOWLV2    0

#define ABS(x)				((x) >0 ? (x) : -(x) )

#ifdef	CONFIG_AMLOGIC_YK618
/*YK GPIO start NUM,根据平台实际情况定义*/
#define YK_NR_BASE 100

/*YK GPIO NUM,包括马达驱动、LCD power以及VBUS driver pin*/
#define YK_NR 5

/*初始化开路电压对应百分比表*/
/*
	可以使用默认值，但是最好根据实际测试的电池来确定每级
	对应的剩余百分比，特别需要注意，关机电压SHUTDOWNVOL和电池
	容量开始校准剩余容量百分比BATCAPCORRATE这两级的准确性
	YK适用
*/
#define OCVREG0				0		 //3.13V
#define OCVREG1				0		 //3.27V
#define OCVREG2				0		 //3.34V
#define OCVREG3				0		 //3.41V
#define OCVREG4				0		 //3.48V
#define OCVREG5				1		 //3.52V
#define OCVREG6				2		 //3.55V
#define OCVREG7				3		 //3.57V
#define OCVREG8				4		 //3.59V
#define OCVREG9				5		 //3.61V
#define OCVREGA				5		 //3.63V
#define OCVREGB				6		 //3.64V
#define OCVREGC				8		 //3.66V
#define OCVREGD				9		 //3.7V
#define OCVREGE				13		 //3.73V
#define OCVREGF				17		 //3.77V
#define OCVREG10		 	20                //3.78V
#define OCVREG11		 	23                //3.8V
#define OCVREG12		 	27                //3.82V
#define OCVREG13		 	32                //3.84V
#define OCVREG14		 	38                //3.85V
#define OCVREG15		 	43                //3.87V
#define OCVREG16		 	52                //3.91V
#define OCVREG17		 	57                //3.94V
#define OCVREG18		 	65                //3.98V
#define OCVREG19		 	75                //4.01V
#define OCVREG1A		 	89                //4.05V
#define OCVREG1B		 	95                //4.08V
#define OCVREG1C		 	97                //4.1V
#define OCVREG1D		 	98                //4.12V
#define OCVREG1E		 	99                //4.14V
#define OCVREG1F		 	100                //4.15V

#endif

/*选择需要打开的中断使能*/
static const uint64_t YK_NOTIFIER_ON = (YK_IRQ_USBIN |YK_IRQ_USBRE |
				       		                            YK_IRQ_ACIN |YK_IRQ_ACRE |
				       		                            YK_IRQ_BATIN |YK_IRQ_BATRE |
				       		                            YK_IRQ_CHAST |YK_IRQ_CHAOV |
						                            (uint64_t)YK_IRQ_PEKFE |(uint64_t)YK_IRQ_PEKRE);

/* 需要做插入火牛、usb关机重启进boot时power_start设置为1，否则为0*/
#define POWER_START 0


#endif
