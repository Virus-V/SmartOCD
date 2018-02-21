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
typedef struct USBObjectStrcut USBObject;
struct USBObjectStrcut{
	uint16_t Vid;	// 设备制造商id
	uint16_t Pid;	// 产品id
	int currConfVal;	// 当前活动配置的bConfigurationValue
	int clamedIFNum;	// 声明过的bInterfaceNumber
	uint8_t readEP, writeEP;	// 当前声明的读端点和写端点
	uint16_t readEPMaxPackSize, writeEPMaxPackSize;	// 读写端点支持的最大包长度
	char *SerialNum;	// 序列号
	libusb_context *libusbContext;	// LibUSB上下文
	libusb_device_handle *devHandle;	// 设备操作句柄
	int (*Read)(USBObject *usbObj, uint8_t *data, int dataLength, int timeout),	// 读取数据
		(*Write)(USBObject *usbObj, uint8_t *data, int dataLength, int timeout);	// 写入数据

};

USBObject * NewUSBObject();
void FreeUSBObject(USBObject *object);
BOOL USBOpen(USBObject *usbObj, const uint16_t vid, const uint16_t pid, const char *serial);
void USBClose(USBObject *usbObj);
int USBControlTransfer(USBObject *usbObj, uint8_t requestType, uint8_t request,
	uint16_t wValue, uint16_t wIndex, uint8_t *data, uint16_t dataLength, unsigned int timeout);
int USBBulkTransfer(USBObject *usbObj, uint8_t endpoint, uint8_t *data, int dataLength, int timeout);
int USBInterruptTransfer(USBObject *usbObj, uint8_t endpoint, uint8_t *data, int dataLength, int timeout);
BOOL USBSetConfiguration(USBObject *usbObj, int configurationIndex);
BOOL USBClaimInterface(USBObject *usbObj, int IFClass, int IFSubclass, int IFProtocol, int transType);
BOOL USBGetPidVid(libusb_device *dev, uint16_t *pid_out, uint16_t *vid_out);
#endif /* SRC_DEBUGGER_USB_H_ */
