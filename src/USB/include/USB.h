/*
 * USB.h
 *
 *  Created on: 2019-5-7
 *      Author: virusv
 */

#ifndef SRC_USB_INCLUDE_USB_H_
#define SRC_USB_INCLUDE_USB_H_

#include "smart_ocd.h"

struct usb {
	// open
	// close
	// reset
	// control
	// bulk transfer
	// interrupt transfer
	// set configuration
	// claim interface
};

// USB对象的构造和析构函数
BOOL __CONSTRUCT(USB)(USBObject *obj);
void __DESTORY(USB)(USBObject *obj);
BOOL USBOpen(USBObject *usbObj, const uint16_t vid, const uint16_t pid, const char *serial);
void USBClose(USBObject *usbObj);
int USBControlTransfer(USBObject *usbObj, uint8_t requestType, uint8_t request,
	uint16_t wValue, uint16_t wIndex, uint8_t *data, uint16_t dataLength, unsigned int timeout);
int USBBulkTransfer(USBObject *usbObj, uint8_t endpoint, uint8_t *data, int dataLength, int timeout);
int USBInterruptTransfer(USBObject *usbObj, uint8_t endpoint, uint8_t *data, int dataLength, int timeout);
BOOL USBSetConfiguration(USBObject *usbObj, int configurationIndex);
BOOL USBClaimInterface(USBObject *usbObj, int IFClass, int IFSubclass, int IFProtocol, int transType);
BOOL USBResetDevice(USBObject *usbObj);


#endif /* SRC_USB_INCLUDE_USB_H_ */
