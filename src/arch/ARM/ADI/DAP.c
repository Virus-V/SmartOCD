/*
 * DAP.c
 *
 *  Created on: 2018-2-25
 *      Author: virusv
 */

#ifdef HAVE_CONFIG
#include "global_config.h"
#endif

#include <stdlib.h>
#include <string.h>
#include "smart_ocd.h"
#include "misc/log.h"
#include "debugger/adapter.h"

/*
 * DAP 操作函数
 * 读写DP寄存器
 * 读写AP寄存器
 * ///读写32位地址的内存8、16、32、64
 * ///读写64位地址的内存8、16、32、64
 * ///读写地址自增方式读写一块内存
 */

uint32_t DAP_ReadDPReg(DAPObject *dapObj, int DPReg){
	assert(dapObj->adapterObj != NULL);


}
