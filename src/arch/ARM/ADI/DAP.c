/*
 * DAP.c
 *
 *  Created on: 2018-2-25
 *      Author: virusv
 */

#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include "smart_ocd.h"
#include "misc/log.h"
#include "arch/ARM/ADI/DAP.h"

/**
 * 此宏的目的是构造nPACC扫描链的数据
 * buff是指针
 * 将ctrl[2:0]放在buff[2:0]
 * 将data[31:0]放在buff[34:3]
 * 注意，buff至少要可以容纳35bit
 */
#define MAKE_nPACC_CHAIN_DATA(buff,data,ctrl) {	\
	uint32_t data_t = data;					\
	uint8_t msb3 = ((data_t) & 0xe0) >> 5;	\
	uint8_t lsb3 = (ctrl) & 0x7;			\
	int n;									\
	for(n=0; n<4; n++){						\
		*(CAST(uint8_t *,(buff)) + n) = (((data_t) & 0xff) << 3) | lsb3;	\
		(data_t) >>= 8;						\
		lsb3 = msb3;						\
		msb3 = ((data_t) & 0xe0) >> 5;		\
	}										\
	*(CAST(uint8_t *,(buff)) + n) = lsb3;	\
}

/**
 * 此宏的目的是获得nPACC扫描链的数据部分
 * 返回buff[34:3]
 */
#define GET_nPACC_CHAIN_DATA(buff) ({	\
	uint8_t *data_t = CAST(uint8_t *, (buff)), lsb3;	\
	uint32_t tmp = 0;	\
	for(int n=0;n<4;n++){	\
		lsb3 = (data_t[n+1] & 0x7) << 5;	\
		tmp |= ((data_t[n] >> 3) | lsb3) << (n << 3);	\
	}	\
	tmp;	\
})

/**
 * DAP对象构造函数
 * TAP_Index：该DAP在JTAG扫描链中的索引位置
 */
BOOL __CONSTRUCT(DAP)(DAPObject *dapObj, AdapterObject *adapterObj){
	assert(dapObj != NULL && adapterObj != NULL);
	memset(dapObj, 0x0, sizeof(DAPObject));
	// 初始化TAP对象
	if(__CONSTRUCT(TAP)(&dapObj->tapObj, adapterObj) == FALSE){
		log_warn("TAP initialization failed.");
		return FALSE;
	}
	// 初始化队列
	INIT_LIST_HEAD(&dapObj->instrQueue);
	INIT_LIST_HEAD(&dapObj->retryQueue);
	dapObj->ir = JTAG_BYPASS;	// ir选择BYPASS
	dapObj->retry = 5;	// 重试5次
	// 初始化AP
	for(int idx = 0; idx<=255; idx++){
		//dapObj->AP[idx].apIdx = idx;
		dapObj->AP[idx].ctrl_state.init = 0;
	}
	return TRUE;
}

/**
 * DAP对象析构函数
 */
void __DESTORY(DAP)(DAPObject *dapObj){
	assert(dapObj != NULL);
	// 销毁TAP对象
	__DESTORY(TAP)(&dapObj->tapObj);
}

/**
 * 读取DAP寄存器
 * index：JTAG模式下TAP的索引，SWD模式下忽略该参数
 * APnDP：读取AP寄存器还是DP寄存器 AP:1,DP:0
 * reg：要读取的寄存器
 * data_out：将读取的数据输出
 */
static BOOL DAP_Read(DAPObject *dapObj, int APnDP, uint8_t reg, uint32_t *data_out){
	int retry = dapObj->retry;
	jmp_buf exception;
	// 先将数据清零
	*data_out = 0;
	// 错误处理
	switch(setjmp(exception)){
	case 1: log_warn("TAP_IR_Write:Failed!"); return FALSE;
	case 2: log_warn("TAP_DR_Exchange:Failed!"); return FALSE;
	case 3: log_warn("TAP_Execute:Failed!"); return FALSE;
	default: log_warn("Unknow Error."); return FALSE;
	case 0:break;
	}
	// 如果当前协议是JTAG
	if(ADAPTER_CURR_TRANS(dapObj->tapObj.adapterObj) == JTAG){
		uint8_t buff[5] = {0,0,0,0,0};	// 扫描链35比特，清零防止valgrind报指向未初始化数据的错误。
		// 选择AP还是DP
		if(APnDP == 1){
			if(dapObj->ir != JTAG_APACC){
				dapObj->ir = JTAG_APACC;
				// 选中APACC扫描链
				if(TAP_IR_Write(&dapObj->tapObj, dapObj->TAP_index, JTAG_APACC) == FALSE) longjmp(exception, 1);
			}
		}else{
			if(dapObj->ir != JTAG_DPACC){
				dapObj->ir = JTAG_DPACC;
				// 选中DPACC扫描链
				if(TAP_IR_Write(&dapObj->tapObj, dapObj->TAP_index, JTAG_DPACC) == FALSE) longjmp(exception, 1);
			}
		}
		// 写入
		do{
			// 装填数据：data[0]是RnW,1是R，0是W
			// data[2:1]是地址A[3:2]
			buff[0] = ((reg >> 1) & 0x6) | 1;
			// 写入控制信息 A[3:2] RnW
			if(TAP_DR_Exchange(&dapObj->tapObj, dapObj->TAP_index, 35, buff) == FALSE) longjmp(exception, 2);
			// 执行队列
			if(TAP_Execute(&dapObj->tapObj) == FALSE) longjmp(exception, 3);
		}while((buff[0] & 0x7) == JTAG_RESP_WAIT && retry--);
		// 判断是否成功
		if((buff[0] & 0x7) != JTAG_RESP_OK_FAULT){
			log_warn("The last JTAG ACK was WAIT. Read Failed.");
			return FALSE;
		}
		retry = dapObj->retry;
		// 读取上次的寄存器内容，重试
		do{
			buff[0] = ((DP_REG_RDBUFF >> 1) & 0x6) | 1;
			if(TAP_DR_Exchange(&dapObj->tapObj, dapObj->TAP_index, 35, buff) == FALSE) longjmp(exception, 2);
			// 执行队列
			if(TAP_Execute(&dapObj->tapObj) == FALSE) longjmp(exception, 3);
		} while((buff[0] & 0x7) == JTAG_RESP_WAIT && retry--);
		// 重试过后
		if((buff[0] & 0x7) != JTAG_RESP_OK_FAULT){
			log_warn("Read RDBUFF Failed. The last JTAG ACK was WAIT.");
			return FALSE;
		}
		// 将数据输出
		*data_out = GET_nPACC_CHAIN_DATA(buff);
		return TRUE;
	}else if(ADAPTER_CURR_TRANS(dapObj->tapObj.adapterObj) == SWD){
		// TODO SWD模式下
		return FALSE;
	}
	return FALSE;
}

/**
 * 写入DP寄存器
 * index：JTAG模式下TAP的索引，SWD模式下忽略该参数
 * APnDP：读取AP寄存器还是DP寄存器 AP:1,DP:0
 * reg：要写入的寄存器
 * data：写入的内容
 */
static BOOL DAP_Write(DAPObject *dapObj, int APnDP, uint8_t reg, uint32_t data_in){
	int retry = dapObj->retry;
	jmp_buf exception;
	// 错误处理
	switch(setjmp(exception)){
	case 1: log_warn("TAP_IR_Write:Failed!"); return FALSE;
	case 2: log_warn("TAP_DR_Exchange:Failed!"); return FALSE;
	case 3: log_warn("TAP_Execute:Failed!"); return FALSE;
	default: log_warn("Unknow Error."); return FALSE;
	case 0:break;
	}
	// 如果当前协议是JTAG
	if(ADAPTER_CURR_TRANS(dapObj->tapObj.adapterObj) == JTAG){
		uint8_t buff[5];	// 扫描链35比特
		// 选择AP还是DP
		if(APnDP == 1){
			if(dapObj->ir != JTAG_APACC){
				dapObj->ir = JTAG_APACC;
				// 选中APACC扫描链
				if(TAP_IR_Write(&dapObj->tapObj, dapObj->TAP_index, JTAG_APACC) == FALSE) longjmp(exception, 1);
			}
		}else{
			if(dapObj->ir != JTAG_DPACC){
				dapObj->ir = JTAG_DPACC;
				// 选中DPACC扫描链
				if(TAP_IR_Write(&dapObj->tapObj, dapObj->TAP_index, JTAG_DPACC) == FALSE) longjmp(exception, 1);
			}
		}
		// 装填数据：data[0]是RnW,1是R，0是W
		// data[2:1]是地址A[3:2]
		do{
			// 装填数据
			MAKE_nPACC_CHAIN_DATA(buff, data_in, (reg >> 1) & 0x6);
			// 写入数据
			if(TAP_DR_Exchange(&dapObj->tapObj, dapObj->TAP_index, 35, buff) == FALSE) longjmp(exception, 2);
			// 执行队列
			if(TAP_Execute(&dapObj->tapObj) == FALSE) longjmp(exception, 3);
		}while((buff[0] & 0x7) == JTAG_RESP_WAIT && retry--);
		// 判断当前命令是否被提交
		if((buff[0] & 0x7) != JTAG_RESP_OK_FAULT){
			log_warn("The last JTAG ACK was WAIT. The data to write was discarded.");
			return FALSE;
		}
		return TRUE;
	}else if(ADAPTER_CURR_TRANS(dapObj->tapObj.adapterObj) == SWD){
		// TODO SWD模式下
		return FALSE;
	}
	return FALSE;
}

// 设置AP和DP的寄存器Bank
static BOOL DAP_SetBank(DAPObject *dapObj, int APnDP, uint8_t reg){
	uint8_t regBank = (reg >> 4) & 0xF;
	// 当前select寄存器备份
	uint32_t selectBak = dapObj->SelectReg.data;
	BOOL result = TRUE;
	if(APnDP == 1){	// AP
		if(dapObj->SelectReg.info.AP_BankSel == regBank){
			return TRUE;
		}
		dapObj->SelectReg.info.AP_BankSel = regBank;

	}else{
		if(dapObj->SelectReg.info.DP_BankSel == regBank){
			return TRUE;
		}
		dapObj->SelectReg.info.DP_BankSel = regBank;
	}
	// 写入SELECT寄存器
	result = DAP_Write(dapObj, 0, DP_REG_SELECT, dapObj->SelectReg.data);
	// 如果执行失败，则恢复寄存器
	if(result == FALSE){
		log_warn("AP Bank Selection Failed.");
		dapObj->SelectReg.data = selectBak;
	}
	return result;
}

// 读取DP寄存器
BOOL DAP_DP_ReadReg(DAPObject *dapObj, uint8_t reg, uint32_t *data_out){
	assert(dapObj != NULL);
	// 设置BankSel
	if(DAP_SetBank(dapObj, 0, reg) == FALSE){
		return FALSE;
	}
	return DAP_Read(dapObj, 0, reg, data_out);
}

// 写入DP寄存器
BOOL DAP_DP_WriteReg(DAPObject *dapObj, uint8_t reg, uint32_t data){
	assert(dapObj != NULL);
	// 设置BankSel
	if(DAP_SetBank(dapObj, 0, reg) == FALSE){
		return FALSE;
	}
	return DAP_Write(dapObj, 0, reg, data);
}

// 读取AP寄存器
BOOL DAP_AP_ReadReg(DAPObject *dapObj, uint8_t reg, uint32_t *data_out){
	assert(dapObj != NULL);
	// 设置BankSel
	if(DAP_SetBank(dapObj, 1, reg) == FALSE){
		return FALSE;
	}
	return DAP_Read(dapObj, 1, reg, data_out);
}

// 写入AP寄存器
BOOL DAP_AP_WriteReg(DAPObject *dapObj, uint8_t reg, uint32_t data){
	assert(dapObj != NULL);
	// 设置BankSel
	if(DAP_SetBank(dapObj, 1, reg) == FALSE){
		return FALSE;
	}
	return DAP_Write(dapObj, 1, reg, data);
}

/**
 * 选择AP
 * index：JTAG模式下TAP的索引，SWD模式下忽略该参数
 * apIdx：ap的索引，0~255
 */
BOOL DAP_AP_Select(DAPObject *dapObj, uint8_t apIdx){
	assert(dapObj != NULL);
	if(dapObj->SelectReg.info.AP_Sel == apIdx && dapObj->AP[apIdx].ctrl_state.init == 1){
		return TRUE;
	}
	// 当前select寄存器备份
	uint32_t selectBak = dapObj->SelectReg.data;
	// 修改当前apSel值
	dapObj->SelectReg.info.AP_Sel = apIdx;
	// 写入SELECT寄存器
	if(DAP_Write(dapObj, 0, DP_REG_SELECT, dapObj->SelectReg.data) == FALSE){
		log_warn("AP Selection Failed.");
		dapObj->SelectReg.data = selectBak;
		return FALSE;
	}
	// 获取AP相关信息：CFG
	if(dapObj->AP[apIdx].ctrl_state.init == 0){	// 初始化ap相关参数
		uint32_t tmp;
		if(DAP_AP_ReadReg(dapObj, AP_REG_CFG, &tmp) == FALSE) return FALSE;
		dapObj->AP[apIdx].ctrl_state.largeAddress = !!(tmp & 0x2);
		dapObj->AP[apIdx].ctrl_state.largeData = !!(tmp & 0x4);
		dapObj->AP[apIdx].ctrl_state.bigEndian = !!(tmp & 0x1);
		// 写入之前先读取CSW寄存器
		if(DAP_AP_ReadReg(dapObj, AP_REG_CSW, &dapObj->AP[apIdx].CSW.regData) == FALSE) return FALSE;
		tmp = dapObj->AP[apIdx].CSW.regData;	// 备份到tmp
		// 写入packed模式和8位模式
		dapObj->AP[apIdx].CSW.regInfo.AddrInc = ADDRINC_PACKED;
		dapObj->AP[apIdx].CSW.regInfo.Size = SIZE_8;
		if(DAP_AP_WriteReg(dapObj, AP_REG_CSW, dapObj->AP[apIdx].CSW.regData) == FALSE) return FALSE;
		if(DAP_AP_ReadReg(dapObj, AP_REG_CSW, &dapObj->AP[apIdx].CSW.regData) == FALSE) return FALSE;
		/**
		 * ARM ID080813
		 * 第7-133
		 * An implementation that supports transfers smaller than word must support packed transfers.
		 * Packed transfers cannot be supported on a MEM-AP that only supports whole-word transfers.
		 * 第7-136
		 * If the MEM-AP Large Data Extention is not supported, then when a MEM-AP implementation supports different
		 * sized access, it MUST support word, halfword and byte accesses.
		 */
		if(dapObj->AP[apIdx].CSW.regInfo.AddrInc == AP_CSW_PADDRINC){
			dapObj->AP[apIdx].ctrl_state.packedTransfers = 1;
			dapObj->AP[apIdx].ctrl_state.lessWordTransfers = 1;
		}else{
			dapObj->AP[apIdx].ctrl_state.lessWordTransfers = dapObj->AP[apIdx].CSW.regInfo.Size == SIZE_8 ? 1 : 0;
		}
		// 恢复CSW寄存器的值
		if(DAP_AP_WriteReg(dapObj, AP_REG_CSW, tmp) == FALSE) return FALSE;
		// 赋值给CSW
		dapObj->AP[apIdx].CSW.regData = tmp;
		dapObj->AP[apIdx].ctrl_state.init = 1;
	}
	// 检查结果
	return TRUE;
}

/**
 * 检查是否出现错误，同时并更新CTRL寄存器
 * 返回：TRUE 没有错误标志位置位
 * 		FALSE 有错误标志位置位
 */
BOOL DAP_CheckError(DAPObject *dapObj){
	assert(dapObj != NULL);
	jmp_buf exception;
	uint32_t ctrl_status;
	switch(setjmp(exception)){
	case 1: log_error("DAP_CheckError:Write SELECT Register Failed!"); return FALSE;
	case 2: log_error("DAP_CheckError:Read CTRL/STATE Register Failed!"); return FALSE;
	case 3: log_error("DAP_CheckError:Failed!"); return FALSE;
	default: log_error("Unknow Error."); return FALSE;
	case 0:break;
	}
	// 要原生读取
	if(dapObj->SelectReg.info.DP_BankSel != 0){	// 当前DP bank不等于0
		// 写入SELECT寄存器
		if(DAP_Write(dapObj, 0, DP_REG_SELECT, 0) == FALSE) longjmp(exception, 1);
		// 读取CTRL_STATE寄存器
		if(DAP_Read(dapObj, 0, DP_REG_CTRL_STAT, &ctrl_status) == FALSE) longjmp(exception, 2);
		// 恢复SELECT寄存器
		if(DAP_Write(dapObj, 0, DP_REG_SELECT, dapObj->SelectReg.data) == FALSE) longjmp(exception, 1);
	}else{
		// 读取CTRL寄存器
		if(DAP_Read(dapObj, 0, DP_REG_CTRL_STAT, &ctrl_status) == FALSE) longjmp(exception, 2);
	}
	// 更新寄存器的内容
	dapObj->CTRL_STAT_Reg.regData = ctrl_status;
	if(ctrl_status & (DP_STAT_STICKYORUN | DP_STAT_STICKYCMP | DP_STAT_STICKYERR | DP_STAT_WDATAERR)){
		return FALSE;
	}
	return TRUE;
}

/**
 * 写入Abort寄存器
 */
BOOL DAP_WriteAbort(DAPObject *dapObj, uint32_t abort){
	assert(dapObj != NULL);
	int retry = dapObj->retry;
	jmp_buf exception;
	// 错误处理
	switch(setjmp(exception)){
	case 1: log_warn("TAP_IR_Write:Failed!"); return FALSE;
	case 2: log_warn("TAP_DR_Exchange:Failed!"); return FALSE;
	case 3: log_warn("TAP_Execute:Failed!"); return FALSE;
	default: log_warn("Unknow Error."); return FALSE;
	case 0:break;
	}
	// 如果当前协议是JTAG
	if(ADAPTER_CURR_TRANS(dapObj->tapObj.adapterObj) == JTAG){
		uint8_t buff[5];	// 扫描链35比特
		// 选择AP还是DP
		if(dapObj->ir != JTAG_ABORT){
			dapObj->ir = JTAG_ABORT;
			// 选中DPACC扫描链
			if(TAP_IR_Write(&dapObj->tapObj, dapObj->TAP_index, JTAG_ABORT) == FALSE) longjmp(exception, 1);
		}
		do{
			// 装填数据
			MAKE_nPACC_CHAIN_DATA(buff, abort, 0);
			// 写入数据
			if(TAP_DR_Exchange(&dapObj->tapObj, dapObj->TAP_index, 35, buff) == FALSE) longjmp(exception, 2);
			// 执行队列
			if(TAP_Execute(&dapObj->tapObj) == FALSE) longjmp(exception, 3);
		}while((buff[0] & 0x7) == JTAG_RESP_WAIT && retry--);
		// 判断当前命令是否被提交
		if((buff[0] & 0x7) != JTAG_RESP_OK_FAULT){
			log_warn("The last JTAG ACK was WAIT. The data to write was discarded.");
			return FALSE;
		}
		return TRUE;
	}else if(ADAPTER_CURR_TRANS(dapObj->tapObj.adapterObj) == SWD){
		// TODO SWD模式下
		return FALSE;
	}
	return FALSE;
}

/**
 * 清除粘性错误标志
 * JTAG-DP（所有实现）需要直接写1到CTRL/STATUS相应的位
 * SWD-DP（所有实现）需要写入Abort寄存器，写1清除
 * 注意：此函数操作的CTRL/STATUS寄存器的值不保证是最新的
 */
BOOL DAP_ClearStickyFlag(DAPObject *dapObj, uint32_t flags){
	assert(dapObj != NULL);
	// 如果当前是JTAG
	if(ADAPTER_CURR_TRANS(dapObj->tapObj.adapterObj) == JTAG){
		uint32_t ctrl_status;
		// 写相应位到CTRL/STAT寄存器中
		flags &= DP_STAT_STICKYORUN | DP_STAT_STICKYCMP | DP_STAT_STICKYERR | DP_STAT_WDATAERR;
		ctrl_status = ~(DP_STAT_STICKYORUN | DP_STAT_STICKYCMP | DP_STAT_STICKYERR | DP_STAT_WDATAERR);
		ctrl_status &= dapObj->CTRL_STAT_Reg.regData;	// 保留其他位的值
		ctrl_status |= flags;	// 置位要清除的标志位
		log_info("CLEAR:%X",ctrl_status);
		// 写入到CTRL/STATUS寄存器
		if(DAP_DP_WriteReg(dapObj, DP_REG_CTRL_STAT, ctrl_status) == FALSE){
			log_warn("Sticky error flag clearing failed.");
			return FALSE;
		}else{
			// 更新寄存器
			dapObj->CTRL_STAT_Reg.regData = ctrl_status;
			return TRUE;
		}
	}else if(ADAPTER_CURR_TRANS(dapObj->tapObj.adapterObj) == SWD){
		// swd
		return FALSE;
	}
	return FALSE;
}

/**
 * 查找某一类型的AP
 * apType：需要查找的AP类型
 * 返回：-1：错误 其他 ap的index
 * 同时会选中该AP
 */
BOOL DAP_Find_AP(DAPObject *dapObj, enum ap_type apType, uint8_t *apIdx_Out){
	assert(dapObj != NULL);
	*apIdx_Out = 0;
	int apIdx; AP_IDR_Parse ap_IDR;

	for(apIdx = 0; apIdx < 256; apIdx++){
		// 选择AP
		if(DAP_AP_Select(dapObj, apIdx) == FALSE){
			log_warn("Find AP Failed! Select AP %d Failed.", apIdx);
			return FALSE;
		}
		if(DAP_AP_ReadReg(dapObj, AP_REG_IDR, &ap_IDR.regData) == FALSE){
			log_warn("Find AP Failed! Read AP %d IDR Failed.", apIdx);
			return FALSE;
		}
		// 检查是否有错误
		if(DAP_CheckError(dapObj) == FALSE){
			log_warn("Find AP Failed! Sticky Flaged in AP %d.", apIdx);
			return FALSE;
		}
		// 检查厂商
		if(ap_IDR.regInfo.JEP106Code == JEP106_CODE_ARM){
			if(apType == AP_TYPE_JTAG && ap_IDR.regInfo.Class == 0){	// 选择JTAG-AP
				break;
			}else if(apType != AP_TYPE_JTAG && ap_IDR.regInfo.Class == 8){	// 选择MEM-AP
				if(ap_IDR.regInfo.Type == apType){
					break;
				}
			}
		}
	}
	*apIdx_Out = apIdx;
	return TRUE;
}

/**
 * 写入TAR寄存器
 */
BOOL DAP_Write_TAR(DAPObject *dapObj, uint64_t addr){
	assert(dapObj != NULL);
	// 如果支持Large Address
	if(dapObj->AP[DAP_CURR_AP(dapObj)].ctrl_state.init == 1 && dapObj->AP[DAP_CURR_AP(dapObj)].ctrl_state.largeAddress == 1){
		// 将地址的高32位写入TAR_MSB
		if(DAP_AP_WriteReg(dapObj, AP_REG_TAR_MSB, (uint32_t)(addr >> 32)) == FALSE) return FALSE;
	}
	// 将低32位写入TAR_LSB
	if(DAP_AP_WriteReg(dapObj, AP_REG_TAR_LSB, (uint32_t)(addr & 0xffffffffu)) == FALSE) return FALSE;
	return TRUE;
}

/**
 * 读取TAR寄存器
 */
BOOL DAP_Read_TAR(DAPObject *dapObj, uint64_t *addr_out){
	assert(dapObj != NULL);
	*addr_out = 0;
	uint64_t addr = 0; uint32_t tmp;
	// 如果支持Large Address
	if(dapObj->AP[DAP_CURR_AP(dapObj)].ctrl_state.init == 1 && dapObj->AP[DAP_CURR_AP(dapObj)].ctrl_state.largeAddress == 1){
		if(DAP_AP_ReadReg(dapObj, AP_REG_TAR_MSB, &tmp) == FALSE) return FALSE;
		addr = tmp;
		addr <<= 32;	// 移到高32位
	}
	if(DAP_AP_ReadReg(dapObj, AP_REG_TAR_LSB, &tmp) == FALSE) return FALSE;
	addr |= tmp;	// 赋值低32位
	*addr_out = addr;
	return TRUE;
}


/**
 * 读取8位数据
 * addr：64位地址
 * data_out：读取的数据输出地址
 */
BOOL DAP_ReadMem8(DAPObject *dapObj, uint64_t addr, uint8_t *data_out){
	assert(dapObj != NULL && dapObj->AP[DAP_CURR_AP(dapObj)].ctrl_state.init == 1);
	// 设置CSW：Size=Byte，AddrInc=off
	uint32_t csw_bak = dapObj->AP[DAP_CURR_AP(dapObj)].CSW.regData;
	dapObj->AP[DAP_CURR_AP(dapObj)].CSW.regInfo.AddrInc = ADDRINC_OFF;	// AddrInc Off
	if(dapObj->AP[DAP_CURR_AP(dapObj)].ctrl_state.lessWordTransfers == 1){	// 支持lessWordTransfer
		dapObj->AP[DAP_CURR_AP(dapObj)].CSW.regInfo.Size = SIZE_8;	// Byte
	}else{
		dapObj->AP[DAP_CURR_AP(dapObj)].CSW.regInfo.Size = SIZE_32;	// Word
	}
	// 是否需要更新寄存器？
	if(dapObj->AP[DAP_CURR_AP(dapObj)].CSW.regData != csw_bak)
		if(DAP_AP_WriteReg(dapObj, AP_REG_CSW, dapObj->AP[DAP_CURR_AP(dapObj)].CSW.regData) == FALSE) return FALSE;
	// 获得字节在字中的索引
	int byteIdx = addr & 0x3;
	// 读取数据
	if(dapObj->AP[DAP_CURR_AP(dapObj)].ctrl_state.lessWordTransfers == 0){
		/**
		 * 不支持lessWordTransfer
		 * 需要将地址的低2位清零，4字节读取
		 */
		addr = (addr >> 2) << 2;
	}
	// 写入TAR
	if(DAP_Write_TAR(dapObj, addr) == FALSE) return FALSE;
	// 读取数据，根据byte lane获得数据
	uint32_t data;
	if(DAP_AP_ReadReg(dapObj, AP_REG_DRW, &data) == FALSE) return FALSE;
	// 拆分数据
	*data_out = (uint8_t)(data >> (8*byteIdx));
	return TRUE;
}

/**
 * 读取16位数据
 * addr：64位地址
 * data_out：读取的数据输出地址
 * 注意：地址要以字对齐，否则返回FALSE
 */
BOOL DAP_ReadMem16(DAPObject *dapObj, uint64_t addr, uint16_t *data_out){
	assert(dapObj != NULL && dapObj->AP[DAP_CURR_AP(dapObj)].ctrl_state.init == 1);
	// 检查对齐
	if(addr & 0x1){
		log_warn("Memory address is not halfword aligned!");
		return FALSE;
	}
	// 设置CSW：Size=Halfword，AddrInc=off
	uint32_t csw_bak = dapObj->AP[DAP_CURR_AP(dapObj)].CSW.regData;
	dapObj->AP[DAP_CURR_AP(dapObj)].CSW.regInfo.AddrInc = ADDRINC_OFF;	// AddrInc Off
	if(dapObj->AP[DAP_CURR_AP(dapObj)].ctrl_state.lessWordTransfers == 1){	// 支持lessWordTransfer
		dapObj->AP[DAP_CURR_AP(dapObj)].CSW.regInfo.Size = SIZE_16;	// Halfword
	}else{
		dapObj->AP[DAP_CURR_AP(dapObj)].CSW.regInfo.Size = SIZE_32;	// Word
	}
	// 是否需要更新寄存器？
	if(dapObj->AP[DAP_CURR_AP(dapObj)].CSW.regData != csw_bak)
		if(DAP_AP_WriteReg(dapObj, AP_REG_CSW, dapObj->AP[DAP_CURR_AP(dapObj)].CSW.regData) == FALSE) return FALSE;
	// 获得半字在字中的索引
	int byteIdx = addr & 0x2;
	// 读取数据
	if(dapObj->AP[DAP_CURR_AP(dapObj)].ctrl_state.lessWordTransfers == 0){
		/**
		 * 不支持lessWordTransfer
		 * 需要将地址的低2位清零，4字节读取
		 */
		addr = (addr >> 2) << 2;
	}
	// 写入TAR
	if(DAP_Write_TAR(dapObj, addr) == FALSE) return FALSE;
	// 读取数据，根据byte lane获得数据
	uint32_t data;
	if(DAP_AP_ReadReg(dapObj, AP_REG_DRW, &data) == FALSE) return FALSE;
	// 拆分数据
	*data_out = (uint16_t)(data >> (8*byteIdx));
	return TRUE;
}

/**
 * 读取32位数据
 * addr：64位地址
 * data_out：读取的数据输出地址
 * 注意：地址要以字对齐，否则报错
 */
BOOL DAP_ReadMem32(DAPObject *dapObj, uint64_t addr, uint32_t *data_out){
	assert(dapObj != NULL && dapObj->AP[DAP_CURR_AP(dapObj)].ctrl_state.init == 1);
	// 检查对齐
	if(addr & 0x3){
		log_warn("Memory address is not word aligned!");
		return FALSE;
	}
	// 设置CSW：Size=Word，AddrInc=off
	uint32_t csw_bak = dapObj->AP[DAP_CURR_AP(dapObj)].CSW.regData;
	dapObj->AP[DAP_CURR_AP(dapObj)].CSW.regInfo.AddrInc = ADDRINC_OFF;	// AddrInc Off
	dapObj->AP[DAP_CURR_AP(dapObj)].CSW.regInfo.Size = SIZE_32;	// Word
	// 是否需要更新寄存器？
	if(dapObj->AP[DAP_CURR_AP(dapObj)].CSW.regData != csw_bak)
		if(DAP_AP_WriteReg(dapObj, AP_REG_CSW, dapObj->AP[DAP_CURR_AP(dapObj)].CSW.regData) == FALSE) return FALSE;
	// 写入TAR
	if(DAP_Write_TAR(dapObj, addr) == FALSE) return FALSE;
	// 读取数据，根据byte lane获得数据
	if(DAP_AP_ReadReg(dapObj, AP_REG_DRW, data_out) == FALSE) return FALSE;
	return TRUE;
}

/**
 * 写入32位数据
 * addr：64位地址
 * data_out：读取的数据输出地址
 * 注意：地址要以字对齐，否则报错
 */
BOOL DAP_WriteMem32(DAPObject *dapObj, uint64_t addr, uint32_t data){
	assert(dapObj != NULL && dapObj->AP[DAP_CURR_AP(dapObj)].ctrl_state.init == 1);
	// 检查对齐
	if(addr & 0x3){
		log_warn("Memory address is not word aligned!");
		return FALSE;
	}
	// 设置CSW：Size=Word，AddrInc=off
	uint32_t csw_bak = dapObj->AP[DAP_CURR_AP(dapObj)].CSW.regData;
	dapObj->AP[DAP_CURR_AP(dapObj)].CSW.regInfo.AddrInc = ADDRINC_OFF;	// AddrInc Off
	dapObj->AP[DAP_CURR_AP(dapObj)].CSW.regInfo.Size = SIZE_32;	// Word
	// 是否需要更新寄存器？
	if(dapObj->AP[DAP_CURR_AP(dapObj)].CSW.regData != csw_bak)
		if(DAP_AP_WriteReg(dapObj, AP_REG_CSW, dapObj->AP[DAP_CURR_AP(dapObj)].CSW.regData) == FALSE) return FALSE;
	// 写入TAR
	if(DAP_Write_TAR(dapObj, addr) == FALSE) return FALSE;
	// 读取数据，根据byte lane获得数据
	if(DAP_AP_WriteReg(dapObj, AP_REG_DRW, data) == FALSE) return FALSE;
	return TRUE;
}

/**
 * 读取内存数据
 * memAddr：读取的内存地址
 * dataSize：每次读取的位数。8,16,32,64,128,256
 * buff：从哪儿读取数据
 * cnt：需要传输多少个数据，以dataSize为单位
 * transdered_out：传输多少个数据
 * buffInc：buff地址自增？单位dataSize
 * memInc：内存地址自增？单位dataSize
 * 返回：TRUE，FALSE
 */
BOOL DAP_ReadMemory(DAPObject *dapObj, uint64_t memAddr, int dataSize, uint8_t *buff, int cnt, int *transdered_out, BOOL buffInc, BOOL memInc){
	return FALSE;
}

/**
 * 读取Component ID和Peripheral ID
 * componentBase：Component的基址，必须4KB对齐
 * cid_out：读取的Component ID
 * pid_out：读取的Peripheral ID
 */
BOOL DAP_Read_CID_PID(DAPObject *dapObj, uint32_t componentBase, uint32_t *cid_out, uint64_t *pid_out){
	assert(dapObj != NULL);
	jmp_buf exception;
	if((componentBase & 0xFFF) != 0) {
		log_warn("Component base address is not 4KB aligned!");
		return FALSE;
	}
	*cid_out = 0; *pid_out = 0;
	uint32_t cid0, cid1, cid2, cid3;
	uint32_t pid0, pid1, pid2, pid3, pid4;	// pid5-7全是0，所以不用读
	// 错误处理
	switch(setjmp(exception)){
	case 1: log_warn("DAP_ReadMem32:Read Component ID Failed!"); return FALSE;
	case 2: log_warn("DAP_ReadMem32:Read Peripheral ID Failed!"); return FALSE;
	case 3: log_warn("TAP_Execute:Failed!"); return FALSE;
	default: log_warn("Unknow Error."); return FALSE;
	case 0:break;
	}
	// 读取Component ID
	if(DAP_ReadMem32(dapObj, componentBase + 0xFF0, &cid0) == FALSE) longjmp(exception, 1);
	if(DAP_ReadMem32(dapObj, componentBase + 0xFF4, &cid1) == FALSE) longjmp(exception, 1);
	if(DAP_ReadMem32(dapObj, componentBase + 0xFF8, &cid2) == FALSE) longjmp(exception, 1);
	if(DAP_ReadMem32(dapObj, componentBase + 0xFFC, &cid3) == FALSE) longjmp(exception, 1);
	// 读取Peripheral ID
	if(DAP_ReadMem32(dapObj, componentBase + 0xFE0, &pid0) == FALSE) longjmp(exception, 2);
	if(DAP_ReadMem32(dapObj, componentBase + 0xFE4, &pid1) == FALSE) longjmp(exception, 2);
	if(DAP_ReadMem32(dapObj, componentBase + 0xFE8, &pid2) == FALSE) longjmp(exception, 2);
	if(DAP_ReadMem32(dapObj, componentBase + 0xFEC, &pid3) == FALSE) longjmp(exception, 2);
	if(DAP_ReadMem32(dapObj, componentBase + 0xFD0, &pid4) == FALSE) longjmp(exception, 2);

	*cid_out = (cid3 & 0xff) << 24 | (cid2 & 0xff) << 16 | (cid1 & 0xff) << 8 | (cid0 & 0xff);
	*pid_out = (uint64_t)(pid4 & 0xff) << 32 | (pid3 & 0xff) << 24 | (pid2 & 0xff) << 16 | (pid1 & 0xff) << 8 | (pid0 & 0xff);
	return TRUE;
}

/**
 * 将一个新指令插入DAP指令队列
 * 返回NULL 失败
 */
static struct DAP_Instr * DAP_NewInstruction(DAPObject *dapObj){
	// 新建指令对象
	struct DAP_Instr *instruct = calloc(1, sizeof(struct DAP_Instr));
	if(instruct == NULL){
		log_warn("Failed to create a new instruction object.");
		return NULL;
	}
	// 插入到指令队列尾部
	list_add_tail(&instruct->list_entry, &dapObj->instrQueue);
	return instruct;
}

//#define DAP_QUEUE_DEBUG
/**
 * 将一个读取寄存器指令加入队列
 * APnDP:0 = Debug Port (DP), 1 = Access Port (AP).
 * reg:要读取的寄存器
 * data_out:读取的数据存放地址
 * 返回：TRUE、FALSE
 */
BOOL DAP_Queue_xP_Read(DAPObject *dapObj, int APnDP, uint8_t A, uint32_t *data_out){
	assert(dapObj != NULL);
	*data_out = 0;	// 先清零
	struct DAP_Instr * instr = DAP_NewInstruction(dapObj);
	if(instr == NULL){
		return FALSE;
	}
	instr->seq.info.RnW = 1;	// Read
	instr->seq.info.APnDP = APnDP;
	instr->data.out = data_out;	// 数据地址
	instr->seq.info.A = (A & 0xC) >> 2;
	// TODO likely 分支预测
	if(instr->list_entry.prev != &dapObj->instrQueue){	// 如果上一个节点不是头结点
		struct DAP_Instr * last_instr = list_entry(instr->list_entry.prev, struct DAP_Instr, list_entry);
		// 计算当前指令的缓冲索引
		// Ra Rb a!=b
		// W R
		if((last_instr->seq.info.RnW == 0) || ((last_instr->seq.info.RnW == 1) && (last_instr->seq.info.APnDP != instr->seq.info.APnDP))){
			last_instr->seq.info.end = 1;	// 标记上一个指令结束
		}
	}
	return TRUE;
}

/**
 * 将一个写入寄存器指令加入队列
 * APnDP:0 = Debug Port (DP), 1 = Access Port (AP).
 * reg:要写入的寄存器
 * data:要写入的值
 * 返回：TRUE、FALSE
 */
BOOL DAP_Queue_xP_Write(DAPObject *dapObj, int APnDP, uint8_t A, uint32_t data){
	assert(dapObj != NULL);
	struct DAP_Instr * instr = DAP_NewInstruction(dapObj);
	instr->seq.info.RnW = 0;	// Write
	instr->seq.info.APnDP = APnDP;
	instr->data.in = data;	// 数据
	instr->seq.info.A = (A & 0xC) >> 2;
	// TODO likely 分支预测
	if(instr->list_entry.prev != &dapObj->instrQueue){	// 如果上一个节点不是头结点
		struct DAP_Instr *last_instr = list_entry(instr->list_entry.prev, struct DAP_Instr, list_entry);
		// Rx Wx
		if(last_instr->seq.info.RnW == 1){
			last_instr->seq.info.end = 1;
		}
	}
	return TRUE;
}

/**
 * JTAG模式下执行DAP指令队列错误恢复点
 */
static jmp_buf JTAG_Instr_exception;

/**
 * 指令分拣
 * 1、将响应为WAIT的指令插入到重试队列
 * 2、将响应为ok的指令找到下一个响应为ok的数据索引
 * TODO review
 */
static void DAP_JTAG_instr_sort_out(DAPObject *dapObj, struct list_head *instrQueue, uint8_t (*buff)[5], int buffLen){
	// 数据已经在缓冲区就绪了，同步到指令中
	struct DAP_Instr *instr,*instr_t,*instr_last;
	int buffIdx = 0;
	// 指令分拣
	list_for_each_entry_safe(instr, instr_t, instrQueue, list_entry){
		if((*(buff + buffIdx)[0] & 0x7) == JTAG_RESP_WAIT){
			if(instr->seq.info.end && instr->list_entry.prev != instrQueue ){	// 如果当前指令是最后一个，那么将上一个指令设置成最后一个
				instr_last = list_entry(instr->list_entry.prev, struct DAP_Instr, list_entry);
				instr_last->seq.info.end = 1;
			}
			list_move_tail(&instr->list_entry, &dapObj->retryQueue);	// 将该指令插入到重试队列
		}else{	// 当前指令成功发布，找到从buffIdx+1开始的一个ok
			int t;
			for(t = buffIdx + 1;t < buffLen;t++){
				if((*(buff + t)[0] & 0x7) != JTAG_RESP_WAIT) break;
			}
			if(t < buffLen){	// 找到了
				instr->buffIdx = t;
			}else{	// 没找到，TODO 将该指令和后面的所有指令都插入重试队列
				if(instr->seq.info.end && instr->list_entry.prev != instrQueue){	// 如果当前指令是最后一个，那么将上一个指令设置成最后一个
					instr_last = list_entry(instr->list_entry.prev, struct DAP_Instr, list_entry);
					instr_last->seq.info.end = 1;
				}
				list_move_tail(&instr->list_entry, &dapObj->retryQueue);
			}
		}
		buffIdx++;
		if(instr->seq.info.end)break;
	}
}


/**
 * 执行队列里的一组指令
 * 将这组指令转换成DAP协议插入到JTAG命令队列中
 * 该指令出现错误不会返回。
 */
static int DAP_JTAG_exe_queue(DAPObject *dapObj, struct list_head *instrQueue, uint8_t (*buff)[5]){
	int buffIdx = 0;
	struct DAP_Instr *instr;
	list_for_each_entry(instr, instrQueue, list_entry){
		// 当前指令的扫描链
		uint32_t currentIR = instr->seq.info.APnDP ? JTAG_APACC : JTAG_DPACC;
		// 选择合适的扫描链
		if (dapObj->ir != currentIR) {
			dapObj->ir = currentIR;
			// 选择当前IR
			if(TAP_IR_Write(&dapObj->tapObj, dapObj->TAP_index, currentIR) == FALSE) longjmp(JTAG_Instr_exception, 1);
		}
		if(instr->seq.info.RnW == 1){	// 读寄存器
			// 构造数据
			*(buff + buffIdx)[0] = (instr->seq.info.A << 1) | 1;
		}else{	// 写寄存器
			// 构建当前扫描链数据
			MAKE_nPACC_CHAIN_DATA(buff + buffIdx, instr->data.in, instr->seq.info.A << 1);
		}
		if(TAP_DR_Exchange(&dapObj->tapObj, dapObj->TAP_index, 35, CAST(uint8_t *,buff + buffIdx)) == FALSE) longjmp(JTAG_Instr_exception, 2);
		buffIdx++;
		if(instr->seq.info.end) break;	// 如果当前处理的指令标志了end位，则该指令组结束
	}
	// 最后读RDBUFF
	if (dapObj->ir != JTAG_DPACC) {
		dapObj->ir = JTAG_DPACC;
		if(TAP_IR_Write(&dapObj->tapObj, dapObj->TAP_index, JTAG_DPACC) == FALSE) longjmp(JTAG_Instr_exception, 1);
	}
	// 读取RDBUFF，如果上一个是写，则只需要ACK。
	*(buff + buffIdx)[0] = ((DP_REG_RDBUFF >> 1) & 0x6) | 1;
	if(TAP_DR_Exchange(&dapObj->tapObj, dapObj->TAP_index, 35, CAST(uint8_t *,buff + buffIdx)) == FALSE) longjmp(JTAG_Instr_exception, 2);
	// 指令都加入到队列中，执行队列
	if(TAP_Execute(&dapObj->tapObj) == FALSE) longjmp(JTAG_Instr_exception, 3);
	return buffIdx;
}

/**
 * 同步指令数据
 */
static void DAP_JTAG_instr_sync(struct list_head *instrQueue, uint8_t (*buff)[5]){
	struct DAP_Instr *instr, *instr_t;
	// 同步指令数据
	list_for_each_entry_safe(instr, instr_t, instrQueue, list_entry){
		// 找到是ok的响应。
		if(instr->seq.info.RnW == 1){	// 如果是读
			// 返回数据
			*instr->data.out = GET_nPACC_CHAIN_DATA(buff + instr->buffIdx);
		}
		// 从指令寄存器中移除
		list_del(&instr->list_entry);	// 将链表中删除
		if(instr->seq.info.end){
			free(instr);
			break;
		}else{
			free(instr);
		}
	}
}


/**
 * JTAG模式下刷新指令队列
 */
static BOOL DAP_JTAG_Execute(DAPObject *dapObj){
	BOOL result = FALSE;
	// 错误处理
	switch(setjmp(JTAG_Instr_exception)){
	case 1: log_warn("TAP_IR_Write: Select Scan Chain Failed!"); goto EXIT_STEP_2;
	case 2: log_warn("TAP_DR_Exchange:Failed!"); goto EXIT_STEP_2;
	case 3: log_warn("TAP_Execute:Failed!"); goto EXIT_STEP_2;
	default: log_warn("Unknow Error."); goto EXIT_STEP_2;
	case 0:break;
	}
	struct DAP_Instr *instr, *last_t;
	int instrCnt;
EXEQUEUE:
	instrCnt = 0;
	// 计算本组指令个数
	list_for_each_entry(instr, &dapObj->instrQueue, list_entry){
		instrCnt ++;
		if(instr->seq.info.end) break;
	}
	// 分配本组指令JTAG数据交换缓冲区
	uint8_t (*jtag_buff)[5] = calloc((instrCnt + 1) * 5, sizeof(uint8_t));	// +1 是后面的RDBUFF
	if(jtag_buff == NULL){
		log_warn("Failed to create JTAG-DP data buff.");
		return FALSE;
	}
	// 执行指令
	DAP_JTAG_exe_queue(dapObj, &dapObj->instrQueue, jtag_buff);
	// 分拣指令
	DAP_JTAG_instr_sort_out(dapObj, &dapObj->instrQueue, jtag_buff, instrCnt + 1);
	// 同步数据
	DAP_JTAG_instr_sync(&dapObj->instrQueue, jtag_buff);

	int retry = dapObj->retry;
	// 判断重试队列是否为空，不为空则执行重试队列
	while(!list_empty(&dapObj->retryQueue) && retry--){
		// 执行指令
		int buff_len = DAP_JTAG_exe_queue(dapObj, &dapObj->retryQueue, jtag_buff);
		// 分拣指令
		DAP_JTAG_instr_sort_out(dapObj, &dapObj->retryQueue, jtag_buff, buff_len + 1);	// buff_len是从0开始算的
		// 同步数据
		DAP_JTAG_instr_sync(&dapObj->retryQueue, jtag_buff);
	}
	if(!list_empty(&dapObj->retryQueue)){
		log_warn("Failed after retrying!");
		free(jtag_buff);
		return FALSE;
	}
	free(jtag_buff);	// 释放掉内存
	if(!list_empty(&dapObj->instrQueue)){
		goto EXEQUEUE;
	}
	result = TRUE;
	goto EXIT_STEP_1;	// 正常退出
EXIT_STEP_2:
	free(jtag_buff);	// 释放掉内存
EXIT_STEP_1:
	TAP_FlushQueue(&dapObj->tapObj);	// 当出现错误的时候清空指令队列
EXIT_STEP_0:
	return result;
}

/**
 * 执行指令队列
 * 刷新指令队列，并同步结果
 * 返回：TRUE、FALSE
 */
BOOL DAP_Queue_Execute(DAPObject *dapObj){
	assert(dapObj != NULL);
	if(ADAPTER_CURR_TRANS(dapObj->tapObj.adapterObj) == JTAG){
		return DAP_JTAG_Execute(dapObj);
	}else{
		return FALSE;	// 其他
	}
}
