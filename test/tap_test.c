/*
 * target_test.c
 *
 *  Created on: 2018-3-30
 *      Author: virusv
 */

#include <stdio.h>
#include <stdlib.h>
#include "misc/log.h"
#include "debugger/cmsis-dap.h"
#include "lib/TAP.h"

extern int print_bulk(char *data, int length, int rowLen);

uint16_t vids[] = {0xc251, 0};
uint16_t pids[] = {0xf001, 0};

int main(){
	TAPObject *tapObj;
	struct cmsis_dap cmsis_dapObj;
	log_set_level(LOG_TRACE);
	tapObj = calloc(1, sizeof(TAPObject));
	if(tapObj == NULL){
		log_fatal("failed to new target.");
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
	if(__CONSTRUCT(TAP)(tapObj, GET_ADAPTER(&cmsis_dapObj)) == FALSE){
		log_fatal("Target initialization failed.");
		goto EXIT_STEP_1;
	}
	// 设置SWJ频率，最大30MHz
	adapter_SetClock(GET_ADAPTER(&cmsis_dapObj), 100000u);
	// 切换到JTAG模式
	adapter_SelectTransmission(GET_ADAPTER(&cmsis_dapObj), JTAG);
	TAP_Set_DR_Delay(tapObj, 128);
	// 复位TAP，这句可能没必要
	TAP_Reset(tapObj, FALSE, 0);
	// 声明TAP
	// 测试irlen长度为9会怎么样？ 正常
	uint16_t irLens[] = {4, 5, 9, 17};	// 35
	log_debug("target_JTAG_Set_TAP_Info:%d.", TAP_SetInfo(tapObj, 4, irLens));
	//uint32_t idCode[4] = {0, 0, 0, 0};
	//TAP_Get_IDCODE(tapObj, idCode);
	//log_debug("0x%08X, 0x%08X.", idCode[0], idCode[1]);
	// 读取idcode
	uint8_t *tmp_data = calloc(2048, sizeof(uint8_t));
	if(tmp_data == NULL) goto EXIT_STEP_2;
	log_debug("target_JTAG_IR_Write:%d.", TAP_IR_Write(tapObj, 3, 0x0u));
	// DR长度如果为9则会出现错误
	for(int n=0;n<512;n++){
		log_debug("target_JTAG_DR_Exchange:%d.", TAP_DR_Exchange(tapObj, 3, 129, tmp_data));
	}
	log_debug("target_JTAG_Execute:%d.", TAP_Execute(tapObj));
	misc_PrintBulk(tmp_data, 2048, 32);
	//log_debug("0x%08X.", idCode[0]);
EXIT_STEP_2:
	// 释放对象
	__DESTORY(TAP)(tapObj);
EXIT_STEP_1:
	GET_ADAPTER(&cmsis_dapObj)->Destroy(&cmsis_dapObj);
EXIT_STEP_0:
	free(tapObj);
	return 0;
}
