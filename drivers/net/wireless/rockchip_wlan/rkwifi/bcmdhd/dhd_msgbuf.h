/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * DHD debugability header file
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

#ifndef _dhd_msgbuf_h_
#define _dhd_msgbuf_h_

#include <bcmpcie.h>
#include <dhd.h>
#include <dhd_flowring.h>
#include <pcie_core.h>
#include <dhd_pcie.h>
#include <bcmmsgbuf.h>
#include <dhd_plat.h>
#include <bcmendian.h>

#include <typedefs.h>
#include <osl.h>

#include <bcmutils.h>
#include <bcmmsgbuf.h>
#include <bcmendian.h>
#include <bcmstdlib_s.h>

#include <dngl_stats.h>
#include <dhd.h>
#include <dhd_proto.h>
#include <dhd_bus.h>

#include <typedefs.h>
#include <osl.h>

#include <bcmutils.h>
#include <bcmmsgbuf.h>
#include <bcmendian.h>
#include <bcmstdlib_s.h>

#include <dngl_stats.h>
#include <dhd.h>
#include <dhd_proto.h>
#include <pcicfg.h>
#include <sbchipc.h>
#include <dhd_dbg.h>

/*  data structure align */
#define RING_NAME_MAX_LENGTH 24
#define TXP_FLUSH_NITEMS
#define DHD_DEBUG_INVALID_PKTID

/*
 * +----------------------------------------------------------------------------
 *
 * RingIds and FlowId are not equivalent as ringids include D2H rings whereas
 * flowids do not.
 *
 * Dongle advertizes the max H2D rings, as max_sub_queues = 'N' which includes
 * the H2D common rings as well as the (N-BCMPCIE_H2D_COMMON_MSGRINGS) flowrings
 *
 * Here is a sample mapping for (based on PCIE Full Dongle Rev5) where,
 *  BCMPCIE_H2D_COMMON_MSGRINGS = 2, i.e. 2 H2D common rings,
 *  BCMPCIE_COMMON_MSGRINGS     = 5, i.e. include 3 D2H common rings.
 *
 *  H2D Control  Submit   RingId = 0        FlowId = 0 reserved never allocated
 *  H2D RxPost   Submit   RingId = 1        FlowId = 1 reserved never allocated
 *
 *  D2H Control  Complete RingId = 2
 *  D2H Transmit Complete RingId = 3
 *  D2H Receive  Complete RingId = 4
 *
 *  H2D TxPost   FLOWRING RingId = 5         FlowId = 2     (1st flowring)
 *  H2D TxPost   FLOWRING RingId = 6         FlowId = 3     (2nd flowring)
 *  H2D TxPost   FLOWRING RingId = 5 + (N-1) FlowId = (N-1) (Nth flowring)
 *
 * When TxPost FlowId(s) are allocated, the FlowIds [0..FLOWID_RESERVED) are
 * unused, where FLOWID_RESERVED is BCMPCIE_H2D_COMMON_MSGRINGS.
 *
 * Example: when a system supports 4 bc/mc and 128 uc flowrings, with
 * BCMPCIE_H2D_COMMON_MSGRINGS = 2, and BCMPCIE_H2D_COMMON_MSGRINGS = 5, and the
 * FlowId values would be in the range [2..133] and the corresponding
 * RingId values would be in the range [5..136].
 *
 * The flowId allocator, may chose to, allocate Flowids:
 *   bc/mc (per virtual interface) in one consecutive range [2..(2+VIFS))
 *   X# of uc flowids in consecutive ranges (per station Id), where X is the
 *   packet's access category (e.g. 4 uc flowids per station).
 *
 * CAUTION:
 * When DMA indices array feature is used, RingId=5, corresponding to the 0th
 * FLOWRING, will actually use the FlowId as index into the H2D DMA index,
 * since the FlowId truly represents the index in the H2D DMA indices array.
 *
 * Likewise, in the D2H direction, the RingId - BCMPCIE_H2D_COMMON_MSGRINGS,
 * will represent the index in the D2H DMA indices array.
 *
 * +----------------------------------------------------------------------------
 */

/* First TxPost Flowring Id */
#define DHD_FLOWRING_START_FLOWID   BCMPCIE_H2D_COMMON_MSGRINGS

/* Determine whether a ringid belongs to a TxPost flowring */
#define DHD_IS_FLOWRING(ringid, max_flow_rings) \
	((ringid) >= BCMPCIE_COMMON_MSGRINGS && \
	(ringid) < ((max_flow_rings) + BCMPCIE_COMMON_MSGRINGS))

/* Convert a H2D TxPost FlowId to a MsgBuf RingId */
#define DHD_FLOWID_TO_RINGID(flowid) \
	(BCMPCIE_COMMON_MSGRINGS + ((flowid) - BCMPCIE_H2D_COMMON_MSGRINGS))

/* Convert a MsgBuf RingId to a H2D TxPost FlowId */
#define DHD_RINGID_TO_FLOWID(ringid) \
	(BCMPCIE_H2D_COMMON_MSGRINGS + ((ringid) - BCMPCIE_COMMON_MSGRINGS))

/* Convert a H2D MsgBuf RingId to an offset index into the H2D DMA indices array
 * This may be used for the H2D DMA WR index array or H2D DMA RD index array or
 * any array of H2D rings.
 */
#define DHD_H2D_RING_OFFSET(ringid) \
	(((ringid) >= BCMPCIE_COMMON_MSGRINGS) ? DHD_RINGID_TO_FLOWID(ringid) : (ringid))

/* Convert a H2D MsgBuf Flowring Id to an offset index into the H2D DMA indices array
 * This may be used for IFRM.
 */
#define DHD_H2D_FRM_FLOW_RING_OFFSET(ringid) \
	((ringid) - BCMPCIE_COMMON_MSGRINGS)

/* Convert a D2H MsgBuf RingId to an offset index into the D2H DMA indices array
 * This may be used for the D2H DMA WR index array or D2H DMA RD index array or
 * any array of D2H rings.
 * d2h debug ring is located at the end, i.e. after all the tx flow rings and h2d debug ring
 * max_h2d_rings: total number of h2d rings
 */
#define DHD_D2H_RING_OFFSET(ringid, max_h2d_rings) \
	((ringid) > (max_h2d_rings) ? \
		((ringid) - max_h2d_rings) : \
		((ringid) - BCMPCIE_H2D_COMMON_MSGRINGS))

/* Convert a D2H DMA Indices Offset to a RingId */
#define DHD_D2H_RINGID(offset) \
	((offset) + BCMPCIE_H2D_COMMON_MSGRINGS)

/* The ringid and flowid and dma indices array index idiosyncracy is error
 * prone. While a simplification is possible, the backward compatability
 * requirement (DHD should operate with any PCIE rev version of firmware),
 * limits what may be accomplished.
 *
 * At the minimum, implementation should use macros for any conversions
 * facilitating introduction of future PCIE FD revs that need more "common" or
 * other dynamic rings.
 */

/*
 * +----------------------------------------------------------------------------
 * Flowring Pool
 *
 * Unlike common rings, which are attached very early on (dhd_prot_attach),
 * flowrings are dynamically instantiated. Moreover, flowrings may require a
 * larger DMA-able buffer. To avoid issues with fragmented cache coherent
 * DMA-able memory, a pre-allocated pool of msgbuf_ring_t is allocated once.
 * The DMA-able buffers are attached to these pre-allocated msgbuf_ring.
 *
 * Each DMA-able buffer may be allocated independently, or may be carved out
 * of a single large contiguous region that is registered with the protocol
 * layer into flowrings_dma_buf. On a 64bit platform, this contiguous region
 * may not span 0x00000000FFFFFFFF (avoid dongle side 64bit ptr arithmetic).
 *
 * No flowring pool action is performed in dhd_prot_attach(), as the number
 * of h2d rings is not yet known.
 *
 * In dhd_prot_init(), the dongle advertized number of h2d rings is used to
 * determine the number of flowrings required, and a pool of msgbuf_rings are
 * allocated and a DMA-able buffer (carved or allocated) is attached.
 * See: dhd_prot_flowrings_pool_attach()
 *
 * A flowring msgbuf_ring object may be fetched from this pool during flowring
 * creation, using the flowid. Likewise, flowrings may be freed back into the
 * pool on flowring deletion.
 * See: dhd_prot_flowrings_pool_fetch(), dhd_prot_flowrings_pool_release()
 *
 * In dhd_prot_detach(), the flowring pool is detached. The DMA-able buffers
 * are detached (returned back to the carved region or freed), and the pool of
 * msgbuf_ring and any objects allocated against it are freed.
 * See: dhd_prot_flowrings_pool_detach()
 *
 * In dhd_prot_reset(), the flowring pool is simply reset by returning it to a
 * state as-if upon an attach. All DMA-able buffers are retained.
 * Following a dhd_prot_reset(), in a subsequent dhd_prot_init(), the flowring
 * pool attach will notice that the pool persists and continue to use it. This
 * will avoid the case of a fragmented DMA-able region.
 *
 * +----------------------------------------------------------------------------
 */

	/* Conversion of a flowid to a flowring pool index */
#define DHD_FLOWRINGS_POOL_OFFSET(flowid) \
	((flowid) - BCMPCIE_H2D_COMMON_MSGRINGS)

/* Fetch the msgbuf_ring_t from the flowring pool given a flowid */
#define DHD_RING_IN_FLOWRINGS_POOL(prot, flowid) \
	((msgbuf_ring_t *)((prot)->h2d_flowrings_pool) + \
	    DHD_FLOWRINGS_POOL_OFFSET(flowid))

/* Traverse each flowring in the flowring pool, assigning ring and flowid */
#define FOREACH_RING_IN_FLOWRINGS_POOL(prot, ring, flowid, total_flowrings) \
	for ((flowid) = DHD_FLOWRING_START_FLOWID, \
		(ring) = DHD_RING_IN_FLOWRINGS_POOL(prot, flowid); \
		 (flowid) < ((total_flowrings) + DHD_FLOWRING_START_FLOWID); \
		 (ring)++, (flowid)++)

#define USE_DHD_PKTID_LOCK   1

#ifdef USE_DHD_PKTID_LOCK
#define DHD_PKTID_LOCK_INIT(osh)                osl_spin_lock_init(osh)
#define DHD_PKTID_LOCK_DEINIT(osh, lock)        osl_spin_lock_deinit(osh, lock)
#define DHD_PKTID_LOCK(lock, flags)             ((flags) = osl_spin_lock(lock))
#define DHD_PKTID_UNLOCK(lock, flags)           osl_spin_unlock(lock, flags)
#else
#define DHD_PKTID_LOCK_INIT(osh)                ((void *)(1))
#define DHD_PKTID_LOCK_DEINIT(osh, lock)	\
	do { \
		BCM_REFERENCE(osh); \
		BCM_REFERENCE(lock); \
	} while (0)
#define DHD_PKTID_LOCK(lock)                    0
#define DHD_PKTID_UNLOCK(lock, flags)           \
	do { \
		BCM_REFERENCE(lock); \
		BCM_REFERENCE(flags); \
	} while (0)
#endif /* !USE_DHD_PKTID_LOCK */

typedef enum dhd_locker_state {
	LOCKER_IS_FREE,
	LOCKER_IS_BUSY,
	LOCKER_IS_RSVD
} dhd_locker_state_t;

/* Enum for marking the buffer color based on usage */
typedef enum dhd_pkttype {
	PKTTYPE_DATA_TX = 0,
	PKTTYPE_DATA_RX,
	PKTTYPE_IOCTL_RX,
	PKTTYPE_EVENT_RX,
	PKTTYPE_INFO_RX,
	/* dhd_prot_pkt_free no check, if pktid reserved and no space avail case */
	PKTTYPE_NO_CHECK,
	PKTTYPE_TSBUF_RX
} dhd_pkttype_t;

typedef struct dhd_pktid_item {
	dhd_locker_state_t state;  /* tag a locker to be free, busy or reserved */
	uint8       dir;      /* dma map direction (Tx=flush or Rx=invalidate) */
	dhd_pkttype_t pkttype; /* pktlists are maintained based on pkttype */
	uint16      len;      /* length of mapped packet's buffer */
	void        *pkt;     /* opaque native pointer to a packet */
	dmaaddr_t   pa;       /* physical address of mapped packet's buffer */
	void        *dmah;    /* handle to OS specific DMA map */
	void		*secdma;

} dhd_pktid_item_t;

typedef uint32 dhd_pktid_key_t;

typedef struct dhd_pktid_map {
	uint32      items;    /* total items in map */
	uint32      avail;    /* total available items */
	int         failures; /* lockers unavailable count */
	/* Spinlock to protect dhd_pktid_map in process/tasklet context */
	void        *pktid_lock; /* Used when USE_DHD_PKTID_LOCK is defined */

#if defined(DHD_PKTID_AUDIT_ENABLED)
	void		*pktid_audit_lock;
	struct bcm_mwbmap *pktid_audit; /* multi word bitmap based audit */
#endif /* DHD_PKTID_AUDIT_ENABLED */
	dhd_pktid_key_t	*keys; /* map_items +1 unique pkt ids */
	dhd_pktid_item_t lockers[];           /* metadata storage */
} dhd_pktid_map_t;

typedef void *dhd_pktid_map_handle_t; /* opaque handle to a pktid map */

#ifdef AGG_H2D_DB
typedef struct _agg_h2d_db_info {
	void *dhd;
	struct hrtimer timer;
	bool init;
	uint32 direct_db_cnt;
	uint32 timer_db_cnt;
	uint64  *inflight_histo;
} agg_h2d_db_info_t;
#endif /* AGG_H2D_DB */

/* Used in loopback tests */
typedef struct dhd_dmaxfer {
	dhd_dma_buf_t srcmem;
	dhd_dma_buf_t dstmem;
	uint32        srcdelay;
	uint32        destdelay;
	uint32        len;
	bool          in_progress;
	uint64        start_usec;
	uint64        time_taken;
	uint32        d11_lpbk;
	int           status;
} dhd_dmaxfer_t;

#ifdef DHD_HMAPTEST
/* Used in HMAP test */
typedef struct dhd_hmaptest {
	dhd_dma_buf_t	mem;
	uint32		len;
	bool	in_progress;
	uint32	is_write;
	uint32	accesstype;
	uint64  start_usec;
	uint32	offset;
} dhd_hmaptest_t;
#endif /* DHD_HMAPTEST */

#ifdef TX_FLOW_RING_INDICES_TRACE

/* By default 4K, which can be overridden by Makefile */
#ifndef TX_FLOW_RING_INDICES_TRACE_SIZE
#define TX_FLOW_RING_INDICES_TRACE_SIZE (4 * 1024)
#endif /* TX_FLOW_RING_INDICES_TRACE_SIZE */

typedef struct rw_trace {
	uint64 timestamp;
	uint32 err_rollback_idx_cnt;
	uint16 rd; /* shared mem or dma rd */
	uint16 wr; /* shared mem or dma wr */
	uint16 local_wr;
	uint16 local_rd;
	uint8 current_phase;
	bool start; /* snapshot updated from the start of tx function */
} rw_trace_t;
#endif /* TX_FLOW_RING_INDICES_TRACE */

/**
 * msgbuf_ring : This object manages the host side ring that includes a DMA-able
 * buffer, the WR and RD indices, ring parameters such as max number of items
 * an length of each items, and other miscellaneous runtime state.
 * A msgbuf_ring may be used to represent a H2D or D2H common ring or a
 * H2D TxPost ring as specified in the PCIE FullDongle Spec.
 * Ring parameters are conveyed to the dongle, which maintains its own peer end
 * ring state. Depending on whether the DMA Indices feature is supported, the
 * host will update the WR/RD index in the DMA indices array in host memory or
 * directly in dongle memory.
 */
typedef struct msgbuf_ring {
	bool           inited;
	uint16         idx;       /* ring id */
	uint16         rd;        /* read index */
	uint16         curr_rd;   /* read index for debug */
	uint16         wr;        /* write index */
	uint16         max_items; /* maximum number of items in ring */
	uint16         item_len;  /* length of each item in the ring */
	sh_addr_t      base_addr; /* LITTLE ENDIAN formatted: base address */
	dhd_dma_buf_t  dma_buf;   /* DMA-able buffer: pa, va, len, dmah, secdma */
	uint32         seqnum;    /* next expected item's sequence number */
#ifdef TXP_FLUSH_NITEMS
	void           *start_addr;
	/* # of messages on ring not yet announced to dongle */
	uint16         pend_items_count;
#ifdef AGG_H2D_DB
	osl_atomic_t	inflight;
#endif /* AGG_H2D_DB */
#endif /* TXP_FLUSH_NITEMS */

	uint8   ring_type;
	uint8   n_completion_ids;
	bool    create_pending;
	uint16  create_req_id;
	uint8   current_phase;
	uint16	compeltion_ring_ids[MAX_COMPLETION_RING_IDS_ASSOCIATED];
	uchar		name[RING_NAME_MAX_LENGTH];
	uint32		ring_mem_allocated;
	void	*ring_lock;

	bool   mesh_ring;	/* TRUE if it is a mesh ring */
#ifdef DHD_AGGR_WI
	aggr_state_t aggr_state; /* Aggregation state */
	uint8 pending_pkt; /* Pending packet count */
#endif /* DHD_AGGR_WI */
	struct msgbuf_ring *linked_ring; /* Ring Associated to metadata ring */
#ifdef TX_FLOW_RING_INDICES_TRACE
	rw_trace_t *tx_flow_rw_trace;
	uint32	tx_flow_rw_trace_cnt;
	uint32  err_rollback_idx_cnt;
#endif /* TX_FLOW_RING_INDICES_TRACE */
} msgbuf_ring_t;

/**
 * Custom callback attached based upon D2H DMA Sync mode advertized by dongle.
 * For EDL messages.
 *
 * On success: return cmn_msg_hdr_t::msg_type
 * On failure: return 0 (invalid msg_type)
 */
#ifdef EWP_EDL
typedef int (*d2h_edl_sync_cb_t)(dhd_pub_t *dhd, struct msgbuf_ring *ring,
	volatile cmn_msg_hdr_t *msg);
#endif /* EWP_EDL */

/**
 * Custom callback attached based upon D2H DMA Sync mode advertized by dongle.
 *
 * On success: return cmn_msg_hdr_t::msg_type
 * On failure: return 0 (invalid msg_type)
 */
typedef uint8 (*d2h_sync_cb_t)(dhd_pub_t *dhd, struct msgbuf_ring *ring,
	volatile cmn_msg_hdr_t *msg, int msglen);

#ifdef DHD_AGGR_WI

typedef struct dhd_aggr_stat {
	uint32 aggr_txpost;	/* # of aggregated txpost work item posted */
	uint32 aggr_rxpost;	/* # of aggregated rxpost work item posted */
	uint32 aggr_txcpl;	/* # of aggregated txcpl work item processed */
	uint32 aggr_rxcpl;	/* # of aggregated rxcpl work item processed */
} dhd_aggr_stat_t;

#endif /* DHD_AGGR_WI */

/** DHD protocol handle. Is an opaque type to other DHD software layers. */
typedef struct dhd_prot {
	osl_t *osh;		/* OSL handle */
	uint16 rxbufpost_sz;		/* Size of rx buffer posted to dongle */
	uint16 rxbufpost_alloc_sz;	/* Actual rx buffer packet allocated in the host */
	osl_atomic_t rxbufpost;         /* making rxbufpost atomic, as its shared across contexts */
	uint16 rx_buf_burst;
	uint16 rx_bufpost_threshold;
	uint16 max_rxbufpost;
	uint32 tot_rxbufpost;
	uint32 tot_rxcpl;
	void *rxp_bufinfo_pool;		/* Scratch buffer pool to hold va, pa and pktlens */
	uint16 rxp_bufinfo_pool_size;	/* scartch buffer pool length */
	uint16 max_eventbufpost;
	uint16 max_ioctlrespbufpost;
	uint16 max_tsbufpost;
	uint16 max_infobufpost;
	uint16 infobufpost;
	uint16 cur_event_bufs_posted;
	uint16 cur_ioctlresp_bufs_posted;
	uint16 cur_ts_bufs_posted;

	/* Flow control mechanism based on active transmits pending */
	osl_atomic_t active_tx_count; /* increments/decrements on every packet tx/tx_status */
	uint16 h2d_max_txpost;
	uint16 h2d_htput_max_txpost;
	uint16 txp_threshold;  /* optimization to write "n" tx items at a time to ring */

	/* MsgBuf Ring info: has a dhd_dma_buf that is dynamically allocated */
	msgbuf_ring_t h2dring_ctrl_subn; /* H2D ctrl message submission ring */
	msgbuf_ring_t h2dring_rxp_subn; /* H2D RxBuf post ring */
	msgbuf_ring_t d2hring_ctrl_cpln; /* D2H ctrl completion ring */
	msgbuf_ring_t d2hring_tx_cpln; /* D2H Tx complete message ring */
	msgbuf_ring_t d2hring_rx_cpln; /* D2H Rx complete message ring */
	msgbuf_ring_t *h2dring_info_subn; /* H2D info submission ring */
	msgbuf_ring_t *d2hring_info_cpln; /* D2H info completion ring */
	msgbuf_ring_t *d2hring_edl; /* D2H Enhanced Debug Lane (EDL) ring */

	msgbuf_ring_t *h2d_flowrings_pool; /* Pool of preallocated flowings */
	dhd_dma_buf_t flowrings_dma_buf; /* Contiguous DMA buffer for flowrings */
	uint16        h2d_rings_total; /* total H2D (common rings + flowrings) */

	uint32		rx_dataoffset;

	dhd_mb_ring_t	mb_ring_fn;	/* called when dongle needs to be notified of new msg */
	dhd_mb_ring_2_t	mb_2_ring_fn;	/* called when dongle needs to be notified of new msg */
#ifdef DHD_DB0TS
	dhd_mb_ring_t	idma_db0_fn;	/* called when dongle needs to be notified of timestamp */
#endif /* DHD_DB0TS */

	/* ioctl related resources */
	uint8 ioctl_state;
	int16 ioctl_status;		/* status returned from dongle */
	uint16 ioctl_resplen;
	dhd_ioctl_received_status_t ioctl_received;
	uint curr_ioctl_cmd;
	dhd_dma_buf_t	retbuf;		/* For holding ioctl response */
	dhd_dma_buf_t	ioctbuf;	/* For holding ioctl request */

	dhd_dma_buf_t	d2h_dma_scratch_buf;	/* For holding d2h scratch */

	/* DMA-able arrays for holding WR and RD indices */
	uint32          rw_index_sz; /* Size of a RD or WR index in dongle */
	dhd_dma_buf_t   h2d_dma_indx_wr_buf;	/* Array of H2D WR indices */
	dhd_dma_buf_t	h2d_dma_indx_rd_buf;	/* Array of H2D RD indices */
	dhd_dma_buf_t	d2h_dma_indx_wr_buf;	/* Array of D2H WR indices */
	dhd_dma_buf_t	d2h_dma_indx_rd_buf;	/* Array of D2H RD indices */
	dhd_dma_buf_t h2d_ifrm_indx_wr_buf;	/* Array of H2D WR indices for ifrm */

	dhd_dma_buf_t	host_bus_throughput_buf; /* bus throughput measure buffer */

	dhd_dma_buf_t   *flowring_buf;    /* pool of flow ring buf */
#ifdef DHD_DMA_INDICES_SEQNUM
	char *h2d_dma_indx_rd_copy_buf; /* Local copy of H2D WR indices array */
	char *d2h_dma_indx_wr_copy_buf; /* Local copy of D2H WR indices array */
	uint32 h2d_dma_indx_rd_copy_bufsz; /* H2D WR indices array size */
	uint32 d2h_dma_indx_wr_copy_bufsz; /* D2H WR indices array size */
	uint32 host_seqnum;	/* Seqence number for D2H DMA Indices sync */
#endif /* DHD_DMA_INDICES_SEQNUM */
	uint32			flowring_num;

	d2h_sync_cb_t d2h_sync_cb; /* Sync on D2H DMA done: SEQNUM or XORCSUM */
#ifdef EWP_EDL
	d2h_edl_sync_cb_t d2h_edl_sync_cb; /* Sync on EDL D2H DMA done: SEQNUM or XORCSUM */
#endif /* EWP_EDL */
	ulong d2h_sync_wait_max; /* max number of wait loops to receive one msg */
	ulong d2h_sync_wait_tot; /* total wait loops */

	dhd_dmaxfer_t	dmaxfer; /* for test/DMA loopback */

	uint16		ioctl_seq_no;
	uint16		data_seq_no;  /* this field is obsolete */
	uint16		ioctl_trans_id;
	void		*pktid_ctrl_map; /* a pktid maps to a packet and its metadata */
	void		*pktid_rx_map;	/* pktid map for rx path */
	void		*pktid_tx_map;	/* pktid map for tx path */
	bool		metadata_dbg;
	void		*pktid_map_handle_ioctl;
#ifdef DHD_MAP_PKTID_LOGGING
	void		*pktid_dma_map;	/* pktid map for DMA MAP */
	void		*pktid_dma_unmap; /* pktid map for DMA UNMAP */
#endif /* DHD_MAP_PKTID_LOGGING */

	uint64		ioctl_fillup_time;	/* timestamp for ioctl fillup */
	uint64		ioctl_ack_time;		/* timestamp for ioctl ack */
	uint64		ioctl_cmplt_time;	/* timestamp for ioctl completion */

	/* Applications/utilities can read tx and rx metadata using IOVARs */
	uint16		rx_metadata_offset;
	uint16		tx_metadata_offset;

#if (defined(BCM_ROUTER_DHD) && defined(HNDCTF))
	rxchain_info_t	rxchain;	/* chain of rx packets */
#endif

#if defined(DHD_D2H_SOFT_DOORBELL_SUPPORT)
	/* Host's soft doorbell configuration */
	bcmpcie_soft_doorbell_t soft_doorbell[BCMPCIE_D2H_COMMON_MSGRINGS];
#endif /* DHD_D2H_SOFT_DOORBELL_SUPPORT */

	/* Work Queues to be used by the producer and the consumer, and threshold
	 * when the WRITE index must be synced to consumer's workq
	 */
	dhd_dma_buf_t	fw_trap_buf; /* firmware trap buffer */
#ifdef FLOW_RING_PREALLOC
	/* pre-allocation htput ring buffer */
	dhd_dma_buf_t	*prealloc_htput_flowring_buf;
	/* pre-allocation folw ring(non htput rings) */
	dhd_dma_buf_t	*prealloc_regular_flowring_buf;
#endif /* FLOW_RING_PREALLOC */
	uint32  host_ipc_version; /* Host sypported IPC rev */
	uint32  device_ipc_version; /* FW supported IPC rev */
	uint32  active_ipc_version; /* Host advertised IPC rev */
	dhd_dma_buf_t   hostts_req_buf; /* For holding host timestamp request buf */
	bool    hostts_req_buf_inuse;
	bool    rx_ts_log_enabled;
	bool    tx_ts_log_enabled;
#ifdef BTLOG
	msgbuf_ring_t *h2dring_btlog_subn; /* H2D btlog submission ring */
	msgbuf_ring_t *d2hring_btlog_cpln; /* D2H btlog completion ring */
	uint16 btlogbufpost;
	uint16 max_btlogbufpost;
#endif	/* BTLOG */
#ifdef DHD_HMAPTEST
	uint32 hmaptest_rx_active;
	uint32 hmaptest_rx_pktid;
	char *hmap_rx_buf_va;
	dmaaddr_t hmap_rx_buf_pa;
	uint32 hmap_rx_buf_len;

	uint32 hmaptest_tx_active;
	uint32 hmaptest_tx_pktid;
	char *hmap_tx_buf_va;
	dmaaddr_t hmap_tx_buf_pa;
	uint32	  hmap_tx_buf_len;
	dhd_hmaptest_t	hmaptest; /* for hmaptest */
	bool hmap_enabled; /* TRUE = hmap is enabled */
#endif /* DHD_HMAPTEST */
#ifdef SNAPSHOT_UPLOAD
	dhd_dma_buf_t snapshot_upload_buf;	/* snapshot upload buffer */
	uint32 snapshot_upload_len;		/* snapshot uploaded len */
	uint8 snapshot_type;			/* snapshot uploaded type */
	bool snapshot_cmpl_pending;		/* snapshot completion pending */
#endif	/* SNAPSHOT_UPLOAD */
	bool no_retry;
	bool no_aggr;
	bool fixed_rate;
	bool rts_protect;
	dhd_dma_buf_t	host_scb_buf; /* scb host offload buffer */
#if defined(DHD_MESH)
	msgbuf_ring_t *d2hring_mesh_rxcpl; /* D2H Mesh Rx completion ring */
#endif /* DHD_MESH */
	uint32 txcpl_db_cnt;
#ifdef AGG_H2D_DB
	agg_h2d_db_info_t agg_h2d_db_info;
#endif /* AGG_H2D_DB */
	uint64 tx_h2d_db_cnt;
#ifdef DHD_DEBUG_INVALID_PKTID
	uint8 ctrl_cpl_snapshot[D2HRING_CTRL_CMPLT_ITEMSIZE];
#endif /* DHD_DEBUG_INVALID_PKTID */
	uint32 event_wakeup_pkt; /* Number of event wakeup packet rcvd */
	uint32 rx_wakeup_pkt;    /* Number of Rx wakeup packet rcvd */
	uint32 info_wakeup_pkt;  /* Number of info cpl wakeup packet rcvd */
#ifdef DHD_AGGR_WI
	dhd_aggr_stat_t aggr_stat; /* Aggregated work item statistics */
#endif /* DHD_AGGR_WI */
	uint32 rxbuf_post_err;
	msgbuf_ring_t *d2hring_md_cpl; /* D2H metadata completion ring */
	/* no. which controls how many rx cpl/post items are processed per dpc */
	uint32 rx_cpl_post_bound;
	/*
	 * no. which controls how many tx post items are processed per dpc,
	 * i.e., how many tx pkts are posted to flowring from the bkp queue
	 * from dpc context
	 */
	uint32 tx_post_bound;
	/* no. which controls how many tx cpl items are processed per dpc */
	uint32 tx_cpl_bound;
	/* no. which controls how many ctrl cpl/post items are processed per dpc */
	uint32 ctrl_cpl_post_bound;
} dhd_prot_t;

#endif /* _dhd_msgbuf_h_ */
