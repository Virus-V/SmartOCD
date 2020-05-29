/***
 * @Author: Virus.V
 * @Date: 2020-05-25 14:11:03
 * @LastEditors: Virus.V
 * @LastEditTime: 2020-05-25 14:11:04
 * @Description: DAP Highlevel Transport API based on SWD
 * @Email: virusv@live.com
 */

#ifndef SRC_ADAPTER_ADAPTER_DAP_H_
#define SRC_ADAPTER_ADAPTER_DAP_H_

#include "smartocd.h"
#include "Adapter/adapter.h"

/* DAP寄存器类型 */
enum dapRegType {
  ADPT_DAP_DP_REG, // DP类型的寄存器
  ADPT_DAP_AP_REG, // AP类型的寄存器
};

/**
 * DapSingleRead - 单次读:AP或者DP,寄存器地址
 * 会将该动作加入Pending队列,不会立即执行
 * 参数:
 * 	self:Adapter对象自身
 * 	type:寄存器类型,DP还是AP
 * 	reg:reg地址
 * 	data:将寄存器的内容写入到该参数指定的地址
 * 返回:
 */
typedef int (*ADPT_DAP_SINGLE_READ)(IN Adapter self, IN enum dapRegType type,
                                    IN int reg, OUT uint32_t *data);

/**
 * DapSingleWrite - 单次写:AP或者DP,寄存器地址
 * 会将该动作加入Pending队列,不会立即执行
 * 参数:
 * 	self:Adapter对象自身
 * 	type:寄存器类型,DP还是AP
 * 	reg:reg地址
 * 	data:将该参数的数据写入到寄存器
 * 返回:
 */
typedef int (*ADPT_DAP_SINGLE_WRITE)(IN Adapter self, IN enum dapRegType type,
                                     IN int reg, IN uint32_t data);

/**
 * DapMultiRead - 多次读:AP或者DP,寄存器地址
 * 会将该动作加入Pending队列,不会立即执行
 * 参数:
 * 	self:Adapter对象自身
 * 	type:寄存器类型,DP还是AP
 * 	reg:reg地址
 * 	count:读取的次数
 * 	data:将寄存器的内容写入到该参数指定的数组
 * 返回:
 */
typedef int (*ADPT_DAP_MULTI_READ)(IN Adapter self, IN enum dapRegType type,
                                   IN int reg, IN int count,
                                   OUT uint32_t *data);

/**
 * DapMultiWrite - 多次写:AP或者DP,寄存器地址
 * 会将该动作加入Pending队列,不会立即执行
 * 参数:
 * 	self:Adapter对象自身
 * 	type:寄存器类型,DP还是AP
 * 	reg:reg地址
 * 	count:读取的次数
 * 	data:将该参数指定数组的数据写入到寄存器
 * 返回:
 */
typedef int (*ADPT_DAP_MULTI_WRITE)(IN Adapter self, IN enum dapRegType type,
                                    IN int reg, IN int count,
                                    IN uint32_t *data);

/**
 * DapCommit - 提交Pending的动作
 * 参数:
 * 	self:Adapter对象自身
 * 返回:
 * 	ADPT_SUCCESS:成功
 * 	ADPT_FAILED:失败
 * 	或者其他错误
 */
typedef int (*ADPT_DAP_COMMIT)(IN Adapter self);

/**
 * DapCleanPending - 清除pending的动作
 * 参数:
 * 	self:Adapter对象自身
 * 返回:
 * 	ADPT_SUCCESS:成功
 * 	ADPT_FAILED:失败
 * 	或者其他错误
 */
typedef int (*ADPT_DAP_CLEAN_PENDING)(IN Adapter self);

/* DAP 能力集 */
struct dapCapability {
  struct capability header;

  ADPT_DAP_SINGLE_READ DapSingleRead;   // 单次读:AP或者DP,寄存器编号
  ADPT_DAP_SINGLE_WRITE DapSingleWrite; // 单次写:AP或者DP,寄存器编号
  ADPT_DAP_MULTI_READ DapMultiRead;     // 连续读
  ADPT_DAP_MULTI_WRITE DapMultiWrite;   // 连续写
  ADPT_DAP_COMMIT DapCommit;            // 提交Pending动作
  ADPT_DAP_CLEAN_PENDING DapCleanPending; // 清除Pending的动作
};

#endif
