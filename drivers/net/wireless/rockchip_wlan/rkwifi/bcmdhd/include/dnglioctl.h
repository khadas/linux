/*
 * HND Run Time Environment ioctl.
 *
 * Copyright (C) 2025 Synaptics Incorporated. All rights reserved.
 *
 * This software is licensed to you under the terms of the
 * GNU General Public License version 2 (the "GPL") with Broadcom special exception.
 *
 * INFORMATION CONTAINED IN THIS DOCUMENT IS PROVIDED "AS-IS," AND SYNAPTICS
 * EXPRESSLY DISCLAIMS ALL EXPRESS AND IMPLIED WARRANTIES, INCLUDING ANY
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE,
 * AND ANY WARRANTIES OF NON-INFRINGEMENT OF ANY INTELLECTUAL PROPERTY RIGHTS.
 * IN NO EVENT SHALL SYNAPTICS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, PUNITIVE, OR CONSEQUENTIAL DAMAGES ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OF THE INFORMATION CONTAINED IN THIS DOCUMENT, HOWEVER CAUSED
 * AND BASED ON ANY THEORY OF LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, AND EVEN IF SYNAPTICS WAS ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE. IF A TRIBUNAL OF COMPETENT JURISDICTION
 * DOES NOT PERMIT THE DISCLAIMER OF DIRECT DAMAGES OR ANY OTHER DAMAGES,
 * SYNAPTICS' TOTAL CUMULATIVE LIABILITY TO ANY PARTY SHALL NOT
 * EXCEED ONE HUNDRED U.S. DOLLARS
 *
 * Copyright (C) 2025, Broadcom.
 *
 *      Unless you and Broadcom execute a separate written software license
 * agreement governing use of this software, this software is licensed to you
 * under the terms of the GNU General Public License version 2 (the "GPL"),
 * available at http://www.broadcom.com/licenses/GPLv2.php, with the
 * following added to such license:
 *
 *      As a special exception, the copyright holders of this software give you
 * permission to link this software with independent modules, and to copy and
 * distribute the resulting executable under terms of your choice, provided that
 * you also meet, for each linked independent module, the terms and conditions of
 * the license of that module.  An independent module is a module which is not
 * derived from this software.  The special exception does not apply to any
 * modifications of the software.
 *
 *
 * <<Broadcom-WL-IPTag/Dual:>>
 */

#ifndef _dngl_ioctl_h_
#define _dngl_ioctl_h_

/* ==== Dongle IOCTLs i.e. non-d11 IOCTLs ==== */

#ifndef _rte_ioctl_h_
/* ================================================================ */
/* These are the existing ioctls moved from src/include/rte_ioctl.h */
/* ================================================================ */

/* RTE IOCTL definitions for generic ether devices */
#define RTEIOCTLSTART		0x8901
#define RTEGHWADDR		0x8901
#define RTESHWADDR		0x8902
#define RTEGMTU			0x8903
#define RTEGSTATS		0x8904
#define RTEGALLMULTI		0x8905
#define RTESALLMULTI		0x8906
#define RTEGPROMISC		0x8907
#define RTESPROMISC		0x8908
#define RTESMULTILIST		0x8909
#define RTEGUP			0x890A
#define RTEGPERMADDR		0x890B
#define RTEDEVPWRSTCHG		0x890C	/* Device pwr state change for PCIedev */
#define RTEDEVPMETOGGLE		0x890D	/* Toggle PME# to wake up the host */
#define RTEDEVTIMESYNC		0x890E	/* Device TimeSync */
#define RTEDEVDSNOTIFY		0x890F	/* Bus DS state notification */
#define RTED11DMALPBK_INIT	0x8910	/* D11 DMA loopback init */
#define RTED11DMALPBK_UNINIT	0x8911	/* D11 DMA loopback uninit */
#define RTED11DMALPBK_RUN	0x8912	/* D11 DMA loopback run */
#define RTEDEVTSBUFPOST		0x8913	/* Async interface for tsync buffer post */
#define RTED11DMAHOSTLPBK_RUN	0x8914  /* D11 DMA host memory loopback run */
#define RTEDEVGETTSF		0x8915  /* Get device TSF */
#define RTEDURATIONUNIT		0x8916  /* Duration unit */
#define RTEWRITE_WAR_REGS	0x8917  /* write workaround regs */
#define RTEDEVRMPMK		0x8918  /* Remove PMK */
#define RTEDEVDBGVAL		0x8919  /* Set debug val */
#define RTEDEVVIFDEL		0x891A  /* Delete virtual cfgs */
#define RTEQUIESCEFORCE		0x891B	/* Force D11 core into quiesce */
#define RTEPTMENABLE		0x891C	/* Start/stop PTM time stamping in TX/RX status */
#define RTEMACSUSPEND		0x891D	/* Suspend MAC cores */
#define RTEPTMHOFFSET		0x891E	/* PTM offsets */
#define RTE_REAL_D3_D0		0x891F	/* Transition from real D3/D0. */
#define RTERXCMPLCHAINENABLE	0x8920	/* Chained RxCompletion Enable */
#define RTEGRADIO_STAT		0x8921	/* Radio Stat */
#define RTEDEVGETSTATSTYPE	0x8922	/* Get flowring stats type from WL */
#define RTEDEVGETSTATSBITMAP	0x8923	/* Get flowring stats bitmap from WL */
/* Ensure last RTE IOCTL define val is assigned to RTEIOCTLEND */
#define RTEIOCTLEND		0x8923  /* LAST RTE IOCTL value */

#define RTE_IOCTL_QUERY		0x00
#define RTE_IOCTL_SET		0x01
#define RTE_IOCTL_OVL_IDX_MASK	0x1e
#define RTE_IOCTL_OVL_RSV	0x20
#define RTE_IOCTL_OVL		0x40
#define RTE_IOCTL_OVL_IDX_SHIFT	1

enum hnd_ioctl_cmd {
	HND_RTE_DNGL_IS_SS = 1, /* true if device connected at super speed */

	/* PCIEDEV specific wl <--> bus ioctls */
	BUS_GET_VAR = 2,
	BUS_SET_VAR = 3,
	BUS_FLUSH_RXREORDER_Q = 4,
	BUS_SET_LTR_STATE = 5,
	BUS_FLUSH_CHAINED_PKTS = 6,
	BUS_SET_COPY_COUNT = 7,
	BUS_UPDATE_FLOW_PKTS_MAX = 8,
	BUS_UPDATE_EXTRA_TXLFRAGS = 9,
	BUS_UPDATE_FRWD_RESRV_BUFCNT = 10,
	BUS_PCIE_CONFIG_ACCESS = 11,
	BUS_HC_EVENT_MASK_UPDATE = 12,
	BUS_SET_MAC_WAKE_STATE = 13,
	BUS_FRWD_PKT_RXCMPLT = 14,
	BUS_PCIE_LATENCY_ENAB = 15, /* to enable latency feature in pcie */
	BUS_GET_MAXITEMS = 16,
	BUS_SET_BUS_CSO_CAP = 17,	/* Update the CSO cap from wl layer to bus layer */
	BUS_DUMP_RX_DMA_STALL_RELATED_INFO = 18,
	BUS_UPDATE_RESVPOOL_STATE = 19,	/* Update resvpool state */
	BUS_GET_MAX_RING_NUM = 20, /* Get the Max num of the Tx rings */
	BUS_M2M_LOW_PRIO_DESCR = 21, /* enable/disable m2m low prio descriptor */
	BUS_SET_DEV_TRAP_FATAL = 22, /* communicate to host to perform big-hammer */
	BUS_UPDATE_RX_PKTFETCH_CNT = 23, /* Num of Rx packets fetched from host */
	BUS_UPDATE_RX_PKTFETCH_FW_CNSMD = 24, /* Num of Rx packets fetched and consumed in FW */
	BUS_D2H_DMAQ_BUF_REINIT = 25
};

#define SDPCMDEV_SET_MAXTXPKTGLOM	1
#define RTE_MEMUSEINFO_VER 0x00

typedef struct memuse_info {
	uint16 ver;			/* version of this struct */
	uint16 len;			/* length in bytes of this structure */
	uint32 tot;			/* Total memory */
	uint32 text_len;	/* Size of Text segment memory */
	uint32 data_len;	/* Size of Data segment memory */
	uint32 bss_len;		/* Size of BSS segment memory */

	uint32 arena_size;	/* Total Heap size */
	uint32 arena_free;	/* Heap memory available or free */
	uint32 inuse_size;	/* Heap memory currently in use */
	uint32 inuse_hwm;	/* High watermark of memory - reclaimed memory */
	uint32 inuse_overhead;	/* tally of allocated mem_t blocks */
	uint32 inuse_total;	/* Heap in-use + Heap overhead memory  */
	uint32 free_lwm;        /* Least free size since reclaim */
	uint32 mf_count;        /* Malloc failure count */
} memuse_info_t;

#define RTE_POOLUSEINFO_VER 0x01
#define RTE_POOLUSEINFO_VER2 0x02

typedef struct pooluse_info {
	uint16 ver;				/* version of this struct */
	uint16 len;				/* length in bytes of this structure */

	uint32 shared_count;			/* pktpool_shared pkt count */
	uint32 shared_available;		/* pktpool_shared pkt left */
	uint32 shared_max_pkt_bytes;		/* pktpool_shared max pkt size */
	uint32 shared_overhead;			/* pktpool_shared with overhead */

	uint32 lfrag_count;			/* pktpool_shared_lfrag pkt count */
	uint32 lfrag_available;			/* pktpool_shared_lfrag pkt left */
	uint32 lfrag_max_pkt_bytes;		/* pktpool_shared_lfrag max pkt size */
	uint32 lfrag_overhead;			/* pktpool_shared_lfrag with overhead */

	uint32 resvlfrag_count;			/* pktpool_resv_lfrag pkt count */
	uint32 resvlfrag_available;		/* pktpool_resv_lfrag pkt left */
	uint32 resvlfrag_max_pkt_bytes;		/* pktpool_resv_lfrag max pkt size */
	uint32 resvlfrag_overhead;		/* pktpool_resv_lfrag with overhead */

	uint32 rxlfrag_count;			/* pktpool_shared_rxlfrag pkt count */
	uint32 rxlfrag_available;		/* pktpool_shared_rxlfrag pkt left */
	uint32 rxlfrag_max_pkt_bytes;		/* pktpool_shared_rxlfrag max pkt size */
	uint32 rxlfrag_overhead;		/* pktpool_shared_rxlfrag with overhead */

	uint32 rxdata_count;			/* pktpool_shared_rxdata pkt count */
	uint32 rxdata_available;		/* pktpool_shared_rxdata pkt left */
	uint32 rxdata_max_pkt_bytes;		/* pktpool_shared_rxdata max pkt size */
	uint32 rxdata_overhead;			/* pktpool_shared_rxdata with overhead */

	uint32 alfrag_count;			/* pktpool_shared_alfrag pkt count */
	uint32 alfrag_available;		/* pktpool_shared_alfrag pkt left */
	uint32 alfrag_max_pkt_bytes;		/* pktpool_shared_alfrag max pkt size */
	uint32 alfrag_overhead;			/* pktpool_shared_alfrag with overhead */

	uint32 alfragdata_count;		/* pktpool_shared_alfrag_data pkt count */
	uint32 alfragdata_available;		/* pktpool_shared_alfrag_data pkt left */
	uint32 alfragdata_max_pkt_bytes;	/* pktpool_shared_alfrag_data max pkt size */
	uint32 alfragdata_overhead;		/* pktpool_shared_alfrag_data with overhead */

	uint32 resvalfrag_count;		/* pktpool_resv_alfrag pkt count */
	uint32 resvalfrag_available;		/* pktpool_resv_alfrag pkt left */
	uint32 resvalfrag_max_pkt_bytes;	/* pktpool_resv_alfrag max pkt size */
	uint32 resvalfrag_overhead;		/* pktpool_resv_alfrag with overhead */

	uint32 resvalfragdata_count;		/* pktpool_resv_alfrag_data pkt count */
	uint32 resvalfragdata_available;	/* pktpool_resv_alfrag_data pkt left */
	uint32 resvalfragdata_max_pkt_bytes;	/* pktpool_resv_alfrag_data max pkt size */
	uint32 resvalfragdata_overhead;		/* pktpool_resv_alfrag_data with overhead */

	uint32 total_size;			/* pktpool total size in bytes */
	uint32 total_overhead;			/* pktpool total with overhead */

	uint32 urb_main_sz;			/* URB memory used for main core. */
	uint32 urb_aux_sz;			/* URB memory used for aux core. */
	uint32 inuse_sz_bm;			/* Total Bootmem size used. */

	uint32 alfragmdata_count;               /* pktpool_shared_alfrag_mdata pkt count */
	uint32 alfragmdata_available;           /* pktpool_shared_alfrag_mdata pkt left */
	uint32 alfragmdata_max_pkt_bytes;       /* pktpool_shared_alfrag_mdata max pkt size */
	uint32 alfragmdata_overhead;            /* pktpool_shared_alfrag_mdata with overhead */
} pooluse_info_t;

typedef struct memuse_ext_info {
	memuse_info_t hu;	/* heap usage */
	pooluse_info_t pu;	/* pktpool usage */
} memuse_ext_info_t;

/* Different DMA loopback modes */
#define M2M_DMA_LOOPBACK	0	/* PCIE M2M mode */
#define D11_DMA_LOOPBACK	1	/* PCIE M2M and D11 mode without ucode */
#define BMC_DMA_LOOPBACK	2	/* PCIE M2M and D11 mode with ucode */
#define M2M_NON_DMA_LOOPBACK	3	/* Non DMA(indirect) mode */
#define D11_DMA_HOST_MEM_LPBK	4	/* D11 mode */
#define M2M_DMA_WRITE_TO_RAM	6	/* PCIE M2M write to specific memory mode */
#define M2M_DMA_READ_FROM_RAM	7	/* PCIE M2M read from specific memory mode */
#define D11_DMA_WRITE_TO_RAM	8	/* D11 write to specific memory mode */
#define D11_DMA_READ_FROM_RAM	9	/* D11 read from specific memory mode */

/* For D11 DMA loopback test */
typedef struct d11_dmalpbk_init_args {
	uint8 core_num;
	uint8 lpbk_mode;
} d11_dmalpbk_init_args_t;

typedef struct d11_dmalpbk_args {
	uint8 *buf;
	int32 len;
	void *p;
	uint8 core_num;
	uint8 pad[3];
} d11_dmalpbk_args_t;

typedef enum wl_config_var {
	WL_VAR_TX_PKTFETCH_INDUCE = 1,
	WL_VAR_LAST
} wl_config_var_t;

typedef struct wl_config_buf {
	wl_config_var_t var;
	uint32 val;
} wl_config_buf_t;

/* ================================================================ */
/* These are the existing ioctls moved from src/include/rte_ioctl.h */
/* ================================================================ */
#endif /* _rte_ioctl_h_ */

/* MPU test iovar version */
#define MPU_TEST_STRUCT_VER	0

/* MPU test OP */
#define MPU_TEST_OP_READ	0
#define MPU_TEST_OP_WRITE	1
#define MPU_TEST_OP_EXECUTE	2

/* Debug iovar for MPU testing */
typedef struct mpu_test_args {
	/* version control */
	uint16 ver;
	uint16 len;	/* the length of this structure */
	/* data */
	uint32 addr;
	uint8 op;	/* see MPU_TEST_OP_XXXX */
	uint8 rsvd;
	uint16 size;	/* valid for read/write */
	uint8 val[];
} mpu_test_args_t;

/* MMU test iovar version returned by MMU_IOV_SUBCMD_VER */
#define MMU_IOV_TEST_VER_1	1u

/* Debug iovar for MMU testing */
typedef struct mmu_iov_test_args_v1 {
	uint32 addr;	/* address to test */
	uint32 value;	/* valid for read/write */
} mmu_iov_test_args_v1_t;

/* valid when getting a read response */
typedef struct mmu_iov_test_read_resp_v1 {
	uint16 size;	/* size of variable length data */
	uint8 data[];	/* variable length read data */
} mmu_iov_test_read_resp_v1_t;

/* subcommand ids */
enum mmu_iov_cmd_ids {
	MMU_IOV_SUBCMD_VER	= 0u,	/* MMU IOVAR version */
	MMU_IOV_SUBCMD_READ	= 1u,	/* MMU READ */
	MMU_IOV_SUBCMD_WRITE	= 2u,	/* MMU WRITE */
	MMU_IOV_SUBCMD_EXECUTE	= 3u,	/* MMU EXECUTE */
	MMU_IOV_SUBCMD_DUMP	= 4u	/* MMU DUMP */
};

/* dsec command uses bcm iov framework (bcm_iov_buf_t) for iovar input/output */
/* dsec command vesion */
#define DSEC_IOV_VERSION_V1		1u

enum desc_cmd_ids {
	DSEC_CMD_ALL			= 0u,
	DSEC_CMD_TRANSUNLOCK		= 1u,
	DSEC_CMD_SBOOT			= 2u,
	DSEC_CMD_RGNLOCK		= 3u,
	DSEC_CMD_SBOOT_DATA		= 4u
};

/* TRANSIENT unlock command */
#define TRANSIENT_UNLOCK_VER_V1		1u
#define TRANSIENT_UNLOCK_KEY_SIZE	4u	/* 4 Words of 32-bit each */
#define TRANSIENT_UNLOCK_SALT_SIZE	16u	/* 16 Words of 32-bit each */

enum desc_crypto_subcmd_ids {
	DSEC_TRUNLOCK_SUBCMD_NONE	= 0u,
	DSEC_TRUNLOCK_SUBCMD_VER	= 1u,
	DSEC_TRUNLOCK_SUBCMD_STATUS	= 2u,
	DSEC_TRUNLOCK_SUBCMD_UNLOCK	= 3u
};

typedef struct {
	uint16 version;				/* cmd structure version */
	uint16 length;				/* cmd struct len */
	uint16 subcmd_id;			/* transient_unlock sub command */
	uint16 status;				/* Unlock status */
	uint32 key[TRANSIENT_UNLOCK_KEY_SIZE];	/* otp read mode */
	uint32 salt[TRANSIENT_UNLOCK_SALT_SIZE]; /* byte offset into otp to start read */
} transient_unlock_cmd_v1_t;

/* OTP Region lock command */
#define DSEC_RGN_LOCK_VER_V1		1u

/* OTP Lock Regions */
#define DSEC_LOCK_RGN_NONE		0u
#define DSEC_LOCK_RGN_WAFER_SORT	1u
#define DSEC_LOCK_RGN_HASH_DATA		2u
#define DSEC_LOCK_RGN_FINAL_TEST	3u
#define DSEC_LOCK_RGN_AUTOLOAD		4u
#define DSEC_LOCK_RGN_UPPER_GU		5u
#define DSEC_LOCK_RGN_LOWER_GU		6u
#define DSEC_LOCK_RGN_HW_SW		7u
#define DSEC_LOCK_RGN_BT		8u
#define DSEC_LOCK_RGN_SECURE		9u
#define DSEC_LOCK_RGN_SECURE_IV		10u
#define DSEC_LOCK_RGN_SECURE_V		11u
#define DSEC_LOCK_RGN_SECURE_VI_0	12u
#define DSEC_LOCK_RGN_SECURE_VI_1	13u

/* SECURE OTP command */
#define DSEC_OTP_VER_V1			1u

enum dsec_sboot_xtlv_id {
	DSEC_OTP_XTLV_NONE			= 0u,	/* Not valid otp tag */
	DSEC_OTP_XTLV_VER			= 1u,	/* OTP region type */
	/* 2u is can be used */

	/* RNG Lock Tags: */
	DSEC_OTP_XTLV_RGN			= 3u,	/* OTP region type */
	DSEC_OTP_XTLV_DATA			= 4u,	/* OTP region lock data */

	/* SBOOT Tags: */
	DSEC_OTP_XTLV_SBOOT_FW_SIG_ENABLE	= 5u,	/* FW signing enable bit */
	DSEC_OTP_XTLV_SBOOT_FW_SIG_DISABLE	= 6u,	/* FW signing disaable bit */
	DSEC_OTP_XTLV_SBOOT_ROM_PROTECT_ENABLE	= 7u,	/* ROM protect enable bit */
	DSEC_OTP_XTLV_SBOOT_ROM_PROTECT_PATCH	= 8u,	/* ROM protect from patch */
	DSEC_OTP_XTLV_SBOOT_HOST_RD_NONSEC_EN	= 9u,	/* Host read non secure enable bit */
	DSEC_OTP_XTLV_SBOOT_HOST_RD_NONSEC_DIS	= 10u,	/* Host read non secure disable bit */
	DSEC_OTP_XTLV_SBOOT_HOST_WR_NONSEC_EN	= 11u,	/* Host write non secure enable bit */
	DSEC_OTP_XTLV_SBOOT_HOST_WR_NONSEC_DIS	= 12u,	/* Host write non secure disable bit */
	DSEC_OTP_XTLV_SBOOT_DBGREGS_PROT_ENAB	= 13u,	/* ARM DBG regs protect enable bit */
	DSEC_OTP_XTLV_SBOOT_DBGREGS_PROT_DIS	= 14u,	/* ARM DBG regs protect disable bit */
	DSEC_OTP_XTLV_SBOOT_JTAG_PROTECT_ENAB	= 15u,	/* JTAG protect disable bit */
	DSEC_OTP_XTLV_SBOOT_JTAG_PROTECT_DIS	= 16u,	/* JTAG protect re-enable bit */
	DSEC_OTP_XTLV_SBOOT_TCAM_PROTECT_SIZE	= 17u,	/* TCAM protect enable size field 8 bits */
	DSEC_OTP_XTLV_SBOOT_ACTIVATE_SECURITY	= 18u,	/* Active security enable bit */
	DSEC_OTP_XTLV_SBOOT_KEY_REVOC_BITS	= 19u,	/* Key revocation Bits field 16 bits */
	DSEC_OTP_XTLV_SBOOT_CUSTOMER_PUB_KEY_1	= 20u,	/* Customer public key 1 field 257 bits */
	DSEC_OTP_XTLV_SBOOT_CUSTOMER_PUB_KEY_2	= 21u,	/* Customer public key 2 field 257 bits */
	DSEC_OTP_XTLV_SBOOT_LOT_NUM		= 22u,	/* Chip lot num low bits [0:16] 17 bits */
	DSEC_OTP_XTLV_SBOOT_WAFER_NUM		= 23u,	/* Chip wafer num 5 bits */
	DSEC_OTP_XTLV_SBOOT_WAFER_X		= 24u,	/* Chip wafer X 9 bits */
	DSEC_OTP_XTLV_SBOOT_WAFER_Y		= 25u,	/* Chip wafer Y 9 bits */
	DSEC_OTP_XTLV_SBOOT_UNLOCK_HASH_VAL	= 26u,	/* Unlock Hash Val 128 bits */
	DSEC_OTP_XTLV_SBOOT_PRODUCTION_CHIP	= 27u,	/* Production chip bit */
	DSEC_OTP_XTLV_SBOOT_ENCRYPTION_KEY	= 28u,	/* AES wrapped fw encryption key 320 bits */
	DSEC_OTP_XTLV_SBOOT_LOT_NUM_MS		= 29u,	/* Chip lot num high bits [17:47] 31 bits */
	DSEC_OTP_XTLV_SBOOT_OTP_WR_LOCK_ENAB	= 30u,	/* OTP write lock enable bit */
};

/* SMBM (Shared Memory Bank Manager) IOVAR sub-command IDs for IOVAR "smbm" */
typedef enum {
	SMBM_SUBCMD_GETVER = 0,   /* Get SMBM IOVAR version */
	SMBM_SUBCMD_DUMP = 1u,   /* Dump SMBM diagnostic information */
	SMBM_SUBCMD_MODE = 2u,   /* SMBM mode - who can use SMB - WLAN/BT/'via arbitration' */
	SMBM_SUBCMD_POLICY = 3u, /* SMBM policy - The list of SW features that use SMB Memory. */
	SMBM_SUBCMD_STATUS = 4u, /* SMBM's current Status - if SMB Memory is with WLAN or BT */
	SMBM_SUBCMD_TEST = 5u	/* For testing only. This will have sub IDs */

} smbm_subcmd_id_t;

#define SMBM_IOVAR_CMD_VER_0    0  /* Version 0 of the SMBM IOVAR SMBM_SUBCMD_VER */
#define SMBM_IOVAR_CMD_VER      SMBM_IOVAR_CMD_VER_0

#define SMBM_DUMP_VER_0 0
#define SMBM_DUMP_VER	SMBM_DUMP_VER_0
/**
 * @brief Structure for the payload of the 'dump' subcommand response XTLV.
 */
typedef struct smbm_dump_info {
	uint16 ver;		/**< Version of this dump structure (SMBM_DUMP_VER_0) */
	uint16 len;		/**< Total length of this smbm_dump_info_t structure itself */
	uint32 total_mem;	/**< Total SMB memory size in bytes */
	uint32 smb_start_addr;	/**< Start address of SMB memory */
	uint32 smb_end_addr;	/**< End address of SMB memory */
	uint8 smb_status;	/**< returns smbm_status_t */
	uint8 smbm_usr_hndl_cnt; /**< MAX Number of SMBM users. */
	uint16 reserved;
	char   val[BCM_FLEX_ARRAY]; /**< For formatted text output */
} smbm_dump_resp_info_v0_t;

/* SMBM Mode */
typedef enum {
	SMBM_MODE_BT = 0,			/**<  SMB will not be used by WLAN. */
	SMBM_MODE_WLAN_BT_ARBITRATION = 1u,	/**< SMB will be used by WLAN when available.
						  * Via SMBM GCI protocol arbitration.
						  */
	SMBM_MODE_WLAN = 2u			/**< SMB will be used by WLAN only. */
} smbm_mode_value_t;

#define SMBM_MODE_VER_0 0
#define SMBM_MODE_VER	SMBM_MODE_VER_0

typedef struct smbm_mode {
	uint16 ver;		/**< Version of smbm_mode_t */
	uint16 len;		/**< Total length of this smbm_mode_t structure itself */
	uint8 mode;		/**< smbm_mode_value_t */
	uint8 reserved;
	uint16 reserved1;
} smbm_mode_t;

/* SMBM Policy bitmap. Each set bit from below enable that particular feature. */
typedef enum {
	SMBM_POLICY_NONE = 0,				/* No features use SMB Memory. */
	SMBM_POLICY_ANY_ASSOC = (1u << 0),		/* Use SMB for any association. */
	SMBM_POLICY_320MHZ_ASSOC_ONLY = (1u << 1u),	/* Use SMB only when 320Mhz assoc */
	SMBM_POLICY_LOGGING = (1u << 2u)		/* Use SMB for logging */
} smbm_policy_map_t;

/* SMBM policy operation to set. SET/ADD/REMOVE */
typedef enum {
	SMBM_POLICY_SET = 0,	/**< Setting new policy bitmap value. */
	SMBM_POLICY_ADD = 1u,	/**<  Adds to existing policy bitmap value. */
	SMBM_POLICY_REMOVE = 2u	/**<  Removes from existing policy bitmap value. */
} smbm_policy_op_t;

#define SMBM_POLICY_VER_0 0
#define SMBM_POLICY_VER	SMBM_POLICY_VER_0

/* Structure to set/get SMBM policy. */
typedef struct {
	uint16 ver;		/**< Version of smbm_policy_t */
	uint16 len;		/**< Total length of this smbm_policy_t structure itself */
	uint32 policy;		/**< Value of smbm_policy_map_t */
	uint8  policy_op;	/**< Policy operation smbm_policy_op_t. Valid only for set. */
	uint8  reserved;
	uint16 reserved1;
} smbm_policy_t;

/* SMBM Status */
typedef enum smbm_status_e {
	SMBM_STATUS_UNKNOWN = 0,	/**< Invalid value. */
	SMBM_STATUS_BT = 1u,		/**< SMB Memory is with BT */
	SMBM_STATUS_WLAN = 2u,		/**< SMB Memory is with WLAN */
} smbm_status_type_t;

#define SMBM_STATUS_VER_0 0
#define SMBM_STATUS_VER	SMBM_STATUS_VER_0

/* SMBM Status */
typedef struct smbm_status {
	uint16 ver;		/**< Version. */
	uint16 len;		/**< Total length of this structure. */
	uint8 status;		/* smbm_status_type_t */
	uint8 reserved;
	uint16 reserved1;
} smbm_status_t;

typedef smbm_dump_resp_info_v0_t smbm_dump_resp_info_t;

typedef enum {
	SMBM_TEST_SMBM_USER_REGISTER	= 0, /** Register new SMB memory user. Returns handle */
	SMBM_TEST_SMBM_MALLOC		= 1u,	/** SMB Malloc using handle from user_register */
	SMBM_TEST_SMBM_MFREE		= 2u,	/** free SMB Memory using address */
	SMBM_TEST_SWITCH_TO_WLAN	= 3u,	/** Switch SMB to WLAN */
	SMBM_TEST_SWITCH_TO_BT		= 4u,	/** Switch SMB to BT */
	SMBM_TEST_MEMDIAG		= 5u	/** Memory diag test */
} smbm_subcmd_test_id_t;

#define SMBM_TEST_VER_0 0
#define SMBM_TEST_VER	SMBM_TEST_VER_0

typedef struct smbm_test_params {
	uint16 ver;		/**< Version of this test structure */
	uint16 len;		/**< Total length structure itself */
	uint16 test_id;		/**< smbm_subcmd_test_id_t */
	uint16 reserved;
	union {
		struct {
			uint32 smbm_user_handle;
			uint32 malloc_size;
		} smbm_malloc;

		struct {
			uint32 allocated_address;
		} smbm_free;

		uint32 mem_diag_testid;	/**< Memory diag test ID. */
	} u;
} smbm_test_params_t;

typedef struct smbm_test_results {
	uint16 ver;		/**< Version of this test results structure */
	uint16 len;		/**< Total length structure itself */
	uint16 test_id;		/**< smbm_subcmd_test_id_t */
	uint16 reserved;
	int32  status;		/**< BCME_ error code of test status. */
	union {
		struct {
			uint32 allocated_address;
		} smbm_malloc;

		struct {
			uint32 smbm_user_handle;
		} smbm_user_register;
	} u;
} smbm_test_results_t;

/*
 * sub-cmd ids shared between FW and wl. Required to qualify sub-cmd data
 */
typedef enum {
	HNDEPMU_ver_SUBCMD = 0u,		/* FW returns IOVAR version */
	HNDEPMU_reg0_SUBCMD = 1u,	/* reg0 read/write */
	HNDEPMU_dvfs_SUBCMD = 2u,	/* dvfs registers read/write */
	HNDEPMU_vreg_SUBCMD = 3u,	/* vreg registers read/write */
	HNDEPMU_sws_SUBCMD = 4u,	/* sws registers read/write */
	HNDEPMU_chip_SUBCMD = 5u,	/* chip registers read/write */
	HNDEPMU_otp_SUBCMD = 6u,	/* otp registers read/write */
	HNDEPMU_wsl_SUBCMD = 7u,	/* wsl registers read/write */
	HNDEPMU_revinfo_SUBCMD = 8u,	/* FW returns chip revinfo */
	HNDEPMU_dump_SUBCMD = 9u,	/* provides dump of epmu region or other context */
	HNDEPMU_reg_SUBCMD = 0xFEu	/* epmu(all) registers read/write */
} epmu_subcmd_id_t;

#define HNDEPMU_SUBCMD(r) HNDEPMU_## r ##_SUBCMD

#define HNDEPMU_SUBCMD_CNT	10u

#define EPMU_IOVAR_CMD_VER_0	0u
#define EPMU_IOVAR_CMD_VER	EPMU_IOVAR_CMD_VER_0

/*
 * Used in register read/write sub-commands
 */
typedef struct epmu_reg_rw {
	uint16 addr; /* epmu register address */
	uint16 val;  /* value read or to be written */
} epmu_reg_rw_t;

/*
 * used in 'revinfo' subcmd
 */
typedef struct epmu_rev_info {
	uint16 ver;
	uint16 len;
	uint16 chipid;	/* chip id */
	uint16 revid;	/* revision id */
} epmu_rev_info_t;

/*
 * wl epmu dump - defines
 */

#define EPMU_DVFS_REGN_STR	"dvfs"
#define EPMU_VREG_REGN_STR	"vreg"
#define EPMU_SWS_REGN_STR	"sws"
#define EPMU_CHIP_REGN_STR	"chip"
#define EPMU_OTP_REGN_STR	"otp"
#define EPMU_WSL_REGN_STR	"wsl"

/*
 * 'dump' sub-cmd response struct
 * 'ver' - version of the implementaton
 * 'len' - strlen(val) + 1;
 * 'val' - string of register 'addr: val' pairs
 */
typedef struct epmu_dump_resp {
	uint16 ver;
	uint16 len;
	char val[];		/* preformated string of register 'addr: val' pairs */
} epmu_dump_resp_t;

typedef struct flowring_stats_cfg {
	uint8 ifindex;
	uint8 tid;
	uint8 peer_mac[6]; /* Peer MAC address */
} flowring_stats_cfg_t;

#define EPMU_DUMP_VER_0		0u
#define EPMU_DUMP_VER		EPMU_DUMP_VER_0

#define HND_GPDMA_MAX_CHAN	1u
#define HND_GPDMA_CMD_VER_1	1u
#define HND_GPDMA_CMD_VER	HND_GPDMA_CMD_VER_1

typedef struct hnd_gpdma_cmd {
	uint16 ver;
	uint16 len;
	uint8 data[];
} hnd_gpdma_cmd_t;

typedef struct hnd_gpdma_cmd_payload_v1 {
	uint8 coalesce_descr;	/* Coalesce descriptors? */
	uint8 verify_data;	/* verify data contents at the end of each DMA transfer */
	uint16 num_iterations;	/* Number of test iterations */
} hnd_gpdma_cmd_payload_v1_t;

/*
 * GPDMA sub-cmd ids shared between FW and wl. Required to qualify sub-cmd data
 */
typedef enum {
	HND_GPDMA_SUBCMD_RSVD = 0u,		/* Reserved */
	HND_GPDMA_SUBCMD_DUMP = 1u,		/* dump */
	HND_GPDMA_SUBCMD_INBOUND_SYNC = 2u,	/* SYNC DMA read from HW memories */
	HND_GPDMA_SUBCMD_OUTBOUND_SYNC = 3u,	/* SYNC DMA write to HW memories */
	HND_GPDMA_SUBCMD_CONCURRENT_SYNC = 4u,	/* SYNC concurrent read/write */
	HND_GPDMA_SUBCMD_MAX
} hnd_gpdma_subcmd_id_t;
#endif /* _dngl_ioctl_h_ */
