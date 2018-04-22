/*
 * TAP.h
 *
 *  Created on: 2018-3-29
 *      Author: virusv
 */

#ifndef SRC_LIB_TAP_H_
#define SRC_LIB_TAP_H_

#include "smart_ocd.h"
#include "misc/list/list.h"
#include "debugger/adapter.h"
#include "lib/JTAG.h"

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
 * TAP对象
 * 提供jTAG通用接口
 */
typedef struct TAPObject TAPObject;
struct TAPObject {
	AdapterObject *adapterObj;	// Adapter对象
	enum JTAG_TAP_Status currentStatus;	// TAP状态机当前状态
	int TAP_actived;
	int IR_Bytes;	// IR固定字节
	uint16_t TAP_Count, *TAP_Info;
	list_t *jtagInstrQueue;	// JTAG指令队列，元素类型：struct JTAG_Instr
	list_node_t *currProcessing;	// 下一个将要处理的指令
	int JTAG_SequenceCount;	// 一共有多少个Sequence
};

// TAP对象构造函数和析构函数
BOOL __CONSTRUCT(TAP)(TAPObject *tapObj, AdapterObject *adapterObj);
void __DESTORY(TAP)(TAPObject *tapObj);

BOOL TAP_Reset(TAPObject *tapObj, BOOL hard, uint32_t pinWait);
BOOL TAP_SetInfo(TAPObject *tapObj, uint16_t tapCount, uint16_t *IR_Len);
BOOL TAP_Get_IDCODE(TAPObject *tapObj, uint32_t *idCode);
BOOL TAP_IR_Write(TAPObject *tapObj, uint16_t index, uint32_t ir);
BOOL TAP_DR_Exchange(TAPObject *tapObj, uint16_t index, uint8_t count, uint8_t *data);
BOOL TAP_Execute(TAPObject *tapObj);

#endif /* SRC_LIB_TAP_H_ */
