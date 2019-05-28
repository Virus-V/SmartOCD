/*
 * ADIv5.h
 *
 *  Created on: 2018年6月10日
 *      Author: virusv
 */

#ifndef SRC_ARCH_ARM_ADI_ADIV5_H_
#define SRC_ARCH_ARM_ADI_ADIV5_H_

#include "smart_ocd.h"
#include "misc/list.h"

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

#define IS_DP_REG(reg) ((reg) == DP_REG_CTRL_STAT || (reg) == DP_REG_SELECT || (reg) == DP_REG_RDBUFF || (reg) == DP_REG_DPIDR || \
		(reg) == DP_REG_ABORT || (reg) == DP_REG_DLCR || (reg) == DP_REG_RESEND || (reg) == DP_REG_TARGETID || (reg) == DP_REG_DLPIDR \
		 || (reg) == DP_REG_EVENTSTAT || (reg) == DP_REG_TARGETSEL)

#define IS_AP_REG(reg) ((reg) == AP_REG_CSW || (reg) == AP_REG_TAR_LSB || (reg) == AP_REG_TAR_MSB || (reg) == AP_REG_DRW || (reg) == AP_REG_BD0 || (reg) == AP_REG_BD1 || \
		(reg) == AP_REG_BD2 || (reg) == AP_REG_BD3 || (reg) == AP_REG_CFG || (reg) == AP_REG_ROM_MSB || (reg) == AP_REG_ROM_LSB || (reg) == AP_REG_IDR)

/**
 * 错误码
 */
enum {
	ADI_SUCCESS = 0,
	ADT_FAILED,

};

// 类型预定义
typedef struct dap *DAP;

/**
 * 初始化DAP
 * 参数:
 * 	adapter:Adapter对象
 */
DAP ADIv5_CreateDap(
		IN Adapter adapter,	// Adapter对象
);

/**
 * 销毁DAP对象
 */
void ADIv5_DestoryDap(
		IN DAP* dap
);

// AP的类型
enum apType{
	AP_TYPE_JTAG = 0,	// JTAG AP
	AP_TYPE_AMBA_AHB = 0x1,	// AMBA AHB bus
	AP_TYPE_AMBA_APB = 0x2,	// AMBA APB2 or APB3 bus
	AP_TYPE_AMBA_AXI = 0x4	// AMBA AXI3 or AXI4 bus, with optional ACT-Lite support
};

// AP类型预定义
typedef struct accessPort *AccessPort;

/**
 * 寻找指定类型的AP
 * 参数:
 * 	self:dap自身对象
 * 	type:ap的type,请看 enum apType 定义
 * 	ap:找到的AP对象
 */
typedef int (*ADIv5_FIND_ACCESS_PORT)(
		IN DAP self,
		IN enum apType type,
		OUT AccessPort* ap
);

/**
 * DAP接口
 */
struct dap {
	ADIv5_FIND_ACCESS_PORT FindAccessPort;
};

/**
 * DP IDR Register layout
 */
typedef union {
	uint32_t regData;
	struct {
		uint32_t Designer : 12;	// Designer ID. An 11-bit JEDEC code
		/**
		 * Version of the Debug Port architecture implemented. Permitted values are:
		 * 0x0 Reserved. Implementations of DPv0 do not implement DPIDR.
		 * 0x1 DPv1 is implemented.
		 * 0x2 DPv2 is implemented.
		 */
		uint32_t Version : 4;
		/**
		 * Minimal Debug Port (MINDP) functions implemented:
		 * 0 : Transaction counter, Pushed-verify, and Pushed-find operations are implemented.
		 * 1 : Transaction counter, Pushed-verify, and Pushed-find operations are not implemented.
		 */
		uint32_t Min : 1;
		uint32_t : 3;	// RES0
		uint32_t PartNo : 8;	// Part Number，调试端口部件号
		uint32_t Revision : 4;	// 版本
	} regInfo;
} ADIv5_DpIdrRegister;

/**
 * AP IDR Register layout
 */
typedef union {
	uint32_t regData;
	struct {
		uint32_t Type : 4;	// Identifies the type of bus, or other connection, that connects to the AP（保留原文）
		uint32_t Variant : 4;	// Identifies different AP implementations of the same Type.
		uint32_t : 5;	// RES0 SBZ
		uint32_t Class : 4;	// Defines the class of AP.- 0b0000:No defined class. - 0b1000:Memory Access Port.
		uint32_t JEP106Code : 11;	// For an AP designed by ARM this field has the value 0x23B
		uint32_t Revision : 4;	// 版本
	} regInfo;
} ADIv5_ApIdrRegister;

/**
 * AP CSW Register layout
 */
typedef union {
	uint32_t regData;
	struct {
		uint32_t Size : 3;	// [RW] Size of access. This field indicates the size of access to perform.
		uint32_t : 1;	// RES0
		uint32_t AddrInc : 2;	// [RW] Address auto-increment and packing mode. This field controls whether the access address increments
								// automatically on read and write data accesses through the DRW register.
		uint32_t DeviceEn : 1;	// [RO or RAO] Device enabled. This bit is set to 1 when transactions can be issued through the MEM-AP.
		uint32_t TrInProg : 1;	// [RO] Transfer in progress. This bit is set to 1 while a transfer is in progress on the connection to the
								// memory system, and is set to 0 while the connection is idle.
								// After an ABORT operation, debug software can read this bit to check whether the aborted
								// transaction completed.
		uint32_t Mode : 4;		// [RW or RO] Mode of operation. The possible values of this bit are:
								// 0000 Basic mode.
								// 0001 Barrier support enabled.
		uint32_t Type : 4;		// See the entry for {Prot, Type}.
		uint32_t : 7;	// RES0
		uint32_t SPIDEN : 1;	// Secure Privileged Debug Enabled. If this bit is implemented, the possible values are:
								// 0 Secure accesses disabled.
								// 1 Secure accesses enabled.
								// This bit is optional, and read-only. If not implemented, the bit is RES 0.
		uint32_t Prot : 7;		// Bus access protection control. A debugger can use these fields to specify flags for a debug access.
		uint32_t DbgSwEnable : 1;	// Debug software access enable.
									// DbgSwEnable must be ignored and treated as one if DeviceEn is set to 0.
	} regInfo;
} ADIv5_ApCswRegister;

/**
 * DP CTRL/STAT Register layout
 */
typedef union {
	uint32_t regData;
	struct {
		uint32_t ORUNDETECT:1;	// 此位置1打开过载检测
		uint32_t STICKYORUN:1;	// 如果启用过载检测，此位为1则表示发生过载
		uint32_t TRNMODE:2;		// AP操作的Transfer mode。0：正常模式;1：pushed-verify模式；2：pushed-compare模式；3：保留
		uint32_t STICKYCMP:1;	// 匹配，pushed模式下有用
		uint32_t STICKYERR:1;	// AP Transaction发生错误
		uint32_t READOK:1;		// JTAG:RES0;SWD:RO/WI.该位为1表示上次AP读或者RDBUFF读是否OK
		uint32_t WDATAERR:1;	// JTAG:RES0;SWD:RO/WI.该位表示写入数据出错
								// 在SWD帧中的数据部分parity错误或者帧错误
		uint32_t MASKLANE:4;	// indicares the bytes to be masked in pushed-compare and pushed-verify operation.
		uint32_t TRNCNT:12;		// 传输计数器 Transaction Counter
		uint32_t :2;			// RES0
		uint32_t CDBGRSTREQ:1;	// Debug reset request. 这个位控制CDBGRSTREQ信号
		uint32_t CDBGRSTACK:1;	// Debug reset acknowledge. 这个位表示CDBGRSTACK信号
		uint32_t CDBGPWRUPREQ:1;	// Debug power up request. 这个位控制CDBGPWRUPREQ信号
		uint32_t CDBGPWRUPACK:1;	// Debug power up acknowledge. 这个位表示CDBGPWRUPACK信号
		uint32_t CSYSPWRUPREQ:1;	// System power up request. 这个位控制CSYSPWRUPREQ信号
		uint32_t CSYSPWRUPACK:1;	// System power up acknowledge. 这个位表示CSYSPWRUPACK信号
	} regInfo;
} ADIv5_DpCtrlStatRegister;

// SELECT寄存器解析
typedef union {
	uint32_t regData;
	struct {
		uint32_t DP_BankSel : 4;
		uint32_t AP_BankSel : 4;
		uint32_t Invaild : 1;	// 表示是否需要强制更新SELECT寄存器
		uint32_t : 15;
		uint32_t AP_Sel : 8;
	} regInfo;
} ADIv5_DpSelectRegister;

// packed transfer
union PackedTransferData{
	uint32_t data_32;
	uint16_t data_16[2];
	uint8_t data_8[4];
};

/**
 * ADIv5 的DAP结构体
 */
struct ADIv5_Dap{
	struct list_head apList;	// AP链表
	struct dap dap;
	ADIv5_DpSelectRegister select;	// SELECT寄存器
	ADIv5_DpCtrlStatRegister ctrlStat;

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
 * AccessPort接口定义
 */
union accessPort{
	// MEM-AP
	struct {
		ADIv5_MEM_AP_READ_32 Read32;

		ADIv5_MEM_AP_WRITE_32 Write32;
	}Memory;
	// JTAG-AP
	struct {
		// TODO 增加JTAG-AP的相关接口
	}Jtag;
};

// AP定义
union ADIv5_AccessPort{
	// MEM-AP
	struct {
		ADIv5_ApIdrRegister IDR;
		ADIv5_ApCswRegister CSW;
		uint64_t TAR;
		union accessPort ApApi;
	} Memory;
	//JTAG-AP
	struct {
		// TODO 增加JTAG-AP相关属性
		union accessPort ApApi;
	} JTAG;
};

#endif /* SRC_ARCH_ARM_ADI_ADIV5_H_ */
