/*
 * target.c
 *
 *  Created on: 2018-3-29
 *      Author: virusv
 */

#include <stdlib.h>
#include <string.h>
#include "target/target.h"
#include "misc/log.h"

// 检测Transport是否匹配
#define TRANSPORT_MATCH(tp,type) ((tp)->adapterObj->currTrans == (type))

BOOL target_JTAG_TAP_Reset(TargetObject *targetObj, BOOL hard, uint32_t pinWait);

// 在销毁Target对象时释放TAP对象
static void free_tap(struct JTAG_TAP *tapObj){
	// 释放tap的指令列表
	list_destroy(tapObj->instructQueue);
	free(tapObj);
}
/**
 * Target对象构造函数
 */
BOOL __CONSTRUCT(Target)(TargetObject *targetObj, AdapterObject *adapterObj){
	assert(targetObj != NULL && adapterObj != NULL);
	// 创建TAP列表表头
	targetObj->taps = list_new();
	if(targetObj->taps == NULL){
		log_warn("Init TAP list failed.");
		return FALSE;
	}
	// 初始化链表的匹配和元素释放函数
	targetObj->taps->free = (void (*)(void *))free_tap;
	//targetObj->taps->match = ;
	// 创建指令队列表头
	targetObj->jtagInstrQueue = list_new();
	if(targetObj->jtagInstrQueue == NULL){
		log_warn("Init JTAG Instruction queue failed.");
		list_destroy(targetObj->taps);
		return FALSE;
	}

	targetObj->adapterObj = adapterObj;
	targetObj->currentStatus = JTAG_TAP_RESET;
	// 初始化Adapter
	if(adapterObj->Init(adapterObj) == FALSE){
		log_warn("Adapter initialization failed.");
		list_destroy(targetObj->taps);
		targetObj->taps = NULL;
		return FALSE;
	}
	return TRUE;
}

/**
 * Target对象析构函数
 */
void __DESTORY(Target)(TargetObject *targetObj){
	assert(targetObj != NULL && targetObj->adapterObj != NULL);
	// 关闭Adapter
	targetObj->adapterObj->Deinit(targetObj->adapterObj);
	// 释放TAP链表和指令队列
	list_destroy(targetObj->taps);
	list_destroy(targetObj->jtagInstrQueue);
}

/**
 * 设置仿真器通信频率（Hz）
 */
BOOL target_SetClock(TargetObject *targetObj, uint32_t clockHz){
	assert(targetObj != NULL && targetObj->adapterObj != NULL);
	if(targetObj->adapterObj->Operate(targetObj->adapterObj, AINS_SET_CLOCK, clockHz) == FALSE){
		log_warn("Failed to set adapter frequency.");
		return FALSE;
	}
	return TRUE;
}

/**
 * 切换仿真模式
 * SWD和JTAG等等
 */
BOOL target_SelectTrasnport(TargetObject *targetObj, enum transportType type){
	assert(targetObj != NULL && targetObj->adapterObj != NULL);
	if(targetObj->adapterObj->SelectTrans(targetObj->adapterObj, type) == FALSE){
		log_error("%s select %s Mode Failed.", targetObj->adapterObj->DeviceDesc, adapter_Transport2Str(type));
		return FALSE;
	}
	return TRUE;
}
/**
 * TAP复位
 * hard:TRUE：使用TRST引脚进行TAP复位；FALSE：使用连续5个脉冲宽度的TMS高电平进行TAP复位
 * pinWait：死区时间，只有在hard参数为TRUE时有效
 */
BOOL target_JTAG_TAP_Reset(TargetObject *targetObj, BOOL hard, uint32_t pinWait){
	assert(targetObj != NULL);
	// 检查状态
	if(!TRANSPORT_MATCH(targetObj, JTAG)){
		return FALSE;
	}
	if(hard){	// assert nTRST引脚等待pinWait µs，最大不超过3s
		return targetObj->adapterObj->Operate(targetObj->adapterObj, AINS_JTAG_PINS, 0x1<<5, 0x1<<5, NULL, pinWait);
	}else{	// 发送6个时钟宽度TMS高电平(其实5个就够了，多一个有保证吧，心理作用)
		uint8_t request[]= {0x46, 0x00};
		return targetObj->adapterObj->Operate(targetObj->adapterObj, AINS_JTAG_SEQUENCE, 1, request, NULL);
	}
}

/**
 * 增加JTAG扫描链中的对象
 * targetObj:Target对象
 * irLen：指令寄存器长度
 * 返回值：<0 错误，否则为扫描链中的索引
 * 注意：不需要检查当前传输是否是JTAG
 */
int target_JTAG_Add_TAP(TargetObject *targetObj, uint16_t irLen){
	assert(targetObj != NULL);
	struct JTAG_TAP *tap;
	if((tap = calloc(1, sizeof(struct JTAG_TAP))) == NULL){
		log_warn("Failed to alloc JTAG_TAP object.");
		return -1;
	}
	// 该TAP指令寄存器长度
	tap->IR_Len = irLen;
	// 初始化JTAG TAP对象
	tap->instructQueue = list_new();
	if(tap->instructQueue == NULL){
		free(tap);
		return -1;
	}
	// 初始化instr链表对象
	tap->instructQueue->free = free;	// 销毁指令对象函数
	// 新建node对象
	list_node_t *node = list_node_new(tap);
	if(node == NULL){
		log_warn("Failed to create a new node.");
		free(tap);
		list_destroy(tap->instructQueue);
		return FALSE;
	}
	list_rpush(targetObj->taps, node);
	return targetObj->taps->len - 1;	// 返回索引值
}

/**
 * 删除JTAG扫描链中的TAP对象
 * index:索引值，based 0
 * 注意：不需要检查当前传输是否是JTAG
 */
void target_JTAG_Remove_TAP(TargetObject *targetObj, int index){
	assert(targetObj != NULL);
	list_node_t *node = list_at(targetObj->taps, index);
	if(node == NULL){
		log_info("The TAP index %d not found.", index);
		return;
	}
	list_remove(targetObj->taps, node);
	// 重新计算剩下的TAP索引
}

/**
 * 获得index的tap指针
 * 返回：TAP对象指针或者NULL
 */
static struct JTAG_TAP *target_Get_TAP(TargetObject *targetObj, int index){
	assert(targetObj != NULL);
	struct JTAG_TAP *tap;
	list_node_t *tap_node = list_at(targetObj->taps, index);
	if(tap_node == NULL){
		log_info("The TAP index %d not found.", index);
		return NULL;
	}
	// 获取TAP对象
	return (struct JTAG_TAP *)tap_node->val;
}

/**
 * 向TAP中增加一个指令
 * toStatus：该指令执行时TAP要到达的状态
 * seqinfo：TDI要输出多少个数据（输出多少个时钟周期），最高位表示是否捕获TDO的数据
 * data：用于交换的数据，TDI的数据从这里读，TDO的数据写入到这里
 */
static BOOL target_JTAG_TAP_AddInstruction(struct JTAG_TAP *tap, enum JTAG_TAP_Status toStatus, uint8_t seqinfo, uint8_t *data){
	assert(tap != NULL && toStatus < JTAG_TAP_STATUS_NUM);
	// 新建指令对象
	struct JTAG_Instr *instruct = calloc(1, sizeof(struct JTAG_Instr));
	if(instruct == NULL){
		log_warn("Failed to create a new instruction object.");
		return FALSE;
	}
	// 新建指令链表对象
	list_node_t *instr_node = list_node_new(instruct);
	if(instr_node == NULL){
		log_warn("Failed to create a new instruction node.");
		free(instruct);
		return FALSE;
	}
	// 插入到该TAP的指令队列尾部
	list_lpush(tap->instructQueue, instr_node);
	// 装填指令对象
	instruct->Status = toStatus;
	instruct->TAP = tap;
	instruct->data.ctrl = seqinfo;
	instruct->data.tdiData = data;
	return TRUE;
}

/**
 * JTAG状态机切换
 * targetObj：要操作的Target对象
 * index：JTAG扫描链TAP索引
 * toStatus：TAP要转到的状态
 * 返回值：
 */
BOOL target_JTAG_TAP_ToStatus(TargetObject *targetObj, int index, enum JTAG_TAP_Status toStatus){
	assert(targetObj != NULL);
	struct JTAG_TAP *tap = target_Get_TAP(targetObj, index);
	if(tap == NULL){
		return FALSE;
	}
	return target_JTAG_TAP_AddInstruction(tap, toStatus, 0, NULL);
}

/**
 * TDI <-> TDO数据交换，在特定状态下，JTAG_TAP_DRSHIFT、JTAG_TAP_IRSHIFT
 * ctrl:第七位为1 TDO Capture，0-5 位表示长度
 */
BOOL target_JTAG_TAP_Exchange(TargetObject *targetObj, int index, enum JTAG_TAP_Status toStatus, uint8_t ctrl, uint8_t *dataEx){
	assert(targetObj != NULL && toStatus != JTAG_TAP_DRSHIFT && toStatus != JTAG_TAP_IRSHIFT);
	struct JTAG_TAP *tap = target_Get_TAP(targetObj, index);
	if(tap == NULL){
		return FALSE;
	}
	return target_JTAG_TAP_AddInstruction(tap, toStatus, ctrl, dataEx);
}

/**
 * 写入IR寄存器
 */
BOOL target_JTAG_TAP_IRWrite(TargetObject *targetObj, int index, uint8_t *dataEx){
	assert(targetObj != NULL);
	struct JTAG_TAP *tap = target_Get_TAP(targetObj, index);
	if(tap == NULL){
		return FALSE;
	}
	return target_JTAG_TAP_AddInstruction(tap, JTAG_TAP_IRSHIFT, tap->IR_Len, dataEx);
}

/**
 * 交换DR的值
 * DRLen：DR寄存器的长度
 * dataEx:TDI数据要从这里读取，TDO的数据要保存在这里
 */
BOOL target_JTAG_TAP_DRExchange(TargetObject *targetObj, int index, uint8_t DRLen, uint8_t *dataEx){
	assert(targetObj != NULL);
	struct JTAG_TAP *tap = target_Get_TAP(targetObj, index);
	if(tap == NULL){
		return FALSE;
	}
	return target_JTAG_TAP_AddInstruction(tap, JTAG_TAP_DRSHIFT, 0x80 | DRLen, dataEx);	// TDO Capture
}

/**
 * 执行JTAG的指令
 */
BOOL target_JTAG_Execute(TargetObject *targetObj){
	/**
	 * 扫描全部的TAP指令队列，选择指令，选择策略：
	 * 1、如果其他TAP没有指令，则执行该TAP的
	 * 2、如果其他TAP指令队列同时也有指令：
	 * 	1、如果指令队列头部的指令不相同，则挑选TAP当前状态切换到指令指定状态TMS需要的时钟周期最少的那个。
	 * 	2、如果指令队列头部的指令相同，则一起执行。
	 */
	assert(targetObj != NULL && targetObj->adapterObj != NULL);
	int instrStatusCount[JTAG_TAP_STATUS_NUM];	//用来统计指令个数
	// 从后往前遍历
	list_iterator_t * tapIterator = list_iterator_new(targetObj->taps, LIST_TAIL);
REPROC:
	memset(instrStatusCount, 0x0, sizeof(instrStatusCount));	// 数组清零
	// 复位迭代器
	list_iterator_reset(tapIterator);
	list_node_t *tap_node = list_iterator_next(tapIterator);
	/**
	 * 取得每个tap的第一个jtag指令，统计状态并计算时钟数
	 */
	for(; tap_node; tap_node = list_iterator_next(tapIterator)){
		struct JTAG_TAP *tap = tap_node->val;
		list_node_t * instr_node = list_at(tap->instructQueue, 0);	// 获得第一个指令
		struct JTAG_Instr *jtag_instr = instr_node->val;	// 指令对象
		instrStatusCount[jtag_instr->Status] ++;	//统计当前
		// 同时计算当前TAP状态和指令需要的状态之间的长度
		tap->NeedClockNum = JTAG_getTMSSequence(targetObj->currentStatus, jtag_instr->Status) & 0xff;	// 取得低8位长度数据
	}
	int idx = 0, maxIdx = 0;
	// 找到最多的那个
	for(; idx < JTAG_TAP_STATUS_NUM; idx ++){
		if(instrStatusCount[idx] <= instrStatusCount[maxIdx]) continue;
		else maxIdx = idx;
	}
	if(instrStatusCount[maxIdx] > 1){	// 处理最多的那个指令
		//
		goto REPROC;
	}else if(instrStatusCount[maxIdx] == 1){	// 各TAP指令队列中第一个指令各不相同，选择clocknum最少的那个执行

		goto REPROC;
	}else{	// 没有指令要执行，返回

		return TRUE;
	}
	if(instrStatusCount[idx] > 1) {	// 如果当前指令数大于1
				list_iterator_reset(tapIterator);
				tap_node = list_iterator_next(tapIterator);
				for(; tap_node; tap_node = list_iterator_next(tapIterator)){
					struct JTAG_TAP *tap = tap_node->val;
					list_node_t * instr_node = list_at(tap->JTAG_Instr, 0);	// 获得第一个指令
					struct JTAG_Instr *jtag_instr = instr_node->val;	// 指令对象

					if(jtag_instr->Status == idx){

					}
				}
				// 找到该状态的指令
				//break;
			}
	// 标记需要处理指令的TAP


	list_iterator_destroy(tapIterator);


}
