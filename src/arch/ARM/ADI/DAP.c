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
		// XXX SWD模式下第一个读取的寄存器必须要是DPIDR，这样才能读取其他的寄存器
		// 否则其他寄存器无法读取
		adapter_DAP_Read_DP_Single(adapterObj, DPIDR, &dpidr, FALSE);
		adapter_DAP_Write_DP_Single(adapterObj, ABORT, 0x1e, FALSE);	// 清空ERROR
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
	adapter_DAP_Write_DP_Single(adapterObj, SELECT, 0, FALSE);
	if(adapter_DAP_Execute(adapterObj) == FALSE){
		log_fatal("SELECT Register Clean Failed.");
		return -1;
	}
	// 读取SELECT寄存器
	adapter_DAP_Read_DP_Single(adapterObj, SELECT, &adapterObj->dap.SELECT_Reg.regData, FALSE);
	adapter_DAP_Execute(adapterObj);

	// 写0x20到CTRL，并读取
	adapter_DAP_Write_DP_Single(adapterObj, CTRL_STAT, 0x20, TRUE);
	adapter_DAP_Read_DP_Single(adapterObj, CTRL_STAT, &ctrl_stat, TRUE);
	// 写上电请求
	adapter_DAP_Write_DP_Single(adapterObj, CTRL_STAT, DP_CTRL_CSYSPWRUPREQ | DP_CTRL_CDBGPWRUPREQ, TRUE);
	adapter_DAP_Execute(adapterObj);
	do{
		adapter_DAP_Read_DP_Single(adapterObj, CTRL_STAT, &ctrl_stat, TRUE);
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
	jmp_buf exception;
	// 错误处理
	switch(setjmp(exception)){
	case 1:
		log_warn("Add New Command Failed!");
		return FALSE;

	case 2:
		log_warn("Execute Command Failed!");
		adapter_DAP_CleanCommandQueue(adapterObj);
		return FALSE;

	default:
		log_warn("Unknow Error.");
		return FALSE;

	case 0:break;
	}
	DP_SELECT_Parse selectTmp;
	// 当前select寄存器
	selectTmp.regData = adapterObj->dap.SELECT_Reg.regData;
	// 修改当前apSel值
	selectTmp.regInfo.AP_Sel = apIdx;
	// 写入SELECT寄存器
	TRY(adapter_DAP_Write_DP_Single(adapterObj, SELECT, adapterObj->dap.SELECT_Reg.regData, FALSE), 1);
	TRY(adapter_DAP_Execute(adapterObj), 2);
	// select更新成功，同步select寄存器数据
	adapterObj->dap.SELECT_Reg.regData = selectTmp.regData;
	// 获取AP相关信息：CFG
	if(adapterObj->dap.AP[apIdx].ctrl_state.init == 0){	// 初始化ap相关参数
		/**
		 * FIX: Valgrind 会在类似
		 * if(adapterObj->dap.AP[DAP_CURR_AP(adapterObj)].CSW.regData != cswTmp.regData){...}
		 * 的判断中报“根据未初始化的值跳转”错误
		 * 所以在这儿为tmp,cfg,csw赋初值
		 */
		uint32_t tmp=0, cfg=0, csw=0;
		TRY(adapter_DAP_Read_AP_Single(adapterObj, CFG, &cfg, TRUE), 1);
		TRY(adapter_DAP_Read_AP_Single(adapterObj, CSW, &csw, TRUE), 1);
		TRY(adapter_DAP_Execute(adapterObj), 2);

		adapterObj->dap.AP[apIdx].ctrl_state.largeAddress = !!(cfg & 0x2);
		adapterObj->dap.AP[apIdx].ctrl_state.largeData = !!(cfg & 0x4);
		adapterObj->dap.AP[apIdx].ctrl_state.bigEndian = !!(cfg & 0x1);
		// 写入到本地CSW
		adapterObj->dap.AP[apIdx].CSW.regData = csw;
		// 写入packed模式和8位模式
		adapterObj->dap.AP[apIdx].CSW.regInfo.AddrInc = DAP_ADDRINC_PACKED;
		adapterObj->dap.AP[apIdx].CSW.regInfo.Size = AP_CSW_SIZE8;
		TRY(adapter_DAP_Write_AP_Single(adapterObj, CSW, adapterObj->dap.AP[apIdx].CSW.regData, TRUE), 1);
		TRY(adapter_DAP_Read_AP_Single(adapterObj, CSW, &adapterObj->dap.AP[apIdx].CSW.regData, TRUE), 1);
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
			adapterObj->dap.AP[apIdx].ctrl_state.lessWordTransfers = adapterObj->dap.AP[apIdx].CSW.regInfo.Size == AP_CSW_SIZE8 ? 1 : 0;
		}
		// 恢复CSW寄存器的值
		TRY(adapter_DAP_Write_AP_Single(adapterObj, CSW, csw, TRUE), 1);
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
	uint32_t ctrl_stat;

	for(apIdx = 0; apIdx < 256; apIdx++){
		// 选择AP
		if(DAP_AP_Select(adapterObj, apIdx) == FALSE){
			log_info("DAP_AP_Select fail,apIdx:%d", apIdx);
			return -1;
		}
		if(adapter_DAP_Read_AP_Single(adapterObj, IDR, &ap_IDR.regData, TRUE) == FALSE){
			log_info("Add read AP_IDR command fail,apIdx:%d", apIdx);
			return -1;
		}
		if(adapter_DAP_Read_DP_Single(adapterObj, CTRL_STAT, &ctrl_stat, TRUE) == FALSE){
			log_info("Add read DP_CTRL/STAT command fail,apIdx:%d", apIdx);
			return -1;

		}
		if(adapter_DAP_Execute(adapterObj) == FALSE){
			log_warn("Failed to Execute DAP Command.");
			adapter_DAP_CleanCommandQueue(adapterObj);	// 清空指令队列中未执行的指令
			return -1;
		}
		log_info("Probe AP %d, AP_IDR: 0x%08X, CTRL/STAT: 0x%08X.", apIdx, ap_IDR.regData, ctrl_stat);
		// 检查厂商
		if(ap_IDR.regInfo.JEP106Code == JEP106_CODE_ARM){
			if(apType == AP_TYPE_JTAG && ap_IDR.regInfo.Class == 0){	// 选择JTAG-AP
				return apIdx;
			}else if(apType != AP_TYPE_JTAG && ap_IDR.regInfo.Class == 8){	// 选择MEM-AP
				if(ap_IDR.regInfo.Type == apType){
					return apIdx;
				}
			}
		}
	}
	return -1;
}

/**
 * 读TAR寄存器
 * 64位数： 0x77 66 55 44  33 22 11 00
 * 在内存中
 * 00 11 22 33 44 55 66 77
 * low <------------> high
 * 低32位：0x33 22 11 00
 * 高32位：0x77 66 55 44
 */
BOOL DAP_Read_TAR(AdapterObject *adapterObj, uint64_t *address_out){
	assert(adapterObj != NULL);
	*address_out = 0;
	// 如果支持Large Address
	if(adapterObj->dap.AP[DAP_CURR_AP(adapterObj)].ctrl_state.init == 1 &&
			adapterObj->dap.AP[DAP_CURR_AP(adapterObj)].ctrl_state.largeAddress == 1){
		if(adapter_DAP_Read_AP_Single(adapterObj, TAR_MSB, CAST(uint32_t *, address_out) + 1, TRUE) == FALSE) return FALSE;
	}
	if(adapter_DAP_Read_AP_Single(adapterObj, TAR_LSB, CAST(uint32_t *, address_out), TRUE) == FALSE) return FALSE;
	return TRUE;
}

/**
 * 写TAR寄存器
 */
BOOL DAP_Write_TAR(AdapterObject *adapterObj, uint64_t address_in){
	assert(adapterObj != NULL);
	// 如果支持Large Address
	if(adapterObj->dap.AP[DAP_CURR_AP(adapterObj)].ctrl_state.init == 1 &&
			adapterObj->dap.AP[DAP_CURR_AP(adapterObj)].ctrl_state.largeAddress == 1){
		// 将地址的高32位写入TAR_MSB
		if(adapter_DAP_Write_AP_Single(adapterObj, TAR_MSB, CAST(uint32_t, address_in >> 32), TRUE) == FALSE) return FALSE;
	}
	// 将低32位写入TAR_LSB
	if(adapter_DAP_Write_AP_Single(adapterObj, TAR_LSB, CAST(uint32_t, address_in & 0xffffffffu), TRUE) == FALSE) return FALSE;
	return TRUE;
}

/**
 * 读取8位数据
 * addr：64位地址
 * data_out：读取的数据输出地址
 */
BOOL DAP_ReadMem8(AdapterObject *adapterObj, uint64_t addr, uint8_t *data_out){
	assert(adapterObj != NULL && adapterObj->dap.AP[DAP_CURR_AP(adapterObj)].ctrl_state.init == 1);
	MEM_AP_CSW_Parse cswTmp;
	cswTmp.regData = adapterObj->dap.AP[DAP_CURR_AP(adapterObj)].CSW.regData;
	// 设置CSW：Size=Word，AddrInc=off
	cswTmp.regInfo.AddrInc = DAP_ADDRINC_OFF;	// AddrInc Off
	if(adapterObj->dap.AP[DAP_CURR_AP(adapterObj)].ctrl_state.lessWordTransfers == 1){	// 支持lessWordTransfer
		cswTmp.regInfo.Size = AP_CSW_SIZE8;	// Byte
	}else{
		log_warn("Don't Support Less Word Transfers.");
		return FALSE;
	}
	// 是否需要更新CSW寄存器？
	if(adapterObj->dap.AP[DAP_CURR_AP(adapterObj)].CSW.regData != cswTmp.regData){
		if(adapter_DAP_Write_AP_Single(adapterObj, CSW, cswTmp.regData, TRUE) == FALSE){
			return FALSE;
		}
	}
	// 写入TAR
	if(DAP_Write_TAR(adapterObj, addr) == FALSE) return FALSE;
	uint32_t data_tmp = 0;
	// 读取数据，根据byte lane获得数据
	if(adapter_DAP_Read_AP_Single(adapterObj, DRW, &data_tmp, TRUE) == FALSE) return FALSE;
	// 执行指令队列
	if(adapter_DAP_Execute(adapterObj) == FALSE){
		return FALSE;
	}
	// 指令执行成功，同步CSW
	adapterObj->dap.AP[DAP_CURR_AP(adapterObj)].CSW.regData = cswTmp.regData;
	// 提取数据
	*data_out = (data_tmp >> ((addr & 3) << 3)) & 0xff;
	return TRUE;
}

/**
 * 读取16位数据
 * addr：64位地址
 * data_out：读取的数据输出地址
 */
BOOL DAP_ReadMem16(AdapterObject *adapterObj, uint64_t addr, uint16_t *data_out){
	assert(adapterObj != NULL && adapterObj->dap.AP[DAP_CURR_AP(adapterObj)].ctrl_state.init == 1);
	// 检查对齐
	if(addr & 0x1){
		log_warn("Memory address is not half word aligned!");
		return FALSE;
	}
	MEM_AP_CSW_Parse cswTmp;
	cswTmp.regData = adapterObj->dap.AP[DAP_CURR_AP(adapterObj)].CSW.regData;
	// 设置CSW：Size=Word，AddrInc=off
	cswTmp.regInfo.AddrInc = DAP_ADDRINC_OFF;	// AddrInc Off
	if(adapterObj->dap.AP[DAP_CURR_AP(adapterObj)].ctrl_state.lessWordTransfers == 1){	// 支持lessWordTransfer
		cswTmp.regInfo.Size = AP_CSW_SIZE16;	// Half Word
	}else{
		log_warn("Don't Support Less Word Transfers.");
		return FALSE;
	}
	// 是否需要更新CSW寄存器？
	if(adapterObj->dap.AP[DAP_CURR_AP(adapterObj)].CSW.regData != cswTmp.regData){
		if(adapter_DAP_Write_AP_Single(adapterObj, CSW, cswTmp.regData, TRUE) == FALSE){
			return FALSE;
		}
	}
	// 写入TAR
	if(DAP_Write_TAR(adapterObj, addr) == FALSE) return FALSE;
	uint32_t data_tmp = 0;
	// 读取数据，根据byte lane获得数据
	if(adapter_DAP_Read_AP_Single(adapterObj, DRW, &data_tmp, TRUE) == FALSE) return FALSE;
	// 执行指令队列
	if(adapter_DAP_Execute(adapterObj) == FALSE){
		return FALSE;
	}
	// 指令执行成功，同步CSW
	adapterObj->dap.AP[DAP_CURR_AP(adapterObj)].CSW.regData = cswTmp.regData;
	// 提取数据
	*data_out = (data_tmp >> ((addr & 3) << 3)) & 0xffff;
	return TRUE;
}

/**
 * 读取32位数据
 * addr：64位地址
 * data_out：读取的数据输出地址
 * 注意：地址要以字对齐，否则报错
 */
BOOL DAP_ReadMem32(AdapterObject *adapterObj, uint64_t addr, uint32_t *data_out){
	assert(adapterObj != NULL && adapterObj->dap.AP[DAP_CURR_AP(adapterObj)].ctrl_state.init == 1);
	// 检查对齐
	if(addr & 0x3){
		log_warn("Memory address is not word aligned!");
		return FALSE;
	}
	MEM_AP_CSW_Parse cswTmp;
	cswTmp.regData = adapterObj->dap.AP[DAP_CURR_AP(adapterObj)].CSW.regData;
	// 设置CSW：Size=Word，AddrInc=off
	cswTmp.regInfo.AddrInc = DAP_ADDRINC_OFF;	// AddrInc Off
	cswTmp.regInfo.Size = AP_CSW_SIZE32;	// Word
	// 是否需要更新CSW寄存器？
	if(adapterObj->dap.AP[DAP_CURR_AP(adapterObj)].CSW.regData != cswTmp.regData){
		if(adapter_DAP_Write_AP_Single(adapterObj, CSW, cswTmp.regData, TRUE) == FALSE){
			return FALSE;
		}
	}
	// 写入TAR
	if(DAP_Write_TAR(adapterObj, addr) == FALSE) return FALSE;
	// 读取数据，根据byte lane获得数据
	if(adapter_DAP_Read_AP_Single(adapterObj, DRW, data_out, TRUE) == FALSE) return FALSE;
	// 执行指令队列
	if(adapter_DAP_Execute(adapterObj) == FALSE){
		return FALSE;
	}
	// 指令执行成功，同步CSW
	adapterObj->dap.AP[DAP_CURR_AP(adapterObj)].CSW.regData = cswTmp.regData;
	return TRUE;
}

/**
 * 读取64位数据
 * addr：64位地址
 * data_out：读取的数据输出地址
 * 注意：地址要以双字对齐，否则报错
 * TODO 没有测试
 */
BOOL DAP_ReadMem64(AdapterObject *adapterObj, uint64_t addr, uint64_t *data_out){
	assert(adapterObj != NULL && adapterObj->dap.AP[DAP_CURR_AP(adapterObj)].ctrl_state.init == 1);
	// 检查对齐
	if(addr & 0x7){
		log_warn("Memory address is not double word aligned!");
		return FALSE;
	}
	MEM_AP_CSW_Parse cswTmp;
	cswTmp.regData = adapterObj->dap.AP[DAP_CURR_AP(adapterObj)].CSW.regData;
	// 设置CSW：Size=Word，AddrInc=off
	cswTmp.regInfo.AddrInc = DAP_ADDRINC_OFF;	// AddrInc Off
	// 判断是否支持Big Data
	if(adapterObj->dap.AP[DAP_CURR_AP(adapterObj)].ctrl_state.largeData == 1){
		cswTmp.regInfo.Size = AP_CSW_SIZE64;	// Double Word
	}else{
		log_warn("Don't Support Big Data Transfers.");
		return FALSE;
	}
	// 是否需要更新CSW寄存器？
	if(adapterObj->dap.AP[DAP_CURR_AP(adapterObj)].CSW.regData != cswTmp.regData){
		if(adapter_DAP_Write_AP_Single(adapterObj, CSW, cswTmp.regData, TRUE) == FALSE){
			return FALSE;
		}
	}
	// 写入TAR
	if(DAP_Write_TAR(adapterObj, addr) == FALSE) return FALSE;
	uint32_t data_tmp[2];	// 定义64位数据
	// 读取数据，第一次读取的是低位，接下来读取高位，必须两次读取后才能完成本次AP Memory access
	if(adapter_DAP_Read_AP_Single(adapterObj, DRW, data_tmp, TRUE) == FALSE) return FALSE;
	if(adapter_DAP_Read_AP_Single(adapterObj, DRW, data_tmp + 1, TRUE) == FALSE) return FALSE;
	// 执行指令队列
	if(adapter_DAP_Execute(adapterObj) == FALSE){
		return FALSE;
	}
	// 指令执行成功，同步CSW
	adapterObj->dap.AP[DAP_CURR_AP(adapterObj)].CSW.regData = cswTmp.regData;
	*data_out = ((uint64_t)data_tmp[1] << 32) | data_tmp[0];
	return TRUE;
}

/**
 * 写8位数据
 * addr：64位地址
 * data_in：写入的数据
 */
BOOL DAP_WriteMem8(AdapterObject *adapterObj, uint64_t addr, uint8_t data_in){
	assert(adapterObj != NULL && adapterObj->dap.AP[DAP_CURR_AP(adapterObj)].ctrl_state.init == 1);
	MEM_AP_CSW_Parse cswTmp;
	cswTmp.regData = adapterObj->dap.AP[DAP_CURR_AP(adapterObj)].CSW.regData;
	// 设置CSW：Size=Word，AddrInc=off
	cswTmp.regInfo.AddrInc = DAP_ADDRINC_OFF;	// AddrInc Off
	if(adapterObj->dap.AP[DAP_CURR_AP(adapterObj)].ctrl_state.lessWordTransfers == 1){	// 支持lessWordTransfer
		cswTmp.regInfo.Size = AP_CSW_SIZE8;	// Byte
	}else{
		log_warn("Don't Support Less Word Transfers.");
		return FALSE;
	}
	// 是否需要更新CSW寄存器？
	if(adapterObj->dap.AP[DAP_CURR_AP(adapterObj)].CSW.regData != cswTmp.regData){
		if(adapter_DAP_Write_AP_Single(adapterObj, CSW, cswTmp.regData, TRUE) == FALSE){
			return FALSE;
		}
	}
	// 写入TAR
	if(DAP_Write_TAR(adapterObj, addr) == FALSE) return FALSE;
	// 放到Byte Lane确定的位置
	uint32_t data_tmp = data_in << ((addr & 3) << 3);
	// 读取数据，根据byte lane获得数据
	if(adapter_DAP_Write_AP_Single(adapterObj, DRW, data_tmp, TRUE) == FALSE) return FALSE;
	// 执行指令队列
	if(adapter_DAP_Execute(adapterObj) == FALSE){
		return FALSE;
	}
	// 指令执行成功，同步CSW
	adapterObj->dap.AP[DAP_CURR_AP(adapterObj)].CSW.regData = cswTmp.regData;
	return TRUE;
}

/**
 * 写16位数据
 * addr：64位地址
 * data_in：写入的数据
 */
BOOL DAP_WriteMem16(AdapterObject *adapterObj, uint64_t addr, uint16_t data_in){
	assert(adapterObj != NULL && adapterObj->dap.AP[DAP_CURR_AP(adapterObj)].ctrl_state.init == 1);
	// 检查对齐
	if(addr & 0x1){
		log_warn("Memory address is not half word aligned!");
		return FALSE;
	}
	MEM_AP_CSW_Parse cswTmp;
	cswTmp.regData = adapterObj->dap.AP[DAP_CURR_AP(adapterObj)].CSW.regData;
	// 设置CSW：Size=Word，AddrInc=off
	cswTmp.regInfo.AddrInc = DAP_ADDRINC_OFF;	// AddrInc Off
	if(adapterObj->dap.AP[DAP_CURR_AP(adapterObj)].ctrl_state.lessWordTransfers == 1){	// 支持lessWordTransfer
		cswTmp.regInfo.Size = AP_CSW_SIZE16;	// Half Word
	}else{
		log_warn("Don't Support Less Word Transfers.");
		return FALSE;
	}
	// 是否需要更新CSW寄存器？
	if(adapterObj->dap.AP[DAP_CURR_AP(adapterObj)].CSW.regData != cswTmp.regData){
		if(adapter_DAP_Write_AP_Single(adapterObj, CSW, cswTmp.regData, TRUE) == FALSE){
			return FALSE;
		}
	}
	// 写入TAR
	if(DAP_Write_TAR(adapterObj, addr) == FALSE) return FALSE;

	// 放到Byte Lane确定的位置
	uint32_t data_tmp = data_in << ((addr & 3) << 3);
	// 读取数据，根据byte lane获得数据
	if(adapter_DAP_Write_AP_Single(adapterObj, DRW, data_tmp, TRUE) == FALSE) return FALSE;
	// 执行指令队列
	if(adapter_DAP_Execute(adapterObj) == FALSE){
		return FALSE;
	}
	// 指令执行成功，同步CSW
	adapterObj->dap.AP[DAP_CURR_AP(adapterObj)].CSW.regData = cswTmp.regData;
	return TRUE;
}

/**
 * 写入32位数据
 * addr：64位地址
 * data_in：要写入的数据
 * 注意：地址要以字对齐，否则报错
 */
BOOL DAP_WriteMem32(AdapterObject *adapterObj, uint64_t addr, uint32_t data_in){
	assert(adapterObj != NULL && adapterObj->dap.AP[DAP_CURR_AP(adapterObj)].ctrl_state.init == 1);
	// 检查对齐
	if(addr & 0x3){
		log_warn("Memory address is not word aligned!");
		return FALSE;
	}
	MEM_AP_CSW_Parse cswTmp;
	cswTmp.regData = adapterObj->dap.AP[DAP_CURR_AP(adapterObj)].CSW.regData;
	// 设置CSW：Size=Word，AddrInc=off
	cswTmp.regInfo.AddrInc = DAP_ADDRINC_OFF;	// AddrInc Off
	cswTmp.regInfo.Size = AP_CSW_SIZE32;	// Word
	// 是否需要更新CSW寄存器？
	if(adapterObj->dap.AP[DAP_CURR_AP(adapterObj)].CSW.regData != cswTmp.regData){
		if(adapter_DAP_Write_AP_Single(adapterObj, CSW, cswTmp.regData, TRUE) == FALSE){
			return FALSE;
		}
	}
	// 写入TAR
	if(DAP_Write_TAR(adapterObj, addr) == FALSE) return FALSE;

	// 读取数据，根据byte lane获得数据
	if(adapter_DAP_Write_AP_Single(adapterObj, DRW, data_in, TRUE) == FALSE) return FALSE;
	// 执行指令队列
	if(adapter_DAP_Execute(adapterObj) == FALSE){
		return FALSE;
	}
	// 指令执行成功，同步CSW
	adapterObj->dap.AP[DAP_CURR_AP(adapterObj)].CSW.regData = cswTmp.regData;
	return TRUE;
}

/**
 * 写入64位数据
 * addr：64位地址
 * data_in：写入的数据
 * 注意：地址要以双字对齐，否则报错
 * TODO 没有测试
 */
BOOL DAP_WriteMem64(AdapterObject *adapterObj, uint64_t addr, uint64_t data_in){
	assert(adapterObj != NULL && adapterObj->dap.AP[DAP_CURR_AP(adapterObj)].ctrl_state.init == 1);
	// 检查对齐
	if(addr & 0x7){
		log_warn("Memory address is not double word aligned!");
		return FALSE;
	}
	MEM_AP_CSW_Parse cswTmp;
	cswTmp.regData = adapterObj->dap.AP[DAP_CURR_AP(adapterObj)].CSW.regData;
	// 设置CSW：Size=Word，AddrInc=off
	cswTmp.regInfo.AddrInc = DAP_ADDRINC_OFF;	// AddrInc Off
	// 判断是否支持Big Data
	if(adapterObj->dap.AP[DAP_CURR_AP(adapterObj)].ctrl_state.largeData == 1){
		cswTmp.regInfo.Size = AP_CSW_SIZE64;	// Double Word
	}else{
		log_warn("Don't Support Big Data Transfers.");
		return FALSE;
	}
	// 是否需要更新CSW寄存器？
	if(adapterObj->dap.AP[DAP_CURR_AP(adapterObj)].CSW.regData != cswTmp.regData){
		if(adapter_DAP_Write_AP_Single(adapterObj, CSW, cswTmp.regData, TRUE) == FALSE){
			return FALSE;
		}
	}
	// 写入TAR
	if(DAP_Write_TAR(adapterObj, addr) == FALSE) return FALSE;
	// 写入数据，先写低位，再写高位，最后一个高位写完后才初始化Memory access
	if(adapter_DAP_Write_AP_Single(adapterObj, DRW, data_in & 0xffffffffu, TRUE) == FALSE) return FALSE;
	if(adapter_DAP_Write_AP_Single(adapterObj, DRW, data_in >> 32, TRUE) == FALSE) return FALSE;
	// 执行指令队列
	if(adapter_DAP_Execute(adapterObj) == FALSE){
		return FALSE;
	}
	// 指令执行成功，同步CSW
	adapterObj->dap.AP[DAP_CURR_AP(adapterObj)].CSW.regData = cswTmp.regData;
	return TRUE;
}

/**
 * 批量读取数据
 * addr：要读取的地址
 * addrIncMode：是否启用地址自增或者模式
 * 可取的值有：
 * DAP_ADDRINC_OFF：在每次传输之后TAR中的地址不自增
 * DAP_ADDRINC_SINGLE：每次传输成功后，TAR的地址增加传输数据的大小
 * DAP_ADDRINC_PACKED：当设置此参数时，启动packed传送模式；每次传输成功后，TAR增加传输的数据大小。
 * 注意，在启用地址自增的时候，只保证操作TAR寄存器中地址的低10位[9:0]，如果自增到[10]位或者超过[10]时，自增动作
 * 是视不同实现而定的（IMPLEMENTATION DEFINED）。也就是说地址自增在1kb的内存边界内进行，超过1kb的边界自增动作是视不同实现而定的（IMPLEMENTATION DEFINED）。
 * transSize：传输数据大小（在非packed模式下注意byte lane）
 * transCnt：传输数据的个数，单位为字
 * data_out：读取的数据存放地址
 */
BOOL DAP_ReadMemBlock(AdapterObject *adapterObj, uint64_t addr, int addrIncMode, int transSize, int transCnt, uint32_t *data_out){
	assert(adapterObj != NULL && adapterObj->dap.AP[DAP_CURR_AP(adapterObj)].ctrl_state.init == 1);
	// 设置csw
	MEM_AP_CSW_Parse cswTmp;
	cswTmp.regData = adapterObj->dap.AP[DAP_CURR_AP(adapterObj)].CSW.regData;
	// some checks
	switch(transSize){
	case AP_CSW_SIZE8:
		// 检查是否支持less word Transfer
		if(adapterObj->dap.AP[DAP_CURR_AP(adapterObj)].ctrl_state.lessWordTransfers == 1){	// 支持lessWordTransfer
			cswTmp.regInfo.Size = AP_CSW_SIZE8;	// Half Word
		}else{
			log_warn("Don't Support Less Word Transfers.");
			return FALSE;
		}
		break;
	case AP_CSW_SIZE16:
		// 检查是否对齐
		if(addr & 0x1){
			log_warn("Memory address is not half word aligned!");
			return FALSE;
		}
		// 检查是否支持less word Transfer
		if(adapterObj->dap.AP[DAP_CURR_AP(adapterObj)].ctrl_state.lessWordTransfers == 1){	// 支持lessWordTransfer
			cswTmp.regInfo.Size = AP_CSW_SIZE16;	// Half Word
		}else{
			log_warn("Don't Support Less Word Transfers.");
			return FALSE;
		}
		break;
	case AP_CSW_SIZE32:
		if(addr & 0x3){
			log_warn("Memory address is not word aligned!");
			return FALSE;
		}
		cswTmp.regInfo.Size = AP_CSW_SIZE32;	// Word
		break;
	case AP_CSW_SIZE64:
	case AP_CSW_SIZE128:
	case AP_CSW_SIZE256:
		log_warn("Specified Data Size is not support.");
		return FALSE;
		break;
	default: break;
	}
	cswTmp.regInfo.AddrInc = addrIncMode;	// 地址自增模式
	// 是否需要更新CSW寄存器？
	if(adapterObj->dap.AP[DAP_CURR_AP(adapterObj)].CSW.regData != cswTmp.regData){
		if(adapter_DAP_Write_AP_Single(adapterObj, CSW, cswTmp.regData, TRUE) == FALSE){
			return FALSE;
		}
	}

	// TODO 处理地址自增模式下超过1kb边界的情况，超过1kb的边界了需要拆分，每次地址自增控制在1kb以内
	uint64_t addrCurr = addr, addrEnd;	// 当前地址，结束地址
	uint64_t addrNextBoundary;	// 地址的下一个1kb边界
	int thisTimeTransCnt,dataPos = 0;// 指向data_out的偏移

	if(addrIncMode == DAP_ADDRINC_SINGLE){
		addrEnd = addr + (transCnt << transSize);	// 每次写DRW，发起一次memory access，之后自增TAR
		while(addrCurr < addrEnd){
			addrNextBoundary = ((addrCurr >> 10) + 1) << 10;	// 找到下一个1kb边界
			// 写入地址
			if(DAP_Write_TAR(adapterObj, addrCurr) == FALSE) return FALSE;
			//log_debug("SINGLE:CurrAddr:0x%08X;next boundary:0x%08X.", addrCurr, addrNextBoundary);
			// 如果下一个边界大于结束地址
			if(addrNextBoundary > addrEnd){
				thisTimeTransCnt = (addrEnd - addrCurr) >> transSize;
				addrCurr = addrEnd;
			}else{
				thisTimeTransCnt = (addrNextBoundary - addrCurr) >> transSize;
				addrCurr = addrNextBoundary;
			}
			//log_debug("SINGLE:dataPos:%d;thisTimeTransCnt:%d.", dataPos, thisTimeTransCnt);
			// 读取block
			if(adapter_DAP_Read_AP_Block(adapterObj, DRW, data_out + dataPos, thisTimeTransCnt, TRUE) == FALSE){
				return FALSE;
			}
			dataPos += thisTimeTransCnt;
		}
	}else if(addrIncMode == DAP_ADDRINC_PACKED){
		addrEnd = addr + (transCnt << 2);	// 每次写DRW，发起多次memory access，每次Memory access成功后自增TAR
		while(addrCurr < addrEnd){
			addrNextBoundary = ((addrCurr >> 10) + 1) << 10;	// 找到下一个1kb边界
			// 写入地址
			if(DAP_Write_TAR(adapterObj, addrCurr) == FALSE) return FALSE;
			//log_debug("PACKED:CurrAddr:0x%08X;next boundary:0x%08X.", addrCurr, addrNextBoundary);
			// 如果下一个边界大于结束地址
			if(addrNextBoundary > addrEnd){
				thisTimeTransCnt = (addrEnd - addrCurr) >> 2;
				addrCurr = addrEnd;
			}else{
				thisTimeTransCnt = (addrNextBoundary - addrCurr) >> 2;
				addrCurr = addrNextBoundary;
			}
			//log_debug("PACKED:dataPos:%d;thisTimeTransCnt:%d.", dataPos, thisTimeTransCnt);
			// 读取block
			if(adapter_DAP_Read_AP_Block(adapterObj, DRW, data_out + dataPos, thisTimeTransCnt, TRUE) == FALSE){
				return FALSE;
			}
			dataPos += thisTimeTransCnt;
		}
	}
	// 执行指令队列
	if(adapter_DAP_Execute(adapterObj) == FALSE){
		return FALSE;
	}
	// 指令执行成功，同步CSW
	adapterObj->dap.AP[DAP_CURR_AP(adapterObj)].CSW.regData = cswTmp.regData;
	return TRUE;
}

BOOL DAP_WriteMemBlock(AdapterObject *adapterObj, uint64_t addr, int addrIncMode, int transSize, int transCnt, uint32_t *data_in){
	assert(adapterObj != NULL && adapterObj->dap.AP[DAP_CURR_AP(adapterObj)].ctrl_state.init == 1);
	// 设置csw
	MEM_AP_CSW_Parse cswTmp;
	cswTmp.regData = adapterObj->dap.AP[DAP_CURR_AP(adapterObj)].CSW.regData;
	// some checks
	switch(transSize){
	case AP_CSW_SIZE8:
		// 检查是否支持less word Transfer
		if(adapterObj->dap.AP[DAP_CURR_AP(adapterObj)].ctrl_state.lessWordTransfers == 1){	// 支持lessWordTransfer
			cswTmp.regInfo.Size = AP_CSW_SIZE8;	// Half Word
		}else{
			log_warn("Don't Support Less Word Transfers.");
			return FALSE;
		}
		break;
	case AP_CSW_SIZE16:
		// 检查是否对齐
		if(addr & 0x1){
			log_warn("Memory address is not half word aligned!");
			return FALSE;
		}
		// 检查是否支持less word Transfer
		if(adapterObj->dap.AP[DAP_CURR_AP(adapterObj)].ctrl_state.lessWordTransfers == 1){	// 支持lessWordTransfer
			cswTmp.regInfo.Size = AP_CSW_SIZE16;	// Half Word
		}else{
			log_warn("Don't Support Less Word Transfers.");
			return FALSE;
		}
		break;
	case AP_CSW_SIZE32:
		if(addr & 0x3){
			log_warn("Memory address is not word aligned!");
			return FALSE;
		}
		cswTmp.regInfo.Size = AP_CSW_SIZE32;	// Word
		break;
	case AP_CSW_SIZE64:
	case AP_CSW_SIZE128:
	case AP_CSW_SIZE256:
		log_warn("Specified Data Size is not support.");
		return FALSE;
		break;
	default: break;
	}
	cswTmp.regInfo.AddrInc = addrIncMode;	// 地址自增模式
	// 是否需要更新CSW寄存器？
	if(adapterObj->dap.AP[DAP_CURR_AP(adapterObj)].CSW.regData != cswTmp.regData){
		if(adapter_DAP_Write_AP_Single(adapterObj, CSW, cswTmp.regData, TRUE) == FALSE){
			return FALSE;
		}
	}

	// TODO 处理地址自增模式下超过1kb边界的情况，超过1kb的边界了需要拆分，每次地址自增控制在1kb以内
	uint64_t addrCurr = addr, addrEnd;	// 当前地址，结束地址
	uint64_t addrNextBoundary;	// 地址的下一个1kb边界
	int thisTimeTransCnt,dataPos = 0;// 指向data_out的偏移

	if(addrIncMode == DAP_ADDRINC_SINGLE){
		addrEnd = addr + (transCnt << transSize);	// 每次写DRW，发起一次memory access，之后自增TAR
		while(addrCurr < addrEnd){
			addrNextBoundary = ((addrCurr >> 10) + 1) << 10;	// 找到下一个1kb边界
			// 写入地址
			if(DAP_Write_TAR(adapterObj, addrCurr) == FALSE) return FALSE;
			//log_debug("SINGLE:CurrAddr:0x%08X;next boundary:0x%08X.", addrCurr, addrNextBoundary);
			// 如果下一个边界大于结束地址
			if(addrNextBoundary > addrEnd){
				thisTimeTransCnt = (addrEnd - addrCurr) >> transSize;
				addrCurr = addrEnd;
			}else{
				thisTimeTransCnt = (addrNextBoundary - addrCurr) >> transSize;
				addrCurr = addrNextBoundary;
			}
			//log_debug("SINGLE:dataPos:%d;thisTimeTransCnt:%d.", dataPos, thisTimeTransCnt);
			// 读取block
			if(adapter_DAP_Write_AP_Block(adapterObj, DRW, data_in + dataPos, thisTimeTransCnt, TRUE) == FALSE){
				return FALSE;
			}
			dataPos += thisTimeTransCnt;
		}
	}else if(addrIncMode == DAP_ADDRINC_PACKED){
		addrEnd = addr + (transCnt << 2);	// 每次写DRW，发起多次memory access，每次Memory access成功后自增TAR
		while(addrCurr < addrEnd){
			addrNextBoundary = ((addrCurr >> 10) + 1) << 10;	// 找到下一个1kb边界
			// 写入地址
			if(DAP_Write_TAR(adapterObj, addrCurr) == FALSE) return FALSE;
			//log_debug("PACKED:CurrAddr:0x%08X;next boundary:0x%08X.", addrCurr, addrNextBoundary);
			// 如果下一个边界大于结束地址
			if(addrNextBoundary > addrEnd){
				thisTimeTransCnt = (addrEnd - addrCurr) >> 2;
				addrCurr = addrEnd;
			}else{
				thisTimeTransCnt = (addrNextBoundary - addrCurr) >> 2;
				addrCurr = addrNextBoundary;
			}
			//log_debug("PACKED:dataPos:%d;thisTimeTransCnt:%d.", dataPos, thisTimeTransCnt);
			// 读取block
			if(adapter_DAP_Write_AP_Block(adapterObj, DRW, data_in + dataPos, thisTimeTransCnt, TRUE) == FALSE){
				return FALSE;
			}
			dataPos += thisTimeTransCnt;
		}
	}
	// 执行指令队列
	if(adapter_DAP_Execute(adapterObj) == FALSE){
		return FALSE;
	}
	// 指令执行成功，同步CSW
	adapterObj->dap.AP[DAP_CURR_AP(adapterObj)].CSW.regData = cswTmp.regData;
	return TRUE;
}
