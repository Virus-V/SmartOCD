#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "misc/log.h"
#include "adapter/include/adapter.h"

#include "adapter/cmsis-dap/cmsis-dap.h"

extern int misc_PrintBulk(char *data, int length, int rowLen);

uint16_t vids[] = {0xc251, 0};
uint16_t pids[] = {0xf001, 0};

#define USE_JTAG

int main(){
	uint32_t idcode[2] = {0xdeadbeef,0xdeadbeef};

	printf("Hello world\n");

	log_set_level(LOG_TRACE);
	Adapter cmdap = CreateCmsisDap();
	if(!cmdap){
		log_error("Failed!");
		return 1;
	}
	if(ConnectCmsisDap(cmdap, vids, pids, NULL) != ADPT_SUCCESS){
		log_error("Connect Failed!");
		return 1;
	}
	// 测试状态灯
	cmdap->SetStatus(cmdap, ADPT_STATUS_RUNING);
//	for(int i=0;i<60;i++){
//		cmdap->SetStatus(cmdap, ADPT_STATUS_CONNECTED);
//		sleep (1);
//		cmdap->SetStatus(cmdap, ADPT_STATUS_RUNING);
//		sleep (1);
//		cmdap->SetStatus(cmdap, ADPT_STATUS_IDLE);
//		sleep (1);
//		cmdap->SetStatus(cmdap, ADPT_STATUS_DISCONNECT);
//		sleep (1);
//	}
	// 读取引脚值
	uint8_t pinData;
	cmdap->JtagPins(cmdap, 0, 0x0, &pinData, 3000);
	log_info("JTAG Pins :0x%02X.", pinData);
	// 设置频率
	cmdap->SetFrequent(cmdap, 200000);	// 2MHz
	// 选择传输模式
	cmdap->SetTransferMode(cmdap, ADPT_MODE_SWD);
	cmdap->Reset(cmdap, ADPT_RESET_SYSTEM_RESET);	// 调试系统复位

#if 0
	// 读取idcode
	cmdap->Reset(cmdap, ADPT_RESET_DEBUG_RESET);	// 调试系统复位
	// 当前状态机切换到DRSHIFT状态
	cmdap->JtagToState(cmdap, JTAG_TAP_DRSHIFT);
	cmdap->JtagExchangeData(cmdap, CAST(uint8_t *, idcode), 64);

	cmdap->JtagToState(cmdap, JTAG_TAP_IDLE);

	cmdap->JtagCommit(cmdap);
	log_info("Current TAP State: %s\n", JtagStateToStr(cmdap->currState));
	log_debug("Origin Method read IDCODE: 0x%08X, 0x%08X.", idcode[0], idcode[1]);
	uint8_t irs[2] = {4, 5};	// 扫描链的IR链长度
	// 配置JTAG
	CmdapJtagConfig(cmdap, 2, irs);
	if(CmdapSetTapIndex(cmdap, 0) != ADPT_SUCCESS){
		log_error("CmdapSetTapIndex:failed!");
		return 1;
	}
#endif

	// 设置SWD协议
	CmdapSwdConfig(cmdap, 0);
	// 设置传输配置
	CmdapTransferConfigure(cmdap, 5, 5, 5);
	// 读取DPIDR
	uint32_t idr,tmp;
	cmdap->DapSingleRead(cmdap, ADPT_DAP_DP_REG, 0, &idr);	// 读IDR
	cmdap->DapSingleWrite(cmdap, ADPT_DAP_DP_REG, 0, 0x1e);	// 写ABORT

	cmdap->DapSingleWrite(cmdap, ADPT_DAP_DP_REG, 0x08, 0);	// 写SELECT
	cmdap->DapSingleWrite(cmdap, ADPT_DAP_DP_REG, 0x04, 0x50000000);	// 写SELECT
	cmdap->DapCommit(cmdap);	// 提交指令
	log_info("DPIDR: 0x%08X.", idr);

	do{
		cmdap->DapSingleRead(cmdap, ADPT_DAP_DP_REG, 0x04, &tmp);	// 读IDR
		cmdap->DapCommit(cmdap);	// 提交指令
		log_trace("CTRL_STAT: 0x%08X.", tmp);
	}while((tmp & 0xA0000000) != 0xA0000000);

/**
union {
	uint32_t data;
	struct {
		uint32_t DP_BankSel : 4;
		uint32_t AP_BankSel : 4;
		uint32_t : 16;
		uint32_t AP_Sel : 8;
	} info;
}SelectReg;	// 当前Select寄存器
 */
	// 读AP CSW IDR
	cmdap->DapSingleWrite(cmdap, ADPT_DAP_DP_REG, 0x08, 0x0F000000);	// 写SELECT
	cmdap->DapSingleRead(cmdap, ADPT_DAP_AP_REG, 0x0C, &idr);	// 读AP0的IDR
	cmdap->DapSingleWrite(cmdap, ADPT_DAP_DP_REG, 0x08, 0x00000000);	// 写SELECT
	cmdap->DapSingleRead(cmdap, ADPT_DAP_AP_REG, 0x00, &tmp);	// 读AP0的CSW
	cmdap->DapCommit(cmdap);	// 提交指令
	log_info("AP[0] IDR: 0x%08X, CSW: 0x%08X.", idr, tmp);
	// 读取ROM
	cmdap->DapSingleWrite(cmdap, ADPT_DAP_DP_REG, 0x08, 0x00000000);	// 写SELECT
	cmdap->DapSingleWrite(cmdap, ADPT_DAP_AP_REG, 0x04, 0x08000000);	// TAR_LSB
	cmdap->DapSingleRead(cmdap, ADPT_DAP_AP_REG, 0x0C, &tmp);	// 读AP0的DRW
	cmdap->DapCommit(cmdap);	// 提交指令
	log_info("0x08000000: 0x%08X.", tmp);

	// 多次读取测试
	uint32_t *buff = calloc(32, 4);
	if(!buff){
		log_error("alloc!");
		return 1;
	}
	cmdap->DapMultiRead(cmdap, ADPT_DAP_DP_REG, 0, 32, buff);
	cmdap->DapMultiWrite(cmdap, ADPT_DAP_DP_REG, 0, 32, buff);

	cmdap->DapCommit(cmdap);	// 提交指令
	misc_PrintBulk((char *)buff, 32<<2, 16);
	cmdap->SetStatus(cmdap, ADPT_STATUS_IDLE);
	DisconnectCmsisDap(cmdap);
	DestroyCmsisDap(&cmdap);
	return 0;
}
