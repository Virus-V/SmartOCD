/*
 * adapter.h
 * SmartOCD
 *
 *  Created on: 2018-1-29
 *      Author: virusv
 */

#ifndef SRC_DEBUGGER_ADAPTER_H_
#define SRC_DEBUGGER_ADAPTER_H_

#include <stdarg.h>
#include "smart_ocd.h"
#include "misc/list.h"
#include "lib/JTAG.h"
#include "arch/ARM/ADI/ADIv5.h"
#include "arch/ARM/ADI/DAP.h"

// SW和JTAG引脚映射，用在JTAG_INS_RW_PINS指令里
#define SWJ_PIN_SWCLK_TCK		0       // SWCLK/TCK
#define SWJ_PIN_SWDIO_TMS		1       // SWDIO/TMS
#define SWJ_PIN_TDI				2       // TDI
#define SWJ_PIN_TDO				3       // TDO
#define SWJ_PIN_nTRST			5       // nTRST
#define SWJ_PIN_nRESET			7       // nRESET

// 仿真底层通信方式
enum transportType {
	UNKNOW_NULL = 0,	// 未知协议，由仿真器默认指定
	JTAG,
	SWD
};

// JTAG指令类型
enum JTAG_InstrType{
	/**
	 * 状态机改变状态
	 */
	JTAG_INS_STATUS_MOVE,
	/**
	 * 交换TDI-TDO数据
	 */
	JTAG_INS_EXCHANGE_IO,
	/**
	 * 进入IDLE等待几个时钟周期
	 */
	JTAG_INS_IDLE_WAIT,
};

// DAP指令类型
enum DAP_InstrType{
	/**
	 * 读写单个nP寄存器
	 */
	DAP_INS_RW_REG_SINGLE,
	/**
	 * 读写多个寄存器
	 */
	DAP_INS_RW_REG_BLOCK,
};

// JTAG状态机切换指令
struct JTAG_StatusMove{
	TMS_SeqInfo seqInfo;
};

// JTAG交换IO
struct JTAG_ExchangeIO{
	uint8_t *data;	// 需要交换的数据地址
	int bitNum;	// 交换的二进制位个数
};

// 进入IDLE状态等待几个周期
struct JTAG_IDLE_Wait{
	int wait;	// 等待的时钟周期数
};

// JTAG指令
struct JTAG_Command{
	enum JTAG_InstrType type;	// JTAG指令类型
	struct list_head list_entry;	// 链表对象
	// 指令结构共用体
	union {
		struct JTAG_StatusMove statusMove;
		struct JTAG_ExchangeIO exchangeIO;
		struct JTAG_IDLE_Wait idleWait;
	} instr;
};

// DAP读写单个寄存器指令结构体
struct DAP_RW_RegisterSingle{
	uint32_t RnW:1;	// 读或者写
	uint32_t APnDP:1;	// AP或者DP
	int reg;	// 寄存器
	// 指令的数据
	union {
		uint32_t write;	// 写单个寄存器
		uint32_t *read;
	} data;
};

// DAP读写多个寄存器指令结构体
struct DAP_RW_RegisterBlock{
	uint32_t RnW:1;	// 读或者写
	uint32_t APnDP:1;	// AP或者DP
	int reg;
	// 指令的数据
	union {
		uint32_t *read, *write;
	} data;
	int blockCnt;	// 块传输个数
};

// DAP指令
struct DAP_Command{
	enum DAP_InstrType type;	// DAP指令类型
	struct list_head list_entry;
	// 指令结构共用体
	union {
		struct DAP_RW_RegisterSingle RWRegSingle;	// 读写单个寄存器
		struct DAP_RW_RegisterBlock RWRegBlock;	// 读写多个寄存器
	} instr;
};

// 仿真器的公共操作命令，每个仿真器都必须实现这些
enum commonOperation {
	/**
	 * 设置仿真器状态
	 * 参数：int status 状态
	 * 返回：成功 TRUE，失败 FALSE
	 */
	AINS_SET_STATUS,
	/**
	 * 设置最大通信时钟频率
	 * 参数：uint32_t clock 频率，单位Hz
	 * 返回：成功 TRUE，失败 FALSE
	 */
	AINS_SET_CLOCK,
	/**
	 * 执行复位操作
	 * BOOL hard：是否执行硬复位
	 * BOOL srst：是否系统复位
	 * int pinWait：死区时间
	 */
	AINS_RESET,
	/**
	 * 读写JTAG-SWD引脚电平
	 * pinSelect：要写入的引脚
	 * 引脚映射：
	 *  Bit 0: SWCLK/TCK
	 *  Bit 1: SWDIO/TMS
	 *  Bit 2: TDI
	 *  Bit 3: TDO
	 *  Bit 5: nTRST
	 *  Bit 7: nRESET
	 * pinData：引脚数据，数据位映射如上，写入后读取引脚状态写回。
	 * pinWait：引脚等待时间。
	 * 引脚等待时间在nRESET引脚实现为开漏输出的系统中非常有用。
	 * 调试器释放nRESET后，外部电路仍然可以将目标器件保持在复位状态一段时间。
	 * 使用引脚等待时间，调试器可能会监视选定的I/O引脚并等待，直到它们出现预期值或超时。
	 */
	AINS_JTAG_PINS,
	/**
	 * 执行命令队列
	 */
	AINS_JTAG_CMD_EXECUTE,	// 解析并执行JTAG队列命令
	/**
	 * 写DAP ABORT寄存器
	 */
	AINS_DAP_WRITE_ABOTR,
	AINS_DAP_CMD_EXECUTE,	// 解析并执行DAP队列命令
	AINS_COMM_LAST	// 分隔符，代表最后一个
};

// CMSIS-DAP Status灯
enum adapterStatus {
	ADAPTER_STATUS_CONNECTED,	// Adapter已连接
	ADAPTER_STATUS_DISCONNECT,	// Adapter已断开
	ADAPTER_STATUS_RUNING,		// Adapter正在运行
	ADAPTER_STATUS_IDLE,		// Adapter空闲
};

// TAP对象
struct TAP {
	enum JTAG_TAP_Status currentStatus;	// TAP状态机当前状态
	uint16_t TAP_Count, *TAP_Info;
	struct list_head instrQueue;	// JTAG指令队列，元素类型：struct JTAG_Instr
	int IR_Bits;	// IR寄存器链一共多少位
};

// AP对象 TODO 没研究过JTAG AP，以后添加。
struct ap{
	//uint8_t apIdx;	// 当前ap的索引
	struct {
		uint8_t init:1;	// 该AP是否初始化过
		uint8_t largeAddress:1;	// 该AP是否支持64位地址访问，如果支持，则TAR和ROM寄存器是64位
		uint8_t largeData:1;	// 是否支持大于32位数据传输
		uint8_t bigEndian:1;	// 是否是大端字节序，ADI5.2废弃该功能，所以该位必须为0
		uint8_t packedTransfers:1;	// 是否支持packed传输
		uint8_t lessWordTransfers:1;	// 是否支持小于1个字的传输
	} ctrl_state;
	MEM_AP_CSW_Parse CSW;	// CSW 寄存器
};

// DAP对象
struct DAP{
	// uint8_t DP_Version;	// DP版本号
	struct ap AP[256];	// AP列表
	uint32_t ir;	// 当前ir寄存器
	uint16_t DAP_Index;	// JTAG扫描链中DAP的索引，从0开始
	DP_CTRL_STATUS_Parse CTRL_STAT_Reg;	// 当前CTRL/STAT寄存器
	DP_SELECT_Parse SELECT_Reg;	// SELECT寄存器
	int Retry;	// 接收到WAIT应答时重试次数
	int IdleCycle;	// UPDATE-DR后需要等待的时钟周期数，0表示不等待
	// DAP指令队列
	struct list_head instrQueue;	// DAP指令队列，元素类型struct DAP_Command
};

//仿真器对象
typedef struct AdapterObject AdapterObject;
/**
 * Adapter仿真器 为上位机提供控制调试对象方法的一个中间件
 * 仿真器提供一种或多种仿真协议(Transport)支持，比如SWD、JTAG等等
 */
struct AdapterObject{
	char *DeviceDesc;	// 设备描述字符
	enum transportType currTrans;	// 当前使用的仿真协议
	const enum transportType *supportedTrans;	// 支持的仿真协议列表（不能出现UNKNOW）
	struct TAP tap;	// TAP对象
	struct DAP dap;	// DAP对象
	BOOL isInit;	// 是否已经初始化
	BOOL (*Init)(AdapterObject *adapterObj);	// 连接仿真器，并执行初始化
	BOOL (*Deinit)(AdapterObject *adapterObj);	// 反初始化：断开仿真器
	BOOL (*SelectTrans)(AdapterObject *adapterObj, enum transportType type);	// 选择transport类型(当仿真器支持多个transport时)
	BOOL (*Operate)(AdapterObject *adapterObj, int operate, ...);	// 执行动作
	void (*Destroy)(AdapterObject *adapterObj);	// XXX 销毁该对象，释放相关资源
};

// 获得子类的Adapter对象
#define GET_ADAPTER(p) CAST(AdapterObject *, &(p)->AdapterObj)
// 返回仿真器当前激活的传输类型
#define ADAPTER_CURR_TRANS(p) ((p)->currTrans)

/**
 * 在一串数据中获得第n个二进制位，n从0开始
 * lsb
 * data:数据存放的位置指针
 * n:第几位，最低位是第0位
 */
#define GET_Nth_BIT(data,n) ((*(CAST(uint8_t *, (data)) + ((n)>>3)) >> ((n) & 0x7)) & 0x1)
/**
 * 设置data的第n个二进制位
 * data:数据存放的位置
 * n：要修改哪一位，从0开始
 * val：要设置的位，0或者1，只使用最低位
 */
#define SET_Nth_BIT(data,n,val) do{	\
	uint8_t tmp_data = *(CAST(uint8_t *, (data)) + ((n)>>3)); \
	tmp_data &= ~(1 << ((n) & 0x7));	\
	tmp_data |= ((val) & 0x1) << ((n) & 0x7);	\
	*(CAST(uint8_t *, (data)) + ((n)>>3)) = tmp_data;	\
}while(0);


BOOL __CONSTRUCT(Adapter)(AdapterObject *object, const char *desc);
void __DESTORY(Adapter)(AdapterObject *object);

/**
 * 对仿真器的公共基础操作
 */
// 设置仿真器JTAG的CLK频率
BOOL adapter_SetClock(AdapterObject *adapterObj, uint32_t clockHz);
// 设置仿真器的传输方式
BOOL adapter_SelectTransmission(AdapterObject *adapterObj, enum transportType type);
// 判断是否支持某一个传输方式
BOOL adapter_HaveTransmission(AdapterObject *adapterObj, enum transportType type);
// 设置状态指示灯
BOOL adapter_SetStatus(AdapterObject *adapterObj, enum adapterStatus status);
// JTAG复位
BOOL adapter_Reset(AdapterObject *adapterObj, BOOL hard, BOOL srst, int pinWait);
/*
 * JTAG基础操作
 * 这些指令都是队列缓冲的
 * 调用adapter_JTAG_Execute才会真正执行
 */
// 状态机改变
BOOL adapter_JTAG_StatusChange(AdapterObject *adapterObj, enum JTAG_TAP_Status newStatus);
// 交换TDI和TDO的数据
BOOL adapter_JTAG_Exchange_IO(AdapterObject *adapterObj, uint8_t *dataPtr, int bitCount);
// 进入IDLE状态空转几个时钟周期，一般是执行UPDATE之后会产生耗时的操作，需要等待
BOOL adapter_JTAG_IdleWait(AdapterObject *adapterObj, int clkCnt);
// 执行JTAG队列中的指令
BOOL adapter_JTAG_Execute(AdapterObject *adapterObj);
// 清空JTAG指令队列
void adapter_JTAG_CleanCommandQueue(AdapterObject *adapterObj);

// 读写Pins 同步操作
BOOL adapter_JTAG_RW_Pins(AdapterObject *adapterObj, uint8_t pinSelect, uint8_t *pinData, int pinWait);

/**
 * 较高级别的JTAG TAP操作，在JTAG扫描链中有多个TAP时用到它，
 * 根据要操作的TAP在扫描链中的位置，它会在IR和DR前后自动填充一些数据
 * 在JTAG方式下实现DAP协议的时候会用到
 */
// 设置JTAG扫描链中TAP的信息
BOOL adapter_JTAG_Set_TAP_Info(AdapterObject *adapterObj, uint16_t tapCount, uint16_t *IR_Len);
BOOL adapter_JTAG_Wirte_TAP_IR(AdapterObject *adapterObj, uint16_t tapIndex, uint32_t IR_Data);
BOOL adapter_JTAG_Exchange_TAP_DR(AdapterObject *adapterObj, uint16_t tapIndex, uint8_t *DR_Data, int DR_BitCnt);

/**
 * DAP基础操作
 */
// 设置当前DAP所在的TAP
#define adapter_DAP_Index(pa,n) (pa)->dap.DAP_Index = (n)
// 获得当前AP
#define adapter_DAP_CURR_AP(pa) ((pa)->SelectReg.info.AP_Sel)
/**
 * TODO 完善一些检查，比如在JTAG模式下不可以访问写ABORT寄存器和读IDCODE寄存器等等
 */
// Single方式读写寄存器
BOOL adapter_DAP_RW_nP_Single(AdapterObject *adapterObj, int reg, BOOL read, BOOL ap, uint32_t data_in, uint32_t *data_out, BOOL updateSelect);
// 读DP寄存器
#define adapter_DAP_Read_DP_Single(ao,reg,data_out,update) ({\
		assert(IS_DP_REG(reg));	\
		adapter_DAP_RW_nP_Single((ao),(reg),TRUE,FALSE,0,(data_out),(update));	\
})

// 写DP寄存器
#define adapter_DAP_Write_DP_Single(ao,reg,data_in,update) ({\
		assert(IS_DP_REG(reg));	\
		adapter_DAP_RW_nP_Single((ao),(reg),FALSE,FALSE,(data_in),NULL,(update));	\
})

// 读AP寄存器
#define adapter_DAP_Read_AP_Single(ao,reg,data_out,update) ({\
		assert(IS_AP_REG(reg));	\
		adapter_DAP_RW_nP_Single((ao),(reg),TRUE,TRUE,0,(data_out),(update));	\
})

// 写AP寄存器
#define adapter_DAP_Write_AP_Single(ao,reg,data_in,update) ({\
		assert(IS_AP_REG(reg));	\
		adapter_DAP_RW_nP_Single((ao),(reg),FALSE,TRUE,(data_in),NULL,(update));	\
})

// Block方式读写寄存器
BOOL adapter_DAP_RW_nP_Block(AdapterObject *adapterObj, int reg, BOOL read, BOOL ap, uint32_t *dataIO, int blockCnt, BOOL updateSelect);
// 多次读DP寄存器
#define adapter_DAP_Read_DP_Block(ao,reg,data_out,cnt,update) ({\
		assert(IS_DP_REG(reg));	\
		adapter_DAP_RW_nP_Block((ao),(reg),TRUE,FALSE,(data_out),(cnt),(update));	\
})

// 多次写DP寄存器
#define adapter_DAP_Write_DP_Block(ao,reg,data_in,cnt,update) ({\
		assert(IS_DP_REG(reg));	\
		adapter_DAP_RW_nP_Block((ao),(reg),FALSE,FALSE,(data_in),(cnt),(update));	\
})

// 多次读AP寄存器
#define adapter_DAP_Read_AP_Block(ao,reg,data_out,cnt,update) ({\
		assert(IS_AP_REG(reg));	\
		adapter_DAP_RW_nP_Block((ao),(reg),TRUE,TRUE,(data_out),(cnt),(update));	\
})

// 多次写AP寄存器
#define adapter_DAP_Write_AP_Block(ao,reg,data_in,cnt,update) ({\
		assert(IS_AP_REG(reg));	\
		adapter_DAP_RW_nP_Block((ao),(reg),FALSE,TRUE,(data_in),(cnt),(update));	\
})

// 执行DAP队列中的指令
BOOL adapter_DAP_Execute(AdapterObject *adapterObj);

// 清空DAP队列指令
void adapter_DAP_CleanCommandQueue(AdapterObject *adapterObj);

// 同步操作 写ABORT寄存器或者ABORT扫描链
BOOL adapter_DAP_WriteAbortReg(AdapterObject *adapterObj, uint32_t data);

// 返回传输方式的字符串形式
const char *adapter_Transport2Str(enum transportType type);
const char *adapter_Status2Str(enum adapterStatus type);

#endif /* SRC_DEBUGGER_ADAPTER_H_ */
