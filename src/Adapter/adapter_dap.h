/**
 * src/Adapter/adapter_dap.h
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
 * DapCleanPending - 清除pending的动作
 * 参数:
 * 	self:DapSkill对象自身
 * 返回:
 * 	ADPT_SUCCESS:成功
 * 	ADPT_FAILED:失败
 * 	或者其他错误
 */
typedef int (*SKILL_DAP_CLEAN_PENDING)(IN DapSkill self);

/* DAP 能力集 */
struct dapSkill {
  struct skill header;

  SKILL_DAP_SINGLE_READ DapSingleRead;     // 单次读:AP或者DP,寄存器编号
  SKILL_DAP_SINGLE_WRITE DapSingleWrite;   // 单次写:AP或者DP,寄存器编号
  SKILL_DAP_MULTI_READ DapMultiRead;       // 连续读
  SKILL_DAP_MULTI_WRITE DapMultiWrite;     // 连续写
  SKILL_DAP_COMMIT DapCommit;              // 提交Pending动作
  SKILL_DAP_CLEAN_PENDING DapCleanPending; // 清除Pending的动作
};

/* 获得Adapter DAP能力接口 */
#define ADAPTER_GET_DAP_SKILL(adapter) (CAST(DapSkill, Adapter_GetSkill((adapter), ADPT_SKILL_DAP)))

#endif
