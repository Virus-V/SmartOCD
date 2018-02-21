/*
 * adapter.h
 * SmartOCD
 *
 *  Created on: 2018-1-29
 *      Author: virusv
 */

#ifndef SRC_DEBUGGER_ADAPTER_H_
#define SRC_DEBUGGER_ADAPTER_H_

#include <stdarg.h>
#include "debugger/usb.h"

// 仿真通信方式
enum transportType {
	UNKNOW_NULL = 0,	// 未知协议，由仿真器指定
	JTAG,
	SWD,
	SWIM
};

// 仿真器的公共操作命令
enum commonInstr {
	AINS_GET_IDCODE,		// 获得IDCode
	AINS_COMM_LAST	// 分隔符，代表最后一个
};

enum connType {
	USB,	// 使用USB方式连接
	ETHERNAET,	// 以太网，Wifi
	SERIAL_PORT	// 串行口
};

// 仿真器的连接方式，比如使用USB、以太网、串口等等
// 此处先实现使用usb协议的仿真器类型
struct adapterConnector{
	enum connType type;	// 指示当前的仿真器连接类型
	int connectFlag;	// 是否已连接
	union {	// 指针共用体，指向连接对象
		USBObject *usbObj;
	} object;
};
//仿真器对象
typedef struct AdapterObject AdapterObject;
/**
 * Adapter仿真器 为上位机提供控制调试对象方法的一个中间件
 * 仿真器提供一种或多种仿真协议(Transport)支持，比如SWD、JTAG等等
 */
struct AdapterObject{
	struct adapterConnector ConnObject;	// 仿真器连接对象
	char *DeviceDesc;	// 设备描述字符
	enum transportType currTrans;	// 当前使用的仿真协议
	const enum transportType *supportedTrans;	// 支持的仿真协议列表（不能出现UNKNOW）
	BOOL (*Init)(AdapterObject *adapterObj);	// 执行初始化动作
	BOOL (*Deinit)(AdapterObject *adapterObj);	// 反初始化（告诉我怎么形容合适）
	BOOL (*SelectTrans)(AdapterObject *adapterObj, enum transportType type);	// 选择transport类型(当仿真器支持多个transport时)
	BOOL (*Operate)(AdapterObject *adapterObj, int operate, ...);	// 执行动作
};

#define GET_USBOBJ(pa) CAST(USBObject *, (pa)->ConnObject.object.usbObj)
#define SET_USBOBJ(pa,pobj) ((pa)->ConnObject.object.usbObj = CAST(USBObject *, pobj))
// 获得子类的Adapter对象
#define GET_ADAPTER(p) CAST(AdapterObject *, &(p)->AdapterObj)

AdapterObject *InitAdapterObject(AdapterObject *object, const char *desc);
void DeinitAdapterObject(AdapterObject *object);

#endif /* SRC_DEBUGGER_ADAPTER_H_ */
