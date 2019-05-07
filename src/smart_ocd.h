/*
 * smart_ocd.h
 *
 *  Created on: 2018-1-19
 *      Author: virusv
 */

#ifndef SRC_SMART_OCD_H_
#define SRC_SMART_OCD_H_

#include <assert.h>
#include <stdint.h>
#include <inttypes.h>
#include <setjmp.h>

#ifdef HAVE_CONFIG
#include "global_config.h"
#endif

// 定义offsetof宏
#ifndef offsetof
	#define offsetof(type, member) (size_t)&(((type*)0)->member)
#endif
// 定义container_of宏
#ifndef container_of
	#define container_of(ptr, type, member) ({ \
		 const typeof( ((type *)0)->member ) *__mptr = (ptr); \
		 (type *)( (char *)__mptr - offsetof(type,member) );})
#endif

// 接口参数属性
#define IN	// 入参
#define OUT	// 出参
#define OPTIONAL	// 可选，根据其他参数指定

// 接口返回值类型为int型，具体长度由平台指定
// 接口的返回值只能作为该接口执行状态
// 接口返回状态值具体含义由各个模块定义
// 但是规定0返回值作为成功

// 定义BOOL类型
typedef enum {
	FALSE = 0,
	TRUE
} BOOL;

// 致命错误恢复点
extern jmp_buf fatalException;

#define CAST(type,val) ((type)(val))
#define BYTE_IDX(data,idx) (((data) >> (idx * 8)) & 0xff)

// 对象的构造函数和析构函数的定义和调用  XXX 弃用
#define __CONSTRUCT(class) __construct_##class##Object
#define __DESTORY(class) __destory_##class##Object

// 用户输入lua命令历史记录文件
#define COMMAND_HISTORY "smartocd_history.txt"
#endif /* SRC_SMART_OCD_H_ */
