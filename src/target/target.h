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

enum JTAG_instructions{
	/**
	 * 写入IR寄存器
	 */
	JTAG_INS_WRITE_IR,
	JTAG_INS_EXCHANGE_DR,
};

/**
 * JTAG 命令对象
 */
struct JTAG_Instr{
	enum JTAG_instructions type;	// 指令类型
	uint16_t TAP_Index;
	union {
		uint32_t IR_Data;	// ir数据
		struct {
			int length;	// 数据进行编码后的长度
			uint8_t *data;	// 指向TDI将要输出的数据，LSB
			uint8_t bitCount:7;
			uint8_t segment:1;
			uint8_t segment_pos;	// 切分的位原本的位置
		} DR;
	} info;
};

/**
 * Target对象
 * 对外开放jTAG和SWD通用接口
 */
typedef struct TargetObject TargetObject;
struct TargetObject {
	AdapterObject *adapterObj;	// Adapter对象
	enum JTAG_TAP_Status currentStatus;	// TAP状态机当前状态
	int TAP_actived;
	int IR_Bytes;	// IR固定字节
	uint16_t TAP_Count, *TAP_Info;
	list_t *jtagInstrQueue;	// JTAG指令队列，元素类型：struct JTAG_Instr
	list_node_t *currProcessing;	// 下一个将要处理的指令
	int JTAG_SequenceCount;	// 一共有多少个Sequence
	// TODO SWD部分

};

// Target对象构造函数和析构函数
BOOL __CONSTRUCT(Target)(TargetObject *targetObj, AdapterObject *adapterObj);
void __DESTORY(Target)(TargetObject *targetObj);

BOOL target_SetClock(TargetObject *targetObj, uint32_t clockHz);
BOOL target_SelectTrasnport(TargetObject *targetObj, enum transportType type);
BOOL target_JTAG_TAP_Reset(TargetObject *targetObj, BOOL hard, uint32_t pinWait);
BOOL target_JTAG_Set_TAP_Info(TargetObject *targetObj, uint16_t tapCount, uint16_t *IR_Len);
BOOL target_JTAG_Get_IDCODE(TargetObject *targetObj, uint32_t *idCode);
BOOL target_JTAG_IR_Write(TargetObject *targetObj, uint16_t index, uint32_t ir);
BOOL target_JTAG_DR_Exchange(TargetObject *targetObj, uint16_t index, uint8_t count, uint8_t *data);
BOOL target_JTAG_Execute(TargetObject *targetObj);

#endif /* SRC_TARGET_TARGET_H_ */
