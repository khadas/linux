#ifndef _REMOTE_H
#define _REMOTE_H
#include <asm/ioctl.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/io.h>
#include <linux/amlogic/iomap.h>
#ifdef REMOTE_FIQ
#include <plat/fiq_bridge.h>
#endif

/*suspend relative ao register*/

/*remote register*/
#define LDR_ACTIVE 0x0
#define LDR_IDLE 0x1
#define LDR_REPEAT 0x2
#define DURATION_REG0    0x3
#define OPERATION_CTRL_REG0 0x4
#define FRAME_BODY 0x5
#define DURATION_REG1_AND_STATUS 0x6
#define OPERATION_CTRL_REG1 0x7
#define OPERATION_CTRL_REG2 0x8
#define DURATION_REG2    0x9
#define DURATION_REG3    0xa
#define FRAME_BODY1 0xb
#define OPERATION_CTRL_REG3 0xe
#define CONFIG_END 0xff
/*config remote register val*/
struct reg_s {
	int reg;
	unsigned int val;
};
enum {
	NORMAL = 0,
	TIMER = 1 ,
};

/*
   Decode_mode.(format selection)
   0x0 =NEC
   0x1= skip leader (just bits)
   0x2=measure width (software decode)
   0x3=MITSUBISHI
   0x4=Thomson
   0x5=Toshiba
   0x6=Sony SIRC
   0x7=RC5
   0x8=Reserved
   0x9=RC6
   0xA=RCMM
   0xB=Duokan
   0xC=Reserved
   0xD=Reserved
   0xE=Comcast
   0xF=Sanyo
 */
enum {
	DECODEMODE_NEC = 0,
	DECODEMODE_DUOKAN = 1 ,
	DECODEMODE_RCMM ,
	DECODEMODE_SONYSIRC,
	DECODEMODE_SKIPLEADER ,
	DECODEMODE_MITSUBISHI,
	DECODEMODE_THOMSON,
	DECODEMODE_TOSHIBA,
	DECODEMODE_RC5,
	DECODEMODE_RESERVED,
	DECODEMODE_RC6,
	DECODEMODE_RCA,
	DECODEMODE_COMCAST,
	DECODEMODE_SANYO,
	DECODEMODE_NEC_RCA_2IN1 = 14,
	DECODEMODE_NEC_TOSHIBA_2IN1 = 15,
	DECODEMODE_NEC_RCMM_2IN1,
	DECODEMODE_SW,
	DECODEMODE_MAX ,
	DECODEMODE_SW_NEC,
	DECODEMODE_SW_DUOKAN

};

/*remote config val*/
/****************************************************************/
static const struct reg_s RDECODEMODE_NEC[] = {
	{LDR_ACTIVE, ((unsigned)500 << 16) | ((unsigned)400 << 0)},
	/* NEC leader 9500us,max 477:
	(477* timebase = 20) = 9540 ;min 400 = 8000us*/
	{LDR_IDLE, 300 << 16 | 200 << 0}, /* leader idle*/
	{LDR_REPEAT, 150 << 16 | 80 << 0}, /* leader repeat*/
	{DURATION_REG0, 72 << 16 | 40 << 0 }, /* logic '0' or '00'*/
	{OPERATION_CTRL_REG0, 7 << 28 | (0xFA0 << 12) | 0x13},
	/* sys clock boby time.base time = 20 body frame 108ms*/
	{DURATION_REG1_AND_STATUS, (134 << 20) | (90 << 10)},
	/* logic '1' or '01'*/
	{OPERATION_CTRL_REG1, 0x9f00}, /* boby long decode (8-13)*/
	/*{OPERATION_CTRL_REG1,0xbe40},// boby long decode (8-13)*/
	{OPERATION_CTRL_REG2, 0x0}, /* hard decode mode*/
	{DURATION_REG2, 0},
	{DURATION_REG3, 0},
	{CONFIG_END,            0      }
};
/****************************************************************/
static const struct reg_s RDECODEMODE_DUOKAN[] = {
	{LDR_ACTIVE, ((70 << 16) | (30 << 0))},
	{LDR_IDLE, ((50 << 16) | (15 << 0))},
	{LDR_REPEAT, ((30 << 16) | (26 << 0))},
	{DURATION_REG0, ((66 << 16) | (40 << 0))},
	{OPERATION_CTRL_REG0, ((3 << 28) | (0x4e2 << 12) | (0x13))},
	/*logic '00' max=66*20us ,min=40*20us  */
	/*body frame 30ms*/
	{DURATION_REG1_AND_STATUS, ((80 << 20) | (66 << 10))},
	/*logic '01' max=80*20us,min=66*20us */
	{OPERATION_CTRL_REG1, 0x9300},
	{OPERATION_CTRL_REG2, 0xb90b},
	{DURATION_REG2, ((97 << 16) | (80 << 0))},
	/*logic '10' max=97*20us,min=80*20us */
	{DURATION_REG3, ((120 << 16) | (97 << 0))},
	/*logic '11' max=120*20us,min=97*20us */
	{OPERATION_CTRL_REG3, 5000<<0},
	{CONFIG_END,            0      }
};
/****************************************************************/
static const struct reg_s RDECODEMODE_RCMM[] = {
	{LDR_ACTIVE, 25 << 16 | 22 << 0},
	{LDR_IDLE, 14 << 16 | 13 << 0},
	{LDR_REPEAT, 14 << 16 | 13 << 0},
	{DURATION_REG0, 25 << 16 | 21 << 0 },
	{OPERATION_CTRL_REG0, 3 << 28 | (0x708 << 12) | 0x13},
	/* body frame 28 or 36 ms*/
	{DURATION_REG1_AND_STATUS, 33 << 20 | 29 << 10},
	{OPERATION_CTRL_REG1, 0xbe40},
	{OPERATION_CTRL_REG2, 0xa},
	{DURATION_REG2, 39 << 16 | 36 << 0},
	{DURATION_REG3, 50 << 16 | 46 << 0},
	{CONFIG_END,            0      }
};
/****************************************************************/
static const struct reg_s RDECODEMODE_SONYSIRC[] = {
	{LDR_ACTIVE, 130 << 16 | 110 << 0},
	{LDR_IDLE, 33 << 16 | 27 << 0},
	{LDR_REPEAT, 33 << 16 | 27 << 0},
	{DURATION_REG0, 63 << 16 | 56 << 0 },
	{OPERATION_CTRL_REG0, 3 << 28 | (0x8ca << 12) | 0x13},
	/* body frame 45ms*/
	{DURATION_REG1_AND_STATUS, 94 << 20 | 82 << 10},
	{OPERATION_CTRL_REG1, 0xbe40},
	{OPERATION_CTRL_REG2, 0x6},
	{DURATION_REG2, 0},
	{DURATION_REG3, 0},
	{CONFIG_END,            0      }
};

/**************************************************************/

static const struct reg_s RDECODEMODE_MITSUBISHI[] = {
	{LDR_ACTIVE, 410 << 16 | 390 << 0},
	{LDR_IDLE, 225 << 16 | 200 << 0},
	{LDR_REPEAT, 225 << 16 | 200 << 0},
	{DURATION_REG0, 60 << 16 | 48 << 0 },
	{OPERATION_CTRL_REG0, 3 << 28 | (0xBB8 << 12) | 0x13},
	/*An IR command is repeated 60ms for as long as the key
	on the remote is held down. body frame 60ms*/
	{DURATION_REG1_AND_STATUS, 110 << 20 | 95 << 10},
	{OPERATION_CTRL_REG1, 0xbe40},
	{OPERATION_CTRL_REG2, 0x3},
	{DURATION_REG2, 0},
	{DURATION_REG3, 0},
	{CONFIG_END,            0      }

};
/**********************************************************/
static const struct reg_s RDECODEMODE_TOSHIBA[] = {
	{LDR_ACTIVE, 477 << 16 | 389 << 0}, /*TOSHIBA leader 9000us*/
	{LDR_IDLE, 477 << 16 | 389 << 0}, /* leader idle*/
	{LDR_REPEAT, 460 << 16 | 389 << 0}, /* leader repeat*/
	{DURATION_REG0, 60 << 16 | 40 << 0 }, /* logic '0' or '00'*/
	{OPERATION_CTRL_REG0, 3 << 28 | (0xFA0 << 12) | 0x13},
	{DURATION_REG1_AND_STATUS, 111 << 20 | 100 << 10},
	/* logic '1' or '01'*/
	{OPERATION_CTRL_REG1, 0xbe40}, /* boby long decode (8-13)*/
	{OPERATION_CTRL_REG2, 0x5}, /* hard decode mode*/
	{DURATION_REG2, 0},
	{DURATION_REG3, 0},
	{CONFIG_END,            0      }

};
/*****************************************************************/
static const struct reg_s RDECODEMODE_THOMSON[] = {
	{LDR_ACTIVE, 477 << 16 | 390 << 0}, /* THOMSON leader 8000us,*/
	{LDR_IDLE, 477 << 16 | 390 << 0}, /* leader idle*/
	{LDR_REPEAT, 460 << 16 | 390 << 0}, /* leader repeat*/
	{DURATION_REG0, 80 << 16 | 60 << 0 }, /* logic '0' or '00'*/
	{OPERATION_CTRL_REG0, 3 << 28 | (0xFA0 << 12) | 0x13},
	{DURATION_REG1_AND_STATUS, 140 << 20 | 120 << 10},
	/* logic '1' or '01'*/
	{OPERATION_CTRL_REG1, 0xbe40}, /* boby long decode (8-13)*/
	{OPERATION_CTRL_REG2, 0x4}, /* hard decode mode*/
	{DURATION_REG2, 0},
	{DURATION_REG3, 0},
	{CONFIG_END,            0      }
};
/*********************************************************************/
static const struct reg_s RDECODEMODE_COMCAST[] = {
	{LDR_ACTIVE, 0   },
	{LDR_IDLE, 0  },
	{LDR_REPEAT, 0   },
	{DURATION_REG0, 0},
	{OPERATION_CTRL_REG0, 0},
	{DURATION_REG1_AND_STATUS,},
	{OPERATION_CTRL_REG1,},
	{OPERATION_CTRL_REG2,},
	{DURATION_REG2,},
	{DURATION_REG3,},
	{CONFIG_END,            0      }
};
static const struct reg_s RDECODEMODE_SKIPLEADER[] = {
	{LDR_ACTIVE,    },
	{LDR_IDLE,     },
	{LDR_REPEAT,    },
	{DURATION_REG0, },
	{OPERATION_CTRL_REG0,},
	{DURATION_REG1_AND_STATUS,},
	{OPERATION_CTRL_REG1,},
	{OPERATION_CTRL_REG2,},
	{DURATION_REG2,},
	{DURATION_REG3,},
	{CONFIG_END,            0      }
};
static const struct reg_s RDECODEMODE_SW[] = {
	{LDR_ACTIVE,    },
	{LDR_IDLE,     },
	{LDR_REPEAT,    },
	{DURATION_REG0, },
	{OPERATION_CTRL_REG0,},
	{DURATION_REG1_AND_STATUS,},
	{OPERATION_CTRL_REG1,},
	{OPERATION_CTRL_REG2,},
	{DURATION_REG2,},
	{DURATION_REG3,},
	{CONFIG_END,            0      }
};
static const struct reg_s RDECODEMODE_SW_NEC[] = {
	{LDR_ACTIVE, ((unsigned)477 << 16) | ((unsigned)400 << 0)},
	/* NEC leader 9500us,max 477:
	(477* timebase = 20) = 9540 ;min 400 = 8000us*/
	{LDR_IDLE, 248 << 16 | 202 << 0},
	/* leader idle*/
	{LDR_REPEAT, 130 << 16 | 110 << 0},
	/* leader repeat*/
	{DURATION_REG0, 60 << 16 | 48 << 0 },
	/* logic '0' or '00'*/
	{OPERATION_CTRL_REG0, 3 << 28 | (0xFA0 << 12) | 0x13},
	/* sys clock boby time.base time = 20 body frame 108ms*/
	{DURATION_REG1_AND_STATUS, (111 << 20) | (100 << 10)},
	/* logic '1' or '01'*/
	{OPERATION_CTRL_REG1, 0x8578}, /* boby long decode (8-13)*/
	{OPERATION_CTRL_REG2, 0x2}, /**/
	{DURATION_REG2, 0},
	{DURATION_REG3, 0},
	{CONFIG_END,            0      }
};
static const struct reg_s RDECODEMODE_SW_DUOKAN[] = {
	{LDR_ACTIVE, 52 << 16 | 49 << 0},
	{LDR_IDLE, 30 << 16 | 26 << 0},
	{LDR_REPEAT, 30 << 16 | 26 << 0},
	{DURATION_REG0, 60 << 16 | 56 << 0 },
	{OPERATION_CTRL_REG0, 3 << 28 | (0x5DC << 12) | 0x13},
	/*body frame 30ms*/
	{DURATION_REG1_AND_STATUS, (75 << 20) | 70 << 10},
	/* logic '1' or '01'*/
	{OPERATION_CTRL_REG1, 0x8578}, /* boby long decode (8-13)*/
	{OPERATION_CTRL_REG2, 0x2}, /* sw_duokan*/
	{DURATION_REG2, 0},
	{DURATION_REG3, 0},
	{CONFIG_END,            0      }
};
static const struct reg_s RDECODEMODE_NEC_RCA_2IN1[] = {
	/* used old decode*/
	{LDR_ACTIVE - 0x40, ((unsigned)477 << 16) | ((unsigned)400 << 0)},
	/* NEC leader 9500us,max 477:
	(477* timebase = 20) = 9540 ;min 400 = 8000us*/
	{LDR_IDLE - 0x40, 248 << 16 | 202 << 0}, /* leader idle*/
	{LDR_REPEAT - 0x40, 130 << 16 | 110 << 0}, /* leader repeat*/
	{DURATION_REG0 - 0x40, 60 << 16 | 48 << 0 }, /* logic '0' or '00'*/
	{OPERATION_CTRL_REG0 - 0x40, 3 << 28 | (0xFA0 << 12) | 0x13},
	/* sys clock boby time.base time = 20 body frame 108ms*/
	{DURATION_REG1_AND_STATUS - 0x40, (111 << 20) | (100 << 10)},
	/* logic '1' or '01'*/
	{OPERATION_CTRL_REG1 - 0x40, 0xbe40}, /* boby long decode (9-13)*/
	/* used new decode*/
	{LDR_ACTIVE, ((unsigned)250 << 16) | ((unsigned)160 << 0)},
	/*rca leader 4000us,200* timebase*/
	{LDR_IDLE, 250 << 16 | 160 << 0}, /* leader idle 400*/
	{LDR_REPEAT, 250 << 16 | 160 << 0}, /* leader repeat*/
	{DURATION_REG0, 100 << 16 | 48 << 0 }, /* logic '0' or '00' 1500us*/
	{OPERATION_CTRL_REG0, 3 << 28 | (0xFA0 << 12) | 0x13},
	/* sys clock boby time.base time = 20 body frame*/
	{DURATION_REG1_AND_STATUS, (150 << 20) | (110 << 10)},
	/* logic '1' or '01'     2500us*/
	{OPERATION_CTRL_REG1, 0x9740},
	/* boby long decode (8-13) //framn len = 24bit*/
	/*it may get the wrong customer value and key value from register if
	the value is set to 0x4,so the register value must set to 0x104*/
	{OPERATION_CTRL_REG2, 0x104},
	{DURATION_REG2, 0},
	{DURATION_REG3, 0},
	{CONFIG_END,            0      }
};
static const struct reg_s RDECODEMODE_NEC_TOSHIBA_2IN1[] = {
	/* used old decode*/
	{LDR_ACTIVE - 0x40, ((unsigned)500 << 16) | ((unsigned)400 << 0)},
	/* NEC leader 9500us,max 477:
	(477* timebase = 20) = 9540 ;min 400 = 8000us*/
	{LDR_IDLE - 0x40, 300 << 16 | 200 << 0},
	/* leader idle*/
	{LDR_REPEAT - 0x40, 150 << 16 | 80 << 0},
	/* leader repeat*/
	{DURATION_REG0 - 0x40, 72 << 16 | 40 << 0 },
	/* logic '0' or '00'*/
	{OPERATION_CTRL_REG0 - 0x40, 3 << 28 | (0xFA0 << 12) | 0x13},
	/* sys clock boby time.base time = 20 body frame 108ms*/
	{DURATION_REG1_AND_STATUS - 0x40, (134 << 20) | (90 << 10)},
	/* logic '1' or '01'*/
	{OPERATION_CTRL_REG1 - 0x40, 0xbe10},
	/* boby long decode (9-13)*/
	/* used new decode*/
	{LDR_ACTIVE, ((unsigned)300 << 16) | ((unsigned)160 << 0)},
	/*toshiba leader 4500us,20* timebase*/
	{LDR_IDLE, 300 << 16 | 160 << 0},
	/* leader idle 4500*/
	{LDR_REPEAT, 300 << 16 | 160 << 0},
	/* leader repeat*/
	{DURATION_REG0, 90 << 16 | 48 << 0 },
	/* logic '0' or '00' 1200us*/
	{OPERATION_CTRL_REG0, 3 << 28 | (0xFA0 << 12) | 0x13},
	/* sys clock boby time.base time = 20 body frame*/
	{DURATION_REG1_AND_STATUS, (150 << 20) | (100 << 10)},
	/* logic '1' or '01'     2400us*/
	{OPERATION_CTRL_REG1, 0x9f40},
	/* boby long decode (8-13) framn len = 24bit*/
	{OPERATION_CTRL_REG2, 0x5},
	{DURATION_REG2, 0},
	{DURATION_REG3, 0},
	{CONFIG_END,            0      }
};
static const struct reg_s RDECODEMODE_NEC_RCMM_2IN1[] = {
	/*used old decode*/
	{LDR_ACTIVE-0x40, ((unsigned)477<<16) | ((unsigned)400<<0)},/*leader*/
	{LDR_IDLE-0x40, 248<<16 | 202<<0},/*leader idle*/
	{LDR_REPEAT-0x40, 130<<16|110<<0}, /* leader repeat*/
	{DURATION_REG0-0x40, 60<<16|48<<0 },/*logic '0' or '00'*/
	{OPERATION_CTRL_REG0-0x40, 3<<28|(0xFA0<<12)|0x13}, /*base time = 20*/
	{DURATION_REG1_AND_STATUS-0x40, (111<<20)|(100<<10)},/*logic '1'or'01'*/
	{OPERATION_CTRL_REG1-0x40, 0xbe00},/*boby long decode (9-13)*/
	/*used new decode*/
	{LDR_ACTIVE, ((unsigned)35<<16) | ((unsigned)17<<0)},/*leader active*/
	{LDR_IDLE, 17<<16 | 8<<0},/*leader idle*/
	{LDR_REPEAT, 31<<16 | 11<<0},  /*leader repeat*/
	{DURATION_REG0, 25<<16|21<<0 },/*logic '0' or '00' 1200us*/
	{OPERATION_CTRL_REG0, 3<<28|(590<<12)|0x13},  /*time.base time = 20*/
	{DURATION_REG1_AND_STATUS, (33<<20)|(29<<10)},/*logic '1' or '01'*/
	{OPERATION_CTRL_REG1, 0x9f00},/*boby long decode (8-13)*/
	{OPERATION_CTRL_REG2, 0x1150a},
	{DURATION_REG2, 41<<16 | 36<<0},
	{DURATION_REG3, 50<<16 | 44<<0},
	{OPERATION_CTRL_REG3, 1200<<0},
	{CONFIG_END,            0      }
};

static const struct reg_s RDECODEMODE_RC5[] = {
	{LDR_ACTIVE,    },
	{LDR_IDLE,     },
	{LDR_REPEAT,    },
	{DURATION_REG0, },
	{OPERATION_CTRL_REG0,},
	{DURATION_REG1_AND_STATUS,},
	{OPERATION_CTRL_REG1,},
	{OPERATION_CTRL_REG2,},
	{DURATION_REG2,},
	{DURATION_REG3,},
	{CONFIG_END,            0      }
};
static const struct reg_s RDECODEMODE_RESERVED[] = {
	{LDR_ACTIVE,    },
	{LDR_IDLE,     },
	{LDR_REPEAT,    },
	{DURATION_REG0, },
	{OPERATION_CTRL_REG0,},
	{DURATION_REG1_AND_STATUS,},
	{OPERATION_CTRL_REG1,},
	{OPERATION_CTRL_REG2,},
	{DURATION_REG2,},
	{DURATION_REG3,},
	{CONFIG_END,            0      }
};
static const struct reg_s RDECODEMODE_RC6[] = {
	{LDR_ACTIVE, ((unsigned)210 << 16) | ((unsigned)125 << 0)},
	/*rca leader 4000us,200* timebase*/
	{LDR_IDLE, 50 << 16 | 38 << 0}, /* leader idle 400*/
	{LDR_REPEAT, 145 << 16 | 125 << 0}, /* leader repeat*/
	{DURATION_REG0, 51 << 16 | 38 << 0 }, /* logic '0' or '00' 1500us*/
	{OPERATION_CTRL_REG0, (3 << 28)|(0xFA0 << 12)|0x13},
	/* sys clock boby time.base time = 20 body frame*/
	{DURATION_REG1_AND_STATUS, (94 << 20) | (82 << 10)},
	/* logic '1' or '01'     2500us*/
	{OPERATION_CTRL_REG1, 0xa440},/*20bit 9440  36bit a340 32bit 9f40*/
	/* boby long decode (8-13) //framn len = 24bit*/
	/*it may get the wrong customer value and key value from register if
	the value is set to 0x4,so the register value must set to 0x104*/
	{OPERATION_CTRL_REG2, 0x109},
	{DURATION_REG2, ((28 << 16) | (16 << 0))},
	{DURATION_REG3, ((51 << 16) | (38 << 0))},
	{CONFIG_END,            0      }
};



static const struct reg_s RDECODEMODE_SANYO[] = {
	{LDR_ACTIVE,    },
	{LDR_IDLE,     },
	{LDR_REPEAT,    },
	{DURATION_REG0, },
	{OPERATION_CTRL_REG0,},
	{DURATION_REG1_AND_STATUS,},
	{OPERATION_CTRL_REG1,},
	{OPERATION_CTRL_REG2,},
	{DURATION_REG2,},
	{DURATION_REG3,},
	{CONFIG_END,            0      }
};



extern char *remote_log_buf;
extern unsigned long g_remote_ao_offset;

#ifdef CONFIG_AML_HDMI_TX
extern int cec_power_flag;
extern int rc_long_press_pwr_key;
#endif

#define am_remote_write_reg(x, val) (aml_write_aobus \
	((g_remote_ao_offset+((x)<<2)), val))

#define am_remote_read_reg(x) (aml_read_aobus \
	(g_remote_ao_offset+((x)<<2)))

#define am_remote_set_mask(x, val) (aml_write_aobus \
	((g_remote_ao_offset+((x)<<2)), (am_remote_read_reg(x)|(val))))

#define am_remote_clear_mask(x, val) (aml_write_aobus \
	((g_remote_ao_offset+((x)<<2)), (am_remote_read_reg(x)&(~(val)))))
void setremotereg(const struct reg_s *r);


/*remote config  ioctl  cmd*/
#define REMOTE_IOC_INFCODE_CONFIG       _IOW('I', 13, u32)
#define REMOTE_IOC_RESET_KEY_MAPPING        _IOW('I', 3, u32)
#define REMOTE_IOC_SET_KEY_MAPPING          _IOW('I', 4, u32)
#define REMOTE_IOC_SET_REPEAT_KEY_MAPPING   _IOW('I', 20, u32)
#define REMOTE_IOC_SET_MOUSE_MAPPING        _IOW('I', 5, u32)
#define REMOTE_IOC_SET_REPEAT_DELAY         _IOW('I', 6, u32)
#define REMOTE_IOC_SET_REPEAT_PERIOD        _IOW('I', 7, u32)

#define REMOTE_IOC_SET_REPEAT_ENABLE        _IOW('I', 8, u32)
#define REMOTE_IOC_SET_DEBUG_ENABLE         _IOW('I', 9, u32)
#define REMOTE_IOC_SET_MODE                 _IOW('I', 10, u32)

#define REMOTE_IOC_SET_CUSTOMCODE       _IOW('I', 100, u32)
#define REMOTE_IOC_SET_RELEASE_DELAY        _IOW('I', 99, u32)

/*reg*/
#define REMOTE_IOC_SET_REG_BASE_GEN         _IOW('I', 101, u32)
#define REMOTE_IOC_SET_REG_CONTROL          _IOW('I', 102, u32)
#define REMOTE_IOC_SET_REG_LEADER_ACT       _IOW('I', 103, u32)
#define REMOTE_IOC_SET_REG_LEADER_IDLE      _IOW('I', 104, u32)
#define REMOTE_IOC_SET_REG_REPEAT_LEADER    _IOW('I', 105, u32)
#define REMOTE_IOC_SET_REG_BIT0_TIME         _IOW('I', 106, u32)

/*sw*/
#define REMOTE_IOC_SET_BIT_COUNT            _IOW('I', 107, u32)
#define REMOTE_IOC_SET_TW_LEADER_ACT        _IOW('I', 108, u32)
#define REMOTE_IOC_SET_TW_BIT0_TIME         _IOW('I', 109, u32)
#define REMOTE_IOC_SET_TW_BIT1_TIME         _IOW('I', 110, u32)
#define REMOTE_IOC_SET_TW_REPEATE_LEADER    _IOW('I', 111, u32)

#define REMOTE_IOC_GET_TW_LEADER_ACT        _IOR('I', 112, u32)
#define REMOTE_IOC_GET_TW_BIT0_TIME         _IOR('I', 113, u32)
#define REMOTE_IOC_GET_TW_BIT1_TIME         _IOR('I', 114, u32)
#define REMOTE_IOC_GET_TW_REPEATE_LEADER    _IOR('I', 115, u32)

#define REMOTE_IOC_GET_REG_BASE_GEN         _IOR('I', 121, u32)
#define REMOTE_IOC_GET_REG_CONTROL          _IOR('I', 122, u32)
#define REMOTE_IOC_GET_REG_LEADER_ACT       _IOR('I', 123, u32)
#define REMOTE_IOC_GET_REG_LEADER_IDLE      _IOR('I', 124, u32)
#define REMOTE_IOC_GET_REG_REPEAT_LEADER    _IOR('I', 125, u32)
#define REMOTE_IOC_GET_REG_BIT0_TIME        _IOR('I', 126, u32)
#define REMOTE_IOC_GET_REG_FRAME_DATA       _IOR('I', 127, u32)
#define REMOTE_IOC_GET_REG_FRAME_STATUS     _IOR('I', 128, u32)

#define REMOTE_IOC_SET_TW_BIT2_TIME         _IOW('I', 129, u32)
#define REMOTE_IOC_SET_TW_BIT3_TIME         _IOW('I', 130, u32)

#define   REMOTE_IOC_SET_FN_KEY_SCANCODE     _IOW('I', 131, u32)
#define   REMOTE_IOC_SET_LEFT_KEY_SCANCODE   _IOW('I', 132, u32)
#define   REMOTE_IOC_SET_RIGHT_KEY_SCANCODE  _IOW('I', 133, u32)
#define   REMOTE_IOC_SET_UP_KEY_SCANCODE     _IOW('I', 134, u32)
#define   REMOTE_IOC_SET_DOWN_KEY_SCANCODE   _IOW('I', 135, u32)
#define   REMOTE_IOC_SET_OK_KEY_SCANCODE     _IOW('I', 136, u32)
#define   REMOTE_IOC_SET_PAGEUP_KEY_SCANCODE _IOW('I', 137, u32)
#define REMOTE_IOC_SET_PAGEDOWN_KEY_SCANCODE _IOW('I', 138, u32)
#define   REMOTE_IOC_SET_RELT_DELAY     _IOW('I', 140, u32)

#define   REMOTE_IOC_GET_POWERKEY			 _IOR('I', 141, u32)

#define REMOTE_HW_DECODER_STATUS_MASK       (0xf<<4)
#define REMOTE_HW_DECODER_STATUS_OK         (0<<4)
#define REMOTE_HW_DECODER_STATUS_TIMEOUT    (1<<4)
#define REMOTE_HW_DECODER_STATUS_LEADERERR  (2<<4)
#define REMOTE_HW_DECODER_STATUS_REPEATERR  (3<<4)

/* software  decode status*/
#define REMOTE_STATUS_WAIT       0
#define REMOTE_STATUS_LEADER     1
#define REMOTE_STATUS_DATA       2
#define REMOTE_STATUS_SYNC       3

#define REPEARTFLAG 0x1
/*status register repeat set flag*/
#define KEYDOMIAN 1
/* find key val vail data domain*/
#define CUSTOMDOMAIN 0
/* find key val vail custom domain*/
/*phy page user debug*/
#define REMOTE_LOG_BUF_LEN       4098
#define REMOTE_LOG_BUF_ORDER        1


enum rc_key_state {
	RC_KEY_STATE_UP = 0,
	RC_KEY_STATE_DN = 1,
};

typedef int (*type_printk)(const char *fmt, ...);
/* this is a message of IR input device,include release timer repeat timer*/
/*
 */
struct remote {
	struct input_dev *input;
	struct timer_list timer;
	/*release timer*/
	struct timer_list repeat_timer;
	/*repeat timer*/
	struct timer_list rel_timer;
	/*repeat timer*/
	unsigned long repeat_tick;
	int irq;
	int save_mode;
	int work_mode;
	/* use ioctl config decode mode*/
	int temp_work_mode;
	/* use ioctl config decode mode*/
	int frame_mode;
	/* same protocol frame have diffrent mode*/
	unsigned int register_data;
	unsigned int frame_status;
	unsigned int cur_keycode;
	unsigned int cur_lsbkeycode;
	/* rcv low 32bit save*/
	unsigned int cur_msbkeycode;
	/* rcv high 10bit save*/
	unsigned int repeat_release_code;
	/* save*/
	unsigned int last_keycode;
	unsigned int repeate_flag;
	unsigned int repeat_enable;
	unsigned int debounce;
	unsigned int status;
	/* we can only support 20 maptable*/
	int map_num;
	int ig_custom_enable;
	int enable_repeat_falg;
	unsigned int custom_code[20];
	/*use duble protocol release time*/
	/*frist protocol*/
	unsigned int release_fdelay;
	/* second protocol*/
	unsigned int release_sdelay;
	unsigned int release_delay[20];
	/*debug swtich*/
	unsigned int debug_enable;
	/*sw*/
	unsigned int sleep;
	unsigned int delay;
	unsigned int step;
	unsigned int send_data;

	/* 0:up(default) 1:down */
	unsigned int keystate;
	/* store irq time */
	unsigned long jiffies_irq;
	unsigned long jiffies_old;
	unsigned long jiffies_new;

#ifdef REMOTE_FIQ
	bridge_item_t fiq_handle_item;
#endif
	int want_repeat_enable;
	unsigned int key_repeat_map[20][256];
	unsigned int bit_count;
	unsigned int bit_num;
	unsigned int last_jiffies;
	unsigned int time_window[18];
	int last_pulse_width;
	int repeat_time_count;
	/*config*/
	int config_major;
	char config_name[20];
	struct class *config_class;
	struct device *config_dev;
	unsigned int repeat_delay[20];
	unsigned int relt_delay[20];
	unsigned int repeat_peroid[20];
	int (*remote_reprot_press_key)(struct remote *);
	int (*key_report)(struct remote *);
	void (*key_release_report)(struct remote *);
	void (*remote_send_key)(struct input_dev *,
		unsigned int, unsigned int, int);
};

extern type_printk input_dbg;

int set_remote_mode(int mode);
void set_remote_init(struct remote *remote_data);
void kdb_send_key(struct input_dev *dev, unsigned int scancode,
		  unsigned int type, int event);
void remote_send_key(struct input_dev *dev, unsigned int scancode,
		     unsigned int type, int event);
extern irqreturn_t remote_bridge_isr(int irq, void *dev_id);
extern irqreturn_t remote_null_bridge_isr(int irq, void *dev_id);
extern int remote_hw_report_null_key(struct remote *remote_data);
extern int remote_hw_report_key(struct remote *remote_data);
extern int remote_duokan_report_key(struct remote *remote_data);
extern int remote_rc6_report_key(struct remote *remote_data);
extern int remote_hw_nec_rca_2in1_report_key(struct remote *remote_data);
extern int remote_hw_nec_toshiba_2in1_report_key(struct remote *remote_data);
extern int remote_hw_nec_rcmm_2in1_report_key(struct remote *remote_data);
extern int remote_sw_report_key(struct remote *remote_data);
extern void remote_nec_report_release_key(struct remote *remote_data);
extern void remote_nec_rca_2in1_report_release_key(struct remote *remote_data);
extern void remote_nec_toshiba_2in1_report_release_key(struct remote
		*remote_data);
extern void remote_nec_rcmm_2in1_report_release_key(struct remote *remote_data);
extern void remote_duokan_report_release_key(struct remote *remote_data);
extern void remote_rc6_report_release_key(struct remote *remote_data);
extern void remote_sw_report_release_key(struct remote *remote_data);
extern void remote_null_report_release_key(struct remote *remote_data);
#ifdef REMOTE_FIQ
extern int register_fiq_bridge_handle(bridge_item_t *c_item);
extern int unregister_fiq_bridge_handle(bridge_item_t *c_item);
extern int fiq_bridge_pulse_trigger(bridge_item_t *c_item);
#endif
extern int remote_save_regs(int mode);
extern int remote_restore_regs(int mode);


#endif
