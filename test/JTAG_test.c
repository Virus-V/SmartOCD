/*
 * JTAG_test.c
 *
 *  Created on: 2018-3-24
 *      Author: virusv
 */

#include <stdio.h>
#include <stdlib.h>

#include "smart_ocd.h"
#include "misc/log.h"
#include "arch/common/JTAG.h"

// 判断一个状态是不是在某个阶段中
#define JTAG_IN_DR(state) (JTAG_TAP_DRSELECT <= (state) && (state) <= JTAG_TAP_DRUPDATE)
#define JTAG_IN_IR(state) (JTAG_TAP_IRSELECT <= (state) && (state) <= JTAG_TAP_IRUPDATE)
/**
 * 单元测试函数部分
 * 用于产生在当前状态到指定状态的TMS时序
 * 返回值：sequence 高8位为时序信息，第八位为时序个数
 * 时序信息是最低位在前。比如 0101 1111，则发送顺序是 1111 1010
 *
 */
uint16_t getTMSSequence(enum JTAG_TAP_Status fromStatus,
		enum JTAG_TAP_Status toStatus) {
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

const char *state2str(enum JTAG_TAP_Status tap_state) {
#define X(_s) if (tap_state == _s) return #_s;
	X(JTAG_TAP_RESET)
	X(JTAG_TAP_IDLE)
	X(JTAG_TAP_DRSELECT)
	X(JTAG_TAP_DRCAPTURE)
	X(JTAG_TAP_DRSHIFT)
	X(JTAG_TAP_DREXIT1)
	X(JTAG_TAP_DRPAUSE)
	X(JTAG_TAP_DREXIT2)
	X(JTAG_TAP_DRUPDATE)
	X(JTAG_TAP_IRSELECT)
	X(JTAG_TAP_IRCAPTURE)
	X(JTAG_TAP_IRSHIFT)
	X(JTAG_TAP_IREXIT1)
	X(JTAG_TAP_IRPAUSE)
	X(JTAG_TAP_IREXIT2)
	X(JTAG_TAP_IRUPDATE)
#undef X
	return "UNKOWN_STATE";
}

char* itoa(int num, char *str, int radix) {/*索引表*/
	char index[] = "0123456789ABCDEF";
	unsigned unum;/*中间变量*/
	int i = 0, j, k;
	/*确定unum的值*/
	if (radix == 10 && num < 0)/*十进制负数*/
	{
		unum = (unsigned) -num;
		str[i++] = '-';
	} else
		unum = (unsigned) num;/*其他情况*/
	/*转换*/
	do {
		str[i++] = index[unum % (unsigned) radix];
		unum /= radix;
	} while (unum);
	str[i] = '\0';
	/*逆序*/
	if (str[0] == '-')
		k = 1;/*十进制负数*/
	else k = 0;
	char temp;
	for (j = k; j <= (i - 1) / 2; j++) {
		temp = str[j];
		str[j] = str[i - 1 + k - j];
		str[i - 1 + k - j] = temp;
	}
	return str;
}
/**
 * JTAG 测试
 * 产生时序
 * 在所有时序中任意组合，最长时序不超过8位。
 */
int main() {
	char s[10];
	uint16_t data;
	enum JTAG_TAP_Status fromStatus, toStatus;
	for (fromStatus = JTAG_TAP_RESET; fromStatus <= JTAG_TAP_IRUPDATE;
			fromStatus++) {
		for (toStatus = JTAG_TAP_RESET; toStatus <= JTAG_TAP_IRUPDATE;
				toStatus++) {
			printf("%s ==> %s.\n", state2str(fromStatus), state2str(toStatus));
			data = getTMSSequence(fromStatus, toStatus);
			itoa(data >> 8, s, 2);
			printf("%d Clocks:%s\n", data & 0xff, s);
		}
	}
	return 0;
}
