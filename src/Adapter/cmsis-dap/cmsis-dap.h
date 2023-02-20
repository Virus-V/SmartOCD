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

#ifndef SRC_ADAPTER_CMSIS_DAP_CMSIS_DAP_H_
#define SRC_ADAPTER_CMSIS_DAP_CMSIS_DAP_H_

#include "smartocd.h"

#include "Adapter/adapter_dap.h"
#include "Adapter/adapter_jtag.h"

/**
 * 创建CMSIS-DAP对象
 * 返回:
 * 	Adapter对象
 */
Adapter CreateCmsisDap(void);

/**
 * 销毁CMSIS-DAP对象
 * 参数:
 * 	self:自身对象的指针!
 */
void DestroyCmsisDap(IN Adapter *self);

/**
 * ConnectCmsisDap - 连接CMSIS-DAP仿真器
 * 参数:
 * 	vids: Vendor ID 列表
 * 	pids: Product ID 列表
 * 	serialNum:设备序列号
 * 返回:
 */
int ConnectCmsisDap(IN Adapter self, IN const uint16_t *vids, IN const uint16_t *pids,
                    IN const char *serialNum);

/**
 * DisconnectCmsisDap - 断开CMSIS-DAP设备
 *
 */
int DisconnectCmsisDap(IN Adapter self);

/**
 * 设置传输参数
 * 在调用DapTransfer和DapTransferBlock之前要先调用该函数
 * idleCycle：每一次传输后面附加的空闲时钟周期数
 * waitRetry：如果收到WAIT响应，重试的次数
 * matchRetry：如果在匹配模式下发现值不匹配，重试的次数
 * SWD、JTAG模式下均有效
 */
int CmdapTransferConfigure(IN Adapter self, IN uint8_t idleCycle, IN uint16_t waitRetry,
                           IN uint16_t matchRetry);

/**
 * 设置JTAG信息
 * count：扫描链中TAP的个数，不超过8个
 * irData：每个TAP的IR寄存器长度
 */
int CmdapJtagConfig(IN Adapter self, IN uint8_t count, IN uint8_t *irData);

/**
 * 配置SWD
 */
int CmdapSwdConfig(IN Adapter self, IN uint8_t cfg);

/**
 * DAP写ABORT寄存器
 * The DAP_WriteABORT Command writes an abort request
 * to the CoreSight ABORT register of the Target Device.
 */
int CmdapWriteAbort(IN Adapter self, IN uint32_t data);

/**
 * 选中DAP模式下JTAG扫描链中的TAP对象
 * index:不得大于CmdapJtagConfig中设置的个数
 */
int CmdapSetTapIndex(IN Adapter self, IN unsigned int index);

#endif /* SRC_ADAPTER_CMSIS_DAP_CMSIS_DAP_H_ */
