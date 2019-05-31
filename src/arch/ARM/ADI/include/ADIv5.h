/*
 * ADIv5.h
 *
 *  Created on: 2018年6月10日
 *      Author: virusv
 */

#ifndef SRC_ARCH_ARM_ADI_ADIV5_H_
#define SRC_ARCH_ARM_ADI_ADIV5_H_

#include "smart_ocd.h"
#include "adapter/include/adapter.h"

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
};

// 类型预定义
typedef struct dap *DAP;

/**
 * 初始化DAP
 * 参数:
 * 	adapter:Adapter对象
 */
DAP ADIv5_CreateDap(
		IN Adapter adapter	// Adapter对象
);

/**
 * 销毁DAP对象
 */
void ADIv5_DestoryDap(
		IN DAP* dap
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
	BUS_AMBA_AHB = 0x1,	// AMBA AHB bus
	BUS_AMBA_APB = 0x2,	// AMBA APB2 or APB3 bus
	BUS_AMBA_AXI = 0x4	// AMBA AXI3 or AXI4 bus, with optional ACT-Lite support
};

// AP类型预定义
typedef struct accessPort *AccessPort;

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
 * Access Port接口定义
 */
struct accessPort{
	enum AccessPortType type;	// AccessPort类型,只读! 不可以修改!
	union {
		// MEM-AP
		struct {
			// TODO 读取ROM Table的基址
			ADIv5_MEM_AP_READ_32 Read32;
			ADIv5_MEM_AP_WRITE_32 Write32;
		}Memory;
		// JTAG-AP
		struct {
			// TODO 增加JTAG-AP的相关接口
		}Jtag;
	} Interface;
};


#endif /* SRC_ARCH_ARM_ADI_ADIV5_H_ */
