/*
 * JTAG.h
 *
 *  Created on: 2018-3-24
 *      Author: virusv
 */

#ifndef SRC_TARGET_JTAG_H_
#define SRC_TARGET_JTAG_H_

#include "smart_ocd.h"

/**
 * TMS时序信息
 * 高八位为时序信息，低八位为时序个数
 * 高八位时序信息低位先发送
 */
typedef uint16_t TMS_SeqInfo;

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
};

TMS_SeqInfo JTAG_Get_TMS_Sequence(enum JTAG_TAP_Status fromStatus, enum JTAG_TAP_Status toStatus);
enum JTAG_TAP_Status JTAG_NextStatus(enum JTAG_TAP_Status fromStatus, int tms);
int JTAG_Cal_TMS_LevelStatus(uint32_t tms, int count);
const char *JTAG_state2str(enum JTAG_TAP_Status tap_state);

#endif /* SRC_TARGET_JTAG_H_ */
