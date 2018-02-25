/*
 * DAP.h
 *
 *  Created on: 2018-2-18
 *      Author: virusv
 */

#ifndef SRC_ARCH_CORESIGHT_DAP_H_
#define SRC_ARCH_CORESIGHT_DAP_H_

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


// Debug Port Register Addresses
#define DP_IDCODE                       0x00U   // IDCODE Register (SW Read only)
#define DP_ABORT                        0x00U   // Abort Register (SW Write only)
#define DP_CTRL_STAT                    0x04U   // Control & Status
#define DP_WCR                          0x04U   // Wire Control Register (SW Only)
#define DP_SELECT                       0x08U   // Select Register (JTAG R/W & SW W)
#define DP_RESEND                       0x08U   // Resend (SW Read Only)
#define DP_RDBUFF                       0x0CU   // Read Buffer (Read Only)

// Abort Register definitions
#define DAPABORT       0x00000001  // DAP Abort
#define STKCMPCLR      0x00000002  // Clear STICKYCMP Flag (SW Only)
#define STKERRCLR      0x00000004  // Clear STICKYERR Flag (SW Only)
#define WDERRCLR       0x00000008  // Clear WDATAERR Flag (SW Only)
#define ORUNERRCLR     0x00000010  // Clear STICKYORUN Flag (SW Only)

// Debug Control and Status definitions
#define ORUNDETECT     0x00000001  // Overrun Detect
#define STICKYORUN     0x00000002  // Sticky Overrun
#define TRNMODE        0x0000000C  // Transfer Mode Mask
#define TRNNORMAL      0x00000000  // Transfer Mode: Normal
#define TRNVERIFY      0x00000004  // Transfer Mode: Pushed Verify
#define TRNCOMPARE     0x00000008  // Transfer Mode: Pushed Compare
#define STICKYCMP      0x00000010  // Sticky Compare
#define STICKYERR      0x00000020  // Sticky Error
#define READOK         0x00000040  // Read OK (SW Only)
#define WDATAERR       0x00000080  // Write Data Error (SW Only)
#define MASKLANE       0x00000F00  // Mask Lane Mask
#define MASKLANE0      0x00000100  // Mask Lane 0
#define MASKLANE1      0x00000200  // Mask Lane 1
#define MASKLANE2      0x00000400  // Mask Lane 2
#define MASKLANE3      0x00000800  // Mask Lane 3
#define TRNCNT         0x001FF000  // Transaction Counter Mask
#define CDBGRSTREQ     0x04000000  // Debug Reset Request
#define CDBGRSTACK     0x08000000  // Debug Reset Acknowledge
#define CDBGPWRUPREQ   0x10000000  // Debug Power-up Request
#define CDBGPWRUPACK   0x20000000  // Debug Power-up Acknowledge
#define CSYSPWRUPREQ   0x40000000  // System Power-up Request
#define CSYSPWRUPACK   0x80000000  // System Power-up Acknowledge

// Debug Select Register definitions
#define CTRLSEL        0x00000001  // CTRLSEL (SW Only)
#define APBANKSEL      0x000000F0  // APBANKSEL Mask
#define APSEL          0xFF000000  // APSEL Mask

// Access Port Register Addresses
#define AP_CSW			0x00        // Control and Status Word
#define AP_TAR_LSB		0x04        // Transfer Address
#define AP_TAR_MSB		0x08
#define AP_DRW			0x0C        // Data Read/Write
#define AP_BD0			0x10        // Banked Data 0
#define AP_BD1			0x14        // Banked Data 1
#define AP_BD2			0x18        // Banked Data 2
#define AP_BD3			0x1C        // Banked Data 3
#define AP_CFG			0xF4		// CFG Register
#define AP_ROM_LSB		0xF8        // Debug ROM Address
#define AP_ROM_MSB		0xF0
#define AP_IDR			0xFC        // Identification Register

// AP Control and Status Word definitions
#define CSW_SIZE       0x00000007  // Access Size: Selection Mask
#define CSW_SIZE8      0x00000000  // Access Size: 8-bit
#define CSW_SIZE16     0x00000001  // Access Size: 16-bit
#define CSW_SIZE32     0x00000002  // Access Size: 32-bit
#define CSW_ADDRINC    0x00000030  // Auto Address Increment Mask
#define CSW_NADDRINC   0x00000000  // No Address Increment
#define CSW_SADDRINC   0x00000010  // Single Address Increment
#define CSW_PADDRINC   0x00000020  // Packed Address Increment
#define CSW_DBGSTAT    0x00000040  // Debug Status
#define CSW_TINPROG    0x00000080  // Transfer in progress
#define CSW_HPROT      0x02000000  // User/Privilege Control
#define CSW_MSTRTYPE   0x20000000  // Master Type Mask
#define CSW_MSTRCORE   0x00000000  // Master Type: Core
#define CSW_MSTRDBG    0x20000000  // Master Type: Debug
#define CSW_RESERVED   0x01000000  // Reserved Value

// Core Debug Register Address Offsets
#define DBG_OFS        0x0DF0      // Debug Register Offset inside NVIC
#define DBG_HCSR_OFS   0x00        // Debug Halting Control & Status Register
#define DBG_CRSR_OFS   0x04        // Debug Core Register Selector Register
#define DBG_CRDR_OFS   0x08        // Debug Core Register Data Register
#define DBG_EMCR_OFS   0x0C        // Debug Exception & Monitor Control Register

// Core Debug Register Addresses
#define DBG_HCSR       (DBG_Addr + DBG_HCSR_OFS)
#define DBG_CRSR       (DBG_Addr + DBG_CRSR_OFS)
#define DBG_CRDR       (DBG_Addr + DBG_CRDR_OFS)
#define DBG_EMCR       (DBG_Addr + DBG_EMCR_OFS)

// Debug Halting Control and Status Register definitions
#define C_DEBUGEN      0x00000001  // Debug Enable
#define C_HALT         0x00000002  // Halt
#define C_STEP         0x00000004  // Step
#define C_MASKINTS     0x00000008  // Mask Interrupts
#define C_SNAPSTALL    0x00000020  // Snap Stall
#define S_REGRDY       0x00010000  // Register R/W Ready Flag
#define S_HALT         0x00020000  // Halt Flag
#define S_SLEEP        0x00040000  // Sleep Flag
#define S_LOCKUP       0x00080000  // Lockup Flag
#define S_RETIRE_ST    0x01000000  // Sticky Retire Flag
#define S_RESET_ST     0x02000000  // Sticky Reset Flag
#define DBGKEY         0xA05F0000  // Debug Key

// Debug Exception and Monitor Control Register definitions
#define VC_CORERESET   0x00000001  // Reset Vector Catch
#define VC_MMERR       0x00000010  // Debug Trap on MMU Fault
#define VC_NOCPERR     0x00000020  // Debug Trap on No Coprocessor Fault
#define VC_CHKERR      0x00000040  // Debug Trap on Checking Error Fault
#define VC_STATERR     0x00000080  // Debug Trap on State Error Fault
#define VC_BUSERR      0x00000100  // Debug Trap on Bus Error Fault
#define VC_INTERR      0x00000200  // Debug Trap on Interrupt Error Fault
#define VC_HARDERR     0x00000400  // Debug Trap on Hard Fault
#define MON_EN         0x00010000  // Monitor Enable
#define MON_PEND       0x00020000  // Monitor Pend
#define MON_STEP       0x00040000  // Monitor Step
#define MON_REQ        0x00080000  // Monitor Request
#define TRCENA         0x01000000  // Trace Enable (DWT, ITM, ETM, TPIU)

// JTAG IR Codes
#define JTAG_ABORT                      0x08U
#define JTAG_DPACC                      0x0AU
#define JTAG_APACC                      0x0BU
#define JTAG_IDCODE                     0x0EU
#define JTAG_BYPASS                     0x0FU

// JTAG Sequence Info
#define JTAG_SEQUENCE_TCK               0x3FU   // TCK count	0b0011 1111
#define JTAG_SEQUENCE_TMS               0x40U   // TMS value	0b0100 0000
#define JTAG_SEQUENCE_TDO               0x80U   // TDO capture	0b1000 0000

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
} DP_IDRParse;
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
} AP_IDRParse;

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
} MEM_AP_CSWParse;

#endif /* SRC_ARCH_CORESIGHT_DAP_H_ */
