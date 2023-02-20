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

#ifndef SRC_ARCH_ARM_ADI_ADIV5_H_
#define SRC_ARCH_ARM_ADI_ADIV5_H_

#include "smartocd.h"

#include "Adapter/adapter_dap.h"

#ifdef _IMPORTED_ARM_ADI_DEFINES_
#error "Already imported ADI defines!!"
#endif

#define _IMPORTED_ARM_ADI_DEFINES_
#define _IMPORTED_ARM_ADI_VERSION "ADIv5"

// JTAG扫描链的IR Code
#define ADI_JTAG_ABORT				0x08U
#define ADI_JTAG_DPACC				0x0AU
#define ADI_JTAG_APACC				0x0BU
#define ADI_JTAG_IDCODE				0x0EU
#define ADI_JTAG_BYPASS				0x0FU

// JTAG 应答
#define ADI_JTAG_RESP_OK_FAULT		0x02U	// OK/FAULT
#define ADI_JTAG_RESP_WAIT			0x01U	// WAIT

/**
 * 所有寄存器格式是 bank在[8:4], Addr在[3:2],低二位RES0,所以最bit0用来表示寄存器类型:0=DP,1=AP
 */

// DPv0只有JTAG模式，所以没有Abort寄存器，也没有IDCODE/DPIDR寄存器，都有一个单独的扫描链
/* DP寄存器地址 bit0 = 0 表示DP */
// DPv0下有如下寄存器
#define DP_REG_CTRL_STAT	0x04U	// Control & Status
#define DP_REG_SELECT		0x08U	// Select Register (JTAG R/W & DPv0 SW W)
#define DP_REG_RDBUFF		0x0CU	// Read Buffer (Read Only)
// DPv1增加下列寄存器
#define DP_REG_DPIDR		0x00U	// IDCODE Register (SW Read only)
#define DP_REG_ABORT		0x00U	// Abort Register (SW Write only)
#define DP_REG_DLCR			0x14U	// Data Link Control Register(SW Only RW)
#define DP_REG_RESEND		0x08U	// Read Resend Register (SW Read Only)
// DPv2增加下列寄存器
#define DP_REG_TARGETID		0x24U	// Target Identification register (RO)
#define DP_REG_DLPIDR		0x34U	// Data Link Protocol Identification Register (SW RO)
#define DP_REG_EVENTSTAT	0x44U	// Event Status register (RO)
#define DP_REG_TARGETSEL	0x0CU	// Target Selection register (SW WO, SWD Protocol v2才存在)
/* AP寄存器 bit0 = 1 表示AP */
#define AP_REG_CSW			0x01U		// Control and Status Word
#define AP_REG_TAR_LSB 		0x05U		// Transfer Address
#define AP_REG_TAR_MSB 		0x09U
#define AP_REG_DRW			0x0DU		// Data Read/Write
#define AP_REG_BD0			0x11U		// Banked Data 0
#define AP_REG_BD1			0x15U		// Banked Data 1
#define AP_REG_BD2			0x19U		// Banked Data 2
#define AP_REG_BD3			0x1DU		// Banked Data 3
#define AP_REG_ROM_MSB		0xF1U		// Debug ROM Address MSByte
#define AP_REG_CFG			0xF5U		// CFG Register
#define AP_REG_ROM_LSB		0xF9U		// Debug ROM Address LSByte
#define AP_REG_IDR			0xFDU		// Identification Register

/**
 * 错误码
 */
enum {
	ADI_SUCCESS = 0,
	ADI_FAILED,
	ADI_ERR_INTERNAL_ERROR,	// 不是由ADI的逻辑造成的错误
	ADI_ERR_BAD_PARAMETER,	// 无效的参数
	ADI_ERR_UNSUPPORT,	// 不支持的操作
};

// DAP类型预定义
typedef struct dap *DAP;
// AP类型预定义
typedef struct accessPort *AccessPort;

/**
 * 初始化DAP
 * 参数:
 * 	adapter:Adapter对象
 */
DAP ADIv5_CreateDap(
		IN DapSkill skillObj // Adapter对象
);

/**
 * 销毁DAP对象
 */
void ADIv5_DestoryDap(
		IN DAP* dap
);

/**
 * 读取Component ID和Peripheral ID
 * 参数:
 * 	self:AccessPort对象
 * 	componentBase：Component的基址，必须4KB对齐
 * 	cid：读取的Component ID
 * 	pid：读取的Peripheral ID
 */
int ADIv5_ReadCidPid(
		IN AccessPort self,
		IN uint64_t componentBase,
		OUT uint32_t *cid,
		OUT uint64_t *pid
);

/**
 * Access Port 类型
 */
enum AccessPortType {
	AccessPort_Memory,	// Memory Access Port
	AccessPort_JTAG,
	AccessPort_MAX	// 非法值
};

// AP的类型
enum busType{
	Bus_AMBA_AHB = 0x1,	// AMBA AHB bus
	Bus_AMBA_APB = 0x2,	// AMBA APB2 or APB3 bus
	Bus_AMBA_AXI = 0x4	// AMBA AXI3 or AXI4 bus, with optional ACT-Lite support
};

/**
 * 寻找指定类型的AP
 * 参数:
 * 	self:dap自身对象
 * 	type:ap的type,请看 enum AccessPortType 定义
 * 	bus:bus类型,请看 enum busType 定义
 * 	ap:找到的AP对象
 */
typedef int (*ADIv5_FIND_ACCESS_PORT)(
		IN DAP self,
		IN enum AccessPortType type,
		OPTIONAL enum busType bus,
		OUT AccessPort* ap
);

/**
 * DAP接口
 */
struct dap {
	ADIv5_FIND_ACCESS_PORT FindAccessPort;
};

/**
 * MEM-AP 读8位数据
 * 参数:
 * 	self:AP对象
 * 	addr:要读的地址
 * 	data:写入数据
 */
typedef int (*ADIv5_MEM_AP_READ_8)(
		IN AccessPort self,
		IN uint64_t addr,
		OUT uint8_t *data
);

/**
 * MEM-AP 读16位数据
 * 参数:
 * 	self:AP对象
 * 	addr:要读的地址
 * 	data:写入数据
 */
typedef int (*ADIv5_MEM_AP_READ_16)(
		IN AccessPort self,
		IN uint64_t addr,
		OUT uint16_t *data
);

/**
 * MEM-AP 读32位数据
 * 参数:
 * 	self:AP对象
 * 	addr:要读的地址
 * 	data:写入数据
 */
typedef int (*ADIv5_MEM_AP_READ_32)(
		IN AccessPort self,
		IN uint64_t addr,
		OUT uint32_t *data
);

/**
 * MEM-AP 读64位数据
 * 参数:
 * 	self:AP对象
 * 	addr:要读的地址
 * 	data:写入数据
 */
typedef int (*ADIv5_MEM_AP_READ_64)(
		IN AccessPort self,
		IN uint64_t addr,
		OUT uint64_t *data
);

/**
 * MEM-AP 写8位数据
 * 参数:
 * 	self:AP对象
 * 	addr:要写的地址
 * 	data:要写入的数据
 */
typedef int (*ADIv5_MEM_AP_WRITE_8)(
		IN AccessPort self,
		IN uint64_t addr,
		IN uint8_t data
);

/**
 * MEM-AP 写16位数据
 * 参数:
 * 	self:AP对象
 * 	addr:要写的地址
 * 	data:要写入的数据
 */
typedef int (*ADIv5_MEM_AP_WRITE_16)(
		IN AccessPort self,
		IN uint64_t addr,
		IN uint16_t data
);

/**
 * MEM-AP 写32位数据
 * 参数:
 * 	self:AP对象
 * 	addr:要写的地址
 * 	data:要写入的数据
 */
typedef int (*ADIv5_MEM_AP_WRITE_32)(
		IN AccessPort self,
		IN uint64_t addr,
		IN uint32_t data
);

/**
 * MEM-AP 写64位数据
 * 参数:
 * 	self:AP对象
 * 	addr:要写的地址
 * 	data:要写入的数据
 */
typedef int (*ADIv5_MEM_AP_WRITE_64)(
		IN AccessPort self,
		IN uint64_t addr,
		IN uint64_t data
);

/**
 * 地址自增模式
 * AddrInc_Off：在每次传输之后TAR中的地址不自增
 * AddrInc_Single：每次传输成功后，TAR的地址增加传输数据的大小
 * AddrInc_Packed：当设置此参数时，启动packed传送模式；每次传输成功后，TAR增加传输的数据大小。
 */
enum addrIncreaseMode{
	AddrInc_Off = 0,
	AddrInc_Single,
	AddrInc_Packed,
};

/**
 * 每次总线请求的数据长度
 */
enum dataSize{
	DataSize_8 = 0,
	DataSize_16,
	DataSize_32,
	DataSize_64,
	DataSize_128,
	DataSize_256,
};

/**
 * Block读,用于发起多次请求
 * 参数:
 * 	self:AccessPort对象
 * 	addr:访问的起始地址
 * 	mode:地址自增模式
 * 	size:单次总线请求的数据长度
 * 	count:传输的总次数
 * 	data:数据存放地址
 */
typedef int (*ADIv5_MEM_AP_BLOCK_READ)(
		IN AccessPort self,
		IN uint64_t addr,
		IN enum addrIncreaseMode mode,
		IN enum dataSize size,
		IN unsigned int count,
		OUT uint8_t *data
);

/**
 * Block写,用于发起多次请求
 * 参数:
 * 	self:AccessPort对象
 * 	addr:访问的起始地址
 * 	mode:地址自增模式
 * 	size:单次总线请求的数据长度
 * 	count:传输的总次数
 * 	data:数据存放地址
 */
typedef int (*ADIv5_MEM_AP_BLOCK_WRITE)(
		IN AccessPort self,
		IN uint64_t addr,
		IN enum addrIncreaseMode mode,
		IN enum dataSize size,
		IN unsigned int count,
		IN uint8_t *data
);

/**
 * 读CSW寄存器
 * 参数:
 * 	self:AccessPort对象
 * 	data:数据存放地址
 */
typedef int (*ADIv5_MEM_AP_CSW_READ)(
		IN AccessPort self,
		OUT uint32_t *data
);

/**
 * 写CSW寄存器
 * 参数:
 * 	self:AccessPort对象
 * 	data:CSW寄存器的值
 */
typedef int (*ADIv5_MEM_AP_CSW_WRITE)(
		IN AccessPort self,
		IN uint32_t data
);

/**
 * 终止本次传输
 * 参数:
 * 	self:AccessPort对象
 */
typedef int (*ADIv5_MEM_AP_ABORT)(
		IN AccessPort self
);

/**
 * Access Port接口定义
 */
struct accessPort{
	const enum AccessPortType type;	// AccessPort类型,只读! 不可以修改!
	union {
		// MEM-AP
		struct {
			const uint64_t RomTableBase;	// ROM Table的基址,只读!不可修改
			// 读写CSW寄存器
			ADIv5_MEM_AP_CSW_READ ReadCSW;
			ADIv5_MEM_AP_CSW_WRITE WriteCSW;
			// 写ABORT寄存器:终止本次传输
			ADIv5_MEM_AP_ABORT Abort;

			ADIv5_MEM_AP_READ_8 Read8;
			ADIv5_MEM_AP_READ_16 Read16;
			ADIv5_MEM_AP_READ_32 Read32;
			ADIv5_MEM_AP_READ_64 Read64;
			ADIv5_MEM_AP_BLOCK_READ BlockRead;

			ADIv5_MEM_AP_WRITE_8 Write8;
			ADIv5_MEM_AP_WRITE_16 Write16;
			ADIv5_MEM_AP_WRITE_32 Write32;
			ADIv5_MEM_AP_WRITE_64 Write64;
			ADIv5_MEM_AP_BLOCK_WRITE BlockWrite;
		}Memory;
		// JTAG-AP
		struct {
			// TODO 增加JTAG-AP的相关接口
		}Jtag;
	} Interface;
};


#endif /* SRC_ARCH_ARM_ADI_ADIV5_H_ */
