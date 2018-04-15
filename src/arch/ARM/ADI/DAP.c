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

// 错误处理句柄
static jmp_buf exception;

/**
 * DAP对象构造函数
 */
BOOL __CONSTRUCT(DAP)(DAPObject *dapObj, AdapterObject *adapterObj){
	assert(dapObj != NULL && adapterObj != NULL);
	// 初始化TAP对象
	if(__CONSTRUCT(TAP)(&dapObj->tapObj, adapterObj) == FALSE){
		log_warn("TAP initialization failed.");
		return FALSE;
	}
	dapObj->ir = JTAG_BYPASS;	// ir选择BYPASS
	dapObj->retry = 5;	// 重试5次
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
static BOOL DAP_Read(DAPObject *dapObj, uint16_t index, int APnDP, uint8_t reg, uint32_t *data_out){
	int resu, retry = dapObj->retry;
	// 先将数据清零
	*data_out = 0;
	// 错误处理
	if((resu = setjmp(exception)) != 0){
		switch(resu){
		case 1:
			log_warn("TAP_IR_Write:Failed!");
			break;

		case 2:
			log_warn("TAP_DR_Exchange:Failed!");
			break;

		case 3:
			log_warn("TAP_Execute:Failed!");
			break;
		default:
			log_warn("Unknow Error.");
		}
		return FALSE;
	}
	// 如果当前协议是JTAG
	if(ADAPTER_CURR_TRANS(dapObj->tapObj.adapterObj) == JTAG){
		uint8_t buff[5];	// 扫描链35比特
		// 选择AP还是DP
		if(APnDP == 1){
			if(dapObj->ir != JTAG_APACC){
				dapObj->ir = JTAG_APACC;
				// 选中APACC扫描链
				if(TAP_IR_Write(&dapObj->tapObj, index, JTAG_APACC) == FALSE) longjmp(exception, 1);
			}
		}else{
			if(dapObj->ir != JTAG_DPACC){
				dapObj->ir = JTAG_DPACC;
				// 选中DPACC扫描链
				if(TAP_IR_Write(&dapObj->tapObj, index, JTAG_DPACC) == FALSE) longjmp(exception, 1);
			}
		}
		// 写入
		do{
			// 装填数据：data[0]是RnW,1是R，0是W
			// data[2:1]是地址A[3:2]
			buff[0] = ((reg >> 1) & 0x6) | 1;
			// 写入控制信息 A[3:2] RnW
			if(TAP_DR_Exchange(&dapObj->tapObj, index, 35, buff) == FALSE) longjmp(exception, 2);
			// 执行队列
			if(TAP_Execute(&dapObj->tapObj) == FALSE) longjmp(exception, 3);
		}while((buff[0] & 0x7) == JTAG_RESP_WAIT && retry--);
		// 判断是否成功
		if((buff[0] & 0x7) != JTAG_RESP_OK_FAULT){
			log_warn("The last JTAG ACK was WAIT.");
			return FALSE;
		}
		retry = dapObj->retry;
		// 读取上次的寄存器内容，重试
		do{
			buff[0] = ((DP_RDBUFF >> 1) & 0x6) | 1;
			if(TAP_DR_Exchange(&dapObj->tapObj, index, 35, buff) == FALSE) longjmp(exception, 2);
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
	}else{
		// TODO SWD模式下
		return FALSE;
	}
}

/**
 * 写入DP寄存器
 * index：JTAG模式下TAP的索引，SWD模式下忽略该参数
 * APnDP：读取AP寄存器还是DP寄存器 AP:1,DP:0
 * reg：要写入的寄存器
 * data：写入的内容
 */
static BOOL DAP_Write(DAPObject *dapObj, uint16_t index, int APnDP, uint8_t reg, uint32_t data_in){
	int resu, retry = dapObj->retry;
	// 错误处理
	if((resu = setjmp(exception)) != 0){
		switch(resu){
		case 1:
			log_warn("TAP_IR_Write:Failed!");
			break;

		case 2:
			log_warn("TAP_DR_Exchange:Failed!");
			break;

		case 3:
			log_warn("TAP_Execute:Failed!");
			break;
		default:
			log_warn("Unknow Error.");
		}
		return 0;
	}
	// 如果当前协议是JTAG
	if(ADAPTER_CURR_TRANS(dapObj->tapObj.adapterObj) == JTAG){
		uint8_t buff[5];	// 扫描链35比特
		// 选择AP还是DP
		if(APnDP == 1){
			if(dapObj->ir != JTAG_APACC){
				dapObj->ir = JTAG_APACC;
				// 选中APACC扫描链
				if(TAP_IR_Write(&dapObj->tapObj, index, JTAG_APACC) == FALSE) longjmp(exception, 1);
			}
		}else{
			if(dapObj->ir != JTAG_DPACC){
				dapObj->ir = JTAG_DPACC;
				// 选中DPACC扫描链
				if(TAP_IR_Write(&dapObj->tapObj, index, JTAG_DPACC) == FALSE) longjmp(exception, 1);
			}
		}
		// 装填数据：data[0]是RnW,1是R，0是W
		// data[2:1]是地址A[3:2]
		do{
			// 装填数据
			MAKE_nPACC_CHAIN_DATA(buff, data_in, (reg >> 1) & 0x6);
			// 写入数据
			if(TAP_DR_Exchange(&dapObj->tapObj, index, 35, buff) == FALSE) longjmp(exception, 2);
			// 执行队列
			if(TAP_Execute(&dapObj->tapObj) == FALSE) longjmp(exception, 3);
		}while((buff[0] & 0x7) == JTAG_RESP_WAIT && retry--);
		// 判断当前命令是否被提交
		if((buff[0] & 0x7) != JTAG_RESP_OK_FAULT){
			log_warn("The last JTAG ACK was WAIT. This time the data was discarded.");
			return FALSE;
		}
//		// 读取上次的寄存器内容
//		buff[0] = ((DP_RDBUFF >> 1) & 0x6) | 1;
//		if(TAP_DR_Exchange(&dapObj->tapObj, index, 35, buff) == FALSE) longjmp(exception, 2);
//		// 执行队列
//		if(TAP_Execute(&dapObj->tapObj) == FALSE) longjmp(exception, 3);
//		if((buff[0] & 0x7) != JTAG_RESP_OK_FAULT){
//			log_warn("Read RDBUFF Failed. The last JTAG ACK was WAIT.");
//			return FALSE;
//		}
		return TRUE;
	}else{
		// TODO SWD模式下
		return FALSE;
	}
}

// 设置AP和DP的寄存器Bank
static BOOL DAP_SetBank(DAPObject *dapObj, uint16_t index, int APnDP, uint8_t reg){
	uint8_t regBank = (reg >> 4) & 0xF;
	// 当前select寄存器备份
	uint32_t selectBak = dapObj->SelectReg.data;
	BOOL result = TRUE;
	if(APnDP == 1){	// AP
		if(dapObj->SelectReg.info.AP_BankSel == regBank){
			return TRUE;
		}
		dapObj->SelectReg.info.AP_BankSel = regBank;
		// 写入SELECT寄存器
		result = DAP_Write(dapObj, index, 0, DP_SELECT, dapObj->SelectReg.data);
		// 如果执行失败，则恢复寄存器
		if(result == FALSE){
			log_warn("AP Bank Selection Failed.");
			dapObj->SelectReg.data = selectBak;
		}
	}else{
		if(dapObj->SelectReg.info.DP_BankSel == regBank){
			return TRUE;
		}
		dapObj->SelectReg.info.DP_BankSel = regBank;
		// 写入SELECT寄存器
		result = DAP_Write(dapObj, index, 0, DP_SELECT, dapObj->SelectReg.data);
		// 如果执行失败，则恢复寄存器
		if(result == FALSE){
			log_warn("DP Bank Selection Failed.");
			dapObj->SelectReg.data = selectBak;
		}
	}
	return result;
}

// 读取DP寄存器
BOOL DAP_DP_Read(DAPObject *dapObj, uint16_t index, uint8_t reg, uint32_t *data_out){
	assert(dapObj != NULL);
	// 设置BankSel
	if(DAP_SetBank(dapObj, index, 0, reg) == FALSE){
		return FALSE;
	}
	return DAP_Read(dapObj, index, 0, reg, data_out);
}

// 写入DP寄存器
BOOL DAP_DP_Write(DAPObject *dapObj, uint16_t index, uint8_t reg, uint32_t data){
	assert(dapObj != NULL);
	// 设置BankSel
	if(DAP_SetBank(dapObj, index, 0, reg) == FALSE){
		return FALSE;
	}
	return DAP_Write(dapObj, index, 0, reg, data);
}

// 读取AP寄存器
BOOL DAP_AP_Read(DAPObject *dapObj, uint16_t index, uint8_t reg, uint32_t *data_out){
	assert(dapObj != NULL);
	// 设置BankSel
	if(DAP_SetBank(dapObj, index, 1, reg) == FALSE){
		return FALSE;
	}
	return DAP_Read(dapObj, index, 1, reg, data_out);
}

// 写入AP寄存器
BOOL DAP_AP_Write(DAPObject *dapObj, uint16_t index, uint8_t reg, uint32_t data){
	assert(dapObj != NULL);
	// 设置BankSel
	if(DAP_SetBank(dapObj, index, 1, reg) == FALSE){
		return FALSE;
	}
	return DAP_Write(dapObj, index, 1, reg, data);
}

/**
 * 选择AP
 * index：JTAG模式下TAP的索引，SWD模式下忽略该参数
 * apIdx：ap的索引，0~255
 */
BOOL DAP_AP_Select(DAPObject *dapObj, uint16_t index, uint8_t apIdx){
	assert(dapObj != NULL);
	// 当前select寄存器备份
	uint32_t selectBak = dapObj->SelectReg.data;
	BOOL result = TRUE;
	if(dapObj->SelectReg.info.AP_Sel == apIdx){
		return TRUE;
	}
	// 修改当前apSel值
	dapObj->SelectReg.info.AP_Sel = apIdx;
	// 写入SELECT寄存器
	result = DAP_Write(dapObj, index, 0, DP_SELECT, dapObj->SelectReg.data);
	if(result == FALSE){
		log_warn("AP Selection Failed.");
		dapObj->SelectReg.data = selectBak;
	}
	return result;
}

/**
 * 检查STATUS状态
 */

