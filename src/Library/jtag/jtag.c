/**
 * src/Library/jtag/jtag.c
 * Copyright (c) 2020 Virus.V <virusv@live.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "jtag.h"

#include "log/log.h"

// 判断一个状态是不是在某个阶段中
#define JTAG_IN_DR(state) (JTAG_TAP_DRSELECT <= (state) && (state) <= JTAG_TAP_DRUPDATE)
#define JTAG_IN_IR(state) (JTAG_TAP_IRSELECT <= (state) && (state) <= JTAG_TAP_IRUPDATE)

/**
 * 用于产生在当前状态到指定状态的TMS时序
 * 返回值：sequence 高8位为时序信息，低八位为时序个数
 * 时序信息是最低位先发送。比如 0101 1111，则发送顺序是 <-1111 1010<-
 */
TMS_SeqInfo JtagGetTmsSequence(enum JTAG_TAP_State fromState, enum JTAG_TAP_State toState) {
  assert(fromState >= JTAG_TAP_RESET && fromState <= JTAG_TAP_IRUPDATE);
  assert(toState >= JTAG_TAP_RESET && toState <= JTAG_TAP_IRUPDATE);
  int sequence, idx;
#define _SET_BIT_(x) (sequence |= ((x) << idx))
  for (sequence = 0, idx = 8; fromState != toState; sequence++, idx++) {
    switch (fromState) {
      case JTAG_TAP_RESET:
        // 复位状态只有一个指向下一个相邻的状态，直接赋值
        fromState = JTAG_TAP_IDLE;
        _SET_BIT_(0);  // TMS =0 切换到IDLE状态
        break;
      case JTAG_TAP_IDLE:
        fromState = JTAG_TAP_DRSELECT;
        _SET_BIT_(1);
        break;
      case JTAG_TAP_DRSELECT:
        if (JTAG_IN_DR(toState)) {
          fromState = JTAG_TAP_DRCAPTURE;
          _SET_BIT_(0);
        } else {
          fromState = JTAG_TAP_IRSELECT;
          _SET_BIT_(1);
        }
        break;
      case JTAG_TAP_DRCAPTURE:
        if (toState == JTAG_TAP_DRSHIFT) {
          fromState = JTAG_TAP_DRSHIFT;
          _SET_BIT_(0);
        } else {
          fromState = JTAG_TAP_DREXIT1;
          _SET_BIT_(1);
        }
        break;

      case JTAG_TAP_DRSHIFT:
        fromState = JTAG_TAP_DREXIT1;
        _SET_BIT_(1);
        break;

      case JTAG_TAP_DREXIT1:
        if (toState == JTAG_TAP_DRPAUSE || toState == JTAG_TAP_DREXIT2) {
          fromState = JTAG_TAP_DRPAUSE;
          _SET_BIT_(0);
        } else {
          fromState = JTAG_TAP_DRUPDATE;
          _SET_BIT_(1);
        }
        break;

      case JTAG_TAP_DRPAUSE:
        fromState = JTAG_TAP_DREXIT2;
        _SET_BIT_(1);
        break;

      case JTAG_TAP_DREXIT2:
        if (toState == JTAG_TAP_DRSHIFT) {
          fromState = JTAG_TAP_DRSHIFT;
          _SET_BIT_(0);
        } else {
          fromState = JTAG_TAP_DRUPDATE;
          _SET_BIT_(1);
        }
        break;

      case JTAG_TAP_DRUPDATE:
        if (toState == JTAG_TAP_IDLE) {
          fromState = JTAG_TAP_IDLE;
          _SET_BIT_(0);
        } else {
          fromState = JTAG_TAP_DRSELECT;
          _SET_BIT_(1);
        }
        break;

      case JTAG_TAP_IRSELECT:
        if (JTAG_IN_IR(toState)) {
          fromState = JTAG_TAP_IRCAPTURE;
          _SET_BIT_(0);
        } else {
          fromState = JTAG_TAP_RESET;
          _SET_BIT_(1);
        }
        break;

      case JTAG_TAP_IRCAPTURE:
        if (toState == JTAG_TAP_IRSHIFT) {
          fromState = JTAG_TAP_IRSHIFT;
          _SET_BIT_(0);
        } else {
          fromState = JTAG_TAP_IREXIT1;
          _SET_BIT_(1);
        }
        break;

      case JTAG_TAP_IRSHIFT:
        fromState = JTAG_TAP_IREXIT1;
        _SET_BIT_(1);
        break;

      case JTAG_TAP_IREXIT1:
        if (toState == JTAG_TAP_IRPAUSE || toState == JTAG_TAP_IREXIT2) {
          fromState = JTAG_TAP_IRPAUSE;
          _SET_BIT_(0);
        } else {
          fromState = JTAG_TAP_IRUPDATE;
          _SET_BIT_(1);
        }
        break;

      case JTAG_TAP_IRPAUSE:
        fromState = JTAG_TAP_IREXIT2;
        _SET_BIT_(1);
        break;

      case JTAG_TAP_IREXIT2:
        if (toState == JTAG_TAP_IRSHIFT) {
          fromState = JTAG_TAP_IRSHIFT;
          _SET_BIT_(0);
        } else {
          fromState = JTAG_TAP_IRUPDATE;
          _SET_BIT_(1);
        }
        break;

      case JTAG_TAP_IRUPDATE:
        if (toState == JTAG_TAP_IDLE) {
          fromState = JTAG_TAP_IDLE;
          _SET_BIT_(0);
        } else {
          fromState = JTAG_TAP_DRSELECT;
          _SET_BIT_(1);
        }
        break;
    }
  }
#undef _SET_BIT_
  return (TMS_SeqInfo)(sequence & 0xFFFF);
}

/**
 * 从当前TAP状态和给定的TMS，返回下一个TAP状态
 */
enum JTAG_TAP_State JtagNextState(enum JTAG_TAP_State fromState, int tms) {
  assert(fromState >= JTAG_TAP_RESET && fromState <= JTAG_TAP_IRUPDATE);
  enum JTAG_TAP_State nextState;
  switch (fromState) {
    case JTAG_TAP_RESET:
      nextState = tms ? JTAG_TAP_RESET : JTAG_TAP_IDLE;
      break;

    case JTAG_TAP_IDLE:
      nextState = tms ? JTAG_TAP_DRSELECT : JTAG_TAP_IDLE;
      break;

    case JTAG_TAP_DRSELECT:
      nextState = tms ? JTAG_TAP_IRSELECT : JTAG_TAP_DRCAPTURE;
      break;

    case JTAG_TAP_DRCAPTURE:
      nextState = tms ? JTAG_TAP_DREXIT1 : JTAG_TAP_DRSHIFT;
      break;

    case JTAG_TAP_DRSHIFT:
      nextState = tms ? JTAG_TAP_DREXIT1 : JTAG_TAP_DRSHIFT;
      break;

    case JTAG_TAP_DREXIT1:
      nextState = tms ? JTAG_TAP_DRUPDATE : JTAG_TAP_DRPAUSE;
      break;

    case JTAG_TAP_DRPAUSE:
      nextState = tms ? JTAG_TAP_DREXIT2 : JTAG_TAP_DRPAUSE;
      break;

    case JTAG_TAP_DREXIT2:
      nextState = tms ? JTAG_TAP_DRUPDATE : JTAG_TAP_DRSHIFT;
      break;

    case JTAG_TAP_DRUPDATE:
      nextState = tms ? JTAG_TAP_DRSELECT : JTAG_TAP_IDLE;
      break;

    case JTAG_TAP_IRSELECT:
      nextState = tms ? JTAG_TAP_RESET : JTAG_TAP_IRCAPTURE;
      break;

    case JTAG_TAP_IRCAPTURE:
      nextState = tms ? JTAG_TAP_IREXIT1 : JTAG_TAP_IRSHIFT;
      break;

    case JTAG_TAP_IRSHIFT:
      nextState = tms ? JTAG_TAP_IREXIT1 : JTAG_TAP_IRSHIFT;
      break;

    case JTAG_TAP_IREXIT1:
      nextState = tms ? JTAG_TAP_IRUPDATE : JTAG_TAP_IRPAUSE;
      break;

    case JTAG_TAP_IRPAUSE:
      nextState = tms ? JTAG_TAP_IREXIT2 : JTAG_TAP_IRPAUSE;
      break;

    case JTAG_TAP_IREXIT2:
      nextState = tms ? JTAG_TAP_IRUPDATE : JTAG_TAP_IRSHIFT;
      break;

    case JTAG_TAP_IRUPDATE:
      nextState = tms ? JTAG_TAP_DRSELECT : JTAG_TAP_IDLE;
      break;
  }
  return nextState;
}

/**
 * 计算多少个TMS信号有多少个电平状态
 * TMS:TMS信号，LSB
 * count：TMS信号位数
 * 比如：1001110，count=7，有四个电平状态：1周期的低电平，3周期的高电平，2周期的低电平，1周期的高电平
 */
int JtagCalTmsLevelState(uint32_t tms, int count) {
  int n = 0, i;
  uint32_t tms_s = (tms << 1) | (tms & 0x1);
  uint32_t diff = tms_s ^ tms;
  if (count == 0) return 0;
  for (i = 0; i < count; i++) {
    if (diff & 0x1) n++;
    diff >>= 1;
  }
  return n + 1;
}

const char *JtagStateToStr(enum JTAG_TAP_State tap_state) {
#define X(_s) \
  if (tap_state == _s) return #_s;
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
