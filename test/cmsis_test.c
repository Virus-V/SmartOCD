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
#include <math.h>

#include "smartocd.h"
#include "misc/log.h"
#include "misc/misc.h"
#include "debugger/cmsis-dap.h"
#include "arch/ARM/ADI/DAP.h"

uint16_t vids[] = {0xc251, 0};
uint16_t pids[] = {0xf001, 0};


/*
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

static uint32_t readMem(AdapterObject *adapterObj, uint32_t addr){
	uint32_t tmp;
	adapterObj->Operate(adapterObj, CMDAP_WRITE_DP_REG, 0, DP_SELECT, 0x0);	// 选择AP bank0
	adapterObj->Operate(adapterObj, CMDAP_WRITE_AP_REG, 0, AP_TAR_LSB, addr);
	adapterObj->Operate(adapterObj, CMDAP_READ_AP_REG, 0, AP_DRW, &tmp);
	return tmp;
}

static void printComponentInfo(AdapterObject *adapterObj, uint32_t startAddr){
	uint32_t addr, tmp, size;
	addr = (startAddr & ~0xfff) | 0xFF4;
	tmp = readMem(adapterObj, addr);
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
	addr = (startAddr & ~0xfff) | 0xFD0;
	tmp = readMem(adapterObj, addr);
	printf("%08X\n", tmp);
	tmp = (tmp >> 4);
	size = pow(2, tmp);
	printf(",occupies %d blocks.\n", size);
}
*/

static BOOL DAP_TransferBlock(uint8_t *respBuff, AdapterObject *adapterObj, uint8_t index,
		int sequenceCnt, uint8_t *data, uint8_t *response, int *okSeqCnt)
{
	BOOL result = FALSE;
	// 本次传输长度
	int restCnt = 0, readCnt = 0, writeCnt = 0;
	int sentPacketMaxCnt = (1024 - 5) >> 2;	// 发送数据包可以装填的数据个数
	int readPacketMaxCnt = (1024 - 4) >> 2;	// 接收数据包可以装填的数据个数

	log_debug("send max:%d,read max:%d.", sentPacketMaxCnt, readPacketMaxCnt);
	// 开辟本次数据包的空间
	uint8_t *buff = calloc(1024, sizeof(uint8_t));
	if(buff == NULL){
		log_warn("Unable to allocate send packet buffer, the heap may be full.");
		return FALSE;
	}
	// 构造数据包头部
	buff[0] = ID_DAP_TransferBlock;
	buff[1] = index;	// DAP index, JTAG ScanChain 中的位置，在SWD模式下忽略该参数

	for(*okSeqCnt = 0; *okSeqCnt < sequenceCnt; (*okSeqCnt)++){
		restCnt = *CAST(int *, data + readCnt); readCnt += sizeof(int);
		log_debug("restCnt:%d.", restCnt);
		uint8_t seq = *CAST(uint8_t *, data + readCnt++);
		if(seq & DAP_TRANSFER_RnW){	// 读操作
			log_debug("Read");
			while(restCnt > 0){
				log_debug("restCnt:%d.", restCnt);
				if(restCnt > readPacketMaxCnt){
					*CAST(uint16_t *, buff + 2) = readPacketMaxCnt;
					buff[4] = seq;
					// 交换数据
					//DAP_EXCHANGE_DATA(adapterObj, buff, 5, respBuff);
					misc_PrintBulk(buff, 5, 25);
					// 判断操作成功，写回数据 XXX 小端字节序
					//if(*CAST(uint16_t *, respBuff + 1) == readPacketMaxCnt && respBuff[3] == DAP_TRANSFER_OK){	// 成功
					if(1){	// 成功
						//memcpy(response + writeCnt, respBuff + 4, readPacketMaxCnt << 2);	// 将数据拷贝到
						log_info("memcpy(response + %d, respBuff + 4, %d)", writeCnt, readPacketMaxCnt << 2);
						writeCnt += readPacketMaxCnt << 2;
						restCnt -= readPacketMaxCnt;	// 成功读取readPacketMaxCnt个字
					}else{	// 失败
						goto END;
					}
				}else{
					*CAST(uint16_t *, buff + 2) = restCnt;
					buff[4] = seq;
					// 交换数据
					//DAP_EXCHANGE_DATA(adapterObj, buff, 5, respBuff);
					misc_PrintBulk(buff, 5, 25);
					// 判断操作成功，写回数据 XXX 小端字节序
					//if(*CAST(uint16_t *, respBuff + 1) == restCnt && respBuff[3] == DAP_TRANSFER_OK){	// 成功
					if(1){	// 成功
						//memcpy(response + writeCnt, respBuff + 4, restCnt << 2);	// 将数据拷贝到
						log_info("memcpy(response + %d, respBuff + 4, %d)", writeCnt, restCnt << 2);
						writeCnt += restCnt << 2;
						restCnt = 0;	// 全部发送完了
					}else{	// 失败
						goto END;
					}
				}
			}
		}else{	// 写操作
			log_debug("write");
			while(restCnt > 0){
				log_debug("restCnt:%d.", restCnt);
				if(restCnt > sentPacketMaxCnt){
					*CAST(uint16_t *, buff + 2) = sentPacketMaxCnt;
					buff[4] = seq;
					// 拷贝数据
					//memcpy(buff + 5, data + readCnt, sentPacketMaxCnt << 2);	// 将数据拷贝到
					log_info("memcpy(buff + 5, data + %d, %d), %d", readCnt, sentPacketMaxCnt << 2, sentPacketMaxCnt);
					readCnt += sentPacketMaxCnt << 2;
					// 交换数据
					//DAP_EXCHANGE_DATA(adapterObj, buff, 5 + sentPacketMaxCnt << 2, respBuff);
					log_debug("read buff size:%d.", 5 + (sentPacketMaxCnt << 2));
					misc_PrintBulk(buff, 5 + (sentPacketMaxCnt << 2), 25);   // FIXME
					// 判断操作成功 XXX 小端字节序
					//if(*CAST(uint16_t *, respBuff + 1) == sentPacketMaxCnt && respBuff[3] == DAP_TRANSFER_OK){	// 成功
					if(1){	// 成功
						restCnt -= sentPacketMaxCnt;
					}else{	// 失败
						goto END;
					}
				}else{
					*CAST(uint16_t *, buff + 2) = restCnt;
					buff[4] = seq;
					// 拷贝数据
					//memcpy(buff + 5, data + readCnt, restCnt << 2);	// 将数据拷贝到
					log_info("memcpy(buff + 5, data + %d, %d), %d", readCnt, restCnt << 2, restCnt);
					readCnt += restCnt << 2;
					// 交换数据
					//DAP_EXCHANGE_DATA(adapterObj, buff, 5 + restCnt << 2, respBuff);
					log_debug("read buff size:%d.", 5 + (restCnt << 2));
					misc_PrintBulk(buff, 5 + (restCnt << 2), 25);
					// 判断操作成功 XXX 小端字节序
					if(1){	// 成功
					//if(*CAST(uint16_t *, respBuff + 1) == restCnt && respBuff[3] == DAP_TRANSFER_OK){	// 成功
						restCnt = 0;
					}else{	// 失败
						//goto END;
					}
				}
			}
		}
	}
	result = TRUE;
END:
	free(buff);
	return result;
}


// 测试入口点
int main(){
	uint8_t *data = calloc(20480, sizeof(uint32_t));
	int okNum = 0;
	assert(data != NULL);
	*CAST(int *, data) = 1000;
	//data[sizeof(int)] = 0xA;
	data[sizeof(int)] = 0x8;
	DAP_TransferBlock(NULL, NULL, 0, 1, data, NULL, &okNum);
	free(data);
	exit(0);
}
