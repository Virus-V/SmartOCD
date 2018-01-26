/*
 * SmartOCD
 * usb.h
 *
 *  Created on: 2018-1-5
 *  Author:  Virus.V <virusv@live.com>
 * LICENSED UNDER GPL.
 */

#ifndef SRC_DEBUGGER_USB_H_
#define SRC_DEBUGGER_USB_H_

// libusb-1.0
#include <libusb-1.0/libusb.h>
/*
 * usb设备对象
 */
typedef struct USBObjectStrcut{
	char *deviceDesc;	// 设备描述字符
	uint16_t vid;	// 设备制造商id
	uint16_t pid;	// 产品id
	char *serialNum;	// 序列号
	//TODO 增加usb描述对象指针
	libusb_context *libusbContext;	// LibUSB上下文
	libusb_device_handle *devHandle;	// 设备操作句柄
} USBObject;

#endif /* SRC_DEBUGGER_USB_H_ */
