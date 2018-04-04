/*
 * target_test.c
 *
 *  Created on: 2018-3-30
 *      Author: virusv
 */


#include <stdio.h>
#include "misc/log.h"
#include "target/target.h"
#include "debugger/cmsis-dap.h"

extern int print_bulk(char *data, int length, int rowLen);

uint16_t vids[] = {0xc251, 0};
uint16_t pids[] = {0xf001, 0};

int main(){
	TargetObject *targetObj;
	struct cmsis_dap *cmsis_dapObj;
	log_set_level(LOG_DEBUG);
	targetObj = calloc(1, sizeof(TargetObject));
	if(targetObj == NULL){
		log_fatal("failed to new target.");
		return 1;
	}
	cmsis_dapObj = NewCMSIS_DAP();
	if(cmsis_dapObj == NULL){
		log_fatal("failed to new cmsis dap.");
		goto EXIT_FAIL_CMSIS_DAP_INIT;
	}
	// 连接CMSIS-DAP
	if(ConnectCMSIS_DAP(cmsis_dapObj, vids, pids, NULL) == FALSE){
		log_fatal("failed to connect.");
		goto EXIT_FAIL_CMSIS_DAP_INIT;
	}
	if(__CONSTRUCT(Target)(targetObj, GET_ADAPTER(cmsis_dapObj)) == FALSE){
		log_fatal("Target initialization failed.");
		goto EXIT_ADAPTER_INIT_FAIL;
	}
	// 设置SWJ频率，最大30MHz
	target_SetClock(targetObj, 10000000u);

	// 切换到JTAG模式
	target_SelectTrasnport(targetObj, JTAG);

	// 增加tap
	log_debug("TAP index:%d.", target_JTAG_Add_TAP(targetObj, 4));
	log_debug("TAP index:%d.", target_JTAG_Add_TAP(targetObj, 5));

	list_iterator_t * tapIterator = list_iterator_new(targetObj->taps, LIST_HEAD);
	list_node_t *node = list_iterator_next(tapIterator);
	for(; node; node = list_iterator_next(tapIterator)){
		struct JTAG_TAP *tapObj = node->val;
		log_info("irLen %d.", tapObj->irLen);
	}
	list_iterator_destroy(tapIterator);

	// 增加JTAG指令

	// 执行JTAG指令

	// 删除tap
	target_JTAG_Remove_TAP(targetObj, 1);
	target_JTAG_Remove_TAP(targetObj, 0);

	// 释放对象
	__DESTORY(Target)(targetObj);
EXIT_ADAPTER_INIT_FAIL:
	FreeCMSIS_DAP(cmsis_dapObj);
EXIT_FAIL_CMSIS_DAP_INIT:
	free(targetObj);
	return 1;
EXIT_OK:
	return 0;
}
