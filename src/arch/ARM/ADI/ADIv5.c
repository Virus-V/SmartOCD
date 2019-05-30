/*
 * ADIv5.c
 *
 *  Created on: 2019-5-29
 *      Author: virusv
 */

#include "smart_ocd.h"
#include "misc/log.h"

#include "arch/ARM/ADI/ADIv5_private.h"

/**
 * DAP 初始化
 * 上电,初始化寄存器
 */
static int dapInit(struct ADIv5_Dap *dap){
	// 调试部分复位
	dap->adapter->Reset(ADPT_RESET_DEBUG_RESET);
	if(dap->adapter->currTransMode == ADPT_MODE_SWD){
		uint32_t dpidr = 0;
		// SWD模式下第一个读取的寄存器必须要是DPIDR，这样才能读取其他的寄存器
		// 否则其他寄存器无法读取
		dap->adapter->DapSingleRead(dap->adapter, ADPT_DAP_DP_REG, DP_REG_DPIDR, &dpidr);
		dap->adapter->DapSingleWrite(dap->adapter, ADPT_DAP_DP_REG, DP_REG_ABORT, &dpidr);	// 清空STICKER ERROR 信息
		if(dap->adapter->DapCommit(dap->adapter) != ADPT_SUCCESS){
			log_error("Init DAP failed!");
			return ADI_INTERNAL_ERROR;
		}
		log_info("DAP DPIDR:0x%08X.", dpidr);
	}else if(dap->adapter->currTransMode == ADPT_MODE_JTAG){
		dap->adapter->JtagToState(dap->adapter, JTAG_TAP_IDLE);
		if(dap->adapter->JtagCommit(dap->adapter) != ADPT_SUCCESS){
			log_error("Change TAP to IDLE state failed!");
			return ADI_INTERNAL_ERROR;
		}
	}else{
		log_error("Unknow transfer type!");
		return ADI_FAILED;
	}

	uint32_t ctrl_stat = 0;
	// 清零SELECT寄存器
	dap->adapter->DapSingleWrite(dap->adapter, ADPT_DAP_DP_REG, DP_REG_SELECT, 0);
	// 写0x20到CTRL，并读取
	dap->adapter->DapSingleWrite(dap->adapter, ADPT_DAP_DP_REG, DP_REG_CTRL_STAT, 0x20);
	//dap->adapter->DapSingleRead(dap->adapter, ADPT_DAP_DP_REG, DP_REG_CTRL_STAT, &ctrl_stat);
	// 写上电请求
	dap->adapter->DapSingleWrite(dap->adapter, ADPT_DAP_DP_REG, DP_REG_CTRL_STAT, DP_CTRL_CSYSPWRUPREQ | DP_CTRL_CDBGPWRUPREQ);
	if(dap->adapter->DapCommit(dap->adapter) != ADPT_SUCCESS){
		log_error("Init DAP register failed!");
		return ADI_INTERNAL_ERROR;
	}
	do{
		dap->adapter->DapSingleRead(dap->adapter, ADPT_DAP_DP_REG, DP_REG_CTRL_STAT, &ctrl_stat);
		if(dap->adapter->DapCommit(dap->adapter) != ADPT_SUCCESS){
			log_error("Read DP CTRL/STAT register failed!");
			return ADI_INTERNAL_ERROR;
		}
	}while((ctrl_stat & (DP_STAT_CDBGPWRUPACK | DP_STAT_CSYSPWRUPACK)) != (DP_STAT_CDBGPWRUPACK | DP_STAT_CSYSPWRUPACK));
	log_debug("DAP Power up. CTRL_STAT:0x%08X.", ctrl_stat);
	return ADI_SUCCESS;
}

/**
 * getApInfo 获得AP的信息:IDR,CSW,CFG
 */
static int getApInfo(ap, index){

}

/**
 * findAP
 */
static int findAP(DAP self, enum apType type, AccessPort* ap){
	struct ADIv5_AccessPort *cmd, *cmd_t;
	// 先在链表中搜索,如果没有,则尝试创建新的AP,插入链表
	return ADI_SUCCESS;
}

// 创建DAP对象
DAP ADIv5_CreateDap(Adapter adapter){
	assert(adapter != NULL);
	struct ADIv5_Dap *dap = calloc(1, sizeof(struct ADIv5_Dap));
	if(!dap){
		log_error("Failed to create DAP object!");
		return NULL;
	}
	INIT_LIST_HEAD(&dap->apList);
	dap->adapter = adapter;
	dap->dap.FindAccessPort = findAP;
	// 初始化DAP对象
	if(dapInit(dap) != ADI_SUCCESS){
		free(dap);
		return NULL;
	}
	// 返回接口
	return (DAP)&dap->dap;
}
