/*
 * JTAG.c
 *
 *  Created on: 2018-3-24
 *      Author: virusv
 */

#ifdef HAVE_CONFIG
#include "global_config.h"
#endif

#include "smart_ocd.h"
#include "misc/log.h"
#include "arch/common/JTAG.h"

/**
 * TAP复位
 * hard:TRUE：使用TRST引脚进行TAP复位；FALSE：使用连续5个脉冲宽度的TMS高电平进行TAP复位
 * pinWait：死区时间，只有在hard参数为TRUE时有效
 */
BOOL JTAG_TAP_Reset(AdapterObject *adapterObj, BOOL hard, uint32_t pinWait){
	assert(adapterObj != NULL);
	if(adapterObj->SelectTrans(adapterObj, JTAG) == FALSE){
		log_error("%s select JTAG Mode Failed.", adapterObj->DeviceDesc);
		return FALSE;
	}
	if(hard){	// assert nTRST引脚等待pinWait µs，最大不超过3s
		return adapterObj->Operate(adapterObj, AINS_JTAG_PINS, 0x1<<5, 0x1<<5, NULL, pinWait);
	}else{	// 发送6个时钟宽度TMS高电平(其实5个就够了，多一个有保证吧，心理作用)
		uint8_t request[]= {0x46, 0x00};
		return adapterObj->Operate(adapterObj, AINS_JTAG_SEQUENCE, 1, request, NULL);
	}
}



