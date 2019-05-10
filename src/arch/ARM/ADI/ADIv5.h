/*
 * ADIv5.h
 *
 *  Created on: 2018年6月10日
 *      Author: virusv
 */

#ifndef SRC_ARCH_ARM_ADI_ADIV5_H_
#define SRC_ARCH_ARM_ADI_ADIV5_H_

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

#endif /* SRC_ARCH_ARM_ADI_ADIV5_H_ */
