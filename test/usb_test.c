/*
 * usb_test.c
 *
 *  Created on: 2018-1-27
 *      Author: virusv
 */


#include <stdio.h>
#include <stdlib.h>

#include "smart_ocd.h"
#include "debugger/usb.h"
#include "misc/log.h"

#define ARRAY_COUNT(x) (sizeof(x) / sizeof(x[0]))

char indata[64], outdata[][3] = {
		//{0x01, 0x00, 0x01},
		//{0x01, 0x01, 0x01},
		{0x00, 0xf0},
		//{0x00, 0xf1},
		//{0x00, 0xf2},
		//{0x00, 0x04},
};
/*
 * 打印数据块
 * data：数据内容
 * length：数据长度
 * rowLen：每一行打印多少个byte
 * 返回：打印的行数
 */
static int print_bulk(char *data, int length, int rowLen){
	// 计算要打印多少行
	int lineCnt, placeCnt, tmp;	// 数据总行数，行数的数字位数（用来计算前导0）
	int currRow, printedCnt = 0;	// 当前输出的行和列
	lineCnt = (length / rowLen) + 1;	// 计算行数
	for(tmp = length - (length % rowLen), placeCnt = 1; tmp >= 16; placeCnt++){
		tmp /= 16;
	}
    // 输出一行数据：前导位置信息：数据...
	for(currRow = 0; currRow < lineCnt && rowLen > 0; currRow ++){
		int currPlaceCnt;	// 已输出的数据数字位数和已输出的数据
		// 计算位数
		for(tmp = printedCnt, currPlaceCnt = 1; tmp >= 16; currPlaceCnt++){
			tmp /= 16;
		}
		// 输出前导信息
		for(tmp = placeCnt - currPlaceCnt; tmp > 0; tmp--){
			putchar('0');
		}
		printf("%X: ", printedCnt);
		// 输出数据
		for(tmp = 0; tmp < rowLen ; tmp++){
			printf("%02hhX ", *(data + printedCnt + tmp));
		}
		// 加入输出总长度
		printedCnt += rowLen;
		printf("\n");
		// 判断是否是最后一行
		if(currRow == lineCnt-2){
			rowLen = length % rowLen;
		}
	}
	return currRow;
}
int usb_main(){
	USBObject *doggle;
	int tmp;

	log_set_level(LOG_TRACE);
	doggle = NewUSBObject("test doggle(CMSIS-DAP)");
	if(doggle == NULL){
		log_fatal("make usb object failed");
		exit(1);
	}
	if(USBOpen(doggle, 0xc251, 0xf001, NULL) != TRUE){
		log_fatal("Couldnt open usb device.");
		exit(1);
	}

	if(USBSetConfiguration(doggle, 0) != TRUE){
		log_fatal("USBSetConfiguration Failed.");
		exit(1);
	}

	if(USBClaimInterface(doggle, 3, 0, 0, 3) != TRUE){
		log_fatal("Claim interface failed.");
		exit(1);
	}
	for(tmp=0; tmp < ARRAY_COUNT(outdata); tmp++){
		log_debug("write to doggle %d byte.", doggle->write(doggle, outdata[tmp], sizeof(outdata[tmp]), 0));
		log_debug("read from doggle %d byte.", doggle->read(doggle, indata, 64, 0));
		log_debug("receive data:");
		print_bulk(indata, sizeof(indata), 8);
	}

	USBClose(doggle);
	FreeUSBObject(doggle);
	exit(0);
}
