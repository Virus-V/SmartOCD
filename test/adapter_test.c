#include <stdio.h>
#include <unistd.h>

#include "misc/log.h"
#include "adapter/include/adapter.h"

#include "adapter/cmsis-dap/cmsis-dap.h"

extern int print_bulk(char *data, int length, int rowLen);

uint16_t vids[] = {0xc251, 0};
uint16_t pids[] = {0xf001, 0};

#define USE_JTAG

int main(){
	uint32_t idcode[2] = {0xdeadbeef,0xdeadbeef};

	printf("Hello world\n");

	log_set_level(LOG_DEBUG);
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
	// 设置频率
	cmdap->SetFrequent(cmdap, 1000);	// 2MHz
	// 选择传输模式
	cmdap->SetTransferMode(cmdap, ADPT_MODE_JTAG);
	// 读取引脚值
	uint8_t pinData;
	cmdap->JtagPins(cmdap, 0, 0x0, &pinData, 3000);
	log_info("JTAG Pins :0x%02X.", pinData);
	//getchar();
	//printf("%s:%d\n", __FILE__, __LINE__);
	// 读取idcode
	cmdap->Reset(cmdap, ADPT_RESET_DEBUG_RESET);	// 调试系统复位
	//printf("%s:%d\n", __FILE__, __LINE__);
	//sleep (3);
	// 当前状态机切换到DRSHIFT状态
	cmdap->JtagToState(cmdap, JTAG_TAP_DRSHIFT);
	//printf("%s:%d\n", __FILE__, __LINE__);
	cmdap->JtagExchangeData(cmdap, CAST(uint8_t *, idcode), 64);

	cmdap->JtagToState(cmdap, JTAG_TAP_IDLE);

	cmdap->JtagCommit(cmdap);
	//printf("%s\n", JtagStateToStr(cmdap->currState));
	//printf("%s:%d\n", __FILE__, __LINE__);
	log_debug("Origin Method read IDCODE: 0x%08X, 0x%08X.", idcode[0], idcode[1]);

	cmdap->SetStatus(cmdap, ADPT_STATUS_IDLE);
	DisconnectCmsisDap(cmdap);
	DestroyCmsisDap(&cmdap);
//	// 设置频率
//	adapter_SetClock(adapterObj, 5000000);	// 5MHz
//#ifdef USE_JTAG
//	// 选择JTAG传输方式
//	if(adapter_SelectTransmission(adapterObj, JTAG) == FALSE){
//		log_fatal("failed to select JTAG.");
//		return -1;
//	}
//	// 复位
//	adapter_Reset(adapterObj, FALSE, FALSE, 0);
//	uint32_t idcode[2] = {0,0};
//	adapter_JTAG_StatusChange(adapterObj, JTAG_TAP_DRSHIFT);
//	adapter_JTAG_Exchange_IO(adapterObj, CAST(uint8_t *, idcode), 64);
//	adapter_JTAG_StatusChange(adapterObj, JTAG_TAP_IDLE);	// 要保持在IDLE状态，JTAG模式下DAP_Transfer才能用
//	adapter_JTAG_Execute(adapterObj);
//	log_debug("Origin Method read IDCODE: 0x%08X, 0x%08X.", idcode[0], idcode[1]);
//
//	uint16_t irss[2] = {4, 5};
//	adapter_JTAG_Set_TAP_Info(adapterObj, 2, irss);
//
//	uint8_t irs[2] = {4, 5};
//	// 设置JTAG信息
//	CMSIS_DAP_JTAG_Configure(adapterObj, 2, irs);
//	CMSIS_DAP_TransferConfigure(adapterObj, 5, 5, 5);
//	adapter_DAP_Index(adapterObj, 0);
//#else
//	// 选择SWD传输方式
//	if(adapter_SelectTransmission(adapterObj, SWD) == FALSE){
//		log_fatal("failed to select SWD.");
//		return -1;
//	}
//	// 复位
//	adapter_Reset(adapterObj, FALSE, FALSE, 0);
//	// 设置频率
//	//adapter_SetClock(adapterObj, 5000000);	// 5MHz
//	// CMSIS-DAP专用函数
//	CMSIS_DAP_TransferConfigure(adapterObj,  5, 5, 5);
//	CMSIS_DAP_SWD_Configure(adapterObj, 0);
//	// SWD模式下第一个读取的寄存器必须要是DPIDR，这样才能读取其他的寄存器
//	// 否则其他寄存器无法读取
//	adapter_DAP_Read_DP_Single(adapterObj, DPIDR, &dpidr, FALSE);
//	adapter_DAP_Write_DP_Single(adapterObj, ABORT, 0x1e, FALSE);	// 清空ERROR
//#endif
//	// 初始化动作
//	int ctrl_stat = 0;
//	adapter_DAP_Write_DP_Single(adapterObj, SELECT, 0, FALSE);
//
//	if(adapter_DAP_Execute(adapterObj) == FALSE){
//		log_fatal("SELECT Register Clean Failed.");
//		return -1;
//	}
//
//	adapter_DAP_Read_DP_Single(adapterObj, SELECT, &adapterObj->dap.SELECT_Reg.regData, FALSE);
//	adapter_DAP_Execute(adapterObj);
//	log_debug("DPIDR:0x%08X, SELECT:0x%08X.", dpidr, adapterObj->dap.SELECT_Reg.regData);
//
//	adapter_DAP_Write_DP_Single(adapterObj, CTRL_STAT, DP_CTRL_CSYSPWRUPREQ | DP_CTRL_CDBGPWRUPREQ, TRUE);
//	adapter_DAP_Execute(adapterObj);
//	do{
//		adapter_DAP_Read_DP_Single(adapterObj, CTRL_STAT, &ctrl_stat, TRUE);
//		adapter_DAP_Execute(adapterObj);
//	}while((ctrl_stat & (DP_STAT_CDBGPWRUPACK | DP_STAT_CSYSPWRUPACK)) != (DP_STAT_CDBGPWRUPACK | DP_STAT_CSYSPWRUPACK));
//	log_debug("Power up. CTRL_STAT:0x%08X.", ctrl_stat);
//
//	// 读AP0的CSW寄存器
//	uint32_t ap_csw = 0,apidr = 0, rom_data = 0;
//	adapter_DAP_Read_AP_Single(adapterObj, CSW, &ap_csw, TRUE);
//	adapter_DAP_Read_AP_Single(adapterObj, IDR, &apidr, TRUE);
//	//adapter_DAP_Execute(adapterObj);
//	log_debug("CSW:0x%08X, APIDR:0x%08X.", ap_csw, apidr);
//	// 写入AP的TAR_LSB，读取一个字
//	adapter_DAP_Write_AP_Single(adapterObj, TAR_LSB, 0x08000004u, TRUE);
//	adapter_DAP_Read_AP_Single(adapterObj, DRW, &rom_data, TRUE);
//	adapter_DAP_Execute(adapterObj);
//	log_debug("CSW:0x%08X, APIDR:0x%08X.", ap_csw, apidr);
//	log_debug("0x08000000: 0x%08X.");
//	//05 00 02 02 00 1E 00 00 00	// 读DPIDR，写ABORT
//	//05 00 02 08 00 00 00 00 06	// 清零SELECT，读CTRL/STAT
//	//05 00 04 04 20 00 00 00 06 04 00 00 00 50 06 00 00 00 00 写CTRL/STAT 0x20，读CTRL，写CTRL/STAT：上电，读CTRL_STAT
//	//05 00 01 06 // 读CTRL
//	//05 00 03 06 04 00 00 00 50 06 // 读CTRLSTAT，写CTRL，读CTRL
//
//	adapterObj->Deinit(adapterObj);	// 断开连接
//	adapterObj->Destroy(adapterObj);	// 销毁结构
	return 0;
}
