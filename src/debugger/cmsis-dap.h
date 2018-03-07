/*
 * cmsis-dap.h
 *
 *  Created on: 2018-2-19
 *      Author: virusv
 */

#ifndef SRC_DEBUGGER_CMSIS_DAP_H_
#define SRC_DEBUGGER_CMSIS_DAP_H_

#include "debugger/adapter.h"

// DAP Command IDs
// V1.0
#define ID_DAP_Info                     0x00U
#define ID_DAP_HostStatus               0x01U
#define ID_DAP_Connect                  0x02U
#define ID_DAP_Disconnect               0x03U
#define ID_DAP_TransferConfigure        0x04U
#define ID_DAP_Transfer                 0x05U
#define ID_DAP_TransferBlock            0x06U
#define ID_DAP_TransferAbort            0x07U
#define ID_DAP_WriteABORT               0x08U
#define ID_DAP_Delay                    0x09U
#define ID_DAP_ResetTarget              0x0AU
#define ID_DAP_SWJ_Pins                 0x10U
#define ID_DAP_SWJ_Clock                0x11U
#define ID_DAP_SWJ_Sequence             0x12U
#define ID_DAP_SWD_Configure            0x13U
#define ID_DAP_JTAG_Sequence            0x14U
#define ID_DAP_JTAG_Configure           0x15U
#define ID_DAP_JTAG_IDCODE              0x16U
// V1.1
#define ID_DAP_SWO_Transport            0x17U
#define ID_DAP_SWO_Mode                 0x18U
#define ID_DAP_SWO_Baudrate             0x19U
#define ID_DAP_SWO_Control              0x1AU
#define ID_DAP_SWO_Status               0x1BU
#define ID_DAP_SWO_Data                 0x1CU

#define ID_DAP_QueueCommands            0x7EU
#define ID_DAP_ExecuteCommands          0x7FU

// DAP Vendor Command IDs
#define ID_DAP_Vendor0                  0x80U
#define ID_DAP_Vendor1                  0x81U
#define ID_DAP_Vendor2                  0x82U
#define ID_DAP_Vendor3                  0x83U
#define ID_DAP_Vendor4                  0x84U
#define ID_DAP_Vendor5                  0x85U
#define ID_DAP_Vendor6                  0x86U
#define ID_DAP_Vendor7                  0x87U
#define ID_DAP_Vendor8                  0x88U
#define ID_DAP_Vendor9                  0x89U
#define ID_DAP_Vendor10                 0x8AU
#define ID_DAP_Vendor11                 0x8BU
#define ID_DAP_Vendor12                 0x8CU
#define ID_DAP_Vendor13                 0x8DU
#define ID_DAP_Vendor14                 0x8EU
#define ID_DAP_Vendor15                 0x8FU
#define ID_DAP_Vendor16                 0x90U
#define ID_DAP_Vendor17                 0x91U
#define ID_DAP_Vendor18                 0x92U
#define ID_DAP_Vendor19                 0x93U
#define ID_DAP_Vendor20                 0x94U
#define ID_DAP_Vendor21                 0x95U
#define ID_DAP_Vendor22                 0x96U
#define ID_DAP_Vendor23                 0x97U
#define ID_DAP_Vendor24                 0x98U
#define ID_DAP_Vendor25                 0x99U
#define ID_DAP_Vendor26                 0x9AU
#define ID_DAP_Vendor27                 0x9BU
#define ID_DAP_Vendor28                 0x9CU
#define ID_DAP_Vendor29                 0x9DU
#define ID_DAP_Vendor30                 0x9EU
#define ID_DAP_Vendor31                 0x9FU

#define ID_DAP_Invalid                  0xFFU

// DAP Status Code
#define DAP_OK                          0U
#define DAP_ERROR                       0xFFU

// DAP ID
#define DAP_ID_VENDOR                   1U
#define DAP_ID_PRODUCT                  2U
#define DAP_ID_SER_NUM                  3U
#define DAP_ID_FW_VER                   4U
#define DAP_ID_DEVICE_VENDOR            5U
#define DAP_ID_DEVICE_NAME              6U
#define DAP_ID_CAPABILITIES             0xF0U
#define DAP_ID_SWO_BUFFER_SIZE          0xFDU	// V1.1
#define DAP_ID_PACKET_COUNT             0xFEU
#define DAP_ID_PACKET_SIZE              0xFFU

// DAP Host Status
#define DAP_DEBUGGER_CONNECTED          0U
#define DAP_TARGET_RUNNING              1U

// DAP Port
#define DAP_PORT_AUTODETECT             0U      // Autodetect Port
#define DAP_PORT_DISABLED               0U      // Port Disabled (I/O pins in High-Z)
#define DAP_PORT_SWD                    1U      // SWD Port (SWCLK, SWDIO) + nRESET
#define DAP_PORT_JTAG                   2U      // JTAG Port (TCK, TMS, TDI, TDO, nTRST) + nRESET

// DAP SWJ Pins
#define DAP_SWJ_SWCLK_TCK               0       // SWCLK/TCK
#define DAP_SWJ_SWDIO_TMS               1       // SWDIO/TMS
#define DAP_SWJ_TDI                     2       // TDI
#define DAP_SWJ_TDO                     3       // TDO
#define DAP_SWJ_nTRST                   5       // nTRST
#define DAP_SWJ_nRESET                  7       // nRESET

// DAP Transfer Request
#define DAP_TRANSFER_APnDP              (1U<<0)
#define DAP_TRANSFER_RnW                (1U<<1)
#define DAP_TRANSFER_A2                 (1U<<2)
#define DAP_TRANSFER_A3                 (1U<<3)
#define DAP_TRANSFER_MATCH_VALUE        (1U<<4)
#define DAP_TRANSFER_MATCH_MASK         (1U<<5)
#define DAP_TRANSFER_TIME_STAMP         (1U<<7)	// > V1.0

// DAP Transfer Response
#define DAP_TRANSFER_OK                 (1U<<0)
#define DAP_TRANSFER_WAIT               (1U<<1)
#define DAP_TRANSFER_FAULT              (1U<<2)
#define DAP_TRANSFER_ERROR              (1U<<3)
#define DAP_TRANSFER_MISMATCH           (1U<<4)

// DAP SWO Trace Mode	V1.1
#define DAP_SWO_OFF                     0U
#define DAP_SWO_UART                    1U
#define DAP_SWO_MANCHESTER              2U

// DAP SWO Trace Status	V1.1
#define DAP_SWO_CAPTURE_ACTIVE          (1U<<0)
#define DAP_SWO_CAPTURE_PAUSED          (1U<<1)
#define DAP_SWO_STREAM_ERROR            (1U<<6)
#define DAP_SWO_BUFFER_OVERRUN          (1U<<7)

// Capablities Flags
#define CAP_FLAG_SWD					0x1 << 0
#define CAP_FLAG_JTAG					0x1 << 1
#define CAP_FLAG_SWO_UART				0x1 << 2
#define CAP_FLAG_SWO_MANCHESTER			0x1 << 3
#define CAP_FLAG_ATOMIC					0x1 << 4
#define CAP_FLAG_SWD_SEQUENCE			0x1 << 5
#define CAP_FLAG_TEST_DOMAIN_TIMER		0x1 << 6
#define CAP_FLAG_TRACE_DATA_MANAGE		0x1 << 7

// 判断CMSIS-DAP是否支持某功能
#define CMSIS_DAP_HAS_CAPALITY(pc,cap) (((pc)->capablityFlag & (cap)) == (cap))

// CMSIS-DAP Status灯
enum cmsis_dapStatus {
	DAP_STATUS_CONNECTED,	// DAP已连接
	DAP_STATUS_DISCONNECT,	// DAP已断开
	DAP_STATUS_RUNING,		// DAP正在运行
	DAP_STATUS_IDLE,		// DAP空闲
};

// CMSIS-DAP操作指令
enum cmsis_dapInstr {
	CMDAP_TRANSFER_CONFIG = AINS_COMM_LAST,	// 接着公共指令排列
	CMDAP_TRANSFER,		// DAP_Transfer
	CMDAP_TRANSFER_BLOCK,	// DAP_TransferBlock
	CMDAP_WRITE_ABORT,	// 写入ABORT寄存器
	CMDAP_READ_DP_REG,	// 读取DP寄存器
	CMDAP_WRITE_DP_REG,	// 写入DP寄存器
	CMDAP_READ_AP_REG,	// 读取AP寄存器
	CMDAP_WRITE_AP_REG,	// 写入AP寄存器
	CMDAP_JTAG_IDCODE,		// 获得JTAG的IDCode
	CMDAP_JTAG_CONFIGURE,	// 设置JTAG设备的IR长度
};

struct cmsis_dap {
	AdapterObject AdapterObj;	// 对象名必须为AdapterObj，不可以为指针，而且必须在第一个
	int Version;	// CMSIS-DAP 版本
	int MaxPcaketCount;	// 缓冲区最多容纳包的个数
	int PacketSize;	// 包最大长度
	uint32_t capablityFlag;	// 该仿真器支持的功能
	// TODO 实现更高版本仿真器支持 SWO、
};

struct cmsis_dap *NewCMSIS_DAP();
void FreeCMSIS_DAP(struct cmsis_dap *cmsis_dapObj);
BOOL ConnectCMSIS_DAP(struct cmsis_dap *cmsis_dapObj, const uint16_t *vids, const uint16_t *pids, const char *serialNum);

#endif /* SRC_DEBUGGER_CMSIS_DAP_H_ */
