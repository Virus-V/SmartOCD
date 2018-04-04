/*
 * JTAG.h
 *
 *  Created on: 2018-3-24
 *      Author: virusv
 */

#ifndef SRC_TARGET_JTAG_H_
#define SRC_TARGET_JTAG_H_

#include "smart_ocd.h"
#include "misc/list/list.h"

enum JTAG_TAP_Status {
	/* 特殊状态 */
	JTAG_TAP_RESET = 0,
	JTAG_TAP_IDLE,
	/* DR相关状态 */
	JTAG_TAP_DRSELECT,
	JTAG_TAP_DRCAPTURE,
	JTAG_TAP_DRSHIFT,
	JTAG_TAP_DREXIT1,
	JTAG_TAP_DRPAUSE,
	JTAG_TAP_DREXIT2,
	JTAG_TAP_DRUPDATE,
	/* IR相关状态 */
	JTAG_TAP_IRSELECT,
	JTAG_TAP_IRCAPTURE,
	JTAG_TAP_IRSHIFT,
	JTAG_TAP_IREXIT1,
	JTAG_TAP_IRPAUSE,
	JTAG_TAP_IREXIT2,
	JTAG_TAP_IRUPDATE,
	JTAG_TAP_STATUS_NUM,	// JTAG所有状态个数
};

uint16_t JTAG_getTMSSequence(enum JTAG_TAP_Status fromStatus, enum JTAG_TAP_Status toStatus);

#endif /* SRC_TARGET_JTAG_H_ */
