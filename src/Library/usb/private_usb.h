/**
 * Copyright (c) 2023, Virus.V <virusv@live.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * * Neither the name of SmartOCD nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * Copyright 2023 Virus.V <virusv@live.com>
 */

#ifndef SRC_USB_SRC_USB_PRIVATE_H_
#define SRC_USB_SRC_USB_PRIVATE_H_

#if defined(__FreeBSD__)
#include <libusb.h>
#elif defined(__linux__)
#include <libusb-1.0/libusb.h>
#endif

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
