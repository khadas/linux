/*
 * PHY related EVENT_LOG System Definitions
 *
 * This file describes the payloads of PHY related event log entries that are data buffers
 * rather than formatted string entries.
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

#ifndef _PHY_EVENT_LOG_PAYLOAD_H_
#define _PHY_EVENT_LOG_PAYLOAD_H_

#include <typedefs.h>
#include <bcmutils.h>
#include <ethernet.h>
#include <event_log_tag.h>
#include <wlioctl.h>

typedef struct {
	uint32	txallfrm;	/**< total number of frames sent, incl. Data, ACK, RTS, CTS,
				* Control Management (includes retransmissions)
				*/
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
	uint32	rxanyerr;	/**< Any RX error that is not counted by other counters. */
	uint32	rxbeaconmbss;	/**< beacons received from member of BSS */
	uint32	rxdtucastmbss;	/**< number of received DATA frames with good FCS and matching RA */
	uint32	rxdtocast;	/**< number of received DATA frames (good FCS and no matching RA) */
	uint32  rxtoolate;	/**< receive too late */
	uint32  goodfcs;        /**< Good fcs counters  */
	uint32  rxf0ovfl;	/** < Rx FIFO0 overflow counters information */
	uint32  rxf1ovfl;	/** < Rx FIFO1 overflow counters information */
} phy_periodic_counters_v1_t;

typedef struct {
	uint32	txallfrm;		/**< total number of frames sent, incl. Data, ACK, RTS,
					* CTS, Control Management
					*  (includes retransmissions)
					*/
	uint32	rxrsptmout;		/**< number of response timeouts for transmitted frames
								* expecting a response
								*/
	uint32	rxstrt;				/**< number of received frames with a good PLCP */
	uint32  rxbadplcp;	/**< number of parity check of the PLCP header failed */
	uint32  rxcrsglitch;	/**< PHY was able to correlate the preamble but not the header */
	uint32  rxnodelim;	/**< number of no valid delimiter detected by ampdu parser */
	uint32  bphy_badplcp;	/**< number of bad PLCP reception on BPHY rate */
	uint32  bphy_rxcrsglitch;	/**< PHY count of bphy glitches */
	uint32  rxbadfcs;	/**< number of frames for which the CRC check failed in the MAC */
	uint32	rxanyerr;	/**< Any RX error that is not counted by other counters. */
	uint32	rxbeaconmbss;	/**< beacons received from member of BSS */
	uint32	rxdtucastmbss;	/**< number of received DATA frames with good FCS and matching RA */
	uint32	rxdtocast;	/**< number of received DATA frames (good FCS and no matching RA) */
	uint32  rxtoolate;	/**< receive too late */
	uint32  goodfcs;	/**< Good fcs counters  */
	uint32  rxf0ovfl;	/** < Rx FIFO0 overflow counters information */
	uint32  rxf1ovfl;	/** < Rx FIFO1 overflow counters information */

	uint32 txctl;
	uint32 rxctl;
	uint32 txbar;
	uint32 rxbar;
	uint32 rxbeaconbss;

	uint32 txrts;
	uint32	txucast;	/**< number of unicast tx expecting response other than cts/cwcts */
	uint32	rxackucast;	/**< number of ucast ACKS received (good FCS) */
	uint32	txackfrm;	/**< number of ACK frames sent out */

	uint32	txrtsfrm;	/**< number of RTS sent out by the MAC */
	uint32	txrtsfail;	/**< number of rts transmission failure that reach retry limit */
	uint32	rxctsucast;	/**< number of unicast CTS addressed to the MAC (good FCS) */

	uint32 rxrtsucast;	/**< number of unicast RTS addressed to the MAC (good FCS) */
	uint32	txctsfrm;	/**< number of CTS sent out by the MAC */

	uint32	rxback;		/**< blockack rxcnt */
	uint32	txback;		/**< blockack txcnt */
	uint32	rxctlucast;	/**< number of received CNTRL frames
						* with good FCS and matching RA
						*/

	uint32 last_bcn_seq_num;	/**< * L_TSF and Seq # from the last beacon received */
	uint32 last_bcn_ltsf;		/**< * L_TSF and Seq # from the last beacon received */

	uint16 counter_noise_request;	/**< counters of requesting noise samples for noisecal> */
	uint16 counter_noise_crsbit;	/**< counters of noise mmt being interrupted
									* due to PHYCRS>
									*/
	uint16 counter_noise_apply;		/**< counts of applying noisecal result> */

	uint8 phylog_noise_mode;		/**< noise cal mode> */
	uint8 total_desense_on;			/**< total desense on flag> */
	uint8 initgain_desense;			/**< initgain desense when total desense is on> */
	uint8 crsmin_init;		/**< crsmin_init threshold when total desense is on> */
	uint8 lna1_gainlmt_desense;	/**< lna1_gain limit desense when applying desense> */
	uint8 clipgain_desense0;		/**< clipgain desense when applying desense > */

	uint32 desense_reason;			/**< desense paraemters to indicate reasons
									* for bphy and ofdm_desense>
									*/
	uint8 lte_ofdm_desense;			/**< ofdm desense dut to lte> */
	uint8 lte_bphy_desense;			/**< bphy desense due to lte> */

	uint8 hw_aci_status;			/**< HW ACI status flag> */
	uint8 aci_clipgain_desense0;		/**< clipgain desense due to aci> */
	uint8 lna1_tbl_desense;			/**< LNA1 table desense due to aci> */

	int8 crsmin_high;			/**< crsmin high when applying desense> */
	int8 weakest_rssi;			/**< weakest link RSSI> */
	int8 phylog_noise_pwr_array[8][2];	/**< noise buffer array> */
} phy_periodic_counters_v5_t;

typedef struct {

	/* RX error related */
	uint32	rxrsptmout;	/* number of response timeouts for transmitted frames
				* expecting a response
				*/
	uint32	rxbadplcp;	/* number of parity check of the PLCP header failed */
	uint32	rxcrsglitch;	/* PHY was able to correlate the preamble but not the header */
	uint32	rxnodelim;	/* number of no valid delimiter detected by ampdu parser */
	uint32	bphy_badplcp;	/* number of bad PLCP reception on BPHY rate */
	uint32	bphy_rxcrsglitch;	/* PHY count of bphy glitches */
	uint32	rxbadfcs;	/* number of frames for which the CRC check failed in the MAC */
	uint32  rxtoolate;	/* receive too late */
	uint32  rxf0ovfl;	/* Rx FIFO0 overflow counters information */
	uint32  rxf1ovfl;	/* Rx FIFO1 overflow counters information */
	uint32	rxanyerr;	/* Any RX error that is not counted by other counters. */
	uint32	rxdropped;	/* Frame dropped */
	uint32	rxnobuf;	/* Rx error due to no buffer */
	uint32	rxrunt;		/* Runt frame counter */
	uint32	rxfrmtoolong;	/* Number of received frame that are too long */
	uint32	rxdrop20s;

	/* RX related */
	uint32	rxstrt;		/* number of received frames with a good PLCP */
	uint32	rxbeaconmbss;	/* beacons received from member of BSS */
	uint32	rxdtucastmbss;	/* number of received DATA frames with good FCS and matching RA */
	uint32	rxdtocast;	/* number of received DATA frames (good FCS and no matching RA) */
	uint32  goodfcs;        /* Good fcs counters  */
	uint32	rxctl;		/* Number of control frames */
	uint32	rxaction;	/* Number of action frames */
	uint32	rxback;		/* Number of block ack frames rcvd */
	uint32	rxctlucast;	/* Number of received unicast ctl frames */
	uint32	rxframe;	/* Number of received frames */

	/* TX related */
	uint32	txallfrm;	/* total number of frames sent, incl. Data, ACK, RTS, CTS,
				* Control Management (includes retransmissions)
				*/
	uint32	txmpdu;			/* Numer of transmitted mpdus */
	uint32	txackbackctsfrm;	/* Number of ACK + BACK + CTS */

	/* TX error related */
	uint32	txrtsfail;		/* RTS TX failure count */
	uint32	txphyerr;		/* PHY TX error count */

	uint16	nav_cntr_l;		/* The state of the NAV */
	uint16	nav_cntr_h;
} phy_periodic_counters_v3_t;

typedef struct phy_periodic_counters_v4 {
	uint32	txallfrm;	/**< total number of frames sent, incl. Data, ACK, RTS, CTS,
				* Control Management (includes retransmissions)
				*/
	uint32	rxrsptmout;	/**< number of response timeouts for transmitted frames
				* expecting a response
				*/
	uint32	rxstrt;		/**< number of received frames with a good PLCP */
	uint32  rxbadplcp;	/**< number of parity check of the PLCP header failed */
	uint32  rxcrsglitch;	/**< PHY was able to correlate the preamble but not the header */
	uint32  bphy_badplcp;	/**< number of bad PLCP reception on BPHY rate */
	uint32  bphy_rxcrsglitch;	/**< PHY count of bphy glitches */
	uint32	rxbeaconmbss;	/**< beacons received from member of BSS */
	uint32	rxdtucastmbss;	/**< number of received DATA frames with good FCS and matching RA */
	uint32  rxf0ovfl;	/** < Rx FIFO0 overflow counters information */
	uint32  rxf1ovfl;	/** < Rx FIFO1 overflow counters information */
	uint32	rxdtocast;	/**< number of received DATA frames (good FCS and no matching RA) */
	uint32  rxtoolate;	/**< receive too late */
	uint32  rxbadfcs;	/**< number of frames for which the CRC check failed in the MAC */
	uint32  rxdropped;
	uint32  rxcrc;
	uint32  rxnobuf;
	uint32  rxrunt;
	uint32  rxgiant;
	uint32  rxctl;
	uint32  rxaction;
	uint32  rxdrop20s;
	uint32  rxctsucast;
	uint32  rxrtsucast;
	uint32  txctsfrm;
	uint32  rxackucast;
	uint32  rxback;
	uint32  txphyerr;
	uint32  txrtsfrm;
	uint32  txackfrm;
	uint32  txback;
	uint32	rxnodelim;
	uint32	rxfrmtoolong;
	uint32	rxctlucast;
	uint32	txbcnfrm;
	uint32	txdnlfrm;
	uint32	txampdu;
	uint32	txmpdu;
	uint32  txinrtstxop;
	uint32	prs_timeout;
} phy_periodic_counters_v4_t;

/* phy_periodic_counters_v5_t  reserved for 4378 */
typedef struct phy_periodic_counters_v6 {
	/* RX error related */
	uint32	rxrsptmout;	/* number of response timeouts for transmitted frames
				* expecting a response
				*/
	uint32	rxbadplcp;	/* number of parity check of the PLCP header failed */
	uint32	rxcrsglitch;	/* PHY was able to correlate the preamble but not the header */
	uint32	rxnodelim;	/* number of no valid delimiter detected by ampdu parser */
	uint32	bphy_badplcp;	/* number of bad PLCP reception on BPHY rate */
	uint32	bphy_rxcrsglitch;	/* PHY count of bphy glitches */
	uint32	rxbadfcs;	/* number of frames for which the CRC check failed in the MAC */
	uint32  rxtoolate;	/* receive too late */
	uint32  rxf0ovfl;	/* Rx FIFO0 overflow counters information */
	uint32  rxf1ovfl;	/* Rx FIFO1 overflow counters information */
	uint32	rxanyerr;	/* Any RX error that is not counted by other counters. */
	uint32	rxdropped;	/* Frame dropped */
	uint32	rxnobuf;	/* Rx error due to no buffer */
	uint32	rxrunt;		/* Runt frame counter */
	uint32	rxfrmtoolong;	/* Number of received frame that are too long */
	uint32	rxdrop20s;

	/* RX related */
	uint32	rxstrt;		/* number of received frames with a good PLCP */
	uint32	rxbeaconmbss;	/* beacons received from member of BSS */
	uint32	rxdtucastmbss;	/* number of received DATA frames with good FCS and matching RA */
	uint32	rxdtocast;	/* number of received DATA frames (good FCS and no matching RA) */
	uint32  goodfcs;        /* Good fcs counters  */
	uint32	rxctl;		/* Number of control frames */
	uint32	rxaction;	/* Number of action frames */
	uint32	rxback;		/* Number of block ack frames rcvd */
	uint32	rxctlucast;	/* Number of received unicast ctl frames */
	uint32	rxframe;	/* Number of received frames */

	uint32	rxbar;		/* Number of block ack requests rcvd */
	uint32	rxackucast;	/* number of ucast ACKS received (good FCS) */
	uint32	rxbeaconobss;	/* number of OBSS beacons received */
	uint32	rxctsucast;	/* number of unicast CTS addressed to the MAC (good FCS) */
	uint32	rxrtsucast;	/* number of unicast RTS addressed to the MAC (good FCS) */

	/* TX related */
	uint32	txallfrm;	/* total number of frames sent, incl. Data, ACK, RTS, CTS,
				* Control Management (includes retransmissions)
				*/
	uint32	txmpdu;			/* Numer of transmitted mpdus */
	uint32	txackbackctsfrm;	/* Number of ACK + BACK + CTS */
	uint32	txackfrm;	/* number of ACK frames sent out */
	uint32	txrtsfrm;	/* number of RTS sent out by the MAC */
	uint32	txctsfrm;	/* number of CTS sent out by the MAC */

	uint32	txctl;		/* Number of control frames txd */
	uint32	txbar;		/* Number of block ack requests txd */
	uint32	txrts;		/* Number of RTS txd */
	uint32	txback;		/* Number of block ack frames txd */
	uint32	txucast;	/* number of unicast tx expecting response other than cts/cwcts */

	/* TX error related */
	uint32	txrtsfail;	/* RTS TX failure count */
	uint32	txphyerr;	/* PHY TX error count */

	uint32	last_bcn_seq_num;	/* last beacon seq no. */
	uint32	last_bcn_ltsf;		/* last beacon ltsf */

	uint16	nav_cntr_l;		/* The state of the NAV */
	uint16	nav_cntr_h;
} phy_periodic_counters_v6_t;

typedef struct {
	uint32	txallfrm;		/* total number of frames sent, incl. Data, ACK, RTS,
					* CTS, Control Management
					*  (includes retransmissions)
					*/
	uint32	rxrsptmout;		/* number of response timeouts for transmitted frames
					* expecting a response
					*/
	uint32	rxstrt;			/* number of received frames with a good PLCP */
	uint32	rxcrsglitch;		/* PHY was able to correlate the preamble but not
					* the header
					*/
	uint32	bphy_badplcp;		/* number of bad PLCP reception on BPHY rate */
	uint32	bphy_rxcrsglitch;	/* PHY count of bphy glitches */
	uint32	rxbadfcs;		/* number of frames for which the CRC check failed
					* in the MAC
					*/
	uint32	rxbeaconmbss;		/* beacons received from member of BSS */
	uint32	rxdtucastmbss;		/* number of received DATA frames with good FCS and
					* matching RA
					*/
	uint32	rxdtocast;		/* number of received DATA frames
					* (good FCS and no matching RA)
					*/
	uint32	goodfcs;		/* Good fcs counters  */

	uint32	txctl;
	uint32	rxctl;
	uint32	txbar;
	uint32	rxbar;
	uint32	rxbeaconobss;

	uint32	txrts;
	uint32	txucast;		/* number of unicast tx expecting response
					* other than cts/cwcts
					*/
	uint32	rxackucast;		/* number of ucast ACKS received (good FCS) */
	uint32	txackfrm;		/* number of ACK frames sent out */

	uint32	txrtsfrm;		/* number of RTS sent out by the MAC */
	uint32	txrtsfail;		/* number of rts transmission failure
					* that reach retry limit
					*/
	uint32	rxctsucast;		/* number of unicast CTS addressed to the MAC (good FCS) */

	uint32	rxrtsucast;		/* number of unicast RTS addressed to the MAC (good FCS) */
	uint32	txctsfrm;		/* number of CTS sent out by the MAC */

	uint32	rxback;			/* blockack rxcnt */
	uint32	txback;			/* blockack txcnt */
	uint32	rxctlucast;		/* number of received CNTRL frames
					* with good FCS and matching RA
					*/

	uint32	last_bcn_seq_num;	/* L_TSF and Seq # from the last beacon received */
	uint32	last_bcn_ltsf;		/* L_TSF and Seq # from the last beacon received */

	uint32	desense_reason;		/* desense paraemters to indicate reasons
					* for bphy and ofdm_desense
					*/

	/* RX error related */
	uint32	rxdropped;		/* Frame dropped */
	uint32	rxnobuf;		/* Rx error due to no buffer */
	uint32	rxrunt;			/* Runt frame counter */

	uint32	rxframe;		/* Number of received frames */

	/* Misc general purpose debug counters (will be used for future debugging) */
	uint32	tbtt;			/* Per-interface stats report tbtt count */
	uint32  p2ptbtt;		/* MCNX TBTT */
	uint32  p2ptbttmiss;		/* TBTT coming when the radio is on an off channel */
	uint32  missbcn_dbg;		/* Number of beacon missed to receive */
	uint32  noise_iqest_to;		/* Count of IQ Est timeout during noise measurement */

	uint32	rxanyerr;		/* Any RX error that is not counted by other counters. */

	uint32	phyovfl_cnt;		/* RX PHY FIFO overflow */
	uint32  rxf0ovfl;		/* Rx FIFO0 overflow counters information */
	uint32  rxf1ovfl;		/* Rx FIFO1 overflow counters information */

	uint32	lenfovfl_cnt;		/* RX LFIFO overflow */
	uint32	weppeof_cnt;		/* WEP asserted premature end-of-frame */
	uint32	rxbadplcp;		/* number of parity check of the PLCP header failed */
	uint32	strmeof_cnt;		/* RX frame got aborted because PHY FIFO did not have
					* sufficient bytes
					*/
	uint32	pfifo_drop;		/* PHY FIFO was not empty when a new frame arrived */
	uint32	ctx_fifo_full;		/* Low Priority Context FIFO is full */
	uint32	ctx_fifo2_full;		/* High Priority Context FIFO is full */
	uint32	rxnodelim;		/* number of no valid delimiter detected by ampdu parser */
	uint32	rx20s_cnt;		/* secondary 20 counter */
	uint32	rxdrop20s;		/* RX was discarded as the CRS was not seen on
					* primary channel
					*/
	uint32	new_rxin_plcp_wait_cnt;	/* A new reception started waiting for PLCP bytes
						* from a previous receive
						*/
	uint32  rxtoolate;		/* receive too late */
	uint32	laterx_cnt;		/* RX frame dropped as it was seen too (30us) late
					* from the start of reception
					*/
	uint32	rxfrmtoolong;		/* Number of received frame that are too long */
	uint32	rxfrmtooshrt;		/* RX frame was dropped as it did not meet minimum
					* number of bytes to be a valid 802.11 frame
					*/

	uint32	rxlegacyfrminvalid;	/* Invalid BPHY or L-OFDM reception */
	uint32	txsifserr;		/* A frame arrived in SIFS while we were about to
					* transmit B/ACK
					*/
	uint32	ooseq_macsusp;		/* Ucode is out of sequence in processing reception
					* (especially due to macsuspend).
					* RX MEND is seen without RX STRT
					*/

	uint16	counter_noise_request;	/* counters of requesting noise samples for noisecal */
	uint16	counter_noise_crsbit;	/* counters of noise mmt being interrupted
					* due to PHYCRS>
					*/
	uint16	counter_noise_apply;	/* counts of applying noisecal result */

	uint16	deaf_count;		/* Depth of stay_in_carrier_search function */

	uint8	phylog_noise_mode;	/* noise cal mode */
	uint8	total_desense_on;	/* total desense on flag */
	uint8	initgain_desense;	/* initgain desense when total desense is on */
	uint8	crsmin_init;		/* crsmin_init threshold when total desense is on */
	uint8	lna1_gainlmt_desense;	/* lna1_gain limit desense when applying desense */
	uint8	clipgain_desense0;	/* clipgain desense when applying desense */

	uint8	lte_ofdm_desense;	/* ofdm desense dut to lte */
	uint8	lte_bphy_desense;	/* bphy desense due to lte */

	uint8	hw_aci_status;		/* HW ACI status flag */
	uint8	aci_clipgain_desense0;	/* clipgain desense due to aci */
	uint8	lna1_tbl_desense;	/* LNA1 table desense due to aci */

	int8	crsmin_high;		/* crsmin high when applying desense */
	int8	weakest_rssi;		/* weakest link RSSI */

	uint8	max_fp;
	uint8	crsminpwr_initgain;
	uint8	crsminpwr_clip1_high;
	uint8	crsminpwr_clip1_med;
	uint8	crsminpwr_clip1_lo;
	uint8	pad1;
	uint8	pad2;
} phy_periodic_counters_v7_t;

typedef struct phy_periodic_counters_v8 {
	/* RX error related */
	uint32	rxrsptmout;		/* number of response timeouts for transmitted frames
					* expecting a response
					*/
	uint32	rxcrsglitch;		/* PHY was able to correlate the preamble
					* but not the header
					*/
	uint32	bphy_badplcp;		/* number of bad PLCP reception on BPHY rate */
	uint32	bphy_rxcrsglitch;	/* PHY count of bphy glitches */
	uint32	rxdropped;		/* Frame dropped */
	uint32	rxnobuf;		/* Rx error due to no buffer */
	uint32	rxrunt;			/* Runt frame counter */
	uint32	rxbadfcs;		/* number of frames for which the CRC check failed
					* in the MAC
					*/

	/* RX related */
	uint32	rxstrt;			/* number of received frames with a good PLCP */
	uint32	rxbeaconmbss;		/* beacons received from member of BSS */
	uint32	rxdtucastmbss;		/* number of received DATA frames with good FCS
					* and matching RA
					*/
	uint32	rxdtocast;		/* number of received DATA frames
					* (good FCS and no matching RA)
					*/
	uint32  goodfcs;		/* Good fcs counters  */
	uint32	rxctl;			/* Number of control frames */
	uint32	rxaction;		/* Number of action frames */
	uint32	rxback;			/* Number of block ack frames rcvd */
	uint32	rxctlucast;		/* Number of received unicast ctl frames */
	uint32	rxframe;		/* Number of received frames */

	uint32	rxbar;			/* Number of block ack requests rcvd */
	uint32	rxackucast;		/* number of ucast ACKS received (good FCS) */
	uint32	rxbeaconobss;		/* number of OBSS beacons received */
	uint32	rxctsucast;		/* number of unicast CTS addressed to the MAC (good FCS) */
	uint32	rxrtsucast;		/* number of unicast RTS addressed to the MAC (good FCS) */

	/* TX related */
	uint32	txallfrm;		/* total number of frames sent, incl. Data, ACK, RTS, CTS,
					* Control Management (includes retransmissions)
					*/
	uint32	txmpdu;			/* Numer of transmitted mpdus */
	uint32	txackbackctsfrm;	/* Number of ACK + BACK + CTS */
	uint32	txackfrm;		/* number of ACK frames sent out */
	uint32	txrtsfrm;		/* number of RTS sent out by the MAC */
	uint32	txctsfrm;		/* number of CTS sent out by the MAC */

	uint32	txctl;			/* Number of control frames txd */
	uint32	txbar;			/* Number of block ack requests txd */
	uint32	txrts;			/* Number of RTS txd */
	uint32	txback;			/* Number of block ack frames txd */
	uint32	txucast;		/* number of unicast tx expecting response
					* other than cts/cwcts
					*/

	/* TX error related */
	uint32	txrtsfail;		/* RTS TX failure count */
	uint32	txphyerr;		/* PHY TX error count */

	uint32	last_bcn_seq_num;	/* last beacon seq no. */
	uint32	last_bcn_ltsf;		/* last beacon ltsf */

	/* Misc general purpose debug counters (will be used for future debugging) */
	uint32	tbtt;			/* Per-interface stats report tbtt count */
	uint32  p2ptbtt;		/* MCNX TBTT */
	uint32  p2ptbttmiss;		/* TBTT coming when the radio is on an off channel */
	uint32  missbcn_dbg;		/* Number of beacon missed to receive */
	uint32  noise_iqest_to;		/* Count of IQ Est timeout during noise measurement */

	uint32	rxanyerr;		/* Any RX error that is not counted by other counters. */

	uint32	phyovfl_cnt;		/* RX PHY FIFO overflow */
	uint32  rxf0ovfl;		/* Rx FIFO0 overflow counters information */
	uint32  rxf1ovfl;		/* Rx FIFO1 overflow counters information */

	uint32	lenfovfl_cnt;		/* RX LFIFO overflow */
	uint32	weppeof_cnt;		/* WEP asserted premature end-of-frame */
	uint32	rxbadplcp;		/* number of parity check of the PLCP header failed */
	uint32	strmeof_cnt;		/* RX frame got aborted because PHY FIFO did not have
					* sufficient bytes
					*/
	uint32	pfifo_drop;		/* PHY FIFO was not empty when a new frame arrived */
	uint32	ctx_fifo_full;		/* Low Priority Context FIFO is full */
	uint32	ctx_fifo2_full;		/* High Priority Context FIFO is full */
	uint32	rxnodelim;		/* number of no valid delimiter detected by ampdu parser */
	uint32	rx20s_cnt;		/* secondary 20 counter */
	uint32	rxdrop20s;		/* RX was discarded as the CRS was not seen on
					* primary channel
					*/
	uint32	new_rxin_plcp_wait_cnt;	/* A new reception started waiting for PLCP bytes
					* from a previous receive
					*/
	uint32  rxtoolate;		/* receive too late */
	uint32	laterx_cnt;		/* RX frame dropped as it was seen too (30us) late
					* from the start of reception
					*/
	uint32	rxfrmtoolong;		/* Number of received frame that are too long */
	uint32	rxfrmtooshrt;		/* RX frame was dropped as it did not meet minimum
					* number of bytes to be a valid 802.11 frame
					*/

	uint32	rxlegacyfrminvalid;	/* Invalid BPHY or L-OFDM reception */
	uint32	txsifserr;		/* A frame arrived in SIFS while we were about to
					* transmit B/ACK
					*/
	uint32	ooseq_macsusp;		/* Ucode is out of sequence in processing reception
					* (especially due to macsuspend).
					* RX MEND is seen without RX STRT
					*/
	uint32	desense_reason;		/* desense paraemters to indicate reasons
					* for bphy and ofdm_desense
					*/

	uint16	nav_cntr_l;		/* The state of the NAV */
	uint16	nav_cntr_h;
} phy_periodic_counters_v8_t;

typedef struct phy_periodic_counters_v9 {
	/* RX error related */
	uint32	rxrsptmout;		/* number of response timeouts for transmitted frames
					* expecting a response
					*/
	uint32	rxcrsglitch;		/* PHY was able to correlate the preamble
					* but not the header
					*/
	uint32	bphy_badplcp;		/* number of bad PLCP reception on BPHY rate */
	uint32	bphy_rxcrsglitch;	/* PHY count of bphy glitches */
	uint32	rxdropped;		/* Frame dropped */
	uint32	rxnobuf;		/* Rx error due to no buffer */
	uint32	rxrunt;			/* Runt frame counter */
	uint32	rxbadfcs;		/* number of frames for which the CRC check failed
					* in the MAC
					*/

	/* RX related */
	uint32	rxstrt;			/* number of received frames with a good PLCP */
	uint32	rxbeaconmbss;		/* beacons received from member of BSS */
	uint32	rxdtucastmbss;		/* number of received DATA frames with good FCS
					* and matching RA
					*/
	uint32	rxdtocast;		/* number of received DATA frames
					* (good FCS and no matching RA)
					*/
	uint32  goodfcs;		/* Good fcs counters  */
	uint32	rxctl;			/* Number of control frames */
	uint32	rxaction;		/* Number of action frames */
	uint32	rxback;			/* Number of block ack frames rcvd */
	uint32	rxctlucast;		/* Number of received unicast ctl frames */
	uint32	rxframe;		/* Number of received frames */

	uint32	rxbar;			/* Number of block ack requests rcvd */
	uint32	rxackucast;		/* number of ucast ACKS received (good FCS) */
	uint32	rxbeaconobss;		/* number of OBSS beacons received */
	uint32	rxctsucast;		/* number of unicast CTS addressed to the MAC (good FCS) */
	uint32	rxrtsucast;		/* number of unicast RTS addressed to the MAC (good FCS) */

	/* TX related */
	uint32	txallfrm;		/* total number of frames sent, incl. Data, ACK, RTS, CTS,
					* Control Management (includes retransmissions)
					*/
	uint32	txmpdu;			/* Numer of transmitted mpdus */
	uint32	txackbackctsfrm;	/* Number of ACK + BACK + CTS */
	uint32	txackfrm;		/* number of ACK frames sent out */
	uint32	txrtsfrm;		/* number of RTS sent out by the MAC */
	uint32	txctsfrm;		/* number of CTS sent out by the MAC */

	uint32	txctl;			/* Number of control frames txd */
	uint32	txbar;			/* Number of block ack requests txd */
	uint32	txrts;			/* Number of RTS txd */
	uint32	txback;			/* Number of block ack frames txd */
	uint32	txucast;		/* number of unicast tx expecting response
					* other than cts/cwcts
					*/

	/* TX error related */
	uint32	txrtsfail;		/* RTS TX failure count */
	uint32	txphyerr;		/* PHY TX error count */

	uint32	last_bcn_seq_num;	/* last beacon seq no. */
	uint32	last_bcn_ltsf;		/* last beacon ltsf */

	/* Misc general purpose debug counters (will be used for future debugging) */
	uint32	debug_01;
	uint32  debug_02;
	uint32  debug_03;
	uint32  debug_04;
	uint32  debug_05;

	uint32	rxanyerr;		/* Any RX error that is not counted by other counters. */

	uint32	phyovfl_cnt;		/* RX PHY FIFO overflow */
	uint32  rxf0ovfl;		/* Rx FIFO0 overflow counters information */
	uint32  rxf1ovfl;		/* Rx FIFO1 overflow counters information */

	uint32	lenfovfl_cnt;		/* RX LFIFO overflow */
	uint32	weppeof_cnt;		/* WEP asserted premature end-of-frame */
	uint32	rxbadplcp;		/* number of parity check of the PLCP header failed */
	uint32	strmeof_cnt;		/* RX frame got aborted because PHY FIFO did not have
					* sufficient bytes
					*/
	uint32	pfifo_drop;		/* PHY FIFO was not empty when a new frame arrived */
	uint32	ctx_fifo_full;		/* Low Priority Context FIFO is full */
	uint32	ctx_fifo2_full;		/* High Priority Context FIFO is full */
	uint32	rxnodelim;		/* number of no valid delimiter detected by ampdu parser */
	uint32	rx20s_cnt;		/* secondary 20 counter */
	uint32	rxdrop20s;		/* RX was discarded as the CRS was not seen on
					* primary channel
					*/
	uint32	new_rxin_plcp_wait_cnt;	/* A new reception started waiting for PLCP bytes
					* from a previous receive
					*/
	uint32  rxtoolate;		/* receive too late */
	uint32	laterx_cnt;		/* RX frame dropped as it was seen too (30us) late
					* from the start of reception
					*/
	uint32	rxfrmtoolong;		/* Number of received frame that are too long */
	uint32	rxfrmtooshrt;		/* RX frame was dropped as it did not meet minimum
					* number of bytes to be a valid 802.11 frame
					*/

	uint32	rxlegacyfrminvalid;	/* Invalid BPHY or L-OFDM reception */
	uint32	txsifserr;		/* A frame arrived in SIFS while we were about to
					* transmit B/ACK
					*/
	uint32	ooseq_macsusp;		/* Ucode is out of sequence in processing reception
					* (especially due to macsuspend).
					* RX MEND is seen without RX STRT
					*/
	uint32	desense_reason;		/* desense paraemters to indicate reasons
					* for bphy and ofdm_desense
					*/

	uint16	nav_cntr_l;		/* The state of the NAV */
	uint16	nav_cntr_h;

	uint32	tbtt;			/* Per-interface stats report tbtt count */
	uint32  p2ptbtt;		/* MCNX TBTT */
	uint32  p2ptbttmiss;		/* TBTT coming when the radio is on an off channel */
	uint32  noise_iqest_to;		/* Count of IQ Est timeout during noise measurement */
	uint16  missbcn_dbg;		/* Number of beacon missed to receive */

	/* Misc general purpose debug counters (will be used for future debugging) */
	uint16	debug_06;
	uint16	debug_07;
	uint16	debug_08;
	uint16	debug_09;
	uint16	debug_10;

	uint8	sc_dccal_incc_cnt;	/* scan dccal counter */
	uint8	sc_rxiqcal_skip_cnt;	/* scan rxiqcal counter */
	uint8	sc_noisecal_incc_cnt;	/* scan noise cal counter */
	uint8	debug_14;
} phy_periodic_counters_v9_t;

typedef struct phy_periodic_counters_v10 {
	/* RX error related */
	uint32	rxrsptmout;		/* number of response timeouts for transmitted frames
					* expecting a response
					*/
	uint32	rxcrsglitch;		/* PHY was able to correlate the preamble
					* but not the header
					*/
	uint32	bphy_badplcp;		/* number of bad PLCP reception on BPHY rate */
	uint32	bphy_rxcrsglitch;	/* PHY count of bphy glitches */
	uint32	rxdropped;		/* Frame dropped */
	uint32	rxnobuf;		/* Rx error due to no buffer */
	uint32	rxrunt;			/* Runt frame counter */
	uint32	rxbadfcs;		/* number of frames for which the CRC check failed
					* in the MAC
					*/

	/* RX related */
	uint32	rxstrt;			/* number of received frames with a good PLCP */
	uint32	rxbeaconmbss;		/* beacons received from member of BSS */
	uint32	rxdtucastmbss;		/* number of received DATA frames with good FCS
					* and matching RA
					*/
	uint32	rxdtocast;		/* number of received DATA frames
					* (good FCS and no matching RA)
					*/
	uint32  goodfcs;		/* Good fcs counters  */
	uint32	rxctl;			/* Number of control frames */
	uint32	rxaction;		/* Number of action frames */
	uint32	rxback;			/* Number of block ack frames rcvd */
	uint32	rxctlucast;		/* Number of received unicast ctl frames */
	uint32	rxframe;		/* Number of received frames */

	uint32	rxbar;			/* Number of block ack requests rcvd */
	uint32	rxackucast;		/* number of ucast ACKS received (good FCS) */
	uint32	rxbeaconobss;		/* number of OBSS beacons received */
	uint32	rxctsucast;		/* number of unicast CTS addressed to the MAC (good FCS) */
	uint32	rxrtsucast;		/* number of unicast RTS addressed to the MAC (good FCS) */

	/* TX related */
	uint32	txallfrm;		/* total number of frames sent, incl. Data, ACK, RTS, CTS,
					* Control Management (includes retransmissions)
					*/
	uint32	txmpdu;			/* Numer of transmitted mpdus */
	uint32	txackbackctsfrm;	/* Number of ACK + BACK + CTS */
	uint32	txackfrm;		/* number of ACK frames sent out */
	uint32	txrtsfrm;		/* number of RTS sent out by the MAC */
	uint32	txctsfrm;		/* number of CTS sent out by the MAC */

	uint32	txctl;			/* Number of control frames txd */
	uint32	txbar;			/* Number of block ack requests txd */
	uint32	txrts;			/* Number of RTS txd */
	uint32	txback;			/* Number of block ack frames txd */
	uint32	txucast;		/* number of unicast tx expecting response
					* other than cts/cwcts
					*/

	/* TX error related */
	uint32	txrtsfail;		/* RTS TX failure count */
	uint32	txphyerr;		/* PHY TX error count */

	uint32	last_bcn_seq_num;	/* last beacon seq no. */
	uint32	last_bcn_ltsf;		/* last beacon ltsf */

	uint32	rxanyerr;		/* Any RX error that is not counted by other counters. */

	uint32	phyovfl_cnt;		/* RX PHY FIFO overflow */
	uint32  rxf0ovfl;		/* Rx FIFO0 overflow counters information */
	uint32  rxf1ovfl;		/* Rx FIFO1 overflow counters information */

	uint32	lenfovfl_cnt;		/* RX LFIFO overflow */
	uint32	weppeof_cnt;		/* WEP asserted premature end-of-frame */
	uint32	rxbadplcp;		/* number of parity check of the PLCP header failed */
	uint32	strmeof_cnt;		/* RX frame got aborted because PHY FIFO did not have
					* sufficient bytes
					*/
	uint32	pfifo_drop;		/* PHY FIFO was not empty when a new frame arrived */
	uint32	ctx_fifo_full;		/* Low Priority Context FIFO is full */
	uint32	ctx_fifo2_full;		/* High Priority Context FIFO is full */
	uint32	rxnodelim;		/* number of no valid delimiter detected by ampdu parser */
	uint32	rx20s_cnt;		/* secondary 20 counter */
	uint32	rxdrop20s;		/* RX was discarded as the CRS was not seen on
					* primary channel
					*/
	uint32	new_rxin_plcp_wait_cnt;	/* A new reception started waiting for PLCP bytes
					* from a previous receive
					*/
	uint32  rxtoolate;		/* receive too late */
	uint32	laterx_cnt;		/* RX frame dropped as it was seen too (30us) late
					* from the start of reception
					*/
	uint32	rxfrmtoolong;		/* Number of received frame that are too long */
	uint32	rxfrmtooshrt;		/* RX frame was dropped as it did not meet minimum
					* number of bytes to be a valid 802.11 frame
					*/

	uint32	rxlegacyfrminvalid;	/* Invalid BPHY or L-OFDM reception */
	uint32	txsifserr;		/* A frame arrived in SIFS while we were about to
					* transmit B/ACK
					*/
	uint32	ooseq_macsusp;		/* Ucode is out of sequence in processing reception
					* (especially due to macsuspend).
					* RX MEND is seen without RX STRT
					*/
	uint32	desense_reason;		/* desense paraemters to indicate reasons
					* for bphy and ofdm_desense
					*/

	uint16	nav_cntr_l;		/* The state of the NAV */
	uint16	nav_cntr_h;

	uint32	tbtt;			/* Per-interface stats report tbtt count */
	uint32  p2ptbtt;		/* MCNX TBTT */
	uint32  p2ptbttmiss;		/* TBTT coming when the radio is on an off channel */
	uint32  noise_iqest_to;		/* Count of IQ Est timeout during noise measurement */
	uint32  ctmode_ufc_cnt;		/* Underflow cnt in ctmode */
	uint16  missbcn_dbg;		/* Number of beacon missed to receive */

	uint8	sc_dccal_incc_cnt;	/* scan dccal counter */
	uint8	sc_rxiqcal_skip_cnt;	/* scan rxiqcal counter */
	uint8	sc_noisecal_incc_cnt;	/* scan noise cal counter */
	uint8	debug_01;		/* Misc general purpose debug counters */
	uint8	debug_02;		/* Misc general purpose debug counters */
	uint8	debug_03;		/* Misc general purpose debug counters */

	/* Misc general purpose debug counters (will be used for future debugging) */
	uint32  debug_04;
	uint32  debug_05;
	uint32	debug_06;
	uint32	debug_07;
	uint16	debug_08;
	uint16	debug_09;
	uint16	debug_10;
	uint16	debug_11;
} phy_periodic_counters_v10_t;

typedef struct phy_periodic_counters_v11 {
	/* RX error related */
	uint32	rxrsptmout;		/* number of response timeouts for transmitted frames
					* expecting a response
					*/
	uint32	rxcrsglitch;		/* PHY was able to correlate the preamble
					* but not the header
					*/
	uint32	bphy_badplcp;		/* number of bad PLCP reception on BPHY rate */
	uint32	bphy_rxcrsglitch;	/* PHY count of bphy glitches */
	uint32	rxdropped;		/* Frame dropped */
	uint32	rxnobuf;		/* Rx error due to no buffer */
	uint32	rxrunt;			/* Runt frame counter */
	uint32	rxbadfcs;		/* number of frames for which the CRC check failed
					* in the MAC
					*/

	/* RX related */
	uint32	rxstrt;			/* number of received frames with a good PLCP */
	uint32	rxbeaconmbss;		/* beacons received from member of BSS */
	uint32	rxdtucastmbss;		/* number of received DATA frames with good FCS
					* and matching RA
					*/
	uint32	rxdtocast;		/* number of received DATA frames
					* (good FCS and no matching RA)
					*/
	uint32  goodfcs;		/* Good fcs counters  */
	uint32	rxctl_mgt;		/* Number of control frames */
	uint32	rxaction;		/* Number of action frames */
	uint32	rxback;			/* Number of block ack frames rcvd */
	uint32	rxctlucast;		/* Number of received unicast ctl frames */
	uint32	rxframe;		/* Number of received frames */

	uint32	rxbar;			/* Number of block ack requests rcvd */
	uint32	rxackucast;		/* number of ucast ACKS received (good FCS) */
	uint32	rxbeaconobss;		/* number of OBSS beacons received */
	uint32	rxctsucast;		/* number of unicast CTS addressed to the MAC (good FCS) */
	uint32	rxrtsucast;		/* number of unicast RTS addressed to the MAC (good FCS) */
	uint32	rxauth;			/* Number of RX AUTH */
	uint32	rxdeauth;		/* Number of RX DEAUTH */
	uint32	rxassocrsp;		/* Number of RX ASSOC response */

	/* TX related */
	uint32	txallfrm;		/* total number of frames sent, incl. Data, ACK, RTS, CTS,
					* Control Management (includes retransmissions)
					*/
	uint32	txmpdu;			/* Numer of transmitted mpdus */
	uint32	txackbackctsfrm;	/* Number of ACK + BACK + CTS */
	uint32	txackfrm;		/* number of ACK frames sent out */
	uint32	txrtsfrm;		/* number of RTS sent out by the MAC */
	uint32	txctsfrm;		/* number of CTS sent out by the MAC */

	uint32	txctl;			/* Number of control frames txd */
	uint32	txbar;			/* Number of block ack requests txd */
	uint32	txrts;			/* Number of RTS txd */
	uint32	txback;			/* Number of block ack frames txd */
	uint32	txucast;		/* number of unicast tx expecting response
					* other than cts/cwcts
					*/
	uint32	txauth;			/* Number of TX AUTH */
	uint32	txdeauth;		/* Number of TX DEAUTH */
	uint32	txassocreq;		/* Number of TX ASSOC request */

	/* TX error related */
	uint32	txrtsfail;		/* RTS TX failure count */
	uint32	txphyerr;		/* PHY TX error count */

	uint32	last_bcn_seq_num;	/* last beacon seq no. */
	uint32	last_bcn_ltsf;		/* last beacon ltsf */

	uint32	rxanyerr;		/* Any RX error that is not counted by other counters. */

	uint32	phyovfl_cnt;		/* RX PHY FIFO overflow */
	uint32  rxf0ovfl;		/* Rx FIFO0 overflow counters information */
	uint32  rxf1ovfl;		/* Rx FIFO1 overflow counters information */

	uint32	lenfovfl_cnt;		/* RX LFIFO overflow */
	uint32	weppeof_cnt;		/* WEP asserted premature end-of-frame */
	uint32	rxbadplcp;		/* number of parity check of the PLCP header failed */
	uint32	strmeof_cnt;		/* RX frame got aborted because PHY FIFO did not have
					* sufficient bytes
					*/
	uint32	pfifo_drop;		/* PHY FIFO was not empty when a new frame arrived */
	uint32	ctx_fifo_full;		/* Low Priority Context FIFO is full */
	uint32	ctx_fifo2_full;		/* High Priority Context FIFO is full */
	uint32	rxnodelim;		/* number of no valid delimiter detected by ampdu parser */
	uint32	rx20s_cnt;		/* secondary 20 counter */
	uint32	rxdrop20s;		/* RX was discarded as the CRS was not seen on
					* primary channel
					*/
	uint32	new_rxin_plcp_wait_cnt;	/* A new reception started waiting for PLCP bytes
					* from a previous receive
					*/
	uint32  rxtoolate;		/* receive too late */
	uint32	laterx_cnt;		/* RX frame dropped as it was seen too (30us) late
					* from the start of reception
					*/
	uint32	rxfrmtoolong;		/* Number of received frame that are too long */
	uint32	rxfrmtooshrt;		/* RX frame was dropped as it did not meet minimum
					* number of bytes to be a valid 802.11 frame
					*/

	uint32	rxlegacyfrminvalid;	/* Invalid BPHY or L-OFDM reception */
	uint32	txsifserr;		/* A frame arrived in SIFS while we were about to
					* transmit B/ACK
					*/
	uint32	ooseq_macsusp;		/* Ucode is out of sequence in processing reception
					* (especially due to macsuspend).
					* RX MEND is seen without RX STRT
					*/
	uint32	desense_reason;		/* desense paraemters to indicate reasons
					* for bphy and ofdm_desense
					*/

	uint16	nav_cntr_l;		/* The state of the NAV */
	uint16	nav_cntr_h;

	uint32	tbtt;			/* Per-interface stats report tbtt count */
	uint32  p2ptbtt;		/* MCNX TBTT */
	uint32  p2ptbttmiss;		/* TBTT coming when the radio is on an off channel */
	uint32  noise_iqest_to;		/* Count of IQ Est timeout during noise measurement */
	uint32  ctmode_ufc_cnt;		/* Underflow cnt in ctmode */
	uint32	fourwayfail;		/* FourWayHandshakeFailures */
	uint16  missbcn_dbg;		/* Number of beacon missed to receive */

	uint8	sc_dccal_incc_cnt;	/* scan dccal counter */
	uint8	sc_rxiqcal_skip_cnt;	/* scan rxiqcal counter */
	uint8	sc_noisecal_incc_cnt;	/* scan noise cal counter */

	/* BFR path */
	uint8	bfr_last_snd_success;	/* Last sounding successful val */
	uint16	bfr_txndpa;		/* null data packet announcements */
	uint16	bfr_txndp;		/* null data packets */
	uint16	bfr_rxsf;
	/* BFE path */
	uint16	bfe_rxndpa_u;		/* unicast NDPAs */
	uint16	bfe_rxndpa_m;		/* multicast NDPAs */
	uint16	bfe_rpt;		/* beamforming reports */
	uint16	bfe_txsf;		/* subframes */

	/* Misc general purpose debug counters (will be used for future debugging) */
	uint16	debug_01;
	uint16	debug_02;
	uint32  debug_03;
} phy_periodic_counters_v11_t;

#define PHY_PERIODIC_COUNTERS_VER_12	(12u)
typedef struct phy_periodic_counters_v12 {
	uint16 version;
	uint16 len;
	uint16 seq;
	uint8 PAD[2];

	/* RX error related */
	uint32	rxrsptmout;		/* number of response timeouts for transmitted frames
					* expecting a response
					*/
	uint32	rxcrsglitch;		/* PHY was able to correlate the preamble
					* but not the header
					*/
	uint32	bphy_badplcp;		/* number of bad PLCP reception on BPHY rate */
	uint32	bphy_rxcrsglitch;	/* PHY count of bphy glitches */
	uint32	rxdropped;		/* Frame dropped */
	uint32	rxnobuf;		/* Rx error due to no buffer */
	uint32	rxrunt;			/* Runt frame counter */
	uint32	rxbadfcs;		/* number of frames for which the CRC check failed
					* in the MAC
					*/

	/* RX related */
	uint32	rxstrt;			/* number of received frames with a good PLCP */
	uint32	rxbeaconmbss;		/* beacons received from member of BSS */
	uint32	rxdtucastmbss;		/* number of received DATA frames with good FCS
					* and matching RA
					*/
	uint32	rxdtocast;		/* number of received DATA frames
					* (good FCS and no matching RA)
					*/
	uint32  goodfcs;		/* Good fcs counters  */
	uint32	rxctl_mgt;		/* Number of control frames */
	uint32	rxaction;		/* Number of action frames */
	uint32	rxback;			/* Number of block ack frames rcvd */
	uint32	rxctlucast;		/* Number of received unicast ctl frames */
	uint32	rxframe;		/* Number of received frames */

	uint32	rxbar;			/* Number of block ack requests rcvd */
	uint32	rxackucast;		/* number of ucast ACKS received (good FCS) */
	uint32	rxbeaconobss;		/* number of OBSS beacons received */
	uint32	rxctsucast;		/* number of unicast CTS addressed to the MAC (good FCS) */
	uint32	rxrtsucast;		/* number of unicast RTS addressed to the MAC (good FCS) */
	uint32	rxauth;			/* Number of RX AUTH */
	uint32	rxdeauth;		/* Number of RX DEAUTH */
	uint32	rxassocrsp;		/* Number of RX ASSOC response */

	/* TX related */
	uint32	txallfrm;		/* total number of frames sent, incl. Data, ACK, RTS, CTS,
					* Control Management (includes retransmissions)
					*/
	uint32	txmpdu;			/* Numer of transmitted mpdus */
	uint32	txackbackctsfrm;	/* Number of ACK + BACK + CTS */
	uint32	txackfrm;		/* number of ACK frames sent out */
	uint32	txrtsfrm;		/* number of RTS sent out by the MAC */
	uint32	txctsfrm;		/* number of CTS sent out by the MAC */

	uint32	txctl;			/* Number of control frames txd */
	uint32	txbar;			/* Number of block ack requests txd */
	uint32	txrts;			/* Number of RTS txd */
	uint32	txback;			/* Number of block ack frames txd */
	uint32	txucast;		/* number of unicast tx expecting response
					* other than cts/cwcts
					*/
	uint32	txauth;			/* Number of TX AUTH */
	uint32	txdeauth;		/* Number of TX DEAUTH */
	uint32	txassocreq;		/* Number of TX ASSOC request */

	uint32	txnull;			/* Number of TX NULL_DATA */
	uint32	txqosnull;		/* Number of TX NULL_QoSDATA */
	uint32	txnull_pm;		/* Number of TX NULL_DATA total */
	uint32	txnull_pm_succ;		/* Number of TX NULL_DATA successes */

	/* TX error related */
	uint32	txrtsfail;		/* RTS TX failure count */
	uint32	txphyerr;		/* PHY TX error count */

	uint32	last_bcn_seq_num;	/* last beacon seq no. */
	uint32	last_bcn_ltsf;		/* last beacon ltsf */

	uint32	rxanyerr;		/* Any RX error that is not counted by other counters. */

	uint32	phyovfl_cnt;		/* RX PHY FIFO overflow */
	uint32  rxf0ovfl;		/* Rx FIFO0 overflow counters information */
	uint32  rxf1ovfl;		/* Rx FIFO1 overflow counters information */

	uint32	lenfovfl_cnt;		/* RX LFIFO overflow */
	uint32	weppeof_cnt;		/* WEP asserted premature end-of-frame */
	uint32	rxbadplcp;		/* number of parity check of the PLCP header failed */
	uint32	strmeof_cnt;		/* RX frame got aborted because PHY FIFO did not have
					* sufficient bytes
					*/
	uint32	pfifo_drop;		/* PHY FIFO was not empty when a new frame arrived */
	uint32	ctx_fifo_full;		/* Low Priority Context FIFO is full */
	uint32	ctx_fifo2_full;		/* High Priority Context FIFO is full */
	uint32	rxnodelim;		/* number of no valid delimiter detected by ampdu parser */
	uint32	rx20s_cnt;		/* secondary 20 counter */
	uint32	rxdrop20s;		/* RX was discarded as the CRS was not seen on
					* primary channel
					*/
	uint32	new_rxin_plcp_wait_cnt;	/* A new reception started waiting for PLCP bytes
					* from a previous receive
					*/
	uint32  rxtoolate;		/* receive too late */
	uint32	laterx_cnt;		/* RX frame dropped as it was seen too (30us) late
					* from the start of reception
					*/
	uint32	rxfrmtoolong;		/* Number of received frame that are too long */
	uint32	rxfrmtooshrt;		/* RX frame was dropped as it did not meet minimum
					* number of bytes to be a valid 802.11 frame
					*/

	uint32	rxlegacyfrminvalid;	/* Invalid BPHY or L-OFDM reception */
	uint32	txsifserr;		/* A frame arrived in SIFS while we were about to
					* transmit B/ACK
					*/
	uint32	ooseq_macsusp;		/* Ucode is out of sequence in processing reception
					* (especially due to macsuspend).
					* RX MEND is seen without RX STRT
					*/
	uint32	desense_reason;		/* desense paraemters to indicate reasons
					* for bphy and ofdm_desense
					*/

	uint16	nav_cntr_l;		/* Can be removed */
	uint16	nav_cntr_h;		/* Can be removed */

	uint32	tbtt;			/* Per-interface stats report tbtt count */
	uint32  p2ptbtt;		/* MCNX TBTT */
	uint32  p2ptbttmiss;		/* TBTT coming when the radio is on an off channel */
	uint32  noise_iqest_to;		/* Count of IQ Est timeout during noise measurement */
	uint32  ctmode_ufc_cnt;		/* Underflow cnt in ctmode */
	uint32	fourwayfail;		/* FourWayHandshakeFailures */
	uint16  missbcn_dbg;		/* Number of beacon missed to receive */

	uint8	sc_dccal_incc_cnt;	/* scan dccal counter */
	uint8	sc_rxiqcal_skip_cnt;	/* scan rxiqcal counter */
	uint8	sc_noisecal_incc_cnt;	/* scan noise cal counter */

	/* BFR path */
	uint8	bfr_last_snd_success;	/* Last sounding successful val */
	uint16	bfr_txndpa;		/* null data packet announcements */
	uint16	bfr_txndp;		/* null data packets */
	uint16	bfr_rxsf;
	/* BFE path */
	uint16	bfe_rxndpa_u;		/* unicast NDPAs */
	uint16	bfe_rxndpa_m;		/* multicast NDPAs */
	uint16	bfe_rpt;		/* beamforming reports */
	uint16	bfe_txsf;		/* subframes */

	uint16	txexptime;
	uint16	txdc;

	/* Misc general purpose debug counters (will be used for future debugging) */
	uint16	debug_01;
	uint16	debug_02;
	uint32  debug_03;
} phy_periodic_counters_v12_t;

#define PHY_PERIODIC_COUNTERS_VER_13	(13u)
typedef struct phy_periodic_counters_v13 {
	uint16 version;
	uint16 len;
	uint16 seq;
	uint8 PAD[2];

	/* RX error related */
	uint32	rxrsptmout;		/* number of response timeouts for transmitted frames
					* expecting a response
					*/
	uint32	rxcrsglitch;		/* PHY was able to correlate the preamble
					* but not the header
					*/
	uint32	bphy_badplcp;		/* number of bad PLCP reception on BPHY rate */
	uint32	bphy_rxcrsglitch;	/* PHY count of bphy glitches */
	uint32	rxdropped;		/* Frame dropped */
	uint32	rxnobuf;		/* Rx error due to no buffer */
	uint32	rxrunt;			/* Runt frame counter */
	uint32	rxbadfcs;		/* number of frames for which the CRC check failed
					* in the MAC
					*/

	/* RX related */
	uint32	rxstrt;			/* number of received frames with a good PLCP */
	uint32	rxbeaconmbss;		/* beacons received from member of BSS */
	uint32	rxdtucastmbss;		/* number of received DATA frames with good FCS
					* and matching RA
					*/
	uint32	rxdtocast;		/* number of received DATA frames
					* (good FCS and no matching RA)
					*/
	uint32  goodfcs;		/* Good fcs counters  */
	uint32	rxctl;			/* Number of control frames */
	uint32	rxaction;		/* Number of action frames */
	uint32	rxback;			/* Number of block ack frames rcvd */
	uint32	rxctlucast;		/* Number of received unicast ctl frames */
	uint32	rxframe;		/* Number of received frames */

	uint32	rxbar;			/* Number of block ack requests rcvd */
	uint32	rxackucast;		/* number of ucast ACKS received (good FCS) */
	uint32	rxbeaconobss;		/* number of OBSS beacons received */
	uint32	rxctsucast;		/* number of unicast CTS addressed to the MAC (good FCS) */
	uint32	rxrtsucast;		/* number of unicast RTS addressed to the MAC (good FCS) */
	uint32	rxauth;			/* Number of RX AUTH */
	uint32	rxdeauth;		/* Number of RX DEAUTH */
	uint32	rxassocrsp;		/* Number of RX ASSOC response */

	/* TX related */
	uint32	txallfrm;		/* total number of frames sent, incl. Data, ACK, RTS, CTS,
					* Control Management (includes retransmissions)
					*/
	uint32	txmpdu;			/* Numer of transmitted mpdus */
	uint32	txackbackctsfrm;	/* Number of ACK + BACK + CTS */
	uint32	txackfrm;		/* number of ACK frames sent out */
	uint32	txrtsfrm;		/* number of RTS sent out by the MAC */
	uint32	txctsfrm;		/* number of CTS sent out by the MAC */

	uint32	txctl;			/* Number of control frames txd */
	uint32	txbar;			/* Number of block ack requests txd */
	uint32	txrts;			/* Number of RTS txd */
	uint32	txback;			/* Number of block ack frames txd */
	uint32	txucast;		/* number of unicast tx expecting response
					* other than cts/cwcts
					*/
	uint32	txauth;			/* Number of TX AUTH */
	uint32	txdeauth;		/* Number of TX DEAUTH */
	uint32	txassocreq;		/* Number of TX ASSOC request */

	uint32	txnull;			/* Number of TX NULL_DATA */
	uint32	txqosnull;		/* Number of TX NULL_QoSDATA */
	uint32	txnull_pm;		/* Number of TX NULL_DATA total */
	uint32	txnull_pm_succ;		/* Number of TX NULL_DATA successes */

	/* TX error related */
	uint32	txrtsfail;		/* RTS TX failure count */
	uint32	txphyerr;		/* PHY TX error count */

	uint32	last_bcn_seq_num;	/* last beacon seq no. */
	uint32	last_bcn_ltsf;		/* last beacon ltsf */

	uint32	rxanyerr;		/* Any RX error that is not counted by other counters. */

	uint32	phyovfl_cnt;		/* RX PHY FIFO overflow */
	uint32  rxf0ovfl;		/* Rx FIFO0 overflow counters information */
	uint32  rxf1ovfl;		/* Rx FIFO1 overflow counters information */

	uint32	lenfovfl_cnt;		/* RX LFIFO overflow */
	uint32	weppeof_cnt;		/* WEP asserted premature end-of-frame */
	uint32	rxbadplcp;		/* number of parity check of the PLCP header failed */
	uint32	strmeof_cnt;		/* RX frame got aborted because PHY FIFO did not have
					* sufficient bytes
					*/
	uint32	pfifo_drop;		/* PHY FIFO was not empty when a new frame arrived */
	uint32	ctx_fifo_full;		/* Low Priority Context FIFO is full */
	uint32	ctx_fifo2_full;		/* High Priority Context FIFO is full */
	uint32	rxnodelim;		/* number of no valid delimiter detected by ampdu parser */
	uint32	rx20s_cnt;		/* secondary 20 counter */
	uint32	rxdrop20s;		/* RX was discarded as the CRS was not seen on
					* primary channel
					*/
	uint32	new_rxin_plcp_wait_cnt;	/* A new reception started waiting for PLCP bytes
					* from a previous receive
					*/
	uint32  rxtoolate;		/* receive too late */
	uint32	laterx_cnt;		/* RX frame dropped as it was seen too (30us) late
					* from the start of reception
					*/
	uint32	rxfrmtoolong;		/* Number of received frame that are too long */
	uint32	rxfrmtooshrt;		/* RX frame was dropped as it did not meet minimum
					* number of bytes to be a valid 802.11 frame
					*/

	uint32	rxlegacyfrminvalid;	/* Invalid BPHY or L-OFDM reception */
	uint32	txsifserr;		/* A frame arrived in SIFS while we were about to
					* transmit B/ACK
					*/
	uint32	ooseq_macsusp;		/* Ucode is out of sequence in processing reception
					* (especially due to macsuspend).
					* RX MEND is seen without RX STRT
					*/
	uint32	desense_reason;		/* desense paraemters to indicate reasons
					* for bphy and ofdm_desense
					*/

	uint32	tbtt;			/* Per-interface stats report tbtt count */
	uint32  p2ptbtt;		/* MCNX TBTT */
	uint32  p2ptbttmiss;		/* TBTT coming when the radio is on an off channel */
	uint32  noise_iqest_to;		/* Count of IQ Est timeout during noise measurement */
	uint32  ctmode_ufc_cnt;		/* Underflow cnt in ctmode */
	uint32	fourwayfail;		/* FourWayHandshakeFailures */
	uint16  missbcn_dbg;		/* Number of beacon missed to receive */

	uint8	sc_dccal_incc_cnt;	/* scan dccal counter */
	uint8	sc_rxiqcal_skip_cnt;	/* scan rxiqcal counter */
	uint8	sc_noisecal_incc_cnt;	/* scan noise cal counter */

	/* BFR path */
	uint8	bfr_last_snd_success;	/* Last sounding successful val */
	uint16	bfr_txndpa;		/* null data packet announcements */
	uint16	bfr_txndp;		/* null data packets */
	uint16	bfr_rxsf;
	/* BFE path */
	uint16	bfe_rxndpa_u;		/* unicast NDPAs */
	uint16	bfe_rxndpa_m;		/* multicast NDPAs */
	uint16	bfe_rpt;		/* beamforming reports */
	uint16	bfe_txsf;		/* subframes */

	uint16	txexptime;
	uint16	txdc;

	/* Misc general purpose debug counters (will be used for future debugging) */
	uint16	debug_01;
	uint16	debug_02;
} phy_periodic_counters_v13_t;

// From v12 for v30
#define PHY_PERIODIC_COUNTERS_VER_14	(14u)
typedef struct phy_periodic_counters_v14 {
	uint16 version;
	uint16 len;
	uint16 seq;
	uint8 PAD[2];

	/* RX error related */
	uint32	rxrsptmout_mpdu;	/* number of response timeouts for transmitted frames
					* expecting a response
					*/
	uint32	rxcrsglitch;		/* PHY was able to correlate the preamble
					* but not the header
					*/
	uint32	bphy_badplcp;		/* number of bad PLCP reception on BPHY rate */
	uint32	bphy_rxcrsglitch;	/* PHY count of bphy glitches */
	uint32	rxdropped;		/* Frame dropped */
	uint32	rxnobuf;		/* Rx error due to no buffer */
	uint32	rxrunt;			/* Runt frame counter */
	uint32	rxbadfcs;		/* number of frames for which the CRC check failed
					* in the MAC
					*/

	/* RX related */
	uint32	rxstrt_ampdu;		/* number of received frames with a good PLCP */
	uint32	rxbeaconmbss;		/* beacons received from member of BSS */
	uint32	rxdtucastmbss;		/* number of received DATA frames with good FCS
					* and matching RA
					*/
	uint32	rxdtocast;		/* number of received DATA frames
					* (good FCS and no matching RA)
					*/
	uint32  goodfcs;		/* Good fcs counters  */
	uint32	rxctl_mgt;		/* Number of control frames */
	uint32	rxaction;		/* Number of action frames */
	uint32	rxback;			/* Number of block ack frames rcvd */
	uint32	rxctlucast;		/* Number of received unicast ctl frames */
	uint32	rxframe_mpdu;		/* Number of received frames */

	uint32	rxbar;			/* Number of block ack requests rcvd */
	uint32	rxackucast;		/* number of ucast ACKS received (good FCS) */
	uint32	rxbeaconobss;		/* number of OBSS beacons received */
	uint32	rxctsucast;		/* number of unicast CTS addressed to the MAC (good FCS) */
	uint32	rxrtsucast;		/* number of unicast RTS addressed to the MAC (good FCS) */
	uint32	rxauth;			/* Number of RX AUTH */
	uint32	rxdeauth;		/* Number of RX DEAUTH */
	uint32	rxassocrsp;		/* Number of RX ASSOC response */

	/* TX related */
	uint32	txallfrm;		/* total number of frames sent, incl. Data, ACK, RTS, CTS,
					* Control Management (includes retransmissions)
					*/
	uint32	txmpdu;			/* Numer of transmitted mpdus */
	uint32	txackbackctsfrm;	/* Number of ACK + BACK + CTS */
	uint32	txackfrm;		/* number of ACK frames sent out */
	uint32	txrtsfrm;		/* number of RTS sent out by the MAC */
	uint32	txctsfrm;		/* number of CTS sent out by the MAC */

	uint32	txctl;			/* Number of control frames txd */
	uint32	txbar;			/* Number of block ack requests txd */
	uint32	txrts;			/* Number of RTS txd */
	uint32	txback;			/* Number of block ack frames txd */
	uint32	txucast;		/* number of unicast tx expecting response
					* other than cts/cwcts
					*/
	uint32	txauth;			/* Number of TX AUTH */
	uint32	txdeauth;		/* Number of TX DEAUTH */
	uint32	txassocreq;		/* Number of TX ASSOC request */

	uint32	txnull;			/* Number of TX NULL_DATA */
	uint32	txqosnull;		/* Number of TX NULL_QoSDATA */
	uint32	txnull_pm;		/* Number of TX NULL_DATA total */
	uint32	txnull_pm_succ;		/* Number of TX NULL_DATA successes */

	/* TX error related */
	uint32	txrtsfail_BA;		/* RTS TX failure count */
	uint32	txphyerr;		/* PHY TX error count */

	uint32	last_bcn_seq_num;	/* last beacon seq no. */
	uint32	last_bcn_ltsf;		/* last beacon ltsf */

	uint32	rxanyerr;		/* Any RX error that is not counted by other counters. */

	uint32	phyovfl_cnt;		/* RX PHY FIFO overflow */
	uint32  rxf0ovfl;		/* Rx FIFO0 overflow counters information */
	uint32  rxf1ovfl;		/* Rx FIFO1 overflow counters information */

	uint32	lenfovfl_cnt;		/* RX LFIFO overflow */
	uint32	weppeof_cnt;		/* WEP asserted premature end-of-frame */
	uint32	rxbadplcp;		/* number of parity check of the PLCP header failed */
	uint32	strmeof_cnt;		/* RX frame got aborted because PHY FIFO did not have
					* sufficient bytes
					*/
	uint32	pfifo_drop;		/* PHY FIFO was not empty when a new frame arrived */
	uint32	ctx_fifo_full;		/* Low Priority Context FIFO is full */
	uint32	ctx_fifo2_full;		/* High Priority Context FIFO is full */
	uint32	rxnodelim;		/* number of no valid delimiter detected by ampdu parser */
	uint32	rx20s_cnt;		/* secondary 20 counter */
	uint32	rxdrop20s;		/* RX was discarded as the CRS was not seen on
					* primary channel
					*/
	uint32	new_rxin_plcp_wait_cnt;	/* A new reception started waiting for PLCP bytes
					* from a previous receive
					*/
	uint32  rxtoolate;		/* receive too late */
	uint32	laterx_cnt;		/* RX frame dropped as it was seen too (30us) late
					* from the start of reception
					*/
	uint32	rxfrmtoolong;		/* Number of received frame that are too long */
	uint32	rxfrmtooshrt;		/* RX frame was dropped as it did not meet minimum
					* number of bytes to be a valid 802.11 frame
					*/

	uint32	rxlegacyfrminvalid;	/* Invalid BPHY or L-OFDM reception */
	uint32	txsifserr;		/* A frame arrived in SIFS while we were about to
					* transmit B/ACK
					*/
	uint32	ooseq_macsusp;		/* Ucode is out of sequence in processing reception
					* (especially due to macsuspend).
					* RX MEND is seen without RX STRT
					*/
	uint32	desense_reason;		/* desense paraemters to indicate reasons
					* for bphy and ofdm_desense
					*/

	uint32	tbtt;			/* Per-interface stats report tbtt count */
	uint32  p2ptbtt;		/* MCNX TBTT */
	uint32  p2ptbttmiss;		/* TBTT coming when the radio is on an off channel */
	uint32  noise_iqest_to;		/* Count of IQ Est timeout during noise measurement */
	uint32  ctmode_ufc_cnt;		/* Underflow cnt in ctmode */
	uint32	fourwayfail;		/* FourWayHandshakeFailures */
	uint16  missbcn_dbg;		/* Number of beacon missed to receive */

	uint8	sc_dccal_incc_cnt;	/* scan dccal counter */
	uint8	sc_rxiqcal_skip_cnt;	/* scan rxiqcal counter */
	uint8	sc_noisecal_incc_cnt;	/* scan noise cal counter */

	/* BFR path */
	uint8	bfr_last_snd_success;	/* Last sounding successful val */
	uint16	bfr_txndpa;		/* null data packet announcements */
	uint16	bfr_txndp;		/* null data packets */
	uint16	bfr_rxsf;
	/* BFE path */
	uint16	bfe_rxndpa_u;		/* unicast NDPAs */
	uint16	bfe_rxndpa_m;		/* multicast NDPAs */
	uint16	bfe_rpt;		/* beamforming reports */
	uint16	bfe_txsf;		/* subframes */

	uint16	txexptime;
	uint16	txdc;

	uint32	he_rxtrig_myaid;	/* rxed valid trigger frame with myaid */
	uint32	he_rxtrig_murts;	/* reception of MU-RTS trigger frame */
	uint32	he_null_tbppdu;		/* null TB PPDU's sent as response to basic trig frame */
	uint32	he_txtbppdu;		/* increments on transmission of every TB PPDU */
	uint32	he_colormiss;		/* for bss color mismatch cases */
	uint32	txretrans;		/* tx mac retransmits */
	uint32	txsu;
	uint32	rx_toss;

	uint32	txampdu;		/* number of AMPDUs transmitted */
	uint32	rxampdu;		/* ampdus recd */
	uint32	rxmpdu;			/* mpdus recd in a ampdu */
	uint32	rxnull;			/* Number of RX NULL_DATA */
	uint32	rxqosnull;		/* Number of RX NULL_QoSDATA */
	uint32	txmpduperampdu;
	uint32	rxmpduperampdu;

	/* Misc general purpose debug counters (will be used for future debugging) */
	uint16	debug_01;
	uint16	debug_02;
	uint32  debug_03;
	uint32  debug_04;
} phy_periodic_counters_v14_t;

// From v14 for v41
typedef struct phy_periodic_counters_v15 {
	/* RX error related */
	uint32	rxrsptmout_mpdu;	/* number of response timeouts for transmitted frames
					* expecting a response
					*/
	uint32	rxcrsglitch;		/* PHY was able to correlate the preamble
					* but not the header
					*/
	uint32	bphy_badplcp;		/* number of bad PLCP reception on BPHY rate */
	uint32	bphy_rxcrsglitch;	/* PHY count of bphy glitches */
	uint32	rxdropped;		/* Frame dropped */
	uint32	rxnobuf;		/* Rx error due to no buffer */
	uint32	rxrunt;			/* Runt frame counter */
	uint32	rxbadfcs;		/* number of frames for which the CRC check failed
					* in the MAC
					*/

	/* RX related */
	uint32	rxstrt_phy;		/* number of received frames with a good PLCP */
	uint32	rxbeaconmbss;		/* beacons received from member of BSS */
	uint32	rxdtucastmbss;		/* number of received DATA frames with good FCS
					* and matching RA
					*/
	uint32	rxdtocast;		/* number of received DATA frames
					* (good FCS and no matching RA)
					*/
	uint32  goodfcs;		/* Good fcs counters  */
	uint32	rxctl_mgt;		/* Number of control frames */
	uint32	rxaction;		/* Number of action frames */
	uint32	rxback;			/* Number of block ack frames rcvd */
	uint32	rxctlucast;		/* Number of received unicast ctl frames */
	uint32	rxframe_mpdu;		/* Number of received frames */

	uint32	rxbar;			/* Number of block ack requests rcvd */
	uint32	rxackucast;		/* number of ucast ACKS received (good FCS) */
	uint32	rxbeaconobss;		/* number of OBSS beacons received */
	uint32	rxctsucast;		/* number of unicast CTS addressed to the MAC (good FCS) */
	uint32	rxrtsucast;		/* number of unicast RTS addressed to the MAC (good FCS) */
	uint32	rxauth;			/* Number of RX AUTH */
	uint32	rxdeauth;		/* Number of RX DEAUTH */
	uint32	rxassocrsp;		/* Number of RX ASSOC response */

	/* TX related */
	uint32	txallfrm;		/* total number of frames sent, incl. Data, ACK, RTS, CTS,
					* Control Management (includes retransmissions)
					*/
	uint32	txmpdu;			/* Numer of transmitted mpdus */
	uint32	txackbackctsfrm;	/* Number of ACK + BACK + CTS */
	uint32	txackfrm;		/* number of ACK frames sent out */
	uint32	txrtsfrm;		/* number of RTS sent out by the MAC */
	uint32	txctsfrm;		/* number of CTS sent out by the MAC */

	uint32	txctl;			/* Number of control frames txd */
	uint32	txbar;			/* Number of block ack requests txd */
	uint32	txrts;			/* Number of RTS txd */
	uint32	txback;			/* Number of block ack frames txd */
	uint32	txucast;		/* number of unicast tx expecting response
					* other than cts/cwcts
					*/
	uint32	txauth;			/* Number of TX AUTH */
	uint32	txdeauth;		/* Number of TX DEAUTH */
	uint32	txassocreq;		/* Number of TX ASSOC request */

	uint32	txnull;			/* Number of TX NULL_DATA */
	uint32	txqosnull;		/* Number of TX NULL_QoSDATA */
	uint32	txnull_pm;		/* Number of TX NULL_DATA total */
	uint32	txnull_pm_succ;		/* Number of TX NULL_DATA successes */

	/* TX error related */
	uint32	txrtsfail_BA;		/* RTS TX failure count */
	uint32	txphyerr;		/* PHY TX error count */

	uint32	last_bcn_seq_num;	/* last beacon seq no. */
	uint32	last_bcn_ltsf;		/* last beacon ltsf */

	uint32	rxanyerr;		/* Any RX error that is not counted by other counters. */

	uint32	phyovfl_cnt;		/* RX PHY FIFO overflow */
	uint32  rxf0ovfl;		/* Rx FIFO0 overflow counters information */
	uint32  rxf1ovfl;		/* Rx FIFO1 overflow counters information */

	uint32	lenfovfl_cnt;		/* RX LFIFO overflow */
	uint32	weppeof_cnt;		/* WEP asserted premature end-of-frame */
	uint32	rxbadplcp;		/* number of parity check of the PLCP header failed */
	uint32	pfifo_drop;		/* PHY FIFO was not empty when a new frame arrived */
	uint32	ctx_fifo_full;		/* Low Priority Context FIFO is full */
	uint32	ctx_fifo2_full;		/* High Priority Context FIFO is full */
	uint32	rxnodelim;		/* number of no valid delimiter detected by ampdu parser */
	uint32	rx20s_cnt;		/* secondary 20 counter */
	uint32	rxdrop20s;		/* RX was discarded as the CRS was not seen on
					* primary channel
					*/
	uint32	new_rxin_plcp_wait_cnt;	/* A new reception started waiting for PLCP bytes
					* from a previous receive
					*/
	uint32  rxtoolate;		/* receive too late */
	uint32	laterx_cnt;		/* RX frame dropped as it was seen too (30us) late
					* from the start of reception
					*/
	uint32	rxfrmtoolong;		/* Number of received frame that are too long */
	uint32	rxfrmtooshrt;		/* RX frame was dropped as it did not meet minimum
					* number of bytes to be a valid 802.11 frame
					*/

	uint32	rxlegacyfrminvalid;	/* Invalid BPHY or L-OFDM reception */
	uint32	txsifserr;		/* A frame arrived in SIFS while we were about to
					* transmit B/ACK
					*/
	uint32	ooseq_macsusp;		/* Ucode is out of sequence in processing reception
					* (especially due to macsuspend).
					* RX MEND is seen without RX STRT
					*/
	uint32	desense_reason;		/* desense paraemters to indicate reasons
					* for bphy and ofdm_desense
					*/

	uint32	tbtt;			/* Per-interface stats report tbtt count */
	uint32  p2ptbtt;		/* MCNX TBTT */
	uint32  p2ptbttmiss;		/* TBTT coming when the radio is on an off channel */
	uint32  noise_iqest_to;		/* Count of IQ Est timeout during noise measurement */
	uint32  ctmode_ufc_cnt;		/* Underflow cnt in ctmode */
	uint32	fourwayfail;		/* FourWayHandshakeFailures */
	uint16  missbcn_dbg;		/* Number of beacon missed to receive */

	uint8	sc_dccal_incc_cnt;	/* scan dccal counter */
	uint8	sc_rxiqcal_skip_cnt;	/* scan rxiqcal counter */
	uint8	sc_noisecal_incc_cnt;	/* scan noise cal counter */

	/* BFR path */
	uint8	bfr_last_snd_success;	/* Last sounding successful val */
	uint16	bfr_txndpa;		/* null data packet announcements */
	uint16	bfr_txndp;		/* null data packets */
	uint16	bfr_rxsf;
	/* BFE path */
	uint16	bfe_rxndpa_u;		/* unicast NDPAs */
	uint16	bfe_rxndpa_m;		/* multicast NDPAs */
	uint16	bfe_rpt;		/* beamforming reports */
	uint16	bfe_txsf;		/* subframes */

	uint16	txexptime;
	uint8	tx_links;		/* Number of per-link stats supported on slice */
	uint8	rx_links;		/* Number of per-link stats supported on slice */

	uint32	he_rxtrig_myaid;	/* rxed valid trigger frame with myaid */
	uint32	he_rxtrig_murts;	/* reception of MU-RTS trigger frame */
	uint32	he_null_tbppdu;		/* null TB PPDU's sent as response to basic trig frame */
	uint32	he_txtbppdu;		/* increments on transmission of every TB PPDU */
	uint32	he_colormiss;		/* for bss color mismatch cases */
	uint32	txretrans;		/* tx mac retransmits */
	uint32	txsu;
	uint32	rx_toss;

	uint32	txampdu;		/* number of AMPDUs transmitted */
	uint32	rxampdu;		/* ampdus recd */
	uint32	rxmpdu;			/* mpdus recd in a ampdu */
	uint32	rxnull;			/* Number of RX NULL_DATA */
	uint32	rxqosnull;		/* Number of RX NULL_QoSDATA */
	uint32	txmpduperampdu;
	uint32	rxmpduperampdu;

	uint32	chup_mode0;	/* Channel update mode 0 counter legacy */
	uint32	chup_mode1;	/* Channel update mode 1 counter itr */
	uint32	dmd_mode0;	/* DynamicML feature ZF */
	uint32	dmd_mode1;	/* DynamicML feature ML */

	/* Misc general purpose debug counters (will be used for future debugging) */
	uint32	debug_01;
	uint32	debug_02;
	uint32	debug_03;
} phy_periodic_counters_v15_t;

// From v15 for v42
typedef struct phy_periodic_counters_v16 {
	/* RX error related */
	uint32	rxrsptmout_mpdu;	/* number of response timeouts for transmitted frames
					* expecting a response
					*/
	uint32	rxcrsglitch;		/* PHY was able to correlate the preamble
					* but not the header
					*/
	uint32	bphy_badplcp;		/* number of bad PLCP reception on BPHY rate */
	uint32	bphy_rxcrsglitch;	/* PHY count of bphy glitches */
	uint32	rxdropped;		/* Frame dropped */
	uint32	rxnobuf;		/* Rx error due to no buffer */
	uint32	rxrunt;			/* Runt frame counter */
	uint32	rxbadfcs;		/* number of frames for which the CRC check failed
					* in the MAC
					*/

	/* RX related */
	uint32	rxstrt_phy;		/* number of received frames with a good PLCP */
	uint32	rxbeaconmbss;		/* beacons received from member of BSS */
	uint32	rxdtucastmbss;		/* number of received DATA frames with good FCS
					* and matching RA
					*/
	uint32	rxdtocast;		/* number of received DATA frames
					* (good FCS and no matching RA)
					*/
	uint32  goodfcs;		/* Good fcs counters  */
	uint32	rxctl_mgt;		/* Number of control frames */
	uint32	rxaction;		/* Number of action frames */
	uint32	rxback;			/* Number of block ack frames rcvd */
	uint32	rxctlucast;		/* Number of received unicast ctl frames */
	uint32	rxframe_mpdu;		/* Number of received frames */

	uint32	rxbar;			/* Number of block ack requests rcvd */
	uint32	rxackucast;		/* number of ucast ACKS received (good FCS) */
	uint32	rxbeaconobss;		/* number of OBSS beacons received */
	uint32	rxctsucast;		/* number of unicast CTS addressed to the MAC (good FCS) */
	uint32	rxrtsucast;		/* number of unicast RTS addressed to the MAC (good FCS) */
	uint32	rxauth;			/* Number of RX AUTH */
	uint32	rxdeauth;		/* Number of RX DEAUTH */
	uint32	rxassocrsp;		/* Number of RX ASSOC response */

	/* TX related */
	uint32	txallfrm;		/* total number of frames sent, incl. Data, ACK, RTS, CTS,
					* Control Management (includes retransmissions)
					*/
	uint32	txmpdu;			/* Numer of transmitted mpdus */
	uint32	txackbackctsfrm;	/* Number of ACK + BACK + CTS */
	uint32	txackfrm;		/* number of ACK frames sent out */
	uint32	txrtsfrm;		/* number of RTS sent out by the MAC */
	uint32	txctsfrm;		/* number of CTS sent out by the MAC */

	uint32	txctl;			/* Number of control frames txd */
	uint32	txbar;			/* Number of block ack requests txd */
	uint32	txrts;			/* Number of RTS txd */
	uint32	txback;			/* Number of block ack frames txd */
	uint32	txucast;		/* number of unicast tx expecting response
					* other than cts/cwcts
					*/
	uint32	txauth;			/* Number of TX AUTH */
	uint32	txdeauth;		/* Number of TX DEAUTH */
	uint32	txassocreq;		/* Number of TX ASSOC request */

	uint32	txnull;			/* Number of TX NULL_DATA */
	uint32	txqosnull;		/* Number of TX NULL_QoSDATA */
	uint32	txnull_pm;		/* Number of TX NULL_DATA total */
	uint32	txnull_pm_succ;		/* Number of TX NULL_DATA successes */

	/* TX error related */
	uint32	txrtsfail_BA;		/* RTS TX failure count */
	uint32	txphyerr;		/* PHY TX error count */

	uint32	last_bcn_seq_num;	/* last beacon seq no. */
	uint32	last_bcn_ltsf;		/* last beacon ltsf */

	uint32	rxanyerr;		/* Any RX error that is not counted by other counters. */

	uint32	phyovfl_cnt;		/* RX PHY FIFO overflow */
	uint32  rxf0ovfl;		/* Rx FIFO0 overflow counters information */
	uint32  rxf1ovfl;		/* Rx FIFO1 overflow counters information */

	uint32	lenfovfl_cnt;		/* RX LFIFO overflow */
	uint32	weppeof_cnt;		/* WEP asserted premature end-of-frame */
	uint32	rxbadplcp;		/* number of parity check of the PLCP header failed */
	uint32	strmeof_cnt;		/* RX frame got aborted because PHY FIFO did not have
					* sufficient bytes
					*/
	uint32	pfifo_drop;		/* PHY FIFO was not empty when a new frame arrived */
	uint32	ctx_fifo_full;		/* Low Priority Context FIFO is full */
	uint32	ctx_fifo2_full;		/* High Priority Context FIFO is full */
	uint32	rxnodelim;		/* number of no valid delimiter detected by ampdu parser */
	uint32	rx20s_cnt;		/* secondary 20 counter */
	uint32	rxdrop20s;		/* RX was discarded as the CRS was not seen on
					* primary channel
					*/
	uint32	new_rxin_plcp_wait_cnt;	/* A new reception started waiting for PLCP bytes
					* from a previous receive
					*/
	uint32  rxtoolate;		/* receive too late */
	uint32	laterx_cnt;		/* RX frame dropped as it was seen too (30us) late
					* from the start of reception
					*/
	uint32	rxfrmtoolong;		/* Number of received frame that are too long */
	uint32	rxfrmtooshrt;		/* RX frame was dropped as it did not meet minimum
					* number of bytes to be a valid 802.11 frame
					*/

	uint32	rxlegacyfrminvalid;	/* Invalid BPHY or L-OFDM reception */
	uint32	txsifserr;		/* A frame arrived in SIFS while we were about to
					* transmit B/ACK
					*/
	uint32	ooseq_macsusp;		/* Ucode is out of sequence in processing reception
					* (especially due to macsuspend).
					* RX MEND is seen without RX STRT
					*/
	uint32	desense_reason;		/* desense paraemters to indicate reasons
					* for bphy and ofdm_desense
					*/

	uint32	tbtt;			/* Per-interface stats report tbtt count */
	uint32  p2ptbtt;		/* MCNX TBTT */
	uint32  p2ptbttmiss;		/* TBTT coming when the radio is on an off channel */
	uint32  noise_iqest_to;		/* Count of IQ Est timeout during noise measurement */
	uint32  ctmode_ufc_cnt;		/* Underflow cnt in ctmode */
	uint32	fourwayfail;		/* FourWayHandshakeFailures */
	uint16  missbcn_dbg;		/* Number of beacon missed to receive */

	uint8	sc_dccal_incc_cnt;	/* scan dccal counter */
	uint8	sc_rxiqcal_skip_cnt;	/* scan rxiqcal counter */
	uint8	sc_noisecal_incc_cnt;	/* scan noise cal counter */

	/* BFR path */
	uint8	bfr_last_snd_success;	/* Last sounding successful val */
	uint16	bfr_txndpa;		/* null data packet announcements */
	uint16	bfr_txndp;		/* null data packets */
	uint16	bfr_rxsf;
	/* BFE path */
	uint16	bfe_rxndpa_u;		/* unicast NDPAs */
	uint16	bfe_rxndpa_m;		/* multicast NDPAs */
	uint16	bfe_rpt;		/* beamforming reports */
	uint16	bfe_txsf;		/* subframes */

	uint16	txexptime;
	uint8	tx_links;		/* Number of per-link stats supported on slice */
	uint8	rx_links;		/* Number of per-link stats supported on slice */

	uint32	he_rxtrig_myaid;	/* rxed valid trigger frame with myaid */
	uint32	he_rxtrig_murts;	/* reception of MU-RTS trigger frame */
	uint32	he_null_tbppdu;		/* null TB PPDU's sent as response to basic trig frame */
	uint32	he_txtbppdu;		/* increments on transmission of every TB PPDU */
	uint32	he_colormiss;		/* for bss color mismatch cases */
	uint32	txretrans;		/* tx mac retransmits */
	uint32	txsu;
	uint32	rx_toss;

	uint32	txampdu;		/* number of AMPDUs transmitted */
	uint32	rxampdu;		/* ampdus recd */
	uint32	rxmpdu;			/* mpdus recd in a ampdu */
	uint32	rxnull;			/* Number of RX NULL_DATA */
	uint32	rxqosnull;		/* Number of RX NULL_QoSDATA */
	uint32	txmpduperampdu;
	uint32	rxmpduperampdu;

	uint32	chup_mode0;		/* Channel update mode 0 counter legacy */
	uint32	chup_mode1;		/* Channel update mode 1 counter itr */
	uint32	dmd_mode0;		/* DynamicML feature ZF */
	uint32	dmd_mode1;		/* DynamicML feature ML */

	uint32	txprobereq;		/* Number of TX probe request */
	uint32	rxprobereq;		/* Number of RX probe request */
	uint32	txprobersp;		/* Number of TX probe response */
	uint32	rxprobersp;		/* Number of RX probe response */

	uint16	pending_packet_cnt;

	/* Misc general purpose debug counters (will be used for future debugging) */
	uint16	debug_01;
	uint32	debug_02;
	uint32	debug_03;
} phy_periodic_counters_v16_t;

// From v16 for v44
#define FB_HIST_BINS	8u
typedef struct phy_periodic_counters_v17 {
	/* Beacon */
	uint32	rxbeaconmbss;		/* beacons received from member of BSS */
	uint32	last_bcn_ltsf;		/* last beacon ltsf */
	uint32	rxbeaconobss;		/* number of OBSS beacons received */
	uint32	tbtt;			/* Per-interface stats report tbtt count */
	uint32  p2ptbtt;		/* MCNX TBTT */
	uint32  p2ptbttmiss;		/* TBTT coming when the radio is on an off channel */
	uint16	missbcn_dbg;		/* Number of beacon missed to receive */

	uint16	debug_cnt_01;

	/* RX */
	uint32	rxstrt;			/* number of received frames with a good PLCP */
	uint32	rxdtucastmbss;		/* number of received DATA frames with good FCS
					* and matching RA
					*/
	uint32	rxdtocast;		/* number of received DATA frames
					* (good FCS and no matching RA)
					*/
	uint32  goodfcs;		/* Good fcs counters  */
	uint32	rxctl_mgt;		/* Number of control frames */
	uint32	rxaction;		/* Number of action frames */
	uint32	rxback;			/* Number of block ack frames rcvd */
	uint32	rxctlucast;		/* Number of received unicast ctl frames */
	uint32	rxframe_mpdu;		/* Number of received frames */
	uint32	rxbar;			/* Number of block ack requests rcvd */
	uint32	rxackucast;		/* number of ucast ACKS received (good FCS) */
	uint32	rxampdu;		/* ampdus recd */
	uint32	rxmpdu;			/* mpdus recd in a ampdu */
	uint32	rxmpduperampdu;
	uint32	rxnull;			/* Number of RX NULL_DATA */
	uint32	rxqosnull;		/* Number of RX NULL_QoSDATA */
	uint32	chup_mode0_leg;		/* Channel update mode 0 counter legacy */
	uint32	chup_mode1_itr;		/* Channel update mode 1 counter itr */
	uint32	dmd_mode0_leg;		/* DynamicML feature ZF */
	uint32	dmd_mode1_itr;		/* DynamicML feature ML */
	uint32	rxmpdu_sgi;		/* Count for sgi received */

	uint32	he_rxtrig_myaid;	/* rxed valid trigger frame with myaid */
	uint32	he_rxtrig_murts;	/* reception of MU-RTS trigger frame */

	/* Rx error counters */
	uint32	rxanyerr;		/* Any RX error that is not counted by other counters. */
	uint32	pfifo_drop;		/* PHY FIFO was not empty when a new frame arrived */
	uint32	rxdrop20s;		/* RX was discarded as the CRS was not seen on
					* primary channel
					*/
	uint32	rxtoolate;		/* receive too late */
	uint32	rx20s_cnt;		/* secondary 20 counter */
	uint32	laterx_cnt;		/* RX frame dropped as it was seen too (30us) late
					* from the start of reception
					*/
	uint32	rxnodelim;		/* number of no valid delimiter detected by ampdu parser */
	uint16	rxerr_invlphy;		/* bit3 in phyrxstatus 0 (bogus frame) */
	uint16	rxerr_rsptx;		/* ACTS_PRGR set (within SIFS or PIFS) */
	uint16	rxerr_lessphybyt;	/* phy gives < 6bytes */
	uint16	rxerr_anothercrs;	/* BCC_RX_FRAM in RXCRS high */
	uint16	rxerr_rxeerratcrshigh;	/* BCC_RXE_ERR in RXCRS high */
	uint16	rxerr_sendfrmhigh;	/* Sendframe is still high after txcancel (proceed tx) */
	uint16	rxerr_tamismatchtof;	/* TA mismatched in TOF_RX */
	uint16	rxerr_phyfifoovfl;	/* BCC_RXE_ERR in read PLCP loop. maybe phy-fifo ovfl */
	uint16	rxerr_invampdu;		/* No good AMPDU under BCC_RX_AMPDU */

	uint16	debug_cnt_02;

	uint32	rxbadfcs;		/* number of frames for which the CRC check failed
					* in the MAC
					*/
	uint32	rx_toss;
	uint32	rxcrsglitch;		/* PHY was able to correlate the preamble
					* but not the header
					*/
	uint32	bphy_rxcrsglitch;	/* PHY count of bphy glitches */
	uint32	rxbadplcp;		/* number of parity check of the PLCP header failed */
	uint32	bphy_badplcp;		/* number of bad PLCP reception on BPHY rate */
	uint32	phyovfl_cnt;		/* RX PHY FIFO overflow */
	uint32  rxf0ovfl;		/* Rx FIFO0 overflow counters information */
	uint32  rxf1ovfl;		/* Rx FIFO1 overflow counters information */
	uint32	lenfovfl_cnt;		/* RX LFIFO overflow */
	uint32	weppeof_cnt;		/* WEP asserted premature end-of-frame */
	uint32	new_rxin_plcp_wait_cnt;	/* A new reception started waiting for PLCP bytes
					* from a previous receive
					*/
	uint32	rxfrmtoolong;		/* Number of received frame that are too long */
	uint32	rxfrmtooshrt;		/* RX frame was dropped as it did not meet minimum
					* number of bytes to be a valid 802.11 frame
					*/
	uint32	rxlegacyfrminvalid;	/* Invalid BPHY or L-OFDM reception */
	uint32	rxdropped;		/* Frame dropped */
	uint32	rxnobuf;		/* Rx error due to no buffer */
	uint32	rxrunt;			/* Runt frame counter */
	uint32	rxgiant;		/* rx giant frames */
	uint32	strmeof_cnt;		/* RX frame got aborted because PHY FIFO did not have
					* sufficient bytes
					*/
	uint32	ctx_fifo_full;		/* Low Priority Context FIFO is full */
	uint32	ctx_fifo2_full;		/* High Priority Context FIFO is full */
	uint32  ctmode_ufc_cnt;		/* Underflow cnt in ctmode */
	uint32	ooseq_macsusp;		/* Ucode is out of sequence in processing reception
					* (especially due to macsuspend).
					* RX MEND is seen without RX STRT
					*/
	/* Tx */
	uint32	txallfrm;		/* total number of frames sent, incl. Data, ACK, RTS, CTS,
					* Control Management (includes retransmissions)
					*/
	uint32	txmpdu;			/* Numer of transmitted mpdus */
	uint32	txampdu;		/* number of AMPDUs transmitted */
	uint32	txmpduperampdu;
	uint16	tx_pending_mpdus;
	uint16	tx_pending_mpdus_mlq;
	uint32	txackbackctsfrm;	/* Number of ACK + BACK + CTS */
	uint32	txackfrm;		/* number of ACK frames sent out */
	uint32	txctl;			/* Number of control frames txd */
	uint32	txbar;			/* Number of block ack requests txd */
	uint32	txback;			/* Number of block ack frames txd */
	uint32	txucast;		/* number of unicast tx expecting response
					* other than cts/cwcts
					*/
	uint32	txnull;			/* Number of TX NULL_DATA */
	uint32	txqosnull;		/* Number of TX NULL_QoSDATA */
	uint32	txnull_pm;		/* Number of TX NULL_DATA total */
	uint32	txnull_pm_succ;		/* Number of TX NULL_DATA successes */
	uint32	txsu;

	uint32	txretry_mpdu;		/* dot11RetryCount */
	uint32	txretrie_mpdu;		/* dot11MultipleRetryCount */
	uint32	txmpdu_sgi;		/* count for sgi transmit */

	uint32	he_null_tbppdu;		/* null TB PPDU's sent as response to basic trig frame */
	uint32	he_txtbppdu;		/* increments on transmission of every TB PPDU */
	uint32	he_omitx_success;	/* Count for OMI Tx success */

	uint16	tx_aggstp_fempty;	/* Agg stop reason FEMPTY */
	uint16	tx_fb_hist[FB_HIST_BINS];

	/* Tx error counters */
	uint16	txexptime;
	uint32	txerror;		/* Tx data errors (derived: sum of others) */
	uint32	txphyerr;		/* PHY TX error count */
	uint32	txretrans;		/* tx mac retransmits */
	uint32	rxrsptmout;		/* number of response timeouts for transmitted frames
					* expecting a response
					*/
	uint32	txsifserr;		/* A frame arrived in SIFS while we were about to
					* transmit B/ACK
					*/
	uint32	txchanrej;		/* Tx frames suppressed due to channel rejection */

	/* RTS-CTS */
	uint32	txrtsfrm;		/* number of RTS sent out by the MAC */
	uint32	rxctsucast;		/* number of unicast CTS addressed to the MAC (good FCS) */
	uint32	txrtsfail_BA;		/* RTS TX failure count */
	uint32	rxrtsucast;		/* number of unicast RTS addressed to the MAC (good FCS) */
	uint32	txctsfrm;		/* number of CTS sent out by the MAC total */
	uint32	txctsfrm_infra;		/* number of CTS sent out by the MAC for infra */
	uint32	norxfrm_aftertxcts;	/* no rxframe after cts total */
	uint32	norxfrm_aftertxcts_infra;	/* no rxframe after cts only for infra */

	/* Probe RxTx */
	uint32	txprobereq;		/* Number of TX probe request */
	uint32	rxprobersp;		/* Number of RX probe response */
	uint32	rxprobereq;		/* Number of RX probe request */
	uint32	txprobersp;		/* Number of TX probe response */

	/* Join */
	uint32	txauth;			/* Number of TX AUTH */
	uint32	rxauth;			/* Number of RX AUTH */
	uint32	txdeauth;		/* Number of TX DEAUTH */
	uint32	rxdeauth;		/* Number of RX DEAUTH */
	uint32	txassocreq;		/* Number of TX ASSOC request */
	uint32	rxassocrsp;		/* Number of RX ASSOC response */
	uint32	fourwayfail;		/* FourWayHandshakeFailures */
	uint32	txdisassoc;		/* Number of TX DISASSOC */
	uint32	rxdisassoc;		/* Number of RX DISASSOC */

	/* Beamforming */
	uint32	bfe_rxndpa_u;		/* unicast NDPAs */
	uint32	bfe_rxndpa_m;		/* multicast NDPAs */
	uint32	bfe_rpt;		/* beamforming reports */
	uint32	bfe_txsf;		/* subframes */

	/* Scan core */
	uint8	sc_dccal_incc_cnt;	/* scan dccal counter */
	uint8	sc_rxiqcal_skip_cnt;	/* scan rxiqcal counter */
	uint8	sc_noisecal_incc_cnt;	/* scan noise cal counter */

	/* Misc general purpose debug counters (will be used for future debugging) */
	uint8	debug_cnt_03;
	uint32	debug_cnt_04;
	uint32	debug_cnt_05;
} phy_periodic_counters_v17_t;

#define PHY_PERIODIC_COUNTERS_VER_255	(255u)
typedef struct phy_periodic_counters_v255 {
	/* Beacon */
	uint32	rxbeaconmbss;		/* beacons received from member of BSS */
	uint32	last_bcn_ltsf;		/* last beacon ltsf */
	uint32	rxbeaconobss;		/* number of OBSS beacons received */
	uint32	tbtt;			/* Per-interface stats report tbtt count */
	uint32  p2ptbtt;		/* MCNX TBTT */
	uint32  p2ptbttmiss;		/* TBTT coming when the radio is on an off channel */
	uint16	missbcn_dbg;		/* Number of beacon missed to receive */

	uint16	debug_cnt_01;

	/* RX */
	uint32	rxstrt;			/* number of received frames with a good PLCP */
	uint32	rxdtucastmbss;		/* number of received DATA frames with good FCS
					* and matching RA
					*/
	uint32	rxdtocast;		/* number of received DATA frames
					* (good FCS and no matching RA)
					*/
	uint32  goodfcs;		/* Good fcs counters  */
	uint32	rxctl_mgt;		/* Number of control frames */
	uint32	rxaction;		/* Number of action frames */
	uint32	rxback;			/* Number of block ack frames rcvd */
	uint32	rxctlucast;		/* Number of received unicast ctl frames */
	uint32	rxframe_mpdu;		/* Number of received frames */
	uint32	rxbar;			/* Number of block ack requests rcvd */
	uint32	rxackucast;		/* number of ucast ACKS received (good FCS) */
	uint32	rxampdu;		/* ampdus recd */
	uint32	rxmpdu;			/* mpdus recd in a ampdu */
	uint32	rxmpduperampdu;
	uint32	rxnull;			/* Number of RX NULL_DATA */
	uint32	rxqosnull;		/* Number of RX NULL_QoSDATA */
	uint32	chup_mode0_leg;		/* Channel update mode 0 counter legacy */
	uint32	chup_mode1_itr;		/* Channel update mode 1 counter itr */
	uint32	dmd_mode0_leg;		/* DynamicML feature ZF */
	uint32	dmd_mode1_itr;		/* DynamicML feature ML */
	uint32	rxmpdu_sgi;		/* Count for sgi received */

	uint32	he_rxtrig_myaid;	/* rxed valid trigger frame with myaid */
	uint32	he_rxtrig_murts;	/* reception of MU-RTS trigger frame */

	/* Rx error counters */
	uint32	rxanyerr;		/* Any RX error that is not counted by other counters. */
	uint32	pfifo_drop;		/* PHY FIFO was not empty when a new frame arrived */
	uint32	rxdrop20s;		/* RX was discarded as the CRS was not seen on
					* primary channel
					*/
	uint32	rxtoolate;		/* receive too late */
	uint32	rx20s_cnt;		/* secondary 20 counter */
	uint32	laterx_cnt;		/* RX frame dropped as it was seen too (30us) late
					* from the start of reception
					*/
	uint32	rxnodelim;		/* number of no valid delimiter detected by ampdu parser */
	uint16	rxerr_invlphy;		/* bit3 in phyrxstatus 0 (bogus frame) */
	uint16	rxerr_rsptx;		/* ACTS_PRGR set (within SIFS or PIFS) */
	uint16	rxerr_lessphybyt;	/* phy gives < 6bytes */
	uint16	rxerr_anothercrs;	/* BCC_RX_FRAM in RXCRS high */
	uint16	rxerr_rxeerratcrshigh;	/* BCC_RXE_ERR in RXCRS high */
	uint16	rxerr_sendfrmhigh;	/* Sendframe is still high after txcancel (proceed tx) */
	uint16	rxerr_tamismatchtof;	/* TA mismatched in TOF_RX */
	uint16	rxerr_phyfifoovfl;	/* BCC_RXE_ERR in read PLCP loop. maybe phy-fifo ovfl */
	uint16	rxerr_invampdu;		/* No good AMPDU under BCC_RX_AMPDU */
	uint16	rxerr_total;

	uint32	rxbadfcs;		/* number of frames for which the CRC check failed
					* in the MAC
					*/
	uint32	rx_toss;
	uint32	rxcrsglitch;		/* PHY was able to correlate the preamble
					* but not the header
					*/
	uint32	bphy_rxcrsglitch;	/* PHY count of bphy glitches */
	uint32	rxbadplcp;		/* number of parity check of the PLCP header failed */
	uint32	bphy_badplcp;		/* number of bad PLCP reception on BPHY rate */
	uint32	phyovfl_cnt;		/* RX PHY FIFO overflow */
	uint32  rxf0ovfl;		/* Rx FIFO0 overflow counters information */
	uint32  rxf1ovfl;		/* Rx FIFO1 overflow counters information */
	uint32	lenfovfl_cnt;		/* RX LFIFO overflow */
	uint32	weppeof_cnt;		/* WEP asserted premature end-of-frame */
	uint32	new_rxin_plcp_wait_cnt;	/* A new reception started waiting for PLCP bytes
					* from a previous receive
					*/
	uint32	rxfrmtoolong;		/* Number of received frame that are too long */
	uint32	rxfrmtooshrt;		/* RX frame was dropped as it did not meet minimum
					* number of bytes to be a valid 802.11 frame
					*/
	uint32	rxlegacyfrminvalid;	/* Invalid BPHY or L-OFDM reception */
	uint32	rxdropped;		/* Frame dropped */
	uint32	rxnobuf;		/* Rx error due to no buffer */
	uint32	rxrunt;			/* Runt frame counter */
	uint32	rxgiant;		/* rx giant frames */
	uint32	debug_cnt_05;
	uint32	ctx_fifo_full;		/* Low Priority Context FIFO is full */
	uint32	ctx_fifo2_full;		/* High Priority Context FIFO is full */
	uint32  ctmode_ufc_cnt;		/* Underflow cnt in ctmode */
	uint32	ooseq_macsusp;		/* Ucode is out of sequence in processing reception
					* (especially due to macsuspend).
					* RX MEND is seen without RX STRT
					*/

	/* Tx */
	uint32	txallfrm;		/* total number of frames sent, incl. Data, ACK, RTS, CTS,
					* Control Management (includes retransmissions)
					*/
	uint32	txmpdu;			/* Numer of transmitted mpdus */
	uint32	txampdu;		/* number of AMPDUs transmitted */
	uint32	txmpduperampdu;
	uint16	tx_pending_mpdus;
	uint16	tx_pending_mpdus_mlq;
	uint32	txackbackctsfrm;	/* Number of ACK + BACK + CTS */
	uint32	txackfrm;		/* number of ACK frames sent out */
	uint32	txctl;			/* Number of control frames txd */
	uint32	txbar;			/* Number of block ack requests txd */
	uint32	txback;			/* Number of block ack frames txd */
	uint32	txucast;		/* number of unicast tx expecting response
					* other than cts/cwcts
					*/
	uint32	txnull;			/* Number of TX NULL_DATA */
	uint32	txqosnull;		/* Number of TX NULL_QoSDATA */
	uint32	txnull_pm;		/* Number of TX NULL_DATA total */
	uint32	txnull_pm_succ;		/* Number of TX NULL_DATA successes */
	uint32	txsu;

	uint32	txretry_mpdu;		/* dot11RetryCount */
	uint32	txretrie_mpdu;		/* dot11MultipleRetryCount */
	uint32	txmpdu_sgi;		/* count for sgi transmit */

	uint32	he_null_tbppdu;		/* null TB PPDU's sent as response to basic trig frame */
	uint32	he_txtbppdu;		/* increments on transmission of every TB PPDU */
	uint32	he_omitx_success;	/* Count for OMI Tx success */

	uint16	tx_aggstp_fempty;	/* Agg stop reason FEMPTY */
	uint16	tx_fb_hist[FB_HIST_BINS];

	/* Tx error counters */
	uint16	txexptime;
	uint32	txerror;		/* Tx data errors (derived: sum of others) */
	uint32	txphyerr;		/* PHY TX error count */
	uint32	txretrans;		/* tx mac retransmits */
	uint32	rxrsptmout;		/* number of response timeouts for transmitted frames
					* expecting a response
					*/
	uint32	txsifserr;		/* A frame arrived in SIFS while we were about to
					* transmit B/ACK
					*/
	uint32	txchanrej;		/* Tx frames suppressed due to channel rejection */

	/* RTS-CTS */
	uint32	txrtsfrm;		/* number of RTS sent out by the MAC */
	uint32	rxctsucast;		/* number of unicast CTS addressed to the MAC (good FCS) */
	uint32	txrtsfail_BA;		/* RTS TX failure count */
	uint32	rxrtsucast;		/* number of unicast RTS addressed to the MAC (good FCS) */
	uint32	txctsfrm;		/* number of CTS sent out by the MAC total */
	uint32	txctsfrm_infra;		/* number of CTS sent out by the MAC for infra */
	uint32	norxfrm_aftertxcts;	/* no rxframe after cts total */
	uint32	norxfrm_aftertxcts_infra;	/* no rxframe after cts only for infra */

	/* Probe RxTx */
	uint32	txprobereq;		/* Number of TX probe request */
	uint32	rxprobersp;		/* Number of RX probe response */
	uint32	rxprobereq;		/* Number of RX probe request */
	uint32	txprobersp;		/* Number of TX probe response */

	/* Join */
	uint32	txauth;			/* Number of TX AUTH */
	uint32	rxauth;			/* Number of RX AUTH */
	uint32	txdeauth;		/* Number of TX DEAUTH */
	uint32	rxdeauth;		/* Number of RX DEAUTH */
	uint32	txassocreq;		/* Number of TX ASSOC request */
	uint32	rxassocrsp;		/* Number of RX ASSOC response */
	uint32	fourwayfail;		/* FourWayHandshakeFailures */
	uint32	txdisassoc;		/* Number of TX DISASSOC */
	uint32	rxdisassoc;		/* Number of RX DISASSOC */

	/* Beamforming - BFE */
	uint32	bfe_rxndpa_u;		/* unicast NDPAs */
	uint32	bfe_rxndpa_m;		/* multicast NDPAs */
	uint32	bfe_rpt;		/* beamforming reports */
	uint32	bfe_txsf;		/* subframes */
	/* BFR path */
	uint16	bfr_txndpa;		/* null data packet announcements */
	uint16	bfr_txndp;		/* null data packets */
	uint16	bfr_rxsf;
	uint8	bfr_last_snd_success;	/* Last sounding successful val */

	/* Scan core */
	uint8	sc_dccal_incc_cnt;	/* scan dccal counter */
	uint8	sc_rxiqcal_skip_cnt;	/* scan rxiqcal counter */
	uint8	sc_noisecal_incc_cnt;	/* scan noise cal counter */

	/* Misc general purpose debug counters (will be used for future debugging) */
	uint16	debug_cnt_02;
	uint32	debug_cnt_03;
	uint32	debug_cnt_04;
} phy_periodic_counters_v255_t;

typedef struct phycal_log_cmn {
	uint16 chanspec; /* Current phy chanspec */
	uint8  last_cal_reason;  /* Last Cal Reason */
	uint8  pad1;  /* Padding byte to align with word */
	uint32  last_cal_time; /* Last cal time in sec */
} phycal_log_cmn_t;

typedef struct phycal_log_cmn_v2 {
	uint16 chanspec;	/* current phy chanspec */
	uint8  reason;		/* cal reason */
	uint8  phase;		/* cal phase */
	uint32 time;		/* time at which cal happened in sec */
	uint16 temp;		/* temperature at the time of cal */
	uint16 dur;		/* duration of cal in usec */

	/* Misc general purpose debug counters (will be used for future debugging) */
	uint16 debug_01;	/* reused for slice info */
	uint16 debug_02;
	uint16 debug_03;
	uint16 debug_04;
} phycal_log_cmn_v2_t;

typedef struct phycal_log_core {
	uint16 ofdm_txa; /* OFDM Tx IQ Cal a coeff */
	uint16 ofdm_txb; /* OFDM Tx IQ Cal b coeff */
	uint16 ofdm_txd; /* contain di & dq */
	uint16 bphy_txa; /* BPHY Tx IQ Cal a coeff */
	uint16 bphy_txb; /* BPHY Tx IQ Cal b coeff */
	uint16 bphy_txd; /* contain di & dq */

	uint16 rxa; /* Rx IQ Cal A coeffecient */
	uint16 rxb; /* Rx IQ Cal B coeffecient */
	int32 rxs;  /* FDIQ Slope coeffecient */

	uint8 baseidx; /* TPC Base index */
	uint8 adc_coeff_cap0_adcI; /* ADC CAP Cal Cap0 I */
	uint8 adc_coeff_cap1_adcI; /* ADC CAP Cal Cap1 I */
	uint8 adc_coeff_cap2_adcI; /* ADC CAP Cal Cap2 I */
	uint8 adc_coeff_cap0_adcQ; /* ADC CAP Cal Cap0 Q */
	uint8 adc_coeff_cap1_adcQ; /* ADC CAP Cal Cap1 Q */
	uint8 adc_coeff_cap2_adcQ; /* ADC CAP Cal Cap2 Q */
	uint8 pad; /* Padding byte to align with word */
} phycal_log_core_t;

typedef struct phycal_log_core_v3 {
	uint16 ofdm_txa; /* OFDM Tx IQ Cal a coeff */
	uint16 ofdm_txb; /* OFDM Tx IQ Cal b coeff */
	uint16 ofdm_txd; /* contain di & dq */
	uint16 bphy_txa; /* BPHY Tx IQ Cal a coeff */
	uint16 bphy_txb; /* BPHY Tx IQ Cal b coeff */
	uint16 bphy_txd; /* contain di & dq */

	uint16 rxa; /* Rx IQ Cal A coeffecient */
	uint16 rxb; /* Rx IQ Cal B coeffecient */
	int32 rxs;  /* FDIQ Slope coeffecient */

	uint8 baseidx; /* TPC Base index */
	uint8 adc_coeff_cap0_adcI; /* ADC CAP Cal Cap0 I */
	uint8 adc_coeff_cap1_adcI; /* ADC CAP Cal Cap1 I */
	uint8 adc_coeff_cap2_adcI; /* ADC CAP Cal Cap2 I */
	uint8 adc_coeff_cap0_adcQ; /* ADC CAP Cal Cap0 Q */
	uint8 adc_coeff_cap1_adcQ; /* ADC CAP Cal Cap1 Q */
	uint8 adc_coeff_cap2_adcQ; /* ADC CAP Cal Cap2 Q */
	uint8 pad; /* Padding byte to align with word */

	/* Gain index based txiq ceffiecients for 2G(3 gain indices) */
	uint16 txiqlo_2g_a0; /* 2G TXIQ Cal a coeff for high TX gain */
	uint16 txiqlo_2g_b0; /* 2G TXIQ Cal b coeff for high TX gain */
	uint16 txiqlo_2g_a1; /* 2G TXIQ Cal a coeff for mid TX gain */
	uint16 txiqlo_2g_b1; /* 2G TXIQ Cal b coeff for mid TX gain */
	uint16 txiqlo_2g_a2; /* 2G TXIQ Cal a coeff for low TX gain */
	uint16 txiqlo_2g_b2; /* 2G TXIQ Cal b coeff for low TX gain */

	uint16	rxa_vpoff; /* Rx IQ Cal A coeff Vp off */
	uint16	rxb_vpoff; /* Rx IQ Cal B coeff Vp off */
	uint16	rxa_ipoff; /* Rx IQ Cal A coeff Ip off */
	uint16	rxb_ipoff; /* Rx IQ Cal B coeff Ip off */
	int32	rxs_vpoff; /* FDIQ Slope coeff Vp off */
	int32	rxs_ipoff; /* FDIQ Slope coeff Ip off */
} phycal_log_core_v3_t;

#define PHYCAL_LOG_VER1         (1u)

typedef struct phycal_log_v1 {
	uint8  version; /* Logging structure version */
	uint8  numcores; /* Numbe of cores for which core specific data present */
	uint16 length;  /* Length of the entire structure */
	phycal_log_cmn_t phycal_log_cmn; /* Logging common structure */
	/* This will be a variable length based on the numcores field defined above */
	phycal_log_core_t phycal_log_core[BCM_FLEX_ARRAY];
} phycal_log_v1_t;

typedef struct phy_periodic_log_cmn {
	uint16  chanspec; /* Current phy chanspec */
	uint16  vbatmeas; /* Measured VBAT sense value */
	uint16  featureflag; /* Currently active feature flags */
	int8    chiptemp; /* Chip temparature */
	int8    femtemp;  /* Fem temparature */

	uint32  nrate; /* Current Tx nrate */

	uint8   cal_phase_id; /* Current Multi phase cal ID */
	uint8   rxchain; /* Rx Chain */
	uint8   txchain; /* Tx Chain */
	uint8   ofdm_desense; /* OFDM desense */

	uint8   bphy_desense; /* BPHY desense */
	uint8   pll_lockstatus; /* PLL Lock status */
	uint8   pad1; /* Padding byte to align with word */
	uint8   pad2; /* Padding byte to align with word */

	uint32 duration;	/**< millisecs spent sampling this channel */
	uint32 congest_ibss;	/**< millisecs in our bss (presumably this traffic will */
				/**<  move if cur bss moves channels) */
	uint32 congest_obss;	/**< traffic not in our bss */
	uint32 interference;	/**< millisecs detecting a non 802.11 interferer. */

} phy_periodic_log_cmn_t;

typedef struct phy_periodic_log_cmn_v2 {
	uint16  chanspec; /* Current phy chanspec */
	uint16  vbatmeas; /* Measured VBAT sense value */
	uint16  featureflag; /* Currently active feature flags */
	int8    chiptemp; /* Chip temparature */
	int8    femtemp;  /* Fem temparature */

	uint32  nrate; /* Current Tx nrate */

	uint8   cal_phase_id; /* Current Multi phase cal ID */
	uint8   rxchain; /* Rx Chain */
	uint8   txchain; /* Tx Chain */
	uint8   ofdm_desense; /* OFDM desense */

	uint8   bphy_desense; /* BPHY desense */
	uint8   pll_lockstatus; /* PLL Lock status */

	uint32 duration;	/* millisecs spent sampling this channel */
	uint32 congest_ibss;	/* millisecs in our bss (presumably this traffic will */
				/*  move if cur bss moves channels) */
	uint32 congest_obss;	/* traffic not in our bss */
	uint32 interference;	/* millisecs detecting a non 802.11 interferer. */

	uint8 slice;
	uint8 version;		/* version of fw/ucode for debug purposes */
	bool phycal_disable;		/* Set if calibration is disabled */
	uint8 pad;
	uint16 phy_log_counter;
	uint16 noise_mmt_overdue;	/* Count up if ucode noise mmt is overdue for 5 sec */
	uint16 chan_switch_tm; /* Channel switch time */

	/* HP2P related params */
	uint16 shm_mpif_cnt_val;
	uint16 shm_thld_cnt_val;
	uint16 shm_nav_cnt_val;
	uint16 shm_cts_cnt_val;

	uint16 shm_m_prewds_cnt;	/* Count of pre-wds fired in the ucode */
	uint32 last_cal_time;		/* Last cal execution time */
	uint16 deaf_count;		/* Depth of stay_in_carrier_search function */
	uint32 ed20_crs0;		/* ED-CRS status on core 0 */
	uint32 ed20_crs1;		/* ED-CRS status on core 1 */
	uint32 noise_cal_req_ts;	/* Time-stamp when noise cal was requested */
	uint32 noise_cal_intr_ts;	/* Time-stamp when noise cal was completed */
	uint32 phywdg_ts;		/* Time-stamp when wd was fired */
	uint32 phywd_dur;			/* Duration of the watchdog */
	uint32 noise_mmt_abort_crs; /* Count of CRS during noise mmt */
	uint32 chanspec_set_ts;		/* Time-stamp when chanspec was set */
	uint32 vcopll_failure_cnt;	/* Number of VCO cal failures
					* (including failures detected in ucode).
					*/
	uint32 dcc_fail_counter;	/* Number of DC cal failures */
	uint32 log_ts;			/* Time-stamp when this log was collected */

	uint16 btcxovrd_dur;		/* Cumulative btcx overide between WDGs */
	uint16 btcxovrd_err_cnt;	/* BTCX override flagged errors */

	uint16  femtemp_read_fail_counter; /* Fem temparature read fail counter */
	/* Misc general purpose debug counters (will be used for future debugging) */
	uint16 debug_01;
	uint16 debug_02;
} phy_periodic_log_cmn_v2_t;

typedef struct phy_periodic_log_cmn_v3 {
	uint32  nrate; /* Current Tx nrate */
	uint32	duration;	/**< millisecs spent sampling this channel */
	uint32	congest_ibss;	/**< millisecs in our bss (presumably this traffic will */
				/**<  move if cur bss moves channels) */
	uint32	congest_obss;	/**< traffic not in our bss */
	uint32	interference;	/**< millisecs detecting a non 802.11 interferer. */
	uint32  noise_cfg_exit1;
	uint32  noise_cfg_exit2;
	uint32  noise_cfg_exit3;
	uint32  noise_cfg_exit4;
	uint32	ed20_crs0;
	uint32	ed20_crs1;
	uint32	noise_cal_req_ts;
	uint32	noise_cal_crs_ts;
	uint32	log_ts;
	uint32	last_cal_time;
	uint32	phywdg_ts;
	uint32	chanspec_set_ts;
	uint32	noise_zero_inucode;
	uint32	phy_crs_during_noisemmt;
	uint32	wd_dur;

	int32	deaf_count;

	uint16  chanspec; /* Current phy chanspec */
	uint16  vbatmeas; /* Measured VBAT sense value */
	uint16  featureflag; /* Currently active feature flags */
	uint16	nav_cntr_l;
	uint16	nav_cntr_h;
	uint16	chanspec_set_last;
	uint16	ucode_noise_fb_overdue;
	uint16	phy_log_counter;
	uint16	shm_mpif_cnt_val;
	uint16	shm_thld_cnt_val;
	uint16	shm_nav_cnt_val;
	uint16	shm_dc_cnt_val;
	uint16	shm_txff_cnt_val;
	uint16	shm_cts_cnt_val;
	uint16	shm_m_prewds_cnt;

	uint8   cal_phase_id; /* Current Multi phase cal ID */
	uint8   rxchain; /* Rx Chain */
	uint8   txchain; /* Tx Chain */
	uint8   ofdm_desense; /* OFDM desense */
	uint8   bphy_desense; /* BPHY desense */
	uint8   pll_lockstatus; /* PLL Lock status */
	int8    chiptemp; /* Chip temparature */
	int8    femtemp;  /* Fem temparature */

	bool	phycal_disable;
	uint8   pad; /* Padding byte to align with word */
} phy_periodic_log_cmn_v3_t;

typedef struct phy_periodic_log_cmn_v4 {
	uint16  chanspec; /* Current phy chanspec */
	uint16  vbatmeas; /* Measured VBAT sense value */

	uint16  featureflag; /* Currently active feature flags */
	int8    chiptemp; /* Chip temparature */
	int8    femtemp;  /* Fem temparature */

	uint32  nrate; /* Current Tx nrate */

	uint8   cal_phase_id; /* Current Multi phase cal ID */
	uint8   rxchain; /* Rx Chain */
	uint8   txchain; /* Tx Chain */
	uint8   ofdm_desense; /* OFDM desense */

	uint8   slice;
	uint8   dbgfw_ver;	/* version of fw/ucode for debug purposes */
	uint8   bphy_desense; /* BPHY desense */
	uint8   pll_lockstatus; /* PLL Lock status */

	uint32 duration;	/* millisecs spent sampling this channel */
	uint32 congest_ibss;	/* millisecs in our bss (presumably this traffic will */
				/*  move if cur bss moves channels) */
	uint32 congest_obss;	/* traffic not in our bss */
	uint32 interference;	/* millisecs detecting a non 802.11 interferer. */

	/* HP2P related params */
	uint16 shm_mpif_cnt_val;
	uint16 shm_thld_cnt_val;
	uint16 shm_nav_cnt_val;
	uint16 shm_cts_cnt_val;

	uint16 shm_m_prewds_cnt;	/* Count of pre-wds fired in the ucode */
	uint16 deaf_count;		/* Depth of stay_in_carrier_search function */
	uint32 last_cal_time;		/* Last cal execution time */
	uint32 ed20_crs0;		/* ED-CRS status on core 0: TODO change to uint16 */
	uint32 ed20_crs1;		/* ED-CRS status on core 1: TODO change to uint16 */
	uint32 noise_cal_req_ts;	/* Time-stamp when noise cal was requested */
	uint32 noise_cal_intr_ts;	/* Time-stamp when noise cal was completed */
	uint32 phywdg_ts;		/* Time-stamp when wd was fired */
	uint32 phywd_dur;		/* Duration of the watchdog */
	uint32 noise_mmt_abort_crs;	/* Count of CRS during noise mmt: TODO change to uint16 */
	uint32 chanspec_set_ts;		/* Time-stamp when chanspec was set */
	uint32 vcopll_failure_cnt;	/* Number of VCO cal failures
					* (including failures detected in ucode).
					*/
	uint16 dcc_attempt_counter;	/* Number of DC cal attempts */
	uint16 dcc_fail_counter;	/* Number of DC cal failures */
	uint32 log_ts;			/* Time-stamp when this log was collected */

	uint16 btcxovrd_dur;		/* Cumulative btcx overide between WDGs */
	uint16 btcxovrd_err_cnt;	/* BTCX override flagged errors */

	uint16 femtemp_read_fail_counter; /* Fem temparature read fail counter */
	uint16 phy_log_counter;
	uint16 noise_mmt_overdue;	/* Count up if ucode noise mmt is overdue for 5 sec */
	uint16 chan_switch_tm;		/* Channel switch time */

	bool phycal_disable;		/* Set if calibration is disabled */

	/* dccal dcoe & idacc */
	uint8 dcc_err;			/* dccal health check error status */
	uint8 dcoe_num_tries;		/* number of retries on dcoe cal */
	uint8 idacc_num_tries;		/* number of retries on idac cal */

	uint8 dccal_phyrxchain;		/* phy rxchain during dc calibration */
	uint8 dccal_type;		/* DC cal type: single/multi phase, chan change, etc. */
	uint16 dcc_hcfail;		/* dcc health check failure count */
	uint16 dcc_calfail;		/* dcc failure count */

	uint16 noise_mmt_request_cnt;	/* Count of ucode noise cal request from phy */
	uint16 crsmin_pwr_apply_cnt;	/* Count of desense power threshold update to phy */
	/* Misc general purpose debug counters (will be used for future debugging) */
	uint16 debug_03;
	uint16 debug_04;
	uint16 debug_05;
} phy_periodic_log_cmn_v4_t;

typedef struct phy_periodic_log_cmn_v5 {

	uint32 nrate;		/* Current Tx nrate */
	uint32 duration;	/* millisecs spent sampling this channel */
	uint32 congest_ibss;	/* millisecs in our bss (presumably this traffic will */
				/*  move if cur bss moves channels) */
	uint32 congest_obss;	/* traffic not in our bss */
	uint32 interference;	/* millisecs detecting a non 802.11 interferer. */
	uint32 last_cal_time;	/* Last cal execution time */

	uint32 noise_cal_req_ts;	/* Time-stamp when noise cal was requested */
	uint32 noise_cal_intr_ts;	/* Time-stamp when noise cal was completed */
	uint32 phywdg_ts;		/* Time-stamp when wd was fired */
	uint32 phywd_dur;		/* Duration of the watchdog */
	uint32 chanspec_set_ts;		/* Time-stamp when chanspec was set */
	uint32 vcopll_failure_cnt;	/* Number of VCO cal failures including */
					/* failures detected in ucode */
	uint32 log_ts;			/* Time-stamp when this log was collected */

	/* glitch based desense input from cca */
	uint32 cca_stats_total_glitch;
	uint32 cca_stats_bphy_glitch;
	uint32 cca_stats_total_badplcp;
	uint32 cca_stats_bphy_badplcp;
	uint32 cca_stats_mbsstime;

	uint32 counter_noise_request;	/* count of noisecal request */
	uint32 counter_noise_crsbit;	/* count of crs high during noisecal request */
	uint32 counter_noise_apply;	/* count of applying noisecal result to crsmin */
	uint32 fullphycalcntr;		/* count of performing single phase cal */
	uint32 multiphasecalcntr;	/* count of performing multi-phase cal */

	uint16 chanspec;		/* Current phy chanspec */
	uint16 vbatmeas;		/* Measured VBAT sense value */
	uint16 featureflag;		/* Currently active feature flags */

	/* HP2P related params */
	uint16 shm_mpif_cnt_val;
	uint16 shm_thld_cnt_val;
	uint16 shm_nav_cnt_val;
	uint16 shm_cts_cnt_val;

	uint16 shm_m_prewds_cnt;	/* Count of pre-wds fired in the ucode */
	uint16 deaf_count;		/* Depth of stay_in_carrier_search function */

	uint16 ed20_crs0;		/* ED-CRS status on core 0 */
	uint16 ed20_crs1;		/* ED-CRS status on core 1 */

	uint16 dcc_attempt_counter;	/* Number of DC cal attempts */
	uint16 dcc_fail_counter;	/* Number of DC cal failures */

	uint16 btcxovrd_dur;		/* Cumulative btcx overide between WDGs */
	uint16 btcxovrd_err_cnt;	/* BTCX override flagged errors */

	uint16 femtemp_read_fail_counter;	/* Fem temparature read fail counter */
	uint16 phy_log_counter;
	uint16 noise_mmt_overdue;	/* Count up if ucode noise mmt is overdue for 5 sec */
	uint16 chan_switch_tm;		/* Channel switch time */

	uint16 dcc_hcfail;		/* dcc health check failure count */
	uint16 dcc_calfail;		/* dcc failure count */
	uint16 crsmin_pwr_apply_cnt;	/* Count of desense power threshold update to phy */

	uint16 txpustatus;		/* txpu off definations */
	uint16 tempinvalid_count;	/* Count no. of invalid temp. measurements */
	uint16 log_event_id;		/* reuse debug_01, logging event id */

	/* Misc general purpose debug counters (will be used for future debugging) */
	uint16 debug_02;
	uint16 debug_03;
	uint16 debug_04;
	uint16 debug_05;

	int8 chiptemp;			/* Chip temparature */
	int8 femtemp;			/* Fem temparature */

	uint8 cal_phase_id;		/* Current Multi phase cal ID */
	uint8 rxchain;			/* Rx Chain */
	uint8 txchain;			/* Tx Chain */
	uint8 ofdm_desense;		/* OFDM desense */

	uint8 slice;
	uint8 dbgfw_ver;		/* version of fw/ucode for debug purposes */
	uint8 bphy_desense;		/* BPHY desense */
	uint8 pll_lockstatus;		/* PLL Lock status */

	/* dccal dcoe & idacc */
	uint8 dcc_err;			/* dccal health check error status */
	uint8 dcoe_num_tries;		/* number of retries on dcoe cal */
	uint8 idacc_num_tries;		/* number of retries on idac cal */

	uint8 dccal_phyrxchain;		/* phy rxchain during dc calibration */
	uint8 dccal_type;		/* DC cal type: single/multi phase, chan change, etc. */

	uint8 gbd_bphy_sleep_counter;	/* gbd sleep counter */
	uint8 gbd_ofdm_sleep_counter;	/* gbd sleep counter */
	uint8 curr_home_channel;	/* gbd input channel from cca */

	/* desense data */
	int8 btcx_mode;			/* btcoex desense mode */
	int8 ltecx_mode;		/* lte coex desense mode */
	uint8 gbd_ofdm_desense;		/* gbd ofdm desense level */
	uint8 gbd_bphy_desense;		/* gbd bphy desense level */
	uint8 current_elna_bypass;	/* init gain desense: elna bypass */
	uint8 current_tia_idx;		/* init gain desense: tia index */
	uint8 current_lpf_idx;		/* init gain desense: lpf index */
	uint8 crs_auto_thresh;		/* crs auto threshold after desense */

	int8 weakest_rssi;		/* weakest link RSSI */
	uint8 noise_cal_mode;		/* noisecal mode */

	bool phycal_disable;		/* Set if calibration is disabled */
	bool hwpwrctrlen;		/* tx hwpwrctrl enable */
} phy_periodic_log_cmn_v5_t;

typedef struct phy_periodic_log_cmn_v6 {
	uint32	nrate;			/* Current Tx nrate */
	uint32	duration;		/* millisecs spent sampling this channel */
	uint32	congest_ibss;		/* millisecs in our bss (presumably this traffic will */
					/* move if cur bss moves channels) */
	uint32	congest_obss;		/* traffic not in our bss */
	uint32	interference;		/* millisecs detecting a non 802.11 interferer. */

	uint32	macsusp_dur;		/* mac suspend duration */

	uint16	chanspec;		/* Current phy chanspec */
	uint16	vbatmeas;		/* Measured VBAT sense value */
	uint16	featureflag;		/* Currently active feature flags */

	uint16	macsusp_cnt;		/* mac suspend counter */
	uint16	log_event_id;		/* logging event id */

	/* Misc general purpose debug counters (will be used for future debugging) */
	uint16	measurehold_high;	/* PHY hold activities high16 */
	uint16	measurehold_low;	/* PHY hold activities low16 */
	uint16	btcx_ackpwroffset;	/* CoreMask (low8) and ack_pwr_offset (high8) */
	uint16	debug_04;
	uint16	debug_05;

	int8	chiptemp;		/* Chip temparature */
	int8	femtemp;		/* Fem temparature */

	uint8	cal_phase_id;		/* Current Multi phase cal ID */
	uint8	rxchain;		/* Rx Chain */

	uint8	txchain;		/* Tx Chain */
	uint8	ofdm_desense;		/* OFDM desense */

	uint8	bphy_desense;		/* BPHY desense */
	uint8	pll_lockstatus;		/* PLL Lock status */

	uint8	amtbitmap;		/* AMT status bitamp */
	uint8	pad1;			/* Padding byte to align with word */
	uint8	pad2;			/* Padding byte to align with word */
	uint8	pad3;			/* Padding byte to align with word */
} phy_periodic_log_cmn_v6_t;

typedef struct phy_periodic_log_cmn_v7 {

	uint32	nrate;			/* Current Tx nrate */
	uint32	duration;		/* millisecs spent sampling this channel */
	uint32	congest_ibss;		/* millisecs in our bss (presumably this traffic will */
					/*  move if cur bss moves channels) */
	uint32	congest_obss;		/* traffic not in our bss */
	uint32	interference;		/* millisecs detecting a non 802.11 interferer. */
	uint32	last_cal_time;		/* Last cal execution time */

	uint32	noise_cal_req_ts;	/* Time-stamp when noise cal was requested */
	uint32	noise_cal_intr_ts;	/* Time-stamp when noise cal was completed */
	uint32	phywdg_ts;		/* Time-stamp when wd was fired */
	uint32	phywd_dur;		/* Duration of the watchdog */
	uint32	chanspec_set_ts;	/* Time-stamp when chanspec was set */
	uint32	vcopll_failure_cnt;	/* Number of VCO cal failures including */
					/* failures detected in ucode */
	uint32	log_ts;			/* Time-stamp when this log was collected */

	/* glitch based desense input from cca */
	uint32	cca_stats_total_glitch;
	uint32	cca_stats_bphy_glitch;
	uint32	cca_stats_total_badplcp;
	uint32	cca_stats_bphy_badplcp;
	uint32	cca_stats_mbsstime;

	uint32	counter_noise_request;	/* count of noisecal request */
	uint32	counter_noise_crsbit;	/* count of crs high during noisecal request */
	uint32	counter_noise_apply;	/* count of applying noisecal result to crsmin */
	uint32	fullphycalcntr;		/* count of performing single phase cal */
	uint32	multiphasecalcntr;	/* count of performing multi-phase cal */

	uint32	macsusp_dur;		/* mac suspend duration */

	uint16	chanspec;		/* Current phy chanspec */
	uint16	vbatmeas;		/* Measured VBAT sense value */

	uint16	featureflag;		/* Currently active feature flags */

	/* HP2P related params */
	uint16	shm_mpif_cnt_val;
	uint16	shm_thld_cnt_val;
	uint16	shm_nav_cnt_val;
	uint16	shm_cts_cnt_val;
	uint16	shm_m_prewds_cnt;	/* Count of pre-wds fired in the ucode */

	uint16	deaf_count;		/* Depth of stay_in_carrier_search function */

	uint16	ed20_crs0;		/* ED-CRS status on core 0 */
	uint16	ed20_crs1;		/* ED-CRS status on core 1 */

	uint16	dcc_attempt_counter;	/* Number of DC cal attempts */
	uint16	dcc_fail_counter;	/* Number of DC cal failures */

	uint16	btcxovrd_dur;		/* Cumulative btcx overide between WDGs */
	uint16	btcxovrd_err_cnt;	/* BTCX override flagged errors */

	uint16	femtemp_read_fail_counter;	/* Fem temparature read fail counter */
	uint16	phy_log_counter;
	uint16	noise_mmt_overdue;	/* Count up if ucode noise mmt is overdue for 5 sec */
	uint16	chan_switch_tm;		/* Channel switch time */

	uint16	dcc_hcfail;		/* dcc health check failure count */
	uint16	dcc_calfail;		/* dcc failure count */
	uint16	crsmin_pwr_apply_cnt;	/* Count of desense power threshold update to phy */

	uint16	txpustatus;		/* txpu off definations */
	uint16	tempinvalid_count;	/* Count no. of invalid temp. measurements */
	uint16	log_event_id;		/* logging event id */

	/* Misc general purpose debug counters (will be used for future debugging) */
	uint16	debug_01;
	uint16	debug_02;
	uint16	debug_03;
	uint16	debug_04;
	uint16	debug_05;

	uint16	macsusp_cnt;		/* mac suspend counter */
	uint8	amtbitmap;		/* AMT status bitamp */

	int8	chiptemp;		/* Chip temparature */
	int8	femtemp;		/* Fem temparature */

	uint8	cal_phase_id;		/* Current Multi phase cal ID */
	uint8	rxchain;		/* Rx Chain */
	uint8	txchain;		/* Tx Chain */
	uint8	ofdm_desense;		/* OFDM desense */

	uint8	slice;
	uint8	dbgfw_ver;		/* version of fw/ucode for debug purposes */
	uint8	bphy_desense;		/* BPHY desense */
	uint8	pll_lockstatus;		/* PLL Lock status */

	/* dccal dcoe & idacc */
	uint8	dcc_err;		/* dccal health check error status */
	uint8	dcoe_num_tries;		/* number of retries on dcoe cal */
	uint8	idacc_num_tries;	/* number of retries on idac cal */

	uint8	dccal_phyrxchain;	/* phy rxchain during dc calibration */
	uint8	dccal_type;		/* DC cal type: single/multi phase, chan change, etc. */

	uint8	gbd_bphy_sleep_counter;	/* gbd sleep counter */
	uint8	gbd_ofdm_sleep_counter;	/* gbd sleep counter */
	uint8	curr_home_channel;	/* gbd input channel from cca */

	/* desense data */
	int8	btcx_mode;		/* btcoex desense mode */
	int8	ltecx_mode;		/* lte coex desense mode */
	uint8	gbd_ofdm_desense;	/* gbd ofdm desense level */
	uint8	gbd_bphy_desense;	/* gbd bphy desense level */
	uint8	current_elna_bypass;	/* init gain desense: elna bypass */
	uint8	current_tia_idx;	/* init gain desense: tia index */
	uint8	current_lpf_idx;	/* init gain desense: lpf index */
	uint8	crs_auto_thresh;	/* crs auto threshold after desense */

	int8	weakest_rssi;		/* weakest link RSSI */
	uint8	noise_cal_mode;		/* noisecal mode */

	bool	phycal_disable;		/* Set if calibration is disabled */
	bool	hwpwrctrlen;		/* tx hwpwrctrl enable */
	uint8	pad1;			/* Padding byte to align with word */
	uint8	pad2;			/* Padding byte to align with word */
	uint8	pad3;			/* Padding byte to align with word */
} phy_periodic_log_cmn_v7_t;

typedef struct phy_periodic_log_cmn_v8 {

	uint32	nrate;			/* Current Tx nrate */
	uint32	duration;		/* millisecs spent sampling this channel */
	uint32	congest_ibss;		/* millisecs in our bss (presumably this traffic will */
					/*  move if cur bss moves channels) */
	uint32	congest_obss;		/* traffic not in our bss */
	uint32	interference;		/* millisecs detecting a non 802.11 interferer. */
	uint32	last_cal_time;		/* Last cal execution time */

	uint32	noise_cal_req_ts;	/* Time-stamp when noise cal was requested */
	uint32	noise_cal_intr_ts;	/* Time-stamp when noise cal was completed */
	uint32	phywdg_ts;		/* Time-stamp when wd was fired */
	uint32	phywd_dur;		/* Duration of the watchdog */
	uint32	chanspec_set_ts;	/* Time-stamp when chanspec was set */
	uint32	vcopll_failure_cnt;	/* Number of VCO cal failures including */
					/* failures detected in ucode */
	uint32	log_ts;			/* Time-stamp when this log was collected */

	/* glitch based desense input from cca */
	uint32	cca_stats_total_glitch;
	uint32	cca_stats_bphy_glitch;
	uint32	cca_stats_total_badplcp;
	uint32	cca_stats_bphy_badplcp;
	uint32	cca_stats_mbsstime;

	uint32	counter_noise_request;	/* count of noisecal request */
	uint32	counter_noise_crsbit;	/* count of crs high during noisecal request */
	uint32	counter_noise_apply;	/* count of applying noisecal result to crsmin */
	uint32	fullphycalcntr;		/* count of performing single phase cal */
	uint32	multiphasecalcntr;	/* count of performing multi-phase cal */

	uint32	macsusp_dur;		/* mac suspend duration */

	uint16	chanspec;		/* Current phy chanspec */
	uint16	vbatmeas;		/* Measured VBAT sense value */

	uint16	featureflag;		/* Currently active feature flags */

	/* HP2P related params */
	uint16	shm_mpif_cnt_val;
	uint16	shm_thld_cnt_val;
	uint16	shm_nav_cnt_val;
	uint16	shm_cts_cnt_val;
	uint16	shm_m_prewds_cnt;	/* Count of pre-wds fired in the ucode */

	uint16	deaf_count;		/* Depth of stay_in_carrier_search function */

	uint16	ed20_crs0;		/* ED-CRS status on core 0 */
	uint16	ed20_crs1;		/* ED-CRS status on core 1 */

	uint16	dcc_attempt_counter;	/* Number of DC cal attempts */
	uint16	dcc_fail_counter;	/* Number of DC cal failures */

	uint16	btcxovrd_dur;		/* Cumulative btcx overide between WDGs */
	uint16	btcxovrd_err_cnt;	/* BTCX override flagged errors */

	uint16	femtemp_read_fail_counter;	/* Fem temparature read fail counter */
	uint16	phy_log_counter;
	uint16	noise_mmt_overdue;	/* Count up if ucode noise mmt is overdue for 5 sec */
	uint16	chan_switch_tm;		/* Channel switch time */

	uint16	dcc_hcfail;		/* dcc health check failure count */
	uint16	dcc_calfail;		/* dcc failure count */
	uint16	crsmin_pwr_apply_cnt;	/* Count of desense power threshold update to phy */

	uint16	txpustatus;		/* txpu off definations */
	uint16	tempinvalid_count;	/* Count no. of invalid temp. measurements */
	uint16	log_event_id;		/* logging event id */

	/* Misc general purpose debug counters (will be used for future debugging) */
	uint16	measurehold_high;	/* PHY hold activities high16 */
	uint16	measurehold_low;	/* PHY hold activities low16 */
	uint16	tpc_hc_count;		/* RAM A HC counter */
	uint16	tpc_hc_bkoff;		/* RAM A HC core0 power backoff */
	uint16	btcx_ackpwroffset;	/* CoreMask (low8) and ack_pwr_offset (high8) */

	uint16	macsusp_cnt;		/* mac suspend counter */
	uint8	amtbitmap;		/* AMT status bitamp */

	int8	chiptemp;		/* Chip temparature */
	int8	femtemp;		/* Fem temparature */

	uint8	cal_phase_id;		/* Current Multi phase cal ID */
	uint8	rxchain;		/* Rx Chain */
	uint8	txchain;		/* Tx Chain */
	uint8	ofdm_desense;		/* OFDM desense */

	uint8	slice;
	uint8	dbgfw_ver;		/* version of fw/ucode for debug purposes */
	uint8	bphy_desense;		/* BPHY desense */
	uint8	pll_lockstatus;		/* PLL Lock status */

	/* dccal dcoe & idacc */
	uint8	dcc_err;		/* dccal health check error status */
	uint8	dcoe_num_tries;		/* number of retries on dcoe cal */
	uint8	idacc_num_tries;	/* number of retries on idac cal */

	uint8	dccal_phyrxchain;	/* phy rxchain during dc calibration */
	uint8	dccal_type;		/* DC cal type: single/multi phase, chan change, etc. */

	uint8	gbd_bphy_sleep_counter;	/* gbd sleep counter */
	uint8	gbd_ofdm_sleep_counter;	/* gbd sleep counter */
	uint8	curr_home_channel;	/* gbd input channel from cca */

	/* desense data */
	int8	btcx_mode;		/* btcoex desense mode */
	int8	ltecx_mode;		/* lte coex desense mode */
	uint8	gbd_ofdm_desense;	/* gbd ofdm desense level */
	uint8	gbd_bphy_desense;	/* gbd bphy desense level */
	uint8	current_elna_bypass;	/* init gain desense: elna bypass */
	uint8	current_tia_idx;	/* init gain desense: tia index */
	uint8	current_lpf_idx;	/* init gain desense: lpf index */
	uint8	crs_auto_thresh;	/* crs auto threshold after desense */

	int8	weakest_rssi;		/* weakest link RSSI */
	uint8	noise_cal_mode;		/* noisecal mode */

	bool	phycal_disable;		/* Set if calibration is disabled */
	bool	hwpwrctrlen;		/* tx hwpwrctrl enable */
	int8	ed_threshold;		/* Threshold applied for ED */
	uint16	ed_crs_status;		/* Status of ED and CRS during noise cal */
	uint16	preempt_status1;	/* status of preemption */
	uint16	preempt_status2;	/* status of preemption */
	uint16	preempt_status3;	/* status of preemption */
	uint16	preempt_status4;	/* status of preemption */
	uint32	ed_duration;		/* ccastats: ed_duration */
} phy_periodic_log_cmn_v8_t;

/* Inherited from v8 */
typedef struct phy_periodic_log_cmn_v9 {

	uint32	nrate;			/* Current Tx nrate */
	uint32	duration;		/* millisecs spent sampling this channel */
	uint32	congest_ibss;		/* millisecs in our bss (presumably this traffic will */
					/*  move if cur bss moves channels) */
	uint32	congest_obss;		/* traffic not in our bss */
	uint32	interference;		/* millisecs detecting a non 802.11 interferer. */
	uint32	last_cal_time;		/* Last cal execution time */

	uint32	noise_cal_req_ts;	/* Time-stamp when noise cal was requested */
	uint32	noise_cal_intr_ts;	/* Time-stamp when noise cal was completed */
	uint32	phywdg_ts;		/* Time-stamp when wd was fired */
	uint32	phywd_dur;		/* Duration of the watchdog */
	uint32	chanspec_set_ts;	/* Time-stamp when chanspec was set */
	uint32	vcopll_failure_cnt;	/* Number of VCO cal failures including */
					/* failures detected in ucode */
	uint32	log_ts;			/* Time-stamp when this log was collected */

	/* glitch based desense input from cca */
	uint32	cca_stats_total_glitch;
	uint32	cca_stats_bphy_glitch;
	uint32	cca_stats_total_badplcp;
	uint32	cca_stats_bphy_badplcp;
	uint32	cca_stats_mbsstime;

	uint32	counter_noise_request;	/* count of noisecal request */
	uint32	counter_noise_crsbit;	/* count of crs high during noisecal request */
	uint32	counter_noise_apply;	/* count of applying noisecal result to crsmin */
	uint32	fullphycalcntr;		/* count of performing single phase cal */
	uint32	multiphasecalcntr;	/* count of performing multi-phase cal */

	uint32	macsusp_dur;		/* mac suspend duration */

	uint32	featureflag;		/* Currently active feature flags */

	uint16	chanspec;		/* Current phy chanspec */
	uint16	vbatmeas;		/* Measured VBAT sense value */

	/* HP2P related params */
	uint16	shm_mpif_cnt_val;
	uint16	shm_thld_cnt_val;
	uint16	shm_nav_cnt_val;
	uint16	shm_cts_cnt_val;
	uint16	shm_m_prewds_cnt;	/* Count of pre-wds fired in the ucode */

	uint16	deaf_count;		/* Depth of stay_in_carrier_search function */

	uint16	ed20_crs0;		/* ED-CRS status on core 0 */
	uint16	ed20_crs1;		/* ED-CRS status on core 1 */

	uint16	dcc_attempt_counter;	/* Number of DC cal attempts */
	uint16	dcc_fail_counter;	/* Number of DC cal failures */

	uint16	btcxovrd_dur;		/* Cumulative btcx overide between WDGs */
	uint16	btcxovrd_err_cnt;	/* BTCX override flagged errors */

	uint16	femtemp_read_fail_counter;	/* Fem temparature read fail counter */
	uint16	phy_log_counter;
	uint16	noise_mmt_overdue;	/* Count up if ucode noise mmt is overdue for 5 sec */
	uint16	chan_switch_tm;		/* Channel switch time */

	uint16	dcc_hcfail;		/* dcc health check failure count */
	uint16	dcc_calfail;		/* dcc failure count */
	uint16	crsmin_pwr_apply_cnt;	/* Count of desense power threshold update to phy */

	uint16	txpustatus;		/* txpu off definations */
	uint16	tempinvalid_count;	/* Count no. of invalid temp. measurements */
	uint16	log_event_id;		/* logging event id */

	/* Misc general purpose debug counters (will be used for future debugging) */
	int8	ed_threshold0;		/* Threshold applied for ED core 0 */
	int8	ed_threshold1;		/* Threshold applied for ED core 1 */
	uint16	debug_01;
	uint16	debug_02;
	uint16	debug_03;
	uint16	debug_04;

	uint16	macsusp_cnt;		/* mac suspend counter */
	uint8	amtbitmap;		/* AMT status bitamp */

	int8	chiptemp;		/* Chip temparature */
	int8	femtemp;		/* Fem temparature */

	uint8	cal_phase_id;		/* Current Multi phase cal ID */
	uint8	rxchain;		/* Rx Chain */
	uint8	txchain;		/* Tx Chain */
	uint8	ofdm_desense;		/* OFDM desense */

	uint8	slice;
	uint8	dbgfw_ver;		/* version of fw/ucode for debug purposes */
	uint8	bphy_desense;		/* BPHY desense */
	uint8	pll_lockstatus;		/* PLL Lock status */

	/* dccal dcoe & idacc */
	uint8	dcc_err;		/* dccal health check error status */
	uint8	dcoe_num_tries;		/* number of retries on dcoe cal */
	uint8	idacc_num_tries;	/* number of retries on idac cal */

	uint8	dccal_phyrxchain;	/* phy rxchain during dc calibration */
	uint8	dccal_type;		/* DC cal type: single/multi phase, chan change, etc. */

	uint8	gbd_bphy_sleep_counter;	/* gbd sleep counter */
	uint8	gbd_ofdm_sleep_counter;	/* gbd sleep counter */
	uint8	curr_home_channel;	/* gbd input channel from cca */

	/* desense data */
	int8	btcx_mode;		/* btcoex desense mode */
	int8	ltecx_mode;		/* lte coex desense mode */
	uint8	gbd_ofdm_desense;	/* gbd ofdm desense level */
	uint8	gbd_bphy_desense;	/* gbd bphy desense level */
	uint8	current_elna_bypass;	/* init gain desense: elna bypass */
	uint8	current_tia_idx;	/* init gain desense: tia index */
	uint8	current_lpf_idx;	/* init gain desense: lpf index */
	uint8	crs_auto_thresh;	/* crs auto threshold after desense */

	int8	weakest_rssi;		/* weakest link RSSI */
	uint8	noise_cal_mode;		/* noisecal mode */

	bool	phycal_disable;		/* Set if calibration is disabled */
	bool	hwpwrctrlen;		/* tx hwpwrctrl enable */
	int8	ed_threshold;		/* Threshold applied for ED */
	uint16	ed_crs_status;		/* Status of ED and CRS during noise cal */
	uint16	preempt_status1;	/* status of preemption */
	uint16	preempt_status2;	/* status of preemption */
	uint16	preempt_status3;	/* status of preemption */
	uint16	preempt_status4;	/* status of preemption */
	uint8	debug_05;
	uint8	cal_suppressed_cntr_ed;	/* counter including ss, mp cals, MSB is current state */
	uint32	ed_duration;		/* ccastats: ed_duration */
} phy_periodic_log_cmn_v9_t;

/* Inherited from v9 */
typedef struct phy_periodic_log_cmn_v10 {

	uint32	nrate;			/* Current Tx nrate */
	uint32	duration;		/* millisecs spent sampling this channel */
	uint32	congest_ibss;		/* millisecs in our bss (presumably this traffic will */
					/*  move if cur bss moves channels) */
	uint32	congest_obss;		/* traffic not in our bss */
	uint32	interference;		/* millisecs detecting a non 802.11 interferer. */
	uint32	last_cal_time;		/* Last cal execution time */

	uint32	noise_cal_req_ts;	/* Time-stamp when noise cal was requested */
	uint32	noise_cal_intr_ts;	/* Time-stamp when noise cal was completed */
	uint32	phywdg_ts;		/* Time-stamp when wd was fired */
	uint32	phywd_dur;		/* Duration of the watchdog */
	uint32	chanspec_set_ts;	/* Time-stamp when chanspec was set */
	uint32	vcopll_failure_cnt;	/* Number of VCO cal failures including */
					/* failures detected in ucode */
	uint32	log_ts;			/* Time-stamp when this log was collected */

	/* glitch based desense input from cca */
	uint32	cca_stats_total_glitch;
	uint32	cca_stats_bphy_glitch;
	uint32	cca_stats_total_badplcp;
	uint32	cca_stats_bphy_badplcp;
	uint32	cca_stats_mbsstime;

	uint32	counter_noise_request;	/* count of noisecal request */
	uint32	counter_noise_crsbit;	/* count of crs high during noisecal request */
	uint32	counter_noise_apply;	/* count of applying noisecal result to crsmin */
	uint32	fullphycalcntr;		/* count of performing single phase cal */
	uint32	multiphasecalcntr;	/* count of performing multi-phase cal */

	uint32	macsusp_dur;		/* mac suspend duration */

	uint32	featureflag;		/* Currently active feature flags */

	uint16	chanspec;		/* Current phy chanspec */
	uint16	vbatmeas;		/* Measured VBAT sense value */

	/* HP2P related params */
	uint16	shm_mpif_cnt_val;
	uint16	shm_thld_cnt_val;
	uint16	shm_nav_cnt_val;
	uint16	shm_cts_cnt_val;
	uint16	shm_m_prewds_cnt;	/* Count of pre-wds fired in the ucode */

	uint16	deaf_count;		/* Depth of stay_in_carrier_search function */

	uint16	ed20_crs0;		/* ED-CRS status on core 0 */
	uint16	ed20_crs1;		/* ED-CRS status on core 1 */

	uint16	dcc_attempt_counter;	/* Number of DC cal attempts */
	uint16	dcc_fail_counter;	/* Number of DC cal failures */

	uint16	btcxovrd_dur;		/* Cumulative btcx overide between WDGs */
	uint16	btcxovrd_err_cnt;	/* BTCX override flagged errors */

	uint16	femtemp_read_fail_counter;	/* Fem temparature read fail counter */
	uint16	phy_log_counter;
	uint16	noise_mmt_overdue;	/* Count up if ucode noise mmt is overdue for 5 sec */
	uint16	chan_switch_tm;		/* Channel switch time */

	uint16	dcc_hcfail;		/* dcc health check failure count */
	uint16	dcc_calfail;		/* dcc failure count */
	uint16	crsmin_pwr_apply_cnt;	/* Count of desense power threshold update to phy */

	uint16	txpustatus;		/* txpu off definations */
	uint16	tempinvalid_count;	/* Count no. of invalid temp. measurements */
	uint16	log_event_id;		/* logging event id */

	/* Misc general purpose debug counters (will be used for future debugging) */
	uint16	pktprocdebug;
	uint16	pktprocdebug2;
	uint16	timeoutstatus;
	uint16	debug_04;
	uint16	debug_05;

	uint16	macsusp_cnt;		/* mac suspend counter */
	uint8	amtbitmap;		/* AMT status bitamp */

	int8	chiptemp;		/* Chip temparature */
	int8	femtemp;		/* Fem temparature */

	uint8	cal_phase_id;		/* Current Multi phase cal ID */
	uint8	rxchain;		/* Rx Chain */
	uint8	txchain;		/* Tx Chain */
	uint8	ofdm_desense;		/* OFDM desense */

	uint8	slice;
	uint8	dbgfw_ver;		/* version of fw/ucode for debug purposes */
	uint8	bphy_desense;		/* BPHY desense */
	uint8	pll_lockstatus;		/* PLL Lock status */

	/* dccal dcoe & idacc */
	uint8	dcc_err;		/* dccal health check error status */
	uint8	dcoe_num_tries;		/* number of retries on dcoe cal */
	uint8	idacc_num_tries;	/* number of retries on idac cal */

	uint8	dccal_phyrxchain;	/* phy rxchain during dc calibration */
	uint8	dccal_type;		/* DC cal type: single/multi phase, chan change, etc. */

	uint8	gbd_bphy_sleep_counter;	/* gbd sleep counter */
	uint8	gbd_ofdm_sleep_counter;	/* gbd sleep counter */
	uint8	curr_home_channel;	/* gbd input channel from cca */

	/* desense data */
	int8	btcx_mode;		/* btcoex desense mode */
	int8	ltecx_mode;		/* lte coex desense mode */
	uint8	gbd_ofdm_desense;	/* gbd ofdm desense level */
	uint8	gbd_bphy_desense;	/* gbd bphy desense level */
	uint8	current_elna_bypass;	/* init gain desense: elna bypass */
	uint8	current_tia_idx;	/* init gain desense: tia index */
	uint8	current_lpf_idx;	/* init gain desense: lpf index */
	uint8	crs_auto_thresh;	/* crs auto threshold after desense */

	int8	weakest_rssi;		/* weakest link RSSI */
	uint8	noise_cal_mode;		/* noisecal mode */

	bool	phycal_disable;		/* Set if calibration is disabled */
	bool	hwpwrctrlen;		/* tx hwpwrctrl enable */
	int8	ed_threshold;		/* Threshold applied for ED */
	uint32	measurehold;		/* PHY hold activities */
	uint32	ed_duration;		/* ccastats: ed_duration */
	uint16	ed_crs_status;		/* Status of ED and CRS during noise cal */
	uint16	preempt_status1;	/* status of preemption */
	uint16	preempt_status2;	/* status of preemption */
	uint16	preempt_status3;	/* status of preemption */
	uint16	preempt_status4;	/* status of preemption */

	uint16	counter_noise_iqest_to;	/* count of IQ_Est time out */
	uint16	rfemstate2g;		/* rFEM state register for 2g */
	uint16	rfemstate5g;		/* rFEM state register for 5g */
	uint16	txiqcal_max_retry_cnt;	/* txiqlocal retries reached max allowed count */
	uint16	txiqcal_max_slope_cnt;	/* txiqlocal slope reached max allowed count */
	uint16	mppc_cal_failed_cnt;	/* MPPC cal failure count */

	uint16	gci_lst_inv_ctr;
	uint16	gci_lst_rst_ctr;
	uint16	gci_lst_sem_fail;
	uint16	gci_lst_rb_state;
	uint16	gci_lst_pad01;
	uint16	gci_lst_pad02;
	uint16	gci_lst_pad03;
	uint16	gci_lst_pad04;
	uint16	gci_lst_pad05;
	uint16	gci_lst_state_mask;
	uint16	gci_inv_tx;		/* Tx inv cnt */
	uint16	gci_inv_rx;		/* Rx inv cnt */
	uint16	gci_rst_tx;		/* gci Tx reset cnt */
	uint16	gci_rst_rx;		/* gci Rx reset cnt */
	uint16	gci_sem_fail;
	uint16	gci_invstate;		/* inv state */
	uint16	gci_phyctl0;		/* TX_PHYCTL0 */
	uint16	gci_phyctl1;		/* TX_PHYCTL1 */
	uint16	gci_phyctl2;		/* TX_PHYCTL2 */
	uint16	gci_ctxtst;		/* S_CTXTST */
	uint16	gci_invts;		/* When gci read request was placed and completed */
	uint16	gci_invtsdn;		/* */
	uint16	gci_invtxs;		/* timestamp for TXCRS assertion */
	uint16	gci_invtxdur;		/* IFS_TX_DUR */
	uint16	gci_invifs;		/* IFS status */
	uint16	gci_chan;		/* For additional ucode data */
	uint16	gci_pad02;		/* For additional ucode data */
	uint16	gci_pad03;		/* For additional ucode data */
	uint16	gci_pad04;		/* For additional ucode data */

	uint32	rxsense_disable_req_ch;	/* channel disable requests */
	uint32	ocl_disable_reqs;	/* OCL disable bitmap */

	int8	rxsense_noise_idx;	/* rxsense detection threshold desense index */
	int8	rxsense_offset;		/* rxsense min power desense index */
	uint8	ocl_en_status;		/* OCL requested state and OCL HW state */
	uint8	lpc_status;		/* Flag to enable/disable LPC, and runtime flag status */

	uint16	rspfrm_ed_txncl_cnt;	/* Response frame not sent due to ED */

	int16	last_cal_temp;
	uint8	cal_reason;		/* reason for the cal */
	uint8	cal_suppressed_cntr_ed;	/* counter including ss, mp cals, MSB is current state */
	uint8	phylog_noise_mode;	/* Noise mode used */
	uint8	debug_08;

	/* Misc general purpose debug counters (will be used for future debugging) */
	uint32	interference_mode;	/* interference mitigation mode */
	uint32	power_mode;		/* LP/VLP logging */
	uint32	debug_11;
	uint32	debug_12;
} phy_periodic_log_cmn_v10_t;

typedef struct phy_periodic_log_cmn_v11 {
	uint32	nrate;			/* Current Tx nrate */
	uint32	duration;		/* millisecs spent sampling this channel */
	uint32	congest_ibss;		/* millisecs in our bss (presumably this traffic will */
					/*  move if cur bss moves channels) */
	uint32	congest_obss;		/* traffic not in our bss */
	uint32	interference;		/* millisecs detecting a non 802.11 interferer. */
	uint32	last_cal_time;		/* Last cal execution time */

	uint32	noise_cal_req_ts;	/* Time-stamp when noise cal was requested */
	uint32	noise_cal_intr_ts;	/* Time-stamp when noise cal was completed */
	uint32	phywdg_ts;		/* Time-stamp when wd was fired */
	uint32	phywd_dur;		/* Duration of the watchdog */
	uint32	chanspec_set_ts;	/* Time-stamp when chanspec was set */
	uint32	vcopll_failure_cnt;	/* Number of VCO cal failures including */
					/* failures detected in ucode */
	uint32	log_ts;			/* Time-stamp when this log was collected */

	/* glitch based desense input from cca */
	uint32	cca_stats_total_glitch;
	uint32	cca_stats_bphy_glitch;
	uint32	cca_stats_total_badplcp;
	uint32	cca_stats_bphy_badplcp;
	uint32	cca_stats_mbsstime;

	uint32	counter_noise_request;	/* count of noisecal request */
	uint32	counter_noise_crsbit;	/* count of crs high during noisecal request */
	uint32	counter_noise_apply;	/* count of applying noisecal result to crsmin */
	uint32	fullphycalcntr;		/* count of performing single phase cal */
	uint32	multiphasecalcntr;	/* count of performing multi-phase cal */

	uint32	macsusp_dur;		/* mac suspend duration */

	uint32	featureflag;		/* Currently active feature flags */

	uint16	chanspec;		/* Current phy chanspec */
	uint16	vbatmeas;		/* Measured VBAT sense value */

	/* HP2P related params */
	uint16	shm_mpif_cnt_val;
	uint16	shm_thld_cnt_val;
	uint16	shm_nav_cnt_val;
	uint16	shm_cts_cnt_val;
	uint16	shm_m_prewds_cnt;	/* Count of pre-wds fired in the ucode */

	uint16	deaf_count;		/* Depth of stay_in_carrier_search function */

	uint16	ed20_crs0;		/* ED-CRS status on core 0 */
	uint16	ed20_crs1;		/* ED-CRS status on core 1 */

	uint16	dcc_attempt_counter;	/* Number of DC cal attempts */
	uint16	dcc_fail_counter;	/* Number of DC cal failures */

	uint16	btcxovrd_dur;		/* Cumulative btcx overide between WDGs */
	uint16	btcxovrd_err_cnt;	/* BTCX override flagged errors */

	uint16	femtemp_read_fail_counter;	/* Fem temparature read fail counter */
	uint16	phy_log_counter;
	uint16	noise_mmt_overdue;	/* Count up if ucode noise mmt is overdue for 5 sec */
	uint16	chan_switch_tm;		/* Channel switch time */

	uint16	dcc_hcfail;		/* dcc health check failure count */
	uint16	dcc_calfail;		/* dcc failure count */
	uint16	crsmin_pwr_apply_cnt;	/* Count of desense power threshold update to phy */

	uint16	txpustatus;		/* txpu off definations */
	uint16	tempinvalid_count;	/* Count no. of invalid temp. measurements */
	uint16	log_event_id;		/* logging event id */

	/* Misc general purpose debug counters (will be used for future debugging) */
	uint16	debug_01;
	uint16	debug_02;
	uint16	debug_03;
	uint16	debug_04;
	uint16	debug_05;

	uint16	macsusp_cnt;		/* mac suspend counter */
	uint8	amtbitmap;		/* AMT status bitamp */

	int8	chiptemp;		/* Chip temparature */
	int8	femtemp;		/* Fem temparature */

	uint8	cal_phase_id;		/* Current Multi phase cal ID */
	uint8	rxchain;		/* Rx Chain */
	uint8	txchain;		/* Tx Chain */
	uint8	ofdm_desense;		/* OFDM desense */

	uint8	slice;
	uint8	bphy_desense;		/* BPHY desense */
	uint8	pll_lockstatus;		/* PLL Lock status */

	/* dccal dcoe & idacc */
	uint8	dcc_err;		/* dccal health check error status */
	uint8	dcoe_num_tries;		/* number of retries on dcoe cal */
	uint8	idacc_num_tries;	/* number of retries on idac cal */

	uint8	dccal_phyrxchain;	/* phy rxchain during dc calibration */
	uint8	dccal_type;		/* DC cal type: single/multi phase, chan change, etc. */

	uint8	gbd_bphy_sleep_counter;	/* gbd sleep counter */
	uint8	gbd_ofdm_sleep_counter;	/* gbd sleep counter */

	/* desense data */
	int8	btcx_mode;		/* btcoex desense mode */
	int8	ltecx_mode;		/* lte coex desense mode */
	uint8	gbd_ofdm_desense;	/* gbd ofdm desense level */
	uint8	gbd_bphy_desense;	/* gbd bphy desense level */
	uint8	current_elna_bypass;	/* init gain desense: elna bypass */
	uint8	current_tia_idx;	/* init gain desense: tia index */
	uint8	current_lpf_idx;	/* init gain desense: lpf index */

	int8	weakest_rssi;		/* weakest link RSSI */

	bool	phycal_disable;		/* Set if calibration is disabled */
	bool	hwpwrctrlen;		/* tx hwpwrctrl enable */
	int8	debug_15;		/* multipurpose debug counter */
	uint32	ed_duration;		/* ccastats: ed_duration */
	uint16	ed_crs_status;		/* Status of ED and CRS during noise cal */
	uint16	preempt_status1;	/* status of preemption */
	uint16	preempt_status2;	/* status of preemption */
	uint16	preempt_status3;	/* status of preemption */
	uint16	preempt_status4;	/* status of preemption */

	uint16	timeoutstatus;
	uint32	measurehold;		/* PHY hold activities */
	uint16	pktprocdebug;
	uint16	pktprocdebug2;

	uint16	counter_noise_iqest_to;	/* count of IQ_Est time out */
	uint16	rfemstate2g;		/* rFEM state register for 2g */
	uint16	rfemstate5g;		/* rFEM state register for 5g */
	uint16	txiqcal_max_retry_cnt;	/* txiqlocal retries reached max allowed count */
	uint16	txiqcal_max_slope_cnt;	/* txiqlocal slope reached max allowed count */
	uint16	mppc_cal_failed_cnt;	/* MPPC cal failure count */

	uint16	gci_lst_inv_ctr;
	uint16	gci_lst_rst_ctr;
	uint16	gci_lst_sem_fail;
	uint16	gci_lst_rb_state;
	uint16	gci_lst_pad01;
	uint16	gci_lst_pad02;
	uint16	gci_lst_pad03;
	uint16	gci_lst_pad04;
	uint16	gci_lst_pad05;
	uint16	gci_lst_state_mask;
	uint16	gci_inv_tx;		/* Tx inv cnt */
	uint16	gci_inv_rx;		/* Rx inv cnt */
	uint16	gci_rst_tx;		/* gci Tx reset cnt */
	uint16	gci_rst_rx;		/* gci Rx reset cnt */
	uint16	gci_sem_fail;
	uint16	gci_invstate;		/* inv state */
	uint16	gci_phyctl0;		/* TX_PHYCTL0 */
	uint16	gci_phyctl2;		/* TX_PHYCTL2 */
	uint16	gci_ctxtst;		/* S_CTXTST */
	uint16	gci_chan;		/* GCI channel */
	uint16	gci_cm;
	uint16	gci_inv_intr;
	uint16	gci_rst_intr;
	uint16	gci_rst_prdc_rx;
	uint16	gci_rst_wk_rx;
	uint16	gci_rst_rmac_rx;
	uint16	gci_rst_tx_rx;
	uint16	gci_pad01;		/* For additional ucode data */
	uint16	gci_pad02;		/* For additional ucode data */
	uint16	gci_pad03;		/* For additional ucode data */
	uint16	gci_pad04;		/* For additional ucode data */

	uint32	rxsense_disable_req_ch;	/* channel disable requests */
	uint32	ocl_disable_reqs;	/* OCL disable bitmap */

	int8	rxsense_noise_idx;	/* rxsense detection threshold desense index */
	int8	rxsense_offset;		/* rxsense min power desense index */
	uint8	ocl_en_status;		/* OCL requested state and OCL HW state */
	uint8	lpc_status;		/* Flag to enable/disable LPC, and runtime flag status */

	uint16	rspfrm_ed_txncl_cnt;	/* Response frame not sent due to ED */

	int16	last_cal_temp;
	uint8	cal_reason;		/* reason for the cal */
	uint8	cal_suppressed_cntr_ed;	/* counter including ss, mp cals, MSB is current state */
	uint8	phylog_noise_mode;	/* Noise mode used */
	uint8	noise_cal_mode;		/* noisecal mode */

	/* Misc general purpose debug counters (will be used for future debugging) */
	int8	ed_threshold0;		/* Threshold applied for ED core 0 */
	int8	ed_threshold1;		/* Threshold applied for ED core 1 */
	uint8	debug_08;
	uint8	debug_09;

	uint32	interference_mode;	/* interference mitigation mode */
	uint32	power_mode;		/* LP/VLP logging */

	/* Misc general purpose debug counters (will be used for future debugging) */
	uint32	debug_10;
	uint32	debug_11;
	uint32	debug_12;
	uint32	debug_13;
	uint32	debug_14;
} phy_periodic_log_cmn_v11_t;

typedef struct phy_periodic_log_cmn_v12 {
	uint32	nrate;			/* Current Tx nrate */
	uint32	duration;		/* millisecs spent sampling this channel */
	uint32	congest_ibss;		/* millisecs in our bss (presumably this traffic will */
					/*  move if cur bss moves channels) */
	uint32	congest_obss;		/* traffic not in our bss */
	uint32	interference;		/* millisecs detecting a non 802.11 interferer. */
	uint32	last_cal_time;		/* Last cal execution time */

	uint32	noise_cal_req_ts;	/* Time-stamp when noise cal was requested */
	uint32	noise_cal_intr_ts;	/* Time-stamp when noise cal was completed */
	uint32	phywdg_ts;		/* Time-stamp when wd was fired */
	uint32	phywd_dur;		/* Duration of the watchdog */
	uint32	chanspec_set_ts;	/* Time-stamp when chanspec was set */
	uint32	vcopll_failure_cnt;	/* Number of VCO cal failures including */
					/* failures detected in ucode */
	uint32	log_ts;			/* Time-stamp when this log was collected */

	/* glitch based desense input from cca */
	uint32	cca_stats_total_glitch;
	uint32	cca_stats_bphy_glitch;
	uint32	cca_stats_total_badplcp;
	uint32	cca_stats_bphy_badplcp;
	uint32	cca_stats_mbsstime;

	uint32	counter_noise_request;	/* count of noisecal request */
	uint32	counter_noise_crsbit;	/* count of crs high during noisecal request */
	uint32	counter_noise_apply;	/* count of applying noisecal result to crsmin */
	uint32	fullphycalcntr;		/* count of performing single phase cal */
	uint32	multiphasecalcntr;	/* count of performing multi-phase cal */

	uint32	macsusp_dur;		/* mac suspend duration */

	uint32	featureflag;		/* Currently active feature flags */

	uint16	chanspec;		/* Current phy chanspec */
	uint16	vbatmeas;		/* Measured VBAT sense value */

	/* HP2P related params */
	uint16	shm_mpif_cnt_val;
	uint16	shm_thld_cnt_val;
	uint16	shm_nav_cnt_val;
	uint16	shm_cts_cnt_val;
	uint16	shm_m_prewds_cnt;	/* Count of pre-wds fired in the ucode */

	uint16	deaf_count;		/* Depth of stay_in_carrier_search function */

	uint16	dcc_attempt_counter;	/* Number of DC cal attempts */
	uint16	dcc_fail_counter;	/* Number of DC cal failures */

	uint16	btcxovrd_dur;		/* Cumulative btcx overide between WDGs */
	uint16	btcxovrd_err_cnt;	/* BTCX override flagged errors */

	uint16	femtemp_read_fail_counter;	/* Fem temparature read fail counter */
	uint16	phy_log_counter;
	uint16	noise_mmt_overdue;	/* Count up if ucode noise mmt is overdue for 5 sec */
	uint16	chan_switch_tm;		/* Channel switch time */

	uint16	dcc_hcfail;		/* dcc health check failure count */
	uint16	dcc_calfail;		/* dcc failure count */
	uint16	crsmin_pwr_apply_cnt;	/* Count of desense power threshold update to phy */

	uint16	txpustatus;		/* txpu off definations */
	uint16	tempinvalid_count;	/* Count no. of invalid temp. measurements */
	uint16	log_event_id;		/* logging event id */

	uint16	counter_noise_interrupt_cleared;	/* interrupt cleared on channel change */
	uint16	counter_noise_cal_cancelled;		/* trigger cancelled on channel change */

	/* Misc general purpose debug counters (will be used for future debugging) */
	uint16	debug_03;
	uint16	debug_04;
	uint16	debug_05;

	uint16	macsusp_cnt;		/* mac suspend counter */
	uint8	amtbitmap;		/* AMT status bitamp */

	int8	chiptemp;		/* Chip temparature */
	int8	femtemp;		/* Fem temparature */

	uint8	cal_phase_id;		/* Current Multi phase cal ID */
	uint8	rxchain;		/* Rx Chain */
	uint8	txchain;		/* Tx Chain */
	uint8	ofdm_desense;		/* OFDM desense */

	uint8	slice;
	uint8	bphy_desense;		/* BPHY desense */
	uint8	pll_lockstatus;		/* PLL Lock status */

	/* dccal dcoe & idacc */
	uint8	dcc_err;		/* dccal health check error status */
	uint8	dcoe_num_tries;		/* number of retries on dcoe cal */
	uint8	idacc_num_tries;	/* number of retries on idac cal */

	uint8	dccal_phyrxchain;	/* phy rxchain during dc calibration */
	uint8	dccal_type;		/* DC cal type: single/multi phase, chan change, etc. */

	uint8	gbd_bphy_sleep_counter;	/* gbd sleep counter */
	uint8	gbd_ofdm_sleep_counter;	/* gbd sleep counter */

	/* desense data */
	int8	btcx_mode;		/* btcoex desense mode */
	int8	ltecx_mode;		/* lte coex desense mode */
	uint8	gbd_ofdm_desense;	/* gbd ofdm desense level */
	uint8	gbd_bphy_desense;	/* gbd bphy desense level */
	uint8	current_elna_bypass;	/* init gain desense: elna bypass */
	uint8	current_tia_idx;	/* init gain desense: tia index */
	uint8	current_lpf_idx;	/* init gain desense: lpf index */

	int8	weakest_rssi;		/* weakest link RSSI */

	bool	phycal_disable;		/* Set if calibration is disabled */
	bool	hwpwrctrlen;		/* tx hwpwrctrl enable */
	uint8	ocl_en_status;		/* OCL requested state and OCL HW state */
	uint32	ed_duration;		/* ccastats: ed_duration */
	uint16	ed_crs_status;		/* Status of ED and CRS during noise cal */
	uint16	preempt_status1;	/* status of preemption */
	uint16	preempt_status2;	/* status of preemption */
	uint16	preempt_status3;	/* status of preemption */
	uint16	preempt_status4;	/* status of preemption */

	uint16	timeoutstatus;
	uint32	measurehold;		/* PHY hold activities */
	uint16	pktprocdebug;
	uint16	pktprocdebug2;

	uint16	counter_noise_iqest_to;	/* count of IQ_Est time out */
	uint16	rfemstate2g;		/* rFEM state register for 2g */
	uint16	rfemstate5g;		/* rFEM state register for 5g */
	uint16	txiqcal_max_retry_cnt;	/* txiqlocal retries reached max allowed count */
	uint16	txiqcal_max_slope_cnt;	/* txiqlocal slope reached max allowed count */
	uint16	mppc_cal_failed_cnt;	/* MPPC cal failure count */

	uint16	gci_lst_inv_ctr;
	uint16	gci_lst_rst_ctr;
	uint16	gci_lst_sem_fail;
	uint16	gci_lst_rb_state;
	uint16	gci_lst_state_mask;
	uint16	gci_inv_tx;		/* Tx inv cnt */
	uint16	gci_inv_rx;		/* Rx inv cnt */
	uint16	gci_rst_tx;		/* gci Tx reset cnt */
	uint16	gci_rst_rx;		/* gci Rx reset cnt */
	uint16	gci_sem_fail;
	uint16	gci_invstate;		/* inv state */
	uint16	gci_phyctl0;		/* TX_PHYCTL0 */
	uint16	gci_phyctl2;		/* TX_PHYCTL2 */
	uint16	gci_ctxtst;		/* S_CTXTST */
	uint16	gci_chan;		/* GCI channel */
	uint16	gci_cm;
	uint16	gci_inv_intr;
	uint16	gci_rst_intr;
	uint16	gci_rst_prdc_rx;
	uint16	gci_rst_wk_rx;
	uint16	gci_rst_rmac_rx;
	uint16	gci_rst_tx_rx;

	uint8	rfem_rxmode_curr_hwstate;
	uint8   rfem_rxmode_bands_req;		/* mode as requested by SW layer */
	uint8   rfem_rxmode_bands_applied;	/* mode currently configured in HW */

	uint8	lpc_status;		/* Flag to enable/disable LPC, and runtime flag status */

	uint32	rxsense_disable_req_ch;	/* channel disable requests */
	uint32	ocl_disable_reqs;	/* OCL disable bitmap */

	uint16	rspfrm_ed_txncl_cnt;	/* Response frame not sent due to ED */

	int16	last_cal_temp;
	uint8	cal_reason;		/* reason for the cal */
	uint8	cal_suppressed_cntr_ed;	/* counter including ss, mp cals, MSB is current state */
	uint8	phylog_noise_mode;	/* Noise mode used */
	int8	rxsense_noise_idx;	/* rxsense detection threshold desense index */
	int8	rxsense_offset;		/* rxsense min power desense index */
	uint8	lpf_lut_type;		/* high or low noise radio lpf lut */
	uint8	noisecal_saved_radio_lpf_lut_type;	/* high or low noise radio lpf lut */

	uint8	noise_cal_mode;		/* noisecal mode */
	uint16	noise_cal_timeout;	/* noisecal timeout */

	uint16	txpwr_recalc_reasons;	/* Reasons bitmap for triggered Tx pwr recalc */

	uint32	interference_mode;	/* interference mitigation mode */
	uint32	power_mode;		/* LP/VLP logging */

	uint16	channel_type;		/* Channel type bitmap */

	/* Misc general purpose debug counters (will be used for future debugging) */
	uint16	debug_06;
	uint8	debug_07;
	uint8	debug_08;
	uint8	debug_09;
	uint8	debug_10;
	uint32	debug_11;
	uint32	debug_12;
} phy_periodic_log_cmn_v12_t;

typedef struct phy_periodic_log_cmn_v13 {
	uint32	nrate;			/* Current Tx nrate */
	uint32	duration;		/* millisecs spent sampling this channel */
	uint32	congest_ibss;		/* millisecs in our bss (presumably this traffic will */
					/*  move if cur bss moves channels) */
	uint32	congest_obss;		/* traffic not in our bss */
	uint32	interference;		/* millisecs detecting a non 802.11 interferer. */
	uint32	last_cal_time;		/* Last cal execution time */

	uint32	noise_cal_req_ts;	/* Time-stamp when noise cal was requested */
	uint32	noise_cal_intr_ts;	/* Time-stamp when noise cal was completed */
	uint32	phywdg_ts;		/* Time-stamp when wd was fired */
	uint32	phywd_dur;		/* Duration of the watchdog */
	uint32	chanspec_set_ts;	/* Time-stamp when chanspec was set */
	uint32	vcopll_failure_cnt;	/* Number of VCO cal failures including */
					/* failures detected in ucode */
	uint32	log_ts;			/* Time-stamp when this log was collected */

	/* glitch based desense input from cca */
	uint32	cca_stats_total_glitch;
	uint32	cca_stats_bphy_glitch;
	uint32	cca_stats_total_badplcp;
	uint32	cca_stats_bphy_badplcp;
	uint32	cca_stats_mbsstime;

	uint32	counter_noise_request;	/* count of noisecal request */
	uint32	counter_noise_crsbit;	/* count of crs high during noisecal request */
	uint32	counter_noise_apply;	/* count of applying noisecal result to crsmin */
	uint32	fullphycalcntr;		/* count of performing single phase cal */
	uint32	multiphasecalcntr;	/* count of performing multi-phase cal */

	uint32	macsusp_dur;		/* mac suspend duration */

	uint32	featureflag;		/* Currently active feature flags */

	uint16	chanspec;		/* Current phy chanspec */
	uint16	vbatmeas;		/* Measured VBAT sense value */

	/* HP2P related params */
	uint16	shm_mpif_cnt_val;
	uint16	shm_thld_cnt_val;
	uint16	shm_nav_cnt_val;
	uint16	shm_cts_cnt_val;
	uint16	shm_m_prewds_cnt;	/* Count of pre-wds fired in the ucode */

	uint16	deaf_count;		/* Depth of stay_in_carrier_search function */

	uint16	dcc_attempt_counter;	/* Number of DC cal attempts */
	uint16	dcc_fail_counter;	/* Number of DC cal failures */

	uint16	btcxovrd_dur;		/* Cumulative btcx overide between WDGs */
	uint16	btcxovrd_err_cnt;	/* BTCX override flagged errors */

	uint16	femtemp_read_fail_counter;	/* Fem temparature read fail counter */
	uint16	phy_log_counter;
	uint16	noise_mmt_overdue;	/* Count up if ucode noise mmt is overdue for 5 sec */
	uint16	chan_switch_dur;	/* Channel switch time */

	uint16	dcc_hcfail;		/* dcc health check failure count */
	uint16	dcc_calfail;		/* dcc failure count */
	uint16	crsmin_pwr_apply_cnt;	/* Count of desense power threshold update to phy */

	uint16	txpustatus;		/* txpu off definations */
	uint16	tempinvalid_count;	/* Count no. of invalid temp. measurements */
	uint16	log_event_id;		/* logging event id */

	uint16	counter_noise_interrupt_cleared;	/* interrupt cleared on channel change */
	uint16	counter_noise_cal_cancelled;		/* trigger cancelled on channel change */

	uint16	macsusp_cnt;		/* mac suspend counter */
	uint8	amtbitmap;		/* AMT status bitamp */

	int8	chiptemp;		/* Chip temparature */
	int8	femtemp;		/* Fem temparature */

	uint8	cal_phase_id;		/* Current Multi phase cal ID */
	uint8	rxchain;		/* Rx Chain */
	uint8	txchain;		/* Tx Chain */
	uint8	ofdm_desense;		/* OFDM desense */

	uint8	slice;
	uint8	bphy_desense;		/* BPHY desense */
	uint8	pll_lockstatus;		/* PLL Lock status */

	/* dccal dcoe & idacc */
	uint8	dcc_err;		/* dccal health check error status */
	uint8	dcoe_num_tries;		/* number of retries on dcoe cal */
	uint8	idacc_num_tries;	/* number of retries on idac cal */

	uint8	dccal_phyrxchain;	/* phy rxchain during dc calibration */
	uint8	dccal_type;		/* DC cal type: single/multi phase, chan change, etc. */

	uint8	gbd_bphy_sleep_counter;	/* gbd sleep counter */
	uint8	gbd_ofdm_sleep_counter;	/* gbd sleep counter */

	/* desense data */
	int8	btcx_mode;		/* btcoex desense mode */
	int8	ltecx_mode;		/* lte coex desense mode */
	uint8	gbd_ofdm_desense;	/* gbd ofdm desense level */
	uint8	gbd_bphy_desense;	/* gbd bphy desense level */
	uint8	current_elna_bypass;	/* init gain desense: elna bypass */
	uint8	current_tia_idx;	/* init gain desense: tia index */
	uint8	current_lpf_idx;	/* init gain desense: lpf index */

	int8	weakest_rssi;		/* weakest link RSSI */

	bool	phycal_disable;		/* Set if calibration is disabled */
	bool	hwpwrctrlen;		/* tx hwpwrctrl enable */
	uint8	ocl_en_status;		/* OCL requested state and OCL HW state */
	uint16	timeoutstatus;
	uint32	measurehold;		/* PHY hold activities */
	uint32	ed_duration;		/* ccastats: ed_duration */
	uint16	ed_crs_status;		/* Status of ED and CRS during noise cal */
	uint16	preempt_status1;	/* status of preemption */
	uint16	preempt_status2;	/* status of preemption */
	uint16	preempt_status3;	/* status of preemption */
	uint16	preempt_status4;	/* status of preemption */

	uint16	pktprocdebug;
	uint16	pktprocdebug2;

	uint16	counter_noise_iqest_to;	/* count of IQ_Est time out */
	uint16	rfemstate2g;		/* rFEM state register for 2g */
	uint16	rfemstate5g;		/* rFEM state register for 5g */
	uint16	txiqcal_max_retry_cnt;	/* txiqlocal retries reached max allowed count */
	uint16	txiqcal_max_slope_cnt;	/* txiqlocal slope reached max allowed count */
	uint16	mppc_cal_failed_cnt;	/* MPPC cal failure count */

	uint16	gci_lst_inv_ctr;
	uint16	gci_lst_rst_ctr;
	uint16	gci_lst_sem_fail;
	uint16	gci_lst_rb_state;
	uint16	gci_lst_state_mask;
	uint16	gci_inv_tx;		/* Tx inv cnt */
	uint16	gci_inv_rx;		/* Rx inv cnt */
	uint16	gci_rst_tx;		/* gci Tx reset cnt */
	uint16	gci_rst_rx;		/* gci Rx reset cnt */
	uint16	gci_sem_fail;
	uint16	gci_invstate;		/* inv state */
	uint16	gci_phyctl0;		/* TX_PHYCTL0 */
	uint16	gci_phyctl2;		/* TX_PHYCTL2 */
	uint16	gci_ctxtst;		/* S_CTXTST */
	uint16	gci_chan;		/* GCI channel */
	uint16	gci_cm;
	uint16	gci_inv_intr;
	uint16	gci_rst_intr;
	uint16	gci_rst_prdc_rx;
	uint16	gci_rst_wk_rx;
	uint16	gci_rst_rmac_rx;
	uint16	gci_rst_tx_rx;

	uint16	rspfrm_ed_txncl_cnt;	/* Response frame not sent due to ED */

	uint8	rfem_rxmode_curr_hwstate;
	uint8   rfem_rxmode_bands_req;		/* mode as requested by SW layer */
	uint8   rfem_rxmode_bands_applied;	/* mode currently configured in HW */

	uint8	lpc_status;		/* Flag to enable/disable LPC, and runtime flag status */

	uint32	rxsense_disable_req_ch;	/* channel disable requests */
	uint32	ocl_disable_reqs;	/* OCL disable bitmap */

	int16	last_cal_temp;
	uint8	cal_reason;		/* reason for the cal */
	uint8	cal_suppressed_cntr_ed;	/* counter including ss, mp cals, MSB is current state */
	uint8	phylog_noise_mode;	/* Noise mode used */
	int8	rxsense_noise_idx;	/* rxsense detection threshold desense index */
	int8	rxsense_offset;		/* rxsense min power desense index */
	uint8	lpf_lut_type;		/* high or low noise radio lpf lut */
	uint8	noisecal_saved_radio_lpf_lut_type;	/* high or low noise radio lpf lut */

	uint8	noise_cal_mode;		/* noisecal mode */
	uint16	noise_cal_timeout;	/* noisecal timeout */

	uint16	txpwr_recalc_reasons;	/* Reasons bitmap for triggered Tx pwr recalc */

	uint16	channel_active;		/* Channel active status */

	uint32	interference_mode;	/* interference mitigation mode */
	uint32	power_mode;		/* LP/VLP logging */

	uint32	temp_sense_cnt;
	uint16	ncap_misc;

	uint16	nap_disable_reqs;	/* NAP disable bitmap */
	uint8	nap_en_status;		/* NAP enable status */

	/* Misc general purpose debug counters (will be used for future debugging) */
	uint8	debug_01;
	uint16	debug_02;
	uint32	debug_03;
} phy_periodic_log_cmn_v13_t;

typedef struct phy_periodic_log_cmn_v14 {
	uint32	txratespec;		/* Current Tx rate spec */
	uint32	rxratespec;		/* Current rx rate spec */
	uint32	duration;		/* millisecs spent sampling this channel */
	uint32	congest_ibss;		/* millisecs in our bss (presumably this traffic will */
					/*  move if cur bss moves channels) */
	uint32	congest_obss;		/* traffic not in our bss */
	uint32	interference;		/* millisecs detecting a non 802.11 interferer. */
	uint32	last_cal_time;		/* Last cal execution time */

	uint32	noise_cal_req_ts;	/* Time-stamp when noise cal was requested */
	uint32	noise_cal_intr_ts;	/* Time-stamp when noise cal was completed */
	uint32	phywdg_ts;		/* Time-stamp when wd was fired */
	uint32	phywd_dur;		/* Duration of the watchdog */
	uint32	chanspec_set_ts;	/* Time-stamp when chanspec was set */
	uint32	vcopll_failure_cnt;	/* Number of VCO cal failures including */
					/* failures detected in ucode */
	uint32	log_ts;			/* Time-stamp when this log was collected */

	/* glitch based desense input from cca */
	uint32	cca_stats_total_glitch;
	uint32	cca_stats_bphy_glitch;
	uint32	cca_stats_total_badplcp;
	uint32	cca_stats_bphy_badplcp;
	uint32	cca_stats_mbsstime;

	uint32	counter_noise_request;	/* count of noisecal request */
	uint32	counter_noise_crsbit;	/* count of crs high during noisecal request */
	uint32	counter_noise_apply;	/* count of applying noisecal result to crsmin */
	uint32	fullphycalcntr;		/* count of performing single phase cal */
	uint32	multiphasecalcntr;	/* count of performing multi-phase cal */

	uint32	macsusp_dur;		/* mac suspend duration */

	uint32	featureflag;		/* Currently active feature flags */

	uint16	chanspec;		/* Current phy chanspec */
	uint16	vbatmeas;		/* Measured VBAT sense value */

	/* HP2P related params */
	uint16	shm_mpif_cnt_val;
	uint16	shm_thld_cnt_val;
	uint16	shm_nav_cnt_val;
	uint16	shm_cts_cnt_val;
	uint16	shm_m_prewds_cnt;	/* Count of pre-wds fired in the ucode */

	uint16	timeoutstatus;
	uint16	deaf_count;		/* Depth of stay_in_carrier_search function */

	uint16	dcc_attempt_counter;	/* Number of DC cal attempts */
	uint16	dcc_fail_counter;	/* Number of DC cal failures */

	uint16	btcxovrd_dur;		/* Cumulative btcx overide between WDGs */
	uint16	btcxovrd_err_cnt;	/* BTCX override flagged errors */

	uint16	femtemp_read_fail_counter;	/* Fem temparature read fail counter */
	uint16	phy_log_counter;
	uint16	noise_mmt_overdue;	/* Count up if ucode noise mmt is overdue for 5 sec */
	uint16	chan_switch_dur;	/* Channel switch time */

	uint16	dcc_hcfail;		/* dcc health check failure count */
	uint16	dcc_calfail;		/* dcc failure count */
	uint16	crsmin_pwr_apply_cnt;	/* Count of desense power threshold update to phy */

	uint16	txpustatus;		/* txpu off definations */
	uint16	tempinvalid_count;	/* Count no. of invalid temp. measurements */
	uint16	log_event_id;		/* logging event id */

	uint16	counter_noise_interrupt_cleared;	/* interrupt cleared on channel change */
	uint16	counter_noise_cal_cancelled;		/* trigger cancelled on channel change */

	uint16	macsusp_cnt;		/* mac suspend counter */
	uint8	amtbitmap;		/* AMT status bitamp */

	int8	chiptemp;		/* Chip temparature */
	int8	femtemp;		/* Fem temparature */

	uint8	cal_phase_id;		/* Current Multi phase cal ID */
	uint8	rxchain;		/* Rx Chain */
	uint8	txchain;		/* Tx Chain */
	uint8	ofdm_desense;		/* OFDM desense */

	uint8	slice;
	uint8	bphy_desense;		/* BPHY desense */
	uint8	pll_lockstatus;		/* PLL Lock status */

	/* dccal dcoe & idacc */
	uint8	dcc_err;		/* dccal health check error status */
	uint8	dcoe_num_tries;		/* number of retries on dcoe cal */
	uint8	idacc_num_tries;	/* number of retries on idac cal */

	uint8	dccal_phyrxchain;	/* phy rxchain during dc calibration */
	uint8	dccal_type;		/* DC cal type: single/multi phase, chan change, etc. */

	uint8	gbd_bphy_sleep_counter;	/* gbd sleep counter */
	uint8	gbd_ofdm_sleep_counter;	/* gbd sleep counter */

	/* desense data */
	int8	btcx_mode;		/* btcoex desense mode */
	int8	ltecx_mode;		/* lte coex desense mode */
	uint8	gbd_ofdm_desense;	/* gbd ofdm desense level */
	uint8	gbd_bphy_desense;	/* gbd bphy desense level */
	uint8	current_elna_bypass;	/* init gain desense: elna bypass */
	uint8	current_tia_idx;	/* init gain desense: tia index */
	uint8	current_lpf_idx;	/* init gain desense: lpf index */

	int8	weakest_rssi;		/* weakest link RSSI */

	bool	phycal_disable;		/* Set if calibration is disabled */
	bool	hwpwrctrlen;		/* tx hwpwrctrl enable */
	uint8	ocl_en_status;		/* OCL requested state and OCL HW state */
	uint32	measurehold;		/* PHY hold activities */
	uint32	ed_duration;		/* ccastats: ed_duration */
	uint16	ed_crs_status;		/* Status of ED and CRS during noise cal */

	uint16	pktprocdebug;
	uint16	pktprocdebug2;

	uint16	counter_noise_iqest_to;	/* count of IQ_Est time out */
	uint16	rfemstate2g;		/* rFEM state register for 2g */
	uint16	rfemstate5g;		/* rFEM state register for 5g */
	uint16	txiqcal_max_retry_cnt;	/* txiqlocal retries reached max allowed count */
	uint16	txiqcal_max_slope_cnt;	/* txiqlocal slope reached max allowed count */
	uint16	mppc_cal_failed_cnt;	/* MPPC cal failure count */

	uint16	gci_lst_inv_ctr;
	uint16	gci_lst_rst_ctr;
	uint16	gci_lst_sem_fail;
	uint16	gci_lst_rb_state;
	uint16	gci_lst_state_mask;
	uint16	gci_inv_tx;		/* Tx inv cnt */
	uint16	gci_inv_rx;		/* Rx inv cnt */
	uint16	gci_rst_tx;		/* gci Tx reset cnt */
	uint16	gci_rst_rx;		/* gci Rx reset cnt */
	uint16	gci_sem_fail;
	uint16	gci_invstate;		/* inv state */
	uint16	gci_phyctl0;		/* TX_PHYCTL0 */
	uint16	gci_phyctl2;		/* TX_PHYCTL2 */
	uint16	gci_ctxtst;		/* S_CTXTST */
	uint16	gci_chan;		/* GCI channel */
	uint16	gci_cm;
	uint16	gci_inv_intr;
	uint16	gci_rst_intr;
	uint16	gci_rst_prdc_rx;
	uint16	gci_rst_wk_rx;
	uint16	gci_rst_rmac_rx;
	uint16	gci_rst_tx_rx;

	uint16	rspfrm_ed_txncl_cnt;	/* Response frame not sent due to ED */

	uint8	rfem_rxmode_curr_hwstate;
	uint8   rfem_rxmode_bands_req;		/* mode as requested by SW layer */
	uint8   rfem_rxmode_bands_applied;	/* mode currently configured in HW */

	uint8	lpc_status;		/* Flag to enable/disable LPC, and runtime flag status */

	uint32	rxsense_disable_req_ch;	/* channel disable requests */
	uint32	ocl_disable_reqs;	/* OCL disable bitmap */

	int16	last_cal_temp;
	uint8	cal_reason;		/* reason for the cal */
	uint8	cal_suppressed_cntr_ed;	/* counter including ss, mp cals, MSB is current state */
	uint8	phylog_noise_mode;	/* Noise mode used */
	int8	rxsense_noise_idx;	/* rxsense detection threshold desense index */
	int8	rxsense_offset;		/* rxsense min power desense index */
	uint8	lpf_lut_type;		/* high or low noise radio lpf lut */
	uint8	noisecal_saved_radio_lpf_lut_type;	/* high or low noise radio lpf lut */

	uint8	noise_cal_mode;		/* noisecal mode */
	uint16	noise_cal_timeout;	/* noisecal timeout */

	uint16	txpwr_recalc_reasons;	/* Reasons bitmap for triggered Tx pwr recalc */

	uint16	channel_active;		/* Channel active status */

	uint32	interference_mode;	/* interference mitigation mode */
	uint32	power_mode;		/* LP/VLP logging */

	uint32	temp_sense_cnt;
	uint32	pm_dur;
	uint16	ncap_misc;

	uint16	nap_disable_reqs;	/* NAP disable bitmap */
	uint8	nap_en_status;		/* NAP enable status */

	uint8	tvpm_mitigation;	/* Bitmap of enabled and which mitigation active */

	uint16	phyctl_w0_pa0;		/* PHYCTL Word-0 with PA 0 */
	uint16	phyctl_w2_pa0;		/* PHYCTL Word-2 with PA 0 */
	uint16	phyctl_w5_pa0;		/* PHYCTL Word-5/6 with PA 0
						* Bit[7:0] Core0 PPR, Bit[15:8] Core1 PPR
						*/
	uint16  phyctl_w0_pa1;		/* PHYCTL Word-0 with PA 1 */
	uint16  phyctl_w2_pa1;		/* PHYCTL Word-2 with PA 1 */
	uint16  phyctl_w5_pa1;		/* PHYCTL Word-5/6 with PA 1
						* Bit[7:0] Core0 PPR, Bit[15:8] Core1 PPR
						*/
	uint16	phyctl_w0_pa2;		/* PHYCTL Word-0 with PA 2 */
	uint16	phyctl_w2_pa2;		/* PHYCTL Word-2 with PA 2 */
	uint16	phyctl_w5_pa2;		/* PHYCTL Word-5/6 with PA 2
						* Bit[7:0] Core0 PPR, Bit[15:8] Core1 PPR
						*/
	uint16	preempt_status2;	/* status of preemption */
	uint8	macsusp_phy_cnt;

	uint8	dsa_mode;
	uint8	dsa_status;
	uint8	dsa_offset;

	uint8	dsa_util[3];

	/* Misc general purpose debug counters (will be used for future debugging) */
	uint8	debug_01;	/* pamode_dbg */
	uint16	debug_02;
	uint16	debug_03;
	uint32	debug_04;	/* Used by M_NAVSTAT_ACC */
} phy_periodic_log_cmn_v14_t;

typedef struct phy_periodic_log_cmn_v15 {
	uint32	txratespec;		/* Current Tx rate spec */
	uint32	rxratespec;		/* Current rx rate spec */
	uint32	duration;		/* millisecs spent sampling this channel */
	uint32	congest_ibss;		/* millisecs in our bss (presumably this traffic will */
					/*  move if cur bss moves channels) */
	uint32	congest_obss;		/* traffic not in our bss */
	uint32	interference;		/* millisecs detecting a non 802.11 interferer. */
	uint32	last_cal_time;		/* Last cal execution time */

	uint32	noise_cal_req_ts;	/* Time-stamp when noise cal was requested */
	uint32	noise_cal_intr_ts;	/* Time-stamp when noise cal was completed */
	uint32	phywdg_ts;		/* Time-stamp when wd was fired */
	uint32	phywd_dur;		/* Duration of the watchdog */
	uint32	chanspec_set_ts;	/* Time-stamp when chanspec was set */
	uint32	vcopll_failure_cnt;	/* Number of VCO cal failures including */
					/* failures detected in ucode */
	uint32	log_ts;			/* Time-stamp when this log was collected */

	/* glitch based desense input from cca */
	uint32	cca_stats_total_glitch;
	uint32	cca_stats_bphy_glitch;
	uint32	cca_stats_total_badplcp;
	uint32	cca_stats_bphy_badplcp;
	uint32	cca_stats_mbsstime;

	uint32	counter_noise_request;	/* count of noisecal request */
	uint32	counter_noise_crsbit;	/* count of crs high during noisecal request */
	uint32	counter_noise_apply;	/* count of applying noisecal result to crsmin */
	uint32	fullphycalcntr;		/* count of performing single phase cal */
	uint32	multiphasecalcntr;	/* count of performing multi-phase cal */

	uint32	macsusp_dur;		/* mac suspend duration */

	uint32	featureflag;		/* Currently active feature flags */

	uint16	chanspec;		/* Current phy chanspec */
	uint16	vbatmeas;		/* Measured VBAT sense value */

	/* HP2P related params */
	uint16	shm_mpif_cnt_val;
	uint16	shm_thld_cnt_val;
	uint16	shm_cts_cnt_val;
	uint16	shm_m_prewds_cnt;	/* Count of pre-wds fired in the ucode */

	uint16	navstat_max;

	uint16	timeoutstatus;
	uint16	deaf_count;		/* Depth of stay_in_carrier_search function */

	uint16	dcc_attempt_counter;	/* Number of DC cal attempts */
	uint16	dcc_fail_counter;	/* Number of DC cal failures */

	uint16	btcxovrd_dur;		/* Cumulative btcx overide between WDGs */
	uint16	btcxovrd_err_cnt;	/* BTCX override flagged errors */

	uint16	femtemp_read_fail_counter;	/* Fem temparature read fail counter */
	uint16	phy_log_counter;
	uint16	noise_mmt_overdue;	/* Count up if ucode noise mmt is overdue for 5 sec */
	uint16	chan_switch_dur;	/* Channel switch time */

	uint16	dcc_hcfail;		/* dcc health check failure count */
	uint16	dcc_calfail;		/* dcc failure count */
	uint16	crsmin_pwr_apply_cnt;	/* Count of desense power threshold update to phy */

	uint16	txpustatus;		/* txpu off definations */
	uint16	tempinvalid_count;	/* Count no. of invalid temp. measurements */
	uint16	log_event_id;		/* logging event id */

	uint16	counter_noise_interrupt_cleared;	/* interrupt cleared on channel change */
	uint16	counter_noise_cal_cancelled;		/* trigger cancelled on channel change */

	uint16	macsusp_cnt;		/* mac suspend counter */
	uint8	amtbitmap;		/* AMT status bitamp */

	int8	chiptemp;		/* Chip temparature */
	int8	femtemp;		/* Fem temparature */

	uint8	cal_phase_id;		/* Current Multi phase cal ID */
	uint8	rxchain;		/* Rx Chain */
	uint8	txchain;		/* Tx Chain */
	uint8	ofdm_desense;		/* OFDM desense */

	uint8	slice;
	uint8	bphy_desense;		/* BPHY desense */
	uint8	pll_lockstatus;		/* PLL Lock status */

	/* dccal dcoe & idacc */
	uint8	dcc_err;		/* dccal health check error status */
	uint8	dcoe_num_tries;		/* number of retries on dcoe cal */
	uint8	idacc_num_tries;	/* number of retries on idac cal */

	uint8	dccal_phyrxchain;	/* phy rxchain during dc calibration */
	uint8	dccal_type;		/* DC cal type: single/multi phase, chan change, etc. */

	uint8	gbd_bphy_sleep_counter;	/* gbd sleep counter */
	uint8	gbd_ofdm_sleep_counter;	/* gbd sleep counter */

	/* desense data */
	int8	btcx_mode;		/* btcoex desense mode */
	int8	ltecx_mode;		/* lte coex desense mode */
	uint8	gbd_ofdm_desense;	/* gbd ofdm desense level */
	uint8	gbd_bphy_desense;	/* gbd bphy desense level */
	uint8	current_elna_bypass;	/* init gain desense: elna bypass */
	uint8	current_tia_idx;	/* init gain desense: tia index */
	uint8	current_lpf_idx;	/* init gain desense: lpf index */

	int8	weakest_rssi;		/* weakest link RSSI */

	bool	phycal_disable;		/* Set if calibration is disabled */
	bool	hwpwrctrlen;		/* tx hwpwrctrl enable */
	uint8	ocl_en_status;		/* OCL requested state and OCL HW state */
	uint32	measurehold;		/* PHY hold activities */
	uint32	ed_duration;		/* ccastats: ed_duration */
	uint16	ed_crs_status;		/* Status of ED and CRS during noise cal */

	uint16	pktprocdebug;
	uint16	pktprocdebug2;

	uint16	counter_noise_iqest_to;	/* count of IQ_Est time out */
	uint16	rfemstate2g;		/* rFEM state register for 2g */
	uint16	rfemstate5g;		/* rFEM state register for 5g */
	uint16	txiqcal_max_retry_cnt;	/* txiqlocal retries reached max allowed count */
	uint16	txiqcal_max_slope_cnt;	/* txiqlocal slope reached max allowed count */
	uint16	mppc_cal_failed_cnt;	/* MPPC cal failure count */

	uint16	gci_lst_inv_ctr;
	uint16	gci_lst_rst_ctr;
	uint16	gci_lst_sem_fail;
	uint16	gci_lst_rb_state;
	uint16	gci_lst_state_mask;
	uint16	gci_inv_tx;		/* Tx inv cnt */
	uint16	gci_inv_rx;		/* Rx inv cnt */
	uint16	gci_rst_tx;		/* gci Tx reset cnt */
	uint16	gci_rst_rx;		/* gci Rx reset cnt */
	uint16	gci_sem_fail;
	uint16	gci_invstate;		/* inv state */
	uint16	gci_phyctl0;		/* TX_PHYCTL0 */
	uint16	gci_phyctl2;		/* TX_PHYCTL2 */
	uint16	gci_ctxtst;		/* S_CTXTST */
	uint16	gci_chan;		/* GCI channel */
	uint16	gci_cm;
	uint16	gci_inv_intr;
	uint16	gci_rst_intr;
	uint16	gci_rst_prdc_rx;
	uint16	gci_rst_wk_rx;
	uint16	gci_rst_rmac_rx;
	uint16	gci_rst_tx_rx;

	uint16	rspfrm_ed_txncl_cnt;	/* Response frame not sent due to ED */

	uint8	rfem_rxmode_curr_hwstate;
	uint8   rfem_rxmode_bands_req;		/* mode as requested by SW layer */
	uint8   rfem_rxmode_bands_applied;	/* mode currently configured in HW */

	uint8	lpc_status;		/* Flag to enable/disable LPC, and runtime flag status */

	uint32	rxsense_disable_req_ch;	/* channel disable requests */
	uint32	ocl_disable_reqs;	/* OCL disable bitmap */

	int16	last_cal_temp;
	uint8	cal_reason;		/* reason for the cal */
	uint8	cal_suppressed_cntr_ed;	/* counter including ss, mp cals, MSB is current state */
	int8	rxsense_noise_idx;	/* rxsense detection threshold desense index */
	int8	rxsense_offset;		/* rxsense min power desense index */

	uint8	rccal_lpf_tmout;
	uint8	rccal_tia_tmout;
	uint8	rccal_rxpll_tmout;

	uint8	noise_cal_mode;		/* noisecal mode */
	uint16	noise_cal_timeout;	/* noisecal timeout */

	uint16	txpwr_recalc_reasons;	/* Reasons bitmap for triggered Tx pwr recalc */

	uint16	channel_active;		/* Channel active status */

	uint32	interference_mode;	/* interference mitigation mode */
	uint32	power_mode;		/* LP/VLP logging */

	uint32	temp_sense_cnt;
	uint32	pm_dur;
	uint16	ncap_misc;

	uint16	nap_disable_reqs;	/* NAP disable bitmap */
	uint8	nap_en_status;		/* NAP enable status */
	uint8	phylog_noise_mode;	/* Noise mode used */

	uint16	phyctl_w0_pa0;		/* PHYCTL Word-0 with PA 0 */
	uint16	phyctl_w2_pa0;		/* PHYCTL Word-2 with PA 0 */
	uint16	phyctl_w5_pa0;		/* PHYCTL Word-5/6 with PA 0
						* Bit[7:0] Core0 PPR, Bit[15:8] Core1 PPR
						*/
	uint16	phyctl_w0_pa1;		/* PHYCTL Word-0 with PA 1 */
	uint16	phyctl_w2_pa1;		/* PHYCTL Word-2 with PA 1 */
	uint16	phyctl_w5_pa1;		/* PHYCTL Word-5/6 with PA 1
						* Bit[7:0] Core0 PPR, Bit[15:8] Core1 PPR
						*/
	uint16	phyctl_w0_pa2;		/* PHYCTL Word-0 with PA 2 */
	uint16	phyctl_w2_pa2;		/* PHYCTL Word-2 with PA 2 */
	uint16	phyctl_w5_pa2;		/* PHYCTL Word-5/6 with PA 2
						* Bit[7:0] Core0 PPR, Bit[15:8] Core1 PPR
						*/
	uint32	navstat_acc;

	uint16	preempt_status2;	/* status of preemption */
	uint8	macsusp_phy_cnt;

	uint8	tvpm_mitigation;	/* Bitmap of enabled and which mitigation active */
	uint8	dsa_mode;
	uint8	dsa_status;
	uint8	dsa_offset;
	uint8	dsa_util[6u];		/* Size subject to change */

	uint8	pa_mode;

	/* Misc general purpose debug counters (will be used for future debugging) */
	uint16	debug_01;
	uint32	debug_02;
	uint32	debug_03;
} phy_periodic_log_cmn_v15_t;

// From v14 for v30
typedef struct phy_periodic_log_cmn_v16 {
	uint32	txratespec;		/* Current Tx rate spec */
	uint32	rxratespec;		/* Current rx rate spec */
	uint32	duration;		/* millisecs spent sampling this channel */
	uint32	congest_ibss;		/* millisecs in our bss (presumably this traffic will */
					/*  move if cur bss moves channels) */
	uint32	congest_obss;		/* traffic not in our bss */
	uint32	interference;		/* millisecs detecting a non 802.11 interferer. */
	uint32	last_cal_time;		/* Last cal execution time */

	uint32	noise_cal_req_ts;	/* Time-stamp when noise cal was requested */
	uint32	noise_cal_intr_ts;	/* Time-stamp when noise cal was completed */
	uint32	phywdg_ts;		/* Time-stamp when wd was fired */
	uint32	phywd_dur;		/* Duration of the watchdog */
	uint32	chanspec_set_ts;	/* Time-stamp when chanspec was set */
	uint32	vcopll_failure_cnt;	/* Number of VCO cal failures including */
					/* failures detected in ucode */
	uint32	log_ts;			/* Time-stamp when this log was collected */

	/* glitch based desense input from cca */
	uint32	cca_stats_total_glitch;
	uint32	cca_stats_bphy_glitch;
	uint32	cca_stats_total_badplcp;
	uint32	cca_stats_bphy_badplcp;
	uint32	cca_stats_mbsstime;

	uint32	counter_noise_request;	/* count of noisecal request */
	uint32	counter_noise_crsbit;	/* count of crs high during noisecal request */
	uint32	counter_noise_apply;	/* count of applying noisecal result to crsmin */
	uint32	fullphycalcntr;		/* count of performing single phase cal */
	uint32	multiphasecalcntr;	/* count of performing multi-phase cal */

	uint32	macsusp_dur;		/* mac suspend duration */

	uint32	featureflag;		/* Currently active feature flags */

	uint16	chanspec;		/* Current phy chanspec */
	uint16	vbatmeas;		/* Measured VBAT sense value */

	/* HP2P related params */
	uint16	shm_mpif_cnt_val;
	uint16	shm_thld_cnt_val;
	uint16	shm_cts_cnt_val;
	uint16	shm_m_prewds_cnt;	/* Count of pre-wds fired in the ucode */

	uint32	navstat_acc;
	uint16	navstat_max;

	uint16	timeoutstatus;
	uint16	deaf_count;		/* Depth of stay_in_carrier_search function */

	uint16	dcc_attempt_counter;	/* Number of DC cal attempts */
	uint16	dcc_fail_counter;	/* Number of DC cal failures */

	uint16	btcxovrd_dur;		/* Cumulative btcx overide between WDGs */
	uint16	btcxovrd_err_cnt;	/* BTCX override flagged errors */

	uint16	femtemp_read_fail_counter;	/* Fem temparature read fail counter */
	uint16	phy_log_counter;
	uint16	noise_mmt_overdue;	/* Count up if ucode noise mmt is overdue for 5 sec */
	uint16	chan_switch_dur;	/* Channel switch time */

	uint16	dcc_hcfail;		/* dcc health check failure count */
	uint16	dcc_calfail;		/* dcc failure count */
	uint16	crsmin_pwr_apply_cnt;	/* Count of desense power threshold update to phy */

	uint16	txpustatus;		/* txpu off definations */
	uint16	tempinvalid_count;	/* Count no. of invalid temp. measurements */
	uint16	log_event_id;		/* logging event id */

	uint16	counter_noise_interrupt_cleared;	/* interrupt cleared on channel change */
	uint16	counter_noise_cal_cancelled;		/* trigger cancelled on channel change */

	uint16	macsusp_cnt;		/* mac suspend counter */
	uint8	amtbitmap;		/* AMT status bitamp */

	int8	chiptemp;		/* Chip temparature */
	int8	femtemp;		/* Fem temparature */

	uint8	cal_phase_id;		/* Current Multi phase cal ID */
	uint8	rxchain;		/* Rx Chain */
	uint8	txchain;		/* Tx Chain */
	uint8	ofdm_desense;		/* OFDM desense */

	uint8	slice;
	uint8	bphy_desense;		/* BPHY desense */
	uint8	pll_lockstatus;		/* PLL Lock status */

	/* dccal dcoe & idacc */
	uint8	dcc_err;		/* dccal health check error status */
	uint8	dcoe_num_tries;		/* number of retries on dcoe cal */
	uint8	idacc_num_tries;	/* number of retries on idac cal */

	uint8	dccal_phyrxchain;	/* phy rxchain during dc calibration */
	uint8	dccal_type;		/* DC cal type: single/multi phase, chan change, etc. */

	uint8	gbd_bphy_sleep_counter;	/* gbd sleep counter */
	uint8	gbd_ofdm_sleep_counter;	/* gbd sleep counter */

	/* desense data */
	int8	btcx_mode;		/* btcoex desense mode */
	int8	ltecx_mode;		/* lte coex desense mode */
	uint8	gbd_ofdm_desense;	/* gbd ofdm desense level */
	uint8	gbd_bphy_desense;	/* gbd bphy desense level */
	uint8	current_elna_bypass;	/* init gain desense: elna bypass */
	uint8	current_tia_idx;	/* init gain desense: tia index */
	uint8	current_lpf_idx;	/* init gain desense: lpf index */

	int8	weakest_rssi;		/* weakest link RSSI */

	bool	phycal_disable;		/* Set if calibration is disabled */
	bool	hwpwrctrlen;		/* tx hwpwrctrl enable */
	uint8	ocl_en_status;		/* OCL requested state and OCL HW state */
	uint32	measurehold;		/* PHY hold activities */
	uint32	ed_duration;		/* ccastats: ed_duration */
	uint16	ed_crs_status;		/* Status of ED and CRS during noise cal */

	uint16	pktprocdebug;
	uint16	pktprocdebug2;

	uint16	counter_noise_iqest_to;	/* count of IQ_Est time out */
	uint16	rfemstate2g;		/* rFEM state register for 2g */
	uint16	rfemstate5g;		/* rFEM state register for 5g */
	uint16	txiqcal_max_retry_cnt;	/* txiqlocal retries reached max allowed count */
	uint16	txiqcal_max_slope_cnt;	/* txiqlocal slope reached max allowed count */
	uint16	mppc_cal_failed_cnt;	/* MPPC cal failure count */

	uint16	gci_lst_inv_ctr;
	uint16	gci_lst_rst_ctr;
	uint16	gci_lst_sem_fail;
	uint16	gci_lst_rb_state;
	uint16	gci_lst_state_mask;
	uint16	gci_inv_tx;		/* Tx inv cnt */
	uint16	gci_inv_rx;		/* Rx inv cnt */
	uint16	gci_rst_tx;		/* gci Tx reset cnt */
	uint16	gci_rst_rx;		/* gci Rx reset cnt */
	uint16	gci_sem_fail;
	uint16	gci_invstate;		/* inv state */
	uint16	gci_phyctl0;		/* TX_PHYCTL0 */
	uint16	gci_phyctl2;		/* TX_PHYCTL2 */
	uint16	gci_ctxtst;		/* S_CTXTST */
	uint16	gci_chan;		/* GCI channel */
	uint16	gci_cm;
	uint16	gci_inv_intr;
	uint16	gci_rst_intr;
	uint16	gci_rst_prdc_rx;
	uint16	gci_rst_wk_rx;
	uint16	gci_rst_rmac_rx;
	uint16	gci_rst_tx_rx;

	uint16	rspfrm_ed_txncl_cnt;	/* Response frame not sent due to ED */

	uint8	rfem_rxmode_curr_hwstate;
	uint8   rfem_rxmode_bands_req;		/* mode as requested by SW layer */
	uint8   rfem_rxmode_bands_applied;	/* mode currently configured in HW */

	uint8	lpc_status;		/* Flag to enable/disable LPC, and runtime flag status */

	uint32	rxsense_disable_req_ch;	/* channel disable requests */
	uint32	ocl_disable_reqs;	/* OCL disable bitmap */

	int16	last_cal_temp;
	uint8	cal_reason;		/* reason for the cal */
	uint8	cal_suppressed_cntr_ed;	/* counter including ss, mp cals, MSB is current state */
	uint8	phylog_noise_mode;	/* Noise mode used */
	int8	rxsense_noise_idx;	/* rxsense detection threshold desense index */
	int8	rxsense_offset;		/* rxsense min power desense index */
	uint8	lpf_lut_type;		/* high or low noise radio lpf lut */
	uint8	noisecal_saved_radio_lpf_lut_type;	/* high or low noise radio lpf lut */

	uint8	noise_cal_mode;		/* noisecal mode */
	uint16	noise_cal_timeout;	/* noisecal timeout */

	uint16	txpwr_recalc_reasons;	/* Reasons bitmap for triggered Tx pwr recalc */

	uint16	channel_active;		/* Channel active status */

	uint32	interference_mode;	/* interference mitigation mode */
	uint32	power_mode;		/* LP/VLP logging */

	uint32	temp_sense_cnt;
	uint32	pm_dur;
	uint16	ncap_misc;

	uint16	nap_disable_reqs;	/* NAP disable bitmap */
	uint8	nap_en_status;		/* NAP enable status */

	uint8	tvpm_mitigation;	/* Bitmap of enabled and which mitigation active */

	uint16	phyctl_w0_pa0;		/* PHYCTL Word-0 with PA 0 */
	uint16	phyctl_w2_pa0;		/* PHYCTL Word-2 with PA 0 */
	uint16	phyctl_w5_pa0;		/* PHYCTL Word-5/6 with PA 0
						* Bit[7:0] Core0 PPR, Bit[15:8] Core1 PPR
						*/
	uint16  phyctl_w0_pa1;		/* PHYCTL Word-0 with PA 1 */
	uint16  phyctl_w2_pa1;		/* PHYCTL Word-2 with PA 1 */
	uint16  phyctl_w5_pa1;		/* PHYCTL Word-5/6 with PA 1
						* Bit[7:0] Core0 PPR, Bit[15:8] Core1 PPR
						*/
	uint16	phyctl_w0_pa2;		/* PHYCTL Word-0 with PA 2 */
	uint16	phyctl_w2_pa2;		/* PHYCTL Word-2 with PA 2 */
	uint16	phyctl_w5_pa2;		/* PHYCTL Word-5/6 with PA 2
						* Bit[7:0] Core0 PPR, Bit[15:8] Core1 PPR
						*/
	uint16	preempt_status2;	/* status of preemption */
	uint8	macsusp_phy_cnt;

	uint8	dsa_mode;
	uint8	dsa_state;
	uint8	dsa_boost;
	uint8	dsa_util[3];

	uint8	pa_mode;

	uint16	rxgpr0_cnt_ltestschange;
	uint32	txcap_reason;
	uint32	phywd_override_cnt;
	uint32	tx_prohibited_cnt;
	uint8	bsscolor;				/* bsscolor value from 0 to 63 */
	uint8	mws_coex_cellst_prev_currltests;	/* previous cellstatus */

	/* SWDiv */
	uint8	active_ant;			/* Lower byte */
	uint8	swap_trig_event_id;		/* Lower byte */
	uint8	swap_trig_event_id_prev;	/* Lower byte */

	/* Misc general purpose debug counters (will be used for future debugging) */
	uint8	debug_01;
	uint16	debug_02;
	uint32	debug_03;
	uint32	debug_04;
} phy_periodic_log_cmn_v16_t;

// From v15 for v41
typedef struct phy_periodic_log_cmn_v17 {
	uint32	txratespec;		/* Current Tx rate spec */
	uint32	rxratespec;		/* Current rx rate spec */
	uint32	duration;		/* millisecs spent sampling this channel */
	uint32	congest_ibss;		/* millisecs in our bss (presumably this traffic will */
					/*  move if cur bss moves channels) */
	uint32	congest_obss;		/* traffic not in our bss */
	uint32	interference;		/* millisecs detecting a non 802.11 interferer. */
	uint32	last_cal_time;		/* Last cal execution time */

	uint32	noise_cal_req_ts;	/* Time-stamp when noise cal was requested */
	uint32	noise_cal_intr_ts;	/* Time-stamp when noise cal was completed */
	uint32	phywdg_ts;		/* Time-stamp when wd was fired */
	uint32	phywd_dur;		/* Duration of the watchdog */
	uint32	chanspec_set_ts;	/* Time-stamp when chanspec was set */
	uint32	vcopll_failure_cnt;	/* Number of VCO cal failures including */
					/* failures detected in ucode */
	uint32	log_ts;			/* Time-stamp when this log was collected */

	/* glitch based desense input from cca */
	uint32	cca_stats_total_glitch;
	uint32	cca_stats_bphy_glitch;
	uint32	cca_stats_total_badplcp;
	uint32	cca_stats_bphy_badplcp;
	uint32	cca_stats_mbsstime;

	uint32	counter_noise_request;	/* count of noisecal request */
	uint32	counter_noise_crsbit;	/* count of crs high during noisecal request */
	uint32	counter_noise_apply;	/* count of applying noisecal result to crsmin */
	uint32	fullphycalcntr;		/* count of performing single phase cal */
	uint32	multiphasecalcntr;	/* count of performing multi-phase cal */

	uint32	macsusp_dur;		/* mac suspend duration */

	uint32	featureflag;		/* Currently active feature flags */

	uint16	chanspec;		/* Current phy chanspec */
	uint16	vbatmeas;		/* Measured VBAT sense value */

	/* HP2P related params */
	uint16	shm_mpif_cnt_val;
	uint16	shm_thld_cnt_val;
	uint16	shm_cts_cnt_val;
	uint16	shm_m_prewds_cnt;	/* Count of pre-wds fired in the ucode */

	uint16	navstat_max;

	uint16	timeoutstatus;
	uint16	deaf_count;		/* Depth of stay_in_carrier_search function */

	uint16	dcc_attempt_counter;	/* Number of DC cal attempts */
	uint16	dcc_fail_counter;	/* Number of DC cal failures */
	uint16	dcc_hcfail;		/* dcc health check failure count */
	uint16	dcc_calfail;		/* dcc failure count */

	uint16	btcxovrd_dur;		/* Cumulative btcx overide between WDGs */
	uint16	btcxovrd_err_cnt;	/* BTCX override flagged errors */

	uint16	femtemp_read_fail_counter;	/* Fem temparature read fail counter */
	uint16	phy_log_counter;
	uint16	noise_mmt_overdue;	/* Count up if ucode noise mmt is overdue for 5 sec */
	uint16	chan_switch_dur;	/* Channel switch time */

	uint16	crsmin_pwr_apply_cnt;	/* Count of desense power threshold update to phy */

	uint16	txpustatus;		/* txpu off definations */
	uint16	tempinvalid_count;	/* Count no. of invalid temp. measurements */
	uint16	log_event_id;		/* logging event id */

	uint16	counter_noise_interrupt_cleared;	/* interrupt cleared on channel change */
	uint16	counter_noise_cal_cancelled;		/* trigger cancelled on channel change */

	uint16	macsusp_cnt;		/* mac suspend counter */
	uint8	amtbitmap;		/* AMT status bitamp */

	int8	chiptemp;		/* Chip temparature */
	int8	femtemp;		/* Fem temparature */

	uint8	cal_phase_id;		/* Current Multi phase cal ID */
	uint8	rxchain;		/* Rx Chain */
	uint8	txchain;		/* Tx Chain */
	uint8	ofdm_desense;		/* OFDM desense */

	uint8	slice;
	uint8	bphy_desense;		/* BPHY desense */
	uint8	pll_lockstatus;		/* PLL Lock status */

	/* dccal dcoe & idacc */
	uint8	dcc_err;		/* dccal health check error status */
	uint8	dcoe_num_tries;		/* number of retries on dcoe cal */
	uint8	idacc_num_tries;	/* number of retries on idac cal */

	uint8	dccal_phyrxchain;	/* phy rxchain during dc calibration */
	uint8	dccal_type;		/* DC cal type: single/multi phase, chan change, etc. */

	uint8	gbd_bphy_sleep_counter;	/* gbd sleep counter */
	uint8	gbd_ofdm_sleep_counter;	/* gbd sleep counter */

	/* desense data */
	int8	btcx_mode;		/* btcoex desense mode */
	int8	ltecx_mode;		/* lte coex desense mode */
	uint8	gbd_ofdm_desense;	/* gbd ofdm desense level */
	uint8	gbd_bphy_desense;	/* gbd bphy desense level */
	uint8	current_elna_bypass;	/* init gain desense: elna bypass */
	uint8	current_tia_idx;	/* init gain desense: tia index */
	uint8	current_lpf_idx;	/* init gain desense: lpf index */

	int8	weakest_rssi;		/* weakest link RSSI */

	bool	phycal_disable;		/* Set if calibration is disabled */
	bool	hwpwrctrlen;		/* tx hwpwrctrl enable */
	uint8	ocl_en_status;		/* OCL requested state and OCL HW state */
	uint32	measurehold;		/* PHY hold activities */
	uint32	ed_duration;		/* ccastats: ed_duration */
	uint16	ed_crs_status;		/* Status of ED and CRS during noise cal */

	uint16	pktprocdebug;
	uint16	pktprocdebug2;

	uint16	counter_noise_iqest_to;	/* count of IQ_Est time out */
	uint16	rfemstate2g;		/* rFEM state register for 2g */
	uint16	rfemstate5g;		/* rFEM state register for 5g */
	uint16	txiqcal_max_retry_cnt;	/* txiqlocal retries reached max allowed count */
	uint16	txiqcal_max_slope_cnt;	/* txiqlocal slope reached max allowed count */
	uint16	mppc_cal_failed_cnt;	/* MPPC cal failure count */

	uint16	rspfrm_ed_txncl_cnt;	/* Response frame not sent due to ED */

	uint8	rfem_rxmode_curr_hwstate;
	uint8   rfem_rxmode_bands_req;		/* mode as requested by SW layer */
	uint8   rfem_rxmode_bands_applied;	/* mode currently configured in HW */

	uint8	lpc_status;		/* Flag to enable/disable LPC, and runtime flag status */

	uint32	rxsense_disable_req_ch;	/* channel disable requests */
	uint32	ocl_disable_reqs;	/* OCL disable bitmap */

	int16	last_cal_temp;
	uint8	cal_reason;		/* reason for the cal */
	uint8	cal_suppressed_cntr_ed;	/* counter including ss, mp cals, MSB is current state */
	uint8	cal_suppressed_cntr_phymute;	/* Cal suppressed counter due to PHY MUTE */
	uint8	cal_suppressed_cntr_fullcal;	/* Cal suppressed counter due to no Full cal */
	uint16	cal_suppressed_cntr_mphase;	/* Cal suppressed counter due to SW */
	uint8	cal_suppressed_cntr_reason01;	/* Cal suppressed counter due to reason 1 */
	uint8	cal_suppressed_bitmap;		/* Cal suppressed bitmap */
	uint8   cal_mphase_retry_cnt;	/* mphase cal retry count */
	uint8	chiptemp_retry_cnt;
	int8	rxsense_noise_idx;	/* rxsense detection threshold desense index */
	int8	rxsense_offset;		/* rxsense min power desense index */

	uint8	rccal_lpf_tmout;
	uint8	rccal_tia_tmout;
	uint8	rccal_rxpll_tmout;
	uint8	rccal_btadcfb_tmout;
	uint8	rccal_btadcres_tmout;
	uint8	rccal_btbpf_tmout;

	uint8	macsusp_phy_cnt;
	uint8	pa_mode;
	uint16	channel_active;		/* Channel active status */

	uint8	mws_coex_cellst_prev_currltests;	/* previous cellstatus */

	uint8	noise_cal_mode;		/* noisecal mode */
	uint16	noise_cal_timeout;	/* noisecal timeout */

	uint32	interference_mode;	/* interference mitigation mode */
	uint32	power_mode;		/* LP/VLP logging */

	uint32	temp_sense_cnt;
	uint32	pm_dur;

	uint16	nap_disable_reqs;	/* NAP disable bitmap */
	uint8	nap_en_status;		/* NAP enable status */
	uint8	phylog_noise_mode;	/* Noise mode used */

	uint16	phyctl_w0_pa0;		/* PHYCTL Word-0 with PA 0 */
	uint16	phyctl_w2_pa0;		/* PHYCTL Word-2 with PA 0 */
	uint16	phyctl_w5_pa0;		/* PHYCTL Word-5/6 with PA 0
						* Bit[7:0] Core0 PPR, Bit[15:8] Core1 PPR
						*/
	uint16	phyctl_w0_pa1;		/* PHYCTL Word-0 with PA 1 */
	uint16	phyctl_w2_pa1;		/* PHYCTL Word-2 with PA 1 */
	uint16	phyctl_w5_pa1;		/* PHYCTL Word-5/6 with PA 1
						* Bit[7:0] Core0 PPR, Bit[15:8] Core1 PPR
						*/
	uint16	phyctl_w0_pa2;		/* PHYCTL Word-0 with PA 2 */
	uint16	phyctl_w2_pa2;		/* PHYCTL Word-2 with PA 2 */
	uint16	phyctl_w5_pa2;		/* PHYCTL Word-5/6 with PA 2
						* Bit[7:0] Core0 PPR, Bit[15:8] Core1 PPR
						*/
	uint16	preempt_status2;	/* status of preemption */

	uint32	navstat_acc;

	uint16	txdc;
	uint8	tvpm_mitigation;	/* Bitmap of enabled and which mitigation active */
	uint8	tvpm_dynamic_rate;	/* dynamic update monitor rate */
	uint8	dsa_mode;
	uint8	dsa_state;
	uint8	dsa_boost;
	uint8	dsa_util[6u];		/* Size subject to change */

	uint8	bsscolor;		/* bsscolor value from 0 to 63 */

	uint16	rxgpr0_cnt_ltestschange;

	uint32	phywd_override_cnt;
	uint32	tx_prohibited_cnt;
	uint32	txcap_reason;

	uint16	txpwr_recalc_reasons;	/* Reasons bitmap for triggered Tx pwr recalc */
	uint16	crs_pwrind_status;

	/* SWDiv */
	uint8	active_ant;		/* Lower byte */
	uint8	swap_trig_event_id;	/* Lower byte */
	uint8	swap_trig_event_id_prev;	/* Lower byte */

	uint8	dig_gain;
	uint8	dcest_overflow_cnt;

	uint8	tiadccal_i_retry;
	uint8	tiadccal_q_retry;
	uint8	femtemp_retry_cnt;
	int8	cal_ctsdur_ms;

	/* Misc general purpose debug counters (will be used for future debugging) */
	uint8	debug_01;
	uint16	debug_02;
} phy_periodic_log_cmn_v17_t;

// From v17 for v42
typedef struct phy_periodic_log_cmn_v18 {
	uint32	txratespec;		/* Current Tx rate spec */
	uint32	rxratespec;		/* Current rx rate spec */
	uint32	duration;		/* millisecs spent sampling this channel */
	uint32	congest_ibss;		/* millisecs in our bss (presumably this traffic will */
					/*  move if cur bss moves channels) */
	uint32	congest_obss;		/* traffic not in our bss */
	uint32	congest_meonly;		/* microsecs in my own traffic (TX + RX) */
	uint32	interference;		/* millisecs detecting a non 802.11 interferer. */
	uint32	last_cal_time;		/* Last cal execution time */

	uint32	noise_cal_req_ts;	/* Time-stamp when noise cal was requested */
	uint32	noise_cal_intr_ts;	/* Time-stamp when noise cal was completed */
	uint32	phywdg_ts;		/* Time-stamp when wd was fired */
	uint32	phywd_dur;		/* Duration of the watchdog */
	uint32	chanspec_set_ts;	/* Time-stamp when chanspec was set */
	uint32	vcopll_failure_cnt;	/* Number of VCO cal failures including */
					/* failures detected in ucode */
	uint32	log_ts;			/* Time-stamp when this log was collected */

	/* glitch based desense input from cca */
	uint32	cca_stats_total_glitch;
	uint32	cca_stats_bphy_glitch;
	uint32	cca_stats_total_badplcp;
	uint32	cca_stats_bphy_badplcp;
	uint32	cca_stats_mbsstime;

	uint32	counter_noise_request;	/* count of noisecal request */
	uint32	counter_noise_crsbit;	/* count of crs high during noisecal request */
	uint32	counter_noise_apply;	/* count of applying noisecal result to crsmin */
	uint32	fullphycalcntr;		/* count of performing single phase cal */
	uint32	multiphasecalcntr;	/* count of performing multi-phase cal */

	uint32	macsusp_dur;		/* mac suspend duration */

	uint32	featureflag;		/* Currently active feature flags */

	uint16	chanspec;		/* Current phy chanspec */
	uint16	vbatmeas;		/* Measured VBAT sense value */

	/* HP2P related params */
	uint16	shm_mpif_cnt_val;
	uint16	shm_thld_cnt_val;
	uint16	shm_cts_cnt_val;
	uint16	shm_m_prewds_cnt;	/* Count of pre-wds fired in the ucode */

	uint16	navstat_max;

	uint16	timeoutstatus;
	uint16	deaf_count;		/* Depth of stay_in_carrier_search function */

	uint16	dcc_attempt_counter;	/* Number of DC cal attempts */
	uint16	dcc_fail_counter;	/* Number of DC cal failures */
	uint16	dcc_hcfail;		/* dcc health check failure count */
	uint16	dcc_calfail;		/* dcc failure count */

	uint16	btcxovrd_dur;		/* Cumulative btcx overide between WDGs */
	uint16	btcxovrd_err_cnt;	/* BTCX override flagged errors */

	uint16	femtemp_read_fail_counter;	/* Fem temparature read fail counter */
	uint16	phy_log_counter;
	uint16	noise_mmt_overdue;	/* Count up if ucode noise mmt is overdue for 5 sec */
	uint16	chan_switch_dur;	/* Channel switch time */

	uint16	crsmin_pwr_apply_cnt;	/* Count of desense power threshold update to phy */

	uint16	txpustatus;		/* txpu off definations */
	uint16	tempinvalid_count;	/* Count no. of invalid temp. measurements */
	uint16	log_event_id;		/* logging event id */

	uint16	counter_noise_interrupt_cleared;	/* interrupt cleared on channel change */
	uint16	counter_noise_cal_cancelled;		/* trigger cancelled on channel change */

	uint16	macsusp_cnt;		/* mac suspend counter */
	uint8	amtbitmap;		/* AMT status bitamp */

	int8	chiptemp;		/* Chip temparature */
	int8	femtemp;		/* Fem temparature */

	uint8	cal_phase_id;		/* Current Multi phase cal ID */
	uint8	rxchain;		/* Rx Chain */
	uint8	txchain;		/* Tx Chain */
	uint8	ofdm_desense;		/* OFDM desense */

	uint8	slice;
	uint8	bphy_desense;		/* BPHY desense */
	uint8	pll_lockstatus;		/* PLL Lock status */

	/* dccal dcoe & idacc */
	uint8	dcc_err;		/* dccal health check error status */
	uint8	dcoe_num_tries;		/* number of retries on dcoe cal */
	uint8	idacc_num_tries;	/* number of retries on idac cal */

	uint8	dccal_phyrxchain;	/* phy rxchain during dc calibration */
	uint8	dccal_type;		/* DC cal type: single/multi phase, chan change, etc. */

	uint8	gbd_bphy_sleep_counter;	/* gbd sleep counter */
	uint8	gbd_ofdm_sleep_counter;	/* gbd sleep counter */

	/* desense data */
	int8	btcx_mode;		/* btcoex desense mode */
	int8	ltecx_mode;		/* lte coex desense mode */
	uint8	gbd_ofdm_desense;	/* gbd ofdm desense level */
	uint8	gbd_bphy_desense;	/* gbd bphy desense level */
	uint8	current_elna_bypass;	/* init gain desense: elna bypass */
	uint8	current_tia_idx;	/* init gain desense: tia index */
	uint8	current_lpf_idx;	/* init gain desense: lpf index */

	int8	weakest_rssi;		/* weakest link RSSI */

	bool	phycal_disable;		/* Set if calibration is disabled */
	bool	hwpwrctrlen;		/* tx hwpwrctrl enable */
	uint8	ocl_en_status;		/* OCL requested state and OCL HW state */
	uint32	measurehold;		/* PHY hold activities */
	uint32	ed_duration;		/* ccastats: ed_duration */
	uint16	ed_crs_status;		/* Status of ED and CRS during noise cal */

	uint16	pktprocdebug;
	uint16	pktprocdebug2;

	uint16	counter_noise_iqest_to;	/* count of IQ_Est time out */
	uint16	rfemstate2g;		/* rFEM state register for 2g */
	uint16	rfemstate5g;		/* rFEM state register for 5g */
	uint16	txiqcal_max_retry_cnt;	/* txiqlocal retries reached max allowed count */
	uint16	txiqcal_max_slope_cnt;	/* txiqlocal slope reached max allowed count */
	uint16	mppc_cal_failed_cnt;	/* MPPC cal failure count */

	uint16	rspfrm_ed_txncl_cnt;	/* Response frame not sent due to ED */

	uint8	rfem_rxmode_curr_hwstate;
	uint8   rfem_rxmode_bands_req;		/* mode as requested by SW layer */
	uint8   rfem_rxmode_bands_applied;	/* mode currently configured in HW */

	uint8	lpc_status;		/* Flag to enable/disable LPC, and runtime flag status */

	uint32	rxsense_disable_req_ch;	/* channel disable requests */
	uint32	ocl_disable_reqs;	/* OCL disable bitmap */

	int16	last_cal_temp;
	uint8	cal_reason;		/* reason for the cal */

	uint8	cal_suppressed_cntr_ed;	/* counter including ss, mp cals, MSB is current state */
	uint8	cal_suppressed_cntr_phymute;	/* Cal suppressed counter due to PHY MUTE */
	uint8	cal_suppressed_cntr_fullcal;	/* Cal suppressed counter due to no Full cal */
	uint16	cal_suppressed_cntr_mphase;	/* Cal suppressed counter due to SW */
	uint8	cal_suppressed_cntr_reason01;	/* Cal suppressed counter due to reason 1 */
	uint8	cal_suppressed_bitmap;		/* Cal suppressed bitmap */
	uint8   cal_mphase_retry_cnt;	/* mphase cal retry count */
	uint8	chiptemp_retry_cnt;
	int8	rxsense_noise_idx;	/* rxsense detection threshold desense index */
	int8	rxsense_offset;		/* rxsense min power desense index */

	uint8	rccal_lpf_tmout;
	uint8	rccal_tia_tmout;
	uint8	rccal_rxpll_tmout;
	uint8	rccal_btadcfb_tmout;
	uint8	rccal_btadcres_tmout;
	uint8	rccal_btbpf_tmout;

	uint8	macsusp_phy_cnt;
	uint8	pa_mode;
	uint16	channel_active;		/* Channel active status */

	uint8	mws_coex_cellst_prev_currltests;	/* previous cellstatus */

	uint8	noise_cal_mode;		/* noisecal mode */
	uint16	noise_cal_timeout;	/* noisecal timeout */

	uint32	interference_mode;	/* interference mitigation mode */
	uint32	power_mode;		/* LP/VLP logging */

	uint32	temp_sense_cnt;
	uint32	pm_dur;

	uint16	nap_disable_reqs;	/* NAP disable bitmap */
	uint8	nap_en_status;		/* NAP enable status */
	uint8	phylog_noise_mode;	/* Noise mode used */

	uint16	phyctl_w0_pa0;		/* PHYCTL Word-0 with PA 0 */
	uint16	phyctl_w2_pa0;		/* PHYCTL Word-2 with PA 0 */
	uint16	phyctl_w5_pa0;		/* PHYCTL Word-5/6 with PA 0
						* Bit[7:0] Core0 PPR, Bit[15:8] Core1 PPR
						*/
	uint16	phyctl_w0_pa1;		/* PHYCTL Word-0 with PA 1 */
	uint16	phyctl_w2_pa1;		/* PHYCTL Word-2 with PA 1 */
	uint16	phyctl_w5_pa1;		/* PHYCTL Word-5/6 with PA 1
						* Bit[7:0] Core0 PPR, Bit[15:8] Core1 PPR
						*/
	uint16	phyctl_w0_pa2;		/* PHYCTL Word-0 with PA 2 */
	uint16	phyctl_w2_pa2;		/* PHYCTL Word-2 with PA 2 */
	uint16	phyctl_w5_pa2;		/* PHYCTL Word-5/6 with PA 2
						* Bit[7:0] Core0 PPR, Bit[15:8] Core1 PPR
						*/
	uint16	preempt_status2;	/* status of preemption */

	uint32	navstat_acc;

	uint16	txdc;
	uint8	tvpm_mitigation;	/* Bitmap of enabled and which mitigation active */
	uint8	tvpm_dynamic_rate;	/* dynamic update monitor rate */
	uint8	dsa_mode;
	uint8	dsa_state;
	uint8	dsa_boost;
	uint8	dsa_util[6u];		/* Size subject to change */

	uint8	bsscolor;		/* bsscolor value from 0 to 63 */

	uint16	rxgpr0_cnt_ltestschange;

	uint32	phywd_override_cnt;
	uint32	tx_prohibited_cnt;
	uint32	txcap_reason;

	uint16	txpwr_recalc_reasons;	/* Reasons bitmap for triggered Tx pwr recalc */
	uint16	crs_pwrind_status;

	/* SWDiv */
	uint8	active_ant;		/* Lower byte */
	uint8	swap_trig_event_id;	/* Lower byte */
	uint8	swap_trig_event_id_prev;	/* Lower byte */

	uint8	dig_gain;
	uint8	dcest_overflow_cnt;

	uint8	tiadccal_i_retry;
	uint8	tiadccal_q_retry;
	uint8	femtemp_retry_cnt;
	int8	cal_ctsdur_ms;

	uint8	bsscolor_parse_ctrl;
	uint8	bsscolor_idx0;
	uint8	bsscolor_idx1;
	uint32	phy_chansw_cb_cal_counter;	/* # of times cal done from callback */
	uint16	bsscolor_change_cnt;
	uint16	pp;				/* Puncture pattern */
	uint16	pp_80;
	uint16	pp_160;
	uint16	pp_bw;
	uint16	phy_dbg_cnt_regwr_bank1;	/* How many times shadowed-bank1 is set */
	uint16	phy_dbg_cnt_set_chanind1;	/* How many times bank1 regs are written */
	uint16	tpc_tx_baseidxchk_dis;
	uint16	tpc_olpcwar;
	uint16	tpc_txbaseidx0;
	uint16	tpc_txbaseidx127;
	uint16	tpc_txpwrctrl_reset_count;	/* For later WD implementation */
	uint16	noise_iqest2_status;		/* iqest hw status indication */
	uint16	noise_iqest2_trig;		/* number of iqest triggers */
	uint16	noise_iqest2_failcnt;		/* accumulated fail counts during iqest */
	uint16	noise_iqest2_rstcca;		/* number of reset cca during iqest */
	uint8	hc_tempfail_count;
	uint8	tof_active_ctr;
	uint8	chan_switch_indicator;

	/* Misc general purpose debug counters (will be used for future debugging) */
	uint8	debug_01;
	uint32	debug_02;
	uint32	debug_03;
	uint32	debug_04;
	uint32	debug_05;
	uint32	debug_06;
} phy_periodic_log_cmn_v18_t;

// From v18 for v43
typedef struct phy_periodic_log_cmn_v19 {
	uint32	txratespec;		/* Current Tx rate spec */
	uint32	rxratespec;		/* Current rx rate spec */
	uint32	duration;		/* millisecs spent sampling this channel */
	uint32	congest_ibss;		/* millisecs in our bss (presumably this traffic will */
					/*  move if cur bss moves channels) */
	uint32	congest_obss;		/* traffic not in our bss */
	uint32	congest_meonly;		/* microsecs in my own traffic (TX + RX) */
	uint32	interference;		/* millisecs detecting a non 802.11 interferer. */
	uint32	last_cal_time;		/* Last cal execution time */

	uint32	noise_cal_req_ts;	/* Time-stamp when noise cal was requested */
	uint32	noise_cal_intr_ts;	/* Time-stamp when noise cal was completed */
	uint32	phywdg_ts;		/* Time-stamp when wd was fired */
	uint32	phywd_dur;		/* Duration of the watchdog */
	uint32	chanspec_set_ts;	/* Time-stamp when chanspec was set */
	uint32	vcopll_failure_cnt;	/* Number of VCO cal failures including */
					/* failures detected in ucode */
	uint32	log_ts;			/* Time-stamp when this log was collected */

	/* glitch based desense input from cca */
	uint32	cca_stats_total_glitch;
	uint32	cca_stats_bphy_glitch;
	uint32	cca_stats_total_badplcp;
	uint32	cca_stats_bphy_badplcp;
	uint32	cca_stats_mbsstime;

	uint32	counter_noise_request;	/* count of noisecal request */
	uint32	counter_noise_crsbit;	/* count of crs high during noisecal request */
	uint32	counter_noise_apply;	/* count of applying noisecal result to crsmin */
	uint32	fullphycalcntr;		/* count of performing single phase cal */
	uint32	multiphasecalcntr;	/* count of performing multi-phase cal */

	uint32	macsusp_dur;		/* mac suspend duration */

	uint32	featureflag;		/* Currently active feature flags */

	uint16	chanspec;		/* Current phy chanspec */
	uint16	debug_01;

	/* HP2P related params */
	uint16	shm_mpif_cnt_val;
	uint16	shm_thld_cnt_val;
	uint16	shm_cts_cnt_val;
	uint16	shm_m_prewds_cnt;	/* Count of pre-wds fired in the ucode */

	uint16	navstat_max;

	uint16	timeoutstatus;
	uint16	deaf_count;		/* Depth of stay_in_carrier_search function */

	uint16	dcc_attempt_counter;	/* Number of DC cal attempts */
	uint16	dcc_fail_counter;	/* Number of DC cal failures */
	uint16	dcc_hcfail;		/* dcc health check failure count */
	uint16	dcc_calfail;		/* dcc failure count */

	uint16	btcxovrd_dur;		/* Cumulative btcx overide between WDGs */
	uint16	btcxovrd_err_cnt;	/* BTCX override flagged errors */

	uint16	femtemp_read_fail_counter;	/* Fem temparature read fail counter */
	uint16	phy_log_counter;
	uint16	noise_mmt_overdue;	/* Count up if ucode noise mmt is overdue for 5 sec */
	uint16	chan_switch_dur;	/* Channel switch time */

	uint16	crsmin_pwr_apply_cnt;	/* Count of desense power threshold update to phy */

	uint16	txpustatus;		/* txpu off definations */
	uint16	tempinvalid_count;	/* Count no. of invalid temp. measurements */
	uint16	log_event_id;		/* logging event id */

	uint16	counter_noise_interrupt_cleared;	/* interrupt cleared on channel change */
	uint16	counter_noise_cal_cancelled;		/* trigger cancelled on channel change */

	uint16	macsusp_cnt;		/* mac suspend counter */
	uint8	amtbitmap;		/* AMT status bitamp */

	int8	chiptemp;		/* Chip temparature */
	int8	femtemp;		/* Fem temparature */

	uint8	cal_phase_id;		/* Current Multi phase cal ID */
	uint8	rxchain;		/* Rx Chain */
	uint8	txchain;		/* Tx Chain */
	uint8	ofdm_desense;		/* OFDM desense */

	uint8	slice;
	uint8	bphy_desense;		/* BPHY desense */
	uint8	pll_lockstatus;		/* PLL Lock status */

	/* dccal dcoe & idacc */
	uint8	dcc_err;		/* dccal health check error status */
	uint8	dcoe_num_tries;		/* number of retries on dcoe cal */
	uint8	idacc_num_tries;	/* number of retries on idac cal */

	uint8	dccal_phyrxchain;	/* phy rxchain during dc calibration */
	uint8	dccal_type;		/* DC cal type: single/multi phase, chan change, etc. */

	uint8	gbd_bphy_sleep_counter;	/* gbd sleep counter */
	uint8	gbd_ofdm_sleep_counter;	/* gbd sleep counter */

	/* desense data */
	int8	btcx_mode;		/* btcoex desense mode */
	int8	ltecx_mode;		/* lte coex desense mode */
	uint8	gbd_ofdm_desense;	/* gbd ofdm desense level */
	uint8	gbd_bphy_desense;	/* gbd bphy desense level */
	uint8	current_elna_bypass;	/* init gain desense: elna bypass */
	uint8	current_tia_idx;	/* init gain desense: tia index */
	uint8	current_lpf_idx;	/* init gain desense: lpf index */

	int8	weakest_rssi;		/* weakest link RSSI */

	bool	phycal_disable;		/* Set if calibration is disabled */
	bool	hwpwrctrlen;		/* tx hwpwrctrl enable */
	uint8	ocl_en_status;		/* OCL requested state and OCL HW state */
	uint32	measurehold;		/* PHY hold activities */
	uint32	ed_duration;		/* ccastats: ed_duration */
	uint16	ed_crs_status;		/* Status of ED and CRS during noise cal */

	uint16	pktprocdebug;
	uint16	pktprocdebug2;

	uint16	counter_noise_iqest_to;	/* count of IQ_Est time out */
	uint16	rfemstate2g;		/* rFEM state register for 2g */
	uint16	rfemstate5g;		/* rFEM state register for 5g */
	uint16	txiqcal_max_retry_cnt;	/* txiqlocal retries reached max allowed count */
	uint16	txiqcal_max_slope_cnt;	/* txiqlocal slope reached max allowed count */
	uint16	mppc_cal_failed_cnt;	/* MPPC cal failure count */

	uint16	rspfrm_ed_txncl_cnt;	/* Response frame not sent due to ED */

	uint8	rfem_rxmode_curr_hwstate;
	uint8   rfem_rxmode_bands_req;		/* mode as requested by SW layer */
	uint8   rfem_rxmode_bands_applied;	/* mode currently configured in HW */

	uint8	lpc_status;		/* Flag to enable/disable LPC, and runtime flag status */

	uint32	rxsense_disable_req_ch;	/* channel disable requests */
	uint32	ocl_disable_reqs;	/* OCL disable bitmap */

	int16	last_cal_temp;
	uint8	cal_reason;		/* reason for the cal */

	uint8	cal_suppressed_cntr_ed;	/* counter including ss, mp cals, MSB is current state */
	uint8	cal_suppressed_cntr_phymute;	/* Cal suppressed counter due to PHY MUTE */
	uint8	cal_suppressed_cntr_fullcal;	/* Cal suppressed counter due to no Full cal */
	uint16	cal_suppressed_cntr_mphase;	/* Cal suppressed counter due to SW */
	uint8	cal_suppressed_cntr_reason01;	/* Cal suppressed counter due to reason 1 */
	uint8	cal_suppressed_bitmap;		/* Cal suppressed bitmap */
	uint8   cal_mphase_retry_cnt;	/* mphase cal retry count */
	uint8	chiptemp_retry_cnt;
	int8	rxsense_noise_idx;	/* rxsense detection threshold desense index */
	int8	rxsense_offset;		/* rxsense min power desense index */

	uint8	rccal_lpf_tmout;
	uint8	rccal_tia_tmout;
	uint8	rccal_rxpll_tmout;
	uint8	rccal_btadcfb_tmout;
	uint8	rccal_btadcres_tmout;
	uint8	rccal_btbpf_tmout;

	uint8	macsusp_phy_cnt;
	uint8	pa_mode;
	uint16	channel_active;		/* Channel active status */

	uint8	mws_coex_cellst_prev_currltests;	/* previous cellstatus */

	uint8	noise_cal_mode;		/* noisecal mode */
	uint16	noise_cal_timeout;	/* noisecal timeout */

	uint32	interference_mode;	/* interference mitigation mode */
	uint32	power_mode;		/* LP/VLP logging */

	uint32	temp_sense_cnt;
	uint32	pm_dur;

	uint16	nap_disable_reqs;	/* NAP disable bitmap */
	uint8	nap_en_status;		/* NAP enable status */
	uint8	phylog_noise_mode;	/* Noise mode used */

	uint16	phyctl_w0_pa0;		/* PHYCTL Word-0 with PA 0 */
	uint16	phyctl_w2_pa0;		/* PHYCTL Word-2 with PA 0 */
	uint16	phyctl_w5_pa0;		/* PHYCTL Word-5/6 with PA 0
						* Bit[7:0] Core0 PPR, Bit[15:8] Core1 PPR
						*/
	uint16	phyctl_w0_pa1;		/* PHYCTL Word-0 with PA 1 */
	uint16	phyctl_w2_pa1;		/* PHYCTL Word-2 with PA 1 */
	uint16	phyctl_w5_pa1;		/* PHYCTL Word-5/6 with PA 1
						* Bit[7:0] Core0 PPR, Bit[15:8] Core1 PPR
						*/
	uint16	phyctl_w0_pa2;		/* PHYCTL Word-0 with PA 2 */
	uint16	phyctl_w2_pa2;		/* PHYCTL Word-2 with PA 2 */
	uint16	phyctl_w5_pa2;		/* PHYCTL Word-5/6 with PA 2
						* Bit[7:0] Core0 PPR, Bit[15:8] Core1 PPR
						*/
	uint16	preempt_status2;	/* status of preemption */

	uint32	navstat_acc;

	uint16	txdc;
	uint8	tvpm_mitigation;	/* Bitmap of enabled and which mitigation active */
	uint8	tvpm_dynamic_rate;	/* dynamic update monitor rate */
	uint8	dsa_mode;
	uint8	dsa_state;
	uint8	dsa_boost;
	uint8	dsa_util[6u];		/* Size subject to change */

	uint8	bsscolor;		/* bsscolor value from 0 to 63 */

	uint16	rxgpr0_cnt_ltestschange;

	uint32	phywd_override_cnt;
	uint32	tx_prohibited_cnt;
	uint32	txcap_reason;

	uint16	txpwr_recalc_reasons;	/* Reasons bitmap for triggered Tx pwr recalc */
	uint16	crs_pwrind_status;

	/* SWDiv */
	uint8	active_ant;		/* Lower byte */
	uint8	swap_trig_event_id;	/* Lower byte */
	uint8	swap_trig_event_id_prev;	/* Lower byte */

	uint8	dig_gain;
	uint8	dcest_overflow_cnt;

	uint8	tiadccal_i_retry;
	uint8	tiadccal_q_retry;
	uint8	femtemp_retry_cnt;
	int8	cal_ctsdur_ms;

	uint8	bsscolor_parse_ctrl;
	uint8	bsscolor_idx0;
	uint8	bsscolor_idx1;
	uint32	phy_chansw_cb_cal_counter;	/* # of times cal done from callback */
	uint16	bsscolor_change_cnt;
	uint16	pp;				/* Puncture pattern */
	uint16	pp_80;
	uint16	pp_160;
	uint16	pp_bw;
	uint16	phy_dbg_cnt_regwr_bank1;	/* How many times shadowed-bank1 is set */
	uint16	phy_dbg_cnt_set_chanind1;	/* How many times bank1 regs are written */
	uint16	tpc_tx_baseidxchk_dis;
	uint16	tpc_olpcwar;
	uint16	tpc_txbaseidx0;
	uint16	tpc_txbaseidx127;
	uint16	tpc_txpwrctrl_reset_count;	/* For later WD implementation */
	uint16	noise_iqest2_status;		/* iqest hw status indication */
	uint16	noise_iqest2_trig;		/* number of iqest triggers */
	uint16	noise_iqest2_failcnt;		/* accumulated fail counts during iqest */
	uint16	noise_iqest2_rstcca;		/* number of reset cca during iqest */
	uint16	noise_iqest2_rstcca_fw;		/* number of reset cca during iqest from fw */
	uint8	hc_tempfail_count;
	uint8	tof_active_ctr;
	uint8	chan_switch_indicator;
	uint8	lo_divider_recovery_fail_cnt;
	uint8	lo_divider_recovery_pass_cnt;
	uint8	lo_sync_war_cnt;
	uint32	accum_pmdur;
	uint16	dcest_overflow_gainidx;

	/* Misc general purpose debug counters (will be used for future debugging) */
	uint16	debug_02;
	uint32	debug_03;
	uint32	debug_04;
	uint32	debug_05;
	uint32	debug_06;
} phy_periodic_log_cmn_v19_t;

#define PHYCTL_WORDS	3u
#define PA_MODE_SIZE	3u
// From v19 for v44
typedef struct phy_periodic_log_cmn_v20 {
	/* High level info */
	uint16	chanspec;		/* Current phy chanspec */
	uint8	channel_active;		/* Channel active status */
	uint8	slice;
	uint16	log_event_id;		/* logging event id */
	uint16	ap_chanspec_bss;
	uint32	measurehold;		/* PHY hold activities */
	uint32	featureflag;		/* Currently active feature flags */
	int8	weakest_rssi;		/* weakest link RSSI */

	/* SW Config */
	uint8	amtbitmap;		/* AMT status bitamp */
	uint8	rxchain;		/* Rx Chain */
	uint8	txchain;		/* Tx Chain */
	uint32	txratespec;		/* Current Tx rate spec */
	uint32	rxratespec;		/* Current rx rate spec */
	uint32	scb_flags;
	uint32	scb_flags2;
	uint32	scb_flags3;

	/* CCA1 */
	uint32	congest_duration_ms;	/* millisecs spent sampling this channel */
	uint32	congest_meonly_ms;	/* microsecs in my own traffic (TX + RX) */
	uint32	congest_ibss_ms;	/* millisecs in our bss (presumably this traffic will */
					/*  move if cur bss moves channels) */
	uint32	congest_obss_ms;	/* traffic not in our bss */
	uint32	congest_interference_ms; /* millisecs detecting a non 802.11 interferer. */
	uint32	congest_ed_duration_ms;	/* ccastats: ed_duration */

	/* ED */
	uint16	ed_crs_status;		/* Status of ED and CRS during noise cal */
	uint16	ed_rspfrm_txncl_cnt_shm;	/* Response frame not sent due to ED */

	/* glitch based desense input from cca */
	uint32	cca_stats_total_glitch;
	uint32	cca_stats_bphy_glitch;
	uint32	cca_stats_total_badplcp;
	uint32	cca_stats_bphy_badplcp;
	uint32	cca_stats_mbsstime;

	/* MAC Susp */
	uint32	macsusp_dur_us;		/* mac suspend duration */
	uint8	macsusp_cnt;		/* mac suspend counter */
	uint8	macsusp_phy_cnt;

	/* Ucode State */
	uint16	prewds_cnt_shm;		/* Count of pre-wds fired in the ucode */
	uint32	tsf_l_us;		/* Ucode TSF_ML/L */
	uint16	prewds_rxe_status1_shm;
	uint16	prewds_rxe_status2_shm;
	uint16	prewds_phyfifo_wd_cnt_shm;
	uint16	prewds_txcrsstk_macwar_cnt_shm;
	uint16	prewds_txcrsstk_phywar_cnt_shm;
	uint16	prewds_txsm_not_clean_cnt_shm;
	uint16	tx_preempt_btcx_wlan_cnt_shm;
	uint16	tx_preempt_uwbcx_cnt_shm;
	uint16	tx_preempt_rc1cx_wlan_cnt_shm;
	uint16	txpustatus_shm;		/* txpu off definations */
	uint16	tdmtx_txpuen_shm;
	uint16	tdmtx_txpudis_shm;
	uint16	tdmtx_txmute_shm;
	uint16	hpp_mpif_cnt_shm;
	uint16	hpp_thld_cnt_shm;
	uint16	hpp_cts_cnt_shm;

	/* NAV */
	uint32	navstat_acc_shm;
	uint16	navstat_max_shm_us;

	/* Noise */
	uint16	ncal_crsmin_pwr_apply_cnt;	/* Cnt of desense power threshold update to phy */
	uint32	ncal_request_cnt;	/* count of noisecal request */
	uint32	ncal_crsbit_cnt;	/* count of crs high during noisecal request */
	uint32	ncal_apply_cnt;		/* count of applying noisecal result to crsmin */
	uint32	ncal_req_ts_ms;		/* Time-stamp when noise cal was requested */
	uint32	ncal_intr_ts_ms;	/* Time-stamp when noise cal was completed */
	uint16	ncal_m2_status_shm;	/* iqest hw status indication */
	uint16	ncal_m2_trig_shm;	/* number of iqest triggers */
	uint16	ncal_m2_failcnt_shm;	/* accumulated fail counts during iqest */
	uint16	ncal_m2_rstcca_shm;	/* number of reset cca during iqest */
	uint16	ncal_m2_rstcca_fw;	/* number of reset cca during iqest from fw */
	uint16	ncal_iqest_to_cnt;	/* count of IQ_Est time out */
	uint16	ncal_interrupt_cleared_cnt;	/* interrupt cleared on channel change */
	uint16	ncal_cancelled_cnt;	/* trigger cancelled on channel change */
	uint16	ncal_mmt_overdue;	/* Count up if ucode noise mmt is overdue for 5 sec */
	uint8	ncal_cfg_mode;		/* Configured noisecal mode */
	uint8	ncal_last_mode;		/* Last used noisecal mode */
	uint16	ncal_timeout_ms;	/* noisecal timeout */
	uint16	ncal_crs_pwrind_status_shm;

	/* Calibration */
	uint32	cal_last_time_s;	/* Last cal execution time */
	int16	cal_last_temp;
	uint8	cal_phase_id;		/* Current Multi phase cal ID */
	uint8	cal_reason;		/* reason for the cal */
	uint8	cal_suppressed_ed_cnt;	/* counter including ss, mp cals, MSB is current state */
	uint8	cal_suppressed_phymute_cnt;	/* Cal suppressed counter due to PHY MUTE */
	uint8	cal_suppressed_fullcal_cnt;	/* Cal suppressed counter due to no Full cal */
	uint8	cal_suppressed_reason01_cnt;	/* Cal suppressed counter due to reason 1 */
	uint16	cal_suppressed_mphase_cnt;	/* Cal suppressed counter due to SW */
	uint16	cal_suppressed_uctxpuoff_cnt;
	uint8	cal_suppressed_bitmap;		/* Cal suppressed bitmap */
	uint8   cal_mphase_retry_cnt;		/* mphase cal retry count */
	uint8	cal_phy_disable_pending;	/* b0: set if calibration is disabled
						* b6: cal pending
						* b7: fbc cal pending
						*/
	int8	cal_ctsdur_ms;
	uint32	cal_fullphy_cnt;	/* count of performing single phase cal */
	uint32	cal_multiphase_cnt;	/* count of performing multi-phase cal */
	uint16	cal_txiq_max_retry_cnt; /* txiqlocal retries reached max allowed count */
	uint16	cal_txiq_max_slope_cnt; /* txiqlocal slope reached max allowed count */
	uint32	cal_vcopll_failure_cnt; /* Number of VCO cal failures including */
					/* failures detected in ucode */
	uint32	cal_phy_chansw_cb_cnt;	/* # of times cal done from callback */
	uint16	cal_mppc_failed_cnt;	/* MPPC cal failure count */
	uint8	cal_pll_lockstatus;	/* PLL Lock status */

	/* RC Calibration */
	uint8	rccal_lpf_tmout;
	uint8	rccal_tia_tmout;
	uint8	rccal_rxpll_tmout;
	uint8	rccal_btadcfb_tmout;
	uint8	rccal_btadcres_tmout;
	uint8	rccal_btbpf_tmout;

	/* DCCal */
	uint8	dccal_err;		/* dccal health check error status */
	uint16	dccal_attempt_cnt;	/* Number of DC cal attempts */
	uint16	dccal_fail_cnt;		/* Number of DC cal failures */
	uint16	dccal_hcfail_cnt;	/* dcc health check failure count */
	uint16	dccal_calfail_cnt;	/* dcc failure count */
	uint8	dccal_dcoe_retry_cnt;	/* number of retries on dcoe cal */
	uint8	dccal_idacc_retry_cnt;	/* number of retries on idac cal */
	uint8	dccal_phyrxchain;	/* phy rxchain during dc calibration */
	uint8	dccal_type;		/* DC cal type: single/multi phase, chan change, etc. */
	uint8	dccal_dig_gain;
	uint8	dccal_dcest_overflow_cnt;
	uint16	dccal_dcest_overflow_gainidx;

	uint16	debug_cmn_01;

	/* WDG */
	uint32	phywdg_ts_us;		/* Time-stamp when wd was fired */
	uint32	phywdg_dur_us;		/* Duration of the watchdog */
	uint32	phywdg_override_cnt;

	/* LTE Coex */
	uint16	ltecx_t4_inhibit_dur;	/* Accumulated tx blanking duration */
	uint16	ltecx_t4_current;	/* 1 blanking, 0 no blanking */
	uint16	ltecx_t4_timeout;	/* Timeout value for wlan to resume transmission */
	uint16	ltecx_t4_none_zero_cnt;	/* Total number of blanking requests */
	uint16	ltecx_t4_timeout_cnt;	/* Accumulated number of events that wlan resume
					* transmission due to timeout timer expires. This time
					* out event usually happens when the celluar does not
					* send us tx blanking OFF message
					*/

	/* BT Coex */
	uint16	btcx_ovrd_dur_ms;	/* Cumulative btcx overide between WDGs */
	uint16	btcx_ovrd_err_cnt;	/* BTCX override flagged errors */
	int8	btcx_mode_phy_desense_level;	/* btcoex desense mode */

	/* Temp/Vbat */
	int8	temp_chip;		/* Chip temparature */
	int8	temp_fem;		/* Fem temparature */
	uint8	temp_hc_fail_cnt;
	uint16	temp_fem_read_fail_cnt;	/* Fem temparature read fail counter */
	uint16	temp_invalid_cnt;	/* Count no. of invalid temp. measurements */
	uint8	temp_chip_retry_cnt;
	uint8	temp_fem_retry_cnt;
	uint32	temp_sense_cnt;

	/* Chanspec Set */
	uint32	cst_set_ts_us;		/* Time-stamp when chanspec was set */
	uint32	cst_full_us;		/* Full channel switch time */
	uint32	cst_phy_us;		/* Channel switch time in PHY */
	uint32	cst_phy_btcxovrd_ms;	/* Channel switch time in PHY for BTCX override */

	/* Desense */
	uint32	interference_mode;	/* interference mitigation mode */
	uint32	desense_reason;		/* desense parameters to indicate reasons
					* for bphy and ofdm_desense
					*/
	uint8	ofdm_desense;		/* OFDM desense */
	uint8	bphy_desense;		/* BPHY desense */
	uint8	gbd_ofdm_sleep_cnt;	/* gbd sleep counter */
	uint8	gbd_bphy_sleep_cnt;	/* gbd sleep counter */
	uint8	gbd_ofdm_desense;	/* gbd ofdm desense level */
	uint8	gbd_bphy_desense;	/* gbd bphy desense level */
	uint8	curr_desense_elna_bypass;	/* init gain desense: elna bypass */
	uint8	curr_desense_tia_idx;	/* init gain desense: tia index */
	uint8	curr_desense_lpf_idx;	/* init gain desense: lpf index */

	uint8	debug_cmn_02;

	/* PHY state */
	uint16	pktproctimeoutstatus_reg;
	uint16	pktprocdebug_reg;
	uint16	pktprocdebug2_reg;
	uint16	deaf_cnt;		/* Depth of stay_in_carrier_search function */
	uint16	preempt_status2_reg;	/* status of preemption */
	uint16	rstcca_fw_cnt;

	/* RFEM */
	uint16	rfem_state2g;			/* rFEM state register for 2g */
	uint16	rfem_state5g;			/* rFEM state register for 5g */
	uint8	rfem_rxmode_curr_hwstate;
	uint8   rfem_rxmode_bands_req;		/* mode as requested by SW layer */
	uint8   rfem_rxmode_bands_applied;	/* mode currently configured in HW */

	/* BSSColor */
	uint8	bsscolor;		/* bsscolor value from 0 to 63 */
	uint8	bsscolor_parse_ctrl_reg;
	uint8	bsscolor_idx0_reg;
	uint32	bsscolor_miss_cnt;	/* for bss color mismatch cases */
	uint16	bsscolor_change_cnt;
	uint8	bsscolor_idx1_reg;

	/* Misc. Features */
	uint8	tof_active_cnt;
	uint32	tx_prohibited_cnt;
	uint32	pmdur_ms;
	uint32	ocl_disable_reqs;	/* OCL disable bitmap */
	uint8	ocl_en_status;		/* OCL requested state and OCL HW state */
	uint8	lpc_offset_hdb;		/* Half dB power backoff value */
	uint16	nap_disable_reqs;	/* NAP disable bitmap */

	/* TVPM and DSA */
	uint16	tvpm_txdc;
	uint8	tvpm_mitigation;	/* Bitmap of enabled and which mitigation active */
	uint8	tvpm_dynamic_rate;	/* dynamic update monitor rate */
	uint8	dsa_mode;
	uint8	dsa_state;
	uint8	dsa_boost;
	uint8	dsa_util[6u];		/* Size subject to change */

	/* Per PA 0-2 read words 0, 2, and 5/6 */
	uint8	 pa_mode_reg;
	uint16	phyctl_w025_pa_shm[PA_MODE_SIZE][PHYCTL_WORDS];	/* PHYCTL Word-0 with PA 0
							 * PHYCTL Word-2 with PA 0
							 * PHYCTL Word-5/6 with PA 0
							 * Bit[7:0] Core0 PPR, Bit[15:8] Core1 PPR
							 */

	/* TIA ADC */
	uint8	tiadccal_i_retry;
	uint8	tiadccal_q_retry;

	/* Preamble Puncture */
	uint16	pp;				/* Puncture pattern */
	uint16	pp_80;
	uint16	pp_160;
	uint16	pp_bw;

	/* TPC */
	uint16	tpc_tx_baseidxchk_dis_shm;
	uint32	power_mode;		/* LP/VLP logging */
	uint32	txcap_reason;
	uint16	tpc_olpcwar_shm;
	uint16	tpc_txbaseidx0_shm;
	uint16	tpc_txbaseidx127_shm;
	uint16	txpwr_recalc_reasons;		/* Reasons bitmap for triggered Tx pwr recalc */
	bool	hwpwrctrlen_reg;	/* tx hwpwrctrl enable */

	uint8	debug_cmn_03;

	/* Sequence Numbers */
	uint16	seq_num_rx;	/* Sequence num of last received packet */
	uint32	seq_num_bcn;	/* Sequence num of last beacon */
	uint16	seq_num_tx;	/* Sequence num of last transmitter packet */

	/* Misc general purpose debug counters (will be used for future debugging) */
	uint16	debug_cmn_04;
	uint32	debug_cmn_05;
	uint32	debug_cmn_06;
} phy_periodic_log_cmn_v20_t;

// Trunk version
typedef struct phy_periodic_log_cmn_v255 {
	/* High level info */
	uint16	chanspec;		/* Current phy chanspec */
	uint8	channel_active;		/* Channel active status */
	uint8	slice;
	uint16	log_event_id;		/* logging event id */
	uint16	ap_chanspec_bss;
	uint32	measurehold;		/* PHY hold activities */
	uint32	featureflag;		/* Currently active feature flags */
	int8	weakest_rssi;		/* weakest link RSSI */

	/* SW Config */
	uint8	amtbitmap;		/* AMT status bitamp */
	uint8	rxchain;		/* Rx Chain */
	uint8	txchain;		/* Tx Chain */
	uint32	txratespec;		/* Current Tx rate spec */
	uint32	rxratespec;		/* Current rx rate spec */
	uint32	scb_flags;
	uint32	scb_flags2;
	uint32	scb_flags3;

	/* CCA1 */
	uint32	congest_duration_ms;	/* millisecs spent sampling this channel */
	uint32	congest_meonly_ms;	/* microsecs in my own traffic (TX + RX) */
	uint32	congest_ibss_ms;	/* millisecs in our bss (presumably this traffic will */
					/*  move if cur bss moves channels) */
	uint32	congest_obss_ms;	/* traffic not in our bss */
	uint32	congest_interference_ms; /* millisecs detecting a non 802.11 interferer. */
	uint32	congest_ed_duration_ms;	/* ccastats: ed_duration */

	/* ED */
	uint16	ed_crs_status;		/* Status of ED and CRS during noise cal */
	uint16	ed_rspfrm_txncl_cnt_shm;	/* Response frame not sent due to ED */

	/* glitch based desense input from cca */
	uint32	cca_stats_total_glitch;
	uint32	cca_stats_bphy_glitch;
	uint32	cca_stats_total_badplcp;
	uint32	cca_stats_bphy_badplcp;
	uint32	cca_stats_mbsstime;

	/* MAC Susp */
	uint32	macsusp_dur_us;		/* mac suspend duration */
	uint8	macsusp_cnt;		/* mac suspend counter */
	uint8	macsusp_phy_cnt;

	/* Ucode State */
	uint16	prewds_cnt_shm;		/* Count of pre-wds fired in the ucode */
	uint32	tsf_l_us;		/* Ucode TSF_ML/L */
	uint16	prewds_rxe_status1_shm;
	uint16	prewds_rxe_status2_shm;
	uint16	prewds_phyfifo_wd_cnt_shm;
	uint16	prewds_txcrsstk_macwar_cnt_shm;
	uint16	prewds_txcrsstk_phywar_cnt_shm;
	uint16	prewds_txsm_not_clean_cnt_shm;
	uint16	tx_preempt_btcx_wlan_cnt_shm;
	uint16	tx_preempt_uwbcx_cnt_shm;
	uint16	tx_preempt_rc1cx_wlan_cnt_shm;
	uint16	txpustatus_shm;		/* txpu off definations */
	uint16	tdmtx_txpuen_shm;
	uint16	tdmtx_txpudis_shm;
	uint16	tdmtx_txmute_shm;
	uint16	hpp_mpif_cnt_shm;
	uint16	hpp_thld_cnt_shm;
	uint16	hpp_cts_cnt_shm;

	/* NAV */
	uint32	navstat_acc_shm;
	uint16	navstat_max_shm_us;

	/* Noise */
	uint16	ncal_crsmin_pwr_apply_cnt;	/* Cnt of desense power threshold update to phy */
	uint32	ncal_request_cnt;	/* count of noisecal request */
	uint32	ncal_crsbit_cnt;	/* count of crs high during noisecal request */
	uint32	ncal_apply_cnt;		/* count of applying noisecal result to crsmin */
	uint32	ncal_req_ts_ms;		/* Time-stamp when noise cal was requested */
	uint32	ncal_intr_ts_ms;	/* Time-stamp when noise cal was completed */
	uint16	ncal_m2_status_shm;	/* iqest hw status indication */
	uint16	ncal_m2_trig_shm;	/* number of iqest triggers */
	uint16	ncal_m2_failcnt_shm;	/* accumulated fail counts during iqest */
	uint16	ncal_m2_rstcca_shm;	/* number of reset cca during iqest */
	uint16	ncal_m2_rstcca_fw;	/* number of reset cca during iqest from fw */
	uint16	ncal_iqest_to_cnt;	/* count of IQ_Est time out */
	uint16	ncal_interrupt_cleared_cnt;	/* interrupt cleared on channel change */
	uint16	ncal_cancelled_cnt;	/* trigger cancelled on channel change */
	uint16	ncal_mmt_overdue;	/* Count up if ucode noise mmt is overdue for 5 sec */
	uint8	ncal_cfg_mode;		/* Configured noisecal mode */
	uint8	ncal_last_mode;		/* Last used noisecal mode */
	uint16	ncal_timeout_ms;	/* noisecal timeout */
	uint16	ncal_crs_pwrind_status_shm;

	/* Calibration */
	uint32	cal_last_time_s;	/* Last cal execution time */
	int16	cal_last_temp;
	uint8	cal_phase_id;		/* Current Multi phase cal ID */
	uint8	cal_reason;		/* reason for the cal */
	uint8	cal_suppressed_ed_cnt;	/* counter including ss, mp cals, MSB is current state */
	uint8	cal_suppressed_phymute_cnt;	/* Cal suppressed counter due to PHY MUTE */
	uint8	cal_suppressed_fullcal_cnt;	/* Cal suppressed counter due to no Full cal */
	uint8	cal_suppressed_reason01_cnt;	/* Cal suppressed counter due to reason 1 */
	uint16	cal_suppressed_mphase_cnt;	/* Cal suppressed counter due to SW */
	uint16	cal_suppressed_uctxpuoff_cnt;
	uint8	cal_suppressed_bitmap;		/* Cal suppressed bitmap */
	uint8   cal_mphase_retry_cnt;	/* mphase cal retry count */
	uint8	cal_phy_disable_pending;	/* b0: set if calibration is disabled
						* b6: cal pending
						* b7: fbc cal pending
						*/
	int8	cal_ctsdur_ms;
	uint32	cal_fullphy_cnt;	/* count of performing single phase cal */
	uint32	cal_multiphase_cnt;	/* count of performing multi-phase cal */
	uint16	cal_txiq_max_retry_cnt; /* txiqlocal retries reached max allowed count */
	uint16	cal_txiq_max_slope_cnt; /* txiqlocal slope reached max allowed count */
	uint32	cal_vcopll_failure_cnt; /* Number of VCO cal failures including */
					/* failures detected in ucode */
	uint32	cal_phy_chansw_cb_cnt;	/* # of times cal done from callback */
	uint16	cal_mppc_failed_cnt;	/* MPPC cal failure count */
	uint8	cal_pll_lockstatus;	/* PLL Lock status */

	/* RC Calibration */
	uint8	rccal_lpf_tmout;
	uint8	rccal_tia_tmout;
	uint8	rccal_rxpll_tmout;
	uint8	rccal_btadcfb_tmout;
	uint8	rccal_btadcres_tmout;
	uint8	rccal_btbpf_tmout;

	/* DCCal */
	uint8	dccal_err;		/* dccal health check error status */
	uint16	dccal_attempt_cnt;	/* Number of DC cal attempts */
	uint16	dccal_fail_cnt;		/* Number of DC cal failures */
	uint16	dccal_hcfail_cnt;	/* dcc health check failure count */
	uint16	dccal_calfail_cnt;	/* dcc failure count */
	uint8	dccal_dcoe_retry_cnt;	/* number of retries on dcoe cal */
	uint8	dccal_idacc_retry_cnt;	/* number of retries on idac cal */
	uint8	dccal_phyrxchain;	/* phy rxchain during dc calibration */
	uint8	dccal_type;		/* DC cal type: single/multi phase, chan change, etc. */
	uint8	dccal_dig_gain;
	uint8	dccal_dcest_overflow_cnt;
	uint16	dccal_dcest_overflow_gainidx;

	uint16	debug_cmn_01;

	/* WDG */
	uint32	phywdg_ts_us;		/* Time-stamp when wd was fired */
	uint32	phywdg_dur_us;		/* Duration of the watchdog */
	uint32	phywdg_override_cnt;

	/* LTE Coex */
	uint16	ltecx_t4_inhibit_dur;	/* Accumulated tx blanking duration */
	uint16	ltecx_t4_current;	/* 1 blanking, 0 no blanking */
	uint16	ltecx_t4_timeout;	/* Timeout value for wlan to resume transmission */
	uint16	ltecx_t4_msg12_cnt;	/* Times inhibit was requested */
	uint16	ltecx_t4_timeout_cnt;	/* Accumulated number of events that wlan resume
					* transmission due to timeout timer expires. This time
					* out event usually happens when the celluar does not
					* send us tx blanking OFF message
					*/

	/* BT Coex */
	uint16	btcx_ovrd_dur_ms;	/* Cumulative btcx overide between WDGs */
	uint16	btcx_ovrd_err_cnt;	/* BTCX override flagged errors */
	int8	btcx_mode_phy_desense_level;	/* btcoex desense mode */

	/* Temp/Vbat */
	int8	temp_chip;		/* Chip temparature */
	int8	temp_fem;		/* Fem temparature */
	uint8	temp_hc_fail_cnt;
	uint16	temp_fem_read_fail_cnt;	/* Fem temparature read fail counter */
	uint16	temp_invalid_cnt;	/* Count no. of invalid temp. measurements */
	uint8	temp_chip_retry_cnt;
	uint8	temp_fem_retry_cnt;
	uint32	temp_sense_cnt;

	/* Chanspec Set */
	uint32	cst_set_ts_us;		/* Time-stamp when chanspec was set */
	uint32	cst_full_us;		/* Full channel switch time */
	uint32	cst_phy_us;		/* Channel switch time in PHY */
	uint32	cst_phy_btcxovrd_ms;	/* Channel switch time in PHY for BTCX override */

	/* Desense */
	uint32	interference_mode;	/* interference mitigation mode */
	uint32	desense_reason;		/* desense parameters to indicate reasons
					* for bphy and ofdm_desense
					*/
	uint8	ofdm_desense;		/* OFDM desense */
	uint8	bphy_desense;		/* BPHY desense */
	uint8	gbd_ofdm_sleep_cnt;	/* gbd sleep counter */
	uint8	gbd_bphy_sleep_cnt;	/* gbd sleep counter */
	uint8	gbd_ofdm_desense;	/* gbd ofdm desense level */
	uint8	gbd_bphy_desense;	/* gbd bphy desense level */
	uint8	curr_desense_elna_bypass;	/* init gain desense: elna bypass */
	uint8	curr_desense_tia_idx;	/* init gain desense: tia index */
	uint8	curr_desense_lpf_idx;	/* init gain desense: lpf index */

	uint8	debug_cmn_02;

	/* PHY state */
	uint16	pktproctimeoutstatus_reg;
	uint16	pktprocdebug_reg;
	uint16	pktprocdebug2_reg;
	uint16	deaf_cnt;		/* Depth of stay_in_carrier_search function */
	uint16	preempt_status2_reg;	/* status of preemption */
	uint16	rstcca_fw_cnt;

	/* RFEM */
	uint16	rfem_state2g;			/* rFEM state register for 2g */
	uint16	rfem_state5g;			/* rFEM state register for 5g */
	uint8	rfem_rxmode_curr_hwstate;
	uint8   rfem_rxmode_bands_req;		/* mode as requested by SW layer */
	uint8   rfem_rxmode_bands_applied;	/* mode currently configured in HW */

	/* BSSColor */
	uint8	bsscolor;		/* bsscolor value from 0 to 63 */
	uint8	bsscolor_parse_ctrl_reg;
	uint8	bsscolor_idx0_reg;
	uint32	bsscolor_miss_cnt;	/* for bss color mismatch cases */
	uint16	bsscolor_change_cnt;
	uint8	bsscolor_idx1_reg;

	/* Misc. Features */
	uint8	tof_active_cnt;
	uint32	tx_prohibited_cnt;
	uint32	pmdur_ms;
	uint32	ocl_disable_reqs;	/* OCL disable bitmap */
	uint8	ocl_en_status;		/* OCL requested state and OCL HW state */
	uint8	lpc_offset_hdb;		/* Half dB power backoff value */
	uint16	nap_disable_reqs;	/* NAP disable bitmap */

	/* TVPM and DSA */
	uint16	tvpm_txdc;
	uint8	tvpm_mitigation;	/* Bitmap of enabled and which mitigation active */
	uint8	tvpm_dynamic_rate;	/* dynamic update monitor rate */
	uint8	dsa_mode;
	uint8	dsa_state;
	uint8	dsa_boost;
	uint8	dsa_util[6u];		/* Size subject to change */

	/* Per PA 0-2 read words 0, 2, and 5/6 */
	uint8	 pa_mode_reg;
	uint16	phyctl_w025_pa_shm[PA_MODE_SIZE][PHYCTL_WORDS];	/* PHYCTL Word-0 with PA 0
							 * PHYCTL Word-2 with PA 0
							 * PHYCTL Word-5/6 with PA 0
							 * Bit[7:0] Core0 PPR, Bit[15:8] Core1 PPR
							 */

	/* TIA ADC */
	uint8	tiadccal_i_retry;
	uint8	tiadccal_q_retry;

	/* Preamble Puncture */
	uint16	pp;				/* Puncture pattern */
	uint16	pp_80;
	uint16	pp_160;
	uint16	pp_bw;

	/* TPC */
	uint16	tpc_tx_baseidxchk_dis_shm;
	uint32	power_mode;		/* LP/VLP logging */
	uint32	txcap_reason;
	uint16	tpc_olpcwar_shm;
	uint16	tpc_txbaseidx0_shm;
	uint16	tpc_txbaseidx127_shm;
	uint16	txpwr_recalc_reasons;	/* Reasons bitmap for triggered Tx pwr recalc */
	bool	hwpwrctrlen_reg;	/* tx hwpwrctrl enable */

	uint8	debug_cmn_03;

	/* Sequence Numbers */
	uint16	seq_num_rx;	/* Sequence num of last received packet */
	uint32	seq_num_bcn;	/* Sequence num of last beacon */
	uint16	seq_num_tx;	/* Sequence num of last transmitter packet */

	/* LESI */
	int8	rxsense_noise_idx;	/* rxsense detection threshold desense index */
	int8	rxsense_offset;		/* rxsense min power desense index */
	uint32	rxsense_disable_req_ch;	/* channel disable requests */

	/* LTECX */
	uint16	rxgpr0_cnt_ltestschange;
	uint8	mws_coex_cellst_prev_currltests;	/* previous cellstatus */
	int8	ltecx_mode;

	/* SWDiv */
	uint8	active_ant;			/* Lower byte */
	uint8	swap_trig_event_id;		/* Lower byte */
	uint8	swap_trig_event_id_prev;	/* Lower byte */

	/* Misc general purpose debug counters (will be used for future debugging) */
	uint8	debug_cmn_04;
	uint32	debug_cmn_05;
	uint32	debug_cmn_06;
} phy_periodic_log_cmn_v255_t;

typedef struct phy_periodic_log_core {
	uint8	baseindxval; /* TPC Base index */
	int8	tgt_pwr; /* Programmed Target power */
	int8	estpwradj; /* Current Est Power Adjust value */
	int8	crsmin_pwr; /* CRS Min/Noise power */
	int8	rssi_per_ant; /* RSSI Per antenna */
	int8	snr_per_ant; /* SNR Per antenna */
	int8	pad1; /* Padding byte to align with word */
	int8	pad2; /* Padding byte to align with word */
} phy_periodic_log_core_t;

typedef struct phy_periodic_log_core_v3 {
	uint8	baseindxval; /* TPC Base index */
	int8	tgt_pwr; /* Programmed Target power */
	int8	estpwradj; /* Current Est Power Adjust value */
	int8	crsmin_pwr; /* CRS Min/Noise power */
	int8	rssi_per_ant; /* RSSI Per antenna */
	int8	snr_per_ant; /* SNR Per antenna */

	/* dccal dcoe & idacc */
	uint16	dcoe_done_0;	/* dccal control register 44 */
	uint16	dcoe_done_1;	/* dccal control register 45 */
	uint16	dcoe_done_2;	/* dccal control register 46 */
	uint16	idacc_done_0;	/* dccal control register 21 */
	uint16	idacc_done_1;	/* dccal control register 60 */
	uint16	idacc_done_2;	/* dccal control register 61 */
	int16	psb;		/* psb read during dccal health check */
	uint8	pktproc;	/* pktproc read during dccal health check */

	int8	noise_level;	/* noise level = crsmin_pwr if gdb desense is lesser */
	int8	pad2; /* Padding byte to align with word */
	int8	pad3; /* Padding byte to align with word */
} phy_periodic_log_core_v3_t;

typedef struct phy_periodic_log_core_v4 {
	/* dccal dcoe & idacc */
	uint16	dcoe_done_0;	/* dccal control register 44 */
	uint16	dcoe_done_1;	/* dccal control register 45 */
	uint16	dcoe_done_2;	/* dccal control register 46 */
	uint16	idacc_done_0;	/* dccal control register 21 */
	uint16	idacc_done_1;	/* dccal control register 60 */
	uint16	idacc_done_2;	/* dccal control register 61 */
	int16	psb;		/* psb read during dccal health check */
	int16	txcap;		/* Txcap value */

	uint16	debug_01;	/* multipurpose debug register */
	uint16	debug_02;	/* multipurpose debug register */
	uint16	debug_03;	/* multipurpose debug register */
	uint16	debug_04;	/* multipurpose debug register */

	uint8	pktproc;	/* pktproc read during dccal health check */
	uint8	baseindxval;	/* TPC Base index */
	int8	tgt_pwr;	/* Programmed Target power */
	int8	estpwradj;	/* Current Est Power Adjust value */
	int8	crsmin_pwr;		/* CRS Min/Noise power */
	int8	rssi_per_ant;	/* RSSI Per antenna */
	int8	snr_per_ant;	/* SNR Per antenna */

	int8	noise_level;	/* noise pwr after filtering & averageing */
	int8	noise_level_inst;	/* instantaneous noise cal pwr */
	int8	estpwr;		/* tx powerDet value */
} phy_periodic_log_core_v4_t;

typedef struct phy_periodic_log_core_v5 {
	uint8	baseindxval;			/* TPC Base index */
	int8	tgt_pwr;			/* Programmed Target power */
	int8	estpwradj;			/* Current Est Power Adjust value */
	int8	crsmin_pwr;			/* CRS Min/Noise power */
	int8	rssi_per_ant;			/* RSSI Per antenna */
	int8	snr_per_ant;			/* SNR Per antenna */
	uint8	fp;
	int8	phylog_noise_pwr_array[8];	/* noise buffer array */
	int8	noise_dbm_ant;			/* from uCode shm read, afer converting to dBm */
} phy_periodic_log_core_v5_t;

#define PHY_NOISE_PWR_ARRAY_SIZE	(8u)
typedef struct phy_periodic_log_core_v6 {
	/* dccal dcoe & idacc */
	uint16	dcoe_done_0;	/* dccal control register 44 */
	uint16	dcoe_done_1;	/* dccal control register 45 */
	uint16	dcoe_done_2;	/* dccal control register 46 */
	uint16	idacc_done_0;	/* dccal control register 21 */
	uint16	idacc_done_1;	/* dccal control register 60 */
	uint16	idacc_done_2;	/* dccal control register 61 */
	int16	psb;		/* psb read during dccal health check */
	int16	txcap;		/* Txcap value */

	uint16	curr_tssival;	/* TxPwrCtrlInit_path[01].TSSIVal */
	uint16	pwridx_init;	/* TxPwrCtrlInit_path[01].pwrIndex_init_path[01] */
	uint16	tpc_hc_tssi;	/* RAM A HC TSSI value */
	uint16	btcx_antmask;	/* antenna to be used by BT */

	uint8	pktproc;	/* pktproc read during dccal health check */
	uint8	baseindxval;	/* TPC Base index */
	int8	tgt_pwr;	/* Programmed Target power */
	int8	estpwradj;	/* Current Est Power Adjust value */
	int8	crsmin_pwr;		/* CRS Min/Noise power */
	int8	rssi_per_ant;	/* RSSI Per antenna */
	int8	snr_per_ant;	/* SNR Per antenna */

	int8	noise_level;	/* noise pwr after filtering & averageing */
	int8	noise_level_inst;	/* instantaneous noise cal pwr */
	int8	estpwr;		/* tx powerDet value */
	int8	crsmin_th_idx;	/* idx used to lookup crs min thresholds */
	int8	tpc_hc_bidx;	/*  RAM A HC base index */

	int8	phy_noise_pwr_array[PHY_NOISE_PWR_ARRAY_SIZE];	/* noise buffer array */
} phy_periodic_log_core_v6_t;

typedef struct phy_periodic_log_core_v7 {
	/* dccal dcoe & idacc */
	uint16	dcoe_done_0;	/* dccal control register 44 */
	uint16	dcoe_done_1;	/* dccal control register 45 */
	uint16	dcoe_done_2;	/* dccal control register 46 */
	uint16	idacc_done_0;	/* dccal control register 21 */
	uint16	idacc_done_1;	/* dccal control register 60 */
	uint16	idacc_done_2;	/* dccal control register 61 */
	int16	psb;		/* psb read during dccal health check */
	int16	txcap;		/* Txcap value */

	/* Misc general purpose debug counters (will be used for future debugging) */
	uint16	bad_txbaseidx_cnt;	/* cntr for tx_baseidx=127 in healthcheck */
	uint16	debug_02;
	uint16	debug_03;
	uint16	debug_04;

	uint8	pktproc;	/* pktproc read during dccal health check */
	uint8	baseindxval;	/* TPC Base index */
	int8	tgt_pwr;	/* Programmed Target power */
	int8	estpwradj;	/* Current Est Power Adjust value */
	int8	crsmin_pwr;		/* CRS Min/Noise power */
	int8	rssi_per_ant;	/* RSSI Per antenna */
	int8	snr_per_ant;	/* SNR Per antenna */

	int8	noise_level;	/* noise pwr after filtering & averageing */
	int8	noise_level_inst;	/* instantaneous noise cal pwr */
	int8	estpwr;		/* tx powerDet value */
	int8	crsmin_th_idx;	/* idx used to lookup crs min thresholds */
	uint8	pad01;

	uint16	curr_tssival;	/* TxPwrCtrlInit_path[01].TSSIVal */
	uint16	pwridx_init;	/* TxPwrCtrlInit_path[01].pwrIndex_init_path[01] */
	uint16	auxphystats;
	uint16	phystatsgaininfo;
	uint16	flexpwrAFE;
	uint16	flexpwrdig1;
	uint16	flexpwrdig2;
	uint16	flexpwrdig3;
	uint16	flexpwrdig4;
	uint16	flexgaininfo_A;

	/* Misc general purpose debug counters (will be used for future debugging) */
	uint8	debug_05;
	uint8	debug_06;
	uint8	debug_07;
	uint8	debug_08;

	int8	phy_noise_pwr_array[PHY_NOISE_PWR_ARRAY_SIZE];	/* noise buffer array */
} phy_periodic_log_core_v7_t;

typedef struct phy_periodic_log_core_v8 {
	/* dccal dcoe & idacc */
	uint16	dcoe_done_0;	/* dccal control register 44 */
	uint16	dcoe_done_1;	/* dccal control register 45 */
	uint16	dcoe_done_2;	/* dccal control register 46 */
	uint16	idacc_done_0;	/* dccal control register 21 */
	uint16	idacc_done_1;	/* dccal control register 60 */
	uint16	idacc_done_2;	/* dccal control register 61 */
	int16	psb;		/* psb read during dccal health check */
	int16	txcap;		/* Txcap value */

	/* Misc general purpose debug counters (will be used for future debugging) */
	uint16	debug_01;
	uint16	debug_02;
	uint16	debug_03;
	uint16	debug_04;

	uint8	pktproc;	/* pktproc read during dccal health check */
	uint8	baseindxval;	/* TPC Base index */
	int8	tgt_pwr;	/* Programmed Target power */
	int8	estpwradj;	/* Current Est Power Adjust value */
	int8	crsmin_pwr;		/* CRS Min/Noise power */
	int8	rssi_per_ant;	/* RSSI Per antenna */
	int8	snr_per_ant;	/* SNR Per antenna */

	int8	noise_level;	/* noise pwr after filtering & averageing */
	int8	noise_level_inst;	/* instantaneous noise cal pwr */
	int8	estpwr;		/* tx powerDet value */
	int8	crsmin_th_idx;	/* idx used to lookup crs min thresholds */

	int8	ed_threshold;	/* ed threshold */
	uint16	ed20_crs;	/* ED-CRS status */

	uint16	curr_tssival;	/* TxPwrCtrlInit_path[01].TSSIVal */
	uint16	pwridx_init;	/* TxPwrCtrlInit_path[01].pwrIndex_init_path[01] */
	uint16	auxphystats;
	uint16	phystatsgaininfo;
	uint16	flexpwrAFE;
	uint16	flexpwrdig1;
	uint16	flexpwrdig2;
	uint16	flexpwrdig3;
	uint16	flexpwrdig4;
	uint16	flexgaininfo_A;

	uint16	bad_txbaseidx_cnt;	/* cntr for tx_baseidx=127 in healthcheck */
	uint16	tpc_vmid;
	uint16	debug_05;	/* Misc general purpose debug counters */
	uint8	tpc_av;

	/* Misc general purpose debug counters (will be used for future debugging) */
	uint8	debug_06;
	uint8	debug_07;
	uint8	debug_08;

	int8	phy_noise_pwr_array[PHY_NOISE_PWR_ARRAY_SIZE];	/* noise buffer array */
} phy_periodic_log_core_v8_t;

typedef struct phy_periodic_log_core_v9 {
	/* dccal dcoe & idacc */
	uint16	dcoe_done_0;	/* dccal control register 44 */
	uint16	dcoe_done_1;	/* dccal control register 45 */
	uint16	dcoe_done_2;	/* dccal control register 46 */
	uint16	idacc_done_0;	/* dccal control register 21 */
	uint16	idacc_done_1;	/* dccal control register 60 */
	uint16	idacc_done_2;	/* dccal control register 61 */
	int16	psb;		/* psb read during dccal health check */
	int16	txcap;		/* Txcap value */

	uint8	pktproc;	/* pktproc read during dccal health check */
	uint8	baseindxval;	/* TPC Base index */
	int8	tgt_pwr;	/* Programmed Target power */
	int8	estpwradj;	/* Current Est Power Adjust value */
	int8	crsmin_pwr;		/* CRS Min/Noise power */
	int8	rssi_per_ant;	/* RSSI Per antenna */
	int8	snr_per_ant;	/* SNR Per antenna */

	int8	noise_level;	/* noise pwr after filtering & averageing */
	int8	noise_level_inst;	/* instantaneous noise cal pwr */
	int8	estpwr;		/* tx powerDet value */
	int8	crsmin_th_idx;	/* idx used to lookup crs min thresholds */

	int8	ed_threshold;	/* ed threshold */
	uint16	ed20_crs;	/* ED-CRS status */

	uint16	curr_tssival;	/* TxPwrCtrlInit_path[01].TSSIVal */
	uint16	pwridx_init;	/* TxPwrCtrlInit_path[01].pwrIndex_init_path[01] */

	uint16	auxphystats;
	uint16	phystatsgaininfo;
	uint16	flexpwrAFE;
	uint16	flexpwrdig1;
	uint16	flexpwrdig2;
	uint16	flexpwrdig3;
	uint16	flexpwrdig4;
	uint16	flexgaininfo_A;

	uint16	bad_txbaseidx_cnt;	/* cntr for tx_baseidx=127 in healthcheck */
	uint32	rfseq_rst_ctr;		/* rfseq reset counter */
	uint16	tpc_vmid;
	uint8	tpc_av;

	/* Misc general purpose debug counters (will be used for future debugging) */
	uint8	debug_01;
	uint32	debug_02;
	uint16	debug_03;
	uint16	debug_04;

	int8	phy_noise_pwr_array[PHY_NOISE_PWR_ARRAY_SIZE];	/* noise buffer array */
} phy_periodic_log_core_v9_t;

typedef struct phy_periodic_log_core_v10 {
	/* dccal dcoe & idacc */
	uint16	dcoe_done_0;	/* dccal control register 44 */
	uint16	dcoe_done_1;	/* dccal control register 45 */
	uint16	dcoe_done_2;	/* dccal control register 46 */
	uint16	idacc_done_0;	/* dccal control register 21 */
	uint16	idacc_done_1;	/* dccal control register 60 */
	uint16	idacc_done_2;	/* dccal control register 61 */
	int16	psb;		/* psb read during dccal health check */
	int16	txcap;		/* Txcap value */

	uint8	pktproc;	/* pktproc read during dccal health check */
	int8	crsmin_pwr;		/* CRS Min/Noise power */
	int8	rssi_per_ant;	/* RSSI Per antenna */
	int8	snr_per_ant;	/* SNR Per antenna */

	int8	noise_level;	/* noise pwr after filtering & averageing */
	int8	noise_level_inst;	/* instantaneous noise cal pwr */
	int8	crsmin_th_idx;	/* idx used to lookup crs min thresholds */

	int8	ed_threshold;	/* ed threshold */
	uint16	ed20_crs;	/* ED-CRS status */

	uint16	bad_txbaseidx_cnt;	/* cntr for tx_baseidx=127 in healthcheck */

	/* PA0 parameters */
	uint8	baseindxval_pa0;	/* TPC Base index */
	int8	tgt_pwr_pa0;		/* Programmed Target power */
	int8	estpwradj_pa0;		/* Current Est Power Adjust value */
	int8	estpwr_pa0;		/* tx powerDet value */
	uint16	curr_tssival_pa0;	/* TxPwrCtrlInit_path[01].TSSIVal */
	uint16	pwridx_init_pa0;	/* TxPwrCtrlInit_path[01].pwrIndex_init_path[01] */
	/* PA1 parameters */
	uint8	baseindxval_pa1;
	int8	tgt_pwr_pa1;
	int8	estpwradj_pa1;
	int8	estpwr_pa1;
	uint16	curr_tssival_pa1;
	uint16	pwridx_init_pa1;
	/* PA2 parameters */
	uint8	baseindxval_pa2;
	int8	tgt_pwr_pa2;
	int8	estpwradj_pa2;
	int8	estpwr_pa2;
	uint16	curr_tssival_pa2;
	uint16	pwridx_init_pa2;

	uint16	auxphystats;
	uint16	phystatsgaininfo;
	uint16	flexpwrAFE;
	uint16	flexpwrdig1;
	uint16	flexpwrdig2;
	uint16	flexpwrdig3;
	uint16	flexpwrdig4;
	uint16	flexgaininfo_A;

	uint32	rfseq_rst_ctr;		/* rfseq reset counter */
	uint16	tpc_vmid;
	uint8	tpc_av;

	/* Misc general purpose debug counters (will be used for future debugging) */
	uint8	debug_01;
	uint16	debug_02;
	uint16	debug_03;
	uint32	debug_04;

	int8	phy_noise_pwr_array[PHY_NOISE_PWR_ARRAY_SIZE];	/* noise buffer array */
} phy_periodic_log_core_v10_t;

typedef struct phy_periodic_log_core_v11 {
	/* dccal dcoe & idacc */
	uint16	dcoe_done_0;	/* dccal control register 44 */
	uint16	dcoe_done_1;	/* dccal control register 45 */
	uint16	dcoe_done_2;	/* dccal control register 46 */
	uint16	idacc_done_0;	/* dccal control register 21 */
	uint16	idacc_done_1;	/* dccal control register 60 */
	uint16	idacc_done_2;	/* dccal control register 61 */
	int16	psb;		/* psb read during dccal health check */
	int16	txcap;		/* Txcap value */

	uint8	pktproc;	/* pktproc read during dccal health check */
	int8	crsmin_pwr;		/* CRS Min/Noise power */
	int8	rssi_per_ant;	/* RSSI Per antenna */
	int8	snr_per_ant;	/* SNR Per antenna */

	int8	noise_level;	/* noise pwr after filtering & averageing */
	int8	noise_level_inst;	/* instantaneous noise cal pwr */
	int8	crsmin_th_idx;	/* idx used to lookup crs min thresholds */

	int8	ed_threshold;	/* ed threshold */
	uint16	ed20_crs;	/* ED-CRS status */

	uint16	bad_txbaseidx_cnt;	/* cntr for tx_baseidx=127 in healthcheck */

	/* PA0 parameters */
	uint8	baseindxval_pa0;	/* TPC Base index */
	int8	tgt_pwr_pa0;		/* Programmed Target power */
	int8	estpwradj_pa0;		/* Current Est Power Adjust value */
	int8	estpwr_pa0;		/* tx powerDet value */
	uint16	curr_tssival_pa0;	/* TxPwrCtrlInit_path[01].TSSIVal */
	uint16	pwridx_init_pa0;	/* TxPwrCtrlInit_path[01].pwrIndex_init_path[01] */
	/* PA1 parameters */
	uint8	baseindxval_pa1;
	int8	tgt_pwr_pa1;
	int8	estpwradj_pa1;
	int8	estpwr_pa1;
	uint16	curr_tssival_pa1;
	uint16	pwridx_init_pa1;
	/* PA2 parameters */
	uint8	baseindxval_pa2;
	int8	tgt_pwr_pa2;
	int8	estpwradj_pa2;
	int8	estpwr_pa2;
	uint16	curr_tssival_pa2;
	uint16	pwridx_init_pa2;

	uint16	auxphystats;
	uint16	phystatsgaininfo;
	uint16	flexpwrAFE;
	uint16	flexpwrdig1;
	uint16	flexpwrdig2;
	uint16	flexpwrdig3;
	uint16	flexpwrdig4;
	uint16	flexgaininfo_A;

	/* SW Diversity */
	int32	avg_snr_per_ant;
	uint32	swap_txfail;
	uint32	swap_alivecheck;
	uint32	rxcount_per_ant;
	uint32	swap_snrdrop;

	uint32	rfseq_rst_ctr;		/* rfseq reset counter */
	uint16	tpc_vmid;
	uint8	tpc_av;

	/* Misc general purpose debug counters (will be used for future debugging) */
	uint8	debug_01;
	uint32	debug_02;
	uint32	debug_03;
	uint32	debug_04;

	int8	phy_noise_pwr_array[PHY_NOISE_PWR_ARRAY_SIZE];	/* noise buffer array */
} phy_periodic_log_core_v11_t;

typedef struct phy_periodic_log_core_v12 {
	/* dccal dcoe & idacc */
	uint16	dcoe_done_0;	/* dccal control register 44 */
	uint16	dcoe_done_1;	/* dccal control register 45 */
	uint16	dcoe_done_2;	/* dccal control register 46 */
	uint16	idacc_done_0;	/* dccal control register 21 */
	uint16	idacc_done_1;	/* dccal control register 60 */
	uint16	idacc_done_2;	/* dccal control register 61 */
	int16	psb;		/* psb read during dccal health check */
	int16	txcap;		/* Txcap value */

	uint8	pktproc;	/* pktproc read during dccal health check */
	int8	crsmin_pwr;		/* CRS Min/Noise power */
	int8	rssi_per_ant;	/* RSSI Per antenna */
	int8	snr_per_ant;	/* SNR Per antenna */

	int8	noise_level;	/* noise pwr after filtering & averageing */
	int8	noise_level_inst;	/* instantaneous noise cal pwr */
	int8	crsmin_th_idx;	/* idx used to lookup crs min thresholds */

	int8	ed_threshold;	/* ed threshold */
	uint16	ed20_crs;	/* ED-CRS status */

	uint16	bad_txbaseidx_cnt;	/* cntr for tx_baseidx=127 in healthcheck */

	/* PA0 parameters */
	uint8	baseindxval_pa0;	/* TPC Base index */
	int8	tgt_pwr_pa0;		/* Programmed Target power */
	int8	estpwradj_pa0;		/* Current Est Power Adjust value */
	int8	estpwr_pa0;		/* tx powerDet value */
	uint16	curr_tssival_pa0;	/* TxPwrCtrlInit_path[01].TSSIVal */
	uint16	pwridx_init_pa0;	/* TxPwrCtrlInit_path[01].pwrIndex_init_path[01] */
	/* PA1 parameters */
	uint8	baseindxval_pa1;
	int8	tgt_pwr_pa1;
	int8	estpwradj_pa1;
	int8	estpwr_pa1;
	uint16	curr_tssival_pa1;
	uint16	pwridx_init_pa1;
	/* PA2 parameters */
	uint8	baseindxval_pa2;
	int8	tgt_pwr_pa2;
	int8	estpwradj_pa2;
	int8	estpwr_pa2;
	uint16	curr_tssival_pa2;
	uint16	pwridx_init_pa2;

	uint16	auxphystats;
	uint16	phystatsgaininfo;
	uint16	flexpwrAFE;
	uint16	flexpwrdig1;
	uint16	flexpwrdig2;
	uint16	flexpwrdig3;
	uint16	flexpwrdig4;
	uint16	flexgaininfo_A;

	/* SW Diversity */
	int32	avg_snr_per_ant;
	uint32	swap_txfail;
	uint32	swap_alivecheck;
	uint32	rxcount_per_ant;
	uint32	swap_snrdrop;

	uint32	gainidx_latchout;
	uint32	adcreset_i;
	uint32	adcreset_q;

	uint16	tpc_vmid;
	int16	dcRe_aux;		/*  */
	int16	dcIm_aux;		/*  */
	uint16	iqest_mode2_gain;
	uint8	tpc_av;

	/* Misc general purpose debug counters (will be used for future debugging) */
	uint8	debug_01;
	uint16	debug_02;
	uint32	debug_03;
	uint32	debug_04;

	int8	phy_noise_pwr_array[PHY_NOISE_PWR_ARRAY_SIZE];	/* noise buffer array */
} phy_periodic_log_core_v12_t;

typedef struct phy_periodic_log_core_v13 {
	/* dccal dcoe & idacc */
	uint16	dccal_dcoe_done_0;	/* dccal control register 44 */
	uint16	dccal_dcoe_done_1;	/* dccal control register 45 */
	uint16	dccal_dcoe_done_2;	/* dccal control register 46 */
	uint16	dccal_idacc_done_0;	/* dccal control register 21 */
	uint16	dccal_idacc_done_1;	/* dccal control register 60 */
	uint16	dccal_idacc_done_2;	/* dccal control register 61 */
	int16	dccal_psb;		/* psb read during dccal health check */

	int16	dcRe_aux;
	int16	dcIm_aux;

	int16	txcap;			/* Txcap value */
	uint16	bad_txbaseidx_cnt;	/* cntr for tx_baseidx=127 in healthcheck */

	/* PA0-2 parameters */
	/* TxPwrCtrlInit_path[01].pwrIndex_init_path[01] */
	uint16	pwridx_init_pa_reg[PA_MODE_SIZE];
	uint8	baseindxval_pa_reg[PA_MODE_SIZE];	/* TPC Base index */
	int8	tgt_pwr_pa_reg[PA_MODE_SIZE];		/* Programmed Target power */
	uint16	curr_tssival_pa_reg[PA_MODE_SIZE];	/* TxPwrCtrlInit_path[01].TSSIVal */
	int8	estpwr_pa_reg[PA_MODE_SIZE];		/* tx powerDet value */
	int8	estpwradj_pa_reg[PA_MODE_SIZE];		/* Current Est Power Adjust value */

	int8	rssi_per_ant;		/* RSSI Per antenna */
	int8	snr_per_ant;		/* SNR Per antenna */

	uint16	ed20_crs_reg;		/* ED-CRS status */
	int8	ed_threshold;		/* ed threshold */

	int8	crsmin_pwr;		/* CRS Min/Noise power */
	int8	phy_noise_pwr_array[PHY_NOISE_PWR_ARRAY_SIZE];	/* noise buffer array */
	int8	noise_level;		/* noise pwr after filtering & averageing */
	int8	noise_level_inst;	/* instantaneous noise cal pwr */
	int8	crsmin_th_idx;		/* idx used to lookup crs min thresholds */

	uint8	lo_sync_war_cnt;	/* Track number of retries of LO SYNC WAR */

	uint32	gainidx_latchout_reg;
	uint32	adcreset_i_reg;
	uint32	adcreset_q_reg;

	uint16	ncal_m2_gain;
	uint16	auxphystats;
	uint16	phystatsgaininfo;
	uint16	flexpwrAFE;
	uint16	flexpwrdig1;
	uint16	flexpwrdig2;
	uint16	flexpwrdig3;
	uint16	flexpwrdig4;
	uint16	flexgaininfo_A;

	uint8	dccal_pktproc;		/* pktproc read during dccal health check */

	/* Misc general purpose debug counters (will be used for future debugging) */
	uint8	debug_core_01;
	uint32	debug_core_02;
	uint32	debug_core_03;
} phy_periodic_log_core_v13_t;

typedef struct phy_periodic_log_core_v255 {
	/* dccal dcoe & idacc */
	uint16	dccal_dcoe_done_0;	/* dccal control register 44 */
	uint16	dccal_dcoe_done_1;	/* dccal control register 45 */
	uint16	dccal_dcoe_done_2;	/* dccal control register 46 */
	uint16	dccal_idacc_done_0;	/* dccal control register 21 */
	uint16	dccal_idacc_done_1;	/* dccal control register 60 */
	uint16	dccal_idacc_done_2;	/* dccal control register 61 */
	int16	dccal_psb;		/* psb read during dccal health check */

	int16	dcRe_aux;
	int16	dcIm_aux;

	int16	txcap;		/* Txcap value */
	uint16	bad_txbaseidx_cnt;	/* cntr for tx_baseidx=127 in healthcheck */

	/* PA0-2 parameters */
	/* TxPwrCtrlInit_path[01].pwrIndex_init_path[01] */
	uint16	pwridx_init_pa_reg[PA_MODE_SIZE];
	uint8	baseindxval_pa_reg[PA_MODE_SIZE];	/* TPC Base index */
	int8	tgt_pwr_pa_reg[PA_MODE_SIZE];		/* Programmed Target power */
	uint16	curr_tssival_pa_reg[PA_MODE_SIZE];	/* TxPwrCtrlInit_path[01].TSSIVal */
	int8	estpwr_pa_reg[PA_MODE_SIZE];		/* tx powerDet value */
	int8	estpwradj_pa_reg[PA_MODE_SIZE];		/* Current Est Power Adjust value */

	int8	rssi_per_ant;	/* RSSI Per antenna */
	int8	snr_per_ant;	/* SNR Per antenna */

	uint16	ed20_crs_reg;		/* ED-CRS status */
	int8	ed_threshold;	/* ed threshold */

	int8	crsmin_pwr;		/* CRS Min/Noise power */
	int8	phy_noise_pwr_array[PHY_NOISE_PWR_ARRAY_SIZE];	/* noise buffer array */
	int8	noise_level;	/* noise pwr after filtering & averageing */
	int8	noise_level_inst;	/* instantaneous noise cal pwr */
	int8	crsmin_th_idx;	/* idx used to lookup crs min thresholds */

	uint8	lo_sync_war_cnt;	/* Track number of retries of LO SYNC WAR */

	uint32	gainidx_latchout_reg;
	uint32	adcreset_i_reg;
	uint32	adcreset_q_reg;

	uint16	ncal_m2_gain;
	uint16	auxphystats;
	uint16	phystatsgaininfo;
	uint16	flexpwrAFE;
	uint16	flexpwrdig1;
	uint16	flexpwrdig2;
	uint16	flexpwrdig3;
	uint16	flexpwrdig4;
	uint16	flexgaininfo_A;

	uint8	dccal_pktproc;	/* pktproc read during dccal health check */

	uint8	tpc_av;
	uint16	tpc_vmid;

	uint16	debug_core_01;

	/* SW Diversity */
	int32	swdiv_avg_snr_per_ant;
	uint32	swdiv_swap_txfail;
	uint32	swdiv_swap_alivecheck;
	uint32	swdiv_rxcount_per_ant;
	uint32	swdiv_swap_snrdrop;

	/* Misc general purpose debug counters (will be used for future debugging) */
	uint32	debug_core_02;
} phy_periodic_log_core_v255_t;

typedef struct phy_periodic_log_core_v2 {
	int32 rxs; /* FDIQ Slope coeffecient */

	uint16	ofdm_txa; /* OFDM Tx IQ Cal a coeff */
	uint16	ofdm_txb; /* OFDM Tx IQ Cal b coeff */
	uint16	ofdm_txd; /* contain di & dq */
	uint16	rxa; /* Rx IQ Cal A coeffecient */
	uint16	rxb; /* Rx IQ Cal B coeffecient */
	uint16	baseidx; /* TPC Base index */

	uint8	baseindxval; /* TPC Base index */

	int8	tgt_pwr; /* Programmed Target power */
	int8	estpwradj; /* Current Est Power Adjust value */
	int8	crsmin_pwr; /* CRS Min/Noise power */
	int8	rssi_per_ant; /* RSSI Per antenna */
	int8	snr_per_ant; /* SNR Per antenna */
	int8	pad1; /* Padding byte to align with word */
	int8	pad2; /* Padding byte to align with word */
} phy_periodic_log_core_v2_t;

/* Shared BTCX Statistics for BTCX and PHY Logging, storing the accumulated values. */
#define BTCX_STATS_PHY_LOGGING_VER 1
typedef struct wlc_btc_shared_stats {
	uint32 bt_req_cnt;		/* #BT antenna requests since last stats sampl */
	uint32 bt_gnt_cnt;		/* #BT antenna grants since last stats sample */
	uint32 bt_gnt_dur;		/* usec BT owns antenna since last stats sample */
	uint16 bt_pm_attempt_cnt;	/* PM1 attempts */
	uint16 bt_succ_pm_protect_cnt;	/* successful PM protection */
	uint16 bt_succ_cts_cnt;		/* successful CTS2A protection */
	uint16 bt_wlan_tx_preempt_cnt;	/* WLAN TX Preemption */
	uint16 bt_wlan_rx_preempt_cnt;	/* WLAN RX Preemption */
	uint16 bt_ap_tx_after_pm_cnt;	/* AP TX even after PM protection */
	uint16 bt_crtpri_cnt;		/* Ant grant by critical BT task */
	uint16 bt_pri_cnt;		/* Ant grant by high BT task */
	uint16 antgrant_lt10ms;		/* Ant grant duration cnt 0~10ms */
	uint16 antgrant_lt30ms;		/* Ant grant duration cnt 10~30ms */
	uint16 antgrant_lt60ms;		/* Ant grant duration cnt 30~60ms */
	uint16 antgrant_ge60ms;		/* Ant grant duration cnt 60~ms */
} wlc_btc_shared_stats_v1_t;

#define BTCX_STATS_PHY_LOGGING_VER2 2u
typedef struct wlc_btc_shared_stats_v2 {
	uint32 bt_req_cnt;		/* #BT antenna requests since last stats sampl */
	uint32 bt_gnt_cnt;		/* #BT antenna grants since last stats sample */
	uint32 bt_gnt_dur;		/* usec BT owns antenna since last stats sample */
	uint16 bt_pm_attempt_cnt;	/* PM1 attempts */
	uint16 bt_succ_pm_protect_cnt;	/* successful PM protection */
	uint16 bt_succ_cts_cnt;		/* successful CTS2A protection */
	uint16 bt_wlan_tx_preempt_cnt;	/* WLAN TX Preemption */
	uint16 bt_wlan_rx_preempt_cnt;	/* WLAN RX Preemption */
	uint16 bt_ap_tx_after_pm_cnt;	/* AP TX even after PM protection */
	uint16 bt_crtpri_cnt;		/* Ant grant by critical BT task */
	uint16 bt_pri_cnt;		/* Ant grant by high BT task */
	uint16 antgrant_lt10ms;		/* Ant grant duration cnt 0~10ms */
	uint16 antgrant_lt30ms;		/* Ant grant duration cnt 10~30ms */
	uint16 antgrant_lt60ms;		/* Ant grant duration cnt 30~60ms */
	uint16 antgrant_ge60ms;		/* Ant grant duration cnt 60~ms */
	uint16 ackpwroffset;		/* CoreMask (low8) and ack_pwr_offset (high8) */
	uint8 prisel_ant_mask;		/* antenna to be used by BT */

	/* Misc general purpose debug counters (will be used for future debugging) */
	uint8	debug_01;
	uint16	debug_02;
	uint16	debug_03;
	uint16	debug_04;
	uint16	debug_05;
	uint8	debug_06;
	uint8	debug_07;
	uint8	debug_08;
	uint8	debug_09;
} wlc_btc_shared_stats_v2_t;

typedef struct wlc_btc_shared_stats_v3 {
	uint32 bt_req_cnt;		/* #BT antenna requests since last stats sampl */
	uint32 bt_gnt_cnt;		/* #BT antenna grants since last stats sample */
	uint32 bt_gnt_dur;		/* usec BT owns antenna since last stats sample */
	uint16 bt_pm_attempt_cnt;	/* PM1 attempts */
	uint16 bt_succ_pm_protect_cnt;	/* successful PM protection */
	uint16 bt_succ_cts_cnt;		/* successful CTS2A protection */
	uint16 bt_wlan_tx_preempt_cnt;	/* WLAN TX Preemption */
	uint16 bt_wlan_rx_preempt_cnt;	/* WLAN RX Preemption */
	uint16 bt_ap_tx_after_pm_cnt;	/* AP TX even after PM protection */
	uint16 bt_crtpri_cnt;		/* Ant grant by critical BT task */
	uint16 bt_pri_cnt;		/* Ant grant by high BT task */
	uint16 antgrant_lt10ms;		/* Ant grant dur. cnt 0~10ms, obsolete, not in v255 */
	uint16 antgrant_lt30ms;		/* Ant grant dur. cnt 10~30ms, obsolete, not in v255 */
	uint16 antgrant_lt60ms;		/* Ant grant dur. cnt 30~60ms, obsolete, not in v255 */
	uint16 antgrant_ge60ms;		/* Ant grant dur. cnt 60~ms, obsolete, not in v255 */
	uint16 ackpwroffset;		/* CoreMask (low8) and ack_pwr_offset (high8) */
	uint8 prisel_ant_mask;		/* antenna to be used by BT */
	uint8 pad;
	uint16 btc_susp_dur;		/* MAC core sleep time, ms */
	uint16 btc_slp_dur;		/* MAC core sleep time, ms */
	uint16 bt_pm_attempt_noack_cnt; /* PM1 packets that not acked by peer */
} wlc_btc_shared_stats_v3_t;

typedef struct wlc_btc_shared_stats_v4 {
	uint32 bt_req_cnt;			/* #BT antenna requests since last stats sampl */
	uint32 bt_gnt_cnt;			/* #BT antenna grants since last stats sample */
	uint32 bt_gnt_dur;			/* usec BT owns antenna since last stats sample */
	uint16 bt_pm_attempt_cnt;		/* PM1 attempts */
	uint16 bt_succ_pm_protect_cnt;		/* successful PM protection */
	uint16 bt_succ_cts_cnt;			/* successful CTS2A protection */
	uint16 bt_wlan_tx_preempt_cnt;		/* WLAN TX Preemption */
	uint16 bt_wlan_rx_preempt_cnt;		/* WLAN RX Preemption */
	uint16 bt_ap_tx_after_pm_cnt;		/* AP TX even after PM protection */
	uint16 bt_crtpri_cnt;			/* Ant grant by critical BT task */
	uint16 bt_pri_cnt;			/* Ant grant by high BT task */
	uint16 antgrant_lt10ms;			/* grant dur. cnt 0~10ms, obsolete, not in v255 */
	uint16 antgrant_lt30ms;			/* grant dur. cnt 10~30ms, obsolete, not in v255 */
	uint16 antgrant_lt60ms;			/* grant dur. cnt 30~60ms, obsolete, not in v255 */
	uint16 antgrant_ge60ms;			/* grant dur. cnt 60~ms, obsolete, not in v255 */
	uint16 ackpwroffset;			/* CoreMask (low8) and ack_pwr_offset (high8) */
	uint8 prisel_ant_mask;			/* antenna to be used by BT */
	uint8 debug_00;
	uint16 btc_susp_dur;			/* MAC core sleep time, ms */
	uint16 btc_slp_dur;			/* MAC core sleep time, ms */
	uint16 bt_pm_attempt_noack_cnt;		/* PM1 packets that not acked by peer */
	uint16 bt5g_defer_cnt;			/* BT 5G Coex Defer Count */
	uint16 bt5g_switch_succ_cnt;		/* BT 5G Coex Switch Succ Cnt */
	uint16 bt5g_switch_fail_cnt;		/* BT 5G Coex Switch Fail Cnt */
	uint16 bt5g_no_defer_cnt;		/* BT 5G Coex No Defer Count */
	uint16 bt5g_switch_reason_bm;		/* BT 5G Switch Reason Bitmap */
} wlc_btc_shared_stats_v4_t;

#define BTCX_STATS_PHY_LOGGING_VER5 5u
typedef struct wlc_btc_shared_stats_v5 {
	uint32 bt_req_cnt;			/* #BT antenna requests since last stats sampl */
	uint32 bt_gnt_cnt;			/* #BT antenna grants since last stats sample */
	uint32 bt_gnt_dur;			/* usec BT owns antenna since last stats sample */
	uint16 bt_pm_attempt_cnt;		/* PM1 attempts */
	uint16 bt_succ_pm_protect_cnt;		/* successful PM protection */
	uint16 bt_succ_cts_cnt;			/* successful CTS2A protection */
	uint16 bt_wlan_tx_preempt_cnt;		/* WLAN TX Preemption */
	uint16 bt_wlan_rx_preempt_cnt;		/* WLAN RX Preemption */
	uint16 bt_ap_tx_after_pm_cnt;		/* AP TX even after PM protection */
	uint16 bt_crtpri_cnt;			/* Ant grant by critical BT task */
	uint16 bt_pri_cnt;			/* Ant grant by high BT task */
	uint16 ackpwroffset;			/* CoreMask (low8) and ack_pwr_offset (high8) */
	uint8 prisel_ant_mask;			/* antenna to be used by BT */
	uint8 debug_00;
	uint16 btc_susp_dur;			/* MAC core sleep time, ms */
	uint16 btc_slp_dur;			/* MAC core sleep time, ms */
	uint16 bt_pm_attempt_noack_cnt;		/* PM1 packets that not acked by peer */
	uint16 bt5g_defer_cnt;			/* BT 5G Coex Defer Count */
	uint32 bt5g_defer_max_switch_dur;	/* BT 5G Coex Defer Max Switch Dur */
	uint32 bt5g_no_defer_max_switch_dur;	/* BT 5G Coex No Defer Max Switch Dur */
	uint16 bt5g_switch_succ_cnt;		/* BT 5G Coex Switch Succ Cnt */
	uint16 bt5g_switch_fail_cnt;		/* BT 5G Coex Switch Fail Cnt */
	uint16 bt5g_no_defer_cnt;		/* BT 5G Coex No Defer Count */
	uint16 bt5g_switch_reason_bm;		/* BT 5G Switch Reason Bitmap */
} wlc_btc_shared_stats_v5_t;

#define BTCX_STATS_PHY_LOGGING_VER6 6u
typedef struct wlc_btc_shared_stats_v6 {
	uint32	bt_req_cnt;			/* #BT antenna requests since last stats sampl */
	uint32	bt_gnt_cnt;			/* #BT antenna grants since last stats sample */
	uint32	bt_gnt_dur_us;			/* usec BT owns antenna since last stats sample */
	uint16	bt_pm_attempt_cnt;		/* PM1 attempts */
	uint16	bt_succ_pm_protect_cnt;		/* successful PM protection */
	uint16	bt_succ_cts_cnt;		/* successful CTS2A protection */
	uint16	bt_wlan_tx_preempt_cnt;		/* WLAN TX Preemption */
	uint16	bt_wlan_rx_preempt_cnt;		/* WLAN RX Preemption */
	uint16	bt_ap_tx_after_pm_cnt;		/* AP TX even after PM protection */
	uint16	bt_crtpri_cnt;			/* Ant grant by critical BT task */
	uint16	bt_pri_cnt;			/* Ant grant by high BT task */
	uint16	bt_ackpwroffset;		/* CoreMask (low8) and ack_pwr_offset (high8) */
	uint8	bt_prisel_ant_mask;		/* antenna to be used by BT */
	uint8	debug_btc_00;
	uint16	bt_susp_dur_ms;			/* MAC core sleep time, ms */
	uint16	bt_slp_dur_ms;			/* MAC core sleep time, ms */
	uint16	bt_pm_attempt_noack_cnt;	/* PM1 packets that not acked by peer */
	uint16	bt5g_defer_cnt;			/* BT 5G Coex Defer Count */
	uint32	bt5g_defer_max_switch_dur_ms;	/* BT 5G Coex Defer Max Switch Dur */
	uint32	bt5g_no_defer_max_switch_dur_ms; /* BT 5G Coex No Defer Max Switch Dur */
	uint16	bt5g_switch_succ_cnt;		/* BT 5G Coex Switch Succ Cnt */
	uint16	bt5g_switch_fail_cnt;		/* BT 5G Coex Switch Fail Cnt */
	uint16	bt5g_no_defer_cnt;		/* BT 5G Coex No Defer Count */
	uint16	bt5g_switch_reason_bm;		/* BT 5G Switch Reason Bitmap */
} wlc_btc_shared_stats_v6_t;

#define BTCX_STATS_PHY_LOGGING_VER255 255u
typedef struct wlc_btc_shared_stats_v255 {
	uint32	bt_req_cnt;			/* #BT antenna requests since last stats sampl */
	uint32	bt_gnt_cnt;			/* #BT antenna grants since last stats sample */
	uint32	bt_gnt_dur_us;			/* usec BT owns antenna since last stats sample */
	uint16	bt_pm_attempt_cnt;		/* PM1 attempts */
	uint16	bt_succ_pm_protect_cnt;		/* successful PM protection */
	uint16	bt_succ_cts_cnt;		/* successful CTS2A protection */
	uint16	bt_wlan_tx_preempt_cnt;		/* WLAN TX Preemption */
	uint16	bt_wlan_rx_preempt_cnt;		/* WLAN RX Preemption */
	uint16	bt_ap_tx_after_pm_cnt;		/* AP TX even after PM protection */
	uint16	bt_crtpri_cnt;			/* Ant grant by critical BT task */
	uint16	bt_pri_cnt;			/* Ant grant by high BT task */
	uint16	bt_ackpwroffset;		/* CoreMask (low8) and ack_pwr_offset (high8) */
	uint8	bt_prisel_ant_mask;		/* antenna to be used by BT */
	uint8	debug_btc_00;
	uint16	bt_susp_dur_ms;			/* MAC core sleep time, ms */
	uint16	bt_slp_dur_ms;			/* MAC core sleep time, ms */
	uint16	bt_pm_attempt_noack_cnt;	/* PM1 packets that not acked by peer */
	uint16	bt5g_defer_cnt;			/* BT 5G Coex Defer Count */
	uint32	bt5g_defer_max_switch_dur_ms;	/* BT 5G Coex Defer Max Switch Dur */
	uint32	bt5g_no_defer_max_switch_dur_ms; /* BT 5G Coex No Defer Max Switch Dur */
	uint16	bt5g_switch_succ_cnt;		/* BT 5G Coex Switch Succ Cnt */
	uint16	bt5g_switch_fail_cnt;		/* BT 5G Coex Switch Fail Cnt */
	uint16	bt5g_no_defer_cnt;		/* BT 5G Coex No Defer Count */
	uint16	bt5g_switch_reason_bm;		/* BT 5G Switch Reason Bitmap */
} wlc_btc_shared_stats_v255_t;

/* BTCX Statistics for PHY Logging */
typedef struct wlc_btc_stats_phy_logging {
	uint16 ver;
	uint16 length;
	uint32 btc_status;		/* btc status log */
	uint32 bt_req_type_map;		/* BT Antenna Req types since last stats sample */
	wlc_btc_shared_stats_v1_t shared;
} phy_periodic_btc_stats_v1_t;

/* BTCX Statistics for PHY Logging */
typedef struct wlc_btc_stats_phy_logging_v2 {
	uint16 ver;
	uint16 length;
	uint32 btc_status;		/* btc status log */
	uint32 bt_req_type_map;		/* BT Antenna Req types since last stats sample */
	wlc_btc_shared_stats_v2_t shared;
} phy_periodic_btc_stats_v2_t;

/* BTCX Statistics for PHY Logging */
typedef struct wlc_btc_stats_phy_logging_v3 {
	uint16 ver;
	uint16 length;
	uint32 btc_status;		/* btc status log */
	uint32 btc_status2;		/* BT coex status 2 */
	uint32 bt_req_type_map;		/* BT Antenna Req types since last stats sample */
	int8 btcx_desense_mode;		/* btcoex desense mode, 0 - 7 */
	int8 btrssi;			/* the snapshot of bt rssi */
	int8 profile_2g_active;		/* 2G active profile index */
	int8 profile_5g_active;		/* 5G active profile index */
	wlc_btc_shared_stats_v3_t shared;
} phy_periodic_btc_stats_v3_t;

typedef struct wlc_btc_stats_phy_logging_v4 {
	uint16 ver;
	uint16 length;
	uint32 btc_status;		/* btc status log */
	uint32 btc_status2;		/* BT coex status 2 */
	uint32 bt_req_type_map;		/* BT Antenna Req types since last stats sample */
	int8 btcx_desense_mode;		/* btcoex desense mode, 0 - 7 */
	int8 btrssi;			/* the snapshot of bt rssi */
	int8 profile_2g_active;		/* 2G active profile index */
	int8 profile_5g_active;		/* 5G active profile index */
	uint32 bt5g_status;		/* BT 5G Coex Status */
	wlc_btc_shared_stats_v4_t shared;
} phy_periodic_btc_stats_v4_t;

typedef struct wlc_btc_stats_phy_logging_v5 {
	uint16 ver;
	uint16 length;
	uint32 btc_status;		/* btc status log */
	uint32 btc_status2;		/* BT coex status 2 */
	uint32 bt_req_type_map;		/* BT Antenna Req types since last stats sample */
	int8 btcx_desense_mode;		/* btcoex desense mode, 0 - 7 */
	int8 btrssi;			/* the snapshot of bt rssi */
	int8 profile_2g_active;		/* 2G active profile index */
	int8 profile_5g_active;		/* 5G active profile index */
	uint32 bt5g_status;		/* BT 5G Coex Status */
	wlc_btc_shared_stats_v5_t shared;
} phy_periodic_btc_stats_v5_t;

typedef struct wlc_btc_stats_phy_logging_v6 {
	uint16	ver;
	uint16	length;
	uint32	bt_status;		/* btc status log */
	uint32	bt_status2;		/* BT coex status 2 */
	uint32	bt_req_type_map;	/* BT Antenna Req types since last stats sample */
	int8	bt_desense_mode;		/* btcoex desense mode, 0 - 7 */
	int8	bt_rssi;			/* the snapshot of bt rssi */
	int8	bt_profile_2g_active;	/* 2G active profile index */
	int8	bt_profile_5g_active;	/* 5G active profile index */
	uint32	bt5g_status;		/* BT 5G Coex Status */
	wlc_btc_shared_stats_v6_t	shared;
} phy_periodic_btc_stats_v6_t;

/* BTCX Statistics for PHY Logging */
typedef struct wlc_btc_stats_phy_logging_v255 {
	uint16	ver;
	uint16	length;
	uint32	bt_status;		/* btc status log */
	uint32	bt_status2;		/* BT coex status 2 */
	uint32	bt_req_type_map;	/* BT Antenna Req types since last stats sample */
	int8	bt_desense_mode;		/* btcoex desense mode, 0 - 7 */
	int8	bt_rssi;			/* the snapshot of bt rssi */
	int8	bt_profile_2g_active;	/* 2G active profile index */
	int8	bt_profile_5g_active;	/* 5G active profile index */
	uint32	bt5g_status;		/* BT 5G Coex Status */
	wlc_btc_shared_stats_v255_t shared;
} phy_periodic_btc_stats_v255_t;

/* OBSS Statistics for PHY Logging */
typedef struct phy_periodic_obss_stats_v1 {
	uint32	obss_last_read_time;			/* last stats read time */
	uint16	obss_mit_bw;				/* selected mitigation BW */
	uint16	obss_stats_cnt;				/* stats count */
	uint8	obss_mit_mode;				/* mitigation mode */
	uint8	obss_mit_status;			/* obss mitigation status */
	uint8	obss_curr_det[ACPHY_OBSS_SUBBAND_CNT];	/* obss curr detection */
	uint16	dynbw_init_reducebw_cnt;		/*
							 * bandwidth reduction cnt of
							 * initiator (txrts+rxcts)
							 */
	uint16	dynbw_resp_reducebw_cnt;		/*
							 * bandwidth reduction cnt of
							 * responder (rxrts+txcts)
							 */
	uint16	dynbw_rxdata_reducebw_cnt;		/*
							 * rx data cnt with reduced bandwidth
							 * as txcts requested
							 */
} phy_periodic_obss_stats_v1_t;

/* OBSS Statistics for PHY Logging */
typedef struct phy_periodic_obss_stats_v2 {
	uint32	obss_last_read_time;			/* last stats read time */
	uint16	obss_mit_bw;				/* selected mitigation BW */
	uint16	obss_stats_cnt;				/* stats count */
	uint8	obss_need_updt;				/* BW update needed flag */
	uint8	obss_mit_status;			/* obss mitigation status */
	uint8	obss_curr_det[ACPHY_OBSS_SUBBAND_CNT];	/* obss curr detection */
	uint16	dynbw_init_reducebw_cnt;		/*
							 * bandwidth reduction cnt of
							 * initiator (txrts+rxcts)
							 */
	uint16	dynbw_resp_reducebw_cnt;		/*
							 * bandwidth reduction cnt of
							 * responder (rxrts+txcts)
							 */
	uint16	dynbw_rxdata_reducebw_cnt;		/*
							 * rx data cnt with reduced bandwidth
							 * as txcts requested
							 */
	uint16	obss_mmt_skip_cnt;			/* mmt skipped due to powersave */
	uint16	obss_mmt_no_result_cnt;			/* mmt with no result */
	uint16	obss_mmt_intr_err_cnt;			/* obss reg mismatch between ucode and fw */
	uint8	obss_final_rec_bw;			/* final recommended bw to wlc-Sent to SW */
	uint8	debug01;
	uint16	obss_det_stats[ACPHY_OBSS_SUBBAND_CNT];
} phy_periodic_obss_stats_v2_t;

typedef struct phy_periodic_obss_stats_v3 {
	uint32	obss_last_read_time;			/* last stats read time */
	uint16	obss_mit_bw;				/* selected mitigation BW */
	uint16	obss_stats_cnt;				/* stats count */
	uint8	obss_need_updt;				/* BW update needed flag */
	uint8	obss_mit_status;			/* obss mitigation status */
	uint8	obss_curr_det[ACPHY_OBSS_SUBBAND_CNT];	/* obss curr detection */
	uint16	dynbw_init_reducebw_cnt;		/*
							 * bandwidth reduction cnt of
							 * initiator (txrts+rxcts)
							 */
	uint16	dynbw_resp_reducebw_cnt;		/*
							 * bandwidth reduction cnt of
							 * responder (rxrts+txcts)
							 */
	uint16	dynbw_rxdata_reducebw_cnt;		/*
							 * rx data cnt with reduced bandwidth
							 * as txcts requested
							 */
	uint16	obss_mmt_skip_cnt;			/* mmt skipped due to powersave */
	uint16	obss_mmt_no_result_cnt;			/* mmt with no result */
	uint16	obss_mmt_intr_err_cnt;			/* obss reg mismatch between ucode and fw */
	uint8	obss_last_rec_bw;			/* last recommended bw to wlc-Sent to SW */
	uint8	debug_01;

	/* Misc general purpose debug counters (will be used for future debugging) */
	uint16	debug_02;
	uint16	debug_03;
	uint16	debug_04;
	uint16	debug_05;
} phy_periodic_obss_stats_v3_t;

#define DYNBW_MAX_NUM	4u
typedef struct phy_periodic_obss_stats_v4 {
	uint32	obss_last_read_time;			/* last stats read time */
	uint16	obss_mit_bw;				/* selected mitigation BW */
	uint16	obss_stats_cnt;				/* stats count */
	uint8	obss_need_updt;				/* BW update needed flag */
	uint8	obss_mit_status;			/* obss mitigation status */
	uint8	obss_curr_det[ACPHY_OBSS_SUBBAND_CNT];	/* obss curr detection */
	uint16	obss_mmt_skip_cnt;			/* mmt skipped due to powersave */
	uint16	obss_mmt_no_result_cnt;			/* mmt with no result */
	uint16	obss_mmt_intr_err_cnt;			/* obss reg mismatch between ucode and fw */
	uint16	dynbw_reqbw_txrts20_cnt;		/* RTS Tx in 20MHz cnt */
	uint16	dynbw_reqbw_txrts40_cnt;		/* RTS Tx in 40MHz cnt */
	uint16	dynbw_reqbw_txrts80_cnt;		/* RTS Tx in 80MHz cnt */
	uint16	dynbw_grntbw_txcts20_cnt;		/* CTS Tx in 20MHz cnt */
	uint16	dynbw_grntbw_txcts40_cnt;		/* CTS Tx in 40MHz cnt */
	uint16	dynbw_grntbw_txcts80_cnt;		/* CTS Tx in 80MHz cnt */
	uint16	dynbw_reqbw_rxrts20_cnt;		/* Rx dynamicRTS in 20MHz cnt */
	uint16	dynbw_reqbw_rxrts40_cnt;		/* Rx dynamicRTS in 40MHz cnt */
	uint16	dynbw_reqbw_rxrts80_cnt;		/* Rx dynamicRTS in 80MHz cnt */
	uint16	dynbw_grntbw_rxcts20_cnt;		/* Rx CTS responses in 20MHz cnt */
	uint16	dynbw_grntbw_rxcts40_cnt;		/* Rx CTS responses in 40MHz cnt */
	uint16	dynbw_grntbw_rxcts80_cnt;		/* Rx CTS responses in 80MHz cnt */
	uint16	dynbw_availbw_blk[DYNBW_MAX_NUM];	/* BW histogram when Rx RTS */
	int8	obss_pwrest[WL_OBSS_ANT_MAX][ACPHY_OBSS_SUBBAND_CNT]; /* OBSS signal power per
									* sub-band in dBm
									*/
	uint8	obss_last_rec_bw;			/* last recommended bw to wlc-Sent to SW */

	/* Misc general purpose debug counters (will be used for future debugging) */
	uint8	debug_01;
	uint8	debug_02;
	uint8	debug_03;
	uint16	debug_04;
	uint16	debug_05;
} phy_periodic_obss_stats_v4_t;

typedef struct phy_periodic_obss_stats_v5 {
	uint32	obss_last_read_time;		/* last stats read time */
	uint16	obss_stats_cnt;			/* stats count */
	uint16	obss_mmt_skip_cnt;		/* mmt skipped due to powersave */
	uint16	obss_mmt_no_result_cnt;		/* mmt with no result */
	uint16	obss_mmt_intr_err_cnt;		/* obss reg mismatch between ucode and fw */
	uint16	dynbw_reqbw_txrts20_cnt;	/* RTS Tx in 20MHz cnt */
	uint16	dynbw_reqbw_txrts40_cnt;	/* RTS Tx in 40MHz cnt */
	uint16	dynbw_reqbw_txrts80_cnt;	/* RTS Tx in 80MHz cnt */
	uint16	dynbw_grntbw_txcts20_cnt;	/* CTS Tx in 20MHz cnt */
	uint16	dynbw_grntbw_txcts40_cnt;	/* CTS Tx in 40MHz cnt */
	uint16	dynbw_grntbw_txcts80_cnt;	/* CTS Tx in 80MHz cnt */
	uint16	dynbw_reqbw_rxrts20_cnt;	/* Rx dynamicRTS in 20MHz cnt */
	uint16	dynbw_reqbw_rxrts40_cnt;	/* Rx dynamicRTS in 40MHz cnt */
	uint16	dynbw_reqbw_rxrts80_cnt;	/* Rx dynamicRTS in 80MHz cnt */
	uint16	dynbw_grntbw_rxcts20_cnt;	/* Rx CTS responses in 20MHz cnt */
	uint16	dynbw_grntbw_rxcts40_cnt;	/* Rx CTS responses in 40MHz cnt */
	uint16	dynbw_grntbw_rxcts80_cnt;	/* Rx CTS responses in 80MHz cnt */
	uint16	dynbw_availbw_blk[DYNBW_MAX_NUM];	/* BW histogram when Rx RTS */
	int8	obss_pwrest[WL_OBSS_ANT_MAX][ACPHY_OBSS_SUBBAND_CNT]; /* OBSS signal power per
									* sub-band in dBm
									*/
	uint8	obss_last_rec_bw;		/* last recommended bw to wlc-Sent to SW */
	uint8	obss_curr_det_bitmap;		/* obss curr detection bitmap: 0 - LLL, 7 - UUU */

	/* Misc general purpose debug counters (will be used for future debugging) */
	uint8	debug_01;
	uint8	debug_02;
} phy_periodic_obss_stats_v5_t;

typedef struct phy_periodic_obss_stats_v6 {
	uint32	obss_last_read_time_s;		/* last stats read time */
	uint16	obss_stats_cnt;			/* stats count */
	uint16	obss_mmt_skip_cnt;		/* mmt skipped due to powersave */
	uint16	obss_mmt_no_result_cnt;		/* mmt with no result */
	uint16	obss_mmt_intr_err_cnt;		/* obss reg mismatch between ucode and fw */
	int8	obss_pwrest_dBm[WL_OBSS_ANT_MAX][ACPHY_OBSS_SUBBAND_CNT]; /* OBSS signal power per
									     * sub-band in dBm
									     */
	uint8   obss_last_rec_bw;		/* last recommended bw to wlc-Sent to SW */
	uint8   obss_curr_det_bitmap;		/* obss curr detection bitmap: 0 - LLL, 7 - UUU */

	uint16	dynbw_reqbw_rxrts20_cnt;	/* Rx dynamicRTS in 20MHz cnt */
	uint16	dynbw_grntbw_txcts20_cnt;	/* CTS Tx in 20MHz cnt */
	uint16	dynbw_reqbw_rxrts40_cnt;	/* Rx dynamicRTS in 40MHz cnt */
	uint16	dynbw_grntbw_txcts40_cnt;	/* CTS Tx in 40MHz cnt */
	uint16	dynbw_reqbw_rxrts80_cnt;	/* Rx dynamicRTS in 80MHz cnt */
	uint16	dynbw_grntbw_txcts80_cnt;	/* CTS Tx in 80MHz cnt */
	uint16	dynbw_availbw_blk[DYNBW_MAX_NUM];	/* BW histogram when Rx RTS */

	/* Misc general purpose debug counters (will be used for future debugging) */
	uint8	debug_obss_01;
	uint8	debug_obss_02;
} phy_periodic_obss_stats_v6_t;

typedef struct phy_periodic_obss_stats_v255 {
	uint32	obss_last_read_time_s;		/* last stats read time */
	uint16	obss_stats_cnt;			/* stats count */
	uint16	obss_mmt_skip_cnt;		/* mmt skipped due to powersave */
	uint16	obss_mmt_no_result_cnt;		/* mmt with no result */
	uint16	obss_mmt_intr_err_cnt;		/* obss reg mismatch between ucode and fw */
	int8	obss_pwrest_dBm[WL_OBSS_ANT_MAX][ACPHY_OBSS_SUBBAND_CNT]; /* OBSS signal power per
									* sub-band in dBm
									*/
	uint8	obss_last_rec_bw;		/* last recommended bw to wlc-Sent to SW */
	uint8	obss_curr_det_bitmap;		/* obss curr detection bitmap: 0 - LLL, 7 - UUU */

	uint16	dynbw_reqbw_rxrts20_cnt;	/* Rx dynamicRTS in 20MHz cnt */
	uint16	dynbw_grntbw_txcts20_cnt;	/* CTS Tx in 20MHz cnt */
	uint16	dynbw_reqbw_txrts20_cnt;	/* RTS Tx in 20MHz cnt */
	uint16	dynbw_grntbw_rxcts20_cnt;	/* Rx CTS responses in 20MHz cnt */
	uint16	dynbw_reqbw_rxrts40_cnt;	/* Rx dynamicRTS in 40MHz cnt */
	uint16	dynbw_grntbw_txcts40_cnt;	/* CTS Tx in 40MHz cnt */
	uint16	dynbw_reqbw_txrts40_cnt;	/* RTS Tx in 40MHz cnt */
	uint16	dynbw_grntbw_rxcts40_cnt;	/* Rx CTS responses in 40MHz cnt */
	uint16	dynbw_reqbw_rxrts80_cnt;	/* Rx dynamicRTS in 80MHz cnt */
	uint16	dynbw_grntbw_txcts80_cnt;	/* CTS Tx in 80MHz cnt */
	uint16	dynbw_reqbw_txrts80_cnt;	/* RTS Tx in 80MHz cnt */
	uint16	dynbw_grntbw_rxcts80_cnt;	/* Rx CTS responses in 80MHz cnt */
	uint16	dynbw_availbw_blk[DYNBW_MAX_NUM];	/* BW histogram when Rx RTS */

	/* Misc general purpose debug counters (will be used for future debugging) */
	uint8	debug_obss_01;
	uint8	debug_obss_02;
} phy_periodic_obss_stats_v255_t;

/* SmartCCA related PHY Logging */
typedef struct phy_periodic_scca_stats_v1 {
	uint32 asym_intf_cmplx_pwr[2];
	uint32 asym_intf_ncal_time;
	uint32 asym_intf_host_req_mit_turnon_time;
	uint32 core1_smask_val_bk;	/* bt fem control related */
	int32 asym_intf_ed_thresh;

	uint16 crsminpoweru0;			/* crsmin thresh */
	uint16 crsminpoweroffset0;		/* ac_offset core0 */
	uint16 crsminpoweroffset1;		/* ac_offset core1 */
	uint16 Core0InitGainCodeB;		/* rx mitigation: eLNA bypass setting */
	uint16 Core1InitGainCodeB;		/* rx mitigation: eLNA bypass setting */
	uint16 ed_crsEn;				/* phyreg(ed_crsEn) */
	uint16 nvcfg0;				/* LLR deweighting coefficient */
	uint16 SlnaRxMaskCtrl0;
	uint16 SlnaRxMaskCtrl1;
	uint16 save_SlnaRxMaskCtrl0;
	uint16 save_SlnaRxMaskCtrl1;
	uint16 asym_intf_ncal_req_chspec;	/* channel request noisecal */
	/* asym_intf_stats includes the following bits:
	* b[0]:   bool asym_intf_rx_noise_mit_on;
	* b[1]:   bool asym_intf_tx_smartcca_on;
	* b[3:2]: bool asym_intf_elna_bypass[2];
	* b[4]:   bool asym_intf_valid_noise_samp;
	* b[5]:   bool asym_intf_fill_noise_buf;
	* b[6]:   bool asym_intf_ncal_discard;
	* b[7]:   bool slna_reg_saved;
	* b[8]:   bool asym_intf_host_ext_usb;		//Host control related variable
	* b[9]:   bool asym_intf_host_ext_usb_chg;	// Host control related variable
	* b[10]:  bool asym_intf_host_en;		// Host control related variable
	* b[11]:  bool asym_intf_host_enable;
	* b[12]:  bool asym_intf_pending_host_req;	// Set request pending if clk not present
	*/
	uint16 asym_intf_stats;

	uint8 elna_bypass;	/* from bt_desense.elna_bypass in gainovr_shm_config() */
	uint8 btc_mode;		/* from bt_desense in gainovr_shm_config() */
	/* noise at antenna from phy_ac_noise_ant_noise_calc() */
	int8 noise_dbm_ant[2];
	/* in phy_ac_noise_calc(), also used by wl noise */
	int8 noisecalc_cmplx_pwr_dbm[2];
	uint8 gain_applied;		/* from phy_ac_rxgcrs_set_init_clip_gain() */

	uint8 asym_intf_tx_smartcca_cm;
	uint8 asym_intf_rx_noise_mit_cm;
	int8 asym_intf_avg_noise[2];
	int8 asym_intf_latest_noise[2];
	/* used to calculate noise_delta for rx mitigation on/off */
	int8 asym_intf_prev_noise_lvl[2];
	uint8 asym_intf_noise_calc_gain_2g[2];
	uint8 asym_intf_noise_calc_gain_5g[2];
	uint8 asym_intf_ant_noise_idx;
	uint8 asym_intf_least_core_idx;
	uint8 phy_crs_thresh_save[2];
	uint8 asym_intf_initg_desense[2];
	uint8 asym_intf_pending_host_req_type;	/* Set request pending if clk not present */
} phy_periodic_scca_stats_v1_t;

/* SmartCCA related PHY Logging */
typedef struct phy_periodic_scca_stats_v2 {
	uint32 asym_intf_cmplx_pwr[2];
	uint32 asym_intf_ncal_time;
	uint32 asym_intf_host_req_mit_turnon_time;
	uint32 core1_smask_val_bk;	/* bt fem control related */
	int32 asym_intf_ed_thresh;

	uint16 crsminpoweru0;			/* crsmin thresh */
	uint16 crsminpoweroffset0;		/* ac_offset core0 */
	uint16 crsminpoweroffset1;		/* ac_offset core1 */
	uint16 Core0InitGainCodeB;		/* rx mitigation: eLNA bypass setting */
	uint16 Core1InitGainCodeB;		/* rx mitigation: eLNA bypass setting */
	uint16 ed_crsEn;			/* phyreg(ed_crsEn) */
	uint16 nvcfg0;				/* LLR deweighting coefficient */
	uint16 SlnaRxMaskCtrl0;
	uint16 SlnaRxMaskCtrl1;
	uint16 save_SlnaRxMaskCtrl0;
	uint16 save_SlnaRxMaskCtrl1;
	uint16 asym_intf_ncal_req_chspec;	/* channel request noisecal */
	/* asym_intf_stats includes the following bits:
	* b[0]:   bool asym_intf_rx_noise_mit_on;
	* b[1]:   bool asym_intf_tx_smartcca_on;
	* b[3:2]: bool asym_intf_elna_bypass[2];
	* b[4]:   bool asym_intf_valid_noise_samp;
	* b[5]:   bool asym_intf_fill_noise_buf;
	* b[6]:   bool asym_intf_ncal_discard;
	* b[7]:   bool slna_reg_saved;
	* b[8]:   bool asym_intf_host_ext_usb;		//Host control related variable
	* b[9]:   bool asym_intf_host_ext_usb_chg;	// Host control related variable
	* b[10]:  bool asym_intf_host_en;		// Host control related variable
	* b[11]:  bool asym_intf_host_enable;
	* b[12]:  bool asym_intf_pending_host_req;	// Set request pending if clk not present
	*/
	uint16 asym_intf_stats;

	uint8 elna_bypass;	/* from bt_desense.elna_bypass in gainovr_shm_config() */
	uint8 btc_mode;		/* from bt_desense in gainovr_shm_config() */
	/* noise at antenna from phy_ac_noise_ant_noise_calc() */
	int8 noise_dbm_ant[2];
	/* in phy_ac_noise_calc(), also used by wl noise */
	int8 noisecalc_cmplx_pwr_dbm[2];
	uint8 gain_applied;		/* from phy_ac_rxgcrs_set_init_clip_gain() */

	uint8 asym_intf_tx_smartcca_cm;
	uint8 asym_intf_rx_noise_mit_cm;
	int8 asym_intf_avg_noise[2];
	int8 asym_intf_latest_noise[2];
	/* used to calculate noise_delta for rx mitigation on/off */
	int8 asym_intf_prev_noise_lvl[2];
	uint8 asym_intf_noise_calc_gain_2g[2];
	uint8 asym_intf_noise_calc_gain_5g[2];
	uint8 asym_intf_ant_noise_idx;
	uint8 asym_intf_least_core_idx;
	uint8 phy_crs_thresh_save[2];
	uint8 asym_intf_initg_desense[2];
	uint8 asym_intf_pending_host_req_type;	/* Set request pending if clk not present */
	uint16	asym_intf_ncal_crs_stat[12];
	uint8	asym_intf_ncal_crs_stat_idx;
} phy_periodic_scca_stats_v2_t;

/* SmartCCA related PHY Logging - v3 NEWT SmartCCA based */
typedef struct phy_periodic_scca_stats_v3 {
	uint32	asym_intf_ncal_time;
	uint32	asym_intf_host_req_mit_turnon_time;
	int32	asym_intf_ed_thresh;

	uint16	crsminpoweru0;			/* crsmin thresh */
	uint16	crsminpoweroffset0;		/* ac_offset core0 */
	uint16	crsminpoweroffset1;		/* ac_offset core1 */
	uint16	ed_crsEn;			/* phyreg(ed_crsEn) */
	uint16	nvcfg0;				/* LLR deweighting coefficient */
	uint16	SlnaRxMaskCtrl0;
	uint16	SlnaRxMaskCtrl1;
	uint16	CRSMiscellaneousParam;
	uint16	AntDivConfig2059;
	uint16	HPFBWovrdigictrl;
	uint16	save_SlnaRxMaskCtrl0;
	uint16	save_SlnaRxMaskCtrl1;
	uint16	asym_intf_ncal_req_chspec;	/* channel request noisecal */
	/* asym_intf_stats includes the following bits:
	* b[0]:   bool asym_intf_rx_noise_mit_on;	// SmartCCA Rx mititagion enabled
	* b[1]:   bool asym_intf_tx_smartcca_on;	// SmartCCA Tx mititagion enabled
	* b[2]:   bool asym_intf_valid_noise_samp;	// Latest noise sample is valid
	* b[3]:   bool asym_intf_fill_noise_buf;	// Fill the same sample to entire buffer
	* b[4]:   bool asym_intf_ncal_discard;		// Discard current noise sample
	* b[5]:   bool slna_reg_saved;			// SLNA register values are saved
	* b[6]:   bool asym_intf_host_ext_usb;		// Host control related variable
	* b[7]:   bool asym_intf_host_ext_usb_chg;	// Host control related variable
	* b[8]:   bool asym_intf_host_en;		// Host control related variable
	* b[9]:   bool asym_intf_host_enable;		// Host control related variable
	* b[10]:  bool asym_intf_pending_host_req;	// Set request pending if clk not present
	*/
	uint16	asym_intf_stats;

	uint8	btc_mode;			/* from bt_desense in gainovr_shm_config() */

	/* noise at antenna from phy_ac_noise_calc() */
	int8	noisecalc_cmplx_pwr_dbm[2];
	int8	asym_intf_ant_noise[2];

	uint8	asym_intf_tx_smartcca_cm;
	uint8	asym_intf_rx_noise_mit_cm;
	int8	asym_intf_avg_noise[2];
	int8	asym_intf_latest_noise[2];
	/* used to calculate noise_delta for rx mitigation on/off */
	int8	asym_intf_prev_noise_lvl[2];
	uint8	asym_intf_ant_noise_idx;
	uint8	asym_intf_least_core_idx;
	uint8	asym_intf_pending_host_req_type;
						/* Set request pending if clk not present */
	uint16	asym_intf_ncal_crs_stat;
	uint8	asym_intf_ncal_crs_stat_idx;
	uint8	pad;
} phy_periodic_scca_stats_v3_t;

/* SmartCCA related PHY Logging - v4 SmartCCA with LTE asym jammer */
typedef struct phy_periodic_scca_stats_v4 {
	uint32	asym_intf_ncal_time;
	uint32	asym_intf_host_req_mit_turnon_time;
	int32	asym_intf_ed_thresh;

	uint16	crsminpoweru0;			/* crsmin thresh */
	uint16	crsminpoweroffset0;		/* ac_offset core0 */
	uint16	crsminpoweroffset1;		/* ac_offset core1 */
	uint16	ed_crsEn;			/* phyreg(ed_crsEn) */
	uint16	nvcfg0;				/* LLR deweighting coefficient */
	uint16	SlnaRxMaskCtrl0;
	uint16	SlnaRxMaskCtrl1;
	uint16	CRSMiscellaneousParam;
	uint16	AntDivConfig2059;
	uint16	HPFBWovrdigictrl;
	uint16	save_SlnaRxMaskCtrl0;
	uint16	save_SlnaRxMaskCtrl1;
	uint16	asym_intf_ncal_req_chspec;	/* channel request noisecal */
	/* asym_intf_stats includes the following bits:
	* b[0]:   bool asym_intf_rx_noise_mit_on;	// SmartCCA Rx mititagion enabled
	* b[1]:   bool asym_intf_tx_smartcca_on;	// SmartCCA Tx mititagion enabled
	* b[2]:   bool asym_intf_valid_noise_samp;	// Latest noise sample is valid
	* b[3]:   bool asym_intf_fill_noise_buf;	// Fill the same sample to entire buffer
	* b[4]:   bool asym_intf_ncal_discard;		// Discard current noise sample
	* b[5]:   bool slna_reg_saved;			// SLNA register values are saved
	* b[6]:   bool asym_intf_host_ext_usb;		// Host control related variable
	* b[7]:   bool asym_intf_host_ext_usb_chg;	// Host control related variable
	* b[8]:   bool asym_intf_host_en;		// Host control related variable
	* b[9]:   bool asym_intf_host_enable;		// Host control related variable
	* b[10]:  bool asym_intf_pending_host_req;	// Set request pending if clk not present
	*/
	uint16	asym_intf_stats;

	uint8	btc_mode;			/* from bt_desense in gainovr_shm_config() */

	/* noise at antenna from phy_ac_noise_calc() */
	int8	noisecalc_cmplx_pwr_dbm[2];
	int8	asym_intf_ant_noise[2];

	uint8	asym_intf_tx_smartcca_cm;
	uint8	asym_intf_rx_noise_mit_cm;	/* Available */

	/* LTE asymmetric jammer paramaters */
	bool	asym_intf_jammer_en;
	uint8	asym_intf_jammer_cm;
	int8	asym_intf_jammer_pwr[2];

	int8	asym_intf_avg_noise[2];
	int8	asym_intf_latest_noise[2];
	/* used to calculate noise_delta for rx mitigation on/off */
	int8	asym_intf_prev_noise_lvl[2];
	uint8	asym_intf_ant_noise_idx;
	uint8	asym_intf_least_core_idx;
	uint8	asym_intf_pending_host_req_type;
						/* Set request pending if clk not present */
	uint16	asym_intf_ncal_crs_stat;
	uint8	asym_intf_ncal_crs_stat_idx;
	uint8	debug_01;
} phy_periodic_scca_stats_v4_t;

/* SmartCCA related PHY Logging - v5 SmartCCA with LTE asym jammer */
typedef struct phy_periodic_scca_stats_v5 {
	uint32	asym_intf_ncal_time_us;
	uint32	asym_intf_host_req_mit_turnon_time;
	int32	asym_intf_ed_thresh_dBm;

	uint16	ncal_crsminpoweru0_reg;		/* crsmin thresh */
	uint16	ncal_crsminpoweroffset0_reg;	/* ac_offset core0 */
	uint16	ncal_crsminpoweroffset1_reg;	/* ac_offset core1 */
	uint16	ncal_ed_crsEn_reg;		/* phyreg(ed_crsEn) */
	uint16	ncal_nvcfg0_reg;		/* LLR deweighting coefficient */
	uint16	ncal_SlnaRxMaskCtrl0_reg;
	uint16	ncal_SlnaRxMaskCtrl1_reg;
	uint16	ncal_CRSMiscellaneousParam_reg;
	uint16	ncal_AntDivConfig2059_reg;
	uint16	ncal_HPFBWovrdigictrl_reg;
	uint16	save_SlnaRxMaskCtrl0_reg;
	uint16	save_SlnaRxMaskCtrl1_reg;
	uint16	asym_intf_ncal_req_chspec;	/* channel request noisecal */
	/* asym_intf_stats includes the following bits:
	* b[0]:   bool asym_intf_rx_noise_mit_on;	// SmartCCA Rx mititagion enabled
	* b[1]:   bool asym_intf_tx_smartcca_on;	// SmartCCA Tx mititagion enabled
	* b[2]:   bool asym_intf_valid_noise_samp;	// Latest noise sample is valid
	* b[3]:   bool asym_intf_fill_noise_buf;	// Fill the same sample to entire buffer
	* b[4]:   bool asym_intf_ncal_discard;		// Discard current noise sample
	* b[5]:   bool slna_reg_saved;			// SLNA register values are saved
	* b[6]:   bool asym_intf_host_ext_usb;		// Host control related variable
	* b[7]:   bool asym_intf_host_ext_usb_chg;	// Host control related variable
	* b[8]:   bool asym_intf_host_en;		// Host control related variable
	* b[9]:   bool asym_intf_host_enable;		// Host control related variable
	* b[10]:  bool asym_intf_pending_host_req;	// Set request pending if clk not present
	*/
	uint16	asym_intf_stats;

	uint8	asym_btc_mode;			/* from bt_desense in gainovr_shm_config() */

	/* noise at antenna from phy_ac_noise_calc() */
	int8	asym_noisecalc_cmplx_pwr_dbm[2];
	int8	asym_intf_ant_noise[2];

	/* LTE asymmetric jammer paramaters */
	bool	asym_intf_jammer_en;
	uint8	asym_intf_jammer_cm;
	int8	asym_intf_jammer_pwr[2];

	int8	asym_intf_avg_noise[2];
	int8	asym_intf_latest_noise[2];
	/* used to calculate noise_delta for rx mitigation on/off */
	int8	asym_intf_prev_noise_lvl[2];

	uint8	asym_intf_tx_smartcca_cm;
	uint8	asym_intf_ant_noise_idx;
	uint8	asym_intf_least_core_idx;
	uint8	asym_intf_pending_host_req_type;
						/* Set request pending if clk not present */
	uint8	asym_intf_ncal_crs_stat_idx;
	uint16	asym_intf_ncal_crs_stat;

	uint16	debug_scca_01;
} phy_periodic_scca_stats_v5_t;

/* SmartCCA related PHY Logging */
typedef struct phy_periodic_scca_stats_v255 {
	uint32	asym_intf_ncal_time_us;
	uint32	asym_intf_host_req_mit_turnon_time;
	int32	asym_intf_ed_thresh_dBm;

	uint16	ncal_crsminpoweru0_reg;		/* crsmin thresh */
	uint16	ncal_crsminpoweroffset0_reg;	/* ac_offset core0 */
	uint16	ncal_crsminpoweroffset1_reg;	/* ac_offset core1 */
	uint16	ncal_ed_crsEn_reg;		/* phyreg(ed_crsEn) */
	uint16	ncal_nvcfg0_reg;		/* LLR deweighting coefficient */
	uint16	ncal_SlnaRxMaskCtrl0_reg;
	uint16	ncal_SlnaRxMaskCtrl1_reg;
	uint16	ncal_CRSMiscellaneousParam_reg;
	uint16	ncal_AntDivConfig2059_reg;
	uint16	ncal_HPFBWovrdigictrl_reg;
	uint16	save_SlnaRxMaskCtrl0_reg;
	uint16	save_SlnaRxMaskCtrl1_reg;
	uint16	asym_intf_ncal_req_chspec;	/* channel request noisecal */
	/* asym_intf_stats includes the following bits:
	* b[0]:   bool asym_intf_rx_noise_mit_on;	// SmartCCA Rx mititagion enabled
	* b[1]:   bool asym_intf_tx_smartcca_on;	// SmartCCA Tx mititagion enabled
	* b[2]:   bool asym_intf_valid_noise_samp;	// Latest noise sample is valid
	* b[3]:   bool asym_intf_fill_noise_buf;	// Fill the same sample to entire buffer
	* b[4]:   bool asym_intf_ncal_discard;		// Discard current noise sample
	* b[5]:   bool slna_reg_saved;			// SLNA register values are saved
	* b[6]:   bool asym_intf_host_ext_usb;		// Host control related variable
	* b[7]:   bool asym_intf_host_ext_usb_chg;	// Host control related variable
	* b[8]:   bool asym_intf_host_en;		// Host control related variable
	* b[9]:   bool asym_intf_host_enable;		// Host control related variable
	* b[10]:  bool asym_intf_pending_host_req;	// Set request pending if clk not present
	*/
	uint16	asym_intf_stats;

	uint8	asym_btc_mode;			/* from bt_desense in gainovr_shm_config() */

	/* noise at antenna from phy_ac_noise_calc() */
	int8	asym_noisecalc_cmplx_pwr_dbm[2];
	int8	asym_intf_ant_noise[2];

	/* LTE asymmetric jammer paramaters */
	bool	asym_intf_jammer_en;
	uint8	asym_intf_jammer_cm;
	int8	asym_intf_jammer_pwr[2];

	int8	asym_intf_avg_noise[2];
	int8	asym_intf_latest_noise[2];
	/* used to calculate noise_delta for rx mitigation on/off */
	int8	asym_intf_prev_noise_lvl[2];

	uint8	asym_intf_tx_smartcca_cm;
	uint8	asym_intf_ant_noise_idx;
	uint8	asym_intf_least_core_idx;
	uint8	asym_intf_pending_host_req_type; /* Set request pending if clk not present */
	uint8	asym_intf_ncal_crs_stat_idx;
	uint16	asym_intf_ncal_crs_stat;

	uint16	debug_scca_01;
} phy_periodic_scca_stats_v255_t;

/* MLO related PHY Logging */
typedef struct phy_periodic_mlo_stats_v1 {
	uint32	emlsr_disable_rsn;	/* eMLSR Disable reason */
	uint32	emlsr_pause_rsn;	/* eMLSR Pause reason */
	uint8	trans_dly;		/* EMLSR transition delay */
	uint8	link_id;		/* link ID - AP managed */
	uint8	dtim_cnt;		/* current DTIM count */
	uint8	dtim_bcn_cnt;		/* DTIM bcn cnt since Crit Upd Ind last set */
	uint8	link_hibernated;	/* Link is in hibernated state : 1 */
	uint8	ml_scan_in_prog;	/* ML scan in progress... : 1 */
	uint8	link_disabled;		/* Link is disabled : 1 */
	uint8	emlsr_state;		/* bsscfg emlsr FSM state */
	uint32	emlsr_ctrl_params;	/* emlsr present ctrl params state */
	uint16	eml_req_txcnt;		/* EML notif req txcnt new attempts */
	uint16	eml_req_tot_retry_cnt;	/* EML notif req total retries */
	uint16	eml_resp_rxcnt;		/* EML notif resp rxcnt */
	uint16	eml_resp_match_rxcnt;	/* EML notif resp all conditions matched */
	uint8	eml_req_retry_cnt;	/* EML Notificaion Request retry count */
	bool	emlsr_en_notif_done;	/* EML Enable Notif complete */
	uint8	recent_st;		/* recent state for EMLSR state machines */
	uint8	recent_ev;		/* recent event for EMLSR state machines */
	bool	link_swap_inprog;	/* eMLSR link swap is in progress */
	bool	non_prim_bcn_rx_failed;	/* eMLSR 2nd link bcn rx failed */
	bool	pref_link_sw_in_prog;
	uint8	debug_01;
} phy_periodic_mlo_stats_v1_t;

/* MLO/EMLSR related PHY Logging */
typedef struct phy_periodic_mlo_stats_v2 {
	uint32	emlsr_disable_rsn;	/* eMLSR Disable reason */
	uint32	emlsr_pause_rsn;	/* eMLSR Pause reason */
	uint8	trans_dly;		/* EMLSR transition delay */
	uint8	link_id;		/* link ID - AP managed */
	uint8	dtim_cnt;		/* current DTIM count */
	uint8	dtim_bcn_cnt;		/* DTIM bcn cnt since Crit Upd Ind last set */
	uint8	link_hibernated;	/* Link is in hibernated state : 1 */
	uint8	ml_scan_in_prog;	/* ML scan in progress... : 1 */
	uint8	link_disabled;		/* Link is disabled : 1 */
	uint8	emlsr_state;		/* bsscfg emlsr FSM state */
	uint32	emlsr_ctrl_params;	/* emlsr present ctrl params state */
	uint16	eml_req_txcnt;		/* EML notif req txcnt new attempts */
	uint16	eml_req_tot_retry_cnt;	/* EML notif req total retries */
	uint16	eml_resp_rxcnt;		/* EML notif resp rxcnt */
	uint16	eml_resp_match_rxcnt;	/* EML notif resp all conditions matched */
	uint8	eml_req_retry_cnt;	/* EML Notificaion Request retry count */
	bool	emlsr_en_notif_done;	/* EML Enable Notif complete */
	uint8	recent_st;		/* recent state for EMLSR state machines */
	uint8	recent_ev;		/* recent event for EMLSR state machines */
	bool	link_swap_inprog;	/* eMLSR link swap is in progress */
	bool	non_prim_bcn_rx_failed;	/* eMLSR 2nd link bcn rx failed */
	bool	pref_link_sw_in_prog;
	uint8	chan_switch_indicator_reg;
	uint16	bank1_regwr_cnt;	/* How many times shadowed-bank1 register is written to */
	uint16	bank1_chanind1_cnt;	/* How many times bank1 regs are written */
	uint32	debug_mlo_01;
} phy_periodic_mlo_stats_v2_t;

/* MLO related PHY Logging */
typedef struct phy_periodic_mlo_stats_v255 {
	uint32	emlsr_disable_rsn;	/* eMLSR Disable reason */
	uint32	emlsr_pause_rsn;	/* eMLSR Pause reason */
	uint8	trans_dly;		/* EMLSR transition delay */
	uint8	link_id;		/* link ID - AP managed */
	uint8	dtim_cnt;		/* current DTIM count */
	uint8	dtim_bcn_cnt;		/* DTIM bcn cnt since Crit Upd Ind last set */
	uint8	link_hibernated;	/* Link is in hibernated state : 1 */
	uint8	ml_scan_in_prog;	/* ML scan in progress... : 1 */
	uint8	link_disabled;		/* Link is disabled : 1 */
	uint8	emlsr_state;		/* bsscfg emlsr FSM state */
	uint32	emlsr_ctrl_params;	/* emlsr present ctrl params state */
	uint16	eml_req_txcnt;		/* EML notif req txcnt new attempts */
	uint16	eml_req_tot_retry_cnt;	/* EML notif req total retries */
	uint16	eml_resp_rxcnt;		/* EML notif resp rxcnt */
	uint16	eml_resp_match_rxcnt;	/* EML notif resp all conditions matched */
	uint8	eml_req_retry_cnt;	/* EML Notificaion Request retry count */
	bool	emlsr_en_notif_done;	/* EML Enable Notif complete */
	uint8	recent_st;		/* recent state for EMLSR state machines */
	uint8	recent_ev;		/* recent event for EMLSR state machines */
	bool	link_swap_inprog;	/* eMLSR link swap is in progress */
	bool	non_prim_bcn_rx_failed;	/* eMLSR 2nd link bcn rx failed */
	bool	pref_link_sw_in_prog;
	uint8	chan_switch_indicator_reg;
	uint16	bank1_regwr_cnt;	/* How many times shadowed-bank1 register is written to */
	uint16	bank1_chanind1_cnt;	/* How many times bank1 regs are written */
	uint32	debug_mlo_01;
} phy_periodic_mlo_stats_v255_t;

/* SRCB related PHY Logging */
typedef struct phy_periodic_srcb_stats_v1 {
	uint32	fem_debug_bus_2g;
	uint32	fem_debug_bus_5g;
	uint32	srmc_5g_debug_bus;
	uint32	srmc_5g_input_status;
	uint32	srmc_5g_output_status;
	uint32	rfactv_reg;
	uint32	currenttrigger;
	uint32	previous2triggers;
	uint32	previous4triggers;
	uint32	rfseq2rdig_debug_1;
	uint32	rfseq2rdig_debug_2;
	uint32	rfseq2rdig_debug_3;
	uint32	rfseq2rdig_debug_4;
	uint32	rfseq2rdig_debug_5;
	uint32	rfseq2rdig_debug_6;
	uint32	rfseq2rdig_debug_7;
	uint32	rfseq2rdig_debug_8;
	uint32	srcb_srmc_2g_input_status;
	uint32	srcb_srmc_2g_input_status_2;
	uint32	srcb_srmc_2g_output_status;
	uint32	srcb_srmc_grant;
	uint32	srcb_srmc_2g_output_status_2;
	uint32	srcb_srmc_2g_debug_bus_1;
	uint32	dacclkstatus;
	uint32	adcclkstatus;
	uint32	ocl_mode_enable;
	uint32	rfpll_2g_config28;
	uint32	pll_2g_need_refresh;
	uint16	srcb_2g_sem_cnt;	/* total 2g semaphore acquisition count */
	uint16	srcb_5g_sem_cnt;	/* total 5g semaphore acquisition count */
	uint16	srcb_2g_sem_cnt_bt;	/* total 2g semaphore acquisition count from bt side */
	uint16	srcb_5g_sem_cnt_bt;	/* total 5g semaphore acquisition count from bt side */
	uint32	srcb_2g_sem_acc_dur_us;	/* accumulated duration of 2g semaphore */
	uint32	srcb_2g_sem_max_dur_us;	/* max duration of single 2g semaphore acquisition */
	uint32	srcb_5g_sem_acc_dur_us;	/* accumulated duration of 5g semaphore */
	uint32	srcb_5g_sem_max_dur_us;	/* max duration of single 5g semaphore acquisition */
	uint32	init_done_wl;		/* sra init done status bit in WL side */
	uint32	init_done_bt;		/* sra init done status bit in BT side */
	uint32	gci_sem_reserved_cnt;	/* counter of gci semaphore reserving and waiting */
	uint8	srcb_sem_id;		/* current acquired semaphore id */
	bool	initsts_incomp;		/* init status incomplete warning flag */
	uint8	total_2g_cal_retries;	/* number of retries for 2g cals */
	uint8	debug_02;		/* reserved for debugging */
} phy_periodic_srcb_stats_v1_t;

typedef struct phy_periodic_srcb_stats_v2 {
	uint32	fem_debug_bus_2g_gci;
	uint32	fem_debug_bus_5g_gci;
	uint32	srmc_5g_debug_bus_reg;
	uint32	srmc_5g_input_status_reg;
	uint32	srmc_5g_output_status_reg;
	uint32	rfactv_reg;
	uint32	currenttrigger_reg;
	uint32	previous2triggers_reg;
	uint32	previous4triggers_reg;
	uint32	rfseq2rdig_debug_1_reg;
	uint32	rfseq2rdig_debug_2_reg;
	uint32	rfseq2rdig_debug_3_reg;
	uint32	rfseq2rdig_debug_4_reg;
	uint32	rfseq2rdig_debug_5_reg;
	uint32	rfseq2rdig_debug_6_reg;
	uint32	rfseq2rdig_debug_7_reg;
	uint32	rfseq2rdig_debug_8_reg;
	uint32	srcb_srmc_2g_input_status_reg;
	uint32	srcb_srmc_2g_input_status_2_reg;
	uint32	srcb_srmc_2g_output_status_reg;
	uint32	srcb_srmc_grant_reg;
	uint32	srcb_srmc_2g_output_status_2_reg;
	uint32	srcb_srmc_2g_debug_bus_1_reg;
	uint32	dacclkstatus_reg;
	uint32	adcclkstatus_reg;
	uint32	ocl_mode_enable_reg;
	uint32	rfpll_2g_config28_reg;
	uint32	pll_2g_need_refresh_reg;
	uint16	srcb_2g_sem_cnt;	/* total 2g semaphore acquisition count */
	uint16	srcb_5g_sem_cnt;	/* total 5g semaphore acquisition count */
	uint16	srcb_2g_sem_cnt_bt;	/* total 2g semaphore acquisition count from bt side */
	uint16	srcb_5g_sem_cnt_bt;	/* total 5g semaphore acquisition count from bt side */
	uint32	srcb_2g_sem_acc_dur_us;	/* accumulated duration of 2g semaphore */
	uint32	srcb_2g_sem_max_dur_us;	/* max duration of single 2g semaphore acquisition */
	uint32	srcb_5g_sem_acc_dur_us;	/* accumulated duration of 5g semaphore */
	uint32	srcb_5g_sem_max_dur_us;	/* max duration of single 5g semaphore acquisition */
	uint32	init_done_wl_reg;	/* sra init done status bit in WL side */
	uint32	init_done_bt_reg;	/* sra init done status bit in BT side */
	uint32	gci_sem_reserved_cnt;	/* counter of gci semaphore reserving and waiting */
	uint8	srcb_sem_id;		/* current acquired semaphore id */
	bool	initsts_incomp;		/* init status incomplete warning flag */
	uint8	total_2g_cal_retries;	/* number of retries for 2g cals */
	uint8	debug_srcb_01;		/* reserved for debugging */
	uint32	debug_srcb_02;		/* reserved for debugging */
} phy_periodic_srcb_stats_v2_t;

/* SRCB related PHY Logging */
typedef struct phy_periodic_srcb_stats_v255 {
	uint32	fem_debug_bus_2g_gci;
	uint32	fem_debug_bus_5g_gci;
	uint32	srmc_5g_debug_bus_reg;
	uint32	srmc_5g_input_status_reg;
	uint32	srmc_5g_output_status_reg;
	uint32	rfactv_reg;
	uint32	currenttrigger_reg;
	uint32	previous2triggers_reg;
	uint32	previous4triggers_reg;
	uint32	rfseq2rdig_debug_1_reg;
	uint32	rfseq2rdig_debug_2_reg;
	uint32	rfseq2rdig_debug_3_reg;
	uint32	rfseq2rdig_debug_4_reg;
	uint32	rfseq2rdig_debug_5_reg;
	uint32	rfseq2rdig_debug_6_reg;
	uint32	rfseq2rdig_debug_7_reg;
	uint32	rfseq2rdig_debug_8_reg;
	uint32	srcb_srmc_2g_input_status_reg;
	uint32	srcb_srmc_2g_input_status_2_reg;
	uint32	srcb_srmc_2g_output_status_reg;
	uint32	srcb_srmc_grant_reg;
	uint32	srcb_srmc_2g_output_status_2_reg;
	uint32	srcb_srmc_2g_debug_bus_1_reg;
	uint32	dacclkstatus_reg;
	uint32	adcclkstatus_reg;
	uint32	ocl_mode_enable_reg;
	uint32	rfpll_2g_config28_reg;
	uint32	pll_2g_need_refresh_reg;
	uint16	srcb_2g_sem_cnt;	/* total 2g semaphore acquisition count */
	uint16	srcb_5g_sem_cnt;	/* total 5g semaphore acquisition count */
	uint16	srcb_2g_sem_cnt_bt;	/* total 2g semaphore acquisition count from bt side */
	uint16	srcb_5g_sem_cnt_bt;	/* total 5g semaphore acquisition count from bt side */
	uint32	srcb_2g_sem_acc_dur_us;	/* accumulated duration of 2g semaphore */
	uint32	srcb_2g_sem_max_dur_us;	/* max duration of single 2g semaphore acquisition */
	uint32	srcb_5g_sem_acc_dur_us;	/* accumulated duration of 5g semaphore */
	uint32	srcb_5g_sem_max_dur_us;	/* max duration of single 5g semaphore acquisition */
	uint32	init_done_wl_reg;	/* sra init done status bit in WL side */
	uint32	init_done_bt_reg;	/* sra init done status bit in BT side */
	uint32	gci_sem_reserved_cnt;	/* counter of gci semaphore reserving and waiting */
	uint8	srcb_sem_id;		/* current acquired semaphore id */
	bool	initsts_incomp;		/* init status incomplete warning flag */
	uint8	total_2g_cal_retries;	/* number of retries for 2g cals */
	uint8	debug_srcb_01;		/* reserved for debugging */
	uint32	debug_srcb_02;		/* reserved for debugging */
} phy_periodic_srcb_stats_v255_t;

/* FBC related PHY Logging */
typedef struct phy_periodic_fbc_stats_v1 {
	uint32	multiphasecalcntr;	/* count of performing multi-phase cal */
	uint32	last_cal_time;
	uint16	fbc_channels;		/* Channels parked on */
	uint16	fbc_starts;		/* Indicates frame starts */
	uint16	fbc_detect[3];		/* Indicates frame detections */
	uint16	fbc_good_fcs[3];	/* Indicates good FCS Rx counter */
	uint16	fbc_bad_fcs;		/* Indicates bad FCS Rx counter */
	uint16	fbc_be_busy;		/* Indicates hardware busy */
	uint16	fbc_errors;		/* Indicates error counter */
	int16	last_cal_temp;
	uint8	cal_phase_id;
	uint8	cal_reason;
	uint16	debug_01;
} phy_periodic_fbc_stats_v1_t;

#define PHY_SCAN_MULTI_NUM_FE 4u
typedef struct phy_periodic_fbc_stats_v255 {
	uint32	fbc_multiphasecalcntr;	/* count of performing multi-phase cal */
	uint32	fbc_last_cal_time;
	uint16	fbc_channels;		/* Channels parked on */
	uint16	fbc_starts;		/* Indicates frame starts */
	uint16	fbc_detect[PHY_SCAN_MULTI_NUM_FE];	/* Indicates frame detections */
	uint16	fbc_good_fcs[PHY_SCAN_MULTI_NUM_FE];	/* Indicates good FCS Rx counter */
	uint16	fbc_bad_fcs;		/* Indicates bad FCS Rx counter */
	uint16	fbc_be_busy;		/* Indicates hardware busy */
	uint16	fbc_errors;		/* Indicates error counter */
	int16	fbc_last_cal_temp;
	uint8	fbc_cal_phase_id;
	uint8	fbc_cal_reason;
	uint16	fbc_channel_mask;
} phy_periodic_fbc_stats_v255_t;

typedef struct phy_periodic_vcocal_stats_v1 {
	uint16	debug_cal_code_main_slice;
	uint16	pll_2g_vcocal_calcaprb;
	uint16	dpll_lp_dig_observation[8];
	uint8	force_pu_cnt_ded;		/* force PU retry cnt as part of ded cal */
	uint8	force_pu_cnt_need_refresh;	/* force PU retry cnt as part of need refresh */
	bool	force_pu_req_need_refresh;	/* force PU required as part of need refresh */
	uint8	force_pu_cnt_fbc;		/* force PU retry cnt as part of FBC cal */
} phy_periodic_vcocal_stats_v1_t;

typedef struct phy_periodic_vcocal_stats_v2 {
	uint16	debug_cal_code_main_slice;
	uint16	pll_2g_vcocal_calcaprb;
	uint16	dpll_lp_dig_observation[8];
	uint8	force_pu_cnt_ded;		/* force PU retry cnt as part of ded cal */
	uint8	force_pu_cnt_need_refresh;	/* force PU retry cnt as part of need refresh */
	bool	force_pu_req_need_refresh;	/* force PU required as part of need refresh */
	uint8	force_pu_cnt_fbc;		/* force PU retry cnt as part of FBC cal */
	uint8	vcocalitr_cnt;
	uint8	debug_vcocal_01;
	uint16	debug_vcocal_02;
} phy_periodic_vcocal_stats_v2_t;

typedef struct phy_periodic_vcocal_stats_v255 {
	uint16	debug_cal_code_main_slice;
	uint16	pll_2g_vcocal_calcaprb;
	uint16	dpll_lp_dig_observation[8];
	uint8	force_pu_cnt_ded;		/* force PU retry cnt as part of ded cal */
	uint8	force_pu_cnt_need_refresh;	/* force PU retry cnt as part of need refresh */
	bool	force_pu_req_need_refresh;	/* force PU required as part of need refresh */
	uint8	force_pu_cnt_fbc;		/* force PU retry cnt as part of FBC cal */
	uint8	vcocalitr_cnt;
	uint8	debug_vcocal_01;
	uint16	debug_vcocal_02;
} phy_periodic_vcocal_stats_v255_t;

#define FBCX_STATS_PHY_LOGGING_VER1	1u
typedef struct wlc_fbcx_stats_phy_logging_v1 {
	uint32	fbcx_ovd_cnt;		/**< # of times FBC override applied */
	uint32	fbcx_last_ovd_start_ts;	/**< last FBC ovd start TS */
	uint32	fbcx_last_ovd_end_ts;	/**< last FBC ovd end TS */
	uint32	fbcx_ovd_dur;		/**< accumulated FBC ovd duration */
	uint32	fbcx_actv_cfg;		/**< FBC ovd active conditions */
	uint32	fbcx_cfg;		/* global FBC config flag */
} phy_periodic_fbcx_stats_v1_t;

#define FBCX_STATS_PHY_LOGGING_VER2	2u
typedef struct wlc_fbcx_stats_phy_logging_v2 {
	uint32	fbcx_ovd_cnt;		/**< # of times FBC override applied */
	uint32	fbcx_last_ovd_start_ts;	/**< last FBC ovd start TS */
	uint32	fbcx_last_ovd_end_ts;	/**< last FBC ovd end TS */
	uint32	fbcx_ovd_dur;		/**< accumulated FBC ovd duration */
	uint32	fbcx_actv_cfg;		/**< FBC ovd active conditions */
	uint32	fbcx_cfg;		/* global FBC config flag */

	uint16  fbaci_status_idx[4];    /**< FBAGC ACI status idx */
	uint16  fbaci_nsamples_idx[4];  /**< FBAGC ACI nsamples idx */

	uint32	pmu_mode_switch_time;
	uint16	pmu_mode_switch_cnt;
	uint16	pmu_force_pwm_cnt;
	uint16	pmu_auto_mode_cnt;
	uint16	fpwm_ovrd_reason_bitmap;
	uint8	pmu_mode;
	uint8	traffic_valid_bitmap;
	uint16	debug_01;
	uint32	debug_02;
} phy_periodic_fbcx_stats_v2_t;

#define FBCX_STATS_PHY_LOGGING_VER3	3u
typedef struct wlc_fbcx_stats_phy_logging_v3 {
	/* Params for CxCPU based BT Coex scheme */
	uint32	fbcx_ovd_cnt;		/**< # of times FBC override applied */
	uint32	fbcx_last_ovd_start_ts_us;	/**< last FBC ovd start TS */
	uint32	fbcx_last_ovd_end_ts_us;	/**< last FBC ovd end TS */
	uint32	fbcx_ovd_dur_us;	/**< accumulated FBC ovd duration */
	uint32	fbcx_actv_cfg;		/**< FBC ovd active conditions */
	uint32	fbcx_cfg;		/* global FBC config flag */

	/* Params for Force PWM ABUCK feature */
	uint32	fpwm_pmu_mode_switch_time_us;
	uint16	fpwm_pmu_mode_switch_cnt;
	uint16	fpwm_pmu_force_pwm_cnt;
	uint16	fpwm_pmu_auto_mode_cnt;
	uint16	fpwm_ovrd_reason_bitmap;
	uint8	fpwm_pmu_mode;
	uint8	fpwm_traffic_valid_bitmap;

	/* Params for FB-ACI feature */
	uint16	fbaci_blkr_entry_exit_cnt;
	uint32	fbaci_aux_samp_cnt;
	uint16  fbaci_status_idx[4];    /**< FBAGC ACI status idx */
	uint16  fbaci_nsamples_idx[4];  /**< FBAGC ACI nsamples idx */

	uint32	debug_fbcx_01;
	uint32	debug_fbcx_02;
} phy_periodic_fbcx_stats_v3_t;

#define FBCX_STATS_PHY_LOGGING_VER255	255u
typedef struct wlc_fbcx_stats_phy_logging_v255 {
	/* Params for CxCPU based BT Coex scheme */
	uint32	fbcx_ovd_cnt;		/**< # of times FBC override applied */
	uint32	fbcx_last_ovd_start_ts_us;	/**< last FBC ovd start TS */
	uint32	fbcx_last_ovd_end_ts_us;	/**< last FBC ovd end TS */
	uint32	fbcx_ovd_dur_us;	/**< accumulated FBC ovd duration */
	uint32	fbcx_actv_cfg;		/**< FBC ovd active conditions */
	uint32	fbcx_cfg;		/* global FBC config flag */

	/* Params for Force PWM ABUCK feature */
	uint32	fpwm_pmu_mode_switch_time_us;
	uint16	fpwm_pmu_mode_switch_cnt;
	uint16	fpwm_pmu_force_pwm_cnt;
	uint16	fpwm_pmu_auto_mode_cnt;
	uint16	fpwm_ovrd_reason_bitmap;
	uint8	fpwm_pmu_mode;
	uint8	fpwm_traffic_valid_bitmap;

	/* Params for FB-ACI feature */
	uint16	fbaci_blkr_entry_exit_cnt;
	uint32	fbaci_aux_samp_cnt;
	uint16  fbaci_status_idx[4];    /**< FBAGC ACI status idx */
	uint16  fbaci_nsamples_idx[4];  /**< FBAGC ACI nsamples idx */

	uint32	debug_fbcx_01;
} phy_periodic_fbcx_stats_v255_t;

typedef struct phy_periodic_sra_stats_v1 {
	uint16	sr_crash_reason_bt;	/* Last BT fw crash reason */
	uint16	sr_crash_reason_wl;	/* Last WL fw crash reason */
	uint16	sr_crash_counter_bt;	/* BT fw crash counter */
	uint16	sr_crash_counter_wl;	/* WL fw crash counter */
	uint16	sr_calreq_counter_bt;	/* BT Cal Request counter */
	uint16	sr_tempreq_counter_bt;	/* BT Temp Request counter */
	uint16	sr_crit_region_2g;	/* 2G Sema Crit region counter */
	uint16	sr_crit_region_5g;	/* 5G Sema Crit region counter */
	uint8	sr_boot_count;		/* Boots without power cycle or REG ON */
	uint8	sr_softrecovery_count;	/* Soft recoveries */
	uint8	sr_bt_param1;		/* BT param 1 */
	uint8	sr_bt_param2;		/* BT param 2 */
	uint32	debug_01;
} phy_periodic_sra_stats_v1_t;

#define SEM_MAX			2u	/* max sem idx */
typedef struct phy_periodic_sra_stats_v2 {
	uint16	sr_notif_counter_bt;	/* BT fw crash counter */
	uint16	sr_notif_state_bt;	/* Last BT fw notif state */
	uint32	sr_notif_option_bt;	/* Last BT fw notif reason */
	uint16	sr_notif_counter_wl;	/* WL fw crash counter */
	uint16	sr_notif_state_wl;	/* Last WL fw crash reason */
	uint16	sr_calreq_counter_bt;	/* BT Cal Request counter */
	uint16	sr_tempreq_counter_bt;	/* BT Temp Request counter */
	uint16	sr_crit_region_wl[SEM_MAX]; /* 2, 5G Sema Crit region counter */
	uint16	sr_crit_region_bt[SEM_MAX]; /* 2, 5G Sema Crit region counter */
	uint8	sr_boot_count;		/* Boots without power cycle or REG ON */
	uint8	sr_softrecovery_count;	/* Soft recoveries */
	uint8	phy_crash_rc;		/* Critical region crash reason code */
	uint8	phy_crash_boot_count;	/* Boot count when critical region crash happened */
	uint16	sr_param2;		/* SR param 2 */
	uint16	sr_param3;		/* SR param 3 */
} phy_periodic_sra_stats_v2_t;

typedef struct phy_periodic_sra_stats_v3 {
	uint16	sr_notif_counter_bt;	/* BT fw crash counter */
	uint16	sr_notif_state_bt;	/* Last BT fw notif state */
	uint32	sr_notif_option_bt;	/* Last BT fw notif reason */
	uint16	sr_notif_counter_wl;	/* WL fw crash counter */
	uint16	sr_notif_state_wl;	/* Last WL fw crash reason */
	uint16	sr_calreq_counter_bt;	/* BT Cal Request counter */
	uint16	sr_tempreq_counter_bt;	/* BT Temp Request counter */
	uint16	sr_crit_region_wl[SEM_MAX]; /* 2, 5G Sema Crit region counter */
	uint16	sr_crit_region_bt[SEM_MAX]; /* 2, 5G Sema Crit region counter */
	uint8	sr_boot_count;		/* Boots without power cycle or REG ON */
	uint8	sr_softrecovery_count;	/* Soft recoveries */
	uint8	phy_crash_rc;		/* Critical region crash reason code */
	uint8	phy_crash_boot_count;	/* Boot count when critical region crash happened */
	uint16	sr_bootstat_bt;		/* BT boot stat */
	uint16	debug_sra_01;
	uint32	debug_sra_02;
	uint32	debug_sra_03;
	uint32	debug_sra_04;
} phy_periodic_sra_stats_v3_t;

typedef struct phy_periodic_sra_stats_v255 {
	uint16	sr_notif_counter_bt;	/* BT fw crash counter */
	uint16	sr_notif_state_bt;	/* Last BT fw notif state */
	uint32	sr_notif_option_bt;	/* Last BT fw notif reason */
	uint16	sr_notif_counter_wl;	/* WL fw crash counter */
	uint16	sr_notif_state_wl;	/* Last WL fw crash reason */
	uint16	sr_calreq_counter_bt;	/* BT Cal Request counter */
	uint16	sr_tempreq_counter_bt;	/* BT Temp Request counter */
	uint16	sr_crit_region_wl[SEM_MAX]; /* 2, 5G Sema Crit region counter */
	uint16	sr_crit_region_bt[SEM_MAX]; /* 2, 5G Sema Crit region counter */
	uint8	sr_boot_count;		/* Boots without power cycle or REG ON */
	uint8	sr_softrecovery_count;	/* Soft recoveries */
	uint8	phy_crash_rc;		/* Critical region crash reason code */
	uint8	phy_crash_boot_count;	/* Boot count when critical region crash happened */
	uint16	sr_bootstat_bt;		/* BT boot stat */
	uint16	debug_sra_01;
	uint32	debug_sra_02;
} phy_periodic_sra_stats_v255_t;

#define PHY_PERIODIC_LOG_VER1         (1u)

typedef struct phy_periodic_log_v1 {
	uint8  version; /* Logging structure version */
	uint8  numcores; /* Number of cores for which core specific data present */
	uint16 length;  /* Length of the entire structure */
	phy_periodic_log_cmn_t phy_perilog_cmn;
	phy_periodic_counters_v1_t counters_peri_log;
	phy_periodic_log_core_t phy_perilog_core[1];
} phy_periodic_log_v1_t;

#define PHYCAL_LOG_VER3		(3u)
#define PHY_PERIODIC_LOG_VER3	(3u)

/* 4387 onwards */
typedef struct phy_periodic_log_v3 {
	uint8  version; /* Logging structure version */
	uint8  numcores; /* Number of cores for which core specific data present */
	uint16 length;  /* Length of the structure */

	/* Logs general PHY parameters */
	phy_periodic_log_cmn_v2_t phy_perilog_cmn;

	/* Logs ucode counters and NAVs */
	phy_periodic_counters_v3_t counters_peri_log;

	/* Logs data pertaining to each core */
	phy_periodic_log_core_t phy_perilog_core[BCM_FLEX_ARRAY];
} phy_periodic_log_v3_t;

#define PHY_PERIODIC_LOG_VER5	(5u)

typedef struct phy_periodic_log_v5 {
	uint8  version; /* Logging structure version */
	uint8  numcores; /* Number of cores for which core specific data present */
	uint16 length;  /* Length of the structure */

	/* Logs general PHY parameters */
	phy_periodic_log_cmn_v4_t phy_perilog_cmn;

	/* Logs ucode counters and NAVs */
	phy_periodic_counters_v3_t counters_peri_log;

	/* Logs data pertaining to each core */
	phy_periodic_log_core_v3_t phy_perilog_core[BCM_FLEX_ARRAY];
} phy_periodic_log_v5_t;

#define PHY_PERIODIC_LOG_VER6	(6u)

typedef struct phy_periodic_log_v6 {
	uint8  version; /* Logging structure version */
	uint8  numcores; /* Number of cores for which core specific data present */
	uint16 length;  /* Length of the structure */

	/* Logs general PHY parameters */
	phy_periodic_log_cmn_v5_t phy_perilog_cmn;

	/* Logs ucode counters and NAVs */
	phy_periodic_counters_v6_t counters_peri_log;

	/* log data for BTcoex */
	phy_periodic_btc_stats_v1_t phy_perilog_btc_stats;

	/* Logs data pertaining to each core */
	phy_periodic_log_core_v4_t phy_perilog_core[BCM_FLEX_ARRAY];
} phy_periodic_log_v6_t;

typedef struct phycal_log_v3 {
	uint8  version; /* Logging structure version */
	uint8  numcores; /* Number of cores for which core specific data present */
	uint16 length;  /* Length of the entire structure */
	phycal_log_cmn_v2_t phycal_log_cmn; /* Logging common structure */
	/* This will be a variable length based on the numcores field defined above */
	phycal_log_core_v3_t phycal_log_core[BCM_FLEX_ARRAY];
} phycal_log_v3_t;

#define PHY_CAL_EVENTLOG_VER4		(4u)
typedef struct phycal_log_v4 {
	uint8  version; /* Logging structure version */
	uint8  numcores; /* Number of cores for which core specific data present */
	uint16 length;  /* Length of the entire structure */
	phy_phycal_v2_t phy_calibration;
} phycal_log_v4_t;

#define PHY_CAL_EVENTLOG_VER5		(5u)
#define PHY_CAL_EVENTLOG_VER5_SIZE	544u
typedef struct phycal_log_v5 {
	uint8  version; /* Logging structure version */
	uint8  numcores; /* Number of cores for which core specific data present */
	uint16 length;  /* Length of the entire structure */
	phy_phycal_v3_t phy_calibration;
} phycal_log_v5_t;

/* For 27.10 */
#define PHY_CAL_EVENTLOG_VER6		(6u)
typedef struct phycal_log_v6 {
	uint8  version; /* Logging structure version */
	uint8  numcores; /* Number of cores for which core specific data present */
	uint16 length;  /* Length of the entire structure */
	phy_phycal_v4_t phy_calibration;
} phycal_log_v6_t;

/* Trunk ONLY */
#define PHY_CAL_EVENTLOG_VER255		(255u)
#define PHY_CAL_EVENTLOG_VER255_SIZE	552u
typedef struct phycal_log_v255 {
	uint8  version; /* Logging structure version */
	uint8  numcores; /* Number of cores for which core specific data present */
	uint16 length;  /* Length of the entire structure */
	phy_phycal_v255_t phy_calibration;
} phycal_log_v255_t;

/* Note: The version 2 is reserved for 4357 only. Future chips must not use this version. */

#define MAX_CORE_4357		(2u)
#define PHYCAL_LOG_VER2		(2u)
#define PHY_PERIODIC_LOG_VER2	(2u)

typedef struct {
	uint32	txallfrm;	/**< total number of frames sent, incl. Data, ACK, RTS, CTS,
				* Control Management (includes retransmissions)
				*/
	uint32	rxrsptmout;	/**< number of response timeouts for transmitted frames
				* expecting a response
				*/
	uint32	rxstrt;		/**< number of received frames with a good PLCP */
	uint32  rxbadplcp;	/**< number of parity check of the PLCP header failed */
	uint32  rxcrsglitch;	/**< PHY was able to correlate the preamble but not the header */
	uint32  bphy_badplcp;	/**< number of bad PLCP reception on BPHY rate */
	uint32  bphy_rxcrsglitch;	/**< PHY count of bphy glitches */
	uint32	rxbeaconmbss;	/**< beacons received from member of BSS */
	uint32	rxdtucastmbss;	/**< number of received DATA frames with good FCS and matching RA */
	uint32  rxf0ovfl;	/** < Rx FIFO0 overflow counters information */
	uint32  rxf1ovfl;	/** < Rx FIFO1 overflow counters information */
	uint32	rxdtocast;	/**< number of received DATA frames (good FCS and no matching RA) */
	uint32  rxtoolate;	/**< receive too late */
	uint32  rxbadfcs;	/**< number of frames for which the CRC check failed in the MAC */
} phy_periodic_counters_v2_t;

/* Note: The version 2 is reserved for 4357 only. All future chips must not use this version. */

typedef struct phycal_log_core_v2 {
	uint16 ofdm_txa; /* OFDM Tx IQ Cal a coeff */
	uint16 ofdm_txb; /* OFDM Tx IQ Cal b coeff */
	uint16 ofdm_txd; /* contain di & dq */
	uint16 rxa; /* Rx IQ Cal A coeffecient */
	uint16 rxb; /* Rx IQ Cal B coeffecient */
	uint8 baseidx; /* TPC Base index */
	uint8 pad;
	int32 rxs; /* FDIQ Slope coeffecient */
} phycal_log_core_v2_t;

/* Note: The version 2 is reserved for 4357 only. All future chips must not use this version. */

typedef struct phycal_log_v2 {
	uint8  version; /* Logging structure version */
	uint16 length;  /* Length of the entire structure */
	uint8 pad;
	phycal_log_cmn_t phycal_log_cmn; /* Logging common structure */
	phycal_log_core_v2_t phycal_log_core[MAX_CORE_4357];
} phycal_log_v2_t;

/* Note: The version 2 is reserved for 4357 only. All future chips must not use this version. */

typedef struct phy_periodic_log_v2 {
	uint8  version; /* Logging structure version */
	uint16 length;  /* Length of the entire structure */
	uint8 pad;
	phy_periodic_log_cmn_t phy_perilog_cmn;
	phy_periodic_counters_v2_t counters_peri_log;
	phy_periodic_log_core_t phy_perilog_core[MAX_CORE_4357];
} phy_periodic_log_v2_t;

#define PHY_PERIODIC_LOG_VER4	(4u)

/*
 * Note: The version 4 is reserved for 4357 Deafness Debug only.
 * All future chips must not use this version.
 */
typedef struct phy_periodic_log_v4 {
	uint8  version; /* Logging structure version */
	uint8  pad;
	uint16 length;  /* Length of the entire structure */
	phy_periodic_log_cmn_v3_t  phy_perilog_cmn;
	phy_periodic_counters_v4_t counters_peri_log;
	phy_periodic_log_core_v2_t phy_perilog_core[MAX_CORE_4357];
} phy_periodic_log_v4_t;

#define PHY_PERIODIC_LOG_VER7		(7u)
typedef struct phy_periodic_log_v7 {
	uint8  version;		/* Logging structure version */
	uint8  numcores;	/* Number of cores for which core specific data present */
	uint16 length;		/* Length of the entire structure */
	phy_periodic_log_cmn_t phy_perilog_cmn;
	phy_periodic_counters_v5_t counters_peri_log;
	phy_periodic_btc_stats_v1_t btc_stats_peri_log;
	/* This will be a variable length based on the numcores field defined above */
	phy_periodic_log_core_t phy_perilog_core[BCM_FLEX_ARRAY];
} phy_periodic_log_v7_t;

#define PHY_PERIODIC_LOG_VER8		(8u)
typedef struct phy_periodic_log_v8 {
	uint8  version;		/* Logging structure version */
	uint8  numcores;	/* Number of cores for which core specific data present */
	uint16 length;		/* Length of the entire structure */

	/* Logs general PHY parameters */
	phy_periodic_log_cmn_v6_t phy_perilog_cmn;

	/* Logs ucode counters and NAVs */
	phy_periodic_counters_v7_t counters_peri_log;

	/* log data for BTcoex */
	phy_periodic_btc_stats_v1_t btc_stats_peri_log;

	/* Logs data pertaining to each core */
	phy_periodic_log_core_v5_t phy_perilog_core[BCM_FLEX_ARRAY];
} phy_periodic_log_v8_t;

#define PHY_PERIODIC_LOG_VER9	(9u)
typedef struct phy_periodic_log_v9 {
	uint8  version;		/* Logging structure version */
	uint8  numcores;	/* Number of cores for which core specific data present */
	uint16 length;		/* Length of the structure */

	/* Logs general PHY parameters */
	phy_periodic_log_cmn_v7_t phy_perilog_cmn;

	/* Logs ucode counters and NAVs */
	phy_periodic_counters_v8_t counters_peri_log;

	/* log data for BTcoex */
	phy_periodic_btc_stats_v1_t phy_perilog_btc_stats;

	/* Logs data pertaining to each core */
	phy_periodic_log_core_v4_t phy_perilog_core[BCM_FLEX_ARRAY];
} phy_periodic_log_v9_t;

#define PHY_PERIODIC_LOG_VER10	10u
typedef struct phy_periodic_log_v10 {
	uint8  version;		/* Logging structure version */
	uint8  numcores;	/* Number of cores for which core specific data present */
	uint16 length;		/* Length of the entire structure */

	/* Logs general PHY parameters */
	phy_periodic_log_cmn_v6_t phy_perilog_cmn;

	/* Logs ucode counters and NAVs */
	phy_periodic_counters_v7_t counters_peri_log;

	/* log data for BTcoex */
	phy_periodic_btc_stats_v1_t btc_stats_peri_log;

	/* Logs data pertaining to each core */
	phy_periodic_log_core_v5_t phy_perilog_core[2];

	/* log data for smartCCA */
	phy_periodic_scca_stats_v1_t scca_counters_peri_log;
} phy_periodic_log_v10_t;

#define PHY_PERIODIC_LOG_VER11	(11u)
typedef struct phy_periodic_log_v11 {
	uint8  version;		/* Logging structure version */
	uint8  numcores;	/* Number of cores for which core specific data present */
	uint16 length;		/* Length of the structure */

	/* Logs general PHY parameters */
	phy_periodic_log_cmn_v8_t phy_perilog_cmn;

	/* Logs ucode counters and NAVs */
	phy_periodic_counters_v8_t counters_peri_log;

	/* log data for BTcoex */
	phy_periodic_btc_stats_v1_t phy_perilog_btc_stats;

	/* Logs data pertaining to each core */
	phy_periodic_log_core_v6_t phy_perilog_core[2];

	/* log data for smartCCA */
	phy_periodic_scca_stats_v3_t scca_counters_peri_log;
} phy_periodic_log_v11_t;

#define AMT_MATCH_INFRA_BSSID	(1 << 0)
#define AMT_MATCH_INFRA_MYMAC	(1 << 1)

#define PHY_PERIODIC_LOG_VER20	20u
typedef struct phy_periodic_log_v20 {
	uint8  version;		/* Logging structure version */
	uint8  numcores;	/* Number of cores for which core specific data present */
	uint16 length;		/* Length of the structure */

	/* Logs general PHY parameters */
	phy_periodic_log_cmn_v9_t phy_perilog_cmn;

	/* Logs ucode counters and NAVs */
	phy_periodic_counters_v8_t counters_peri_log;

	/* log data for BTcoex */
	phy_periodic_btc_stats_v1_t phy_perilog_btc_stats;

	/* log data for obss/dynbw */
	phy_periodic_obss_stats_v1_t phy_perilog_obss_stats;

	/* Logs data pertaining to each core */
	phy_periodic_log_core_v6_t phy_perilog_core[2];

	/* log data for smartCCA */
	phy_periodic_scca_stats_v2_t scca_counters_peri_log;
} phy_periodic_log_v20_t;

#define PHY_PERIODIC_LOG_VER21	21u
typedef struct phy_periodic_log_v21 {
	uint8  version;		/* Logging structure version */
	uint8  numcores;	/* Number of cores for which core specific data present */
	uint16 length;		/* Length of the structure */

	/* Logs general PHY parameters */
	phy_periodic_log_cmn_v9_t phy_perilog_cmn;

	/* Logs ucode counters and NAVs */
	phy_periodic_counters_v8_t counters_peri_log;

	/* log data for BTcoex */
	phy_periodic_btc_stats_v1_t phy_perilog_btc_stats;

	/* log data for obss/dynbw */
	phy_periodic_obss_stats_v1_t phy_perilog_obss_stats;

	/* Logs data pertaining to each core */
	phy_periodic_log_core_v6_t phy_perilog_core[2];

	/* log data for smartCCA */
	phy_periodic_scca_stats_v2_t scca_counters_peri_log;
} phy_periodic_log_v21_t;

#define PHY_PERIODIC_LOG_VER22	22u
typedef struct phy_periodic_log_v22 {
	uint8  version;		/* Logging structure version */
	uint8  numcores;	/* Number of cores for which core specific data present */
	uint16 length;		/* Length of the structure */

	/* Logs general PHY parameters */
	phy_periodic_log_cmn_v10_t phy_perilog_cmn;

	/* Logs ucode counters and NAVs */
	phy_periodic_counters_v9_t counters_peri_log;

	/* log data for BTcoex */
	phy_periodic_btc_stats_v2_t phy_perilog_btc_stats;

	/* log data for obss/dynbw */
	phy_periodic_obss_stats_v1_t phy_perilog_obss_stats;

	/* Logs data pertaining to each core */
	phy_periodic_log_core_v7_t phy_perilog_core[2];

	/* log data for smartCCA */
	phy_periodic_scca_stats_v2_t scca_counters_peri_log;
} phy_periodic_log_v22_t;

#define PHY_PERIODIC_LOG_VER23	23u
typedef struct phy_periodic_log_v23 {
	uint8  version;		/* Logging structure version */
	uint8  numcores;	/* Number of cores for which core specific data present */
	uint16 length;		/* Length of the structure */

	/* Logs general PHY parameters */
	phy_periodic_log_cmn_v10_t phy_perilog_cmn;

	/* Logs ucode counters and NAVs */
	phy_periodic_counters_v9_t counters_peri_log;

	/* log data for BTcoex */
	phy_periodic_btc_stats_v2_t phy_perilog_btc_stats;

	/* log data for obss/dynbw */
	phy_periodic_obss_stats_v1_t phy_perilog_obss_stats;

	/* Logs data pertaining to each core */
	phy_periodic_log_core_v7_t phy_perilog_core[2];

	/* log data for smartCCA */
	phy_periodic_scca_stats_v3_t scca_counters_peri_log;
} phy_periodic_log_v23_t;

#define PHY_PERIODIC_LOG_VER24	24u
typedef struct phy_periodic_log_v24 {
	uint8  version;		/* Logging structure version */
	uint8  numcores;	/* Number of cores for which core specific data present */
	uint16 length;		/* Length of the structure */

	/* Logs general PHY parameters */
	phy_periodic_log_cmn_v10_t phy_perilog_cmn;

	/* Logs ucode counters and NAVs */
	phy_periodic_counters_v9_t counters_peri_log;

	/* log data for BTcoex */
	phy_periodic_btc_stats_v2_t phy_perilog_btc_stats;

	/* log data for obss/dynbw */
	phy_periodic_obss_stats_v2_t phy_perilog_obss_stats;

	/* Logs data pertaining to each core */
	phy_periodic_log_core_v7_t phy_perilog_core[2];

	/* log data for smartCCA */
	phy_periodic_scca_stats_v3_t scca_counters_peri_log;
} phy_periodic_log_v24_t;

#define PHY_PERIODIC_LOG_VER25	25u
typedef struct phy_periodic_log_v25 {
	uint8  version;		/* Logging structure version */
	uint8  numcores;	/* Number of cores for which core specific data present */
	uint16 length;		/* Length of the structure */

	/* Logs general PHY parameters */
	phy_periodic_log_cmn_v11_t phy_perilog_cmn;

	/* Logs ucode counters and NAVs */
	phy_periodic_counters_v9_t counters_peri_log;

	/* log data for BTcoex */
	phy_periodic_btc_stats_v2_t phy_perilog_btc_stats;

	/* log data for obss/dynbw */
	phy_periodic_obss_stats_v3_t phy_perilog_obss_stats;

	/* Logs data pertaining to each core */
	phy_periodic_log_core_v7_t phy_perilog_core[2];

	/* log data for smartCCA */
	phy_periodic_scca_stats_v3_t scca_counters_peri_log;
} phy_periodic_log_v25_t;

#define PHY_PERIODIC_LOG_VER26	26u
typedef struct phy_periodic_log_v26 {
	uint8  version;		/* Logging structure version */
	uint8  numcores;	/* Number of cores for which core specific data present */
	uint16 length;		/* Length of the structure */

	/* Logs general PHY parameters */
	phy_periodic_log_cmn_v12_t phy_perilog_cmn;

	/* Logs ucode counters and NAVs */
	phy_periodic_counters_v10_t counters_peri_log;

	/* log data for BTcoex */
	phy_periodic_btc_stats_v2_t phy_perilog_btc_stats;

	/* log data for obss/dynbw */
	phy_periodic_obss_stats_v4_t phy_perilog_obss_stats;

	/* Logs data pertaining to each core */
	phy_periodic_log_core_v8_t phy_perilog_core[2];

	/* log data for smartCCA */
	phy_periodic_scca_stats_v3_t scca_counters_peri_log;
} phy_periodic_log_v26_t;

#define PHY_PERIODIC_LOG_VER27	27u
typedef struct phy_periodic_log_v27 {
	uint8  version;		/* Logging structure version */
	uint8  numcores;	/* Number of cores for which core specific data present */
	uint16 length;		/* Length of the structure */

	/* Logs general PHY parameters */
	phy_periodic_log_cmn_v13_t phy_perilog_cmn;

	/* Logs ucode counters and NAVs */
	phy_periodic_counters_v11_t counters_peri_log;

	/* log data for BTcoex */
	phy_periodic_btc_stats_v2_t phy_perilog_btc_stats;

	/* log data for obss/dynbw */
	phy_periodic_obss_stats_v4_t phy_perilog_obss_stats;

	/* Logs data pertaining to each core */
	phy_periodic_log_core_v9_t phy_perilog_core[2];

	/* log data for smartCCA */
	phy_periodic_scca_stats_v4_t scca_counters_peri_log;
} phy_periodic_log_v27_t;

#define PHY_PERIODIC_LOG_VER28	28u
typedef struct phy_periodic_log_v28 {
	uint8  version;		/* Logging structure version */
	uint8  numcores;	/* Number of cores for which core specific data present */
	uint16 length;		/* Length of the structure */

	/* Logs general PHY parameters */
	phy_periodic_log_cmn_v13_t phy_perilog_cmn;

	/* Logs ucode counters and NAVs */
	phy_periodic_counters_v11_t counters_peri_log;

	/* log data for BTcoex */
	phy_periodic_btc_stats_v4_t phy_perilog_btc_stats;

	/* log data for obss/dynbw */
	phy_periodic_obss_stats_v4_t phy_perilog_obss_stats;

	/* Logs data pertaining to each core */
	phy_periodic_log_core_v9_t phy_perilog_core[2];

	/* log data for smartCCA */
	phy_periodic_scca_stats_v4_t scca_counters_peri_log;
} phy_periodic_log_v28_t;

#define PHY_PERIODIC_LOG_VER29	29u
/* Used by parser to parse data */
typedef struct phy_log_data_v29 {
	uint8  version;		/* Logging structure version */
	uint8  numcores;	/* Number of cores for which core specific data present */
	uint16 length;		/* Length of the structure */
	uint16 seq;		/* FOr matching with other structs sent out at the same time */
	uint8 PAD[2];

	/* Logs general PHY parameters */
	phy_periodic_log_cmn_v14_t phy_perilog_cmn;

	/* log data for obss/dynbw */
	phy_periodic_obss_stats_v4_t phy_perilog_obss_stats;

	/* Logs data pertaining to each core */
	phy_periodic_log_core_v10_t phy_perilog_core[2];

	/* log data for BTcoex */
	phy_periodic_btc_stats_v4_t phy_perilog_btc_stats;

	/* log data for smartCCA */
	phy_periodic_scca_stats_v4_t scca_counters_peri_log;
} phy_log_data_v29_t;

/* USed by FW to populate counters */
typedef struct phy_periodic_log_v29 {
	uint8  version;		/* Logging structure version */
	uint8  numcores;	/* Number of cores for which core specific data present */
	uint16 length;		/* Length of the structure */
	uint16 seq;		/* FOr matching with other structs sent out at the same time */
	uint8 PAD[2];

	/* Logs general PHY parameters */
	phy_periodic_log_cmn_v14_t phy_perilog_cmn;

	/* log data for obss/dynbw */
	phy_periodic_obss_stats_v4_t phy_perilog_obss_stats;

	/* Logs data pertaining to each core */
	phy_periodic_log_core_v10_t phy_perilog_core[2];

	/* log data for BTcoex */
	phy_periodic_btc_stats_v4_t phy_perilog_btc_stats;

	/* log data for smartCCA */
	phy_periodic_scca_stats_v4_t scca_counters_peri_log;

	/* Logs ucode counters and NAVs */
	phy_periodic_counters_v12_t counters_peri_log;
} phy_periodic_log_v29_t;

#define PHY_PERIODIC_LOG_VER30	30u
/* Used by parser to parse data */
typedef struct phy_log_data_v30 {
	uint8  version;		/* Logging structure version */
	uint8  numcores;	/* Number of cores for which core specific data present */
	uint16 length;		/* Length of the structure */
	uint16 seq;		/* FOr matching with other structs sent out at the same time */
	uint8 PAD[2];

	/* Logs general PHY parameters */
	phy_periodic_log_cmn_v16_t phy_perilog_cmn;

	/* log data for obss/dynbw */
	phy_periodic_obss_stats_v4_t phy_perilog_obss_stats;

	/* Logs data pertaining to each core */
	phy_periodic_log_core_v11_t phy_perilog_core[2];

	/* log data for BTcoex */
	phy_periodic_btc_stats_v4_t phy_perilog_btc_stats;

	/* log data for smartCCA */
	phy_periodic_scca_stats_v4_t scca_counters_peri_log;
} phy_log_data_v30_t;

/* USed by FW to populate counters */
typedef struct phy_periodic_log_v30 {
	uint8  version;		/* Logging structure version */
	uint8  numcores;	/* Number of cores for which core specific data present */
	uint16 length;		/* Length of the structure */
	uint16 seq;		/* FOr matching with other structs sent out at the same time */
	uint8 PAD[2];

	/* Logs general PHY parameters */
	phy_periodic_log_cmn_v16_t phy_perilog_cmn;

	/* log data for obss/dynbw */
	phy_periodic_obss_stats_v4_t phy_perilog_obss_stats;

	/* Logs data pertaining to each core */
	phy_periodic_log_core_v11_t phy_perilog_core[2];

	/* log data for BTcoex */
	phy_periodic_btc_stats_v4_t phy_perilog_btc_stats;

	/* log data for smartCCA */
	phy_periodic_scca_stats_v4_t scca_counters_peri_log;

	/* Logs ucode counters and NAVs */
	phy_periodic_counters_v14_t counters_peri_log;
} phy_periodic_log_v30_t;

/* Used by parser to parse data */
typedef struct phy_log_data_v40 {
	uint8  version;		/* Logging structure version */
	uint8  numcores;	/* Number of cores for which core specific data present */
	uint16 length;		/* Length of the structure */
	uint16 seq;		/* FOr matching with other structs sent out at the same time */
	uint8 PAD[2];

	/* Logs general PHY parameters */
	phy_periodic_log_cmn_v15_t phy_perilog_cmn;

	/* log data for BTcoex */
	phy_periodic_btc_stats_v5_t phy_perilog_btc_stats;

	/* log data for obss/dynbw */
	phy_periodic_obss_stats_v4_t phy_perilog_obss_stats;

	/* Logs data pertaining to each core */
	phy_periodic_log_core_v10_t phy_perilog_core[2];

	/* log data for smartCCA */
	phy_periodic_scca_stats_v4_t scca_counters_peri_log;
} phy_log_data_v40_t;

/* Used by FW to populate counters */
#define PHY_PERIODIC_LOG_VER40	40u
typedef struct phy_periodic_log_v40 {
	uint8  version;		/* Logging structure version */
	uint8  numcores;	/* Number of cores for which core specific data present */
	uint16 length;		/* Length of the structure */
	uint16 seq;		/* FOr matching with other structs sent out at the same time */
	uint8 PAD[2];

	/* Logs general PHY parameters */
	phy_periodic_log_cmn_v15_t phy_perilog_cmn;

	/* log data for BTcoex */
	phy_periodic_btc_stats_v5_t phy_perilog_btc_stats;

	/* log data for obss/dynbw */
	phy_periodic_obss_stats_v4_t phy_perilog_obss_stats;

	/* Logs data pertaining to each core */
	phy_periodic_log_core_v10_t phy_perilog_core[2];

	/* log data for smartCCA */
	phy_periodic_scca_stats_v4_t scca_counters_peri_log;

	/* Logs ucode counters and NAVs */
	phy_periodic_counters_v13_t counters_peri_log;
} phy_periodic_log_v40_t;

#define PHY_PERIODIC_LOG_EXT_VER41	41u
typedef struct phy_periodic_log_ext_v41 {
	uint16 version;
	uint16 len;
	uint16 seq;
	uint8 PAD[2];

	/* Logs ucode counters and NAVs */
	phy_periodic_counters_v15_t counters_peri_log;

	/* Logs data pertaining to each core */
	phy_periodic_log_core_v12_t phy_perilog_core[2];
} phy_periodic_log_ext_v41_t;

/* Used by FW to populate counters */
#define PHY_PERIODIC_LOG_VER41	41u
typedef struct phy_periodic_log_v41 {
	uint8  version;		/* Logging structure version */
	uint8  numcores;	/* Number of cores for which core specific data present */
	uint16 length;		/* Length of the structure */
	uint16 seq;		/* FOr matching with other structs sent out at the same time */
	uint8 PAD[2];

	/* Logs general PHY parameters */
	phy_periodic_log_cmn_v17_t phy_perilog_cmn;

	/* log data for BTcoex */
	phy_periodic_btc_stats_v5_t phy_perilog_btc_stats;

	/* log data for obss/dynbw */
	phy_periodic_obss_stats_v5_t phy_perilog_obss_stats;

	/* log data for smartCCA */
	phy_periodic_scca_stats_v4_t scca_counters_peri_log;

	/* MLO related PHY Logging */
	phy_periodic_mlo_stats_v1_t phy_perilog_mlo_stats;

	/* SRCB related PHY Logging */
	phy_periodic_srcb_stats_v1_t phy_perilog_srcb_stats;

	/* FBC related PHY Logging */
	phy_periodic_fbc_stats_v1_t phy_perilog_fbc_stats;

	/* VCOCAL related PHY Logging */
	phy_periodic_vcocal_stats_v1_t phy_perilog_vcocal_stats;

	/* FBCX related PHY Logging */
	phy_periodic_fbcx_stats_v1_t phy_perilog_fbcx_stats;

	/* Shared radio related PHY Logging */
	phy_periodic_sra_stats_v1_t phy_perilog_sra_stats;

	/* Logging going in separate log buffer */
	phy_periodic_log_ext_v41_t extended_logs;
} phy_periodic_log_v41_t;

#define PHY_PERIODIC_LOG_EXT_VER42	42u
typedef struct phy_periodic_log_ext_v42 {
	uint16 version;
	uint16 len;
	uint16 seq;
	uint8 PAD[2];

	/* Logs ucode counters and NAVs */
	phy_periodic_counters_v16_t counters_peri_log;

	/* Logs data pertaining to each core */
	phy_periodic_log_core_v12_t phy_perilog_core[2];
} phy_periodic_log_ext_v42_t;

/* Used by FW to populate counters */
#define PHY_PERIODIC_LOG_VER42	42u
typedef struct phy_periodic_log_v42 {
	uint8  version;		/* Logging structure version */
	uint8  numcores;	/* Number of cores for which core specific data present */
	uint16 length;		/* Length of the structure */
	uint16 seq;		/* For matching with other structs sent out at the same time */
	uint8 PAD[2];

	/* Logs general PHY parameters */
	phy_periodic_log_cmn_v18_t phy_perilog_cmn;

	/* log data for BTcoex */
	phy_periodic_btc_stats_v5_t phy_perilog_btc_stats;

	/* log data for obss/dynbw */
	phy_periodic_obss_stats_v5_t phy_perilog_obss_stats;

	/* log data for smartCCA */
	phy_periodic_scca_stats_v4_t scca_counters_peri_log;

	/* MLO related PHY Logging */
	phy_periodic_mlo_stats_v1_t phy_perilog_mlo_stats;

	/* SRCB related PHY Logging */
	phy_periodic_srcb_stats_v1_t phy_perilog_srcb_stats;

	/* FBC related PHY Logging */
	phy_periodic_fbc_stats_v1_t phy_perilog_fbc_stats;

	/* VCOCAL related PHY Logging */
	phy_periodic_vcocal_stats_v2_t phy_perilog_vcocal_stats;

	/* FBCX related PHY Logging */
	phy_periodic_fbcx_stats_v1_t phy_perilog_fbcx_stats;

	/* Shared radio related PHY Logging */
	phy_periodic_sra_stats_v2_t phy_perilog_sra_stats;

	/* Logging going in separate log buffer */
	phy_periodic_log_ext_v42_t extended_logs;
} phy_periodic_log_v42_t;

#define PHY_PERIODIC_LOG_VER43	43u
typedef struct phy_periodic_log_v43 {
	uint8  version;		/* Logging structure version */
	uint8  numcores;	/* Number of cores for which core specific data present */
	uint16 length;		/* Length of the structure */
	uint16 seq;		/* For matching with other structs sent out at the same time */
	uint8 PAD[2];

	/* Logs general PHY parameters */
	phy_periodic_log_cmn_v19_t phy_perilog_cmn;

	/* log data for BTcoex */
	phy_periodic_btc_stats_v5_t phy_perilog_btc_stats;

	/* log data for obss/dynbw */
	phy_periodic_obss_stats_v5_t phy_perilog_obss_stats;

	/* log data for smartCCA */
	phy_periodic_scca_stats_v4_t scca_counters_peri_log;

	/* MLO related PHY Logging */
	phy_periodic_mlo_stats_v1_t phy_perilog_mlo_stats;

	/* SRCB related PHY Logging */
	phy_periodic_srcb_stats_v1_t phy_perilog_srcb_stats;

	/* FBC related PHY Logging */
	phy_periodic_fbc_stats_v1_t phy_perilog_fbc_stats;

	/* VCOCAL related PHY Logging */
	phy_periodic_vcocal_stats_v2_t phy_perilog_vcocal_stats;

	/* FBCX related PHY Logging */
	phy_periodic_fbcx_stats_v2_t phy_perilog_fbcx_stats;

	/* Shared radio related PHY Logging */
	phy_periodic_sra_stats_v3_t phy_perilog_sra_stats;

	/* Logging going in separate log buffer */
	phy_periodic_log_ext_v42_t extended_logs;
} phy_periodic_log_v43_t;

#define PHY_PERIODIC_LOG_EXT_VER43	43u
#define PHY_PERIODIC_LOG_EXT_VER43_SIZE	880u
typedef struct phy_periodic_log_ext_v43 {
	uint16 version;
	uint16 len;
	uint16 seq;
	uint8 PAD[2];

	/* Logs ucode counters and NAVs */
	phy_periodic_counters_v17_t counters_peri_log;

	/* Logs data pertaining to each core */
	phy_periodic_log_core_v13_t phy_perilog_core[2];

	/* SRCB related PHY Logging */
	phy_periodic_srcb_stats_v2_t phy_perilog_srcb_stats;
} phy_periodic_log_ext_v43_t;

#define PHY_PERIODIC_LOG_VER44		44u
#define PHY_PERIODIC_LOG_VER44_SIZE	1720u
typedef struct phy_periodic_log_v44 {
	uint8  version;		/* Logging structure version */
	uint8  numcores;	/* Number of cores for which core specific data present */
	uint16 length;		/* Length of the structure */
	uint16 seq;		/* For matching with other structs sent out at the same time */
	uint8 PAD[2];

	/* Logs general PHY parameters */
	phy_periodic_log_cmn_v20_t phy_perilog_cmn;

	/* log data for BTcoex */
	phy_periodic_btc_stats_v6_t phy_perilog_btc_stats;

	/* log data for obss/dynbw */
	phy_periodic_obss_stats_v6_t phy_perilog_obss_stats;

	/* log data for smartCCA */
	phy_periodic_scca_stats_v5_t phy_perilog_scca_stats;

	/* MLO related PHY Logging */
	phy_periodic_mlo_stats_v2_t phy_perilog_mlo_stats;

	/* VCOCAL related PHY Logging */
	phy_periodic_vcocal_stats_v2_t phy_perilog_vcocal_stats;

	/* FBCX related PHY Logging */
	phy_periodic_fbcx_stats_v3_t phy_perilog_fbcx_stats;

	/* Shared radio related PHY Logging */
	phy_periodic_sra_stats_v3_t phy_perilog_sra_stats;

	/* Logging going in separate log buffer */
	phy_periodic_log_ext_v43_t extended_logs;
} phy_periodic_log_v44_t;

/* ************************************************** */
/* The version 255 for the logging data structures    */
/* is for use in trunk ONLY. In release branches the  */
/* next available version for the particular data     */
/* structure should be used.                          */
/* ************************************************** */
#define PHY_PERIODIC_LOG_EXT_VER255		255u
#define PHY_PERIODIC_LOG_EXT_VER255_SIZE	928u
typedef struct phy_periodic_log_ext_v255 {
	uint16 version;
	uint16 len;
	uint16 seq;
	uint8 PAD[2];

	/* Logs ucode counters and NAVs */
	phy_periodic_counters_v255_t counters_peri_log;

	/* Logs data pertaining to each core */
	phy_periodic_log_core_v255_t phy_perilog_core[2];

	/* SRCB related PHY Logging */
	phy_periodic_srcb_stats_v255_t phy_perilog_srcb_stats;
} phy_periodic_log_ext_v255_t;

/* Used by FW to populate counters */
#define PHY_PERIODIC_LOG_VER255		255u
#define PHY_PERIODIC_LOG_VER255_SIZE	1820u
typedef struct phy_periodic_log_v255 {
	uint8  version;		/* Logging structure version */
	uint8  numcores;	/* Number of cores for which core specific data present */
	uint16 length;		/* Length of the structure */
	uint16 seq;		/* FOr matching with other structs sent out at the same time */
	uint8 PAD[2];

	/* Logs general PHY parameters */
	phy_periodic_log_cmn_v255_t phy_perilog_cmn;

	/* log data for BTcoex */
	phy_periodic_btc_stats_v255_t phy_perilog_btc_stats;

	/* log data for obss/dynbw */
	phy_periodic_obss_stats_v255_t phy_perilog_obss_stats;

	/* log data for smartCCA */
	phy_periodic_scca_stats_v255_t phy_perilog_scca_stats;

	/* MLO related PHY Logging */
	phy_periodic_mlo_stats_v255_t phy_perilog_mlo_stats;

	/* FBC related PHY Logging */
	phy_periodic_fbc_stats_v255_t phy_perilog_fbc_stats;

	/* VCOCAL related PHY Logging */
	phy_periodic_vcocal_stats_v255_t phy_perilog_vcocal_stats;

	/* FBCX related PHY Logging */
	phy_periodic_fbcx_stats_v255_t phy_perilog_fbcx_stats;

	/* Shared radio related PHY Logging */
	phy_periodic_sra_stats_v255_t phy_perilog_sra_stats;

	/* Logging going in separate log buffer */
	phy_periodic_log_ext_v255_t extended_logs;
} phy_periodic_log_v255_t;
#endif /* _PHY_EVENT_LOG_PAYLOAD_H_ */
