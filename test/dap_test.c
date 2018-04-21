/*
 * dap_test.c
 *
 *  Created on: 2018-4-14
 *      Author: virusv
 */


#include <stdio.h>
#include <math.h>

#include "misc/log.h"
#include "debugger/cmsis-dap.h"
#include "arch/ARM/ADI/DAP.h"

extern int print_bulk(char *data, int length, int rowLen);

uint16_t vids[] = {0xc251, 0};
uint16_t pids[] = {0xf001, 0};

/**
 * 打印AP信息
 */
static void printAPInfo(uint32_t APIDR){
	AP_IDRParse parse;
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

// 打印组件信息
static void printComponentInfo(DAPObject *dapObj, uint32_t startAddr){
	uint32_t addr, tmp, size;
	addr = (startAddr & ~0xfff) + 0xFF4;
	//log_info("addr:0x%08X.", addr);
	// 读取
	DAP_AP_Write(dapObj, AP_REG_TAR_LSB, addr);
	DAP_AP_Read(dapObj, AP_REG_DRW, &tmp);
	//log_info("CIDR4:0x%08X.", tmp);
	// 判断组件类型
	switch(tmp>>4){
	case 0x0:
		printf("Generic verification componment");
		break;
	case 0x1:
		printf("ROM Table");
		break;
	case 0x9:
		printf("Debug componment");
		break;
	case 0xb:
		printf("Peripheral Test Block(PTB)");
		break;
	case 0xd:
		printf("OptimoDE Data Engine Subsystem (DESS) componment");
		break;
	case 0xe:
		printf("Generic IP Componment");
		break;
	case 0xf:
		printf("PrimeCell peripheral");
		break;
	default:
		printf("Unknow componment");
		break;
	}
	// 计算组件占用空间大小
	addr = (startAddr & ~0xfff) + 0xFD0;
	DAP_AP_Write(dapObj, AP_REG_TAR_LSB, addr);
	DAP_AP_Read(dapObj, AP_REG_DRW, &tmp);
	//log_info("PIDR4:0x%08X.", tmp);
	tmp = (tmp >> 4);
	size = pow(2, tmp);
	printf(",occupies %d blocks.\n", size);
}

static BOOL run = TRUE;

// 错误处理函数
static void errorHandle(DAPObject *dapObj){
	log_error("Error:CTRL/STATUS:0x%08X.", dapObj->CTRL_STAT_Reg);
	uint32_t ctrl_status = dapObj->CTRL_STAT_Reg;
	run = FALSE;
	// 过载
	if(ctrl_status & DP_STAT_STICKYORUN){
		log_info("Over Run!");
		DAP_ClearStickyFlag(dapObj, DP_STAT_STICKYORUN);
	}
	// push verify 错误
	if(ctrl_status & DP_STAT_STICKYERR){
		log_info("push verify!");
		DAP_ClearStickyFlag(dapObj, DP_STAT_STICKYERR);
	}
	// push cmp 错误
	if(ctrl_status & DP_STAT_STICKYCMP){
		log_info("push cmp!");
		DAP_ClearStickyFlag(dapObj, DP_STAT_STICKYCMP);
	}
	// 写入错误
	if(ctrl_status & DP_STAT_WDATAERR){
		log_info("wdata error!");
		DAP_ClearStickyFlag(dapObj, DP_STAT_WDATAERR);
	}
}

int main(){
	DAPObject *dapObj;
	struct cmsis_dap *cmsis_dapObj;
	log_set_level(LOG_DEBUG);
	dapObj = calloc(1, sizeof(DAPObject));
	if(dapObj == NULL){
		log_fatal("failed to new dap object.");
		return 1;
	}
	cmsis_dapObj = NewCMSIS_DAP();
	if(cmsis_dapObj == NULL){
		log_fatal("failed to new cmsis dap.");
		goto EXIT_STEP_0;
	}
	// 连接CMSIS-DAP
	if(ConnectCMSIS_DAP(cmsis_dapObj, vids, pids, NULL) == FALSE){
		log_fatal("failed to connect.");
		goto EXIT_STEP_1;
	}
	// 同时对cmsis-dap进行初始化
	if(__CONSTRUCT(DAP)(dapObj, GET_ADAPTER(cmsis_dapObj)) == FALSE){
		log_fatal("Target initialization failed.");
		goto EXIT_STEP_1;
	}
	// 设置SWJ频率，最大30MHz
	adapter_SetClock(GET_ADAPTER(cmsis_dapObj), 100000u);
	// 切换到JTAG模式
	adapter_SelectTransmission(GET_ADAPTER(cmsis_dapObj), JTAG);
	// 设置DAP在JTAG扫描链的索引值
	DAP_Set_TAP_Index(dapObj, 0);
	// 设置DAP WAIT响应的重试次数
	DAP_SetRetry(dapObj, 10);
	// 设置错误回调函数
	DAP_SetErrorHandle(dapObj, errorHandle);
	// 复位TAP
	// TAP_Reset(dapObj, FALSE, 0);
	// 声明TAP
	uint16_t irLens[] = {4};
	log_debug("target_JTAG_Set_TAP_Info:%d.", TAP_SetInfo(&dapObj->tapObj, 1, irLens));
	// 写入Abort寄存器
	//DAP_WriteAbort(dapObj, 0x1);

	DAP_DP_Write(dapObj, DP_SELECT, 0);
	// 上电
	DAP_DP_Write(dapObj, DP_CTRL_STAT, DP_CTRL_CSYSPWRUPREQ | DP_CTRL_CDBGPWRUPREQ);
	// 等待上电完成
	do {
		if(DAP_DP_Read(dapObj, DP_CTRL_STAT, &dapObj->CTRL_STAT_Reg) == FALSE){
			log_fatal("Read CTRL_STAT Failed.");
			goto EXIT_STEP_2;
		}
	} while((dapObj->CTRL_STAT_Reg & (DP_STAT_CDBGPWRUPACK | DP_STAT_CSYSPWRUPACK)) != (DP_STAT_CDBGPWRUPACK | DP_STAT_CSYSPWRUPACK));
	log_info("Power up.");
	log_debug("CTRL/STAT: 0x%08X.", dapObj->CTRL_STAT_Reg);
	// 读取DPIDR寄存器
	uint32_t dpidr = 0;
	DAP_DP_Read(dapObj, DP_DPIDR, &dpidr);
	log_debug("IDR: 0x%08X.", dpidr);
	DAP_DP_Read(dapObj, DP_CTRL_STAT, &dapObj->CTRL_STAT_Reg);
	log_debug("CTRL/STAT: 0x%08X.", dapObj->CTRL_STAT_Reg);

	// 写入初始化数据
	DAP_DP_Write(dapObj,  DP_CTRL_STAT, dapObj->CTRL_STAT_Reg | DP_CTRL_TRNNORMAL | DP_CTRL_MASKLANEMSK);	// 不启用过载检测

	// 读取AP的IDR
	time_t t_start, t_end;
	uint32_t romtable = 0;
	uint32_t tmp = 0, addr;
	int32_t componentOffset;	// 组件偏移地址
	uint32_t componentAddr;

	BOOL result;
	t_start = time(NULL) ;
	for(int ap=0;ap<3;ap++){
		// 修改select选中ap
		if(DAP_AP_Select(dapObj, ap) == FALSE) {
			// write abort;
			DAP_DP_Read(dapObj, DP_CTRL_STAT, &dapObj->CTRL_STAT_Reg);
			log_warn("Write SELECT Failed, CTRL : 0x%08X.", dapObj->CTRL_STAT_Reg);
			goto EXIT_STEP_2;
		}
		// 读取AP的IDR
		result = DAP_AP_Read(dapObj, AP_REG_IDR, &tmp);
		DAP_CheckStatus(dapObj, FALSE);
		if(!run){
			goto EXIT_STEP_2;
		}
		if(tmp != 0){
			printf("probe AP: 0x%X => 0x%08X\n", ap, tmp);
			printAPInfo(tmp);
			printf("---------------------------\n");
		}
	}


	t_end = time(NULL);
	printf("time: %.0f s\n", difftime(t_end,t_start));

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
	if(DAP_AP_Select(dapObj, 1) == FALSE) {
		// write abort;
		DAP_DP_Read(dapObj, DP_CTRL_STAT, &dapObj->CTRL_STAT_Reg);
		log_warn("Write SELECT Failed, CTRL : 0x%08X.", dapObj->CTRL_STAT_Reg);
		goto EXIT_STEP_2;
	}
	// 读取ROM Table的基址
	DAP_AP_Read(dapObj, AP_REG_ROM_LSB, &romtable);
	// 读取CFG寄存器
	DAP_AP_Read(dapObj, AP_REG_CFG, &tmp);
	log_info("CFG [LD,LA,BE]:%X.", tmp);
	// 写入CSW。Privileged,Data, TAR not INC
	//DAP_AP_Write(dapObj, AP_REG_CSW, 0xB0000040u | AP_CSW_SIZE32);
	// ROM Table的首地址
	romtable = romtable & ~0xfff;
	addr = romtable;
	log_info("ROM Table Base: 0x%08X.", addr);
	uint32_t ROM_TableEntry;
	do{
		// ROM Table[11:0]不在BaseAddr里面，[1]为1代表该寄存器是ADIv5的类型，[0]代表是否存在DebugEntry
		// 注意还有Lagacy format要看一下资料
		DAP_AP_Write(dapObj, AP_REG_TAR_LSB, addr);	// 将ROM TABLE写入到TAR寄存器
		DAP_AP_Read(dapObj, AP_REG_DRW, &ROM_TableEntry);
		//log_info("ROM Table Entry:0x%08X.", ROM_TableEntry);
		// 求出偏移地址
		componentOffset = 0xfffff000 & ROM_TableEntry;
		// 计算组件的地址
		componentAddr = romtable + componentOffset;
		if(ROM_TableEntry != 0){
			//log_info("Component Base Addr: 0x%08X.", componentAddr);
			printComponentInfo(dapObj, componentAddr);
		}
		addr += 4;
	}while(ROM_TableEntry != 0);
	/*
	addr = (romtable & ~0xfff) | 0xFCC;
	adapterObj->Operate(adapterObj, CMDAP_WRITE_AP_REG, 0, AP_TAR_LSB, addr);
	adapterObj->Operate(adapterObj, CMDAP_READ_AP_REG, 0, AP_DRW, &int_val);
	log_info("Mem Type: %x.", int_val);
	for(idx=0xFD0; idx < 0xFFF; idx += 4){
		addr = (romtable & ~0xfff) | idx;
		adapterObj->Operate(adapterObj, CMDAP_WRITE_AP_REG, 0, AP_TAR_LSB, addr);
		adapterObj->Operate(adapterObj, CMDAP_READ_AP_REG, 0, AP_DRW, &int_val);
		log_info("0x%03X:0x%08X.", idx, int_val);
	}
	 */


EXIT_STEP_2:
	// 释放对象
	__DESTORY(DAP)(dapObj);
EXIT_STEP_1:
	FreeCMSIS_DAP(cmsis_dapObj);
EXIT_STEP_0:
	free(dapObj);
	return 0;
}
