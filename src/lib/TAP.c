/*
 * TAP.c
 *
 *  Created on: 2018-3-29
 *      Author: virusv
 */

/**
 * TODO
 * 这些内存操作只在小端模式下可用，兼容大端字节序有待实现
 * 2018年04月23日：将DP数据64位长度限制去掉
 */

#include <stdlib.h>
#include <string.h>
#include "misc/log.h"
#include "misc/misc.h"
#include "lib/TAP.h"

#define TAP_INFO_IR_LEN		0
#define TAP_INFO_IR_BEFORE	1
#define TAP_INFO_IR_AFTER	2
#define ACCESS_TAP_INFO_ARRAY(pt,x,y) (*((pt)->TAP_Info + (pt)->TAP_Count*(x) + (y)))

/**
 * 在一串数据中获得第n个二进制位，n从1开始
 * lsb
 * pos:数据位存放的位置
 * n:第几位，0无效，最低位是第一位
 */
#define GET_Nth_BIT(pos,n) ((*(CAST(uint8_t *, (pos)) + (((n)-1)>>3)) >> (((n)-1) & 0x7)) & 0x1)

/**
 * 由多少个比特位构造出多少个字节，包括控制字
 * 比如bitCnt=64
 * 则返回9
 * 如果bitCnt=65
 * 则返回11。因为 发送65位需要两个控制字节了。
 */
#define BIT2BYTE(bitCnt) ({	\
	int byteCnt, tmp = (bitCnt) >> 6;	\
	byteCnt = tmp + (tmp << 3);	\
	tmp = (bitCnt) & 0x3f;	\
	byteCnt += tmp ? ((tmp + 7) >> 3) + 1 : 0;	\
})

/**
 * TAP对象构造函数
 */
BOOL __CONSTRUCT(TAP)(TAPObject *tapObj, AdapterObject *adapterObj){
	assert(tapObj != NULL && adapterObj != NULL);
	memset(tapObj, 0x0, sizeof(TAPObject));
	// 创建指令队列表头
	tapObj->jtagInstrQueue = list_new();
	if(tapObj->jtagInstrQueue == NULL){
		log_warn("Init JTAG Instruction queue failed.");
		return FALSE;
	}
	tapObj->jtagInstrQueue->free = free;
	tapObj->adapterObj = adapterObj;
	tapObj->currentStatus = JTAG_TAP_RESET;
	tapObj->TAP_actived = -1;
	tapObj->DR_Delay = 0;	// 不延迟
	// 初始化Adapter
	if(adapterObj->Init(adapterObj) == FALSE){
		log_warn("Adapter initialization failed.");
		list_destroy(tapObj->jtagInstrQueue);
		tapObj->jtagInstrQueue = NULL;
		return FALSE;
	}
	return TRUE;
}

/**
 * TAP对象析构函数
 */
void __DESTORY(TAP)(TAPObject *tapObj){
	assert(tapObj != NULL && tapObj->adapterObj != NULL);
	// 关闭Adapter
	tapObj->adapterObj->Deinit(tapObj->adapterObj);
	list_destroy(tapObj->jtagInstrQueue);
	if(tapObj->TAP_Info){
		free(tapObj->TAP_Info);
		tapObj->TAP_Info = NULL;
	}
	tapObj->TAP_Count = 0;
}

/**
 * 解析TMS信息，并写入到buff
 * seqInfo:由JTAG_getTMSSequence函数返回的TMS时序信息
 * 返回写入的字节数
 */
static int parseTMS(TAPObject *tapObj, uint8_t *buff, uint16_t seqInfo){
	assert(tapObj != NULL && buff != NULL);
	uint8_t bitCount = seqInfo & 0xff;
	uint8_t TMS_Seq = seqInfo >> 8;
	if(bitCount == 0) return 0;
	int writeCount = 1;
	*buff = (TMS_Seq & 0x1) << 6;
	// sequence 个数+1
	tapObj->JTAG_SequenceCount ++;
	while(bitCount--){
		(*buff)++;
		TMS_Seq >>= 1;
		if( bitCount && (((TMS_Seq & 0x1) << 6) ^ (*buff & 0x40))){
			*++buff = 0;
			*++buff = (TMS_Seq & 0x1) << 6;
			tapObj->JTAG_SequenceCount ++;
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
 * 此函数会将TAP改变成EXIT1-IR状态
 */
static int build_IR_InstrData(TAPObject *tapObj, uint8_t *buff, uint16_t index, uint32_t IR_Data){
	assert(tapObj != NULL && buff != NULL);
	int all_IR_Bytes, all_IR_Bits = 0;
	// 统计并计算所有TAP的IR寄存器长度以及它们所占的字节数
	for(int n=0;n<tapObj->TAP_Count;n++){
		all_IR_Bits += ACCESS_TAP_INFO_ARRAY(tapObj, TAP_INFO_IR_LEN, n);
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
	IR_Padded <<= ACCESS_TAP_INFO_ARRAY(tapObj, TAP_INFO_IR_LEN, index);
	// 将IRData写入到空间中
	// 1111 1111 1111 1011
	IR_Padded |= IR_Data;
	// 修改buff的偏移地址 15/8 = 1
	int seekLen = ACCESS_TAP_INFO_ARRAY(tapObj, TAP_INFO_IR_BEFORE, index) / 8;
	// 计算填充的长度 15%8 = 7
	int padLen = ACCESS_TAP_INFO_ARRAY(tapObj, TAP_INFO_IR_BEFORE, index) % 8;
	// 1111 1101 1000 0000
	IR_Padded <<= padLen;
	// 将最低填充位置1
	// 1111 1101 1111 1111
	IR_Padded |= (0x1 << padLen) - 1;
	// 写入数据，同时避免内存越界 XXX 大端模式不可用
	memcpy(tmp_buff + seekLen, &IR_Padded, (all_IR_Bytes - seekLen  > 4 ? 4 : all_IR_Bytes - seekLen));
	// 写入buff
	int writeCnt = 0;
	// -1 是不发送最后一位数据，因为最后一位发送的同时也同时退出SHIFT-IR状态
	// ISSUE: 如果all_IR_Bits = 65的时候呢.. 测试通过
	for(int n=all_IR_Bits - 1,readCnt=0; n>0;){
		if(n >= 64){	// 当生成的时序字节小于8个
			*buff++ = 0x0;	// TMS=0;TDO Capture=FALSE; 64Bit
			tapObj->JTAG_SequenceCount ++;
			memcpy(buff, tmp_buff+readCnt, 8);
			buff+=8; n-=64; readCnt+=8;
			writeCnt += 9;	// 数据加上一个头部
		}else{
			int bytesCnt = (n + 7) >> 3;
			*buff++ = n;
			tapObj->JTAG_SequenceCount ++;
			memcpy(buff, tmp_buff+readCnt, bytesCnt);
			buff+=bytesCnt; readCnt+=bytesCnt;
			writeCnt += bytesCnt+1;	// 数据加上一个头部
			n=0;
		}
	}
	// 发送最后一位同时退出SHIFT-IR状态
	*buff++ = 0x41;
	*buff++ = GET_Nth_BIT(tmp_buff, all_IR_Bits) & 0xff;
	writeCnt += 2;
	tapObj->JTAG_SequenceCount ++;
	free(tmp_buff);
	return writeCnt;
}

/**
 * 生成DR指令数据
 * 此函数会将TAP改变成EXIT1-DR状态
 */
static int build_DR_InstrData(TAPObject *tapObj, uint8_t *buff, struct JTAG_Instr *instr){
	assert(tapObj != NULL && buff != NULL && instr != NULL);
	int writeCnt = 0, byteCnt = 0, n;

	// 前面的tap
	for(n=instr->TAP_Index; n > 0;){
		if(n >= 64){
			*buff++ = 0x0;	// TDO Capture=FALSE, TMS=0,Count=64
			tapObj->JTAG_SequenceCount ++;
			memset(buff, 0x0, 8);
			writeCnt += 9;
			buff+=8;
			n-=64;
		}else{
			*buff++ = n;	// TDO Capture=FALSE, TMS=0,Count = instr->TAP_Index
			tapObj->JTAG_SequenceCount ++;
			byteCnt = (n + 7) >> 3;
			memset(buff, 0x0, byteCnt);
			buff += byteCnt;
			writeCnt += byteCnt + 1;
			n = 0;
		}
	}
	// 记录最后一个seqInfo的控制字节
	uint8_t *seqInfo_ptr; int DR_DataOffset=0;
	// 构造DR数据
	for(n=instr->info.DR.bitCount; n>0;){
		seqInfo_ptr = buff;
		if(n>=64){
			*buff++ = 0x80;
			tapObj->JTAG_SequenceCount ++;
			memcpy(buff, instr->info.DR.data + DR_DataOffset, 8);	// 拷贝8字节
			DR_DataOffset+=8;
			writeCnt+=9;
			buff+=8;
			n-=64;
		}else{
			*buff++ = 0x80 + n;
			tapObj->JTAG_SequenceCount ++;
			byteCnt = (n + 7) >> 3;
			memcpy(buff, instr->info.DR.data + DR_DataOffset, byteCnt);	// 拷贝8字节
			DR_DataOffset+=byteCnt;
			buff += byteCnt;
			writeCnt += byteCnt + 1;
			n = 0;
		}
	}
	//后面的tap
	for(n= tapObj->TAP_Count - instr->TAP_Index - 1; n > 0;){
		seqInfo_ptr = buff;
		if(n >= 64){
			*buff++ = 0x0;	// TDO Capture=FALSE, TMS=0,Count=64
			tapObj->JTAG_SequenceCount ++;
			memset(buff, 0x0, 8);
			writeCnt += 9;
			buff+=8;
			n-=64;
		}else{
			*buff++ = n;	// TDO Capture=FALSE, TMS=0,Count = instr->TAP_Index
			tapObj->JTAG_SequenceCount ++;
			byteCnt = (n + 7) >> 3;
			memset(buff, 0x0, byteCnt);
			buff += byteCnt;
			writeCnt += byteCnt + 1;
			n = 0;
		}
	}
	/**
	 * ISSUE:
	 * 当bitCount=9,17,25,33... 8*n+1时，将会多一个数据
	 * 例如：当等于9时，生成：
	 * 03 00 88 00 00 C1 00
	 * 应该生成：
	 * 03 00 88 00 C1 00
	 * 而且该问题只会出现在读取索引为最后一个TAP的DR时
	 *
	 * ..测试通过
	 */
	// 重新修改最后一个数据
	uint8_t cnt = (*seqInfo_ptr) & 0x3f;
	if(cnt == 1){ // 如果最后一个时序控制字是输出1个字节，那么直接修改这个控制字，将TMS置高，跳出SHIFT-DR状态
		*seqInfo_ptr |= 0x40;
	}else{	// 如果最后一个控制字输出多个二进制位，则将二进制位总数减1，并获得最后的一个二进制位，再附加一个跳出SHIFT-DR状态的控制字
		tapObj->JTAG_SequenceCount ++;
		// 当seqInfo_ptr是0x80的时候，减1就等于7f了，这就造成了错误，正确结果应该是BF
		if(cnt == 0){
			*seqInfo_ptr |= 0x3f;	// 将低6位全部置1
		}else (*seqInfo_ptr)--;	// 当cnt不等于0的时候可以长度减1
		// 先获得最后一个二进制位，因为后面可能会将最后一个字节覆盖
		uint8_t last_bit = GET_Nth_BIT(seqInfo_ptr + 1, cnt) & 0xff;
		if((cnt & 0x7) == 1){	// 当bitCount=9,17,25,33... 8*n+1时，将会多一个数据
			buff--; // 将多的那个数据覆盖掉
			writeCnt--;	// 写入数据长度减一
		}
		if(*seqInfo_ptr & 0x80){
			instr->info.DR.segment = 1;	// 表示这个指令的数据被切分
			instr->info.DR.segment_pos = cnt - 1;	//最后一位的位置，从0开始索引
			*buff++ = 0xc1;	// TDO Capture， TMS=1，TDI Count=1
			*buff++ = last_bit;
		}else{
			*buff++ = 0x41;	// TMS=1，TDI Count=1
			*buff++ = last_bit;
		}
		writeCnt += 2;
	}
	return writeCnt;
}

/**
 * 向TAP指令队列里新增一个指令
 * 返回：节点对象指针或者NULL
 */
static list_node_t * JTAG_NewInstruction(TAPObject *tapObj){
	assert(tapObj != NULL);
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
	list_rpush(tapObj->jtagInstrQueue, instr_node);
	instr_node->val = instruct;
	return instr_node;
}

/**
 * 设置DR操作后延迟的时钟数
 * DR操作后跳到IDLE状态循环delay个周期
 */
void TAP_Set_DR_Delay(TAPObject *tapObj, int delay){
	assert(tapObj != NULL);
	tapObj->DR_Delay = delay;
	// delay就是在IDLE状态空转多少个时钟周期，用来等待DR操作完成
	tapObj->delayBytes = BIT2BYTE(delay);
	log_debug("%d, %d.", tapObj->DR_Delay, tapObj->delayBytes);
}

/**
 * TAP复位
 * hard:TRUE：使用TRST引脚进行TAP复位；FALSE：使用连续5个脉冲宽度的TMS高电平进行TAP复位
 * pinWait：死区时间，只有在hard参数为TRUE时有效
 */
BOOL TAP_Reset(TAPObject *tapObj, BOOL hard, uint32_t pinWait){
	assert(tapObj != NULL);
	// 检查状态
	if(ADAPTER_CURR_TRANS(tapObj->adapterObj) != JTAG){
		log_warn("Currently not a JTAG transmission method, current is %s.", adapter_Transport2Str(tapObj->adapterObj->currTrans));
		return FALSE;
	}
	BOOL result = FALSE;
	if(hard){	// assert nTRST引脚等待pinWait µs，最大不超过3s
		result = tapObj->adapterObj->Operate(tapObj->adapterObj, AINS_JTAG_PINS, 0x1<<5, 0x1<<5, NULL, pinWait);
	}else{	// 发送5个时钟宽度TMS高电平
		uint8_t request[]= {0x45, 0x00};
		result = tapObj->adapterObj->Operate(tapObj->adapterObj, AINS_JTAG_SEQUENCE, 1, request, NULL);
	}
	if(result == TRUE){
		tapObj->currentStatus = JTAG_TAP_RESET;
	}
	return result;
}

/**
 * 设置TAP信息
 * tapCount：TAP的个数
 * 注意：离TDO最近的那个TAP是第0个。
 */
BOOL TAP_SetInfo(TAPObject *tapObj, uint16_t tapCount, uint16_t *IR_Len){
	assert(tapObj != NULL);
	int idx,bits = 0;
	// 释放
	if(tapObj->TAP_Count < tapCount){
		// 3倍空间
		uint16_t *new_ptr = realloc(tapObj->TAP_Info, tapCount * 3 * sizeof(uint16_t));
		if(new_ptr == NULL){
			log_warn("Failed to expand TAPInfo space.");
			return FALSE;
		}
		tapObj->TAP_Info = new_ptr;
	}
	tapObj->TAP_Count = tapCount;	// 更新TAP数量
	for(idx = 0; idx < tapObj->TAP_Count; idx++){
		ACCESS_TAP_INFO_ARRAY(tapObj,TAP_INFO_IR_LEN,idx) = IR_Len[idx];
		ACCESS_TAP_INFO_ARRAY(tapObj,TAP_INFO_IR_BEFORE,idx) = bits;
		bits += IR_Len[idx];
	}
	// 更新ir指令固定字节
	tapObj->IR_Bytes = BIT2BYTE(bits);	// bits长度的

	for(idx = 0; idx < tapObj->TAP_Count; idx++){
		bits -= ACCESS_TAP_INFO_ARRAY(tapObj,TAP_INFO_IR_LEN,idx);
		ACCESS_TAP_INFO_ARRAY(tapObj,TAP_INFO_IR_AFTER,idx) = bits;
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
BOOL TAP_Get_IDCODE(TAPObject *tapObj, uint32_t *idCode){
	assert(tapObj != NULL && tapObj->adapterObj != NULL);
	if(ADAPTER_CURR_TRANS(tapObj->adapterObj) != JTAG){
		log_warn("Currently not a JTAG transmission method, current is %s.", adapter_Transport2Str(tapObj->adapterObj->currTrans));
		return FALSE;
	}
	int byteCount = 2;
	uint16_t seqInfo[2];
	// 从当前状态跳到RESET状态
	seqInfo[0] = JTAG_Get_TMS_Sequence(tapObj->currentStatus, JTAG_TAP_RESET);
	byteCount += JTAG_Cal_TMS_LevelStatus(seqInfo[0] >> 8, seqInfo[0] & 0xff) << 1;
	// 跳到SHIFT-DR状态
	seqInfo[1] = JTAG_Get_TMS_Sequence(JTAG_TAP_RESET, JTAG_TAP_DRSHIFT);
	byteCount += JTAG_Cal_TMS_LevelStatus(seqInfo[1] >> 8, seqInfo[1] & 0xff) << 1;
	// 将TAP当前状态换成JTAG_TAP_DRSHIFT
	tapObj->currentStatus = JTAG_TAP_RESET;
	// 计算IDCODE的空间大小
	byteCount += tapObj->TAP_Count * 5;	//
	// 这儿，获得了从当前TAP先到复位再到SELECT-DR状态的TMS时序
	uint8_t *seqInfoBuff_tmp, *seqInfoBuff = calloc(byteCount, sizeof(uint8_t));
	if(seqInfoBuff == NULL){
		log_warn("IDCODE Timing Buffer Allocation Failed.");
		return FALSE;
	}
	seqInfoBuff_tmp = seqInfoBuff;
	tapObj->JTAG_SequenceCount = tapObj->TAP_Count;
	seqInfoBuff_tmp += parseTMS(tapObj, seqInfoBuff_tmp, seqInfo[0]);
	seqInfoBuff_tmp += parseTMS(tapObj, seqInfoBuff_tmp, seqInfo[1]);

	for(int n = 0; n < tapObj->TAP_Count; n++){
		*seqInfoBuff_tmp = 0xA0;
		seqInfoBuff_tmp += 5;
	}
	// 返回RESET状态
	*seqInfoBuff_tmp++ = 0x45;
	*seqInfoBuff_tmp++ = 0x0;
	tapObj->JTAG_SequenceCount++;
	if(tapObj->adapterObj->Operate(tapObj->adapterObj, AINS_JTAG_SEQUENCE, tapObj->JTAG_SequenceCount, seqInfoBuff, CAST(uint8_t *, idCode)) == FALSE){
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
BOOL TAP_IR_Write(TAPObject *tapObj, uint16_t index, uint32_t ir){
	assert(tapObj != NULL);
	if(index >= tapObj->TAP_Count){
		log_warn("Object index %d does not exist.", index);
		return FALSE;
	}
	list_node_t *node = JTAG_NewInstruction(tapObj);
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
BOOL TAP_DR_Exchange(TAPObject *tapObj, uint16_t index, int count, uint8_t *data){
	assert(tapObj != NULL);
	if(index >= tapObj->TAP_Count){
		log_warn("Object index %d does not exist.", index);
		return FALSE;
	}
	if(count == 0){
		log_warn("TDI data is zero length.");
		return FALSE;
	}
	list_node_t *node = JTAG_NewInstruction(tapObj);
	if(node == NULL){
		return FALSE;
	}
	struct JTAG_Instr *instruct = node->val;
	instruct->type = JTAG_INS_EXCHANGE_DR;
	instruct->TAP_Index = index;
	instruct->info.DR.bitCount = count;
	instruct->info.DR.data = data;
	// 计算指令数据编码之后的数据长度
	instruct->info.DR.length = BIT2BYTE(count);	// 数据位长度转换成字节数，包括控制字节
	// 计算跳过前后bypass TAP的空间
	instruct->info.DR.length += BIT2BYTE(index);	// 该TAP前面的设备数
	instruct->info.DR.length += BIT2BYTE(tapObj->TAP_Count - index - 1);	// 该TAP后面的设备数
	// 一个跳出SHIFT-DR的控制字
	instruct->info.DR.length += 2;

	return TRUE;
}

/**
 * 执行队列指令
 * JTAG_INS_WRITE_IR指令执行完，状态机停留在UPDATE-IR
 * JTAG_INS_EXCHANGE_DR指令执行完，状态机停留在UPDATE-DR
 */
BOOL TAP_Execute(TAPObject *tapObj){
	assert(tapObj != NULL);
	BOOL result = FALSE;	// 函数结果
	// 当前指令队列上面所有指令经过解析之后需要的空间大小
	uint8_t *instrBuffer, *resultBuffer;
	int instrBufferLen = 0, resultBufferLen = 0;
	// 当前是否是JTAG模式
	if(ADAPTER_CURR_TRANS(tapObj->adapterObj) != JTAG){
		log_warn("Currently not a JTAG transmission method, current is %s.", adapter_Transport2Str(tapObj->adapterObj->currTrans));
		return FALSE;
	}
	// 判断如果指令列表为空则返回
	if(tapObj->jtagInstrQueue->len == 0){
		return TRUE;
	}
	// 创建迭代器
	list_iterator_t *iterator = list_iterator_new(tapObj->jtagInstrQueue, LIST_HEAD);
	if(iterator == NULL){
		log_warn("Failed to create iterator.");
		goto EXIT_STEP_0;
	}
	list_node_t *node = list_iterator_next(iterator);
	// 当前TAP的状态
	enum JTAG_TAP_Status lastStatus = tapObj->currentStatus;
	for(; node; node = list_iterator_next(iterator)){
		struct JTAG_Instr *instr = CAST(struct JTAG_Instr *, node->val);
		uint16_t seqInfo;
		/**
		 * 两种指令都需要从Select-xR 到 Shift-xR
		 * 再到 Update-xR，所以这两个变换需要4字节
		 */
		instrBufferLen += 2;
		if(instr->type == JTAG_INS_EXCHANGE_DR){
			seqInfo = JTAG_Get_TMS_Sequence(lastStatus, JTAG_TAP_DRSELECT);
			instrBufferLen += JTAG_Cal_TMS_LevelStatus(seqInfo >> 8, seqInfo & 0xFF) << 1;
			resultBufferLen += ((instr->info.DR.bitCount + 7) >> 3) + 1;	// 计算 结果缓冲区的大小,+1 可能的数据分片
			instrBufferLen += instr->info.DR.length;
			// 如果在DR后有延迟，则转到IDLE状态延时额外时钟周期
			if(tapObj->DR_Delay > 0){
				// 获得从DR_EXIT1状态转换到IDLE状态的时序
				seqInfo = JTAG_Get_TMS_Sequence(JTAG_TAP_DREXIT1, JTAG_TAP_IDLE);
				instrBufferLen += JTAG_Cal_TMS_LevelStatus(seqInfo >> 8, seqInfo & 0xFF) << 1;
				instrBufferLen += tapObj->delayBytes;	// 增加延时的字节
				// 更新TAP状态机为IDLE状态
				lastStatus = JTAG_TAP_IDLE;
			}else{
				// 更新当前TAP状态为EXIT1-DR
				lastStatus = JTAG_TAP_DREXIT1;
			}
		}else{	// 写入IR寄存器，不管写入哪个，长度都是固定的
			seqInfo = JTAG_Get_TMS_Sequence(lastStatus, JTAG_TAP_IRSELECT);
			instrBufferLen += JTAG_Cal_TMS_LevelStatus(seqInfo >> 8, seqInfo & 0xFF) << 1;
			// 更新当前TAP状态为UPDATE-IR
			lastStatus = JTAG_TAP_IREXIT1;
			instrBufferLen += tapObj->IR_Bytes + 2;	// 跳出控制字
		}
	}
	// 开辟缓冲区空间
	log_trace("Instr buffer size: %d; Result buffer size: %d.", instrBufferLen, resultBufferLen);
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
	tapObj->JTAG_SequenceCount = 0;
	// 开始下一轮迭代之前要复位迭代器
	list_iterator_reset(iterator);
	// 解析指令
	node = list_iterator_next(iterator);
	for(; node; node = list_iterator_next(iterator)){
		struct JTAG_Instr *instr = CAST(struct JTAG_Instr *, node->val);
		uint16_t seqInfo;
		tapObj->currProcessing = node;	// 设置当前处理的node
		if(instr->type == JTAG_INS_WRITE_IR){
			// 写入状态切换时序信息
			seqInfo = JTAG_Get_TMS_Sequence(tapObj->currentStatus, JTAG_TAP_IRSELECT);
			currInstr += parseTMS(tapObj, currInstr, seqInfo);
			/* 现在TAP状态为SELECT-IR，转换到SHIFT-IR，TMS => 00
			 * 所以需要填入 0x02 0x00
			 */
			*currInstr++ = 0x02;
			*currInstr++ = 0x00;
			tapObj->JTAG_SequenceCount ++;
			/* 现在TAP状态是SHIFT-IR，TDI输出数据，TMS保持为0
			 * 不捕获TDO。
			 * 还得跳过该TAP前后的TAP，让这些TAP进入PASSBY状态
			 */
			int res = build_IR_InstrData(tapObj, currInstr, instr->TAP_Index, instr->info.IR_Data);
			if(res == -1){
				log_warn("build_IR_Data:Failed!");
				goto EXIT_STEP_3;
			}
			currInstr += res;
			// 修改当前活动的TAP的状态
			tapObj->TAP_actived = instr->TAP_Index;
			tapObj->currentStatus = JTAG_TAP_IREXIT1;
		}else{	// JTAG_INS_EXCHANGE_DR
			if(tapObj->TAP_actived == -1 || tapObj->TAP_actived != instr->TAP_Index){
				log_info("Missing active TAP or currently activated TAP does not match the instruction.");
				goto EXIT_STEP_3;
			}
			seqInfo = JTAG_Get_TMS_Sequence(tapObj->currentStatus, JTAG_TAP_DRSELECT);
			currInstr += parseTMS(tapObj, currInstr, seqInfo);
			/**
			 * 现在TAP状态是SELECT-DR，转换到SHIFT-DR
			 */
			*currInstr++ = 0x02;
			*currInstr++ = 0x00;
			tapObj->JTAG_SequenceCount ++;

			// TAP状态SHIFT-DR
			currInstr += build_DR_InstrData(tapObj, currInstr, instr);
			if(tapObj->DR_Delay > 0){
				// 增加额外的时钟周期去等待一些操作完成
				seqInfo = JTAG_Get_TMS_Sequence(JTAG_TAP_DREXIT1, JTAG_TAP_IDLE);
				currInstr += parseTMS(tapObj, currInstr, seqInfo);
				for(int n = tapObj->DR_Delay; n > 0;){
					if(n >= 64){
						*currInstr++ = 0x0;	// TDO Capture=FALSE, TMS=0,Count=64
						tapObj->JTAG_SequenceCount ++;
						memset(currInstr, 0x0, 8);
						currInstr += 8;
						n -= 64;
					}else{
						*currInstr++ = n;	// TDO Capture=FALSE, TMS=0,Count = n
						tapObj->JTAG_SequenceCount ++;
						int byteCnt = (n + 7) >> 3;
						memset(currInstr, 0x0, byteCnt);
						currInstr += byteCnt;
						n = 0;
					}
				}
				// 修改当前状态为IDLE状态
				tapObj->currentStatus = JTAG_TAP_IDLE;
			}else{
				// 修改当前状态位EXIT1-DR状态
				tapObj->currentStatus = JTAG_TAP_DREXIT1;
			}
		}
	}
	// 执行
	if(tapObj->adapterObj->Operate(tapObj->adapterObj, AINS_JTAG_SEQUENCE, tapObj->JTAG_SequenceCount, instrBuffer, resultBuffer) == FALSE){
		log_warn("Unable to execute instruction sequence.");
		goto EXIT_STEP_3;
	}

	list_iterator_reset(iterator);
	uint8_t *resultBuff_tmp = resultBuffer;
	// 对返回数据进行装填
	node = list_iterator_next(iterator);
	for(; node; node = list_iterator_next(iterator)){
		struct JTAG_Instr *instr = CAST(struct JTAG_Instr *, node->val);
		// 跳过IR类型的指令
		if(instr->type == JTAG_INS_WRITE_IR) continue;
		int byteCnt = (instr->info.DR.bitCount + 7) >> 3;
		memcpy(instr->info.DR.data, resultBuff_tmp, byteCnt);
		// 如果bitCount不等于8*n+1时就要处理分片
		if((instr->info.DR.bitCount & 0x7) != 1){
			// 如果分片，大端字节序下面将不可用
			if(instr->info.DR.segment){
				// MSB
				*(instr->info.DR.data + byteCnt - 1) |= *(resultBuff_tmp + byteCnt) << (instr->info.DR.segment_pos & 0x7);
				resultBuff_tmp++;	// 因为分片了，所以多出一个数据位
			}
		}
		resultBuff_tmp += byteCnt;
	}
	// 销毁执行完毕的指令
	list_iterator_reset(iterator);
	node = list_iterator_next(iterator);
	for(; node; node = list_iterator_next(iterator)){
		list_remove(tapObj->jtagInstrQueue, node);
	}
	tapObj->currProcessing = NULL;
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
