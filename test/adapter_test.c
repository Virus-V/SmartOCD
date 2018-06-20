#include <stdio.h>

#include "misc/log.h"
#include "debugger/cmsis-dap.h"
#include "debugger/adapter.h"

extern int print_bulk(char *data, int length, int rowLen);

uint16_t vids[] = {0xc251, 0};
uint16_t pids[] = {0xf001, 0};

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
	adapter_DAP_Index(adapterObj, 0);
#else
	// 选择SWD传输方式
	if(adapter_SelectTransmission(adapterObj, SWD) == FALSE){
		log_fatal("failed to select SWD.");
		return -1;
	}
	// 复位
	adapter_Reset(adapterObj, FALSE, FALSE, 0);
	// 设置频率
	//adapter_SetClock(adapterObj, 5000000);	// 5MHz
	// CMSIS-DAP专用函数
	CMSIS_DAP_TransferConfigure(adapterObj,  5, 5, 5);
	CMSIS_DAP_SWD_Configure(adapterObj, 0);
	// SWD模式下第一个读取的寄存器必须要是DPIDR，这样才能读取其他的寄存器
	// 否则其他寄存器无法读取
	adapter_DAP_Read_DP(adapterObj, DPIDR, &dpidr, FALSE);
	adapter_DAP_Write_DP(adapterObj, ABORT, 0x1e, FALSE);	// 清空ERROR
#endif
	// 初始化动作
	int ctrl_stat = 0;
	adapter_DAP_Write_DP(adapterObj, SELECT, 0, FALSE);

	if(adapter_DAP_Execute(adapterObj) == FALSE){
		log_fatal("SELECT Register Clean Failed.");
		return -1;
	}

	adapter_DAP_Read_DP(adapterObj, SELECT, &adapterObj->dap.SELECT_Reg.regData, FALSE);
	adapter_DAP_Execute(adapterObj);
	log_debug("DPIDR:0x%08X, SELECT:0x%08X.", dpidr, adapterObj->dap.SELECT_Reg.regData);

	adapter_DAP_Write_DP(adapterObj, CTRL_STAT, DP_CTRL_CSYSPWRUPREQ | DP_CTRL_CDBGPWRUPREQ, TRUE);
	adapter_DAP_Execute(adapterObj);
	do{
		adapter_DAP_Read_DP(adapterObj, CTRL_STAT, &ctrl_stat, TRUE);
		adapter_DAP_Execute(adapterObj);
	}while((ctrl_stat & (DP_STAT_CDBGPWRUPACK | DP_STAT_CSYSPWRUPACK)) != (DP_STAT_CDBGPWRUPACK | DP_STAT_CSYSPWRUPACK));
	log_debug("Power up. CTRL_STAT:0x%08X.", ctrl_stat);

	// 读AP0的CSW寄存器
	uint32_t ap_csw = 0,apidr = 0;
	adapter_DAP_Read_AP(adapterObj, SELECT, &ap_csw, TRUE);
	adapter_DAP_Read_AP(adapterObj, IDR, &apidr, TRUE);
	adapter_DAP_Execute(adapterObj);
	log_debug("CSW:0x%08X, APIDR:0x%08X.", ap_csw, apidr);
	//05 00 02 02 00 1E 00 00 00	// 读DPIDR，写ABORT
	//05 00 02 08 00 00 00 00 06	// 清零SELECT，读CTRL/STAT
	//05 00 04 04 20 00 00 00 06 04 00 00 00 50 06 00 00 00 00 写CTRL/STAT 0x20，读CTRL，写CTRL/STAT：上电，读CTRL_STAT
	//05 00 01 06 // 读CTRL
	//05 00 03 06 04 00 00 00 50 06 // 读CTRLSTAT，写CTRL，读CTRL

	adapterObj->Deinit(adapterObj);	// 断开连接
	adapterObj->Destroy(adapterObj);	// 销毁结构
	return 0;
}
