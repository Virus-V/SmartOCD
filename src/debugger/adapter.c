/*
 * adapter.c
 *
 *  Created on: 2018-2-18
 *      Author: virusv
 */

#ifdef HAVE_CONFIG
#include "global_config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "misc/log.h"
#include "debugger/adapter.h"

static BOOL init(AdapterObject *adapterObj);	// 初始化
static BOOL deinit(AdapterObject *adapterObj);	// 反初始化（告诉我怎么形容合适）
static BOOL selectTrans(AdapterObject *adapterObj, enum transportType type);	// 选择transport类型(当仿真器支持多个transport时)
static BOOL operate(AdapterObject *adapterObj, int operate, ...);	// 执行动作

// 初始化Adapter对象
BOOL __CONSTRUCT(Adapter)(AdapterObject *adapterObj, const char *desc){
	assert(adapterObj != NULL);
	adapterObj->DeviceDesc = strdup(desc);

	// 初始化默认函数
	adapterObj->Init = init;
	adapterObj->Deinit = deinit;
	adapterObj->SelectTrans = selectTrans;
	adapterObj->Operate = operate;
	return TRUE;
}

void __DESTORY(Adapter)(AdapterObject *adapterObj){
	assert(adapterObj != NULL);
	if(adapterObj->DeviceDesc != NULL){
		free(adapterObj->DeviceDesc);
		adapterObj->DeviceDesc = NULL;
	}
}

// 默认的初始化函数
static BOOL init(AdapterObject *adapterObj){
	(void)adapterObj;
	log_warn("Tried to call undefined function: init.");
	return FALSE;
}

static BOOL deinit(AdapterObject *adapterObj){
	(void)adapterObj;
	log_warn("Tried to call undefined function: deinit.");
	return FALSE;
}

static BOOL selectTrans(AdapterObject *adapterObj, enum transportType type){
	(void)adapterObj;
	(void)type;
	log_warn("Tried to call undefined function: selectTrans.");
	return FALSE;
}

/**
 * 注意可变参数的数据类型提升
 * float类型的实际参数将提升到double
 * char、short和相应的signed、unsigned类型的实际参数提升到int
 * 如果int不能存储原值，则提升到unsigned int
 */
static BOOL operate(AdapterObject *adapterObj, int operate, ...){
	(void)adapterObj; (void)operate;
	log_warn("Tried to call undefined function: operate.");
	return FALSE;
}

/**
 * 设置仿真器通信频率（Hz）
 */
BOOL adapter_SetClock(AdapterObject *adapterObj, uint32_t clockHz){
	assert(adapterObj != NULL);
	if(adapterObj->Operate(adapterObj, AINS_SET_CLOCK, clockHz) == FALSE){
		log_warn("Failed to set adapter frequency.");
		return FALSE;
	}
	return TRUE;
}

/**
 * 切换通信模式
 * SWD和JTAG等等
 */
BOOL adapter_SelectTransmission(AdapterObject *adapterObj, enum transportType type){
	assert(adapterObj != NULL);
	if(adapterObj->SelectTrans(adapterObj, type) == FALSE){
		log_warn("%s select %s Mode Failed.", adapterObj->DeviceDesc, adapter_Transport2Str(type));
		return FALSE;
	}
	return TRUE;
}

/**
 * 判断当前adapter是否支持某一个传输方式
 */
BOOL adapter_HaveTransmission(AdapterObject *adapterObj, enum transportType type){
	assert(adapterObj != NULL && adapterObj->supportedTrans != NULL);
	if(type == UNKNOW_NULL) return FALSE;
	int idx;
	for(idx=0; adapterObj->supportedTrans[idx] != UNKNOW_NULL; idx++){
		if(adapterObj->supportedTrans[idx] == type) return TRUE;
	}
	return FALSE;
}

/**
 * 获得Transport协议字符串
 */
const char *adapter_Transport2Str(enum transportType type){
#define X(_s) if (type == _s) return #_s
	X(JTAG);
	X(SWD);
#undef X
	return "UNKOWN_TRANSPORT";
}
