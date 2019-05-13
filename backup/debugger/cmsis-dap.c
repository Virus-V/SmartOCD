/*
 * cmsis-dap.c
 *
 *  Created on: 2018-2-18
 *      Author: virusv
 */

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "misc/log.h"
#include "debugger/cmsis-dap.h"

#include "misc/misc.h"

/**
 * 这个宏是用来作为与cmsis-dap通信函数的外包部分
 * 该宏实现自动申请cmsis-dap返回包缓冲空间并且带函数执行结束后
 * 释放该空间
 */
#define CMDAP_FUN_WARP(pa,func,...) ({	\
	uint8_t *responseBuff; BOOL result = FALSE;	\
	struct cmsis_dap *cmsis_dapObj = CAST(struct cmsis_dap*, (pa));	\
	assert(cmsis_dapObj->PacketSize > 0);	\
	if((responseBuff = calloc(cmsis_dapObj->PacketSize, sizeof(uint8_t))) == NULL){	\
		log_warn("Alloc reply packet buffer memory failed.");	\
		result = FALSE;	\
	}else{	\
		result = (func)(responseBuff, pa, ##__VA_ARGS__);	\
		free(responseBuff);	\
	}	\
	result;	\
})

/**
 * 由多少个比特位构造出多少个字节，包括控制字
 * 比如bitCnt=64
 * 则返回9
 * 如果bitCnt=65
 * 则返回11。因为 发送65位需要两个控制字节了。
 */
#define BIT2BYTE(bitCnt) ({	\
	int byteCnt, tmp = (bitCnt) >> 6;	\
	byteCnt = tmp + (tmp << 3);	\
	tmp = (bitCnt) & 0x3f;	\
	byteCnt += tmp ? ((tmp + 7) >> 3) + 1 : 0;	\
})

// CMSIS-DAP支持的仿真传输协议类型
static const enum transportType supportTrans[] = {JTAG, SWD, UNKNOW_NULL};	// 必须以UNKNOW_NULL结尾
static BOOL init(AdapterObject *adapterObj);
static BOOL deinit(AdapterObject *adapterObj);
static BOOL selectTrans(AdapterObject *adapterObj, enum transportType type);
static BOOL operate(AdapterObject *adapterObj, int operate, ...);
static void destroy(AdapterObject *adapterObj);

/**
 * 创建新的CMSIS-DAP仿真器对象
 */
BOOL NewCMSIS_DAP(struct cmsis_dap *cmsis_dapObj){
	assert(cmsis_dapObj != NULL);
	AdapterObject *adapterObj = CAST(AdapterObject *, cmsis_dapObj);
	// 清空数据
	memset(cmsis_dapObj, 0x0, sizeof(struct cmsis_dap));
	// 构造Adapter对象
	if(__CONSTRUCT(Adapter)(adapterObj, "ARM CMSIS-DAP") == FALSE){
		log_warn("Failed to Init AdapterObject.");
		return FALSE;
	}
	// 构造USB对象
	if(__CONSTRUCT(USB)(&cmsis_dapObj->usbObj) == FALSE){
		__DESTORY(Adapter)(adapterObj);
		log_warn("Failed to create USB object.");
		return FALSE;
	}

	// 设置该设备支持的传输类型
	adapterObj->supportedTrans = supportTrans;
	adapterObj->currTrans = UNKNOW_NULL;
	// 配置方法函数
	adapterObj->Init = init;
	adapterObj->Deinit = deinit;
	adapterObj->SelectTrans = selectTrans;
	adapterObj->Operate = operate;
	adapterObj->Destroy = destroy;

	cmsis_dapObj->Connected = FALSE;
	return TRUE;
}

// 释放CMSIS-DAP对象
static void destroy(AdapterObject *adapterObj){
	assert(adapterObj != NULL);
	struct cmsis_dap *cmsis_dapObj = CAST(struct cmsis_dap *, adapterObj);
	// 关闭USB对象
	if(cmsis_dapObj->Connected == TRUE){
		USBClose(&cmsis_dapObj->usbObj);
		cmsis_dapObj->Connected = FALSE;
	}
	// 释放USB对象
	__DESTORY(USB)(&cmsis_dapObj->usbObj);
	// 反初始化Adapter对象
	__DESTORY(Adapter)(adapterObj);
}

// 搜索并连接CMSIS-DAP仿真器
BOOL Connect_CMSIS_DAP(struct cmsis_dap *cmsis_dapObj, const uint16_t *vids, const uint16_t *pids, const char *serialNum){
	assert(cmsis_dapObj != NULL);
	int idx = 0;
	//如果当前已连接则不重新连接
	if(cmsis_dapObj->Connected == TRUE) return TRUE;

	for(; vids[idx] && pids[idx]; idx++){
		log_debug("Try connecting vid: 0x%02x, pid: 0x%02x usb device.", vids[idx], pids[idx]);
		if(USBOpen(&cmsis_dapObj->usbObj, vids[idx], pids[idx], serialNum)){
			log_info("Successful connection vid: 0x%02x, pid: 0x%02x usb device.", vids[idx], pids[idx]);
			// 复位设备
			USBResetDevice(&cmsis_dapObj->usbObj);
			// 标志已连接
			cmsis_dapObj->Connected = TRUE;
			// 选择配置和声明接口
			if(USBSetConfiguration(&cmsis_dapObj->usbObj, 0) != TRUE){
				log_warn("USBSetConfiguration Failed.");
				return FALSE;
			}
			if(USBClaimInterface(&cmsis_dapObj->usbObj, 3, 0, 0, 3) != TRUE){
				log_warn("Claim interface failed.");
				return FALSE;
			}
			return TRUE;
		}
	}
	log_warn("No suitable device found.");
	return FALSE;
}

/**
 * 从仿真器读数据
 * buff是保存数据的缓冲区
 * 注意：该函数会读取固定大小 cmsis_dapObj->PacketSize
 */
static int DAPRead(AdapterObject *adapterObj, uint8_t *buff){
	assert(adapterObj != NULL);
	struct cmsis_dap *cmsis_dapObj = CAST(struct cmsis_dap *, adapterObj);
	assert(cmsis_dapObj->PacketSize != 0);
	int length = cmsis_dapObj->usbObj.Read(&cmsis_dapObj->usbObj, buff, cmsis_dapObj->PacketSize, 0);
	log_trace("Read %d byte(s) from %s.", length, adapterObj->DeviceDesc);
	return length;
}

/**
 * 从仿真器写数据
 * data数数据缓冲区指针
 * len是要发送的数据长度
 */
static int DAPWrite(AdapterObject *adapterObj, uint8_t *data, int len){
	assert(adapterObj != NULL);
	struct cmsis_dap *cmsis_dapObj = CAST(struct cmsis_dap *, adapterObj);
	int length =  cmsis_dapObj->usbObj.Write(&cmsis_dapObj->usbObj, data, len, 0);
	//log_debug("-------------------------------------------");
	//misc_PrintBulk(data, len, 16);
	//log_debug("-------------------------------------------");
	log_trace("Write %d byte(s) to %s.", length, adapterObj->DeviceDesc);
	return length;
}

#define DAP_EXCHANGE_DATA(pa,data,len,buff) \
	DAPWrite((pa), (data), (len)); \
	DAPRead((pa), (buff));

// 初始化CMSIS-DAP设备
static BOOL init(AdapterObject *adapterObj){
	assert(adapterObj != NULL);
	struct cmsis_dap *cmsis_dapObj = CAST(struct cmsis_dap *, adapterObj);
	// 判断是否已经执行过初始化
	if(adapterObj->isInit == TRUE){
		return TRUE;
	}
	char capablity;

	USBObject *usbObj = &cmsis_dapObj->usbObj;
	uint8_t command[2] = {ID_DAP_Info, DAP_ID_PACKET_SIZE}, *resp;	// 命令存储缓冲和返回值缓冲区
	if((resp = calloc(usbObj->readEPMaxPackSize, sizeof(uint8_t))) == NULL){
		log_warn("Alloc response buffer failed.");
		return FALSE;
	}
	// 获得DAP_Info 判断
	log_info("CMSIS-DAP Init.");
	// 先以endpoint最大包长读取packet大小，然后读取剩下的
	usbObj->Write(usbObj, command, 2, 0);
	usbObj->Read(usbObj, resp, usbObj->readEPMaxPackSize, 0);

	// 判断返回值是否是一个16位的数字
	if(resp[0] != 0 || resp[1] != 2){
		free(resp);
		log_warn("Get packet size failed.");
		return FALSE;
	}
	// 取得包长度
	cmsis_dapObj->PacketSize = *CAST(uint16_t *, resp+2);
	if(cmsis_dapObj->PacketSize == 0){
		free(resp);
		log_warn("Packet Size is Zero!!!");
		return FALSE;
	}
	// 重新分配缓冲区大小
	if((resp = realloc(resp, cmsis_dapObj->PacketSize * sizeof(uint8_t))) == NULL){
		log_warn("Alloc response buffer failed.");
		return FALSE;
	}
	log_info("CMSIS-DAP the maximum Packet Size is %d.", cmsis_dapObj->PacketSize);
	// 读取剩下的内容
	if(cmsis_dapObj->PacketSize > usbObj->readEPMaxPackSize){
		int rest_len = cmsis_dapObj->PacketSize - usbObj->readEPMaxPackSize;
		usbObj->Read(usbObj, resp, rest_len > usbObj->readEPMaxPackSize ? rest_len : usbObj->readEPMaxPackSize, 0);
	}
	// 获得CMSIS-DAP固件版本
	command[1] = DAP_ID_FW_VER;
	DAP_EXCHANGE_DATA(adapterObj, command, 2, resp);

	cmsis_dapObj->Version = (int)(atof(resp+2) * 100); // XXX 改成了整数
	log_info("CMSIS-DAP FW Version is %f.", cmsis_dapObj->Version / 100.0);

	// 获得CMSIS-DAP的最大包长度和最大包个数
	command[1] = DAP_ID_PACKET_COUNT;
	DAP_EXCHANGE_DATA(adapterObj, command, 2, resp);
	cmsis_dapObj->MaxPcaketCount = *CAST(uint8_t *, resp+2);
	log_info("CMSIS-DAP the maximum Packet Count is %d.", cmsis_dapObj->MaxPcaketCount);

	// Capabilities. The information BYTE contains bits that indicate which communication methods are provided to the Device.
	command[1] = DAP_ID_CAPABILITIES;
	DAP_EXCHANGE_DATA(adapterObj, command, 2, resp);

	// XXX Version改成了整数，乘以100
	switch(cmsis_dapObj->Version){
	case 110: // 1.1f
		// 获得info0
		capablity = *CAST(uint8_t *, resp+2);
		CMSIS_DAP_SET_CAP(cmsis_dapObj, capablity, 2, CMDAP_CAP_SWO_UART);
		CMSIS_DAP_SET_CAP(cmsis_dapObj, capablity, 3, CMDAP_CAP_SWO_MANCHESTER);
		CMSIS_DAP_SET_CAP(cmsis_dapObj, capablity, 4, CMDAP_CAP_ATOMIC);
		CMSIS_DAP_SET_CAP(cmsis_dapObj, capablity, 5, CMDAP_CAP_SWD_SEQUENCE);
		CMSIS_DAP_SET_CAP(cmsis_dapObj, capablity, 6, CMDAP_CAP_TEST_DOMAIN_TIMER);

		if(*CAST(uint8_t *, resp+1) == 2){	// 是否存在info1
			// 获得info1
			capablity = *CAST(uint8_t *, resp+3);
			CMSIS_DAP_SET_CAP(cmsis_dapObj, capablity, 0, CMDAP_CAP_TRACE_DATA_MANAGE);
		}
		/* no break */

	case 100: // 1.0f
		// 获得info0
		capablity = *CAST(uint8_t *, resp+2);
		CMSIS_DAP_SET_CAP(cmsis_dapObj, capablity, 0, CMDAP_CAP_SWD);
		CMSIS_DAP_SET_CAP(cmsis_dapObj, capablity, 1, CMDAP_CAP_JTAG);
		break;

	default: break;
	}
	log_info("CMSIS-DAP Capabilities 0x%X.", cmsis_dapObj->capablityFlag);
	// 在初始化过程中不选择transport
	adapterObj->currTrans = UNKNOW_NULL;
	free(resp);
	// 标记当前Adapter已经执行过初始化
	adapterObj->isInit = TRUE;
	return TRUE;
}

/**
 * Disconnect
 */
static BOOL DAP_Disconnect(uint8_t *respBuff, AdapterObject *adapterObj){
	assert(adapterObj != NULL);
	uint8_t command[1] = {ID_DAP_Disconnect};
	DAP_EXCHANGE_DATA(adapterObj, command, 1, respBuff);
	// 检查返回值
	if(respBuff[1] != DAP_OK){
		log_warn("Disconnect execution failed.");
		return FALSE;
	}
	return TRUE;
}

// 反初始化
static BOOL deinit(AdapterObject *adapterObj){
	assert(adapterObj != NULL);
	// 判断是否初始化
	if(adapterObj->isInit == FALSE){
		return TRUE;
	}
	// 断开连接
	if(CMDAP_FUN_WARP(adapterObj, DAP_Disconnect) == TRUE){
		adapterObj->isInit = FALSE;
		return TRUE;
	}else{
		return FALSE;
	}
}

/**
 * JTAG协议转SWD
 * 转换后自动增加一个lineReset操作
 */
static BOOL SWJ_JTAG2SWD(uint8_t *respBuff, AdapterObject *adapterObj){
	assert(adapterObj != NULL);
	//SWJ发送 0xE79E，老版本的ARM发送0xB76D强制切换
	// 12 76 FF FF FF FF FF FF 7B 9E FF FF FF FF FF FF 0F
	uint8_t switchSque[] = {ID_DAP_SWJ_Sequence, 136, // 144
			0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // 56 bit
			0x9e, 0xe7,	// 16 bit
			0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // 56 bit
			0x00,	// 8 bit
	};	// DAP_SWJ_Sequence Command
	DAP_EXCHANGE_DATA(adapterObj, switchSque, sizeof(switchSque), respBuff);
	return TRUE;
}

/**
 * SWD协议转JTAG
 *
 */
static BOOL SWJ_SWD2JTAG(uint8_t *respBuff, AdapterObject *adapterObj){
	assert(adapterObj != NULL);
	//SWJ发送 0xE79E，老版本的ARM发送0xB76D强制切换
	uint8_t switchSque[] = {ID_DAP_SWJ_Sequence, 84,
			0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // 56 bit
			0x3c, 0xe7,	// 16 bit
			0xff, 0x00,	// 8 bit
	};	// DAP_SWJ_Sequence Command
	DAP_EXCHANGE_DATA(adapterObj, switchSque, sizeof(switchSque), respBuff);
	return TRUE;
}

/**
 * line reset
 */
static BOOL SWD_LineReset(uint8_t *respBuff, AdapterObject *adapterObj){
	uint8_t resetSque[] = {ID_DAP_SWJ_Sequence, 55,
			0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x03, // 51 bit，后面跟一个0
	};	// DAP_SWJ_Sequence Command
	DAP_EXCHANGE_DATA(adapterObj, resetSque, sizeof(resetSque), respBuff);
	return TRUE;
}

// 选择和切换传输协议
static BOOL selectTrans(AdapterObject *adapterObj, enum transportType type){
	int idx;
	assert(adapterObj != NULL);
	struct cmsis_dap *cmsis_dapObj = CAST(struct cmsis_dap *, adapterObj);
	assert(cmsis_dapObj->PacketSize != 0);

	uint8_t command[2] = {ID_DAP_Connect, DAP_PORT_AUTODETECT}, *resp;	// 命令存储缓冲和返回值缓冲区

	if((resp = calloc(cmsis_dapObj->PacketSize, sizeof(uint8_t))) == NULL){
		log_error("Failed to calloc receive buffer space.");
		return FALSE;
	}
	// 先断开
	if(CMDAP_FUN_WARP(adapterObj, DAP_Disconnect) == FALSE){
		log_warn("Failed to disconnect.");
		return FALSE;
	}
	// 切换协议
	switch(type){
	case SWD:
		if(!CMSIS_DAP_HAS_CAPALITY(cmsis_dapObj, CMDAP_CAP_SWD)){
			goto EXIT_FAILED;
		}
		// 切换到SWD模式
		command[1] = DAP_PORT_SWD;
		DAP_EXCHANGE_DATA(adapterObj, command, 2, resp);
		if(*CAST(uint8_t *, resp+1) != DAP_PORT_SWD){
			log_warn("Switching SWD mode failed.");
			goto EXIT_FAILED;
		}else{
			// 发送切换swd序列
			CMDAP_FUN_WARP(adapterObj, SWJ_JTAG2SWD);
			log_info("Switch to SWD mode.");
			goto EXIT_TRUE;
		}
		break;

	case JTAG:
		if(!CMSIS_DAP_HAS_CAPALITY(cmsis_dapObj, CMDAP_CAP_JTAG)){
			goto EXIT_FAILED;
		}
		// 切换到JTAG模式
		command[1] = DAP_PORT_JTAG;
		DAP_EXCHANGE_DATA(adapterObj, command, 2, resp);
		if(*CAST(uint8_t *, resp+1) != DAP_PORT_JTAG){
			log_warn("Switching JTAG mode failed.");
			goto EXIT_FAILED;
		}else{
			// 发送切换JTAG序列
			CMDAP_FUN_WARP(adapterObj, SWJ_SWD2JTAG);
			log_info("Switch to JTAG mode.");
			goto EXIT_TRUE;
		}
		break;
	default :
		goto EXIT_FAILED;
	}

EXIT_FAILED:
	// 协议不支持
	free(resp);
	return FALSE;
EXIT_TRUE:
	free(resp);
	return TRUE;
}

/**
 * 设置传输参数
 * 在调用DAP_Transfer和DAP_TransferBlock之前要先调用该函数
 * idleCycle：每一次传输后面附加的空闲时钟周期数
 * waitRetry：如果收到WAIT响应，重试的次数
 * matchRetry：如果在匹配模式下发现值不匹配，重试的次数
 * SWD、JTAG模式下均有效
 */
static BOOL DAP_TransferConfigure(uint8_t *respBuff, AdapterObject *adapterObj, uint8_t idleCycle, uint16_t waitRetry, uint16_t matchRetry){
	assert(adapterObj != NULL);
	uint8_t DAPTransfer[6] = {ID_DAP_TransferConfigure};
	DAPTransfer[1] = idleCycle;	// Number of extra idle cycles after each transfer.
	DAPTransfer[2] = BYTE_IDX(waitRetry, 0);
	DAPTransfer[3] = BYTE_IDX(waitRetry, 1);
	DAPTransfer[4] = BYTE_IDX(matchRetry, 0);
	DAPTransfer[5] = BYTE_IDX(matchRetry, 1);

	DAP_EXCHANGE_DATA(adapterObj, DAPTransfer, sizeof(DAPTransfer), respBuff);
	// 增加判断是否成功
	if(respBuff[1] != DAP_OK){
		log_warn("Transfer config execution failed.");
		return FALSE;
	}
	// 更新对象数据副本
	adapterObj->dap.Retry = waitRetry;
	adapterObj->dap.IdleCycle = idleCycle;
	return TRUE;
}

/**
 * 必须实现指令之 AINS_SET_STATUS
 * DAP_HostStatus 设置仿真器状态灯
 * 参数：status 状态
 */
static BOOL DAP_HostStatus(uint8_t *respBuff, AdapterObject *adapterObj, int status){
	assert(adapterObj != NULL);
	uint8_t DAP_HostStatusPack[] = {ID_DAP_HostStatus, 0, 0};
	switch(status){
	case ADAPTER_STATUS_CONNECTED:
		DAP_HostStatusPack[1] = 0;
		DAP_HostStatusPack[2] = 1;
		break;
	case ADAPTER_STATUS_DISCONNECT:
		DAP_HostStatusPack[1] = 0;
		DAP_HostStatusPack[2] = 0;
		break;
	case ADAPTER_STATUS_RUNING:
		DAP_HostStatusPack[1] = 1;
		DAP_HostStatusPack[2] = 1;
		break;
	case ADAPTER_STATUS_IDLE:
		DAP_HostStatusPack[1] = 1;
		DAP_HostStatusPack[2] = 0;
		break;
	default:
		return FALSE;
	}
	DAP_EXCHANGE_DATA(adapterObj, DAP_HostStatusPack,  3, respBuff);
	return TRUE;
}

// 格式化时钟频率
// 返回：除去单位之后的数字部分
// 参数：freq 待格式化的始终频率数字
// unit：单位部分字符串
static double formatFreq(double freq, const char **unit){
	static const char *unit_str[] = {"Hz", "KHz", "MHz", "GHz", "THz", "PHz", "ZHz"}; // 7
	int unitPos = 0;
	while(freq > 1000.0){
		freq /= 1000.0;
		unitPos++;
	}
	if(unitPos > 6) {
		*unit = "Infinite";
		return 0;
	}
	*unit = unit_str[unitPos];
	return freq;
}

/**
 * 必须实现指令之 AINS_SET_CLOCK
 * 设置SWJ的最大频率
 * 参数 clockHz 时钟频率，单位是 Hz
 */
static BOOL DAP_SWJ_Clock (uint8_t *respBuff, AdapterObject *adapterObj, uint32_t clockHz){
	assert(adapterObj != NULL);
	uint8_t clockHzPack[5] = {ID_DAP_SWJ_Clock};

	clockHzPack[1] = BYTE_IDX(clockHz, 0);
	clockHzPack[2] = BYTE_IDX(clockHz, 1);
	clockHzPack[3] = BYTE_IDX(clockHz, 2);
	clockHzPack[4] = BYTE_IDX(clockHz, 3);

	DAP_EXCHANGE_DATA(adapterObj, clockHzPack, sizeof(clockHzPack), respBuff);
	// 增加判断是否成功
	if(respBuff[1] != DAP_OK){
		log_warn("SWJ Clock execution failed.");
		return FALSE;
	}
	// 计算格式
	const char *unit;
	double freq = formatFreq(clockHz, &unit);
	log_info("CMSIS-DAP Clock Frequency: %.2lf%s.", freq, unit);
	return TRUE;
}

/**
 * 执行JTAG时序
 * sequenceCount:需要产生多少组时序
 * data ：时序表示数据
 * response：TDO返回数据
 */
static BOOL DAP_JTAG_Sequence(uint8_t *respBuff, AdapterObject *adapterObj, int sequenceCount, uint8_t *data, uint8_t *response){
	assert(adapterObj != NULL);
	// 判断当前是否是JTAG模式
	if(adapterObj->currTrans != JTAG) return FALSE;
	struct cmsis_dap *cmsis_dapObj = CAST(struct cmsis_dap *, adapterObj);
	assert(cmsis_dapObj->PacketSize != 0);
	/**
	 * 分配所有缓冲区，包括max packet count和packet buff
	 */
	uint8_t *buff = calloc(sizeof(int) * cmsis_dapObj->MaxPcaketCount + cmsis_dapObj->PacketSize, sizeof(uint8_t));
	if(buff == NULL){
		log_warn("Unable to allocate send packet buffer, the heap may be full.");
		return FALSE;
	}
	// 记录每次分包需要接收的result
	int *resultLength = CAST(int *,buff);
	// 发送包缓冲区
	uint8_t *sendPackBuff = buff + sizeof(int) * cmsis_dapObj->MaxPcaketCount;
	int inputIdx = 0,outputIdx = 0, seqIdx = 0;	// data数据索引，response数据索引，sequence索引
	int packetStartIdx = 0;	// 当前的data的偏移
	int sendPackCnt = 0;	// 当前发包计数
	int readPayloadLen, sendPayloadLen;	// 要发送和接收的载荷总字节，不包括头部
	uint8_t seqCount;	// 统计当前有多少个seq
	sendPackBuff[0] = ID_DAP_JTAG_Sequence;	// 指令头部 0

MAKE_PACKT:
	seqCount = 0;
	readPayloadLen = 0;
	sendPayloadLen = 0;
	packetStartIdx = inputIdx;
	// 计算空间长度
	for(; seqIdx < sequenceCount; seqIdx++){
		// GNU编译器下面可以直接这样用：uint8_t tckCount = data[idx] & 0x3f ? : 64;
		uint8_t tckCount = data[inputIdx] & 0x3f;
		tckCount = tckCount ? tckCount : 64;
		uint8_t tdiByte = (tckCount + 7) >> 3;	// 将TCK个数圆整到字节，表示后面跟几个byte的tdi数据
		//log_debug("Offset:%d, seqIdx:%d, Seq:0x%02x, tck:%d, tdiByte:%d, TMS:%d.", inputIdx, seqIdx, data[inputIdx], tckCount, tdiByte, !!(data[inputIdx] & 0x40));
		// 如果当前数据长度加上tdiByte之后大于包长度，+3的意思是两个指令头部和SeqInfo字节
		if(sendPayloadLen + tdiByte + 3 > cmsis_dapObj->PacketSize){
			break;
		}else sendPayloadLen += tdiByte + 1;	// FIX:+1是要计算头部
		//如果TDO Capture标志置位，则从TDO接收tdiByte字节的数据
		if((data[inputIdx] & 0x80)){
			readPayloadLen += tdiByte;	// readByteCount 一定会比sendByteCount少，所以上面检查过sendByteCount不超过最大包长度，readByteCount必不会超过
		}
		inputIdx += tdiByte + 1;	// 跳到下一个sequence info
		seqCount++;
		if(seqCount == 255){	// 判断是否到达最大序列数目
			// BUG: 达到255后没有增加seqIdx，而inputIdx增加了，导致内存访问越界
			seqIdx++;
			break;
		}
	}
	/**
	 * 到达这儿，有三种情况：
	 * 1、seqIdx == sequenceCount
	 * 2、发送的总数据马上要大于包长度
	 * 3、seqCount == 255
	 */
	sendPackBuff[1] = seqCount;	// sequence count
	memcpy(sendPackBuff + 2, data + packetStartIdx, sendPayloadLen);

	// 发送包
	DAPWrite(adapterObj, sendPackBuff, sendPayloadLen + 2);
	resultLength[sendPackCnt++] = readPayloadLen;	// 本次包的响应包包含多少个数据

	/**
	 * 如果没发完，而且没有达到最大包数量，则再构建一个包发送过去
	 */
	if(seqIdx < (sequenceCount-1) && sendPackCnt < cmsis_dapObj->MaxPcaketCount) goto MAKE_PACKT;

	BOOL result = TRUE;
	// 读数据
	for(int readPackCnt = 0; readPackCnt < sendPackCnt; readPackCnt++){
		DAPRead(adapterObj, respBuff);
		if(result == TRUE && respBuff[1] == DAP_OK){
			// 拷贝数据
			if(response){
				memcpy(response + outputIdx, respBuff + 2, resultLength[readPackCnt]);
				outputIdx += resultLength[readPackCnt];
			}
		}else{
			result = FALSE;
		}
	}
	// 中间有错误发生
	if(result == FALSE){
		free(buff);
		return FALSE;
	}
	// 判断是否处理完，如果没有则跳回去重新处理
	if(seqIdx < (sequenceCount-1)) {
		// 将已发送包清零
		sendPackCnt = 0;
		goto MAKE_PACKT;
	}
	//log_debug("Write Back Len:%d.", outputIdx);
	free(buff);
	return TRUE;
}

/**
 * pinSelect：选择哪一个pin
 * pinOutput：引脚将要输出的信号
 * pinInput：pinOutput的信号稳定后，读取全部的引脚数据
 * pinWait：死区时间
 * 0 = no wait
 * 1 .. 3000000 = time in µs (max 3s)
 * 返回值 TRUE；
 */
static BOOL DAP_SWJ_Pins(uint8_t *respBuff, AdapterObject *adapterObj, uint8_t pinSelect, uint8_t *pinData, uint32_t pinWait){
	assert(adapterObj != NULL);
	uint8_t DAP_PinPack[7] = {ID_DAP_SWJ_Pins};
	// 构造包数据
	DAP_PinPack[1] = *pinData;
	DAP_PinPack[2] = pinSelect;
	DAP_PinPack[3] = BYTE_IDX(pinWait, 0);
	DAP_PinPack[4] = BYTE_IDX(pinWait, 1);
	DAP_PinPack[5] = BYTE_IDX(pinWait, 2);
	DAP_PinPack[6] = BYTE_IDX(pinWait, 3);

	DAP_EXCHANGE_DATA(adapterObj, DAP_PinPack, sizeof(DAP_PinPack), respBuff);
	*pinData = *(respBuff + 1);
	return TRUE;
}

/* 设置JTAG */
static BOOL DAP_JTAG_Config(uint8_t *respBuff, AdapterObject *adapterObj, uint8_t count, uint8_t *irData){
	assert(adapterObj != NULL);
	if(count > 8){
		log_warn("TAP Too lot.");
		return FALSE;
	}
	uint8_t *DAP_JTAG_ConfigPack = malloc((2 + count) * sizeof(uint8_t));
	// 构造包数据
	DAP_JTAG_ConfigPack[0] = ID_DAP_JTAG_Configure;
	DAP_JTAG_ConfigPack[1] = count;
	memcpy(DAP_JTAG_ConfigPack + 2, irData, count);

	DAP_EXCHANGE_DATA(adapterObj, DAP_JTAG_ConfigPack, 2 + count, respBuff);
	free(DAP_JTAG_ConfigPack);
	if(*(respBuff + 1) == DAP_OK){
		return TRUE;
	}
	return FALSE;
}

struct dap_pack_info {
	uint8_t seqCnt;	// 数据包中时序个数
	int dataLen;	// 数据包的读取的数据长度
};
/**
 * SWD和JTAG模式下均有效
 * 具体手册参考CMSIS-DAP DAP_Transfer这一小节
 * sequenceCnt:要发送的Sequence个数
 * okSeqCnt：执行成功的Sequence个数
 * XXX：由于DAP指令可能会出错，出错之后要立即停止执行后续指令，所以就
 * 不使用批量功能 cmsis_dapObj->MaxPcaketCount = 1
 */
static BOOL DAP_Transfer(uint8_t *respBuff, AdapterObject *adapterObj, uint8_t index,
		int sequenceCnt, uint8_t *data, uint8_t *response, int *okSeqCnt)
{
	assert(adapterObj != NULL && okSeqCnt != NULL);
	struct cmsis_dap *cmsis_dapObj = CAST(struct cmsis_dap *, adapterObj);
	assert(cmsis_dapObj->PacketSize != 0);
	// 先清零
	*okSeqCnt = 0;
	/**
	 * 分配所有缓冲区，包括max packet count和packet buff
	 */
	// uint8_t *buff = calloc(sizeof(struct dap_pack_info) * cmsis_dapObj->MaxPcaketCount + cmsis_dapObj->PacketSize, sizeof(uint8_t));
	uint8_t *buff = calloc(sizeof(struct dap_pack_info) * 1 + cmsis_dapObj->PacketSize, sizeof(uint8_t));
	if(buff == NULL){
		log_warn("Unable to allocate send packet buffer, the heap may be full.");
		return FALSE;
	}
	// 记录每次分包需要接收的result
	struct dap_pack_info *packetInfo = CAST(struct dap_pack_info *,buff);
	// 发送包缓冲区
	// uint8_t *sendPackBuff = buff + sizeof(struct dap_pack_info) * cmsis_dapObj->MaxPcaketCount;
	uint8_t *sendPackBuff = buff + sizeof(struct dap_pack_info) * 1;
	int readCount = 0, writeCount = 0, seqIdx = 0;
	int idx = 0,outIdx = 0, packetStartIdx, sendPackCnt = 0;	// 指向下一个sequence控制字节的索引，数据包的开始索引，发送数据包的个数
	uint8_t thisPackSeqCnt;	//本次数据包中Sequence个数

	// ===============构造本次数据包==================
MAKE_PACKT:
	//log_debug("make packet.");
	thisPackSeqCnt = 0;
	readCount = 0;
	writeCount = 0;
	packetStartIdx = idx;
	// 统计一些信息
	for(; seqIdx < sequenceCnt; seqIdx ++){
		data[idx] &= 0xf;	// 只保留[3:0]位
		// 判断是否是读寄存器
		if((data[idx] & DAP_TRANSFER_RnW) == DAP_TRANSFER_RnW){
			// 判断是否超出最大包长度
			if((3 + readCount + 4) > cmsis_dapObj->PacketSize || (3 + writeCount + 1) > cmsis_dapObj->PacketSize){
				break;
			}
			idx += 1;
			readCount += 4;
			writeCount ++;
		}else{	// 写寄存器
			if((3 + writeCount + 5) > cmsis_dapObj->PacketSize){
				break;
			}
			idx += 5;
			writeCount += 5;
		}
		thisPackSeqCnt++;	// 本数据包sequence个数自增
		// 判断数据包个数是否达到最大
		if(thisPackSeqCnt == 0xFFu){
			seqIdx ++;
			break;
		}
	}
	// 构造数据包头部
	sendPackBuff[0] = ID_DAP_Transfer;
	sendPackBuff[1] = index;	// DAP index, JTAG ScanChain 中的位置，在SWD模式下忽略该参数
	sendPackBuff[2] = thisPackSeqCnt;	// 传输多少个request

	// 将数据拷贝到包中
	memcpy(sendPackBuff + 3, data + packetStartIdx, writeCount);
	// 交换数据
	DAPWrite(adapterObj, sendPackBuff, 3 + writeCount);
	packetInfo[sendPackCnt].dataLen = readCount;	// 本次包的响应包包含多少个数据
	packetInfo[sendPackCnt].seqCnt = thisPackSeqCnt;
	sendPackCnt++;
	/**
	 * 如果没发完，而且没有达到最大包数量，则再构建一个包发送过去
	 */
	// if(seqIdx < (sequenceCnt-1) && sendPackCnt < cmsis_dapObj->MaxPcaketCount) goto MAKE_PACKT;
	if(seqIdx < (sequenceCnt-1) && sendPackCnt < 1) goto MAKE_PACKT;

	BOOL result = TRUE;
	for(int readPackCnt = 0; readPackCnt < sendPackCnt; readPackCnt++){	// NOTIC 这个循环也就执行一次
		DAPRead(adapterObj, respBuff);
		// 本次执行的Sequence是否等于应该执行的个数
		if(result == TRUE){
			*okSeqCnt += respBuff[1];
			if(respBuff[1] != packetInfo[readPackCnt].seqCnt){
				log_warn("Last Response: %d.", respBuff[2]);
				result = FALSE;
			}
			// 拷贝数据
			if(response){
				memcpy(response + outIdx, respBuff + 3, packetInfo[readPackCnt].dataLen);
				outIdx += packetInfo[readPackCnt].dataLen;
			}
		}
	}
	if(result == FALSE){
		free(buff);
		return FALSE;
	}
	// 判断是否处理完，如果没有则跳回去重新处理
	if(seqIdx < (sequenceCnt-1)) {
		// 将已发送包清零
		sendPackCnt = 0;
		goto MAKE_PACKT;
	}
	free(buff);
	return TRUE;
}

/**
 * DAP_TransferBlock
 * 对单个寄存器进行多次读写，常配合地址自增使用
 * 参数列表和意义与DAP_Transfer相同
 */
static BOOL DAP_TransferBlock(uint8_t *respBuff, AdapterObject *adapterObj, uint8_t index,
		int sequenceCnt, uint8_t *data, uint8_t *response, int *okSeqCnt)
{
	assert(adapterObj != NULL && okSeqCnt != NULL);
	struct cmsis_dap *cmsis_dapObj = CAST(struct cmsis_dap *, adapterObj);
	assert(cmsis_dapObj->PacketSize != 0);
	BOOL result = FALSE;
	// 本次传输长度
	int restCnt = 0, readCnt = 0, writeCnt = 0;
	int sentPacketMaxCnt = (cmsis_dapObj->PacketSize - 5) >> 2;	// 发送数据包可以装填的数据个数
	int readPacketMaxCnt = (cmsis_dapObj->PacketSize - 4) >> 2;	// 接收数据包可以装填的数据个数

	// 开辟本次数据包的空间
	uint8_t *buff = calloc(cmsis_dapObj->PacketSize, sizeof(uint8_t));
	if(buff == NULL){
		log_warn("Unable to allocate send packet buffer, the heap may be full.");
		return FALSE;
	}
	// 构造数据包头部
	buff[0] = ID_DAP_TransferBlock;
	buff[1] = index;	// DAP index, JTAG ScanChain 中的位置，在SWD模式下忽略该参数
	for(*okSeqCnt = 0; *okSeqCnt < sequenceCnt; (*okSeqCnt)++){
		restCnt = *CAST(int *, data + readCnt); readCnt += sizeof(int);
		uint8_t seq = *CAST(uint8_t *, data + readCnt++);
		if(seq & DAP_TRANSFER_RnW){	// 读操作
			while(restCnt > 0){
				if(restCnt > readPacketMaxCnt){
					*CAST(uint16_t *, buff + 2) = readPacketMaxCnt;
					buff[4] = seq;
					// 交换数据
					DAP_EXCHANGE_DATA(adapterObj, buff, 5, respBuff);
					// 判断操作成功，写回数据 XXX 小端字节序
					if(*CAST(uint16_t *, respBuff + 1) == readPacketMaxCnt && respBuff[3] == DAP_TRANSFER_OK){	// 成功
						memcpy(response + writeCnt, respBuff + 4, readPacketMaxCnt << 2);	// 将数据拷贝到
						writeCnt += readPacketMaxCnt << 2;
						restCnt -= readPacketMaxCnt;	// 成功读取readPacketMaxCnt个字
					}else{	// 失败
						goto END;
					}
				}else{
					*CAST(uint16_t *, buff + 2) = restCnt;
					buff[4] = seq;
					// 交换数据
					DAP_EXCHANGE_DATA(adapterObj, buff, 5, respBuff);
					// 判断操作成功，写回数据 XXX 小端字节序
					if(*CAST(uint16_t *, respBuff + 1) == restCnt && respBuff[3] == DAP_TRANSFER_OK){	// 成功
						memcpy(response + writeCnt, respBuff + 4, restCnt << 2);	// 将数据拷贝到
						writeCnt += restCnt << 2;
						restCnt = 0;	// 全部发送完了
					}else{	// 失败
						goto END;
					}
				}
			}
		}else{	// 写操作
			while(restCnt > 0){
				if(restCnt > sentPacketMaxCnt){
					*CAST(uint16_t *, buff + 2) = sentPacketMaxCnt;
					buff[4] = seq;
					// 拷贝数据
					memcpy(buff + 5, data + readCnt, sentPacketMaxCnt << 2);	// 将数据拷贝到
					readCnt += sentPacketMaxCnt << 2;
					// 交换数据
					DAP_EXCHANGE_DATA(adapterObj, buff, 5 + (sentPacketMaxCnt << 2), respBuff);
					// 判断操作成功 XXX 小端字节序
					if(*CAST(uint16_t *, respBuff + 1) == sentPacketMaxCnt && respBuff[3] == DAP_TRANSFER_OK){	// 成功
						restCnt -= sentPacketMaxCnt;
					}else{	// 失败
						goto END;
					}
				}else{
					*CAST(uint16_t *, buff + 2) = restCnt;
					buff[4] = seq;
					// 拷贝数据
					memcpy(buff + 5, data + readCnt, restCnt << 2);	// 将数据拷贝到
					readCnt += restCnt << 2;
					// 交换数据 FIXED 以后运算符根据优先级都要打上括号！！MMP
					DAP_EXCHANGE_DATA(adapterObj, buff, 5 + (restCnt << 2), respBuff);
					// 判断操作成功 XXX 小端字节序
					if(*CAST(uint16_t *, respBuff + 1) == restCnt && respBuff[3] == DAP_TRANSFER_OK){	// 成功
						restCnt = 0;
					}else{	// 失败
						goto END;
					}
				}
			}
		}
	}
	result = TRUE;
END:
	free(buff);
	return result;
}

/**
 * 复位操作
 * hard：是否硬复位（nTRST、nSRST）
 * srst：系统复位nRESET
 * pinWait：死区时间
 * 注意：有的电路没有连接nTRST，所以TAP状态机复位尽量使用软复位
 */
static BOOL DAP_Reset(uint8_t *respBuff, AdapterObject *adapterObj, BOOL hard, BOOL srst, int pinWait){
	assert(adapterObj != NULL);
	if(adapterObj->currTrans == JTAG){
		if(hard == TRUE){	// 硬复位
			uint8_t pinData = 0, pinSelect = 1 << SWJ_PIN_nTRST;	// 状态机复位，
			if(srst == TRUE){	// 系统复位
				pinSelect |= 1 << SWJ_PIN_nRESET;
			}
			if(DAP_SWJ_Pins(respBuff, adapterObj, pinSelect, &pinData, pinWait) == FALSE){
				log_warn("Assert Hard Reset Failed!");
				return FALSE;
			}
			// 取消复位
			pinData = 0xFF;
			if(DAP_SWJ_Pins(respBuff, adapterObj, pinSelect, &pinData, pinWait) == FALSE){
				log_warn("Deassert Hard Reset Failed!");
				return FALSE;
			}
		}else{
			uint8_t resetSeq[] = {0x45, 0x00};
			// 执行队列
			if(DAP_JTAG_Sequence(respBuff, adapterObj, 1, resetSeq, NULL) == FALSE){
				return FALSE;
			}
		}
		return TRUE;
	}else if(adapterObj->currTrans == SWD){	// line reset ，忽略硬复位和srst
		// 执行line reset
		return SWD_LineReset(respBuff, adapterObj);
	}else{
		return FALSE;
	}
}

/**
 * 配置SWD
 */
static BOOL DAP_SWD_Config(uint8_t *respBuff, AdapterObject *adapterObj, uint8_t cfg){
	assert(adapterObj != NULL);
	uint8_t DAP_SWDCFGPack[2] = {ID_DAP_SWD_Configure};
	// 构造包数据
	DAP_SWDCFGPack[1] = cfg;

	DAP_EXCHANGE_DATA(adapterObj, DAP_SWDCFGPack, sizeof(DAP_SWDCFGPack), respBuff);
	if(*(respBuff + 1) == DAP_OK){
		return TRUE;
	}
	return FALSE;
}

/**
 * 解析TMS信息，并写入到buff
 * seqInfo:由JTAG_getTMSSequence函数返回的TMS时序信息
 * seqCnt:
 * 返回写入的字节数
 * Sequence Info: Contains number of TDI bits and fixed TMS value
    Bit 5 .. 0: Number of TCK cycles: 1 .. 64 (64 encoded as 0)
    Bit 6: TMS value
    Bit 7: TDO Capture
 *
 */
static int parseTMS(uint8_t *buff, TMS_SeqInfo seqInfo, int *seqCnt){
	assert(buff != NULL);
	uint8_t bitCount = seqInfo & 0xff;
	uint8_t TMS_Seq = seqInfo >> 8;
	if(bitCount == 0) return 0;
	int writeCount = 1;
	*buff = (TMS_Seq & 0x1) << 6;
	// sequence 个数+1
	(*seqCnt) ++;
	while(bitCount--){
		(*buff)++;
		TMS_Seq >>= 1;
		if( bitCount && (((TMS_Seq & 0x1) << 6) ^ (*buff & 0x40)) ){
			*++buff = 0;
			*++buff = (TMS_Seq & 0x1) << 6;
			(*seqCnt) ++;
			writeCount += 2;
		}
	}
	*++buff = 0;
	return writeCount + 1;
}

/**
 * 解析TDI数据
 */
static int parseTDI(uint8_t *buff, uint8_t *TDIData, int bitCnt, int *seqCnt){
	assert(buff != NULL);
	int writeCnt = 0;	// 写入了多少个字节
	for(int n=bitCnt - 1, readCnt=0; n>0;){
		if(n >= 64){	// 当生成的时序字节小于8个
			*buff++ = 0x80;	// TMS=0;TDO Capture=1; 64Bit
			(*seqCnt) ++;
			memcpy(buff, TDIData + readCnt, 8);
			buff+=8; n-=64; readCnt+=8;
			writeCnt += 9;	// 数据加上一个头部
		}else{
			int bytesCnt = (n + 7) >> 3;
			*buff++ = 0x80 + n;
			(*seqCnt) ++;
			memcpy(buff, TDIData + readCnt, bytesCnt);
			buff+=bytesCnt; readCnt+=bytesCnt;
			writeCnt += bytesCnt+1;	// 数据加上一个头部
			n=0;
		}
	}
	// 解析最后一位
	*buff++ = 0xC1;	// 0xC1 TMS=1 TCLK=1 TDO Capature=1
	*buff++ = GET_Nth_BIT(TDIData, bitCnt-1);
	writeCnt += 2;
	(*seqCnt) ++;
	return writeCnt;
}

/**
 * 解析IDLE Wait
 */
static int parseIDLEWait(uint8_t *buff, int wait, int *seqCnt){
	assert(buff != NULL);
	int writeCnt = 0;	// 写入了多少个字节
	for(int n=wait; n>0;){
		if(n >= 64){	// 当生成的时序字节小于8个
			*buff++ = 0x00;	// TMS=0;TDO Capture=0; 64Bit
			(*seqCnt) ++;
			memset(buff, 0x0, 8);
			buff+=8; n-=64;
			writeCnt += 9;	// 数据加上一个头部
		}else{
			int bytesCnt = (n + 7) >> 3;
			*buff++ = 0x00 + n;
			(*seqCnt) ++;
			memset(buff, 0x0, bytesCnt);
			buff+=bytesCnt;
			writeCnt += bytesCnt+1;	// 数据加上一个头部
			n=0;
		}
	}
	return writeCnt;
}

/**
 * 解析执行JTAG指令队列
 */
static BOOL Execute_JTAG_Cmd(uint8_t *respBuff, AdapterObject *adapterObj){
	assert(adapterObj != NULL);
	int seqCnt = 0;	// CMSIS-DAP的Sequence个数，具体看CMSIS-DAP手册
	int writeBuffLen = 0;	// 生成指令缓冲区的长度
	int readBuffLen = 0;	// 需要读的字节个数
	int writeCnt = 0, readCnt = 0;	// 写入数据个数，读取数据个数
	// 遍历指令，计算解析后的数据长度，开辟空间
	struct JTAG_Command *cmd, *cmd_t;
	list_for_each_entry(cmd, &adapterObj->tap.instrQueue, list_entry){
		switch(cmd->type){
		case JTAG_INS_STATUS_MOVE:	// 状态机切换
			// 电平状态数乘以2，因为CMSIS-DAP的JTAG指令格式是一个seqinfo加一个tdi数据，而且TAP状态机的任意两个状态切换tms不会多于8个时钟
			writeBuffLen += JTAG_Cal_TMS_LevelStatus(cmd->instr.statusMove.seqInfo >> 8, cmd->instr.statusMove.seqInfo & 0xff) * 2;
			break;
		case JTAG_INS_EXCHANGE_IO:	// 交换IO
			do{
				int writeCnt = 0, readCnt = 0;
				for(int n = cmd->instr.exchangeIO.bitNum - 1; n>0;){
					if(n >= 64){
						n-=64; readCnt += 8;
						writeCnt += 9;	// 数据加上一个头部
					}else{
						int bytesCnt = (n + 7) >> 3;
						readCnt+=bytesCnt;
						writeCnt += bytesCnt+1;	// 数据加上一个头部
						n=0;
					}
				}
				writeBuffLen += writeCnt + 2;	// 最后加2是跳出SHIFT-xR状态
				readBuffLen += readCnt + 1;	// 最后+1是跳出SHIFT-xR的状态需要的一位数据
			}while(0);
			break;

		case JTAG_INS_IDLE_WAIT:
			// 根据等待时钟周期数来计算需要多少个控制字，因为CMSIS-DAP的等待时钟周期每个控制字最多可以有64个
			// 所以等待周期多的时候会需要多个控制字
			writeBuffLen += BIT2BYTE(cmd->instr.idleWait.wait);
			break;
		}
	}

	// 创建内存空间
	log_trace("CMSIS-DAP JTAG Parsed buff length: %d, read buff length: %d.", writeBuffLen, readBuffLen);
	uint8_t *writeBuff = malloc(writeBuffLen * sizeof(uint8_t));
	if(writeBuff == NULL){
		log_warn("CMSIS-DAP JTAG Instruct buff allocte failed.");
		return FALSE;
	}
	uint8_t *readBuff = malloc(readBuffLen * sizeof(uint8_t));
	if(readBuff == NULL){
		log_warn("CMSIS-DAP JTAG Read buff allocte failed.");
		free(writeBuff);
		return FALSE;
	}

	// 第二次遍历，生成指令对应的数据
	list_for_each_entry(cmd, &adapterObj->tap.instrQueue, list_entry){
		switch(cmd->type){
		case JTAG_INS_STATUS_MOVE:	// 状态机切换
			writeCnt += parseTMS(writeBuff + writeCnt, cmd->instr.statusMove.seqInfo, &seqCnt);
			break;
		case JTAG_INS_EXCHANGE_IO:	// 交换IO
			writeCnt += parseTDI(writeBuff + writeCnt, cmd->instr.exchangeIO.data, cmd->instr.exchangeIO.bitNum, &seqCnt);
			break;
		case JTAG_INS_IDLE_WAIT:	// 进入IDLE状态等待
			writeCnt += parseIDLEWait(writeBuff + writeCnt, cmd->instr.idleWait.wait, &seqCnt);
			break;
		}
	}
	// 执行指令
	if(DAP_JTAG_Sequence(respBuff, adapterObj, seqCnt, writeBuff, readBuff) == FALSE){
		log_warn("Execute JTAG Instruction Failed.");
		free(writeBuff);
		free(readBuff);
		return FALSE;
	}

	// 第三次遍历：同步数据，并删除执行成功的指令
	list_for_each_entry_safe(cmd, cmd_t, &adapterObj->tap.instrQueue, list_entry){
		// 跳过状态机改变指令
		if(cmd->type == JTAG_INS_STATUS_MOVE || cmd->type == JTAG_INS_IDLE_WAIT) {
			goto FREE_CMD;
		}
		// 所占的数据长度
		int byteCnt = (cmd->instr.exchangeIO.bitNum + 7) >> 3;
		int restBit = (cmd->instr.exchangeIO.bitNum - 1) & 0x7;	// 剩余二进制位个数
		memcpy(cmd->instr.exchangeIO.data, readBuff + readCnt, byteCnt);
		readCnt += byteCnt;
		/**
		 * bitCnt-1之后如果是8的倍数，就不需要组合最后一字节的数据
		 */
		if(restBit != 0){	// 将最后一个字节的数据组合到前一个字节上
			*(cmd->instr.exchangeIO.data + byteCnt - 1) |= (*(readBuff + readCnt) & 1) << restBit;
			readCnt ++;
		}
		FREE_CMD:
		list_del(&cmd->list_entry);	// 将链表中删除
		free(cmd);
	}

	// 释放资源
	free(writeBuff);
	free(readBuff);
	return TRUE;
}

/**
 * 解析执行DAP指令队列
 * 注意：对于读操作，成功之后才写入内存地址，如果读取失败，则值保持不变，不要清零
 */
static BOOL Execute_DAP_Cmd(uint8_t *respBuff, AdapterObject *adapterObj){
	assert(adapterObj != NULL);
	struct DAP_Command *cmd, *cmd_t;
	int readCnt, writeCnt, seqCnt;
	int readBuffLen, writeBuffLen;
	if(adapterObj->dap.DAP_Index > 7){
		log_warn("DAP TAP Index Too large.");
		return FALSE;
	}
REEXEC:;
	readCnt = 0; writeCnt = 0; seqCnt = 0;
	readBuffLen = 0;writeBuffLen = 0;
	// 本次处理的指令类型，找到指令队列中第一个指令的类型
	enum DAP_InstrType thisType = container_of(adapterObj->dap.instrQueue.next, struct DAP_Command, list_entry)->type;
	// 第一次遍历，计算所占用的空间
	list_for_each_entry(cmd, &adapterObj->dap.instrQueue, list_entry){
		if(cmd->type != thisType){
			break;
		}
		switch(cmd->type){
		case DAP_INS_RW_REG_SINGLE:	// 单次读写寄存器
			writeBuffLen += 1;
			if(cmd->instr.RWRegSingle.RnW == 1){	// 读操作
				readBuffLen += 4;
			}else{
				writeBuffLen += 4;
			}
			break;

		case DAP_INS_RW_REG_BLOCK:	// 多次读写寄存器
			writeBuffLen += 1 + sizeof(int);	// int:blockCnt, byte:seq
			if(cmd->instr.RWRegBlock.RnW == 1){	// 读操作
				readBuffLen += cmd->instr.RWRegBlock.blockCnt << 2;
			}else{
				writeBuffLen += cmd->instr.RWRegBlock.blockCnt << 2;
			}
			break;
		}
		
	}
	// 分配内存空间
	log_trace("CMSIS-DAP DAP Parsed buff length: %d, read buff length: %d.", writeBuffLen, readBuffLen);
	uint8_t *writeBuff = malloc(writeBuffLen * sizeof(uint8_t));
	if(writeBuff == NULL){
		log_warn("CMSIS-DAP DAP Instruct buff allocte failed.");
		return FALSE;
	}
	uint8_t *readBuff = malloc(readBuffLen * sizeof(uint8_t));
	if(readBuff == NULL){
		log_warn("CMSIS-DAP DAP Read buff allocte failed.");
		free(writeBuff);
		return FALSE;
	}

	// 第二次遍历 生成指令数据
	list_for_each_entry(cmd, &adapterObj->dap.instrQueue, list_entry){
		if(cmd->type != thisType){
			break;
		}
		switch(cmd->type){
		case DAP_INS_RW_REG_SINGLE:
			*(writeBuff + writeCnt++) = (cmd->instr.RWRegSingle.reg & 0xC)
				| (cmd->instr.RWRegSingle.RnW ? DAP_TRANSFER_RnW : 0)
				| (cmd->instr.RWRegSingle.APnDP ? DAP_TRANSFER_APnDP : 0);
			
//			do{
//				log_debug("-----------------------------");
//				log_debug("%s %s Reg %x.",
//					cmd->instr.RWRegSingle.RnW ? "Read" : "Write",
//					cmd->instr.RWRegSingle.APnDP ? "AP" : "DP",
//					cmd->instr.RWRegSingle.reg & 0xC);
//				if(cmd->instr.RWRegSingle.RnW == FALSE){
//					log_debug("0x%08X", cmd->instr.RWRegSingle.data.write);
//				}
//			}while(0);

			// 如果是写操作
			if(cmd->instr.RWRegSingle.RnW == 0){
				// XXX 小端字节序
				memcpy(writeBuff + writeCnt, CAST(uint8_t *, &cmd->instr.RWRegSingle.data.write), 4);
				writeCnt += 4;
			}
			seqCnt++;
			break;

		case DAP_INS_RW_REG_BLOCK:
			// 写入本次操作的次数
			*CAST(int *, writeBuff + writeCnt) = cmd->instr.RWRegBlock.blockCnt;
			writeCnt += sizeof(int);
			*(writeBuff + writeCnt++) = (cmd->instr.RWRegSingle.reg & 0xC)
				| (cmd->instr.RWRegBlock.RnW ? DAP_TRANSFER_RnW : 0)
				| (cmd->instr.RWRegBlock.APnDP ? DAP_TRANSFER_APnDP : 0);
			// 如果是写操作
			if(cmd->instr.RWRegBlock.RnW == 0){
				// XXX 小端字节序
				memcpy(writeBuff + writeCnt, CAST(uint8_t *, cmd->instr.RWRegBlock.data.write), cmd->instr.RWRegBlock.blockCnt << 2);
				writeCnt += cmd->instr.RWRegBlock.blockCnt << 2;
			}
			seqCnt++;
			break;
		}
	}

	// 执行成功的Sequence个数
	int okSeqCnt = 0;
	BOOL result = TRUE;
	switch(thisType){
	case DAP_INS_RW_REG_SINGLE:
		// 执行指令 DAP_Transfer
		if(DAP_Transfer(respBuff, adapterObj, adapterObj->dap.DAP_Index, seqCnt, writeBuff, readBuff, &okSeqCnt) == FALSE){
			log_warn("DAP_Transfer:Some DAP Instruction Execute Failed. Success:%d, All:%d.", okSeqCnt, seqCnt);
			result = FALSE;
		}
	break;

	case DAP_INS_RW_REG_BLOCK:
		// transfer block
		if(DAP_TransferBlock(respBuff, adapterObj, adapterObj->dap.DAP_Index, seqCnt, writeBuff, readBuff, &okSeqCnt) == FALSE){
			log_warn("DAP_TransferBlock:Some DAP Instruction Execute Failed. Success:%d, All:%d.", okSeqCnt, seqCnt);
			result = FALSE;
		}
	break;
	}

	// 第三次遍历：同步数据
	list_for_each_entry_safe(cmd, cmd_t, &adapterObj->dap.instrQueue, list_entry){
		if(cmd->type != thisType){
			break;
		}
		if(okSeqCnt <= 0){
			// 未执行的指令保存在指令队列中
			break;
		}
		okSeqCnt--;	// 只同步执行成功的Seq个数
		if(cmd->type == DAP_INS_RW_REG_SINGLE && cmd->instr.RWRegSingle.RnW == 1){	// 单次读寄存器
			memcpy(cmd->instr.RWRegSingle.data.read, readBuff + readCnt, 4);
			readCnt += 4;
		}else if(cmd->type == DAP_INS_RW_REG_BLOCK && cmd->instr.RWRegSingle.RnW == 1){	// 多次读寄存器
			memcpy(cmd->instr.RWRegBlock.data.read, readBuff + readCnt, cmd->instr.RWRegBlock.blockCnt << 2);
			readCnt += cmd->instr.RWRegBlock.blockCnt << 2;
		}
		list_del(&cmd->list_entry);	// 将链表中删除
		free(cmd);
	}
	free(writeBuff);
	free(readBuff);
	// 判断是否继续执行
	if(result && !list_empty(&adapterObj->dap.instrQueue)){
		goto REEXEC;
	}
	return result;
}

/**
 * DAP写ABORT寄存器
 * The DAP_WriteABORT Command writes an abort request to the CoreSight ABORT register of the Target Device.
 */
static BOOL DAP_WriteAbort(uint8_t *respBuff, AdapterObject *adapterObj, uint32_t data){
	assert(respBuff != NULL && adapterObj != NULL);
	uint8_t DAP_AbortPacket[6] = {ID_DAP_WriteABORT};
	DAP_AbortPacket[1] = adapterObj->dap.DAP_Index;
	DAP_AbortPacket[2] = BYTE_IDX(data, 0);
	DAP_AbortPacket[3] = BYTE_IDX(data, 1);
	DAP_AbortPacket[4] = BYTE_IDX(data, 2);
	DAP_AbortPacket[5] = BYTE_IDX(data, 3);
	DAP_EXCHANGE_DATA(adapterObj, DAP_AbortPacket, sizeof(DAP_AbortPacket), respBuff);
	if(respBuff[1] != DAP_OK){
		return FALSE;
	}
	return TRUE;
}

/**
 * 执行仿真器指令
 */
static BOOL operate(AdapterObject *adapterObj, int operate, ...){
	assert(adapterObj != NULL);
	va_list parames;
	BOOL result = FALSE;
	va_start(parames, operate);	// 确定可变参数起始位置
	// 根据指令选择操作
	switch(operate){
	case AINS_SET_STATUS:
		log_trace("Execution command: Set Status.");
		do{
			int status = va_arg(parames, int);
			result = CMDAP_FUN_WARP(adapterObj, DAP_HostStatus, status);
		}while(0);
		break;

	case AINS_SET_CLOCK:
		log_trace("Execution command: Set SWJ Clock.");
		do{
			uint32_t clockHz = va_arg(parames, uint32_t);
			result = CMDAP_FUN_WARP(adapterObj, DAP_SWJ_Clock, clockHz);
		}while(0);
		break;

	case AINS_RESET:
		log_trace("Execution command: Reset.");
		do{
			BOOL hard = (BOOL)va_arg(parames, BOOL);
			BOOL srst = (BOOL)va_arg(parames, BOOL);
			int pinWait = va_arg(parames, int);
			result = CMDAP_FUN_WARP(adapterObj, DAP_Reset, hard, srst, pinWait);
		}while(0);
		break;

	case AINS_JTAG_CMD_EXECUTE:
		log_trace("Execution command: Execute JTAG Command.");
		do{
			result = CMDAP_FUN_WARP(adapterObj, Execute_JTAG_Cmd);
		}while(0);
		break;

	case AINS_JTAG_PINS:
		log_trace("Execution command: JTAG Pins.");
		do{
			uint8_t pinSelect = (uint8_t)va_arg(parames, int);
			uint8_t *pinData = va_arg(parames, uint8_t *);
			uint32_t pinWait = (uint32_t)va_arg(parames, int);
			result = CMDAP_FUN_WARP(adapterObj, DAP_SWJ_Pins, pinSelect, pinData, pinWait);
		}while(0);
		break;

	case AINS_DAP_CMD_EXECUTE:
		log_trace("Execution command: Execute_DAP_Cmd.");
		do {
			uint8_t dap_index = (uint8_t)va_arg(parames, int);
			uint8_t count = (uint8_t)va_arg(parames, int);
			uint8_t *data = va_arg(parames, uint8_t *);
			uint8_t *response = va_arg(parames, uint8_t *);
			result = CMDAP_FUN_WARP(adapterObj, Execute_DAP_Cmd);
		}while(0);
		break;

	case AINS_DAP_WRITE_ABOTR:
		do{
			uint32_t data = (uint32_t)va_arg(parames, int);
			result = CMDAP_FUN_WARP(adapterObj, DAP_WriteAbort, data);
		}while(0);
		break;

	case CMDAP_TRANSFER_CONFIG:	// CMSIS-DAP指定的的接口
		log_trace("Execution command: Transfer Config.");
		do{
			uint8_t idleCycle = (uint8_t)va_arg(parames, int);	// 在每次传输后面加多少个空闲时钟周期
			uint16_t waitRetry = (uint16_t)va_arg(parames, int);	// 得到WAIT Response后重试次数
			uint16_t matchRetry = (uint16_t)va_arg(parames, int);	// 匹配模式下重试次数
			log_info("Set Transfer Configure: Idle:%d, Wait:%d, Match:%d.", idleCycle, waitRetry, matchRetry);
			result = CMDAP_FUN_WARP(adapterObj, DAP_TransferConfigure, idleCycle, waitRetry, matchRetry);
		}while(0);
		break;

	case CMDAP_JTAG_CONFIGURE:
		do{
			uint8_t count = (uint8_t)va_arg(parames, int);
			uint8_t *irData = (uint8_t *)va_arg(parames, uint8_t *);
			log_info("Set JTAG Configure: Count:%d, irData[0]:%d.", count, irData[0]);
			result = CMDAP_FUN_WARP(adapterObj, DAP_JTAG_Config, count, irData);
		}while(0);
		break;

	case CMDAP_SWD_CONFIGURE:
		do{
			uint8_t cfg = (uint8_t)va_arg(parames, int);
			log_info("SWD: Turnaround clock period: %d clock cycles. DataPhase: %s.", (cfg & 0x3)+1, (cfg >> 2) ? "Always" : "No");
			result = CMDAP_FUN_WARP(adapterObj, DAP_SWD_Config, cfg);
		}while(0);
		break;

	default:
		log_warn("Unsupported operation: %d.", operate);
		break;
	}
	va_end(parames);
	return result;
}

/* CMSIS-DAP specific operation */
/**
 * 设置JTAG信息
 * count：扫描链中TAP的个数，不超过8个
 * irData：每个TAP的IR寄存器长度
 * 返回 成功 失败
 */
BOOL CMSIS_DAP_JTAG_Configure(AdapterObject *adapterObj, uint8_t count, uint8_t *irData){
	assert(adapterObj != NULL);
	return adapterObj->Operate(adapterObj, CMDAP_JTAG_CONFIGURE, count, irData);
}

/**
 * 配置DAP传输参数，同时会修改AdapterObject.DAP里面的Retry、IdleCycle对象
 */
BOOL CMSIS_DAP_TransferConfigure(AdapterObject *adapterObj, uint8_t idleCycle, uint16_t waitRetry, uint16_t matchRetry){
	assert(adapterObj != NULL);
	return adapterObj->Operate(adapterObj, CMDAP_TRANSFER_CONFIG, idleCycle, waitRetry, matchRetry);
}

BOOL CMSIS_DAP_SWD_Configure(AdapterObject *adapterObj, uint8_t cfg){
	assert(adapterObj != NULL);
	return adapterObj->Operate(adapterObj, CMDAP_SWD_CONFIGURE, cfg);
}

// 文件结束
