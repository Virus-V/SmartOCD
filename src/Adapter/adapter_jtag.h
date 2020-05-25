/***
 * @Author: Virus.V
 * @Date: 2020-05-25 12:53:07
 * @LastEditors: Virus.V
 * @LastEditTime: 2020-05-25 14:05:51
 * @Description: JTAG Transport API
 * @Email: virusv@live.com
 */

#ifndef SRC_ADAPTER_ADAPTER_JTAG_H_
#define SRC_ADAPTER_ADAPTER_JTAG_H_

#include "Library/jtag/jtag.h"
#include "smartocd.h"

/* 仿真器对象 */
typedef struct adapter *Adapter;
/* 传输模式接口 */
struct transport;

// SW和JTAG引脚映射,用于指定ADPT_JTAG_PINS服务的pinSelect参数
#define SWJ_PIN_SWCLK_TCK_POS 0 // SWCLK/TCK引脚在pinMask参数的对应位置
#define SWJ_PIN_SWDIO_TMS_POS 1 // SWDIO/TMS引脚在pinMask参数的对应位置
#define SWJ_PIN_TDI_POS 2       // TDI引脚在pinMask参数的对应位置
#define SWJ_PIN_TDO_POS 3       // TDO引脚在pinMask参数的对应位置
#define SWJ_PIN_nTRST_POS 5     // nTRST引脚在pinMask参数的对应位置
#define SWJ_PIN_nRESET_POS 7    // nRESET引脚在pinMask参数的对应位置

#define SWJ_PIN_SWCLK_TCK 0x1 << SWJ_PIN_SWCLK_TCK_POS // SWCLK/TCK
#define SWJ_PIN_SWDIO_TMS 0x1 << SWJ_PIN_SWDIO_TMS_POS // SWDIO/TMS
#define SWJ_PIN_TDI 0x1 << SWJ_PIN_TDI_POS             // TDI
#define SWJ_PIN_TDO 0x1 << SWJ_PIN_TDO_POS             // TDO
#define SWJ_PIN_nTRST 0x1 << SWJ_PIN_nTRST_POS         // nTRST
#define SWJ_PIN_nRESET 0x1 << SWJ_PIN_nRESET_POS       // nRESET

/**
 * JtagPins - 读写仿真器的JTAG引脚
 * 参数:
 *	self:Adapter对象自身
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
typedef int (*ADPT_JTAG_PINS)(IN Adapter self, IN uint8_t pinMask,
                              IN uint8_t pinDataOut, OUT uint8_t *pinDataIn,
                              IN unsigned int pinWait);

/**
 * JtagExchangeData - 交换TDI和TDO的数据
 * 会将该动作加入Pending队列,不会立即执行
 * 参数:
 * 	self:Adapter对象自身
 * 	data:数据缓冲区,必须可读可写
 *	bitCount:要进行交换的二进制位个数
 * 返回:
 * 	ADPT_SUCCESS:成功
 * 	ADPT_FAILED:失败
 * 	或者其他错误
 */
typedef int (*ADPT_JTAG_EXCHANGE_DATA)(IN Adapter self, IN uint8_t *data,
                                       IN unsigned int bitCount);

/**
 * JtagIdle - 在Idle状态等待几个周期
 * 会将该动作加入Pending队列,不会立即执行
 * 参数:
 * 	self:Adapter对象自身
 * 	clkCount:在JTAG的Idle状态等待的时钟周期
 * 返回:
 */
typedef int (*ADPT_JTAG_IDLE)(IN Adapter self, IN unsigned int clkCount);

/**
 * JtagToState - 切换到JTAG状态机的某个状态
 * 会将该动作加入Pending队列,不会立即执行
 * 参数:
 * 	self:Adapter对象自身
 * 	toState:JTAG状态机的最终状态
 * 返回:
 */
typedef int (*ADPT_JTAG_TO_STATE)(IN Adapter self,
                                  IN enum JTAG_TAP_State toState);

/**
 * JtagCommit - 提交Pending的动作
 * 参数:
 * 	self:Adapter对象自身
 * 返回:
 * 	ADPT_SUCCESS:成功
 * 	ADPT_FAILED:失败
 * 	或者其他错误
 */
typedef int (*ADPT_JTAG_COMMIT)(IN Adapter self);

/**
 * JtagCleanPending - 清除pending的动作
 * 参数:
 * 	self:Adapter对象自身
 * 返回:
 * 	ADPT_SUCCESS:成功
 * 	ADPT_FAILED:失败
 * 	或者其他错误
 */
typedef int (*ADPT_JTAG_CLEAN_PENDING)(IN Adapter self);

/* JTAG 传输接口 */
struct jtagTrasnport {
  struct trasnport header;

  enum JTAG_TAP_State currState; // JTAG 当前状态

  ADPT_JTAG_PINS JtagPins;                  // 读写仿真器的JTAG引脚
  ADPT_JTAG_EXCHANGE_DATA JtagExchangeData; // 交换TDI和TDO的数据
  ADPT_JTAG_IDLE JtagIdle;                  // 在Idle状态等待几个周期
  ADPT_JTAG_TO_STATE JtagToState; // 切换到JTAG状态机的某个状态
  ADPT_JTAG_COMMIT JtagCommit;    // 提交Pending的动作
  ADPT_JTAG_CLEAN_PENDING JtagCleanPending; // 清除pending的动作
};

#endif
