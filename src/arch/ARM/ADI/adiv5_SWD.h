/*
 * adiv5_SWD.h
 *
 *  Created on: 2018-3-8
 *      Author: virusv
 */

#ifndef SRC_ARCH_ARM_ADI_ADIV5_SWD_H_
#define SRC_ARCH_ARM_ADI_ADIV5_SWD_H_

#include "arch/ARM/ADI/DAP.h"

// 定义DAP命令结构体
struct swdInstr {
	DAPObject *dapObj;	// DAP 对象
	uint8_t request;	// SWD 指令
	uint8_t ack;		// 该指令响应值
	uint32_t select;	// SELECT寄存器
	uint32_t data;		// 要读取或写入的值

};

/*
 * 将SWD指令加入队列
 * 执行队列的指令
 * 清空队列指令
 * 删除队列指令
 */

#endif /* SRC_ARCH_ARM_ADI_ADIV5_SWD_H_ */
