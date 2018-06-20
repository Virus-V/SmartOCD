/*
 * DAP.c
 *
 *  Created on: 2018年6月19日
 *      Author: virusv
 */

#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include "smart_ocd.h"
#include "misc/log.h"
#include "debugger/adapter.h"

#define TRY(x,n) if((x) == FALSE) longjmp(exception, (n))

/**
 * 初始化DAP
 * 上电，复位相关寄存等
 */
BOOL DAP_Init(AdapterObject *adapterObj){
	assert(adapterObj != NULL);
	if(adapterObj->currTrans == SWD){
		uint32_t dpidr = 0;
		adapter_DAP_Read_DP(adapterObj, DPIDR, &dpidr, FALSE);
		adapter_DAP_Write_DP(adapterObj, ABORT, 0x1e, FALSE);	// 清空ERROR
		adapter_DAP_Execute(adapterObj);
		log_info("DAP DPIDR:0x%08X.", dpidr);
	}else if(adapterObj->currTrans == JTAG){
		// 先复位TAP（软复位），再切换到IDLE状态
		adapter_Reset(adapterObj, FALSE, FALSE, 0);
		adapter_JTAG_StatusChange(adapterObj, JTAG_TAP_IDLE);	// 要保持在IDLE状态，JTAG模式下DAP_Transfer才能用
		adapter_JTAG_Execute(adapterObj);
	}else{
		log_warn("Unknow Transfer Type!");
		return FALSE;
	}
	uint32_t ctrl_stat = 0;
	// 清零SELECT寄存器
	adapter_DAP_Write_DP(adapterObj, SELECT, 0, FALSE);
	if(adapter_DAP_Execute(adapterObj) == FALSE){
		log_fatal("SELECT Register Clean Failed.");
		return -1;
	}
	// 读取SELECT寄存器
	adapter_DAP_Read_DP(adapterObj, SELECT, &adapterObj->dap.SELECT_Reg.regData, FALSE);
	adapter_DAP_Execute(adapterObj);

	// 写0x20到CTRL，并读取
	adapter_DAP_Write_DP(adapterObj, CTRL_STAT, 0x20, TRUE);
	adapter_DAP_Read_DP(adapterObj, CTRL_STAT, &ctrl_stat, TRUE);
	// 写上电请求
	adapter_DAP_Write_DP(adapterObj, CTRL_STAT, DP_CTRL_CSYSPWRUPREQ | DP_CTRL_CDBGPWRUPREQ, TRUE);
	adapter_DAP_Execute(adapterObj);
	do{
		adapter_DAP_Read_DP(adapterObj, CTRL_STAT, &ctrl_stat, TRUE);
		adapter_DAP_Execute(adapterObj);
	}while((ctrl_stat & (DP_STAT_CDBGPWRUPACK | DP_STAT_CSYSPWRUPACK)) != (DP_STAT_CDBGPWRUPACK | DP_STAT_CSYSPWRUPACK));
	log_debug("DAP Power up. CTRL_STAT:0x%08X.", ctrl_stat);
	return TRUE;
}

/**
 * 选择AP
 * apIndex:AP的索引0~255
 */
BOOL DAP_AP_Select(AdapterObject *adapterObj, uint8_t apIdx){
	assert(adapterObj != NULL);
	if(adapterObj->dap.SELECT_Reg.regInfo.AP_Sel == apIdx && adapterObj->dap.AP[apIdx].ctrl_state.init == 1){
		return TRUE;
	}
	// 当前select寄存器备份
	uint32_t selectBak = adapterObj->dap.SELECT_Reg.regData;

	jmp_buf exception;
	// 错误处理
	switch(setjmp(exception)){
	case 1:
		log_warn("Add New Command Failed!");
		adapterObj->dap.SELECT_Reg.regData = selectBak;
		return FALSE;

	case 2:
		log_warn("Execute Command Failed!");
		adapterObj->dap.SELECT_Reg.regData = selectBak;
		return FALSE;

	default:
		log_warn("Unknow Error.");
		adapterObj->dap.SELECT_Reg.regData = selectBak;
		return FALSE;

	case 0:break;
	}

	// 修改当前apSel值
	adapterObj->dap.SELECT_Reg.regInfo.AP_Sel = apIdx;
	// 写入SELECT寄存器
	TRY(adapter_DAP_Write_DP(adapterObj, SELECT, adapterObj->dap.SELECT_Reg.regData, FALSE), 1);
	TRY(adapter_DAP_Execute(adapterObj), 2);

	// 获取AP相关信息：CFG
	if(adapterObj->dap.AP[apIdx].ctrl_state.init == 0){	// 初始化ap相关参数
		uint32_t tmp, cfg, csw;
		TRY(adapter_DAP_Read_AP(adapterObj, CFG, &cfg, TRUE), 1);
		TRY(adapter_DAP_Read_AP(adapterObj, CSW, &csw, TRUE), 1);
		TRY(adapter_DAP_Execute(adapterObj), 2);

		adapterObj->dap.AP[apIdx].ctrl_state.largeAddress = !!(cfg & 0x2);
		adapterObj->dap.AP[apIdx].ctrl_state.largeData = !!(cfg & 0x4);
		adapterObj->dap.AP[apIdx].ctrl_state.bigEndian = !!(cfg & 0x1);
		// 写入到本地CSW
		adapterObj->dap.AP[apIdx].CSW.regData = csw;
		// 写入packed模式和8位模式
		adapterObj->dap.AP[apIdx].CSW.regInfo.AddrInc = ADDRINC_PACKED;
		adapterObj->dap.AP[apIdx].CSW.regInfo.Size = SIZE_8;
		TRY(adapter_DAP_Write_AP(adapterObj, CSW, adapterObj->dap.AP[apIdx].CSW.regData, TRUE), 1);
		TRY(adapter_DAP_Read_AP(adapterObj, CSW, &adapterObj->dap.AP[apIdx].CSW.regData, TRUE), 1);
		TRY(adapter_DAP_Execute(adapterObj), 2);

		/**
		 * ARM ID080813
		 * 第7-133
		 * An implementation that supports transfers smaller than word must support packed transfers.
		 * Packed transfers cannot be supported on a MEM-AP that only supports whole-word transfers.
		 * 第7-136
		 * If the MEM-AP Large Data Extention is not supported, then when a MEM-AP implementation supports different
		 * sized access, it MUST support word, halfword and byte accesses.
		 */
		if(adapterObj->dap.AP[apIdx].CSW.regInfo.AddrInc == AP_CSW_PADDRINC){
			adapterObj->dap.AP[apIdx].ctrl_state.packedTransfers = 1;
			adapterObj->dap.AP[apIdx].ctrl_state.lessWordTransfers = 1;
		}else{
			adapterObj->dap.AP[apIdx].ctrl_state.lessWordTransfers = adapterObj->dap.AP[apIdx].CSW.regInfo.Size == SIZE_8 ? 1 : 0;
		}
		// 恢复CSW寄存器的值
		TRY(adapter_DAP_Write_AP(adapterObj, CSW, csw, TRUE), 1);
		TRY(adapter_DAP_Execute(adapterObj), 2);
		// 赋值给CSW
		adapterObj->dap.AP[apIdx].CSW.regData = tmp;
		adapterObj->dap.AP[apIdx].ctrl_state.init = 1;
	}
	// 检查结果
	return TRUE;
}

/**
 * 寻找AP
 */
int DAP_Find_AP(AdapterObject *adapterObj, enum ap_type apType){
	assert(adapterObj != NULL);
	int apIdx; AP_IDR_Parse ap_IDR;

	for(apIdx = 0; apIdx < 256; apIdx++){
		// 选择AP
		if(DAP_AP_Select(adapterObj, apIdx) == FALSE){
			log_info("DAP_AP_Select fail,apIdx:%d", apIdx);
			return FALSE;
		}
		if(adapter_DAP_Read_AP(adapterObj, IDR, &ap_IDR.regData, TRUE) == FALSE){
			log_info("DAP_AP_Read fail,apIdx:%d", apIdx);
			return FALSE;
		}
//		if(adapter_DAP_Read_DP(adapterObj, CTRL_STAT, &ap_IDR.regData, TRUE) == FALSE){
//			log_info("DAP_AP_Read fail,apIdx:%d", apIdx);
//			return FALSE;
//		}
		if(adapter_DAP_Execute(adapterObj) == FALSE){
			log_warn("Failed to Execute DAP Command.");
			return FALSE;
		}
//		// 检查是否有错误
//		if(DAP_CheckError(dapObj) == FALSE){
//			log_warn("Some Error Flaged! apIdx:%d.", apIdx);
//			return FALSE;
//		}
		// 检查厂商
		if(ap_IDR.regInfo.JEP106Code == JEP106_CODE_ARM){
			if(apType == AP_TYPE_JTAG && ap_IDR.regInfo.Class == 0){	// 选择JTAG-AP
				break;
			}else if(apType != AP_TYPE_JTAG && ap_IDR.regInfo.Class == 8){	// 选择MEM-AP
				if(ap_IDR.regInfo.Type == apType){
					break;
				}
			}
		}
	}
	return TRUE;
}





