/*
 * adapter.c
 *
 *  Created on: 2018-2-18
 *      Author: virusv
 */

#ifdef HAVE_CONFIG
#include "global_config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "misc/log.h"
#include "debugger/adapter.h"

#define TAP_INFO_IR_LEN		0
#define TAP_INFO_IR_BEFORE	1
#define TAP_INFO_IR_AFTER	2
#define ACCESS_TAP_INFO_ARRAY(pa,x,y) (*((pa)->tap.TAP_Info + (pa)->tap.TAP_Count*(x) + (y)))

static BOOL init(AdapterObject *adapterObj);	// 初始化
static BOOL deinit(AdapterObject *adapterObj);	// 反初始化（告诉我怎么形容合适）
static BOOL selectTrans(AdapterObject *adapterObj, enum transportType type);	// 选择transport类型(当仿真器支持多个transport时)
static BOOL operate(AdapterObject *adapterObj, int operate, ...);	// 执行动作
static void destroy(AdapterObject *adapterObj);	// 销毁Adapter对象

// 初始化Adapter对象
BOOL __CONSTRUCT(Adapter)(AdapterObject *adapterObj, const char *desc){
	assert(adapterObj != NULL);
	memset(adapterObj, 0x0, sizeof(AdapterObject));
	adapterObj->DeviceDesc = strdup(desc);
	// 初始化默认函数
	adapterObj->isInit = FALSE;
	adapterObj->Init = init;
	adapterObj->Deinit = deinit;
	adapterObj->SelectTrans = selectTrans;
	adapterObj->Operate = operate;
	adapterObj->Destroy = destroy;
	// 初始化TAP指令链表头
	INIT_LIST_HEAD(&adapterObj->tap.instrQueue);
	// 初始化DAP指令链表头
	INIT_LIST_HEAD(&adapterObj->dap.instrQueue);
	adapterObj->dap.ir = JTAG_IDCODE;	// 默认连接到IDCODE扫描链
	adapterObj->dap.SELECT_Reg.regData = DP_SELECT_INVALID;	// 初始化非法值
	return TRUE;
}

void __DESTORY(Adapter)(AdapterObject *adapterObj){
	assert(adapterObj != NULL);
	if(adapterObj->DeviceDesc != NULL){
		free(adapterObj->DeviceDesc);
		adapterObj->DeviceDesc = NULL;
	}
	if(adapterObj->tap.TAP_Info != NULL){
		free(adapterObj->tap.TAP_Info);
		adapterObj->tap.TAP_Info = NULL;
	}
}

// 默认的初始化函数
static BOOL init(AdapterObject *adapterObj){
	(void)adapterObj;
	log_warn("Tried to call undefined function: init.");
	return FALSE;
}

static BOOL deinit(AdapterObject *adapterObj){
	(void)adapterObj;
	log_warn("Tried to call undefined function: deinit.");
	return FALSE;
}

static BOOL selectTrans(AdapterObject *adapterObj, enum transportType type){
	(void)adapterObj;
	(void)type;
	log_warn("Tried to call undefined function: selectTrans.");
	return FALSE;
}

/**
 * 注意可变参数的数据类型提升
 * float类型的实际参数将提升到double
 * char、short和相应的signed、unsigned类型的实际参数提升到int
 * 如果int不能存储原值，则提升到unsigned int
 */
static BOOL operate(AdapterObject *adapterObj, int operate, ...){
	(void)adapterObj; (void)operate;
	log_warn("Tried to call undefined function: operate.");
	return FALSE;
}

/**
 * 销毁该对象
 */
static void destroy(AdapterObject *adapterObj){
	(void)adapterObj;
	log_warn("Tried to call undefined function: destroy.");
}
/**
 * 设置仿真器通信频率（Hz）
 */
BOOL adapter_SetClock(AdapterObject *adapterObj, uint32_t clockHz){
	assert(adapterObj != NULL);
	if(adapterObj->Operate(adapterObj, AINS_SET_CLOCK, clockHz) == FALSE){
		log_warn("Failed to set adapter frequency.");
		return FALSE;
	}
	return TRUE;
}

/**
 * 切换通信模式
 * SWD和JTAG等等
 */
BOOL adapter_SelectTransmission(AdapterObject *adapterObj, enum transportType type){
	assert(adapterObj != NULL);
	if(type == UNKNOW_NULL) return FALSE;
	if(adapterObj->currTrans == type) return TRUE;
	// 如果不支持该协议
	if(adapter_HaveTransmission(adapterObj, type) == FALSE) return FALSE;

	if(adapterObj->SelectTrans(adapterObj, type) == FALSE){
		log_warn("%s select %s Mode Failed.", adapterObj->DeviceDesc, adapter_Transport2Str(type));
		return FALSE;
	}
	adapterObj->currTrans = type;
	return TRUE;
}

/**
 * 判断当前adapter是否支持某一个传输方式
 */
BOOL adapter_HaveTransmission(AdapterObject *adapterObj, enum transportType type){
	assert(adapterObj != NULL && adapterObj->supportedTrans != NULL);
	if(type == UNKNOW_NULL) return FALSE;
	for(int idx=0; adapterObj->supportedTrans[idx] != UNKNOW_NULL; idx++){
		if(adapterObj->supportedTrans[idx] == type) return TRUE;
	}
	return FALSE;
}

/**
 * 设置Adapter状态灯
 */
BOOL adapter_SetStatus(AdapterObject *adapterObj, enum adapterStatus status){
	assert(adapterObj != NULL);
	return adapterObj->Operate(adapterObj, AINS_SET_STATUS, status);
}

// 创建新的JTAG指令对象，并将其插入JTAG指令队列尾部
static struct JTAG_Command *new_JTAG_Command(struct list_head *list){
	assert(list != NULL);
	// 新建指令对象
	struct JTAG_Command *command = calloc(1, sizeof(struct JTAG_Command));
	if(command == NULL){
		log_warn("Failed to create a new JTAG Command object.");
		return NULL;
	}
	// 将指令插入链表尾部
	list_add_tail(&command->list_entry, list);
	return command;
}

// 向链表中插入状态机转换的指令
static BOOL add_JTAG_StatusChange(struct list_head *list, enum JTAG_TAP_Status fromStatus, enum JTAG_TAP_Status newStatus){
	assert(list != NULL);
	struct JTAG_Command *command = new_JTAG_Command(list);
	if(command == NULL){
		return FALSE;
	}
	command->type = JTAG_INS_STATUS_MOVE;	// 状态机改变指令
	// 计算时序信息
	command->instr.statusMove.seqInfo = JTAG_Get_TMS_Sequence(fromStatus, newStatus);	// 高8位为时序信息
	return TRUE;
}

/**
 * JTAG状态机改变
 * newStatus:需要跳转到的下一个JTAG TAP状态
 * free_cb:该指令对象释放时，调用的清理回调
 */
BOOL adapter_JTAG_StatusChange(AdapterObject *adapterObj, enum JTAG_TAP_Status newStatus){
	assert(adapterObj != NULL);
	// 判断该仿真器是否支持JTAG
	if(adapter_HaveTransmission(adapterObj, JTAG) == FALSE){
		log_warn("This adapter doesn't support JTAG transmission.");
		return FALSE;
	}
	// 如果当前状态等于新状态，则直接返回
	if(adapterObj->tap.currentStatus == newStatus) return TRUE;
	if(add_JTAG_StatusChange(&adapterObj->tap.instrQueue, adapterObj->tap.currentStatus, newStatus) == FALSE){
		return FALSE;
	}
	adapterObj->tap.currentStatus = newStatus;	// 改变当前状态
	return TRUE;
}

/**
 * TAP状态机复位
 * hard：是否使用硬复位，就是使用tRST引脚进行复位
 * 	如果该参数为true，则接下来的srst和pinWait参数用来指定是否执行
 * 	系统复位（sRST引脚），pinWait参考adapter_JTAG_RW_Pins函数的注释
 */
BOOL adapter_Reset(AdapterObject *adapterObj, BOOL hard, BOOL srst, int pinWait){
	assert(adapterObj != NULL);
	if(adapterObj->Operate(adapterObj, AINS_RESET, hard, srst, pinWait) == TRUE){
		adapterObj->tap.currentStatus = JTAG_TAP_RESET;	// 改变当前状态为复位状态，SWD没有状态机
		return TRUE;
	}else{
		return FALSE;
	}
}

// 向链表中插入Exchange IO的指令
static BOOL add_JTAG_Exchange_IO(struct list_head *list, uint8_t *dataPtr, int bitCount){
	assert(list != NULL);
	// 新建指令
	struct JTAG_Command *command = new_JTAG_Command(list);
	if(command == NULL){
		return FALSE;
	}
	command->type = JTAG_INS_EXCHANGE_IO;
	command->instr.exchangeIO.bitNum = bitCount;
	command->instr.exchangeIO.data = dataPtr;
	return TRUE;
}

/**
 * 交换TDI和TDO的数据，在传输完成后会自动将状态机从SHIFT-xR跳转到EXTI1-xR
 * bitCount：需要传输的位个数
 * dataPtr:传输的数据
 * free_cb:该指令对象释放时，调用的清理回调
 */
BOOL adapter_JTAG_Exchange_IO(AdapterObject *adapterObj, uint8_t *dataPtr, int bitCount){
	assert(adapterObj != NULL && (adapterObj->tap.currentStatus == JTAG_TAP_DRSHIFT || adapterObj->tap.currentStatus == JTAG_TAP_IRSHIFT));
	// 判断该仿真器是否支持JTAG
	if(adapter_HaveTransmission(adapterObj, JTAG) == FALSE){
		log_warn("This adapter doesn't support JTAG transmission.");
		return FALSE;
	}
	if(add_JTAG_Exchange_IO(&adapterObj->tap.instrQueue, dataPtr, bitCount) == FALSE){
		return FALSE;
	}
	// 修改当前状态机状态
	adapterObj->tap.currentStatus = adapterObj->tap.currentStatus == JTAG_TAP_DRSHIFT ? JTAG_TAP_DREXIT1 : JTAG_TAP_IREXIT1;
	return TRUE;
}

static BOOL add_JTAG_IdleWait(struct list_head *list, int clkCnt){
	assert(list != NULL);
	struct JTAG_Command *command = new_JTAG_Command(list);
	if(command == NULL){
		return FALSE;
	}
	command->type = JTAG_INS_IDLE_WAIT;
	command->instr.idleWait.wait = clkCnt;
	return TRUE;
}

/**
 * TAP进入IDLE状态空转clkCnt个时钟周期
 */
BOOL adapter_JTAG_IdleWait(AdapterObject *adapterObj, int clkCnt){
	assert(adapterObj != NULL);
	LIST_HEAD(list_tmp);
	// 判断该仿真器是否支持JTAG
	if(adapter_HaveTransmission(adapterObj, JTAG) == FALSE){
		log_warn("This adapter doesn't support JTAG transmission.");
		return FALSE;
	}
	// 插入状态改变指令
	if(add_JTAG_StatusChange(&list_tmp, adapterObj->tap.currentStatus, JTAG_TAP_IDLE) == FALSE){
		goto ERR_EXIT;
	}
	// 插入等待指令
	if(add_JTAG_IdleWait(&list_tmp, clkCnt) == FALSE){
		goto ERR_EXIT;
	}
	// 将临时指令队列的指令并入指令队列
	list_splice_tail(&list_tmp, &adapterObj->tap.instrQueue);
	// 改变当前TAP的状态
	adapterObj->tap.currentStatus = JTAG_TAP_IDLE;
	return TRUE;
ERR_EXIT:;
	// 循环遍历临时链表，释放指令
	struct JTAG_Command *cmd, *cmd_t;
	list_for_each_entry_safe(cmd, cmd_t, &list_tmp, list_entry){
		free(cmd);
	}
	return FALSE;
}

/**
 * 读写JTAG引脚电平
 */
BOOL adapter_JTAG_RW_Pins(AdapterObject *adapterObj, uint8_t pinSelect, uint8_t *pinData, int pinWait){
	assert(adapterObj != NULL);
	// 判断该仿真器是否支持JTAG
	if(adapter_HaveTransmission(adapterObj, JTAG) == FALSE){
		log_warn("This adapter doesn't support JTAG transmission.");
		return FALSE;
	}
	return adapterObj->Operate(adapterObj, AINS_JTAG_PINS, pinSelect, pinData, pinWait);
}

/**
 * 执行JTAG队列
 */
BOOL adapter_JTAG_Execute(AdapterObject *adapterObj){
	assert(adapterObj != NULL);
	// 判断当前传输协议是否是JTAG
	if(adapterObj->currTrans != JTAG){
		log_warn("Actived transmission is not JTAG.");
		return FALSE;
	}
	// 如果指令队列为空，则直接成功返回
	if(list_empty(&adapterObj->tap.instrQueue)) return TRUE;
	// 解析并执行JTAG队列指令
	return adapterObj->Operate(adapterObj, AINS_JTAG_CMD_EXECUTE);
}

/**
 * 清空JTAG指令队列
 */
void adapter_JTAG_CleanCommandQueue(AdapterObject *adapterObj){
	assert(adapterObj != NULL);
	struct JTAG_Command *cmd, *cmd_t;
	list_for_each_entry_safe(cmd, cmd_t, &adapterObj->tap.instrQueue, list_entry){
		list_del(&cmd->list_entry);	// 将链表中删除
		free(cmd);
	}
}

/**
 * 设置TAP的信息
 * 扫描链的TAP数量，每个TAP的IR长度
 */
BOOL adapter_JTAG_Set_TAP_Info(AdapterObject *adapterObj, uint16_t tapCount, uint16_t *IR_Len){
	assert(adapterObj != NULL);
	int idx,bits = 0;
	// 释放
	if(adapterObj->tap.TAP_Count < tapCount){
		// 3倍空间
		uint16_t *new_ptr = realloc(adapterObj->tap.TAP_Info, tapCount * 3 * sizeof(uint16_t));
		if(new_ptr == NULL){
			log_warn("Failed to expand TAPInfo space.");
			return FALSE;
		}
		adapterObj->tap.TAP_Info = new_ptr;
	}
	adapterObj->tap.TAP_Count = tapCount;	// 更新TAP数量
	for(idx = 0; idx < tapCount; idx++){
		ACCESS_TAP_INFO_ARRAY(adapterObj, TAP_INFO_IR_LEN,idx) = IR_Len[idx];
		ACCESS_TAP_INFO_ARRAY(adapterObj, TAP_INFO_IR_BEFORE,idx) = bits;
		bits += IR_Len[idx];
	}
	// 更新IR寄存器链一共多少位
	adapterObj->tap.IR_Bits = bits;

	for(idx = 0; idx < tapCount; idx++){
		bits -= ACCESS_TAP_INFO_ARRAY(adapterObj, TAP_INFO_IR_LEN,idx);
		ACCESS_TAP_INFO_ARRAY(adapterObj, TAP_INFO_IR_AFTER,idx) = bits;
	}
	return TRUE;
}

/**
 * 组合数据
 * 将dest[startPos, startPos+len)的数据替换成data指向的数据，长度是len参数指定
 */
static void bitReplace(uint8_t *dest, int startPos, int len, uint8_t *data){
	assert(dest != NULL && data != NULL && startPos >= 0 && len > 0);
	for(int i=0;i<len;i++){
		// BUG:不可以在宏定义里面使用++自增
		SET_Nth_BIT(dest, startPos+i, GET_Nth_BIT(data, i));
	}
}

/**
 * 数据提取
 * 将source[startPos, startPos+len)的数据写入到data中，长度是len参数指定
 */
static void bitExtract(uint8_t *source, int startPos, int len, uint8_t *data){
	assert(source != NULL && data != NULL && startPos >= 0 && len > 0);
	for(int i=0;i<len;i++){
		// BUG:不可以在宏定义里面使用++自增
		SET_Nth_BIT(data, i, GET_Nth_BIT(source, startPos+i));
	}
}

/**
 * 写入指定TAP的IR寄存器
 * tapIndex：TAP的索引
 * IR_Data：IR数据
 * 该函数会执行JTAG指令队列！
 */
BOOL adapter_JTAG_Wirte_TAP_IR(AdapterObject *adapterObj, uint16_t tapIndex, uint32_t IR_Data){
	assert(adapterObj != NULL && tapIndex < adapterObj->tap.TAP_Count);

	// 计算所有TAP的IR寄存器长度 所占的字节数
	int all_IR_Bytes = (adapterObj->tap.IR_Bits + 7) >> 3;

	uint8_t *tmp_buff = malloc(all_IR_Bytes * sizeof(uint8_t));
	if(tmp_buff == NULL){
		log_warn("adapter_JTAG_Wirte_TAP_IR:Temporary buffer allocation failed.");
		return FALSE;
	}
	// 全部置1
	memset(tmp_buff, 0xFF, all_IR_Bytes);
	// 获得必要的数据来计算位置
	int IRBefore = ACCESS_TAP_INFO_ARRAY(adapterObj, TAP_INFO_IR_BEFORE, tapIndex);
	int IRLen = ACCESS_TAP_INFO_ARRAY(adapterObj, TAP_INFO_IR_LEN, tapIndex);
	// 构造IR数据
	bitReplace(tmp_buff, IRBefore, IRLen, CAST(uint8_t *, &IR_Data));
	//misc_PrintBulk(tmp_buff, all_IR_Bytes, 8);
	// 新建临时指令队列
	LIST_HEAD(list_tmp);
	// 插入状态改变指令
	if(add_JTAG_StatusChange(&list_tmp, adapterObj->tap.currentStatus, JTAG_TAP_IRSHIFT) == FALSE){
		free(tmp_buff);
		goto ERR_EXIT;
	}

	// 交换IO
	if(add_JTAG_Exchange_IO(&list_tmp, tmp_buff, adapterObj->tap.IR_Bits) == FALSE){
		free(tmp_buff);
		goto ERR_EXIT;
	}
	// 并入指令队列
	list_splice_tail(&list_tmp, &adapterObj->tap.instrQueue);
	// 改变当前TAP的状态
	adapterObj->tap.currentStatus = JTAG_TAP_IRSHIFT;
	// 执行队列
	if(adapter_JTAG_Execute(adapterObj) == FALSE){
		free(tmp_buff);
		return FALSE;
	}
	free(tmp_buff);
	// 队列执行成功
	return TRUE;
ERR_EXIT:;
	// 循环遍历临时链表，释放指令
	struct JTAG_Command *cmd, *cmd_t;
	list_for_each_entry_safe(cmd, cmd_t, &list_tmp, list_entry){
		free(cmd);
	}
	return FALSE;
}

/**
 * 交换指定TAP的DR数据
 * tapIndex：TAP索引
 * DR_Data：DR数据
 * DR_BitCnt：DR的二进制位个数
 * 该函数会执行JTAG指令队列！
 */
BOOL adapter_JTAG_Exchange_TAP_DR(AdapterObject *adapterObj, uint16_t tapIndex, uint8_t *DR_Data, int DR_BitCnt){
	assert(adapterObj != NULL && tapIndex < adapterObj->tap.TAP_Count);
	int all_DR_Bits = adapterObj->tap.TAP_Count - 1 + DR_BitCnt;
	// 占用的字节数
	int all_DR_Bytes = (all_DR_Bits + 7) >> 3;
	uint8_t *tmp_buff = malloc(all_DR_Bytes * sizeof(uint8_t));
	if(tmp_buff == NULL){
		log_warn("adapter_JTAG_Exchange_TAP_DR:Temporary buffer allocation failed.");
		return FALSE;
	}
	// 全部置1
	memset(tmp_buff, 0xFF, all_DR_Bytes);
	// 构造DR数据
	bitReplace(tmp_buff, tapIndex, DR_BitCnt, DR_Data);
	//misc_PrintBulk(tmp_buff, all_DR_Bytes, 8);
	// 新建临时指令队列
	LIST_HEAD(list_tmp);
	// 插入状态改变指令
	if(add_JTAG_StatusChange(&list_tmp, adapterObj->tap.currentStatus, JTAG_TAP_DRSHIFT) == FALSE){
		free(tmp_buff);
		goto ERR_EXIT;
	}

	// 交换IO
	if(add_JTAG_Exchange_IO(&list_tmp, tmp_buff, all_DR_Bits) == FALSE){
		free(tmp_buff);
		goto ERR_EXIT;
	}
	// 并入指令队列
	list_splice_tail(&list_tmp, &adapterObj->tap.instrQueue);

	// 改变当前TAP的状态
	adapterObj->tap.currentStatus = JTAG_TAP_DRSHIFT;
	// 执行队列
	if(adapter_JTAG_Execute(adapterObj) == FALSE){
		free(tmp_buff);
		return FALSE;
	}
	// 数据提取
	bitExtract(tmp_buff, tapIndex, DR_BitCnt, DR_Data);

	free(tmp_buff);
	// 队列执行成功
	return TRUE;
ERR_EXIT:
	// 循环遍历临时链表，释放指令
	do{
		struct JTAG_Command *cmd, *cmd_t;
		list_for_each_entry_safe(cmd, cmd_t, &list_tmp, list_entry){
			free(cmd);
		}
	}while(0);
	return FALSE;
}

// 创建新的DAP指令对象，并将其插入DAP指令队列尾部
static struct DAP_Command *new_DAP_Command(struct list_head *list){
	assert(list != NULL);
	// 新建指令对象
	struct DAP_Command *command = calloc(1, sizeof(struct DAP_Command));
	if(command == NULL){
		log_warn("Failed to create a new DAP Command object.");
		return NULL;
	}
	// 将指令插入链表尾部
	list_add_tail(&command->list_entry, list);
	return command;
}

// 设置指令类型和数据，单个寄存器
static BOOL add_DAP_RW_RegSingle(
	struct list_head *list, int reg, BOOL read, BOOL ap, 
	uint32_t *dataR, uint32_t dataW)
{
	assert(list != NULL);
	assert((read == TRUE && dataR != NULL) || read == FALSE);
	// 创建指令并插入队列
	struct DAP_Command *command = new_DAP_Command(list);
	if(command == NULL){
		return FALSE;
	}
	command->type = DAP_INS_RW_REG_SINGLE;	// 读写单个寄存器
	// DP和AP在同一个内存位置
	command->instr.RWRegSingle.reg = reg;
	command->instr.RWRegSingle.RnW = read;
	command->instr.RWRegSingle.APnDP = ap;
	if(read == TRUE){
		command->instr.RWRegSingle.data.read = dataR;
	}else{
		command->instr.RWRegSingle.data.write = dataW;
	}
	return TRUE;
}

// 对单个寄存器进行多次读写
static BOOL add_DAP_RW_RegBlock(
	struct list_head *list, int reg, BOOL read, BOOL ap, 
	uint32_t *dataIO, int count)
{
	assert(list != NULL);
	assert((read == TRUE && dataIO != NULL) || read == FALSE);
	// 创建指令并插入队列
	struct DAP_Command *command = new_DAP_Command(list);
	if(command == NULL){
		return FALSE;
	}
	command->type = DAP_INS_RW_REG_BLOCK;	// 对寄存器进行批量读写
	// DP和AP在同一个内存位置
	command->instr.RWRegBlock.reg = reg;
	command->instr.RWRegBlock.RnW = read;
	command->instr.RWRegBlock.APnDP = ap;
	command->instr.RWRegBlock.blockCnt = count;	// 读写寄存器次数
	if(read == TRUE){
		command->instr.RWRegBlock.data.read = dataIO;
	}else{
		command->instr.RWRegBlock.data.write = dataIO;
	}
	return TRUE;
}

/**
 * Single 方式读写nP寄存器
 * updateSelect:是否自动更新SELECT寄存器
 */
BOOL adapter_DAP_RW_nP_Single(AdapterObject *adapterObj, int reg, BOOL read, BOOL ap, uint32_t data_in, uint32_t *data_out, BOOL updateSelect){
	assert(adapterObj != NULL);
	if(updateSelect == TRUE){
		uint32_t select_bak = adapterObj->dap.SELECT_Reg.regData;
		LIST_HEAD(list_tmp);
		if(adapterObj->dap.SELECT_Reg.regInfo.AP_BankSel != (reg >> 4)){
			// 更新本地AP BankSel部分
			adapterObj->dap.SELECT_Reg.regInfo.AP_BankSel = reg >> 4;
			// 写SELECT寄存器
			if(add_DAP_RW_RegSingle(&list_tmp, SELECT, FALSE, FALSE, NULL, adapterObj->dap.SELECT_Reg.regData) == FALSE){
				log_warn("Update SELECT Register Failed!");
				adapterObj->dap.SELECT_Reg.regData = select_bak;
				return FALSE;
			}
		}
		// 写 AP reg
		if(add_DAP_RW_RegSingle(&list_tmp, reg, read, ap, data_out, data_in) == FALSE){
			adapterObj->dap.SELECT_Reg.regData = select_bak;
			// 释放队列指令数据
			struct DAP_Command *cmd, *cmd_t;
			list_for_each_entry_safe(cmd, cmd_t, &list_tmp, list_entry){
				list_del(&cmd->list_entry);	// 将链表中删除
				free(cmd);
			}
			return FALSE;
		}
		// 并入DAP指令队列
		list_splice_tail(&list_tmp, &adapterObj->dap.instrQueue);
	}else{
		if(add_DAP_RW_RegSingle(&adapterObj->dap.instrQueue, reg, read, ap, data_out, data_in) == FALSE){
			return FALSE;
		}
	}
	return TRUE;
}

/**
 * Block方式读写nP寄存器
 * updateSelect:是否自动更新SELECT寄存器
 */
BOOL adapter_DAP_RW_nP_Block(AdapterObject *adapterObj, int reg, BOOL read, BOOL ap, uint32_t *dataIO, int blockCnt, BOOL updateSelect){
	assert(adapterObj != NULL);
	if(updateSelect == TRUE){
		uint32_t select_bak = adapterObj->dap.SELECT_Reg.regData;
		LIST_HEAD(list_tmp);
		if(adapterObj->dap.SELECT_Reg.regInfo.AP_BankSel != (reg >> 4)){
			// 更新本地AP BankSel部分
			adapterObj->dap.SELECT_Reg.regInfo.AP_BankSel = reg >> 4;
			// 写SELECT寄存器
			if(add_DAP_RW_RegSingle(&list_tmp, SELECT, FALSE, FALSE, NULL, adapterObj->dap.SELECT_Reg.regData) == FALSE){
				log_warn("Update SELECT Register Failed!");
				adapterObj->dap.SELECT_Reg.regData = select_bak;
				return FALSE;
			}
		}
		// 加入block 指令
		if(add_DAP_RW_RegBlock(&list_tmp, reg, read, ap, dataIO, blockCnt) == FALSE){
			adapterObj->dap.SELECT_Reg.regData = select_bak;
			// 释放队列指令数据
			struct DAP_Command *cmd, *cmd_t;
			list_for_each_entry_safe(cmd, cmd_t, &list_tmp, list_entry){
				list_del(&cmd->list_entry);	// 将链表中删除
				free(cmd);
			}
			return FALSE;
		}
		// 并入DAP指令队列
		list_splice_tail(&list_tmp, &adapterObj->dap.instrQueue);
	}else{
		// 加入block指令
		if(add_DAP_RW_RegBlock(&adapterObj->dap.instrQueue, reg, read, ap, dataIO, blockCnt) == FALSE){
			return FALSE;
		}
	}
	return TRUE;
}


/**
 * 执行DAP指令队列
 * 并同步SELECT、CTRL_STAT寄存器
 */
BOOL adapter_DAP_Execute(AdapterObject *adapterObj){
	assert(adapterObj != NULL);
	// 如果指令队列为空，则直接成功返回
	if(list_empty(&adapterObj->dap.instrQueue)) return TRUE;
	// 解析并执行JTAG队列指令
	if(adapterObj->Operate(adapterObj, AINS_DAP_CMD_EXECUTE) == FALSE){
		log_warn("Execute Commands Failed.");
		return FALSE;
	}
	// 执行成功之后读取SELECT、CTRL_STAT并更新本地缓存
	LIST_HEAD(list_tmp);
	uint32_t select = 0, ctrl_stat = 0;
	// 读SELECT寄存器
	if(add_DAP_RW_RegSingle(&list_tmp, SELECT, TRUE, FALSE, &select, 0) == FALSE){
		goto FAILED;
	}
	// 加入block 指令
	if(add_DAP_RW_RegSingle(&list_tmp, CTRL_STAT, TRUE, FALSE, &ctrl_stat, 0) == FALSE){
		goto FAILED;
	}
	// 并入DAP指令队列
	list_splice_tail(&list_tmp, &adapterObj->dap.instrQueue);
	// 执行
	if(adapterObj->Operate(adapterObj, AINS_DAP_CMD_EXECUTE) == TRUE){
		adapterObj->dap.SELECT_Reg.regData = select;
		adapterObj->dap.CTRL_STAT_Reg.regData = ctrl_stat;
		return TRUE;
	}else{
		log_warn("Refresh SELECT & CTRL/STAT Register Failed!");
		return FALSE;
	}
FAILED:;
	struct DAP_Command *cmd, *cmd_t;
	list_for_each_entry_safe(cmd, cmd_t, &list_tmp, list_entry){
		list_del(&cmd->list_entry);	// 将链表中删除
		free(cmd);
	}
	log_warn("Refresh SELECT & CTRL/STAT Register Failed!");
	return FALSE;
}

/**
 * 清空DAP指令队列
 */
void adapter_DAP_CleanCommandQueue(AdapterObject *adapterObj){
	assert(adapterObj != NULL);
	struct DAP_Command *cmd, *cmd_t;
	list_for_each_entry_safe(cmd, cmd_t, &adapterObj->dap.instrQueue, list_entry){
		list_del(&cmd->list_entry);	// 将链表中删除
		free(cmd);
	}
}

/**
 * 写DAP的ABORT寄存器，或者ABORT扫描链
 */
BOOL adapter_DAP_WriteAbortReg(AdapterObject *adapterObj, uint32_t data){
	assert(adapterObj != NULL);
	return adapterObj->Operate(adapterObj, AINS_DAP_WRITE_ABOTR, data);
}

/**
 * 获得Transport协议字符串
 */
const char *adapter_Transport2Str(enum transportType type){
#define X(_s) if (type == _s) return #_s
	X(JTAG);
	X(SWD);
#undef X
	return "UNKOWN_TRANSPORT";
}

/**
 * 获得仿真器状态字符串
 */
const char *adapter_Status2Str(enum adapterStatus type){
#define X(_s) if (type == _s) return #_s
	X(ADAPTER_STATUS_CONNECTED);
	X(ADAPTER_STATUS_DISCONNECT);
	X(ADAPTER_STATUS_RUNING);
	X(ADAPTER_STATUS_IDLE);
#undef X
	return "UNKOWN_STATUS";
}
