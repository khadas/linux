#ifndef AM_IRBLASTER_H
#define AM_IRBLASTER_H
#define MAX_PLUSE 1024
struct aml_blaster {
	unsigned int consumerir_freqs;
	unsigned int pattern[MAX_PLUSE];
	unsigned int pattern_len;
	unsigned int fisrt_pulse_width;
};
enum  {	/* Modulation level*/
	fisrt_low  = 0,
	fisrt_high = 1
};
#define AO_IR_BLASTER_ADDR0 ((0x00 << 10) | (0x30 << 2))
#define AO_IR_BLASTER_ADDR1 ((0x00 << 10) | (0x31 << 2))
#define AO_IR_BLASTER_ADDR2 ((0x00 << 10) | (0x32 << 2))
#define AO_RTI_PIN_MUX_REG (0x14)

#define CONSUMERIR_TRANSMIT     0x5500
#define GET_CARRIER         0x5501
#define SET_CARRIER         0x5502
#define SET_MODLEVEL         0x5503
#endif
