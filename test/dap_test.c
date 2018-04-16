/*
 * dap_test.c
 *
 *  Created on: 2018-4-14
 *      Author: virusv
 */


#include <stdio.h>
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
	adapter_SetClock(GET_ADAPTER(cmsis_dapObj), 1000000u);
	// 切换到JTAG模式
	adapter_SelectTransmission(GET_ADAPTER(cmsis_dapObj), JTAG);
	// 设置DAP在JTAG扫描链的索引值
	DAP_Set_TAP_Index(dapObj, 0);
	// 设置DAP WAIT响应的重试次数
	DAP_SetRetry(dapObj, 10);
	// 复位TAP
	// TAP_Reset(dapObj, FALSE, 0);
	// 声明TAP
	uint16_t irLens[] = {4, 5};
	log_debug("target_JTAG_Set_TAP_Info:%d.", TAP_SetInfo(&dapObj->tapObj, 2, irLens));
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
	//直接退出
	goto EXIT_STEP_2;

	log_debug("CTRL/STAT: 0x%08X.", dapObj->CTRL_STAT_Reg);
	// 写入初始化数据
	DAP_DP_Write(dapObj,  DP_CTRL_STAT, dapObj->CTRL_STAT_Reg | DP_CTRL_TRNNORMAL | DP_CTRL_MASKLANEMSK | DP_CTRL_ORUNDETECT);

	// 读取AP的IDR
	do{
		time_t t_start, t_end;
		uint16_t apsel = 0;
		uint32_t select = 0xf0;
		uint32_t romtable = 0;
		uint32_t tmp = 0, addr;

		BOOL result;
		t_start = time(NULL) ;
		for(;apsel < 255; apsel ++){

			// 修改select选中ap
			if(DAP_AP_Select(dapObj, apsel) == FALSE) {
				// write abort;
				DAP_DP_Read(dapObj, DP_CTRL_STAT, &dapObj->CTRL_STAT_Reg);
				log_warn("Write SELECT Failed, CTRL : 0x%08X.", dapObj->CTRL_STAT_Reg);
				break;
			}
			// 读取AP的IDR
			result = DAP_AP_Read(dapObj, AP_REG_IDR, &tmp);
			DAP_DP_Read(dapObj, DP_CTRL_STAT, &dapObj->CTRL_STAT_Reg);
			log_debug("CTRL : 0x%08X, Select: 0x%08X.", dapObj->CTRL_STAT_Reg, dapObj->SelectReg.data);

			if(tmp != 0){
				printf("probe Address: 0x%08X => 0x%08X\n", dapObj->SelectReg.data, tmp);
				printAPInfo(tmp);
				// 读取ROM Table
				//adapterObj->Operate(adapterObj, CMDAP_WRITE_DP_REG, 0, DP_SELECT, 0xf0);
				//adapterObj->Operate(adapterObj, CMDAP_READ_AP_REG, 0, AP_ROM_LSB, &romtable);
				//adapterObj->Operate(adapterObj, CMDAP_WRITE_DP_REG, 0, DP_SELECT, 0x0);	// 选择AP bank0
				//adapterObj->Operate(adapterObj, CMDAP_WRITE_AP_REG, 0, AP_CSW, 0x23000050 | CSW_SIZE32);
				// ROM Table的首地址
				/*
				addr = romtable & ~0xfff;
				log_info("ROM Table Base: 0x%08X.", addr);
				do{
					// ROM Table[11:0]不在BaseAddr里面，[1]为1代表该寄存器是ADIv5的类型，[0]代表是否存在DebugEntry
					// 注意还有Lagacy format要看一下资料
					adapterObj->Operate(adapterObj, CMDAP_WRITE_AP_REG, 0, AP_TAR_LSB, addr);
					adapterObj->Operate(adapterObj, CMDAP_READ_AP_REG, 0, AP_DRW, &int_val);
					if(int_val == 0) break;
					int_val &= 0xfffff000u;	// Address Offset
					int_val = (int32_t)addr + int_val;
					log_info("Component Base Addr: 0x%08X.", int_val);
					printComponentInfo(adapterObj, int_val);
					addr += 4;
				}while(1);
				addr = (romtable & ~0xfff) | 0xFCC;
				adapterObj->Operate(adapterObj, CMDAP_WRITE_AP_REG, 0, AP_TAR_LSB, addr);
				adapterObj->Operate(adapterObj, CMDAP_READ_AP_REG, 0, AP_DRW, &int_val);
				log_info("Mem Type: %x.", int_val);
				for(idx=0xFD0; idx < 0xFFF; idx += 4){
					addr = (romtable & ~0xfff) | idx;
					adapterObj->Operate(adapterObj, CMDAP_WRITE_AP_REG, 0, AP_TAR_LSB, addr);
					adapterObj->Operate(adapterObj, CMDAP_READ_AP_REG, 0, AP_DRW, &int_val);
					log_info("0x%03X:0x%08X.", idx, int_val);
				}*/
			}

		}
		t_end = time(NULL);
		printf("time: %.0f s\n", difftime(t_end,t_start));
	}while(0);
	//DAP_DP_Read(dapObj, 0, DP_SELECT);
	//DAP_DP_Read(dapObj, 0, DP_CTRL_STAT);

EXIT_STEP_2:
	// 释放对象
	__DESTORY(DAP)(dapObj);
EXIT_STEP_1:
	FreeCMSIS_DAP(cmsis_dapObj);
EXIT_STEP_0:
	free(dapObj);
	return 0;
}
