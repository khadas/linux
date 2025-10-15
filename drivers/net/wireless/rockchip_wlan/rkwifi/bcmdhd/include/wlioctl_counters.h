/*
 * Custom OID/ioctl definitions for counters
 *
 *
 * Broadcom 802.11abg Networking Device Driver
 *
 * Definitions subject to change without notice.
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
#ifndef _wlioctl_counters_h_
#define	_wlioctl_counters_h_

#define SWDIV_STATS_VERSION_1 1u
#define SWDIV_STATS_VERSION_2 2u

struct wlc_swdiv_stats_v1 {
	uint32 auto_en;
	uint32 active_ant;
	uint32 rxcount;
	int32 avg_snr_per_ant0;
	int32 avg_snr_per_ant1;
	int32 avg_snr_per_ant2;
	uint32 swap_ge_rxcount0;
	uint32 swap_ge_rxcount1;
	uint32 swap_ge_snrthresh0;
	uint32 swap_ge_snrthresh1;
	uint32 swap_txfail0;
	uint32 swap_txfail1;
	uint32 swap_timer0;
	uint32 swap_timer1;
	uint32 swap_alivecheck0;
	uint32 swap_alivecheck1;
	uint32 rxcount_per_ant0;
	uint32 rxcount_per_ant1;
	uint32 acc_rxcount;
	uint32 acc_rxcount_per_ant0;
	uint32 acc_rxcount_per_ant1;
	uint32 tx_auto_en;
	uint32 tx_active_ant;
	uint32 rx_policy;
	uint32 tx_policy;
	uint32 cell_policy;
	uint32 swap_snrdrop0;
	uint32 swap_snrdrop1;
	uint32 mws_antsel_ovr_tx;
	uint32 mws_antsel_ovr_rx;
	uint8 swap_trig_event_id;
};

struct wlc_swdiv_stats_v2 {
	uint16	version;	/* version of the structure
				 * as defined by SWDIV_STATS_CURRENT_VERSION
				 */
	uint16	length;		/* length of the entire structure */
	uint32 auto_en;
	uint32 active_ant;
	uint32 rxcount;
	int32 avg_snr_per_ant0;
	int32 avg_snr_per_ant1;
	int32 avg_snr_per_ant2;
	uint32 swap_ge_rxcount0;
	uint32 swap_ge_rxcount1;
	uint32 swap_ge_snrthresh0;
	uint32 swap_ge_snrthresh1;
	uint32 swap_txfail0;
	uint32 swap_txfail1;
	uint32 swap_timer0;
	uint32 swap_timer1;
	uint32 swap_alivecheck0;
	uint32 swap_alivecheck1;
	uint32 rxcount_per_ant0;
	uint32 rxcount_per_ant1;
	uint32 acc_rxcount;
	uint32 acc_rxcount_per_ant0;
	uint32 acc_rxcount_per_ant1;
	uint32 tx_auto_en;
	uint32 tx_active_ant;
	uint32 rx_policy;
	uint32 tx_policy;
	uint32 cell_policy;
	uint32 swap_snrdrop0;
	uint32 swap_snrdrop1;
	uint32 mws_antsel_ovr_tx;
	uint32 mws_antsel_ovr_rx;
	uint16 swap_trig_event_id;
	uint16 swap_trig_event_id_prev;
	uint8  cbuf_indx;
	uint8  cbuf_active_ant[12];
	uint8  cbuf_swaprsn[12];
	uint8  cbuf_avgSNRprimAnt[12];
	uint8  cbuf_avgSNRdivAnt[12];
	uint32 rxcnt_prim_ant;
	uint32 rxcnt_div_ant;
	uint16 histo_act_cnt_primant;
	uint16 histo_act_cnt_divant;
};

/**
 * The number of variables in wl macstat cnt struct.
 * (wl_cnt_ge40mcst_v1_t, wl_cnt_lt40mcst_v1_t, wl_cnt_v_le10_mcst_t)
 */
#define WL_CNT_MCST_VAR_NUM 64
/* sizeof(wl_cnt_ge40mcst_v1_t), sizeof(wl_cnt_lt40mcst_v1_t), and sizeof(wl_cnt_v_le10_mcst_t) */
#define WL_CNT_MCST_STRUCT_SZ ((uint32)sizeof(uint32) * WL_CNT_MCST_VAR_NUM)
#define WL_CNT_REV80_MCST_STRUCT_SZ ((uint32)sizeof(wl_cnt_ge80mcst_v1_t))
#define WL_CNT_REV80_MCST_TXFUNFlW_STRUCT_FIXED_SZ \
	((uint32)OFFSETOF(wl_cnt_ge80_txfunfl_v1_t, txfunfl))
#define WL_CNT_REV80_MCST_TXFUNFl_STRUCT_SZ(fcnt) \
	(WL_CNT_REV80_MCST_TXFUNFlW_STRUCT_FIXED_SZ + (fcnt * sizeof(uint32)))
#define WL_CNT_REV80_RXERR_MCST_STRUCT_SZ ((uint32)sizeof(wl_cnt_ge80_rxerr_mcst_v1_t))

#define WL_CNT_MCXST_STRUCT_SZ ((uint32)sizeof(wl_cnt_ge64mcxst_v1_t))

#define WL_CNT_HE_STRUCT_V5_SZ ((uint32)sizeof(wl_he_cnt_wlc_v5_t))
#define WL_CNT_HE_STRUCT_V6_SZ ((uint32)sizeof(wl_he_cnt_wlc_v6_t))
#define WL_CNT_HE_STRUCT_V7_SZ ((uint32)sizeof(wl_he_cnt_wlc_v7_t))

#define WL_CNT_SECVLN_STRUCT_SZ ((uint32)sizeof(wl_secvln_cnt_t))

#define WL_CNT_HE_OMI_STRUCT_SZ ((uint32)sizeof(wl_he_omi_cnt_wlc_v1_t))
#define WL_CNT_DYN_BW_STRUCT_SZ ((uint32)sizeof(wlc_dyn_bw_cnt_v1_t))

#define INVALID_CNT_VAL (uint32)(-1)

#define WL_XTLV_CNTBUF_MAX_SIZE ((uint32)(OFFSETOF(wl_cnt_info_t, data)) +	\
		(uint32)BCM_XTLV_HDR_SIZE + (uint32)sizeof(wl_cnt_wlc_t) +		\
		(uint32)BCM_XTLV_HDR_SIZE + WL_CNT_MCST_STRUCT_SZ +              \
		(uint32)BCM_XTLV_HDR_SIZE + WL_CNT_MCXST_STRUCT_SZ)

#define WL_CNTBUF_MAX_SIZE MAX(WL_XTLV_CNTBUF_MAX_SIZE, (uint32)sizeof(wl_cnt_ver_11_t))

/** Top structure of counters IOVar buffer */
typedef struct {
	uint16	version;	/**< see definition of WL_CNT_T_VERSION */
	uint16	datalen;	/**< length of data including all paddings. */
	uint8   data [];	/**< variable length payload:
				 * 1 or more bcm_xtlv_t type of tuples.
				 * each tuple is padded to multiple of 4 bytes.
				 * 'datalen' field of this structure includes all paddings.
				 */
} wl_cnt_info_t;

/* Top structure of subcounters IOVar buffer
 * Whenever we make any change in this structure
 * WL_SUBCNTR_IOV_VER should be updated accordingly
 * The structure definition should remain consistant b/w
 * FW and wl/WLM app.
 */
typedef struct {
	uint16	version;	  /* Version of IOVAR structure. Used for backward
				   * compatibility in future. Whenever we make any
				   * changes to this structure then value of WL_SUBCNTR_IOV_VER
				   * needs to be updated properly.
				   */
	uint16	length;		  /* length in bytes of this structure */
	uint16	counters_version; /* see definition of WL_CNT_T_VERSION
				   * wl app will send the version of counters
				   * which is used to calculate the offset of counters.
				   * It must match the version of counters FW is using
				   * else FW will return error with his version of counters
				   * set in this field.
				   */
	uint16	num_subcounters;  /* Number of counter offset passed by wl app to FW. */
	uint32	data[BCM_FLEX_ARRAY];  /* variable length payload:
				   * Offsets to the counters will be passed to FW
				   * throught this data field. FW will return the value of counters
				   * at the offsets passed by wl app in this fiels itself.
				   */
} wl_subcnt_info_t;

/* Top structure of counters TLV version IOVar buffer
 * The structure definition should remain consistant b/w
 * FW and wl/WLM app.
 */
typedef struct {
	uint16   version;	/* Version of IOVAR structure. Added for backward
			* compatibility feature. If any changes are done,
			* WL_TLV_IOV_VER need to be updated.
			*/
	uint16   length;	/* total len in bytes of this structure + payload */
	uint16   counters_version;	/* See definition of WL_CNT_VERSION_XTLV
			* wl app will update counter tlv version to be used
			* so to calculate offset of supported TLVs.
			* If there is a mismatch in the version, FW will update an error
			*/
	uint16  num_tlv;	/* for WL_CNT_VERSION_XTLV: Max number of TLV info passed by FW
			* to WL app. and vice-versa
			* For WL_CNT_VERSION_XTLV_ML: this field carries links and sliceix-
			* additionally. see SUBC_SUBFLD_xxx
			*/
	uint32   data[];	/* variable length payload:
			* This stores the tlv as supported by F/W to the wl app.
			* This table is required to compute subcounter offsets at WLapp end.
			*/
} wl_cntr_tlv_info_t;

/** wlc layer counters */
typedef struct {
	/* transmit stat counters */
	uint32	txframe;	/**< tx data frames */
	uint32	txbyte;		/**< tx data bytes */
	uint32	txretrans;	/**< tx mac retransmits */
	uint32	txerror;	/**< tx data errors (derived: sum of others) */
	uint32	txctl;		/**< tx management frames */
	uint32	txprshort;	/**< tx short preamble frames */
	uint32	txserr;		/**< tx status errors */
	uint32	txnobuf;	/**< tx out of buffers errors */
	uint32	txnoassoc;	/**< tx discard because we're not associated */
	uint32	txrunt;		/**< tx runt frames */
	uint32	txchit;		/**< tx header cache hit (fastpath) */
	uint32	txcmiss;	/**< tx header cache miss (slowpath) */

	/* transmit chip error counters */
	uint32	txuflo;		/**< tx fifo underflows */
	uint32	txphyerr;	/**< tx phy errors (indicated in tx status) */
	uint32	txphycrs;	/**< PR8861/8963 counter */

	/* receive stat counters */
	uint32	rxframe;	/**< rx data frames */
	uint32	rxbyte;		/**< rx data bytes */
	uint32	rxerror;	/**< rx data errors (derived: sum of others) */
	uint32	rxctl;		/**< rx management frames */
	uint32	rxnobuf;	/**< rx out of buffers errors */
	uint32	rxnondata;	/**< rx non data frames in the data channel errors */
	uint32	rxbadds;	/**< rx bad DS errors */
	uint32	rxbadcm;	/**< rx bad control or management frames */
	uint32	rxfragerr;	/**< rx fragmentation errors */
	uint32	rxrunt;		/**< rx runt frames */
	uint32	rxgiant;	/**< rx giant frames */
	uint32	rxnoscb;	/**< rx no scb error */
	uint32	rxbadproto;	/**< rx invalid frames */
	uint32	rxbadsrcmac;	/**< rx frames with Invalid Src Mac */
	uint32	rxbadda;	/**< rx frames tossed for invalid da */
	uint32	rxfilter;	/**< rx frames filtered out */

	/* receive chip error counters */
	uint32	rxoflo;		/**< rx fifo overflow errors */
	uint32	rxuflo[NFIFO];	/**< rx dma descriptor underflow errors */

	uint32	d11cnt_txrts_off;	/**< d11cnt txrts value when reset d11cnt */
	uint32	d11cnt_rxcrc_off;	/**< d11cnt rxcrc value when reset d11cnt */
	uint32	d11cnt_txnocts_off;	/**< d11cnt txnocts value when reset d11cnt */

	/* misc counters */
	uint32	dmade;		/**< tx/rx dma descriptor errors */
	uint32	dmada;		/**< tx/rx dma data errors */
	uint32	dmape;		/**< tx/rx dma descriptor protocol errors */
	uint32	reset;		/**< reset count */
	uint32	tbtt;		/**< cnts the TBTT int's */
	uint32	txdmawar;	/**< # occurrences of PR15420 workaround */
	uint32	pkt_callback_reg_fail;	/**< callbacks register failure */

	/* 802.11 MIB counters, pp. 614 of 802.11 reaff doc. */
	uint32	txfrag;		/**< dot11TransmittedFragmentCount */
	uint32	txmulti;	/**< dot11MulticastTransmittedFrameCount */
	uint32	txfail;		/**< dot11FailedCount */
	uint32	txretry;	/**< dot11RetryCount */
	uint32	txretrie;	/**< dot11MultipleRetryCount */
	uint32	rxdup;		/**< dot11FrameduplicateCount */
	uint32	txrts;		/**< dot11RTSSuccessCount */
	uint32	txnocts;	/**< dot11RTSFailureCount */
	uint32	txnoack;	/**< dot11ACKFailureCount */
	uint32	rxfrag;		/**< dot11ReceivedFragmentCount */
	uint32	rxmulti;	/**< dot11MulticastReceivedFrameCount */
	uint32	rxcrc;		/**< dot11FCSErrorCount */
	uint32	txfrmsnt;	/**< dot11TransmittedFrameCount (bogus MIB?) */
	uint32	rxundec;	/**< dot11WEPUndecryptableCount */

	/* WPA2 counters (see rxundec for DecryptFailureCount) */
	uint32	tkipmicfaill;	/**< TKIPLocalMICFailures */
	uint32	tkipcntrmsr;	/**< TKIPCounterMeasuresInvoked */
	uint32	tkipreplay;	/**< TKIPReplays */
	uint32	ccmpfmterr;	/**< CCMPFormatErrors */
	uint32	ccmpreplay;	/**< CCMPReplays */
	uint32	ccmpundec;	/**< CCMPDecryptErrors */
	uint32	fourwayfail;	/**< FourWayHandshakeFailures */
	uint32	wepundec;	/**< dot11WEPUndecryptableCount */
	uint32	wepicverr;	/**< dot11WEPICVErrorCount */
	uint32	decsuccess;	/**< DecryptSuccessCount */
	uint32	tkipicverr;	/**< TKIPICVErrorCount */
	uint32	wepexcluded;	/**< dot11WEPExcludedCount */

	uint32	txchanrej;	/**< Tx frames suppressed due to channel rejection */
	uint32	psmwds;		/**< Count PSM watchdogs */
	uint32	phywatchdog;	/**< Count Phy watchdogs (triggered by ucode) */

	/* MBSS counters, AP only */
	uint32	prq_entries_handled;	/**< PRQ entries read in */
	uint32	prq_undirected_entries;	/**<    which were bcast bss & ssid */
	uint32	prq_bad_entries;	/**<    which could not be translated to info */
	uint32	atim_suppress_count;	/**< TX suppressions on ATIM fifo */
	uint32	bcn_template_not_ready;	/**< Template marked in use on send bcn ... */
	uint32	bcn_template_not_ready_done; /**< ...but "DMA done" interrupt rcvd */
	uint32	late_tbtt_dpc;	/**< TBTT DPC did not happen in time */

	/* per-rate receive stat counters */
	uint32  rx1mbps;	/**< packets rx at 1Mbps */
	uint32  rx2mbps;	/**< packets rx at 2Mbps */
	uint32  rx5mbps5;	/**< packets rx at 5.5Mbps */
	uint32  rx6mbps;	/**< packets rx at 6Mbps */
	uint32  rx9mbps;	/**< packets rx at 9Mbps */
	uint32  rx11mbps;	/**< packets rx at 11Mbps */
	uint32  rx12mbps;	/**< packets rx at 12Mbps */
	uint32  rx18mbps;	/**< packets rx at 18Mbps */
	uint32  rx24mbps;	/**< packets rx at 24Mbps */
	uint32  rx36mbps;	/**< packets rx at 36Mbps */
	uint32  rx48mbps;	/**< packets rx at 48Mbps */
	uint32  rx54mbps;	/**< packets rx at 54Mbps */
	uint32  rx108mbps;	/**< packets rx at 108mbps */
	uint32  rx162mbps;	/**< packets rx at 162mbps */
	uint32  rx216mbps;	/**< packets rx at 216 mbps */
	uint32  rx270mbps;	/**< packets rx at 270 mbps */
	uint32  rx324mbps;	/**< packets rx at 324 mbps */
	uint32  rx378mbps;	/**< packets rx at 378 mbps */
	uint32  rx432mbps;	/**< packets rx at 432 mbps */
	uint32  rx486mbps;	/**< packets rx at 486 mbps */
	uint32  rx540mbps;	/**< packets rx at 540 mbps */

	uint32	rfdisable;	/**< count of radio disables */

	uint32	txexptime;	/**< Tx frames suppressed due to timer expiration */

	uint32	txmpdu_sgi;	/**< count for sgi transmit */
	uint32	rxmpdu_sgi;	/**< count for sgi received */
	uint32	txmpdu_stbc;	/**< count for stbc transmit */
	uint32	rxmpdu_stbc;	/**< count for stbc received */

	uint32	rxundec_mcst;	/**< dot11WEPUndecryptableCount */

	/* WPA2 counters (see rxundec for DecryptFailureCount) */
	uint32	tkipmicfaill_mcst;	/**< TKIPLocalMICFailures */
	uint32	tkipcntrmsr_mcst;	/**< TKIPCounterMeasuresInvoked */
	uint32	tkipreplay_mcst;	/**< TKIPReplays */
	uint32	ccmpfmterr_mcst;	/**< CCMPFormatErrors */
	uint32	ccmpreplay_mcst;	/**< CCMPReplays */
	uint32	ccmpundec_mcst;	/**< CCMPDecryptErrors */
	uint32	fourwayfail_mcst;	/**< FourWayHandshakeFailures */
	uint32	wepundec_mcst;	/**< dot11WEPUndecryptableCount */
	uint32	wepicverr_mcst;	/**< dot11WEPICVErrorCount */
	uint32	decsuccess_mcst;	/**< DecryptSuccessCount */
	uint32	tkipicverr_mcst;	/**< TKIPICVErrorCount */
	uint32	wepexcluded_mcst;	/**< dot11WEPExcludedCount */

	uint32	dma_hang;	/**< count for dma hang */
	uint32	reinit;		/**< count for reinit */

	uint32  pstatxucast;	/**< count of ucast frames xmitted on all psta assoc */
	uint32  pstatxnoassoc;	/**< count of txnoassoc frames xmitted on all psta assoc */
	uint32  pstarxucast;	/**< count of ucast frames received on all psta assoc */
	uint32  pstarxbcmc;	/**< count of bcmc frames received on all psta */
	uint32  pstatxbcmc;	/**< count of bcmc frames transmitted on all psta */

	uint32  cso_passthrough; /**< hw cso required but passthrough */
	uint32	cso_normal;	/**< hw cso hdr for normal process */
	uint32	chained;	/**< number of frames chained */
	uint32	chainedsz1;	/**< number of chain size 1 frames */
	uint32	unchained;	/**< number of frames not chained */
	uint32	maxchainsz;	/**< max chain size so far */
	uint32	currchainsz;	/**< current chain size */
	uint32	pciereset;	/**< Secondary Bus Reset issued by driver */
	uint32	cfgrestore;	/**< configspace restore by driver */
	uint32	reinitreason[NREINITREASONCOUNT]; /**< reinitreason counters; 0: Unknown reason */
	uint32	rxrtry;
	uint32  rxmpdu_mu;      /**< Number of MU MPDUs received */

	/* detailed control/management frames */
	uint32	txbar;		/**< Number of TX BAR */
	uint32	rxbar;		/**< Number of RX BAR */
	uint32	txpspoll;	/**< Number of TX PS-poll */
	uint32	rxpspoll;	/**< Number of RX PS-poll */
	uint32	txnull;		/**< Number of TX NULL_DATA */
	uint32	rxnull;		/**< Number of RX NULL_DATA */
	uint32	txqosnull;	/**< Number of TX NULL_QoSDATA */
	uint32	rxqosnull;	/**< Number of RX NULL_QoSDATA */
	uint32	txassocreq;	/**< Number of TX ASSOC request */
	uint32	rxassocreq;	/**< Number of RX ASSOC request */
	uint32	txreassocreq;	/**< Number of TX REASSOC request */
	uint32	rxreassocreq;	/**< Number of RX REASSOC request */
	uint32	txdisassoc;	/**< Number of TX DISASSOC */
	uint32	rxdisassoc;	/**< Number of RX DISASSOC */
	uint32	txassocrsp;	/**< Number of TX ASSOC response */
	uint32	rxassocrsp;	/**< Number of RX ASSOC response */
	uint32	txreassocrsp;	/**< Number of TX REASSOC response */
	uint32	rxreassocrsp;	/**< Number of RX REASSOC response */
	uint32	txauth;		/**< Number of TX AUTH */
	uint32	rxauth;		/**< Number of RX AUTH */
	uint32	txdeauth;	/**< Number of TX DEAUTH */
	uint32	rxdeauth;	/**< Number of RX DEAUTH */
	uint32	txprobereq;	/**< Number of TX probe request */
	uint32	rxprobereq;	/**< Number of RX probe request */
	uint32	txprobersp;	/**< Number of TX probe response */
	uint32	rxprobersp;	/**< Number of RX probe response */
	uint32	txaction;	/**< Number of TX action frame */
	uint32	rxaction;	/**< Number of RX action frame */
	uint32  ampdu_wds;	/**< Number of AMPDU watchdogs */
	uint32  txlost;		/**< Number of lost packets reported in txs */
	uint32	txdatamcast;	/**< Number of TX multicast data packets */
	uint32	txdatabcast;	/**< Number of TX broadcast data packets */
	uint32	psmxwds;	/**< Number of PSMx watchdogs */
	uint32  rxback;
	uint32  txback;
	uint32  p2p_tbtt;	/**< Number of P2P TBTT Events */
	uint32  p2p_tbtt_miss;	/**< Number of P2P TBTT Events Miss */
	uint32	txqueue_start;
	uint32	txqueue_end;
	uint32  txbcast;        /* Broadcast TransmittedFrameCount */
	uint32  txdropped;      /* tx dropped pkts */
	uint32  rxbcast;        /* BroadcastReceivedFrameCount */
	uint32  rxdropped;      /* rx dropped pkts (derived: sum of others) */
	uint32	txq_end_assoccb; /* forced txqueue_end callback fired in assoc */
	uint32	tx_toss_cnt;	/* number of tx packets tossed */
	uint32	rx_toss_cnt;	/* number of rx packets tossed	*/
	uint32	last_tx_toss_rsn; /* reason because of which last tx pkt tossed */
	uint32	last_rx_toss_rsn; /* reason because of which last rx pkt tossed */
	uint32	pmk_badlen_cnt;	/* number of invalid pmk len */
	uint32	txbar_notx;	/* number of TX BAR not sent (maybe supressed or muted) */
	uint32	txbar_noack;	/* number of TX BAR sent, but not acknowledged by peer */
	uint32	rxfrag_agedout;	/**< # of aged out rx fragmentation */
	uint32	pmkid_mismatch_cnt; /* number of EAPOL msg1 PMKID mismatch */
	uint32	txaction_vndr_attempt; /* Number of VS AFs scheduled successfully for Tx */
	uint32	txaction_vndr_fail; /* Number of VS AFs not sent or not acked */
	uint32	rxnofrag;	/* # of nobuf failure due to no pkt availability */
	uint32	rxnocmplid;	/* # of nobuf failure due to rxcmplid non-availability */
	uint32	rxnohaddr;	/* # of nobuf failure due to host address non-availability */
	uint32	txnull_pm;	/**< Number of TX NULL_DATA total */
	uint32	txnull_pm_succ;	/**< Number of TX NULL_DATA successes */
	uint32	ccmpreplay_qosdata_nobapol_rxretry;	/**< Retried QOS data MPDUs RX without
							 * BA policy and tossed by key mgmt as
							 * replays
							 */
	uint32	txnoalfdatabuf;	/**< out of alfrag data buffers errors */
	uint32	txalfdatabuf;	/**< number of tx alfrag data buffers attepted for transmission */
	uint32	txalfrag;	/**< number of txalfrags attepted for transmission */
	uint32	txlfrag;	/**< number of txlfrags attepted for transmission */
	uint32  rxunsolicitedproberesp; /**< number of "unsoliocited" probe responses RXed */
	uint32  rco_passthrough;	/**< hw rco required but passthrough */
	uint32	rco_normal;	/**< hw rco hdr for normal process */
	uint32	rxnodatabuf;	/**< # of nobuf failure due to rxdata buf non-availability */
	uint32	txmlprobereq;	/* No of ML probe requests sent */
	uint32	rxmlprobersp;	/* No of ML probe response recieved */
	/* Do not remove or rename in the middle of this struct.
	 * All counter variables have to be of uint32.
	 */
} wl_cnt_wlc_t;

/* he counters Version 1 */
#define HE_COUNTERS_V1		(1)
typedef struct wl_he_cnt_wlc_v1 {
	uint32 he_rxtrig_myaid;
	uint32 he_rxtrig_rand;
	uint32 he_colormiss_cnt;
	uint32 he_txmampdu;
	uint32 he_txmtid_back;
	uint32 he_rxmtid_back;
	uint32 he_rxmsta_back;
	uint32 he_txfrag;
	uint32 he_rxdefrag;
	uint32 he_txtrig;
	uint32 he_rxtrig_basic;
	uint32 he_rxtrig_murts;
	uint32 he_rxtrig_bsrp;
	uint32 he_rxdlmu;
	uint32 he_physu_rx;
	uint32 he_phyru_rx;
	uint32 he_txtbppdu;
} wl_he_cnt_wlc_v1_t;

/* he counters Version 2 */
#define HE_COUNTERS_V2		(2)
typedef struct wl_he_cnt_wlc_v2 {
	uint16 version;
	uint16 len;
	uint32 he_rxtrig_myaid; /**< rxed valid trigger frame with myaid */
	uint32 he_rxtrig_rand; /**< rxed valid trigger frame with random aid */
	uint32 he_colormiss_cnt; /**< for bss color mismatch cases */
	uint32 he_txmampdu; /**< for multi-TID AMPDU transmission */
	uint32 he_txmtid_back; /**< for multi-TID BACK transmission */
	uint32 he_rxmtid_back; /**< reception of multi-TID BACK */
	uint32 he_rxmsta_back; /**< reception of multi-STA BACK */
	uint32 he_txfrag; /**< transmission of Dynamic fragmented packets */
	uint32 he_rxdefrag; /**< reception of dynamic fragmented packets */
	uint32 he_txtrig; /**< transmission of trigger frames */
	uint32 he_rxtrig_basic; /**< reception of basic trigger frame */
	uint32 he_rxtrig_murts; /**< reception of MU-RTS trigger frame */
	uint32 he_rxtrig_bsrp; /**< reception of BSR poll trigger frame */
	uint32 he_rxdlmu; /**< reception of DL MU PPDU */
	uint32 he_physu_rx; /**< reception of SU frame */
	uint32 he_phyru_rx; /**< reception of RU frame */
	uint32 he_txtbppdu; /**< increments on transmission of every TB PPDU */
	uint32 he_null_tbppdu; /**< null TB PPDU's sent as a response to basic trigger frame */
} wl_he_cnt_wlc_v2_t;

/* he counters Version 3 */
#define WL_RU_TYPE_MAX			6
#define WL_EHT_RU_TYPE_MAX	(16u)
#define HE_COUNTERS_V3		(3)

typedef struct wl_he_cnt_wlc_v3 {
	uint16 version;
	uint16 len;
	uint32 he_rxtrig_myaid; /**< rxed valid trigger frame with myaid */
	uint32 he_rxtrig_rand; /**< rxed valid trigger frame with random aid */
	uint32 he_colormiss_cnt; /**< for bss color mismatch cases */
	uint32 he_txmampdu; /**< for multi-TID AMPDU transmission */
	uint32 he_txmtid_back; /**< for multi-TID BACK transmission */
	uint32 he_rxmtid_back; /**< reception of multi-TID BACK */
	uint32 he_rxmsta_back; /**< reception of multi-STA BACK */
	uint32 he_txfrag; /**< transmission of Dynamic fragmented packets */
	uint32 he_rxdefrag; /**< reception of dynamic fragmented packets */
	uint32 he_txtrig; /**< transmission of trigger frames */
	uint32 he_rxtrig_basic; /**< reception of basic trigger frame */
	uint32 he_rxtrig_murts; /**< reception of MU-RTS trigger frame */
	uint32 he_rxtrig_bsrp; /**< reception of BSR poll trigger frame */
	uint32 he_rxhemuppdu_cnt; /**< rxing HE MU PPDU */
	uint32 he_physu_rx; /**< reception of SU frame */
	uint32 he_phyru_rx; /**< reception of RU frame */
	uint32 he_txtbppdu; /**< increments on transmission of every TB PPDU */
	uint32 he_null_tbppdu; /**< null TB PPDU's sent as a response to basic trigger frame */
	uint32 he_rxhesuppdu_cnt; /**< rxing SU PPDU */
	uint32 he_rxhesureppdu_cnt; /**< rxing Range Extension(RE) SU PPDU */
	uint32 he_null_zero_agg; /**< null AMPDU's transmitted in response to basic trigger
				 * because of zero aggregation
				 */
	uint32 he_null_bsrp_rsp; /**< null AMPDU's txed in response to BSR poll */
	uint32 he_null_fifo_empty; /**< null AMPDU's in response to basic trigger
				 * because of no frames in fifo's
				 */
	uint32 he_myAID_cnt;
	uint32 he_rxtrig_bfm_cnt;
	uint32 he_rxtrig_mubar;
	uint32 rxheru[WL_RU_TYPE_MAX];		/**< HE of rx pkts */
	uint32 txheru[WL_RU_TYPE_MAX];
	uint32 he_mgmt_tbppdu;
	uint32 he_cs_req_tx_cancel;
	uint32 he_wrong_nss;
	uint32 he_trig_unsupp_rate;
	uint32 he_rxtrig_nfrp;
	uint32 he_rxtrig_bqrp;
	uint32 he_rxtrig_gcrmubar;
	uint32 he_txtbppdu_cnt[AC_COUNT];
} wl_he_cnt_wlc_v3_t;

/* he counters Version 4 */
#define HE_COUNTERS_V4		(4)
typedef struct wl_he_cnt_wlc_v4 {
	uint16 version;
	uint16 len;
	uint32 he_rxtrig_myaid; /**< rxed valid trigger frame with myaid */
	uint32 he_rxtrig_rand; /**< rxed valid trigger frame with random aid */
	uint32 he_colormiss_cnt; /**< for bss color mismatch cases */
	uint32 he_txmampdu; /**< for multi-TID AMPDU transmission */
	uint32 he_txmtid_back; /**< for multi-TID BACK transmission */
	uint32 he_rxmtid_back; /**< reception of multi-TID BACK */
	uint32 he_rxmsta_back; /**< reception of multi-STA BACK */
	uint32 he_txfrag; /**< transmission of Dynamic fragmented packets */
	uint32 he_rxdefrag; /**< reception of dynamic fragmented packets */
	uint32 he_txtrig; /**< transmission of trigger frames */
	uint32 he_rxtrig_basic; /**< reception of basic trigger frame */
	uint32 he_rxtrig_murts; /**< reception of MU-RTS trigger frame */
	uint32 he_rxtrig_bsrp; /**< reception of BSR poll trigger frame */
	uint32 he_rxtsrt_hemuppdu_cnt; /**< rxing HE MU PPDU */
	uint32 he_physu_rx; /**< reception of SU frame */
	uint32 he_phyru_rx; /**< reception of RU frame */
	uint32 he_txtbppdu; /**< increments on transmission of every TB PPDU */
	uint32 he_null_tbppdu; /**< null TB PPDU's sent as a response to basic trigger frame */
	uint32 he_rxstrt_hesuppdu_cnt; /**< rxing SU PPDU */
	uint32 he_rxstrt_hesureppdu_cnt; /**< rxing Range Extension(RE) SU PPDU */
	uint32 he_null_zero_agg; /**< null AMPDU's transmitted in response to basic trigger
				 * because of zero aggregation
				 */
	uint32 he_null_bsrp_rsp; /**< null AMPDU's txed in response to BSR poll */
	uint32 he_null_fifo_empty; /**< null AMPDU's in response to basic trigger
				 * because of no frames in fifo's
				 */
	uint32 he_myAID_cnt;
	uint32 he_rxtrig_bfm_cnt;
	uint32 he_rxtrig_mubar;
	uint32 rxheru[WL_RU_TYPE_MAX];		/**< HE of rx pkts */
	uint32 txheru[WL_RU_TYPE_MAX];
	uint32 he_mgmt_tbppdu;
	uint32 he_cs_req_tx_cancel;
	uint32 he_wrong_nss;
	uint32 he_trig_unsupp_rate;
	uint32 he_rxtrig_nfrp;
	uint32 he_rxtrig_bqrp;
	uint32 he_rxtrig_gcrmubar;
	uint32 he_rxtrig_basic_htpack; /**< triggers received with HTP ack policy */
	uint32 he_rxtrig_ed_cncl;	/**< count of cancelled packets
					 * because of cs_req in trigger frame
					 */
	uint32 he_rxtrig_suppr_null_tbppdu; /**<  count of null frame sent because of
					 * suppression scenarios
					 */
	uint32 he_ulmu_disable;		/**< number of UL MU disable scenario's handled in ucode */
	uint32 he_ulmu_data_disable;	/**<number of UL MU data disable scenarios
					 * handled in ucode
					 */
	uint32 he_txtbppdu_cnt[AC_COUNT];
} wl_he_cnt_wlc_v4_t;

/* he counters Version 5 */
#define HE_COUNTERS_V5		(5)
typedef struct wl_he_cnt_wlc_v5 {
	uint16 version;
	uint16 len;
	uint32 he_rxtrig_myaid;			/* rxed valid trigger frame with myaid */
	uint32 he_rxtrig_rand;			/* rxed valid trigger frame with random aid */
	uint32 he_colormiss_cnt;		/* for bss color mismatch cases */
	uint32 he_txmampdu;			/* for multi-TID AMPDU transmission */
	uint32 he_txmtid_back;			/* for multi-TID BACK transmission */
	uint32 he_rxmtid_back;			/* reception of multi-TID BACK */
	uint32 he_rxmsta_back;			/* reception of multi-STA BACK */
	uint32 he_txfrag;			/* transmission of Dynamic fragmented packets */
	uint32 he_rxdefrag;			/* reception of dynamic fragmented packets */
	uint32 he_txtrig;			/* transmission of trigger frames */
	uint32 he_rxtrig_basic;			/* reception of basic trigger frame */
	uint32 he_rxtrig_murts;			/* reception of MU-RTS trigger frame */
	uint32 he_rxtrig_bsrp;			/* reception of BSR poll trigger frame */
	uint32 he_rxtsrt_hemuppdu_cnt;		/* rxing HE MU PPDU */
	uint32 he_physu_rx;			/* reception of SU frame */
	uint32 he_phyru_rx;			/* reception of RU frame */
	uint32 he_txtbppdu;			/* increments on transmission of every TB PPDU */
	uint32 he_null_tbppdu;			/* null TBPPDU's sent as a response to
						 * basic trigger frame
						 */
	uint32 he_rxstrt_hesuppdu_cnt;		/* rxing SU PPDU */
	uint32 he_rxstrt_hesureppdu_cnt;	/* rxing Range Extension(RE) SU PPDU */
	uint32 he_null_zero_agg;		/* nullAMPDU's transmitted in response to
						 * basic trigger because of zero aggregation
						 */
	uint32 he_null_bsrp_rsp;		/* null AMPDU's txed in response to BSR poll */
	uint32 he_null_fifo_empty;		/* null AMPDU's in response to basic trigger
						 * because of no frames in fifo's
						 */
	uint32 he_rxtrig_bfm_cnt;
	uint32 he_rxtrig_mubar;
	uint32 rxheru[WL_RU_TYPE_MAX];		/* HE of rx pkts */
	uint32 txheru[WL_RU_TYPE_MAX];
	uint32 he_mgmt_tbppdu;
	uint32 he_cs_req_tx_cancel;
	uint32 he_wrong_nss;
	uint32 he_trig_unsupp_rate;
	uint32 he_rxtrig_nfrp;
	uint32 he_rxtrig_bqrp;
	uint32 he_rxtrig_gcrmubar;
	uint32 he_rxtrig_basic_htpack;		/* triggers received with HTP ack policy */
	uint32 he_rxtrig_suppr_null_tbppdu;	/*  count of null frame sent because of
						 * suppression scenarios
						 */
	uint32 he_ulmu_disable;			/* number of ULMU dis scenario's handled in ucode */
	uint32 he_ulmu_data_disable;		/* number of UL MU data disable scenarios
						 * handled in ucode
						 */
	uint32 rxheru_2x996T;
	uint32 he_txtbppdu_cnt[AC_COUNT];
	uint32 he_rxtrig_ruidx_invalid;		/* basic trigger with invalid RU index or RU size
						 * greater than BW
						 */
	uint32 txheru_2x996T;
} wl_he_cnt_wlc_v5_t;

/* HE counters Version 6 structure definitions */
#define HE_COUNTERS_V6		(6u)

/* Rev GE88 HE Tx counters (SW based) */
typedef struct wl_he_tx_cnt_ge88_v1 {
	uint8 link_idx;
	uint8 pad[3];
	uint32 he_mgmt_tbppdu;		/**< # Tx HE MGMT TBPPDU frames */
	uint32 he_txtbppdu_cnt[AC_COUNT];	/**< # Tx packets in each AC */
	uint32 txheru[WL_RU_TYPE_MAX];	/**< # Tx HE TBPPDU frames */
	uint32 txheru_2x996T;		/**< # Tx packets in 2x996 tone RU */
	uint32 txheru_4x996T;		/**< # Tx packets in 4x996 tone RU */
	uint32 txehtru[WL_EHT_RU_TYPE_MAX];
} wl_he_tx_cnt_ge88_v1_t;

/* Rev GE88 HE Rx counters (SW based) */
typedef struct wl_he_rx_cnt_ge88_v1 {
	uint8 link_idx;
	uint8 pad[3];
	uint32 rxheru[WL_RU_TYPE_MAX];	/**< # Rx HE RU frames */
	uint32 rxheru_2x996T;		/**< # Rx packets in 2x996 tone RU */
	uint32 rxheru_4x996T;		/**< # Rx packets in 4x996 tone RU */
	uint32 he_rxtrig_ru_4x996T;	/**< Rx'd trigger frame with STA RU index 320mhz */
	uint32 rxehtru[WL_EHT_RU_TYPE_MAX];
} wl_he_rx_cnt_ge88_v1_t;

/* Version6 - HE Counters */
typedef struct wl_he_cnt_wlc_v6 {
	uint16	version;
	uint16	len;
	uint8	num_links;	/* Number of Tx/Rx links supported on slice */
	uint8	pad[3];
	/* Per ML Link TX HE counters (esp. eMLSR) */
	uint8	counters[];
} wl_he_cnt_wlc_v6_t;

/*  HE counters version 7 meant for HE ecounters only */
#define HE_COUNTERS_V7		(7)
typedef struct wl_he_cnt_wlc_v7 {
	uint16 version;
	uint16 len;
	uint32 he_rxtrig_myaid;			/* rxed valid trigger frame with myaid */
	uint32 he_rxtrig_rand;			/* rxed valid trigger frame with random aid */
	uint32 he_colormiss_cnt;		/* for bss color mismatch cases */
	uint32 he_txmampdu;			/* for multi-TID AMPDU transmission */
	uint32 he_txmtid_back;			/* for multi-TID BACK transmission */
	uint32 he_rxmtid_back;			/* reception of multi-TID BACK */
	uint32 he_rxmsta_back;			/* reception of multi-STA BACK */
	uint32 he_txfrag;			/* transmission of Dynamic fragmented packets */
	uint32 he_rxdefrag;			/* reception of dynamic fragmented packets */
	uint32 he_txtrig;			/* transmission of trigger frames */
	uint32 he_rxtrig_basic;			/* reception of basic trigger frame */
	uint32 he_rxtrig_murts;			/* reception of MU-RTS trigger frame */
	uint32 he_rxtrig_bsrp;			/* reception of BSR poll trigger frame */
	uint32 he_rxtsrt_hemuppdu_cnt;		/* rxing HE MU PPDU */
	uint32 he_physu_rx;			/* reception of SU frame */
	uint32 he_phyru_rx;			/* reception of RU frame */
	uint32 he_txtbppdu;			/* increments on transmission of every TB PPDU */
	uint32 he_null_tbppdu;			/* null TBPPDU's sent as a response to
						 * basic trigger frame
						 */
	uint32 he_rxstrt_hesuppdu_cnt;		/* rxing SU PPDU */
	uint32 he_rxstrt_hesureppdu_cnt;	/* rxing Range Extension(RE) SU PPDU */
	uint32 he_null_zero_agg;		/* nullAMPDU's transmitted in response to
						 * basic trigger because of zero aggregation
						 */
	uint32 he_null_bsrp_rsp;		/* null AMPDU's txed in response to BSR poll */
	uint32 he_null_fifo_empty;		/* null AMPDU's in response to basic trigger
						 * because of no frames in fifo's
						 */
	uint32 he_rxtrig_bfm_cnt;
	uint32 he_rxtrig_mubar;
	uint32 rxheru[WL_RU_TYPE_MAX];		/* HE of rx pkts */
	uint32 txheru[WL_RU_TYPE_MAX];
	uint32 he_mgmt_tbppdu;
	uint32 he_cs_req_tx_cancel;
	uint32 he_wrong_nss;
	uint32 he_trig_unsupp_rate;
	uint32 he_rxtrig_nfrp;
	uint32 he_rxtrig_bqrp;
	uint32 he_rxtrig_gcrmubar;
	uint32 he_rxtrig_basic_htpack;		/* triggers received with HTP ack policy */
	uint32 he_rxtrig_suppr_null_tbppdu;	/*  count of null frame sent because of
						 * suppression scenarios
						 */
	uint32 he_ulmu_disable;			/* number of ULMU dis scenario's handled in ucode */
	uint32 he_ulmu_data_disable;		/* number of UL MU data disable scenarios
						 * handled in ucode
						 */
	uint32 rxheru_2x996T;
	uint32 he_txtbppdu_cnt[AC_COUNT];	/**< # Tx packets in each AC */
	uint32 he_rxtrig_ruidx_invalid;		/* basic trigger with invalid RU index or RU size
						 * greater than BW
						 */
	uint32 txheru_2x996T;			/**< # Tx packets in 2x996 tone RU */
	/* SW counters */
	uint32 txheru_4x996T;			/**< # Tx packets in 4x996 tone RU */
	uint32 txehtru[WL_EHT_RU_TYPE_MAX];
	uint32 rxheru_4x996T;		/**< # Rx packets in 4x996 tone RU */
	uint32 he_rxtrig_ru_4x996T;	/**< Rx'd trigger frame with STA RU index 320mhz */
	uint32 rxehtru[WL_EHT_RU_TYPE_MAX];
	/* ucode counters */
	uint32 he_txtbppdu_ack;		/**< Number of tx HE TBPPDU acks */
	uint32 he_rxdlmu;			/**< number of rx'd DL MU frames */
	uint32 he_rxtrig_rngpoll;
	uint32 he_rxtrig_rngsnd;
	uint32 he_rxtrig_rngssnd;
	uint32 he_rxtrig_rngrpt;
	uint32 he_rxtrig_rngpasv;
	uint32 he_rxtrig_ru_2x996T;		/**< Rx'd trigger frame with STA RU index 160mhz */
	uint32 he_rxtrig_invalid_ru;	/**< Rx'd trigger frame with invalid STA20 RU index */
	uint32 he_rxtrig_drop_cnt;		/**< # of trigger frames dropped */
} wl_he_cnt_wlc_v7_t;

/* he omi counters Version 1 */
#define HE_OMI_COUNTERS_V1		(1)
typedef struct wl_he_omi_cnt_wlc_v1 {
	uint16 version;
	uint16 len;
	uint32 he_omitx_sched;          /* Count for total number of OMIs scheduled */
	uint32 he_omitx_success;        /* Count for OMI Tx success */
	uint32 he_omitx_retries;        /* Count for OMI retries as TxDone not set */
	uint32 he_omitx_dur;            /* Accumulated duration of OMI completion time */
	uint32 he_omitx_ulmucfg;        /* count for UL MU enable/disable change req */
	uint32 he_omitx_ulmucfg_ack;    /* count for UL MU enable/disable req txed successfully */
	uint32 he_omitx_txnsts;         /* count for Txnsts change req */
	uint32 he_omitx_txnsts_ack;     /* count for Txnsts change req txed successfully */
	uint32 he_omitx_rxnss;          /* count for Rxnss change req */
	uint32 he_omitx_rxnss_ack;      /* count for Rxnss change req txed successfully */
	uint32 he_omitx_bw;             /* count for BW change req */
	uint32 he_omitx_bw_ack;         /* count for BW change req txed successfully */
	uint32 he_omitx_ersudis;        /* count for ER SU enable/disable req */
	uint32 he_omitx_ersudis_ack;    /* count for ER SU enable/disable req txed successfully */
	uint32 he_omitx_dlmursdrec;	/* count for Resound recommendation change req */
	uint32 he_omitx_dlmursdrec_ack;	/* count for Resound recommendation req txed successfully */
} wl_he_omi_cnt_wlc_v1_t;

typedef struct wl_he_omi_cnt_v2 {
	uint16 len;
	uint8  link_idx;
	uint8  PAD;
	uint32 he_omitx_sched;          /* Count for total number of OMIs scheduled */
	uint32 he_omitx_success;        /* Count for OMI Tx success */
	uint32 he_omitx_retries;        /* Count for OMI retries as TxDone not set */
	uint32 he_omitx_dur;            /* Accumulated duration of OMI completion time */
	uint32 he_omitx_ulmucfg;        /* count for UL MU enable/disable change req */
	uint32 he_omitx_ulmucfg_ack;    /* count for UL MU enable/disable req txed successfully */
	uint32 he_omitx_txnsts;         /* count for Txnsts change req */
	uint32 he_omitx_txnsts_ack;     /* count for Txnsts change req txed successfully */
	uint32 he_omitx_rxnss;          /* count for Rxnss change req */
	uint32 he_omitx_rxnss_ack;      /* count for Rxnss change req txed successfully */
	uint32 he_omitx_bw;             /* count for BW change req */
	uint32 he_omitx_bw_ack;         /* count for BW change req txed successfully */
	uint32 he_omitx_ersudis;        /* count for ER SU enable/disable req */
	uint32 he_omitx_ersudis_ack;    /* count for ER SU enable/disable req txed successfully */
	uint32 he_omitx_dlmursdrec;	/* count for Resound recommendation change req */
	uint32 he_omitx_dlmursdrec_ack;	/* count for Resound recommendation req txed successfully */
} wl_he_omi_cnt_v2_t;

/* he omi counters Version 2 */
#define HE_OMI_COUNTERS_V2		(2u)
typedef struct wl_he_omi_cnt_wlc_v2 {
	uint16	version;
	uint16	len;
	uint8	num_links;	/* Number of links supported on slice */
	uint8	pad[3];
	/* Per ML Link OMI counters */
	wl_he_omi_cnt_v2_t counters[];
} wl_he_omi_cnt_wlc_v2_t;

typedef struct wlc_dyn_bw_cnt_v1 {
	uint32 dyn_bw_tx_rts20_cnt;
	uint32 dyn_bw_tx_rts40_cnt;
	uint32 dyn_bw_tx_rts80_cnt;
	uint32 dyn_bw_tx_rts160_cnt;
	uint32 dyn_bw_rx_rts20_cnt;
	uint32 dyn_bw_rx_rts40_cnt;
	uint32 dyn_bw_rx_rts80_cnt;
	uint32 dyn_bw_rx_rts160_cnt;
	uint32 dyn_bw_tx_cts20_cnt;
	uint32 dyn_bw_tx_cts40_cnt;
	uint32 dyn_bw_tx_cts80_cnt;
	uint32 dyn_bw_tx_cts160_cnt;
	uint32 dyn_bw_rx_cts20_cnt;
	uint32 dyn_bw_rx_cts40_cnt;
	uint32 dyn_bw_rx_cts80_cnt;
	uint32 dyn_bw_rx_cts160_cnt;
} wlc_dyn_bw_cnt_v1_t;

#define WLC_DYN_BW_CNT_VERSION_V2              2

typedef struct wlc_dyn_bw_cnt_v2 {
	uint16 version;
	uint16 len;

	uint32 dyn_bw_tx_rts20_cnt;
	uint32 dyn_bw_tx_rts40_cnt;
	uint32 dyn_bw_tx_rts80_cnt;
	uint32 dyn_bw_tx_rts160_cnt;

	uint32 dyn_bw_rx_rts20_cnt;
	uint32 dyn_bw_rx_rts40_cnt;
	uint32 dyn_bw_rx_rts80_cnt;
	uint32 dyn_bw_rx_rts160_cnt;

	uint32 dyn_bw_tx_cts20_cnt;
	uint32 dyn_bw_tx_cts40_cnt;
	uint32 dyn_bw_tx_cts80_cnt;
	uint32 dyn_bw_tx_cts160_cnt;

	uint32 dyn_bw_rx_cts20_cnt;
	uint32 dyn_bw_rx_cts40_cnt;
	uint32 dyn_bw_rx_cts80_cnt;
	uint32 dyn_bw_rx_cts160_cnt;

	 /* MPDU counts */
	uint32 dyn_bw_rx_data20_cnt;
	uint32 dyn_bw_rx_data40_cnt;
	uint32 dyn_bw_rx_data80_cnt;
	uint32 dyn_bw_rx_data160_cnt;

	uint32 dyn_bw_tx_data20_cnt;
	uint32 dyn_bw_tx_data40_cnt;
	uint32 dyn_bw_tx_data80_cnt;
	uint32 dyn_bw_tx_data160_cnt;
} wlc_dyn_bw_cnt_v2_t;

#define WLC_DYN_BW_BLK_CNT_VERSION_V1              (1u)

typedef struct wlc_dyn_bw_blk_cnt_v1 {
	uint16 version;
	uint16 len;

	uint32 tx_rts20;
	uint32 tx_rts40;
	uint32 tx_rts80;
	uint32 tx_rts160;

	uint32 rx_rts20;
	uint32 rx_rts40;
	uint32 rx_rts80;
	uint32 rx_rts160;

	uint32 tx_cts20;
	uint32 tx_cts40;
	uint32 tx_cts80;
	uint32 tx_cts160;

	uint32 rx_cts20;
	uint32 rx_cts40;
	uint32 rx_cts80;
	uint32 rx_cts160;
} wlc_dyn_bw_blk_cnt_v1_t;

#define WLC_DYN_BW_CNT_VERSION_V3              (3u)

typedef struct wlc_dyn_bw_cnt_v3 {
	uint16 version;
	uint16 len;
	uint8  num_blks;  /* Number of stats blocks */
	uint8  pad[3];
	/* Per blk counters - wlc_dyn_bw_blk_cnt_vX_t */
	uint8  cnts[];
} wlc_dyn_bw_cnt_v3_t;

#define WLC_DATA_BW_BLK_CNT_VERSION_V1              (1u)

typedef struct wlc_data_bw_blk_cnt_v1 {
	uint16 version;
	uint16 len;

	/* MPDU TX counts */
	uint32 tx_data20;
	uint32 tx_data40;
	uint32 tx_data60;
	uint32 tx_data80;
	uint32 tx_data120;
	uint32 tx_data140;
	uint32 tx_data160;

	/* MPDU BA RX/TX MPDU success counts */
	uint32 rx_ack20;
	uint32 rx_ack40;
	uint32 rx_ack60;
	uint32 rx_ack80;
	uint32 rx_ack120;
	uint32 rx_ack140;
	uint32 rx_ack160;

	/* MPDU RX counts */
	uint32 rx_data20;
	uint32 rx_data40;
	uint32 rx_data60;
	uint32 rx_data80;
	uint32 rx_data120;
	uint32 rx_data140;
	uint32 rx_data160;
} wlc_data_bw_blk_cnt_v1_t;

#define WLC_DATA_BW_CNT_VERSION_V1              (1u)

typedef struct wlc_data_bw_cnt_v1 {
	uint16 version;
	uint16 len;
	uint8  num_blks;  /* Number of stats blocks */
	uint8  pad[3];
	/* Per blk counters -> wlc_data_bw_blk_cnt_vX_t */
	uint8  cnts[];
} wlc_data_bw_cnt_v1_t;

/* mesh pkt counters Version 1 */
#define MESH_PKT_COUNTERS_V1		(1)
typedef struct wl_mesh_pkt_cnt_v1 {
	// RX Mesh Data counts

	// in-mesh addressing
	uint32 rx_local_mesh_da;       // RA  = MeshDA, RA unicast
	uint32 rx_other_mesh_da;       // RA != MeshDA, RA unicast
	uint32 rx_group_mesh_da;       // RA is bcast/mcast
	// proxy addressing
	uint32 rx_proxy_local_mesh_da; // RA  = MeshDA, RA unicast
	uint32 rx_proxy_other_mesh_da; // RA != MeshDA, RA unicast
	uint32 rx_proxy_group_mesh_da; // RA is bcast/mcast

	// TX Mesh Data counts

	// in-mesh addressing
	uint32 tx_peer_mesh_da;        // RA  = MeshDA, RA unicast
	uint32 tx_other_mesh_da;       // RA != MeshDA, RA unicast
	uint32 tx_group_mesh_da;       // RA is bcast/mcast
	// proxy addressing
	uint32 tx_proxy_peer_mesh_da;  // RA  = MeshDA, RA unicast
	uint32 tx_proxy_other_mesh_da; // RA != MeshDA, RA unicast
	uint32 tx_proxy_group_mesh_da; // RA is bcast/mcast

	// RX Mesh Multihop Action counts
	uint32 rx_act_local_mesh_da;   // RA  = MeshDA, RA unicast
	uint32 rx_act_other_mesh_da;   // RA != MeshDA, RA unicast
	uint32 rx_act_group_mesh_da;   // RA is bcast/mcast

	// TX Mesh Multihop Action counts
	uint32 tx_act_peer_mesh_da;    // RA  = MeshDA, RA unicast
	uint32 tx_act_other_mesh_da;   // RA != MeshDA, RA unicast
	uint32 tx_act_group_mesh_da;   // RA is bcast/mcast
} wl_mesh_pkt_cnt_v1_t;

#define WL_SC_SLIM_SCAN_CNT_VER_V1            1u

typedef struct wl_sc_slim_scan_cnts_v1 {
	uint16 version;
	uint16 len;
	uint32 rx_start; /* Rx start/frame complete cnt */
	uint32 good_fcs; /* Good FCS frame cnt */
	uint32 bad_plcp; /* Frame drop cnt due to bad plcp */
	uint32 pfifo_nempty; /* Frame drop cnt due to PFIFO not empty */
	uint32 framelen_drop; /* Frame drop cnt due to excess frame length */
	uint32 fc_addr_chckfail; /* Frame drop cnt due to FC/addr check fail */
} wl_sc_slim_scan_cnts_v1_t;

/* WL_IFSTATS_XTLV_WL_SLICE_TXBF */
/* beamforming counters version 1 */
#define TXBF_ECOUNTERS_V1	(1u)
#define WL_TXBF_CNT_ARRAY_SZ	(8u)
typedef struct wl_txbf_ecounters_v1 {
	uint16 version;
	uint16 len;
	/* transmit beamforming stats */
	uint16 txndpa;				/* null data packet announcements */
	uint16 txndp;				/* null data packets */
	uint16 txbfpoll;			/* beamforming report polls */
	uint16 txsf;				/* subframes */
	uint16 txcwrts;				/* contention window rts */
	uint16 txcwcts;				/* contention window cts */
	uint16 txbfm;
	/* receive beamforming stats */
	uint16 rxndpa_u;			/* unicast NDPAs */
	uint16 rxndpa_m;			/* multicast NDPAs */
	uint16 rxbfpoll;			/* unicast bf-polls */
	uint16 bferpt;				/* beamforming reports */
	uint16 rxsf;
	uint16 rxcwrts;
	uint16 rxcwcts;
	uint16 rxtrig_bfpoll;
	uint16 unused_uint16;			/* pad */
	/* sounding stats - interval capture */
	uint16 rxnontb_sound[WL_TXBF_CNT_ARRAY_SZ];	/* non-TB sounding for last 8 captures */
	uint16 rxtb_sound[WL_TXBF_CNT_ARRAY_SZ];	/* TB sounding count for last 8 captures */
	uint32 cap_dur_ms[WL_TXBF_CNT_ARRAY_SZ];	/* last 8 capture durations (in ms) */
	uint32 cap_last_ts;			/* timestamp of last sample capture */
} wl_txbf_ecounters_v1_t;

/* security vulnerabilities counters */
typedef struct {
	uint32	ie_unknown;		/* number of unknown IEs */
	uint32	ie_invalid_length;	/* number of IEs with invalid length */
	uint32	ie_invalid_data;	/* number of IEs with invalid data */
	uint32	ipv6_invalid_length;	/* number of IPv6 packets with invalid payload length */
} wl_secvln_cnt_t;

/* Reinit reasons - do not put anything else other than reinit reasons here */
/* LEGACY STRUCTURE, DO NO MODIFY, SEE reinit_rsns_v1_t and further versions */
typedef struct {
	uint32 rsn[WL_REINIT_RC_LAST];
} reinit_rsns_t;

typedef struct {
	uint16 version;
	uint16 len;
	uint32 rsn[WL_REINIT_RC_LAST_V2 + 1u]; /* Note:WL_REINIT_RC_LAST_V2 is last value */
} reinit_rsns_v2_t;

/* MACXSTAT counters for ucodex (corerev >= 64) */
typedef struct {
	uint32 macxsusp;
	uint32 m2vmsg;
	uint32 v2mmsg;
	uint32 mboxout;
	uint32 musnd;
	uint32 sfb2v;
} wl_cnt_ge64mcxst_v1_t;

/** MACSTAT counters for ucode (corerev >= 40) */
typedef struct {
	/* MAC counters: 32-bit version of d11.h's macstat_t */
	uint32	txallfrm;	/**< total number of frames sent, incl. Data, ACK, RTS, CTS,
				 * Control Management (includes retransmissions)
				 */
	uint32	txrtsfrm;	/**< number of RTS sent out by the MAC */
	uint32	txctsfrm;	/**< number of CTS sent out by the MAC */
	uint32	txackfrm;	/**< number of ACK frames sent out */
	uint32	txdnlfrm;	/**< number of Null-Data transmission generated from template  */
	uint32	txbcnfrm;	/**< beacons transmitted */
	uint32	txfunfl[6];	/**< per-fifo tx underflows */
	uint32	txampdu;	/**< number of AMPDUs transmitted */
	uint32	txmpdu;		/**< number of MPDUs transmitted */
	uint32	txtplunfl;	/**< Template underflows (mac was too slow to transmit ACK/CTS
				 * or BCN)
				 */
	uint32	txphyerror;	/**< Transmit phy error, type of error is reported in tx-status for
				 * driver enqueued frames
				 */
	uint32  pktengrxducast; /**< unicast frames rxed by the pkteng code */
	uint32  pktengrxdmcast; /**< multicast frames rxed by the pkteng code */
	uint32	rxfrmtoolong;	/**< Received frame longer than legal limit (2346 bytes) */
	uint32	rxfrmtooshrt; /**< Received frame did not contain enough bytes for its frame type */
	uint32	rxanyerr;	/**< Any RX error that is not counted by other counters. */
	uint32	rxbadfcs;	/**< number of frames for which the CRC check failed in the MAC */
	uint32	rxbadplcp;	/**< parity check of the PLCP header failed */
	uint32	rxcrsglitch;	/**< PHY was able to correlate the preamble but not the header */
	uint32	rxstrt;		/**< Number of received frames with a good PLCP
				 * (i.e. passing parity check)
				 */
	uint32	rxdtucastmbss; /**< number of received DATA frames with good FCS and matching RA */
	uint32	rxmgucastmbss; /**< number of received mgmt frames with good FCS and matching RA */
	uint32	rxctlucast; /**< number of received CNTRL frames with good FCS and matching RA */
	uint32	rxrtsucast;	/**< number of unicast RTS addressed to the MAC (good FCS) */
	uint32	rxctsucast;	/**< number of unicast CTS addressed to the MAC (good FCS) */
	uint32	rxackucast;	/**< number of ucast ACKS received (good FCS) */
	uint32	rxdtocast; /**< number of received DATA frames (good FCS and not matching RA) */
	uint32	rxmgocast; /**< number of received MGMT frames (good FCS and not matching RA) */
	uint32	rxctlocast; /**< number of received CNTRL frame (good FCS and not matching RA) */
	uint32	rxrtsocast;	/**< number of received RTS not addressed to the MAC */
	uint32	rxctsocast;	/**< number of received CTS not addressed to the MAC */
	uint32	rxdtmcast;	/**< number of RX Data multicast frames received by the MAC */
	uint32	rxmgmcast;	/**< number of RX Management multicast frames received by the MAC */
	uint32	rxctlmcast;	/**< number of RX Control multicast frames received by the MAC
				 * (unlikely to see these)
				 */
	uint32	rxbeaconmbss;	/**< beacons received from member of BSS */
	uint32	rxdtucastobss; /**< number of unicast frames addressed to the MAC from
				  * other BSS (WDS FRAME)
				  */
	uint32	rxbeaconobss;	/**< beacons received from other BSS */
	uint32	rxrsptmout;	/**< number of response timeouts for transmitted frames
				 * expecting a response
				 */
	uint32	bcntxcancl;	/**< transmit beacons canceled due to receipt of beacon (IBSS) */
	uint32	rxnodelim;	/**< number of no valid delimiter detected by ampdu parser */
	uint32	rxf0ovfl;	/**< number of receive fifo 0 overflows */
	uint32	rxf1ovfl;	/**< number of receive fifo 1 overflows */
	uint32	rxhlovfl;	/**< number of length / header fifo overflows */
	uint32	missbcn_dbg;	/**< number of beacon missed to receive */
	uint32	pmqovfl;	/**< number of PMQ overflows */
	uint32	rxcgprqfrm;	/**< number of received Probe requests that made it into
				 * the PRQ fifo
				 */
	uint32	rxcgprsqovfl;	/**< Rx Probe Request Que overflow in the AP */
	uint32	txcgprsfail;	/**< Tx Probe Response Fail. AP sent probe response but did
				 * not get ACK
				 */
	uint32	txcgprssuc;	/**< Tx Probe Response Success (ACK was received) */
	uint32	prs_timeout;	/**< number of probe requests that were dropped from the PRQ
				 * fifo because a probe response could not be sent out within
				 * the time limit defined in M_PRS_MAXTIME
				 */
	uint32	txrtsfail;	/**< number of rts transmission failure that reach retry limit */
	uint32	txucast;	/**< number of unicast tx expecting response other than cts/cwcts */
	uint32  txinrtstxop;	/**< number of data frame transmissions during rts txop */
	uint32	rxback;		/**< blockack rxcnt */
	uint32	txback;		/**< blockack txcnt */
	uint32	bphy_rxcrsglitch;	/**< PHY count of bphy glitches */
	uint32	rxdrop20s;	/**< drop secondary cnt */
	uint32	rxtoolate;	/**< receive too late */
	uint32  bphy_badplcp;	/**< number of bad PLCP reception on BPHY rate */
	/* All counter variables have to be of uint32. */
} wl_cnt_ge40mcst_v1_t;

/** MACSTAT counters for ucode (corerev < 40) */
typedef struct {
	/* MAC counters: 32-bit version of d11.h's macstat_t */
	uint32	txallfrm;	/**< total number of frames sent, incl. Data, ACK, RTS, CTS,
				 * Control Management (includes retransmissions)
				 */
	uint32	txrtsfrm;	/**< number of RTS sent out by the MAC */
	uint32	txctsfrm;	/**< number of CTS sent out by the MAC */
	uint32	txackfrm;	/**< number of ACK frames sent out */
	uint32	txdnlfrm;	/**< number of Null-Data transmission generated from template  */
	uint32	txbcnfrm;	/**< beacons transmitted */
	uint32	txfunfl[6];	/**< per-fifo tx underflows */
	uint32	txampdu;	/**< number of AMPDUs transmitted */
	uint32	txmpdu;		/**< number of MPDUs transmitted */
	uint32	txtplunfl;	/**< Template underflows (mac was too slow to transmit ACK/CTS
				 * or BCN)
				 */
	uint32	txphyerror;	/**< Transmit phy error, type of error is reported in tx-status for
				 * driver enqueued frames
				 */
	uint32  pktengrxducast; /**< unicast frames rxed by the pkteng code */
	uint32  pktengrxdmcast; /**< multicast frames rxed by the pkteng code */
	uint32	rxfrmtoolong;	/**< Received frame longer than legal limit (2346 bytes) */
	uint32	rxfrmtooshrt; /**< Received frame did not contain enough bytes for its frame type */
	uint32	rxanyerr;	/**< Any RX error that is not counted by other counters. */
	uint32	rxbadfcs;	/**< number of frames for which the CRC check failed in the MAC */
	uint32	rxbadplcp;	/**< parity check of the PLCP header failed */
	uint32	rxcrsglitch;	/**< PHY was able to correlate the preamble but not the header */
	uint32	rxstrt;		/**< Number of received frames with a good PLCP
				 * (i.e. passing parity check)
				 */
	uint32	rxdtucastmbss; /**< number of received DATA frames with good FCS and matching RA */
	uint32	rxmgucastmbss; /**< number of received mgmt frames with good FCS and matching RA */
	uint32	rxctlucast; /**< number of received CNTRL frames with good FCS and matching RA */
	uint32	rxrtsucast;	/**< number of unicast RTS addressed to the MAC (good FCS) */
	uint32	rxctsucast;	/**< number of unicast CTS addressed to the MAC (good FCS) */
	uint32	rxackucast;	/**< number of ucast ACKS received (good FCS) */
	uint32	rxdtocast;  /**< number of received DATA frames (good FCS and not matching RA) */
	uint32	rxmgocast;  /**< number of received MGMT frames (good FCS and not matching RA) */
	uint32	rxctlocast; /**< number of received CNTRL frame (good FCS and not matching RA) */
	uint32	rxrtsocast;	/**< number of received RTS not addressed to the MAC */
	uint32	rxctsocast;	/**< number of received CTS not addressed to the MAC */
	uint32	rxdtmcast;	/**< number of RX Data multicast frames received by the MAC */
	uint32	rxmgmcast;	/**< number of RX Management multicast frames received by the MAC */
	uint32	rxctlmcast;	/**< number of RX Control multicast frames received by the MAC
				 * (unlikely to see these)
				 */
	uint32	rxbeaconmbss;	/**< beacons received from member of BSS */
	uint32	rxdtucastobss; /**< number of unicast frames addressed to the MAC from
				  * other BSS (WDS FRAME)
				  */
	uint32	rxbeaconobss;	/**< beacons received from other BSS */
	uint32	rxrsptmout;	/**< number of response timeouts for transmitted frames
				 * expecting a response
				 */
	uint32	bcntxcancl;	/**< transmit beacons canceled due to receipt of beacon (IBSS) */
	uint32	rxnodelim;	/**< number of no valid delimiter detected by ampdu parser */
	uint32	rxf0ovfl;	/**< number of receive fifo 0 overflows */
	uint32	dbgoff46;	/**< BTCX protection failure count,
				 * getting RX antenna in PHY DEBUG,
				 * PR84273 timeout count
				 */
	uint32	dbgoff47;	/**< BTCX preemption failure count,
				 * getting RX antenna in PHY DEBUG,
				 * PR84273 reset CCA count,
				 * RATEENGDBG
				 */
	uint32	dbgoff48;	/**< Used for counting txstatus queue overflow (corerev <= 4)  */
	uint32	pmqovfl;	/**< number of PMQ overflows */
	uint32	rxcgprqfrm;	/**< number of received Probe requests that made it into
				 * the PRQ fifo
				 */
	uint32	rxcgprsqovfl;	/**< Rx Probe Request Que overflow in the AP */
	uint32	txcgprsfail;	/**< Tx Probe Response Fail. AP sent probe response but did
				 * not get ACK
				 */
	uint32	txcgprssuc;	/**< Tx Probe Response Success (ACK was received) */
	uint32	prs_timeout;	/**< number of probe requests that were dropped from the PRQ
				 * fifo because a probe response could not be sent out within
				 * the time limit defined in M_PRS_MAXTIME
				 */
	uint32	txrtsfail;	/**< number of rts transmission failure that reach retry limit */
	uint32	txucast;	/**< number of unicast tx expecting response other than cts/cwcts */
	uint32  txinrtstxop;	/**< number of data frame transmissions during rts txop */
	uint32	rxback;		/**< blockack rxcnt */
	uint32	txback;		/**< blockack txcnt */
	uint32	bphy_rxcrsglitch;	/**< PHY count of bphy glitches */
	uint32	phywatch;	/**< number of phywatchdog to kill any pending transmissions.
				 * (PR 38187 corerev == 11)
				 */
	uint32	rxtoolate;	/**< receive too late */
	uint32  bphy_badplcp;	/**< number of bad PLCP reception on BPHY rate */
	/* All counter variables have to be of uint32. */
} wl_cnt_lt40mcst_v1_t;

/* ==== REV GE88 Counter Structs === */
/* Rev Ge88 TX specific macstats - version 1 */
typedef struct {
	uint32	txallfrm;			/**< num of frames sent, incl. Data, ACK, RTS, CTS,
						 * Control Management (includes retransmissions)
						 */
	uint32	txrtsfrm;			/**< number of RTS sent out by the MAC */
	uint32	txctsfrm;			/**< number of CTS sent out by the MAC */
	uint32	txackfrm;			/**< number of ACK frames sent out */
	uint32	txback;				/**< blockack txcnt */
	uint32	he_txmtid_back;			/**< number of mtid BAs */
	uint32	txdnlfrm;			/**< number of Null-Data tx from template  */
	uint32	txbcnfrm;			/**< beacons transmitted */
	uint32	txndpa;				/**< Number of TX NDPAs */
	uint32	txndp;				/**< Number of TX NDPs */
	uint32	txbfm;				/**< Number of TX Bfm cnt */
	uint32	txcwrts;			/**< Number of tx cw rts */
	uint32	txcwcts;			/**< Number of tx cw cts */
	uint32	txbfpoll;			/**< Number of tx bfpolls */
	uint32  txfbw;				/**< transmit at fallback bw (dynamic bw) */
	uint32	txampdu;			/**< number of AMPDUs transmitted */
	uint32	he_txmampdu;			/**< Number of tx m-ampdus */
	uint32	txmpdu;				/**< number of MPDUs transmitted */
	uint32	txucast;			/**< # of ucast tx expecting resp (not cts/cwcts) */
	uint32	he_txfrag;			/**< Number of tx frags */
	uint32	he_txtbppdu;			/**< increments on transmission of every TB PPDU */
	uint32	he_txtbppdu_ack;		/**< Number of tx HE TBPPDU acks */
	uint32  txinrtstxop;			/**< number of data frame tx during rts txop */
	uint32	null_txsts_empty;		/**< Number empty null-txstatus' */
	uint32	he_ulmu_disable;		/**< # of ULMU disables handled in ucode */
	uint32	he_ulmu_data_disable;		/**< number of UL MU data disable scenarios
						 * handled in ucode
						 */
	uint32	he_rxtrig_suppr_null_tbppdu;	/**<  count of null frame sent because of
						 * suppression scenarios
						 */
	uint32	he_null_zero_agg;		/**< nullAMPDU's transmitted in response to
						 * basic trigger because of zero aggregation
						 */
	uint32	he_null_tbppdu;			/**< null TBPPDU's sent as a response to
						 * basic trigger frame
						 */
	uint32	he_null_bsrp_rsp;		/**< null AMPDU's txed in response to BSR poll */
	uint32	he_null_fifo_empty;		/**< null AMPDU's in response to basic trigger
						 * because of no frames in fifo's
						 */
	uint32	txrtsfail;			/**< # of rts TX fails that reach retry limit */
	uint32	txcgprsfail;			/**< Tx Probe Response Fail.
						 * AP sent probe response but did not get ACK.
						 */
	uint32	bcntxcancl;			/**< TX bcns canceled due to rx of beacon (IBSS) */
	uint32	txtplunfl;			/**< Template unfl
						 *  (mac too slow to tx ACK/CTS or BCN)
						 */
	uint32	txphyerror;			/**< TX phyerr - reported in txs for
						 * driver queued frames
						 */
	uint32	ctmode_ufc_cnt;			/**< Number of UFCs with CT mode enabled */
	uint32	txshmunfl_cnt;			/**< TX SHM unfl cnt */
	uint32	txfunfl[11];			/**< per-fifo tx underflows */
	uint32	txfmlunfl[9];			/**< ML fifos underflow cnts */
	uint32	bferpt_inv_cfg;			/**< Invalid bfe report cfg */
	uint32	bferpt_drop_cnt1;		/**< bfe rpt drop cnt 1 */
	uint32	bferpt_drop_cnt2;		/**< bfe rpt drop cnt 2 */
	uint32	bferot_txcrs_high;		/**< bfe rpt tx crs high */
	uint32	txbfm_errcnt;			/**< TX bfm error cnt */
	uint32	PAD[23];			/**< PAD GAP */
	uint32	btcx_rfact_ctr_l;		/**< btcx rxfact counter low */
	uint32	btcx_rfact_ctr_h;		/**< btcx rxfact counter high */
	uint32	btcx_txconf_ctr_l;		/**< btcx txconf counter low */
	uint32	btcx_txconf_ctr_h;		/**< btcx txconf counter high */
	uint32	btcx_txconf_dur_ctr_l;		/**< btcx txconf duration counter low */
	uint32	btcx_txconf_dur_ctr_h;		/**< btcx txconf duration counter high */
	uint32	txcgprssuc;			/**< Tx Probe Response succ cnt */
	uint32	txsf;				/**< # of Tx'd SF */
	uint32	macsusp_cnt;			/**< # of macsuspends */
	uint32	prs_timeout;			/**< # of pre wds */
	uint32	emlsr_tx_nosrt;			/**< # of no TX starts for eMLSR */
} wl_cnt_ge88mcst_tx_v1_t;

/* Rev Ge88 RX specific macstats - version 1 */
typedef struct {
	uint32	rxstrt;			/**< Number of received frames with a good PLCP
					 * (i.e. passing parity check)
					 */
	uint32	rx20s_cnt;		/**< Increments if RXFrame does not include primary 20 */
	uint32	C_SECRSSI0;		/**< SEC RSSI0 info */
	uint32	C_SECRSSI1;		/**< SEC RSSI1 info */
	uint32	C_SECRSSI2;		/**< SEC RSSI2 info */
	uint32	C_CCA_RXPRI_LO;		/**< SEC RXPRI Low */
	uint32	C_CCA_RXPRI_HI;		/**< SEC RXPRI High */
	uint32	C_CCA_RXSEC20_LO;	/**< SEC CCA RX 20mhz low */
	uint32	C_CCA_RXSEC20_HI;	/**< SEC CCA RX 20mhz high */
	uint32	C_CCA_RXSEC40_LO;	/**< SEC CCA RX 40mhz low */
	uint32	C_CCA_RXSEC40_HI;	/**< SEC CCA RX 40mhz high */
	uint32	C_CCA_RXSEC80_LO;	/**< SEC CCA RX 80mhz low */
	uint32	C_CCA_RXSEC80_HI;	/**< SEC CCA RX 80mhz high */
	uint32	rxctlmcast;		/**< # of RX ctrl mcast frames */
	uint32  rxmgmcast;		/**< # of rx'd Management mcast frames */
	uint32	rxdtmcast;		/**< # of rx'd Data mcast frames */
	uint32	rxbeaconmbss;		/**< beacons rx'd from member of BSS */
	uint32	rxndpa_m;		/**< number of RX NDPA Multicast */
	uint32	rxrtsucast;		/**< # of ucast RTS (good FCS) */
	uint32	rxctsucast;		/**< # of ucast CTS (good FCS) */
	uint32	rxctlucast;		/**< # of rx'd CNTRL frames (good FCS & matching RA) */
	uint32	rxmgucastmbss;		/**< # of rx'd mgmt frames (good FCS & matching RA) */
	uint32	rxdtucastmbss;		/**< # of rx'd DATA frames (good FCS & matching RA) */
	uint32	rxackucast;		/**< number of ucast ACKS received (good FCS) */
	uint32	rxndpa_u;		/**< number of unicast RX NDPAs */
	uint32	rxsf;			/**< number of rxsfucast */
	uint32	rxcwrts;		/**< number of rx'd cw ucast rts */
	uint32	rxcwcts;		/**< number of rx'd cw ucast cts */
	uint32	rxbfpoll;		/**< number of rx'd BF ucast poll */
	uint32	pktengrxducast;		/**< number of rx'd good fcs ucast frames */
	uint32	pktengrxdmcast;		/**< number of rx'd good fcs ocast frames */
	uint32	rxdtocast;		/**< # of rx'd DATA frames (good FCS & not matching RA) */
	uint32	rxmgocast;		/**< # of rx'd MGMT frames (good FCS & not matching RA) */
	uint32	rxctlocast;		/**< # of rx'd CNTRL frame (good FCS & not matching RA) */
	uint32	rxrtsocast;		/**< # of rx'd RTS not addressed */
	uint32	rxctsocast;		/**< # of rx'd CTS not addressed */
	uint32	rxdtucastobss;		/**< number of unicast frames addressed to the MAC from
					 * other BSS (WDS FRAME)
					 */
	uint32  rxbeaconobss;		/* beacons rx'd from other BSS */
	uint32	he_rx_ppdu_cnt;		/**< rx'd HE PPDU cnt */
	uint32	he_rxstrt_hesuppdu_cnt;	/**< rx'd HE su PPDU cnt */
	uint32	he_rxstrt_hesureppdu_cnt; /**< rx'd HE SU RE PPDU cnt */
	uint32	he_rxtsrt_hemuppdu_cnt;	/**< rx'd HE MU PPDU cnt */
	uint32	rxbar;			/**< number of rx'd BARs */
	uint32	rxback;			/**< number of rx'd BARs */
	uint32	he_rxmtid_back;		/**< number of rx'd HE RX MultiTID BAs */
	uint32	he_rxmsta_back;		/**< number of rx'd HE RX MultiSTA BAs */
	uint32	bferpt;			/**< number of rx'd BFE report ready cnts */
	uint32	goodfcs;		/**< number of rx'd goodfcs cnts */
	uint32	he_colormiss_cnt;	/**< HE BSS color mismatch counts cnts */
	uint32	he_rxdefrag;		/**< number of rx'd HE dynamic fragmented pkts */
	uint32	he_rxdlmu;		/**< number of rx'd DL MU frames */
	uint32	rxcgprqfrm;		/**< number of received Probe requests that made it into
					 * the PRQ fifo
					 */
	uint32	rx_fp_shm_corrupt_cnt;	/**< SHM corrupt count */
	uint32	PAD[11];		/**< PAD Gap */
	uint32	rxanyerr;		/**< Any RX error that is not counted by other counters. */
	uint32	rxbadfcs;		/**< # of frames with CRC check failed */
	uint32	rxbadplcp;		/**< parity check of the PLCP header failed */
	uint32	rxcrsglitch;		/**< PHY able to correlate the plcp but not the hdr */
	uint32	rxfrmtoolong;		/**< rx'd frame longer than legal limit (2346 bytes) */
	uint32	rxfrmtooshrt;		/**< rx'd frame not enough bytes for ft */
	uint32	rxnodelim;		/**< # of not valid delim -> ampdu parser */
	uint32	rxbad_ampdu;		/**< number of rx'd bad ampdus */
	uint32  rxcgprsqovfl;		/**< Rx Probe Request Que overflow in the AP */
	uint32	bphy_rxcrsglitch;	/**< PHY count of bphy glitches */
	uint32	rxdrop20s;		/**< drop secondary cnt */
	uint32	rxtoolate;		/**< receive too late */
	uint32	m_pfifo_drop;		/**< # of pfifo dropped frames */
	uint32  bphy_badplcp;		/**< number of bad PLCP reception on BPHY rate */
	uint32	phyovfl;		/**< number of phy overflows */
	uint32	rxf0ovfl;		/**< number of rx fifo 0 overflows */
	uint32	rxf1ovfl;		/**< number of rx fifo 1 overflows */
	uint32	lenfovfl;		/**< number of length overflows */
	uint32	badplcp;		/**< parity check of the PLCP header failed */
	uint32	rxerr_stat;		/**< rx error statistics */
	uint32	stsfifofull;		/**< status fifo full */
	uint32	stsfifoerr;		/**< status fifo error */
	uint32	ctx_fifo_full;		/**< fw not draining frames fast enough */
	uint32	ctx_fifo2_full;		/**< fw not draining frames fast enough */
	uint32	missbcn_dbg;		/**< number of beacon missed to receive */
	uint32	rxrsptmout;		/**< number of response timeouts for tx'd frames */
	uint32	laterx_cnt;		/**< ucode sees frame 30us late */
	uint32	bcn_drop_cnt;		/**< number of BCNs dropped in ucode */
	uint32	bfr_timeout;		/**< number of bfr timeouts */
	uint32	rxgaininfo_ant0;	/**< ANT-0 phy RX gain info - main? */
	uint32	rxauxgaininfo_ant0;	/**< ANT-0 phy RX gain info - aux */
	uint32	he_rxtrig_myaid;	/**< number of rx'd valid trigger frame with myaid */
	uint32	he_rxtrig_rand;		/**< number of rx'd valid trigger frame with random aid */
	uint32	he_rxtrig_basic;	/**< number of rx'd of basic trigger frame */
	uint32	he_rxtrig_bfm_cnt;	/**< number of rx'd trigger frame with bfm */
	uint32	he_rxtrig_mubar;	/**< number of rx'd MUBAR trigger frame variant */
	uint32	he_rxtrig_murts;	/**< number of rx'd MU-RTS trigger frame variant */
	uint32	he_rxtrig_bsrp;		/**< number of rx'd of BSR poll trigger frame variant */
	uint32	he_rxtrig_gcrmubar;	/**< number of rx'd gcr mu bar trigger frame variant? */
	uint32	he_rxtrig_bqrp;		/**< number of rx'd bqrp trigger frame variant? */
	uint32	he_rxtrig_nfrp;		/**< Todo: check on functionality */
	uint32	he_rxtrig_basic_htpack;	/**< triggers received with HTP ack policy */
	uint32	he_cs_req_tx_cancel;	/**< tx cancelled due to trigger rx or ch sw? */
	uint32	he_rxtrig_rngpoll;	/**< todo: check functionality */
	uint32	he_rxtrig_rngsnd;	/**< todo: check functionality */
	uint32	he_rxtrig_rngssnd;	/**< todo: check functionality */
	uint32	he_rxtrig_rngrpt;	/**< todo: check functionality */
	uint32	he_rxtrig_rngpasv;	/**< todo: check functionality */
	uint32	he_rxtrig_ru_2x996T;	/**< Rx'd trigger frame with STA RU index 160mhz */
	uint32	he_rxtrig_invalid_ru;	/**< Rx'd trigger frame with invalid STA20 RU index */
	uint32	he_rxtrig_inv_ru_cnt;	/**< # of Rx'd trigger frames with invalid RU cnt */
	uint32	he_rxtrig_drop_cnt;	/**< # of trigger frames dropped */
	uint32	ndp_fail_cnt;		/**< # of NDP fails */
	uint32	rxfrmtoolong2_cnt;	/**< # of Rx'd too long pkts */
	uint32	hwaci_status;		/**< HW ACI status */
	uint32	pmqovfl;		/**< number of PMQ overflows */
} wl_cnt_ge88mcst_rx_v1_t;

/* Rev GE88 per ML link supportive wl counters (macstats) - version 1 */
typedef struct wl_macst_rx_ge88mcst {
	uint8	num_links;	/* Number of per-link stats supported on slice */
	uint8	pad[3];

	/* Per ML Link RX macstats (esp. eMLSR) */
	wl_cnt_ge88mcst_rx_v1_t cnt[];
} wl_macst_rx_ge88mcst_v1_t;

/* Rev GE88 per ML link supportive wl counters (macstats) - version 1 */
typedef struct wl_macst_tx_ge88mcst {
	uint8	num_links;	/* Number of per-link stats supported on slice */
	uint8	pad[3];

	/* Per ML Link TX macstats (esp. eMLSR) */
	wl_cnt_ge88mcst_tx_v1_t cnt[];
} wl_macst_tx_ge88mcst_v1_t;

/* Rev Ge88 TX specific macstats - version 2 */
typedef struct {
	uint32	txallfrm;			/**< num of frames sent, incl. Data, ACK, RTS, CTS,
						 * Control Management (includes retransmissions)
						 */
	uint32	txrtsfrm;			/**< number of RTS sent out by the MAC */
	uint32	txctsfrm;			/**< number of CTS sent out by the MAC */
	uint32	txackfrm;			/**< number of ACK frames sent out */
	uint32	txback;				/**< blockack txcnt */
	uint32	he_txmtid_back;			/**< number of mtid BAs */
	uint32	txdnlfrm;			/**< number of Null-Data tx from template  */
	uint32	txbcnfrm;			/**< beacons transmitted */
	uint32	txndpa;				/**< Number of TX NDPAs */
	uint32	txndp;				/**< Number of TX NDPs */
	uint32	txbfm;				/**< Number of TX Bfm cnt */
	uint32	txcwrts;			/**< Number of tx cw rts */
	uint32	txcwcts;			/**< Number of tx cw cts */
	uint32	txbfpoll;			/**< Number of tx bfpolls */
	uint32  txfbw;				/**< transmit at fallback bw (dynamic bw) */
	uint32	txampdu;			/**< number of AMPDUs transmitted */
	uint32	he_txmampdu;			/**< Number of tx m-ampdus */
	uint32	txucast;			/**< # of ucast tx expecting resp (not cts/cwcts) */
	uint32	he_txfrag;			/**< Number of tx frags */
	uint32	he_txtbppdu;			/**< increments on transmission of every TB PPDU */
	uint32	he_txtbppdu_ack;		/**< Number of tx HE TBPPDU acks */
	uint32  txinrtstxop;			/**< number of data frame tx during rts txop */
	uint32	null_txsts_empty;		/**< Number empty null-txstatus' */
	uint32	he_ulmu_disable;		/**< # of ULMU disables handled in ucode */
	uint32	he_ulmu_data_disable;		/**< number of UL MU data disable scenarios
						 * handled in ucode
						 */
	uint32	he_rxtrig_suppr_null_tbppdu;	/**<  count of null frame sent because of
						 * suppression scenarios
						 */
	uint32	he_null_zero_agg;		/**< nullAMPDU's transmitted in response to
						 * basic trigger because of zero aggregation
						 */
	uint32	he_null_tbppdu;			/**< null TBPPDU's sent as a response to
						 * basic trigger frame
						 */
	uint32	he_null_bsrp_rsp;		/**< null AMPDU's txed in response to BSR poll */
	uint32	he_null_fifo_empty;		/**< null AMPDU's in response to basic trigger
						 * because of no frames in fifo's
						 */
	uint32	txrtsfail;			/**< # of rts TX fails that reach retry limit */
	uint32	txcgprsfail;			/**< Tx Probe Response Fail.
						 * AP sent probe response but did not get ACK.
						 */
	uint32	bcntxcancl;			/**< TX bcns canceled due to rx of beacon (IBSS) */
	uint32	txtplunfl;			/**< Template unfl
						 *  (mac too slow to tx ACK/CTS or BCN)
						 */
	uint32	txphyerror;			/**< TX phyerr - reported in txs for
						 * driver queued frames
						 */
	uint32	txshmunfl_cnt;			/**< TX SHM unfl cnt */
	uint32	txfunfl[11];			/**< per-fifo tx underflows */
	uint32	txfmlunfl[9];			/**< ML fifos underflow cnts */
	uint32	bferpt_inv_cfg;			/**< Invalid bfe report cfg */
	uint32	bferpt_drop_cnt1;		/**< bfe rpt drop cnt 1 */
	uint32	bferpt_drop_cnt2;		/**< bfe rpt drop cnt 2 */
	uint32	bferot_txcrs_high;		/**< bfe rpt tx crs high */
	uint32	txbfm_errcnt;			/**< TX bfm error cnt */
	uint32	tx_murts_cnt;			/**< Tx MURTS Count */
	uint32	tx_noavail_cnt;			/**< Tx Not avail Count */
	uint32	tx_null_link_pref;		/**< Null Link Pref */
	uint32	btcx_rfact_ctr_l;		/**< btcx rxfact counter low */
	uint32	btcx_rfact_ctr_h;		/**< btcx rxfact counter high */
	uint32	btcx_txconf_ctr_l;		/**< btcx txconf counter low */
	uint32	btcx_txconf_ctr_h;		/**< btcx txconf counter high */
	uint32	btcx_txconf_dur_ctr_l;		/**< btcx txconf duration counter low */
	uint32	btcx_txconf_dur_ctr_h;		/**< btcx txconf duration counter high */
	uint32	txcgprssuc;			/**< Tx Probe Response succ cnt */
	uint32	txsf;				/**< # of Tx'd SF */
	uint32	macsusp_cnt;			/**< # of macsuspends */
	uint32	prs_timeout;			/**< # of pre wds */
	uint32	emlsr_tx_nosrt;			/**< # of no TX starts for eMLSR */
	uint32	rts_to_self_cnt;		/**< # of RTS to self */
	uint32	saqm_sendfrm_agg_cnt;		/**< # SAQM Send frame aggregation */
	uint32	txbcn_phyerr_cnt;		/**< # Tx Beacon Phy error */
	uint32	he_txtrig;			/**< # Tx Trigger Frames */
} wl_cnt_ge88mcst_tx_v2_t;

/* Rev Ge88 RX specific macstats - version 2 */
typedef struct {
	uint32	rxstrt;			/**< Number of received frames with a good PLCP
					 * (i.e. passing parity check)
					 */
	uint32	rx20s_cnt;		/**< Increments if RXFrame does not include primary 20 */
	uint32	C_SECRSSI0;		/**< SEC RSSI0 info */
	uint32	C_SECRSSI1;		/**< SEC RSSI1 info */
	uint32	C_SECRSSI2;		/**< SEC RSSI2 info */
	uint32	C_CCA_RXPRI_LO;		/**< SEC RXPRI Low */
	uint32	C_CCA_RXPRI_HI;		/**< SEC RXPRI High */
	uint32	C_CCA_RXSEC20_LO;	/**< SEC CCA RX 20mhz low */
	uint32	C_CCA_RXSEC20_HI;	/**< SEC CCA RX 20mhz high */
	uint32	C_CCA_RXSEC40_LO;	/**< SEC CCA RX 40mhz low */
	uint32	C_CCA_RXSEC40_HI;	/**< SEC CCA RX 40mhz high */
	uint32	C_CCA_RXSEC80_LO;	/**< SEC CCA RX 80mhz low */
	uint32	C_CCA_RXSEC80_HI;	/**< SEC CCA RX 80mhz high */
	uint32	rxctlmcast;		/**< # of RX ctrl mcast frames */
	uint32	rxmgmcast;		/**< # of rx'd Management mcast frames */
	uint32	rxbeaconmbss;		/**< beacons rx'd from member of BSS */
	uint32	rxndpa_m;		/**< number of RX NDPA Multicast */
	uint32	rxrtsucast;		/**< # of ucast RTS (good FCS) */
	uint32	rxctsucast;		/**< # of ucast CTS (good FCS) */
	uint32	rxctlucast;		/**< # of rx'd CNTRL frames (good FCS & matching RA) */
	uint32	rxmgucastmbss;		/**< # of rx'd mgmt frames (good FCS & matching RA) */
	uint32	rxackucast;		/**< number of ucast ACKS received (good FCS) */
	uint32	rxndpa_u;		/**< number of unicast RX NDPAs */
	uint32	rxsf;			/**< number of rxsfucast */
	uint32	rxcwrts;		/**< number of rx'd cw ucast rts */
	uint32	rxcwcts;		/**< number of rx'd cw ucast cts */
	uint32	rxbfpoll;		/**< number of rx'd BF ucast poll */
	uint32	rxmgocast;		/**< # of rx'd MGMT frames (good FCS & not matching RA) */
	uint32	rxctlocast;		/**< # of rx'd CNTRL frame (good FCS & not matching RA) */
	uint32	rxrtsocast;		/**< # of rx'd RTS not addressed */
	uint32	rxctsocast;		/**< # of rx'd CTS not addressed */
	uint32	rxbeaconobss;		/* beacons rx'd from other BSS */
	uint32	he_rxstrt_hesuppdu_cnt;	/**< rx'd HE su PPDU cnt */
	uint32	he_rxstrt_hesureppdu_cnt; /**< rx'd HE SU RE PPDU cnt */
	uint32	he_rxtsrt_hemuppdu_cnt;	/**< rx'd HE MU PPDU cnt */
	uint32	rxbar;			/**< number of rx'd BARs */
	uint32	rxback;			/**< number of rx'd BARs */
	uint32	he_rxmtid_back;		/**< number of rx'd HE RX MultiTID BAs */
	uint32	he_rxmsta_back;		/**< number of rx'd HE RX MultiSTA BAs */
	uint32	bferpt;			/**< number of rx'd BFE report ready cnts */
	uint32	he_colormiss_cnt;	/**< HE BSS color mismatch counts cnts */
	uint32	he_rxdefrag;		/**< number of rx'd HE dynamic fragmented pkts */
	uint32	he_rxdlmu;		/**< number of rx'd DL MU frames */
	uint32	rxcgprqfrm;		/**< number of received Probe requests that made it into
					 * the PRQ fifo
					 */
	uint32	rx_fp_shm_corrupt_cnt;	/**< SHM corrupt count */
	uint32	he_physu_rx;		/**< Number of PHY SU Frames received */
	uint32	he_phyru_rx;		/**< Number of PHY RU Frames received */
	uint32	PAD[17];		/**< PAD Gap */
	uint32	rxbadplcp;		/**< parity check of the PLCP header failed */
	uint32	rxcrsglitch;		/**< PHY able to correlate the plcp but not the hdr */
	uint32	rxfrmtoolong;		/**< rx'd frame longer than legal limit (2346 bytes) */
	uint32	rxfrmtooshrt;		/**< rx'd frame not enough bytes for ft */
	uint32	rxnodelim;		/**< # of not valid delim -> ampdu parser */
	uint32	rxbad_ampdu;		/**< number of rx'd bad ampdus */
	uint32	rxcgprsqovfl;		/**< Rx Probe Request Que overflow in the AP */
	uint32	bphy_rxcrsglitch;	/**< PHY count of bphy glitches */
	uint32	rxdrop20s;		/**< drop secondary cnt */
	uint32	rxtoolate;		/**< receive too late */
	uint32	m_pfifo_drop;		/**< # of pfifo dropped frames */
	uint32	bphy_badplcp;		/**< number of bad PLCP reception on BPHY rate */
	uint32	phyovfl;		/**< number of phy overflows */
	uint32	rxf0ovfl;		/**< number of rx fifo 0 overflows */
	uint32	rxf1ovfl;		/**< number of rx fifo 1 overflows */
	uint32	lenfovfl;		/**< number of length overflows */
	uint32	weppeof;		/**< number of weppeof  */
	uint32	badplcp;		/**< parity check of the PLCP header failed */
	uint32	stsfifofull;		/**< status fifo full */
	uint32	stsfifoerr;		/**< status fifo error */
	uint32	ctx_fifo_full;		/**< fw not draining frames fast enough */
	uint32	ctx_fifo2_full;		/**< fw not draining frames fast enough */
	uint32	missbcn_dbg;		/**< number of beacon missed to receive */
	uint32	rxrsptmout;		/**< number of response timeouts for tx'd frames */
	uint32	laterx_cnt;		/**< ucode sees frame 30us late */
	uint32	bcn_drop_cnt;		/**< number of BCNs dropped in ucode */
	uint32	bfr_timeout;		/**< number of bfr timeouts */
	uint32	rxgaininfo_ant0;	/**< ANT-0 phy RX gain info - main? */
	uint32	rxauxgaininfo_ant0;	/**< ANT-0 phy RX gain info - aux */
	uint32	he_rxtrig_myaid;	/**< number of rx'd valid trigger frame with myaid */
	uint32	he_rxtrig_rand;		/**< number of rx'd valid trigger frame with random aid */
	uint32	he_rxtrig_basic;	/**< number of rx'd of basic trigger frame */
	uint32	he_rxtrig_bfm_cnt;	/**< number of rx'd trigger frame with bfm */
	uint32	he_rxtrig_mubar;	/**< number of rx'd MUBAR trigger frame variant */
	uint32	he_rxtrig_murts;	/**< number of rx'd MU-RTS trigger frame variant */
	uint32	he_rxtrig_bsrp;		/**< number of rx'd of BSR poll trigger frame variant */
	uint32	he_rxtrig_gcrmubar;	/**< number of rx'd gcr mu bar trigger frame variant? */
	uint32	he_rxtrig_bqrp;		/**< number of rx'd bqrp trigger frame variant? */
	uint32	he_rxtrig_nfrp;		/**< Todo: check on functionality */
	uint32	he_rxtrig_basic_htpack;	/**< triggers received with HTP ack policy */
	uint32	he_cs_req_tx_cancel;	/**< tx cancelled due to trigger rx or ch sw? */
	uint32	he_rxtrig_rngpoll;	/**< todo: check functionality */
	uint32	he_rxtrig_rngsnd;	/**< todo: check functionality */
	uint32	he_rxtrig_rngssnd;	/**< todo: check functionality */
	uint32	he_rxtrig_rngrpt;	/**< todo: check functionality */
	uint32	he_rxtrig_rngpasv;	/**< todo: check functionality */
	uint32	he_rxtrig_ru_2x996T;	/**< Rx'd trigger frame with STA RU index 160mhz */
	uint32	he_rxtrig_invalid_ru;	/**< Rx'd trigger frame with invalid STA20 RU index */
	uint32	he_rxtrig_inv_ru_cnt;	/**< # of Rx'd trigger frames with invalid RU cnt */
	uint32	he_rxtrig_drop_cnt;	/**< # of trigger frames dropped */
	uint32	ndp_fail_cnt;		/**< # of NDP fails */
	uint32	rxfrmtoolong2_cnt;	/**< # of Rx'd too long pkts */
	uint32	hwaci_status;		/**< HW ACI status */
	uint32	pmqovfl;		/**< number of PMQ overflows */
	uint32	sctrg_rxcrs_drop_cnt;	/**< Number of scan trigger dropped due to rxcrs */
	uint32	inv_punc_usig_cnt;	/**< Number of invalid punctured USIG */
	uint32	sctrg_drop_cnt;		/**< Number of scan trigger drop */
	uint32	he_wrong_nss;		/**< Number of triggers with wrong NSS */
	uint32	he_trig_unsupp_rate;	/**< Number of triggers with unsupported rates */
} wl_cnt_ge88mcst_rx_v2_t;

/* Rev GE88 per ML link supportive wl counters (macstats) - version 1 */
typedef struct wl_macst_rx_ge88mcst_v2 {
	uint8	num_links;	/* Number of per-link stats supported on slice */
	uint8	pad[3];

	/* Per ML Link RX macstats (esp. eMLSR) */
	wl_cnt_ge88mcst_rx_v2_t cnt[];
} wl_macst_rx_ge88mcst_v2_t;

/* Rev GE88 per ML link supportive wl counters (macstats) - version 2 */
typedef struct wl_macst_tx_ge88mcst_v2 {
	uint8	num_links;	/* Number of per-link stats supported on slice */
	uint8	pad[3];

	/* Per ML Link TX macstats (esp. eMLSR) */
	wl_cnt_ge88mcst_tx_v2_t cnt[];
} wl_macst_tx_ge88mcst_v2_t;

/* Rev Ge88 TX 32 specific macstats - version 1 */
typedef struct {
	uint32	txmpdu;			/**< number of MPDUs transmitted */
	uint32	ctmode_ufc_cnt;		/**< Number of UFCs with CT mode enabled */
} wl_cnt_ge88mcst_tx_u32_v1_t;

/* Rev Ge88 RX 32 specific macstats - version 1 */
typedef struct {
	uint32 rxdtucastmbss;	/**< # of rx'd DATA frames (good FCS & matching RA) */
	uint32 pktengrxducast;	/**< number of rx'd good fcs ucast frames */
	uint32 pktengrxdmcast;	/**< number of rx'd good fcs mcast frames */
	uint32 rxdtocast;		/**< # of rx'd DATA frames (good FCS & not matching RA) */
	uint32 rxdtucastobss;	/**< number of unicast frames addressed to the MAC from
					 * other BSS (WDS FRAME)
					 */
	uint32 goodfcs;		/**< number of rx'd goodfcs cnts */
	uint32 rxdtmcast;	/**< # of rx'd Data mcast frames */
	uint32 rxanyerr;	/**< Any RX error that is not counted by other counters */
	uint32 rxbadfcs;	/**< # of frames with CRC check failed */
} wl_cnt_ge88mcst_rx_u32_v1_t;

/* Rev GE88 per ML link supportive wl counters (macstats) - version 1 */
typedef struct wl_macst_rx_ge88mcst_u32 {
	uint8	num_links;	/* Number of per-link stats supported on slice */
	uint8	pad[3];

	/* Per ML Link RX macstats (esp. eMLSR) */
	wl_cnt_ge88mcst_rx_u32_v1_t cnt[];
} wl_macst_rx_ge88mcst_u32_v1_t;

/* Rev GE88 per ML link supportive wl counters (macstats) - version 1 */
typedef struct wl_macst_tx_ge88mcst_u32 {
	uint8	num_links;	/* Number of per-link stats supported on slice */
	uint8	pad[3];

	/* Per ML Link TX macstats (esp. eMLSR) */
	wl_cnt_ge88mcst_tx_u32_v1_t cnt[];
} wl_macst_tx_ge88mcst_u32_v1_t;

/* ********** v3 start ************* */
/* wrapper structure contain link_idx values which might not be same as the actual array ix */
typedef struct wl_cnt_ge88mcst_rx_wrap_v1 {
	uint8	link_idx;
	uint8	d11_cntr_idx;		/* shmem counter block  identifier 0 or 1 */
	uint8	pad[2];
	wl_cnt_ge88mcst_rx_v2_t cnt;
} wl_cnt_ge88mcst_rx_wrap_v1_t;

typedef struct wl_cnt_ge88mcst_tx_wrap_v1 {
	uint8	link_idx;
	uint8	d11_cntr_idx;		/* shmem counter block  identifier 0 or 1 */
	uint8	pad[2];
	wl_cnt_ge88mcst_tx_v2_t cnt;
} wl_cnt_ge88mcst_tx_wrap_v1_t;

typedef struct wl_cnt_ge88mcst_rx_u32_wrap_v1 {
	uint8	link_idx;
	uint8	d11_cntr_idx;		/* shmem counter block  identifier 0 or 1 */
	uint8	pad[2];
	wl_cnt_ge88mcst_rx_u32_v1_t cnt;
} wl_cnt_ge88mcst_rx_u32_wrap_v1_t;

typedef struct wl_cnt_ge88mcst_tx_u32_wrap_v1 {
	uint8	link_idx;
	uint8	d11_cntr_idx;		/* shmem counter block  identifier 0 or 1 */
	uint8	pad[2];
	wl_cnt_ge88mcst_tx_u32_v1_t cnt;
} wl_cnt_ge88mcst_tx_u32_wrap_v1_t;

/* Rev GE88 per ML link supportive wl counters (macstats) - version 3 */
typedef struct wl_macst_rx_ge88mcst_v3 {
	uint8	num_links;	/* Number of per-link stats supported on slice */
	uint8	pad[3];
	/* Per ML Link RX macstats (esp. eMLSR) */
	wl_cnt_ge88mcst_rx_wrap_v1_t cnt_wrap[];
} wl_macst_rx_ge88mcst_v3_t;

/* Rev GE88 per ML link supportive wl counters (macstats) - version 3 */
typedef struct wl_macst_tx_ge88mcst_v3 {
	uint8	num_links;	/* Number of per-link stats supported on slice */
	uint8	pad[3];
	/* Per ML Link TX macstats (esp. eMLSR) */
	wl_cnt_ge88mcst_tx_wrap_v1_t cnt_wrap[];
} wl_macst_tx_ge88mcst_v3_t;

/* Rev GE88 per ML link supportive wl counters (macstats) - version 3 */
typedef struct wl_macst_rx_ge88mcst_u32_v3 {
	uint8	num_links;	/* Number of per-link stats supported on slice */
	uint8	pad[3];
	/* Per ML Link RX macstats (esp. eMLSR) */
	wl_cnt_ge88mcst_rx_u32_wrap_v1_t cnt_wrap[];
} wl_macst_rx_ge88mcst_u32_v3_t;

/* Rev GE88 per ML link supportive wl counters (macstats) - version 3 */
typedef struct wl_macst_tx_ge88mcst_u32_v3 {
	uint8	num_links;	/* Number of per-link stats supported on slice */
	uint8	pad[3];
	/* Per ML Link TX macstats (esp. eMLSR) */
	wl_cnt_ge88mcst_tx_u32_wrap_v1_t cnt_wrap[];
} wl_macst_tx_ge88mcst_u32_v3_t;
/* ********** v3 end ************* */

/* ********** update v4 START **** */
/* TX specific macstats - v3 for reporting struct v4 */
typedef struct wl_cnt_mcst_tx_v3 {
	uint32	txallfrm;			/**< num of frames sent, incl. Data, ACK, RTS, CTS,
						 * Control Management (includes retransmissions)
						 */
	uint32	txrtsfrm;			/**< number of RTS sent out by the MAC */
	uint32	txctsfrm;			/**< number of CTS sent out by the MAC */
	uint32	txackfrm;			/**< number of ACK frames sent out */
	uint32	txback;				/**< blockack txcnt */
	uint32	he_txmtid_back;			/**< number of mtid BAs */
	uint32	txdnlfrm;			/**< number of Null-Data tx from template  */
	uint32	txbcnfrm;			/**< beacons transmitted */
	uint32	txndpa;				/**< Number of TX NDPAs */
	uint32	txndp;				/**< Number of TX NDPs */
	uint32	txbfm;				/**< Number of TX Bfm cnt */
	uint32	txcwrts;			/**< Number of tx cw rts */
	uint32	txcwcts;			/**< Number of tx cw cts */
	uint32	txbfpoll;			/**< Number of tx bfpolls */
	uint32  txfbw;				/**< transmit at fallback bw (dynamic bw) */
	uint32	txampdu;			/**< number of AMPDUs transmitted */
	uint32	he_txmampdu;			/**< Number of tx m-ampdus */
	uint32	txucast;			/**< # of ucast tx expecting resp (not cts/cwcts) */
	uint32	he_txfrag;			/**< Number of tx frags */
	uint32	he_txtbppdu;			/**< increments on transmission of every TB PPDU */
	uint32	he_txtbppdu_ack;		/**< Number of tx HE TBPPDU acks */
	uint32  txinrtstxop;			/**< number of data frame tx during rts txop */
	uint32	null_txsts_empty;		/**< Number empty null-txstatus' */
	uint32	he_ulmu_disable;		/**< # of ULMU disables handled in ucode */
	uint32	he_ulmu_data_disable;		/**< number of UL MU data disable scenarios
						 * handled in ucode
						 */
	uint32	he_rxtrig_suppr_null_tbppdu;	/**<  count of null frame sent because of
						 * suppression scenarios
						 */
	uint32	he_null_zero_agg;		/**< nullAMPDU's transmitted in response to
						 * basic trigger because of zero aggregation
						 */
	uint32	he_null_tbppdu;			/**< null TBPPDU's sent as a response to
						 * basic trigger frame
						 */
	uint32	he_null_bsrp_rsp;		/**< null AMPDU's txed in response to BSR poll */
	uint32	he_null_fifo_empty;		/**< null AMPDU's in response to basic trigger
						 * because of no frames in fifo's
						 */
	uint32	txrtsfail;			/**< # of rts TX fails that reach retry limit */
	uint32	txcgprsfail;			/**< Tx Probe Response Fail.
						 * AP sent probe response but did not get ACK.
						 */
	uint32	bcntxcancl;			/**< TX bcns canceled due to rx of beacon (IBSS) */
	uint32	txtplunfl;			/**< Template unfl
						 *  (mac too slow to tx ACK/CTS or BCN)
						 */
	uint32	txphyerror;			/**< TX phyerr - reported in txs for
						 * driver queued frames
						 */
	uint32	txshmunfl_cnt;			/**< TX SHM unfl cnt */
	uint32	txfunfl[11];			/**< per-fifo tx underflows */
	uint32	txfmlunfl[12];			/**< ML fifos underflow cnts */
	uint32	bferpt_inv_cfg;			/**< Invalid bfe report cfg */
	uint32	bferpt_drop_cnt1;		/**< bfe rpt drop cnt 1 */
	uint32	bferpt_drop_cnt2;		/**< bfe rpt drop cnt 2 */
	uint32	bferot_txcrs_high;		/**< bfe rpt tx crs high */
	uint32	txbfm_errcnt;			/**< TX bfm error cnt */
	uint32	tx_murts_cnt;			/**< Tx MURTS Count */
	uint32	tx_noavail_cnt;			/**< Tx Not avail Count */
	uint32	tx_null_link_pref;		/**< Null Link Pref */
	uint32	btcx_rfact_ctr_l;		/**< btcx rxfact counter low */
	uint32	btcx_rfact_ctr_h;		/**< btcx rxfact counter high */
	uint32	btcx_txconf_ctr_l;		/**< btcx txconf counter low */
	uint32	btcx_txconf_ctr_h;		/**< btcx txconf counter high */
	uint32	btcx_txconf_dur_ctr_l;		/**< btcx txconf duration counter low */
	uint32	btcx_txconf_dur_ctr_h;		/**< btcx txconf duration counter high */
	uint32	txcgprssuc;			/**< Tx Probe Response succ cnt */
	uint32	txsf;				/**< # of Tx'd SF */
	uint32	macsusp_cnt;			/**< # of macsuspends */
	uint32	prs_timeout;			/**< # of pre wds */
	uint32	emlsr_tx_nosrt;			/**< # of no TX starts for eMLSR */
	uint32	rts_to_self_cnt;		/**< # of RTS to self */
	uint32	saqm_sendfrm_agg_cnt;		/**< # SAQM Send frame aggregation */
	uint32	txbcn_phyerr_cnt;		/**< # Tx Beacon Phy error */
	uint32	he_txtrig;			/**< # Tx Trigger Frames */

	uint32	txctsfrm_infra;			/**< # CTS sent out by the MAC for infra */
	uint32	norxfrm_aftertxcts;		/**< # rxframe after cts total */
	uint32	norxfrm_aftertxcts_infra;	/**< # rxframe after cts only for infra */
	uint32	norxfrm_aftertxcts_mu_cnt;	/**< # rxframe after TX cts for MU */
} wl_cnt_mcst_tx_v3_t;

typedef struct wl_cnt_mcst_tx_wrap_v2 {
	uint8	link_idx;
	uint8	d11_cntr_idx;		/* shmem counter block  identifier 0 or 1 */
	uint8	pad[2];
	wl_cnt_mcst_tx_v3_t cnt;
} wl_cnt_mcst_tx_wrap_v2_t;

/* per ML link supportive wl counters (macstats) - version 4 */
typedef struct wl_macst_tx_mcst_v4 {
	uint8	num_links;	/* Number of per-link stats supported on slice */
	uint8	pad[3];
	/* Per ML Link TX macstats (esp. eMLSR) */
	wl_cnt_mcst_tx_wrap_v2_t cnt_wrap[];
} wl_macst_tx_mcst_v4_t;

/* ******* update v4 END ********** */

/** MACSTAT counters for ucode (corerev >= 80) */
typedef struct {
	/* MAC counters: 32-bit version of d11.h's macstat_t */
	/* Start of PSM2HOST stats(72) block */
	uint32	txallfrm;	/**< total number of frames sent, incl. Data, ACK, RTS, CTS,
				 * Control Management (includes retransmissions)
				 */
	uint32	txrtsfrm;	/**< number of RTS sent out by the MAC */
	uint32	txctsfrm;	/**< number of CTS sent out by the MAC */
	uint32	txackfrm;	/**< number of ACK frames sent out */
	uint32	txdnlfrm;	/**< number of Null-Data transmission generated from template  */
	uint32	txbcnfrm;	/**< beacons transmitted */
	uint32	txampdu;	/**< number of AMPDUs transmitted */
	uint32	txmpdu;		/**< number of MPDUs transmitted */
	uint32	txtplunfl;	/**< Template underflows (mac was too slow to transmit ACK/CTS
				 * or BCN)
				 */
	uint32	txphyerror;	/**< Transmit phy error, type of error is reported in tx-status for
				 * driver enqueued frames
				 */
	uint32  pktengrxducast; /**< unicast frames rxed by the pkteng code */
	uint32  pktengrxdmcast; /**< multicast frames rxed by the pkteng code */
	uint32	rxfrmtoolong;	/**< Received frame longer than legal limit (2346 bytes) */
	uint32	rxfrmtooshrt; /**< Received frame did not contain enough bytes for its frame type */
	uint32	rxanyerr;	/**< Any RX error that is not counted by other counters. */
	uint32	rxbadfcs;	/**< number of frames for which the CRC check failed in the MAC */
	uint32	rxbadplcp;	/**< parity check of the PLCP header failed */
	uint32	rxcrsglitch;	/**< PHY was able to correlate the preamble but not the header */
	uint32	rxstrt;		/**< Number of received frames with a good PLCP
				 * (i.e. passing parity check)
				 */
	uint32	rxdtucastmbss; /**< number of received DATA frames with good FCS and matching RA */
	uint32	rxmgucastmbss; /**< number of received mgmt frames with good FCS and matching RA */
	uint32	rxctlucast; /**< number of received CNTRL frames with good FCS and matching RA */
	uint32	rxrtsucast;	/**< number of unicast RTS addressed to the MAC (good FCS) */
	uint32	rxctsucast;	/**< number of unicast CTS addressed to the MAC (good FCS) */
	uint32	rxackucast;	/**< number of ucast ACKS received (good FCS) */
	uint32	rxdtocast; /**< number of received DATA frames (good FCS and not matching RA) */
	uint32	rxmgocast; /**< number of received MGMT frames (good FCS and not matching RA) */
	uint32	rxctlocast; /**< number of received CNTRL frame (good FCS and not matching RA) */
	uint32	rxrtsocast;	/**< number of received RTS not addressed to the MAC */
	uint32	rxctsocast;	/**< number of received CTS not addressed to the MAC */
	uint32	rxdtmcast;	/**< number of RX Data multicast frames received by the MAC */
	uint32	rxmgmcast;	/**< number of RX Management multicast frames received by the MAC */
	uint32	rxctlmcast;	/**< number of RX Control multicast frames received by the MAC
				 * (unlikely to see these)
				 */
	uint32	rxbeaconmbss;	/**< beacons received from member of BSS */
	uint32	rxdtucastobss; /**< number of unicast frames addressed to the MAC from
				  * other BSS (WDS FRAME)
				  */
	uint32	rxbeaconobss;	/**< beacons received from other BSS */
	uint32	rxrsptmout;	/**< number of response timeouts for transmitted frames
				 * expecting a response
				 */
	uint32	bcntxcancl;	/**< transmit beacons canceled due to receipt of beacon (IBSS) */
	uint32	rxnodelim;	/**< number of no valid delimiter detected by ampdu parser */
	uint32	missbcn_dbg;	/**< number of beacon missed to receive */
	uint32	pmqovfl;	/**< number of PMQ overflows */
	uint32	rxcgprqfrm;	/**< number of received Probe requests that made it into
				 * the PRQ fifo
				 */
	uint32	rxcgprsqovfl;	/**< Rx Probe Request Que overflow in the AP */
	uint32	txcgprsfail;	/**< Tx Probe Response Fail. AP sent probe response but did
				 * not get ACK
				 */
	uint32	txcgprssuc;	/**< Tx Probe Response Success (ACK was received) */
	uint32	prs_timeout;	/**< number of probe requests that were dropped from the PRQ
				 * fifo because a probe response could not be sent out within
				 * the time limit defined in M_PRS_MAXTIME
				 */
	uint32	txrtsfail;	/**< number of rts transmission failure that reach retry limit */
	uint32	txucast;	/**< number of unicast tx expecting response other than cts/cwcts */
	uint32  txinrtstxop;	/**< number of data frame transmissions during rts txop */
	uint32	rxback;		/**< blockack rxcnt */
	uint32	txback;		/**< blockack txcnt */
	uint32	bphy_rxcrsglitch;	/**< PHY count of bphy glitches */
	uint32	rxdrop20s;	/**< drop secondary cnt */
	uint32	rxtoolate;	/**< receive too late */
	uint32  bphy_badplcp;	/**< number of bad PLCP reception on BPHY rate */
	uint32	rxtrig_myaid;	/* New counters added in corerev 80 */
	uint32	rxtrig_rand;
	uint32	goodfcs;
	uint32	colormiss;
	uint32	txmampdu;
	uint32	rxmtidback;
	uint32	rxmstaback;
	uint32	txfrag;
	/* start of rxerror overflow counter(24) block which are modified/added in corerev 80 */
	uint32	phyovfl;
	uint32	rxf0ovfl;	/**< number of receive fifo 0 overflows */
	uint32	rxf1ovfl;	/**< number of receive fifo 1 overflows */
	uint32	lenfovfl;
	uint32	weppeof;
	uint32	badplcp;
	uint32	msduthresh;
	uint32	strmeof;
	uint32	stsfifofull;
	uint32	stsfifoerr;
	uint32	rxerr_stat;
	uint32	ctx_fifo_full;	/* Firmware not draining frames fast enough */
	uint32	PAD[20];
	uint32	ctmode_ufc_cnt;
	uint32	PAD[12];	/* PAD added for counter elements to be added soon */
	uint32	ctx_fifo2_full;	/* Firmware not draining frames fast enough */
	uint32	PAD[10];	/* PAD to match to the struct size before ctx_fifo2_full count was
				 * introduced. Can be repurposed to a different counter
				 */
} wl_cnt_ge80mcst_v1_t;

/* RX error related counters in addition to RX counters in MAC stats above.
 * Counters collected from noncontiguous SHM locations.
 */
typedef struct {
	uint32 rx20s_cnt;		/* Increments if RXFrame does not include primary 20 */
	uint32 m_pfifo_drop;		/* ucode is late processing RX frame */
	uint32 new_rxin_plcp_wait_cnt;	/* invalid reception/ ucode late in processing rx/ something
					 * wrong over MACPHY interface
					 */
	uint32 laterx_cnt;		/* ucode sees frame 30us late */
	uint32 rxlegacyfrminvalid;	/* Invalid BPHY or L-OFDM reception */
	uint32 txsifserr;		/* Frame arrived in SIF when about to TX (B)ACK */
	uint32 ooseq_macsusp;		/* ucode out of seq in processing reception due to mac
					 * suspend
					 */
} wl_cnt_ge80_rxerr_mcst_v1_t;

typedef struct {
	uint32 fifocount;
	uint32 txfunfl[];
} wl_cnt_ge80_txfunfl_v1_t;

#define WL_SCANAUX_CNT_VER_V1	1u
/* additional mac stats captured in scanaux chips */
typedef struct {
	uint16 version;
	uint16 len;
	uint32 rxsigB_unsupported_cnt;	/* SIGB in unsupported frame types given by HW */
	uint32 rxsigB_notpresent_cnt;	/* SIGB not present in HE MU frames */
	uint32 rxbadpyld_present_cnt;	/* Payload present along with SIGB */
	uint32 rxsigB_norxstart_cnt;	/* vp: tbd */
	uint32 rxsigB_norxframe_cnt;	/* vp: tbd */
	uint32 norxstart_cnt;	/* Frames with no rxstart, 11n, 11ac, 11ax, 11be */
	uint32 rxgoodplcplen_cnt;	/* Good PLCP Length in No Rxstart frames */
	uint32 rxgoodsigA_cnt;	/* SIGA CRC Pass in No rxstart frames */
} wl_cnt_scanaux_mcst_v1_t;

/** MACSTAT counters for "wl counter" version <= 10 */
/*  With ucode before its macstat cnts cleaned up */
typedef struct {
	/* MAC counters: 32-bit version of d11.h's macstat_t */
	uint32	txallfrm;	/**< total number of frames sent, incl. Data, ACK, RTS, CTS,
				 * Control Management (includes retransmissions)
				 */
	uint32	txrtsfrm;	/**< number of RTS sent out by the MAC */
	uint32	txctsfrm;	/**< number of CTS sent out by the MAC */
	uint32	txackfrm;	/**< number of ACK frames sent out */
	uint32	txdnlfrm;	/**< number of Null-Data transmission generated from template  */
	uint32	txbcnfrm;	/**< beacons transmitted */
	uint32	txfunfl[6];	/**< per-fifo tx underflows */
	uint32	txfbw;		/**< transmit at fallback bw (dynamic bw) */
	uint32	PAD;		/**< number of MPDUs transmitted */
	uint32	txtplunfl;	/**< Template underflows (mac was too slow to transmit ACK/CTS
				 * or BCN)
				 */
	uint32	txphyerror;	/**< Transmit phy error, type of error is reported in tx-status for
				 * driver enqueued frames
				 */
	uint32  pktengrxducast; /**< unicast frames rxed by the pkteng code */
	uint32  pktengrxdmcast; /**< multicast frames rxed by the pkteng code */
	uint32	rxfrmtoolong;	/**< Received frame longer than legal limit (2346 bytes) */
	uint32	rxfrmtooshrt; /**< Received frame did not contain enough bytes for its frame type */
	uint32	rxinvmachdr;	/**< Either the protocol version != 0 or frame type not
				 * data/control/management
				 */
	uint32	rxbadfcs;	/**< number of frames for which the CRC check failed in the MAC */
	uint32	rxbadplcp;	/**< parity check of the PLCP header failed */
	uint32	rxcrsglitch;	/**< PHY was able to correlate the preamble but not the header */
	uint32	rxstrt;		/**< Number of received frames with a good PLCP
				 * (i.e. passing parity check)
				 */
	uint32	rxdfrmucastmbss; /* number of received DATA frames with good FCS and matching RA */
	uint32	rxmfrmucastmbss; /* number of received mgmt frames with good FCS and matching RA */
	uint32	rxcfrmucast; /**< number of received CNTRL frames with good FCS and matching RA */
	uint32	rxrtsucast;  /**< number of unicast RTS addressed to the MAC (good FCS) */
	uint32	rxctsucast;  /**< number of unicast CTS addressed to the MAC (good FCS) */
	uint32	rxackucast;	/**< number of ucast ACKS received (good FCS) */
	uint32	rxdfrmocast; /**< number of received DATA frames (good FCS and not matching RA) */
	uint32	rxmfrmocast; /**< number of received MGMT frames (good FCS and not matching RA) */
	uint32	rxcfrmocast; /**< number of received CNTRL frame (good FCS and not matching RA) */
	uint32	rxrtsocast;	/**< number of received RTS not addressed to the MAC */
	uint32	rxctsocast;	/**< number of received CTS not addressed to the MAC */
	uint32	rxdfrmmcast;	/**< number of RX Data multicast frames received by the MAC */
	uint32	rxmfrmmcast;	/**< number of RX Management multicast frames received by the MAC */
	uint32	rxcfrmmcast;	/**< number of RX Control multicast frames received by the MAC
				 * (unlikely to see these)
				 */
	uint32	rxbeaconmbss;	/**< beacons received from member of BSS */
	uint32	rxdfrmucastobss; /**< number of unicast frames addressed to the MAC from
				  * other BSS (WDS FRAME)
				  */
	uint32	rxbeaconobss;	/**< beacons received from other BSS */
	uint32	rxrsptmout;	/**< number of response timeouts for transmitted frames
				 * expecting a response
				 */
	uint32	bcntxcancl;	/**< transmit beacons canceled due to receipt of beacon (IBSS) */
	uint32	PAD;
	uint32	rxf0ovfl;	/**< number of receive fifo 0 overflows */
	uint32	rxf1ovfl;	/**< Number of receive fifo 1 overflows (obsolete) */
	uint32	rxf2ovfl;	/**< Number of receive fifo 2 overflows (obsolete) */
	uint32	txsfovfl;	/**< Number of transmit status fifo overflows (obsolete) */
	uint32	pmqovfl;	/**< number of PMQ overflows */
	uint32	rxcgprqfrm;	/**< number of received Probe requests that made it into
				 * the PRQ fifo
				 */
	uint32	rxcgprsqovfl;	/**< Rx Probe Request Que overflow in the AP */
	uint32	txcgprsfail;	/**< Tx Probe Response Fail. AP sent probe response but did
				 * not get ACK
				 */
	uint32	txcgprssuc;	/**< Tx Probe Response Success (ACK was received) */
	uint32	prs_timeout;	/**< number of probe requests that were dropped from the PRQ
				 * fifo because a probe response could not be sent out within
				 * the time limit defined in M_PRS_MAXTIME
				 */
	uint32	rxnack;		/**< obsolete */
	uint32	frmscons;	/**< obsolete */
	uint32  txnack;		/**< obsolete */
	uint32	rxback;		/**< blockack rxcnt */
	uint32	txback;		/**< blockack txcnt */
	uint32	bphy_rxcrsglitch;	/**< PHY count of bphy glitches */
	uint32	rxdrop20s;	/**< drop secondary cnt */
	uint32	rxtoolate;	/**< receive too late */
	uint32  bphy_badplcp;	/**< number of bad PLCP reception on BPHY rate */
	/* All counter variables have to be of uint32. */
} wl_cnt_v_le10_mcst_t;

#define MAX_RX_FIFO 3
#define WL_RXFIFO_CNT_VERSION_1  1   /* current version of wl_rxfifo_cnt_t */
typedef struct {
	/* Counters for frames received from rx fifos */
	uint16	version;
	uint16	length;		/* length of entire structure */
	uint32	rxf_data[MAX_RX_FIFO];		/* data frames from rx fifo */
	uint32	rxf_mgmtctl[MAX_RX_FIFO];	/* mgmt/ctl frames from rx fifo */
} wl_rxfifo_cnt_t;

typedef struct {
	uint16	version;	/**< see definition of WL_CNT_T_VERSION */
	uint16	length;		/**< length of entire structure */

	/* transmit stat counters */
	uint32	txframe;	/**< tx data frames */
	uint32	txbyte;		/**< tx data bytes */
	uint32	txretrans;	/**< tx mac retransmits */
	uint32	txerror;	/**< tx data errors (derived: sum of others) */
	uint32	txctl;		/**< tx management frames */
	uint32	txprshort;	/**< tx short preamble frames */
	uint32	txserr;		/**< tx status errors */
	uint32	txnobuf;	/**< tx out of buffers errors */
	uint32	txnoassoc;	/**< tx discard because we're not associated */
	uint32	txrunt;		/**< tx runt frames */
	uint32	txchit;		/**< tx header cache hit (fastpath) */
	uint32	txcmiss;	/**< tx header cache miss (slowpath) */

	/* transmit chip error counters */
	uint32	txuflo;		/**< tx fifo underflows */
	uint32	txphyerr;	/**< tx phy errors (indicated in tx status) */
	uint32	txphycrs;	/**< PR8861/8963 counter */

	/* receive stat counters */
	uint32	rxframe;	/**< rx data frames */
	uint32	rxbyte;		/**< rx data bytes */
	uint32	rxerror;	/**< rx data errors (derived: sum of others) */
	uint32	rxctl;		/**< rx management frames */
	uint32	rxnobuf;	/**< rx out of buffers errors */
	uint32	rxnondata;	/**< rx non data frames in the data channel errors */
	uint32	rxbadds;	/**< rx bad DS errors */
	uint32	rxbadcm;	/**< rx bad control or management frames */
	uint32	rxfragerr;	/**< rx fragmentation errors */
	uint32	rxrunt;		/**< rx runt frames */
	uint32	rxgiant;	/**< rx giant frames */
	uint32	rxnoscb;	/**< rx no scb error */
	uint32	rxbadproto;	/**< rx invalid frames */
	uint32	rxbadsrcmac;	/**< rx frames with Invalid Src Mac */
	uint32	rxbadda;	/**< rx frames tossed for invalid da */
	uint32	rxfilter;	/**< rx frames filtered out */

	/* receive chip error counters */
	uint32	rxoflo;		/**< rx fifo overflow errors */
	uint32	rxuflo[NFIFO];	/**< rx dma descriptor underflow errors */

	uint32	d11cnt_txrts_off;	/**< d11cnt txrts value when reset d11cnt */
	uint32	d11cnt_rxcrc_off;	/**< d11cnt rxcrc value when reset d11cnt */
	uint32	d11cnt_txnocts_off;	/**< d11cnt txnocts value when reset d11cnt */

	/* misc counters */
	uint32	dmade;		/**< tx/rx dma descriptor errors */
	uint32	dmada;		/**< tx/rx dma data errors */
	uint32	dmape;		/**< tx/rx dma descriptor protocol errors */
	uint32	reset;		/**< reset count */
	uint32	tbtt;		/**< cnts the TBTT int's */
	uint32	txdmawar;	/**< # occurrences of PR15420 workaround */
	uint32	pkt_callback_reg_fail;	/**< callbacks register failure */

	/* MAC counters: 32-bit version of d11.h's macstat_t */
	uint32	txallfrm;	/**< total number of frames sent, incl. Data, ACK, RTS, CTS,
				 * Control Management (includes retransmissions)
				 */
	uint32	txrtsfrm;	/**< number of RTS sent out by the MAC */
	uint32	txctsfrm;	/**< number of CTS sent out by the MAC */
	uint32	txackfrm;	/**< number of ACK frames sent out */
	uint32	txdnlfrm;	/**< Not used */
	uint32	txbcnfrm;	/**< beacons transmitted */
	uint32	txfunfl[6];	/**< per-fifo tx underflows */
	uint32	rxtoolate;	/**< receive too late */
	uint32  txfbw;		/**< transmit at fallback bw (dynamic bw) */
	uint32	txtplunfl;	/**< Template underflows (mac was too slow to transmit ACK/CTS
				 * or BCN)
				 */
	uint32	txphyerror;	/**< Transmit phy error, type of error is reported in tx-status for
				 * driver enqueued frames
				 */
	uint32	rxfrmtoolong;	/**< Received frame longer than legal limit (2346 bytes) */
	uint32	rxfrmtooshrt; /**< Received frame did not contain enough bytes for its frame type */
	uint32	rxinvmachdr;	/**< Either the protocol version != 0 or frame type not
				 * data/control/management
				 */
	uint32	rxbadfcs;	/**< number of frames for which the CRC check failed in the MAC */
	uint32	rxbadplcp;	/**< parity check of the PLCP header failed */
	uint32	rxcrsglitch;	/**< PHY was able to correlate the preamble but not the header */
	uint32	rxstrt;		/**< Number of received frames with a good PLCP
				 * (i.e. passing parity check)
				 */
	uint32	rxdfrmucastmbss; /* Number of received DATA frames with good FCS and matching RA */
	uint32	rxmfrmucastmbss; /* number of received mgmt frames with good FCS and matching RA */
	uint32	rxcfrmucast; /**< number of received CNTRL frames with good FCS and matching RA */
	uint32	rxrtsucast;	/**< number of unicast RTS addressed to the MAC (good FCS) */
	uint32	rxctsucast;	/**< number of unicast CTS addressed to the MAC (good FCS) */
	uint32	rxackucast;	/**< number of ucast ACKS received (good FCS) */
	uint32	rxdfrmocast; /**< number of received DATA frames (good FCS and not matching RA) */
	uint32	rxmfrmocast; /**< number of received MGMT frames (good FCS and not matching RA) */
	uint32	rxcfrmocast; /**< number of received CNTRL frame (good FCS and not matching RA) */
	uint32	rxrtsocast;	/**< number of received RTS not addressed to the MAC */
	uint32	rxctsocast;	/**< number of received CTS not addressed to the MAC */
	uint32	rxdfrmmcast;	/**< number of RX Data multicast frames received by the MAC */
	uint32	rxmfrmmcast;	/**< number of RX Management multicast frames received by the MAC */
	uint32	rxcfrmmcast;	/**< number of RX Control multicast frames received by the MAC
				 * (unlikely to see these)
				 */
	uint32	rxbeaconmbss;	/**< beacons received from member of BSS */
	uint32	rxdfrmucastobss; /**< number of unicast frames addressed to the MAC from
				  * other BSS (WDS FRAME)
				  */
	uint32	rxbeaconobss;	/**< beacons received from other BSS */
	uint32	rxrsptmout;	/**< Number of response timeouts for transmitted frames
				 * expecting a response
				 */
	uint32	bcntxcancl;	/**< transmit beacons canceled due to receipt of beacon (IBSS) */
	uint32	rxf0ovfl;	/**< Number of receive fifo 0 overflows */
	uint32	rxf1ovfl;	/**< Number of receive fifo 1 overflows (obsolete) */
	uint32	rxf2ovfl;	/**< Number of receive fifo 2 overflows (obsolete) */
	uint32	txsfovfl;	/**< Number of transmit status fifo overflows (obsolete) */
	uint32	pmqovfl;	/**< Number of PMQ overflows */
	uint32	rxcgprqfrm;	/**< Number of received Probe requests that made it into
				 * the PRQ fifo
				 */
	uint32	rxcgprsqovfl;	/**< Rx Probe Request Que overflow in the AP */
	uint32	txcgprsfail;	/**< Tx Probe Response Fail. AP sent probe response but did
				 * not get ACK
				 */
	uint32	txcgprssuc;	/**< Tx Probe Response Success (ACK was received) */
	uint32	prs_timeout;	/**< Number of probe requests that were dropped from the PRQ
				 * fifo because a probe response could not be sent out within
				 * the time limit defined in M_PRS_MAXTIME
				 */
	uint32	rxnack;		/**< obsolete */
	uint32	frmscons;	/**< obsolete */
	uint32  txnack;		/**< obsolete */
	uint32	rxback;		/**< blockack rxcnt */
	uint32	txback;		/**< blockack txcnt */

	/* 802.11 MIB counters, pp. 614 of 802.11 reaff doc. */
	uint32	txfrag;		/**< dot11TransmittedFragmentCount */
	uint32	txmulti;	/**< dot11MulticastTransmittedFrameCount */
	uint32	txfail;		/**< dot11FailedCount */
	uint32	txretry;	/**< dot11RetryCount */
	uint32	txretrie;	/**< dot11MultipleRetryCount */
	uint32	rxdup;		/**< dot11FrameduplicateCount */
	uint32	txrts;		/**< dot11RTSSuccessCount */
	uint32	txnocts;	/**< dot11RTSFailureCount */
	uint32	txnoack;	/**< dot11ACKFailureCount */
	uint32	rxfrag;		/**< dot11ReceivedFragmentCount */
	uint32	rxmulti;	/**< dot11MulticastReceivedFrameCount */
	uint32	rxcrc;		/**< dot11FCSErrorCount */
	uint32	txfrmsnt;	/**< dot11TransmittedFrameCount (bogus MIB?) */
	uint32	rxundec;	/**< dot11WEPUndecryptableCount */

	/* WPA2 counters (see rxundec for DecryptFailureCount) */
	uint32	tkipmicfaill;	/**< TKIPLocalMICFailures */
	uint32	tkipcntrmsr;	/**< TKIPCounterMeasuresInvoked */
	uint32	tkipreplay;	/**< TKIPReplays */
	uint32	ccmpfmterr;	/**< CCMPFormatErrors */
	uint32	ccmpreplay;	/**< CCMPReplays */
	uint32	ccmpundec;	/**< CCMPDecryptErrors */
	uint32	fourwayfail;	/**< FourWayHandshakeFailures */
	uint32	wepundec;	/**< dot11WEPUndecryptableCount */
	uint32	wepicverr;	/**< dot11WEPICVErrorCount */
	uint32	decsuccess;	/**< DecryptSuccessCount */
	uint32	tkipicverr;	/**< TKIPICVErrorCount */
	uint32	wepexcluded;	/**< dot11WEPExcludedCount */

	uint32	txchanrej;	/**< Tx frames suppressed due to channel rejection */
	uint32	psmwds;		/**< Count PSM watchdogs */
	uint32	phywatchdog;	/**< Count Phy watchdogs (triggered by ucode) */

	/* MBSS counters, AP only */
	uint32	prq_entries_handled;	/**< PRQ entries read in */
	uint32	prq_undirected_entries;	/**<    which were bcast bss & ssid */
	uint32	prq_bad_entries;	/**<    which could not be translated to info */
	uint32	atim_suppress_count;	/**< TX suppressions on ATIM fifo */
	uint32	bcn_template_not_ready;	/**< Template marked in use on send bcn ... */
	uint32	bcn_template_not_ready_done; /**< ...but "DMA done" interrupt rcvd */
	uint32	late_tbtt_dpc;	/**< TBTT DPC did not happen in time */

	/* per-rate receive stat counters */
	uint32  rx1mbps;	/**< packets rx at 1Mbps */
	uint32  rx2mbps;	/**< packets rx at 2Mbps */
	uint32  rx5mbps5;	/**< packets rx at 5.5Mbps */
	uint32  rx6mbps;	/**< packets rx at 6Mbps */
	uint32  rx9mbps;	/**< packets rx at 9Mbps */
	uint32  rx11mbps;	/**< packets rx at 11Mbps */
	uint32  rx12mbps;	/**< packets rx at 12Mbps */
	uint32  rx18mbps;	/**< packets rx at 18Mbps */
	uint32  rx24mbps;	/**< packets rx at 24Mbps */
	uint32  rx36mbps;	/**< packets rx at 36Mbps */
	uint32  rx48mbps;	/**< packets rx at 48Mbps */
	uint32  rx54mbps;	/**< packets rx at 54Mbps */
	uint32  rx108mbps;	/**< packets rx at 108mbps */
	uint32  rx162mbps;	/**< packets rx at 162mbps */
	uint32  rx216mbps;	/**< packets rx at 216 mbps */
	uint32  rx270mbps;	/**< packets rx at 270 mbps */
	uint32  rx324mbps;	/**< packets rx at 324 mbps */
	uint32  rx378mbps;	/**< packets rx at 378 mbps */
	uint32  rx432mbps;	/**< packets rx at 432 mbps */
	uint32  rx486mbps;	/**< packets rx at 486 mbps */
	uint32  rx540mbps;	/**< packets rx at 540 mbps */

	/* pkteng rx frame stats */
	uint32	pktengrxducast; /**< unicast frames rxed by the pkteng code */
	uint32	pktengrxdmcast; /**< multicast frames rxed by the pkteng code */

	uint32	rfdisable;	/**< count of radio disables */
	uint32	bphy_rxcrsglitch;	/**< PHY count of bphy glitches */
	uint32  bphy_badplcp;

	uint32	txexptime;	/**< Tx frames suppressed due to timer expiration */

	uint32	txmpdu_sgi;	/**< count for sgi transmit */
	uint32	rxmpdu_sgi;	/**< count for sgi received */
	uint32	txmpdu_stbc;	/**< count for stbc transmit */
	uint32	rxmpdu_stbc;	/**< count for stbc received */

	uint32	rxundec_mcst;	/**< dot11WEPUndecryptableCount */

	/* WPA2 counters (see rxundec for DecryptFailureCount) */
	uint32	tkipmicfaill_mcst;	/**< TKIPLocalMICFailures */
	uint32	tkipcntrmsr_mcst;	/**< TKIPCounterMeasuresInvoked */
	uint32	tkipreplay_mcst;	/**< TKIPReplays */
	uint32	ccmpfmterr_mcst;	/**< CCMPFormatErrors */
	uint32	ccmpreplay_mcst;	/**< CCMPReplays */
	uint32	ccmpundec_mcst;	/**< CCMPDecryptErrors */
	uint32	fourwayfail_mcst;	/**< FourWayHandshakeFailures */
	uint32	wepundec_mcst;	/**< dot11WEPUndecryptableCount */
	uint32	wepicverr_mcst;	/**< dot11WEPICVErrorCount */
	uint32	decsuccess_mcst;	/**< DecryptSuccessCount */
	uint32	tkipicverr_mcst;	/**< TKIPICVErrorCount */
	uint32	wepexcluded_mcst;	/**< dot11WEPExcludedCount */

	uint32	dma_hang;	/**< count for dma hang */
	uint32	reinit;		/**< count for reinit */

	uint32  pstatxucast;	/**< count of ucast frames xmitted on all psta assoc */
	uint32  pstatxnoassoc;	/**< count of txnoassoc frames xmitted on all psta assoc */
	uint32  pstarxucast;	/**< count of ucast frames received on all psta assoc */
	uint32  pstarxbcmc;	/**< count of bcmc frames received on all psta */
	uint32  pstatxbcmc;	/**< count of bcmc frames transmitted on all psta */

	uint32  cso_passthrough; /**< hw cso required but passthrough */
	uint32	cso_normal;	/**< hw cso hdr for normal process */
	uint32	chained;	/**< number of frames chained */
	uint32	chainedsz1;	/**< number of chain size 1 frames */
	uint32	unchained;	/**< number of frames not chained */
	uint32	maxchainsz;	/**< max chain size so far */
	uint32	currchainsz;	/**< current chain size */
	uint32	rxdrop20s;	/**< drop secondary cnt */
	uint32	pciereset;	/**< Secondary Bus Reset issued by driver */
	uint32	cfgrestore;	/**< configspace restore by driver */
	uint32	reinitreason[NREINITREASONCOUNT]; /**< reinitreason counters; 0: Unknown reason */
	uint32  rxrtry;		/**< num of received packets with retry bit on */
	uint32	txmpdu;		/**< macstat cnt only valid in ver 11. number of MPDUs txed.  */
	uint32	rxnodelim;	/**< macstat cnt only valid in ver 11.
				 * number of occasions that no valid delimiter is detected
				 * by ampdu parser.
				 */
	uint32  rxmpdu_mu;      /**< Number of MU MPDUs received */

	/* detailed control/management frames */
	uint32	txbar;		/**< Number of TX BAR */
	uint32	rxbar;		/**< Number of RX BAR */
	uint32	txpspoll;	/**< Number of TX PS-poll */
	uint32	rxpspoll;	/**< Number of RX PS-poll */
	uint32	txnull;		/**< Number of TX NULL_DATA */
	uint32	rxnull;		/**< Number of RX NULL_DATA */
	uint32	txqosnull;	/**< Number of TX NULL_QoSDATA */
	uint32	rxqosnull;	/**< Number of RX NULL_QoSDATA */
	uint32	txassocreq;	/**< Number of TX ASSOC request */
	uint32	rxassocreq;	/**< Number of RX ASSOC request */
	uint32	txreassocreq;	/**< Number of TX REASSOC request */
	uint32	rxreassocreq;	/**< Number of RX REASSOC request */
	uint32	txdisassoc;	/**< Number of TX DISASSOC */
	uint32	rxdisassoc;	/**< Number of RX DISASSOC */
	uint32	txassocrsp;	/**< Number of TX ASSOC response */
	uint32	rxassocrsp;	/**< Number of RX ASSOC response */
	uint32	txreassocrsp;	/**< Number of TX REASSOC response */
	uint32	rxreassocrsp;	/**< Number of RX REASSOC response */
	uint32	txauth;		/**< Number of TX AUTH */
	uint32	rxauth;		/**< Number of RX AUTH */
	uint32	txdeauth;	/**< Number of TX DEAUTH */
	uint32	rxdeauth;	/**< Number of RX DEAUTH */
	uint32	txprobereq;	/**< Number of TX probe request */
	uint32	rxprobereq;	/**< Number of RX probe request */
	uint32	txprobersp;	/**< Number of TX probe response */
	uint32	rxprobersp;	/**< Number of RX probe response */
	uint32	txaction;	/**< Number of TX action frame */
	uint32	rxaction;	/**< Number of RX action frame */
	uint32  ampdu_wds;      /**< Number of AMPDU watchdogs */
	uint32  txlost;         /**< Number of lost packets reported in txs */
	uint32  txdatamcast;	/**< Number of TX multicast data packets */
	uint32  txdatabcast;	/**< Number of TX broadcast data packets */
	uint32  txbcast;        /* Broadcast TransmittedFrameCount */
	uint32  txdropped;      /* tx dropped pkts */
	uint32  rxbcast;        /* BroadcastReceivedFrameCount */
	uint32  rxdropped;      /* rx dropped pkts (derived: sum of others) */

	/* This structure is deprecated and used only for ver <= 11.
	 * All counter variables have to be of uint32.
	 */
} wl_cnt_ver_11_t;

typedef struct {
	uint16	version;	/* see definition of WL_CNT_T_VERSION */
	uint16	length;		/* length of entire structure */

	/* transmit stat counters */
	uint32	txframe;	/* tx data frames */
	uint32	txbyte;		/* tx data bytes */
	uint32	txretrans;	/* tx mac retransmits */
	uint32	txerror;	/* tx data errors (derived: sum of others) */
	uint32	txctl;		/* tx management frames */
	uint32	txprshort;	/* tx short preamble frames */
	uint32	txserr;		/* tx status errors */
	uint32	txnobuf;	/* tx out of buffers errors */
	uint32	txnoassoc;	/* tx discard because we're not associated */
	uint32	txrunt;		/* tx runt frames */
	uint32	txchit;		/* tx header cache hit (fastpath) */
	uint32	txcmiss;	/* tx header cache miss (slowpath) */

	/* transmit chip error counters */
	uint32	txuflo;		/* tx fifo underflows */
	uint32	txphyerr;	/* tx phy errors (indicated in tx status) */
	uint32	txphycrs;	/* PR8861/8963 counter */

	/* receive stat counters */
	uint32	rxframe;	/* rx data frames */
	uint32	rxbyte;		/* rx data bytes */
	uint32	rxerror;	/* rx data errors (derived: sum of others) */
	uint32	rxctl;		/* rx management frames */
	uint32	rxnobuf;	/* rx out of buffers errors */
	uint32	rxnondata;	/* rx non data frames in the data channel errors */
	uint32	rxbadds;	/* rx bad DS errors */
	uint32	rxbadcm;	/* rx bad control or management frames */
	uint32	rxfragerr;	/* rx fragmentation errors */
	uint32	rxrunt;		/* rx runt frames */
	uint32	rxgiant;	/* rx giant frames */
	uint32	rxnoscb;	/* rx no scb error */
	uint32	rxbadproto;	/* rx invalid frames */
	uint32	rxbadsrcmac;	/* rx frames with Invalid Src Mac */
	uint32	rxbadda;	/* rx frames tossed for invalid da */
	uint32	rxfilter;	/* rx frames filtered out */

	/* receive chip error counters */
	uint32	rxoflo;		/* rx fifo overflow errors */
	uint32	rxuflo[NFIFO];	/* rx dma descriptor underflow errors */

	uint32	d11cnt_txrts_off;	/* d11cnt txrts value when reset d11cnt */
	uint32	d11cnt_rxcrc_off;	/* d11cnt rxcrc value when reset d11cnt */
	uint32	d11cnt_txnocts_off;	/* d11cnt txnocts value when reset d11cnt */

	/* misc counters */
	uint32	dmade;		/* tx/rx dma descriptor errors */
	uint32	dmada;		/* tx/rx dma data errors */
	uint32	dmape;		/* tx/rx dma descriptor protocol errors */
	uint32	reset;		/* reset count */
	uint32	tbtt;		/* cnts the TBTT int's */
	uint32	txdmawar;	/* # occurrences of PR15420 workaround */
	uint32	pkt_callback_reg_fail;	/* callbacks register failure */

	/* MAC counters: 32-bit version of d11.h's macstat_t */
	uint32	txallfrm;	/* total number of frames sent, incl. Data, ACK, RTS, CTS,
				 * Control Management (includes retransmissions)
				 */
	uint32	txrtsfrm;	/* number of RTS sent out by the MAC */
	uint32	txctsfrm;	/* number of CTS sent out by the MAC */
	uint32	txackfrm;	/* number of ACK frames sent out */
	uint32	txdnlfrm;	/* Not used */
	uint32	txbcnfrm;	/* beacons transmitted */
	uint32	txfunfl[8];	/* per-fifo tx underflows */
	uint32	txtplunfl;	/* Template underflows (mac was too slow to transmit ACK/CTS
				 * or BCN)
				 */
	uint32	txphyerror;	/* Transmit phy error, type of error is reported in tx-status for
				 * driver enqueued frames
				 */
	uint32	rxfrmtoolong;	/* Received frame longer than legal limit (2346 bytes) */
	uint32	rxfrmtooshrt;	/* Received frame did not contain enough bytes for its frame type */
	uint32	rxinvmachdr;	/* Either the protocol version != 0 or frame type not
				 * data/control/management
				 */
	uint32	rxbadfcs;	/* number of frames for which the CRC check failed in the MAC */
	uint32	rxbadplcp;	/* parity check of the PLCP header failed */
	uint32	rxcrsglitch;	/* PHY was able to correlate the preamble but not the header */
	uint32	rxstrt;		/* Number of received frames with a good PLCP
				 * (i.e. passing parity check)
				 */
	uint32	rxdfrmucastmbss; /* Number of received DATA frames with good FCS and matching RA */
	uint32	rxmfrmucastmbss; /* number of received mgmt frames with good FCS and matching RA */
	uint32	rxcfrmucast;	/* number of received CNTRL frames with good FCS and matching RA */
	uint32	rxrtsucast;	/* number of unicast RTS addressed to the MAC (good FCS) */
	uint32	rxctsucast;	/* number of unicast CTS addressed to the MAC (good FCS) */
	uint32	rxackucast;	/* number of ucast ACKS received (good FCS) */
	uint32	rxdfrmocast;	/* number of received DATA frames (good FCS and not matching RA) */
	uint32	rxmfrmocast;	/* number of received MGMT frames (good FCS and not matching RA) */
	uint32	rxcfrmocast;	/* number of received CNTRL frame (good FCS and not matching RA) */
	uint32	rxrtsocast;	/* number of received RTS not addressed to the MAC */
	uint32	rxctsocast;	/* number of received CTS not addressed to the MAC */
	uint32	rxdfrmmcast;	/* number of RX Data multicast frames received by the MAC */
	uint32	rxmfrmmcast;	/* number of RX Management multicast frames received by the MAC */
	uint32	rxcfrmmcast;	/* number of RX Control multicast frames received by the MAC
				 * (unlikely to see these)
				 */
	uint32	rxbeaconmbss;	/* beacons received from member of BSS */
	uint32	rxdfrmucastobss; /* number of unicast frames addressed to the MAC from
				  * other BSS (WDS FRAME)
				  */
	uint32	rxbeaconobss;	/* beacons received from other BSS */
	uint32	rxrsptmout;	/* Number of response timeouts for transmitted frames
				 * expecting a response
				 */
	uint32	bcntxcancl;	/* transmit beacons canceled due to receipt of beacon (IBSS) */
	uint32	rxf0ovfl;	/* Number of receive fifo 0 overflows */
	uint32	rxf1ovfl;	/* Number of receive fifo 1 overflows (obsolete) */
	uint32	rxf2ovfl;	/* Number of receive fifo 2 overflows (obsolete) */
	uint32	txsfovfl;	/* Number of transmit status fifo overflows (obsolete) */
	uint32	pmqovfl;	/* Number of PMQ overflows */
	uint32	rxcgprqfrm;	/* Number of received Probe requests that made it into
				 * the PRQ fifo
				 */
	uint32	rxcgprsqovfl;	/* Rx Probe Request Que overflow in the AP */
	uint32	txcgprsfail;	/* Tx Probe Response Fail. AP sent probe response but did
				 * not get ACK
				 */
	uint32	txcgprssuc;	/* Tx Probe Response Success (ACK was received) */
	uint32	prs_timeout;	/* Number of probe requests that were dropped from the PRQ
				 * fifo because a probe response could not be sent out within
				 * the time limit defined in M_PRS_MAXTIME
				 */
	uint32	rxnack;		/* obsolete */
	uint32	frmscons;	/* obsolete */
	uint32	txnack;		/* obsolete */
	uint32	txglitch_nack;	/* obsolete */
	uint32	txburst;	/* obsolete */

	/* 802.11 MIB counters, pp. 614 of 802.11 reaff doc. */
	uint32	txfrag;		/* dot11TransmittedFragmentCount */
	uint32	txmulti;	/* dot11MulticastTransmittedFrameCount */
	uint32	txfail;		/* dot11FailedCount */
	uint32	txretry;	/* dot11RetryCount */
	uint32	txretrie;	/* dot11MultipleRetryCount */
	uint32	rxdup;		/* dot11FrameduplicateCount */
	uint32	txrts;		/* dot11RTSSuccessCount */
	uint32	txnocts;	/* dot11RTSFailureCount */
	uint32	txnoack;	/* dot11ACKFailureCount */
	uint32	rxfrag;		/* dot11ReceivedFragmentCount */
	uint32	rxmulti;	/* dot11MulticastReceivedFrameCount */
	uint32	rxcrc;		/* dot11FCSErrorCount */
	uint32	txfrmsnt;	/* dot11TransmittedFrameCount (bogus MIB?) */
	uint32	rxundec;	/* dot11WEPUndecryptableCount */

	/* WPA2 counters (see rxundec for DecryptFailureCount) */
	uint32	tkipmicfaill;	/* TKIPLocalMICFailures */
	uint32	tkipcntrmsr;	/* TKIPCounterMeasuresInvoked */
	uint32	tkipreplay;	/* TKIPReplays */
	uint32	ccmpfmterr;	/* CCMPFormatErrors */
	uint32	ccmpreplay;	/* CCMPReplays */
	uint32	ccmpundec;	/* CCMPDecryptErrors */
	uint32	fourwayfail;	/* FourWayHandshakeFailures */
	uint32	wepundec;	/* dot11WEPUndecryptableCount */
	uint32	wepicverr;	/* dot11WEPICVErrorCount */
	uint32	decsuccess;	/* DecryptSuccessCount */
	uint32	tkipicverr;	/* TKIPICVErrorCount */
	uint32	wepexcluded;	/* dot11WEPExcludedCount */

	uint32	txchanrej;	/* Tx frames suppressed due to channel rejection */
	uint32	psmwds;		/* Count PSM watchdogs */
	uint32	phywatchdog;	/* Count Phy watchdogs (triggered by ucode) */

	/* MBSS counters, AP only */
	uint32	prq_entries_handled;	/* PRQ entries read in */
	uint32	prq_undirected_entries;	/*    which were bcast bss & ssid */
	uint32	prq_bad_entries;	/*    which could not be translated to info */
	uint32	atim_suppress_count;	/* TX suppressions on ATIM fifo */
	uint32	bcn_template_not_ready;	/* Template marked in use on send bcn ... */
	uint32	bcn_template_not_ready_done; /* ...but "DMA done" interrupt rcvd */
	uint32	late_tbtt_dpc;	/* TBTT DPC did not happen in time */

	/* per-rate receive stat counters */
	uint32  rx1mbps;	/* packets rx at 1Mbps */
	uint32  rx2mbps;	/* packets rx at 2Mbps */
	uint32  rx5mbps5;	/* packets rx at 5.5Mbps */
	uint32  rx6mbps;	/* packets rx at 6Mbps */
	uint32  rx9mbps;	/* packets rx at 9Mbps */
	uint32  rx11mbps;	/* packets rx at 11Mbps */
	uint32  rx12mbps;	/* packets rx at 12Mbps */
	uint32  rx18mbps;	/* packets rx at 18Mbps */
	uint32  rx24mbps;	/* packets rx at 24Mbps */
	uint32  rx36mbps;	/* packets rx at 36Mbps */
	uint32  rx48mbps;	/* packets rx at 48Mbps */
	uint32  rx54mbps;	/* packets rx at 54Mbps */
	uint32  rx108mbps;	/* packets rx at 108mbps */
	uint32  rx162mbps;	/* packets rx at 162mbps */
	uint32  rx216mbps;	/* packets rx at 216 mbps */
	uint32  rx270mbps;	/* packets rx at 270 mbps */
	uint32  rx324mbps;	/* packets rx at 324 mbps */
	uint32  rx378mbps;	/* packets rx at 378 mbps */
	uint32  rx432mbps;	/* packets rx at 432 mbps */
	uint32  rx486mbps;	/* packets rx at 486 mbps */
	uint32  rx540mbps;	/* packets rx at 540 mbps */

	/* pkteng rx frame stats */
	uint32	pktengrxducast; /* unicast frames rxed by the pkteng code */
	uint32	pktengrxdmcast; /* multicast frames rxed by the pkteng code */

	uint32	rfdisable;	/* count of radio disables */
	uint32	bphy_rxcrsglitch;	/* PHY count of bphy glitches */

	uint32	txexptime;	/* Tx frames suppressed due to timer expiration */

	uint32	txmpdu_sgi;	/* count for sgi transmit */
	uint32	rxmpdu_sgi;	/* count for sgi received */
	uint32	txmpdu_stbc;	/* count for stbc transmit */
	uint32	rxmpdu_stbc;	/* count for stbc received */

	uint32	rxundec_mcst;	/* dot11WEPUndecryptableCount */

	/* WPA2 counters (see rxundec for DecryptFailureCount) */
	uint32	tkipmicfaill_mcst;	/* TKIPLocalMICFailures */
	uint32	tkipcntrmsr_mcst;	/* TKIPCounterMeasuresInvoked */
	uint32	tkipreplay_mcst;	/* TKIPReplays */
	uint32	ccmpfmterr_mcst;	/* CCMPFormatErrors */
	uint32	ccmpreplay_mcst;	/* CCMPReplays */
	uint32	ccmpundec_mcst;	/* CCMPDecryptErrors */
	uint32	fourwayfail_mcst;	/* FourWayHandshakeFailures */
	uint32	wepundec_mcst;	/* dot11WEPUndecryptableCount */
	uint32	wepicverr_mcst;	/* dot11WEPICVErrorCount */
	uint32	decsuccess_mcst;	/* DecryptSuccessCount */
	uint32	tkipicverr_mcst;	/* TKIPICVErrorCount */
	uint32	wepexcluded_mcst;	/* dot11WEPExcludedCount */

	uint32	dma_hang;	/* count for stbc received */
	uint32	rxrtry;		/* number of packets with retry bit set to 1 */
} wl_cnt_ver_7_t;

typedef struct {
	uint16  version;    /**< see definition of WL_CNT_T_VERSION */
	uint16  length;     /**< length of entire structure */

	/* transmit stat counters */
	uint32  txframe;    /**< tx data frames */
	uint32  txbyte;     /**< tx data bytes */
	uint32  txretrans;  /**< tx mac retransmits */
	uint32  txerror;    /**< tx data errors (derived: sum of others) */
	uint32  txctl;      /**< tx management frames */
	uint32  txprshort;  /**< tx short preamble frames */
	uint32  txserr;     /**< tx status errors */
	uint32  txnobuf;    /**< tx out of buffers errors */
	uint32  txnoassoc;  /**< tx discard because we're not associated */
	uint32  txrunt;     /**< tx runt frames */
	uint32  txchit;     /**< tx header cache hit (fastpath) */
	uint32  txcmiss;    /**< tx header cache miss (slowpath) */

	/* transmit chip error counters */
	uint32  txuflo;     /**< tx fifo underflows */
	uint32  txphyerr;   /**< tx phy errors (indicated in tx status) */
	uint32  txphycrs;   /**< PR8861/8963 counter */

	/* receive stat counters */
	uint32  rxframe;    /**< rx data frames */
	uint32  rxbyte;     /**< rx data bytes */
	uint32  rxerror;    /**< rx data errors (derived: sum of others) */
	uint32  rxctl;      /**< rx management frames */
	uint32  rxnobuf;    /**< rx out of buffers errors */
	uint32  rxnondata;  /**< rx non data frames in the data channel errors */
	uint32  rxbadds;    /**< rx bad DS errors */
	uint32  rxbadcm;    /**< rx bad control or management frames */
	uint32  rxfragerr;  /**< rx fragmentation errors */
	uint32  rxrunt;     /**< rx runt frames */
	uint32  rxgiant;    /**< rx giant frames */
	uint32  rxnoscb;    /**< rx no scb error */
	uint32  rxbadproto; /**< rx invalid frames */
	uint32  rxbadsrcmac;    /**< rx frames with Invalid Src Mac */
	uint32  rxbadda;    /**< rx frames tossed for invalid da */
	uint32  rxfilter;   /**< rx frames filtered out */

	/* receive chip error counters */
	uint32  rxoflo;     /**< rx fifo overflow errors */
	uint32  rxuflo[NFIFO];  /**< rx dma descriptor underflow errors */

	uint32  d11cnt_txrts_off;   /**< d11cnt txrts value when reset d11cnt */
	uint32  d11cnt_rxcrc_off;   /**< d11cnt rxcrc value when reset d11cnt */
	uint32  d11cnt_txnocts_off; /**< d11cnt txnocts value when reset d11cnt */

	/* misc counters */
	uint32  dmade;      /**< tx/rx dma descriptor errors */
	uint32  dmada;      /**< tx/rx dma data errors */
	uint32  dmape;      /**< tx/rx dma descriptor protocol errors */
	uint32  reset;      /**< reset count */
	uint32  tbtt;       /**< cnts the TBTT int's */
	uint32  txdmawar;   /**< # occurrences of PR15420 workaround */
	uint32  pkt_callback_reg_fail;  /**< callbacks register failure */

	/* MAC counters: 32-bit version of d11.h's macstat_t */
	uint32  txallfrm;   /**< total number of frames sent, incl. Data, ACK, RTS, CTS,
			     * Control Management (includes retransmissions)
			     */
	uint32  txrtsfrm;   /**< number of RTS sent out by the MAC */
	uint32  txctsfrm;   /**< number of CTS sent out by the MAC */
	uint32  txackfrm;   /**< number of ACK frames sent out */
	uint32  txdnlfrm;   /**< Not used */
	uint32  txbcnfrm;   /**< beacons transmitted */
	uint32  txfunfl[6]; /**< per-fifo tx underflows */
	uint32	rxtoolate;	/**< receive too late */
	uint32  txfbw;	    /**< transmit at fallback bw (dynamic bw) */
	uint32  txtplunfl;  /**< Template underflows (mac was too slow to transmit ACK/CTS
			     * or BCN)
			     */
	uint32  txphyerror; /**< Transmit phy error, type of error is reported in tx-status for
			     * driver enqueued frames
			     */
	uint32  rxfrmtoolong;   /**< Received frame longer than legal limit (2346 bytes) */
	uint32  rxfrmtooshrt; /**< Received frame did not contain enough bytes for its frame type */
	uint32  rxinvmachdr;    /**< Either the protocol version != 0 or frame type not
				 * data/control/management
			   */
	uint32  rxbadfcs;   /**< number of frames for which the CRC check failed in the MAC */
	uint32  rxbadplcp;  /**< parity check of the PLCP header failed */
	uint32  rxcrsglitch;    /**< PHY was able to correlate the preamble but not the header */
	uint32  rxstrt;     /**< Number of received frames with a good PLCP
			     * (i.e. passing parity check)
			     */
	uint32  rxdfrmucastmbss; /**< # of received DATA frames with good FCS and matching RA */
	uint32  rxmfrmucastmbss; /**< # of received mgmt frames with good FCS and matching RA */
	uint32  rxcfrmucast;     /**< # of received CNTRL frames with good FCS and matching RA */
	uint32  rxrtsucast; /**< number of unicast RTS addressed to the MAC (good FCS) */
	uint32  rxctsucast; /**< number of unicast CTS addressed to the MAC (good FCS) */
	uint32  rxackucast; /**< number of ucast ACKS received (good FCS) */
	uint32  rxdfrmocast;    /**< # of received DATA frames (good FCS and not matching RA) */
	uint32  rxmfrmocast;    /**< # of received MGMT frames (good FCS and not matching RA) */
	uint32  rxcfrmocast;    /**< # of received CNTRL frame (good FCS and not matching RA) */
	uint32  rxrtsocast; /**< number of received RTS not addressed to the MAC */
	uint32  rxctsocast; /**< number of received CTS not addressed to the MAC */
	uint32  rxdfrmmcast;    /**< number of RX Data multicast frames received by the MAC */
	uint32  rxmfrmmcast;    /**< number of RX Management multicast frames received by the MAC */
	uint32  rxcfrmmcast;    /**< number of RX Control multicast frames received by the MAC
				 * (unlikely to see these)
				 */
	uint32  rxbeaconmbss;   /**< beacons received from member of BSS */
	uint32  rxdfrmucastobss; /**< number of unicast frames addressed to the MAC from
				  * other BSS (WDS FRAME)
				  */
	uint32  rxbeaconobss;   /**< beacons received from other BSS */
	uint32  rxrsptmout; /**< Number of response timeouts for transmitted frames
			     * expecting a response
			     */
	uint32  bcntxcancl; /**< transmit beacons canceled due to receipt of beacon (IBSS) */
	uint32  rxf0ovfl;   /**< Number of receive fifo 0 overflows */
	uint32  rxf1ovfl;   /**< Number of receive fifo 1 overflows (obsolete) */
	uint32  rxf2ovfl;   /**< Number of receive fifo 2 overflows (obsolete) */
	uint32  txsfovfl;   /**< Number of transmit status fifo overflows (obsolete) */
	uint32  pmqovfl;    /**< Number of PMQ overflows */
	uint32  rxcgprqfrm; /**< Number of received Probe requests that made it into
			     * the PRQ fifo
			     */
	uint32  rxcgprsqovfl;   /**< Rx Probe Request Que overflow in the AP */
	uint32  txcgprsfail;    /**< Tx Probe Response Fail. AP sent probe response but did
				 * not get ACK
				 */
	uint32  txcgprssuc; /**< Tx Probe Response Success (ACK was received) */
	uint32  prs_timeout;    /**< Number of probe requests that were dropped from the PRQ
				 * fifo because a probe response could not be sent out within
				 * the time limit defined in M_PRS_MAXTIME
				 */
	uint32  rxnack;     /**< Number of NACKS received (Afterburner) */
	uint32  frmscons;   /**< Number of frames completed without transmission because of an
			     * Afterburner re-queue
			     */
	uint32  txnack;		/**< obsolete */
	uint32	rxback;		/**< blockack rxcnt */
	uint32	txback;		/**< blockack txcnt */

	/* 802.11 MIB counters, pp. 614 of 802.11 reaff doc. */
	uint32  txfrag;     /**< dot11TransmittedFragmentCount */
	uint32  txmulti;    /**< dot11MulticastTransmittedFrameCount */
	uint32  txfail;     /**< dot11FailedCount */
	uint32  txretry;    /**< dot11RetryCount */
	uint32  txretrie;   /**< dot11MultipleRetryCount */
	uint32  rxdup;      /**< dot11FrameduplicateCount */
	uint32  txrts;      /**< dot11RTSSuccessCount */
	uint32  txnocts;    /**< dot11RTSFailureCount */
	uint32  txnoack;    /**< dot11ACKFailureCount */
	uint32  rxfrag;     /**< dot11ReceivedFragmentCount */
	uint32  rxmulti;    /**< dot11MulticastReceivedFrameCount */
	uint32  rxcrc;      /**< dot11FCSErrorCount */
	uint32  txfrmsnt;   /**< dot11TransmittedFrameCount (bogus MIB?) */
	uint32  rxundec;    /**< dot11WEPUndecryptableCount */

	/* WPA2 counters (see rxundec for DecryptFailureCount) */
	uint32  tkipmicfaill;   /**< TKIPLocalMICFailures */
	uint32  tkipcntrmsr;    /**< TKIPCounterMeasuresInvoked */
	uint32  tkipreplay; /**< TKIPReplays */
	uint32  ccmpfmterr; /**< CCMPFormatErrors */
	uint32  ccmpreplay; /**< CCMPReplays */
	uint32  ccmpundec;  /**< CCMPDecryptErrors */
	uint32  fourwayfail;    /**< FourWayHandshakeFailures */
	uint32  wepundec;   /**< dot11WEPUndecryptableCount */
	uint32  wepicverr;  /**< dot11WEPICVErrorCount */
	uint32  decsuccess; /**< DecryptSuccessCount */
	uint32  tkipicverr; /**< TKIPICVErrorCount */
	uint32  wepexcluded;    /**< dot11WEPExcludedCount */

	uint32  rxundec_mcst;   /**< dot11WEPUndecryptableCount */

	/* WPA2 counters (see rxundec for DecryptFailureCount) */
	uint32  tkipmicfaill_mcst;  /**< TKIPLocalMICFailures */
	uint32  tkipcntrmsr_mcst;   /**< TKIPCounterMeasuresInvoked */
	uint32  tkipreplay_mcst;    /**< TKIPReplays */
	uint32  ccmpfmterr_mcst;    /**< CCMPFormatErrors */
	uint32  ccmpreplay_mcst;    /**< CCMPReplays */
	uint32  ccmpundec_mcst; /**< CCMPDecryptErrors */
	uint32  fourwayfail_mcst;   /**< FourWayHandshakeFailures */
	uint32  wepundec_mcst;  /**< dot11WEPUndecryptableCount */
	uint32  wepicverr_mcst; /**< dot11WEPICVErrorCount */
	uint32  decsuccess_mcst;    /**< DecryptSuccessCount */
	uint32  tkipicverr_mcst;    /**< TKIPICVErrorCount */
	uint32  wepexcluded_mcst;   /**< dot11WEPExcludedCount */

	uint32  txchanrej;  /**< Tx frames suppressed due to channel rejection */
	uint32  txexptime;  /**< Tx frames suppressed due to timer expiration */
	uint32  psmwds;     /**< Count PSM watchdogs */
	uint32  phywatchdog;    /**< Count Phy watchdogs (triggered by ucode) */

	/* MBSS counters, AP only */
	uint32  prq_entries_handled;    /**< PRQ entries read in */
	uint32  prq_undirected_entries; /**<    which were bcast bss & ssid */
	uint32  prq_bad_entries;    /**<    which could not be translated to info */
	uint32  atim_suppress_count;    /**< TX suppressions on ATIM fifo */
	uint32  bcn_template_not_ready; /**< Template marked in use on send bcn ... */
	uint32  bcn_template_not_ready_done; /**< ...but "DMA done" interrupt rcvd */
	uint32  late_tbtt_dpc;  /**< TBTT DPC did not happen in time */

	/* per-rate receive stat counters */
	uint32  rx1mbps;    /**< packets rx at 1Mbps */
	uint32  rx2mbps;    /**< packets rx at 2Mbps */
	uint32  rx5mbps5;   /**< packets rx at 5.5Mbps */
	uint32  rx6mbps;    /**< packets rx at 6Mbps */
	uint32  rx9mbps;    /**< packets rx at 9Mbps */
	uint32  rx11mbps;   /**< packets rx at 11Mbps */
	uint32  rx12mbps;   /**< packets rx at 12Mbps */
	uint32  rx18mbps;   /**< packets rx at 18Mbps */
	uint32  rx24mbps;   /**< packets rx at 24Mbps */
	uint32  rx36mbps;   /**< packets rx at 36Mbps */
	uint32  rx48mbps;   /**< packets rx at 48Mbps */
	uint32  rx54mbps;   /**< packets rx at 54Mbps */
	uint32  rx108mbps;  /**< packets rx at 108mbps */
	uint32  rx162mbps;  /**< packets rx at 162mbps */
	uint32  rx216mbps;  /**< packets rx at 216 mbps */
	uint32  rx270mbps;  /**< packets rx at 270 mbps */
	uint32  rx324mbps;  /**< packets rx at 324 mbps */
	uint32  rx378mbps;  /**< packets rx at 378 mbps */
	uint32  rx432mbps;  /**< packets rx at 432 mbps */
	uint32  rx486mbps;  /**< packets rx at 486 mbps */
	uint32  rx540mbps;  /**< packets rx at 540 mbps */

	/* pkteng rx frame stats */
	uint32  pktengrxducast; /**< unicast frames rxed by the pkteng code */
	uint32  pktengrxdmcast; /**< multicast frames rxed by the pkteng code */

	uint32  rfdisable;  /**< count of radio disables */
	uint32  bphy_rxcrsglitch;   /**< PHY count of bphy glitches */
	uint32  bphy_badplcp;

	uint32  txmpdu_sgi; /**< count for sgi transmit */
	uint32  rxmpdu_sgi; /**< count for sgi received */
	uint32  txmpdu_stbc;    /**< count for stbc transmit */
	uint32  rxmpdu_stbc;    /**< count for stbc received */

	uint32	rxdrop20s;	/**< drop secondary cnt */
	/* All counter variables have to be of uint32. */
} wl_cnt_ver_6_t;

#define	WL_DELTA_STATS_T_VERSION	2	/**< current version of wl_delta_stats_t struct */

typedef struct {
	uint16 version;     /**< see definition of WL_DELTA_STATS_T_VERSION */
	uint16 length;      /**< length of entire structure */

	/* transmit stat counters */
	uint32 txframe;     /**< tx data frames */
	uint32 txbyte;      /**< tx data bytes */
	uint32 txretrans;   /**< tx mac retransmits */
	uint32 txfail;      /**< tx failures */

	/* receive stat counters */
	uint32 rxframe;     /**< rx data frames */
	uint32 rxbyte;      /**< rx data bytes */

	/* per-rate receive stat counters */
	uint32  rx1mbps;	/**< packets rx at 1Mbps */
	uint32  rx2mbps;	/**< packets rx at 2Mbps */
	uint32  rx5mbps5;	/**< packets rx at 5.5Mbps */
	uint32  rx6mbps;	/**< packets rx at 6Mbps */
	uint32  rx9mbps;	/**< packets rx at 9Mbps */
	uint32  rx11mbps;	/**< packets rx at 11Mbps */
	uint32  rx12mbps;	/**< packets rx at 12Mbps */
	uint32  rx18mbps;	/**< packets rx at 18Mbps */
	uint32  rx24mbps;	/**< packets rx at 24Mbps */
	uint32  rx36mbps;	/**< packets rx at 36Mbps */
	uint32  rx48mbps;	/**< packets rx at 48Mbps */
	uint32  rx54mbps;	/**< packets rx at 54Mbps */
	uint32  rx108mbps;	/**< packets rx at 108mbps */
	uint32  rx162mbps;	/**< packets rx at 162mbps */
	uint32  rx216mbps;	/**< packets rx at 216 mbps */
	uint32  rx270mbps;	/**< packets rx at 270 mbps */
	uint32  rx324mbps;	/**< packets rx at 324 mbps */
	uint32  rx378mbps;	/**< packets rx at 378 mbps */
	uint32  rx432mbps;	/**< packets rx at 432 mbps */
	uint32  rx486mbps;	/**< packets rx at 486 mbps */
	uint32  rx540mbps;	/**< packets rx at 540 mbps */

	/* phy stats */
	uint32 rxbadplcp;
	uint32 rxcrsglitch;
	uint32 bphy_rxcrsglitch;
	uint32 bphy_badplcp;

	uint32 slice_index; /**< Slice for which stats are reported */

} wl_delta_stats_t;

/* Partial statistics counter report */
#define WL_CNT_CTL_MGT_FRAMES	0

typedef struct {
	uint16	type;
	uint16	len;

	/* detailed control/management frames */
	uint32	txnull;
	uint32	rxnull;
	uint32	txqosnull;
	uint32	rxqosnull;
	uint32	txassocreq;
	uint32	rxassocreq;
	uint32	txreassocreq;
	uint32	rxreassocreq;
	uint32	txdisassoc;
	uint32	rxdisassoc;
	uint32	txassocrsp;
	uint32	rxassocrsp;
	uint32	txreassocrsp;
	uint32	rxreassocrsp;
	uint32	txauth;
	uint32	rxauth;
	uint32	txdeauth;
	uint32	rxdeauth;
	uint32	txprobereq;
	uint32	rxprobereq;
	uint32	txprobersp;
	uint32	rxprobersp;
	uint32	txaction;
	uint32	rxaction;
	uint32	txrts;
	uint32	rxrts;
	uint32	txcts;
	uint32	rxcts;
	uint32	txack;
	uint32	rxack;
	uint32	txbar;
	uint32	rxbar;
	uint32	txback;
	uint32	rxback;
	uint32	txpspoll;
	uint32	rxpspoll;
} wl_ctl_mgt_cnt_t;

typedef struct {
	uint32 packets;
	uint32 bytes;
} wl_traffic_stats_t;

typedef struct {
	uint16	version;	/**< see definition of WL_WME_CNT_VERSION */
	uint16	length;		/**< length of entire structure */

	wl_traffic_stats_t tx[AC_COUNT];	/**< Packets transmitted */
	wl_traffic_stats_t tx_failed[AC_COUNT];	/**< Packets dropped or failed to transmit */
	wl_traffic_stats_t rx[AC_COUNT];	/**< Packets received */
	wl_traffic_stats_t rx_failed[AC_COUNT];	/**< Packets failed to receive */

	wl_traffic_stats_t forward[AC_COUNT];	/**< Packets forwarded by AP */

	wl_traffic_stats_t tx_expired[AC_COUNT]; /**< packets dropped due to lifetime expiry */
} wl_wme_cnt_t;

typedef struct wl_wme_cnt_v2 {
	uint16	version;	/**< see definition of WL_WME_CNT_VERSION */
	uint16	length;		/**< length of entire structure */

	wl_traffic_stats_t tx[AC_COUNT];	/**< Packets transmitted */
	wl_traffic_stats_t tx_failed[AC_COUNT];	/**< Packets dropped or failed to transmit */
	wl_traffic_stats_t rx[AC_COUNT];	/**< Packets received */
	wl_traffic_stats_t rx_failed[AC_COUNT];	/**< Packets failed to receive */

	wl_traffic_stats_t forward[AC_COUNT];	/**< Packets forwarded by AP */

	wl_traffic_stats_t tx_expired[AC_COUNT]; /**< packets dropped due to lifetime expiry */
	wl_traffic_stats_t tx_retry[AC_COUNT];  /**< Packets retried */
} wl_wme_cnt_v2_t;

typedef struct wl_wme_cnt_v3 {
	uint16	version;	/**< see definition of WL_WME_CNT_VERSION */
	uint16	length;		/**< length of entire structure */

	wl_traffic_stats_t tx_msdu[AC_COUNT];	/**< msdu packets transmitted */
	wl_traffic_stats_t rx_msdu[AC_COUNT];	/**< msdu packets received */
	/**< msdu packets failed to receive */
	wl_traffic_stats_t rx_msdu_failed[AC_COUNT];
	/**< msdu packets dropped or failed to transmit */
	wl_traffic_stats_t tx_msdu_failed[AC_COUNT];

	wl_traffic_stats_t tx_mpdu[AC_COUNT];	/**< mpdu packets transmitted */
	wl_traffic_stats_t rx_mpdu[AC_COUNT];	/**< mpdu packets received */
	/**< mpdu packets dropped or failed to transmit */
	wl_traffic_stats_t tx_mpdu_failed[AC_COUNT];
	/**< mpdu packets failed to receive */
	wl_traffic_stats_t rx_mpdu_failed[AC_COUNT];

	wl_traffic_stats_t forward[AC_COUNT];	/**< Packets forwarded by AP */

	/**< packets dropped due to lifetime expiry */
	wl_traffic_stats_t tx_mpdu_expired[AC_COUNT];
	wl_traffic_stats_t tx_mpdu_retry[AC_COUNT];  /**< Packets retried */
} wl_wme_cnt_v3_t;

#define WL_WME_CNT_VER_1	1u
#define WL_WME_CNT_VER_2	2u
#define WL_WME_CNT_VER_3	3u

/* #ifdef WLBA */

#define WLC_BA_CNT_VERSION_1  1   /**< current version of wlc_ba_cnt_t */

/** block ack related stats */
typedef struct wlc_ba_cnt {
	uint16  version;    /**< WLC_BA_CNT_VERSION */
	uint16  length;     /**< length of entire structure */

	/* transmit stat counters */
	uint32 txpdu;       /**< pdus sent */
	uint32 txsdu;       /**< sdus sent */
	uint32 txfc;        /**< tx side flow controlled packets */
	uint32 txfci;       /**< tx side flow control initiated */
	uint32 txretrans;   /**< retransmitted pdus */
	uint32 txbatimer;   /**< ba resend due to timer */
	uint32 txdrop;      /**< dropped packets */
	uint32 txaddbareq;  /**< addba req sent */
	uint32 txaddbaresp; /**< addba resp sent */
	uint32 txdelba;     /**< delba sent */
	uint32 txba;        /**< ba sent */
	uint32 txbar;       /**< bar sent */
	uint32 txpad[4];    /**< future */

	/* receive side counters */
	uint32 rxpdu;       /**< pdus recd */
	uint32 rxqed;       /**< pdus buffered before sending up */
	uint32 rxdup;       /**< duplicate pdus */
	uint32 rxnobuf;     /**< pdus discarded due to no buf */
	uint32 rxaddbareq;  /**< addba req recd */
	uint32 rxaddbaresp; /**< addba resp recd */
	uint32 rxdelba;     /**< delba recd */
	uint32 rxba;        /**< ba recd */
	uint32 rxbar;       /**< bar recd */
	uint32 rxinvba;     /**< invalid ba recd */
	uint32 rxbaholes;   /**< ba recd with holes */
	uint32 rxunexp;     /**< unexpected packets */
	uint32 rxpad[4];    /**< future */
} wlc_ba_cnt_t;
/* #endif  WLBA */

/* ##### Power Stats section ##### */

#define WL_PWRSTATS_VERSION	2
#define WL_PWRSTATS_VERSION_3	3
#define WL_PWRSTATS_VERSION_4	4

/** Input structure for pwrstats IOVAR */
typedef struct wl_pwrstats_query {
	uint16 length;			/**< Number of entries in type array. */
	uint16 type[BCM_FLEX_ARRAY];	/**< Types (tags) to retrieve.
					* Length 0 (no types) means get all.
					*/
} wl_pwrstats_query_t;

/** This structure is for version 2; version 1 will be deprecated in by FW */
#include <packed_section_start.h>
typedef BWL_PRE_PACKED_STRUCT struct wl_pwrstats {
	uint16 version;			/**< Version = 2 is TLV format */
	uint16 length;			/**< Length of entire structure */
	uint8 data[BCM_FLEX_ARRAY];	/**< TLV data, a series of structures,
				       * each starting with type and length.
				       *
				       * Padded as necessary so each section
				       * starts on a 4-byte boundary.
				       *
				       * Both type and len are uint16, but the
				       * upper nibble of length is reserved so
				       * valid len values are 0-4095.
				       */
} BWL_POST_PACKED_STRUCT wl_pwrstats_t;
#include <packed_section_end.h>
#define WL_PWR_STATS_HDRLEN	OFFSETOF(wl_pwrstats_t, data)

/* Bits for wake reasons */
#define WLC_PMD_WAKE_SET		0x1u
#define WLC_PMD_PM_AWAKE_BCN		0x2u
/* BIT:3 is no longer being used */
#define WLC_PMD_SCAN_IN_PROGRESS	0x8u
#define WLC_PMD_RM_IN_PROGRESS		0x10u
#define WLC_PMD_AS_IN_PROGRESS		0x20u
#define WLC_PMD_PM_PEND			0x40u
#define WLC_PMD_PS_POLL			0x80u
#define WLC_PMD_CHK_UNALIGN_TBTT	0x100u
#define WLC_PMD_APSD_STA_UP		0x200u
#define WLC_PMD_TX_PEND_WAR		0x400u   /* obsolete, can be reused */
#define WLC_PMD_NAN_AWAKE		0x400u   /* Reusing for NAN */
#define WLC_PMD_GPTIMER_STAY_AWAKE	0x800u
#define WLC_PMD_RRM_IN_PROGRESS		0x1000u
#define WLC_PMD_UCODE_WAKE_OVRRIDE	0x2000u
#define WLC_PMD_WD_SLP_READY_REQ	0x4000u
#define WLC_PMD_WD_TDLS			0x8000u

#define WLC_PMD_PM2_RADIO_SOFF_PEND	0x2000u
#define WLC_PMD_NON_PRIM_STA_UP		0x4000u
#define WLC_PMD_AP_UP			0x8000u
#define WLC_PMD_TX_IN_PROGRESS		0x10000u	/* Dongle awake due to packet TX */
#define WLC_PMD_4WAYHS_IN_PROGRESS	0x20000u	/* Dongle awake due to 4 way handshake */
#define WLC_PMD_PM_OVERRIDE		0x40000u	/* Dongle awake due to PM override */
#define WLC_PMD_PASN_IN_PROGRESS	0x80000u	/* Dongle awake due to PASN exchange */
#define WLC_PMD_WAKE_OTHER		0x100000u
#define WLC_PMD_USER_WAKE_REQ		0x200000u

typedef struct wlc_pm_debug {
	uint32 timestamp;	     /**< timestamp in millisecond */
	uint32 reason;		     /**< reason(s) for staying awake */
} wlc_pm_debug_t;

/** WL_PWRSTATS_TYPE_PM_AWAKE1 structures (for 6.25 firmware) */
#define WLC_STA_AWAKE_STATES_MAX_V1	30
#define WLC_PMD_EVENT_MAX_V1		32
/** Data sent as part of pwrstats IOVAR (and EXCESS_PM_WAKE event) */
#include <packed_section_start.h>
typedef BWL_PRE_PACKED_STRUCT struct pm_awake_data_v1 {
	uint32 curr_time;	/**< ms */
	uint32 hw_macc;		/**< HW maccontrol */
	uint32 sw_macc;		/**< SW maccontrol */
	uint32 pm_dur;		/**< Total sleep time in PM, msecs */
	uint32 mpc_dur;		/**< Total sleep time in MPC, msecs */

	/* int32 drifts = remote - local; +ve drift => local-clk slow */
	int32 last_drift;	/**< Most recent TSF drift from beacon */
	int32 min_drift;	/**< Min TSF drift from beacon in magnitude */
	int32 max_drift;	/**< Max TSF drift from beacon in magnitude */

	uint32 avg_drift;	/**< Avg TSF drift from beacon */

	/* Wake history tracking */
	uint8  pmwake_idx;				   /**< for stepping through pm_state */
	wlc_pm_debug_t pm_state[WLC_STA_AWAKE_STATES_MAX_V1]; /**< timestamped wake bits */
	uint32 pmd_event_wake_dur[WLC_PMD_EVENT_MAX_V1];   /**< cumulative usecs per wake reason */
	uint32 drift_cnt;	/**< Count of drift readings over which avg_drift was computed */
} BWL_POST_PACKED_STRUCT pm_awake_data_v1_t;
#include <packed_section_end.h>

#include <packed_section_start.h>
typedef BWL_PRE_PACKED_STRUCT struct wl_pwr_pm_awake_stats_v1 {
	uint16 type;	     /**< WL_PWRSTATS_TYPE_PM_AWAKE */
	uint16 len;	     /**< Up to 4K-1, top 4 bits are reserved */

	pm_awake_data_v1_t awake_data;
	uint32 frts_time;	/**< Cumulative ms spent in frts since driver load */
	uint32 frts_end_cnt;	/**< No of times frts ended since driver load */
} BWL_POST_PACKED_STRUCT wl_pwr_pm_awake_stats_v1_t;
#include <packed_section_end.h>

/** WL_PWRSTATS_TYPE_PM_AWAKE2 structures. Data sent as part of pwrstats IOVAR */
typedef struct pm_awake_data_v2 {
	uint32 curr_time;	/**< ms */
	uint32 hw_macc;		/**< HW maccontrol */
	uint32 sw_macc;		/**< SW maccontrol */
	uint32 pm_dur;		/**< Total sleep time in PM, msecs */
	uint32 mpc_dur;		/**< Total sleep time in MPC, msecs */

	/* int32 drifts = remote - local; +ve drift => local-clk slow */
	int32 last_drift;	/**< Most recent TSF drift from beacon */
	int32 min_drift;	/**< Min TSF drift from beacon in magnitude */
	int32 max_drift;	/**< Max TSF drift from beacon in magnitude */

	uint32 avg_drift;	/**< Avg TSF drift from beacon */

	/* Wake history tracking */

	/* pmstate array (type wlc_pm_debug_t) start offset */
	uint16 pm_state_offset;
	/** pmstate number of array entries */
	uint16 pm_state_len;

	/** array (type uint32) start offset */
	uint16 pmd_event_wake_dur_offset;
	/** pmd_event_wake_dur number of array entries */
	uint16 pmd_event_wake_dur_len;

	uint32 drift_cnt;	/**< Count of drift readings over which avg_drift was computed */
	uint8  pmwake_idx;	/**< for stepping through pm_state */
	uint8  flags;		/**< bit0: 1-sleep, 0- wake. bit1: 0-bit0 invlid, 1-bit0 valid */
	uint8  PAD[2];
	uint32 frts_time;	/**< Cumulative ms spent in frts since driver load */
	uint32 frts_end_cnt;	/**< No of times frts ended since driver load */
} pm_awake_data_v2_t;

typedef struct wl_pwr_pm_awake_stats_v2 {
	uint16 type;	     /**< WL_PWRSTATS_TYPE_PM_AWAKE */
	uint16 len;	     /**< Up to 4K-1, top 4 bits are reserved */

	pm_awake_data_v2_t awake_data;
} wl_pwr_pm_awake_stats_v2_t;

/* bit0: 1-sleep, 0- wake. bit1: 0-bit0 invlid, 1-bit0 valid */
#define WL_PWR_PM_AWAKE_STATS_WAKE      0x02
#define WL_PWR_PM_AWAKE_STATS_ASLEEP    0x03
#define WL_PWR_PM_AWAKE_STATS_WAKE_MASK 0x03

/* WL_PWRSTATS_TYPE_PM_AWAKE Version 2 structures taken from 4324/43342 */
/* These structures are only to be used with 4324/43342 devices */

#define WL_STA_AWAKE_STATES_MAX_V2	30
#define WL_PMD_EVENT_MAX_V2		32
#define MAX_P2P_BSS_DTIM_PRD		4

/** WL_PWRSTATS_TYPE_PM_ACCUMUL structures. Data sent as part of pwrstats IOVAR */
typedef struct pm_accum_data_v1 {
	uint64	current_ts;
	uint64	pm_cnt;
	uint64	pm_dur;
	uint64	pm_last_entry_us;
	uint64	awake_cnt;
	uint64	awake_dur;
	uint64	awake_last_entry_us;
} pm_accum_data_v1_t;

typedef struct wl_pwr_pm_accum_stats_v1 {
	uint16 type;	     /**< WL_PWRSTATS_TYPE_PM_ACCUMUL */
	uint16 len;	     /**< Up to 4K-1, top 4 bits are reserved */
	uint8 PAD[4];
	pm_accum_data_v1_t accum_data;
} wl_pwr_pm_accum_stats_v1_t;

#include <packed_section_start.h>
typedef BWL_PRE_PACKED_STRUCT struct ucode_dbg_v2 {
	uint32 macctrl;
	uint16 m_p2p_hps;
	uint16 m_p2p_bss_dtim_prd[MAX_P2P_BSS_DTIM_PRD];
	uint32 psmdebug[20];
	uint32 phydebug[20];
	uint32 psm_brc;
	uint32 ifsstat;
} BWL_POST_PACKED_STRUCT ucode_dbg_v2_t;
#include <packed_section_end.h>

#include <packed_section_start.h>
typedef BWL_PRE_PACKED_STRUCT struct pmalert_awake_data_v2 {
	uint32 curr_time;	/* ms */
	uint32 hw_macc;		/* HW maccontrol */
	uint32 sw_macc;		/* SW maccontrol */
	uint32 pm_dur;		/* Total sleep time in PM, msecs */
	uint32 mpc_dur;		/* Total sleep time in MPC, msecs */

	/* int32 drifts = remote - local; +ve drift => local-clk slow */
	int32 last_drift;	/* Most recent TSF drift from beacon */
	int32 min_drift;	/* Min TSF drift from beacon in magnitude */
	int32 max_drift;	/* Max TSF drift from beacon in magnitude */

	uint32 avg_drift;	/* Avg TSF drift from beacon */

	/* Wake history tracking */
	uint8  pmwake_idx;				   /* for stepping through pm_state */
	wlc_pm_debug_t pm_state[WL_STA_AWAKE_STATES_MAX_V2]; /* timestamped wake bits */
	uint32 pmd_event_wake_dur[WL_PMD_EVENT_MAX_V2];      /* cumulative usecs per wake reason */
	uint32 drift_cnt;	/* Count of drift readings over which avg_drift was computed */
	uint32	start_event_dur[WL_PMD_EVENT_MAX_V2]; /* start event-duration */
	ucode_dbg_v2_t ud;
	uint32 frts_time;	/* Cumulative ms spent in frts since driver load */
	uint32 frts_end_cnt;	/* No of times frts ended since driver load */
} BWL_POST_PACKED_STRUCT pmalert_awake_data_v2_t;
#include <packed_section_end.h>

#include <packed_section_start.h>
typedef BWL_PRE_PACKED_STRUCT struct pm_alert_data_v2 {
	uint32 version;
	uint32 length; /* Length of entire structure */
	uint32 reasons; /* reason(s) for pm_alert */
	/* Following fields are present only for reasons
	 * PM_DUR_EXCEEDED, MPC_DUR_EXCEEDED & CONST_AWAKE_DUR_EXCEEDED
	 */
	uint32 prev_stats_time;	/* msecs */
	uint32 prev_pm_dur;	/* msecs */
	uint32 prev_mpc_dur;	/* msecs */
	pmalert_awake_data_v2_t awake_data;
} BWL_POST_PACKED_STRUCT pm_alert_data_v2_t;
#include <packed_section_end.h>

#include <packed_section_start.h>
typedef BWL_PRE_PACKED_STRUCT struct wl_pwr_pm_awake_status_v2 {
	uint16 type;	     /* WL_PWRSTATS_TYPE_PM_AWAKE */
	uint16 len;	     /* Up to 4K-1, top 4 bits are reserved */

	pmalert_awake_data_v2_t awake_data;
	uint32 frts_time;	/* Cumulative ms spent in frts since driver load */
	uint32 frts_end_cnt;	/* No of times frts ended since driver load */
} BWL_POST_PACKED_STRUCT wl_pwr_pm_awake_status_v2_t;
#include <packed_section_end.h>

/* Below are latest definitions from PHO25178RC100_BRANCH_6_50 */
/* wl_pwr_pm_awake_stats_v1_t is used for WL_PWRSTATS_TYPE_PM_AWAKE */
/* Use regs from d11.h instead of raw addresses for */
/* (at least) the chip independent registers */
typedef struct ucode_dbg_ext {
	uint32 x120;
	uint32 x124;
	uint32 x154;
	uint32 x158;
	uint32 x15c;
	uint32 x180;
	uint32 x184;
	uint32 x188;
	uint32 x18c;
	uint32 x1a0;
	uint32 x1a8;
	uint32 x1e0;
	uint32 scr_x14;
	uint32 scr_x2b;
	uint32 scr_x2c;
	uint32 scr_x2d;
	uint32 scr_x2e;

	uint16 x40a;
	uint16 x480;
	uint16 x490;
	uint16 x492;
	uint16 x4d8;
	uint16 x4b8;
	uint16 x4ba;
	uint16 x4bc;
	uint16 x4be;
	uint16 x500;
	uint16 x50e;
	uint16 x522;
	uint16 x546;
	uint16 x578;
	uint16 x602;
	uint16 x646;
	uint16 x648;
	uint16 x666;
	uint16 x670;
	uint16 x690;
	uint16 x692;
	uint16 x6a0;
	uint16 x6a2;
	uint16 x6a4;
	uint16 x6b2;
	uint16 x7c0;

	uint16 shm_x20;
	uint16 shm_x4a;
	uint16 shm_x5e;
	uint16 shm_x5f;
	uint16 shm_xaab;
	uint16 shm_x74a;
	uint16 shm_x74b;
	uint16 shm_x74c;
	uint16 shm_x74e;
	uint16 shm_x756;
	uint16 shm_x75b;
	uint16 shm_x7b9;
	uint16 shm_x7d4;

	uint16 shm_P2P_HPS;
	uint16 shm_P2P_intr[16];
	uint16 shm_P2P_perbss[48];
} ucode_dbg_ext_t;

#include <packed_section_start.h>
typedef BWL_PRE_PACKED_STRUCT struct pm_alert_data_v1 {
	uint32 version;
	uint32 length; /**< Length of entire structure */
	uint32 reasons; /**< reason(s) for pm_alert */
	/* Following fields are present only for reasons
	 * PM_DUR_EXCEEDED, MPC_DUR_EXCEEDED & CONST_AWAKE_DUR_EXCEEDED
	 */
	uint32 prev_stats_time;	/**< msecs */
	uint32 prev_pm_dur;	/**< msecs */
	uint32 prev_mpc_dur;	/**< msecs */
	pm_awake_data_v1_t awake_data;
	uint32	start_event_dur[WLC_PMD_EVENT_MAX_V1]; /**< start event-duration */
	ucode_dbg_v2_t ud;
	uint32 frts_time;	/**< Cumulative ms spent in frts since driver load */
	uint32 frts_end_cnt;	/**< No of times frts ended since driver load */
	ucode_dbg_ext_t ud_ext;
	uint32 prev_frts_dur; /**< ms */
} BWL_POST_PACKED_STRUCT pm_alert_data_v1_t;
#include <packed_section_end.h>

/* End of 43342/4324 v2 structure definitions */

/* Original bus structure is for HSIC */

typedef struct bus_metrics {
	uint32 suspend_ct;	/**< suspend count */
	uint32 resume_ct;	/**< resume count */
	uint32 disconnect_ct;	/**< disconnect count */
	uint32 reconnect_ct;	/**< reconnect count */
	uint32 active_dur;	/**< msecs in bus, usecs for user */
	uint32 suspend_dur;	/**< msecs in bus, usecs for user */
	uint32 disconnect_dur;	/**< msecs in bus, usecs for user */
} bus_metrics_t;

#define BUS_DUMP_PARAM_VER_1		(1u)
#define SUB_CMD_MAX			(32u)
typedef struct bus_dump_param {
	uint16	version;		/**< version */
	uint16	len;			/**< length */
	uint32	flags;			/**< flags */
	uint32	value;			/**< value to set */
	char	sub_cmd[SUB_CMD_MAX];	/**< sub command name */
} bus_dump_param_t;

#define BUS_DUMP_FLAGS_CLEAR		(1u << 0u)
#define BUS_DUMP_FLAGS_SET		(1u << 1u)

/** Bus interface info for USB/HSIC */
#include <packed_section_start.h>
typedef BWL_PRE_PACKED_STRUCT struct wl_pwr_usb_hsic_stats {
	uint16 type;	     /**< WL_PWRSTATS_TYPE_USB_HSIC */
	uint16 len;	     /**< Up to 4K-1, top 4 bits are reserved */

	bus_metrics_t hsic;	/**< stats from hsic bus driver */
} BWL_POST_PACKED_STRUCT wl_pwr_usb_hsic_stats_t;
#include <packed_section_end.h>

/* PCIe Event counter tlv IDs */
enum pcie_cnt_xtlv_id {
	PCIE_CNT_XTLV_METRICS = 0x1,	/**< PCIe Bus Metrics */
	PCIE_CNT_XTLV_BUS_CNT = 0x2	/**< PCIe Bus counters */
};

typedef struct pcie_bus_metrics {
	uint32 d3_suspend_ct;	/**< suspend count */
	uint32 d0_resume_ct;	/**< resume count */
	uint32 perst_assrt_ct;	/**< PERST# assert count */
	uint32 perst_deassrt_ct;	/**< PERST# de-assert count */
	uint32 active_dur;	/**< msecs */
	uint32 d3_suspend_dur;	/**< msecs */
	uint32 perst_dur;	/**< msecs */
	uint32 l0_cnt;		/**< L0 entry count */
	uint32 l0_usecs;	/**< L0 duration in usecs */
	uint32 l1_cnt;		/**< L1 entry count */
	uint32 l1_usecs;	/**< L1 duration in usecs */
	uint32 l1_1_cnt;	/**< L1_1ss entry count */
	uint32 l1_1_usecs;	/**< L1_1ss duration in usecs */
	uint32 l1_2_cnt;	/**< L1_2ss entry count */
	uint32 l1_2_usecs;	/**< L1_2ss duration in usecs */
	uint32 l2_cnt;		/**< L2 entry count */
	uint32 l2_usecs;	/**< L2 duration in usecs */
	uint32 timestamp;	/**< Timestamp on when stats are collected */
	uint32 num_h2d_doorbell;	/**< # of doorbell interrupts - h2d */
	uint32 num_d2h_doorbell;	/**< # of doorbell interrupts - d2h */
	uint32 num_submissions; /**< # of submissions */
	uint32 num_completions; /**< # of completions */
	uint32 num_rxcmplt;	/**< # of rx completions */
	uint32 num_rxcmplt_drbl;	/**< of drbl interrupts for rx complt. */
	uint32 num_txstatus;	/**< # of tx completions */
	uint32 num_txstatus_drbl;	/**< of drbl interrupts for tx complt. */
	uint32 deepsleep_count; /**< # of times chip went to deepsleep */
	uint32 deepsleep_dur;   /**< # of msecs chip was in deepsleep */
	uint32 ltr_active_ct;	/**< # of times chip went to LTR ACTIVE */
	uint32 ltr_active_dur;	/**< # of msecs chip was in LTR ACTIVE */
	uint32 ltr_sleep_ct;	/**< # of times chip went to LTR SLEEP */
	uint32 ltr_sleep_dur;	/**< # of msecs chip was in LTR SLEEP */
} pcie_bus_metrics_t;

typedef struct pcie_bus_metrics_v2 {
	uint64 active_dur_ns;		/**< nsecs */
	uint64 d3_suspend_dur_ns;	/**< nsecs */
	uint64 perst_dur_ns;		/**< nsecs */
	uint64 timestamp_ns;		/**< Timestamp on when stats are collected */
	uint64 deepsleep_dur_ns;	/**< # of nsecs chip was in deepsleep */
	uint64 ltr_active_dur_ns;	/**< # of nsecs chip was in LTR ACTIVE */
	uint64 ltr_sleep_dur_ns;	/**< # of nsecs chip was in LTR SLEEP */
	uint32 d3_suspend_ct;		/**< suspend count */
	uint32 d0_resume_ct;		/**< resume count */
	uint32 perst_assrt_ct;		/**< PERST# assert count */
	uint32 perst_deassrt_ct;	/**< PERST# de-assert count */
	uint32 l0_cnt;			/**< L0 entry count */
	uint32 l0_usecs;		/**< L0 duration in usecs */
	uint32 l1_cnt;			/**< L1 entry count */
	uint32 l1_usecs;		/**< L1 duration in usecs */
	uint32 l1_1_cnt;		/**< L1_1ss entry count */
	uint32 l1_1_usecs;		/**< L1_1ss duration in usecs */
	uint32 l1_2_cnt;		/**< L1_2ss entry count */
	uint32 l1_2_usecs;		/**< L1_2ss duration in usecs */
	uint32 l2_cnt;			/**< L2 entry count */
	uint32 l2_usecs;		/**< L2 duration in usecs */
	uint32 num_h2d_doorbell;	/**< # of doorbell interrupts - h2d */
	uint32 num_d2h_doorbell;	/**< # of doorbell interrupts - d2h */
	uint32 num_submissions;		/**< # of submissions */
	uint32 num_completions;		/**< # of completions */
	uint32 num_rxcmplt;		/**< # of rx completions */
	uint32 num_rxcmplt_drbl;	/**< of drbl interrupts for rx complt. */
	uint32 num_txstatus;		/**< # of tx completions */
	uint32 num_txstatus_drbl;	/**< of drbl interrupts for tx complt. */
	uint32 deepsleep_count;		/**< # of times chip went to deepsleep */
	uint32 ltr_active_ct;		/**< # of times chip went to LTR ACTIVE */
	uint32 ltr_sleep_ct;		/**< # of times chip went to LTR SLEEP */
} pcie_bus_metrics_v2_t;

typedef struct pcie_bus_metrics_v3 {
	uint32 d3_suspend_ct;	/**< suspend count */
	uint32 d0_resume_ct;	/**< resume count */
	uint32 perst_assrt_ct;	/**< PERST# assert count */
	uint32 perst_deassrt_ct;	/**< PERST# de-assert count */
	uint32 active_dur;	/**< msecs */
	uint32 d3_suspend_dur;	/**< msecs */
	uint32 perst_dur;	/**< msecs */
	uint32 l0_cnt;		/**< L0 entry count */
	uint32 l0_usecs;	/**< L0 duration in usecs */
	uint32 l1_cnt;		/**< L1 entry count */
	uint32 l1_usecs;	/**< L1 duration in usecs */
	uint32 l1_1_cnt;	/**< L1_1ss entry count */
	uint32 l1_1_usecs;	/**< L1_1ss duration in usecs */
	uint32 l1_2_cnt;	/**< L1_2ss entry count */
	uint32 l1_2_usecs;	/**< L1_2ss duration in usecs */
	uint32 l2_cnt;		/**< L2 entry count */
	uint32 l2_usecs;	/**< L2 duration in usecs */
	uint32 timestamp;	/**< Timestamp on when stats are collected */
	uint32 num_h2d_doorbell;	/**< # of doorbell interrupts - h2d */
	uint32 num_d2h_doorbell;	/**< # of doorbell interrupts - d2h */
	uint32 num_submissions; /**< # of submissions */
	uint32 num_completions; /**< # of completions */
	uint32 num_rxcmplt;	/**< # of rx completions */
	uint32 num_rxcmplt_drbl;	/**< of drbl interrupts for rx complt. */
	uint32 num_txstatus;	/**< # of tx completions */
	uint32 num_txstatus_drbl;	/**< of drbl interrupts for tx complt. */
	uint32 deepsleep_count; /**< # of times chip went to deepsleep */
	uint32 deepsleep_dur;   /**< # of msecs chip was in deepsleep */
	uint32 ltr_active_ct;	/**< # of times chip went to LTR ACTIVE */
	uint32 ltr_active_dur;	/**< # of msecs chip was in LTR ACTIVE */
	uint32 ltr_sleep_ct;	/**< # of times chip went to LTR SLEEP */
	uint32 ltr_sleep_dur;	/**< # of msecs chip was in LTR SLEEP */
	uint32 shallow_dur;	/**< # shallow state duration */
} pcie_bus_metrics_v3_t;

typedef struct pcie_cnt {
	uint32 ltr_state; /**< Current LTR state */
	uint32 l0_sr_cnt; /**< SR count during L0 */
	uint32 l2l3_sr_cnt; /**< SR count during L2L3 */
	uint32 d3_ack_sr_cnt; /**< srcount during last D3-ACK */
	uint32 d3_sr_cnt; /**< SR count during D3 */
	uint32 d3_info_start; /**< D3 INFORM received time */
	uint32 d3_info_enter_cnt; /**< # of D3 INFORM received */
	uint32 d3_cnt; /**< # of real D3 */
	uint32 d3_ack_sent_cnt; /**< # of D3 ACK sent count */
	uint32 d3_drop_cnt_event; /**< # of events dropped during D3 */
	uint32 d2h_req_q_len; /**< # of Packet pending in D2H request queue */
	uint32 hw_reason; /**< Last Host wake assert reason */
	uint32 hw_assert_cnt; /**< # of times Host wake Asserted */
	uint32 host_ready_cnt; /**< # of Host ready interrupts */
	uint32 hw_assert_reason_0; /**< timestamp when hw_reason is TRAP  */
	uint32 hw_assert_reason_1; /**< timestamp when hw_reason is WL_EVENT */
	uint32 hw_assert_reason_2; /**< timestamp when hw_reason is DATA */
	uint32 hw_assert_reason_3; /**< timestamp when hw_reason is DELAYED_WAKE */
	uint32 last_host_ready; /**< Timestamp of last Host ready */
	bool hw_asserted; /**< Flag to indicate if Host wake is Asserted */
	bool event_delivery_pend; /**< No resources to send event */
	uint16 PAD; /**< Word alignment for scripts */
} pcie_cnt_t;

/** Bus interface info for PCIE */
typedef struct wl_pwr_pcie_stats {
	uint16 type;			/**< WL_PWRSTATS_TYPE_PCIE */
	uint16 len;			/**< Up to 4K-1, top 4 bits are reserved */
	pcie_bus_metrics_t	pcie;	/**< stats from pcie bus driver */
} wl_pwr_pcie_stats_t;

/** Bus interface info for PCIE */
typedef struct wl_pwr_pcie_stats_v3 {
	uint16 type;                    /**< WL_PWRSTATS_TYPE_PCIE */
	uint16 len;                     /**< Up to 4K-1, top 4 bits are reserved */
	pcie_bus_metrics_v3_t   pcie;   /**< stats from pcie bus driver */
} wl_pwr_pcie_stats_v3_t;

typedef struct wl_pwr_pcie_stats_v2 {
	uint16 type;			/**< WL_PWRSTATS_TYPE_PCIE */
	uint16 len;			/**< Up to 4K-1, top 4 bits are reserved */
	uint32 pad;
	pcie_bus_metrics_v2_t pcie;	/**< stats from pcie bus driver */
} wl_pwr_pcie_stats_v2_t;

typedef struct scan_data_ext_v1 {
	uint32 count;		/**< Number of scans performed */
	uint32 dur;		/**< Total time (in us) used */
	uint32 off_chan_dur;	/**< Total time excluding home channel time */
} scan_data_ext_v1_t;

typedef struct wl_pwr_scan_stats_ext_v1 {
	uint16 type;				/**< WL_PWRSTATS_TYPE_SCAN_EXT */
	uint16 len;				/**< Up to 4K-1, top 4 bits are reserved */

	/* Scan history */
	scan_data_ext_v1_t user_scans;		/**< User-requested scans: (i/e/p)scan */
	scan_data_ext_v1_t assoc_scans;		/**< Scans initiated by association requests */
	scan_data_ext_v1_t roam_scans;		/**< Scans initiated by the roam engine */
	scan_data_ext_v1_t pno_scans[8];	/**< For future PNO bucketing (BSSID, SSID, etc) */
	scan_data_ext_v1_t other_scans;		/**< Scan engine usage not assigned to the above */
} wl_pwr_scan_stats_ext_v1_t;

/** Scan information history per category */
typedef struct scan_data {
	uint32 count;		/**< Number of scans performed */
	uint32 dur;		/**< Total time (in us) used */
} scan_data_t;

typedef struct wl_pwr_scan_stats {
	uint16 type;			/**< WL_PWRSTATS_TYPE_SCAN */
	uint16 len;			/**< Up to 4K-1, top 4 bits are reserved */

	/* Scan history */
	scan_data_t user_scans;		/**< User-requested scans: (i/e/p)scan */
	scan_data_t assoc_scans;	/**< Scans initiated by association requests */
	scan_data_t roam_scans;		/**< Scans initiated by the roam engine */
	scan_data_t pno_scans[8];	/**< For future PNO bucketing (BSSID, SSID, etc) */
	scan_data_t other_scans;	/**< Scan engine usage not assigned to the above */
} wl_pwr_scan_stats_t;

typedef struct wl_pwr_connect_stats {
	uint16 type;	     /**< WL_PWRSTATS_TYPE_CONNECTION */
	uint16 len;	     /**< Up to 4K-1, top 4 bits are reserved */

	/* Connection (Association + Key exchange) data */
	uint32 count;	/**< Number of connections performed */
	uint32 dur;		/**< Total time (in ms) used */
} wl_pwr_connect_stats_t;

typedef struct wl_pwr_phy_stats {
	uint16 type;	    /**< WL_PWRSTATS_TYPE_PHY */
	uint16 len;	    /**< Up to 4K-1, top 4 bits are reserved */
	uint32 tx_dur;	    /**< TX Active duration in us */
	uint32 rx_dur;	    /**< RX Active duration in us */
} wl_pwr_phy_stats_t;

typedef struct wl_mimo_meas_metrics_v1 {
	uint16 type;
	uint16 len;
	/* Total time(us) idle in MIMO RX chain configuration */
	uint32 total_idle_time_mimo;
	/* Total time(us) idle in SISO  RX chain configuration */
	uint32 total_idle_time_siso;
	/* Total receive time (us) in SISO RX chain configuration */
	uint32 total_rx_time_siso;
	/* Total receive time (us) in MIMO RX chain configuration */
	uint32 total_rx_time_mimo;
	/* Total 1-chain transmit time(us) */
	uint32 total_tx_time_1chain;
	/* Total 2-chain transmit time(us) */
	uint32 total_tx_time_2chain;
	/* Total 3-chain transmit time(us) */
	uint32 total_tx_time_3chain;
} wl_mimo_meas_metrics_v1_t;

typedef struct wl_mimo_meas_metrics {
	uint16 type;
	uint16 len;
	/* Total time(us) idle in MIMO RX chain configuration */
	uint32 total_idle_time_mimo;
	/* Total time(us) idle in SISO  RX chain configuration */
	uint32 total_idle_time_siso;
	/* Total receive time (us) in SISO RX chain configuration */
	uint32 total_rx_time_siso;
	/* Total receive time (us) in MIMO RX chain configuration */
	uint32 total_rx_time_mimo;
	/* Total 1-chain transmit time(us) */
	uint32 total_tx_time_1chain;
	/* Total 2-chain transmit time(us) */
	uint32 total_tx_time_2chain;
	/* Total 3-chain transmit time(us) */
	uint32 total_tx_time_3chain;
	/* End of original, OCL fields start here */
	/* Total time(us) idle in ocl mode */
	uint32 total_idle_time_ocl;
	/* Total receive time (us) in ocl mode */
	uint32 total_rx_time_ocl;
	/* End of OCL fields, internal adjustment fields here */
	/* Total SIFS idle time in MIMO mode */
	uint32 total_sifs_time_mimo;
	/* Total SIFS idle time in SISO mode */
	uint32 total_sifs_time_siso;
} wl_mimo_meas_metrics_t;

typedef struct wl_pwr_slice_index {
	uint16 type;	     /* WL_PWRSTATS_TYPE_SLICE_INDEX */
	uint16 len;

	uint32 slice_index;	/* Slice index for which stats are meant for */
} wl_pwr_slice_index_t;

typedef struct wl_pwr_tsync_stats {
	uint16 type;		/**< WL_PWRSTATS_TYPE_TSYNC */
	uint16 len;
	uint32 avb_uptime;	/**< AVB uptime in msec */
} wl_pwr_tsync_stats_t;

typedef struct wl_pwr_ops_stats {
	uint16 type;			/* WL_PWRSTATS_TYPE_OPS_STATS */
	uint16 len;			/* total length includes fixed fields */
	uint32 partial_ops_dur;		/* Total time(in usec) partial ops duration */
	uint32 full_ops_dur;		/* Total time(in usec) full ops duration */
} wl_pwr_ops_stats_t;

typedef struct wl_pwr_bcntrim_stats {
	uint16 type;			/* WL_PWRSTATS_TYPE_BCNTRIM_STATS */
	uint16 len;			/* total length includes fixed fields */
	uint8  associated;		/* STA is associated ? */
	uint8  slice_idx;		/* on which slice STA is associated */
	uint16 PAD;			/* padding */
	uint32 slice_beacon_seen;	/* number of beacons seen on the Infra
		                         * interface on this slice
		                         */
	uint32 slice_beacon_trimmed;	/* number beacons actually trimmed on this slice */
	uint32 total_beacon_seen;	/* total number of beacons seen on the Infra interface */
	uint32 total_beacon_trimmed;	/* total beacons actually trimmed */
} wl_pwr_bcntrim_stats_t;

typedef struct wl_pwr_slice_index_band {
	uint16 type;			/* WL_PWRSTATS_TYPE_SLICE_INDEX_BAND_INFO */
	uint16 len;			/* Total length includes fixed fields */
	uint16 index;			/* Slice Index */
	int16  bandtype;		/* Slice Bandtype */
} wl_pwr_slice_index_band_t;

typedef struct wl_pwr_psbw_stats {
	uint16 type;			/* WL_PWRSTATS_TYPE_PSBW_STATS */
	uint16 len;			/* total length includes fixed fields */
	uint8  slice_idx;		/* on which slice STA is associated */
	uint8  PAD[3];
	uint32 slice_enable_dur;	/* time(ms) psbw remains enabled on this slice */
	uint32 total_enable_dur;	/* time(ms) psbw remains enabled total */
} wl_pwr_psbw_stats_t;

typedef struct wl_pwr_scan_6E_stats {
	uint16 type;			/* WL_PWRSTATS_TYPE_SCAN_6E */
	uint16 len;			/* total length includes fixed fields */
	uint32 rx_upr_processed;	/* total unsolicited probe responses processed */
	uint32 rx_upr_ignored;		/* total unsolicited probe responses ignored */

	uint32 rx_fils_processed;	/* total FILS processed */
	uint32 rx_fils_ignored;		/* total FILS ignored */

	uint32 referred_6g_scans;	/* Referred scans to 6G channels due to RNR */
} wl_pwr_scan_6E_stats_t;

/* ##### End of Power Stats section ##### */

/* Version of wlc_btc_stats_t structure.
 * Increment whenever a change is made to wlc_btc_stats_t
 */
#define BTCX_STATS_VER_13 13
typedef struct wlc_btc_stats_v13 {
	uint16 version;			/* version number of struct */
	uint16 valid;			/* validness */
	uint32 stats_update_timestamp;	/* tStamp when data is updated. */
	uint32 btc_status;		/* btc status log */
	uint32 bt_gcishm_active_task_bm; /* Active task bitmap of BT shared thru gci shm */
	uint32 bt_gcishm_bt_tasks; /* BT Tasks info shared in GCI Shm */
	uint32 bt_req_type_map;		/* BT Antenna Req types since last stats sample */
	uint32 bt_req_cnt;		/* #BT antenna requests since last stats sampl */
	uint32 bt_gnt_cnt;		/* #BT antenna grants since last stats sample */
	uint32 bt_gnt_dur;		/* usec BT owns antenna since last stats sample */
	uint16 bt_abort_cnt;		/* #Times WL was preempted due to BT since WL up */
	uint16 bt_latency_cnt;		/* #Time ucode high latency detected since WL up */
	uint16 bt_pm_protect_cnt;	/* PM protection count requested by Coex */
	uint16 bt_succ_pm_protect_cnt;	/* successful PM protection */
	uint16 bt_succ_cts_cnt;		/* successful CTS2A protection */
	uint16 bt_wlan_tx_preempt_cnt;	/* WLAN TX Preemption */
	uint16 bt_wlan_rx_preempt_cnt;	/* WLAN RX Preemption */
	uint16 bt_ap_tx_after_pm_cnt;	/* AP TX even after PM protection */
	uint16 bt_peraud_cumu_gnt_cnt;	/* Grant cnt for periodic audio */
	uint16 bt_peraud_cumu_deny_cnt; /* Deny cnt for periodic audio */
	uint16 bt_a2dp_cumu_gnt_cnt;	/* Grant cnt for A2DP */
	uint16 bt_a2dp_cumu_deny_cnt;	/* Deny cnt for A2DP */
	uint16 bt_sniff_cumu_gnt_cnt;	/* Grant cnt for Sniff */
	uint16 bt_sniff_cumu_deny_cnt;	/* Deny cnt for Sniff */
	uint16 bt_frameburst_ack_cncl_cnt;	/* Count of Ack Cancel for Frame Burst */
	uint16 bt_le_scan_tx_intr_cnt;	/* LE Scan Tx Interrupt Count */
	uint16 bt_le_scan_intr_cnt; /* LE Scan INterrupt Count */
	uint16 bt_a2dp_grant_ext_intr;	/* A2DP Grant Extension Count */
	uint16 bt_a2dp_uhp_intr_cnt;	/* A2DP UHP Interrupt Count */
	uint16 bt_pred_out_of_sync_cnt; /* Predictor Out Of Sync Count */
	uint16 bt_isoc_intr_cnt;	/* ISOC Interrupt count */
	uint16 bt_ampdu_collision_cnt;	/* BTCX ampdu collision count */
	uint16 bt_back_collision_cnt;	/* BTCX BACK collision count */
	uint16 bt_dcsn_map;		/* Accumulated decision bitmap once Ant grant */
	uint16 bt_dcsn_cnt;		/* Accumulated decision bitmap counters once Ant grant */
	uint16 bt_a2dp_hiwat_cnt;	/* Ant grant by a2dp high watermark */
	uint16 bt_datadelay_cnt;	/* Ant grant by acl/a2dp datadelay */
	uint16 bt_crtpri_cnt;		/* Ant grant by critical BT task */
	uint16 bt_pri_cnt;		/* Ant grant by high BT task */
	uint16 a2dpbuf1cnt;		/* Ant request with a2dp buffercnt 1 */
	uint16 a2dpbuf2cnt;		/* Ant request with a2dp buffercnt 2 */
	uint16 a2dpbuf3cnt;		/* Ant request with a2dp buffercnt 3 */
	uint16 a2dpbuf4cnt;		/* Ant request with a2dp buffercnt 4 */
	uint16 a2dpbuf5cnt;		/* Ant request with a2dp buffercnt 5 */
	uint16 a2dpbuf6cnt;		/* Ant request with a2dp buffercnt 6 */
	uint16 a2dpbuf7cnt;		/* Ant request with a2dp buffercnt 7 */
	uint16 a2dpbuf8cnt;		/* Ant request with a2dp buffercnt 8 */
	uint16 antgrant_lt10ms;		/* Ant grant duration cnt 0~10ms */
	uint16 antgrant_lt30ms;		/* Ant grant duration cnt 10~30ms */
	uint16 antgrant_lt60ms;		/* Ant grant duration cnt 30~60ms */
	uint16 antgrant_ge60ms;		/* Ant grant duration cnt 60~ms */
	uint16 wldurn_ge0ms;		/* WL duration count between 0-5ms */
	uint16 wldurn_ge5ms;		/* WL duration count between 5-12ms */
	uint16 wldurn_ge12ms;		/* WL duration count between 12-21ms */
	uint16 wldurn_ge21ms;		/* WL duration count between 21-30ms */
	uint16 wldurn_ge30ms;		/* WL duration count between 30-65ms */
	uint16 wldurn_ge65ms;		/* WL Duration greater than 65ms */
	uint16 btcx_override_cnt;	/* Btcx override count */
	uint32 btcx_override_dur;	/* Btcx override duration */
	uint16 fbaci_status_idx0;	/* channel and maxgain index for index0 */
	uint16 fbaci_nsamples_idx0;	/* number of samples at index0 */
	uint16 fbaci_status_idx1;	/* channel and maxgain index for index1 */
	uint16 fbaci_nsamples_idx1;	/* number of samples at index1 */
	uint16 fbaci_status_idx2;	/* channel and maxgain index for index2 */
	uint16 fbaci_nsamples_idx2;	/* number of samples at index2 */
	uint16 fbaci_status_idx3;	/* channel and maxgain index for index3 */
	uint16 fbaci_nsamples_idx3;	/* number of samples at index3 */
	uint16 idle2fbc_cnt;		/* count of radio state changes from IDle to FBC */
	uint16 idle2wlauxrx_cnt;	/* count of radio state changes from IDle to Aux Ded Rx. */
	uint32 fbcx_ovd_cnt;		/* FBC override cnt */
	uint32 fbcx_ovd_dur;		/* FBC override duration */
	uint32 fbcx_bt_forced_fbc_cnt;	/* bt fored fbc cnt */
	uint32 fbcx_bt_forced_fbc_dur;	/* bt forced fbc_dur */
	uint32 fbcx_bt_auto_fbc_cnt;	/* bt auto fbc cnt */
	uint32 fbcx_act_cfg;		/* bt act cfg mask to put bt in fbc */
	uint8 fbaci_acipwr_cdf_idx_c0_ch0;
	/* core0, channel0 histogram index of  2% ACI power */
	uint8 fbaci_acipwr_cdf_idx_c1_ch0;
	/* core1, channel0 histogram index of  2% ACI power */
	uint16 fbaci_acipwr_cdf_cnt_c0_ch0;
	/* core0, channel0 histogram count of ACI power (2%) */
	uint16 fbaci_acipwr_cdf_cnt_c1_ch0;
	/* core1, channel0 histogram count of ACI power (2%) */
	uint8 fbaci_acipwr_cdf_idx_c0_ch1;
	/* core0, channel1 histogram index of  2% ACI power */
	uint8 fbaci_acipwr_cdf_idx_c1_ch1;
	/* core1, channel1 histogram index of  2% ACI power */
	uint16 fbaci_acipwr_cdf_cnt_c0_ch1;
	/* core0, channel1 histogram count of ACI power (2%) */
	uint16 fbaci_acipwr_cdf_cnt_c1_ch1;
	/* core1, channel1 histogram count of ACI power (2%) */
	uint8 fbaci_acipwr_cdf_idx_c0_ch2;
	/* core0, channel2 histogram index of  2% ACI power */
	uint8 fbaci_acipwr_cdf_idx_c1_ch2;
	/* core1, channel2 histogram index of  2% ACI power */
	uint16 fbaci_acipwr_cdf_cnt_c0_ch2;
	/* core0, channel2 histogram count of ACI power (2%) */
	uint16 fbaci_acipwr_cdf_cnt_c1_ch2;
	/* core1, channel2 histogram count of ACI power (2%) */
	uint8 fbaci_acipwr_cdf_idx_c0_ch3;
	/* core0, channel3 histogram index of  2% ACI power */
	uint8 fbaci_acipwr_cdf_idx_c1_ch3;
	/* core1, channel3 histogram index of  2% ACI power */
	uint16 fbaci_acipwr_cdf_cnt_c0_ch3;
	/* core0, channel3 histogram count of ACI power (2%) */
	uint16 fbaci_acipwr_cdf_cnt_c1_ch3;
	/* core1, channel3 histogram count of ACI power (2%) */
	uint32 fbagc_fbc_gain_stuck_cnt;
	/* fbc gain stuck counter */
	uint32 fbcx_forced_ded_dur;	/* forced dedicated cnt */
	uint32 bt_gcishm_bt_stats;	/* BT Tasks info shared in GCI Shm */
	uint32 hybrid_durn_win;		/* Hybrid Window Duration */
	uint32 bt_gcishm_sar_query_cnt;	/* Dynamic SAR query count */
	uint8 cxcpu_ranging_cnt;
	uint8 cxcpu_assoc_cnt;
	uint8 cxcpu_scan_cnt;
	uint8 pad[1];
	uint16 bt_pm_no_ack_cnt;	/* BTCX no Ack for PM count */
	uint16 bt_thread_crt_cnt;	/* BTCX Thread Critical count */
	uint16 bt_thread_hi_pri_cnt;	/* BTCX Thread Hi Pri count */
} wlc_btc_stats_v13_t;

#define BTCX_STATS_VER_12 12
typedef struct wlc_btc_stats_v12 {
	uint16 version; /* version number of struct */
	uint16 len; /* length */
	uint32 stats_update_timestamp;	/* tStamp when data is updated. */
	uint32 btc_status; /* Hybrid/TDM indicator: Bit2:Hybrid, Bit1:TDM,Bit0:CoexEnabled */
	uint32 bt_req_type_map; /* BT Antenna Req types since last stats sample */
	uint32 bt_req_cnt; /* #BT antenna requests since last stats sampl */
	uint32 bt_gnt_cnt; /* #BT antenna grants since last stats sample */
	uint32 bt_gnt_dur; /* usec BT owns antenna since last stats sample */
	uint16 bt_abort_cnt; /* #Times WL was preempted due to BT since WL up */
	uint16 bt_rxf1ovfl_cnt; /* #Time PSNULL retry count exceeded since WL up */
	uint16 bt_latency_cnt; /* #Time ucode high latency detected since WL up */
	uint16 bt_pm_attempt_cnt; /* PM protection attempts */
	uint16 bt_succ_pm_protect_cnt; /* successful PM protection */
	uint16 bt_succ_cts_cnt; /* successful CTS2A protection */
	uint16 bt_wlan_tx_preempt_cnt; /* WLAN TX Preemption */
	uint16 bt_wlan_rx_preempt_cnt; /* WLAN RX Preemption */
	uint16 bt_ap_tx_after_pm_cnt; /* AP TX even after PM protection */
	uint16 bt_peraud_cumu_gnt_cnt; /* Grant cnt for periodic audio */
	uint16 bt_peraud_cumu_deny_cnt; /* Deny cnt for periodic audio */
	uint16 bt_a2dp_cumu_gnt_cnt; /* Grant cnt for A2DP */
	uint16 bt_a2dp_cumu_deny_cnt; /* Deny cnt for A2DP */
	uint16 bt_sniff_cumu_gnt_cnt; /* Grant cnt for Sniff */
	uint16 bt_sniff_cumu_deny_cnt; /* Deny cnt for Sniff */
	uint16 bt_crtpri_cnt; /* Ant grant by critical BT task */
	uint16 bt_pri_cnt; /* Ant grant by high BT task */
	uint16 antgrant_lt10ms; /* Ant grant duration cnt 0~10ms */
	uint16 antgrant_lt30ms; /* Ant grant duration cnt 10~30ms */
	uint16 antgrant_lt60ms; /* Ant grant duration cnt 30~60ms */
	uint16 antgrant_ge60ms; /* Ant grant duration cnt 60~ms */
	uint16 ap_leakiness; /* AP leakines, ms */
	uint8 rr_cnt; /* WLAN rate recovery count */
	uint8 rr_succ_cnt; /* WLAN successful rate recovery count */
	uint8 slice_index; /* Slice to report. 0: 5GHz, 1: 2.4GHz. */
	int8 btcx_desense_mode; /* btcoex desense mode, 0 - 7 */
	int8 wlrssi; /* the snapshot of wl rssi */
	int8 btrssi; /* the snapshot of bt rssi */
	int8 profile_2g_active; /* 2G active profile index */
	int8 profile_5g_active; /* 5G active profile index */
	uint16 mac_inactive_dur; /* MAC core sleep time, ms */
	uint16 bt_pm_attempt_noack_cnt; /* PM1 packets that not acked by peer */
	uint32 btc_status2; /* BT coex status 2 */
	uint32 bt5g_status;			/* BT 5G coex status.
						 * 0: BT5G Coex enabled,
						 * 1: protection enabled,
						 * 2: BT active,
						 * 3-5: recent band switch rsn, reserved for Fire.
						 * 6: PM protection enabled
						 * 7-31: reserved for Fire
						 */
	uint16 bt5g_defer_cnt;			/* BT5G starts band switch with WL5G activity
						 * is deferred till BT 5G move out completion.
						 */
	uint16 bt5g_no_defer_cnt;		/* BT5G starts band switch immediately
						 * without defer(delay). No wait for BT 5G
						 * move out completion.
						 */
	uint32 bt5g_defer_max_switch_dur;	/* maximum defer
						 * band switching took in ms.
						 */
	uint32 bt5g_no_defer_max_switch_dur;	/* maximum no-defer
						 * band switching took in ms.
						 */
	uint16 bt5g_switch_succ_cnt;		/* BT5G band switch success within a
						 * maximum delay timeout.
						 */
	uint16 bt5g_switch_fail_cnt;		/* BT5G band switch fails within a
						 * maximum delay timeout.
						 */
	uint16 bt5g_switch_reason_bm;		/* WLAN reason bitmap triggering BT5G band switch.
						 * bit0: Host, bit1: Infra, bit2: AWDL/NAN,
						 * bit3: Personal Hot Spot.
						 */
	uint8 pad[2];
} wlc_btc_stats_v12_t;

#define BTCX_STATS_VER_11 11
typedef struct wlc_btc_stats_v11 {
	uint16 version; /* version number of struct */
	uint16 len; /* length */
	uint32 stats_update_timestamp;	/* tStamp when data is updated. */
	uint32 btc_status; /* Hybrid/TDM indicator: Bit2:Hybrid, Bit1:TDM,Bit0:CoexEnabled */
	uint32 bt_req_type_map; /* BT Antenna Req types since last stats sample */
	uint32 bt_req_cnt; /* #BT antenna requests since last stats sampl */
	uint32 bt_gnt_cnt; /* #BT antenna grants since last stats sample */
	uint32 bt_gnt_dur; /* usec BT owns antenna since last stats sample */
	uint16 bt_abort_cnt; /* #Times WL was preempted due to BT since WL up */
	uint16 bt_rxf1ovfl_cnt; /* #Time PSNULL retry count exceeded since WL up */
	uint16 bt_latency_cnt; /* #Time ucode high latency detected since WL up */
	uint16 bt_pm_attempt_cnt; /* PM protection attempts */
	uint16 bt_succ_pm_protect_cnt; /* successful PM protection */
	uint16 bt_succ_cts_cnt; /* successful CTS2A protection */
	uint16 bt_wlan_tx_preempt_cnt; /* WLAN TX Preemption */
	uint16 bt_wlan_rx_preempt_cnt; /* WLAN RX Preemption */
	uint16 bt_ap_tx_after_pm_cnt; /* AP TX even after PM protection */
	uint16 bt_peraud_cumu_gnt_cnt; /* Grant cnt for periodic audio */
	uint16 bt_peraud_cumu_deny_cnt; /* Deny cnt for periodic audio */
	uint16 bt_a2dp_cumu_gnt_cnt; /* Grant cnt for A2DP */
	uint16 bt_a2dp_cumu_deny_cnt; /* Deny cnt for A2DP */
	uint16 bt_sniff_cumu_gnt_cnt; /* Grant cnt for Sniff */
	uint16 bt_sniff_cumu_deny_cnt; /* Deny cnt for Sniff */
	uint16 bt_crtpri_cnt; /* Ant grant by critical BT task */
	uint16 bt_pri_cnt; /* Ant grant by high BT task */
	uint16 antgrant_lt10ms; /* Ant grant duration cnt 0~10ms */
	uint16 antgrant_lt30ms; /* Ant grant duration cnt 10~30ms */
	uint16 antgrant_lt60ms; /* Ant grant duration cnt 30~60ms */
	uint16 antgrant_ge60ms; /* Ant grant duration cnt 60~ms */
	uint16 ap_leakiness; /* AP leakines, ms */
	uint8 rr_cnt; /* WLAN rate recovery count */
	uint8 rr_succ_cnt; /* WLAN successful rate recovery count */
	uint8 slice_index; /* Slice to report. 0: 5GHz, 1: 2.4GHz. */
	int8 btcx_desense_mode; /* btcoex desense mode, 0 - 7 */
	int8 wlrssi; /* the snapshot of wl rssi */
	int8 btrssi; /* the snapshot of bt rssi */
	int8 profile_2g_active; /* 2G active profile index */
	int8 profile_5g_active; /* 5G active profile index */
	uint16 mac_inactive_dur; /* MAC core sleep time, ms */
	uint16 bt_pm_attempt_noack_cnt; /* PM1 packets that not acked by peer */
	uint32 btc_status2; /* BT coex status 2 */
} wlc_btc_stats_v11_t;

#define BTCX_STATS_VER_10 10
typedef struct wlc_btc_stats_v10 {
	uint16 version;			/* version number of struct */
	uint16 valid;			/* validness */
	uint32 stats_update_timestamp;	/* tStamp when data is updated. */
	uint32 btc_status;		/* btc status log */
	uint32 bt_gcishm_active_task_bm; /* Active task bitmap of BT shared thru gci shm */
	uint32 bt_gcishm_bt_tasks; /* BT Tasks info shared in GCI Shm */
	uint32 bt_req_type_map;		/* BT Antenna Req types since last stats sample */
	uint32 bt_req_cnt;		/* #BT antenna requests since last stats sampl */
	uint32 bt_gnt_cnt;		/* #BT antenna grants since last stats sample */
	uint32 bt_gnt_dur;		/* usec BT owns antenna since last stats sample */
	uint16 bt_abort_cnt;		/* #Times WL was preempted due to BT since WL up */
	uint16 bt_latency_cnt;		/* #Time ucode high latency detected since WL up */
	uint16 bt_pm_protect_cnt;	/* PM protection count requested by Coex */
	uint16 bt_succ_pm_protect_cnt;	/* successful PM protection */
	uint16 bt_succ_cts_cnt;		/* successful CTS2A protection */
	uint16 bt_wlan_tx_preempt_cnt;	/* WLAN TX Preemption */
	uint16 bt_wlan_rx_preempt_cnt;	/* WLAN RX Preemption */
	uint16 bt_ap_tx_after_pm_cnt;	/* AP TX even after PM protection */
	uint16 bt_peraud_cumu_gnt_cnt;	/* Grant cnt for periodic audio */
	uint16 bt_peraud_cumu_deny_cnt;	/* Deny cnt for periodic audio */
	uint16 bt_a2dp_cumu_gnt_cnt;	/* Grant cnt for A2DP */
	uint16 bt_a2dp_cumu_deny_cnt;	/* Deny cnt for A2DP */
	uint16 bt_sniff_cumu_gnt_cnt;	/* Grant cnt for Sniff */
	uint16 bt_sniff_cumu_deny_cnt;	/* Deny cnt for Sniff */
	uint16 bt_frameburst_ack_cncl_cnt;	/* Count of Ack Cancel for Frame Burst */
	uint16 bt_le_scan_tx_intr_cnt;	/* LE Scan Tx Interrupt Count */
	uint16 bt_le_scan_intr_cnt;	/* LE Scan INterrupt Count */
	uint16 bt_a2dp_grant_ext_intr;	/* A2DP Grant Extension Count */
	uint16 bt_a2dp_uhp_intr_cnt;	/* A2DP UHP Interrupt Count */
	uint16 bt_pred_out_of_sync_cnt;	/* Predictor Out Of Sync Count */
	uint16 bt_isoc_intr_cnt;	/* ISOC Interrupt count */
	uint16 bt_ampdu_collision_cnt;	/* BTCX ampdu collision count */
	uint16 bt_back_collision_cnt;	/* BTCX BACK collision count */
	uint16 bt_dcsn_map;		/* Accumulated decision bitmap once Ant grant */
	uint16 bt_dcsn_cnt;		/* Accumulated decision bitmap counters once Ant grant */
	uint16 bt_a2dp_hiwat_cnt;	/* Ant grant by a2dp high watermark */
	uint16 bt_datadelay_cnt;	/* Ant grant by acl/a2dp datadelay */
	uint16 bt_crtpri_cnt;		/* Ant grant by critical BT task */
	uint16 bt_pri_cnt;		/* Ant grant by high BT task */
	uint16 a2dpbuf1cnt;		/* Ant request with a2dp buffercnt 1 */
	uint16 a2dpbuf2cnt;		/* Ant request with a2dp buffercnt 2 */
	uint16 a2dpbuf3cnt;		/* Ant request with a2dp buffercnt 3 */
	uint16 a2dpbuf4cnt;		/* Ant request with a2dp buffercnt 4 */
	uint16 a2dpbuf5cnt;		/* Ant request with a2dp buffercnt 5 */
	uint16 a2dpbuf6cnt;		/* Ant request with a2dp buffercnt 6 */
	uint16 a2dpbuf7cnt;		/* Ant request with a2dp buffercnt 7 */
	uint16 a2dpbuf8cnt;		/* Ant request with a2dp buffercnt 8 */
	uint16 antgrant_lt10ms;		/* Ant grant duration cnt 0~10ms */
	uint16 antgrant_lt30ms;		/* Ant grant duration cnt 10~30ms */
	uint16 antgrant_lt60ms;		/* Ant grant duration cnt 30~60ms */
	uint16 antgrant_ge60ms;		/* Ant grant duration cnt 60~ms */
	uint16 wldurn_ge0ms;		/* WL duration count between 0-5ms */
	uint16 wldurn_ge5ms;		/* WL duration count between 5-12ms */
	uint16 wldurn_ge12ms;		/* WL duration count between 12-21ms */
	uint16 wldurn_ge21ms;		/* WL duration count between 21-30ms */
	uint16 wldurn_ge30ms;		/* WL duration count between 30-65ms */
	uint16 wldurn_ge65ms;		/* WL Duration greater than 65ms */
	uint16 nan_idle_cnt;		/* Nan Idle Slot Count */
	uint16 nan_pre_dw_cnt;		/* Nan Pre Dw Slot Count */
	uint16 nan_pre_data_cnt;	/* Nan Pre Data Slot Count */
	uint16 nan_post_dw_cnt;		/* Nan Post Dw Slot Count */
	uint16 nan_dw_cnt;		/* Nan Dw Slot Count */
	uint16 nan_data_p1_cnt;		/* Nan P1 Data Slot Count */
	uint16 nan_data_p2_cnt;		/* Nan P2 Data Slot Count */
	uint16 nan_pri_deny_cnt;	/* Nan Priority Slot Denial Count */
	uint16 PAD;			/* Padding */
} wlc_btc_stats_v10_t;

#define BTCX_STATS_VER_9 9
typedef struct wlc_btc_stats_v9 {
	uint16 version; /* version number of struct */
	uint16 valid; /* validness */
	uint32 stats_update_timestamp;	/* tStamp when data is updated. */
	uint32 btc_status; /* Hybrid/TDM indicator: Bit2:Hybrid, Bit1:TDM,Bit0:CoexEnabled */
	uint32 bt_req_type_map; /* BT Antenna Req types since last stats sample */
	uint32 bt_req_cnt; /* #BT antenna requests since last stats sampl */
	uint32 bt_gnt_cnt; /* #BT antenna grants since last stats sample */
	uint32 bt_gnt_dur; /* usec BT owns antenna since last stats sample */
	uint16 bt_abort_cnt; /* #Times WL was preempted due to BT since WL up */
	uint16 bt_rxf1ovfl_cnt; /* #Time PSNULL retry count exceeded since WL up */
	uint16 bt_latency_cnt; /* #Time ucode high latency detected since WL up */
	uint16 bt_pm_attempt_cnt; /* PM protection attempts */
	uint16 bt_succ_pm_protect_cnt; /* successful PM protection */
	uint16 bt_succ_cts_cnt; /* successful CTS2A protection */
	uint16 bt_wlan_tx_preempt_cnt; /* WLAN TX Preemption */
	uint16 bt_wlan_rx_preempt_cnt; /* WLAN RX Preemption */
	uint16 bt_ap_tx_after_pm_cnt; /* AP TX even after PM protection */
	uint16 bt_peraud_cumu_gnt_cnt; /* Grant cnt for periodic audio */
	uint16 bt_peraud_cumu_deny_cnt; /* Deny cnt for periodic audio */
	uint16 bt_a2dp_cumu_gnt_cnt; /* Grant cnt for A2DP */
	uint16 bt_a2dp_cumu_deny_cnt; /* Deny cnt for A2DP */
	uint16 bt_sniff_cumu_gnt_cnt; /* Grant cnt for Sniff */
	uint16 bt_sniff_cumu_deny_cnt; /* Deny cnt for Sniff */
	uint16 bt_crtpri_cnt; /* Ant grant by critical BT task */
	uint16 bt_pri_cnt; /* Ant grant by high BT task */
	uint16 antgrant_lt10ms; /* Ant grant duration cnt 0~10ms */
	uint16 antgrant_lt30ms; /* Ant grant duration cnt 10~30ms */
	uint16 antgrant_lt60ms; /* Ant grant duration cnt 30~60ms */
	uint16 antgrant_ge60ms; /* Ant grant duration cnt 60~ms */
	uint16 ap_leakiness; /* AP leakines, ms */
	uint8 rr_cnt; /* WLAN rate recovery count */
	uint8 rr_succ_cnt; /* WLAN successful rate recovery count */
	uint8 slice_index; /* Slice to report */
	uint8 PAD; /* Padding */
} wlc_btc_stats_v9_t;

#define BTCX_STATS_VER_8 8
typedef struct wlc_btc_stats_v8 {
	uint16 version;			/* version number of struct */
	uint16 valid;			/* validness */
	uint32 stats_update_timestamp;	/* tStamp when data is updated. */
	uint32 btc_status;		/* btc status log */
	uint32 bt_gcishm_active_task_bm; /* Active task bitmap of BT shared thru gci shm */
	uint32 bt_gcishm_bt_tasks; /* BT Tasks info shared in GCI Shm */
	uint32 bt_req_type_map;		/* BT Antenna Req types since last stats sample */
	uint32 bt_req_cnt;		/* #BT antenna requests since last stats sampl */
	uint32 bt_gnt_cnt;		/* #BT antenna grants since last stats sample */
	uint32 bt_gnt_dur;		/* usec BT owns antenna since last stats sample */
	uint16 bt_abort_cnt;		/* #Times WL was preempted due to BT since WL up */
	uint16 bt_latency_cnt;		/* #Time ucode high latency detected since WL up */
	uint16 bt_pm_protect_cnt;	/* PM protection count requested by Coex */
	uint16 bt_succ_pm_protect_cnt;	/* successful PM protection */
	uint16 bt_succ_cts_cnt;		/* successful CTS2A protection */
	uint16 bt_wlan_tx_preempt_cnt;	/* WLAN TX Preemption */
	uint16 bt_wlan_rx_preempt_cnt;	/* WLAN RX Preemption */
	uint16 bt_ap_tx_after_pm_cnt;	/* AP TX even after PM protection */
	uint16 bt_peraud_cumu_gnt_cnt;	/* Grant cnt for periodic audio */
	uint16 bt_peraud_cumu_deny_cnt;	/* Deny cnt for periodic audio */
	uint16 bt_a2dp_cumu_gnt_cnt;	/* Grant cnt for A2DP */
	uint16 bt_a2dp_cumu_deny_cnt;	/* Deny cnt for A2DP */
	uint16 bt_sniff_cumu_gnt_cnt;	/* Grant cnt for Sniff */
	uint16 bt_sniff_cumu_deny_cnt;	/* Deny cnt for Sniff */
	uint16 bt_frameburst_ack_cncl_cnt;	/* Count of Ack Cancel for Frame Burst */
	uint16 bt_le_scan_tx_intr_cnt;	/* LE Scan Tx Interrupt Count */
	uint16 bt_le_scan_intr_cnt;	/* LE Scan INterrupt Count */
	uint16 bt_a2dp_grant_ext_intr;	/* A2DP Grant Extension Count */
	uint16 bt_a2dp_grant_ext_prcsd_cnt;	/* A2DP Grant Extension Processed Count */
	uint16 bt_pred_out_of_sync_cnt;	/* Predictor Out Of Sync Count */
	uint16 bt_dcsn_map;		/* Accumulated decision bitmap once Ant grant */
	uint16 bt_dcsn_cnt;		/* Accumulated decision bitmap counters once Ant grant */
	uint16 bt_a2dp_hiwat_cnt;	/* Ant grant by a2dp high watermark */
	uint16 bt_datadelay_cnt;	/* Ant grant by acl/a2dp datadelay */
	uint16 bt_crtpri_cnt;		/* Ant grant by critical BT task */
	uint16 bt_pri_cnt;		/* Ant grant by high BT task */
	uint16 a2dpbuf1cnt;		/* Ant request with a2dp buffercnt 1 */
	uint16 a2dpbuf2cnt;		/* Ant request with a2dp buffercnt 2 */
	uint16 a2dpbuf3cnt;		/* Ant request with a2dp buffercnt 3 */
	uint16 a2dpbuf4cnt;		/* Ant request with a2dp buffercnt 4 */
	uint16 a2dpbuf5cnt;		/* Ant request with a2dp buffercnt 5 */
	uint16 a2dpbuf6cnt;		/* Ant request with a2dp buffercnt 6 */
	uint16 a2dpbuf7cnt;		/* Ant request with a2dp buffercnt 7 */
	uint16 a2dpbuf8cnt;		/* Ant request with a2dp buffercnt 8 */
	uint16 antgrant_lt10ms;		/* Ant grant duration cnt 0~10ms */
	uint16 antgrant_lt30ms;		/* Ant grant duration cnt 10~30ms */
	uint16 antgrant_lt60ms;		/* Ant grant duration cnt 30~60ms */
	uint16 antgrant_ge60ms;		/* Ant grant duration cnt 60~ms */
	uint16 wldurn_ge0ms;		/* WL duration count between 0-5ms */
	uint16 wldurn_ge5ms;		/* WL duration count between 5-12ms */
	uint16 wldurn_ge12ms;		/* WL duration count between 12-21ms */
	uint16 wldurn_ge21ms;		/* WL duration count between 21-30ms */
	uint16 wldurn_ge30ms;		/* WL duration count between 30-65ms */
	uint16 wldurn_ge65ms;		/* WL Duration greater than 65ms */
	uint16 nan_idle_cnt;		/* Nan Idle Slot Count */
	uint16 nan_pre_dw_cnt;		/* Nan Pre Dw Slot Count */
	uint16 nan_pre_data_cnt;	/* Nan Pre Data Slot Count */
	uint16 nan_post_dw_cnt;		/* Nan Post Dw Slot Count */
	uint16 nan_dw_cnt;		/* Nan Dw Slot Count */
	uint16 nan_data_p1_cnt;		/* Nan P1 Data Slot Count */
	uint16 nan_data_p2_cnt;		/* Nan P2 Data Slot Count */
	uint16 nan_pri_deny_cnt;	/* Nan Priority Slot Denial Count */
} wlc_btc_stats_v8_t;

#define BTCX_STATS_VER_7 7
typedef struct wlc_btc_stats_v7 {
	uint16 version; /* version number of struct */
	uint16 valid; /* validness */
	uint32 stats_update_timestamp;	/* tStamp when data is updated. */
	uint32 btc_status; /* Hybrid/TDM indicator: Bit2:Hybrid, Bit1:TDM,Bit0:CoexEnabled */
	uint32 bt_req_type_map; /* BT Antenna Req types since last stats sample */
	uint32 bt_req_cnt; /* #BT antenna requests since last stats sampl */
	uint32 bt_gnt_cnt; /* #BT antenna grants since last stats sample */
	uint32 bt_gnt_dur; /* usec BT owns antenna since last stats sample */
	uint16 bt_abort_cnt; /* #Times WL was preempted due to BT since WL up */
	uint16 bt_rxf1ovfl_cnt; /* #Time PSNULL retry count exceeded since WL up */
	uint16 bt_latency_cnt; /* #Time ucode high latency detected since WL up */
	uint16 bt_pm_attempt_cnt; /* PM protection attempts */
	uint16 bt_succ_pm_protect_cnt; /* successful PM protection */
	uint16 bt_succ_cts_cnt; /* successful CTS2A protection */
	uint16 bt_wlan_tx_preempt_cnt; /* WLAN TX Preemption */
	uint16 bt_wlan_rx_preempt_cnt; /* WLAN RX Preemption */
	uint16 bt_ap_tx_after_pm_cnt; /* AP TX even after PM protection */
	uint16 bt_peraud_cumu_gnt_cnt; /* Grant cnt for periodic audio */
	uint16 bt_peraud_cumu_deny_cnt; /* Deny cnt for periodic audio */
	uint16 bt_a2dp_cumu_gnt_cnt; /* Grant cnt for A2DP */
	uint16 bt_a2dp_cumu_deny_cnt; /* Deny cnt for A2DP */
	uint16 bt_sniff_cumu_gnt_cnt; /* Grant cnt for Sniff */
	uint16 bt_sniff_cumu_deny_cnt; /* Deny cnt for Sniff */
	uint16 bt_crtpri_cnt; /* Ant grant by critical BT task */
	uint16 bt_pri_cnt; /* Ant grant by high BT task */
	uint16 antgrant_lt10ms; /* Ant grant duration cnt 0~10ms */
	uint16 antgrant_lt30ms; /* Ant grant duration cnt 10~30ms */
	uint16 antgrant_lt60ms; /* Ant grant duration cnt 30~60ms */
	uint16 antgrant_ge60ms; /* Ant grant duration cnt 60~ms */
	uint8 slice_index; /* Slice to report */
	uint8 PAD; /* Padding */
} wlc_btc_stats_v7_t;

#define BTCX_STATS_VER_6 6
typedef struct wlc_btc_stats_v6 {
	uint16 version; /* version number of struct */
	uint16 valid; /* validness */
	uint32 stats_update_timestamp;	/* tStamp when data is updated. */
	uint32 btc_status; /* Hybrid/TDM indicator: Bit2:Hybrid, Bit1:TDM,Bit0:CoexEnabled */
	uint32 bt_req_type_map; /* BT Antenna Req types since last stats sample */
	uint32 bt_req_cnt; /* #BT antenna requests since last stats sampl */
	uint32 bt_gnt_cnt; /* #BT antenna grants since last stats sample */
	uint32 bt_gnt_dur; /* usec BT owns antenna since last stats sample */
	uint16 bt_abort_cnt; /* #Times WL was preempted due to BT since WL up */
	uint16 bt_rxf1ovfl_cnt; /* #Time PSNULL retry count exceeded since WL up */
	uint16 bt_latency_cnt; /* #Time ucode high latency detected since WL up */
	uint16 bt_pm_attempt_cnt; /* PM protection attempts */
	uint16 bt_succ_pm_protect_cnt; /* successful PM protection */
	uint16 bt_succ_cts_cnt; /* successful CTS2A protection */
	uint16 bt_wlan_tx_preempt_cnt; /* WLAN TX Preemption */
	uint16 bt_wlan_rx_preempt_cnt; /* WLAN RX Preemption */
	uint16 bt_ap_tx_after_pm_cnt; /* AP TX even after PM protection */
	uint16 bt_peraud_cumu_gnt_cnt; /* Grant cnt for periodic audio */
	uint16 bt_peraud_cumu_deny_cnt; /* Deny cnt for periodic audio */
	uint16 bt_a2dp_cumu_gnt_cnt; /* Grant cnt for A2DP */
	uint16 bt_a2dp_cumu_deny_cnt; /* Deny cnt for A2DP */
	uint16 bt_sniff_cumu_gnt_cnt; /* Grant cnt for Sniff */
	uint16 bt_sniff_cumu_deny_cnt; /* Deny cnt for Sniff */
	uint8 PAD; /* Padding */
	uint8 slice_index; /* Slice to report */
} wlc_btc_stats_v6_t;

#define BTCX_STATS_VER_5 5
typedef struct wlc_btc_stats_v5 {
	uint16 version;			/* version number of struct */
	uint16 valid;			/* validness */
	uint32 stats_update_timestamp;	/* tStamp when data is updated. */
	uint32 btc_status;		/* btc status log */
	uint32 bt_req_type_map;		/* BT Antenna Req types since last stats sample */
	uint32 bt_req_cnt;		/* #BT antenna requests since last stats sampl */
	uint32 bt_gnt_cnt;		/* #BT antenna grants since last stats sample */
	uint32 bt_gnt_dur;		/* usec BT owns antenna since last stats sample */
	uint16 bt_abort_cnt;		/* #Times WL was preempted due to BT since WL up */
	uint16 bt_latency_cnt;		/* #Time ucode high latency detected since WL up */
	uint16 bt_pm_protect_cnt;	/* PM protection count requested by Coex */
	uint16 bt_succ_pm_protect_cnt;	/* successful PM protection */
	uint16 bt_succ_cts_cnt;		/* successful CTS2A protection */
	uint16 bt_wlan_tx_preempt_cnt;	/* WLAN TX Preemption */
	uint16 bt_wlan_rx_preempt_cnt;	/* WLAN RX Preemption */
	uint16 bt_ap_tx_after_pm_cnt;	/* AP TX even after PM protection */
	uint16 bt_peraud_cumu_gnt_cnt;	/* Grant cnt for periodic audio */
	uint16 bt_peraud_cumu_deny_cnt;	/* Deny cnt for periodic audio */
	uint16 bt_a2dp_cumu_gnt_cnt;	/* Grant cnt for A2DP */
	uint16 bt_a2dp_cumu_deny_cnt;	/* Deny cnt for A2DP */
	uint16 bt_sniff_cumu_gnt_cnt;	/* Grant cnt for Sniff */
	uint16 bt_sniff_cumu_deny_cnt;	/* Deny cnt for Sniff */
	uint16 bt_frameburst_ack_cncl_cnt;	/* Count of Ack Cancel for Frame Burst */
	uint16 bt_le_scan_tx_intr_cnt;	/* LE Scan Tx Interrupt Count */
	uint16 bt_le_scan_intr_cnt;	/* LE Scan INterrupt Count */
	uint16 bt_a2dp_grant_ext_intr;	/* A2DP Grant Extension Count */
	uint16 bt_a2dp_grant_ext_prcsd_cnt;	/* A2DP Grant Extension Processed Count */
	uint16 bt_pred_out_of_sync_cnt;	/* Predictor Out Of Sync Count */
	uint16 bt_dcsn_map;		/* Accumulated decision bitmap once Ant grant */
	uint16 bt_dcsn_cnt;		/* Accumulated decision bitmap counters once Ant grant */
	uint16 bt_a2dp_hiwat_cnt;	/* Ant grant by a2dp high watermark */
	uint16 bt_datadelay_cnt;	/* Ant grant by acl/a2dp datadelay */
	uint16 bt_crtpri_cnt;		/* Ant grant by critical BT task */
	uint16 bt_pri_cnt;		/* Ant grant by high BT task */
	uint16 a2dpbuf1cnt;		/* Ant request with a2dp buffercnt 1 */
	uint16 a2dpbuf2cnt;		/* Ant request with a2dp buffercnt 2 */
	uint16 a2dpbuf3cnt;		/* Ant request with a2dp buffercnt 3 */
	uint16 a2dpbuf4cnt;		/* Ant request with a2dp buffercnt 4 */
	uint16 a2dpbuf5cnt;		/* Ant request with a2dp buffercnt 5 */
	uint16 a2dpbuf6cnt;		/* Ant request with a2dp buffercnt 6 */
	uint16 a2dpbuf7cnt;		/* Ant request with a2dp buffercnt 7 */
	uint16 a2dpbuf8cnt;		/* Ant request with a2dp buffercnt 8 */
	uint16 antgrant_lt10ms;		/* Ant grant duration cnt 0~10ms */
	uint16 antgrant_lt30ms;		/* Ant grant duration cnt 10~30ms */
	uint16 antgrant_lt60ms;		/* Ant grant duration cnt 30~60ms */
	uint16 antgrant_ge60ms;		/* Ant grant duration cnt 60~ms */
	uint16 wldurn_ge0ms;		/* WL duration count between 0-5ms */
	uint16 wldurn_ge5ms;		/* WL duration count between 5-12ms */
	uint16 wldurn_ge12ms;		/* WL duration count between 12-21ms */
	uint16 wldurn_ge21ms;		/* WL duration count between 21-30ms */
	uint16 wldurn_ge30ms;		/* WL duration count between 30-65ms */
	uint16 wldurn_ge65ms;		/* WL Duration greater than 65ms */
	uint16 nan_idle_cnt;		/* Nan Idle Slot Count */
	uint16 nan_pre_dw_cnt;		/* Nan Pre Dw Slot Count */
	uint16 nan_pre_data_cnt;	/* Nan Pre Data Slot Count */
	uint16 nan_post_dw_cnt;		/* Nan Post Dw Slot Count */
	uint16 nan_dw_cnt;		/* Nan Dw Slot Count */
	uint16 nan_data_p1_cnt;		/* Nan P1 Data Slot Count */
	uint16 nan_data_p2_cnt;		/* Nan P2 Data Slot Count */
	uint16 nan_pri_deny_cnt;	/* Nan Priority Slot Denial Count */
} wlc_btc_stats_v5_t;

#define BTCX_STATS_VER_4 4
typedef struct wlc_btc_stats_v4 {
	uint16 version; /* version number of struct */
	uint16 valid; /* validness */
	uint32 stats_update_timestamp;	/* tStamp when data is updated. */
	uint32 btc_status; /* Hybrid/TDM indicator: Bit2:Hybrid, Bit1:TDM,Bit0:CoexEnabled */
	uint32 bt_req_type_map; /* BT Antenna Req types since last stats sample */
	uint32 bt_req_cnt; /* #BT antenna requests since last stats sampl */
	uint32 bt_gnt_cnt; /* #BT antenna grants since last stats sample */
	uint32 bt_gnt_dur; /* usec BT owns antenna since last stats sample */
	uint16 bt_abort_cnt; /* #Times WL was preempted due to BT since WL up */
	uint16 bt_rxf1ovfl_cnt; /* #Time PSNULL retry count exceeded since WL up */
	uint16 bt_latency_cnt; /* #Time ucode high latency detected since WL up */
	uint16 bt_succ_pm_protect_cnt; /* successful PM protection */
	uint16 bt_succ_cts_cnt; /* successful CTS2A protection */
	uint16 bt_wlan_tx_preempt_cnt; /* WLAN TX Preemption */
	uint16 bt_wlan_rx_preempt_cnt; /* WLAN RX Preemption */
	uint16 bt_ap_tx_after_pm_cnt; /* AP TX even after PM protection */
	uint16 bt_peraud_cumu_gnt_cnt; /* Grant cnt for periodic audio */
	uint16 bt_peraud_cumu_deny_cnt; /* Deny cnt for periodic audio */
	uint16 bt_a2dp_cumu_gnt_cnt; /* Grant cnt for A2DP */
	uint16 bt_a2dp_cumu_deny_cnt; /* Deny cnt for A2DP */
	uint16 bt_sniff_cumu_gnt_cnt; /* Grant cnt for Sniff */
	uint16 bt_sniff_cumu_deny_cnt; /* Deny cnt for Sniff */
	uint16 bt_dcsn_map; /* Accumulated decision bitmap once Ant grant */
	uint16 bt_dcsn_cnt; /* Accumulated decision bitmap counters once Ant grant */
	uint16 bt_a2dp_hiwat_cnt; /* Ant grant by a2dp high watermark */
	uint16 bt_datadelay_cnt; /* Ant grant by acl/a2dp datadelay */
	uint16 bt_crtpri_cnt; /* Ant grant by critical BT task */
	uint16 bt_pri_cnt; /* Ant grant by high BT task */
	uint16 a2dpbuf1cnt;	/* Ant request with a2dp buffercnt 1 */
	uint16 a2dpbuf2cnt;	/* Ant request with a2dp buffercnt 2 */
	uint16 a2dpbuf3cnt;	/* Ant request with a2dp buffercnt 3 */
	uint16 a2dpbuf4cnt;	/* Ant request with a2dp buffercnt 4 */
	uint16 a2dpbuf5cnt;	/* Ant request with a2dp buffercnt 5 */
	uint16 a2dpbuf6cnt;	/* Ant request with a2dp buffercnt 6 */
	uint16 a2dpbuf7cnt;	/* Ant request with a2dp buffercnt 7 */
	uint16 a2dpbuf8cnt;	/* Ant request with a2dp buffercnt 8 */
	uint16 antgrant_lt10ms; /* Ant grant duration cnt 0~10ms */
	uint16 antgrant_lt30ms; /* Ant grant duration cnt 10~30ms */
	uint16 antgrant_lt60ms; /* Ant grant duration cnt 30~60ms */
	uint16 antgrant_ge60ms; /* Ant grant duration cnt 60~ms */
} wlc_btc_stats_v4_t;

#define BTCX_STATS_VER_3 3

typedef struct wlc_btc_stats_v3 {
	uint16 version; /* version number of struct */
	uint16 valid; /* validness */
	uint32 stats_update_timestamp;	/* tStamp when data is updated. */
	uint32 btc_status; /* Hybrid/TDM indicator: Bit2:Hybrid, Bit1:TDM,Bit0:CoexEnabled */
	uint32 bt_req_type_map; /* BT Antenna Req types since last stats sample */
	uint32 bt_req_cnt; /* #BT antenna requests since last stats sampl */
	uint32 bt_gnt_cnt; /* #BT antenna grants since last stats sample */
	uint32 bt_gnt_dur; /* usec BT owns antenna since last stats sample */
	uint16 bt_abort_cnt; /* #Times WL was preempted due to BT since WL up */
	uint16 bt_rxf1ovfl_cnt; /* #Time PSNULL retry count exceeded since WL up */
	uint16 bt_latency_cnt; /* #Time ucode high latency detected since WL up */
	uint16 rsvd; /* pad to align struct to 32bit bndry	 */
	uint16 bt_succ_pm_protect_cnt; /* successful PM protection */
	uint16 bt_succ_cts_cnt; /* successful CTS2A protection */
	uint16 bt_wlan_tx_preempt_cnt; /* WLAN TX Preemption */
	uint16 bt_wlan_rx_preempt_cnt; /* WLAN RX Preemption */
	uint16 bt_ap_tx_after_pm_cnt; /* AP TX even after PM protection */
	uint16 bt_peraud_cumu_gnt_cnt; /* Grant cnt for periodic audio */
	uint16 bt_peraud_cumu_deny_cnt; /* Deny cnt for periodic audio */
	uint16 bt_a2dp_cumu_gnt_cnt; /* Grant cnt for A2DP */
	uint16 bt_a2dp_cumu_deny_cnt; /* Deny cnt for A2DP */
	uint16 bt_sniff_cumu_gnt_cnt; /* Grant cnt for Sniff */
	uint16 bt_sniff_cumu_deny_cnt; /* Deny cnt for Sniff */
	uint8 PAD; /* Padding */
	uint8 slice_index; /* Slice to report */
} wlc_btc_stats_v3_t;

#define BTCX_STATS_VER_2 2

typedef struct wlc_btc_stats_v2 {
	uint16 version; /* version number of struct */
	uint16 valid; /* validness */
	uint32 stats_update_timestamp;	/* tStamp when data is updated. */
	uint32 btc_status; /* Hybrid/TDM indicator: Bit2:Hybrid, Bit1:TDM,Bit0:CoexEnabled */
	uint32 bt_req_type_map; /* BT Antenna Req types since last stats sample */
	uint32 bt_req_cnt; /* #BT antenna requests since last stats sampl */
	uint32 bt_gnt_cnt; /* #BT antenna grants since last stats sample */
	uint32 bt_gnt_dur; /* usec BT owns antenna since last stats sample */
	uint16 bt_abort_cnt; /* #Times WL was preempted due to BT since WL up */
	uint16 bt_rxf1ovfl_cnt; /* #Time PSNULL retry count exceeded since WL up */
	uint16 bt_latency_cnt; /* #Time ucode high latency detected since WL up */
	uint16 rsvd; /* pad to align struct to 32bit bndry	 */
} wlc_btc_stats_v2_t;

#define TXCAL_MAX_PA_MODE		4	/* signed for assigning minus for undefined */

#define ACPHY_OBSS_SUBBAND_CNT		8u	/* Max sub band counts i.e., 160Mhz = 8 * 20MHZ */

#define	PHY_RX_GAIN_INDICES		16u	/* num of Rx gain indices */
#define	PHY_TX_GAIN_CAL			4u	/* num of Tx gain indices */

typedef struct phy_ecounter_v1 {
	chanspec_t	chanspec;
	uint8		slice;
	uint8		PAD;
	uint16		phy_wdg;	/* Count of times watchdog happened. */
	uint16		noise_req;	/* Count of phy noise sample requests. */
	uint16		noise_crsbit;	/* Count of CRS high during noisecal request. */
	uint16		noise_apply;	/* Count of applying noisecal result to crsmin. */
	uint16		cal_counter;	/* Count of performing single and multi phase cal. */
} phy_ecounter_v1_t;

typedef struct phy_ecounter_log_core_v1 {
	int8	crsmin_pwr;		/* Noise level for applied desense */
	int8	noise_level_inst;	/* Instantaneous noise cal pwr */
} phy_ecounter_log_core_v1_t;

typedef struct phy_ecounter_log_core_v2 {
	int8	crsmin_pwr;			/* Noise level for applied desense */
	int8	rssi_per_ant;			/* Instantaneous noise cal pwr */
	int8	phylog_noise_pwr_array[8];	/* noise buffer array */
} phy_ecounter_log_core_v2_t;

typedef struct phy_ecounter_log_core_v3 {
	uint16	bad_txbaseidx_cnt;	/* cntr for tx_baseidx=127 in healthcheck */
	uint16	curr_tssival;		/* TxPwrCtrlInit_path[01].TSSIVal */
	uint16	pwridx_init;		/* TxPwrCtrlInit_path[01].pwrIndex_init_path[01] */
	uint16	auxphystats;		/* Indicates the PHY stats for aux slice */
	uint16	phystatsgaininfo;	/* Indicates the gain stats */
	uint16	flexgaininfo_A;		/* Indicates the gain settings */
	uint8	crsmin_pwr_idx;		/* Index to the crsminpower threshold array */
	uint8	baseindxval;		/* TPC Base index */
	int8	crsmin_pwr;		/* Noise level for applied desense */
	int8	noise_level_inst;	/* Instantaneous noise cal pwr */
	int8	tgt_pwr;		/* Programmed Target power */
	int8	estpwradj;		/* Current Est Power Adjust value */
	uint8	PAD1[2];
} phy_ecounter_log_core_v3_t;

typedef struct phy_ecounter_log_core_v4 {
	uint16	bad_txbaseidx_cnt;	/* cntr for tx_baseidx=127 in healthcheck */
	uint16	curr_tssival;		/* TxPwrCtrlInit_path[01].TSSIVal */
	uint16	pwridx_init;		/* TxPwrCtrlInit_path[01].pwrIndex_init_path[01] */
	uint16	auxphystats;		/* Indicates the PHY stats for aux slice */
	uint16	phystatsgaininfo;	/* Indicates the gain stats */
	uint16	flexgaininfo_A;		/* Indicates the gain settings */
	uint8	crsmin_pwr_idx;		/* Index to the crsminpower threshold array */
	uint8	baseindxval;		/* TPC Base index */
	int8	crsmin_pwr;		/* Noise level for applied desense */
	int8	noise_level_inst;	/* Instantaneous noise cal pwr */
	int8	tgt_pwr;		/* Programmed Target power */
	int8	estpwradj;		/* Current Est Power Adjust value */
	int8	ed_threshold;		/* Energy detection threshold */
	uint8	PAD1;
	int8	obss_pwrest[ACPHY_OBSS_SUBBAND_CNT];	/* OBSS signal power per sub-band in dBm */
} phy_ecounter_log_core_v4_t;

#define PHY_ECOUNTER_LOG_CORE_VER5_SIZE		48u
typedef struct phy_ecounter_log_core_v5 {
	uint16	bad_txbaseidx_cnt;	/* cntr for tx_baseidx=127 in healthcheck */
	uint16	curr_tssival;		/* TxPwrCtrlInit_path[01].TSSIVal */
	uint16	pwridx_init;		/* TxPwrCtrlInit_path[01].pwrIndex_init_path[01] */
	uint16	auxphystats;		/* Indicates the PHY stats for aux slice */
	uint16	phystatsgaininfo;	/* Indicates the gain stats */
	uint16	flexgaininfo_A;		/* Indicates the gain settings */
	uint8	crsmin_pwr_idx;		/* Index to the crsminpower threshold array */
	uint8	baseindxval;		/* TPC Base index */
	int8	crsmin_pwr;		/* Noise level for applied desense */
	int8	noise_level_inst;	/* Instantaneous noise cal pwr */
	int8	tgt_pwr;		/* Programmed Target power */
	int8	estpwradj;		/* Current Est Power Adjust value */
	int8	ed_threshold;		/* Energy detection threshold */
	uint8	debug_01;		/* for future debugging */
	int16	debug_02;		/* for future debugging */
	int16	debug_03;		/* for future debugging */
	int16	debug_04;		/* for future debugging */
	int16	debug_05;		/* for future debugging */
	uint16	debug_06;		/* for future debugging */
	uint16	debug_07;		/* for future debugging */
	uint16	debug_08;		/* for future debugging */
	uint16	debug_09;		/* for future debugging */
	uint32	debug_10;		/* for future debugging */
	int8	obss_pwrest[ACPHY_OBSS_SUBBAND_CNT];	/* OBSS signal power per sub-band in dBm */
} phy_ecounter_log_core_v5_t;

/* For trunk ONLY */
#define PHY_ECOUNTER_LOG_CORE_VER255_SIZE		36u
typedef struct phy_ecounter_log_core_v255 {
	uint16	bad_txbaseidx_cnt;	/* cntr for tx_baseidx=127 in healthcheck */
	uint16	curr_tssival;		/* TxPwrCtrlInit_path[01].TSSIVal */
	uint16	pwridx_init;		/* TxPwrCtrlInit_path[01].pwrIndex_init_path[01] */
	uint16	auxphystats;		/* Indicates the PHY stats for aux slice */
	uint16	phystatsgaininfo;	/* Indicates the gain stats */
	uint16	flexgaininfo_A;		/* Indicates the gain settings */
	uint8	crsmin_pwr_idx;		/* Index to the crsminpower threshold array */
	uint8	baseindxval;		/* TPC Base index */
	int8	crsmin_pwr;		/* Noise level for applied desense */
	int8	noise_level_inst;	/* Instantaneous noise cal pwr */
	int8	tgt_pwr;		/* Programmed Target power */
	int8	estpwradj;		/* Current Est Power Adjust value */
	int8	ed_threshold;		/* Energy detection threshold */
	uint8	debug_01;		/* for future debugging */
	uint16	debug_02;		/* for future debugging */
	uint16	debug_03;		/* for future debugging */
	uint32	debug_04;		/* for future debugging */
	int8	obss_pwrest[ACPHY_OBSS_SUBBAND_CNT];	/* OBSS signal power per sub-band in dBm */
} phy_ecounter_log_core_v255_t;

/* Do not remove phy_ecounter_v1_t parameters */
typedef struct phy_ecounter_v2 {
	chanspec_t	chanspec;
	uint8		slice;
	uint8		PAD1;			/* padding */
	uint16		phy_wdg;		/* Count of times watchdog happened. */
	uint16		noise_req;		/* Count of phy noise sample requests. */
	uint16		noise_crsbit;		/* Count of CRS high during noisecal request. */
	uint16		noise_apply;		/* Count of applying noisecal result to crsmin. */
	uint16		cal_counter;		/* Count of performing single&multi phase cal. */
	uint16		featureflag;		/* Currently active feature flags */
	uint32		chan_switch_cnt;	/* channel switch count */
	int8		chiptemp;		/* Chip temparature */
	int8		femtemp;		/* Fem temparature */
	uint8		rxchain;		/* Rx Chain */
	uint8		txchain;		/* Tx Chain */
	uint8		ofdm_desense;		/* OFDM desense */
	uint8		bphy_desense;		/* BPHY desense */
	uint16		deaf_count;		/* Depth of stay_in_carrier_search function */
	uint8		phylog_noise_mode;	/* noise cal mode */
	uint8		total_desense_on;	/* total desense on flag */
	uint8		initgain_desense;	/* initgain desense when total desense is on */
	uint8		crsmin_init;		/* crsmin_init threshold when total desense is on */
	uint8		lte_ofdm_desense;	/* ofdm desense dut to lte */
	uint8		lte_bphy_desense;	/* ofdm desense dut to bphy */
	int8		crsmin_high;		/* crsmin high when applying desense */
	int8		weakest_rssi;		/* weakest link RSSI */
	int8		ed_threshold;		/* Threshold applied for ED */
	uint8		PAD2;			/* padding */
	uint16		ed_crs_status;		/* Status of ED and CRS during noise cal */
	uint16		preempt_status1;	/* status of preemption */
	uint16		preempt_status2;	/* status of preemption */
	uint16		preempt_status3;	/* status of preemption */
	uint16		preempt_status4;	/* status of preemption */
	uint32		cca_stats_total_glitch;	/* ccastats: count of total glitches */
	uint32		cca_stats_bphy_glitch;	/* ccastats: count of bphy glitches */
	uint32		cca_stats_total_badplcp; /* ccastats: count of total badplcp */
	uint32		cca_stats_bphy_badplcp;	/* ccastats: count of bphy badplcp */
	uint32		cca_stats_mbsstime;	/* ccastats: monitor duration in msec */
	uint32		cca_stats_ed_duration;	/* ccastats: ed_duration */
	phy_ecounter_log_core_v2_t phy_ecounter_core[2];
} phy_ecounter_v2_t;

/* Do not remove phy_ecounter_v1_t parameters */
typedef struct phy_ecounter_v3 {
	chanspec_t	chanspec;
	uint16		phy_wdg;	/* Count of times watchdog happened. */
	uint16		noise_req;	/* Count of phy noise sample requests. */
	uint16		noise_crsbit;	/* Count of CRS high during noisecal request. */
	uint16		noise_apply;	/* Count of applying noisecal result to crsmin. */
	uint16		cal_counter;	/* Count of performing single and multi phase cal. */
	uint8		crsmin_pwr_idx;	/* Index to the crsminpower threshold array */
	uint8		slice;		/* Slice # 0 - MAIN, 1 - AUX, 2 - SCAN */
	uint8		rxchain;		/* Status of active RX chains */
	uint8		txchain;		/* Status of active TX chains */
	uint8		gbd_bphy_sleep_counter;	/* Sleep count for bphy GBD */
	uint8		gbd_ofdm_sleep_counter;	/* Sleep count for ofdm GBD */
	uint8		curr_home_channel;	/* Current home channel */
	uint8		gbd_ofdm_desense;	/* Glitch based desense level for ofdm reception */
	uint8		gbd_bphy_desense;	/* Glitch based desense level for bphy reception */
	int8		chiptemp;		/* Chip temperature */
	int8		femtemp;		/* Fem temperature */
	int8		btcx_mode;		/* BT coex desense mode */
	int8		ltecx_mode;		/* LTE coex desense mode */
	int8		weakest_rssi;		/* Weakest link RSSI */
	int8		ed_threshold;		/* Threshold applied for ED */
	uint8		chan_switch_cnt;	/* Count to track channel change */
	uint8		phycal_disable;		/* Status of phy calibration */
	uint8		scca_txstall_precondition;	/* SmartCCA TX stall precondition */
	uint16		featureflag;		/* Currently active feature flags */
	uint16		deaf_count;		/* Count for RX stay in carrier search state */
	uint16		noise_mmt_overdue;	/* Noise measurement overdue status */
	uint16		crsmin_pwr_apply_cnt;	/* Count for desense updates */
	uint16		ed_crs_status;		/* Status of ED and CRS during noise cal */
	uint16		preempt_status1;	/* status of preemption */
	uint16		preempt_status2;	/* status of preemption */
	uint16		preempt_status3;	/* status of preemption */
	uint16		preempt_status4;	/* status of preemption */
	uint32		cca_stats_total_glitch;	/* ccastats: count of total glitches */
	uint32		cca_stats_bphy_glitch;	/* ccastats: count of bphy glitches */
	uint32		cca_stats_total_badplcp; /* ccastats: count of total badplcp */
	uint32		cca_stats_bphy_badplcp;	/* ccastats: count of bphy badplcp */
	uint32		cca_stats_mbsstime;	/* ccastats: monitor duration in msec */
	uint32		cca_stats_ed_duration;	/* ccastats: ed_duration */
	phy_ecounter_log_core_v1_t phy_ecounter_core[2];
} phy_ecounter_v3_t;

/* Do not remove phy_ecounter_v1_t parameters */
typedef struct phy_ecounter_v4 {
	chanspec_t	chanspec;
	uint16		phy_wdg;	/* Count of times watchdog happened. */
	uint16		noise_req;	/* Count of phy noise sample requests. */
	uint16		noise_crsbit;	/* Count of CRS high during noisecal request. */
	uint16		noise_apply;	/* Count of applying noisecal result to crsmin. */
	uint16		cal_counter;	/* Count of performing single and multi phase cal. */
	uint8		slice;		/* Slice # 0 - MAIN, 1 - AUX, 2 - SCAN */
	uint8		rxchain;		/* Status of active RX chains */
	uint8		txchain;		/* Status of active TX chains */
	uint8		gbd_bphy_sleep_counter;	/* Sleep count for bphy GBD */
	uint8		gbd_ofdm_sleep_counter;	/* Sleep count for ofdm GBD */
	uint8		curr_home_channel;	/* Current home channel */
	uint8		gbd_ofdm_desense;	/* Glitch based desense level for ofdm reception */
	uint8		gbd_bphy_desense;	/* Glitch based desense level for bphy reception */
	int8		chiptemp;		/* Chip temperature */
	int8		femtemp;		/* Fem temperature */
	int8		weakest_rssi;		/* Weakest link RSSI */
	int8		ltecx_mode;		/* LTE coex desense mode */
	int32		btcx_mode;		/* BT coex desense mode */
	int8		ed_threshold;		/* Threshold applied for ED */
	uint8		chan_switch_cnt;	/* Count to track channel change */
	uint8		phycal_disable;		/* Status of phy calibration */
	uint8		scca_txstall_precondition;	/* SmartCCA TX stall precondition */
	uint16		featureflag;		/* Currently active feature flags */
	uint16		deaf_count;		/* Count for RX stay in carrier search state */
	uint16		noise_mmt_overdue;	/* Noise measurement overdue status */
	uint16		crsmin_pwr_apply_cnt;	/* Count for desense updates */
	uint16		ed_crs_status;		/* Status of ED and CRS during noise cal */
	uint16		preempt_status1;	/* status of preemption */
	uint16		preempt_status2;	/* status of preemption */
	uint16		preempt_status3;	/* status of preemption */
	uint16		preempt_status4;	/* status of preemption */
	uint16		counter_noise_iqest_to;	/* count of IQ_Est time out */
	uint32		cca_stats_total_glitch;	/* ccastats: count of total glitches */
	uint32		cca_stats_bphy_glitch;	/* ccastats: count of bphy glitches */
	uint32		cca_stats_total_badplcp; /* ccastats: count of total badplcp */
	uint32		cca_stats_bphy_badplcp;	/* ccastats: count of bphy badplcp */
	uint32		cca_stats_mbsstime;	/* ccastats: monitor duration in msec */
	uint32		cca_stats_ed_duration;	/* ccastats: ed_duration */
	uint32		measurehold;		/* PHY hold activities */
	uint32		rxsense_disable_req_ch;	/* channel disable requests */
	uint32		ocl_disable_reqs;	/* OCL disable bitmap */
	uint32		interference_mode;	/* interference mitigation mode */
	uint32		power_mode;		/* power mode */
	uint32		obss_last_read_time;	/* last stats read time */
	int32		asym_intf_ed_thresh;	/* smartcca ed threshold %d */
	uint16		obss_mit_bw;		/* selected mitigation BW */
	uint16		obss_stats_cnt;		/* stats count */
	uint16		dynbw_init_reducebw_cnt;	/* BW reduction cnt of initiator */
	uint16		dynbw_resp_reducebw_cnt;	/* BW reduction cnt of responder */
	uint16		dynbw_rxdata_reducebw_cnt;	/* rx data cnt with reduced BW */
	uint16		obss_mmt_skip_cnt;	/* mmt skipped due to powersave */
	uint16		obss_mmt_no_result_cnt;	/* mmt with no result */
	uint16		obss_mmt_intr_err_cnt;	/* obss reg mismatch between ucode and fw */
	uint16		gci_lst_inv_ctr;	/* last gci invalid */
	uint16		gci_lst_rst_ctr;	/* last gci restore 0x%04x */
	uint16		gci_lst_sem_ctr;	/* last gci seq number 0x%04x */
	uint16		gci_lst_rb_st;		/* last gci status */
	uint16		gci_dbg01;		/* gci dbg1 readback */
	uint16		gci_dbg02;		/* gci dbg2 readback */
	uint16		gci_dbg03;		/* gci dbg3 readback */
	uint16		gci_dbg04;		/* gci dbg4 readback */
	uint16		gci_dbg05;		/* gci dbg5 readback */
	uint16		gci_lst_st_msk;		/* gci last status mask */
	uint16		gci_inv_tx;		/* invalid gci during tx */
	uint16		gci_inv_rx;		/* invalid gci during rx */
	uint16		gci_rst_tx;		/* gci restore during tx */
	uint16		gci_rst_rx;		/* gci restore during rx */
	uint16		gci_sem_ctr;		/* gci seq number ctr */
	uint16		gci_invstate;		/* gci status 0x%04x */
	uint16		gci_ctl2;		/* gci ctrl 2 */
	uint16		gci_chan;		/* channel during gci read 0x%04x */
	uint16		gci_cm;			/* channel during gci read */
	uint16		gci_sc;			/* gci read during scan */
	uint16		gci_rst_sc;		/* gci restore during scan */
	uint16		gci_prdc_rx;		/* periodic gci hc */
	uint16		gci_wk_rx;		/* gci hc during wake */
	uint16		gci_rmac_rx;		/* gci hc during mac read */
	uint16		gci_tx_rx;		/* gci hc during tx/rx */
	uint16		asym_intf_stats;	/* smartCCA status 0x%04x */
	uint16		asym_intf_ncal_crs_stat;	/* noise cal and crs status %d */
	int16		ed_crsEn;		/* ed enable 0x%04x */
	int16		nvcfg0;			/* noise update to hw 0x%04x */
	uint8		cal_suppressed_cntr_ed;	/* cnt including ss, mp cals, MSB is cur state */
	uint8		sc_dccal_incc_cnt;	/* scan dccal counter */
	uint8		sc_noisecal_incc_cnt;	/* scan noise cal counter */
	uint8		obss_need_updt;		/* BW update needed flag */
	uint8		obss_mit_status;	/* obss mitigation status */
	uint8		obss_final_rec_bw;	/* final recommended bw to wlc-Sent to SW */
	uint8		btc_mode;		/* btc mode */
	uint8		asym_intf_ant_noise_idx;		/* current noise storage index */
	uint8		asym_intf_pending_host_req_type;	/* usb plugin request */
	uint8		asym_intf_ncal_crs_stat_idx;		/* crs status storage index %d */
	int8		rxsense_noise_idx;			/* rxsense det thresh desense idx */
	int8		rxsense_offset;				/* rxsense min power desense idx */
	int8		asym_intf_tx_smartcca_cm;		/* smartCCA tx coremask %d */
	int8		asym_intf_rx_noise_mit_cm;		/* smartCCA rx coremask %d */
	int8		asym_intf_avg_noise[2];			/* average noise %d */
	int8		asym_intf_latest_noise[2];		/* current noise %d */
	uint8		obss_curr_det[ACPHY_OBSS_SUBBAND_CNT];	/* obss curr detection */
	phy_ecounter_log_core_v3_t phy_ecounter_core[2];
} phy_ecounter_v4_t;

typedef struct phy_ecounter_v5 {
	chanspec_t	chanspec;
	uint16		phy_wdg;		/* Count of times watchdog happened */
	uint16		noise_req;		/* Count of phy noise sample requests */
	uint16		noise_crsbit;	/* Count of CRS high during noisecal request */
	uint16		noise_apply;	/* Count of applying noisecal result to crsmin */
	uint16		cal_counter;	/* Count of performing single and multi phase cal */
	uint8		slice;			/* Slice # 0 - MAIN, 1 - AUX, 2 - SCAN */
	uint8		rxchain;		/* Status of active RX chains */
	uint8		txchain;		/* Status of active TX chains */
	uint8		gbd_bphy_sleep_counter;	/* Sleep count for bphy GBD */
	uint8		gbd_ofdm_sleep_counter;	/* Sleep count for ofdm GBD */
	uint8		btc_mode;		/* btc mode */
	uint8		gbd_ofdm_desense;	/* Glitch based desense level for ofdm reception */
	uint8		gbd_bphy_desense;	/* Glitch based desense level for bphy reception */
	int8		chiptemp;		/* Chip temperature */
	int8		femtemp;		/* Fem temperature */
	int8		weakest_rssi;		/* Weakest link RSSI */
	int8		ltecx_mode;		/* LTE coex desense mode */
	int32		btcx_mode;		/* BT coex desense mode */
	uint8		chan_switch_cnt;	/* Count to track channel change */
	uint8		phycal_disable;		/* Status of phy calibration */
	int8		rxsense_noise_idx;	/* rxsense det thresh desense idx */
	int8		rxsense_offset;		/* rxsense min power desense idx */
	uint16		featureflag;		/* Currently active feature flags */
	uint16		deaf_count;		/* Count for RX stay in carrier search state */
	uint16		noise_mmt_overdue;	/* Noise measurement overdue status */
	uint16		crsmin_pwr_apply_cnt;	/* Count for desense updates */
	uint16		ed_crs_status;		/* Status of ED and CRS during noise cal */
	uint16		preempt_status1;	/* status of preemption */
	uint16		preempt_status2;	/* status of preemption */
	uint16		preempt_status3;	/* status of preemption */
	uint16		preempt_status4;	/* status of preemption */
	uint16		counter_noise_iqest_to;	/* count of IQ_Est time out */
	uint32		cca_stats_total_glitch;	/* ccastats: count of total glitches */
	uint32		cca_stats_bphy_glitch;	/* ccastats: count of bphy glitches */
	uint32		cca_stats_total_badplcp; /* ccastats: count of total badplcp */
	uint32		cca_stats_bphy_badplcp;	/* ccastats: count of bphy badplcp */
	uint32		cca_stats_mbsstime;	/* ccastats: monitor duration in msec */
	uint32		cca_stats_ed_duration;	/* ccastats: ed_duration */
	uint32		measurehold;		/* PHY hold activities */
	uint32		rxsense_disable_req_ch;	/* channel disable requests */
	uint32		ocl_disable_reqs;	/* OCL disable bitmap */
	uint32		interference_mode;	/* interference mitigation mode */
	uint32		power_mode;		/* power mode */
	uint32		obss_last_read_time;	/* last stats read time */
	int32		asym_intf_ed_thresh;	/* smartcca ed threshold %d */
	uint16		obss_mit_bw;		/* selected mitigation BW */
	uint16		obss_stats_cnt;		/* stats count */
	uint16		dynbw_init_reducebw_cnt;	/* BW reduction cnt of initiator */
	uint16		dynbw_resp_reducebw_cnt;	/* BW reduction cnt of responder */
	uint16		dynbw_rxdata_reducebw_cnt;	/* rx data cnt with reduced BW */
	uint16		obss_mmt_skip_cnt;	/* mmt skipped due to powersave */
	uint16		obss_mmt_no_result_cnt;	/* mmt with no result */
	uint16		obss_mmt_intr_err_cnt;	/* obss reg mismatch between ucode and fw */
	uint16		gci_lst_inv_ctr;	/* last gci invalid */
	uint16		gci_lst_rst_ctr;	/* last gci restore 0x%04x */
	uint16		gci_lst_sem_ctr;	/* last gci seq number 0x%04x */
	uint16		gci_lst_rb_st;		/* last gci status */
	uint16		gci_dbg01;		/* gci dbg1 readback */
	uint16		gci_dbg02;		/* gci dbg2 readback */
	uint16		gci_dbg03;		/* gci dbg3 readback */
	uint16		gci_dbg04;		/* gci dbg4 readback */
	uint16		gci_dbg05;		/* gci dbg5 readback */
	uint16		gci_lst_st_msk;		/* gci last status mask */
	uint16		gci_inv_tx;		/* invalid gci during tx */
	uint16		gci_inv_rx;		/* invalid gci during rx */
	uint16		gci_rst_tx;		/* gci restore during tx */
	uint16		gci_rst_rx;		/* gci restore during rx */
	uint16		gci_sem_ctr;		/* gci seq number ctr */
	uint16		gci_invstate;		/* gci status 0x%04x */
	uint16		gci_ctl2;		/* gci ctrl 2 */
	uint16		gci_chan;		/* channel during gci read 0x%04x */
	uint16		gci_cm;			/* channel during gci read */
	uint16		gci_sc;			/* gci read during scan */
	uint16		gci_rst_sc;		/* gci restore during scan */
	uint16		gci_prdc_rx;		/* periodic gci hc */
	uint16		gci_wk_rx;		/* gci hc during wake */
	uint16		gci_rmac_rx;		/* gci hc during mac read */
	uint16		gci_tx_rx;		/* gci hc during tx/rx */
	uint16		asym_intf_stats;	/* smartCCA status 0x%04x */
	uint16		asym_intf_ncal_crs_stat;	/* noise cal and crs status %d */
	int16		ed_crsEn;		/* ed enable 0x%04x */
	int16		nvcfg0;			/* noise update to hw 0x%04x */
	uint8		cal_suppressed_cntr_ed;	/* cnt including ss, mp cals, MSB is cur state */
	uint8		sc_dccal_incc_cnt;	/* scan dccal counter */
	uint8		sc_noisecal_incc_cnt;	/* scan noise cal counter */
	uint8		obss_need_updt;		/* BW update needed flag */
	uint8		obss_mit_status;	/* obss mitigation status */
	uint8		obss_last_rec_bw;	/* last recommended bw to wlc-Sent to SW */
	uint8		asym_intf_ant_noise_idx;		/* current noise storage index */
	uint8		asym_intf_pending_host_req_type;	/* usb plugin request */
	uint8		asym_intf_ncal_crs_stat_idx;		/* crs status storage index %d */
	int8		asym_intf_tx_smartcca_cm;		/* smartCCA tx coremask %d */
	int8		asym_intf_rx_noise_mit_cm;		/* smartCCA rx coremask %d */
	int8		asym_intf_avg_noise[2];			/* average noise %d */
	int8		asym_intf_latest_noise[2];		/* current noise %d */
	uint8		obss_curr_det[ACPHY_OBSS_SUBBAND_CNT];	/* obss curr detection */
	uint8		debug_01;		/* padding */
	uint8		debug_02;		/* padding */
	uint8		debug_03;		/* padding */
	phy_ecounter_log_core_v4_t phy_ecounter_core[2];
} phy_ecounter_v5_t;

typedef struct phy_ecounter_v6 {
	chanspec_t	chanspec;
	uint16		phy_wdg;		/* Count of times watchdog happened */
	uint16		noise_req;		/* Count of phy noise sample requests */
	uint16		noise_crsbit;	/* Count of CRS high during noisecal request */
	uint16		noise_apply;	/* Count of applying noisecal result to crsmin */
	uint16		cal_counter;	/* Count of performing single and multi phase cal */
	uint8		slice;			/* Slice # 0 - MAIN, 1 - AUX, 2 - SCAN */
	uint8		rxchain;		/* Status of active RX chains */
	uint8		txchain;		/* Status of active TX chains */
	uint8		gbd_bphy_sleep_counter;	/* Sleep count for bphy GBD */
	uint8		gbd_ofdm_sleep_counter;	/* Sleep count for ofdm GBD */
	uint8		btc_mode;		/* btc mode */
	uint8		gbd_ofdm_desense;	/* Glitch based desense level for ofdm reception */
	uint8		gbd_bphy_desense;	/* Glitch based desense level for bphy reception */
	int8		chiptemp;		/* Chip temperature */
	int8		femtemp;		/* Fem temperature */
	int8		weakest_rssi;		/* Weakest link RSSI */
	int8		ltecx_mode;		/* LTE coex desense mode */
	int32		btcx_mode;		/* BT coex desense mode */
	uint8		chan_switch_cnt;	/* Count to track channel change */
	uint8		phycal_disable;		/* Status of phy calibration */
	int8		rxsense_noise_idx;	/* rxsense det thresh desense idx */
	int8		rxsense_offset;		/* rxsense min power desense idx */
	uint16		featureflag;		/* Currently active feature flags */
	uint16		deaf_count;		/* Count for RX stay in carrier search state */
	uint16		noise_mmt_overdue;	/* Noise measurement overdue status */
	uint16		crsmin_pwr_apply_cnt;	/* Count for desense updates */
	uint16		ed_crs_status;		/* Status of ED and CRS during noise cal */
	uint16		preempt_status1;	/* status of preemption */
	uint16		preempt_status2;	/* status of preemption */
	uint16		preempt_status3;	/* status of preemption */
	uint16		preempt_status4;	/* status of preemption */
	uint16		counter_noise_iqest_to;	/* count of IQ_Est time out */
	uint32		cca_stats_total_glitch;	/* ccastats: count of total glitches */
	uint32		cca_stats_bphy_glitch;	/* ccastats: count of bphy glitches */
	uint32		cca_stats_total_badplcp; /* ccastats: count of total badplcp */
	uint32		cca_stats_bphy_badplcp;	/* ccastats: count of bphy badplcp */
	uint32		cca_stats_mbsstime;	/* ccastats: monitor duration in msec */
	uint32		cca_stats_ed_duration;	/* ccastats: ed_duration */
	uint32		measurehold;		/* PHY hold activities */
	uint32		rxsense_disable_req_ch;	/* channel disable requests */
	uint32		ocl_disable_reqs;	/* OCL disable bitmap */
	uint32		interference_mode;	/* interference mitigation mode */
	uint32		power_mode;		/* power mode */
	uint32		obss_last_read_time;	/* last stats read time */
	int32		asym_intf_ed_thresh;	/* smartcca ed threshold %d */
	uint16		obss_mit_bw;		/* selected mitigation BW */
	uint16		obss_stats_cnt;		/* stats count */
	uint16		dynbw_init_reducebw_cnt;	/* BW reduction cnt of initiator */
	uint16		dynbw_resp_reducebw_cnt;	/* BW reduction cnt of responder */
	uint16		dynbw_rxdata_reducebw_cnt;	/* rx data cnt with reduced BW */
	uint16		obss_mmt_skip_cnt;	/* mmt skipped due to powersave */
	uint16		obss_mmt_no_result_cnt;	/* mmt with no result */
	uint16		obss_mmt_intr_err_cnt;	/* obss reg mismatch between ucode and fw */
	uint16		gci_lst_inv_ctr;	/* last gci invalid */
	uint16		gci_lst_rst_ctr;	/* last gci restore 0x%04x */
	uint16		gci_lst_sem_ctr;	/* last gci seq number 0x%04x */
	uint16		gci_lst_rb_st;		/* last gci status */
	uint16		gci_dbg01;		/* gci dbg1 readback */
	uint16		gci_dbg02;		/* gci dbg2 readback */
	uint16		gci_dbg03;		/* gci dbg3 readback */
	uint16		gci_dbg04;		/* gci dbg4 readback */
	uint16		gci_dbg05;		/* gci dbg5 readback */
	uint16		gci_lst_st_msk;		/* gci last status mask */
	uint16		gci_inv_tx;		/* invalid gci during tx */
	uint16		gci_inv_rx;		/* invalid gci during rx */
	uint16		gci_rst_tx;		/* gci restore during tx */
	uint16		gci_rst_rx;		/* gci restore during rx */
	uint16		gci_sem_ctr;		/* gci seq number ctr */
	uint16		gci_invstate;		/* gci status 0x%04x */
	uint16		gci_ctl2;		/* gci ctrl 2 */
	uint16		gci_chan;		/* channel during gci read 0x%04x */
	uint16		gci_cm;			/* channel during gci read */
	uint16		gci_sc;			/* gci read during scan */
	uint16		gci_rst_sc;		/* gci restore during scan */
	uint16		gci_prdc_rx;		/* periodic gci hc */
	uint16		gci_wk_rx;		/* gci hc during wake */
	uint16		gci_rmac_rx;		/* gci hc during mac read */
	uint16		gci_tx_rx;		/* gci hc during tx/rx */
	uint16		asym_intf_stats;	/* smartCCA status 0x%04x */
	uint16		asym_intf_ncal_crs_stat;	/* noise cal and crs status %d */
	int16		ed_crsEn;		/* ed enable 0x%04x */
	int16		nvcfg0;			/* noise update to hw 0x%04x */
	uint8		cal_suppressed_cntr_ed;	/* cnt including ss, mp cals, MSB is cur state */
	uint8		sc_dccal_incc_cnt;	/* scan dccal counter */
	uint8		sc_noisecal_incc_cnt;	/* scan noise cal counter */
	uint8		obss_need_updt;		/* BW update needed flag */
	uint8		obss_mit_status;	/* obss mitigation status */
	uint8		obss_last_rec_bw;	/* last recommended bw to wlc-Sent to SW */
	uint8		asym_intf_ant_noise_idx;		/* current noise storage index */
	uint8		asym_intf_pending_host_req_type;	/* usb plugin request */
	uint8		asym_intf_ncal_crs_stat_idx;		/* crs status storage index %d */
	int8		asym_intf_tx_smartcca_cm;		/* smartCCA tx coremask %d */
	int8		asym_intf_rx_noise_mit_cm;		/* smartCCA rx coremask %d */
	int8		asym_intf_avg_noise[2];			/* average noise %d */
	int8		asym_intf_latest_noise[2];		/* current noise %d */
	uint8		obss_curr_det[ACPHY_OBSS_SUBBAND_CNT];	/* obss curr detection */
	uint8		debug_01;		/* padding */
	uint8		debug_02;		/* padding */
	uint8		debug_03;		/* padding */
	uint32		duration;
	uint32		congest_meonly;
	uint32		congest_ibss;
	uint32		congest_obss;
	uint32		interference;
	phy_ecounter_log_core_v4_t phy_ecounter_core[2];
} phy_ecounter_v6_t;

#define PHY_ECOUNTER_VER7_SIZE	344u
typedef struct phy_ecounter_v7 {
	chanspec_t	chanspec;
	uint16		phy_wdg;		/* Count of times watchdog happened */
	uint16		noise_req;		/* Count of phy noise sample requests */
	uint16		noise_crsbit;		/* Count of CRS high during noisecal request */
	uint16		noise_apply;		/* Count of applying noisecal result to crsmin */
	uint16		cal_counter;		/* Count of performing single and multi phase cal */
	uint8		slice;			/* Slice # 0 - MAIN, 1 - AUX, 2 - SCAN */
	uint8		rxchain;		/* Status of active RX chains */
	uint8		txchain;		/* Status of active TX chains */
	uint8		gbd_bphy_sleep_counter;	/* Sleep count for bphy GBD */
	uint8		gbd_ofdm_sleep_counter;	/* Sleep count for ofdm GBD */
	uint8		btc_mode;		/* btc mode */
	uint8		gbd_ofdm_desense;	/* Glitch based desense level for ofdm reception */
	uint8		gbd_bphy_desense;	/* Glitch based desense level for bphy reception */
	int8		chiptemp;		/* Chip temperature */
	int8		femtemp;		/* Fem temperature */
	int8		weakest_rssi;		/* Weakest link RSSI */
	int8		ltecx_mode;		/* LTE coex desense mode */
	int32		btcx_mode;		/* BT coex desense mode */
	uint8		chan_switch_cnt;	/* Count to track channel change */
	uint8		phycal_disable;		/* Status of phy calibration */
	int8		rxsense_noise_idx;	/* rxsense det thresh desense idx */
	int8		rxsense_offset;		/* rxsense min power desense idx */
	uint32		rxsense_disable_req_ch;	/* channel disable requests */
	uint16		featureflag;		/* Currently active feature flags */
	uint16		deaf_count;		/* Count for RX stay in carrier search state */
	uint16		noise_mmt_overdue;	/* Noise measurement overdue status */
	uint16		counter_noise_iqest_to;	/* count of IQ_Est time out */
	uint16		crsmin_pwr_apply_cnt;	/* Count for desense updates */
	uint16		ed_crs_status;		/* Status of ED and CRS during noise cal */
	uint16		preempt_status2;	/* status of preemption */
	uint16		debug_01;		/* for future debugging */
	uint32		cca_stats_total_glitch;	/* ccastats: count of total glitches */
	uint32		cca_stats_bphy_glitch;	/* ccastats: count of bphy glitches */
	uint32		cca_stats_total_badplcp;	/* ccastats: count of total badplcp */
	uint32		cca_stats_bphy_badplcp;	/* ccastats: count of bphy badplcp */
	uint32		cca_stats_mbsstime;	/* ccastats: monitor duration in msec */
	uint32		cca_stats_ed_duration;	/* ccastats: ed_duration */
	uint32		measurehold;		/* PHY hold activities */
	uint32		ocl_disable_reqs;	/* OCL disable bitmap */
	uint32		interference_mode;	/* interference mitigation mode */
	uint32		power_mode;		/* power mode */
	int32		asym_intf_ed_thresh;	/* smartcca ed threshold %d */
	uint32		obss_last_read_time;	/* last stats read time */
	uint16		obss_mit_bw;		/* selected mitigation BW */
	uint16		obss_stats_cnt;		/* stats count */
	uint16		obss_mmt_skip_cnt;	/* mmt skipped due to powersave */
	uint16		obss_mmt_no_result_cnt;	/* mmt with no result */
	uint16		obss_mmt_intr_err_cnt;	/* obss reg mismatch between ucode and fw */
	uint8		obss_last_rec_bw;	/* last recommended bw to wlc-Sent to SW */
	uint8		obss_cur_det_bitmap;	/* OBSS curr detection bitmap */
	uint8		obss_need_updt;		/* BW update needed flag */
	uint8		obss_mit_status;	/* OBSS mitigation status */
	uint16		dynbw_init_reducebw_cnt;	/* BW reduction cnt of initiator */
	uint16		dynbw_resp_reducebw_cnt;	/* BW reduction cnt of responder */
	uint16		dynbw_rxdata_reducebw_cnt;	/* rx data cnt with reduced BW */
	int16		ed_crsEn;		/* ed enable 0x%04x */
	int16		nvcfg0;			/* noise update to hw 0x%04x */
	uint16		asym_intf_stats;	/* smartCCA status 0x%04x */
	uint16		asym_intf_ncal_crs_stat;	/* noise cal and crs status %d */
	uint8		asym_intf_ant_noise_idx;	/* current noise storage index */
	uint8		asym_intf_pending_host_req_type;	/* usb plugin request */
	uint8		asym_intf_ncal_crs_stat_idx;	/* crs status storage index %d */
	int8		asym_intf_tx_smartcca_cm;	/* smartCCA tx coremask %d */
	int8		asym_intf_rx_noise_mit_cm;	/* smartCCA rx coremask %d */
	int8		asym_intf_avg_noise[2];		/* average noise %d */
	int8		asym_intf_latest_noise[2];	/* current noise %d */
	uint8		cal_suppressed_cntr_ed;	/* cnt including ss, mp cals, MSB is cur state */
	uint8		sc_dccal_incc_cnt;	/* scan dccal counter */
	uint8		sc_noisecal_incc_cnt;	/* scan noise cal counter */
	uint8		dcc_digi_gain;		/* digi gain value used for DC cal */
	uint8		dcc_est_overflow;	/* DC cal estimation overflow indicator */
	uint32		fbcx_info01;		/* Indicates FBCX debug information */
	uint32		fbcx_info02;		/* Indicates FBCX debug information */
	uint32		fbcx_info03;		/* Indicates FBCX debug information */
	uint32		fbcx_info04;		/* Indicates FBCX debug information */
	uint32		fbcx_info05;		/* Indicates FBCX debug information */
	uint32		fbcx_info06;		/* Indicates FBCX debug information */
	uint16		scan_info;		/* Indicates scan information */
	uint16		scan_starts;		/* Indicates frame starts */
	uint16		scan_detect[3];		/* Indicates frame detections */
	uint16		scan_good_fcs[3];	/* Indicates good FCS Rx counter */
	uint16		scan_bad_fcs;		/* Indicates bad FCS Rx counter */
	uint16		scan_busy;		/* Indicates hardware busy */
	uint16		scan_errors;		/* Indicates error counter */
	uint16		debug_02;		/* for future debugging */
	uint32		srmc_debug_01;		/* Indicates radio state info */
	uint16		debug_cal_code_main_slice;	/* Indicates VCO cal code main slice */
	uint16		debug_cal_code_scan_5g_slice[8]; /* Indicates VCO cal code scan 5G radio */
	uint16		PLL_2g_VCOCAL_calCapRB;	/* Indicates VCO cal code */
	int16		phy_cal_debug_01;	/* Indicates DC cal params */
	int16		phy_cal_debug_02;	/* Indicates DC cal params */
	uint16		ml_req_txcnt;		/* Indicates ML notif req Tx count new attempts */
	uint16		ml_req_tot_retry_cnt;	/* Indicates ML notif req total retries */
	uint16		ml_resp_rxcnt;		/* Indicates ML notif resp Rx count */
	uint16		ml_resp_match_rxcnt;	/* Indicates ML notif resp all conds matched */
	uint8		ml_req_retry_cnt;	/* ML notification request retry count */
	uint8		pa_mode;		/* PA mode */
	uint16		debug_03;		/* for future debugging */
	uint16		debug_04;		/* for future debugging */
	uint16		debug_05;		/* for future debugging */
	uint16		debug_06;		/* for future debugging */
	uint16		debug_07;		/* for future debugging */
	uint16		debug_08;		/* for future debugging */
	uint16		debug_09;		/* for future debugging */
	uint16		debug_10;		/* for future debugging */
	phy_ecounter_log_core_v5_t phy_ecounter_core[2];
} phy_ecounter_v7_t;

/* For trunk ONLY */
/* Do not remove phy_ecounter_v1_t parameters */
#define PHY_ECOUNTER_VER255_SIZE	304u
typedef struct phy_ecounter_v255 {
	chanspec_t	chanspec;
	uint16		phy_wdg;		/* Count of times watchdog happened. */
	uint16		noise_req;		/* Count of phy noise sample requests. */
	uint16		noise_crsbit;	/* Count of CRS high during noisecal request. */
	uint16		noise_apply;	/* Count of applying noisecal result to crsmin. */
	uint16		cal_counter;	/* Count of performing single and multi phase cal. */
	uint8		slice;			/* Slice # 0 - MAIN, 1 - AUX, 2 - SCAN */
	uint8		rxchain;		/* Status of active RX chains */
	uint8		txchain;		/* Status of active TX chains */
	uint8		gbd_bphy_sleep_counter;	/* Sleep count for bphy GBD */
	uint8		gbd_ofdm_sleep_counter;	/* Sleep count for ofdm GBD */
	uint8		btc_mode;		/* btc mode */
	uint8		gbd_ofdm_desense;	/* Glitch based desense level for ofdm reception */
	uint8		gbd_bphy_desense;	/* Glitch based desense level for bphy reception */
	int8		chiptemp;		/* Chip temperature */
	int8		femtemp;		/* Fem temperature */
	int8		weakest_rssi;		/* Weakest link RSSI */
	int8		ltecx_mode;		/* LTE coex desense mode */
	int32		btcx_mode;		/* BT coex desense mode */
	uint8		chan_switch_cnt;	/* Count to track channel change */
	uint8		phycal_disable;		/* Status of phy calibration */
	int8		rxsense_noise_idx;	/* rxsense det thresh desense idx */
	int8		rxsense_offset;		/* rxsense min power desense idx */
	uint32		rxsense_disable_req_ch;	/* channel disable requests */
	uint16		featureflag;		/* Currently active feature flags */
	uint16		deaf_count;		/* Count for RX stay in carrier search state */
	uint16		noise_mmt_overdue;	/* Noise measurement overdue status */
	uint16		crsmin_pwr_apply_cnt;	/* Count for desense updates */
	uint16		ed_crs_status;		/* Status of ED and CRS during noise cal */
	uint16		preempt_status2;	/* status of preemption */
	uint32		cca_stats_total_glitch;	/* ccastats: count of total glitches */
	uint32		cca_stats_bphy_glitch;	/* ccastats: count of bphy glitches */
	uint32		cca_stats_total_badplcp; /* ccastats: count of total badplcp */
	uint32		cca_stats_bphy_badplcp;	/* ccastats: count of bphy badplcp */
	uint32		cca_stats_mbsstime;	/* ccastats: monitor duration in msec */
	uint32		cca_stats_ed_duration;	/* ccastats: ed_duration */
	uint32		measurehold;		/* PHY hold activities */
	uint32		ocl_disable_reqs;	/* OCL disable bitmap */
	uint32		interference_mode;	/* interference mitigation mode */
	uint32		power_mode;		/* power mode */
	int32		asym_intf_ed_thresh;	/* smartcca ed threshold %d */
	uint32		obss_last_read_time;	/* last stats read time */
	uint16		counter_noise_iqest_to;	/* count of IQ_Est time out */
	uint16		obss_mit_bw;		/* selected mitigation BW */
	uint16		obss_stats_cnt;		/* stats count */
	uint16		obss_mmt_skip_cnt;	/* mmt skipped due to powersave */
	uint16		obss_mmt_no_result_cnt;	/* mmt with no result */
	uint16		obss_mmt_intr_err_cnt;	/* obss reg mismatch between ucode and fw */
	uint8		obss_need_updt;		/* BW update needed flag */
	uint8		obss_mit_status;	/* obss mitigation status */
	uint8		obss_last_rec_bw;	/* last recommended bw to wlc-Sent to SW */
	uint8		obss_cur_det_bitmap;	/* obss curr detection bitmap */
	uint16		dynbw_init_reducebw_cnt;	/* BW reduction cnt of initiator */
	uint16		dynbw_resp_reducebw_cnt;	/* BW reduction cnt of responder */
	uint16		dynbw_rxdata_reducebw_cnt;	/* rx data cnt with reduced BW */
	int16		ed_crsEn;		/* ed enable 0x%04x */
	int16		nvcfg0;			/* noise update to hw 0x%04x */
	uint16		asym_intf_stats;	/* smartCCA status 0x%04x */
	uint16		asym_intf_ncal_crs_stat;	/* noise cal and crs status %d */
	uint8		asym_intf_ant_noise_idx;		/* current noise storage index */
	uint8		asym_intf_pending_host_req_type;	/* usb plugin request */
	uint8		asym_intf_ncal_crs_stat_idx;		/* crs status storage index %d */
	int8		asym_intf_tx_smartcca_cm;		/* smartCCA tx coremask %d */
	int8		asym_intf_rx_noise_mit_cm;		/* smartCCA rx coremask %d */
	int8		asym_intf_avg_noise[2];			/* average noise %d */
	int8		asym_intf_latest_noise[2];		/* current noise %d */
	uint8		cal_suppressed_cntr_ed;	/* cnt including ss, mp cals, MSB is cur state */
	uint8		sc_dccal_incc_cnt;	/* scan dccal counter */
	uint8		sc_noisecal_incc_cnt;	/* scan noise cal counter */
	uint8		dcc_digi_gain;		/* digi gain value used for DC cal */
	uint8		dcc_est_overflow;	/* DC cal estimation overflow indicator */
	uint32		fbcx_info01;		/* Indicates FBCX debug information */
	uint32		fbcx_info02;		/* Indicates FBCX debug information */
	uint32		fbcx_info03;		/* Indicates FBCX debug information */
	uint32		fbcx_info04;		/* Indicates FBCX debug information */
	uint32		fbcx_info05;		/* Indicates FBCX debug information */
	uint32		fbcx_info06;		/* Indicates FBCX debug information */
	uint16		scan_info;		/* Indicates scan information */
	uint16		scan_starts;		/* Indicates frame starts */
	uint16		scan_detect[4];		/* Indicates frame detections */
	uint16		scan_good_fcs[4];	/* Indicates good FCS Rx counter */
	uint16		scan_bad_fcs;		/* Indicates bad FCS Rx counter */
	uint16		scan_busy;		/* Indicates hardware busy */
	uint16		scan_errors;		/* Indicates error counter */
	uint16		PLL_2g_VCOCAL_calCapRB;	/* Indicates VCO cal code */
	uint32		srmc_debug_01;		/* Indicates radio state info */
	uint16		debug_cal_code_scan_5g_slice[8]; /* Indicates VCO cal code scan 5G radio */
	uint16		debug_cal_code_main_slice;	/* Indicates VCO cal code main slice */
	int16		phy_cal_debug_01;	/* Indicates DC cal params */
	int16		phy_cal_debug_02;	/* Indicates DC cal params */
	uint16		ml_req_txcnt;		/* Indicates ML notif req Tx count new attempts */
	uint16		ml_req_tot_retry_cnt;	/* Indicates ML notif req total retries */
	uint16		ml_resp_rxcnt;		/* Indicates ML notif resp Rx count */
	uint16		ml_resp_match_rxcnt;	/* Indicates ML notif resp all conds matched */
	uint8		ml_req_retry_cnt;	/* ML notification request retry count */
	uint8		pa_mode;		/* PA mode */
	uint8		scan_channel_mask;	/* Indicates scan mask */
	uint8		debug_02;		/* for future debugging */
	uint16		debug_03;		/* for future debugging */
	phy_ecounter_log_core_v255_t phy_ecounter_core[2];
} phy_ecounter_v255_t;

typedef struct phy_ecounter_phycal_core_v1 {
	/* RxIQ imbalance coeff */
	int32	rxs;
	int32	rxs_vpoff;
	int32	rxs_ipoff;
	/* OFDM and BPHY TxIQ imbalance coeff */
	uint16	ofdm_txa;
	uint16	ofdm_txb;
	uint16	ofdm_txd; /* contain di & dq */
	uint16	bphy_txa;
	uint16	bphy_txb;
	uint16	bphy_txd; /* contain di & dq */
	/* the number of times the baseidx is
	 * greater than a certain threshold
	 */
	uint16	txbaseidx_gtthres_cnt;
	/* RxIQ imbalance coeff */
	uint16	rxa;
	uint16	rxb;
	uint8	PAD2;
	uint8	PAD3;
	/* Rx IQ Cal coeff */
	uint16	rxa_vpoff;	/* not present in 4378 */
	uint16	rxb_vpoff;	/* not present in 4378 */
	uint16	rxa_ipoff;	/* not present in 4378 */
	uint16	rxb_ipoff;	/* not present in 4378 */
	/* Tx IQ/LO calibration coeffs */
	uint16	txiqlo_2g_a0;
	uint16	txiqlo_2g_b0;
	uint16	txiqlo_2g_a1;
	uint16	txiqlo_2g_b1;
	uint16	txiqlo_2g_a2;
	uint16	txiqlo_2g_b2;
	/* tx baseindex */
	uint8	baseidx;
	uint8	baseidx_cck;
	/* adc cap cal */
	uint8	adc_coeff_cap0_adcI;
	uint8	adc_coeff_cap1_adcI;
	uint8	adc_coeff_cap2_adcI;
	uint8	adc_coeff_cap0_adcQ;
	uint8	adc_coeff_cap1_adcQ;
	uint8	adc_coeff_cap2_adcQ;
} phy_ecounter_phycal_core_v1_t;

typedef struct phy_phycal_core_v2 {
	/* RxIQ imbalance coeff */
	int32	rxs;

	/* OFDM and BPHY TxIQ imbalance coeff */
	uint16	ofdm_txa;
	uint16	ofdm_txb;
	uint16	ofdm_txd; /* contain di & dq */
	uint16	bphy_txa;
	uint16	bphy_txb;
	uint16	bphy_txd;

	/* RxIQ imbalance coeff */
	uint16	rxa;
	uint16	rxb;

	/* Rx IQ Cal coeff */
	uint16	rxa_vpoff;
	uint16	rxb_vpoff;
	uint16	rxa_ipoff;
	uint16	rxb_ipoff;
	int32	rxs_vpoff;
	int32	rxs_ipoff;
	/* Tx IQ/LO calibration coeffs */
	uint16	txiqlo_2g_a0;
	uint16	txiqlo_2g_b0;
	uint16	txiqlo_2g_a1;
	uint16	txiqlo_2g_b1;
	uint16	txiqlo_2g_a2;
	uint16	txiqlo_2g_b2;
	/* tx baseindex */
	uint8	baseidx;
	uint8	baseidx_cck;
	/* adc cap cal */
	uint8	adc_coeff_cap0_adcI;
	uint8	adc_coeff_cap1_adcI;
	uint8	adc_coeff_cap2_adcI;
	uint8	adc_coeff_cap0_adcQ;
	uint8	adc_coeff_cap1_adcQ;
	uint8	adc_coeff_cap2_adcQ;

	int32	txs;
	int16	txs_mean;
	uint16	txbaseidx_gtthres_cnt; /* cntr for tx_baseidx > hi_thres in healthcheck */
	uint16	txgain_rad_gain;
	uint16	txgain_rad_gain_mi;
	uint16	txgain_rad_gain_hi;
	uint16	txgain_dac_gain;
	uint16	txgain_bbmult;
	int16	rxs_mean_vpoff;
	int16	rxs_mean_ipoff;
	int16	rxs_mean;
	uint8	rxms;
	uint8	rxms_vpoff;
	uint8	rxms_ipoff;
	uint8	ccktxgain_offset;
	int8	mppc_gain_offset_qdB[TXCAL_MAX_PA_MODE];

	/* Misc general purpose debug counters (will be used for future debugging) */
	uint8	debug_01;
	uint8	debug_02;
	uint8	debug_03;
	uint8	debug_04;
	uint16	debug_05;
	uint16	debug_06;
	uint16	debug_07;
	uint16	debug_08;
	uint32	debug_09;
	uint32	debug_10;
	uint32	debug_11;
	uint32	debug_12;
} phy_phycal_core_v2_t;

#define PHY_PHYCAL_CORE_VER3_SIZE		224u
typedef struct phy_phycal_core_v3 {
	/* RxIQ imbalance coeff */
	uint16	rxa;
	uint16	rxb;
	int32	rxs;

	/* OFDM and BPHY TxIQ imbalance coeff */
	uint16	ofdm_txd; /* contain di & dq */

	/* tx baseindex */
	uint8	baseidx;
	uint8	baseidx_cck;

	/* Rx IQ Cal coeff */
	uint16	rxa_vpoff;
	uint16	rxb_vpoff;
	uint16	rxa_ipoff;
	uint16	rxb_ipoff;
	int32	rxs_vpoff;
	int32	rxs_ipoff;

	/* Tx IQ/LO calibration coeffs */
	uint16	txiqlo_a0;
	uint16	txiqlo_b0;
	uint16	txiqlo_a1;
	uint16	txiqlo_b1;
	uint16	txiqlo_a2;
	uint16	txiqlo_b2;

	/* Misc. calibration params */
	int32	txs;
	int16	txs_mean;
	uint16	txbaseidx_gtthres_cnt;
	uint16	txgain_rad_gain;
	uint16	txgain_rad_gain_mi;
	uint16	txgain_rad_gain_hi;
	uint16	txgain_bbmult;
	int16	rxs_mean_vpoff;
	int16	rxs_mean_ipoff;
	int16	rxs_mean;
	uint8	rxms;
	uint8	rxms_vpoff;
	uint8	rxms_ipoff;
	uint8	ccktxgain_offset;
	int8	mppc_gain_offset_qdB[TXCAL_MAX_PA_MODE];

	int16	dc_est_i;		/* Residual DC Estimate */
	int16	dc_est_q;		/* Residual DC Estimate */
	int16	kappa_theta[PHY_RX_GAIN_INDICES][2u];	/* RX-IQ comp coefficients */
	int16	dc_re_im[PHY_RX_GAIN_INDICES][2u];	/* DC compensation coefficients */
	int16	txgaincal[3];		/* txgaincal correction factor */

	/* Misc general purpose debug counters (will be used for future debugging) */
	uint16	debug_01;
	uint16	debug_02;
	uint16	debug_03;
	uint16	debug_04;
	uint32	debug_05;
	uint32	debug_06;
} phy_phycal_core_v3_t;

/* For 27.10 */
typedef struct phy_phycal_core_v4 {
	/* RxIQ imbalance coeff */
	int32	rxs;

	/* OFDM and BPHY TxIQ imbalance coeff */
	uint16	ofdm_txa;
	uint16	ofdm_txb;
	uint16	ofdm_txd; /* contain di & dq */
	uint16	bphy_txa;
	uint16	bphy_txb;
	uint16	bphy_txd;

	/* RxIQ imbalance coeff */
	uint16	rxa;
	uint16	rxb;

	/* Rx IQ Cal coeff */
	uint16	rxa_vpoff;
	uint16	rxb_vpoff;
	uint16	rxa_ipoff;
	uint16	rxb_ipoff;
	int32	rxs_vpoff;
	int32	rxs_ipoff;
	/* Tx IQ/LO calibration coeffs */
	uint16	txiqlo_2g_a0;
	uint16	txiqlo_2g_b0;
	uint16	txiqlo_2g_a1;
	uint16	txiqlo_2g_b1;
	uint16	txiqlo_2g_a2;
	uint16	txiqlo_2g_b2;
	/* tx baseindex */
	uint8	baseidx;
	uint8	baseidx_cck;
	/* adc cap cal */
	uint8	adc_coeff_cap0_adcI;
	uint8	adc_coeff_cap1_adcI;
	uint8	adc_coeff_cap2_adcI;
	uint8	adc_coeff_cap0_adcQ;
	uint8	adc_coeff_cap1_adcQ;
	uint8	adc_coeff_cap2_adcQ;

	int32	txs;
	int16	txs_mean;
	uint16	txbaseidx_gtthres_cnt; /* cntr for tx_baseidx > hi_thres in healthcheck */
	uint16	txgain_rad_gain;
	uint16	txgain_rad_gain_mi;
	uint16	txgain_rad_gain_hi;
	uint16	txgain_dac_gain;
	uint16	txgain_bbmult;
	int16	rxs_mean_vpoff;
	int16	rxs_mean_ipoff;
	int16	rxs_mean;
	uint8	rxms;
	uint8	rxms_vpoff;
	uint8	rxms_ipoff;
	uint8	ccktxgain_offset;
	int8	mppc_gain_offset_qdB[TXCAL_MAX_PA_MODE];

	/* Misc general purpose debug counters (will be used for future debugging) */
	uint8	debug_01;
	uint8	debug_02;
	uint8	debug_03;
	uint8	debug_04;
	uint16	debug_05;
	uint16	debug_06;
	uint16	debug_07;
	uint16	debug_08;
	uint32	debug_09;
	uint32	debug_10;
	uint32	debug_11;
	uint32	debug_12;
} phy_phycal_core_v4_t;

/* For trunk ONLY */
#define PHY_PHYCAL_CORE_VER255_SIZE	228u
typedef struct phy_phycal_core_v255 {
	/* RxIQ imbalance coeff */
	uint16	rxa;
	uint16	rxb;
	int32	rxs;

	/* OFDM and BPHY TxIQ imbalance coeff */
	uint16	ofdm_txd; /* contain di & dq */

	/* tx baseindex */
	uint8	baseidx;
	uint8	baseidx_cck;

	/* Rx IQ Cal coeff */
	uint16	rxa_vpoff;
	uint16	rxb_vpoff;
	uint16	rxa_ipoff;
	uint16	rxb_ipoff;
	int32	rxs_vpoff;
	int32	rxs_ipoff;

	/* Tx IQ/LO calibration coeffs */
	uint16	txiqlo_a0;
	uint16	txiqlo_b0;
	uint16	txiqlo_a1;
	uint16	txiqlo_b1;
	uint16	txiqlo_a2;
	uint16	txiqlo_b2;

	/* Misc. calibration params */
	int32	txs;
	int16	txs_mean;
	uint16	txbaseidx_gtthres_cnt;
	uint16	txgain_rad_gain;
	uint16	txgain_rad_gain_mi;
	uint16	txgain_rad_gain_hi;
	uint16	txgain_bbmult;
	int16	rxs_mean_vpoff;
	int16	rxs_mean_ipoff;
	int16	rxs_mean;
	uint8	rxms;
	uint8	rxms_vpoff;
	uint8	rxms_ipoff;
	uint8	ccktxgain_offset;
	int8	mppc_gain_offset_qdB[TXCAL_MAX_PA_MODE];

	int16	dc_est_i;		/* Residual DC Estimate */
	int16	dc_est_q;		/* Residual DC Estimate */
	int16	kappa_theta[PHY_RX_GAIN_INDICES][2u];	/* RX-IQ comp coefficients */
	int16	dc_re_im[PHY_RX_GAIN_INDICES][2u];	/* DC compensation coefficients */
	int16	txgaincal[PHY_TX_GAIN_CAL];		/* txgaincal correction factor */

	/* Misc general purpose debug counters (will be used for future debugging) */
	uint16	debug_01;
	uint16	debug_02;
	uint16	debug_03;
	uint16	debug_04;
	uint32	debug_05;
	uint32	debug_06;
} phy_phycal_core_v255_t;

typedef struct phy_ecounter_phycal_v1 {
	uint32 last_cal_time; /* in [sec], covers 136 years if 32 bit */
	chanspec_t chanspec;
	int16 last_cal_temp;
	bool txiqlocal_retry;
	bool rxe;
	uint8 cal_phase_id;
	uint8 slice;
	phy_ecounter_phycal_core_v1_t phy_ecounter_phycal_core[2];
} phy_ecounter_phycal_v1_t;

typedef struct phy_phycal_v2 {
	uint32 last_cal_time; /* in [sec], covers 136 years if 32 bit */
	chanspec_t chanspec;
	int16 last_cal_temp;
	bool txiqlocal_retry;
	bool rxe;
	uint8 cal_phase_id;
	uint8 slice;
	uint32 desense_reason;
	uint16 dur;	/* duration of cal in usec */

	uint8 reason;
	uint8 hc_retry_count_vpoff;
	uint8 hc_retry_count_ipoff;
	uint8 hc_retry_count_rx;
	uint8 hc_dev_exceed_log_rx_vpoff;
	uint8 hc_dev_exceed_log_rx_ipoff;
	uint8 hc_dev_exceed_log_rx;
	uint8 sc_rxiqcal_skip_cnt;

	uint8 hc_retry_count_tx;
	uint8 hc_dev_exceed_log_tx;
	uint16 txiqcal_max_retry_cnt;
	uint16 txiqcal_max_slope_cnt;
	uint16 mppc_cal_failed_count;
	uint16 pad01;
	uint16 txiqlocal_coeffs[20];
	bool is_mppc_gain_offset_cal_success;

	/* Misc general purpose debug counters (will be used for future debugging) */
	uint8	debug_01;
	uint8	debug_02;
	uint8	debug_03;
	uint16	debug_04;
	uint16	debug_05;
	uint16	debug_06;
	uint16	debug_07;
	uint32	debug_08;
	uint32	debug_09;
	uint32	debug_10;
	uint32	debug_11;

	phy_phycal_core_v2_t phy_phycal_core[2];
} phy_phycal_v2_t;

#define PHY_PHYCAL_VER3_SIZE	540u
typedef struct phy_phycal_v3 {
	/* General info */
	uint32	last_cal_time; /* in [sec], covers 136 years if 32 bit */
	chanspec_t	chanspec;
	int16	last_cal_temp;
	uint8	txiqlocal_retry;
	uint8	rxe;
	uint8	cal_phase_id;
	uint8	slice;
	uint32	desense_reason;
	uint16	dur;	/* duration of cal in usec */
	uint8	reason;

	/* Health check counters */
	uint8	hc_retry_count_vpoff;
	uint8	hc_retry_count_ipoff;
	uint8	hc_retry_count_rx;
	uint8	hc_retry_count_tx;
	/* Health check exceed conditions */
	uint8	hc_dev_exceed_log_rx_vpoff;
	uint8	hc_dev_exceed_log_rx_ipoff;
	uint8	hc_dev_exceed_log_rx;
	uint8	hc_dev_exceed_log_tx;

	uint8	sc_rxiqcal_skip_cnt;
	/* Tx cal counters */
	uint16	txiqcal_max_retry_cnt;
	uint16	txiqcal_max_slope_cnt;
	uint16	mppc_cal_failed_count;
	uint16	debug_01;
	uint16	txiqlocal_coeffs[20];
	uint8	is_mppc_gain_offset_cal_success;

	/* Misc general purpose debug counters (will be used for future debugging) */
	uint8	debug_02;
	uint8	debug_03;
	uint8	debug_04;
	uint16	debug_05;
	uint16	debug_06;
	uint32	debug_07;
	uint32	debug_08;

	phy_phycal_core_v3_t phy_phycal_core[2];
} phy_phycal_v3_t;

/* For 27.10 */
typedef struct phy_phycal_v4 {
	uint32 last_cal_time; /* in [sec], covers 136 years if 32 bit */
	chanspec_t chanspec;
	int16 last_cal_temp;
	uint8 txiqlocal_retry;
	uint8 rxe;
	uint8 cal_phase_id;
	uint8 slice;
	uint32 desense_reason;
	uint16 dur;	/* duration of cal in usec */

	uint8 reason;
	uint8 hc_retry_count_vpoff;
	uint8 hc_retry_count_ipoff;
	uint8 hc_retry_count_rx;
	uint8 hc_dev_exceed_log_rx_vpoff;
	uint8 hc_dev_exceed_log_rx_ipoff;
	uint8 hc_dev_exceed_log_rx;
	uint8 sc_rxiqcal_skip_cnt;

	uint8 hc_retry_count_tx;
	uint8 hc_dev_exceed_log_tx;
	uint16 txiqcal_max_retry_cnt;
	uint16 txiqcal_max_slope_cnt;
	uint16 mppc_cal_failed_count;
	uint16 pad01;
	uint16 txiqlocal_coeffs[20];
	uint8 is_mppc_gain_offset_cal_success;

	/* Misc general purpose debug counters (will be used for future debugging) */
	uint8	debug_01;
	uint8	debug_02;
	uint8	debug_03;
	uint16	debug_04;
	uint16	debug_05;
	uint16	debug_06;
	uint16	debug_07;
	uint32	debug_08;
	uint32	debug_09;
	uint32	debug_10;
	uint32	debug_11;

	phy_phycal_core_v4_t phy_phycal_core[2];
} phy_phycal_v4_t;

/* For trunk ONLY */
#define PHY_PHYCAL_VER255_SIZE		548u
typedef struct phy_phycal_v255 {
	/* General info */
	uint32 last_cal_time; /* in [sec], covers 136 years if 32 bit */
	chanspec_t chanspec;
	int16 last_cal_temp;
	uint8 txiqlocal_retry;
	uint8 rxe;
	uint8 cal_phase_id;
	uint8 slice;
	uint32 desense_reason;
	uint16 dur;	/* duration of cal in usec */
	uint8 reason;

	/* Health check counters */
	uint8 hc_retry_count_vpoff;
	uint8 hc_retry_count_ipoff;
	uint8 hc_retry_count_rx;
	uint8 hc_retry_count_tx;

	/* Health check exceed conditions */
	uint8 hc_dev_exceed_log_rx_vpoff;
	uint8 hc_dev_exceed_log_rx_ipoff;
	uint8 hc_dev_exceed_log_rx;
	uint8 hc_dev_exceed_log_tx;

	uint8 sc_rxiqcal_skip_cnt;

	/* Tx cal counters */
	uint16 txiqcal_max_retry_cnt;
	uint16 txiqcal_max_slope_cnt;
	uint16 mppc_cal_failed_count;
	uint16 debug_01;
	uint16 txiqlocal_coeffs[20];
	uint8 is_mppc_gain_offset_cal_success;

	/* Misc general purpose debug counters (will be used for future debugging) */
	uint8	debug_02;
	uint8	debug_03;
	uint8	debug_04;
	uint16	debug_05;
	uint16	debug_06;
	uint32	debug_07;
	uint32	debug_08;

	phy_phycal_core_v255_t phy_phycal_core[2];
} phy_phycal_v255_t;

#define PHY_ECOUNTERS_PHYCAL_STATS_VER1	1u
typedef struct phy_ecounter_phycal_stats_v1 {
	uint16			version;
	uint16			length;
	uint8			num_channel;	/* Number of active channels. */
	uint8			PAD[3];
	phy_ecounter_phycal_v1_t phy_counter[];
} phy_ecounter_phycal_stats_v1_t;

#define PHY_ECOUNTERS_PHYCAL_STATS_VER2	2u
typedef struct phy_ecounter_phycal_stats_v2 {
	uint16			version;
	uint16			length;
	uint8			num_channel;	/* Number of active channels. */
	uint8			PAD[3];
	phy_phycal_v2_t		phy_counter[];
} phy_ecounter_phycal_stats_v2_t;

#define PHY_ECOUNTERS_PHYCAL_STATS_VER3		3u
#define PHY_ECOUNTERS_PHYCAL_STATS_VER3_SIZE	8u
typedef struct phy_ecounter_phycal_stats_v3 {
	uint16			version;
	uint16			length;
	uint8			num_channel;	/* Number of active channels. */
	uint8			PAD[3];
	phy_phycal_v3_t		phy_counter[];
} phy_ecounter_phycal_stats_v3_t;

/* For 27.10 */
#define PHY_ECOUNTERS_PHYCAL_STATS_VER4	4u
typedef struct phy_ecounter_phycal_stats_v4 {
	uint16			version;
	uint16			length;
	uint8			num_channel;	/* Number of active channels. */
	uint8			PAD[3];
	phy_phycal_v4_t		phy_counter[];
} phy_ecounter_phycal_stats_v4_t;

/* For trunk ONLY */
#define PHY_ECOUNTERS_PHYCAL_STATS_VER255	255u
#define PHY_ECOUNTERS_PHYCAL_STATS_VER255_SIZE	8u
typedef struct phy_ecounter_phycal_stats_v255 {
	uint16			version;
	uint16			length;
	uint8			num_channel;	/* Number of active channels. */
	uint8			PAD[3];
	phy_phycal_v255_t	phy_counter[];
} phy_ecounter_phycal_stats_v255_t;

#define PHY_ECOUNTERS_STATS_VER1	1u
typedef struct phy_ecounter_stats_v1 {
	uint16			version;
	uint16			length;
	uint8			num_channel;	/* Number of active channels. */
	uint8			PAD;
	phy_ecounter_v1_t	phy_counter[];
} phy_ecounter_stats_v1_t;

#define PHY_ECOUNTERS_STATS_VER2	2u
typedef struct phy_ecounter_stats_v2 {
	uint16			version;
	uint16			length;
	uint8			num_channel;	/* Number of active channels. */
	uint8			PAD[3];
	phy_ecounter_v2_t	phy_counter[];
} phy_ecounter_stats_v2_t;

#define PHY_ECOUNTERS_STATS_VER3	3u
typedef struct phy_ecounter_stats_v3 {
	uint16			version;
	uint16			length;
	uint8			num_channel;	/* Number of active channels. */
	uint8			PAD[3];
	phy_ecounter_v3_t	phy_counter[];
} phy_ecounter_stats_v3_t;

#define PHY_ECOUNTERS_STATS_VER4	4u
typedef struct phy_ecounter_stats_v4 {
	uint16			version;
	uint16			length;
	uint8			num_channel;	/* Number of active channels. */
	uint8			PAD[3];
	phy_ecounter_v4_t	phy_counter[];
} phy_ecounter_stats_v4_t;

#define PHY_ECOUNTERS_STATS_VER5	5u
typedef struct phy_ecounter_stats_v5 {
	uint16			version;
	uint16			length;
	uint8			num_channel;	/* Number of active channels. */
	uint8			PAD[3];
	phy_ecounter_v5_t	phy_counter[];
} phy_ecounter_stats_v5_t;

#define PHY_ECOUNTERS_STATS_VER6	6u
typedef struct phy_ecounter_stats_v6 {
	uint16			version;
	uint16			length;
	uint8			num_channel;	/* Number of active channels. */
	uint8			PAD[3];
	phy_ecounter_v6_t	phy_counter[];
} phy_ecounter_stats_v6_t;

#define PHY_ECOUNTERS_STATS_VER7	7u
#define PHY_ECOUNTERS_STATS_VER7_SIZE	8u
typedef struct phy_ecounter_stats_v7 {
	uint16			version;
	uint16			length;
	uint8			num_channel;	/* Number of active channels. */
	uint8			PAD[3];
	phy_ecounter_v7_t	phy_counter[];
} phy_ecounter_stats_v7_t;

/* For trunk ONLY */
#define PHY_ECOUNTERS_STATS_VER255	255u
#define PHY_ECOUNTERS_STATS_VER255_SIZE	8u
typedef struct phy_ecounter_stats_v255 {
	uint16			version;
	uint16			length;
	uint8			num_channel;	/* Number of active channels. */
	uint8			PAD[3];
	phy_ecounter_v255_t	phy_counter[];
} phy_ecounter_stats_v255_t;

/* Durations for each bt task in millisecond */
#define WL_BTCX_DURSTATS_VER_2 (2u)
typedef struct wlc_btcx_durstats_v2 {
	uint16 version;			/* version number of struct */
	uint16 valid;			/* validity of this struct */
	uint32 stats_update_timestamp;	/* tStamp when data is updated */
	uint16 bt_acl_dur;		/* acl duration in ms */
	uint16 bt_sco_dur;		/* sco duration in ms */
	uint16 bt_esco_dur;		/* esco duration in ms */
	uint16 bt_a2dp_dur;		/* a2dp duration in ms */
	uint16 bt_sniff_dur;		/* sniff duration in ms */
	uint16 bt_pscan_dur;		/* page scan duration in ms */
	uint16 bt_iscan_dur;		/* inquiry scan duration in ms */
	uint16 bt_page_dur;		/* paging duration in ms */
	uint16 bt_inquiry_dur;		/* inquiry duration in ms */
	uint16 bt_mss_dur;		/* mss duration in ms */
	uint16 bt_chsd_dur;		/* channel sounding duration in ms */
	uint16 bt_rssiscan_dur;		/* rssiscan duration in ms */
	uint16 bt_iscan_sco_dur;	/* inquiry scan sco duration in ms */
	uint16 bt_pscan_sco_dur;	/* page scan sco duration in ms */
	uint16 bt_tpoll_dur;		/* tpoll duration in ms */
	uint16 bt_sacq_dur;		/* sacq duration in ms */
	uint16 bt_sdata_dur;		/* sdata duration in ms */
	uint16 bt_rs_listen_dur;	/* rs listen duration in ms */
	uint16 bt_rs_burst_dur;		/* rs brust duration in ms */
	uint16 bt_ble_adv_dur;		/* ble adv duration in ms */
	uint16 bt_ble_scan_dur;		/* ble scan duration in ms */
	uint16 bt_ble_init_dur;		/* ble init duration in ms */
	uint16 bt_ble_conn_dur;		/* ble connection duration in ms */
	uint16 bt_task_lmp_dur;		/* lmp duration in ms */
	uint16 bt_esco_retran_dur;	/* esco retransmission duration in ms */
	uint16 bt_task26_dur;		/* task26 duration in ms */
	uint16 bt_task27_dur;		/* task27 duration in ms */
	uint16 bt_task28_dur;		/* task28 duration in ms */
	uint16 bt_task_pred_dur;	/* prediction task duration in ms */
	uint16 bt_multihid_dur;		/* multihid duration in ms */
	uint16 bt_scan_tx_dur;		/* Scan Tx duration in ms */
	uint16 bt_disable_dual_bt_dur;		/* Duration of Dual BT disable */
} wlc_btcx_durstats_v2_t;

#define WL_BTCX_DURSTATS_VER_1 (1u)
typedef struct wlc_btcx_durstats_v1 {
	uint16 version;			/* version number of struct */
	uint16 valid;			/* validity of this struct */
	uint32 stats_update_timestamp;	/* tStamp when data is updated */
	uint16 bt_acl_dur;		/* acl duration in ms */
	uint16 bt_sco_dur;		/* sco duration in ms */
	uint16 bt_esco_dur;		/* esco duration in ms */
	uint16 bt_a2dp_dur;		/* a2dp duration in ms */
	uint16 bt_sniff_dur;		/* sniff duration in ms */
	uint16 bt_pscan_dur;		/* page scan duration in ms */
	uint16 bt_iscan_dur;		/* inquiry scan duration in ms */
	uint16 bt_page_dur;		/* paging duration in ms */
	uint16 bt_inquiry_dur;		/* inquiry duration in ms */
	uint16 bt_mss_dur;		/* mss duration in ms */
	uint16 bt_park_dur;		/* park duration in ms */
	uint16 bt_rssiscan_dur;		/* rssiscan duration in ms */
	uint16 bt_iscan_sco_dur;	/* inquiry scan sco duration in ms */
	uint16 bt_pscan_sco_dur;	/* page scan sco duration in ms */
	uint16 bt_tpoll_dur;		/* tpoll duration in ms */
	uint16 bt_sacq_dur;		/* sacq duration in ms */
	uint16 bt_sdata_dur;		/* sdata duration in ms */
	uint16 bt_rs_listen_dur;	/* rs listen duration in ms */
	uint16 bt_rs_burst_dur;		/* rs brust duration in ms */
	uint16 bt_ble_adv_dur;		/* ble adv duration in ms */
	uint16 bt_ble_scan_dur;		/* ble scan duration in ms */
	uint16 bt_ble_init_dur;		/* ble init duration in ms */
	uint16 bt_ble_conn_dur;		/* ble connection duration in ms */
	uint16 bt_task_lmp_dur;		/* lmp duration in ms */
	uint16 bt_esco_retran_dur;	/* esco retransmission duration in ms */
	uint16 bt_task26_dur;		/* task26 duration in ms */
	uint16 bt_task27_dur;		/* task27 duration in ms */
	uint16 bt_task28_dur;		/* task28 duration in ms */
	uint16 bt_task_pred_dur;	/* prediction task duration in ms */
	uint16 bt_multihid_dur;		/* multihid duration in ms */
} wlc_btcx_durstats_v1_t;

#define	WL_IF_STATS_T_VERSION_1	 1	/**< current version of wl_if_stats structure */

/** per interface counters */
typedef struct wl_if_stats {
	uint16	version;		/**< version of the structure */
	uint16	length;			/**< length of the entire structure */
	uint32	PAD;			/**< padding */

	/* transmit stat counters */
	uint64	txframe;		/**< tx data frames */
	uint64	txbyte;			/**< tx data bytes */
	uint64	txerror;		/**< tx data errors (derived: sum of others) */
	uint64  txnobuf;		/**< tx out of buffer errors */
	uint64  txrunt;			/**< tx runt frames */
	uint64  txfail;			/**< tx failed frames */
	uint64	txretry;		/**< tx retry frames */
	uint64	txretrie;		/**< tx multiple retry frames */
	uint64	txfrmsnt;		/**< tx sent frames */
	uint64	txmulti;		/**< tx mulitcast sent frames */
	uint64	txfrag;			/**< tx fragments sent */

	/* receive stat counters */
	uint64	rxframe;		/**< rx data frames */
	uint64	rxbyte;			/**< rx data bytes */
	uint64	rxerror;		/**< rx data errors (derived: sum of others) */
	uint64	rxnobuf;		/**< rx out of buffer errors */
	uint64  rxrunt;			/**< rx runt frames */
	uint64  rxfragerr;		/**< rx fragment errors */
	uint64	rxmulti;		/**< rx multicast frames */

	uint64	txexptime;		/* DATA Tx frames suppressed due to timer expiration */
	uint64	txrts;			/* RTS/CTS succeeeded count */
	uint64	txnocts;		/* RTS/CTS faled count */

	uint64	txretrans;		/* Number of frame retransmissions */
} wl_if_stats_t;

/* ##### Ecounters section ##### */
#define ECOUNTERS_VERSION_1	1

/* Input structure for ecounters IOVAR */
typedef struct ecounters_config_request {
	uint16 version;		/* config version */
	uint16 set;		/* Set where data will go. */
	uint16 size;		/* Size of the set. */
	uint16 timeout;		/* timeout in seconds. */
	uint16 num_events;	/* Number of events to report. */
	uint16 ntypes;		/* Number of entries in type array. */
	uint16 type[BCM_FLEX_ARRAY];		/* Statistics Types (tags) to retrieve. */
} ecounters_config_request_t;

#define ECOUNTERS_EVENTMSGS_VERSION_1		1
#define ECOUNTERS_TRIGGER_CONFIG_VERSION_1	1

#define ECOUNTERS_EVENTMSGS_EXT_MASK_OFFSET	\
		OFFSETOF(ecounters_eventmsgs_ext_t, mask)

#define ECOUNTERS_TRIG_CONFIG_TYPE_OFFSET	\
		OFFSETOF(ecounters_trigger_config_t, type)

typedef struct ecounters_eventmsgs_ext {
	uint8 version;
	uint8 len;
	uint8 mask[BCM_FLEX_ARRAY];
} ecounters_eventmsgs_ext_t;

typedef struct ecounters_trigger_config {
	uint16 version;		/* version */
	uint16 set;		/* set where data should go */
	uint16 rsvd;		/* reserved */
	uint16 PAD;		/* pad/reserved */
	uint16 ntypes;		/* number of types/tags */
	uint16 type[BCM_FLEX_ARRAY];		/* list of types */
} ecounters_trigger_config_t;

#define ECOUNTERS_TRIGGER_REASON_VERSION_1	1
typedef enum {
	/* Triggered due to timer based ecounters */
	ECOUNTERS_TRIGGER_REASON_TIMER = 0,
	/* Triggered due to event based configuration */
	ECOUNTERS_TRIGGER_REASON_EVENTS = 1,
	ECOUNTERS_TRIGGER_REASON_D2H_EVENTS = 2,
	ECOUNTERS_TRIGGER_REASON_H2D_EVENTS = 3,
	ECOUNTERS_TRIGGER_REASON_USER_EVENTS = 4,
	ECOUNTERS_TRIGGER_REASON_MAX = 5
} ecounters_trigger_reasons_list_t;

typedef struct ecounters_trigger_reason {
	uint16 version;			/* version */
	uint16 trigger_reason;		/* trigger reason */
	uint32 sub_reason_code;		/* sub reason code */
	uint32 trigger_time_now;	/* time in ms  at trigger */
	uint32 host_ref_time;		/* host ref time */
} ecounters_trigger_reason_t;

#define WL_LQM_VERSION_1 1

/* For wl_lqm_t flags field */
#define WL_LQM_CURRENT_BSS_VALID 0x1
#define WL_LQM_TARGET_BSS_VALID 0x2

#define WL_PERIODIC_COMPACT_CNTRS_VER_1 (1)
typedef struct {
	uint16 version;
	uint16 PAD;
	/* taken from wl_wlc_cnt_t */
	uint32 txfail;
	/* taken from wl_cnt_ge40mcst_v1_t */
	uint32	txallfrm;	/**< total number of frames sent, incl. Data, ACK, RTS, CTS,
				 * Control Management (includes retransmissions)
				 */
	uint32	txrtsfrm;	/**< number of RTS sent out by the MAC */
	uint32	txctsfrm;	/**< number of CTS sent out by the MAC */
	uint32	txback;		/**< blockack txcnt */
	uint32	txucast;	/**< number of unicast tx expecting response other than cts/cwcts */
	uint32  txnoack;	/**< dot11ACKFailureCount */
	uint32  txframe;	/**< tx data frames */
	uint32  txretrans;	/**< tx mac retransmits */
	uint32  txpspoll;	/**< Number of TX PS-poll */

	uint32	rxrsptmout;	/**< number of response timeouts for transmitted frames
				 * expecting a response
				 */
	uint32	txrtsfail;	/**< number of rts transmission failure that reach retry limit */
	uint32	rxstrt;		/**< number of received frames with a good PLCP */
	uint32  rxbadplcp;	/**< number of parity check of the PLCP header failed */
	uint32  rxcrsglitch;	/**< PHY was able to correlate the preamble but not the header */
	uint32  rxnodelim;	/**< number of no valid delimiter detected by ampdu parser */
	uint32  bphy_badplcp;	/**< number of bad PLCP reception on BPHY rate */
	uint32  bphy_rxcrsglitch;	/**< PHY count of bphy glitches */
	uint32  rxbadfcs;	/**< number of frames for which the CRC check failed in the MAC */
	uint32  rxf0ovfl;	/**< number of receive fifo 0 overflows */
	uint32  rxf1ovfl;	/**< number of receive fifo 0 overflows */
	uint32  rxhlovfl;	/**< number of length / header fifo overflows */
	uint32	rxrtsucast;	/**< number of unicast RTS addressed to the MAC (good FCS) */
	uint32	rxctsucast;	/**< number of unicast CTS addressed to the MAC (good FCS) */
	uint32	rxackucast;	/**< number of ucast ACKS received (good FCS) */
	uint32	rxback;		/**< blockack rxcnt */
	uint32	rxbeaconmbss;	/**< beacons received from member of BSS */
	uint32	rxdtucastmbss;	/**< number of received DATA frames with good FCS and matching RA */
	uint32	rxbeaconobss;	/**< beacons received from other BSS */
	uint32	rxdtucastobss;	/**< number of unicast frames addressed to the MAC from
				* other BSS (WDS FRAME)
				*/
	uint32	rxdtocast;	/**< number of received DATA frames (good FCS and no matching RA) */
	uint32	rxrtsocast;	/**< number of received RTS not addressed to the MAC */
	uint32	rxctsocast;	/**< number of received CTS not addressed to the MAC */
	uint32	rxdtmcast;	/**< number of RX Data multicast frames received by the MAC */
	uint32  rxmpdu_mu;	/**< Number of MU MPDUs received */
	uint32  rxtoolate;	/**< receive too late */
	uint32  rxframe;	/**< rx data frames */
	uint32  lqcm_report;	/**<  lqcm metric tx/rx idx */
	uint32	tx_toss_cnt;	/* number of tx packets tossed */
	uint32	rx_toss_cnt;	/* number of rx packets tossed	*/
	uint32	last_tx_toss_rsn; /* reason because of which last tx pkt tossed */
	uint32	last_rx_toss_rsn; /* reason because of which last rx pkt tossed */
	uint32	txbcnfrm;	/**< beacons transmitted */
} wl_periodic_compact_cntrs_v1_t;

#define WL_PERIODIC_COMPACT_CNTRS_VER_2 (2)
typedef struct {
	uint16 version;
	uint16 PAD;
	/* taken from wl_wlc_cnt_t */
	uint32 txfail;
	/* taken from wl_cnt_ge40mcst_v1_t */
	uint32	txallfrm;	/**< total number of frames sent, incl. Data, ACK, RTS, CTS,
				 * Control Management (includes retransmissions)
				 */
	uint32	txrtsfrm;	/**< number of RTS sent out by the MAC */
	uint32	txctsfrm;	/**< number of CTS sent out by the MAC */
	uint32	txback;		/**< blockack txcnt */
	uint32	txucast;	/**< number of unicast tx expecting response other than cts/cwcts */
	uint32  txnoack;	/**< dot11ACKFailureCount */
	uint32  txframe;	/**< tx data frames */
	uint32  txretrans;	/**< tx mac retransmits */
	uint32  txpspoll;	/**< Number of TX PS-poll */

	uint32	rxrsptmout;	/**< number of response timeouts for transmitted frames
				 * expecting a response
				 */
	uint32	txrtsfail;	/**< number of rts transmission failure that reach retry limit */
	uint32	rxstrt;		/**< number of received frames with a good PLCP */
	uint32  rxbadplcp;	/**< number of parity check of the PLCP header failed */
	uint32  rxcrsglitch;	/**< PHY was able to correlate the preamble but not the header */
	uint32  rxnodelim;	/**< number of no valid delimiter detected by ampdu parser */
	uint32  bphy_badplcp;	/**< number of bad PLCP reception on BPHY rate */
	uint32  bphy_rxcrsglitch;	/**< PHY count of bphy glitches */
	uint32  rxbadfcs;	/**< number of frames for which the CRC check failed in the MAC */
	uint32  rxf0ovfl;	/**< number of receive fifo 0 overflows */
	uint32  rxf1ovfl;	/**< number of receive fifo 0 overflows */
	uint32  rxhlovfl;	/**< number of length / header fifo overflows */
	uint32	rxrtsucast;	/**< number of unicast RTS addressed to the MAC (good FCS) */
	uint32	rxctsucast;	/**< number of unicast CTS addressed to the MAC (good FCS) */
	uint32	rxackucast;	/**< number of ucast ACKS received (good FCS) */
	uint32	rxback;		/**< blockack rxcnt */
	uint32	rxbeaconmbss;	/**< beacons received from member of BSS */
	uint32	rxdtucastmbss;	/**< number of received DATA frames with good FCS and matching RA */
	uint32	rxbeaconobss;	/**< beacons received from other BSS */
	uint32	rxdtucastobss;	/**< number of unicast frames addressed to the MAC from
				* other BSS (WDS FRAME)
				*/
	uint32	rxdtocast;	/**< number of received DATA frames (good FCS and no matching RA) */
	uint32	rxrtsocast;	/**< number of received RTS not addressed to the MAC */
	uint32	rxctsocast;	/**< number of received CTS not addressed to the MAC */
	uint32	rxdtmcast;	/**< number of RX Data multicast frames received by the MAC */
	uint32  rxmpdu_mu;	/**< Number of MU MPDUs received */
	uint32  rxtoolate;	/**< receive too late */
	uint32  rxframe;	/**< rx data frames */
	uint32  lqcm_report;	/**<  lqcm metric tx/rx idx */
	uint32	tx_toss_cnt;	/* number of tx packets tossed */
	uint32	rx_toss_cnt;	/* number of rx packets tossed	*/
	uint32	last_tx_toss_rsn; /* reason because of which last tx pkt tossed */
	uint32	last_rx_toss_rsn; /* reason because of which last rx pkt tossed */
	uint32	txbcnfrm;	/**< beacons transmitted */
	uint32	rxretry;	/* Number of rx packets received after retry */
	uint32	rxdup;		/* Number of dump packet. Indicates whether peer is receiving ack */
	uint32	chswitch_cnt;	/* Number of channel switches */
	uint32 pm_dur;		/* Total sleep time in PM, msecs */
} wl_periodic_compact_cntrs_v2_t;

#define WL_PERIODIC_COMPACT_CNTRS_VER_3 (3)
typedef struct {
	uint16 version;
	uint16 PAD;
	/* taken from wl_wlc_cnt_t */
	uint32 txfail;
	/* taken from wl_cnt_ge40mcst_v1_t */
	uint32	txallfrm;	/**< total number of frames sent, incl. Data, ACK, RTS, CTS,
				 * Control Management (includes retransmissions)
				 */
	uint32	txrtsfrm;	/**< number of RTS sent out by the MAC */
	uint32	txctsfrm;	/**< number of CTS sent out by the MAC */
	uint32	txback;		/**< blockack txcnt */
	uint32	txucast;	/**< number of unicast tx expecting response other than cts/cwcts */
	uint32  txnoack;	/**< dot11ACKFailureCount */
	uint32  txframe;	/**< tx data frames */
	uint32  txretrans;	/**< tx mac retransmits */
	uint32  txpspoll;	/**< Number of TX PS-poll */

	uint32	rxrsptmout;	/**< number of response timeouts for transmitted frames
				 * expecting a response
				 */
	uint32	txrtsfail;	/**< number of rts transmission failure that reach retry limit */
	uint32	rxstrt;		/**< number of received frames with a good PLCP */
	uint32  rxbadplcp;	/**< number of parity check of the PLCP header failed */
	uint32  rxcrsglitch;	/**< PHY was able to correlate the preamble but not the header */
	uint32  rxnodelim;	/**< number of no valid delimiter detected by ampdu parser */
	uint32  bphy_badplcp;	/**< number of bad PLCP reception on BPHY rate */
	uint32  bphy_rxcrsglitch;	/**< PHY count of bphy glitches */
	uint32  rxbadfcs;	/**< number of frames for which the CRC check failed in the MAC */
	uint32  rxf0ovfl;	/**< number of receive fifo 0 overflows */
	uint32  rxf1ovfl;	/**< number of receive fifo 0 overflows */
	uint32  rxhlovfl;	/**< number of length / header fifo overflows */
	uint32	rxrtsucast;	/**< number of unicast RTS addressed to the MAC (good FCS) */
	uint32	rxctsucast;	/**< number of unicast CTS addressed to the MAC (good FCS) */
	uint32	rxackucast;	/**< number of ucast ACKS received (good FCS) */
	uint32	rxback;		/**< blockack rxcnt */
	uint32	rxbeaconmbss;	/**< beacons received from member of BSS */
	uint32	rxdtucastmbss;	/**< number of received DATA frames with good FCS and matching RA */
	uint32	rxbeaconobss;	/**< beacons received from other BSS */
	uint32	rxdtucastobss;	/**< number of unicast frames addressed to the MAC from
				* other BSS (WDS FRAME)
				*/
	uint32	rxdtocast;	/**< number of received DATA frames (good FCS and no matching RA) */
	uint32	rxrtsocast;	/**< number of received RTS not addressed to the MAC */
	uint32	rxctsocast;	/**< number of received CTS not addressed to the MAC */
	uint32	rxdtmcast;	/**< number of RX Data multicast frames received by the MAC */
	uint32  rxmpdu_mu;	/**< Number of MU MPDUs received */
	uint32  rxtoolate;	/**< receive too late */
	uint32  rxframe;	/**< rx data frames */
	uint32  lqcm_report;	/**<  lqcm metric tx/rx idx */
	uint32	tx_toss_cnt;	/* number of tx packets tossed */
	uint32	rx_toss_cnt;	/* number of rx packets tossed	*/
	uint32	last_tx_toss_rsn; /* reason because of which last tx pkt tossed */
	uint32	last_rx_toss_rsn; /* reason because of which last rx pkt tossed */
	uint32	txbcnfrm;	/**< beacons transmitted */
	uint32	rxretry;	/* Number of rx packets received after retry */
	uint32	rxdup;		/* Number of dump packet. Indicates whether peer is receiving ack */
	uint32	chswitch_cnt;	/* Number of channel switches */
	uint32 pm_dur;		/* Total sleep time in PM, msecs */
	uint32 rxholes;		/* Count of missed packets from peer */
} wl_periodic_compact_cntrs_v3_t;

#define WL_PERIODIC_COMPACT_CNTRS_VER_4 (4)
typedef struct {
	uint16 version;
	uint16 PAD;
	/* taken from wl_wlc_cnt_t */
	uint32 txfail;
	/* taken from wl_cnt_ge40mcst_v1_t */
	uint32	txallfrm;	/**< total number of frames sent, incl. Data, ACK, RTS, CTS,
				 * Control Management (includes retransmissions)
				 */
	uint32	txrtsfrm;	/**< number of RTS sent out by the MAC */
	uint32	txctsfrm;	/**< number of CTS sent out by the MAC */
	uint32	txback;		/**< blockack txcnt */
	uint32	txucast;	/**< number of unicast tx expecting response other than cts/cwcts */
	uint32  txnoack;	/**< dot11ACKFailureCount */
	uint32  txframe;	/**< tx data frames */
	uint32  txretrans;	/**< tx mac retransmits */
	uint32  txpspoll;	/**< Number of TX PS-poll */

	uint32	rxrsptmout;	/**< number of response timeouts for transmitted frames
				 * expecting a response
				 */
	uint32	txrtsfail;	/**< number of rts transmission failure that reach retry limit */
	uint32	rxstrt;		/**< number of received frames with a good PLCP */
	uint32  rxbadplcp;	/**< number of parity check of the PLCP header failed */
	uint32  rxcrsglitch;	/**< PHY was able to correlate the preamble but not the header */
	uint32  rxnodelim;	/**< number of no valid delimiter detected by ampdu parser */
	uint32  bphy_badplcp;	/**< number of bad PLCP reception on BPHY rate */
	uint32  bphy_rxcrsglitch;	/**< PHY count of bphy glitches */
	uint32  rxbadfcs;	/**< number of frames for which the CRC check failed in the MAC */
	uint32  rxf0ovfl;	/**< number of receive fifo 0 overflows */
	uint32  rxf1ovfl;	/**< number of receive fifo 0 overflows */
	uint32  rxhlovfl;	/**< number of length / header fifo overflows */
	uint32	rxrtsucast;	/**< number of unicast RTS addressed to the MAC (good FCS) */
	uint32	rxctsucast;	/**< number of unicast CTS addressed to the MAC (good FCS) */
	uint32	rxackucast;	/**< number of ucast ACKS received (good FCS) */
	uint32	rxback;		/**< blockack rxcnt */
	uint32	rxbeaconmbss;	/**< beacons received from member of BSS */
	uint32	rxdtucastmbss;	/**< number of received DATA frames with good FCS and matching RA */
	uint32	rxbeaconobss;	/**< beacons received from other BSS */
	uint32	rxdtucastobss;	/**< number of unicast frames addressed to the MAC from
				* other BSS (WDS FRAME)
				*/
	uint32	rxdtocast;	/**< number of received DATA frames (good FCS and no matching RA) */
	uint32	rxrtsocast;	/**< number of received RTS not addressed to the MAC */
	uint32	rxctsocast;	/**< number of received CTS not addressed to the MAC */
	uint32	rxdtmcast;	/**< number of RX Data multicast frames received by the MAC */
	uint32  rxmpdu_mu;	/**< Number of MU MPDUs received */
	uint32  rxtoolate;	/**< receive too late */
	uint32  rxframe;	/**< rx data frames */
	uint32  lqcm_report;	/**<  lqcm metric tx/rx idx */
	uint32	tx_toss_cnt;	/* number of tx packets tossed */
	uint32	rx_toss_cnt;	/* number of rx packets tossed	*/
	uint32	last_tx_toss_rsn; /* reason because of which last tx pkt tossed */
	uint32	last_rx_toss_rsn; /* reason because of which last rx pkt tossed */
	uint32	txbcnfrm;	/**< beacons transmitted */
	uint32	rxretry;	/* Number of rx packets received after retry */
	uint32	rxdup;		/* Number of dump packet. Indicates whether peer is receiving ack */
	uint32	chswitch_cnt;	/* Number of channel switches */
	uint32	pm_dur;		/* Total sleep time in PM, msecs */
	uint32	rxholes;	/* Count of missed packets from peer */

	uint32	rxundec;	/* Decrypt failures */
	uint32	rxundec_mcst;	/* Decrypt failures multicast */
	uint16	replay;		/* replay failures */
	uint16	replay_mcst;	/* ICV failures */

	uint32	pktfilter_discard;	/* Filtered packtets by pkt filter */
	uint32	pktfilter_forward;	/* Forwared packets by pkt filter */
	uint32	mac_rxfilter;	/* Pkts filtered due to class/auth state mismatch */
} wl_periodic_compact_cntrs_v4_t;

#define WL_PERIODIC_COMPACT_CNTRS_VER_5 (5)
typedef struct {
	uint16 version;
	uint8	PAD;
	uint8	link_id;	/**< link id corr to slice. NOT cfg idx */
	/* taken from wl_wlc_cnt_t */
	uint32 txfail;
	/* taken from wl_cnt_ge88mcst_v1_t */
	/* --------- TX ------------------- */
	uint32	txallfrm;	/**< total number of frames sent, incl. Data, ACK, RTS, CTS,
				 * Control Management (includes retransmissions)
				 */
	uint32	txrtsfrm;	/**< number of RTS sent out by the MAC */
	uint32	txctsfrm;	/**< number of CTS sent out by the MAC */
	uint32	txback;		/**< blockack txcnt */
	uint32	txucast;	/**< number of unicast tx expecting response other than cts/cwcts */
	uint32  txnoack;	/**< dot11ACKFailureCount */
	uint32  txframe;	/**< tx data frames */
	uint32  txretrans;	/**< tx mac retransmits */
	uint32  txpspoll;	/**< Number of TX PS-poll */
	uint32	txrtsfail;	/**< number of rts transmission failure that reach retry limit */
	uint32	tx_toss_cnt;	/* number of tx packets tossed */
	uint32	last_tx_toss_rsn; /* reason because of which last tx pkt tossed */
	uint32	txbcnfrm;	/**< beacons transmitted */

	/* --------- RX ------------------- */
	uint32	rxrsptmout;	/**< number of response timeouts for transmitted frames
				 * expecting a response
				 */
	uint32	rxstrt;		/**< number of received frames with a good PLCP */
	uint32  rxbadplcp;	/**< number of parity check of the PLCP header failed */
	uint32  rxcrsglitch;	/**< PHY was able to correlate the preamble but not the header */
	uint32  rxnodelim;	/**< number of no valid delimiter detected by ampdu parser */
	uint32  bphy_badplcp;	/**< number of bad PLCP reception on BPHY rate */
	uint32  bphy_rxcrsglitch;	/**< PHY count of bphy glitches */
	uint32  rxbadfcs;	/**< number of frames for which the CRC check failed in the MAC */
	uint32  rxf0ovfl;	/**< number of receive fifo 0 overflows */
	uint32  rxf1ovfl;	/**< number of receive fifo 0 overflows */
	uint32  rxhlovfl;	/**< number of length / header fifo overflows */
	uint32	rxrtsucast;	/**< number of unicast RTS addressed to the MAC (good FCS) */
	uint32	rxctsucast;	/**< number of unicast CTS addressed to the MAC (good FCS) */
	uint32	rxackucast;	/**< number of ucast ACKS received (good FCS) */
	uint32	rxback;		/**< blockack rxcnt */
	uint32	rxbeaconmbss;	/**< beacons received from member of BSS */
	uint32	rxdtucastmbss;	/**< number of received DATA frames with good FCS and matching RA */
	uint32	rxbeaconobss;	/**< beacons received from other BSS */
	uint32	rxdtucastobss;	/**< number of unicast frames addressed to the MAC from
				* other BSS (WDS FRAME)
				*/
	uint32	rxdtocast;	/**< number of received DATA frames (good FCS and no matching RA) */
	uint32	rxrtsocast;	/**< number of received RTS not addressed to the MAC */
	uint32	rxctsocast;	/**< number of received CTS not addressed to the MAC */
	uint32	rxdtmcast;	/**< number of RX Data multicast frames received by the MAC */
	uint32  rxmpdu_mu;	/**< Number of MU MPDUs received */
	uint32  rxtoolate;	/**< receive too late */
	uint32  rxframe;	/**< rx data frames */
	uint32	rx_toss_cnt;	/* number of rx packets tossed	*/
	uint32	last_rx_toss_rsn; /* reason because of which last rx pkt tossed */
	uint32	rxretry;	/* Number of rx packets received after retry */
	uint32	rxdup;		/* Number of dump packet. Indicates whether peer is receiving ack */
	uint32	rxholes;	/* Count of missed packets from peer */
	uint32	rxundec;	/* Decrypt failures */
	uint32	rxundec_mcst;	/* Decrypt failures multicast */
	uint16	replay;		/* replay failures */
	uint16	replay_mcst;	/* ICV failures */

	/* -------------OTHERS--------------- */
	uint32  lqcm_report;	/**<  lqcm metric tx/rx idx */
	uint32	chswitch_cnt;	/* Number of channel switches */
	uint32	pm_dur;		/* Total sleep time in PM, msecs */
	uint32	pktfilter_discard;	/* Filtered packtets by pkt filter */
	uint32	pktfilter_forward;	/* Forwared packets by pkt filter */
	uint32	mac_rxfilter;	/* Pkts filtered due to class/auth state mismatch */
} wl_periodic_compact_cntrs_v5_t;

#define WL_PERIODIC_COMPACT_HE_CNTRS_VER_1 (1)
typedef struct {
	uint16 version;
	uint16 len;
	uint32 he_rxtrig_rand;
	uint32 he_colormiss_cnt;
	uint32 he_txmtid_back;
	uint32 he_rxmtid_back;
	uint32 he_rxmsta_back;
	uint32 he_rxtrig_basic;
	uint32 he_rxtrig_murts;
	uint32 he_rxtrig_bsrp;
	uint32 he_rxdlmu;
	uint32 he_physu_rx;
	uint32 he_txtbppdu;
} wl_compact_he_cnt_wlc_v1_t;

#define WL_PERIODIC_COMPACT_HE_CNTRS_VER_2 (2)
typedef struct {
	uint16 version;
	uint16 len;
	uint32 he_rxtrig_myaid;
	uint32 he_rxtrig_rand;
	uint32 he_colormiss_cnt;
	uint32 he_txmampdu;
	uint32 he_txmtid_back;
	uint32 he_rxmtid_back;
	uint32 he_rxmsta_back;
	uint32 he_txfrag;
	uint32 he_rxdefrag;
	uint32 he_txtrig;
	uint32 he_rxtrig_basic;
	uint32 he_rxtrig_murts;
	uint32 he_rxtrig_bsrp;
	uint32 he_rxhemuppdu_cnt;
	uint32 he_physu_rx;
	uint32 he_phyru_rx;
	uint32 he_txtbppdu;
	uint32 he_null_tbppdu;
	uint32 he_rxhesuppdu_cnt;
	uint32 he_rxhesureppdu_cnt;
	uint32 he_null_zero_agg;
	uint32 he_null_bsrp_rsp;
	uint32 he_null_fifo_empty;
} wl_compact_he_cnt_wlc_v2_t;

#define WL_PERIODIC_COMPACT_HE_CNTRS_VER_3 (3)
typedef struct {
	uint16 version;
	uint16 len;
	uint8 link_id;
	uint8 pad[3];
	uint32 he_rxtrig_myaid;
	uint32 he_rxtrig_rand;
	uint32 he_colormiss_cnt;
	uint32 he_txmampdu;
	uint32 he_txmtid_back;
	uint32 he_rxmtid_back;
	uint32 he_rxmsta_back;
	uint32 he_txfrag;
	uint32 he_rxdefrag;
	uint32 he_txtrig;
	uint32 he_rxtrig_basic;
	uint32 he_rxtrig_murts;
	uint32 he_rxtrig_bsrp;
	uint32 he_rxhemuppdu_cnt;
	uint32 he_physu_rx;
	uint32 he_phyru_rx;
	uint32 he_txtbppdu;
	uint32 he_null_tbppdu;
	uint32 he_rxhesuppdu_cnt;
	uint32 he_rxhesureppdu_cnt;
	uint32 he_null_zero_agg;
	uint32 he_null_bsrp_rsp;
	uint32 he_null_fifo_empty;
} wl_compact_he_cnt_wlc_v3_t;

#define WL_PERIODIC_TXBF_CNTRS_VER_1 (1)
/* for future versions of this data structure, can consider wl_txbf_ecounters_t
 * which contains the full list of txbf dump counters
 */
typedef struct {
	uint16	version;
	uint16	coreup;
	uint32  txndpa;
	uint32	txndp;
	uint32	rxsf;
	uint32	txbfm;
	uint32	rxndpa_u;
	uint32	rxndpa_m;
	uint32	bferpt;
	uint32	rxbfpoll;
	uint32	txsf;
} wl_periodic_txbf_cntrs_v1_t;

#define WL_PERIODIC_TXBF_CNTRS_VER_2 (2)
typedef struct {
	uint16	version;
	uint8	link_id;
	uint8	pad[3];
	uint16	coreup;
	uint32  txndpa;
	uint32	txndp;
	uint32	rxsf;
	uint32	txbfm;
	uint32	rxndpa_u;
	uint32	rxndpa_m;
	uint32	bferpt;
	uint32	rxbfpoll;
	uint32	txsf;
} wl_periodic_txbf_cntrs_v2_t;

typedef struct {
	struct ether_addr BSSID;
	chanspec_t chanspec;
	int32 rssi;
	int32 snr;
} wl_rx_signal_metric_t;

typedef struct {
	uint8 version;
	uint8 flags;
	uint16 PAD;
	int32 noise_level; /* current noise level */
	wl_rx_signal_metric_t current_bss;
	wl_rx_signal_metric_t target_bss;
} wl_lqm_t;

#define WL_PERIODIC_IF_STATE_VER_1 (1)
typedef struct wl_if_state_compact {
	uint8 version;
	uint8 assoc_state;
	uint8 antenna_count;		/**< number of valid antenna rssi */
	int8 noise_level;		/**< noise right after tx (in dBm) */
	int8 snr;			/* current noise level */
	int8 rssi_sum;			/**< summed rssi across all antennas */
	uint16 PAD;
	int8 rssi_ant[WL_RSSI_ANT_MAX]; /**< rssi per antenna */
	struct ether_addr BSSID;
	chanspec_t chanspec;
} wl_if_state_compact_t;

#define WL_EVENT_STATISTICS_VER_1 (1)
/* Event based statistics ecounters */
typedef struct {
	uint16 version;
	uint16 PAD;
	struct ether_addr   BSSID;			/* BSSID of the BSS */
	uint16 PAD;
	uint32 txdeauthivalclass;
} wl_event_based_statistics_v1_t;

#define WL_EVENT_STATISTICS_VER_2 (2)
/* Event based statistics ecounters */
typedef struct {
	uint16 version;
	uint16 PAD;
	struct ether_addr   BSSID;		/* BSSID of the BSS */
	uint16 PAD;
	uint32 txdeauthivalclass;
	/* addition for v2 */
	int32 timestamp;                        /* last deauth time */
	struct ether_addr last_deauth;          /* wrong deauth MAC */
	uint16 misdeauth;                       /* wrong deauth count every 1sec */
	int16 cur_rssi;                         /* current bss rssi */
	int16 deauth_rssi;                      /* deauth pkt rssi */
} wl_event_based_statistics_v2_t;

#define WL_EVENT_STATISTICS_VER_3 (3)
/* Event based statistics ecounters */
typedef struct {
	uint16 version;
	uint16 PAD;
	struct ether_addr   BSSID;			/* BSSID of the BSS */
	uint16 PAD;
	uint32 txdeauthivalclass;
	/* addition for v2 */
	int32 timestamp;                        /* last deauth time */
	struct ether_addr last_deauth;          /* wrong deauth MAC */
	uint16 misdeauth;                       /* wrong deauth count every 1sec */
	int16 cur_rssi;                         /* current bss rssi */
	int16 deauth_rssi;                      /* deauth pkt rssi */
	/* addition for v3 (roam statistics) */
	uint32 initial_assoc_time;
	uint32 prev_roam_time;
	uint32 last_roam_event_type;
	uint32 last_roam_event_status;
	uint32 last_roam_event_reason;
	uint16 roam_success_cnt;
	uint16 roam_fail_cnt;
	uint16 roam_attempt_cnt;
	uint16 max_roam_target_cnt;
	uint16 min_roam_target_cnt;
	uint16 max_cached_ch_cnt;
	uint16 min_cached_ch_cnt;
	uint16 partial_roam_scan_cnt;
	uint16 full_roam_scan_cnt;
	uint16 most_roam_reason;
	uint16 most_roam_reason_cnt;
	uint16 PAD;
} wl_event_based_statistics_v3_t;

#define WL_EVENT_STATISTICS_VER_4 (4u)
/* Event based statistics ecounters */
typedef struct {
	uint16 version;
	uint16 PAD;
	struct ether_addr   BSSID;			/* BSSID of the BSS */
	uint16 PAD;
	uint32 txdeauthivalclass;
	/* addition for v2 */
	int32 timestamp;                        /* last deauth time */
	struct ether_addr last_deauth;          /* wrong deauth MAC */
	uint16 misdeauth;                       /* wrong deauth count every 1sec */
	int16 cur_rssi;                         /* current bss rssi */
	int16 deauth_rssi;                      /* deauth pkt rssi */
} wl_event_based_statistics_v4_t;

/* ##### SC/ Sc offload/ WBUS related ecounters */

#define WL_SC_PERIODIC_COMPACT_CNTRS_VER_1 (1)
typedef struct {
	uint16	version;
	uint16	PAD;
	uint32	rxstrt;		/**< number of received frames with a good PLCP */
	uint32  rxbadplcp;	/**< number of parity check of the PLCP header failed */
	uint32  rxcrsglitch;	/**< PHY was able to correlate the preamble but not the header */
	uint32  rxnodelim;	/**< number of no valid delimiter detected by ampdu parser */
	uint32  bphy_badplcp;	/**< number of bad PLCP reception on BPHY rate */
	uint32  bphy_rxcrsglitch;	/**< PHY count of bphy glitches */
	uint32  rxbadfcs;	/**< number of frames for which the CRC check failed in the MAC */
	uint16	rxrtsucast;	/**< number of unicast RTS addressed to the MAC (good FCS) */
	uint16  rxf0ovfl;	/**< number of receive fifo 0 overflows */
	uint16  rxf1ovfl;	/**< number of receive fifo 0 overflows */
	uint16  rxhlovfl;	/**< number of length / header fifo overflows */
	uint16	rxbeaconmbss;	/**< beacons received from member of BSS */
	uint16	rxdtucastmbss;	/**< number of received DATA frames with good FCS and matching RA */
	uint16	rxbeaconobss;	/**< beacons received from other BSS */
	uint16	rxdtmcast;	/**< number of RX Data multicast frames received by the MAC */
	uint16  rxtoolate;	/**< receive too late */
	uint16	chswitch_cnt;	/* Number of channel switches */
	uint32	pm_dur;		/* Total sleep time in PM, msecs */
	uint16	hibernate_cnt;	/* Number of times sc went to hibernate */
	uint16	awake_cnt;	/* Number of times sc awake is called */
	uint16	sc_up_cnt;	/* Number of times sc up/down happened */
	uint16	sc_down_cnt;	/* Number of times sc down happened */
} wl_sc_periodic_compact_cntrs_v1_t;

#define WL_SC_PERIODIC_COMPACT_CNTRS_VER_2 (2)
typedef struct {
	uint16	version;
	uint8	PAD;
	uint8	link_id;	/**< link id corr to slice. NOT cfg idx */
	/* -----RX------------- */
	uint32	rxstrt;		/**< number of received frames with a good PLCP */
	uint32  rxbadplcp;	/**< number of parity check of the PLCP header failed */
	uint32  rxcrsglitch;	/**< PHY was able to correlate the preamble but not the header */
	uint32  rxnodelim;	/**< number of no valid delimiter detected by ampdu parser */
	uint32  bphy_badplcp;	/**< number of bad PLCP reception on BPHY rate */
	uint32  bphy_rxcrsglitch;	/**< PHY count of bphy glitches */
	uint32  rxbadfcs;	/**< number of frames for which the CRC check failed in the MAC */
	uint16	rxrtsucast;	/**< number of unicast RTS addressed to the MAC (good FCS) */
	uint16  rxf0ovfl;	/**< number of receive fifo 0 overflows */
	uint16  rxf1ovfl;	/**< number of receive fifo 0 overflows */
	uint16  rxhlovfl;	/**< number of length / header fifo overflows */
	uint16	rxbeaconmbss;	/**< beacons received from member of BSS */
	uint16	rxdtucastmbss;	/**< number of received DATA frames with good FCS and matching RA */
	uint16	rxbeaconobss;	/**< beacons received from other BSS */
	uint16	rxdtmcast;	/**< number of RX Data multicast frames received by the MAC */
	uint16  rxtoolate;	/**< receive too late */

	/* --------OTHERS------------ */
	uint16	chswitch_cnt;	/* Number of channel switches */
	uint32	pm_dur;		/* Total sleep time in PM, msecs */
	uint16	hibernate_cnt;	/* Number of times sc went to hibernate */
	uint16	awake_cnt;	/* Number of times sc awake is called */
	uint16	sc_up_cnt;	/* Number of times sc up/down happened */
	uint16	sc_down_cnt;	/* Number of times sc down happened */
} wl_sc_periodic_compact_cntrs_v2_t;

#define WL_WBUS_PERIODIC_CNTRS_VER_1 (1)
typedef struct {
	uint16 version;
	uint16 PAD;
	uint16 num_register;		/* Number of registrations */
	uint16 num_deregister;		/* Number of deregistrations */
	uint8 num_pending;		/* Number of pending non-bt */
	uint8 num_active;		/* Number of active non-bt */
	uint8 num_bt;			/* Number of bt users */
	uint8 PAD;
	uint16 num_rej;			/* Number of reject */
	uint16 num_rej_bt;		/* Number of rejects for bt */
	uint16 num_accept_attempt;	/* Numbber of accept attempt */
	uint16 num_accept_ok;		/* Number of accept ok */
} wl_wbus_periodic_cntrs_v1_t;

#define WL_STA_OFLD_CNTRS_VER_1 (1)
typedef struct {
	uint16	version;
	uint16	PAD;

	uint16	sc_ofld_enter_cnt;
	uint16	sc_ofld_exit_cnt;
	uint16	sc_ofld_wbus_reject_cnt;
	uint16	sc_ofld_wbus_cb_fail_cnt;
	uint16	sc_ofld_missed_bcn_cnt;
	uint8	sc_ofld_last_exit_reason;
	uint8	sc_ofld_last_enter_fail_reason;
} wl_sta_ofld_cntrs_v1_t;

#define WL_STA_OFLD_CNTRS_VER_2 (2u)
typedef struct {
	uint16	version;
	uint16	PAD;

	uint16	sc_ofld_enter_cnt;		/* offload entry cnt */
	uint16	sc_ofld_exit_cnt;		/* offload exit cnt */
	uint16	sc_ofld_wbus_reject_cnt;	/* wbus rejected cnt */
	uint16	sc_ofld_wbus_cb_fail_cnt;	/* wbus callbk fail cnt */
	uint16	sc_ofld_missed_bcn_cnt;		/* missed bcn cnt */
	uint8	sc_ofld_last_exit_reason;	/* reason of last exit */
	uint8	PAD[5];
	uint32	sc_ofld_last_enter_fail_reason;	/* reason preventing entry */
	uint32	sc_ofld_last_sc_bcn_ts;      /* SC ofld last bcn ts  */
	uint32	sc_ofld_last_enter_ts;       /* SC ofld last enter ts  */
	uint32	sc_ofld_last_exit_ts;        /* SC ofld last exit ts  */
} wl_sta_ofld_cntrs_v2_t;

#define WL_STA_MLO_SLOT_SW_STATS_VERSION_1 1u
typedef struct wl_sta_mlo_slot_sw_stats_v1 {
	uint16	version;		/* version field */
	uint16	length;			/* struct length starting from version */
	uint16	bcn_chanspec;		/**< AP operating chanspec corresponding to link index */
	uint8	link_index;		/**< link index number to which the stats are collected */

	uint8	link_slot_on_off;	/**< -1: link state not specified,
					 * 1:link state on chan w.r.t AP operating channel
					 * 0:link state off chan w.r.t AP operating channel
					 */
	uint64	timestamp;		/* time of collection of stats */

	uint32	txframe;		/**< Overall transmitted frame, slice specific */
	uint32	txmulti;		/**< tx mulitcast sent frames */
	uint32	txfail;			/**< tx failed frames */
	uint32	txretry;		/**< tx retry frames */
	uint32	txmultiretry;		/**< tx multiple retry frames */

	uint32	rxframe;		/**< rx data frames */
	uint32	rxmulti;		/**< rx multicast frames */
	uint32	rx_decrypt_failures;	/**< rx decrypt failures */
} wl_sta_mlo_slot_sw_stats_v1_t;

/* MLO LINK STATS */
/* TLVs for LINK STATs related IOVAR */
typedef enum wl_mlo_link_stats_tlv {
	MLO_LINK_STATS_RSVD = 0,
	MLO_LINK_STATS_PLINK = 1u,
	MLO_LINK_STATS_NPLINK = 2u,
	MLO_LINK_STATS_MAX
} wl_mlo_link_stats_tlv_t;

/* common stats of each link */
typedef struct wl_mlo_link_stats_common_v1 {
	uint8	link_idx;		/* link index - link config idx */
	uint8	is_pref;		/* Is Preferred link */
	chanspec_t	chanspec;	/* Chanspec */
	uint8	is_emlsr_primary;	/* Is emlsr primary */
	uint8	PAD[3];
	uint32	txframe;		/* total num of tx pkts */
	uint32	txfail;			/* num of packets failed */
	uint32	txretry;		/* num of packets where a retry was necessary */
	uint32	txretry_exhausted;	/* num of packets where a retry was exhausted */
	uint32	rxframe;		/* num of unicast packets received */
} wl_mlo_link_stats_common_v1_t;

typedef struct wl_mlp_nplink_specific_stats_v1 {
	uint32	nplink_switch_try;	/* # of link switch trial */
	uint32	nplink_use;		/* # of link switch used */
	uint32	nplink_block_old_rssi;	/* # of nplink blocked by old rssi */
	uint32	nplink_block_low_rssi;	/* # of nplink blocked by low rssi */
	uint32	nplink_block_psr;	/* # of nplink blocked by txpsr */
} wl_mlo_nplink_specific_stats_v1_t;

typedef struct wl_mlo_plink_specific_stats_v1 {
	uint32	plink_offchan_rsn_scan;	/* # of plink offchan by scan */
	uint32	plink_offchan_rsn_sb;	/* # of plink offchan by sb */
} wl_mlo_plink_specific_stats_v1_t;

#define WL_MLO_PLINK_STATS_VERSION_1 1u
/* mlo plink stats structure */
typedef struct wl_mlo_plink_stats_v1 {
	uint16	version;		/* version field */
	uint8	PAD[2];
	wl_mlo_link_stats_common_v1_t link_cmn_stats;
	wl_mlo_plink_specific_stats_v1_t link_specific_stats;
} wl_mlo_plink_stats_v1_t;

#define WL_MLO_NPLINK_STATS_VERSION_1 1u
/* mlo nplink stats structure */
typedef struct wl_mlo_nplink_stats_v1 {
	uint16	version;		/* version field */
	uint8	PAD[2];
	wl_mlo_link_stats_common_v1_t link_cmn_stats;
	wl_mlo_nplink_specific_stats_v1_t link_specific_stats;
} wl_mlo_nplink_stats_v1_t;

#define WL_MLO_STATS_VERSION_1 1u
typedef struct wl_mlo_stats_v1 {
	uint16	version;	/* version field */
	uint16	length;		/* struct length starting from version */
	uint8	link_stats_tlvs[];	/* link stat xtlv per each link */
} wl_mlo_stats_v1_t;

/* ##### Ecounters v2 section ##### */

#define ECOUNTERS_VERSION_2	2

/* Enumeration of various ecounters request types. This namespace is different from
 * global reportable stats namespace.
*/
enum {
	WL_ECOUNTERS_XTLV_REPORT_REQ = 1
};

/* Input structure for ecounters IOVAR */
typedef struct ecounters_config_request_v2 {
	uint16 version;		/* config version */
	uint16 len;		/* Length of this struct including variable len */
	uint16 logset;		/* Set where data will go. */
	uint16 reporting_period;	/* reporting_period */
	uint16 num_reports;	/* Number of timer expirations to report on */
	uint8 PAD[2];		/* Reserved for future use */
	uint8 ecounters_xtlvs[];	/* Statistics Types (tags) to retrieve. */
} ecounters_config_request_v2_t;

#define ECOUNTERS_STATS_TYPES_FLAG_SLICE	0x1
#define ECOUNTERS_STATS_TYPES_FLAG_IFACE	0x2
#define ECOUNTERS_STATS_TYPES_FLAG_GLOBAL	0x4
#define ECOUNTERS_STATS_TYPES_DEFAULT		0x8

/* Slice mask bits */
#define ECOUNTERS_STATS_TYPES_SLICE_MASK_SLICE0		0x1u
#define ECOUNTERS_STATS_TYPES_SLICE_MASK_SLICE1		0x2u
#define ECOUNTERS_STATS_TYPES_SLICE_MASK_SLICE_SC	0x4u

typedef struct ecounters_stats_types_report_req {
	/* flags: bit0 = slice, bit1 = iface, bit2 = global,
	 * rest reserved
	 */
	uint16 flags;
	uint16 if_index;	/* host interface index */
	uint16 slice_mask;	/* bit0 = slice0, bit1=slice1, rest reserved */
	uint8 PAD[2];	/* padding */
	uint8 stats_types_req[]; /* XTLVs of requested types */
} ecounters_stats_types_report_req_t;

/* ##### Ecounters_Eventmsgs v2 section ##### */

#define ECOUNTERS_EVENTMSGS_VERSION_2		2

typedef struct event_ecounters_config_request_v2 {
	uint16 version;	/* config version */
	uint16 len;	/* Length of this struct including variable len */
	uint16 logset;	/* Set where data will go. */
	uint16 event_id;	/* Event id for which this config is meant for */
	uint8 flags;	/* Config flags */
	uint8 PAD[3];	/* Reserved for future use */
	uint8 ecounters_xtlvs[];	/* Statistics Types (tags) to retrieve. */
} event_ecounters_config_request_v2_t;

#define EVENT_ECOUNTERS_FLAGS_ADD	(1 << 0) /* Add configuration for the event_id if set */
#define EVENT_ECOUNTERS_FLAGS_DEL	(1 << 1) /* Delete configuration for event_id if set */
#define EVENT_ECOUNTERS_FLAGS_ANYIF	(1 << 2) /* Interface filtering disable / off bit */
#define EVENT_ECOUNTERS_FLAGS_BE	(1 << 3) /* If cleared report stats of
						    * one event log buffer
						    */
#define EVENT_ECOUNTERS_FLAGS_DEL_ALL	(1 << 4) /* Delete all the configurations of
						    * event ecounters if set
						    */

#define EVENT_ECOUNTERS_FLAGS_BUS	(1 << 5) /* Add configuration for the bus events */
#define EVENT_ECOUNTERS_FLAGS_BUS_H2D	(1 << 6) /* Add configuration for the bus direction
						  * 0 - D2H and 1 - H2D
						  */

#define EVENT_ECOUNTERS_FLAGS_DELAYED_FLUSH	(1 << 7) /* Flush only when half of the total size
						   * of blocks gets filled. This is to avoid
						   * many interrupts to host.
						   */
#define EVENT_ECOUNTERS_FLAGS_USER	(1 << 6) /* Add configuration for user defined events
						* Reuse the same flag as H2D
						*/

/* Ecounters suspend resume */
#define ECOUNTERS_SUSPEND_VERSION_V1	1
/* To be used in populating suspend_mask and suspend_bitmap */
#define ECOUNTERS_SUSPEND_TIMER (1 << ECOUNTERS_TRIGGER_REASON_TIMER)
#define ECOUNTERS_SUSPEND_EVENTS (1 << ECOUNTERS_TRIGGER_REASON_EVENTS)

typedef struct ecounters_suspend {
	uint16 version;
	uint16 len;
	uint32 suspend_bitmap; /* type of ecounter reporting to be suspended */
	uint32 suspend_mask; /* type of ecounter reporting to be suspended */
} ecounters_suspend_t;

/* current version of wl_stats_report_t structure for request */
#define WL_STATS_REPORT_REQUEST_VERSION_V2	2

/* current version of wl_stats_report_t structure for response */
#define WL_STATS_REPORT_RESPONSE_VERSION_V2	2

/** Top structure of if_counters IOVar buffer */
typedef struct wl_stats_report {
	uint16	version;	/**< see version definitions above */
	uint16	length;		/**< length of data including all paddings. */
	uint8   data [];	/**< variable length payload:
				 * 1 or more bcm_xtlv_t type of tuples.
				 * each tuple is padded to multiple of 4 bytes.
				 * 'length' field of this structure includes all paddings.
				 */
} wl_stats_report_t;

/* interface specific mgt count */
#define WL_MGT_STATS_VERSION_V1	1
/* Associated stats type: WL_IFSTATS_MGT_CNT */
typedef struct {
	uint16	version;
	uint16	length;

	/* detailed control/management frames */
	uint32	txnull;
	uint32	rxnull;
	uint32	txqosnull;
	uint32	rxqosnull;
	uint32	txassocreq;
	uint32	rxassocreq;
	uint32	txreassocreq;
	uint32	rxreassocreq;
	uint32	txdisassoc;
	uint32	rxdisassoc;
	uint32	txassocrsp;
	uint32	rxassocrsp;
	uint32	txreassocrsp;
	uint32	rxreassocrsp;
	uint32	txauth;
	uint32	rxauth;
	uint32	txdeauth;
	uint32	rxdeauth;
	uint32	txprobereq;
	uint32	rxprobereq;
	uint32	txprobersp;
	uint32	rxprobersp;
	uint32	txaction;
	uint32	rxaction;
	uint32	txpspoll;
	uint32	rxpspoll;
} wl_if_mgt_stats_t;

/* This structure (wl_if_infra_stats_t) is deprecated in favour of
 * versioned structure (wl_if_infra_enh_stats_vxxx_t) defined below
 */
#define WL_INFRA_STATS_VERSION_V1	1
/* Associated stats type: WL_IFSTATS_INFRA_SPECIFIC */
typedef struct wl_infra_stats {
	uint16 version;             /**< version of the structure */
	uint16 length;
	uint32 rxbeaconmbss;
	uint32 tbtt;
} wl_if_infra_stats_t;

/* Starting the versioned structure with version as 2 to distinguish
 * between legacy unversioned structure
 */
#define WL_INFRA_ENH_STATS_VERSION_V2	2u
/* Associated stats type: WL_IFSTATS_INFRA_SPECIFIC */
typedef struct wl_infra_enh_stats_v2 {
	uint16 version;		/**< version of the structure */
	uint16 length;
	uint32 rxbeaconmbss;
	uint32 tbtt;
	uint32 tim_mcast_ind;	/**< number of beacons with tim bits indicating multicast data */
	uint32 tim_ucast_ind;	/**< number of beacons with tim bits indicating unicast data */
	uint32 rxdur_broadcast;	/**< broadcast RX duration (exclude beacon) */
	uint32 rxdur_multicast;	/**< multicast RX duration (include rxdur_broadcast) */
} wl_if_infra_enh_stats_v2_t;

#define WL_INFRA_STATS_HE_VERSION_V1	(1u)
/* Associated stats type: WL_IFSTATS_INFRA_SPECIFIC_HE */
typedef struct wl_infra_stats_he {
	uint16 version;			/**< version of the structure */
	uint16 length;
	uint32	PAD;			/**< Explicit padding */

	/* DL SU MPDUs and total number of bytes */
	uint64 dlsu_mpdudata;
	uint64 dlsu_mpdu_bytes;

	/* DL MUMIMO MPDUs and total number of bytes  */
	uint64 dlmumimo_mpdudata;
	uint64 dlmumimo_mpdu_bytes;

	/* DL OFDMA MPDUs and total number of bytes  */
	uint64 dlofdma_mpdudata;
	uint64 dlofdma_mpdu_bytes;

	/* UL SU MPDUs and total number of bytes  */
	uint64 ulsu_mpdudata;
	uint64 ulsu_mpdu_bytes;

	/* ULOFDMA MPSUs and total number of bytes  */
	uint64 ulofdma_mpdudata;
	uint64 ulofdma_mpdu_bytes;
} wl_if_infra_stats_he_t;

#define WL_RX_MPDU_LOST_CNT_VERSION	(1u)
typedef struct rx_mpdu_lost_cnt {
	uint16	version;
	uint16	length;
	uint32	rx_mpdu_lost_ba[NUMPRIO];
	uint32	rx_mpdu_lost_nonba[NUMPRIO];
} rx_mpdu_lost_cnt_t;

#define LTECOEX_STATS_VER   1

typedef struct wlc_ltecoex_stats {
	uint16 version;	     /**< WL_IFSTATS_XTLV_WL_SLICE_LTECOEX */
	uint16 len;			/* Length of  wl_ltecx_stats structure */
	uint8 slice_index;	/* Slice unit of  wl_ltecx_stats structure */
	uint8 PAD[3];	/* Padding */
	/* LTE noise based eCounters Bins
	 cumulative the wl_cnt_wlc_t and  wl_ctl_mgt_cnt_t
	 counter information based on LTE Coex interference level
	 */
	uint32	txframe_no_LTE;		/* txframe counter in no LTE Coex case */
	uint32	rxframe_no_LTE;		/* rxframe counter in no LTE Coex case */
	uint32	rxrtry_no_LTE;		/* rxrtry counter in no LTE Coex case */
	uint32	txretrans_no_LTE;	/* txretrans counter in no LTE Coex case */
	uint32	txnocts_no_LTE;		/* txnocts counter in no LTE Coex case */
	uint32	txrts_no_LTE;		/* txrts counter in no LTE Coex case */
	uint32	txdeauth_no_LTE;	/* txdeauth counter in no LTE Coex case */
	uint32	txassocreq_no_LTE;	/* txassocreq counter in no LTE Coex case */
	uint32	txassocrsp_no_LTE;		/* txassocrsp counter in no LTE Coex case */
	uint32	txreassocreq_no_LTE;	/* txreassocreq counter in no LTE Coex case */
	uint32	txreassocrsp_no_LTE;	/* txreassocrsp counter in no LTE Coex case */
	uint32	txframe_light_LTE;	/* txframe counter in light LTE Coex case */
	uint32	txretrans_light_LTE;	/* txretrans counter in light LTE Coex case */
	uint32	rxframe_light_LTE;	/* rxframe counter in light LTE Coex case */
	uint32	rxrtry_light_LTE;	/* rxrtry counter in light LTE Coex case */
	uint32	txnocts_light_LTE;	/* txnocts counter in light LTE Coex case */
	uint32	txrts_light_LTE;	/* txrts counter in light LTE Coex case */
	uint32	txdeauth_light_LTE;	/* txdeauth counter in light LTE Coex case */
	uint32	txassocreq_light_LTE;	/* txassocreq counter in light LTE Coex case */
	uint32	txassocrsp_light_LTE;	/* txassocrsp counter in light LTE Coex case */
	uint32	txreassocreq_light_LTE;	/* txreassocreq counter in light LTE Coex case */
	uint32	txreassocrsp_light_LTE;	/* txreassocrsp counter in light LTE Coex case */
	uint32	txframe_heavy_LTE;	/* txframe counter in heavy LTE Coex case */
	uint32	txretrans_heavy_LTE;	/* txretrans counter in heavy LTE Coex case */
	uint32	rxframe_heavy_LTE;	/* rxframe counter in heavy LTE Coex case */
	uint32	rxrtry_heavy_LTE;	/* rxrtry counter in heavy LTE Coex case */
	uint32	txnocts_heavy_LTE;	/* txnocts counter in heavy LTE Coex case */
	uint32	txrts_heavy_LTE;	/* txrts counter in heavy LTE Coex case */
	uint32	txdeauth_heavy_LTE;	/* txdeauth counter in heavy LTE Coex case */
	uint32	txassocreq_heavy_LTE;	/* txassocreq counter in heavy LTE Coex case */
	uint32	txassocrsp_heavy_LTE;	/* txassocrsp counter in heavy LTE Coex case */
	uint32	txreassocreq_heavy_LTE;	/* txreassocreq counter in heavy LTE Coex case */
	uint32	txreassocrsp_heavy_LTE;	/* txreassocrsp counter in heavy LTE Coex case */

	/* LTE specific ecounters */
	uint16	type4_txinhi_dur;	/* Duration of tx inhibit(in ms) due to Type4 */
	uint16	type4_nonzero_cnt;	/* Counts of none zero Type4 msg */
	uint16	type4_timeout_cnt;	/* Counts of Type4 timeout */
	uint16	rx_pri_dur;		/* Duration of wlan_rx_pri assertions */
	uint16	rx_pri_cnt;		/* Count of wlan_rx_pri assertions */
	uint16	type6_dur;		/* duration of LTE Tx power limiting assertions */
	uint16	type6_cnt;		/* Count of LTE Tx power limiting assertions */
	uint16	ts_prot_frm_cnt;	/* count of WLAN protection frames triggered by LTE coex */
	uint16	ts_gr_cnt;		/* count of intervals granted to WLAN in timesharing */
	uint16	ts_gr_dur;		/* duration granted to WLAN in timesharing */
} wlc_ltecoex_stats_t;

/* Per channel ecounters. Repurpose existing structure definitions */
#ifdef WLC_CHAN_ECNTR_TEST
#define WL_CHAN_PERIODIC_CNTRS_VER_1 1
typedef struct wlc_chan_periodic_cntr
{
	uint16 version;
	uint16 PAD;
	uint32	rxstrt;
} wlc_chan_periodic_cntr_t;
#endif /* WLC_CHAN_ECNTR_TEST */

/* For ecounters: Per chan stats are configured as a global stats across all slices
 * Ecounters will report per-chan stats in WL_IFSTATS_XTLV_CHAN_STATS = 0x105 XTLV
 * WL_CHAN_STATS_XTLV_IOVAR_CONTAINER XTLV ids reported with iovar only
 */
enum wl_chan_stats_iovar_container_xtlv {
	WL_CHAN_STATS_XTLV_IOVAR_CONTAINER_RSVD = 0,
	WL_CHAN_STATS_XTLV_IOVAR_CONTAINER = 1,
	WL_CHAN_STATS_XTLV_IOVAR_CONTAINER_MAX
};

/* Sub tlvs for chan_counters */
enum wl_chan_stats_xtlv {
	WL_CHAN_STATS_XTLV_RSVD = 0,
	WL_CHAN_GENERIC_COUNTERS = 0x1, /* Already in use so keep it */
	WL_CHAN_STATS_XTLV_CHANSPEC_CONTAINER = WL_CHAN_GENERIC_COUNTERS,
	WL_CHAN_PERIODIC_COUNTERS = 0x2, /* Already in use so keep it */
	WL_CHAN_STATS_XTLV_MAX
};

/* WL_CHAN_STATS_XTLV_CHANSPEC_CONTAINER above carries payload below */
#define WL_CHANCNTR_HDR_VER_1	(1u)
typedef struct wlc_chan_cntr_hdr_v1 {
	uint16 version;		/* Already in use. So keep it */
	uint16 PAD;
	chanspec_t chanspec;	/* Dont add any fields above this */
	uint8 flags;		/* See bit fields defn  below */
	uint8 PAD;
	uint32 total_time;
	uint32 chan_entry_cnt;
	uint32 data[];
} wlc_chan_cntr_hdr_v1_t;

#define WL_CHANCNTR_HDR_VER_2	(2u)
typedef struct wlc_chan_cntr_hdr_v2 {
	uint16 version;		/* Already in use. So keep it */
	chanspec_t chanspec;	/* Dont add any fields above this */
	uint8 flags;		/* See bit fields defn  below */
	uint8 PAD[3];
	uint32 chan_entry_cnt;
	uint64 total_time_ns;
	uint32 data[];
} wlc_chan_cntr_hdr_v2_t;

/* flags field bit fields in structure above */
#define WL_CHAN_STATS_FLAGS_RESTART	(1u << 0)

/* channel specific XTLV stats types carried in data[] of wlc_chan_cntr_hdr structure */
enum wl_chan_stats_chanspec_xtlv {
	WL_CHAN_STATS_XTLV_CHANSPEC_RSVD = 0,
	WL_CHAN_STATS_XTLV_CHANSPEC_MACSTATS = 1,
	WL_CHAN_STATS_XTLV_CHANSPEC_MAX
};

/* Per chan stats
 * Payload on WL_CHAN_STATS_XTLV_CHANSPEC_MACSTATS
 */
typedef struct wl_chan_macstats_v1 {
	uint32 rxfrmtoolong;	/**< Received frame longer than legal limit (2346 bytes) */
	uint32 rxfrmtooshrt; /**< Received frame did not contain enough bytes for its frame type */
	uint32 rxanyerr;	/**< Any RX error that is not counted by other counters. */
	uint32 rxbadfcs;	/**< number of frames for which the CRC check failed in the MAC */
	uint32 rxbadplcp;	/**< parity check of the PLCP header failed */
	uint32 rxcrsglitch;	/**< PHY was able to correlate the preamble but not the header */
	uint32 rxstrt;		/**< Number of received frames with a good PLCP
				 * (i.e. passing parity check)
				 */
	uint32 rxrsptmout;	/**< number of response timeouts for transmitted frames
				 * expecting a response
				 */
	uint32 rxnodelim;	/**< number of no valid delimiter detected by ampdu parser */
	uint32 rxf0ovfl;	/**< number of receive fifo 0 overflows */
	uint32 rxf1ovfl;	/**< number of receive fifo 1 overflows */
	uint32 txrtsfail;	/**< number of rts transmission failure that reach retry limit */
} wl_chan_macstats_v1_t;

/* For ecounters: Per peer stats are configured for an interface. Report will contain per peer
 * stats for the configured interface. Note that some per-peer stats are split into per-slice stats
 * Top level Container types for peer stats
 * WL_PEER_STATS_XTLV_IOVAR_CONTAINER ids reported with iovar only
 */
enum wl_peer_stats_iovar_container_xtlv {
	WL_PEER_STATS_XTLV_IOVAR_CONTAINER_RSVD = 0,
	WL_PEER_STATS_XTLV_IOVAR_CONTAINER = 1,		/* version 1 */
	WL_PEER_STATS_XTLV_IOVAR_SOFTAP_CONTAINER = 2,
	WL_PEER_STATS_XTLV_IOVAR_CONTAINER_MAX
};

/* 2nd level container XTLV ids to hold per-peer stats collection
 * Once collection container will hold stats for one peer
 * One WL_PEER_STATS_XTLV_IOVAR_CONTAINER for instance can carry multiple
 * WL_PEER_STATS_XTLV_PER_PEER_COLLECTION_V1 XTLVs.
 */
enum wl_per_peer_stats_collection_xtlv {
	WL_PEER_STATS_XTLV_PER_PEER_RSVD = 0,
	WL_PEER_STATS_XTLV_PER_PEER_COLLECTION_V1 = 1,
	WL_PEER_STATS_XTLV_PER_PEER_MAX
};

/* Per peer stats structure. Payload on WL_PEER_STATS_XTLV_PER_PEER_COLLECTION_V1 */
typedef struct wl_peer_stats_per_peer_collection_v1 {
	struct ether_addr peer_ea;
	uint8 flags;
	uint8 PAD;
	uint32 peer_observation_time; /* total observation time in ms of stats reported */
	uint32 data[];
} wl_peer_stats_per_peer_collection_v1_t;
/* A flag to signal that the report instance is brand new
 * (host may use this to rebase the counter snapshot)
 */
#define WL_PEER_STATS_PER_PEER_FLAGS_RESTART	(1u << 0)
#define WL_PEER_STATS_SOFTAP_CLIENT_START	WL_PEER_STATS_PER_PEER_FLAGS_RESTART
#define WL_PEER_STATS_SOFTAP_CLIENT_IND		(1u << 1)

/* XTLV types reported within in peer stats collection structure
 * Types below are payload in data[] of peer stats collection structure above
 */
enum wl_peer_stats_xtlv {
	WL_PEER_STATS_XTLV_RSVD = 0,
	WL_PEER_STATS_XTLV_CHAN_CONTAINER_V1 = 1, /* Per chanspec container for stats */
	WL_PEER_STATS_XTLV_AMPDU_RX = 2,
	WL_PEER_STATS_XTLV_AMPDU_TX = 3,
	WL_PEER_STATS_XTLV_MAX
};

/* Per peer per chan weighted rates */
typedef struct wl_peer_chan_stats_wrates_v1 {
	int32 w_rssi;
	int32 w_snr;
	uint32 w_txrate;	/* kbps */
	uint32 w_rxrate;	/* kbps */
} wl_peer_chan_stats_wrates_v1_t;

/* per chan generic TX counters */
typedef struct wl_peer_chan_stats_tx_v1 {
	uint32 txrtsfrm;
	uint32 rxctsucast;
	uint32 txphyerror;
	uint32 txucastnoagg;		/* TX ucast with no aggregation */
	uint32 txall_butcts_frm;	/* Total tx Data, ACK, RTS, Control Management
					 * (includes retransmissions). TX CTS is not included
					 */
} wl_peer_chan_stats_tx_v1_t;

/* per chan rx and tx durations */
typedef struct wl_peer_chan_stats_duration_v1 {
	uint32 txduration; /* duration in us */
	uint32 rxduration; /* duration in us */
} wl_peer_chan_stats_duration_v1_t;

/* Per peer per chan stats
 * Per-peer per-chan container. Payload on WL_PEER_STATS_XTLV_CHAN_CONTAINER
 */
typedef struct wl_peer_chan_stats_v1 {
	chanspec_t chanspec;
	uint8 flags;
	uint8 PAD;
	uint32 chan_observation_time; /* total time in ms on a channel for a peer */
	wl_peer_chan_stats_duration_v1_t duration;
	wl_peer_chan_stats_wrates_v1_t wrates;
	wl_peer_chan_stats_tx_v1_t tx;
} wl_peer_chan_stats_v1_t;
/* A flag to signal that the report instance is brand new
 * (host may use this to rebase the counter snapshot)
 */
#define WL_PEER_STATS_PER_PEER_CHAN_FLAGS_RESTART	(1u << 0)

/* Per peer AMPDU RX
 * Payload on WL_PEER_STATS_XTLV_AMPDU_RX above
 * Reported in WL_PEER_STATS_XTLV_PER_PEER_COLLECTION_V1 container above
 */
typedef struct wl_peer_stats_ampdu_rx_v1 {
	uint32 rxampdu;		/**< ampdus recd */
	uint32 rxmpdu;		/**< mpdus recd in a ampdu */
	uint32 rxholes;		/**< missed seq numbers on rx side */
	uint32 rxdup;		/**< duplicate pdus */
	uint32 rxstuck;		/**< watchdog bailout for stuck state */
	uint32 rxoow;		/**< out of window pdus */
	uint32 rxoos;		/**< out of seq pdus */
	uint32 txback;		/**< BACKs sent */
	uint32 rxnobapol;	/**< mpdus recd without a ba policy */
	uint32 rxretrynobapol;	/**< retried mpdus rxed without a ba policy */

	uint32 rxaddbareq;	/**< addba req recd */
	uint32 txaddbaresp;	/**< addba resp sent */
	uint32 rxbar;		/**< bar recd */
	uint32 txdelba;		/**< delba sent */
	uint32 rxdelba;		/**< delba recd */
} wl_peer_stats_ampdu_rx_v1_t;

/* Per peer AMPDU TX
 * Payload on WL_PEER_STATS_XTLV_AMPDU_TX above
 * Reported in WL_PEER_STATS_XTLV_PER_PEER_COLLECTION_V1 container above
 */
typedef struct wl_peer_stats_ampdu_tx_v1 {
	uint32 txampdu;
	uint32 txmpdu;
	uint32 txucast;
	uint32 txaddbareq;
	uint32 rxaddbaresp;
	uint32 txdelba;
	uint32 rxdelba;
	uint32 txbar;
} wl_peer_stats_ampdu_tx_v1_t;

/* IOVAR parameter to FW for WL_PEER_STATS_XTLV_IOVAR_CONTAINER (i.e. v1) */
typedef struct wl_peer_stats_iovar_v1 {
	struct ether_addr	peer_mac;
	uint16			flags;
} wl_peer_stats_iovar_v1_t;
#define WL_PEER_STATS_IOVAR_FLAGS_GET			0u
#define WL_PEER_STATS_IOVAR_FLAGS_SET_START		(1u << 0)
#define WL_PEER_STATS_IOVAR_FLAGS_SET_STOP		(1u << 1)
/* SoftAP Stats flags */
#define WL_SOFTAP_STATS_IOVAR_FLAGS_GET			WL_PEER_STATS_IOVAR_FLAGS_GET
#define WL_SOFTAP_STATS_IOVAR_FLAGS_SET_START		WL_PEER_STATS_IOVAR_FLAGS_SET_START
#define WL_SOFTAP_STATS_IOVAR_FLAGS_SET_STOP		WL_PEER_STATS_IOVAR_FLAGS_SET_STOP

#define WL_DTIM_INFO_MISS_VERSION_1 1u
/* dtim miss reason count */
typedef struct wl_dtim_miss_reason_cnt_v1 {
	uint32 reason_p2p;		/* DTIM missed cnt due AWDL/NAN offchannel activity */
	uint32 reason_iovar;		/* DTIM missed cnt due offchannel actframe
					 * transmit using iovar
					 */
	uint32 reason_scan;		/* DTIM missed cnt due to channel switch during scan */
	uint32 reason_roam_assoc;	/* DTIM missed cnt due to channel switch during roam */
	uint32 reason_homechan;		/* DTIM missed cnt while in home channel */
	uint32 reason_sleep;		/* DTIM missed cnt due to FW sleep */
	uint32 reason_misc_offchan;	/* DTIM missed cnt due to misc offchan */
} wl_dtim_miss_reason_cnt_v1_t;

typedef struct wl_missed_dtim_info_ecounters_v1 {
	uint16	version;
	uint16	length;
	wl_dtim_miss_reason_cnt_v1_t dtim_miss_reason_cnt;
} wl_missed_dtim_info_ecounters_v1_t;

/* Flat structures for reporting with Ecounters */
/* Rev Ge88 RX unified macstats - version 1 */
#define WL_CNT_UCODE_MCST_UNIFIED_RX_V1	(1u)

typedef struct wl_cnt_ucode_mcst_unified_rx_v1 {
	uint16 version;
	uint16 len;

	uint32	rxstrt;			/**< Number of received frames with a good PLCP
					 * (i.e. passing parity check)
					 */
	uint32	rx20s_cnt;		/**< Increments if RXFrame does not include primary 20 */
	uint32	C_SECRSSI0;		/**< SEC RSSI0 info */
	uint32	C_SECRSSI1;		/**< SEC RSSI1 info */
	uint32	C_SECRSSI2;		/**< SEC RSSI2 info */
	uint32	C_CCA_RXPRI_LO;		/**< SEC RXPRI Low */
	uint32	C_CCA_RXPRI_HI;		/**< SEC RXPRI High */
	uint32	C_CCA_RXSEC20_LO;	/**< SEC CCA RX 20mhz low */
	uint32	C_CCA_RXSEC20_HI;	/**< SEC CCA RX 20mhz high */
	uint32	C_CCA_RXSEC40_LO;	/**< SEC CCA RX 40mhz low */
	uint32	C_CCA_RXSEC40_HI;	/**< SEC CCA RX 40mhz high */
	uint32	C_CCA_RXSEC80_LO;	/**< SEC CCA RX 80mhz low */
	uint32	C_CCA_RXSEC80_HI;	/**< SEC CCA RX 80mhz high */
	uint32	rxctlmcast;		/**< # of RX ctrl mcast frames */
	uint32	rxmgmcast;		/**< # of rx'd Management mcast frames */
	uint32	rxbeaconmbss;		/**< beacons rx'd from member of BSS */
	uint32	rxndpa_m;		/**< number of RX NDPA Multicast */
	uint32	rxrtsucast;		/**< # of ucast RTS (good FCS) */
	uint32	rxctsucast;		/**< # of ucast CTS (good FCS) */
	uint32	rxctlucast;		/**< # of rx'd CNTRL frames (good FCS & matching RA) */
	uint32	rxmgucastmbss;		/**< # of rx'd mgmt frames (good FCS & matching RA) */
	uint32	rxackucast;		/**< number of ucast ACKS received (good FCS) */
	uint32	rxndpa_u;		/**< number of unicast RX NDPAs */
	uint32	rxsf;			/**< number of rxsfucast */
	uint32	rxcwrts;		/**< number of rx'd cw ucast rts */
	uint32	rxcwcts;		/**< number of rx'd cw ucast cts */
	uint32	rxbfpoll;		/**< number of rx'd BF ucast poll */
	uint32	rxmgocast;		/**< # of rx'd MGMT frames (good FCS & not matching RA) */
	uint32	rxctlocast;		/**< # of rx'd CNTRL frame (good FCS & not matching RA) */
	uint32	rxrtsocast;		/**< # of rx'd RTS not addressed */
	uint32	rxctsocast;		/**< # of rx'd CTS not addressed */
	uint32	rxbeaconobss;		/* beacons rx'd from other BSS */
	uint32	he_rxstrt_hesuppdu_cnt;	/**< rx'd HE su PPDU cnt */
	uint32	he_rxstrt_hesureppdu_cnt; /**< rx'd HE SU RE PPDU cnt */
	uint32	he_rxtsrt_hemuppdu_cnt;	/**< rx'd HE MU PPDU cnt */
	uint32	rxbar;			/**< number of rx'd BARs */
	uint32	rxback;			/**< number of rx'd BARs */
	uint32	he_rxmtid_back;		/**< number of rx'd HE RX MultiTID BAs */
	uint32	he_rxmsta_back;		/**< number of rx'd HE RX MultiSTA BAs */
	uint32	bferpt;			/**< number of rx'd BFE report ready cnts */
	uint32	he_colormiss_cnt;	/**< HE BSS color mismatch counts cnts */
	uint32	he_rxdefrag;		/**< number of rx'd HE dynamic fragmented pkts */
	uint32	he_rxdlmu;		/**< number of rx'd DL MU frames */
	uint32	rxcgprqfrm;		/**< number of received Probe requests that made it into
					 * the PRQ fifo
					 */
	uint32	rx_fp_shm_corrupt_cnt;	/**< SHM corrupt count */
	uint32	he_physu_rx;		/**< Number of PHY SU Frames received */
	uint32	he_phyru_rx;		/**< Number of PHY RU Frames received */
	uint32	PAD[17];		/**< PAD Gap */
	uint32	rxbadplcp;		/**< parity check of the PLCP header failed */
	uint32	rxcrsglitch;		/**< PHY able to correlate the plcp but not the hdr */
	uint32	rxfrmtoolong;		/**< rx'd frame longer than legal limit (2346 bytes) */
	uint32	rxfrmtooshrt;		/**< rx'd frame not enough bytes for ft */
	uint32	rxnodelim;		/**< # of not valid delim -> ampdu parser */
	uint32	rxbad_ampdu;		/**< number of rx'd bad ampdus */
	uint32	rxcgprsqovfl;		/**< Rx Probe Request Que overflow in the AP */
	uint32	bphy_rxcrsglitch;	/**< PHY count of bphy glitches */
	uint32	rxdrop20s;		/**< drop secondary cnt */
	uint32	rxtoolate;		/**< receive too late */
	uint32	m_pfifo_drop;		/**< # of pfifo dropped frames */
	uint32	bphy_badplcp;		/**< number of bad PLCP reception on BPHY rate */
	uint32	phyovfl;		/**< number of phy overflows */
	uint32	rxf0ovfl;		/**< number of rx fifo 0 overflows */
	uint32	rxf1ovfl;		/**< number of rx fifo 1 overflows */
	uint32	lenfovfl;		/**< number of length overflows */
	uint32	weppeof;		/**< number of weppeof  */
	uint32	badplcp;		/**< parity check of the PLCP header failed */
	uint32	stsfifofull;		/**< status fifo full */
	uint32	stsfifoerr;		/**< status fifo error */
	uint32	ctx_fifo_full;		/**< fw not draining frames fast enough */
	uint32	ctx_fifo2_full;		/**< fw not draining frames fast enough */
	uint32	missbcn_dbg;		/**< number of beacon missed to receive */
	uint32	rxrsptmout;		/**< number of response timeouts for tx'd frames */
	uint32	laterx_cnt;		/**< ucode sees frame 30us late */
	uint32	bcn_drop_cnt;		/**< number of BCNs dropped in ucode */
	uint32	bfr_timeout;		/**< number of bfr timeouts */
	uint32	rxgaininfo_ant0;	/**< ANT-0 phy RX gain info - main? */
	uint32	rxauxgaininfo_ant0;	/**< ANT-0 phy RX gain info - aux */
	uint32	he_rxtrig_myaid;	/**< number of rx'd valid trigger frame with myaid */
	uint32	he_rxtrig_rand;		/**< number of rx'd valid trigger frame with random aid */
	uint32	he_rxtrig_basic;	/**< number of rx'd of basic trigger frame */
	uint32	he_rxtrig_bfm_cnt;	/**< number of rx'd trigger frame with bfm */
	uint32	he_rxtrig_mubar;	/**< number of rx'd MUBAR trigger frame variant */
	uint32	he_rxtrig_murts;	/**< number of rx'd MU-RTS trigger frame variant */
	uint32	he_rxtrig_bsrp;		/**< number of rx'd of BSR poll trigger frame variant */
	uint32	he_rxtrig_gcrmubar;	/**< number of rx'd gcr mu bar trigger frame variant? */
	uint32	he_rxtrig_bqrp;		/**< number of rx'd bqrp trigger frame variant? */
	uint32	he_rxtrig_nfrp;		/**< Todo: check on functionality */
	uint32	he_rxtrig_basic_htpack;	/**< triggers received with HTP ack policy */
	uint32	he_cs_req_tx_cancel;	/**< tx cancelled due to trigger rx or ch sw? */
	uint32	he_rxtrig_rngpoll;	/**< todo: check functionality */
	uint32	he_rxtrig_rngsnd;	/**< todo: check functionality */
	uint32	he_rxtrig_rngssnd;	/**< todo: check functionality */
	uint32	he_rxtrig_rngrpt;	/**< todo: check functionality */
	uint32	he_rxtrig_rngpasv;	/**< todo: check functionality */
	uint32	he_rxtrig_ru_2x996T;	/**< Rx'd trigger frame with STA RU index 160mhz */
	uint32	he_rxtrig_invalid_ru;	/**< Rx'd trigger frame with invalid STA20 RU index */
	uint32	he_rxtrig_inv_ru_cnt;	/**< # of Rx'd trigger frames with invalid RU cnt */
	uint32	he_rxtrig_drop_cnt;	/**< # of trigger frames dropped */
	uint32	ndp_fail_cnt;		/**< # of NDP fails */
	uint32	rxfrmtoolong2_cnt;	/**< # of Rx'd too long pkts */
	uint32	hwaci_status;		/**< HW ACI status */
	uint32	pmqovfl;		/**< number of PMQ overflows */
	uint32	sctrg_rxcrs_drop_cnt;	/**< Number of scan trigger dropped due to rxcrs */
	uint32	inv_punc_usig_cnt;	/**< Number of invalid punctured USIG */
	uint32	sctrg_drop_cnt;		/**< Number of scan trigger drop */
	uint32	he_wrong_nss;		/**< Number of triggers with wrong NSS */
	uint32	he_trig_unsupp_rate;	/**< Number of triggers with unsupported rates */

	uint32 rxdtucastmbss;		/**< # of rx'd DATA frames (good FCS & matching RA) */
	uint32 pktengrxducast;		/**< number of rx'd good fcs ucast frames */
	uint32 pktengrxdmcast;		/**< number of rx'd good fcs mcast frames */
	uint32 rxdtocast;		/**< # of rx'd DATA frames (good FCS & not matching RA) */
	uint32 rxdtucastobss;		/**< number of unicast frames addressed to the MAC from
					 * other BSS (WDS FRAME)
					 */
	uint32 goodfcs;			/**< number of rx'd goodfcs cnts */
	uint32 rxdtmcast;		/**< # of rx'd Data mcast frames */
	uint32 rxanyerr;		/**< Any RX error that is not counted by other counters */
	uint32 rxbadfcs;		/**< # of frames with CRC check failed */
} wl_cnt_ucode_mcst_unified_rx_v1_t;

/* Rev Ge88 RXERR version 1 */
#define WL_CNT_UCODE_MCST_RXERR_V1	(1u)

/* RX error related counters. Counters collected from noncontiguous SHM locations  */
typedef struct wl_cnt_ucode_mcst_rxerr_v1 {
	uint16 version;
	uint16 len;
	uint32 rx20s_cnt;		/* Increments if RXFrame does not include primary 20 */
	uint32 m_pfifo_drop;		/* ucode is late processing RX frame */
	uint32 new_rxin_plcp_wait_cnt;	/* invalid reception/ ucode late in processing rx/ something
					 * wrong over MACPHY interface
					 */
	uint32 laterx_cnt;		/* ucode sees frame 30us late */
	uint32 rxlegacyfrminvalid;	/* Invalid BPHY or L-OFDM reception */
	uint32 txsifserr;		/* Frame arrived in SIF when about to TX (B)ACK */
	uint32 ooseq_macsusp;		/* ucode out of seq in processing reception due to mac
					 * suspend
					 */
} wl_cnt_ucode_mcst_rxerr_v1_t;

/* Rev Ge88 TX unified specific macstats - version 1 */
#define WL_CNT_UCODE_MCST_UNIFIED_TX_V1	(1u)

typedef struct wl_cnt_ucode_mcst_unified_tx_v1 {
	uint16 version;
	uint16 len;

	uint32	txallfrm;			/**< num of frames sent, incl. Data, ACK, RTS, CTS,
						 * Control Management (includes retransmissions)
						 */
	uint32	txrtsfrm;			/**< number of RTS sent out by the MAC */
	uint32	txctsfrm;			/**< number of CTS sent out by the MAC */
	uint32	txackfrm;			/**< number of ACK frames sent out */
	uint32	txback;				/**< blockack txcnt */
	uint32	he_txmtid_back;			/**< number of mtid BAs */
	uint32	txdnlfrm;			/**< number of Null-Data tx from template  */
	uint32	txbcnfrm;			/**< beacons transmitted */
	uint32	txndpa;				/**< Number of TX NDPAs */
	uint32	txndp;				/**< Number of TX NDPs */
	uint32	txbfm;				/**< Number of TX Bfm cnt */
	uint32	txcwrts;			/**< Number of tx cw rts */
	uint32	txcwcts;			/**< Number of tx cw cts */
	uint32	txbfpoll;			/**< Number of tx bfpolls */
	uint32  txfbw;				/**< transmit at fallback bw (dynamic bw) */
	uint32	txampdu;			/**< number of AMPDUs transmitted */
	uint32	he_txmampdu;			/**< Number of tx m-ampdus */
	uint32	txucast;			/**< # of ucast tx expecting resp (not cts/cwcts) */
	uint32	he_txfrag;			/**< Number of tx frags */
	uint32	he_txtbppdu;			/**< increments on transmission of every TB PPDU */
	uint32	he_txtbppdu_ack;		/**< Number of tx HE TBPPDU acks */
	uint32  txinrtstxop;			/**< number of data frame tx during rts txop */
	uint32	null_txsts_empty;		/**< Number empty null-txstatus' */
	uint32	he_ulmu_disable;		/**< # of ULMU disables handled in ucode */
	uint32	he_ulmu_data_disable;		/**< number of UL MU data disable scenarios
						 * handled in ucode
						 */
	uint32	he_rxtrig_suppr_null_tbppdu;	/**<  count of null frame sent because of
						 * suppression scenarios
						 */
	uint32	he_null_zero_agg;		/**< nullAMPDU's transmitted in response to
						 * basic trigger because of zero aggregation
						 */
	uint32	he_null_tbppdu;			/**< null TBPPDU's sent as a response to
						 * basic trigger frame
						 */
	uint32	he_null_bsrp_rsp;		/**< null AMPDU's txed in response to BSR poll */
	uint32	he_null_fifo_empty;		/**< null AMPDU's in response to basic trigger
						 * because of no frames in fifo's
						 */
	uint32	txrtsfail;			/**< # of rts TX fails that reach retry limit */
	uint32	txcgprsfail;			/**< Tx Probe Response Fail.
						 * AP sent probe response but did not get ACK.
						 */
	uint32	bcntxcancl;			/**< TX bcns canceled due to rx of beacon (IBSS) */
	uint32	txtplunfl;			/**< Template unfl
						 *  (mac too slow to tx ACK/CTS or BCN)
						 */
	uint32	txphyerror;			/**< TX phyerr - reported in txs for
						 * driver queued frames
						 */
	uint32	txshmunfl_cnt;			/**< TX SHM unfl cnt */
	uint32	txfunfl[11];			/**< per-fifo tx underflows */
	uint32	txfmlunfl[12];			/**< ML fifos underflow cnts */
	uint32	bferpt_inv_cfg;			/**< Invalid bfe report cfg */
	uint32	bferpt_drop_cnt1;		/**< bfe rpt drop cnt 1 */
	uint32	bferpt_drop_cnt2;		/**< bfe rpt drop cnt 2 */
	uint32	bferot_txcrs_high;		/**< bfe rpt tx crs high */
	uint32	txbfm_errcnt;			/**< TX bfm error cnt */
	uint32	tx_murts_cnt;			/**< Tx MURTS Count */
	uint32	tx_noavail_cnt;			/**< Tx Not avail Count */
	uint32	tx_null_link_pref;		/**< Null Link Pref */
	uint32	btcx_rfact_ctr_l;		/**< btcx rxfact counter low */
	uint32	btcx_rfact_ctr_h;		/**< btcx rxfact counter high */
	uint32	btcx_txconf_ctr_l;		/**< btcx txconf counter low */
	uint32	btcx_txconf_ctr_h;		/**< btcx txconf counter high */
	uint32	btcx_txconf_dur_ctr_l;		/**< btcx txconf duration counter low */
	uint32	btcx_txconf_dur_ctr_h;		/**< btcx txconf duration counter high */
	uint32	txcgprssuc;			/**< Tx Probe Response succ cnt */
	uint32	txsf;				/**< # of Tx'd SF */
	uint32	macsusp_cnt;			/**< # of macsuspends */
	uint32	prs_timeout;			/**< # of pre wds */
	uint32	emlsr_tx_nosrt;			/**< # of no TX starts for eMLSR */
	uint32	rts_to_self_cnt;		/**< # of RTS to self */
	uint32	saqm_sendfrm_agg_cnt;		/**< # SAQM Send frame aggregation */
	uint32	txbcn_phyerr_cnt;		/**< # Tx Beacon Phy error */
	uint32	he_txtrig;			/**< # Tx Trigger Frames */

	uint32	txmpdu;				/**< number of MPDUs transmitted */
	uint32	ctmode_ufc_cnt;			/**< Number of UFCs with CT mode enabled */
} wl_cnt_ucode_mcst_unified_tx_v1_t;

/* For ecounters only */
#define HE_OMI_COUNTERS_ECNTR_V3		(3u)
typedef struct wl_he_omi_cnt_v3 {
	uint16 version;
	uint16 len;

	uint32 he_omitx_sched;          /* Count for total number of OMIs scheduled */
	uint32 he_omitx_success;        /* Count for OMI Tx success */
	uint32 he_omitx_retries;        /* Count for OMI retries as TxDone not set */
	uint32 he_omitx_dur;            /* Accumulated duration of OMI completion time */
	uint32 he_omitx_ulmucfg;        /* count for UL MU enable/disable change req */
	uint32 he_omitx_ulmucfg_ack;    /* count for UL MU enable/disable req txed successfully */
	uint32 he_omitx_txnsts;         /* count for Txnsts change req */
	uint32 he_omitx_txnsts_ack;     /* count for Txnsts change req txed successfully */
	uint32 he_omitx_rxnss;          /* count for Rxnss change req */
	uint32 he_omitx_rxnss_ack;      /* count for Rxnss change req txed successfully */
	uint32 he_omitx_bw;             /* count for BW change req */
	uint32 he_omitx_bw_ack;         /* count for BW change req txed successfully */
	uint32 he_omitx_ersudis;        /* count for ER SU enable/disable req */
	uint32 he_omitx_ersudis_ack;    /* count for ER SU enable/disable req txed successfully */
	uint32 he_omitx_dlmursdrec;	/* count for Resound recommendation change req */
	uint32 he_omitx_dlmursdrec_ack;	/* count for Resound recommendation req txed successfully */
} wl_he_omi_cnt_v3_t;

/* For ecounters only */
#define HE_TX_COUNTERS_ECNTR_V2		(2u)
/* Rev GE88 HE Tx counters (SW based) */
typedef struct wl_he_tx_cnt_v2 {
	uint16 version;
	uint16 len;
	uint32 he_mgmt_tbppdu;			/**< # Tx HE MGMT TBPPDU frames */
	uint32 he_txtbppdu_cnt[AC_COUNT];	/**< # Tx packets in each AC */
	uint32 txheru[WL_RU_TYPE_MAX];		/**< # Tx HE TBPPDU frames */
	uint32 txheru_2x996T;			/**< # Tx packets in 2x996 tone RU */
	uint32 txheru_4x996T;			/**< # Tx packets in 4x996 tone RU */
	uint32 txehtru[WL_EHT_RU_TYPE_MAX];
} wl_he_tx_cnt_v2_t;

#define HE_RX_COUNTERS_ECNTR_V2		(2u)
/* Rev GE88 HE Rx counters (SW based) */
typedef struct wl_he_rx_cnt_v2 {
	uint16 version;
	uint16 len;
	uint32 rxheru[WL_RU_TYPE_MAX];	/**< # Rx HE RU frames */
	uint32 rxheru_2x996T;		/**< # Rx packets in 2x996 tone RU */
	uint32 rxheru_4x996T;		/**< # Rx packets in 4x996 tone RU */
	uint32 he_rxtrig_ru_4x996T;	/**< Rx'd trigger frame with STA RU index 320mhz */
	uint32 rxehtru[WL_EHT_RU_TYPE_MAX];
} wl_he_rx_cnt_v2_t;

/* iov counters reporting Version 1 */
#define WL_CNT_REQ_VER_1	(1u)
#define WL_CNT_RESP_VER_1	(1u)

/* IOV Counters Flags */
#define IOV_COUNTERS_REPORTING_RESERVED		(1 << 0u) /* RESERVED bit */
#define IOV_COUNTERS_REPORTING_START		(1 << 1u) /* Start */
#define IOV_COUNTERS_REPORTING_STOP		(1 << 2u) /* Stop */
#define IOV_COUNTERS_REPORTING_CONTINUE		(1 << 3u) /* Continue */
#define IOV_COUNTERS_REPORTING_RESET		(1 << 4u) /* Reset */
#define IOV_COUNTERS_REPORTING_ALLOC		(1 << 5u) /* Alloc Memory */
#define IOV_COUNTERS_REPORTING_FREE		(1 << 6u) /* Free Counter memory */
#define IOV_COUNTERS_REPORTING_PARTIAL		(1 << 7u) /* Partial */
#define IOV_COUNTERS_REPORTING_CMPLT		(1 << 8u) /* Complete successfully */
#define IOV_COUNTERS_REPORTING_CMPLTERR		(1 << 9u) /* Complete with errors */

/* IOV Counters Req Data */
typedef struct wl_cnt_req_v1 {
	uint16  version;	/* WL_CNT_REQ_VER */
	uint16 len;
	uint32 sync_id;
	uint32 flags;
} wl_cnt_req_v1_t;

/* IOV Counters Resp Data */
typedef struct wl_cnt_resp_status_v1 {
	uint16  version;        /* WL_CNT_RESP_VER */
	uint16	len;
	uint32	sync_id;
	uint32	flags;
	uint16	idx;
	uint8	PAD[2];
} wl_cnt_resp_status_v1_t;

#define WL_SC_MULTI_SCAN_CNT_VER_V1            1u
#define WL_SC_MULTI_SCAN_FES_V1                3u

typedef struct wl_sc_multi_scan_cnts_v1 {
	uint16  version;	/* WL_SC_MULTI_SCAN_CNT_VER_V1 */
	uint16  len;
	uint32  ofdm_crs_detect;
	uint32  ofdm_be_busy;
	uint32  ofdm_false_detect;
	uint32  ofdm_cstr_timeout;
	uint32  ofdm_fstr_timeout;
	uint32  ofdm_sig1_error;
	uint32  ofdm_sig2_error;
	uint32  ofdm_filt_reject;
	uint32  ofdm_fifo_drop;
	uint32  ofdm_unsupported;
	uint32  ofdm_be_timeout;
	uint32  ofdm_fcs_fail;
	uint32  ofdm_fcs_pass;
	uint32  dsss_crs_detect;
	uint32  dsss_be_busy;
	uint32  dsss_false_detect;
	uint32  dsss_fos_timeout;
	uint32  dsss_sfd_timeout;
	uint32  dsss_phr_error;
	uint32  dsss_filt_reject;
	uint32  dsss_fifo_drop;
	uint32  dsss_unsupported;
	uint32  dsss_be_timeout;
	uint32  dsss_fcs_fail;
	uint32  dsss_fcs_pass;
	uint32  tot_queue_drop;
	uint32  tot_aborted;
	uint32	tot_be_busy;		/* Added from dsss and ofdm */
	uint32	tot_filt_reject;	/* Added from dsss and ofdm */
	uint32	tot_fifo_drop;		/* Added from dsss and ofdm */
	uint32	tot_unsupported;	/* Added from dsss and ofdm */
	uint32	tot_fcs_fail;		/* Added from dsss and ofdm */
	uint32	tot_fcs_pass;		/* Added from dsss and ofdm */
	uint32  fe_ofdm_crs_detect[WL_SC_MULTI_SCAN_FES_V1];
	uint32  fe_ofdm_be_busy[WL_SC_MULTI_SCAN_FES_V1];
	uint32  fe_ofdm_fcs_fail[WL_SC_MULTI_SCAN_FES_V1];
	uint32  fe_ofdm_fcs_pass[WL_SC_MULTI_SCAN_FES_V1];
	uint32  fe_ofdm_depri_detect[WL_SC_MULTI_SCAN_FES_V1];
	uint32  fe_ofdm_be_reassign[WL_SC_MULTI_SCAN_FES_V1];
	uint32  fe_dsss_crs_detect[WL_SC_MULTI_SCAN_FES_V1];
	uint32  fe_dsss_be_busy[WL_SC_MULTI_SCAN_FES_V1];
	uint32  fe_dsss_fcs_fail[WL_SC_MULTI_SCAN_FES_V1];
	uint32  fe_dsss_fcs_pass[WL_SC_MULTI_SCAN_FES_V1];
	uint32  fe_dsss_depri_detect[WL_SC_MULTI_SCAN_FES_V1];
	uint32  fe_dsss_be_reassign[WL_SC_MULTI_SCAN_FES_V1];
	uint32  fe_tot_aborted[WL_SC_MULTI_SCAN_FES_V1];
	uint32  fe_tot_timeout[WL_SC_MULTI_SCAN_FES_V1];
	uint32  fe_tot_reset[WL_SC_MULTI_SCAN_FES_V1];
	uint32	fe_tot_be_busy[WL_SC_MULTI_SCAN_FES_V1];	/* Added from dsss and ofdm */
	uint32	fe_tot_fcs_fail[WL_SC_MULTI_SCAN_FES_V1];	/* Added from dsss and ofdm */
	uint32	fe_tot_fcs_pass[WL_SC_MULTI_SCAN_FES_V1];	/* Added from dsss and ofdm */
} wl_sc_multi_scan_cnts_v1_t;

#define WL_SC_MULTI_SCAN_CNT_VER_V2		2u
#define WL_SC_MULTI_SCAN_FES_V2			4u

typedef struct wl_sc_multi_scan_cnts_v2 {
	uint16  version;	/* WL_SC_MULTI_SCAN_CNT_VER_V2 */
	uint16  len;
	uint32  ofdm_crs_detect;
	uint32  ofdm_be_busy;
	uint32  ofdm_false_detect;
	uint32  ofdm_cstr_timeout;
	uint32  ofdm_fstr_timeout;
	uint32  ofdm_sig1_error;
	uint32  ofdm_sig2_error;
	uint32  ofdm_filt_reject;
	uint32  ofdm_fifo_drop;
	uint32  ofdm_unsupported;
	uint32  ofdm_be_timeout;
	uint32  ofdm_fcs_fail;
	uint32  ofdm_fcs_pass;
	uint32  dsss_crs_detect;
	uint32  dsss_be_busy;
	uint32  dsss_false_detect;
	uint32  dsss_fos_timeout;
	uint32  dsss_sfd_timeout;
	uint32  dsss_phr_error;
	uint32  dsss_filt_reject;
	uint32  dsss_fifo_drop;
	uint32  dsss_unsupported;
	uint32  dsss_be_timeout;
	uint32  dsss_fcs_fail;
	uint32  dsss_fcs_pass;
	uint32  tot_queue_drop;
	uint32  tot_aborted;
	uint32	tot_be_busy;		/* Added from dsss and ofdm */
	uint32	tot_filt_reject;	/* Added from dsss and ofdm */
	uint32	tot_fifo_drop;		/* Added from dsss and ofdm */
	uint32	tot_unsupported;	/* Added from dsss and ofdm */
	uint32	tot_fcs_fail;		/* Added from dsss and ofdm */
	uint32	tot_fcs_pass;		/* Added from dsss and ofdm */
	uint32  fe_ofdm_crs_detect[WL_SC_MULTI_SCAN_FES_V2];
	uint32  fe_ofdm_be_busy[WL_SC_MULTI_SCAN_FES_V2];
	uint32  fe_ofdm_fcs_fail[WL_SC_MULTI_SCAN_FES_V2];
	uint32  fe_ofdm_fcs_pass[WL_SC_MULTI_SCAN_FES_V2];
	uint32  fe_ofdm_depri_detect[WL_SC_MULTI_SCAN_FES_V2];
	uint32  fe_ofdm_be_reassign[WL_SC_MULTI_SCAN_FES_V2];
	uint32  fe_dsss_crs_detect[WL_SC_MULTI_SCAN_FES_V2];
	uint32  fe_dsss_be_busy[WL_SC_MULTI_SCAN_FES_V2];
	uint32  fe_dsss_fcs_fail[WL_SC_MULTI_SCAN_FES_V2];
	uint32  fe_dsss_fcs_pass[WL_SC_MULTI_SCAN_FES_V2];
	uint32  fe_dsss_depri_detect[WL_SC_MULTI_SCAN_FES_V2];
	uint32  fe_dsss_be_reassign[WL_SC_MULTI_SCAN_FES_V2];
	uint32  fe_tot_aborted[WL_SC_MULTI_SCAN_FES_V2];
	uint32  fe_tot_timeout[WL_SC_MULTI_SCAN_FES_V2];
	uint32  fe_tot_reset[WL_SC_MULTI_SCAN_FES_V2];
	uint32	fe_tot_be_busy[WL_SC_MULTI_SCAN_FES_V2];	/* Added from dsss and ofdm */
	uint32	fe_tot_fcs_fail[WL_SC_MULTI_SCAN_FES_V2];	/* Added from dsss and ofdm */
	uint32	fe_tot_fcs_pass[WL_SC_MULTI_SCAN_FES_V2];	/* Added from dsss and ofdm */
} wl_sc_multi_scan_cnts_v2_t;

/* LLW stats */
typedef enum wl_llw_xtlv {
	WL_LLW_XTLV_STATS = 0,
	WL_LLW_XTLV_STATS_V2 = 1
} wl_llw_xtlv_t;

/* Session/receiver statistics */
typedef struct wl_llw_stats {
	uint32 txpkt_pri_rate;
	uint32 txpkt_fb0_rate;
	uint32 txpkt_fb1_rate;
	uint32 txpkt_fb2_rate;
	uint32 txpackets;
	uint32 txbytes;
	uint32 txnoack;
	uint32 rxpackets;
	uint32 rxbytes;
	uint32 rxretry;
} wl_llw_stats_t;

#define LLW_RX_MCS_BIN		16u
#define LLW_RX_NSS_BIN		2u
#define LLW_RX_BW_BIN		5u
#define LLW_RX_RSSI_BIN		18u
#define LLW_RX_SNR_BIN		18u

/* Session/receiver statistics V2 (WL_LLW_XTLV_STATS_V2) */
typedef struct wl_llw_stats_v2 {
	uint32 txpkt_pri_rate;
	uint32 txpkt_fb0_rate;
	uint32 txpkt_fb1_rate;
	uint32 txpkt_fb2_rate;
	uint32 txpackets;
	uint32 txbytes;
	uint32 txnoack;
	uint32 rxpackets;
	uint32 rxbytes;
	uint32 rxretry;
	uint32 rxholes;
	uint32 txpkt_wl;
	uint32 rxmpdu;
	ratespec_t rx_last_rspec;
	int16 rx_last_rssi;
	int16 rx_last_snr;
} wl_llw_stats_v2_t;

/* GCR-UR Tx stats collected in PCIEDEV */
typedef struct wl_llw_gcr_ur_pcie_tx_stats {
	uint16 version;
	uint16 length;
	uint32 txpkt_pcie;
	uint32 txdrop_pcie;
	uint32 txdrop_wl;
} wl_llw_gcr_ur_pcie_tx_stats_t;

typedef struct wl_llw_stats_hdr {
	uint16 version;
	uint16 stats_cnt;
	uint32 tot_len;
	uint8 stat_xtlvs[];
} wl_llw_stats_hdr_t;

/* WL_LLW_XTLV_STATS */
typedef struct wl_llw_stats_xtlv {
	uint16 type;
	uint16 len;
	uint8 stats[];
} wl_llw_stats_xtlv_t;

/* PHY RX counters in WL counters. SW based counters */
typedef struct wl_cnt_phy_rx_stats_block_v1 {
	uint8 stats_block_idx;
	uint8 pad[3];
	/* chup_mode0 and chup_mode1 need to be next to each other */
	uint32 chup_mode0;
	uint32 chup_mode1;
	/* dmd_mode0 and dmd_mode1 need to be next to each other */
	uint32 dmd_mode0;
	uint32 dmd_mode1;
} wl_cnt_phy_rx_stats_block_v1_t;

#define WL_CNT_PHY_RX_STATS_V1		(1u)
typedef struct wl_cnt_phy_rx_stats_v1_t {
	uint16	version;
	uint16	len;
	uint8	num_stats_blocks; /* Number of stats blocks supported on slice */
	uint8	pad[3];
	/* Per ML Link PHY RX counters (esp. eMLSR) */
	uint8	counters[];
} wl_cnt_phy_rx_stats_v1_t;

typedef struct sbi_sc_pkt_stats_v1 {
	uint32 ucast_data_cnt;          /* Ucast data pkt */
	uint32 bcmc_data_cnt;           /* BCMC data pkt */
	uint32 mgmt_pkt_cnt;            /* MGMT pkt */
	uint32 ctl_pkt_cnt;             /* CTL pkt  */
} sbi_sc_pkt_stats_v1_t;

typedef struct sbi_sc_infra_stats_v1 {
	uint32 tbtt_cnt;                /* TBTT */
	uint32 tbtt_offchan_cnt;        /* TBTT when SC is not on infra chan */
	uint32 bcn_cnt;                 /* BCN RX */
	uint32 tim_bcn_cnt;             /* BCN with TIM set */
	uint32 dtim_bcn_cnt;            /* DTIM BCN with BCMC set */
	uint32 bcn_miss_cnt;            /* BCN miss */
	uint32 bcmc_loss_evt_cnt;       /* BCMC loss after dtim */
	sbi_sc_pkt_stats_v1_t pkt_stats; /* Infra pkt stats */
} sbi_sc_infra_stats_v1_t;

typedef struct sbi_sc_stats_v1 {
	sbi_sc_infra_stats_v1_t infra_stats; /* Infra specific stats */
	sbi_sc_pkt_stats_v1_t sbss_stats;    /* SBSS specific stats */
	uint32 slot_skip_cnt;                /* Slot skip count */
} sbi_sc_stats_v1_t;

typedef struct sbi_sc_chan_stats_v1 {
	chanspec_t chan;
	uint16 flags;
	uint32 dur_ms;
	sbi_sc_stats_v1_t stats;
} sbi_sc_chan_stats_v1_t;

#define WLC_SBI_SC_STATS_CTR_FIXED_LEN_V1    OFFSETOF(wlc_sbi_sc_stats_ctr_v1_t, chstats)

#define WLC_SBI_SC_STATS_CTR_VER_V1           1u

typedef struct wlc_sbi_sc_stats_ctr_v1 {
	uint16	version;
	uint16	len;
	sbi_sc_chan_stats_v1_t chstats[];
} wlc_sbi_sc_stats_ctr_v1_t;

#define WLC_SBI_SC_AGG_STATS_VER_V1          1u
typedef struct sbi_sc_agg_stats_v1 {
	uint16 version;
	uint16 len;
	sbi_sc_stats_v1_t stats;
} sbi_sc_agg_stats_v1_t;

/* ucode counters layout change due to 11bn */
enum {
	WL_CNT_MCST_BLK_RSVD = 0,
	WL_CNT_MCST_BLK_EMLSR_0 = 1,	/* Static assignment EMLSR_0 */
	WL_CNT_MCST_BLK_EMLSR_1 = 2,	/* Static assignment EMLSR_1 */
	WL_CNT_MCST_BLK_NPCA_0 = 3,	/* Static assignment NPCA_0 */
	WL_CNT_MCST_BLK_NPCA_1 = 4,	/* Static assignment NPCA_1 */
	WL_CNT_MCST_BLK_MAX
};

/* full flat structure for reporting
 * This will updated as a compact set of ucode counters are defined for reporting to host
 */
typedef struct {
	uint16 chanspec;		/**< chanspec at the time stats were collected */
	uint8  blk_id;			/**< stats block id from where stats were collected */
	uint8  link_idx;		/**< Link idx if infra_sta is associated */

	/* 16 bit accumulated counters */
	uint32	rx20s_cnt;		/**< Increments if RXFrame does not include primary 20 */
	uint32	rxfrmtoolong;		/**< rx'd frame longer than legal limit (2346 bytes) */
	uint32	rxfrmtooshrt;		/**< rx'd frame not enough bytes for ft */
	uint32	rxnodelim;		/**< # of not valid delim -> ampdu parser */
	uint32	rxbad_ampdu;		/**< number of rx'd bad ampdus */

	uint32	rxcgprsqovfl;		/**< Rx Probe Request Que overflow in the AP */
	uint32	rxdrop20s;		/**< drop secondary cnt */
	uint32	rxtoolate;		/**< receive too late */
	uint32	m_pfifo_drop;		/**< # of pfifo dropped frames */

	/* The following need to be contiguous */
	uint32	phyovfl;		/**< number of phy overflows */
	uint32	rxf0ovfl;		/**< number of rx fifo 0 overflows */
	uint32	rxf1ovfl;		/**< number of rx fifo 1 overflows */
	uint32	lenfovfl;		/**< number of length overflows */
	uint32	weppeof;		/**< number of weppeof  */
	uint32	badplcp;		/**< parity check of the PLCP header failed */

	uint32	ctx_fifo_full;		/**< fw not draining frames fast enough */
	uint32	ctx_fifo2_full;		/**< fw not draining frames fast enough */
	uint32	missbcn_dbg;		/**< number of beacon missed to receive */
	uint32	laterx_cnt;		/**< ucode sees frame 30us late */
	uint32	he_colormiss_cnt;	/**< HE BSS color mismatch counts cnts */

	uint32	bcn_drop_cnt;		/**< number of BCNs dropped in ucode */
	uint32	bfr_timeout;		/**< number of bfr timeouts */
	uint32	rxfrmtoolong2_cnt;	/**< # of Rx'd too long pkts */
	uint32	pmqovfl;		/* Number of PMQ overflows */
	uint32	inv_punc_usig_cnt;	/**< Number of invalid punctured USIG */

	uint32	rxf0giantovfl;		/**< number of rx fifo 0 overflows */
	uint32	rxf1giantovfl;		/**< number of rx fifo 1 overflows */
	uint32	he_rxdefrag;		/**< number of rx'd HE dynamic fragmented pkts */
	uint32	rxrsptmout;		/**< number of response timeouts for tx'd frames */
} wl_cnt_mcst_rxerr_v1_t;

/* RX ERR counter reporting container */
typedef struct {
	uint8 num_blks;
	uint8 PAD[3];

	wl_cnt_mcst_rxerr_v1_t cnt_wrap[];
} wl_cnt_mcst_rxerr_container_v1_t;

/* full flat structure for reporting u32 counters */
typedef struct {
	uint16 chanspec;		/**< chanspec at the time stats were collected */
	uint8  blk_id;			/**< stats block id from where stats were collected */
	uint8  link_idx;		/**< Link idx if infra_sta is associated */

	uint32 rxanyerr;		/**< Any RX error that is not counted by other counters */
	uint32 rxbadfcs;		/**< # of frames with CRC check failed */
	uint32 rxcrsglitch;		/**< PHY able to correlate preamble but not the header */
	uint32 bphy_rxcrsglitch;	/**< PHY count of bphy glitches */
	uint32 rxbadplcp;		/**< parity check of the PLCP header failed */

	uint32 bphy_badplcp;		/**< number of bad PLCP reception on BPHY rate */
} wl_cnt_mcst_rxerr_u32_v1_t;

/* RX ERR counter reporting container */
typedef struct {
	uint8 num_blks;
	uint8 PAD[3];

	wl_cnt_mcst_rxerr_u32_v1_t cnt_wrap[];
} wl_cnt_mcst_rxerr_u32_container_v1_t;

/* full flat structure for reporting
 * This will updated as a compact set of ucode counters are defined for reporting to host
 */
typedef struct {
	uint16 chanspec;		/**< chanspec at the time stats were collected */
	uint8  blk_id;			/**< stats block id from where stats were collected */
	uint8  link_idx;		/**< Link idx if infra_sta is associated */

	/* 16-bit accumulated counters */
	uint32	he_rxtrig_myaid;	/**< number of rx'd valid trigger frame with myaid */
	uint32	he_rxtrig_rand;		/**< number of rx'd valid trigger frame with random aid */
	uint32	he_rxtrig_basic;	/**< number of rx'd of basic trigger frame */
	uint32	he_rxtrig_bfm_cnt;	/**< number of rx'd trigger frame with bfm */
	uint32	he_rxtrig_mubar;	/**< number of rx'd MUBAR trigger frame variant */

	uint32	he_rxtrig_murts;	/**< number of rx'd MU-RTS trigger frame variant */
	uint32	he_rxtrig_bsrp;		/**< number of rx'd of BSR poll trigger frame variant */
	uint32	he_rxtrig_gcrmubar;	/**< number of rx'd gcr mu bar trigger frame variant? */
	uint32	he_rxtrig_bqrp;		/**< number of rx'd bqrp trigger frame variant? */
	uint32	he_rxtrig_nfrp;		/**< Todo: check on functionality */

	uint32	he_rxtrig_basic_htpack;	/**< triggers received with HTP ack policy */
	uint32	he_cs_req_tx_cancel;	/**< tx cancelled due to trigger rx or ch sw? */
	uint32	he_rxtrig_rngpoll;	/**< todo: check functionality */
	uint32	he_rxtrig_rngsnd;	/**< todo: check functionality */
	uint32	he_rxtrig_rngssnd;	/**< todo: check functionality */

	uint32	he_rxtrig_rngrpt;	/**< todo: check functionality */
	uint32	he_rxtrig_rngpasv;	/**< todo: check functionality */
	uint32	he_rxtrig_invalid_ru;	/**< Rx'd trigger frame with invalid STA20 RU index */
	uint32	he_rxtrig_inv_ru_cnt;	/**< # of Rx'd trigger frames with invalid RU cnt */
	uint32	he_rxtrig_drop_cnt;	/**< # of trigger frames dropped */

	uint32	sctrg_rxcrs_drop_cnt;	/**< Number of scan trigger dropped due to rxcrs */
	uint32	sctrg_drop_cnt;		/**< Number of scan trigger drop */
	uint32	he_wrong_nss;		/**< Number of triggers with wrong NSS */
	uint32	he_trig_unsupp_rate;	/**< Number of triggers with unsupported rates */
	uint32	he_trig_drop_smif_cnt;

	uint32	he_trig_murts_sifs_drop_cnt;	/**< Number of MURTS seen during SIFS */
	uint32	he_trig_bsrp_wrong_nss_cnt;	/**< Number of BSRPs with incorrect NSS */
	uint32	he_trig_ru_82_83_bcc_cnt;
	uint32	he_trig_inv_dcmmcs;	/**< Number of triggers with DCM MCS */
	uint32	duo_bsrp_rx_cnt;	/**< # BSRPs RXed for DUO */
} wl_cnt_mcst_rxtb_v1_t;

/* RX TB counter reporting container */
typedef struct {
	uint8 num_blks;
	uint8 PAD[3];

	wl_cnt_mcst_rxtb_v1_t cnt_wrap[];
} wl_cnt_mcst_rxtb_container_v1_t;

/* full flat structure for reporting
 * This will updated as a compact set of ucode counters are defined for reporting to host
 */
typedef struct {
	uint16 chanspec;		/**< chanspec at the time stats were collected */
	uint8  blk_id;			/**< stats block id from where stats were collected */
	uint8  link_idx;		/**< Link idx if infra_sta is associated */

	/* 16 bit accumulated counters */
	uint32	rxstrt;			/**< Number of received frames with a good PLCP
					 * (i.e. passing parity check)
					 */
	uint32	rxctlmcast;		/**< # of RX ctrl mcast frames */
	uint32	rxmgmcast;		/**< # of rx'd Management mcast frames */
	uint32	rxbeaconmbss;		/**< beacons rx'd from member of BSS */
	uint32	rxndpa_m;		/**< number of RX NDPA Multicast */

	uint32	rxrtsucast;		/**< # of ucast RTS (good FCS) */
	uint32	rxctsucast;		/**< # of ucast CTS (good FCS) */
	uint32	rxctlucast;		/**< # of rx'd CNTRL frames (good FCS & matching RA) */
	uint32	rxmgucastmbss;		/**< # of rx'd mgmt frames (good FCS & matching RA) */
	uint32	rxackucast;		/**< number of ucast ACKS received (good FCS) */

	uint32	rxndpa_u;		/**< number of unicast RX NDPAs */
	uint32	rxsf;			/**< number of rxsfucast */
	uint32	rxcwrts;		/**< number of rx'd cw ucast rts */
	uint32	rxcwcts;		/**< number of rx'd cw ucast cts */
	uint32	rxbfpoll;		/**< number of rx'd BF ucast poll */

	uint32	rxmgocast;		/**< # of rx'd MGMT frames (good FCS & not matching RA) */
	uint32	rxctlocast;		/**< # of rx'd CNTRL frame (good FCS & not matching RA) */
	uint32	rxrtsocast;		/**< # of rx'd RTS not addressed */
	uint32	rxctsocast;		/**< # of rx'd CTS not addressed */
	uint32	rxbeaconobss;		/* beacons rx'd from other BSS */

	uint32	he_rxstrt_hesuppdu_cnt;	/**< rx'd HE su PPDU cnt */
	uint32	he_rxstrt_hesureppdu_cnt; /**< rx'd HE SU RE PPDU cnt */
	uint32	he_rxtsrt_hemuppdu_cnt;	/**< rx'd HE MU PPDU cnt */
	uint32	rxbar;			/**< number of rx'd BARs */
	uint32	rxback;			/**< number of rx'd BARs */

	uint32	he_rxmtid_back;		/**< number of rx'd HE RX MultiTID BAs */
	uint32	he_rxmsta_back;		/**< number of rx'd HE RX MultiSTA BAs */
	uint32	bferpt;			/**< number of rx'd BFE report ready cnts */
	uint32	he_rxdlmu;		/**< number of rx'd DL MU frames */
	uint32	rxcgprqfrm;		/**< number of received Probe requests that made it into
					 * the PRQ fifo
					 */
	uint32	rx_fp_shm_corrupt_cnt;	/**< SHM corrupt count */
	uint32	he_physu_rx;		/**< Number of PHY SU Frames received */
	uint32	he_phyru_rx;		/**< Number of PHY RU Frames received */
	uint32	be_muppducnt;		/**< EHT BE MU PPDU count */
	uint32	rx_uhr_elr_ppdu_cnt;	/**< UHR ELR PPDU count */
} wl_cnt_mcst_rxfrm_v1_t;

/* RXFRM counter reporting container */
typedef struct {
	uint8 num_blks;
	uint8 PAD[3];

	wl_cnt_mcst_rxfrm_v1_t cnt_wrap[];
} wl_cnt_mcst_rxfrm_container_v1_t;

/* full flat structure for reporting
 * This will updated as a compact set of ucode counters are defined for reporting to host
 */
typedef struct {
	uint16 chanspec;	/**< chanspec at the time stats were collected */
	uint8  blk_id;		/**< stats block id from where stats were collected */
	uint8  link_idx;	/**< Link idx if infra_sta is associated */

	/* 32 bit accumulated counters */
	uint32 rxdtucastmbss;	/**< # of rx'd DATA frames (good FCS & matching RA) */
	uint32 pktengrxducast;	/**< number of rx'd good fcs ucast frames */
	uint32 pktengrxdmcast;	/**< number of rx'd good fcs mcast frames */
	uint32 rxdtocast;	/**< # of rx'd DATA frames (good FCS & not matching RA) */
	uint32 rxdtucastobss;	/**< number of unicast frames addressed to the MAC from
				 * other BSS (WDS FRAME)
				 */
	uint32 goodfcs;		/**< number of rx'd goodfcs cnts */
	uint32 rxdtmcast;	/**< # of rx'd Data mcast frames */
} wl_cnt_mcst_rxfrm_u32_v1_t;

/* RX FRM u32 counter reporting container */
typedef struct {
	uint8 num_blks;
	uint8 PAD[3];

	wl_cnt_mcst_rxfrm_u32_v1_t cnt_wrap[];
} wl_cnt_mcst_rxfrm_u32_container_v1_t;

/* full flat structure for reporting
 * This will updated as a compact set of ucode counters are defined for reporting to host
 */
typedef struct {
	uint16 chanspec;			/**< chanspec at the time stats were collected */
	uint8  blk_id;				/**< stats block id from where stats were taken */
	uint8  link_idx;			/**< Link idx if infra_sta is associated */

	/* 16-bit accumulated counters */
	uint32	he_txtbppdu;			/**< increments on transmission of every TB PPDU */
	uint32	he_txtbppdu_ack;		/**< Number of tx HE TBPPDU acks */
	uint32	null_txsts_empty;		/**< Number empty null-txstatus' */
	uint32	he_ulmu_disable;		/**< # of ULMU disables handled in ucode */
	uint32	he_ulmu_data_disable;		/**< number of UL MU data disable scenarios
						 * handled in ucode
						 */
	uint32	he_rxtrig_suppr_null_tbppdu;	/**<  count of null frame sent because of
						 * suppression scenarios
						 */
	uint32	he_null_zero_agg;		/**< nullAMPDU's transmitted in response to
						 * basic trigger because of zero aggregation
						 */
	uint32	he_null_tbppdu;			/**< null TBPPDU's sent as a response to
						 * basic trigger frame
						 */
	uint32	he_null_bsrp_rsp;		/**< null AMPDU's txed in response to BSR poll */
	uint32	he_null_fifo_empty;		/**< null AMPDU's in response to basic trigger
						 * because of no frames in fifo's
						 */
	uint32	tx_murts_cnt;			/**< Tx MURTS Count */
	uint32	tx_null_link_pref;		/**< Null Link Pref */
	uint32	he_txtrig;			/**< # Tx Trigger Frames */
} wl_cnt_mcst_txtb_v1_t;

/* RX TB counter reporting container */
typedef struct {
	uint8 num_blks;
	uint8 PAD[3];

	wl_cnt_mcst_txtb_v1_t cnt_wrap[];
} wl_cnt_mcst_txtb_container_v1_t;

/* full flat structure for reporting
 * This will updated as a compact set of ucode counters are defined for reporting to host
 */
typedef struct {
	uint16 chanspec;		/**< chanspec at the time stats were collected */
	uint8  blk_id;			/**< stats block id from where stats were collected */
	uint8  link_idx;		/**< Link idx if infra_sta is associated */

	/* 16-bit accumulated counters */
	uint32	txrtsfail;		/**< # of rts TX fails that reach retry limit */
	uint32	txcgprsfail;		/**< Tx Probe Response Fail.
					 * AP sent probe response but did not get ACK.
					 */
	uint32	bcntxcancl;		/**< TX bcns canceled due to rx of beacon (IBSS) */
	uint32	txtplunfl;		/**< Template unfl
					 *  (mac too slow to tx ACK/CTS or BCN)
					 */
	uint32	txphyerror;		/**< TX phyerr - reported in txs for
					 * driver queued frames
					 */
	uint32	rspfrm_ed_txcncl_cnt;	/**< Resp frame TX cancle due to ED */
	uint32	txfunfl[11];		/**< per-fifo tx underflows */
	uint32	txfmlunfl[12];		/**< ML fifos underflow cnts */
	uint32	bferpt_inv_cfg;		/**< Invalid bfe report cfg */
	uint32	bferpt_drop_cnt1;	/**< bfe rpt drop cnt 1 */

	uint32	bferpt_drop_cnt2;	/**< bfe rpt drop cnt 2 */
	uint32	bferot_txcrs_high;	/**< bfe rpt tx crs high */
	uint32	emlsr_tx_nosrt;		/**< # of no TX starts for eMLSR */
	uint32	txbcn_phyerr_cnt;	/**< # Tx Beacon Phy error */
	uint32	emlsr_bcndrop;		/**< eMLSR bcn drop count */

	uint32	bferpt_drop_cnt3;	/**< bfe rpt drop cnt 3 */
} wl_cnt_mcst_txerr_v1_t;

/* RX TB counter reporting container */
typedef struct {
	uint8 num_blks;
	uint8 PAD[3];

	wl_cnt_mcst_txerr_v1_t cnt_wrap[];
} wl_cnt_mcst_txerr_container_v1_t;

/* full flat structure for reporting u32 counters */
typedef struct {
	uint16 chanspec;		/**< chanspec at the time stats were collected */
	uint8  blk_id;			/**< stats block id from where stats were collected */
	uint8  link_idx;		/**< Link idx if infra_sta is associated */

	uint32	ctmode_ufc_cnt;		/**< Number of UFCs with CT mode enabled */
} wl_cnt_mcst_txerr_u32_v1_t;

/* RX ERR counter reporting container */
typedef struct {
	uint8 num_blks;
	uint8 PAD[3];

	wl_cnt_mcst_txerr_u32_v1_t cnt_wrap[];
} wl_cnt_mcst_txerr_u32_container_v1_t;

/* TX FRM full flat structure for reporting
 * This will updated as a compact set of ucode counters are defined for reporting to host
 */
typedef struct {
	uint16 chanspec;		/**< chanspec at the time stats were collected */
	uint8  blk_id;			/**< stats block id from where stats were collected */
	uint8  link_idx;		/**< Link idx if infra_sta is associated */

	/* 16-bit accumulated counters */
	uint32	txrtsfrm;		/**< number of RTS sent out by the MAC */
	uint32	txctsfrm;		/**< number of CTS sent out by the MAC */
	uint32	txackfrm;		/**< number of ACK frames sent out */
	uint32	txback;			/**< blockack txcnt */
	uint32	he_txmtid_back;		/**< number of mtid BAs */

	uint32	txdnlfrm;		/**< number of Null-Data tx from template  */
	uint32	txbcnfrm;		/**< beacons transmitted */
	uint32	txndpa;			/**< Number of TX NDPAs */
	uint32	txndp;			/**< Number of TX NDPs */
	uint32	txbfm;			/**< Number of TX Bfm cnt */

	uint32	txcwrts;		/**< Number of tx cw rts */
	uint32	txcwcts;		/**< Number of tx cw cts */
	uint32	txbfpoll;		/**< Number of tx bfpolls */
	uint32  txfbw;			/**< transmit at fallback bw (dynamic bw) */
	uint32	txampdu;		/**< number of AMPDUs transmitted */

	uint32	he_txmampdu;		/**< Number of tx m-ampdus */
	uint32	txucast;		/**< # of ucast tx expecting resp (not cts/cwcts) */
	uint32	he_txfrag;		/**< Number of tx frags */
	uint32  txinrtstxop;		/**< number of data frame tx during rts txop */
	uint32	txcgprssuc;		/**< Tx Probe Response succ cnt */

	uint32	txsf;			/**< # of Tx'd SF */
	uint32	rts_to_self_cnt;	/**< # of RTS to self */
	uint32	saqm_sendfrm_agg_cnt;	/**< # SAQM Send frame aggregation */
	uint32	multista_ba_tx_cnt;	/**< # MBA sent */
	uint32	bsrp_tx_cnt;		/**< # BSRP sent */
	uint32	duo_tx_icf_cnt;		/**< # ICFs sent for DUO feature */
	uint32	duo_tx_icr_cnt;		/**< # ICRs sent for DUO feature */
	uint32	duo_fw_req_succ_cnt;	/**< # FW DUO reqs successfully handled */
	uint32	duo_coex_req_succ_cnt;	/**< # COEX DUO reqs successfully handled */
} wl_cnt_mcst_txfrm_v1_t;

/* RX TB counter reporting container */
typedef struct {
	uint8 num_blks;
	uint8 PAD[3];

	wl_cnt_mcst_txfrm_v1_t cnt_wrap[];
} wl_cnt_mcst_txfrm_container_v1_t;

/* full flat structure for reporting u32 counters */
typedef struct {
	uint16 chanspec;		/**< chanspec at the time stats were collected */
	uint8  blk_id;			/**< stats block id from where stats were collected */
	uint8  link_idx;		/**< Link idx if infra_sta is associated */

	uint32	txallfrm;		/**< num of frames sent, incl. Data, ACK, RTS, CTS,
					 * Control Management (includes retransmissions)
					 */
	uint32	txmpdu;			/**< number of MPDUs transmitted */
} wl_cnt_mcst_txfrm_u32_v1_t;

/* TX FRM u32 counter reporting container */
typedef struct {
	uint8 num_blks;
	uint8 PAD[3];

	wl_cnt_mcst_txfrm_u32_v1_t cnt_wrap[];
} wl_cnt_mcst_txfrm_u32_container_v1_t;

/* Scan core related structs */
/* full flat structure for reporting
 * This will updated as a compact set of ucode counters are defined for reporting to host
 */
typedef struct {
	uint16 chanspec;		/**< chanspec at the time stats were collected */
	uint8  blk_id;			/**< stats block id from where stats were collected */
	uint8  link_idx;		/**< Link idx if infra_sta is associated */

	/* 16 bit accumulated counters */
	uint32	rx20s_cnt;		/**< Increments if RXFrame does not include primary 20 */
	uint32	rxfrmtoolong;		/**< rx'd frame longer than legal limit (2346 bytes) */
	uint32	rxfrmtooshrt;		/**< rx'd frame not enough bytes for ft */
	uint32	rxnodelim;		/**< # of not valid delim -> ampdu parser */
	uint32	rxbad_ampdu;		/**< number of rx'd bad ampdus */

	uint32	rxcgprsqovfl;		/**< Rx Probe Request Que overflow in the AP */
	uint32	bphy_rxcrsglitch;	/**< PHY count of bphy glitches */
	uint32	rxdrop20s;		/**< drop secondary cnt */
	uint32	rxtoolate;		/**< receive too late */
	uint32	m_pfifo_drop;		/**< # of pfifo dropped frames */

	/* The following need to be contiguous */
	uint32	phyovfl;		/**< number of phy overflows */
	uint32	rxf0ovfl;		/**< number of rx fifo 0 overflows */
	uint32	rxf1ovfl;		/**< number of rx fifo 1 overflows */
	uint32	lenfovfl;		/**< number of length overflows */
	uint32	weppeof;		/**< number of weppeof  */
	uint32	badplcp;		/**< parity check of the PLCP header failed */

	uint32	ctx_fifo_full;		/**< fw not draining frames fast enough */
	uint32	ctx_fifo2_full;		/**< fw not draining frames fast enough */
	uint32	missbcn_dbg;		/**< number of beacon missed to receive */
	uint32	laterx_cnt;		/**< ucode sees frame 30us late */
	uint32	he_colormiss_cnt;	/**< HE BSS color mismatch counts cnts */

	uint32	rxfrmtoolong2_cnt;	/**< # of Rx'd too long pkts */
	uint32 rxanyerr;	/**< Any RX error that is not counted by other counters */
	uint32 rxbadfcs;	/**< # of frames with CRC check failed */
	uint32	rxbadplcp;	/**< parity check of the PLCP header failed */
	uint32	rxcrsglitch;	/**< PHY was able to correlate the preamble but not the header */
	uint32  bphy_badplcp;	/**< number of bad PLCP reception on BPHY rate */
} wl_cnt_mcst_sc_rxerr_v1_t;

/* RX ERR counter reporting container */
typedef struct {
	uint8 num_blks;
	uint8 PAD[3];

	wl_cnt_mcst_sc_rxerr_v1_t cnt_wrap[];
} wl_cnt_mcst_sc_rxerr_container_v1_t;

/* full flat structure for reporting
 * This will updated as a compact set of ucode counters are defined for reporting to host
 */
typedef struct {
	uint16 chanspec;		/**< chanspec at the time stats were collected */
	uint8  blk_id;			/**< stats block id from where stats were collected */
	uint8  link_idx;		/**< Link idx if infra_sta is associated */

	/* 16-bit accumulated counters */
	uint32	he_rxtrig_myaid;	/**< number of rx'd valid trigger frame with myaid */
	uint32	he_rxtrig_rand;		/**< number of rx'd valid trigger frame with random aid */
	uint32	he_rxtrig_basic;	/**< number of rx'd of basic trigger frame */
	uint32	he_rxtrig_bfm_cnt;	/**< number of rx'd trigger frame with bfm */
	uint32	he_rxtrig_mubar;	/**< number of rx'd MUBAR trigger frame variant */

	uint32	he_rxtrig_murts;	/**< number of rx'd MU-RTS trigger frame variant */
	uint32	he_rxtrig_bsrp;		/**< number of rx'd of BSR poll trigger frame variant */
	uint32	he_rxtrig_gcrmubar;	/**< number of rx'd gcr mu bar trigger frame variant? */
	uint32	he_rxtrig_bqrp;		/**< number of rx'd bqrp trigger frame variant? */
	uint32	he_rxtrig_nfrp;		/**< Todo: check on functionality */

	uint32	he_rxtrig_invalid_ru;	/**< Rx'd trigger frame with invalid STA20 RU index */
	uint32	he_rxtrig_inv_ru_cnt;	/**< # of Rx'd trigger frames with invalid RU cnt */
	uint32	he_rxtrig_drop_cnt;	/**< # of trigger frames dropped */
	uint32	he_rxtrig_min_len_cnt;
	uint32	he_rxtrig_drop_nav_active_cnt;

	uint32	he_rxtrig_drop_main_sleep_cnt;
	uint32	duo_bsrp_rx_cnt;	/**< # BSRPs RXed for DUO */
} wl_cnt_mcst_sc_rxtb_v1_t;

/* RX TB counter reporting container */
typedef struct {
	uint8 num_blks;
	uint8 PAD[3];

	wl_cnt_mcst_sc_rxtb_v1_t cnt_wrap[];
} wl_cnt_mcst_sc_rxtb_container_v1_t;

/* full flat structure for reporting
 * This will updated as a compact set of ucode counters are defined for reporting to host
 */
typedef struct {
	uint16 chanspec;		/**< chanspec at the time stats were collected */
	uint8  blk_id;			/**< stats block id from where stats were collected */
	uint8  link_idx;		/**< Link idx if infra_sta is associated */

	/* 16 bit accumulated counters */
	uint32	rxstrt;			/**< Number of received frames with a good PLCP
					 * (i.e. passing parity check)
					 */
	uint32	rxctlmcast;		/**< # of RX ctrl mcast frames */
	uint32	rxmgmcast;		/**< # of rx'd Management mcast frames */
	uint32	rxbeaconmbss;		/**< beacons rx'd from member of BSS */
	uint32	rxndpa_m;		/**< number of RX NDPA Multicast */

	uint32	rxrtsucast;		/**< # of ucast RTS (good FCS) */
	uint32	rxctsucast;		/**< # of ucast CTS (good FCS) */
	uint32	rxctlucast;		/**< # of rx'd CNTRL frames (good FCS & matching RA) */
	uint32	rxmgucastmbss;		/**< # of rx'd mgmt frames (good FCS & matching RA) */
	uint32	rxackucast;		/**< number of ucast ACKS received (good FCS) */

	uint32	rxndpa_u;		/**< number of unicast RX NDPAs */
	uint32	rxsf;			/**< number of rxsfucast */
	uint32	rxcwrts;		/**< number of rx'd cw ucast rts */
	uint32	rxcwcts;		/**< number of rx'd cw ucast cts */
	uint32	rxbfpoll;		/**< number of rx'd BF ucast poll */

	uint32	rxmgocast;		/**< # of rx'd MGMT frames (good FCS & not matching RA) */
	uint32	rxctlocast;		/**< # of rx'd CNTRL frame (good FCS & not matching RA) */
	uint32	rxrtsocast;		/**< # of rx'd RTS not addressed */
	uint32	rxctsocast;		/**< # of rx'd CTS not addressed */
	uint32	rxbeaconobss;		/* beacons rx'd from other BSS */

	uint32	rxbar;			/**< number of rx'd BARs */
	uint32	rxback;			/**< number of rx'd BARs */
	uint32	he_rxmtid_back;		/**< number of rx'd HE RX MultiTID BAs */
	uint32	he_rxmsta_back;		/**< number of rx'd HE RX MultiSTA BAs */
	uint32	he_rxdlmu;		/**< number of rx'd DL MU frames */

	uint32	rxcgprqfrm;		/**< number of received Probe requests that made it into
					 * the PRQ fifo
					 */
	uint32 rxdtucastmbss;		/**< # of rx'd DATA frames (good FCS & matching RA) */
	uint32 pktengrxducast;		/**< number of rx'd good fcs ucast frames */
	uint32 pktengrxdmcast;		/**< number of rx'd good fcs mcast frames */
	uint32 rxdtocast;		/**< # of rx'd DATA frames (good FCS & not matching RA) */

	uint32 rxdtucastobss;		/**< number of unicast frames addressed to the MAC from
					 * other BSS (WDS FRAME)
					 */
	uint32 goodfcs;			/**< number of rx'd goodfcs cnts */
	uint32 rxdtmcast;		/**< # of rx'd Data mcast frames */
} wl_cnt_mcst_sc_rxfrm_v1_t;

/* RXFRM counter reporting container */
typedef struct {
	uint8 num_blks;
	uint8 PAD[3];

	wl_cnt_mcst_sc_rxfrm_v1_t cnt_wrap[];
} wl_cnt_mcst_sc_rxfrm_container_v1_t;
#endif /* _wlioctl_counters_h_ */
