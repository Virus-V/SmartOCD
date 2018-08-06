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
	int apIdx = DAP_Find_AP(adapterObj, AP_TYPE_AMBA_AHB);
	if(apIdx == -1){
		log_fatal("Find APB-AP Failed!");
		return -1;
	}else{
		log_info("APB-AP in %d.", apIdx);
	}
	// 写入TAR
//	if(DAP_Write_TAR(adapterObj, 0x08000000u) == FALSE){
//		log_fatal("Write TAR Failed!");
//		return -1;
//	}
	// 读取数据测试
	uint32_t data_tmp;
	uint8_t char_tmp;
	log_info("Word Read Test.");
	if(DAP_ReadMem32(adapterObj, 0x08000000u, &data_tmp) == FALSE){
		log_fatal("Read 0x08000000 Failed!");
		return -1;
	}else{
		log_info("0x08000000 => 0x%08X.", data_tmp);
	}

	if(DAP_ReadMem32(adapterObj, 0x08000010u, &data_tmp) == FALSE){
		log_fatal("Read 0x08000010 Failed!");
		return -1;
	}else{
		log_info("0x08000010 => 0x%08X.", data_tmp);
	}
	log_info("Byte Read Test.");
	// 读取字节
	if(DAP_ReadMem8(adapterObj, 0x08000000u, &char_tmp) == FALSE){
		log_fatal("Read 0x08000000 Failed!");
		return -1;
	}else{
		log_info("0x08000000 => 0x%08X.", char_tmp);
	}
	if(DAP_ReadMem8(adapterObj, 0x08000011u, &char_tmp) == FALSE){
		log_fatal("Read 0x08000011 Failed!");
		return -1;
	}else{
		log_info("0x08000011 => 0x%08X.", char_tmp);
	}
	if(DAP_ReadMem8(adapterObj, 0x08000002u, &char_tmp) == FALSE){
		log_fatal("Read 0x08000002 Failed!");
		return -1;
	}else{
		log_info("0x08000002 => 0x%08X.", char_tmp);
	}
	if(DAP_ReadMem8(adapterObj, 0x08000013u, &char_tmp) == FALSE){
		log_fatal("Read 0x08000013 Failed!");
		return -1;
	}else{
		log_info("0x08000013 => 0x%08X.", char_tmp);
	}

	uint16_t hword_tmp;
	// 读取半字
	log_info("Half word Read Test.");
	if(DAP_ReadMem16(adapterObj, 0x08000000u, &hword_tmp) == FALSE){
		log_fatal("Read 0x08000000 Failed!");
		return -1;
	}else{
		log_info("0x08000000 => 0x%08X.", hword_tmp);
	}
	if(DAP_ReadMem16(adapterObj, 0x08000002u, &hword_tmp) == FALSE){
		log_fatal("Read 0x08000002 Failed!");
		return -1;
	}else{
		log_info("0x08000002 => 0x%08X.", hword_tmp);
	}

	if(DAP_ReadMem16(adapterObj, 0x08000010u, &hword_tmp) == FALSE){
		log_fatal("Read 0x08000010 Failed!");
		return -1;
	}else{
		log_info("0x08000010 => 0x%08X.", hword_tmp);
	}
	if(DAP_ReadMem16(adapterObj, 0x08000012u, &hword_tmp) == FALSE){
		log_fatal("Read 0x08000012 Failed!");
		return -1;
	}else{
		log_info("0x08000012 => 0x%08X.", hword_tmp);
	}

	log_info("Word Write Test.");
	// 写入数据测试
	if(DAP_WriteMem32(adapterObj, 0x20000000u, 0x12345678u) == FALSE){
		log_fatal("Write 0x20000000 Failed!");
		return -1;
	}
	// 读取结果
	if(DAP_ReadMem32(adapterObj, 0x20000000u, &data_tmp) == FALSE){
		log_fatal("Read 0x20000000 Failed!");
		return -1;
	}else{
		log_info("0x20000000 => 0x%08X.", data_tmp);
	}

	log_info("Byte Write Test.");
	// 清零
	if(DAP_WriteMem32(adapterObj, 0x20000000u, 0x0u) == FALSE){
		log_fatal("Write 0x20000000 Failed!");
		return -1;
	}
	// 写入字节
	if(DAP_WriteMem8(adapterObj, 0x20000000u, 0x12u) == FALSE){
		log_fatal("Write 0x20000000 Failed!");
		return -1;
	}
	if(DAP_WriteMem8(adapterObj, 0x20000001u, 0x34u) == FALSE){
		log_fatal("Write 0x20000001 Failed!");
		return -1;
	}
	if(DAP_WriteMem8(adapterObj, 0x20000002u, 0x56u) == FALSE){
		log_fatal("Write 0x20000002 Failed!");
		return -1;
	}
	if(DAP_WriteMem8(adapterObj, 0x20000003u, 0x78u) == FALSE){
		log_fatal("Write 0x20000003 Failed!");
		return -1;
	}
	// 读取结果
	if(DAP_ReadMem32(adapterObj, 0x20000000u, &data_tmp) == FALSE){
		log_fatal("Read 0x20000000 Failed!");
		return -1;
	}else{
		log_info("0x20000000 => 0x%08X.", data_tmp);
	}

	log_info("Half Word Write Test.");
	if(DAP_WriteMem16(adapterObj, 0x20000000u, 0xdeadu) == FALSE){
		log_fatal("Write 0x20000000 Failed!");
		return -1;
	}
	if(DAP_WriteMem16(adapterObj, 0x20000002u, 0xbeefu) == FALSE){
		log_fatal("Write 0x20000002 Failed!");
		return -1;
	}
	// 读取结果
	if(DAP_ReadMem32(adapterObj, 0x20000000u, &data_tmp) == FALSE){
		log_fatal("Read 0x20000000 Failed!");
		return -1;
	}else{
		log_info("0x20000000 => 0x%08X.", data_tmp);
	}
	// block读取测试
	do{
		uint64_t baseAddr = 0x08000000u;
		uint32_t *block_tmp;
		block_tmp = calloc(2056, sizeof(uint32_t));
		assert(block_tmp != NULL);

		log_info("Block Read Test.");
//		DAP_Write_TAR(adapterObj, 0x08000000u);
//		adapter_DAP_Read_AP_Block(adapterObj, DRW, block_tmp, 16, TRUE);
//		adapter_DAP_Execute(adapterObj);
		log_info("Read size 8, result:%d.",
				DAP_ReadMemBlock(adapterObj, baseAddr, DAP_ADDRINC_SINGLE, DAP_DATA_SIZE_8, 2056, block_tmp));
		misc_PrintBulk(CAST(uint8_t *, block_tmp), sizeof(block_tmp), 16);

		memset(block_tmp, 0x0, sizeof(block_tmp));
		log_info("Read size 16, result:%d.",
				DAP_ReadMemBlock(adapterObj, baseAddr, DAP_ADDRINC_SINGLE, DAP_DATA_SIZE_16, 2056, block_tmp));
		misc_PrintBulk(CAST(uint8_t *, block_tmp), sizeof(block_tmp), 16);

//		memset(block_tmp, 0x0, sizeof(block_tmp));
//		log_info("Read size 32, result:%d.",
//				DAP_ReadMemBlock(adapterObj, baseAddr, DAP_ADDRINC_SINGLE, DAP_DATA_SIZE_32, 2056, block_tmp));
//		misc_PrintBulk(CAST(uint8_t *, block_tmp), sizeof(block_tmp), 16);
//
//		log_info("AddrInc packed\n Read size 8, result:%d.",
//				DAP_ReadMemBlock(adapterObj, baseAddr, DAP_ADDRINC_PACKED, DAP_DATA_SIZE_8, 2056, block_tmp));
//		misc_PrintBulk(CAST(uint8_t *, block_tmp), sizeof(block_tmp), 16);
//
//		memset(block_tmp, 0x0, sizeof(block_tmp));
//		log_info("Read size 16, result:%d.",
//				DAP_ReadMemBlock(adapterObj, baseAddr, DAP_ADDRINC_PACKED, DAP_DATA_SIZE_16, 2056, block_tmp));
//		misc_PrintBulk(CAST(uint8_t *, block_tmp), sizeof(block_tmp), 16);
//
//		memset(block_tmp, 0x0, sizeof(block_tmp));
//		log_info("Read size 32, result:%d.",
//				DAP_ReadMemBlock(adapterObj, baseAddr, DAP_ADDRINC_PACKED, DAP_DATA_SIZE_32, 2056, block_tmp));
//		misc_PrintBulk(CAST(uint8_t *, block_tmp), sizeof(block_tmp), 16);
		// block 写入测试
		//block_tmp
		//memset(block_tmp, 0x0, sizeof(block_tmp));
		//DAP_ReadMemBlock(adapterObj, 0x08000000u, DAP_ADDRINC_SINGLE, DAP_DATA_SIZE_32, 16, block_tmp);
		//misc_PrintBulk(CAST(uint8_t *, block_tmp), sizeof(block_tmp), 16);
	}while(0);

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
