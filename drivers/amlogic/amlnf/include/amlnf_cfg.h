

/**************PHY****************/
#define	AML_SLC_NAND_SUPPORT
#define	AML_MLC_NAND_SUPPORT
/*
#define	AML_NAND_DBG
*/
#define	NEW_NAND_SUPPORT
#define AML_NAND_NEW_OOB

#define NAND_ADJUST_PART_TABLE

#ifdef NAND_ADJUST_PART_TABLE
#define	ADJUST_BLOCK_NUM	4
#else
#define	ADJUST_BLOCK_NUM	0
#endif

#define AML_NAND_RB_IRQ
/*
#define AML_NAND_DMA_POLLING
*/

extern  int is_phydev_off_adjust(void);
extern  int get_adjust_block_num(void);

