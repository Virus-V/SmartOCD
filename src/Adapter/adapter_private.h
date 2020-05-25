/***
 * @Author: Virus.V
 * @Date: 2018-01-29 08:58:05
 * @LastEditors: Virus.V
 * @LastEditTime: 2020-05-25 14:49:47
 * @Description: file content
 * @Email: virusv@live.com
 */

#ifndef SRC_ADAPTER_ADAPTER_PRIVATE_H_
#define SRC_ADAPTER_ADAPTER_PRIVATE_H_

#include "smartocd.h"
#include "Library/misc/list.h"

#include "Adapter/adapter.h"

// 创建传输接口结构体，可以支持一次性创建多个
struct transport *adapterCreateTransport(int count);

// 注册传输接口API到adapter
int adapterRegistTransport(Adapter *adapter, struct transport *transport);

// 释放传输接口结构体
void adapterFreeTransport(struct transport *transport);

#endif /* SRC_ADAPTER_ADAPTER_PRIVATE_H_ */
