/*
 * hdcp.h
 *
 *  Created on: Jul 21, 2010
 *
 *  Synopsys Inc.
 *  SG DWC PT02
 */

#ifndef HDCP_H_
#define HDCP_H_

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

enum hdcp_status_t {
	HDCP_IDLE = 0,
	HDCP_KSV_LIST_READY,
	HDCP_ERR_KSV_LIST_NOT_VALID,
	HDCP_KSV_LIST_ERR_DEPTH_EXCEEDED,
	HDCP_KSV_LIST_ERR_MEM_ACCESS,
	HDCP_ENGAGED,
	HDCP_FAILED
};

/* HDCP Interrupt bit fields */
#define INT_KSV_ACCESS   (0)
#define INT_KSV_SHA1     (1)
#define INT_HDCP_FAIL    (6)
#define INT_HDCP_ENGAGED (7)

#endif /* HDCP_H_ */
