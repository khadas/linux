/*
 * Source file for DHD OS Logger implmentation on Linux
 * This is a OS dependent file
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
 *
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/inetdevice.h>
#include <linux/rtnetlink.h>
#include <linux/etherdevice.h>
#include <linux/spinlock.h>
#include <linux/ethtool.h>
#include <linux/ip.h>
#include <linux/notifier.h>
#include <net/addrconf.h>
#include <linuxver.h>
#include <osl.h>
#include <dhd_linux.h>
#include <dhd_logger.h>
#include <dhd_os_logger.h>
#include <dhd_dbg.h>
#include <bcmutils.h>
#include <ethernet.h>
#include <bcmstdlib_s.h>

/*
 * DHD OS logger structure, private to Linux Logger module.
 * The rest of the DHD code need not know the implementation
 * Similarly, Logger module does not need access to any of the
 * DHD structures, only OSL Handle is needed. So during attach
 * osh should be populated into dhd_os_logger_info.
 */
struct dhd_os_logger_info
{
	osl_t *osh;	/* OSL Handle	*/
	struct net_device *logger_netdev; /* DHD logger interface net dev */

	/*
	 * Filter to enable/disable logging of each type
	 * Each bit poisition for type defined in dhd_log_type
	 * If the corresponding bit position for type is 1, logging is enabled
	 * If the corresponding bit position for type is 0, logging is disabled
	 */
	uint32  filter;

	/*
	 * free_q1 - List of skbs of size FREE_Q1_BUFSIZE each.
	 * free_q2 - List of skbs of size FREE_Q2_BUFSIZE each.
	 *           8K for IOVAR request/response and 256 bytes for logger headers.
	 *
	 * Producer of free_q1, free_q2:
	 * ----------------------------
	 * During "attach" phase these two queues are filled
	 * When the q1len or q2len falls below a "threshold" a kthread context is
	 * woken up to fill the queues.
	 *
	 * Consumer of free_q1,free_q2:
	 * --------------------------
	 * When the API to "log" a debug data is called, an skb is taken from
	 * free_q1 or free_q2 based on the "size" requested. after filling the data
	 * its added to ready_q.
	 *
	 * Producer of ready_q:
	 * -------------------
	 * As mentioned above the API dhd_log or dhd_pkt_log
	 *
	 * Consumer of ready_q:
	 * -------------------
	 * The context that sends the debug data available in the ready_q via "logger"
	 * interface using netlink sockets. This context actually appends the ready_q
	 * to "sendup_q" so that while its sending the debug data up, the system continues
	 * to log into ready_q without log contention (without memory cost)
	 *
	 */
	struct sk_buff_head   free_q1    ____cacheline_aligned;
	struct sk_buff_head   free_q2    ____cacheline_aligned;
	struct sk_buff_head   ready_q    ____cacheline_aligned;
	struct sk_buff_head   sendup_q   ____cacheline_aligned;

	/* Variables to hold the length of the queues */
	uint32 q1len;
	uint32 q2len;
	uint32 ready_qlen;
	uint32 sendup_qlen;
	/* counters */
	uint32 flowctrl_hitcnt;

	/* sendup kthread context, to send the logged packets to userspace sockets */
	tsk_ctl_t sendup_kthread;
	/*
	 * Fill buffer kthread context. This context allocates the skbs in thread context
	 * and keeps ready when the callers wants to log the IOCTL, IPC ..etc.
	 */
	tsk_ctl_t fill_buff_kthread;
	/* sysfs control to dump debug information to dmesg */
	uint32 qdump;
};

static void dhd_os_log_sendup(dhd_os_logger_t *poslg, struct sk_buff *p);
#define DHD_LOOGER_IFNAME	"logger"
/*
 * Queue management support to add the packet to readyq and send to stack from sendup context
 */
#define LOG_READYQ_SENDUPQ
/*
 * 1. Queue management support to mantain a pool of buffers
 *    of size FREE_Q1_BUFSIZE and FREE_Q2_BUFSIZE.
 * 2. The same will be added to readyq after consuming the packet.
 */
#define LOG_FREEQ_READYQ
/* When skbs are not available reschedule delays and max attempts */
#define LOGGER_FREEQ_RESCHED_DELAY_MS 500u
#define LOGGER_FREEQ_FETCH_MAX_ATTEMPTS 10u

#define FREE_Q1_LEN_MAX 512u
#define FREE_Q1_LEN_THR 256u
#define FREE_Q1_LEN_MIN 2u
#define FREE_Q1_BUFSIZE (2u * 1024u)

#define FREE_Q2_LEN_MAX 64u
#define FREE_Q2_LEN_THR 32u
/* 8K for IOCTL response and 256 bytes for logger headers */
#define FREE_Q2_BUFSIZE ((8u * 1024u) + 256u)

/* Flow control */
#define READYQ_SENDUPQ_MAX (10u * 1024u)

/*
 * If the bit is set dump the corresponding queue trace to dmesg.
 * sysfs has control for which queue to be dumped or not.
 */
#define DUMP_FREEQ	0x1u
#define DUMP_READYQ	0x2u
#define DUMP_SENDUPQ	0x4u

#ifdef DUMP_SKBQ

/**
 * Dump freeq1 skbs
 *
 * @param[poslg] Pointer to context structure
 * @return  void
 */
static void
dhd_os_log_dump_freeq1(dhd_os_logger_t *poslg)
{
	/*
	 * 1. Lock is needed to protect against the dhd_os_log context while dequeuing the
	 *    buffer from freeq1. Also to pretect against list operations on freeq1 from
	 *    dhd_os_log_fill_buffer_kthread() context to fill the buffers.
	 * 2. Dump only if the bit DUMP_FREEQ is set
	 */
	if (skb_queue_len(&poslg->free_q1) && (poslg->qdump & DUMP_FREEQ)) {
		spin_lock_bh(&poslg->free_q1.lock);
		dhd_os_skbq_dump(&poslg->free_q1, "free_q1");
		spin_unlock_bh(&poslg->free_q1.lock);
	}
}

/**
 * Dump freeq2 skbs
 *
 * @param[poslg] Pointer to context structure
 * @return  void
 */
static void
dhd_os_log_dump_freeq2(dhd_os_logger_t *poslg)
{
	/*
	 * 1. Lock is needed to protect against the dhd_os_log context while dequeuing the
	 *    buffer from freeq2. Also to pretect against list operations on freeq2 from
	 *    dhd_os_log_fill_buffer_kthread() context to fill the buffers.
	 * 2. Dump only if the bit DUMP_FREEQ is set
	 */
	if (skb_queue_len(&poslg->free_q2) && (poslg->qdump & DUMP_FREEQ)) {
		spin_lock_bh(&poslg->free_q2.lock);
		dhd_os_skbq_dump(&poslg->free_q2, "free_q2");
		spin_unlock_bh(&poslg->free_q2.lock);
	}
}

/**
 * Dump readyq skbs
 *
 * @param[poslg] Pointer to context structure
 * @return  void
 */
static void
dhd_os_log_dump_readyq(dhd_os_logger_t *poslg)
{
	/*
	 * 1. Lock is needed to protect against the sendupq from splicing the readq to sendupq tail
	 * 2. Dump only if the bit DUMP_READYQ is set
	 */
	if (skb_queue_len(&poslg->ready_q) && (poslg->qdump & DUMP_READYQ)) {
		spin_lock_bh(&poslg->ready_q.lock);
		dhd_os_skbq_dump(&poslg->ready_q, "ready_q");
		spin_unlock_bh(&poslg->ready_q.lock);
	}
}

/**
 * Dump sendupq skbs.
 *
 * @param[poslg] Pointer to context structure
 * @return  void
 */
static void
dhd_os_log_dump_sendupq(dhd_os_logger_t *poslg)
{
	 /*
	  * Dump only if the bit DUMP_SENDUPQ is set
	  */
	if (skb_queue_len(&poslg->sendup_q) && (poslg->qdump & DUMP_SENDUPQ)) {
		dhd_os_skbq_dump(&poslg->sendup_q, "sendup_q");
	}
}

/**
 * Dump queuex lengths to dmesg.
 */
static void
dhd_os_log_dump_counters(dhd_os_logger_t *poslg, uint32 pktlen, uint32 readyq_sendupq_len)
{
	DHD_PRINT(("%s() pktlen=%d\n"
		"q1len=%d q2len=%d ready_qlen=%d sendup_qlen=%d redyq_sendupq_len=%d\n"
		"flowctrl_hitcnt=%d\n",
		__FUNCTION__, pktlen,
		poslg->q1len, poslg->q2len, poslg->ready_qlen, poslg->sendup_qlen,
		readyq_sendupq_len,
		poslg->flowctrl_hitcnt));
}
#else
/* Empty functions when DUMP_SKBQ is not defined(default) */

static void
dhd_os_log_dump_freeq1(dhd_os_logger_t *poslg)
{

}

static void
dhd_os_log_dump_freeq2(dhd_os_logger_t *poslg)
{

}

static void
dhd_os_log_dump_readyq(dhd_os_logger_t *poslg)
{

}

static void
dhd_os_log_dump_sendupq(dhd_os_logger_t *poslg)
{

}

static void
dhd_os_log_dump_counters(dhd_os_logger_t *poslg, uint32 pktlen, uint32 readyq_sendupq_len)
{

}
#endif /* DUMP_SKBQ */

/**
 * Given an skb log/send to network stack.
 * This is lockless funtion. Any race has to be taken care by the caller.
 *
 * @param[poslg] Pointer to context structure
 * @param[p] Pointer to the skb to send up to network stack
 * @return  void
 */
static void
dhd_os_log_sendup(dhd_os_logger_t *poslg, struct sk_buff *p)
{
	struct sk_buff *skb;
	int len;
	uchar *skb_data;

	skb = PKTTONATIVE(poslg->osh, p);
	skb_data = skb->data;
	len = PKTLEN(poslg->osh, p);
	ASSERT(poslg->logger_netdev);
	skb->dev = poslg->logger_netdev;
	skb->protocol = eth_type_trans(skb, skb->dev);
	skb->data = skb_data;
	PKTSETLEN(poslg->osh, skb, len);

	/* Strip header, count, deliver upward */
	skb_pull(skb, ETH_HLEN);

	bcm_object_trace_opr(skb, BCM_OBJDBG_REMOVE,
			__FUNCTION__, __LINE__);
	/* Send the packet up */
	if (in_interrupt()) {
		netif_rx(skb);
	} else {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 18, 0))
		netif_rx(skb);
#else
		netif_rx_ni(skb);
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(5, 18, 0) */
	}
}

/**
 * Called to log debug data
 *
 * @param[poslg]  Pointer to context structure
 * @param[type] Type of the data to log (one of dhd_log_type)
 * @param[header] Pointer to the header of the debug buffer to be logged
 * @param[header_len] Length if the header
 * @param[buf]  pointer to debug buffer to be logged
 * @param[len]  length of debug buffer
 * @return      BCME_OK on success / Error code on failure
 */
int32
dhd_os_log(dhd_os_logger_t *poslg, uint32 type, const void *header,
	int header_len, const void *data, uint32 data_len)
{
	struct sk_buff *skb;
	uint32 readyq_sendupq_len;
	uint32 pktlen;
	int ret = 0;

	BCM_REFERENCE(readyq_sendupq_len);
	if (poslg == NULL) {
		DHD_ERROR(("%s() poslg is NULL\n", __FUNCTION__));
		return BCME_NOTREADY;
	}
	pktlen = header_len + data_len;
#ifdef LOG_FREEQ_READYQ
	poslg->q1len = skb_queue_len(&poslg->free_q1);
	poslg->q2len = skb_queue_len(&poslg->free_q2);
	poslg->ready_qlen = skb_queue_len(&poslg->ready_q);
	poslg->sendup_qlen = skb_queue_len(&poslg->sendup_q);
	readyq_sendupq_len = poslg->ready_qlen + poslg->sendup_qlen;

	dhd_os_log_dump_counters(poslg, pktlen, readyq_sendupq_len);

	/* Flow control, to be fair in consuming the resources.
	 * If the ready_q and sendup_q grows beyond a threshold, network stack is not
	 * consuming the packets fast. i.e consumer is slower that producer.
	 * Hence drop the log buffer and update the statistics.
	 */
	if (readyq_sendupq_len >= READYQ_SENDUPQ_MAX) {
		poslg->flowctrl_hitcnt++;
		DHD_ERROR(("%s() flow contol has hit. readyq_sendupq_len: %d flowctrl_hitcnt: %d\n",
			__FUNCTION__, readyq_sendupq_len, poslg->flowctrl_hitcnt));
		return BCME_BUSY;
	}
	/*
	 * If the number of free buffers falls below the corresponding threshold
	 * of free_q1 or free_q2, inform the context by raising a semaphore
	 * to allocate the buffers.
	 */
	if ((poslg->q1len < FREE_Q1_LEN_THR) ||
		(poslg->q2len < FREE_Q2_LEN_THR)) {
		binary_sema_up(&poslg->fill_buff_kthread);
	}
	/*
	 * 1. Based on the "pktlen" take an skb from free_q1 or free_q2
	 * 2. dhd_os_log_fill_buffer_kthread context could be doing the
	 *    free_q1 or free_q2 list operations. skb_dequeue() will take the lock.
	 */
	if (pktlen <= FREE_Q1_BUFSIZE) {
		skb = skb_dequeue(&poslg->free_q1);
	} else if (pktlen <= FREE_Q2_BUFSIZE) {
		skb = skb_dequeue(&poslg->free_q2);
	} else {
		DHD_ERROR(("%s pktlen=%d out of bound\n", __FUNCTION__, pktlen));
		ASSERT(0);
		return BCME_ERROR;
	}
#else
	/* allocate skb buffer */
	skb = PKTGET(poslg->osh, pktlen, FALSE);
#endif /* LOG_FREEQ_READYQ */
	if (skb == NULL) {
		/* Could not allocate a sk_buf or get from the free_qx */
		DHD_ERROR(("%s: unable to get sk_buf for pktlen: %d\n", __FUNCTION__, pktlen));
		return BCME_NOMEM;
	}
	ASSERT(ISALIGNED((uintptr)PKTDATA(poslg->osh, skb), sizeof(uint32)));

	/* Copy both header and data to skb->data */
	ret = memcpy_s(PKTDATA(poslg->osh, skb), PKTLEN(poslg->osh, skb), header, header_len);
	if (ret != 0) {
		DHD_ERROR(("%s() memcpy header error\n", __FUNCTION__));
		goto exit;
	}
	if (data_len != 0) {
		ret = memcpy_s(PKTDATA(poslg->osh, skb) + header_len,
				PKTLEN(poslg->osh, skb) - header_len, data, data_len);
		if (ret != 0) {
			DHD_ERROR(("%s() memcpy data error\n", __FUNCTION__));
			goto exit;
		}
	}
	PKTSETLEN(poslg->osh, skb, pktlen);

#ifdef LOG_READYQ_SENDUPQ
	/* Put the skb in ready_q */
	skb_queue_tail(&poslg->ready_q, skb);
	/* to log readyq */
	dhd_os_log_dump_readyq(poslg);
	/* Inform the context to send the debug data up by raising a semaphore even for a packet
	 * Ex:- Imagine user enabled only IOCTL logs to be sentup and fired only one wl command.
	 * An IOCTL if succeed will have only two log packets.
	 * So we cannot wait for N number of packets to accumilate and then sendup.
	 */
	binary_sema_up(&poslg->sendup_kthread);
#else
	/* without readyq and sendupq, to send debug data to network stack */
	dhd_os_log_sendup(poslg, skb);
#endif /* LOG_READYQ_SENDUPQ */

	return BCME_OK;
exit:
	return ret;
}

/**
 * Called to route events
 *
 * @param[poslg]  Pointer to context structure
 * @param[buf]  pointer to network buffer
 * @param[len]  length of data in network data buffer
 * @return      BCME_OK on success / Error code on failure
 */
int32
dhd_os_log_route_events(dhd_os_logger_t *poslg, void *pktbuf, uint32 pktlen)
{
	void *npktbuf = NULL;

#ifdef DHD_USE_STATIC_CTRLBUF
	npktbuf = pskb_copy((struct sk_buff *)pktbuf, GFP_ATOMIC);
#else
	npktbuf = skb_clone((struct sk_buff *)pktbuf, GFP_ATOMIC);
#endif /* DHD_USE_STATIC_CTRLBUF */
	if (npktbuf == NULL) {
		DHD_ERROR(("%s clone of skb failed\n", __FUNCTION__));
		return BCME_NOMEM;
	}
#ifdef LOG_READYQ_SENDUPQ
	/* Put the skb in ready_q */
	skb_queue_tail(&poslg->ready_q, npktbuf);
	/* to log readyq */
	dhd_os_log_dump_readyq(poslg);
	/* Inform the context to send the data up by raising a semaphore */
	binary_sema_up(&poslg->sendup_kthread);
#else
	/* without readyq and sendupq, to send debug data to network stack */
	dhd_os_log_sendup(poslg, npktbuf);
#endif /* LOG_READYQ_SENDUPQ */
	return BCME_OK;
}

/**
 * Called to log eventlogs through infobuf
 *
 * @param[poslg]  Pointer to context structure
 * @param[buf]  pointer to network buffer
 * @param[len]  length of data in network data buffer
 * @param[edl]  true if edl or false if info buf
 * @return      BCME_OK on success / Error code on failure
 */
int32
dhd_os_log_infobuf_eventlogs(dhd_os_logger_t *poslg,
	const void *header, int header_len, struct sk_buff *pktbuf)
{
	struct sk_buff *npktbuf = NULL;
	int ret = 0;

#ifdef DHD_USE_STATIC_CTRLBUF
	npktbuf = pskb_copy((struct sk_buff *)pktbuf, GFP_KERNEL);
#else
	npktbuf = skb_clone((struct sk_buff *)pktbuf, GFP_KERNEL);
#endif /* DHD_USE_STATIC_CTRLBUF */
	if (npktbuf == NULL) {
		DHD_ERROR(("%s clone of skb failed\n", __FUNCTION__));
		return BCME_NOMEM;
	}
	if (skb_headroom(npktbuf) < header_len) {
		DHD_ERROR(("%s insufficient headroom(%d) to push event log header. header_len=%d\n",
			__FUNCTION__, skb_headroom(npktbuf), header_len));
		ASSERT(0);
		return BCME_ERROR;
	}
	skb_push(npktbuf, header_len);
	/* Copy header skb->data */
	ret = memcpy_s(PKTDATA(poslg->osh, npktbuf), header_len, header, header_len);
	if (ret != 0) {
		DHD_ERROR(("%s() memcpy error\n", __FUNCTION__));
		goto exit;
	}
#ifdef LOG_READYQ_SENDUPQ
	/* Put the skb in ready_q */
	skb_queue_tail(&poslg->ready_q, npktbuf);
	/* to log readyq */
	dhd_os_log_dump_readyq(poslg);
	/* Inform the context to send the data up by raising a semaphore */
	binary_sema_up(&poslg->sendup_kthread);
#else
	/* without readyq and sendupq, to send debug data to network stack */
	dhd_os_log_sendup(poslg, npktbuf);
#endif /* LOG_READYQ_SENDUPQ */
exit:
	return BCME_OK;
}

/**
 * Called to log network packets
 *
 * @param[poslg]  Pointer to context structure
 * @param[type] Type of the data to log (one of dhd_log_type)
 * @param[buf]  pointer to network buffer
 * @param[len]  length of data in network data buffer
 * @return      BCME_OK on success / Error code on failure
 */
int32
dhd_os_log_pkt(dhd_os_logger_t *poslg, uint32 type, void *pkt, uint32 len)
{

	/* Clone the pkt and add to ready_q */
	return BCME_OK;
}

#ifdef LOG_READYQ_SENDUPQ
/**
 * dhd_os_log_sendup_kthread - Context takes the lock attaches the ready list to tail of a
 * process list, releases the lock of ready list and starts sending the content
 * of process list to debug interface
 *
 * @param[ctxt]  Pointer to the context
 * @return       BCME_OK
 */
static int32
dhd_os_log_sendup_kthread(void *ctxt)
{
	tsk_ctl_t *tsk = (tsk_ctl_t *)ctxt;
	dhd_os_logger_t *poslg = (dhd_os_logger_t *)tsk->parent;
	struct sk_buff *skb = NULL;

	do {
		/* Wait on a semaphore */
		if (!binary_sema_down(tsk)) {
			SMP_RD_BARRIER_DEPENDS();
			/* Check terminated before processing the items */
			if (tsk->terminated) {
				DHD_ERROR(("%s: task terminated\n", __FUNCTION__));
				goto exit;
			}
			/*
			 * The context that calls dhd_log to add data into ready_q can be
			 * from Bottom half, so use _bh variant
			 */
			spin_lock_bh(&poslg->ready_q.lock);
			skb_queue_splice_tail_init(&poslg->ready_q, &poslg->sendup_q);
			spin_unlock_bh(&poslg->ready_q.lock);

			/* to log sendupq */
			dhd_os_log_dump_sendupq(poslg);
			/*
			 * Walk through sendup_q in a while loop till its empty and send up
			 * to logger interface
			 */
			while ((skb = __skb_dequeue(&poslg->sendup_q))) {
				dhd_os_log_sendup(poslg, skb);
			}
		} else {
			DHD_ERROR_RLMT(("%s: binary_sema_down unexpected error break\n",
					__FUNCTION__));
			break;
		}

	} while (1);

exit:
	DHD_INFO(("%s: exit\n", __FUNCTION__));
	KTHREAD_COMPLETE_AND_EXIT(&tsk->completed, 0);
	return BCME_OK;
}
#endif /* LOG_READYQ_SENDUPQ */

#ifdef LOG_FREEQ_READYQ
/**
 * dhd_os_log_fill_free_qx - Allocate qx_num_skbs
 *
 * @param[ctxt]		Pointer to the context
 * @param[qx]		Pointer to the queuex head
 * @param[qx_num_skbs]	Number of skb buffers requested to allocate
 * @param[qx_buffsize]	Size of the skb buffer to be allocated
 * @param[qx_name]	Name of the queue
 * @return		Number of skb buffers allocated
 */
static uint32
dhd_os_log_fill_free_qx(dhd_os_logger_t *poslg, struct sk_buff_head *qx,
	int32 qx_num_skbs, uint16 qx_buffsize, char *qx_name)
{
	struct sk_buff *pskb;
	uint num_attempts = 0;

	DHD_TRACE(("%s: before alloc, q_name=%s q_num_skbs=%d q_len=%u, q_buffsize=%u\n",
		__FUNCTION__, qx_name, qx_num_skbs, skb_queue_len(qx), qx_buffsize));

	num_attempts = 0;
	while (qx_num_skbs > 0) {
		pskb = PKTGET(poslg->osh, qx_buffsize, FALSE);
		if (!pskb) {
			DHD_ERROR_RLMT(("%s: pktget failed, resched...\n", __FUNCTION__));
			/* retry after some time to fetch packets
			 * if maximum attempts hit, stop
			 */
			num_attempts++;
			if (num_attempts >= LOGGER_FREEQ_FETCH_MAX_ATTEMPTS) {
				DHD_ERROR_RLMT(("%s: max attempts to fetch exceeded.\n",
					__FUNCTION__));
				break;
			}
			OSL_SLEEP(LOGGER_FREEQ_RESCHED_DELAY_MS);
		} else {
			skb_queue_tail(qx, pskb);
		}
		qx_num_skbs--;
	}
	DHD_TRACE(("%s: After alloc, q_name=%s q_num_skbs: %d q_len=%u\n",
		__FUNCTION__, qx_name, qx_num_skbs, skb_queue_len(qx)));

	return skb_queue_len(qx);
}

/**
 * dhd_log_fill_buffer - Context that adds the skbs to free_q1 and free_q2
 *
 * @param[ctxt]  Pointer to the context
 * @return       BCME_OK
 */
static int32
dhd_os_log_fill_buffer_kthread(void *ctxt)
{
	tsk_ctl_t *tsk = (tsk_ctl_t *)ctxt;
	dhd_os_logger_t *poslg = (dhd_os_logger_t *)tsk->parent;
	struct sk_buff_head   alloc_q;
	uint32 q1_num_skbs; /* number of skbs to be filled in q1 */
	uint32 q2_num_skbs; /* number of skbs to be filled in q2 */

	do {
		/* Wait on a semaphore */
		if (!binary_sema_down(tsk)) {
			SMP_RD_BARRIER_DEPENDS();
			/* Check if we are asked to break from work loop and return if so .. */
			if (tsk->terminated) {
				DHD_ERROR(("%s: task terminated\n", __FUNCTION__));
				goto exit;
			}
			/* Find out how many buffers to allocate for q1 */
			q1_num_skbs = FREE_Q1_LEN_MAX - skb_queue_len(&poslg->free_q1);

			if (q1_num_skbs) {
				skb_queue_head_init(&alloc_q);
				/*
				 * For free_q1, allocate in a while loop as many buffers needed
				 * and add to the local list alloc_q
				 */
				dhd_os_log_fill_free_qx(poslg, &alloc_q,
						q1_num_skbs,
						FREE_Q1_BUFSIZE, "free_q1");

				/*
				 * 1. Now that we have needed buffer, lets add it to free_q1
				 * 2. The context that calls dhd_os_log takes a skb from free_q1
				 *    can be from Bottom half, so use _bh variant
				 */
				spin_lock_bh(&poslg->free_q1.lock);
				skb_queue_splice_tail_init(&alloc_q, &poslg->free_q1);
				spin_unlock_bh(&poslg->free_q1.lock);

				/* dump freeq1 skb addresses to dmesg */
				dhd_os_log_dump_freeq1(poslg);
			}

			/* Find out how many buffers to allocate for q2 */
			q2_num_skbs = FREE_Q2_LEN_MAX - skb_queue_len(&poslg->free_q2);
			if (q2_num_skbs) {
				skb_queue_head_init(&alloc_q);
				/*
				 * For free_q2, allocate in a while loop as many buffers needed
				 * and add to the local list alloc_q
				 */
				dhd_os_log_fill_free_qx(poslg, &alloc_q,
						q2_num_skbs,
						FREE_Q2_BUFSIZE, "free_q2");

				/*
				 * 1. Now that we have needed buffer, lets add it to free_q2
				 * 2. The context that calls dhd_os_log takes a skb from free_q2
				 *    can be from Bottom half, so use _bh variant
				 */
				spin_lock_bh(&poslg->free_q2.lock);
				skb_queue_splice_tail_init(&alloc_q, &poslg->free_q2);
				spin_unlock_bh(&poslg->free_q2.lock);

				/* dump freeq2 skb addresses to dmesg */
				dhd_os_log_dump_freeq2(poslg);
			}

		}
	} while (1);

exit:
	DHD_INFO(("%s: exit\n", __FUNCTION__));
	KTHREAD_COMPLETE_AND_EXIT(&tsk->completed, 0);
	return BCME_OK;
}
#endif /* LOG_FREEQ_READYQ */

/* Sysfs control APIs */
/**
 * Called to get the queue debug dump level being set
 *
 * @param[pdl]  Pointer to context structure
 * @return      queue debug dump level
 */
uint32
dhd_os_log_get_qdump(dhd_os_logger_t *poslg)
{
	return poslg->qdump;
}

/**
 * Called to set the queue debug dump level
 *
 * @param[pdl]  Pointer to context structure
 * @param[qdump] queue/queues to dump
 * @return      BCME_OK on success/error code on failure
 */
int32
dhd_os_log_set_qdump(dhd_os_logger_t *poslg, int32 qdump)
{
	poslg->qdump = qdump;
	return BCME_OK;
}

/**
 * Called when ifconfig up or an equivalent event happens that
 * readies the network interface for operation
 *
 * @param[poslg] Pointer to context structure
 * @return     BCME_OK on success / Error code on failure
 */
int32
dhd_os_logger_init(dhd_os_logger_t *poslg)
{
	/* Create the dhd logger Network interface */

	/* TODO: Should some steps from attach be moved here ?
	 * Prefer to keep allocations in attach itself
	 */

	return BCME_OK;
}

/**
 * Called when ifconfig down or an equivalent event happens that
 * retires the network interface for operation
 *
 * @param[poslg] Pointer to context structure
 * @return     BCME_OK on success / Error code on failure
 */
int32
dhd_os_logger_deinit(dhd_os_logger_t *poslg)
{
	/* Remove the dhd logger Network interface */
	return BCME_OK;
}

/* logger interface open callback */
static int
dhd_os_logger_open(struct net_device *net)
{
	DHD_TRACE(("(%s) DHD Logger iface open \n", __FUNCTION__));
	return 0;
}

/* logger interface close callback */
static int
dhd_os_logger_close(struct net_device *net)
{
	DHD_TRACE(("(%s) DHD Logger iface close \n", __FUNCTION__));
	return 0;
}

/* logger interface tx callback */
static netdev_tx_t
dhd_os_logger_start_xmit(struct sk_buff *skb, struct net_device *ndev)
{
	if (skb)
	{
		DHD_TRACE(("(%s) is not used for data operations.Droping the packet.\n",
			ndev->name));
		dev_kfree_skb_any(skb);
	}
	return 0;
}

/* logger interface ioctl callback */
static int
dhd_os_logger_do_ioctl(struct net_device *net, struct ifreq *ifr, int cmd)
{
	DHD_TRACE(("(%s) DHD Logger iface do_ioctl cmd %d \n", __FUNCTION__, cmd));
	return 0;
}

/* to register with network stack */
static const struct net_device_ops dhd_os_logger_if_ops = {
	.ndo_open       = dhd_os_logger_open,
	.ndo_stop       = dhd_os_logger_close,
	.ndo_do_ioctl   = dhd_os_logger_do_ioctl,
	.ndo_start_xmit = dhd_os_logger_start_xmit,
};

/* Register logger interface with network stack */
static int
dhd_os_logger_register_netdev(dhd_os_logger_t *poslg, bool need_rtnl_lock)
{
	int ret = 0;
	struct net_device* ndev = NULL;
	const uint8 temp_addr[ETHER_ADDR_LEN] = { LOGGER_ETHER_DEST_ADDR };

	if (poslg->logger_netdev) {
		DHD_ERROR(("DHD logger interface already allocated\n"));
		return -EINVAL;
	}

	/* Allocate etherdev, including space for private structure */
	if (!(ndev = alloc_etherdev(sizeof(dhd_os_logger_t)))) {
		DHD_ERROR(("%s: OOM - alloc_etherdev\n", __FUNCTION__));
		return -ENODEV;
	}

	strlcpy(ndev->name, DHD_LOOGER_IFNAME, sizeof(ndev->name));

	ASSERT(!ndev->netdev_ops);
	ndev->netdev_ops = &dhd_os_logger_if_ops;

	/* Register with a dummy MAC addr */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
	__dev_addr_set(ndev, temp_addr, ETHER_ADDR_LEN);
#else
	eacopy(temp_addr, ndev->dev_addr);
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0) */

	if (need_rtnl_lock) {
		ret = register_netdev(ndev);
	} else {
		ret = register_netdevice(ndev);
	}
	if (ret) {
		DHD_ERROR(("%s register_netdevice failed (%d)\n", __FUNCTION__, ret));
		goto fail;
	}

	poslg->logger_netdev = ndev;

	DHD_PRINT(("%s() %s: DHD Logger Interface Registered\n", __FUNCTION__, ndev->name));
	return ret;
fail:
	free_netdev(ndev);
	return -ENODEV;
}

/**
 * Called when DHD gets inserted into the Kernel
 * Perform context structure allocation, create any module specific
 * threads/execution contexts here
 *
 * @param[in]  osh OSL Handle
 * @return  pointer to the logger context
 */
dhd_os_logger_t *
dhd_os_logger_attach(osl_t *osh)
{
	int ret = 0;
	dhd_os_logger_t *poslg = NULL;

	/* Input parameter validation */
	if (osh == NULL) {
		DHD_ERROR(("%s, invalid input\n", __FUNCTION__));
		return NULL;
	}

	/* Allocate the context structure */
	poslg = (dhd_os_logger_t *)VMALLOCZ(osh, sizeof(dhd_os_logger_t));
	if (unlikely(!poslg)) {
		DHD_ERROR(("%s(): could not allocate memory for dhd_os_logger_t\n", __FUNCTION__));
		return NULL;
	}
	DHD_INFO(("%s poslg  %p\n", __FUNCTION__, poslg));

	/* Populate the fields */
	poslg->osh = osh;

	/* register the os logger interface */
	ret = dhd_os_logger_register_netdev(poslg, TRUE);
	if (ret != BCME_OK) {
		DHD_ERROR(("%s(), os logger register failed ret %d\n", __FUNCTION__, ret));
		goto exit;
	}

	/* Initialize the skb queues and associated locks, length */

	/*
	 * Free queue is accessed by multiple contexts hence one
	 * should use Queue management APIs with locks.
	 *
	 * The process q has only one user - the context that sends
	 * packet from sendup_q to debug logger interface. Its the
	 * same context that copies the content of ready_q into sendup_q
	 * So while adding the skbs from ready_q all we need to do is to
	 * hold the lock of ready_q. So the APIs to operate on sendup_q
	 * should be lock less APIs while others should be ones with lock
	 */
	skb_queue_head_init(&poslg->free_q1);
	skb_queue_head_init(&poslg->free_q2);
	skb_queue_head_init(&poslg->ready_q);
	__skb_queue_head_init(&poslg->sendup_q);

	/*
	 * Allocate qx maximum number of skbs and add to free_q1 and free_q2
	 */
	dhd_os_log_fill_free_qx(poslg, &poslg->free_q1, FREE_Q1_LEN_MAX,
		FREE_Q1_BUFSIZE, "free_q1");
	dhd_os_log_fill_free_qx(poslg, &poslg->free_q2, FREE_Q2_LEN_MAX,
		FREE_Q2_BUFSIZE, "free_q2");

	/* Default qdump. If DUMP_SKBQ debug flag enabled helps to dump the queues */
	poslg->qdump = (DUMP_FREEQ | DUMP_READYQ | DUMP_SENDUPQ);

	/* dump freeq1 and freeq2 skb addresses to dmesg */
	dhd_os_log_dump_freeq1(poslg);
	dhd_os_log_dump_freeq2(poslg);

	/* Create the context that fills free queues buffers(skbs) */
#ifdef LOG_FREEQ_READYQ
	PROC_START(dhd_os_log_fill_buffer_kthread, poslg, &poslg->fill_buff_kthread, 0,
		"dhd_os_log_fill_buffer_kthread");
	if (poslg->fill_buff_kthread.thr_pid < 0) {
		DHD_ERROR(("%s: init fill_skb kthread failed\n", __FUNCTION__));
		ASSERT(0);
		goto exit;
	}
#endif /* LOG_FREEQ_READYQ */

	/* Create the context that sends the debug data to debug interface */
#ifdef LOG_READYQ_SENDUPQ
	PROC_START(dhd_os_log_sendup_kthread, poslg, &poslg->sendup_kthread, 0,
			"dhd_os_log_sendup_kthread");
	if (poslg->sendup_kthread.thr_pid < 0) {
		DHD_ERROR(("%s: init sendup kthread failed\n", __FUNCTION__));
		ASSERT(0);
		goto exit;
	}
#endif /* LOG_READYQ_SENDUPQ */

	return poslg;
exit:
	return NULL;
}

static int
dhd_os_logger_unregister_netdev(dhd_os_logger_t *poslg, bool need_rtnl_lock)
{
	if (!poslg->logger_netdev) {
		return -ENODEV;
	}

	if (need_rtnl_lock) {
		unregister_netdev(poslg->logger_netdev);
	} else {
		unregister_netdevice(poslg->logger_netdev);
	}
	free_netdev(poslg->logger_netdev);
	poslg->logger_netdev = NULL;

	return BCME_OK;
}

/**
 * Called when DHD gets removed from the Kernel
 * Perform context structure de-allocation, free any module specific
 * threads/execution contexts here
 *
 * @param[poslg] Pointer to os logger context structure
 */
void
dhd_os_logger_detach(dhd_os_logger_t *poslg)
{
	tsk_ctl_t *tsk;
	int ret = 0;

	BCM_REFERENCE(tsk);
	/* Input parameter validation */
	if (!poslg) {
		DHD_ERROR(("%s(), NULL dhd_os_logger_t\n", __FUNCTION__));
		goto exit;
	}

	BCM_REFERENCE(tsk);
#ifdef LOG_READYQ_SENDUPQ
	/* stop sendup kthread  */
	tsk = &poslg->sendup_kthread;
	if (tsk->parent && tsk->thr_pid >= 0) {
		PROC_STOP_USING_BINARY_SEMA(tsk);
	} else {
		DHD_ERROR(("%s: sendup_kthread(%ld) not inited\n", __FUNCTION__, tsk->thr_pid));
	}
#endif /* LOG_READYQ_SENDUPQ */
#ifdef LOG_FREEQ_READYQ
	/* stop fill buff kthread  */
	tsk = &poslg->fill_buff_kthread;
	if (tsk->parent && tsk->thr_pid >= 0) {
		PROC_STOP_USING_BINARY_SEMA(tsk);
	} else {
		DHD_ERROR(("%s: fill_buff_kthread(%ld) not inited\n", __FUNCTION__, tsk->thr_pid));
	}
#endif /* LOG_FREEQ_READYQ */

	/* Free the skbs in the list */
	skb_queue_purge(&poslg->free_q1);
	skb_queue_purge(&poslg->free_q2);
	skb_queue_purge(&poslg->ready_q);
	__skb_queue_purge(&poslg->sendup_q);

	/* detach the os logger interface */
	ret = dhd_os_logger_unregister_netdev(poslg, TRUE);
	if (ret != BCME_OK) {
		DHD_ERROR(("%s(), os logger unregister failed ret %d\n", __FUNCTION__, ret));
	}

	/* Free poslg itself */
	VMFREE(poslg->osh, poslg, sizeof(dhd_os_logger_t));
exit:
	return;
}
