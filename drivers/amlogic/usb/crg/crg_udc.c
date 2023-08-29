// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/net.h>
#include <asm/byteorder.h>
#include <linux/errno.h>
#include <linux/io.h>
#include <asm/unaligned.h>
#include <linux/types.h>
#include <linux/bitops.h>
#include <linux/dma-mapping.h>
#include <linux/usb/ch9.h>
#include <linux/usb/gadget.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <linux/irqreturn.h>
#include <linux/interrupt.h>
#include <linux/ctype.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/err.h>
#include <linux/wait.h>
#include <linux/kernel.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/amlogic/usbtype.h>
#include <linux/of_gpio.h>
#include <linux/clk.h>
#include "crg_udc.h"
#include <linux/amlogic/usb-v2.h>

#define CRG_MTP_WR 1

#define MAX_PACKET_SIZE 1024
int g_dnl_board_usb_cable_connected(void);

#define	DMA_ADDR_INVALID	(~(dma_addr_t)0)

#define CRG_ERST_SIZE 1
#define CRG_EVENT_RING_SIZE 256
#define CRG_NUM_EP_CX	32

#define TRB_MAX_BUFFER_SIZE		65536
#define CRGUDC_CONTROL_EP_TD_RING_SIZE	16
#if CRG_MTP_WR
#define CRGUDC_BULK_EP_TD_RING_SIZE	512
#else
#define CRGUDC_BULK_EP_TD_RING_SIZE	32
#endif
#define CRGUDC_ISOC_EP_TD_RING_SIZE	32
#define CRGUDC_INT_EP_TD_RING_SIZE	8

/*#define	U1_TIMEOUT_VAL	0x70*/
/*#define	U2_TIMEOUT_VAL	0x70*/
#define	U1_TIMEOUT_VAL	0x70
#define	U2_TIMEOUT_VAL	0x70

/*********Feature switches********************/
#define U12_FORBIDDEN	1
#define U12_INITIATE_FORBIDDEN 1
#define CRG_UDC_INT_EN
#define REINIT_EP0_ON_BUS_RESET
/*********************************************/

enum EP_STATE_E {
	EP_STATE_DISABLED = 0,
	EP_STATE_RUNNING = 1,
	EP_STATE_HALTED	= 2,
	EP_STATE_STOPPED = 3
};

enum EP_TYPE_E {
	EP_TYPE_INVALID = 0,
	EP_TYPE_ISOCH_OUTBOUND,
	EP_TYPE_BULK_OUTBOUND,
	EP_TYPE_INTR_OUTBOUND,
	EP_TYPE_INVALID2,
	EP_TYPE_ISOCH_INBOUND,
	EP_TYPE_BULK_INBOUND,
	EP_TYPE_INTR_INBOUND
};

enum TRB_TYPE_E {
	TRB_TYPE_RSVD = 0,
	TRB_TYPE_XFER_NORMAL,
	TRB_TYPE_RSVD2,
	TRB_TYPE_XFER_DATA_STAGE,
	TRB_TYPE_XFER_STATUS_STAGE,
	TRB_TYPE_XFER_DATA_ISOCH,   /* 5*/
	TRB_TYPE_LINK,
	TRB_TYPE_RSVD7,
	TRB_TYPE_NO_OP,

	TRB_TYPE_EVT_TRANSFER = 32,
	TRB_TYPE_EVT_CMD_COMPLETION = 33,
	TRB_TYPE_EVT_PORT_STATUS_CHANGE = 34,
	TRB_TYPE_EVT_MFINDEX_WRAP = 39,
	TRB_TYPE_EVT_SETUP_PKT = 40,
};

/*Table 127*/
enum TRB_CMPL_CODES_E {
	CMPL_CODE_INVALID       = 0,
	CMPL_CODE_SUCCESS,
	CMPL_CODE_DATA_BUFFER_ERR,
	CMPL_CODE_BABBLE_DETECTED_ERR,
	CMPL_CODE_USB_TRANS_ERR,
	CMPL_CODE_TRB_ERR,  /*5*/
	CMPL_CODE_TRB_STALL,
	CMPL_CODE_INVALID_STREAM_TYPE_ERR = 10,
	CMPL_CODE_SHORT_PKT = 13,
	CMPL_CODE_RING_UNDERRUN,
	CMPL_CODE_RING_OVERRUN, /*15*/
	CMPL_CODE_EVENT_RING_FULL_ERR = 21,
	CMPL_CODE_STOPPED = 26,
	CMPL_CODE_STOPPED_LENGTH_INVALID = 27,
	CMPL_CODE_ISOCH_BUFFER_OVERRUN = 31,
	/*192-224 vendor defined error*/
	CMPL_CODE_PROTOCOL_STALL = 192,
	CMPL_CODE_SETUP_TAG_MISMATCH = 193,
	CMPL_CODE_HALTED = 194,
	CMPL_CODE_HALTED_LENGTH_INVALID = 195,
	CMPL_CODE_DISABLED = 196,
	CMPL_CODE_DISABLED_LENGTH_INVALID = 197,
};

static const char driver_name[] = "crg_udc";

struct buffer_info {
	void *vaddr;
	dma_addr_t dma;
	u32 len;
};

struct transfer_trb_s {
	__le32   dw0;
	__le32   dw1;

#define TRB_TRANSFER_LEN_MASK       0x0001FFFF
#define TRB_TRANSFER_LEN_SHIFT               0
#define TRB_TD_SIZE_MASK            0x003E0000
#define TRB_TD_SIZE_SHIFT                   17
#define TRB_INTR_TARGET_MASK        0xFFC00000
#define TRB_INTR_TARGET_SHIFT               22
	__le32   dw2;

#define TRB_CYCLE_BIT_MASK          0x00000001
#define TRB_CYCLE_BIT_SHIFT                  0
#define TRB_LINK_TOGGLE_CYCLE_MASK  0x00000002
#define TRB_LINK_TOGGLE_CYCLE_SHIFT          1
#define TRB_INTR_ON_SHORT_PKT_MASK  0x00000004
#define TRB_INTR_ON_SHORT_PKT_SHIFT          2
#define TRB_NO_SNOOP_MASK           0x00000008
#define TRB_NO_SNOOP_SHIFT                   3
#define TRB_CHAIN_BIT_MASK          0x00000010
#define TRB_CHAIN_BIT_SHIFT                  4
#define TRB_INTR_ON_COMPLETION_MASK 0x00000020
#define TRB_INTR_ON_COMPLETION_SHIFT         5

#define TRB_APPEND_ZLP_MASK	    0x00000080
#define TRB_APPEND_ZLP_SHIFT		     7

#define TRB_BLOCK_EVENT_INT_MASK    0x00000200
#define TRB_BLOCK_EVENT_INT_SHIFT            9
#define TRB_TYPE_MASK               0x0000FC00
#define TRB_TYPE_SHIFT                      10
#define DATA_STAGE_TRB_DIR_MASK     0x00010000
#define DATA_STAGE_TRB_DIR_SHIFT            16
#define TRB_SETUP_TAG_MASK          0x00060000
#define TRB_SETUP_TAG_SHIFT                 17
#define STATUS_STAGE_TRB_STALL_MASK 0x00080000
#define STATUS_STAGE_TRB_STALL_SHIFT        19
#define STATUS_STAGE_TRB_SET_ADDR_MASK 0x00100000
#define STATUS_STAGE_TRB_SET_ADDR_SHIFT        20

#define ISOC_TRB_FRAME_ID_MASK      0x7FF00000
#define ISOC_TRB_FRAME_ID_SHIFT             20
#define ISOC_TRB_SIA_MASK           0x80000000
#define ISOC_TRB_SIA_SHIFT                  31
	__le32   dw3;
};

struct event_trb_s {
	__le32 dw0;
	__le32 dw1;

#define EVE_TRB_TRAN_LEN_MASK       0x0001FFFF
#define EVE_TRB_TRAN_LEN_SHIFT               0
#define EVE_TRB_COMPL_CODE_MASK     0xFF000000
#define EVE_TRB_COMPL_CODE_SHIFT            24
	__le32 dw2;

#define EVE_TRB_CYCLE_BIT_MASK		0x00000001
#define EVE_TRB_CYCLE_BIT_SHIFT		0
#define EVE_TRB_TYPE_MASK		0x0000FC00
#define EVE_TRB_TYPE_SHIFT		10
#define EVE_TRB_ENDPOINT_ID_MASK	0x001F0000
#define EVE_TRB_ENDPOINT_ID_SHIFT	16
#define EVE_TRB_SETUP_TAG_MASK		0x00600000
#define EVE_TRB_SETUP_TAG_SHIFT		21
	__le32 dw3;
};

struct ep_cx_s {
/*#define EP_CX_EP_STATE_MASK		0x00000007*/
/*#define EP_CX_EP_STATE_SHIFT		0*/

#define EP_CX_LOGICAL_EP_NUM_MASK	0x00000078
#define EP_CX_LOGICAL_EP_NUM_SHIFT	3

/*#define EP_CX_LOGICAL_EP_NUM_MASK	0x0000003c*/
/*#define EP_CX_LOGICAL_EP_NUM_SHIFT	2*/

#define EP_CX_INTERVAL_MASK		0x00FF0000
#define EP_CX_INTERVAL_SHIFT            16
	__le32 dw0;

#define EP_CX_EP_TYPE_MASK		0x00000038
#define EP_CX_EP_TYPE_SHIFT		3
#define EP_CX_MAX_BURST_SIZE_MASK	0x0000FF00
#define EP_CX_MAX_BURST_SIZE_SHIFT	8
#define EP_CX_MAX_PACKET_SIZE_MASK	0xFFFF0000
#define EP_CX_MAX_PACKET_SIZE_SHIFT	16
	__le32 dw1;

#define EP_CX_DEQ_CYC_STATE_MASK	0x00000001
#define EP_CX_DEQ_CYC_STATE_SHIFT	0
#define EP_CX_TR_DQPT_LO_MASK		0xFFFFFFF0
#define EP_CX_TR_DQPT_LO_SHIFT		4
	__le32 dw2;
	__le32 dw3;
};

struct erst_s {
	/* 64-bit event ring segment address */
	__le32	seg_addr_lo;
	__le32	seg_addr_hi;
	__le32	seg_size;
	/* Set to zero */
	__le32	rsvd;
};

struct sel_value_s {
	u16 u2_pel_value;
	u16 u2_sel_value;
	u8 u1_pel_value;
	u8 u1_sel_value;
};

struct crg_udc_request {
	struct usb_request usb_req;
	struct list_head queue;
	bool mapped;
	u64 buff_len_left;
	u32 trbs_needed;
	struct transfer_trb_s *first_trb;
	struct transfer_trb_s *last_trb;
	bool all_trbs_queued;
	bool short_pkt;
	atomic_t used;
};

#define CRE_REQ_NUM 100
struct crg_udc_request g_udc_req_ptr[CRE_REQ_NUM];

struct crg_udc_ep {
	struct usb_ep usb_ep;

	struct buffer_info tran_ring_info;
	struct transfer_trb_s *first_trb;
	struct transfer_trb_s *last_trb;

	struct transfer_trb_s *enq_pt;
	struct transfer_trb_s *deq_pt;
	u8 pcs;

	char name[10];
	u8 DCI;
	struct list_head queue;
	const struct usb_endpoint_descriptor *desc;
	const struct usb_ss_ep_comp_descriptor *comp_desc;
	bool tran_ring_full;
	struct crg_gadget_dev *crg_udc;

	int ep_state;

	unsigned wedge:1;
};

#define CRG_RING_NUM	1

struct crg_udc_event {
	struct buffer_info erst;
	struct erst_s *p_erst;
	struct buffer_info event_ring;
	struct event_trb_s *evt_dq_pt;
	u8 CCS;
	struct event_trb_s *evt_seg0_last_trb;
};

struct crg_setup_packet {
	struct usb_ctrlrequest usbctrlreq;
	u16 setup_tag;
};

struct crg_gadget_dev {
	void __iomem *mmio_virt_base;
	void __iomem *phy_reg_addr;
	struct resource	*mmio_phys_base;
	resource_size_t mmio_phys_len;
	struct crg_uccr *uccr;
	struct crg_uicr *uicr[CRG_RING_NUM];

	/* udc_lock device lock */
	spinlock_t udc_lock;

	struct device *dev;
	struct usb_gadget gadget;
	struct usb_gadget_driver *gadget_driver;

	int irq;
	struct task_struct *vbus_task;

	struct crg_udc_ep udc_ep[32];
	struct buffer_info ep_cx;
	struct ep_cx_s *p_epcx;

	struct crg_udc_event udc_event[CRG_RING_NUM];

	struct crg_udc_request *status_req;
	u16 *statusbuf;
	struct sel_value_s sel_value;
	void (*setup_fn_call_back)(struct crg_gadget_dev *a);

#define WAIT_FOR_SETUP      0
#define SETUP_PKT_PROCESS_IN_PROGRESS 1
#define DATA_STAGE_XFER     2
#define DATA_STAGE_RECV     3
#define STATUS_STAGE_XFER   4
#define STATUS_STAGE_RECV   5
	u8 setup_status;
#define CTRL_REQ_QUEUE_DEPTH  5
	struct crg_setup_packet ctrl_req_queue[CTRL_REQ_QUEUE_DEPTH];
	u8    ctrl_req_enq_idx;

	u8 device_state;
	u8 resume_state;
	u16 dev_addr;
	u8 setup_tag;
	u8 set_tm;

	u32 num_enabled_eps;

	int connected;

	unsigned u2_RWE:1;
	unsigned feature_u1_enable:1;
	unsigned feature_u2_enable:1;

	int setup_tag_mismatch_found;
	int portsc_on_reconnecting;
	int controller_type;
	int phy_id;
};

/*An array should be implemented if we want to support multi
 * usb device controller
 */
static struct crg_gadget_dev crg_udc_dev;
static DECLARE_WAIT_QUEUE_HEAD(vbus_wait);

static bool xfer_debug;
module_param(xfer_debug, bool, 0644);
MODULE_PARM_DESC(xfer_debug, "0 or 1");

static bool crg_debug = 1;
module_param(crg_debug, bool, 0644);
MODULE_PARM_DESC(crg_debug, "0 or 1");

static bool portsc_debug = 1;
module_param(portsc_debug, bool, 0644);
MODULE_PARM_DESC(portsc_debug, "0 or 1");

#ifdef CRG_DUAL_ROLE
static bool dr_udc_debug;
module_param(dr_udc_debug, bool, 0644);
MODULE_PARM_DESC(dr_udc_debug, "0 or 1");
#endif

static bool eltest_flag;
module_param(eltest_flag, bool, 0644);
MODULE_PARM_DESC(eltest_flag, "0 or 1");

#ifdef CRG_UDC_DEBUG
#define xdebug(fmt...)					\
do {									\
	if (xfer_debug)					\
		pr_err(fmt);			\
} while (0)

#define CRG_DEBUG(fmt...)				\
do {									\
	if (crg_debug)						\
		pr_err(fmt);			\
} while (0)

#define pdebug(fmt...)					\
do {									\
	if (portsc_debug)					\
		pr_err(fmt);			\
} while (0)
#else
#define xdebug(fmt...)
#define CRG_DEBUG(fmt...)
#define pdebug(fmt...)
#endif

#define CRG_ERROR(fmt...) pr_err(fmt)

static struct usb_endpoint_descriptor crg_udc_ep0_desc = {
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,
	.bEndpointAddress = 0,
	.bmAttributes = USB_ENDPOINT_XFER_CONTROL,
	.wMaxPacketSize = cpu_to_le16(64),
};

static int get_ep_state(struct crg_gadget_dev *crg_udc, int DCI)
{
	struct crg_udc_ep *udc_ep_ptr;

	if (DCI < 0 || DCI == 1)
		return -EINVAL;

	udc_ep_ptr = &crg_udc->udc_ep[DCI];

	return udc_ep_ptr->ep_state;
}

/************command related ops**************************/
static int crg_issue_command(struct crg_gadget_dev *crg_udc,
			enum crg_cmd_type type, u32 param0, u32 param1)
{
	struct crg_uccr *uccr = crg_udc->uccr;
	u32 status;
	bool check_complete = false;
	u32 tmp;

	tmp = reg_read(&uccr->control);
	if (tmp & CRG_U3DC_CTRL_RUN)
		check_complete = true;

	if (check_complete) {
		tmp = reg_read(&uccr->cmd_control);
		if (tmp & CRG_U3DC_CMD_CTRL_ACTIVE) {
			CRG_ERROR("%s prev command is not complete!\n", __func__);
			return -1;
		}
	}
	/* wmb */
	wmb();

	reg_write(&uccr->cmd_param0, param0);
	reg_write(&uccr->cmd_param1, param1);

	/*ignore CMD IOC, in uboot no irq is*/
	tmp = CRG_U3DC_CMD_CTRL_ACTIVE |
		CRG_U3DC_CMD_CTRL_TYPE(type);
	reg_write(&uccr->cmd_control, tmp);

	CRG_DEBUG("%s start, type=%d, par0=0x%x, par1=0x%x\n",
		__func__, type, param0, param1);

	if (check_complete) {
		do {
			tmp = reg_read(&uccr->cmd_control);
		} while (tmp & CRG_U3DC_CMD_CTRL_ACTIVE);

		CRG_DEBUG("%s successful\n", __func__);

		status = CRG_U3DC_CMD_CTRL_STATUS_GET(tmp);
		if (status != 0) {
			CRG_DEBUG("%s fail\n", __func__);
			return -EIO;
		}
	}

	CRG_DEBUG("%s end\n", __func__);

	return 0;
}

static void setup_link_trb(struct transfer_trb_s *link_trb,
					bool toggle, ulong next_trb)
{
	u32 dw = 0;

	link_trb->dw0 = cpu_to_le32(lower_32_bits(next_trb));
	link_trb->dw1 = cpu_to_le32(upper_32_bits(next_trb));

	link_trb->dw2 = 0;

	dw = SETF_VAR(TRB_TYPE, dw, TRB_TYPE_LINK);
	if (toggle)
		dw = SETF_VAR(TRB_LINK_TOGGLE_CYCLE, dw, 1);
	else
		dw = SETF_VAR(TRB_LINK_TOGGLE_CYCLE, dw, 0);

	link_trb->dw3 = cpu_to_le32(dw);

	/* wmb */
	wmb();
}

static dma_addr_t tran_trb_virt_to_dma(struct crg_udc_ep *udc_ep,
	struct transfer_trb_s *trb)
{
	unsigned long offset;
	int trb_idx;
	dma_addr_t dma_addr = 0;

	trb_idx = trb - udc_ep->first_trb;
	if (unlikely(trb_idx < 0))
		return 0;

	offset = trb_idx * sizeof(*trb);
	if (unlikely(offset > udc_ep->tran_ring_info.len))
		return 0;
	dma_addr = udc_ep->tran_ring_info.dma + offset;
	return dma_addr;
}

static struct transfer_trb_s *tran_trb_dma_to_virt
	(struct crg_udc_ep *udc_ep, dma_addr_t address)
{
	unsigned long offset;
	struct transfer_trb_s *trb_virt;

	if (lower_32_bits(address) & 0xf) {
		CRG_DEBUG("transfer ring dma address incorrect\n");
		return NULL;
	}

	offset = address - udc_ep->tran_ring_info.dma;
	if (unlikely(offset > udc_ep->tran_ring_info.len))
		return NULL;
	offset = offset / sizeof(struct transfer_trb_s);
	trb_virt = udc_ep->first_trb + offset;
	return trb_virt;
}

static dma_addr_t event_trb_virt_to_dma
	(struct crg_udc_event *udc_event, struct event_trb_s *event)
{
	dma_addr_t dma_addr = 0;
	unsigned long seg_offset;

	if (!udc_event || !event)
		return 0;

	/* update dequeue pointer */
	seg_offset = (void *)event - udc_event->event_ring.vaddr;
	dma_addr = udc_event->event_ring.dma + seg_offset;

	return dma_addr;
}

/* Completes request.  Calls gadget completion handler
 * caller must have acquired spin lock.
 */
static void req_done(struct crg_udc_ep *udc_ep,
			struct crg_udc_request *udc_req, int status)
{
	unsigned long flags = 0;
	struct crg_gadget_dev *crg_udc = udc_ep->crg_udc;

	if (likely(udc_req->usb_req.status == -EINPROGRESS))
		udc_req->usb_req.status = status;

	list_del_init(&udc_req->queue);

	if (udc_req->mapped) {
		if (udc_req->usb_req.length) {
			dma_unmap_single(crg_udc->dev, udc_req->usb_req.dma,
				udc_req->usb_req.length, usb_endpoint_dir_in(udc_ep->desc)
				? DMA_TO_DEVICE : DMA_FROM_DEVICE);
		}
#define DMA_ADDR_INVALID    (~(dma_addr_t)0)
		udc_req->usb_req.dma = DMA_ADDR_INVALID;
		udc_req->mapped = 0;
	}

	if (udc_req->usb_req.complete) {
		spin_unlock_irqrestore(&crg_udc->udc_lock, flags);
		udc_req->usb_req.complete(&udc_ep->usb_ep, &udc_req->usb_req);
		spin_lock_irqsave(&crg_udc->udc_lock, flags);
	}
}

static void nuke(struct crg_udc_ep *udc_ep, int status)
{
	struct crg_udc_request *req = NULL;

	while (!list_empty(&udc_ep->queue)) {
		req = list_entry(udc_ep->queue.next,
				struct crg_udc_request,
				queue);

		req_done(udc_ep, req, status);
	}
}

void clear_req_container(struct crg_udc_request *udc_req_ptr)
{
	udc_req_ptr->buff_len_left = 0;
	udc_req_ptr->trbs_needed = 0;
	udc_req_ptr->all_trbs_queued = 0;
	udc_req_ptr->first_trb = NULL;
	udc_req_ptr->last_trb = NULL;
	udc_req_ptr->short_pkt = 0;
}

bool is_pointer_less_than(struct transfer_trb_s *a,
	struct transfer_trb_s *b, struct crg_udc_ep *udc_ep)
{
	if (b > a && (udc_ep->enq_pt >= b || udc_ep->enq_pt < a))
		return true;
	if (b < a && (udc_ep->enq_pt >= b && udc_ep->enq_pt < a))
		return true;
	return false;
}

/* num_trbs here is the size of the ring. */
u32 room_on_ring(struct crg_gadget_dev *crg_udc, u32 num_trbs,
		struct transfer_trb_s *p_ring, struct transfer_trb_s *enq_pt,
		struct transfer_trb_s *dq_pt)
{
	u32 i = 0;

	/* CRG_DEBUG("room_on_ring enq_pt = 0x%p, dq_pt = 0x%p", enq_pt, dq_pt); */
	if (enq_pt == dq_pt) {
		/* ring is empty */
		return num_trbs - 1;
	}

	while (enq_pt != dq_pt) {
		i++;

		enq_pt++;

		if (GETF(TRB_TYPE, enq_pt->dw3) == TRB_TYPE_LINK)
			enq_pt = p_ring;

		if (i > num_trbs)
			break;
	}

	/* CRG_DEBUG("room_on_ring 0x%x\n", i); */
	return i - 1;
}

static void crg_udc_epcx_setup(struct crg_udc_ep *udc_ep)
{
	struct crg_gadget_dev *crg_udc = udc_ep->crg_udc;
	const struct usb_endpoint_descriptor *desc = udc_ep->desc;
	const struct usb_ss_ep_comp_descriptor *comp_desc = udc_ep->comp_desc;
	u8 DCI = udc_ep->DCI;
	struct ep_cx_s *epcx = (struct ep_cx_s *)(crg_udc->p_epcx + DCI - 2);
	enum EP_TYPE_E ep_type;
	u16 maxburst = 0;
	u8 maxstreams = 0;
	u16 maxsize;
	u32 dw;

	CRG_DEBUG("crgudc->p_epcx %p, epcx %p\n", crg_udc->p_epcx, epcx);
	CRG_DEBUG("DCI %d, sizeof ep_cx %ld\n", DCI, sizeof(struct ep_cx_s));
	CRG_DEBUG("desc epaddr = 0x%x\n", desc->bEndpointAddress);

	/*corigine gadget dir should be opposite to host dir*/
	if (usb_endpoint_dir_out(desc))
		ep_type = usb_endpoint_type(desc) + EP_TYPE_INVALID2;
	else
		ep_type = usb_endpoint_type(desc);

	maxsize = usb_endpoint_maxp(desc) & 0x07ff; /* D[0:10] */

	if (crg_udc->gadget.speed >= USB_SPEED_SUPER) {
		maxburst = comp_desc->bMaxBurst;

		if (usb_endpoint_xfer_bulk(udc_ep->desc))
			maxstreams = comp_desc->bmAttributes & 0x1f;

	} else if ((crg_udc->gadget.speed == USB_SPEED_HIGH ||
		crg_udc->gadget.speed == USB_SPEED_FULL) &&
			(usb_endpoint_xfer_int(udc_ep->desc) ||
				usb_endpoint_xfer_isoc(udc_ep->desc))) {
		if (crg_udc->gadget.speed == USB_SPEED_HIGH)
			maxburst = (usb_endpoint_maxp(desc) >> 11) & 0x3;
		if (maxburst == 0x3) {
			CRG_ERROR("invalid maxburst\n");
			maxburst = 0x2; /* really need ? */
		}
	}

	/* fill ep_dw0 */
	dw = 0;
	dw = SETF_VAR(EP_CX_LOGICAL_EP_NUM, dw, udc_ep->DCI / 2);
	dw = SETF_VAR(EP_CX_INTERVAL, dw, desc->bInterval);
	/*SETF_VAR(EP_CX_MULT, dw, mult);*/
	if (maxstreams) {
		CRG_ERROR("%s maxstream=%d is not expected\n",
			__func__, maxstreams);
		/*SETF_VAR(EP_CX_MAX_PSTREAMS, dw, maxstreams);*/
		/*SETF_VAR(EP_LINEAR_STRM_ARR, dw, 1);*/
	}
	epcx->dw0 = cpu_to_le32(dw);

	/* fill ep_dw1 */
	dw = 0;
	dw = SETF_VAR(EP_CX_EP_TYPE, dw, ep_type);
	dw = SETF_VAR(EP_CX_MAX_PACKET_SIZE, dw, maxsize);
	dw = SETF_VAR(EP_CX_MAX_BURST_SIZE, dw, maxburst);
	epcx->dw1 = cpu_to_le32(dw);

	/* fill ep_dw2 */
	dw = lower_32_bits(udc_ep->tran_ring_info.dma) & EP_CX_TR_DQPT_LO_MASK;
	dw = SETF_VAR(EP_CX_DEQ_CYC_STATE, dw, udc_ep->pcs);
	epcx->dw2 = cpu_to_le32(dw);

	/* fill ep_dw3 */
	dw = upper_32_bits(udc_ep->tran_ring_info.dma);
	epcx->dw3 = cpu_to_le32(dw);
	/* wmb */
	wmb();
}

static void crg_udc_epcx_update_dqptr(struct crg_udc_ep *udc_ep)
{
	struct crg_gadget_dev *crg_udc = udc_ep->crg_udc;
	u8 DCI = udc_ep->DCI;
	struct ep_cx_s *epcx = (struct ep_cx_s *)(crg_udc->p_epcx + DCI - 2);
	u32 dw;
	dma_addr_t dqptaddr;
	u32 cmd_param0;

	if (DCI == 0) {
		CRG_ERROR("%s Cannot update dqptr for ep0\n", __func__);
		return;
	}

	dqptaddr = tran_trb_virt_to_dma(udc_ep, udc_ep->deq_pt);

	/* fill ep_dw2 */
	dw = lower_32_bits(dqptaddr) & EP_CX_TR_DQPT_LO_MASK;
	dw = SETF_VAR(EP_CX_DEQ_CYC_STATE, dw, udc_ep->pcs);
	epcx->dw2 = cpu_to_le32(dw);

	/* fill ep_dw3 */
	dw = upper_32_bits(dqptaddr);
	epcx->dw3 = cpu_to_le32(dw);

	cmd_param0 = (0x1 << udc_ep->DCI);
	/* wmb */
	wmb();

	crg_issue_command(crg_udc, CRG_CMD_SET_TR_DQPTR, cmd_param0, 0);
}

void setup_status_trb(struct crg_gadget_dev *crg_udc,
		struct transfer_trb_s *p_trb,
		struct usb_request *usb_req, u8 pcs, u8 set_addr, u8 stall)
{
	u32 tmp, dir = 0;

	/* There are some cases where setup_status_trb() is called with
	 * usb_req set to NULL.
	 */

	p_trb->dw0 = 0;
	p_trb->dw1 = 0;

	CRG_DEBUG("data_buf_ptr_lo = 0x%x, data_buf_ptr_hi = 0x%x\n",
		p_trb->dw0, p_trb->dw1);

	tmp = 0;
	tmp = SETF_VAR(TRB_INTR_TARGET, tmp, 0);
	p_trb->dw2 = tmp;

	tmp = 0;
	tmp = SETF_VAR(TRB_CYCLE_BIT, tmp, pcs);
	tmp = SETF_VAR(TRB_INTR_ON_COMPLETION, tmp, 1);/*IOC:1*/
	tmp = SETF_VAR(TRB_TYPE, tmp, TRB_TYPE_XFER_STATUS_STAGE);

	dir = (crg_udc->setup_status == STATUS_STAGE_XFER) ? 0 : 1;
	tmp = SETF_VAR(DATA_STAGE_TRB_DIR, tmp, dir);

	tmp = SETF_VAR(TRB_SETUP_TAG, tmp, crg_udc->setup_tag);
	tmp = SETF_VAR(STATUS_STAGE_TRB_STALL, tmp, stall);
	tmp = SETF_VAR(STATUS_STAGE_TRB_SET_ADDR, tmp, set_addr);

	p_trb->dw3 = tmp;
	CRG_DEBUG("trb_dword2 = 0x%x, trb_dword3 = 0x%x\n",
			p_trb->dw2, p_trb->dw3);
	/* wmb */
	wmb();
}

void knock_doorbell(struct crg_gadget_dev *crg_udc, int DCI)
{
	u32 tmp;
	struct crg_uccr *uccr;

	uccr = crg_udc->uccr;
	/* wmb */
	wmb();

	tmp = CRG_U3DC_DB_TARGET(DCI);
	CRG_DEBUG("DOORBELL = 0x%x\n", tmp);
	reg_write(&uccr->doorbell, tmp);
}

void setup_datastage_trb(struct crg_gadget_dev *crg_udc,
		struct transfer_trb_s *p_trb, struct usb_request *usb_req,
		u8 pcs, u32 num_trb, u32 transfer_length, u32 td_size,
		u8 IOC, u8 AZP, u8 dir, u8 setup_tag)
{
	u32 tmp;

	CRG_DEBUG("dma = 0x%x, ", usb_req->dma);
	CRG_DEBUG("buf = 0x%x, ", usb_req->buf);

	p_trb->dw0 = lower_32_bits(usb_req->dma);
	p_trb->dw1 = upper_32_bits(usb_req->dma);

	CRG_DEBUG("data_buf_ptr_lo = 0x%x, data_buf_ptr_hi = 0x%x\n",
		p_trb->dw0, p_trb->dw1);

	//p_trb->dw0 = usb_req->buf;
	//p_trb->dw1 = 0;
	//CRG_DEBUG("data_buf_ptr_lo = 0x%x, data_buf_ptr_hi = 0x%x\n",
	//	p_trb->dw0, p_trb->dw1);

	/* TRB_Transfer_Length
	 *For USB_DIR_OUT, this field is the number of data bytes expected from
	 *xhc. For USB_DIR_IN, this field is the number of data bytes the device
	 *will send.
	 */
	tmp = 0;
	tmp = SETF_VAR(TRB_TRANSFER_LEN, tmp, transfer_length);
	tmp = SETF_VAR(TRB_TD_SIZE, tmp, td_size);
	tmp = SETF_VAR(TRB_INTR_TARGET, tmp, 0);
	p_trb->dw2 = tmp;

	tmp = 0;
	tmp = SETF_VAR(TRB_CYCLE_BIT, tmp, pcs);
	tmp = SETF_VAR(TRB_INTR_ON_SHORT_PKT, tmp, 1);
	tmp = SETF_VAR(TRB_INTR_ON_COMPLETION, tmp, IOC);
	tmp = SETF_VAR(TRB_TYPE, tmp, TRB_TYPE_XFER_DATA_STAGE);
	tmp = SETF_VAR(TRB_APPEND_ZLP, tmp, AZP);
	tmp = SETF_VAR(DATA_STAGE_TRB_DIR, tmp, dir);
	tmp = SETF_VAR(TRB_SETUP_TAG, tmp, setup_tag);

	p_trb->dw3 = tmp;
	/* wmb */
	wmb();

	CRG_DEBUG("trb_dword0 = 0x%x, trb_dword1 = 0x%x trb_dword2 = 0x%x, trb_dword3 = 0x%x\n",
			p_trb->dw0, p_trb->dw1, p_trb->dw2, p_trb->dw3);
}

void setup_trb(struct crg_gadget_dev *crg_udc,
		struct transfer_trb_s *p_trb,
		struct usb_request *usb_req, u32 xfer_len,
		dma_addr_t xfer_buf_addr, u8 td_size, u8 pcs,
		u8 trb_type, u8 short_pkt, u8 chain_bit,
		u8 intr_on_compl, bool b_setup_stage, u8 usb_dir,
		bool b_isoc, u8 tlb_pc, u16 frame_i_d, u8 SIA, u8 AZP)
{
	u32 tmp;

	p_trb->dw0 = lower_32_bits(xfer_buf_addr);
	p_trb->dw1 = upper_32_bits(xfer_buf_addr);

	CRG_DEBUG("data_buf_ptr_lo = 0x%x, data_buf_ptr_hi = 0x%x\n",
		p_trb->dw0, p_trb->dw1);

	tmp = 0;
	tmp = SETF_VAR(TRB_TRANSFER_LEN, tmp, xfer_len);
	tmp = SETF_VAR(TRB_TD_SIZE, tmp, td_size);
	tmp = SETF_VAR(TRB_INTR_TARGET, tmp, 0);

	p_trb->dw2 = tmp;

	tmp = 0;
	tmp = SETF_VAR(TRB_CYCLE_BIT, tmp, pcs);
	tmp = SETF_VAR(TRB_INTR_ON_SHORT_PKT, tmp, short_pkt);
	tmp = SETF_VAR(TRB_CHAIN_BIT, tmp, chain_bit);
	tmp = SETF_VAR(TRB_INTR_ON_COMPLETION, tmp, intr_on_compl);
	tmp = SETF_VAR(TRB_APPEND_ZLP, tmp, AZP);
	tmp = SETF_VAR(TRB_TYPE, tmp, trb_type);

	if (b_setup_stage)
		tmp = SETF_VAR(DATA_STAGE_TRB_DIR, tmp, usb_dir);

	if (b_isoc) {
		/*XHCI_SETF_VAR(ISOC_TRB_TLBPC, tmp, tlb_pc);*/
		tmp = SETF_VAR(ISOC_TRB_FRAME_ID, tmp, frame_i_d);
		tmp = SETF_VAR(ISOC_TRB_SIA, tmp, SIA);
	}

	p_trb->dw3 = tmp;
	/* wmb */
	wmb();
	CRG_DEBUG("trb_dword2 = 0x%.8x, trb_dword3 = 0x%.8x\n",
		p_trb->dw2, p_trb->dw3);
}

int crg_udc_queue_trbs(struct crg_udc_ep *udc_ep_ptr,
		struct crg_udc_request *udc_req_ptr,  bool b_isoc,
		u32 xfer_ring_size,
		u32 num_trbs_needed, u64 buffer_length)
{
	struct crg_gadget_dev *crg_udc = udc_ep_ptr->crg_udc;
	struct transfer_trb_s *p_xfer_ring = udc_ep_ptr->first_trb;
	u32 num_trbs_ava = 0;
	struct usb_request *usb_req = &udc_req_ptr->usb_req;
	u64 buff_len_temp = 0;
	u32 i, j = 1;
	struct transfer_trb_s *enq_pt = udc_ep_ptr->enq_pt;
	u8 td_size;
	u8 chain_bit = 1;
	u8 short_pkt = 0;
	u8 intr_on_compl = 0;
	u32 count;
	bool full_td = true;
	u32 intr_rate;
	dma_addr_t trb_buf_addr;
	bool need_zlp = false;

	/*CRG_DEBUG("crg_udc_queue_trbs\n");*/

	if (!b_isoc) {
		if (udc_req_ptr->usb_req.zero == 1 &&
			udc_req_ptr->usb_req.length != 0 &&
			((udc_req_ptr->usb_req.length %
			  udc_ep_ptr->usb_ep.maxpacket) == 0)) {
			need_zlp = true;
		}
	}

	td_size = num_trbs_needed;

	num_trbs_ava = room_on_ring(crg_udc, xfer_ring_size,
		p_xfer_ring, udc_ep_ptr->enq_pt, udc_ep_ptr->deq_pt);

	/* trb_buf_addr points to the addr of the buffer that we write in
	 * each TRB. If this function is called to complete the pending TRB
	 * transfers of a previous request, point it to the buffer that is
	 * not transferred, or else point it to the starting address of the
	 * buffer received in usb_request
	 */
	if (udc_req_ptr->trbs_needed) {
		/* Here udc_req_ptr->trbs_needed is used to indicate if we
		 * are completing a previous req
		 */
		trb_buf_addr = usb_req->dma +
			(usb_req->length - udc_req_ptr->buff_len_left);
	} else {
		trb_buf_addr = usb_req->dma;
	}

	if (num_trbs_ava >= num_trbs_needed) {
		count = num_trbs_needed;
	} else {
		if (b_isoc) {
			struct crg_udc_request *udc_req_ptr_temp;
			u8 temp = 0;

			list_for_each_entry(udc_req_ptr_temp,
					&udc_ep_ptr->queue, queue) {
				temp++;
			}

			if (temp >= 2) {
				CRG_ERROR("%s don't do isoc discard\n", __func__);
				/*  we already scheduled two mfi in advance. */
				return 0;
			}
		}

		/* always keep one trb for zlp. */
		count = num_trbs_ava;
		full_td = false;
		xdebug("TRB Ring Full. Avail: 0x%x Req: 0x%x\n",
				num_trbs_ava, num_trbs_needed);
		udc_ep_ptr->tran_ring_full = true;

		/*xyl: if there is still some trb not queued,
		 *it means last queued
		 *trb is not the last trb of TD, so no need zlp
		 */
		need_zlp = false;
	}

	for (i = 0; i < count; i++) {
		if (buffer_length > TRB_MAX_BUFFER_SIZE)
			buff_len_temp = TRB_MAX_BUFFER_SIZE;
		else
			buff_len_temp = buffer_length;

		buffer_length -= buff_len_temp;

		if (usb_endpoint_dir_out(udc_ep_ptr->desc))
			short_pkt = 1;

		if (buffer_length == 0) {
			chain_bit = 0;
			intr_on_compl = 1;
			udc_req_ptr->all_trbs_queued = 1;
		}

#define BULK_EP_INTERRUPT_RATE      5
#define ISOC_EP_INTERRUPT_RATE      1
		if  (b_isoc)
			intr_rate = ISOC_EP_INTERRUPT_RATE;
		else
			intr_rate = BULK_EP_INTERRUPT_RATE;

		if  (!full_td && j == intr_rate) {
			intr_on_compl = 1;
			j = 0;
		}

		if (b_isoc) {
			setup_trb(crg_udc, enq_pt, usb_req, buff_len_temp,
				trb_buf_addr, td_size - 1, udc_ep_ptr->pcs,
				TRB_TYPE_XFER_DATA_ISOCH, short_pkt, chain_bit,
				intr_on_compl, 0, 0, 1, 0, 0, 1, 0);
		} else {
			u8 pcs = udc_ep_ptr->pcs;

			if (udc_ep_ptr->comp_desc &&
				(usb_ss_max_streams(udc_ep_ptr->comp_desc))) {
				CRG_ERROR("%s don't do bulk stream\n", __func__);
			} else {
				if (udc_req_ptr->all_trbs_queued) {
					/*it is the last trb of TD,
					 * so consider zlp
					 */
					u8 AZP = 0;

					AZP = (need_zlp ? 1 : 0);

					setup_trb(crg_udc, enq_pt, usb_req,
						buff_len_temp, trb_buf_addr,
						td_size - 1, pcs,
						TRB_TYPE_XFER_NORMAL, short_pkt,
						chain_bit, intr_on_compl,
						0, 0, 0, 0, 0, 0, AZP);

				} else {
					setup_trb(crg_udc, enq_pt, usb_req,
						buff_len_temp, trb_buf_addr,
						td_size - 1, pcs,
						TRB_TYPE_XFER_NORMAL, short_pkt,
						chain_bit, intr_on_compl,
						0, 0, 0, 0, 0, 0, 0);
				}
			}
		}
		trb_buf_addr += buff_len_temp;
		td_size--;
		enq_pt++;
		j++;
		if (GETF(TRB_TYPE, enq_pt->dw3) == TRB_TYPE_LINK) {
			if (GETF(TRB_LINK_TOGGLE_CYCLE,
					enq_pt->dw3)) {
				enq_pt->dw3 = SETF_VAR(TRB_CYCLE_BIT,
					enq_pt->dw3, udc_ep_ptr->pcs);
				udc_ep_ptr->pcs ^= 0x1;
				/* wmb */
				wmb();
				enq_pt = udc_ep_ptr->first_trb;
			}
		}
	}

	if (!udc_req_ptr->trbs_needed)
		udc_req_ptr->first_trb = udc_ep_ptr->enq_pt;
	udc_ep_ptr->enq_pt = enq_pt;
	udc_req_ptr->buff_len_left = buffer_length;
	udc_req_ptr->trbs_needed = td_size;

	if (udc_req_ptr->buff_len_left == 0) {
		/* It is actually last trb of a request plus 1 */
		if (udc_ep_ptr->enq_pt == udc_ep_ptr->first_trb)
			udc_req_ptr->last_trb = udc_ep_ptr->last_trb - 1;
		else
			udc_req_ptr->last_trb = udc_ep_ptr->enq_pt - 1;
	}

	return 0;
}

int crg_udc_queue_ctrl(struct crg_udc_ep *udc_ep_ptr,
		struct crg_udc_request *udc_req_ptr, u32 num_of_trbs_needed)
{
	struct crg_gadget_dev *crg_udc = udc_ep_ptr->crg_udc;
	u8 ep_state;
	struct transfer_trb_s *enq_pt = udc_ep_ptr->enq_pt;
	struct transfer_trb_s *dq_pt = udc_ep_ptr->deq_pt;
	struct usb_request *usb_req = &udc_req_ptr->usb_req;
	struct transfer_trb_s *p_trb;
	u32 transfer_length;
	u32 td_size = 0;
	u8 IOC;
	u8 AZP;
	u8 dir = 0;
	u8 setup_tag = crg_udc->setup_tag;

	ep_state = get_ep_state(crg_udc, 0);

	/*CRG_DEBUG("num_of_trbs_needed = 0x%x\n", num_of_trbs_needed);*/

	/* Need to queue the request even ep is paused or halted */
	if (ep_state != EP_STATE_RUNNING) {
		CRG_DEBUG("EP State = 0x%x\n", ep_state);
		return -EINVAL;
	}

	if (list_empty(&udc_ep_ptr->queue)) {
		/* For control endpoint, we can handle one setup request at a
		 * time. so if there are TD pending in the transfer ring.
		 * wait for the sequence number error event. Then put the new
		 * request to transfer ring
		 */
		if (enq_pt == dq_pt) {
			u32 tmp = 0, i;
			bool need_zlp = false;

			CRG_DEBUG("Setup Data Stage TRBs\n");
			/* Transfer ring is empty
			 * setup data stage TRBs
			 */
			udc_req_ptr->first_trb = udc_ep_ptr->enq_pt;

			if (crg_udc->setup_status ==  DATA_STAGE_XFER)
				dir = 0;
			else if (crg_udc->setup_status == DATA_STAGE_RECV)
				dir = 1;
			else
				CRG_DEBUG("unexpected setup_status!%d\n",
					crg_udc->setup_status);

			if (udc_req_ptr->usb_req.zero == 1 &&
				udc_req_ptr->usb_req.length != 0 &&
				((udc_req_ptr->usb_req.length %
				  udc_ep_ptr->usb_ep.maxpacket) == 0))
				need_zlp = true;/*zlp = zero length packet*/

			/*CRG_DEBUG("dir=%d, enq_pt=0x%p\n", dir, enq_pt);*/

			for (i = 0; i < num_of_trbs_needed; i++) {
				p_trb = enq_pt;
				if (i < (num_of_trbs_needed - 1)) {
					transfer_length = TRB_MAX_BUFFER_SIZE;
					IOC = 0;
					AZP = 0;
				} else {
					tmp = TRB_MAX_BUFFER_SIZE * i;
					transfer_length = (u32)usb_req->length
						- tmp;

					IOC = 1;
					AZP = (need_zlp ? 1 : 0);
				}

				CRG_DEBUG("tx_len = 0x%x, tmp = 0x%x\n",
					transfer_length, tmp);

				setup_datastage_trb(crg_udc, p_trb, usb_req,
					udc_ep_ptr->pcs, i, transfer_length,
					td_size, IOC, AZP, dir, setup_tag);
				udc_req_ptr->all_trbs_queued = 1;
				enq_pt++;

				if (GETF(TRB_TYPE, enq_pt->dw3) ==
						TRB_TYPE_LINK) {
					if (GETF(TRB_LINK_TOGGLE_CYCLE,
							enq_pt->dw3)) {
						enq_pt->dw3 = SETF_VAR(TRB_CYCLE_BIT,
							enq_pt->dw3,
							udc_ep_ptr->pcs);
						udc_ep_ptr->pcs ^= 0x1;
					}
					/* wmb */
					wmb();
					enq_pt = udc_ep_ptr->first_trb;
				}
			}

			udc_ep_ptr->enq_pt = enq_pt;

			tmp = 0;

			knock_doorbell(crg_udc, 0);

			if (udc_ep_ptr->enq_pt == udc_ep_ptr->first_trb)
				udc_req_ptr->last_trb =
					udc_ep_ptr->last_trb - 1;
			else
				udc_req_ptr->last_trb = udc_ep_ptr->enq_pt - 1;
		} else {
			/* we process one setup request at a time, so ring
			 * should already be empty.
			 */
			CRG_ERROR("Eq = 0x%p != Dq = 0x%p\n", enq_pt, dq_pt);
			/* Assert() */
		}
	} else {
		CRG_ERROR("udc_ep_ptr->queue not empty\n");
		/* New setup packet came
		 * Drop the this req..
		 */
		return -EINVAL;
	}

	return 0;
}

void build_ep0_status(struct crg_udc_ep *udc_ep_ptr,
		 bool default_value, u32 status,
		 struct crg_udc_request *udc_req_ptr, u8 set_addr, u8 stall)
{
	struct crg_gadget_dev *crg_udc = udc_ep_ptr->crg_udc;
	struct transfer_trb_s *enq_pt = udc_ep_ptr->enq_pt;

	if (default_value) {
		udc_req_ptr = crg_udc->status_req;
		udc_req_ptr->usb_req.length = 0;
		udc_req_ptr->usb_req.status = status;
		udc_req_ptr->usb_req.actual = 0;
		udc_req_ptr->usb_req.complete = NULL;
	} else {
		udc_req_ptr->usb_req.status = status;
		udc_req_ptr->usb_req.actual = 0;
	}

	setup_status_trb(crg_udc, enq_pt, &udc_req_ptr->usb_req,
			udc_ep_ptr->pcs, set_addr, stall);

	enq_pt++;

	/* check if we are at end of trb segment.  If so, update
	 * pcs and enq for next segment
	 */
	if (GETF(TRB_TYPE, enq_pt->dw3) == TRB_TYPE_LINK) {
		if (GETF(TRB_LINK_TOGGLE_CYCLE, enq_pt->dw3)) {
			enq_pt->dw3 = SETF_VAR
				(TRB_CYCLE_BIT, enq_pt->dw3, udc_ep_ptr->pcs);
			udc_ep_ptr->pcs ^= 0x1;
		}
		enq_pt = udc_ep_ptr->first_trb;
	}
	udc_ep_ptr->enq_pt = enq_pt;

	knock_doorbell(crg_udc, 0);

	list_add_tail(&udc_req_ptr->queue, &udc_ep_ptr->queue);
}

void ep0_req_complete(struct crg_udc_ep *udc_ep_ptr)
{
	struct crg_gadget_dev *crg_udc = udc_ep_ptr->crg_udc;

	switch (crg_udc->setup_status) {
	case DATA_STAGE_XFER:
		crg_udc->setup_status = STATUS_STAGE_RECV;
		build_ep0_status(udc_ep_ptr, true, -EINPROGRESS, NULL, 0, 0);
		break;
	case DATA_STAGE_RECV:
		crg_udc->setup_status = STATUS_STAGE_XFER;
		build_ep0_status(udc_ep_ptr, true, -EINPROGRESS, NULL, 0, 0);
		break;
	default:
		if (crg_udc->setup_fn_call_back)
			crg_udc->setup_fn_call_back(crg_udc);

		crg_udc->setup_status = WAIT_FOR_SETUP;
		break;
	}
}

void handle_cmpl_code_success(struct crg_gadget_dev *crg_udc,
		struct event_trb_s *event, struct crg_udc_ep *udc_ep_ptr)
{
	u64 trb_pt;
	struct transfer_trb_s *p_trb;
	struct crg_udc_request *udc_req_ptr;
	u32 trb_transfer_length;

	trb_pt = (u64)event->dw0 + ((u64)(event->dw1) << 32);
	p_trb = tran_trb_dma_to_virt(udc_ep_ptr, trb_pt);

	xdebug("trb_pt = 0x%lx, p_trb = 0x%p\n", (unsigned long)trb_pt, p_trb);
	xdebug("trb dw0 = 0x%x\n", p_trb->dw0);
	xdebug("trb dw1 = 0x%x\n", p_trb->dw1);
	xdebug("trb dw2 = 0x%x\n", p_trb->dw2);
	xdebug("trb dw3 = 0x%x\n", p_trb->dw3);

	if (!GETF(TRB_CHAIN_BIT, p_trb->dw3)) {
		/* chain bit is not set, which means it
		 * is the end of a TD
		 */
		udc_req_ptr = list_entry(udc_ep_ptr->queue.next,
					struct crg_udc_request, queue);

		CRG_DEBUG("udc_req_ptr = 0x%p\n", udc_req_ptr);

		trb_transfer_length = GETF(EVE_TRB_TRAN_LEN,
					event->dw2);
		udc_req_ptr->usb_req.actual = udc_req_ptr->usb_req.length -
					trb_transfer_length;
		CRG_DEBUG("Actual data xfer = 0x%x, tx_len = 0x%x\n",
			udc_req_ptr->usb_req.actual, trb_transfer_length);

		CRG_DEBUG("udc_req_ptr->usb_req.buf = 0x%p\n", udc_req_ptr->usb_req.buf);
		/* wmb */
		wmb();
		req_done(udc_ep_ptr, udc_req_ptr, 0);
		if (!udc_ep_ptr->desc) {
			CRG_DEBUG("udc_ep_ptr->desc is NULL\n");
		} else {
			if (usb_endpoint_xfer_control(udc_ep_ptr->desc))
				ep0_req_complete(udc_ep_ptr);
		}
	}
}

void update_dequeue_pt(struct event_trb_s *event,
	struct crg_udc_ep *udc_ep)
{
	u32 deq_pt_lo = event->dw0;
	u32 deq_pt_hi = event->dw1;
	u64 dq_pt_addr = (u64)deq_pt_lo + ((u64)deq_pt_hi << 32);
	struct transfer_trb_s *deq_pt;

	deq_pt = tran_trb_dma_to_virt(udc_ep, dq_pt_addr);
	deq_pt++;

	if (GETF(TRB_TYPE, deq_pt->dw3) == TRB_TYPE_LINK)
		deq_pt = udc_ep->first_trb;

	udc_ep->deq_pt = deq_pt;
}

void advance_dequeue_pt(struct crg_udc_ep *udc_ep)
{
	struct crg_udc_request *udc_req;

	if (!list_empty(&udc_ep->queue)) {
		xdebug("%s\n", __func__);
		udc_req = list_entry(udc_ep->queue.next,
				struct crg_udc_request,
				queue);

		if (udc_req->first_trb) {
			xdebug("%s first trb = 0x%p\n",
				__func__, udc_req->first_trb);

			udc_ep->deq_pt = udc_req->first_trb;
		} else {
			xdebug("%s enq_pt = 0x%p\n", __func__, udc_ep->enq_pt);
			udc_ep->deq_pt = udc_ep->enq_pt;
		}
	} else {
		xdebug("%s empty enq_pt = 0x%p\n", __func__, udc_ep->enq_pt);
		udc_ep->deq_pt = udc_ep->enq_pt;
	}
}

bool is_request_dequeued(struct crg_gadget_dev *crg_udc,
		struct crg_udc_ep *udc_ep, struct event_trb_s *event)
{
	struct crg_udc_request *udc_req;
	u32 trb_pt_lo = event->dw0;
	u32 trb_pt_hi = event->dw1;
	u64 trb_addr = (u64)trb_pt_lo + ((u64)trb_pt_hi << 32);
	struct transfer_trb_s *trb_pt;
	bool status = true;

	if (udc_ep->DCI == 0)
		return false;

	trb_pt = tran_trb_dma_to_virt(udc_ep, trb_addr);
	list_for_each_entry(udc_req, &udc_ep->queue, queue) {
		if (trb_pt == udc_req->last_trb ||
			trb_pt == udc_req->first_trb) {
			status = false;
			break;
		}

		if (is_pointer_less_than(trb_pt, udc_req->last_trb, udc_ep) &&
			is_pointer_less_than(udc_req->first_trb, trb_pt,
				udc_ep)) {
			status = false;
			break;
		}
	}

	return status;
}

int crg_udc_build_td(struct crg_udc_ep *udc_ep_ptr,
		struct crg_udc_request *udc_req_ptr)
{
	int status = 0;
	struct crg_gadget_dev *crg_udc = udc_ep_ptr->crg_udc;
	u32 num_trbs_needed;
	u64 buffer_length;
	u32 tmp;

	xdebug("udc_req buf = 0x%p\n", udc_req_ptr->usb_req.buf);

	if (udc_req_ptr->trbs_needed) {
		/* If this is called to complete pending TRB transfers
		 * of previous Request
		 */
		buffer_length = udc_req_ptr->buff_len_left;
		num_trbs_needed = udc_req_ptr->trbs_needed;
	} else {
		buffer_length = (u64)udc_req_ptr->usb_req.length;
		num_trbs_needed = (u32)(buffer_length / TRB_MAX_BUFFER_SIZE);

		if (buffer_length == 0 ||
			(buffer_length % TRB_MAX_BUFFER_SIZE))
			num_trbs_needed += 1;
	}

	CRG_DEBUG("buf_len = %ld, num_trb_needed = %d\n",
		(unsigned long)buffer_length, num_trbs_needed);

	if (usb_endpoint_xfer_control(udc_ep_ptr->desc)) {
		CRG_DEBUG("crg_udc_queue_trbs control\n");
		status = crg_udc_queue_ctrl(udc_ep_ptr,
				 udc_req_ptr, num_trbs_needed);
	} else if (usb_endpoint_xfer_isoc(udc_ep_ptr->desc)) {
		CRG_DEBUG("crg_udc_queue_trbs isoc\n");
		status = crg_udc_queue_trbs(udc_ep_ptr, udc_req_ptr, 1,
				CRGUDC_ISOC_EP_TD_RING_SIZE,
				num_trbs_needed, buffer_length);

		tmp = udc_ep_ptr->DCI;
		tmp = CRG_U3DC_DB_TARGET(tmp);
		CRG_DEBUG("DOORBELL = 0x%x\n", tmp);

		knock_doorbell(crg_udc, udc_ep_ptr->DCI);
	} else if (usb_endpoint_xfer_bulk(udc_ep_ptr->desc)) {
		CRG_DEBUG("crg_udc_queue_trbs bulk\n");
		status = crg_udc_queue_trbs(udc_ep_ptr, udc_req_ptr, 0,
				CRGUDC_BULK_EP_TD_RING_SIZE,
				num_trbs_needed, buffer_length);
		tmp = udc_ep_ptr->DCI;
		tmp = CRG_U3DC_DB_TARGET(tmp);
		if (udc_ep_ptr->comp_desc &&
				usb_ss_max_streams(udc_ep_ptr->comp_desc)) {
			/* hold the doorbell if stream_rejected is set */
			CRG_ERROR("%s, WANT TO have bulk stream\n", __func__);
		}

		knock_doorbell(crg_udc, udc_ep_ptr->DCI);
	} else {
		/* CRG_DEBUG("crg_udc_queue_trbs int\n"); */
		/* interrupt endpoint */
		status = crg_udc_queue_trbs(udc_ep_ptr, udc_req_ptr, 0,
				CRGUDC_INT_EP_TD_RING_SIZE,
				num_trbs_needed, buffer_length);
		tmp = udc_ep_ptr->DCI;
		tmp = CRG_U3DC_DB_TARGET(tmp);

		knock_doorbell(crg_udc, udc_ep_ptr->DCI);
	}

	return status;
}

/* This function will go through the list of the USB requests for the
 * given endpoint and schedule any unscheduled trb's to the xfer ring
 */
void queue_pending_trbs(struct crg_udc_ep *udc_ep_ptr)
{
	struct crg_udc_request *udc_req_ptr;
	/* schedule  trbs till there arent any pending unscheduled ones
	 * or the ring is full again
	 */

	CRG_DEBUG("%s 1\n", __func__);
	list_for_each_entry(udc_req_ptr, &udc_ep_ptr->queue, queue) {
		if (udc_req_ptr->all_trbs_queued == 0)
			crg_udc_build_td(udc_ep_ptr, udc_req_ptr);

		if (udc_ep_ptr->tran_ring_full)
			break;
	}

	CRG_DEBUG("%s 2\n", __func__);
}

void squeeze_xfer_ring(struct crg_udc_ep *udc_ep_ptr,
		struct crg_udc_request *udc_req_ptr)
{
	struct transfer_trb_s *temp = udc_req_ptr->first_trb;
	struct crg_udc_request *next_req;

	/* All the incompleted Requests are recorded in crg_udc_ep.queue by SW*/
	/* 1. Clear all the queued-in-ring trbs from the deleted point */
	/* 2. Re-queue in ring the Requests that are after the deleted Request*/
	while (temp != udc_ep_ptr->enq_pt) {
		temp->dw0 = 0;
		temp->dw1 = 0;
		temp->dw2 = 0;
		temp->dw3 = 0;

		temp++;

		if (GETF(TRB_TYPE, temp->dw3) == TRB_TYPE_LINK)
			temp = udc_ep_ptr->first_trb;
	}

	/* Update the new enq_ptr starting from the deleted req */
	udc_ep_ptr->enq_pt = udc_req_ptr->first_trb;

	if (udc_ep_ptr->tran_ring_full)
		udc_ep_ptr->tran_ring_full = false;

	next_req = list_entry(udc_req_ptr->queue.next,
				struct crg_udc_request, queue);

	list_for_each_entry_from(next_req, &udc_ep_ptr->queue, queue) {
		next_req->usb_req.status = -EINPROGRESS;
		next_req->usb_req.actual = 0;

		/* clear the values of the nv_udc_request container **/
		clear_req_container(next_req);

		if (udc_ep_ptr->tran_ring_full)
			break;

		/* push the request to the transfer ring */
		crg_udc_build_td(udc_ep_ptr, next_req);
	}
}

static int set_ep0_halt(struct crg_gadget_dev *crg_udc)
{
	struct crg_udc_ep *udc_ep_ptr = &crg_udc->udc_ep[0];
	int ep_state;

	ep_state = get_ep_state(crg_udc, udc_ep_ptr->DCI);
	if (ep_state == EP_STATE_HALTED ||
		ep_state == EP_STATE_DISABLED) {
		return 0;
	}

	CRG_DEBUG("%s\n", __func__);
	build_ep0_status(udc_ep_ptr, true, -EINVAL, NULL, 0, 1);

	udc_ep_ptr->ep_state = EP_STATE_HALTED;

	return 0;
}

static int set_ep_halt(struct crg_gadget_dev *crg_udc, int DCI)
{
	struct crg_uccr *uccr = crg_udc->uccr;
	struct crg_udc_ep *udc_ep_ptr = &crg_udc->udc_ep[DCI];
	u32 param0;
	u32 tmp;

	CRG_DEBUG("%s DCI=%d  !!\n", __func__, DCI);

	if (DCI == 0)
		return 0;

	if (udc_ep_ptr->ep_state == EP_STATE_DISABLED ||
		udc_ep_ptr->ep_state == EP_STATE_HALTED)
		return 0;

	param0 = (0x1 << DCI);
	crg_issue_command(crg_udc, CRG_CMD_SET_HALT, param0, 0);
	do {
		tmp = reg_read(&uccr->ep_running);
	} while ((tmp & param0) != 0);

	/* clean up the request queue */
	nuke(udc_ep_ptr, -ECONNRESET);

	udc_ep_ptr->deq_pt = udc_ep_ptr->enq_pt;
	udc_ep_ptr->tran_ring_full = false;
	udc_ep_ptr->ep_state = EP_STATE_HALTED;

	return 0;
}

static int ep_halt(struct crg_udc_ep *udc_ep_ptr,
					int halt, int ignore_wedge)
{
	struct crg_gadget_dev *crg_udc = udc_ep_ptr->crg_udc;
	struct crg_uccr *uccr = crg_udc->uccr;
	int ep_state;
	bool reset_seq_only = false;
	int do_halt;
	u32 param0;
	u32 tmp;
	struct crg_udc_request *udc_req_ptr;

	if (!udc_ep_ptr->desc) {
		CRG_ERROR("NULL desc\n");
		return -EINVAL;
	}

	if (udc_ep_ptr->desc->bmAttributes == USB_ENDPOINT_XFER_ISOC) {
		CRG_ERROR("Isoc ep, halt not supported\n");
		return -EOPNOTSUPP;
	}

	if (udc_ep_ptr->DCI == 0) {
		//return set_ep0_halt(crg_udc);
		return 0;
	}

	ep_state = get_ep_state(crg_udc, udc_ep_ptr->DCI);

	if (ep_state == EP_STATE_DISABLED)
		return 0;

	if (ep_state == EP_STATE_HALTED) {
		if (halt != 0)
			return 0;

		/* want unhalt an halted ep*/
		if (udc_ep_ptr->wedge && !ignore_wedge)	{
			do_halt = -1;
			reset_seq_only = true;
		} else {
			do_halt = 0;
		}

	} else {
		/*ep state == running or stopped*/
		if (halt != 0) {
			/* want halt a running ep*/
			do_halt = 1;
		} else {
			/* reset a running ep*/
			do_halt = 0;
			reset_seq_only = true;
		}
	}

	param0 = (0x1 << udc_ep_ptr->DCI);
	if (do_halt == 1) {
		/* setting ep to halt */
		CRG_DEBUG("HALT EP DCI = %d\n", udc_ep_ptr->DCI);
		crg_issue_command(crg_udc, CRG_CMD_SET_HALT, param0, 0);
		do {
			tmp = reg_read(&uccr->ep_running);
		} while ((tmp & param0) != 0);

		if (crg_udc->gadget.speed >= USB_SPEED_SUPER) {
			/* clean up the request queue */
			nuke(udc_ep_ptr, -ECONNRESET);

			udc_ep_ptr->deq_pt = udc_ep_ptr->enq_pt;
			udc_ep_ptr->tran_ring_full = false;

			CRG_DEBUG("update deq_pt tp enq_pt 0x%x\n", udc_ep_ptr->deq_pt);
		}
		/* clean up the request queue */
		//nuke(udc_ep_ptr, -ECONNRESET); maybe we no need clean up the request queue

		//udc_ep_ptr->deq_pt = udc_ep_ptr->enq_pt;
		//udc_ep_ptr->tran_ring_full = false;
		udc_ep_ptr->ep_state = EP_STATE_HALTED;
	} else if (do_halt == 0) {
		/* clearing ep halt state */
		CRG_DEBUG("Clear EP HALT DCI = %d\n", udc_ep_ptr->DCI);
		/* reset sequence number */
		crg_issue_command(crg_udc, CRG_CMD_RESET_SEQNUM, param0, 0);

		if (!reset_seq_only) {
			/* Clear halt for a halted EP.*/
			/* NOTE: we must CLEAR_HALT first, then SET_TR_DQPTR*/
			crg_issue_command(crg_udc,
				CRG_CMD_CLEAR_HALT, param0, 0);
			//if (udc_ep_ptr->enq_pt == udc_ep_ptr->first_trb) {
			//}
			crg_udc_epcx_update_dqptr(udc_ep_ptr);

			CRG_DEBUG("crg_udc_epcx_update_dqptr , PCS = %d\n", udc_ep_ptr->pcs);
		}

		udc_ep_ptr->wedge = 0;
		udc_ep_ptr->ep_state = EP_STATE_RUNNING;
		/* set endpoint to running state */
		/* clear pause for the endpoint */
		if (!list_empty(&udc_ep_ptr->queue)) {
			tmp = udc_ep_ptr->DCI;
			tmp = CRG_U3DC_DB_TARGET(tmp);

			list_for_each_entry(udc_req_ptr, &udc_ep_ptr->queue, queue) {
				struct transfer_trb_s *tmp_ptr = udc_ep_ptr->deq_pt;

				tmp_ptr--;
				tmp_ptr = udc_ep_ptr->deq_pt + 1;
				if (GETF(TRB_TYPE, tmp_ptr->dw3) == TRB_TYPE_LINK) {
					udc_ep_ptr->pcs ^= 0x1;
					crg_udc_epcx_update_dqptr(udc_ep_ptr);
					udc_ep_ptr->pcs ^= 0x1;
				}
			}
			 /*reg_write(&uccr->doorbell, tmp);*/
			knock_doorbell(crg_udc, udc_ep_ptr->DCI);
		}
	} else {
		/* wedged EP deny CLEAR HALT */
		CRG_DEBUG("wedged EP deny CLEAR HALT DCI = %d\n", udc_ep_ptr->DCI);
		/* reset sequence number */
		if (reset_seq_only)
			crg_issue_command(crg_udc,
				CRG_CMD_RESET_SEQNUM, param0, 0);
	}

	return 0;
}

/************ep related ops*******************************/
static int crg_udc_ep_disable(struct usb_ep *ep)
{
	struct crg_udc_ep *udc_ep;
	struct crg_gadget_dev *crg_udc;
	struct ep_cx_s *p_ep_cx;
	int ep_state;
	struct crg_uccr *uccr;
	unsigned long flags = 0;

	CRG_DEBUG("%s\n", __func__);

	if (!ep)
		return -EINVAL;

	udc_ep = container_of(ep, struct crg_udc_ep, usb_ep);
	crg_udc = udc_ep->crg_udc;

	if (udc_ep->DCI == 0)
		return 0;

	spin_lock_irqsave(&crg_udc->udc_lock, flags);

	uccr = crg_udc->uccr;
	p_ep_cx = (struct ep_cx_s *)crg_udc->p_epcx + udc_ep->DCI - 2;

	ep_state = get_ep_state(crg_udc, udc_ep->DCI);
	if (ep_state == EP_STATE_DISABLED) {
		/* get here if ep is already disabled */
		spin_unlock_irqrestore(&crg_udc->udc_lock, flags);
		return -EINVAL;
	}

	CRG_DEBUG("EPDCI = 0x%x\n", udc_ep->DCI);

	/*Maybe we need to halt ep before ep disable*/
	/*ep_halt(udc_ep_ptr, 1);*/

	reg_write(&uccr->ep_enable, 0x1 << udc_ep->DCI);

	/* clean up the request queue */
	nuke(udc_ep, -ESHUTDOWN);

	/* decrement ep counters */
	crg_udc->num_enabled_eps--;

	/* release all the memory allocate for the endpoint
	 * dma_free_coherent(nvudc->dev, nvudc->event_ring0.buff_len,
	 * nvudc->event_ring0.virt_addr, nvudc->event_ring0.dma_addr);
	 */

	udc_ep->desc = NULL;

	/* clean up the endpoint context */
	memset(p_ep_cx, 0, sizeof(struct ep_cx_s));

	CRG_DEBUG("num_enabled_eps = %d\n", crg_udc->num_enabled_eps);

	/* If device state was changed to default by port
	 * reset, should not overwrite it again
	 */
	if (crg_udc->num_enabled_eps == 0 &&
		crg_udc->device_state == USB_STATE_CONFIGURED) {
		CRG_DEBUG("Device State USB_STATE_CONFIGURED\n");
		CRG_DEBUG("Set Device State to addressed\n");
		crg_udc->device_state = USB_STATE_ADDRESS;

		/*        power_gate(); */
	}

	udc_ep->ep_state = EP_STATE_DISABLED;

	spin_unlock_irqrestore(&crg_udc->udc_lock, flags);

	return 0;
}

static int crg_udc_ep_enable(struct usb_ep *ep,
		const struct usb_endpoint_descriptor *desc)
{
	struct crg_udc_ep *udc_ep;
	struct crg_gadget_dev *crg_udc;
	u32 param0;
	unsigned long flags = 0;
	struct ep_cx_s *epcx;
	struct crg_uccr *uccr;

	CRG_DEBUG("%s\n", __func__);

	if  (!ep || !desc || desc->bDescriptorType != USB_DT_ENDPOINT)
		return -EINVAL;

	udc_ep = container_of(ep, struct crg_udc_ep, usb_ep);

	CRG_DEBUG("%s DCI = %d\n", __func__, udc_ep->DCI);

	/*ep0 is always running*/
	if (udc_ep->DCI == 0)
		return 0;

	crg_udc = udc_ep->crg_udc;
	uccr = crg_udc->uccr;

	if (!crg_udc->gadget_driver)
		return -ESHUTDOWN;

	spin_lock_irqsave(&crg_udc->udc_lock, flags);

	/*crg ep context start from ep1*/
	if (get_ep_state(crg_udc, udc_ep->DCI) != EP_STATE_DISABLED) {
		CRG_DEBUG("%s disable first\n", __func__);
		spin_unlock_irqrestore(&crg_udc->udc_lock, flags);
		crg_udc_ep_disable(ep);
		spin_lock_irqsave(&crg_udc->udc_lock, flags);
	}

	udc_ep->desc = desc;
	udc_ep->comp_desc = ep->comp_desc;

	/* setup endpoint context for regular endpoint
	 * the endpoint context for control endpoint has been
	 * setted up in probe function
	 */
	if (udc_ep->DCI) {
		CRG_DEBUG("ep_enable udc_ep->DCI = %d\n", udc_ep->DCI);

		/* setup transfer ring */
		if (!udc_ep->tran_ring_info.vaddr) {
			dma_addr_t dma;
			u32 ring_size = 0;
			void *vaddr;
			size_t len;

			if (usb_endpoint_xfer_bulk(desc))
				ring_size =  CRGUDC_BULK_EP_TD_RING_SIZE;
			else if (usb_endpoint_xfer_isoc(desc))
				ring_size = CRGUDC_ISOC_EP_TD_RING_SIZE;
			else if (usb_endpoint_xfer_int(desc))
				ring_size = CRGUDC_INT_EP_TD_RING_SIZE;
			len = ring_size * sizeof(struct transfer_trb_s);
			spin_unlock_irqrestore(&crg_udc->udc_lock, flags);
			vaddr = dma_alloc_coherent(crg_udc->dev, len,
					&dma, GFP_ATOMIC);
			if (!vaddr) {
				CRG_ERROR("failed to allocate trb ring\n");
				return -ENOMEM;
			}
			spin_lock_irqsave(&crg_udc->udc_lock, flags);

			udc_ep->tran_ring_info.vaddr = vaddr;
			udc_ep->tran_ring_info.dma = dma;
			udc_ep->tran_ring_info.len = len;
			udc_ep->first_trb = vaddr;
			udc_ep->last_trb = udc_ep->first_trb + ring_size - 1;
		}
		memset(udc_ep->first_trb, 0, udc_ep->tran_ring_info.len);
		/* wmb */
		wmb();
		setup_link_trb(udc_ep->last_trb, true,
					udc_ep->tran_ring_info.dma);

		udc_ep->enq_pt = udc_ep->first_trb;
		udc_ep->deq_pt = udc_ep->first_trb;
		udc_ep->pcs = 1;
		udc_ep->tran_ring_full = false;
		crg_udc->num_enabled_eps++;
		crg_udc_epcx_setup(udc_ep);
	}

	CRG_DEBUG("num_enabled_eps = %d\n", crg_udc->num_enabled_eps);

	epcx = (struct ep_cx_s *)(crg_udc->p_epcx + udc_ep->DCI - 2);

	param0 = (0x1 << udc_ep->DCI);
	crg_issue_command(crg_udc, CRG_CMD_CONFIG_EP, param0, 0);

	CRG_DEBUG("config ep and start, DCI=%d\n", udc_ep->DCI);
	if (crg_udc->device_state == USB_STATE_ADDRESS)
		crg_udc->device_state = USB_STATE_CONFIGURED;

	udc_ep->wedge = 0;
	udc_ep->ep_state = EP_STATE_RUNNING;

	spin_unlock_irqrestore(&crg_udc->udc_lock, flags);

	return 0;
}

static struct usb_request *
crg_udc_alloc_request(struct usb_ep *_ep, gfp_t gfp_flags)
{
	struct crg_udc_request *udc_req_ptr = NULL;

	int i = 0;

	//udc_req_ptr = kzalloc(sizeof(*udc_req_ptr), gfp_flags);
	for (i = 0; i < CRE_REQ_NUM; i++) {
		if (atomic_xchg(&g_udc_req_ptr[i].used, 1) == 0) {
			udc_req_ptr = &g_udc_req_ptr[i];
			break;
		}
	}

	if (i >= CRE_REQ_NUM) {
		CRG_ERROR("fail to alloc g_udc_req_ptr\n");
		return NULL;
	}

	CRG_DEBUG("udc_req_ptr = 0x%p\n", udc_req_ptr);

	//if (!udc_req_ptr)
		//return NULL;

	udc_req_ptr->usb_req.dma = DMA_ADDR_INVALID;
	INIT_LIST_HEAD(&udc_req_ptr->queue);

	return &udc_req_ptr->usb_req;
}

static void crg_udc_free_request(struct usb_ep *_ep, struct usb_request *_req)
{
	struct crg_udc_request *udc_req_ptr = NULL;

	CRG_DEBUG("%s\n", __func__);
	if (!_ep || !_req)
		return;

	udc_req_ptr = container_of(_req, struct crg_udc_request, usb_req);
	WARN_ON(!atomic_xchg(&udc_req_ptr->used, 0));
	//kfree(udc_req_ptr);
}

static int
crg_udc_ep_queue(struct usb_ep *_ep, struct usb_request *_req,
				gfp_t gfp_flags)
{
	struct crg_udc_request *udc_req_ptr;
	struct crg_udc_ep *udc_ep_ptr;
	struct crg_gadget_dev *crg_udc;
	int status;
	unsigned long flags = 0;
	int dma_data_dir = 0;

	CRG_DEBUG("%s\n", __func__);

	if (!_req || !_ep)
		return -EINVAL;

	udc_req_ptr = container_of(_req, struct crg_udc_request, usb_req);
	if (!udc_req_ptr)
		return -EINVAL;

	udc_ep_ptr = container_of(_ep, struct crg_udc_ep, usb_ep);
	if (!udc_ep_ptr)
		return -EINVAL;

	crg_udc = udc_ep_ptr->crg_udc;
	if (!crg_udc)
		return -EINVAL;

	spin_lock_irqsave(&crg_udc->udc_lock, flags);

	if (!udc_ep_ptr->first_trb ||
		!udc_req_ptr->usb_req.complete ||
		!udc_req_ptr->usb_req.buf ||
		!list_empty(&udc_req_ptr->queue)) {
		CRG_DEBUG("%s, invalid usbrequest\n", __func__);
		if (!udc_ep_ptr->first_trb)
			CRG_DEBUG("%s, no first_trb\n", __func__);

		if (!udc_req_ptr->usb_req.complete)
			CRG_DEBUG("%s, no complete\n", __func__);

		if (!udc_req_ptr->usb_req.buf)
			CRG_DEBUG("%s, no req buf\n", __func__);

		if (!list_empty(&udc_req_ptr->queue))
			CRG_DEBUG("%s, list not empty\n", __func__);

		spin_unlock_irqrestore(&crg_udc->udc_lock, flags);
		return -EINVAL;
	}
	xdebug("enqueue EPDCI = 0x%x\n", udc_ep_ptr->DCI);
	xdebug("udc_req buf = 0x%p\n", udc_req_ptr->usb_req.buf);

	if (!udc_ep_ptr->desc) {
		CRG_DEBUG("udc_ep_ptr->Desc is null\n");
		spin_unlock_irqrestore(&crg_udc->udc_lock, flags);
		return -EINVAL;
	}
	/* wmb */
	wmb();
	/* Clearing the Values of the UDC_REQUEST container */
	clear_req_container(udc_req_ptr);
	udc_req_ptr->mapped = 0;

	if (usb_endpoint_xfer_control(udc_ep_ptr->desc) &&
				_req->length == 0) {
		crg_udc->setup_status = STATUS_STAGE_XFER;
		status = -EINPROGRESS;
		CRG_DEBUG("udc_req_ptr = 0x%p\n", udc_req_ptr);

		build_ep0_status(&crg_udc->udc_ep[0], false, status,
			udc_req_ptr, 0, 0);
		CRG_DEBUG("act status request for control endpoint\n");
		spin_unlock_irqrestore(&crg_udc->udc_lock, flags);
		return 0;
	}

	/* request length is possible to be 0. Like SCSI blank command */
	CRG_DEBUG("request length=%d\n", _req->length);

	if (udc_req_ptr->usb_req.dma == DMA_ADDR_INVALID && _req->length != 0) {
		if (usb_endpoint_xfer_control(udc_ep_ptr->desc)) {
			if (crg_udc->setup_status == DATA_STAGE_XFER ||
				crg_udc->setup_status == STATUS_STAGE_XFER)
				dma_data_dir = DMA_TO_DEVICE;

			if (crg_udc->setup_status == DATA_STAGE_RECV ||
				crg_udc->setup_status == STATUS_STAGE_RECV)
				dma_data_dir = DMA_FROM_DEVICE;
		} else {
			dma_data_dir = (usb_endpoint_dir_in(udc_ep_ptr->desc)
					? DMA_TO_DEVICE : DMA_FROM_DEVICE);
		}
		udc_req_ptr->usb_req.dma =
			dma_map_single(crg_udc->dev, udc_req_ptr->usb_req.buf,
					udc_req_ptr->usb_req.length,
					dma_data_dir);

		//udc_req_ptr->usb_req.dma = g_dma;
		//memcpy(g_vaddr, udc_req_ptr->usb_req.buf, udc_req_ptr->usb_req.length);

		udc_req_ptr->mapped = 1;

		CRG_DEBUG("dma_map_single and mapped is 1\n");
	}

	udc_req_ptr->usb_req.status = -EINPROGRESS;
	udc_req_ptr->usb_req.actual = 0;

	/* If the transfer ring for this particular end point is full,
	 * then simply queue the request and return
	 */
	if (udc_ep_ptr->tran_ring_full) {
		status = 0;
	} else {
		/* push the request to the transfer ring if possible. */
		status = crg_udc_build_td(udc_ep_ptr, udc_req_ptr);
	}
	if (!status)
		list_add_tail(&udc_req_ptr->queue, &udc_ep_ptr->queue);

	spin_unlock_irqrestore(&crg_udc->udc_lock, flags);

	return status;
}

static int
crg_udc_ep_dequeue(struct usb_ep *_ep, struct usb_request *_req)
{
	struct crg_udc_ep *udc_ep_ptr;
	struct crg_gadget_dev *crg_udc;
	struct crg_uccr *uccr;
	u32 tmp = 0;
	struct crg_udc_request *udc_req;
	struct ep_cx_s *p_ep_cx;
	int old_ep_state;
	struct transfer_trb_s *pause_pt;
	u32 deq_pt_lo, deq_pt_hi;
	u64 dq_pt_addr;
	u8 DCI;
	unsigned long flags = 0;

	CRG_DEBUG("%s\n", __func__);

	if (!_ep || !_req)
		return -EINVAL;

	udc_ep_ptr = container_of(_ep, struct crg_udc_ep, usb_ep);
	crg_udc = udc_ep_ptr->crg_udc;
	uccr = crg_udc->uccr;
	DCI = udc_ep_ptr->DCI;

	spin_lock_irqsave(&crg_udc->udc_lock, flags);

	CRG_DEBUG("%s EPDCI = 0x%x\n", __func__, DCI);
	old_ep_state = get_ep_state(crg_udc, DCI);
	if (DCI != 0 && old_ep_state == EP_STATE_RUNNING) {
		u32 param0;

		CRG_DEBUG("%s, EP_STATE_RUNNING\n", __func__);
		param0 = (0x1 << DCI);
		/* stop the DMA from HW first */
		crg_issue_command(crg_udc, CRG_CMD_STOP_EP, param0, 0);
		do {
			tmp = reg_read(&uccr->ep_running);
		} while ((tmp & param0) != 0);
		udc_ep_ptr->ep_state = EP_STATE_STOPPED;
	}

	list_for_each_entry(udc_req, &udc_ep_ptr->queue, queue) {
		if (&udc_req->usb_req == _req)
			break;
	}

	if (&udc_req->usb_req != _req) {
		CRG_DEBUG("did not find the request in request queue\n");
		spin_unlock_irqrestore(&crg_udc->udc_lock, flags);
		return -EINVAL;
	}
	/* Request hasn't been queued to transfer ring yet
	 * dequeue it from sw queue only
	 */
	if (!udc_req->first_trb) {
		req_done(udc_ep_ptr, udc_req, -ECONNRESET);
		spin_unlock_irqrestore(&crg_udc->udc_lock, flags);
		return 0;
	}

	/* ep0 cannot be stopped. So if request has already been queued to
	 * transfer ring, it cannot be dequeued
	 */
	if (DCI == 0) {
		spin_unlock_irqrestore(&crg_udc->udc_lock, flags);
		return -EINVAL;
	}

	p_ep_cx = crg_udc->p_epcx + DCI - 2;
	deq_pt_lo = p_ep_cx->dw2 & EP_CX_TR_DQPT_LO_MASK;
	deq_pt_hi = p_ep_cx->dw3;
	dq_pt_addr = (u64)deq_pt_lo + ((u64)deq_pt_hi << 32);
	pause_pt = tran_trb_dma_to_virt(udc_ep_ptr, dq_pt_addr);

	CRG_DEBUG("dequeue pause_pt = 0x%p, first_trb = 0x%p\n",
		pause_pt, udc_req->first_trb);
	CRG_DEBUG("dequeue deq_pt = 0x%p, enq_pt = 0x%p\n",
		udc_ep_ptr->deq_pt, udc_ep_ptr->enq_pt);

	if (is_pointer_less_than(pause_pt, udc_req->first_trb, udc_ep_ptr)) {
		CRG_DEBUG("squeeze_xfer_ring\n");
		/* HW hasn't process the request yet */
		squeeze_xfer_ring(udc_ep_ptr, udc_req);
		req_done(udc_ep_ptr, udc_req, -ECONNRESET);
	} else if (udc_req->last_trb &&
		is_pointer_less_than(udc_req->last_trb, pause_pt, udc_ep_ptr)) {
		/* Request has been completed by HW
		 * There must be transfer events pending in event ring, and
		 * it will be processed later once interrupt context gets spin
		 * lock.
		 * Gadget driver free the request without checking the return
		 * value of usb_ep_dequeue, so we have to complete the request
		 * here and drop the transfer event later.
		 */
		CRG_DEBUG(" Request has been complete by HW, reject request\n");
		req_done(udc_ep_ptr, udc_req, -ECONNRESET);
		spin_unlock_irqrestore(&crg_udc->udc_lock, flags);
		return -EINVAL;

	} else {
		/* Request has been partially completed by HW */

		CRG_DEBUG(" Request has been partially completed by HW\n");
		/*udc_req->usb_req.actual = actual_data_xfered(udc_ep, udc_req);*/

		CRG_DEBUG("%s, complete requests\n", __func__);
		req_done(udc_ep_ptr, udc_req, -ECONNRESET);

		advance_dequeue_pt(udc_ep_ptr);
		crg_udc_epcx_update_dqptr(udc_ep_ptr);

		/* For big TD, we generated completion event every 5 TRBS.
		 * So, we do not need to update sw dequeue pointer here.
		 * Wait for interrupt context to update it.
		 * Do not need to queue more trbs also.
		 */
	}
#if CRG_MTP_WR
	crg_issue_command(crg_udc, CRG_CMD_STOP_EP, 1 << DCI, 0);
	do {
		tmp = reg_read(&uccr->ep_running);
	} while ((tmp & (1 << DCI)) != 0);

	mdelay(10);
	crg_udc_epcx_update_dqptr(udc_ep_ptr);

	mdelay(10);
	crg_issue_command(crg_udc, CRG_CMD_STOP_EP, 1 << DCI, 0);
	do {
		tmp = reg_read(&uccr->ep_running);
	} while ((tmp & (1 << DCI)) != 0);
#endif
	CRG_DEBUG("End dequeue deq_pt = 0x%p, enq_pt = 0x%p\n",
			udc_ep_ptr->deq_pt, udc_ep_ptr->enq_pt);

	/* knock doorbell and resume data transfer */
	if (old_ep_state == EP_STATE_RUNNING) {
		tmp = DCI;
		tmp = CRG_U3DC_DB_TARGET(tmp);

		knock_doorbell(crg_udc, udc_ep_ptr->DCI);

		udc_ep_ptr->ep_state = EP_STATE_RUNNING;
	}

	spin_unlock_irqrestore(&crg_udc->udc_lock, flags);

	return 0;
}

static int crg_udc_ep_set_halt(struct usb_ep *_ep, int value)
{
	struct crg_udc_ep *udc_ep_ptr;
	int status;
	unsigned long flags = 0;
	struct crg_gadget_dev *crg_udc;

	CRG_DEBUG("%s\n", __func__);

	if (!_ep)
		return -EINVAL;

	udc_ep_ptr = container_of(_ep, struct crg_udc_ep, usb_ep);
	crg_udc = udc_ep_ptr->crg_udc;

	spin_lock_irqsave(&crg_udc->udc_lock, flags);

	if (value && usb_endpoint_dir_in(udc_ep_ptr->desc) &&
			!list_empty(&udc_ep_ptr->queue)) {
		CRG_ERROR("set_halt: list not empty\n");
		spin_unlock_irqrestore(&crg_udc->udc_lock, flags);
		return -EAGAIN;
	}

	status = ep_halt(udc_ep_ptr, value, 1);

	spin_unlock_irqrestore(&crg_udc->udc_lock, flags);

	return status;
}

static int crg_udc_ep_set_wedge(struct usb_ep *_ep)
{
	struct crg_udc_ep *udc_ep_ptr;
	int status;
	unsigned long flags = 0;
	struct crg_gadget_dev *crg_udc;

	CRG_DEBUG("%s\n", __func__);

	if (!_ep)
		return -EINVAL;

	udc_ep_ptr = container_of(_ep, struct crg_udc_ep, usb_ep);
	crg_udc = udc_ep_ptr->crg_udc;

	spin_lock_irqsave(&crg_udc->udc_lock, flags);

	udc_ep_ptr->wedge = 1;

	status = ep_halt(udc_ep_ptr, 1, 1);

	spin_unlock_irqrestore(&crg_udc->udc_lock, flags);

	return status;
}

static struct usb_ep_ops crg_udc_ep_ops = {
	.enable = crg_udc_ep_enable,
	.disable = crg_udc_ep_disable,
	.alloc_request = crg_udc_alloc_request,
	.free_request = crg_udc_free_request,
	.queue = crg_udc_ep_queue,
	.dequeue = crg_udc_ep_dequeue,
	.set_halt = crg_udc_ep_set_halt,
	.set_wedge = crg_udc_ep_set_wedge,
};

static void crg_ep_struct_setup(struct crg_gadget_dev *crg_udc,
			u32 DCI, const char *name)
{
	struct crg_udc_ep *ep = &crg_udc->udc_ep[DCI];

	ep->DCI = DCI;

	if (ep->DCI > 1) {
		strncpy(ep->name, name, sizeof(ep->name) - 1);
		ep->usb_ep.name = ep->name;
		ep->usb_ep.maxpacket = 1024;
		ep->usb_ep.max_streams = 16;

		ep->usb_ep.caps.type_iso = 1;
		ep->usb_ep.caps.type_bulk = 1;
		ep->usb_ep.caps.type_int = 1;
		ep->usb_ep.caps.dir_in = 1;
		ep->usb_ep.caps.type_control = 1;

		ep->usb_ep.caps.type_iso = 1;
		ep->usb_ep.caps.type_bulk = 1;
		ep->usb_ep.caps.type_int = 1;
		ep->usb_ep.caps.type_control = 1;
		if (ep->DCI % 2)
			ep->usb_ep.caps.dir_out = 1;
		else
			ep->usb_ep.caps.dir_in = 1;
		usb_ep_set_maxpacket_limit(&ep->usb_ep, MAX_PACKET_SIZE);
	} else {
		strcpy(ep->name, "ep0");
		ep->usb_ep.name = ep->name;
		ep->usb_ep.maxpacket = 64;
		ep->usb_ep.caps.type_control = 1;
		ep->usb_ep.caps.dir_in = 1;
		ep->usb_ep.caps.dir_out = 1;
	}

	CRG_DEBUG("ep = 0x%p, ep name = %s maxpacket = %d DCI=%d\n",
			ep, ep->name, ep->usb_ep.maxpacket, ep->DCI);
	ep->usb_ep.ops = &crg_udc_ep_ops;
	ep->crg_udc = crg_udc;

	INIT_LIST_HEAD(&ep->queue);
	if (ep->DCI > 1)
		list_add_tail(&ep->usb_ep.ep_list, &crg_udc->gadget.ep_list);
}


static void enable_setup_event(struct crg_gadget_dev *crg_udc)
{
	struct crg_uccr *uccr = crg_udc->uccr;
	u32 val;

	CRG_DEBUG("before setup en config1[0x%p]=0x%x\n",
		&uccr->config1, reg_read(&uccr->config1));

	//SETBITS(&uccr->config1, CRG_U3DC_CFG1_SETUP_EVENT_EN);
	val = reg_read(&uccr->config1);
	val |= CRG_U3DC_CFG1_SETUP_EVENT_EN;
	reg_write(&uccr->config1, val);
	CRG_DEBUG("update config1[0x%p]=0x%x\n",
		&uccr->config1, reg_read(&uccr->config1));
}

int is_event_ring_x_empty(struct crg_gadget_dev *crg_udc, int index)
{
	struct event_trb_s *event;
	struct crg_udc_event *udc_event;

	udc_event = &crg_udc->udc_event[index];
	if (udc_event->evt_dq_pt) {
		event = (struct event_trb_s *)udc_event->evt_dq_pt;

		if (GETF(EVE_TRB_CYCLE_BIT, event->dw3) !=
				udc_event->CCS)
			return 1;
	}

	return 0;
}

int is_event_rings_empty(struct crg_gadget_dev *crg_udc)
{
	int i;

	for (i = 0; i < CRG_RING_NUM; i++) {
		if (!is_event_ring_x_empty(crg_udc, i)) {
			CRG_ERROR("%s evt ring not empty\n", __func__);
			return 0;
		}
	}
	return 1;
}

static int enable_setup(struct crg_gadget_dev *crg_udc)
{
	enable_setup_event(crg_udc);
	crg_udc->device_state = USB_STATE_DEFAULT;
	crg_udc->setup_status = WAIT_FOR_SETUP;
	CRG_DEBUG("%s ready to receive setup events\n", __func__);

	return 0;
}

static int prepare_for_setup(struct crg_gadget_dev *crg_udc)
{
	struct crg_udc_ep *udc_ep0_ptr;

#ifdef REINIT_EP0_ON_BUS_RESET
	dma_addr_t dqptaddr;
	u32 cmd_param0;
	u32 cmd_param1;
#endif

	if (!is_event_rings_empty(crg_udc) ||
		crg_udc->portsc_on_reconnecting == 1)
		return -EBUSY;

	udc_ep0_ptr = &crg_udc->udc_ep[0];
/* If we reinit ep0 on bus reset, we just make ep0 dequeue pointer align
 * with enqueue pointer, all remaining xfer trbs became dumb ones which
 * will not produce xfer event anymore.
 *
 * If we considering the opposite solution, we should wait all ep0 xfer
 * trbs be completed(with some err complete code)
 */
#ifdef REINIT_EP0_ON_BUS_RESET
	/* Complete any reqs on EP0 queue */
	nuke(udc_ep0_ptr, -ESHUTDOWN);

	crg_udc->ctrl_req_enq_idx = 0;
	memset(crg_udc->ctrl_req_queue, 0,
			sizeof(struct crg_setup_packet) * CTRL_REQ_QUEUE_DEPTH);

	/*context related ops*/
	dqptaddr = tran_trb_virt_to_dma(udc_ep0_ptr, udc_ep0_ptr->enq_pt);
	cmd_param0 = (lower_32_bits(dqptaddr) &
			CRG_CMD0_0_DQPTRLO_MASK) |
			CRG_CMD0_0_DCS(udc_ep0_ptr->pcs);
	cmd_param1 = upper_32_bits(dqptaddr);
	crg_issue_command(crg_udc,
		CRG_CMD_INIT_EP0, cmd_param0, cmd_param1);

	/* TRY this solution*/
	udc_ep0_ptr->deq_pt = udc_ep0_ptr->enq_pt;
	udc_ep0_ptr->tran_ring_full = false;
#else
	if (!list_empty(&udc_ep0_ptr->queue)) {
		CRG_ERROR("%s remaining ep0 xfer trbs on reset!\n", __func__);
		return -EBUSY;
	}
#endif

	enable_setup(crg_udc);

	return 0;
}

static void update_ep0_maxpacketsize(struct crg_gadget_dev *crg_udc)
{
	u16 maxpacketsize = 0;
	struct crg_udc_ep *udc_ep0 = &crg_udc->udc_ep[0];
	u32 param0;

	if (crg_udc->gadget.speed >= USB_SPEED_SUPER)
		maxpacketsize = 512;
	else
		maxpacketsize = 64;

	param0 = CRG_CMD1_0_MPS(maxpacketsize);
	crg_issue_command(crg_udc, CRG_CMD_UPDATE_EP0_CFG, param0, 0);

	crg_udc_ep0_desc.wMaxPacketSize = cpu_to_le16(maxpacketsize);
	udc_ep0->usb_ep.maxpacket = maxpacketsize;
}

static int init_event_ring(struct crg_gadget_dev *crg_udc, int index)
{
	struct crg_uicr *uicr = crg_udc->uicr[index];
	struct crg_udc_event *udc_event = &crg_udc->udc_event[index];

	int ret;
	u32 buff_length;
	dma_addr_t mapping;

	buff_length = CRG_ERST_SIZE * sizeof(struct erst_s);
	if (!udc_event->erst.vaddr) {
		udc_event->erst.vaddr =
			dma_alloc_coherent(crg_udc->dev, buff_length,
				&mapping, GFP_KERNEL);

		if (!udc_event->erst.vaddr) {
			ret = -ENOMEM;
			return ret;
		}
	} else {
		mapping = udc_event->erst.dma;
	}

	udc_event->erst.len = buff_length;
	udc_event->erst.dma = mapping;
	udc_event->p_erst = udc_event->erst.vaddr;

	buff_length = CRG_EVENT_RING_SIZE * sizeof(struct event_trb_s);
	if (!udc_event->event_ring.vaddr) {
		udc_event->event_ring.vaddr =
			dma_alloc_coherent(crg_udc->dev, buff_length,
				&mapping, GFP_KERNEL);

		if (!udc_event->event_ring.vaddr) {
			ret = -ENOMEM;
			return ret;
		}
	} else {
		mapping = udc_event->event_ring.dma;
	}

	udc_event->event_ring.len = buff_length;
	udc_event->event_ring.dma = mapping;
	udc_event->evt_dq_pt = udc_event->event_ring.vaddr;

	udc_event->evt_seg0_last_trb =
		(struct event_trb_s *)(udc_event->event_ring.vaddr)
		+ (CRG_EVENT_RING_SIZE - 1);

	udc_event->CCS = 1;

	udc_event->p_erst->seg_addr_lo =
		lower_32_bits(udc_event->event_ring.dma);
	udc_event->p_erst->seg_addr_hi =
		upper_32_bits(udc_event->event_ring.dma);
	udc_event->p_erst->seg_size = cpu_to_le32(CRG_EVENT_RING_SIZE);
	udc_event->p_erst->rsvd = 0;
	/* wmb */
	wmb();
	/*clear the event ring, to avoid hw unexpected ops
	 *because of dirty data
	 */
	memset(udc_event->event_ring.vaddr, 0, buff_length);

	/*hw related ops ERSTBA && ERSTSZ && ERDP*/
	/* wmb */
	wmb();

	/**************************/
	reg_write(&uicr->erstsz, CRG_ERST_SIZE);
	reg_write(&uicr->erstbalo, lower_32_bits(udc_event->erst.dma));
	reg_write(&uicr->erstbahi, upper_32_bits(udc_event->erst.dma));
	reg_write(&uicr->erdplo,
		lower_32_bits(udc_event->event_ring.dma) | CRG_U3DC_ERDPLO_EHB);
	reg_write(&uicr->erdphi, upper_32_bits(udc_event->event_ring.dma));

	reg_write(&uicr->iman, (CRG_U3DC_IMAN_INT_EN | CRG_U3DC_IMAN_INT_PEND));
	reg_write(&uicr->imod, (0L << 0) | (4000L << 0));

	return 0;
}

static int init_device_context(struct crg_gadget_dev *crg_udc)
{
	struct crg_uccr *uccr = crg_udc->uccr;
	int ret;
	u32 buff_length;
	dma_addr_t mapping;

	/*ep0 is not included in ep contexts in crg udc*/
	buff_length = (CRG_NUM_EP_CX - 2) * sizeof(struct ep_cx_s);

	if (!crg_udc->ep_cx.vaddr) {
		crg_udc->ep_cx.vaddr =
			dma_alloc_coherent(crg_udc->dev, buff_length,
				&mapping, GFP_KERNEL);
			/* wmb */
			wmb();
		if  (!crg_udc->ep_cx.vaddr) {
			ret = -ENOMEM;
			return ret;
		}
	} else {
		mapping = crg_udc->ep_cx.dma;
	}

	crg_udc->p_epcx = crg_udc->ep_cx.vaddr;
	crg_udc->ep_cx.len = buff_length;
	crg_udc->ep_cx.dma = mapping;

	/*hw ops DCBAPLO DCBAPHI*/
	reg_write(&uccr->dcbaplo, lower_32_bits(crg_udc->ep_cx.dma));
	reg_write(&uccr->dcbaphi, upper_32_bits(crg_udc->ep_cx.dma));

	CRG_DEBUG("dcbaplo[0x%p]=0x%x\n", &uccr->dcbaplo, reg_read(&uccr->dcbaplo));
	CRG_DEBUG("dcbaphi[0x%p]=0x%x\n", &uccr->dcbaphi, reg_read(&uccr->dcbaphi));

	return 0;
}

static int reset_data_struct(struct crg_gadget_dev *crg_udc)
{
	u32 tmp;
	int i;
	struct crg_uccr *uccr = crg_udc->uccr;
	u32 val;

	val = reg_read(&uccr->control);
	val &= (~(CRG_U3DC_CTRL_INT_EN | CRG_U3DC_CTRL_RUN));
	reg_write(&uccr->control, val);

	CRG_DEBUG("capability[0x%p]=0x%x\n", &uccr->capability,
			reg_read(&uccr->capability));

	switch (crg_udc->gadget.max_speed) {
	case USB_SPEED_FULL:
		tmp = CRG_U3DC_CFG0_MAXSPEED_FS;
		break;
	case USB_SPEED_HIGH:
		tmp = CRG_U3DC_CFG0_MAXSPEED_HS;
		break;
	case USB_SPEED_SUPER:
		tmp = CRG_U3DC_CFG0_MAXSPEED_SS;
		break;
	case USB_SPEED_SUPER_PLUS:
		tmp = CRG_U3DC_CFG0_MAXSPEED_SSP;
		break;
	case USB_SPEED_UNKNOWN:
	default:
		return -EINVAL;
	}
	reg_write(&uccr->config0, tmp);

	for (i = 0; i < CRG_RING_NUM; i++)
		init_event_ring(crg_udc, i);

	init_device_context(crg_udc);

	if (!crg_udc->status_req) {
		crg_udc->status_req =
		container_of(crg_udc_alloc_request(&crg_udc->udc_ep[0].usb_ep,
			GFP_ATOMIC), struct crg_udc_request,
			usb_req);
	}

	/*other hw ops*/

	return 0;
}

static int init_ep0(struct crg_gadget_dev *crg_udc)
{
	struct crg_udc_ep *udc_ep_ptr = &crg_udc->udc_ep[0];
	u32 cmd_param0;
	u32 cmd_param1;

	/* setup transfer ring */
	if (!udc_ep_ptr->tran_ring_info.vaddr) {
		dma_addr_t dma;
		u32 ring_size = CRGUDC_CONTROL_EP_TD_RING_SIZE;
		void *vaddr;
		size_t len;

		len = ring_size * sizeof(struct transfer_trb_s);
		vaddr = dma_alloc_coherent(crg_udc->dev, len,
					&dma, GFP_KERNEL);
		if (!vaddr) {
			CRG_ERROR("failed to allocate trb ring\n");
			return -ENOMEM;
		}

		udc_ep_ptr->tran_ring_info.vaddr = vaddr;
		udc_ep_ptr->tran_ring_info.dma = dma;
		udc_ep_ptr->tran_ring_info.len = len;
		udc_ep_ptr->first_trb = vaddr;
		udc_ep_ptr->last_trb = udc_ep_ptr->first_trb + ring_size - 1;
	}

	memset(udc_ep_ptr->first_trb, 0, udc_ep_ptr->tran_ring_info.len);
	/* wmb */
	wmb();
	udc_ep_ptr->enq_pt = udc_ep_ptr->first_trb;
	udc_ep_ptr->deq_pt = udc_ep_ptr->first_trb;
	udc_ep_ptr->pcs = 1;
	udc_ep_ptr->tran_ring_full = false;

	setup_link_trb(udc_ep_ptr->last_trb,
		true, udc_ep_ptr->tran_ring_info.dma);

	/*context related ops*/
	cmd_param0 = (lower_32_bits(udc_ep_ptr->tran_ring_info.dma) &
			CRG_CMD0_0_DQPTRLO_MASK) |
			CRG_CMD0_0_DCS(udc_ep_ptr->pcs);
	cmd_param1 = upper_32_bits(udc_ep_ptr->tran_ring_info.dma);

	CRG_DEBUG("ep0 ring dma addr = 0x%llx\n", udc_ep_ptr->tran_ring_info.dma);

	CRG_DEBUG("ep0 ring vaddr = 0x%p\n", udc_ep_ptr->tran_ring_info.vaddr);

	CRG_DEBUG("INIT EP0 CMD, par0=0x%x, par1=0x%x\n", cmd_param0, cmd_param1);

	crg_issue_command(crg_udc, CRG_CMD_INIT_EP0, cmd_param0, cmd_param1);

	udc_ep_ptr->ep_state = EP_STATE_RUNNING;

	return 0;
}

static int EP0_Start(struct crg_gadget_dev *crg_udc)
{
	crg_udc->udc_ep[0].desc = &crg_udc_ep0_desc;

	return 0;
}

static void crg_udc_start(struct crg_gadget_dev *crg_udc)
{
	struct crg_uccr *uccr;
	u32 val;

	CRG_DEBUG("%s %d\n", __func__, __LINE__);

	uccr = crg_udc->uccr;

	/*****interrupt related*****/
	val = reg_read(&uccr->config1);
	val |= (CRG_U3DC_CFG1_CSC_EVENT_EN |
			CRG_U3DC_CFG1_PEC_EVENT_EN |
			CRG_U3DC_CFG1_PPC_EVENT_EN |
			CRG_U3DC_CFG1_PRC_EVENT_EN |
			CRG_U3DC_CFG1_PLC_EVENT_EN |
			CRG_U3DC_CFG1_CEC_EVENT_EN);
	reg_write(&uccr->config1, val);
	CRG_DEBUG("config1[0x%p]=0x%x\n", &uccr->config1, reg_read(&uccr->config1));
	CRG_DEBUG("config0[0x%p]=0x%x\n", &uccr->config0, reg_read(&uccr->config0));

	val = reg_read(&uccr->control);
	val |= (CRG_U3DC_CTRL_SYSERR_EN |
			CRG_U3DC_CTRL_INT_EN);
	reg_write(&uccr->control, val);
	/*****interrupt related end*****/

	val = reg_read(&uccr->control);
	val |= CRG_U3DC_CTRL_RUN;
	reg_write(&uccr->control, val);
	CRG_DEBUG("%s, control=0x%x\n", __func__, reg_read(&uccr->control));
}

void halt_all_eps(struct crg_gadget_dev *crg_udc)
{
	struct crg_uccr *uccr = crg_udc->uccr;
	u32 tmp;

	tmp = reg_read(&uccr->ep_running);
	crg_issue_command(crg_udc, CRG_CMD_SET_HALT, tmp, 0);
	do {
		tmp = reg_read(&uccr->ep_running);
	} while (tmp != 0);
}

void crg_udc_clear_portpm(struct crg_gadget_dev *crg_udc)
{
	struct crg_uccr *uccr = crg_udc->uccr;
	u32 tmp;

	tmp = reg_read(&uccr->u3portpmsc);

	tmp &= (~CRG_U3DC_U3PORTPM_U1IEN);
	/*tmp &= (~CRG_U3DC_U3PORTPM_U1AEN);*/
	tmp = SETF_VAR(CRG_U3DC_U3PORTPM_U1TMOUT, tmp, 0);
	tmp &= (~CRG_U3DC_U3PORTPM_U2IEN);
	/*tmp &= (~CRG_U3DC_U3PORTPM_U2AEN);*/
	tmp = SETF_VAR(CRG_U3DC_U3PORTPM_U2TMOUT, tmp, 0);

	reg_write(&uccr->u3portpmsc, tmp);

	crg_udc->feature_u1_enable = 0;
	crg_udc->feature_u2_enable = 0;
}

void crg_udc_reinit(struct crg_gadget_dev *crg_udc)
{
	struct crg_uccr *uccr = crg_udc->uccr;
	u32 i, tmp;
	unsigned long flags = 0;
	struct crg_udc_ep *udc_ep_ptr;

	crg_udc->setup_status = WAIT_FOR_SETUP;
	/* Base on Figure 9-1, default USB_STATE is attached */
	crg_udc->device_state = USB_STATE_RECONNECTING;

	/* halt all the endpoints */
	/*halt_all_eps(crg_udc);*/

	CRG_DEBUG("ep_enable=0x%x\n", reg_read(&uccr->ep_enable));
	CRG_DEBUG("ep_running=0x%x\n", reg_read(&uccr->ep_running));

	/* disable all the endpoints */
	tmp = reg_read(&uccr->ep_enable);
	reg_write(&uccr->ep_enable, tmp);
	for (i = 0; i < 50; i++) {
		tmp = reg_read(&uccr->ep_enable);
		if (tmp == 0)
			break;
	}

	CRG_DEBUG("i=%d\n", i);
	CRG_DEBUG("after ep_enable=0x%x\n", reg_read(&uccr->ep_enable));

	for (i = 2; i < 32; i++) {
		udc_ep_ptr = &crg_udc->udc_ep[i];
		udc_ep_ptr->usb_ep.enabled = 0;
		if (udc_ep_ptr->desc)
			nuke(udc_ep_ptr, -ESHUTDOWN);
		udc_ep_ptr->tran_ring_full = false;
		udc_ep_ptr->ep_state = EP_STATE_DISABLED;
	}
	crg_udc->num_enabled_eps = 0;

/* we don't handle ep0 here, we init_ep0 when event ring is empty*/

	if (crg_udc->dev_addr != 0) {
		u32 param0;

		param0 = CRG_CMD2_0_DEV_ADDR(0);
		crg_issue_command(crg_udc, CRG_CMD_SET_ADDR, param0, 0);
		crg_udc->dev_addr = 0;
	}

	crg_udc_clear_portpm(crg_udc);

	if (crg_udc->gadget_driver) {
		CRG_DEBUG("calling disconnect\n");
		spin_unlock_irqrestore(&crg_udc->udc_lock, flags);
		crg_udc->gadget_driver->disconnect(&crg_udc->gadget);
		spin_lock_irqsave(&crg_udc->udc_lock, flags);
	}
}

static int crg_udc_reset(struct crg_gadget_dev *crg_udc)
{
	struct crg_uccr *uccr = crg_udc->uccr;
	u32 i, tmp, count;
	struct crg_udc_ep *udc_ep_ptr;
	u32 val;

	CRG_DEBUG("capability = %x\n",  reg_read(&uccr->capability));

	count = 0;
	val = reg_read(&uccr->control);
	val |= CRG_U3DC_CTRL_SWRST;
	reg_write(&uccr->control, val);
	do {
		mdelay(10);
		tmp = reg_read(&uccr->control);
		count++;

		if (count == 50) {
			CRG_ERROR("reset error\n");
			return -1;
		}
	} while ((tmp & CRG_U3DC_CTRL_SWRST) != 0);

	crg_udc_clear_portpm(crg_udc);

	crg_udc->setup_status = WAIT_FOR_SETUP;
	/* Base on Figure 9-1, default USB_STATE is attached */
	crg_udc->device_state = USB_STATE_ATTACHED;
	crg_udc->dev_addr = 0;

	/* Complete any reqs on EP1-EP15 queue */
	for (i = 2; i < 32; i++) {
		udc_ep_ptr = &crg_udc->udc_ep[i];

		if (udc_ep_ptr->desc)
			nuke(udc_ep_ptr, -ESHUTDOWN);
		udc_ep_ptr->tran_ring_full = false;
		udc_ep_ptr->ep_state = EP_STATE_DISABLED;
	}
	crg_udc->num_enabled_eps = 0;

	/* Complete any reqs on EP0 queue */
	udc_ep_ptr = &crg_udc->udc_ep[0];
	if (udc_ep_ptr->desc)
		nuke(udc_ep_ptr, -ESHUTDOWN);

	crg_udc->ctrl_req_enq_idx = 0;
	memset(crg_udc->ctrl_req_queue, 0,
			sizeof(struct crg_setup_packet) * CTRL_REQ_QUEUE_DEPTH);

	CRG_DEBUG("%s %d\n", __func__, __LINE__);

	return 0;
}

/************controller related ops*******************************/
static int crg_gadget_get_frame(struct usb_gadget *g)
{
	CRG_DEBUG("%s\n", __func__);

	return 0;
}

static int crg_gadget_wakeup(struct usb_gadget *g)
{
	CRG_DEBUG("%s\n", __func__);

	return 0;
}

static int crg_gadget_set_selfpowered(struct usb_gadget *g,
		int is_selfpowered)
{
	CRG_DEBUG("%s\n", __func__);

	return 0;
}

static int crg_gadget_pullup(struct usb_gadget *g, int is_on)
{
	CRG_DEBUG("%s\n", __func__);
	g_dnl_board_usb_cable_connected();

	return 0;
}

#define gadget_to_udc(g)	(container_of(g, struct crg_gadget_dev, gadget))

static int crg_vbus_detect_thread(void *data);

static void crg_vbus_detect(struct crg_gadget_dev *crg_udc, int enable)
{
	if (enable) {
		if (crg_udc->vbus_task) {
			CRG_ERROR("vbus task already run, wake up vbus_wait/n");
			wake_up_interruptible(&vbus_wait);
			return;
		}

		crg_udc->vbus_task = kthread_run(crg_vbus_detect_thread,
								NULL, "corigine_vbus_thread");
		if (IS_ERR(crg_udc->vbus_task)) {
			CRG_ERROR("Unable to create corigine_vbus_thread./n");
			crg_udc->vbus_task = NULL;
			return;
		}
	} else {
		if (crg_udc->vbus_task) {
			wake_up_interruptible(&vbus_wait);
			kthread_stop(crg_udc->vbus_task);
			crg_udc->vbus_task = NULL;
			return;
		}
	}
}

static int crg_gadget_start(struct usb_gadget *g,
		struct usb_gadget_driver *driver)
{
	struct crg_gadget_dev *crg_udc;

	CRG_DEBUG("%s %d\n", __func__, __LINE__);

	crg_udc = gadget_to_udc(g);
	crg_udc->gadget_driver = driver;

	CRG_DEBUG("%s %d gadget speed=%d, max speed=%d\n",
		__func__, __LINE__, g->speed, g->max_speed);
	CRG_DEBUG("%s %d driver speed=%d\n", __func__, __LINE__, driver->max_speed);
	g_dnl_board_usb_cable_connected();

	return 0;
}

static void crg_reg_dump(struct crg_gadget_dev *crg_udc)
{
}

static int crg_gadget_stop(struct usb_gadget *g)
{
	struct crg_gadget_dev *crg_udc;
	unsigned long flags = 0;

	crg_udc = gadget_to_udc(g);

	crg_udc->device_state = USB_STATE_ATTACHED;
	crg_vbus_detect(crg_udc, 0);

	spin_lock_irqsave(&crg_udc->udc_lock, flags);

	crg_reg_dump(crg_udc);

	crg_udc_reset(crg_udc);

	reset_data_struct(crg_udc);
	crg_udc->connected = 0;
	crg_udc->gadget_driver = NULL;
	crg_udc->gadget.speed = USB_SPEED_UNKNOWN;

	init_ep0(crg_udc);

	spin_unlock_irqrestore(&crg_udc->udc_lock, flags);

	return 0;
}

static const struct usb_gadget_ops crg_gadget_ops = {
	.get_frame		= crg_gadget_get_frame,
	.wakeup			= crg_gadget_wakeup,
	.set_selfpowered	= crg_gadget_set_selfpowered,
	.pullup			= crg_gadget_pullup,
	.udc_start		= crg_gadget_start,
	.udc_stop		= crg_gadget_stop,
};

static int init_ep_info(struct crg_gadget_dev *crg_udc)
{
	int i;

	/*udc_ep[0] is reserved, */
	crg_ep_struct_setup(crg_udc, 0, NULL);

	for (i = 1; i < CRG_NUM_EP_CX / 2; i++) {
		char name[14];

		sprintf(name, "ep%din", i);
		crg_ep_struct_setup(crg_udc, i * 2, name);
		sprintf(name, "ep%dout", i);
		crg_ep_struct_setup(crg_udc, i * 2 + 1, name);
	}

	return 0;
}

void queue_setup_pkt(struct crg_gadget_dev *crg_udc,
		struct usb_ctrlrequest *setup_pkt,
		u16 setup_tag)
{
	if (crg_udc->ctrl_req_enq_idx == CTRL_REQ_QUEUE_DEPTH) {
		CRG_ERROR("ctrl request queque is full\n");
		return;
	}

	memcpy(&crg_udc->ctrl_req_queue[crg_udc->ctrl_req_enq_idx].usbctrlreq,
			setup_pkt, sizeof(struct usb_ctrlrequest));
	crg_udc->ctrl_req_queue[crg_udc->ctrl_req_enq_idx].setup_tag =
		setup_tag;

	crg_udc->ctrl_req_enq_idx++;
}

static inline u32 index2DCI(u16 index)
{
	if (index == 0)
		return 0;

	return (index & USB_ENDPOINT_NUMBER_MASK) * 2 + ((index &
				USB_DIR_IN) ? 0 : 1);
}

void get_status_cmpl(struct usb_ep *ep, struct usb_request *req)
{
	kfree(req->buf);
}

void getstatusrequest(struct crg_gadget_dev *crg_udc,
		u8 RequestType, u16 value, u16 index, u16 length)
{
	u32 status_val = 0;
	u32 status = -EINPROGRESS;
	struct crg_udc_ep *udc_ep_ptr;
	struct crg_udc_request *udc_req_ptr = crg_udc->status_req;

	if (!udc_req_ptr)
		return;

	udc_req_ptr->usb_req.buf = kzalloc(sizeof(u16), GFP_ATOMIC);
	CRG_DEBUG("udc_req_ptr->usb_req.buf = 0x%p\n", udc_req_ptr->usb_req.buf);
	if (!udc_req_ptr->usb_req.buf)
		return;

	if (value || length > 2 || !length) {
		status = -EINVAL;
		goto get_status_error;
	}

	CRG_DEBUG("Get status request RequestType = 0x%x Index=%x\n",
			RequestType, index);
	if ((RequestType & USB_RECIP_MASK) == USB_RECIP_DEVICE) {
		CRG_DEBUG("Get status request Device request\n");
		if (index) {
			status = -EINVAL;
			goto get_status_error;
		}

		if (crg_udc->gadget.speed == USB_SPEED_HIGH ||
			crg_udc->gadget.speed == USB_SPEED_FULL) {
			if (crg_udc->u2_RWE)
				status_val |= BIT(USB_DEVICE_REMOTE_WAKEUP);
		}

		if (crg_udc->gadget.speed >= USB_SPEED_SUPER) {
			if (crg_udc->feature_u1_enable)
				status_val |= BIT(USB_DEV_STAT_U1_ENABLED);
			if (crg_udc->feature_u2_enable)
				status_val |= BIT(USB_DEV_STAT_U2_ENABLED);
		}

		status_val |= USB_DEVICE_SELF_POWERED;
		CRG_DEBUG("Status = 0x%x\n", status_val);

	} else if ((RequestType & USB_RECIP_MASK) == USB_RECIP_INTERFACE) {
		CRG_DEBUG("Get status request Interface request\n");
		status_val = 0;
	} else if ((RequestType & USB_RECIP_MASK) == USB_RECIP_ENDPOINT) {
		u32 DCI;

		DCI = index2DCI(index);

		CRG_DEBUG("Get status request endpoint request DCI = %d\n", DCI);

		if (DCI == 1) {
			status_val = 0;
			CRG_DEBUG("Get status request INVALID! DCI = %d\n", DCI);
			goto get_status_error;
		}
		/* if device state is address state, index should be 0
		 * if device state is configured state, index should be an
		 * endpoint configured.
		 */

		CRG_DEBUG("crg_udc->device_state = %d\n", crg_udc->device_state);
		if (crg_udc->device_state == USB_STATE_ADDRESS && DCI != 0) {
			status = -EINVAL;
			goto get_status_error;
		}

		if (crg_udc->device_state == USB_STATE_CONFIGURED) {
			if (get_ep_state(crg_udc, DCI) == EP_STATE_DISABLED) {
				status = -EINVAL;
				goto get_status_error;
			}

			if (get_ep_state(crg_udc, DCI) == EP_STATE_HALTED) {
				status_val = BIT(USB_ENDPOINT_HALT);
				CRG_DEBUG("endpoint was halted = 0x%lx\n",
					(unsigned long)status_val);
			}
		}
	}

get_status_error:
	if (status != -EINPROGRESS) {
		udc_req_ptr->usb_req.length = 0;
	} else {
		*(u16 *)udc_req_ptr->usb_req.buf = cpu_to_le16(status_val);
		CRG_DEBUG("usb_req.buf = 0x%x\n",
				*((u16 *)udc_req_ptr->usb_req.buf));

		CRG_DEBUG("usb_req.buf addr = 0x%p\n",
				(udc_req_ptr->usb_req.buf));

		udc_req_ptr->usb_req.length = 2;
	}
	udc_req_ptr->usb_req.status = status;
	udc_req_ptr->usb_req.actual = 0;
	udc_req_ptr->usb_req.complete = get_status_cmpl;

	if (udc_req_ptr->usb_req.dma == DMA_ADDR_INVALID) {
		udc_req_ptr->usb_req.dma =
			dma_map_single(crg_udc->dev, udc_req_ptr->usb_req.buf,
					udc_req_ptr->usb_req.length,
					DMA_FROM_DEVICE);
		udc_req_ptr->mapped = 1;
	}
	CRG_DEBUG("status_val = 0x%x, cpu_to_le16(status_val) = 0x%x\n",
		status_val, cpu_to_le16(status_val));
	CRG_DEBUG("udc_req_ptr->usb_req.buf = 0x%p, value = 0x%x\n",
		udc_req_ptr->usb_req.buf, *(u16 *)(udc_req_ptr->usb_req.buf));
	CRG_DEBUG("udc_req_ptr->usb_req.dma = 0x%llx\n",
		udc_req_ptr->usb_req.dma);

	udc_ep_ptr = &crg_udc->udc_ep[0];

	crg_udc->setup_status = DATA_STAGE_XFER;
	status = crg_udc_build_td(udc_ep_ptr, udc_req_ptr);

	CRG_DEBUG("getstatus databuf eqpt = 0x%p\n", udc_ep_ptr->enq_pt);

	if (!status)
		list_add_tail(&udc_req_ptr->queue, &udc_ep_ptr->queue);
}

void set_address_cmpl(struct crg_gadget_dev *crg_udc)
{
	if (crg_udc->device_state == USB_STATE_DEFAULT &&
				crg_udc->dev_addr != 0) {
		crg_udc->device_state = USB_STATE_ADDRESS;
		CRG_DEBUG("USB State Addressed\n");

	} else if (crg_udc->device_state == USB_STATE_ADDRESS) {
		if (crg_udc->dev_addr == 0)
			crg_udc->device_state = USB_STATE_DEFAULT;
	}
}

void setaddressrequest(struct crg_gadget_dev *crg_udc,
		u16 value, u16 index, u16 length)
{
	int status = -EINPROGRESS;
	u8 status_set_addr = 0;

	if (value > 127 || index != 0 || length != 0) {
		status = -EINVAL;
		goto set_address_error;
	}

	if ((crg_udc->device_state == USB_STATE_DEFAULT && value != 0) ||
			crg_udc->device_state == USB_STATE_ADDRESS) {
		u32 param0;

		crg_udc->dev_addr = value;

		param0 = CRG_CMD2_0_DEV_ADDR(value);
		crg_issue_command(crg_udc, CRG_CMD_SET_ADDR, param0, 0);
		status_set_addr = 1;
	} else {
		status = -EINVAL;
	}

set_address_error:
	CRG_DEBUG("build_ep0_status for Address Device\n");

	crg_udc->setup_status = STATUS_STAGE_XFER;
	crg_udc->setup_fn_call_back = &set_address_cmpl;
	build_ep0_status(&crg_udc->udc_ep[0],
		true, status, NULL, status_set_addr, 0);
}

void set_sel_cmpl(struct usb_ep *ep, struct usb_request *req)
{
	struct crg_udc_ep *udc_ep;
	struct crg_gadget_dev *crg_udc;
	struct sel_value_s *sel_value;

	udc_ep = container_of(ep, struct crg_udc_ep, usb_ep);
	crg_udc = udc_ep->crg_udc;

	if (req->buf) {
		sel_value = req->buf;
		crg_udc->sel_value.u2_pel_value = sel_value->u2_pel_value;
		crg_udc->sel_value.u2_sel_value = sel_value->u2_sel_value;
		crg_udc->sel_value.u1_pel_value = sel_value->u1_pel_value;
		crg_udc->sel_value.u1_sel_value = sel_value->u1_sel_value;

		kfree(req->buf);
	}

	CRG_DEBUG("u1_sel_value = 0x%x, u2_sel_value = 0x%x\n",
			crg_udc->sel_value.u1_sel_value,
			crg_udc->sel_value.u2_sel_value);
}

void setselrequest(struct crg_gadget_dev *crg_udc,
		u16 value, u16 index, u16 length, u64 data)
{
	int status = -EINPROGRESS;
	struct crg_udc_request *udc_req_ptr = crg_udc->status_req;
	struct crg_udc_ep *udc_ep_ptr = &crg_udc->udc_ep[0];

	if (!udc_req_ptr)
		return;

	if (crg_udc->device_state == USB_STATE_DEFAULT)
		status = -EINVAL;

	if (index != 0 || value != 0 || length != 6)
		status = -EINVAL;

	if (status != -EINPROGRESS) {
		/* udc_req_ptr->usb_req.length = 0; */
	} else {
		udc_req_ptr->usb_req.length = length;
		udc_req_ptr->usb_req.buf =
			kzalloc(sizeof(*udc_req_ptr->usb_req.buf), GFP_ATOMIC);
	}

	udc_req_ptr->usb_req.status = -EINPROGRESS;
	udc_req_ptr->usb_req.actual = 0;
	udc_req_ptr->usb_req.complete = set_sel_cmpl;

	if (udc_req_ptr->usb_req.dma == DMA_ADDR_INVALID) {
		udc_req_ptr->usb_req.dma =
			dma_map_single(crg_udc->dev, udc_req_ptr->usb_req.buf,
					udc_req_ptr->usb_req.length,
					DMA_TO_DEVICE);
		udc_req_ptr->mapped = 1;
	}

	status = crg_udc_build_td(udc_ep_ptr, udc_req_ptr);

	if (!status)
		list_add_tail(&udc_req_ptr->queue, &udc_ep_ptr->queue);
}

void set_test_mode_cmpl(struct crg_gadget_dev *crg_udc)
{
	if (crg_udc->set_tm != 0) {
		u32 tmp;
		struct crg_uccr *uccr = crg_udc->uccr;

		if (crg_udc->controller_type == USB_M31) {
			if (crg_udc->set_tm == USB_TEST_J ||
					crg_udc->set_tm == USB_TEST_K) {
				writel(0x0, crg_udc->phy_reg_addr + 0x848);
				udelay(9);
			} else {
				writel(0x3, crg_udc->phy_reg_addr + 0x848);
				udelay(9);
			}
		}

		tmp = reg_read(&uccr->u2portpmsc);
		tmp = SETF_VAR(CRG_U3DC_U2PORTPM_TM, tmp, crg_udc->set_tm);
		reg_write(&uccr->u2portpmsc, tmp);

		crg_udc->set_tm = 0;
	}
}

bool setfeaturesrequest(struct crg_gadget_dev *crg_udc,
	u8 RequestType, u8 bRequest, u16 value, u16 index, u16 length)
{
	int status = -EINPROGRESS;
	u8  DCI;
	struct crg_udc_ep *udc_ep_ptr;
	u32 tmp;
	bool set_feat = 0;
	struct crg_uccr *uccr = crg_udc->uccr;

	if (length != 0) {
		status = -EINVAL;
		goto set_feature_error;
	}

	if (crg_udc->device_state == USB_STATE_DEFAULT) {
		status = -EINVAL;
		goto set_feature_error;
	}

	set_feat = (bRequest == USB_REQ_SET_FEATURE) ? 1 : 0;
	if ((RequestType & (USB_RECIP_MASK | USB_TYPE_MASK)) ==
			(USB_RECIP_ENDPOINT | USB_TYPE_STANDARD)) {
		CRG_DEBUG("Halt/Unhalt EP\n");
		if (crg_udc->device_state == USB_STATE_ADDRESS) {
			if (index != 0) {
				status = -EINVAL;
				goto set_feature_error;
			}
		}

		DCI = index2DCI(index);

		if (DCI == 1) {
			CRG_DEBUG("setfeat INVALID DCI = 0x%x !!\n", DCI);
			goto set_feature_error;
		}

		udc_ep_ptr = &crg_udc->udc_ep[DCI];
		CRG_DEBUG("halt/Unhalt endpoint DCI = 0x%x\n", DCI);

		status = ep_halt(udc_ep_ptr,
				(bRequest == USB_REQ_SET_FEATURE) ? 1 : 0,
				0);

		if (status < 0)
			goto set_feature_error;
	} else if ((RequestType & (USB_RECIP_MASK | USB_TYPE_MASK)) ==
			(USB_RECIP_DEVICE | USB_TYPE_STANDARD)) {
		switch (value) {
		case USB_DEVICE_REMOTE_WAKEUP:
			CRG_DEBUG("USB_DEVICE_REMOTE_WAKEUP called\n");
			/* REMOTE_WAKEUP selector is not used by USB3.0 */
			if (crg_udc->device_state < USB_STATE_DEFAULT ||
				crg_udc->gadget.speed >= USB_SPEED_SUPER) {
				status = -EINVAL;
				goto set_feature_error;
			}
			CRG_DEBUG("%s_Feature RemoteWake\n",
				set_feat ? "Set" : "Clear");

			/*TODO corigine hardware ops needed*/
			crg_udc->u2_RWE = set_feat;

			break;
		case USB_DEVICE_U1_ENABLE:
		case USB_DEVICE_U2_ENABLE:
		{
			u32 timeout_val;

			CRG_DEBUG("USB_DEVICE_U12_ENABLE called\n");
			if (crg_udc->device_state != USB_STATE_CONFIGURED) {
				CRG_ERROR("%s u12 enable fail, usb state=%d\n",
					__func__, crg_udc->device_state);
				status = -EINVAL;
				goto set_feature_error;
			}

			if (index & 0xff) {
				status = -EINVAL;
				goto set_feature_error;
			}

			if (set_feat == 1 && U12_FORBIDDEN > 0) {
				status = -EINVAL;
				goto set_feature_error;
			}

			tmp = reg_read(&uccr->u3portpmsc);
			/*TODO corigine hardware ops needed*/
			if (value == USB_DEVICE_U1_ENABLE) {
#if (U12_INITIATE_FORBIDDEN == 0)
				tmp &= (~CRG_U3DC_U3PORTPM_U1IEN);
				tmp |= (set_feat <<
					CRG_U3DC_U3PORTPM_U1IEN_SHIFT);
#endif
				if (U12_FORBIDDEN == 0)
					timeout_val = set_feat ? U1_TIMEOUT_VAL : 0;
				else
					timeout_val = 0;

				tmp = SETF_VAR(CRG_U3DC_U3PORTPM_U1TMOUT,
					tmp, timeout_val);

				crg_udc->feature_u1_enable = set_feat;
			}

			if (value == USB_DEVICE_U2_ENABLE) {
#if (U12_INITIATE_FORBIDDEN == 0)
				tmp &= (~CRG_U3DC_U3PORTPM_U2IEN);
				tmp |= (set_feat <<
					CRG_U3DC_U3PORTPM_U2IEN_SHIFT);
#endif
				if (U12_FORBIDDEN == 0)
					timeout_val = set_feat ? U1_TIMEOUT_VAL : 0;
				else
					timeout_val = 0;

				tmp = SETF_VAR(CRG_U3DC_U3PORTPM_U2TMOUT,
					tmp, timeout_val);

				crg_udc->feature_u2_enable = set_feat;
			}
			reg_write(&uccr->u3portpmsc, tmp);

			break;
		}
		case USB_DEVICE_TEST_MODE:
		{
			u32 u_pattern;

			CRG_DEBUG("USB_DEVICE_TEST_MODE called\n");
			if (crg_udc->gadget.speed > USB_SPEED_HIGH)
				goto set_feature_error;

			if (crg_udc->device_state < USB_STATE_DEFAULT)
				goto set_feature_error;

			u_pattern = index >> 8;
			/* TESTMODE is only defined for high speed device */
			if (crg_udc->gadget.speed == USB_SPEED_HIGH) {
				CRG_DEBUG("high speed test mode enter\n");
				crg_udc->set_tm = u_pattern;
				crg_udc->setup_fn_call_back =
					&set_test_mode_cmpl;
			}
			break;
		}

		default:
			goto set_feature_error;
		}

	} else if ((RequestType & (USB_RECIP_MASK | USB_TYPE_MASK)) ==
			(USB_RECIP_INTERFACE | USB_TYPE_STANDARD)) {
		if (crg_udc->device_state != USB_STATE_CONFIGURED) {
			CRG_ERROR("%s interface u12 enable fail, usb state=%d\n",
					__func__, crg_udc->device_state);
			status = -EINVAL;
			goto set_feature_error;
		}

		/* Suspend Option */
		if (value == USB_INTRF_FUNC_SUSPEND) {
			if (index & USB_INTR_FUNC_SUSPEND_OPT_MASK &
				USB_INTRF_FUNC_SUSPEND_LP) {
				if (index & USB_INTRF_FUNC_SUSPEND_RW)
					CRG_DEBUG("Interface En Remote Wakeup\n");
				else
					CRG_DEBUG("Interface Dis RemoteWakeup\n");

				/* Do not need to return status stage here
				 * Pass to composite gadget driver to process
				 * the request
				 */
				return false;
			}
		}
	}

	crg_udc->setup_status = STATUS_STAGE_XFER;
	build_ep0_status(&crg_udc->udc_ep[0], true, status, NULL, 0, 0);
	return true;

set_feature_error:
	set_ep0_halt(crg_udc);
	return true;
}

bool setconfigurationrequest(struct crg_gadget_dev *crg_udc, u16 value)
{
	if (crg_udc->device_state <= USB_STATE_DEFAULT)
		goto set_config_error;

	/*return false means need further process by composite gadget driver*/
	return false;

set_config_error:
	set_ep0_halt(crg_udc);
	return true;
}

void set_isoch_delay(struct crg_gadget_dev *crg_udc,
		 u16 value, u16 index, u16 length)
{
	int status = -EINPROGRESS;

	if (value > 65535 || index != 0 || length != 0)
		status = -EINVAL;

	/*need further ops for isoch delay*/

	crg_udc->setup_status = STATUS_STAGE_XFER;
	build_ep0_status(&crg_udc->udc_ep[0], true, status, NULL, 0, 0);
}

void crg_handle_setup_pkt(struct crg_gadget_dev *crg_udc,
		struct usb_ctrlrequest *setup_pkt, u8 setup_tag)
{
	u16 wValue = setup_pkt->wValue;
	u16 wIndex = setup_pkt->wIndex;
	u16 wLength = setup_pkt->wLength;
	u64 wData = 0;
	unsigned long flags = 0;

	CRG_DEBUG("bRequest=0x%x, wValue=0x%.4x, wIndex=0x%x, wLength=%d\n",
			setup_pkt->bRequest, wValue, wIndex, wLength);

	/* EP0 come backs to running when new setup packet comes*/
	crg_udc->udc_ep[0].ep_state = EP_STATE_RUNNING;

	crg_udc->setup_tag = setup_tag;
	crg_udc->setup_status = SETUP_PKT_PROCESS_IN_PROGRESS;
	crg_udc->setup_fn_call_back = NULL;

	if ((setup_pkt->bRequestType & USB_TYPE_MASK) == USB_TYPE_STANDARD) {
		switch (setup_pkt->bRequest) {
		case USB_REQ_GET_STATUS:
			CRG_DEBUG("USB_REQ_GET_STATUS\n");
			if ((setup_pkt->bRequestType & (USB_DIR_IN |
							USB_TYPE_MASK))
				!= (USB_DIR_IN | USB_TYPE_STANDARD)) {
				crg_udc->setup_status = WAIT_FOR_SETUP;
				return;
			}

			getstatusrequest(crg_udc, setup_pkt->bRequestType,
						wValue, wIndex, wLength);
			return;
		case USB_REQ_SET_ADDRESS:
			CRG_DEBUG("USB_REQ_SET_ADDRESS\n");
			if (setup_pkt->bRequestType != (USB_DIR_OUT |
						USB_RECIP_DEVICE |
						USB_TYPE_STANDARD)) {
				crg_udc->setup_status = WAIT_FOR_SETUP;
				return;
			}

			setaddressrequest(crg_udc, wValue, wIndex, wLength);
			return;
		case USB_REQ_SET_SEL:
			CRG_DEBUG("USB_REQ_SET_SEL\n");

			if (setup_pkt->bRequestType != (USB_DIR_OUT |
						USB_RECIP_DEVICE |
						USB_TYPE_STANDARD)) {
				crg_udc->setup_status = WAIT_FOR_SETUP;
				return;
			}

			crg_udc->setup_status = DATA_STAGE_RECV;
			setselrequest(crg_udc, wValue, wIndex, wLength, wData);
			return;
		case USB_REQ_SET_ISOCH_DELAY:
			if (setup_pkt->bRequestType != (USB_DIR_OUT |
						USB_RECIP_DEVICE |
						USB_TYPE_STANDARD))
				break;

			CRG_DEBUG("USB_REQ_SET_ISOCH_DELAY\n");
			set_isoch_delay(crg_udc, wValue, wIndex, wLength);
			return;

		case USB_REQ_CLEAR_FEATURE:
		case USB_REQ_SET_FEATURE:
			CRG_DEBUG("USB_REQ_CLEAR/SET_FEATURE\n");

			/* Need composite gadget driver
			 * to process the function remote wakeup request
			 */
			if (setfeaturesrequest(crg_udc, setup_pkt->bRequestType,
						setup_pkt->bRequest,
					wValue, wIndex, wLength)) {
				/* Get here if request has been processed.*/
				return;
			}
			break;
		case USB_REQ_SET_CONFIGURATION:
				/*In theory we need to clear RUN bit before
				 *status stage of deconfig request sent.
				 *But seeing problem if we do it before all the
				 *endpoints belonging to the configuration
				 *disabled.
				 */
			CRG_DEBUG("USB_REQ_SET_CONFIGURATION\n");
			CRG_DEBUG("CONFIGURATION wValue=%d\n", wValue);

			if (setconfigurationrequest(crg_udc, wValue)) {
				/* Get here if request has been processed.
				 * Or error happens
				 */
				return;
			}

			if (crg_udc->device_state == USB_STATE_ADDRESS)
				crg_udc->device_state = USB_STATE_CONFIGURED;
			CRG_DEBUG("USB_REQ_SET_CONFIGURATION: device_state is %d\n",
				crg_udc->device_state);
			break;
		default:
			CRG_DEBUG("USB_REQ default bRequest=%d, bRequestType=%d\n",
			 setup_pkt->bRequest, setup_pkt->bRequestType);
		}
	}

	if (wLength) {
		/* data phase from gadget like GET_CONFIGURATION
		 * call the setup routine of gadget driver.
		 * remember the request direction.
		 */
		crg_udc->setup_status =
			(setup_pkt->bRequestType & USB_DIR_IN) ?
			DATA_STAGE_XFER :  DATA_STAGE_RECV;
	}
	spin_unlock_irqrestore(&crg_udc->udc_lock, flags);
	if (crg_udc->gadget_driver->setup(&crg_udc->gadget, setup_pkt) < 0) {
		spin_lock_irqsave(&crg_udc->udc_lock, flags);
		set_ep0_halt(crg_udc);
		return;
	}
	spin_lock_irqsave(&crg_udc->udc_lock, flags);
}

int crg_handle_xfer_event(struct crg_gadget_dev *crg_udc,
			struct event_trb_s *event)
{
	u8 DCI = GETF(EVE_TRB_ENDPOINT_ID, event->dw3);
	struct crg_udc_ep *udc_ep_ptr = &crg_udc->udc_ep[DCI];
	/*Corigine ep contexts start from ep1*/
	u16 comp_code;
	struct crg_udc_request *udc_req_ptr;
	bool trbs_dequeued = false;

	if (!udc_ep_ptr->first_trb ||
		get_ep_state(crg_udc, DCI) == EP_STATE_DISABLED)
		return -ENODEV;

	comp_code = GETF(EVE_TRB_COMPL_CODE, event->dw2);
	if (comp_code == CMPL_CODE_STOPPED ||
		comp_code == CMPL_CODE_STOPPED_LENGTH_INVALID ||
		comp_code == CMPL_CODE_DISABLED ||
		comp_code == CMPL_CODE_DISABLED_LENGTH_INVALID ||
		comp_code == CMPL_CODE_HALTED ||
		comp_code == CMPL_CODE_HALTED_LENGTH_INVALID) {
		CRG_DEBUG("comp_code = %d(STOPPED/HALTED/DISABLED)\n", comp_code);
	} else {
		update_dequeue_pt(event, udc_ep_ptr);
	}

	xdebug("%s ep%d dqpt=0x%p, eqpt=0x%p\n", __func__,
		DCI, udc_ep_ptr->deq_pt, udc_ep_ptr->enq_pt);
	xdebug("comp_code = %d\n", comp_code);

	if (is_request_dequeued(crg_udc, udc_ep_ptr, event)) {
		trbs_dequeued = true;
		CRG_DEBUG("WARNING: Drop the transfer event\n");
		goto queue_more_trbs;
	}

	comp_code = GETF(EVE_TRB_COMPL_CODE, event->dw2);

	switch (comp_code) {
	case CMPL_CODE_SUCCESS:
	{
		xdebug("%s Complete SUCCESS\n", __func__);
		handle_cmpl_code_success(crg_udc, event, udc_ep_ptr);

		/*CRG_DEBUG("%s handle cmpl end\n", __func__);*/
		trbs_dequeued = true;
		break;
	}
	case CMPL_CODE_SHORT_PKT:
	{
		u32 trb_transfer_length;

		xdebug("handle_exfer_event CMPL_CODE_SHORT_PKT\n");
		if (usb_endpoint_dir_out(udc_ep_ptr->desc)) {
			/* CRG_DEBUG("dir out EPIdx = 0x%x\n", DCI); */

			trb_transfer_length = GETF(EVE_TRB_TRAN_LEN,
						event->dw2);
			udc_req_ptr = list_entry(udc_ep_ptr->queue.next,
						struct crg_udc_request, queue);

			udc_req_ptr->usb_req.actual =
				udc_req_ptr->usb_req.length -
				trb_transfer_length;
			/* wmb */
			wmb();
			if (udc_req_ptr->usb_req.actual != 512 &&
				udc_req_ptr->usb_req.actual != 31) {
				u64 trb_pt;
				struct transfer_trb_s *p_trb;

				CRG_DEBUG("Actual Data transferred = %d\n",
					udc_req_ptr->usb_req.actual);

				trb_pt = (u64)event->dw0 +
					((u64)(event->dw1) << 32);

				p_trb = tran_trb_dma_to_virt(udc_ep_ptr, trb_pt);

				CRG_DEBUG("event dw0 = 0x%x\n", event->dw0);
				CRG_DEBUG("event dw1 = 0x%x\n", event->dw1);
				CRG_DEBUG("event dw2 = 0x%x\n", event->dw2);
				CRG_DEBUG("event dw3 = 0x%x\n", event->dw3);

				CRG_DEBUG("trb_pt = 0x%lx, p_trb = 0x%p\n",
					(unsigned long)trb_pt, p_trb);

				CRG_DEBUG("trb dw0 = 0x%x\n", p_trb->dw0);
				CRG_DEBUG("trb dw1 = 0x%x\n", p_trb->dw1);
				CRG_DEBUG("trb dw2 = 0x%x\n", p_trb->dw2);
				CRG_DEBUG("trb dw3 = 0x%x\n", p_trb->dw3);
			}
			req_done(udc_ep_ptr, udc_req_ptr, 0);
		} else {
			CRG_DEBUG("ep dir in\n");
		}

		trbs_dequeued = true;

		/* Advance the dequeue pointer to next TD */
		advance_dequeue_pt(udc_ep_ptr);

		break;
	}

	case CMPL_CODE_PROTOCOL_STALL:
	{
		CRG_DEBUG("%s CMPL_CODE_PROTOCOL_STALL\n", __func__);

		udc_req_ptr = list_entry(udc_ep_ptr->queue.next,
					struct crg_udc_request, queue);
		req_done(udc_ep_ptr, udc_req_ptr, -EINVAL);
		trbs_dequeued = true;
		crg_udc->setup_status = WAIT_FOR_SETUP;
		advance_dequeue_pt(udc_ep_ptr);
		break;
	}

	case CMPL_CODE_SETUP_TAG_MISMATCH:
	{
		u32 enq_idx = crg_udc->ctrl_req_enq_idx;
		struct usb_ctrlrequest *setup_pkt;
		struct crg_setup_packet *crg_setup_pkt;
		u16 setup_tag;

		CRG_ERROR("%s SETUP TAG MISMATCH\n", __func__);
		CRG_DEBUG("NOW setup tag = 0x%x\n", crg_udc->setup_tag);

		/* skip seqnum err event until last one arrives. */
		if (udc_ep_ptr->deq_pt == udc_ep_ptr->enq_pt) {
			udc_req_ptr = list_entry(udc_ep_ptr->queue.next,
					struct crg_udc_request,
					queue);

			if (udc_req_ptr)
				req_done(udc_ep_ptr, udc_req_ptr, -EINVAL);

			/* drop all the queued setup packet, only
			 * process the latest one.
			 */
			crg_udc->setup_status = WAIT_FOR_SETUP;
			if (enq_idx) {
				crg_setup_pkt =
					&crg_udc->ctrl_req_queue[enq_idx - 1];
				setup_pkt = &crg_setup_pkt->usbctrlreq;
				setup_tag = crg_setup_pkt->setup_tag;
				crg_handle_setup_pkt(crg_udc, setup_pkt,
							setup_tag);
				/* flash the queue after the latest
				 * setup pkt got handled..
				 */
				memset(crg_udc->ctrl_req_queue, 0,
					sizeof(struct crg_setup_packet)
					* CTRL_REQ_QUEUE_DEPTH);
				crg_udc->ctrl_req_enq_idx = 0;
			}
		} else {
			CRG_DEBUG("setuptag mismatch skp dpt!=ept: 0x%p, 0x%p\n",
				udc_ep_ptr->deq_pt, udc_ep_ptr->enq_pt);
		}

		crg_udc->setup_tag_mismatch_found = 1;
		CRG_DEBUG("%s SETUP TAG MISMATCH END\n", __func__);
		break;
	}

	case CMPL_CODE_BABBLE_DETECTED_ERR:
	case CMPL_CODE_INVALID_STREAM_TYPE_ERR:
	case CMPL_CODE_RING_UNDERRUN:
	case CMPL_CODE_RING_OVERRUN:
	case CMPL_CODE_ISOCH_BUFFER_OVERRUN:
	case CMPL_CODE_USB_TRANS_ERR:
	case CMPL_CODE_TRB_ERR:
	{
		CRG_ERROR("XFER event err, comp_code = 0x%x\n", comp_code);
		set_ep_halt(crg_udc, DCI);
		break;
	}

	case CMPL_CODE_STOPPED:
	case CMPL_CODE_STOPPED_LENGTH_INVALID:
		/* Any ep stop ops should deal with stopped trbs itselves
		 * Event handler didn't know whether the stopped trb should
		 * be discarded or continued. So we do nothing here
		 */
		CRG_ERROR("STOP, comp_code = 0x%x\n", comp_code);
		break;
	default:
		CRG_DEBUG("CRG UNKNOWN comp_code = 0x%x\n", comp_code);
		CRG_DEBUG("EPDCI = 0x%x\n", udc_ep_ptr->DCI);
		break;
	}

	xdebug("%s 2 ep%d dqpt=0x%p, eqpt=0x%p\n", __func__,
		DCI, udc_ep_ptr->deq_pt, udc_ep_ptr->enq_pt);

queue_more_trbs:
	/* If there are some trbs dequeued by HW and the ring
	 * was full before, then schedule any pending TRB's
	 */
	if (trbs_dequeued && udc_ep_ptr->tran_ring_full) {
		udc_ep_ptr->tran_ring_full = false;
		queue_pending_trbs(udc_ep_ptr);
	}
	return 0;
}

/*temporary solution, this function should be board specific*/
int g_dnl_board_usb_cable_connected(void)
{
	struct crg_gadget_dev *crg_udc;
	struct crg_uccr *uccr;
	u32 tmp;

	crg_udc = &crg_udc_dev;

	uccr = crg_udc->uccr;
	tmp = reg_read(&uccr->portsc);
	if (tmp & CRG_U3DC_PORTSC_PP) {
		mdelay(100);
		tmp = reg_read(&uccr->portsc);
		if (tmp & CRG_U3DC_PORTSC_PP) {
			if (crg_udc->device_state < USB_STATE_POWERED) {
				u32 tmp_cfg0;

				CRG_DEBUG("%s powered, portsc[0x%p]=0x%x\n", __func__,
					&uccr->portsc, tmp);

				/*set usb 3 disable count to 15*/
				tmp_cfg0 = reg_read(&uccr->config0);
				tmp_cfg0 &= (~0xf0);
				tmp_cfg0 |= 0xf0;
				reg_write(&uccr->config0, tmp_cfg0);
				/**/

				mdelay(3);
				crg_udc_start(crg_udc);

				CRG_DEBUG("%s device state powered\n", __func__);
				crg_udc->device_state = USB_STATE_POWERED;
			}
		}
		return 1;
	}

	pdebug("%s no power, portsc[0x%p]=0x%x\n", __func__,
		&uccr->portsc, tmp);

	return 0;
}

int crg_handle_port_status(struct crg_gadget_dev *crg_udc)
{
	struct crg_uccr *uccr = crg_udc->uccr;
	u32 portsc_val;
	u32 tmp;
	u32 speed;
	unsigned long flags = 0;

	/* handle Port Reset */
	portsc_val = reg_read(&uccr->portsc);
	reg_write(&uccr->portsc, portsc_val);

	tmp = reg_read(&uccr->portsc);
	pdebug("%s RAW,portsc[0x%p]=0x%x\n", __func__,
		 &uccr->portsc, portsc_val);
	/*CRG_DEBUG("AFTER ,portsc[0x%p]=0x%x\n",&uccr->portsc, tmp);*/

	if (portsc_val & CRG_U3DC_PORTSC_PRC) {
		mdelay(3);

		tmp = reg_read(&uccr->portsc);
		if (tmp & CRG_U3DC_PORTSC_PRC) {
			pdebug("PRC is still set\n");
		} else if (tmp & CRG_U3DC_PORTSC_PR) {
			/* first port status change event for port reset*/
			pdebug("PRC is not set, but PR is set!!!!!!!!\n");
		} else {
			/* CRG_DEBUG("PR completed. PRC has fell, and PR is low\n"); */
			/* CRG_DEBUG("portsc[0x%p]=0x%x\n", &uccr->portsc, tmp); */
			if (CRG_U3DC_PORTSC_PLS_GET(tmp) != 0 ||
				(!(tmp & CRG_U3DC_PORTSC_PED))) {
				pdebug("portsc[0x%p]=0x%x no PED\n", &uccr->portsc, tmp);
				return 0;
			}

			switch (CRG_U3DC_PORTSC_SPEED_GET(tmp)) {
			case CRG_U3DC_PORTSC_SPEED_SSP:
				speed = USB_SPEED_SUPER_PLUS;
				break;
			case CRG_U3DC_PORTSC_SPEED_SS:
				speed = USB_SPEED_SUPER;
				break;
			case CRG_U3DC_PORTSC_SPEED_HS:
				speed = USB_SPEED_HIGH;
				break;

			case CRG_U3DC_PORTSC_SPEED_FS:
				speed = USB_SPEED_FULL;
				break;

			case CRG_U3DC_PORTSC_SPEED_LS:
				speed = USB_SPEED_LOW;
				break;
			default:
				speed = USB_SPEED_UNKNOWN;
				return 0;
			}

			if (crg_udc->device_state >= USB_STATE_DEFAULT)
				crg_udc_reinit(crg_udc);

			crg_udc->gadget.speed = speed;
			pdebug("gadget speed = 0x%x\n", crg_udc->gadget.speed);

			update_ep0_maxpacketsize(crg_udc);

			crg_udc->connected = 1;

			if (crg_udc->device_state < USB_STATE_RECONNECTING)
				enable_setup(crg_udc);

			pdebug("PORTSC = 0x%x\n", reg_read(&uccr->portsc));
		}
	}
	/* handle Port Reset end */

	/* handle Port Connection Change*/
	/*CRG_DEBUG("%s,portsc[0x%p]=0x%x\n", __func__, &uccr->portsc, tmp);*/
	if (portsc_val & CRG_U3DC_PORTSC_CSC) {
		tmp = reg_read(&uccr->portsc);
		if ((tmp & (CRG_U3DC_PORTSC_CCS | CRG_U3DC_PORTSC_PP)) ==
			(CRG_U3DC_PORTSC_CCS | CRG_U3DC_PORTSC_PP)) {
			pdebug("connect int checked\n");
			pdebug("portsc[0x%p]=0x%x\n", &uccr->portsc, tmp);

			if (CRG_U3DC_PORTSC_PLS_GET(tmp) != 0 ||
				(!(tmp & CRG_U3DC_PORTSC_PED)))
				return 0;

			switch (CRG_U3DC_PORTSC_SPEED_GET(tmp)) {
			case CRG_U3DC_PORTSC_SPEED_SSP:
				speed = USB_SPEED_SUPER_PLUS;
				break;
			case CRG_U3DC_PORTSC_SPEED_SS:
				speed = USB_SPEED_SUPER;
				break;
			case CRG_U3DC_PORTSC_SPEED_HS:
				speed = USB_SPEED_HIGH;
				break;

			case CRG_U3DC_PORTSC_SPEED_FS:
				speed = USB_SPEED_FULL;
				break;

			case CRG_U3DC_PORTSC_SPEED_LS:
			default:
				speed = USB_SPEED_UNKNOWN;
				return 0;
			}
			crg_udc->gadget.speed = speed;

			update_ep0_maxpacketsize(crg_udc);

			crg_udc->connected = 1;

			if (crg_udc->device_state < USB_STATE_RECONNECTING)
				enable_setup(crg_udc);

			pdebug("connect speed = %d\n", speed);

		} else if (!(tmp & CRG_U3DC_PORTSC_CCS)) {
			int cable_connected;

			int ccs_drop_ignore = 0;

			if ((CRG_U3DC_PORTSC_PLS_GET(tmp) == 0x0) &&
				(CRG_U3DC_PORTSC_SPEED_GET(tmp) <
					 CRG_U3DC_PORTSC_SPEED_SS)) {
				ccs_drop_ignore = 1;
				CRG_ERROR("ccs glitch detect on HS/FS!\n");
			}
#ifdef CONFIG_AMLOGIC_USB
			/**eye diagram test fail **/
			if (CRG_U3DC_PORTSC_PLS_GET(tmp) == 0xb)
				ccs_drop_ignore = 1;
#endif
			if (!ccs_drop_ignore)
				crg_udc->gadget.speed = USB_SPEED_UNKNOWN;

			mdelay(150);
			cable_connected = g_dnl_board_usb_cable_connected();
			if (cable_connected && !ccs_drop_ignore) {
				crg_udc->device_state = USB_STATE_POWERED;
				pdebug("do warm reset\n");
				/*if (!crg_udc->setup_tag_mismatch_found) {*/
				if (1) {
					crg_udc_reinit(crg_udc);
					crg_udc->connected = 0;
				} else {
					pdebug("warm reset, rst controller\n");
					crg_udc->setup_tag_mismatch_found = 0;
					crg_udc_reset(crg_udc);

					if (crg_udc->gadget_driver->disconnect) {
						spin_unlock_irqrestore(&crg_udc->udc_lock, flags);
						crg_udc->gadget_driver->disconnect
							(&crg_udc->gadget);
						spin_lock_irqsave(&crg_udc->udc_lock, flags);
					}
					reset_data_struct(crg_udc);
					crg_udc->connected = 0;
					init_ep0(crg_udc);
					crg_udc_start(crg_udc);
					return -ECONNRESET;
				}
			} else if (!cable_connected) {
				pdebug("cable disconnected, rst controller\n");

				crg_udc_reset(crg_udc);
				if (crg_udc->gadget_driver->disconnect) {
					spin_unlock_irqrestore(&crg_udc->udc_lock, flags);
					crg_udc->gadget_driver->disconnect(&crg_udc->gadget);
					spin_lock_irqsave(&crg_udc->udc_lock, flags);
				}

				reset_data_struct(crg_udc);
				crg_udc->connected = 0;
				init_ep0(crg_udc);

				crg_udc->device_state = USB_STATE_ATTACHED;

				/*crg_udc_start(crg_udc);*/
				wake_up_interruptible(&vbus_wait);
				return -ECONNRESET;
			}
		}
	}
	/* handle Port Connection Change end*/

	if (portsc_val & CRG_U3DC_PORTSC_PLC) {
		tmp = reg_read(&uccr->portsc);
		if (CRG_U3DC_PORTSC_PLS_GET(tmp) == 0xf) {
			tmp = SETF_VAR(CRG_U3DC_PORTSC_PLS, tmp, 0x0);
			reg_write(&uccr->portsc, tmp);
			CRG_DEBUG("pls to 0, write portsc 0x%x\n", tmp);
		}  else if (CRG_U3DC_PORTSC_PLS_GET(tmp) == 0x3) {
			if (crg_udc->gadget_driver->disconnect) {
				spin_unlock_irqrestore(&crg_udc->udc_lock, flags);
				if (eltest_flag == 0)
					crg_udc->gadget_driver->disconnect(&crg_udc->gadget);
				spin_lock_irqsave(&crg_udc->udc_lock, flags);
			}
		}
	}

	return 0;
}

int crg_udc_handle_event(struct crg_gadget_dev *crg_udc,
			struct event_trb_s *event)
{
	int ret;

	switch (GETF(EVE_TRB_TYPE, event->dw3)) {
	case TRB_TYPE_EVT_PORT_STATUS_CHANGE:
		if (crg_udc->device_state == USB_STATE_RECONNECTING) {
			crg_udc->portsc_on_reconnecting = 1;
			break;
		}
		/* 1.Port Reset  2.Port Connection Change 3.Port Link Change */
		ret = crg_handle_port_status(crg_udc);
		if (ret)
			return ret;

		break;
	case TRB_TYPE_EVT_TRANSFER:
		if (crg_udc->device_state < USB_STATE_RECONNECTING) {
			CRG_ERROR("Xfer compl event rcved when dev state=%d !\n",
				crg_udc->device_state);
			break;
		}

		crg_handle_xfer_event(crg_udc, event);
		break;
	case TRB_TYPE_EVT_SETUP_PKT:
		{
			struct usb_ctrlrequest *setup_pkt;
			u8 setup_tag;

			CRG_DEBUG("handle_setup_pkt(%d)\n", crg_udc->device_state);

			//if (crg_udc->device_state < USB_STATE_DEFAULT) {
			//	CRG_ERROR("%s state(%d) < DEFAULT!\n",
			//		__func__, crg_udc->device_state);
			//	break;
			//}

			setup_pkt = (struct usb_ctrlrequest *)&event->dw0;

			setup_tag = GETF(EVE_TRB_SETUP_TAG, event->dw3);
			CRG_DEBUG("setup_pkt = 0x%p, setup_tag = 0x%x\n",
				setup_pkt, setup_tag);
			if (crg_udc->setup_status != WAIT_FOR_SETUP) {
				/*previous setup packet hasn't
				 * completed yet. Just ignore the prev setup
				 */
				CRG_ERROR("consecutive setup\n");
				queue_setup_pkt(crg_udc, setup_pkt, setup_tag);
				break;
			}

			crg_handle_setup_pkt(crg_udc, setup_pkt, setup_tag);

			break;
		}
	default:
		CRG_DEBUG("unexpect TRB_TYPE = 0x%x",
			GETF(EVE_TRB_TYPE, event->dw3));
		break;
	}

	return 0;
}

int process_event_ring(struct crg_gadget_dev *crg_udc, int index)
{
	struct event_trb_s *event;
	struct crg_udc_event *udc_event;
	struct crg_uicr *uicr = crg_udc->uicr[index];
	dma_addr_t tmp;
	u32 val;
	dma_addr_t erdp;
	int ret;

	if (!uicr)
		return IRQ_NONE;

	tmp = reg_read(&uicr->iman);
	if ((tmp & (CRG_U3DC_IMAN_INT_EN | CRG_U3DC_IMAN_INT_PEND)) !=
		(CRG_U3DC_IMAN_INT_EN | CRG_U3DC_IMAN_INT_PEND)) {
	}

	val = reg_read(&uicr->iman);
	val |= CRG_U3DC_IMAN_INT_PEND;
	reg_write(&uicr->iman, val);

	udc_event = &crg_udc->udc_event[index];
	while (udc_event->evt_dq_pt) {
		/* rmb */
		rmb();
		event = (struct event_trb_s *)udc_event->evt_dq_pt;

		if (GETF(EVE_TRB_CYCLE_BIT, event->dw3) !=
				udc_event->CCS)
			break;

		ret = crg_udc_handle_event(crg_udc, event);
		if (ret == -ECONNRESET)
			return ret;

		if (event == udc_event->evt_seg0_last_trb) {
			udc_event->CCS = udc_event->CCS ? 0 : 1;
			udc_event->evt_dq_pt = udc_event->event_ring.vaddr;
		} else {
			udc_event->evt_dq_pt++;
		}
	}

	/* update dequeue pointer */
	erdp = event_trb_virt_to_dma(udc_event, udc_event->evt_dq_pt);
	tmp =  upper_32_bits(erdp);
	reg_write(&uicr->erdphi, upper_32_bits(erdp));
	tmp = lower_32_bits(erdp);
	tmp |= CRG_U3DC_ERDPLO_EHB;
	reg_write(&uicr->erdplo, lower_32_bits(erdp | CRG_U3DC_ERDPLO_EHB));

	return 0;
}

int crg_gadget_handle_interrupt(struct crg_gadget_dev *crg_udc)
{
	struct crg_uccr *uccr = crg_udc->uccr;
	u32 tmp_status;
	unsigned long flags = 0;

	spin_lock_irqsave(&crg_udc->udc_lock, flags);

	/******************************/

	tmp_status = reg_read(&uccr->status);

	if (tmp_status & CRG_U3DC_STATUS_SYS_ERR) {
		CRG_ERROR("%s System error happens!!!\n", __func__);
		/*Handle system error*/
		reg_write(&uccr->status, CRG_U3DC_STATUS_SYS_ERR);
	}

	if (tmp_status & CRG_U3DC_STATUS_EINT) {
		int i;

		reg_write(&uccr->status, CRG_U3DC_STATUS_EINT);

		/*process event rings*/
		for (i = 0; i < CRG_RING_NUM; i++)
			process_event_ring(crg_udc, i);
	}

	if (crg_udc->device_state == USB_STATE_RECONNECTING &&
		crg_udc->portsc_on_reconnecting == 1 &&
		is_event_rings_empty(crg_udc)) {
		crg_udc->portsc_on_reconnecting = 0;
		crg_handle_port_status(crg_udc);
	}

	if (crg_udc->device_state == USB_STATE_RECONNECTING &&
		crg_udc->connected == 1) {
		CRG_DEBUG("check if ready for setup\n");
		prepare_for_setup(crg_udc);
	}

	spin_unlock_irqrestore(&crg_udc->udc_lock, flags);

	return 0;
}

static irqreturn_t crg_udc_common_irq(int irq, void *dev)
{
	int retval = 0;

	retval = crg_gadget_handle_interrupt(dev);

	return IRQ_HANDLED;
}

int crg_gadget_irq_init(struct platform_device *pdev)
{
	struct crg_gadget_dev *crg_udc;
	int	irq = 0;
	int retval = 0;

	crg_udc = &crg_udc_dev;

	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
		return -ENODEV;

	crg_udc->irq = irq;

	retval = request_irq(irq, crg_udc_common_irq,
			     IRQF_SHARED, "crg_udc",
			     crg_udc);
	if (retval) {
		CRG_ERROR("request of irq%d failed\n", irq);
		retval = -EBUSY;
	}

	CRG_DEBUG("%s done\n", __func__);

	return 0;
}

int crg_gadget_irq_remove(struct crg_gadget_dev *crg_udc)
{
	free_irq(crg_udc->irq, crg_udc);

	return 0;
}

static ssize_t udc_debug_show(struct device *d, struct device_attribute *attr,
			char *buf)
{
	return sprintf(buf, " ");
}

#define CRG_DEBUG_OFF	0
#define CRG_DEBUG_ON		1
static ssize_t udc_debug_store(struct device *_dev, struct device_attribute *attr,
	const char *buf, size_t size)
{
	unsigned long val;
	int ret = kstrtoul(buf, 0, &val);

	if (ret) {
		CRG_ERROR("is not in hex or decimal form.\n");
	} else if (val == CRG_DEBUG_OFF) {
		crg_debug = 0;
		xfer_debug = 0;
		portsc_debug = 0;
	} else if (val == CRG_DEBUG_ON) {
		crg_debug = 1;
		xfer_debug = 1;
		portsc_debug = 1;
	} else {
		CRG_ERROR("error\n");
	}

	return size;
}

static DEVICE_ATTR_RW(udc_debug);

static int crg_vbus_detect_thread(void *data)
{
	struct crg_gadget_dev *crg_udc = &crg_udc_dev;
	unsigned long flags = 0;
	int cable_connected = 0;

	while (1) {
		if (kthread_should_stop())
			break;

		CRG_DEBUG("crg_udc->device_state is %d\n",
					crg_udc->device_state);

		spin_lock_irqsave(&crg_udc->udc_lock, flags);

		cable_connected = g_dnl_board_usb_cable_connected();

		spin_unlock_irqrestore(&crg_udc->udc_lock, flags);

		if (cable_connected) {
			wait_event_interruptible
				(vbus_wait, crg_udc->device_state < USB_STATE_POWERED);
		} else {
			CRG_DEBUG("wait for vbus\n");
			msleep(1000);
		}
	}

	return 0;
}

void amlogic_crg_m31_phy_init(struct crg_gadget_dev *crg_udc)
{
#define M31_SETTING 0x1E30CEB9
	writel(1, crg_udc->phy_reg_addr + 0x8);
	udelay(9);

	writel(0, crg_udc->phy_reg_addr + 0xc);
	udelay(9);

	writel(0x3, crg_udc->phy_reg_addr + 0x848);
	udelay(9);

	/* to do */
	writel(M31_SETTING, crg_udc->phy_reg_addr);
	udelay(9);
}

/**
 * crg_gadget_probe - Initializes gadget driver
 *
 *
 * Returns 0 on success otherwise negative errno.
 */

#ifdef CONFIG_OF
static const struct of_device_id of_crg_udc_match[] = {
	{
		.compatible = "amlogic, crg_udc"
	},
	{ },
};
MODULE_DEVICE_TABLE(of, of_crg_udc_match);
#endif

static int crg_udc_probe(struct platform_device *pdev)
{
	int ret;
	int i;
	struct crg_gadget_dev *crg_udc;
	int port_speed = USB_PORT_SPEED_DEFAULT;
	void __iomem *phy_reg_addr = NULL;
	unsigned int p_phy_reg_addr = 0;
	unsigned int phy_reg_addr_size = 0;
	int controller_type = USB_NORMAL;
	const struct of_device_id *match;
	struct device_node	*of_node = pdev->dev.of_node;
	const void *prop;
	int retval = 0;
	u32 phy_id = 1;
	u32 version = 0;

	crg_udc = &crg_udc_dev;
	crg_udc->dev = &pdev->dev;

	spin_lock_init(&crg_udc->udc_lock);
	platform_set_drvdata(pdev, crg_udc);

	dev_set_name(&crg_udc->gadget.dev, "gadget");
	crg_udc->gadget.ops = &crg_gadget_ops;
	crg_udc->gadget.ep0 = &crg_udc->udc_ep[0].usb_ep;
	crg_udc->gadget.dev.parent = &pdev->dev;
	INIT_LIST_HEAD(&crg_udc->gadget.ep_list);
	crg_udc->gadget.max_speed = USB_SPEED_HIGH;
	crg_udc->gadget.speed = USB_SPEED_HIGH;// USB_SPEED_UNKNOWN;
	crg_udc->gadget.name = "crg-gadget";

	crg_udc->connected = 0;
	crg_udc->dev_addr = 0;

	if (pdev->dev.of_node) {
		match = of_match_node(of_crg_udc_match, of_node);
		if (match) {
			prop = of_get_property(of_node,
				"controller-type", NULL);
			if (prop)
				controller_type = of_read_ulong(prop, 1);
			crg_udc->controller_type = controller_type;

			prop = of_get_property(of_node, "port-speed", NULL);
			if (prop) {
				port_speed = of_read_ulong(prop, 1);
				crg_udc->gadget.max_speed = port_speed;
				crg_udc->gadget.speed = port_speed;
			}

			retval = of_property_read_u32(of_node, "phy-reg", &p_phy_reg_addr);
			if (retval < 0)
				return -EINVAL;

			retval = of_property_read_u32(of_node, "phy-reg-size", &phy_reg_addr_size);
			if (retval < 0)
				return -EINVAL;

			phy_reg_addr = devm_ioremap_nocache
				(&pdev->dev, (resource_size_t)p_phy_reg_addr,
					(unsigned long)phy_reg_addr_size);
			if (!phy_reg_addr)
				return -ENOMEM;

			crg_udc->phy_reg_addr = phy_reg_addr;

			prop = of_get_property(of_node, "phy-id", NULL);
			if (prop)
				phy_id = of_read_ulong(prop, 1);
			else
				phy_id = 1;
			crg_udc->phy_id = phy_id;
			pdev->id = phy_id;

			prop = of_get_property(of_node, "version", NULL);
			if (prop)
				version = of_read_ulong(prop, 1);
			else
				version = 0;
		}
	}

	if (crg_clk_enable_usb(pdev,
		(unsigned long)phy_reg_addr, controller_type)) {
		dev_err(&pdev->dev, "Set crg_udc PHY clock failed!\n");
		return -ENODEV;
	}

	if (controller_type != USB_M31)
		amlogic_crg_device_usb2_init(phy_id);
	else
		amlogic_crg_m31_phy_init(crg_udc);

	crg_udc->mmio_phys_base = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!crg_udc->mmio_phys_base) {
		dev_err(&pdev->dev, "missing memory resource\n");
		return -ENODEV;
	}

	crg_udc->mmio_virt_base = devm_ioremap_nocache
		(&pdev->dev, crg_udc->mmio_phys_base->start,
			resource_size(crg_udc->mmio_phys_base));
	if (!crg_udc->mmio_virt_base) {
		dev_err(&pdev->dev, "ioremap failed\n");
		return -ENOMEM;
	}

	/* set controller device role*/
	reg_write(crg_udc->mmio_virt_base + 0x20FC,
		(reg_read(crg_udc->mmio_virt_base + 0x20FC) | 0x1));

	if (version == 1) {
		reg_write(crg_udc->mmio_virt_base + 0x2110,
			(reg_read(crg_udc->mmio_virt_base + 0x2110) & (~0x1f)));
	}

	for (i = 0; i < CRG_RING_NUM; i++) {
		crg_udc->uicr[i] = crg_udc->mmio_virt_base +
				CRG_UICR_OFFSET + i * CRG_UICR_STRIDE;

		CRG_DEBUG("crg_udc->uicr[%d] = %x\n", i, crg_udc->uicr[i]);
	}
	crg_udc->uccr = crg_udc->mmio_virt_base + CRG_UCCR_OFFSET;

	crg_udc_reset(crg_udc);

	crg_udc_clear_portpm(crg_udc);

	ret = reset_data_struct(crg_udc);
	if (ret) {
		CRG_ERROR("reset_data_struct error\n");
		goto err0;
	}

	init_ep_info(crg_udc);
	init_ep0(crg_udc);

	EP0_Start(crg_udc);

	crg_gadget_irq_init(pdev);

	ret = usb_add_gadget_udc(&pdev->dev, &crg_udc->gadget);
	if (ret)
		goto err0;

	ret = device_create_file(&pdev->dev, &dev_attr_udc_debug);
	if (ret)
		goto err0;

	/*g_vaddr = dma_alloc_coherent(crg_udc->dev,4096, &g_dma, */
	/*	GFP_KERNEL);*/

	return 0;

err0:
	return -1;
}

static int crg_udc_remove(struct platform_device *pdev)
{
	struct crg_gadget_dev *crg_udc;
	u32 tmp = 0;
	struct crg_uccr *uccr;
	int i = 0;

	crg_udc = &crg_udc_dev;
	uccr = crg_udc->uccr;
	CRG_DEBUG("%s %d called\n", __func__, __LINE__);

	crg_udc->device_state = USB_STATE_ATTACHED;
	crg_vbus_detect(crg_udc, 0);

	device_remove_file(&pdev->dev, &dev_attr_udc_debug);

	usb_del_gadget_udc(&crg_udc->gadget);

	tmp = reg_read(&uccr->control);
	tmp |= CRG_U3DC_CTRL_SWRST;
	reg_write(&uccr->control, tmp);

	/* set controller host role*/
	tmp = reg_read(crg_udc->mmio_virt_base + 0x20FC) & ~0x1;
	reg_write(crg_udc->mmio_virt_base + 0x20FC, tmp);

	if (crg_udc->irq)
		free_irq(crg_udc->irq, crg_udc);

	if (crg_udc->controller_type != USB_M31)
		amlogic_crg_device_usb2_shutdown(crg_udc->phy_id);

	crg_clk_disable_usb(pdev, (unsigned long)crg_udc->phy_reg_addr);
	/*
	 * Clear the drvdata pointer.
	 */
	platform_set_drvdata(pdev, 0);

	for (i = 0; i < CRE_REQ_NUM; i++)
		atomic_set(&g_udc_req_ptr[i].used, 0);
	memset(&crg_udc_dev, 0, sizeof(struct crg_gadget_dev));

	CRG_DEBUG("%s %d gadget remove\n", __func__, __LINE__);

	return 0;
}

static void crg_udc_shutdown(struct platform_device *pdev)
{
	struct crg_gadget_dev *crg_udc = &crg_udc_dev;

	CRG_DEBUG("%s %d called\n", __func__, __LINE__);

	crg_udc->device_state = USB_STATE_ATTACHED;
	crg_vbus_detect(crg_udc, 0);
	usb_del_gadget_udc(&crg_udc->gadget);

	if (crg_udc->irq)
		free_irq(crg_udc->irq, crg_udc);

	if (crg_udc->controller_type != USB_M31)
		amlogic_crg_device_usb2_shutdown(crg_udc->phy_id);

	crg_clk_disable_usb(pdev, (unsigned long)crg_udc->phy_reg_addr);

	/*
	 * Clear the drvdata pointer.
	 */
	platform_set_drvdata(pdev, 0);
}

/*Now we can support multi corigine device controllers*/
/*But if we need to support multi device controllers of various vendors*/
/*Gadget framework needs to be improved*/
int usb_gadget_handle_interrupts(int index)
{
	int ret = 0;

	struct crg_gadget_dev *crg_udc;

	crg_udc = &crg_udc_dev;

	if (!crg_udc->gadget_driver)
		return -ENODEV;

	return ret;
}

#ifdef CONFIG_PM
static int crg_udc_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct crg_gadget_dev *crg_udc;

	crg_udc = &crg_udc_dev;

	if (crg_udc->controller_type != USB_M31)
		amlogic_crg_device_usb2_shutdown(crg_udc->phy_id);

	crg_clk_disable_usb(pdev, (unsigned long)crg_udc->phy_reg_addr);
	return 0;
}

static int crg_udc_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct crg_gadget_dev *crg_udc;

	crg_udc = &crg_udc_dev;
	crg_clk_enable_usb(pdev,
			(unsigned long)crg_udc->phy_reg_addr,
			crg_udc->controller_type);

	if (crg_udc->controller_type != USB_M31)
		amlogic_crg_device_usb2_init(crg_udc->phy_id);

	return 0;
}
#else
#define crg_udc_suspend	NULL
#define crg_udc_resume	NULL
#endif

static const struct dev_pm_ops crg_udc_pm_ops = {
	.suspend = crg_udc_suspend,
	.resume = crg_udc_resume,
};

static struct platform_driver crg_udc_driver = {
	.probe = crg_udc_probe,
	.remove = crg_udc_remove,
	.shutdown = crg_udc_shutdown,
	.driver			= {
		.name		= "crg_udc",
		.owner		= THIS_MODULE,
		.pm			= &crg_udc_pm_ops,
		.of_match_table	= of_match_ptr(of_crg_udc_match),
	},
};

void crg_gadget_exit(void)
{
	CRG_ERROR("crg gadget exit\n");
	if (!crg_udc_driver.driver.p)
		return;
	platform_driver_unregister(&crg_udc_driver);
}

int crg_gadget_init(void)
{
	CRG_ERROR("crg gadget init\n");

	return platform_driver_register(&crg_udc_driver);
}

MODULE_DESCRIPTION("Corigine USB Device Controller");
MODULE_LICENSE("GPL");
