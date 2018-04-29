/*
 * dap_queue_test.c
 *
 *  Created on: 2018-4-27
 *      Author: virusv
 */


#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "misc/log.h"
#include "debugger/cmsis-dap.h"
#include "arch/ARM/ADI/DAP.h"

extern int print_bulk(char *data, int length, int rowLen);

uint16_t vids[] = {0xc251, 0};
uint16_t pids[] = {0xf001, 0};

int main(){
	DAPObject *dapObj;
	struct cmsis_dap cmsis_dapObj;
	log_set_level(LOG_TRACE);
	dapObj = calloc(1, sizeof(DAPObject));
	if(dapObj == NULL){
		log_fatal("failed to new dap object.");
		return 1;
	}
	if(NewCMSIS_DAP(&cmsis_dapObj) == FALSE){
		log_fatal("failed to new cmsis dap.");
		goto EXIT_STEP_0;
	}
	// 连接CMSIS-DAP
	if(ConnectCMSIS_DAP(&cmsis_dapObj, vids, pids, NULL) == FALSE){
		log_fatal("failed to connect.");
		goto EXIT_STEP_1;
	}
	// 同时对cmsis-dap进行初始化
	if(__CONSTRUCT(DAP)(dapObj, GET_ADAPTER(&cmsis_dapObj)) == FALSE){
		log_fatal("Target initialization failed.");
		goto EXIT_STEP_1;
	}
	// 设置SWJ频率，最大30MHz
	adapter_SetClock(GET_ADAPTER(&cmsis_dapObj), 100000u);
	// 切换到JTAG模式
	adapter_SelectTransmission(GET_ADAPTER(&cmsis_dapObj), JTAG);
	// 设置DAP在JTAG扫描链的索引值
	DAP_Set_TAP_Index(dapObj, 0);
	// 设置延时
	//TAP_Set_DR_Delay(&dapObj->tapObj, 15);
	// 设置DAP WAIT响应的重试次数
	DAP_SetRetry(dapObj, 10);
	// 设置错误回调函数
	//DAP_SetErrorHandle(dapObj, errorHandle);
	// 复位TAP
	// TAP_Reset(dapObj, FALSE, 0);
	// 声明TAP
	uint16_t irLens[] = {4,5};
	log_debug("target_JTAG_Set_TAP_Info:%d.", TAP_SetInfo(&dapObj->tapObj, 2, irLens));
	// 写入Abort寄存器
	//DAP_WriteAbort(dapObj, 0x1);
	dapObj->CTRL_STAT_Reg.regData = 0;
	DAP_DP_Write(dapObj, DP_REG_SELECT, 0);
	// 上电
	DAP_DP_Write(dapObj, DP_REG_CTRL_STAT, DP_CTRL_CSYSPWRUPREQ | DP_CTRL_CDBGPWRUPREQ);
	// 等待上电完成
	do {
		if(DAP_DP_Read(dapObj, DP_REG_CTRL_STAT, &dapObj->CTRL_STAT_Reg.regData) == FALSE){
			log_fatal("Read CTRL_STAT Failed.");
			goto EXIT_STEP_2;
		}
	} while((dapObj->CTRL_STAT_Reg.regData & (DP_STAT_CDBGPWRUPACK | DP_STAT_CSYSPWRUPACK)) != (DP_STAT_CDBGPWRUPACK | DP_STAT_CSYSPWRUPACK));
	log_info("Power up.");

	uint32_t *regs = malloc(sizeof(uint32_t) * 2048);
	uint32_t *buff = regs;
	// 队列操作
	DAP_Queue_DP_Write(dapObj, DP_REG_SELECT, 	0x00);	// 写DPSelect
	DAP_Queue_AP_Write(dapObj, AP_REG_CSW, 	0x23000050 | AP_CSW_SIZE32);	// CSW
	for(int addr=0;addr<32;addr++){
		DAP_Queue_AP_Write(dapObj, AP_REG_TAR_LSB, 	0x08000000u + (addr << 2));	// 写地址寄存器
		DAP_Queue_AP_Read(dapObj, AP_REG_TAR_LSB, 	buff++);	// 读DRW
		DAP_Queue_AP_Read(dapObj, AP_REG_DRW, 	buff++);	// 读DRW
		//DAP_Queue_xP_Read(dapObj,  AP, AP_REG_DRW, 	buff++);	// 读DRW 两次读DRW会出错
	}
	DAP_Queue_Execute(dapObj);
	for(int n=0;n<128;n+=2){
		printf("0x%08X => 0x%08X\n", regs[n], regs[n+1]);
	}

	free(regs);

EXIT_STEP_2:
	// 释放对象
	__DESTORY(DAP)(dapObj);
EXIT_STEP_1:
	GET_ADAPTER(&cmsis_dapObj)->Destroy(&cmsis_dapObj);
EXIT_STEP_0:
	free(dapObj);
	return 0;
}
