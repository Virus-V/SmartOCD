/*
 * cmsis_test.c
 *
 *  Created on: 2018-2-15
 *      Author: virusv
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "smart_ocd.h"
#include "misc/log.h"
#include "debugger/cmsis-dap.h"
#include "arch/CoreSight/DAP.h"

extern int print_bulk(char *data, int length, int rowLen);

uint16_t vids[] = {0xc251, 0};
uint16_t pids[] = {0xf001, 0};
// 测试入口点
int main(){
	AdapterObject *adapterObj;
	struct cmsis_dap *cmsis_dapObj;
	uint32_t idcode;
	log_set_level(LOG_INFO);
	cmsis_dapObj = NewCMSIS_DAP();
	if(cmsis_dapObj == NULL){
		log_fatal("failed to new.");
		exit(1);
	}
	// 连接CMSIS-DAP
	if(ConnectCMSIS_DAP(cmsis_dapObj, vids, pids, NULL) == FALSE){
		log_fatal("failed to connect.");
		exit(1);
	}
	adapterObj = CAST(AdapterObject *, cmsis_dapObj);
	// 初始化
	adapterObj->Init(adapterObj);
	// 切换到SWD模式
	adapterObj->SelectTrans(adapterObj, SWD);
	// 设置SWJ频率，1MHz
	//adapterObj->Operate(adapterObj, AINS_SET_CLOCK, 1000000u);
	adapterObj->Operate(adapterObj, AINS_TRANSFER_CONFIG, 5, 10, 10);
	// 读取id code 寄存器
	adapterObj->Operate(adapterObj, AINS_READ_DP_REG, DP_CTRL_STAT, &idcode);
	log_info("SWD-DP with ID :0x%08x", idcode);

	adapterObj->Deinit(adapterObj);
	FreeCMSIS_DAP(cmsis_dapObj);
	exit(0);
}
