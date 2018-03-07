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
#include "smart_ocd.h"
#include "misc/log.h"
#include "debugger/adapter.h"

static BOOL init(AdapterObject *adapterObj);	// 初始化
static BOOL deinit(AdapterObject *adapterObj);	// 反初始化（告诉我怎么形容合适）
static BOOL selectTrans(AdapterObject *adapterObj, enum transportType type);	// 选择transport类型(当仿真器支持多个transport时)
static BOOL operate(AdapterObject *adapterObj, int operate, ...);	// 执行动作

// 初始化Adapter对象
BOOL InitAdapterObject(AdapterObject *object, const char *desc){
	assert(object != NULL);
	object->DeviceDesc = strdup(desc);

	// 初始化默认函数
	object->Init = init;
	object->Deinit = deinit;
	object->SelectTrans = selectTrans;
	object->Operate = operate;
	return TRUE;
}

void DeinitAdapterObject(AdapterObject *object){
	assert(object != NULL);
	if(object->DeviceDesc != NULL){
		free(object->DeviceDesc);
		object->DeviceDesc = NULL;
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
