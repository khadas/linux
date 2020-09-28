#ifndef __LINUX_YK_MFD_618_H_
#define __LINUX_YK_MFD_618_H_
/* Unified sub device IDs for YK */

/*For YK618*/
#define YK618                   (216)
#define YK_STATUS              (0x00)
#define YK_MODE_CHGSTATUS      (0x01)
#define YK_IC_TYPE			    (0x03)
#define YK_BUFFER1             (0x04)
#define YK_BUFFER2             (0x05)
#define YK_BUFFER3             (0x06)
#define YK_BUFFER4             (0x07)
#define YK_BUFFER5             (0x08)
#define YK_BUFFER6             (0x09)
#define YK_BUFFER7             (0x0A)
#define YK_BUFFER8             (0x0B)
#define YK_BUFFER9             (0x0C)
#define YK_BUFFERA             (0x0D)
#define YK_BUFFERB             (0x0E)
#define YK_BUFFERC             (0x0F)
#define YK_IPS_SET             (0x30)
#define YK_VOFF_SET            (0x31)
#define YK_OFF_CTL             (0x32)
#define YK_CHARGE1             (0x33)
#define YK_CHARGE2             (0x34)
#define YK_CHARGE3             (0x35)
#define YK_POK_SET             (0x36)
#define YK_INTEN1              (0x40)
#define YK_INTEN2              (0x41)
#define YK_INTEN3              (0x42)
#define YK_INTEN4              (0x43)
#define YK_INTEN5              (0x44)
#define YK_INTSTS1             (0x48)
#define YK_INTSTS2             (0x49)
#define YK_INTSTS3             (0x4A)
#define YK_INTSTS4             (0x4B)
#define YK_INTSTS5             (0x4C)

#define YK_LDO_DC_EN1          (0X10)
#define YK_LDO_DC_EN2          (0X12)
#define YK_LDO_DC_EN3          (0X13)
#define YK_DLDO1OUT_VOL        (0x15)
#define YK_DLDO2OUT_VOL        (0x16)
#define YK_DLDO3OUT_VOL        (0x17)
#define YK_DLDO4OUT_VOL        (0x18)
#define YK_ELDO1OUT_VOL        (0x19)
#define YK_ELDO2OUT_VOL        (0x1A)
#define YK_ELDO3OUT_VOL        (0x1B)
#define YK_DC5LDOOUT_VOL       (0x1C)
#define YK_DC1OUT_VOL          (0x21)
#define YK_DC2OUT_VOL          (0x22)
#define YK_DC3OUT_VOL          (0x23)
#define YK_DC4OUT_VOL          (0x24)
#define YK_DC5OUT_VOL          (0x25)
#define YK_GPIO0LDOOUT_VOL     (0x91)
#define YK_GPIO1LDOOUT_VOL     (0x93)
#define YK_ALDO1OUT_VOL        (0x28)
#define YK_ALDO2OUT_VOL        (0x29)
#define YK_ALDO3OUT_VOL        (0x2A)
#define YK_OFFLEVEL_DELAY      (0x37)
#define YK_DCDC_FREQSET        (0x3B)

#define YK_DCDC_MODESET        (0x80)
#define YK_ADC_EN              (0x82)
#define YK_HOTOVER_CTL         (0x8F)

#define YK_GPIO0_CTL           (0x90)
#define YK_GPIO1_CTL           (0x92)
#define YK_GPIO01_SIGNAL       (0x94)
#define YK_BAT_CHGCOULOMB3     (0xB0)
#define YK_BAT_CHGCOULOMB2     (0xB1)
#define YK_BAT_CHGCOULOMB1     (0xB2)
#define YK_BAT_CHGCOULOMB0     (0xB3)
#define YK_BAT_DISCHGCOULOMB3  (0xB4)
#define YK_BAT_DISCHGCOULOMB2  (0xB5)
#define YK_BAT_DISCHGCOULOMB1  (0xB6)
#define YK_BAT_DISCHGCOULOMB0  (0xB7)
#define YK_COULOMB_CTL         (0xB8)



/* bit definitions for YK events ,irq event */
/*  YK618*/
#define	YK_IRQ_USBLO			( 1 <<  1)
#define	YK_IRQ_USBRE			( 1 <<  2)
#define	YK_IRQ_USBIN			( 1 <<  3)
#define	YK_IRQ_USBOV     	( 1 <<  4)
#define	YK_IRQ_ACRE     		( 1 <<  5)
#define	YK_IRQ_ACIN     		( 1 <<  6)
#define	YK_IRQ_ACOV     		( 1 <<  7)
#define	YK_IRQ_TEMLO      	( 1 <<  8)
#define	YK_IRQ_TEMOV      	( 1 <<  9)
#define	YK_IRQ_CHAOV			( 1 << 10)
#define	YK_IRQ_CHAST 	    ( 1 << 11)
#define	YK_IRQ_BATATOU    	( 1 << 12)
#define	YK_IRQ_BATATIN  		( 1 << 13)
#define YK_IRQ_BATRE			( 1 << 14)
#define YK_IRQ_BATIN		( 1 << 15)
#define YK_IRQ_QBATINWORK	( 1 << 16)
#define YK_IRQ_BATINWORK	( 1 << 17)
#define YK_IRQ_QBATOVWORK	( 1 << 18)
#define YK_IRQ_BATOVWORK	( 1 << 19)
#define YK_IRQ_QBATINCHG	( 1 << 20)
#define YK_IRQ_BATINCHG	( 1 << 21)
#define YK_IRQ_QBATOVCHG	( 1 << 22)
#define YK_IRQ_BATOVCHG	( 1 << 23)
#define YK_IRQ_EXTLOWARN2  	( 1 << 24)
#define YK_IRQ_EXTLOWARN1  	( 1 << 25)
#define YK_IRQ_ICTEMOV    	( 1 << 31)
#define YK_IRQ_GPIO0TG     	((uint64_t)1 << 32)
#define YK_IRQ_GPIO1TG     	((uint64_t)1 << 33)
#define YK_IRQ_POKLO     	((uint64_t)1 << 35)
#define YK_IRQ_POKSH     	((uint64_t)1 << 36)

#define YK_IRQ_PEKFE     	((uint64_t)1 << 37)
#define YK_IRQ_PEKRE     	((uint64_t)1 << 38)
#define YK_IRQ_TIMER     	((uint64_t)1 << 39)


/* Status Query Interface */
/*  YK618 */
#define YK_STATUS_SOURCE    	( 1 <<  0)
#define YK_STATUS_ACUSBSH 	( 1 <<  1)
#define YK_STATUS_BATCURDIR 	( 1 <<  2)
#define YK_STATUS_USBLAVHO 	( 1 <<  3)
#define YK_STATUS_USBVA    	( 1 <<  4)
#define YK_STATUS_USBEN    	( 1 <<  5)
#define YK_STATUS_ACVA	    ( 1 <<  6)
#define YK_STATUS_ACEN	    ( 1 <<  7)

#define YK_STATUS_BATINACT  	( 1 << 11)

#define YK_STATUS_BATEN     	( 1 << 13)
#define YK_STATUS_INCHAR    	( 1 << 14)
#define YK_STATUS_ICTEMOV   	( 1 << 15)

#define YK_LDO1_MIN		3000000
#define YK_LDO1_MAX		3000000
#define YK_ALDO1_MIN		700000
#define YK_ALDO1_MAX		3300000
#define YK_ALDO2_MIN		700000
#define YK_ALDO2_MAX		3300000
#define YK_ALDO3_MIN		700000
#define YK_ALDO3_MAX		3300000

#define YK_ELDO1_MIN		700000
#define YK_ELDO1_MAX		3300000
#define YK_ELDO2_MIN         700000
#define YK_ELDO2_MAX         3300000


#define YK_DCDC1_MIN		1600000
#define YK_DCDC1_MAX		3400000
#define YK_DCDC2_MIN		600000
#define YK_DCDC2_MAX		1540000
#define YK_DCDC3_MIN		600000
#define YK_DCDC3_MAX		1860000
#define YK_DCDC4_MIN		600000
#define YK_DCDC4_MAX		1540000
#define YK_DCDC5_MIN		1000000
#define YK_DCDC5_MAX		2550000


#define GPIO_LDO1_MIN		700000
#define GPIO_LDO1_MAX		3300000

#endif /* __LINUX_YK_MFD_618_H_ */
