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

/**
 * 参考：https://www.ftdichip.com/Support/Documents/AppNotes/AN2232C-01_MPSSE_Cmnd.pdf
 * 参考：http://developer.intra2net.com/mailarchive/html/libftdi/2009/msg00292.html
 */

#ifndef SRC_ADAPTER_FTDI_FTDI_H_
#define SRC_ADAPTER_FTDI_FTDI_H_

#include "smartocd.h"

#include "Adapter/adapter_jtag.h"

/**
 * 创建FTDI对象
 * 返回:
 * 	Adapter对象
 */
Adapter CreateFtdi(void);

/**
 * 销毁FTDI对象
 * 参数:
 * 	self:自身对象的指针!
 */
void DestroyFtdi(IN Adapter *self);

/**
 * ConnectFtdi - 连接FTDI仿真器
 * 参数:
 * 	vids: Vendor ID 列表
 * 	pids: Product ID 列表
 * 	serialNum:设备序列号,可以为NULL
 * 	channel:FTDI的channel
 * 返回:
 */
int ConnectFtdi(IN Adapter self, IN const uint16_t *vids, IN const uint16_t *pids,
                IN const char *serialNum, IN int channel);

/**
 * DisconnectFtdi - 断开FTDI设备
 *
 */
int DisconnectFtdi(IN Adapter self);

#if 0
/**
 * FtdiSetLayout - 选择channel的状态
 * 参数：
 *  out：默认输出电平
 *  mode：工作模式
 */
int FtdiSetLayout(IN Adapter self, IN int out, IN int mode);
#endif

#endif /* SRC_ADAPTER_FTDI_FTDI_H_ */
