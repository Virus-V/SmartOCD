/**
 * src/Adapter/cmsis-dap/cmsis-dap.h
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

#ifndef SRC_ADAPTER_CMSIS_DAP_CMSIS_DAP_H_
#define SRC_ADAPTER_CMSIS_DAP_CMSIS_DAP_H_

#include "smartocd.h"

#include "Adapter/adapter_private.h"

#include "Adapter/adapter_dap.h"
#include "Adapter/adapter_jtag.h"
#include "Library/usb/usb.h"

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
