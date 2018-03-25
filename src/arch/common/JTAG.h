/*
 * JTAG.h
 *
 *  Created on: 2018-3-24
 *      Author: virusv
 */

#ifndef SRC_ARCH_COMMON_JTAG_H_
#define SRC_ARCH_COMMON_JTAG_H_

#include "debugger/adapter.h"

enum JTAG_TAP_Status {
	/* 特殊状态 */
	//JTAG_TAP_INIT = 0,
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
	JTAG_TAP_IRUPDATE
};

/*
 * JTAG TAP状态机
 */
struct JTAG_TAPSM {
	enum JTAG_TAP_Status currentStatus;	// TAP状态机当前状态
};

// 状态机复位
BOOL JTAG_TAP_Reset(AdapterObject *adapterObj, BOOL hard);
// TODO 状态机切换状态

#endif /* SRC_ARCH_COMMON_JTAG_H_ */
