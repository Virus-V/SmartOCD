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
 * 读取DP寄存器
 * index：JTAG模式下TAP的索引，SWD模式下忽略该参数
 * reg：要读取的寄存器
 */
uint32_t DAP_DP_Read(DAPObject *dapObj, uint16_t index, uint8_t reg){
	assert(dapObj != NULL);
	int resu;
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
		uint8_t data[5];	// 扫描链35比特
		// 装填数据：data[0]是RnW,1是R，0是W
		// data[2:1]是地址A[3:2]
		data[0] = ((reg >> 1) & 0x6) | 1;
		if(dapObj->ir != JTAG_DPACC){
			dapObj->ir = JTAG_DPACC;
			// 选中DPACC扫描链
			if(TAP_IR_Write(&dapObj->tapObj, index, JTAG_DPACC) == FALSE) longjmp(exception, 1);
		}
		// 写入控制信息 A[3:2] RnW
		if(TAP_DR_Exchange(&dapObj->tapObj, index, 35, data) == FALSE) longjmp(exception, 2);
		// 执行队列
		if(TAP_Execute(&dapObj->tapObj) == FALSE) longjmp(exception, 3);
		// 判断是否成功
		if((data[0] & 0x7) != JTAG_RESP_OK_FAULT){
			log_warn("The last JTAG ACK was WAIT.");
			return 0;
		}
		// 读取上次的寄存器内容
		data[0] = ((DP_RDBUFF >> 1) & 0x6) | 1;
		if(TAP_DR_Exchange(&dapObj->tapObj, index, 35, data) == FALSE) longjmp(exception, 2);
		// 执行队列
		if(TAP_Execute(&dapObj->tapObj) == FALSE) longjmp(exception, 3);
		if((data[0] & 0x7) != JTAG_RESP_OK_FAULT){
			log_warn("Read RDBUFF Failed. The last JTAG ACK was WAIT.");
			return 0;
		}
		return GET_nPACC_CHAIN_DATA(data);
	}else{
		// TODO SWD模式下
		return 0;
	}
}

/**
 * 写入DP寄存器
 * index：JTAG模式下TAP的索引，SWD模式下忽略该参数
 * reg：要写入的寄存器
 * data：写入的内容
 */
BOOL DAP_DP_Write(DAPObject *dapObj, uint16_t index, uint8_t reg, uint32_t data){
	assert(dapObj != NULL);
	int resu;
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
		// 装填数据：data[0]是RnW,1是R，0是W
		// data[2:1]是地址A[3:2]
		// 装填数据
		MAKE_nPACC_CHAIN_DATA(buff, data, (reg >> 1) & 0x6);

		if(dapObj->ir != JTAG_DPACC){
			dapObj->ir = JTAG_DPACC;
			// 选中DPACC扫描链
			if(TAP_IR_Write(&dapObj->tapObj, index, JTAG_DPACC) == FALSE) longjmp(exception, 1);
		}
		// 写入数据
		if(TAP_DR_Exchange(&dapObj->tapObj, index, 35, buff) == FALSE) longjmp(exception, 2);
		// 执行队列
		if(TAP_Execute(&dapObj->tapObj) == FALSE) longjmp(exception, 3);
		// 判断写入是否成功
		if((buff[0] & 0x7) != JTAG_RESP_OK_FAULT){
			log_warn("The last JTAG ACK was WAIT. This time the data was discarded.");
			return FALSE;
		}
		// 读取上次的寄存器内容
		buff[0] = ((DP_RDBUFF >> 1) & 0x6) | 1;
		if(TAP_DR_Exchange(&dapObj->tapObj, index, 35, buff) == FALSE) longjmp(exception, 2);
		// 执行队列
		if(TAP_Execute(&dapObj->tapObj) == FALSE) longjmp(exception, 3);
		if((buff[0] & 0x7) != JTAG_RESP_OK_FAULT){
			log_warn("Read RDBUFF Failed. The last JTAG ACK was WAIT.");
			return FALSE;
		}
	}else{
		// TODO SWD模式下
		return 0;
	}
}

uint32_t DAP_AP_Read(DAPObject *dapObj);
BOOL DAP_AP_Write(DAPObject *dapObj);
// 选择AP
BOOL DAP_AP_Select(DAPObject *dapObj);

