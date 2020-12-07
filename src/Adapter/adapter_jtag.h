/**
 * src/Adapter/adapter_jtag.h
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

#ifndef SRC_ADAPTER_ADAPTER_JTAG_H_
#define SRC_ADAPTER_ADAPTER_JTAG_H_

#include "smartocd.h"

#include "Adapter/adapter.h"
#include "Library/jtag/jtag.h"

// SW和JTAG引脚映射,用于指定SKILL_JTAG_PINS服务的pinSelect参数
#define JTAG_PIN_SWCLK_TCK_POS 0 // SWCLK/TCK引脚在pinMask参数的对应位置
#define JTAG_PIN_SWDIO_TMS_POS 1 // SWDIO/TMS引脚在pinMask参数的对应位置
#define JTAG_PIN_TDI_POS 2       // TDI引脚在pinMask参数的对应位置
#define JTAG_PIN_TDO_POS 3       // TDO引脚在pinMask参数的对应位置
#define JTAG_PIN_nTRST_POS 5     // nTRST引脚在pinMask参数的对应位置
#define JTAG_PIN_nRESET_POS 7    // nRESET引脚在pinMask参数的对应位置

#define JTAG_PIN_SWCLK_TCK 0x1 << JTAG_PIN_SWCLK_TCK_POS // SWCLK/TCK
#define JTAG_PIN_SWDIO_TMS 0x1 << JTAG_PIN_SWDIO_TMS_POS // SWDIO/TMS
#define JTAG_PIN_TDI 0x1 << JTAG_PIN_TDI_POS             // TDI
#define JTAG_PIN_TDO 0x1 << JTAG_PIN_TDO_POS             // TDO
#define JTAG_PIN_nTRST 0x1 << JTAG_PIN_nTRST_POS         // nTRST
#define JTAG_PIN_nRESET 0x1 << JTAG_PIN_nRESET_POS       // nRESET

/* JTAG 能力集对象 */
typedef const struct jtagSkill *JtagSkill;

/**
 * JtagPins - 读写仿真器的JTAG引脚
 * 参数:
 *	self:JtagSkill对象自身
 *	pinMask:选中哪些引脚,见上面的宏定义
 *	pinDataOut:将对应的二进制位写入到pinMask选中的引脚中
 *	pinDataIn:设置引脚后,等引脚的信号稳定,读取的全部引脚的值
 *	pinWait:死区时间,引脚电平切换到信号稳定的时间
 *		0 = no wait
 * 		1 .. 3000000 = time in µs (max 3s)
 * 返回:
 * 	ADPT_SUCCESS:成功
 * 	ADPT_FAILED:失败
 * 	或者其他错误
 */
typedef int (*SKILL_JTAG_PINS)(IN JtagSkill self, IN uint8_t pinMask, IN uint8_t pinDataOut,
                               OUT uint8_t *pinDataIn, IN unsigned int pinWait);

/**
 * JtagExchangeData - 交换TDI和TDO的数据
 * 会将该动作加入Pending队列,不会立即执行
 * 按字节依次LSB方式发出
 * 参数:
 * 	self:JtagSkill对象自身
 * 	data:数据缓冲区,必须可读可写
 *	bitCount:要进行交换的二进制位个数
 * 返回:
 * 	ADPT_SUCCESS:成功
 * 	ADPT_FAILED:失败
 * 	或者其他错误
 */
typedef int (*SKILL_JTAG_EXCHANGE_DATA)(IN JtagSkill self, IN uint8_t *data, IN unsigned int bitCount);

/**
 * JtagIdle - 在Idle状态等待几个周期
 * 会将该动作加入Pending队列,不会立即执行
 * 参数:
 * 	self:JtagSkill对象自身
 * 	clkCount:在JTAG的Idle状态等待的时钟周期
 * 返回:
 */
typedef int (*SKILL_JTAG_IDLE)(IN JtagSkill self, IN unsigned int clkCount);

/**
 * JtagToState - 切换到JTAG状态机的某个状态
 * 会将该动作加入Pending队列,不会立即执行
 * 参数:
 * 	self:JtagSkill对象自身
 * 	toState:JTAG状态机的最终状态
 * 返回:
 */
typedef int (*SKILL_JTAG_TO_STATE)(IN JtagSkill self, IN enum JTAG_TAP_State toState);

/**
 * JtagCommit - 提交Pending的动作
 * 参数:
 * 	self:JtagSkill对象自身
 * 返回:
 * 	ADPT_SUCCESS:成功
 * 	ADPT_FAILED:失败
 * 	或者其他错误
 */
typedef int (*SKILL_JTAG_COMMIT)(IN JtagSkill self);

/**
 * JtagCancel - 清除pending的动作
 * 参数:
 * 	self:JtagSkill对象自身
 * 返回:
 * 	ADPT_SUCCESS:成功
 * 	ADPT_FAILED:失败
 * 	或者其他错误
 */
typedef int (*SKILL_JTAG_CANCEL)(IN JtagSkill self);

/* JTAG 传输接口 */
struct jtagSkill {
  struct skill header;

  const enum JTAG_TAP_State currState; // JTAG 当前状态

  SKILL_JTAG_PINS Pins;                  // 读写仿真器的JTAG引脚
  SKILL_JTAG_EXCHANGE_DATA ExchangeData; // 交换TDI和TDO的数据
  SKILL_JTAG_IDLE Idle;                  // 在Idle状态等待几个周期
  SKILL_JTAG_TO_STATE ToState;           // 切换到JTAG状态机的某个状态
  SKILL_JTAG_COMMIT Commit;              // 提交Pending的动作
  SKILL_JTAG_CANCEL Cancel;              // 清除pending的动作
};

/* 获得JTAG 能力接口 */
#define ADAPTER_GET_JTAG_SKILL(adapter) (CAST(JtagSkill, Adapter_GetSkill((adapter), ADPT_SKILL_JTAG)))

#endif
