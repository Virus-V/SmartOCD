/**
 * src/Adapter/ftdi/ftdi.h
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

/**
 * 参考：https://www.ftdichip.com/Support/Documents/AppNotes/AN2232C-01_MPSSE_Cmnd.pdf
 * 参考：http://developer.intra2net.com/mailarchive/html/libftdi/2009/msg00292.html
 */

#ifndef SRC_ADAPTER_FTDI_FTDI_H_
#define SRC_ADAPTER_FTDI_FTDI_H_

#include "smartocd.h"

#include "Adapter/adapter_private.h"
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
 * 	serialNum:设备序列号
 * 返回:
 */
int ConnectFtdi(IN Adapter self, IN const uint16_t *vids, IN const uint16_t *pids,
                IN const char *serialNum);

/**
 * DisconnectFtdi - 断开FTDI设备
 *
 */
int DisconnectFtdi(IN Adapter self);

/**
 * FtdiSetInterface - 选择FTDI的接口
 * 参数：
 *  interfaceNum：FTDI的接口
 *  mode：工作模式
 */
int FtdiSetInterface(IN Adapter self, IN int interfaceNum, IN int mode);

#endif /* SRC_ADAPTER_FTDI_FTDI_H_ */
