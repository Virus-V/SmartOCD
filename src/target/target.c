/*
 * target.c
 *
 *  Created on: 2018-3-29
 *      Author: virusv
 */

/**
 * TODO
 * 这些内存操作只在小端模式下可用，兼容大端字节序有待实现
 */

#include <stdlib.h>
#include <string.h>
#include "target/target.h"
#include "misc/log.h"
#include "misc/misc.h"

#define TAP_INFO_IR_LEN		0
#define TAP_INFO_IR_BEFORE	1
#define TAP_INFO_IR_AFTER	2
#define ACCESS_TAP_INFO_ARRAY(pt,x,y) (*((pt)->TAP_Info + (pt)->TAP_Count*(x) + (y)))
// 检测Transport是否匹配
#define TRANSPORT_MATCH(tp,type) ((tp)->adapterObj->currTrans == (type))
#define CEIL_FACTOR(x,n) (((x) + (n)-1) / (n))
/**
 * 获得n个连续bit位最后一位，
 * lsb，
 * pos:数据位存放的位置
 * n:bit总数。n=32，也就是第31位是最后一位，
 * 将返回第31位
 */
#define GET_N_BIT_LAST_ONE(pos,n) ((*(CAST(uint8_t *, (pos)) + (((n)-1)>>3)) >> (((n)-1) & 0x7)) & 0x1)

BOOL target_JTAG_TAP_Reset(TargetObject *targetObj, BOOL hard, uint32_t pinWait);

/**
 * Target对象构造函数
 */
BOOL __CONSTRUCT(Target)(TargetObject *targetObj, AdapterObject *adapterObj){
	assert(targetObj != NULL && adapterObj != NULL);
	// 创建指令队列表头
	targetObj->jtagInstrQueue = list_new();
	if(targetObj->jtagInstrQueue == NULL){
		log_warn("Init JTAG Instruction queue failed.");
		return FALSE;
	}
	targetObj->jtagInstrQueue->free = free;
	targetObj->adapterObj = adapterObj;
	targetObj->currentStatus = JTAG_TAP_RESET;
	targetObj->TAP_actived = -1;
	// 初始化Adapter
	if(adapterObj->Init(adapterObj) == FALSE){
		log_warn("Adapter initialization failed.");
		list_destroy(targetObj->jtagInstrQueue);
		targetObj->jtagInstrQueue = NULL;
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
	list_destroy(targetObj->jtagInstrQueue);
	if(targetObj->TAP_Info){
		free(targetObj->TAP_Info);
		targetObj->TAP_Info = NULL;
	}
	targetObj->TAP_Count = 0;
}

/**
 * 解析TMS信息，并写入到buff
 * seqInfo:由JTAG_getTMSSequence函数返回的TMS时序信息
 * 返回写入的字节数
 */
static int parseTMS(TargetObject *targetObj, uint8_t *buff, uint16_t seqInfo){
	assert(targetObj != NULL && buff != NULL);
	uint8_t bitCount = seqInfo & 0xff;
	uint8_t TMS_Seq = seqInfo >> 8;
	if(bitCount == 0) return 0;
	int writeCount = 1;
	*buff = (TMS_Seq & 0x1) << 6;
	// sequence 个数+1
	targetObj->JTAG_SequenceCount ++;
	while(bitCount--){
		// *buff |= (TMS_Seq & 0x1) << 6;
		(*buff)++;
		TMS_Seq >>= 1;
		if( bitCount && (((TMS_Seq & 0x1) << 6) ^ (*buff & 0x40))){
			*++buff = 0;
			//*++buff = 0;	// Sequence
			*++buff = (TMS_Seq & 0x1) << 6;
			targetObj->JTAG_SequenceCount ++;
			writeCount += 2;
		}
	}
	*++buff = 0;
	return writeCount + 1;
}

/**
 * 构建IR指令数据
 * buff:要写入的数据
 * index：TAP的位置
 * IR_Data:要写入的IR数据
 * 返回：写入buff的长度，失败-1
 */
// TODO 此函数需要测试
static int build_IR_InstrData(TargetObject *targetObj, uint8_t *buff, uint16_t index, uint32_t IR_Data){
	assert(targetObj != NULL && buff != NULL);
	int all_IR_Bytes, all_IR_Bits = 0;
	// 统计并计算所有TAP的IR寄存器长度以及它们所占的字节数
	for(int n=0;n<targetObj->TAP_Count;n++){
		all_IR_Bits += ACCESS_TAP_INFO_ARRAY(targetObj, TAP_INFO_IR_LEN, n);
	}
	all_IR_Bytes = (all_IR_Bits + 7) >> 3;	//计算所有TAP的IR数据所占的字节数
	uint8_t *tmp_buff = malloc(all_IR_Bytes);
	if(tmp_buff == NULL){
		log_warn("build_IR_Data:Temporary buffer allocation failed.");
		return -1;
	}
	memset(tmp_buff, 0xFF, all_IR_Bytes);
	// 计算填充后的数据
	uint32_t IR_Padded = 0xFFFFFFFFu;
	// 在最低位为IR_Data腾出空间，比如IRData=1011，IRLen=4,IRBefore = 15
	// 1111 1111 1111 xxxx
	IR_Padded <<= ACCESS_TAP_INFO_ARRAY(targetObj, TAP_INFO_IR_LEN, index);
	// 将IRData写入到空间中
	// 1111 1111 1111 1011
	IR_Padded |= IR_Data;
	// 修改buff的偏移地址 15/8 = 1
	int seekLen = ACCESS_TAP_INFO_ARRAY(targetObj, TAP_INFO_IR_BEFORE, index) / 8;
	// 计算填充的长度 15%8 = 7
	int padLen = ACCESS_TAP_INFO_ARRAY(targetObj, TAP_INFO_IR_BEFORE, index) % 8;
	// 1111 1101 1000 0000
	IR_Padded <<= padLen;
	// 将最低填充位置1
	// 1111 1101 1111 1111
	IR_Padded |= (0x1 << padLen) - 1;
	// XXX 如果出问题，这儿可能性最大
	memcpy(tmp_buff + seekLen, &IR_Padded, (all_IR_Bytes - seekLen  > 4 ? 4 : all_IR_Bytes - seekLen));
	// 写入buff
	int writeCnt = 0;
	// -1 是不发送最后一位数据，因为最后一位发送的同时也同时退出SHIFT-IR状态
	for(int n=all_IR_Bits - 1,readCnt=0; n>0;){
		if(n >= 64){	// 当生成的时序字节小于8个
			*buff++ = 0x0;	// TMS=0;TDO Capture=FALSE; 64Bit
			targetObj->JTAG_SequenceCount ++;
			memcpy(buff, tmp_buff+readCnt, 8);
			buff+=8; n-=64; readCnt+=8;
			writeCnt += 9;	// 数据加上一个头部
		}else{
			int bytesCnt = (n + 7) >> 3;
			*buff++ = n;
			targetObj->JTAG_SequenceCount ++;
			memcpy(buff, tmp_buff+readCnt, bytesCnt);
			buff+=bytesCnt; readCnt+=bytesCnt;
			writeCnt += bytesCnt+1;	// 数据加上一个头部
			n=0;
		}
	}
	// 发送最后一位同时退出SHIFT-IR状态
	*buff++ = 0x41;
	*buff++ = GET_N_BIT_LAST_ONE(tmp_buff, all_IR_Bits) & 0xff;
	writeCnt += 2;
	targetObj->JTAG_SequenceCount ++;
	// 退出SHIFT-IR状态
	targetObj->currentStatus = JTAG_TAP_IREXIT1;
	free(tmp_buff);
	return writeCnt;
}

/**
 * 生成DR指令数据
 */
static int build_DR_InstrData(TargetObject *targetObj, uint8_t *buff, struct JTAG_Instr *instr){
	assert(targetObj != NULL && buff != NULL && instr != NULL);
	int writeCnt = 0, byteCnt = 0, n;

	// 前面的tap
	for(n=instr->TAP_Index; n > 0;){
		if(n >= 64){
			*buff++ = 0x0;	// TDO Capture=FALSE, TMS=0,Count=64
			targetObj->JTAG_SequenceCount ++;
			memset(buff, 0x0, 8);
			writeCnt += 9;
			buff+=8;
			n-=64;
		}else{
			*buff++ = n;	// TDO Capture=FALSE, TMS=0,Count = instr->TAP_Index
			targetObj->JTAG_SequenceCount ++;
			byteCnt = (n + 7) >> 3;
			memset(buff, 0x0, byteCnt);
			buff += byteCnt;
			writeCnt += byteCnt + 1;
			n = 0;
		}
	}
	// 记录最后一个seqInfo的控制字节
	uint8_t *seqInfo_ptr = buff;
	// info.DR.bitCount 不会大于64
	*buff++ = instr->info.DR.bitCount == 64 ? 0x80 : (0x80 + instr->info.DR.bitCount);
	targetObj->JTAG_SequenceCount ++;
	byteCnt = (instr->info.DR.bitCount + 7) >> 3;
	for(n=0; n < byteCnt; n++){	// 复制数据
		*buff++ = *(instr->info.DR.data + n);
	}
	writeCnt += byteCnt + 1;
	//后面的tap
	for(n= targetObj->TAP_Count - instr->TAP_Index - 1; n > 0;){
		if(n >= 64){
			seqInfo_ptr = buff;
			*buff++ = 0x0;	// TDO Capture=FALSE, TMS=0,Count=64
			targetObj->JTAG_SequenceCount ++;
			memset(buff, 0x0, 8);
			writeCnt += 9;
			buff+=8;
			n-=64;
		}else{
			seqInfo_ptr = buff;
			*buff++ = n;	// TDO Capture=FALSE, TMS=0,Count = instr->TAP_Index
			targetObj->JTAG_SequenceCount ++;
			byteCnt = (n + 7) >> 3;
			memset(buff, 0x0, byteCnt);
			buff += byteCnt;
			writeCnt += byteCnt + 1;
			n = 0;
		}
	}
	// 重新修改最后一个数据
	uint8_t cnt = (*seqInfo_ptr) & 0x3f;
	if(cnt == 1){ // 如果最后一个时序控制字是输出1个字节，那么直接修改这个控制字，将TMS置高，跳出SHIFT-DR状态
		*seqInfo_ptr |= 0x40;
	}else{	// 如果最后一个控制字输出多个二进制位，则将二进制位总数减1，并获得最后的一个二进制位，再附加一个跳出SHIFT-DR状态的控制字
		(*seqInfo_ptr)--;	// 长度减1
		targetObj->JTAG_SequenceCount ++;
		if(*seqInfo_ptr & 0x80){
			instr->info.DR.segment = 1;	// 表示这个指令的数据被切分
			instr->info.DR.segment_pos = cnt - 1;	//最后一位的位置
			*buff++ = 0xc1;	// TDO Capture， TMS=1，TDI Count=1
			*buff++ = GET_N_BIT_LAST_ONE(seqInfo_ptr + 1, cnt) & 0xff;
		}else{
			*buff++ = 0x41;	// TMS=1，TDI Count=1
			*buff++ = GET_N_BIT_LAST_ONE(seqInfo_ptr + 1, cnt) & 0xff;
		}
		writeCnt += 2;
	}
	return writeCnt;
}

/**
 * 向TAP指令队列里新增一个指令
 * 返回：节点对象指针或者NULL
 */
static list_node_t * JTAG_NewInstruction(TargetObject *targetObj){
	assert(targetObj != NULL);
	// 新建指令对象
	struct JTAG_Instr *instruct = calloc(1, sizeof(struct JTAG_Instr));
	if(instruct == NULL){
		log_warn("Failed to create a new instruction object.");
		return NULL;
	}
	// 新建指令链表元素
	list_node_t *instr_node = list_node_new(instruct);
	if(instr_node == NULL){
		log_warn("Failed to create a new instruction node.");
		free(instruct);
		return NULL;
	}
	// 插入到指令队列尾部
	list_rpush(targetObj->jtagInstrQueue, instr_node);
	instr_node->val = instruct;
	return instr_node;
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
	BOOL result = FALSE;
	if(hard){	// assert nTRST引脚等待pinWait µs，最大不超过3s
		result = targetObj->adapterObj->Operate(targetObj->adapterObj, AINS_JTAG_PINS, 0x1<<5, 0x1<<5, NULL, pinWait);
	}else{	// 发送5个时钟宽度TMS高电平
		uint8_t request[]= {0x45, 0x00};
		result = targetObj->adapterObj->Operate(targetObj->adapterObj, AINS_JTAG_SEQUENCE, 1, request, NULL);
	}
	if(result == TRUE){
		targetObj->currentStatus = JTAG_TAP_RESET;
	}
	return result;
}

/**
 * 设置TAP信息
 * tapCount：TAP的个数
 * 注意：离TDO最近的那个TAP是第0个。
 */
BOOL target_JTAG_Set_TAP_Info(TargetObject *targetObj, uint16_t tapCount, uint16_t *IR_Len){
	assert(targetObj != NULL);
	int idx,bits = 0;
	// 释放
	if(targetObj->TAP_Count < tapCount){
		// 3倍空间
		uint16_t *new_ptr = realloc(targetObj->TAP_Info, tapCount * 3 * sizeof(uint16_t));
		if(new_ptr == NULL){
			log_warn("Failed to expand TAPInfo space.");
			return FALSE;
		}
		targetObj->TAP_Info = new_ptr;
	}
	targetObj->TAP_Count = tapCount;	// 更新TAP数量
	for(idx = 0; idx < targetObj->TAP_Count; idx++){
		ACCESS_TAP_INFO_ARRAY(targetObj,TAP_INFO_IR_LEN,idx) = IR_Len[idx];
		ACCESS_TAP_INFO_ARRAY(targetObj,TAP_INFO_IR_BEFORE,idx) = bits;
		bits += IR_Len[idx];
	}
	for(idx = 0; idx < targetObj->TAP_Count; idx++){
		bits -= ACCESS_TAP_INFO_ARRAY(targetObj,TAP_INFO_IR_LEN,idx);
		ACCESS_TAP_INFO_ARRAY(targetObj,TAP_INFO_IR_AFTER,idx) = bits;
	}
	return TRUE;
}

/**
 * 获得IDCODE
 * 据小道消息，当TAP处于Test-Logic-Reset的时候，IDCODE扫描链就自动连接到TDI和TDO之间。
 * Have a try~
 *
 * Get the IDs of the devices in the JTAG chain
 * Most JTAG ICs support the IDCODE instruction.
 * In IDCODE mode, the DR register is loaded with a 32-bits value that represents the device ID.
 * Unlike for the BYPASS instruction, the IR value for the IDCODE is not standard.
 * Fortunately, each time the TAP controller goes into Test-Logic-Reset, it goes into IDCODE mode (and loads the IDCODE into DR).
 *
 * emmm...然并卵。
 * 调试了一上午。emmm，原来是芯片出问题了。这个方法是可行的。
 */
BOOL target_JTAG_Get_IDCODE(TargetObject *targetObj, uint32_t *idCode){
	assert(targetObj != NULL && targetObj->adapterObj != NULL);
	int byteCount;
	uint16_t seqInfo[2];
	// 从当前状态跳到RESET状态
	seqInfo[0] = JTAG_Get_TMS_Sequence(targetObj->currentStatus, JTAG_TAP_RESET);
	byteCount  = JTAG_Cal_TMS_LevelStatus(seqInfo[0] >> 8, seqInfo[0] & 0xff) << 1;
	// 跳到SHIFT-DR状态
	seqInfo[1] = JTAG_Get_TMS_Sequence(JTAG_TAP_RESET, JTAG_TAP_DRSHIFT);
	byteCount += JTAG_Cal_TMS_LevelStatus(seqInfo[1] >> 8, seqInfo[1] & 0xff) << 1;
	// 将Target当前状态换成JTAG_TAP_DRSHIFT
	targetObj->currentStatus = JTAG_TAP_DRSHIFT;
	// 计算IDCODE的空间大小
	byteCount += targetObj->TAP_Count * 5;	//
	// 这儿，获得了从当前TAP先到复位再到SELECT-DR状态的TMS时序
	uint8_t *seqInfoBuff_tmp, *seqInfoBuff = calloc(byteCount, sizeof(uint8_t));
	if(seqInfoBuff == NULL){
		log_warn("IDCODE Timing Buffer Allocation Failed.");
		return FALSE;
	}
	seqInfoBuff_tmp = seqInfoBuff;
	targetObj->JTAG_SequenceCount = targetObj->TAP_Count;
	seqInfoBuff_tmp += parseTMS(targetObj, seqInfoBuff_tmp, seqInfo[0]);
	seqInfoBuff_tmp += parseTMS(targetObj, seqInfoBuff_tmp, seqInfo[1]);

	for(int n = 0; n < targetObj->TAP_Count; n++){
		*seqInfoBuff_tmp = 0xA0;
		seqInfoBuff_tmp += 5;
	}
	if(targetObj->adapterObj->Operate(targetObj->adapterObj, AINS_JTAG_SEQUENCE, targetObj->JTAG_SequenceCount, seqInfoBuff, CAST(uint8_t *, idCode)) == FALSE){
		log_warn("Unable to execute get IDCODE instruction sequence.");
		free(seqInfoBuff);
		return FALSE;
	}
	free(seqInfoBuff);
	return TRUE;
}

/**
 * 写入IR寄存器
 */
BOOL target_JTAG_IR_Write(TargetObject *targetObj, uint16_t index, uint32_t ir){
	assert(targetObj != NULL);
	list_node_t *node = JTAG_NewInstruction(targetObj);
	if(node == NULL){
		return FALSE;
	}
	struct JTAG_Instr *instruct = node->val;
	instruct->type = JTAG_INS_WRITE_IR;
	instruct->TAP_Index = index;
	instruct->info.IR_Data = ir;
	return TRUE;
}

/**
 * 交换DR寄存器的数据
 */
BOOL target_JTAG_DR_Exchange(TargetObject *targetObj, uint16_t index, uint8_t count, uint8_t *data){
	assert(targetObj != NULL);
	if(count > 64){
		log_warn("TDI data is too long.");
		return FALSE;
	}
	if(count == 0){
		log_warn("TDI data is zero length.");
		return FALSE;
	}
	list_node_t *node = JTAG_NewInstruction(targetObj);
	if(node == NULL){
		return FALSE;
	}
	struct JTAG_Instr *instruct = node->val;
	instruct->type = JTAG_INS_EXCHANGE_DR;
	instruct->TAP_Index = index;
	instruct->info.DR.bitCount = count;
	instruct->info.DR.data = data;
	return TRUE;
}

/**
 * 执行队列指令
 * JTAG_INS_WRITE_IR指令执行完，状态机停留在UPDATE-IR
 * JTAG_INS_EXCHANGE_DR指令执行完，状态机停留在UPDATE-DR
 */
BOOL target_JTAG_Execute(TargetObject *targetObj){
	assert(targetObj != NULL);
	BOOL result = FALSE;	// 函数结果
	// 当前指令队列上面所有指令经过解析之后需要的空间大小
	uint8_t *instrBuffer, *resultBuffer;
	int instrBufferLen = 0, resultBufferLen = 0;
	// 创建迭代器
	list_iterator_t *iterator = list_iterator_new(targetObj->jtagInstrQueue, LIST_HEAD);
	if(iterator == NULL){
		log_warn("Failed to create iterator.");
		goto EXIT_STEP_0;
	}
	list_node_t *node = list_iterator_next(iterator);
	// 计算所有TAP的IR寄存器位数总和
	int All_IR_Bits = 0, IR_InstrBytes = 0;
	for(int n=0; n<targetObj->TAP_Count;n++){
		All_IR_Bits += ACCESS_TAP_INFO_ARRAY(targetObj, TAP_INFO_IR_LEN, n);
	}
	// 计算一共有多少个8字节
	int IR_LengthByte = All_IR_Bits >> 6;
	// 算上头部和所有8个字节的数据
	IR_InstrBytes += IR_LengthByte + (IR_LengthByte << 3);
	// 取得剩余多少个位
	IR_LengthByte = All_IR_Bits & 0x3f;
	// 最终每一个IR指令的长度
	IR_InstrBytes += ((IR_LengthByte + 7) >> 3) + 1;
	// 当前target的TAP状态
	enum JTAG_TAP_Status lastStatus = targetObj->currentStatus;

	/**
	 * FIXME  看！
	 * go to Shift IR
	 * go to Shift IR - 0
	 * go to Shift IR - 1
	 * go to Shift IR - 0
	 * go to Exit IR  - 0
	 * go to Update IR
	 */

	for(; node; node = list_iterator_next(iterator)){
		struct JTAG_Instr *instr = CAST(struct JTAG_Instr *, node->val);
		uint16_t seqInfo;
		/**
		 * 两种指令都需要从Select-xR 到 Shift-xR
		 * 再到 Update-xR，所以这两个变换需要4字节
		 */
		instrBufferLen += 4;
		if(instr->type == JTAG_INS_EXCHANGE_DR){
			seqInfo = JTAG_Get_TMS_Sequence(lastStatus, JTAG_TAP_DRSELECT);
			instrBufferLen += JTAG_Cal_TMS_LevelStatus(seqInfo >> 8, seqInfo & 0xFF) << 1;
			// 更新当前TAP状态为UPDATE-DR
			lastStatus = JTAG_TAP_DRUPDATE;
			// 这里不会超过64个数据位，所以只会存在一个数据头，将数据位总数圆整到字节
			int dataByte = (instr->info.DR.bitCount + 7) >> 3;
			instrBufferLen += dataByte + 1;	// 一个头部加数据
			resultBufferLen += dataByte + 1;	// 计算 结果缓冲区的大小,+1 可能的数据分片

			// 计算跳过前后bypass TAP的空间
			int TAP_BypassBefore = instr->TAP_Index >> 6;
			// 算上头部和所有8个字节的数据
			instrBufferLen += TAP_BypassBefore + (TAP_BypassBefore << 3);
			// 取得剩余多少个位
			TAP_BypassBefore = instr->TAP_Index & 0x3f;
			instrBufferLen += TAP_BypassBefore ? ((TAP_BypassBefore + 7) >> 3) + 1 : 0;

			int TAP_BypassAfter = (targetObj->TAP_Count - instr->TAP_Index - 1) >> 6;
			// 算上头部和所有8个字节的数据
			instrBufferLen += TAP_BypassAfter + (TAP_BypassAfter << 3);
			// 取得剩余多少个位
			TAP_BypassAfter = (targetObj->TAP_Count - instr->TAP_Index - 1) & 0x3f;
			instrBufferLen += TAP_BypassAfter ? ((TAP_BypassAfter + 7) >> 3) + 1 : 0;
			// 一个跳出SHIFT-DR的控制字
			instrBufferLen += 2;
		}else{	// 写入IR寄存器，不管写入哪个，长度都是固定的
			seqInfo = JTAG_Get_TMS_Sequence(lastStatus, JTAG_TAP_IRSELECT);
			instrBufferLen += JTAG_Cal_TMS_LevelStatus(seqInfo >> 8, seqInfo & 0xFF) << 1;
			// 更新当前TAP状态为UPDATE-IR
			lastStatus = JTAG_TAP_IRUPDATE;
			instrBufferLen += IR_InstrBytes + 2;	// 跳出控制字
		}
	}
	// 开辟缓冲区空间
	log_debug("Instruction buffer size: %d; Result buffer size: %d.", instrBufferLen, resultBufferLen);
	//goto EXIT_STEP_1;
	instrBuffer = malloc(instrBufferLen);
	if(instrBuffer == NULL){
		log_warn("Unable to create instruction buffer.");
		goto EXIT_STEP_1;
	}
	resultBuffer = malloc(resultBufferLen);
	if(resultBuffer == NULL){
		log_warn("Unable to create result buffer.");
		goto EXIT_STEP_2;
	}

	uint8_t *currInstr = instrBuffer;
	// 清空Sequence Counter
	targetObj->JTAG_SequenceCount = 0;
	// 开始下一轮迭代之前要复位迭代器
	list_iterator_reset(iterator);
	// 解析指令
	node = list_iterator_next(iterator);
	log_debug("curr status :%s.", JTAG_state2str(targetObj->currentStatus));
	for(; node; node = list_iterator_next(iterator)){
		struct JTAG_Instr *instr = CAST(struct JTAG_Instr *, node->val);
		uint16_t seqInfo;
		targetObj->currProcessing = node;	// 设置当前处理的node
		if(instr->type == JTAG_INS_WRITE_IR){
			// 写入状态切换时序信息
			seqInfo = JTAG_Get_TMS_Sequence(targetObj->currentStatus, JTAG_TAP_IRSELECT);
			currInstr += parseTMS(targetObj, currInstr, seqInfo);
			/* 现在TAP状态为SELECT-IR，转换到SHIFT-IR，TMS => 00
			 * 所以需要填入 0x02 0x00
			 */
			*currInstr++ = 0x02;
			*currInstr++ = 0x00;
			targetObj->JTAG_SequenceCount ++;
			/* 现在TAP状态是SHIFT-IR，TDI输出数据，TMS保持为0
			 * 不捕获TDO。
			 * 还得跳过该TAP前后的TAP，让这些TAP进入PASSBY状态
			 */
			int res = build_IR_InstrData(targetObj, currInstr, instr->TAP_Index, instr->info.IR_Data);
			if(res == -1){
				log_warn("build_IR_Data:Failed!");
				goto EXIT_STEP_3;
			}
			currInstr += res;
			/*
			 * 跳到UPDATE-IR状态，TMS => 11
			 */
			*currInstr++ = 0x41;
			*currInstr++ = 0x00;
			targetObj->JTAG_SequenceCount ++;
			// 修改当前活动的TAP的状态
			targetObj->TAP_actived = instr->TAP_Index;
			targetObj->currentStatus = JTAG_TAP_IRUPDATE;
		}else{	// JTAG_INS_EXCHANGE_DR
			if(targetObj->TAP_actived == -1 || targetObj->TAP_actived != instr->TAP_Index){
				log_info("Missing active TAP or currently activated TAP does not match the instruction.");
				goto EXIT_STEP_3;
			}
			seqInfo = JTAG_Get_TMS_Sequence(targetObj->currentStatus, JTAG_TAP_DRSELECT);
			currInstr += parseTMS(targetObj, currInstr, seqInfo);
			/**
			 * 现在TAP状态是SELECT-DR，转换到SHIFT-DR
			 */
			*currInstr++ = 0x02;
			*currInstr++ = 0x00;
			targetObj->JTAG_SequenceCount ++;
			// TAP状态SHIFT-DR
			currInstr += build_DR_InstrData(targetObj, currInstr, instr);
			/**
			 * 跳到UPDATE-DR
			 */
			*currInstr++ = 0x41;
			*currInstr++ = 0x00;
			targetObj->JTAG_SequenceCount ++;
			// 修改当前状态
			targetObj->currentStatus = JTAG_TAP_DRUPDATE;
		}
	}
	// 执行
	if(targetObj->adapterObj->Operate(targetObj->adapterObj, AINS_JTAG_SEQUENCE, targetObj->JTAG_SequenceCount, instrBuffer, resultBuffer) == FALSE){
		log_warn("Unable to execute instruction sequence.");
		goto EXIT_STEP_3;
	}
	// 对返回数据进行装填
	list_iterator_reset(iterator);
	uint8_t *resultBuff_tmp = resultBuffer;
	// 解析指令
	node = list_iterator_next(iterator);
	for(; node; node = list_iterator_next(iterator)){
		struct JTAG_Instr *instr = CAST(struct JTAG_Instr *, node->val);
		// 跳过IR类型的指令
		if(instr->type == JTAG_INS_WRITE_IR) continue;
		// 指向当前node
		targetObj->currProcessing = node;
		// TODO 处理分片，待测试
		int byteCnt = (instr->info.DR.bitCount + 7) >> 3;	// 转换成字节
		memcpy(instr->info.DR.data, resultBuff_tmp, byteCnt);
		// 如果分片，小端模式下
		if(instr->info.DR.segment){
			*(instr->info.DR.data + byteCnt - 1) |= *(resultBuff_tmp + byteCnt) << instr->info.DR.segment_pos;
			resultBuff_tmp++;	// 因为分片了，所以多出一个数据位
		}
		resultBuff_tmp += byteCnt;
	}
	// 销毁执行完毕的指令
	list_iterator_reset(iterator);
	node = list_iterator_next(iterator);
	// 可以安全释放
	for(; node; node = list_iterator_next(iterator)){
		list_remove(targetObj->jtagInstrQueue, node);
	}

	result = TRUE;
EXIT_STEP_3:
	free(resultBuffer);
EXIT_STEP_2:
	free(instrBuffer);
EXIT_STEP_1:
	list_iterator_destroy(iterator);
EXIT_STEP_0:
	return result;
}
