/*
 * cmsis-dap.h
 *
 *  Created on: 2018-2-19
 *      Author: virusv
 */

#ifndef SRC_ADAPTER_CMSIS_DAP_CMSIS_DAP_H_
#define SRC_ADAPTER_CMSIS_DAP_CMSIS_DAP_H_

#include "smart_ocd.h"
#include "USB/include/USB.h"
#include "adapter/adapter_private.h"

// CMSIS-DAP Command IDs
// V1.0
#define CMDAP_ID_DAP_Info                     0x00U
#define CMDAP_ID_DAP_HostStatus               0x01U
#define CMDAP_ID_DAP_Connect                  0x02U
#define CMDAP_ID_DAP_Disconnect               0x03U
#define CMDAP_ID_DAP_TransferConfigure        0x04U
#define CMDAP_ID_DAP_Transfer                 0x05U
#define CMDAP_ID_DAP_TransferBlock            0x06U
#define CMDAP_ID_DAP_TransferAbort            0x07U
#define CMDAP_ID_DAP_WriteABORT               0x08U
#define CMDAP_ID_DAP_Delay                    0x09U
#define CMDAP_ID_DAP_ResetTarget              0x0AU
#define CMDAP_ID_DAP_SWJ_Pins                 0x10U
#define CMDAP_ID_DAP_SWJ_Clock                0x11U
#define CMDAP_ID_DAP_SWJ_Sequence             0x12U
#define CMDAP_ID_DAP_SWD_Configure            0x13U
#define CMDAP_ID_DAP_JTAG_Sequence            0x14U
#define CMDAP_ID_DAP_JTAG_Configure           0x15U
#define CMDAP_ID_DAP_JTAG_IDCODE              0x16U
// V1.1
#define CMDAP_ID_DAP_SWO_Transport            0x17U
#define CMDAP_ID_DAP_SWO_Mode                 0x18U
#define CMDAP_ID_DAP_SWO_Baudrate             0x19U
#define CMDAP_ID_DAP_SWO_Control              0x1AU
#define CMDAP_ID_DAP_SWO_Status               0x1BU
#define CMDAP_ID_DAP_SWO_Data                 0x1CU

#define CMDAP_ID_DAP_QueueCommands            0x7EU
#define CMDAP_ID_DAP_ExecuteCommands          0x7FU

// DAP Vendor Command IDs
#define CMDAP_ID_DAP_Vendor0                  0x80U
#define CMDAP_ID_DAP_Vendor1                  0x81U
#define CMDAP_ID_DAP_Vendor2                  0x82U
#define CMDAP_ID_DAP_Vendor3                  0x83U
#define CMDAP_ID_DAP_Vendor4                  0x84U
#define CMDAP_ID_DAP_Vendor5                  0x85U
#define CMDAP_ID_DAP_Vendor6                  0x86U
#define CMDAP_ID_DAP_Vendor7                  0x87U
#define CMDAP_ID_DAP_Vendor8                  0x88U
#define CMDAP_ID_DAP_Vendor9                  0x89U
#define CMDAP_ID_DAP_Vendor10                 0x8AU
#define CMDAP_ID_DAP_Vendor11                 0x8BU
#define CMDAP_ID_DAP_Vendor12                 0x8CU
#define CMDAP_ID_DAP_Vendor13                 0x8DU
#define CMDAP_ID_DAP_Vendor14                 0x8EU
#define CMDAP_ID_DAP_Vendor15                 0x8FU
#define CMDAP_ID_DAP_Vendor16                 0x90U
#define CMDAP_ID_DAP_Vendor17                 0x91U
#define CMDAP_ID_DAP_Vendor18                 0x92U
#define CMDAP_ID_DAP_Vendor19                 0x93U
#define CMDAP_ID_DAP_Vendor20                 0x94U
#define CMDAP_ID_DAP_Vendor21                 0x95U
#define CMDAP_ID_DAP_Vendor22                 0x96U
#define CMDAP_ID_DAP_Vendor23                 0x97U
#define CMDAP_ID_DAP_Vendor24                 0x98U
#define CMDAP_ID_DAP_Vendor25                 0x99U
#define CMDAP_ID_DAP_Vendor26                 0x9AU
#define CMDAP_ID_DAP_Vendor27                 0x9BU
#define CMDAP_ID_DAP_Vendor28                 0x9CU
#define CMDAP_ID_DAP_Vendor29                 0x9DU
#define CMDAP_ID_DAP_Vendor30                 0x9EU
#define CMDAP_ID_DAP_Vendor31                 0x9FU

#define CMDAP_ID_DAP_Invalid                  0xFFU

// DAP Status Code
#define CMDAP_OK                          0U
#define CMDAP_ERROR                       0xFFU

// DAP ID
#define CMDAP_ID_VENDOR                   1U
#define CMDAP_ID_PRODUCT                  2U
#define CMDAP_ID_SER_NUM                  3U
#define CMDAP_ID_FW_VER                   4U
#define CMDAP_ID_DEVICE_VENDOR            5U
#define CMDAP_ID_DEVICE_NAME              6U
#define CMDAP_ID_CAPABILITIES             0xF0U
#define CMDAP_ID_SWO_BUFFER_SIZE          0xFDU	// V1.1
#define CMDAP_ID_PACKET_COUNT             0xFEU
#define CMDAP_ID_PACKET_SIZE              0xFFU

// DAP Host Status
#define CMDAP_DEBUGGER_CONNECTED          0U
#define CMDAP_TARGET_RUNNING              1U

// DAP Port
#define CMDAP_PORT_AUTODETECT             0U      // Autodetect Port
#define CMDAP_PORT_DISABLED               0U      // Port Disabled (I/O pins in High-Z)
#define CMDAP_PORT_SWD                    1U      // SWD Port (SWCLK, SWDIO) + nRESET
#define CMDAP_PORT_JTAG                   2U      // JTAG Port (TCK, TMS, TDI, TDO, nTRST) + nRESET

// DAP Transfer Response
#define CMDAP_TRANSFER_OK                 (1U<<0)
#define CMDAP_TRANSFER_WAIT               (1U<<1)
#define CMDAP_TRANSFER_FAULT              (1U<<2)
#define CMDAP_TRANSFER_ERROR              (1U<<3)
#define CMDAP_TRANSFER_MISMATCH           (1U<<4)

// DAP SWO Trace Mode	V1.1
#define DAP_SWO_OFF                     0U
#define DAP_SWO_UART                    1U
#define DAP_SWO_MANCHESTER              2U

// DAP SWO Trace Status	V1.1
#define CMDAP_SWO_CAPTURE_ACTIVE          (1U<<0)
#define CMDAP_SWO_CAPTURE_PAUSED          (1U<<1)
#define CMDAP_SWO_STREAM_ERROR            (1U<<6)
#define CMDAP_SWO_BUFFER_OVERRUN          (1U<<7)

// JTAG底层指令类型
enum JTAG_InstrType{
	JTAG_INS_STATUS_MOVE,	// 状态机改变状态
	JTAG_INS_EXCHANGE_DATA,	// 交换TDI-TDO数据
	JTAG_INS_IDLE_WAIT,		// 进入IDLE等待几个时钟周期
};

// JTAG指令对象
struct JTAG_Command{
	enum JTAG_InstrType type;	// JTAG指令类型
	struct list_head list_entry;	// JTAG指令链表对象
	// 指令结构共用体
	union {
		struct {
			enum JTAG_TAP_State toState;
		} statusMove;
		struct {
			uint8_t *data;	// 需要交换的数据地址
			unsigned int bitCount;	// 交换的二进制位个数
		} exchangeData;
		struct {
			unsigned int clkCount;	// 时钟个数
		} idleWait;
	} instr;
};

// DAP指令类型
enum DAP_InstrType{
	DAP_INS_RW_REG_SINGLE,	// 单次读写nP寄存器
	DAP_INS_RW_REG_MULTI,	// 多次读写寄存器
};

// DAP指令对象
struct DAP_Command{
	enum DAP_InstrType type;	// DAP指令类型
	struct list_head list_entry;	// DAP指令链表对象
	// 指令结构共用体
	union {
		struct {
			/**
			 * Bit 0: APnDP: 0 = Debug Port (DP), 1 = Access Port (AP).
			 * Bit 1: RnW: 0 = Write Register, 1 = Read Register.
			 * Bit 2: A2 Register Address bit 2.
			 * Bit 3: A3 Register Address bit 3.
			 */
			uint8_t request;
			// 指令的数据
			union {
				uint32_t write;	// 写单个寄存器
				uint32_t *read;
			} data;
		} singleReg;	// 单次读写寄存器
		struct {
			/**
			 * Bit 0: APnDP: 0 = Debug Port (DP), 1 = Access Port (AP).
			 * Bit 1: RnW: 0 = Write Register, 1 = Read Register.
			 * Bit 2: A2 Register Address bit 2.
			 * Bit 3: A3 Register Address bit 3.
			 */
			uint8_t request;
			// 指令的数据
			uint32_t *data;
			int count;	// 读写次数
		} multiReg;	// 多次读写寄存器
	} instr;
};

/* CMSIS-DAP对象 */
struct cmsis_dap {
	USB usbObj;	// USB连接对象
	struct adapter adaperAPI;	// Adapter接口对象
	BOOL inited;	// 是否已经初始化
	BOOL connected;	// USB设备是否已连接

	enum transfertMode currTransMode;	// 当前传输协议
	int Version;	// CMSIS-DAP 版本
	int MaxPcaketCount;	// 缓冲区最多容纳包的个数
	int PacketSize;	// 包最大长度
	uint8_t *respBuffer;	// 应答缓冲区
	uint32_t capablityFlag;	// 该仿真器支持的功能

	enum JTAG_TAP_State currState;	// JTAG 当前状态
	struct list_head JtagInsQueue;	// JTAG指令队列，元素类型：struct JTAG_Command
	struct list_head DapInsQueue;	// DAP指令队列，元素类型struct DAP_Command
	int tapIndex;	// TAP在扫描链中的索引,在DAP相关函数中会用到
	// TODO 实现更高版本仿真器支持 SWO、
};

/*
Available transfer protocols to target:
    Info0 - Bit 0: 1 = SWD Serial Wire Debug communication is implemented (0 = SWD Commands not implemented).
    Info0 - Bit 1: 1 = JTAG communication is implemented (0 = JTAG Commands not implemented).

Serial Wire Trace (SWO) support:
    Info0 - Bit 2: 1 = SWO UART - UART Serial Wire Output is implemented (0 = not implemented).
    Info0 - Bit 3: 1 = SWO Manchester - Manchester Serial Wire Output is implemented (0 = not implemented).

Command extensions for transfer protocol:
    Info0 - Bit 4: 1 = Atomic Commands - Atomic Commands support is implemented (0 = Atomic Commands not implemented).

Time synchronisation via Test Domain Timer:
    Info0 - Bit 5: 1 = Test Domain Timer - debug unit support for Test Domain Timer is implemented (0 = not implemented).

SWO Streaming Trace support:
    Info0 - Bit 6: 1 = SWO Streaming Trace is implemented (0 = not implemented).
 */
// CMSIS功能标志位
#define CMDAP_CAP_SWD					0
#define CMDAP_CAP_JTAG					1
#define CMDAP_CAP_SWO_UART				2
#define CMDAP_CAP_SWO_MANCHESTER		3
#define CMDAP_CAP_ATOMIC				4
#define CMDAP_CAP_SWD_SEQUENCE			5
#define CMDAP_CAP_TEST_DOMAIN_TIMER		6
#define CMDAP_CAP_TRACE_DATA_MANAGE		7

/**
 * 创建CMSIS-DAP对象
 * 返回:
 * 	Adapter对象
 */
Adapter CreateCmsisDap(void);

/**
 * 销毁CMSIS-DAP对象
 * 参数:
 * 	self:自身对象的指针!
 */
void DestroyCmsisDap(
		IN Adapter *self
);

/**
 * ConnectCmsisDap - 连接CMSIS-DAP仿真器
 * 参数:
 * 	vids: Vendor ID 列表
 * 	pids: Product ID 列表
 * 	serialNum:设备序列号
 * 返回:
 */
int ConnectCmsisDap(
		IN Adapter self,
		IN const uint16_t *vids,
		IN const uint16_t *pids,
		IN const char *serialNum
);

/**
 * DisconnectCmsisDap - 断开CMSIS-DAP设备
 *
 */
int DisconnectCmsisDap(
		IN Adapter self
);

/**
 * 发送line reset 指令
 * 只可用在SWD传输模式下
 */
int CmdapSwdLineReset(
		IN Adapter self
);

/**
 * 设置传输参数
 * 在调用DapTransfer和DapTransferBlock之前要先调用该函数
 * idleCycle：每一次传输后面附加的空闲时钟周期数
 * waitRetry：如果收到WAIT响应，重试的次数
 * matchRetry：如果在匹配模式下发现值不匹配，重试的次数
 * SWD、JTAG模式下均有效
 */
int CmdapTransferConfigure(
		IN Adapter self,
		IN uint8_t idleCycle,
		IN uint16_t waitRetry,
		IN uint16_t matchRetry
);

/**
 * 执行JTAG时序
 * sequenceCount:需要产生多少组时序
 * data ：时序表示数据
 * response：TDO返回数据
 */
int CmdapJtagSequence(
		IN Adapter self,
		IN int sequenceCount,
		IN uint8_t *data,
		OUT uint8_t *response
);

/**
 * 设置JTAG信息
 * count：扫描链中TAP的个数，不超过8个
 * irData：每个TAP的IR寄存器长度
 */
int CmdapJtagConfig(
		IN Adapter self,
		IN uint8_t count,
		IN uint8_t *irData
);

/**
 * SWD和JTAG模式下均有效
 * 具体手册参考CMSIS-DAP DAP_Transfer这一小节
 * sequenceCnt:要发送的Sequence个数
 * okSeqCnt：执行成功的Sequence个数
 * XXX：由于DAP指令可能会出错，出错之后要立即停止执行后续指令，所以就
 * 不使用批量功能 ,此函数内默认cmsis_dapObj->MaxPcaketCount = 1
 */
int CmdapTransfer(
		IN Adapter self,
		IN uint8_t index,
		IN int sequenceCnt,
		IN uint8_t *data,
		IN uint8_t *response,
		OUT int *okSeqCnt
);

/**
 * DAP_TransferBlock
 * 对单个寄存器进行多次读写，常配合地址自增使用
 * 参数列表和意义与DAP_Transfer相同
 */
int CmdapTransferBlock(
		IN Adapter self,
		IN uint8_t index,
		IN int sequenceCnt,
		IN uint8_t *data,
		OUT uint8_t *response,
		OUT int *okSeqCnt
);

/**
 * 配置SWD
 */
int CmdapSwdConfig(
		IN Adapter self,
		IN uint8_t cfg
);

/**
 * DAP写ABORT寄存器
 * The DAP_WriteABORT Command writes an abort request
 * to the CoreSight ABORT register of the Target Device.
 */
int CmdapWriteAbort(
		IN Adapter self,
		IN uint32_t data
);

#endif /* SRC_ADAPTER_CMSIS_DAP_CMSIS_DAP_H_ */
