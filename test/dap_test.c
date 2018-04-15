/*
 * dap_test.c
 *
 *  Created on: 2018-4-14
 *      Author: virusv
 */


#include <stdio.h>
#include "misc/log.h"
#include "debugger/cmsis-dap.h"
#include "arch/ARM/ADI/DAP.h"

extern int print_bulk(char *data, int length, int rowLen);

uint16_t vids[] = {0xc251, 0};
uint16_t pids[] = {0xf001, 0};

int main(){
	DAPObject *dapObj;
	struct cmsis_dap *cmsis_dapObj;
	log_set_level(LOG_DEBUG);
	dapObj = calloc(1, sizeof(DAPObject));
	if(dapObj == NULL){
		log_fatal("failed to new dap object.");
		return 1;
	}
	cmsis_dapObj = NewCMSIS_DAP();
	if(cmsis_dapObj == NULL){
		log_fatal("failed to new cmsis dap.");
		goto EXIT_STEP_0;
	}
	// 连接CMSIS-DAP
	if(ConnectCMSIS_DAP(cmsis_dapObj, vids, pids, NULL) == FALSE){
		log_fatal("failed to connect.");
		goto EXIT_STEP_1;
	}
	// 同时对cmsis-dap进行初始化
	if(__CONSTRUCT(DAP)(dapObj, GET_ADAPTER(cmsis_dapObj)) == FALSE){
		log_fatal("Target initialization failed.");
		goto EXIT_STEP_1;
	}
	// 设置SWJ频率，最大30MHz
	adapter_SetClock(GET_ADAPTER(cmsis_dapObj), 100000u);
	// 切换到JTAG模式
	adapter_SelectTransmission(GET_ADAPTER(cmsis_dapObj), JTAG);

	// 复位TAP
	// TAP_Reset(dapObj, FALSE, 0);
	// 声明TAP
	uint16_t irLens[] = {4, 5};
	log_debug("target_JTAG_Set_TAP_Info:%d.", TAP_SetInfo(&dapObj->tapObj, 2, irLens));
	DAP_DP_Write(dapObj, 0, DP_SELECT, 0);
	// 上电
	DAP_DP_Write(dapObj, 0, DP_CTRL_STAT, DP_CTRL_CSYSPWRUPREQ | DP_CTRL_CDBGPWRUPREQ);
	// 等待上电完成
	uint32_t tmp;
	do {
		tmp = DAP_DP_Read(dapObj, 0, DP_CTRL_STAT);
		if(tmp == 0){
			log_fatal("Read CTRL_STAT Failed.");
			goto EXIT_STEP_2;
		}
	} while((tmp & (DP_STAT_CDBGPWRUPACK | DP_STAT_CSYSPWRUPACK)) != (DP_STAT_CDBGPWRUPACK | DP_STAT_CSYSPWRUPACK));
	log_info("Power up.");
	log_debug("CTRL/STAT: 0x%08X.", tmp);
	//DAP_DP_Read(dapObj, 0, DP_SELECT);
	//DAP_DP_Read(dapObj, 0, DP_CTRL_STAT);

EXIT_STEP_2:
	// 释放对象
	__DESTORY(DAP)(dapObj);
EXIT_STEP_1:
	FreeCMSIS_DAP(cmsis_dapObj);
EXIT_STEP_0:
	free(dapObj);
	return 0;
}
