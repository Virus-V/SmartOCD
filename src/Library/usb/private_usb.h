/**
 * src/Library/usb/private_usb.h
 * Copyright (c) 2020 Virus.V <virusv@live.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef SRC_USB_SRC_USB_PRIVATE_H_
#define SRC_USB_SRC_USB_PRIVATE_H_

// libusb-1.0
#include <libusb-1.0/libusb.h>
// USB库的头文件
#include "usb.h"

/*
 * usb设备私有对象
 */
struct _usb_private {
  uint16_t Vid;                                   // 设备制造商id
  uint16_t Pid;                                   // 产品id
  struct usb usbInterface;                        // USB 接口
  int currConfVal;                                // 当前活动配置的bConfigurationValue
  int clamedIFNum;                                // 声明过的bInterfaceNumber
  uint8_t readEP, writeEP;                        // 当前声明的读端点和写端点
  uint16_t readEPMaxPackSize, writeEPMaxPackSize; // 读写端点支持的最大包长度
  char *SerialNum;                                // 序列号
  libusb_context *libusbContext;                  // LibUSB上下文
  libusb_device_handle *devHandle;                // 设备操作句柄
  libusb_device **devs;                           // 设备列表
};

#endif /* SRC_USB_SRC_USB_PRIVATE_H_ */
