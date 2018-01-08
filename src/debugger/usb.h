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

/*
 * usb设备对象
 */
typedef struct USBObjectStrcut{
	int idVendor;	// 设备制造商id
	int idProduct;	// 产品id
	char *deviceDesc;	// 设备描述字符
	//TODO 增加usb描述对象指针
} USBObject;

#endif /* SRC_DEBUGGER_USB_H_ */
