/*
 * JTAG.c
 *
 *  Created on: 2018-3-24
 *      Author: virusv
 */

#include "target/JTAG.h"
#include "misc/log.h"

// 判断一个状态是不是在某个阶段中
#define JTAG_IN_DR(state) (JTAG_TAP_DRSELECT <= (state) && (state) <= JTAG_TAP_DRUPDATE)
#define JTAG_IN_IR(state) (JTAG_TAP_IRSELECT <= (state) && (state) <= JTAG_TAP_IRUPDATE)
/**
 * 单元测试函数部分
 * 用于产生在当前状态到指定状态的TMS时序
 * 返回值：sequence 高8位为时序信息，低八位为时序个数
 * 时序信息是最低位在前。比如 0101 1111，则发送顺序是 1111 1010
 */
uint16_t JTAG_getTMSSequence(enum JTAG_TAP_Status fromStatus, enum JTAG_TAP_Status toStatus) {
	int sequence, idx;
#define _SET_BIT_(x) (sequence |= ((x) << idx))
	for (sequence = 0, idx = 8; fromStatus != toStatus; sequence++, idx++) {
		switch (fromStatus) {
		case JTAG_TAP_RESET:
			// 复位状态只有一个指向下一个相邻的状态，直接赋值
			fromStatus = JTAG_TAP_IDLE;
			_SET_BIT_(0);	// TMS =0 切换到IDLE状态
			break;
		case JTAG_TAP_IDLE:
			fromStatus = JTAG_TAP_DRSELECT;
			_SET_BIT_(1);
			break;
		case JTAG_TAP_DRSELECT:
			if (JTAG_IN_DR(toStatus)) {
				fromStatus = JTAG_TAP_DRCAPTURE;
				_SET_BIT_(0);
			} else {
				fromStatus = JTAG_TAP_IRSELECT;
				_SET_BIT_(1);
			}
			break;
		case JTAG_TAP_DRCAPTURE:
			if (toStatus == JTAG_TAP_DRSHIFT) {
				fromStatus = JTAG_TAP_DRSHIFT;
				_SET_BIT_(0);
			} else {
				fromStatus = JTAG_TAP_DREXIT1;
				_SET_BIT_(1);
			}
			break;

		case JTAG_TAP_DRSHIFT:
			fromStatus = JTAG_TAP_DREXIT1;
			_SET_BIT_(1);
			break;

		case JTAG_TAP_DREXIT1:
			if (toStatus == JTAG_TAP_DRPAUSE || toStatus == JTAG_TAP_DREXIT2) {
				fromStatus = JTAG_TAP_DRPAUSE;
				_SET_BIT_(0);
			} else {
				fromStatus = JTAG_TAP_DRUPDATE;
				_SET_BIT_(1);
			}
			break;

		case JTAG_TAP_DRPAUSE:
			fromStatus = JTAG_TAP_DREXIT2;
			_SET_BIT_(1);
			break;

		case JTAG_TAP_DREXIT2:
			if (toStatus == JTAG_TAP_DRSHIFT) {
				fromStatus = JTAG_TAP_DRSHIFT;
				_SET_BIT_(0);
			} else {
				fromStatus = JTAG_TAP_DRUPDATE;
				_SET_BIT_(1);
			}
			break;

		case JTAG_TAP_DRUPDATE:
			if (toStatus == JTAG_TAP_IDLE) {
				fromStatus = JTAG_TAP_IDLE;
				_SET_BIT_(0);
			} else {
				fromStatus = JTAG_TAP_DRSELECT;
				_SET_BIT_(1);
			}
			break;

		case JTAG_TAP_IRSELECT:
			if (JTAG_IN_IR(toStatus)) {
				fromStatus = JTAG_TAP_IRCAPTURE;
				_SET_BIT_(0);
			} else {
				fromStatus = JTAG_TAP_RESET;
				_SET_BIT_(1);
			}
			break;

		case JTAG_TAP_IRCAPTURE:
			if (toStatus == JTAG_TAP_IRSHIFT) {
				fromStatus = JTAG_TAP_IRSHIFT;
				_SET_BIT_(0);
			} else {
				fromStatus = JTAG_TAP_IREXIT1;
				_SET_BIT_(1);
			}
			break;

		case JTAG_TAP_IRSHIFT:
			fromStatus = JTAG_TAP_IREXIT1;
			_SET_BIT_(1);
			break;

		case JTAG_TAP_IREXIT1:
			if (toStatus == JTAG_TAP_IRPAUSE || toStatus == JTAG_TAP_IREXIT2) {
				fromStatus = JTAG_TAP_IRPAUSE;
				_SET_BIT_(0);
			} else {
				fromStatus = JTAG_TAP_IRUPDATE;
				_SET_BIT_(1);
			}
			break;

		case JTAG_TAP_IRPAUSE:
			fromStatus = JTAG_TAP_IREXIT2;
			_SET_BIT_(1);
			break;

		case JTAG_TAP_IREXIT2:
			if (toStatus == JTAG_TAP_IRSHIFT) {
				fromStatus = JTAG_TAP_IRSHIFT;
				_SET_BIT_(0);
			} else {
				fromStatus = JTAG_TAP_IRUPDATE;
				_SET_BIT_(1);
			}
			break;

		case JTAG_TAP_IRUPDATE:
			if (toStatus == JTAG_TAP_IDLE) {
				fromStatus = JTAG_TAP_IDLE;
				_SET_BIT_(0);
			} else {
				fromStatus = JTAG_TAP_DRSELECT;
				_SET_BIT_(1);
			}
			break;
		}
	}
#undef _SET_BIT_
	return sequence & 0xFFFF;
}
