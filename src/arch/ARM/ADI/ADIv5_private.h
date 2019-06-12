/*
 * ADIv5_private.h
 *
 *  Created on: 2019-5-30
 *      Author: virusv
 */

#ifndef SRC_ARCH_ARM_ADI_ADIV5_PRIVATE_H_
#define SRC_ARCH_ARM_ADI_ADIV5_PRIVATE_H_

#include "misc/list.h"

#include "adapter/include/adapter.h"
#include "arch/ARM/ADI/include/ADIv5.h"

#define IS_DP_REG(reg) ((reg) == DP_REG_CTRL_STAT || (reg) == DP_REG_SELECT || (reg) == DP_REG_RDBUFF || (reg) == DP_REG_DPIDR || \
		(reg) == DP_REG_ABORT || (reg) == DP_REG_DLCR || (reg) == DP_REG_RESEND || (reg) == DP_REG_TARGETID || (reg) == DP_REG_DLPIDR \
		 || (reg) == DP_REG_EVENTSTAT || (reg) == DP_REG_TARGETSEL)

#define IS_AP_REG(reg) ((reg) == AP_REG_CSW || (reg) == AP_REG_TAR_LSB || (reg) == AP_REG_TAR_MSB || (reg) == AP_REG_DRW || (reg) == AP_REG_BD0 || (reg) == AP_REG_BD1 || \
		(reg) == AP_REG_BD2 || (reg) == AP_REG_BD3 || (reg) == AP_REG_CFG || (reg) == AP_REG_ROM_MSB || (reg) == AP_REG_ROM_LSB || (reg) == AP_REG_IDR)

// Abort Register definitions
#define DP_ABORT_DAPABORT       0x00000001  // DAP Abort
#define DP_ABORT_STKCMPCLR      0x00000002  // Clear STICKYCMP Flag (SW Only)
#define DP_ABORT_STKERRCLR      0x00000004  // Clear STICKYERR Flag (SW Only)
#define DP_ABORT_WDERRCLR       0x00000008  // Clear WDATAERR Flag (SW Only)
#define DP_ABORT_ORUNERRCLR     0x00000010  // Clear STICKYORUN Flag (SW Only)

// Debug Control and Status definitions
#define DP_CTRL_ORUNDETECT		0x00000001  // Overrun Detect
#define DP_STAT_STICKYORUN		0x00000002  // Sticky Overrun
#define DP_CTRL_TRNMODEMSK		0x0000000C  // Transfer Mode Mask
#define DP_CTRL_TRNNORMAL		0x00000000  // Transfer Mode: Normal
#define DP_CTRL_TRNVERIFY		0x00000004  // Transfer Mode: Pushed Verify
#define DP_CTRL_TRNCOMPARE		0x00000008  // Transfer Mode: Pushed Compare
#define DP_STAT_STICKYCMP		0x00000010  // Sticky Compare
#define DP_STAT_STICKYERR		0x00000020  // Sticky Error
#define DP_STAT_READOK			0x00000040  // Read OK (SW Only)
#define DP_STAT_WDATAERR		0x00000080  // Write Data Error (SW Only)
#define DP_CTRL_MASKLANEMSK		0x00000F00  // Mask Lane Mask
#define DP_CTRL_MASKLANE0		0x00000100  // Mask Lane 0
#define DP_CTRL_MASKLANE1		0x00000200  // Mask Lane 1
#define DP_CTRL_MASKLANE2		0x00000400  // Mask Lane 2
#define DP_CTRL_MASKLANE3		0x00000800  // Mask Lane 3
#define DP_CTRL_TRNCNTMSK		0x001FF000  // Transaction Counter Mask
#define DP_CTRL_CDBGRSTREQ		0x04000000  // Debug Reset Request
#define DP_STAT_CDBGRSTACK		0x08000000  // Debug Reset Acknowledge
#define DP_CTRL_CDBGPWRUPREQ	0x10000000  // Debug Power-up Request
#define DP_STAT_CDBGPWRUPACK	0x20000000  // Debug Power-up Acknowledge
#define DP_CTRL_CSYSPWRUPREQ	0x40000000  // System Power-up Request
#define DP_STAT_CSYSPWRUPACK	0x80000000  // System Power-up Acknowledge

// Debug Select Register definitions
#define DP_SELECT_CTRLSELMSK	0x00000001  // CTRLSEL (SW Only)
#define DP_SELECT_APBANKSELMSK	0x000000F0  // APBANKSEL Mask
#define DP_SELECT_APSELMSK		0xFF000000  // APSEL Mask

// AP Control and Status Word definitions
#define AP_CSW_SIZEMSK			0x00000007  // Access Size: Selection Mask
#define AP_CSW_SIZE8			0x00000000  // Access Size: 8-bit
#define AP_CSW_SIZE16			0x00000001  // Access Size: 16-bit
#define AP_CSW_SIZE32			0x00000002  // Access Size: 32-bit
#define AP_CSW_SIZE64			0x00000003	// Access Size: 64-bit
#define AP_CSW_SIZE128			0x00000004	// Access Size: 128-bit
#define AP_CSW_SIZE256			0x00000005	// Access Size: 256-bit

#define AP_CSW_ADDRINC_POS		0x4			// Addr Inc 偏移位置
#define AP_CSW_ADDRINC_MSK		0x3			// Addr Inc 位掩码
#define AP_CSW_NADDRINC			0x00000000  // No Address Increment
#define AP_CSW_SADDRINC			0x00000001  // Single Address Increment
#define AP_CSW_PADDRINC			0x00000002  // Packed Address Increment

#define AP_CSW_DEVENABLE		0x00000040  // Device Enable
#define AP_CSW_TINPROG			0x00000080  // Transfer in progress
#define AP_CSW_HPROT			0x02000000  // User/Privilege Control

#define AP_CSW_MSTRTYPEMSK		0x20000000  // Master Type Mask
#define AP_CSW_MSTRCORE			0x00000000  // Master Type: Core
#define AP_CSW_MSTRDBG			0x20000000  // Master Type: Debug
#define AP_CSW_RESERVED			0x01000000  // Reserved Value

#define AP_CFG_LARGE_DATA		0x4
#define AP_CFG_LARGE_ADDRESS	0x2
#define AP_CFG_BIG_ENDIAN		0x1

#define JEP106_CODE_ARM			0x23B	// ARM JEP106 CODE

#define DP_SELECT_APSEL			0xFF000000
#define DP_SELECT_APBANK		0x000000F0
#define DP_SELECT_DPBANK		0x0000000F
#define DP_SELECT_MASK			0xFF0000FF

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
		uint32_t : 16;
		uint32_t AP_Sel : 8;
	} regInfo;
} ADIv5_DpSelectRegister;

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
	Adapter adapter;	// Adapter对象的接口
	struct dap dapApi;
	ADIv5_DpSelectRegister select;	// SELECT寄存器
};

// AP定义
struct ADIv5_AccessPort{
	ADIv5_ApIdrRegister idr;	// APIDR寄存器
	unsigned int index;	// AP的索引
	struct list_head list_entry;
	struct accessPort apApi;
	struct ADIv5_Dap *dap;
	union {
		// MEM-AP
		struct {
			ADIv5_ApCswRegister csw;
			uint64_t rom;	// ROM Table基址
			struct {
				uint8_t largeAddress:1;	// 该AP是否支持64位地址访问，如果支持，则TAR和ROM寄存器是64位
				uint8_t largeData:1;	// 是否支持大于32位数据传输
				uint8_t bigEndian:1;	// 是否是大端字节序，ADI5.2废弃该功能，所以该位必须为0
				uint8_t packedTransfers:1;	// 是否支持packed传输
				uint8_t lessWordTransfers:1;	// 是否支持小于1个字的传输
			} config;
		} memory;
		//JTAG-AP
		struct {
			// TODO 增加JTAG-AP相关属性
		} jtag;
	} type;
};

#endif /* SRC_ARCH_ARM_ADI_ADIV5_PRIVATE_H_ */
