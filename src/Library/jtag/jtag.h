/*
 * JTAG.h
 *
 *  Created on: 2018-3-24
 *      Author: virusv
 */

#ifndef SRC_JTAG_JTAG_H_
#define SRC_JTAG_JTAG_H_

#include "smart_ocd.h"

enum JTAG_TAP_State {
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

/**
 * TMS时序信息
 * 高八位为时序信息，低八位为时序个数
 * 高八位时序信息低位先发送
 */
typedef uint16_t TMS_SeqInfo;

/**
 * 生成两个JTAG状态之间切换的TMS时序
 * 参数:
 * 	fromState:JTAG状态机的当前状态
 * 	toState:要转换到的JTAG状态机状态
 * 返回:
 * 	TMS时序,见TMS_SeqInfo类型定义
 */
TMS_SeqInfo JtagGetTmsSequence(IN enum JTAG_TAP_State fromState,
                               IN enum JTAG_TAP_State toState);

/**
 * 获得当前状态通过一个给定TMS信号时切换到的状态
 * 参数:
 *	fromState:当前JTAG状态机的状态
 *	tms:给定的TMS信号
 * 返回:
 * 	下一个状态机状态
 */
enum JTAG_TAP_State JtagNextStatus(IN enum JTAG_TAP_State fromState,
                                   IN int tms);

/**
 * 计算多少个TMS信号有多少个电平状态
 * 参数:
 * 	tms:TMS信号，LSB
 * 	count:TMS信号位数
 * 返回:
 * 	给定TMS信号的电平状态
 * 比如：
 * 	tms = 0b1001110
 * 	count = 7
 * 	返回值 = 4: 1周期的低电平，3周期的高电平，2周期的低电平，1周期的高电平
 */
int JtagCalTmsLevelState(IN uint32_t tms, IN int count);

/**
 * 返回JTAG状态对应的字符串
 * 参数:
 * 	tap_state:JTAG状态机状态
 * 返回:
 * 	该状态对应的字符串描述
 */
const char* JtagStateToStr(IN enum JTAG_TAP_State tap_state);

#endif /* SRC_JTAG_JTAG_H_ */
