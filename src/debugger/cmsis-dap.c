/*
 * cmsis-dap.c
 *
 *  Created on: 2018-2-18
 *      Author: virusv
 */

#include <stdlib.h>
#include <stdint.h>

#include "smart_ocd.h"
#include "misc/log.h"
#include "debugger/cmsis-dap.h"
#include "arch/CoreSight/DAP.h"

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
			// 把usb max packet最大包长复制到cmsis-dap对象中
			cmsis_dapObj->SendPacketSize = usbObj->writeEPMaxPackSize;
			cmsis_dapObj->ReceivePacketSize = usbObj->readEPMaxPackSize;
			if(cmsis_dapObj->SendPacketSize != cmsis_dapObj->ReceivePacketSize){
				log_info("The maximum length of communication data packets between the two parties is inconsistent.Rx:%d,Tx:%d.",
						cmsis_dapObj->ReceivePacketSize, cmsis_dapObj->SendPacketSize);
			}
			return TRUE;
		}
	}
	log_warn("No suitable device found.");
	return FALSE;
}

// 读取应答包并丢弃
static void discardPacket(AdapterObject *adapterObj){
	assert(adapterObj != NULL && adapterObj->ConnObject.type == USB);
	int packetSize; char capablity;
	struct cmsis_dap *cmsis_dapObj = CAST(struct cmsis_dap *, adapterObj);
	USBObject *usbObj = GET_USBOBJ(adapterObj);
	uint8_t *response;
	if((response = calloc(cmsis_dapObj->ReceivePacketSize, sizeof(uint8_t))) == NULL){
		log_error("Failed to calloc receive buffer space. Cannot Discard.");
	}
	usbObj->Read(usbObj, response, cmsis_dapObj->ReceivePacketSize, 0);
	free(response);
}

// 初始化CMSIS-DAP设备
static BOOL init(AdapterObject *adapterObj){
	assert(adapterObj != NULL && adapterObj->ConnObject.type == USB);
	int packetSize; char capablity;
	struct cmsis_dap *cmsis_dapObj = CAST(struct cmsis_dap *, adapterObj);
	USBObject *usbObj = GET_USBOBJ(adapterObj);
	uint8_t command[2] = {ID_DAP_Info, DAP_ID_FW_VER}, resp[64];	// 命令存储缓冲和返回值缓冲区
	// XXX 初始化的时候读取包长度是固定64字节
	// 获得DAP_Info 判断
	log_info("CMSIS-DAP Init.");
	// 获得CMSIS-DAP固件版本
	log_trace("Write %d byte(s) to %s.", usbObj->Write(usbObj, command, 2, 0), adapterObj->DeviceDesc);
	log_trace("Read %d byte(s) from %s.", usbObj->Read(usbObj, resp, 64, 0), adapterObj->DeviceDesc);
	cmsis_dapObj->Version = (int)(atof(resp+2) * 100); // XXX 改成了整数
	log_info("CMSIS-DAP FW Version is %f.", cmsis_dapObj->Version / 100.0);
	// 获得CMSIS-DAP的最大包长度和最大包个数
	command[1] = DAP_ID_PACKET_COUNT;
	log_trace("Write %d byte(s) to %s.", usbObj->Write(usbObj, command, 2, 0), adapterObj->DeviceDesc);
	log_trace("Read %d byte(s) from %s.", usbObj->Read(usbObj, resp, 64, 0), adapterObj->DeviceDesc);
	cmsis_dapObj->MaxPcaketCount = *CAST(uint8_t *, resp+2);
	log_info("CMSIS-DAP the maximum Packet Count is %d.", cmsis_dapObj->MaxPcaketCount);
	command[1] = DAP_ID_PACKET_SIZE;
	log_trace("Write %d byte(s) to %s.", usbObj->Write(usbObj, command, 2, 0), adapterObj->DeviceDesc);
	log_trace("Read %d byte(s) from %s.", usbObj->Read(usbObj, resp, 64, 0), adapterObj->DeviceDesc);
	packetSize = *CAST(uint16_t *, resp+2);
	// XXX 这里的包长度是从CMSIS-DAP固件中获得的，在libusb驱动层，当包长度和端点发送的最大长度不相等的时候可能造成OVERFLOW
	cmsis_dapObj->SendPacketSize = packetSize > cmsis_dapObj->SendPacketSize ? : packetSize;
	cmsis_dapObj->ReceivePacketSize = packetSize > cmsis_dapObj->ReceivePacketSize ? : packetSize;
	log_info("CMSIS-DAP the maximum Packet Size is Tx:%d, Rx:%d.", cmsis_dapObj->SendPacketSize, cmsis_dapObj->ReceivePacketSize);
	// Capabilities. The information BYTE contains bits that indicate which communication methods are provided to the Device.
	command[1] = DAP_ID_CAPABILITIES;
	log_trace("Write %d byte(s) to %s.", usbObj->Write(usbObj, command, 2, 0), adapterObj->DeviceDesc);
	log_trace("Read %d byte(s) from %s.", usbObj->Read(usbObj, resp, 64, 0), adapterObj->DeviceDesc);

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
	log_info("CMSIS-DAP Capabilities %X.", cmsis_dapObj->capablityFlag);
	// 在初始化过程中不选择transport
	adapterObj->currTrans = UNKNOW_NULL;
	return TRUE;
}

// 反初始化
static BOOL deinit(AdapterObject *adapterObj){
	assert(adapterObj != NULL && adapterObj->ConnObject.type == USB);
	USBObject *usbObj = GET_USBOBJ(adapterObj);
	uint8_t command[1] = {ID_DAP_Disconnect}, response[64];
	log_trace("Write %d byte(s) to %s.", usbObj->Write(usbObj, command, 1, 0), adapterObj->DeviceDesc);
	log_trace("Read %d byte(s) from %s.", usbObj->Read(usbObj, response, 64, 0), adapterObj->DeviceDesc);
	// 检查返回值
	if(response[1] != DAP_OK){
		log_warn("Disconnect execution failed.");
		return FALSE;
	}
	return TRUE;
}

/**
 * JTAG协议转SWD
 */
static void SWJ_JTAG2SWD(AdapterObject *adapterObj){
	assert(adapterObj != NULL && adapterObj->ConnObject.type == USB);
	USBObject *usbObj = GET_USBOBJ(adapterObj);
	//SWJ发送 0xE79E，老版本的ARM发送0xB76D强制切换
	uint8_t switchSque[] = {ID_DAP_SWJ_Sequence, 144,
			0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // 56 bit
			0x9e, 0xe7,	// 16 bit
			0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // 56 bit
			0x00, 0x00,	// 16 bit
	};	// DAP_SWJ_Sequence Command
	usbObj->Write(usbObj, switchSque, sizeof(switchSque), 0);
	// 丢弃它的相应包
	discardPacket(adapterObj);
}

// 选择和切换传输协议
static BOOL selectTrans(AdapterObject *adapterObj, enum transportType type){
	int idx;
	assert(adapterObj != NULL && adapterObj->ConnObject.type == USB);
	USBObject *usbObj = GET_USBOBJ(adapterObj);
	struct cmsis_dap *cmsis_dapObj = CAST(struct cmsis_dap *, adapterObj);
	uint8_t command[2] = {ID_DAP_Connect, DAP_PORT_AUTODETECT}, resp[64];	// 命令存储缓冲和返回值缓冲区
	if(type == UNKNOW_NULL) return FALSE;
	if(adapterObj->currTrans == type) return TRUE;
	for(idx=0; adapterObj->supportedTrans[idx] != UNKNOW_NULL; idx++){
		// 如果支持指定的协议
		if(adapterObj->supportedTrans[idx] == type){
			switch(type){
			case SWD:
				if(!CMSIS_DAP_HAS_CAPALITY(cmsis_dapObj, CAP_FLAG_SWD)){
					// 不支持
					return FALSE;
				}
				// 切换到SWD模式
				command[1] = DAP_PORT_SWD;
				log_trace("Write %d byte(s) to %s.", usbObj->Write(usbObj, command, 2, 0), adapterObj->DeviceDesc);
				log_trace("Read %d byte(s) from %s.", usbObj->Read(usbObj, resp, 64, 0), adapterObj->DeviceDesc);
				if(*CAST(uint8_t *, resp+1) != DAP_PORT_SWD){
					log_warn("Switching SWD mode failed.");
				}
				// 发送切换swd序列
				SWJ_JTAG2SWD(adapterObj);
				adapterObj->currTrans = SWD;
				log_info("Switch to SWD mode.");
				break;

			case JTAG:
				if(!CMSIS_DAP_HAS_CAPALITY(cmsis_dapObj, CAP_FLAG_JTAG)){
					// 不支持
					return FALSE;
				}
				// 切换到JTAG模式
				command[1] = DAP_PORT_JTAG;
				log_trace("Write %d byte(s) to %s.", usbObj->Write(usbObj, command, 2, 0), adapterObj->DeviceDesc);
				log_trace("Read %d byte(s) from %s.", usbObj->Read(usbObj, resp, 64, 0), adapterObj->DeviceDesc);
				if(*CAST(uint8_t *, resp+1) != DAP_PORT_JTAG){
					log_warn("Switching JTAG mode failed.");
				}
				// TODO 发送切换JTAG序列

				adapterObj->currTrans = JTAG;
				log_info("Switch to JTAG mode.");
				break;
			}
			return TRUE;
		}
	}
	// 协议不支持
	return FALSE;
}

/**
 * Configure Transfers.
 * The DAP_TransferConfigure Command sets parameters for DAP_Transfer and DAP_TransferBlock.
 * —— CMSIS-DAP 文档
 */
static BOOL TransferConfig(AdapterObject *adapterObj, uint8_t idleCycle, uint16_t waitRetry, uint16_t matchRetry){
	uint8_t DAPTransfer[6] = {ID_DAP_TransferConfigure}, *response;
	USBObject *usbObj = GET_USBOBJ(adapterObj);
	struct cmsis_dap *cmsis_dapObj = CAST(struct cmsis_dap *, adapterObj);
	DAPTransfer[1] = idleCycle;	// Number of extra idle cycles after each transfer.
	DAPTransfer[2] = BYTE_IDX(waitRetry, 0);
	DAPTransfer[3] = BYTE_IDX(waitRetry, 1);
	DAPTransfer[4] = BYTE_IDX(matchRetry, 0);
	DAPTransfer[5] = BYTE_IDX(matchRetry, 1);
	if((response = calloc(cmsis_dapObj->ReceivePacketSize, sizeof(uint8_t))) == NULL){
		log_error("Failed to calloc receive buffer space.");
		return FALSE;
	}
	usbObj->Write(usbObj, DAPTransfer, sizeof(DAPTransfer), 0);
	usbObj->Read(usbObj, response, cmsis_dapObj->ReceivePacketSize, 0);
	// 增加判断是否成功
	if(response[1] != DAP_OK){
		free(response);
		log_warn("Transfer config execution failed.");
		return FALSE;
	}
	free(response);
	return TRUE;
}

/**
 * Select SWD/JTAG Clock.
 * The DAP_SWJ_Clock Command sets the clock frequency for JTAG and SWD communication mode.
 * —— CMSIS-DAP 文档
 */
static BOOL SWJ_Clock(AdapterObject *adapterObj, uint32_t clockHz){
	uint8_t clockHzPack[5] = {ID_DAP_SWJ_Clock}, *response;
	USBObject *usbObj = GET_USBOBJ(adapterObj);
	struct cmsis_dap *cmsis_dapObj = CAST(struct cmsis_dap *, adapterObj);
	clockHzPack[1] = BYTE_IDX(clockHz, 0);
	clockHzPack[2] = BYTE_IDX(clockHz, 1);
	clockHzPack[3] = BYTE_IDX(clockHz, 2);
	clockHzPack[4] = BYTE_IDX(clockHz, 3);
	if((response = calloc(cmsis_dapObj->ReceivePacketSize, sizeof(uint8_t))) == NULL){
		log_error("Failed to calloc receive buffer space.");
		return FALSE;
	}
	usbObj->Write(usbObj, clockHzPack, sizeof(clockHzPack), 0);
	usbObj->Read(usbObj, response, cmsis_dapObj->ReceivePacketSize, 0);
	// 增加判断是否成功
	if(response[1] != DAP_OK){
		free(response);
		log_warn("SWJ Clock execution failed.");
		return FALSE;
	}
	free(response);
	return TRUE;
}

/**
 * 获得DAP的IDCode
 * 32位的ID code
 */
static BOOL SWDGetIDCode(AdapterObject *adapterObj, uint32_t *idCode_out){
	uint8_t DAPTransferPack[] = {ID_DAP_Transfer, 0x00, 0x01, DAP_TRANSFER_RnW | DP_IDCODE}, *response;
	USBObject *usbObj = GET_USBOBJ(adapterObj);
	struct cmsis_dap *cmsis_dapObj = CAST(struct cmsis_dap *, adapterObj);
	if((response = calloc(cmsis_dapObj->ReceivePacketSize, sizeof(uint8_t))) == NULL){
		log_error("Failed to calloc receive buffer space.");
		return FALSE;
	}
	usbObj->Write(usbObj, DAPTransferPack, sizeof(DAPTransferPack), 0);
	usbObj->Read(usbObj, response, cmsis_dapObj->ReceivePacketSize, 0);
	if(response[1] == 1 && response[2] == 1){
		*idCode_out = *CAST(uint32_t *, response+3);
		return TRUE;
	}else{
		log_warn("Transmission %d bytes, the last response: %x.", response[1], response[2]);
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
	va_start(parames, operate);	// 确定可变参数起始位置
	// 根据指令选择操作
	switch(operate){
	case AINS_GET_IDCODE:
		log_trace("Execution command: Get ID Code.");
		do{
			uint32_t *idCode_out = va_arg(parames, uint32_t *);
			SWDGetIDCode(adapterObj, idCode_out);
		}while(0);
		break;

	case AINS_SET_CLOCK:
		log_trace("Execution command: Set SWJ Clock.");
		do{
			uint32_t clockHz = va_arg(parames, uint32_t);
			log_debug("clockHz: %d.", clockHz);
			result = SWJ_Clock(adapterObj, clockHz);
		}while(0);
		break;

	case AINS_TRANSFER_CONFIG:
		log_trace("Execution command: Transfer Config.");
		do{
			uint8_t idleCycle = (uint8_t)va_arg(parames, int);	// 在每次传输后面加多少个空闲时钟周期
			uint16_t waitRetry = (uint16_t)va_arg(parames, int);	// 得到WAIT Response后重试次数
			uint16_t matchRetry = (uint16_t)va_arg(parames, int);	// 在匹配模式下不匹配时重试次数
			log_debug("idle:%d, wait:%d, match:%d.", idleCycle, waitRetry, matchRetry);
			result = TransferConfig(adapterObj, idleCycle, waitRetry, matchRetry);
		}while(0);
		break;

	default:
		log_warn("Unsupported operation: %d.", operate);
		break;
	}
	va_end(parames);
	return result;
}

