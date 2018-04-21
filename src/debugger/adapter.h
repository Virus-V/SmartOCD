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
#include "debugger/usb.h"

// 仿真通信方式
enum transportType {
	UNKNOW_NULL = 0,	// 未知协议，由仿真器默认指定
	JTAG,
	SWD
};

// 支持的仿真器类型
enum supportedAdapter{
	LEGACY,
	CMSIS_DAP,	// ARM 公司提供的一个仿真器
	USB_BLASTER,
	STLINK,
	OPENJTAG,
};

// 仿真器的公共操作命令
// XXX 提取出来公共操作，每个仿真器都必须实现这些
enum commonInstr {
	/**
	 * 设置仿真器状态
	 * 参数：int status 状态
	 * 返回：成功 TRUE，失败 FALSE
	 */
	AINS_SET_STATUS,
	/**
	 * 设置最大通信时钟
	 * 参数：uint32_t clock 频率，单位Hz
	 * 返回：成功 TRUE，失败 FALSE
	 */
	AINS_SET_CLOCK,
	/**
	 * 发送一个jtag时序
	 * 参数：int sequenceCount, uint8_t *data, uint8_t *response
	 * 其中：sequenceCount是一共需要发送多少个序列；
	 * data是一个数组，用来描述jtag时序。格式为：
	 * {(uint8_t)Sequence Info + (uint8_t)TDI Data...}...
	 * 每一个Sequence Info后面紧跟着一个或多个TDI Data，根据Sequence Info[5:0]来决定
	 * 后面跟多少个TDI Data。
	 * Sequence Info: Contains number of TDI bits and fixed TMS value
	 * Bit 5 .. 0: Number of TCK cycles: 1 .. 64 (64 encoded as 0)
	 * Bit 6: TMS value
	 * Bit 7: TDO Capture
	 *
	 * TDI Data: Data generated on TDI
	 * One bit for each TCK cycle
	 * LSB transmitted first, padded to BYTE boundary
	 * 举个栗子：
	 * 0x45, 0x00, 0x01, 0x00 => jtag将在TCK产生6个时钟周期；TMS序列为：111110，TDI序列为：000000
	 * 0x04, 0x0e => jtag将在TCK产生4个时钟周期；这4个时钟周期TMS为1，TDI序列为 0111
	 * 0xa0, 0x00, 0x00, 0x00, 0x00 => jtag将在TCK产生32个时钟周期，TMS为0，TDI为32个0，同时将TDO的数据一共32位放入response
	 *
	 * response是一个数组空间，用来保存输出的，具体大小需要根据你的指令去决定。如果不需要TDO则该参数为NULL。格式为：
	 * (uint8_t)TDO Data...
	 *
	 * TDO Data: Data captured from TDO
	 * One bit for each TCK cycle when TDO Capture is enabled
	 * LSB received first, padded to BYTE boundary
	 *
	 * 返回值：成功：TRUE，失败：FALSE
	 */
	AINS_JTAG_SEQUENCE,
	/**
	 * 读取或写入jtag的某个引脚
	 * 参数：uint8_t pinSelect, uint8_t pinOutput, uint8_t *pinInput, uint32_t pinWait
	 * 其中：pinSelect是选中哪几个输出引脚需要被修改。当该值为0也就是任何引脚没有被选中时，函数只读取
	 * 全部引脚，不对引脚进行操作。引脚映射为：
	 * Bit 0: SWCLK/TCK
	 * Bit 1: SWDIO/TMS
	 * Bit 2: TDI
	 * Bit 3: TDO
	 * Bit 5: nTRST
	 * Bit 7: nRESET
	 *
	 * pinOutput是引脚将要设置的值，引脚映射参见上面。
	 * pinInput是当pinOutput的数据实现（全部响应或者等待pinWait超时）之后，读取全部引脚当前的值。如果不需要读取则该参数为NULL。
	 * pinWait是死区等待时间(µs)。死区等待时间在nRESET引脚或其他引脚实现为开漏输出的系统中非常有用。
	 * 在调试器de-assert nRESET引脚之后，外部电路可能没有立即反应，仍然将目标器件保持在复位状态一段时间。
	 * 调试器可以监视选定的I/O引脚并等待，直到它们出现预期值或超时pinWait设置的死区时间。不可超过3秒
	 */
	AINS_JTAG_PINS,
	/**
	 * 发送一个或多个DAP协议时序，仿真器支持SWD模式时（ADAPTER_CAP_SWD置位）必须实现该方法
	 * 参数：uint8_t index, uint8_t transferCount, uint8_t *data, uint8_t *response
	 * 其中各参数：
	 * index: 从0开始算起JTAG扫描链中设备的索引值。在SWD模式下该值被忽略。
	 * transferCount: Number of transfers: 1 .. 255. For each transfer a Transfer Request BYTE is sent.
	 * 		Depending on the request an additional Transfer Data WORD is sent.
	 * data: Contains information about requested access from host debugger.
	 * 		Bit 0: APnDP: 0 = Debug Port (DP), 1 = Access Port (AP).
	 * 		Bit 1: RnW: 0 = Write Register, 1 = Read Register.
	 * 		Bit 2: A2 Register Address bit 2.
	 * 		Bit 3: A3 Register Address bit 3.
	 * response:是一个数组空间，用来保存输出的，具体大小需要根据你的指令去决定。
	 * 第一个字节：Transfer Count: Number of transfers: 1 .. 255 that are executed.
	 * 第二个字节：Transfer Response: Contains information about last response from target Device.
	 * 		Bit 2..0: ACK (Acknowledge) value:
	 * 		1 = OK (for SWD protocol), OK or FAULT (for JTAG protocol),
	 * 		2 = WAIT
	 * 		4 = FAULT
	 * 		7 = NO_ACK (no response from target)
	 * 		Bit 3: 1 = Protocol Error (SWD)
	 * Transfer Data: register value or match value in the order of the Transfer Request.
	 * 		for Read Register transfer request: the register value of the CoreSight register.
	 * 		no data is sent for other operations.
	 * 返回值：只有TRUE
	 */
	AINS_DAP_TRANSFER,
	AINS_COMM_LAST	// 分隔符，代表最后一个
};

enum connType {
	UNKNOW,	// 未知
	USB,	// 使用USB方式连接
	ETHERNAET,	// 以太网，Wifi
	SERIAL_PORT	// 串行口
};

// CMSIS-DAP Status灯
enum adapterStatus {
	ADAPTER_STATUS_CONNECTED,	// Adapter已连接
	ADAPTER_STATUS_DISCONNECT,	// Adapter已断开
	ADAPTER_STATUS_RUNING,		// Adapter正在运行
	ADAPTER_STATUS_IDLE,		// Adapter空闲
};

// 仿真器的连接方式，比如使用USB、以太网、串口等等
// 此处先实现使用usb协议的仿真器类型
struct adapterConnector{
	enum connType type;	// 指示当前的仿真器连接类型
	int connectFlag;	// 是否已连接
	union {	// 连接对象共用体
		USBObject usbObj;
	} object;
};

// 判断是否支持某些功能
#define ADAPTER_HAS_CAPALITY(p,flags) (((p)->capablityFlag & (flags)) == (flags))

// 设置标志位
// 如果ca的第bit位为1，则执行p->capablityFlag |= flag
#define ADAPTER_SET_CAP(p,ca,bit,flag) \
	if( ((ca) & (0x1 << (bit))) != 0) (p)->capablityFlag |= (flag);

// 返回仿真器激活的传输类型
#define ADAPTER_CURR_TRANS(p) ((p)->currTrans)

//仿真器对象
typedef struct AdapterObject AdapterObject;
/**
 * Adapter仿真器 为上位机提供控制调试对象方法的一个中间件
 * 仿真器提供一种或多种仿真协议(Transport)支持，比如SWD、JTAG等等
 */
struct AdapterObject{
	struct adapterConnector ConnObject;	// 仿真器连接对象
	char *DeviceDesc;	// 设备描述字符
	enum supportedAdapter AdapterClass;	// 仿真器类型
	enum transportType currTrans;	// 当前使用的仿真协议
	const enum transportType *supportedTrans;	// 支持的仿真协议列表（不能出现UNKNOW）
	BOOL (*Init)(AdapterObject *adapterObj);	// 执行初始化动作
	BOOL (*Deinit)(AdapterObject *adapterObj);	// 反初始化（告诉我怎么形容合适）
	BOOL (*SelectTrans)(AdapterObject *adapterObj, enum transportType type);	// 选择transport类型(当仿真器支持多个transport时)
	BOOL (*Operate)(AdapterObject *adapterObj, int operate, ...);	// 执行动作
};

// 获得USBObject的指针
#define GET_USBOBJ(pa) CAST(USBObject *, &(pa)->ConnObject.object.usbObj)
// 获得子类的Adapter对象
#define GET_ADAPTER(p) CAST(AdapterObject *, &(p)->AdapterObj)

BOOL __CONSTRUCT(Adapter)(AdapterObject *object, const char *desc);
void __DESTORY(Adapter)(AdapterObject *object);
// 设置仿真器JTAG的CLK频率
BOOL adapter_SetClock(AdapterObject *adapterObj, uint32_t clockHz);
// 设置仿真器的传输方式
BOOL adapter_SelectTransmission(AdapterObject *adapterObj, enum transportType type);
// 判断是否支持某一个传输方式
BOOL adapter_HaveTransmission(AdapterObject *adapterObj, enum transportType type);
// 设置状态指示灯
BOOL adapter_SetStatus(AdapterObject *adapterObj, enum adapterStatus status);
// 返回传输方式的字符串形式
const char *adapter_Transport2Str(enum transportType type);

#endif /* SRC_DEBUGGER_ADAPTER_H_ */
