/*
 * USB_private.h
 *
 *  Created on: 2019-5-7
 *      Author: virusv
 */

#ifndef SRC_USB_SRC_USB_PRIVATE_H_
#define SRC_USB_SRC_USB_PRIVATE_H_

// libusb-1.0
#include <libusb-1.0/libusb.h>
// USB库的头文件
#include "USB/include/USB.h"

/*
 * usb设备私有对象
 */
struct _usb_private{
	uint16_t Vid;	// 设备制造商id
	uint16_t Pid;	// 产品id
	struct usb usbInterface;	// USB 接口
	int currConfVal;	// 当前活动配置的bConfigurationValue
	int clamedIFNum;	// 声明过的bInterfaceNumber
	uint8_t readEP, writeEP;	// 当前声明的读端点和写端点
	uint16_t readEPMaxPackSize, writeEPMaxPackSize;	// 读写端点支持的最大包长度
	char *SerialNum;	// 序列号
	libusb_context *libusbContext;	// LibUSB上下文
	libusb_device_handle *devHandle;	// 设备操作句柄
	libusb_device **devs;	// 设备列表
};

#endif /* SRC_USB_SRC_USB_PRIVATE_H_ */
