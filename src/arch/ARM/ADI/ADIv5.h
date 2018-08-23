/*
 * ADIv5.h
 *
 *  Created on: 2018年6月10日
 *      Author: virusv
 */

#ifndef SRC_ARCH_ARM_ADI_ADIV5_H_
#define SRC_ARCH_ARM_ADI_ADIV5_H_

// JTAG扫描链的IR Code
#define JTAG_ABORT				0x08U
#define JTAG_DPACC				0x0AU
#define JTAG_APACC				0x0BU
#define JTAG_IDCODE				0x0EU
#define JTAG_BYPASS				0x0FU

// JTAG 应答
#define JTAG_RESP_OK_FAULT		0x02	// OK/FAULT
#define JTAG_RESP_WAIT			0x01	// WAIT
/**
 * 注意，所有寄存器格式是 bank[8:4], A[3:2]低二位RES0
 */
// DP寄存器地址 bit0表示APnDP
// DPv0只有JTAG模式，所以没有Abort寄存器，也没有IDCODE/DPIDR寄存器，都有一个单独的扫描链
enum DP_Regs{
	// DPv0下有如下寄存器
	CTRL_STAT	= 0x04U,	// Control & Status
	SELECT		= 0x08U,	// Select Register (JTAG R/W & DPv0 SW W)
	RDBUFF		= 0x0CU,	// Read Buffer (Read Only)
	// DPv1增加下列寄存器
	DPIDR		= 0x00U,	// IDCODE Register (SW Read only)
	ABORT		= 0x00U,	// Abort Register (SW Write only)
	DLCR		= 0x14U,	// Data Link Control Register(SW Only RW)
	RESEND		= 0x08U,	// Read Resend Register (SW Read Only)
	// DPv2增加下列寄存器
	TARGETID	= 0x24U,	// Target Identification register (RO)
	DLPIDR		= 0x34U,	// Data Link Protocol Identification Register (SW RO)
	EVENTSTAT	= 0x44U,	// Event Status register (RO)
	TARGETSEL	= 0x0CU,	// Target Selection register (SW WO, SWD Protocol v2才存在)
};

#define IS_DP_REG(reg) ((reg) == CTRL_STAT || (reg) == SELECT || (reg) == RDBUFF || (reg) == DPIDR || \
		(reg) == ABORT || (reg) == DLCR || (reg) == RESEND || (reg) == TARGETID || (reg) == DLPIDR \
		 || (reg) == EVENTSTAT || (reg) == TARGETSEL)

// AP 寄存器: bit0表示APnDP
enum AP_Regs{
	CSW		= 0x01,		// Control and Status Word
	TAR_LSB = 0x05,		// Transfer Address
	TAR_MSB = 0x09,
	DRW		= 0x0D,		// Data Read/Write
	BD0		= 0x11,		// Banked Data 0
	BD1		= 0x15,		// Banked Data 1
	BD2		= 0x19,		// Banked Data 2
	BD3		= 0x1D,		// Banked Data 3
	ROM_MSB	= 0xF1,		// Debug ROM Address MSByte
	CFG		= 0xF5,		// CFG Register
	ROM_LSB	= 0xF9,		// Debug ROM Address LSByte
	IDR		= 0xFD,		// Identification Register
};

#define IS_AP_REG(reg) ((reg) == CSW || (reg) == TAR_LSB || (reg) == TAR_MSB || (reg) == DRW || (reg) == BD0 || (reg) == BD1 || \
		(reg) == BD2 || (reg) == BD3 || (reg) == CFG || (reg) == ROM_LSB || (reg) == ROM_MSB || (reg) == IDR)

#endif /* SRC_ARCH_ARM_ADI_ADIV5_H_ */
