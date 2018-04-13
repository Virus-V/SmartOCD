/*
 * target_test.c
 *
 *  Created on: 2018-3-30
 *      Author: virusv
 */


#include <stdio.h>
#include "misc/log.h"
#include "lib/tap.h"
#include "debugger/cmsis-dap.h"

extern int print_bulk(char *data, int length, int rowLen);

uint16_t vids[] = {0xc251, 0};
uint16_t pids[] = {0xf001, 0};

int main(){
	TAPObject *tapObj;
	struct cmsis_dap *cmsis_dapObj;
	log_set_level(LOG_DEBUG);
	tapObj = calloc(1, sizeof(TAPObject));
	if(tapObj == NULL){
		log_fatal("failed to new target.");
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
	if(__CONSTRUCT(TAP)(tapObj, GET_ADAPTER(cmsis_dapObj)) == FALSE){
		log_fatal("Target initialization failed.");
		goto EXIT_STEP_1;
	}
	// 设置SWJ频率，最大30MHz
	TAP_SetClock(tapObj, 100000u);
	// 切换到JTAG模式
	TAP_SelectTrasnport(tapObj, JTAG);
	// 复位TAP，这句可能没必要
	TAP_Reset(tapObj, FALSE, 0);
	// 声明TAP
	// TODO 测试irlen长度为9会怎么样
	uint16_t irLens[] = {9, 9, 4, 9};
	log_debug("target_JTAG_Set_TAP_Info:%d.", TAP_Set_Info(tapObj, 4, irLens));
	uint32_t idCode[4] = {0, 0, 0, 0};
	TAP_Get_IDCODE(tapObj, idCode);
	//log_debug("0x%08X, 0x%08X, 0x%08X, 0x%08X.", idCode[0], idCode[1], idCode[2], idCode[3]);
	// 读取idcode
	for(int t = 0; t < 4; t++){
		for(int l=7;l<26;l++){
			idCode[0] = 0;
			log_debug("target_JTAG_IR_Write:%d.", TAP_IR_Write(tapObj, t, 0x0));
			// DR长度如果为9则会出现错误
			log_debug("target_JTAG_DR_Exchange:%d.", TAP_DR_Exchange(tapObj, t, l, CAST(uint8_t *, &idCode[0])));
			log_debug("target_JTAG_Execute:%d.", TAP_Execute(tapObj));
			log_debug("0x%08X.", idCode[0]);
		}
	}
EXIT_STEP_2:
	// 释放对象
	__DESTORY(TAP)(tapObj);
EXIT_STEP_1:
	FreeCMSIS_DAP(cmsis_dapObj);
EXIT_STEP_0:
	free(tapObj);
	return 0;
}
