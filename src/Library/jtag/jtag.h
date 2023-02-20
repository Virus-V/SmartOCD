/**
 * Copyright (c) 2023, Virus.V <virusv@live.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * * Neither the name of SmartOCD nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * Copyright 2023 Virus.V <virusv@live.com>
 */

#ifndef SRC_JTAG_JTAG_H_
#define SRC_JTAG_JTAG_H_

#include "smartocd.h"

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
TMS_SeqInfo JtagGetTmsSequence(IN enum JTAG_TAP_State fromState, IN enum JTAG_TAP_State toState);

/**
 * 获得当前状态通过一个给定TMS信号时切换到的状态
 * 参数:
 *	fromState:当前JTAG状态机的状态
 *	tms:给定的TMS信号
 * 返回:
 * 	下一个状态机状态
 */
enum JTAG_TAP_State JtagNextStatus(IN enum JTAG_TAP_State fromState, IN int tms);

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
