/*
 * dap_test.c
 *
 *  Created on: 2018-4-14
 *      Author: virusv
 */


#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "misc/log.h"
#include "misc/misc.h"
#include "debugger/cmsis-dap.h"
#include "debugger/adapter.h"


uint16_t vids[] = {0xc251, 0};
uint16_t pids[] = {0xf001, 0};

/**
 * 打印AP信息
 */
static void printAPInfo(uint32_t APIDR){
	AP_IDR_Parse parse;
	parse.regData = APIDR;
	if(parse.regInfo.Class == 0x0){	// JTAG AP
		printf("JTAG-AP:JTAG Connection to this AP\n");
	}else if(parse.regInfo.Class ==0x8){	// MEM-AP
		printf("MEM-AP:");
		switch(parse.regInfo.Type){
		case 0x1:
			printf("AMBA AHB bus");
			break;
		case 0x2:
			printf("AMBA APB2 or APB3 bus");
			break;
		case 0x4:
			printf("AMBA AXI3 or AXI4 bus, with optional ACT-Lite support");
		}
		printf(" connection to this AP.\n");
	}
	printf("Revision:0x%x, Manufacturer:0x%x, Variant:0x%x.\n",
			parse.regInfo.Revision,
			parse.regInfo.JEP106Code,
			parse.regInfo.Variant);

}

//// 打印组件信息
//static void printComponentInfo(DAPObject *dapObj, uint32_t startAddr){
//	uint32_t addr, tmp, size;
//	addr = (startAddr & ~0xfff) + 0xFF4;
//	//log_info("addr:0x%08X.", addr);
//	// 读取
//	DAP_AP_WriteReg(dapObj, AP_REG_TAR_LSB, addr);
//	DAP_AP_ReadReg(dapObj, AP_REG_DRW, &tmp);
//	//log_info("CIDR4:0x%08X.", tmp);
//	// 判断组件类型
//	switch(tmp>>4){
//	case 0x0:
//		printf("Generic verification componment");
//		break;
//	case 0x1:
//		printf("ROM Table");
//		break;
//	case 0x9:
//		printf("Debug componment");
//		break;
//	case 0xb:
//		printf("Peripheral Test Block(PTB)");
//		break;
//	case 0xd:
//		printf("OptimoDE Data Engine Subsystem (DESS) componment");
//		break;
//	case 0xe:
//		printf("Generic IP Componment");
//		break;
//	case 0xf:
//		printf("PrimeCell peripheral");
//		break;
//	default:
//		printf("Unknow componment");
//		break;
//	}
//	// 计算组件占用空间大小
//	addr = (startAddr & ~0xfff) + 0xFD0;
//	DAP_AP_WriteReg(dapObj, AP_REG_TAR_LSB, addr);
//	DAP_AP_ReadReg(dapObj, AP_REG_DRW, &tmp);
//	//log_info("PIDR4:0x%08X.", tmp);
//	tmp = (tmp >> 4);
//	size = pow(2, tmp);
//	printf(",occupies %d blocks.\n", size);
//}
#define USE_JTAG
int main(){
	struct cmsis_dap cmsis_dapObj;
	AdapterObject *adapterObj = CAST(AdapterObject *, &cmsis_dapObj);
	log_set_level(LOG_DEBUG);
	uint32_t dpidr = 0;
	// 初始化CMSIS-DAP对象
	if(NewCMSIS_DAP(&cmsis_dapObj) == FALSE){
		log_fatal("failed to new cmsis dap.");
		return -1;
	}
	// 连接
	if(Connect_CMSIS_DAP(&cmsis_dapObj, vids, pids, NULL) == FALSE){
		log_fatal("failed to connect.");
		return -1;
	}
	// 初始化adapter
	adapterObj->Init(adapterObj);
	adapter_SetStatus(adapterObj, ADAPTER_STATUS_RUNING);
	// 设置频率
	adapter_SetClock(adapterObj, 5000000);	// 5MHz
	// CMSIS-DAP专用函数
	CMSIS_DAP_TransferConfigure(adapterObj,  5, 5, 5);
	CMSIS_DAP_SWD_Configure(adapterObj, 0);
#ifdef USE_JTAG
	// 选择JTAG传输方式
	if(adapter_SelectTransmission(adapterObj, JTAG) == FALSE){
		log_fatal("failed to select JTAG.");
		return -1;
	}
	// 复位
	adapter_Reset(adapterObj, FALSE, FALSE, 0);
	uint32_t idcode[2] = {0,0};
	adapter_JTAG_StatusChange(adapterObj, JTAG_TAP_DRSHIFT);
	adapter_JTAG_Exchange_IO(adapterObj, CAST(uint8_t *, idcode), 64);
	adapter_JTAG_StatusChange(adapterObj, JTAG_TAP_IDLE);	// 要保持在IDLE状态，JTAG模式下DAP_Transfer才能用
	adapter_JTAG_Execute(adapterObj);
	log_debug("Origin Method read IDCODE: 0x%08X, 0x%08X.", idcode[0], idcode[1]);

	uint8_t irs[2] = {4, 5};
	// 设置JTAG信息
	CMSIS_DAP_JTAG_Configure(adapterObj, 2, irs);
	CMSIS_DAP_TransferConfigure(adapterObj, 5, 5, 5);
	// 选中第0个TAP为DAP
	adapter_DAP_Index(adapterObj, 0);
#else
	// 选择SWD传输方式
	if(adapter_SelectTransmission(adapterObj, SWD) == FALSE){
		log_fatal("failed to select SWD.");
		return -1;
	}
#endif
	// 初始化DAP
	if(DAP_Init(adapterObj) == FALSE){
		log_fatal("Failed To Init DAP!");
		return -1;
	}
	// 寻找AP
	uint8_t apIdx =0;

	if(DAP_Find_AP(adapterObj, AP_TYPE_AMBA_AHB, &apIdx) == FALSE){
		log_fatal("Find AHB-AP Failed!");
		return -1;
	}else{
		log_info("AHB-AP in %d.", apIdx);
	}

	if(DAP_WriteCSW(adapterObj, 0xa2000000) == FALSE){	//
		log_fatal("Write CSW Failed!");
		return -1;
	}
	uint32_t csw = 0;
	if(DAP_ReadCSW(adapterObj, &csw) == FALSE){
		log_fatal("Read CSW Failed!");
		return -1;
	}
	log_debug("CSW:0x%08X", csw);
	log_debug("CTRL:0x%08X", adapterObj->dap.CTRL_STAT_Reg.regData);
	// 写入TAR
	if(DAP_Write_TAR(adapterObj, 0xE000ED00) == FALSE){
		log_fatal("Write TAR Failed!");
		return -1;
	}
	// 读取数据测试
	uint32_t temp_data;
	if(adapter_DAP_Read_AP_Single(adapterObj, DRW, &temp_data, TRUE) == FALSE){
		log_fatal("adapter_DAP_Read_AP_Single Failed!");
		return -1;
	}
	if(adapter_DAP_Execute(adapterObj) == FALSE){
		log_fatal("adapter_DAP_Execute Failed!");
		return -1;
	}
	log_debug("data:0x%08X, CTRL:0x%08X", temp_data, adapterObj->dap.CTRL_STAT_Reg.regData);

	// block写入

	adapterObj->Deinit(adapterObj);	// 断开连接
	adapterObj->Destroy(adapterObj);	// 销毁结构
	return 0;


	/**
	 *  probe AP: 0x0 => 0x24770004
		MEM-AP:AMBA AXI3 or AXI4 bus, with optional ACT-Lite support connection to this AP.
		Revision:0x2, Manufacturer:0x23b, Variant:0x0.
		---------------------------
		probe AP: 0x1 => 0x24770002
		MEM-AP:AMBA APB2 or APB3 bus connection to this AP.
		Revision:0x2, Manufacturer:0x23b, Variant:0x0.
		---------------------------
		probe AP: 0x2 => 0x14760010
		JTAG-AP:JTAG Connection to this AP
		Revision:0x1, Manufacturer:0x23b, Variant:0x1.
		---------------------------
	 */

}
