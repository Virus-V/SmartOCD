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

#ifndef SRC_ADAPTER_ADAPTER_DAP_H_
#define SRC_ADAPTER_ADAPTER_DAP_H_

#include "smartocd.h"

#include "Adapter/adapter.h"

/* DAP寄存器类型 */
enum dapRegType {
  SKILL_DAP_DP_REG, // DP类型的寄存器
  SKILL_DAP_AP_REG, // AP类型的寄存器
};

/* DAP 能力集对象 */
typedef const struct dapSkill *DapSkill;

/**
 * DapSingleRead - 单次读:AP或者DP,寄存器地址
 * 会将该动作加入Pending队列,不会立即执行
 * 参数:
 * 	self:DapSkill对象自身
 * 	type:寄存器类型,DP还是AP
 * 	reg:reg地址
 * 	data:将寄存器的内容写入到该参数指定的地址
 * 返回:
 */
typedef int (*SKILL_DAP_SINGLE_READ)(IN DapSkill self, IN enum dapRegType type, IN int reg,
                                     OUT uint32_t *data);

/**
 * DapSingleWrite - 单次写:AP或者DP,寄存器地址
 * 会将该动作加入Pending队列,不会立即执行
 * 参数:
 * 	self:DapSkill对象自身
 * 	type:寄存器类型,DP还是AP
 * 	reg:reg地址
 * 	data:将该参数的数据写入到寄存器
 * 返回:
 */
typedef int (*SKILL_DAP_SINGLE_WRITE)(IN DapSkill self, IN enum dapRegType type, IN int reg,
                                      IN uint32_t data);

/**
 * DapMultiRead - 多次读:AP或者DP,寄存器地址
 * 会将该动作加入Pending队列,不会立即执行
 * 参数:
 * 	self:DapSkill对象自身
 * 	type:寄存器类型,DP还是AP
 * 	reg:reg地址
 * 	count:读取的次数
 * 	data:将寄存器的内容写入到该参数指定的数组
 * 返回:
 */
typedef int (*SKILL_DAP_MULTI_READ)(IN DapSkill self, IN enum dapRegType type, IN int reg,
                                    IN int count, OUT uint32_t *data);

/**
 * DapMultiWrite - 多次写:AP或者DP,寄存器地址
 * 会将该动作加入Pending队列,不会立即执行
 * 参数:
 * 	self:DapSkill对象自身
 * 	type:寄存器类型,DP还是AP
 * 	reg:reg地址
 * 	count:读取的次数
 * 	data:将该参数指定数组的数据写入到寄存器
 * 返回:
 */
typedef int (*SKILL_DAP_MULTI_WRITE)(IN DapSkill self, IN enum dapRegType type, IN int reg,
                                     IN int count, IN uint32_t *data);

/**
 * DapCommit - 提交Pending的动作
 * 参数:
 * 	self:DapSkill对象自身
 * 返回:
 * 	ADPT_SUCCESS:成功
 * 	ADPT_FAILED:失败
 * 	或者其他错误
 */
typedef int (*SKILL_DAP_COMMIT)(IN DapSkill self);

/**
 * DapCancel - 清除pending的动作
 * 参数:
 * 	self:DapSkill对象自身
 * 返回:
 * 	ADPT_SUCCESS:成功
 * 	ADPT_FAILED:失败
 * 	或者其他错误
 */
typedef int (*SKILL_DAP_CANCEL)(IN DapSkill self);

/* DAP 能力集 */
struct dapSkill {
  struct skill header;

  SKILL_DAP_SINGLE_READ SingleRead;   // 单次读:AP或者DP,寄存器编号
  SKILL_DAP_SINGLE_WRITE SingleWrite; // 单次写:AP或者DP,寄存器编号
  SKILL_DAP_MULTI_READ MultiRead;     // 连续读
  SKILL_DAP_MULTI_WRITE MultiWrite;   // 连续写
  SKILL_DAP_COMMIT Commit;            // 提交Pending动作
  SKILL_DAP_CANCEL Cancel;            // 清除Pending的动作
};

/* 获得Adapter DAP能力接口 */
#define ADAPTER_GET_DAP_SKILL(adapter) (CAST(DapSkill, Adapter_GetSkill((adapter), ADPT_SKILL_DAP)))

#endif
