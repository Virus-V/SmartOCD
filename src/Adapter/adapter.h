/*
 * adapter.h
 * SmartOCD
 *
 *  Created on: 2018-1-29
 *      Author: virusv
 */

#ifndef SRC_ADAPTER_INCLUDE_ADAPTER_H_
#define SRC_ADAPTER_INCLUDE_ADAPTER_H_

#include "smartocd.h"
#include "Library/jtag/jtag.h"

/**
 * Adapter 接口状态码
 */
enum {
	ADPT_SUCCESS = 0,	// 成功
	ADPT_FAILED,	// 失败
	ADPT_ERR_TRANSPORT_ERROR,	// 底层传输错误
	ADPT_ERR_PROTOCOL_ERROR,	// 传输协议错误
	ADPT_ERR_NO_DEVICE,	// 找不到设备
	ADPT_ERR_UNSUPPORT,	// 不支持的操作
	ADPT_ERR_INTERNAL_ERROR,	// 内部错误,不是由于Adapter功能部分造成的失败
	ADPT_ERR_BAD_PARAMETER,	// 无效的参数
};

/* 仿真器对象 */
typedef struct adapter *Adapter;

// 仿真器的状态
enum adapterStatus {
	ADPT_STATUS_CONNECTED,	// Adapter已连接
	ADPT_STATUS_DISCONNECT,	// Adapter已断开
	ADPT_STATUS_RUNING,		// Adapter正在运行
	ADPT_STATUS_IDLE		// Adapter空闲
};

/**
 * Status - 设置仿真器状态
 * 如果仿真器有状态指示灯的话,则可以通过这个状态来控制这些指示灯
 * 参数:
 * 	self:Adapter对象自身
 * 	status:仿真器的状态
 * 返回:
 * 	ADPT_SUCCESS:成功
 * 	ADPT_FAILED:失败
 * 	或者其他错误
 */
typedef int (*ADPT_SET_STATUS)(
		IN Adapter self,
		IN enum adapterStatus status
);

/**
 * SetFrequent - 设置仿真器的工作频率
 * 参数:
 * 	self:Adapter对象自身
 * 	clock:工作频率(Hz)
 * 返回:
 * 	ADPT_SUCCESS:成功
 * 	ADPT_FAILED:失败
 * 	或者其他错误
 */
typedef int (*ADPT_SET_FREQUENT)(
		IN Adapter self,
		IN unsigned int freq
);

/**
 * target复位类型
 */
enum targetResetType {
	ADPT_RESET_SYSTEM_RESET,	// 全部系统复位
	ADPT_RESET_DEBUG_RESET,	// 调试系统复位
};

/**
 * Reset - 仿真器复位
 * 参数:
 * 	self:Adapter对象自身
 * 	type:目标复位类型
 * 返回:
 * 	ADPT_SUCCESS:成功
 * 	ADPT_FAILED:失败
 * 	或者其他错误
 */
typedef int (*ADPT_RESET)(
		IN Adapter self,
		IN enum targetResetType type
);

/**
 * 仿真器工作模式,JTAG模式还是DAP模式
 */
enum transfertMode {
	ADPT_MODE_JTAG,
	ADPT_MODE_SWD,
	ADPT_MODE_MAX
};

/**
 * SetTransferMode - 设置传输类型:DAP还是JTAG
 * 参数:
 *	self:Adapter对象自身
 *	mode:仿真器工作模式,DAP还是JTAG
 * 返回:
 * 	ADPT_SUCCESS:成功
 * 	ADPT_FAILED:失败
 * 	或者其他错误
 */
typedef int (*ADPT_SET_TRANSFER_MODE)(
		IN Adapter self,
		IN enum transfertMode mode
);

// SW和JTAG引脚映射,用于指定ADPT_JTAG_PINS服务的pinSelect参数
#define SWJ_PIN_SWCLK_TCK_POS		0       // SWCLK/TCK引脚在pinMask参数的对应位置
#define SWJ_PIN_SWDIO_TMS_POS		1       // SWDIO/TMS引脚在pinMask参数的对应位置
#define SWJ_PIN_TDI_POS				2       // TDI引脚在pinMask参数的对应位置
#define SWJ_PIN_TDO_POS				3       // TDO引脚在pinMask参数的对应位置
#define SWJ_PIN_nTRST_POS			5       // nTRST引脚在pinMask参数的对应位置
#define SWJ_PIN_nRESET_POS			7       // nRESET引脚在pinMask参数的对应位置

#define SWJ_PIN_SWCLK_TCK		0x1 << SWJ_PIN_SWCLK_TCK_POS	// SWCLK/TCK
#define SWJ_PIN_SWDIO_TMS		0x1 << SWJ_PIN_SWDIO_TMS_POS	// SWDIO/TMS
#define SWJ_PIN_TDI				0x1 << SWJ_PIN_TDI_POS			// TDI
#define SWJ_PIN_TDO				0x1 << SWJ_PIN_TDO_POS			// TDO
#define SWJ_PIN_nTRST			0x1 << SWJ_PIN_nTRST_POS		// nTRST
#define SWJ_PIN_nRESET			0x1 << SWJ_PIN_nRESET_POS       // nRESET

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
typedef int (*ADPT_JTAG_PINS)(
		IN Adapter self,
		IN uint8_t pinMask,
		IN uint8_t pinDataOut,
		OUT uint8_t *pinDataIn,
		IN unsigned int pinWait
);

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
typedef int (*ADPT_JTAG_EXCHANGE_DATA)(
		IN Adapter self,
		IN uint8_t *data,
		IN unsigned int bitCount
);

/**
 * JtagIdle - 在Idle状态等待几个周期
 * 会将该动作加入Pending队列,不会立即执行
 * 参数:
 * 	self:Adapter对象自身
 * 	clkCount:在JTAG的Idle状态等待的时钟周期
 * 返回:
 */
typedef int (*ADPT_JTAG_IDLE)(
		IN Adapter self,
		IN unsigned int clkCount
);

/**
 * JtagToState - 切换到JTAG状态机的某个状态
 * 会将该动作加入Pending队列,不会立即执行
 * 参数:
 * 	self:Adapter对象自身
 * 	toState:JTAG状态机的最终状态
 * 返回:
 */
typedef int (*ADPT_JTAG_TO_STATE)(
		IN Adapter self,
		IN enum JTAG_TAP_State toState
);

/**
 * JtagCommit - 提交Pending的动作
 * 参数:
 * 	self:Adapter对象自身
 * 返回:
 * 	ADPT_SUCCESS:成功
 * 	ADPT_FAILED:失败
 * 	或者其他错误
 */
typedef int (*ADPT_JTAG_COMMIT)(
		IN Adapter self
);

/**
 * JtagCleanPending - 清除pending的动作
 * 参数:
 * 	self:Adapter对象自身
 * 返回:
 * 	ADPT_SUCCESS:成功
 * 	ADPT_FAILED:失败
 * 	或者其他错误
 */
typedef int (*ADPT_JTAG_CLEAN_PENDING)(
		IN Adapter self
);

/* DAP寄存器类型 */
enum dapRegType {
	ADPT_DAP_DP_REG,	// DP类型的寄存器
	ADPT_DAP_AP_REG,	// AP类型的寄存器
};

/**
 * DapSingleRead - 单次读:AP或者DP,寄存器地址
 * 会将该动作加入Pending队列,不会立即执行
 * 参数:
 * 	self:Adapter对象自身
 * 	type:寄存器类型,DP还是AP
 * 	reg:reg地址
 * 	data:将寄存器的内容写入到该参数指定的地址
 * 返回:
 */
typedef int (*ADPT_DAP_SINGLE_READ)(
		IN Adapter self,
		IN enum dapRegType type,
		IN int reg,
		OUT uint32_t *data
);

/**
 * DapSingleWrite - 单次写:AP或者DP,寄存器地址
 * 会将该动作加入Pending队列,不会立即执行
 * 参数:
 * 	self:Adapter对象自身
 * 	type:寄存器类型,DP还是AP
 * 	reg:reg地址
 * 	data:将该参数的数据写入到寄存器
 * 返回:
 */
typedef int (*ADPT_DAP_SINGLE_WRITE)(
		IN Adapter self,
		IN enum dapRegType type,
		IN int reg,
		IN uint32_t data
);

/**
 * DapMultiRead - 多次读:AP或者DP,寄存器地址
 * 会将该动作加入Pending队列,不会立即执行
 * 参数:
 * 	self:Adapter对象自身
 * 	type:寄存器类型,DP还是AP
 * 	reg:reg地址
 * 	count:读取的次数
 * 	data:将寄存器的内容写入到该参数指定的数组
 * 返回:
 */
typedef int (*ADPT_DAP_MULTI_READ)(
		IN Adapter self,
		IN enum dapRegType type,
		IN int reg,
		IN int count,
		OUT uint32_t *data
);

/**
 * DapMultiWrite - 多次写:AP或者DP,寄存器地址
 * 会将该动作加入Pending队列,不会立即执行
 * 参数:
 * 	self:Adapter对象自身
 * 	type:寄存器类型,DP还是AP
 * 	reg:reg地址
 * 	count:读取的次数
 * 	data:将该参数指定数组的数据写入到寄存器
 * 返回:
 */
typedef int (*ADPT_DAP_MULTI_WRITE)(
		IN Adapter self,
		IN enum dapRegType type,
		IN int reg,
		IN int count,
		IN uint32_t *data
);

/**
 * DapCommit - 提交Pending的动作
 * 参数:
 * 	self:Adapter对象自身
 * 返回:
 * 	ADPT_SUCCESS:成功
 * 	ADPT_FAILED:失败
 * 	或者其他错误
 */
typedef int (*ADPT_DAP_COMMIT)(
		IN Adapter self
);

/**
 * DapCleanPending - 清除pending的动作
 * 参数:
 * 	self:Adapter对象自身
 * 返回:
 * 	ADPT_SUCCESS:成功
 * 	ADPT_FAILED:失败
 * 	或者其他错误
 */
typedef int (*ADPT_DAP_CLEAN_PENDING)(
		IN Adapter self
);

/**
 * Adapter接口对象
 */
struct adapter {
	/* 属性 */
	const enum JTAG_TAP_State currState;		// JTAG 当前状态
	const enum transfertMode currTransMode;	// 当前传输协议

	/* 服务 */
	ADPT_SET_STATUS SetStatus;				// 仿真器状态指示
	ADPT_SET_FREQUENT SetFrequent;			// 设置仿真器的工作频率
	ADPT_RESET Reset;						// 仿真器复位
	ADPT_SET_TRANSFER_MODE SetTransferMode;	// 设置传输类型:DAP还是JTAG

	ADPT_JTAG_PINS JtagPins;					// 读写仿真器的JTAG引脚
	ADPT_JTAG_EXCHANGE_DATA JtagExchangeData;	// 交换TDI和TDO的数据
	ADPT_JTAG_IDLE JtagIdle;					// 在Idle状态等待几个周期
	ADPT_JTAG_TO_STATE JtagToState;				// 切换到JTAG状态机的某个状态
	ADPT_JTAG_COMMIT JtagCommit;				// 提交Pending的动作
	ADPT_JTAG_CLEAN_PENDING JtagCleanPending;	// 清除pending的动作

	ADPT_DAP_SINGLE_READ DapSingleRead;		// 单次读:AP或者DP,寄存器编号
	ADPT_DAP_SINGLE_WRITE DapSingleWrite;	// 单次写:AP或者DP,寄存器编号
	ADPT_DAP_MULTI_READ DapMultiRead;		// 连续读
	ADPT_DAP_MULTI_WRITE DapMultiWrite;		// 连续写
	ADPT_DAP_COMMIT DapCommit;				// 提交Pending动作
	ADPT_DAP_CLEAN_PENDING DapCleanPending;	// 清除Pending的动作
};

/**
 * 在一串数据中获得第n个二进制位，n从0开始
 * lsb
 * data:数据存放的位置指针
 * n:第几位，最低位是第0位
 */
#define GET_Nth_BIT(data,n) ((*(CAST(uint8_t *, (data)) + ((n)>>3)) >> ((n) & 0x7)) & 0x1)
/**
 * 设置data的第n个二进制位
 * data:数据存放的位置
 * n：要修改哪一位，从0开始
 * val：要设置的位，0或者1，只使用最低位
 */
#define SET_Nth_BIT(data,n,val) do{	\
	uint8_t tmp_data = *(CAST(uint8_t *, (data)) + ((n)>>3)); \
	tmp_data &= ~(1 << ((n) & 0x7));	\
	tmp_data |= ((val) & 0x1) << ((n) & 0x7);	\
	*(CAST(uint8_t *, (data)) + ((n)>>3)) = tmp_data;	\
}while(0);

#endif /* SRC_ADAPTER_ADAPTER_H_ */
