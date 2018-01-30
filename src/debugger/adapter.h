/*
 * adapter.h
 * SmartOCD
 *
 *  Created on: 2018-1-29
 *      Author: virusv
 */

#ifndef SRC_DEBUGGER_ADAPTER_H_
#define SRC_DEBUGGER_ADAPTER_H_

#include "debugger/usb.h"
#include "debugger/transport.h"

enum connType {
	USB,	// 使用USB方式连接
	ETHERNAET,	// 以太网，Wifi
	SERIAL_PORT	// 串行口
};

// 仿真器的连接方式，比如使用USB、以太网、串口等等
// 此处先实现使用usb协议的仿真器类型
struct adapterConnector{
	enum connType type;	// 指示当前的仿真器连接类型
	union {	// 指针共用体，指向连接对象
		USBObject *usbObj;
	} object;
};
//仿真器对象
typedef struct AdapterObject Adapter;
/**
 * Adapter仿真器 为上位机提供控制调试对象方法的一个中间件
 * 仿真器提供一种或多种仿真协议(Transport)支持，比如SWD、JTAG等等
 */
struct AdapterObject{
	struct adapterConnector connObject;	// 仿真器连接对象
	char *deviceDesc;	// 设备描述字符
	enum transportType currTrans;	// 当前使用的仿真协议
	enum transportType supportedTrans[];	// 支持的仿真协议列表（不能出现UNKNOW）
	BOOL (*init)(Adapter *adapterObj);	// 初始化
	BOOL (*deinit)(Adapter *adapterObj);	// 反初始化（告诉我怎么形容合适）
	BOOL (*selectTrans)(Adapter *adapterObj, enum transportType type);	// 选择transport类型(当仿真器支持多个transport时)
	//增加仿真器其他接口，比如写入
};
#endif /* SRC_DEBUGGER_ADAPTER_H_ */
