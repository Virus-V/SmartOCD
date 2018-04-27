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
	log_set_level(LOG_DEBUG);
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
	DAP_DP_Write(dapObj, DP_SELECT, 0);
	// 上电
	DAP_DP_Write(dapObj, DP_CTRL_STAT, DP_CTRL_CSYSPWRUPREQ | DP_CTRL_CDBGPWRUPREQ);
	// 等待上电完成
	do {
		if(DAP_DP_Read(dapObj, DP_CTRL_STAT, &dapObj->CTRL_STAT_Reg.regData) == FALSE){
			log_fatal("Read CTRL_STAT Failed.");
			goto EXIT_STEP_2;
		}
	} while((dapObj->CTRL_STAT_Reg.regData & (DP_STAT_CDBGPWRUPACK | DP_STAT_CSYSPWRUPACK)) != (DP_STAT_CDBGPWRUPACK | DP_STAT_CSYSPWRUPACK));
	log_info("Power up.");

	uint32_t regs[16];

	// 队列操作
	log_debug("%d.",DAP_Queue_xP_Read(dapObj, DP, DP_CTRL_STAT, 	regs));	// 读状态寄存器
	log_debug("%d.",DAP_Queue_xP_Read(dapObj, DP, DP_SELECT, 	regs+1));
	log_debug("%d.",DAP_Queue_xP_Write(dapObj, DP, DP_SELECT, 	0xF0));	// 写DPSelect
	log_debug("%d.",DAP_Queue_xP_Read(dapObj, DP, DP_SELECT, 	regs+2));	// 读DPSelect

	log_debug("%d.",DAP_Queue_xP_Read(dapObj,  AP, AP_REG_ROM_LSB, 	regs+3));	// 读AP ROM
	log_debug("%d.",DAP_Queue_xP_Write(dapObj, DP, DP_SELECT, 	0x0));	// 写DPSelect
	log_debug("%d.",DAP_Queue_xP_Read(dapObj,  AP, AP_REG_CSW, 	regs+4));	// 读AP CSW

	log_debug("%d.",DAP_Queue_xP_Read(dapObj, DP, DP_CTRL_STAT, 	regs+5));	// 读DP STATUS
	log_debug("%d.",DAP_Queue_xP_Write(dapObj, DP, DP_SELECT, 	0xF0));	// 写DPSelect
	log_debug("%d.",DAP_Queue_xP_Read(dapObj,  DP, AP_REG_IDR, 	regs+6));	//读AP IDR
	log_debug("%d.",DAP_Queue_xP_Read(dapObj, DP, DP_SELECT, 	regs+7));
	log_debug("%d.",DAP_Queue_Execute(dapObj));
	for(int n=0;n<8;n++){
		printf("%x\n", regs[n]);
	}



EXIT_STEP_2:
	// 释放对象
	__DESTORY(DAP)(dapObj);
EXIT_STEP_1:
	GET_ADAPTER(&cmsis_dapObj)->Destroy(&cmsis_dapObj);
EXIT_STEP_0:
	free(dapObj);
	return 0;
}
