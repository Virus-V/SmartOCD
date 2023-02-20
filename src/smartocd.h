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

#ifndef SRC_SMART_OCD_H_
#define SRC_SMART_OCD_H_

#include <assert.h>
#include <inttypes.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdlib.h>

#ifdef HAVE_CONFIG
#include "global_config.h"
#endif

// 定义offsetof宏
#ifndef offsetof
#define offsetof(type, member) (size_t) & (((type *)0)->member)
#endif
// 定义container_of宏
#ifndef container_of
#define container_of(ptr, type, member)                \
  ({                                                   \
    const typeof(((type *)0)->member) *__mptr = (ptr); \
    (type *)((char *)__mptr - offsetof(type, member)); \
  })
#endif

// 接口参数属性
#define IN       // 入参
#define OUT      // 出参
#define OPTIONAL // 可选，根据其他参数指定

#define SIGNATURE_16(A, B) ((A) | (B << 8))
#define SIGNATURE_32(A, B, C, D) (SIGNATURE_16(A, B) | (SIGNATURE_16(C, D) << 16))
#define SIGNATURE_64(A, B, C, D, E, F, G, H) \
  (SIGNATURE_32(A, B, C, D) | ((UINT64)(SIGNATURE_32(E, F, G, H)) << 32))

// 接口返回值类型为int型，具体长度由平台指定
// 接口的返回值一般只能作为该接口执行状态
// 但是当一个接口永远执行成功且只有一个返回值时,可以通过函数返回值返回函数结果
// 接口返回状态值具体含义由各个模块定义
// 但是规定0返回值作为成功

// 定义BOOL类型
typedef enum { FALSE = 0, TRUE } BOOL;

#define CAST(type, val) ((type)(val))
#define BYTE_IDX(data, idx) (((data) >> (idx * 8)) & 0xff)
// 接口常量值初始化(只可以在结构体中的常量使用):
// type:const常量类型,obj:const常量对象,val:const常量值.
// FIXME 名字改成INTERFACE_PROTO_SET更合适
#define INTERFACE_CONST_INIT(type, obj, val) (*((type *)&(obj)) = (type)(val))

// 用户输入lua命令历史记录文件
#define COMMAND_HISTORY "smartocd_history.txt"
#endif /* SRC_SMART_OCD_H_ */
