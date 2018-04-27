/*
 * DAP.h
 *
 *  Created on: 2018-2-18
 *      Author: virusv
 */

#ifndef SRC_ARCH_CORESIGHT_DAP_H_
#define SRC_ARCH_CORESIGHT_DAP_H_

#include "smart_ocd.h"
#include "misc/list.h"
#include "debugger/adapter.h"
#include "lib/TAP.h"

/*
 * Copyright (c) 2013-2016 ARM Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the License); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an AS IS BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * ----------------------------------------------------------------------
 *
 * $Date:        20. May 2015
 * $Revision:    V1.10
 *
 * Project:      CMSIS-DAP Include
 * Title:        DAP.h Definitions
 *
 *---------------------------------------------------------------------------*/
// DAP SWJ Pins
#define DAP_SWJ_SWCLK_TCK		0       // SWCLK/TCK
#define DAP_SWJ_SWDIO_TMS		1       // SWDIO/TMS
#define DAP_SWJ_TDI				2       // TDI
#define DAP_SWJ_TDO				3       // TDO
#define DAP_SWJ_nTRST			5       // nTRST
#define DAP_SWJ_nRESET			7       // nRESET

// DAP Transfer Request
#define DAP_TRANSFER_APnDP		(1U<<0)
#define DAP_TRANSFER_RnW		(1U<<1)
#define DAP_TRANSFER_A2			(1U<<2)
#define DAP_TRANSFER_A3			(1U<<3)

// DP寄存器地址
// DPv0只有JTAG模式，所以没有Abort寄存器，也没有IDCODE/DPIDR寄存器，都有一个单独的扫描链
// DPv0下有如下寄存器
#define DP_CTRL_STAT			0x04U   // Control & Status
#define DP_SELECT				0x08U   // Select Register (JTAG R/W & SW W)
#define DP_RDBUFF				0x0CU   // Read Buffer (Read Only)
// DPv1增加下列寄存器
#define DP_DPIDR				0x00U   // IDCODE Register (SW Read only)
#define DP_ABORT				0x00U   // Abort Register (SW Write only)
#define DP_DLCR					0x14U	// Data Link Control Register(SW Only RW)
#define DP_RESEND				0x08U	// Read Resend Register (SW Read Only)
// DPv2增加下列寄存器
#define DP_TARGETID				0x24U	// Target Identification register (RO)
#define DP_DLPIDR				0x34U	// Data Link Protocol Identification Register (SW RO)
#define DP_EVENTSTAT			0x44U	// Event Status register (RO)
#define DP_TARGETSEL			0x0CU	// Target Selection register (SW WO, SWD Protocol v2才存在)

// JTAG IR Codes
#define JTAG_ABORT				0x08U
#define JTAG_DPACC				0x0AU
#define JTAG_APACC				0x0BU
#define JTAG_IDCODE				0x0EU
#define JTAG_BYPASS				0x0FU

// JTAG Response
#define JTAG_RESP_OK_FAULT		0x02	// OK/FAULT
#define JTAG_RESP_WAIT			0x01	// WAIT

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

// Access Port Register Addresses
#define AP_REG_CSW				0x00        // Control and Status Word
#define AP_REG_TAR_LSB			0x04        // Transfer Address
#define AP_REG_TAR_MSB			0x08
#define AP_REG_DRW				0x0C        // Data Read/Write
#define AP_REG_BD0				0x10        // Banked Data 0
#define AP_REG_BD1				0x14        // Banked Data 1
#define AP_REG_BD2				0x18        // Banked Data 2
#define AP_REG_BD3				0x1C        // Banked Data 3
#define AP_REG_CFG				0xF4		// CFG Register
#define AP_REG_ROM_LSB			0xF8        // Debug ROM Address
#define AP_REG_ROM_MSB			0xF0
#define AP_REG_IDR				0xFC        // Identification Register

// AP Control and Status Word definitions
#define AP_CSW_SIZEMSK			0x00000007  // Access Size: Selection Mask
#define AP_CSW_SIZE8			0x00000000  // Access Size: 8-bit
#define SIZE_8					0x0
#define AP_CSW_SIZE16			0x00000001  // Access Size: 16-bit
#define SIZE_16					0x1
#define AP_CSW_SIZE32			0x00000002  // Access Size: 32-bit
#define SIZE_32					0x2
#define AP_CSW_SIZE64			0x00000003	// Access Size: 64-bit
#define SIZE_64					0x3
#define AP_CSW_SIZE128			0x00000004	// Access Size: 128-bit
#define SIZE_128				0x4
#define AP_CSW_SIZE256			0x00000005	// Access Size: 256-bit
#define SIZE_256				0x5

#define AP_CSW_ADDRINCMSK		0x00000030  // Auto Address Increment Mask
#define AP_CSW_NADDRINC			0x00000000  // No Address Increment
#define ADDRINC_OFF				0x0
#define AP_CSW_SADDRINC			0x00000010  // Single Address Increment
#define ADDRINC_SINGLE			0x1
#define AP_CSW_PADDRINC			0x00000020  // Packed Address Increment
#define ADDRINC_PACKED			0x2

#define AP_CSW_DBGSTAT			0x00000040  // Debug Status
#define AP_CSW_TINPROG			0x00000080  // Transfer in progress
#define AP_CSW_HPROT			0x02000000  // User/Privilege Control

#define AP_CSW_MSTRTYPEMSK		0x20000000  // Master Type Mask
#define AP_CSW_MSTRCORE			0x00000000  // Master Type: Core
#define AP_CSW_MSTRDBG			0x20000000  // Master Type: Debug
#define AP_CSW_RESERVED			0x01000000  // Reserved Value

#define JEP106_CODE_ARM			0x23B	// ARM JEP106 CODE

#define DP_SELECT_APSEL			0xFF000000
#define DP_SELECT_APBANK		0x000000F0
#define DP_SELECT_DPBANK		0x0000000F
#define DP_SELECT_INVALID		0x00FFFF00 /* Reserved bits one */

enum ap_type{
	AP_TYPE_JTAG = 0,	// JTAG AP
	AP_TYPE_AMBA_AHB,	// AMBA AHB bus
	AP_TYPE_AMBA_APB,	// AMBA APB2 or APB3 bus
	AP_TYPE_AMBA_AXI	// AMBA AXI3 or AXI4 bus, with optional ACT-Lite support
};

/**
 * 队列操作的时候选择AP还是DP
 */
enum queue_apndp {DP = 0,AP};

// DP IDR Register 解析
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
} DP_IDR_Parse;
// AP IDR Register 解析
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
} AP_IDR_Parse;

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
} MEM_AP_CSW_Parse;

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
}DP_CTRL_STATUS_Parse;

// AP对象 TODO 没研究过JTAG AP，以后添加。
struct ap{
	//uint8_t apIdx;	// 当前ap的索引
	struct {
		uint8_t init:1;	// 该AP是否初始化过
		uint8_t largeAddress:1;	// 该AP是否支持64位地址访问，如果支持，则TAR和ROM寄存器是64位
		uint8_t largeData:1;	// 是否支持大于32位数据
		uint8_t bigEndian:1;	// 是否是大端字节序，ADI5.2废弃该功能，所以该位必须为0
		uint8_t packedTransfers:1;	// 是否支持packed传输
		uint8_t lessWordTransfers:1;	// 是否支持小于1个字的传输
	} ctrl_state;
	MEM_AP_CSW_Parse CSW;	// CSW 寄存器
};

// DAP对象
typedef struct DAPObject DAPObject;
struct DAPObject{
	TAPObject tapObj;	// 继承于TAP
	uint8_t DP_Version;	// DP版本号
	struct ap AP[256];	// AP列表
	uint16_t TAP_index;	// TAP在JTAG中的索引，SWD模式下忽略该参数
	DP_CTRL_STATUS_Parse CTRL_STAT_Reg;	// 当前CTRL/STAT寄存器
	uint32_t ir;	// 当前ir寄存器
	union {
		uint32_t data;
		struct {
			uint32_t DP_BankSel : 4;
			uint32_t AP_BankSel : 4;
			uint32_t : 16;
			uint32_t AP_Sel : 8;
		} info;
	}SelectReg;	// 当前Select寄存器
	int retry;	// 接收到WAIT时重试次数
	// 错误处理的回调函数列表
	// WDATAERR, STICKYERR, STICKYCMP, STICKYORUN
	void (*stickyErrHandle)(DAPObject *dapObj);
	// DAP指令队列
	struct list_head instrQueue, retryQueue;	// DAP指令队列，元素类型：struct DAP_Instr
	//int instrQueue_len, retryQueue_len;	// 指令队列长度
	//int RDBUFF_Cnt;		// 需要插入多少个RDBUFF
};

// DAP指令队列
struct DAP_Instr{
	struct list_head list_entry;	// 链表节点
	union {
		struct {
			uint32_t APnDP:1;	// 0 = Debug Port (DP), 1 = Access Port (AP).
			uint32_t RnW:1;		// 0 = Write Register, 1 = Read Register.
			uint32_t A:2;		// A[3:2] Register Address.
			uint32_t :5;		// Padding
			uint32_t ack:3;		// 当前指令的应答
			uint32_t end:1;	// 下一个需要读取RDBUFF
		} info;
		uint8_t data;
	}seq;
	// 指令的数据
	union {
		uint32_t in;
		uint32_t *out;
	} data;
	int buffIdx;	// 指令数据缓冲地址
};

// packed transfer
union PackedTransferData{
	uint32_t data_32;
	uint16_t data_16[2];
	uint8_t data_8[4];
};

// 获得当前AP
#define DAP_CURR_AP(pd) ((pd)->SelectReg.info.AP_Sel)

// 构造和析构函数
BOOL __CONSTRUCT(DAP)(DAPObject *dapObj, AdapterObject *adapterObj);
void __DESTORY(DAP)(DAPObject *dapObj);
// 设置DAP的TAP索引位置
#define DAP_Set_TAP_Index(p,idx) ((p)->TAP_index = (idx))
// 设置DAP的WAIT重试次数
#define DAP_SetRetry(p,cnt) ((p)->retry = (cnt))
// 设置错误标志
#define DAP_SetErrorHandle(p,fun) ((p)->stickyErrHandle = (fun))

BOOL DAP_DP_Read(DAPObject *dapObj, uint8_t reg, uint32_t *data_out);
BOOL DAP_DP_Write(DAPObject *dapObj, uint8_t reg, uint32_t data);
BOOL DAP_AP_Read(DAPObject *dapObj, uint8_t reg, uint32_t *data_out);
BOOL DAP_AP_Write(DAPObject *dapObj, uint8_t reg, uint32_t data);
BOOL DAP_AP_Select(DAPObject *dapObj, uint8_t apIdx);
// 写入Abort寄存器
BOOL DAP_WriteAbort(DAPObject *dapObj, uint32_t abort);
// 检查状态标志
BOOL DAP_CheckStatus(DAPObject *dapObj, BOOL updateOnly);
// 清除粘性标志
BOOL DAP_ClearStickyFlag(DAPObject *dapObj, uint32_t flags);
// 寻找指定类型的AP
BOOL DAP_Find_AP(DAPObject *dapObj, enum ap_type apType, uint8_t *apIdx_Out);

// 内存读取操作
BOOL DAP_ReadMem8(DAPObject *dapObj, uint64_t addr, uint8_t *data_out);
BOOL DAP_ReadMem16(DAPObject *dapObj, uint64_t addr, uint16_t *data_out);
BOOL DAP_ReadMem32(DAPObject *dapObj, uint64_t addr, uint32_t *data_out);
/*
 * TODO 完善下面这些函数
BOOL DAP_ReadMem64(DAPObject *dapObj, uint64_t addr, uint64_t *data_out);

// 内存写入操作
BOOL DAP_WriteMem8(DAPObject *dapObj, uint64_t addr, uint8_t data);
BOOL DAP_WriteMem16(DAPObject *dapObj, uint64_t addr, uint16_t data);
BOOL DAP_WriteMem32(DAPObject *dapObj, uint64_t addr, uint32_t data);
BOOL DAP_WriteMem64(DAPObject *dapObj, uint64_t addr, uint64_t data);
*/

BOOL DAP_Read_CID_PID(DAPObject *dapObj, uint32_t componentBase, uint32_t *cid_out, uint64_t *pid_out);

// 队列操作，只在SWD模式下可用
BOOL DAP_Queue_xP_Read(DAPObject *dapObj, enum queue_apndp APnDP, uint8_t reg, uint32_t *data_out);
BOOL DAP_Queue_xP_Write(DAPObject *dapObj, enum queue_apndp APnDP, uint8_t reg, uint32_t data);
BOOL DAP_Queue_Execute(DAPObject *dapObj);

#endif /* SRC_ARCH_CORESIGHT_DAP_H_ */
