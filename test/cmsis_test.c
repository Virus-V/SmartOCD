/*
 * cmsis_test.c
 *
 *  Created on: 2018-2-15
 *      Author: virusv
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

#include "smart_ocd.h"
#include "misc/log.h"
#include "debugger/cmsis-dap.h"
#include "arch/ARM/ADI/DAP.h"

extern int print_bulk(char *data, int length, int rowLen);

uint16_t vids[] = {0xc251, 0};
uint16_t pids[] = {0xf001, 0};


// 测试入口点
int main(){
	AdapterObject *adapterObj;
	struct cmsis_dap *cmsis_dapObj;
	USBObject *usbObj;
	uint32_t idcode = 0, tmp;
	uint8_t irLen[] = {4,4};
	log_set_level(LOG_DEBUG);
	cmsis_dapObj = NewCMSIS_DAP();
	if(cmsis_dapObj == NULL){
		log_fatal("failed to new.");
		exit(1);
	}
	// 连接CMSIS-DAP
	if(ConnectCMSIS_DAP(cmsis_dapObj, vids, pids, NULL) == FALSE){
		log_fatal("failed to connect.");
		exit(1);
	}
	adapterObj = CAST(AdapterObject *, cmsis_dapObj);
	usbObj = GET_USBOBJ(adapterObj);
	// 初始化
	adapterObj->Init(adapterObj);
//#define USE_SWD
#ifdef USE_SWD
	// 设置SWJ频率，最大30MHz
	adapterObj->Operate(adapterObj, CMDAP_SET_CLOCK, 1000u);
	// 切换到SWD模式
	adapterObj->SelectTrans(adapterObj, SWD);
	adapterObj->Operate(adapterObj, CMDAP_TRANSFER_CONFIG, 5, 10, 10);
#else
	// 设置SWJ频率，最大30MHz
	adapterObj->Operate(adapterObj, CMDAP_SET_CLOCK, 100000u);
	// 切换到JTAG模式
	adapterObj->SelectTrans(adapterObj, JTAG);
	irLen[1] = 5;
	adapterObj->Operate(adapterObj, CMDAP_JTAG_CONFIGURE, 1, irLen);
	adapterObj->Operate(adapterObj, CMDAP_JTAG_IDCODE, 0, &tmp);
	log_info("JTAG IDCODE:0x%X.", tmp);
#endif
	// 读取id code 寄存器
	adapterObj->Operate(adapterObj, CMDAP_READ_DP_REG, 0, DP_IDCODE, &idcode);
	log_info("SWD-DP with ID :0x%08x", idcode);
	// 清除上次的stick error （SW Only）
	adapterObj->Operate(adapterObj, CMDAP_WRITE_DP_REG, 0, DP_ABORT, STKCMPCLR | STKERRCLR | WDERRCLR | ORUNERRCLR | DAPABORT);
	// 确保DPBANKSEL = 0，选中CTRL/STAT寄存器
	adapterObj->Operate(adapterObj, CMDAP_WRITE_DP_REG, 0, DP_SELECT, 0);
	// 上电
	adapterObj->Operate(adapterObj, CMDAP_WRITE_DP_REG, 0, DP_CTRL_STAT, CSYSPWRUPREQ | CDBGPWRUPREQ);
	// 等待上电完成
	do {
		if(adapterObj->Operate(adapterObj, CMDAP_READ_DP_REG, 0, DP_CTRL_STAT, &tmp) == FALSE){
			log_fatal("Read CTRL_STAT Failed.");
			exit(1);
		}
	} while((tmp & (CDBGPWRUPACK | CSYSPWRUPACK)) != (CDBGPWRUPACK | CSYSPWRUPACK));
	log_info("Power up.");
	// 写入一些初始化数据
	adapterObj->Operate(adapterObj, CMDAP_WRITE_DP_REG, 0, DP_CTRL_STAT, CSYSPWRUPREQ | CDBGPWRUPREQ | TRNNORMAL | MASKLANE);
	// 读取AP的IDR
	do{
		time_t t_start, t_end;
		uint16_t apsel = 0;
		uint32_t select = 0xf0;
		uint32_t romtable = 0;
		uint32_t tmp = 0;
		AP_IDRParse parse;
		BOOL result;
		t_start = time(NULL) ;
		for(;apsel < 256; apsel ++){
			select &= ~0xff000000u;
			select |= apsel << 24;
			// 修改select选中ap
			result = adapterObj->Operate(adapterObj, CMDAP_WRITE_DP_REG, 0, DP_SELECT, select);
			if(result == FALSE) {
				// write abort;
				adapterObj->Operate(adapterObj, CMDAP_WRITE_DP_REG, 0, DP_ABORT, DAPABORT);
				break;
			}
			// 读取AP的IDR
			result = adapterObj->Operate(adapterObj, CMDAP_READ_AP_REG, 0, AP_IDR, &parse.regData);
			if(result == FALSE) {
				// write abort;
				adapterObj->Operate(adapterObj, CMDAP_WRITE_DP_REG, 0, DP_ABORT, DAPABORT);
				break;
			}
			if(parse.regData != 0){
				printf("probe Address: 0x%08X => 0x%08X\n", select, parse.regData);
				printf("Revision:0x%x\nJep106:0x%x\nClass:0x%x\nVariant:0x%x\nType:0x%x.\n",
						parse.regInfo.Revision,
						parse.regInfo.JEP106Code,
						parse.regInfo.Class,
						parse.regInfo.Variant,
						parse.regInfo.Type);
			}
		}
		t_end = time(NULL);
		printf("time: %.0f s\n", difftime(t_end,t_start));
		// 读取ROM Table
		adapterObj->Operate(adapterObj, CMDAP_WRITE_DP_REG, 0, DP_SELECT, 0xf0);
		adapterObj->Operate(adapterObj, CMDAP_READ_AP_REG, 0, AP_ROM_LSB, &romtable);
		printf("Rom table Base:%X.\n", romtable);
		// 读取romtable
		adapterObj->Operate(adapterObj, CMDAP_WRITE_DP_REG, 0, DP_SELECT, 0x0);	// 选择AP bank0
		adapterObj->Operate(adapterObj, CMDAP_WRITE_AP_REG, 0, AP_TAR_LSB, 0x08000004u);
		adapterObj->Operate(adapterObj, CMDAP_WRITE_AP_REG, 0, AP_CSW, 0x23000050 | CSW_SIZE32);
		// 读取CFG
		//adapterObj->Operate(adapterObj, CMDAP_WRITE_DP_REG, 0, DP_SELECT, 0xf0);
		adapterObj->Operate(adapterObj, CMDAP_READ_AP_REG, 0, AP_DRW, &tmp);
		printf("rom table value :0x%08X.\n", tmp);
	}while(0);
	adapterObj->Deinit(adapterObj);
	FreeCMSIS_DAP(cmsis_dapObj);
	exit(0);
}
