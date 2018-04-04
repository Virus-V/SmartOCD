/*
 * target.h
 *
 *  Created on: 2018-3-29
 *      Author: virusv
 */

#ifndef SRC_TARGET_TARGET_H_
#define SRC_TARGET_TARGET_H_


#include "smart_ocd.h"
#include "misc/list/list.h"
#include "debugger/adapter.h"
#include "target/JTAG.h"
#include "arch/ARM/ADI/adiv5_SWD.h"

/**
 * JTAG 命令对象
 */
struct JTAG_Instr{
	enum JTAG_TAP_Status Status;	// 要切换到的状态
	struct JTAG_TAP *TAP;
	struct {
		uint8_t *tdiData;	// 指向TDI将要输出的数据，LSB
		uint8_t ctrl;	// 记录TDI要传输的数据个数，最大不超过64个k[5:0]，0代表64，最高位为TDO capture
	} data;
};

/**
 * JTAG TAP对象
 */
struct JTAG_TAP{
	uint32_t IDCODE;	// 该TAP的IDCODE
	uint16_t IR_Len;		// IR 寄存器长度
	uint8_t NeedClockNum;	// 从当前状态切换到该TAP所需要的状态需要的时钟周期数
	list_t *instructQueue;	// 该TAP待执行的指令
};

// JTAG指令队列
struct JTAG_InstrQueueEle{
	enum JTAG_TAP_Status Status;	// 要切换到的状态
	list_t *instructQueue;	// jtag指令：对象
};

/**
 * Target对象
 * 对外开放jTAG和SWD通用接口
 */
typedef struct TargetObject TargetObject;
struct TargetObject {
	AdapterObject *adapterObj;	// Adapter对象
	enum JTAG_TAP_Status currentStatus;	// TAP状态机当前状态
	list_t *taps;	// jtag扫描链中的TAP对象
	list_t *jtagInstrQueue;	// JTAG指令队列，元素对象：struct JTAG_InstrQueueEle
	list_node_t *nextToProc;	// 下一个将要处理的指令
	// TODO SWD部分

};

// Target对象构造函数和析构函数
BOOL __CONSTRUCT(Target)(TargetObject *targetObj, AdapterObject *adapterObj);
void __DESTORY(Target)(TargetObject *targetObj);

/**
 * 设置仿真器通信频率（Hz）
 */
BOOL target_SetClock(TargetObject *targetObj, uint32_t clockHz);

/**
 * 切换仿真模式
 * SWD和JTAG等等
 */
BOOL target_SelectTrasnport(TargetObject *targetObj, enum transportType type);

int target_JTAG_Add_TAP(TargetObject *targetObj, uint16_t irLen);
void target_JTAG_Remove_TAP(TargetObject *targetObj, int index);

BOOL target_JTAG_TAP_ToStatus(TargetObject *targetObj, int index, enum JTAG_TAP_Status toStatus);
BOOL target_JTAG_TAP_IRWrite(TargetObject *targetObj, int index, uint8_t *dataEx);
BOOL target_JTAG_TAP_DRExchange(TargetObject *targetObj, int index, uint8_t DRLen, uint8_t *dataEx);
BOOL target_JTAG_TAP_Exchange(TargetObject *targetObj, int index, enum JTAG_TAP_Status toStatus, uint8_t ctrl, uint8_t *dataEx);
BOOL target_JTAG_Execute(TargetObject *targetObj);

#endif /* SRC_TARGET_TARGET_H_ */
