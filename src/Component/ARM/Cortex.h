/*
 * Cortex.h
 *
 *  Created on: 2018-2-25
 *      Author: virusv
 */

#ifndef SRC_ARCH_ARM_CORTEX_H_
#define SRC_ARCH_ARM_CORTEX_H_

#include "debugger/adapter.h"
/** 架构定义头文件 */
#include "arch/ARM/ARMv8.h"
#include "arch/ARM/ARMv7.h"

/**
 * TODO Cortex-M 系列核心，32位
 * Cortex-M0, Cortex M0+, and Cortex-M23 for applications requiring minimal cost, power and area
 * Cortex-M3, Cortex-M4, and Cortex-M33 for all applications where a balance between 32-bit performance and energy efficiency is desirable
 * Cortex-M7 is designed for embedded applications requiring high performance
 */
typedef struct Cortex_M {
	AdapterObject *adapterObj;	// 仿真器对象
	BOOL (*Read8Addr32)(uint32_t address, uint8_t *data);
	BOOL (*Read16Addr32)(uint32_t address, uint16_t *data);
	BOOL (*Read32Addr32)(uint32_t address, uint32_t *data);
	BOOL (*Read64Addr32)(uint32_t address, uint64_t *data);

	BOOL (*Write8Addr32)(uint32_t address, uint8_t data);
	BOOL (*Write16Addr32)(uint32_t address, uint16_t data);
	BOOL (*Write32Addr32)(uint32_t address, uint32_t data);
	BOOL (*Write64Addr32)(uint32_t address, uint64_t data);
} Cortex_M;

/**
 * TODO Cortex-R 系列核心，32位
 * Cortex-R8 Highest performance 5G modem and storage
 * Cortex-R7 High performance 4G modem and storage
 */
typedef struct Cortex_R {

} Cortex_R;

/**
 * TODO Cortex-A 系列核心，32/64位
 * * Highest performance *
 * Cortex-A75 - 64/32-bit Armv8-A 2018 DynamIQ processor for highest performance from mobile to cloud
 * Cortex-A73 - 64/32-bit Armv8-A 2017 Premium Mobile, Consumer
 * Cortex-A72 - 64/32-bit Armv8-A 2016 Premium Mobile, Infrastructure & Auto
 * Cortex-A57 - 64/32-bit Armv8-A Proven high-performance
 * Cortex-A17 - Armv7-A High-performance with lower power and smaller area relative to Cortex-A15
 * Cortex-A15 - Armv7-A High-performance with infrastructure feature set
 *
 * * Performance and efficiency *
 * Cortex-A55 - 64/32-bit Armv8-A Highest efficiency DynamIQ processor for mid-range solutions
 * Cortex-A53 - 64/32-bit Armv8-A The most widely used 64-bit CPU
 * Cortex-A9 - Armv7-A Well-established mid-range processor
 * Cortex-A8 - Armv7-A First Armv7-A processor
 *
 * * Smallest, lowest power *
 * The Cortex-A3X series processors are available on the Armv8-A architecture.
 * Cortex-A35 - 64/32-bit Armv8-A Highest efficiency Armv8-A CPU
 * Cortex-A32 - 32-bit Armv8-A Smallest and lowest power Armv8-A CPU
 * Cortex-A7 - Armv7-A Most efficient Armv7-A CPU, higher performance than Cortex-A5
 * Cortex-A5 - Armv7-A Smallest and lowest power Armv7-A CPU
 */

typedef struct Cortex_A {
	AdapterObject *adapterObj;	// 仿真器对象
	// TODO 添加Cache MMU操作
	/* 32位地址读写 */
	BOOL (*Read8Addr32)(uint32_t address, uint8_t *data);
	BOOL (*Read16Addr32)(uint32_t address, uint16_t *data);
	BOOL (*Read32Addr32)(uint32_t address, uint32_t *data);
	BOOL (*Read64Addr32)(uint32_t address, uint64_t *data);

	BOOL (*Write8Addr32)(uint32_t address, uint8_t data);
	BOOL (*Write16Addr32)(uint32_t address, uint16_t data);
	BOOL (*Write32Addr32)(uint32_t address, uint32_t data);
	BOOL (*Write64Addr32)(uint32_t address, uint64_t data);

	/* 64位地址读写 */
	BOOL (*Read8Addr64)(uint32_t address, uint8_t *data);
	BOOL (*Read16Addr64)(uint32_t address, uint16_t *data);
	BOOL (*Read32Addr64)(uint32_t address, uint32_t *data);
	BOOL (*Read64Addr64)(uint32_t address, uint64_t *data);

	BOOL (*Write8Addr64)(uint32_t address, uint8_t data);
	BOOL (*Write16Addr64)(uint32_t address, uint16_t data);
	BOOL (*Write32Addr64)(uint32_t address, uint32_t data);
	BOOL (*Write64Addr64)(uint32_t address, uint64_t data);
} Cortex_A;


#endif /* SRC_ARCH_ARM_CORTEX_H_ */
