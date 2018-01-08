/*
 * SmartOCD
 * usb.c
 *
 *  Created on: 2018-1-5
 *  Author:  Virus.V <virusv@live.com>
 * LICENSED UNDER GPL.
 */

#ifdef HAVE_CONFIG
#include "global_config.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <setjmp.h>

#include "misc/log.h"
/*
 * 新建Debugger USB对象
 * vid: vender id
 * pid: product id
 * desc: 设备的描述
 */
USBObject * NewUSBObject(int vid, int pid, char const *desc){
	USBObject *object = calloc(sizeof(USBObject));
	if(object == NULL){
		// 内存分配失败
		log_error("Failed to make USB Object");
		return NULL;
	}
	object->deviceDesc = desc ? strdup(desc) : NULL;
	object->idProduct = pid;
	object->idVendor = vid;
	return object;
}

/*
 * 销毁USB对象
 * object: 要销毁的USB对象
 */
void FreeUSBObject(USBObject *object){
	// 关闭连接
	if(object->deviceDesc != NULL) free(object->deviceDesc);
	free(object);
}
