/*
 * USB.h
 *
 *  Created on: 2019-5-7
 *      Author: virusv
 */

#ifndef SRC_USB_INCLUDE_USB_H_
#define SRC_USB_INCLUDE_USB_H_

#include "smart_ocd.h"

// USB 接口状态值
enum{
	USB_SUCCESS,	// 成功
	USB_FAILED,	// 失败
	USB_BAD_PARAMETER,	// 非法参数
	USB_ERR_NOT_FOUND,	// 未找到设备
	USB_ERR_INTERNAL_ERROR,	// USB库内部错误
	USB_ERR_UNSUPPORT,	// 不支持的操作
	USB_ERR_MAX
};

typedef struct usb *USB;

/**
 * Open - 打开一个USB设备
 * 参数：
 * 	self：当前USB接口对象
 * 	vid：USB设备的Vendor ID
 * 	pid：USB设备的Production ID
 * 	serial：USB设备的序列号，可以为NULL
 * 返回：
 *  USB_SUCCESS：成功
 *  USB_ERR_NOT_FOUND：未找到指定设备
 *  USB_ERR_INTERNAL_ERROR：内部错误
 */
typedef int (*USB_OPEN)(
		IN USB self,
		IN uint16_t vid,
		IN uint16_t pid,
		OPTIONAL const char *serial
);

/**
 * Close - 关闭USB设备
 * 参数:
 *	self:当前USB接口对象
 */
typedef void (*USB_CLOSE)(
		IN USB self
);

/**
 * Reset - 复位USB设备
 * 参数:
 * 	self:当前USB接口对象
 * 返回:
 * 	USB_SUCCESS:复位成功
 * 	USB_ERR_INTERNAL_ERROR:内部错误
 */
typedef int (*USB_RESET)(
		IN USB self
);

/**
 * ControlTransfer - USB控制传输
 * 参数:
 * 	self:当前USB接口对象
 * 	requestType:SETUP包的bmRequestType字段
 * 	request:SETUP包的bRequest字段
 * 	wValue:SETUP包的bRequest字段
 * 	wIndex:SETUP包的bRequest字段
 * 	data:SETUP包接下来传输的内容缓冲区,收发
 * 	dataLength:缓冲区大小
 *	timeout:等待超时
 *	count:实际传输多少字节
 * 返回:
 * 	USB_SUCCESS:操作成功
 * 	USB_ERR_INTERNAL_ERROR:内部错误
 */
typedef int (*USB_CONTROL_TRANSFER)(
		IN USB self,
		IN uint8_t requestType,
		IN uint8_t request,
		IN uint16_t wValue,
		IN uint16_t wIndex,
		IN unsigned char *data,
		IN uint16_t dataLength,
		IN unsigned int timeout,
		OUT int *count
);

/**
 * BulkTransfer - USB Bulk 传输类型
 * 参数:
 * 	self:当前USB接口对象
 * 	endpoint:端点号
 * 	data:数据缓冲区
 * 	dataLength:数据缓冲区长度
 * 	timeout:等待超时时间
 * 	transferred:实际传输字节数
 * 返回:
 * 	USB_SUCCESS:操作成功
 * 	USB_ERR_INTERNAL_ERROR:内部错误
 */
typedef int (*USB_BULK_TRANSFER)(
		IN USB self,
		IN uint8_t endpoint,
		IN unsigned char *data,
		IN int dataLength,
		IN int timeout,
		OUT int *transferred
);

/**
 * InterruptTransfer - 中断传输
 * 参数:
 * 	self:当前USB接口对象
 * 	endpoint:端点号
 * 	data:数据缓冲区
 * 	dataLength:数据缓冲区长度
 * 	timeout:等待超时时间
 * 	transferred:实际传输字节数
 * 返回:
 * 	USB_SUCCESS:操作成功
 * 	USB_ERR_INTERNAL_ERROR:内部错误
 */
typedef int (*USB_INTERRUPT_TRANSFER)(
		IN USB self,
		IN uint8_t endpoint,
		IN unsigned char *data,
		IN int dataLength,
		IN int timeout,
		OUT int *transferred
);

/**
 * SetConfiguration - 激活配置
 * 参数:
 * 	self:当前USB接口对象
 * 	configurationIndex:配置编号
 * 返回:
 * 	USB_SUCCESS:操作成功
 * 	USB_ERR_INTERNAL_ERROR:内部错误
 */
typedef int (*USB_SET_CONFIGURATION)(
		IN USB self,
		IN uint8_t configurationIndex
);

/**
 * ClaimInterface - 声明接口
 * 参数:
 * 	self:当前USB接口对象
 * 	IFClass:接口的类别码
 * 	IFSubclass:接口子类别码
 * 	IFProtocol:接口协议码
 * 	transType:传输类型，最低两位表示传输类型：
 * 		0为控制传输
 * 		1为等时传输
 * 		2为批量传输
 * 		3为中断传输
 * 返回:
 * 	USB_SUCCESS:操作成功
 * 	USB_BAD_PARAMETER:参数无效,请先激活配置
 * 	USB_ERR_INTERNAL_ERROR:内部错误
 * 	USB_ERR_NOT_FOUND:未找到相关接口
 */
typedef int (*USB_CLAIM_INTERFACE)(
		IN USB self,
		IN uint8_t IFClass,
		IN uint8_t IFSubclass,
		IN uint8_t IFProtocol,
		IN uint8_t transType
);

/**
 * Read and Write - 从当前活动端点读写数据
 * 参数:
 * 	self:当前USB接口对象
 * 	data:数据缓冲区
 * 	dataLength:数据缓冲区长度
 * 	timeout:等待超时时间
 * 	transferred:实际传输字节数
 * 返回:
 * 	USB_SUCCESS:操作成功
 * 	USB_ERR_INTERNAL_ERROR:内部错误
 */
typedef int (*USB_READ_WRITE)(
		IN USB self,
		IN unsigned char *data,
		IN int dataLength,
		IN int timeout,
		OUT int *transferred
);

/**
 * USB接口定义结构体
 */
struct usb {
	USB_OPEN Open;
	USB_CLOSE Close;
	USB_RESET Reset;
	USB_CONTROL_TRANSFER ControlTransfer;
	USB_BULK_TRANSFER BulkTransfer;
	USB_INTERRUPT_TRANSFER InterruptTransfer;
	USB_SET_CONFIGURATION SetConfiguration;
	USB_CLAIM_INTERFACE ClaimInterface;
	USB_READ_WRITE Read;
	USB_READ_WRITE Write;
};

/**
 * CreateUSB - 创建USB对象
 */
USB CreateUSB(void);

/**
 * DestoryUSB - 销毁USB对象
 */
void DestoryUSB(USB *self);

#endif /* SRC_USB_INCLUDE_USB_H_ */
