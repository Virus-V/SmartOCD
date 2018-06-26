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
	TRY(adapter_DAP_Write_DP(adapterObj, SELECT, selectTmp.regData, FALSE), 1);
	TRY(adapter_DAP_Execute(adapterObj), 2);
	// select更新成功，同步select寄存器数据
	adapterObj->dap.SELECT_Reg.regData = selectTmp.regData;
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
	uint32_t ctrl_stat;

	for(apIdx = 0; apIdx < 256; apIdx++){
		// 选择AP
		if(DAP_AP_Select(adapterObj, apIdx) == FALSE){
			log_info("DAP_AP_Select fail,apIdx:%d", apIdx);
			return -1;
		}
		if(adapter_DAP_Read_AP(adapterObj, IDR, &ap_IDR.regData, TRUE) == FALSE){
			log_info("Add read AP_IDR command fail,apIdx:%d", apIdx);
			return -1;
		}
		if(adapter_DAP_Read_DP(adapterObj, CTRL_STAT, &ctrl_stat, TRUE) == FALSE){
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
 */
BOOL DAP_Read_TAR(AdapterObject *adapterObj, uint64_t *address_out){
	assert(adapterObj != NULL);
	*address_out = 0;
	uint64_t addr = 0; uint32_t tmp;
	// 如果支持Large Address
	if(adapterObj->dap.AP[DAP_CURR_AP(adapterObj)].ctrl_state.init == 1 &&
			adapterObj->dap.AP[DAP_CURR_AP(adapterObj)].ctrl_state.largeAddress == 1){
		if(adapter_DAP_Read_AP(adapterObj, TAR_MSB, &tmp, TRUE) == FALSE) return FALSE;
		addr = tmp;
		addr <<= 32;	// 移到高32位
	}
	if(adapter_DAP_Read_AP(adapterObj, TAR_LSB, &tmp, TRUE) == FALSE) return FALSE;
	// 执行指令
	if(adapter_DAP_Execute(adapterObj) == FALSE){
		adapter_DAP_CleanCommandQueue(adapterObj);
		log_warn("Read TAR Failed.");
		return FALSE;
	}
	addr |= tmp;	// 赋值低32位
	*address_out = addr;
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
		if(adapter_DAP_Write_AP(adapterObj, TAR_MSB, (uint32_t)(address_in >> 32), TRUE) == FALSE) return FALSE;
	}
	// 将低32位写入TAR_LSB
	if(adapter_DAP_Write_AP(adapterObj, TAR_LSB, (uint32_t)(address_in & 0xffffffffu), TRUE) == FALSE) return FALSE;
	// 执行指令
	if(adapter_DAP_Execute(adapterObj) == FALSE){
		adapter_DAP_CleanCommandQueue(adapterObj);
		log_warn("Write TAR Failed.");
		return FALSE;
	}
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
	cswTmp.regInfo.AddrInc = ADDRINC_OFF;	// AddrInc Off
	if(adapterObj->dap.AP[DAP_CURR_AP(adapterObj)].ctrl_state.lessWordTransfers == 1){	// 支持lessWordTransfer
		cswTmp.regInfo.Size = SIZE_8;	// Byte
	}else{
		log_warn("Don't Support Less Word Transfers.");
		return FALSE;
	}
	// 是否需要更新CSW寄存器？
	if(adapterObj->dap.AP[DAP_CURR_AP(adapterObj)].CSW.regData != cswTmp.regData){
		if(adapter_DAP_Write_AP(adapterObj, CSW, cswTmp.regData, TRUE) == FALSE){
			return FALSE;
		}
	}
	// 写入TAR
	if(DAP_Write_TAR(adapterObj, addr) == FALSE) return FALSE;
	// TAR已更新，同步CSW
	adapterObj->dap.AP[DAP_CURR_AP(adapterObj)].CSW.regData = cswTmp.regData;
	uint32_t data_tmp = 0;
	// 读取数据，根据byte lane获得数据
	if(adapter_DAP_Read_AP(adapterObj, DRW, &data_tmp, TRUE) == FALSE) return FALSE;
	// 执行指令队列
	if(adapter_DAP_Execute(adapterObj) == FALSE){
		return FALSE;
	}
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
	cswTmp.regInfo.AddrInc = ADDRINC_OFF;	// AddrInc Off
	if(adapterObj->dap.AP[DAP_CURR_AP(adapterObj)].ctrl_state.lessWordTransfers == 1){	// 支持lessWordTransfer
		cswTmp.regInfo.Size = SIZE_16;	// Half Word
	}else{
		log_warn("Don't Support Less Word Transfers.");
		return FALSE;
	}
	// 是否需要更新CSW寄存器？
	if(adapterObj->dap.AP[DAP_CURR_AP(adapterObj)].CSW.regData != cswTmp.regData){
		if(adapter_DAP_Write_AP(adapterObj, CSW, cswTmp.regData, TRUE) == FALSE){
			return FALSE;
		}
	}
	// 写入TAR
	if(DAP_Write_TAR(adapterObj, addr) == FALSE) return FALSE;
	// TAR已更新，同步CSW
	adapterObj->dap.AP[DAP_CURR_AP(adapterObj)].CSW.regData = cswTmp.regData;
	uint32_t data_tmp = 0;
	// 读取数据，根据byte lane获得数据
	if(adapter_DAP_Read_AP(adapterObj, DRW, &data_tmp, TRUE) == FALSE) return FALSE;
	// 执行指令队列
	if(adapter_DAP_Execute(adapterObj) == FALSE){
		return FALSE;
	}
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
	cswTmp.regInfo.AddrInc = ADDRINC_OFF;	// AddrInc Off
	cswTmp.regInfo.Size = SIZE_32;	// Word
	// 是否需要更新CSW寄存器？
	if(adapterObj->dap.AP[DAP_CURR_AP(adapterObj)].CSW.regData != cswTmp.regData){
		if(adapter_DAP_Write_AP(adapterObj, CSW, cswTmp.regData, TRUE) == FALSE){
			return FALSE;
		}
	}
	// 写入TAR
	if(DAP_Write_TAR(adapterObj, addr) == FALSE) return FALSE;
	// TAR已更新，同步CSW
	adapterObj->dap.AP[DAP_CURR_AP(adapterObj)].CSW.regData = cswTmp.regData;

	// 读取数据，根据byte lane获得数据
	if(adapter_DAP_Read_AP(adapterObj, DRW, data_out, TRUE) == FALSE) return FALSE;
	// 执行指令队列
	if(adapter_DAP_Execute(adapterObj) == FALSE){
		return FALSE;
	}
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
	cswTmp.regInfo.AddrInc = ADDRINC_OFF;	// AddrInc Off
	// 判断是否支持Big Data
	if(adapterObj->dap.AP[DAP_CURR_AP(adapterObj)].ctrl_state.largeData == 1){
		cswTmp.regInfo.Size = SIZE_64;	// Double Word
	}else{
		log_warn("Don't Support Big Data Transfers.");
		return FALSE;
	}
	// 是否需要更新CSW寄存器？
	if(adapterObj->dap.AP[DAP_CURR_AP(adapterObj)].CSW.regData != cswTmp.regData){
		if(adapter_DAP_Write_AP(adapterObj, CSW, cswTmp.regData, TRUE) == FALSE){
			return FALSE;
		}
	}
	// 写入TAR
	if(DAP_Write_TAR(adapterObj, addr) == FALSE) return FALSE;
	// TAR已更新，同步CSW
	adapterObj->dap.AP[DAP_CURR_AP(adapterObj)].CSW.regData = cswTmp.regData;
	uint32_t data_tmp[2];	// 定义64位数据
	// 读取数据，第一次读取的是低位，接下来读取高位，必须两次读取后才能完成本次AP Memory access
	if(adapter_DAP_Read_AP(adapterObj, DRW, data_tmp, TRUE) == FALSE) return FALSE;
	if(adapter_DAP_Read_AP(adapterObj, DRW, data_tmp + 1, TRUE) == FALSE) return FALSE;
	// 执行指令队列
	if(adapter_DAP_Execute(adapterObj) == FALSE){
		return FALSE;
	}
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
	cswTmp.regInfo.AddrInc = ADDRINC_OFF;	// AddrInc Off
	if(adapterObj->dap.AP[DAP_CURR_AP(adapterObj)].ctrl_state.lessWordTransfers == 1){	// 支持lessWordTransfer
		cswTmp.regInfo.Size = SIZE_8;	// Byte
	}else{
		log_warn("Don't Support Less Word Transfers.");
		return FALSE;
	}
	// 是否需要更新CSW寄存器？
	if(adapterObj->dap.AP[DAP_CURR_AP(adapterObj)].CSW.regData != cswTmp.regData){
		if(adapter_DAP_Write_AP(adapterObj, CSW, cswTmp.regData, TRUE) == FALSE){
			return FALSE;
		}
	}
	// 写入TAR
	if(DAP_Write_TAR(adapterObj, addr) == FALSE) return FALSE;
	// TAR已更新，同步CSW
	adapterObj->dap.AP[DAP_CURR_AP(adapterObj)].CSW.regData = cswTmp.regData;
	// 放到Byte Lane确定的位置
	uint32_t data_tmp = data_in << ((addr & 3) << 3);
	// 读取数据，根据byte lane获得数据
	if(adapter_DAP_Write_AP(adapterObj, DRW, data_tmp, TRUE) == FALSE) return FALSE;
	// 执行指令队列
	if(adapter_DAP_Execute(adapterObj) == FALSE){
		return FALSE;
	}
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
	cswTmp.regInfo.AddrInc = ADDRINC_OFF;	// AddrInc Off
	if(adapterObj->dap.AP[DAP_CURR_AP(adapterObj)].ctrl_state.lessWordTransfers == 1){	// 支持lessWordTransfer
		cswTmp.regInfo.Size = SIZE_16;	// Half Word
	}else{
		log_warn("Don't Support Less Word Transfers.");
		return FALSE;
	}
	// 是否需要更新CSW寄存器？
	if(adapterObj->dap.AP[DAP_CURR_AP(adapterObj)].CSW.regData != cswTmp.regData){
		if(adapter_DAP_Write_AP(adapterObj, CSW, cswTmp.regData, TRUE) == FALSE){
			return FALSE;
		}
	}
	// 写入TAR
	if(DAP_Write_TAR(adapterObj, addr) == FALSE) return FALSE;
	// TAR已更新，同步CSW
	adapterObj->dap.AP[DAP_CURR_AP(adapterObj)].CSW.regData = cswTmp.regData;
	// 放到Byte Lane确定的位置
	uint32_t data_tmp = data_in << ((addr & 3) << 3);
	// 读取数据，根据byte lane获得数据
	if(adapter_DAP_Write_AP(adapterObj, DRW, data_tmp, TRUE) == FALSE) return FALSE;
	// 执行指令队列
	if(adapter_DAP_Execute(adapterObj) == FALSE){
		return FALSE;
	}
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
	cswTmp.regInfo.AddrInc = ADDRINC_OFF;	// AddrInc Off
	cswTmp.regInfo.Size = SIZE_32;	// Word
	// 是否需要更新CSW寄存器？
	if(adapterObj->dap.AP[DAP_CURR_AP(adapterObj)].CSW.regData != cswTmp.regData){
		if(adapter_DAP_Write_AP(adapterObj, CSW, cswTmp.regData, TRUE) == FALSE){
			return FALSE;
		}
	}
	// 写入TAR
	if(DAP_Write_TAR(adapterObj, addr) == FALSE) return FALSE;
	// TAR已更新，同步CSW
	adapterObj->dap.AP[DAP_CURR_AP(adapterObj)].CSW.regData = cswTmp.regData;

	// 读取数据，根据byte lane获得数据
	if(adapter_DAP_Write_AP(adapterObj, DRW, data_in, TRUE) == FALSE) return FALSE;
	// 执行指令队列
	if(adapter_DAP_Execute(adapterObj) == FALSE){
		return FALSE;
	}
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
	cswTmp.regInfo.AddrInc = ADDRINC_OFF;	// AddrInc Off
	// 判断是否支持Big Data
	if(adapterObj->dap.AP[DAP_CURR_AP(adapterObj)].ctrl_state.largeData == 1){
		cswTmp.regInfo.Size = SIZE_64;	// Double Word
	}else{
		log_warn("Don't Support Big Data Transfers.");
		return FALSE;
	}
	// 是否需要更新CSW寄存器？
	if(adapterObj->dap.AP[DAP_CURR_AP(adapterObj)].CSW.regData != cswTmp.regData){
		if(adapter_DAP_Write_AP(adapterObj, CSW, cswTmp.regData, TRUE) == FALSE){
			return FALSE;
		}
	}
	// 写入TAR
	if(DAP_Write_TAR(adapterObj, addr) == FALSE) return FALSE;
	// TAR已更新，同步CSW
	adapterObj->dap.AP[DAP_CURR_AP(adapterObj)].CSW.regData = cswTmp.regData;
	// XXX 写入数据，先写低位，再写高位，最后一个高位写完后才初始化Memory access
	if(adapter_DAP_Write_AP(adapterObj, DRW, data_in & 0xffffffffu, TRUE) == FALSE) return FALSE;
	if(adapter_DAP_Write_AP(adapterObj, DRW, data_in >> 32, TRUE) == FALSE) return FALSE;
	// 执行指令队列
	if(adapter_DAP_Execute(adapterObj) == FALSE){
		return FALSE;
	}
	return TRUE;
}
