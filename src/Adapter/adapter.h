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

#ifndef SRC_ADAPTER_INCLUDE_ADAPTER_H_
#define SRC_ADAPTER_INCLUDE_ADAPTER_H_

#include "Library/misc/list.h"
#include "smartocd.h"

/**
 * Adapter 接口状态码
 */
enum {
  ADPT_SUCCESS = 0,          // 成功
  ADPT_FAILED,               // 失败
  ADPT_ERR_TRANSPORT_ERROR,  // 底层传输错误
  ADPT_ERR_PROTOCOL_ERROR,   // 传输协议错误
  ADPT_ERR_NO_DEVICE,        // 找不到设备
  ADPT_ERR_UNSUPPORT,        // 不支持的操作
  ADPT_ERR_INTERNAL_ERROR,   // 内部错误,不是由于Adapter功能部分造成的失败
  ADPT_ERR_BAD_PARAMETER,    // 无效的参数
  ADPT_ERR_DEVICE_NOT_MATCH, // 设备类型不匹配
};

/* 仿真器对象 */
typedef const struct adapter *Adapter;

// 仿真器的状态
enum adapterStatus {
  ADPT_STATUS_CONNECTED,  // Adapter已连接
  ADPT_STATUS_DISCONNECT, // Adapter已断开
  ADPT_STATUS_RUNING,     // Adapter正在运行
  ADPT_STATUS_IDLE        // Adapter空闲
};

/**
 * Status - 设置仿真器状态
 * 如果仿真器有状态指示灯的话,则可以通过这个状态来控制这些指示灯
 * 参数:
 * 	self:Adapter对象自身
 * 	status:仿真器的状态
 * 返回:
 * 	ADPT_SUCCESS:成功
 * 	ADPT_FAILED:失败
 * 	或者其他错误
 */
typedef int (*ADPT_SET_STATUS)(IN Adapter self, IN enum adapterStatus status);

/**
 * SetFrequency - 设置仿真器的工作频率
 * 参数:
 * 	self:Adapter对象自身
 * 	clock:工作频率(Hz)
 * 返回:
 * 	ADPT_SUCCESS:成功
 * 	ADPT_FAILED:失败
 * 	或者其他错误
 */
typedef int (*ADPT_SET_FREQUENCY)(IN Adapter self, IN unsigned int freq);

/**
 * target复位类型
 */
enum targetResetType {
  ADPT_RESET_SYSTEM, // 全部系统复位
  ADPT_RESET_DEBUG,  // 调试系统复位
};

/**
 * Reset - 仿真器复位
 * 参数:
 * 	self:Adapter对象自身
 * 	type:目标复位类型
 * 返回:
 * 	ADPT_SUCCESS:成功
 * 	ADPT_FAILED:失败
 * 	或者其他错误
 */
typedef int (*ADPT_RESET)(IN Adapter self, IN enum targetResetType type);

/**
 * 仿真器传输模式
 */
enum transferMode {
  // Joint Test Action Group，联合测试行动组
  ADPT_MODE_JTAG,
  // Serial wire Debug 串行调试
  ADPT_MODE_SWD,
  // TODO 其他传输模式,SWIM,UART,TCP等
  ADPT_MODE_MAX
};

/**
 * 仿真器能力类型
 */
enum skillType {
  // CMSIS-DAP或者STLINK等针对ARM ADI的仿真器
  ADPT_SKILL_DAP,
  // 传统仿真器的JTAG接口
  ADPT_SKILL_JTAG,
  ADPT_SKILL_MAX
};
/**
 * 仿真器能力集
 * 设备所支持的能力
 */
struct skill {
  enum skillType type;     // 能力集类型
  struct list_head skills; // 传输模式链表
};

/**
 * SetTransferMode - 设置传输类型:DAP还是JTAG
 * 参数:
 *	self:Adapter对象自身
 *	mode:仿真器工作模式,DAP还是JTAG
 * 返回:
 * 	ADPT_SUCCESS:成功
 * 	ADPT_FAILED:失败
 * 	或者其他错误
 */
typedef int (*ADPT_SET_TRANSFER_MODE)(IN Adapter self, IN enum transferMode mode);

/**
 * Adapter接口对象
 */
struct adapter {
  /* 属性 */
  enum adapterStatus currStatus;   // 当前状态
  unsigned int currFrequency;      // 当前频率
  enum transferMode currTransMode; // 当前传输方式
  struct list_head skills;         // 设备支持的能力集列表

  /* 服务 */
  ADPT_SET_STATUS SetStatus;              // 仿真器状态指示
  ADPT_SET_FREQUENCY SetFrequency;        // 设置仿真器的工作频率
  ADPT_RESET Reset;                       // 仿真器复位
  ADPT_SET_TRANSFER_MODE SetTransferMode; // 设置传输类型:DAP还是JTAG
};

/**
 * 获取设备能力集
 * 参数:
 *  adapter:Adapter对象
 *  type:Skill类型
 **/
const struct skill *Adapter_GetSkill(Adapter adapter, enum skillType type);
#endif /* SRC_ADAPTER_ADAPTER_H_ */
