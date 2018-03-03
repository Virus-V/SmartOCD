/*
 * cmsis-dap.c
 *
 *  Created on: 2018-2-18
 *      Author: virusv
 */

#ifdef HAVE_CONFIG
#include "global_config.h"
#endif

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "smart_ocd.h"
#include "misc/log.h"
#include "debugger/cmsis-dap.h"
#include "arch/ARM/ADI/DAP.h"

/**
 * 这个宏是用来作为与cmsis-dap通信函数的外包部分
 * 该宏实现自动申请cmsis-dap返回包缓冲空间并且带函数执行结束后
 * 释放该空间
 */
#define CMDAP_FUN_WARP(size,func,...) ({	\
	uint8_t *responseBuff; BOOL result = FALSE;	\
	assert((size) > 0);	\
	if((responseBuff = calloc((size), sizeof(uint8_t))) == NULL){	\
		log_warn("Alloc reply packet buffer memory failed.");	\
		result = FALSE;	\
	}else{	\
		result = (func)(responseBuff, ##__VA_ARGS__);	\
		free(responseBuff);	\
	}	\
	result;	\
})

// CMSIS-DAP支持的仿真传输协议类型
static const enum transportType supportTrans[] = {JTAG, SWD, UNKNOW_NULL};
static BOOL init(AdapterObject *adapterObj);
static BOOL deinit(AdapterObject *adapterObj);
static BOOL selectTrans(AdapterObject *adapterObj, enum transportType type);
static BOOL operate(AdapterObject *adapterObj, int operate, ...);

/**
 * 创建新的CMSIS-DAP仿真器对象
 */
struct cmsis_dap *NewCMSIS_DAP(){
	USBObject *usbObj;
	AdapterObject *adapterObj;
	struct cmsis_dap *cmsis_dapObj;	// cmsis_dap对象

	cmsis_dapObj = calloc(1, sizeof(struct cmsis_dap));
	if(cmsis_dapObj == NULL){
		log_warn("Failed to create CMSIS-DAP object.");
		return NULL;
	}
	// 构造父对象
	adapterObj = InitAdapterObject(CAST(AdapterObject *, cmsis_dapObj), "ARM CMSIS-DAP");

	// 初始化USB对象
	if((usbObj = NewUSBObject()) == NULL){
		DeinitAdapterObject(adapterObj);
		free(cmsis_dapObj);
		log_warn("Failed to create USB object.");
		return NULL;
	}
	adapterObj->ConnObject.type = USB;
	adapterObj->ConnObject.connectFlag = 0;
	SET_USBOBJ(adapterObj, usbObj);

	// 设置该设备支持的传输类型
	adapterObj->supportedTrans = supportTrans;
	adapterObj->currTrans = UNKNOW_NULL;
	// 配置方法函数
	adapterObj->Init = init;
	adapterObj->Deinit = deinit;
	adapterObj->SelectTrans = selectTrans;
	adapterObj->Operate = operate;

	return cmsis_dapObj;
}

// 释放CMSIS-DAP对象
void FreeCMSIS_DAP(struct cmsis_dap *cmsis_dapObj){
	assert(cmsis_dapObj != NULL);
	// 反初始化Adapter对象
	DeinitAdapterObject(CAST(AdapterObject *, cmsis_dapObj));
	// 关闭USB对象
	if(GET_ADAPTER(cmsis_dapObj)->ConnObject.connectFlag){
		USBClose(GET_USBOBJ(CAST(AdapterObject *, cmsis_dapObj)));
		GET_ADAPTER(cmsis_dapObj)->ConnObject.connectFlag = 0;
	}
	// 释放USB对象
	FreeUSBObject(GET_USBOBJ(CAST(AdapterObject *, cmsis_dapObj)));
	// 释放CMSIS-DAP对象
	free(cmsis_dapObj);
}

// 连接CMSIS-DAP设备，只支持USB
BOOL ConnectCMSIS_DAP(struct cmsis_dap *cmsis_dapObj, const uint16_t *vids, const uint16_t *pids, const char *serialNum){
	assert(cmsis_dapObj != NULL && CAST(AdapterObject *, cmsis_dapObj)->ConnObject.type == USB);
	int idx = 0;
	USBObject *usbObj = GET_USBOBJ(CAST(AdapterObject *, cmsis_dapObj));
	for(; vids[idx] && pids[idx]; idx++){
		log_debug("Try connecting vid: 0x%02x, pid: 0x%02x usb device.", vids[idx], pids[idx]);
		if(USBOpen(usbObj, vids[idx], pids[idx], serialNum)){
			log_info("Successful connection vid: 0x%02x, pid: 0x%02x usb device.", vids[idx], pids[idx]);
			// 复位设备
			USBResetDevice(usbObj);
			// 标志已连接
			CAST(AdapterObject *, cmsis_dapObj)->ConnObject.connectFlag = 1;
			// 选择配置和声明接口
			if(USBSetConfiguration(usbObj, 0) != TRUE){
				log_warn("USBSetConfiguration Failed.");
				return FALSE;
			}
			if(USBClaimInterface(usbObj, 3, 0, 0, 3) != TRUE){
				log_warn("Claim interface failed.");
				return FALSE;
			}
			return TRUE;
		}
	}
	log_warn("No suitable device found.");
	return FALSE;
}

// 初始化CMSIS-DAP设备
static BOOL init(AdapterObject *adapterObj){
	assert(adapterObj != NULL && adapterObj->ConnObject.type == USB);
	char capablity;
	struct cmsis_dap *cmsis_dapObj = CAST(struct cmsis_dap *, adapterObj);
	USBObject *usbObj = GET_USBOBJ(adapterObj);
	uint8_t command[2] = {ID_DAP_Info, DAP_ID_PACKET_SIZE}, *resp;	// 命令存储缓冲和返回值缓冲区
	if((resp = calloc(usbObj->readEPMaxPackSize, sizeof(uint8_t))) == NULL){
		log_warn("Alloc response buffer failed.");
		return FALSE;
	}
	// 获得DAP_Info 判断
	log_info("CMSIS-DAP Init.");
	// TODO 先以endpoint最大包长读取packet大小，然后读取剩下的
	log_trace("Write %d byte(s) to %s.", usbObj->Write(usbObj, command, 2, 0), adapterObj->DeviceDesc);
	log_trace("Read %d byte(s) from %s.", usbObj->Read(usbObj, resp, usbObj->readEPMaxPackSize, 0), adapterObj->DeviceDesc);
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
		log_trace("Read %d byte(s) from %s.", usbObj->Read(usbObj, resp, rest_len > usbObj->readEPMaxPackSize ? rest_len : usbObj->readEPMaxPackSize, 0), adapterObj->DeviceDesc);
	}
	// 获得CMSIS-DAP固件版本
	command[1] = DAP_ID_FW_VER;
	log_trace("Write %d byte(s) to %s.", usbObj->Write(usbObj, command, 2, 0), adapterObj->DeviceDesc);
	log_trace("Read %d byte(s) from %s.", usbObj->Read(usbObj, resp, cmsis_dapObj->PacketSize, 0), adapterObj->DeviceDesc);
	cmsis_dapObj->Version = (int)(atof(resp+2) * 100); // XXX 改成了整数
	log_info("CMSIS-DAP FW Version is %f.", cmsis_dapObj->Version / 100.0);
	// 获得CMSIS-DAP的最大包长度和最大包个数
	command[1] = DAP_ID_PACKET_COUNT;
	log_trace("Write %d byte(s) to %s.", usbObj->Write(usbObj, command, 2, 0), adapterObj->DeviceDesc);
	log_trace("Read %d byte(s) from %s.", usbObj->Read(usbObj, resp, cmsis_dapObj->PacketSize, 0), adapterObj->DeviceDesc);
	cmsis_dapObj->MaxPcaketCount = *CAST(uint8_t *, resp+2);
	log_info("CMSIS-DAP the maximum Packet Count is %d.", cmsis_dapObj->MaxPcaketCount);
	// Capabilities. The information BYTE contains bits that indicate which communication methods are provided to the Device.
	command[1] = DAP_ID_CAPABILITIES;
	log_trace("Write %d byte(s) to %s.", usbObj->Write(usbObj, command, 2, 0), adapterObj->DeviceDesc);
	log_trace("Read %d byte(s) from %s.", usbObj->Read(usbObj, resp, cmsis_dapObj->PacketSize, 0), adapterObj->DeviceDesc);

#define SET_CAP(pc,ca,bit,flag) \
	if( ((ca) & (0x1 << (bit))) != 0) (pc)->capablityFlag |= (flag);

	// XXX Version改成了整数，乘以100
	switch(cmsis_dapObj->Version){
	case 110: // 1.1f
		// 获得info0
		capablity = *CAST(uint8_t *, resp+2);
		SET_CAP(cmsis_dapObj, capablity, 2, CAP_FLAG_SWO_UART);
		SET_CAP(cmsis_dapObj, capablity, 3, CAP_FLAG_SWO_MANCHESTER);
		SET_CAP(cmsis_dapObj, capablity, 4, CAP_FLAG_ATOMIC);
		SET_CAP(cmsis_dapObj, capablity, 5, CAP_FLAG_SWD_SEQUENCE);
		SET_CAP(cmsis_dapObj, capablity, 6, CAP_FLAG_TEST_DOMAIN_TIMER);

		if(*CAST(uint8_t *, resp+1) == 2){	// 是否存在info1
			// 获得info1
			capablity = *CAST(uint8_t *, resp+3);
			SET_CAP(cmsis_dapObj, capablity, 0, CAP_FLAG_TRACE_DATA_MANAGE);
		}
		/* no break */

	case 100: // 1.0f
		// 获得info0
		capablity = *CAST(uint8_t *, resp+2);
		SET_CAP(cmsis_dapObj, capablity, 0, CAP_FLAG_SWD);
		SET_CAP(cmsis_dapObj, capablity, 1, CAP_FLAG_JTAG);
		break;

	default: break;
	}
	log_info("CMSIS-DAP Capabilities 0x%X.", cmsis_dapObj->capablityFlag);
	// 在初始化过程中不选择transport
	adapterObj->currTrans = UNKNOW_NULL;
	return TRUE;
}

/**
 * Disconnect
 */
static BOOL DAP_Disconnect(uint8_t *respBuff, AdapterObject *adapterObj){
	assert(adapterObj != NULL && adapterObj->ConnObject.type == USB);
	struct cmsis_dap *cmsis_dapObj = CAST(struct cmsis_dap *, adapterObj);
	assert(cmsis_dapObj->PacketSize != 0);
	USBObject *usbObj = GET_USBOBJ(adapterObj);
	uint8_t command[1] = {ID_DAP_Disconnect};
	log_trace("Write %d byte(s) to %s.", usbObj->Write(usbObj, command, 1, 0), adapterObj->DeviceDesc);
	log_trace("Read %d byte(s) from %s.", usbObj->Read(usbObj, respBuff, cmsis_dapObj->PacketSize, 0), adapterObj->DeviceDesc);
	// 检查返回值
	if(respBuff[1] != DAP_OK){
		log_warn("Disconnect execution failed.");
		return FALSE;
	}
	return TRUE;
}

// 反初始化
static BOOL deinit(AdapterObject *adapterObj){
	assert(adapterObj != NULL && adapterObj->ConnObject.type == USB);
	struct cmsis_dap *cmsis_dapObj = CAST(struct cmsis_dap *, adapterObj);
	assert(cmsis_dapObj->PacketSize != 0);
	// 断开连接
	return CMDAP_FUN_WARP(cmsis_dapObj->PacketSize, DAP_Disconnect, adapterObj);
}

/**
 * JTAG协议转SWD
 */
static BOOL SWJ_JTAG2SWD(uint8_t *respBuff, AdapterObject *adapterObj){
	assert(adapterObj != NULL && adapterObj->ConnObject.type == USB);
	USBObject *usbObj = GET_USBOBJ(adapterObj);
	struct cmsis_dap *cmsis_dapObj = CAST(struct cmsis_dap *, adapterObj);
	assert(cmsis_dapObj->PacketSize != 0);

	//SWJ发送 0xE79E，老版本的ARM发送0xB76D强制切换
	uint8_t switchSque[] = {ID_DAP_SWJ_Sequence, 144,
			0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // 56 bit
			0x9e, 0xe7,	// 16 bit
			0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // 56 bit
			0x00, 0x00,	// 16 bit
	};	// DAP_SWJ_Sequence Command
	usbObj->Write(usbObj, switchSque, sizeof(switchSque), 0);
	usbObj->Read(usbObj, respBuff, cmsis_dapObj->PacketSize, 0);
	return TRUE;
}

/**
 * SWD协议转JTAG
 */
static BOOL SWJ_SWD2JTAG(uint8_t *respBuff, AdapterObject *adapterObj){
	assert(adapterObj != NULL && adapterObj->ConnObject.type == USB);
	USBObject *usbObj = GET_USBOBJ(adapterObj);
	struct cmsis_dap *cmsis_dapObj = CAST(struct cmsis_dap *, adapterObj);
	assert(cmsis_dapObj->PacketSize != 0);

	//SWJ发送 0xE79E，老版本的ARM发送0xB76D强制切换
	uint8_t switchSque[] = {ID_DAP_SWJ_Sequence, 80,
			0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // 56 bit
			0x3c, 0xe7,	// 16 bit
			0xff,	// 8 bit
	};	// DAP_SWJ_Sequence Command
	usbObj->Write(usbObj, switchSque, sizeof(switchSque), 0);
	usbObj->Read(usbObj, respBuff, cmsis_dapObj->PacketSize, 0);
	return TRUE;
}

// 选择和切换传输协议
static BOOL selectTrans(AdapterObject *adapterObj, enum transportType type){
	int idx;
	assert(adapterObj != NULL && adapterObj->ConnObject.type == USB);
	USBObject *usbObj = GET_USBOBJ(adapterObj);
	struct cmsis_dap *cmsis_dapObj = CAST(struct cmsis_dap *, adapterObj);
	assert(cmsis_dapObj->PacketSize != 0);

	uint8_t command[2] = {ID_DAP_Connect, DAP_PORT_AUTODETECT}, *resp;	// 命令存储缓冲和返回值缓冲区
	if(type == UNKNOW_NULL) return FALSE;
	if(adapterObj->currTrans == type) return TRUE;
	if((resp = calloc(cmsis_dapObj->PacketSize, sizeof(uint8_t))) == NULL){
		log_error("Failed to calloc receive buffer space.");
		return FALSE;
	}
	for(idx=0; adapterObj->supportedTrans[idx] != UNKNOW_NULL; idx++){
		// 如果支持指定的协议
		if(adapterObj->supportedTrans[idx] == type){
			switch(type){
			case SWD:
				if(!CMSIS_DAP_HAS_CAPALITY(cmsis_dapObj, CAP_FLAG_SWD)){
					free(resp);
					// 不支持
					return FALSE;
				}
				// 切换到SWD模式
				command[1] = DAP_PORT_SWD;
				log_trace("Write %d byte(s) to %s.", usbObj->Write(usbObj, command, 2, 0), adapterObj->DeviceDesc);
				log_trace("Read %d byte(s) from %s.", usbObj->Read(usbObj, resp, cmsis_dapObj->PacketSize, 0), adapterObj->DeviceDesc);
				if(*CAST(uint8_t *, resp+1) != DAP_PORT_SWD){
					log_warn("Switching SWD mode failed.");
				}else{
					// 发送切换swd序列
					CMDAP_FUN_WARP(cmsis_dapObj->PacketSize, SWJ_JTAG2SWD, adapterObj);
					adapterObj->currTrans = SWD;
					log_info("Switch to SWD mode.");
				}
				break;

			case JTAG:
				if(!CMSIS_DAP_HAS_CAPALITY(cmsis_dapObj, CAP_FLAG_JTAG)){
					free(resp);
					// 不支持
					return FALSE;
				}
				// 切换到JTAG模式
				command[1] = DAP_PORT_JTAG;
				log_trace("Write %d byte(s) to %s.", usbObj->Write(usbObj, command, 2, 0), adapterObj->DeviceDesc);
				log_trace("Read %d byte(s) from %s.", usbObj->Read(usbObj, resp, cmsis_dapObj->PacketSize, 0), adapterObj->DeviceDesc);
				if(*CAST(uint8_t *, resp+1) != DAP_PORT_JTAG){
					log_warn("Switching JTAG mode failed.");
				}else{
					// 发送切换JTAG序列
					CMDAP_FUN_WARP(cmsis_dapObj->PacketSize, SWJ_SWD2JTAG, adapterObj);
					adapterObj->currTrans = JTAG;
					log_info("Switch to JTAG mode.");
				}
				break;
			}
			free(resp);
			return TRUE;
		}
	}
	free(resp);
	// 协议不支持
	return FALSE;
}

/**
 * Configure Transfers.
 * The DAP_TransferConfigure Command sets parameters for DAP_Transfer and DAP_TransferBlock.
 * —— CMSIS-DAP 文档
 * SWD、JTAG模式下均有效
 */
static BOOL DAP_TransferConfigure(uint8_t *respBuff, AdapterObject *adapterObj, uint8_t idleCycle, uint16_t waitRetry, uint16_t matchRetry){
	assert(adapterObj != NULL && adapterObj->ConnObject.type == USB);
	uint8_t DAPTransfer[6] = {ID_DAP_TransferConfigure};
	USBObject *usbObj = GET_USBOBJ(adapterObj);
	struct cmsis_dap *cmsis_dapObj = CAST(struct cmsis_dap *, adapterObj);
	assert(cmsis_dapObj->PacketSize != 0);

	DAPTransfer[1] = idleCycle;	// Number of extra idle cycles after each transfer.
	DAPTransfer[2] = BYTE_IDX(waitRetry, 0);
	DAPTransfer[3] = BYTE_IDX(waitRetry, 1);
	DAPTransfer[4] = BYTE_IDX(matchRetry, 0);
	DAPTransfer[5] = BYTE_IDX(matchRetry, 1);
	usbObj->Write(usbObj, DAPTransfer, sizeof(DAPTransfer), 0);
	usbObj->Read(usbObj, respBuff, cmsis_dapObj->PacketSize, 0);
	// 增加判断是否成功
	if(respBuff[1] != DAP_OK){
		log_warn("Transfer config execution failed.");
		return FALSE;
	}
	return TRUE;
}

/**
 * Select SWD/JTAG Clock.
 * The DAP_SWJ_Clock Command sets the clock frequency for JTAG and SWD communication mode.
 * —— CMSIS-DAP 文档
 * SWD、JTAG模式下均有效
 */
static BOOL DAP_SWJ_Clock(uint8_t *respBuff, AdapterObject *adapterObj, uint32_t clockHz){
	assert(adapterObj != NULL && adapterObj->ConnObject.type == USB);
	uint8_t clockHzPack[5] = {ID_DAP_SWJ_Clock};
	USBObject *usbObj = GET_USBOBJ(adapterObj);
	struct cmsis_dap *cmsis_dapObj = CAST(struct cmsis_dap *, adapterObj);
	assert(cmsis_dapObj->PacketSize != 0);

	clockHzPack[1] = BYTE_IDX(clockHz, 0);
	clockHzPack[2] = BYTE_IDX(clockHz, 1);
	clockHzPack[3] = BYTE_IDX(clockHz, 2);
	clockHzPack[4] = BYTE_IDX(clockHz, 3);

	usbObj->Write(usbObj, clockHzPack, sizeof(clockHzPack), 0);
	usbObj->Read(usbObj, respBuff, cmsis_dapObj->PacketSize, 0);
	// 增加判断是否成功
	if(respBuff[1] != DAP_OK){
		log_warn("SWJ Clock execution failed.");
		return FALSE;
	}
	return TRUE;
}

/**
 * 读取AP、DP寄存器
 * SWD、JTAG模式下均可调用，区分协议在DAP固件中完成
 * ——详情查看 DAP 手册
 */
static BOOL DAPRegister(uint8_t *respBuff, AdapterObject *adapterObj, uint8_t index, uint8_t request, uint32_t *reg_inout){
	assert(adapterObj != NULL && adapterObj->ConnObject.type == USB);
	USBObject *usbObj = GET_USBOBJ(adapterObj);
	struct cmsis_dap *cmsis_dapObj = CAST(struct cmsis_dap *, adapterObj);
	assert(cmsis_dapObj->PacketSize != 0);

	uint8_t DAPTransferPack[] = {ID_DAP_Transfer, 0, 0x01, 0xFF, 0, 0, 0, 0};
	DAPTransferPack[1] = index;
	DAPTransferPack[3] = request;
	if((request & DAP_TRANSFER_RnW) == DAP_TRANSFER_RnW){	// 读取寄存器
		usbObj->Write(usbObj, DAPTransferPack, 4, 0);
		usbObj->Read(usbObj, respBuff, cmsis_dapObj->PacketSize, 0);
		// 检查返回值
		if(respBuff[1] == 1 && respBuff[2] == 1){
			*reg_inout = *CAST(uint32_t *, respBuff+3);
			return TRUE;
		}else{
			log_warn("Transmission %d bytes, the last response: %x.", respBuff[1], respBuff[2]);
			return FALSE;
		}
	}else{	// 写寄存器
		DAPTransferPack[4] = BYTE_IDX(*reg_inout, 0);
		DAPTransferPack[5] = BYTE_IDX(*reg_inout, 1);
		DAPTransferPack[6] = BYTE_IDX(*reg_inout, 2);
		DAPTransferPack[7] = BYTE_IDX(*reg_inout, 3);
		usbObj->Write(usbObj, DAPTransferPack, 8, 0);
		usbObj->Read(usbObj, respBuff, cmsis_dapObj->PacketSize, 0);
		// 检查返回值
		if(respBuff[1] == 1 && respBuff[2] == 1){
			return TRUE;
		}else{
			log_warn("Transmission %d bytes, the last response: %x.", respBuff[1], respBuff[2]);
			return FALSE;
		}
	}
}

/**
 * JTAG ID Code
 */
static BOOL JTAG_IDcode(uint8_t *respBuff, AdapterObject *adapterObj, uint8_t index, uint32_t *reg_inout){
	assert(adapterObj != NULL && adapterObj->ConnObject.type == USB);
	USBObject *usbObj = GET_USBOBJ(adapterObj);
	struct cmsis_dap *cmsis_dapObj = CAST(struct cmsis_dap *, adapterObj);
	assert(cmsis_dapObj->PacketSize != 0);

	// 使状态机进入RUN-TEST/IDLE状态
	uint8_t DAPTransferPack[] = {ID_DAP_JTAG_Sequence, 2,
			0x45, 0x00, 0x01, 0x00,
//			0x42, 0x00, 0x02, 0x00,
//			0x04, 0x0e, 0x43, 0x00,
//			0x02, 0x00, /* 19 */0xa0, 0x00, 0x00, 0x00, 0x00,
//			0x42, 0x00, 0x01, 0x00
	};

	usbObj->Write(usbObj, DAPTransferPack, sizeof(DAPTransferPack), 0);
	usbObj->Read(usbObj, respBuff, cmsis_dapObj->PacketSize, 0);

	DAPTransferPack[0] = ID_DAP_JTAG_IDCODE;
	DAPTransferPack[1] = index;
	usbObj->Write(usbObj, DAPTransferPack, 2, 0);
	usbObj->Read(usbObj, respBuff, cmsis_dapObj->PacketSize, 0);
	// 检查返回值
	if(respBuff[1] == DAP_OK){
		*reg_inout = *CAST(uint32_t *, respBuff+2);
		return TRUE;
	}else{
		log_warn("Get index %d JTAG ID Code Failed.", index);
		return FALSE;
	}
	return TRUE;
}

/**
 * The DAP_JTAG_Configure Command sets the JTAG device chain information for communication with Transfer Commands.
 * The JTAG device chain needs to be iterated with DAP_JTAG_Sequence or manually configured by the debugger on the host computer.
 */
static BOOL JTAG_Configure(uint8_t *respBuff, AdapterObject *adapterObj, uint8_t count, uint8_t *irLen){
	assert(adapterObj != NULL && adapterObj->ConnObject.type == USB);
	USBObject *usbObj = GET_USBOBJ(adapterObj);
	struct cmsis_dap *cmsis_dapObj = CAST(struct cmsis_dap *, adapterObj);
	assert(cmsis_dapObj->PacketSize != 0);

	uint8_t DAPTransferPack[10] = {ID_DAP_JTAG_Configure, 0};	// 2 + 8 一般jtag scan chain最大有8个
	if(count > 8){
		log_warn("Devices count more than 8!");
		return FALSE;
	}
	DAPTransferPack[1] = count;
	// 复制irlen data
	memcpy(DAPTransferPack+2, irLen, count);
	usbObj->Write(usbObj, DAPTransferPack, count + 2, 0);
	usbObj->Read(usbObj, respBuff, cmsis_dapObj->PacketSize, 0);
	// 检查返回值
	if(respBuff[1] == DAP_OK){
		return TRUE;
	}else{
		log_warn("Set IR Length Failed.");
		return FALSE;
	}
}

/**
 * 发送JTAG时序
 */
// TODO jtag时序，swjpins

/**
 * DAP_Transfer
 * SWD和JTAG模式下均有效
 * 具体手册参考CMSIS-DAP DAP_Transfer这一小节
 * 关于response 参数所指向的内存区域，resquest中有多少个读指令，就应该备好多少个WORD
 * 的内存区域，需要多大内存自己把握
 * response 中的数据只在返回值为TRUE时有效
 */
static BOOL DAP_Transfer(uint8_t *respBuff, AdapterObject *adapterObj, uint8_t index, uint8_t count, uint8_t *data, int length, uint8_t *response){
	assert(adapterObj != NULL && adapterObj->ConnObject.type == USB);
	USBObject *usbObj = GET_USBOBJ(adapterObj);
	struct cmsis_dap *cmsis_dapObj = CAST(struct cmsis_dap *, adapterObj);
	assert(cmsis_dapObj->PacketSize != 0);

	uint8_t *DAP_TransferPacket, readCount = 0, idx;
	if((length + 3) > cmsis_dapObj->PacketSize){
		log_error("Packet too large.");
		return FALSE;
	}
	for(idx = 0; idx < length;){
		// 判断是否是读内存
		if((data[idx] & DAP_TRANSFER_RnW) == DAP_TRANSFER_RnW){
			idx += 1;
			readCount ++;
		}else{
			idx += 5;
		}
	}
	log_info("%d read requests.", readCount);
	if((DAP_TransferPacket = calloc(3 + length, sizeof(uint8_t))) == NULL){
		log_error("Failed to alloc Packet buffer.");
		return FALSE;
	}
	// 构造数据包头部
	DAP_TransferPacket[0] = ID_DAP_Transfer;
	DAP_TransferPacket[1] = index;	// DAP index, JTAG ScanChain 中的位置，在SWD模式下忽略该参数
	DAP_TransferPacket[2] = count;	// 传输多少个request
	// 将数据拷贝到包中
	memcpy(DAP_TransferPacket + 3, data, length);
	log_trace("Write %d byte(s) to %s.", usbObj->Write(usbObj, DAP_TransferPacket, 3 + length, 0), adapterObj->DeviceDesc);
	log_trace("Read %d byte(s) from %s.", usbObj->Read(usbObj, respBuff, cmsis_dapObj->PacketSize, 0), adapterObj->DeviceDesc);
	if(respBuff[1] == count && respBuff[2] == 1){
		// 拷贝数据
		memcpy(response, respBuff, readCount*sizeof(uint32_t));
		free(DAP_TransferPacket);
		return TRUE;
	}else{
		log_warn("Transmission %d bytes, the last response: %x.", respBuff[1], respBuff[2]);
		free(DAP_TransferPacket);
		return FALSE;
	}

}

/**
 * 执行仿真器指令
 */
static BOOL operate(AdapterObject *adapterObj, int operate, ...){
	assert(adapterObj != NULL && adapterObj->ConnObject.type == USB);
	va_list parames;
	BOOL result = FALSE;
	struct cmsis_dap *cmsis_dapObj = CAST(struct cmsis_dap *, adapterObj);
	va_start(parames, operate);	// 确定可变参数起始位置
	// 根据指令选择操作
	// TODO 根据协议（SWD、JTAG）选择不同的操作
	switch(operate){
	case AINS_GET_IDCODE:
		log_trace("Execution command: Get ID Code.");
		do{
			// 获得index
			uint8_t dap_index = (uint8_t)va_arg(parames, int);
			uint32_t *idCode_out = va_arg(parames, uint32_t *);
			result = CMDAP_FUN_WARP(cmsis_dapObj->PacketSize, DAPRegister, adapterObj, dap_index, DAP_TRANSFER_RnW | DP_IDCODE, idCode_out);
		}while(0);
		break;

	case AINS_JTAG_SEQUENCE:
		log_trace("Execution command: JTAG Sequence.");
		// TODO 发送一个JTAG时序
		break;

	case AINS_SWD_SEQUENCE:
		log_trace("Execution command: SWD Sequence.");
		break;

	case CMDAP_SET_CLOCK:
		log_trace("Execution command: Set SWJ Clock.");
		do{
			uint32_t clockHz = va_arg(parames, uint32_t);
			log_debug("clockHz: %d.", clockHz);
			result = CMDAP_FUN_WARP(cmsis_dapObj->PacketSize, DAP_SWJ_Clock, adapterObj, clockHz);
		}while(0);
		break;

	case CMDAP_TRANSFER_CONFIG:
		log_trace("Execution command: Transfer Config.");
		do{
			uint8_t idleCycle = (uint8_t)va_arg(parames, int);	// 在每次传输后面加多少个空闲时钟周期
			uint16_t waitRetry = (uint16_t)va_arg(parames, int);	// 得到WAIT Response后重试次数
			uint16_t matchRetry = (uint16_t)va_arg(parames, int);	// 在匹配模式下不匹配时重试次数
			log_debug("idle:%d, wait:%d, match:%d.", idleCycle, waitRetry, matchRetry);
			result = CMDAP_FUN_WARP(cmsis_dapObj->PacketSize, DAP_TransferConfigure, adapterObj, idleCycle, waitRetry, matchRetry);
		}while(0);
		break;

	case CMDAP_READ_DP_REG:
		log_trace("Execution command: Read DP Register.");
		do{
			// 获得index
			uint8_t dap_index = (uint8_t)va_arg(parames, int);
			// 获得地址
			uint8_t address = (uint8_t)va_arg(parames, int);
			address &= (DAP_TRANSFER_A2 | DAP_TRANSFER_A3);	// 提取地址
			uint32_t *reg_out = va_arg(parames, uint32_t *);	// 数据输出地址
			result = CMDAP_FUN_WARP(cmsis_dapObj->PacketSize, DAPRegister, adapterObj, dap_index, DAP_TRANSFER_RnW | address, reg_out);
		}while(0);

		break;

	case CMDAP_READ_AP_REG:
		log_trace("Execution command: Read AP Register.");
		do{
			// 获得index
			uint8_t dap_index = (uint8_t)va_arg(parames, int);
			// 获得地址
			uint8_t address = (uint8_t)va_arg(parames, int);
			address &= (DAP_TRANSFER_A2 | DAP_TRANSFER_A3);	// 提取地址
			uint32_t *reg_out = va_arg(parames, uint32_t *);	// 数据输出地址
			result = CMDAP_FUN_WARP(cmsis_dapObj->PacketSize, DAPRegister, adapterObj, dap_index, DAP_TRANSFER_RnW | DAP_TRANSFER_APnDP | address, reg_out);
		}while(0);
		break;

	case CMDAP_WRITE_DP_REG:
		log_trace("Execution command: Write DP Register.");
		do{
			// 获得index
			uint8_t dap_index = (uint8_t)va_arg(parames, int);
			// 获得地址
			uint8_t address = (uint8_t)va_arg(parames, int);
			address &= (DAP_TRANSFER_A2 | DAP_TRANSFER_A3);	// 提取地址
			uint32_t reg_in = (uint32_t)va_arg(parames, int);	// 数据地址
			result = CMDAP_FUN_WARP(cmsis_dapObj->PacketSize, DAPRegister, adapterObj, dap_index, address, &reg_in);
		}while(0);
		break;

	case CMDAP_WRITE_AP_REG:
		log_trace("Execution command: Write AP Register.");
		do{
			// 获得index
			uint8_t dap_index = (uint8_t)va_arg(parames, int);
			// 获得地址
			uint8_t address = (uint8_t)va_arg(parames, int);
			address &= (DAP_TRANSFER_A2 | DAP_TRANSFER_A3);	// 提取地址
			uint32_t reg_in = (uint32_t)va_arg(parames, int);	// 数据地址
			result = CMDAP_FUN_WARP(cmsis_dapObj->PacketSize, DAPRegister, adapterObj, dap_index, DAP_TRANSFER_APnDP | address, &reg_in);
		}while(0);
		break;

	case CMDAP_TRANSFER:
		log_trace("Execution command: DAP Transfer.");
		do {
			uint8_t dap_index = (uint8_t)va_arg(parames, int);
			uint8_t count = (uint8_t)va_arg(parames, int);
			uint8_t *data = va_arg(parames, uint8_t *);
			int length = va_arg(parames, int);
			uint8_t *response = va_arg(parames, uint8_t *);
			result = CMDAP_FUN_WARP(cmsis_dapObj->PacketSize, DAP_Transfer, adapterObj, dap_index, count, data, length, response);
		}while(0);
		break;

	case CMDAP_TRANSFER_BLOCK:
		log_trace("Execution command: DAP Transfer Block.");

		break;

	case CMDAP_TRANSFER_ABORT:
		log_trace("Execution command: DAP Transfer Abort.");

		break;

	case CMDAP_JTAG_IDCODE:
		log_trace("Execution command: JTAG IDCODE.");
		do{
			uint8_t dap_index = (uint8_t)va_arg(parames, int);
			uint32_t *idCode = va_arg(parames, uint32_t *);
			result = CMDAP_FUN_WARP(cmsis_dapObj->PacketSize, JTAG_IDcode, adapterObj, dap_index, idCode);
		}while(0);
		break;

	case CMDAP_JTAG_CONFIGURE:
		log_trace("Execution command: JTAG CONFIGURE.");
		do{
			uint8_t count = (uint8_t)va_arg(parames, int);
			uint8_t *irLen = va_arg(parames, uint8_t *);
			result = CMDAP_FUN_WARP(cmsis_dapObj->PacketSize, JTAG_Configure, adapterObj, count, irLen);
		}while(0);
		break;
	default:
		log_warn("Unsupported operation: %d.", operate);
		break;
	}
	va_end(parames);
	return result;
}

