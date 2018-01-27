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
	int currConfVal;	// 当前活动配置的bConfigurationValue
	int clamedIFNum;	// 声明过的bInterfaceNumber
	uint8_t readEP, writeEP;	// 当前声明的读端点和写端点
	char *serialNum;	// 序列号
	libusb_context *libusbContext;	// LibUSB上下文
	libusb_device_handle *devHandle;	// 设备操作句柄
} USBObject;

USBObject * NewUSBObject(char const *desc);
void FreeUSBObject(USBObject *object);
BOOL USBOpen(USBObject *usbObj, const uint16_t vid, const uint16_t pid, const char *serial);
void USBClose(USBObject *usbObj);
int USBControlTransfer(USBObject *usbObj, uint8_t requestType, uint8_t request,
	uint16_t wValue, uint16_t wIndex, char *data, uint16_t dataLength, unsigned int timeout);
int USBBulkTransfer(USBObject *usbObj, int endpoint, char *data, int dataLength, int timeout);
int USBInterruptTransfer(USBObject *usbObj, int endpoint, char *data, int dataLength, int timeout);
BOOL USBSetConfiguration(USBObject *usbObj, int configurationIndex);
BOOL USBClaimInterface(USBObject *usbObj, unsigned int *ReadEndPoint_out, unsigned int *WriteEndPoint_out,
	int IFClass, int IFSubclass, int IFProtocol, int transType);
BOOL USBGetPidVid(libusb_device *dev, uint16_t *pid_out, uint16_t *vid_out);
#endif /* SRC_DEBUGGER_USB_H_ */